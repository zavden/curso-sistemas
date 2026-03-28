# T01 — vmstat

> **Objetivo:** Dominar `vmstat` para diagnosticar problemas de rendimiento en CPU, memoria, swap e I/O. Entender qué significan cada una de sus columnas.

## Erratas detectadas en el material fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| README.max.md | 324,328,331,337 | Texto en chino en el Ejercicio 2: `正常运行`, `CPU饱和`, `I/O瓶颈`, `VM资源不足` | Traducir a español: "Operación normal", "CPU saturada", "Cuello de botella I/O", "VM sin recursos" |

---

## Qué es vmstat

**vmstat** (Virtual Memory Statistics) muestra un resumen en tiempo real de la actividad del sistema: procesos, memoria, swap, I/O de disco y CPU. Es la primera herramienta que un sysadmin ejecuta para diagnosticar problemas de rendimiento.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           VMSTAT OUTPUT                                     │
│                                                                             │
│  procs -----------memory---------- ---swap-- -----io---- -system-- ---cpu---│
│    r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st│
│    1  0      0 2048000 128000 1024000  0    0     5    10  100  200 10  3 85  2  0│
│                                                                             │
│  └procesos┘  └─────memoria─────┘  └swap┘ └─I/O──┘ └system┘ └───CPU───┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Snapshot único (promedio desde el boot):
vmstat

# Monitoreo continuo:
vmstat 2          # cada 2 segundos (infinito)
vmstat 2 10       # cada 2 segundos, 10 muestras

# IMPORTANTE: La primera línea SIEMPRE es un PROMEDIO desde el boot
# Las líneas siguientes son datos del intervalo
```

---

## Interpretación de columnas

### procs — Estado de procesos

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `r` | Run queue | Procesos en ejecución o **esperando CPU** |
| `b` | Blocked | Procesos **bloqueados** esperando I/O (disco, red) |

```bash
# r = Run queue
# Si r > número de CPUs → CPU saturada

# b = Blocked
# Si b > 0 sostenido → problema de I/O

# Escenarios:
#  r  b
#  1  0     → normal: 1 proceso corriendo, 0 bloqueados
# 12  0     → CPU saturada: 12 procesos esperan CPU
#  1  5     → I/O problema: 5 procesos esperan disco
# 15  8     → ambos saturados: CPU e I/O
```

```bash
# ¿Cuántas CPUs tienes?
nproc
# 4

# Regla: si r > nproc consistentemente → CPU es el cuello de botella
```

### memory — Memoria (KB)

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `swpd` | Swap used | Memoria actualmente en swap |
| `free` | Free memory | RAM libre |
| `buff` | Buffers | Memoria para buffers de I/O del kernel (metadata FS) |
| `cache` | Cache | Page cache (archivos leídos del disco) |

```bash
# Linux usa TODA la RAM disponible para cache
# free bajo NO significa problema — el cache se libera bajo presión

# La memoria "disponible" real es aproximadamente:
# free + buff + cache

# Ejemplo:
#   swpd   free    buff   cache
#      0 2048000 128000 1024000  → sin swap, 2GB free, 1GB cache
# 512000   64000  32000  256000  → 512MB en swap, poca RAM libre

# Para ver la memoria real disponible:
free -h
# La columna "available" es la que importa
```

### swap — Actividad de swap

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `si` | Swap In | KB/s leídos desde swap (disco → RAM) |
| `so` | Swap Out | KB/s escritos a swap (RAM → disco) |

```
ESCENARIOS:

  si   so
   0    0    → sin actividad de swap (ideal)
   0  500    → escribiendo 500KB/s a swap (presión de memoria)
 200  300    → lectura Y escritura (THRASHING — MUY malo)

THRASHING = si Y so altos simultáneamente
  El sistema pasa más tiempo MOVIENDO páginas que TRABAJANDO
  Solución: agregar RAM o reducir carga
```

### io — Actividad de disco

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `bi` | Blocks In | Bloques/s leídos desde dispositivos de bloque (1 bloque = 1 KB) |
| `bo` | Blocks Out | Bloques/s escritos a dispositivos de bloque |

```
  bi    bo
   5    10    → I/O bajo (servidor idle normal)
