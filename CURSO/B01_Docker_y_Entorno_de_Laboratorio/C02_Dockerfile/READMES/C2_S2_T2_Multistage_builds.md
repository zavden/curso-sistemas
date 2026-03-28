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

### Ventajas de nombres vs índices

| Aspecto | Por nombre `AS builder` | Por índice `COPY --from=0` |
|---|---|---|
| Legibilidad | Clara | Confusa |
| Robustez | Agregar stages no rompe referencias | Agregar stages puede romper índices |
| Reordenamiento | Los nombres se mantienen | Los índices cambian |

**Recomendación**: siempre usar nombres, no índices.

### Copiar desde imágenes externas

`--from` puede referenciar cualquier imagen, no solo stages del mismo Dockerfile:

```dockerfile
FROM debian:bookworm-slim

# Copiar un binario de otra imagen
COPY --from=golang:1.22 /usr/local/go/bin/go /usr/local/bin/go

# Copiar configuración de otra imagen
COPY --from=nginx:latest /etc/nginx/ /etc/nginx/
```

**Nota importante**: copiar binarios entre imágenes solo funciona de forma
confiable si el binario es estático o si las librerías compartidas que necesita
están presentes en la imagen destino. Para archivos de configuración, datos o
binarios estáticos no hay problema.

### Copiar directorios completos

```dockerfile
# Copiar directorio
COPY --from=builder /app/dist ./dist

# Copiar archivos individuales
COPY --from=builder /app/server /server
COPY --from=builder /app/config.json /config.json
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
RUN npm ci --omit=dev
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

### Java / Maven

```dockerfile
FROM maven:3.9-eclipse-temurin-21 AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B

COPY src ./src
RUN mvn package -DskipTests

FROM eclipse-temurin:21-jre
WORKDIR /app
COPY --from=builder /app/target/myapp.jar /app/
CMD ["java", "-jar", "/app/myapp.jar"]
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

### Objetivos comunes para --target

| Target | Propósito |
|---|---|
| `builder` | Solo compilar, sin ejecutar |
| `development` | Imagen con herramientas de debug |
| `test` | Imagen con frameworks de test |
| `production` | Imagen mínima de producción |

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
# Build solo hasta los tests
docker build --target tester -t myapp:test .

# Build sin tests (solo production, salta tester)
docker build --target production -t myapp:prod .
```

**Detalle importante sobre BuildKit**: si construyes el target `production`,
BuildKit **no ejecuta** el stage `tester` porque `production` depende de
`builder`, no de `tester`. BuildKit solo ejecuta los stages que son
dependencias (directas o transitivas) del target solicitado.

Esto significa que un `docker build -t myapp .` (sin `--target`) con el
Dockerfile de arriba tampoco ejecutará `tester`, porque el stage final
(`production`) no depende de él. Para forzar la ejecución de tests, usa
`--target tester` o haz que el stage final dependa de `tester`:

```dockerfile
# Alternativa: hacer que production dependa de tester
FROM builder AS tester
RUN go test -v ./...

FROM scratch AS production
COPY --from=tester /app/server /server
#                   ^^^^^^ depende de tester, que a su vez hereda de builder
CMD ["/server"]
```

Ahora `production` depende de `tester` → BuildKit ejecuta los tests siempre.

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

## `COPY --from` con `--link` (BuildKit)

```dockerfile
# Sin --link (comportamiento clásico)
COPY --from=builder /app/bin /app/bin

# Con --link (BuildKit)
COPY --from=builder --link /app/bin /app/bin
```

`--link` crea la capa COPY como una capa **independiente** de las capas
anteriores del stage actual. La ventaja es que si las capas previas del
stage final cambian, la capa de COPY no necesita recalcularse — puede
reutilizarse directamente. Esto acelera los rebuilds en multi-stage builds.

Sin `--link`, un cambio en cualquier capa anterior invalida todas las capas
posteriores (incluidas las COPY), aunque los archivos copiados no hayan
cambiado.

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

## Combinación con BuildKit cache mounts

Los cache mounts persisten entre builds y son especialmente útiles en stages
de build para cachear dependencias descargadas:

```dockerfile
FROM golang:1.22 AS builder
WORKDIR /app

