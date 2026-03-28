# T03 — Daemons

## Errata y aclaraciones sobre el material original

### Error: grep para identificar daemons

README.md línea 22:

```bash
ps -eo pid,tty,user,cmd | grep '^\s*[0-9].*?'
```

Este patrón coincide con cualquier línea que contenga `?` en cualquier
posición (incluyendo el nombre del comando). El método correcto es filtrar
por el campo TTY:

```bash
ps -eo pid,tty,user,cmd | awk '$2 == "?"'
```

### Error: "procesos con & también tienen TTY=?"

Labs Paso 1.4 línea 82:

> "No todos los procesos sin TTY son daemons (procesos en background con &
> tambien tienen TTY=?)"

Esto es **incorrecto**. Un proceso lanzado con `&` desde un shell interactivo
**SÍ tiene TTY** — hereda la terminal de control del shell:

```bash
sleep 300 &
ps -o pid,tty -p $!
# PID  TTY
# 1234 pts/0    ← TIENE terminal

setsid sleep 301 </dev/null &>/dev/null &
sleep 0.3
ps -o pid,tty -p $(pgrep -nf "sleep 301")
# PID  TTY
# 1235 ?        ← SIN terminal (daemon-like)
```

Un proceso background con `&` sigue teniendo TTY. Para perder la TTY se
necesita `setsid`, estar en un namespace sin terminal, o haber pasado por
daemonización.

### Error: ejercicios 2 y 3 usan systemctl

Los ejercicios 2 y 3 usan `systemctl show` y `systemctl cat`, que no
funcionan en contenedores sin systemd. Reemplazados con alternativas vía
`/proc`.

### Aclaración: SID == PID no siempre aplica a daemons modernos

El material dice que un daemon típico tiene SID == PID (session leader). Esto
es cierto para **daemons clásicos** que hicieron `setsid()`. Con systemd,
los servicios pueden tener un SID diferente porque systemd gestiona sus
sesiones vía cgroups, no vía session IDs.

---

## Qué es un daemon

Un **daemon** (pronunciado "dimon") es un proceso que ejecuta en segundo plano,
sin terminal de control, proporcionando un servicio de forma continua. Los
nombres de daemons tradicionalmente terminan en `d`: `sshd`, `httpd`, `crond`,
`systemd`.

```
Características de un daemon:
  1. No tiene terminal de control (TTY = ?)
  2. Se desacopla del proceso que lo inició (PPID = 1 o subreaper)
  3. Normalmente ejecuta como un usuario específico del sistema
  4. Su directorio de trabajo es / (para no bloquear mount points)
  5. stdin/stdout/stderr redirigidos a /dev/null o logs
  6. Se inicia automáticamente al arrancar el sistema
```

```bash
# Identificar daemons con awk (no grep)
ps -eo pid,tty,user,cmd | awk '$2 == "?"' | head -10
# PID  TTY USER     CMD
# 842  ?   root     /usr/sbin/sshd -D
# 900  ?   www-data /usr/sbin/nginx: worker process
# 920  ?   postgres /usr/lib/postgresql/15/bin/postgres
```

### Daemon vs proceso background

```
Proceso con &          Daemon
────────────────       ────────────────
TTY = pts/0            TTY = ?
PPID = shell           PPID = 1
Misma sesión (SID)     Sesión propia (SID = PID)
CWD = donde se lanzó   CWD = /
fds 0,1,2 al terminal  fds 0,1,2 a /dev/null o logs
Muere con el terminal   Sobrevive al terminal
```

`nohup` y `disown` hacen que un proceso **sobreviva** al terminal, pero no lo
convierten en un daemon completo (sigue teniendo TTY, CWD heredado, etc.).
`setsid` se acerca más al crear una nueva sesión sin terminal.

## Método clásico: doble fork

Antes de systemd, un daemon tenía que **daemonizarse a sí mismo**:

```
Paso 1 — Primer fork
  padre (PID 100) → fork() → hijo (PID 101)
  padre sale (exit) → hijo queda huérfano → adoptado por init

Paso 2 — setsid()
  hijo (PID 101) → setsid()
  Crea nueva sesión: PID 101 es session leader
  Se desconecta del terminal de control

Paso 3 — Segundo fork
  session leader (PID 101) → fork() → nieto (PID 102)
  session leader sale (exit) → nieto queda
  El nieto NO es session leader → no puede readquirir terminal

Paso 4 — Configuración final
  nieto (PID 102):
    chdir("/")                    ← no bloquear mount points
    umask(0)                      ← no heredar umask del padre
    close(stdin/stdout/stderr)    ← desconectar del terminal
    open("/dev/null", ...)        ← redirigir a /dev/null
    write PID to /var/run/X.pid   ← para que otros lo encuentren

Resultado: PID 102 es un daemon limpio
```

