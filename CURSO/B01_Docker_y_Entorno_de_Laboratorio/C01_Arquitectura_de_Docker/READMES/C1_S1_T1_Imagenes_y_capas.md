# T01 — Imágenes y capas

## Qué es una imagen Docker

Una imagen Docker es un paquete inmutable que contiene todo lo necesario para ejecutar
una aplicación: sistema de archivos, bibliotecas, dependencias, variables de entorno y
el comando a ejecutar. No contiene un kernel — los contenedores comparten el kernel del
host.

> **Nota importante**: "Inmutable" se refiere a que las capas de la imagen son de solo lectura.
> Una vez construida la imagen, sus capas no cambian. Lo que un contenedor escribe vive
> en una capa efímera separada.

Internamente, una imagen no es un solo archivo. Es una **pila ordenada de capas de solo
lectura**, donde cada capa representa un cambio incremental sobre la anterior.

## Capas y Union Filesystem

### OverlayFS

El mecanismo que permite apilar capas se llama **union filesystem**. En Linux moderno,
Docker usa **OverlayFS** (en su variante `overlay2`) por defecto. Puedes verificarlo con:

```bash
docker info --format '{{.Driver}}'
# overlay2
```

Puedes cambiar el storage driver editando `/etc/docker/daemon.json`:

```json
{
  "storage-driver": "overlay2"
}
```

Cambiar el storage driver requiere borrar todo (`docker system prune -a`) ya que los
formatos de almacenamiento son incompatibles entre drivers.

#### Anatomía de OverlayFS

OverlayFS combina varios directorios en una vista unificada:

| Directorio | Función | Contenedor lo ve? |
|------------|---------|-------------------|
| `lowerdir` | Capas de solo lectura (pueden ser muchas, apiladas) | No directamente |
| `upperdir` | Capa de escritura (solo una) | Sí |
| `workdir`  | Directorio de trabajo interno (operaciones atómicas de rename) | No |
| `merged`   | Vista unificada de lowerdir + upperdir | Sí, es el filesystem root |

El proceso dentro del contenedor opera sobre `merged`. Cuando lee un archivo, OverlayFS
busca desde `upperdir` hacia abajo en los `lowerdir` y devuelve la primera coincidencia.
Cuando escribe, siempre lo hace en `upperdir`.

```
┌─────────────────────────────────────────────────────────────┐
│  /var/lib/docker/overlay2/<container_id>/                   │
│                                                              │
│  ┌───────────┐  merged/     ← vista unificada (el "root")  │
│  ├───────────┤  diff/       ← capa de escritura (upperdir)  │
│  ├───────────┤  lower       ← archivo con rutas a lowerdirs │
│  ├───────────┤  work/       ← operaciones atómicas internas │
│  └───────────┘  link        ← nombre corto para esta capa   │
└─────────────────────────────────────────────────────────────┘
```

### Cómo se construyen las capas

Cada instrucción en un Dockerfile que modifica el filesystem crea una nueva capa:

```dockerfile
FROM debian:bookworm          # Capa 1: el filesystem base de Debian
RUN apt-get update            # Capa 2: los índices de paquetes actualizados
RUN apt-get install -y gcc    # Capa 3: gcc y sus dependencias instaladas
COPY hello.c /src/hello.c     # Capa 4: el archivo hello.c copiado
RUN gcc -o /src/hello /src/hello.c  # Capa 5: el binario compilado
```

Las instrucciones que solo establecen metadata (CMD, ENV, EXPOSE, LABEL, WORKDIR,
ARG, USER, HEALTHCHECK) **no crean capas con contenido** — generan capas de 0 bytes
que solo guardan configuración.

> **WORKDIR es un caso especial**: aunque se considera metadata, si la ruta no existe
> la crea, y esa creación sí genera contenido en una capa.

### Visualización de capas

```
┌──────────────────────────────┐
│  Capa de escritura           │  ← Solo existe en el contenedor (efímera)
│  (upperdir, writable layer)  │
├──────────────────────────────┤
│  Capa 5: binario compilado   │  ← Solo lectura (parte de la imagen)
├──────────────────────────────┤
│  Capa 4: hello.c copiado     │
├──────────────────────────────┤
│  Capa 3: gcc instalado       │
├──────────────────────────────┤
│  Capa 2: apt-get update      │
├──────────────────────────────┤
│  Capa 1: debian:bookworm     │
└──────────────────────────────┘
```

