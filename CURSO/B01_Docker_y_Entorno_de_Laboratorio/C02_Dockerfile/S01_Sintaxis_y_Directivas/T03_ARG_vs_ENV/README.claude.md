# T03 — ARG vs ENV

## Dos mecanismos, dos propósitos

Docker tiene dos formas de definir variables en un Dockerfile. Aunque parecen
similares, tienen ciclos de vida completamente diferentes:

```
                 Build time                     Runtime
            ┌──────────────────┐          ┌──────────────────┐
     ARG ─▶ │  docker build    │          │  docker run      │
            │  ✓ Disponible    │          │  ✗ NO disponible │
            └──────────────────┘          └──────────────────┘

            ┌──────────────────┐          ┌──────────────────┐
     ENV ─▶ │  docker build    │          │  docker run      │
            │  ✓ Disponible    │          │  ✓ Disponible    │
            └──────────────────┘          └──────────────────┘
```

| | ARG | ENV |
|---|---|---|
| Disponible durante build | Sí | Sí |
| Disponible en runtime | **No** | **Sí** |
| Se pasa con | `--build-arg` | `-e` / `--env-file` |
| Persiste en la imagen | No (pero visible en `docker history`) | Sí |
| Caso de uso | Parametrizar el build | Configurar la aplicación |
| Puede tener valor por defecto | Sí (en declaración) | Sí (en declaración) |
| Se puede cambiar en runtime | No | Sí (con `-e`) |

## ARG — Variables de build

`ARG` declara variables que solo existen durante la construcción de la imagen:

```dockerfile
# Declarar con valor por defecto
ARG DEBIAN_FRONTEND=noninteractive
ARG APP_VERSION=1.0.0

# Usar en instrucciones
RUN echo "Construyendo versión $APP_VERSION"

# Sin valor por defecto — queda vacío si no se pasa --build-arg
ARG CUSTOM_REPO
RUN echo "Repo: $CUSTOM_REPO"
```

```bash
# Usar valores por defecto
docker build -t myapp .

# Sobrescribir un ARG
docker build --build-arg APP_VERSION=2.0.0 -t myapp .

# Pasar un ARG sin valor por defecto
docker build --build-arg CUSTOM_REPO=https://repo.example.com -t myapp .

# Múltiples ARGs
docker build \
  --build-arg APP_VERSION=2.0.0 \
  --build-arg BUILD_MODE=release \
  -t myapp .
```

> **Nota**: un ARG declarado sin valor por defecto (`ARG CUSTOM_REPO`) **no**
> es obligatorio. Si no se pasa con `--build-arg`, simplemente queda como
> string vacío. Docker muestra un warning, pero el build no falla.

### ARG no existe en runtime

```dockerfile
ARG BUILD_DATE=2024-01-15
RUN echo "Build: $BUILD_DATE"
```

```bash
docker build -t arg-test .
docker run --rm arg-test sh -c 'echo $BUILD_DATE'
# (vacío) — ARG no existe en el contenedor
```

### Scope de ARG

Un ARG es visible desde su declaración hasta el final del **stage actual**:

```dockerfile
# Stage 1
FROM debian:bookworm AS builder
ARG APP_VERSION=1.0     # Visible desde aquí hasta el final del stage
RUN echo $APP_VERSION

# Stage 2
FROM debian:bookworm-slim
# APP_VERSION NO está definido aquí — es otro stage
RUN echo $APP_VERSION   # Vacío
ARG APP_VERSION         # Hay que redeclararlo (sin valor — hereda de --build-arg)
RUN echo $APP_VERSION   # Ahora sí funciona
```

### ARG antes de FROM

Un ARG declarado **antes del primer FROM** solo es visible en las instrucciones
`FROM`. Tiene un scope especial:

```dockerfile
ARG DEBIAN_VERSION=bookworm
FROM debian:${DEBIAN_VERSION}

# DEBIAN_VERSION ya NO está disponible aquí
RUN echo $DEBIAN_VERSION  # Vacío

# Si lo necesitas dentro del stage, hay que redeclararlo
ARG DEBIAN_VERSION
RUN echo $DEBIAN_VERSION  # bookworm
```

