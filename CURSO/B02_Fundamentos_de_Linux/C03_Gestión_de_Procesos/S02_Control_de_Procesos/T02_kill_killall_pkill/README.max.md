# T02 — kill, killall, pkill (Enhanced)

## kill — Enviar señal por PID

`kill` es la herramienta base para enviar señales. A pesar de su nombre, no
solo mata procesos — envía cualquier señal:

```bash
# Sintaxis
kill [-señal] PID [PID ...]

# Enviar SIGTERM (default)
kill 1234

# Enviar una señal específica
kill -9 1234          # por número
kill -SIGKILL 1234    # por nombre con SIG
kill -KILL 1234       # por nombre sin SIG
kill -s KILL 1234     # con -s

# Todas son equivalentes: enviar SIGKILL al PID 1234
```

### Enviar a múltiples procesos

```bash
# Varios PIDs
kill 1234 1235 1236

# Todos los procesos de un usuario (peligroso)
kill -TERM -1         # -1 = todos los procesos que el usuario puede señalizar

# A un process group (PID negativo)
kill -TERM -1234      # envía a todo el process group 1234
```

### Process groups y kill negativo

Cuando se ejecuta un pipeline o un grupo de comandos, el shell crea un
**process group**. Se puede enviar señal a todo el grupo usando PID negativo:

```bash
# Ver el PGID de un proceso
ps -o pid,pgid,cmd -p 1234

# Ejemplo: matar un pipeline completo
cat /dev/urandom | gzip > /dev/null &
# [1] 1234
# Esto crea un process group. cat y gzip tienen el mismo PGID

ps -o pid,pgid,cmd --forest -g $(ps -o pgid= -p $!)
# 1234 1234 cat /dev/urandom
# 1235 1234 gzip

kill -- -1234    # mata cat Y gzip (todo el group)
# El -- evita que -1234 se interprete como una opción
```

### Listar señales

```bash
# Lista de todas las señales disponibles
kill -l
#  1) SIGHUP       2) SIGINT       3) SIGQUIT  ...
# 15) SIGTERM     ...

# Número de una señal por nombre
kill -l TERM
# 15

# Nombre de una señal por número
kill -l 15
# TERM
```

### kill es un builtin

```bash
# En bash y zsh, kill es un builtin del shell Y un comando externo
type kill
# kill is a shell builtin

which kill
# /usr/bin/kill (o /bin/kill)

# El builtin puede enviar señales usando job IDs (%1, %2...)
kill %1          # mata el job 1

# El comando externo (/usr/bin/kill) solo acepta PIDs
/usr/bin/kill 1234

# En situaciones extremas (fork bomb, sistema sin memoria),
# el builtin funciona cuando el comando externo no puede ejecutarse
# porque no se puede crear un nuevo proceso
```

## killall — Enviar señal por nombre exacto

`killall` envía una señal a **todos los procesos** que coincidan con el nombre
del ejecutable:

```bash
# Sintaxis
killall [-señal] nombre

# Matar todos los procesos con nombre exacto "nginx"
killall nginx

# Equivale a: kill $(pidof nginx)

# Con señal específica
killall -9 nginx
killall -KILL nginx

# Solo procesos de un usuario
killall -u dev python3
```

### Opciones útiles

```bash
# -e / --exact: coincidencia exacta (por defecto solo compara 15 chars)
killall -e process-with-long-name

# -i / --interactive: pedir confirmación para cada proceso
killall -i python3
# Kill python3(1234) ? (y/N)
# Kill python3(1235) ? (y/N)

# -w / --wait: esperar a que todos los procesos mueran
killall -w nginx

# -r / --regexp: usar expresión regular
killall -r 'worker[0-9]+'

# -v / --verbose: mostrar qué se señalizó
killall -v nginx
# Killed nginx(1234) with signal 15
```

### Peligro: killall en otros Unix

En **Solaris y otros Unix**, `killall` sin argumentos mata **todos los
procesos del sistema**. En Linux (paquete `psmisc`), `killall` requiere un
nombre de proceso. Si trabajas en sistemas no-Linux, verificar qué versión de
`killall` tienes antes de usarlo.

### killall vs kill

| Aspecto | kill | killall |
|---|---|---|
| Identificador | PID | Nombre del proceso |
| Alcance | Un proceso (o lista) | Todos con ese nombre |
| Precisión | Exacta | Puede matar procesos no deseados |
| Seguridad | Mayor | Verificar antes de usar en producción |

## pkill — Enviar señal por patrón

`pkill` envía señales basándose en patrones de coincidencia. Es más flexible
que `killall`:

```bash
# Sintaxis
pkill [-señal] [opciones] patrón

# Coincidencia parcial por defecto (regex)
pkill nginx          # mata nginx, nginx-worker, etc.

# Coincidencia exacta
pkill -x nginx       # solo procesos nombrados exactamente "nginx"

# Con señal específica
pkill -9 nginx
pkill -KILL nginx
```

