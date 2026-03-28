# T03 — Limpieza agresiva

## Objetivo

Entender `docker system prune` como herramienta de limpieza integral: qué
elimina, qué conserva, cuándo es seguro usarlo y cuándo no.

---

## `docker system prune`

Combina todas las limpiezas selectivas en un solo comando:

```bash
docker system prune
```

```
WARNING! This will remove:
  - all stopped containers
  - all networks not used by at least one container
  - all dangling images
  - all dangling build cache

Are you sure you want to continue? [y/N] y

Deleted Containers:
abc123def456

Deleted Networks:
old_project_default

Deleted Images:
deleted: sha256:9876fedc5432...

Total reclaimed space: 1.2GB
```

### Lo que elimina (por defecto)

```
docker system prune  =  docker container prune
                      +  docker network prune
                      +  docker image prune       (solo dangling)
                      +  docker builder prune
```

### Lo que NO elimina (por defecto)

| Recurso | ¿Eliminado? | Flag necesaria |
|---|---|---|
| Contenedores detenidos | Sí | — |
| Redes no usadas | Sí | — |
| Imágenes dangling | Sí | — |
| Build cache dangling | Sí | — |
| Imágenes con tag no usadas | **No** | `-a` |
| Volúmenes | **No** | `--volumes` |
| Contenedores corriendo | **No** | — (nunca) |
| Redes de contenedores activos | **No** | — (nunca) |

---

## Variantes

### `-a`: incluir imágenes no usadas

```bash
docker system prune -a
```

Además de lo anterior, elimina **todas las imágenes** que no tienen un
contenedor (activo o detenido) que las referencie. Esto incluye imágenes base
descargadas con `pull` e imágenes custom construidas con `build`.

**Impacto:** el siguiente `docker compose build` reconstruye desde cero,
descargando imágenes base y ejecutando todos los pasos del Dockerfile sin
cache.

### `--volumes`: incluir volúmenes

```bash
docker system prune --volumes
```

Incluye volúmenes huérfanos en la limpieza. Esta flag no se activa por defecto
precisamente por el riesgo de perder datos.

**PELIGRO:** Los volúmenes contienen datos persistentes. Una vez eliminados,
no hay forma de recuperarlos. Verificar siempre con
`docker volume ls --filter dangling=true` antes de usar esta flag.

### La combinación nuclear

```bash
docker system prune -a --volumes
```

Elimina **todo** lo que no esté activamente en uso:

```
Lo que se pierde:
├── Contenedores detenidos           ← sus capas de escritura
├── Imágenes no usadas               ← se re-descargan/reconstruyen
├── Imágenes dangling                ← no recuperables
├── Volúmenes huérfanos              ← DATOS PERDIDOS permanentemente
├── Build cache                      ← el próximo build será lento
└── Redes no usadas                  ← se recrean automáticamente

Lo que sobrevive:
├── Contenedores corriendo           ← y sus volúmenes montados
├── Imágenes de contenedores activos ← necesarias para los activos
└── Redes de contenedores activos    ← necesarias para los activos
```

### `-f`: sin confirmación

```bash
docker system prune -a -f
```

Ejecuta sin pedir confirmación. Útil en scripts de CI/CD, peligroso en uso
interactivo.

### Tabla de variantes

| Comando | Containers | Dangling | Imágenes no usadas | Redes | Build cache | Volúmenes |
|---|---|---|---|---|---|---|
| `system prune` | Sí | Sí | No | Sí | Sí | **No** |
| `system prune -a` | Sí | Sí | Sí | Sí | Sí | **No** |
| `system prune --volumes` | Sí | Sí | No | Sí | Sí | **Sí** |
| `system prune -a --volumes` | Sí | Sí | Sí | Sí | Sí | **Sí** |

---

## Cuándo es seguro

### En máquinas de desarrollo personal

Si no hay datos importantes en volúmenes Docker y las imágenes se pueden
reconstruir con Dockerfiles versionados:

```bash
# Todo está en Git, se puede reconstruir
docker system prune -a
docker compose build    # reconstruye desde cero
docker compose up -d    # lab funcional de nuevo
```

### Entre proyectos

Al terminar un proyecto y empezar otro, liberar todo el espacio del anterior:

```bash
# Terminar el proyecto actual
cd /path/to/old-project
docker compose down -v

# Liberar todo lo que quedó
docker system prune -a

# Empezar el nuevo proyecto
cd /path/to/new-project
docker compose build
docker compose up -d
```

### En CI/CD

Los runners de CI/CD necesitan liberar espacio regularmente. No hay datos
persistentes que conservar:

```bash
# Script de limpieza para CI
docker system prune -a -f --volumes
```

---

## Cuándo NO es seguro

### Bases de datos en volúmenes Docker

```yaml
# compose.yml de producción
services:
  postgres:
    image: postgres:16
    volumes:
      - pgdata:/var/lib/postgresql/data    # ← datos de la BD

volumes:
  pgdata:
```

Si el contenedor de la base de datos está **detenido** (no corriendo) y se
ejecuta `docker system prune --volumes`, el volumen `pgdata` se elimina
porque no tiene un contenedor activo que lo referencie. Los datos se pierden
irreversiblemente.

> **Matiz importante:** Un contenedor detenido sí protege su volumen de
> `docker volume prune` (el volumen no aparece como dangling). Pero
> `docker system prune` primero elimina contenedores detenidos, dejando
> los volúmenes huérfanos, y luego (con `--volumes`) los elimina.

### Imágenes que tardan mucho en reconstruirse

Imágenes con builds de 30+ minutos (compilación de dependencias pesadas,
modelos de ML, etc.):

```bash
docker system prune -a
# Elimina la imagen que tardó 45 minutos en construirse
# El siguiente build vuelve a tardar 45 minutos
```

### Contenedores detenidos con datos necesarios

```bash
# Contenedor con un informe generado en la capa de escritura
docker run --name report-gen myapp generate-report
# El informe está dentro del contenedor

docker system prune
# Elimina el contenedor y el informe
```

**La solución** es siempre usar volúmenes o bind mounts para datos importantes,
no depender de la capa de escritura del contenedor.

---

## Escenarios de riesgo

| Escenario | `prune` | `prune -a` | `prune --volumes` | `prune -a --volumes` |
|---|---|---|---|---|
| Dev personal, sin datos en volumes | Seguro | Seguro | Seguro | Seguro |
| BD corriendo (container up) | Seguro | Seguro | Seguro | Seguro |
| BD detenida (container stopped) | Pierde container | Pierde container + img | Pierde container + volume | Pierde todo |
| Datos en named volumes | Seguro | Seguro | **Peligroso** | **Peligroso** |
| Build cache de 2GB | Limpia | Limpia | Limpia | Limpia |
| Imágenes base (debian, alpine) | No las toca | Las elimina | No las toca | Las elimina |

---

## Espacio antes y después

Documentar el impacto de la limpieza:

```bash
# Antes
echo "=== ANTES ==="
docker system df

# Limpiar (sin --volumes: los volúmenes NO cambian)
docker system prune -a -f

# Después
echo "=== DESPUÉS ==="
docker system df
```

Ejemplo típico en una máquina de desarrollo:

```
=== ANTES ===
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          15        2         8.5GB     6.2GB (72%)
Containers      12        2         450MB     380MB (84%)
Local Volumes   8         2         1.2GB     800MB (66%)
Build Cache     25        0         2.1GB     2.1GB (100%)

=== DESPUÉS ===
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          2         2         1.5GB     0B (0%)
Containers      2         2         70MB      0B (0%)
Local Volumes   8         2         1.2GB     800MB (66%)     ← sin cambios
Build Cache     0         0         0B        0B (0%)
```

**Espacio liberado: ~8.8 GB** (imágenes + contenedores + build cache).
Los volúmenes no se tocaron porque no se usó `--volumes`.

---

## Prevención: evitar pérdidas

La mejor estrategia es prevenir, no recuperar:

| Estrategia | Descripción |
|---|---|
| Bind mounts para datos importantes | Los datos viven en el host, fuera del control de Docker |
| Backup de volúmenes | `docker run --rm -v vol:/data -v $(pwd):/backup alpine tar czf /backup/vol.tar.gz /data` |
| `docker compose down` sin `-v` | Preserva volúmenes al detener el proyecto |
| Labels en volúmenes | `docker volume create --label keep=true nombre` + filtros en prune |
| Nunca `--volumes` sin verificar | Siempre `docker volume ls --filter dangling=true` antes |

> **Importante:** Si ejecutaste `prune --volumes` por error, los datos de los
> volúmenes eliminados **no son recuperables**. Docker borra los archivos del
> filesystem. No hay papelera de reciclaje ni undo.

---

## Ejercicios

### Ejercicio 1 — Medir el impacto de prune básico

```bash
# Crear recursos de prueba para limpiar
docker run --name aggressive-test-1 alpine echo "test 1"
docker run --name aggressive-test-2 alpine echo "test 2"
docker network create aggressive-test-net
docker build -t aggressive-test -f labs/Dockerfile.rebuild labs/

# Documentar espacio antes
docker system df > /tmp/before.txt
cat /tmp/before.txt

# Ejecutar limpieza básica (sin -a, sin --volumes)
docker system prune -f

# Documentar después
docker system df > /tmp/after.txt
cat /tmp/after.txt

# Comparar
diff /tmp/before.txt /tmp/after.txt
```

<details><summary>Predicción</summary>

`docker system prune -f` (sin flags) elimina:
- `aggressive-test-1` y `aggressive-test-2` (contenedores detenidos)
- `aggressive-test-net` (red sin contenedores)
- Build cache dangling (si existe)

**NO elimina:**
- La imagen `aggressive-test` (tiene tag, no es dangling)
- Volúmenes (nunca sin `--volumes`)

En `diff`:
- **Containers**: TOTAL y SIZE bajan (se eliminaron los detenidos)
- **Build Cache**: puede bajar si había cache dangling
- **Images**: sin cambios (la imagen tiene tag)
- **Local Volumes**: sin cambios

</details>

---

### Ejercicio 2 — Efecto de `-a` en imágenes con tag

```bash
# Verificar que la imagen de test sigue
docker image ls aggressive-test

# Ejecutar prune con -a
docker system prune -a -f

# ¿Sobrevivió la imagen?
docker image ls aggressive-test
```

<details><summary>Predicción</summary>

Antes del prune, `aggressive-test` existe:
```
REPOSITORY        TAG       IMAGE ID       CREATED          SIZE
aggressive-test   latest    <hash>         X minutes ago    ~10MB
```

Después de `prune -a -f`, la imagen **desaparece**. El flag `-a` elimina
todas las imágenes que no tienen un contenedor (activo o detenido) que las
referencie. Como no hay ningún contenedor usando `aggressive-test`, se
elimina.

```
REPOSITORY        TAG       IMAGE ID       CREATED          SIZE
(vacío)
```

Sin `-a`, la imagen habría sobrevivido porque tiene tag y no es dangling.
La diferencia clave:
- `prune` (sin -a): solo elimina `<none>:<none>` (dangling)
- `prune -a`: elimina TODAS las imágenes sin contenedores asociados

</details>

---

### Ejercicio 3 — Simular el escenario peligroso con volúmenes

```bash
# Crear un "servicio" con datos en volumen
docker run -d --name fake-db -v fake-dbdata:/data alpine sh -c \
    'echo "registros importantes" > /data/records.txt && sleep 300'

# Verificar que los datos existen
docker exec fake-db cat /data/records.txt

# Paso 1: detener (pero NO eliminar) el contenedor
docker stop fake-db

# ¿El volumen es huérfano?
docker volume ls --filter dangling=true | grep fake-dbdata

# Paso 2: eliminar el contenedor
docker rm fake-db

# ¿Ahora es huérfano?
docker volume ls --filter dangling=true | grep fake-dbdata

# Paso 3: los datos aún existen (antes del prune)
docker run --rm -v fake-dbdata:/data alpine cat /data/records.txt

# Limpiar
docker volume rm fake-dbdata
```

