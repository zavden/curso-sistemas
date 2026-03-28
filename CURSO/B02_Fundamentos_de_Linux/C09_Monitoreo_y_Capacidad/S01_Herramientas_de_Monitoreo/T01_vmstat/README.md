# T01 — vmstat

## Qué es vmstat

vmstat (Virtual Memory Statistics) muestra un resumen en tiempo real de
la actividad del sistema: procesos, memoria, swap, I/O de disco, y CPU.
Es una de las primeras herramientas que un sysadmin ejecuta para
diagnosticar problemas de rendimiento:

```bash
vmstat
# procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
#  r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
#  1  0      0 2048000 128000 1024000  0    0     5    10  100  200 10  3 85  2  0
```

## Sintaxis

```bash
# Snapshot único (valores desde el boot):
vmstat

# Monitoreo continuo (intervalo en segundos):
vmstat 2          # cada 2 segundos (infinito)
vmstat 2 10       # cada 2 segundos, 10 muestras

# IMPORTANTE: la primera línea de vmstat siempre es un PROMEDIO
# desde el boot. Las líneas siguientes son datos del intervalo
```

## Interpretación de columnas

### procs — Procesos

```bash
# r = Run queue
#     Procesos en ejecución o esperando CPU
#     Si r > número de CPUs → CPU saturada
#
# b = Blocked
#     Procesos bloqueados esperando I/O (disco, red)
#     Si b > 0 sostenido → problema de I/O

# Ejemplo:
#  r  b
#  1  0     ← normal: 1 proceso corriendo, 0 bloqueados
# 12  0     ← CPU saturada: 12 procesos esperan CPU
#  1  5     ← I/O problem: 5 procesos esperan disco
# 15  8     ← ambos saturados
```

```bash
# ¿Cuántas CPUs tienes?
nproc
# 4

# Si r > 4 consistentemente → necesitas más CPU o optimizar
# Si r < 4 → hay capacidad de CPU disponible
```

### memory — Memoria (en KB)

```bash
# swpd = Swap used
#     Memoria en swap (disco). Si > 0, la RAM no fue suficiente
#     en algún momento (pero puede ser swap viejo no reclamado)
#
# free = Free memory
#     RAM libre (no usada). Linux intenta usar TODA la RAM para cache,
#     así que free bajo NO significa problema
#
# buff = Buffers
#     Memoria usada como buffers de I/O del kernel
#     Metadata de filesystem, entradas de directorio
#
# cache = Cache
#     Memoria usada como page cache (archivos leídos del disco)
#     Se libera automáticamente si se necesita

# Ejemplo:
#   swpd   free    buff   cache
#      0  2048000 128000 1024000  ← sin swap, 2GB free, 1GB cache
# 512000   64000  32000  256000  ← 512MB en swap, poca RAM libre
```

```bash
# La memoria "disponible" real es:
# free + buff + cache (aproximadamente)
# Porque buff y cache se liberan bajo presión de memoria

# Para ver la memoria disponible real, usar free (T04):
free -h
# available es la columna que importa
```

### swap — Actividad de swap

```bash
# si = Swap In
#     KB/s leídos desde swap (disco → RAM)
#     El sistema está recuperando páginas del swap
#
# so = Swap Out
#     KB/s escritos a swap (RAM → disco)
#     El sistema está moviendo páginas a disco por falta de RAM

# Ejemplo:
#  si   so
#   0    0    ← sin actividad de swap (ideal)
#   0  500    ← escribiendo 500KB/s a swap (presión de memoria)
# 200  300    ← lectura Y escritura de swap (thrashing — MUY malo)

# si Y so altos simultáneamente = THRASHING
# El sistema pasa más tiempo moviendo páginas que ejecutando trabajo
# Solución: agregar RAM o reducir carga
```

### io — Actividad de disco

```bash
# bi = Blocks In
#     Bloques/s leídos desde dispositivos de bloque (disco → RAM)
#
# bo = Blocks Out
#     Bloques/s escritos a dispositivos de bloque (RAM → disco)

# Ejemplo:
#   bi    bo
#    5    10    ← I/O bajo (normal en un servidor idle)
# 5000    50    ← lectura intensa (base de datos leyendo)
#   10  8000    ← escritura intensa (backup, logging)
# 5000  5000    ← I/O alto bidireccional (copia de archivos)

# Un bloque = 1024 bytes (1 KB) en la mayoría de sistemas
```

### system — Actividad del sistema

