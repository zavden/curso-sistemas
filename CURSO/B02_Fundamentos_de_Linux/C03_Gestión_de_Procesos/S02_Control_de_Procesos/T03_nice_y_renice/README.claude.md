# T03 — nice y renice

## Errata y notas sobre el material original

1. **max.md ejercicio 8 — syntax error**:
   `nice -n 19 (while true; do :; done) &` es un error de sintaxis.
   `nice` necesita un comando ejecutable, no una subshell. Correcto:
   `nice -n 19 bash -c 'while true; do :; done' &`

2. **max.md ejercicio 8 — "ambos ~50%"**: Dice que nice 0 y nice 19
   "deberían consumir ~50% cada uno". Incorrecto: nice 0 vs nice 19
   da ~99%/1% según los pesos CFS (1024 vs 15). En multi-core sin
   contención, ambos pueden obtener 100% de un core separado — nice
   no tiene efecto sin contención.

3. **max.md ejercicio 9 — pipeline**: `sleep 100 | sleep 101` — sleep
   no lee stdin ni escribe stdout. Un pipe entre sleeps no demuestra
   nada. Reescrito con proceso útil.

4. **max.md ejercicio 10 — ulimit -e interpretación**: Dice "valor 5 =
   puede usar nice -5". Incorrecto. `ulimit -e` devuelve el
   RLIMIT_NICE. El mínimo nice permitido es `20 - ulimit_e`. Con
   ulimit_e=5, el mínimo nice es 15 (ni siquiera 0). Se necesita
   ulimit_e > 20 para usar nice negativo (ej: 25 → nice -5, 40 →
   nice -20).

5. **labs paso 2.1 — renice sin -n**: `renice 10 -p $PID` usa la
   sintaxis vieja (sin `-n`). Funciona pero está deprecated. La
   forma moderna es `renice -n 10 -p $PID`.

---

## Prioridades de procesos

Linux usa el scheduler **CFS** (Completely Fair Scheduler) para
distribuir tiempo de CPU entre procesos. Cada proceso tiene una
prioridad que influye en cuánto tiempo de CPU recibe **en relación
con otros procesos**:

```
Dos conceptos relacionados pero diferentes:

nice value:  -20 ←───────── 0 ──────────→ +19
              alta prioridad  default      baja prioridad
              (más CPU)                    (menos CPU)

PR (priority): lo que muestra top
              PR = 20 + nice
              PR va de 0 a 39 para procesos normales
              PR 20 = nice 0 (default)
              PR = rt → proceso real-time (no usa nice)
```

### Qué hace realmente el nice value

El nice value **no es un porcentaje de CPU**. Es un **peso relativo**.
CFS asigna tiempo proporcional al peso:

```
nice  0 → peso 1024
nice  1 → peso 820  (×0.8 aprox)
nice -1 → peso 1277 (×1.25 aprox)
nice 10 → peso 110  (×0.1 aprox)
nice 19 → peso 15   (×0.015 aprox)

Si hay dos procesos compitiendo por CPU en el mismo core:
  nice 0 vs nice 0   → 50%/50%
  nice 0 vs nice 10  → 91%/9%
  nice 0 vs nice 19  → 99%/1%
  nice -5 vs nice 0  → 75%/25%
```

| Nice | Peso CFS | % CPU (vs nice 0, 2 procesos) |
|---|---|---|
| -20 | 88761 | 98.9% |
| -10 | 9548 | 90.3% |
| -5 | 3121 | 75.3% |
| 0 | 1024 | 50.0% |
| 5 | 335 | 24.7% |
| 10 | 110 | 9.7% |
| 15 | 36 | 3.4% |
| 19 | 15 | 1.4% |

> **Clave**: El nice value solo importa **cuando hay contención**. Si
> el sistema tiene CPU libre, un proceso con nice 19 recibe todo el
> CPU que necesite. Nice solo afecta la distribución cuando hay
> procesos compitiendo por el mismo recurso.

### nice se hereda

Los procesos hijos heredan el nice value del padre:

```bash
nice -n 10 bash -c 'nice; sleep 100 & ps -o pid,ni -p $!'
# nice: 10
# El sleep hereda nice 10 del bash padre
```

---

## nice — Iniciar un proceso con prioridad modificada