<details><summary>Predicción</summary>

**Paso 1 (container stopped):** El volumen `fake-dbdata` **NO** aparece como
dangling. Un contenedor detenido sigue referenciando sus volúmenes. Esto
protege el volumen de `docker volume prune`.

**Paso 2 (container removed):** Ahora `fake-dbdata` **SÍ** aparece como
dangling — no tiene ningún contenedor asociado.

**Paso 3:** Los datos aún existen. Un volumen dangling no está vacío — contiene
los datos que tenía antes. Solo se pierden cuando se ejecuta `docker volume rm`
o `docker volume prune`.

**El escenario peligroso completo:**
1. `docker stop fake-db` → volumen protegido
2. `docker rm fake-db` → volumen huérfano
3. `docker system prune --volumes` → **datos perdidos permanentemente**

La secuencia `stop → rm → prune --volumes` es la que destruye datos
accidentalmente. Por eso es crucial verificar volúmenes antes de prune.

</details>

---

### Ejercicio 4 — Ver qué sobrevive a la limpieza nuclear

```bash
# Arrancar un contenedor con volumen
docker run -d --name survivor -v survivor-data:/data alpine \
    sh -c 'echo "datos activos" > /data/file.txt && sleep infinity'

# Verificar que está corriendo
docker ps | grep survivor
docker volume ls | grep survivor

# Ejecutar la limpieza más agresiva posible
docker system prune -a -f --volumes

# ¿Qué sobrevivió?
docker ps | grep survivor
docker volume ls | grep survivor
docker exec survivor cat /data/file.txt

# Limpiar
docker rm -f survivor
docker volume rm survivor-data
```

<details><summary>Predicción</summary>

Después de `prune -a --volumes -f`:
- El contenedor `survivor` **sigue corriendo** — prune nunca toca
  contenedores activos
- El volumen `survivor-data` **sigue existiendo** — está montado por un
  contenedor corriendo
- La imagen de `survivor` (alpine) **sigue existiendo** — es necesaria para
  el contenedor activo

```bash
docker exec survivor cat /data/file.txt
# datos activos
```

La regla de supervivencia es simple: **todo lo asociado a un contenedor
corriendo sobrevive**. Todo lo demás se elimina.

</details>

---

### Ejercicio 5 — Medir el costo de reconstrucción

```bash
# Construir la imagen de prueba (para tener cache)
docker build -t rebuild-test -f labs/Dockerfile.rebuild labs/

# Build con cache (segundo build idéntico)
echo "=== Build CON cache ==="
time docker build -t rebuild-test -f labs/Dockerfile.rebuild labs/ 2>&1 | tail -3

# Limpiar todo
docker system prune -a -f

# Build sin cache
echo ""
echo "=== Build SIN cache ==="
time docker build -t rebuild-test -f labs/Dockerfile.rebuild labs/ 2>&1 | tail -3

# Limpiar
docker rmi rebuild-test
```

<details><summary>Predicción</summary>

**Con cache:** El build tarda fracciones de segundo. Docker detecta que todas
las capas ya existen y no ejecuta nada:
```
 => CACHED [1/2] RUN apk add --no-cache curl
 => CACHED [2/2] RUN echo "lab image..."

real    0m0.5s
```

**Sin cache:** Docker descarga `alpine:latest` de nuevo y ejecuta todos los
`RUN`. Para este Dockerfile simple, tarda ~5-10 segundos. Para las imágenes
del curso (Debian dev con `apt-get install` + Rust), la diferencia es
**~5 segundos vs ~5-15 minutos**.

```
 => [1/2] RUN apk add --no-cache curl
 => [2/2] RUN echo "lab image..."

real    0m8.0s
```

