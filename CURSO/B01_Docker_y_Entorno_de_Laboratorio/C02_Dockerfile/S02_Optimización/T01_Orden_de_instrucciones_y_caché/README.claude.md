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

Se invalida cuando el **checksum del contenido** de los archivos copiados cambia.
Docker compara el hash del contenido, no los timestamps:

```bash
# Estos dos builds usan la caché porque el contenido no cambió
touch main.c            # Cambia el timestamp
docker build -t test .  # CACHED — Docker compara contenido, no timestamps
```

**Importante**: `COPY . .` compara el contenido de **todos** los archivos
copiados contra la capa anterior. Si cualquier archivo cambió, toda la capa
se invalida.

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

### ENV y ARG

Las variables pueden afectar la caché de formas sutiles:

```dockerfile
ARG APP_VERSION
ENV APP_VERSION=${APP_VERSION}
RUN echo "Building version $APP_VERSION"

# Si APP_VERSION cambia entre builds (diferente --build-arg),
# la capa RUN se invalida aunque el string del comando no cambió
```

### FROM

Se invalida si la imagen base tiene un nuevo digest (nueva versión publicada con
el mismo tag):

```bash
# Ver el digest actual de una imagen local
docker inspect debian:bookworm --format '{{index .RepoDigests 0}}'
# debian@sha256:abc123...

# Forzar actualización de la imagen base
docker pull debian:bookworm
# Si el digest cambió, el próximo build invalida FROM y todo lo demás
```

## Patrón: dependencias primero por lenguaje

### Node.js

```dockerfile
FROM node:20-slim
WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --omit=dev

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

# Truco: compilar dependencias con un main.rs dummy
COPY Cargo.toml Cargo.lock ./
RUN mkdir src && echo 'fn main() {}' > src/main.rs && cargo build --release
RUN rm -rf src

# Copiar código real y recompilar (solo lo que cambió)
COPY src/ src/
RUN touch src/main.rs && cargo build --release

FROM debian:bookworm-slim
COPY --from=builder /app/target/release/myapp /usr/local/bin/
CMD ["myapp"]
```

El truco de Rust es crear un `main.rs` dummy para que `cargo build` descargue
y compile las dependencias. Luego se copia el código real y se recompila solo
lo que cambió. `touch src/main.rs` fuerza a cargo a recompilar el código
(sin recompilar las dependencias).

### Java / Maven

```dockerfile
FROM maven:3.9-eclipse-temurin-21 AS builder
WORKDIR /app

# Primero solo el POM (manifest de dependencias)
COPY pom.xml .
RUN mvn dependency:go-offline -B

# Luego el código fuente
COPY src ./src
RUN mvn package -DskipTests

FROM eclipse-temurin:21-jre
COPY --from=builder /app/target/myapp.jar /app/
CMD ["java", "-jar", "/app/myapp.jar"]
```

### Ruby (Bundler)

```dockerfile
FROM ruby:3.2-slim
WORKDIR /app

COPY Gemfile Gemfile.lock ./
RUN bundle install --jobs=4 --retry=3

COPY . .
CMD ["ruby", "app.rb"]
```

En todos los casos, el patrón es el mismo: **copiar el manifest de dependencias
primero, instalar, y luego copiar el código fuente**.

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

BuildKit (habilitado por defecto desde Docker 23.0) ofrece mejoras de caché:

### Cache mounts

Persisten el caché de un package manager entre builds, sin incluirlo en la imagen:

```dockerfile
# syntax=docker/dockerfile:1

# El caché de pip persiste entre builds (no se incluye en la imagen)
# NOTA: NO usar --no-cache-dir con cache mounts — son opuestos
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# El caché de apt persiste entre builds
RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt/lists \
    apt-get update && apt-get install -y gcc
```

**Sin cache mount**: cada build descarga todos los paquetes desde cero.
**Con cache mount**: los paquetes descargados persisten en un volumen dedicado
que no se incluye en la imagen final.