```bash
# Sintaxis
nice [-n ajuste] comando [argumentos]

# Iniciar con nice 10 (prioridad baja)
nice -n 10 make -j4

# Nice 19 (mínima prioridad)
nice -n 19 find / -name "*.log" -mtime +30

# Sin argumento -n, nice usa 10 por defecto
nice make -j4    # equivale a nice -n 10

# Nice negativo (alta prioridad) — requiere root
sudo nice -n -5 /usr/sbin/critical-service

# nice sin comando muestra el nice actual
nice     # → 0 (default)
```

### Quién puede hacer qué

```bash
# Usuarios regulares:
# - Pueden SUBIR nice (bajar prioridad): 0 → 1..19
# - NO pueden BAJAR nice (subir prioridad): 0 → -1..-20
# - NO pueden volver atrás: si subieron a 10, no pueden volver a 0
#   (es IRREVERSIBLE sin root)

nice -n 10 sleep 100 &    # OK
nice -n -5 sleep 100 &    # nice: cannot set niceness: Permission denied

# root:
# - Puede cualquier valor de -20 a 19
# - Puede bajar nice de procesos de cualquier usuario
sudo nice -n -10 sleep 100 &    # OK
```

### Límite configurable

```bash
# El límite de nice para usuarios se configura en limits.conf
# /etc/security/limits.conf:
# dev   hard  nice  -5
# @audio  -   nice  -10

# ulimit -e muestra el RLIMIT_NICE actual
ulimit -e
# El mínimo nice permitido = 20 - ulimit_e
# ulimit_e = 0  → mínimo nice = 20 (solo puede subir, default)
# ulimit_e = 25 → mínimo nice = -5
# ulimit_e = 40 → mínimo nice = -20 (sin restricciones)
```

---

## renice — Cambiar prioridad de un proceso en ejecución

```bash
# Sintaxis
renice -n nice_value [-p PID] [-g PGRP] [-u user]

# Cambiar nice de un proceso (valor ABSOLUTO, no relativo)
renice -n 10 -p 1234

# Cambiar nice de todos los procesos de un usuario
renice -n 15 -u dev

# Cambiar nice de un process group
renice -n 5 -g 1234
```

> **Importante**: `renice -n 10` establece el nice a 10 (absoluto),
> no "suma 10" al nice actual.

### Restricciones de renice

```bash
# Usuarios regulares:
# - Solo pueden subir nice de SUS propios procesos
renice -n 15 -p 1234     # OK si es del usuario y nice actual < 15
renice -n 5 -p 1234      # FALLA si nice actual es 10 (no puede bajar)
renice -n 10 -p 9999     # FALLA si 9999 no es del usuario

# root:
# - Puede cualquier valor, cualquier proceso
sudo renice -n -20 -p 1234    # máxima prioridad
sudo renice -n 19 -u dev      # bajar prioridad de todo lo de dev
```

---

## Ver prioridades

```bash
# Con ps
ps -eo pid,ni,pri,cmd --sort=ni | head
# PID   NI  PRI  CMD
# 842  -10   30  /usr/sbin/irqbalance
# 1     0    20  /usr/lib/systemd/systemd

# Con top (columnas PR y NI)
top -bn1 | head -12

# Con /proc (campo 19 de stat es nice)
awk '{print $19}' /proc/$$/stat

# Nice de tu shell
nice
```

---

## ionice — Prioridad de I/O

`nice` solo afecta la **CPU**. Para controlar la prioridad de acceso
a disco, existe `ionice`:

```bash
# Tres clases de I/O scheduling:
# 1 = Realtime   — acceso prioritario (solo root, puede causar starvation)
# 2 = Best-effort — default, 8 niveles (0-7, menor = más prioridad)
# 3 = Idle       — solo usa I/O cuando nadie más lo necesita

# Ver la clase actual de un proceso
ionice -p $$
# best-effort: prio 4

# Iniciar con prioridad idle de I/O
ionice -c 3 tar czf backup.tar.gz /srv/data

# Iniciar con best-effort nivel 7
ionice -c 2 -n 7 updatedb

# Cambiar un proceso existente
ionice -c 3 -p 1234

# Combinar nice e ionice para tareas de mantenimiento
nice -n 19 ionice -c 3 find / -name "*.tmp" -delete
```