```bash
# Cambiar la imagen base desde la línea de comandos
docker build --build-arg DEBIAN_VERSION=bullseye -t myapp .
```

### ARGs predefinidos

Docker reconoce ciertos ARGs sin necesidad de declararlos con `ARG`. Estos son
especiales porque **no aparecen en `docker history`**:

| ARG | Descripción |
|---|---|
| `HTTP_PROXY` / `http_proxy` | Proxy HTTP para el build |
| `HTTPS_PROXY` / `https_proxy` | Proxy HTTPS para el build |
| `FTP_PROXY` / `ftp_proxy` | Proxy FTP para el build |
| `NO_PROXY` / `no_proxy` | Hosts que no usan proxy |
| `BUILDKIT_INLINE_CACHE` | Habilita cache inline para registries remotos |

```bash
# Estos funcionan sin declaración en el Dockerfile
docker build --build-arg HTTP_PROXY=http://proxy:8080 .
docker build --build-arg HTTPS_PROXY=http://proxy:8080 .
```

BuildKit añade ARGs automáticos para multi-arquitectura (disponibles sin
declarar dentro de `FROM`):

| ARG | Ejemplo |
|---|---|
| `TARGETPLATFORM` | `linux/amd64` |
| `TARGETOS` | `linux` |
| `TARGETARCH` | `amd64` |
| `TARGETVARIANT` | `v7` (para arm) |
| `BUILDPLATFORM` | Plataforma donde corre el build |

### ARG en FROM

```dockerfile
# Útil para cambiar la imagen base dinámicamente
ARG BASE_IMAGE=debian
ARG BASE_TAG=bookworm-slim
FROM ${BASE_IMAGE}:${BASE_TAG}
```

```bash
# Construir con diferentes imágenes base
docker build --build-arg BASE_IMAGE=ubuntu --build-arg BASE_TAG=24.04 -t myapp .
```

## ENV — Variables de entorno persistentes

`ENV` define variables que persisten en la imagen y están disponibles en los
contenedores:

```dockerfile
ENV APP_VERSION=1.0
ENV APP_HOME=/app
ENV NODE_ENV=production

# Uso durante el build
RUN echo "Versión: $APP_VERSION"
WORKDIR $APP_HOME

# También disponible en runtime
CMD ["sh", "-c", "echo Running version $APP_VERSION"]
```

```bash
docker build -t env-test .

# La variable existe en el contenedor
docker run --rm env-test env | grep APP_VERSION
# APP_VERSION=1.0

# Sobrescribir en runtime
docker run --rm -e APP_VERSION=2.0 env-test env | grep APP_VERSION
# APP_VERSION=2.0
```

### Agrupar ENV (una capa)

```dockerfile
# Cada instrucción ENV añade una capa a la imagen
ENV VAR1=value1
ENV VAR2=value2
ENV VAR3=value3
# → 3 capas (aunque cada una es muy pequeña — solo metadata)

# Mejor: una sola instrucción ENV (1 capa)
ENV VAR1=value1 \
    VAR2=value2 \
    VAR3=value3
```

### ENV con valores complejos

```dockerfile
# Valores con espacios requieren comillas
ENV APP_NAME="Mi Aplicación"
ENV DB_CONNECTION="host=localhost port=5432"

# Expansión entre variables
ENV BASE_DIR=/app
ENV DATA_DIR=${BASE_DIR}/data
# DATA_DIR=/app/data

# Concatenación
ENV VERSION_SUFFIX=-stable
ENV FULL_VERSION=1.0${VERSION_SUFFIX}
# FULL_VERSION=1.0-stable
```

### Precedencia

Cuando la misma variable se define en múltiples lugares, la precedencia es:

```
docker run -e VAR=runtime    ← Mayor prioridad (siempre gana)
    │
    ▼
ENV VAR=build                ← Definida en el Dockerfile
    │
    ▼
ARG VAR=default              ← Menor prioridad (solo durante build)
```

