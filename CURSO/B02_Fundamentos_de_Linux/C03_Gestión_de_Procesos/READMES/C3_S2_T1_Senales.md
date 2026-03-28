# T01 — Señales

## Errata y notas sobre el material original

1. **max.md ejercicio 4 — trap syntax**: La línea
   `trap 'echo "Ignorando SIGTERM" TERM; :' TERM` incluye `TERM`
   dentro de las comillas del echo, imprimiendo "Ignorando SIGTERM TERM".
   Debería ser `trap 'echo "Ignorando SIGTERM"' TERM`.

2. **max.md ejercicio 7 — "Ver señales pendientes"**: No muestra señales
   pendientes. Lo que hace es ejecutar `trap` para cada señal, modificando
   los handlers del shell actual. Destructivo e incorrecto. Las señales
   pendientes se ven en `/proc/PID/status` campos `SigPnd` y `ShdPnd`.

3. **max.md ejercicio 10 — zombie demo**: `bash -c 'sleep 100 & exec sleep
   1000'` tardaría 100 segundos en producir un zombie. Y `orphaned_pid=$!`
   captura el PID del `bash -c` externo (que ya murió), no del sleep
   interno. Reescrito con demo que funciona en segundos.

4. **max.md tabla — SIGCONT**: Dice "N/A (no se captura)". Incorrecto:
   SIGCONT **sí** puede tener handler. La acción de reanudar ocurre
   siempre (no se puede bloquear), pero además se puede ejecutar código
   adicional con un handler.

5. **max.md tabla — SIGSEGV**: Dice "No (genera core dump)". Incorrecto:
   SIGSEGV **sí** es capturable (JVMs, fault handlers, sanitizers lo
   hacen). La acción por defecto es core dump, pero se puede registrar
   un handler.

6. **max.md ejercicio 5**: Paréntesis sin cerrar en comentario.

---

## Qué son las señales

Las señales son **notificaciones asíncronas** que el kernel envía a un
proceso para informarle de un evento. Son el mecanismo más básico de
comunicación entre procesos y entre el kernel y los procesos.

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

Excepto dos señales que **no pueden** ser capturadas ni ignoradas:
`SIGKILL` y `SIGSTOP`. El kernel las ejecuta directamente.

---

## Señales esenciales

### SIGTERM (15) — Terminación limpia

```bash
# "Por favor, termina ordenadamente"
# La señal por defecto de kill
kill 1234      # envía SIGTERM

# El proceso PUEDE:
# - Capturarla y hacer cleanup (cerrar archivos, guardar estado)
# - Ignorarla (proceso mal escrito o colgado)
# - No recibirla si está en estado D (uninterruptible sleep)
```

SIGTERM es la forma **correcta** de pedir a un proceso que termine. Le da
la oportunidad de cerrar conexiones, guardar datos y limpiar archivos
temporales.

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

SIGKILL es el **último recurso**. El proceso no tiene oportunidad de
limpiar: archivos temporales quedan en disco, conexiones no se cierran,
buffers no se escriben, locks no se liberan.

### SIGINT (2) — Interrupción del teclado

```bash
# Ctrl+C en el terminal envía SIGINT al foreground process group
# (no solo al proceso, sino a todo su grupo)

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

El doble significado es un artefacto histórico: los daemons se desacoplan
del terminal (no tienen tty), así que SIGHUP "sobra" y se reutiliza.

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
# La acción de reanudar siempre ocurre (no se puede bloquear)
# Además, el proceso puede tener un handler que ejecute código adicional
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

yes | head -1
# 'yes' recibe SIGPIPE cuando 'head' termina después de leer 1 línea
# Acción por defecto: terminar el proceso escritor
```

### SIGSEGV (11) — Segmentation fault

```bash
# El kernel la envía cuando un proceso accede a memoria inválida
# - Puntero nulo
# - Acceso fuera de los límites de memoria asignada
# - Escritura en memoria de solo lectura

# Acción por defecto: terminar + core dump
# Es capturable (JVMs, sanitizers lo hacen) pero generalmente
# indica un bug en el programa
```

---

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