Esta es la razón por la que `docker builder prune --keep-storage 1GB` es
mejor que `prune -a` para desarrollo diario — mantiene las capas más
recientes y evita rebuilds largos.

</details>

---

### Ejercicio 6 — Verificar que `--volumes` es necesario para volúmenes

```bash
# Crear volúmenes huérfanos
docker volume create orphan-vol-1
docker volume create orphan-vol-2
docker volume create orphan-vol-3

# Verificar que son huérfanos
docker volume ls --filter dangling=true | grep orphan

# Ejecutar prune SIN --volumes
docker system prune -f

# ¿Siguen los volúmenes?
docker volume ls | grep orphan

# Ahora con --volumes
docker system prune -f --volumes

# ¿Siguen los volúmenes?
docker volume ls | grep orphan
```

<details><summary>Predicción</summary>

Después de `prune -f` (sin `--volumes`):
```bash
docker volume ls | grep orphan
# orphan-vol-1
# orphan-vol-2
# orphan-vol-3
```

Los tres volúmenes **sobreviven**. `docker system prune` sin `--volumes`
nunca toca volúmenes — es una protección deliberada.

Después de `prune -f --volumes`:
```
Deleted Volumes:
orphan-vol-1
orphan-vol-2
orphan-vol-3
```

Ahora sí se eliminaron. El flag `--volumes` es necesario explícitamente
porque los volúmenes pueden contener datos irrecuperables.

</details>

---

### Ejercicio 7 — Script de limpieza para desarrollo

```bash
cat > /tmp/docker-clean-dev.sh << 'EOF'
#!/bin/bash
# Limpieza para máquinas de desarrollo
# NO elimina volúmenes ni imágenes con tag

set -euo pipefail

echo "=== Docker Clean (desarrollo) ==="
echo ""

echo "Estado antes:"
docker system df
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

echo "4. Build cache (conservando 500MB)..."
docker builder prune --keep-storage 500MB -f
echo ""

echo "Estado después:"
docker system df
EOF

chmod +x /tmp/docker-clean-dev.sh
/tmp/docker-clean-dev.sh
```

<details><summary>Predicción</summary>

El script limpia de forma conservadora:
- Contenedores detenidos → liberan poca memoria (capas de escritura)
- Imágenes dangling → solo `<none>:<none>`, las imágenes con tag sobreviven
- Redes no usadas → sin riesgo
- Build cache → conserva 500MB para builds futuros

**NO toca:**
- Volúmenes (protege datos)
- Imágenes con tag (evita re-downloads)
- Build cache reciente (mantiene 500MB para builds rápidos)

Este script es seguro para uso diario. Es el compromiso entre liberar espacio
y mantener el entorno operativo. Para limpieza total (entre proyectos), usar
`docker system prune -a` directamente.

</details>

---

### Ejercicio 8 — Script de limpieza para CI/CD

```bash
cat > /tmp/docker-clean-ci.sh << 'EOF'
#!/bin/bash
# Limpieza agresiva para CI/CD — TODO se reconstruye
# NUNCA usar en máquinas con datos persistentes

set -euo pipefail

echo "=== Docker Clean CI/CD ==="
echo ""
echo "ANTES:"
docker system df --format 'table {{.Type}}\t{{.Size}}\t{{.Reclaimable}}'
echo ""

docker system prune -a -f --volumes

echo ""
echo "DESPUÉS:"
docker system df --format 'table {{.Type}}\t{{.Size}}\t{{.Reclaimable}}'
EOF

chmod +x /tmp/docker-clean-ci.sh

# No ejecutar a menos que realmente quieras limpiar todo
echo "Script creado en /tmp/docker-clean-ci.sh"
echo "Ejecuta manualmente si estás seguro: /tmp/docker-clean-ci.sh"
```

<details><summary>Predicción</summary>

El script CI elimina TODO con `prune -a --volumes -f`:
- Todas las imágenes no usadas
- Todos los contenedores detenidos
- Todos los volúmenes huérfanos
- Todo el build cache
- Todas las redes no usadas