> **Importante**: `--no-cache-dir` (de pip) y `--mount=type=cache` son opuestos.
> `--no-cache-dir` dice "no uses caché" y `--mount=type=cache` dice "usa este
> directorio como caché persistente". Usar ambos juntos anula el cache mount.

### Cache export/import

Exportar la caché a un registry para compartirla entre máquinas de CI/CD:

```bash
# Exportar caché al registry
docker build --cache-to type=registry,ref=myrepo/cache:latest -t myapp .

# Importar caché del registry
docker build --cache-from type=registry,ref=myrepo/cache:latest -t myapp .
```

## .dockerignore y caché

Si un archivo está en `.dockerignore`, **no se copia** con `COPY . .`, por lo
que **no puede invalidar la caché**:

```
# .dockerignore
.git
*.md
node_modules/
__pycache__/
.env
```

Sin `.dockerignore`, archivos como `.git` (que cambia en cada commit) o
`node_modules/` invalidarían la caché de `COPY . .` innecesariamente.

## Verificar qué capas usaron caché

```bash
# BuildKit muestra CACHED para capas reutilizadas
docker build -t myapp . 2>&1 | grep -E '(CACHED|RUN|COPY)'
# [2/6] RUN apt-get update ... CACHED
# [3/6] COPY requirements.txt . CACHED
# [4/6] RUN pip install ... CACHED
# [5/6] COPY . .
# [6/6] CMD ...

# Para ver todo el output (incluyendo RUN steps):
docker build --progress=plain -t myapp .
```

---

## Práctica

> **Nota**: este tópico tiene un directorio `labs/` con Dockerfiles preparados
> para ejercicios más detallados.

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

**Predicción**: en el segundo build, `COPY requirements.txt` y `RUN pip install`
aparecen como CACHED (no cambiaron). Solo `COPY . .` se re-ejecuta porque
`app.py` cambió. La invalidación en cascada afecta a CMD (posterior), pero no
a las capas anteriores al cambio.

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
docker build -f Dockerfile.bad -t order-bad . 2>&1 | grep -iE '(CACHED|pip)'

# Rebuild correcto: pip install está CACHED
echo "=== Orden correcto ==="
docker build -f Dockerfile.good -t order-good . 2>&1 | grep -iE '(CACHED|pip)'

docker rmi order-bad order-good
rm -rf /tmp/docker_order
```

**Predicción**: en el orden incorrecto, `COPY . .` está antes de
`RUN pip install`. Cambiar `app.py` invalida `COPY . .`, que en cascada
invalida `RUN pip install` — reinstalando flask innecesariamente. En el orden
correcto, `COPY requirements.txt` no cambió, así que `pip install` usa caché.

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
docker build --progress=plain -t run-cache . 2>&1 | grep -E '(CACHED|\[.*/.*\])'

# Rebuild sin cambios: todo CACHED
docker build --progress=plain -t run-cache . 2>&1 | grep -E '(CACHED|\[.*/.*\])'

# Cambiar solo step 1
cat > Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "build step 1 modified"
RUN echo "build step 2"
CMD ["echo", "done"]
EOF

docker build --progress=plain -t run-cache . 2>&1 | grep -E '(CACHED|\[.*/.*\])'
# Step 1: se re-ejecuta (string cambió)
# Step 2: también se re-ejecuta (invalidación en cascada)

docker rmi run-cache
rm -rf /tmp/docker_run_cache
```

**Predicción**: sin cambios, todo está CACHED. Al modificar el string del
primer RUN, esa capa se invalida. El segundo RUN tiene el mismo string que
antes, pero se re-ejecuta de todos modos por invalidación en cascada (la
capa anterior cambió).

### Ejercicio 4 — Timestamp vs contenido

```bash
mkdir -p /tmp/docker_ts && cd /tmp/docker_ts

echo 'hello' > file.txt
cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY file.txt .
CMD ["cat", "file.txt"]
EOF

docker build -t ts-test . > /dev/null 2>&1

# Cambiar timestamp pero NO contenido
sleep 2
touch file.txt

echo "=== Mismo contenido, timestamp diferente (debe usar CACHE) ==="
docker build -t ts-test . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

# Cambiar contenido
echo 'hello world' > file.txt

echo "=== Contenido diferente (debe reconstruir) ==="
docker build -t ts-test . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

docker rmi ts-test
rm -rf /tmp/docker_ts
```

