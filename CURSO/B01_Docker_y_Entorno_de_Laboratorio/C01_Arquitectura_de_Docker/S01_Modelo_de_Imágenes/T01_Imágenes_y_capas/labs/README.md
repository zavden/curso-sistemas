# Lab — Imágenes y capas

## Objetivo

Laboratorio práctico para verificar de forma empírica los conceptos de capas,
Copy-on-Write, deduplicación, whiteouts y la capa de escritura efímera.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `hello.c` | Programa C simple para compilar dentro de una imagen |
| `Dockerfile.layers` | Imagen con múltiples capas para observar anatomía |
| `Dockerfile.dedup-a` | Imagen con curl (para comparar deduplicación) |
| `Dockerfile.dedup-b` | Imagen con wget (para comparar deduplicación) |
| `Dockerfile.bad` | Patrón incorrecto: elimina archivo en capa separada |
| `Dockerfile.good` | Patrón correcto: crea y elimina en la misma capa |
| `Dockerfile.gcc` | Caso real: compilar con gcc y desinstalarlo en un solo RUN |

---

## Parte 1 — Anatomía de capas

### Objetivo

Construir una imagen, identificar cada capa, y distinguir capas con contenido
de capas de metadata.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.layers
```

Antes de construir, responde mentalmente:

- ¿Cuántas instrucciones tiene el Dockerfile?
- ¿Cuántas de esas instrucciones crearán capas con contenido (>0 bytes)?
- ¿Cuáles solo generan metadata?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2: Construir la imagen

```bash
docker build -f Dockerfile.layers -t lab-layers .
```

Observa la salida del build: Docker enumera cada step. Compara con tu predicción.

### Paso 1.3: Ver el historial de capas

```bash
docker history lab-layers
```

Salida esperada (aproximada):

```
IMAGE          CREATED         CREATED BY                                      SIZE      COMMENT
<hash>         seconds ago     CMD ["/src/hello"]                              0B
<hash>         seconds ago     LABEL description=Lab - Anatomia de capas       0B
<hash>         seconds ago     RUN gcc -o /src/hello /src/hello.c              16.4kB
<hash>         seconds ago     RUN apt-get update && apt-get install -y ...    ~170MB
<hash>         seconds ago     COPY hello.c /src/hello.c                       ~150B
<hash>         seconds ago     WORKDIR /src                                    0B
<hash>         2 weeks ago     /bin/sh -c #(nop)  CMD ["bash"]                 0B
<hash>         2 weeks ago     /bin/sh -c #(nop) ADD file:... in /             ~117MB
```

### Paso 1.4: Analizar

Mapea cada instrucción del Dockerfile a su capa:

| Instrucción | Tamaño | ¿Capa con contenido? |
|---|---|---|
| `FROM debian:bookworm` (ADD) | ~117MB | Sí — filesystem base completo |
| `WORKDIR /src` | 0B | No — solo cambia directorio de trabajo |
| `COPY hello.c` | ~150B | Sí — copia archivo al filesystem |
| `RUN apt-get install gcc` | ~170MB | Sí — instala paquetes |
| `RUN gcc -o hello hello.c` | ~16kB | Sí — crea binario compilado |
| `LABEL` | 0B | No — solo metadata |
| `CMD` | 0B | No — solo metadata |

**Regla**: FROM, RUN, COPY y ADD crean capas con contenido. CMD, ENV, EXPOSE,
LABEL, WORKDIR crean capas de metadata (0 bytes).

### Paso 1.5: Inspeccionar los hashes SHA256

```bash
docker image inspect lab-layers --format '{{json .RootFS.Layers}}' | python3 -m json.tool
```

Cada hash identifica una capa de forma única. Estos hashes son los que Docker
usa para deduplicar capas entre imágenes.

### Paso 1.6: Contar capas

```bash
# Total de entradas en el historial
docker history lab-layers --format '{{.Size}}' | wc -l

# Capas con contenido (tamaño > 0)
docker history lab-layers --format '{{.Size}}' | grep -cv '^0B$'

# Capas de solo metadata (tamaño = 0)
docker history lab-layers --format '{{.Size}}' | grep -c '^0B$'
```

### Paso 1.7: Ejecutar la imagen

```bash
docker run --rm lab-layers
```

```
Hola desde una imagen Docker con multiples capas!
Este binario fue compilado dentro de una capa del Dockerfile.
```

### Paso 1.8: Ver instrucciones completas

```bash
docker history --no-trunc --format "table {{.CreatedBy}}\t{{.Size}}" lab-layers
```

Útil para ver exactamente qué ejecutó cada RUN cuando se encadenan múltiples
comandos con `&&`.

---

## Parte 2 — Copy-on-Write en acción

### Objetivo

Demostrar que las modificaciones en un contenedor no alteran la imagen, que cada
contenedor tiene su propia capa de escritura independiente, y medir el costo de
CoW con archivos de distintos tamaños.

### Paso 2.1: Inmutabilidad de la imagen

```bash
# Modificar un archivo dentro de un contenedor
docker run --name cow-test debian:bookworm \
    bash -c "echo 'MODIFICADO POR CONTENEDOR' > /etc/motd"

