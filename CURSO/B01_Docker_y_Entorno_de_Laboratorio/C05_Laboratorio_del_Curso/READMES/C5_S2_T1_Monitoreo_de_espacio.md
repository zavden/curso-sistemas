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
| ACTIVE | Recursos en uso (contenedores corriendo, imágenes referenciadas por contenedores, volúmenes montados) |
| SIZE | Espacio total en disco (con layer sharing contabilizado) |
| RECLAIMABLE | Espacio que se puede liberar de recursos no activos |

> **Nota sobre RECLAIMABLE:** Para imágenes, ACTIVE cuenta las imágenes usadas
> por contenedores **corriendo**. Las imágenes usadas solo por contenedores
> detenidos se consideran no activas y su espacio aparece como reclaimable.
> Para poder reclamarlo, primero hay que eliminar esos contenedores detenidos.

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
| SHARED SIZE | Capas compartidas entre imágenes (se almacenan una sola vez en disco) |
| UNIQUE SIZE | Capas exclusivas de esta imagen |
| `<none>:<none>` | Imágenes "dangling" — resultado de rebuilds que reemplazan el tag anterior |
| LINKS = 0 | Volúmenes huérfanos sin ningún contenedor asociado |
| CONTAINERS = 0 | Imágenes sin ningún contenedor (candidatas a limpieza con `prune -a`) |
| LAST USED | Cuándo se usó el build cache por última vez |

---

## Espacio en disco del host

`docker system df` muestra el espacio desde la perspectiva de Docker. Para ver
el espacio real en el filesystem del host:

```bash
# Docker rootful (instalación estándar)
sudo du -sh /var/lib/docker/

# Docker rootless
du -sh ~/.local/share/docker/

# Podman rootless
du -sh ~/.local/share/containers/
```

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

| Directorio | Contenido | Crece con... |
|---|---|---|
| `overlay2/` | Capas de imágenes y escritura de containers | Cada `docker build` y `docker run` |
| `volumes/` | Datos de volúmenes persistentes | Datos de aplicaciones |
| `buildkit/` | Cache de builds (capas intermedias) | Cada build |
| `containers/` | Logs JSON, config, archivos temporales | Logs de contenedores en ejecución |
| `image/` | Metadata de imágenes (manifests, config) | Cada imagen descargada o construida |

> **¿Por qué `du` y `docker system df` pueden diferir?**
> `docker system df` contabiliza layer sharing y muestra tamaños lógicos.
> `du` cuenta bloques de filesystem reales (mínimo 4KB por archivo). Además,
> `du` incluye metadata, logs de contenedores activos y archivos temporales
> que Docker no reporta en `system df`.

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

Estas imágenes se crean cuando haces `docker build` con un tag que ya existe:
la imagen anterior pierde su tag y queda como `<none>:<none>`. Son las
candidatas más seguras para limpieza — nadie las referencia.

### Imágenes no usadas

```bash
# Imágenes usadas por contenedores activos
docker ps --format '{{.Image}}' | sort -u

# Imágenes usadas por TODOS los contenedores (incluyendo detenidos)
docker ps -a --format '{{.Image}}' | sort -u

# Todas las imágenes con su tamaño
docker image ls --format '{{.Repository}}:{{.Tag}}\t{{.Size}}'
```

Las imágenes que aparecen en `docker image ls` pero NO en `docker ps -a` son
candidatas a limpieza con `docker image prune -a`.

### Contenedores detenidos

```bash
docker ps -a --filter status=exited --format 'table {{.Names}}\t{{.Status}}\t{{.Size}}'
```

Cada contenedor detenido mantiene su capa de escritura (overlay) en disco.
Contenedores con mucha actividad de escritura pueden ocupar cientos de MB.

> **Nota:** Para que la columna SIZE muestre valores, necesitas `docker ps -s`
> (con `-s`). Sin este flag, el campo puede aparecer vacío.

### Volúmenes huérfanos

```bash
docker volume ls --filter dangling=true
```

```
DRIVER    VOLUME NAME
local     3a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b
local     old_project_data
```

Los volúmenes anónimos (hash largo) suelen ser seguros de eliminar — se crean
automáticamente con `docker run -v /data` sin nombre. Los volúmenes nombrados
requieren verificación antes de eliminar.

### Build cache

```bash
docker builder du
```

