# T04 — Diferencias entre stop, kill y rm

## Resumen rápido

| Comando | Señal | Grace period | Estado resultante | Datos |
|---------|-------|-------------|-------------------|-------|
| `docker stop` | SIGTERM → SIGKILL | 10s (configurable) | exited | Capa de escritura preservada |
| `docker kill` | SIGKILL (default) | Ninguno | exited | Capa de escritura preservada |
| `docker rm` | — | — | (eliminado) | Capa de escritura destruida |
| `docker rm -f` | SIGKILL + rm | Ninguno | (eliminado) | Capa de escritura destruida |

## `docker stop`

Detiene un contenedor de forma **ordenada** (graceful shutdown).

### Secuencia

1. Docker envía **SIGTERM** a PID 1
2. Espera un **grace period** (default: 10 segundos)
3. Si PID 1 no ha terminado, envía **SIGKILL** (terminación forzada)
4. El contenedor pasa a estado `exited`

```bash
docker run -d --name graceful debian:bookworm sleep 300

# Detener con grace period default (10s)
docker stop graceful

# Detener con grace period custom (3 segundos)
docker stop -t 3 graceful

# Detener inmediatamente (grace period 0 = SIGTERM + SIGKILL casi simultáneos)
docker stop -t 0 graceful
```

### El propósito del grace period

SIGTERM es una solicitud educada: "por favor, termina". Le da al proceso la oportunidad de:
- Cerrar conexiones de red limpiamente
- Escribir datos pendientes a disco (flush buffers)
- Liberar recursos (archivos temporales, locks)
- Notificar a otros servicios que se va

Un servidor web bien implementado, al recibir SIGTERM:
1. Deja de aceptar conexiones nuevas
2. Termina de servir las requests en curso
3. Cierra conexiones a bases de datos
4. Sale con exit code 0

### Cuando el grace period no es suficiente

Si PID 1 no termina en los 10 segundos, Docker envía SIGKILL. SIGKILL no se puede
atrapar, ignorar ni manejar — es una terminación abrupta a nivel de kernel.

```bash
# Ejemplo: proceso que tarda en apagar
docker run -d --name slow-stop debian:bookworm bash -c '
  trap "echo SIGTERM received; sleep 15; exit 0" TERM
  while true; do sleep 1; done
'

# Con timeout default (10s): SIGTERM, espera 10s, luego SIGKILL
time docker stop slow-stop
# real   10.xxx s  (esperó 10s y luego mató con SIGKILL)

docker rm slow-stop

# Con timeout suficiente (20s): SIGTERM, el proceso termina a los 15s
docker run -d --name slow-stop2 debian:bookworm bash -c '
  trap "echo SIGTERM received; sleep 15; exit 0" TERM
  while true; do sleep 1; done
'
time docker stop -t 20 slow-stop2
# real   15.xxx s  (terminó limpiamente a los 15s)

docker rm slow-stop2
```

### PID 1 y señales: el problema de los shell scripts

Bash como PID 1 **no propaga señales** a procesos hijos por defecto. Si tu ENTRYPOINT
es un shell script que lanza un servidor, SIGTERM llega a bash, no al servidor:

```bash
# Problema: bash ignora SIGTERM como PID 1
docker run -d --name bash-pid1 debian:bookworm bash -c "sleep 300"

time docker stop bash-pid1
# real   10.xxx s  ← esperó 10s completos, luego SIGKILL
# bash no propagó SIGTERM a sleep

docker rm bash-pid1
```

Soluciones:

```bash
# Solución 1: usar exec para reemplazar bash por el proceso real
docker run -d --name exec-pid1 debian:bookworm bash -c "exec sleep 300"
time docker stop exec-pid1
# real   0.xxx s  ← sleep recibe SIGTERM directamente y termina
docker rm exec-pid1

# Solución 2: usar --init (tini como PID 1)
docker run -d --init --name init-pid1 debian:bookworm sleep 300
time docker stop init-pid1
# real   0.xxx s  ← tini propaga SIGTERM a sleep
docker rm init-pid1

# Solución 3: trap en el script
docker run -d --name trap-pid1 debian:bookworm bash -c '
  cleanup() { kill $PID; wait $PID; exit 0; }
  trap cleanup TERM
  sleep 300 &
  PID=$!
  wait $PID
'
time docker stop trap-pid1
# real   0.xxx s  ← trap captura SIGTERM y limpia
docker rm trap-pid1
```

