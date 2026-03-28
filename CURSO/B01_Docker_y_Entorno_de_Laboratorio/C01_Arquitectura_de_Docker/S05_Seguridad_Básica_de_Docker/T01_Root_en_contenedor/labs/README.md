# Lab — Root en contenedor

## Objetivo

Demostrar que los contenedores Docker corren como root por defecto, entender los
riesgos de seguridad que esto implica (especialmente con bind mounts), y
aplicar las soluciones: `--user` en la linea de comandos y la instruccion
`USER` en el Dockerfile.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas: `docker pull alpine:latest`, `docker pull debian:bookworm`, `docker pull nginx:latest`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.nonroot` | Imagen que crea y usa un usuario non-root |

---

## Parte 1 — Root por defecto

### Objetivo

Verificar que el usuario predeterminado en contenedores Docker es root (UID 0)
y demostrar lo que root puede hacer dentro de un contenedor.

### Paso 1.1: Verificar el usuario predeterminado

```bash
docker run --rm alpine id
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

```bash
docker run --rm alpine whoami
```

Salida esperada:

```
root
```

### Paso 1.2: Verificar con otra imagen base

```bash
docker run --rm debian:bookworm id
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

Antes de continuar, responde mentalmente:

- Si el usuario es root dentro del contenedor, ?puede instalar paquetes?
- ?Puede modificar archivos del sistema como `/etc/passwd`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3: Root puede instalar paquetes

```bash
docker run --rm alpine sh -c 'apk add --no-cache curl > /dev/null 2>&1 && curl --version | head -1'
```

Salida esperada (aproximada):

```
curl 8.x.x ...
```

La instalacion funciona porque el proceso corre como root.

### Paso 1.4: Root puede modificar archivos del sistema

```bash
docker run --rm alpine sh -c '
    echo "usuario-inyectado:x:0:0::/root:/bin/sh" >> /etc/passwd
    echo "--- Contenido de /etc/passwd (ultimas 3 lineas) ---"
    tail -3 /etc/passwd
'
```

Salida esperada:

```
--- Contenido de /etc/passwd (ultimas 3 lineas) ---
nobody:x:65534:65534:nobody:/:/sbin/nologin
guest:x:405:100:guest:/dev/null:/sbin/nologin
usuario-inyectado:x:0:0::/root:/bin/sh
```

Root puede modificar cualquier archivo del sistema dentro del contenedor.
Esto es esperable, pero el riesgo aumenta cuando se combinan bind mounts.

### Paso 1.5: Inspeccionar el UID del proceso principal

```bash
docker run -d --name root-check alpine sleep 120

docker exec root-check cat /proc/1/status | grep -E '^(Uid|Gid)'
```

Salida esperada:

```
Uid:    0       0       0       0
Gid:    0       0       0       0
```

Los cuatro valores (real, effective, saved, filesystem) son 0 — root completo
dentro del contenedor.

### Limpieza de Parte 1

```bash
docker rm -f root-check
```

---

## Parte 2 — Riesgo de root con bind mounts

### Objetivo

Demostrar que root en el contenedor puede modificar archivos del host cuando
se montan directorios con `-v`, y entender por que esto es un riesgo de
seguridad.

### Paso 2.1: Crear un archivo de prueba en el host

```bash
echo "datos originales del host" > /tmp/lab-root-test.txt
ls -la /tmp/lab-root-test.txt
```

Observa el propietario del archivo — es tu usuario.

### Paso 2.2: Predecir el resultado

Antes de ejecutar el siguiente comando, responde mentalmente:

- Si el contenedor corre como root (UID 0) y montamos el archivo con `-v`,
  ?puede el contenedor modificar el archivo del host?
- ?Que propietario tendran los cambios?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3: Root en contenedor modifica archivos del host

```bash
docker run --rm -v /tmp/lab-root-test.txt:/data/test.txt alpine \
    sh -c 'echo "SOBRESCRITO POR ROOT EN CONTENEDOR" > /data/test.txt'

cat /tmp/lab-root-test.txt
```

Salida esperada:

```
SOBRESCRITO POR ROOT EN CONTENEDOR
```

El contenedor, corriendo como root, sobrescribio el archivo del host. Si
en lugar de un archivo de prueba fuera un archivo de configuracion del
sistema, las consecuencias serian graves.

### Paso 2.4: Root puede crear archivos propiedad de root en el host

```bash
docker run --rm -v /tmp:/hostdir alpine \
    sh -c 'echo "creado por root" > /hostdir/lab-created-by-root.txt'

