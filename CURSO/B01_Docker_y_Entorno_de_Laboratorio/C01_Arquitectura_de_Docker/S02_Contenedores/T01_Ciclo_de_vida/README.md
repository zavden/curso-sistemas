# T01 — Ciclo de vida

## ¿Qué es un contenedor?

Un contenedor es una **instancia en ejecución de una imagen**. Si la imagen es la
receta, el contenedor es el plato servido. Técnicamente, un contenedor es:

1. Las capas de solo lectura de la imagen
2. Una **capa de escritura** (writable layer) encima
3. Un **proceso aislado** que ve el filesystem resultante

Un contenedor no es una máquina virtual. No tiene un kernel propio, no emula hardware,
y no ejecuta un sistema operativo completo. Es un proceso Linux normal con aislamiento
proporcionado por **namespaces** (lo que el proceso puede ver) y **cgroups** (cuántos
recursos puede usar).

## Estados del contenedor

Un contenedor pasa por los siguientes estados durante su existencia:

```
docker create       docker start       docker pause
     │                   │                  │
     ▼                   ▼                  ▼
 ┌─────────┐       ┌──────────┐       ┌──────────┐
 │ created │──────▶│ running  │──────▶│  paused  │
 └─────────┘       └──────────┘       └──────────┘
                     │    ▲                 │
                     │    │   docker unpause│
                     │    └─────────────────┘
                     │
           PID 1 termina / docker stop / docker kill
                     │
                     ▼
               ┌──────────┐       docker rm       ┌──────────┐
               │  exited  │─────────────────────▶ │ (removed)│
               └──────────┘                       └──────────┘
```

### created

El contenedor existe pero nunca se ha iniciado. Se creó con `docker create`, que:
- Reserva la capa de escritura
- Asigna configuración de red (IP, hostname)
- Prepara los mount points (volúmenes)
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

### running

El proceso principal (PID 1) está activo. Se llega aquí con `docker start` o
`docker run`.

```bash
# Iniciar el contenedor creado
docker start my-container

# O crear + iniciar en un solo paso
docker run --name another debian:bookworm sleep 60
```

El contenedor permanece en `running` mientras PID 1 esté vivo.

### paused

El proceso está **congelado** en el estado exacto en que estaba. No ejecuta
instrucciones, no consume CPU, pero mantiene toda la memoria.

Docker usa el **cgroups freezer** para pausar, que envía internamente SIGSTOP a todos
los procesos del contenedor.

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

### exited (stopped)

PID 1 terminó (por cuenta propia, por señal, o por `docker stop`/`docker kill`).
El contenedor conserva:
- Su capa de escritura (todos los archivos modificados/creados)
- Sus logs
- Su configuración

```bash
# Un contenedor que termina naturalmente
docker run --name echo-test debian:bookworm echo "done"

docker ps -a --filter name=echo-test --format '{{.Status}}'
# Exited (0) 2 seconds ago
```

El número entre paréntesis es el **exit code** de PID 1.

### dead

Estado raro. Ocurre cuando Docker no puede eliminar un contenedor (por ejemplo,
un filesystem montado que está busy). Requiere intervención manual.

```bash
# Ver contenedores en estado dead (si existen)
docker ps -a --filter status=dead
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

| Exit code | Significado |
|-----------|------------|
| 0 | Terminación exitosa |
| 1 | Error genérico de la aplicación |
| 2 | Uso incorrecto del shell/comando |
| 126 | Comando encontrado pero no ejecutable (permisos) |
| 127 | Comando no encontrado |
| 130 | 128 + 2 = SIGINT (Ctrl+C) |
| 137 | 128 + 9 = SIGKILL (`docker kill` o OOM killer) |
| 143 | 128 + 15 = SIGTERM (`docker stop` exitoso) |

La fórmula para señales: exit code = 128 + número de señal.

### PID 1 y el manejo de señales

En Linux, PID 1 tiene un comportamiento especial: **ignora señales que no tiene
handler para manejar**. Esto significa que si PID 1 es un script de bash que no
atrapa SIGTERM:

```bash
# Este contenedor NO responde a docker stop (SIGTERM)
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
- Propaga señales a los procesos hijos
- Limpia procesos zombie (reaps child processes)
- Termina limpiamente con SIGTERM

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

# ¿Pero el archivo sigue? Depende: docker start ejecuta el CMD original
# (que era bash -c "echo 'data' > /tmp/myfile"), no bash interactivo.
# Para verificar:
docker start persist-test
docker logs persist-test
# Muestra "data" (ejecutó el mismo comando de nuevo)

docker rm persist-test
```

Para realmente verificar la persistencia de la capa de escritura:

```bash
# Contenedor interactivo que se detiene
docker run -it --name persist-demo debian:bookworm bash
# Dentro: echo "persistent data" > /tmp/testfile
# Dentro: exit

# Reiniciar e inspeccionar
docker start -i persist-demo
# Dentro: cat /tmp/testfile
# "persistent data" ← la capa de escritura sobrevivió al stop/start

# Limpiar
docker rm persist-demo
```

### `docker restart`

Equivale a `docker stop` + `docker start`, con un timeout configurable:

```bash
# Reiniciar con timeout de 5 segundos para SIGTERM
docker restart -t 5 my-container

# Reiniciar con timeout de 0 (SIGKILL inmediato, luego start)
docker restart -t 0 my-container
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
# Terminación normal
docker run --rm --name ec0 debian:bookworm true
echo "Exit code: $?"
# Exit code: 0

# Comando no encontrado
docker run --rm --name ec127 debian:bookworm comando_inexistente
echo "Exit code: $?"
# Exit code: 127

# SIGKILL (137)
docker run -d --name ec137 debian:bookworm sleep 300
docker kill ec137
docker inspect ec137 --format '{{.State.ExitCode}}'
# 137
docker rm ec137

# SIGTERM manejado (143)
docker run -d --init --name ec143 debian:bookworm sleep 300
docker stop ec143
docker inspect ec143 --format '{{.State.ExitCode}}'
# 143
docker rm ec143
```

### Ejercicio 3: Persistencia de la capa de escritura

```bash
# Crear un contenedor interactivo, escribir datos, salir
docker run -it --name layer-test debian:bookworm bash
# Dentro del contenedor:
#   echo "written during first run" > /tmp/evidence
#   ls -la /tmp/evidence
#   exit

# Verificar que está detenido, NO eliminado
docker ps -a --filter name=layer-test

# Reiniciar y verificar que el archivo sobrevivió
docker start -i layer-test
# Dentro del contenedor:
#   cat /tmp/evidence
#   → "written during first run"
#   exit

# Eliminar y verificar que el archivo se pierde para siempre
docker rm layer-test
# Ya no hay forma de recuperar /tmp/evidence
```
