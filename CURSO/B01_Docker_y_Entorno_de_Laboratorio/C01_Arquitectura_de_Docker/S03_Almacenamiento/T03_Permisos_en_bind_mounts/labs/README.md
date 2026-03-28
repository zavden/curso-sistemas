# Lab -- Permisos en bind mounts

## Objetivo

Laboratorio practico para reproducir el problema clasico de permisos en bind
mounts, demostrar dos soluciones (`--user` y Dockerfile con USER + ARG),
y verificar el comportamiento de `COPY --chown` en la construccion de imagenes.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas: `docker pull alpine:latest` y `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.problem` | Imagen que corre como root para reproducir el problema |
| `Dockerfile.user-arg` | Imagen con USER + ARG para UID dinamico |
| `Dockerfile.copy-chown` | Demuestra COPY con y sin --chown |
| `sample-config.txt` | Archivo de ejemplo para copiar en el build |

---

## Parte 1 -- El problema de permisos

### Objetivo

Reproducir el problema clasico: un contenedor que corre como root crea archivos
en un bind mount, y esos archivos pertenecen a root en el host, impidiendo que
tu usuario los edite o elimine.

### Paso 1.1: Verificar tu UID en el host

```bash
id
```

Salida esperada (los valores exactos dependen de tu sistema):

```
uid=1000(<tu_usuario>) gid=1000(<tu_grupo>) ...
```

Toma nota de tu UID y GID. Los necesitaras para entender los resultados.

### Paso 1.2: Crear un directorio de prueba

```bash
mkdir -p /tmp/lab-perms
echo "created by host user" > /tmp/lab-perms/host-file.txt
```

### Paso 1.3: Verificar permisos del archivo del host

```bash
ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 1000 1000 ... host-file.txt
```

El archivo pertenece a UID 1000 (tu usuario).

### Paso 1.4: Crear un archivo desde un contenedor (como root)

```bash
docker run --rm -v /tmp/lab-perms:/app alpine sh -c \
    'echo "created by container as root" > /app/container-file.txt && id'
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root) ...
```

El contenedor corrio como root (UID 0).

### Paso 1.5: Comparar propietarios en el host

```bash
ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 0    0    ... container-file.txt
-rw-r--r-- 1 1000 1000 ... host-file.txt
```

`container-file.txt` pertenece a UID 0 (root). `host-file.txt` pertenece a
UID 1000 (tu usuario). El kernel no traduce UIDs entre host y contenedor:
son el mismo espacio de UIDs.

### Paso 1.6: Intentar modificar el archivo de root

```bash
echo "trying to append" >> /tmp/lab-perms/container-file.txt 2>&1
```

Salida esperada:

```
... Permission denied
```

Tu usuario (UID 1000) no puede escribir en un archivo propiedad de root
(UID 0). Tampoco puedes eliminarlo:

```bash
rm /tmp/lab-perms/container-file.txt 2>&1
```

Salida esperada:

```
rm: cannot remove '/tmp/lab-perms/container-file.txt': Permission denied
```

### Paso 1.7: Usar un Dockerfile que corre como root

```bash
docker build -f Dockerfile.problem -t lab-perm-problem .
docker run --rm -v /tmp/lab-perms:/app lab-perm-problem
```

Salida esperada:

```
File created successfully
```

```bash
ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 0    0    ... container-file.txt
-rw-r--r-- 1 1000 1000 ... host-file.txt
-rw-r--r-- 1 0    0    ... output.txt
```

Otro archivo mas propiedad de root. Cada archivo que genera un contenedor
que corre como root tendra este problema.

### Paso 1.8: Limpiar

```bash
sudo rm -rf /tmp/lab-perms
docker rmi lab-perm-problem
```

> `sudo` es necesario porque algunos archivos pertenecen a root.

---

## Parte 2 -- Solucion con `--user`

### Objetivo

Resolver el problema de permisos pasando `--user $(id -u):$(id -g)` al
contenedor, y observar el efecto secundario "I have no name!".

### Paso 2.1: Crear un directorio limpio

```bash
mkdir -p /tmp/lab-perms
```

### Paso 2.2: Ejecutar con `--user`

```bash
docker run --rm \
    --user $(id -u):$(id -g) \
    -v /tmp/lab-perms:/app \
    alpine sh -c 'echo "created with --user" > /app/user-file.txt && id'
```

Salida esperada:

```
uid=1000 gid=1000
```

Observa que `id` no muestra un nombre de usuario. El contenedor corre con
UID 1000, pero ese UID no existe en `/etc/passwd` del contenedor.

### Paso 2.3: Verificar permisos en el host

Antes de verificar, responde mentalmente:

- El contenedor corrio con UID 1000 (tu usuario). ¿A quien pertenecera
  el archivo `user-file.txt` en el host?

Intenta predecir antes de continuar al siguiente paso.

