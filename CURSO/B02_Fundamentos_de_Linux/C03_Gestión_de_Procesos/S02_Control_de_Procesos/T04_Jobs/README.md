# T04 — Jobs

## Job control

El **job control** es una funcionalidad del shell que permite gestionar múltiples
procesos desde un solo terminal: enviar procesos al fondo, traerlos de vuelta,
suspenderlos y reanudarlos.

```
Foreground (primer plano):
  - El proceso recibe input del teclado
  - Solo un job puede estar en foreground a la vez
  - Ctrl+C, Ctrl+Z, Ctrl+\ van al foreground process group

Background (segundo plano):
  - El proceso ejecuta sin recibir input del teclado
  - Múltiples jobs pueden estar en background simultáneamente
  - Si intenta leer del teclado, se detiene (SIGTTIN)
```

## Operaciones básicas

### Iniciar en background con &

```bash
# & al final del comando lo envía al background
sleep 300 &
# [1] 1234
#  ^   ^^^^
#  |   PID del proceso
#  job number

make -j4 &
# [2] 1235

# El shell imprime el job number y el PID
# El prompt vuelve inmediatamente — puedes seguir trabajando
```

### Suspender con Ctrl+Z

```bash
# Ctrl+Z envía SIGTSTP al foreground process
# El proceso se DETIENE (no termina)

vim largefile.txt
# (presionar Ctrl+Z)
# [1]+  Stopped                 vim largefile.txt

# El proceso está congelado en memoria
# Su estado en ps es T (stopped)
```

### Ver jobs activos

```bash
jobs
# [1]+  Stopped                 vim largefile.txt
# [2]   Running                 make -j4 &
# [3]-  Running                 sleep 300 &

# + = job actual (default para fg/bg sin argumento)
# - = job anterior

# jobs -l: incluir PIDs
jobs -l
# [1]+  1234 Stopped              vim largefile.txt
# [2]   1235 Running              make -j4 &
# [3]-  1236 Running              sleep 300 &

# jobs -p: solo PIDs
jobs -p
# 1234
# 1235
# 1236
```

### bg — Reanudar en background

```bash
# Reanudar el job detenido en background
bg           # reanuda el job marcado con +
bg %1        # reanuda el job 1 específicamente

# El proceso recibe SIGCONT y continúa ejecutando
# pero en background (no recibe input del teclado)

# Flujo típico:
make -j4          # inicia en foreground
# (Ctrl+Z)        # suspender
# [1]+  Stopped   make -j4
bg                # reanudar en background
# [1]+ make -j4 &
```

### fg — Traer al foreground

```bash
# Traer un job al foreground
fg           # trae el job marcado con +
fg %1        # trae el job 1

# Ahora el proceso recibe input del teclado
# y Ctrl+C/Ctrl+Z lo afectan directamente
```

### Especificar jobs

```bash
%1        # job número 1
%2        # job número 2
%+        # job actual (último suspendido o puesto en bg)
%%        # igual que %+
%-        # job anterior
%string   # job cuyo comando empieza con "string"
%?string  # job cuyo comando contiene "string"

# Ejemplos
fg %1           # traer job 1
kill %2         # matar job 2
bg %vim         # reanudar el job que empieza con "vim"
fg %?largefile  # traer el job que contiene "largefile"
```

## El problema: cerrar el terminal

Cuando se cierra un terminal (o se hace logout), el kernel envía **SIGHUP** a
todos los procesos del session group. Resultado: todos los procesos background
mueren:

```
Terminal abierto:
  shell (session leader)
    ├── job 1 (background)
    ├── job 2 (background)
    └── job 3 (foreground)

Terminal cerrado → SIGHUP a todos:
  shell recibe SIGHUP → reenvía SIGHUP a todos sus jobs → todos mueren
```

Hay varias formas de evitar esto.

## nohup — Ignorar SIGHUP

`nohup` ejecuta un comando inmune a SIGHUP:

```bash
# Sintaxis
nohup comando [argumentos] &

# Ejemplo
nohup python3 /srv/app/server.py &
# nohup: ignoring input and appending output to 'nohup.out'
# [1] 1234

# Lo que hace nohup:
# 1. Hace que el proceso ignore SIGHUP
# 2. Redirige stdout a nohup.out (si stdout es un terminal)
# 3. Redirige stderr a stdout (si stderr es un terminal)
# 4. No cambia stdin (el proceso sigue conectado al terminal para input)
```

### Redireccionar output con nohup

```bash
# nohup escribe a nohup.out por defecto — mejor especificar el archivo
nohup python3 server.py > /var/log/server.log 2>&1 &

# Desglose:
# > /var/log/server.log    stdout va al log
# 2>&1                     stderr va donde va stdout (al log)
# &                        background

# Con stdout y stderr separados
nohup python3 server.py > /var/log/server.out 2> /var/log/server.err &
```

### Limitaciones de nohup

```bash
# nohup solo protege contra SIGHUP
# El proceso AÚN puede ser matado por SIGTERM, SIGKILL, etc.

# nohup no desacopla el proceso del terminal completamente:
# - Sigue perteneciendo al session group del shell
# - Si el shell muere, el proceso queda huérfano
#   (es re-parenteado a PID 1)
# - Esto funciona, pero no es tan limpio como un daemon real
```

## disown — Desasociar del shell

`disown` elimina un job de la tabla de jobs del shell. El shell ya no le
enviará SIGHUP cuando se cierre:

```bash
# Iniciar en background
python3 server.py &
# [1] 1234

# Desasociar del shell
disown %1
# o
disown        # desasocia el job actual

# Ahora no aparece en jobs
jobs
# (vacío)

# Pero el proceso sigue corriendo
ps -p 1234
```

### disown -h

```bash
# -h: no enviar SIGHUP al cerrar, pero mantener en la tabla de jobs
disown -h %1

# El job sigue apareciendo en jobs
jobs
# [1]+  Running    python3 server.py &

# Pero no recibirá SIGHUP al cerrar el terminal
```

### nohup vs disown

| Aspecto | nohup | disown |
|---|---|---|
| Cuándo se usa | Al INICIAR el proceso | DESPUÉS de iniciar |
| Qué hace | Ignora SIGHUP en el proceso | Quita el job del shell |
| Output | Redirige a nohup.out | No redirige — el output se pierde al cerrar |
| Disponibilidad | Comando externo | Builtin del shell (bash/zsh) |

```bash
# Patrón típico cuando olvidaste nohup:
python3 server.py        # ups, lo inicié sin nohup
# Ctrl+Z
bg                       # reanudar en background
disown -h %1             # proteger contra SIGHUP

# Pero el output se perderá al cerrar el terminal
# Mejor haber usado nohup desde el inicio
```

## setsid — Crear nueva sesión

`setsid` ejecuta un programa en una **nueva sesión** completamente desacoplada
del terminal:

```bash
setsid python3 server.py &

# Lo que hace:
# 1. Crea una nueva sesión (el proceso es session leader)
# 2. No tiene terminal de control (no recibirá SIGHUP del terminal)
# 3. Es más limpio que nohup para daemons manuales
```

## screen y tmux — Terminales persistentes

Para procesos interactivos de larga duración (no daemons), la solución
correcta es un multiplexor de terminal:

```bash
# tmux: crear una sesión
tmux new -s trabajo

# (ejecutar comandos dentro de tmux)
make -j4

# Desconectar de tmux (Ctrl+B, luego D)
# El terminal tmux sigue vivo en background

# Reconectar desde cualquier terminal (incluso otra sesión SSH)
tmux attach -t trabajo
# o
tmux a -t trabajo

# Listar sesiones activas
tmux ls
```

```bash
# screen: equivalente más antiguo
screen -S trabajo

# Desconectar: Ctrl+A, luego D
# Reconectar:
screen -r trabajo
```

tmux/screen mantienen los procesos vivos porque el proceso corre dentro del
servidor tmux/screen, no dentro del terminal SSH. Cerrar SSH no afecta al
servidor tmux.

## Comparación de métodos

| Método | Sobrevive logout | Interactivo | Output | Complejidad |
|---|---|---|---|---|
| `&` solo | No | No | Se pierde | Mínima |
| `nohup cmd &` | Sí | No | nohup.out | Baja |
| `disown` | Sí | No | Se pierde | Baja |
| `setsid` | Sí | No | Se pierde | Baja |
| `tmux`/`screen` | Sí | Sí | Visible al reconectar | Media |
| systemd service | Sí | No | journal | Media-Alta |

Para servicios en producción, la respuesta correcta es casi siempre un unit
file de systemd, no nohup ni screen.

## Diferencias entre shells

| Funcionalidad | bash | zsh |
|---|---|---|
| Job control | Sí | Sí |
| disown | Sí | Sí |
| `setopt NO_HUP` | — | Sí (no envía SIGHUP al cerrar) |
| `setopt NO_CHECK_JOBS` | — | Sí (no avisa de jobs al salir) |
| Aviso al salir con jobs | "There are stopped jobs" | Configurable |

```bash
# bash: si intentas salir con jobs pendientes
exit
# There are running jobs.
# (hay que hacer exit otra vez para forzar)

# zsh: se puede desactivar el aviso
setopt NO_CHECK_JOBS
setopt NO_HUP          # no enviar SIGHUP a los jobs
```

---

## Ejercicios

### Ejercicio 1 — Job control básico

```bash
# Iniciar tres jobs
sleep 300 &
sleep 301 &
vim /tmp/test-jobs

# En vim, presionar Ctrl+Z para suspender
# Ver los jobs
jobs -l

# Reanudar vim en foreground
fg %3
# Salir de vim (:q)

# Matar los sleeps
kill %1 %2
jobs
```

### Ejercicio 2 — nohup y disown

```bash
# Con nohup
nohup sleep 400 > /tmp/nohup-test.log 2>&1 &
PID1=$!

# Sin nohup, luego disown
sleep 401 &
PID2=$!
disown %sleep

# Verificar que ambos corren
ps -p $PID1,$PID2 -o pid,cmd

# ¿Aparecen en jobs?
jobs

# Limpiar
kill $PID1 $PID2
rm -f /tmp/nohup-test.log
```

### Ejercicio 3 — Proceso suspendido vs background

```bash
# Iniciar un proceso
ping localhost > /tmp/ping-test.log &
PID=$!

# Suspenderlo
kill -STOP $PID
# ¿Qué dice jobs?
jobs -l
# ¿Crece el log?
wc -l /tmp/ping-test.log; sleep 2; wc -l /tmp/ping-test.log

# Reanudarlo
kill -CONT $PID
# ¿Ahora crece?
sleep 2; wc -l /tmp/ping-test.log

# Limpiar
kill $PID
rm /tmp/ping-test.log
```
