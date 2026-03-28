# T03 — Inspección

## `docker image inspect`

El comando `docker image inspect` devuelve toda la metadata de una imagen en formato JSON.
Es la fuente de verdad sobre cómo se configuró la imagen.

```bash
docker image inspect debian:bookworm
```

La salida es extensa — fácilmente cientos de líneas. Para extraer información específica
se usa `--format` con Go templates, o se canaliza a `jq`/`python3 -m json.tool`.

## `docker images` / `docker image ls`

Antes de inspeccionar una imagen, necesitas saber qué tienes localmente:

```bash
# Listar todas las imágenes locales
docker images
# equivalente a: docker image ls

# Filtrar por nombre
docker images debian
docker images "python*"

# Mostrar también imágenes intermedias (dangling)
docker images -a

# Solo IDs (útil para scripting)
docker images -q
```

### Imágenes dangling (`<none>:<none>`)

Cuando reconstruyes una imagen con el mismo tag, la versión anterior pierde su tag
y queda como `<none>:<none>` — una imagen "dangling":

```bash
docker images
# REPOSITORY   TAG       IMAGE ID       SIZE
# myapp        latest    a1b2c3d4e5f6   150MB   ← versión nueva
# <none>       <none>    f6e5d4c3b2a1   148MB   ← versión anterior (dangling)
```

```bash
# Ver solo imágenes dangling
docker images --filter dangling=true

# Eliminar imágenes dangling (seguro — no borra imágenes en uso)
docker image prune
# WARNING! This will remove all dangling images.

# Eliminar TODAS las imágenes no usadas por contenedores (más agresivo)
docker image prune -a
```

### Formato personalizado

```bash
# Tabla custom
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"

# Solo repositorio:tag
docker images --format '{{.Repository}}:{{.Tag}}'

# Con digest
docker images --digests
```

## Go templates con `--format`

Docker usa Go templates para formatear la salida de `--format`. Esto permite extraer
campos específicos sin herramientas externas.

### Sintaxis básica

```bash
# Acceder a campos directos
docker image inspect debian:bookworm --format '{{.Architecture}}'
# amd64

# Acceder a campos anidados
docker image inspect debian:bookworm --format '{{.Config.Cmd}}'
# [/bin/bash]

# Formateo con printf
docker image inspect debian:bookworm --format '{{printf "%.1f MB" (divf .Size 1048576.0)}}'
# 117.2 MB
```

### Iteración y condicionales

```bash
# Iterar sobre arrays/slices
docker image inspect debian:bookworm --format '{{range .RepoTags}}{{.}} {{end}}'
# debian:bookworm debian:12 debian:latest

# Iterar sobre maps (key-value)
docker image inspect nginx:latest --format '{{range $k, $v := .Config.Labels}}{{$k}}={{$v}}
{{end}}'
# maintainer=NGINX Docker Maintainers <docker-maint@nginx.com>

# Condicionales
docker image inspect debian:bookworm --format '{{if .Config.Entrypoint}}Entrypoint: {{.Config.Entrypoint}}{{else}}No entrypoint{{end}}'
# No entrypoint

# Combinar con json para pasar a jq
docker image inspect debian:bookworm --format '{{json .Config}}' | jq '.Env'
```

### Campos útiles organizados por categoría

#### Plataforma

```bash
docker image inspect debian:bookworm --format '{{.Os}}/{{.Architecture}}'
# linux/amd64
```

Indica para qué arquitectura fue compilada la imagen. Las imágenes multi-platform
(manifest lists) contienen variantes para múltiples arquitecturas; Docker selecciona
automáticamente la de tu plataforma al hacer pull.

#### Configuración de ejecución

```bash
docker image inspect debian:bookworm --format '{{json .Config}}' | python3 -m json.tool
```

Campos clave dentro de `.Config`:

| Campo | Significado | Ejemplo |
|-------|-------------|---------|
| `Cmd` | Comando por defecto (CMD del Dockerfile) | `["bash"]` |
| `Entrypoint` | Punto de entrada (ENTRYPOINT del Dockerfile) | `["/docker-entrypoint.sh"]` |
| `Env` | Variables de entorno (ENV) | `["PATH=...", "NGINX_VERSION=1.25.4"]` |
| `ExposedPorts` | Puertos documentados (EXPOSE) | `{"80/tcp":{}}` |
| `WorkingDir` | Directorio de trabajo (WORKDIR) | `/app` |
| `User` | Usuario que ejecuta (USER) | `nginx` |
| `Volumes` | Puntos de montaje (VOLUME) | `{"/var/lib/postgresql/data":{}}` |
| `Labels` | Metadata key-value (LABEL) | `{"maintainer":"..."}` |
| `StopSignal` | Signal para detener contenedor | `SIGTERM` (default) |

```bash
# Ver el CMD de una imagen
docker image inspect nginx:latest --format '{{json .Config.Cmd}}'
# ["nginx","-g","daemon off;"]

# Ver el ENTRYPOINT
docker image inspect nginx:latest --format '{{json .Config.Entrypoint}}'
# ["/docker-entrypoint.sh"]

# Ver las variables de entorno una por línea
docker image inspect nginx:latest --format '{{range .Config.Env}}{{.}}
{{end}}'

# Ver los puertos expuestos
docker image inspect nginx:latest --format '{{json .Config.ExposedPorts}}'
# {"80/tcp":{}}

# Ver los labels
docker image inspect nginx:latest --format '{{range $k, $v := .Config.Labels}}{{$k}}={{$v}}
{{end}}'

# Ver el stop signal
docker image inspect nginx:latest --format '{{.Config.StopSignal}}'
# SIGQUIT  (nginx usa SIGQUIT para graceful shutdown)
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

#### Metadatos de la imagen

```bash
# ID de la imagen (hash del config JSON)
docker image inspect debian:bookworm --format '{{.Id}}'
# sha256:a6d8...

# Tags asociados
docker image inspect debian:bookworm --format '{{range .RepoTags}}{{.}} {{end}}'
# debian:bookworm debian:12

# Digests
docker image inspect debian:bookworm --format '{{range .RepoDigests}}{{.}} {{end}}'
# debian@sha256:abc123...

# Fecha de creación
docker image inspect debian:bookworm --format '{{.Created}}'
# 2024-01-15T10:30:00.000000000Z

# Solo la fecha (sin hora)
docker image inspect debian:bookworm --format '{{.Created}}' | cut -dT -f1
# 2024-01-15

# Tamaño total en MB
docker image inspect debian:bookworm --format '{{printf "%.1f MB" (divf .Size 1048576.0)}}'
# 117.2 MB
```

> **Nota**: El campo `VirtualSize` fue deprecado en la API v1.44 de Docker. Era
> redundante con `Size` para imágenes locales.

### Formato JSON con jq

Para consultas más complejas, `{{json .}}` convierte cualquier campo a JSON y permite
usar `jq`:

```bash
# Ver todos los keys de nivel superior
docker image inspect debian:bookworm --format '{{json .}}' | jq 'keys'

# Filtrar variables de entorno
docker image inspect python:3.12 --format '{{json .Config.Env}}' | jq '.[] | select(startswith("PYTHON"))'

# Obtener todos los digests
docker image inspect debian:bookworm --format '{{json .RepoDigests}}' | jq '.[]'
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

| Columna | Significado |
|---------|-------------|
| `IMAGE` | ID de la capa. `<missing>` = capa descargada remotamente (normal) |
| `CREATED` | Cuándo se creó la capa |
| `CREATED BY` | La instrucción del Dockerfile que generó la capa |
| `SIZE` | Tamaño de la capa |

#### El significado de `#(nop)`

Las instrucciones en `CREATED BY` usan dos formatos:

```
#(nop)  CMD ["bash"]              → instrucción de metadata (0B, no modifica filesystem)
#(nop)  ENV PATH=...              → instrucción de metadata
#(nop)  EXPOSE 80                 → instrucción de metadata
/bin/sh -c apt-get update...      → instrucción RUN (modifica filesystem, >0B)
```

`#(nop)` = "no operation" a nivel de filesystem. La instrucción solo agrega metadata
a la configuración de la imagen, no crea archivos.

#### Entendiendo `<missing>`

