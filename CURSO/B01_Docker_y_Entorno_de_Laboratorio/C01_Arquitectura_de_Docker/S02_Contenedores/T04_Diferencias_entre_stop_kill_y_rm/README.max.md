# T04 — Diferencias entre stop, kill y rm

## Resumen rápido

| Comando | Señal enviada | Grace period | Estado resultante | Datos sobreviven |
|---------|---------------|--------------|-------------------|------------------|
| `docker stop` | SIGTERM → SIGKILL | 10s (configurable con `-t`) | `exited` | Sí (capa RW preservada) |
| `docker kill` | SIGKILL (default) | Ninguno | `exited` | Sí (capa RW preservada) |
| `docker rm` | — | — | (eliminado) | No (capa RW destruida) |
| `docker rm -f` | SIGKILL + rm | Ninguno | (eliminado) | No (capa RW destruida) |

## `docker stop`

Detiene un contenedor de forma **ordenada** (graceful shutdown).

### Secuencia interna

```
docker stop my-container
│
├─→ 1. Envía SIGTERM a PID 1 del contenedor
│
├─→ 2. Espera grace period (default: 10 segundos)
│   │
│   ├─→ Si PID 1 termina dentro del grace period:
│   │       └─→ Contenedor pasa a estado "exited" (código de salida de PID 1)
│   │
│   └─→ Si PID 1 NO termina dentro del grace period:
│           └─→ Envía SIGKILL a PID 1
│               └─→ Contenedor pasa a estado "exited" (código 137)
```

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
- Escribir datos pendientes a disco (flush de buffers)
- Liberar recursos (archivos temporales, locks, sockets)
- Notificar a otros servicios que se va
- Guardar estado interno si es necesario

Un servidor web bien implementado, al recibir SIGTERM:
1. Deja de aceptar conexiones nuevas
2. Termina de servir las requests en curso
3. Cierra conexiones a bases de datos
4. Escribe logs de cierre
5. Sale con exit code 0

### Cuando el grace period no es suficiente

Si PID 1 no termina en los 10 segundos, Docker envía SIGKILL. SIGKILL no se puede
atrapar, ignorar ni manejar — es una terminación forzada a nivel de kernel.

```bash
# Ejemplo: proceso que tarda en apagar
docker run -d --name slow-stop debian:bookworm bash -c '
  trap "echo SIGTERM received, will wait 15s; sleep 15; exit 0" TERM
  while true; do sleep 1; done
'

# Con timeout default (10s): SIGTERM, espera 10s, luego SIGKILL
time docker stop slow-stop
# real   0m10.xxx s  (esperó 10s y luego mató con SIGKILL)

docker rm slow-stop

# Con timeout suficiente (20s): SIGTERM, el proceso termina a los 15s limpiamente
docker run -d --name slow-stop2 debian:bookworm bash -c '
  trap "echo SIGTERM received, will wait 15s; sleep 15; exit 0" TERM
  while true; do sleep 1; done
'
time docker stop -t 20 slow-stop2
# real   0m15.xxx s  (terminó limpiamente a los 15s)

docker rm slow-stop2
```

### PID 1 y señales: el problema de los shell scripts

Bash como PID 1 **no propaga señales** a procesos hijos por defecto. Si tu ENTRYPOINT
es un shell script que lanza un servidor, SIGTERM llega a bash, no al servidor:

```bash
# Problema: bash ignora SIGTERM como PID 1
docker run -d --name bash-pid1 debian:bookworm bash -c "sleep 300"

time docker stop bash-pid1
# real   0m10.xxx s  ← esperó 10s completos, luego SIGKILL
# bash no propagó SIGTERM a sleep, así que sleep nunca recibió la señal

docker rm bash-pid1
```

Soluciones:

```bash
# Solución 1: usar exec para reemplazar bash por el proceso real
docker run -d --name exec-pid1 debian:bookworm bash -c "exec sleep 300"
time docker stop exec-pid1
# real   0m0.xxx s  ← sleep recibe SIGTERM directamente y termina
docker rm exec-pid1

# Solución 2: usar --init (tini como PID 1)
docker run -d --init --name init-pid1 debian:bookworm sleep 300
time docker stop init-pid1
# real   0m0.xxx s  ← tini propaga SIGTERM a sleep
docker rm init-pid1

# Solución 3: trap en el script
docker run -d --name trap-pid1 debian:bookworm bash -c '
  cleanup() {
    echo "Cleaning up..."
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    exit 0
  }
  trap cleanup TERM
  sleep 300 &
  PID=$!
  wait $PID
'
time docker stop trap-pid1
# real   0m0.xxx s  ← trap captura SIGTERM y limpia
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
docker kill -s SIGHUP my-container    # convención para "recargar configuración"
docker kill -s SIGINT my-container    # como Ctrl+C

# Formas equivalentes de especificar la señal
docker kill -s TERM my-container
docker kill --signal TERM my-container
docker kill --signal=15 my-container
```

### Señales comunes

| Señal | Número | Descripción | Capturable? |
|-------|--------|-------------|-------------|
| SIGTERM | 15 | Solicitud de terminación | **Sí** |
| SIGKILL | 9 | Terminación forzada | No (siempre mata) |
| SIGINT | 2 | Interrupción (Ctrl+C) | **Sí** |
| SIGHUP | 1 | Hangup, convención para recargar config | **Sí** |
| SIGUSR1 | 10 | Definida por la aplicación | **Sí** |
| SIGUSR2 | 12 | Definida por la aplicación | **Sí** |
| SIGSTOP | 19 | Stop (Ctrl+Z) | No |
| SIGCONT | 18 | Continuar después de STOP | N/A |

### Uso de señales custom

Algunos servidores usan señales para operaciones sin reinicio:

```bash
# Nginx: recargar configuración
docker kill -s SIGHUP my-nginx
# nginx -s reload

# Postgresql: recargar config
docker kill -s SIGHUP postgres-container

# Redis: guardar datos a disco (BGSAVE)
docker kill -s SIGUSR1 redis-container

# Apache: graceful restart
docker kill -s SIGHUP httpd-container
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

# Eliminar al detener (flag útil)
docker run --rm nginx:latest
# Se elimina automáticamente cuando termina

# Eliminar todos los contenedores detenidos
docker rm $(docker ps -aq --filter status=exited)

# Versión interactiva (pide confirmación)
docker container prune
```

### `-f` (force)

`docker rm -f` equivale a `docker kill` + `docker rm`:

```bash
docker run -d --name force-test debian:bookworm sleep 300

# Esto falla (el contenedor está corriendo)
docker rm force-test
# Error response from daemon: No such container: force-test
# Actually: Error: cannot remove a running container.

# Esto funciona (mata y elimina)
docker rm -f force-test
```

### `-v` (volumes)

Elimina los volúmenes **anónimos** asociados al contenedor:

```bash
# Crear un contenedor con un volumen anónimo
docker run -d --name vol-test -v /data debian:bookworm sleep 300
docker stop vol-test

# Sin -v: el volumen anónimo queda huérfano (orphaned)
docker rm vol-test
docker volume ls   # el volumen anónimo sigue ahí
# Drivetest: <volumen_id>   ← volumen sin nombre, inútil

# Con -v: el volumen anónimo se elimina junto con el contenedor
docker run -d --name vol-test2 -v /data debian:bookworm sleep 300
docker stop vol-test2
docker rm -v vol-test2
docker volume ls   # el volumen anónimo fue eliminado
```

**Importante**: `-v` solo elimina volúmenes anónimos (los creados automáticamente con `-v /path`).
Los **named volumes** (`-v mi-volumen:/data`) no se eliminan con `-v` — solo los anonymous volumes.
Para eliminar named volumes: `docker volume rm mi-volumen`.

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

### Con `rm -f`

```
docker run → escribir datos → docker rm -f → datos perdidos para siempre
(SIGKILL se envía primero, luego se elimina)
```

### Resumen de persistencia

