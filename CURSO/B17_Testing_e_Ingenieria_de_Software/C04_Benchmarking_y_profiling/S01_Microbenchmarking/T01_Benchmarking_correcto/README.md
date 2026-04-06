# T01 - Benchmarking correcto: warmup, iteraciones, varianza, por qué `time ./programa` no basta

## Índice

1. [Qué es benchmarking](#1-qué-es-benchmarking)
2. [Por qué medir rendimiento es difícil](#2-por-qué-medir-rendimiento-es-difícil)
3. [El problema con `time ./programa`](#3-el-problema-con-time-programa)
4. [Fuentes de ruido en las mediciones](#4-fuentes-de-ruido-en-las-mediciones)
5. [Relojes del sistema: cuál usar](#5-relojes-del-sistema-cuál-usar)
6. [Wall clock vs CPU time vs monotonic](#6-wall-clock-vs-cpu-time-vs-monotonic)
7. [Resolución y overhead del reloj](#7-resolución-y-overhead-del-reloj)
8. [Warmup: por qué las primeras mediciones son basura](#8-warmup-por-qué-las-primeras-mediciones-son-basura)
9. [Iteraciones: cuántas repeticiones necesitas](#9-iteraciones-cuántas-repeticiones-necesitas)
10. [Estadística básica para benchmarks](#10-estadística-básica-para-benchmarks)
11. [Media vs mediana vs percentiles](#11-media-vs-mediana-vs-percentiles)
12. [Desviación estándar y coeficiente de variación](#12-desviación-estándar-y-coeficiente-de-variación)
13. [Intervalos de confianza](#13-intervalos-de-confianza)
14. [Outliers: detectar y manejar](#14-outliers-detectar-y-manejar)
15. [Benchmark de una función en C](#15-benchmark-de-una-función-en-c)
16. [Benchmark de una función en Rust](#16-benchmark-de-una-función-en-rust)
17. [Evitar que el compilador elimine tu benchmark](#17-evitar-que-el-compilador-elimine-tu-benchmark)
18. [Diseño de un microbenchmark correcto](#18-diseño-de-un-microbenchmark-correcto)
19. [Entorno de ejecución: preparar la máquina](#19-entorno-de-ejecución-preparar-la-máquina)
20. [CPU frequency scaling y turbo boost](#20-cpu-frequency-scaling-y-turbo-boost)
21. [Afinidad de CPU (CPU pinning)](#21-afinidad-de-cpu-cpu-pinning)
22. [Caché: el elefante en la habitación](#22-caché-el-elefante-en-la-habitación)
23. [Benchmark comparativo: A vs B](#23-benchmark-comparativo-a-vs-b)
24. [Reportar resultados correctamente](#24-reportar-resultados-correctamente)
25. [Errores clásicos de benchmarking](#25-errores-clásicos-de-benchmarking)
26. [Checklist de benchmarking](#26-checklist-de-benchmarking)
27. [Programa de práctica: bench_framework](#27-programa-de-práctica-bench_framework)
28. [Ejercicios](#28-ejercicios)

---

## 1. Qué es benchmarking

Benchmarking es la **medición sistemática** del rendimiento de un programa, función, o algoritmo. El objetivo es obtener números reproducibles y significativos que permitan tomar decisiones de optimización.

```
┌──────────────────────────────────────────────────────────────────┐
│                     Benchmarking                                  │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐      │
│  │                Niveles de benchmarking                   │      │
│  │                                                        │      │
│  │  Microbenchmark     Una función aislada                 │      │
│  │  ──────────────     qsort(), parse_json(), hash()      │      │
│  │  Milisegundos a     "¿Cuánto tarda hash(key)?"          │      │
│  │  nanosegundos                                          │      │
│  │                                                        │      │
│  │  Component bench    Un módulo o subsistema              │      │
│  │  ──────────────     parser + transformer                │      │
│  │  Segundos           "¿Cuánto tarda procesar 1M lines?" │      │
│  │                                                        │      │
│  │  System bench       El sistema completo                 │      │
│  │  ──────────────     servidor bajo carga                 │      │
│  │  Minutos             "¿Cuántas req/s soporta?"          │      │
│  │                                                        │      │
│  │  ◄── Más control ─────────── Más realista ──►          │      │
│  │  ◄── Más reproducible ────── Más variable ──►          │      │
│  └────────────────────────────────────────────────────────┘      │
│                                                                  │
│  Este tópico se enfoca en MICROBENCHMARKING:                     │
│  cómo medir correctamente funciones individuales.                │
└──────────────────────────────────────────────────────────────────┘
```

### ¿Para qué sirve?

```
Benchmarking responde preguntas como:
• ¿Mi optimización realmente mejoró el rendimiento?
• ¿Qué implementación es más rápida: A o B?
• ¿Cuál es el costo de esta abstracción?
• ¿Mi cambio introdujo una regresión de rendimiento?
• ¿Cuántas operaciones por segundo soporta esta función?
```

---

## 2. Por qué medir rendimiento es difícil

```
┌──────────────────────────────────────────────────────────────────┐
│      ¿Por qué no puedes simplemente cronometrar tu programa?     │
│                                                                  │
│  Fuente de error         │ Impacto                               │
│  ────────────────────────┼──────────────────────────              │
│  Frequency scaling       │ La CPU cambia velocidad durante       │
│                          │ la ejecución (1.2 GHz → 4.5 GHz)    │
│                          │                                       │
│  Cache effects           │ Primera ejecución: cache frío (lento) │
│                          │ Siguientes: cache caliente (rápido)   │
│                          │                                       │
│  Branch prediction       │ El predictor aprende tu patrón        │
│                          │ durante el benchmark                  │
│                          │                                       │
│  OS scheduling           │ Otro proceso roba tu CPU              │
│                          │ Context switches añaden latencia      │
│                          │                                       │
│  Interrupts              │ Timer interrupts, network, disk I/O   │
│                          │ causan pausas impredecibles           │
│                          │                                       │
│  Memory allocator        │ malloc puede ser rápido o lento       │
│                          │ dependiendo de fragmentación          │
│                          │                                       │
│  NUMA effects            │ Acceso a memoria de otro nodo NUMA    │
│                          │ es 2-3x más lento                     │
│                          │                                       │
│  TLB misses              │ Page table lookups en accesos         │
│                          │ dispersos a memoria virtual           │
│                          │                                       │
│  Compiler optimizations  │ El compilador puede eliminar código   │
│                          │ que cree que no tiene efecto           │
│                          │                                       │
│  Thermal throttling      │ CPU se calienta → reduce velocidad    │
│                          │ durante benchmarks largos             │
│                          │                                       │
│  Resultado: cada ejecución da un tiempo DIFERENTE.               │
│  Una sola medición no dice NADA confiable.                       │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. El problema con `time ./programa`

### ¿Qué hace `time`?

```bash
$ time ./my_program

real    0m1.234s    # Wall clock: tiempo total transcurrido
user    0m0.890s    # CPU time en modo usuario
sys     0m0.120s    # CPU time en modo kernel (syscalls)
```

### ¿Por qué no basta?

```
┌──────────────────────────────────────────────────────────────────┐
│         Problemas de `time ./programa`                           │
│                                                                  │
│  1. UNA sola medición                                            │
│     No hay repeticiones, no hay estadística.                     │
│     ¿Cómo sabes si 1.234s es normal o un outlier?               │
│                                                                  │
│  2. Incluye STARTUP del proceso                                  │
│     Cargar el binario, linker dinámico, inicializar libc,        │
│     mmap de páginas, etc. → overhead significativo para          │
│     programas que ejecutan en milisegundos.                      │
│                                                                  │
│  3. Incluye I/O                                                  │
│     Si tu programa imprime resultados, el tiempo de             │
│     write(stdout) se incluye.                                    │
│                                                                  │
│  4. Resolución baja                                              │
│     `time` típicamente tiene resolución de ~10ms.                │
│     Para funciones que tardan microsegundos, es inútil.          │
│                                                                  │
│  5. No mide lo que quieres medir                                 │
│     Quieres medir UNA función. `time` mide TODO el proceso.     │
│                                                                  │
│  6. No controla el entorno                                       │
│     No hace warmup, no fija CPU frequency, no controla cache.   │
│                                                                  │
│  Resultado: `time` es útil para mediciones GROSERAS             │
│  ("tarda ~1 segundo" vs "tarda ~10 segundos"),                  │
│  pero es INADECUADO para microbenchmarking.                      │
└──────────────────────────────────────────────────────────────────┘
```

### ¿Cuándo SÍ es útil `time`?

```
time SÍ es útil para:
• Verificar que un programa no se cuelga (timeout burdo)
• Comparaciones muy groseras (1s vs 60s)
• Medir el tiempo total de un pipeline completo
• Medir programas que tardan minutos (el ruido relativo es bajo)
• "¿Esto tarda más de lo razonable?" (triage rápido)

time NO es útil para:
• Comparar dos implementaciones (diferencia de 5-10%)
• Medir funciones individuales
• Detectar regresiones sutiles de rendimiento
• Cualquier medición que necesite ser reproducible
```

### Alternativa rápida: `hyperfine`

```bash
# hyperfine: benchmarking de línea de comandos correcto
# Hace warmup, múltiples ejecuciones, estadística
$ hyperfine './programa_a' './programa_b'

Benchmark 1: ./programa_a
  Time (mean ± σ):     123.4 ms ±  2.1 ms    [User: 118.2 ms, System: 4.8 ms]
  Range (min … max):   120.1 ms … 128.3 ms    10 runs

Benchmark 2: ./programa_b
  Time (mean ± σ):      98.7 ms ±  1.8 ms    [User: 94.1 ms, System: 4.2 ms]
  Range (min … max):    96.2 ms … 102.1 ms    10 runs

Summary
  ./programa_b ran
    1.25 ± 0.03 times faster than ./programa_a

# Opciones útiles de hyperfine:
# --warmup 5        → 5 ejecuciones de warmup antes de medir
# --runs 50         → 50 ejecuciones medidas
# --min-runs 20     → al menos 20 ejecuciones
# --export-json     → exportar resultados para análisis
# --prepare 'cmd'   → ejecutar antes de cada run (limpiar cache)
# --show-output     → mostrar output del programa
```

---

## 4. Fuentes de ruido en las mediciones

### Categorías de ruido

```
┌──────────────────────────────────────────────────────────────────────┐
│              Fuentes de ruido en benchmarks                          │
│                                                                      │
│  RUIDO DEL HARDWARE:                                                 │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │  • Frequency scaling: CPU ajusta MHz según carga            │    │
│  │  • Turbo boost: boost temporal que se desactiva por calor   │    │
│  │  • Thermal throttling: reducción por temperatura            │    │
│  │  • Cache contention: otros cores contaminan cache compartido│    │
│  │  • DRAM refresh: ciclos de refresh pausan accesos           │    │
│  │  • Hyper-threading: otro hilo lógico compite por recursos   │    │
│  └──────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  RUIDO DEL OS:                                                       │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │  • Scheduling: context switches a otros procesos            │    │
│  │  • Timer interrupts: ~1000/s (cada 1ms con CONFIG_HZ=1000) │    │
│  │  • Page faults: primer acceso a página = soft fault         │    │
│  │  • TLB shootdowns: invalidación de TLB por otro core        │    │
│  │  • Network interrupts: si la red está activa                │    │
│  │  • Filesystem cache: lecturas pueden ser cache hit o miss   │    │
│  │  • ASLR: posiciones de memoria varían entre ejecuciones     │    │
│  │    (afecta alineamiento de funciones en cache de instruc.)  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  RUIDO DEL SOFTWARE:                                                 │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │  • Allocator state: fragmentación, thread-local caches      │    │
│  │  • GC pauses: en lenguajes con GC (no aplica a C/Rust)     │    │
│  │  • JIT compilation: en lenguajes interpretados              │    │
│  │  • Lazy initialization: primera llamada inicializa          │    │
│  │  • Branch predictor state: training durante el benchmark    │    │
│  │  • Compiler optimizations: dead code elimination            │    │
│  └──────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

### Magnitud del ruido

```
┌──────────────────────────────────────────────────────────────────┐
│           Magnitud típica de ruido por fuente                    │
│                                                                  │
│  Fuente                  │ Impacto en tiempo                     │
│  ────────────────────────┼─────────────────────                  │
│  Cache miss L1 → L2      │ ~4-7 ns (vs ~1 ns hit)               │
│  Cache miss L2 → L3      │ ~10-20 ns                            │
│  Cache miss L3 → DRAM    │ ~60-100 ns                           │
│  TLB miss                │ ~10-30 ns                             │
│  Context switch           │ ~1-10 μs                             │
│  Page fault (soft)       │ ~1-5 μs                              │
│  Page fault (hard/disk)  │ ~1-10 ms                             │
│  Frequency transition    │ ~10-100 μs (transient)               │
│  Turbo boost fluctuation │ ~10-30% variación en throughput      │
│  Timer interrupt          │ ~1-5 μs cada ~1 ms                  │
│  Network interrupt        │ ~1-10 μs (esporádico)               │
│  malloc (fast path)       │ ~20-50 ns                           │
│  malloc (slow path/mmap) │ ~1-100 μs                            │
│                                                                  │
│  Para una función que tarda 100 ns:                              │
│  Un cache miss L3 (80 ns) es un +80% de ruido.                  │
│  Un context switch (5 μs) es un +5000% de ruido.                │
│  → Las primeras iteraciones son MUCHO más ruidosas.              │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. Relojes del sistema: cuál usar

### Tipos de reloj disponibles

```
┌──────────────────────────────────────────────────────────────────────┐
│              Relojes disponibles en Linux                            │
│                                                                      │
│  Reloj                      │ Qué mide          │ Para benchmarks?  │
│  ───────────────────────────┼────────────────────┼───────────────────│
│  CLOCK_REALTIME             │ Hora del sistema   │ NO                │
│                             │ (UTC)              │ Puede saltar      │
│                             │                    │ (NTP adjustments) │
│                                                                      │
│  CLOCK_MONOTONIC            │ Tiempo transcurrido│ SÍ (recomendado) │
│                             │ Nunca retrocede    │ Incluye sleep     │
│                             │ No afectado por NTP│                   │
│                                                                      │
│  CLOCK_MONOTONIC_RAW        │ Igual que MONOTONIC│ SÍ (más preciso) │
│                             │ Sin ajuste NTP     │ No en todos los   │
│                             │ (hardware puro)    │ sistemas          │
│                                                                      │
│  CLOCK_PROCESS_CPUTIME_ID   │ CPU time del       │ ÚTIL              │
│                             │ proceso (user+sys) │ Excluye waits     │
│                             │ No cuenta sleep    │ pero incluye      │
│                             │                    │ interrupts        │
│                                                                      │
│  CLOCK_THREAD_CPUTIME_ID    │ CPU time del hilo  │ ÚTIL              │
│                             │ Excluye otros hilos│ Para benchmarks   │
│                             │                    │ single-threaded   │
│                                                                      │
│  TSC (rdtsc)                │ Ciclos del CPU     │ AVANZADO          │
│                             │ Resolución máxima  │ Necesita calibrar │
│                             │ (~1 ns)            │ frecuencia        │
│                                                                      │
│  RECOMENDACIÓN:                                                      │
│  • Benchmarks normales: CLOCK_MONOTONIC                              │
│  • Benchmarks de CPU pura: CLOCK_PROCESS_CPUTIME_ID                  │
│  • Alta precisión: CLOCK_MONOTONIC_RAW o rdtsc                       │
└──────────────────────────────────────────────────────────────────────┘
```

### En C

```c
#include <time.h>

// Obtener timestamp con CLOCK_MONOTONIC:
struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

// ... código a medir ...

clock_gettime(CLOCK_MONOTONIC, &end);

// Calcular diferencia en nanosegundos:
long long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL
                     + (end.tv_nsec - start.tv_nsec);

printf("Elapsed: %lld ns\n", elapsed_ns);
```

### En Rust

```rust
use std::time::Instant;

// Instant usa CLOCK_MONOTONIC internamente:
let start = Instant::now();

// ... código a medir ...

let elapsed = start.elapsed();

println!("Elapsed: {:?}", elapsed);           // "1.234567ms"
println!("Elapsed: {} ns", elapsed.as_nanos()); // nanosegundos
```

---

## 6. Wall clock vs CPU time vs monotonic

```
┌──────────────────────────────────────────────────────────────────┐
│         Wall clock vs CPU time: cuándo usar cada uno             │
│                                                                  │
│  WALL CLOCK (CLOCK_MONOTONIC):                                   │
│  ─────────────────────────────                                   │
│  Mide: tiempo real transcurrido (incluye esperas)                │
│                                                                  │
│  Ejemplo: función que hace I/O                                   │
│  ┌──────────┬──────────┬──────────┬──────────┐                   │
│  │ compute  │  wait    │ compute  │  wait    │                   │
│  │  20ms    │  80ms    │  10ms    │  50ms    │                   │
│  └──────────┴──────────┴──────────┴──────────┘                   │
│  Wall clock: 160ms  ← mide TODO                                 │
│  CPU time:   30ms   ← solo el compute                            │
│                                                                  │
│  Usar wall clock cuando:                                         │
│  • Mides latencia visible al usuario                             │
│  • Tu función hace I/O o espera                                  │
│  • Mides el sistema completo                                     │
│                                                                  │
│  CPU TIME (CLOCK_PROCESS_CPUTIME_ID):                            │
│  ────────────────────────────────────                             │
│  Mide: tiempo que el CPU ejecutó TU código                       │
│  No cuenta: sleep, I/O waits, context switches                   │
│                                                                  │
│  Usar CPU time cuando:                                           │
│  • Mides CPU pura (algoritmos, cómputo)                          │
│  • Quieres excluir ruido de scheduling                           │
│  • Comparas eficiencia algorítmica                               │
│                                                                  │
│  CUIDADO: CPU time puede ser > wall clock en multi-thread        │
│  (4 hilos × 1s cada uno = 4s CPU time, 1s wall clock)            │
│                                                                  │
│  PARA MICROBENCHMARKS: usar CLOCK_MONOTONIC es lo estándar.      │
│  Criterion, Google Benchmark, etc. todos usan wall clock.        │
└──────────────────────────────────────────────────────────────────┘
```

---

## 7. Resolución y overhead del reloj

```
┌──────────────────────────────────────────────────────────────────┐
│         Resolución y overhead de clock_gettime                   │
│                                                                  │
│  Reloj                    │ Resolución típica │ Overhead llamada │
│  ─────────────────────────┼───────────────────┼──────────────────│
│  CLOCK_MONOTONIC          │ 1 ns              │ ~20-30 ns (vDSO) │
│  CLOCK_MONOTONIC_RAW      │ 1 ns              │ ~20-30 ns (vDSO) │
│  CLOCK_PROCESS_CPUTIME_ID │ 1 ns              │ ~200-500 ns      │
│  CLOCK_THREAD_CPUTIME_ID  │ 1 ns              │ ~200-500 ns      │
│  rdtsc (inline asm)       │ ~0.3 ns           │ ~10-20 ns        │
│  Rust Instant::now()      │ 1 ns              │ ~20-30 ns (vDSO) │
│                                                                  │
│  vDSO: Virtual Dynamic Shared Object                             │
│  Linux mapea clock_gettime en userspace para evitar syscall.     │
│  CLOCK_MONOTONIC y REALTIME usan vDSO → muy rápidos.            │
│  CPUTIME no usa vDSO → necesita syscall real → más lento.        │
│                                                                  │
│  Implicación para benchmarks:                                    │
│  Si tu función tarda 10 ns, y clock_gettime tarda 25 ns,        │
│  el overhead de medición es del 250%.                             │
│  Solución: medir N iteraciones juntas y dividir por N.           │
│                                                                  │
│  Ejemplo:                                                        │
│  // MAL: medir cada iteración (overhead 250%)                    │
│  for (i = 0; i < 1000; i++) {                                    │
│      start = now();                                              │
│      f();          // 10 ns                                      │
│      times[i] = now() - start;  // + 50 ns de medición          │
│  }                                                               │
│                                                                  │
│  // BIEN: medir el bloque completo (overhead <1%)                │
│  start = now();                                                  │
│  for (i = 0; i < 1000; i++) {                                    │
│      f();                                                        │
│  }                                                               │
│  avg = (now() - start) / 1000;  // 50 ns / 1000 = 0.05 ns       │
└──────────────────────────────────────────────────────────────────┘
```

### Verificar resolución en tu sistema

```c
// check_resolution.c
#include <stdio.h>
#include <time.h>

int main() {
    struct timespec res;

    clock_getres(CLOCK_MONOTONIC, &res);
    printf("CLOCK_MONOTONIC resolution: %ld s, %ld ns\n",
           res.tv_sec, res.tv_nsec);

    clock_getres(CLOCK_REALTIME, &res);
    printf("CLOCK_REALTIME resolution:  %ld s, %ld ns\n",
           res.tv_sec, res.tv_nsec);

    clock_getres(CLOCK_PROCESS_CPUTIME_ID, &res);
    printf("CLOCK_PROCESS_CPUTIME_ID:   %ld s, %ld ns\n",
           res.tv_sec, res.tv_nsec);

    // Medir overhead de clock_gettime:
    struct timespec start, end;
    int N = 1000000;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N; i++) {
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL
                      + (end.tv_nsec - start.tv_nsec);
    printf("clock_gettime overhead: %lld ns/call\n", elapsed / N);

    return 0;
}
```

```bash
$ gcc -O1 check_resolution.c -o check_resolution && ./check_resolution
CLOCK_MONOTONIC resolution: 0 s, 1 ns
CLOCK_REALTIME resolution:  0 s, 1 ns
CLOCK_PROCESS_CPUTIME_ID:   0 s, 1 ns
clock_gettime overhead: 22 ns/call
```

---

## 8. Warmup: por qué las primeras mediciones son basura

### ¿Qué pasa en las primeras ejecuciones?

```
┌──────────────────────────────────────────────────────────────────┐
│              Efectos de "arranque en frío"                        │
│                                                                  │
│  Iteración 1 (FRÍA):                                             │
│  • Instrucciones de la función no están en L1i cache             │
│  • Datos no están en L1d/L2/L3 cache                             │
│  • TLB no tiene las entradas de páginas                          │
│  • Branch predictor no conoce los patrones                       │
│  • CPU puede estar a frecuencia baja                             │
│  • malloc puede necesitar pedir páginas al kernel                │
│  → Tiempo: 500 ns                                                │
│                                                                  │
│  Iteración 2-10 (TRANSICIÓN):                                   │
│  • Instrucciones parcialmente en cache                            │
│  • Datos cargándose en cache                                      │
│  • Branch predictor aprendiendo                                  │
│  • CPU subiendo de frecuencia                                    │
│  → Tiempo: 200 ns → 100 ns → 80 ns                              │
│                                                                  │
│  Iteración 100+ (CALIENTE):                                     │
│  • Todo en cache                                                  │
│  • Branch predictor entrenado                                    │
│  • CPU a máxima frecuencia                                        │
│  • malloc servido desde free list                                 │
│  → Tiempo: 50 ns (estable)                                       │
│                                                                  │
│  ┌──────────────────────────────────────────────────────┐         │
│  │   500 ─ ×                                           │         │
│  │        │   ×                                        │         │
│  │   300 ─│     ×                                      │         │
│  │  ns    │       × ×                                  │         │
│  │   100 ─│           × × × × × × × × × × × × × × ×  │         │
│  │    50 ─│                                            │         │
│  │        └──────────────────────────────────────────  │         │
│  │         1   5   10  15  20  25  30  ...  iteración  │         │
│  │         ◄── warmup ──►◄──── mediciones válidas ──►  │         │
│  └──────────────────────────────────────────────────────┘         │
│                                                                  │
│  SOLUCIÓN: descartar las primeras N iteraciones (warmup).        │
│  N depende de la función, pero 10-100 suele ser suficiente.      │
└──────────────────────────────────────────────────────────────────┘
```

### Implementar warmup

```c
// Warmup en C:
void benchmark_function(void (*func)(void), int warmup, int iterations) {
    // Warmup: ejecutar sin medir
    for (int i = 0; i < warmup; i++) {
        func();
    }

    // Ahora sí medir
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < iterations; i++) {
        func();
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL
                         + (end.tv_nsec - start.tv_nsec);
    double avg_ns = (double)elapsed_ns / iterations;
    printf("Average: %.2f ns/iter\n", avg_ns);
}
```

```rust
// Warmup en Rust:
fn benchmark<F: Fn()>(func: F, warmup: usize, iterations: usize) {
    // Warmup
    for _ in 0..warmup {
        func();
    }

    // Medir
    let start = std::time::Instant::now();
    for _ in 0..iterations {
        func();
    }
    let elapsed = start.elapsed();
    let avg_ns = elapsed.as_nanos() as f64 / iterations as f64;
    println!("Average: {:.2} ns/iter", avg_ns);
}
```

---

## 9. Iteraciones: cuántas repeticiones necesitas

```
┌──────────────────────────────────────────────────────────────────┐
│          ¿Cuántas iteraciones para un benchmark?                 │
│                                                                  │
│  Regla general: el benchmark debe ejecutar por al menos          │
│  1-5 SEGUNDOS para que el ruido sea estadísticamente manejable.  │
│                                                                  │
│  Tiempo de la función  │  Iteraciones mínimas                    │
│  ──────────────────────┼───────────────────────                   │
│  ~1 ns                 │  1,000,000,000 (10^9) → ~1s             │
│  ~10 ns                │  100,000,000 (10^8) → ~1s               │
│  ~100 ns               │  10,000,000 (10^7) → ~1s                │
│  ~1 μs                 │  1,000,000 (10^6) → ~1s                 │
│  ~10 μs                │  100,000 (10^5) → ~1s                   │
│  ~100 μs               │  10,000 (10^4) → ~1s                    │
│  ~1 ms                 │  1,000 (10^3) → ~1s                     │
│  ~10 ms                │  100 → ~1s                              │
│  ~100 ms               │  10-50 → ~1-5s                          │
│  ~1 s                  │  5-10 → ~5-10s                          │
│                                                                  │
│  Además de las iteraciones DENTRO de un sample, necesitas        │
│  múltiples SAMPLES para calcular estadísticas:                   │
│                                                                  │
│  Sample 1: ejecutar 10^6 iteraciones → avg = 102.3 ns            │
│  Sample 2: ejecutar 10^6 iteraciones → avg = 101.8 ns            │
│  Sample 3: ejecutar 10^6 iteraciones → avg = 103.1 ns            │
│  ...                                                             │
│  Sample 50: ejecutar 10^6 iteraciones → avg = 102.0 ns           │
│                                                                  │
│  Con 50 samples: media ± desviación, intervalos de confianza.    │
│                                                                  │
│  RECOMENDACIÓN:                                                  │
│  • Mínimo 10 samples                                             │
│  • Idealmente 50-100 samples                                     │
│  • Cada sample: al menos 10^5 iteraciones o 100ms               │
│  • Total: al menos 5 segundos de medición                        │
│  • Criterion (Rust) maneja esto automáticamente                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 10. Estadística básica para benchmarks

### ¿Por qué necesitas estadística?

```
┌──────────────────────────────────────────────────────────────────┐
│  Sin estadística:                                                │
│  "Mi función tarda 102 ns"                                       │
│  → ¿Es 102 o podría ser 90-120? No se sabe.                     │
│                                                                  │
│  Con estadística:                                                │
│  "Mi función tarda 102.3 ± 1.2 ns (95% CI)"                     │
│  → Con 95% de confianza, está entre 101.1 y 103.5 ns.           │
│  → Puedo comparar con otra implementación significativamente.    │
│                                                                  │
│  Sin estadística:                                                │
│  "A tarda 102 ns, B tarda 105 ns. A es más rápido."             │
│  → ¿O es solo ruido? Tal vez la diferencia no es real.           │
│                                                                  │
│  Con estadística:                                                │
│  "A: 102.3 ± 1.2 ns, B: 105.1 ± 1.4 ns. Los intervalos de     │
│   confianza no se solapan → la diferencia es significativa."     │
└──────────────────────────────────────────────────────────────────┘
```

### Fórmulas clave

```
Dado un conjunto de N mediciones: t₁, t₂, ..., tₙ

Media (average):
  x̄ = (t₁ + t₂ + ... + tₙ) / N

Mediana (median):
  El valor central cuando los datos están ordenados
  Si N es par: promedio de los dos centrales

Desviación estándar (std dev, σ):
  σ = sqrt( Σ(tᵢ - x̄)² / (N-1) )
  Mide cuánto se dispersan las mediciones

Desviación estándar de la media (SEM):
  SEM = σ / sqrt(N)
  Mide cuán precisa es la estimación de la media

Coeficiente de variación (CV):
  CV = σ / x̄ × 100%
  Mide la dispersión relativa al valor
  CV < 1% → medición muy estable
  CV 1-5% → aceptable
  CV > 5% → hay mucho ruido, investigar

Intervalo de confianza 95% (aproximado):
  IC₉₅ = x̄ ± 1.96 × SEM
  "Con 95% de probabilidad, el valor real está en este rango"
```

---

## 11. Media vs mediana vs percentiles

```
┌──────────────────────────────────────────────────────────────────┐
│           ¿Qué métrica reportar?                                 │
│                                                                  │
│  Mediciones (ns): 100, 101, 102, 99, 103, 101, 100, 5000, 101   │
│                                                            ↑     │
│                                                    outlier (GC,  │
│                                                    interrupt)    │
│                                                                  │
│  Media:    622.9 ns  ← DISTORSIONADA por el outlier             │
│  Mediana:  101.0 ns  ← ROBUSTA contra outliers                  │
│  Min:       99.0 ns  ← El mejor caso posible                    │
│  Max:     5000.0 ns  ← El peor caso (outlier)                   │
│  P95:      103.0 ns  ← El 95% de las mediciones están debajo   │
│  P99:     5000.0 ns  ← El 99% están debajo (incluye outlier)   │
│                                                                  │
│  PARA MICROBENCHMARKS: la MEDIANA es generalmente mejor que      │
│  la media, porque los outliers (context switches, interrupts)    │
│  inflan la media pero no afectan la mediana.                     │
│                                                                  │
│  CUÁNDO USAR CADA UNA:                                           │
│                                                                  │
│  Media:    Cuando la distribución es simétrica y sin outliers    │
│            "¿Cuál es el throughput promedio?"                     │
│                                                                  │
│  Mediana:  Cuando hay outliers (la mayoría de microbenchmarks)   │
│            "¿Cuál es el tiempo típico?"                           │
│                                                                  │
│  Min:      Para estimar el tiempo "óptimo" sin ruido             │
│            "¿Cuál es el piso de rendimiento?"                    │
│            Cuidado: puede ser un fluke                            │
│                                                                  │
│  Percentiles (P95, P99):                                         │
│            Para latencia en sistemas reales                       │
│            "¿Cuál es la latencia que el 99% de requests ven?"    │
│            CRUCIAL para SLOs/SLAs                                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Desviación estándar y coeficiente de variación

```
┌──────────────────────────────────────────────────────────────────┐
│    Interpretar la desviación estándar en benchmarks              │
│                                                                  │
│  Resultado: 102.3 ± 1.2 ns (σ)                                  │
│                                                                  │
│  ¿Es 1.2 ns mucho o poco?                                       │
│  → Depende del contexto. Calcular CV:                            │
│                                                                  │
│  CV = 1.2 / 102.3 × 100% = 1.17%                                │
│  → Dispersión del 1.17% → BUENO para un microbenchmark          │
│                                                                  │
│  Regla práctica para CV:                                         │
│                                                                  │
│  CV < 1%:   Excelente. Medición muy estable.                     │
│             El benchmark es confiable.                            │
│                                                                  │
│  CV 1-3%:   Bueno. Variación normal para microbenchmarks.        │
│             Resultados confiables si N > 30.                     │
│                                                                  │
│  CV 3-5%:   Aceptable. Hay algo de ruido.                        │
│             Aumentar N o reducir ruido del entorno.               │
│                                                                  │
│  CV 5-10%:  Preocupante. Mucho ruido.                            │
│             ¿Hay I/O? ¿Allocation? ¿Context switches?            │
│             Fijar CPU frequency, aislar cores.                    │
│                                                                  │
│  CV > 10%:  Inaceptable. Benchmark no confiable.                 │
│             Algo está fundamentalmente mal:                       │
│             • La función tiene comportamiento bimodal             │
│             • Hay I/O no controlado                               │
│             • El sistema tiene carga concurrente                  │
│             • El benchmark mide algo no determinista              │
└──────────────────────────────────────────────────────────────────┘
```

---

## 13. Intervalos de confianza

```
┌──────────────────────────────────────────────────────────────────┐
│            Intervalos de confianza en benchmarks                 │
│                                                                  │
│  Un intervalo de confianza (IC) dice:                            │
│  "Con X% de probabilidad, el valor real está en [a, b]"         │
│                                                                  │
│  Fórmula (asumiendo distribución normal):                        │
│  IC₉₅ = x̄ ± z × (σ / √N)                                       │
│  donde z = 1.96 para 95%, z = 2.576 para 99%                    │
│                                                                  │
│  Ejemplo:                                                        │
│  Media = 102.3 ns, σ = 1.2 ns, N = 100 muestras                 │
│  SEM = 1.2 / √100 = 0.12 ns                                     │
│  IC₉₅ = 102.3 ± 1.96 × 0.12 = 102.3 ± 0.24 ns                  │
│  → [102.06, 102.54] con 95% de confianza                        │
│                                                                  │
│  COMPARAR dos implementaciones:                                  │
│                                                                  │
│  A: 102.3 ± 0.24 ns  →  [102.06, 102.54]                        │
│  B: 105.1 ± 0.31 ns  →  [104.79, 105.41]                        │
│                                                                  │
│  Los intervalos NO se solapan → diferencia SIGNIFICATIVA         │
│  B es ~2.7% más lento que A (con alta confianza)                 │
│                                                                  │
│  A: 102.3 ± 0.24 ns  →  [102.06, 102.54]                        │
│  B: 102.8 ± 0.30 ns  →  [102.50, 103.10]                        │
│                                                                  │
│  Los intervalos SE SOLAPAN → diferencia NO SIGNIFICATIVA         │
│  No puedes concluir que B es más lento.                          │
│  Necesitas más muestras para reducir el IC.                      │
└──────────────────────────────────────────────────────────────────┘
```

---

## 14. Outliers: detectar y manejar

```
┌──────────────────────────────────────────────────────────────────┐
│                Manejar outliers en benchmarks                     │
│                                                                  │
│  Estrategias (de más conservadora a más agresiva):               │
│                                                                  │
│  1. REPORTAR MEDIANA (en vez de media)                            │
│     Pro: automáticamente ignora outliers extremos                │
│     Contra: pierde información sobre la dispersión               │
│                                                                  │
│  2. DESCARTAR outliers > 3σ (regla de 3-sigma)                   │
│     Pro: estándar estadístico                                    │
│     Contra: puede descartar datos legítimos                      │
│     Implementación:                                              │
│     - Calcular media y σ                                         │
│     - Descartar todo |x - media| > 3σ                            │
│     - Recalcular media y σ sin outliers                          │
│                                                                  │
│  3. DESCARTAR percentiles extremos (trimmed mean)                │
│     Pro: robusto contra cualquier cantidad de outliers           │
│     Contra: pierde datos reales de los extremos                  │
│     Implementación:                                              │
│     - Ordenar mediciones                                         │
│     - Descartar el 5% más bajo y el 5% más alto                 │
│     - Calcular media del 90% restante                            │
│                                                                  │
│  4. USAR MÍNIMO como estimador                                   │
│     Pro: la medición más rápida es la que menos ruido tuvo       │
│     Contra: puede ser un fluke; no representa "típico"           │
│     Uso: comparaciones de "piso de rendimiento"                   │
│                                                                  │
│  RECOMENDACIÓN:                                                  │
│  Usar mediana para reportar, media para detectar inestabilidad.  │
│  Si CV > 5%, investigar la causa antes de descartar outliers.    │
└──────────────────────────────────────────────────────────────────┘
```

---

## 15. Benchmark de una función en C

### Implementación completa

```c
// bench.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════
//  Framework de benchmark mínimo en C
// ═══════════════════════════════════════════════════════════════

typedef struct {
    double *samples;    // Tiempo por sample en nanosegundos
    int n_samples;
    int iters_per_sample;
    double mean;
    double median;
    double stddev;
    double min;
    double max;
    double cv;          // Coeficiente de variación
} BenchResult;

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static long long timespec_diff_ns(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) * 1000000000LL
         + (end->tv_nsec - start->tv_nsec);
}

BenchResult bench_function(void (*func)(void *), void *arg,
                           int warmup, int n_samples, int iters_per_sample) {
    BenchResult result;
    result.samples = (double *)malloc(n_samples * sizeof(double));
    result.n_samples = n_samples;
    result.iters_per_sample = iters_per_sample;

    // Warmup
    for (int i = 0; i < warmup; i++) {
        func(arg);
    }

    // Medir samples
    for (int s = 0; s < n_samples; s++) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < iters_per_sample; i++) {
            func(arg);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        long long elapsed = timespec_diff_ns(&start, &end);
        result.samples[s] = (double)elapsed / iters_per_sample;
    }

    // Calcular estadísticas
    // Media
    double sum = 0;
    for (int i = 0; i < n_samples; i++) sum += result.samples[i];
    result.mean = sum / n_samples;

    // Min/Max
    result.min = result.samples[0];
    result.max = result.samples[0];
    for (int i = 1; i < n_samples; i++) {
        if (result.samples[i] < result.min) result.min = result.samples[i];
        if (result.samples[i] > result.max) result.max = result.samples[i];
    }

    // Desviación estándar
    double sum_sq = 0;
    for (int i = 0; i < n_samples; i++) {
        double diff = result.samples[i] - result.mean;
        sum_sq += diff * diff;
    }
    result.stddev = sqrt(sum_sq / (n_samples - 1));

    // CV
    result.cv = (result.mean > 0) ? (result.stddev / result.mean) * 100.0 : 0;

    // Mediana
    double *sorted = (double *)malloc(n_samples * sizeof(double));
    memcpy(sorted, result.samples, n_samples * sizeof(double));
    qsort(sorted, n_samples, sizeof(double), cmp_double);
    if (n_samples % 2 == 0) {
        result.median = (sorted[n_samples/2 - 1] + sorted[n_samples/2]) / 2.0;
    } else {
        result.median = sorted[n_samples/2];
    }
    free(sorted);

    return result;
}

void bench_print(const char *name, BenchResult *r) {
    printf("%-30s  mean: %8.1f ns  median: %8.1f ns  "
           "σ: %6.1f ns  CV: %.1f%%  [%8.1f, %8.1f]\n",
           name, r->mean, r->median, r->stddev, r->cv,
           r->min, r->max);
}

void bench_free(BenchResult *r) {
    free(r->samples);
}

// ═══════════════════════════════════════════════════════════════
//  Funciones a benchmarkear
// ═══════════════════════════════════════════════════════════════

// Función 1: suma de array
static int array_data[1024];

void sum_array(void *arg) {
    (void)arg;
    volatile int sum = 0;  // volatile: evita que el compilador elimine
    for (int i = 0; i < 1024; i++) {
        sum += array_data[i];
    }
}

// Función 2: búsqueda lineal
void linear_search(void *arg) {
    int target = *(int *)arg;
    volatile int found = -1;
    for (int i = 0; i < 1024; i++) {
        if (array_data[i] == target) {
            found = i;
            break;
        }
    }
}

// Función 3: búsqueda binaria (array debe estar ordenado)
void binary_search(void *arg) {
    int target = *(int *)arg;
    volatile int found = -1;
    int lo = 0, hi = 1023;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (array_data[mid] == target) {
            found = mid;
            break;
        } else if (array_data[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
}

int main() {
    // Inicializar datos
    for (int i = 0; i < 1024; i++) {
        array_data[i] = i;
    }

    int target = 512;  // Elemento en el medio

    printf("Benchmarking con %d warmup, %d samples, %d iters/sample:\n\n",
           100, 50, 100000);

    BenchResult r1 = bench_function(sum_array, NULL, 100, 50, 100000);
    bench_print("sum_array (1024 ints)", &r1);

    BenchResult r2 = bench_function(linear_search, &target, 100, 50, 100000);
    bench_print("linear_search (target=512)", &r2);

    BenchResult r3 = bench_function(binary_search, &target, 100, 50, 100000);
    bench_print("binary_search (target=512)", &r3);

    // Comparación
    printf("\nComparación linear vs binary:\n");
    printf("  linear / binary = %.2fx\n", r2.median / r3.median);
    if (r2.median > r3.median) {
        printf("  binary_search es %.1f%% más rápido\n",
               (1.0 - r3.median / r2.median) * 100);
    }

    bench_free(&r1);
    bench_free(&r2);
    bench_free(&r3);

    return 0;
}
```

```bash
$ clang -O2 -lm bench.c -o bench && ./bench

Benchmarking con 100 warmup, 50 samples, 100000 iters/sample:

sum_array (1024 ints)           mean:    312.4 ns  median:    311.8 ns  σ:    3.2 ns  CV: 1.0%  [   308.1,    325.4]
linear_search (target=512)      mean:    198.5 ns  median:    197.9 ns  σ:    2.8 ns  CV: 1.4%  [   195.1,    210.3]
binary_search (target=512)      mean:     38.2 ns  median:     37.9 ns  σ:    1.1 ns  CV: 2.9%  [    36.8,     42.1]

Comparación linear vs binary:
  linear / binary = 5.22x
  binary_search es 80.8% más rápido
```

---

## 16. Benchmark de una función en Rust

### Usando `std::time::Instant` (sin dependencias)

```rust
// src/main.rs
use std::time::Instant;

fn bench<F: Fn()>(name: &str, func: F, warmup: usize, samples: usize, iters: usize) {
    // Warmup
    for _ in 0..warmup {
        func();
    }

    // Medir samples
    let mut times: Vec<f64> = Vec::with_capacity(samples);
    for _ in 0..samples {
        let start = Instant::now();
        for _ in 0..iters {
            func();
        }
        let elapsed = start.elapsed().as_nanos() as f64 / iters as f64;
        times.push(elapsed);
    }

    // Estadísticas
    let mean = times.iter().sum::<f64>() / times.len() as f64;
    let variance = times.iter().map(|t| (t - mean).powi(2)).sum::<f64>()
                   / (times.len() - 1) as f64;
    let stddev = variance.sqrt();
    let cv = stddev / mean * 100.0;

    let mut sorted = times.clone();
    sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let median = if sorted.len() % 2 == 0 {
        (sorted[sorted.len() / 2 - 1] + sorted[sorted.len() / 2]) / 2.0
    } else {
        sorted[sorted.len() / 2]
    };
    let min = sorted[0];
    let max = sorted[sorted.len() - 1];

    println!(
        "{:<30} mean: {:>8.1} ns  median: {:>8.1} ns  \
         σ: {:>6.1} ns  CV: {:.1}%  [{:>8.1}, {:>8.1}]",
        name, mean, median, stddev, cv, min, max,
    );
}

fn main() {
    let data: Vec<i32> = (0..1024).collect();
    let target = 512;

    println!("Benchmarking (100 warmup, 50 samples, 100000 iters):\n");

    bench("sum (1024 ints)", || {
        let sum: i32 = std::hint::black_box(&data)
            .iter()
            .sum();
        std::hint::black_box(sum);
    }, 100, 50, 100000);

    bench("linear_search (512)", || {
        let pos = std::hint::black_box(&data)
            .iter()
            .position(|&x| x == std::hint::black_box(target));
        std::hint::black_box(pos);
    }, 100, 50, 100000);

    bench("binary_search (512)", || {
        let result = std::hint::black_box(&data)
            .binary_search(&std::hint::black_box(target));
        std::hint::black_box(result);
    }, 100, 50, 100000);
}
```

---

## 17. Evitar que el compilador elimine tu benchmark

Este es el problema **más importante** del microbenchmarking. Si el compilador determina que el resultado de tu función no se usa, puede **eliminar la función entera**.

```
┌──────────────────────────────────────────────────────────────────┐
│       Dead Code Elimination: el enemigo del benchmark            │
│                                                                  │
│  // Lo que escribiste:                                           │
│  for (int i = 0; i < N; i++) {                                   │
│      int result = expensive_computation(data);                   │
│      // result nunca se usa                                      │
│  }                                                               │
│                                                                  │
│  // Lo que el compilador genera (con -O2):                       │
│  // ... nada. Todo el loop se eliminó.                           │
│  // Benchmark reporta: 0.1 ns/iter (imposiblemente rápido)      │
│                                                                  │
│  ¿Por qué? El compilador ve que:                                 │
│  1. result no se usa después                                     │
│  2. expensive_computation no tiene side effects visibles         │
│  3. El loop no tiene efecto observable                           │
│  → ELIMINADO como dead code                                     │
│                                                                  │
│  Esto NO es un bug del compilador. Es una optimización correcta. │
│  El problema es del benchmark.                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Soluciones

```
┌──────────────────────────────────────────────────────────────────┐
│            Técnicas anti-eliminación                              │
│                                                                  │
│  EN C:                                                           │
│                                                                  │
│  1. volatile (fuerza lectura/escritura a memoria)                │
│     volatile int result = 0;                                     │
│     for (int i = 0; i < N; i++) {                                │
│         result = expensive(data);                                │
│     }                                                            │
│     PRO: simple, siempre funciona                                │
│     CONTRA: volatile store añade ~1-5 ns de overhead             │
│                                                                  │
│  2. escape() / clobber() (Google Benchmark style)                │
│     // Decirle al compilador "no sé qué hace con este puntero"  │
│     static void escape(void *p) {                                │
│         asm volatile("" : : "g"(p) : "memory");                  │
│     }                                                            │
│     // Decirle "puede que la memoria haya cambiado"              │
│     static void clobber(void) {                                  │
│         asm volatile("" : : : "memory");                         │
│     }                                                            │
│     int result = expensive(data);                                │
│     escape(&result);  // Compilador asume que alguien usa result │
│     PRO: zero overhead real                                      │
│     CONTRA: depende de inline asm (GCC/Clang only)               │
│                                                                  │
│  3. Acumular resultado y usarlo al final                         │
│     int sum = 0;                                                 │
│     for (int i = 0; i < N; i++) {                                │
│         sum += expensive(data);  // Acumula                      │
│     }                                                            │
│     printf("Sum: %d\n", sum);  // Usa al final                  │
│     PRO: portable                                                │
│     CONTRA: el += puede cambiar el codegen                       │
│                                                                  │
│  EN RUST:                                                        │
│                                                                  │
│  std::hint::black_box(value)                                     │
│  → Le dice al compilador: "no sé qué hace con este valor"       │
│  → El compilador no puede eliminar el cómputo                   │
│  → Zero overhead en release builds                               │
│                                                                  │
│  let result = std::hint::black_box(expensive(                    │
│      std::hint::black_box(&data)                                 │
│  ));                                                             │
│  // black_box en el input: evita que el compilador              │
│  //   pre-compute el resultado en compilación (const-folding)    │
│  // black_box en el output: evita que elimine el cómputo        │
└──────────────────────────────────────────────────────────────────┘
```

### Ejemplo detallado en C

```c
#include <stdio.h>

// asm barrier para evitar eliminación (Clang/GCC):
static inline void do_not_optimize(void *p) {
    asm volatile("" : : "g"(p) : "memory");
}

int compute(int x) {
    return x * x + x * 3 + 7;
}

void bench_wrong(void) {
    // MAL: resultado no usado → compilador elimina el loop
    for (int i = 0; i < 1000000; i++) {
        compute(i);  // ← ELIMINADO por el compilador
    }
}

void bench_correct(void) {
    // BIEN: resultado "escapa" al compilador
    for (int i = 0; i < 1000000; i++) {
        int result = compute(i);
        do_not_optimize(&result);  // Compilador asume que se usa
    }
}
```

### Ejemplo detallado en Rust

```rust
use std::hint::black_box;

fn compute(x: i32) -> i32 {
    x * x + x * 3 + 7
}

fn bench_wrong() {
    // MAL: resultado no usado
    for i in 0..1_000_000 {
        compute(i);  // ← ELIMINADO por el compilador
    }
}

fn bench_correct() {
    // BIEN: black_box en input y output
    for i in 0..1_000_000 {
        black_box(compute(black_box(i)));
    }
}
```

---

## 18. Diseño de un microbenchmark correcto

```
┌──────────────────────────────────────────────────────────────────┐
│           Anatomía de un microbenchmark correcto                 │
│                                                                  │
│  1. SETUP (fuera del timing)                                     │
│     Preparar datos de entrada, allocar memoria,                  │
│     inicializar estructuras.                                     │
│     → NO se mide                                                 │
│                                                                  │
│  2. WARMUP (ejecutar sin medir)                                  │
│     Ejecutar la función N veces para:                            │
│     • Cargar instrucciones y datos en cache                      │
│     • Entrenar branch predictor                                  │
│     • Estabilizar CPU frequency                                  │
│     → NO se mide                                                 │
│                                                                  │
│  3. MEDICIÓN (múltiples samples)                                 │
│     Para cada sample:                                            │
│     a) Tomar timestamp inicio                                    │
│     b) Ejecutar la función M iteraciones                         │
│     c) Tomar timestamp fin                                       │
│     d) Calcular: (fin - inicio) / M                              │
│     → SÍ se mide                                                 │
│                                                                  │
│  4. ANÁLISIS                                                     │
│     Calcular: media, mediana, σ, CV, min, max                    │
│     Verificar CV < 5%                                            │
│     Descartar outliers si es necesario                           │
│                                                                  │
│  5. REPORTE                                                      │
│     Reportar: mediana ± SEM, o media ± σ                         │
│     Incluir: N samples, M iters, CV                              │
│                                                                  │
│  6. TEARDOWN (fuera del timing)                                  │
│     Liberar memoria, verificar resultados.                       │
│     → NO se mide                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### Qué medir y qué no

```
┌──────────────────────────────────────────────────────────────────┐
│  INCLUIR en la medición:                                         │
│  • La función que quieres medir (obvio)                          │
│  • Acceso a los datos de input (parte del costo real)            │
│                                                                  │
│  EXCLUIR de la medición:                                         │
│  • Allocación de datos de input (setup)                          │
│  • Impresión de resultados (I/O)                                 │
│  • Verificación de correctitud (asserts)                         │
│  • Generación de datos aleatorios                                │
│  • Conversión de resultados para display                         │
│                                                                  │
│  CUIDADO con:                                                    │
│  • Allocación DENTRO de la función medida                        │
│    Si la función hace malloc, eso es parte del costo real.       │
│    No lo excluyas a menos que explícitamente quieras medir       │
│    solo el cómputo sin allocación.                               │
│  • I/O DENTRO de la función medida                               │
│    Si mides un parser que lee de disco, el I/O es parte          │
│    del costo real. Pero para aislar el parsing, lee todo         │
│    a memoria primero y mide solo el parsing.                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 19. Entorno de ejecución: preparar la máquina

### Checklist para mediciones reproducibles

```bash
# 1. Cerrar aplicaciones no esenciales
# (navegador, IDE, servicios innecesarios)

# 2. Desactivar turbo boost (para estabilidad):
# Intel:
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
# AMD:
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost

# 3. Fijar CPU frequency (desactivar frequency scaling):
# Verificar governor actual:
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Fijar a máxima frecuencia:
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 4. Desactivar hyperthreading (opcional, para menos interferencia):
echo 0 | sudo tee /sys/devices/system/cpu/cpu{1,3,5,7}/online
# (desactiva los hyperthreads; ajustar según tu CPU)

# 5. Desactivar ASLR (opcional, para reproducibilidad):
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
# ADVERTENCIA: desactiva una medida de seguridad.
# Reactivar después: echo 2 | sudo tee /proc/sys/...

# 6. Ejecutar con prioridad alta:
sudo nice -n -20 ./benchmark

# 7. Fijar a un core específico (CPU pinning):
taskset -c 0 ./benchmark
```

---

## 20. CPU frequency scaling y turbo boost

```
┌──────────────────────────────────────────────────────────────────┐
│         CPU frequency scaling y su efecto en benchmarks          │
│                                                                  │
│  Sin control de frecuencia:                                      │
│                                                                  │
│  ┌──────────────────────────────────────────────────────┐        │
│  │ GHz                                                  │        │
│  │ 4.5 ─          ┌─────┐     ┌──┐                      │        │
│  │ 4.0 ─    ┌─────┘     └─┐   │  └─┐                    │        │
│  │ 3.5 ─  ┌─┘             └───┘    └──┐                 │        │
│  │ 3.0 ─ ─┘                           └─────            │        │
│  │ 2.0 ─                                                │        │
│  │ 1.2 ─ ×  (inicio: CPU en idle)                       │        │
│  │      └──────────────────────────────────── tiempo     │        │
│  └──────────────────────────────────────────────────────┘        │
│                                                                  │
│  Problema: las primeras mediciones se ejecutan a 1.2 GHz,        │
│  las del medio a 4.5 GHz (turbo), las últimas a 3.5 GHz         │
│  (thermal throttling). No son comparables.                       │
│                                                                  │
│  Con frecuencia fija:                                            │
│                                                                  │
│  ┌──────────────────────────────────────────────────────┐        │
│  │ GHz                                                  │        │
│  │ 3.5 ─ ─────────────────────────────────────          │        │
│  │       └──────────────────────────────────── tiempo    │        │
│  └──────────────────────────────────────────────────────┘        │
│                                                                  │
│  Estable: todas las mediciones a la misma frecuencia.            │
│  El valor absoluto no importa; la estabilidad sí.                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 21. Afinidad de CPU (CPU pinning)

```bash
# Fijar un proceso a un core específico:
taskset -c 2 ./benchmark
# Ejecuta solo en CPU core 2

# En código C:
#define _GNU_SOURCE
#include <sched.h>

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

// Al inicio del benchmark:
pin_to_core(2);  // Fijar a core 2
```

```
┌──────────────────────────────────────────────────────────────────┐
│         ¿Por qué CPU pinning mejora benchmarks?                  │
│                                                                  │
│  Sin pinning:                                                    │
│  Core 0: [bench] → migrado a Core 2 → migrado a Core 1         │
│  Cada migración:                                                 │
│  • Invalida L1/L2 cache (cold start de nuevo)                    │
│  • ~10-50 μs de overhead de migración                            │
│  • Puede caer en nodo NUMA diferente (~100 ns extra por acceso) │
│                                                                  │
│  Con pinning:                                                    │
│  Core 2: [bench────────────────────────────────────────]         │
│  Sin migraciones: cache estable, sin overhead                    │
│                                                                  │
│  Bonus: elegir un core que NO comparta hyperthread con            │
│  otro proceso activo.                                            │
└──────────────────────────────────────────────────────────────────┘
```

---

## 22. Caché: el elefante en la habitación

```
┌──────────────────────────────────────────────────────────────────┐
│         Cache effects en microbenchmarks                         │
│                                                                  │
│  La pregunta más importante:                                     │
│  ¿Tu benchmark mide rendimiento con cache CALIENTE o FRÍO?      │
│                                                                  │
│  Cache caliente (datos en L1/L2):                                │
│  • Array de 4 KB iterado en loop → siempre en L1               │
│  • Representa: tight loops sobre datos pequeños                  │
│  • Tiempo: dominado por cómputo, no por memoria                  │
│                                                                  │
│  Cache frío (datos en L3 o DRAM):                                │
│  • Array de 100 MB recorrido una vez → casi todo en DRAM        │
│  • Representa: primera pasada sobre datos nuevos                 │
│  • Tiempo: dominado por latencia de memoria                      │
│                                                                  │
│  AMBOS son mediciones válidas, pero miden COSAS DIFERENTES.      │
│                                                                  │
│  Tamaños de cache típicos (por core):                            │
│  L1d:  32-48 KB   (~4 ns latencia)                               │
│  L2:   256-512 KB (~10-15 ns latencia)                           │
│  L3:   8-32 MB    (~30-50 ns latencia, compartido)               │
│  DRAM: GBs        (~60-100 ns latencia)                          │
│                                                                  │
│  Si tu benchmark itera sobre un array de 1 KB,                   │
│  está midiendo rendimiento con datos en L1.                      │
│  Si en producción la función recibe datos de 10 MB,              │
│  el benchmark NO refleja el rendimiento real.                    │
│                                                                  │
│  SOLUCIÓN: benchmarkear con MÚLTIPLES tamaños de input:          │
│  bench("1KB",  func, data_1kb,  ...);                            │
│  bench("32KB", func, data_32kb, ...);  // L1                    │
│  bench("256KB",func, data_256kb,...);  // L2                    │
│  bench("8MB",  func, data_8mb,  ...);  // L3                    │
│  bench("100MB",func, data_100mb,...); // DRAM                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 23. Benchmark comparativo: A vs B

### Protocolo para comparar dos implementaciones

```
┌──────────────────────────────────────────────────────────────────┐
│         Comparar A vs B correctamente                            │
│                                                                  │
│  1. MISMO INPUT: A y B deben recibir exactamente los mismos      │
│     datos de entrada.                                            │
│                                                                  │
│  2. MISMO ENTORNO: ejecutar en la misma máquina, misma sesión,  │
│     misma CPU frequency, mismo estado de cache.                  │
│                                                                  │
│  3. INTERCALAR: no medir todo A y luego todo B.                  │
│     Mejor: ABABABAB... para que efectos temporales               │
│     (thermal throttling) afecten a ambos por igual.              │
│                                                                  │
│  4. ESTADÍSTICA: comparar intervalos de confianza, no valores    │
│     puntuales. "A = 102 ± 1 ns, B = 105 ± 1 ns" dice más       │
│     que "A = 102, B = 105".                                     │
│                                                                  │
│  5. VERIFICAR CORRECTITUD: ambas implementaciones deben dar      │
│     el MISMO resultado. Si B da resultados diferentes,            │
│     no estás comparando lo mismo.                                │
│                                                                  │
│  6. REPORTAR RATIO: "B es 1.25x más rápido que A"               │
│     mejor que "A tarda 102 ns, B tarda 82 ns".                   │
│     El ratio es más portátil entre máquinas.                     │
│                                                                  │
│  7. MÚLTIPLES INPUTS: no solo con un tamaño.                     │
│     A puede ganar con inputs pequeños y B con grandes,           │
│     o viceversa.                                                 │
└──────────────────────────────────────────────────────────────────┘
```

### Ejemplo de comparación

```c
// Comparar qsort vs merge sort en C:
void compare_sorts(void) {
    int sizes[] = {100, 1000, 10000, 100000};

    printf("%-10s  %-15s  %-15s  %-10s\n",
           "Size", "qsort (ns)", "merge (ns)", "Ratio");
    printf("%-10s  %-15s  %-15s  %-10s\n",
           "----", "----------", "----------", "-----");

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];
        int *data_a = generate_random(n);
        int *data_b = copy_array(data_a, n);

        BenchResult ra = bench_sort(qsort_wrapper, data_a, n, ...);
        BenchResult rb = bench_sort(merge_sort, data_b, n, ...);

        printf("%-10d  %10.1f ± %.1f  %10.1f ± %.1f  %8.2fx\n",
               n, ra.median, ra.stddev, rb.median, rb.stddev,
               ra.median / rb.median);

        free(data_a);
        free(data_b);
    }
}
```

---

## 24. Reportar resultados correctamente

### Formato de reporte

```
┌──────────────────────────────────────────────────────────────────┐
│              Cómo reportar benchmarks                             │
│                                                                  │
│  INCLUIR siempre:                                                │
│  • Valor central (mediana o media)                               │
│  • Dispersión (± σ o IC95)                                       │
│  • Número de samples y iteraciones                               │
│  • Unidad (ns, μs, ms, ops/sec)                                  │
│  • Descripción del input (tamaño, tipo)                          │
│  • Entorno (CPU, OS, compilador, flags de optimización)          │
│                                                                  │
│  BUENO:                                                          │
│  "binary_search (n=1024, sorted i32): 37.9 ± 1.1 ns             │
│   (50 samples × 100K iters, CV=2.9%)                             │
│   Intel i7-12700K, Linux 6.1, clang 17 -O2"                     │
│                                                                  │
│  MALO:                                                           │
│  "binary search tarda 38 ns" (sin contexto)                     │
│  "mi función es 5x más rápida" (5x más rápida que qué?)        │
│  "mejora del 15%" (¿con qué confianza? ¿cuál es el baseline?)  │
│                                                                  │
│  PARA COMPARACIONES:                                             │
│  "Implementación A: 102.3 ± 0.24 ns (mediana, IC95)             │
│   Implementación B:  82.1 ± 0.31 ns (mediana, IC95)             │
│   B es 1.25x más rápido (IC no se solapan, p < 0.01)"           │
└──────────────────────────────────────────────────────────────────┘
```

### Unidades

```
┌──────────────────────────────────────────────────┐
│  Unidad      │ Cuándo usar                       │
│  ────────────┼───────────────────────────────     │
│  ns          │ Funciones < 1 μs                  │
│  μs          │ Funciones 1-1000 μs               │
│  ms          │ Funciones 1-1000 ms               │
│  s           │ Operaciones > 1 s                  │
│  ops/sec     │ Throughput (cuántas por segundo)   │
│  MB/s o GB/s │ Throughput de datos                │
│  ns/byte     │ Costo por byte procesado           │
│  cycles      │ Para comparaciones hardware-level  │
└──────────────────────────────────────────────────┘
```

---

## 25. Errores clásicos de benchmarking

```
┌──────────────────────────────────────────────────────────────────────┐
│              Errores clásicos de benchmarking                        │
│                                                                      │
│  1. DEAD CODE ELIMINATION                                            │
│     El compilador elimina la función porque el resultado no se usa  │
│     Síntoma: "0.1 ns/iter" (imposible)                              │
│     Fix: black_box / volatile / escape                               │
│                                                                      │
│  2. CONSTANT FOLDING                                                 │
│     El compilador pre-calcula el resultado en compilación           │
│     Síntoma: todas las iteraciones "tardan" lo mismo (casi 0)       │
│     Fix: black_box en los INPUTS, no solo en el output              │
│                                                                      │
│  3. MEDIR SETUP EN VEZ DE FUNCIÓN                                   │
│     Incluir allocación/inicialización en el timing                  │
│     Síntoma: tiempos dominados por malloc, no por la función        │
│     Fix: setup fuera del loop de medición                            │
│                                                                      │
│  4. UNA SOLA MEDICIÓN                                                │
│     Ejecutar una vez y confiar en ese número                        │
│     Síntoma: números irreproducibles                                │
│     Fix: múltiples samples + estadística                             │
│                                                                      │
│  5. SIN WARMUP                                                       │
│     Las primeras iteraciones con cache frío distorsionan            │
│     Síntoma: media mucho mayor que mediana                          │
│     Fix: warmup de 10-100 iteraciones                               │
│                                                                      │
│  6. BENCHMARK DEMASIADO PEQUEÑO                                      │
│     Input que cabe en L1 cuando en producción es mucho mayor        │
│     Síntoma: "mi sort tarda 5 ns/elem" (solo con 100 elementos)    │
│     Fix: benchmarkear con múltiples tamaños de input                │
│                                                                      │
│  7. MEDIR A Y LUEGO B (sin intercalar)                               │
│     Efectos temporales (thermal throttling) afectan B pero no A     │
│     Síntoma: B siempre parece más lento                             │
│     Fix: intercalar ABABABAB, o ejecutar en orden aleatorio         │
│                                                                      │
│  8. COMPILAR CON -O0                                                 │
│     Benchmark sin optimizaciones mide el overhead del compilador    │
│     Síntoma: diferencias de rendimiento no reales                   │
│     Fix: compilar con -O2 o -O3 (lo que usarías en producción)     │
│                                                                      │
│  9. IGNORAR LA VARIANZA                                              │
│     "A = 100ns, B = 102ns, A gana" (pero σ = 5ns)                  │
│     Síntoma: conclusiones que no se sostienen al re-ejecutar        │
│     Fix: calcular intervalos de confianza antes de concluir         │
│                                                                      │
│  10. COMPARAR MÁQUINAS DIFERENTES                                    │
│      "En mi laptop A es más rápido" → no aplica en el servidor     │
│      Síntoma: resultados no reproducibles en otro hardware          │
│      Fix: comparar siempre en la misma máquina, reportar hardware  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 26. Checklist de benchmarking

```
┌──────────────────────────────────────────────────────────────────┐
│              Checklist antes de publicar un benchmark             │
│                                                                  │
│  Preparación:                                                    │
│  □ ¿Cerré aplicaciones innecesarias?                             │
│  □ ¿Fijé CPU frequency (governor = performance)?                 │
│  □ ¿Desactivé turbo boost?                                       │
│  □ ¿Fijé el proceso a un core (taskset)?                         │
│                                                                  │
│  Correctitud:                                                    │
│  □ ¿La función produce resultados CORRECTOS?                     │
│  □ ¿El compilador NO eliminó mi código? (¿tiempos razonables?)  │
│  □ ¿Usé black_box/volatile/escape en inputs Y outputs?           │
│  □ ¿Compilé con el nivel de optimización correcto (-O2)?         │
│                                                                  │
│  Medición:                                                       │
│  □ ¿Hice warmup suficiente?                                      │
│  □ ¿Tengo al menos 10 samples (idealmente 50)?                   │
│  □ ¿Cada sample tiene suficientes iteraciones?                   │
│  □ ¿El tiempo total de medición es > 1 segundo?                  │
│                                                                  │
│  Estadística:                                                    │
│  □ ¿Calculé media Y mediana?                                     │
│  □ ¿Calculé desviación estándar?                                 │
│  □ ¿El CV es < 5%?                                               │
│  □ ¿Los intervalos de confianza son razonablemente estrechos?    │
│                                                                  │
│  Reporte:                                                        │
│  □ ¿Incluyo valor central ± dispersión?                          │
│  □ ¿Especifico input (tamaño, tipo)?                             │
│  □ ¿Especifico entorno (CPU, OS, compilador, flags)?             │
│  □ ¿Las comparaciones usan intervalos de confianza?              │
│  □ ¿Los ratios son significativos (ICs no se solapan)?           │
└──────────────────────────────────────────────────────────────────┘
```

---

## 27. Programa de práctica: bench_framework

### Descripción

Un framework de benchmarking mínimo pero completo en C y Rust, que implementa todo lo cubierto en este tópico.

### Versión en C

```c
// bench_framework.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════
//  Framework de benchmark completo
// ═══════════════════════════════════════════════════════════════

// Anti-optimización
static inline void do_not_optimize(void *p) {
    asm volatile("" : : "g"(p) : "memory");
}

static inline void clobber_memory(void) {
    asm volatile("" : : : "memory");
}

typedef struct {
    const char *name;
    double mean_ns;
    double median_ns;
    double stddev_ns;
    double cv_pct;
    double min_ns;
    double max_ns;
    double ci95_low;
    double ci95_high;
    int n_samples;
    int iters_per_sample;
} BenchResult;

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static long long ts_diff_ns(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000000000LL
         + (b->tv_nsec - a->tv_nsec);
}

BenchResult bench_run(const char *name,
                      void (*func)(void *), void *arg,
                      int warmup, int n_samples, int iters) {
    BenchResult r;
    r.name = name;
    r.n_samples = n_samples;
    r.iters_per_sample = iters;

    double *samples = (double *)malloc(n_samples * sizeof(double));

    // Warmup
    for (int i = 0; i < warmup; i++) func(arg);

    // Medición
    for (int s = 0; s < n_samples; s++) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < iters; i++) func(arg);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        samples[s] = (double)ts_diff_ns(&t0, &t1) / iters;
    }

    // Media
    double sum = 0;
    for (int i = 0; i < n_samples; i++) sum += samples[i];
    r.mean_ns = sum / n_samples;

    // Stddev
    double sq = 0;
    for (int i = 0; i < n_samples; i++) {
        double d = samples[i] - r.mean_ns;
        sq += d * d;
    }
    r.stddev_ns = sqrt(sq / (n_samples - 1));
    r.cv_pct = (r.mean_ns > 0) ? r.stddev_ns / r.mean_ns * 100.0 : 0;

    // Mediana, min, max
    double *sorted = (double *)malloc(n_samples * sizeof(double));
    memcpy(sorted, samples, n_samples * sizeof(double));
    qsort(sorted, n_samples, sizeof(double), cmp_double);
    r.min_ns = sorted[0];
    r.max_ns = sorted[n_samples - 1];
    r.median_ns = (n_samples % 2 == 0)
        ? (sorted[n_samples/2 - 1] + sorted[n_samples/2]) / 2.0
        : sorted[n_samples / 2];

    // IC 95%
    double sem = r.stddev_ns / sqrt(n_samples);
    r.ci95_low  = r.mean_ns - 1.96 * sem;
    r.ci95_high = r.mean_ns + 1.96 * sem;

    free(sorted);
    free(samples);
    return r;
}

void bench_print_result(BenchResult *r) {
    printf("  %-28s  %8.1f ns  (median: %8.1f, σ: %5.1f, CV: %.1f%%)\n",
           r->name, r->mean_ns, r->median_ns, r->stddev_ns, r->cv_pct);
    printf("  %30s  IC95: [%.1f, %.1f]  range: [%.1f, %.1f]\n",
           "", r->ci95_low, r->ci95_high, r->min_ns, r->max_ns);
}

void bench_compare(BenchResult *a, BenchResult *b) {
    double ratio = a->median_ns / b->median_ns;
    int significant = (a->ci95_low > b->ci95_high) ||
                      (b->ci95_low > a->ci95_high);

    printf("\n  Comparación: %s vs %s\n", a->name, b->name);
    printf("    Ratio: %.2fx ", ratio);
    if (ratio > 1.0) {
        printf("(%s es %.1f%% más rápido)\n",
               b->name, (ratio - 1.0) * 100);
    } else {
        printf("(%s es %.1f%% más rápido)\n",
               a->name, (1.0 / ratio - 1.0) * 100);
    }
    printf("    Diferencia %s\n",
           significant ? "SIGNIFICATIVA (ICs no se solapan)"
                       : "no significativa (ICs se solapan)");
}

// ═══════════════════════════════════════════════════════════════
//  Funciones a benchmarkear
// ═══════════════════════════════════════════════════════════════

#define ARRAY_SIZE 4096
static int bench_data[ARRAY_SIZE];

void bench_sum_loop(void *arg) {
    (void)arg;
    int sum = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        sum += bench_data[i];
    }
    do_not_optimize(&sum);
}

void bench_sum_unrolled(void *arg) {
    (void)arg;
    int s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < ARRAY_SIZE; i += 4) {
        s0 += bench_data[i];
        s1 += bench_data[i + 1];
        s2 += bench_data[i + 2];
        s3 += bench_data[i + 3];
    }
    int sum = s0 + s1 + s2 + s3;
    do_not_optimize(&sum);
}

void bench_memset(void *arg) {
    (void)arg;
    memset(bench_data, 0, sizeof(bench_data));
    clobber_memory();
}

void bench_memcpy(void *arg) {
    static int dest[ARRAY_SIZE];
    (void)arg;
    memcpy(dest, bench_data, sizeof(bench_data));
    clobber_memory();
}

int main() {
    // Inicializar datos
    for (int i = 0; i < ARRAY_SIZE; i++) {
        bench_data[i] = i;
    }

    printf("═══════════════════════════════════════════════════════\n");
    printf("  Benchmark: operaciones sobre array de %d ints (%zu KB)\n",
           ARRAY_SIZE, sizeof(bench_data) / 1024);
    printf("  Config: 100 warmup, 100 samples, 50000 iters/sample\n");
    printf("═══════════════════════════════════════════════════════\n\n");

    BenchResult r1 = bench_run("sum_loop", bench_sum_loop, NULL,
                                100, 100, 50000);
    BenchResult r2 = bench_run("sum_unrolled_4x", bench_sum_unrolled, NULL,
                                100, 100, 50000);
    BenchResult r3 = bench_run("memset_zero", bench_memset, NULL,
                                100, 100, 50000);
    BenchResult r4 = bench_run("memcpy", bench_memcpy, NULL,
                                100, 100, 50000);

    bench_print_result(&r1);
    bench_print_result(&r2);
    bench_print_result(&r3);
    bench_print_result(&r4);

    bench_compare(&r1, &r2);

    printf("\n  Throughput estimado:\n");
    printf("    sum_loop:    %.1f MB/s\n",
           (double)(ARRAY_SIZE * 4) / r1.median_ns * 1000.0);
    printf("    memcpy:      %.1f MB/s\n",
           (double)(ARRAY_SIZE * 4) / r4.median_ns * 1000.0);

    return 0;
}
```

### Compilar y ejecutar

```bash
$ clang -O2 -lm bench_framework.c -o bench_framework
$ taskset -c 0 ./bench_framework

═══════════════════════════════════════════════════════
  Benchmark: operaciones sobre array de 4096 ints (16 KB)
  Config: 100 warmup, 100 samples, 50000 iters/sample
═══════════════════════════════════════════════════════

  sum_loop                       1234.5 ns  (median:  1233.8, σ:  12.3, CV: 1.0%)
                                             IC95: [1232.1, 1236.9]  range: [1215.0, 1280.4]
  sum_unrolled_4x                 312.1 ns  (median:   311.5, σ:   4.2, CV: 1.3%)
                                             IC95: [311.3, 312.9]  range: [305.2, 328.1]
  ...
```

---

## 28. Ejercicios

### Ejercicio 1: medir y verificar

**Objetivo**: Demostrar por qué una sola medición no basta.

**Tareas**:

**a)** Escribe un programa que calcula la suma de un array de 10000 enteros. Mide con `time` 5 veces. ¿Los tiempos son iguales?

**b)** Ahora usa `clock_gettime(CLOCK_MONOTONIC)` para medir solo la función de suma (excluyendo startup y I/O). Ejecuta 100 samples de 100000 iteraciones cada uno.

**c)** Calcula: media, mediana, σ, CV. ¿El CV es < 5%?

**d)** Compara tu media con el resultado de `time`. ¿Por qué difieren? ¿Cuánto del tiempo de `time` era startup del proceso?

---

### Ejercicio 2: detectar dead code elimination

**Objetivo**: Experimentar el efecto de las optimizaciones del compilador.

**Tareas**:

**a)** Escribe una función `int square(int x) { return x * x; }` y un benchmark que la llama 10^8 veces en un loop.

**b)** Compila con `-O0` y mide. Luego con `-O2` y mide. ¿Cuánto más rápido es con `-O2`?

**c)** Si con `-O2` el resultado es suspicazmente rápido (<< 1 ns/iter), verifica: ¿el compilador eliminó tu código? Inspecciona el assembly:
   ```bash
   clang -O2 -S bench.c -o bench.s
   # Buscar si el loop existe en bench.s
   ```

**d)** Aplica `do_not_optimize` (volatile o asm barrier). ¿Cómo cambia el tiempo?

**e)** Aplica `black_box` en Rust. ¿Mismo efecto?

---

### Ejercicio 3: efecto del cache

**Objetivo**: Visualizar el efecto del tamaño del working set en el rendimiento.

**Tareas**:

**a)** Escribe un benchmark que suma un array de N enteros, con N = {1KB, 4KB, 16KB, 64KB, 256KB, 1MB, 4MB, 16MB, 64MB}.

**b)** Para cada tamaño, mide el tiempo por elemento (ns/int) con el framework del programa de práctica.

**c)** Grafica los resultados (ns/int vs tamaño). Deberías ver "escalones" en los límites de L1, L2, y L3.

**d)** ¿A qué tamaño se produce cada salto? ¿Corresponde con los tamaños de cache de tu CPU?
   ```bash
   lscpu | grep -i cache
   # O:
   getconf -a | grep CACHE
   ```

---

### Ejercicio 4: benchmark comparativo C vs Rust

**Objetivo**: Comparar la misma operación implementada en C y Rust.

**Tareas**:

**a)** Implementa una búsqueda binaria en C y en Rust, con arrays de 10000 elementos ordenados.

**b)** Benchmarkea ambas con el framework de este tópico. Usa los mismos parámetros: 100 warmup, 50 samples, 100000 iteraciones.

**c)** Compila C con `clang -O2` y Rust con `cargo build --release`.

**d)** Compara:
   - ¿Los intervalos de confianza se solapan?
   - ¿La diferencia es significativa?
   - ¿Quién gana? ¿Por cuánto?

**e)** Reflexiona: si la diferencia es < 5%, ¿tiene sentido elegir un lenguaje sobre otro por rendimiento para esta operación?

---

## Navegación

- **Anterior**: [C03/S03/T04 - ThreadSanitizer (TSan)](../../../C03_Fuzzing_y_Sanitizers/S03_Sanitizers_Red_de_Seguridad/T04_ThreadSanitizer/README.md)
- **Siguiente**: [T02 - Criterion en Rust](../T02_Criterion_en_Rust/README.md)

---

> **Capítulo 4: Benchmarking y Profiling** — Sección 1: Microbenchmarking — Tópico 1 de 4