COPY go.mod go.sum ./
RUN --mount=type=cache,target=/root/go/pkg/mod \
    go mod download

COPY . .
RUN --mount=type=cache,target=/root/.cache/go-build \
    CGO_ENABLED=0 go build -o server .

FROM scratch
COPY --from=builder /app/server /server
CMD ["/server"]
```

Los cache mounts (`/root/go/pkg/mod` y `/root/.cache/go-build`) persisten
entre builds pero **no se incluyen en la imagen final** — ni siquiera en la
imagen del stage de build.

## Comparación de tamaños

| Lenguaje | Single-stage | Multi-stage | Reducción |
|---|---|---|---|
| C (con gcc) | ~350 MB | ~75 MB (slim) / ~1 MB (scratch) | 78-99% |
| Go | ~800 MB | ~10 MB (scratch) | 99% |
| Rust | ~2 GB | ~80 MB (slim) | 96% |
| Node.js | ~1 GB | ~200 MB (slim, sin devDeps) | 80% |
| Python | ~900 MB | ~150 MB (slim) | 83% |
| Java (Maven + JRE) | ~500 MB | ~200 MB | 60% |

## Errores comunes

### 1. Copiar antes de compilar

```dockerfile
# INCORRECTO: copia todo antes de compilar
FROM golang:1.22 AS builder
WORKDIR /app
COPY . .           # Copia todo primero
RUN go build ...   # Cualquier cambio de código invalida este RUN

# CORRECTO: separar deps del código
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build ...
```

### 2. Depender de un stage sin copiar

```dockerfile
# INCORRECTO: el stage 'builder' tiene el binario pero nunca se copia
FROM golang:1.22 AS builder
RUN go build -o /app/server .

FROM debian:bookworm-slim
# Falta: COPY --from=builder /app/server /server
CMD ["/bin/bash"]  # No hay nada útil en esta imagen
```

### 3. Asumir que todos los stages se ejecutan

```dockerfile
FROM alpine AS builder
RUN echo "Building..."

FROM builder AS tester
RUN echo "Testing..."   # ¿Se ejecuta siempre?

FROM alpine
CMD ["echo", "Done"]
```

Con BuildKit, el stage `tester` **no se ejecuta** porque el stage final no
depende de él. Para que se ejecute, el stage final debe depender de `tester`
(vía `COPY --from=tester`) o usar `--target tester`.

---

**Nota**: el directorio `labs/` contiene un laboratorio completo con 4 partes
y Dockerfiles listos para usar. Consultar `LABS.md` para la guía.

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
```

**Predicción**: ¿cuántas veces más pequeña será la multi-stage? La imagen
`gcc:12` pesa ~1.3 GB. La multi-stage con `FROM scratch` solo contiene el
binario (~1 MB). Diferencia de ~1000x.

```bash
# Ambas funcionan
docker run --rm single-test
docker run --rm multi-test
# Hello from multi-stage build!

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
```

**Predicción**: `target-dev` será más grande que `target-prod` porque incluye
ipython y debugpy. `target-prod` no tiene herramientas de desarrollo extra.

```bash
docker run --rm target-prod
# Running application

# Verificar que producción corre como non-root
docker run --rm target-prod id
# uid=65534(nobody) gid=65534(nobody)

docker rmi target-dev target-prod
rm -rf /tmp/docker_target
```

### Ejercicio 3 — Copiar archivos desde imagen externa

```bash
mkdir -p /tmp/docker_from && cd /tmp/docker_from

cat > Dockerfile << 'EOF'
FROM debian:bookworm-slim

# Copiar archivos de configuración de otras imágenes
COPY --from=alpine:latest /etc/os-release /alpine-os-release
COPY --from=nginx:latest /etc/nginx/nginx.conf /nginx-default.conf

CMD ["sh", "-c", "echo '=== Esta imagen (Debian) ===' && head -2 /etc/os-release && echo && echo '=== Copiado de Alpine ===' && head -2 /alpine-os-release && echo && echo '=== Config de nginx ===' && head -5 /nginx-default.conf"]
EOF

docker build -t from-test .
docker run --rm from-test
```