```bash
ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 1000 1000 ... user-file.txt
```

El archivo pertenece a UID 1000 (tu usuario). Puedes editarlo y eliminarlo
sin problemas:

```bash
echo "appended from host" >> /tmp/lab-perms/user-file.txt
cat /tmp/lab-perms/user-file.txt
```

Salida esperada:

```
created with --user
appended from host
```

### Paso 2.4: El warning "I have no name!"

```bash
docker run --rm -it --user $(id -u):$(id -g) alpine sh -c 'whoami 2>&1; echo "---"; id'
```

Salida esperada:

```
whoami: unknown uid 1000
---
uid=1000 gid=1000
```

El UID 1000 no tiene entrada en `/etc/passwd` del contenedor, por lo que
`whoami` no puede resolverlo. Esto es **cosmetico**: las operaciones de I/O
sobre el bind mount funcionan correctamente porque el kernel usa UIDs
numericos, no nombres.

### Paso 2.5: Limitacion de `--user` — no puede instalar paquetes

```bash
docker run --rm --user $(id -u):$(id -g) alpine sh -c 'apk add curl 2>&1 | head -5'
```

Salida esperada:

```
ERROR: Unable to lock database: Permission denied
ERROR: Failed to open apk database: Permission denied
```

Al correr como un usuario sin privilegios, el proceso no puede escribir en
directorios del sistema. Este es el trade-off de `--user`: resuelve los
permisos del bind mount, pero el contenedor pierde capacidad de administracion.

### Paso 2.6: Limpiar

```bash
rm -rf /tmp/lab-perms
```

> No se necesita `sudo` porque todos los archivos pertenecen a tu usuario.

---

## Parte 3 -- Solucion con Dockerfile (USER + ARG)

### Objetivo

Construir una imagen con un usuario cuyo UID se configura via build argument,
de forma que los archivos creados por el contenedor tengan la propiedad correcta
sin necesidad de `--user` en cada `docker run`.

### Paso 3.1: Examinar el Dockerfile

```bash
cat Dockerfile.user-arg
```

Observa:

- `ARG USER_UID=1000` y `ARG USER_GID=1000` permiten configurar el UID en
  build time.
- `addgroup` y `adduser` crean un usuario real con ese UID.
- `USER devuser` cambia el usuario por defecto del contenedor.

### Paso 3.2: Construir con tu UID

```bash
docker build \
    --build-arg USER_UID=$(id -u) \
    --build-arg USER_GID=$(id -g) \
    -f Dockerfile.user-arg \
    -t lab-perm-user-arg .
```

### Paso 3.3: Verificar el usuario dentro de la imagen

```bash
docker run --rm lab-perm-user-arg id
```

Salida esperada:

```
uid=1000(devuser) gid=1000(devuser) groups=1000(devuser)
```

A diferencia de `--user`, ahora el usuario tiene nombre (`devuser`), grupo,
y home directory. No aparece el warning "I have no name!".

### Paso 3.4: Crear archivos en un bind mount

```bash
mkdir -p /tmp/lab-perms

docker run --rm \
    -v /tmp/lab-perms:/app \
    lab-perm-user-arg \
    sh -c 'echo "created by devuser" > /app/devuser-file.txt'
```

### Paso 3.5: Verificar permisos en el host

```bash
ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 1000 1000 ... devuser-file.txt
```

El archivo pertenece a tu UID. Puedes editarlo y eliminarlo libremente.

### Paso 3.6: Verificar que whoami funciona

```bash
docker run --rm lab-perm-user-arg whoami
```

Salida esperada:

```
devuser
```

No hay warning "I have no name!" porque el usuario esta definido en
`/etc/passwd` de la imagen.

### Paso 3.7: Comparar ambas soluciones

Crea un archivo con cada metodo y compara:

```bash
# Method 1: --user flag
docker run --rm \
    --user $(id -u):$(id -g) \
    -v /tmp/lab-perms:/app \
    alpine sh -c 'echo "via --user" > /app/method1.txt'

# Method 2: Dockerfile with USER
docker run --rm \
    -v /tmp/lab-perms:/app \
    lab-perm-user-arg \
    sh -c 'echo "via Dockerfile USER" > /app/method2.txt'

ls -ln /tmp/lab-perms/
```

Salida esperada:

```
-rw-r--r-- 1 1000 1000 ... devuser-file.txt
-rw-r--r-- 1 1000 1000 ... method1.txt
-rw-r--r-- 1 1000 1000 ... method2.txt
```

Ambos metodos producen archivos con UID 1000. La diferencia es que el metodo
del Dockerfile no requiere `--user` en cada invocacion.

### Paso 3.8: Limpiar

```bash
rm -rf /tmp/lab-perms
docker rmi lab-perm-user-arg
```

---

## Parte 4 -- `COPY --chown` en Dockerfiles

### Objetivo

