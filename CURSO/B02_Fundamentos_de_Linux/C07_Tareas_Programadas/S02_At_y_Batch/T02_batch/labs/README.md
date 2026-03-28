# Lab — batch

## Objetivo

Entender batch: ejecucion cuando la carga del sistema es baja,
el concepto de load average, el umbral de atd (-l) y el batch
interval (-b), crear tareas con batch, el comportamiento de la
cola (ejecucion uno por uno), prioridad nice de la cola "b",
diferencias con at, limitaciones (sin timeout, sin reintentos),
y alternativas modernas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Load average y umbral

### Objetivo

Entender el load average y como batch decide cuando ejecutar.

### Paso 1.1: Load average

```bash
docker compose exec debian-dev bash -c '
echo "=== Load average ==="
echo ""
echo "Indica cuantos procesos estan esperando CPU + I/O:"
echo ""
echo "--- uptime ---"
uptime
echo ""
echo "--- /proc/loadavg ---"
cat /proc/loadavg
echo "  Campos: 1min 5min 15min running/total lastPID"
echo ""
echo "--- Interpretacion (N CPUs) ---"
echo "load < N     hay CPUs libres"
echo "load = N     CPUs al 100% sin cola"
echo "load > N     hay procesos esperando"
echo ""
NCPU=$(nproc 2>/dev/null || echo "?")
LOAD=$(awk "{print \$1}" /proc/loadavg)
echo "Este sistema: $NCPU CPUs, load 1min: $LOAD"
echo ""
if command -v awk &>/dev/null && [[ "$NCPU" != "?" ]]; then
    awk -v load="$LOAD" -v ncpu="$NCPU" "BEGIN {
        if (load < ncpu) print \"Estado: CPUs libres\"
        else if (load == ncpu) print \"Estado: CPUs al 100%\"
        else print \"Estado: procesos esperando\"
    }"
fi
'
```

### Paso 1.2: El umbral de batch

```bash
docker compose exec debian-dev bash -c '
echo "=== Umbral de batch ==="
echo ""
echo "atd controla cuando ejecutar trabajos batch:"
echo ""
echo "  atd -l UMBRAL    load maximo para ejecutar (default: 1.5)"
echo "  atd -b INTERVALO  segundos entre verificaciones (default: 60)"
echo ""
echo "batch ejecuta cuando el load average (1 min) < umbral"
echo ""
echo "--- Configuracion actual ---"
ps aux 2>/dev/null | grep "[a]td" || \
    echo "atd no corriendo (normal en contenedores)"
echo ""
echo "--- Cambiar el umbral ---"
echo "sudo systemctl edit atd"
echo "[Service]"
echo "ExecStart="
echo "ExecStart=/usr/sbin/atd -f -l 2.0 -b 30"
echo ""
echo "  -l 2.0  ejecutar cuando load < 2.0"
echo "  -b 30   verificar cada 30 segundos"
echo ""
echo "--- Verificar si batch ejecutaria ahora ---"
LOAD=$(awk "{print \$1}" /proc/loadavg)
echo "Load actual: $LOAD, umbral default: 1.5"
awk -v load="$LOAD" "BEGIN {
    if (load < 1.5) print \"batch EJECUTARIA (load < 1.5)\"
    else print \"batch ESPERARIA (load >= 1.5)\"
}"
'
```

---

## Parte 2 — Crear y gestionar tareas

### Objetivo

Crear tareas con batch y entender el comportamiento de la cola.

