# T01 — Ciclo de vida

## ¿Qué es un contenedor?

Un contenedor es una **instancia en ejecución de una imagen**. Si la imagen es la
receta, el contenedor es el plato servido. Técnicamente, un contenedor es:

1. Las capas de solo lectura de la imagen (el filesystem base)
2. Una **capa de escritura** (writable layer) encima
3. Un **proceso aislado** que ve el filesystem resultante como una vista unificada (OverlayFS merge)

Un contenedor no es una máquina virtual. No tiene un kernel propio, no emula hardware,
y no ejecuta un sistema operativo completo. Es un proceso Linux normal con aislamiento
proporcionado por:

| Mecanismo | Qué aísla |
|-----------|-----------|
| **namespaces** | Lo que el proceso puede ver (PID, red, mount, user, etc.) |
| **cgroups** | Cuántos recursos puede usar (CPU, memoria, I/O) |
| **seccomp** | Qué syscalls puede llamar |
| **capabilities** | Qué privilegios de root tiene |

## Estados del contenedor

Un contenedor pasa por los siguientes estados durante su existencia:

```
docker create              docker start            docker pause
     │                        │                       │
     ▼                        ▼                       ▼
┌─────────┐             ┌──────────┐           ┌──────────┐
│ created │────────────▶│ running  │──────────▶│  paused  │
└─────────┘             └──────────┘           └──────────┘
                         │    ▲                       │
                         │    │   docker unpause      │
                         │    └───────────────────────┘
                         │
              PID 1 termina / docker stop / docker kill
                         │
                         ▼
                   ┌──────────┐         docker rm       ┌──────────┐
                   │  exited  │────────────────────────▶│ (removed)│
                   └──────────┘                         └──────────┘
```

### created

El contenedor existe pero nunca se ha iniciado. Se creó con `docker create`, que:

- Reserva la capa de escritura (OverlayFS upperdir)
- Asigna configuración de red (IP del bridge, hostname)
- Prepara los mount points (volúmenes, bind mounts)
- Prepara namespaces (pero no los entra)
- **No ejecuta ningún proceso**

```bash
# Crear sin iniciar
docker create --name my-container debian:bookworm echo "hello"

# Verificar el estado
docker ps -a --filter name=my-container --format '{{.Status}}'
# Created
```

Usos de `docker create`:

- Preparar un contenedor y copiarte archivos con `docker cp` antes de iniciarlo
- Inspeccionar la configuración antes de ejecutar
- Separar la creación de la ejecución en scripts
- Preparar contenedores para runners de CI/CD que luego se iniciarán

### running

El proceso principal (PID 1 del contenedor) está activo. Se llega aquí con `docker start` o
`docker run`.

```bash
# Iniciar el contenedor creado
docker start my-container

# O crear + iniciar en un solo paso
docker run --name another debian:bookworm sleep 60
```

El contenedor permanece en `running` mientras PID 1 esté vivo. Si el proceso termina,
el contenedor pasa a `exited`.

### paused

El proceso está **congelado** en el estado exacto en que estaba. No ejecuta
instrucciones, no consume CPU, pero mantiene toda la memoria y el estado.

Docker usa el **cgroups freezer** para pausar. Internamente envía SIGSTOP a todos
los procesos del contenedor, igual que harías con `kill -STOP` en un proceso normal.

```bash
# Pausar un contenedor corriendo
docker pause my-container

# Verificar
docker ps --filter name=my-container --format '{{.Status}}'
# Up 30 seconds (Paused)

# Reanudar
docker unpause my-container
```

Usos de `docker pause`:

- Tomar un snapshot consistente del filesystem (sin escrituras en curso)
- Liberar CPU temporalmente sin perder el estado
- Debugging: congelar un proceso para inspeccionar sus archivos
- Pausar durante un backup del contenedor

### exited (stopped)

PID 1 terminó (por cuenta propia, por señal, o por `docker stop`/`docker kill`).
El contenedor conserva:

- Su capa de escritura (todos los archivos modificados/creados)
- Sus logs (`docker logs` los preserva)
- Su configuración

```bash
# Un contenedor que termina naturalmente
docker run --name echo-test debian:bookworm echo "done"

docker ps -a --filter name=echo-test --format '{{.Status}}'
# Exited (0) 2 seconds ago
```

El número entre paréntesis es el **exit code** de PID 1.

### dead

Estado raro. Ocurre cuando Docker no puede eliminar un contenedor porque algún recurso
estaba ocupado (por ejemplo, un filesystem montado que está busy, o un proceso que
no terminó correctamente). Requiere intervención manual.

