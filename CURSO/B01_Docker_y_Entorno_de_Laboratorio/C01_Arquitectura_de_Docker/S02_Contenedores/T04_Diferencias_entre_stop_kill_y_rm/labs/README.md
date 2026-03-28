# Lab — Diferencias entre stop, kill y rm

## Objetivo

Laboratorio practico para verificar de forma empirica las diferencias entre
`docker stop` (SIGTERM con grace period), `docker kill` (SIGKILL inmediato),
y `docker rm` (eliminacion del contenedor y su capa de escritura). Incluye
un programa en C que atrapa SIGTERM para demostrar graceful shutdown.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull debian:bookworm`
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `README.md` | Este documento: guia paso a paso |
| `signal-handler.c` | Programa en C que registra un handler para SIGTERM |
| `Dockerfile.signal-handler` | Imagen que compila y ejecuta signal-handler.c |

---

## Parte 1 — `stop` vs `kill`

### Objetivo

Demostrar que `docker stop` envia SIGTERM con un grace period (default 10s)
seguido de SIGKILL, mientras que `docker kill` envia SIGKILL de forma
inmediata.

### Paso 1.1: `docker stop` con un proceso que no maneja SIGTERM

```bash
docker run -d --name stop-test debian:bookworm bash -c "while true; do sleep 1; done"
```

Antes de ejecutar `docker stop`, predice:

- ¿Cuanto tardara `docker stop` si bash como PID 1 no maneja SIGTERM?
- ¿Cual sera el exit code?

Intenta predecir antes de continuar al siguiente paso.

```bash
time docker stop stop-test
```

Salida esperada:

```
stop-test
real    ~10.XXXs
```

Tardo aproximadamente 10 segundos. Bash como PID 1 ignora SIGTERM (no tiene
handler). Docker espero 10 segundos (grace period default) y luego envio
SIGKILL.

```bash
docker inspect stop-test --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 137
```

137 = 128 + 9 (SIGKILL). El proceso fue matado por SIGKILL porque no
respondio a SIGTERM.

### Paso 1.2: `docker kill` es inmediato

```bash
docker rm stop-test
docker run -d --name kill-test debian:bookworm bash -c "while true; do sleep 1; done"
time docker kill kill-test
```

Salida esperada:

```
kill-test
real    ~0.XXXs
```

`docker kill` envia SIGKILL de forma inmediata, sin grace period. El proceso
no tiene oportunidad de hacer cleanup.

```bash
docker inspect kill-test --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 137
```

Mismo exit code (137 = SIGKILL), pero la diferencia es el **tiempo**:
`stop` espero 10s, `kill` fue inmediato.

### Paso 1.3: `docker stop` con timeout reducido

```bash
docker rm kill-test
docker run -d --name stop-quick debian:bookworm bash -c "while true; do sleep 1; done"
time docker stop -t 2 stop-quick
```

Salida esperada:

```
stop-quick
real    ~2.XXXs
```

Con `-t 2`, Docker solo espera 2 segundos antes de enviar SIGKILL.

### Paso 1.4: `docker stop -t 0` (SIGTERM + SIGKILL casi simultaneos)

```bash
docker rm stop-quick
docker run -d --name stop-zero debian:bookworm bash -c "while true; do sleep 1; done"
time docker stop -t 0 stop-zero
```

Salida esperada:

```
stop-zero
real    ~0.XXXs
```

Con `-t 0`, Docker envia SIGTERM y casi inmediatamente SIGKILL. El resultado
practico es similar a `docker kill`, pero tecnicamente se envia SIGTERM
primero.

### Paso 1.5: Limpiar

```bash
docker rm stop-zero
```

---

## Parte 2 — Manejo de senales con un programa en C

### Objetivo

Usar un programa en C que atrapa SIGTERM para demostrar la diferencia entre
graceful shutdown (stop) y terminacion forzada (kill).

### Paso 2.1: Examinar el codigo fuente

```bash
cat signal-handler.c
```

El programa:
1. Registra un handler para SIGTERM usando `sigaction`
2. Entra en un loop infinito esperando senales
3. Al recibir SIGTERM, imprime un mensaje, simula cleanup (2 segundos), y
   sale con exit code 0

Antes de construir, predice:

- Cuando ejecutes `docker stop`, ¿cuanto tardara?
- ¿Cual sera el exit code?
- ¿Y si usas `docker kill` en vez de `docker stop`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2: Construir la imagen

```bash
cat Dockerfile.signal-handler
```

Observa que el Dockerfile compila el programa y luego desinstala gcc en el
mismo `RUN` para mantener la imagen pequena (patron de la seccion S01).

```bash
docker build -f Dockerfile.signal-handler -t lab-signal-handler .
```

### Paso 2.3: Graceful shutdown con `docker stop`

```bash
docker run -d --name signal-graceful lab-signal-handler
```

Espera un par de segundos para que el programa arranque y registre el handler.

```bash
docker logs signal-graceful
```

Salida esperada:

```
[PID 1] Signal handler registered for SIGTERM
[PID 1] Running... send SIGTERM to trigger graceful shutdown
```

```bash
time docker stop signal-graceful
```

Salida esperada:

```
signal-graceful
real    ~2.XXXs
```

Tardo aproximadamente 2-3 segundos (no los 10 del grace period). El programa
recibio SIGTERM, ejecuto su cleanup, y salio limpiamente antes del timeout.

```bash
docker logs signal-graceful
```

Salida esperada:

```
[PID 1] Signal handler registered for SIGTERM
[PID 1] Running... send SIGTERM to trigger graceful shutdown
[PID 1] SIGTERM received — starting graceful shutdown...
[PID 1] Cleaning up resources...
[PID 1] Cleanup complete. Exiting with code 0.
```

Los tres mensajes de shutdown confirman que el proceso tuvo oportunidad de
hacer cleanup.

```bash
docker inspect signal-graceful --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 0
```

Exit code 0: terminacion limpia. El programa decidio su propio exit code.

### Paso 2.4: Terminacion forzada con `docker kill`

```bash
docker rm signal-graceful
docker run -d --name signal-forced lab-signal-handler
```

Espera un par de segundos.

```bash
docker logs signal-forced
```

Salida esperada:

```
[PID 1] Signal handler registered for SIGTERM
[PID 1] Running... send SIGTERM to trigger graceful shutdown
```

```bash
time docker kill signal-forced
```

Salida esperada:

```
signal-forced
real    ~0.XXXs
```

Terminacion inmediata.

```bash
docker logs signal-forced
```

Salida esperada:

```
[PID 1] Signal handler registered for SIGTERM
[PID 1] Running... send SIGTERM to trigger graceful shutdown
```

**No hay mensajes de cleanup.** SIGKILL no se puede atrapar — el kernel
termina el proceso inmediatamente. El handler de SIGTERM nunca se ejecuto.

```bash
docker inspect signal-forced --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 137
```

137 = 128 + 9 (SIGKILL). El proceso no tuvo oportunidad de elegir su
exit code.

### Paso 2.5: Comparacion directa

| Aspecto | `docker stop` | `docker kill` |
|---|---|---|
| Senal enviada | SIGTERM (luego SIGKILL si timeout) | SIGKILL |
| Handler ejecutado | Si | No |
| Mensajes de cleanup | Si (3 mensajes) | No (0 mensajes) |
| Tiempo | ~2-3s (el proceso termino limpiamente) | ~0s |
| Exit code | 0 (decidido por el programa) | 137 (SIGKILL forzado) |

### Paso 2.6: Limpiar

```bash
docker rm signal-forced
```

---

## Parte 3 — `rm` vs `rm -f`

### Objetivo

Demostrar que `docker rm` requiere un contenedor detenido, que `docker rm -f`
mata y elimina en un paso, y que ambos destruyen la capa de escritura
permanentemente.

### Paso 3.1: `docker rm` con contenedor detenido

```bash
docker run --name rm-stopped debian:bookworm bash -c "echo 'data' > /tmp/file.txt"

