# T01 — Orden de instrucciones y caché

## Cómo funciona la caché de capas

Cada instrucción en un Dockerfile produce una **capa**. Docker almacena estas capas
en caché y las reutiliza si la instrucción y sus inputs no han cambiado. Esto
hace que los builds subsecuentes sean mucho más rápidos.

```
Primer build (sin caché):
  FROM debian:bookworm       → descarga/crea capa     [5s]
  RUN apt-get install gcc    → ejecuta y cachea       [45s]
  COPY requirements.txt .    → copia y cachea         [0.1s]
  RUN pip install -r req...  → ejecuta y cachea       [30s]
  COPY . .                   → copia y cachea         [0.5s]
  CMD ["python3", "app.py"]  → metadata               [0.1s]
                                              Total:   ~81s

Segundo build (código fuente cambió, deps no):
  FROM debian:bookworm       → CACHED                 [0s]
  RUN apt-get install gcc    → CACHED                 [0s]
  COPY requirements.txt .    → CACHED (no cambió)     [0s]
  RUN pip install -r req...  → CACHED                 [0s]
  COPY . .                   → NUEVA (código cambió)  [0.5s]
  CMD ["python3", "app.py"]  → NUEVA (invalidada)     [0.1s]
                                              Total:   ~0.6s
```

La caché convierte un build de 81 segundos en uno de menos de 1 segundo cuando
solo cambió el código fuente.

## La regla de invalidación en cascada

Cuando una capa se invalida, **todas las capas posteriores se reconstruyen**,
aunque no hayan cambiado:

```
Capa 1: FROM         → CACHED  ✓
Capa 2: RUN install  → CACHED  ✓
Capa 3: COPY . .     → CAMBIÓ  ✗ ← código fuente modificado
Capa 4: RUN pip      → INVALIDADA (reconstruida aunque deps no cambiaron)
Capa 5: CMD          → INVALIDADA
```

Esto es la **invalidación en cascada**: un cambio en la capa 3 fuerza la
reconstrucción de las capas 4 y 5.

## El orden importa

El orden de las instrucciones determina qué tan efectiva es la caché. La regla es:

**Ordenar de menos frecuente a más frecuente en cambios**:

```
Cambia raramente    → FROM (imagen base)
                    → RUN apt-get install (dependencias del sistema)
                    → COPY package.json / requirements.txt (manifest de deps)
                    → RUN npm install / pip install (instalar deps)
Cambia cada commit  → COPY . . (código fuente)
                    → CMD (casi nunca cambia)
```

### Ejemplo: orden correcto (dependencias primero)

```dockerfile
FROM python:3.12-slim

# Paso 1: dependencias del sistema (cambian muy raramente)
RUN apt-get update && \
    apt-get install -y --no-install-recommends libpq-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Paso 2: manifest de dependencias (cambia al añadir/quitar deps)
COPY requirements.txt .

# Paso 3: instalar dependencias (solo se re-ejecuta si requirements.txt cambió)
RUN pip install --no-cache-dir -r requirements.txt

# Paso 4: código fuente (cambia en cada commit)
COPY . .

CMD ["python3", "app.py"]
```

Cambiar un archivo `.py` solo invalida la capa `COPY . .` y posteriores. Las
dependencias (que tardan más en instalarse) se reutilizan de la caché.

### Ejemplo: orden incorrecto (todo se invalida)

```dockerfile
FROM python:3.12-slim

WORKDIR /app

# MAL: COPY . . incluye todo — cualquier cambio en el código invalida esta capa
COPY . .

# Esta capa se re-ejecuta cada vez que cambias cualquier archivo
RUN pip install --no-cache-dir -r requirements.txt

CMD ["python3", "app.py"]
```

Cambiar un archivo `.py` invalida `COPY . .`, lo que invalida `RUN pip install`,
reinstalando todas las dependencias en cada build.

## Cuándo se invalida la caché

### COPY y ADD

Se invalida cuando el **checksum** de los archivos copiados cambia. Docker
calcula un hash del contenido (no del timestamp):

```bash
# Estos dos builds usan la caché porque el contenido no cambió
touch main.c            # Cambia el timestamp
docker build -t test .  # CACHED — Docker compara contenido, no timestamps
```

### RUN

Se invalida cuando el **string del comando** cambia:

```dockerfile
# Estos dos RUN son diferentes capas (strings diferentes)
RUN apt-get install -y gcc
RUN apt-get install -y gcc make
```

**Problema**: `apt-get update` sin cambiar el string usa la caché vieja, con
listas de paquetes desactualizadas:

```dockerfile
# MAL: apt-get update cacheado puede tener paquetes obsoletos
RUN apt-get update
RUN apt-get install -y gcc  # Si el string no cambió, usa caché vieja

# BIEN: combinar update + install en un solo RUN
RUN apt-get update && apt-get install -y gcc && rm -rf /var/lib/apt/lists/*
# Si necesitas añadir un paquete, el string cambia → todo se re-ejecuta
```

### FROM

Se invalida si la imagen base tiene un nuevo digest (nueva versión publicada con
el mismo tag). En la práctica, Docker verifica el digest local vs remoto.

## Patrón: dependencias primero por lenguaje

### Node.js

```dockerfile
FROM node:20-slim
WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --production

COPY . .
CMD ["node", "index.js"]
```

### Python

