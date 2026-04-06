# T01 - perf en Linux: perf stat (contadores), perf record + perf report (sampling), perf annotate (instrucciones)

## Índice

1. [De benchmarking a profiling](#1-de-benchmarking-a-profiling)
2. [Qué es perf y por qué es fundamental](#2-qué-es-perf-y-por-qué-es-fundamental)
3. [Arquitectura de perf: kernel, PMU, y userspace](#3-arquitectura-de-perf-kernel-pmu-y-userspace)
4. [Instalación de perf en distintas distribuciones](#4-instalación-de-perf-en-distintas-distribuciones)
5. [Permisos: perf_event_paranoid](#5-permisos-perf_event_paranoid)
6. [Contadores de hardware (PMU counters)](#6-contadores-de-hardware-pmu-counters)
7. [perf stat: uso básico](#7-perf-stat-uso-básico)
8. [Eventos predefinidos de perf stat](#8-eventos-predefinidos-de-perf-stat)
9. [Eventos personalizados con -e](#9-eventos-personalizados-con--e)
10. [Eventos de hardware vs software](#10-eventos-de-hardware-vs-software)
11. [Métricas derivadas: IPC, branch-miss rate, cache-miss rate](#11-métricas-derivadas-ipc-branch-miss-rate-cache-miss-rate)
12. [perf stat -d: niveles de detalle](#12-perf-stat--d-niveles-de-detalle)
13. [perf stat con múltiples ejecuciones: -r y estabilidad](#13-perf-stat-con-múltiples-ejecuciones--r-y-estabilidad)
14. [perf stat por CPU o por thread](#14-perf-stat-por-cpu-o-por-thread)
15. [perf record: introducción al sampling profiler](#15-perf-record-introducción-al-sampling-profiler)
16. [Cómo funciona el sampling: interrupciones periódicas](#16-cómo-funciona-el-sampling-interrupciones-periódicas)
17. [perf record: frecuencia vs período](#17-perf-record-frecuencia-vs-período)
18. [Call graphs: -g, --call-graph fp/dwarf/lbr](#18-call-graphs--g---call-graph-fpdwarflbr)
19. [perf report: navegación interactiva](#19-perf-report-navegación-interactiva)
20. [Symbols: por qué necesitas debug info](#20-symbols-por-qué-necesitas-debug-info)
21. [perf annotate: instrucciones anotadas](#21-perf-annotate-instrucciones-anotadas)
22. [perf script: export de samples raw](#22-perf-script-export-de-samples-raw)
23. [perf top: profiling en vivo](#23-perf-top-profiling-en-vivo)
24. [Profiling de Rust con perf](#24-profiling-de-rust-con-perf)
25. [Profiling del kernel y syscalls](#25-profiling-del-kernel-y-syscalls)
26. [Interpretación: identificar hotspots reales](#26-interpretación-identificar-hotspots-reales)
27. [Overhead de perf y cuándo afecta](#27-overhead-de-perf-y-cuándo-afecta)
28. [Errores comunes al usar perf](#28-errores-comunes-al-usar-perf)
29. [Programa de práctica: profile_target](#29-programa-de-práctica-profile_target)
30. [Ejercicios](#30-ejercicios)

---

## 1. De benchmarking a profiling

La Sección 1 enseñó a **medir**: "¿cuánto tarda esta función?". Esta Sección 2 enseña a **diagnosticar**: "¿por qué tarda lo que tarda, y dónde está el tiempo?".

```
┌──────────────────────────────────────────────────────────────────────┐
│            Benchmarking vs Profiling                                 │
│                                                                      │
│  Benchmarking:                                                       │
│  - Responde: "¿CUÁNTO tarda?"                                        │
│  - Unidad: nanosegundos, operaciones/segundo                         │
│  - Entrega: un número estadísticamente robusto                       │
│  - Herramientas: Criterion, bench.h, hyperfine                       │
│  - Cuándo usar: comparar alternativas, detectar regresiones          │
│                                                                      │
│  Profiling:                                                          │
│  - Responde: "¿DÓNDE está el tiempo?"                                │
│  - Unidad: % del tiempo, contadores de hardware                      │
│  - Entrega: lista de funciones/líneas ordenadas por coste            │
│  - Herramientas: perf, Valgrind/Callgrind, flamegraphs, heaptrack    │
│  - Cuándo usar: optimizar, diagnosticar lentitud, entender código    │
│                                                                      │
│  Flujo típico:                                                       │
│  ┌──────────────────────────────────────────────┐                    │
│  │ 1. Benchmark detecta: "es lento"             │                    │
│  │ 2. Profiling identifica: "porque hace X"     │                    │
│  │ 3. Optimización arregla: "reduce X"          │                    │
│  │ 4. Benchmark valida: "ahora es rápido"       │                    │
│  └──────────────────────────────────────────────┘                    │
└──────────────────────────────────────────────────────────────────────┘
```

**Regla de oro**: nunca optimices sin profiling. Como dice Knuth, "premature optimization is the root of all evil". El profiling te dice **dónde** está realmente el cuello de botella, que raramente coincide con tu intuición.

---

## 2. Qué es perf y por qué es fundamental

`perf` (también llamado `perf_events` o `perftools`) es el profiler oficial de Linux. Está **integrado en el kernel** y es la herramienta estándar para análisis de rendimiento en Linux.

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Características de perf                             │
│                                                                      │
│  Parte del kernel: no es una herramienta externa                     │
│    → fuente en linux/tools/perf/                                     │
│    → se compila junto con el kernel                                  │
│    → cada versión del kernel tiene su perf correspondiente           │
│                                                                      │
│  Acceso a PMU (Performance Monitoring Unit):                         │
│    Unidad de hardware en cada CPU moderno que cuenta                 │
│    eventos de bajo nivel: ciclos, instrucciones, cache misses,       │
│    branch mispredictions, etc.                                        │
│                                                                      │
│  Cuatro modos principales:                                            │
│  ─────────────────────────                                           │
│  1. perf stat       → contadores agregados (cuánto)                  │
│  2. perf record     → sampling (dónde)                               │
│  3. perf top        → profiling en vivo                              │
│  4. perf trace      → tracing de syscalls                            │
│                                                                      │
│  Ventajas frente a otras herramientas:                               │
│  - Overhead muy bajo (~1-3%)                                         │
│  - Sin instrumentación del código                                    │
│  - Funciona con binarios ya compilados                                │
│  - Acceso directo a PMU                                              │
│  - Compatible con binarios strippeds                                  │
│  - Profile del sistema entero (kernel + userspace)                   │
│                                                                      │
│  Limitaciones:                                                        │
│  - Solo Linux                                                        │
│  - Requiere permisos (perf_event_paranoid)                           │
│  - Algunas funcionalidades requieren kernel reciente                 │
│  - PMU events varían entre CPUs (Intel, AMD, ARM)                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Arquitectura de perf: kernel, PMU, y userspace

```
┌──────────────────────────────────────────────────────────────────────┐
│                 Stack de perf                                         │
│                                                                      │
│  ┌────────────────────────────────────────────────────────┐          │
│  │           Userspace                                    │          │
│  │  ┌──────────────────────────────────────────────────┐  │          │
│  │  │  perf (CLI tool)                                 │  │          │
│  │  │  perf stat / record / report / annotate / ...    │  │          │
│  │  └──────────────────────────────────────────────────┘  │          │
│  │                        │                               │          │
│  │                        ▼                               │          │
│  │  ┌──────────────────────────────────────────────────┐  │          │
│  │  │  libperf / sys_perf_event_open(2) syscall        │  │          │
│  │  └──────────────────────────────────────────────────┘  │          │
│  └────────────────────────────────────────────────────────┘          │
│                            │                                         │
│                            ▼                                         │
│  ┌────────────────────────────────────────────────────────┐          │
│  │           Kernel                                       │          │
│  │  ┌──────────────────────────────────────────────────┐  │          │
│  │  │  perf subsystem                                  │  │          │
│  │  │  kernel/events/core.c                            │  │          │
│  │  │  - Programa los contadores                        │  │          │
│  │  │  - Maneja interrupciones                          │  │          │
│  │  │  - Asocia samples con procesos                    │  │          │
│  │  │  - Escribe al ring buffer en memoria              │  │          │
│  │  └──────────────────────────────────────────────────┘  │          │
│  │                        │                               │          │
│  │                        ▼                               │          │
│  │  ┌──────────────────────────────────────────────────┐  │          │
│  │  │  PMU Drivers                                      │  │          │
│  │  │  arch/x86/events/{intel,amd}/core.c               │  │          │
│  │  │  - Traduce eventos abstractos a MSRs              │  │          │
│  │  │  - Lee contadores específicos del CPU             │  │          │
│  │  └──────────────────────────────────────────────────┘  │          │
│  └────────────────────────────────────────────────────────┘          │
│                            │                                         │
│                            ▼                                         │
│  ┌────────────────────────────────────────────────────────┐          │
│  │           Hardware (CPU)                               │          │
│  │  ┌──────────────────────────────────────────────────┐  │          │
│  │  │  Performance Monitoring Unit (PMU)               │  │          │
│  │  │  - ~8 contadores fijos (cycles, instructions)    │  │          │
│  │  │  - 4-8 contadores programables                   │  │          │
│  │  │  - Contadores de uncore (L3, memory controller)  │  │          │
│  │  │  - Precise Event Based Sampling (PEBS)           │  │          │
│  │  │  - Last Branch Record (LBR)                       │  │          │
│  │  └──────────────────────────────────────────────────┘  │          │
│  └────────────────────────────────────────────────────────┘          │
└──────────────────────────────────────────────────────────────────────┘
```

### La PMU: el corazón del profiling

Cada CPU moderno tiene una **Performance Monitoring Unit** con registros dedicados para contar eventos de bajo nivel. Estos contadores son de hardware: el CPU los incrementa sin overhead de software.

```
┌──────────────────────────────────────────────────────────────────────┐
│              Estructura típica de una PMU (Intel Skylake+)           │
│                                                                      │
│  Contadores fijos (siempre cuentan lo mismo):                        │
│  ┌─────────────────────────────────────────────┐                     │
│  │ FixCtr0: Instructions retired               │                     │
│  │ FixCtr1: Core cycles (unhalted)             │                     │
│  │ FixCtr2: Reference cycles                    │                     │
│  │ FixCtr3: Topdown slots                       │                     │
│  └─────────────────────────────────────────────┘                     │
│                                                                      │
│  Contadores programables (4-8 por core):                             │
│  ┌─────────────────────────────────────────────┐                     │
│  │ PMC0, PMC1, PMC2, PMC3 (4 en la mayoría)   │                     │
│  │ Cada uno puede contar CUALQUIER evento       │                     │
│  │ programable vía MSRs (IA32_PERFEVTSEL*)     │                     │
│  └─────────────────────────────────────────────┘                     │
│                                                                      │
│  Con Hyperthreading: los contadores se                                │
│  comparten entre los 2 hyperthreads del mismo core.                   │
│  Si activas HT y quieres contadores completos,                        │
│  hay que multiplexar.                                                 │
│                                                                      │
│  Cuando pides más eventos que contadores físicos,                    │
│  perf multiplexa: activa unos durante X tiempo,                      │
│  otros durante Y tiempo, y escala los resultados.                     │
│  Esto introduce error estadístico.                                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. Instalación de perf en distintas distribuciones

`perf` no siempre viene instalado por defecto. La instalación depende de la distribución:

```bash
# ═══════════════════════════════════════════════════════════
# Fedora / RHEL / CentOS / Rocky / Alma
# ═══════════════════════════════════════════════════════════
sudo dnf install perf

# La versión de perf corresponde a la versión del kernel.
# Verificar con:
perf --version
uname -r

# ═══════════════════════════════════════════════════════════
# Debian / Ubuntu
# ═══════════════════════════════════════════════════════════
# Debian/Ubuntu instala perf con un wrapper que apunta
# a la versión del kernel actual.
sudo apt install linux-tools-generic
# O específicamente para tu kernel:
sudo apt install linux-tools-$(uname -r) linux-tools-generic

# ═══════════════════════════════════════════════════════════
# Arch Linux
# ═══════════════════════════════════════════════════════════
sudo pacman -S perf

# ═══════════════════════════════════════════════════════════
# Alpine (musl)
# ═══════════════════════════════════════════════════════════
sudo apk add perf

# ═══════════════════════════════════════════════════════════
# Compilar desde el source del kernel (avanzado)
# ═══════════════════════════════════════════════════════════
cd /usr/src/linux-$(uname -r)/tools/perf
make
sudo make install
```

### Verificar la instalación

```bash
# Ver la versión
perf --version
# perf version 6.11.3

# Listar subcomandos disponibles
perf help

# Ver si la PMU funciona
perf stat sleep 1
# Debería mostrar algo como:
# Performance counter stats for 'sleep 1':
#           0.50 msec task-clock
#              1      context-switches
#              0      cpu-migrations
#             62      page-faults
#        ...
#    1.001 seconds time elapsed
```

---

## 5. Permisos: perf_event_paranoid

Linux controla quién puede usar `perf` mediante el parámetro `kernel.perf_event_paranoid`:

```
┌──────────────────────────────────────────────────────────────────────┐
│       Niveles de perf_event_paranoid                                 │
│                                                                      │
│  Valor  Descripción                                                  │
│  ─────  ───────────                                                  │
│   -1    Sin restricciones.                                           │
│         Usuarios sin root pueden acceder a todos los eventos.         │
│         (Requiere capability CAP_PERFMON en kernel >=5.8)            │
│                                                                      │
│    0    Acceso a CPU y kernel events.                                │
│         Requiere CAP_SYS_ADMIN.                                      │
│         Puede ver eventos del kernel y otros procesos del usuario.   │
│                                                                      │
│    1    Sin acceso a kernel events.                                  │
│         Puede medir solo procesos del usuario actual.                │
│         Typical default en kernels antiguos.                         │
│                                                                      │
│    2    Solo hardware events del proceso propio.                     │
│         No puede hacer sampling de otros procesos.                   │
│         Default en kernels recientes (seguro por defecto).           │
│                                                                      │
│    3    Sin acceso al subsistema de perf.                           │
│         Bloqueo total (raro, solo en sistemas endurecidos).          │
└──────────────────────────────────────────────────────────────────────┘
```

### Ver el valor actual

```bash
# Leer el valor actual
cat /proc/sys/kernel/perf_event_paranoid

# Cambiar temporalmente (hasta reboot)
sudo sysctl kernel.perf_event_paranoid=1

# Cambiar permanentemente
echo 'kernel.perf_event_paranoid = 1' | sudo tee -a /etc/sysctl.d/99-perf.conf
sudo sysctl -p /etc/sysctl.d/99-perf.conf
```

### Error típico de permisos

```bash
$ perf record ./my_program
Error:
Access to performance monitoring and observability operations is limited.
Consider adjusting /proc/sys/kernel/perf_event_paranoid setting to open
access to performance monitoring and observability operations for processes
without CAP_PERFMON, CAP_SYS_PTRACE or CAP_SYS_ADMIN Linux capability.
...
```

### Alternativa segura: ejecutar con sudo

```bash
# Usar sudo respeta el valor pero te da los privilegios
sudo perf record ./my_program
sudo perf report
# Los archivos perf.data quedan con permisos de root.
# Para que tu usuario pueda leerlos después:
sudo chown $USER:$USER perf.data
```

---

## 6. Contadores de hardware (PMU counters)

Los contadores de hardware miden eventos a nivel de instrucción sin overhead de software. Los más importantes son:

```
┌──────────────────────────────────────────────────────────────────────┐
│            Contadores esenciales para profiling                      │
│                                                                      │
│  cycles                                                              │
│  ──────                                                              │
│  Ciclos de CPU ejecutados. Varía con la frecuencia.                  │
│  Unidad fundamental para medir "cuánto trabajó" el CPU.              │
│                                                                      │
│  instructions                                                        │
│  ────────────                                                        │
│  Instrucciones retiradas (completadas exitosamente).                 │
│  Excluye instrucciones especulativas que fueron descartadas.         │
│                                                                      │
│  ref-cycles                                                          │
│  ──────────                                                          │
│  Ciclos en frecuencia de referencia (no cambia con turbo boost).     │
│  Útil para normalizar medidas entre cores a diferentes frecuencias.  │
│                                                                      │
│  cache-references                                                    │
│  ────────────────                                                    │
│  Accesos al último nivel de cache (típicamente L3).                  │
│                                                                      │
│  cache-misses                                                        │
│  ────────────                                                        │
│  Fallos en el último nivel de cache (accesos que van a DRAM).        │
│                                                                      │
│  branches                                                            │
│  ────────                                                            │
│  Instrucciones de branch (jumps, calls, returns) ejecutadas.         │
│                                                                      │
│  branch-misses                                                       │
│  ─────────────                                                       │
│  Branches cuya predicción fue incorrecta.                            │
│                                                                      │
│  bus-cycles                                                          │
│  ──────────                                                          │
│  Ciclos del bus externo (acceso a memoria, I/O).                     │
│                                                                      │
│  page-faults                                                         │
│  ───────────                                                         │
│  Page faults (soft y hard). No es hardware strictly, pero útil.      │
│                                                                      │
│  context-switches                                                    │
│  ────────────────                                                    │
│  Cambios de contexto entre procesos o threads.                       │
│                                                                      │
│  cpu-migrations                                                      │
│  ──────────────                                                      │
│  Veces que el proceso migró a otro CPU (signo de mal scheduling).    │
└──────────────────────────────────────────────────────────────────────┘
```

### Contadores avanzados (solo algunos CPUs)

```
┌──────────────────────────────────────────────────────────────────────┐
│          Contadores avanzados (Intel)                                │
│                                                                      │
│  L1-dcache-loads              Accesos a L1 data cache                │
│  L1-dcache-load-misses        Fallos en L1 data cache                │
│  L1-dcache-stores             Escrituras a L1 data cache             │
│                                                                      │
│  LLC-loads                    Accesos al Last Level Cache            │
│  LLC-load-misses              Fallos en LLC (van a DRAM)             │
│  LLC-stores                   Escrituras al LLC                      │
│                                                                      │
│  dTLB-loads                   Accesos al dTLB                        │
│  dTLB-load-misses             Fallos en dTLB (page walks)            │
│  dTLB-stores                  Escrituras al dTLB                     │
│                                                                      │
│  iTLB-loads                   Accesos al iTLB                        │
│  iTLB-load-misses             Fallos en iTLB                         │
│                                                                      │
│  node-loads                   Accesos a otro NUMA node               │
│  node-load-misses             Fallos que cruzan NUMA boundary        │
│                                                                      │
│  mem_inst_retired.all_loads   Cargas de memoria (más preciso)        │
│  mem_inst_retired.all_stores  Escrituras de memoria                  │
│                                                                      │
│  Ver todos los eventos disponibles:                                  │
│  perf list                                                           │
│  perf list hw                                                        │
│  perf list cache                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 7. perf stat: uso básico

`perf stat` ejecuta un programa y muestra contadores agregados al final. Es la forma más simple de ver el rendimiento de un programa.

```bash
# Uso básico: medir un comando
perf stat ./mi_programa

# Medir una tarea del sistema
perf stat ls -R /usr
perf stat sleep 1
perf stat dd if=/dev/zero of=/dev/null count=1000000
```

### Salida típica

```
$ perf stat ./my_program

 Performance counter stats for './my_program':

          1,234.56 msec task-clock                #    0.998 CPUs utilized
                12      context-switches          #    9.719 /sec
                 0      cpu-migrations            #    0.000 /sec
             1,234      page-faults               #  999.635 /sec
     3,456,789,012      cycles                    #    2.800 GHz
     8,765,432,109      instructions              #    2.54  insn per cycle
     1,234,567,890      branches                  #  999.967 M/sec
         1,234,567      branch-misses             #    0.10% of all branches

       1.236789012 seconds time elapsed

       1.198765432 seconds user
       0.032345678 seconds sys
```

### Desglose línea por línea

```
task-clock
──────────
Tiempo total de CPU (suma de todos los cores que usaste).
"CPUs utilized" = task-clock / wall-clock
Si < 1.0: el programa estuvo esperando (I/O, locks, sleep)
Si > 1.0: el programa es multi-threaded

context-switches
────────────────
Cambios de contexto. Muchos indican:
- Contention por locks
- Llamadas a sleep/wait
- Scheduling activo

cpu-migrations
──────────────
Veces que el kernel movió tu proceso a otro CPU.
Idealmente: 0 (usar taskset para pinning)

page-faults
───────────
Accesos a memoria que requirieron mapear nuevas páginas.
Soft faults: memoria ya en RAM, solo mapear
Hard faults: leer de disco (malos para rendimiento)

cycles
──────
Ciclos totales de CPU consumidos.

instructions
────────────
Instrucciones completadas. Incluye todas las ejecutadas
(no solo las no especulativas).

insn per cycle (IPC)
───────────────────
La métrica más importante.
- IPC < 0.5: código memory-bound (esperando cache/DRAM)
- IPC 0.5-1.5: código mixto (algo de memoria, algo de cómputo)
- IPC 1.5-3.0: código bien optimizado, CPU-bound
- IPC 3.0-4.0: código muy bien optimizado (SIMD, pipeline lleno)
- IPC > 4.0: raro, indica uso masivo de SIMD/AVX

branches
────────
Branches ejecutados. Valor absoluto poco informativo.

branch-misses
─────────────
% de branches predichos incorrectamente.
- < 0.5%: patrones muy predecibles (posible benchmark artificial)
- 0.5-2%: código normal
- 2-5%: código con patrones variables (típico en producción)
- > 5%: posible oportunidad de optimización
```

---

## 8. Eventos predefinidos de perf stat

Por defecto, `perf stat` mide un conjunto común de eventos. Puedes listarlos:

```bash
# Ver todos los eventos disponibles
perf list

# Filtrar por categoría
perf list hw          # hardware events
perf list sw          # software events
perf list cache       # cache events
perf list tracepoint  # kernel tracepoints
perf list pmu         # eventos específicos del PMU
```

### Eventos hardware estándar (predefinidos)

```
cpu-cycles OR cycles                     [Hardware event]
instructions                             [Hardware event]
cache-references                         [Hardware event]
cache-misses                             [Hardware event]
branch-instructions OR branches          [Hardware event]
branch-misses                            [Hardware event]
bus-cycles                               [Hardware event]
ref-cycles                               [Hardware event]
```

### Eventos software

```
cpu-clock                                [Software event]
task-clock                               [Software event]
page-faults OR faults                    [Software event]
context-switches OR cs                   [Software event]
cpu-migrations OR migrations             [Software event]
minor-faults                             [Software event]
major-faults                             [Software event]
alignment-faults                         [Software event]
emulation-faults                         [Software event]
```

### Eventos de cache (por nivel)

```
L1-dcache-loads                          [Hardware cache event]
L1-dcache-load-misses                    [Hardware cache event]
L1-dcache-stores                         [Hardware cache event]
L1-icache-loads                          [Hardware cache event]
L1-icache-load-misses                    [Hardware cache event]
LLC-loads                                [Hardware cache event]
LLC-load-misses                          [Hardware cache event]
LLC-stores                               [Hardware cache event]
dTLB-loads                               [Hardware cache event]
dTLB-load-misses                         [Hardware cache event]
iTLB-loads                               [Hardware cache event]
iTLB-load-misses                         [Hardware cache event]
node-loads                               [Hardware cache event]
node-load-misses                         [Hardware cache event]
```

---

## 9. Eventos personalizados con -e

Con `-e` puedes elegir exactamente qué contar:

```bash
# Un solo evento
perf stat -e cycles ./my_program

# Múltiples eventos separados por comas
perf stat -e cycles,instructions,cache-misses ./my_program

# Combinación completa para análisis profundo
perf stat -e cycles,instructions,cache-references,cache-misses,\
branches,branch-misses,dTLB-loads,dTLB-load-misses,\
L1-dcache-loads,L1-dcache-load-misses ./my_program
```

### Agrupar eventos con llaves {}

Cuando necesitas garantizar que ciertos eventos se cuenten simultáneamente (no multiplexados), usa llaves:

```bash
# Los eventos entre llaves se cuentan en el mismo contador
# y nunca se multiplexan entre sí
perf stat -e '{cycles,instructions}' ./my_program

# Útil cuando calculas IPC: quieres que cycles e instructions
# se cuenten exactamente durante el mismo período
```

### Eventos raw (específicos del CPU)

```bash
# Algunos eventos solo son accesibles por código raw
# Formato: rXXXX donde XXXX es el UMask:Event del Intel manual
perf stat -e r00c0 ./my_program  # instructions retired en Intel

# Formato PMU-specific:
perf stat -e cpu/event=0xc0,umask=0x00/ ./my_program
# event=0xc0 → instruction retired
# umask=0x00 → all instructions
```

---

## 10. Eventos de hardware vs software

```
┌──────────────────────────────────────────────────────────────────────┐
│        Hardware events vs Software events                            │
│                                                                      │
│  Hardware events:                                                    │
│  ────────────────                                                    │
│  - Provistos por la PMU del CPU                                      │
│  - Cuenta física: el CPU incrementa registros                        │
│  - Overhead: casi cero                                               │
│  - Limitación: número fijo de contadores                             │
│  - Ejemplos: cycles, instructions, cache-misses                      │
│                                                                      │
│  Software events:                                                    │
│  ────────────────                                                    │
│  - Provistos por el kernel                                           │
│  - El kernel incrementa un contador cuando detecta el evento         │
│  - Overhead: bajo pero no cero                                       │
│  - Sin límite de número                                              │
│  - Ejemplos: page-faults, context-switches, cpu-migrations           │
│                                                                      │
│  Tracepoint events:                                                  │
│  ──────────────────                                                  │
│  - Puntos de instrumentación estáticos en el kernel                  │
│  - Ejemplo: syscalls:sys_enter_read cuenta llamadas a read()         │
│  - Overhead bajo (optimizados por el kernel)                         │
│                                                                      │
│  Breakpoint events (menos usados):                                   │
│  ───────────────────────────────                                     │
│  - Puntos específicos en una función                                 │
│  - Útil para contar accesos a una variable específica                │
│  - Alto overhead                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo con tracepoints

```bash
# Contar syscalls read, write, open
perf stat -e 'syscalls:sys_enter_read,syscalls:sys_enter_write,syscalls:sys_enter_openat' \
    ./my_program

# Salida:
# syscalls:sys_enter_read:       1,234
# syscalls:sys_enter_write:         56
# syscalls:sys_enter_openat:        12
```

---

## 11. Métricas derivadas: IPC, branch-miss rate, cache-miss rate

Los valores absolutos de los contadores rara vez son útiles por sí solos. Las **métricas derivadas** son las que de verdad informan:

```
┌──────────────────────────────────────────────────────────────────────┐
│          Métricas derivadas esenciales                               │
│                                                                      │
│  IPC (Instructions Per Cycle)                                        │
│  ─────────────────────────                                           │
│  IPC = instructions / cycles                                         │
│                                                                      │
│  Interpretación:                                                     │
│  < 0.5    Severely memory-bound, lots of stalls                      │
│  0.5-1.0  Memory-bound                                               │
│  1.0-1.5  Mixed                                                      │
│  1.5-2.5  Good, CPU-bound                                            │
│  2.5-3.5  Very good, well-optimized                                  │
│  3.5-4.5  Excellent, likely SIMD/vectorized                          │
│  > 4.5    Rare, heavy SIMD/AVX-512                                   │
│                                                                      │
│  Branch miss rate                                                    │
│  ────────────────                                                    │
│  branch_miss_rate = branch-misses / branches                         │
│                                                                      │
│  Interpretación:                                                     │
│  < 0.1%   Muy predecible (posible benchmark artificial)              │
│  0.1-1%   Normal para código con patrones                            │
│  1-3%     Típico en producción                                       │
│  3-5%     Patrón variable, posible oportunidad                       │
│  > 5%     Alto: revisar branches (sorting, collections, I/O checks)  │
│                                                                      │
│  Cache miss rate (LLC)                                               │
│  ─────────────────────                                               │
│  llc_miss_rate = LLC-load-misses / LLC-loads                         │
│                                                                      │
│  Interpretación:                                                     │
│  < 1%     Datos caben en cache, acceso secuencial                    │
│  1-5%     Normal                                                     │
│  5-20%    Presión sobre el cache                                     │
│  > 20%    Memory-bound, considerar cambiar estructura de datos       │
│                                                                      │
│  L1 dcache miss rate                                                 │
│  ────────────────────                                                │
│  l1d_miss_rate = L1-dcache-load-misses / L1-dcache-loads             │
│                                                                      │
│  Interpretación:                                                     │
│  < 2%     Datos calientes en L1                                      │
│  2-10%    Normal                                                     │
│  10-30%   Presión sobre L1                                           │
│  > 30%    Datos no caben en L1, esperando L2+                        │
│                                                                      │
│  dTLB miss rate                                                      │
│  ───────────────                                                     │
│  dtlb_miss_rate = dTLB-load-misses / dTLB-loads                      │
│                                                                      │
│  Interpretación:                                                     │
│  < 0.1%   Normal                                                     │
│  0.1-1%   Presión sobre el TLB (datos grandes)                       │
│  > 1%     TLB thrashing, considerar huge pages                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo interpretado

```bash
$ perf stat -e cycles,instructions,cache-references,cache-misses,\
branches,branch-misses ./matrix_multiply

 Performance counter stats for './matrix_multiply':

    12,345,678,901      cycles
    34,567,890,123      instructions              #    2.80  insn per cycle
       123,456,789      cache-references
         5,678,912      cache-misses              #    4.60% of all cache refs
     3,456,789,012      branches
        12,345,678      branch-misses             #    0.36% of all branches

       4.416 seconds time elapsed

Interpretación:
─────────────
• IPC = 2.80 → muy bueno, bien optimizado, CPU-bound
• Cache miss = 4.6% → algo de presión sobre el cache, pero razonable
• Branch miss = 0.36% → muy bajo, patrones predecibles (correcto para matmul)
• 4.4s wall time con IPC alto → el código es compute-bound
• Si quieres acelerar: enfócate en reducir instrucciones (SIMD) o
  mejorar la localidad (blocking, cache-oblivious algorithms)
```

---

## 12. perf stat -d: niveles de detalle

La flag `-d` (detail) añade contadores adicionales automáticamente:

```bash
# Nivel 1: contadores de cache L1
perf stat -d ./my_program

# Nivel 2: añade LLC (last level cache)
perf stat -d -d ./my_program

# Nivel 3: añade más detalles (dTLB, iTLB, etc.)
perf stat -d -d -d ./my_program
```

### Salida con -d -d -d

```
$ perf stat -d -d -d ./my_program

 Performance counter stats for './my_program':

          1,234.56 msec task-clock                #    0.998 CPUs utilized
                12      context-switches          #    9.719 /sec
                 0      cpu-migrations            #    0.000 /sec
             1,234      page-faults               #  999.635 /sec
     3,456,789,012      cycles                    #    2.800 GHz
     8,765,432,109      instructions              #    2.54  insn per cycle
     1,234,567,890      branches                  #  999.967 M/sec
         1,234,567      branch-misses             #    0.10% of all branches
     2,345,678,901      L1-dcache-loads           #    1.900 G/sec
       123,456,789      L1-dcache-load-misses     #    5.26% of all L1-dcache accesses
        12,345,678      LLC-loads                 #   10.000 M/sec
         1,234,567      LLC-load-misses           #   10.00% of all LL-cache accesses
     1,234,567,890      L1-icache-loads           #    1.000 G/sec
         1,234,567      L1-icache-load-misses     #    0.10% of all L1-icache accesses
     2,345,678,901      dTLB-loads                #    1.900 G/sec
        23,456,789      dTLB-load-misses          #    1.00% of all dTLB cache accesses
     1,234,567,890      iTLB-loads                #    1.000 G/sec
         1,234,567      iTLB-load-misses          #    0.10% of all iTLB cache accesses

       1.236789012 seconds time elapsed
```

### Qué buscar en cada métrica

```
task-clock vs time elapsed
   ratio = CPUs utilized
   < 1.0: esperando algo (I/O, sleep, locks)
   1.0: bien, pero sin paralelismo
   > 1.0: multi-threaded

L1-dcache-load-misses (%)
   < 2%: datos bien en L1
   2-10%: normal
   > 10%: cambiar estructura de datos

LLC-load-misses (%)  ← MÁS IMPORTANTE
   < 5%: bien
   5-20%: algo de presión, pero OK
   > 20%: memory bound, optimizar locality

dTLB-load-misses (%)
   < 0.1%: normal
   > 1%: datos dispersos, huge pages puede ayudar

iTLB-load-misses (%)
   < 0.1%: normal (raro exceder esto)
   > 0.5%: código disperso, demasiadas funciones inlined
```

---

## 13. perf stat con múltiples ejecuciones: -r y estabilidad

Como cualquier benchmark, `perf stat` tiene varianza entre ejecuciones. La flag `-r N` ejecuta el comando N veces y reporta media y desviación estándar:

```bash
# Ejecutar 10 veces y promediar
perf stat -r 10 ./my_program

# Salida:
 Performance counter stats for './my_program' (10 runs):

          1,234.56 msec task-clock        #    0.998 CPUs utilized    ( +-  0.12% )
                12      context-switches  #    9.719 /sec             ( +-  1.34% )
                 0      cpu-migrations    #    0.000 /sec
             1,234      page-faults       #  999.635 /sec             ( +-  0.08% )
     3,456,789,012      cycles            #    2.800 GHz              ( +-  0.15% )
     8,765,432,109      instructions      #    2.54  insn per cycle   ( +-  0.05% )
     1,234,567,890      branches          #  999.967 M/sec            ( +-  0.10% )
         1,234,567      branch-misses     #    0.10% of all branches  ( +-  0.25% )

            1.23679 +- 0.00234 seconds time elapsed  ( +-  0.19% )
```

La columna `+- X%` es el coeficiente de variación. Úsala como sanity check:

```
CV interpretación:
  < 1%    Excelente, medición estable
  1-3%    Bueno
  3-5%    Aceptable
  5-10%   Problemático (cerrar apps, CPU pinning)
  > 10%   Inaceptable, investigar ruido
```

### Opciones relacionadas

```bash
# -n N: no ejecutar warmup runs
perf stat -r 10 -n ./my_program

# --pre CMD: ejecutar CMD antes de cada run (útil para cleanup)
perf stat -r 10 --pre 'echo 3 > /proc/sys/vm/drop_caches' ./my_program

# --post CMD: ejecutar CMD después de cada run
perf stat -r 10 --post './cleanup.sh' ./my_program

# --repeat N: igual que -r N (forma alternativa)
perf stat --repeat 5 ./my_program
```

---

## 14. perf stat por CPU o por thread

Por defecto, `perf stat` agrega los contadores del programa completo. Puedes desglosarlos:

```bash
# Por CPU (core)
perf stat -a -A ./my_program
# -a: agregado del sistema
# -A: uno por CPU (no agregar)

# Por thread
perf stat --per-thread ./my_program

# Profile todo el sistema durante 5 segundos
perf stat -a sleep 5

# Profile un PID existente
perf stat -p 1234 sleep 5

# Profile un CPU específico
perf stat -C 0 sleep 5
# Mide todo lo que pase en el CPU 0

# Múltiples CPUs
perf stat -C 0,2,4 sleep 5
```

### Ejemplo: análisis por core

```
$ perf stat -a -A sleep 3

 Performance counter stats for 'system wide':

CPU0     2,456.78 msec cpu-clock    #    0.819 CPUs utilized
CPU1     1,234.56 msec cpu-clock    #    0.412 CPUs utilized
CPU2       456.78 msec cpu-clock    #    0.152 CPUs utilized
CPU3       123.45 msec cpu-clock    #    0.041 CPUs utilized
...

→ Puedes ver que CPU0 está sobrecargado mientras los demás están idle.
  Indica un problema de balanceo de carga en el sistema.
```

---

## 15. perf record: introducción al sampling profiler

`perf stat` te dice **cuánto** trabajó el CPU. `perf record` te dice **dónde** pasó el tiempo.

```
┌──────────────────────────────────────────────────────────────────────┐
│                 perf stat vs perf record                             │
│                                                                      │
│  perf stat:                                                          │
│  - Contadores agregados del programa completo                        │
│  - Sin información de dónde ocurrieron los eventos                    │
│  - Resultado: un número por contador                                 │
│                                                                      │
│  perf record:                                                        │
│  - Toma samples periódicos durante la ejecución                      │
│  - Cada sample incluye: PC, stack, CPU, timestamp                    │
│  - Guarda en perf.data                                               │
│  - Analizas después con perf report / perf annotate                  │
│  - Resultado: mapa de calor sobre funciones/líneas                   │
│                                                                      │
│  Analogía:                                                            │
│  - perf stat = báscula: cuánto pesa tu programa                       │
│  - perf record = MRI: dónde está la masa                              │
└──────────────────────────────────────────────────────────────────────┘
```

### Uso básico

```bash
# Profile un comando
perf record ./my_program

# Esto genera perf.data en el directorio actual
ls -lh perf.data
# -rw------- 1 user user 2.3M Nov  1 14:23 perf.data

# Ver el resultado
perf report
```

### Salida interactiva de perf report

```
Samples: 12K of event 'cpu-clock:ppp', Event count (approx.): 3076250000
Overhead  Command      Shared Object        Symbol
  45.23%  my_program   my_program           [.] matrix_multiply
  18.56%  my_program   my_program           [.] compute_hash
  12.34%  my_program   libc-2.34.so         [.] __memcpy_avx2_unaligned
   8.91%  my_program   my_program           [.] parse_input
   5.67%  my_program   libc-2.34.so         [.] __malloc
   3.21%  my_program   [kernel.kallsyms]    [k] __schedule
   1.23%  my_program   my_program           [.] validate_data
   ...
```

```
Columnas:
─────────
Overhead: % del tiempo total que tomó esta función
Command:  programa principal
Shared Object: librería o binario donde vive el símbolo
Symbol:   nombre de la función

Las primeras 2-3 entradas son tus hotspots principales.
En este ejemplo:
  - 45% del tiempo está en matrix_multiply
  - 18% en compute_hash
  - 12% en memcpy (de libc, seguramente llamado desde algún lado)
```

---

## 16. Cómo funciona el sampling: interrupciones periódicas

El sampling profiler de perf funciona interrumpiendo tu programa periódicamente y grabando dónde está ejecutándose:

```
┌──────────────────────────────────────────────────────────────────────┐
│             Funcionamiento del sampling                              │
│                                                                      │
│  Tu programa ejecutando:                                             │
│    función_A() → función_B() → función_C() → ...                    │
│                                                                      │
│  perf configura un contador de hardware (por ej. cycles)            │
│  con un valor de overflow. Cada N ciclos (ej. 1,000,000):           │
│                                                                      │
│  1. El contador alcanza cero                                         │
│  2. El CPU genera una interrupción NMI (non-maskable)                │
│  3. perf handler captura:                                            │
│     - PC (program counter) → dónde estaba                            │
│     - Stack trace → quién llamó a quién                              │
│     - CPU, PID, TID, timestamp                                        │
│  4. perf escribe el sample al ring buffer                            │
│  5. perf reset el contador                                           │
│  6. El programa continúa                                             │
│                                                                      │
│  Resultado: miles de samples distribuidos temporalmente.             │
│  Las funciones que aparecen MÁS VECES son los hotspots.              │
│                                                                      │
│  Overhead típico: 1-3% con frecuencia default.                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Ilustración temporal

```
Tiempo →

Programa: [func_A][func_B][func_B][func_B][func_C][func_B][func_A][func_C]
           │      │      │      │      │      │      │      │
           sample sample sample sample sample sample sample sample
             A      B      B      B      C      B      A      C

Counts:
  func_A: 2  (25%)
  func_B: 4  (50%)
  func_C: 2  (25%)

Conclusión: el 50% del tiempo está en func_B → es el hotspot principal.
```

---

## 17. perf record: frecuencia vs período

Hay dos formas de controlar la tasa de sampling:

```bash
# ═══════════════════════════════════════════════════════════
# Método 1: frecuencia (-F Hz)
# ═══════════════════════════════════════════════════════════
# Samples por segundo
perf record -F 99 ./my_program      # 99 samples/segundo
perf record -F 999 ./my_program     # 999 samples/segundo (default reciente)
perf record -F 4000 ./my_program    # 4000 samples/segundo (detallado)

# ¿Por qué 99 y no 100? Para evitar coincidir con ticks periódicos
# del kernel (100 Hz, 1000 Hz). Si coincides, puedes perder eventos.

# ═══════════════════════════════════════════════════════════
# Método 2: período (-c COUNT)
# ═══════════════════════════════════════════════════════════
# Un sample cada COUNT eventos del contador
perf record -c 1000000 ./my_program   # 1 sample cada 1M ciclos

# Con evento específico:
perf record -e cycles -c 1000000 ./my_program
perf record -e instructions -c 10000000 ./my_program
perf record -e cache-misses -c 10000 ./my_program
# Tomar un sample cada 10000 cache misses
# → samples concentrados en el código que causa cache misses
```

### Cómo elegir la frecuencia

```
┌──────────────────────────────────────────────────────────────────────┐
│           Guía de frecuencia de sampling                             │
│                                                                      │
│  Tipo de profiling         Frecuencia    Overhead                    │
│  ──────────────────        ──────────    ─────────                   │
│  Rápido, overview          99 Hz         <1%                         │
│  Normal                    999 Hz        1-2%                        │
│  Detallado                 4000 Hz       3-5%                        │
│  Muy detallado             9997 Hz       5-10%                       │
│                                                                      │
│  Reglas prácticas:                                                   │
│  1. Más frecuencia = más samples = más precisión                     │
│  2. Más samples = más overhead = más distorsión                      │
│  3. Para programas cortos (<10s): frecuencia alta                    │
│  4. Para programas largos (>1 min): frecuencia baja                  │
│  5. Para PGO (profile guided optimization): 99 Hz es suficiente      │
│                                                                      │
│  ¿Cuántos samples necesitas?                                         │
│  ──────────────────────────                                          │
│  Regla: al menos 10,000 samples para confianza estadística           │
│  Si tu programa tarda 10s y quieres 10,000 samples:                 │
│    frecuencia = 10000 / 10 = 1000 Hz                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Eventos de sampling (no solo cycles)

```bash
# Sampling por cache misses → dónde están los cache misses
perf record -e cache-misses ./my_program

# Sampling por branch mispredictions → dónde están los branches malos
perf record -e branch-misses ./my_program

# Sampling por dTLB misses → dónde está el TLB thrashing
perf record -e dTLB-load-misses ./my_program

# Sampling múltiple
perf record -e cycles,cache-misses,branch-misses ./my_program
```

---

## 18. Call graphs: -g, --call-graph fp/dwarf/lbr

El sample por defecto solo captura el PC (instrucción actual). Para ver la **cadena de llamadas**, activa call graphs:

```bash
# Activar call graphs (método por defecto)
perf record -g ./my_program

# O explícitamente
perf record --call-graph fp ./my_program

# Ver el resultado con call graph
perf report -g
```

### Los 3 métodos de call graph

```
┌──────────────────────────────────────────────────────────────────────┐
│         Métodos de captura de call graphs                            │
│                                                                      │
│  1. fp (Frame Pointer) ← default en perf                             │
│  ─────────────────────                                               │
│  Usa el registro %rbp/%ebp que apunta al frame actual.               │
│  Camina la cadena de %rbp para reconstruir el stack.                 │
│                                                                      │
│  Requisito: compilar con -fno-omit-frame-pointer                    │
│  (gcc/clang omiten el frame pointer por default con -O2)            │
│                                                                      │
│  Ventajas:                                                           │
│  + Overhead mínimo                                                   │
│  + Funciona a velocidad completa                                     │
│                                                                      │
│  Desventajas:                                                        │
│  - Requiere recompilar con -fno-omit-frame-pointer                  │
│  - Rendimiento ~5-10% peor del binario                               │
│                                                                      │
│  2. dwarf (DWARF debug info)                                         │
│  ───────────────────────────                                         │
│  Usa la información de unwinding de DWARF (debug info).              │
│  Copia el stack en cada sample y lo procesa después.                 │
│                                                                      │
│  Requisito: compilar con -g (debug info)                             │
│  Puede usar -gdwarf-4 o -gdwarf-5 específicamente.                   │
│                                                                      │
│  Ventajas:                                                           │
│  + No requiere -fno-omit-frame-pointer                              │
│  + Funciona con binarios optimizados normalmente                     │
│                                                                      │
│  Desventajas:                                                        │
│  - Overhead más alto (copia 8KB del stack en cada sample)            │
│  - perf.data más grande                                              │
│  - Procesamiento lento                                               │
│                                                                      │
│  3. lbr (Last Branch Record)                                         │
│  ───────────────────────────                                         │
│  Usa LBR, un buffer de hardware en Intel CPUs modernos.              │
│  El CPU guarda las últimas N (16-32) ramas automáticamente.          │
│                                                                      │
│  Requisito: CPU Intel con LBR (Haswell+)                             │
│                                                                      │
│  Ventajas:                                                           │
│  + Overhead casi cero                                                │
│  + Funciona con cualquier binario                                    │
│  + Precisión a nivel de branch                                       │
│                                                                      │
│  Desventajas:                                                        │
│  - Profundidad limitada (16-32 frames)                               │
│  - Solo Intel (no AMD)                                               │
│  - Kernel >= 3.18                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Cuándo usar cada uno

```bash
# Método 1: fp — mejor opción si puedes recompilar
# CFLAGS="-fno-omit-frame-pointer -O2 -g"
perf record --call-graph fp ./my_program

# Método 2: dwarf — mejor si no puedes recompilar
# o si necesitas profundidad completa
perf record --call-graph dwarf ./my_program

# Método 3: lbr — si tienes CPU Intel reciente y quieres overhead mínimo
perf record --call-graph lbr ./my_program

# Con dwarf puedes controlar el tamaño del stack copiado:
perf record --call-graph dwarf,8192 ./my_program
# 8192 bytes = más profundidad, más overhead
```

---

## 19. perf report: navegación interactiva

`perf report` abre una interfaz interactiva para explorar los samples:

```bash
perf report
```

### Teclas de navegación

```
┌──────────────────────────────────────────────────────────────────────┐
│            Atajos de teclado en perf report                          │
│                                                                      │
│  ↑ ↓                  Navegar entre funciones                        │
│  Enter                Expandir/colapsar (con call graph)             │
│  + / -                Expandir/colapsar call graph                   │
│  /                    Buscar por nombre de función                   │
│  F                    Filtrar por nombre                             │
│  d                    Debug info (mostrar librería/objeto)           │
│  a                    Anotar la función actual (mostrar assembly)    │
│  O                    Ordenar por diferentes columnas                │
│  h                    Mostrar ayuda                                  │
│  q                    Salir                                          │
└──────────────────────────────────────────────────────────────────────┘
```

### Opciones útiles de la CLI

```bash
# Mostrar porcentajes por hilo
perf report --per-thread

# Solo mostrar las top 10
perf report --stdio --sort=overhead,symbol --fields=overhead,symbol | head -20

# Mostrar como texto (sin interactividad)
perf report --stdio

# Ordenar por diferentes criterios
perf report --sort=dso         # por librería
perf report --sort=comm        # por comando
perf report --sort=symbol      # por símbolo (default)
perf report --sort=srcline     # por línea de código

# Excluir el kernel
perf report --exclude-kernel

# Solo mostrar symbols que contienen "parse"
perf report -g --symbol-filter=parse
```

### Ejemplo con call graph expandido

```
-   45.23%  my_program   my_program   [.] matrix_multiply
     matrix_multiply
   - main
     - __libc_start_main
+   18.56%  my_program   my_program   [.] compute_hash
-   12.34%  my_program   libc-2.34.so [.] __memcpy_avx2_unaligned
     __memcpy_avx2_unaligned
     compute_hash
     main
     __libc_start_main
```

Las entradas con `-` están expandidas (muestran quién llamó a la función). Las entradas con `+` están colapsadas.

---

## 20. Symbols: por qué necesitas debug info

Los samples de perf son direcciones de memoria. Para traducirlas a nombres de funciones, perf necesita **símbolos**:

```
┌──────────────────────────────────────────────────────────────────────┐
│          Niveles de información simbólica                            │
│                                                                      │
│  1. Stripped binary (sin símbolos)                                   │
│  ─────────────────────────────────                                   │
│  gcc -O2 -s -o my_prog my_prog.c                                     │
│                                                                      │
│  perf report muestra:                                                │
│    45.23%  my_program  my_program  [.] 0x0000000000012345            │
│  → solo direcciones, inútil                                          │
│                                                                      │
│  2. Binary con tabla de símbolos (sin debug info)                    │
│  ────────────────────────────────────────────────                    │
│  gcc -O2 -o my_prog my_prog.c                                        │
│                                                                      │
│  perf report muestra:                                                │
│    45.23%  my_program  my_program  [.] matrix_multiply               │
│  → nombres de funciones, bien para profile básico                    │
│                                                                      │
│  3. Binary con debug info                                            │
│  ────────────────────────                                            │
│  gcc -O2 -g -o my_prog my_prog.c                                     │
│                                                                      │
│  perf report + perf annotate muestra:                                │
│    - Nombres de funciones                                            │
│    - Números de línea                                                │
│    - Código fuente intercalado con assembly                          │
│  → opción ideal para profiling                                       │
│                                                                      │
│  4. Debug info separado (recomendado para binarios grandes)          │
│  ─────────────────────────────────────────────────────────           │
│  gcc -O2 -g -o my_prog my_prog.c                                     │
│  objcopy --only-keep-debug my_prog my_prog.debug                     │
│  strip --strip-debug my_prog                                         │
│  objcopy --add-gnu-debuglink=my_prog.debug my_prog                   │
│  → binario sin debug info, pero perf lo encuentra                    │
└──────────────────────────────────────────────────────────────────────┘
```

### Compilar para profiling

```bash
# Compilación óptima para profiling con perf
gcc -O2 -g -fno-omit-frame-pointer -o my_prog my_prog.c

# -O2: optimizaciones de producción
# -g: debug info completo
# -fno-omit-frame-pointer: mantiene %rbp para call graphs rápidos
```

### Para Rust

```toml
# Cargo.toml
[profile.release]
debug = true                # incluir debug info
opt-level = 3               # -O3
lto = false                 # LTO puede complicar symbols

[profile.bench]
debug = true
```

Con esto, `cargo build --release` genera un binario con debug info que perf puede usar.

### Librerías del sistema (glibc, libc)

Por defecto, las distribuciones proveen librerías del sistema sin debug info. Para profilar código que llama a libc:

```bash
# Fedora/RHEL: instalar debug info de paquetes
sudo dnf install glibc-debuginfo

# Ubuntu/Debian
sudo apt install libc6-dbg

# Verificar que están disponibles
ls /usr/lib/debug/
```

---

## 21. perf annotate: instrucciones anotadas

`perf annotate` muestra el assembly (y código fuente si hay debug info) con samples anotados en cada instrucción:

```bash
# Anotar una función específica
perf annotate -s matrix_multiply

# Anotar con código fuente intercalado (requiere -g)
perf annotate -s matrix_multiply --stdio

# Interactivo
perf annotate matrix_multiply
```

### Ejemplo de salida

```
 Percent | Source code & Disassembly of my_program for cycles:ppp
─────────────────────────────────────────────────────────────────
         : void matrix_multiply(double *A, double *B, double *C, int N) {
    0.03 :    push   %rbp
    0.01 :    mov    %rsp,%rbp
    0.00 :    push   %r15
         :    for (int i = 0; i < N; i++) {
    0.15 :    xor    %eax,%eax
         :        for (int j = 0; j < N; j++) {
    0.23 :    xor    %edx,%edx
         :            double sum = 0;
    1.34 :    vxorpd %xmm0,%xmm0,%xmm0
         :            for (int k = 0; k < N; k++) {
    8.45 :    vmovsd (%rcx,%rax,8),%xmm1      ← 8.45% de samples aquí
   23.67 :    vfmadd231sd (%rsi,%rdx,8),%xmm1,%xmm0  ← HOTSPOT
   12.34 :    add    $0x8,%rdx
    5.67 :    cmp    %rdx,%rbx
    0.45 :    jne    .L3
         :            C[i*N+j] = sum;
    1.23 :    vmovsd %xmm0,(%rdi)
         :        }
    0.34 :    add    $0x8,%rdi
    0.23 :    cmp    %rdi,%r9
    0.12 :    jne    .L2
         :    }
    0.01 :    pop    %r15
    0.00 :    pop    %rbp
    0.00 :    ret
```

### Qué buscar en perf annotate

```
┌──────────────────────────────────────────────────────────────────────┐
│       Interpretar perf annotate                                      │
│                                                                      │
│  Instrucciones con alto %:                                           │
│  - Fuseable (fused μops): vfmadd, vaddps, etc.                       │
│    → ya están optimizadas                                            │
│  - Load/store (mov, vmovsd, vmovapd):                                │
│    → posible cache miss                                              │
│    → o código memory-bound normal                                    │
│  - Branch (jne, je, ja, etc.):                                       │
│    → misprediction o cache miss en el target                         │
│                                                                      │
│  Patrones típicos:                                                   │
│  ─────────────                                                       │
│  1. Load con alto % → cache miss en lectura                          │
│     Solución: mejor localidad, prefetch, struct-of-arrays             │
│                                                                      │
│  2. Branch con alto % → misprediction                                │
│     Solución: hint al compilador (__builtin_expect), reordenar       │
│                                                                      │
│  3. Division (div, vdivpd) con alto % → operación costosa            │
│     Solución: evitar si es posible (multiplicar por recíproco)        │
│                                                                      │
│  4. Call con alto % → función costosa o no inlined                   │
│     Solución: marcar __attribute__((always_inline)) o refactorizar    │
└──────────────────────────────────────────────────────────────────────┘
```

### Perf annotate con fuente (debug info)

```bash
# Requiere debug info y source disponible
perf annotate -s matrix_multiply --source
```

```
Percent |      Source code & Disassembly
────────────────────────────────────────────────────────────
        : void matrix_multiply(double *A, double *B, double *C, int N) {
    0.5 :     for (int i = 0; i < N; i++) {
    0.3 :         for (int j = 0; j < N; j++) {
    1.2 :             double sum = 0;
   28.5 :             for (int k = 0; k < N; k++) {
   45.8 :                 sum += A[i*N+k] * B[k*N+j];   ← LÍNEA CRÍTICA
    0.2 :             }
    2.3 :             C[i*N+j] = sum;
        :         }
        :     }
        : }
```

Línea `sum += A[i*N+k] * B[k*N+j]` toma el 45.8% + 28.5% del tiempo → claramente el hotspot. Con esto puedes enfocar la optimización (blocking, transposición de B, SIMD explícito).

---

## 22. perf script: export de samples raw

`perf script` exporta los samples en formato texto, útil para post-procesamiento con scripts custom o para alimentar otras herramientas (como flamegraphs):

```bash
# Exportar todos los samples como texto
perf script > samples.txt

# Salida típica:
# my_program 12345 [000]  1234.567: 1000000 cycles:
#     7f1234567890 matrix_multiply+0x123 (/path/to/my_program)
#     7f1234568abc main+0x45 (/path/to/my_program)
#     7f12345900de __libc_start_main+0x78 (/usr/lib64/libc.so.6)
```

### Formato customizable

```bash
# Campos específicos
perf script -F pid,time,event,sym,dso

# Solo PID y símbolo
perf script -F pid,sym
```

### Integración con flamegraph (preview, ver T02)

```bash
# Flamegraph requiere perf script como input
perf record -g ./my_program
perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > flame.svg
# Veremos esto en profundidad en T02
```

---

## 23. perf top: profiling en vivo

`perf top` muestra el profiling en tiempo real del sistema entero, similar a `htop` pero a nivel de funciones:

```bash
# Profiling del sistema en vivo
sudo perf top

# Solo tu proceso
perf top -p 1234

# Con call graph
sudo perf top -g
```

### Salida de perf top

```
Samples: 5K of event 'cycles', 4000 Hz, Event count (approx.): 1234567890

Overhead  Shared Object                  Symbol
  12.34%  [kernel]                       [k] __memcpy_erms
   8.67%  [kernel]                       [k] __softirq
   6.23%  libc-2.34.so                   [.] __memset_avx2_unaligned
   5.45%  firefox                        [.] js::jit::MaybeCallFromJit
   4.12%  [kernel]                       [k] finish_task_switch.isra.0
   ...
```

### Uso típico de perf top

```
Casos donde perf top brilla:
────────────────────────────

1. Debug de "por qué mi sistema está lento"
   sudo perf top
   → ver quién está consumiendo CPU en este momento

2. Identificar cold spots durante una prueba manual
   perf top -p $(pgrep my_server)
   → mientras haces click en la UI, ves qué funciones se activan

3. Encontrar daemons problemáticos
   sudo perf top --sort=comm
   → agrupar por proceso, ver si algún daemon consume mucho

4. Ver impacto de cambios en tiempo real
   # En una ventana: perf top -p $(pgrep bench)
   # En otra: modificar configuración del bench
   # Ver cómo cambian los porcentajes
```

---

## 24. Profiling de Rust con perf

perf funciona con binarios de Rust sin problemas adicionales:

```bash
# 1. Compilar con debug info y frame pointers
RUSTFLAGS="-C debuginfo=2 -C force-frame-pointers=yes" cargo build --release

# O en Cargo.toml:
# [profile.release]
# debug = true

# 2. Profiling
perf record --call-graph fp ./target/release/my_app

# 3. Report
perf report
```

### Symbol demangling

Los símbolos de Rust están manglings (por ej. `_ZN10my_program4main17h1234567890abcdefE`). perf los demangle automáticamente:

```
Antes (sin demangling):
  45.23%  my_app  my_app  [.] _ZN9my_module14matrix_multiply17h1a2b3c4d5e6f7g8hE

Después (perf demangle automáticamente):
  45.23%  my_app  my_app  [.] my_module::matrix_multiply
```

### Limitaciones con Rust

```
┌──────────────────────────────────────────────────────────────────────┐
│         Profiling de Rust: consideraciones                           │
│                                                                      │
│  1. Inlining agresivo                                                │
│     Rust inlinea mucho más que C por defecto.                        │
│     Muchas funciones pequeñas desaparecen.                           │
│     Solución: marcar con #[inline(never)] durante profiling.          │
│                                                                      │
│  2. Generics y monomorphización                                      │
│     Una función genérica puede aparecer como múltiples               │
│     entradas en perf report (una por cada tipo concreto).            │
│                                                                      │
│  3. Closures                                                         │
│     Closures aparecen como nombres raros generados por el compilador:│
│     my_module::{{closure}}::h1234567890abcdef                        │
│                                                                      │
│  4. Async (tokio, async-std)                                         │
│     Las funciones async se transforman en state machines.            │
│     Las llamadas .await no aparecen como "esperas" en el profile.    │
│     Puedes ver muchos accesos a campos del state struct.             │
│     Para async, considera usar tokio-console o herramientas          │
│     específicas.                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### cargo-flamegraph: wrapper conveniente

```bash
# Instalar
cargo install flamegraph

# Profile un binary con cargo-flamegraph
cargo flamegraph --release --bin my_app

# Esto ejecuta internamente:
# cargo build --release
# perf record ... ./target/release/my_app
# perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

---

## 25. Profiling del kernel y syscalls

perf puede profilar el kernel mismo y ver dónde se pasa el tiempo en syscalls:

```bash
# Profile incluyendo kernel (requiere permisos adecuados)
sudo perf record -g ./my_program

# perf report mostrará símbolos del kernel como [k]
#   15.34%  my_program  [kernel.kallsyms]  [k] __schedule
#    8.45%  my_program  [kernel.kallsyms]  [k] copy_user_enhanced_fast_string
#    5.67%  my_program  [kernel.kallsyms]  [k] handle_mm_fault
```

### perf trace: tracing de syscalls

`perf trace` es la alternativa moderna a `strace`, con menos overhead:

```bash
# Trace syscalls de un comando
sudo perf trace ./my_program

# Salida similar a strace:
#      0.000 ( 0.012 ms): my_program/12345 brk(brk: 0x1234567)
#      0.015 ( 0.005 ms): my_program/12345 read(fd: 3, buf: 0x7fffffff, count: 4096)
#      0.025 ( 0.003 ms): my_program/12345 write(fd: 1, buf: "hello", count: 6)
```

### Contadores de syscalls

```bash
# Contar syscalls sin tracing detallado
sudo perf stat -e 'syscalls:*' ./my_program

# Ver cuántas veces se llamó a cada syscall
sudo perf trace -s ./my_program
# Salida:
#  Summary of events:
#  my_program (12345), 456 events, 100.0%
#    syscall            calls    total       min       avg       max      stddev
#                                 (msec)    (msec)    (msec)    (msec)        (%)
#    ------------ -------- --------- --------- --------- ---------     ------
#    read                123    12.345    0.045    0.100    2.345     15.23%
#    write                89    34.567    0.123    0.388    3.456     22.45%
#    openat               45     1.234    0.023    0.027    0.056      3.45%
#    ...
```

---

## 26. Interpretación: identificar hotspots reales

Tener datos no es lo mismo que saber interpretarlos. Aquí está cómo leer un profile de perf para sacar conclusiones útiles:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Patrones comunes en profiles de perf                         │
│                                                                      │
│  Patrón 1: Hotspot claro (caso ideal)                                │
│  ────────────────────────────────                                    │
│  45%  matrix_multiply                                                │
│   8%  compute_hash                                                   │
│   6%  ...                                                            │
│                                                                      │
│  Una función domina. Optimizarla tiene impacto grande.               │
│  Ley de Amdahl: speedup máximo = 1 / (1 - 0.45) = 1.82x             │
│  Aunque matrix_multiply sea instantáneo, el speedup total            │
│  está limitado por el resto.                                          │
│                                                                      │
│  Patrón 2: Distribución uniforme (difícil)                           │
│  ─────────────────────────────────                                   │
│   5%  func_a                                                         │
│   4%  func_b                                                         │
│   4%  func_c                                                         │
│   3%  func_d                                                         │
│   ...                                                                 │
│                                                                      │
│  No hay hotspot claro. Optimizaciones locales dan poco beneficio.    │
│  Considerar:                                                          │
│  - Cambio arquitectural (ej. otro algoritmo)                         │
│  - Cambio de estructura de datos                                     │
│  - Paralelización                                                    │
│                                                                      │
│  Patrón 3: Dominancia de memcpy/memset                               │
│  ──────────────────────────────────                                  │
│  30%  __memcpy_avx2_unaligned (libc)                                 │
│  15%  __memset_avx2_unaligned (libc)                                 │
│  ...                                                                  │
│                                                                      │
│  Programa memory-bound. Oportunidades:                               │
│  - Reducir allocations (reuse buffers)                               │
│  - Evitar copias innecesarias (moves, references)                    │
│  - Mejor estructura de datos (menos datos = menos copias)            │
│                                                                      │
│  Patrón 4: Dominancia de malloc/free                                 │
│  ───────────────────────────────                                     │
│  20%  __malloc                                                       │
│  10%  __free                                                         │
│  ...                                                                  │
│                                                                      │
│  Problema de allocator. Oportunidades:                               │
│  - Pool allocator / arena                                            │
│  - Reutilizar objetos en lugar de allocar/liberar                    │
│  - Cambiar a jemalloc o mimalloc                                     │
│                                                                      │
│  Patrón 5: Dominancia del kernel                                     │
│  ────────────────────────────                                        │
│  15%  __schedule (kernel)                                            │
│  10%  __futex (kernel)                                               │
│   8%  sys_read (kernel)                                              │
│                                                                      │
│  Problema de sistema:                                                │
│  - __schedule: mucho context switching → locks, sleeps               │
│  - __futex: lock contention → mejor synchronization                  │
│  - sys_read: mucho I/O → async, batching, buffers                   │
│                                                                      │
│  Patrón 6: Funciones de string (strlen, strcmp, strcpy)              │
│  ────────────────────────────────────────────────                    │
│  12%  __strlen_avx2                                                  │
│   8%  __strcmp_avx2                                                  │
│   6%  __strcpy_avx2                                                  │
│                                                                      │
│  Problema de parsing/manipulación de strings:                        │
│  - Usar lengths guardados en lugar de strlen repetido                │
│  - Evitar strcpy a favor de memcpy con lengths conocidos             │
│  - Considerar string interning                                       │
└──────────────────────────────────────────────────────────────────────┘
```

### La ley de Amdahl en la práctica

```
┌──────────────────────────────────────────────────────────────────────┐
│         Ley de Amdahl: el límite de la optimización                  │
│                                                                      │
│  Si una función toma X% del tiempo total, y la aceleras en K veces: │
│                                                                      │
│  speedup_total = 1 / ((1 - X) + X/K)                                │
│                                                                      │
│  Ejemplos:                                                           │
│  ─────────                                                           │
│  X = 50%, K = ∞ (infinito) → speedup = 2x                           │
│  X = 50%, K = 2 → speedup = 1.33x                                    │
│  X = 50%, K = 10 → speedup = 1.82x                                   │
│                                                                      │
│  X = 10%, K = ∞ → speedup = 1.11x                                    │
│  X = 10%, K = 10 → speedup = 1.09x                                   │
│                                                                      │
│  X = 90%, K = ∞ → speedup = 10x                                      │
│  X = 90%, K = 2 → speedup = 1.82x                                    │
│                                                                      │
│  Moraleja:                                                           │
│  1. Optimiza lo que toma más tiempo (no tu función favorita).        │
│  2. El speedup está limitado por las partes no optimizables.         │
│  3. Optimizar algo que toma < 5% es perder el tiempo.                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 27. Overhead de perf y cuándo afecta

`perf` es eficiente pero no gratis. Entender su overhead ayuda a interpretar resultados:

```
┌──────────────────────────────────────────────────────────────────────┐
│              Overhead típico de perf                                 │
│                                                                      │
│  perf stat:                                                          │
│  ───────────                                                         │
│  Contadores agregados. Overhead: <1%                                 │
│  Prácticamente invisible incluso para programas cortos.              │
│                                                                      │
│  perf record sin call graph:                                         │
│  ──────────────────────────                                          │
│  Sampling simple. Overhead: ~1-2%                                    │
│  Aceptable para casi todos los usos.                                 │
│                                                                      │
│  perf record --call-graph fp:                                        │
│  ──────────────────────────                                          │
│  Samples + frame pointer unwind. Overhead: ~2-3%                     │
│  Recomendado para profiling normal.                                  │
│                                                                      │
│  perf record --call-graph dwarf:                                     │
│  ─────────────────────────────                                       │
│  Samples + copia de stack. Overhead: ~10-20%                         │
│  El más alto. Afecta el rendimiento del programa.                    │
│                                                                      │
│  perf trace:                                                         │
│  ──────────                                                          │
│  Tracing de syscalls. Overhead: ~5-10% (menos que strace)            │
│                                                                      │
│  perf record con alta frecuencia (-F 9997):                          │
│  ──────────────────────────────────                                  │
│  Overhead puede llegar a 10-30%.                                     │
│  Solo usar para programas cortos.                                    │
│                                                                      │
│  Reglas:                                                              │
│  - Si el benchmark es sensible a 1-2% de ruido, usa perf stat sólo. │
│  - Para profiling exploratorio, perf record es fine.                 │
│  - Para decisiones de optimización, considerar el overhead.          │
└──────────────────────────────────────────────────────────────────────┘
```

### Cuando el overhead distorsiona

```
Si tu programa tarda 100ms con perf record y 98ms sin perf:
  Overhead = 2% → aceptable, los datos son representativos

Si tu programa tarda 150ms con perf record y 100ms sin perf:
  Overhead = 50% → MUY alto
  Los samples pueden estar sesgados:
    - Funciones pequeñas no alcanzan a ser sampleadas
    - El overhead mismo aparece en el profile
  Acción: reducir frecuencia de sampling (-F 99 en vez de -F 4000)

Para medir overhead:
  time ./my_program                  # baseline
  time perf record ./my_program      # con perf
  Calcular diferencia porcentual
```

---

## 28. Errores comunes al usar perf

```
┌──────────────────────────────────────────────────────────────────────┐
│          Errores comunes al usar perf                                │
│                                                                      │
│  1. Profilar binarios stripped                                       │
│  ──────────────────────                                              │
│  Síntoma: perf report muestra direcciones en lugar de nombres        │
│  Solución: compilar con -g o no hacer strip                          │
│                                                                      │
│  2. Olvidar -g en perf record                                        │
│  ──────────────────────                                              │
│  Síntoma: sin call graphs en perf report                             │
│  Solución: perf record -g ./program                                  │
│                                                                      │
│  3. Usar frame pointer sin recompilar                                │
│  ──────────────────────                                              │
│  Síntoma: call graphs vacíos o truncados                             │
│  Causa: el binario no tiene frame pointers                           │
│  Solución: compilar con -fno-omit-frame-pointer                     │
│  O alternativa: --call-graph dwarf                                   │
│                                                                      │
│  4. Profile de programa con -O0                                      │
│  ──────────────────────                                              │
│  Síntoma: resultados no se parecen al comportamiento de producción   │
│  Solución: profilar binarios compilados con -O2 o -O3               │
│                                                                      │
│  5. perf_event_paranoid muy alto                                     │
│  ──────────────────────                                              │
│  Síntoma: "Access to performance monitoring... is limited"           │
│  Solución: sudo sysctl kernel.perf_event_paranoid=1 (o menos)        │
│                                                                      │
│  6. Profilar con hyperthreading activo sin pinning                   │
│  ──────────────────────────────────                                  │
│  Síntoma: IPC inconsistente entre runs                               │
│  Solución: taskset -c N antes de perf                               │
│                                                                      │
│  7. Interpretar porcentajes como tiempos absolutos                   │
│  ──────────────────────────────────                                  │
│  Error: "matrix_multiply toma 45%, entonces 450ms de 1 segundo"     │
│  Realidad: toma 45% del TIEMPO DE CPU, no del wall time.             │
│  Si el programa no usa todos los cores, los números son diferentes.  │
│                                                                      │
│  8. Olvidar sudo para profile del sistema                            │
│  ──────────────────────                                              │
│  Síntoma: no ves símbolos del kernel o de otros procesos             │
│  Solución: sudo perf record -a (system wide)                         │
│                                                                      │
│  9. No distinguir cycles vs wall time                                │
│  ──────────────────────                                              │
│  perf stat reporta cycles: frecuencia × wall time                    │
│  Si el CPU está a 2 GHz, 1 segundo = 2e9 ciclos                      │
│  Si a 3.6 GHz, 1 segundo = 3.6e9 ciclos                              │
│  Para comparar, usa task-clock o ref-cycles.                         │
│                                                                      │
│  10. Asumir que perf es 100% preciso                                 │
│  ──────────────────────                                              │
│  El sampling tiene error estadístico.                                │
│  Con 1000 samples, error ~3% en cada porcentaje.                    │
│  Para decisiones precisas, aumentar frecuencia o duración.           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 29. Programa de práctica: profile_target

Un programa deliberadamente diseñado con varios tipos de hotspots para practicar profiling. Ejecutas perf sobre él y aprendes a identificar cada patrón.

```c
// profile_target.c — Programa con hotspots intencionales para profiling
//
// Compilar:
//   gcc -O2 -g -fno-omit-frame-pointer -o profile_target profile_target.c -lm
//
// Profile:
//   perf record -g ./profile_target
//   perf report
//
// Lo que deberías encontrar:
//   1. matrix_multiply: ~40% (CPU-bound, lots of floating point)
//   2. linear_search:   ~25% (branch prediction, memory access)
//   3. hash_compute:    ~15% (integer arithmetic)
//   4. memcpy:          ~10% (llamado desde varios lados)
//   5. malloc/free:     ~5%  (allocations)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════
// Workload 1: Matrix multiply (CPU-bound)
// Este será el hotspot principal (~40%)
// ═══════════════════════════════════════════════════════════

void matrix_multiply(const double *A, const double *B, double *C, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                sum += A[i * N + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// Workload 2: Linear search (memory + branch prediction)
// Hotspot secundario (~25%)
// ═══════════════════════════════════════════════════════════

int linear_search(const int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;
}

int search_many(const int *arr, int n, const int *targets, int m) {
    int found = 0;
    for (int i = 0; i < m; i++) {
        if (linear_search(arr, n, targets[i]) != -1) {
            found++;
        }
    }
    return found;
}

// ═══════════════════════════════════════════════════════════
// Workload 3: Hash compute (integer arithmetic)
// Hotspot terciario (~15%)
// ═══════════════════════════════════════════════════════════

// FNV-1a hash
unsigned long hash_compute(const unsigned char *data, size_t len) {
    unsigned long hash = 14695981039346656037UL;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 1099511628211UL;
    }
    return hash;
}

// ═══════════════════════════════════════════════════════════
// Workload 4: Copy buffers (memcpy-heavy)
// ~10% del tiempo
// ═══════════════════════════════════════════════════════════

void process_buffer(char *dst, const char *src, size_t len) {
    memcpy(dst, src, len);
    // Hacer algo "útil" con la copia
    for (size_t i = 0; i < len; i++) {
        dst[i] = (dst[i] + 1) & 0xFF;
    }
}

// ═══════════════════════════════════════════════════════════
// Workload 5: Allocation churn (malloc/free-heavy)
// ~5% del tiempo
// ═══════════════════════════════════════════════════════════

void alloc_churn(int iterations) {
    for (int i = 0; i < iterations; i++) {
        int size = 512 + (i * 31) % 2048;
        void *p = malloc(size);
        memset(p, i & 0xFF, size);
        free(p);
    }
}

// ═══════════════════════════════════════════════════════════
// Main: orquestar los workloads
// ═══════════════════════════════════════════════════════════

int main(void) {
    srand(42);
    
    printf("profile_target: programa con hotspots intencionales\n");
    printf("Corre: perf record -g ./profile_target\n\n");
    
    // ── Setup: preparar datos ────────────────────────────
    const int MATRIX_SIZE = 256;
    const int ARRAY_SIZE = 100000;
    const int NUM_TARGETS = 500;
    const int BUFFER_SIZE = 4096;
    const int NUM_BUFFERS = 200;
    
    double *A = malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
    double *B = malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
    double *C = malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(double));
    
    for (int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; i++) {
        A[i] = (double)rand() / RAND_MAX;
        B[i] = (double)rand() / RAND_MAX;
    }
    
    int *arr = malloc(ARRAY_SIZE * sizeof(int));
    for (int i = 0; i < ARRAY_SIZE; i++) arr[i] = rand();
    
    int *targets = malloc(NUM_TARGETS * sizeof(int));
    for (int i = 0; i < NUM_TARGETS; i++) targets[i] = rand();
    
    char *src_buf = malloc(BUFFER_SIZE);
    char *dst_buf = malloc(BUFFER_SIZE);
    for (int i = 0; i < BUFFER_SIZE; i++) src_buf[i] = (char)rand();
    
    unsigned char *hash_data = malloc(65536);
    for (int i = 0; i < 65536; i++) hash_data[i] = (unsigned char)rand();
    
    // ── Workload: ejecutar cada task ─────────────────────
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Workload 1: matrix multiply (dominante)
    for (int iter = 0; iter < 5; iter++) {
        matrix_multiply(A, B, C, MATRIX_SIZE);
    }
    
    // Workload 2: linear search
    int total_found = 0;
    for (int iter = 0; iter < 10; iter++) {
        total_found += search_many(arr, ARRAY_SIZE, targets, NUM_TARGETS);
    }
    
    // Workload 3: hash compute
    unsigned long total_hash = 0;
    for (int iter = 0; iter < 100; iter++) {
        total_hash += hash_compute(hash_data, 65536);
    }
    
    // Workload 4: buffer processing
    for (int iter = 0; iter < NUM_BUFFERS; iter++) {
        process_buffer(dst_buf, src_buf, BUFFER_SIZE);
    }
    
    // Workload 5: allocation churn
    alloc_churn(50000);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Hacer "uso" de los resultados para prevenir DCE
    printf("Matrix trace: %f\n", C[0] + C[MATRIX_SIZE * MATRIX_SIZE - 1]);
    printf("Search found: %d\n", total_found);
    printf("Total hash:   %lu\n", total_hash);
    printf("Last byte:    %d\n", (int)(unsigned char)dst_buf[BUFFER_SIZE - 1]);
    printf("Elapsed:      %.3f s\n", elapsed);
    
    // Cleanup
    free(A); free(B); free(C);
    free(arr); free(targets);
    free(src_buf); free(dst_buf);
    free(hash_data);
    
    return 0;
}
```

### Sesión completa de profiling

```bash
# 1. Compilar con las flags adecuadas
gcc -O2 -g -fno-omit-frame-pointer -o profile_target profile_target.c -lm

# 2. Primer vistazo: contadores básicos
perf stat ./profile_target

# Salida esperada (aproximada):
#  Performance counter stats for './profile_target':
#
#           2,345.67 msec task-clock           #    0.998 CPUs utilized
#                 12      context-switches     #    5.119 /sec
#                  0      cpu-migrations       #    0.000 /sec
#                890      page-faults          #  379.574 /sec
#      6,543,210,987      cycles               #    2.790 GHz
#     15,432,109,876      instructions         #    2.36  insn per cycle
#      1,234,567,890      branches             #  526.454 M/sec
#         12,345,678      branch-misses        #    1.00% of all branches
#
#        2.351 seconds time elapsed

# 3. Análisis detallado de cache y branches
perf stat -d -d -d ./profile_target

# 4. Sampling profile con call graphs
perf record --call-graph fp ./profile_target

# 5. Ver el reporte
perf report

# Salida típica esperada:
#   38.45%  profile_target  [.] matrix_multiply
#   22.34%  profile_target  [.] linear_search
#   15.67%  profile_target  [.] hash_compute
#    9.34%  libc            [.] __memcpy_avx2_unaligned
#    5.67%  profile_target  [.] process_buffer
#    4.23%  libc            [.] __malloc
#    2.34%  libc            [.] __memset_avx2_unaligned
#    1.56%  profile_target  [.] alloc_churn
#    0.40%  ...

# 6. Anotar la función hotspot
perf annotate -s matrix_multiply

# 7. Ver el contexto de llamadas
perf report -g

# Con call graph expandido, podrás ver:
#   - 38.45% matrix_multiply ← llamado desde main directamente
#   - 22.34% linear_search ← llamado desde search_many ← main
#   - 15.67% hash_compute ← llamado directamente desde main
```

### Ejercicio mental

Después de ejecutar el profiling, pregúntate:

```
1. ¿Cuál es el hotspot #1? ¿Qué porcentaje toma?
2. ¿Qué optimización tendría el mayor impacto?
3. Si optimizas matrix_multiply en 2x, ¿cuánto mejora el programa completo?
   → Aplicar Amdahl: 1 / ((1 - 0.38) + 0.38/2) = 1 / 0.81 = 1.23x
   → 23% de mejora total para 2x de mejora local
4. ¿Valdría la pena optimizar alloc_churn (1.56%)? → Casi nada
5. ¿Podrías mejorar linear_search con binary search? → Sí, pero si sobra el
   tiempo en el orden del array, o ya está ordenado
```

---

## 30. Ejercicios

### Ejercicio 1: primer contacto con perf stat

**Objetivo**: Familiarizarse con los contadores básicos.

**Tareas**:

**a)** Escribe un programa simple en C que sume los elementos de un array de 10 millones de ints.

**b)** Compílalo con `-O0`, `-O2`, y `-O3 -march=native`. Ejecuta `perf stat` en cada versión.

**c)** Compara:
   - `instructions`: ¿cuál ejecuta menos? ¿por qué?
   - `IPC`: ¿cuál es más alto? ¿qué indica?
   - `cycles`: ¿cuál tarda menos?
   - `branch-misses` (%): ¿cambia entre versiones?

**d)** Ejecuta con `-r 10` para obtener promedio y desviación estándar. ¿El CV es < 3%?

---

### Ejercicio 2: identificar memory-bound vs CPU-bound

**Objetivo**: Distinguir entre programas memory-bound y CPU-bound usando métricas de perf.

**Tareas**:

**a)** Escribe dos programas:
   - **Programa A**: calcula π con el método de Leibniz durante 1 billón de iteraciones (puro cómputo de FP).
   - **Programa B**: suma todos los elementos de un array de 1 GB (8 veces lo que cabe en L3 típico).

**b)** Ejecuta `perf stat -d -d ./programa_A` y `./programa_B`.

**c)** Para cada programa, calcula:
   - IPC
   - LLC-load-miss rate

**d)** Verifica:
   - Programa A debe tener IPC alto (>2) y LLC-miss rate bajo (<1%) → CPU-bound.
   - Programa B debe tener IPC bajo (<1) y LLC-miss rate alto (>20%) → memory-bound.

**e)** ¿Qué optimizaciones aplicarías a cada uno?
   - CPU-bound: SIMD, mejor algoritmo, mejor uso del pipeline.
   - Memory-bound: mejor locality, prefetch, compresión, estructura de datos más compacta.

---

### Ejercicio 3: call graphs y perf annotate

**Objetivo**: Practicar la navegación de profiles con call graphs.

**Tareas**:

**a)** Usa el `profile_target.c` de este README. Compila con `-O2 -g -fno-omit-frame-pointer`.

**b)** Ejecuta `perf record --call-graph fp ./profile_target` y luego `perf report -g`.

**c)** Navega al hotspot principal (matrix_multiply). Expándelo para ver el call graph. ¿Quién llama a matrix_multiply?

**d)** Presiona `a` (annotate) en la función. Observa qué instrucciones consumen más tiempo. ¿Son loads, stores, o cómputo?

**e)** Identifica la línea de código fuente más caliente. ¿Es la del `sum += A[i*N+k] * B[k*N+j]`?

**f)** Propón tres optimizaciones basadas en lo que viste.

---

### Ejercicio 4: profiling de un programa Rust

**Objetivo**: Aplicar perf a código Rust.

**Tareas**:

**a)** Crea un proyecto Rust `cargo new --bin rust_profile`.

**b)** En main.rs, implementa tres funciones que hagan trabajo diferente:
   - `sum_array(data: &[u64]) -> u64` sobre un array de 10M elementos.
   - `hash_all(data: &[u8]) -> u64` usando FNV-1a.
   - `matmul(a: &[f64], b: &[f64], n: usize) -> Vec<f64>` con matrices de 256x256.

**c)** Configura `Cargo.toml`:
```toml
[profile.release]
debug = true
```

**d)** Compila y ejecuta con perf:
```bash
cargo build --release
RUSTFLAGS="-C force-frame-pointers=yes" cargo build --release
perf record --call-graph fp ./target/release/rust_profile
perf report
```

**e)** Verifica que:
   - Los símbolos de Rust aparecen demangled (nombres legibles, no `_ZN...`).
   - Ves `rust_profile::sum_array`, `rust_profile::hash_all`, `rust_profile::matmul`.
   - El hotspot principal corresponde a lo que esperas.

**f)** Usa `perf annotate` en una de las funciones para ver el assembly generado por el compilador de Rust. ¿Ves instrucciones SIMD (AVX/SSE)?

---

## Navegación

- **Anterior**: [C04/S01/T04 - Trampas comunes](../../S01_Microbenchmarking/T04_Trampas_comunes/README.md)
- **Siguiente**: [T02 - Flamegraphs](../T02_Flamegraphs/README.md)

---

> **Capítulo 4: Benchmarking y Profiling** — Sección 2: Profiling de Aplicaciones — Tópico 1 de 4