5000    50    → lectura intensa (base de datos leyendo)
  10  8000    → escritura intensa (backup, logging)
5000  5000    → I/O alto bidireccional (copia de archivos)
```

### system — Actividad del sistema

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `in` | Interrupts | Interrupciones por segundo (hardware + software) |
| `cs` | Context Switches | Cambios de contexto por segundo |

```
  in    cs
 100   200    → sistema tranquilo
5000 15000    → sistema ocupado (normal bajo carga)
50000 100000  → posible problema (demasiados context switches)

Context switches altos pueden indicar:
  - Demasiados procesos compitiendo por CPU
  - Locks contention (muchos threads esperando)
  - Interrupciones de red excesivas (DDoS, storm)
```

### cpu — Uso de CPU (porcentaje)

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `us` | User | Tiempo en código de usuario (aplicaciones) |
| `sy` | System | Tiempo en código del kernel (syscalls, drivers) |
| `id` | Idle | Tiempo ocioso |
| `wa` | Wait I/O | Tiempo esperando I/O de disco |
| `st` | Stolen | Tiempo robado por hipervisor (VMs) |

```bash
# Los 5 valores suman ~100%

# Patrones de diagnóstico:
# us alto + id bajo      → aplicación consume CPU (optimizar código)
# sy alto                → demasiadas syscalls o I/O del kernel
# wa alto                → disco lento o I/O excesivo
# st alto                → VM sin recursos (hablar con el proveedor)
# us+sy alto + r > nproc → necesitas más CPU
```

```
EJEMPLOS:

  us sy id wa st
  10  3 85  2  0    → sistema tranquilo (85% idle)
  85  5  5  5  0    → CPU ocupada por aplicaciones (us=85%)
  10 60  5 25  0    → mucho kernel + I/O wait (problema de disco)
   5  2 90  3  0    → idle (sin carga)
  30  5 10  5 50    → VM con 50% stolen (hipervisor saturado)
```

---

## Opciones útiles

```bash
# Unidades legibles:
vmstat -S M      # mostrar en MB (mayúscula = 1024)
vmstat -S m      # mostrar en MB (minúscula = 1000)
vmstat -S k      # mostrar en KB (default)

# Estadísticas de disco:
vmstat -d        # estadísticas de todos los discos
vmstat -p sda1   # estadísticas por partición

# Modo wide (columnas más anchas):
vmstat -w 2      # evita que números grandes se peguen

# Con timestamp:
vmstat -t 2      # agrega timestamp a cada línea

# Slab allocator (cache del kernel):
vmstat -m | head -10

# Memoria active/inactive (reemplaza buff/cache):
vmstat -a 2      # muestra inact/active en lugar de buff/cache
```

---

## Patrones de uso comunes

### Monitoreo rápido de un problema

```bash
# ¿El servidor está sobrecargado?
vmstat 1 5
# Mirar: r (run queue), us+sy (CPU), wa (I/O wait)

# CPU saturada:       r > nproc Y id < 5%
# I/O problema:       wa > 20%
```

### Detectar presión de memoria

```bash
# ¿El sistema está swappeando?
vmstat 1 5
# Mirar: si, so (swap), swpd, free

# si/so > 0 sostenido → la RAM es insuficiente
# swpd > 0 pero si/so = 0 → swap DEL PASADO, ya no hay presión
```

### Capturar baseline y durante incidente

```bash
# CAPTURA 1: Baseline (sistema normal)
vmstat 5 > /tmp/vmstat-baseline.txt &
# Dejar corriendo en background

# CAPTURA 2: Durante el problema
vmstat 1 60 | tee /tmp/vmstat-incident.txt
# Comparar después: diff baseline vs incident
```

### vmstat en scripts de monitoreo

```bash
# Alerta si I/O wait > 30%:
WA=$(vmstat 1 2 | tail -1 | awk '{print $16}')
if [ "$WA" -gt 30 ]; then
    echo "ALERTA: I/O wait al ${WA}%"
fi

# Alerta si run queue excede CPUs:
CPUS=$(nproc)
RQ=$(vmstat 1 2 | tail -1 | awk '{print $1}')
if [ "$RQ" -gt "$CPUS" ]; then
    echo "ALERTA: run queue $RQ > $CPUS CPUs"
fi