# Convertir entre nombre y número
kill -l TERM    # → 15
kill -l 15      # → TERM
```

## Acciones por defecto

| Acción | Señales |
|---|---|
| Terminar | SIGHUP, SIGINT, SIGPIPE, SIGALRM, SIGTERM, SIGUSR1, SIGUSR2 |
| Terminar + core dump | SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGBUS |
| Ignorar | SIGCHLD, SIGURG, SIGWINCH |
| Detener | SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU |
| Continuar | SIGCONT |

## Tabla: señales comunes en la práctica

| Señal | Nº | Capturable | Default | Uso típico |
|---|---|---|---|---|
| SIGTERM | 15 | Sí | Terminar | `kill PID` (default) |
| SIGKILL | 9 | **No** | Terminar | Forzar muerte |
| SIGINT | 2 | Sí | Terminar | Ctrl+C |
| SIGQUIT | 3 | Sí | Core dump | Ctrl+\\ |
| SIGHUP | 1 | Sí | Terminar | Reload / terminal cerrado |
| SIGSTOP | 19 | **No** | Detener | Congelar proceso |
| SIGTSTP | 20 | Sí | Detener | Ctrl+Z |
| SIGCONT | 18 | Sí | Reanudar | Despertar proceso |
| SIGUSR1/2 | 10/12 | Sí | Terminar | Definido por app |
| SIGCHLD | 17 | Sí | Ignorar | Hijo terminó |
| SIGPIPE | 13 | Sí | Terminar | Pipe roto |
| SIGSEGV | 11 | Sí | Core dump | Acceso a memoria inválida |
| SIGALRM | 14 | Sí | Terminar | Temporizador expirado |

---

## `trap` — Capturar señales en bash

```bash
# Sintaxis: trap 'comandos' SEÑAL [SEÑAL...]

# Capturar SIGTERM para hacer cleanup antes de salir
trap 'echo "Limpiando..."; rm -f /tmp/mi_lock; exit 0' TERM

# Capturar SIGINT (Ctrl+C) para limpiar archivos temporales
trap 'echo "Interrumpido"; cleanup; exit 1' INT

# Ignorar una señal (cadena vacía)
trap '' HUP    # Ignora SIGHUP

# Restaurar acción por defecto
trap - TERM    # Vuelve al comportamiento default para SIGTERM

# Trap especial: EXIT se ejecuta al salir (por cualquier razón)
trap 'rm -f $TMPFILE' EXIT

# Listar traps activos
trap -p
```

El patrón más común en scripts:

```bash
#!/bin/bash
TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"; exit' INT TERM EXIT

# ... usar $TMPFILE ...
# Al salir (por señal o normalmente), se limpia automáticamente
```

---

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

---

## `nohup` — Sobrevivir a SIGHUP

Cuando cierras un terminal, los procesos lanzados desde él reciben
SIGHUP y mueren. `nohup` los protege:

```bash
# Sin nohup: muere al cerrar el terminal
long_running_command &

# Con nohup: sobrevive al cierre del terminal
nohup long_running_command &
# - Ignora SIGHUP
# - Redirige stdout/stderr a nohup.out si no están redirigidos

# Alternativa moderna: disown
long_running_command &
disown %1    # Quita el job del control del shell
             # El proceso ya no recibe SIGHUP al cerrar
```

---

## Señales en contenedores

PID 1 dentro de un contenedor tiene un comportamiento especial:

```bash
# PID 1 en Linux ignora señales para las que no tiene handler
# Esto es una protección: no puedes matar init accidentalmente

# En contenedores, si el entrypoint es PID 1:
docker kill --signal=SIGTERM container    # Puede ser ignorado
                                          # si PID 1 no tiene handler

# Por eso Docker espera 10s tras SIGTERM y luego envía SIGKILL
# docker stop = SIGTERM + espera (10s) + SIGKILL
```

Si el entrypoint es un script de shell, el shell como PID 1 **no reenvía
señales** a los procesos hijos. El hijo nunca recibe SIGTERM y solo muere
por SIGKILL después del timeout. Esto causa shutdowns lentos.

**Soluciones:**
1. `exec` en el script: `exec my_app` (reemplaza el shell, app es PID 1)
2. `docker run --init`: usa tini como PID 1 que reenvía señales
3. Registrar trap en el script: `trap 'kill $PID' TERM`

---

## Señales de tiempo real (RT signals)

```bash
# Señales 34-64 (SIGRTMIN a SIGRTMAX)
# Diferencias con señales estándar:
# - Se pueden encolar (las estándar se pierden si llegan varias)
# - Se entregan en orden FIFO
# - Pueden llevar datos adicionales (siginfo_t)