Cuando un contenedor se crea a partir de esta imagen, Docker añade una **capa de escritura
(writable layer)** encima. Todas las modificaciones que haga el contenedor (crear archivos,
editar configuración, instalar paquetes) se escriben en esta capa.

## Copy-on-Write (CoW)

Copy-on-write es el mecanismo por el cual OverlayFS optimiza lectura y escritura entre capas.

### Funcionamiento en lecturas

Cuando un proceso dentro del contenedor lee un archivo:

1. El kernel redirige la lectura al filesystem OverlayFS
2. OverlayFS busca el archivo comenzando por `upperdir` (capa superior)
3. Si no lo encuentra, sigue buscando en cada `lowerdir` en orden descendente
4. Retorna el primer resultado encontrado

Este proceso es transparente para el proceso — no sabe que está leyendo de múltiples
capas fusionadas.

### Funcionamiento en escrituras (copy-up)

Cuando un proceso **modifica** un archivo que está en una capa inferior (de solo lectura):

1. OverlayFS copia el **archivo completo** de la capa inferior a `upperdir`
2. La modificación se aplica sobre la copia en `upperdir`
3. El archivo original en la capa inferior **permanece inmutable**

Esto es **copy-on-write**: el costo de la copia solo se paga cuando hay una escritura,
no en cada lectura. La operación de copiar el archivo hacia arriba se llama **copy-up**.

> **Importante**: OverlayFS hace copy-up del archivo completo, no por fragmentos ni por
> páginas. Si un archivo de 500MB se modifica en 1 byte, los 500MB se copian a
> `upperdir`. Esto es diferente del CoW a nivel de memoria del kernel (que sí trabaja
> por páginas de 4KB) — no confundir los dos mecanismos.

### Implicaciones prácticas de CoW

- **Modificar un archivo grande es costoso la primera vez**: si un contenedor modifica
  un archivo de 500MB de una capa inferior, OverlayFS copia los 500MB a la capa de
  escritura antes de aplicar el cambio — incluso si solo cambia 1 byte.

- **Eliminar un archivo no libera espacio**: al "borrar" un archivo de una capa inferior,
  OverlayFS no elimina nada — crea un archivo especial llamado **whiteout** en
  `upperdir`. El whiteout oculta el archivo original, que sigue existiendo en la
  capa inferior.

```bash
# Cuando un contenedor borra /etc/motd de la imagen:
# Se crea un whiteout: upperdir/etc/.wh.motd (character device 0/0)
# El archivo original en lowerdir sigue intacto
# El espacio en disco de la imagen NO se libera
```

- **El primer copy-up es lento**: la primera vez que un proceso modifica un archivo
  grande de una capa inferior (librerías del sistema, por ejemplo), ocurre el copy-up
  completo. Accesos subsiguientes al mismo archivo ya van directo a `upperdir`.

- **Espacio en disco puede agotarse sorpresivamente**: si un contenedor modifica muchos
  archivos grandes de la imagen, consume más espacio del esperado porque cada copy-up
  duplica el archivo en `upperdir`.

### Opaque whiteouts

Cuando un contenedor borra un directorio completo de una capa inferior, OverlayFS no
crea un whiteout por cada archivo. En su lugar, crea un **opaque whiteout**: un archivo
especial `.wh..wh..opq` dentro del directorio en `upperdir`. Esto indica que todo lo que
había debajo de ese directorio en `lowerdir` debe ignorarse.

```bash
# Contenedor borra /var/cache/ completo
# En lugar de N whiteouts individuales:
# upperdir/var/cache/.wh..wh..opq  ← un solo archivo, oculta todo lowerdir/var/cache/
```

## Capas compartidas (deduplicación)

Cada capa se identifica por un hash SHA256 de su contenido. Si dos imágenes comparten
la misma capa base, Docker almacena esa capa **una sola vez** en disco.

```
Imagen A:                 Imagen B:
┌───────────────┐         ┌───────────────┐
│ Capa: app-a   │         │ Capa: app-b   │
├───────────────┤         ├───────────────┤
│ Capa: python  │ ◄═══════╡ Capa: python  │  ← Misma capa, almacenada una sola vez
├───────────────┤         ├───────────────┤
│ Capa: debian  │ ◄═══════╡ Capa: debian  │  ← Misma capa, almacenada una sola vez
└───────────────┘         └───────────────┘
```

