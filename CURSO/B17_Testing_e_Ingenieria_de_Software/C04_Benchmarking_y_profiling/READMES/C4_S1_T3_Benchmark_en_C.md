# T03 - Benchmark en C: clock_gettime(CLOCK_MONOTONIC), repeticiones manuales, media y desviación, evitar optimización del compilador

## Índice

1. [Por qué construir tu propio framework en C](#1-por-qué-construir-tu-propio-framework-en-c)
2. [El panorama de herramientas de benchmarking en C](#2-el-panorama-de-herramientas-de-benchmarking-en-c)
3. [Relojes disponibles en Linux: repaso profundo](#3-relojes-disponibles-en-linux-repaso-profundo)
4. [clock_gettime en detalle: interfaz y semántica](#4-clock_gettime-en-detalle-interfaz-y-semántica)
5. [CLOCK_MONOTONIC vs CLOCK_MONOTONIC_RAW](#5-clock_monotonic-vs-clock_monotonic_raw)
6. [Resolución del reloj: clock_getres](#6-resolución-del-reloj-clock_getres)
7. [Overhead de la medición: cuánto cuesta medir](#7-overhead-de-la-medición-cuánto-cuesta-medir)
8. [Aritmética de struct timespec](#8-aritmética-de-struct-timespec)
9. [El problema de medir funciones rápidas](#9-el-problema-de-medir-funciones-rápidas)
10. [Warmup: estabilizar el entorno antes de medir](#10-warmup-estabilizar-el-entorno-antes-de-medir)
11. [Elegir el número de iteraciones](#11-elegir-el-número-de-iteraciones)
12. [Repeticiones (samples) vs iteraciones por sample](#12-repeticiones-samples-vs-iteraciones-por-sample)
13. [Estadística: media, mediana, desviación estándar](#13-estadística-media-mediana-desviación-estándar)
14. [Error estándar e intervalo de confianza al 95%](#14-error-estándar-e-intervalo-de-confianza-al-95)
15. [Coeficiente de variación: cuándo la medición es confiable](#15-coeficiente-de-variación-cuándo-la-medición-es-confiable)
16. [Detección y manejo de outliers](#16-detección-y-manejo-de-outliers)
17. [Dead code elimination: el enemigo del benchmark](#17-dead-code-elimination-el-enemigo-del-benchmark)
18. [do_not_optimize: forzar que el valor exista](#18-do_not_optimize-forzar-que-el-valor-exista)
19. [clobber_memory: invalidar suposiciones del compilador](#19-clobber_memory-invalidar-suposiciones-del-compilador)
20. [volatile: usos y abusos en benchmarks](#20-volatile-usos-y-abusos-en-benchmarks)
21. [Constant folding y cómo prevenirlo](#21-constant-folding-y-cómo-prevenirlo)
22. [Loop-invariant code motion: el otro enemigo](#22-loop-invariant-code-motion-el-otro-enemigo)
23. [Interacción entre -O2/-O3 y benchmarks](#23-interacción-entre--o2-o3-y-benchmarks)
24. [Framework completo: bench.h](#24-framework-completo-benchh)
25. [Usar el framework: ejemplo paso a paso](#25-usar-el-framework-ejemplo-paso-a-paso)
26. [Benchmark comparativo: qsort vs mergesort vs heapsort](#26-benchmark-comparativo-qsort-vs-mergesort-vs-heapsort)
27. [Integrar con herramientas externas: hyperfine y perf stat](#27-integrar-con-herramientas-externas-hyperfine-y-perf-stat)
28. [Makefile para benchmarks](#28-makefile-para-benchmarks)
29. [Diferencias con Criterion (Rust): lo que se pierde y lo que se gana](#29-diferencias-con-criterion-rust-lo-que-se-pierde-y-lo-que-se-gana)
30. [Programa de práctica: bench_strings](#30-programa-de-práctica-bench_strings)
31. [Ejercicios](#31-ejercicios)

---

## 1. Por qué construir tu propio framework en C

En Rust, Criterion resuelve el benchmarking con una sola dependencia. En C, no existe un equivalente estándar. Las opciones son:

```
┌──────────────────────────────────────────────────────────────────────┐
│          Benchmarking en C: ¿qué opciones hay?                       │
│                                                                      │
│  Opción                 Pros                    Contras               │
│  ──────                 ────                    ───────               │
│  Google Benchmark       Maduro, completo        C++ (no C puro)       │
│  (C++)                  Estadísticas buenas     Dependencia pesada    │
│                                                                      │
│  time ./programa        Cero setup              Inútil para           │
│                                                 microbenchmarks       │
│                                                                      │
│  hyperfine              Excelente para           Solo mide el         │
│                         binarios completos       programa entero      │
│                                                                      │
│  perf stat              Contadores HW            No es benchmarking   │
│                         (ciclos, cache misses)   propiamente dicho    │
│                                                                      │
│  Framework propio       Control total            Hay que escribirlo   │
│  con clock_gettime      Cero dependencias        Propenso a errores   │
│                         Portable (POSIX)                              │
│                         Exactamente lo que                            │
│                         necesitas                                     │
│                                                                      │
│  En este tópico construimos un framework propio desde cero,          │
│  entendiendo cada decisión de diseño. En el T01 vimos la teoría,    │
│  aquí la implementamos en C como librería reutilizable.              │
└──────────────────────────────────────────────────────────────────────┘
```

Construir tu propio framework no es reinventar la rueda: es **entender la rueda**. Cada función que escribamos corresponde a un concepto del T01. Y cuando uses Google Benchmark o cualquier otro framework en el futuro, sabrás exactamente qué hacen internamente.

---

## 2. El panorama de herramientas de benchmarking en C

```
┌──────────────────────────────────────────────────────────────────────┐
│          Niveles de medición de rendimiento en C                     │
│                                                                      │
│  Nivel 1: Macro (programa completo)                                  │
│  ──────────────────────────────────                                  │
│  Herramientas: time, hyperfine                                       │
│  Granularidad: milisegundos a segundos                               │
│  Uso: "¿cuánto tarda mi programa en procesar este archivo?"          │
│  Incluye: startup, I/O, cleanup, todo                               │
│                                                                      │
│  Nivel 2: Meso (función o módulo)                                    │
│  ──────────────────────────────────                                  │
│  Herramientas: clock_gettime manual, framework propio                │
│  Granularidad: microsegundos a milisegundos                          │
│  Uso: "¿cuánto tarda parse_json() con este input?"                   │
│  Incluye: la función + sus allocations                               │
│                                                                      │
│  Nivel 3: Micro (operación individual)                               │
│  ──────────────────────────────────                                  │
│  Herramientas: framework propio con anti-DCE, rdtsc                  │
│  Granularidad: nanosegundos                                          │
│  Uso: "¿cuánto cuesta una lookup en la hash table?"                  │
│  Requiere: cuidado extremo con optimizaciones del compilador         │
│                                                                      │
│  Nivel 4: Instrucción (ciclos de CPU)                                │
│  ──────────────────────────────────                                  │
│  Herramientas: perf stat, PAPI, rdtsc inline                         │
│  Granularidad: ciclos individuales                                   │
│  Uso: "¿cuántos ciclos cuesta esta secuencia de instrucciones?"      │
│  Requiere: conocimiento de microarquitectura                         │
│                                                                      │
│  Este tópico cubre niveles 2 y 3.                                    │
│  El nivel 4 se verá en profiling (Sección 2).                        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Relojes disponibles en Linux: repaso profundo

Linux ofrece múltiples relojes a través de `clock_gettime()`. Cada uno tiene propiedades diferentes:

```c
#include <time.h>

// Prototipo:
int clock_gettime(clockid_t clk_id, struct timespec *tp);

// Relojes disponibles:
```

```
┌──────────────────────────────────────────────────────────────────────┐
│              Relojes de Linux para benchmarking                      │
│                                                                      │
│  CLOCK_REALTIME                                                      │
│  ─────────────                                                       │
│  Reloj de pared. Puede saltar hacia adelante o atrás               │
│  (NTP, ajuste manual, DST). NUNCA para benchmarks.                   │
│  Epoch: 1 enero 1970 00:00:00 UTC.                                   │
│  Resolución: típicamente 1ns (pero depende del hardware).            │
│                                                                      │
│  CLOCK_MONOTONIC                                                     │
│  ────────────────                                                    │
│  ★ RECOMENDADO para benchmarks.                                      │
│  Nunca retrocede, nunca salta. Avanza de forma constante             │
│  desde un punto arbitrario (típicamente el boot).                    │
│  Ajustado por NTP gradualmente (slewing, no stepping).               │
│  Resolución: típicamente 1ns.                                        │
│  Implementado vía vDSO (no syscall real → ~20-25ns).                │
│                                                                      │
│  CLOCK_MONOTONIC_RAW                                                 │
│  ───────────────────                                                 │
│  Como MONOTONIC pero sin ajuste NTP.                                 │
│  Directamente del oscilador del hardware.                            │
│  Más preciso para intervalos cortos, pero puede                      │
│  derivar respecto al "tiempo real" a largo plazo.                    │
│  Resolución: típicamente 1ns.                                        │
│  ⚠ NO siempre vía vDSO → puede ser syscall real (~50-100ns).       │
│                                                                      │
│  CLOCK_PROCESS_CPUTIME_ID                                            │
│  ────────────────────────                                            │
│  Tiempo de CPU consumido por el proceso.                             │
│  NO avanza cuando el proceso está dormido o suspendido.              │
│  Útil para medir eficiencia algorítmica pura.                        │
│  ⚠ Problemático en sistemas multi-core (puede migrar de CPU).       │
│  ⚠ Overhead más alto (~200-500ns, siempre es syscall).              │
│                                                                      │
│  CLOCK_THREAD_CPUTIME_ID                                             │
│  ────────────────────────                                            │
│  Como PROCESS_CPUTIME_ID pero para un hilo específico.               │
│  Útil para benchmarks multi-hilo.                                    │
│  ⚠ Mismo overhead alto.                                             │
│                                                                      │
│  CLOCK_MONOTONIC_COARSE                                              │
│  ─────────────────────                                               │
│  Versión rápida de MONOTONIC (~5ns overhead).                        │
│  Resolución: ~1-4ms (jiffies). Inútil para microbenchmarks.         │
│  Útil para timestamps que no necesitan precisión de ns.              │
│                                                                      │
│  Resumen para benchmarking:                                          │
│  ┌───────────────────────┬──────────────┬─────────┬────────────┐     │
│  │ Reloj                  │ Overhead     │ Preciso │ Recomendado│     │
│  ├───────────────────────┼──────────────┼─────────┼────────────┤     │
│  │ CLOCK_MONOTONIC        │ ~20-25ns     │ Sí      │ ★ Sí       │     │
│  │ CLOCK_MONOTONIC_RAW    │ ~50-100ns    │ Más     │ A veces    │     │
│  │ CLOCK_PROCESS_CPUTIME  │ ~200-500ns   │ CPU     │ Específico │     │
│  │ CLOCK_REALTIME         │ ~20-25ns     │ Sí      │ ✗ Nunca    │     │
│  │ CLOCK_MONOTONIC_COARSE │ ~5ns         │ No      │ ✗ Nunca    │     │
│  └───────────────────────┴──────────────┴─────────┴────────────┘     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. clock_gettime en detalle: interfaz y semántica

```c
#include <stdio.h>
#include <time.h>

int main(void) {
    struct timespec ts;
    
    // clock_gettime llena la estructura timespec:
    // struct timespec {
    //     time_t tv_sec;   // segundos (entero)
    //     long   tv_nsec;  // nanosegundos [0, 999999999]
    // };
    
    // Obtener el tiempo actual del reloj monotónico
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        return 1;
    }
    
    printf("Segundos:      %ld\n", (long)ts.tv_sec);
    printf("Nanosegundos:  %ld\n", ts.tv_nsec);
    printf("Total en ns:   %ld\n", (long)ts.tv_sec * 1000000000L + ts.tv_nsec);
    
    return 0;
}
```

### Compilación

```bash
# clock_gettime está en librt (real-time) en sistemas antiguos.
# En Linux moderno con glibc >= 2.17, está en libc directamente.

# Compilación moderna (sin -lrt):
gcc -O2 -o clock_demo clock_demo.c

# Si da error de enlace (sistemas antiguos):
gcc -O2 -o clock_demo clock_demo.c -lrt
```

### Implementación vía vDSO

```
┌──────────────────────────────────────────────────────────────────────┐
│           clock_gettime y el vDSO                                    │
│                                                                      │
│  vDSO (virtual Dynamic Shared Object):                               │
│  Página de memoria compartida entre kernel y userspace.              │
│  Contiene implementaciones de ciertas syscalls que NO                │
│  necesitan entrar al kernel.                                         │
│                                                                      │
│  Flujo SIN vDSO (syscall real):                                      │
│  ┌──────────┐     ┌───────────────┐     ┌──────────┐                │
│  │ Userspace │────→│ Transición    │────→│ Kernel   │                │
│  │ tu código │     │ modo kernel   │     │ lee TSC  │                │
│  │           │←────│ (~100-200ns)  │←────│ retorna  │                │
│  └──────────┘     └───────────────┘     └──────────┘                │
│                                                                      │
│  Flujo CON vDSO (sin cambio de modo):                                │
│  ┌──────────┐     ┌───────────────────────────┐                      │
│  │ Userspace │────→│ vDSO (mapeado en espacio │                      │
│  │ tu código │←────│  de usuario, lee datos    │                      │
│  │           │     │  actualizados por kernel) │                      │
│  └──────────┘     └───────────────────────────┘                      │
│  Costo: ~20-25ns (lectura de memoria, sin context switch)            │
│                                                                      │
│  CLOCK_MONOTONIC:        vía vDSO ✓  (~20-25ns)                     │
│  CLOCK_REALTIME:         vía vDSO ✓  (~20-25ns)                     │
│  CLOCK_MONOTONIC_RAW:    depende del kernel/arch                     │
│  CLOCK_PROCESS_CPUTIME:  siempre syscall real (~200-500ns)           │
│                                                                      │
│  Implicación para benchmarks:                                        │
│  CLOCK_MONOTONIC es la opción más eficiente:                         │
│  el overhead de medición es mínimo y predecible.                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. CLOCK_MONOTONIC vs CLOCK_MONOTONIC_RAW

¿Cuándo usar RAW? Casi nunca. Pero entender la diferencia importa:

```
┌──────────────────────────────────────────────────────────────────────┐
│       CLOCK_MONOTONIC vs CLOCK_MONOTONIC_RAW                         │
│                                                                      │
│  CLOCK_MONOTONIC:                                                    │
│  - Ajustado gradualmente por NTP (slewing)                           │
│  - "Slewing" = acelera o frena el reloj suavemente                  │
│  - Máximo ajuste: ±500 ppm (0.05%)                                  │
│  - En 1 segundo de benchmark: error máximo de 500ns                  │
│  - En la práctica: la diferencia es < 1ns para intervalos < 1s       │
│                                                                      │
│  CLOCK_MONOTONIC_RAW:                                                │
│  - Sin ajuste NTP, directamente del oscilador                        │
│  - Puede derivar respecto al tiempo real                             │
│  - Más "puro" para intervalos cortos                                 │
│  - ⚠ En muchos kernels/archs: NO está en vDSO                      │
│    → overhead 5-10x mayor que MONOTONIC                              │
│  - El mayor overhead puede introducir más error                      │
│    que el que el ajuste NTP causaría                                  │
│                                                                      │
│  Ejemplo numérico:                                                   │
│  ─────────────────                                                   │
│  Función a medir: 100ns                                              │
│  Overhead MONOTONIC: 25ns (vDSO), ajuste NTP: <0.01ns               │
│  Overhead RAW: 150ns (syscall), ajuste NTP: 0ns                     │
│                                                                      │
│  Error total MONOTONIC: 25ns + 0.01ns ≈ 25ns                        │
│  Error total RAW: 150ns + 0ns = 150ns                                │
│                                                                      │
│  MONOTONIC gana por 6x a pesar de la "impureza" del NTP.            │
│                                                                      │
│  Conclusión: usar CLOCK_MONOTONIC siempre, excepto                   │
│  en benchmarks de horas de duración donde el drift NTP               │
│  podría importar (caso extremadamente raro).                         │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 6. Resolución del reloj: clock_getres

Antes de medir, verifica que tu reloj tiene la resolución que necesitas:

```c
#include <stdio.h>
#include <time.h>

int main(void) {
    struct timespec res;
    
    const clockid_t clocks[] = {
        CLOCK_REALTIME,
        CLOCK_MONOTONIC,
        CLOCK_MONOTONIC_RAW,
        CLOCK_PROCESS_CPUTIME_ID,
        CLOCK_THREAD_CPUTIME_ID,
        CLOCK_MONOTONIC_COARSE,
    };
    
    const char *names[] = {
        "CLOCK_REALTIME",
        "CLOCK_MONOTONIC",
        "CLOCK_MONOTONIC_RAW",
        "CLOCK_PROCESS_CPUTIME_ID",
        "CLOCK_THREAD_CPUTIME_ID",
        "CLOCK_MONOTONIC_COARSE",
    };
    
    printf("%-30s %s\n", "Reloj", "Resolución");
    printf("%-30s %s\n", "─────", "──────────");
    
    for (int i = 0; i < 6; i++) {
        if (clock_getres(clocks[i], &res) == 0) {
            if (res.tv_sec > 0) {
                printf("%-30s %ld s\n", names[i], (long)res.tv_sec);
            } else if (res.tv_nsec >= 1000000) {
                printf("%-30s %ld ms\n", names[i], res.tv_nsec / 1000000);
            } else if (res.tv_nsec >= 1000) {
                printf("%-30s %ld µs\n", names[i], res.tv_nsec / 1000);
            } else {
                printf("%-30s %ld ns\n", names[i], res.tv_nsec);
            }
        } else {
            printf("%-30s No disponible\n", names[i]);
        }
    }
    
    return 0;
}
```

### Salida típica en Linux moderno

```
Reloj                          Resolución
─────                          ──────────
CLOCK_REALTIME                 1 ns
CLOCK_MONOTONIC                1 ns
CLOCK_MONOTONIC_RAW            1 ns
CLOCK_PROCESS_CPUTIME_ID       1 ns
CLOCK_THREAD_CPUTIME_ID        1 ns
CLOCK_MONOTONIC_COARSE         1 ms       ← ¡1000000x peor!
```

**Advertencia**: resolución de 1ns **no significa precisión de 1ns**. La resolución es el incremento mínimo del contador, pero el overhead de leer el reloj (~20-25ns) limita la precisión práctica. No puedes medir algo que tarda 5ns con un timer que cuesta 25ns de overhead.

```
Resolución vs Precisión vs Overhead:

  Resolución: 1ns   ← El reloj puede representar diferencias de 1ns
  Overhead:   25ns  ← Pero leer el reloj cuesta 25ns
  Precisión:  ~25ns ← No puedes medir intervalos menores que esto

  Para medir algo de 5ns, necesitas:
  - Ejecutar 10,000 iteraciones
  - Medir el tiempo total (50,000ns + 25ns overhead = 50,025ns)
  - Dividir: 50,025 / 10,000 = ~5.0ns por iteración
  - El overhead se amortiza: 25ns / 10,000 = 0.0025ns por iteración
```

---

## 7. Overhead de la medición: cuánto cuesta medir

Medir el overhead de `clock_gettime` es un meta-benchmark esencial:

```c
#include <stdio.h>
#include <time.h>
#include <float.h>

#define SAMPLES 1000
#define ITERS   10000

int main(void) {
    struct timespec start, end;
    double times[SAMPLES];
    
    // Warmup
    for (int i = 0; i < 100; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        clock_gettime(CLOCK_MONOTONIC, &end);
    }
    
    // Medir el costo de clock_gettime
    for (int s = 0; s < SAMPLES; s++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERS; i++) {
            struct timespec dummy;
            clock_gettime(CLOCK_MONOTONIC, &dummy);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L
                        + (end.tv_nsec - start.tv_nsec);
        times[s] = (double)elapsed_ns / ITERS;
    }
    
    // Estadísticas
    double min_val = DBL_MAX, max_val = 0, sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        if (times[i] < min_val) min_val = times[i];
        if (times[i] > max_val) max_val = times[i];
        sum += times[i];
    }
    double mean = sum / SAMPLES;
    
    double var_sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        double diff = times[i] - mean;
        var_sum += diff * diff;
    }
    double stddev = __builtin_sqrt(var_sum / (SAMPLES - 1));
    
    printf("Overhead de clock_gettime(CLOCK_MONOTONIC):\n");
    printf("  Media:   %.1f ns\n", mean);
    printf("  Mínimo:  %.1f ns\n", min_val);
    printf("  Máximo:  %.1f ns\n", max_val);
    printf("  Stddev:  %.1f ns\n", stddev);
    printf("  CV:      %.2f%%\n", (stddev / mean) * 100.0);
    
    // Comparar con otros relojes
    printf("\nComparación de overhead entre relojes:\n");
    
    clockid_t clocks[] = {
        CLOCK_MONOTONIC,
        CLOCK_MONOTONIC_RAW,
        CLOCK_PROCESS_CPUTIME_ID,
        CLOCK_REALTIME,
        CLOCK_MONOTONIC_COARSE,
    };
    const char *names[] = {
        "MONOTONIC",
        "MONOTONIC_RAW",
        "PROCESS_CPUTIME_ID",
        "REALTIME",
        "MONOTONIC_COARSE",
    };
    
    for (int c = 0; c < 5; c++) {
        // Warmup
        for (int i = 0; i < 100; i++) {
            struct timespec dummy;
            clock_gettime(clocks[c], &dummy);
        }
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int i = 0; i < ITERS; i++) {
            struct timespec dummy;
            clock_gettime(clocks[c], &dummy);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L
                        + (end.tv_nsec - start.tv_nsec);
        printf("  %-22s %.1f ns/call\n", names[c], (double)elapsed_ns / ITERS);
    }
    
    return 0;
}
```

### Resultados típicos

```
Overhead de clock_gettime(CLOCK_MONOTONIC):
  Media:   22.3 ns
  Mínimo:  20.1 ns
  Máximo:  45.7 ns
  Stddev:  3.2 ns
  CV:      14.35%

Comparación de overhead entre relojes:
  MONOTONIC              22.3 ns/call    ← vDSO
  MONOTONIC_RAW          85.4 ns/call    ← syscall (kernel-dependent)
  PROCESS_CPUTIME_ID     312.7 ns/call   ← siempre syscall
  REALTIME               21.8 ns/call    ← vDSO
  MONOTONIC_COARSE       5.2 ns/call     ← vDSO (sin lectura TSC)
```

```
Implicación para diseño del framework:

Si tu función tarda ~100ns y usas CLOCK_MONOTONIC (25ns overhead):
- 2 llamadas (start + end) = 50ns de overhead
- Mides: 100ns (real) + 50ns (overhead) = 150ns
- Error: 50%

Solución: ejecutar N iteraciones en un loop:
- 10,000 iteraciones = 1,000,000ns de trabajo
- Overhead: 50ns (solo al inicio y al final)
- Error: 0.005%

Regla: overhead_total / tiempo_total_medido < 0.1%
→ tiempo_total_medido > 1000 × overhead
→ Para MONOTONIC: medir al menos ~50µs por sample
```

---

## 8. Aritmética de struct timespec

Restar dos `struct timespec` correctamente es sorprendentemente propenso a errores:

```c
#include <time.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════
// INCORRECTO: no maneja el borrow de nanosegundos
// ═══════════════════════════════════════════════════════════
long WRONG_diff_ns(struct timespec start, struct timespec end) {
    // Si end.tv_nsec < start.tv_nsec, el resultado es NEGATIVO
    // para el componente de nanosegundos
    return (end.tv_sec - start.tv_sec) * 1000000000L
         + (end.tv_nsec - start.tv_nsec);
    // Ejemplo: start = {5, 999999000}, end = {6, 000001000}
    // = (6-5) * 1e9 + (1000 - 999999000)
    // = 1000000000 + (-999998000)
    // = 2000 ns ✓ (funciona por casualidad aritmética con signed long)
    
    // PERO: en arquitecturas donde long es 32 bits,
    // (end.tv_sec - start.tv_sec) * 1000000000L puede desbordarse
}

// ═══════════════════════════════════════════════════════════
// CORRECTO: maneja el borrow explícitamente
// ═══════════════════════════════════════════════════════════
int64_t timespec_diff_ns(struct timespec start, struct timespec end) {
    int64_t sec_diff  = (int64_t)(end.tv_sec - start.tv_sec);
    int64_t nsec_diff = (int64_t)(end.tv_nsec - start.tv_nsec);
    return sec_diff * 1000000000LL + nsec_diff;
}

// ═══════════════════════════════════════════════════════════
// ALTERNATIVA: normalizar primero, restar después
// ═══════════════════════════════════════════════════════════
struct timespec timespec_diff(struct timespec start, struct timespec end) {
    struct timespec result;
    if (end.tv_nsec < start.tv_nsec) {
        // Borrow: tomar 1 segundo y convertir a nanosegundos
        result.tv_sec  = end.tv_sec - start.tv_sec - 1;
        result.tv_nsec = 1000000000L + end.tv_nsec - start.tv_nsec;
    } else {
        result.tv_sec  = end.tv_sec - start.tv_sec;
        result.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════
// RECOMENDADA para benchmarks: todo en int64_t nanosegundos
// ═══════════════════════════════════════════════════════════
static inline int64_t ts_to_ns(struct timespec ts) {
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// Uso:
// clock_gettime(CLOCK_MONOTONIC, &start);
// ... código ...
// clock_gettime(CLOCK_MONOTONIC, &end);
// int64_t elapsed = ts_to_ns(end) - ts_to_ns(start);
```

### Por qué int64_t y no unsigned

```
int64_t puede representar hasta 2^63 - 1 nanosegundos
= 9,223,372,036,854,775,807 ns
= ~292 años

Para benchmarks, esto es más que suficiente.

Usar int64_t (con signo) en lugar de uint64_t porque:
- Si start > end por error, obtienes un número negativo (detectable)
- Con unsigned, obtendrías un número enorme (silencioso y peligroso)
- Las operaciones de resta son naturales con tipos con signo
```

---

## 9. El problema de medir funciones rápidas

```
┌──────────────────────────────────────────────────────────────────────┐
│         El reto de medir funciones de nanosegundos                   │
│                                                                      │
│  Función: hash_lookup() ≈ 15ns                                       │
│  Timer overhead: 2 × clock_gettime() ≈ 50ns                         │
│                                                                      │
│  Medición directa:                                                   │
│  ┌─────────────────────────────────────────────────────────┐         │
│  │ start ← clock_gettime()                                 │ 25ns   │
│  │ resultado ← hash_lookup(key)                            │ 15ns   │
│  │ end ← clock_gettime()                                   │ 25ns   │
│  │                                                         │        │
│  │ elapsed = 65ns                                          │        │
│  │ hash_lookup "tarda" 65ns (pero realmente tarda 15ns)    │        │
│  │ Error: 333%                                              │        │
│  └─────────────────────────────────────────────────────────┘         │
│                                                                      │
│  Medición con loop de 100,000 iteraciones:                           │
│  ┌─────────────────────────────────────────────────────────┐         │
│  │ start ← clock_gettime()                                 │ 25ns   │
│  │ for i in 0..100000:                                      │        │
│  │     resultado ← hash_lookup(key)                        │ 15ns   │
│  │ end ← clock_gettime()                                   │ 25ns   │
│  │                                                         │        │
│  │ elapsed = 1,500,050ns                                    │        │
│  │ per_iter = 1,500,050 / 100,000 = 15.0005ns              │        │
│  │ Error: 0.003%                                            │        │
│  └─────────────────────────────────────────────────────────┘         │
│                                                                      │
│  Regla: iters × función_ns >> 2 × overhead_ns                        │
│  Con overhead de 50ns y función de 15ns:                             │
│  iters >> 50 / 15 ≈ 4                                               │
│  Para error < 0.1%: iters > 50 / (15 × 0.001) ≈ 3,333              │
│  Para error < 0.01%: iters > 50 / (15 × 0.0001) ≈ 33,333           │
└──────────────────────────────────────────────────────────────────────┘
```

```c
// Patrón básico: medir N iteraciones y dividir
#include <stdio.h>
#include <time.h>
#include <stdint.h>

// Función a benchmarkear
int sum_array(const int *data, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    return sum;
}

int main(void) {
    const int N = 10000;
    int data[10000];
    for (int i = 0; i < N; i++) data[i] = i;
    
    const int ITERS = 100000;
    
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERS; i++) {
        volatile int result = sum_array(data, N);
        (void)result; // Suprimir warning
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    int64_t elapsed_ns = (int64_t)(end.tv_sec - start.tv_sec) * 1000000000LL
                       + (int64_t)(end.tv_nsec - start.tv_nsec);
    
    double per_iter = (double)elapsed_ns / ITERS;
    printf("sum_array(%d): %.1f ns/iter\n", N, per_iter);
    
    return 0;
}
```

---

## 10. Warmup: estabilizar el entorno antes de medir

En el T01 explicamos por qué el warmup es necesario. Aquí implementamos la lógica:

```c
#include <stdio.h>
#include <time.h>
#include <stdint.h>

static inline int64_t ts_to_ns(struct timespec ts) {
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// ═══════════════════════════════════════════════════════════
// Warmup basado en tiempo (no en iteraciones fijas)
// ═══════════════════════════════════════════════════════════
// Ejecuta la función durante al menos warmup_ns nanosegundos.
// Retorna el número de iteraciones ejecutadas (útil para
// calibrar el número de iteraciones del benchmark).

typedef void (*bench_fn)(void *ctx);

int64_t warmup(bench_fn fn, void *ctx, int64_t warmup_ns) {
    struct timespec start, now;
    int64_t total_iters = 0;
    int64_t batch = 1;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (;;) {
        // Ejecutar un batch de iteraciones
        for (int64_t i = 0; i < batch; i++) {
            fn(ctx);
        }
        total_iters += batch;
        
        // Verificar si hemos superado el tiempo de warmup
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed = ts_to_ns(now) - ts_to_ns(start);
        
        if (elapsed >= warmup_ns) {
            break;
        }
        
        // Duplicar el batch para converger rápido
        // (no queremos llamar a clock_gettime demasiado)
        batch *= 2;
    }
    
    return total_iters;
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│             Warmup basado en tiempo vs iteraciones                   │
│                                                                      │
│  Iteraciones fijas (warmup = 1000):                                  │
│  - Función rápida (10ns): warmup = 10µs → insuficiente              │
│  - Función lenta (10ms):  warmup = 10s  → excesivo                  │
│  Problema: un número fijo no se adapta al costo de la función.       │
│                                                                      │
│  Tiempo fijo (warmup = 1 segundo):                                   │
│  - Función rápida: ejecuta ~100M iteraciones → OK                   │
│  - Función lenta:  ejecuta ~100 iteraciones → OK                    │
│  Se adapta automáticamente al costo de la función.                   │
│                                                                      │
│  Qué estabiliza el warmup:                                           │
│  ┌──────────────────────────────────────┐                            │
│  │ • CPU frequency scaling (ramp up)   │                             │
│  │ • Instruction cache (code loading)  │                             │
│  │ • Data cache (data loading)         │                             │
│  │ • Branch predictor (pattern learn)  │                             │
│  │ • Memory allocator (pool setup)     │                             │
│  │ • TLB (page table entries)          │                             │
│  │ • JIT (si applies, no en C puro)    │                             │
│  └──────────────────────────────────────┘                            │
│                                                                      │
│  Valores recomendados:                                               │
│  - Desarrollo rápido: 500ms – 1s                                     │
│  - Benchmark serio: 2s – 5s                                          │
│  - Hardware nuevo/desconocido: 5s – 10s                              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 11. Elegir el número de iteraciones

¿Cuántas veces ejecutar la función dentro de cada sample? Depende de cuánto tarda:

```c
// Calibración automática de iteraciones:
// Ejecutar hasta que el tiempo total sea >= target_time_ns
int64_t calibrate_iters(bench_fn fn, void *ctx, int64_t target_time_ns) {
    struct timespec start, end;
    int64_t iters = 1;
    
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int64_t i = 0; i < iters; i++) {
            fn(ctx);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        int64_t elapsed = ts_to_ns(end) - ts_to_ns(start);
        
        if (elapsed >= target_time_ns) {
            return iters;
        }
        
        // Escalar: si tardó T con N iters,
        // necesitamos ~(target/T) × N iters para alcanzar target
        if (elapsed > 0) {
            int64_t factor = target_time_ns / elapsed;
            if (factor < 2) factor = 2;
            iters *= factor;
        } else {
            // Función tan rápida que clock_gettime no detectó nada
            iters *= 10;
        }
    }
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│            Tabla de iteraciones por costo de función                 │
│                                                                      │
│  Tiempo por         Iteraciones por    Tiempo por                    │
│  llamada            sample (aprox)     sample                        │
│  ──────────         ──────────────     ──────────                    │
│  1ns                10,000,000         ~10ms                         │
│  10ns               1,000,000          ~10ms                         │
│  100ns              100,000            ~10ms                         │
│  1µs                10,000             ~10ms                         │
│  10µs               1,000              ~10ms                         │
│  100µs              100                ~10ms                         │
│  1ms                10                 ~10ms                         │
│  10ms               1                  ~10ms                         │
│  100ms              1                  ~100ms                        │
│  1s                 1                  ~1s                            │
│                                                                      │
│  Target: cada sample debería tardar ~10ms (10,000,000ns)             │
│  - Suficiente para amortiguar el overhead del timer                  │
│  - Suficiente para obtener precisión estadística                     │
│  - No tanto que cada sample tarde una eternidad                      │
│                                                                      │
│  Para funciones > 10ms: 1 iteración por sample está bien.            │
│  Más iteraciones no mejorarían la precisión.                         │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 12. Repeticiones (samples) vs iteraciones por sample

Esta es la distinción más confundida del benchmarking:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Samples vs Iteraciones: dos niveles de repetición            │
│                                                                      │
│  Iteraciones (inner loop):                                           │
│  ─────────────────────────                                           │
│  Cuántas veces ejecutas la función DENTRO de una medición.           │
│  Objetivo: amortiguar el overhead del timer.                         │
│  El tiempo se divide entre el número de iteraciones.                 │
│                                                                      │
│  Samples (outer loop):                                               │
│  ─────────────────────                                               │
│  Cuántas mediciones independientes tomas.                            │
│  Objetivo: obtener una distribución estadística.                     │
│  Cada sample es un punto de datos para calcular                      │
│  media, stddev, IC, etc.                                             │
│                                                                      │
│  ┌────────────────────────────────────────────────────────┐          │
│  │ for sample in 0..SAMPLES:              ← loop externo │          │
│  │     start = clock_gettime()                            │          │
│  │     for iter in 0..ITERS:              ← loop interno │          │
│  │         fn()                                           │          │
│  │     end = clock_gettime()                              │          │
│  │     times[sample] = (end - start) / ITERS              │          │
│  └────────────────────────────────────────────────────────┘          │
│                                                                      │
│  Ejemplo concreto:                                                   │
│  50 samples × 100,000 iteraciones = 5,000,000 llamadas              │
│  Cada sample mide el promedio de 100,000 llamadas.                   │
│  La estadística (media, IC) se calcula sobre los 50 samples.         │
│                                                                      │
│  ¿Cuántos samples necesitas?                                         │
│  ──────────────────────────                                          │
│  10:   Estadísticas muy imprecisas (IC ancho)                        │
│  30:   Mínimo razonable (Teorema del Límite Central)                 │
│  50:   Bueno para desarrollo                                        │
│  100:  Bueno para publicación                                        │
│  200+: Cuando necesitas detectar diferencias < 1%                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 13. Estadística: media, mediana, desviación estándar

Implementación completa en C de las estadísticas necesarias:

```c
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct {
    double mean;
    double median;
    double stddev;
    double sem;        // Standard Error of the Mean
    double cv;         // Coeficiente de variación
    double ci95_lower; // Intervalo de confianza 95% inferior
    double ci95_upper; // Intervalo de confianza 95% superior
    double min;
    double max;
    double p25;        // Percentil 25
    double p75;        // Percentil 75
    int    samples;
} BenchStats;

// Comparador para qsort
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

// Calcular percentil (interpolación lineal)
static double percentile(const double *sorted, int n, double p) {
    if (n == 1) return sorted[0];
    
    double rank = p * (n - 1);
    int lower = (int)rank;
    int upper = lower + 1;
    if (upper >= n) upper = n - 1;
    
    double frac = rank - lower;
    return sorted[lower] * (1.0 - frac) + sorted[upper] * frac;
}

BenchStats compute_stats(const double *data, int n) {
    BenchStats stats = {0};
    stats.samples = n;
    
    if (n == 0) return stats;
    
    // Copiar y ordenar para mediana y percentiles
    double *sorted = (double *)malloc(n * sizeof(double));
    memcpy(sorted, data, n * sizeof(double));
    qsort(sorted, n, sizeof(double), cmp_double);
    
    // Min y max
    stats.min = sorted[0];
    stats.max = sorted[n - 1];
    
    // Media
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
    }
    stats.mean = sum / n;
    
    // Mediana
    if (n % 2 == 0) {
        stats.median = (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    } else {
        stats.median = sorted[n/2];
    }
    
    // Percentiles
    stats.p25 = percentile(sorted, n, 0.25);
    stats.p75 = percentile(sorted, n, 0.75);
    
    // Desviación estándar (sample, no population: dividir por n-1)
    if (n > 1) {
        double var_sum = 0;
        for (int i = 0; i < n; i++) {
            double diff = data[i] - stats.mean;
            var_sum += diff * diff;
        }
        double variance = var_sum / (n - 1);
        stats.stddev = sqrt(variance);
        
        // Error estándar de la media
        stats.sem = stats.stddev / sqrt(n);
        
        // Coeficiente de variación
        if (stats.mean > 0) {
            stats.cv = stats.stddev / stats.mean;
        }
        
        // Intervalo de confianza al 95% (z = 1.96 para n grande)
        // Para n < 30, deberíamos usar t-distribution, pero
        // con n >= 30, la aproximación normal es aceptable.
        double z = 1.96;
        stats.ci95_lower = stats.mean - z * stats.sem;
        stats.ci95_upper = stats.mean + z * stats.sem;
    }
    
    free(sorted);
    return stats;
}
```

### Interpretación de cada métrica

```
┌──────────────────────────────────────────────────────────────────────┐
│           Qué te dice cada métrica                                   │
│                                                                      │
│  Media (mean):                                                       │
│  El promedio aritmético. Sensible a outliers.                        │
│  "En promedio, la función tarda 42.3ns"                              │
│                                                                      │
│  Mediana (median):                                                   │
│  El valor del medio al ordenar. Robusta contra outliers.             │
│  "El 50% de las veces tarda ≤ 41.8ns"                               │
│  Si median << mean → hay outliers inflando la media.                 │
│                                                                      │
│  Desviación estándar (stddev):                                       │
│  Cuánto varían las mediciones respecto a la media.                   │
│  "Las mediciones típicamente varían ±2.1ns de la media"              │
│                                                                      │
│  Error estándar (SEM):                                               │
│  Cuánta incertidumbre hay en la ESTIMACIÓN DE LA MEDIA.              │
│  SEM = stddev / √n                                                   │
│  Más samples → menor SEM → media más precisa.                        │
│                                                                      │
│  Coeficiente de variación (CV):                                      │
│  stddev / mean. Variabilidad relativa al valor medido.               │
│  ┌────────────────────────────────────────┐                          │
│  │ CV < 1%    Excelente — medición limpia │                          │
│  │ CV 1-3%    Bueno — aceptable           │                          │
│  │ CV 3-5%    Regular — posible ruido     │                          │
│  │ CV 5-10%   Problemático — investigar   │                          │
│  │ CV > 10%   Inaceptable — fix entorno   │                          │
│  └────────────────────────────────────────┘                          │
│                                                                      │
│  Intervalo de confianza (IC 95%):                                    │
│  "Con 95% de confianza, el verdadero promedio está                   │
│   entre 41.9ns y 42.7ns"                                             │
│  IC estrecho = mucha confianza en el número.                         │
│  IC ancho = poca confianza, necesitas más samples o menos ruido.     │
│                                                                      │
│  P25 / P75 (cuartiles):                                              │
│  "El 25% más rápido terminó en ≤ 41.2ns"                            │
│  "El 75% terminó en ≤ 43.1ns"                                       │
│  IQR = P75 - P25: medida robusta de dispersión.                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 14. Error estándar e intervalo de confianza al 95%

```
┌──────────────────────────────────────────────────────────────────────┐
│           Error estándar e intervalo de confianza                    │
│                                                                      │
│  La media calculada de tus samples NO es el verdadero                │
│  tiempo promedio de la función. Es una ESTIMACIÓN.                   │
│                                                                      │
│  El error estándar (SEM) cuantifica la incertidumbre:                │
│                                                                      │
│       SEM = σ / √n                                                   │
│                                                                      │
│  Donde σ es la desviación estándar y n es el número de samples.      │
│                                                                      │
│  El IC al 95% define el rango donde probablemente                    │
│  está el verdadero valor:                                            │
│                                                                      │
│       IC₉₅ = [x̄ - 1.96 × SEM,  x̄ + 1.96 × SEM]                   │
│                                                                      │
│  Ejemplo:                                                            │
│  x̄ = 42.3ns, σ = 2.1ns, n = 50                                     │
│  SEM = 2.1 / √50 = 0.297ns                                          │
│  IC₉₅ = [42.3 - 0.58, 42.3 + 0.58] = [41.72, 42.88]ns             │
│                                                                      │
│  Efecto de más samples:                                              │
│  n = 50:   SEM = 0.297ns, IC₉₅ = ±0.58ns                           │
│  n = 100:  SEM = 0.210ns, IC₉₅ = ±0.41ns                           │
│  n = 200:  SEM = 0.148ns, IC₉₅ = ±0.29ns                           │
│  n = 500:  SEM = 0.094ns, IC₉₅ = ±0.18ns                           │
│                                                                      │
│  Duplicar samples reduce el SEM por √2 ≈ 1.41x.                     │
│  Para reducir el IC a la mitad, necesitas 4x samples.                │
│                                                                      │
│  ⚠ Para n < 30: usar t-distribution en vez de z=1.96                │
│  ┌──────────────────────────────────────┐                            │
│  │ n = 10: t₀.₉₇₅,₉  = 2.262          │                            │
│  │ n = 15: t₀.₉₇₅,₁₄ = 2.145          │                            │
│  │ n = 20: t₀.₉₇₅,₁₉ = 2.093          │                            │
│  │ n = 30: t₀.₉₇₅,₂₉ = 2.045          │                            │
│  │ n → ∞:  t → z = 1.960              │                            │
│  └──────────────────────────────────────┘                            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 15. Coeficiente de variación: cuándo la medición es confiable

```c
// Verificar la calidad de la medición
void check_measurement_quality(BenchStats *stats) {
    printf("  CV: %.2f%%", stats->cv * 100.0);
    
    if (stats->cv < 0.01) {
        printf(" [EXCELENTE] medición muy estable\n");
    } else if (stats->cv < 0.03) {
        printf(" [BUENO] aceptable para decisiones\n");
    } else if (stats->cv < 0.05) {
        printf(" [REGULAR] considerar más samples o limpiar entorno\n");
    } else if (stats->cv < 0.10) {
        printf(" [PROBLEMÁTICO] resultados poco confiables\n");
        printf("    → Cerrar aplicaciones pesadas\n");
        printf("    → Desactivar turbo boost\n");
        printf("    → Usar CPU pinning (taskset)\n");
    } else {
        printf(" [INACEPTABLE] no usar estos resultados\n");
        printf("    → Revisar si la función tiene comportamiento variable\n");
        printf("    → Verificar que no haya I/O o allocations en el loop\n");
        printf("    → Comprobar que el sistema no está bajo carga\n");
    }
}
```

### ¿Cuándo la variabilidad es inherente?

```
A veces el CV alto NO es un problema del entorno:

┌──────────────────────────────────────────────────────────────────────┐
│  Función                     CV esperado    Razón                    │
│  ───────                     ───────────    ─────                    │
│  fibonacci(20)               < 1%           Determinista, sin I/O   │
│  qsort(datos aleatorios)     2-5%           Depende del input       │
│  malloc(4096)                3-10%          Estado del allocator     │
│  read(fd, buf, 4096)         10-50%         I/O del kernel          │
│  send(sock, data, n, 0)     20-100%         Red, scheduling         │
│                                                                      │
│  Para funciones con CV inherentemente alto:                          │
│  - Reportar la mediana en vez de la media                            │
│  - Usar muchos más samples (200+)                                    │
│  - Reportar percentiles (P50, P95, P99)                              │
│  - No comparar IC — comparar distribuciones completas                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 16. Detección y manejo de outliers

```c
typedef struct {
    int low_severe;
    int low_mild;
    int high_mild;
    int high_severe;
    int total;
    double iqr;
    double lower_fence_mild;
    double upper_fence_mild;
    double lower_fence_severe;
    double upper_fence_severe;
} OutlierInfo;

// Método de Tukey (misma técnica que usa Criterion)
OutlierInfo detect_outliers(const double *sorted_data, int n) {
    OutlierInfo info = {0};
    
    if (n < 4) return info;
    
    // Calcular cuartiles
    double q1 = percentile(sorted_data, n, 0.25);
    double q3 = percentile(sorted_data, n, 0.75);
    info.iqr = q3 - q1;
    
    // Fences (vallas)
    info.lower_fence_mild   = q1 - 1.5 * info.iqr;
    info.upper_fence_mild   = q3 + 1.5 * info.iqr;
    info.lower_fence_severe = q1 - 3.0 * info.iqr;
    info.upper_fence_severe = q3 + 3.0 * info.iqr;
    
    for (int i = 0; i < n; i++) {
        if (sorted_data[i] < info.lower_fence_severe) {
            info.low_severe++;
        } else if (sorted_data[i] < info.lower_fence_mild) {
            info.low_mild++;
        } else if (sorted_data[i] > info.upper_fence_severe) {
            info.high_severe++;
        } else if (sorted_data[i] > info.upper_fence_mild) {
            info.high_mild++;
        }
    }
    
    info.total = info.low_severe + info.low_mild 
               + info.high_mild + info.high_severe;
    
    return info;
}

void print_outliers(OutlierInfo *info, int total_samples) {
    if (info->total == 0) {
        printf("  Outliers: ninguno detectado\n");
        return;
    }
    
    printf("  Outliers: %d de %d (%.1f%%)\n", 
           info->total, total_samples,
           100.0 * info->total / total_samples);
    
    if (info->low_severe > 0)
        printf("    %d low severe\n", info->low_severe);
    if (info->low_mild > 0)
        printf("    %d low mild\n", info->low_mild);
    if (info->high_mild > 0)
        printf("    %d high mild\n", info->high_mild);
    if (info->high_severe > 0)
        printf("    %d high severe\n", info->high_severe);
    
    double pct = 100.0 * info->total / total_samples;
    if (pct > 10.0) {
        printf("  ⚠ Más del 10%% de outliers — entorno ruidoso\n");
    }
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│           Método de Tukey para outliers                              │
│                                                                      │
│  Distribución de tiempos (ordenados):                                │
│                                                                      │
│  low severe   low mild   normal        high mild   high severe       │
│  ◆ ◆          ◇ ◇ ◇     ●●●●●●●●●●    ◇ ◇        ◆ ◆ ◆            │
│  │            │          │          │   │           │                 │
│  │            Q1-1.5×IQR Q1        Q3  Q3+1.5×IQR  Q3+3×IQR        │
│  Q1-3×IQR                                                           │
│                                                                      │
│  IQR = Q3 - Q1 (rango intercuartil)                                 │
│                                                                      │
│  Mild outlier:   fuera de [Q1-1.5×IQR, Q3+1.5×IQR]                 │
│  Severe outlier: fuera de [Q1-3.0×IQR, Q3+3.0×IQR]                 │
│                                                                      │
│  En benchmarks:                                                      │
│  - High outliers (lentos): causados por el OS (scheduling,           │
│    page faults, interrupts). Son normales en pequeñas cantidades.    │
│  - Low outliers (rápidos): raros. Pueden indicar un bug              │
│    en el benchmark (el compilador eliminó algo).                     │
│                                                                      │
│  Estrategias:                                                        │
│  1. Reportar outliers pero NO eliminarlos de la estadística          │
│  2. Usar la mediana (robusta a outliers)                             │
│  3. Usar trimmed mean (eliminar 5% de cada extremo)                 │
│  4. Si > 10% outliers: arreglar el entorno, no los datos            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 17. Dead code elimination: el enemigo del benchmark

En el T01 introdujimos el problema. En el T02 (Criterion) vimos `black_box`. Ahora implementamos las técnicas anti-DCE en C con detalle:

```
┌──────────────────────────────────────────────────────────────────────┐
│        Dead Code Elimination en benchmarks de C                      │
│                                                                      │
│  El compilador con -O2 o -O3 puede:                                 │
│                                                                      │
│  1. Eliminar código cuyo resultado no se usa                         │
│     int x = expensive_computation();                                 │
│     // x nunca se lee → el compilador elimina la llamada             │
│                                                                      │
│  2. Constant folding: precalcular en compilación                     │
│     int result = fibonacci(20);                                      │
│     // fibonacci(20) = 6765, constante conocida                      │
│     // → reemplaza toda la llamada por "6765"                        │
│                                                                      │
│  3. Loop-invariant code motion (LICM)                                │
│     for (int i = 0; i < N; i++) {                                    │
│         result = compute(42);  // no depende de i                    │
│     }                                                                │
│     // → se mueve FUERA del loop, se ejecuta 1 vez                   │
│                                                                      │
│  4. Eliminación de loops sin efecto                                  │
│     for (int i = 0; i < N; i++) {                                    │
│         compute(i);  // resultado no usado                           │
│     }                                                                │
│     // → loop entero eliminado                                       │
│                                                                      │
│  Resultado: tu benchmark reporta ~0ns porque                        │
│  el compilador eliminó todo lo que querías medir.                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 18. do_not_optimize: forzar que el valor exista

La técnica principal anti-DCE en C usa **inline assembly**:

```c
// ═══════════════════════════════════════════════════════════
// do_not_optimize(val): evitar que el compilador elimine val
// ═══════════════════════════════════════════════════════════
// Le dice al compilador: "este valor podría ser leído por
// código que no puedes ver, así que DEBES calcularlo".

// Para GCC y Clang:
#define do_not_optimize(val) \
    __asm__ __volatile__("" : : "r,m"(val) : "memory")

// Desglose del inline assembly:
//
// __asm__: bloque de ensamblador inline
// __volatile__: NO mover, NO eliminar, NO reordenar
//
// "" : instrucción vacía (no genera código real)
//
// : : nada (no hay outputs)
//
// "r,m"(val) : input constraint
//   "r" = el valor debe estar en un registro
//   "m" = el valor debe estar en memoria
//   El compilador elige la más conveniente.
//   EFECTO: el compilador DEBE calcular val y ponerlo
//   en un registro o en memoria.
//
// "memory" : clobber list
//   El compilador asume que TODA la memoria
//   podría haber sido modificada.
//   EFECTO: invalida cualquier suposición sobre
//   valores cacheados en registros.
```

### Ejemplo de uso

```c
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#define do_not_optimize(val) \
    __asm__ __volatile__("" : : "r,m"(val) : "memory")

int fibonacci(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        int t = a + b;
        a = b;
        b = t;
    }
    return b;
}

int main(void) {
    struct timespec start, end;
    const int ITERS = 1000000;
    
    // ❌ SIN do_not_optimize: el compilador puede eliminar todo
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERS; i++) {
        int result = fibonacci(20);
        (void)result; // "uso" que el compilador puede ignorar
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    int64_t t1 = (int64_t)(end.tv_sec - start.tv_sec) * 1000000000LL
               + (int64_t)(end.tv_nsec - start.tv_nsec);
    
    // ✅ CON do_not_optimize: el compilador debe calcular el resultado
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERS; i++) {
        int result = fibonacci(20);
        do_not_optimize(result); // fuerza que result exista
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    int64_t t2 = (int64_t)(end.tv_sec - start.tv_sec) * 1000000000LL
               + (int64_t)(end.tv_nsec - start.tv_nsec);
    
    printf("Sin do_not_optimize: %ld ns total (%.1f ns/iter)\n",
           (long)t1, (double)t1 / ITERS);
    printf("Con do_not_optimize: %ld ns total (%.1f ns/iter)\n",
           (long)t2, (double)t2 / ITERS);
    
    return 0;
}
```

### Resultado típico con -O2

```bash
$ gcc -O2 -o dno_demo dno_demo.c -lm && ./dno_demo
Sin do_not_optimize: 42 ns total (0.0 ns/iter)     ← ¡ELIMINADO!
Con do_not_optimize: 7234000 ns total (7.2 ns/iter) ← Correcto
```

---

## 19. clobber_memory: invalidar suposiciones del compilador

`clobber_memory` es complementario a `do_not_optimize`. Mientras `do_not_optimize` fuerza que un valor exista, `clobber_memory` fuerza que el compilador re-lea la memoria:

```c
// ═══════════════════════════════════════════════════════════
// clobber_memory(): el compilador no puede asumir que la
// memoria no ha cambiado
// ═══════════════════════════════════════════════════════════
#define clobber_memory() \
    __asm__ __volatile__("" : : : "memory")

// Desglose:
// "" : instrucción vacía
// : : sin outputs
// : : sin inputs
// "memory" : clobber — "la memoria podría haber cambiado"
//
// El compilador NO puede:
// - Mantener valores de arrays/structs en registros entre iteraciones
// - Asumir que el contenido de la memoria es el mismo que antes
// - Mover lecturas de memoria fuera del loop
```

### Cuándo necesitas clobber_memory

```c
// ═══════════════════════════════════════════════════════════
// Caso 1: benchmark de escritura a un array
// ═══════════════════════════════════════════════════════════

void fill_array(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = i * 31;
    }
}

// Sin clobber: el compilador puede determinar que arr[]
// nunca se lee después de fill_array, y eliminar las escrituras.

// loop de benchmark:
for (int i = 0; i < ITERS; i++) {
    fill_array(arr, 1000);
    clobber_memory(); // "alguien podría leer arr ahora"
}

// ═══════════════════════════════════════════════════════════
// Caso 2: benchmark de sort (modifica array in-place)
// ═══════════════════════════════════════════════════════════

// Sin clobber: el compilador sabe que sort() no tiene efecto
// observable si nadie lee el array después.

for (int i = 0; i < ITERS; i++) {
    // Restaurar el array al estado desordenado
    memcpy(arr, original, n * sizeof(int));
    clobber_memory(); // forzar que memcpy se materialice
    
    qsort(arr, n, sizeof(int), cmp_int);
    clobber_memory(); // forzar que qsort se materialice
}

// ═══════════════════════════════════════════════════════════
// Caso 3: combinar ambas técnicas
// ═══════════════════════════════════════════════════════════

for (int i = 0; i < ITERS; i++) {
    int result = compute_something(input);
    do_not_optimize(result);  // forzar que el resultado exista
    clobber_memory();          // invalidar suposiciones de memoria
}
```

### do_not_optimize vs clobber_memory: cuándo usar cada uno

```
┌──────────────────────────────────────────────────────────────────────┐
│    do_not_optimize vs clobber_memory                                 │
│                                                                      │
│  do_not_optimize(val):                                               │
│  - Aplica a UN VALOR específico                                      │
│  - "Este valor debe existir en registro o memoria"                   │
│  - Previene: DCE del cómputo que produce val                         │
│  - Previene: constant folding de val                                 │
│  - NO previene: loop-invariant code motion                           │
│                                                                      │
│  clobber_memory():                                                   │
│  - Aplica a TODA la memoria                                          │
│  - "Asume que cualquier cosa en memoria pudo cambiar"                │
│  - Previene: que el compilador cachee valores de memoria             │
│  - Previene: que elimine escrituras a memoria                        │
│  - Previene: loop-invariant code motion (para datos en memoria)      │
│                                                                      │
│  Regla práctica:                                                     │
│  ┌───────────────────────────────────┬──────────────────────────┐    │
│  │ La función retorna un valor      │ do_not_optimize(result)  │    │
│  │ La función modifica memoria       │ clobber_memory()         │    │
│  │ Ambos                            │ Ambos                    │    │
│  │ Función pura con input constante │ do_not_optimize(input)   │    │
│  │                                  │ + do_not_optimize(result) │    │
│  └───────────────────────────────────┴──────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 20. volatile: usos y abusos en benchmarks

`volatile` es otra herramienta anti-DCE, pero más tosca:

```c
// ═══════════════════════════════════════════════════════════
// volatile: le dice al compilador que el valor puede
// cambiar en cualquier momento (hardware, otro hilo, etc.)
// ═══════════════════════════════════════════════════════════

// Uso 1: variable volatile para forzar lectura/escritura
volatile int sink;

for (int i = 0; i < ITERS; i++) {
    int result = fibonacci(20);
    sink = result; // escritura a volatile → no eliminable
}
// Funciona, pero:
// - Genera una escritura REAL a memoria en cada iteración
// - Esa escritura tiene un costo (~1-5ns según cache)
// - Estás midiendo fibonacci + escritura a memoria

// Uso 2: input volatile
volatile int input = 20;

for (int i = 0; i < ITERS; i++) {
    int result = fibonacci(input); // lectura de volatile en cada iter
    sink = result;
}
// Evita constant folding: el compilador no sabe
// que input siempre es 20 (podría cambiar).
// Pero: lectura de volatile tiene costo.
```

### volatile vs asm barriers

```
┌──────────────────────────────────────────────────────────────────────┐
│         volatile vs inline asm barriers                              │
│                                                                      │
│  volatile:                                                           │
│  + Simple de usar                                                    │
│  + Portable (estándar C)                                             │
│  - Genera código REAL (lectura/escritura a memoria)                  │
│  - Overhead medible (~1-5ns por acceso)                              │
│  - Puede alterar el patrón de cache del benchmark                    │
│  - No previene todas las optimizaciones (el compilador               │
│    aún puede reordenar código no-volatile alrededor)                 │
│                                                                      │
│  asm volatile("" : : "r"(val) : "memory"):                          │
│  + Cero código generado (la instrucción asm está vacía)              │
│  + Previene TODAS las optimizaciones relevantes                      │
│  + Sin overhead medible                                              │
│  - No portable (extensión GCC/Clang)                                 │
│  - Sintaxis críptica                                                 │
│  - No funciona en MSVC (tiene _ReadWriteBarrier en su lugar)         │
│                                                                      │
│  Recomendación:                                                      │
│  → Usar asm barriers para GCC/Clang (Linux, macOS)                   │
│  → Usar volatile como fallback si necesitas portabilidad MSVC        │
│  → NUNCA mezclar ambos en el mismo benchmark                        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 21. Constant folding y cómo prevenirlo

El compilador puede calcular resultados en tiempo de compilación:

```c
// ═══════════════════════════════════════════════════════════
// El compilador puede transformar esto:
// ═══════════════════════════════════════════════════════════

int result = fibonacci(20);
// fibonacci(20) es una función pura con input constante.
// Con -O2 y suficiente inlining, GCC/Clang puede calcular
// fibonacci(20) = 6765 en compilación y reemplazar la
// llamada por una constante.

// Verificar con:
// gcc -O2 -S benchmark.c && grep "6765" benchmark.s
// Si aparece "movl $6765, ..." → constant folding ocurrió.

// ═══════════════════════════════════════════════════════════
// Prevención 1: do_not_optimize en el input
// ═══════════════════════════════════════════════════════════
int n = 20;
do_not_optimize(n);  // el compilador no puede asumir que n = 20
int result = fibonacci(n);  // debe computar en runtime
do_not_optimize(result);

// ═══════════════════════════════════════════════════════════
// Prevención 2: volatile input
// ═══════════════════════════════════════════════════════════
volatile int n = 20;
int result = fibonacci(n);  // n podría haber cambiado → compute
volatile int out = result;

// ═══════════════════════════════════════════════════════════
// Prevención 3: función en otro translation unit
// ═══════════════════════════════════════════════════════════
// Si fibonacci() está en fibonacci.c y el benchmark en bench.c,
// y compilas POR SEPARADO (sin LTO):
//   gcc -O2 -c fibonacci.c
//   gcc -O2 -c bench.c
//   gcc -O2 -o bench fibonacci.o bench.o
// El compilador NO puede hacer constant folding a través
// de translation units (sin LTO).
// ⚠ Con LTO (-flto): SÍ puede → no confiar en esta técnica.
```

```
┌──────────────────────────────────────────────────────────────────────┐
│       Demostrando constant folding con godbolt                       │
│                                                                      │
│  Código:                                                             │
│  int fib(int n) {                                                    │
│      int a=0, b=1;                                                   │
│      for(int i=0; i<n; i++) { int t=a+b; a=b; b=t; }               │
│      return b;                                                       │
│  }                                                                   │
│  int main() { return fib(20); }                                      │
│                                                                      │
│  GCC -O2 genera:                                                     │
│  main:                                                               │
│      mov eax, 10946     ← fibonacci(20) = 10946                     │
│      ret                ← ni loop ni llamada a función               │
│                                                                      │
│  ¡El compilador calculó fibonacci(20) en compilación!                │
│  Si tu benchmark hace esto, mides literalmente 0 trabajo.            │
│                                                                      │
│  Con do_not_optimize:                                                │
│  int n = 20;                                                         │
│  do_not_optimize(n);    ← "n podría no ser 20"                      │
│  return fib(n);         ← DEBE computar en runtime                  │
│                                                                      │
│  GCC -O2 genera:                                                     │
│  main:                                                               │
│      mov edi, 20                                                     │
│      ... (loop real de fibonacci)                                    │
│      ret                                                             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 22. Loop-invariant code motion: el otro enemigo

```c
// LICM: el compilador mueve código fuera del loop si
// el resultado no depende de la variable de iteración.

// ═══════════════════════════════════════════════════════════
// Ejemplo de LICM en acción:
// ═══════════════════════════════════════════════════════════

// Tu código:
for (int i = 0; i < ITERS; i++) {
    int result = expensive_pure_function(42);
    // result es el mismo en cada iteración
}

// El compilador transforma a:
int result = expensive_pure_function(42); // una sola vez
// for loop: eliminado (no tiene efecto)

// ═══════════════════════════════════════════════════════════
// Prevención con do_not_optimize + clobber:
// ═══════════════════════════════════════════════════════════

for (int i = 0; i < ITERS; i++) {
    int result = expensive_pure_function(42);
    do_not_optimize(result); // resultado "podría ser usado"
    clobber_memory();        // función "podría depender de memoria"
    // El compilador no puede probar que el resultado
    // sería el mismo en la siguiente iteración
    // (porque clobber_memory dice que la memoria cambió,
    //  y la función podría depender de ella).
}

// ═══════════════════════════════════════════════════════════
// Prevención más robusta: variar el input
// ═══════════════════════════════════════════════════════════

// Si es posible, hacer que el input dependa de i:
for (int i = 0; i < ITERS; i++) {
    int input = 42 + (i & 0); // siempre 42, pero el compilador no lo sabe
    do_not_optimize(input);
    int result = expensive_pure_function(input);
    do_not_optimize(result);
}
// (i & 0) = 0 siempre, pero con do_not_optimize el compilador
// no puede simplificarlo.
```

### LICM en benchmarks de arrays

```c
// Caso sutil: benchmark de búsqueda en array

const int target = 42;

// LICM puede ocurrir:
for (int i = 0; i < ITERS; i++) {
    int found = linear_search(array, N, target);
    (void)found;
}
// Si linear_search es pura y array + target no cambian,
// el compilador puede calcular found UNA VEZ y eliminar el loop.

// Solución:
for (int i = 0; i < ITERS; i++) {
    int found = linear_search(array, N, target);
    do_not_optimize(found);
    clobber_memory(); // "array podría haber cambiado"
}
// Ahora el compilador debe re-ejecutar la búsqueda cada vez
// porque clobber_memory invalida su conocimiento sobre array[].
```

---

## 23. Interacción entre -O2/-O3 y benchmarks

```
┌──────────────────────────────────────────────────────────────────────┐
│     Flags de optimización y su impacto en benchmarks                 │
│                                                                      │
│  -O0 (sin optimización):                                            │
│  - Nunca medir con -O0. No representa el rendimiento real.          │
│  - Todo es más lento: sin inlining, sin register allocation,        │
│    sin SIMD, sin loop unrolling, sin nada.                           │
│  - Medir con -O0 es medir el overhead del compilador, no tu código. │
│                                                                      │
│  -O1:                                                                │
│  - Optimizaciones básicas. Razonable para debug builds.              │
│  - Algo de inlining, algo de constant folding.                       │
│  - Menos agresivo con DCE que -O2.                                   │
│                                                                      │
│  -O2 (recomendado para benchmarks):                                  │
│  - El nivel estándar de producción.                                  │
│  - Inlining agresivo, loop optimizations, vectorización básica.      │
│  - DCE y LICM activos → NECESITAS anti-DCE.                         │
│  - Buen balance: representa el rendimiento real del código.          │
│                                                                      │
│  -O3:                                                                │
│  - Todo lo de -O2 más:                                               │
│  - Vectorización más agresiva (auto-vectorization)                   │
│  - Loop unrolling más agresivo                                       │
│  - Function cloning, tree vectorization                              │
│  - A veces PEOR que -O2 (code size increase → I-cache pressure)     │
│  - Medir con -O2 Y -O3 y comparar.                                  │
│                                                                      │
│  -Os (optimizar por tamaño):                                         │
│  - Similar a -O2 pero minimiza código generado.                      │
│  - Puede ser más rápido que -O2 en código con I-cache pressure.      │
│  - Vale la pena benchmarkear como alternativa.                       │
│                                                                      │
│  -Ofast:                                                             │
│  - -O3 + -ffast-math + otros                                        │
│  - Permite re-asociar operaciones de punto flotante                  │
│  - CAMBIA LA SEMÁNTICA del programa (violación IEEE 754)             │
│  - Solo usar si entiendes las implicaciones.                         │
│                                                                      │
│  Flags adicionales relevantes:                                       │
│  -march=native    Usar instrucciones del CPU actual (AVX2, etc.)     │
│  -flto            Link-time optimization (cross-unit inlining)       │
│  -fno-inline      Prevenir inlining (para medir llamada a función)   │
│  -fno-unroll-loops No desenrollar loops (para medir loop puro)       │
│                                                                      │
│  Regla: SIEMPRE medir con el mismo nivel de optimización             │
│  que usas en producción. Típicamente -O2 o -O2 -march=native.       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 24. Framework completo: bench.h

Aquí está el framework completo como un header-only library:

```c
// bench.h — Framework de microbenchmarking para C
//
// Uso:
//   #define BENCH_IMPLEMENTATION  // solo en UN archivo .c
//   #include "bench.h"

#ifndef BENCH_H
#define BENCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════
// Anti-DCE macros
// ═══════════════════════════════════════════════════════════

// Evitar que el compilador elimine el cómputo de val
#define do_not_optimize(val) \
    __asm__ __volatile__("" : : "r,m"(val) : "memory")

// Invalidar suposiciones sobre el estado de la memoria
#define clobber_memory() \
    __asm__ __volatile__("" : : : "memory")

// ═══════════════════════════════════════════════════════════
// Tipos
// ═══════════════════════════════════════════════════════════

#define BENCH_MAX_NAME 128

typedef struct {
    char    name[BENCH_MAX_NAME];
    double  mean_ns;
    double  median_ns;
    double  stddev_ns;
    double  sem_ns;
    double  cv;
    double  ci95_lower;
    double  ci95_upper;
    double  min_ns;
    double  max_ns;
    double  p25_ns;
    double  p75_ns;
    int     samples;
    int64_t iters_per_sample;
    int     outlier_mild;
    int     outlier_severe;
} BenchResult;

typedef struct {
    BenchResult a;
    BenchResult b;
    double      diff_ns;
    double      diff_pct;
    int         significant; // 1 si IC no se solapan
} BenchComparison;

// Función a benchmarkear: recibe un contexto opaco
typedef void (*BenchFn)(void *ctx);

// ═══════════════════════════════════════════════════════════
// API
// ═══════════════════════════════════════════════════════════

// Ejecutar benchmark de una función
BenchResult bench_run(const char *name, BenchFn fn, void *ctx,
                      int samples, int64_t warmup_ms,
                      int64_t target_time_ms);

// Comparar dos funciones
BenchComparison bench_compare(const char *name_a, BenchFn fn_a, void *ctx_a,
                              const char *name_b, BenchFn fn_b, void *ctx_b,
                              int samples, int64_t warmup_ms,
                              int64_t target_time_ms);

// Imprimir resultado
void bench_print(const BenchResult *result);

// Imprimir comparación
void bench_print_comparison(const BenchComparison *cmp);

// Formatear tiempo con unidades automáticas
void bench_format_time(double ns, char *buf, int buf_size);

#endif // BENCH_H


// ═══════════════════════════════════════════════════════════
// IMPLEMENTACIÓN
// ═══════════════════════════════════════════════════════════

#ifdef BENCH_IMPLEMENTATION

// ─── Utilidades internas ──────────────────────────────────

static inline int64_t bench__ts_to_ns(struct timespec ts) {
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

static int bench__cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

static double bench__percentile(const double *sorted, int n, double p) {
    if (n == 1) return sorted[0];
    double rank = p * (n - 1);
    int lower = (int)rank;
    int upper = lower + 1;
    if (upper >= n) upper = n - 1;
    double frac = rank - lower;
    return sorted[lower] * (1.0 - frac) + sorted[upper] * frac;
}

// ─── Warmup ───────────────────────────────────────────────

static int64_t bench__warmup(BenchFn fn, void *ctx, int64_t warmup_ns) {
    struct timespec start, now;
    int64_t total_iters = 0;
    int64_t batch = 1;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        for (int64_t i = 0; i < batch; i++) {
            fn(ctx);
        }
        total_iters += batch;

        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t elapsed = bench__ts_to_ns(now) - bench__ts_to_ns(start);
        if (elapsed >= warmup_ns) break;
        batch *= 2;
    }
    return total_iters;
}

// ─── Calibración ──────────────────────────────────────────

static int64_t bench__calibrate(BenchFn fn, void *ctx, int64_t target_ns) {
    struct timespec start, end;
    int64_t iters = 1;

    for (int attempt = 0; attempt < 30; attempt++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int64_t i = 0; i < iters; i++) {
            fn(ctx);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        int64_t elapsed = bench__ts_to_ns(end) - bench__ts_to_ns(start);
        if (elapsed >= target_ns) {
            return iters;
        }

        if (elapsed > 0) {
            int64_t factor = target_ns / elapsed;
            if (factor < 2) factor = 2;
            if (factor > 100) factor = 100;
            iters *= factor;
        } else {
            iters *= 10;
        }
    }
    return iters;
}

// ─── Estadísticas ─────────────────────────────────────────

static void bench__compute_stats(BenchResult *r, double *data, int n) {
    r->samples = n;
    if (n == 0) return;

    // Copiar y ordenar
    double *sorted = (double *)malloc(n * sizeof(double));
    memcpy(sorted, data, n * sizeof(double));
    qsort(sorted, n, sizeof(double), bench__cmp_double);

    r->min_ns = sorted[0];
    r->max_ns = sorted[n - 1];

    // Media
    double sum = 0;
    for (int i = 0; i < n; i++) sum += data[i];
    r->mean_ns = sum / n;

    // Mediana
    if (n % 2 == 0) {
        r->median_ns = (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    } else {
        r->median_ns = sorted[n/2];
    }

    // Percentiles
    r->p25_ns = bench__percentile(sorted, n, 0.25);
    r->p75_ns = bench__percentile(sorted, n, 0.75);

    // Desviación estándar
    if (n > 1) {
        double var_sum = 0;
        for (int i = 0; i < n; i++) {
            double diff = data[i] - r->mean_ns;
            var_sum += diff * diff;
        }
        double variance = var_sum / (n - 1);
        r->stddev_ns = sqrt(variance);
        r->sem_ns = r->stddev_ns / sqrt(n);

        if (r->mean_ns > 0) {
            r->cv = r->stddev_ns / r->mean_ns;
        }

        double z = 1.96;
        r->ci95_lower = r->mean_ns - z * r->sem_ns;
        r->ci95_upper = r->mean_ns + z * r->sem_ns;
    }

    // Outliers (Tukey)
    double iqr = r->p75_ns - r->p25_ns;
    double fence_mild   = r->p75_ns + 1.5 * iqr;
    double fence_severe = r->p75_ns + 3.0 * iqr;
    double fence_mild_low   = r->p25_ns - 1.5 * iqr;
    double fence_severe_low = r->p25_ns - 3.0 * iqr;

    r->outlier_mild = 0;
    r->outlier_severe = 0;
    for (int i = 0; i < n; i++) {
        if (sorted[i] > fence_severe || sorted[i] < fence_severe_low) {
            r->outlier_severe++;
        } else if (sorted[i] > fence_mild || sorted[i] < fence_mild_low) {
            r->outlier_mild++;
        }
    }

    free(sorted);
}

// ─── API pública ──────────────────────────────────────────

void bench_format_time(double ns, char *buf, int buf_size) {
    if (ns < 1000.0) {
        snprintf(buf, buf_size, "%.2f ns", ns);
    } else if (ns < 1000000.0) {
        snprintf(buf, buf_size, "%.2f µs", ns / 1000.0);
    } else if (ns < 1000000000.0) {
        snprintf(buf, buf_size, "%.2f ms", ns / 1000000.0);
    } else {
        snprintf(buf, buf_size, "%.2f s", ns / 1000000000.0);
    }
}

BenchResult bench_run(const char *name, BenchFn fn, void *ctx,
                      int samples, int64_t warmup_ms,
                      int64_t target_time_ms)
{
    BenchResult result = {0};
    strncpy(result.name, name, BENCH_MAX_NAME - 1);

    int64_t warmup_ns = warmup_ms * 1000000LL;
    int64_t target_ns = target_time_ms * 1000000LL;

    // Fase 1: Warmup
    bench__warmup(fn, ctx, warmup_ns);

    // Fase 2: Calibrar iteraciones
    // Queremos que cada sample tarde ~target_ns / samples
    int64_t sample_target_ns = target_ns / samples;
    if (sample_target_ns < 1000000) sample_target_ns = 1000000; // mínimo 1ms
    int64_t iters = bench__calibrate(fn, ctx, sample_target_ns);
    result.iters_per_sample = iters;

    // Fase 3: Medir samples
    double *times = (double *)malloc(samples * sizeof(double));
    struct timespec start, end;

    for (int s = 0; s < samples; s++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int64_t i = 0; i < iters; i++) {
            fn(ctx);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        int64_t elapsed = bench__ts_to_ns(end) - bench__ts_to_ns(start);
        times[s] = (double)elapsed / (double)iters;
    }

    // Fase 4: Estadísticas
    bench__compute_stats(&result, times, samples);

    free(times);
    return result;
}

void bench_print(const BenchResult *r) {
    char buf_mean[32], buf_lower[32], buf_upper[32];
    bench_format_time(r->mean_ns, buf_mean, sizeof(buf_mean));
    bench_format_time(r->ci95_lower, buf_lower, sizeof(buf_lower));
    bench_format_time(r->ci95_upper, buf_upper, sizeof(buf_upper));

    printf("%-30s  time: [%s  %s  %s]\n",
           r->name, buf_lower, buf_mean, buf_upper);

    // CV y calidad
    const char *quality;
    if (r->cv < 0.01) quality = "excelente";
    else if (r->cv < 0.03) quality = "bueno";
    else if (r->cv < 0.05) quality = "regular";
    else if (r->cv < 0.10) quality = "problematico";
    else quality = "inaceptable";

    printf("%30s  CV: %.2f%% (%s)\n", "", r->cv * 100.0, quality);

    // Outliers
    int total_outliers = r->outlier_mild + r->outlier_severe;
    if (total_outliers > 0) {
        printf("%30s  Outliers: %d/%d (", "", total_outliers, r->samples);
        if (r->outlier_mild > 0)
            printf("%d mild", r->outlier_mild);
        if (r->outlier_mild > 0 && r->outlier_severe > 0)
            printf(", ");
        if (r->outlier_severe > 0)
            printf("%d severe", r->outlier_severe);
        printf(")\n");
    }
}

BenchComparison bench_compare(const char *name_a, BenchFn fn_a, void *ctx_a,
                              const char *name_b, BenchFn fn_b, void *ctx_b,
                              int samples, int64_t warmup_ms,
                              int64_t target_time_ms)
{
    BenchComparison cmp = {0};

    cmp.a = bench_run(name_a, fn_a, ctx_a, samples, warmup_ms, target_time_ms);
    cmp.b = bench_run(name_b, fn_b, ctx_b, samples, warmup_ms, target_time_ms);

    cmp.diff_ns = cmp.b.mean_ns - cmp.a.mean_ns;
    if (cmp.a.mean_ns > 0) {
        cmp.diff_pct = (cmp.diff_ns / cmp.a.mean_ns) * 100.0;
    }

    // Significancia: IC no se solapan
    // Si el IC de a está completamente separado del IC de b,
    // la diferencia es significativa.
    if (cmp.a.ci95_upper < cmp.b.ci95_lower ||
        cmp.b.ci95_upper < cmp.a.ci95_lower) {
        cmp.significant = 1;
    }

    return cmp;
}

void bench_print_comparison(const BenchComparison *cmp) {
    bench_print(&cmp->a);
    bench_print(&cmp->b);

    printf("\n  Comparacion: %s vs %s\n", cmp->a.name, cmp->b.name);

    char buf[32];
    bench_format_time(fabs(cmp->diff_ns), buf, sizeof(buf));

    if (cmp->diff_ns > 0) {
        printf("  %s es %s mas rapido (%.2f%%)\n",
               cmp->a.name, buf, fabs(cmp->diff_pct));
    } else if (cmp->diff_ns < 0) {
        printf("  %s es %s mas rapido (%.2f%%)\n",
               cmp->b.name, buf, fabs(cmp->diff_pct));
    } else {
        printf("  Sin diferencia medible\n");
    }

    if (cmp->significant) {
        printf("  Diferencia SIGNIFICATIVA (IC no se solapan)\n");
    } else {
        printf("  Diferencia NO significativa (IC se solapan)\n");
    }
}

#endif // BENCH_IMPLEMENTATION
```

---

## 25. Usar el framework: ejemplo paso a paso

```c
// example_basic.c — Ejemplo básico de uso de bench.h

#define BENCH_IMPLEMENTATION
#include "bench.h"

// ═══════════════════════════════════════════════════════════
// Funciones a benchmarkear
// ═══════════════════════════════════════════════════════════

typedef struct {
    int *data;
    int  n;
} ArrayCtx;

// Suma iterativa
void bench_sum_loop(void *ctx) {
    ArrayCtx *ac = (ArrayCtx *)ctx;
    int sum = 0;
    for (int i = 0; i < ac->n; i++) {
        sum += ac->data[i];
    }
    do_not_optimize(sum);
}

// Suma con punteros
void bench_sum_ptr(void *ctx) {
    ArrayCtx *ac = (ArrayCtx *)ctx;
    int sum = 0;
    const int *end = ac->data + ac->n;
    for (const int *p = ac->data; p < end; p++) {
        sum += *p;
    }
    do_not_optimize(sum);
}

// Suma desenrollada manualmente
void bench_sum_unrolled(void *ctx) {
    ArrayCtx *ac = (ArrayCtx *)ctx;
    int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    int i;
    for (i = 0; i + 3 < ac->n; i += 4) {
        sum0 += ac->data[i];
        sum1 += ac->data[i + 1];
        sum2 += ac->data[i + 2];
        sum3 += ac->data[i + 3];
    }
    int sum = sum0 + sum1 + sum2 + sum3;
    for (; i < ac->n; i++) {
        sum += ac->data[i];
    }
    do_not_optimize(sum);
}

int main(void) {
    // ── Preparar datos ────────────────────────────────────
    const int N = 10000;
    int *data = (int *)malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        data[i] = i;
    }

    ArrayCtx ctx = { .data = data, .n = N };

    printf("=== Benchmark: sum_array(%d) ===\n\n", N);

    // ── Benchmark individual ──────────────────────────────
    BenchResult r1 = bench_run("sum_loop", bench_sum_loop, &ctx,
                               50,    // 50 samples
                               1000,  // 1s warmup
                               5000); // 5s medición
    bench_print(&r1);
    printf("\n");

    BenchResult r2 = bench_run("sum_ptr", bench_sum_ptr, &ctx,
                               50, 1000, 5000);
    bench_print(&r2);
    printf("\n");

    BenchResult r3 = bench_run("sum_unrolled", bench_sum_unrolled, &ctx,
                               50, 1000, 5000);
    bench_print(&r3);
    printf("\n");

    // ── Comparación directa ──────────────────────────────
    printf("\n=== Comparacion: loop vs unrolled ===\n\n");
    BenchComparison cmp = bench_compare(
        "sum_loop", bench_sum_loop, &ctx,
        "sum_unrolled", bench_sum_unrolled, &ctx,
        50, 1000, 5000);
    bench_print_comparison(&cmp);

    free(data);
    return 0;
}
```

### Compilación y ejecución

```bash
# Compilar con optimizaciones (OBLIGATORIO para benchmarks)
gcc -O2 -march=native -o example_basic example_basic.c -lm

# Ejecutar con entorno limpio
sudo nice -n -20 taskset -c 0 ./example_basic
# nice -20: máxima prioridad de scheduling
# taskset -c 0: fijar al CPU 0

# Alternativa sin sudo:
taskset -c 0 ./example_basic
```

### Salida esperada

```
=== Benchmark: sum_array(10000) ===

sum_loop                        time: [2.31 µs  2.32 µs  2.34 µs]
                                CV: 0.87% (excelente)

sum_ptr                         time: [2.30 µs  2.31 µs  2.33 µs]
                                CV: 0.92% (excelente)

sum_unrolled                    time: [2.28 µs  2.29 µs  2.31 µs]
                                CV: 0.78% (excelente)
                                Outliers: 2/50 (1 mild, 1 severe)


=== Comparacion: loop vs unrolled ===

sum_loop                        time: [2.31 µs  2.32 µs  2.34 µs]
                                CV: 0.91% (excelente)
sum_unrolled                    time: [2.28 µs  2.29 µs  2.31 µs]
                                CV: 0.82% (excelente)

  Comparacion: sum_loop vs sum_unrolled
  sum_unrolled es 30.00 ns mas rapido (1.29%)
  Diferencia NO significativa (IC se solapan)
```

> **Nota**: con `-O2`, GCC/Clang probablemente auto-vectorizan y desenrollan el loop simple, igualando la versión manual. ¡El compilador ya hace esto por ti!

---

## 26. Benchmark comparativo: qsort vs mergesort vs heapsort

Un benchmark realista que compara algoritmos de sorting de la biblioteca estándar:

```c
// bench_sort.c — Comparar algoritmos de sorting

#define BENCH_IMPLEMENTATION
#include "bench.h"

// ═══════════════════════════════════════════════════════════
// mergesort y heapsort están en BSD/macOS stdlib.
// En Linux (glibc), solo tenemos qsort.
// Implementamos mergesort y heapsort para comparar.
// ═══════════════════════════════════════════════════════════

static int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

// ─── Heap Sort ────────────────────────────────────────────

static void sift_down(int *arr, int start, int end) {
    int root = start;
    while (2 * root + 1 <= end) {
        int child = 2 * root + 1;
        int swap = root;
        if (arr[swap] < arr[child]) swap = child;
        if (child + 1 <= end && arr[swap] < arr[child + 1]) swap = child + 1;
        if (swap == root) return;
        int tmp = arr[root]; arr[root] = arr[swap]; arr[swap] = tmp;
        root = swap;
    }
}

static void heap_sort(int *arr, int n) {
    // Build heap
    for (int start = (n - 2) / 2; start >= 0; start--) {
        sift_down(arr, start, n - 1);
    }
    // Extract
    for (int end = n - 1; end > 0; end--) {
        int tmp = arr[0]; arr[0] = arr[end]; arr[end] = tmp;
        sift_down(arr, 0, end - 1);
    }
}

// ─── Merge Sort ───────────────────────────────────────────

static void merge(int *arr, int *tmp, int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) tmp[k++] = arr[i++];
        else tmp[k++] = arr[j++];
    }
    while (i <= mid) tmp[k++] = arr[i++];
    while (j <= right) tmp[k++] = arr[j++];
    memcpy(arr + left, tmp + left, (right - left + 1) * sizeof(int));
}

static void merge_sort_impl(int *arr, int *tmp, int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    merge_sort_impl(arr, tmp, left, mid);
    merge_sort_impl(arr, tmp, mid + 1, right);
    merge(arr, tmp, left, mid, right);
}

static void merge_sort(int *arr, int n) {
    int *tmp = (int *)malloc(n * sizeof(int));
    merge_sort_impl(arr, tmp, 0, n - 1);
    free(tmp);
}

// ─── Contexto del benchmark ──────────────────────────────

typedef struct {
    int *original;   // datos originales (no se modifican)
    int *working;    // copia de trabajo (se ordena y restaura)
    int  n;
} SortCtx;

void bench_qsort(void *ctx) {
    SortCtx *sc = (SortCtx *)ctx;
    memcpy(sc->working, sc->original, sc->n * sizeof(int));
    clobber_memory();
    qsort(sc->working, sc->n, sizeof(int), cmp_int);
    clobber_memory();
}

void bench_heapsort(void *ctx) {
    SortCtx *sc = (SortCtx *)ctx;
    memcpy(sc->working, sc->original, sc->n * sizeof(int));
    clobber_memory();
    heap_sort(sc->working, sc->n);
    clobber_memory();
}

void bench_mergesort(void *ctx) {
    SortCtx *sc = (SortCtx *)ctx;
    memcpy(sc->working, sc->original, sc->n * sizeof(int));
    clobber_memory();
    merge_sort(sc->working, sc->n);
    clobber_memory();
}

// ─── Generador de datos ──────────────────────────────────

// LCG simple para datos reproducibles (sin dependencia de rand)
static uint32_t bench_lcg_state = 42;
static uint32_t bench_lcg(void) {
    bench_lcg_state = bench_lcg_state * 1664525u + 1013904223u;
    return bench_lcg_state;
}

static void fill_random(int *arr, int n) {
    bench_lcg_state = 42; // misma semilla siempre
    for (int i = 0; i < n; i++) {
        arr[i] = (int)(bench_lcg() % (uint32_t)(n * 10));
    }
}

static void fill_sorted(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
}

static void fill_reversed(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = n - i;
}

static void fill_nearly_sorted(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
    // Intercambiar 5% de elementos
    bench_lcg_state = 42;
    int swaps = n / 20;
    for (int s = 0; s < swaps; s++) {
        int i = bench_lcg() % n;
        int j = bench_lcg() % n;
        int tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
    }
}

int main(void) {
    int sizes[] = {1000, 10000, 100000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    struct {
        const char *name;
        void (*fill)(int *, int);
    } patterns[] = {
        { "random",        fill_random },
        { "sorted",        fill_sorted },
        { "reversed",      fill_reversed },
        { "nearly_sorted", fill_nearly_sorted },
    };
    int num_patterns = sizeof(patterns) / sizeof(patterns[0]);
    
    for (int si = 0; si < num_sizes; si++) {
        int n = sizes[si];
        int *original = (int *)malloc(n * sizeof(int));
        int *working  = (int *)malloc(n * sizeof(int));
        
        SortCtx ctx = { .original = original, .working = working, .n = n };
        
        for (int pi = 0; pi < num_patterns; pi++) {
            patterns[pi].fill(original, n);
            
            printf("\n=== sort n=%d pattern=%s ===\n\n", n, patterns[pi].name);
            
            char name[BENCH_MAX_NAME];
            
            snprintf(name, sizeof(name), "qsort/%s/%d", patterns[pi].name, n);
            BenchResult r1 = bench_run(name, bench_qsort, &ctx, 30, 500, 3000);
            bench_print(&r1);
            
            snprintf(name, sizeof(name), "heapsort/%s/%d", patterns[pi].name, n);
            BenchResult r2 = bench_run(name, bench_heapsort, &ctx, 30, 500, 3000);
            bench_print(&r2);
            
            snprintf(name, sizeof(name), "mergesort/%s/%d", patterns[pi].name, n);
            BenchResult r3 = bench_run(name, bench_mergesort, &ctx, 30, 500, 3000);
            bench_print(&r3);
            
            printf("\n");
        }
        
        free(original);
        free(working);
    }
    
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -march=native -o bench_sort bench_sort.c -lm
taskset -c 0 ./bench_sort
```

### Resultados esperados

```
=== sort n=10000 pattern=random ===

qsort/random/10000              time: [235.45 µs  236.78 µs  238.12 µs]
                                CV: 1.23% (bueno)
heapsort/random/10000           time: [312.34 µs  314.56 µs  316.89 µs]
                                CV: 1.45% (bueno)
mergesort/random/10000          time: [178.23 µs  179.45 µs  180.78 µs]
                                CV: 1.12% (bueno)

=== sort n=10000 pattern=sorted ===

qsort/sorted/10000              time: [45.67 µs  46.12 µs  46.58 µs]
                                CV: 0.89% (excelente)
heapsort/sorted/10000           time: [289.12 µs  291.34 µs  293.67 µs]
                                CV: 1.56% (bueno)
mergesort/sorted/10000          time: [78.45 µs  79.12 µs  79.89 µs]
                                CV: 0.95% (excelente)
```

```
Análisis de los resultados:

┌─────────────────────────────────────────────────────────────┐
│  n=10000         random      sorted     reversed    nearly  │
│  ─────────       ──────      ──────     ────────    ──────  │
│  qsort           237µs       46µs       48µs        52µs   │
│  heapsort        315µs       291µs      295µs       298µs  │
│  mergesort       179µs       79µs       82µs        85µs   │
│                                                             │
│  Observaciones:                                             │
│  • mergesort gana en datos random (mejor cache locality)    │
│  • qsort (glibc) detecta datos ordenados (O(n) adaptativo) │
│  • heapsort es consistente pero siempre más lento           │
│    (peor locality, O(n log n) siempre)                      │
│  • El pattern importa tanto como el algoritmo               │
└─────────────────────────────────────────────────────────────┘
```

---

## 27. Integrar con herramientas externas: hyperfine y perf stat

El framework propio mide funciones. Para medir programas completos, usa herramientas externas:

### hyperfine

```bash
# Instalar hyperfine
# Fedora/RHEL:
sudo dnf install hyperfine
# Ubuntu/Debian:
sudo apt install hyperfine
# Desde cargo:
cargo install hyperfine

# ═══════════════════════════════════════════════════════════
# Uso básico: medir un programa
# ═══════════════════════════════════════════════════════════
hyperfine './bench_sort'

# Con warmup
hyperfine --warmup 3 './bench_sort'

# Comparar dos programas
hyperfine './sort_v1' './sort_v2'

# Con preparación (setup antes de cada run)
hyperfine --prepare 'cp large_file.txt test_copy.txt' \
          'sort test_copy.txt > /dev/null'

# Exportar resultados
hyperfine --export-json results.json './bench_sort'
hyperfine --export-markdown results.md './bench_sort'

# Parámetros de medición
hyperfine --runs 50 \              # número de ejecuciones
          --warmup 5 \             # runs de warmup
          --min-runs 20 \          # mínimo de runs
          --max-runs 100 \         # máximo de runs
          './bench_sort'
```

```
Salida de hyperfine:

Benchmark 1: ./sort_v1
  Time (mean ± σ):      1.234 s ±  0.023 s    [User: 1.198 s, System: 0.032 s]
  Range (min … max):    1.201 s …  1.289 s    10 runs

Benchmark 2: ./sort_v2
  Time (mean ± σ):      0.876 s ±  0.015 s    [User: 0.845 s, System: 0.028 s]
  Range (min … max):    0.852 s …  0.901 s    10 runs

Summary
  ./sort_v2 ran
    1.41 ± 0.03 times faster than ./sort_v1
```

### perf stat

```bash
# perf stat muestra contadores de hardware

# Contadores básicos
perf stat ./bench_sort

# Contadores específicos
perf stat -e cycles,instructions,cache-references,cache-misses,\
branches,branch-misses ./bench_sort

# Repetir 5 veces y promediar
perf stat -r 5 ./bench_sort

# Con formato detallado
perf stat -d ./bench_sort
```

```
Salida de perf stat:

 Performance counter stats for './bench_sort':

          1,234.56 msec task-clock           #    0.998 CPUs utilized
                12      context-switches     #    9.719 /sec
                 0      cpu-migrations       #    0.000 /sec
             1,234      page-faults          #  999.635 /sec
     3,456,789,012      cycles               #    2.800 GHz
     8,765,432,109      instructions         #    2.54  insn per cycle
     1,234,567,890      branches             # 999.967 M/sec
         1,234,567      branch-misses        #    0.10% of all branches
       234,567,890      cache-references     # 190.001 M/sec
         2,345,678      cache-misses         #    1.00% of all cache refs

       1.236789012 seconds time elapsed
       1.198765432 seconds user
       0.032345678 seconds sys
```

```
Métricas clave de perf stat para benchmarking:

┌──────────────────────────────────────────────────────────────────────┐
│  Métrica                Qué indica              Valor ideal          │
│  ───────                ──────────              ───────────          │
│  insn per cycle (IPC)   Eficiencia del pipeline  > 2.0               │
│  branch-misses          Predicciones fallidas     < 1%                │
│  cache-misses           Fallos de caché           < 5% de refs       │
│  context-switches       Interrupciones del OS    Lo más bajo posible │
│  cpu-migrations         Cambios de core          0 (usar taskset)    │
│  page-faults            Fallos de página          Solo al inicio      │
│                                                                      │
│  IPC bajo (< 1.0) → código limitado por memoria (cache misses)      │
│  IPC alto (> 3.0) → código computacional bien optimizado            │
│  branch-misses alto → patrones de branching impredecibles           │
│  cache-misses alto → datos no caben en cache, o acceso disperso     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 28. Makefile para benchmarks

Un Makefile que organiza la compilación y ejecución de benchmarks:

```makefile
# Makefile para benchmarks en C

CC        = gcc
CFLAGS    = -O2 -march=native -Wall -Wextra -std=c11
LDFLAGS   = -lm
BENCH_DIR = benches
BUILD_DIR = build

# Detectar todos los archivos bench_*.c
BENCH_SRCS = $(wildcard $(BENCH_DIR)/bench_*.c)
BENCH_BINS = $(patsubst $(BENCH_DIR)/%.c,$(BUILD_DIR)/%,$(BENCH_SRCS))

# CPU para pinning (cambiar según tu sistema)
BENCH_CPU = 0

.PHONY: all clean bench bench-quick

all: $(BENCH_BINS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%: $(BENCH_DIR)/%.c bench.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Ejecutar todos los benchmarks con entorno limpio
bench: $(BENCH_BINS)
	@echo "=== Ejecutando benchmarks ==="
	@echo "CPU pinning: core $(BENCH_CPU)"
	@echo ""
	@for bin in $(BENCH_BINS); do \
		echo "--- $$(basename $$bin) ---"; \
		taskset -c $(BENCH_CPU) ./$$bin; \
		echo ""; \
	done

# Versión rápida para desarrollo
bench-quick: $(BENCH_BINS)
	@for bin in $(BENCH_BINS); do \
		echo "--- $$(basename $$bin) ---"; \
		./$$bin; \
		echo ""; \
	done

# Verificar ensamblador generado (para detectar DCE)
asm: $(BENCH_SRCS)
	@for src in $(BENCH_SRCS); do \
		echo "=== $$src ==="; \
		$(CC) $(CFLAGS) -S -o - $$src | grep -c "call"; \
		echo "function calls en el assembly"; \
	done

# Ejecutar con perf stat
perf: $(BENCH_BINS)
	@for bin in $(BENCH_BINS); do \
		echo "--- $$(basename $$bin) ---"; \
		perf stat -r 3 taskset -c $(BENCH_CPU) ./$$bin 2>&1 | \
			grep -E "(task-clock|cycles|instructions|cache-misses|branch-misses)"; \
		echo ""; \
	done

clean:
	rm -rf $(BUILD_DIR)
```

### Estructura del proyecto

```
my_benchmarks/
├── Makefile
├── bench.h              ← Framework (header-only)
├── benches/
│   ├── bench_sort.c     ← Benchmark de sorting
│   ├── bench_search.c   ← Benchmark de búsqueda
│   └── bench_hash.c     ← Benchmark de hashing
└── build/               ← Binarios compilados (generado)
    ├── bench_sort
    ├── bench_search
    └── bench_hash
```

```bash
# Compilar todos
make

# Ejecutar todos los benchmarks
make bench

# Ejecutar uno específico
make build/bench_sort && taskset -c 0 ./build/bench_sort

# Ver assembly
make asm

# Ejecutar con perf stat
make perf

# Limpiar
make clean
```

---

## 29. Diferencias con Criterion (Rust): lo que se pierde y lo que se gana

```
┌──────────────────────────────────────────────────────────────────────┐
│       Framework C propio vs Criterion.rs                             │
│                                                                      │
│  Lo que se PIERDE al no tener Criterion:                             │
│  ──────────────────────────────────────                              │
│  • Muestreo lineal (regresión OLS)                                   │
│    Nuestro framework usa media simple de N samples.                  │
│    Criterion estima el slope de la regresión,                        │
│    aislando automáticamente el overhead constante.                   │
│                                                                      │
│  • Bootstrap resampling (100,000 iteraciones)                        │
│    Nuestro IC asume distribución normal.                             │
│    Criterion construye la distribución empírica.                     │
│                                                                      │
│  • HTML reports                                                      │
│    Sin gráficos. Solo texto en consola.                              │
│                                                                      │
│  • Baseline management                                               │
│    No guardamos resultados entre ejecuciones.                        │
│    Cada benchmark es independiente.                                  │
│                                                                      │
│  • Detección automática de regresiones                               │
│    No comparamos contra ejecuciones previas.                         │
│                                                                      │
│  • Calibración adaptativa sofisticada                                │
│    Nuestra calibración es más simple.                                │
│                                                                      │
│  Lo que se GANA:                                                     │
│  ────────────────                                                    │
│  • Cero dependencias                                                 │
│    Un archivo .h, compilas y listo. Sin cargo,                       │
│    sin descargar crates, sin build system.                           │
│                                                                      │
│  • Control total                                                     │
│    Puedes modificar cada aspecto del framework.                      │
│    Añadir métricas custom, integrar con perf,                        │
│    medir allocations, contar syscalls, etc.                          │
│                                                                      │
│  • Portabilidad POSIX                                                │
│    Funciona en cualquier sistema con clock_gettime:                  │
│    Linux, macOS, FreeBSD, etc.                                       │
│                                                                      │
│  • Entendimiento profundo                                            │
│    Sabes exactamente qué hace cada línea.                            │
│    No hay magia ni abstracciones ocultas.                            │
│                                                                      │
│  • Compilación instantánea                                           │
│    gcc -O2 file.c → listo.                                           │
│    Sin esperar a que plotters compile.                                │
│                                                                      │
│  • Embeddable                                                        │
│    Puedes incluir el benchmark directamente en tu                    │
│    programa de producción con #ifdef BENCHMARK.                      │
│                                                                      │
│  Recomendación:                                                      │
│  ─────────────                                                       │
│  • En Rust: usar Criterion SIEMPRE.                                  │
│  • En C: usar el framework propio para microbenchmarks,              │
│    hyperfine para macrobenchmarks, perf stat para                    │
│    análisis de hardware.                                             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 30. Programa de práctica: bench_strings

Benchmark completo de operaciones con strings en C. Compara implementaciones de la biblioteca estándar contra implementaciones manuales.

```c
// bench_strings.c — Benchmark de operaciones con strings
//
// Compara: strlen, strchr, memcmp, memcpy, memset
// estándar vs implementaciones manuales.
// Demuestra el uso completo de bench.h.

#define BENCH_IMPLEMENTATION
#include "bench.h"

#include <string.h>

// ═══════════════════════════════════════════════════════════
// Implementaciones manuales (para comparar con libc)
// ═══════════════════════════════════════════════════════════

// strlen manual: byte a byte
static size_t my_strlen_naive(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

// strlen con truco de word-at-a-time
// Lee 8 bytes a la vez y usa la técnica de null-byte detection
static size_t my_strlen_word(const char *s) {
    // Alinear al boundary de 8 bytes
    const char *p = s;
    while (((uintptr_t)p & 7) != 0) {
        if (*p == '\0') return p - s;
        p++;
    }

    // Leer de a 8 bytes
    const uint64_t *wp = (const uint64_t *)p;
    // Constante mágica: detecta si algún byte es 0
    // Técnica: si un byte es 0, (x - 0x01..01) & ~x & 0x80..80 != 0
    const uint64_t lo = 0x0101010101010101ULL;
    const uint64_t hi = 0x8080808080808080ULL;

    while (1) {
        uint64_t w = *wp;
        if (((w - lo) & ~w & hi) != 0) {
            // Algún byte es 0 — encontrar cuál
            const char *cp = (const char *)wp;
            for (int i = 0; i < 8; i++) {
                if (cp[i] == '\0') return cp + i - s;
            }
        }
        wp++;
    }
}

// strchr manual
static char *my_strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

// memcpy manual (byte a byte)
static void *my_memcpy_naive(void *dst, const void *src, size_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

// memcpy manual (word a word)
static void *my_memcpy_word(void *dst, const void *src, size_t n) {
    uint64_t *d64 = (uint64_t *)dst;
    const uint64_t *s64 = (const uint64_t *)src;
    size_t words = n / 8;
    size_t rest = n % 8;

    for (size_t i = 0; i < words; i++) {
        d64[i] = s64[i];
    }

    char *d = (char *)(d64 + words);
    const char *s = (const char *)(s64 + words);
    for (size_t i = 0; i < rest; i++) {
        d[i] = s[i];
    }

    return dst;
}

// memset manual
static void *my_memset_naive(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        p[i] = val;
    }
    return s;
}

// ═══════════════════════════════════════════════════════════
// Contextos para los benchmarks
// ═══════════════════════════════════════════════════════════

typedef struct {
    char   *str;
    size_t  len;
    char   *dst;        // para memcpy
    char    search_char; // para strchr
} StringCtx;

// ─── strlen benchmarks ───────────────────────────────────

void bench_strlen_libc(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    size_t result = strlen(sc->str);
    do_not_optimize(result);
}

void bench_strlen_naive(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    size_t result = my_strlen_naive(sc->str);
    do_not_optimize(result);
}

void bench_strlen_word(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    size_t result = my_strlen_word(sc->str);
    do_not_optimize(result);
}

// ─── strchr benchmarks ───────────────────────────────────

void bench_strchr_libc(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    char *result = strchr(sc->str, sc->search_char);
    do_not_optimize(result);
}

void bench_strchr_manual(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    char *result = my_strchr(sc->str, sc->search_char);
    do_not_optimize(result);
}

// ─── memcpy benchmarks ──────────────────────────────────

void bench_memcpy_libc(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    memcpy(sc->dst, sc->str, sc->len);
    clobber_memory();
}

void bench_memcpy_naive(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    my_memcpy_naive(sc->dst, sc->str, sc->len);
    clobber_memory();
}

void bench_memcpy_word(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    my_memcpy_word(sc->dst, sc->str, sc->len);
    clobber_memory();
}

// ─── memset benchmarks ──────────────────────────────────

void bench_memset_libc(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    memset(sc->dst, 'A', sc->len);
    clobber_memory();
}

void bench_memset_naive(void *ctx) {
    StringCtx *sc = (StringCtx *)ctx;
    my_memset_naive(sc->dst, 'A', sc->len);
    clobber_memory();
}

// ═══════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════

int main(void) {
    size_t sizes[] = {64, 256, 1024, 4096, 16384, 65536};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int si = 0; si < num_sizes; si++) {
        size_t len = sizes[si];

        // Preparar string: llenar con 'a', terminar con '\0'
        char *str = (char *)malloc(len + 1);
        memset(str, 'a', len);
        str[len] = '\0';

        // Buffer destino para memcpy/memset
        char *dst = (char *)malloc(len + 1);

        // Carácter a buscar: ponerlo al final (peor caso)
        str[len - 1] = 'Z';

        StringCtx ctx = {
            .str = str,
            .len = len,
            .dst = dst,
            .search_char = 'Z',
        };

        printf("\n");
        printf("============================================================\n");
        printf("  String length: %zu bytes\n", len);
        printf("============================================================\n");

        // ── strlen ────────────────────────────────────────
        printf("\n--- strlen ---\n");
        BenchResult r;

        r = bench_run("strlen (libc)", bench_strlen_libc, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("strlen (naive)", bench_strlen_naive, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("strlen (word)", bench_strlen_word, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        // ── strchr ────────────────────────────────────────
        printf("\n--- strchr (buscar al final) ---\n");

        r = bench_run("strchr (libc)", bench_strchr_libc, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("strchr (manual)", bench_strchr_manual, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        // ── memcpy ────────────────────────────────────────
        printf("\n--- memcpy ---\n");

        r = bench_run("memcpy (libc)", bench_memcpy_libc, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("memcpy (naive)", bench_memcpy_naive, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("memcpy (word)", bench_memcpy_word, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        // ── memset ────────────────────────────────────────
        printf("\n--- memset ---\n");

        r = bench_run("memset (libc)", bench_memset_libc, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        r = bench_run("memset (naive)", bench_memset_naive, &ctx,
                       50, 1000, 3000);
        bench_print(&r);

        // ── Comparación directa: libc vs naive memcpy ────
        printf("\n--- Comparacion: memcpy libc vs naive ---\n");
        BenchComparison cmp = bench_compare(
            "memcpy (libc)", bench_memcpy_libc, &ctx,
            "memcpy (naive)", bench_memcpy_naive, &ctx,
            50, 1000, 3000);
        bench_print_comparison(&cmp);

        // Restaurar el string original
        str[len - 1] = 'a';
        free(str);
        free(dst);
    }

    printf("\n");
    printf("============================================================\n");
    printf("  Analisis esperado:\n");
    printf("============================================================\n");
    printf("\n");
    printf("  1. libc strlen/memcpy/memset usan SIMD (SSE4.2, AVX2)\n");
    printf("     y deberían ser 4-16x más rápidas que las naive.\n");
    printf("\n");
    printf("  2. La versión 'word' es intermedia: lee 8 bytes a la vez\n");
    printf("     pero sin instrucciones SIMD.\n");
    printf("\n");
    printf("  3. Para strings cortos (<64 bytes), la diferencia puede\n");
    printf("     ser menor porque el overhead de setup del SIMD domina.\n");
    printf("\n");
    printf("  4. Para strings largos (>4KB), el throughput se estabiliza\n");
    printf("     y la diferencia libc vs naive debería ser máxima.\n");
    printf("\n");
    printf("  5. strchr de libc usa SIMD para buscar en 16/32 bytes\n");
    printf("     a la vez, vs 1 byte a la vez en la versión manual.\n");

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -O2 -march=native -o bench_strings bench_strings.c -lm
taskset -c 0 ./bench_strings
```

### Resultados esperados (aproximados para x86-64 moderno)

```
┌──────────────────────────────────────────────────────────────────────┐
│  Resultados esperados: libc vs naive (ns/operación)                  │
│                                                                      │
│  Tamaño      strlen         memcpy         memset                    │
│              libc / naive   libc / naive   libc / naive              │
│  ──────      ───────────    ───────────    ───────────               │
│  64 B        3 / 18         4 / 22         3 / 20                    │
│  256 B       5 / 68         6 / 85         5 / 80                    │
│  1 KB        12 / 270       14 / 340       12 / 320                  │
│  4 KB        35 / 1080      40 / 1360      35 / 1280                │
│  16 KB       120 / 4320     150 / 5440     130 / 5120               │
│  64 KB       450 / 17280    580 / 21760    500 / 20480              │
│                                                                      │
│  Ratio libc/naive:                                                   │
│  - strlen: ~6x (SIMD pcmpistri vs byte-by-byte)                     │
│  - memcpy: ~6-8x (SIMD rep movsb / AVX2 vs byte)                   │
│  - memset: ~6-8x (SIMD rep stosb / AVX2 vs byte)                   │
│                                                                      │
│  La versión 'word' (8 bytes a la vez): ~2-3x más rápida             │
│  que naive, pero ~2-4x más lenta que libc.                           │
│                                                                      │
│  Throughput para 64KB:                                               │
│  - libc memcpy: ~64KB / 580ns ≈ 105 GB/s (puede saturar L1)        │
│  - naive memcpy: ~64KB / 21760ns ≈ 2.8 GB/s                         │
│  - word memcpy: ~64KB / 8000ns ≈ 7.6 GB/s                           │
│                                                                      │
│  Moraleja: NUNCA reimplementar funciones de libc                     │
│  para producción. Las implementaciones de glibc están                │
│  escritas en ensamblador optimizado para cada microarquitectura.     │
│  Tu implementación en C nunca se acercará.                           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 31. Ejercicios

### Ejercicio 1: medir overhead del timer

**Objetivo**: Verificar el overhead de `clock_gettime` en tu sistema.

**Tareas**:

**a)** Escribe un programa que mida el costo de llamar a `clock_gettime` con cada tipo de reloj disponible (MONOTONIC, MONOTONIC_RAW, PROCESS_CPUTIME_ID, REALTIME, MONOTONIC_COARSE).

**b)** Ejecuta 10,000 llamadas consecutivas y divide el tiempo total. Repite 100 veces para obtener estadísticas.

**c)** Verifica cuál reloj está implementado vía vDSO en tu kernel:
```bash
# Listar funciones en el vDSO
ldd /bin/true | grep vdso
# Extraer el vDSO y ver qué funciones tiene:
# (dentro de un programa C, /proc/self/maps muestra la dirección)
```

**d)** ¿El overhead de MONOTONIC_RAW es mayor que MONOTONIC en tu sistema? ¿Por cuánto?

---

### Ejercicio 2: detectar constant folding

**Objetivo**: Verificar que tus técnicas anti-DCE funcionan.

**Tareas**:

**a)** Escribe un benchmark de `fibonacci(20)` SIN ninguna protección anti-DCE. Compila con `-O2 -S` y examina el assembly. ¿El loop de fibonacci existe en el assembly?

**b)** Añade `do_not_optimize(result)` y repite. ¿Cambió el assembly?

**c)** Añade también `do_not_optimize(n)` para el input. ¿Cambió de nuevo?

**d)** Compara los tiempos medidos en cada caso. Deberías ver:
   - Sin protección: ~0ns (el compilador eliminó todo)
   - Solo output: ~7ns (computa pero con input constante)
   - Input + output: ~7ns (igual, porque fibonacci(20) es tan rápido que el constant folding no cambia mucho)

**e)** Repite con una función más costosa donde el constant folding SÍ impacte: `sum_of_squares(1, 1000000)` que calcule Σ(i²). Sin protección, ¿reporta 0ns?

---

### Ejercicio 3: framework extendido con throughput

**Objetivo**: Extender `bench.h` para reportar throughput.

**Tareas**:

**a)** Añade un campo `throughput_bytes` a `BenchResult`. Si es > 0, `bench_print` debe mostrar el throughput en MB/s o GB/s.

**b)** Crea una función `bench_run_throughput` que acepte un parámetro extra `size_t bytes_per_iter`.

**c)** Usa la extensión para benchmarkear `memcpy` con diferentes tamaños y reportar throughput.

**d)** ¿A qué tamaño el throughput deja de crecer? ¿Corresponde con el tamaño de L1 cache de tu CPU?

---

### Ejercicio 4: benchmark de hash tables caseras

**Objetivo**: Benchmark completo de una estructura de datos.

**Tareas**:

**a)** Implementa una hash table simple (open addressing, linear probing) con las operaciones: `insert`, `lookup`, `delete`.

**b)** Benchmarkea cada operación con el framework `bench.h`:
   - Insert: 10,000 elementos con claves aleatorias
   - Lookup hit: buscar un elemento que existe
   - Lookup miss: buscar un elemento que NO existe
   - Delete: eliminar un elemento existente

**c)** Parametriza por load factor: 0.25, 0.50, 0.75, 0.90. ¿Cómo afecta el load factor al rendimiento de cada operación?

**d)** Compara contra la solución simple de "array lineal + búsqueda lineal" para N = 100, 1000, 10000. ¿A qué N la hash table empieza a ganar?

---

## Navegación

- **Anterior**: [T02 - Criterion en Rust](../T02_Criterion_en_Rust/README.md)
- **Siguiente**: [T04 - Trampas comunes](../T04_Trampas_comunes/README.md)

---

> **Capítulo 4: Benchmarking y Profiling** — Sección 1: Microbenchmarking — Tópico 3 de 4
