# T03 — Inspección

## `docker image inspect`

El comando `docker image inspect` devuelve toda la metadata de una imagen en formato JSON.
Es la fuente de verdad sobre cómo se configuró la imagen.

```bash
docker image inspect debian:bookworm
```

La salida es extensa — fácilmente cientos de líneas. Para extraer información específica
se usa `--format` con Go templates, o se canaliza a `jq`/`python3 -m json.tool`.

## Go templates con `--format`

Docker usa Go templates para formatear la salida de `--format`. Esto permite extraer
campos específicos sin tools externas.

### Conceptos básicos de Go templates

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

# Iterar sobre arrays/slices
docker image inspect debian:bookworm --format '{{range .RepoTags}}{{.}} {{end}}'
# debian:bookworm debian:12 debian:latest

# Iterar sobre maps
docker image inspect nginx:latest --format '{{range $k, $v := .Config.Labels}}{{$k}}={{$v}} {{end}}'
# maintainer=NGINX Docker Maintainers ...

# Condicionales
docker image inspect debian:bookworm --format '{{if .Config.Entrypoint}}{{.Config.Entrypoint}}{{else}}no entrypoint{{end}}'
```

### Campos útiles organizados por categoría

#### Plataforma

```bash
docker image inspect debian:bookworm --format '{{.Architecture}}/{{.Os}}'
# amd64/linux

# Detalle de OS/Arch
docker image inspect debian:bookworm --format '{{.Os}}/{{.Architecture}}/{{.Variant}}'
# linux/amd64/v2
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
| `Volumes` | Puntos de montaje (VOLUME) | `/var/lib/postgresql/data` |
| `Labels` | Metadata key-value (LABEL) | `{"maintainer":"..."}` |
| `StopSignal` | Signal para detener contenedor | `SIGTERM` |

```bash
# Ver el CMD de una imagen
docker image inspect nginx:latest --format '{{json .Config.Cmd}}'
# ["nginx","-g","daemon off;"]

# Ver el ENTRYPOINT
docker image inspect nginx:latest --format '{{json .Config.Entrypoint}}'
# ["/docker-entrypoint.sh"]

# Ver las variables de entorno
docker image inspect nginx:latest --format '{{json .Config.Env}}' | python3 -m json.tool

# Ver los puertos expuestos
docker image inspect nginx:latest --format '{{json .Config.ExposedPorts}}'
# {"80/tcp":{}}

# Ver los labels
docker image inspect nginx:latest --format '{{range $k, $v := .Config.Labels}}{{$k}}={{$v}}{{printf "\n"}}{{end}}'

# Ver el stop signal
docker image inspect nginx:latest --format '{{.Config.StopSignal}}'
# SIGWINCH
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
# ID de la imagen
docker image inspect debian:bookworm --format '{{.Id}}'
# sha256:a6d8...

# Tags asociados
docker image inspect debian:bookworm --format '{{range .RepoTags}}{{.}} {{end}}'
# debian:bookworm debian:12 debian:latest

# Digests (references)
docker image inspect debian:bookworm --format '{{range .RepoDigests}}{{.}} {{end}}'
# debian@sha256:abc123... (si están disponibles)

# Fecha de creación
docker image inspect debian:bookworm --format '{{.Created}}'
# 2024-01-15T10:30:00.000000000Z

# Formato legible de fecha
docker image inspect debian:bookworm --format '{{.Created}}' | cut -d'T' -f1
# 2024-01-15

# Tamaño total de la imagen
docker image inspect debian:bookworm --format '{{printf "%.1f MB" (divf .Size 1048576.0)}}'
# 117.2 MB
```

> **Nota**: El campo `VirtualSize` fue deprecado en la API v1.44 de Docker. Era
> redundante con `Size` para imágenes locales.

### Formato JSON completo

Para obtener el JSON completo con indentación:

```bash
# Sin truncation
docker image inspect debian:bookworm

# Como JSON válido (un objeto por línea)
docker image inspect debian:bookworm --format '{{json .}}'
# Útil para piping a jq

# Con jq para consultas avanzadas
docker image inspect debian:bookworm --format '{{json .}}' | jq '.Config.Env[] | select(startswith("PATH"))'

# Ver todos los keys de nivel superior
docker image inspect debian:bookworm --format '{{json .}}' | jq 'keys'
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
| `IMAGE` | ID de la capa. `<missing>` aparece para capas que vinieron de un pull remoto |
| `CREATED` | Cuándo se creó la capa |
| `CREATED BY` | La instrucción del Dockerfile que generó la capa |
| `SIZE` | Tamaño de la capa |
| `COMMENT` | Información adicional (usualmente vacía) |

#### El significado de `#(nop)`

- `#(nop)` indica que la instrucción no modificó el filesystem (CMD, ENV, EXPOSE, etc.)
- Las instrucciones se truncan por defecto — usar `--no-trunc` para verlas completas

```
#(nop)  CMD ["bash"]              → instrucción CMD (metadata, 0B)
#(nop)  ENV PATH=...              → instrucción ENV (metadata, 0B)
#(nop)  EXPOSE 80                 → instrucción EXPOSE (metadata, 0B)
/bin/sh -c apt-get update...      → instrucción RUN (modificó filesystem, >0B)
```