```bash
# Ver contenedores en estado dead (si existen)
docker ps -a --filter status=dead

# Posible solución: forzar remoción
docker rm -f dead-container

# Si no funciona, investigar qué tiene ocupado
docker inspect dead-container --format '{{json .State}}'
```

## PID 1: el proceso principal

En un contenedor, el proceso principal es PID 1. Todo el ciclo de vida del contenedor
gira alrededor de este proceso:

- **Cuando PID 1 arranca** → el contenedor pasa a `running`
- **Cuando PID 1 termina** → el contenedor pasa a `exited`
- **El exit code del contenedor** = el exit code de PID 1

```bash
# PID 1 termina con exit code 0 (éxito)
docker run --rm debian:bookworm bash -c "exit 0"
echo $?   # 0

# PID 1 termina con exit code 1 (error)
docker run --rm debian:bookworm bash -c "exit 1"
echo $?   # 1

# PID 1 es matado con SIGKILL (exit code 137 = 128 + 9)
docker run -d --name kill-test debian:bookworm sleep 300
docker kill kill-test
docker inspect kill-test --format '{{.State.ExitCode}}'
# 137
docker rm kill-test
```

### Exit codes comunes

| Exit code | Significado | Causa típica |
|-----------|------------|--------------|
| 0 | Terminación exitosa | Programa completó normalmente |
| 1 | Error genérico | Excepción, error de aplicación |
| 2 | Uso incorrecto | flags inválidos, argumentos faltantes |
| 126 | No ejecutable | Sin permiso de ejecución |
| 127 | Comando no encontrado | binary no está en PATH |
| 130 | 128 + 2 = SIGINT | Ctrl+C, o proceso recibió SIGINT |
| 134 | 128 + 6 = SIGABRT | Abort (assert falló, typical panic) |
| 137 | 128 + 9 = SIGKILL | `docker kill`, OOM killer, `kill -9` |
| 139 | 128 + 11 = SIGSEGV | Segmentation fault |
| 143 | 128 + 15 = SIGTERM | `docker stop` exitoso |
| 255 | Error de shell | Exit fuera de rango |

La fórmula: **exit code = 128 + número de señal**

### PID 1 y el manejo de señales

En Linux, PID 1 tiene un comportamiento especial: **ignora señales que no tiene
handler para manejar**. Esto es el comportamiento por defecto del kernel — init
debe manejar señales específicamente.

En un contenedor donde PID 1 es tu aplicación (no un init), si la aplicación no
maneja SIGTERM, ignores will it:

```bash
# Este contenedor NO responde a docker stop (SIGTERM)
# porque bash no tiene handler para SIGTERM
docker run -d --name bad-pid1 debian:bookworm bash -c "while true; do sleep 1; done"
docker stop bad-pid1
# Espera 10 segundos... luego envía SIGKILL
```

`docker stop` espera 10 segundos (configurable con `-t`) a que PID 1 termine tras
SIGTERM. Si no responde, envía SIGKILL (fuerza bruta).

La solución es usar `--init`:

```bash
# tini como PID 1 — maneja señales correctamente
docker run -d --init --name good-pid1 debian:bookworm bash -c "while true; do sleep 1; done"
docker stop good-pid1
# Responde inmediatamente a SIGTERM
```