# Son usadas internamente por glibc (threading) y por
# aplicaciones que necesitan señales confiables
```

---

## Señales en /proc: máscara y estado

El archivo `/proc/PID/status` contiene campos de señales como bitmasks
hexadecimales:

```bash
grep Sig /proc/$$/status
# SigQ:   0/63229          ← señales en cola / máximo
# SigPnd: 0000000000000000 ← señales pendientes para este thread
# ShdPnd: 0000000000000000 ← señales pendientes compartidas (proceso)
# SigBlk: 0000000000010000 ← señales bloqueadas
# SigIgn: 0000000000380004 ← señales ignoradas
# SigCgt: 000000004b817efb ← señales con handler registrado
```

Cada bit corresponde a una señal (bit 0 = señal 1, bit 1 = señal 2, etc.).

```bash
# Decodificar: ¿qué señales tiene capturadas?
python3 -c "
mask = 0x000000004b817efb
for i in range(1, 32):
    if mask & (1 << (i - 1)):
        print(f'  Signal {i}')
"
```

---

## Labs

### Parte 1 — Señales básicas

#### 1.1 Listar señales disponibles

```bash
echo "=== Señales disponibles ==="
kill -l
echo ""
echo "=== Las más importantes ==="
echo " 1) SIGHUP   — terminal cerrada / reload"
echo " 2) SIGINT   — Ctrl+C"
echo " 3) SIGQUIT  — Ctrl+\\ (core dump)"
echo " 9) SIGKILL  — matar (no capturable)"
echo "15) SIGTERM  — terminar (default de kill, capturable)"
echo "18) SIGCONT  — continuar proceso detenido"
echo "19) SIGSTOP  — detener (no capturable)"
echo "20) SIGTSTP  — Ctrl+Z (capturable)"
```

#### 1.2 SIGTERM — terminación educada

```bash
sleep 300 &
PID=$!
echo "sleep PID: $PID"
ps -o pid,stat,cmd -p $PID

echo ""
echo "=== Enviar SIGTERM ==="
kill $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso terminado"
wait $PID 2>/dev/null
```

`kill PID` sin señal envía SIGTERM (15). Es la forma correcta de pedir
a un proceso que termine.

#### 1.3 SIGKILL — terminación forzada

```bash
sleep 300 &
PID=$!
echo "sleep PID: $PID"

echo ""
echo "=== Enviar SIGKILL ==="
kill -9 $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso matado"
wait $PID 2>/dev/null
echo "SIGKILL: el kernel destruye el proceso sin cleanup"
```

#### 1.4 SIGINT — equivalente a Ctrl+C

```bash
sleep 300 &
PID=$!
echo "sleep PID: $PID"

kill -INT $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Terminado por SIGINT"
wait $PID 2>/dev/null
echo "SIGINT se envía a todo el foreground process group"
```

---

### Parte 2 — SIGHUP, SIGSTOP, SIGCONT

#### 2.1 SIGHUP — doble uso

```bash
echo "=== SIGHUP termina un proceso normal ==="
sleep 300 &
PID=$!
kill -HUP $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "sleep terminado por SIGHUP"
wait $PID 2>/dev/null
echo ""
echo "Para daemons, SIGHUP = recargar config (kill -HUP \$(pidof nginx))"
```

#### 2.2 SIGSTOP y SIGCONT — pausar y resumir

```bash
sleep 300 &
PID=$!
echo "sleep PID: $PID, estado inicial:"
ps -o pid,stat,cmd -p $PID

echo ""
echo "=== Enviar SIGSTOP ==="
kill -STOP $PID
sleep 0.5
echo "Después de SIGSTOP:"
ps -o pid,stat,cmd -p $PID
echo "(estado T = stopped)"

