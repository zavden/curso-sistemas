# Lab — Ejecucion

## Objetivo

Laboratorio practico para verificar de forma empirica los modos de ejecucion
de contenedores Docker: flags de `docker run`, ejecucion de comandos con
`docker exec`, diferencias entre `attach` y `exec`, y sobrescritura del CMD.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `README.md` | Este documento: guia paso a paso |
| `Dockerfile.exec-demo` | Imagen con proceso de larga vida y procps para practicar exec |

---

## Parte 1 — Modos interactivo y detached

### Objetivo

Entender las diferencias entre ejecutar contenedores sin flags, con `-it`
(interactivo), con `-d` (detached), y el efecto de `-t` en la salida.

### Paso 1.1: Ejecucion sin flags

```bash
docker run --name run-default debian:bookworm echo "hello from container"
```

Salida esperada:

```
hello from container
```

Sin flags: Docker ejecuta el comando, muestra stdout, y el contenedor pasa
a `exited`. No se acepta input.

```bash
docker rm run-default
```

### Paso 1.2: Efecto de `-t` en la salida

```bash
# Sin -t: salida plana
docker run --rm debian:bookworm ls /usr
```

Salida esperada:

```
bin
games
include
lib
local
sbin
share
src
```

```bash
# Con -t: salida con formato de terminal
docker run --rm -t debian:bookworm ls /usr
```

Salida esperada:

```
bin  games  include  lib  local  sbin  share  src
```

Con `-t`, Docker asigna una pseudo-terminal (PTY). Los programas detectan
la terminal y ajustan el formato: `ls` usa multiples columnas, `grep`
colorea matches, etc.

### Paso 1.3: Modo interactivo (-it)

```bash
docker run -it --rm --name run-interactive debian:bookworm bash
```

Dentro del contenedor:

```bash
hostname
pwd
echo "dentro del contenedor"
ls /
exit
```

Con `-it`, tu terminal esta conectada al contenedor: lo que escribes se envia
al proceso, lo que el proceso imprime aparece en tu terminal.

### Paso 1.4: Modo detached (-d)

```bash
docker run -d --name run-detach debian:bookworm sleep 120
```

Salida esperada:

```
<container-id-hash>
```

Docker imprime el container ID y devuelve el control a tu terminal. El
contenedor esta corriendo en background.

```bash
docker ps --filter name=run-detach --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES        STATUS
run-detach   Up X seconds
```

### Paso 1.5: Piping de datos con `-i`

```bash
echo "hello from stdin" | docker run -i --rm debian:bookworm cat
```

Salida esperada:

```
hello from stdin
```

El flag `-i` mantiene stdin abierto, permitiendo enviar datos por pipe al
contenedor. Sin `-i`, el contenedor no puede recibir input.

```bash
# Sin -i: cat no recibe nada
echo "this will not arrive" | docker run --rm debian:bookworm cat
```

Salida esperada: (ninguna salida, cat no recibe el pipe).

### Paso 1.6: Limpiar

```bash
docker rm -f run-detach
```

---

## Parte 2 — Flags de configuracion

### Objetivo

Practicar `--rm`, `--name`, `-e`, `-w`, y la sobrescritura del CMD.

### Paso 2.1: `--rm` evita acumulacion

```bash
# Sin --rm: el contenedor queda en estado exited
docker run --name name-demo debian:bookworm echo "sin rm"
docker ps -a --filter name=name-demo --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES       STATUS
name-demo   Exited (0) X seconds ago
```

```bash
docker rm name-demo
```

```bash
# Con --rm: se elimina automaticamente
docker run --rm debian:bookworm echo "con rm"
docker ps -a --filter ancestor=debian:bookworm --filter status=exited -q | wc -l
```

El contenedor con `--rm` no deja rastro al terminar.

### Paso 2.2: Nombres duplicados

```bash
docker run --name name-demo debian:bookworm echo "first"
docker run --name name-demo debian:bookworm echo "second"
```

Salida esperada del segundo comando:

```
docker: Error response from daemon: Conflict. The container name "/name-demo" is already in use ...
```

Los nombres deben ser unicos. Incluso un contenedor detenido ocupa el nombre.

```bash
docker rm name-demo
```

### Paso 2.3: Variables de entorno con `-e`

```bash
docker run --rm --name env-demo \
    -e APP_NAME="Lab Docker" \
    -e APP_ENV=development \
    -e APP_DEBUG=true \
    debian:bookworm \
    bash -c 'echo "App: $APP_NAME"; echo "Env: $APP_ENV"; echo "Debug: $APP_DEBUG"'
```

Salida esperada:

```
App: Lab Docker
Env: development
Debug: true
```