`<missing>` aparece en la columna IMAGE para capas que Docker descargó de un registry.
Docker no almacena los IDs intermedios de las capas descargadas — solo el ID de la imagen
final. Es completamente normal y no indica un problema.

```
IMAGE           CREATED BY                                              SIZE
<missing>       /bin/sh -c apt-get update && apt-get install...         45MB  ← descargada
<missing>       /bin/sh -c #(nop)  CMD ["bash"]                        0B    ← descargada
a1b2c3d4e5f6    /bin/sh -c #(nop)  ADD file:...                        80MB  ← construida local
```

Solo las capas construidas localmente muestran su ID.

### Flags útiles

```bash
# Ver instrucciones completas sin truncar
docker history --no-trunc debian:bookworm

# Formato tabla con columnas custom
docker history --format "table {{.CreatedBy}}\t{{.Size}}" debian:bookworm

# Formato JSON
docker history --format json debian:bookworm | python3 -m json.tool

# Ver solo las capas con contenido real (no metadata)
docker history debian:bookworm | grep -v '#(nop)'
```

### Leer el history de una imagen derivada

Cuando construyes una imagen basada en otra, `docker history` muestra **todas** las capas,
incluyendo las de la imagen base (la más reciente arriba):

```bash
# Crear una imagen simple basada en Debian
cat > /tmp/Dockerfile.curl << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*
CMD ["curl", "--version"]
EOF

docker build -t curl-demo -f /tmp/Dockerfile.curl /tmp
docker history curl-demo
```

Salida:

```
IMAGE          CREATED          CREATED BY                                      SIZE
e1f2a3b4c5d6   5 seconds ago    /bin/sh -c #(nop)  CMD ["curl" "--version"]     0B       ← curl-demo
d1e2f3a4b5c6   6 seconds ago    /bin/sh -c apt-get update && apt-get ins...     30MB     ← curl-demo
a1b2c3d4e5f6   2 weeks ago      /bin/sh -c #(nop)  CMD ["bash"]                0B       ← debian
b2c3d4e5f6a7   2 weeks ago      /bin/sh -c #(nop)  ADD file:abc123... in /     117MB    ← debian
```

Las capas de arriba son las que agregó `curl-demo`. Las de abajo son las de `debian:bookworm`.
La frontera entre ambas es visible por la diferencia en `CREATED` (seconds ago vs weeks ago).

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
Build Cache     0         0B        0B        0B
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
|-------|-------------|
| `SIZE` | Tamaño total de la imagen (= VIRTUAL SIZE) |
| `SHARED SIZE` | Capas compartidas con otras imágenes locales |
| `UNIQUE SIZE` | Capas que solo esta imagen tiene |
| `VIRTUAL SIZE` | Tamaño "virtual" (igual a SIZE) |

Las capas de Debian (117MB) aparecen como SHARED en ambas imágenes. El UNIQUE SIZE de
Python (893MB) es lo que realmente añade sobre Debian.

### Build Cache

Docker también almacena capas intermedias del proceso de build:

```bash
# Ver el build cache
docker builder df
# Muestra el tamaño del caché y cuánto es reclaimable

# Limpiar build cache
docker builder prune
```

## Imágenes multi-platform

Las imágenes populares están disponibles para múltiples arquitecturas (amd64, arm64,
arm/v7, etc.) bajo el mismo tag. Esto se logra con **manifest lists** (OCI index).

```bash
# Ver las plataformas disponibles
docker manifest inspect nginx:latest
```

```json
{
  "manifests": [
    {
      "digest": "sha256:aaa...",
      "platform": {
        "architecture": "amd64",
        "os": "linux"
      }
    },
    {
      "digest": "sha256:bbb...",
      "platform": {
        "architecture": "arm64",
        "os": "linux",
        "variant": "v8"
      }
    }
  ]
}
```

Cuando haces `docker pull nginx`, Docker:
1. Descarga el manifest list
2. Identifica tu arquitectura
3. Descarga el manifest específico para tu plataforma
4. Descarga las capas correspondientes

```bash
# Ver la plataforma de la imagen local
docker image inspect nginx:latest --format '{{.Architecture}}'
# amd64  (o arm64, según tu máquina)

# Forzar otra arquitectura
docker pull --platform linux/arm64 nginx:latest
```

