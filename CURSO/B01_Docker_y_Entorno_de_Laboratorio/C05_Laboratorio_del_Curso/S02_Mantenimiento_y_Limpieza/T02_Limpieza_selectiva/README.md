# T02 — Limpieza selectiva

## Objetivo

Aprender a limpiar recursos de Docker de forma granular: imágenes, contenedores,
volúmenes, build cache y redes, eligiendo exactamente qué eliminar.

## Principio: limpiar por tipo

Docker ofrece un comando `prune` para cada tipo de recurso. Cada uno tiene su
nivel de riesgo y sus implicaciones:

```
Riesgo bajo  ─── docker container prune     (contenedores detenidos)
             ─── docker network prune        (redes no usadas)
             ─── docker image prune          (imágenes dangling)
             ─── docker builder prune        (build cache)
Riesgo alto  ─── docker volume prune         (datos persistentes)
```

Todos los comandos `prune` piden confirmación antes de ejecutar, excepto si se
usa `-f` (force).

## Contenedores: `docker container prune`

Elimina todos los contenedores en estado `exited` o `dead`:

```bash
docker container prune
```

```
WARNING! This will remove all stopped containers.
Are you sure you want to continue? [y/N] y
Deleted Containers:
abc123def456
789012ghi345
Total reclaimed space: 95MB
```

### Cuándo usar

Después de sesiones de desarrollo con muchos `docker run` sin `--rm`. Cada
contenedor detenido mantiene su capa de escritura y sus logs en disco.

### Filtros

```bash
# Solo contenedores detenidos hace más de 24 horas
docker container prune --filter "until=24h"

# Solo contenedores con una etiqueta específica
docker container prune --filter "label=test"
```

### Alternativa manual

```bash
# Ver qué se eliminaría
docker ps -a --filter status=exited --format 'table {{.Names}}\t{{.Status}}\t{{.Size}}'

# Eliminar uno específico
docker rm container_name

# Eliminar varios
docker rm $(docker ps -aq --filter status=exited)
```

## Imágenes: `docker image prune`

### Solo dangling (por defecto)

```bash
docker image prune
```

Elimina imágenes `<none>:<none>` — imágenes que perdieron su tag porque se
hizo un rebuild con el mismo tag:

```
Build 1: docker build -t myapp .   → myapp:latest (abc123)
Build 2: docker build -t myapp .   → myapp:latest (def456)
                                     <none>:<none> (abc123) ← dangling
```

### Todas las no usadas

```bash
docker image prune -a
```

Elimina todas las imágenes que no tienen ningún contenedor (activo o detenido)
asociado. Esto incluye imágenes base descargadas con `pull` que no se están
usando.

### Filtros

```bash
# Imágenes dangling creadas hace más de 48 horas
docker image prune --filter "until=48h"

# Imágenes sin una etiqueta específica
docker image prune -a --filter "label!=keep"
```

### Alternativa manual

```bash
# Ver imágenes dangling
docker image ls --filter dangling=true

# Eliminar una imagen específica
docker image rm image_id

# Eliminar por patrón
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep "old-project" | xargs docker image rm
```

## Volúmenes: `docker volume prune`

```bash
docker volume prune
```

Elimina volúmenes no referenciados por ningún contenedor (ni activo ni detenido).

### Por qué es peligroso

Los volúmenes contienen **datos persistentes** — bases de datos, archivos de
configuración, datos de usuario. Un volumen huérfano puede contener datos
importantes de un contenedor que se eliminó pero cuyos datos se pretendía
conservar:

```
Escenario peligroso:

1. docker run -v dbdata:/var/lib/mysql mysql       ← contenedor + volumen
2. docker rm mysql_container                        ← elimina contenedor
3. docker volume prune                              ← ELIMINA dbdata !!!
   (el volumen no tiene contenedor asociado)
```

### Verificar antes de limpiar

```bash
# SIEMPRE listar volúmenes antes de prune
docker volume ls

# Ver cuáles son huérfanos
docker volume ls --filter dangling=true

# Inspeccionar un volumen antes de eliminarlo
docker volume inspect volume_name
```

