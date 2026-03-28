# T01 — Monitoreo de espacio

## Objetivo

Aprender a medir cuánto espacio consume Docker en el sistema, identificar qué
recursos ocupan más disco, y detectar elementos que se pueden limpiar de forma
segura.

## El problema: Docker crece silenciosamente

Docker puede consumir decenas de GB sin que el usuario lo note. Cada imagen
descargada, cada build, cada contenedor detenido y cada volumen huérfano ocupa
espacio que no se libera automáticamente:

```
Sesión 1: pull imagen base                  →  200 MB
Sesión 2: build con dependencias            →  800 MB
Sesión 3: rebuild (nueva capa, vieja queda) → 1.5 GB
Sesión 4: más builds, contenedores efímeros → 3.0 GB
...
Mes 3: "¿por qué no tengo espacio en disco?"→ 25 GB
```

## `docker system df`

El comando principal para auditar el espacio de Docker:

```bash
docker system df
```

```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          8         3         2.1GB     1.4GB (66%)
Containers      5         2         120MB     95MB (79%)
Local Volumes   4         2         350MB     180MB (51%)
Build Cache     12        0         800MB     800MB (100%)
```

### Campos

| Campo | Significado |
|---|---|
| TOTAL | Cantidad total de recursos de ese tipo |
| ACTIVE | Recursos en uso (contenedores corriendo, imágenes referenciadas, volúmenes montados) |
| SIZE | Espacio total en disco |
| RECLAIMABLE | Espacio que se puede liberar sin afectar recursos activos |

### Modo verbose

```bash
docker system df -v
```

Muestra el desglose individual de cada recurso:

```
Images space usage:

REPOSITORY    TAG       IMAGE ID       CREATED        SIZE      SHARED SIZE   UNIQUE SIZE   CONTAINERS
debian-dev    latest    a1b2c3d4e5f6   2 hours ago    720MB     120MB         600MB         1
alma-dev      latest    f6e5d4c3b2a1   2 hours ago    750MB     0B            750MB         1
debian        bookworm  1234abcd5678   2 weeks ago    120MB     120MB         0B            0
<none>        <none>    9876fedc5432   3 hours ago    720MB     120MB         600MB         0

Containers space usage:

CONTAINER ID   IMAGE        COMMAND       LOCAL VOLUMES   SIZE      CREATED        STATUS    NAMES
abc123         debian-dev   "/bin/bash"   2               15MB      2 hours ago    Up        debian-dev
def456         alma-dev     "/bin/bash"   2               8MB       2 hours ago    Up        alma-dev

Local Volumes space usage:

VOLUME NAME   LINKS   SIZE
workspace     2       45MB
old_data      0       180MB     ← 0 links = huérfano, candidato a limpieza

Build cache usage:

CACHE ID       CACHE TYPE     SIZE      CREATED        LAST USED        USAGE   SHARED
abc123def      regular        200MB     3 hours ago    3 hours ago      1       false
```

### Lo que revela `-v`

- **SHARED SIZE vs UNIQUE SIZE**: capas compartidas entre imágenes se cuentan una
  sola vez en el total de `docker system df`, pero `docker images` muestra el
  tamaño virtual (incluyendo las compartidas) por cada imagen
- **`<none>:<none>`**: imágenes "dangling" — resultado de rebuilds que reemplazan
  el tag anterior
- **LINKS = 0**: volúmenes huérfanos sin ningún contenedor asociado
- **Build cache con LAST USED antiguo**: cache que probablemente no se reutilizará

## Espacio en disco del host

`docker system df` muestra el espacio desde la perspectiva de Docker. Para ver
el espacio real en el filesystem del host:

```bash
# Docker rootful (instalación estándar)
sudo du -sh /var/lib/docker/
# 4.2G    /var/lib/docker/

# Docker rootless
du -sh ~/.local/share/docker/

# Podman rootless
du -sh ~/.local/share/containers/
```

El directorio contiene todo: imágenes, capas, volúmenes, contenedores y metadata.

### Desglose por subdirectorio