echo ""
echo "=== Enviar SIGCONT ==="
kill -CONT $PID
sleep 0.5
echo "Después de SIGCONT:"
ps -o pid,stat,cmd -p $PID
echo "(vuelve a S = sleeping)"

kill $PID; wait $PID 2>/dev/null
```

SIGSTOP no se puede capturar — el kernel congela el proceso. SIGCONT lo
despierta.

#### 2.3 Tres formas de enviar una señal

```bash
sleep 300 & P1=$!
sleep 301 & P2=$!
sleep 302 & P3=$!

kill -15 $P1        # por número
kill -TERM $P2      # por nombre sin SIG
kill -SIGTERM $P3   # por nombre completo (bash)

sleep 0.5
echo "Las tres son equivalentes. Todos terminados:"
ps -o pid,stat,cmd -p $P1,$P2,$P3 2>/dev/null || echo "(ninguno existe)"
wait 2>/dev/null
```

---

### Parte 3 — Señales en contenedores

#### 3.1 PID 1 tiene protección especial

```bash
echo "=== PID 1 en este contenedor ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== Intentar SIGTERM a PID 1 ==="
kill -TERM 1 2>&1 || true
echo ""
echo "PID 1 sigue vivo:"
ps -p 1 -o pid,stat,cmd
echo ""
echo "El kernel protege PID 1: solo entrega señales con handler registrado"
echo "SIGTERM sin handler = ignorada silenciosamente"
```

#### 3.2 docker stop = SIGTERM + espera + SIGKILL

```bash
echo "=== Proceso normal vs PID 1 ==="
sleep 300 &
PID=$!
kill -TERM $PID
sleep 0.5
ps -o pid,stat,cmd -p $PID 2>/dev/null || echo "Proceso normal: terminado"
wait $PID 2>/dev/null
echo ""
echo "PID 1 sigue vivo: $(ps -p 1 -o cmd= 2>/dev/null)"
echo ""
echo "docker stop:"
echo "1. Envía SIGTERM"
echo "2. Si PID 1 no tiene handler, la ignora"
echo "3. Espera 10s (StopTimeout)"
echo "4. Envía SIGKILL"
echo ""
echo "Solución: docker run --init (tini como PID 1)"
```

---

## Ejercicios

### Ejercicio 1 — trap: capturar señales en bash

```bash
# Terminal 1: proceso con trap
bash -c '
    trap "echo [SIGINT recibida]" INT
    trap "echo [SIGTERM recibida] — limpiando...; rm -f /tmp/trap_test; exit 0" TERM
    trap "echo [Saliendo]; rm -f /tmp/trap_test" EXIT

    touch /tmp/trap_test
    echo "PID=$$  — esperando señales (archivo /tmp/trap_test creado)"
    while true; do sleep 1; done
'

# Terminal 2: enviar señales
# kill -INT <PID>     → ¿Qué imprime? ¿Muere el proceso?
# kill -TERM <PID>    → ¿Qué imprime? ¿Se borró /tmp/trap_test?
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**SIGINT**: Imprime "[SIGINT recibida]" pero el proceso **no muere**
porque el handler no incluye `exit`. El trap captura la señal y ejecuta
solo el echo. El bucle `while true` continúa.

**SIGTERM**: Imprime "[SIGTERM recibida] — limpiando..." y luego
"[Saliendo]" (del trap EXIT). El proceso muere porque el handler
incluye `exit 0`. El archivo `/tmp/trap_test` se borra (cleanup
del handler de TERM + handler de EXIT).

**Punto clave**: `trap` sin `exit` solo ejecuta el código y continúa.
Para que la señal termine el proceso, el handler debe incluir `exit`.
El trap `EXIT` se ejecuta siempre que el proceso termina, sea por
señal o por salida normal.

</details>

---

### Ejercicio 2 — SIGTERM permite cleanup, SIGKILL no

