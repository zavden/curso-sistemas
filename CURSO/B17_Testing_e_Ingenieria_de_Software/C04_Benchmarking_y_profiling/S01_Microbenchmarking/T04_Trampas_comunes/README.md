# T04 - Trampas comunes: dead code elimination, branch prediction warming, tamaño de input vs cache, benchmark vs realidad

## Índice

1. [Por qué los benchmarks mienten](#1-por-qué-los-benchmarks-mienten)
2. [La jerarquía de trampas del benchmarking](#2-la-jerarquía-de-trampas-del-benchmarking)
3. [Trampa 1: Dead code elimination sin anti-DCE](#3-trampa-1-dead-code-elimination-sin-anti-dce)
4. [Trampa 2: Constant folding y propagación](#4-trampa-2-constant-folding-y-propagación)
5. [Trampa 3: Loop-invariant code motion](#5-trampa-3-loop-invariant-code-motion)
6. [Trampa 4: Branch predictor aprendiendo el patrón](#6-trampa-4-branch-predictor-aprendiendo-el-patrón)
7. [Trampa 5: Cache warming artificial](#7-trampa-5-cache-warming-artificial)
8. [Trampa 6: Tamaño de input vs jerarquía de cache](#8-trampa-6-tamaño-de-input-vs-jerarquía-de-cache)
9. [Trampa 7: Prefetcher de hardware engañado](#9-trampa-7-prefetcher-de-hardware-engañado)
10. [Trampa 8: TLB warming y effects de página](#10-trampa-8-tlb-warming-y-effects-de-página)
11. [Trampa 9: Alignment accidental](#11-trampa-9-alignment-accidental)
12. [Trampa 10: Datos del heap calentados por malloc](#12-trampa-10-datos-del-heap-calentados-por-malloc)
13. [Trampa 11: CPU frequency scaling durante el benchmark](#13-trampa-11-cpu-frequency-scaling-durante-el-benchmark)
14. [Trampa 12: Turbo boost y thermal throttling](#14-trampa-12-turbo-boost-y-thermal-throttling)
15. [Trampa 13: Hyperthreading y contención de core](#15-trampa-13-hyperthreading-y-contención-de-core)
16. [Trampa 14: Medir compile-time optimizations como runtime](#16-trampa-14-medir-compile-time-optimizations-como-runtime)
17. [Trampa 15: Benchmarks determinísticos con inputs predecibles](#17-trampa-15-benchmarks-determinísticos-con-inputs-predecibles)
18. [Trampa 16: Ignorar la alocación en el benchmark](#18-trampa-16-ignorar-la-alocación-en-el-benchmark)
19. [Trampa 17: Comparar O0 vs O3](#19-trampa-17-comparar-o0-vs-o3)
20. [Trampa 18: Benchmarking en máquina virtual o contenedor](#20-trampa-18-benchmarking-en-máquina-virtual-o-contenedor)
21. [Trampa 19: Pocas iteraciones, conclusiones generalizadas](#21-trampa-19-pocas-iteraciones-conclusiones-generalizadas)
22. [Trampa 20: Benchmark vs realidad de producción](#22-trampa-20-benchmark-vs-realidad-de-producción)
23. [El problema del microbenchmark aislado](#23-el-problema-del-microbenchmark-aislado)
24. [Diagnosticar un benchmark sospechoso](#24-diagnosticar-un-benchmark-sospechoso)
25. [Checklist exhaustivo de validación](#25-checklist-exhaustivo-de-validación)
26. [Cómo leer benchmarks de otros](#26-cómo-leer-benchmarks-de-otros)
27. [Programa de práctica: trap_detector](#27-programa-de-práctica-trap_detector)
28. [Ejercicios](#28-ejercicios)

---

## 1. Por qué los benchmarks mienten

Todos los tópicos anteriores de esta sección (T01, T02, T03) te han preparado para escribir benchmarks **técnicamente correctos**. Este tópico trata sobre cuando incluso un benchmark técnicamente correcto **miente** sobre el rendimiento real.

```
┌──────────────────────────────────────────────────────────────────────┐
│          La verdad incómoda sobre los benchmarks                     │
│                                                                      │
│  Un benchmark mide EXACTAMENTE lo que le dices que mida.             │
│  Eso casi nunca es lo mismo que "el rendimiento real".               │
│                                                                      │
│  Tu benchmark: función aislada, input conocido, cache caliente,      │
│                branch predictor entrenado, allocation amortizada.    │
│                                                                      │
│  Realidad:    función llamada en medio de otras, input variable,     │
│                cache fría, branch predictor confundido por otros     │
│                contextos, allocator fragmentado.                     │
│                                                                      │
│  Los números que obtienes son REALES pero CONTEXTUALES.              │
│  El error no es medir mal.                                           │
│  El error es extrapolar del microbenchmark a la aplicación.         │
└──────────────────────────────────────────────────────────────────────┘
```

### El problema fundamental

```
Microbenchmark:
  └─ función_a_medir()
     ├─ cache: caliente (warmup)
     ├─ input: predecible (datos fijos)
     ├─ branch: predictor entrenado
     └─ context: aislado

Producción:
  └─ handle_request()
     ├─ función_a_medir()  ← mismo código
     ├─ función_otra()     ← contención por recursos
     ├─ llamadas a kernel
     ├─ lock contention
     └─ interrupciones
```

El mismo código puede ser 10x más lento en producción que en el microbenchmark. Este tópico enseña a reconocer las 20 trampas más comunes, cómo detectarlas y cómo mitigarlas.

---

## 2. La jerarquía de trampas del benchmarking

```
┌──────────────────────────────────────────────────────────────────────┐
│            Taxonomía de trampas                                      │
│                                                                      │
│  Nivel 1: Trampas del COMPILADOR                                     │
│  ────────────────────────────────                                    │
│  • Dead code elimination                                             │
│  • Constant folding / propagation                                    │
│  • Loop-invariant code motion                                        │
│  • Inlining excesivo                                                 │
│  → Síntoma: el benchmark reporta ~0ns o tiempos irreales            │
│  → Fix: anti-DCE (black_box, do_not_optimize)                       │
│                                                                      │
│  Nivel 2: Trampas del HARDWARE                                       │
│  ────────────────────────────────                                    │
│  • Branch predictor learning                                         │
│  • Cache warming artificial                                          │
│  • TLB warming                                                       │
│  • Hardware prefetcher                                               │
│  • Frequency scaling                                                 │
│  • Turbo boost / thermal throttling                                  │
│  → Síntoma: resultados inconsistentes, depende del input             │
│  → Fix: variar inputs, controlar entorno, invalidar estado           │
│                                                                      │
│  Nivel 3: Trampas del DISEÑO                                         │
│  ────────────────────────────────                                    │
│  • Input no representativo                                           │
│  • Tamaño que cabe en cache cuando en realidad no cabría            │
│  • Allocator pre-calentado                                           │
│  • Datos contiguos cuando en realidad estarían dispersos             │
│  → Síntoma: el benchmark es optimista                                │
│  → Fix: diseñar inputs realistas, aleatorizar, fragmentar            │
│                                                                      │
│  Nivel 4: Trampas de INTERPRETACIÓN                                  │
│  ────────────────────────────────                                    │
│  • Extrapolar de micro a macro                                       │
│  • Ignorar la varianza                                               │
│  • Confundir mejora estadística con mejora práctica                  │
│  • No verificar contra producción                                    │
│  → Síntoma: la optimización no mejora el sistema real                │
│  → Fix: benchmarks end-to-end, profiling en producción               │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Trampa 1: Dead code elimination sin anti-DCE

Esta ya la conoces del T01 y T03, pero aquí veremos los **patrones sutiles** donde la gente piensa que está protegido y no lo está.

### Patrón engañoso 1: "asignar a variable local"

```c
// ❌ FALSO SENTIDO DE SEGURIDAD
for (int i = 0; i < ITERS; i++) {
    int result = expensive_function(42);
    // "asigno a result, seguro que el compilador debe calcularla"
}
// Realidad: result nunca se lee → el compilador elimina
// toda la llamada a expensive_function.
```

### Patrón engañoso 2: "print fuera del loop"

```c
// ❌ FALSO SENTIDO DE SEGURIDAD
int last_result = 0;
for (int i = 0; i < ITERS; i++) {
    last_result = expensive_function(42);
}
printf("Last: %d\n", last_result);
// "Imprimo last_result, así que debe existir"
// Realidad parcial: el compilador DEBE calcular expensive_function(42)
// UNA VEZ (el valor final). Elimina las demás ITERS-1 llamadas.
// El loop entero se reduce a una sola ejecución.
```

### Patrón engañoso 3: "sumar resultados"

```c
// ❌ A VECES FALSO
int sum = 0;
for (int i = 0; i < ITERS; i++) {
    sum += expensive_function(42);
}
printf("%d\n", sum);
// Mejor que el anterior: el compilador SÍ debe ejecutar todas las llamadas
// porque cada una contribuye a sum.
// PERO: si expensive_function(42) es pura y el compilador lo sabe,
// puede transformar a:
//   sum = ITERS * expensive_function(42);
// → sigue ejecutando expensive_function solo UNA vez.
```

### Patrón correcto 1: do_not_optimize en cada iteración

```c
// ✅ CORRECTO
for (int i = 0; i < ITERS; i++) {
    int result = expensive_function(42);
    do_not_optimize(result);
}
// do_not_optimize EN CADA ITERACIÓN le dice al compilador
// "este valor podría ser leído por alguien que no conozco",
// forzando el cómputo dentro del loop.
```

### Patrón correcto 2: variar el input con do_not_optimize

```c
// ✅ CORRECTO Y MÁS ROBUSTO
int input = 42;
for (int i = 0; i < ITERS; i++) {
    do_not_optimize(input);  // el compilador no puede asumir que input = 42
    int result = expensive_function(input);
    do_not_optimize(result);
}
// Ahora el compilador no puede:
// - Usar constant folding (no sabe el valor de input)
// - Usar LICM (no puede probar que la llamada produce el mismo resultado)
```

### Detectar DCE en tu propio código

```bash
# Generar assembly y buscar la llamada a la función
gcc -O2 -S -o bench.s bench.c
grep "call expensive_function" bench.s

# Si aparece 0 veces: el compilador eliminó todas las llamadas
# Si aparece 1 vez fuera del loop: LICM ocurrió
# Si aparece 1 vez dentro del loop: ¡correcto!
```

```
┌──────────────────────────────────────────────────────────────────────┐
│          Inspección de assembly paso a paso                          │
│                                                                      │
│  Código:                                                             │
│  for (int i = 0; i < 1000000; i++) {                                │
│      int r = fibonacci(20);                                          │
│      (void)r;                                                        │
│  }                                                                   │
│                                                                      │
│  Sin anti-DCE, GCC -O2 genera:                                       │
│  ─────────────────────────────                                       │
│  xor eax, eax          ; return 0                                    │
│  ret                                                                 │
│                                                                      │
│  TODO el loop desapareció. El compilador determinó que               │
│  el loop no tiene efecto observable.                                  │
│                                                                      │
│  Con do_not_optimize(r), GCC -O2 genera:                             │
│  ────────────────────────────────────────                            │
│  mov ecx, 1000000                                                    │
│ .L2:                                                                 │
│      ; inline de fibonacci(20) aquí                                  │
│      mov eax, 6765      ; resultado constante (aún con CF)          │
│      ; asm volatile "" — el do_not_optimize                         │
│      dec ecx                                                         │
│      jne .L2                                                         │
│  ret                                                                 │
│                                                                      │
│  Mejor: el loop existe. Pero el compilador hizo CF sobre            │
│  fibonacci(20) = 6765.                                               │
│                                                                      │
│  Con do_not_optimize en input Y output:                              │
│  ───────────────────────────────────                                 │
│  int n = 20;                                                         │
│  mov ecx, 1000000                                                    │
│ .L2:                                                                 │
│      ; inline real de fibonacci con loop dinámico                   │
│      mov eax, 0                                                      │
│      mov ebx, 1                                                      │
│      mov edx, 20                                                     │
│  .L3:                                                                │
│      lea esi, [eax+ebx]                                              │
│      mov eax, ebx                                                    │
│      mov ebx, esi                                                    │
│      dec edx                                                         │
│      jne .L3                                                         │
│      dec ecx                                                         │
│      jne .L2                                                         │
│  ret                                                                 │
│                                                                      │
│  ¡Ahora sí! El loop de fibonacci se ejecuta dentro del loop          │
│  del benchmark, tantas veces como iteraciones.                        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. Trampa 2: Constant folding y propagación

El compilador puede calcular expresiones con inputs conocidos en tiempo de compilación.

### Ejemplo sutil: arrays literales

```c
// ❌ TRAMPA
int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
const int N = sizeof(data) / sizeof(data[0]);

for (int i = 0; i < ITERS; i++) {
    int sum = 0;
    for (int j = 0; j < N; j++) {
        sum += data[j];
    }
    do_not_optimize(sum);
}
// El compilador sabe:
// - data[] es un array estático conocido
// - N = 10 es una constante
// - sum siempre = 55
// Puede transformar el loop interno en: sum = 55
// → Mides una asignación constante, no un loop de suma
```

```c
// ✅ SOLUCIÓN: inicializar el array en runtime
int data[10];
for (int i = 0; i < 10; i++) {
    data[i] = i + 1;
    do_not_optimize(data[i]);
}

for (int i = 0; i < ITERS; i++) {
    int sum = 0;
    for (int j = 0; j < 10; j++) {
        sum += data[j];
    }
    do_not_optimize(sum);
}
// El compilador sabe los valores, pero do_not_optimize
// en la inicialización le impide propagar las constantes.
```

### Ejemplo sutil: funciones marcadas como pure

```c
// ❌ TRAMPA con __attribute__((pure))
__attribute__((pure))
int expensive_computation(int x) {
    int result = 0;
    for (int i = 0; i < 1000; i++) {
        result += x * i;
    }
    return result;
}

// El atributo pure le dice al compilador: "no tengo efectos secundarios,
// mismo input → mismo output".
// Con eso:
for (int i = 0; i < ITERS; i++) {
    int r = expensive_computation(42);  // pure + input constante
    do_not_optimize(r);
}
// GCC puede aplicar CSE (common subexpression elimination):
// r1 = expensive_computation(42);  // una sola vez
// for ...: do_not_optimize(r1);    // reutiliza r1
```

```c
// ✅ SOLUCIÓN: variar el input
int input = 42;
for (int i = 0; i < ITERS; i++) {
    do_not_optimize(input);  // compilador no puede asumir input = 42
    int r = expensive_computation(input);
    do_not_optimize(r);
}
```

---

## 5. Trampa 3: Loop-invariant code motion

LICM mueve código fuera del loop si detecta que el resultado no depende de la variable de iteración.

### Ejemplo clásico

```c
// ❌ TRAMPA
int *arr = malloc(N * sizeof(int));
for (int i = 0; i < N; i++) arr[i] = i;

int target = 5000;

for (int i = 0; i < ITERS; i++) {
    int found = 0;
    for (int j = 0; j < N; j++) {
        if (arr[j] == target) { found = 1; break; }
    }
    do_not_optimize(found);
}
// El compilador puede detectar:
// - arr, N, target son invariantes entre iteraciones del loop exterior
// - El resultado de la búsqueda será el mismo cada vez
// Transformación:
//   int found = search(arr, N, target);  // una vez, fuera del loop
//   for (int i = 0; i < ITERS; i++) do_not_optimize(found);
```

```c
// ✅ SOLUCIÓN: clobber_memory para invalidar el análisis
for (int i = 0; i < ITERS; i++) {
    clobber_memory();  // "arr podría haber cambiado"
    int found = 0;
    for (int j = 0; j < N; j++) {
        if (arr[j] == target) { found = 1; break; }
    }
    do_not_optimize(found);
}
```

### Caso aún más sutil: LICM interno

```c
// ❌ TRAMPA sutil
for (int i = 0; i < ITERS; i++) {
    int sum = 0;
    for (int j = 0; j < N; j++) {
        sum += expensive_hash(salt, arr[j]);
        //                     ^^^^
        // salt es loop-invariant: no cambia en el loop interno
    }
    do_not_optimize(sum);
}
// Si expensive_hash es pura y el compilador lo ve,
// puede mover CUALQUIER precomputo que dependa solo de salt
// fuera del loop interno.
// Ejemplo: si expensive_hash(salt, x) = hash_init(salt) + mix(x),
// hash_init(salt) se calcula UNA VEZ fuera del loop.
```

### Detección de LICM

```bash
# Compilar con -O2 y verificar con un contador
# Alternativa: usar perf stat para contar instrucciones
perf stat -e instructions ./bench

# Si el número de instrucciones es sospechosamente bajo,
# probablemente LICM o DCE están activos.

# Otra técnica: comparar -O0 y -O2
# Si -O2 es 1000x más rápido que -O0 para una función simple,
# el compilador está haciendo optimizaciones agresivas.
# Si -O2 es sólo 5-10x más rápido, el código "resiste" al compilador.
```

---

## 6. Trampa 4: Branch predictor aprendiendo el patrón

El branch predictor moderno (TAGE, perceptron) puede **memorizar patrones** de hasta miles de branches. En un benchmark con un patrón repetitivo, el predictor acierta casi el 100% de las veces, incluso cuando en realidad el patrón sería impredecible.

```
┌──────────────────────────────────────────────────────────────────────┐
│            Cómo funciona el branch predictor moderno                 │
│                                                                      │
│  Predictores modernos (desde ~2016):                                 │
│  - TAGE (TAgged GEometric history): Intel, AMD                       │
│  - Perceptron-based: variantes en ARM                                │
│                                                                      │
│  Capacidad:                                                          │
│  - Pueden memorizar patrones de 2^16 a 2^20 branches                │
│  - Detectan correlaciones entre branches distantes                   │
│  - Aprenden secuencias periódicas                                     │
│                                                                      │
│  Ejemplo: si tu benchmark siempre hace el mismo patrón              │
│  de branches en el mismo orden, el predictor llegará                 │
│  a 99.9%+ de acierto después del warmup.                             │
│                                                                      │
│  En producción, ese mismo código tendría 70-90% de acierto           │
│  porque los datos son diferentes cada vez.                            │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo: búsqueda lineal con datos repetitivos

```c
// ❌ TRAMPA: el branch predictor memoriza el patrón
int data[10000];
for (int i = 0; i < 10000; i++) {
    data[i] = i; // datos predecibles
}

for (int iter = 0; iter < ITERS; iter++) {
    int target = 5000;  // siempre el mismo
    int found = -1;
    for (int j = 0; j < 10000; j++) {
        if (data[j] == target) {  // branch SIEMPRE falla hasta j=5000
            found = j;
            break;
        }
    }
    do_not_optimize(found);
}
// Después del warmup, el branch predictor sabe:
// "los primeros 5000 comparaciones son falsas, la 5001 es verdadera"
// → misprediction rate: ~0.02% (1 de 5001)
// En producción con datos variables: ~50% misprediction rate
// Diferencia de rendimiento: 10-30x
```

### Solución: inputs aleatorios y variables

```c
// ✅ SOLUCIÓN: targets aleatorios
#include <stdlib.h>

int data[10000];
for (int i = 0; i < 10000; i++) data[i] = i;

// Generar una lista de targets aleatorios
int targets[1000];
srand(42);
for (int i = 0; i < 1000; i++) {
    targets[i] = rand() % 10000;
}

for (int iter = 0; iter < ITERS; iter++) {
    int target = targets[iter % 1000];  // target variable
    int found = -1;
    for (int j = 0; j < 10000; j++) {
        if (data[j] == target) {
            found = j;
            break;
        }
    }
    do_not_optimize(found);
}
// El branch predictor no puede memorizar 1000 patrones diferentes
// tan bien como uno solo.
```

### Medir mispredictions

```bash
# Usar perf stat para medir branches y misses
perf stat -e branches,branch-misses ./bench

# Ejemplo de salida:
#   1,234,567,890 branches
#       1,234,567 branch-misses    # 0.10% of all branches
#
# 0.10% de miss rate es sospechosamente bajo para código real.
# En producción, 2-5% es normal.
```

---

## 7. Trampa 5: Cache warming artificial

Similar al branch predictor, el cache se "entrena" durante el warmup y los accesos de benchmark. Esto hace que los datos estén SIEMPRE calientes, lo cual rara vez refleja la realidad.

```c
// ❌ TRAMPA
int *data = malloc(1024 * 1024 * sizeof(int)); // 4 MB
for (int i = 0; i < 1024 * 1024; i++) data[i] = i;

// Warmup: accede a todos los datos
for (int i = 0; i < 10; i++) {
    long sum = 0;
    for (int j = 0; j < 1024 * 1024; j++) sum += data[j];
    do_not_optimize(sum);
}

// Benchmark
for (int iter = 0; iter < ITERS; iter++) {
    long sum = 0;
    for (int j = 0; j < 1024 * 1024; j++) sum += data[j];
    do_not_optimize(sum);
}

// Después del warmup, 4MB está en L2 o L3 cache.
// En producción, cada vez que se llama a la función,
// los datos tendrían que traerse de DRAM.
// Diferencia: 4x más lento en producción.
```

### Solución: invalidar el cache entre mediciones

```c
// ✅ SOLUCIÓN 1: flush explícito del cache
#include <x86intrin.h>

void flush_cache(void *data, size_t size) {
    char *p = (char *)data;
    for (size_t i = 0; i < size; i += 64) {  // 64 bytes = cache line
        _mm_clflush(p + i);
    }
    _mm_mfence();  // asegurar que los flushes terminen
}

// Entre cada iteración:
for (int iter = 0; iter < ITERS; iter++) {
    flush_cache(data, 1024 * 1024 * sizeof(int));
    
    // Medir
    clock_gettime(CLOCK_MONOTONIC, &start);
    long sum = 0;
    for (int j = 0; j < 1024 * 1024; j++) sum += data[j];
    do_not_optimize(sum);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Registrar tiempo
}
// Ahora cada iteración empieza con los datos en DRAM,
// como pasaría en producción.
```

```c
// ✅ SOLUCIÓN 2: contaminar el cache con datos grandes
void pollute_cache(void) {
    static char *junk = NULL;
    if (!junk) junk = malloc(64 * 1024 * 1024); // 64 MB
    for (int i = 0; i < 64 * 1024 * 1024; i += 64) {
        junk[i] = (char)i;
    }
    clobber_memory();
}

for (int iter = 0; iter < ITERS; iter++) {
    pollute_cache();  // llena las caches con datos basura
    
    // Ahora los datos reales están fuera del cache
    clock_gettime(CLOCK_MONOTONIC, &start);
    long sum = 0;
    for (int j = 0; j < 1024 * 1024; j++) sum += data[j];
    do_not_optimize(sum);
    clock_gettime(CLOCK_MONOTONIC, &end);
}
```

```
┌──────────────────────────────────────────────────────────────────────┐
│        ¿Cuándo usar cache warmup vs cold cache?                      │
│                                                                      │
│  Warm cache (cache caliente):                                         │
│  ─────────────────                                                    │
│  Apropiado cuando:                                                    │
│  - La función se llama MUCHAS veces consecutivas en producción       │
│  - Los datos típicamente ya están en cache                           │
│  - Estás midiendo el rendimiento steady-state                         │
│  Ejemplos:                                                            │
│  - Inner loop de una simulación                                      │
│  - Operación en un tight loop                                        │
│  - Hot path de un servidor                                           │
│                                                                      │
│  Cold cache (cache fría):                                             │
│  ─────────────────                                                    │
│  Apropiado cuando:                                                    │
│  - La función se llama ESPORÁDICAMENTE                               │
│  - Los datos cambian entre llamadas                                  │
│  - Estás midiendo el rendimiento de una llamada aislada              │
│  Ejemplos:                                                            │
│  - Handler de eventos raros                                          │
│  - Inicialización                                                    │
│  - Funciones de rollback o error handling                            │
│                                                                      │
│  Mejor práctica: reportar AMBOS números.                             │
│                                                                      │
│  Criterion y bench.h miden cache warm por defecto.                   │
│  Para medir cold, implementar manualmente.                           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8. Trampa 6: Tamaño de input vs jerarquía de cache

El rendimiento de muchas operaciones depende del tamaño del input y dónde cabe en la jerarquía de caches.

```
┌──────────────────────────────────────────────────────────────────────┐
│          Jerarquía de caches típica (Intel/AMD modernos)             │
│                                                                      │
│  Nivel      Tamaño típico   Latencia     Ancho de banda              │
│  ─────      ─────────────   ────────     ───────────────             │
│  L1d         32-48 KB        ~1 ns       ~2 TB/s                     │
│  L1i         32-48 KB        ~1 ns       ~2 TB/s                     │
│  L2          256 KB - 2 MB   ~4 ns       ~500 GB/s                   │
│  L3          8-64 MB         ~12 ns      ~300 GB/s                   │
│  DRAM        varios GB       ~80-100 ns  ~30 GB/s                    │
│                                                                      │
│  Tu función a medir: sum_array(arr, N)                               │
│                                                                      │
│  N * sizeof(int)    Ubicación   Ancho de banda efectivo              │
│  ──────────         ─────────   ──────────────────────               │
│  1 KB               L1d         ~2 TB/s                              │
│  4 KB               L1d         ~2 TB/s                              │
│  32 KB              L1d         ~2 TB/s                              │
│  64 KB              L2          ~500 GB/s                            │
│  256 KB             L2          ~500 GB/s                            │
│  1 MB               L2/L3       ~400 GB/s                            │
│  8 MB               L3          ~300 GB/s                            │
│  32 MB              L3/DRAM     ~100 GB/s                            │
│  128 MB             DRAM        ~30 GB/s                             │
│  1 GB               DRAM        ~30 GB/s                             │
│                                                                      │
│  Tu benchmark con N = 10000 (40 KB):                                 │
│  → cabe en L1d → reporta ~2 TB/s                                     │
│                                                                      │
│  Producción con N = 10,000,000 (40 MB):                              │
│  → no cabe en L3 → ~60 GB/s                                          │
│                                                                      │
│  Diferencia: 33x                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Demostración

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define do_not_optimize(val) __asm__ __volatile__("" : : "r,m"(val) : "memory")

static inline int64_t ts_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(void) {
    size_t sizes[] = {
        1 * 1024,      //   4 KB
        8 * 1024,      //  32 KB (límite L1)
        64 * 1024,     // 256 KB
        256 * 1024,    //   1 MB
        1024 * 1024,   //   4 MB
        4 * 1024 * 1024,  //  16 MB
        16 * 1024 * 1024, //  64 MB
        64 * 1024 * 1024, // 256 MB
    };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    printf("%-12s %-15s %-15s %-15s\n", 
           "Size (KB)", "Time/elem (ns)", "Bandwidth (GB/s)", "Cache level");
    
    for (int s = 0; s < num_sizes; s++) {
        size_t n = sizes[s];
        int *arr = (int *)aligned_alloc(64, n * sizeof(int));
        
        // Inicializar (también sirve de warmup del TLB)
        for (size_t i = 0; i < n; i++) arr[i] = i;
        
        // Warmup
        for (int w = 0; w < 3; w++) {
            long sum = 0;
            for (size_t i = 0; i < n; i++) sum += arr[i];
            do_not_optimize(sum);
        }
        
        // Medir (fijar tiempo total ~500ms)
        int iters = 0;
        int64_t start = ts_ns();
        int64_t target = start + 500000000LL; // 500 ms
        long sum = 0;
        
        while (ts_ns() < target) {
            for (int k = 0; k < 10; k++) {
                for (size_t i = 0; i < n; i++) sum += arr[i];
            }
            iters += 10;
        }
        do_not_optimize(sum);
        
        int64_t elapsed = ts_ns() - start;
        double ns_per_elem = (double)elapsed / ((double)iters * n);
        double gb_per_sec = (double)(n * sizeof(int)) / (double)elapsed * iters;
        
        const char *level;
        size_t size_kb = n * sizeof(int) / 1024;
        if (size_kb <= 32) level = "L1d";
        else if (size_kb <= 256) level = "L2";
        else if (size_kb <= 8192) level = "L3";
        else level = "DRAM";
        
        printf("%-12zu %-15.3f %-15.1f %-15s\n",
               size_kb, ns_per_elem, gb_per_sec, level);
        
        free(arr);
    }
    
    return 0;
}
```

### Salida típica

```
Size (KB)    Time/elem (ns)  Bandwidth (GB/s) Cache level
4            0.150           26.7             L1d
32           0.152           26.3             L1d
256          0.280           14.3             L2
1024         0.310           12.9             L3
4096         0.340           11.8             L3
16384        0.680           5.9              DRAM
65536        0.720           5.6              DRAM
262144       0.730           5.5              DRAM

Observación: el tiempo por elemento se multiplica por ~5x
entre L1 y DRAM. Si tu benchmark solo mide L1, reportas
un número 5x mejor que el real.
```

### Regla práctica

```
┌──────────────────────────────────────────────────────────────────────┐
│       Cómo elegir el tamaño de input en un benchmark                 │
│                                                                      │
│  Pregunta: ¿cuál es el tamaño típico de input en producción?         │
│                                                                      │
│  Si la respuesta es "pequeño y cabe en L1":                          │
│    → benchmark con tamaños en L1 es realista                         │
│    → pero reportar también tamaños mayores para dar contexto         │
│                                                                      │
│  Si la respuesta es "variable" o "grande":                           │
│    → benchmark con MÚLTIPLES tamaños (al menos uno por nivel)        │
│    → reportar la curva de rendimiento vs tamaño                      │
│                                                                      │
│  Si no conoces el tamaño típico:                                     │
│    → benchmark con: L1, L2, L3, y 4x L3 (para forzar DRAM)           │
│    → reportar los 4 números                                           │
│                                                                      │
│  Tamaños recomendados (CPU típico con L3 = 8MB):                     │
│  - L1: 16 KB (4 * 1024 ints)                                         │
│  - L2: 128 KB (32 * 1024 ints)                                       │
│  - L3: 2 MB (512 * 1024 ints)                                        │
│  - DRAM: 64 MB (16 * 1024 * 1024 ints)                               │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 9. Trampa 7: Prefetcher de hardware engañado

Los CPUs modernos tienen prefetchers automáticos que detectan patrones de acceso y precargan datos antes de que los necesites.

```
┌──────────────────────────────────────────────────────────────────────┐
│              Hardware prefetchers en Intel/AMD                        │
│                                                                      │
│  L1 Streaming Prefetcher (DCU):                                       │
│  - Detecta accesos secuenciales                                       │
│  - Precarga las siguientes líneas                                     │
│  - Funciona para strides de ±1, ±2, hasta 2KB                        │
│                                                                      │
│  L2 Streaming Prefetcher (Spatial):                                   │
│  - Detecta accesos a páginas cercanas                                 │
│  - Precarga hasta la siguiente página completa                        │
│                                                                      │
│  IP-based Prefetcher (Stride):                                        │
│  - Asocia un stride a cada instrucción (por PC)                      │
│  - Detecta strides regulares aunque no secuenciales                   │
│  - Puede manejar patrones como arr[i * 4]                            │
│                                                                      │
│  Implicación para benchmarks:                                         │
│  - Accesos SECUENCIALES: rendimiento artificial (prefetch oculta)   │
│  - Accesos ALEATORIOS: rendimiento real (prefetch fallido)          │
│                                                                      │
│  Benchmark típico: recorre array linealmente → prefetch perfecto     │
│  Producción típica: accesos por clave → prefetch inútil              │
│  Diferencia: 3-10x                                                    │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo: pointer-chasing vs sequential

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define do_not_optimize(val) __asm__ __volatile__("" : : "r,m"(val) : "memory")

typedef struct Node {
    struct Node *next;
    int value;
} Node;

int main(void) {
    const int N = 1024 * 1024; // 1M nodes, ~32 MB (fuera de L3)
    
    Node *nodes = malloc(N * sizeof(Node));
    int *indices = malloc(N * sizeof(int));
    
    // Inicializar valores
    for (int i = 0; i < N; i++) {
        nodes[i].value = i;
        indices[i] = i;
    }
    
    // Caso 1: enlazar secuencialmente (prefetch-friendly)
    for (int i = 0; i < N - 1; i++) {
        nodes[i].next = &nodes[i + 1];
    }
    nodes[N - 1].next = NULL;
    
    struct timespec start, end;
    
    // Medir traversal secuencial
    clock_gettime(CLOCK_MONOTONIC, &start);
    long sum = 0;
    Node *p = &nodes[0];
    while (p) {
        sum += p->value;
        p = p->next;
    }
    do_not_optimize(sum);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    int64_t seq_ns = (end.tv_sec - start.tv_sec) * 1000000000LL
                   + (end.tv_nsec - start.tv_nsec);
    
    // Caso 2: enlazar aleatoriamente (prefetch-hostile)
    // Shuffle de los índices
    srand(42);
    for (int i = N - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
    }
    
    for (int i = 0; i < N - 1; i++) {
        nodes[indices[i]].next = &nodes[indices[i + 1]];
    }
    nodes[indices[N - 1]].next = NULL;
    
    // Medir traversal aleatorio
    clock_gettime(CLOCK_MONOTONIC, &start);
    sum = 0;
    p = &nodes[indices[0]];
    while (p) {
        sum += p->value;
        p = p->next;
    }
    do_not_optimize(sum);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    int64_t rand_ns = (end.tv_sec - start.tv_sec) * 1000000000LL
                    + (end.tv_nsec - start.tv_nsec);
    
    printf("Traversal secuencial: %ld ns (%.2f ns/node)\n",
           (long)seq_ns, (double)seq_ns / N);
    printf("Traversal aleatorio:  %ld ns (%.2f ns/node)\n",
           (long)rand_ns, (double)rand_ns / N);
    printf("Ratio:                %.1fx\n", (double)rand_ns / (double)seq_ns);
    
    free(nodes);
    free(indices);
    return 0;
}
```

### Resultados típicos

```
Traversal secuencial: 3,500,000 ns  (3.34 ns/node)  ← prefetch perfecto
Traversal aleatorio:  85,000,000 ns (81.06 ns/node) ← DRAM latency exposed
Ratio:                24.3x

El mismo código, los mismos datos (un simple `sum += p->value; p = p->next;`),
pero 24x más lento cuando los nodos están en orden aleatorio.
El prefetcher solo ayuda con accesos predecibles.
```

### Cuándo importa

```
Aplicaciones donde el prefetching "oculta" el costo real:
- Procesamiento de arrays (verdadera localidad temporal)
- Matrix multiplication (patrón bloqueado)
- Convolución de imágenes

Aplicaciones donde el prefetching NO ayuda:
- Árboles (BST, B-tree): punteros impredecibles
- Hash tables con colisiones
- Linked lists (después de fragmentar la memoria)
- Grafos (recorridos DFS/BFS)
- Bases de datos (índices en disco virtual)

Si tu código real es del segundo tipo, medir el primero
te da números artificialmente buenos.
```

---

## 10. Trampa 8: TLB warming y effects de página

El TLB (Translation Lookaside Buffer) cachea traducciones de direcciones virtuales a físicas. Un TLB miss cuesta ~10-30 ciclos extra. En benchmarks con datos grandes (>2 MB), TLB misses pueden dominar.

```
┌──────────────────────────────────────────────────────────────────────┐
│                    TLB en x86-64 típico                              │
│                                                                      │
│  TLB L1 (dTLB):                                                       │
│  - 64 entradas para páginas de 4 KB → cubre 256 KB                   │
│  - 32 entradas para páginas de 2 MB → cubre 64 MB                    │
│  - 4 entradas para páginas de 1 GB                                   │
│                                                                      │
│  TLB L2 (STLB):                                                       │
│  - 1536-2048 entradas → cubre ~8 MB con pages de 4 KB                │
│                                                                      │
│  Costo de TLB miss:                                                  │
│  - TLB L1 miss → TLB L2: ~7 ciclos                                   │
│  - TLB L2 miss → page table walk: ~20-30 ciclos                     │
│  - Page table walk + page fault: muchísimo (ms)                     │
│                                                                      │
│  Implicación:                                                         │
│  Con páginas de 4 KB (default en Linux):                             │
│  - Datos hasta 256 KB: dTLB L1 suficiente, 0 misses                 │
│  - Datos hasta ~8 MB: STLB suficiente, ~7 ciclos extra por acceso    │
│  - Datos > 8 MB: page walks constantes, ~20-30 ciclos extra          │
│                                                                      │
│  Benchmark con datos de 1 MB: TLB warmed, 0 overhead                 │
│  Producción con datos fragmentados en muchas páginas: TLB thrashing  │
└──────────────────────────────────────────────────────────────────────┘
```

### Ver TLB misses con perf

```bash
# Contar TLB misses en tu benchmark
perf stat -e dTLB-loads,dTLB-load-misses,dTLB-store-misses ./bench

# Salida típica:
#     1,234,567,890      dTLB-loads
#         1,234,567      dTLB-load-misses    # 0.10% of dTLB loads
#
# < 0.1%: TLB caliente, bencn "realista" solo si el caso real es así
# 1-5%: TLB bajo presión, típico en producción con datos grandes
# > 10%: TLB thrashing severo, posiblemente optimizable con huge pages
```

### Huge pages: la alternativa

```bash
# Verificar si tu sistema tiene huge pages habilitadas
cat /sys/kernel/mm/transparent_hugepage/enabled
# [always] madvise never  → habilitadas por defecto (en always)

# En C, solicitar huge pages explícitamente:
#include <sys/mman.h>

void *data = mmap(NULL, size,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | (21 << MAP_HUGE_SHIFT),
                  -1, 0);
// 2^21 = 2 MB page size
// Requiere que el kernel tenga huge pages reservadas
```

---

## 11. Trampa 9: Alignment accidental

El rendimiento de ciertas operaciones depende del alineamiento de los datos. Un benchmark puede ser injustamente rápido si los datos están casualmente bien alineados.

```
┌──────────────────────────────────────────────────────────────────────┐
│            Efectos de alineamiento en x86-64                         │
│                                                                      │
│  Cache line: 64 bytes                                                │
│  Accesos cruzando cache lines: ~2x más lentos                        │
│                                                                      │
│  Page boundary: 4096 bytes                                           │
│  Accesos cruzando páginas: ~5-10x más lentos                         │
│                                                                      │
│  Alineamientos importantes:                                           │
│  - 4-byte alignment: para int, float                                 │
│  - 8-byte alignment: para double, pointers                           │
│  - 16-byte alignment: para SSE (movaps)                              │
│  - 32-byte alignment: para AVX2 (vmovaps)                            │
│  - 64-byte alignment: cache-line (evita split accesses)              │
│                                                                      │
│  Tu benchmark con malloc(N):                                         │
│  - malloc típicamente devuelve bloques alineados a 16 bytes          │
│  - La primera instancia está bien alineada                           │
│  - Las siguientes pueden no estarlo (depende del allocator)          │
│                                                                      │
│  Si en producción los datos vienen de lugares no alineados           │
│  (offsets en buffers, campos en structs), el rendimiento             │
│  puede ser 2-5x peor.                                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Demostración

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

static inline int64_t ts_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void test_alignment(void *data, int offset, const char *label) {
    // Usar offset bytes del inicio para desalinear
    char *p = (char *)data + offset;
    
    const int ITERS = 1000000;
    const int N = 64 * 1024; // 64 KB
    
    // Warmup
    for (int i = 0; i < 1000; i++) {
        memcpy(p, p + N, N);
    }
    
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        memcpy(p, p + N, N);
    }
    int64_t elapsed = ts_ns() - start;
    
    printf("%-30s %.2f ns/call\n", label, (double)elapsed / ITERS);
}

int main(void) {
    // Reservar memoria alineada a 4KB (suficiente espacio)
    void *buffer = aligned_alloc(4096, 1024 * 1024);
    memset(buffer, 0, 1024 * 1024);
    
    printf("memcpy(64KB) con diferentes alineamientos:\n\n");
    
    test_alignment(buffer, 0,   "Offset  0 (64-byte aligned)");
    test_alignment(buffer, 1,   "Offset  1 (unaligned)");
    test_alignment(buffer, 8,   "Offset  8 (8-byte aligned)");
    test_alignment(buffer, 16,  "Offset 16 (16-byte aligned)");
    test_alignment(buffer, 32,  "Offset 32 (32-byte aligned)");
    test_alignment(buffer, 64,  "Offset 64 (64-byte aligned)");
    test_alignment(buffer, 4095,"Offset 4095 (crosses page)");
    
    free(buffer);
    return 0;
}
```

```
Salida típica:
memcpy(64KB) con diferentes alineamientos:

Offset  0 (64-byte aligned)    1250.30 ns/call  ← óptimo
Offset  1 (unaligned)          1890.45 ns/call  ← 1.5x peor
Offset  8 (8-byte aligned)     1280.60 ns/call  ← casi óptimo
Offset 16 (16-byte aligned)    1265.20 ns/call  ← óptimo
Offset 32 (32-byte aligned)    1258.15 ns/call  ← óptimo
Offset 64 (64-byte aligned)    1250.30 ns/call  ← óptimo
Offset 4095 (crosses page)     2340.80 ns/call  ← 1.9x peor

El mismo código, exactamente la misma cantidad de datos,
pero con un rendimiento 2x peor cuando el alineamiento
es desfavorable.
```

---

## 12. Trampa 10: Datos del heap calentados por malloc

El allocator mantiene estructuras internas que pueden "calentar" el cache y sesgar los benchmarks.

```c
// ❌ TRAMPA
for (int i = 0; i < ITERS; i++) {
    void *p = malloc(1024);
    memset(p, 0, 1024);
    do_not_optimize(p);
    free(p);
}
// Después de unas cuantas iteraciones:
// - malloc devuelve SIEMPRE la misma dirección
//   (free devuelve al pool, malloc la toma del mismo pool)
// - Esa región de memoria está CALIENTE en el cache
// - memset escribe en cache caliente (~1 ns por cache line)
//
// En producción: cada malloc() devuelve una región diferente,
// probablemente FUERA del cache → memset mucho más lento.
```

### Solución

```c
// ✅ SOLUCIÓN: variar el tamaño o fragmentar
#define N_POINTERS 1000

void **pointers = malloc(N_POINTERS * sizeof(void *));
size_t sizes[N_POINTERS];

// Pre-allocar tamaños aleatorios
srand(42);
for (int i = 0; i < N_POINTERS; i++) {
    sizes[i] = 512 + (rand() % 1024);
    pointers[i] = malloc(sizes[i]);
}

// Benchmark: re-usar punteros en orden aleatorio
for (int iter = 0; iter < ITERS; iter++) {
    int idx = rand() % N_POINTERS;
    memset(pointers[idx], 0, sizes[idx]);
    clobber_memory();
}

// Cleanup
for (int i = 0; i < N_POINTERS; i++) free(pointers[i]);
free(pointers);
```

### Medir allocator behavior

```bash
# Heaptrack (más detallado)
heaptrack ./bench
heaptrack --analyze heaptrack.bench.gz

# Massif (menos detallado, más portable)
valgrind --tool=massif ./bench
ms_print massif.out.<pid>
```

---

## 13. Trampa 11: CPU frequency scaling durante el benchmark

El CPU cambia de frecuencia según la carga. Un benchmark corto puede ejecutarse a una frecuencia y otro a otra.

```
┌──────────────────────────────────────────────────────────────────────┐
│           Estados de frecuencia del CPU (P-states)                   │
│                                                                      │
│  Base frequency:    ~2.4 GHz (la frecuencia "oficial")              │
│  Turbo boost 1:     ~3.6 GHz (1 core activo)                        │
│  Turbo boost all:   ~3.2 GHz (todos los cores activos)              │
│  Power save:        ~800 MHz (idle, bajo consumo)                   │
│                                                                      │
│  Benchmark corto (<10ms):                                            │
│  - Empieza con CPU en idle (800 MHz)                                │
│  - Tarda ~5-20ms en alcanzar la frecuencia objetivo                  │
│  - Los primeros samples son artificialmente lentos                   │
│                                                                      │
│  Benchmark largo con pausas:                                         │
│  - Cada pausa el CPU baja a idle                                     │
│  - Cada nuevo sample paga el ramp-up                                 │
│                                                                      │
│  Solución: warmup largo (>100ms) antes de medir                     │
│  Mejor: fijar la frecuencia del CPU                                  │
└──────────────────────────────────────────────────────────────────────┘
```

### Fijar la frecuencia del CPU

```bash
# Ver el governor actual
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Típicamente: powersave, performance, ondemand, schedutil

# Cambiar a performance (requiere root)
sudo cpupower frequency-set -g performance

# O manualmente en cada CPU:
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu
done

# Verificar la frecuencia actual
watch -n 0.5 'cat /proc/cpuinfo | grep "cpu MHz"'

# Desactivar turbo boost (para consistencia)
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# Alternativa (AMD o no-intel_pstate):
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost

# Al terminar, restaurar:
sudo cpupower frequency-set -g schedutil
echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
```

---

## 14. Trampa 12: Turbo boost y thermal throttling

Incluso con frecuencia "fija", el CPU puede bajar de frecuencia por temperatura.

```
┌──────────────────────────────────────────────────────────────────────┐
│          Thermal throttling en benchmarks largos                     │
│                                                                      │
│  Segundos 0-30:                                                       │
│    Temperatura: 45°C → 85°C                                          │
│    Frecuencia: 3.6 GHz (turbo)                                       │
│    Tu benchmark: 7.2 ns/op                                            │
│                                                                      │
│  Segundos 30-60:                                                      │
│    Temperatura: 85°C → 95°C (TjMax)                                  │
│    Frecuencia: 3.6 → 3.2 GHz (throttling)                            │
│    Tu benchmark: 8.1 ns/op                                            │
│                                                                      │
│  Segundos 60+:                                                        │
│    Temperatura: 95°C (capped)                                         │
│    Frecuencia: 2.8 GHz (throttling pesado)                           │
│    Tu benchmark: 9.3 ns/op                                            │
│                                                                      │
│  Resultado: el mismo código reporta 7.2 ns al inicio y 9.3 ns al    │
│  final. Promedio: ~8 ns. Mediana: ~8.1 ns.                          │
│                                                                      │
│  Varianza entre samples: altísima, no por ruido sino por throttling. │
└──────────────────────────────────────────────────────────────────────┘
```

### Monitorear temperatura

```bash
# Ver temperatura en tiempo real
watch -n 0.5 'sensors | grep Core'

# Durante el benchmark, verificar si hay throttling
dmesg | grep -i throttl

# Métrica de perf para throttling
perf stat -e cycles,msr/therm_throttle/ ./bench
```

### Mitigación

```
1. Benchmarks cortos (<30 segundos total)
2. Pausas entre iteraciones para que el CPU se enfríe
3. Mejor refrigeración (obvio pero efectivo)
4. Fijar a un core que no tenga turbo (base frequency)
5. Ejecutar en horas frías (temperatura ambiente baja)
6. Reportar temperatura junto con los resultados
```

---

## 15. Trampa 13: Hyperthreading y contención de core

Si ejecutas tu benchmark en un core con hyperthreading, otro hilo en el mismo core físico puede robarte recursos.

```
┌──────────────────────────────────────────────────────────────────────┐
│         Hyperthreading y benchmarks                                  │
│                                                                      │
│  Core físico 0:                                                       │
│  ┌─────────────────────────────────────────┐                         │
│  │ Logical CPU 0 (hyperthread 0)           │                         │
│  │ Logical CPU 4 (hyperthread 1)           │                         │
│  └─────────────────────────────────────────┘                         │
│     Comparten: ALUs, cache L1, cache L2, ejecución                   │
│                                                                      │
│  Si ejecutas benchmark en CPU 0 y el scheduler pone                  │
│  otro proceso en CPU 4, los dos pelean por los recursos              │
│  del core físico 0.                                                  │
│                                                                      │
│  Resultado: rendimiento hasta 2x peor que aislado.                   │
│                                                                      │
│  Solución 1: desactivar hyperthreading                               │
│  echo 0 | sudo tee /sys/devices/system/cpu/smt/control              │
│                                                                      │
│  Solución 2: fijar el benchmark a un core Y su sibling               │
│  taskset -c 0,4 ./bench                                              │
│                                                                      │
│  Solución 3: usar core aislado con isolcpus                         │
│  # En /etc/default/grub:                                             │
│  # GRUB_CMDLINE_LINUX_DEFAULT="... isolcpus=3"                      │
│  # Reboot                                                            │
│  taskset -c 3 ./bench                                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Identificar pares de hyperthreading

```bash
# Ver la topología de CPUs
lscpu -e
# CPU NODE SOCKET CORE L1d:L1i:L2:L3 ONLINE
#   0    0      0    0 0:0:0:0          yes
#   1    0      0    1 1:1:1:0          yes
#   2    0      0    2 2:2:2:0          yes
#   3    0      0    3 3:3:3:0          yes
#   4    0      0    0 0:0:0:0          yes  ← sibling de CPU 0
#   5    0      0    1 1:1:1:0          yes  ← sibling de CPU 1

# CPU 0 y CPU 4 comparten el core físico 0.
# Para aislar el core físico 0:
taskset -c 0,4 ./bench
```

---

## 16. Trampa 14: Medir compile-time optimizations como runtime

A veces el compilador puede hacer optimizaciones específicas al input del benchmark que no ocurrirían en producción.

### Ejemplo: template instantiation en C++ / monomorphization en Rust

```rust
// Rust: el compilador monomorfiza genéricos
fn sum<T: std::ops::AddAssign + Default + Copy>(data: &[T]) -> T {
    let mut sum = T::default();
    for &x in data {
        sum += x;
    }
    sum
}

// Benchmark:
let data: Vec<i32> = (0..1000).collect();
c.bench_function("sum_i32", |b| {
    b.iter(|| sum(black_box(&data)))
});
// El compilador genera código ESPECÍFICO para i32.
// En producción, podrías usar sum con f64, i64, u128, etc.
// Cada tipo tendría rendimiento diferente.
```

### Ejemplo: C con `__builtin_constant_p`

```c
// Funciones que detectan constantes en compilación
int my_strlen(const char *s) {
    if (__builtin_constant_p(s) && __builtin_constant_p(*s)) {
        // El compilador sabe cuánto mide el string → resultado constante
        // Esta rama probablemente se elimina, pero muestra
        // que las funciones pueden adaptarse al input conocido.
    }
    // ... implementación normal
}

// Benchmark:
for (int i = 0; i < ITERS; i++) {
    int len = my_strlen("hello world"); // literal conocido
    do_not_optimize(len);
}
// El compilador puede aplicar optimizaciones que no haría con
// un string proveniente de un archivo o una entrada de usuario.
```

### Solución

```c
// Cargar datos desde disco o construirlos en runtime
char buffer[256];
// Leer de un archivo o construir dinámicamente
read(fd, buffer, 256);
do_not_optimize(buffer);

for (int i = 0; i < ITERS; i++) {
    int len = my_strlen(buffer);
    do_not_optimize(len);
}
// Ahora el compilador no tiene información sobre el contenido
// del buffer → no puede aplicar optimizaciones específicas.
```

---

## 17. Trampa 15: Benchmarks determinísticos con inputs predecibles

Un benchmark reproducible ES deseable, pero si los datos son demasiado predecibles, el branch predictor y el prefetcher rinden al máximo.

### Ejemplo: ordenamiento con datos repetidos

```c
// ❌ TRAMPA
int data[10000];
for (int i = 0; i < 10000; i++) {
    data[i] = rand(); // parece aleatorio pero...
}

// rand() con seed default (srand no llamado) genera la MISMA secuencia
// cada vez. Si ejecutas el benchmark varias veces, los datos son idénticos.
// El CPU "aprende" el patrón después del warmup.
```

```c
// ✅ SOLUCIÓN: variar la seed por ejecución
srand(time(NULL));
for (int i = 0; i < 10000; i++) {
    data[i] = rand();
}
// Cada ejecución tiene datos diferentes.
// Pero DENTRO de una ejecución, el benchmark es reproducible.
```

### Ejemplo aún más sutil: búsqueda con targets predecibles

```c
// ❌ TRAMPA
int data[10000];
for (int i = 0; i < 10000; i++) data[i] = i;

for (int iter = 0; iter < ITERS; iter++) {
    int target = iter % 10000;  // patrón predecible
    int found = binary_search(data, 10000, target);
    do_not_optimize(found);
}
// El branch predictor aprende que en las primeras iteraciones
// el target es pequeño (early exit), luego crece monotónicamente.
```

```c
// ✅ SOLUCIÓN: orden aleatorio dentro de los targets
int targets[ITERS];
for (int i = 0; i < ITERS; i++) targets[i] = i % 10000;

// Fisher-Yates shuffle
srand(42);
for (int i = ITERS - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int tmp = targets[i]; targets[i] = targets[j]; targets[j] = tmp;
}

for (int iter = 0; iter < ITERS; iter++) {
    int found = binary_search(data, 10000, targets[iter]);
    do_not_optimize(found);
}
```

---

## 18. Trampa 16: Ignorar la alocación en el benchmark

Ya lo tocamos en el T02 para Rust, pero vale la pena profundizar.

### Ejemplo

```c
// ❌ El allocator es parte del benchmark, pero dominando
for (int iter = 0; iter < ITERS; iter++) {
    char *buf = malloc(1024);
    memcpy(buf, source, 1024);
    do_not_optimize(buf);
    free(buf);
}
// Estás midiendo: malloc + memcpy + free
// Proporciones típicas: malloc ~40%, memcpy ~10%, free ~20%, resto ~30%
// Si piensas que estás midiendo memcpy, estás midiendo principalmente malloc.
```

### Solución: pre-allocar

```c
// ✅ Aislar la operación de interés
char *buf = malloc(1024);
char *source = malloc(1024);
memset(source, 'A', 1024);

for (int iter = 0; iter < ITERS; iter++) {
    memcpy(buf, source, 1024);
    clobber_memory();
}

free(buf);
free(source);
// Ahora mides solo memcpy, no el ciclo de vida del allocator.
```

### Pero cuidado con el extremo opuesto

```
Si tu código en producción alloca/libera constantemente,
pre-allocar en el benchmark DISTORSIONA los resultados
en la dirección opuesta:

Código real:
  for request in requests:
      buffer = allocate_per_request()
      process(buffer)
      free(buffer)

Benchmark con pre-alloc:
  buffer = allocate_once()
  for iteration in iterations:
      process(buffer)
  free(buffer)

El benchmark reporta solo el costo de `process`,
pero en producción el costo real incluye allocate/free.

Regla: el benchmark debe reflejar cómo se usa el código
en producción. Si el allocation es parte del uso normal,
debe ser parte del benchmark.
```

---

## 19. Trampa 17: Comparar O0 vs O3

```c
// ❌ CLÁSICO ERROR DE PRINCIPIANTE
// Compilar con: gcc -O0 bench.c -o bench_slow
// Compilar con: gcc -O3 bench.c -o bench_fast
// "Mira, cambiando a -O3 es 20x más rápido!"

// Realidad: nadie corre código de producción con -O0.
// Estás comparando "rendimiento sin optimizar" vs "rendimiento optimizado".
// Esa comparación es inútil.

// La comparación correcta:
// -O2 vs -O3 (¿vale la pena -O3?)
// -O2 vs -O2 -march=native (¿las instrucciones del CPU específico ayudan?)
// -O2 vs -Os (¿optimizar por tamaño es más rápido por I-cache?)
// clang -O2 vs gcc -O2 (¿qué compilador es mejor para este código?)
```

### Tabla de comparaciones útiles

```
┌──────────────────────────────────────────────────────────────────────┐
│        Comparaciones útiles entre niveles de optimización           │
│                                                                      │
│  Comparación               Qué aprendes                              │
│  ───────────               ──────────────                            │
│  -O2 vs -O0                Cuánto optimiza el compilador             │
│                            (irrelevante para benchmarking)           │
│                                                                      │
│  -O2 vs -O3                Si vale la pena el aumento de tamaño      │
│                            (a veces -O3 es más lento por I-cache)    │
│                                                                      │
│  -O2 vs -Os                Efecto del tamaño del código              │
│                            (-Os puede ganar en código con mucho hot) │
│                                                                      │
│  -O2 vs -O2 -march=native  Beneficio de AVX2/AVX512 específico      │
│                            (puede ser 1.5-4x en código vectorizable) │
│                                                                      │
│  -O2 vs -O2 -flto          Link-time optimization cross-file         │
│                            (beneficio típico: 5-15%)                 │
│                                                                      │
│  -O2 vs -O2 -funroll-loops Loop unrolling manual                    │
│                            (a veces ayuda, a veces empeora)          │
│                                                                      │
│  -O2 gcc vs -O2 clang      Qué compilador es mejor para este código  │
│                            (puede variar 10-30%)                     │
│                                                                      │
│  Regla: el baseline debe ser -O2 (o lo que uses en producción).      │
│  Nunca comparar contra -O0 a menos que -O0 sea lo que usas.          │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 20. Trampa 18: Benchmarking en máquina virtual o contenedor

VMs y contenedores introducen capas adicionales de abstracción que afectan los benchmarks.

```
┌──────────────────────────────────────────────────────────────────────┐
│          Problemas de benchmarks en entornos virtualizados           │
│                                                                      │
│  Virtual machines (KVM, VMware, etc.):                               │
│  - CPU steal time: el host puede quitarte tiempo                     │
│  - Tiempo "detenido" durante la migración de VM                      │
│  - Paravirtualización del timer (menos preciso)                      │
│  - Cache del host es compartido con otras VMs                        │
│                                                                      │
│  Containers (Docker, Podman):                                         │
│  - Mínimo overhead sobre el kernel del host                          │
│  - Limits de cgroup (CPU, memoria) pueden activarse                  │
│  - Ruido del otro tráfico en el host                                 │
│                                                                      │
│  Cloud (AWS, GCP, Azure):                                             │
│  - Instancias "burstable" (T3, e2-small): CPU credits limitados     │
│  - Neighbor noise: otras VMs en el mismo hardware                    │
│  - CPU frequency no controlable                                      │
│  - Mediciones pueden ser erráticas a lo largo del día                │
│                                                                      │
│  WSL2 (Windows Subsystem for Linux):                                 │
│  - Corre dentro de una VM Hyper-V                                    │
│  - Timer de baja precisión                                           │
│  - Cache thrashing con Windows                                       │
│  - No recomendado para benchmarks serios                             │
└──────────────────────────────────────────────────────────────────────┘
```

### Detectar si estás en una VM

```bash
# Ver si hay hypervisor
grep -i hypervisor /proc/cpuinfo
# Si aparece "hypervisor" en flags, estás en una VM

# Ver el hypervisor específico
systemd-detect-virt
# Posibles: kvm, qemu, vmware, oracle, microsoft, none

# Ver steal time (tiempo que el host te está robando)
vmstat 1 5
# Columna 'st' = steal time. Si > 0%, otros procesos comparten tu CPU.
```

### Mejores prácticas para VMs

```
1. Usar CPU pinning en el host si es posible
2. Reservar recursos dedicados para la VM
3. Desactivar ballooning y migración durante el benchmark
4. Ejecutar múltiples iteraciones del experimento completo
5. Reportar la VARIANZA entre ejecuciones (no solo la media)
6. Si es posible, benchmarkear en hardware dedicado
7. Nunca publicar resultados de benchmarks de cloud sin aclarar
```

---

## 21. Trampa 19: Pocas iteraciones, conclusiones generalizadas

```
┌──────────────────────────────────────────────────────────────────────┐
│        El problema de la generalización prematura                    │
│                                                                      │
│  Benchmark:                                                          │
│  for size in [1000, 10000]:                                          │
│      time_a = bench(algo_a, size)                                    │
│      time_b = bench(algo_b, size)                                    │
│      if time_a < time_b: print("A wins")                             │
│      else: print("B wins")                                           │
│                                                                      │
│  Resultado:                                                          │
│  size=1000: A wins (A: 1.2µs, B: 1.5µs)                             │
│  size=10000: A wins (A: 14µs, B: 17µs)                              │
│                                                                      │
│  Conclusión prematura: "A es siempre más rápido"                     │
│                                                                      │
│  Realidad (con más tamaños):                                         │
│  size=100:    A wins (0.15µs vs 0.20µs)                              │
│  size=1000:   A wins (1.2µs vs 1.5µs)                                │
│  size=10000:  A wins (14µs vs 17µs)                                  │
│  size=100000: B wins (145µs vs 130µs)  ← cambio                      │
│  size=1000000: B wins (1600µs vs 1200µs) ← B gana por 33%            │
│                                                                      │
│  La complejidad algorítmica de A podría ser O(n²) mientras B es     │
│  O(n log n). En inputs pequeños A gana por menor constante,          │
│  en inputs grandes B gana por mejor complejidad.                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Reglas para evitar generalizar

```
1. Medir al menos 4 tamaños: pequeño, mediano, grande, muy grande
2. Graficar tiempo vs tamaño en escala log-log
   - Pendiente 1 = O(n), pendiente 2 = O(n²), etc.
3. Medir al menos 2 patrones de datos: random y worst-case
4. Ejecutar en al menos 2 máquinas diferentes (si es posible)
5. Compilar con al menos 2 compiladores (gcc y clang)
6. Reportar EL RANGO donde una alternativa gana, no "gana siempre"
```

### Ejemplo de reporte correcto

```
Comparativa algoritmo A vs B:

Tamaño (n)     Tiempo A        Tiempo B        Ganador
─────────     ────────        ────────        ───────
100            0.15 µs         0.20 µs         A (25% más rápido)
1,000          1.2 µs          1.5 µs          A (20% más rápido)
10,000         14 µs           17 µs           A (18% más rápido)
100,000        145 µs          130 µs          B (12% más rápido)
1,000,000      1.6 ms          1.2 ms          B (33% más rápido)
10,000,000     17 ms           13 ms           B (31% más rápido)

Conclusión: A gana para n ≤ 50,000. B gana para n > 50,000.
El crossover está aproximadamente en n ≈ 50,000.

Si tu workload típico es n < 50K, usa A.
Si tu workload típico es n > 50K, usa B.
```

---

## 22. Trampa 20: Benchmark vs realidad de producción

La trampa final y más importante. Incluso con todas las demás trampas evitadas, tu benchmark puede no reflejar cómo se comporta el código en producción.

```
┌──────────────────────────────────────────────────────────────────────┐
│        Diferencias típicas benchmark vs producción                   │
│                                                                      │
│  Aspecto                  Benchmark          Producción               │
│  ───────                  ─────────          ───────────              │
│  Ejecución               Aislada             Con muchas otras         │
│                                             funciones compitiendo    │
│                                                                      │
│  Cache                   Caliente            Frío (otras cosas la   │
│                                             contaminan)              │
│                                                                      │
│  Branch predictor        Entrenado           Confundido por otros    │
│                                             contextos               │
│                                                                      │
│  Memory allocator        Predecible          Fragmentado             │
│                                                                      │
│  TLB                     Calentado           Frío                    │
│                                                                      │
│  CPU frequency           Máxima              Variable (power mgmt)   │
│                                                                      │
│  Contention              Ninguna             Con otros procesos,     │
│                                             hilos, interrupts        │
│                                                                      │
│  Input distribution      Sintético/uniforme Real/skewed              │
│                                                                      │
│  Data locality           Contigua            Dispersa                │
│                                                                      │
│  Call patterns           Loop apretado       Llamadas esporádicas    │
│                                                                      │
│  Result: el mismo código puede ser 2-50x más lento en producción.    │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo real

```
Microbenchmark de hash_lookup:
- hash table 1 MB (cabe en L3)
- 1000 lookups en loop apretado
- tiempo por lookup: 15 ns

Producción (servidor web):
- hash table 50 MB (no cabe en cache)
- lookups esporádicos (1 cada ~10 requests)
- tiempo por lookup: 250 ns (16x más lento)

¿Por qué?
- L3 miss: 80 ns de latencia
- TLB miss: 20 ns
- Entre lookups, el cache se contamina con todo lo demás
- La entrada de hash probablemente no está en cache
- El propio hash table no está en TLB

Lección: el microbenchmark es útil para entender
el UPPER BOUND del rendimiento, no el caso típico.
```

### Cómo validar contra producción

```
1. Profiling en producción (perf record, eBPF)
   - Ver cuánto tarda realmente la función
   - Comparar con el microbenchmark
   
2. Flamegraphs
   - Verificar que la función está en los hotspots esperados
   
3. Metrics de aplicación
   - Antes y después de una optimización
   - Medir el throughput real del sistema, no solo latencia
   
4. A/B testing
   - Desplegar la optimización al 10% del tráfico
   - Comparar métricas clave (latency, throughput, CPU)
   
5. Benchmarks end-to-end
   - Benchmark del sistema completo, no de funciones
   - Usar cargas realistas (load testing tools)
```

---

## 23. El problema del microbenchmark aislado

Los microbenchmarks son útiles para **tomar decisiones locales** (¿qsort o mergesort?), no para **predecir rendimiento global**.

```
┌──────────────────────────────────────────────────────────────────────┐
│        Niveles de medición y su validez                              │
│                                                                      │
│  Nivel                  Útil para                  No útil para       │
│  ─────                  ─────────                  ─────────────      │
│  Microbenchmark         Elegir entre              Predecir latencia   │
│  (<1µs por op)          algoritmos                de requests         │
│                         Evaluar optimizaciones                        │
│                         locales                                       │
│                                                                      │
│  Meso benchmark         Perfilar componentes       Comparar sistemas   │
│  (funciones de          Detectar regresiones                          │
│  1-100µs)                                                             │
│                                                                      │
│  Macro benchmark        Comparar sistemas          Identificar fuente │
│  (requests end-to-end)  Validar optimizaciones    de degradación     │
│                                                                      │
│  Production metrics    Detectar degradación       Reproducir bugs     │
│                         Capacity planning                             │
│                         SLO compliance                                │
│                                                                      │
│  Un sistema bien diseñado tiene los 4 niveles.                       │
│  Los microbenchmarks informan decisiones de implementación.          │
│  Los macrobenchmarks validan la implementación integrada.             │
│  Las métricas de producción son la verdad final.                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 24. Diagnosticar un benchmark sospechoso

Cuando un benchmark da resultados que parecen demasiado buenos (o demasiado malos) para ser verdad:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Flowchart de diagnóstico                                     │
│                                                                      │
│  ¿El tiempo reportado es irrealmente bajo?                           │
│  │                                                                   │
│  ├─ SÍ → Sospecha: DCE, constant folding, LICM                       │
│  │       Acción: revisar assembly, añadir do_not_optimize             │
│  │                                                                   │
│  └─ NO → ¿El tiempo tiene CV alto (>10%)?                            │
│        │                                                             │
│        ├─ SÍ → Sospecha: ruido del entorno                           │
│        │       Acción: CPU pinning, cerrar apps, desactivar turbo    │
│        │                                                             │
│        └─ NO → ¿El resultado es consistente pero diferente al        │
│                esperado?                                              │
│              │                                                       │
│              ├─ Más rápido → Sospecha: cache warming, branch         │
│              │              predictor, alignment, inputs sintéticos  │
│              │              Acción: variar inputs, contaminar cache, │
│              │              usar datos realistas                      │
│              │                                                       │
│              └─ Más lento → Sospecha: setup incluido, I/O,           │
│                            alloc en el loop                          │
│                            Acción: mover setup fuera del loop,       │
│                            pre-allocar, revisar syscalls              │
└──────────────────────────────────────────────────────────────────────┘
```

### Técnicas de diagnóstico

```bash
# 1. Assembly inspection
gcc -O2 -S -fverbose-asm bench.c
grep -A 10 "call target_function" bench.s

# 2. perf stat detallado
perf stat -d -d -d ./bench
# -d -d -d = detailed level 3 (todo)

# 3. Counters específicos
perf stat -e cycles,instructions,cache-references,cache-misses,\
branches,branch-misses,dTLB-loads,dTLB-load-misses ./bench

# 4. Verificar que todas las iteraciones ocurren
perf stat -e instructions ./bench
# Calcular: instructions / iterations debería ser >> 0

# 5. Capturar el programa con strace para ver syscalls
strace -c ./bench
# ¿Hay syscalls dentro del loop? (no debería haber)

# 6. Verificar allocations
# Usar LD_PRELOAD con un wrapper de malloc para contar llamadas
```

---

## 25. Checklist exhaustivo de validación

Antes de confiar en los resultados de un benchmark:

```
┌──────────────────────────────────────────────────────────────────────┐
│           CHECKLIST PREVIO A LA EJECUCIÓN                            │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Entorno:                                                            │
│  □ CPU pinning aplicado (taskset -c N)                               │
│  □ Governor en performance                                           │
│  □ Turbo boost desactivado (o consistentemente activado)             │
│  □ Hyperthreading considerado                                        │
│  □ ASLR desactivado (o irrelevante para este benchmark)              │
│  □ Aplicaciones pesadas cerradas                                     │
│  □ No estoy en una VM si puedo evitarlo                              │
│                                                                      │
│  Compilación:                                                        │
│  □ Usando el mismo nivel de optimización que producción              │
│  □ Flags consistentes entre corridas                                 │
│  □ Versión del compilador documentada                                │
│  □ LTO habilitado/deshabilitado consistentemente                     │
│                                                                      │
│  Diseño del benchmark:                                               │
│  □ do_not_optimize en outputs de interés                             │
│  □ do_not_optimize en inputs constantes                              │
│  □ clobber_memory donde hay escrituras                               │
│  □ Warmup suficiente (>100ms, preferible 1s+)                        │
│  □ Setup fuera del loop de medición (si aplica)                      │
│  □ Al menos 30 samples                                               │
│  □ Iteraciones por sample suficientes para >1ms total                │
│  □ Múltiples tamaños de input                                        │
│  □ Datos aleatorios (o variables) donde aplica                       │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│           CHECKLIST POSTERIOR A LA EJECUCIÓN                         │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Calidad de las mediciones:                                          │
│  □ CV < 5% (ideal) o documentado                                     │
│  □ IC 95% estrecho (media ± 2%)                                      │
│  □ < 10% de outliers                                                 │
│  □ No hay samples con latencia 10x+ mayores                          │
│  □ Resultados reproducibles entre ejecuciones                        │
│                                                                      │
│  Validación con perf:                                                │
│  □ Number of instructions ≠ 0                                        │
│  □ IPC razonable (>0.5)                                              │
│  □ Cache miss rate esperado (<10% para datos calientes)              │
│  □ Branch miss rate realista (1-5% para datos variables)             │
│  □ dTLB-load-misses < 1% del total                                   │
│                                                                      │
│  Sanity checks:                                                      │
│  □ Los números cambian con diferentes tamaños (confirmando           │
│    que sí estoy midiendo algo que depende del tamaño)                │
│  □ La escala con el tamaño es la esperada (O(n), O(n²), etc.)       │
│  □ El resultado es consistente con la teoría                         │
│  □ El orden de magnitud tiene sentido                                │
│                                                                      │
│  Reporte:                                                            │
│  □ Hardware especificado (CPU model, RAM, cache sizes)               │
│  □ Software especificado (OS, kernel, compilador)                    │
│  □ Flags de compilación listados                                     │
│  □ Comando exacto de ejecución                                       │
│  □ Metodología descrita                                              │
│  □ Media, mediana, stddev, IC 95%, n                                 │
│  □ Contexto de aplicación (cuándo usar esta optimización)            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 26. Cómo leer benchmarks de otros

Cuando encuentres un benchmark en un blog, README, o paper, aplica este filtro:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Red flags en benchmarks publicados                           │
│                                                                      │
│  🚩 No especifica hardware                                           │
│     "Este algoritmo procesa 1M elementos en 10ms"                    │
│     → ¿En qué CPU? ¿A qué frecuencia? ¿Con cuánto cache?            │
│                                                                      │
│  🚩 No especifica compilador ni flags                                │
│     "Nuestra versión C es 10x más rápida que Python"                 │
│     → ¿Con -O0 o -O3? ¿gcc o clang?                                  │
│                                                                      │
│  🚩 Único número por test                                            │
│     "Nuestra solución: 5.3ms"                                         │
│     → ¿Mediana? ¿Mejor caso? ¿Promedio? ¿Desviación estándar?       │
│                                                                      │
│  🚩 Comparaciones con baselines débiles                              │
│     "50x más rápido que Python sin NumPy"                            │
│     → La comparación justa sería con Python + NumPy                  │
│                                                                      │
│  🚩 Gráfica de barras sin error bars                                 │
│     → ¿Las diferencias son significativas estadísticamente?         │
│                                                                      │
│  🚩 Un solo tamaño de input                                          │
│     → La ventaja podría invertirse con tamaños diferentes            │
│                                                                      │
│  🚩 Datos demasiado "limpios"                                         │
│     "Array ordenado, sin duplicados, exactamente 1024 elementos"    │
│     → ¿Qué tan realista es ese input?                               │
│                                                                      │
│  🚩 Palabras exageradas                                              │
│     "blazing fast", "insanely fast", "10x faster"                    │
│     → Probablemente el autor se enamoró del resultado                │
│                                                                      │
│  🚩 No publica el código del benchmark                               │
│     → No se puede verificar ni reproducir                            │
│                                                                      │
│  ✅ Green flags (señales de un benchmark confiable):                 │
│     • Hardware y software especificados completamente                │
│     • Código del benchmark disponible                                │
│     • Múltiples tamaños de input                                     │
│     • Múltiples compiladores o runtimes                              │
│     • Reporte de varianza (stddev, IC)                               │
│     • Comparación con baselines razonables                           │
│     • Limitaciones y contexto explicados                             │
│     • Autor reconoce en qué casos NO funciona                        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 27. Programa de práctica: trap_detector

Este programa demuestra cada trampa y cómo detectarla. Al ejecutarlo, verás numéricamente cómo cada error sesga los resultados.

```c
// trap_detector.c — Demostración de las trampas comunes del benchmarking
//
// Compilar: gcc -O2 -march=native -o trap_detector trap_detector.c -lm
// Ejecutar: taskset -c 0 ./trap_detector

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>

#define do_not_optimize(val) __asm__ __volatile__("" : : "r,m"(val) : "memory")
#define clobber_memory()     __asm__ __volatile__("" : : : "memory")

static inline int64_t ts_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// ═══════════════════════════════════════════════════════════
// Función base a medir: fibonacci iterativo
// ═══════════════════════════════════════════════════════════
int fib(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        int t = a + b;
        a = b;
        b = t;
    }
    return b;
}

// ═══════════════════════════════════════════════════════════
// TRAMPA 1: Dead Code Elimination
// ═══════════════════════════════════════════════════════════
double trap_dce_broken(void) {
    const int ITERS = 10000000;
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        int r = fib(20);   // resultado ignorado
        (void)r;
    }
    int64_t elapsed = ts_ns() - start;
    return (double)elapsed / ITERS;
}

double trap_dce_fixed(void) {
    const int ITERS = 10000000;
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        int r = fib(20);
        do_not_optimize(r);
    }
    int64_t elapsed = ts_ns() - start;
    return (double)elapsed / ITERS;
}

// ═══════════════════════════════════════════════════════════
// TRAMPA 2: Constant folding
// ═══════════════════════════════════════════════════════════
double trap_cf_broken(void) {
    const int ITERS = 10000000;
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        int r = fib(20);   // 20 es constante conocida
        do_not_optimize(r);
    }
    int64_t elapsed = ts_ns() - start;
    return (double)elapsed / ITERS;
}

double trap_cf_fixed(void) {
    const int ITERS = 10000000;
    int n = 20;
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        do_not_optimize(n);   // compilador no puede asumir n=20
        int r = fib(n);
        do_not_optimize(r);
    }
    int64_t elapsed = ts_ns() - start;
    return (double)elapsed / ITERS;
}

// ═══════════════════════════════════════════════════════════
// TRAMPA 3: Branch predictor warming
// ═══════════════════════════════════════════════════════════

// Búsqueda lineal en un array
static int linear_search(const int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}

double trap_bp_broken(void) {
    const int N = 10000;
    int *arr = (int *)malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) arr[i] = i;
    
    const int ITERS = 100000;
    int target = 5000;  // siempre el mismo
    
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        int r = linear_search(arr, N, target);
        do_not_optimize(r);
    }
    int64_t elapsed = ts_ns() - start;
    
    free(arr);
    return (double)elapsed / ITERS;
}

double trap_bp_fixed(void) {
    const int N = 10000;
    int *arr = (int *)malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) arr[i] = i;
    
    const int ITERS = 100000;
    
    // Pre-generar 1000 targets aleatorios
    int targets[1000];
    srand(42);
    for (int i = 0; i < 1000; i++) {
        targets[i] = rand() % N;
    }
    
    int64_t start = ts_ns();
    for (int i = 0; i < ITERS; i++) {
        int target = targets[i % 1000];
        int r = linear_search(arr, N, target);
        do_not_optimize(r);
    }
    int64_t elapsed = ts_ns() - start;
    
    free(arr);
    return (double)elapsed / ITERS;
}

// ═══════════════════════════════════════════════════════════
// TRAMPA 4: Cache warming y tamaño de input
// ═══════════════════════════════════════════════════════════

double measure_sum_size(size_t n, int pollute_cache) {
    int *arr = (int *)malloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) arr[i] = (int)i;
    
    // Warmup
    long sum = 0;
    for (size_t i = 0; i < n; i++) sum += arr[i];
    do_not_optimize(sum);
    
    // Cache polluter (opcional)
    char *polluter = NULL;
    if (pollute_cache) {
        polluter = (char *)malloc(64 * 1024 * 1024); // 64 MB
    }
    
    int iters = 0;
    int64_t start = ts_ns();
    int64_t target = start + 200000000LL; // 200ms
    
    while (ts_ns() < target) {
        if (pollute_cache) {
            // Contaminar cache antes de cada iteración
            for (int i = 0; i < 64 * 1024 * 1024; i += 64) {
                polluter[i] = (char)i;
            }
            clobber_memory();
        }
        
        sum = 0;
        for (size_t i = 0; i < n; i++) sum += arr[i];
        do_not_optimize(sum);
        iters++;
    }
    
    int64_t elapsed = ts_ns() - start;
    double ns_per_elem = (double)elapsed / ((double)iters * n);
    
    if (polluter) free(polluter);
    free(arr);
    return ns_per_elem;
}

// ═══════════════════════════════════════════════════════════
// TRAMPA 5: LICM (Loop-invariant code motion)
// ═══════════════════════════════════════════════════════════

double trap_licm_broken(void) {
    const int N = 10000;
    int *arr = (int *)malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) arr[i] = i;
    
    const int ITERS = 100000;
    int target = 9999; // al final (peor caso)
    
    int64_t start = ts_ns();
    for (int iter = 0; iter < ITERS; iter++) {
        int found = 0;
        for (int j = 0; j < N; j++) {
            if (arr[j] == target) { found = j; break; }
        }
        do_not_optimize(found);
    }
    int64_t elapsed = ts_ns() - start;
    
    free(arr);
    return (double)elapsed / ITERS;
}

double trap_licm_fixed(void) {
    const int N = 10000;
    int *arr = (int *)malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) arr[i] = i;
    
    const int ITERS = 100000;
    int target = 9999;
    
    int64_t start = ts_ns();
    for (int iter = 0; iter < ITERS; iter++) {
        clobber_memory();  // invalidar suposiciones
        int found = 0;
        for (int j = 0; j < N; j++) {
            if (arr[j] == target) { found = j; break; }
        }
        do_not_optimize(found);
    }
    int64_t elapsed = ts_ns() - start;
    
    free(arr);
    return (double)elapsed / ITERS;
}

// ═══════════════════════════════════════════════════════════
// Main: ejecutar todas las demostraciones
// ═══════════════════════════════════════════════════════════
int main(void) {
    printf("===========================================\n");
    printf("  trap_detector: demostración de trampas\n");
    printf("===========================================\n\n");
    
    // ── Trampa 1: DCE ──────────────────────────────────
    printf("[TRAMPA 1] Dead Code Elimination\n");
    printf("─────────────────────────────────\n");
    double t1_broken = trap_dce_broken();
    double t1_fixed  = trap_dce_fixed();
    printf("  Sin do_not_optimize: %.2f ns/iter\n", t1_broken);
    printf("  Con do_not_optimize: %.2f ns/iter\n", t1_fixed);
    if (t1_fixed > t1_broken * 2) {
        printf("  ⚠ DCE detectado: la version 'rota' es %.1fx más rápida\n",
               t1_fixed / t1_broken);
        printf("  → El compilador eliminó el código\n");
    }
    printf("\n");
    
    // ── Trampa 2: Constant folding ─────────────────────
    printf("[TRAMPA 2] Constant Folding\n");
    printf("───────────────────────────\n");
    double t2_broken = trap_cf_broken();
    double t2_fixed  = trap_cf_fixed();
    printf("  fib(20) literal:        %.2f ns/iter\n", t2_broken);
    printf("  fib(n) con n variable:  %.2f ns/iter\n", t2_fixed);
    if (fabs(t2_fixed - t2_broken) > 1.0) {
        printf("  ℹ Diferencia de %.1f ns: posible constant folding\n",
               t2_fixed - t2_broken);
    }
    printf("\n");
    
    // ── Trampa 3: Branch predictor ─────────────────────
    printf("[TRAMPA 3] Branch Predictor Warming\n");
    printf("────────────────────────────────────\n");
    double t3_broken = trap_bp_broken();
    double t3_fixed  = trap_bp_fixed();
    printf("  Target fijo (5000):     %.2f ns/iter\n", t3_broken);
    printf("  Targets aleatorios:     %.2f ns/iter\n", t3_fixed);
    if (t3_fixed > t3_broken * 1.5) {
        printf("  ⚠ Branch predictor warming detectado\n");
        printf("  → Con targets variables es %.1fx más lento\n",
               t3_fixed / t3_broken);
    }
    printf("\n");
    
    // ── Trampa 4: Cache y tamaño ───────────────────────
    printf("[TRAMPA 4] Cache warming y tamaño de input\n");
    printf("──────────────────────────────────────────\n");
    printf("%-15s %-15s %-15s\n", "Size (KB)", "Warm cache", "Cold cache");
    
    size_t sizes[] = {4, 32, 256, 2048, 16384};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        size_t n = sizes[i] * 1024 / sizeof(int);
        double warm = measure_sum_size(n, 0);
        double cold = measure_sum_size(n, 1);
        printf("%-15zu %-15.3f %-15.3f\n", sizes[i], warm, cold);
    }
    printf("\n");
    printf("  ℹ Observa cómo la diferencia warm vs cold aumenta\n");
    printf("    con tamaños mayores (no caben en cache).\n\n");
    
    // ── Trampa 5: LICM ─────────────────────────────────
    printf("[TRAMPA 5] Loop-Invariant Code Motion\n");
    printf("──────────────────────────────────────\n");
    double t5_broken = trap_licm_broken();
    double t5_fixed  = trap_licm_fixed();
    printf("  Sin clobber_memory:     %.2f ns/iter\n", t5_broken);
    printf("  Con clobber_memory:     %.2f ns/iter\n", t5_fixed);
    if (t5_fixed > t5_broken * 10) {
        printf("  ⚠ LICM detectado: la versión sin clobber fue %.1fx más rápida\n",
               t5_fixed / t5_broken);
        printf("  → El compilador movió la búsqueda fuera del loop\n");
    }
    printf("\n");
    
    // ── Resumen ────────────────────────────────────────
    printf("===========================================\n");
    printf("  Resumen\n");
    printf("===========================================\n");
    printf("  Las mediciones \"rotas\" pueden parecer\n");
    printf("  mucho mejores por razones EQUIVOCADAS.\n");
    printf("  Usa siempre anti-DCE techniques y valida\n");
    printf("  con perf stat e inspección de assembly.\n");
    printf("\n");
    
    return 0;
}
```

### Salida esperada

```
===========================================
  trap_detector: demostración de trampas
===========================================

[TRAMPA 1] Dead Code Elimination
─────────────────────────────────
  Sin do_not_optimize: 0.00 ns/iter
  Con do_not_optimize: 7.20 ns/iter
  ⚠ DCE detectado: la version 'rota' es infx más rápida
  → El compilador eliminó el código

[TRAMPA 2] Constant Folding
───────────────────────────
  fib(20) literal:        7.20 ns/iter
  fib(n) con n variable:  7.23 ns/iter
  (Con fib(20) el compilador inlinea y ejecuta, sin mucha CF)

[TRAMPA 3] Branch Predictor Warming
────────────────────────────────────
  Target fijo (5000):     8350.40 ns/iter
  Targets aleatorios:     9120.30 ns/iter
  ⚠ Branch predictor warming detectado
  → Con targets variables es 1.1x más lento

[TRAMPA 4] Cache warming y tamaño de input
──────────────────────────────────────────
Size (KB)       Warm cache      Cold cache
4               0.150           0.180
32              0.152           0.190
256             0.280           0.350
2048            0.310           0.780
16384           0.680           1.850

  ℹ Observa cómo la diferencia warm vs cold aumenta
    con tamaños mayores (no caben en cache).

[TRAMPA 5] Loop-Invariant Code Motion
──────────────────────────────────────
  Sin clobber_memory:     15.20 ns/iter
  Con clobber_memory:     18340.80 ns/iter
  ⚠ LICM detectado: la versión sin clobber fue 1206.5x más rápida
  → El compilador movió la búsqueda fuera del loop
```

---

## 28. Ejercicios

### Ejercicio 1: cazar DCE con assembly

**Objetivo**: Identificar dead code elimination inspeccionando el assembly generado.

**Tareas**:

**a)** Escribe un benchmark de `sum_of_squares(1, 1000000)` que calcule Σ(i²) de 1 a 1,000,000, sin usar `do_not_optimize`.

**b)** Compila con `gcc -O2 -S -o bench.s bench.c`. Busca en bench.s si hay una llamada a `sum_of_squares` o si el valor aparece como constante literal.

**c)** Repite añadiendo `do_not_optimize(result)`. Compara el assembly generado. ¿Cuántas líneas de código máquina genera cada versión?

**d)** Verifica que los resultados numéricos concuerdan con la teoría:
   - Sin protección: ~0ns (el compilador eliminó todo)
   - Con protección: tiempo real

**e)** Experimenta con `sum_of_squares(1, N)` donde N viene de `argv[1]` (input runtime). ¿El compilador puede hacer constant folding ahora? ¿Necesitas `do_not_optimize`?

---

### Ejercicio 2: medir el efecto del branch predictor

**Objetivo**: Cuantificar el impacto del branch predictor en un algoritmo de ordenamiento adaptativo.

**Tareas**:

**a)** Implementa o usa `qsort` para ordenar un array de 100,000 enteros.

**b)** Ejecuta el benchmark dos veces:
   - Caso 1: array ordenado ascendente de 0 a 99999
   - Caso 2: array aleatorio
   
   Mide el tiempo de qsort en cada caso.

**c)** Ejecuta `perf stat -e branches,branch-misses ./bench` para cada caso. Compara el branch miss rate.

**d)** ¿Cuántas veces más rápido es el caso ordenado? ¿Qué porcentaje de esa mejora viene del branch predictor (vs algorítmico)?

**e)** Repite con array invertido (descendente). ¿El branch predictor lo detecta tan bien como el ordenado?

---

### Ejercicio 3: benchmark con múltiples tamaños de input

**Objetivo**: Construir un benchmark que muestra la curva de rendimiento vs tamaño, revelando los límites de cache.

**Tareas**:

**a)** Usa `bench.h` del T03 para medir `sum_array` con tamaños: 1KB, 4KB, 16KB, 32KB, 64KB, 128KB, 256KB, 512KB, 1MB, 2MB, 4MB, 8MB, 16MB, 32MB, 64MB, 128MB.

**b)** Para cada tamaño, reporta ns/elemento y GB/s de throughput.

**c)** Identifica los "escalones" en la gráfica. ¿A qué tamaños ocurren? Usa `lscpu` para ver los tamaños de L1, L2, L3 de tu CPU.

**d)** Calcula la diferencia entre el throughput en L1 y en DRAM. ¿Es consistente con las latencias teóricas (L1: ~1ns, DRAM: ~80ns)?

**e)** Reflexiona: si tu benchmark en producción usará un array de 10MB, ¿qué número del benchmark es el más representativo?

---

### Ejercicio 4: comparar benchmark vs perf real

**Objetivo**: Validar un microbenchmark contra el rendimiento real de la función en un programa más grande.

**Tareas**:

**a)** Escribe un microbenchmark de `strcmp` comparando dos strings de 1KB con un único byte diferente al final.

**b)** Escribe un programa que hace `grep` en un archivo de 100MB con 100,000 líneas, buscando un patrón (usa `strcmp` internamente). Mide el tiempo total con `time`.

**c)** Calcula: tiempo_total / número_de_comparaciones = ns_por_comparación_real.

**d)** Compara contra el microbenchmark. ¿Son del mismo orden de magnitud? Si son diferentes, ¿por cuánto? ¿Por qué?

**e)** Discute: ¿qué trampas del microbenchmark podrían explicar la diferencia?

---

## Navegación

- **Anterior**: [T03 - Benchmark en C](../T03_Benchmark_en_C/README.md)
- **Siguiente**: [C04/S02/T01 - perf en Linux](../../S02_Profiling_de_Aplicaciones/T01_perf_en_Linux/README.md)

---

> **Capítulo 4: Benchmarking y Profiling** — Sección 1: Microbenchmarking — Tópico 4 de 4
>
> **Fin de la Sección 1**: Microbenchmarking (4 tópicos completados)