### Buscar en la línea de comando completa

```bash
# Por defecto, pkill busca solo en el nombre del proceso (comm)
# -f busca en toda la línea de comando (cmdline)

pkill python3                    # mata cualquier python3
pkill -f "python3 /srv/app.py"  # solo el que ejecuta app.py
pkill -f "gunicorn.*myproject"   # patrón en argumentos

# Sin -f, "pkill python3" mataría todos los procesos Python
# Con -f, se puede ser específico sobre cuál matar
```

### Filtros de pkill

```bash
# Por usuario
pkill -u dev python3        # solo procesos de dev
pkill -u dev,root nginx     # procesos de dev o root

# Por terminal
pkill -t pts/0              # procesos en pts/0

# Por grupo
pkill -G developers         # por grupo real

# Por padre
pkill -P 1234               # solo hijos de PID 1234

# Más nuevo/viejo que
pkill -n nginx              # solo el más nuevo
pkill -o nginx              # solo el más viejo

# Combinando filtros
pkill -u dev -f "python3.*worker"   # procesos de dev con ese patrón
```

### pgrep — El compañero de pkill

`pgrep` usa la misma sintaxis que `pkill` pero solo **muestra** PIDs sin
enviar señales. Usarlo siempre para verificar antes de `pkill`:

```bash
# Ver qué procesos coinciden ANTES de matarlos
pgrep -la nginx
# 1234 nginx: master process /usr/sbin/nginx
# 1235 nginx: worker process
# 1236 nginx: worker process

# -l: mostrar nombre del proceso junto al PID
# -a: mostrar la línea de comando completa

# Contar procesos
pgrep -c nginx
# 3

# Verificar que un patrón -f es correcto
pgrep -fa "python3.*app"
# 1500 python3 /srv/app/main.py --port 8080

# Si el resultado es correcto, entonces:
pkill -f "python3.*app"
```

**Regla**: siempre ejecutar `pgrep` primero para verificar qué procesos
coinciden. Luego usar `pkill` con el mismo patrón.

## pidof — Obtener PID por nombre exacto

```bash
# Devuelve PIDs de procesos con nombre exacto
pidof sshd
# 842 1501

# Solo uno (el más reciente)
pidof -s sshd
# 1501

# Útil en scripts
kill $(pidof -s nginx)
```

## Comparación

| Herramienta | Busca por | Coincidencia | Mejor para |
|---|---|---|---|
| `kill` | PID | Exacta | Matar un proceso específico conocido |
| `killall` | Nombre | Exacta (nombre corto) | Matar todas las instancias de un programa |
| `pkill` | Patrón | Regex, nombre o cmdline | Matar por patrón flexible |
| `pgrep` | Patrón | (igual que pkill) | Encontrar PIDs, verificar antes de pkill |
| `pidof` | Nombre | Exacta | Obtener PID para scripts |

## Patrones prácticos

### Parar un servicio limpiamente

```bash
# Opción 1: systemctl (preferida para servicios)
sudo systemctl stop nginx

# Opción 2: señal manual
kill $(pidof nginx)          # SIGTERM al master
sleep 2
pidof nginx && kill -9 $(pidof nginx)   # SIGKILL si no terminó
```

### Matar un proceso colgado en la terminal

```bash
# 1. Ctrl+C (SIGINT)
# 2. Si no responde: Ctrl+\ (SIGQUIT + core dump)
# 3. Si no responde: desde otra terminal
kill <PID>
# 4. Si no responde:
kill -9 <PID>
# 5. Si ni SIGKILL funciona: el proceso está en estado D
#    (I/O del kernel colgado — no se puede matar, hay que
#    resolver el I/O colgado o reiniciar)
```

### Matar todos los procesos de un usuario

```bash
# Todos los procesos de un usuario (cuidado)
pkill -u usuario

# Con confirmación (más seguro)
pgrep -u usuario -la    # ver primero
pkill -u usuario         # luego matar

# loginctl (systemd) — también mata la sesión
sudo loginctl terminate-user usuario
```

### Log rotation con señales

```bash
# Muchos daemons reabren sus logs al recibir SIGUSR1 o SIGHUP
# El flujo típico de logrotate:

# 1. logrotate renombra el log: access.log → access.log.1
# 2. logrotate envía señal al daemon: kill -USR1 $(pidof nginx)
# 3. nginx cierra el fd viejo y abre uno nuevo (access.log)
# 4. logrotate comprime access.log.1

# Sin la señal, el daemon seguiría escribiendo en access.log.1
# porque tiene el fd abierto (el renombrado no afecta al fd)
```

## Tabla: señales más usadas con kill/pkill/killall

| Señal | Número | Cuándo usarla |
|---|---|---|
| SIGTERM | 15 | Terminación limpia (default) |
| SIGKILL | 9 | Cuando SIGTERM no funciona |
| SIGSTOP | 19 | Congelar sin matar |
| SIGCONT | 18 | Reanudar proceso detenido |
| SIGHUP | 1 | Recargar configuración |
| SIGUSR1 | 10 | Varía según el programa |
| SIGINT | 2 | Interrumpir (equivale a Ctrl+C) |