```bash
# Proceso con cleanup
cleanup_test() {
    bash -c "
        trap 'echo \"Cleanup: borrando archivo\"; rm -f /tmp/cleanup_$1; exit' TERM
        touch /tmp/cleanup_$1
        echo \"PID=\$\$ archivo=/tmp/cleanup_$1 creado\"
        while true; do sleep 1; done
    " &
}

echo "=== Test con SIGTERM ==="
cleanup_test "term"
PID_TERM=$!
sleep 1
kill $PID_TERM
wait $PID_TERM 2>/dev/null
echo "¿Archivo existe? $(ls /tmp/cleanup_term 2>/dev/null && echo SÍ || echo NO)"

echo ""
echo "=== Test con SIGKILL ==="
cleanup_test "kill"
PID_KILL=$!
sleep 1
kill -9 $PID_KILL
wait $PID_KILL 2>/dev/null
echo "¿Archivo existe? $(ls /tmp/cleanup_kill 2>/dev/null && echo SÍ || echo NO)"

rm -f /tmp/cleanup_term /tmp/cleanup_kill
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**SIGTERM**: El handler se ejecuta, imprime "Cleanup: borrando archivo",
y borra `/tmp/cleanup_term`. Al preguntar si existe → **NO**.

**SIGKILL**: El kernel destruye el proceso instantáneamente. El handler
nunca se ejecuta. El archivo `/tmp/cleanup_kill` **sigue existiendo** →
**SÍ**.

**Esta es la razón fundamental para usar SIGTERM primero**: permite que
el proceso cierre archivos, libere locks, guarde transacciones, etc.
SIGKILL los deja huérfanos.

</details>

---

### Ejercicio 3 — SIGSTOP/SIGCONT: pausar y reanudar

```bash
# Crear un proceso que muestra actividad
bash -c 'i=0; while true; do echo "tick $i"; i=$((i+1)); sleep 1; done' &
PID=$!
echo "PID: $PID"
sleep 3

echo ""
echo "=== SIGSTOP: congelando ==="
kill -STOP $PID
echo "Estado: $(ps -o stat= -p $PID)"
sleep 3
echo "(3 segundos sin output — proceso congelado)"

echo ""
echo "=== SIGCONT: reanudando ==="
kill -CONT $PID
echo "Estado: $(ps -o stat= -p $PID)"
sleep 3

kill $PID; wait $PID 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. Primeros 3 segundos: el proceso imprime "tick 0", "tick 1", "tick 2".

2. Después de SIGSTOP: el estado cambia a `T` (stopped). No hay más
   output durante 3 segundos. El proceso no consume CPU pero sigue
   en memoria.

3. Después de SIGCONT: el estado vuelve a `S` (sleeping). El output
   se reanuda con "tick 3", "tick 4", "tick 5". El contador continúa
   donde se quedó.

**Nota**: SIGSTOP no se puede capturar. Ni siquiera `trap '' STOP`
funciona — el kernel lo ignora. SIGTSTP (Ctrl+Z) sí se puede capturar.

</details>

---

### Ejercicio 4 — Proceso que ignora SIGTERM

```bash
bash -c '
    trap "" TERM    # Cadena vacía = ignorar la señal
    echo "PID=$$ — ignoro SIGTERM, acepto SIGINT"
    while true; do sleep 1; done
' &
PID=$!
sleep 1

echo "=== Intentar SIGTERM ==="
kill $PID
sleep 1
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"

echo ""
echo "=== Intentar SIGINT ==="
kill -INT $PID
sleep 1
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"

echo ""
echo "=== SIGKILL (último recurso) ==="
kill -9 $PID 2>/dev/null
wait $PID 2>/dev/null
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- **SIGTERM**: El proceso la ignora (`trap "" TERM` = descartar).
  Sigue vivo → **SÍ**.

- **SIGINT**: No tiene trap para SIGINT, así que se aplica la acción
  por defecto → **terminar**. Respuesta: **NO**.

  (Si el trap fuera `trap "" INT` también, entonces SIGINT sería
  ignorada y habría que usar SIGKILL.)

- **SIGKILL**: Siempre mata. No importa qué traps tenga el proceso.
  Respuesta: **NO**.

**Lección**: `trap "" SIGNAL` ignora la señal completamente (no
ejecuta nada). Es diferente de `trap 'comando' SIGNAL` que ejecuta
el comando pero continúa. Y `trap - SIGNAL` restaura la acción
por defecto.

</details>

---

### Ejercicio 5 — SIGUSR1: enviar progreso a dd

```bash
echo "=== dd con SIGUSR1 para ver progreso ==="
dd if=/dev/zero of=/dev/null bs=1M count=5000 &
DD_PID=$!

