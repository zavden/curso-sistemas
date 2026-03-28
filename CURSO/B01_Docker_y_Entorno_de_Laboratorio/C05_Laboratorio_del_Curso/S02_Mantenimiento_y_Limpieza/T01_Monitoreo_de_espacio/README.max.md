# T01 — Monitoreo de espacio

## Objetivo

Aprender a medir cuánto espacio consume Docker en el sistema, identificar qué
recursos ocupan más disco, y detectar elementos que se pueden limpiar de forma
segura.

---

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

---

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

| Campo | Significado |
|---|---|
| SHARED SIZE | Capas compartidas entre imágenes (cuentan una vez) |
| UNIQUE SIZE | Capas únicas de esta imagen |
| `<none>:<none>` | Imágenes "dangling" de rebuilds |
| LINKS = 0 | Volúmenes huérfanos sin contenedor |
| LAST USED | Cuándo se usó el cache por última vez |

---

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

### Tabla de subdirectorios

| Directorio | Contenido | Crecimiento |
|---|---|---|
| `overlay2/` | Capas de imágenes y containers | Con cada `docker build` |
| `volumes/` | Volúmenes persistentes | Con datos de aplicaciones |
| `buildkit/` | Build cache | Con cada build |
| `containers/` | Logs, config, tmp | Con logs de contenedores |
| `image/` | Metadata de imágenes | Con cada imagen |

---

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

---

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

### Por qué importan los tamaños

| Comando | Usa tamaño... | Para qué es útil |
|---|---|---|
| `docker system df` | Real (con sharing) | Planificar limpieza |
| `docker image ls` | Virtual (por imagen) | Ver cuánto "pesa" cada imagen |
| `du -sh /var/lib/docker/` | Real en disco | Liberar espacio real |

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

**Criterios de análisis:**
- Si `IMAGES` tiene alto `RECLAIMABLE`: muchas imágenes dangling
- Si `Containers` tiene alto `RECLAIMABLE`: muchos contenedores detenidos
- Si `Build Cache` tiene alto `RECLAIMABLE`: cache de builds no utilizado

---

### Ejercicio 2 — Encontrar recursos eliminables

```bash
# Imágenes dangling
docker image ls --filter dangling=true

# Contenedores detenidos
docker ps -a --filter status=exited

# Volúmenes huérfanos
docker volume ls --filter dangling=true

# Comparar con el campo RECLAIMABLE de docker system df
docker system df
```

**Análisis:**
```bash
# Cuántos recursos dangling hay?
echo "Dangling images: $(docker image ls -q --filter dangling=true | wc -l)"
echo "Stopped containers: $(docker ps -aq --filter status=exited | wc -l)"
echo "Dangling volumes: $(docker volume ls -q --filter dangling=true | wc -l)"
```

---

### Ejercicio 3 — Comparar espacio reportado vs real

```bash
# Espacio reportado por Docker
docker system df

# Espacio real en el host
sudo du -sh /var/lib/docker/

# Desglose por subdirectorio
sudo du -sh /var/lib/docker/*/

# ¿Coinciden los números? ¿Por qué podrían diferir?
```

**Diferencias comunes:**
- `docker system df` no incluye logs de contenedores activos
- `du` cuenta bloques de 4KB mínimos por archivo
- Metadatos del filesystem (inodes)

---

### Ejercicio 4 — Identificar imágenes que comparten capas

```bash
# Ver tamaño compartido vs único
docker system df -v 2>/dev/null | grep -A 50 "Images space"

# Identificar imágenes base que comparten capas
docker image ls --format '{{.Repository}}:{{.Tag}}\t{{.Size}}' | sort -k2 -h
```

**Patrón a buscar:**
- Múltiples imágenes con el mismo `SHARED SIZE` → comparten imagen base
- `debian:bookworm` o `ubuntu:22.04` aparecen como base compartida

---

### Ejercicio 5 — Analizar crecimiento de build cache

```bash
# Ver cache de builds
docker builder du

# Ver cache con más detalle
docker builder du --verbose 2>/dev/null | head -100

# ¿Cuándo se usó el cache por última vez?
docker builder du | grep -E "TOTAL|Last Used"
```

---

### Ejercicio 6 — Encontrar contenedores con mucho espacio

```bash
# Ver contenedores más pesados
docker ps -a --format '{{.Names}}\t{{.Size}}' | sort -k2 -hr | head

# Si el tamaño no aparece, usar docker inspect
for c in $(docker ps -aq); do
    size=$(docker inspect --format='{{.SizeRw}}' $c 2>/dev/null || echo "0")
    name=$(docker inspect --format='{{.Name}}' $c)
    echo "$name: ${size}KB"
done | sort -k2 -rn | head
```

---

### Ejercicio 7 — Ver logs que consumen espacio

```bash
# Espacio de logs
sudo du -sh /var/lib/docker/containers/*/

# Logs más grandes
sudo find /var/lib/docker/containers -name "*.log" -exec ls -lh {} \; | sort -k5 -rh | head

# Ver tamaño de logs de un contenedor específico
docker inspect --format='{{.LogPath}}' debian-dev | xargs ls -lh
```

---

### Ejercicio 8 — Verificar espacio antes y después de builds

```bash
# Estado inicial
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}'

# Hacer un build grande
docker build -t test-build - << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y linux-headers-amd64
EOF

# Estado después
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}'
```

---

### Ejercicio 9 — Limpiar y comparar

```bash
# Guardar estado inicial
docker system df > /tmp/before.txt

# Hacer limpieza básica
docker system prune -f

# Guardar estado final
docker system df > /tmp/after.txt

# Comparar
diff /tmp/before.txt /tmp/after.txt
```

---

### Ejercicio 10 — Calcular espacio salvable

```bash
# Script para calcular espacio recuperable
#!/bin/bash
echo "=== Espacio recuperable ==="

# Imágenes dangling
DANGLING=$(docker image ls -q --filter dangling=true | wc -l)
echo "Imágenes dangling: $DANGLING"

# Contenedores detenidos
STOPPED=$(docker ps -aq --filter status=exited | wc -l)
echo "Contenedores detenidos: $STOPPED"

# Volúmenes huérfanos
ORPHAN=$(docker volume ls -q --filter dangling=true | wc -l)
echo "Volúmenes huérfanos: $ORPHAN"

# Build cache
CACHE=$(docker builder du 2>/dev/null | grep "Cache" | awk '{print $1}')
echo "Build cache: $CACHE"

echo ""
echo "Total recursos: $((DANGLING + STOPPED + ORPHAN))"
```

---

## Comandos útiles de monitoreo

```bash
# Ver uso de disco en formato humano
docker system df -h

# Ver solo imágenes
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}\t{{.CreatedSince}}"

# Ver solo volúmenes
docker volume ls --format "table {{.Name}}\t{{.Driver}}\t{{.Mountpoint}}"

# Ver contenedores ordenados por fecha
docker ps -a --format "table {{.Names}}\t{{.Status}}\t{{.CreatedAt}}" | sort -k3 -r
```
