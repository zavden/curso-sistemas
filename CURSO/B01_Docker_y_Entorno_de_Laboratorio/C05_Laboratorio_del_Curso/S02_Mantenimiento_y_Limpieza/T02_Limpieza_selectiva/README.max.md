# T02 — Limpieza selectiva

## Objetivo

Aprender a limpiar recursos de Docker de forma granular: imágenes, contenedores,
volúmenes, build cache y redes, eligiendo exactamente qué eliminar.

---

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

### Tabla de riesgos

| Comando | Riesgo | Datos perdidos | Tiempo de recuperación |
|---|---|---|---|
| `docker container prune` | Bajo | Capas de escritura | Ninguno |
| `docker network prune` | Mínimo | Ninguno | Creación instantánea |
| `docker image prune` | Bajo | Imágenes | Rebuild/download |
| `docker image prune -a` | Medio | Imágenes no usadas | Rebuild/download |
| `docker volume prune` | **Alto** | **Datos persistentes** | **Irrecuperable** |
| `docker builder prune` | Bajo | Build cache | Rebuild lento |

---

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

# Combinar filtros
docker container prune --filter "until=168h" --filter "label!=keep"
```

### Estados de contenedores

| Estado | Significado | Se elimina con prune? |
|---|---|---|
| `running` | Activo | No |
| `exited` | Detenido normalmente | Sí |
| `dead` | Estado irrecuperable | Sí |
| `created` | Creado pero nunca iniciado | No* |
| `paused` | Pausado | No |

*`container prune` solo elimina `exited` y `dead`. Para eliminar contenedores
en otros estados, usar `docker rm -f`.

### Alternativa manual

```bash
# Ver qué se eliminaría
docker ps -a --filter status=exited --format 'table {{.Names}}\t{{.Status}}\t{{.Size}}'

# Eliminar uno específico
docker rm container_name

# Eliminar varios
docker rm $(docker ps -aq --filter status=exited)

# Eliminar incluyendo los pausados
docker rm $(docker ps -aq --filter status=exited)
docker rm -f $(docker ps -aq --filter status=paused)
```

---

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

# Imágenes de un repositorio específico
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep "^myapp:" | xargs docker image rm
```

### Alternativa manual

```bash
# Ver imágenes dangling
docker image ls --filter dangling=true

# Eliminar una imagen específica
docker image rm image_id

# Eliminar por patrón
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep "old-project" | xargs docker image rm

# Eliminar todas las dangling
docker image ls -q --filter dangling=true | xargs docker image rm
```

---

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

### Volúmenes vs Bind mounts

| Aspecto | Volumen | Bind mount |
|---|---|---|
| Creación | `docker volume create` o `VOLUME` en Dockerfile | `docker run -v /host/path:/container/path` |
| Nombre | Named o anónimo (hash) | Depende del path host |
| Localización | `/var/lib/docker/volumes/` | Cualquier path del host |
| Limpieza segura | Solo anónimos o huérfanos | Si el path ya no existe |

---

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

# Combinar opciones
docker builder prune --keep-storage 2GB --filter "until=72h"
```

### Impacto

Sin build cache, el siguiente `docker build` ejecutará todas las instrucciones
desde cero — los `apt-get install`, `dnf install`, `pip install`, etc. vuelven
a descargar todo:

| Escenario | Tiempo de docker compose build |
|---|---|---|
| Con cache | 5-30 segundos |
| Sin cache | 5-15 minutos |

`--keep-storage` es un buen compromiso: libera espacio pero mantiene las capas
más recientes.

---

## Redes: `docker network prune`

```bash
docker network prune
```

Elimina redes custom que no están siendo usadas por ningún contenedor. Las redes
predefinidas (`bridge`, `host`, `none`) nunca se eliminan.

Esta es la operación de prune con menos riesgo — las redes no contienen datos y
se recrean automáticamente con `docker compose up`.

### Tipos de redes

| Red | Tipo | ¿Se elimina? | Propósito |
|---|---|---|---|
| `bridge` | predefined | Nunca | Default para contenedores |
| `host` | predefined | Nunca | Elimina network namespace |
| `none` | predefined | Nunca | Sin red |
| `lab_default` | custom (compose) | Sí | Creada por compose |

---

## Resumen de comandos

| Comando | Qué elimina | Riesgo | Recuperable | Confirmación |
|---|---|---|---|---|
| `docker container prune` | Contenedores detenidos | Bajo | No (capa escritura) | Sí |
| `docker image prune` | Imágenes dangling | Bajo | Sí (rebuild/pull) | Sí |
| `docker image prune -a` | Imágenes no usadas | Medio | Sí (rebuild/pull) | Sí |
| `docker volume prune` | Volúmenes huérfanos | **Alto** | **No** (datos perdidos) | Sí |
| `docker volume prune -a` | Todos los volúmenes huérfanos | **Muy alto** | **No** | Sí |
| `docker builder prune` | Build cache | Bajo | Sí (se regenera) | Sí |
| `docker builder prune -a` | Todo el cache | Medio | Sí (tarda) | Sí |
| `docker network prune` | Redes no usadas | Mínimo | Sí (se recrean) | Sí |

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

# Ver estado antes de limpiar
docker system df | grep Containers

# Limpiar
docker container prune -f

# Verificar que se eliminaron
docker ps -a | grep test_

# Ver espacio recuperado
docker system df | grep Containers
```