# Alerta si hay thrashing (swap in AND out):
SI=$(vmstat 1 2 | tail -1 | awk '{print $7}')
SO=$(vmstat 1 2 | tail -1 | awk '{print $8}')
if [ "$SI" -gt 0 ] && [ "$SO" -gt 0 ]; then
    echo "ALERTA: THRASHING detectado (si=$SI so=$SO)"
fi
```

---

## Quick reference

```
SINTAXIS:
  vmstat              → snapshot (promedio desde boot)
  vmstat N            → cada N segundos (infinito)
  vmstat N M          → cada N segundos, M muestras

OPCIONES:
  -S [kKmM]     → unidades (k=KB, K=KiB, m=MB, M=MiB)
  -d            → estadísticas de disco
  -p PART       → estadísticas por partición
  -w            → modo wide
  -t            → con timestamp
  -m            → slab allocator
  -a            → memoria active/inactive

COLUMNAS CLAVE:
  r > nproc     → CPU saturada
  b > 0         → problema de I/O
  si+so > 0     → presión de memoria (thrashing si ambos)
  wa > 20%      → disco es el cuello de botella
  st > 10%      → VM sin recursos (hipervisor)

NÚMEROS DE COLUMNA (para awk):
  1=r 2=b 3=swpd 4=free 5=buff 6=cache
  7=si 8=so 9=bi 10=bo 11=in 12=cs
  13=us 14=sy 15=id 16=wa 17=st
```

---

## Labs

### Lab Parte 1 — Lectura básica

**Paso 1.1: Snapshot único**

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat — snapshot ==="
echo ""
echo "Un vmstat sin argumentos muestra promedios desde el boot:"
echo ""
vmstat
echo ""
echo "IMPORTANTE: esta salida es un PROMEDIO desde que arrancó"
echo "el sistema. No refleja lo que pasa ahora."
echo ""
echo "--- Uptime del sistema ---"
uptime
'
```

**Paso 1.2: Monitoreo con intervalo**

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat con intervalo ==="
echo ""
echo "vmstat 2 5 → cada 2 segundos, 5 muestras:"
echo ""
vmstat 2 5
echo ""
echo "--- Observar ---"
echo "  Línea 1: PROMEDIO desde el boot (ignorar para diagnóstico)"
echo "  Líneas 2-5: datos del INTERVALO (lo que importa)"
echo ""
echo "Si solo pones vmstat 2 (sin count), corre infinitamente"
echo "hasta Ctrl+C."
'
```

**Paso 1.3: Encabezados de columnas**

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupos de columnas ==="
echo ""
echo "vmstat organiza la salida en 6 grupos:"
echo ""
echo "  procs     r  b          procesos"
echo "  memory    swpd free buff cache   memoria"
echo "  swap      si  so         actividad de swap"
echo "  io        bi  bo         actividad de disco"
echo "  system    in  cs         interrupciones y context switches"
echo "  cpu       us sy id wa st porcentaje de CPU"
echo ""
echo "--- En vivo con -w (wide) ---"
vmstat -w 1 3
echo ""
echo "La opción -w hace las columnas más anchas"
echo "para evitar que los números se peguen."
'
```

### Lab Parte 2 — Interpretación de columnas

**Paso 2.1: procs — procesos**

```bash
docker compose exec debian-dev bash -c '
echo "=== procs ==="
echo ""
echo "  r = Run queue: procesos ejecutándose o esperando CPU"
echo "  b = Blocked: procesos bloqueados esperando I/O"
echo ""
echo "--- Diagnóstico ---"
CPUS=$(nproc)
echo "  Este sistema tiene $CPUS CPUs"
echo ""
echo "  r > $CPUS sostenido → CPU saturada"
echo "  b > 0 sostenido → problema de I/O"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk -v cpus="$CPUS" '"'"'{
    printf "  r=%s (run queue)  b=%s (blocked)\n", $1, $2
    if ($1 > cpus) print "  ALERTA: r > CPUs"
    else print "  OK: r <= CPUs"
    if ($2 > 0) print "  NOTA: hay procesos bloqueados en I/O"
    else print "  OK: sin procesos bloqueados"
}'"'"'
'
```

**Paso 2.2: memory — memoria**

