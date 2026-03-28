# T02 — Multi-stage builds

## El problema: imágenes infladas

Una imagen que compila código necesita herramientas de build (compiladores,
headers, make, npm, cargo) que **no se necesitan en producción**. Sin multi-stage,
todas esas herramientas quedan en la imagen final:

```dockerfile
# Single-stage: la imagen final incluye GCC, headers, código fuente...
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc libc6-dev
WORKDIR /app
COPY main.c .
RUN gcc -o hello main.c
CMD ["./hello"]
# Resultado: ~350 MB (gcc + libs + código fuente + binario)
```

El binario `hello` ocupa unos pocos KB, pero la imagen carga con cientos de MB
de herramientas de compilación.

## La solución: múltiples stages

Multi-stage builds permiten usar **varias instrucciones FROM** en un mismo
Dockerfile. Cada FROM inicia un stage nuevo. Solo las capas del **último stage**
forman la imagen final:

```dockerfile
# Stage 1: BUILD — tiene todas las herramientas de compilación
FROM debian:bookworm AS builder
RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc libc6-dev && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY main.c .
RUN gcc -static -o hello main.c

# Stage 2: RUNTIME — solo el binario, nada más
FROM debian:bookworm-slim
COPY --from=builder /src/hello /usr/local/bin/hello
CMD ["hello"]
# Resultado: ~75 MB (debian-slim + binario de pocos KB)
```

```
Stage 1 (builder):                Stage 2 (runtime):
┌─────────────────────┐           ┌─────────────────────┐
│ debian:bookworm     │           │ debian:bookworm-slim │
│ + gcc, libc6-dev    │           │                     │
│ + main.c            │    COPY   │ + hello (binario)   │
│ + hello (binario)   │ ────────▶ │                     │
└─────────────────────┘           └─────────────────────┘
    ~350 MB (descartado)              ~75 MB (imagen final)
```

## `COPY --from`: copiar entre stages

La clave de multi-stage es `COPY --from=<stage>` que copia archivos desde un
stage anterior:

```dockerfile
# Por nombre del stage (recomendado)
COPY --from=builder /app/binary /usr/local/bin/

# Por índice del stage (frágil, no recomendado)
COPY --from=0 /app/binary /usr/local/bin/

# Desde una imagen externa (no un stage del Dockerfile)
COPY --from=nginx:latest /etc/nginx/nginx.conf /etc/nginx/
```

### Copiar desde imágenes externas

`--from` puede referenciar cualquier imagen, no solo stages del mismo Dockerfile:

```dockerfile
FROM debian:bookworm-slim

# Copiar un binario de otra imagen
COPY --from=golang:1.22 /usr/local/go/bin/go /usr/local/bin/go

# Copiar configuración de otra imagen
COPY --from=nginx:latest /etc/nginx/ /etc/nginx/
```

## Ejemplos por lenguaje

### C

```dockerfile
FROM gcc:12 AS builder
WORKDIR /src
COPY . .
RUN gcc -static -O2 -o myapp main.c

FROM scratch
COPY --from=builder /src/myapp /myapp
CMD ["/myapp"]
# Imagen final: solo el binario (~1-2 MB)
```

### Go

```dockerfile
FROM golang:1.22 AS builder
WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -o /app/server .

FROM scratch
COPY --from=builder /app/server /server
EXPOSE 8080
CMD ["/server"]
# Imagen final: binario estático (~10-20 MB)
```

### Rust

```dockerfile
FROM rust:1.75 AS builder
WORKDIR /app
COPY Cargo.toml Cargo.lock ./
RUN mkdir src && echo 'fn main() {}' > src/main.rs
RUN cargo build --release
RUN rm -rf src

COPY src/ src/
RUN touch src/main.rs && cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && \
    apt-get install -y --no-install-recommends libssl3 ca-certificates && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/target/release/myapp /usr/local/bin/
CMD ["myapp"]
# Imagen final: ~80 MB (vs ~2 GB del stage de build)
```

### Node.js

```dockerfile
FROM node:20 AS builder
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
RUN npm run build

FROM node:20-slim
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci --production
COPY --from=builder /app/dist ./dist
EXPOSE 3000
CMD ["node", "dist/index.js"]
# Imagen final: runtime deps + dist, sin devDependencies ni source
```

### Python

```dockerfile
FROM python:3.12 AS builder
WORKDIR /app
COPY requirements.txt .
RUN pip install --prefix=/install --no-cache-dir -r requirements.txt

FROM python:3.12-slim
COPY --from=builder /install /usr/local
WORKDIR /app
COPY . .
CMD ["python3", "app.py"]
# Imagen final: slim + deps precompiladas, sin pip/wheel/setuptools
```

## `--target`: construir un stage específico

`--target` detiene el build en un stage concreto. Útil para crear imágenes
diferentes desde el mismo Dockerfile:

```dockerfile
FROM debian:bookworm AS base
RUN apt-get update && apt-get install -y --no-install-recommends python3
WORKDIR /app
COPY requirements.txt .
RUN pip3 install -r requirements.txt

FROM base AS development
RUN pip3 install debugpy pytest coverage
COPY . .
CMD ["python3", "-m", "debugpy", "--listen", "0.0.0.0:5678", "app.py"]

FROM base AS production
COPY . .
USER nobody
CMD ["python3", "app.py"]
```

```bash
# Build de desarrollo (incluye debugpy, pytest, coverage)
docker build --target development -t myapp:dev .

# Build de producción (mínimo, non-root)
docker build --target production -t myapp:prod .

# Comparar tamaños
docker image ls myapp
# myapp  dev   ... 250 MB
# myapp  prod  ... 180 MB
```