| Tipo de dato | `stop`/`start` | `rm` | `rm -f` |
|-------------|----------------|------|----------|
| Capa de escritura (archivos del contenedor) | **Preserva** | Destruye | Destruye |
| Named volumes (`-v mydata:/path`) | **Preserva** | **Preserva** | **Preserva** |
| Volúmenes anónimos (`-v /path`) | **Preserva** | Huérfano | Destruye con `-v` |
| Bind mounts (`-v /host:/container`) | **Preserva** | **Preserva** | **Preserva** |
| Logs (`docker logs`) | **Preserva** | Destruye | Destruye |
| Configuración de red, IP | Se reasigna al start | Se elimina | Se elimina |
| Exit code de PID 1 | Disponible | Disponible | Disponible |

## `tini` y `--init`

`--init` inyecta [tini](https://github.com/krallin/tini) como PID 1, que actúa como
un init system mínimo:

```
Sin --init:
  PID 1: tu-app (puede no manejar señales)
       └─→ PID 2: sleep (nunca recibe SIGTERM si tu-app no lo propaga)

Con --init:
  PID 1: tini (maneja señales, reaps zombies)
       └─→ PID 2: tu-app (recibe señales propagadas por tini)
                └─→ PID 3: sleep (también recibe señales)
```

Tini resuelve dos problemas:

1. **Propagación de señales**: SIGTERM se propaga correctamente a los procesos hijos
2. **Zombie reaping**: los procesos zombie (hijos que terminaron pero cuyo exit code
   nadie recolectó) son limpiados por tini. Sin tini, PID 1 debe hacer `wait()` —
   si no lo hace, los zombies se acumulan

### El problema de zombies en contenedores

Un proceso zombie es un proceso que terminó pero cuya entrada en la tabla de procesos
no ha sido liberada porque su padre no leyó su exit code con `wait()`.

```bash
# Demostrar el problema de zombies sin --init
docker run -d --name zombie-factory debian:bookworm bash -c '
  # Crear procesos hijos que terminan inmediatamente
  for i in $(seq 1 10); do
    sleep 0 &
  done
  # Estos sleeps terminarán, pero nadie hace wait() en bash
  # Quedan como zombies hasta que bash muera
  echo "Waiting..."
  sleep 300
'

# Ver los procesos zombie
docker exec zombie-factory ps aux | grep -E "defunct|Sleep"
# <defunct> aparecerán marcados

# Limpiar
docker rm -f zombie-factory

# Con --init: tini limpia los zombies
docker run -d --init --name no-zombies debian:bookworm bash -c '
  for i in $(seq 1 10); do
    sleep 0 &
  done
  echo "Waiting..."
  sleep 300
'

docker exec no-zombies ps aux | grep -E "defunct|Sleep"
# 0 zombies - tini los limpió

docker rm -f no-zombies
```

### tini no es la solución para todo

tini solo propaga señales. Si tu aplicación tiene un bug y no maneja SIGTERM,
el contenedor seguirá sin detenerse limpiamente. En ese caso, el grace period de
`docker stop` eventualmente enviará SIGKILL.

```
Escenario: app que hace infinite loop sin manejar señales

Con --init:
  PID 1: tini
    └─ PID 2: app (infinite loop)
  
docker stop:
  tini recibe SIGTERM → lo propaga a app
  app no responde (no maneja SIGTERM) → tini no puede hacer nada
  Después de grace period → SIGKILL a app
  Contenedor se detiene
```

## El problema del doble fork con daemons

Históricamente, los daemons en Unix usaban "doble fork" para desacoplarse del padre:

```c
// Doble fork clásico (ya no necesario con systemd/init moderno)
if (fork() == 0) {           // Primer hijo
    setsid();                  // Nuevo session ID
    if (fork() == 0) {        // Segundo hijo (el daemon real)
        // Trabaja en segundo plano
    }
    exit(0);                   // Primer hijo termina
}
// Padre hace wait() del primer hijo
```

En contenedores Docker esto generalmente no es necesario porque:
- El contenedor no es un sistema completo con múltiples usuarios
- systemd no está corriendo (o si está, no es PID 1)
- Lo importante es que PID 1 maneje señales y haga wait() de sus hijos

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
# Contenedor que responde a SIGUSR1 y SIGTERM
docker run -d --name signal-demo debian:bookworm bash -c '
  echo "Started at $(date)" >> /tmp/signals.log
  trap "echo SIGUSR1 received at $(date) >> /tmp/signals.log" USR1
  trap "echo SIGTERM received at $(date), exiting >> /tmp/signals.log; exit 0" TERM
  echo "Waiting for signals..." >> /tmp/signals.log
  while true; do sleep 1; done
'

# Enviar SIGUSR1 (no mata el contenedor)
docker kill -s SIGUSR1 signal-demo
sleep 1
docker kill -s SIGUSR1 signal-demo
sleep 1

# Ver el log
docker exec signal-demo cat /tmp/signals.log
# Started at ...
# Waiting for signals...
# SIGUSR1 received at ...
# SIGUSR1 received at ...

# Detener limpiamente
docker stop signal-demo
docker exec signal-demo cat /tmp/signals.log
# Ahora muestra: SIGTERM received at ...

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

### Ejercicio 4: El problema del grace period insuficiente

```bash
# Contenedor que tarda 20 segundos en apagar
docker run -d --name graceful-shutdown debian:bookworm bash -c '
  trap "echo Received SIGTERM, will take 20s to shutdown; sleep 20; echo Done, exiting; exit 0" TERM
  echo "Started, waiting..."
  while true; do sleep 1; done
'

# Con default 10s timeout: docker stop enviará SIGKILL después de 10s
echo "Testing with default 10s timeout..."
time docker stop graceful-shutdown 2>&1
docker rm graceful-shutdown 2>/dev/null || true

# Ahora con timeout suficiente
docker run -d --name graceful-shutdown2 debian:bookworm bash -c '
  trap "echo Received SIGTERM, will take 20s to shutdown; sleep 20; echo Done, exiting; exit 0" TERM
  echo "Started, waiting..."
  while true; do sleep 1; done
'

echo "Testing with 30s timeout..."
time docker stop -t 30 graceful-shutdown2 2>&1
# Verás que termina limpiamente a los 20s

docker rm -f graceful-shutdown2
```

### Ejercicio 5: Exec para reemplazar PID 1

```bash
# Sin exec: bash como PID 1, sleep como hijo
docker run -d --name no-exec debian:bookworm bash -c "sleep 300 & wait"

# Ver los procesos
docker exec no-exec ps -o pid,ppid,args
#   PID   PPID  COMMAND
#     1     0   bash -c sleep 300 & wait
#     7     1   sleep 300
# bash es PID 1, sleep es PID 7 (hijo)

# Cuando docker stop envía SIGTERM a PID 1 (bash), bash no lo propaga a sleep
time docker stop no-exec  # Tardará 10s
docker rm -f no-exec

# Con exec: sleep reemplaza a bash como PID 1
docker run -d --name with-exec debian:bookworm bash -c "exec sleep 300"

docker exec with-exec ps -o pid,ppid,args
#   PID   PPID  COMMAND
#     1     0   sleep 300
# sleep es PID 1 directamente (bash se reemplazó)

time docker stop with-exec  # Inmediato
docker rm -f with-exec
```

### Ejercicio 6: docker rm con volúmenes

```bash
# Crear varios contenedores con volúmenes
docker run -d --name vol-anon -v /tmp/anon debian:bookworm sleep 300
docker run -d --name vol-named -v mi-volumen:/tmp/named debian:bookworm sleep 300
docker run -d --name no-vol debian:bookworm sleep 300

# Ver los volúmenes antes
docker volume ls | grep -E "mi-volumen|anonymous"

# Detener y eliminar algunos con -v, otros sin
docker stop vol-anon vol-named no-vol

docker rm vol-anon        # volumen anónimo queda huérfano
docker rm -v vol-named   # volumen anónimo se elimina
docker rm no-vol          # sin volumen

# Ver los volúmenes después
docker volume ls | grep -E "mi-volumen|anonymous"
# mi-volumen sigue existiendo (named volume)

# Limpiar
docker volume rm mi-volumen 2>/dev/null || true
```
