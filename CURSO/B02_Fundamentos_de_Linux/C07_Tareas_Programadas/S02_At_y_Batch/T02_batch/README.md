# T02 — batch

## Qué es batch

`batch` ejecuta comandos cuando la **carga del sistema** cae por debajo de
un umbral. Es una variante de `at` que no programa por hora sino por carga:

```bash
# at    → "ejecutar a las 03:00"
# batch → "ejecutar cuando el sistema esté libre"

# Internamente, batch es equivalente a:
at -q b -f /dev/stdin
# Encola el trabajo en la cola "b" (batch) de atd
```

```bash
# Ejemplo:
echo "/usr/local/bin/heavy-report.sh" | batch
# job 20 at Tue Mar 17 15:30:00 2026
# Se ejecutará cuando el load average sea suficientemente bajo
```

## Load average — El concepto clave

```bash
# El load average indica cuántos procesos están esperando CPU + I/O:
uptime
# 15:30:00 up 5 days, load average: 0.45, 0.62, 0.71
#                                    1min  5min  15min

cat /proc/loadavg
# 0.45 0.62 0.71 2/345 12345
# 1min 5min 15min running/total lastPID

# Interpretación (en un sistema con N CPUs):
# load < N     → hay CPUs libres
# load = N     → CPUs al 100% sin cola
# load > N     → hay procesos esperando

# Ejemplo con 4 CPUs:
# load 0.5 → sistema casi ocioso
# load 4.0 → todas las CPUs ocupadas
# load 8.0 → 4 procesos esperando CPU
```

## El umbral de batch

```bash
# atd controla cuándo ejecutar los trabajos batch
# El umbral por defecto es load average 1.5 (promedio de 1 minuto)

# atd usa la opción -l para definir el umbral:
# atd -l 1.5    (default)

# Ver la configuración actual del daemon:
systemctl cat atd
# O:
ps aux | grep atd
# /usr/sbin/atd -f

# Cambiar el umbral — editar el servicio de atd:
sudo systemctl edit atd
# [Service]
# ExecStart=
# ExecStart=/usr/sbin/atd -f -l 2.0
# Ahora batch ejecuta cuando el load < 2.0
```

```bash
# El intervalo de verificación (batch interval):
# atd verifica el load average cada 60 segundos por defecto
# Se puede cambiar con -b:
# atd -l 1.5 -b 30    → verificar cada 30 segundos

# Configurar ambos:
sudo systemctl edit atd
# [Service]
# ExecStart=
# ExecStart=/usr/sbin/atd -f -l 2.0 -b 30
sudo systemctl restart atd
```

## Crear tareas con batch

### Modo interactivo

```bash
batch
# at> /usr/local/bin/generate-report.sh
# at> /usr/local/bin/compress-logs.sh
# at> <Ctrl+D>
# job 21 at Tue Mar 17 15:35:00 2026
```

### Desde stdin

```bash
# Con pipe:
echo "/usr/local/bin/heavy-task.sh" | batch

# Con heredoc:
batch << 'EOF'
/usr/local/bin/rebuild-indexes.sh
/usr/local/bin/cleanup-old-data.sh
logger "Tareas batch completadas"
EOF
```

### Desde un archivo

```bash
# batch no tiene -f directamente, pero se puede usar redirección:
batch < /path/to/tasks.sh

# O usar at con la cola batch:
at -q b -f /path/to/tasks.sh now
```

## Gestionar tareas batch

```bash
# Las tareas batch aparecen en la cola de at con la letra "b":
atq
# 20   Tue Mar 17 15:30:00 2026 b user    ← cola "b" = batch
# 7    Wed Mar 18 14:00:00 2026 a user    ← cola "a" = at normal

# Ver el contenido:
at -c 20

# Eliminar:
atrm 20

# batch usa la misma infraestructura que at:
# - Mismo daemon (atd)
# - Mismo spool (/var/spool/at/)
# - Mismos archivos de control de acceso (/etc/at.allow, /etc/at.deny)
# - Misma captura de entorno
```

## Comportamiento de la cola batch

```bash
# Cuando atd detecta que el load está por debajo del umbral:
# 1. Toma el trabajo batch más antiguo de la cola
# 2. Lo ejecuta
# 3. Espera el batch interval (default 60s)
# 4. Verifica de nuevo el load
# 5. Si sigue bajo, ejecuta el siguiente

# Los trabajos batch NO se ejecutan en paralelo entre sí
# Se ejecutan uno por uno, verificando el load entre cada uno
# Esto evita que la propia ejecución de tareas batch sature el sistema

# Si hay 5 trabajos batch y el load es bajo:
# t=0s:   ejecutar trabajo 1
# t=60s:  verificar load → bajo → ejecutar trabajo 2
# t=120s: verificar load → bajo → ejecutar trabajo 3
# t=180s: verificar load → ALTO → esperar
# t=240s: verificar load → bajo → ejecutar trabajo 4
# ...
```

## Casos de uso

### Tareas pesadas que no son urgentes

```bash
# Generar un informe que consume mucha CPU:
echo "/opt/analytics/generate-monthly-report.sh" | batch
# Se ejecutará cuando el servidor esté libre
# No impactará a los usuarios activos

# Comprimir logs antiguos:
echo "find /var/log -name '*.log.*' -not -name '*.gz' -exec gzip {} \;" | batch

# Reconstruir índices de base de datos:
batch << 'EOF'
psql -U postgres -d mydb -c "REINDEX DATABASE mydb;"
logger "Database reindex completado"
EOF
```