Un solo Dockerfile, múltiples imágenes para distintos entornos.

## Stages para testing

Se puede añadir un stage de tests que se ejecuta durante el build:

```dockerfile
FROM golang:1.22 AS builder
WORKDIR /app
COPY . .
RUN go build -o /app/server .

FROM builder AS tester
RUN go test -v ./...

FROM scratch AS production
COPY --from=builder /app/server /server
CMD ["/server"]
```

```bash
# Build completo (incluye tests)
docker build -t myapp .
# Si los tests fallan, el build falla

# Build solo hasta los tests
docker build --target tester -t myapp:test .

# Build sin tests (solo production, salta tester)
docker build --target production -t myapp:prod .
```

BuildKit es inteligente: si construyes el target `production`, no ejecuta el
stage `tester` porque `production` no depende de él (depende de `builder`).

## Paralelización con BuildKit

BuildKit detecta stages **independientes** y los construye en paralelo:

```dockerfile
FROM node:20 AS frontend
WORKDIR /web
COPY frontend/ .
RUN npm ci && npm run build

FROM golang:1.22 AS backend
WORKDIR /api
COPY backend/ .
RUN go build -o /api/server .

FROM debian:bookworm-slim
COPY --from=frontend /web/dist /var/www/html
COPY --from=backend /api/server /usr/local/bin/
CMD ["server"]
```

```
BuildKit ejecuta en paralelo:
  frontend (npm ci + npm run build) ─────────────┐
  backend  (go build) ──────────────────────────┐ │
                                                │ │
                                                ▼ ▼
                                         Stage final
                                    COPY --from=frontend
                                    COPY --from=backend
```

Los stages `frontend` y `backend` se construyen simultáneamente porque no
dependen el uno del otro.

## Stages intermedios y limpieza

Los stages intermedios **no aparecen en `docker images`** como imágenes
independientes. Solo el stage final (o el target) produce una imagen:

```bash
docker build -t myapp .
docker images | grep -E "(builder|tester)"
# No aparecen — fueron descartados

# Las capas de los stages intermedios pueden estar en caché local
# Limpiar con:
docker builder prune
```

## Comparación de tamaños

| Lenguaje | Single-stage | Multi-stage | Reducción |
|---|---|---|---|
| C (con gcc) | ~350 MB | ~75 MB (slim) / ~1 MB (scratch) | 78-99% |
| Go | ~800 MB | ~10 MB (scratch) | 99% |
| Rust | ~2 GB | ~80 MB (slim) | 96% |
| Node.js | ~1 GB | ~200 MB (slim, sin devDeps) | 80% |
| Python | ~900 MB | ~150 MB (slim) | 83% |

---

## Ejercicios

### Ejercicio 1 — Single-stage vs multi-stage en C

```bash
mkdir -p /tmp/docker_multi && cd /tmp/docker_multi

cat > main.c << 'EOF'
#include <stdio.h>
int main(void) {
    printf("Hello from multi-stage build!\n");
    return 0;
}
EOF

# Single-stage
cat > Dockerfile.single << 'EOF'
FROM gcc:12
WORKDIR /src
COPY main.c .
RUN gcc -static -o hello main.c
CMD ["./hello"]
EOF

# Multi-stage
cat > Dockerfile.multi << 'EOF'
FROM gcc:12 AS builder
WORKDIR /src
COPY main.c .
RUN gcc -static -o hello main.c

FROM scratch
COPY --from=builder /src/hello /hello
CMD ["/hello"]
EOF

docker build -f Dockerfile.single -t single-test .
docker build -f Dockerfile.multi -t multi-test .

# Comparar tamaños
docker image ls --format "{{.Repository}}:{{.Tag}}\t{{.Size}}" | grep test
# single-test:latest   ~1.3 GB
# multi-test:latest    ~1 MB  ← 1000x más pequeña

# Ambas funcionan
docker run --rm single-test
docker run --rm multi-test

docker rmi single-test multi-test
rm -rf /tmp/docker_multi
```

### Ejercicio 2 — `--target` para dev vs prod

```bash
mkdir -p /tmp/docker_target && cd /tmp/docker_target

cat > app.py << 'EOF'
print("Running application")
EOF

cat > Dockerfile << 'EOF'
FROM python:3.12-slim AS base
WORKDIR /app
COPY app.py .

FROM base AS development
RUN pip install ipython debugpy
CMD ["python3", "-m", "debugpy", "--listen", "0.0.0.0:5678", "app.py"]

FROM base AS production
USER nobody
CMD ["python3", "app.py"]
EOF

docker build --target development -t target-dev .
docker build --target production -t target-prod .

docker image ls --format "{{.Repository}}\t{{.Size}}" | grep target
# target-dev: más grande (incluye ipython, debugpy)
# target-prod: más pequeño

docker run --rm target-prod
# Running application

docker rmi target-dev target-prod
rm -rf /tmp/docker_target
```

### Ejercicio 3 — Copiar desde imagen externa

```bash
mkdir -p /tmp/docker_from && cd /tmp/docker_from

cat > Dockerfile << 'EOF'
FROM alpine:latest

# Copiar el binario de curl desde la imagen oficial
COPY --from=curlimages/curl:latest /usr/bin/curl /usr/local/bin/curl

# Verificar
RUN curl --version

CMD ["curl", "-s", "https://httpbin.org/ip"]
EOF

docker build -t from-test .
docker run --rm from-test

docker rmi from-test
rm -rf /tmp/docker_from
```
