# T02 — Ejecución

## `docker run`

`docker run` es el comando más usado de Docker. Internamente equivale a tres operaciones:

```
docker run = docker create + docker start + docker attach (si -it)
```

Crea un contenedor a partir de una imagen, lo inicia, y opcionalmente conecta tu
terminal a él.

```bash
# Forma básica
docker run debian:bookworm echo "hello world"

# Forma completa con flags
docker run -it --rm --name dev -v $(pwd):/app -w /app -p 8080:80 debian:bookworm bash
```

### Órdenes de argumentos

El orden de los argumentos en `docker run` importa:

```
docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
```

- **OPTIONS** van antes de la imagen (flags como `-d`, `-it`, `--name`, `-e`, etc.)
- **COMMAND** y **ARG** van después de la imagen y sobrescriben el CMD del Dockerfile

```bash
# Los flags (-it) van antes de la imagen
docker run -it debian:bookworm bash

# COMMAND va después de la imagen
docker run debian:bookworm echo hello
docker run debian:bookworm python --version

# Equivocarse es común:
docker run debian:bookworm -it bash   # INCORRECTO - "-it bash" se interpreta como COMMAND
# En este caso funciona porque la imagen permite ejecutar "bash" como comando
# Pero con nginx:
docker run nginx:latest -it bash       # INCORRECTO - nginx no tiene bash
docker run -it nginx:latest bash        # CORRECTO
```

## Flags críticos

### `-i` (interactive)

Mantiene STDIN abierto, incluso si no está adjunto. Sin `-i`, el proceso no puede
recibir input.

```bash
# Sin -i: el proceso no recibe input
docker run debian:bookworm cat
# Se queda colgado pero no puedes escribir nada — Ctrl+C para salir

# Con -i: puedes enviar datos por stdin
echo "hello" | docker run -i debian:bookworm cat
# hello
```

El flag `-i` hace que el STDIN del contenedor permanezca abierto, permitiendo
redirección de entrada.

### `-t` (tty)

Asigna una pseudo-terminal (PTY). Los programas detectan si están conectados a una
terminal y ajustan su comportamiento:

```bash
# Sin -t: ls no muestra colores, salida en una sola columna
docker run debian:bookworm ls /usr
# bin  games  include  lib  local  sbin  share  src

# Con -t: ls muestra colores y formato multi-columna
docker run -t debian:bookworm ls /usr
# bin  games  include  lib  local  sbin  share  src  (con colores ANSI)
```

Otros efectos de `-t`:

- `grep` muestra matches coloreados
- Python deshabilita el output buffering (muestra `print()` inmediatamente)
- Programas interactivos (vim, less, top) requieren `-t` para funcionar
- El prompt de bash requiere `-t` para mostrarse correctamente
- Programas que usan ncurses ( interfaces de texto) necesitan `-t`

### `-it` juntos

La combinación estándar para sesiones interactivas:

```bash
# Shell interactiva en Debian
docker run -it debian:bookworm bash
root@abc123:/# echo "dentro del contenedor"
root@abc123:/# exit

# Shell interactiva en AlmaLinux
docker run -it almalinux:9 bash
```

`-it` sin `-d`: tu terminal queda **adjunto** al contenedor. Lo que escribes se envía
al proceso, lo que el proceso imprime se muestra en tu terminal. Ctrl+C envía SIGINT
a PID 1.

### `-d` (detach)

Ejecuta el contenedor en background. Docker imprime el container ID y devuelve el
control al terminal.

```bash
# Ejecutar en background
docker run -d --name web nginx:latest
# a1b2c3d4e5f6... (container ID)

# El contenedor está corriendo
docker ps
# CONTAINER ID   IMAGE   ...   STATUS          NAMES
# a1b2c3d4e5f6   nginx   ...   Up 5 seconds    web

# Ver la salida con logs
docker logs web

# Ver logs en tiempo real
docker logs -f web

# Detener
docker stop web && docker rm web
```

### Combinaciones de `-i`, `-t`, `-d`