### Combinación con at

```bash
# Programar una tarea para la noche, pero solo si el sistema está libre:
# No se puede hacer directamente con batch (no acepta hora)
# Pero se puede combinar at + batch:

# Opción 1: at programa batch
at 02:00 << 'EOF'
echo "/usr/local/bin/heavy-task.sh" | batch
EOF
# A las 02:00, at encola la tarea en batch
# batch la ejecuta cuando el load baja

# Opción 2: usar at con la cola batch a una hora específica
# (no soportado — batch siempre es "ahora pero cuando haya carga baja")
```

### Procesamiento por lotes

```bash
# Encolar múltiples tareas independientes:
for report in sales inventory shipping; do
    echo "/opt/reports/generate.sh $report" | batch
done
# Se procesan una por una cuando hay capacidad

# Cada tarea se ejecuta en su propio trabajo batch
# Si una falla, las demás continúan
```

## Prioridad de CPU (nice)

```bash
# La cola "b" (batch) ejecuta con nice 2 por defecto:
# Esto significa una prioridad ligeramente menor que procesos normales

# Las colas se pueden usar para diferentes prioridades:
# Cola a → nice 0  (at normal)
# Cola b → nice 2  (batch)
# Cola c → nice 4
# ...
# Cola z → nice 50

# Tareas con prioridad muy baja:
at -q z now << 'EOF'
/usr/local/bin/low-priority-task.sh
EOF
# nice 50 → casi no compite por CPU

# Combinado con batch:
# No se puede cambiar la cola de batch (siempre es "b")
# Pero se puede usar nice dentro del script:
echo "nice -n 19 /usr/local/bin/heavy-task.sh" | batch
```

## Diferencias entre at y batch

| Aspecto | at | batch |
|---|---|---|
| Cuándo ejecuta | Hora especificada | Cuando load < umbral |
| Cola | a (por defecto) | b |
| Nice | 0 | 2 |
| Requiere hora | Sí | No |
| Ejecución paralela | Sí (entre diferentes jobs) | No (uno por uno) |
| Uso principal | Tareas programadas únicas | Tareas pesadas no urgentes |

## Limitaciones

```bash
# 1. batch no tiene timeout
#    Si un trabajo batch se cuelga, no se ejecutan los siguientes
#    Solución: añadir timeout al comando:
echo "timeout 3600 /usr/local/bin/heavy-task.sh" | batch
# Mata la tarea si tarda más de 1 hora

# 2. Sin notificación de progreso
#    No sabes cuándo se ejecutó ni si terminó bien
#    Solución: agregar logging:
batch << 'EOF'
logger "batch: starting heavy-task"
/usr/local/bin/heavy-task.sh
logger "batch: heavy-task finished with status $?"
EOF

# 3. Sin reintentos
#    Si la tarea falla, no se reintenta
#    Solución: lógica de retry en el script

# 4. El umbral de load es global
#    No distingue entre I/O wait y CPU
#    Un disco lento puede elevar el load sin saturar CPU

# 5. No persiste entre reboots (en algunos sistemas)
#    Los trabajos de at/batch se almacenan en /var/spool/at/
#    Si el spool está en tmpfs (raro), se pierden al reiniciar
```

## Alternativas modernas

```bash
# systemd tiene mecanismos equivalentes:

# Para at (ejecución única programada):
systemd-run --on-active=2h /usr/local/bin/task.sh
# Crea un timer transient que ejecuta en 2 horas

# Para batch (ejecución cuando hay recursos):
# No hay equivalente directo en systemd
# Pero se puede combinar:
# - CPUWeight= en el .service (prioridad de CPU)
# - IOWeight= (prioridad de I/O)
# - Condiciones (ConditionCPUs=, ConditionMemory=)

# nohup + nice como alternativa simple a batch:
nice -n 19 ionice -c 3 /usr/local/bin/heavy-task.sh &
# nice -n 19  → mínima prioridad de CPU
# ionice -c 3 → clase idle (solo I/O cuando nadie más necesita)
# &           → background
```

---

## Ejercicios

### Ejercicio 1 — Verificar at y batch

```bash
# 1. ¿Está atd activo?
systemctl is-active atd 2>/dev/null || echo "atd no activo"

# 2. ¿Cuál es el load average actual?
uptime

# 3. ¿Cuál es el umbral de batch configurado?
ps aux | grep "[a]td"
# Si no muestra -l, el default es 1.5
```

### Ejercicio 2 — Probar batch

```bash
# 1. Encolar una tarea batch:
echo 'echo "batch ejecutado: $(date), load: $(cat /proc/loadavg)" > /tmp/batch-test.txt' | batch

# 2. Verificar que está en la cola:
atq    # debería mostrar cola "b"

# 3. Esperar 1-2 minutos (si el load es bajo) y verificar:
# cat /tmp/batch-test.txt
```

### Ejercicio 3 — Comparar load y ejecución

```bash
# 1. Ver el load actual:
cat /proc/loadavg

# 2. ¿Se ejecutaría batch ahora? (comparar con umbral 1.5):
LOAD=$(awk '{print $1}' /proc/loadavg)
echo "Load: $LOAD"
awk "BEGIN {if ($LOAD < 1.5) print \"Sí, batch ejecutaría\"; else print \"No, load muy alto\"}"
```
