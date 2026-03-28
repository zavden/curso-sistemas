# T03 — Inspección

## `docker image inspect`

El comando `docker image inspect` devuelve toda la metadata de una imagen en formato JSON.
Es la fuente de verdad sobre cómo se configuró la imagen.

```bash
docker image inspect debian:bookworm
```

La salida es extensa. Para extraer información específica se usa `--format` con
Go templates o se filtra con herramientas como `jq` o `python3 -m json.tool`.

### Campos importantes

#### Plataforma

```bash
docker image inspect debian:bookworm --format '{{.Architecture}}/{{.Os}}'
# amd64/linux
```

Indica para qué arquitectura fue compilada la imagen. Las imágenes multi-platform
(manifest lists) contienen variantes para múltiples arquitecturas; Docker selecciona
automáticamente la de tu plataforma al hacer pull.

#### Configuración de ejecución

```bash
docker image inspect debian:bookworm --format '{{json .Config}}' | python3 -m json.tool
```

Campos clave dentro de `.Config`:

| Campo | Significado |
|-------|------------|
| `Cmd` | Comando por defecto (CMD del Dockerfile) |
| `Entrypoint` | Punto de entrada (ENTRYPOINT del Dockerfile) |
| `Env` | Variables de entorno definidas con ENV |
| `ExposedPorts` | Puertos documentados con EXPOSE |
| `WorkingDir` | Directorio de trabajo (WORKDIR) |
| `User` | Usuario que ejecuta el proceso (USER) |
| `Volumes` | Puntos de montaje declarados con VOLUME |
| `Labels` | Metadata key-value definida con LABEL |

```bash
# Ver el CMD de una imagen
docker image inspect nginx:latest --format '{{json .Config.Cmd}}'
# ["nginx","-g","daemon off;"]

# Ver el ENTRYPOINT
docker image inspect nginx:latest --format '{{json .Config.Entrypoint}}'
# ["/docker-entrypoint.sh"]

# Ver las variables de entorno
docker image inspect nginx:latest --format '{{json .Config.Env}}' | python3 -m json.tool
# [
#     "PATH=/usr/local/sbin:/usr/local/bin:...",
#     "NGINX_VERSION=1.25.4",
#     "NJS_VERSION=0.8.3",
#     ...
# ]

# Ver los puertos expuestos
docker image inspect nginx:latest --format '{{json .Config.ExposedPorts}}'
# {"80/tcp":{}}
```

#### Capas del filesystem

```bash
docker image inspect debian:bookworm --format '{{json .RootFS}}' | python3 -m json.tool
```

```json
{
    "Type": "layers",
    "Layers": [
        "sha256:a1b2c3d4e5f6..."
    ]
}
```

Cada entrada en `Layers` es el hash SHA256 de una capa diff. El número de capas indica
cuántas instrucciones que modifican el filesystem tiene el Dockerfile subyacente.

```bash
# Contar las capas de una imagen
docker image inspect debian:bookworm --format '{{len .RootFS.Layers}}'
# 1  (debian:bookworm tiene una sola capa)

docker image inspect python:3.12-bookworm --format '{{len .RootFS.Layers}}'
# 7  (python añade varias capas sobre debian)
```

#### Tamaño

```bash
docker image inspect debian:bookworm --format '{{.Size}}'
# 117218816  (en bytes)

# Convertir a MB
docker image inspect debian:bookworm --format '{{printf "%.1f MB" (divf .Size 1048576.0)}}'
```

Este tamaño es el **tamaño de la imagen completa** (todas sus capas). No considera
deduplicación con otras imágenes.

> **Nota**: El campo `VirtualSize` fue deprecado en la API v1.44 de Docker. Era
> redundante con `Size` para imágenes locales.

### Inspeccionar contenedores vs imágenes

`docker inspect` (sin `image`) también funciona y puede inspeccionar contenedores.
Para evitar ambigüedad si un contenedor y una imagen tienen el mismo nombre:

```bash
docker image inspect debian:bookworm    # Explícitamente imagen
docker container inspect my-container    # Explícitamente contenedor
docker inspect debian:bookworm          # Busca primero contenedores, luego imágenes
```

