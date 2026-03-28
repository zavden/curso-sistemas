# T01 — Instrucciones básicas

## ¿Qué es un Dockerfile?

Un Dockerfile es un archivo de texto que contiene **instrucciones secuenciales**
para construir una imagen Docker. Cada instrucción añade una capa al filesystem
de la imagen. El resultado es una imagen reproducible que cualquiera puede
reconstruir ejecutando `docker build`.

```
Dockerfile                    Imagen resultante
┌──────────────────────┐      ┌──────────────────────┐
│ FROM debian:bookworm │ ───▶ │ Capa 1: debian base  │
│ RUN apt-get install  │ ───▶ │ Capa 2: gcc, libc    │
│ COPY main.c /app/    │ ───▶ │ Capa 3: main.c       │
│ RUN gcc -o app main.c│ ───▶ │ Capa 4: binario app  │
│ CMD ["./app"]        │ ───▶ │ Metadata: cmd=./app  │
└──────────────────────┘      └──────────────────────┘
```

```bash
# Construir una imagen a partir del Dockerfile en el directorio actual
docker build -t myimage:v1 .

# Construir con un Dockerfile en otra ubicación
docker build -t myimage:v1 -f path/to/Dockerfile .

# Construir con un nombre de build stage específico
docker build --target builder -t myimage:builder .
```

## Anatomía básica de un Dockerfile

```
# Comentarios empiezan con #
FROM imagen:tag              # Imagen base (obligatorio, primera instrucción)
LABEL maintainer="..."      # Metadata (opcional)
ARG build_arg               # Variables de build (opcional)
ENV VAR=value               # Variables de entorno (opcional)

RUN apt-get update && \     # Comandos durante el build
    apt-get install -y gcc

COPY . /app/               # Copiar archivos del contexto
WORKDIR /app               # Directorio de trabajo
USER username              # Usuario para instrucciones siguientes
EXPOSE 8080                # Puertos expuestos (documentación)
VOLUME /data               # Puntos de montaje (documentación)
CMD ["./app"]              # Comando por defecto al ejecutar
```

## FROM — La imagen base

Todo Dockerfile empieza con `FROM`. Define la imagen sobre la que se construye.

```dockerfile
# Imagen base con tag específico
FROM debian:bookworm

# Imagen base con digest (inmutable — reproduce exactamente la misma imagen)
FROM debian@sha256:abc123...

# Naming para multi-stage builds
FROM debian:bookworm AS builder

# Imagen vacía — para binarios estáticos (Go, Rust, C estático)
FROM scratch
```

`FROM scratch` crea una imagen sin sistema operativo base. Solo funciona con
binarios compilados estáticamente que no dependen de librerías del sistema:

```dockerfile
# Ejemplo de imagen scratch para Go
FROM scratch
COPY myapp /myapp
ENTRYPOINT ["/myapp"]
# La imagen resultante contiene SOLO el binario — unos pocos MB
```

`FROM scratch` no tiene shell, no tiene `/bin/sh`, no tiene nada. Solo tu binary.

**Regla práctica**: usar la variante más mínima que funcione.

### Variantes de imágenes comunes

| Imagen | Tamaño aprox. | Contenido |
|-------|--------------|-----------|
| `debian:bookworm` | ~120 MB | Debian completo (apt, bash, coreutils) |
| `debian:bookworm-slim` | ~75 MB | Debian sin docs, man pages, locales extra |
| `alpine:3.19` | ~7 MB | musl libc, busybox, apk (muy mínimo) |
| `ubuntu:24.04` | ~80 MB | Ubuntu completo |
| `python:3.12-slim` | ~150 MB | Python + dependencias |
| `scratch` | 0 MB | Nada — solo tu binario |

## RUN — Ejecutar comandos durante el build

`RUN` ejecuta un comando y guarda el resultado como una nueva capa de la imagen.

### Forma shell

```dockerfile
# Se ejecuta como /bin/sh -c "comando"
RUN apt-get update && apt-get install -y gcc
RUN echo "Hello" > /greeting.txt
```

La forma shell permite expansión de variables, pipes, redirecciones y todos los
features del shell:

```dockerfile
RUN echo "Versión: $APP_VERSION" > /version.txt
RUN curl -s https://example.com/data | gzip > /data.gz
```

### Forma exec

```dockerfile
# Se ejecuta directamente, sin shell — array JSON
RUN ["apt-get", "install", "-y", "gcc"]
```

No hay expansión de variables (`$VAR` se toma literal). Útil cuando el shell
interfiere o no está disponible (imágenes `scratch`).

**Con `scratch`, no puedes usar forma shell porque no hay `/bin/sh`**:

```dockerfile
# INCORRECTO para scratch — forma shell requiere shell
# RUN echo "hello" > /file

# CORRECTO para scratch — forma exec
RUN ["dd", "if=/dev/zero", "of=/file", "bs=1", "count=10"]
```

### Buena práctica: combinar comandos relacionados

Cada `RUN` crea una capa. Los archivos eliminados en una capa posterior **siguen
ocupando espacio** en las capas anteriores (OverlayFS copy-on-write). Por eso,
la instalación y limpieza deben estar en el mismo `RUN`:

```dockerfile
# MAL: la caché de apt ocupa espacio en la capa 1 aunque se borre en la capa 2
RUN apt-get update && apt-get install -y gcc
RUN rm -rf /var/lib/apt/lists/*

# BIEN: instalar y limpiar en la misma capa
RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc && \
    rm -rf /var/lib/apt/lists/*
```

El `\` al final de línea permite partir comandos largos en varias líneas para
legibilidad. El `&&` encadena comandos de forma que si uno falla, el build falla.

### Más recomendaciones para RUN

```dockerfile
# Usar --no-install-recommends para instalar solo dependencias directas
RUN apt-get update && \
    apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*

# Encadenar con pipes (necesita set -o pipefail)
RUN set -o pipefail && \
    curl -sSL https://example.com/install.sh | bash

# Limpiar todo en un solo RUN
RUN apt-get update && apt-get install -y nginx && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
```

## COPY — Copiar archivos al filesystem de la imagen

`COPY` toma archivos del **contexto de build** (el directorio que pasas a
`docker build`) y los coloca en la imagen.

```dockerfile
# Copiar un archivo
COPY main.c /app/main.c

# Copiar un directorio completo (el contenido, no el directorio en sí)
COPY src/ /app/src/

# Copiar todo el contexto (usar con .dockerignore)
COPY . /app/

# Establecer propietario al copiar (evita problemas de permisos)
COPY --chown=appuser:appuser config.json /app/config.json

# Copiar archivos con wildcards
COPY *.jar /app/lib/

