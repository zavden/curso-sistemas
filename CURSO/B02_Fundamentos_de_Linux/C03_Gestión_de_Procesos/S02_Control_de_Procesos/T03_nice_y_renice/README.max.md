# T03 — nice y renice (Enhanced)

## Prioridades de procesos

Linux usa el scheduler **CFS** (Completely Fair Scheduler) para distribuir
tiempo de CPU entre procesos. Cada proceso tiene una prioridad que influye en
cuánto tiempo de CPU recibe en relación con otros procesos:

```
Dos conceptos relacionados pero diferentes:

nice value:  -20 ←───────── 0 ──────────→ +19
              alta prioridad  default      baja prioridad
              (más CPU)                  (menos CPU)

PR (priority): lo que muestra top
              PR = 20 + nice
              PR va de 0 a 39 para procesos normales
              PR 20 = nice 0 (default)
```

### Qué hace realmente el nice value

El nice value **no es un porcentaje de CPU**. Es un **peso relativo**. CFS
asigna tiempo proporcional al peso:

```
nice  0 → peso 1024
nice  1 → peso 820  (×0.8 aprox)
nice -1 → peso 1277 (×1.25 aprox)
nice 10 → peso 110  (×0.1 aprox)
nice 19 → peso 15   (×0.015 aprox)

Si hay dos procesos compitiendo por CPU:
  nice 0 vs nice 0  → 50%/50%
  nice 0 vs nice 10 → 91%/9%
  nice 0 vs nice 19 → 99%/1%
  nice -5 vs nice 0 → 75%/25%
```

El nice value solo importa **cuando hay contención** — si el sistema tiene CPU
libre, un proceso con nice 19 aún recibe todo el CPU que necesite.

## nice — Iniciar un proceso con prioridad modificada

```bash
# Sintaxis
nice [-n ajuste] comando [argumentos]

# Iniciar un proceso con nice 10 (prioridad baja)
nice -n 10 make -j4

# Nice 19 (mínima prioridad)
nice -n 19 find / -name "*.log" -mtime +30

# Sin argumento -n, nice usa 10 por defecto
nice make -j4    # equivale a nice -n 10

# Nice negativo (alta prioridad) — requiere root
sudo nice -n -5 /usr/sbin/critical-service
```

### Quién puede hacer qué

```bash
# Usuarios regulares:
# - Pueden SUBIR nice (bajar prioridad): 0 → 1..19
# - NO pueden BAJAR nice (subir prioridad): 0 → -1..-20
# - NO pueden volver atrás: si subieron a 10, no pueden volver a 0

nice -n 10 sleep 100 &    # OK
nice -n -5 sleep 100 &    # nice: cannot set niceness: Permission denied

# root:
# - Puede cualquier valor de -20 a 19
# - Puede bajar nice de procesos de cualquier usuario

sudo nice -n -10 sleep 100 &    # OK
```

### Límite configurable con ulimit

```bash
# El hard limit de nice para usuarios se puede ajustar
ulimit -e
# 0   (default — solo puede subir nice)

# En /etc/security/limits.conf se puede dar permiso a usuarios:
# dev  hard  nice  -5
# @audio  -  nice  -10    # grupo audio puede usar nice -10
```

## renice — Cambiar prioridad de un proceso en ejecución

```bash
# Sintaxis
renice [-n] ajuste [-p PID] [-g PGRP] [-u user]

# Cambiar nice de un proceso
renice -n 10 -p 1234

# Cambiar nice de todos los procesos de un usuario
renice -n 15 -u dev

# Cambiar nice de un process group
renice -n 5 -g 1234
```

### Restricciones de renice

```bash
# Usuarios regulares:
# - Solo pueden subir nice de SUS propios procesos
renice -n 15 -p 1234     # OK si 1234 es del usuario y nice actual < 15
renice -n 5 -p 1234      # Falla si nice actual es 10 (no puede bajar)
renice -n 10 -p 9999     # Falla si 9999 no es del usuario

# root:
# - Puede cualquier valor, cualquier proceso
sudo renice -n -20 -p 1234    # máxima prioridad
sudo renice -n 19 -u dev      # bajar prioridad a todo lo de dev
```

## Ver prioridades