| Clase | Flag -c | Niveles | Descripción |
|---|---|---|---|
| Realtime | -c 1 | 0-7 | Acceso prioritario absoluto (solo root) |
| Best-effort | -c 2 | 0-7 | Default, 0=máxima, 7=mínima |
| Idle | -c 3 | (ninguno) | Solo cuando nadie más usa disco |

### Schedulers de I/O y ionice

```bash
# ionice depende del scheduler de I/O del kernel
# Ver el scheduler actual de un disco
cat /sys/block/sda/queue/scheduler
# [mq-deadline] kyber bfq none

# BFQ: respeta ionice completamente
# mq-deadline: respeta parcialmente
# none: ignora ionice (típico en NVMe)
# En SSDs con none/mq-deadline, ionice tiene poco efecto
```

---

## nice en systemd

Los servicios systemd establecen nice en su unit file:

```bash
# En el unit file del servicio:
# [Service]
# Nice=10
# IOSchedulingClass=idle
# IOSchedulingPriority=7

# Ver la configuración de un servicio
systemctl show nginx | grep -E 'Nice|IOScheduling'

# Cambiar temporalmente (hasta reinicio del servicio)
sudo systemctl set-property nginx Nice=10

# Cambiar permanentemente (override file)
sudo systemctl edit nginx
# [Service]
# Nice=10
```

---

## nice y cgroups (contenedores)

En contenedores Docker/Kubernetes, los **cgroup CPU weights** pueden
sobreescribir el efecto de nice:

```bash
# Docker: --cpu-shares controla el peso relativo
docker run --cpu-shares=512 myapp    # peso 512 (default=1024)

# El cgroup weight tiene prioridad sobre nice
# Un proceso con nice -20 dentro de un contenedor con cpu-shares=100
# aún recibe menos CPU que un proceso con nice 19 en un contenedor
# con cpu-shares=2048
```

---

## Cuándo usar nice/renice

| Tarea | Nice sugerido | Razón |
|---|---|---|
| Compilación (`make -j`) | 10-15 | No debe afectar servicios interactivos |
| Backups (`tar`, `rsync`) | 15-19 | Puede esperar, no es urgente |
| Indexación (`updatedb`) | 19 | Tarea de fondo de baja prioridad |
| Base de datos crítica | -5 a -10 | Necesita respuesta rápida (root) |
| Servicio web producción | 0 (default) | El default es suficiente |
| Debugging/profiling | 10 | No interferir con producción |

> **Recomendación**: No cambiar prioridades a menos que haya un
> problema observable de contención. El scheduler de Linux funciona
> bien con los defaults en la mayoría de situaciones.

---

## Labs

### Parte 1 — nice

#### 1.1 Valor de nice por defecto

```bash
echo "=== Nice por defecto ==="
nice
echo "(0 es el valor por defecto)"
echo ""
echo "=== Rango ==="
echo "-20 (mayor prioridad) ... 0 (default) ... +19 (menor prioridad)"
echo ""
echo "=== Relación con PR (priority en top) ==="
echo "PR = 20 + nice"
echo "nice -20 → PR  0 (máxima prioridad)"
echo "nice   0 → PR 20 (normal)"
echo "nice +19 → PR 39 (mínima prioridad)"
```

#### 1.2 Lanzar con nice

```bash
nice -n 10 sleep 300 &
PID_LOW=$!
sleep 301 &
PID_NORM=$!

echo "=== Comparar ==="
ps -o pid,ni,pri,cmd -p $PID_LOW,$PID_NORM
echo ""
echo "NI = nice value, PRI = kernel priority"

kill $PID_LOW $PID_NORM; wait 2>/dev/null
```

#### 1.3 Restricciones de usuario

```bash
echo "=== Nice negativo requiere root ==="
nice -n -5 sleep 300 2>&1 &
P1=$!
wait $P1 2>/dev/null
echo "Exit code: $? (no-zero = sin permiso)"
echo ""

echo "=== Nice positivo siempre funciona ==="
nice -n 15 sleep 300 &
P2=$!
ps -o pid,ni,cmd -p $P2
kill $P2; wait 2>/dev/null
```

#### 1.4 Múltiples nice values