### Paso 2.4: Directorio de trabajo con `-w`

Antes de ejecutar, predice: ¿que mostrara `pwd` si pasamos `-w /var/log`?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker run --rm -w /var/log debian:bookworm pwd
```

Salida esperada:

```
/var/log
```

`-w` sobrescribe el WORKDIR de la imagen. El proceso arranca en ese
directorio.

### Paso 2.5: Sobrescribir el CMD

```bash
# CMD por defecto de debian:bookworm es "bash"
# Todo lo que va despues de la imagen reemplaza CMD
docker run --rm debian:bookworm cat /etc/os-release
```

Salida esperada (parcial):

```
PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"
...
```

```bash
docker run --rm debian:bookworm uname -a
```

Salida esperada:

```
Linux <container-id> <kernel-version> ...
```

### Paso 2.6: Combinar flags

```bash
docker run --rm \
    --name env-demo \
    -e GREETING="Hola desde Docker" \
    -w /tmp \
    debian:bookworm \
    bash -c 'echo "$GREETING"; echo "Directorio: $(pwd)"; echo "Usuario: $(whoami)"'
```

Salida esperada:

```
Hola desde Docker
Directorio: /tmp
Usuario: root
```

### Paso 2.7: Ejecutar como otro usuario

```bash
docker run --rm --user nobody debian:bookworm id
```

Salida esperada:

```
uid=65534(nobody) gid=65534(nogroup) groups=65534(nogroup)
```

```bash
docker run --rm --user 1000:1000 debian:bookworm id
```

Salida esperada:

```
uid=1000 gid=1000 groups=1000
```

---

## Parte 3 — `docker exec`

### Objetivo

Ejecutar comandos dentro de contenedores que ya estan corriendo, usando
`docker exec` con sus diferentes flags.

### Paso 3.1: Construir la imagen de demo

```bash
cat Dockerfile.exec-demo
```

Examina el Dockerfile: instala `procps` (para `ps`) y crea un script
que imprime informacion al arrancar, luego ejecuta `sleep 600`.

```bash
docker build -f Dockerfile.exec-demo -t lab-exec-demo .
```

### Paso 3.2: Arrancar el contenedor

```bash
docker run -d --name exec-target lab-exec-demo
docker logs exec-target
```

Salida esperada:

```
PID 1 started at <timestamp>
Hostname: <container-id>
Working dir: /app
```

### Paso 3.3: Ejecutar un comando simple

```bash
docker exec exec-target hostname
```

Salida esperada:

```
<container-id>
```

`docker exec` crea un proceso nuevo dentro del contenedor que ya esta
corriendo. No afecta a PID 1.

### Paso 3.4: Shell interactiva con exec

```bash
docker exec -it exec-target bash
```

Dentro del contenedor:

```bash
ps aux
pwd
ls /
exit
```

Observa que `exit` en el `exec` **no detiene el contenedor**. PID 1 (sleep)
sigue corriendo.

```bash
# Confirmar que el contenedor sigue activo
docker ps --filter name=exec-target --format 'table {{.Names}}\t{{.Status}}'
```

### Paso 3.5: Exec con directorio de trabajo y variables de entorno

```bash
docker exec -w /etc exec-target pwd
```

Salida esperada:

```
/etc
```

```bash
docker exec -e MY_VAR=hello exec-target bash -c 'echo $MY_VAR'
```

Salida esperada:

```
hello
```

### Paso 3.6: Exec como otro usuario

```bash
docker exec exec-target id
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

```bash
docker exec -u nobody exec-target id
```

Salida esperada:

```
uid=65534(nobody) gid=65534(nogroup) groups=65534(nogroup)
```

### Paso 3.7: Exec falla en contenedores detenidos

```bash
docker run --name exec-custom debian:bookworm echo "done"
docker exec exec-custom ls
```

Salida esperada:

```
Error response from daemon: Container exec-custom is not running
```

`docker exec` requiere que el contenedor este en estado `running`. Para
inspeccionar contenedores detenidos, usar `docker cp` o `docker logs`.

### Paso 3.8: Compartir filesystem entre shells exec

```bash
# Crear un archivo desde exec
docker exec exec-target bash -c "echo 'from exec session' > /tmp/shared.txt"

# Leerlo desde otro exec
docker exec exec-target cat /tmp/shared.txt
```

Salida esperada:

```
from exec session
```

Todos los procesos dentro del contenedor comparten el mismo filesystem
(la misma capa de escritura).

### Paso 3.9: Limpiar

```bash
docker rm -f exec-target exec-custom
```

---

## Parte 4 — `attach` vs `exec`