sleep 1
echo "Enviando SIGUSR1 a dd (PID $DD_PID)..."
kill -USR1 $DD_PID 2>/dev/null
sleep 2
kill -USR1 $DD_PID 2>/dev/null
sleep 1

wait $DD_PID 2>/dev/null
echo ""
echo "SIGUSR1 en dd imprime bytes transferidos sin detener la operación"
echo "(en versiones modernas de dd, usar status=progress es más cómodo)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Cada vez que dd recibe SIGUSR1, imprime una línea como:
```
2048+0 records in
2048+0 records out
2147483648 bytes (2.1 GB, 2.0 GiB) copied, 1.23 s, 1.7 GB/s
```

El proceso **no se detiene** — continúa la copia. SIGUSR1 solo activa
la impresión de estadísticas.

**Nota**: En `dd` moderno (coreutils ≥ 8.24) se puede usar
`dd ... status=progress` para ver el progreso continuamente sin
necesidad de enviar señales manualmente. Pero SIGUSR1 funciona en
todas las versiones.

**¿Por qué SIGUSR1 y no otra señal?** Porque las señales USR1/USR2
no tienen significado predefinido — están reservadas para que cada
programa las use como quiera. dd eligió USR1 para "mostrar progreso".

</details>

---

### Ejercicio 6 — Crear un zombie

```bash
echo "=== Creando un zombie ==="
# El padre (bash) lanza un hijo y duerme sin hacer wait()
bash -c '
    bash -c "exit 0" &
    CHILD=$!
    echo "Padre=$$, Hijo=$CHILD"
    echo "Esperando 3 segundos sin hacer wait()..."
    sleep 3
    echo "Estado del hijo:"
    ps -o pid,ppid,stat,cmd -p $CHILD 2>/dev/null
    echo ""
    echo "El hijo está en estado Z (zombie) porque:"
    echo "- Ya terminó (exit 0)"
    echo "- El padre no ha hecho wait() para recoger su exit status"
    echo ""
    echo "Ahora el padre hace wait()..."
    wait $CHILD 2>/dev/null
    echo "¿El zombie desapareció?"
    ps -o pid,ppid,stat,cmd -p $CHILD 2>/dev/null || echo "Sí — ya no existe"
'
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

1. El hijo (`bash -c "exit 0"`) termina inmediatamente.

2. Durante los 3 segundos de sleep del padre, el hijo aparece con
   estado `Z+` (zombie) en `ps`. Su COMMAND puede mostrar `[bash]
   <defunct>`.

3. Después de `wait`, el zombie desaparece. `ps` ya no lo encuentra.

**¿Qué es un zombie?**
- Un proceso que terminó pero cuyo padre no ha llamado `wait()`
- Solo ocupa una entrada en la tabla de procesos (no consume CPU ni RAM)
- El kernel mantiene solo su PID, exit status, y uso de recursos
- Si el padre muere, el zombie es adoptado por PID 1 que hace wait()

**¿Cuándo son un problema?**
- Si un proceso crea miles de hijos sin hacer wait(), los zombies
  pueden agotar la tabla de PIDs (máximo /proc/sys/kernel/pid_max)
- Solución: arreglar el programa padre para que haga wait(), o matar
  al padre (los zombies son adoptados por init que los limpia)

</details>

---

### Ejercicio 7 — nohup: sobrevivir a SIGHUP

```bash
echo "=== Sin nohup: muere con SIGHUP ==="
sleep 300 &
PID_NORMAL=$!
kill -HUP $PID_NORMAL
sleep 0.5
echo "Sin nohup: $(ps -o pid= -p $PID_NORMAL 2>/dev/null && echo VIVE || echo MUERTO)"
wait $PID_NORMAL 2>/dev/null