### Paso 2.1: Crear tareas con batch

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear tareas con batch ==="
echo ""
echo "--- Desde pipe ---"
echo "echo \"/usr/local/bin/heavy-report.sh\" | batch"
echo ""
echo "--- Desde heredoc ---"
echo "batch << '\''EOF'\''"
echo "/usr/local/bin/rebuild-indexes.sh"
echo "/usr/local/bin/cleanup-old-data.sh"
echo "logger \"Tareas batch completadas\""
echo "EOF"
echo ""
echo "--- Desde archivo ---"
echo "batch < /path/to/tasks.sh"
echo "  O: at -q b -f /path/to/tasks.sh now"
echo ""
echo "--- Internamente ---"
echo "batch equivale a: at -q b -f /dev/stdin"
echo "Encola el trabajo en la cola \"b\" (batch) de atd"
'
```

### Paso 2.2: Gestion en la cola

```bash
docker compose exec debian-dev bash -c '
echo "=== Tareas batch en la cola ==="
echo ""
echo "Las tareas batch aparecen en atq con la letra \"b\":"
echo ""
echo "  atq"
echo "  20  Tue Mar 17 15:30:00 2026 b user    ← batch (cola b)"
echo "   7  Wed Mar 18 14:00:00 2026 a user    ← at normal (cola a)"
echo ""
echo "Mismos comandos de gestion que at:"
echo "  atq           listar"
echo "  at -c 20      ver contenido"
echo "  atrm 20       eliminar"
echo ""
echo "batch usa la misma infraestructura:"
echo "  Mismo daemon (atd)"
echo "  Mismo spool (/var/spool/at/)"
echo "  Mismo control de acceso (at.allow/at.deny)"
echo "  Misma captura de entorno"
'
```

### Paso 2.3: Ejecucion uno por uno

```bash
docker compose exec debian-dev bash -c '
echo "=== Comportamiento de la cola batch ==="
echo ""
echo "Los trabajos batch NO se ejecutan en paralelo."
echo "Se ejecutan uno por uno, verificando load entre cada uno:"
echo ""
echo "  t=0s:   ejecutar trabajo 1"
echo "  t=60s:  verificar load → bajo → ejecutar trabajo 2"
echo "  t=120s: verificar load → bajo → ejecutar trabajo 3"
echo "  t=180s: verificar load → ALTO → esperar"
echo "  t=240s: verificar load → bajo → ejecutar trabajo 4"
echo ""
echo "Esto evita que las propias tareas batch saturen el sistema."
echo ""
echo "--- Diferencias con at ---"
echo "| Aspecto            | at                | batch              |"
echo "|--------------------|-------------------|--------------------|"
echo "| Cuando ejecuta     | Hora especificada | Cuando load < umbral|"
echo "| Cola               | a (default)       | b                  |"
echo "| Nice               | 0                 | 2                  |"
echo "| Requiere hora      | Si                | No                 |"
echo "| Ejecucion paralela | Si (entre jobs)   | No (uno por uno)   |"
echo "| Uso principal      | Tareas programadas| Tareas pesadas     |"
'
```

---

## Parte 3 — Limitaciones y alternativas

### Objetivo

Conocer las limitaciones de batch y las alternativas modernas.

### Paso 3.1: Limitaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Limitaciones de batch ==="
echo ""
echo "1. Sin timeout"
echo "   Si un trabajo se cuelga, bloquea los siguientes"
echo "   Solucion: timeout 3600 /path/to/heavy-task.sh"
echo ""
echo "2. Sin notificacion de progreso"
echo "   No sabes cuando se ejecuto ni si termino bien"
echo "   Solucion: agregar logging"
echo "     logger \"batch: starting heavy-task\""
echo "     /path/to/heavy-task.sh"
echo "     logger \"batch: finished with status \$?\""
echo ""
echo "3. Sin reintentos"
echo "   Si falla, no se reintenta"
echo "   Solucion: logica de retry en el script"
echo ""
echo "4. Umbral de load es global"
echo "   No distingue I/O wait de CPU"
echo "   Un disco lento eleva el load sin saturar CPU"
echo ""
echo "5. No se puede combinar hora + load"
echo "   batch siempre es \"ahora, cuando haya carga baja\""
echo "   Para hora + load: at programa batch"
echo "     at 02:00 << EOF"
echo "     echo /path/to/task.sh | batch"
echo "     EOF"
'
```

### Paso 3.2: Alternativas modernas

```bash
docker compose exec debian-dev bash -c '
echo "=== Alternativas a batch ==="
echo ""
echo "--- nice + ionice (alternativa simple) ---"
echo "nice -n 19 ionice -c 3 /path/to/heavy-task.sh &"
echo "  nice -n 19   minima prioridad de CPU"
echo "  ionice -c 3  clase idle (solo I/O cuando nadie mas necesita)"
echo "  &            background"
echo ""
echo "--- systemd-run ---"
echo "Para at (ejecucion unica programada):"
echo "  systemd-run --on-active=2h /usr/local/bin/task.sh"
echo ""
echo "Para batch (control de recursos):"
echo "  No hay equivalente directo, pero se puede combinar:"
echo "  CPUWeight= en el .service (prioridad de CPU)"
echo "  IOWeight= (prioridad de I/O)"
echo ""
echo "--- nohup para tareas largas ---"
echo "nohup nice -n 19 /path/to/task.sh > /var/log/task.log 2>&1 &"
echo "  Sobrevive al cierre de sesion"
echo "  Baja prioridad"
echo "  Log capturado"
'
```

### Paso 3.3: Casos de uso de batch

```bash
docker compose exec debian-dev bash -c '
echo "=== Casos de uso de batch ==="
echo ""
echo "--- Tareas pesadas no urgentes ---"
echo "echo \"/opt/analytics/generate-monthly-report.sh\" | batch"
echo "  Se ejecuta cuando el servidor este libre"
echo ""
echo "--- Comprimir logs ---"
echo "echo \"find /var/log -name '\''*.log.*'\'' -not -name '\''*.gz'\'' -exec gzip {} \\;\" | batch"
echo ""
echo "--- Reconstruir indices ---"
echo "echo \"psql -U postgres -d mydb -c '\''REINDEX DATABASE mydb;'\''\" | batch"
echo ""
echo "--- Procesamiento por lotes ---"
echo "for report in sales inventory shipping; do"
echo "    echo \"/opt/reports/generate.sh \$report\" | batch"
echo "done"
echo "  Cada tarea se procesa una por una cuando hay capacidad"
echo ""
echo "--- Combinacion at + batch ---"
echo "at 02:00 << EOF"
echo "echo /usr/local/bin/heavy-task.sh | batch"
echo "EOF"
echo "  A las 02:00, at encola la tarea en batch"
echo "  batch la ejecuta cuando el load baja"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `batch` ejecuta comandos cuando el load average cae por debajo
   del umbral (default 1.5). Internamente es `at -q b`.

2. El load average indica procesos esperando CPU + I/O. En un
   sistema con N CPUs, load < N significa CPUs libres.

3. atd controla el umbral (-l) y el intervalo de verificacion (-b).
   Los trabajos batch se ejecutan uno por uno, verificando load
   entre cada uno.

4. batch no tiene timeout, reintentos ni notificacion de progreso.
   Usar `timeout`, `logger` y logica de retry en los scripts.

5. La cola "b" ejecuta con nice 2 (prioridad ligeramente menor).
   Las mismas herramientas de gestion que at: atq, at -c, atrm.

6. Alternativas modernas: `nice -n 19 ionice -c 3` para baja
   prioridad, `systemd-run` para ejecucion unica con control
   de recursos.