### Objetivo

Demostrar la diferencia fundamental: `docker attach` conecta tu terminal a
PID 1 (proceso principal), mientras que `docker exec` crea un proceso nuevo
e independiente.

### Paso 4.1: Preparar un contenedor con shell interactiva

```bash
docker run -dit --name attach-demo debian:bookworm bash
```

Los flags `-dit` crean un contenedor en background con TTY e input abierto.
Esto permite adjuntarse despues con `docker attach`.

### Paso 4.2: Attach conecta a PID 1

```bash
docker attach attach-demo
```

Dentro del contenedor (este es PID 1 — bash):

```bash
echo "soy PID 1"
echo "si escribo exit, el contenedor se detiene"
```

Para desconectarse **sin detener** el contenedor, usa la secuencia de detach:
**Ctrl+P, Ctrl+Q** (presionar Ctrl+P, soltar, presionar Ctrl+Q).

Despues de la secuencia de detach:

```bash
docker ps --filter name=attach-demo --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES          STATUS
attach-demo    Up X seconds
```

El contenedor sigue corriendo porque no escribiste `exit`.

### Paso 4.3: Exec crea un proceso nuevo

```bash
docker run -d --name exec-compare debian:bookworm sleep 300

docker exec -it exec-compare bash
```

Dentro del contenedor:

```bash
ps aux
```

Salida esperada (parcial):

```
USER       PID   ...  COMMAND
root         1   ...  sleep 300
root        XX   ...  bash
```

Observa: `sleep 300` es PID 1, y `bash` es un proceso separado.

```bash
exit
```

```bash
# El contenedor sigue corriendo
docker ps --filter name=exec-compare --format 'table {{.Names}}\t{{.Status}}'
```

`exit` en un `exec` solo termina el proceso creado por exec. PID 1 no se
afecta.

### Paso 4.4: Attach a un proceso no interactivo

```bash
docker attach exec-compare
```

Este contenedor tiene `sleep 300` como PID 1. `docker attach` conecta tu
terminal a PID 1, pero sleep no genera output ni acepta input. Tu terminal
queda "colgada" viendo la salida de sleep (que no tiene).

Usa **Ctrl+C** para enviar SIGINT a PID 1, lo que detiene el contenedor.

```bash
docker ps -a --filter name=exec-compare --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES          STATUS
exec-compare   Exited (130) X seconds ago
```

Exit code 130 = 128 + 2 (SIGINT por Ctrl+C). Esto demuestra el peligro de
`attach`: Ctrl+C mata PID 1 y detiene el contenedor.

### Paso 4.5: Resumen de diferencias

| Aspecto | `docker attach` | `docker exec` |
|---|---|---|
| Conecta a | PID 1 (proceso principal) | Proceso nuevo e independiente |
| `exit` detiene el contenedor | Si (si PID 1 es shell) | No |
| Ctrl+C | SIGINT a PID 1 (puede detener contenedor) | SIGINT al proceso exec (contenedor no se afecta) |
| Desconectar sin matar | Ctrl+P, Ctrl+Q | No necesario (exit no mata el contenedor) |
| Uso recomendado | Casi nunca | Siempre |

**Regla practica**: usar `docker exec` siempre. Usar `docker attach` casi nunca.

### Paso 4.6: Limpiar

```bash
docker rm -f attach-demo exec-compare
docker rmi lab-exec-demo
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f run-default run-detach name-demo env-demo \
    exec-target exec-custom attach-demo exec-compare 2>/dev/null

# Eliminar imagen del lab
docker rmi lab-exec-demo 2>/dev/null
```

---

## Conceptos reforzados

1. `docker run` sin flags ejecuta el comando, muestra stdout, y no acepta
   input. `-it` crea una sesion interactiva, `-d` ejecuta en background.
2. El flag `-t` asigna una pseudo-terminal (PTY) que cambia el comportamiento
   de los programas: `ls` usa columnas, `grep` colorea, etc.
3. `--rm` elimina automaticamente el contenedor al terminar, evitando la
   acumulacion de contenedores en estado `exited`.
4. `-e` establece variables de entorno, `-w` cambia el directorio de trabajo,
   y todo lo que va despues de la imagen reemplaza el CMD.
5. `docker exec` crea un proceso nuevo dentro de un contenedor corriendo.
   `exit` en un exec **no detiene** el contenedor.
6. `docker attach` conecta tu terminal a PID 1. `exit` o Ctrl+C **detiene**
   el contenedor. Regla practica: usar `exec` siempre.
7. `docker exec` requiere que el contenedor este en estado `running`. Para
   contenedores detenidos, usar `docker cp` o `docker logs`.