# Copiar desde otro stage (multi-stage builds)
COPY --from=builder /app/binary /usr/local/bin/
```

Las rutas de origen son **relativas al contexto de build**, nunca absolutas del
host. No puedes hacer `COPY /etc/passwd /app/` — Docker no permite acceder a
archivos fuera del contexto.

```bash
# El contexto es "." (directorio actual)
docker build -t myapp .
# Todo lo que COPY copia debe estar en el directorio donde ejecutas docker build
```

### Wildcards en COPY

```dockerfile
COPY package*.json /app/
COPY src/*.js /app/src/
COPY config.* /app/config/
```

## ADD — COPY con extras

`ADD` funciona como `COPY` pero con dos features adicionales:

```dockerfile
# Descomprime automáticamente archivos tar (tar, tar.gz, tar.bz2, tar.xz)
ADD app.tar.gz /app/
# El contenido descomprimido queda en /app/

# Puede descargar desde URLs HTTP/HTTPS
ADD https://example.com/file.tar.gz /tmp/
```

**Regla general**: usar `COPY` siempre, excepto cuando necesites descompresión
automática de tar o descarga de URL. Para descargas, `RUN curl` o `RUN wget`
dan más control sobre caché y manejo de errores:

```dockerfile
# Preferir esto (más control):
RUN curl -fsSL https://example.com/tool.tar.gz | tar xz -C /usr/local/bin/

# Sobre esto (menos control):
ADD https://example.com/tool.tar.gz /tmp/
RUN tar xz -C /usr/local/bin/ -f /tmp/tool.tar.gz && rm /tmp/tool.tar.gz
```

## WORKDIR — Directorio de trabajo

Establece el directorio de trabajo para todas las instrucciones siguientes
(`RUN`, `CMD`, `ENTRYPOINT`, `COPY`, `ADD`).

```dockerfile
WORKDIR /app

# Todas estas instrucciones operan en /app
COPY main.c .         # → /app/main.c
RUN gcc -o app main.c # Se ejecuta en /app
CMD ["./app"]         # Ejecuta /app/app
```

`WORKDIR` crea el directorio si no existe. Se pueden encadenar:

```dockerfile
WORKDIR /app
WORKDIR src
# Directorio actual: /app/src
```

**Evitar `RUN cd`**: no funciona como esperas porque cada `RUN` se ejecuta en un
shell nuevo. `cd` no persiste entre instrucciones:

```dockerfile
# MAL: cd no persiste entre RUN
RUN cd /app
RUN ls          # Ejecuta ls en /, no en /app

# BIEN: WORKDIR persiste
WORKDIR /app
RUN ls          # Ejecuta ls en /app
```

### WORKDIR con variables de entorno

```dockerfile
ENV APP_HOME=/app
WORKDIR $APP_HOME
# Equivale a WORKDIR /app
```

## CMD — Comando por defecto

Define el comando que se ejecuta cuando se inicia el contenedor. El usuario puede
sobrescribirlo con argumentos a `docker run`.

```dockerfile
# Forma exec (recomendada) — PID 1 es el proceso directamente
CMD ["python3", "app.py"]

# Forma shell — PID 1 es /bin/sh -c "..."
CMD python3 app.py
```

Solo el **último CMD** cuenta. Si hay varios, los anteriores se ignoran:

```dockerfile
CMD ["echo", "primero"]
CMD ["echo", "segundo"]
# Solo se ejecuta "echo segundo"
```

```bash
# CMD por defecto
docker run myimage
# Ejecuta: python3 app.py

# Sobrescribir CMD
docker run myimage python3 other_script.py
# Ejecuta: python3 other_script.py (CMD ignorado)
```

### Forma exec vs forma shell

| Aspecto | Exec `["cmd", "arg"]` | Shell `cmd arg` |
|---------|----------------------|-----------------|
| PID 1 | El proceso directo | `/bin/sh -c cmd arg` |
| Señales | Se propagan directamente | El shell las recibe primero |
| Expansión de variables | No | Sí (`$VAR`) |
| Piping/redirección | No | Sí |
| Compatible con scratch | Sí | No |

```dockerfile
# Forma exec: las señales llegan directamente al proceso
CMD ["/bin/myapp", "--config", "/etc/app.conf"]

# Forma shell: si myapp recibe SIGTERM, /bin/sh lo recibe primero
# y puede no propagarlo correctamente
CMD /bin/myapp --config /etc/app.conf
```

## ENTRYPOINT — Punto de entrada fijo

A diferencia de CMD, ENTRYPOINT **no se reemplaza** con argumentos de `docker run`.
Los argumentos se **añaden** al ENTRYPOINT:

```dockerfile
ENTRYPOINT ["python3"]
```

```bash
docker run myimage app.py
# Ejecuta: python3 app.py

docker run myimage --version
# Ejecuta: python3 --version
```

ENTRYPOINT es para comandos que siempre deben ejecutarse de la misma manera, mientras
que CMD puede actuar como argumentos por defecto que el usuario puede sobrescribir.

La combinación ENTRYPOINT + CMD se cubre en el siguiente tópico (T02).

## Instrucciones adicionales

### EXPOSE

Documenta qué puertos usa el contenedor. No abre ni publica puertos (eso lo hace `-p`):

```dockerfile
EXPOSE 8080
EXPOSE 443/tcp
EXPOSE 53/udp
```

Útil para documentación y para `-P` (publish all EXPOSE).

### LABEL

Metadata key-value para la imagen:

```dockerfile
LABEL maintainer="team@example.com"
LABEL version="1.0"
LABEL description="API server"

# Múltiples labels en una instrucción (una sola capa)
LABEL maintainer="team@example.com" version="1.0"
```

Ver labels: `docker inspect --format '{{json .Config.Labels}}' imagen`

### VOLUME

Declara un punto de montaje que se convertirá en volumen anónimo automáticamente:

```dockerfile
VOLUME /app/data
VOLUME ["/var/log", "/app/uploads"]
```

Cuando un contenedor corre sin `-v`, Docker crea un volumen anónimo para ese path.
Esto protege los datos de ser eliminados cuando el contenedor se destruye.

### SHELL

Cambia el shell por defecto para instrucciones en forma shell:

```dockerfile
# Por defecto: /bin/sh -c
RUN echo "hello"

# Cambiar a bash
SHELL ["/bin/bash", "-c"]
RUN echo ${BASH_VERSION}  # Ahora usa bash
```

### STOPSIGNAL

Cambia la señal que `docker stop` envía a PID 1:

```dockerfile
STOPSIGNAL SIGQUIT  # Default: SIGTERM
```

Nginx usa SIGQUIT para graceful shutdown (terminar requests en curso antes de salir).

### USER

Define el usuario para las instrucciones siguientes:

```dockerfile
RUN apt-get update && apt-get install -y curl
USER appuser
WORKDIR /home/appuser
CMD ["curl", "--version"]
```

Después de `USER`, todas las instrucciones se ejecutan como ese usuario.

### ONBUILD

Instrucciones que se ejecutan solo cuando la imagen se usa como base de otra:

```dockerfile
FROM node:20
ONBUILD COPY package*.json /app/
ONBUILD RUN npm ci
ONBUILD WORKDIR /app
CMD ["node", "server.js"]
```

Cuando otra imagen haga `FROM my-node-base`, las instrucciones ONBUILD se ejecutan
automáticamente.

## Build cache y capas

Docker guarda en caché cada capa. Si una instrucción no cambió, usa la capa cached.

```bash
# Build que usa caché
docker build -t myapp .
# Step 1/5 : FROM debian:bookworm-slim
# ---> Using cache

# Si cambias algo, solo se rehacen las capas desde el cambio
docker build -t myapp .
# Step 1/5 : FROM debian:bookworm-slim
# ---> abc123... (cached)
# Step 2/5 : RUN apt-get update
# ---> Using cache  ← sigue usando caché si no cambió
```

Para forzar rebuild sin caché: `docker build --no-cache -t myapp .`

## Práctica

### Ejercicio 1 — Imagen que compila un programa C

```bash
mkdir -p /tmp/docker_c_test && cat > /tmp/docker_c_test/main.c << 'EOF'
#include <stdio.h>
int main(void) {
    printf("Hello from a Docker-built C program!\n");
    return 0;
}
EOF

cat > /tmp/docker_c_test/Dockerfile << 'EOF'
FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc libc6-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY main.c .
RUN gcc -o hello main.c

CMD ["./hello"]
EOF

docker build -t c-hello /tmp/docker_c_test
docker run --rm c-hello
# Hello from a Docker-built C program!

docker rmi c-hello
rm -rf /tmp/docker_c_test
```

### Ejercicio 2 — Forma shell vs exec de RUN

```bash
mkdir -p /tmp/docker_shell_test && cat > /tmp/docker_shell_test/Dockerfile << 'EOF'
FROM alpine:latest

# Forma shell: expansión de variables funciona
ENV GREETING="Hello"
RUN echo "$GREETING from shell form"

# Forma exec: NO hay expansión (se toma literal)
RUN ["echo", "$GREETING from exec form"]

CMD ["echo", "done"]
EOF

docker build -t shell-test /tmp/docker_shell_test 2>&1 | grep -E "Hello|exec"
# "Hello from shell form"     ← variable expandida
# "$GREETING from exec form"  ← literal

docker rmi shell-test
rm -rf /tmp/docker_shell_test
```

### Ejercicio 3 — Efecto de WORKDIR

```bash
mkdir -p /tmp/docker_wd_test && cat > /tmp/docker_wd_test/Dockerfile << 'EOF'
FROM alpine:latest

WORKDIR /app
RUN pwd > /app/pwd1.txt

WORKDIR /app/src
RUN pwd > /app/pwd2.txt

CMD ["cat", "/app/pwd1.txt", "/app/pwd2.txt"]
EOF

docker build -t wd-test /tmp/docker_wd_test
docker run --rm wd-test
# /app
# /app/src

docker rmi wd-test
rm -rf /tmp/docker_wd_test
```

### Ejercicio 4 — CMD vs argumentos de docker run

```bash
mkdir -p /tmp/docker_cmd_test && cat > /tmp/docker_cmd_test/Dockerfile << 'EOF'
FROM alpine:latest
CMD ["echo", "CMD default"]
EOF

docker build -t cmd-test /tmp/docker_cmd_test

# Ejecutar CMD por defecto
docker run --rm cmd-test
# CMD default

# Sobrescribir CMD
docker run --rm cmd-test echo "Sobrescrito!"
# Sobrescrito!

docker rmi cmd-test
rm -rf /tmp/docker_cmd_test
```

### Ejercicio 5 — Scratch con binario estático

```bash
# Compilar un programa C estático
cat > /tmp/hello.c << 'EOF'
#include <stdio.h>
int main(void) {
    printf("Static hello from scratch!\n");
    return 0;
}
EOF
gcc --static -o /tmp/hello /tmp/hello.c

# Crear Dockerfile para scratch
mkdir -p /tmp/scratch_test && cat > /tmp/scratch_test/Dockerfile << 'EOF'
FROM scratch
COPY hello /hello
ENTRYPOINT ["/hello"]
EOF
cp /tmp/hello /tmp/scratch_test/

docker build -t static-hello /tmp/scratch_test
docker run --rm static-hello
# Static hello from scratch!

# Ver el tamaño
docker images | grep static-hello
# Muy pequeño (~10KB vs ~120MB de debian)

docker rmi static-hello
rm -rf /tmp/scratch_test /tmp/hello /tmp/hello.c
```

### Ejercicio 6 — COPY con chown

```bash
mkdir -p /tmp/docker_chown_test && cat > /tmp/docker_chown_test/file.txt << 'EOF'
Contenido del archivo
EOF

cat > /tmp/docker_chown_test/Dockerfile << 'EOF'
FROM alpine:latest
RUN adduser -D -u 1000 appuser
COPY --chown=appuser:appuser file.txt /home/appuser/file.txt
USER appuser
CMD ["cat", "/home/appuser/file.txt"]
EOF

docker build -t chown-test /tmp/docker_chown_test
docker run --rm chown-test
# Verifica que el archivo pertenece a appuser
docker run --rm chown-test ls -la /home/appuser/

docker rmi chown-test
rm -rf /tmp/docker_chown_test
```

### Ejercicio 7 — build cache

```bash
mkdir -p /tmp/cache_test && cat > /tmp/cache_test/Dockerfile << 'EOF'
FROM debian:bookworm-slim
RUN echo "Step 1" && sleep 1
RUN echo "Step 2" && sleep 1
RUN echo "Step 3" && sleep 1
CMD ["echo", "Done"]
EOF

# Primer build — todo se ejecuta
docker build -t cache-test /tmp/cache_test

# Segundo build — usa caché para todos los pasos
docker build -t cache-test /tmp/cache_test
# Verás "Using cache" para cada paso

# Forzar rebuild sin caché
docker build --no-cache -t cache-test /tmp/cache_test
# Todos los pasos se ejecutan de nuevo

docker rmi cache-test
rm -rf /tmp/cache_test
```

### Ejercicio 8 — LABEL y EXPOSE

```bash
mkdir -p /tmp/label_test && cat > /tmp/label_test/Dockerfile << 'EOF'
FROM nginx:alpine
LABEL maintainer="admin@example.com"
LABEL version="1.0.0"
LABEL description="Mi servidor web personalizado"
EXPOSE 8080 80
CMD ["nginx", "-g", "daemon off;"]
EOF

docker build -t label-test /tmp/label_test

# Ver labels
docker inspect label-test --format '{{json .Config.Labels}}' | jq '.'

# Ver EXPOSE
docker inspect label-test --format '{{json .Config.ExposedPorts}}' | jq '.'

# Ver todas las config
docker inspect label-test --format '{{json .Config}}' | jq '.'

docker rmi label-test
rm -rf /tmp/label_test
```