```bash
docker compose exec debian-dev bash -c '
echo "=== memory ==="
echo ""
echo "  swpd  = memoria en swap (KB)"
echo "  free  = RAM libre (no usada para nada)"
echo "  buff  = buffers del kernel (metadata de filesystem)"
echo "  cache = page cache (archivos leídos del disco)"
echo ""
echo "--- Valores actuales ---"
vmstat -S M 1 2 | tail -1 | awk '"'"'{
    printf "  swpd=%sMB  free=%sMB  buff=%sMB  cache=%sMB\n", $3, $4, $5, $6
    avail = $4 + $5 + $6
    printf "  Disponible aprox: %sMB (free + buff + cache)\n", avail
}'"'"'
echo ""
echo "--- Interpretación ---"
echo "  free bajo NO es problema — Linux usa RAM libre como cache"
echo "  Lo que importa es: free + buff + cache (disponible)"
echo "  buff y cache se liberan automáticamente si se necesita RAM"
echo ""
echo "--- Comparar con free ---"
free -m | head -2
echo "La columna available de free es la referencia correcta."
'
```

**Paso 2.3: swap — actividad de swap**

```bash
docker compose exec debian-dev bash -c '
echo "=== swap ==="
echo ""
echo "  si = Swap In:  KB/s leídos desde swap (disco → RAM)"
echo "  so = Swap Out: KB/s escritos a swap (RAM → disco)"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  si=%s  so=%s\n", $7, $8
    if ($7 == 0 && $8 == 0) print "  OK: sin actividad de swap"
    else if ($8 > 0 && $7 == 0) print "  NOTA: escribiendo a swap (presión de memoria)"
    else if ($7 > 0 && $8 > 0) print "  ALERTA: thrashing (si Y so altos)"
}'"'"'
echo ""
echo "--- Patrones ---"
echo "  si=0   so=0     ideal (sin swap)"
echo "  si=0   so=500   escribiendo 500KB/s a swap (falta RAM)"
echo "  si=200 so=300   THRASHING — el sistema mueve páginas"
echo "                  entre RAM y disco constantemente"
'
```

**Paso 2.4: io — actividad de disco**

```bash
docker compose exec debian-dev bash -c '
echo "=== io ==="
echo ""
echo "  bi = Blocks In:  bloques/s leídos (disco → RAM)"
echo "  bo = Blocks Out: bloques/s escritos (RAM → disco)"
echo "  Un bloque = 1024 bytes (1 KB)"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  bi=%s  bo=%s\n", $9, $10
    if ($9 < 100 && $10 < 100) print "  I/O bajo (normal en idle)"
    else if ($9 > 1000) print "  Lectura intensa"
    else if ($10 > 1000) print "  Escritura intensa"
}'"'"'
'
```

**Paso 2.5: system — interrupciones y context switches**

```bash
docker compose exec debian-dev bash -c '
echo "=== system ==="
echo ""
echo "  in = Interrupts: interrupciones por segundo"
echo "  cs = Context Switches: cambios de contexto por segundo"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  in=%s  cs=%s\n", $11, $12
    if ($12 > 50000) print "  NOTA: context switches altos"
    else print "  Normal"
}'"'"'
echo ""
echo "  cs altos pueden indicar:"
echo "    - Demasiados procesos compitiendo por CPU"
echo "    - Lock contention (threads esperando)"
echo "    - Interrupciones de red excesivas"
'
```

**Paso 2.6: cpu — porcentaje de CPU**

```bash
docker compose exec debian-dev bash -c '
echo "=== cpu ==="
echo ""
echo "  us = User:    tiempo en código de aplicaciones"
echo "  sy = System:  tiempo en código del kernel"
echo "  id = Idle:    tiempo ocioso"
echo "  wa = Wait IO: esperando I/O de disco"
echo "  st = Stolen:  robado por hipervisor (VMs)"
echo ""
echo "  Los 5 valores suman ~100%"
echo ""
echo "--- Valores actuales ---"
vmstat 1 3 | tail -1 | awk '"'"'{
    printf "  us=%s%%  sy=%s%%  id=%s%%  wa=%s%%  st=%s%%\n", $13, $14, $15, $16, $17
    if ($15 > 80) print "  → Sistema tranquilo (mucho idle)"
    else if ($13 > 70) print "  → CPU ocupada por aplicaciones"
    else if ($16 > 20) print "  → Disco es el cuello de botella"
    else if ($17 > 10) print "  → VM con tiempo robado"
}'"'"'
echo ""
echo "--- Patrones de diagnóstico ---"
echo "  us alto + id bajo      → app consume CPU"
echo "  sy alto                → demasiadas syscalls o I/O kernel"
echo "  wa alto                → disco lento o I/O excesivo"
echo "  st alto                → VM sin recursos (hipervisor saturado)"
'
```

