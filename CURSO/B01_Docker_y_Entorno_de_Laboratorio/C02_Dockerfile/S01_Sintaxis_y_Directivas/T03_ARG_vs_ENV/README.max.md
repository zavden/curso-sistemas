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
| Visible en `docker history` | Sí | No directamente |

## ARG — Variables de build

`ARG` declara variables que solo existen durante la construcción de la imagen:

```dockerfile
# Declarar con valor por defecto
ARG DEBIAN_FRONTEND=noninteractive
ARG APP_VERSION=1.0.0

# Usar en instrucciones
RUN echo "Construyendo versión $APP_VERSION"

# Sin valor por defecto (obligatorio pasarlo con --build-arg)
ARG CUSTOM_REPO
RUN echo "Repo: $CUSTOM_REPO"
```

```bash
# Usar valores por defecto
docker build -t myapp .

# Sobrescribir un ARG
docker build --build-arg APP_VERSION=2.0.0 -t myapp .

# Pasar un ARG obligatorio
docker build --build-arg CUSTOM_REPO=https://repo.example.com -t myapp .

# Múltiples ARGs
docker build \
  --build-arg APP_VERSION=2.0.0 \
  --build-arg BUILD_MODE=release \
  -t myapp .
```

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
ARG APP_VERSION         # Hay que redeclararlo
RUN echo $APP_VERSION   # Ahora sí funciona (usa el valor de --build-arg)
```

### ARG antes de FROM

Un ARG declarado **antes del primer FROM** solo es visible en las instrucciones
`FROM`:

```dockerfile
ARG DEBIAN_VERSION=bookworm
FROM debian:${DEBIAN_VERSION}

# DEBIAN_VERSION ya NO está disponible aquí
RUN echo $DEBIAN_VERSION  # Vacío

# Si lo necesitas, hay que redeclararlo
ARG DEBIAN_VERSION
RUN echo $DEBIAN_VERSION  # bookworm
```

```bash
# Cambiar la imagen base desde la línea de comandos
docker build --build-arg DEBIAN_VERSION=bullseye -t myapp .
```

### ARGs predefinidos

Docker reconoce ciertos ARGs sin necesidad de declararlos con `ARG`:

```bash
# Estos funcionan sin declaración en el Dockerfile
docker build --build-arg HTTP_PROXY=http://proxy:8080 .
docker build --build-arg HTTPS_PROXY=http://proxy:8080 .
docker build --build-arg FTP_PROXY=http://proxy:8080 .
docker build --build-arg NO_PROXY=localhost,127.0.0.1 .
docker build --build-arg http_proxy=http://proxy:8080 .
docker build --build-arg https_proxy=http://proxy:8080 .
```

Estos ARGs predefinidos **no aparecen en `docker history`** (a diferencia de los
ARGs normales).

### Lista completa de ARGs predefinidos

| ARG | Descripción |
|---|---|
| `HTTP_PROXY` | Proxy HTTP para el build |
| `HTTPS_PROXY` | Proxy HTTPS para el build |
| `FTP_PROXY` | Proxy FTP para el build |
| `NO_PROXY` | Hosts que no usan proxy |
| `http_proxy` | Versión minúscula |
| `https_proxy` | Versión minúscula |
| `ftp_proxy` | Versión minúscula |
| `no_proxy` | Versión minúscula |
| `PARALLEL_BUILD` | Construcciones paralelas |
| `BUILDKIT_INLINE_CACHE` | Cache inline para imágenes remotas |

### ARG en FROM

```dockerfile
# Útil para cambiar la imagen base dinámicamente
ARG BASE_IMAGE=debian
ARG BASE_TAG=bookworm-slim
FROM ${BASE_IMAGE}:${BASE_TAG}
```

```bash
# Construir con diferentes imágenes base
docker build --build-arg BASE_IMAGE=ubuntu --build-arg BASE_TAG=jammy -t myapp .
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

### Cada ENV crea una capa

```dockerfile
# Cada instrucción ENV añade una capa a la imagen
ENV VAR1=value1
ENV VAR2=value2
ENV VAR3=value3
# → 3 capas

# Mejor: una sola instrucción ENV (1 capa)
ENV VAR1=value1 \
    VAR2=value2 \
    VAR3=value3
```