ls -la /tmp/lab-created-by-root.txt
```

Salida esperada (aproximada):

```
-rw-r--r-- 1 root root ... /tmp/lab-created-by-root.txt
```

El archivo fue creado con propietario root en el host. Tu usuario podria
no tener permisos para eliminarlo sin `sudo`.

### Paso 2.5: Proteccion con `:ro` (read-only)

```bash
docker run --rm -v /tmp/lab-root-test.txt:/data/test.txt:ro alpine \
    sh -c 'echo "intento de escritura" > /data/test.txt' 2>&1
```

Salida esperada:

```
sh: can't create /data/test.txt: Read-only file system
```

El flag `:ro` previene la escritura, pero requiere que el operador lo
recuerde en cada `docker run`. La solucion mas robusta es no correr como root.

### Limpieza de Parte 2

```bash
rm -f /tmp/lab-root-test.txt
sudo rm -f /tmp/lab-created-by-root.txt 2>/dev/null || rm -f /tmp/lab-created-by-root.txt
```

---

## Parte 3 — `--user` en docker run

### Objetivo

Usar la opcion `--user` para sobrescribir el usuario del contenedor desde
la linea de comandos, y observar las limitaciones de correr como non-root.

### Paso 3.1: Ejecutar como usuario 1000:1000

```bash
docker run --rm --user 1000:1000 alpine id
```

Salida esperada:

```
uid=1000 gid=1000
```

El contenedor corre como UID 1000 en lugar de root, independientemente de
lo que la imagen defina.

### Paso 3.2: Predecir las limitaciones

Antes de ejecutar los siguientes comandos, responde mentalmente:

- Como UID 1000, ?podra el contenedor instalar paquetes?
- ?Podra escribir en `/etc/`?
- ?Podra modificar archivos montados del host que pertenecen a tu usuario?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3: Non-root no puede instalar paquetes

```bash
docker run --rm --user 1000:1000 alpine \
    sh -c 'apk add --no-cache curl 2>&1 | head -3'
```

Salida esperada (aproximada):

```
ERROR: Unable to lock database: Permission denied
ERROR: Failed to open apk database: Permission denied
```

### Paso 3.4: Non-root no puede escribir en directorios del sistema

```bash
docker run --rm --user 1000:1000 alpine \
    sh -c 'touch /etc/test-file 2>&1'
```

Salida esperada:

```
touch: /etc/test-file: Permission denied
```

### Paso 3.5: Non-root SI puede escribir en `/tmp`

```bash
docker run --rm --user 1000:1000 alpine \
    sh -c 'echo "funciona" > /tmp/test.txt && cat /tmp/test.txt'
```

Salida esperada:

```
funciona
```

`/tmp` tiene permisos 1777 (world-writable), por lo que cualquier usuario
puede escribir en el.

### Paso 3.6: Bind mount con non-root protege el host

```bash
echo "datos del host" > /tmp/lab-user-test.txt

docker run --rm --user 1000:1000 -v /tmp/lab-user-test.txt:/data/test.txt alpine \
    sh -c 'echo "intento" > /data/test.txt 2>&1'

cat /tmp/lab-user-test.txt
```

Salida esperada:

```
datos del host
```

El archivo no fue modificado. Si el archivo en el host pertenece a tu
usuario (UID 1000), el contenedor como UID 1000 SI podra modificarlo.
El resultado depende de los permisos del archivo en el host.

### Paso 3.7: --user sobrescribe incluso imagenes que definen USER

```bash
docker run --rm --user 0:0 alpine id
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

`--user` siempre tiene prioridad sobre la instruccion USER del Dockerfile.
Esto es util para debugging, pero tambien significa que `--user root`
puede anular la seguridad de la imagen.

### Limpieza de Parte 3

```bash
rm -f /tmp/lab-user-test.txt
```

---

## Parte 4 — USER en Dockerfile

### Objetivo

Crear una imagen que define un usuario non-root con la instruccion USER,
y comparar el comportamiento con imagenes oficiales que implementan
drop de privilegios.

### Paso 4.1: Examinar el Dockerfile

```bash
cat Dockerfile.nonroot
```

Antes de construir, responde mentalmente:

- ?Que usuario ejecutara el CMD de esta imagen?
- ?Podra el proceso dentro del contenedor instalar paquetes?
- ?En que directorio arrancara el proceso?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.2: Construir la imagen