# Verificar que la imagen original NO fue afectada
docker run --rm debian:bookworm cat /etc/motd
# Salida: vacío o contenido original — NO "MODIFICADO POR CONTENEDOR"

# El contenedor detenido SÍ tiene la modificación en su capa de escritura
docker start cow-test
docker exec cow-test cat /etc/motd
# Salida: MODIFICADO POR CONTENEDOR
docker stop cow-test
docker rm cow-test
```

La modificación solo existe en la capa de escritura del contenedor.
La imagen es inmutable.

### Paso 2.2: Capas de escritura independientes

```bash
# 3 contenedores desde la misma imagen
docker run -d --name cow-a debian:bookworm sleep 120
docker run -d --name cow-b debian:bookworm sleep 120
docker run -d --name cow-c debian:bookworm sleep 120

# Cada uno escribe un valor distinto en el mismo path
docker exec cow-a bash -c "echo 'soy A' > /tmp/id"
docker exec cow-b bash -c "echo 'soy B' > /tmp/id"
docker exec cow-c bash -c "echo 'soy C' > /tmp/id"

# Cada contenedor ve SOLO su propia modificación
docker exec cow-a cat /tmp/id    # soy A
docker exec cow-b cat /tmp/id    # soy B
docker exec cow-c cat /tmp/id    # soy C

docker rm -f cow-a cow-b cow-c
```

Comparten las capas de lectura (la imagen) pero sus capas de escritura son
completamente independientes.

### Paso 2.3: Costo de CoW con archivos grandes

```bash
# Contenedor que crea un archivo de 100MB en la capa de escritura
docker run --name cow-big debian:bookworm \
    bash -c "dd if=/dev/zero of=/bigfile bs=1M count=100 2>/dev/null && sync"

docker inspect cow-big --format 'cow-big: {{.SizeRw}} bytes'
# ~104857600 (100MB)

# Contenedor que modifica solo 1 byte de un archivo pequeño
docker run --name cow-tiny debian:bookworm \
    bash -c "echo 'x' >> /etc/hostname"

docker inspect cow-tiny --format 'cow-tiny: {{.SizeRw}} bytes'
# Muy pequeño (~13 bytes — solo se copió /etc/hostname)

docker rm cow-big cow-tiny
```

El costo de CoW es proporcional al tamaño del **archivo** modificado, no al
tamaño de la **modificación**. Modificar 1 byte de un archivo de 500MB copia
los 500MB completos a la capa de escritura.

### Paso 2.4: `docker diff` muestra los cambios CoW

```bash
docker run --name cow-diff debian:bookworm \
    bash -c "echo 'nuevo' > /tmp/nuevo.txt && rm /etc/hostname && echo 'mod' >> /etc/motd"

docker diff cow-diff
```

Salida esperada:

```
C /etc                ← Changed (directorio modificado)
D /etc/hostname       ← Deleted (oculto con whiteout)
C /etc/motd           ← Changed (copiado por CoW y modificado)
A /tmp                ← Added (directorio nuevo)
A /tmp/nuevo.txt      ← Added (creado en la capa de escritura)
```

Los prefijos:

- `A` = Added — creado directamente en la capa de escritura
- `C` = Changed — archivo de capa inferior copiado por CoW y modificado
- `D` = Deleted — archivo oculto con un whiteout (ver Parte 5)

```bash
docker rm cow-diff
```

---

## Parte 3 — Deduplicación de capas

### Objetivo

Comprobar que Docker almacena cada capa una sola vez en disco, incluso cuando
múltiples imágenes la comparten, y medir el ahorro de espacio.

### Paso 3.1: Examinar los Dockerfiles

```bash
cat Dockerfile.dedup-a
cat Dockerfile.dedup-b
```

Ambos parten de `debian:bookworm`. Uno instala `curl`, el otro `wget`.
Deberían compartir la capa base.

### Paso 3.2: Construir ambas imágenes

```bash
docker build -f Dockerfile.dedup-a -t lab-dedup-curl .
docker build -f Dockerfile.dedup-b -t lab-dedup-wget .
```

### Paso 3.3: Comparar hashes de capas

```bash
echo "=== Capas de lab-dedup-curl ==="
docker image inspect lab-dedup-curl --format '{{json .RootFS.Layers}}' | python3 -m json.tool

