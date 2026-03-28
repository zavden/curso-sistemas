# T03 — Daemons

## Qué es un daemon

Un **daemon** (pronunciado "dimon") es un proceso que ejecuta en segundo plano,
sin terminal de control, proporcionando un servicio de forma continua. Los
nombres de daemons tradicionalmente terminan en `d`: `sshd`, `httpd`, `crond`,
`systemd`.

```
Características de un daemon:
  1. No tiene terminal de control (TTY = ?)
  2. Se desacopla del proceso que lo inició
  3. Normalmente ejecuta como un usuario específico del sistema
  4. Su directorio de trabajo es / (para no bloquear un mount point)
  5. Escribe a logs, no a stdout/stderr
  6. Se inicia automáticamente al arrancar el sistema
```

```bash
# Identificar daemons en ps
ps -eo pid,tty,user,cmd | grep '^\s*[0-9].*?'
# PID  TTY USER    CMD
# 842  ?   root    /usr/sbin/sshd -D
# 900  ?   www-data /usr/sbin/nginx: worker process
# 920  ?   postgres /usr/lib/postgresql/15/bin/postgres

# ? en TTY = sin terminal = daemon (o proceso en background de otro tipo)
```

## Método clásico: doble fork

Antes de systemd, un daemon tenía que **daemonizarse a sí mismo**. El
procedimiento estándar se llama **doble fork**:

```
Paso 1 — Primer fork
  padre (PID 100) → fork() → hijo (PID 101)
  padre sale (exit) → hijo queda huérfano → adoptado por init

Paso 2 — setsid()
  hijo (PID 101) → setsid()
  Crea una nueva sesión: PID 101 es session leader
  Se desconecta del terminal de control

Paso 3 — Segundo fork
  session leader (PID 101) → fork() → nieto (PID 102)
  session leader sale (exit) → nieto queda
  El nieto NO es session leader → no puede readquirir un terminal

Paso 4 — Configuración final
  nieto (PID 102):
    chdir("/")                    ← no bloquear mount points
    umask(0)                      ← no heredar umask del padre
    close(stdin/stdout/stderr)    ← desconectar del terminal
    open("/dev/null", ...)        ← redirigir a /dev/null
    write PID to /var/run/daemon.pid  ← para que otros lo encuentren

Resultado: PID 102 es un daemon limpio
```

### Por qué el doble fork

```
¿Por qué no un solo fork?

Después del primer fork + setsid(), el proceso es session leader.
Un session leader SIN terminal de control puede obtener uno nuevo
al abrir un dispositivo de terminal. El segundo fork crea un
proceso que NO es session leader, así que no puede adquirir un
terminal accidentalmente.

En la práctica, la mayoría de daemons no abren terminales, así
que el segundo fork es una precaución defensiva.
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

    // Ignorar SIGHUP (por precaución en el segundo fork)
    signal(SIGHUP, SIG_IGN);

    // Segundo fork
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);  // session leader sale

    // Configurar el daemon
    umask(0);
    chdir("/");

    // Cerrar file descriptors heredados
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirigir stdin/stdout/stderr a /dev/null
    open("/dev/null", O_RDONLY);  // fd 0 = stdin
    open("/dev/null", O_WRONLY);  // fd 1 = stdout
    open("/dev/null", O_WRONLY);  // fd 2 = stderr

    // Escribir PID file
    FILE *f = fopen("/var/run/mydaemon.pid", "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

int main() {
    daemonize();

    // Loop principal del daemon
    while (1) {
        // ... hacer trabajo ...
        sleep(60);
    }
    return 0;
}
```

### PID files

El PID file (`/var/run/daemon.pid`) permite que scripts y otros procesos
encuentren al daemon:

```bash
# Leer el PID del daemon
cat /var/run/sshd.pid
# 842

# Enviar señal usando el PID file
kill -HUP $(cat /var/run/sshd.pid)

# Problemas con PID files:
# - Stale PID file: el daemon murió pero el archivo quedó
# - PID reuse: otro proceso tomó el mismo PID
# - Race conditions: leer y enviar señal no es atómico
#
# systemd elimina estos problemas — sabe exactamente qué PID
# tiene cada servicio porque él los inició
```

## Método moderno: systemd

Con systemd, un daemon **no necesita daemonizarse**. systemd maneja todo:

```bash
# El daemon simplemente ejecuta en foreground
# systemd se encarga de:
# - Lanzarlo en background
# - Redirigir stdout/stderr al journal
# - Reiniciarlo si muere
# - Rastrear su PID y todos sus hijos (via cgroup)
# - Enviarle señales (SIGTERM para stop, SIGHUP para reload)
```

### Type=simple (default)

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=My Application
After=network.target

[Service]
Type=simple
User=myapp
ExecStart=/usr/bin/myapp --config /etc/myapp/config.yml
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```bash
# Type=simple: systemd asume que el proceso está listo inmediatamente
# al iniciar. El proceso NO debe hacer fork ni daemonizarse.
# systemd rastrea el PID del proceso directamente.
```

