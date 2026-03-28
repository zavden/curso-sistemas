# Lab -- Volumenes

## Objetivo

Laboratorio practico para verificar de forma empirica los tres tipos de montaje
en Docker (named volumes, bind mounts, tmpfs), comparar las sintaxis `-v` y
`--mount`, y demostrar el comportamiento de volumenes anonimos, la copia inicial
de datos, y la comparticion de volumenes entre contenedores.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.anon-vol` | Imagen con instruccion VOLUME para demostrar volumenes anonimos |

---

## Parte 1 -- Named volumes

### Objetivo

Crear un named volume, escribir datos desde un contenedor, comprobar que los
datos persisten despues de eliminar el contenedor, y usar los comandos de
gestion `docker volume`.

### Paso 1.1: Crear un named volume

```bash
docker volume create lab-data
```

### Paso 1.2: Verificar que el volumen existe

```bash
docker volume ls
```

Salida esperada:

```
DRIVER    VOLUME NAME
local     lab-data
```

### Paso 1.3: Inspeccionar el volumen

```bash
docker volume inspect lab-data
```

Salida esperada:

```json
[
    {
        "CreatedAt": "<timestamp>",
        "Driver": "local",
        "Labels": null,
        "Mountpoint": "/var/lib/docker/volumes/lab-data/_data",
        "Name": "lab-data",
        "Options": null,
        "Scope": "local"
    }
]
```

Observa el campo `Mountpoint`: Docker almacena los datos del volumen en
`/var/lib/docker/volumes/lab-data/_data`. Este directorio es gestionado por
Docker y no deberias manipularlo directamente.

### Paso 1.4: Escribir datos en el volumen desde un contenedor

```bash
docker run --rm -v lab-data:/data alpine sh -c \
    'echo "persistent data from container" > /data/test.txt && cat /data/test.txt'
```

Salida esperada:

```
persistent data from container
```

El contenedor escribio un archivo en `/data/test.txt`. Como `/data` esta
montado sobre el named volume `lab-data`, el archivo se guardo en el volumen,
no en la capa de escritura del contenedor.

### Paso 1.5: Comprobar persistencia con un nuevo contenedor

Antes de ejecutar el siguiente comando, responde mentalmente:

- El contenedor anterior fue creado con `--rm` (ya no existe). Los datos
  escritos en `/data/test.txt`, ¿siguen disponibles o se perdieron?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run --rm -v lab-data:/data alpine cat /data/test.txt
```

Salida esperada:

```
persistent data from container
```

Los datos persisten porque estan en el named volume, no en la capa de escritura
del contenedor. El contenedor desaparecio, pero el volumen sobrevive.

### Paso 1.6: Agregar mas datos desde otro contenedor

```bash
docker run --rm -v lab-data:/data alpine sh -c \
    'echo "second entry" >> /data/test.txt && echo "another file" > /data/notes.txt'
```

### Paso 1.7: Verificar ambos archivos

```bash
docker run --rm -v lab-data:/data alpine sh -c \
    'echo "=== test.txt ===" && cat /data/test.txt && echo "" && echo "=== notes.txt ===" && cat /data/notes.txt'
```

Salida esperada:

```
=== test.txt ===
persistent data from container
second entry

=== notes.txt ===
another file
```

Multiples contenedores pueden escribir y leer el mismo volumen a lo largo del
tiempo. El volumen funciona como almacenamiento externo al ciclo de vida de
los contenedores.

### Paso 1.8: Limpiar

```bash
docker volume rm lab-data
```

Verifica que se elimino:

```bash
docker volume ls
```

El volumen `lab-data` ya no debe aparecer en la lista.

---

## Parte 2 -- Bind mounts

### Objetivo

Montar un directorio del host dentro de un contenedor, comprobar que los
cambios son bidireccionales (del host al contenedor y viceversa), y probar
el modo solo lectura.