**Predicción**: la imagen Debian tendrá archivos copiados de Alpine y nginx
sin necesidad de instalar esos paquetes. Se verán los headers de tres fuentes
distintas. Copiar archivos de texto/configuración entre imágenes siempre
funciona; copiar binarios requiere que las librerías compartidas estén
presentes en la imagen destino.

```bash
docker rmi from-test
rm -rf /tmp/docker_from
```

### Ejercicio 4 — BuildKit salta stages sin dependencias

Este ejercicio demuestra que BuildKit solo ejecuta los stages necesarios.

```bash
mkdir -p /tmp/docker_skip && cd /tmp/docker_skip

cat > Dockerfile << 'EOF'
FROM alpine:latest AS builder
RUN echo "Stage builder ejecutado"

FROM builder AS tester
RUN echo "Stage tester ejecutado" && exit 1

FROM alpine:latest AS production
CMD ["echo", "Build exitoso — tester NO se ejecutó"]
EOF

# Build del stage final (production)
docker build --progress=plain -t skip-test . 2>&1
```

**Predicción**: el build **tiene éxito** a pesar de que `tester` tiene
`exit 1`. BuildKit ve que `production` usa `FROM alpine:latest` (no depende
de `builder` ni de `tester`), así que salta ambos stages intermedios. Si no
ves "Stage tester ejecutado" en la salida, confirma que BuildKit lo saltó.

```bash
docker run --rm skip-test
# Build exitoso — tester NO se ejecutó

# Ahora intenta construir el stage tester directamente
docker build --progress=plain --target tester -t skip-tester . 2>&1 || echo "Build falló (esperado)"
# El build falla porque exit 1 se ejecuta

docker rmi skip-test 2>/dev/null
rm -rf /tmp/docker_skip
```

### Ejercicio 5 — Hacer que tests bloqueen el build

```bash
mkdir -p /tmp/docker_testgate && cd /tmp/docker_testgate

cat > app.sh << 'EOF'
#!/bin/sh
echo "App running"
EOF

cat > Dockerfile << 'EOF'
FROM alpine:latest AS builder
WORKDIR /app
COPY app.sh .
RUN chmod +x app.sh

FROM builder AS tester
RUN echo "Running tests..." && sh app.sh && echo "Tests passed"

# production depende de tester (no de builder)
FROM scratch AS production
COPY --from=tester /app/app.sh /app.sh
CMD ["/app.sh"]
EOF

# El build ejecuta tester porque production depende de él
docker build --progress=plain -t testgate . 2>&1
```

**Predicción**: a diferencia del ejercicio 4, aquí `production` usa
`COPY --from=tester`, lo que crea una dependencia. BuildKit **debe** ejecutar
`tester` antes de poder construir `production`. Si los tests fallan (cambiar
el RUN a `exit 1`), el build entero falla.

```bash
docker run --rm testgate
# App running

docker rmi testgate
rm -rf /tmp/docker_testgate
```

### Ejercicio 6 — Paralelización de stages con BuildKit

```bash
mkdir -p /tmp/docker_parallel/{frontend,backend} && cd /tmp/docker_parallel

echo '<h1>Hello</h1>' > frontend/index.html

cat > backend/info.sh << 'SCRIPT'
#!/bin/sh
echo "Backend ready"
cat /var/www/html/index.html
SCRIPT

cat > Dockerfile << 'EOF'
FROM alpine:latest AS frontend
RUN echo "Building frontend..." && sleep 2
WORKDIR /web
COPY frontend/index.html .

FROM alpine:latest AS backend
RUN echo "Building backend..." && sleep 2
WORKDIR /api
COPY backend/info.sh .
RUN chmod +x info.sh

FROM alpine:latest
COPY --from=frontend /web/index.html /var/www/html/
COPY --from=backend /api/info.sh /usr/local/bin/
CMD ["info.sh"]
EOF

# Con --progress=plain puedes ver que frontend y backend se construyen en paralelo
time docker build --progress=plain -t parallel-test . 2>&1
```