```bash
docker build -f Dockerfile.nonroot -t lab-nonroot .
```

### Paso 4.3: Verificar el usuario

```bash
docker run --rm lab-nonroot
```

Salida esperada:

```
uid=1000(appuser) gid=1000(appuser) groups=1000(appuser)
```

La imagen corre como `appuser` por defecto, no como root.

### Paso 4.4: Verificar las limitaciones

```bash
docker run --rm lab-nonroot sh -c 'apk add --no-cache wget 2>&1 | head -2'
```

Salida esperada (aproximada):

```
ERROR: Unable to lock database: Permission denied
ERROR: Failed to open apk database: Permission denied
```

```bash
docker run --rm lab-nonroot sh -c 'touch /etc/test 2>&1'
```

Salida esperada:

```
touch: /etc/test: Permission denied
```

El usuario non-root no puede instalar paquetes ni modificar archivos del
sistema. Esto es exactamente lo que queremos para una aplicacion en
produccion.

### Paso 4.5: Verificar que curl sigue disponible

```bash
docker run --rm lab-nonroot sh -c 'curl --version | head -1'
```

Salida esperada (aproximada):

```
curl 8.x.x ...
```

curl se instalo como root en una capa anterior a USER. El usuario non-root
puede usarlo pero no podria instalarlo.

### Paso 4.6: Inspeccionar la configuracion de usuario en la imagen

```bash
docker inspect lab-nonroot --format '{{.Config.User}}'
```

Salida esperada:

```
appuser
```

### Paso 4.7: Comparar con imagenes oficiales

```bash
echo "=== Nginx ==="
docker inspect nginx:latest --format 'User: "{{.Config.User}}"'

echo ""
echo "=== Alpine ==="
docker inspect alpine:latest --format 'User: "{{.Config.User}}"'
```

Salida esperada:

```
=== Nginx ===
User: ""

=== Alpine ===
User: ""
```

Ambas imagenes muestran User vacio — corren como root por defecto.
Sin embargo, nginx implementa drop de privilegios internamente: el
master process arranca como root para bindear el puerto 80, y los
worker processes corren como usuario `nginx`.

### Paso 4.8: Observar el drop de privilegios de nginx

```bash
docker run -d --name nginx-priv nginx:latest

docker exec nginx-priv ps aux
```

Salida esperada (aproximada):

```
PID   USER     TIME  COMMAND
    1 root      ...  nginx: master process nginx -g daemon off;
   29 nginx     ...  nginx: worker process
   30 nginx     ...  nginx: worker process
```

El master process es root (necesita bindear puerto 80). Los workers que
atienden las peticiones corren como `nginx` (non-root). Este es el patron
de drop de privilegios que usan las imagenes oficiales de servicios.

```bash
docker rm -f nginx-priv
```

---

## Limpieza final

```bash
# Eliminar imagenes del lab
docker rmi lab-nonroot 2>/dev/null

# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f root-check nginx-priv 2>/dev/null

# Eliminar archivos temporales
rm -f /tmp/lab-root-test.txt /tmp/lab-user-test.txt
sudo rm -f /tmp/lab-created-by-root.txt 2>/dev/null
```

---

## Conceptos reforzados

1. Por defecto, los contenedores Docker corren como **root (UID 0)**. Esto
   incluye alpine, debian, ubuntu y la mayoria de imagenes base.

2. Root en el contenedor puede **modificar archivos del host** cuando se
   montan directorios con `-v` sin `:ro`. Esto es un riesgo de seguridad
   real, no teorico.

3. La opcion `--user UID:GID` en `docker run` sobrescribe el usuario de la
   imagen. Es la forma mas rapida de ejecutar como non-root sin modificar
   la imagen.

4. La instruccion `USER` en el Dockerfile establece el usuario predeterminado
   para las instrucciones siguientes y para el CMD. Las instrucciones
   anteriores a USER (como `RUN apt-get install`) se ejecutan como root.

5. Las imagenes oficiales de servicios (nginx, postgres) implementan
   **drop de privilegios**: arrancan como root para tareas de inicializacion
   y cambian a non-root para el servicio real.

6. `--user` en `docker run` siempre tiene prioridad sobre USER en el
   Dockerfile. Esto permite debugging como root, pero tambien significa
   que un operador puede anular la seguridad de la imagen.
