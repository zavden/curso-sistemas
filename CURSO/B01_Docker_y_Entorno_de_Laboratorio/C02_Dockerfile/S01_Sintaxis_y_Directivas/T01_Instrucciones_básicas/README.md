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
```

## FROM — La imagen base

Todo Dockerfile empieza con `FROM`. Define la imagen sobre la que se construye.

```dockerfile
# Imagen base con tag específico
FROM debian:bookworm

# Imagen base con digest (inmutable)
FROM debian@sha256:abc123...

# Naming para multi-stage builds
FROM debian:bookworm AS builder

# Imagen vacía — para binarios estáticos (Go, Rust, C estático)
FROM scratch
```

`FROM scratch` crea una imagen sin sistema operativo base. Solo funciona con
binarios compilados estáticamente que no dependen de librerías del sistema:

```dockerfile
FROM scratch
COPY myapp /myapp
CMD ["/myapp"]
# La imagen resultante contiene SOLO el binario — unos pocos MB
```

**Regla práctica**: usar la variante más mínima que funcione.

| Variante | Tamaño aprox. | Contenido |
|---|---|---|
| `debian:bookworm` | ~120 MB | Debian completo (apt, bash, coreutils) |
| `debian:bookworm-slim` | ~75 MB | Debian sin docs, man pages, locales extra |
| `alpine:3.19` | ~7 MB | musl libc, busybox, apk (muy mínimo) |
| `scratch` | 0 MB | Nada — solo tu binario |

## RUN — Ejecutar comandos durante el build

`RUN` ejecuta un comando y guarda el resultado como una nueva capa de la imagen.

### Forma shell

```dockerfile
# Se ejecuta como /bin/sh -c "..."
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
# Se ejecuta directamente, sin shell
RUN ["apt-get", "install", "-y", "gcc"]
```

No hay expansión de variables (`$VAR` se toma literal). Útil cuando el shell
interfiere o no está disponible (imágenes `scratch`).

### Buena práctica: combinar comandos relacionados

Cada `RUN` crea una capa. Los archivos eliminados en una capa posterior **siguen
ocupando espacio** en las capas anteriores. Por eso, la instalación y limpieza
deben estar en el mismo `RUN`:

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

## COPY — Copiar archivos al filesystem de la imagen

`COPY` toma archivos del **contexto de build** (el directorio que pasas a
`docker build`) y los coloca en la imagen.

```dockerfile
# Copiar un archivo
COPY main.c /app/main.c

# Copiar un directorio completo
COPY src/ /app/src/

# Copiar todo el contexto (usar con .dockerignore)
COPY . /app/

# Establecer propietario al copiar
COPY --chown=appuser:appuser config.json /app/config.json

# Copiar desde otro stage (multi-stage builds)
COPY --from=builder /app/binary /usr/local/bin/
```

Las rutas de origen son **relativas al contexto de build**, nunca absolutas del
host. No puedes hacer `COPY /etc/passwd /app/` — Docker no permite acceder a
archivos fuera del contexto.

## ADD — COPY con extras

`ADD` funciona como `COPY` pero con dos features adicionales:

```dockerfile
# Descomprime automáticamente archivos tar
ADD app.tar.gz /app/
# El contenido descomprimido queda en /app/

# Puede descargar desde URLs (no recomendado)
ADD https://example.com/file.tar.gz /tmp/
```

**Regla general**: usar `COPY` siempre, excepto cuando necesites descompresión
automática de tar. Para descargas, `RUN curl` o `RUN wget` dan más control sobre
caché y manejo de errores.

```dockerfile
# Preferir esto:
RUN curl -fsSL https://example.com/tool.tar.gz | tar xz -C /usr/local/bin/

# Sobre esto:
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
# MAL: cd no persiste
RUN cd /app
RUN ls          # Ejecuta ls en /, no en /app

# BIEN: WORKDIR persiste
WORKDIR /app
RUN ls          # Ejecuta ls en /app
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

La combinación ENTRYPOINT + CMD se cubre en el siguiente tópico (T02).

## Instrucciones adicionales

### EXPOSE

Documenta qué puertos usa el contenedor. No abre ni publica puertos:

```dockerfile
EXPOSE 8080
EXPOSE 443/tcp
EXPOSE 53/udp
```

### LABEL

Metadata key-value para la imagen:

```dockerfile
LABEL maintainer="team@example.com"
LABEL version="1.0"
LABEL description="API server"

# Múltiples labels en una instrucción (una sola capa)
LABEL maintainer="team@example.com" version="1.0"
```

### VOLUME

Declara un punto de montaje que se convertirá en volumen anónimo:

```dockerfile
VOLUME /app/data
VOLUME ["/var/log", "/app/uploads"]
```

### SHELL

Cambia el shell por defecto para instrucciones en forma shell:

```dockerfile
# Por defecto: /bin/sh -c
SHELL ["/bin/bash", "-c"]
RUN echo ${BASH_VERSION}  # Ahora usa bash
```

### STOPSIGNAL

Cambia la señal que `docker stop` envía a PID 1:

```dockerfile
STOPSIGNAL SIGQUIT  # Default: SIGTERM
```

Nginx usa SIGQUIT para graceful shutdown (terminar requests en curso antes de
salir).

---

## Ejercicios

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

docker build -t shell-test /tmp/docker_shell_test
# En el build verás:
# "Hello from shell form"     ← variable expandida
# "$GREETING from exec form"  ← literal, sin expandir

docker rmi shell-test
rm -rf /tmp/docker_shell_test
```

### Ejercicio 3 — Efecto de WORKDIR

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