Muestra el espacio usado por el cache de BuildKit con desglose por tipo.
El build cache se acumula con cada `docker build` y nunca se limpia
automáticamente.

---

## Diferencia entre tamaño virtual y real

```bash
docker image ls
```

```
REPOSITORY    TAG       SIZE
debian-dev    latest    720MB
alma-dev      latest    750MB
debian        bookworm  120MB
```

La suma directa da 1590MB, pero el espacio real es menor porque `debian-dev`
comparte capas con `debian:bookworm` (su imagen base):

```
debian:bookworm    = 120MB (capas base)
debian-dev         = 120MB (compartidas) + 600MB (únicas) = 720MB virtual
alma-dev           = 750MB (no comparte con debian)

Espacio real total = 120MB + 600MB + 750MB = 1470MB (no 1590MB)
```

| Comando | Muestra | Útil para... |
|---|---|---|
| `docker system df` | Espacio real (con sharing contabilizado) | Planificar limpieza |
| `docker image ls` | Tamaño virtual (por imagen) | Ver cuánto "pesa" cada imagen individual |
| `docker system df -v` | Desglose SHARED/UNIQUE por imagen | Entender qué capas se comparten |
| `du -sh /var/lib/docker/` | Espacio real en filesystem | Verificar impacto en disco del host |

---

## Comandos útiles de monitoreo

```bash
# Resumen de espacio Docker
docker system df

# Desglose completo
docker system df -v

# Solo imágenes con tamaño y antigüedad
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}\t{{.CreatedSince}}"

# Solo volúmenes
docker volume ls --format "table {{.Name}}\t{{.Driver}}"

# Contenedores con tamaño real (flag -s)
docker ps -a -s --format "table {{.Names}}\t{{.Status}}\t{{.Size}}"

# Tamaño de un contenedor específico
docker inspect --size debian-dev --format '{{.SizeRw}} bytes (writable layer)'
```

> **Importante sobre `--size` en inspect:** El campo `SizeRw` (tamaño de la
> capa de escritura) solo se popula cuando pasas el flag `--size` a
> `docker inspect`. Sin este flag, el campo devuelve `nil` o 0.

---

## Ejercicios

### Ejercicio 1 — Auditar el espacio actual

Usa `docker system df` para obtener una foto del estado actual:

```bash
# Resumen
docker system df

# Desglose completo
docker system df -v
```

<details><summary>Predicción</summary>

`docker system df` muestra cuatro categorías:

```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          X         Y         ...       ... (XX%)
Containers      X         Y         ...       ... (XX%)
Local Volumes   X         Y         ...       ... (XX%)
Build Cache     X         Y         ...       ... (XX%)
```

Si tienes el lab del curso corriendo (debian-dev + alma-dev), verás:
- **Images**: al menos 3 (debian:bookworm, lab-debian-dev, lab-alma-dev), con
  ~1.5GB de SIZE. Si has hecho rebuilds, habrá imágenes dangling y
  RECLAIMABLE será mayor.
- **Containers**: 2 ACTIVE (los contenedores del lab corriendo).
- **Local Volumes**: al menos 1 (workspace).
- **Build Cache**: variable — crece con cada `docker compose build`.

El modo `-v` desglosa cada recurso individualmente. Busca:
- Imágenes `<none>:<none>` → dangling, limpiar
- Volúmenes con LINKS = 0 → huérfanos, verificar antes de limpiar
- Build cache con LAST USED antiguo → candidato a limpieza

</details>

---

### Ejercicio 2 — Crear recursos descartables y observar crecimiento

Crea imágenes dangling, contenedores detenidos y volúmenes huérfanos para
practicar la identificación:

```bash
# Estado inicial
docker system df

# Crear imágenes dangling (mismo tag, dos builds)
docker build -t lab-space-v1 --build-arg VERSION=1 -f labs/Dockerfile.space labs/
docker build -t lab-space-v1 --build-arg VERSION=2 -f labs/Dockerfile.space labs/

# Crear contenedores detenidos
docker run --name lab-space-exit1 alpine echo "test 1"
docker run --name lab-space-exit2 alpine echo "test 2"

# Crear volumen huérfano
docker volume create lab-space-orphan

# Estado después
docker system df
```

<details><summary>Predicción</summary>

Después de crear los recursos:

**Imágenes dangling:**
```bash
docker image ls --filter dangling=true
# REPOSITORY   TAG       IMAGE ID       CREATED          SIZE
# <none>       <none>    <hash>         <time>           ~10MB
```
La primera imagen (VERSION=1) perdió su tag cuando el segundo build usó el
mismo tag `lab-space-v1`. Ahora es dangling.

**Contenedores detenidos:**
```bash
docker ps -a --filter status=exited --filter name=lab-space
# lab-space-exit1   Exited (0)   ...
# lab-space-exit2   Exited (0)   ...
```

**Volumen huérfano:**
```bash
docker volume ls --filter dangling=true
# DRIVER    VOLUME NAME
# local     lab-space-orphan
```

En `docker system df`, RECLAIMABLE habrá crecido en las tres categorías:
Images, Containers, y Local Volumes.

</details>

---

### Ejercicio 3 — Identificar dangling, detenidos y huérfanos

Usa los filtros de Docker para encontrar recursos eliminables:

```bash
# Imágenes dangling
docker image ls --filter dangling=true

# Contenedores detenidos
docker ps -a --filter status=exited

# Volúmenes huérfanos
docker volume ls --filter dangling=true

# Resumen numérico
echo "Dangling images:    $(docker image ls -q --filter dangling=true | wc -l)"
echo "Stopped containers: $(docker ps -aq --filter status=exited | wc -l)"
echo "Dangling volumes:   $(docker volume ls -q --filter dangling=true | wc -l)"

# Comparar con RECLAIMABLE
docker system df
```

<details><summary>Predicción</summary>

Los tres comandos de filtro muestran exactamente los recursos que contribuyen
al campo RECLAIMABLE en `docker system df`.

Relación entre filtros y RECLAIMABLE:
- **Images RECLAIMABLE** = espacio de imágenes dangling + imágenes sin
  contenedores asociados
- **Containers RECLAIMABLE** = espacio de contenedores no corriendo (detenidos,
  creados)
- **Local Volumes RECLAIMABLE** = espacio de volúmenes sin contenedores
  asociados (LINKS = 0)

El resumen numérico muestra la cantidad de cada tipo. Si hiciste el
ejercicio 2, verás al menos: 1 dangling image, 2 stopped containers,
1 dangling volume.

</details>

---

### Ejercicio 4 — Entender tamaño virtual vs real

Construye dos imágenes que comparten capas y observa la diferencia:

```bash
# Construir dos versiones (comparten la capa base de alpine)
docker build -t lab-space-v1 --build-arg VERSION=1 -f labs/Dockerfile.space labs/
docker build -t lab-space-v2 --build-arg VERSION=2 -f labs/Dockerfile.space labs/

# Tamaño virtual (por imagen)
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-space

# Tamaño real (con sharing)
docker system df -v 2>&1 | head -20

# ¿Cuánto espacio REAL usan las dos imágenes juntas?
```

<details><summary>Predicción</summary>

`docker image ls` muestra ~10MB para cada imagen:
```
lab-space-v1   latest    ~10MB
lab-space-v2   latest    ~10MB
```

La suma virtual sería ~20MB. Pero `docker system df -v` muestra que comparten
la capa base de `alpine`:

```
REPOSITORY    TAG     SIZE    SHARED SIZE   UNIQUE SIZE
lab-space-v1  latest  10MB    7MB           3MB
lab-space-v2  latest  10MB    7MB           3MB
```

El espacio real es: 7MB (compartido, almacenado una vez) + 3MB (único v1) +
3MB (único v2) = **13MB** (no 20MB).

La diferencia viene de la capa `alpine:latest` (~7MB) que se almacena una
sola vez en disco pero se contabiliza en el tamaño virtual de cada imagen
que la usa como base.

</details>

---

### Ejercicio 5 — Espacio reportado vs espacio en host

Compara lo que Docker reporta con lo que realmente hay en disco:

```bash
# Espacio reportado por Docker
docker system df

# Espacio real en el host (necesita sudo para rootful)
sudo du -sh /var/lib/docker/ 2>/dev/null || \
    du -sh ~/.local/share/docker/ 2>/dev/null || \
    echo "Verificar path de Docker data root"

# Desglose por subdirectorio
sudo du -sh /var/lib/docker/*/ 2>/dev/null

# ¿Dónde está la configuración de data-root?
docker info --format '{{.DockerRootDir}}'
```

<details><summary>Predicción</summary>