### `docker buildx imagetools`

Para inspeccionar imágenes remotas sin descargarlas:

```bash
docker buildx imagetools inspect nginx:latest
# Name:      docker.io/library/nginx:latest
# MediaType: ...
# Digest:    sha256:...
#
# Manifests:
#   Name:   ...@sha256:aaa...
#   Platform: linux/amd64
#   ...
```

## Inspeccionar contenedores vs imágenes

`docker inspect` (sin `image` ni `container`) busca en ambos. Para evitar ambigüedad:

```bash
docker image inspect debian:bookworm    # Explícitamente imagen
docker container inspect my-container    # Explícitamente contenedor
docker inspect debian:bookworm          # Busca primero contenedores, luego imágenes
```

### Campos útiles de contenedores

Para contenedores, hay campos adicionales que no existen en imágenes:

```bash
# Estado del contenedor
docker container inspect my-container --format '{{.State.Status}}'
# running, exited, paused, restarting, dead

# PID del proceso principal (en el host)
docker container inspect my-container --format '{{.State.Pid}}'
# 12345

# Mounts (volúmenes y bind mounts)
docker container inspect my-container --format '{{json .Mounts}}' | jq '.'

# IP del contenedor en la red bridge
docker container inspect my-container --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
# 172.17.0.2

# GraphDriver — las capas del contenedor
docker container inspect my-container --format 'Upper: {{.GraphDriver.Data.UpperDir}}'
# Upper: /var/lib/docker/overlay2/<hash>/diff
```

## Tabla: Instrucción Dockerfile → campo en inspect

Cada instrucción del Dockerfile se refleja en un campo específico de `docker image inspect`.
Esta tabla sirve como referencia rápida para saber dónde buscar:

| Instrucción Dockerfile | Campo en inspect | Ejemplo de consulta |
|------------------------|------------------|---------------------|
| `FROM` | `.RootFS.Layers` | `--format '{{len .RootFS.Layers}}'` |
| `RUN` | `.RootFS.Layers` (añade capa) | `docker history <image>` |
| `CMD` | `.Config.Cmd` | `--format '{{json .Config.Cmd}}'` |
| `ENTRYPOINT` | `.Config.Entrypoint` | `--format '{{json .Config.Entrypoint}}'` |
| `ENV` | `.Config.Env` | `--format '{{json .Config.Env}}'` |
| `EXPOSE` | `.Config.ExposedPorts` | `--format '{{json .Config.ExposedPorts}}'` |
| `VOLUME` | `.Config.Volumes` | `--format '{{json .Config.Volumes}}'` |
| `WORKDIR` | `.Config.WorkingDir` | `--format '{{.Config.WorkingDir}}'` |
| `USER` | `.Config.User` | `--format '{{.Config.User}}'` |
| `LABEL` | `.Config.Labels` | `--format '{{json .Config.Labels}}'` |
| `STOPSIGNAL` | `.Config.StopSignal` | `--format '{{.Config.StopSignal}}'` |
| `HEALTHCHECK` | `.Config.Healthcheck` | `--format '{{json .Config.Healthcheck}}'` |
| `SHELL` | `.Config.Shell` | `--format '{{json .Config.Shell}}'` |
| `ARG` | No visible en inspect | Solo disponible durante build |
| `COPY` / `ADD` | `.RootFS.Layers` (añade capa) | `docker history <image>` |

> **Nota**: `ARG` es la única instrucción que no deja rastro en la imagen final.
> Los valores de `ARG` solo existen durante el build y no se almacenan en la metadata.
> Si necesitas que un valor de build sea visible en runtime, usa `ARG` + `ENV`:
> ```dockerfile
> ARG VERSION=1.0
> ENV APP_VERSION=$VERSION
> ```

> **Nota**: `RUN`, `COPY` y `ADD` son las únicas instrucciones que crean capas con
> contenido en el filesystem. El resto solo modifica metadata (aparecen como `#(nop)`
> en `docker history`).

## Práctica

### Ejercicio 1: Inspeccionar una imagen oficial

