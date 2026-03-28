# T02 — batch

## Erratas detectadas en el material base

| Archivo | Línea(s) | Error | Corrección |
|---|---|---|---|
| README.md | 51, 87, 343, 345 | Afirma que el umbral por defecto de batch es **1.5** sin matizar por distro | En **Debian** el compile-time default es **0.8** (`man atd`: *"instead of the compile-time choice of 0.8"*). En **RHEL** sí es **1.5**. |

---

## Qué es batch

`batch` ejecuta comandos cuando la **carga del sistema** cae por debajo de un
umbral. No programa por hora como `at`, sino por carga:

```bash
# at    → "ejecutar a las 03:00"
# batch → "ejecutar cuando el sistema esté libre"

# Internamente, batch equivale a:
at -q b -f /dev/stdin
# Encola el trabajo en la cola "b" (batch) de atd
```

```bash
echo "/usr/local/bin/heavy-report.sh" | batch
# job 20 at Tue Mar 17 15:30:00 2026
# Se ejecutará cuando el load average sea suficientemente bajo
```

---

## Load average — el concepto clave

El load average indica cuántos procesos están **esperando CPU + I/O** en un
momento dado:

```bash
uptime
# 15:30:00 up 5 days, load average: 0.45, 0.62, 0.71
#                                    1min  5min  15min

cat /proc/loadavg
# 0.45 0.62 0.71 2/345 12345
# 1min 5min 15min running/total lastPID
```

### Interpretación (sistema con N CPUs)

| Load vs N CPUs | Significado |
|---|---|
| load < N | Hay CPUs libres |
| load = N | CPUs al 100% sin cola de espera |
| load > N | Hay procesos esperando CPU |

```bash
# Ejemplo con 4 CPUs:
# load 0.5 → sistema casi ocioso
# load 4.0 → todas las CPUs ocupadas
# load 8.0 → 4 procesos esperando CPU

# Ver cuántas CPUs tiene el sistema:
nproc
```

**Nota:** el load average incluye procesos en estado D (uninterruptible sleep,
típicamente I/O). Un disco lento puede elevar el load sin saturar la CPU.

---

## El umbral de batch

`atd` decide cuándo ejecutar los trabajos batch comparando el load average de
1 minuto contra un umbral configurable:

```bash
# El umbral por defecto depende de la distro:
#   Debian: 0.8  (compile-time default)
#   RHEL:   1.5  (compile-time default)

# Ver la configuración actual del daemon:
systemctl cat atd
# O:
ps aux | grep "[a]td"
# Si no muestra -l, se usa el default de la distro
```

### Cambiar el umbral

```bash
sudo systemctl edit atd
# [Service]
# ExecStart=
# ExecStart=/usr/sbin/atd -f -l 2.0

sudo systemctl restart atd
# Ahora batch ejecuta cuando el load < 2.0
```

### Batch interval

El intervalo con el que `atd` re-verifica el load (por defecto 60 segundos).
Se controla con `-b`:

```bash
sudo systemctl edit atd
# [Service]
# ExecStart=
# ExecStart=/usr/sbin/atd -f -l 2.0 -b 30
# -l 2.0  → umbral de load
# -b 30   → verificar cada 30 segundos

sudo systemctl restart atd
```

---

## Crear tareas con batch

### Modo interactivo

```bash
batch
# at> /usr/local/bin/generate-report.sh
# at> /usr/local/bin/compress-logs.sh
# at> <Ctrl+D>
# job 21 at Tue Mar 17 15:35:00 2026
```

### Desde stdin (pipe o heredoc)

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
# Con redirección:
batch < /path/to/tasks.sh

# O usando at con la cola batch:
at -q b -f /path/to/tasks.sh now
```

---

## Gestionar tareas batch

Las tareas batch aparecen en la cola de `at` con la letra **"b"**:

```bash
atq
# 20   Tue Mar 17 15:30:00 2026 b user    ← cola "b" = batch
# 7    Wed Mar 18 14:00:00 2026 a user    ← cola "a" = at normal

# Ver contenido de un trabajo:
at -c 20