| Flags | Comportamiento | Uso |
|-------|---------------|-----|
| (ninguno) | Ejecuta, muestra stdout, no acepta input | Comandos one-shot sin interacción |
| `-i` | Acepta input por stdin, sin PTY | Piping datos al contenedor |
| `-t` | PTY asignada, sin input | Procesos que necesitan TTY para formatear output |
| `-it` | Shell interactiva completa | Sesiones de trabajo, debugging |
| `-d` | Background, sin output en terminal | Servidores, servicios de larga vida |
| `-dit` | Background con PTY, adjuntable después | Contenedores que se reusean con `docker attach` |

### `--rm`

Elimina automáticamente el contenedor cuando PID 1 termina. Sin `--rm`, los contenedores
detenidos se acumulan:

```bash
# Sin --rm: el contenedor queda en estado "exited"
docker run --name acumula debian:bookworm echo "done"
docker ps -a | grep acumula
# ... Exited (0) ...

docker rm acumula   # Limpieza manual

# Con --rm: se elimina automáticamente
docker run --rm debian:bookworm echo "done"
docker ps -a | grep debian
# (nada — se eliminó solo)
```

Regla general:

- Contenedores efímeros (compilar, ejecutar un script, probar algo): `--rm`
- Contenedores que necesitas inspeccionar después (debug, ver logs): sin `--rm`

`--rm` no se puede combinar con `--restart always` (o cualquier restart diferente de `no`),
porque el reinicio automático requiere que el contenedor exista después de terminar.

### `--name`

Asigna un nombre legible al contenedor:

```bash
docker run --name my-server nginx:latest

# Sin --name, Docker genera nombres aleatorios
docker run nginx:latest
docker ps
# ... festive_einstein  (nombre generado: adjetivos + científico)

# Nombres generados siguen un patrón: adjetivo_sustantivo
# ejemplos: romantic_euclid, tender_volhard, festive_shannon
```

Los nombres deben ser únicos. Si un contenedor con ese nombre ya existe (incluso
detenido), `docker run --name` falla:

```bash
docker run --name test debian:bookworm echo "first"
docker run --name test debian:bookworm echo "second"
# Error: Conflict. The container name "/test" is already in use

# Solución: eliminar el anterior, o usar --name para uno y nombre auto para otro
docker rm test
```

### `-e` (environment)

Establece variables de entorno dentro del contenedor:

```bash
# Una variable
docker run --rm -e MY_VAR=hello debian:bookworm env | grep MY_VAR

# Múltiples variables
docker run --rm -e MY_VAR=hello -e DB_HOST=localhost debian:bookworm env

# Pasar una variable del host (exportada)
export HOST_VAR="from-host"
docker run --rm -e HOST_VAR debian:bookworm bash -c 'echo $HOST_VAR'
# from-host

# Si la variable no se exporta, usar directamente el valor
docker run --rm -e HOST_VAR="$HOST_VAR" debian:bookworm bash -c 'echo $HOST_VAR'

# Con archivos .env (docker-compose style en CLI)
# No disponible directamente en docker run, pero puedes usar:
docker run --rm --env-file .env debian:bookworm env
```

#### Variables de entorno reservadas de Docker

| Variable | Efecto |
|----------|--------|
| `HOME` | Establece home del usuario |
| `HOSTNAME` | Nombre del contenedor |
| `PATH` | PATH del contenedor |
| `TERM` | Tipo de terminal (xterm) |

### `-p` (publish)

Mapea puertos del host al contenedor:

```bash
# Puerto 8080 del host → puerto 80 del contenedor
docker run -d -p 8080:80 --name web nginx:latest
curl http://localhost:8080
# (respuesta de nginx)

# Bind a una IP específica del host
docker run -d -p 127.0.0.1:8080:80 --name web-local nginx:latest
# Solo accesible desde localhost

# Puerto UDP
docker run -d -p 53:53/udp --name dns server:udp

# Puertos aleatorios (Docker asigna)
docker run -d -p 80 --name web-dynamic nginx:latest
docker port web-dynamic
# 80/tcp -> 0.0.0.0:32768

docker stop web web-local web-dynamic && docker rm web web-local web-dynamic
```

Se cubre en detalle en S04 (Redes).

### `-v` (volume)

Monta volúmenes o directorios del host:

```bash
# Bind mount: directorio del host montado en el contenedor
docker run --rm -v $(pwd):/app debian:bookworm ls /app

# Named volume
docker run --rm -v mydata:/data debian:bookworm ls /data

# Bind mount con permisos de solo lectura
docker run --rm -v $(pwd):/app:ro debian:bookworm ls /app

# Montar un solo archivo
docker run --rm -v $(pwd)/config.yml:/app/config.yml:ro debian:bookworm cat /app/config.yml
```

Se cubre en detalle en S03 (Almacenamiento).

### `-w` (workdir)

Establece el directorio de trabajo (sobrescribe el WORKDIR de la imagen):

```bash
docker run --rm -w /tmp debian:bookworm pwd
# /tmp

# Combinado con -v para desarrollar localmente
docker run --rm -v $(pwd):/app -w /app debian:bookworm pwd
# /app
```

### `--user`

Ejecuta el proceso como un usuario específico:

```bash
# Por UID:GID
docker run --rm --user 1000:1000 debian:bookworm id
# uid=1000 gid=1000 groups=1000

# Por nombre (debe existir en la imagen)
docker run --rm --user nobody debian:bookworm id
# uid=65534(nobody) gid=65534(nogroup) groups=65534(nogroup)

# Usuario y grupo secundario
docker run --rm --user 1000:1000 -G audio,video debian:bookworm id
# uid=1000 gid=1000 groups=1000,audio,video
```

Importante: el usuario debe existir en la imagen. Si intentas usar un UID que no
existe, Docker lo usa pero algunos comandos pueden fallar (especialmente los que
leen `/etc/passwd`).

## Sobrescribir el CMD

Todo lo que se pone después de la imagen en `docker run` reemplaza el CMD del Dockerfile:

```bash
# CMD por defecto de debian:bookworm es "bash" (sin -it, termina inmediatamente)
docker run --rm debian:bookworm
# (abre bash, pero sin -it no es interactivo — termina inmediatamente)

# Sobrescribir CMD con otro comando
docker run --rm debian:bookworm echo "custom command"
# custom command

docker run --rm debian:bookworm cat /etc/os-release
# PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"
# ...

docker run --rm debian:bookworm ls -la /root
# (listado del directorio /root)
```

Si la imagen tiene un ENTRYPOINT definido, los argumentos de `docker run` se **pasan**
al entrypoint como argumentos (ver C02 Dockerfile para la diferencia CMD vs ENTRYPOINT).

```bash
# nginx tiene ENTRYPOINT ["/docker-entrypoint.sh"] y CMD ["nginx", "-g", "daemon off;"]
# docker run nginx:latest                   → /docker-entrypoint.sh nginx -g daemon off;
# docker run nginx:latest nginx -t           → /docker-entrypoint.sh nginx -t
# docker run nginx:latest /bin/bash          → /docker-entrypoint.sh /bin/bash  ← probablemente falle
```

## Restart policies

Controlan qué pasa cuando PID 1 termina:

| Política | Comportamiento |
|----------|---------------|
| `no` | No reinicia (default) |
| `on-failure` | Reinicia solo si PID 1 sale con exit code ≠ 0 |
| `on-failure:5` | Como `on-failure` pero máximo 5 intentos |
| `always` | Reinicia siempre, incluyendo al reiniciar Docker daemon |
| `unless-stopped` | Como `always`, pero no reinicia si fue detenido manualmente |

```bash
# Reiniciar automáticamente si falla (exit code ≠ 0)
docker run -d --restart on-failure:3 --name resilient debian:bookworm \
  bash -c "exit 1"

# Ver los restart attempts
docker inspect resilient --format '{{.RestartCount}}'
# 3 (después de agotar los reintentos)

# Reiniciar siempre (servidor web que debe estar siempre arriba)
docker run -d --restart always --name always-up nginx:latest

# Ver política de un contenedor
docker inspect always-up --format '{{.HostConfig.RestartPolicy.Name}}'

docker rm -f resilient always-up
```

### La diferencia entre `always` y `unless-stopped`

| Escenario | `always` | `unless-stopped` |
|-----------|----------|-----------------|
| Contenedor corriendo, host se reinicia | Se reinicia | Se reinicia |
| Contenedor detenido con `docker stop`, host se reinicia | Se reinicia | **No se reinicia** |
| Docker daemon se reinicia | Se reinicia | Se reinicia |