**Predicción**: `touch` cambia el timestamp pero no el contenido — Docker
usa CACHED porque compara checksums, no timestamps. Al cambiar el contenido
(`hello` → `hello world`), el checksum cambia y la capa se invalida. Esto
es diferente a `make` que sí usa timestamps.

### Ejercicio 5 — BuildKit cache mounts

```bash
mkdir -p /tmp/docker_bk_cache && cd /tmp/docker_bk_cache

echo 'flask==3.0.0' > requirements.txt

# SIN cache mount (descarga desde cero si se invalida la capa)
cat > Dockerfile.no-mount << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
CMD ["python3", "-c", "import flask; print(f'Flask {flask.__version__}')"]
EOF

# CON cache mount (pip reutiliza descargas previas)
cat > Dockerfile.with-mount << 'EOF'
# syntax=docker/dockerfile:1
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt
CMD ["python3", "-c", "import flask; print(f'Flask {flask.__version__}')"]
EOF

# Build de ambos
docker build -f Dockerfile.no-mount -t bk-no-mount . > /dev/null 2>&1
docker build -f Dockerfile.with-mount -t bk-with-mount . > /dev/null 2>&1

# Forzar rebuild (--no-cache) y medir tiempos
echo "=== Sin cache mount (--no-cache) ==="
time docker build --no-cache -f Dockerfile.no-mount -t bk-no-mount . > /dev/null 2>&1

echo "=== Con cache mount (--no-cache) ==="
time docker build --no-cache -f Dockerfile.with-mount -t bk-with-mount . > /dev/null 2>&1

# Verificar que ambas funcionan
docker run --rm bk-no-mount
docker run --rm bk-with-mount

docker rmi bk-no-mount bk-with-mount
rm -rf /tmp/docker_bk_cache
```

**Predicción**: con `--no-cache`, ambas reconstruyen todas las capas. Pero el
cache mount persiste las descargas de pip en un volumen externo — en el segundo
build, pip no necesita descargar flask de Internet porque ya está en el caché
del mount. Sin cache mount, pip descarga todo de nuevo. La diferencia se nota
más con muchas dependencias.

### Ejercicio 6 — .dockerignore y caché

```bash
mkdir -p /tmp/docker_ignore && cd /tmp/docker_ignore

echo 'print("app")' > app.py
echo 'Initial README' > README.md
mkdir -p .git && echo 'fake git data' > .git/HEAD

cat > Dockerfile << 'EOF'
FROM alpine:latest
COPY . /app/
CMD ["ls", "/app/"]
EOF

# Sin .dockerignore: COPY incluye todo
docker build -t ignore-test . > /dev/null 2>&1

# Cambiar solo README.md (no es código)
echo 'Updated README' > README.md
echo "=== Sin .dockerignore (README invalida COPY) ==="
docker build -t ignore-test . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

# Crear .dockerignore
cat > .dockerignore << 'EOF'
.git
*.md
EOF

# Rebuild: README.md ya no afecta la caché
echo "=== Con .dockerignore (README ignorado) ==="
docker build -t ignore-test . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

# Verificar qué se copió
docker run --rm ignore-test
# Solo app.py (no README.md ni .git)

docker rmi ignore-test
rm -rf /tmp/docker_ignore
```

**Predicción**: sin `.dockerignore`, cambiar `README.md` invalida `COPY . .`
y toda la caché posterior. Con `.dockerignore` excluyendo `*.md` y `.git`,
esos archivos ni se copian — cambiarlos no afecta la caché. Menos archivos
copiados = menos invalidaciones = builds más rápidos.

### Ejercicio 7 — Medir impacto real del orden