```bash
# -e siempre sobrescribe ENV del Dockerfile
docker run --rm -e NODE_ENV=development myimage env | grep NODE_ENV
# NODE_ENV=development (aunque el Dockerfile diga production)
```

### --env-file para múltiples variables

```bash
# Archivo .env
cat > app.env << 'EOF'
PORT=3000
NODE_ENV=development
LOG_LEVEL=debug
EOF

# Pasar todas las variables del archivo
docker run --rm --env-file app.env myimage env
# PORT=3000
# NODE_ENV=development
# LOG_LEVEL=debug
```

## Combo ARG + ENV

Cuando necesitas que un valor de build **también** esté disponible en runtime,
usa el patrón ARG + ENV:

```dockerfile
# ARG para recibir el valor durante el build
ARG APP_VERSION=1.0

# ENV para persistirlo en la imagen (expansión explícita)
ENV APP_VERSION=${APP_VERSION}

RUN echo "Build version: $APP_VERSION"
CMD ["sh", "-c", "echo Runtime version: $APP_VERSION"]
```

```bash
# Build con versión específica
docker build --build-arg APP_VERSION=2.5.0 -t myapp .

# El valor está disponible en runtime
docker run --rm myapp
# Runtime version: 2.5.0
```

> **Importante**: la expansión debe ser explícita: `ENV APP_VERSION=${APP_VERSION}`.
> No existe auto-expansión — `ENV APP_VERSION` sin valor da error de sintaxis.

## Seguridad: no uses ARG para secrets

Aunque ARG no persiste como variable de entorno, los valores **quedan registrados
en el historial de la imagen**:

```dockerfile
ARG DB_PASSWORD=secret123
RUN echo "Connecting to DB..." && some_setup_script
```

```bash
docker history myimage --no-trunc
# RUN |1 DB_PASSWORD=secret123 /bin/sh -c echo "Connecting to DB..."
# ¡La contraseña es visible para cualquiera con acceso a la imagen!
```

### BuildKit secret mounts (la forma segura)

BuildKit permite montar secrets que solo existen durante la ejecución de un
`RUN` — no quedan en ninguna capa:

```dockerfile
# syntax=docker/dockerfile:1
FROM debian:bookworm-slim
RUN --mount=type=secret,id=db_pass \
    DB_PASS=$(cat /run/secrets/db_pass) && \
    setup_db "$DB_PASS"
# El secret NO queda en ninguna capa de la imagen
# /run/secrets/db_pass NO existe después del RUN
```

```bash
# Crear archivo con el secret
echo "mi_password" > db_password.txt

# Construir montando el secret
docker build --secret id=db_pass,src=db_password.txt -t myapp .

# Limpiar
rm db_password.txt
```

El secret solo es accesible dentro del `RUN` donde se monta. No aparece en
`docker history` ni en ninguna capa de la imagen.

Para secrets en runtime, usar `-e` o Docker secrets en Compose/Swarm.

## Ejemplo completo: imagen parametrizable

```dockerfile
# Permite cambiar la imagen base
ARG BASE_IMAGE=python
ARG BASE_TAG=3.12-slim

FROM ${BASE_IMAGE}:${BASE_TAG}

# Metadata de build
ARG BUILD_DATE
ARG APP_VERSION=1.0.0
LABEL build-date=${BUILD_DATE} version=${APP_VERSION}

# Persistir la versión para runtime
ENV APP_VERSION=${APP_VERSION}

# Configuración de runtime (sobrescribible con -e)
ENV PORT=8080
ENV LOG_LEVEL=info
ENV WORKERS=4

WORKDIR /app
COPY . .

EXPOSE ${PORT}
CMD ["sh", "-c", "echo v${APP_VERSION} listening on :${PORT} with ${WORKERS} workers"]
```

```bash
# Build por defecto
docker build -t myapp .

# Build personalizado
docker build \
  --build-arg BASE_TAG=3.11-slim \
  --build-arg APP_VERSION=2.0.0 \
  --build-arg BUILD_DATE=$(date -u +%Y-%m-%dT%H:%M:%SZ) \
  -t myapp:2.0.0 .

# Runtime: sobrescribir configuración
docker run --rm -e PORT=9090 -e WORKERS=8 myapp:2.0.0
# v2.0.0 listening on :9090 with 8 workers
```