## `docker kill`

Envía una señal a PID 1 **sin grace period**. Por defecto envía SIGKILL.

```bash
# SIGKILL (default) — terminación inmediata, sin posibilidad de cleanup
docker kill my-container

# Enviar una señal específica
docker kill -s SIGTERM my-container    # equivale a docker stop -t 0
docker kill -s SIGUSR1 my-container    # señal custom
docker kill -s SIGHUP my-container     # convención para "recargar configuración"
```

### Señales comunes

| Señal | Número | Comportamiento |
|-------|--------|---------------|
| SIGTERM (15) | Solicitud de terminación (capturable) |
| SIGKILL (9) | Terminación forzada (no capturable) |
| SIGINT (2) | Interrupción (Ctrl+C) |
| SIGHUP (1) | Hangup, convención para recargar config |
| SIGUSR1 (10) | Definida por la aplicación |
| SIGUSR2 (12) | Definida por la aplicación |

### Uso de señales custom

Algunos servidores usan señales para operaciones sin reinicio:

```bash
# Nginx: recargar configuración sin reiniciar
docker kill -s SIGHUP my-nginx

# Algunos servidores: reabrir archivos de log (log rotation)
docker kill -s SIGUSR1 my-server
```

## `docker rm`

Elimina un contenedor. Por defecto solo funciona con contenedores detenidos.

```bash
# Eliminar un contenedor detenido
docker rm my-container

# Eliminar un contenedor corriendo (kill + rm)
docker rm -f my-container

# Eliminar múltiples contenedores
docker rm container1 container2 container3

# Eliminar todos los contenedores detenidos
docker rm $(docker ps -aq --filter status=exited)
# O más seguro:
docker container prune
```

### `-f` (force)

`docker rm -f` equivale a `docker kill` + `docker rm`:

```bash
docker run -d --name force-test debian:bookworm sleep 300

# Esto falla (el contenedor está corriendo)
docker rm force-test
# Error: cannot remove a running container

# Esto funciona (mata y elimina)
docker rm -f force-test
```

### `-v` (volumes)

Elimina los volúmenes **anónimos** asociados al contenedor:

```bash
# Crear un contenedor con un volumen anónimo
docker run -d --name vol-test -v /data debian:bookworm sleep 300
docker stop vol-test

# Sin -v: el volumen anónimo queda huérfano
docker rm vol-test
docker volume ls   # el volumen anónimo sigue ahí

# Con -v: el volumen anónimo se elimina junto con el contenedor
docker run -d --name vol-test2 -v /data debian:bookworm sleep 300
docker stop vol-test2
docker rm -v vol-test2
docker volume ls   # el volumen anónimo fue eliminado
```

Los **named volumes** no se eliminan con `-v` — solo los anónimos. Para eliminar
named volumes: `docker volume rm nombre`.

## Qué persiste y qué se pierde

### Entre `stop` y `start`

La capa de escritura se **preserva**:

```
docker run → escribir datos → docker stop → docker start → datos siguen ahí
```

### Con `rm`

La capa de escritura se **destruye**:

```
docker run → escribir datos → docker stop → docker rm → datos perdidos para siempre
```

### Resumen de persistencia

| Tipo de dato | `stop`/`start` | `rm` |
|-------------|----------------|------|
| Capa de escritura (archivos del contenedor) | Persiste | Se destruye |
| Named volumes (`-v mydata:/path`) | Persiste | Persiste (necesita `docker volume rm`) |
| Volúmenes anónimos (`-v /path`) | Persiste | Se destruye con `rm -v`, queda huérfano con `rm` |
| Bind mounts (`-v /host:/container`) | Persiste (están en el host) | Persiste (están en el host) |
| Logs (`docker logs`) | Persiste | Se destruyen |
| Configuración de red, IP | Se reasigna al start | Se elimina |