```bash
sleep 300 &
P1=$!
nice -n 5 sleep 301 &
P2=$!
nice -n 10 sleep 302 &
P3=$!
nice -n 19 sleep 303 &
P4=$!

ps -o pid,ni,pri,cmd -p $P1,$P2,$P3,$P4
echo ""
echo "Mayor NI = menor prioridad"
echo "Con un solo proceso, nice no tiene efecto visible"

kill $P1 $P2 $P3 $P4; wait 2>/dev/null
```

---

### Parte 2 — renice

#### 2.1 renice básico

```bash
sleep 300 &
PID=$!
echo "Antes:"
ps -o pid,ni,cmd -p $PID

echo ""
echo "=== renice -n 10 ==="
renice -n 10 -p $PID
ps -o pid,ni,cmd -p $PID

echo ""
echo "=== Intentar bajar a 5 (sin root, debería fallar) ==="
renice -n 5 -p $PID 2>&1
ps -o pid,ni,cmd -p $PID
echo "(sigue en 10 — no se puede bajar sin root)"

kill $PID; wait $PID 2>/dev/null
```

#### 2.2 Verificar con top

```bash
nice -n 15 sleep 300 &
P1=$!
sleep 302 &
P2=$!
sleep 0.5

echo "=== Columnas NI y PR en top ==="
top -bn1 -p $P1,$P2 | tail -n +7 | head -4
echo ""
echo "NI = nice, PR = priority (20 + nice)"

kill $P1 $P2; wait 2>/dev/null
```

---

### Parte 3 — ionice

#### 3.1 Clases de ionice

```bash
echo "=== ionice actual del shell ==="
ionice -p $$
echo ""
echo "Clase 1 — Realtime: acceso prioritario (solo root)"
echo "Clase 2 — Best-effort: proporcional, 8 niveles (default)"
echo "Clase 3 — Idle: solo cuando nadie más usa disco"
```

#### 3.2 Verificar y cambiar clase

```bash
ionice -c 3 sleep 300 &
PID_IDLE=$!
ionice -c 2 -n 0 sleep 301 &
PID_HIGH=$!

echo "Idle:            $(ionice -p $PID_IDLE)"
echo "Best-effort 0:   $(ionice -p $PID_HIGH)"

kill $PID_IDLE $PID_HIGH; wait 2>/dev/null
```

#### 3.3 Scheduler de I/O

```bash
echo "=== Scheduler actual ==="
cat /sys/block/*/queue/scheduler 2>/dev/null | head -3 \
    || echo "(no disponible en contenedor)"
echo ""
echo "BFQ: ionice tiene efecto completo"
echo "mq-deadline: efecto parcial"
echo "none (NVMe): ionice sin efecto"
```

---

## Ejercicios

### Ejercicio 1 — nice values y PR

```bash
sleep 300 &
P0=$!
nice -n 10 sleep 301 &
P10=$!
nice -n 19 sleep 302 &
P19=$!

ps -o pid,ni,pri,cmd -p $P0,$P10,$P19
echo ""
echo "¿Cuál es la relación entre NI y PRI?"
echo "¿Cuál recibiría más CPU si compitieran?"

kill $P0 $P10 $P19; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

| PID | NI | PRI | CMD |
|---|---|---|---|
| P0 | 0 | 20 | sleep 300 |
| P10 | 10 | 30 | sleep 301 |
| P19 | 19 | 39 | sleep 302 |

**Relación**: PRI = 20 + NI. Menor NI (y menor PRI) = más prioridad.

**Si compitieran por CPU en el mismo core:**
- P0 (nice 0, peso 1024) recibiría ~91% del CPU
- P10 (nice 10, peso 110) recibiría ~8.5%
- P19 (nice 19, peso 15) recibiría ~0.5%

Pero como son sleeps (no consumen CPU), nice no afecta en la práctica.
Nice solo importa para procesos CPU-bound compitiendo por el mismo
recurso.

</details>

---

### Ejercicio 2 — nice solo importa con contención

```bash
echo "=== Un solo proceso CPU-bound ==="
nice -n 19 bash -c 'for i in $(seq 1 5000000); do :; done' &
PID_SOLO=$!
sleep 2
echo "Con nice 19 y sin contención:"
ps -o pid,ni,%cpu -p $PID_SOLO
wait $PID_SOLO 2>/dev/null

