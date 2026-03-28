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
# Ejemplo de imagen scratch para un binario estático
FROM scratch
COPY myapp /myapp
ENTRYPOINT ["/myapp"]
# La imagen resultante contiene SOLO el binario — unos pocos MB
```

`FROM scratch` no tiene shell, no tiene `/bin/sh`, no tiene nada. Solo tu
binario. Esto implica:

- No puedes usar `RUN` (no hay ningún binario para ejecutar)
- No puedes usar CMD/ENTRYPOINT en forma shell (no hay shell)
- Solo puedes usar `COPY`/`ADD` para poner archivos, y metadata (CMD, ENTRYPOINT,
  ENV, EXPOSE, LABEL, etc.) en forma exec

**Regla práctica**: usar la variante más mínima que funcione.

### Variantes de imágenes comunes

| Imagen | Tamaño aprox. | Contenido |
|-------|--------------|-----------|
| `debian:bookworm` | ~120 MB | Debian completo (apt, bash, coreutils) |
| `debian:bookworm-slim` | ~75 MB | Debian sin docs, man pages, locales extra |
| `alpine:3.19` | ~7 MB | musl libc, busybox, apk (muy mínimo) |
| `ubuntu:24.04` | ~80 MB | Ubuntu completo |
| `python:3.12-slim` | ~150 MB | Python + dependencias mínimas |
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
interfiere o cuando necesitas control exacto sobre el proceso.

> **Nota**: la forma exec requiere que el binario exista en la imagen. En
> `FROM scratch` no hay binarios, por lo que `RUN` no funciona en ninguna
> forma — ni shell ni exec.

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

# Encadenar con pipes (necesita set -o pipefail para que falle si el pipe falla)
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
COPY package*.json /app/
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
automática de tar. Para descargas, `RUN curl` o `RUN wget` dan más control
sobre caché y manejo de errores:

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

`WORKDIR` crea el directorio si no existe. Se pueden encadenar y las **rutas
relativas son acumulativas**:

```dockerfile
WORKDIR /app
WORKDIR src
# Directorio actual: /app/src (src es relativo a /app)
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
| Señales | Se propagan directamente al proceso | El shell las recibe primero |
| Expansión de variables | No | Sí (`$VAR`) |
| Piping/redirección | No | Sí |
| Compatible con scratch | Sí | No (no hay shell) |

La distinción de PID 1 es importante: cuando Docker envía SIGTERM (con
`docker stop`), el proceso PID 1 recibe la señal. Con forma shell, PID 1 es
`/bin/sh` que puede no propagar la señal al proceso real, causando un shutdown
forzado después del timeout.

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

Documenta qué puertos usa el contenedor. No abre ni publica puertos (eso lo hace
`-p`):

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

### USER

Define el usuario para las instrucciones siguientes y para el contenedor en
ejecución:

```dockerfile
# Instalar como root (por defecto)
RUN apt-get update && apt-get install -y curl && \
    rm -rf /var/lib/apt/lists/*

# Crear usuario y cambiar
RUN useradd -r -s /bin/false appuser
USER appuser
WORKDIR /home/appuser
CMD ["curl", "--version"]
```

Después de `USER`, todas las instrucciones (`RUN`, `CMD`, `ENTRYPOINT`) se
ejecutan como ese usuario.

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

### ONBUILD

Instrucciones que se ejecutan solo cuando la imagen se usa como base de otra:

```dockerfile
# Imagen base para proyectos Node
FROM node:20
ONBUILD COPY package*.json /app/
ONBUILD RUN npm ci
ONBUILD COPY . /app/
WORKDIR /app
CMD ["node", "server.js"]
```

Cuando otra imagen haga `FROM my-node-base`, las instrucciones ONBUILD se ejecutan
automáticamente. Poco usado en la práctica — la mayoría de equipos prefiere
Dockerfiles explícitos.

## Build cache y capas

Docker guarda en caché cada capa. Si una instrucción no cambió, usa la capa
cached en lugar de re-ejecutarla:

```bash
# Build que usa caché
docker build -t myapp .
# Step 1/5 : FROM debian:bookworm-slim
# ---> Using cache

# Si cambias algo, solo se rehacen las capas desde el cambio en adelante
```

La caché se invalida cuando:

- El contenido de un archivo copiado con `COPY`/`ADD` cambió
- La instrucción `RUN` cambió (texto diferente)
- Cualquier capa anterior se invalidó (efecto cascada)