Esto significa que descargar 10 imágenes basadas en `debian:bookworm` no descarga Debian
10 veces — solo la primera vez. Las siguientes reutilizan las capas existentes localmente.

### Deduplicación en disco vs en transferencia

| Contexto | Mecanismo |
|----------|-----------|
| Almacenamiento local | Capas con mismo hash se almacenan una sola vez en disco |
| Descarga (pull) | Docker solo descarga capas que no tiene localmente |
| Subida (push) | Docker solo sube capas que el registry no tiene |

### Dangling layers

Cuando una imagen se reconstruye (por ejemplo, `debian:bookworm` se actualiza), las capas
viejas quedan como **dangling** (colgadas) si ninguna otra imagen las referencia. Docker
las limpia con:

```bash
docker image prune          # Elimina dangling images
docker image prune -a       # Elimina TODAS las imágenes sin contenedores asociados
```

## La capa de escritura es efímera

La capa de escritura existe **mientras el contenedor existe**. Al eliminarlo con
`docker rm`, la capa de escritura se destruye y todo lo que se escribió se pierde.

```bash
# Crear un contenedor, escribir un archivo
docker run --name test debian:bookworm bash -c "echo 'data' > /tmp/myfile"

# El archivo existe en el contenedor detenido
docker start test
docker exec test cat /tmp/myfile   # "data"
docker stop test

# Pero al eliminar el contenedor, se pierde
docker rm test
# /tmp/myfile ya no existe en ninguna parte
```

> **docker rm vs docker rm -v**: por defecto, `docker rm` no elimina volúmenes anónimos.
> Con `-v` se eliminan. Los volúmenes nombrados sobreviven a menos que se eliminen
> explícitamente.

Para datos que deben sobrevivir a la destrucción del contenedor, se usan **volúmenes**
(cubiertos en la Sección 3).

### Contenedores vs imágenes: persistencia

| Aspecto | Imagen | Contenedor |
|---------|--------|------------|
| Capas | Solo lectura | + 1 capa RW encima |
| Persistencia | Inmutable, sobrevive a reinicios | Su capa RW muere con `docker rm` |
| Compartida | Sí, entre múltiples contenedores | No, cada contenedor tiene su propia capa RW |
| Modificable | No (sus capas son readonly) | Sí, pero los cambios se pierden al eliminar |

## El problema de la eliminación de archivos en capas

Un error común al crear Dockerfiles: instalar algo, usarlo, y luego eliminarlo en un
RUN separado, creyendo que se reduce el tamaño.

```dockerfile
# MAL — el archivo sigue ocupando espacio en la Capa 2
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc    # Capa 2: +200MB (gcc instalado)
RUN apt-get remove -y gcc && apt-get autoremove -y  # Capa 3: +whiteouts (gcc "borrado")
# Tamaño total: ~200MB (Capa 2 sigue ahí, los whiteouts solo la ocultan)
```

La capa 2 no desaparece mágicamente. El RUN de eliminación crea una nueva capa con
whiteouts que ocultan los archivos, pero el espacio de la capa original sigue ocupado.

```dockerfile
# BIEN — todo en la misma capa
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc \
    && gcc -o /app/hello /src/hello.c \
    && apt-get remove -y gcc && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*
# La capa solo contiene el resultado neto: el binario compilado
```

La regla es: **lo que se instala y se desinstala debe estar en el mismo RUN** para que
la capa resultante solo contenga el cambio neto.

### Otros errores comunes con capas

**Instalar paquetes en múltiples capas por separado:**

```dockerfile
# MAL — tres capas; además apt-get update sin install en el mismo RUN
#        puede quedar desactualizado por caché
FROM debian:bookworm
RUN apt-get update
RUN apt-get install -y curl
RUN apt-get install -y vim
```

```dockerfile
# BIEN — una sola capa, update e install juntos
FROM debian:bookworm
RUN apt-get update && apt-get install -y curl vim \
    && rm -rf /var/lib/apt/lists/*
```

**Orden subóptimo de instrucciones (rompe caché):**

Las capas se invalidan en cascada: si una instrucción cambia, todas las que vienen
después se reconstruyen. Lo óptimo es poner lo que cambia menos al inicio y lo que
cambia frecuentemente al final.