## Resumen visual del ciclo de vida

```
┌─────────────────────────────────────────────────────────┐
│                     docker build                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ARG ──────────────► RUN, COPY, ADD (expansión en       │
│    │                 instrucciones del stage actual)     │
│    │                                                    │
│    └── antes de FROM ──► solo en FROM ${VAR}            │
│                                                         │
│  ENV ──────────────► RUN, WORKDIR, EXPOSE, CMD          │
│    │                 (persiste en capas de imagen)       │
│    │                                                    │
│    └── ${ARG_VAR} ──► ENV VAR=${ARG_VAR}                │
│                       (combo ARG+ENV)                   │
│                                                         │
└─────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────┐
│                     docker run                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ENV ──────────────► Proceso del contenedor             │
│    │                                                    │
│    └── -e VAR=val ──► Sobrescribe ENV del Dockerfile    │
│                                                         │
│  ARG ──► NO disponible (descartado tras build)          │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Tabla de casos de uso

| Situación | Solución | Ejemplo |
|---|---|---|
| Cambiar versión de app en build | ARG + ENV | `ARG VERSION` + `ENV VERSION=${VERSION}` |
| Configurar app en runtime | ENV | `ENV PORT=8080` + `-e PORT=3000` |
| Cambiar imagen base | ARG antes de FROM | `ARG BASE=python` + `FROM ${BASE}:3.12` |
| Proxy durante build | ARG predefinido | `--build-arg HTTP_PROXY=http://proxy:8080` |
| Secret durante build | BuildKit secret mount | `--secret id=pass,src=file` |
| Secret en runtime | `-e` o Docker secrets | `docker run -e PASS=xxx` o Compose secrets |
| Múltiples config vars en runtime | `--env-file` | `docker run --env-file app.env` |

---

## Práctica

> **Nota**: este tópico tiene un directorio `labs/` con Dockerfiles preparados
> para ejercicios más detallados.

### Ejercicio 1 — ARG solo existe durante el build

```bash
mkdir -p /tmp/docker_arg && cat > /tmp/docker_arg/Dockerfile << 'EOF'
FROM alpine:latest
ARG GREETING=Hello
RUN echo "Build: $GREETING" > /build_msg.txt
CMD ["sh", "-c", "echo 'Build dice:' && cat /build_msg.txt && echo \"Runtime GREETING=$GREETING\""]
EOF

docker build -t arg-test /tmp/docker_arg
docker run --rm arg-test
# Build dice:
# Build: Hello
# Runtime GREETING=        ← vacío: ARG no persiste

# Sobrescribir ARG
docker build --build-arg GREETING=Hola -t arg-test2 /tmp/docker_arg
docker run --rm arg-test2
# Build dice:
# Build: Hola
# Runtime GREETING=        ← sigue vacío en runtime

docker rmi arg-test arg-test2
rm -rf /tmp/docker_arg
```

**Predicción**: `$GREETING` se expande durante el build (en `RUN echo`), pero
en runtime (`CMD`) queda vacío porque ARG no persiste. Aunque el archivo
`/build_msg.txt` contiene el valor (se escribió durante build), la variable
`$GREETING` en el CMD del contenedor no existe.

### Ejercicio 2 — Combo ARG + ENV

```bash
mkdir -p /tmp/docker_argenv && cat > /tmp/docker_argenv/Dockerfile << 'EOF'
FROM alpine:latest
ARG APP_VERSION=1.0.0
ENV APP_VERSION=${APP_VERSION}
CMD ["sh", "-c", "echo Version: $APP_VERSION"]
EOF

docker build --build-arg APP_VERSION=3.2.1 -t argenv-test /tmp/docker_argenv
docker run --rm argenv-test
# Version: 3.2.1  ← persiste gracias a ENV

# Sobrescribir en runtime
docker run --rm -e APP_VERSION=override argenv-test
# Version: override

docker rmi argenv-test
rm -rf /tmp/docker_argenv
```