### Por qué el doble fork

```
Después del primer fork + setsid(), el proceso es session leader.
Un session leader SIN terminal de control puede obtener uno nuevo
al abrir un dispositivo de terminal (open("/dev/ttyN")).

El segundo fork crea un proceso que NO es session leader, así que
no puede adquirir un terminal accidentalmente. Es una precaución
defensiva.
```

### Ejemplo en C (clásico)

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

void daemonize(void) {
    pid_t pid;

    // Primer fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  // padre sale

    // Crear nueva sesión
    if (setsid() < 0) exit(EXIT_FAILURE);

    // Ignorar SIGHUP (precaución en el segundo fork)
    signal(SIGHUP, SIG_IGN);

    // Segundo fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  // session leader sale

    // Configurar
    umask(0);
    chdir("/");

    // Cerrar fds heredados y redirigir a /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);  // fd 0 = stdin
    open("/dev/null", O_WRONLY);  // fd 1 = stdout
    open("/dev/null", O_WRONLY);  // fd 2 = stderr

    // PID file
    FILE *f = fopen("/var/run/mydaemon.pid", "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}
```

> glibc proporciona `daemon(3)` que hace fork+setsid+chdir+redirect en una
> sola llamada: `daemon(0, 0)`. Pero no hace el segundo fork, por lo que
> el proceso queda como session leader.

### PID files

```bash
# El daemon escribe su PID en /var/run/daemon.pid
cat /var/run/sshd.pid
# 842

# Enviar señal usando el PID file
kill -HUP $(cat /var/run/sshd.pid)

# Problemas:
# 1. Stale PID file: daemon murió pero el archivo quedó
#    → kill $(cat daemon.pid) mata OTRO proceso
# 2. PID reuse: el kernel reutiliza PIDs
# 3. Race conditions: leer PID y enviar señal no es atómico

# systemd elimina estos problemas — rastrea PIDs vía cgroups
```

## Método moderno: systemd

Con systemd, un daemon **no necesita daemonizarse**. systemd maneja todo:

```bash
# El daemon simplemente ejecuta en foreground
# systemd se encarga de:
# - Lanzarlo en background
# - Redirigir stdout/stderr al journal
# - Reiniciarlo si muere (Restart=)
# - Rastrear su PID y todos sus hijos (via cgroup)
# - Enviarle señales (SIGTERM para stop, SIGHUP para reload)
# - Limitar sus recursos (MemoryMax, CPUQuota)
```

### Unit file ejemplo

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My Application
After=network.target

[Service]
Type=simple
User=myapp
ExecStart=/usr/bin/myapp --config /etc/myapp/config.yml
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

### Tipos de servicio

| Type | Fork | Listo cuando | Uso |
|---|---|---|---|
| `simple` (default) | No | Inmediatamente al iniciar | Daemons modernos |
| `exec` | No | Tras `exec()` exitoso | Daemons modernos (systemd 240+) |
| `notify` | No | Cuando el daemon envía `sd_notify("READY=1")` | Apps avanzadas |
| `forking` | Sí | Cuando el padre sale + PIDFile | Daemons legacy |
| `oneshot` | — | Cuando el proceso termina | Scripts de setup |

```bash
# Type=simple: daemon ejecuta en foreground, no hace fork
# Type=exec: como simple, pero detecta errores de exec() (binario no encontrado)
# Type=notify: el daemon avisa cuando está listo (PostgreSQL, nginx)
# Type=forking: para daemons legacy que hacen doble fork + PID file
# Type=oneshot: para scripts que hacen algo y salen (setup, migración)
```

### Gestión de servicios con systemctl

```bash
systemctl start myapp       # iniciar servicio
systemctl stop myapp        # detener (envía SIGTERM, espera, SIGKILL)
systemctl restart myapp     # stop + start
systemctl reload myapp      # recargar config (envía SIGHUP si ExecReload)
systemctl enable myapp      # auto-start al arranque (crea symlink)
systemctl disable myapp     # no auto-start
systemctl status myapp      # ver estado, PID, últimas líneas de log
systemctl is-active myapp   # script-friendly: active/inactive
```

### Logs con journalctl

```bash
journalctl -u myapp           # todos los logs del servicio
journalctl -u myapp -f        # follow (como tail -f)
journalctl -u myapp --since today
journalctl -u myapp -p err    # solo errores y peor
journalctl -u myapp -n 50     # últimas 50 líneas
```

## Por qué los daemons modernos no hacen doble fork

```
Daemon clásico (SysVinit era):
  El daemon debe daemonizarse solo porque init no lo gestiona.
  Necesita: fork, setsid, fork, chdir, close fds, PID file, logs propios.

Daemon moderno (systemd era):
  systemd gestiona todo. El daemon solo tiene que:
  1. Ejecutar en foreground (no fork)
  2. Escribir a stdout/stderr (systemd los captura al journal)
  3. Opcionalmente: sd_notify("READY=1") cuando esté listo
  4. Manejar SIGTERM para shutdown limpio
```

```bash
#!/usr/bin/env python3
# Daemon moderno completo — solo esto:
import signal, sys, time

def handle_sigterm(signum, frame):
    print("Shutting down gracefully...")
    sys.exit(0)

signal.signal(signal.SIGTERM, handle_sigterm)

print("Service started")   # systemd captura esto al journal
while True:
    time.sleep(1)

# Sin fork, sin PID file, sin manejo de logs.
# systemd se encarga del resto.
```

## inetd / xinetd (histórico)

```bash
# inetd: super-daemon que escuchaba en múltiples puertos
# y lanzaba el servicio al recibir una conexión
#
# /etc/inetd.conf:
# ftp   stream tcp nowait root /usr/sbin/vsftpd vsftpd
# ssh   stream tcp nowait root /usr/sbin/sshd   sshd -i
#
# Ventaja: servicios poco usados no consumían recursos
# Desventaja: latencia en la primera conexión (fork+exec)
#
# systemd socket activation es el sucesor moderno de este concepto
```

## Verificar que un proceso es un daemon

```bash
PID=842

# 1. Sin terminal de control
ps -o tty= -p $PID
# ?

# 2. Session leader (SID == PID) — daemons clásicos
ps -o pid,sid -p $PID
# PID   SID
# 842   842

# 3. Padre es PID 1 (o subreaper)
ps -o ppid= -p $PID | tr -d ' '
# 1

# 4. CWD es / (no bloquear mount points)
readlink /proc/$PID/cwd
# /

# 5. fds no conectados a terminal
ls -la /proc/$PID/fd/{0,1,2} 2>/dev/null
# 0 -> /dev/null (o socket, o archivo de log)
```

---

## Labs

### Parte 1 — Identificar daemons

#### Paso 1.1: Procesos sin terminal (TTY=?)

```bash
echo "=== Procesos sin terminal ==="
ps -eo pid,ppid,tty,user,cmd | awk 'NR==1 || $3 == "?"' | head -15

echo ""
echo "=== Procesos CON terminal ==="
ps -eo pid,ppid,tty,user,cmd | awk 'NR==1 || $3 != "?"' | head -10
echo "(estos tienen terminal — no son daemons)"
```

#### Paso 1.2: Contar daemons vs interactivos

```bash
TOTAL=$(ps -e --no-headers | wc -l)
NO_TTY=$(ps -eo tty --no-headers | grep -c '^?$')
WITH_TTY=$((TOTAL - NO_TTY))

echo "Total:         $TOTAL"
echo "Sin terminal:  $NO_TTY"
echo "Con terminal:  $WITH_TTY"
```

#### Paso 1.3: Session leaders

```bash
echo "=== Session leaders sin terminal (SID == PID, TTY = ?) ==="
ps -eo pid,sid,tty,cmd | awk 'NR==1 || ($1 == $2 && $3 == "?")' | head -10

echo ""
echo "Un daemon clásico es session leader (hizo setsid)"
echo "Un daemon moderno bajo systemd puede no serlo"
```

#### Paso 1.4: Background ≠ daemon

```bash
echo "=== Demostrar que & NO crea un daemon ==="

sleep 300 &
PID_BG=$!
echo "Background con &:"
echo "  PID: $PID_BG"
echo "  TTY: $(ps -o tty= -p $PID_BG)"
echo "  (TIENE terminal — no es daemon)"

setsid sleep 301 </dev/null &>/dev/null &
sleep 0.3
PID_DAEMON=$(pgrep -nf "sleep 301")
echo ""
echo "Con setsid:"
echo "  PID: $PID_DAEMON"
echo "  TTY: $(ps -o tty= -p $PID_DAEMON)"
echo "  (SIN terminal — daemon-like)"

kill $PID_BG $PID_DAEMON 2>/dev/null
wait 2>/dev/null
```

### Parte 2 — Inspeccionar un daemon

#### Paso 2.1: Inspeccionar vía /proc

```bash
# Usar PID 1 como ejemplo (siempre disponible)
PID=1

echo "=== Ejecutable ==="
readlink /proc/$PID/exe 2>/dev/null || echo "(no disponible)"

echo ""
echo "=== CWD ==="
readlink /proc/$PID/cwd 2>/dev/null || echo "(no disponible)"

echo ""
echo "=== File descriptors ==="
ls -la /proc/$PID/fd/ 2>/dev/null | head -5

echo ""
echo "=== TTY ==="
echo "$(ps -o tty= -p $PID)"

echo ""
echo "=== Session ID ==="
ps -o pid,sid -p $PID
```

#### Paso 2.2: Crear un daemon simulado con setsid

```bash
setsid bash -c 'cd /; exec sleep 300' </dev/null &>/dev/null &
sleep 0.5
DPID=$(pgrep -nf "sleep 300")

echo "PID:      $DPID"
echo "PPID:     $(ps -o ppid= -p $DPID | tr -d ' ')"
echo "TTY:      $(ps -o tty= -p $DPID)"
echo "SID:      $(ps -o sid= -p $DPID | tr -d ' ')"
echo "SID==PID: $([ "$(ps -o sid= -p $DPID | tr -d ' ')" = "$DPID" ] && echo 'sí' || echo 'no')"
echo "CWD:      $(readlink /proc/$DPID/cwd 2>/dev/null)"

kill $DPID 2>/dev/null
```

### Parte 3 — Doble fork vs systemd

#### Paso 3.1: Doble fork (clásico)

```bash
echo "=== Doble fork: pasos ==="
echo "1. fork() → padre sale → hijo huérfano → PID 1 adopta"
echo "2. setsid() → nueva sesión, sin terminal"
echo "3. fork() → session leader sale → nieto queda"
echo "   (nieto no es session leader → no puede adquirir terminal)"
echo "4. chdir('/'), umask(0), close fds, redirect /dev/null"
echo "5. Escribir PID file"
```

#### Paso 3.2: systemd (moderno)

```bash
echo "=== systemd: el daemon ejecuta en foreground ==="
echo ""
echo "NO necesita: fork, setsid, chdir, close fds, PID file, logs"
echo ""
echo "systemd se encarga de TODO:"
echo "  - Lanzar en background"
echo "  - Redirigir stdout/stderr al journal"
echo "  - Rastrear PIDs (via cgroup)"
echo "  - Reiniciar si muere"
echo "  - Enviar señales"
echo "  - Limitar recursos"
echo ""
echo "=== Unit file mínimo ==="
echo "[Unit]"
echo "Description=My App"
echo ""
echo "[Service]"
echo "Type=simple"
echo "ExecStart=/usr/bin/myapp"
echo "Restart=on-failure"
echo ""
echo "[Install]"
echo "WantedBy=multi-user.target"
```

#### Paso 3.3: PID files — problemas

```bash
echo "=== PID files: por qué son problemáticos ==="
echo ""
echo "1. Stale PID file: daemon murió pero el archivo quedó"
echo "   kill \$(cat daemon.pid) → mata OTRO proceso"
echo ""
echo "2. PID reuse: el kernel reutiliza PIDs"
echo "   El PID del archivo puede ser de otro proceso"
echo ""
echo "3. Race condition: leer + enviar señal no es atómico"
echo ""
echo "systemd usa cgroups → sabe exactamente qué PIDs"
echo "pertenecen a cada servicio, sin archivos intermedios"
```

---

## Ejercicios

### Ejercicio 1 — Identificar daemons en el sistema

```bash
# a) Listar procesos sin terminal
ps -eo pid,ppid,tty,user,cmd | awk '$3 == "?"' | head -15

# b) ¿Cuántos hay?
echo "Sin terminal: $(ps -eo tty --no-headers | grep -c '^?$')"
echo "Con terminal: $(ps -eo tty --no-headers | grep -cv '^?$')"

# c) ¿Cuáles son session leaders? (SID == PID)
echo ""
echo "Session leaders sin TTY:"
ps -eo pid,sid,tty,cmd --no-headers | awk '$1 == $2 && $3 == "?"' | head -5
```

<details><summary>Predicción</summary>

a) Se ven procesos como PID 1 (init/bash), y posiblemente otros servicios del
contenedor/sistema. Todos tienen `?` en la columna TTY.

b) En un contenedor mínimo: la mayoría sin terminal. En un sistema completo:
también mayoría sin terminal (hay muchos daemons y kernel threads).

c) Los session leaders tienen SID == PID. Estos son procesos que crearon su
propia sesión con `setsid()` (daemons clásicos) o que fueron iniciados como
líderes de sesión por init.

No todo proceso con TTY=? es un daemon. Los kernel threads (entre corchetes)
también tienen TTY=?, pero no son daemons — son hilos del kernel.

</details>

### Ejercicio 2 — Background vs daemon: la diferencia

```bash
# a) Proceso con & (background simple)
sleep 400 &
PID_BG=$!
echo "Background:"
echo "  TTY:  $(ps -o tty= -p $PID_BG)"
echo "  PPID: $(ps -o ppid= -p $PID_BG | tr -d ' ')"
echo "  SID:  $(ps -o sid= -p $PID_BG | tr -d ' ')"

# b) Proceso con setsid (daemon-like)
setsid sleep 401 </dev/null &>/dev/null &
sleep 0.3
PID_SETSID=$(pgrep -nf "sleep 401")
echo ""
echo "Setsid:"
echo "  TTY:  $(ps -o tty= -p $PID_SETSID)"
echo "  PPID: $(ps -o ppid= -p $PID_SETSID | tr -d ' ')"
echo "  SID:  $(ps -o sid= -p $PID_SETSID | tr -d ' ')"

# c) Comparar SID con el del shell
echo ""
echo "Shell SID: $(ps -o sid= -p $$ | tr -d ' ')"

# Limpieza
kill $PID_BG $PID_SETSID 2>/dev/null
wait 2>/dev/null
```

<details><summary>Predicción</summary>

a) Background:
- TTY: `pts/0` (o similar) — **tiene terminal**
- PPID: PID del shell
- SID: mismo SID que el shell (misma sesión)

b) Setsid:
- TTY: `?` — **sin terminal**
- PPID: 1 (adoptado por PID 1, porque setsid forks en shell interactivo)
- SID: igual a su propio PID (es session leader)

c) El shell y el proceso background comparten SID. El proceso con setsid
tiene SID diferente.

Un `&` solo pone el proceso en background — sigue vinculado al terminal. Para
desvincularse completamente, necesita `setsid`.

</details>

### Ejercicio 3 — Crear un daemon simulado

```bash
# Crear un proceso con TODAS las características de daemon
setsid bash -c '
    cd /
    umask 0
    exec sleep 500
' </dev/null > /dev/null 2>&1 &
sleep 0.5
DPID=$(pgrep -nf "sleep 500")

# Verificar las 5 características
echo "PID: $DPID"
echo "1. TTY:      $(ps -o tty= -p $DPID) (¿es ?)"
echo "2. PPID:     $(ps -o ppid= -p $DPID | tr -d ' ') (¿es 1?)"
echo "3. SID:      $(ps -o sid= -p $DPID | tr -d ' ') = PID (¿session leader?)"
echo "4. CWD:      $(readlink /proc/$DPID/cwd 2>/dev/null) (¿es /?)"
echo "5. fd 0:     $(readlink /proc/$DPID/fd/0 2>/dev/null) (¿/dev/null?)"

# Limpieza
kill $DPID 2>/dev/null
```

<details><summary>Predicción</summary>

1. TTY = `?` — `setsid` creó nueva sesión sin terminal
2. PPID = 1 — `setsid` forks en shell interactivo; el padre (setsid) sale y
   el hijo es adoptado por PID 1
3. SID = PID del sleep — es session leader (creó la sesión con setsid)
4. CWD = `/` — `cd /` evita bloquear mount points
5. fd 0 = `/dev/null` — la redirección `</dev/null` desconectó stdin del
   terminal

Este proceso cumple las 5 características de un daemon. Es el equivalente
manual (en shell) del doble fork en C. La diferencia es que no hizo el
segundo fork, así que sigue siendo session leader (podría adquirir un
terminal si abriera `/dev/tty*`).

</details>

### Ejercicio 4 — Inspeccionar /proc de un daemon

```bash
# Crear un daemon que escribe a un log
setsid bash -c '
    cd /
    exec bash -c "while true; do echo tick >> /tmp/daemon-log.txt; sleep 2; done"
' </dev/null > /dev/null 2>&1 &
sleep 0.5
DPID=$(pgrep -nf "daemon-log")

echo "PID: $DPID"

# a) Estado del proceso
grep -E '^(Name|State|Pid|PPid|Uid)' /proc/$DPID/status 2>/dev/null

# b) File descriptors
echo ""
echo "=== fds ==="
ls -la /proc/$DPID/fd/ 2>/dev/null

# c) CWD
echo ""
echo "CWD: $(readlink /proc/$DPID/cwd 2>/dev/null)"

# d) environ (variables de entorno heredadas)
echo ""
echo "Variables del daemon:"
tr '\0' '\n' < /proc/$DPID/environ 2>/dev/null | head -5

# e) ¿Escribe al log?
sleep 3
echo ""
echo "Últimas líneas del log:"
tail -3 /tmp/daemon-log.txt

# Limpieza
kill $DPID 2>/dev/null
rm -f /tmp/daemon-log.txt
```

<details><summary>Predicción</summary>

a) State: `S` (sleeping entre sleeps). PPID: 1. Uid: el usuario actual.

b) Los fds muestran: fd 0 → /dev/null, fd 1 → /tmp/daemon-log.txt (el `>>`
lo abrió), y posiblemente otros fds internos de bash.

c) CWD: `/` (hicimos `cd /` al inicio).

d) Las variables de entorno fueron **heredadas** del shell que lo lanzó. Un
daemon real debería tener un entorno limpio. systemd controla exactamente qué
variables recibe cada servicio.

e) El log muestra "tick" cada 2 segundos — el daemon funciona.

</details>

### Ejercicio 5 — Doble fork completo en shell

```bash
# Implementar el doble fork clásico en shell

# Primer fork: subshell que sale inmediatamente
bash -c '
    # Segundo nivel: setsid + fork
    setsid bash -c "
        cd /
        umask 0
        exec sleep 600
    " </dev/null > /dev/null 2>&1 &
    DAEMON=$!
    echo "DAEMON_PID=\$DAEMON"
    exit 0  # el "primer fork" sale
' &
FIRST_FORK=$!
wait $FIRST_FORK  # recoger primer fork (no zombie)
sleep 0.5

# Encontrar el daemon
DPID=$(pgrep -nf "sleep 600")
echo "PID del daemon: $DPID"

# Verificar
echo "PPID:     $(ps -o ppid= -p $DPID | tr -d ' ')"
echo "SID:      $(ps -o sid= -p $DPID | tr -d ' ')"
echo "TTY:      $(ps -o tty= -p $DPID)"
echo "CWD:      $(readlink /proc/$DPID/cwd)"

# ¿Hay zombies?
echo "Zombies:  $(ps -eo stat --no-headers | grep -c '^Z')"

# Limpieza
kill $DPID 2>/dev/null
```

<details><summary>Predicción</summary>

- PPID: 1 (adoptado por PID 1 tras doble fork)
- SID: igual al PID del daemon (session leader por setsid)
- TTY: `?` (sin terminal)
- CWD: `/`
- Zombies: 0 (hicimos `wait $FIRST_FORK` para recoger al hijo intermedio)

Esta es la técnica clásica de daemonización traducida a shell:
1. `bash -c '...' &` = primer fork
2. `wait $FIRST_FORK` = padre recoge al hijo (sin zombie)
3. Dentro: `setsid` = nueva sesión
4. `cd /`, `umask 0`, redirects = configuración de daemon
5. `exec sleep` = el daemon real

Con systemd, nada de esto es necesario — el daemon ejecuta en foreground y
systemd gestiona todo lo demás.

</details>

### Ejercicio 6 — PID file: crear y detectar stale

```bash
# a) Crear un "daemon" con PID file
bash -c '
    echo $$ > /tmp/mydaemon.pid
    echo "Daemon PID: $$"
    sleep 10
' &
DAEMON=$!
sleep 1

# b) Verificar PID file
echo "PID file: $(cat /tmp/mydaemon.pid)"
echo "Proceso vivo: $(ps -o cmd= -p $(cat /tmp/mydaemon.pid) 2>/dev/null)"

# c) Esperar a que el daemon muera
wait $DAEMON 2>/dev/null
echo ""
echo "=== Daemon terminó ==="

# d) PID file stale
echo "PID file todavía dice: $(cat /tmp/mydaemon.pid)"
echo "¿Proceso vivo?"
ps -o cmd= -p $(cat /tmp/mydaemon.pid) 2>/dev/null || echo "  No — PID file STALE"

# e) El peligro: otro proceso podría tener ese PID
echo ""
echo "Si otro proceso obtiene PID $(cat /tmp/mydaemon.pid),"
echo "kill \$(cat /tmp/mydaemon.pid) mataría al proceso equivocado"

rm -f /tmp/mydaemon.pid
```

<details><summary>Predicción</summary>

b) El PID file contiene el PID correcto y el proceso está vivo.

d) Después de 10 segundos, el daemon terminó pero el PID file **sigue ahí**
con el PID antiguo. Esto es un "stale PID file".

e) El kernel reutiliza PIDs. Si un nuevo proceso obtiene el mismo PID, un
`kill $(cat daemon.pid)` mataría al proceso equivocado. Este es el problema
fundamental de los PID files.

systemd resuelve esto con cgroups: rastrea todos los PIDs de un servicio
directamente, sin archivos intermedios. No hay race conditions ni stale PIDs.

</details>

### Ejercicio 7 — Daemon moderno: foreground + SIGTERM

```bash
# Simular un daemon moderno (como lo haría systemd)
bash -c '
    trap "echo Shutdown limpio; exit 0" TERM
    echo "Service started (PID $$)"
    i=0
    while true; do
        echo "Working... tick $((i++))"
        sleep 1
    done
' &
DAEMON=$!
sleep 3

# a) Ver output (systemd lo capturaría al journal)
echo "=== Daemon corriendo ==="

# b) Enviar SIGTERM (como systemctl stop)
echo ""
echo "=== Enviando SIGTERM (systemctl stop) ==="
kill -TERM $DAEMON
wait $DAEMON 2>/dev/null
echo "Exit status: $?"
```

<details><summary>Predicción</summary>

Se ve "Service started" seguido de ticks "Working... tick 0/1/2".

Al enviar SIGTERM: se ve "Shutdown limpio" y el proceso sale con exit 0.

Esto es todo lo que un daemon moderno necesita:
1. Ejecutar en foreground (sin fork)
2. Escribir a stdout (systemd captura al journal)
3. Manejar SIGTERM para shutdown limpio
4. systemd se encarga del resto

Comparado con el doble fork clásico (ejercicio 5), esto es dramáticamente más
simple. La complejidad se movió del daemon a systemd.

</details>

### Ejercicio 8 — Comparar &, nohup, setsid, doble fork

```bash
# a) Solo &
sleep 700 &
PID_BG=$!

# b) nohup
nohup sleep 701 > /dev/null 2>&1 &
PID_NOHUP=$!

# c) setsid
setsid sleep 702 </dev/null &>/dev/null &
sleep 0.3
PID_SETSID=$(pgrep -nf "sleep 702")

# d) doble fork
bash -c 'setsid sleep 703 </dev/null &>/dev/null & exit' &
wait $!
sleep 0.3
PID_DOUBLE=$(pgrep -nf "sleep 703")

# Comparar
echo "Método       PID   TTY      PPID  SID"
echo "------------ ----- -------- ----- -----"
for LABEL_PID in "& $PID_BG" "nohup $PID_NOHUP" "setsid $PID_SETSID" "doble_fork $PID_DOUBLE"; do
    LABEL=$(echo $LABEL_PID | awk '{print $1}')
    P=$(echo $LABEL_PID | awk '{print $2}')
    if [ -n "$P" ] && ps -p $P > /dev/null 2>&1; then
        printf "%-12s %-5s %-8s %-5s %-5s\n" "$LABEL" "$P" \
            "$(ps -o tty= -p $P)" \
            "$(ps -o ppid= -p $P | tr -d ' ')" \
            "$(ps -o sid= -p $P | tr -d ' ')"
    fi
done

# Limpieza
kill $PID_BG $PID_NOHUP $PID_SETSID $PID_DOUBLE 2>/dev/null
wait 2>/dev/null
rm -f nohup.out
```

<details><summary>Predicción</summary>

| Método | TTY | PPID | SID |
|---|---|---|---|
| `&` | pts/0 | shell | = shell |
| `nohup` | pts/0 | shell | = shell |
| `setsid` | ? | 1 | = propio PID |
| `doble_fork` | ? | 1 | = propio PID |

- `&`: background simple, sigue vinculado al terminal y sesión
- `nohup`: protege contra SIGHUP (SIG_IGN), pero sigue con TTY y misma sesión
- `setsid`: nueva sesión, sin terminal, adoptado por PID 1 → daemon-like
- `doble_fork`: mismo que setsid, pero sin ser session leader (no puede
  adquirir terminal)

Solo setsid y doble fork producen procesos con características de daemon
(TTY=?, SID propio, PPID=1). nohup protege contra logout pero no crea un
daemon real.

</details>

### Ejercicio 9 — Daemon que escribe a log file

```bash
# Simular un daemon con logging propio (pre-systemd)
LOG=/tmp/mydaemon.log

setsid bash -c "
    cd /
    echo \"\$(date): started (PID \$\$)\" >> $LOG
    trap 'echo \"\$(date): received SIGTERM\" >> $LOG; exit 0' TERM
    while true; do
        echo \"\$(date): alive\" >> $LOG
        sleep 2
    done
" </dev/null > /dev/null 2>&1 &
sleep 0.5
DPID=$(pgrep -nf "mydaemon.log")

echo "Daemon PID: $DPID"

# a) Verificar que escribe al log
sleep 5
echo "=== Últimas líneas del log ==="
tail -5 $LOG

# b) Enviar SIGTERM (stop)
kill -TERM $DPID 2>/dev/null
sleep 0.5
echo ""
echo "=== Log después de SIGTERM ==="
tail -2 $LOG

# c) ¿Quedó zombie?
echo ""
echo "Zombies: $(ps -eo stat --no-headers | grep -c '^Z')"

rm -f $LOG
```

<details><summary>Predicción</summary>

a) El log muestra "started" y luego "alive" cada 2 segundos.

b) Después de SIGTERM, la última línea del log dice "received SIGTERM".
El daemon hizo shutdown limpio gracias al `trap`.

c) 0 zombies. El daemon fue adoptado por PID 1 (vía setsid), y PID 1 hizo
`wait()` cuando terminó.

Este patrón (logging propio + trap + setsid) era necesario antes de systemd.
Con systemd, el daemon simplemente escribe a stdout y systemd lo captura al
journal — mucho más simple.

</details>

### Ejercicio 10 — Analizar un unit file

```bash
# Aunque no tengamos systemd, podemos analizar un unit file

cat << 'EOF'
# Analizar este unit file:

[Unit]
Description=PostgreSQL database server
After=network.target

[Service]
Type=notify
User=postgres
ExecStart=/usr/lib/postgresql/15/bin/postgres -D /var/lib/postgresql/15/main
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
EOF

echo ""
echo "=== Preguntas ==="
echo "1. ¿Type=notify — qué significa?"
echo "2. ¿Postgres hace doble fork?"
echo "3. ¿Qué pasa con systemctl reload?"
echo "4. ¿Qué pasa si Postgres crashea?"
echo "5. ¿Cuánto espera systemd antes de SIGKILL al hacer stop?"
```

<details><summary>Predicción</summary>

1. `Type=notify`: PostgreSQL usa `sd_notify("READY=1")` para avisar a systemd
   cuando está listo para aceptar conexiones. systemd no considera el servicio
   "activo" hasta recibir esta notificación. Esto evita que servicios
   dependientes arranquen antes de que Postgres esté listo.

2. **No**. Con `Type=notify`, el proceso ejecuta en foreground (no fork).
   systemd espera la notificación, no un fork. Si Postgres hiciera doble fork,
   el Type sería `forking` con `PIDFile=`.

3. `ExecReload=/bin/kill -HUP $MAINPID`: `systemctl reload postgresql` envía
   SIGHUP al proceso principal. PostgreSQL recarga su configuración
   (pg_hba.conf, postgresql.conf) sin reiniciar.

4. `Restart=on-failure`: si Postgres crashea (exit code != 0 o señal), systemd
   lo reinicia automáticamente después de `RestartSec=5` (5 segundos).
   Si Postgres hace `exit 0`, no se reinicia.

5. `TimeoutStopSec=60`: systemd envía SIGTERM, espera **60 segundos** (no los
   90s por defecto), y luego envía SIGKILL. Esto da tiempo a PostgreSQL para
   hacer checkpoint y cerrar conexiones.

</details>