## `tini` y `--init`

`--init` inyecta [tini](https://github.com/krallin/tini) como PID 1, que actúa como
un init system mínimo:

```
Sin --init:
  PID 1: tu-app (puede no manejar señales)

Con --init:
  PID 1: tini (maneja señales, reaps zombies)
   └── PID 2: tu-app (recibe señales propagadas por tini)
```

Tini resuelve dos problemas:

1. **Propagación de señales**: SIGTERM se propaga correctamente a los procesos hijos
2. **Zombie reaping**: los procesos zombie (hijos que terminaron pero cuyo exit code
   nadie recolectó) son limpiados por tini. Sin tini, PID 1 debe hacer wait() — si
   no lo hace, los zombies se acumulan

```bash
# Demostrar el problema de zombies sin --init
docker run -d --name zombie-factory debian:bookworm bash -c '
  # Crear procesos hijos que terminan inmediatamente
  for i in $(seq 1 10); do
    sleep 0 &
  done
  sleep 300
'
# Después de unos segundos, pueden acumularse procesos zombie
docker exec zombie-factory ps aux | grep -c defunct
docker rm -f zombie-factory

# Con --init: tini limpia los zombies
docker run -d --init --name no-zombies debian:bookworm bash -c '
  for i in $(seq 1 10); do
    sleep 0 &
  done
  sleep 300
'
docker exec no-zombies ps aux | grep -c defunct
# 0
docker rm -f no-zombies
```

## Práctica

### Ejercicio 1: Comparar tiempos de stop vs kill

```bash
# Contenedor que no maneja SIGTERM (bash como PID 1)
docker run -d --name no-handler debian:bookworm bash -c "while true; do sleep 1; done"

echo "--- docker stop (esperará 10s) ---"
time docker stop no-handler
docker rm no-handler

# Mismo contenedor, pero con kill
docker run -d --name no-handler2 debian:bookworm bash -c "while true; do sleep 1; done"

echo "--- docker kill (inmediato) ---"
time docker kill no-handler2
docker rm no-handler2

# Contenedor que SÍ maneja SIGTERM (con --init)
docker run -d --init --name with-handler debian:bookworm bash -c "while true; do sleep 1; done"

echo "--- docker stop con --init (rápido) ---"
time docker stop with-handler
docker rm with-handler
```

### Ejercicio 2: Señales custom

```bash
# Contenedor que responde a SIGUSR1
docker run -d --name signal-demo debian:bookworm bash -c '
  trap "echo SIGUSR1 received! >> /tmp/signals.log" USR1
  trap "echo SIGTERM received, exiting; exit 0" TERM
  echo "Waiting for signals..." >> /tmp/signals.log
  while true; do sleep 1; done
'

# Enviar SIGUSR1
docker kill -s SIGUSR1 signal-demo
sleep 1
docker kill -s SIGUSR1 signal-demo
sleep 1

# Ver el log
docker exec signal-demo cat /tmp/signals.log
# Waiting for signals...
# SIGUSR1 received!
# SIGUSR1 received!

# Detener limpiamente
docker stop signal-demo
docker rm signal-demo
```

### Ejercicio 3: Persistencia de datos

```bash
# Datos en capa de escritura — se pierden con rm
docker run --name write-layer debian:bookworm bash -c "echo 'volatile' > /tmp/data"
docker start write-layer   # re-ejecuta el CMD
docker stop write-layer
docker rm write-layer       # /tmp/data perdido para siempre

# Datos en named volume — sobreviven rm
docker run --name vol-persist -v testdata:/data debian:bookworm bash -c "echo 'persistent' > /data/info"
docker rm vol-persist
docker run --rm -v testdata:/data debian:bookworm cat /data/info
# persistent ← los datos sobrevivieron

# Limpiar el volumen
docker volume rm testdata

# Datos en bind mount — siempre en el host
mkdir -p /tmp/docker-test
docker run --rm -v /tmp/docker-test:/data debian:bookworm bash -c "echo 'on host' > /data/file"
cat /tmp/docker-test/file
# on host ← el archivo está en el host, independiente del contenedor
rm -rf /tmp/docker-test
```