`unless-stopped` respeta la intención del operador: si detuviste manualmente el
contenedor, no quieres que se reinicie automáticamente.

## Flags adicionales útiles

### `--init`

Inyecta un init process (tini) como PID 1. Útil para manejar señales correctamente
y limpiar procesos zombie:

```bash
# Sin --init: bash como PID 1 no maneja bien las señales
docker run -d --name bad-init debian:bookworm bash -c 'trap "echo got TERM" SIGTERM; while true; do sleep 1; done'
docker stop bad-init   # Tarda 10 segundos (SIGKILL por timeout)

# Con --init: tini propaga las señales correctamente
docker run -d --init --name good-init debian:bookworm bash -c 'trap "echo got TERM" SIGTERM; while true; do sleep 1; done'
docker stop good-init   # Responde inmediatamente

docker rm -f bad-init good-init
```

### `--network`

Conecta el contenedor a una red específica:

```bash
# Red por defecto (bridge)
docker run --rm --network bridge debian:bookworm ip route

# Host network (sin aislamiento de red)
docker run --rm --network host debian:bookworm ip route
# WARNING: casi equivalente a correr en el host

# Red personalizada
docker network create my-net
docker run -d --network my-net --name app nginx:latest
docker run --rm --network my-net debian:bookworm ping app
# El hostname "app" resuelve a la IP del contenedor nginx

docker network rm my-net
```

### `--hostname`

Establece el hostname del contenedor (visible en `hostname` y `/etc/hostname`):

```bash
docker run --rm --hostname my-container debian:bookworm hostname
# my-container

# Sin --hostname, Docker asigna uno aleatorio basado en ID del contenedor
docker run --rm debian:bookworm hostname
# abc123def456
```

### `--dns`

Configura servidores DNS para el contenedor:

```bash
docker run --rm --dns 8.8.8.8 --dns 1.1.1.1 debian:bookworm cat /etc/resolv.conf
# nameserver 8.8.8.8
# nameserver 1.1.1.1
```

### `--add-host`

Añade entradas al `/etc/hosts` del contenedor:

```bash
docker run --rm --add-host myapp:192.168.1.100 debian:bookworm cat /etc/hosts
# 192.168.1.100    myapp
```

### `--privileged`

Otorga todos los capabilities al contenedor (cuidado: esto es peligroso en producción):

```bash
# Ejemplo: acceder a dispositivos directamente
docker run --rm --privileged debian:bookworm ls /dev
# Mucho más contenido que normalmente

# Mejor alternativa: añadir capabilities específicos
docker run --rm --cap-add SYS_ADMIN --device /dev/fuse debian:bookworm ls /dev
```

### `--read-only`

Haz el filesystem del contenedor de solo lectura (excluyendo volúmenes):

```bash
docker run --rm --read-only debian:bookworm touch /tmp/test
# touch: cannot touch '/tmp/test': Read-only file system

# Con volumen writable
docker run --rm --read-only -v /tmp/writable:/tmp debian:bookworm touch /tmp/writable/test
# Funciona
```

### `--shm-size`

Configura el tamaño de `/dev/shm` (shared memory):

```bash
# 256MB de shared memory (importante para bases de datos, etc.)
docker run --rm --shm-size=256m debian:bookworm df -h | grep shm
# shm              256M     0  256M  0% /dev/shm
```

### `--ulimit`

Configura límites de recursos (open files, processes, etc.):

```bash
docker run --rm --ulimit nofile=1024:1024 debian:bookworm ulimit -n
# 1024
```

### `--memory` y `--cpus`

Límites de recursos (se cubren en B06 para containers con límites reales):

```bash
docker run --rm --memory=512m --cpus=0.5 debian:bookworm cat /sys/fs/cgroup/memory.max
# 536870912 (bytes)
```

## Práctica

### Ejercicio 1: Interactivo vs background

```bash
# Interactivo: ejecutar bash, explorar, salir
docker run -it --rm --name interactive debian:bookworm bash
# Dentro: ls /, cat /etc/os-release, exit

# Background: ejecutar un servidor
docker run -d --rm --name background nginx:latest
docker ps   # verificar que está corriendo
docker logs background   # ver la salida
docker stop background
```