echo ""
echo "=== Con trap para ignorar SIGHUP ==="
bash -c 'trap "" HUP; sleep 300' &
PID_NOHUP=$!
sleep 0.5
kill -HUP $PID_NOHUP
sleep 0.5
echo "Con trap '' HUP: $(ps -o pid= -p $PID_NOHUP 2>/dev/null && echo VIVE || echo MUERTO)"
kill $PID_NOHUP; wait $PID_NOHUP 2>/dev/null

echo ""
echo "=== nohup hace lo mismo que trap '' HUP ==="
echo "nohup long_command &     → ignora SIGHUP, redirige a nohup.out"
echo "disown %1                → alternativa: quita job del shell"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- **Sin nohup**: sleep recibe SIGHUP y muere (acción por defecto de
  SIGHUP = terminar). Resultado: **MUERTO**.

- **Con `trap "" HUP`**: La señal es ignorada. sleep sobrevive.
  Resultado: **VIVE**.

`nohup` internamente hace lo mismo: ignora SIGHUP y redirige
stdout/stderr a `nohup.out` (si no están ya redirigidos).

**Escenarios prácticos:**
- SSH se cierra inesperadamente → los procesos reciben SIGHUP
- Cierras la terminal → SIGHUP a los procesos de esa sesión
- `nohup`, `disown`, `tmux`, o `screen` protegen contra esto

</details>

---

### Ejercicio 8 — Señales pendientes y máscara en /proc

```bash
echo "=== Campos de señales en /proc/\$\$/status ==="
grep '^Sig' /proc/$$/status
echo ""
echo "Campos:"
echo "  SigPnd — señales pendientes (este thread)"
echo "  ShdPnd — señales pendientes (compartidas, proceso)"
echo "  SigBlk — señales bloqueadas (no se entregan hasta desbloquear)"
echo "  SigIgn — señales ignoradas"
echo "  SigCgt — señales con handler registrado (trap)"

echo ""
echo "=== Decodificar SigCgt ==="
CGT=$(grep '^SigCgt:' /proc/$$/status | awk '{print $2}')
echo "SigCgt hex: $CGT"
echo "Señales capturadas:"
python3 -c "
mask = int('$CGT', 16)
sigs = {1:'HUP',2:'INT',3:'QUIT',6:'ABRT',10:'USR1',12:'USR2',
        13:'PIPE',14:'ALRM',15:'TERM',17:'CHLD',28:'WINCH'}
for bit in range(1, 32):
    if mask & (1 << (bit - 1)):
        name = sigs.get(bit, f'SIG{bit}')
        print(f'  {bit:2d}) {name}')
" 2>/dev/null || echo "(python3 no disponible)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Los campos son máscaras hexadecimales donde cada bit corresponde a
una señal (bit 0 = señal 1, bit 1 = señal 2, etc.).

**Para bash interactivo** típicamente:
- **SigBlk**: Puede tener bits para SIGCHLD u otras señales
  temporalmente bloqueadas
- **SigIgn**: Puede incluir SIGTSTP, SIGTTIN, SIGTTOU (bash las
  ignora para manejar jobs correctamente)
- **SigCgt**: Incluye SIGINT, SIGTERM, SIGHUP, SIGWINCH, SIGCHLD
  (bash tiene handlers para todas estas)
- **SigPnd**: Normalmente 0 (no hay señales pendientes sin entregar)

El decodificador convierte el hex a señales legibles. Por ejemplo,
si SigCgt contiene bit 1 (HUP), bit 2 (INT), bit 14 (ALRM), bit 15
(TERM), bit 17 (CHLD), bit 28 (WINCH) — todo esto es normal para bash.

</details>

---

### Ejercicio 9 — Escalada de señales: TERM → INT → QUIT → KILL

```bash
bash -c '
    trap "echo \"[SIGTERM ignorada]\"" TERM
    trap "echo \"[SIGINT ignorada]\"" INT
    trap "echo \"[SIGQUIT ignorada]\"" QUIT
    echo "PID=$$ — ignoro TERM, INT, QUIT. Solo SIGKILL me mata."
    while true; do sleep 1; done
' &
PID=$!
sleep 1

echo "=== Intentar SIGTERM ==="
kill -TERM $PID; sleep 1
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"

