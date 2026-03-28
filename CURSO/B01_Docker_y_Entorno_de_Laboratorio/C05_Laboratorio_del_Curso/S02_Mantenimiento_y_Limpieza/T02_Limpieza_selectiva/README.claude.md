# T02 — Limpieza selectiva

## Objetivo

Aprender a limpiar recursos de Docker de forma granular: imágenes, contenedores,
volúmenes, build cache y redes, eligiendo exactamente qué eliminar.

---

## Principio: limpiar por tipo

Docker ofrece un comando `prune` para cada tipo de recurso. Cada uno tiene su
nivel de riesgo:

```
Riesgo bajo  ─── docker network prune        (redes no usadas)
             ─── docker container prune       (contenedores detenidos)
             ─── docker image prune           (imágenes dangling)
             ─── docker builder prune         (build cache)
Riesgo alto  ─── docker volume prune          (datos persistentes)
```

Todos los comandos `prune` piden confirmación antes de ejecutar, excepto con
`-f` (force).

### Tabla de riesgos

| Comando | Qué elimina | Riesgo | Recuperación |
|---|---|---|---|
| `docker network prune` | Redes custom sin contenedores | Mínimo | Se recrean con `compose up` |
| `docker container prune` | Contenedores no corriendo | Bajo | Capa de escritura perdida |
| `docker image prune` | Imágenes dangling (`<none>`) | Bajo | Rebuild o pull |
| `docker image prune -a` | Imágenes sin contenedores | Medio | Rebuild o pull (tarda) |
| `docker builder prune` | Build cache no referenciado | Bajo | Se regenera al build |
| `docker builder prune -a` | Todo el build cache | Medio | Rebuild lento |
| `docker volume prune` | Volúmenes huérfanos | **Alto** | **Irrecuperable** |

---

## Contenedores: `docker container prune`

Elimina todos los contenedores que no están corriendo:

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

### Estados de contenedores y prune

| Estado | Significado | ¿Se elimina con prune? |
|---|---|---|
| `running` | Activo | No |
| `paused` | Pausado | No |
| `restarting` | Reiniciando | No |
| `created` | Creado, nunca iniciado | Sí (Docker 17.06.1+) |
| `exited` | Detenido normalmente | Sí |
| `dead` | Estado irrecuperable | Sí |

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

### Alternativa manual

```bash
# Ver qué se eliminaría
docker ps -a --filter status=exited --format 'table {{.Names}}\t{{.Status}}'

# Eliminar uno específico
docker rm container_name

# Eliminar todos los detenidos
docker rm $(docker ps -aq --filter status=exited)
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

> **Nota:** Un rebuild solo genera dangling si el Dockerfile o el build context
> cambiaron. Si nada cambió, Docker usa el cache completo y el image ID es el
> mismo — no se crea imagen nueva.

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

# Eliminar por patrón (nombre del repositorio)
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep "old-project" | xargs docker image rm

# Eliminar todas las dangling
docker image ls -q --filter dangling=true | xargs docker image rm
```

---

## Volúmenes: `docker volume prune`

```bash
docker volume prune
```

Elimina volúmenes no referenciados por ningún contenedor (ni activo ni
detenido).

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

Las labels de Compose ayudan a identificar a qué proyecto pertenece un volumen.

### Proteger volúmenes con labels

Las labels solo se pueden establecer al **crear** el volumen — no existe
`docker volume update`:

```bash
# Crear volumen con label de protección
docker volume create --label "keep=true" important_data

# Prune excluyendo volúmenes con label "keep"
docker volume prune --filter "label!=keep"
```

El filtro `--filter "label!=keep"` funciona correctamente: prune solo elimina
volúmenes que **no** tienen la label `keep`.

### Volúmenes anónimos vs nombrados

| Aspecto | Volumen nombrado | Volumen anónimo |
|---|---|---|
| Ejemplo | `workspace`, `dbdata` | `3a7b8c9d0e1f...` (hash) |
| Creación | `docker volume create` o compose | `VOLUME` en Dockerfile o `-v /data` |
| Riesgo de prune | Verificar contenido | Generalmente seguro |
| Identificación | Nombre legible + labels | Solo el hash |