### Type=forking (legacy)

```ini
[Service]
Type=forking
PIDFile=/var/run/mydaemon.pid
ExecStart=/usr/sbin/mydaemon
```

```bash
# Type=forking: para daemons legacy que hacen doble fork
# systemd espera a que el proceso principal salga (el fork)
# y luego lee el PID file para saber cuál es el daemon real
#
# Problemas: race conditions con PID files, menos confiable
# Usar solo para daemons que no se pueden modificar
```

### Type=notify

```ini
[Service]
Type=notify
ExecStart=/usr/bin/myapp
```

```bash
# Type=notify: el daemon avisa a systemd cuando está listo
# usando sd_notify("READY=1")
#
# Ventaja: systemd sabe exactamente cuándo el servicio está
# listo para recibir conexiones (no adivina)
#
# Usado por: PostgreSQL, nginx (con módulo), systemd-journald
```

### Type=exec

```ini
[Service]
Type=exec
ExecStart=/usr/bin/myapp
```

```bash
# Type=exec: como simple, pero systemd espera a que exec() se complete
# exitosamente antes de considerar el servicio iniciado
# Detecta errores de binario no encontrado o permisos incorrectos
# Disponible desde systemd 240+
```

### Comparación de tipos

| Type | El daemon hace fork | systemd sabe cuándo está listo | Recomendado |
|---|---|---|---|
| simple | No | Asume inmediatamente | Sí (default) |
| exec | No | Tras exec() exitoso | Sí |
| notify | No | Cuando el daemon avisa | Sí (si el daemon lo soporta) |
| forking | Sí (doble fork) | Cuando el padre sale | Solo para legacy |
| oneshot | — | Cuando el proceso termina | Para scripts de setup |

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

  No necesita: fork, setsid, PID files, manejo de logs, chdir.
```

```bash
# Ejemplo: un daemon moderno en Python
#!/usr/bin/env python3
import signal
import sys
import time

def handle_sigterm(signum, frame):
    print("Shutting down gracefully...")
    sys.exit(0)

signal.signal(signal.SIGTERM, handle_sigterm)

print("Service started")  # systemd captura esto al journal
while True:
    # ... hacer trabajo ...
    time.sleep(1)

# Eso es todo. Sin fork, sin PID file, sin manejo de logs.
# systemd se encarga del resto.
```

## inetd / xinetd (histórico)

Antes de systemd socket activation, existía `inetd` (y su sucesor `xinetd`):

```bash
# inetd: un super-daemon que escuchaba en múltiples puertos
# y lanzaba el servicio correspondiente al recibir una conexión
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

# Sin terminal de control
ps -o tty= -p $PID
# ?

# Session leader (SID == PID)
ps -o pid,sid -p $PID
# PID   SID
# 842   842

# Su padre es PID 1 (o el subreaper)
ps -o ppid= -p $PID
# 1

# No tiene stdin/stdout/stderr conectados a un terminal
ls -la /proc/$PID/fd/0 /proc/$PID/fd/1 /proc/$PID/fd/2 2>/dev/null
# 0 -> /dev/null (o socket, o archivo de log)

# Directorio de trabajo es /
ls -la /proc/$PID/cwd
# /proc/842/cwd -> /
```

---

## Ejercicios

### Ejercicio 1 — Identificar daemons

```bash
# Listar procesos sin terminal (candidatos a daemon)
ps -eo pid,ppid,tty,user,cmd | awk '$3 == "?"' | head -15

# ¿Cuántos procesos sin terminal hay?
ps -eo tty | grep -c '?'

# ¿Cuáles son session leaders? (SID == PID)
ps -eo pid,sid,tty,cmd | awk '$1 == $2 && $3 == "?"' | head -10
```

### Ejercicio 2 — Inspeccionar un daemon

```bash
# Elegir un daemon (sshd, cron, o cualquiera disponible)
DAEMON=cron
PID=$(pgrep -x $DAEMON | head -1)

if [ -n "$PID" ]; then
    echo "PID: $PID"
    echo "PPID: $(ps -o ppid= -p $PID)"
    echo "TTY: $(ps -o tty= -p $PID)"
    echo "CWD: $(readlink /proc/$PID/cwd)"
    echo "Stdin: $(readlink /proc/$PID/fd/0 2>/dev/null)"
    echo "Type: $(systemctl show -p Type $DAEMON 2>/dev/null | cut -d= -f2)"
else
    echo "$DAEMON no está corriendo"
fi
```

### Ejercicio 3 — Ver el unit file de un daemon

```bash
# Ver cómo systemd gestiona un daemon
systemctl cat sshd 2>/dev/null || systemctl cat ssh 2>/dev/null

# ¿Qué Type usa?
# ¿Tiene PIDFile?
# ¿Qué señal usa para reload (ExecReload)?
# ¿Se reinicia automáticamente (Restart)?
```