## Ejercicios

### Ejercicio 1 — kill, killall, pkill

```bash
# Crear procesos de prueba
sleep 300 &
sleep 301 &
sleep 302 &

# Ver los PIDs
jobs -l

# Matar uno por PID
kill $(jobs -p | head -1)

# Matar los restantes por patrón
pgrep -la 'sleep 30[12]'   # verificar primero
pkill 'sleep'

# ¿Quedan?
jobs
```

### Ejercicio 2 — pgrep antes de pkill

```bash
# Lanzar procesos con nombres similares
bash -c 'exec -a worker-web sleep 200' &
bash -c 'exec -a worker-api sleep 200' &
bash -c 'exec -a worker-batch sleep 200' &

# ¿Qué mataría "pkill worker"?
pgrep -la worker

# Matar solo worker-api
pkill -x worker-api

# Verificar
pgrep -la worker

# Limpiar
pkill worker
```

### Ejercicio 3 — Process groups

```bash
# Crear un pipeline
cat /dev/zero | gzip > /dev/null &
PGID=$(ps -o pgid= -p $! | tr -d ' ')

# Ver el process group
ps -o pid,pgid,cmd -g $PGID

# Matar todo el group
kill -- -$PGID

# Verificar
ps -o pid,pgid,cmd -g $PGID 2>/dev/null
```

### Ejercicio 4 — killall con confirmación

```bash
#!/bin/bash
# Crear procesos de prueba
sleep 200 &
sleep 201 &
sleep 202 &

# Verificar qué matchearía killall -i
echo "Procesos que matchearía killall -i sleep:"
pgrep -la sleep

# killall -i pediría confirmación para cada uno
# killall -i -y sleep  # descomentar para probar
```

### Ejercicio 5 — pkill con múltiples filtros

```bash
#!/bin/bash
# Crear procesos de prueba
bash -c 'sleep 300 & exec sleep 301' &
sleep 302 &

echo "Todos los sleep:"
pgrep -la sleep

echo ""
echo "Solo de usuario actual:"
pgrep -la -u $(id -u) sleep

echo ""
echo "Los más nuevos (sleep con pid mayor):"
pgrep -n sleep

echo ""
echo "Los más viejos (sleep con pid menor):"
pgrep -o sleep
```

### Ejercicio 6 — orphan y SIGHUP

```bash
#!/bin/bash
# Demostrar que SIGHUP mata procesos huérfanos del terminal

# Iniciar proceso en background
sleep 100 &
SPID=$!

# Ver su grupo de proceso
echo "PGID del sleep: $(ps -o pgid= -p $SPID)"
echo "SID del sleep: $(ps -o sid= -p $SPID)"
echo "Sesión del terminal actual: $(ps -o sid= -p $$)"

# Matar el proceso padre (el subshell)
# El sleep queda huérfano, pero sigue corriendo
# porque ya no está en nuestra sesión

# Limpiar
kill $SPID
```

### Ejercicio 7 — pgrep -a para ver la línea completa

```bash
#!/bin/bash
# Mostrar cmdline completa vs nombre corto

pgrep sleep       # solo PIDs
pgrep -l sleep   # PIDs + nombre
pgrep -a sleep   # PIDs + cmdline completa

# Útil para identificar procesos similares
pkill -f "python3.*server"
pgrep -fa "python3"   # verificar primero
```

### Ejercicio 8 — Signals y process groups

```bash
#!/bin/bash
# Matar todos los hijos de un proceso

PARENT_PID=${1:-$$}

echo "Hijos de $PARENT_PID:"
pgrep -P $PARENT_PID

echo ""
echo "Matar todos:"
pkill -P $PARENT_PID

echo ""
echo "¿Quedan hijos?"
pgrep -P $PARENT_PID
```

### Ejercicio 9 — pidof vs pgrep

```bash
#!/bin/bash
# Comparar pidof vs pgrep

echo "=== pidof (nombre exacto) ==="
pidof bash | head -1  # solo uno (el primero o más reciente)

echo ""
echo "=== pgrep (regex parcial) ==="
pgrep bash | wc -l        # todos los bash

echo ""
echo "=== pgrep -x (exacto) ==="
pgrep -x bash | head -1   # uno (exacto)
```

### Ejercicio 10 — kill builtin vs /usr/bin/kill

```bash
#!/bin/bash
# El builtin del shell puede matar jobs (%1, %2)

sleep 300 &
sleep 301 &

echo "Jobs activos:"
jobs -l

echo ""
echo "Matar con builtin kill %1:"
kill %1 2>/dev/null && echo "Mató job 1"

jobs -l

echo ""
echo "Matar el resto con builtin:"
kill %2 2>/dev/null

jobs -l
```