**Predicción**: `ARG APP_VERSION=1.0.0` recibe el valor `3.2.1` via
`--build-arg`. `ENV APP_VERSION=${APP_VERSION}` lo persiste en la imagen. En
runtime existe como variable de entorno. `-e APP_VERSION=override` tiene mayor
prioridad que el ENV del Dockerfile.

### Ejercicio 3 — ARG visible en docker history (problema de seguridad)

```bash
mkdir -p /tmp/docker_secret && cat > /tmp/docker_secret/Dockerfile << 'EOF'
FROM alpine:latest
ARG SECRET_TOKEN=abc123
RUN echo "Using token (hidden)" > /log.txt
CMD ["cat", "/log.txt"]
EOF

docker build --build-arg SECRET_TOKEN=mi_password_real -t secret-test /tmp/docker_secret

# El valor del ARG es visible en el historial
docker history secret-test --no-trunc --format "{{.CreatedBy}}" | head -5
# |1 SECRET_TOKEN=mi_password_real /bin/sh -c echo "Using token (hidden)" > /log.txt
# ¡Cualquiera con acceso a la imagen puede ver el secret!

docker rmi secret-test
rm -rf /tmp/docker_secret
```

**Predicción**: `docker history --no-trunc` muestra el valor completo de
`SECRET_TOKEN` porque Docker registra los ARGs en la metadata de cada capa.
Esto es peligroso — nunca usar ARG para passwords, tokens, o API keys.

### Ejercicio 4 — BuildKit secret mounts (forma segura)

```bash
mkdir -p /tmp/docker_secure

# Crear el archivo de secret
echo "super_secret_password" > /tmp/docker_secure/db_pass.txt

cat > /tmp/docker_secure/Dockerfile << 'EOF'
# syntax=docker/dockerfile:1
FROM alpine:latest

# El secret se monta temporalmente en /run/secrets/db_pass
# Solo existe durante este RUN — no queda en ninguna capa
RUN --mount=type=secret,id=db_pass \
    echo "El secret tiene $(wc -c < /run/secrets/db_pass) bytes" && \
    echo "Setup completado" > /log.txt

CMD ["cat", "/log.txt"]
EOF

docker build --secret id=db_pass,src=/tmp/docker_secure/db_pass.txt -t secure-test /tmp/docker_secure

# El secret NO aparece en docker history
docker history secure-test --no-trunc | grep -i secret
# No muestra el password

# El archivo del secret NO existe en la imagen
docker run --rm secure-test ls /run/secrets/ 2>&1 || echo "No existe /run/secrets/"
# No existe

docker run --rm secure-test
# Setup completado

rm /tmp/docker_secure/db_pass.txt
docker rmi secure-test
rm -rf /tmp/docker_secure
```

**Predicción**: el secret se monta en `/run/secrets/db_pass` solo durante el
`RUN`. No aparece en `docker history` ni queda en ninguna capa. Después del
build, `/run/secrets/` no existe en la imagen. Esto es la forma segura de
usar secrets durante el build.

### Ejercicio 5 — Scope de ARG entre stages

```bash
mkdir -p /tmp/docker_scope && cat > /tmp/docker_scope/Dockerfile << 'EOF'
FROM alpine:latest AS stage1
ARG MY_VAR=from_stage1
RUN echo "Stage 1: $MY_VAR" > /stage1.txt

FROM alpine:latest AS stage2
# MY_VAR NO está definido aquí
RUN echo "Stage 2: $MY_VAR" > /stage2.txt

# Redeclarar para heredar de --build-arg
ARG MY_VAR
RUN echo "Stage 2 (redeclarado): $MY_VAR" > /stage2b.txt

CMD ["sh", "-c", "cat /stage2.txt && cat /stage2b.txt"]
EOF

docker build --build-arg MY_VAR=global_value -t scope-test /tmp/docker_scope
docker run --rm scope-test
# Stage 2:                          ← vacío (ARG no se hereda entre stages)
# Stage 2 (redeclarado): global_value  ← funciona tras redeclarar

docker rmi scope-test
rm -rf /tmp/docker_scope
```