## `docker history`

Muestra las capas de una imagen con la instrucción que creó cada una y su tamaño.
Es como ver el Dockerfile "reconstruido" a partir de la imagen.

```bash
docker history debian:bookworm
```

Salida típica:

```
IMAGE          CREATED       CREATED BY                                      SIZE      COMMENT
a1b2c3d4e5f6   2 weeks ago   /bin/sh -c #(nop)  CMD ["bash"]                 0B
b2c3d4e5f6a7   2 weeks ago   /bin/sh -c #(nop) ADD file:abc123... in /       117MB
```

### Leer la salida

- **IMAGE**: ID de la capa. `<missing>` aparece para capas que vinieron de un pull
  remoto — Docker no almacena los IDs intermedios de capas descargadas. Es normal
  y no indica un problema.

- **CREATED BY**: la instrucción del Dockerfile que generó la capa.
  - `#(nop)` indica que la instrucción no modificó el filesystem (CMD, ENV, EXPOSE, etc.)
  - Las instrucciones se truncan por defecto — usar `--no-trunc` para verlas completas.

- **SIZE**: tamaño de la capa.
  - `0B` para instrucciones de metadata (CMD, ENV, EXPOSE, LABEL, WORKDIR)
  - Valores positivos para instrucciones que modifican el filesystem (RUN, COPY, ADD)

### `--no-trunc`

```bash
docker history --no-trunc debian:bookworm
```

Muestra las instrucciones completas sin truncar. Útil para ver exactamente qué hizo
cada RUN (especialmente los que encadenan múltiples comandos con `&&`).

### `--format`

```bash
# Solo instrucciones y tamaños, en formato tabla
docker history --format "table {{.CreatedBy}}\t{{.Size}}" debian:bookworm

# JSON
docker history --format json debian:bookworm | python3 -m json.tool
```

### Leer el history de una imagen derivada

Cuando construyes una imagen basada en otra, `docker history` muestra **todas** las capas,
incluyendo las de la imagen base.

```bash
# Crear una imagen simple basada en Debian
echo 'FROM debian:bookworm
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*
CMD ["curl", "--version"]' > /tmp/Dockerfile.curl

docker build -t curl-demo -f /tmp/Dockerfile.curl /tmp

docker history curl-demo
```

Salida:

```
IMAGE          CREATED          CREATED BY                                      SIZE
e1f2a3b4c5d6   5 seconds ago    /bin/sh -c #(nop)  CMD ["curl" "--version"]     0B
d1e2f3a4b5c6   6 seconds ago    /bin/sh -c apt-get update && apt-get ins...     30MB
a1b2c3d4e5f6   2 weeks ago      /bin/sh -c #(nop)  CMD ["bash"]                 0B      ← Aquí empieza debian:bookworm
b2c3d4e5f6a7   2 weeks ago      /bin/sh -c #(nop) ADD file:abc123... in /       117MB
```

Las capas de arriba son las que agregó `curl-demo`. Las de abajo son las de `debian:bookworm`.

## Tamaño real vs virtual

### El problema

`docker images` muestra un "SIZE" por cada imagen que representa el tamaño **virtual**:
la suma de todas las capas de esa imagen. Si dos imágenes comparten capas, esas capas
se cuentan en el tamaño de **ambas**, aunque en disco solo están almacenadas una vez.

```bash
docker images | grep -E "debian|python"
# debian   bookworm    a1b2c3   2 weeks ago   117MB
# python   3.12        d4e5f6   2 weeks ago   1.01GB
```

Parece que ocupan 117MB + 1.01GB = 1.13GB. Pero `python:3.12-bookworm` está basada
en `debian:bookworm`, así que comparten las capas de Debian.

### El espacio real

```bash
docker system df
```

```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          2         0         918MB     918MB (100%)
Containers      0         0         0B        0B
Local Volumes   0         0         0B        0B
Build Cache     0         0         0B        0B
```

El **SIZE** aquí refleja el espacio real ocupado con deduplicación. 918MB es menor que
la suma de los tamaños virtuales (1.13GB) porque las capas de Debian se comparten.