Para forzar rebuild sin caché: `docker build --no-cache -t myapp .`

> **Nota sobre BuildKit**: Docker usa BuildKit como builder por defecto desde
> Docker 23.0. BuildKit no muestra el output de `RUN` por defecto. Para verlo,
> usa `docker build --progress=plain`.

---

## Práctica

> **Nota**: este tópico tiene un directorio `labs/` con Dockerfiles preparados
> para ejercicios más detallados. Los ejercicios siguientes son autocontenidos
> y complementan los labs.

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

**Predicción**: el build descarga debian:bookworm-slim (~75 MB), instala gcc y
libc6-dev, compila main.c. La imagen final incluye gcc (no es multi-stage), por
lo que pesará ~250 MB. El contenedor imprime el mensaje y sale.

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

# --progress=plain necesario para ver el output de RUN con BuildKit
docker build --progress=plain -t shell-test /tmp/docker_shell_test 2>&1 | \
  grep -E "Hello|GREETING|exec"
# "Hello from shell form"     ← variable expandida
# "$GREETING from exec form"  ← literal, sin expandir

docker rmi shell-test
rm -rf /tmp/docker_shell_test
```

**Predicción**: la forma shell pasa por `/bin/sh -c`, que expande `$GREETING`
a "Hello". La forma exec ejecuta `echo` directamente sin shell, así que
`$GREETING` queda como texto literal. Sin `--progress=plain`, BuildKit no
muestra el output de los `RUN` steps.

### Ejercicio 3 — Efecto acumulativo de WORKDIR

```bash
mkdir -p /tmp/docker_wd_test && cat > /tmp/docker_wd_test/Dockerfile << 'EOF'
FROM alpine:latest

WORKDIR /app
RUN pwd > /app/pwd1.txt

WORKDIR src
RUN pwd > /app/pwd2.txt

CMD ["sh", "-c", "cat /app/pwd1.txt /app/pwd2.txt"]
EOF

docker build -t wd-test /tmp/docker_wd_test
docker run --rm wd-test
# /app
# /app/src

docker rmi wd-test
rm -rf /tmp/docker_wd_test
```

**Predicción**: `WORKDIR /app` establece `/app` como directorio de trabajo.
`WORKDIR src` (relativo) se resuelve como `/app/src` — no como `/src`. Las
rutas relativas en WORKDIR son acumulativas desde el WORKDIR actual.

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

# Cualquier argumento reemplaza el CMD completo
docker run --rm cmd-test ls /
# Lista el contenido de / (CMD original ignorado)

docker rmi cmd-test
rm -rf /tmp/docker_cmd_test
```

**Predicción**: sin argumentos, ejecuta el CMD del Dockerfile (`echo "CMD
default"`). Con argumentos, el CMD se reemplaza completamente. El argumento
a `docker run` no se añade al CMD — lo sustituye por completo.

### Ejercicio 5 — Capas y eficiencia con RUN combinado

```bash
mkdir -p /tmp/cache_test

# Patrón incorrecto: instalación y limpieza en RUN separados
cat > /tmp/cache_test/Dockerfile.separate << 'EOF'
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends curl
RUN rm -rf /var/lib/apt/lists/*
CMD ["curl", "--version"]
EOF

# Patrón correcto: todo en un solo RUN
cat > /tmp/cache_test/Dockerfile.combined << 'EOF'
FROM debian:bookworm-slim
RUN apt-get update && \
    apt-get install -y --no-install-recommends curl && \
    rm -rf /var/lib/apt/lists/*
CMD ["curl", "--version"]
EOF

docker build -f /tmp/cache_test/Dockerfile.separate -t run-separate /tmp/cache_test
docker build -f /tmp/cache_test/Dockerfile.combined -t run-combined /tmp/cache_test

# Comparar tamaños
docker images --format "table {{.Repository}}\t{{.Size}}" | grep "run-"
# run-separate  ~XXX MB (mayor)
# run-combined  ~YYY MB (menor)

# Ambas funcionan igual
docker run --rm run-separate
docker run --rm run-combined

docker rmi run-separate run-combined
rm -rf /tmp/cache_test
```

**Predicción**: `run-separate` es más grande porque la capa del `apt-get install`
contiene el cache de apt (~20 MB). Aunque la segunda capa lo elimina, la primera
capa es inmutable — los datos siguen ahí. En `run-combined`, la limpieza ocurre
en la misma capa, así que el cache nunca se guarda.