```bash
# Con ps
ps -eo pid,ni,pri,cmd --sort=ni | head
# PID   NI  PRI  CMD
# 842  -10   30  /usr/sbin/irqbalance
# 1     0    20  /usr/lib/systemd/systemd
# 1842  10   10  make -j4

# Con top
# Columnas PR y NI
# PR = 20 + NI (para procesos normales)
# PR = rt para procesos real-time

# Con /proc
cat /proc/1234/stat | awk '{print "nice:", $19}'
```

## ionice — Prioridad de I/O

`nice` solo afecta la **CPU**. Para controlar la prioridad de acceso a disco,
existe `ionice`:

```bash
# Tres clases de I/O scheduling (con el scheduler CFQ):
# 1 = Realtime   — acceso prioritario al disco
# 2 = Best-effort — default, con 8 niveles (0-7, menor = más prioridad)
# 3 = Idle       — solo usa I/O cuando nadie más lo usa

# Ver la clase actual de un proceso
ionice -p 1234
# best-effort: prio 4

# Iniciar un proceso con prioridad idle de I/O
ionice -c 3 tar czf backup.tar.gz /srv/data

# Iniciar con best-effort nivel 7 (mínima dentro de best-effort)
ionice -c 2 -n 7 updatedb

# Cambiar un proceso existente
ionice -c 3 -p 1234

# Combinar nice e ionice para tareas de mantenimiento
nice -n 19 ionice -c 3 find / -name "*.tmp" -delete
```

### Schedulers de I/O y ionice

```bash
# ionice con clases funciona plenamente con el scheduler CFQ
# Los schedulers modernos (mq-deadline, bfq, none) pueden
# no respetar todas las clases

# Ver el scheduler actual de un disco
cat /sys/block/sda/queue/scheduler
# [mq-deadline] kyber bfq none

# BFQ respeta ionice. mq-deadline y none lo ignoran parcialmente.
# En SSDs con none/mq-deadline, ionice tiene poco efecto.
```

## nice y renice en systemd

Los servicios systemd establecen nice en su unit file, no con el comando `nice`:

```bash
# En el unit file del servicio
# [Service]
# Nice=10
# IOSchedulingClass=idle
# IOSchedulingPriority=7

# Ver la configuración de un servicio
systemctl show nginx | grep -E 'Nice|IOScheduling'
# Nice=0
# IOSchedulingClass=0
# IOSchedulingPriority=0

# Cambiar temporalmente (hasta el próximo reinicio del servicio)
sudo systemctl set-property nginx Nice=10

# Cambiar permanentemente
sudo systemctl edit nginx
# [Service]
# Nice=10
```

## Cuándo usar nice/renice

| Tarea | Nice sugerido | Razón |
|---|---|---|
| Compilación (`make -j`) | 10-15 | No debe afectar servicios interactivos |
| Backups (`tar`, `rsync`) | 15-19 | Puede esperar, no es urgente |
| Indexación (`updatedb`, `locate`) | 19 | Tarea de fondo de baja prioridad |
| Base de datos crítica | -5 a -10 | Necesita respuesta rápida (requiere root) |
| Servicio web en producción | 0 (default) | El default es suficiente normalmente |
| Proceso de debugging | 10 | No debe interferir con producción |

La recomendación general: no cambiar prioridades a menos que haya un problema
observable de contención. El scheduler de Linux funciona bien con los defaults
en la mayoría de situaciones.

## Tabla: nice values y pesos relativos

| Nice | Peso CFS | % CPU relativo (2 procesos) |
|---|---|---|
| -20 | 20761 | 96.2% |
| -10 | 3355 | 79.8% |
| -5 | 1688 | 67.5% |
| 0 (default) | 1024 | 50.0% |
| 10 | 110 | 9.1% |
| 19 | 15 | 1.5% |

## ionice — clases y prioridades

| Clase | Flag -c | Niveles | Descripción |
|---|---|---|---|
| Realtime | -c 1 | 0-7 | Acceso prioritario absoluto |
| Best-effort | -c 2 | 0-7 | Default, 0=máxima prioridad |
| Idle | -c 3 | (ninguno) | Solo cuando nadie más usa disco |

## Ejercicios

### Ejercicio 1 — nice al iniciar

```bash
# Comparar nice values
sleep 100 &
PID1=$!
nice -n 15 sleep 100 &
PID2=$!

# Ver las prioridades
ps -o pid,ni,pri,cmd -p $PID1,$PID2
# ¿Cuál tiene mayor prioridad?

kill $PID1 $PID2
```