### Paso 2.1: Crear un directorio de prueba en el host

```bash
mkdir -p /tmp/lab-bind
echo "created on host" > /tmp/lab-bind/host-file.txt
```

### Paso 2.2: Montar el directorio en un contenedor

```bash
docker run --rm -v /tmp/lab-bind:/data alpine ls -la /data/
```

Salida esperada:

```
total 4
...
-rw-r--r--    1 ...    ...    16 ... host-file.txt
```

El contenedor ve el archivo creado en el host. El bind mount mapea
directamente el directorio del host en el filesystem del contenedor.

### Paso 2.3: Leer el archivo del host desde el contenedor

```bash
docker run --rm -v /tmp/lab-bind:/data alpine cat /data/host-file.txt
```

Salida esperada:

```
created on host
```

### Paso 2.4: Crear un archivo desde el contenedor

```bash
docker run --rm -v /tmp/lab-bind:/data alpine sh -c \
    'echo "created in container" > /data/container-file.txt'
```

### Paso 2.5: Verificar en el host

Antes de ejecutar el siguiente comando, responde mentalmente:

- El archivo `container-file.txt` fue creado por un contenedor que ya no
  existe (uso `--rm`). ¿Existe el archivo en el host?

Intenta predecir antes de continuar al siguiente paso.

```bash
ls -la /tmp/lab-bind/
cat /tmp/lab-bind/container-file.txt
```

Salida esperada:

```
...
-rw-r--r-- 1 root root ... container-file.txt
-rw-r--r-- 1 ...  ...  ... host-file.txt
...
created in container
```

El archivo existe en el host. Los bind mounts son bidireccionales: lo que el
contenedor escribe aparece en el host y viceversa.

Observa que `container-file.txt` pertenece a `root:root`. Este es el problema
de permisos que se aborda en T03_Permisos_en_bind_mounts.

### Paso 2.6: Modo solo lectura

```bash
docker run --rm -v /tmp/lab-bind:/data:ro alpine sh -c \
    'echo "trying to write" > /data/readonly-test.txt'
```

Salida esperada:

```
sh: can't create /data/readonly-test.txt: Read-only file system
```

El flag `:ro` impide que el contenedor escriba en el bind mount. Esto es util
para montar configuracion o codigo fuente que no debe ser modificado por el
contenedor.

### Paso 2.7: Verificar que la lectura sigue funcionando en modo ro

```bash
docker run --rm -v /tmp/lab-bind:/data:ro alpine cat /data/host-file.txt
```

Salida esperada:

```
created on host
```

Read-only bloquea escrituras, no lecturas.

### Paso 2.8: Limpiar

```bash
sudo rm -rf /tmp/lab-bind
```

> Nota: se usa `sudo` porque el contenedor creo archivos como root. Este
> problema se explora en detalle en T03_Permisos_en_bind_mounts.

---

## Parte 3 -- tmpfs mounts

### Objetivo

Comprobar que un tmpfs mount almacena datos en memoria RAM y que los datos
desaparecen completamente al detener el contenedor.

### Paso 3.1: Crear un contenedor con tmpfs

```bash
docker run -d --name tmpfs-test --tmpfs /cache:size=10m alpine sleep 300
```

### Paso 3.2: Verificar el filesystem tmpfs

```bash
docker exec tmpfs-test df -h /cache
```

Salida esperada (aproximada):

```
Filesystem                Size      Used Available Use% Mounted on
tmpfs                    10.0M         0     10.0M   0% /cache
```

El filesystem esta montado como `tmpfs` con el tamano solicitado de 10MB.

### Paso 3.3: Escribir datos en tmpfs

```bash
docker exec tmpfs-test sh -c \
    'echo "sensitive data in memory only" > /cache/secret.txt && cat /cache/secret.txt'
```

Salida esperada:

```
sensitive data in memory only
```

### Paso 3.4: Verificar que no hay nada en disco

