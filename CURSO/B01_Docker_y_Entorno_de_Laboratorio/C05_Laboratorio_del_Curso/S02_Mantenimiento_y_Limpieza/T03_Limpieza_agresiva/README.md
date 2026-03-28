# T03 — Limpieza agresiva

## Objetivo

Entender `docker system prune` como herramienta de limpieza integral: qué
elimina, qué conserva, cuándo es seguro usarlo y cuándo no.

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

- Imágenes con tag que no están en uso (para eso: `-a`)
- Volúmenes (para eso: `--volumes`)
- Contenedores corriendo
- Redes usadas por contenedores activos

## Variantes

### `-a`: incluir imágenes no usadas

```bash
docker system prune -a
```

Además de lo anterior, elimina **todas las imágenes** que no tienen un
contenedor activo o detenido que las referencie. Esto incluye imágenes base
descargadas con `pull` e imágenes custom construidas con `build`.

Impacto: el siguiente `docker compose build` reconstruye desde cero, descargando
imágenes base y ejecutando todos los pasos del Dockerfile sin cache.

### `--volumes`: incluir volúmenes

```bash
docker system prune --volumes
```

Incluye volúmenes huérfanos en la limpieza. Esta flag no se activa por defecto
precisamente por el riesgo de perder datos.

### La combinación nuclear

```bash
docker system prune -a --volumes
```

Elimina **todo** lo que no esté activamente en uso:

```
Lo que se pierde:
├── Contenedores detenidos           ← sus capas de escritura
├── Imágenes no usadas               ← se re-descargan/reconstruyen
├── Imágenes dangling                 ← no recuperables
├── Volúmenes huérfanos               ← DATOS PERDIDOS permanentemente
├── Build cache                       ← el próximo build será lento
└── Redes no usadas                   ← se recrean automáticamente

Lo que sobrevive:
├── Contenedores corriendo            ← y sus volúmenes montados
├── Imágenes de contenedores activos  ← necesarias para los activos
└── Redes de contenedores activos     ← necesarias para los activos
```

### `-f`: sin confirmación

```bash
docker system prune -a -f
```

Ejecuta sin pedir confirmación. Útil en scripts de CI/CD, peligroso en uso
interactivo.

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

# Liberar todo
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
  pgdata:    # ← docker system prune --volumes LO ELIMINA si el contenedor no está corriendo
```

Si el contenedor de la base de datos está detenido y se ejecuta
`docker system prune --volumes`, los datos se pierden irreversiblemente.

### Imágenes que tardan mucho en reconstruirse

Imágenes con builds de 30+ minutos (compilación de dependencias pesadas, modelos
de ML, etc.):

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

La solución es siempre usar volúmenes para datos importantes, no depender de la
capa de escritura del contenedor.

## Espacio antes y después

Documentar el impacto de la limpieza:

```bash
# Antes
echo "=== ANTES ==="
docker system df

# Limpiar
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
Local Volumes   2         2         400MB     0B (0%)
Build Cache     0         0         0B        0B (0%)
```

Espacio liberado: ~9.6 GB.

---

## Ejercicios

### Ejercicio 1 — Medir el impacto de la limpieza

```bash
# Documentar espacio antes
docker system df > /tmp/before.txt

# Ejecutar limpieza (sin volúmenes, sin -a para empezar)
docker system prune -f

# Documentar después
docker system df > /tmp/after.txt

# Comparar
diff /tmp/before.txt /tmp/after.txt
```

### Ejercicio 2 — Simular el escenario peligroso

```bash
# Crear un "servicio" con datos en volumen
docker run -d --name fake-db -v fake-dbdata:/data alpine sh -c \
    'echo "datos importantes" > /data/records.txt && sleep 300'

# Verificar que los datos existen
docker exec fake-db cat /data/records.txt

# Detener (pero no eliminar) el contenedor
docker stop fake-db

# ¿Qué pasa con prune --volumes?
docker volume ls --filter dangling=true
# fake-dbdata NO aparece (el contenedor detenido lo referencia)

# ¿Y si eliminamos el contenedor?
docker rm fake-db
docker volume ls --filter dangling=true
# fake-dbdata APARECE como huérfano

# Eliminar si se desea
docker volume rm fake-dbdata
```

### Ejercicio 3 — Reconstruir después de limpieza agresiva

```bash
# Asegurarse de que el lab está corriendo
cd /path/to/lab
docker compose up -d

# Detener el lab
docker compose down

# Limpieza agresiva
docker system prune -a -f

# Verificar que todo se fue
docker image ls
docker volume ls

# Reconstruir desde cero
time docker compose build
time docker compose up -d

# ¿Cuánto tardó comparado con un build con cache?
```