---

### Ejercicio 2 — Generar y limpiar imágenes dangling

```bash
# Crear un Dockerfile simple
mkdir -p /tmp/test-prune
cat > /tmp/test-prune/Dockerfile << 'EOF'
FROM alpine:latest
RUN echo "version 1" > /version.txt
EOF

# Build 1
docker build -t prune-test /tmp/test-prune

# Modificar y rebuild (genera dangling)
sed -i 's/version 1/version 2/' /tmp/test-prune/Dockerfile
docker build -t prune-test /tmp/test-prune

# Ver la imagen dangling
docker image ls --filter dangling=true

# Limpiar dangling
docker image prune -f

# Verificar
docker image ls --filter dangling=true
# (vacío)

# Limpiar la imagen de test
docker image rm prune-test
rm -rf /tmp/test-prune
```

---

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
docker volume prune -f
# Elimina test_safe, no test_important

# Verificar
docker volume ls

# Limpiar
docker rm -f vol_user
docker volume rm test_important
```

---

### Ejercicio 4 — Rebuild y observar crecimiento de imágenes

```bash
# Crear proyecto de prueba
mkdir -p /tmp/image-growth
cat > /tmp/image-growth/Dockerfile << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y curl wget
EOF

# Estado inicial
docker system df | grep Images

# Build 1
docker build -t growth-test /tmp/image-growth

# Estado después de build 1
docker system df | grep Images

# Hacer otro build (sin cambios)
docker build -t growth-test /tmp/image-growth

# Ver imágenes dangling
docker image ls --filter dangling=true
# Verás una imagen <none>

# Limpiar
docker image prune -f
docker image rm growth-test
rm -rf /tmp/image-growth
```

---

### Ejercicio 5 — Limpieza selectiva con filtros

```bash
# Crear contenedores con diferentes labels
docker run --label "env=test" --rm alpine echo test
docker run --label "env=prod" --rm alpine echo prod
docker run --label "env=test" --rm alpine echo test2

# Limpiar solo los de test
docker container prune --filter "label=env=test"

# Verificar que prod sigue corriendo
docker ps -a --filter "label=env=prod"
```

---

### Ejercicio 6 — Estimar espacio antes y después de prune

```bash
# Guardar estado inicial
docker system df > /tmp/before.txt
cat /tmp/before.txt

# Hacer varias operaciones que generan residuos
for i in 1 2 3; do
    docker run --rm alpine echo $i
done

# Recrear scenario con volúmenes huérfanos
docker volume create temp_vol_$i 2>/dev/null || true

# Guardar estado después de generar residuos
docker system df > /tmp/after.txt
cat /tmp/after.txt

# Comparar
echo "=== Diferencia ==="
diff /tmp/before.txt /tmp/after.txt

# Limpiar
docker container prune -f
docker volume prune -f

# Ver estado final
docker system df
```

---

### Ejercicio 7 — Proteger volúmenes importantes con labels

```bash
# Crear volumen con label de protección
docker volume create important_data
docker volume inspect important_data --format '{{.Labels}}'

# Agregar label después de crear
docker volume create --label "keep=true" important_backup

# Ver labels
docker volume ls --format '{{.Name}}\t{{.Labels}}'

# Para proteger: usar filter en prune
# docker volume prune --filter "label!=keep"
# (Esta sintaxis no funciona exactamente así, así que mejor verificar manualmente)
```

---

### Ejercicio 8 — Limpiar build cache conservando lo reciente

```bash
# Ver cache actual
docker builder du

# Limpiar pero mantener 100MB
docker builder prune --keep-storage 100MB

# Ver cache después
docker builder du

# Limpiar todo el cache
docker builder prune -af

# Ver cache después
docker builder du
```

---

### Ejercicio 9 — Combinar prune para limpieza completa

```bash
# Script de limpieza segura (excluye volúmenes)
#!/bin/bash
echo "=== Limpieza de Docker ==="
echo ""

echo "1. Contenedores detenidos..."
docker container prune -f

echo ""
echo "2. Imágenes dangling..."
docker image prune -f

echo ""
echo "3. Redes no usadas..."
docker network prune -f

echo ""
echo "4. Build cache (conservando 1GB)..."
docker builder prune --keep-storage 1GB -f

echo ""
echo "=== Resultado ==="
docker system df
```

---

### Ejercicio 10 — Recuperar espacio sin usar prune

```bash
# Eliminar contenedores específicos directamente
docker rm -v $(docker ps -aq --filter status=exited)

# Eliminar imágenes no usadas (sin prune -a)
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep -v ':latest' | xargs docker image rm 2>/dev/null || true

# Eliminar volúmenes específicos
docker volume rm $(docker volume ls -q --filter dangling=true)

# Verificar espacio
docker system df
```