```dockerfile
# ORDEN OPTIMO: lo estático primero, lo dinámico al final
FROM python:3.12-slim

# Dependencias raramente cambian → primera capa
COPY requirements.txt /tmp/
RUN pip install --no-cache-dir -r /tmp/requirements.txt

# Código cambia frecuentemente → última capa
COPY src/ /app/src/
WORKDIR /app
CMD ["python", "src/main.py"]
```

Si pusieras `COPY src/ /app/src/` antes del `pip install`, cada cambio en tu código
forzaría reinstalar todas las dependencias.

## Storage driver: overlay2 internals

### Estructura de directorios

```
/var/lib/docker/
└── overlay2/
    ├── l/            # Links simbólicos con nombres cortos a diff/ de cada capa
    └── <hash>/
        ├── diff/     # Contenido de esta capa (los archivos reales)
        ├── link      # Nombre del link simbólico en l/ para esta capa
        ├── lower     # Lista de capas inferiores (separadas por :)
        ├── work/     # Directorio de trabajo para operaciones atómicas
        └── merged/   # Vista unificada (solo existe si el contenedor está corriendo)
```

El directorio `l/` contiene symlinks con nombres cortos. Esto es una optimización:
el argumento `lowerdir` del mount de OverlayFS tiene un límite de longitud (page size
del kernel), y con muchas capas apiladas las rutas completas podrían excederlo.

### El archivo "lower"

Cada capa tiene un archivo `lower` que lista sus capas inferiores separadas por `:`:

```bash
# Ver las capas de una imagen
docker image inspect debian:bookworm --format '{{json .RootFS.Layers}}' | python3 -m json.tool

# Ejemplo de salida:
[
  "sha256:a6d8...",  # capa más baja
  "sha256:b3c2...",  # capa encima
  "sha256:c4d7..."   # capa más alta
]
```

Para contenedores activos, puedes inspeccionar las rutas reales:

```bash
# Ver el LowerDir de un contenedor corriendo
docker inspect <container> --format '{{.GraphDriver.Data.LowerDir}}'

# Ver el UpperDir (donde están los cambios del contenedor)
docker inspect <container> --format '{{.GraphDriver.Data.UpperDir}}'

# Ver el MergedDir (la vista unificada)
docker inspect <container> --format '{{.GraphDriver.Data.MergedDir}}'
```

### Performance de overlay2 vs otros drivers

| Driver | Ventajas | Desventajas |
|--------|----------|-------------|
| overlay2 | Rendimiento sólido, manejo eficiente de muchos archivos | Requiere kernel 4.0+ |
| fuse-overlayfs | Funciona en modo rootless sin kernel overlay | Mayor overhead que overlay2 nativo |
| vfs | Funciona en cualquier filesystem, útil para debugging | Muy lento: copia completa por capa, sin CoW |
| btrfs/zfs | CoW nativo del filesystem, snapshots eficientes | Requiere que /var/lib/docker esté en btrfs/zfs |

## Verificar el storage driver y las capas

```bash
# Ver qué storage driver usa Docker
docker info | grep "Storage Driver"
# Storage Driver: overlay2

# Ver dónde se almacenan las capas en disco
docker info | grep "Docker Root Dir"
# Docker Root Dir: /var/lib/docker

# Las capas están en:
# /var/lib/docker/overlay2/<hash>/diff/   ← contenido de cada capa
# /var/lib/docker/overlay2/<hash>/merged/ ← vista unificada (solo contenedores activos)

# Ver espacio ocupado por imágenes, contenedores, volúmenes y build cache
docker system df
# TYPE            TOTAL     SIZE
# Images           15        2.1GB
# Containers       3         124MB
# Local Volumes    2         50MB
# Build Cache      0         0B

# Ver detalle de espacio real vs virtual por imagen
docker system df -v
```

## Práctica

### Ejercicio 1: Observar las capas de una imagen

```bash
# Descargar una imagen base
docker pull debian:bookworm

# Ver las capas y sus tamaños
docker history debian:bookworm
# IMAGE          CREATED        CREATED BY                          SIZE      COMMENT
# a6d8...        3 weeks ago    /bin/sh -c #(nop)  ADD file:0...    117MB
# <missing>      3 weeks ago    /bin/sh -c #(nop)  CMD ["bash"]     0B

# <missing> es normal: significa que la capa fue descargada, no construida localmente

# Ver las instrucciones completas (sin truncar)
docker history --no-trunc debian:bookworm

# Ver los hashes de las capas
docker image inspect debian:bookworm --format '{{json .RootFS.Layers}}' | python3 -m json.tool
```