`--init` inyecta [tini](https://github.com/krallin/tini) como PID 1. Tini:

- Recibe todas las señales primero
- Las propaga correctamente a los procesos hijos
- Limpia procesos zombie (reaps child processes)
- Termina limpiamente con SIGTERM

### El problema de los zombies

En un sistema Linux normal, init se encarga de "reap" (recoger el estado de)
procesos hijos que terminan. Si un proceso padre no llama a `wait()` para
recoger el estado de un hijo, el hijo queda como "zombie" hasta que init lo limpia.

En un contenedor donde PID 1 es tu aplicación, si tu aplicación crea procesos
hijos que no esperan correctamente, esos hijos se convierten en zombies.

```bash
# Ejemplo: proceso que crea un zombie
docker run -d --name zombie-test debian:bookworm bash -c 'sleep 1 & exit'
docker ps -a --filter name=zombie-test
# Verás que el contenedor "exited" pero docker rm puede fallar si hay zombies

# Solución: usar --init o un proceso que espere hijos correctamente
docker run -d --init --name no-zombie debian:bookworm bash -c 'sleep 1 & wait'
docker rm no-zombie  # Funciona bien
```

## `docker start` y `docker restart`

### `docker start`

Reinicia un contenedor detenido. La capa de escritura se preserva: los archivos que
se crearon o modificaron antes del stop siguen ahí.

```bash
# Crear y ejecutar
docker run --name persist-test debian:bookworm bash -c "echo 'data' > /tmp/myfile"

# El contenedor terminó (exited)
docker ps -a --filter name=persist-test

# Iniciarlo de nuevo — la capa de escritura persiste
docker start persist-test

# docker start ejecuta el CMD original (no bash interactivo)
# En este caso: bash -c "echo 'data' > /tmp/myfile"
# Así que sobreescribe el archivo con "data"
docker logs persist-test
# Muestra "data" (ejecutó el mismo comando de nuevo)

docker rm persist-test
```

Para realmente verificar la persistencia de la capa de escritura:

```bash
# Contenedor interactivo que se detiene con Ctrl+D
docker run -it --name persist-demo debian:bookworm bash
# Dentro: echo "persistent data" > /tmp/testfile
# Dentro: ls -la /tmp/testfile
# Dentro: exit

# Reiniciar e inspeccionar
docker start -i persist-demo
# Dentro: cat /tmp/testfile
# "persistent data" ← la capa de escritura sobrevivió al stop/start
# Dentro: exit

# Eliminar y verificar que el archivo se pierde para siempre
docker rm persist-demo
# Ya no hay forma de recuperar /tmp/testfile
```

### `docker restart`

Equivale a `docker stop` + `docker start`, con un timeout configurable:

```bash
# Reiniciar con timeout de 5 segundos para SIGTERM
docker restart -t 5 my-container

# Reiniciar con timeout de 0 (SIGKILL inmediato, luego start)
docker restart -t 0 my-container

# También puedes especificar -a para adjuntar después de restart
docker restart -i my-container
```

### `docker run` combina create + start

`docker run` es simplemente la combinación de `docker create` + `docker start` + (opcionalmente) `docker attach`.

```bash
# Equivalencia:
docker run nginx:latest
# es lo mismo que:
docker create --name <random> nginx:latest && docker start <name>
```

Los flags de `docker run` incluyen todos los de `docker create` y `docker start`.

## La diferencia entre docker stop, docker kill y docker rm

### `docker stop`

Envía SIGTERM a PID 1, espera hasta 10 segundos (configurable con `-t`) a que
termine, y si no responde, envía SIGKILL.

```bash
# Timeout por defecto: 10 segundos
docker stop my-container

# Timeout custom: 30 segundos
docker stop -t 30 my-container

# Equivalente a:
docker kill --signal SIGTERM my-container
# después de -t segundos:
docker kill --signal SIGKILL my-container
```

### `docker kill`

Envía una señal directamente a PID 1. Por defecto, SIGKILL (no da chance de cleanup).

```bash
# SIGKILL inmediato
docker kill my-container

# Enviar otra señal
docker kill --signal SIGTERM my-container
docker kill --signal SIGUSR1 my-container  # Útil para forzar reload de config en algunos servicios
```

### `docker rm`

Elimina el contenedor. Por defecto, no elimina contenedores en estado `running`
(o `paused`) — hay que detenerlos primero.

```bash
# Error si está corriendo
docker rm my-running-container
# Error: You cannot remove a running container...

# Forzar eliminación (stop + rm)
docker rm -f my-container

# Equivale a:
docker stop my-container && docker rm my-container

# Eliminar al detener (flag en docker run)
docker run --rm nginx:latest
# Se elimina automáticamente cuando el contenedor termina

# Eliminar volúmenes asociados
docker rm -v my-container
# Por defecto, volúmenes anónimos se eliminan cuando el contenedor se elimina
# Los volúmenes nombrados sobreviven
```

## Práctica

### Ejercicio 1: Recorrer todos los estados

```bash
# 1. Crear (created)
docker create --name lifecycle debian:bookworm sleep 300
docker ps -a --filter name=lifecycle --format '{{.State}}'
# created

# 2. Iniciar (running)
docker start lifecycle
docker ps --filter name=lifecycle --format '{{.State}}'
# running

# 3. Pausar (paused)
docker pause lifecycle
docker ps --filter name=lifecycle --format '{{.State}}'
# paused

# 4. Reanudar (running)
docker unpause lifecycle
docker ps --filter name=lifecycle --format '{{.State}}'
# running

# 5. Detener (exited)
docker stop lifecycle
docker ps -a --filter name=lifecycle --format '{{.State}}'
# exited

# 6. Reiniciar (running)
docker start lifecycle
docker ps --filter name=lifecycle --format '{{.State}}'
# running

# 7. Eliminar (necesita estar detenido, o usar -f)
docker rm -f lifecycle
```

### Ejercicio 2: Exit codes

```bash
# Terminación normal (exit 0)
docker run --rm --name ec0 debian:bookworm true
echo "Exit code: $?"
# Exit code: 0

# Error genérico (exit 1)
docker run --rm --name ec1 debian:bookworm bash -c 'exit 1'
echo "Exit code: $?"
# Exit code: 1

# Comando no encontrado (exit 127)
docker run --rm --name ec127 debian:bookworm comando_inexistente
echo "Exit code: $?"
# Exit code: 127

# No ejecutable (exit 126)
docker run --rm --name ec126 debian:bookworm /etc/passwd
echo "Exit code: $?"
# Exit code: 126

# SIGKILL (137)
docker run -d --name ec137 debian:bookworm sleep 300
docker kill ec137
docker inspect ec137 --format '{{.State.ExitCode}}'
# 137
docker rm ec137

# SIGTERM con --init (143)
docker run -d --init --name ec143 debian:bookworm sleep 300
docker stop ec143
docker inspect ec143 --format '{{.State.ExitCode}}'
# 143
docker rm ec143

# SIGTERM sin --init (137 por timeout)
docker run -d --name ec143-noinit debian:bookworm bash -c 'while true; do sleep 1; done'
docker stop ec143-noinit
docker inspect ec143-noinit --format '{{.State.ExitCode}}'
# 137 (por timeout, docker stop envía SIGKILL)
docker rm ec143-noinit
```

### Ejercicio 3: Persistencia de la capa de escritura

```bash
# Crear un contenedor interactivo, escribir datos, salir
docker run -it --name layer-test debian:bookworm bash
# Dentro del contenedor:
echo "written during first run" > /tmp/evidence
ls -la /tmp/evidence
exit

# Verificar que está detenido, NO eliminado
docker ps -a --filter name=layer-test

# Reiniciar y verificar que el archivo sobrevivió
docker start -i layer-test
# Dentro del contenedor:
cat /tmp/evidence
# → "written during first run"
exit

# Eliminar y verificar que el archivo se pierde para siempre
docker rm layer-test
# Ya no hay forma de recuperar /tmp/evidence
```

### Ejercicio 4: El problema de PID 1 sin --init

```bash
# Contenedor que no responde a SIGTERM
docker run -d --name no-init debian:bookworm bash -c 'while true; do sleep 1; done'
echo "Started at $(date)"
docker stop no-init
echo "Stopped at $(date)"
# Verás que pasan ~10 segundos antes de que realmente se detenga

# Con --init responde inmediatamente
docker run -d --init --name with-init debian:bookworm bash -c 'while true; do sleep 1; done'
echo "Started at $(date)"
docker stop with-init
echo "Stopped at $(date)"
# Se detiene inmediatamente

# Limpiar
docker rm -f no-init with-init
```

### Ejercicio 5: Usar `docker create` para copiar archivos

```bash
# Crear un contenedor sin iniciar
docker create --name file-prep debian:bookworm /bin/true

# Copiar archivos antes de iniciar (útil para construir archivos de config)
docker cp myapp.conf file-prep:/etc/myapp.conf
docker cp scripts/ file-prep:/opt/scripts/

# Ahora iniciar
docker start file-prep

# Verificar
docker exec file-prep ls /etc/myapp.conf /opt/scripts/

# Limpiar
docker rm -f file-prep
```

### Ejercicio 6: Investigar el estado con inspect

```bash
# Crear y empezar varios contenedores
docker run -d --name state-demo debian:bookworm sleep 300
docker create --name state-demo-2 debian:bookworm sleep 300
docker pause state-demo

# Ver información completa de estado
docker inspect state-demo --format 'Status: {{.State.Status}}
PID: {{.State.Pid}}
ExitCode: {{.State.ExitCode}}
StartedAt: {{.State.StartedAt}}
FinishedAt: {{.State.FinishedAt}}
OOMKilled: {{.State.OOMKilled}}
Paused: {{.State.Paused}}
'

# Comparar con el creado (no iniciado)
docker inspect state-demo-2 --format 'Status: {{.State.Status}}
PID: {{.State.Pid}}
'

# Limpiar
docker rm -f state-demo state-demo-2
```