`docker system df` y `du` muestran números similares pero no idénticos:

```
docker system df:  SIZE total ≈ 2.5GB (ejemplo)
du -sh:            /var/lib/docker/ ≈ 3.0GB
```

La diferencia viene de:
1. **Logs de contenedores** — `du` los cuenta, `docker system df` no los
   muestra como categoría separada
2. **Metadata** — manifests, configs, índices de imágenes
3. **Block size** — `du` cuenta en bloques de 4KB mínimo
4. **Layer deduplication** — `docker system df` contabiliza sharing,
   `du` ve los archivos tal como están en overlay2

`docker info --format '{{.DockerRootDir}}'` confirma el directorio raíz
de Docker (por defecto `/var/lib/docker`). Si usas Docker rootless, será
`~/.local/share/docker/`.

El subdirectorio más grande suele ser `overlay2/` (capas de imágenes).

</details>

---

### Ejercicio 6 — Analizar el build cache

```bash
# Ver espacio del build cache
docker builder du

# Con más detalle (si tu versión lo soporta)
docker builder du --verbose 2>/dev/null | head -50

# Ver build cache en docker system df
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}' | grep "Build"

# Hacer un build y ver el impacto
docker build -t lab-space-test -f labs/Dockerfile.space labs/
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}' | grep "Build"
```

<details><summary>Predicción</summary>

`docker builder du` muestra el cache de BuildKit:
```
ID           RECLAIMABLE  SIZE         LAST ACCESSED
abc123       true         200MB        2 hours ago
...
Total:       800MB
Reclaimable: 800MB
```

El build cache crece con cada `docker build`. Cuando todas las entradas
muestran `RECLAIMABLE: true`, significa que ningún build activo las está
usando y se pueden limpiar con `docker builder prune`.

Después de hacer el build adicional, la fila de Build Cache en
`docker system df` habrá crecido ligeramente.

El build cache es a menudo el recurso más fácil de limpiar porque se puede
regenerar automáticamente en el siguiente build. El costo es solo tiempo de
rebuild.

</details>

---

### Ejercicio 7 — Logs que consumen espacio

Los logs de contenedores se acumulan en `/var/lib/docker/containers/`:

```bash
# Ver tamaño de logs por contenedor
sudo find /var/lib/docker/containers -name "*.log" \
    -exec ls -lh {} \; 2>/dev/null | sort -k5 -rh | head -5

# Ver el path de logs de un contenedor específico
docker inspect --format='{{.LogPath}}' debian-dev

# Ver el tamaño de ese log
docker inspect --format='{{.LogPath}}' debian-dev | xargs ls -lh

# Ver las últimas líneas del log
docker logs --tail 10 debian-dev
```

<details><summary>Predicción</summary>

Los logs se almacenan como archivos JSON en:
```
/var/lib/docker/containers/<container-id>/<container-id>-json.log
```

Los contenedores del lab (debian-dev, alma-dev) tienen logs relativamente
pequeños porque ejecutan `/bin/bash` sin mucha salida. Contenedores de
servicios web o aplicaciones con logging verbose pueden generar GB de logs.

Docker no limita el tamaño de logs por defecto. Para limitar:
```json
# /etc/docker/daemon.json
{
    "log-opts": {
        "max-size": "10m",
        "max-file": "3"
    }
}
```

Esto rota los logs a 10MB con máximo 3 archivos (30MB total por contenedor).

</details>

---

### Ejercicio 8 — Ver impacto de un build grande

```bash
# Estado antes
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}'

# Hacer un build que instala paquetes
docker build -t lab-space-big - << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc g++ make cmake && rm -rf /var/lib/apt/lists/*
EOF

# Estado después
docker system df --format '{{.Type}}\t{{.Size}}\t{{.Reclaimable}}'

# Ver la nueva imagen
docker image ls lab-space-big

# Limpiar
docker rmi lab-space-big
```

<details><summary>Predicción</summary>

Antes del build, la fila de Images tiene un SIZE determinado. Después:
- **Images SIZE** crece en ~300-400MB (el toolchain de C/C++)
- **Build Cache SIZE** crece con las capas intermedias del build
- **Images RECLAIMABLE** crece en la misma cantidad (la nueva imagen no
  tiene contenedores)

`docker image ls lab-space-big` muestra ~400MB. Pero si ya tienes
`debian:bookworm` descargada, la capa base es compartida y el espacio
real adicional es solo ~280MB (UNIQUE SIZE).