### Ejercicio 6 — COPY con --chown y USER

```bash
mkdir -p /tmp/docker_chown_test && echo "Contenido del archivo" > /tmp/docker_chown_test/file.txt

cat > /tmp/docker_chown_test/Dockerfile << 'EOF'
FROM alpine:latest
RUN adduser -D -u 1000 appuser
COPY --chown=appuser:appuser file.txt /home/appuser/file.txt
USER appuser
WORKDIR /home/appuser
CMD ["ls", "-la", "file.txt"]
EOF

docker build -t chown-test /tmp/docker_chown_test
docker run --rm chown-test
# -rw-r--r--  1 appuser  appuser  ... file.txt

# Verificar que CMD se ejecuta como appuser
docker run --rm chown-test whoami
# appuser

docker rmi chown-test
rm -rf /tmp/docker_chown_test
```

**Predicción**: `--chown=appuser:appuser` hace que el archivo pertenezca a
appuser (no a root). `USER appuser` cambia el usuario para CMD, por lo que
`whoami` muestra "appuser". Sin `--chown`, el archivo sería `root:root` y
appuser podría leerlo (644) pero no escribirlo.

### Ejercicio 7 — LABEL, EXPOSE y metadata

```bash
mkdir -p /tmp/label_test && cat > /tmp/label_test/Dockerfile << 'EOF'
FROM alpine:latest
LABEL maintainer="admin@example.com"
LABEL version="1.0.0"
LABEL description="Imagen de ejemplo para metadata"
EXPOSE 8080 443/tcp
RUN echo "Hello from metadata lab" > /greeting.txt
CMD ["cat", "/greeting.txt"]
EOF

docker build -t label-test /tmp/label_test

# Ejecutar con CMD por defecto
docker run --rm label-test
# Hello from metadata lab

# Ver labels
docker inspect label-test --format '{{json .Config.Labels}}' | python3 -m json.tool

# Ver puertos documentados
docker inspect label-test --format '{{json .Config.ExposedPorts}}' | python3 -m json.tool
# {"443/tcp": {}, "8080/tcp": {}}

# Ver capas — LABEL y EXPOSE son 0 bytes
docker history label-test --format "table {{.CreatedBy}}\t{{.Size}}" | head -8

docker rmi label-test
rm -rf /tmp/label_test
```

**Predicción**: labels y EXPOSE son metadata pura — crean capas de 0 bytes. No
afectan el tamaño de la imagen ni abren puertos. EXPOSE solo documenta; para
publicar puertos se necesita `docker run -p`. Las labels son visibles con
`docker inspect`.

### Ejercicio 8 — Build cache en acción

```bash
mkdir -p /tmp/cache_demo && cat > /tmp/cache_demo/Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "Step 1: install" && apk add --no-cache curl
COPY file.txt /app/file.txt
RUN echo "Step 3: process"
CMD ["echo", "Done"]
EOF

echo "version 1" > /tmp/cache_demo/file.txt

# Primer build — todo se ejecuta
docker build --progress=plain -t cache-demo /tmp/cache_demo 2>&1 | tail -5

# Segundo build — usa caché para todos los pasos
docker build --progress=plain -t cache-demo /tmp/cache_demo 2>&1 | grep -i "cached"
# Todos los pasos muestran CACHED

# Cambiar solo el archivo copiado
echo "version 2" > /tmp/cache_demo/file.txt

# Tercer build — solo se rehacen las capas desde COPY en adelante
docker build --progress=plain -t cache-demo /tmp/cache_demo 2>&1 | grep -iE "cached|DONE"
# Step 1 (RUN apk): CACHED
# Step 2 (COPY): se re-ejecuta (el archivo cambió)
# Step 3 (RUN echo): se re-ejecuta (capa anterior invalidada)

docker rmi cache-demo
rm -rf /tmp/cache_demo
```

**Predicción**: Docker cachea cada capa por su instrucción y contenido. Al
cambiar `file.txt`, la capa de `COPY` se invalida. Todas las capas posteriores
también se invalidan (efecto cascada), incluso si no cambiaron. La capa anterior
al `COPY` (`RUN apk add`) permanece cached porque ni su instrucción ni sus
dependencias cambiaron. Esto demuestra por qué el orden de instrucciones importa
para la eficiencia del cache (se cubre en S02/T01).
