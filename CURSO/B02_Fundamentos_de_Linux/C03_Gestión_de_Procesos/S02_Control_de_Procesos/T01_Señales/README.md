# T01 — Señales

## Qué son las señales

Las señales son **notificaciones asíncronas** que el kernel envía a un proceso
para informarle de un evento. Son el mecanismo más básico de comunicación entre
procesos y entre el kernel y los procesos.

```
Fuentes de señales:
  kernel → proceso    (SIGSEGV por acceso inválido a memoria)
  usuario → proceso   (Ctrl+C envía SIGINT)
  proceso → proceso   (kill() syscall)
  proceso → sí mismo  (raise(), abort())
```

Cuando un proceso recibe una señal, puede pasar una de tres cosas:

1. **Acción por defecto** — el kernel aplica la acción predeterminada
2. **Handler personalizado** — el proceso ejecuta su propio código
3. **Ignorar** — el proceso la descarta

Excepto dos señales que **no pueden** ser capturadas ni ignoradas: `SIGKILL` y
`SIGSTOP`. El kernel las ejecuta directamente.

## Señales esenciales

### SIGTERM (15) — Terminación limpia

```bash
# "Por favor, termina ordenadamente"
# La señal por defecto de kill
kill 1234      # envía SIGTERM

# El proceso PUEDE:
# - Capturarla y hacer cleanup (cerrar archivos, guardar estado, liberar recursos)
# - Ignorarla (proceso mal escrito o colgado)
# - No recibirla si está en estado D (uninterruptible sleep)
```

SIGTERM es la forma **correcta** de pedir a un proceso que termine. Le da la
oportunidad de cerrar conexiones, guardar datos y limpiar archivos temporales.

### SIGKILL (9) — Terminación forzada

```bash
# "Muere ahora, sin negociación"
kill -9 1234
kill -KILL 1234

# El proceso NO PUEDE:
# - Capturarla
# - Ignorarla
# - Hacer cleanup

# El kernel destruye el proceso inmediatamente
```

SIGKILL es el **último recurso**. El proceso no tiene oportunidad de limpiar:
archivos temporales quedan en disco, conexiones de red no se cierran
limpiamente, buffers no se escriben, locks no se liberan. Usarla sin intentar
SIGTERM primero es un error frecuente.

### SIGINT (2) — Interrupción del teclado

```bash
# Ctrl+C en el terminal envía SIGINT al foreground process group

# La mayoría de programas la capturan para terminar limpiamente
# Algunos la capturan para pedir confirmación:
# "¿Seguro que quieres salir? (y/n)"

# Ctrl+C NO es SIGKILL — el proceso puede ignorarla
```

### SIGHUP (1) — Hangup

Tiene dos usos completamente diferentes según el contexto:

```
Uso 1 — Terminal cerrada:
  Cuando se cierra un terminal, el kernel envía SIGHUP a todos los
  procesos del session group. Acción por defecto: terminar.
  Esto mata procesos lanzados desde ese terminal.

Uso 2 — Recargar configuración:
  Por convención, muchos daemons capturan SIGHUP para releer su
  configuración sin reiniciar:

  kill -HUP $(pidof nginx)     # nginx recarga nginx.conf
  kill -HUP $(pidof sshd)      # sshd relee sshd_config
  systemctl reload nginx       # internamente envía SIGHUP
```

El doble significado es un artefacto histórico: los daemons se desacoplan del
terminal (no tienen tty), así que SIGHUP "sobra" y se reutiliza para recargar.

### SIGQUIT (3) — Quit con core dump

```bash
# Ctrl+\ en el terminal envía SIGQUIT
# Acción por defecto: terminar + generar core dump

# Útil para debugging: si un proceso no responde a Ctrl+C,
# Ctrl+\ lo mata y genera un core dump para análisis
```

### SIGSTOP (19) — Detener (no capturable)

```bash
# Congela el proceso. No se puede capturar ni ignorar.
kill -STOP 1234

# El proceso se detiene completamente:
# - No ejecuta código
# - No consume CPU
# - Sigue en memoria
# - Su estado en ps es T (stopped)
```

### SIGTSTP (20) — Detener desde teclado

```bash
# Ctrl+Z envía SIGTSTP al foreground process
# Similar a SIGSTOP pero SÍ se puede capturar

# El proceso puede:
# - Capturarla y guardar estado antes de detenerse
# - Ignorarla (raro pero posible)
```

### SIGCONT (18) — Continuar

```bash
# Reanuda un proceso detenido (SIGSTOP o SIGTSTP)
kill -CONT 1234

# El proceso continúa ejecutando donde se quedó
# Es la única forma de reanudar un proceso detenido
```

### SIGUSR1 (10) y SIGUSR2 (12) — Definidas por el usuario

```bash
# No tienen significado predefinido
# Los programas les asignan el significado que quieran

# Ejemplos comunes:
# - dd: SIGUSR1 hace que imprima progreso
dd if=/dev/zero of=/dev/null bs=1M &
kill -USR1 $!   # imprime estadísticas de transferencia

# - nginx: SIGUSR1 reabre logs (log rotation)
# - PostgreSQL: SIGUSR1 para comunicación interna
```

### SIGCHLD (17) — Hijo terminó

```bash
# El kernel envía SIGCHLD al padre cuando un hijo termina o se detiene
# El padre debe hacer wait() para recoger el exit status

# Si el padre ignora SIGCHLD sin hacer wait():
# - El hijo queda como zombie (estado Z)
# - Los zombies solo ocupan una entrada en la tabla de procesos
# - Demasiados zombies pueden agotar la tabla de PIDs
```