---

## Build cache: `docker builder prune`

```bash
docker builder prune
```

Elimina capas de build cacheadas que no están referenciadas por ninguna imagen
existente.

### Opciones

```bash
# Eliminar TODO el cache (incluyendo el referenciado por imágenes)
docker builder prune -a

# Mantener al menos 5GB de cache (eliminar el exceso)
docker builder prune --keep-storage 5GB

# Cache de más de 7 días
docker builder prune --filter "until=168h"

# Combinar: liberar espacio pero mantener lo reciente
docker builder prune --keep-storage 2GB --filter "until=72h"
```

### Impacto

Sin build cache, el siguiente `docker build` ejecuta todas las instrucciones
desde cero:

| Escenario | Tiempo de `docker compose build` |
|---|---|
| Con cache | 5-30 segundos |
| Sin cache | 5-15 minutos (descarga paquetes, compila Rust) |

`--keep-storage` es un buen compromiso: libera espacio pero mantiene las capas
más recientes para que el siguiente build no empiece de cero.

---

## Redes: `docker network prune`

```bash
docker network prune
```

Elimina redes custom que no están siendo usadas por ningún contenedor. Las
redes predefinidas nunca se eliminan:

| Red | Tipo | ¿Se elimina? |
|---|---|---|
| `bridge` | Predefinida | Nunca |
| `host` | Predefinida | Nunca |
| `none` | Predefinida | Nunca |
| `lab_default` | Custom (Compose) | Sí, si no tiene contenedores |

Es la operación de prune con menor riesgo — las redes no contienen datos y
se recrean automáticamente con `docker compose up`.

---

## Ejercicios

### Ejercicio 1 — Crear y limpiar contenedores detenidos

```bash
# Crear contenedores efímeros (sin --rm → quedan detenidos)
for i in 1 2 3 4 5; do
    docker run --name "prune-test-$i" alpine echo "test $i"
done

# Ver los contenedores detenidos
docker ps -a --filter "name=prune-test" --format 'table {{.Names}}\t{{.Status}}'

# Espacio antes
docker system df | grep Containers

# Limpiar
docker container prune -f

# Verificar
docker ps -a | grep prune-test

# Espacio después
docker system df | grep Containers
```

<details><summary>Predicción</summary>

Los 5 contenedores aparecen con estado "Exited (0)":
```
NAMES           STATUS
prune-test-5    Exited (0) 2 seconds ago
prune-test-4    Exited (0) 3 seconds ago
...
```

Después de `container prune -f`, los 5 se eliminan:
```
Deleted Containers:
<hash1>
<hash2>
...
Total reclaimed space: <X>kB
```

El espacio recuperado es pequeño (KB) porque estos contenedores ejecutaron
`echo` y escribieron muy poco. Contenedores que instalan paquetes o generan
archivos ocupan mucho más.

`grep prune-test` no devuelve resultados — todos eliminados.

</details>

---

### Ejercicio 2 — Generar y limpiar imágenes dangling

```bash
# Usar el Dockerfile del lab (labs/Dockerfile.prune)
# Build 1: crear imagen con VERSION=1
docker build -t prune-test --build-arg VERSION=1 -f labs/Dockerfile.prune labs/

# Build 2: mismo tag, diferente versión → genera dangling
docker build -t prune-test --build-arg VERSION=2 -f labs/Dockerfile.prune labs/

# Ver la imagen dangling
docker image ls --filter dangling=true

# Limpiar solo dangling
docker image prune -f

# Verificar: la dangling desapareció pero prune-test sigue
docker image ls --filter dangling=true
docker image ls prune-test

# Limpiar la imagen de test
docker image rm prune-test
```

<details><summary>Predicción</summary>

Después del segundo build, la imagen de VERSION=1 pierde su tag y queda como
dangling:
```
REPOSITORY   TAG       IMAGE ID       CREATED          SIZE
<none>       <none>    <hash>         X seconds ago    ~7MB
```

`docker image prune -f` elimina la dangling:
```
Deleted Images:
deleted: sha256:<hash>
Total reclaimed space: ~2MB
```