```dockerfile
FROM python:3.12-slim
WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .
CMD ["python3", "app.py"]
```

### Go

```dockerfile
FROM golang:1.22 AS builder
WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN go build -o /app/server

FROM scratch
COPY --from=builder /app/server /server
CMD ["/server"]
```

### Rust

```dockerfile
FROM rust:1.75 AS builder
WORKDIR /app

COPY Cargo.toml Cargo.lock ./
RUN mkdir src && echo 'fn main() {}' > src/main.rs && cargo build --release
RUN rm -rf src

COPY src/ src/
RUN touch src/main.rs && cargo build --release

FROM debian:bookworm-slim
COPY --from=builder /app/target/release/myapp /usr/local/bin/
CMD ["myapp"]
```

El truco de Rust es crear un `main.rs` dummy para que `cargo build` descargue
y compile las dependencias. Luego se copia el código real y se recompila solo
lo que cambió.

## `--no-cache`: rebuild completo

```bash
# Forzar la reconstrucción de TODAS las capas
docker build --no-cache -t myapp .
```

Útil cuando:
- Quieres incorporar actualizaciones de seguridad de la imagen base
- Sospechas que la caché tiene datos obsoletos
- Necesitas un build limpio y reproducible

## BuildKit: caché avanzado

BuildKit (habilitado por defecto en versiones recientes de Docker) ofrece
mejoras de caché:

### Cache mounts

Persisten el caché de un package manager entre builds, sin incluirlo en la imagen:

```dockerfile
# syntax=docker/dockerfile:1

# El caché de pip persiste entre builds (no se incluye en la imagen)
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# El caché de apt persiste entre builds
RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt/lists \
    apt-get update && apt-get install -y gcc
```

### Cache export/import

Exportar la caché a un registry para compartirla entre máquinas de CI/CD:

```bash
# Exportar caché al registry
docker build --cache-to type=registry,ref=myrepo/cache:latest -t myapp .

# Importar caché del registry
docker build --cache-from type=registry,ref=myrepo/cache:latest -t myapp .
```

## Verificar qué capas usaron caché

```bash
docker build -t myapp . 2>&1 | grep -E '(CACHED|RUN|COPY)'
# [2/6] RUN apt-get update ... CACHED
# [3/6] COPY requirements.txt . CACHED
# [4/6] RUN pip install ... CACHED
# [5/6] COPY . .
# [6/6] CMD ...
```

Las capas marcadas `CACHED` no se re-ejecutaron — se reutilizó la versión anterior.

---

## Ejercicios

### Ejercicio 1 — Demostrar la invalidación en cascada

```bash
mkdir -p /tmp/docker_cache && cd /tmp/docker_cache

echo 'print("v1")' > app.py
echo 'flask' > requirements.txt

cat > Dockerfile << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
CMD ["python3", "app.py"]
EOF

# Primer build: todo se construye
docker build -t cache-test . 2>&1 | grep -E '\[.*/.*\]'

# Modificar solo el código
echo 'print("v2")' > app.py

# Segundo build: solo COPY . . se re-ejecuta
docker build -t cache-test . 2>&1 | grep -E '\[.*/.*\]'
# Las capas de requirements.txt y pip install están CACHED

docker rmi cache-test
rm -rf /tmp/docker_cache
```

### Ejercicio 2 — Orden incorrecto vs correcto

```bash
mkdir -p /tmp/docker_order && cd /tmp/docker_order

echo 'print("app")' > app.py
echo 'flask' > requirements.txt

# Orden INCORRECTO
cat > Dockerfile.bad << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY . .
RUN pip install --no-cache-dir -r requirements.txt
CMD ["python3", "app.py"]
EOF

# Orden CORRECTO
cat > Dockerfile.good << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
CMD ["python3", "app.py"]
EOF

# Build inicial de ambos
docker build -f Dockerfile.bad -t order-bad . > /dev/null 2>&1
docker build -f Dockerfile.good -t order-good . > /dev/null 2>&1

# Cambiar solo el código
echo 'print("v2")' > app.py

# Rebuild incorrecto: pip install se re-ejecuta (innecesariamente)
echo "=== Orden incorrecto ==="
docker build -f Dockerfile.bad -t order-bad . 2>&1 | grep -E '(CACHED|pip install)'

# Rebuild correcto: pip install está CACHED
echo "=== Orden correcto ==="
docker build -f Dockerfile.good -t order-good . 2>&1 | grep -E '(CACHED|pip install)'

docker rmi order-bad order-good
rm -rf /tmp/docker_order
```

### Ejercicio 3 — Caché de RUN por string

```bash
mkdir -p /tmp/docker_run_cache && cd /tmp/docker_run_cache

cat > Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "build step 1"
RUN echo "build step 2"
CMD ["echo", "done"]
EOF

# Primer build
docker build -t run-cache . 2>&1 | grep -E '\[.*/.*\]'

# Rebuild sin cambios: todo CACHED
docker build -t run-cache . 2>&1 | grep -E '\[.*/.*\]'

# Cambiar el step 1: step 2 también se invalida
cat > Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "build step 1 modified"
RUN echo "build step 2"
CMD ["echo", "done"]
EOF

docker build -t run-cache . 2>&1 | grep -E '\[.*/.*\]'
# Ambos RUN se re-ejecutan por invalidación en cascada

docker rmi run-cache
rm -rf /tmp/docker_run_cache
```