```json
[
    {
        "CreatedAt": "2024-01-15T10:30:00Z",
        "Driver": "local",
        "Labels": {
            "com.docker.compose.project": "lab",
            "com.docker.compose.volume": "workspace"
        },
        "Mountpoint": "/var/lib/docker/volumes/lab_workspace/_data",
        "Name": "lab_workspace"
    }
]
```

Las labels de Compose ayudan a identificar a qué proyecto pertenece el volumen.

### Volúmenes anónimos

```bash
# -a incluye volúmenes anónimos (los de hash largo)
docker volume prune -a
```

Los volúmenes anónimos (creados por `VOLUME` en Dockerfile o `-v` sin nombre)
suelen ser seguros de eliminar. Los volúmenes nombrados requieren verificación.

## Build cache: `docker builder prune`

```bash
docker builder prune
```

Elimina capas de build cacheadas que no están referenciadas por ninguna imagen
existente.

### Opciones

```bash
# Eliminar TODO el cache (incluyendo el referenciado)
docker builder prune -a

# Mantener al menos 5GB de cache (eliminar el exceso)
docker builder prune --keep-storage 5GB

# Cache de más de 7 días
docker builder prune --filter "until=168h"
```

### Impacto

Sin build cache, el siguiente `docker build` ejecutará todas las instrucciones
desde cero — los `apt-get install`, `dnf install`, `pip install`, etc. vuelven
a descargar todo:

```
Con cache:     docker compose build   → 5 segundos
Sin cache:     docker compose build   → 5-15 minutos
```

`--keep-storage` es un buen compromiso: libera espacio pero mantiene las capas
más recientes.

## Redes: `docker network prune`

```bash
docker network prune
```

Elimina redes custom que no están siendo usadas por ningún contenedor. Las redes
predefinidas (`bridge`, `host`, `none`) nunca se eliminan.

Esta es la operación de prune con menos riesgo — las redes no contienen datos y
se recrean automáticamente con `docker compose up`.

## Resumen de comandos

| Comando | Qué elimina | Riesgo | Recuperable |
|---|---|---|---|
| `docker container prune` | Contenedores detenidos | Bajo | No (capa de escritura) |
| `docker image prune` | Imágenes dangling | Bajo | Sí (rebuild/pull) |
| `docker image prune -a` | Imágenes no usadas | Medio | Sí (rebuild/pull, tarda) |
| `docker volume prune` | Volúmenes huérfanos | **Alto** | **No** (datos perdidos) |
| `docker builder prune` | Build cache | Bajo | Sí (se regenera al build) |
| `docker network prune` | Redes no usadas | Mínimo | Sí (se recrean) |

---

## Ejercicios

### Ejercicio 1 — Crear y limpiar contenedores

```bash
# Crear contenedores efímeros
for i in 1 2 3 4 5; do
    docker run --name "test_$i" alpine echo "test $i"
done

# Ver los contenedores detenidos
docker ps -a --filter status=exited

# Limpiar
docker container prune

# Verificar
docker ps -a
```

### Ejercicio 2 — Generar y limpiar imágenes dangling

```bash
# Crear un Dockerfile simple
cat > /tmp/test-prune/Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "version 1" > /version.txt
EOF

mkdir -p /tmp/test-prune

# Build 1
docker build -t prune-test /tmp/test-prune

# Modificar y rebuild (genera dangling)
sed -i 's/version 1/version 2/' /tmp/test-prune/Dockerfile
docker build -t prune-test /tmp/test-prune

# Ver la imagen dangling
docker image ls --filter dangling=true

# Limpiar
docker image prune

# Verificar
docker image ls --filter dangling=true
# (vacío)

# Limpiar la imagen de test
docker image rm prune-test
rm -rf /tmp/test-prune
```

### Ejercicio 3 — Práctica segura de volume prune

```bash
# Crear volúmenes de prueba
docker volume create test_safe
docker volume create test_important

# Asociar uno a un contenedor
docker run -d --name vol_user -v test_important:/data alpine sleep 300

# Verificar: ¿cuál es huérfano?
docker volume ls --filter dangling=true
# Solo test_safe (test_important está en uso)

# Prune elimina solo el huérfano
docker volume prune
# Elimina test_safe, no test_important

# Limpiar
docker rm -f vol_user
docker volume rm test_important
```