### Lab Parte 3 — Opciones y scripts

**Paso 3.1: Opciones útiles**

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de vmstat ==="
echo ""
echo "--- -S M (unidades en MB) ---"
vmstat -S M 1 2 | tail -1
echo "(memoria en MB en lugar de KB)"
echo ""
echo "--- -t (con timestamp) ---"
vmstat -t 1 3
echo ""
echo "--- -a (activo/inactivo) ---"
echo "Reemplaza buff/cache con inact/active:"
vmstat -a 1 2
echo ""
echo "--- -d (estadísticas de disco) ---"
vmstat -d 2>/dev/null | head -5
echo ""
echo "--- -w (wide — columnas más anchas) ---"
vmstat -w 1 2
'
```

**Paso 3.2: Patrones de uso**

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones de uso ==="
echo ""
echo "--- 1. Monitoreo rápido de CPU ---"
echo "vmstat 1 5"
echo "Mirar: r (run queue), us+sy (CPU), wa (I/O wait)"
echo "Si r > nproc Y id < 5% → CPU saturada"
echo ""
echo "--- 2. Detectar presión de memoria ---"
echo "vmstat 1 5"
echo "Mirar: si, so (swap), swpd, free"
echo "si/so > 0 sostenido → RAM insuficiente"
echo ""
echo "--- 3. Capturar baseline ---"
echo "vmstat 5 > /tmp/vmstat-baseline.txt &"
echo ""
echo "--- 4. Durante un incidente ---"
echo "vmstat 1 60 | tee /tmp/vmstat-incident.txt"
echo ""
echo "--- Captura de 5 segundos ---"
vmstat -t 1 5
'
```

**Paso 3.3: vmstat en scripts**

```bash
docker compose exec debian-dev bash -c '
echo "=== vmstat en scripts ==="
echo ""
echo "--- Extraer I/O wait ---"
WA=$(vmstat 1 2 | tail -1 | awk "{print \$16}")
echo "I/O wait actual: ${WA}%"
if [ "$WA" -gt 30 ] 2>/dev/null; then
    echo "  ALERTA: I/O wait supera 30%"
else
    echo "  OK: I/O wait normal"
fi
echo ""
echo "--- Extraer run queue ---"
CPUS=$(nproc)
RQ=$(vmstat 1 2 | tail -1 | awk "{print \$1}")
echo "Run queue: $RQ  CPUs: $CPUS"
if [ "$RQ" -gt "$CPUS" ] 2>/dev/null; then
    echo "  ALERTA: run queue $RQ > $CPUS CPUs"
else
    echo "  OK: run queue dentro de capacidad"
fi
echo ""
echo "--- Verificar swap activo ---"
SO=$(vmstat 1 2 | tail -1 | awk "{print \$8}")
echo "Swap out: ${SO} KB/s"
if [ "$SO" -gt 0 ] 2>/dev/null; then
    echo "  ALERTA: el sistema está swappeando"
else
    echo "  OK: sin swap out"
fi
echo ""
echo "--- Números de columna para awk ---"
echo "  1=r 2=b 3=swpd 4=free 5=buff 6=cache"
echo "  7=si 8=so 9=bi 10=bo 11=in 12=cs"
echo "  13=us 14=sy 15=id 16=wa 17=st"
'
```

---

## Ejercicios

### Ejercicio 1 — Leer vmstat en tu sistema

Ejecuta vmstat y observa el estado de tu sistema.

```bash
vmstat 2 5
```

**Predicción:**

<details><summary>¿La primera línea será diferente de las demás?</summary>