```bash
# in = Interrupts
#     Interrupciones por segundo (hardware + software)
#     Incluye timer, disco, red, etc.
#
# cs = Context Switches
#     Cambios de contexto por segundo
#     El kernel cambia de un proceso a otro

# Ejemplo:
#   in    cs
#  100   200    ← sistema tranquilo
# 5000 15000    ← sistema ocupado (normal bajo carga)
# 50000 100000  ← posible problema (demasiados context switches)

# Context switches altos pueden indicar:
# - Demasiados procesos compitiendo por CPU
# - Locks contention (muchos threads esperando)
# - Interrupciones de red excesivas (DDoS, storm)
```

### cpu — Uso de CPU (porcentaje)

```bash
# us = User
#     Tiempo en código de usuario (aplicaciones)
#
# sy = System
#     Tiempo en código del kernel (syscalls, drivers)
#
# id = Idle
#     Tiempo ocioso (sin nada que hacer)
#
# wa = Wait I/O
#     Tiempo esperando I/O de disco
#     CPU ociosa PERO un proceso está esperando disco
#
# st = Stolen
#     Tiempo robado por el hipervisor (en VMs)
#     La VM quería CPU pero el hipervisor se la dio a otra VM

# Los 5 valores suman ~100%

# Ejemplo:
# us sy id wa st
# 10  3 85  2  0    ← sistema tranquilo (85% idle)
# 85  5  5  5  0    ← CPU ocupada por aplicaciones
# 10 60  5 25  0    ← mucho kernel + I/O wait (problema de disco)
#  5  2 90  3  0    ← idle (sin carga)
# 30  5 10  5 50    ← VM con 50% stolen (hipervisor saturado)
```

```bash
# Patrones de diagnóstico:
# us alto + id bajo      → aplicación consume CPU (optimizar código)
# sy alto                → demasiadas syscalls o I/O del kernel
# wa alto                → disco lento o I/O excesivo
# st alto                → VM sin recursos (hablar con el proveedor)
# us+sy alto + r > nproc → necesitas más CPU
```

## Opciones útiles

```bash
# Unidades legibles (-S):
vmstat -S M 2       # mostrar en MB en lugar de KB
vmstat -S m 2       # mostrar en MB (minúscula = 1000, mayúscula = 1024)

# Estadísticas de disco (-d):
vmstat -d
# disk- -----------reads------------ -----------writes----------- -----IO------
# sda    total  merged  sectors  ms   total  merged  sectors  ms  cur  sec

# Estadísticas de disco por partición (-p):
vmstat -p sda1

# Modo wide (-w) para columnas más anchas:
vmstat -w 2
# Evita que los números se peguen cuando son grandes

# Timestamp (-t):
vmstat -t 2
# Agrega timestamp a cada línea

# Estadísticas del slab allocator (-m):
vmstat -m | head -10
# Cache del kernel (memory pools)

# Estadísticas activo/inactivo (-a):
vmstat -a 2
# Reemplaza buff/cache con inact/active
# Muestra qué memoria se usa activamente
```

## Patrones de uso

### Monitoreo rápido de CPU

```bash
# ¿El servidor está sobrecargado?
vmstat 1 5
# Mirar: r (run queue), us+sy (CPU), wa (I/O wait)

# Si r > nproc Y id < 5% → CPU saturada
# Si wa > 20% → disco es el cuello de botella
```

### Detectar presión de memoria

```bash
# ¿El sistema está swappeando?
vmstat 1 5
# Mirar: si, so (swap), swpd, free

# si/so > 0 sostenido → la RAM es insuficiente
# swpd > 0 pero si/so = 0 → hubo swap en el pasado pero ya no
```

### Antes y durante un problema

```bash
# Capturar baseline (sistema normal):
vmstat 5 > /tmp/vmstat-baseline.txt &
# Dejar corriendo, luego comparar con la captura durante el problema

# Durante el incidente:
vmstat 1 60 | tee /tmp/vmstat-incident.txt
# 1 segundo de intervalo, 60 muestras
```

### En scripts de monitoreo

```bash
# Alerta si I/O wait supera 30%:
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
```

---

## Ejercicios

### Ejercicio 1 — Leer vmstat

```bash
# 1. Ejecutar vmstat con intervalo de 2 segundos, 5 muestras:
vmstat 2 5

# 2. ¿Cuánto idle tiene tu sistema? ¿Hay I/O wait?
# 3. ¿La primera línea es diferente de las demás? ¿Por qué?
```

### Ejercicio 2 — Interpretar escenarios

```bash
# ¿Qué indica cada salida?
# a)  r  b  ... si  so  ... us sy id wa st
#    15  0       0   0      90  5  3  2  0

# b)  r  b  ... si  so  ... us sy id wa st
#     1  8       0   0       5  3 40 52  0

# c)  r  b  ... si  so  ... us sy id wa st
#     2  0     200 500       5  2 90  3  0
```

### Ejercicio 3 — Monitoreo con timestamp

```bash
# Capturar 10 segundos de actividad con timestamp:
vmstat -t 1 10
```