echo "=== Intentar SIGINT ==="
kill -INT $PID; sleep 1
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"

echo "=== Intentar SIGQUIT ==="
kill -QUIT $PID; sleep 1
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"

echo "=== SIGKILL (no se puede capturar) ==="
kill -KILL $PID
wait $PID 2>/dev/null
echo "¿Vive? $(ps -o pid= -p $PID 2>/dev/null && echo SÍ || echo NO)"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- **SIGTERM**: Capturada → imprime "[SIGTERM ignorada]" → **VIVE**
- **SIGINT**: Capturada → imprime "[SIGINT ignorada]" → **VIVE**
- **SIGQUIT**: Capturada → imprime "[SIGQUIT ignorada]" → **VIVE**
  (nota: el trap sobreescribe la acción por defecto que sería core dump)
- **SIGKILL**: **No se puede capturar** → proceso muere → **NO**

**Escalada en la práctica:**
```
1. kill PID          (SIGTERM — "por favor termina")
2. kill -INT PID     (SIGINT — "como Ctrl+C")
3. kill -QUIT PID    (SIGQUIT — "quit, dame core dump")
4. kill -9 PID       (SIGKILL — "muere ahora, sin negociación")
```

En realidad, si SIGTERM no funciona, raramente SIGINT o SIGQUIT
funcionarán (si un proceso ignora una, probablemente ignora todas).
La escalada real es: SIGTERM → esperar → SIGKILL.

</details>

---

### Ejercicio 10 — Señales y PID 1 en contenedores

```bash
echo "=== ¿Quién es PID 1? ==="
ps -p 1 -o pid,user,comm,args
echo ""

echo "=== ¿Tiene handler para SIGTERM? ==="
if [ -r /proc/1/status ]; then
    CGT=$(grep '^SigCgt:' /proc/1/status | awk '{print $2}')
    # Bit 14 (0-indexed) = señal 15 (SIGTERM)
    TERM_BIT=$((1 << 14))
    MASK=$((16#$CGT))
    if (( MASK & TERM_BIT )); then
        echo "SÍ — PID 1 tiene handler para SIGTERM"
        echo "(docker stop será rápido)"
    else
        echo "NO — PID 1 ignora SIGTERM"
        echo "(docker stop tardará 10s antes de SIGKILL)"
    fi
fi

echo ""
echo "=== Enviar SIGTERM a PID 1 ==="
kill -TERM 1 2>&1 || true
sleep 0.5
echo "PID 1 sigue vivo: $(ps -p 1 -o comm= 2>/dev/null)"

echo ""
echo "=== Enviar SIGTERM a un proceso normal ==="
sleep 300 &
P=$!
kill -TERM $P
sleep 0.5
echo "sleep: $(ps -o pid= -p $P 2>/dev/null && echo VIVE || echo MUERTO)"
wait $P 2>/dev/null
echo ""
echo "Conclusión: PID 1 tiene protección especial del kernel"
echo "Solo recibe señales para las que tiene handler registrado"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**PID 1 en un contenedor** es típicamente el entrypoint (bash, o el
proceso principal). El kernel lo protege: solo entrega señales para
las que PID 1 ha registrado un handler.

**¿Tiene handler para SIGTERM?** Depende del entrypoint:
- Si es una aplicación bien escrita (nginx, node) → **SÍ** → docker
  stop es rápido
- Si es `bash` o un script sin trap → **NO** → docker stop tarda 10s

**SIGTERM a PID 1**: Si no tiene handler, el kernel la descarta
silenciosamente. PID 1 sigue vivo. Esto no ocurre con procesos
normales — un sleep sin handler para SIGTERM muere con la acción
por defecto (terminar).

**¿Por qué esta protección?** PID 1 (init) es especial: si muere,
el kernel entra en pánico (kernel panic en el host) o el contenedor
se detiene. La protección previene que señales accidentales lo maten.

**Soluciones para shutdowns rápidos:**
1. `exec` en entrypoint scripts → la app es PID 1 con sus handlers
2. `docker run --init` → tini como PID 1, reenvía señales
3. `trap 'kill $APP_PID' TERM` en el script

</details>