Sí. La primera línea es siempre un **promedio desde el boot** del sistema, no del intervalo actual. Las líneas 2-5 muestran los datos del intervalo de 2 segundos. Por eso, para diagnóstico siempre se ignora la primera línea. Si el sistema lleva días encendido, la primera línea será un promedio muy suavizado que no refleja la situación actual.
</details>

### Ejercicio 2 — Interpretar escenarios

Analiza cada salida y determina el diagnóstico.

```bash
# ¿Qué indica cada escenario?

# a) Operación normal:
#  r  b   swpd   free   buff   cache   si   so   bi   bo   in   cs  us  sy  id  wa  st
#  1  0      0 2048000 128000 1024000   0    0    5   10  100  200  10   3  85   2   0

# b) CPU saturada:
# 15  0      0 1024000  64000  512000   0    0    0    0  500  800  90   5   3   2   0

# c) Cuello de botella I/O:
#  1  8      0  256000  32000  512000   0    0 5000   50 1000 2000   5   3  40  52   0

# d) Thrashing:
#  5  3   512000  64000  32000  256000 200  300    0    0  200  400   5   2  90   3   0

# e) VM sin recursos:
#  2  0      0 2048000 128000 1024000   0    0    5   10  100  200  30   5  10   5  50
```

**Predicción:**

<details><summary>¿Cuál es el diagnóstico de cada escenario?</summary>

- **a)** Sistema sano: r=1 (1 proceso), id=85% (mucho idle), sin swap, I/O bajo.
- **b)** CPU saturada: r=15 (15 procesos compitiendo), us=90% (aplicaciones consumen CPU), id=3% (casi sin idle). Solución: optimizar la app o añadir CPUs.
- **c)** I/O es el cuello de botella: b=8 (8 procesos bloqueados en I/O), wa=52% (más de la mitad del tiempo esperando disco), bi=5000 (lectura intensa). Solución: disco más rápido (SSD), optimizar queries, más RAM para cache.
- **d)** Thrashing: si=200 Y so=300 (swap bidireccional activo), swpd=512MB. El sistema mueve páginas entre RAM y disco constantemente. Solución: agregar RAM o reducir carga.
- **e)** VM sin recursos: st=50% (el hipervisor roba la mitad del CPU). La VM no tiene suficientes recursos asignados. Solución: hablar con el proveedor/equipo de virtualización.
</details>

### Ejercicio 3 — Modo wide y timestamp

Usa las opciones `-w` y `-t` para mejor legibilidad.

```bash
echo "=== Con -w (wide) ==="
vmstat -w 1 3

echo ""
echo "=== Con -t (timestamp) ==="
vmstat -t 1 3
```

**Predicción:**

<details><summary>¿Cuándo es útil cada opción?</summary>

`-w` (wide) es útil cuando los números son grandes y las columnas se pegan (p. ej., `free` de 6+ dígitos). `-t` (timestamp) es esencial cuando capturas a un archivo para correlacionar eventos: `vmstat -t 1 60 > /tmp/vmstat.txt`. Sin timestamp no sabes cuándo ocurrió cada muestra. Para diagnóstico de incidentes, usar siempre `-t`.
</details>

### Ejercicio 4 — Unidades y formato

Compara la salida en diferentes unidades.

```bash
echo "=== En KB (default) ==="
vmstat 1 2 | tail -1

echo ""
echo "=== En MB (-S M) ==="
vmstat -S M 1 2 | tail -1

echo ""
echo "=== Active/Inactive (-a) ==="
vmstat -a 1 2 | tail -1
echo "(reemplaza buff/cache con inact/active)"
```

**Predicción:**

<details><summary>¿Qué cambia entre buff/cache y active/inactive?</summary>

Con `-a`, las columnas `buff` y `cache` se reemplazan por `inact` (páginas inactivas en memoria, candidatas a ser reclamadas) y `active` (páginas activamente en uso, que el kernel intenta mantener en RAM). Es otra forma de ver la presión de memoria: si `active` crece mucho e `inact` baja, el sistema está bajo presión. Los valores `free`, `swpd` y el resto no cambian.
</details>

### Ejercicio 5 — Estadísticas de disco

Usa `vmstat -d` para ver estadísticas de disco.

```bash
echo "=== Estadísticas de disco ==="
vmstat -d 2>/dev/null
```

**Predicción:**