En la práctica, las capas de ENV son muy pequeñas (solo metadata), pero es buena
costumbre agruparlas.

### Precedencia

Cuando la misma variable se define en múltiples lugares, la precedencia es:

```
docker run -e VAR=runtime    ← Mayor prioridad (siempre gana)
    │
    ▼
ENV VAR=build                ← Definida en el Dockerfile
    │
    ▼
ARG VAR=default              ← Menor prioridad
```

```bash
# -e siempre sobrescribe ENV del Dockerfile
docker run --rm -e NODE_ENV=development myimage env | grep NODE_ENV
# NODE_ENV=development (aunque el Dockerfile diga production)
```

### ENV con valores complejos

```dockerfile
# Valores con espacios requieren comillas
ENV APP_NAME="Mi Aplicación"
ENV DB_CONNECTION="host=localhost port=5432"

# EXPAND también funciona
ENV BASE_DIR=/app
ENV DATA_DIR=${BASE_DIR}/data

# Valores vacíos
ENV EMPTY_VAR=
ENV EMPTY_STRING=""
```

### ENV en forma shell (expansión de variables)

```dockerfile
# ENV procesa variables del shell durante el build
ENV HOME=/root
ENV CONFIG_DIR=${HOME}/config

# También puedes usar operaciones de strings
ENV VERSION_SUFFIX=-stable
ENV FULL_VERSION=1.0${VERSION_SUFFIX}
```

## Combo ARG + ENV

Cuando necesitas que un valor de build **también** esté disponible en runtime,
usa el patrón ARG + ENV:

```dockerfile
# ARG para recibir el valor durante el build
ARG APP_VERSION=1.0

# ENV para persistirlo en la imagen
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

### Versión simplificada

Docker automáticamente_expande `${ARG_VAR}` en ENV si existe un ARG con ese nombre:

```dockerfile
ARG APP_VERSION=1.0
ENV APP_VERSION    # Equivalente a ENV APP_VERSION=${APP_VERSION}
```

```bash
# Ambos son equivalentes:
ARG APP_VERSION=1.0
ENV APP_VERSION=${APP_VERSION}

# vs simplemente:
ARG APP_VERSION=1.0
ENV APP_VERSION
```

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
# ¡La contraseña es visible!
```

**Alternativas seguras para secrets en build time**:

```bash
# BuildKit secret mounts (recomendado)
docker build --secret id=db_pass,src=./db_password.txt -t myapp .
```

```dockerfile
# syntax=docker/dockerfile:1
FROM debian:bookworm
RUN --mount=type=secret,id=db_pass \
    DB_PASS=$(cat /run/secrets/db_pass) && \
    setup_db "$DB_PASS"
# El secret no queda en ninguna capa de la imagen
```

### BuildKit secrets en detalle

```bash
# Crear un archivo de secret temporal
echo "mi_password_secreto" > /tmp/db_password.txt

# Construir con el secret montado
DOCKER_BUILDKIT=1 docker build --secret id=db_pass,src=/tmp/db_password.txt -t myapp .

# En el Dockerfile:
# RUN --mount=type=secret,id=db_pass echo $(cat /run/secrets/db_pass)
# El contenido del secret NO queda en ninguna capa

rm /tmp/db_password.txt
```

Para secrets en runtime, usar `-e` con Docker o Docker secrets en Compose/Swarm.

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

## Diferencias en la sintaxis

### ARG: diversas formas

```dockerfile
ARG VAR                    # Sin valor por defecto (requiere --build-arg)
ARG VAR=default            # Con valor por defecto
ARG VAR1=val1 VAR2=val2    # Múltiples en una línea
ARG VAR \
    VAR2=val2              # Multilínea (continuación con \)
```

### ENV: diversas formas

```dockerfile
ENV VAR=value              # Una variable
ENV VAR=value VAR2=value2  # Múltiples en una línea
ENV VAR=value \
    VAR2=value2            # Multilínea
```

## Ejercicios

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

### Ejercicio 3 — ARG visible en docker history

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

### Ejercicio 4 — BuildKit secrets (forma segura)

