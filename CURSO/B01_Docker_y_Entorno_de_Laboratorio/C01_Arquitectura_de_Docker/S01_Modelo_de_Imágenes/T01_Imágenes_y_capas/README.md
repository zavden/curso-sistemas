# T01 — Imágenes y capas

## ¿Qué es una imagen Docker?

Una imagen Docker es un paquete inmutable que contiene todo lo necesario para ejecutar
una aplicación: sistema de archivos, bibliotecas, dependencias, variables de entorno y
el comando a ejecutar. No contiene un kernel — los contenedores comparten el kernel del
host.

Internamente, una imagen no es un solo archivo. Es una **pila ordenada de capas de solo
lectura**, donde cada capa representa un cambio incremental sobre la anterior.

## Capas y Union Filesystem

### OverlayFS

El mecanismo que permite apilar capas se llama **union filesystem**. En Linux moderno,
Docker usa **OverlayFS** (overlay2) por defecto. Puedes verificarlo con:

```bash
docker info --format '{{.Driver}}'
# overlay2
```

OverlayFS combina dos directorios:
- **lowerdir**: capas de solo lectura (la imagen) — pueden ser múltiples, apiladas
- **upperdir**: capa de escritura (una sola, la del contenedor)
- **merged**: la vista unificada que ve el proceso dentro del contenedor

El proceso dentro del contenedor ve `merged` como si fuera un filesystem normal. No sabe
que detrás hay múltiples capas.

### Cómo se construyen las capas

Cada instrucción en un Dockerfile que modifica el filesystem crea una nueva capa:

```dockerfile
FROM debian:bookworm          # Capa 1: el filesystem base de Debian
RUN apt-get update            # Capa 2: los índices de paquetes actualizados
RUN apt-get install -y gcc    # Capa 3: gcc y sus dependencias instaladas
COPY hello.c /src/hello.c     # Capa 4: el archivo hello.c copiado
RUN gcc -o /src/hello /src/hello.c  # Capa 5: el binario compilado
```

Las instrucciones que solo establecen metadata (CMD, ENV, EXPOSE, LABEL, WORKDIR)
**no crean capas con contenido** — generan capas de 0 bytes que solo guardan configuración.

### Visualización de capas

```
┌──────────────────────────────┐
│  Capa de escritura           │  ← Solo existe en el contenedor (efímera)
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

Cuando un proceso dentro del contenedor lee un archivo, OverlayFS busca desde la capa
superior hacia abajo y devuelve la primera coincidencia.

Cuando un proceso **modifica** un archivo que está en una capa inferior (de solo lectura):

1. OverlayFS **copia** el archivo completo a la capa de escritura (upperdir)
2. La modificación se aplica sobre la copia
3. El archivo original en la capa inferior **no cambia**

Esto es **copy-on-write**: el costo de la copia solo se paga cuando hay una escritura,
no en cada lectura.

### Implicaciones prácticas de CoW

- **Modificar un archivo grande es costoso la primera vez**: si un contenedor modifica
  un archivo de 500MB de una capa inferior, OverlayFS copia los 500MB a la capa de
  escritura antes de aplicar el cambio — incluso si solo cambia 1 byte.

- **Eliminar un archivo no libera espacio**: al "borrar" un archivo de una capa inferior,
  OverlayFS crea un archivo especial llamado **whiteout** en la capa de escritura. El
  archivo original sigue existiendo en la capa inferior — solo se oculta.

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
10 veces — solo la primera vez. Las siguientes reutilizan las capas existentes.

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

Para datos que deben sobrevivir a la destrucción del contenedor, se usan **volúmenes**
(cubiertos en la Sección 3).

## El problema de la eliminación de archivos en capas

Un error común al crear Dockerfiles: instalar algo, usarlo, y luego eliminarlo en un
RUN separado, creyendo que se reduce el tamaño.

```dockerfile
# ❌ INCORRECTO — el archivo sigue ocupando espacio en la Capa 2
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc    # Capa 2: +200MB (gcc instalado)
RUN apt-get remove -y gcc && apt-get autoremove -y  # Capa 3: +whiteouts (gcc "borrado")
# Tamaño total: ~200MB (Capa 2 sigue ahí)
```

```dockerfile
# ✅ CORRECTO — todo en la misma capa
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc \
    && gcc -o /app/hello /src/hello.c \
    && apt-get remove -y gcc && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*
# La capa solo contiene el resultado neto: el binario compilado
```

La regla es: **lo que se instala y se desinstala debe estar en el mismo RUN** para que
la capa resultante solo contenga el cambio neto.

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
# /var/lib/docker/overlay2/<hash>/merged/ ← vista unificada (solo para contenedores activos)
```

## Práctica

### Ejercicio 1: Observar las capas de una imagen

```bash
# Descargar una imagen base
docker pull debian:bookworm

# Ver las capas y sus tamaños
docker history debian:bookworm

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

# docker system df muestra el espacio real ocupado (con deduplicación)
docker system df -v | head -20

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

# Limpiar
docker rmi test-bad test-good
```