**Predicción**: ARG tiene scope por stage. En stage2, `$MY_VAR` está vacío
hasta que se redeclara con `ARG MY_VAR`. Tras redeclarar, hereda el valor
de `--build-arg`. El valor por defecto de stage1 (`from_stage1`) no se
propaga a stage2 — solo `--build-arg` cruza stages.

### Ejercicio 6 — ARG sin valor por defecto

```bash
mkdir -p /tmp/docker_nodefault && cat > /tmp/docker_nodefault/Dockerfile << 'EOF'
FROM alpine:latest
ARG OPTIONAL_VAR
RUN echo "Valor: [$OPTIONAL_VAR]" > /result.txt
CMD ["cat", "/result.txt"]
EOF

# Sin pasar --build-arg: NO falla, solo queda vacío
docker build -t nodefault-test /tmp/docker_nodefault 2>&1 | tail -3
docker run --rm nodefault-test
# Valor: []  ← vacío, pero el build no falló

# Pasando --build-arg
docker build --build-arg OPTIONAL_VAR=provided -t nodefault-test2 /tmp/docker_nodefault
docker run --rm nodefault-test2
# Valor: [provided]

docker rmi nodefault-test nodefault-test2
rm -rf /tmp/docker_nodefault
```

**Predicción**: un ARG sin valor por defecto **no** es obligatorio. Sin
`--build-arg`, la variable queda como string vacío. Docker puede mostrar un
warning pero el build completa sin error.

### Ejercicio 7 — Precedencia: -e vs ENV vs ARG

```bash
mkdir -p /tmp/docker_prec && cat > /tmp/docker_prec/Dockerfile << 'EOF'
FROM alpine:latest
ARG CONFIG=from_arg
ENV CONFIG=${CONFIG}
CMD ["sh", "-c", "echo CONFIG=$CONFIG"]
EOF

# 1. Solo ARG por defecto
docker build -t prec-test /tmp/docker_prec
docker run --rm prec-test
# CONFIG=from_arg

# 2. --build-arg sobrescribe ARG → ENV hereda
docker build --build-arg CONFIG=from_buildarg -t prec-test2 /tmp/docker_prec
docker run --rm prec-test2
# CONFIG=from_buildarg

# 3. -e sobrescribe todo en runtime
docker run --rm -e CONFIG=from_runtime prec-test2
# CONFIG=from_runtime

docker rmi prec-test prec-test2
rm -rf /tmp/docker_prec
```

**Predicción**: la cadena de precedencia es clara: `ARG` default → `--build-arg`
→ `ENV` (persiste en imagen) → `-e` (runtime, máxima prioridad). `-e` siempre
gana porque se aplica al momento de ejecutar el contenedor.

### Ejercicio 8 — ARG antes de FROM

```bash
mkdir -p /tmp/docker_argfrom && cat > /tmp/docker_argfrom/Dockerfile << 'EOF'
ARG BASE_TAG=latest
FROM alpine:${BASE_TAG}

# BASE_TAG NO está disponible aquí (scope especial: solo FROM)
RUN echo "Tag: $BASE_TAG" > /result.txt

# Redeclarar para usarlo dentro del stage
ARG BASE_TAG
RUN echo "Tag (redeclarado): $BASE_TAG" >> /result.txt

CMD ["cat", "/result.txt"]
EOF

docker build --build-arg BASE_TAG=3.19 -t argfrom-test /tmp/docker_argfrom
docker run --rm argfrom-test
# Tag:                    ← vacío (ARG pre-FROM tiene scope limitado)
# Tag (redeclarado): 3.19 ← funciona tras redeclarar

docker rmi argfrom-test
rm -rf /tmp/docker_argfrom
```

**Predicción**: ARG antes de FROM tiene un scope especial — solo es visible en
las instrucciones FROM. Dentro del stage, `$BASE_TAG` está vacío. Hay que
redeclararlo (`ARG BASE_TAG` sin valor) para que herede el valor de
`--build-arg`. Esto es un error común al escribir Dockerfiles parametrizados.