```bash
# Descargar si no existe
docker pull nginx:latest

# Ver la configuración completa con jq
docker image inspect nginx:latest --format '{{json .Config}}' | jq '.'

# Extraer información específica
docker image inspect nginx:latest --format 'Entrypoint: {{.Config.Entrypoint}}
Cmd: {{.Config.Cmd}}
ExposedPorts: {{json .Config.ExposedPorts}}
User: {{if .Config.User}}{{.Config.User}}{{else}}root (default){{end}}
WorkingDir: {{if .Config.WorkingDir}}{{.Config.WorkingDir}}{{else}}/ (default){{end}}
StopSignal: {{.Config.StopSignal}}
Layers: {{len .RootFS.Layers}}
'
```

### Ejercicio 2: Comparar history de base vs derivada

```bash
# Ver las capas de debian
docker history debian:bookworm

# Ver las capas de python (derivada de debian)
docker history python:3.12-bookworm

# Contar capas
echo "debian: $(docker image inspect debian:bookworm --format '{{len .RootFS.Layers}}') capas"
echo "python: $(docker image inspect python:3.12-bookworm --format '{{len .RootFS.Layers}}') capas"

# Ver las capas con contenido real (no metadata)
echo "=== debian ==="
docker history debian:bookworm | grep -v '#(nop)'
echo "=== python ==="
docker history python:3.12-bookworm | grep -v '#(nop)'
```

### Ejercicio 3: Calcular espacio real con deduplicación

```bash
# Descargar 3 imágenes que comparten capas
docker pull debian:bookworm
docker pull python:3.12-bookworm
docker pull node:20-bookworm

# Ver tamaños virtuales
docker images | grep -E "debian|python|node"

# Ver espacio real (con deduplicación)
docker system df -v 2>/dev/null | head -20

# Calcular cuánto se ahorra:
# SHARED SIZE de python = capas de debian
# SHARED SIZE de node = capas de debian
# Espacio real = debian + unique_python + unique_node
```

### Ejercicio 4: Entender `<missing>` en history

```bash
docker history python:3.12-bookworm

# Observar que muchas capas muestran <missing> en IMAGE ID
# Esto es normal para capas descargadas remotamente

# Construir una imagen local y ver la diferencia
docker build -t test-local - << 'EOF'
FROM debian:bookworm
RUN echo "hello" > /hello.txt
EOF

docker history test-local
# La capa de "echo hello" sí tiene un IMAGE ID (fue construida localmente)
# La capa de debian muestra <missing> (fue descargada)

docker rmi test-local
```

### Ejercicio 5: Inspeccionar un contenedor activo

```bash
# Iniciar un contenedor
docker run -d --name inspect-test nginx:latest

# Ver estado y PID
docker container inspect inspect-test --format 'Status: {{.State.Status}}
PID: {{.State.Pid}}
StartedAt: {{.State.StartedAt}}'

# Ver redes
docker container inspect inspect-test --format '{{range $k, $v := .NetworkSettings.Networks}}Network: {{$k}}  IP: {{$v.IPAddress}}
{{end}}'

# Ver las capas
docker container inspect inspect-test --format 'Lower: {{.GraphDriver.Data.LowerDir}}
Upper: {{.GraphDriver.Data.UpperDir}}'

# Limpiar
docker stop inspect-test && docker rm inspect-test
```

### Ejercicio 6: Inspección programática para scripts

```bash
# Función: verificar si una imagen existe localmente
image_exists() {
    docker image inspect "$1" > /dev/null 2>&1
}
image_exists "debian:bookworm" && echo "existe" || echo "no existe"

# Función: obtener el tamaño en MB
image_size_mb() {
    docker image inspect "$1" --format '{{.Size}}' | awk '{printf "%.1f MB", $1/1048576}'
}
echo "debian:bookworm ocupa $(image_size_mb debian:bookworm)"

# Listar todas las imágenes locales con su arquitectura
docker images --format '{{.Repository}}:{{.Tag}}' | while read img; do
    arch=$(docker image inspect "$img" --format '{{.Architecture}}' 2>/dev/null)
    size=$(docker image inspect "$img" --format '{{printf "%.0f" (divf .Size 1048576.0)}}' 2>/dev/null)
    [ -n "$arch" ] && printf "%-40s  %-8s  %sMB\n" "$img" "$arch" "$size"
done
```