### Desglose detallado

```bash
docker system df -v
```

```
Images space usage:

REPOSITORY   TAG        IMAGE ID       CREATED       SIZE      SHARED SIZE   UNIQUE SIZE   VIRTUAL SIZE
debian       bookworm   a1b2c3d4e5f6   2 weeks ago   117MB     117MB         0B            117MB
python       3.12       d4e5f6a7b8c9   2 weeks ago   1.01GB    117MB         893MB         1.01GB
```

| Campo | Significado |
|-------|------------|
| SIZE | Tamaño total de la imagen (= VIRTUAL SIZE) |
| SHARED SIZE | Capas compartidas con otras imágenes locales |
| UNIQUE SIZE | Capas que solo esta imagen tiene |
| VIRTUAL SIZE | Tamaño "virtual" (igual a SIZE, incluido por claridad) |

Las capas de Debian (117MB) aparecen como SHARED en ambas imágenes. El UNIQUE SIZE de
Python (893MB) es lo que realmente añade sobre Debian.

## Imágenes multi-platform

Las imágenes populares están disponibles para múltiples arquitecturas (amd64, arm64,
arm/v7, etc.) bajo el mismo tag. Esto se logra con **manifest lists**.

```bash
# Ver las plataformas disponibles
docker manifest inspect nginx:latest
```

Cuando haces `docker pull nginx`, Docker descarga automáticamente la variante para
tu arquitectura. `docker image inspect` muestra la plataforma de la imagen local:

```bash
docker image inspect nginx:latest --format '{{.Architecture}}'
# amd64  (o arm64, según tu máquina)
```

En Docker Desktop (macOS con Apple Silicon), se descarga la variante arm64 por defecto.
Para forzar otra arquitectura:

```bash
docker pull --platform linux/amd64 nginx:latest
```

## Práctica

### Ejercicio 1: Inspeccionar una imagen oficial

```bash
# Descargar si no existe
docker pull nginx:latest

# Ver la configuración completa
docker image inspect nginx:latest --format '{{json .Config}}' | python3 -m json.tool

# Preguntas a responder:
# 1. ¿Cuál es el ENTRYPOINT? ¿Y el CMD?
# 2. ¿Qué variables de entorno están definidas?
# 3. ¿Qué puertos están expuestos?
# 4. ¿Cuántas capas tiene?
# 5. ¿Qué usuario ejecuta el proceso?
```

### Ejercicio 2: Comparar history de base vs derivada

```bash
# Ver las capas de debian
docker history debian:bookworm

# Ver las capas de python (derivada de debian)
docker history python:3.12-bookworm

# Preguntas:
# 1. ¿Cuántas capas tiene Debian? ¿Y Python?
# 2. ¿Cuál es la capa más pesada de Python?
# 3. ¿Puedes identificar dónde termina Debian y empieza Python?
```

### Ejercicio 3: Calcular espacio real con deduplicación

```bash
# Descargar 3 imágenes que comparten capas
docker pull debian:bookworm
docker pull python:3.12-bookworm
docker pull node:20-bookworm

# Ver tamaños virtuales
docker images | grep -E "debian|python|node"

# Sumar los tamaños virtuales mentalmente (será > espacio real)

# Ver espacio real
docker system df -v | grep -E "debian|python|node|REPOSITORY"

# Preguntas:
# 1. ¿Cuánto ocupan las 3 en total según docker images?
# 2. ¿Cuánto ocupan realmente según docker system df?
# 3. ¿Cuánto espacio se ahorra por deduplicación?
```

### Ejercicio 4: Entender `<missing>` en history

```bash
docker history python:3.12-bookworm

# Observar que muchas capas muestran <missing> en IMAGE ID
# Esto es normal para capas descargadas remotamente
# Solo las capas construidas localmente tienen IDs visibles

# Construir una imagen local y ver la diferencia
echo 'FROM debian:bookworm
RUN echo "hello"' | docker build -t test-local -

docker history test-local
# La capa de "echo hello" sí tiene un IMAGE ID (fue construida localmente)
# La capa de debian muestra <missing> (fue descargada)

docker rmi test-local
```