# Eliminar un trabajo:
atrm 20
```

`batch` comparte **toda la infraestructura** de `at`:

| Recurso | Compartido |
|---|---|
| Daemon | `atd` |
| Spool | `/var/spool/at/` |
| Control de acceso | `/etc/at.allow`, `/etc/at.deny` |
| Captura de entorno | Sí (pwd, variables, umask) |

---

## Comportamiento de la cola batch

Los trabajos batch **no se ejecutan en paralelo** entre sí. Se procesan uno
por uno, verificando el load entre cada ejecución:

```
t=0s:   ejecutar trabajo 1
t=60s:  verificar load → bajo → ejecutar trabajo 2
t=120s: verificar load → bajo → ejecutar trabajo 3
t=180s: verificar load → ALTO → esperar
t=240s: verificar load → bajo → ejecutar trabajo 4
...
```

Este comportamiento **autorregulado** evita que las propias tareas batch
saturen el sistema.

---

## Prioridad de CPU (nice)

La cola "b" (batch) ejecuta con **nice 2** por defecto. Las colas de `at`
asignan nice según la letra: `nice = (cola - 'a') × 2`.

| Cola | Nice | Uso |
|---|---|---|
| a | 0 | `at` normal |
| b | 2 | `batch` |
| c | 4 | Baja prioridad |
| ... | ... | ... |
| z | clamped a 19 | Mínima prioridad |

**Nota:** Linux limita nice al rango -20 a 19. Colas altas (como `z`, cuyo
cálculo daría nice 50) se restringen al máximo permitido (19).

```bash
# Tarea con prioridad muy baja mediante cola:
at -q z now << 'EOF'
/usr/local/bin/low-priority-task.sh
EOF

# O combinando nice dentro del script batch:
echo "nice -n 19 /usr/local/bin/heavy-task.sh" | batch
```

**No se puede cambiar la cola de `batch`** — siempre usa "b". Para menor
prioridad, usar `nice` dentro del propio script.

---

## Casos de uso

### Tareas pesadas no urgentes

```bash
# Generar informe que consume mucha CPU:
echo "/opt/analytics/generate-monthly-report.sh" | batch

# Comprimir logs antiguos:
echo "find /var/log -name '*.log.*' -not -name '*.gz' -exec gzip {} \;" | batch

# Reconstruir índices de base de datos:
batch << 'EOF'
psql -U postgres -d mydb -c "REINDEX DATABASE mydb;"
logger "Database reindex completado"
EOF
```

### Procesamiento por lotes

```bash
# Encolar múltiples tareas independientes:
for report in sales inventory shipping; do
    echo "/opt/reports/generate.sh $report" | batch
done
# Se procesan una por una cuando hay capacidad
# Si una falla, las demás continúan
```

### Combinación at + batch (hora + load)

`batch` no acepta una hora de ejecución. Para combinar "a tal hora" con "si el
load es bajo", se anida `batch` dentro de `at`:

```bash
at 02:00 << 'EOF'
echo "/usr/local/bin/heavy-task.sh" | batch
EOF
# A las 02:00, at encola la tarea en batch
# batch la ejecuta cuando el load baja del umbral
```

---

## Diferencias entre at y batch

| Aspecto | `at` | `batch` |
|---|---|---|
| Cuándo ejecuta | Hora especificada | Cuando load < umbral |
| Cola | a (por defecto) | b |
| Nice | 0 | 2 |
| Requiere hora | Sí | No |
| Ejecución paralela | Sí (entre diferentes jobs) | No (uno por uno) |
| Uso principal | Tareas programadas únicas | Tareas pesadas no urgentes |

---

## Limitaciones

| Limitación | Problema | Solución |
|---|---|---|
| Sin timeout | Si un trabajo se cuelga, bloquea los siguientes | `echo "timeout 3600 /path/to/task.sh" \| batch` |
| Sin notificación | No sabes cuándo se ejecutó ni si terminó | Añadir `logger` antes y después |
| Sin reintentos | Si falla, no se reintenta | Lógica de retry en el script |
| Umbral global | No distingue I/O wait de CPU | Disco lento eleva load sin saturar CPU |
| Sin hora + load | `batch` siempre es "ahora cuando haya carga baja" | Anidar `batch` dentro de `at` |

---

## Alternativas modernas

```bash
# nice + ionice — alternativa simple a batch:
nice -n 19 ionice -c 3 /usr/local/bin/heavy-task.sh &
# nice -n 19  → mínima prioridad de CPU
# ionice -c 3 → clase idle (solo I/O cuando nadie más necesita)
# &           → background