```bash
docker inspect tmpfs-test --format '{{json .Mounts}}' | python3 -m json.tool
```

Salida esperada:

```json
[]
```

La lista de mounts esta vacia porque tmpfs no se registra como un mount
persistente. Los datos estan solo en RAM.

### Paso 3.5: Detener y reiniciar el contenedor

Antes de ejecutar, responde mentalmente:

- Si detenemos el contenedor y lo reiniciamos con `docker start`, ¿seguira
  existiendo `/cache/secret.txt`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker stop tmpfs-test
docker start tmpfs-test
docker exec tmpfs-test cat /cache/secret.txt 2>&1
```

Salida esperada:

```
cat: can't open '/cache/secret.txt': No such file or directory
```

A diferencia de la capa de escritura (que sobrevive a `stop` + `start`), tmpfs
se vacia completamente al detener el contenedor. Los datos existieron solo en
memoria mientras el contenedor estaba en ejecucion.

### Paso 3.6: Comparar con la capa de escritura

```bash
# Write to the writable layer (NOT tmpfs)
docker exec tmpfs-test sh -c 'echo "in writable layer" > /tmp/layer-data.txt'

# Stop and restart
docker stop tmpfs-test
docker start tmpfs-test

# Writable layer survives, tmpfs does not
docker exec tmpfs-test cat /tmp/layer-data.txt
```

Salida esperada:

```
in writable layer
```

Conclusion: la capa de escritura sobrevive a `stop`/`start`, tmpfs no.

### Paso 3.7: Limpiar

```bash
docker rm -f tmpfs-test
```

---

## Parte 4 -- `-v` vs `--mount` y volumenes anonimos

### Objetivo

Demostrar la diferencia critica entre `-v` y `--mount` cuando el path de
origen no existe, y observar como la instruccion VOLUME en un Dockerfile
genera volumenes anonimos que se acumulan.

### Paso 4.1: `-v` con path inexistente (bind mount)

```bash
docker run --rm -v /tmp/lab-no-existe-xyz:/data alpine ls -la /data/
```

Salida esperada:

```
total 0
```

El directorio `/data/` dentro del contenedor esta vacio. Ahora verifica en el
host:

```bash
ls -la /tmp/lab-no-existe-xyz
```

Salida esperada:

```
total 0
drwxr-xr-x 2 root root ... .
...
```

Con `-v`, Docker creo el directorio `/tmp/lab-no-existe-xyz` silenciosamente en
el host. Si hubiera un typo en tu path, no recibirias ningun error.

```bash
sudo rm -rf /tmp/lab-no-existe-xyz
```

### Paso 4.2: `--mount` con path inexistente (bind mount)

```bash
docker run --rm --mount type=bind,source=/tmp/lab-no-existe-abc,target=/data alpine ls /data/
```

Salida esperada:

```
docker: Error response from daemon: invalid mount config for type "bind":
bind source path does not exist: /tmp/lab-no-existe-abc.
```

Con `--mount`, Docker falla inmediatamente con un error explicito. Esto
previene errores silenciosos por typos en paths.

### Paso 4.3: Comparar la misma operacion con named volumes

```bash
# -v creates the volume if it doesn't exist
docker run --rm -v lab-auto-vol:/data alpine sh -c 'echo "created" > /data/test.txt'