echo ""
echo "=== Capas de lab-dedup-wget ==="
docker image inspect lab-dedup-wget --format '{{json .RootFS.Layers}}' | python3 -m json.tool
```

Observa: la primera capa de ambas imágenes tiene **el mismo hash SHA256**.
Docker no la duplica en disco — la almacena una sola vez.

### Paso 3.4: Medir espacio real vs virtual

```bash
# Tamaños virtuales (docker images)
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-dedup

# Espacio real con deduplicación
docker system df

# Desglose por imagen (observar SHARED SIZE)
docker system df -v 2>/dev/null | grep -E "lab-dedup|REPOSITORY"
```

La columna `SHARED SIZE` muestra cuánto espacio comparte esa imagen con otras.
El espacio real total (en `docker system df`) es menor que la suma aritmética de
los tamaños virtuales, porque las capas compartidas se cuentan una sola vez.

### Paso 3.5: Limpiar

```bash
docker rmi lab-dedup-curl lab-dedup-wget
```

---

## Parte 4 — El costo oculto de eliminar en capas separadas

### Objetivo

Demostrar que eliminar archivos en un `RUN` separado de donde se crearon **no
reduce** el tamaño de la imagen, porque la capa anterior sigue existiendo con
el archivo completo.

### Paso 4.1: Examinar los Dockerfiles

```bash
echo "=== Patrón incorrecto ==="
cat Dockerfile.bad

echo ""
echo "=== Patrón correcto ==="
cat Dockerfile.good
```

Antes de construir, predice: ¿cuánto pesará cada imagen? ¿Por qué?

### Paso 4.2: Construir y comparar

```bash
docker build -f Dockerfile.bad  -t lab-bad  .
docker build -f Dockerfile.good -t lab-good .

docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep lab-
```

Salida esperada:

```
lab-bad    latest    ~170MB   (debian ~117MB + 50MB del archivo "borrado")
lab-good   latest    ~117MB   (solo debian — el archivo se creó y borró en la misma capa)
```

~50MB desperdiciados por eliminar en una capa separada.

### Paso 4.3: Inspeccionar las capas

```bash
echo "=== lab-bad ==="
docker history lab-bad --format "table {{.CreatedBy}}\t{{.Size}}"

echo ""
echo "=== lab-good ==="
docker history lab-good --format "table {{.CreatedBy}}\t{{.Size}}"
```

En `lab-bad`:
- Una capa de ~50MB (el `dd` que crea el archivo)
- Una capa de ~0B (el `rm` que solo crea un whiteout — el archivo original
  sigue en la capa anterior)

En `lab-good`:
- Una capa de ~0B (el `dd` y `rm` en el mismo RUN — el resultado neto es vacío)

### Paso 4.4: Caso real — compilar con gcc y desinstalarlo

```bash
cat Dockerfile.gcc
```

Este Dockerfile instala gcc, compila `hello.c`, desinstala gcc y limpia caches
— todo en un solo RUN. La capa resultante contiene solo el binario compilado,
no los ~170MB de gcc.

```bash
docker build -f Dockerfile.gcc -t lab-gcc .

# Verificar que el binario funciona
docker run --rm lab-gcc

# Verificar que gcc NO está en la imagen final
docker run --rm lab-gcc which gcc 2>/dev/null || echo "gcc no encontrado (correcto)"

# Comparar tamaños
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep -E "lab-(bad|good|gcc|layers)"
```

`lab-gcc` es significativamente más pequeña que `lab-layers` (que mantiene gcc
instalado), porque todo lo temporal se creó y eliminó en el mismo RUN.

**Regla**: todo lo que se instala y se desinstala debe estar en el mismo `RUN`
para que la capa solo contenga el resultado neto.

### Paso 4.5: Limpiar

```bash
docker rmi lab-bad lab-good lab-gcc
```

---

## Parte 5 — Whiteouts y la capa de escritura efímera

### Objetivo

Entender cómo OverlayFS maneja la eliminación mediante archivos whiteout, y
demostrar que la capa de escritura se destruye con `docker rm`.

### Paso 5.1: Verificar el storage driver

```bash
docker info --format '{{.Driver}}'
# Debe mostrar: overlay2
```

Si muestra otro driver, los conceptos son similares pero los detalles pueden
variar.

### Paso 5.2: Estructura OverlayFS de un contenedor

```bash
docker create --name wh-inspect debian:bookworm bash