<details><summary>¿Qué información adicional da vmstat -d respecto a bi/bo?</summary>

`vmstat -d` muestra totales acumulados desde el boot para cada dispositivo de bloque: número total de lecturas/escrituras, sectores leídos/escritos, milisegundos gastados en I/O, y operaciones en curso. A diferencia de `bi`/`bo` (que muestran bloques/s del intervalo), `-d` muestra el historial completo. Para análisis detallado de I/O por disco, `iostat` (T02) es más apropiado.
</details>

### Ejercicio 6 — Script de alerta I/O wait

Escribe un script que alerte si el I/O wait supera un umbral.

```bash
WA=$(vmstat 1 2 | tail -1 | awk '{print $16}')
echo "I/O wait actual: ${WA}%"
if [ "$WA" -gt 30 ]; then
    echo "ALERTA: I/O wait al ${WA}% (>30%)"
else
    echo "OK: I/O wait dentro de rango normal"
fi
```

**Predicción:**

<details><summary>¿Por qué usamos `vmstat 1 2 | tail -1` y no solo `vmstat`?</summary>

Porque `vmstat` sin argumentos da el promedio desde el boot (inútil para alertas). Con `vmstat 1 2`, tomamos 2 muestras con 1 segundo de intervalo: la primera es el promedio histórico (la descartamos con `tail -1`), la segunda es el dato real del último segundo. `awk '{print $16}'` extrae la columna 16 que es `wa` (I/O wait). Este patrón de `vmstat 1 2 | tail -1` es estándar para extraer valores actuales en scripts.
</details>

### Ejercicio 7 — Script de alerta de run queue

Alerta si la run queue excede el número de CPUs.

```bash
CPUS=$(nproc)
RQ=$(vmstat 1 2 | tail -1 | awk '{print $1}')
echo "CPUs: $CPUS, Run queue: $RQ"
if [ "$RQ" -gt "$CPUS" ]; then
    echo "ALERTA: run queue $RQ excede $CPUS CPUs"
else
    echo "OK: run queue dentro de capacidad"
fi
```

**Predicción:**

<details><summary>¿En un contenedor Docker, qué valor dará nproc?</summary>

`nproc` dentro de un contenedor reporta los CPUs del **host** (o los limitados por `--cpus`/cgroup). Si el host tiene 8 CPUs y el contenedor no tiene límite, `nproc` dirá 8. Pero si el contenedor tiene `--cpus=2`, puede que `nproc` aún diga 8 (depende de la versión de kernel). Para contenedores con cgroup v2, las versiones modernas de `nproc` respetan los límites. Esto es importante: la alerta puede ser incorrecta si `nproc` no refleja los CPUs reales disponibles para el contenedor.
</details>

### Ejercicio 8 — Detectar thrashing

Detecta si hay thrashing (swap in Y out simultáneos).

```bash
LINE=$(vmstat 1 2 | tail -1)
SI=$(echo "$LINE" | awk '{print $7}')
SO=$(echo "$LINE" | awk '{print $8}')
echo "Swap In: $SI KB/s, Swap Out: $SO KB/s"
if [ "$SI" -gt 0 ] && [ "$SO" -gt 0 ]; then
    echo "ALERTA: THRASHING detectado (si=$SI so=$SO)"
    echo "El sistema mueve páginas entre RAM y disco constantemente"
    echo "Solución: agregar RAM o reducir carga"
elif [ "$SO" -gt 0 ]; then
    echo "NOTA: swap out activo (presión de memoria)"
elif [ "$SI" -gt 0 ]; then
    echo "NOTA: swap in activo (recuperando páginas)"
else
    echo "OK: sin actividad de swap"
fi
```

**Predicción:**

<details><summary>¿Por qué es tan grave el thrashing?</summary>

Thrashing ocurre cuando la RAM es insuficiente y el sistema constantemente mueve páginas entre RAM y disco (swap). Cada page fault que requiere leer de disco tarda ~5-10ms (vs ~100ns en RAM — unas 100,000 veces más lento). Cuando tanto `si` como `so` son altos, el sistema gasta más tiempo moviendo páginas que ejecutando trabajo útil. El rendimiento cae drásticamente y puede parecer que el sistema está "congelado". La solución es agregar RAM, matar procesos que consumen mucha memoria, o reducir la carga.
</details>