# El contenedor esta en estado exited
docker ps -a --filter name=rm-stopped --format 'table {{.Names}}\t{{.Status}}'
```

Salida esperada:

```
NAMES        STATUS
rm-stopped   Exited (0) X seconds ago
```

```bash
docker rm rm-stopped
```

Funciona sin problemas. El contenedor y su capa de escritura se eliminaron.

### Paso 3.2: `docker rm` con contenedor corriendo — falla

```bash
docker run -d --name rm-running debian:bookworm sleep 300
docker rm rm-running
```

Salida esperada:

```
Error response from daemon: cannot remove container "/rm-running": container is running: ...
```

`docker rm` no puede eliminar un contenedor que esta corriendo. Hay que
detenerlo primero o usar `-f`.

### Paso 3.3: `docker rm -f` mata y elimina

Antes de ejecutar, predice: ¿cual sera la senal enviada por `docker rm -f`?
¿SIGTERM o SIGKILL?

Intenta predecir antes de continuar al siguiente paso.

```bash
docker rm -f rm-running
docker ps -a --filter name=rm-running
```

Salida esperada:

```
CONTAINER ID   IMAGE   COMMAND   CREATED   STATUS   PORTS   NAMES
```

`docker rm -f` envio SIGKILL (no SIGTERM) y elimino el contenedor. No hay
grace period. Es equivalente a `docker kill` + `docker rm`.

### Paso 3.4: `stop` + `rm` vs `rm -f` — ¿cual permite cleanup?

```bash
# Opcion 1: stop + rm (permite cleanup)
docker run -d --name rm-data lab-signal-handler
sleep 2
docker stop rm-data
docker logs rm-data
```

Salida esperada (ultimas lineas):

```
[PID 1] SIGTERM received — starting graceful shutdown...
[PID 1] Cleaning up resources...
[PID 1] Cleanup complete. Exiting with code 0.
```

```bash
docker rm rm-data
```

```bash
# Opcion 2: rm -f (NO permite cleanup)
docker run -d --name rm-data lab-signal-handler
sleep 2
docker rm -f rm-data
```

La diferencia critica:
- `docker stop` + `docker rm`: el proceso recibe SIGTERM, hace cleanup, y
  luego se elimina el contenedor
- `docker rm -f`: el proceso recibe SIGKILL (no capturable), se muere
  inmediatamente, y se elimina el contenedor. No hay cleanup.

### Paso 3.5: Limpiar

```bash
docker rm -f rm-stopped rm-running rm-data 2>/dev/null
```

---

## Parte 4 — `--stop-timeout` y `--init`

### Objetivo

Configurar el grace period de `docker stop` a nivel de contenedor con
`--stop-timeout`, y demostrar el efecto de `--init` en la propagacion de
senales.

### Paso 4.1: Sin `--init` — bash no propaga SIGTERM

```bash
docker run -d --name no-init-demo debian:bookworm bash -c "sleep 300"
time docker stop no-init-demo
```

Salida esperada:

```
no-init-demo
real    ~10.XXXs
```

Bash como PID 1 no propaga SIGTERM a `sleep`. Docker espero los 10 segundos
completos y luego envio SIGKILL.

```bash
docker inspect no-init-demo --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 137
```

### Paso 4.2: Con `--init` — tini propaga SIGTERM

```bash
docker rm no-init-demo
docker run -d --init --name init-demo debian:bookworm bash -c "sleep 300"
time docker stop init-demo
```

Salida esperada:

```
init-demo
real    ~0.XXXs
```

Con `--init`, tini es PID 1 y propaga SIGTERM correctamente a los procesos
hijos. `sleep` recibe SIGTERM y termina inmediatamente.

```bash
docker inspect init-demo --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 143
```

143 = 128 + 15 (SIGTERM). El proceso termino por SIGTERM, no por SIGKILL.

### Paso 4.3: Comparar sin y con `--init`

| Aspecto | Sin `--init` | Con `--init` |
|---|---|---|
| PID 1 | bash | tini |
| Propaga SIGTERM | No | Si |
| Tiempo de stop | ~10s (espera timeout, luego SIGKILL) | <1s (SIGTERM propagado) |
| Exit code | 137 (SIGKILL) | 143 (SIGTERM) |

### Paso 4.4: `--stop-timeout` a nivel de contenedor

```bash
docker rm init-demo
docker run -d --name timeout-demo --stop-timeout 3 \
    debian:bookworm bash -c "while true; do sleep 1; done"