docker inspect wh-inspect --format '{{json .GraphDriver.Data}}' | python3 -m json.tool
```

Salida esperada:

```json
{
    "LowerDir": "/var/lib/docker/overlay2/.../diff:...",
    "MergedDir": "/var/lib/docker/overlay2/.../merged",
    "UpperDir": "/var/lib/docker/overlay2/.../diff",
    "WorkDir": "/var/lib/docker/overlay2/.../work"
}
```

- **LowerDir** — capas de solo lectura (la imagen), separadas por `:`
- **UpperDir** — capa de escritura del contenedor
- **MergedDir** — vista unificada que ve el proceso
- **WorkDir** — directorio interno de OverlayFS

> Los paths de `/var/lib/docker/` requieren permisos de root para acceder
> directamente. La inspección con `docker inspect` no requiere root.

```bash
docker rm wh-inspect
```

### Paso 5.3: Whiteouts con `docker diff`

```bash
docker run --name wh-demo debian:bookworm bash -c "
    rm /etc/hostname
    rm -rf /var/log/*
    echo 'archivo nuevo' > /tmp/nuevo.txt
    echo 'modificado' >> /etc/motd
"

docker diff wh-demo
```

Salida esperada (parcial):

```
C /etc
D /etc/hostname              ← Whiteout: oculta el archivo de la capa inferior
C /etc/motd                  ← CoW: copiado y modificado
A /tmp                       ← Directorio nuevo
A /tmp/nuevo.txt             ← Creado directamente en la capa de escritura
C /var
C /var/log
D /var/log/alternatives.log  ← Whiteout por cada archivo eliminado
D /var/log/apt               ← Whiteout
...
```

Cada `D` es un archivo whiteout en la capa de escritura. El archivo original
sigue en la capa inferior — solo se oculta de la vista `merged`.

Un whiteout es un **character device** con major/minor `0/0`. Su presencia
indica a OverlayFS que ignore el archivo correspondiente de las capas inferiores.

```bash
docker rm wh-demo
```

### Paso 5.4: La capa de escritura sobrevive a `stop` pero no a `rm`

```bash
# Crear contenedor con datos
docker run -d --name wh-persist debian:bookworm sleep 300
docker exec wh-persist bash -c "echo 'sobrevivo al stop' > /tmp/test.txt"

# Stop + start: los datos sobreviven
docker stop wh-persist
docker start wh-persist
docker exec wh-persist cat /tmp/test.txt
# sobrevivo al stop

# rm: los datos se destruyen permanentemente
docker rm -f wh-persist
# La capa de escritura fue eliminada — /tmp/test.txt ya no existe en ninguna parte
```

### Paso 5.5: Cuantificar capas de escritura

```bash
docker run --name wh-small debian:bookworm \
    bash -c "echo 'poco' > /tmp/small.txt"

docker run --name wh-medium debian:bookworm \
    bash -c "dd if=/dev/zero of=/tmp/medium.bin bs=1M count=10 2>/dev/null"

docker run --name wh-large debian:bookworm \
    bash -c "dd if=/dev/zero of=/tmp/large.bin bs=1M count=100 2>/dev/null"

# Comparar tamaños de las capas de escritura
docker inspect wh-small  --format 'wh-small:  {{.SizeRw}} bytes'
docker inspect wh-medium --format 'wh-medium: {{.SizeRw}} bytes'
docker inspect wh-large  --format 'wh-large:  {{.SizeRw}} bytes'

# Ver espacio total usado por contenedores
docker system df

docker rm wh-small wh-medium wh-large
```

---

## Limpieza final

```bash
# Eliminar imágenes del lab
docker rmi lab-layers 2>/dev/null

# Eliminar contenedores huérfanos (si quedó alguno por un paso saltado)
docker rm -f cow-test cow-a cow-b cow-c cow-big cow-tiny cow-diff \
    wh-inspect wh-demo wh-persist wh-small wh-medium wh-large 2>/dev/null
```

---

## Conceptos reforzados

1. Cada instrucción que modifica el filesystem (FROM, RUN, COPY, ADD) crea una
   capa con contenido. Las instrucciones de metadata crean capas de 0 bytes.
2. **Copy-on-Write** solo copia al escribir, no al leer. El costo es proporcional
   al tamaño del archivo, no al tamaño de la modificación.
3. Las capas se identifican por hash SHA256. Si dos imágenes comparten una capa,
   Docker la almacena una sola vez en disco.
4. Eliminar en un RUN separado **no ahorra espacio** — la capa anterior sigue
   existiendo. Todo lo temporal debe crearse y eliminarse en el mismo RUN.
5. `docker diff` muestra A (Added), C (Changed), D (Deleted). Los `D` son
   whiteouts que ocultan archivos de capas inferiores sin eliminarlos realmente.
6. La capa de escritura sobrevive a `docker stop` pero se destruye con
   `docker rm`. Para datos permanentes se necesitan volúmenes.