`prune-test` (VERSION=2) sigue existiendo — `docker image prune` sin `-a` solo
elimina dangling (`<none>:<none>`), no imágenes con tag.

**Importante:** Si hubieras hecho el segundo build sin cambiar nada (mismo
VERSION=1), Docker habría reutilizado el cache completo y NO se habría creado
ninguna imagen dangling — el image ID sería el mismo.

</details>

---

### Ejercicio 3 — Volúmenes: entender el riesgo de prune

```bash
# Crear dos volúmenes
docker volume create prune-safe
docker volume create prune-important

# Poner datos en ambos
docker run --rm -v prune-safe:/data alpine sh -c 'echo "temporal" > /data/test.txt'
docker run --rm -v prune-important:/data alpine sh -c 'echo "datos críticos" > /data/test.txt'

# Asociar solo uno a un contenedor activo
docker run -d --name prune-vol-user -v prune-important:/data alpine sleep 300

# ¿Cuál es huérfano?
docker volume ls --filter dangling=true

# Prune (solo elimina huérfanos)
docker volume prune -f

# Verificar: ¿qué volúmenes quedan?
docker volume ls | grep prune

# Ahora eliminar el contenedor
docker rm -f prune-vol-user

# ¿Qué pasó con prune-important?
docker volume ls --filter dangling=true | grep prune

# Limpiar
docker volume rm prune-important
```

<details><summary>Predicción</summary>

Solo `prune-safe` aparece como dangling porque `prune-important` está
asociado al contenedor `prune-vol-user`.

Después de `volume prune -f`:
```
Deleted Volumes:
prune-safe
Total reclaimed space: <X>B
```

`prune-important` sobrevive — un volumen en uso no se elimina.

**El escenario peligroso:** Después de `docker rm -f prune-vol-user`,
`prune-important` queda **huérfano**. Si ahora hicieras `docker volume prune`,
se eliminaría con sus "datos críticos". Este es exactamente el escenario
que hace peligroso el volume prune: eliminaste el contenedor pero querías
conservar los datos.

**Lección:** Antes de `volume prune`, siempre verifica con
`docker volume ls --filter dangling=true` y `docker volume inspect` que
no haya datos importantes.

</details>

---

### Ejercicio 4 — Filtros de tiempo en container prune

```bash
# Crear un contenedor que quedará detenido
docker run --name prune-old alpine echo "old"

# Esperar un poco y crear otro
sleep 3
docker run --name prune-new alpine echo "new"

# Limpiar solo contenedores más antiguos que 2 segundos
docker container prune --filter "until=2s" -f

# ¿Cuál sobrevivió?
docker ps -a --filter "name=prune" --format 'table {{.Names}}\t{{.Status}}'

# Limpiar
docker rm prune-new prune-old 2>/dev/null
```

<details><summary>Predicción</summary>

`--filter "until=2s"` solo elimina contenedores que se detuvieron hace más
de 2 segundos. `prune-old` se detuvo hace ~5 segundos, así que se elimina.
`prune-new` se detuvo hace ~2 segundos, así que sobrevive (o podría
eliminarse también si la ejecución fue rápida).

```
NAMES        STATUS
prune-new    Exited (0) 3 seconds ago
```

El filtro `until` es útil para conservar contenedores recientes mientras
limpias los antiguos. Combinaciones comunes:
- `--filter "until=24h"` — eliminar contenedores de ayer
- `--filter "until=168h"` — eliminar contenedores de más de una semana

</details>

---

### Ejercicio 5 — Limpiar build cache conservando espacio

```bash
# Ver estado actual del build cache
docker builder du

# Hacer un build para generar cache
docker build -t prune-cache --build-arg VERSION=1 -f labs/Dockerfile.prune labs/

# Ver cache después
docker builder du

# Limpiar pero mantener 100MB
docker builder prune --keep-storage 100MB -f

# Limpiar todo el cache
docker builder prune -a -f

# Reconstruir (observar que es más lento sin cache)
time docker build -t prune-cache --build-arg VERSION=2 -f labs/Dockerfile.prune labs/

# Limpiar imagen
docker rmi prune-cache
```