#### Entendiendo `<missing>`

`<missing>` aparece en la columna IMAGE para capas que Docker descargó de un registry.
Docker no almacena los IDs intermedios de las capas — solo el ID de la imagen final.
Es normal y no indica un problema.

```
IMAGE           CREATED BY
<missing>       /bin/sh -c apt-get update && apt-get install...  45MB  ← capa descargada
<missing>       /bin/sh -c #(nop)  CMD ["bash"]                   0B   ← capa descargada
a1b2c3d4e5f6   /bin/sh -c #(nop)  ADD file:...                    80MB  ← base (local o construida)
```

Solo las capas construidas localmente muestran su ID. Las descargadas son `<missing>`.

### Flags útiles

```bash
# Ver instrucciones completas sin truncar
docker history --no-trunc debian:bookworm

# Formatos alternativos
docker history --format "table {{.CreatedBy}}\t{{.Size}}" debian:bookworm
docker history --format json debian:bookworm | python3 -m json.tool

# No mostrar etiquetas (tags) - útil para ver solo las capas
docker history --no-trunc --format json debian:bookworm | jq '.'

# Ver solo las capas con contenido (no metadata)
docker history debian:bookworm | grep -v '#(nop)'

# Tamaño total acumulativo
docker history debian:bookworm | tail -n +2 | awk '{sum+=$4} END {print sum}'
```

### Leer el history de una imagen derivada

Cuando construyes una imagen basada en otra, `docker history` muestra **todas** las capas,
incluyendo las de la imagen base (de abajo hacia arriba, la más antigua primero).

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
e1f2a3b4c5d6   5 seconds ago    /bin/sh -c #(nop)  CMD ["curl" "--version"]     0B        ← curl-demo CMD
d1e2f3a4b5c6   6 seconds ago    /bin/sh -c apt-get update && apt-get ins...     30MB     ← curl-demo RUN
a1b2c3d4e5f6   2 weeks ago      /bin/sh -c #(nop)  CMD ["bash"]                 0B        ← debian:bookworm CMD
b2c3d4e5f6a7   2 weeks ago      /bin/sh -c #(nop)  ADD file:abc123... in /       117MB    ← debian:bookworm base
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
| `VIRTUAL SIZE` | Tamaño "virtual" (igual a SIZE, incluido por claridad) |

Las capas de Debian (117MB) aparecen como SHARED en ambas imágenes. El UNIQUE SIZE de
Python (893MB) es lo que realmente añade sobre Debian.

### Build Cache

```bash
docker system df -v

# También puedes ver el build cache
docker builder df
# Muestra el tamaño del caché de build y qué imágenes se construyeron
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
  ],
  "mediaType": "application/vnd.docker.distribution.manifest.list.v2+json",
  "schemaVersion": 2
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

# En Docker Desktop (macOS con Apple Silicon), se descarga la variante arm64 por defecto
docker pull --platform linux/amd64 nginx:latest  # Forzar amd64
docker pull --platform linux/arm64/v7 nginx:latest  # Forzar ARMv7
```

### `docker buildx` para multi-platform

```bash
# Instalar/activar buildx (viene con Docker Desktop)
docker buildx install

# Crear un builder multi-platform
docker buildx create --name mybuilder --use

# Inspect una imagen multi-platform
docker buildx imagetools inspect nginx:latest
```

## Inspeccionar contenedores vs imágenes

`docker inspect` (sin `image` ni `container`) también funciona y busca en ambos.

Para evitar ambigüedad:

```bash
docker image inspect debian:bookworm    # Explícitamente imagen
docker container inspect my-container    # Explícitamente contenedor
docker inspect debian:bookworm          # Busca primero contenedores, luego imágenes
```

Para contenedores, campos adicionales útiles:

```bash
# Ver el estado del contenedor
docker container inspect my-container --format '{{.State.Status}}'
# running, exited, paused, restarting

# Ver el PID del proceso principal
docker container inspect my-container --format '{{.State.Pid}}'
# 12345

# Ver los mounts
docker container inspect my-container --format '{{json .Mounts}}' | python3 -m json.tool

# Ver la config de red
docker container inspect my-container --format '{{json .NetworkSettings}}' | python3 -m json.tool

# Ver el GraphDriver (capas del contenedor)
docker container inspect my-container --format '{{.GraphDriver.Data.LowerDir}}'
# /var/lib/docker/overlay2/.../diff
```

## Práctica

### Ejercicio 1: Inspeccionar una imagen oficial