**Predicción**: aunque cada stage tiene un `sleep 2`, el build total tarda
~2-3 segundos (no ~4+), porque BuildKit ejecuta `frontend` y `backend` en
paralelo. En la salida de `--progress=plain` verás las líneas de ambos
stages intercaladas.

```bash
docker run --rm parallel-test
# Backend ready
# <h1>Hello</h1>

docker rmi parallel-test
rm -rf /tmp/docker_parallel
```

### Ejercicio 7 — `--link` para COPY más eficiente

```bash
mkdir -p /tmp/docker_link && cd /tmp/docker_link

cat > main.c << 'EOF'
#include <stdio.h>
int main(void) { printf("Link test\n"); return 0; }
EOF

cat > Dockerfile << 'EOF'
FROM gcc:12 AS builder
WORKDIR /src
COPY main.c .
RUN gcc -static -o hello main.c

FROM debian:bookworm-slim
RUN echo "layer-1: setup"
RUN echo "layer-2: config"
COPY --from=builder --link /src/hello /usr/local/bin/hello
CMD ["hello"]
EOF

docker build -t link-test .
docker run --rm link-test
# Link test
```

**Predicción**: el build funciona igual que sin `--link`. La diferencia se
nota en rebuilds: si cambias "layer-1" o "layer-2", sin `--link` la capa COPY
se recalcula. Con `--link`, la capa COPY es independiente y se reutiliza del
caché aunque las capas anteriores cambien.

```bash
docker rmi link-test
rm -rf /tmp/docker_link
```

### Ejercicio 8 — Inspeccionar stages con `docker history`

```bash
mkdir -p /tmp/docker_history && cd /tmp/docker_history

cat > main.c << 'EOF'
#include <stdio.h>
int main(void) { printf("History test\n"); return 0; }
EOF

cat > Dockerfile << 'EOF'
FROM gcc:12 AS builder
WORKDIR /src
COPY main.c .
RUN gcc -static -o hello main.c

FROM debian:bookworm-slim AS runtime
COPY --from=builder /src/hello /usr/local/bin/hello
CMD ["hello"]
EOF

docker build -t history-test .

# Inspeccionar las capas de la imagen final
docker history history-test --format "table {{.CreatedBy}}\t{{.Size}}" | head -8
```

**Predicción**: `docker history` solo muestra las capas del stage final
(`runtime`). No verás ninguna referencia a gcc, la instrucción de compilación,
ni el `COPY main.c`. Solo verás las capas de `debian:bookworm-slim` + la capa
de `COPY --from=builder` + CMD. El stage `builder` fue descartado.

```bash
# Verificar: gcc no existe en la imagen final
docker run --rm history-test which gcc 2>/dev/null || echo "gcc no está (correcto)"

docker rmi history-test
rm -rf /tmp/docker_history
```

---

## Resumen de comandos

```bash
# Build normal (último stage)
docker build -t myapp .

# Build hasta un stage específico
docker build --target production -t myapp:prod .

# Build con progreso detallado (ver paralelización)
docker build --progress=plain -t myapp .

# Limpiar caché de build (stages intermedios)
docker builder prune
```

## Checklist de multi-stage

- [ ] El stage de build tiene `AS nombre` para referenciarlo después
- [ ] Las dependencias se copian antes que el código en el builder
- [ ] El stage final usa `FROM scratch` o una imagen mínima (slim, alpine)
- [ ] `COPY --from=builder` usa nombre, no índice
- [ ] Los stages de test son dependencias del target si deben ejecutarse siempre
- [ ] Se usa `--link` para copias más eficientes (BuildKit)
- [ ] Copiar binarios entre imágenes: verificar compatibilidad de librerías compartidas