```bash
sudo du -sh /var/lib/docker/*/
```

```
1.2G    /var/lib/docker/overlay2/     ← Capas de imágenes y contenedores
800M    /var/lib/docker/volumes/      ← Volúmenes nombrados
500M    /var/lib/docker/buildkit/     ← Cache de builds
200M    /var/lib/docker/containers/   ← Logs y metadata de contenedores
50M     /var/lib/docker/image/        ← Metadata de imágenes
```

## Identificar qué ocupa más espacio

### Imágenes sin tag (dangling)

```bash
docker image ls --filter dangling=true
```

```
REPOSITORY   TAG       IMAGE ID       CREATED        SIZE
<none>       <none>    9876fedc5432   3 hours ago    720MB
<none>       <none>    abcd1234ef56   1 day ago      680MB
```

Estas imágenes se crean cuando se hace `docker build` con un tag que ya existe:
la imagen anterior pierde su tag y queda como `<none>:<none>`.

### Imágenes no usadas

```bash
# Imágenes sin ningún contenedor (activo o detenido) que las use
docker image ls --filter dangling=false --format '{{.Repository}}:{{.Tag}}\t{{.Size}}'
```

Para identificar cuáles están en uso:

```bash
# Imágenes usadas por contenedores activos
docker ps --format '{{.Image}}' | sort -u

# Imágenes usadas por TODOS los contenedores (incluyendo detenidos)
docker ps -a --format '{{.Image}}' | sort -u
```

### Contenedores detenidos

```bash
docker ps -a --filter status=exited --format 'table {{.Names}}\t{{.Status}}\t{{.Size}}'
```

Cada contenedor detenido mantiene su capa de escritura en disco. Contenedores
con mucha actividad de escritura pueden ocupar cientos de MB.

### Volúmenes huérfanos

```bash
# Volúmenes no asociados a ningún contenedor
docker volume ls --filter dangling=true
```

```
DRIVER    VOLUME NAME
local     3a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b
local     old_project_data
```

Los volúmenes anónimos (hash largo) suelen ser seguros de eliminar. Los
volúmenes nombrados requieren verificación.

### Build cache

```bash
docker builder du
```

Muestra el espacio usado por el cache de BuildKit con desglose por tipo.

## Diferencia entre SIZE virtual y real

```bash
docker image ls
```

```
REPOSITORY    TAG       SIZE
debian-dev    latest    720MB
alma-dev      latest    750MB
debian        bookworm  120MB
```

La suma directa da 1590MB, pero el espacio real puede ser menor porque
`debian-dev` comparte capas con `debian:bookworm` (la imagen base):

```
debian:bookworm    = 120MB (capas base)
debian-dev         = 120MB (compartidas) + 600MB (únicas) = 720MB virtual
alma-dev           = 750MB (no comparte con debian)

Espacio real total = 120MB + 600MB + 750MB = 1470MB (no 1590MB)
```

`docker system df` reporta el espacio real. `docker image ls` muestra el tamaño
virtual por imagen.

---

## Ejercicios

### Ejercicio 1 — Auditar el espacio actual

```bash
# Ver resumen
docker system df

# Ver desglose completo
docker system df -v

# Identificar el recurso que más espacio consume
# ¿Es una imagen? ¿Un volumen? ¿Build cache?
```

### Ejercicio 2 — Encontrar recursos eliminables

```bash
# Imágenes dangling
docker image ls --filter dangling=true

# Contenedores detenidos
docker ps -a --filter status=exited

# Volúmenes huérfanos
docker volume ls --filter dangling=true

# Comparar con el campo RECLAIMABLE de docker system df
```

### Ejercicio 3 — Comparar espacio reportado vs real

```bash
# Espacio reportado por Docker
docker system df

# Espacio real en el host
sudo du -sh /var/lib/docker/

# Desglose por subdirectorio
sudo du -sh /var/lib/docker/*/

# ¿Coinciden los números? ¿Por qué podrían diferir?
# (logs de contenedores, metadata, archivos temporales)
```