time docker stop timeout-demo
```

Salida esperada:

```
timeout-demo
real    ~3.XXXs
```

`--stop-timeout` configura el grace period a nivel del contenedor. Es
equivalente a pasar `-t 3` en cada `docker stop`, pero se establece una
sola vez al crear el contenedor.

### Paso 4.5: Alternativa a `--init`: `exec` en scripts

```bash
docker rm timeout-demo
docker run -d --name exec-pid1 debian:bookworm bash -c "exec sleep 300"
time docker stop exec-pid1
```

Salida esperada:

```
exec-pid1
real    ~0.XXXs
```

`exec` reemplaza bash por sleep, haciendo que sleep sea PID 1 directamente.
SIGTERM llega a sleep sin intermediarios.

```bash
docker inspect exec-pid1 --format 'ExitCode: {{.State.ExitCode}}'
```

Salida esperada:

```
ExitCode: 143
```

### Paso 4.6: Resumen de soluciones para PID 1

| Solucion | Como funciona | Uso recomendado |
|---|---|---|
| `--init` (tini) | Tini como PID 1, propaga senales | Shell scripts, multiples procesos |
| `exec` en script | Reemplaza shell por el proceso | Un solo proceso final |
| Programa con handler | El propio programa maneja SIGTERM | Aplicaciones C, Rust, Go |

### Paso 4.7: Limpiar

```bash
docker rm -f exec-pid1
docker rmi lab-signal-handler
```

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f stop-test kill-test stop-quick stop-zero \
    signal-graceful signal-forced rm-stopped rm-running rm-data \
    no-init-demo init-demo timeout-demo exec-pid1 2>/dev/null

# Eliminar imagen del lab
docker rmi lab-signal-handler 2>/dev/null
```

---

## Conceptos reforzados

1. `docker stop` envia SIGTERM y espera un grace period (default 10s). Si
   PID 1 no termina, envia SIGKILL. `docker kill` envia SIGKILL de forma
   inmediata sin grace period.
2. Un programa que registra un handler para SIGTERM puede hacer cleanup
   (cerrar conexiones, flush buffers, liberar recursos) antes de salir.
   SIGKILL **no se puede atrapar** — es terminacion a nivel de kernel.
3. El exit code 0 (terminacion limpia por handler) vs 137 (SIGKILL forzado)
   permite distinguir si un proceso tuvo oportunidad de hacer cleanup.
4. `docker rm` requiere un contenedor detenido. `docker rm -f` envia SIGKILL
   y elimina — no permite cleanup. `docker stop` + `docker rm` si permite
   cleanup.
5. Bash como PID 1 **no propaga** SIGTERM a procesos hijos. Soluciones:
   `--init` (tini), `exec` en scripts, o programas con handler propio.
6. `--stop-timeout` configura el grace period a nivel del contenedor. `-t`
   en `docker stop` lo configura por invocacion.
7. La capa de escritura se preserva con `stop`/`start` pero se destruye
   permanentemente con `rm`. Para datos persistentes se necesitan volumenes.