### SIGPIPE (13) — Pipe roto

```bash
# Se envía cuando un proceso escribe a un pipe o socket
# cuyo extremo de lectura se cerró

# Ejemplo: el proceso lector de un pipe termina antes que el escritor
# El escritor recibe SIGPIPE al intentar escribir
# Acción por defecto: terminar el proceso

yes | head -1
# 'yes' recibe SIGPIPE cuando 'head' termina después de leer 1 línea
```

### SIGSEGV (11) — Segmentation fault

```bash
# El kernel la envía cuando un proceso accede a memoria inválida
# - Puntero nulo
# - Acceso fuera de los límites de memoria asignada
# - Escritura en memoria de solo lectura

# Acción por defecto: terminar + core dump
# Es un bug en el programa, no algo que se soluciona con signals
```

## Lista completa de señales estándar

```bash
# Ver todas las señales con su número
kill -l
#  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL
#  5) SIGTRAP      6) SIGABRT      7) SIGBUS       8) SIGFPE
#  9) SIGKILL     10) SIGUSR1     11) SIGSEGV     12) SIGUSR2
# 13) SIGPIPE     14) SIGALRM     15) SIGTERM     17) SIGCHLD
# 18) SIGCONT     19) SIGSTOP     20) SIGTSTP     21) SIGTTIN
# 22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ
# 26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO
# 30) SIGPWR      31) SIGSYS
# 34-64) Señales de tiempo real (SIGRTMIN a SIGRTMAX)
```

## Acciones por defecto

| Acción | Señales |
|---|---|
| Terminar | SIGHUP, SIGINT, SIGPIPE, SIGALRM, SIGTERM, SIGUSR1, SIGUSR2 |
| Terminar + core dump | SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGBUS |
| Ignorar | SIGCHLD, SIGURG, SIGWINCH |
| Detener | SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU |
| Continuar | SIGCONT |

## Orden correcto de terminación

```
1. kill PID            # SIGTERM — pide terminar limpiamente
2. Esperar 5-10 seg    # Dar tiempo al proceso para cleanup
3. kill -9 PID         # SIGKILL — solo si SIGTERM no funcionó

# Nunca usar kill -9 directamente como primera opción
# Consecuencias de SIGKILL sin previo SIGTERM:
# - Archivos temporales huérfanos
# - Base de datos con transacciones incompletas
# - Locks que quedan activos (stale locks)
# - Conexiones de red en TIME_WAIT sin cierre limpio
# - Shared memory segments sin limpiar
```

systemd sigue este protocolo automáticamente: envía SIGTERM, espera
`TimeoutStopSec` (default 90s), y luego SIGKILL si el proceso no terminó.

## Señales en contenedores

PID 1 dentro de un contenedor tiene un comportamiento especial con las señales:

```bash
# PID 1 en Linux ignora señales para las que no tiene handler
# Esto es una protección: no puedes matar init accidentalmente

# En contenedores, si el entrypoint es PID 1:
docker kill --signal=SIGTERM container    # Puede ser ignorado si PID 1
                                          # no tiene handler para SIGTERM

# Por eso Docker espera 10s tras SIGTERM y luego envía SIGKILL
# docker stop = SIGTERM + espera + SIGKILL

# Solución: usar tini o --init para que PID 1 reenvíe señales
docker run --init myimage
```

Si el entrypoint del contenedor es un script de shell, el shell como PID 1
**no reenvía señales** a los procesos hijos. El hijo nunca recibe SIGTERM y
solo muere por SIGKILL después del timeout. Esto causa shutdowns lentos. Se
resuelve con `exec` en el script o usando `tini`.

## Señales de tiempo real (RT signals)

```bash
# Señales 34-64 (SIGRTMIN a SIGRTMAX)
# Diferencias con señales estándar:
# - Se pueden encolar (las estándar se pierden si llegan mientras se procesa otra)
# - Se entregan en orden FIFO
# - Pueden llevar datos adicionales (siginfo_t)

# Son usadas internamente por glibc (threading) y por
# aplicaciones que necesitan señales confiables
```

---

## Ejercicios

### Ejercicio 1 — Observar señales

```bash
# Abrir dos terminales

# Terminal 1: proceso que captura señales
bash -c 'trap "echo SIGINT recibida" INT; trap "echo SIGTERM recibida" TERM; echo PID=$$; while true; do sleep 1; done'

# Terminal 2: enviar señales al PID que se mostró
kill -INT <PID>      # ¿Qué imprime el proceso?
kill -TERM <PID>     # ¿Se imprime algo? ¿El proceso muere?
kill -9 <PID>        # ¿Ahora sí muere?
```

### Ejercicio 2 — SIGSTOP y SIGCONT

```bash
# Iniciar un proceso que haga algo visible
ping localhost &
PID=$!

# Detenerlo
kill -STOP $PID
# ¿Se detuvo el output del ping?

# Reanudarlo
kill -CONT $PID
# ¿Continúa el ping?

# Limpieza
kill $PID
```

### Ejercicio 3 — SIGHUP para recargar

```bash
# Ver si un servicio soporta reload (internamente usa SIGHUP)
systemctl cat sshd | grep ExecReload
# ExecReload=/bin/kill -HUP $MAINPID

# Si sshd está corriendo:
# sudo systemctl reload sshd
# Equivale a: sudo kill -HUP $(pidof sshd)
```