```bash
mkdir -p /tmp/docker_secure

# Crear el archivo de secret
echo "super_secret_password" > /tmp/docker_secure/db_pass.txt

cat > /tmp/docker_secure/Dockerfile << 'EOF'
# syntax=docker/dockerfile:1
FROM alpine:latest
RUN --mount=type=secret,id=db_pass \
    echo "Secret: $(cat /run/secrets/db_pass)" > /secure_log.txt
CMD ["cat", "/secure_log.txt"]
EOF

DOCKER_BUILDKIT=1 docker build --secret id=db_pass,src=/tmp/docker_secure/db_pass.txt -t secure-test /tmp/docker_secure

# El secret NO aparece en docker history
docker history secure-test --no-trunc | grep -i secret
# No muestra nada relacionado con el password

docker run --rm secure-test
# Secret: super_secret_password

# Verificar que el archivo de secret no queda en capas
docker history secure-test
# Solo muestra las capas normales, sin el password

rm /tmp/docker_secure/db_pass.txt
docker rmi secure-test
rm -rf /tmp/docker_secure
```

### Ejercicio 5 — ENV multilínea vs múltiples líneas

```bash
mkdir -p /tmp/docker_env_layer && cat > /tmp/docker_env_layer/Dockerfile.single << 'EOF'
FROM alpine:latest
ENV VAR1=value1 VAR2=value2 VAR3=value3
CMD ["env"]
EOF

cat > /tmp/docker_env_layer/Dockerfile.multi << 'EOF'
FROM alpine:latest
ENV VAR1=value1
ENV VAR2=value2
ENV VAR3=value3
CMD ["env"]
EOF

docker build -t env-single -f /tmp/docker_env_layer/Dockerfile.single /tmp/docker_env_layer
docker build -t env-multi -f /tmp/docker_env_layer/Dockerfile.multi /tmp/docker_env_layer

echo "=== Capas con ENV único ==="
docker history env-single

echo "=== Capas con múltiples ENV ==="
docker history env-multi

docker rmi env-single env-multi
rm -rf /tmp/docker_env_layer
```

### Ejercicio 6 — ARG con valor obligatorio

```bash
mkdir -p /tmp/docker_required_arg

cat > /tmp/docker_required_arg/Dockerfile << 'EOF'
FROM alpine:latest
ARG REQUIRED_VAR
RUN echo "Required: $REQUIRED_VAR"
CMD ["echo", "Done"]
EOF

# Sin pasar el ARG — esto FALLARÁ
docker build -t required-test /tmp/docker_required_arg 2>&1 || echo "Build failed as expected"

# Pasando el ARG — esto FUNCIONARÁ
docker build --build-arg REQUIRED_VAR=success -t required-test /tmp/docker_required_arg
docker run --rm required-test

docker rmi required-test
rm -rf /tmp/docker_required_arg
```

---

## Resumen visual del ciclo de vida

```
┌─────────────────────────────────────────────────────────────────┐
│                        docker build                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ARG ──────────────────────────► RUN instructions                │
│    │                                                           │
│    └── expansion ──────────────► FROM ${VAR}                    │
│                                                                  │
│  ENV ──────────────────────────► RUN instructions               │
│    │                              WORKDIR $VAR                  │
│    │                              EXPOSE $VAR                   │
│    │                                                           │
│    └── ${ARG_VAR} ─────────────► ENV VAR=${ARG_VAR}            │
│                                     │                           │
│                                     ▼                           │
│                              Capas de imagen                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        docker run                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ENV ──────────────────────────► Proceso del contenedor          │
│    │                                                           │
│    └── -e VAR=value ───────────► Sobrescribe ENV                │
│                                                                  │
│  ARG ──► NO disponible (se descarta después del build)           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Tabla de casos de uso

| Situación | Solución | Ejemplo |
|---|---|---|
| Cambiar versión de app en build | ARG + ENV | `ARG VERSION` + `ENV VERSION=${VERSION}` |
| Configurar app en runtime | ENV | `ENV PORT=8080` + `-e PORT=3000` |
| Cambiar imagen base | ARG en FROM | `ARG BASE=python` + `FROM ${BASE}:3.12` |
| Proxy durante build | ARG predefinido | `--build-arg HTTP_PROXY=http://proxy:8080` |
| Secret de build (no persiste) | BuildKit secret | `--secret id=pass,src=file` |
| Secret de runtime | `-e` o Docker secrets | `docker run -e PASS=xxx` o `--secret` en Compose |