Demostrar que `COPY` sin `--chown` crea archivos propiedad de root en la
imagen, y que `COPY --chown` permite especificar el propietario correcto
durante el build.

### Paso 4.1: Examinar el Dockerfile

```bash
cat Dockerfile.copy-chown
```

Observa:

- `COPY --chown=devuser:devuser sample-config.txt /app/owned-by-devuser.txt`
  copia el archivo con propiedad de `devuser`.
- `COPY sample-config.txt /app/owned-by-root.txt` copia el mismo archivo
  sin especificar propietario.

Antes de construir, responde mentalmente:

- ¿A quien pertenecera `owned-by-root.txt`?
- ¿A quien pertenecera `owned-by-devuser.txt`?
- ¿Importa que la instruccion `USER devuser` esta despues de los `COPY`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2: Construir la imagen

```bash
docker build -f Dockerfile.copy-chown -t lab-perm-copy-chown .
```

### Paso 4.3: Verificar propiedad de los archivos

```bash
docker run --rm lab-perm-copy-chown
```

Salida esperada:

```
...
-rw-r--r-- 1 devuser devuser ... owned-by-devuser.txt
-rw-r--r-- 1 root    root    ... owned-by-root.txt
```

Analisis:

- `owned-by-devuser.txt`: propiedad de `devuser` gracias a `--chown`.
- `owned-by-root.txt`: propiedad de `root` porque `COPY` sin `--chown`
  siempre crea archivos como root, independientemente de la instruccion
  `USER`.

### Paso 4.4: Verificar que devuser puede leer ambos archivos

```bash
docker run --rm lab-perm-copy-chown sh -c \
    'cat /app/owned-by-devuser.txt && echo "---" && cat /app/owned-by-root.txt'
```

Salida esperada:

```
app_name=lab-permissions
version=1.0
debug=false
---
app_name=lab-permissions
version=1.0
debug=false
```

`devuser` puede leer ambos archivos porque los permisos son `644` (lectura
para todos). Pero solo podria **escribir** en `owned-by-devuser.txt`.

### Paso 4.5: Verificar que devuser no puede escribir en el archivo de root

```bash
docker run --rm lab-perm-copy-chown sh -c \
    'echo "test" >> /app/owned-by-root.txt 2>&1'
```

Salida esperada:

```
sh: can't create /app/owned-by-root.txt: Permission denied
```

```bash
docker run --rm lab-perm-copy-chown sh -c \
    'echo "test" >> /app/owned-by-devuser.txt && echo "write succeeded"'
```

Salida esperada:

```
write succeeded
```

### Paso 4.6: Implicacion practica

Sin `--chown`, un archivo copiado con `COPY` pertenece a root. Si la imagen
corre como usuario no-root (instruccion `USER`), ese usuario no podra
modificar los archivos copiados. Esto es un error comun en Dockerfiles de
produccion.

La regla es: si el contenedor corre como un usuario no-root, usa
`COPY --chown=usuario:grupo` para todos los archivos que ese usuario
necesite modificar.

### Paso 4.7: Limpiar

```bash
docker rmi lab-perm-copy-chown
```

---

## Limpieza final

```bash
# Remove containers (if any remain from skipped steps)
docker rm -f perm-problem perm-user-fix perm-dockerfile-fix 2>/dev/null

# Remove images
docker rmi lab-perm-problem lab-perm-user-arg lab-perm-copy-chown 2>/dev/null

# Remove host directories
sudo rm -rf /tmp/lab-perms /tmp/lab-chown 2>/dev/null
```

---

## Conceptos reforzados

1. Los **UIDs y GIDs** son compartidos entre host y contenedor. El kernel no
   traduce UIDs: UID 0 en el contenedor es UID 0 (root) en el host.
2. Un contenedor que corre como root (por defecto) crea archivos propiedad de
   root en bind mounts. El usuario del host no puede editarlos ni eliminarlos
   sin `sudo`.
3. La solucion `--user $(id -u):$(id -g)` es inmediata y no requiere modificar
   la imagen, pero produce el warning "I have no name!" y el proceso pierde
   capacidad de administracion dentro del contenedor.
4. Un Dockerfile con `ARG USER_UID` + `adduser` + `USER` produce una imagen
   donde el usuario existe en `/etc/passwd`, tiene home directory, y no
   requiere `--user` en cada ejecucion. La desventaja es que el UID queda
   fijado en build time.
5. `COPY` sin `--chown` crea archivos propiedad de root **siempre**,
   independientemente de la instruccion `USER`. Para que el usuario del
   contenedor pueda modificar archivos copiados, se debe usar
   `COPY --chown=usuario:grupo`.
6. Los **named volumes** no tienen este problema de permisos en la practica,
   porque no se accede directamente a sus archivos desde el host. El problema
   es especifico de **bind mounts** en Linux nativo.