echo ""
echo "=== Dos procesos CPU-bound compitiendo ==="
bash -c 'while true; do :; done' &
PID_HIGH=$!
nice -n 19 bash -c 'while true; do :; done' &
PID_LOW=$!
sleep 3
echo "Con contención en el mismo core:"
ps -o pid,ni,%cpu -p $PID_HIGH,$PID_LOW
kill $PID_HIGH $PID_LOW; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Sin contención**: El proceso con nice 19 obtiene ~100% de CPU de
un core. Nice no limita un proceso — solo distribuye CPU cuando hay
competencia.

**Con contención**: Si el sistema tiene pocos cores:
- nice 0 (peso 1024) debería obtener ~99% del core
- nice 19 (peso 15) debería obtener ~1% del core

En un sistema multi-core, cada proceso podría tomar un core diferente
y ambos obtendrían ~100%. Para forzar contención en un solo core:
```bash
taskset -c 0 bash -c 'while true; do :; done' &
taskset -c 0 nice -n 19 bash -c 'while true; do :; done' &
```

**Nota**: Los %CPU de `ps` son promedios acumulados. Para ver el
efecto real, usar `top -d 1` o `htop` que muestran el uso instantáneo.

</details>

---

### Ejercicio 3 — renice: cambiar en caliente

```bash
sleep 300 &
PID=$!
echo "=== Estado inicial ==="
ps -o pid,ni,pri -p $PID

echo ""
echo "=== renice a 10 ==="
renice -n 10 -p $PID
ps -o pid,ni,pri -p $PID

echo ""
echo "=== renice a 19 ==="
renice -n 19 -p $PID
ps -o pid,ni,pri -p $PID

echo ""
echo "=== Intentar bajar a 5 (sin root) ==="
renice -n 5 -p $PID 2>&1
ps -o pid,ni,pri -p $PID
echo "(debería seguir en 19)"

kill $PID; wait $PID 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

| Paso | NI | PRI | Resultado |
|---|---|---|---|
| Inicial | 0 | 20 | Default |
| renice 10 | 10 | 30 | OK — subir nice siempre funciona |
| renice 19 | 19 | 39 | OK — seguir subiendo |
| renice 5 | 19 | 39 | **FALLA** — no se puede bajar sin root |

El error será: `renice: failed to set priority for 1234 (process ID):
Permission denied`

**renice es ABSOLUTO**: `renice -n 10` establece nice=10, no "suma 10".
Si el proceso tiene nice=5 y haces `renice -n 10`, el nice pasa a 10.

**Irreversibilidad para usuarios**: Una vez que un usuario sube el nice
de su proceso, no puede bajarlo. Solo root puede. Esto previene que un
usuario suba la prioridad de sus procesos a costa de otros.

</details>

---

### Ejercicio 4 — nice se hereda

```bash
echo "=== Nice del shell actual ==="
echo "Nice: $(nice)"

echo ""
echo "=== Hijo hereda nice del padre ==="
nice -n 10 bash -c '
    echo "Padre nice: $(nice)"
    sleep 300 &
    CHILD=$!
    echo "Hijo nice: $(awk "{print \$19}" /proc/$CHILD/stat)"
    echo "Hijo ps:   $(ps -o ni= -p $CHILD)"
    kill $CHILD
'

echo ""
echo "=== Nieto también hereda ==="
nice -n 15 bash -c '
    bash -c "echo Nieto nice: \$(nice)"
'
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- Shell actual: nice 0 (default)
- Padre con `nice -n 10`: nice 10
- Hijo del padre: nice 10 (hereda)
- Nieto: nice 15 (hereda del abuelo `nice -n 15`)

**La herencia es inmediata**: El hijo nace con el nice del padre. No
es posible "resetear" al default sin root. Esto tiene implicaciones:

- Si un script cron tiene nice 10, todos los procesos que lance
  también tendrán nice 10 (o más)
- Si un servicio systemd tiene Nice=15, todos sus workers heredan 15
- Un usuario que hace `renice -n 19 -p $$` afecta todo lo que lance
  desde ese shell

</details>

---

### Ejercicio 5 — nice default (sin -n)