# --mount also creates the volume if it doesn't exist (same behavior for volumes)
docker run --rm --mount type=volume,source=lab-auto-mount,target=/data alpine sh -c 'echo "created" > /data/test.txt'
```

```bash
docker volume ls | grep lab-auto
```

Salida esperada:

```
local     lab-auto-mount
local     lab-auto-vol
```

Para named volumes, ambas sintaxis crean el volumen automaticamente si no
existe. La diferencia critica solo aplica a bind mounts.

```bash
docker volume rm lab-auto-vol lab-auto-mount
```

### Paso 4.4: Examinar el Dockerfile con instruccion VOLUME

```bash
cat Dockerfile.anon-vol
```

Antes de construir, responde mentalmente:

- ¿Que pasara cada vez que ejecutes `docker run` con esta imagen sin
  especificar un volumen con `-v`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.5: Construir y ejecutar la imagen

```bash
docker build -f Dockerfile.anon-vol -t lab-anon-vol .
```

```bash
docker run --name anon-vol-test lab-anon-vol
```

Salida esperada:

```
initial data from image
```

### Paso 4.6: Inspeccionar los volumenes creados

```bash
docker inspect anon-vol-test --format '{{json .Mounts}}' | python3 -m json.tool
```

Salida esperada (aproximada):

```json
[
    {
        "Type": "volume",
        "Name": "<hash-largo>",
        "Source": "/var/lib/docker/volumes/<hash-largo>/_data",
        "Destination": "/app/data",
        "Driver": "local",
        "Mode": "",
        "RW": true,
        "Propagation": ""
    }
]
```

Docker creo un volumen anonimo automaticamente. El nombre es un hash largo
e ilegible.

### Paso 4.7: Demostrar acumulacion de volumenes anonimos

```bash
docker rm anon-vol-test

# Run 3 more instances
docker run --rm lab-anon-vol
docker run --rm lab-anon-vol
docker run --rm lab-anon-vol
```

```bash
docker volume ls
```

Observa que se crearon multiples volumenes con nombres hash. Estos son
volumenes anonimos huerfanos: los contenedores que los crearon ya no existen,
pero los volumenes persisten.

### Paso 4.8: Identificar volumenes dangling

```bash
docker volume ls -f dangling=true
```

Los volumenes dangling son los que no estan asociados a ningun contenedor.
Se acumulan con el tiempo y consumen espacio en disco.

### Paso 4.9: Limpiar

```bash
docker rmi lab-anon-vol
docker volume prune -f
```

---

## Parte 5 -- Copia inicial y volumenes compartidos

### Objetivo

Demostrar el comportamiento de copia inicial de named volumes (cuando se montan
sobre un directorio que ya contiene datos en la imagen) y compartir un volumen
entre dos contenedores simultaneos.

### Paso 5.1: Copia inicial con named volume

La imagen `nginx` tiene archivos en `/usr/share/nginx/html` (por ejemplo,
`index.html` y `50x.html`).

Antes de ejecutar, responde mentalmente:

- Si montas un named volume nuevo y vacio sobre
  `/usr/share/nginx/html`, ¿que contendra el volumen despues?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run --rm -v lab-nginx-data:/usr/share/nginx/html alpine ls /usr/share/nginx/html
```

Salida esperada:

```
(vacio — alpine no tiene esos archivos)
```

Primero, comparemos: cuando la imagen no tiene datos en el punto de montaje,
el volumen queda vacio.

```bash
docker volume rm lab-nginx-data
```

Ahora con nginx:

```bash
docker run --rm -v lab-nginx-data:/usr/share/nginx/html nginx ls /usr/share/nginx/html
```

Salida esperada:

```
50x.html
index.html
```

Docker copio el contenido preexistente de `/usr/share/nginx/html` (de la
imagen nginx) al named volume `lab-nginx-data`. Este comportamiento se llama
**copia inicial** y solo ocurre con named volumes vacios.

### Paso 5.2: La copia inicial no sobrescribe datos existentes

```bash
# The volume already has data from the previous step
docker run --rm -v lab-nginx-data:/usr/share/nginx/html nginx sh -c \
    'echo "custom page" > /usr/share/nginx/html/custom.html && ls /usr/share/nginx/html'
```

Salida esperada:

```
50x.html
custom.html
index.html
```

Ahora eliminamos el contenedor y creamos otro con el mismo volumen:

```bash
docker run --rm -v lab-nginx-data:/usr/share/nginx/html nginx ls /usr/share/nginx/html
```