### Ejercicio 9 — Capturar baseline

Captura una línea base del sistema en reposo.

```bash
echo "=== Baseline (5 muestras) ==="
vmstat -t -w 1 5

echo ""
echo "=== Resumen ==="
vmstat 1 5 | tail -n +2 | awk '{
    r+=$1; b+=$2; us+=$13; sy+=$14; id+=$15; wa+=$16; n++
} END {
    printf "Promedio: r=%.1f b=%.1f us=%.0f%% sy=%.0f%% id=%.0f%% wa=%.0f%%\n",
        r/n, b/n, us/n, sy/n, id/n, wa/n
}'
```

**Predicción:**

<details><summary>¿Por qué excluimos la primera línea con tail -n +2?</summary>

Porque la primera línea es el promedio desde el boot, que contaminaría nuestro cálculo. Con `tail -n +2` la saltamos y solo promediamos las líneas del intervalo real. Este baseline capturado en un momento tranquilo sirve como referencia: durante un incidente, comparas los valores del incidente con el baseline para ver qué cambió (p. ej., si el baseline tiene wa=2% y durante el incidente tiene wa=45%, claramente el disco es el problema).
</details>

### Ejercicio 10 — Script de resumen del sistema

Combina todo en un diagnóstico rápido.

```bash
echo "=== DIAGNÓSTICO RÁPIDO DEL SISTEMA ==="
echo ""

CPUS=$(nproc)
LINE=$(vmstat 1 2 | tail -1)

R=$(echo "$LINE" | awk '{print $1}')
B=$(echo "$LINE" | awk '{print $2}')
SI=$(echo "$LINE" | awk '{print $7}')
SO=$(echo "$LINE" | awk '{print $8}')
US=$(echo "$LINE" | awk '{print $13}')
SY=$(echo "$LINE" | awk '{print $14}')
ID=$(echo "$LINE" | awk '{print $15}')
WA=$(echo "$LINE" | awk '{print $16}')
ST=$(echo "$LINE" | awk '{print $17}')

echo "CPU:    us=${US}% sy=${SY}% id=${ID}% wa=${WA}% st=${ST}%"
echo "Procs:  r=$R (CPUs=$CPUS) b=$B"
echo "Swap:   si=$SI so=$SO"
echo ""

echo "--- Diagnóstico ---"
[ "$R" -gt "$CPUS" ] 2>/dev/null && echo "  CPU: run queue ($R) > CPUs ($CPUS)"
[ "$ID" -lt 5 ] 2>/dev/null && echo "  CPU: casi sin idle (${ID}%)"
[ "$WA" -gt 20 ] 2>/dev/null && echo "  I/O: wait alto (${WA}%)"
[ "$B" -gt 0 ] 2>/dev/null && echo "  I/O: $B procesos bloqueados"
[ "$SO" -gt 0 ] 2>/dev/null && echo "  MEM: swap out activo (${SO} KB/s)"
[ "$SI" -gt 0 ] 2>/dev/null && [ "$SO" -gt 0 ] 2>/dev/null && echo "  MEM: THRASHING detectado"
[ "$ST" -gt 10 ] 2>/dev/null && echo "  VM: ${ST}% tiempo robado por hipervisor"

# Si no hubo alertas:
if [ "$ID" -ge 5 ] && [ "$WA" -le 20 ] && [ "$SO" -eq 0 ] 2>/dev/null; then
    echo "  Sistema OK"
fi
```

**Predicción:**

<details><summary>¿Qué limitaciones tiene vmstat para diagnóstico?</summary>

vmstat da una vista **global** del sistema pero no identifica **qué proceso** causa el problema. Si ves us=90%, sabes que la CPU está ocupada por aplicaciones pero no cuál. Para eso necesitas `top`/`htop` (por proceso) o `pidstat`. Si ves wa=50%, sabes que el disco es el cuello de botella pero no qué disco ni qué proceso lo usa — para eso necesitas `iostat` (T02). Si ves thrashing, `smem` o `/proc/meminfo` dan más detalle. vmstat es la herramienta de **primer vistazo** que te dice **dónde** está el problema; luego usas herramientas específicas para identificar **qué** lo causa.
</details>