### Ejercicio 2: Demostrar la inmutabilidad de la imagen

```bash
# Crear un contenedor y modificar un archivo
docker run --name test-cow debian:bookworm bash -c "echo 'modified' > /etc/hostname"

# La imagen original no se ve afectada
docker run --rm debian:bookworm cat /etc/hostname
# Muestra el hostname original, no "modified"

# Ver qué archivos cambió el contenedor (en su capa RW)
docker diff test-cow
# C /etc              ← C = Changed (directorio modificado)
# C /etc/hostname     ← C = Changed (archivo modificado)
# A = Added (archivo nuevo), D = Deleted (archivo borrado)

# Limpiar
docker rm test-cow
```

### Ejercicio 3: Comparar tamaño real vs virtual

```bash
# Descargar dos imágenes que comparten capas
docker pull debian:bookworm
docker pull python:3.12-bookworm   # basada en debian:bookworm

# docker images muestra el tamaño virtual de cada una
docker images | grep -E "debian|python"
# REPOSITORY    TAG              SIZE
# python        3.12-bookworm    1.2GB  ← tamaño aparente de la imagen completa
# debian        bookworm         117MB

# docker system df muestra el espacio real ocupado (con deduplicación)
docker system df -v | head -30

# El espacio real total es MENOR que la suma de los tamaños virtuales
# porque comparten las capas de Debian
```

### Ejercicio 4: El costo de eliminar en capas separadas

Crear un archivo `Dockerfile.bad`:

```dockerfile
FROM debian:bookworm
RUN dd if=/dev/zero of=/bigfile bs=1M count=50
RUN rm /bigfile
```

Y un archivo `Dockerfile.good`:

```dockerfile
FROM debian:bookworm
RUN dd if=/dev/zero of=/bigfile bs=1M count=50 && rm /bigfile
```

```bash
docker build -f Dockerfile.bad -t test-bad .
docker build -f Dockerfile.good -t test-good .

docker images | grep test-
# test-bad:  ~180MB (debian + 50MB del archivo que "se borró" pero sigue en la capa)
# test-good: ~120MB (debian, el archivo se creó y borró en la misma capa)

# Ver las capas de test-bad
docker history test-bad
# La capa del rm ocupa ~0B pero la capa del dd sigue ocupando 50MB

# Limpiar
docker rmi test-bad test-good
```

### Ejercicio 5: Inspeccionar capas de un contenedor activo

```bash
# Iniciar un contenedor
docker run -d --name inspect-test debian:bookworm sleep 1d

# Ver las capas desde el host
docker inspect inspect-test --format '{{.GraphDriver.Data.UpperDir}}'
# /var/lib/docker/overlay2/<hash>/diff

# Crear un archivo dentro del contenedor
docker exec inspect-test touch /tmp/hello

# Ver el archivo en el upperdir desde el host (requiere root)
# UpperDir ya apunta directamente al diff/ de la capa RW
UPPER=$(docker inspect inspect-test --format '{{.GraphDriver.Data.UpperDir}}')
sudo ls "$UPPER/tmp/"
# hello

# Borrar un archivo de la imagen para ver el whiteout
docker exec inspect-test rm /etc/motd

# El whiteout aparece como character device 0/0 en el upperdir
sudo ls -la "$UPPER/etc/" | grep wh
# c--------- 1 root root 0, 0 ... .wh.motd

# Limpiar
docker stop inspect-test && docker rm inspect-test
```

### Ejercicio 6: Caché de build en acción

Crear un directorio con un Dockerfile:

```bash
mkdir -p /tmp/cache-demo && cd /tmp/cache-demo

cat > Dockerfile << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*
RUN echo "version 1" > /app/version.txt
EOF

# Primera build: todas las capas se construyen
docker build -t cache-demo .

# Cambiar solo la última línea
sed -i 's/version 1/version 2/' Dockerfile

# Segunda build: las primeras capas usan caché
docker build -t cache-demo .
# Step 1/3 : FROM debian:bookworm
#  ---> Using cache
# Step 2/3 : RUN apt-get update...
#  ---> Using cache                  ← no se reconstruye
# Step 3/3 : RUN echo "version 2"
#  ---> Running in xxxxx             ← solo esta se reconstruye

# Limpiar
docker rmi cache-demo
rm -rf /tmp/cache-demo
```