```bash
mkdir -p /tmp/docker_timing && cd /tmp/docker_timing

echo 'print("app")' > app.py
echo -e 'flask\nrequests\njinja2' > requirements.txt

cat > Dockerfile.bad << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY . .
RUN pip install --no-cache-dir -r requirements.txt
CMD ["python3", "app.py"]
EOF

cat > Dockerfile.good << 'EOF'
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
CMD ["python3", "app.py"]
EOF

# Build inicial
docker build -f Dockerfile.bad -t timing-bad . > /dev/null 2>&1
docker build -f Dockerfile.good -t timing-good . > /dev/null 2>&1

# Simular cambio de código
echo 'print("v2")' > app.py

echo "=== Rebuild orden incorrecto ==="
time docker build -f Dockerfile.bad -t timing-bad . > /dev/null 2>&1

echo "=== Rebuild orden correcto ==="
time docker build -f Dockerfile.good -t timing-good . > /dev/null 2>&1

docker rmi timing-bad timing-good
rm -rf /tmp/docker_timing
```

**Predicción**: el rebuild con orden incorrecto tarda significativamente más
porque reinstala flask, requests y jinja2 (y todas sus dependencias). El
rebuild con orden correcto tarda <1 segundo porque solo copia el archivo
cambiado. La diferencia se amplifica con más dependencias.

### Ejercicio 8 — Caché con ARG

```bash
mkdir -p /tmp/docker_arg_cache && cd /tmp/docker_arg_cache

cat > Dockerfile << 'EOF'
FROM alpine:latest
ARG APP_VERSION=1.0
RUN echo "Building version $APP_VERSION" > /version.txt
CMD ["cat", "/version.txt"]
EOF

# Build con version 1.0
docker build --build-arg APP_VERSION=1.0 -t arg-cache . > /dev/null 2>&1

# Rebuild con el mismo ARG: usa caché
echo "=== Mismo ARG ==="
docker build --build-arg APP_VERSION=1.0 -t arg-cache . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

# Rebuild con ARG diferente: se invalida
echo "=== ARG diferente ==="
docker build --build-arg APP_VERSION=2.0 -t arg-cache . 2>&1 | grep -iE '(CACHED|\[.*/.*\])'

docker run --rm arg-cache
# Building version 2.0

docker rmi arg-cache
rm -rf /tmp/docker_arg_cache
```

**Predicción**: con el mismo `--build-arg`, la caché se reutiliza. Con un valor
diferente, la capa `RUN` se invalida porque Docker detecta que el ARG cambió —
aunque el string del comando sea idéntico. ARGs afectan la caché de forma
implícita.

---

## Resumen visual

```
┌──────────────────────────────────────────────────────┐
│            ORDEN DE INSTRUCCIONES                    │
├──────────────────────────────────────────────────────┤
│                                                      │
│  CAMBIOS RAROS (arriba)                              │
│  ┌──────────────────────────────────────────────┐    │
│  │ FROM python:3.12-slim                        │    │
│  │ RUN apt-get update && install gcc            │    │
│  │ COPY requirements.txt .                      │    │
│  │ RUN pip install -r requirements.txt          │    │
│  └──────────────────────────────────────────────┘    │
│                       │                              │
│                       │ Si code cambia, solo esto:   │
│                       ▼                              │
│  CAMBIOS FRECUENTES (abajo)                          │
│  ┌──────────────────────────────────────────────┐    │
│  │ COPY . .           ← se re-ejecuta           │    │
│  │ CMD ["python3", "app.py"]                     │    │
│  └──────────────────────────────────────────────┘    │
│                                                      │
│  INVERSIÓN = rebuild de todas las capas              │
└──────────────────────────────────────────────────────┘
```

## Checklist de optimización

- Las dependencias se copian **antes** que el código fuente
- `COPY requirements.txt` está **antes** de `RUN pip install`
- `apt-get update` y `apt-get install` están en el **mismo** RUN
- `.dockerignore` excluye archivos innecesarios (.git, *.md, node_modules)
- Las capas estáticas (FROM, RUN base) están al inicio del Dockerfile
- `RUN` que generan caché de package managers usan `--mount=type=cache` (BuildKit)
- `--no-cache-dir` NO se usa junto con `--mount=type=cache` (son opuestos)