Después de `docker rmi`, la imagen se elimina y SIZE vuelve
aproximadamente al valor anterior. El build cache permanece (no se limpia
con `rmi`).

</details>

---

### Ejercicio 9 — Limpiar recursos de prueba y comparar

```bash
# Guardar estado antes de limpiar
docker system df > /tmp/before.txt
cat /tmp/before.txt

# Limpiar los recursos de prueba creados en estos ejercicios
docker rm -f lab-space-exit1 lab-space-exit2 2>/dev/null
docker rmi lab-space-v1 lab-space-v2 2>/dev/null
docker image prune -f
docker volume rm lab-space-orphan 2>/dev/null

# Estado después
docker system df > /tmp/after.txt
cat /tmp/after.txt

# Comparar
diff /tmp/before.txt /tmp/after.txt
```

<details><summary>Predicción</summary>

`diff` muestra que todos los campos disminuyeron:

```
< Images          X     Y     2.1GB     1.4GB (66%)
> Images          X-2   Y     1.9GB     1.2GB (63%)
```

Lo que se liberó:
- **Images**: imágenes dangling + lab-space-v1/v2 (~20MB)
- **Containers**: lab-space-exit1/exit2 (poca escritura, ~KB)
- **Local Volumes**: lab-space-orphan (vacío, ~0MB)

`docker image prune -f` elimina todas las imágenes dangling (sin tag) sin
pedir confirmación. Los contenedores se eliminan explícitamente con `docker rm`.

La limpieza más impactante suele ser el build cache (`docker builder prune`)
y las imágenes no usadas (`docker image prune -a`), pero estos no se usaron
aquí para no afectar el lab del curso.

</details>

---

### Ejercicio 10 — Script de auditoría rápida

Construye un script que resume el estado de espacio de Docker:

```bash
#!/bin/bash
echo "=== Auditoría de espacio Docker ==="
echo ""

# Resumen general
docker system df
echo ""

# Recursos eliminables
DANGLING=$(docker image ls -q --filter dangling=true | wc -l)
STOPPED=$(docker ps -aq --filter status=exited | wc -l)
ORPHAN=$(docker volume ls -q --filter dangling=true | wc -l)

echo "=== Recursos eliminables ==="
echo "  Imágenes dangling:     $DANGLING"
echo "  Contenedores detenidos: $STOPPED"
echo "  Volúmenes huérfanos:   $ORPHAN"
echo ""

# Sugerencias
if [ "$DANGLING" -gt 0 ]; then
    echo "  → Limpiar dangling: docker image prune -f"
fi
if [ "$STOPPED" -gt 0 ]; then
    echo "  → Limpiar detenidos: docker container prune -f"
fi
if [ "$ORPHAN" -gt 0 ]; then
    echo "  → Limpiar huérfanos: docker volume prune -f"
fi
echo "  → Limpiar todo no activo: docker system prune -f"
echo "  → Limpiar TODO (incluyendo imágenes): docker system prune -a -f"
```

<details><summary>Predicción</summary>

El script muestra primero `docker system df` (resumen general), luego cuenta
los recursos eliminables, y sugiere comandos de limpieza:

```
=== Auditoría de espacio Docker ===

TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          5         2         1.8GB     500MB (27%)
...

=== Recursos eliminables ===
  Imágenes dangling:     0
  Contenedores detenidos: 0
  Volúmenes huérfanos:   0

  → Limpiar todo no activo: docker system prune -f
  → Limpiar TODO (incluyendo imágenes): docker system prune -a -f
```

Si hiciste la limpieza del ejercicio 9, los contadores estarán en 0. Los
comandos sugeridos tienen diferentes niveles de agresividad:

| Comando | Qué elimina |
|---|---|
| `docker image prune -f` | Solo imágenes dangling (`<none>:<none>`) |
| `docker container prune -f` | Contenedores detenidos |
| `docker volume prune -f` | Volúmenes sin contenedores |
| `docker system prune -f` | Dangling + detenidos + redes sin uso |
| `docker system prune -a -f` | Todo lo anterior + imágenes no usadas |

**Importante:** `docker system prune -a` elimina TODAS las imágenes sin
contenedores asociados — incluyendo las del lab. Solo usar cuando quieras
hacer una limpieza total.

</details>