# nohup para tareas largas que sobrevivan al cierre de sesión:
nohup nice -n 19 /path/to/task.sh > /var/log/task.log 2>&1 &
```

```bash
# systemd — control granular de recursos:

# Ejecución única (equivalente a at):
systemd-run --on-active=2h /usr/local/bin/task.sh

# No hay equivalente directo de batch en systemd, pero se puede
# controlar recursos en un .service con:
#   CPUWeight=    prioridad de CPU
#   IOWeight=     prioridad de I/O
#   ConditionCPUs=, ConditionMemory=
```

---

## Laboratorio

### Parte 1 — Load average y umbral

#### Paso 1.1: Consultar el load average

```bash
docker compose exec debian-dev bash -c '
echo "=== Load average ==="
echo ""
echo "--- uptime ---"
uptime
echo ""
echo "--- /proc/loadavg ---"
cat /proc/loadavg
echo "  Campos: 1min 5min 15min running/total lastPID"
echo ""
NCPU=$(nproc 2>/dev/null || echo "?")
LOAD=$(awk "{print \$1}" /proc/loadavg)
echo "Este sistema: $NCPU CPUs, load 1min: $LOAD"
echo ""
echo "--- Interpretacion ---"
echo "load < N (CPUs)  →  CPUs libres"
echo "load = N         →  CPUs al 100%"
echo "load > N         →  procesos esperando"
'
```

#### Paso 1.2: El umbral de batch

```bash
docker compose exec debian-dev bash -c '
echo "=== Umbral de batch ==="
echo ""
echo "atd controla cuando ejecutar trabajos batch:"
echo "  atd -l UMBRAL     load maximo para ejecutar"
echo "  atd -b INTERVALO  segundos entre verificaciones (default: 60)"
echo ""
echo "Defaults compilados:"
echo "  Debian: 0.8"
echo "  RHEL:   1.5"
echo ""
echo "--- Configuracion actual ---"
ps aux 2>/dev/null | grep "[a]td" || \
    echo "atd no corriendo (normal en contenedores)"
echo ""
echo "--- Cambiar umbral y batch interval ---"
echo "sudo systemctl edit atd"
echo "[Service]"
echo "ExecStart="
echo "ExecStart=/usr/sbin/atd -f -l 2.0 -b 30"
echo ""
echo "--- Verificar si batch ejecutaria ahora ---"
LOAD=$(awk "{print \$1}" /proc/loadavg)
echo "Load actual: $LOAD"
echo "Con umbral Debian (0.8):"
awk -v load="$LOAD" "BEGIN {
    if (load < 0.8) print \"  batch EJECUTARIA (load < 0.8)\"
    else print \"  batch ESPERARIA (load >= 0.8)\"
}"
echo "Con umbral RHEL (1.5):"
awk -v load="$LOAD" "BEGIN {
    if (load < 1.5) print \"  batch EJECUTARIA (load < 1.5)\"
    else print \"  batch ESPERARIA (load >= 1.5)\"
}"
'
```

---

### Parte 2 — Crear y gestionar tareas

#### Paso 2.1: Crear tareas con batch

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
echo "  O equivalente: at -q b -f /path/to/tasks.sh now"
echo ""
echo "--- Internamente ---"
echo "batch equivale a: at -q b -f /dev/stdin"
echo "Encola el trabajo en la cola \"b\" (batch) de atd"
'
```

#### Paso 2.2: Gestión en la cola

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
echo "  at -c 20      ver contenido del trabajo"
echo "  atrm 20       eliminar trabajo"
echo ""
echo "batch comparte toda la infraestructura de at:"
echo "  Daemon:           atd"
echo "  Spool:            /var/spool/at/"
echo "  Control acceso:   at.allow / at.deny"
echo "  Captura entorno:  Si (pwd, variables, umask)"
'
```

#### Paso 2.3: Ejecución uno por uno

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
printf "%-20s %-20s %-20s\n" "Aspecto" "at" "batch"
printf "%-20s %-20s %-20s\n" "Cuando ejecuta" "Hora especificada" "Cuando load < umbral"
printf "%-20s %-20s %-20s\n" "Cola" "a (default)" "b"
printf "%-20s %-20s %-20s\n" "Nice" "0" "2"
printf "%-20s %-20s %-20s\n" "Requiere hora" "Si" "No"
printf "%-20s %-20s %-20s\n" "Paralelo" "Si (entre jobs)" "No (uno por uno)"
'
```