```bash
echo "=== nice sin argumento muestra el nice actual ==="
nice
echo ""

echo "=== nice sin -n usa +10 por defecto ==="
nice sleep 300 &
PID=$!
sleep 0.5
echo "nice sin -n:"
ps -o pid,ni,cmd -p $PID
kill $PID; wait $PID 2>/dev/null

echo ""
echo "=== nice -n 10 explícito ==="
nice -n 10 sleep 300 &
PID=$!
sleep 0.5
echo "nice -n 10:"
ps -o pid,ni,cmd -p $PID
kill $PID; wait $PID 2>/dev/null

echo ""
echo "¿Son iguales?"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Ambos procesos tienen NI=10. `nice` sin `-n` es equivalente a
`nice -n 10`.

`nice` (solo, sin comando) imprime el nice actual: 0 en un shell
normal, o el valor heredado si el shell fue lanzado con nice.

**¿Por qué 10?** Es un compromiso razonable: baja la prioridad
significativamente (peso 1024 → 110, es decir ~10× menos CPU en
contención) pero no tanto como 19 (que prácticamente "mata de
hambre" al proceso bajo contención fuerte).

</details>

---

### Ejercicio 6 — ionice: clases y verificación

```bash
echo "=== ionice del shell ==="
ionice -p $$
echo ""

echo "=== Crear procesos con diferentes clases ==="
ionice -c 2 -n 0 sleep 300 &
PID_BE0=$!
ionice -c 2 -n 7 sleep 301 &
PID_BE7=$!
ionice -c 3 sleep 302 &
PID_IDLE=$!

echo "Best-effort 0: $(ionice -p $PID_BE0)"
echo "Best-effort 7: $(ionice -p $PID_BE7)"
echo "Idle:          $(ionice -p $PID_IDLE)"

echo ""
echo "=== Cambiar clase en caliente ==="
ionice -c 3 -p $PID_BE0
echo "Después: $(ionice -p $PID_BE0)"

kill $PID_BE0 $PID_BE7 $PID_IDLE; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- Shell: `best-effort: prio 4` (default, nivel derivado de nice)
- PID_BE0: `best-effort: prio 0` (máxima prioridad I/O dentro de BE)
- PID_BE7: `best-effort: prio 7` (mínima prioridad I/O dentro de BE)
- PID_IDLE: `idle` (solo usa disco cuando nadie más lo necesita)

Después de `ionice -c 3 -p $PID_BE0`: cambia a `idle`.

**Best-effort default**: Si no especificas clase, el kernel usa
best-effort con nivel derivado del nice del proceso:
`io_prio = (nice + 20) / 5`. Nice 0 → prio 4, nice 19 → prio 7.

**Efecto real**: Depende del scheduler de I/O. Con BFQ (HDDs), el
efecto es notable. Con mq-deadline o none (NVMe/SSDs), ionice tiene
poco o ningún efecto.

</details>

---

### Ejercicio 7 — nice + ionice combinados

```bash
echo "=== Tarea de mantenimiento con mínima prioridad ==="
echo "nice -n 19 ionice -c 3 → mínimo CPU + mínimo I/O"
echo ""

nice -n 19 ionice -c 3 dd if=/dev/zero of=/tmp/nice_test bs=1K count=1000 2>&1 &
PID=$!

echo "PID: $PID"
echo "nice:   $(ps -o ni= -p $PID | tr -d ' ')"
echo "ionice: $(ionice -p $PID)"

wait $PID 2>/dev/null
rm -f /tmp/nice_test

echo ""
echo "=== Casos de uso ==="
echo "Backup:     nice -n 19 ionice -c 3 rsync -a /data /backup"
echo "Indexación: nice -n 19 ionice -c 3 updatedb"
echo "Limpieza:   nice -n 19 ionice -c 3 find /tmp -mtime +7 -delete"
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

- nice: 19 (mínima prioridad CPU)
- ionice: idle (mínima prioridad I/O)

El dd completará pero cediendo CPU e I/O a cualquier otro proceso.
En un sistema idle, no habrá diferencia de velocidad perceptible.
En un sistema cargado, el dd irá mucho más lento que sin nice/ionice.

**¿Por qué combinar ambos?** Un backup puede ser:
- CPU-intensive (compresión con gzip/zstd)
- I/O-intensive (lectura de disco)
- O ambas

`nice -n 19` protege contra la carga de CPU.
`ionice -c 3` protege contra la carga de I/O.
Combinados, la tarea no interfiere con servicios de producción.

</details>

---

### Ejercicio 8 — renice por usuario

```bash
sleep 300 &
P1=$!
sleep 301 &
P2=$!
sleep 302 &
P3=$!