### Ejercicio 2 — renice en ejecución

```bash
# Iniciar un proceso
sleep 200 &
PID=$!

# Ver su nice actual
ps -o pid,ni -p $PID

# Subir nice a 10
renice -n 10 -p $PID
ps -o pid,ni -p $PID

# Intentar bajarlo a 5 (como usuario regular)
renice -n 5 -p $PID
# ¿Qué error da?

# Intentar subirlo más a 19
renice -n 19 -p $PID
ps -o pid,ni -p $PID

kill $PID
```

### Ejercicio 3 — ionice

```bash
# Ver tu clase de I/O actual
ionice -p $$

# Iniciar un proceso con I/O idle
ionice -c 3 dd if=/dev/zero of=/tmp/ionice-test bs=1M count=100 &
PID=$!

# Verificar su clase
ionice -p $PID

wait $PID
rm /tmp/ionice-test
```

### Ejercicio 4 — nice + ionice combinados

```bash
#!/bin/bash
# Tarea de mantenimiento: máxima deferencia

nice -n 19 ionice -c 3 find /tmp -name "*.tmp" -mtime +7 -delete 2>/dev/null
echo "Archivos tmp limpiados"
```

### Ejercicio 5 — Ver prioridad de servicios

```bash
#!/bin/bash
# Ver nice de los servicios del sistema

for svc in sshd nginx cron; do
    pid=$(pgrep -x $svc 2>/dev/null | head -1)
    if [[ -n "$pid" ]]; then
        ni=$(ps -o ni= -p $pid | xargs)
        echo "$svc (PID $pid): nice=$ni"
    fi
done
```

### Ejercicio 6 — Probar que nice negativo requiere root

```bash
#!/bin/bash
# Intentar nice negativo como usuario regular

nice -n -5 sleep 100 &>/dev/null
echo "nice -5: exit=$?"

# Como root
sudo nice -n -5 sleep 100 &
sleep 0.1
PID=$!
ps -o ni= -p $PID
kill $PID 2>/dev/null
```

### Ejercicio 7 — ionice con BFQ vs mq-deadline

```bash
#!/bin/bash
# Ver qué scheduler tiene el disco principal

SCHEDULER=$(cat /sys/block/sda/queue/scheduler 2>/dev/null | grep -o '\[.*\]' | tr -d '[]')
echo "Scheduler de sda: $SCHEDULER"

if [[ "$SCHEDULER" == *"bfq"* ]]; then
    echo "BFQ: ionice tiene efecto completo"
elif [[ "$SCHEDULER" == *"none"* || "$SCHEDULER" == *"mq-deadline"* ]]; then
    echo "none/mq-deadline: ionice tiene efecto limitado"
fi
```

### Ejercicio 8 — Prioridad y load average

```bash
#!/bin/bash
# Demostrar que nice solo importa cuando hay contención

# Launch CPU-bound con nice 0
(while true; do :; done) &
PID1=$!

# Launch CPU-bound con nice 19
nice -n 19 (while true; do :; done) &
PID2=$!

sleep 2

# Ambos deberían consumir ~50% de un core cada uno
ps -o pid,ni,%cpu -p $PID1,$PID2

kill $PID1 $PID2
```

### Ejercicio 9 — renice a un process group

```bash
#!/bin/bash
# Crear un pipeline
sleep 100 | sleep 101 &
PGID=$(ps -o pgid= -p $(jobs -p | head -1) | tr -d ' ')

echo "PGID: $PGID"
echo "Procesos en el PG:"
ps -o pid,ni,cmd -g $PGID

# Bajar prioridad de todo el grupo
renice -n 15 -g $PGID

ps -o pid,ni,cmd -g $PGID

kill -- -$PGID
```

### Ejercicio 10 — ulimit -e (nice cap)

```bash
#!/bin/bash
# Ver el límite de nice para el usuario actual

echo "Nice cap (ulimit -e): $(ulimit -e)"

# El valor 0 = solo puede subir nice (0 → 19)
# El valor 5 = puede usar nice -5 a 19
# El valor 40 = puede usar nice -20 a 19 (sin restricciones)

if [[ $(ulimit -e) -eq 0 ]]; then
    echo "Solo puedes SUBIR nice (bajar prioridad)"
else
    echo "Puedes usar nice desde -$(ulimit -e) hasta 19"
fi
```