Salida esperada:

```
50x.html
custom.html
index.html
```

El archivo `custom.html` sigue ahi. La copia inicial solo ocurre la primera
vez que se monta un volumen vacio. Despues, el volumen conserva sus datos y
la imagen no los sobrescribe.

### Paso 5.3: Bind mount NO hace copia inicial

```bash
mkdir -p /tmp/lab-empty-dir
docker run --rm -v /tmp/lab-empty-dir:/usr/share/nginx/html nginx ls /usr/share/nginx/html
```

Salida esperada:

```
(vacio)
```

Con bind mount, el contenido original de la imagen queda **oculto** bajo
el directorio montado. No hay copia inicial.

```bash
sudo rm -rf /tmp/lab-empty-dir
docker volume rm lab-nginx-data
```

### Paso 5.4: Compartir un volumen entre dos contenedores

```bash
docker volume create lab-shared
```

Inicia un contenedor que escribe datos periodicamente:

```bash
docker run -d --name shared-writer -v lab-shared:/data alpine sh -c \
    'i=1; while [ $i -le 5 ]; do echo "entry $i: $(date)" >> /data/log.txt; sleep 2; i=$((i + 1)); done'
```

Espera unos segundos para que el writer produzca datos, y luego lee desde otro
contenedor:

```bash
docker run --rm -v lab-shared:/data alpine cat /data/log.txt
```

Salida esperada (aproximada):

```
entry 1: Mon Jan 15 10:30:00 UTC 2024
entry 2: Mon Jan 15 10:30:02 UTC 2024
entry 3: Mon Jan 15 10:30:04 UTC 2024
...
```

Ambos contenedores acceden al mismo volumen. El writer escribe, y cualquier
otro contenedor que monte el mismo volumen puede leer esos datos en tiempo
real.

### Paso 5.5: Limpiar

```bash
docker rm -f shared-writer
docker volume rm lab-shared
```

---

## Limpieza final

```bash
# Remove containers (if any remain from skipped steps)
docker rm -f tmpfs-test anon-vol-test shared-writer 2>/dev/null

# Remove images
docker rmi lab-anon-vol 2>/dev/null

# Remove volumes
docker volume rm lab-data lab-shared lab-nginx-data lab-auto-vol lab-auto-mount 2>/dev/null

# Remove dangling anonymous volumes
docker volume prune -f

# Remove host directories
sudo rm -rf /tmp/lab-bind /tmp/lab-no-existe-xyz /tmp/lab-empty-dir
```

---

## Conceptos reforzados

1. Los **named volumes** persisten independientemente del ciclo de vida de los
   contenedores. Un `docker rm` destruye el contenedor pero no el volumen.
2. Los **bind mounts** son bidireccionales: los cambios del contenedor aparecen
   en el host y viceversa. Los archivos creados por un contenedor que corre
   como root pertenecen a `root:root` en el host.
3. Los **tmpfs mounts** almacenan datos solo en memoria RAM. Los datos
   desaparecen al detener el contenedor, a diferencia de la capa de escritura
   que sobrevive a `stop`/`start`.
4. Con `-v`, un bind mount a un path inexistente del host **crea el directorio
   silenciosamente**. Con `--mount`, Docker **falla con un error explicito**.
5. La instruccion `VOLUME` en un Dockerfile genera volumenes anonimos en cada
   `docker run`. Estos volumenes se acumulan y consumen espacio si no se
   eliminan con `docker rm -v` o `docker volume prune`.
6. Cuando un **named volume vacio** se monta sobre un directorio que ya contiene
   datos en la imagen, Docker copia esos datos al volumen (copia inicial). Los
   bind mounts no hacen copia inicial: ocultan el contenido original.
7. Multiples contenedores pueden montar el **mismo named volume** para compartir
   datos. No hay mecanismo de locking integrado; la sincronizacion es
   responsabilidad de la aplicacion.