### Ejercicio 2: Efecto de `-t` en la salida

```bash
# Sin -t: salida plana
docker run --rm debian:bookworm ls --color=auto /usr/bin | head -5

# Con -t: salida con colores y formato de terminal
docker run --rm -t debian:bookworm ls --color=auto /usr/bin | head -5

# Con -t: grep colorea los matches
docker run --rm -t debian:bookworm grep -r "root" /etc/passwd

# Sin -t: grep no colorea (detecta que no hay terminal)
docker run --rm debian:bookworm grep -r "root" /etc/passwd

# Diferencia en Python buffering
docker run --rm python:3.12 python -c "import time; [print(i) for i in range(3)]"
# Puede que veas output en "bloques" por buffering

docker run --rm -t python:3.12 python -c "import time; [print(i) for i in range(3)]"
# Output aparece inmediatamente
```

### Ejercicio 3: `--rm` vs contenedores acumulados

```bash
# Crear 5 contenedores sin --rm
for i in 1 2 3 4 5; do
  docker run --name "temp-$i" debian:bookworm echo "run $i"
done

# Ver la acumulación
docker ps -a --filter "name=temp-"

# Limpiar
docker rm temp-1 temp-2 temp-3 temp-4 temp-5

# Ahora con --rm: no queda nada
for i in 1 2 3 4 5; do
  docker run --rm debian:bookworm echo "run $i"
done
docker ps -a --filter "ancestor=debian:bookworm"
# (ningún contenedor exited)
```

### Ejercicio 4: Variables de entorno y directorio de trabajo

```bash
docker run --rm \
  -e APP_NAME="Mi Aplicación" \
  -e APP_ENV=development \
  -e APP_DEBUG=true \
  -w /opt/app \
  debian:bookworm \
  bash -c 'echo "App: $APP_NAME"; echo "Env: $APP_ENV"; echo "Dir: $(pwd)"'
# App: Mi Aplicación
# Env: development
# Dir: /opt/app
```

### Ejercicio 5: Restart policies

```bash
# on-failure: reinicia solo si el proceso falla
docker run -d --restart on-failure:3 --name fail-restart debian:bookworm bash -c 'echo "exit 1"; exit 1'
sleep 1
docker ps --format '{{.Names}}: {{.Status}}'
# fail-restart: Restarting (1) 2 seconds ago

docker logs fail-restart
docker rm -f fail-restart

# always: nunca se detiene
docker run -d --restart always --name always-nginx nginx:latest
docker ps --format '{{.Names}}: {{.Status}}'
# always-nginx: Up 5 seconds

# Simular restart del daemon (esto en producción reiniciaría el contenedor)
docker stop always-nginx
docker ps -a | grep always-nginx
# (no aparece porque se reinicia con restart=always aunque hagas stop)

# La diferencia con unless-stopped
docker run -d --restart unless-stopped --name unless-nginx nginx:latest
docker stop unless-nginx
# Si ahora se reinicia Docker daemon, unless-nginx NO se levanta
# pero always-nginx SÍ se levantaría

docker rm -f always-nginx unless-nginx
```

### Ejercicio 6: --init para manejo correcto de señales

```bash
# Comparar respuesta a SIGTERM
docker run -d --name no-init debian:bookworm bash -c 'while true; do sleep 1; done'
time docker stop no-init
# Tardará ~10 segundos (timeout)

docker rm -f no-init

docker run -d --init --name with-init debian:bookworm bash -c 'while true; do sleep 1; done'
time docker stop with-init
# Se detiene inmediatamente

docker rm -f with-init
```

### Ejercicio 7: Combinar múltiples flags

```bash
# Servidor de desarrollo completo
docker run -d \
  --name dev-server \
  --restart unless-stopped \
  --network my-net \
  --hostname dev-app \
  --add-host api.local:192.168.1.10 \
  -p 3000:3000 \
  -e NODE_ENV=development \
  -e API_URL=http://api.local:8000 \
  -v $(pwd)/app:/app \
  -w /app \
  node:20-alpine \
  node server.js

docker logs -f dev-server
docker exec -it dev-server sh
docker stop dev-server && docker rm dev-server

docker network rm my-net 2>/dev/null || true
```