echo "=== Antes ==="
ps -o pid,ni,cmd -p $P1,$P2,$P3

echo ""
echo "=== renice -n 15 -u $(whoami) ==="
renice -n 15 -u $(whoami) 2>&1 | head -3
echo ""

echo "=== Después ==="
ps -o pid,ni,cmd -p $P1,$P2,$P3
echo ""
echo "¡Todos los procesos del usuario cambiaron!"

kill $P1 $P2 $P3; wait 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Antes: los 3 procesos con NI=0 (default).

Después de `renice -n 15 -u $(whoami)`: **TODOS** los procesos del
usuario actual pasan a nice=15. Esto incluye:
- Los 3 sleeps
- El shell actual
- Cualquier otro proceso del usuario

**Cuidado**: `renice -u` es poderoso. Cambia la prioridad de TODO lo
que corre bajo ese usuario. Útil para un admin que quiere bajar la
prioridad de un usuario que consume demasiada CPU.

**Después de esto**, el shell mismo tiene nice=15. Todos los procesos
nuevos que lances heredarán nice=15 (o más). Sin root, no puedes
volver a nice=0.

</details>

---

### Ejercicio 9 — Ver prioridades del sistema

```bash
echo "=== Procesos con nice != 0 ==="
ps -eo pid,ni,user,comm --sort=ni | awk '$2 != 0 && $2 != "-" {print}' | head -15
echo ""

echo "=== Distribución de nice values ==="
ps -eo ni --no-headers | sort -n | uniq -c | sort -rn | head -10
echo "(columnas: cantidad  nice_value)"
echo ""

echo "=== Procesos con nice negativo (alta prioridad) ==="
ps -eo pid,ni,user,comm --sort=ni | awk '$2 < 0 {print}' | head -10
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

**Mayoría en nice 0**: La gran mayoría de procesos corren con nice 0
(default). Solo servicios especialmente configurados tienen nice != 0.

**Nice negativo**: Pocos procesos, típicamente:
- Servicios de audio (PulseAudio/PipeWire con nice -11)
- irqbalance (balanceo de interrupciones)
- Procesos de sistema configurados para alta prioridad

**En un contenedor**: La mayoría tendrá nice 0 porque no hay servicios
de audio ni configuraciones especiales de prioridad.

**Distribución típica**:
```
  150  0    ← la inmensa mayoría
    3 -10   ← servicios del sistema
    1  19   ← tareas de background
```

</details>

---

### Ejercicio 10 — nice y /proc/PID/stat

```bash
nice -n 12 sleep 300 &
PID=$!

echo "=== Formas de ver el nice value ==="
echo "1. nice (dentro del proceso):    $(nice)"
echo "2. ps -o ni:                     $(ps -o ni= -p $PID | tr -d ' ')"
echo "3. /proc/PID/stat campo 19:      $(awk '{print $19}' /proc/$PID/stat)"
echo "4. /proc/PID/status (grep Nice): $(grep '^Nice:' /proc/$PID/status 2>/dev/null || echo 'N/A')"
echo "5. top PR (=20+nice):            $(top -bn1 -p $PID | tail -1 | awk '{print $6}')"

echo ""
echo "=== Campo 18 de stat es priority, 19 es nice ==="
echo "stat: $(cat /proc/$PID/stat | awk '{print "priority="$18, "nice="$19}')"

kill $PID; wait $PID 2>/dev/null
```

<details><summary>Predicción</summary>

**¿Qué esperas ver?**

Todas las fuentes muestran nice=12:
1. `nice` → solo funciona dentro del proceso mismo (aquí muestra el del shell, no el del sleep)
2. `ps -o ni` → 12
3. `/proc/PID/stat` campo 19 → 12
4. `/proc/PID/status` → `Nice: 12` (no todos los kernels incluyen este campo)
5. `top` PR → 32 (= 20 + 12)

**Campo 18 vs 19 de /proc/PID/stat:**
- Campo 18: priority (lo que el kernel usa internamente)
- Campo 19: nice value (-20 a 19)
- La relación: priority ≈ 20 + nice para procesos normales

**ps, top, htop, /proc** — todos leen del mismo sitio: `/proc/PID/stat`.
Son solo formateadores diferentes de la misma información del kernel.

</details>