Después de ejecutar, Docker queda prácticamente vacío:
```
TYPE            SIZE      RECLAIMABLE
Images          0B        0B
Containers      0B        0B
Local Volumes   0B        0B
Build Cache     0B        0B
```

(Si hay contenedores corriendo, sus recursos sobreviven.)

**En CI/CD esto es seguro** porque:
1. Todo se reconstruye en cada pipeline
2. No hay datos persistentes entre jobs
3. El espacio en disco del runner es limitado

**En desarrollo NO es seguro** — perderías volúmenes con datos y tendrías
que reconstruir todas las imágenes.

</details>

---

### Ejercicio 9 — Reconstruir el lab del curso después de prune

```bash
# Verificar que el lab está corriendo
cd /path/to/lab
docker compose ps

# Detener el lab (preservar volúmenes)
docker compose down

# Limpieza agresiva (sin --volumes para preservar workspace)
docker system prune -a -f

# Verificar que todo se fue (excepto volúmenes)
docker image ls
docker volume ls

# Reconstruir desde cero y medir tiempo
time docker compose build

# Levantar de nuevo
docker compose up -d

# Verificar que funciona
docker compose exec debian-dev gcc --version
docker compose exec alma-dev gcc --version
```

<details><summary>Predicción</summary>

Después de `prune -a -f`:
- `docker image ls` muestra solo imágenes de contenedores corriendo (si los hay)
- `docker volume ls` muestra el volumen `workspace` (no se usó `--volumes`)

`docker compose build` descarga `debian:bookworm` y `almalinux:9` de nuevo,
ejecuta los `apt-get install`/`dnf install`, y compila Rust con rustup. Esto
tarda **5-15 minutos** dependiendo de la velocidad de red.

Después de rebuild, el lab funciona normalmente. Los datos del volumen
`workspace` se preservaron porque no se usó `--volumes`.

Si se hubiera usado `docker compose down -v` antes del prune, el volumen
workspace también se habría eliminado, perdiendo historial de shell,
configuración, y binarios compilados previamente.

</details>

---

### Ejercicio 10 — Backup de volumen antes de limpieza

```bash
# Crear un volumen con datos
docker volume create backup-test
docker run --rm -v backup-test:/data alpine sh -c \
    'echo "datos que quiero conservar" > /data/important.txt'

# Hacer backup del volumen a un archivo tar
docker run --rm \
    -v backup-test:/source:ro \
    -v "$(pwd)":/backup \
    alpine tar czf /backup/backup-test.tar.gz -C /source .

# Verificar el backup
ls -lh backup-test.tar.gz
tar tzf backup-test.tar.gz

# Ahora es seguro eliminar el volumen
docker volume rm backup-test

# Restaurar si es necesario
docker volume create backup-test
docker run --rm \
    -v backup-test:/target \
    -v "$(pwd)":/backup \
    alpine tar xzf /backup/backup-test.tar.gz -C /target

# Verificar restauración
docker run --rm -v backup-test:/data alpine cat /data/important.txt

# Limpiar
docker volume rm backup-test
rm -f backup-test.tar.gz
```

<details><summary>Predicción</summary>

El backup crea un archivo `backup-test.tar.gz` con el contenido del volumen:
```
-rw-r--r--  1 user user   XXB  <date> backup-test.tar.gz
```

`tar tzf` muestra:
```
./
./important.txt
```

Después de eliminar y restaurar, los datos están intactos:
```
datos que quiero conservar
```

**Cómo funciona el backup:**
1. Monta el volumen como `:ro` (read-only) en `/source`
2. Monta el directorio actual en `/backup`
3. `tar czf` comprime el contenido del volumen al directorio del host

**Patrón para backups del lab:**
```bash
# Backup del workspace del curso
docker run --rm \
    -v lab_workspace:/source:ro \
    -v "$(pwd)":/backup \
    alpine tar czf /backup/workspace-backup.tar.gz -C /source .
```

Hacer esto antes de `docker compose down -v` o `docker system prune --volumes`
protege los datos del workspace.

</details>