```bash
# Descargar si no existe
docker pull nginx:latest

# Ver la configuración completa con jq
docker image inspect nginx:latest --format '{{json .}}' | jq '.Config'

# Extraer información específica
docker image inspect nginx:latest --format 'Entrypoint: {{.Config.Entrypoint}}
Cmd: {{.Config.Cmd}}
ExposedPorts: {{.Config.ExposedPorts}}
User: {{.Config.User}}
WorkingDir: {{.Config.WorkingDir}}
'

# Ver todas las variables de entorno una por línea
docker image inspect nginx:latest --format '{{range .Config.Env}}{{.}}
{{end}}'

# Preguntas a responder:
# 1. ¿Cuál es el ENTRYPOINT? ¿Y el CMD?
# 2. ¿Qué variables de entorno están definidas?
# 3. ¿Qué puertos están expuestos?
# 4. ¿Cuántas capas tiene?
# 5. ¿Qué usuario ejecuta el proceso?
# 6. ¿Cuál es el StopSignal?
```

### Ejercicio 2: Comparar history de base vs derivada

```bash
# Ver las capas de debian
docker history debian:bookworm

# Ver las capas de python (derivada de debian)
docker history python:3.12-bookworm

# Contar capas
docker history debian:bookworm | wc -l
docker history python:3.12-bookworm | wc -l

# Ver el tamaño total acumulado de python
docker history python:3.12-bookworm | tail -n +2 | awk '{sum+=$4} END {print sum}'

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

# Sumar los tamaños virtuales
# docker images no muestra en formato fácil para summing, pero puedes estimar

# Ver espacio real
docker system df -v 2>/dev/null | grep -A 20 "Images space usage"

# Calcular cuánto espacio se comparte
# SHARED SIZE de python = capas de debian
# SHARED SIZE de node = capas de debian
# Espacio real ≈ debian + unique de python + unique de node
```

### Ejercicio 4: Entender `<missing>` en history

```bash
docker history python:3.12-bookworm

# Observar que muchas capas muestran <missing> en IMAGE ID
# Esto es normal para capas descargadas remotamente

# Construir una imagen local y ver la diferencia
docker build -t test-local - << 'EOF'
FROM debian:bookworm
RUN echo "hello"
EOF

docker history test-local
# La capa de "echo hello" sí tiene un IMAGE ID (fue construida localmente)
# La capa de debian muestra <missing> (fue descargada)

docker rmi test-local
```

### Ejercicio 5: Comparar imágenes con buildx

```bash
# Ver qué plataformas soporta nginx:latest
docker buildx imagetools inspect nginx:latest 2>/dev/null || docker manifest inspect nginx:latest

# Ver plataformas soportadas por debian
docker buildx imagetools inspect debian:bookworm 2>/dev/null || docker manifest inspect debian:bookworm

# Forzar descarga de una arquitectura específica
docker pull --platform linux/arm64/v7 debian:bookworm
docker image inspect debian:bookworm --format '{{.Architecture}}'
```

### Ejercicio 6: Inspeccionar un contenedor activo

```bash
# Iniciar un contenedor
docker run -d --name inspect-test nginx:latest

# Ver estado
docker container inspect inspect-test --format '{{.State.Status}}'

# Ver PID
docker container inspect inspect-test --format 'PID: {{.State.Pid}}'

# Ver mounts
docker container inspect inspect-test --format '{{json .Mounts}}' | jq '.[].Source, .[].Destination'

# Ver las capas (lowerdir = imagen, upperdir = cambios del contenedor)
docker container inspect inspect-test --format 'Lower: {{.GraphDriver.Data.LowerDir}}
Upper: {{.GraphDriver.Data.UpperDir}}'

# Ver networks
docker container inspect inspect-test --format '{{range $k, $v := .NetworkSettings.Networks}}{{$k}}: IP={{$v.IPAddress}} {{end}}'

# Limpiar
docker stop inspect-test && docker rm inspect-test
```

### Ejercicio 7: Usar jq para consultas avanzadas

```bash
# Requiere jq instalado: apt install jq

# Encontrar todas las imágenes con más de 10 capas
docker images --format '{{json .}}' | jq -s 'map(select(.Repository + ":" + .Tag | contains("debian") | not)) | .[] | select((.Size | gsub("[^0-9]";"") | tonumber) > 1000000000)'

# Ver todos los tags de debian
docker image inspect debian:bookworm --format '{{json .RepoTags}}' | jq '.'

# Ver todos los RepoDigests
docker image inspect debian:bookworm --format '{{json .RepoDigests}}' | jq '.'

# Filtrar imágenes por arquitectura
docker images --format '{{json .}}' | jq -r 'select(.Repository | startswith("python")) | .Repository + ":" + .Tag + " - " + .Architecture'
```

### Ejercicio 8: Inspección programática para scripts

```bash
# Script para verificar si una imagen existe localmente
image_exists() {
    docker image inspect "$1" > /dev/null 2>&1
}
image_exists "debian:bookworm" && echo "existe" || echo "no existe"

# Script para obtener el tamaño en MB
image_size_mb() {
    docker image inspect "$1" --format '{{.Size}}' | awk '{printf "%.1f", $1/1048576}'
}
echo "debian:bookworm ocupa $(image_size_mb debian:bookworm) MB"

# Script para listar imágenes con sus Architectures
docker images --format '{{.Repository}}:{{.Tag}}' | while read img; do
    arch=$(docker image inspect "$img" --format '{{.Architecture}}' 2>/dev/null)
    echo "$img → $arch"
done
```