---

### Parte 3 — Limitaciones y alternativas

#### Paso 3.1: Limitaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Limitaciones de batch ==="
echo ""
echo "1. Sin timeout"
echo "   Si un trabajo se cuelga, bloquea los siguientes"
echo "   Solucion: timeout 3600 /path/to/heavy-task.sh"
echo ""
echo "2. Sin notificacion de progreso"
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
echo "   Para hora + load: anidar batch dentro de at"
echo "     at 02:00 << EOF"
echo "     echo /path/to/task.sh | batch"
echo "     EOF"
'
```

#### Paso 3.2: Alternativas modernas

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
echo "--- nohup para tareas largas ---"
echo "nohup nice -n 19 /path/to/task.sh > /var/log/task.log 2>&1 &"
echo "  Sobrevive al cierre de sesion"
echo ""
echo "--- systemd-run ---"
echo "Para at (ejecucion unica):"
echo "  systemd-run --on-active=2h /usr/local/bin/task.sh"
echo ""
echo "Para batch (control de recursos, sin equivalente directo):"
echo "  Usar CPUWeight= e IOWeight= en un .service"
'
```

#### Paso 3.3: Casos de uso

```bash
docker compose exec debian-dev bash -c '
echo "=== Casos de uso de batch ==="
echo ""
echo "--- Tareas pesadas no urgentes ---"
echo "echo \"/opt/analytics/generate-monthly-report.sh\" | batch"
echo ""
echo "--- Comprimir logs ---"
echo "echo \"find /var/log -name *.log.* -not -name *.gz -exec gzip {} \\;\" | batch"
echo ""
echo "--- Procesamiento por lotes ---"
echo "for report in sales inventory shipping; do"
echo "    echo \"/opt/reports/generate.sh \$report\" | batch"
echo "done"
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

## Ejercicios

### Ejercicio 1 — batch como variante de at

¿Qué comando interno ejecuta `batch` por debajo? ¿En qué cola se encolan sus
trabajos?

<details><summary>Predicción</summary>

`batch` equivale internamente a `at -q b -f /dev/stdin`. Encola los trabajos
en la cola **"b"** (batch) de `atd`.

</details>

### Ejercicio 2 — Load average

Un sistema con **4 CPUs** muestra load average `5.2, 3.1, 2.8`. ¿Qué indica
el valor de 1 minuto (5.2) respecto al número de CPUs?

<details><summary>Predicción</summary>

Load 5.2 con 4 CPUs significa que hay **1.2 procesos** (en promedio) esperando
CPU en cada instante. El sistema está **sobrecargado** a corto plazo
(5.2 > 4), aunque la tendencia de 5 y 15 minutos (3.1, 2.8) muestra que la
carga estaba bajando.

</details>

### Ejercicio 3 — Umbral por defecto

¿Cuál es el umbral por defecto de `batch` en Debian y en RHEL? ¿Cómo se
verifica cuál está configurado en un sistema concreto?

<details><summary>Predicción</summary>

- **Debian:** 0.8 (compile-time default)
- **RHEL:** 1.5 (compile-time default)

Se verifica con `ps aux | grep "[a]td"` — si no aparece el flag `-l`, se
está usando el default de la distro. También se puede consultar
`systemctl cat atd` para ver la línea `ExecStart`.

</details>

### Ejercicio 4 — Cambiar umbral y batch interval

Un servidor tiene 8 CPUs y quieres que `batch` ejecute cuando el load sea
menor que 4.0, verificando cada 20 segundos. ¿Qué comandos usarías?

<details><summary>Predicción</summary>

```bash
sudo systemctl edit atd
# Añadir:
# [Service]
# ExecStart=
# ExecStart=/usr/sbin/atd -f -l 4.0 -b 20

