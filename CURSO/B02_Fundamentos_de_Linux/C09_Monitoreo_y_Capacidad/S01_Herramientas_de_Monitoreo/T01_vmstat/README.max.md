# T01 — vmstat

> **Objetivo:** Dominar `vmstat` para diagnosticar problemas de rendimiento en CPU, memoria, swap e I/O. Entender qué significan cada una de sus columnas.

## Qué es vmstat

**vmstat** (Virtual Memory Statistics) muestra un resumen en tiempo real de la actividad del sistema: procesos, memoria, swap, I/O de disco y CPU. Es la primera herramienta que un sysadmin ejecuta para diagnosticar problemas de rendimiento.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           VMSTAT OUTPUT                                     │
│                                                                              │
│  procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----  │
│    r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st  │
│    1  0      0 2048000 128000 1024000  0    0     5    10  100  200 10  3 85  2  0  │
│                                                                              │
│  └──procesos──┘  └─────memoria─────┘  └swap┘ └─I/O──┘ └system┘ └──CPU──┘         │
└─────────────────────────────────────────────────────────────────────────────┘
```

```bash
# Snapshot único (promedio desde el boot):
vmstat

# Monitoreo continuo:
vmstat 2          # cada 2 segundos (infinito)
vmstat 2 10       # cada 2 segundos, 10 muestras

# ⚠️ IMPORTANTE: La primera línea SIEMPRE es un PROMEDIO desde el boot
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

# La memoria "disponible" real es:
# free + buff + cache

# Ejemplo de salida:
#   swpd   free    buff   cache
#      0 2048000 128000 1024000  → sin swap, 2GB free, 1GB cache
# 512000   64000  32000  256000  → 512MB en swap, poca RAM libre

# Para ver la memoria real:
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

⚠️  si Y so altos simultáneamente = THRASHING
    El sistema pasa más tiempo MOVIENDO páginas que TRABAJANDO
    Solución: agregar RAM o reducir carga
```

### io — Actividad de disco

| Columna | Nombre | Descripción |
|---------|--------|-------------|
| `bi` | Blocks In | KB/s leídos desde dispositivos de bloque |
| `bo` | Blocks Out | KB/s escritos a dispositivos de bloque |

```
 bi    bo
  5    10    → I/O bajo (servidor idle normal)
5000    50    → lectura intensa (base de datos leyendo)
  10  8000    → escritura intensa (backup, logging)
5000  5000    → I/O alto bidireccional (copia de archivos)

Un bloque = 1024 bytes (1 KB) en la mayoría de sistemas
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
us alto + id bajo     → aplicación consume CPU (optimizar código)
sy alto               → demasiadas syscalls o I/O del kernel
wa alto               → disco lento o I/O excesivo
st alto               → VM sin recursos (hablar con el proveedor)
us+sy alto + r > nproc → necesitas más CPU
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

---

## vmstat en scripts de monitoreo

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
  vmstat N M         → cada N segundos, M muestras

OPCIONES:
  -S [kKmM]     → unidades (KB/KB/MB/MB)
  -d            → estadísticas de disco
  -p PART       → estadísticas por partición
  -w            → modo wide
  -t            → con timestamp
  -m            → slab allocator
  -a            → memoria active/inactive

COLUMNAS CLAVE:
  r > nproc     → CPU saturada
  b > 0         → problema de I/O
  si+so > 0    → presión de memoria (thrashing si ambos)
  wa > 20%     → disco es el cuello de botella
  st > 10%     → VM sin recursos (hipervisor)
```

---

## Ejercicios

### Ejercicio 1 — Leer vmstat en tu sistema

```bash
# 1. Ejecutar vmstat con 5 muestras de 2 segundos:
vmstat 2 5

# 2. Observar:
#    - ¿Cuántos procesos hay en run queue (r)?
#    - ¿Cuánto idle tiene (id)?
#    - ¿Hay I/O wait (wa)?
#    - ¿Hay swap activity (si/so)?

# 3. Comparar la primera línea con las demás:
#    ¿Son diferentes? ¿Por qué?
```

### Ejercicio 2 — Interpretar escenarios

```bash
# ¿Qué indica cada salida?

# a)正常运行:
#  r  b   swpd   free   buff   cache   si   so   bi   bo   in   cs  us  sy  id  wa  st
#  1  0      0 2048000 128000 1024000   0    0    5   10  100  200  10   3  85   2   0

# b)CPU饱和:
# 15  0      0 1024000  64000  512000   0    0    0    0  500  800  90   5   3   2   0

# c)I/O瓶颈:
#  1  8      0  256000  32000  512000   0    0 5000   50 1000 2000   5   3  40  52   0

# d)Thrashing:
#  5  3   512000  64000  32000  256000 200  300    0    0  200  400   5   2  90   3   0

# e)VM资源不足:
#  2  0      0 2048000 128000 1024000   0    0    5   10  100  200  30   5  10   5  50
```

### Ejercicio 3 — Monitoreo con timestamp

```bash
# Capturar 20 segundos con timestamp:
vmstat -t 1 20

# Analizar la salida:
# - ¿En qué momento hubo más actividad?
# - ¿Hubo algún pico de I/O wait?
# - ¿El run queue se mantuvo estable?
```

### Ejercicio 4 — Scripts de monitoreo

```bash
# Crear un script de monitoreo:
cat << 'EOF' > /usr/local/bin/check-system.sh
#!/bin/bash
# Monitoreo rápido del sistema

echo "=== VMSTAT (2 samples) ==="
vmstat 1 2

echo ""
echo "=== Run queue vs CPUs ==="
CPUS=$(nproc)
RQ=$(vmstat 1 2 | tail -1 | awk '{print $1}')
echo "CPUs: $CPUS, Run queue: $RQ"
if [ "$RQ" -gt "$CPUS" ]; then
    echo "⚠️  ALERTA: Run queue excede CPUs"
fi

echo ""
echo "=== I/O Wait ==="
WA=$(vmstat 1 2 | tail -1 | awk '{print $16}')
echo "I/O Wait: ${WA}%"
if [ "$WA" -gt 20 ]; then
    echo "⚠️  ALERTA: I/O wait alto (>20%)"
fi

echo ""
echo "=== Swap Activity ==="
SI=$(vmstat 1 2 | tail -1 | awk '{print $7}')
SO=$(vmstat 1 2 | tail -1 | awk '{print $8}')
echo "Swap In: $SI, Swap Out: $SO"
if [ "$SI" -gt 0 ] && [ "$SO" -gt 0 ]; then
    echo "⚠️  ALERTA: THRASHING detectado"
fi
EOF
chmod +x /usr/local/bin/check-system.sh

# Ejecutar:
/usr/local/bin/check-system.sh
```

### Ejercicio 5 — Comparar con y sin carga

```bash
# 1. Capturar baseline (sin carga):
vmstat 1 5 > /tmp/vmstat-idle.txt &

# 2. Generar carga artificial (si tienes time):
# dd if=/dev/zero of=/tmp/test bs=1M count=1000 &
# O:
# tar -cf /dev/null /usr 2>/dev/null &

# 3. Capturar durante carga:
sleep 2
vmstat 1 5 > /tmp/vmstat-loaded.txt &

# 4. Comparar:
echo "=== IDLE ==="
cat /tmp/vmstat-idle.txt
echo "=== LOADED ==="
cat /tmp/vmstat-loaded.txt
```
