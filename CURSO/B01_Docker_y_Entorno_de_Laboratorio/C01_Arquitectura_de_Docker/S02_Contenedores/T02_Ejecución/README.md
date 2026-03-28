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

### `-t` (tty)

Asigna una pseudo-terminal (PTY). Los programas detectan si están conectados a una
terminal y ajustan su comportamiento:

```bash
# Sin -t: ls no muestra colores, salida en una sola columna
docker run debian:bookworm ls /usr
# bin  games  include  lib  local  sbin  share  src

# Con -t: ls muestra colores y formato multi-columna
docker run -t debian:bookworm ls /usr
# bin  games  include  lib  local  sbin  share  src  (con colores)
```

Otros efectos de `-t`:
- `grep` muestra matches coloreados
- Python deshabilita el output buffering (muestra print() inmediatamente)
- Programas interactivos (vim, less, top) requieren `-t` para funcionar
- El prompt de bash requiere `-t` para mostrarse

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
| `-dit` | Background con PTY, adjuntable después | Contenedores que se reusan con `docker attach` |

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

`--rm` no se puede combinar con `--restart` (excepto `--restart no`), porque el
reinicio automático requiere que el contenedor exista después de terminar.

### `--name`

Asigna un nombre legible al contenedor:

```bash
docker run --name my-server nginx:latest

# Sin --name, Docker genera nombres aleatorios
docker run nginx:latest
docker ps
# ... festive_einstein  (nombre generado)
```

Los nombres deben ser únicos. Si un contenedor con ese nombre ya existe (incluso
detenido), `docker run --name` falla:

```bash
docker run --name test debian:bookworm echo "first"
docker run --name test debian:bookworm echo "second"
# Error: Conflict. The container name "/test" is already in use

# Solución: eliminar el anterior, o usar un nombre diferente
docker rm test
```

### `-e` (environment)

Establece variables de entorno dentro del contenedor:

```bash
docker run --rm -e MY_VAR=hello -e DB_HOST=localhost debian:bookworm env
# ...
# MY_VAR=hello
# DB_HOST=localhost
# ...

# Pasar una variable del host
export HOST_VAR="from-host"
docker run --rm -e HOST_VAR debian:bookworm bash -c 'echo $HOST_VAR'
# from-host
```

### `-p` (publish)

Mapea puertos del host al contenedor:

```bash
# Puerto 8080 del host → puerto 80 del contenedor
docker run -d -p 8080:80 --name web nginx:latest
curl http://localhost:8080
# (respuesta de nginx)

docker stop web && docker rm web
```

Se cubre en detalle en S04 (Redes).

### `-v` (volume)

Monta volúmenes o directorios del host:

```bash
# Bind mount: directorio del host montado en el contenedor
docker run --rm -v $(pwd):/app debian:bookworm ls /app

# Named volume
docker run --rm -v mydata:/data debian:bookworm ls /data
```

Se cubre en detalle en S03 (Almacenamiento).

### `-w` (workdir)

Establece el directorio de trabajo (sobrescribe el WORKDIR de la imagen):

```bash
docker run --rm -w /tmp debian:bookworm pwd
# /tmp
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
```

## Sobrescribir el CMD

Todo lo que se pone después de la imagen en `docker run` reemplaza el CMD del Dockerfile:

```bash
# CMD por defecto de debian:bookworm es "bash"
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

Si la imagen tiene un ENTRYPOINT definido, los argumentos de `docker run` se **añaden**
al entrypoint en vez de reemplazar el CMD (ver C02 Dockerfile para detalles).

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
# Reiniciar automáticamente si falla
docker run -d --restart on-failure:3 --name resilient debian:bookworm \
  bash -c "exit 1"

# Ver los restart attempts
docker inspect resilient --format '{{.RestartCount}}'
# 3 (después de los reintentos)

docker rm -f resilient
```

La diferencia entre `always` y `unless-stopped`:
- `always`: si el host se reinicia y Docker arranca, el contenedor se reinicia
- `unless-stopped`: si hiciste `docker stop` antes del reinicio del host, **no** se
  reinicia al arrancar Docker — respeta el stop manual

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
```

### Ejercicio 3: `--rm` vs contenedores acumulados

```bash
# Crear 5 contenedores sin --rm
for i in 1 2 3 4 5; do
  docker run --name "temp-$i" debian:bookworm echo "run $i"
done

# Ver la acumulación
docker ps -a --filter "name=temp-"
# 5 contenedores en estado Exited

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