sudo systemctl restart atd
```

La línea `ExecStart=` vacía es necesaria para limpiar la directiva anterior
antes de definir la nueva (requisito de systemd para drop-in overrides).

</details>

### Ejercicio 5 — Crear una tarea batch con logging

Escribe un comando que encole en batch una tarea que: (1) registre el inicio
con `logger`, (2) ejecute `/usr/local/bin/process-data.sh` con timeout de 30
minutos, y (3) registre el resultado.

<details><summary>Predicción</summary>

```bash
batch << 'EOF'
logger "batch: starting process-data"
timeout 1800 /usr/local/bin/process-data.sh
logger "batch: process-data finished with status $?"
EOF
```

El `timeout 1800` mata el proceso si tarda más de 30 minutos, evitando que
bloquee la cola batch. Los `logger` permiten verificar en syslog/journal
cuándo se ejecutó y si terminó correctamente.

</details>

### Ejercicio 6 — Identificar tareas batch en atq

En la siguiente salida de `atq`, ¿cuáles son tareas batch y cuáles son `at`?

```
12   Tue Mar 17 15:30:00 2026 b user
 7   Wed Mar 18 14:00:00 2026 a user
15   Tue Mar 17 16:00:00 2026 b user
 9   Thu Mar 19 03:00:00 2026 a user
```

<details><summary>Predicción</summary>

- **Batch (cola "b"):** trabajos 12 y 15
- **At normal (cola "a"):** trabajos 7 y 9

La letra antes del usuario indica la cola. "b" = batch (se ejecuta cuando el
load baja del umbral). "a" = at (se ejecuta a la hora programada).

</details>

### Ejercicio 7 — Ejecución secuencial

Si hay 3 trabajos batch en cola y el batch interval es 60 segundos, ¿cuánto
tarda como **mínimo** en ejecutarse el tercer trabajo (asumiendo load siempre
bajo y ejecución instantánea de cada tarea)?

<details><summary>Predicción</summary>

**120 segundos** como mínimo:

```
t=0s:   ejecutar trabajo 1 (instantáneo)
t=60s:  verificar load → bajo → ejecutar trabajo 2 (instantáneo)
t=120s: verificar load → bajo → ejecutar trabajo 3
```

El tercer trabajo no empieza hasta `t=120s` porque `atd` verifica el load
cada 60 segundos entre ejecuciones, incluso si la carga es baja.

</details>

### Ejercicio 8 — Nice y colas

¿Cuál es el nice de la cola "b"? Si se usa `at -q z now`, ¿cuál es el nice
efectivo y por qué no coincide con el cálculo teórico `(z - a) × 2 = 50`?

<details><summary>Predicción</summary>

- Cola "b" → nice **2** (fórmula: `(1) × 2 = 2`)
- Cola "z" → cálculo teórico: `(25) × 2 = 50`, pero el nice efectivo es
  **19** porque Linux limita los valores de nice al rango **-20 a 19**. El
  sistema hace clamping al máximo permitido.

</details>

### Ejercicio 9 — Combinación at + batch

¿Por qué `batch` no acepta una hora como argumento? Si necesitas ejecutar una
tarea pesada "mañana a las 02:00 solo si el load es bajo", ¿cómo lo resuelves?

<details><summary>Predicción</summary>

`batch` no acepta hora porque su propósito es distinto: ejecutar **ahora**
pero solo cuando el sistema tenga carga baja. Para combinar hora + carga baja,
se anida `batch` dentro de `at`:

```bash
at 02:00 tomorrow << 'EOF'
echo "/usr/local/bin/heavy-task.sh" | batch
EOF
```

A las 02:00, `at` se ejecuta y encola la tarea en `batch`. Luego `batch` la
ejecuta cuando el load caiga por debajo del umbral.

</details>

### Ejercicio 10 — batch vs nice + ionice

¿Qué diferencia fundamental hay entre `batch` y ejecutar un comando con
`nice -n 19 ionice -c 3 ... &`? ¿Cuándo preferirías cada uno?

<details><summary>Predicción</summary>

| Aspecto | `batch` | `nice + ionice` |
|---|---|---|
| Espera carga baja | Sí — no arranca hasta que load < umbral | No — arranca inmediatamente |
| Prioridad | Nice 2 (cola b) | Nice 19 + I/O idle |
| Impacto en sistema | Nulo mientras espera; moderado al ejecutar | Inmediato pero con prioridad mínima |
| Cola secuencial | Sí — uno por uno | No — corre junto a todo lo demás |

**Usar `batch`** cuando no quieres que la tarea compita con carga existente
(espera a que el sistema esté libre). **Usar `nice + ionice`** cuando quieres
que la tarea empiece ya pero sin molestar a los demás procesos. `nice + ionice`
también funciona como usuario normal sin necesidad de `atd`.

</details>