<details><summary>Predicción</summary>

`docker builder du` muestra el cache acumulado:
```
ID           RECLAIMABLE  SIZE        LAST ACCESSED
<hash>       true         <X>MB       <time>
...
Reclaimable: <total>
Total:       <total>
```

Con `--keep-storage 100MB`, solo se elimina cache que exceda los 100MB.
Para imágenes pequeñas como las del lab, esto probablemente no elimine nada.

Con `-a`, se elimina todo el cache. El rebuild posterior descarga `alpine`
de nuevo (si no está en cache de imágenes) y ejecuta todas las capas. Para
este Dockerfile simple, la diferencia es pequeña (1-2 segundos). Para las
imágenes del curso (con `apt-get install` y Rust), la diferencia es de
segundos vs minutos.

</details>

---

### Ejercicio 6 — Limpiar redes custom

```bash
# Crear redes de prueba
docker network create prune-net-1
docker network create prune-net-2

# Asociar una a un contenedor
docker run -d --name prune-net-user --network prune-net-1 alpine sleep 60

# Ver redes
docker network ls | grep prune

# Prune
docker network prune -f

# ¿Cuál sobrevivió?
docker network ls | grep prune

# Limpiar
docker rm -f prune-net-user
docker network rm prune-net-1 2>/dev/null
```

<details><summary>Predicción</summary>

Antes del prune, ambas redes existen:
```
NETWORK ID     NAME           DRIVER    SCOPE
<hash>         prune-net-1    bridge    local
<hash>         prune-net-2    bridge    local
```

Después de `network prune -f`:
```
Deleted Networks:
prune-net-2
```

`prune-net-1` sobrevive porque tiene un contenedor asociado (`prune-net-user`).
`prune-net-2` se elimina porque no tiene contenedores.

Las redes predefinidas (`bridge`, `host`, `none`) nunca se eliminan con prune.
Es la operación de limpieza con menor riesgo: las redes no contienen datos y
Compose las recrea automáticamente.

</details>

---

### Ejercicio 7 — Proteger volúmenes con labels

```bash
# Crear volúmenes con y sin label de protección
docker volume create --label "keep=true" prune-protected
docker volume create prune-unprotected

# Verificar labels
docker volume inspect prune-protected --format '{{.Labels}}'
docker volume inspect prune-unprotected --format '{{.Labels}}'

# Prune EXCLUYENDO volúmenes con label "keep"
docker volume prune --filter "label!=keep" -f

# ¿Qué sobrevivió?
docker volume ls | grep prune

# Limpiar
docker volume rm prune-protected
```

<details><summary>Predicción</summary>

`docker volume inspect prune-protected` muestra:
```
map[keep:true]
```

`docker volume inspect prune-unprotected` muestra:
```
map[]
```

Después de `volume prune --filter "label!=keep" -f`:
```
Deleted Volumes:
prune-unprotected
```

`prune-protected` sobrevive porque tiene la label `keep`. El filtro
`--filter "label!=keep"` significa "solo eliminar volúmenes que NO tengan
la label `keep`".

**Importante:** Las labels solo se pueden establecer al crear el volumen.
No existe `docker volume update` para agregar labels después. Si necesitas
proteger un volumen existente, la única opción es:
1. Crear un nuevo volumen con label
2. Copiar los datos
3. Eliminar el viejo

O simplemente verificar manualmente antes de cada `volume prune`.

</details>

---

### Ejercicio 8 — Script de limpieza segura

Crea un script que limpia todo EXCEPTO volúmenes:

```bash
#!/bin/bash
echo "=== Limpieza segura de Docker ==="
echo "(excluye volúmenes para proteger datos)"
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

echo "=== Estado final ==="
docker system df
```

<details><summary>Predicción</summary>

El script limpia en orden de menor a mayor riesgo, excluyendo volúmenes
intencionalmente. Cada paso muestra qué se eliminó:

```
=== Limpieza segura de Docker ===
(excluye volúmenes para proteger datos)

1. Contenedores detenidos...
Total reclaimed space: XMB

2. Imágenes dangling...
Total reclaimed space: XMB

3. Redes no usadas...
Deleted Networks: ...

4. Build cache (conservando 1GB)...
Total reclaimed space: XMB

=== Estado final ===
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
...
```

Después de la limpieza, RECLAIMABLE en Images, Containers y Build Cache
debería ser cercano a 0 (o bajo). Local Volumes mantiene su valor
original porque no se tocó.

Este script es más seguro que `docker system prune` porque:
1. No toca volúmenes (protege datos)
2. Solo limpia imágenes dangling, no todas las no usadas
3. Conserva 1GB de build cache para builds futuros

</details>

---

### Ejercicio 9 — Comparar `docker system prune` vs limpieza selectiva

```bash
# Crear recursos de prueba
docker run --name prune-compare alpine echo "test"
docker volume create prune-compare-vol
docker network create prune-compare-net

# Ver estado
docker system df

# docker system prune (sin -a, sin --volumes)
docker system prune -f

# ¿Qué se eliminó?
docker ps -a | grep prune-compare
docker volume ls | grep prune-compare
docker network ls | grep prune-compare

# Limpiar lo que quedó
docker volume rm prune-compare-vol 2>/dev/null
```

<details><summary>Predicción</summary>

`docker system prune -f` (sin flags adicionales) elimina:
- Contenedores detenidos → `prune-compare` eliminado
- Imágenes dangling → eliminadas (si hay)
- Redes no usadas → `prune-compare-net` eliminada
- Build cache no referenciado → eliminado

**NO** elimina:
- Volúmenes → `prune-compare-vol` sobrevive
- Imágenes no usadas (solo dangling)

Para eliminar también volúmenes: `docker system prune --volumes`
Para eliminar también imágenes no usadas: `docker system prune -a`
Para eliminar todo: `docker system prune -a --volumes`

Tabla comparativa:

| Flag | Contenedores | Dangling img | Todas img | Redes | Build cache | Volúmenes |
|---|---|---|---|---|---|---|
| (ninguno) | Sí | Sí | No | Sí | Sí | **No** |
| `-a` | Sí | Sí | Sí | Sí | Sí | **No** |
| `--volumes` | Sí | Sí | No | Sí | Sí | **Sí** |
| `-a --volumes` | Sí | Sí | Sí | Sí | Sí | **Sí** |

</details>

---

### Ejercicio 10 — Eliminar recursos por patrón (sin prune)

A veces necesitas más control que lo que `prune` ofrece:

```bash
# Crear recursos de prueba con prefijo
docker run --name pattern-test-1 alpine echo "1"
docker run --name pattern-test-2 alpine echo "2"
docker run --name pattern-keep-1 alpine echo "keep"

# Eliminar solo los que coinciden con un patrón
docker rm $(docker ps -aq --filter "name=pattern-test")

# Verificar: pattern-keep-1 sobrevivió
docker ps -a --filter "name=pattern" --format '{{.Names}}'

# Mismo enfoque con imágenes
docker image ls --format '{{.Repository}}:{{.Tag}}' | grep "old-project" | xargs docker image rm 2>/dev/null

# Y con volúmenes
docker volume ls --format '{{.Name}}' | grep "temp" | xargs docker volume rm 2>/dev/null

# Limpiar
docker rm pattern-keep-1 2>/dev/null
```

<details><summary>Predicción</summary>

`docker ps -aq --filter "name=pattern-test"` devuelve los IDs de
`pattern-test-1` y `pattern-test-2`, que se eliminan con `docker rm`.

`pattern-keep-1` sobrevive porque no coincide con el filtro.

```
NAMES
pattern-keep-1
```

La eliminación manual por patrón es más segura que `prune` cuando:
- Solo quieres eliminar recursos de un proyecto específico
- Necesitas preservar recursos que `prune` eliminaría
- Quieres verificar exactamente qué se va a eliminar antes de actuar

El flujo seguro es siempre: **listar → verificar → eliminar**:
```bash
# 1. Listar qué coincide
docker ps -a --filter "name=pattern" --format '{{.Names}}'

# 2. Verificar que la lista es correcta (revisión visual)

# 3. Eliminar
docker rm $(docker ps -aq --filter "name=pattern")
```

</details>
