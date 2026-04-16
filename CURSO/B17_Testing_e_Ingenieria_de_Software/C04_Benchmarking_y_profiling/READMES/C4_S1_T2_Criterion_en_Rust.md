# T02 - Criterion en Rust: cargo bench, criterion::Criterion, black_box, groups, comparaciones, HTML reports

## Índice

1. [Qué es Criterion.rs](#1-qué-es-criterionrs)
2. [Por qué no usar #\[bench\] de nightly](#2-por-qué-no-usar-bench-de-nightly)
3. [Arquitectura interna de Criterion](#3-arquitectura-interna-de-criterion)
4. [Instalación y configuración](#4-instalación-y-configuración)
5. [Estructura de un proyecto con benchmarks](#5-estructura-de-un-proyecto-con-benchmarks)
6. [Tu primer benchmark con Criterion](#6-tu-primer-benchmark-con-criterion)
7. [El ciclo de medición de Criterion](#7-el-ciclo-de-medición-de-criterion)
8. [black_box: evitar dead code elimination](#8-black_box-evitar-dead-code-elimination)
9. [Configuración del benchmark: sample_size, warm_up_time, measurement_time](#9-configuración-del-benchmark-sample_size-warm_up_time-measurement_time)
10. [Benchmark con inputs parametrizados](#10-benchmark-con-inputs-parametrizados)
11. [Benchmark groups: comparar implementaciones](#11-benchmark-groups-comparar-implementaciones)
12. [Funciones de benchmark: bench_function vs bench_with_input](#12-funciones-de-benchmark-bench_function-vs-bench_with_input)
13. [Iteradores: bench_function con setup costoso](#13-iteradores-bench_function-con-setup-costoso)
14. [Criterion::iter_batched: control fino del setup](#14-criterioniter_batched-control-fino-del-setup)
15. [Throughput: medir bytes/segundo u operaciones/segundo](#15-throughput-medir-bytessegundo-u-operacionessegundo)
16. [Análisis estadístico que hace Criterion](#16-análisis-estadístico-que-hace-criterion)
17. [Detección de regresiones y mejoras](#17-detección-de-regresiones-y-mejoras)
18. [Baseline management: save y compare](#18-baseline-management-save-y-compare)
19. [HTML reports: interpretar los gráficos](#19-html-reports-interpretar-los-gráficos)
20. [Filtrar benchmarks: --bench y nombres](#20-filtrar-benchmarks---bench-y-nombres)
21. [Criterion vs Divan: alternativas modernas](#21-criterion-vs-divan-alternativas-modernas)
22. [Integración con cargo-criterion](#22-integración-con-cargo-criterion)
23. [Custom measurements y wall clock vs CPU time](#23-custom-measurements-y-wall-clock-vs-cpu-time)
24. [Benchmark de código async](#24-benchmark-de-código-async)
25. [Benchmark de código que usa allocations](#25-benchmark-de-código-que-usa-allocations)
26. [Errores comunes con Criterion](#26-errores-comunes-con-criterion)
27. [Comparativa: Criterion vs benchmark manual del T01](#27-comparativa-criterion-vs-benchmark-manual-del-t01)
28. [Programa de práctica: bench_collections](#28-programa-de-práctica-bench_collections)
29. [Ejercicios](#29-ejercicios)

---

## 1. Qué es Criterion.rs

Criterion.rs es un **framework de microbenchmarking estadístico** para Rust. Automatiza todo lo que tuvimos que implementar manualmente en el T01: warmup, iteraciones adaptativas, estadística robusta, detección de regresiones, y generación de reportes.

```
┌──────────────────────────────────────────────────────────────────────┐
│                      Criterion.rs                                    │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │                    ¿Qué resuelve?                            │     │
│  │                                                             │     │
│  │  Manual (T01)              Criterion                         │     │
│  │  ───────────               ─────────                         │     │
│  │  Escribir warmup           Automático                        │     │
│  │  Elegir iteraciones        Adaptativo                        │     │
│  │  Calcular estadísticas     Integrado                         │     │
│  │  Evitar DCE manualmente    black_box integrado               │     │
│  │  Comparar A vs B           Groups + regression detection     │     │
│  │  Formatear resultados      HTML reports + consola             │     │
│  │  Guardar historial         Baselines automáticas              │     │
│  │  Detectar regresiones      Análisis de cambio con IC          │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                                                                      │
│  Autor: Brook Heisler (bheisler)                                     │
│  Repo: https://github.com/bheisler/criterion.rs                     │
│  Basado en: Haskell Criterion (Bryan O'Sullivan)                     │
│  Versión actual: criterion 0.5.x                                     │
└──────────────────────────────────────────────────────────────────────┘
```

Criterion.rs es el estándar de facto para benchmarks en Rust. Funciona en **stable Rust** (a diferencia de `#[bench]`), produce estadísticas rigurosas, y mantiene un historial que permite detectar regresiones automáticamente entre ejecuciones.

---

## 2. Por qué no usar #[bench] de nightly

Rust tiene un sistema de benchmarking integrado: `#[bench]`, parte de `libtest`. Pero tiene problemas serios:

```
┌──────────────────────────────────────────────────────────────────────┐
│          #[bench] (libtest) vs Criterion.rs                          │
│                                                                      │
│  Característica          #[bench]            Criterion               │
│  ─────────────           ────────            ─────────               │
│  Canal de Rust           Nightly only        Stable                  │
│  Estabilización          Años sin progreso   Actualizado             │
│  Estadísticas            ns/iter ± stddev    Media, mediana, IC,     │
│                          (poco informativo)  slopes, outliers        │
│  Warmup                  Mínimo              Configurable            │
│  Iteraciones             Fijas (heurística)  Adaptativo + lineal     │
│  Detección de cambios    Manual              Automático              │
│  Reportes                Solo texto          HTML + plots            │
│  Comparaciones           No soporta          Groups nativo           │
│  Throughput              No soporta          Bytes/s, Elems/s        │
│  Baselines               No soporta          Save/compare            │
│  Parametrización         Manual              BenchmarkId              │
└──────────────────────────────────────────────────────────────────────┘
```

```rust
// #[bench] de libtest — requiere nightly
#![feature(test)]
extern crate test;

#[bench]
fn bench_sort(b: &mut test::Bencher) {
    b.iter(|| {
        let mut v = vec![5, 3, 1, 4, 2];
        v.sort();
        test::black_box(v) // necesario para evitar DCE
    });
}
// Resultado: test bench_sort ... bench:      123 ns/iter (+/- 15)
// Solo eso. Sin historial, sin gráficos, sin análisis.
```

El número `+/- 15` de `#[bench]` es la desviación estándar del estimador, no un intervalo de confianza, y no distingue entre ruido estadístico y cambios reales de rendimiento. Criterion resuelve todos estos problemas.

---

## 3. Arquitectura interna de Criterion

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Arquitectura de Criterion.rs                        │
│                                                                      │
│  cargo bench --bench my_bench                                        │
│       │                                                              │
│       ▼                                                              │
│  ┌──────────────────────────────────────────┐                        │
│  │           Criterion Runner               │                        │
│  │                                          │                        │
│  │  1. Warmup adaptativo                    │                        │
│  │     - Ejecuta función en bucle           │                        │
│  │     - Mide hasta alcanzar warm_up_time   │                        │
│  │     - Determina iteraciones/muestra      │                        │
│  │                                          │                        │
│  │  2. Muestreo lineal                      │                        │
│  │     - N muestras (default: 100)          │                        │
│  │     - Muestra k: ejecuta k*iters veces   │                        │
│  │     - Mide tiempo total de cada muestra  │                        │
│  │                                          │                        │
│  │  3. Análisis estadístico                 │                        │
│  │     - Linear regression (OLS)            │                        │
│  │     - Bootstrap resampling (100,000x)    │                        │
│  │     - Outlier classification             │                        │
│  │     - Change detection vs baseline       │                        │
│  │                                          │                        │
│  │  4. Reporting                            │                        │
│  │     - Console output                     │                        │
│  │     - HTML + SVG plots                   │                        │
│  │     - JSON raw data                      │                        │
│  └──────────────────────────────────────────┘                        │
│       │                                                              │
│       ▼                                                              │
│  target/criterion/                                                   │
│  ├── my_benchmark/                                                   │
│  │   ├── base/                   ← Baseline anterior                 │
│  │   ├── new/                    ← Ejecución actual                  │
│  │   ├── change/                 ← Análisis de cambio                │
│  │   └── report/                 ← HTML + SVG                        │
│  └── report/                     ← Índice general                    │
└──────────────────────────────────────────────────────────────────────┘
```

El enfoque de **muestreo lineal** es clave. En lugar de ejecutar N iteraciones fijas y medir el tiempo, Criterion ejecuta muestras con **cantidades crecientes de iteraciones** (1×base, 2×base, 3×base, ..., N×base) y luego usa **regresión lineal** para estimar el tiempo por iteración. Esto elimina el sesgo de overhead constante (tiempo de setup del timer, llamada a la función de medición, etc.).

```
Muestreo lineal de Criterion:

Tiempo │
  (ns) │                                          ●
       │                                     ●
       │                                ●
       │                           ●
       │                      ●
       │                 ●
       │            ●                    pendiente = tiempo/iteración
       │       ●                         intercepto = overhead constante
       │  ●
       │●
       └───────────────────────────────────── Iteraciones
         1x   2x   3x   4x   5x  ...  100x

    slope = Σ((xi - x̄)(yi - ȳ)) / Σ((xi - x̄)²)
    
    El slope ES el tiempo por iteración, libre del overhead constante.
```

---

## 4. Instalación y configuración

### Cargo.toml

```toml
[package]
name = "my_project"
version = "0.1.0"
edition = "2021"

# Dependencia de dev para benchmarks
[dev-dependencies]
criterion = { version = "0.5", features = ["html_reports"] }

# Declarar el archivo de benchmark
[[bench]]
name = "my_benchmark"
harness = false  # CRÍTICO: desactiva el harness de libtest
```

La línea `harness = false` es **obligatoria**. Sin ella, cargo intenta usar el framework `#[bench]` de libtest, y los benchmarks de Criterion no compilarán.

### Features disponibles

```toml
# Todas las features de criterion 0.5:
criterion = { version = "0.5", features = [
    "html_reports",      # Genera reportes HTML con gráficos SVG
    "csv_output",        # Exporta datos en CSV
    # "plotters" es el backend de gráficos por defecto (incluido)
    # "cargo_bench_support" incluido por defecto
] }
```

### Estructura de archivos

```
my_project/
├── Cargo.toml
├── src/
│   └── lib.rs           ← Código a benchmarkear
├── benches/
│   ├── my_benchmark.rs  ← Archivo de benchmark
│   └── another.rs       ← Otro archivo de benchmark
└── target/
    └── criterion/       ← Resultados (generado automáticamente)
        └── reports/
```

El directorio `benches/` es la ubicación estándar. Cada archivo `.rs` en `benches/` puede ser un target de benchmark independiente, declarado en `Cargo.toml` con `[[bench]]`.

---

## 5. Estructura de un proyecto con benchmarks

Veamos un proyecto realista con múltiples benchmarks:

```toml
# Cargo.toml

[package]
name = "data_processor"
version = "0.1.0"
edition = "2021"

[dependencies]
# ... dependencias del proyecto

[dev-dependencies]
criterion = { version = "0.5", features = ["html_reports"] }
rand = "0.8"  # Para generar datos de test

# Benchmark de operaciones de sorting
[[bench]]
name = "sorting"
harness = false

# Benchmark de operaciones de búsqueda
[[bench]]
name = "searching"
harness = false

# Benchmark de serialización
[[bench]]
name = "serialization"
harness = false
```

```
data_processor/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── sorting.rs
│   ├── searching.rs
│   └── serialization.rs
├── benches/
│   ├── sorting.rs          ← cargo bench --bench sorting
│   ├── searching.rs        ← cargo bench --bench searching
│   └── serialization.rs    ← cargo bench --bench serialization
├── tests/
│   └── integration.rs
└── target/
    └── criterion/
        ├── sorting/
        ├── searching/
        ├── serialization/
        └── report/
            └── index.html  ← Índice de todos los benchmarks
```

Cada `[[bench]]` define un binary target independiente. Se ejecutan por separado:

```bash
# Ejecutar todos los benchmarks
cargo bench

# Ejecutar solo uno
cargo bench --bench sorting

# Filtrar por nombre del benchmark dentro del archivo
cargo bench --bench sorting -- "quicksort"

# Solo compilar (verificar que compila, sin ejecutar)
cargo bench --no-run
```

---

## 6. Tu primer benchmark con Criterion

Empecemos con algo simple. Tenemos una función que queremos medir:

```rust
// src/lib.rs

/// Calcula la suma de los primeros n números de Fibonacci
pub fn fibonacci(n: u64) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => {
            let mut a: u64 = 0;
            let mut b: u64 = 1;
            for _ in 2..=n {
                let temp = a + b;
                a = b;
                b = temp;
            }
            b
        }
    }
}

/// Versión recursiva (ineficiente a propósito)
pub fn fibonacci_recursive(n: u64) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2),
    }
}
```

```rust
// benches/fibonacci.rs

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use data_processor::{fibonacci, fibonacci_recursive};

fn bench_fibonacci(c: &mut Criterion) {
    c.bench_function("fibonacci_iterative_20", |b| {
        b.iter(|| fibonacci(black_box(20)))
    });
}

fn bench_fibonacci_recursive(c: &mut Criterion) {
    c.bench_function("fibonacci_recursive_20", |b| {
        b.iter(|| fibonacci_recursive(black_box(20)))
    });
}

criterion_group!(benches, bench_fibonacci, bench_fibonacci_recursive);
criterion_main!(benches);
```

### Desglose línea por línea

```rust
use criterion::{
    black_box,         // Función para evitar dead code elimination
    criterion_group,   // Macro para agrupar funciones de benchmark
    criterion_main,    // Macro que genera fn main()
    Criterion,         // Struct principal del framework
};

// Cada función de benchmark recibe &mut Criterion
fn bench_fibonacci(c: &mut Criterion) {
    // bench_function(nombre, closure)
    // - nombre: identificador en reportes
    // - closure: recibe un Bencher
    c.bench_function("fibonacci_iterative_20", |b| {
        // b.iter(closure) ejecuta el closure N veces
        // N es determinado automáticamente por Criterion
        b.iter(|| {
            // black_box(20) evita que el compilador
            // propague el 20 como constante y
            // precompute fibonacci(20) en compilación
            fibonacci(black_box(20))
            // El valor de retorno pasa implícitamente
            // por black_box también (en criterion 0.5)
        })
    });
}

// criterion_group! crea una estructura que agrupa benchmarks
// Nombre del grupo: benches
// Funciones incluidas: bench_fibonacci, bench_fibonacci_recursive
criterion_group!(benches, bench_fibonacci, bench_fibonacci_recursive);

// criterion_main! genera fn main() que:
// 1. Parsea argumentos de línea de comandos
// 2. Ejecuta todos los grupos listados
// 3. Genera reportes
criterion_main!(benches);
```

### Ejecución y salida

```bash
$ cargo bench --bench fibonacci
   Compiling data_processor v0.1.0
    Finished `bench` profile [optimized] target(s)
     Running benches/fibonacci.rs

fibonacci_iterative_20  time:   [7.2031 ns 7.2187 ns 7.2380 ns]
                        change: [-0.5765% -0.1702% +0.2345%] (p = 0.42 > 0.05)
                        No change in performance detected.
Found 3 outliers among 100 measurements (3.00%)
  1 (1.00%) high mild
  2 (2.00%) high severe

fibonacci_recursive_20  time:   [24.127 µs 24.189 µs 24.258 µs]
                        change: [+0.0219% +0.3755% +0.7117%] (p = 0.04 < 0.05)
                        Change within noise threshold.
Found 5 outliers among 100 measurements (5.00%)
  3 (3.00%) high mild
  2 (2.00%) high severe
```

### Interpretar la salida

```
fibonacci_iterative_20  time:   [7.2031 ns  7.2187 ns  7.2380 ns]
                                 ─────────  ─────────  ─────────
                                 límite      estimador  límite
                                 inferior    central    superior
                                 IC 95%      (media)    IC 95%

                        change: [-0.5765%   -0.1702%   +0.2345%]
                                 ─────────  ─────────  ──────────
                                 cambio      cambio     cambio
                                 mínimo      estimado   máximo
                                 (IC 95%)    vs base    (IC 95%)

                        (p = 0.42 > 0.05)
                         ─────────────────
                         p-value del test de hipótesis
                         p > 0.05 → no hay cambio significativo

Found 3 outliers among 100 measurements (3.00%)
  1 (1.00%) high mild      ← Ligeramente por encima de lo esperado
  2 (2.00%) high severe    ← Muy por encima (posible interferencia del OS)
```

La primera vez que ejecutas un benchmark, no habrá línea `change:` porque no existe baseline previa.

---

## 7. El ciclo de medición de Criterion

Criterion no mide como un simple bucle. Su algoritmo es más sofisticado:

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Ciclo de medición de Criterion                      │
│                                                                      │
│  Fase 1: WARMUP (configurable, default 3 segundos)                   │
│  ────────────────────────────────────────────────────                 │
│  - Ejecuta la función repetidamente                                  │
│  - Mide cuánto tarda cada iteración                                  │
│  - Objetivo: estabilizar caches, branch predictor, CPU frequency     │
│  - Resultado: estima iteraciones_por_muestra                         │
│                                                                      │
│  Fase 2: MUESTREO LINEAL (configurable, default 5 segundos)         │
│  ────────────────────────────────────────────────────                 │
│  - Toma sample_size muestras (default: 100)                          │
│  - Muestra 1: ejecuta 1 × iters_por_muestra                         │
│  - Muestra 2: ejecuta 2 × iters_por_muestra                         │
│  - ...                                                               │
│  - Muestra N: ejecuta N × iters_por_muestra                         │
│  - Para cada muestra, mide el tiempo total                           │
│                                                                      │
│  Fase 3: ANÁLISIS ESTADÍSTICO                                        │
│  ────────────────────────────────────────────────────                 │
│  - Regresión lineal OLS sobre (iteraciones, tiempo)                  │
│  - Bootstrap resampling (100,000 iteraciones)                        │
│  - Clasificación de outliers                                         │
│  - Si existe baseline: test de cambio                                │
│                                                                      │
│  Fase 4: REPORTING                                                    │
│  ────────────────────────────────────────────────────                 │
│  - Imprime resultados en consola                                     │
│  - Genera HTML + SVG si feature activada                             │
│  - Guarda datos raw en JSON                                          │
│  - Actualiza baseline                                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Muestreo lineal en detalle

```
Ejemplo con sample_size=5, iters_per_sample=1000:

Muestra 1: ejecuta  1,000 iteraciones → mide T₁ total
Muestra 2: ejecuta  2,000 iteraciones → mide T₂ total
Muestra 3: ejecuta  3,000 iteraciones → mide T₃ total
Muestra 4: ejecuta  4,000 iteraciones → mide T₄ total
Muestra 5: ejecuta  5,000 iteraciones → mide T₅ total

Total iteraciones: 1000 + 2000 + 3000 + 4000 + 5000 = 15,000

Regresión lineal:
  Tᵢ = slope × iteraciones_i + intercepto
  
  slope = tiempo por iteración (lo que queremos)
  intercepto = overhead constante (lo que descartamos)

Con 100 muestras (default), se ejecutan 100×(100+1)/2 × iters = 5,050 × iters
```

¿Por qué muestreo lineal y no simplemente medir N veces? Porque el **intercepto absorbe el overhead constante** (llamar al timer, entrar/salir del bucle, etc.), dando una estimación más pura del tiempo por iteración.

---

## 8. black_box: evitar dead code elimination

En el T01 vimos por qué el compilador elimina código que considera sin efecto. En Criterion, `black_box` es la herramienta estándar:

```rust
use criterion::black_box;

// ❌ INCORRECTO: el compilador puede eliminar todo
c.bench_function("bad", |b| {
    b.iter(|| {
        fibonacci(20)  // resultado no usado → puede eliminarse
    })
});

// ✅ CORRECTO: black_box en el input
c.bench_function("good_input", |b| {
    b.iter(|| {
        fibonacci(black_box(20))
    })
});

// ✅ CORRECTO: black_box en el output
c.bench_function("good_output", |b| {
    b.iter(|| {
        black_box(fibonacci(20))
    })
});

// ✅ MÁS SEGURO: black_box en ambos
c.bench_function("safest", |b| {
    b.iter(|| {
        black_box(fibonacci(black_box(20)))
    })
});
```

### Qué hace black_box internamente

```rust
// Implementación conceptual de black_box (simplificada):
pub fn black_box<T>(dummy: T) -> T {
    // En la implementación real, usa inline assembly:
    // asm!("" : : "r"(&dummy) : "memory")
    //
    // Esto le dice al compilador:
    // 1. "r"(&dummy): el valor está en un registro (lo fuerza a existir)
    // 2. "memory": la memoria podría haber cambiado (invalida suposiciones)
    //
    // Resultado: el compilador NO puede:
    //   - Eliminar el cómputo de dummy (podría ser leído)
    //   - Propagar constantes a través del black_box
    //   - Mover código fuera del bucle de medición
    dummy
}
```

### Cuándo usar black_box

```
┌──────────────────────────────────────────────────────────────────────┐
│               Reglas para black_box                                   │
│                                                                      │
│  Caso                                Acción                          │
│  ────                                ──────                          │
│  Input constante conocido            black_box(input)                │
│  en compilación                      evita constant folding          │
│                                                                      │
│  Resultado no usado                  black_box(resultado)            │
│                                      evita dead code elimination     │
│                                                                      │
│  Función pura con input              black_box en input Y output     │
│  constante                           máxima seguridad                │
│                                                                      │
│  Input viene de datos                No necesitas black_box          │
│  dinámicos (vector, archivo)         el compilador no puede          │
│                                      predecir el valor               │
│                                                                      │
│  Efecto secundario (I/O,             No necesitas black_box          │
│  escritura a memoria)                el compilador no puede           │
│                                      eliminar el efecto              │
│                                                                      │
│  Resultado intermedio usado          No necesitas black_box          │
│  por siguiente cómputo               ya hay dependencia de datos     │
└──────────────────────────────────────────────────────────────────────┘
```

```rust
// Ejemplo: no necesitas black_box si el resultado se usa
c.bench_function("no_bb_needed", |b| {
    let mut data = vec![0u64; 1000];
    b.iter(|| {
        for i in 0..data.len() {
            data[i] = data[i].wrapping_add(1); // escritura a memoria = efecto
        }
    })
});

// Ejemplo: SÍ necesitas black_box
c.bench_function("bb_needed", |b| {
    let data = vec![0u64; 1000];
    b.iter(|| {
        let sum: u64 = data.iter().sum(); // resultado no usado
        black_box(sum) // forzar que el cómputo exista
    })
});
```

---

## 9. Configuración del benchmark: sample_size, warm_up_time, measurement_time

Criterion tiene valores por defecto razonables, pero puedes ajustarlos:

```rust
use criterion::{criterion_group, criterion_main, Criterion};
use std::time::Duration;

fn bench_configured(c: &mut Criterion) {
    // Configuración a nivel de grupo
    let mut group = c.benchmark_group("configured_group");
    
    // Número de muestras (default: 100)
    // Más muestras = más precisión, más tiempo
    group.sample_size(200);
    
    // Tiempo de warmup (default: 3 segundos)
    group.warm_up_time(Duration::from_secs(5));
    
    // Tiempo de medición (default: 5 segundos)
    // Criterion ajusta iteraciones para llenar este tiempo
    group.measurement_time(Duration::from_secs(10));
    
    // Nivel de confianza (default: 0.95)
    group.confidence_level(0.99);
    
    // Nivel de significancia para detección de cambios (default: 0.05)
    group.significance_level(0.01);
    
    // Umbral de ruido: cambios menores a este % se consideran ruido
    group.noise_threshold(0.03); // 3% (default: 0.01 = 1%)
    
    group.bench_function("my_function", |b| {
        b.iter(|| {
            // ... código a medir
        })
    });
    
    group.finish(); // IMPORTANTE: siempre llamar finish()
}

// Alternativa: configurar globalmente con criterion_group!
fn custom_criterion() -> Criterion {
    Criterion::default()
        .sample_size(50)             // Menos muestras (rápido)
        .warm_up_time(Duration::from_secs(1))
        .measurement_time(Duration::from_secs(3))
        .with_plots()                // Asegurar generación de plots
}

criterion_group! {
    name = benches;
    config = custom_criterion();
    targets = bench_configured
}
criterion_main!(benches);
```

### Guía para elegir parámetros

```
┌──────────────────────────────────────────────────────────────────────┐
│            Guía de configuración de Criterion                        │
│                                                                      │
│  Escenario                      sample_size  warmup  measurement     │
│  ────────                       ───────────  ──────  ───────────     │
│  Desarrollo rápido              10-20        1s      2s              │
│  (solo quiero un número)                                             │
│                                                                      │
│  Desarrollo normal              50           3s      5s              │
│  (valores por defecto ≈ OK)                                          │
│                                                                      │
│  CI/CD (balance velocidad       50-100       3s      5s              │
│  vs precisión)                                                       │
│                                                                      │
│  Publicación / decisión         200-500      5s      10-30s          │
│  de optimización                                                     │
│                                                                      │
│  Función muy rápida (<10ns)     200+         5s      10s+            │
│  (necesita muchas muestras                                           │
│  para reducir ruido)                                                 │
│                                                                      │
│  Función lenta (>100ms)         10-20        1s      30s-60s         │
│  (cada muestra tarda mucho,                                          │
│  reducir sample_size)                                                │
│                                                                      │
│  ⚠ MÍNIMO sample_size: 10 (Criterion rechaza menos)                 │
│  ⚠ measurement_time debe ser suficiente para al menos               │
│    sample_size iteraciones de la función                              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 10. Benchmark con inputs parametrizados

Medir una función con diferentes tamaños de input es esencial para entender su complejidad:

```rust
use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};

fn sort_vec(data: &mut Vec<u32>) {
    data.sort();
}

fn bench_sort_sizes(c: &mut Criterion) {
    let mut group = c.benchmark_group("sort");
    
    // Probar con diferentes tamaños
    for size in [10, 100, 1_000, 10_000, 100_000].iter() {
        group.bench_with_input(
            BenchmarkId::new("std_sort", size),  // nombre parametrizado
            size,                                  // el input
            |b, &size| {
                // Este closure recibe el Bencher y el input
                b.iter(|| {
                    let mut data: Vec<u32> = (0..size).rev().collect();
                    sort_vec(black_box(&mut data));
                })
            },
        );
    }
    
    group.finish();
}

criterion_group!(benches, bench_sort_sizes);
criterion_main!(benches);
```

### Salida con inputs parametrizados

```
sort/std_sort/10        time:   [98.123 ns  98.456 ns  98.834 ns]
sort/std_sort/100       time:   [1.8234 µs  1.8289 µs  1.8351 µs]
sort/std_sort/1000      time:   [28.234 µs  28.345 µs  28.467 µs]
sort/std_sort/10000     time:   [412.34 µs  413.45 µs  414.78 µs]
sort/std_sort/100000    time:   [5.6234 ms  5.6345 ms  5.6467 ms]
```

### BenchmarkId: anatomía del nombre

```rust
// BenchmarkId crea nombres jerárquicos para los reportes
BenchmarkId::new("algorithm_name", parameter)

// Ejemplos:
BenchmarkId::new("quicksort", 1000)     // → "quicksort/1000"
BenchmarkId::new("mergesort", 1000)     // → "mergesort/1000"
BenchmarkId::from_parameter(1000)       // → "1000" (sin nombre de función)

// En los reportes HTML, el parámetro se usa como eje X del gráfico
// Esto genera gráficos automáticos de rendimiento vs tamaño de input
```

### Inputs complejos

```rust
// El parámetro puede ser cualquier tipo que implemente Display + Debug
fn bench_with_complex_params(c: &mut Criterion) {
    let mut group = c.benchmark_group("matrix_multiply");
    
    // Input: (filas, columnas)
    let sizes = [(8, 8), (16, 16), (32, 32), (64, 64), (128, 128)];
    
    for (rows, cols) in sizes.iter() {
        // Para inputs complejos, crear un string descriptivo
        let param = format!("{}x{}", rows, cols);
        
        group.bench_with_input(
            BenchmarkId::new("naive", &param),
            &(*rows, *cols),
            |b, &(rows, cols)| {
                b.iter(|| {
                    let a = vec![0.0f64; rows * cols];
                    let b_mat = vec![0.0f64; rows * cols];
                    let mut c_mat = vec![0.0f64; rows * cols];
                    // ... multiplicación
                    black_box(&mut c_mat);
                })
            },
        );
    }
    
    group.finish();
}
```

---

## 11. Benchmark groups: comparar implementaciones

Los grupos de benchmark son la herramienta más poderosa de Criterion para comparaciones:

```rust
use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};

// Tres implementaciones de búsqueda
fn linear_search(data: &[u32], target: u32) -> Option<usize> {
    data.iter().position(|&x| x == target)
}

fn binary_search(data: &[u32], target: u32) -> Option<usize> {
    data.binary_search(&target).ok()
}

fn hash_lookup(map: &std::collections::HashMap<u32, usize>, target: u32) -> Option<usize> {
    map.get(&target).copied()
}

fn bench_search_comparison(c: &mut Criterion) {
    let mut group = c.benchmark_group("search_comparison");
    
    for size in [100, 1_000, 10_000, 100_000].iter() {
        let data: Vec<u32> = (0..*size).collect();
        let target = size / 2; // buscar el elemento del medio
        
        let map: std::collections::HashMap<u32, usize> = 
            data.iter().enumerate().map(|(i, &v)| (v, i)).collect();
        
        // Misma familia de parámetros → misma gráfica comparativa
        group.bench_with_input(
            BenchmarkId::new("linear", size),
            &size,
            |b, _| b.iter(|| linear_search(black_box(&data), black_box(target))),
        );
        
        group.bench_with_input(
            BenchmarkId::new("binary", size),
            &size,
            |b, _| b.iter(|| binary_search(black_box(&data), black_box(target))),
        );
        
        group.bench_with_input(
            BenchmarkId::new("hash", size),
            &size,
            |b, _| b.iter(|| hash_lookup(black_box(&map), black_box(target))),
        );
    }
    
    group.finish();
}

criterion_group!(benches, bench_search_comparison);
criterion_main!(benches);
```

### Salida comparativa

```
search_comparison/linear/100      time:   [45.234 ns  45.567 ns  45.923 ns]
search_comparison/binary/100      time:   [12.345 ns  12.456 ns  12.578 ns]
search_comparison/hash/100        time:   [18.234 ns  18.345 ns  18.467 ns]

search_comparison/linear/1000     time:   [423.45 ns  425.67 ns  428.23 ns]
search_comparison/binary/1000     time:   [15.234 ns  15.345 ns  15.467 ns]
search_comparison/hash/1000       time:   [18.456 ns  18.567 ns  18.689 ns]

search_comparison/linear/10000    time:   [4.2345 µs  4.2567 µs  4.2789 µs]
search_comparison/binary/10000    time:   [18.234 ns  18.345 ns  18.467 ns]
search_comparison/hash/10000      time:   [18.789 ns  18.890 ns  19.012 ns]

search_comparison/linear/100000   time:   [42.345 µs  42.567 µs  42.789 µs]
search_comparison/binary/100000   time:   [21.234 ns  21.345 ns  21.467 ns]
search_comparison/hash/100000     time:   [19.123 ns  19.234 ns  19.345 ns]
```

```
Visualización generada por Criterion (HTML report):

Tiempo │
  (ns) │  ●
       │   \                    linear: O(n)
       │    \
  1000 │     \
       │      \
       │       ●──────────────────────────────────●  linear
       │
   100 │
       │
    20 │  ●────────────────────────────────────●  hash: O(1)
    15 │  ●──────────────●─────────────────────●  binary: O(log n)
       └──────────────────────────────────────────── Tamaño
         100     1,000    10,000    100,000
```

El reporte HTML genera automáticamente gráficas con **violin plots** por implementación y **line charts** de rendimiento vs parámetro.

---

## 12. Funciones de benchmark: bench_function vs bench_with_input

Criterion ofrece varias formas de definir qué medir:

```rust
use criterion::{black_box, Criterion, BenchmarkId, BatchSize};

fn various_bench_styles(c: &mut Criterion) {
    // ══════════════════════════════════════════════════════════════
    // Estilo 1: bench_function — el más simple
    // ══════════════════════════════════════════════════════════════
    // Ideal para: función sin parámetros variables
    c.bench_function("simple_function", |b| {
        b.iter(|| {
            let mut sum = 0u64;
            for i in 0..1000 {
                sum += black_box(i);
            }
            sum
        })
    });
    
    // ══════════════════════════════════════════════════════════════
    // Estilo 2: bench_with_input — con parámetro
    // ══════════════════════════════════════════════════════════════
    // Ideal para: comparar la misma función con diferentes inputs
    let mut group = c.benchmark_group("with_input");
    for size in [100, 1000, 10000] {
        group.bench_with_input(
            BenchmarkId::from_parameter(size),
            &size,
            |b, &size| {
                let data: Vec<u64> = (0..size).collect();
                b.iter(|| {
                    data.iter().sum::<u64>()
                })
            },
        );
    }
    group.finish();
    
    // ══════════════════════════════════════════════════════════════
    // Estilo 3: iter_with_setup — setup por iteración
    // ══════════════════════════════════════════════════════════════
    // Ideal para: funciones que consumen su input (sort, drain, etc.)
    c.bench_function("sort_consuming", |b| {
        b.iter_batched(
            || (0..1000).rev().collect::<Vec<u32>>(), // setup: crea vector
            |mut data| data.sort(),                    // routine: lo ordena
            BatchSize::SmallInput,                     // hint de tamaño
        )
    });
    
    // ══════════════════════════════════════════════════════════════
    // Estilo 4: iter_custom — control total del loop
    // ══════════════════════════════════════════════════════════════
    // Ideal para: mediciones que necesitan setup/teardown especial
    c.bench_function("custom_loop", |b| {
        b.iter_custom(|iters| {
            let start = std::time::Instant::now();
            for _i in 0..iters {
                // Hacer algo
                black_box(fibonacci(black_box(20)));
            }
            start.elapsed()
        })
    });
}

fn fibonacci(n: u64) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => {
            let (mut a, mut b) = (0u64, 1u64);
            for _ in 2..=n { let t = a + b; a = b; b = t; }
            b
        }
    }
}
```

### Cuándo usar cada estilo

```
┌──────────────────────────────────────────────────────────────────────┐
│              Guía de selección de estilo de benchmark                 │
│                                                                      │
│  Método              Cuándo usarlo                    Setup          │
│  ──────              ─────────────                    ─────          │
│  b.iter(||)          Función pura que no              Una vez        │
│                      modifica su input                               │
│                                                                      │
│  bench_with_input    Comparar la misma función        Una vez        │
│                      con diferentes parámetros        por param      │
│                                                                      │
│  b.iter_batched      Función que consume/muta         Cada           │
│                      su input (sort, drain)           iteración      │
│                                                                      │
│  b.iter_with_setup   Similar a iter_batched           Cada           │
│  (deprecated 0.5)    pero menos control               iteración      │
│                                                                      │
│  b.iter_custom       Necesitas controlar el           Manual         │
│                      loop de medición tú mismo                       │
│                      (e.g., medir solo parte                         │
│                      del código, excluir setup)                      │
│                                                                      │
│  b.iter_batched_ref  Como iter_batched pero           Cada           │
│                      pasa &mut en vez de              iteración      │
│                      mover el valor                                  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 13. Iteradores: bench_function con setup costoso

Un problema frecuente: el setup tarda más que lo que quieres medir.

```rust
use criterion::{black_box, criterion_group, criterion_main, Criterion, BatchSize};

fn bench_with_expensive_setup(c: &mut Criterion) {
    // ══════════════════════════════════════════════════════
    // ❌ INCORRECTO: el setup se mide junto con la función
    // ══════════════════════════════════════════════════════
    c.bench_function("wrong_includes_setup", |b| {
        b.iter(|| {
            // Este setup se repite y se mide cada vez
            let data: Vec<u32> = (0..100_000).rev().collect();
            data.iter().sum::<u32>()
        })
    });
    // El tiempo reportado incluye crear el vector (costoso)
    // MÁS sumar sus elementos (lo que queremos medir)
    
    // ══════════════════════════════════════════════════════
    // ✅ CORRECTO opción A: setup fuera del iter()
    // ══════════════════════════════════════════════════════
    // Funciona si la función NO modifica el input
    c.bench_function("correct_setup_outside", |b| {
        let data: Vec<u32> = (0..100_000).rev().collect();
        b.iter(|| {
            black_box(data.iter().sum::<u32>())
        })
    });
    // El vector se crea UNA VEZ antes de empezar a medir
    
    // ══════════════════════════════════════════════════════
    // ✅ CORRECTO opción B: iter_batched para setup repetido
    // ══════════════════════════════════════════════════════
    // Necesario si la función CONSUME o MODIFICA el input
    c.bench_function("correct_batched", |b| {
        b.iter_batched(
            // Setup: se ejecuta antes de cada medición (NO se mide)
            || (0..100_000).rev().collect::<Vec<u32>>(),
            // Routine: solo esto se mide
            |mut data| {
                data.sort();
                data
            },
            // Hint: indica cuánta memoria usa el setup
            BatchSize::LargeInput,
        )
    });
}

criterion_group!(benches, bench_with_expensive_setup);
criterion_main!(benches);
```

### BatchSize: control de batching

```rust
use criterion::BatchSize;

// BatchSize controla cómo Criterion agrupa las iteraciones
// para amortizar el costo del setup

// SmallInput: setup es barato y rápido
// Criterion ejecuta muchas iteraciones por batch
BatchSize::SmallInput    // < 1 KB de datos

// LargeInput: setup es costoso o usa mucha memoria
// Criterion ejecuta pocas iteraciones por batch
BatchSize::LargeInput    // > 1 KB de datos

// PerIteration: setup individual para cada iteración
// Más preciso pero más lento (setup en cada iteración)
BatchSize::PerIteration  // Cuando necesitas aislamiento total

// NumBatches(n): exactamente n batches
BatchSize::NumBatches(10)

// NumIterations(n): exactamente n iteraciones por batch  
BatchSize::NumIterations(100)
```

```
┌──────────────────────────────────────────────────────────────────────┐
│         BatchSize y su efecto en la medición                         │
│                                                                      │
│  SmallInput (default para datos pequeños):                           │
│  ┌──────────────────────────────────────────┐                        │
│  │ setup│run│run│run│run│run│run│run│run│run│ ← muchos runs/batch    │
│  └──────────────────────────────────────────┘                        │
│  Ventaja: amortiza el costo del setup                                │
│  Riesgo: la caché puede "aprender" el patrón entre runs              │
│                                                                      │
│  LargeInput:                                                         │
│  ┌─────────────┐ ┌─────────────┐ ┌──────────┐                       │
│  │ setup│run│run│ │setup│run│run│ │setup│run │ ← pocos runs/batch    │
│  └─────────────┘ └─────────────┘ └──────────┘                        │
│  Ventaja: cada batch tiene datos "frescos"                           │
│  Riesgo: más tiempo total de benchmark                               │
│                                                                      │
│  PerIteration:                                                       │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐                        │
│  │setup│run│ │setup│run│ │setup│run│ │setup│run│ ← un run por setup  │
│  └────────┘ └────────┘ └────────┘ └────────┘                        │
│  Ventaja: máximo aislamiento                                         │
│  Riesgo: overhead del setup puede dominar                            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 14. Criterion::iter_batched: control fino del setup

`iter_batched` es la herramienta correcta para funciones destructivas:

```rust
use criterion::{black_box, criterion_group, criterion_main, Criterion, BatchSize};

fn bench_destructive_operations(c: &mut Criterion) {
    let mut group = c.benchmark_group("destructive_ops");
    
    // ── Vec::sort_unstable ─────────────────────────────────────
    // sort modifica el vector → necesita setup por iteración
    group.bench_function("sort_unstable_10k", |b| {
        b.iter_batched(
            || {
                // Setup: crear vector desordenado
                use std::collections::hash_map::DefaultHasher;
                use std::hash::{Hash, Hasher};
                let mut v = Vec::with_capacity(10_000);
                for i in 0..10_000u64 {
                    let mut h = DefaultHasher::new();
                    i.hash(&mut h);
                    v.push(h.finish() as u32);
                }
                v
            },
            |mut data| {
                // Routine: solo esto se mide
                data.sort_unstable();
                black_box(data)
            },
            BatchSize::LargeInput,
        )
    });
    
    // ── Vec::drain ─────────────────────────────────────────────
    // drain consume los elementos → setup necesario
    group.bench_function("drain_all", |b| {
        b.iter_batched(
            || (0..10_000).collect::<Vec<u32>>(),
            |mut data| {
                let drained: Vec<u32> = data.drain(..).collect();
                black_box(drained)
            },
            BatchSize::LargeInput,
        )
    });
    
    // ── String concatenation ───────────────────────────────────
    // push_str modifica el String → setup necesario
    group.bench_function("string_concat_1000", |b| {
        b.iter_batched(
            || String::with_capacity(10_000),
            |mut s| {
                for _ in 0..1_000 {
                    s.push_str("hello");
                }
                black_box(s)
            },
            BatchSize::SmallInput,
        )
    });
    
    // ── HashMap insert ─────────────────────────────────────────
    group.bench_function("hashmap_insert_1000", |b| {
        b.iter_batched(
            || std::collections::HashMap::with_capacity(1_000),
            |mut map| {
                for i in 0..1_000u32 {
                    map.insert(i, i * 2);
                }
                black_box(map)
            },
            BatchSize::SmallInput,
        )
    });
    
    group.finish();
}

criterion_group!(benches, bench_destructive_operations);
criterion_main!(benches);
```

### iter_batched_ref: cuando no quieres mover

```rust
// iter_batched_ref pasa &mut al valor del setup
// en lugar de moverlo (útil si quieres inspeccionar después)
c.bench_function("sort_batched_ref", |b| {
    b.iter_batched_ref(
        || (0..10_000).rev().collect::<Vec<u32>>(),
        |data| {
            // data es &mut Vec<u32>, no Vec<u32>
            data.sort();
            // El vector NO se mueve, Criterion lo descarta después
        },
        BatchSize::LargeInput,
    )
});
```

---

## 15. Throughput: medir bytes/segundo u operaciones/segundo

Para benchmarks de I/O, parsing, serialización, etc., el throughput es más informativo que el tiempo:

```rust
use criterion::{
    black_box, criterion_group, criterion_main, BenchmarkId, Criterion, Throughput,
};

fn parse_json(data: &[u8]) -> usize {
    // Simulación de parsing: contar caracteres '{'
    data.iter().filter(|&&b| b == b'{').count()
}

fn compress(data: &[u8]) -> Vec<u8> {
    // Simulación: copiar
    data.to_vec()
}

fn bench_throughput(c: &mut Criterion) {
    let mut group = c.benchmark_group("json_parsing");
    
    let sizes = [1_024, 4_096, 16_384, 65_536, 262_144]; // 1KB..256KB
    
    for size in sizes {
        let data = vec![b'{'; size]; // datos de test
        
        // ¡CLAVE! Declarar el throughput ANTES del bench
        group.throughput(Throughput::Bytes(size as u64));
        
        group.bench_with_input(
            BenchmarkId::new("parse", size),
            &data,
            |b, data| {
                b.iter(|| parse_json(black_box(data)))
            },
        );
    }
    
    group.finish();
    
    // ── Throughput por elementos ───────────────────────────────
    let mut group = c.benchmark_group("batch_processing");
    
    for count in [100, 1_000, 10_000] {
        let items: Vec<u32> = (0..count).collect();
        
        // Elements en lugar de Bytes
        group.throughput(Throughput::Elements(count as u64));
        
        group.bench_with_input(
            BenchmarkId::new("process", count),
            &items,
            |b, items| {
                b.iter(|| {
                    items.iter().map(|x| x.wrapping_mul(31)).sum::<u32>()
                })
            },
        );
    }
    
    group.finish();
}

criterion_group!(benches, bench_throughput);
criterion_main!(benches);
```

### Salida con throughput

```
json_parsing/parse/1024
                        time:   [234.56 ns  235.67 ns  236.89 ns]
                        thrpt:  [4.1234 GiB/s  4.1413 GiB/s  4.1608 GiB/s]

json_parsing/parse/4096
                        time:   [923.45 ns  925.67 ns  928.12 ns]
                        thrpt:  [4.2100 GiB/s  4.2212 GiB/s  4.2313 GiB/s]

json_parsing/parse/65536
                        time:   [14.567 µs  14.678 µs  14.801 µs]
                        thrpt:  [4.2234 GiB/s  4.2589 GiB/s  4.2911 GiB/s]

batch_processing/process/1000
                        time:   [345.67 ns  346.78 ns  347.89 ns]
                        thrpt:  [2.8745 Gelem/s  2.8837 Gelem/s  2.8930 Gelem/s]
```

```
Throughput constante → tu función escala linealmente con el input
Throughput decreciente → posible efecto de caché o complejidad superlineal

  GiB/s │
        │  ●──────●──────●──────●──────●   Constante (bueno)
   4.0  │
        │
        │  ●──────●──────●
   3.0  │                 \                 Decreciente (investigar)
        │                  \
   2.0  │                   ●──────●
        └──────────────────────────────── Tamaño (KB)
          1     4     16    64    256
```

---

## 16. Análisis estadístico que hace Criterion

Criterion hace mucho más que calcular la media. Entender su análisis estadístico te ayuda a interpretar los resultados:

```
┌──────────────────────────────────────────────────────────────────────┐
│            Pipeline estadístico de Criterion                         │
│                                                                      │
│  Datos raw: [(iters₁, time₁), (iters₂, time₂), ..., (itersₙ, tₙ)] │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────┐                             │
│  │  1. Regresión lineal (OLS)          │                             │
│  │     time = slope × iters + offset   │                             │
│  │     slope = tiempo/iteración        │                             │
│  │     R² ≈ 1.0 si la medición        │                             │
│  │     es estable                       │                             │
│  └─────────────────────────────────────┘                             │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────┐                             │
│  │  2. Bootstrap (100,000 resamples)   │                             │
│  │     - Resamplea los datos con       │                             │
│  │       reemplazo                      │                             │
│  │     - Recalcula slope cada vez       │                             │
│  │     - Construye distribución del     │                             │
│  │       estimador                      │                             │
│  │     - Extrae percentiles para IC     │                             │
│  └─────────────────────────────────────┘                             │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────┐                             │
│  │  3. Intervalo de confianza          │                             │
│  │     [percentil_2.5, percentil_97.5] │                             │
│  │     = IC del 95% del slope           │                             │
│  └─────────────────────────────────────┘                             │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────┐                             │
│  │  4. Clasificación de outliers       │                             │
│  │     (Método de Tukey)               │                             │
│  │     Q1 = percentil 25              │                             │
│  │     Q3 = percentil 75              │                             │
│  │     IQR = Q3 - Q1                   │                             │
│  │     Mild: > Q3 + 1.5×IQR           │                             │
│  │     Severe: > Q3 + 3.0×IQR          │                             │
│  └─────────────────────────────────────┘                             │
│       │                                                              │
│       ▼                                                              │
│  ┌─────────────────────────────────────┐                             │
│  │  5. Comparación con baseline        │                             │
│  │     (si existe ejecución anterior)   │                             │
│  │     change = (new - old) / old       │                             │
│  │     IC del cambio via bootstrap      │                             │
│  │     t-test para significancia        │                             │
│  └─────────────────────────────────────┘                             │
└──────────────────────────────────────────────────────────────────────┘
```

### Regresión lineal vs media simple

```
¿Por qué regresión y no simplemente media(tiempos)?

El problema de la media simple:
  Mides: [100ns, 100ns, 101ns, 99ns, 100ns]
  Media: 100ns
  
  PERO: cada medición incluye el overhead de llamar al timer
  Si el overhead es 20ns:
  Tiempo real = 80ns, no 100ns

La regresión lineal:
  Muestra  Iters  Tiempo_total
  1        1000   100,020ns    ← 80×1000 + 20,020 overhead
  2        2000   180,019ns    ← 80×2000 + 20,019 overhead  
  3        3000   260,021ns    ← 80×3000 + 20,021 overhead

  slope = 80ns    ← CORRECTO: tiempo real por iteración
  intercept ≈ 20µs ← overhead (descartado)

  La pendiente aísla automáticamente el overhead constante.
```

### Bootstrap resampling

```
Datos originales: [d₁, d₂, d₃, d₄, d₅, ..., d₁₀₀]

Resample 1: [d₇, d₃, d₃, d₉₉, d₁₂, ..., d₅₆]  → slope₁ = 7.21ns
Resample 2: [d₄₅, d₁, d₈₈, d₄₅, d₂, ..., d₃₃]  → slope₂ = 7.18ns
Resample 3: [d₂, d₂, d₅₀, d₁, d₁₀₀, ..., d₇₇]  → slope₃ = 7.23ns
...
Resample 100,000: [...]                              → slope₁₀₀₀₀₀ = 7.19ns

Distribución de slopes:
     │    ▄▄
     │   ████
     │  ██████
     │ ████████
     │██████████
     │██████████
     └──────────────
     7.15  7.20  7.25  (ns)
           │        │
     IC 95%: [7.17, 7.24]

Ventaja del bootstrap: NO asume distribución normal.
Funciona con cualquier distribución de los datos.
```

---

## 17. Detección de regresiones y mejoras

Cada vez que ejecutas un benchmark, Criterion compara contra la ejecución anterior:

```
Primera ejecución:
fibonacci_iterative_20  time:   [7.2031 ns 7.2187 ns 7.2380 ns]
                        ← Sin línea "change:" (no hay baseline)

Segunda ejecución (sin cambios):
fibonacci_iterative_20  time:   [7.1987 ns 7.2145 ns 7.2312 ns]
                        change: [-0.5765% -0.1702% +0.2345%] (p = 0.42 > 0.05)
                        No change in performance detected.

Tercera ejecución (con optimización):
fibonacci_iterative_20  time:   [5.3456 ns 5.3567 ns 5.3689 ns]
                        change: [-25.87% -25.72% -25.56%] (p = 0.00 < 0.05)
                        Performance has improved.    ← VERDE en terminal

Cuarta ejecución (con regresión):
fibonacci_iterative_20  time:   [9.1234 ns 9.1567 ns 9.1890 ns]
                        change: [+70.12% +70.45% +70.89%] (p = 0.00 < 0.05)
                        Performance has regressed.   ← ROJO en terminal
```

### Anatomía del análisis de cambio

```
change: [-25.87% -25.72% -25.56%] (p = 0.00 < 0.05)
         ───────  ───────  ───────   ────────────────
         │        │        │         │
         │        │        │         p-value del test de hipótesis
         │        │        │         H₀: "no hubo cambio"
         │        │        │         p < 0.05 → rechazamos H₀
         │        │        │         → SÍ hubo cambio significativo
         │        │        │
         │        │        límite superior del IC 95% del cambio
         │        │
         │        estimador puntual del cambio
         │
         límite inferior del IC 95% del cambio

Interpretación:
  Con 95% de confianza, el rendimiento mejoró entre 25.56% y 25.87%.
  El estimador central indica ~25.72% de mejora.
  p ≈ 0 → la mejora es estadísticamente significativa.
```

### Umbrales y noise threshold

```
┌──────────────────────────────────────────────────────────────────────┐
│           Clasificación del cambio detectado                         │
│                                                                      │
│  change                 noise_threshold (default: 1%)                │
│  ──────                 ─────────────────────────────                │
│                                                                      │
│  │IC del cambio│ ⊆ [-1%, +1%]                                       │
│  → "Change within noise threshold"                                   │
│     El cambio está COMPLETAMENTE dentro del ruido                    │
│     → No es un cambio real, es variación normal                      │
│                                                                      │
│  IC cruza ±1% Y p > 0.05                                            │
│  → "No change in performance detected"                               │
│     El cambio no es estadísticamente significativo                   │
│                                                                      │
│  IC > +1% Y p < 0.05                                                │
│  → "Performance has regressed"   (ROJO)                              │
│     Regresión estadísticamente significativa                         │
│                                                                      │
│  IC < -1% Y p < 0.05                                                │
│  → "Performance has improved"    (VERDE)                             │
│     Mejora estadísticamente significativa                            │
│                                                                      │
│  IC cruza ±1% Y p < 0.05                                            │
│  → "Change within noise threshold" O reporta el cambio              │
│     (depende de la relación exacta entre IC y threshold)             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 18. Baseline management: save y compare

Criterion mantiene **baselines** (referencias de rendimiento) entre ejecuciones:

```bash
# ═══════════════════════════════════════════════════════════
# Flujo normal: baseline automática
# ═══════════════════════════════════════════════════════════

# Primera ejecución: guarda resultados en target/criterion/<bench>/new/
cargo bench

# Segunda ejecución: "new" anterior se mueve a "base",
# nueva ejecución en "new", compara new vs base
cargo bench

# Cada ejecución: base ← new anterior, new ← ejecución actual


# ═══════════════════════════════════════════════════════════
# Baselines nombradas: guardar un punto de referencia
# ═══════════════════════════════════════════════════════════

# Guardar baseline con nombre (antes de hacer cambios)
cargo bench -- --save-baseline before_optimization

# Hacer cambios al código...

# Comparar contra la baseline guardada
cargo bench -- --baseline before_optimization

# Esto compara la ejecución actual contra "before_optimization"
# en lugar de comparar contra la última ejecución


# ═══════════════════════════════════════════════════════════
# Comparar dos baselines existentes (sin ejecutar benchmarks)
# ═══════════════════════════════════════════════════════════

# Guardar estado actual
cargo bench -- --save-baseline version_a

# Cambiar código...
cargo bench -- --save-baseline version_b

# Comparar a vs b sin re-ejecutar
# (requiere cargo-criterion o análisis manual de los JSON)


# ═══════════════════════════════════════════════════════════
# Limpiar baselines
# ═══════════════════════════════════════════════════════════

# Eliminar todos los datos de criterion
rm -rf target/criterion/

# Eliminar baseline específica (buscar en la estructura de directorios)
# target/criterion/<bench_name>/<baseline_name>/
```

### Estructura de directorios de baselines

```
target/criterion/
├── fibonacci_iterative_20/
│   ├── base/                         ← Ejecución anterior (automática)
│   │   ├── estimates.json            ← Estadísticas
│   │   ├── sample.json               ← Datos crudos
│   │   └── tukey.json                ← Clasificación de outliers
│   ├── new/                          ← Ejecución más reciente
│   │   ├── estimates.json
│   │   ├── sample.json
│   │   └── tukey.json
│   ├── change/                       ← Análisis new vs base
│   │   └── estimates.json
│   ├── before_optimization/          ← Baseline nombrada
│   │   ├── estimates.json
│   │   ├── sample.json
│   │   └── tukey.json
│   └── report/                       ← HTML de este benchmark
│       ├── index.html
│       ├── pdf.svg
│       ├── regression.svg
│       └── ...
└── report/                           ← Índice general
    └── index.html
```

### Flujo de trabajo típico con baselines

```
Flujo de optimización:

1. cargo bench -- --save-baseline original
   → Guarda el rendimiento actual como referencia

2. // Haces cambios al código (optimización)

3. cargo bench -- --baseline original
   → Compara el código modificado contra "original"
   → Muestra "Performance has improved" o "regressed"

4. Si la mejora es significativa:
   cargo bench -- --save-baseline optimized_v1
   → Guarda el nuevo estado como referencia

5. // Más cambios...

6. cargo bench -- --baseline optimized_v1
   → ¿La siguiente optimización ayudó?

Resultado: tienes un historial de baselines nombradas
que documenta la evolución del rendimiento.
```

---

## 19. HTML reports: interpretar los gráficos

Con la feature `html_reports`, Criterion genera reportes visuales en `target/criterion/report/index.html`:

```bash
# Generar reportes y abrirlos
cargo bench
# Abrir en navegador:
xdg-open target/criterion/report/index.html    # Linux
open target/criterion/report/index.html         # macOS
```

### Tipos de gráficos generados

```
┌──────────────────────────────────────────────────────────────────────┐
│              Gráficos del reporte HTML de Criterion                   │
│                                                                      │
│  1. PDF (Probability Density Function)                               │
│  ───────────────────────────────────────                             │
│  Densidad │       ▄▄                                                 │
│           │      ████                                                │
│           │    ████████                                              │
│           │  ████████████                                            │
│           │████████████████                                          │
│           └──────────────────── Tiempo (ns)                          │
│                7.0  7.2  7.4                                         │
│                                                                      │
│  Muestra la distribución de los tiempos medidos.                     │
│  - Campana estrecha = medición estable                               │
│  - Campana ancha = mucho ruido                                       │
│  - Doble pico = posible bimodalidad (¡investigar!)                   │
│  - Cola larga a la derecha = outliers por interferencia del OS        │
│                                                                      │
│  2. Regression (Iteraciones vs Tiempo)                               │
│  ───────────────────────────────────────                             │
│  Tiempo │                            ●                               │
│         │                        ●                                   │
│         │                    ●                                       │
│         │                ●                                           │
│         │            ●         ← La línea de regresión               │
│         │        ●               debe pasar por todos                │
│         │    ●                   los puntos                          │
│         │●                                                           │
│         └────────────────────────── Iteraciones                      │
│                                                                      │
│  La pendiente = tiempo/iteración.                                    │
│  - Puntos alineados = medición limpia                                │
│  - Puntos dispersos = ruido excesivo                                 │
│  - Curvatura = la función no escala linealmente con iters            │
│    (raro, indica bug en el benchmark)                                │
│                                                                      │
│  3. Violin Plot (comparación entre parametrizaciones)                │
│  ───────────────────────────────────────                             │
│  size=100    ╔═══╗                                                   │
│              ║   ║                                                   │
│              ╚═══╝                                                   │
│  size=1000     ╔══════╗                                              │
│                ║      ║                                              │
│                ╚══════╝                                              │
│  size=10000          ╔══════════════╗                                 │
│                      ║              ║                                │
│                      ╚══════════════╝                                │
│  ──────────────────────────────────── Tiempo                         │
│                                                                      │
│  Ancho = densidad en ese punto del eje temporal.                     │
│  Permite comparar distribuciones entre parámetros.                   │
│                                                                      │
│  4. Line Chart (parámetro vs tiempo, solo con groups)                │
│  ───────────────────────────────────────                             │
│  Tiempo │                                                            │
│         │          linear ●───────── ●                               │
│         │                /                                           │
│         │               /                                            │
│         │  binary ●────●────●────●                                   │
│         │  hash   ●────●────●────●                                   │
│         └──────────────────────────── Parámetro                      │
│                                                                      │
│  Solo aparece cuando usas benchmark groups con parámetros.           │
│  Muestra la relación entre parámetro y rendimiento                   │
│  para cada implementación.                                           │
│                                                                      │
│  5. Change Plot (solo si existe baseline)                            │
│  ───────────────────────────────────────                             │
│  Muestra la distribución del cambio porcentual                       │
│  entre la baseline y la ejecución actual.                            │
│  La zona gris es el noise_threshold.                                 │
│  Si la distribución cae fuera → cambio real.                         │
└──────────────────────────────────────────────────────────────────────┘
```

### Estructura del reporte HTML

```
target/criterion/report/index.html
│
├── Lista de todos los benchmarks con enlaces
│
├── Para cada benchmark individual:
│   ├── PDF plot (distribución de tiempos)
│   ├── Regression plot (iteraciones vs tiempo)
│   ├── Iteration times plot (tiempo por muestra)
│   ├── Typical plot (distribución bootstrap del slope)
│   ├── Resumen estadístico (tabla)
│   └── Change report (si existe baseline)
│
└── Para cada grupo:
    ├── Violin plot (comparación entre miembros)
    └── Line chart (parámetro vs rendimiento)
```

---

## 20. Filtrar benchmarks: --bench y nombres

Cuando tienes muchos benchmarks, no siempre quieres ejecutar todos:

```bash
# ═══════════════════════════════════════════════════════════
# Filtrar por archivo de benchmark (--bench)
# ═══════════════════════════════════════════════════════════

# Ejecutar solo el archivo benches/sorting.rs
cargo bench --bench sorting

# Ejecutar solo benches/searching.rs
cargo bench --bench searching


# ═══════════════════════════════════════════════════════════
# Filtrar por nombre del benchmark (después de --)
# ═══════════════════════════════════════════════════════════

# Ejecutar solo benchmarks cuyo nombre contiene "quicksort"
cargo bench -- "quicksort"

# Combinado: en el archivo sorting, solo los que contengan "quick"
cargo bench --bench sorting -- "quick"

# Regex: benchmarks que empiezan con "sort/"
cargo bench -- "^sort/"

# Benchmarks parametrizados: solo con tamaño 10000
cargo bench -- "/10000$"


# ═══════════════════════════════════════════════════════════
# Otros flags útiles
# ═══════════════════════════════════════════════════════════

# Solo compilar, no ejecutar (verificar que compila)
cargo bench --no-run

# Listar benchmarks disponibles sin ejecutar
cargo bench -- --list

# Ejecutar sin generar plots (más rápido)
cargo bench -- --noplot

# Modo rápido: menos muestras, menos warmup
cargo bench -- --quick

# Modo verbose: más información durante ejecución
cargo bench -- --verbose

# Formato de salida
cargo bench -- --output-format bencher    # formato compatible con libtest
cargo bench -- --output-format criterion  # formato por defecto
```

### Ejemplo de nombres jerárquicos

```rust
// En el código:
let mut group = c.benchmark_group("sorting");
group.bench_function("quicksort", |b| { ... });
group.bench_with_input(BenchmarkId::new("mergesort", 1000), ...);
group.bench_with_input(BenchmarkId::new("mergesort", 10000), ...);

// Nombres generados:
// sorting/quicksort
// sorting/mergesort/1000
// sorting/mergesort/10000

// Filtros:
// cargo bench -- "sorting"              → todos los sorting
// cargo bench -- "sorting/quicksort"    → solo quicksort
// cargo bench -- "mergesort"            → solo mergesort (todos los tamaños)
// cargo bench -- "mergesort/10000"      → solo mergesort con 10000
```

---

## 21. Criterion vs Divan: alternativas modernas

Criterion no es la única opción. **Divan** es una alternativa más reciente:

```
┌──────────────────────────────────────────────────────────────────────┐
│                Criterion.rs vs Divan                                  │
│                                                                      │
│  Característica        Criterion 0.5        Divan 0.1+               │
│  ─────────────         ────────────         ────────                 │
│  API                   Closures + macros    Atributos #[divan::bench]│
│  Estabilidad           Maduro, 7+ años      Reciente (~2023)         │
│  Rust channel          Stable               Stable                   │
│  HTML reports          Sí (plotters)         No (solo consola)        │
│  Baseline compare      Sí                   No (aún)                 │
│  Bootstrap             100,000 resamples    No (usa estadística      │
│                                             directa)                 │
│  Groups/comparisons    Sí                   Sí (via atributos)       │
│  Throughput             Sí                   Sí                       │
│  Compilación           Lenta (plotters)      Rápida                  │
│  Líneas de código      Más verbose           Más conciso              │
│  Mantenimiento         Activo               Activo                   │
│  Adopción              Estándar de facto     Creciendo                │
│  Paralelismo           No                   Sí (#[divan::bench       │
│                                             (threads = N)])          │
└──────────────────────────────────────────────────────────────────────┘
```

```rust
// ── Criterion ──────────────────────────────────────────────
// benches/my_bench.rs
use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn bench_fibonacci(c: &mut Criterion) {
    c.bench_function("fibonacci_20", |b| {
        b.iter(|| fibonacci(black_box(20)))
    });
}

criterion_group!(benches, bench_fibonacci);
criterion_main!(benches);


// ── Divan ──────────────────────────────────────────────────
// benches/my_bench.rs
fn main() {
    divan::main();
}

#[divan::bench]
fn fibonacci_20() -> u64 {
    fibonacci(divan::black_box(20))
}

// Con parámetros:
#[divan::bench(args = [10, 100, 1000, 10000])]
fn sort(n: usize) {
    let mut data: Vec<u32> = (0..n as u32).rev().collect();
    data.sort();
    divan::black_box(data);
}
```

Divan es más conciso pero carece de HTML reports y baseline management. **Criterion sigue siendo la recomendación** para benchmarks serios donde necesitas comparar entre ejecuciones y generar reportes visuales.

---

## 22. Integración con cargo-criterion

`cargo-criterion` es un frontend alternativo para Criterion que mejora la experiencia:

```bash
# Instalar cargo-criterion
cargo install cargo-criterion

# Usar en lugar de cargo bench
cargo criterion

# Con filtro
cargo criterion --bench sorting -- "quicksort"

# Generar reportes en formato diferente
cargo criterion --message-format=json     # Para CI/CD
cargo criterion --output-format=bencher   # Compatible con benchcmp
```

### Ventajas de cargo-criterion

```
┌──────────────────────────────────────────────────────────────────────┐
│          cargo bench vs cargo criterion                               │
│                                                                      │
│  cargo bench                      cargo criterion                    │
│  ───────────                      ───────────────                    │
│  Incluido con cargo               Instalar aparte                   │
│  Compila tests + benches          Solo compila benchmarks            │
│  Formato texto estándar           Formato texto mejorado             │
│  HTML via librería                HTML vía cargo-criterion            │
│  Sin soporte CI nativo            JSON output para CI                │
│  N/A                              Soporte para external tools        │
│                                   (gnuplot alternativo, etc.)        │
│                                                                      │
│  Nota: ambos ejecutan el mismo código de Criterion.                  │
│  cargo-criterion es solo un "driver" diferente.                      │
│  Los benchmarks se escriben exactamente igual.                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Uso en CI/CD

```yaml
# .github/workflows/bench.yml
name: Benchmarks
on:
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable
      
      - name: Install cargo-criterion
        run: cargo install cargo-criterion
      
      - name: Run benchmarks
        run: cargo criterion --message-format=json > benchmark_results.json
      
      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: benchmark_results.json
```

---

## 23. Custom measurements y wall clock vs CPU time

Por defecto, Criterion mide **wall clock time** con `std::time::Instant`. Pero puedes personalizar qué y cómo medir:

```rust
use criterion::{
    black_box, criterion_group, criterion_main,
    measurement::WallTime,
    Criterion,
};

// ═══════════════════════════════════════════════════════════
// Opción 1: Usar wall clock (default)
// ═══════════════════════════════════════════════════════════
fn bench_wall_time(c: &mut Criterion<WallTime>) {
    // Criterion<WallTime> es el tipo por defecto
    // WallTime usa std::time::Instant internamente
    c.bench_function("wall_time_example", |b| {
        b.iter(|| {
            let mut sum = 0u64;
            for i in 0..10_000 {
                sum = sum.wrapping_add(black_box(i));
            }
            sum
        })
    });
}

// ═══════════════════════════════════════════════════════════
// Opción 2: iter_custom para mediciones especiales
// ═══════════════════════════════════════════════════════════
fn bench_custom_measurement(c: &mut Criterion) {
    c.bench_function("cpu_time_manual", |b| {
        b.iter_custom(|iters| {
            // Puedes usar cualquier método de medición aquí
            let start = std::time::Instant::now();
            for _i in 0..iters {
                black_box(fibonacci(black_box(20)));
            }
            start.elapsed()
        })
    });
}

// ═══════════════════════════════════════════════════════════
// Opción 3: Medir solo una parte del código
// ═══════════════════════════════════════════════════════════
fn bench_partial_measurement(c: &mut Criterion) {
    c.bench_function("only_search_not_build", |b| {
        b.iter_custom(|iters| {
            // Setup NO medido
            let data: Vec<u32> = (0..100_000).collect();
            
            // Solo medir la búsqueda
            let start = std::time::Instant::now();
            for i in 0..iters {
                black_box(data.binary_search(&black_box(i as u32)));
            }
            start.elapsed()
            // Teardown NO medido (drop de data ocurre aquí)
        })
    });
}

fn fibonacci(n: u64) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => {
            let (mut a, mut b) = (0u64, 1u64);
            for _ in 2..=n { let t = a + b; a = b; b = t; }
            b
        }
    }
}

criterion_group!(benches, bench_wall_time, bench_custom_measurement, bench_partial_measurement);
criterion_main!(benches);
```

### Wall time vs CPU time

```
┌──────────────────────────────────────────────────────────────────────┐
│           Wall time vs CPU time en benchmarks                        │
│                                                                      │
│  Wall time (Instant::now):                                           │
│  ─────────────────────────                                           │
│  Mide tiempo transcurrido real, incluyendo:                          │
│  - Tiempo en que el OS suspendió tu proceso                          │
│  - Tiempo esperando I/O                                              │
│  - Tiempo de otros procesos en el mismo core                         │
│  Mejor para: medir la experiencia del usuario                        │
│                                                                      │
│  CPU time (clock_gettime(CLOCK_PROCESS_CPUTIME_ID)):                 │
│  ─────────────────────────────────────────────────                   │
│  Solo cuenta el tiempo de CPU de TU proceso:                         │
│  - Ignora tiempo suspendido                                          │
│  - Ignora I/O wait                                                   │
│  - Más estable (menos afectado por carga del sistema)                │
│  Mejor para: medir eficiencia algorítmica pura                      │
│                                                                      │
│  Ejemplo de divergencia:                                             │
│  ┌────────────────────────────────────────────────┐                  │
│  │ CPU  ████████░░░░████████░░░░████████          │                  │
│  │      running  sleep  running  sleep  running   │                  │
│  │                                                │                  │
│  │ Wall time: ████████████████████████████████     │ 100ms           │
│  │ CPU  time: ████████    ████████    ████████     │  60ms           │
│  └────────────────────────────────────────────────┘                  │
│                                                                      │
│  Criterion usa wall time por defecto. Para código CPU-bound          │
│  puro (nuestro caso en microbenchmarks), la diferencia               │
│  es mínima si sigues las reglas del T01 (cerrar apps,                │
│  CPU pinning, nice -20).                                              │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 24. Benchmark de código async

Si trabajas con `tokio` u otro runtime async, Criterion puede medir código asíncrono:

```rust
use criterion::{black_box, criterion_group, criterion_main, Criterion};

// Necesitas tokio como dev-dependency
// [dev-dependencies]
// tokio = { version = "1", features = ["full"] }
// criterion = { version = "0.5", features = ["html_reports", "async_tokio"] }

async fn async_fibonacci(n: u64) -> u64 {
    // Simulación de trabajo async
    match n {
        0 => 0,
        1 => 1,
        _ => {
            let (mut a, mut b) = (0u64, 1u64);
            for _ in 2..=n {
                let t = a + b;
                a = b;
                b = t;
                // En código real, aquí habría .await
            }
            b
        }
    }
}

async fn async_fetch_and_parse(data: &[u8]) -> usize {
    // Simulación de I/O async
    tokio::task::yield_now().await;
    data.len()
}

fn bench_async(c: &mut Criterion) {
    let rt = tokio::runtime::Runtime::new().unwrap();
    
    // Opción 1: usar block_on manualmente
    c.bench_function("async_fibonacci_manual", |b| {
        b.iter(|| {
            rt.block_on(async {
                black_box(async_fibonacci(black_box(20)).await)
            })
        })
    });
    
    // Opción 2: usar to_async (requiere feature async_tokio)
    c.bench_function("async_fibonacci_native", |b| {
        b.to_async(&rt).iter(|| async {
            black_box(async_fibonacci(black_box(20)).await)
        })
    });
    
    // Opción 3: async con setup
    c.bench_function("async_with_setup", |b| {
        b.to_async(&rt).iter_batched(
            || vec![0u8; 10_000], // setup (sync)
            |data| async move {   // routine (async)
                async_fetch_and_parse(&data).await
            },
            criterion::BatchSize::SmallInput,
        )
    });
}

criterion_group!(benches, bench_async);
criterion_main!(benches);
```

### Consideraciones para benchmarks async

```
┌──────────────────────────────────────────────────────────────────────┐
│          Benchmarking de código async: cuidados                      │
│                                                                      │
│  1. El runtime introduce overhead                                    │
│     - Cada .await tiene costo de scheduling                          │
│     - El benchmark mide: tu código + overhead del runtime            │
│     - Para código CPU-bound, async no aporta nada                    │
│       → mejor benchmarkear la versión sync                           │
│                                                                      │
│  2. Crear el runtime UNA VEZ fuera del benchmark                     │
│     ❌ b.iter(|| { let rt = Runtime::new()... })  ← crea rt cada vez│
│     ✅ let rt = Runtime::new()... fuera de iter   ← crea una vez     │
│                                                                      │
│  3. to_async() vs block_on manual                                    │
│     - to_async(): Criterion gestiona el runtime                      │
│     - block_on(): tú gestionas el runtime                            │
│     - to_async() es preferible cuando está disponible                │
│                                                                      │
│  4. Cuidado con tokio::spawn en benchmarks                           │
│     - Tasks spawneadas pueden interferir entre iteraciones           │
│     - Asegurar que todas las tasks terminen en cada iteración        │
│                                                                      │
│  5. Benchmark de I/O real vs simulado                                │
│     - I/O real (red, disco): alta varianza, muchas muestras          │
│     - I/O simulado: más estable, pero no mide lo que importa        │
│     - Para I/O real: considerar measurement_time largo (30s+)        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 25. Benchmark de código que usa allocations

Las allocaciones son una fuente oculta de ruido en benchmarks:

```rust
use criterion::{black_box, criterion_group, criterion_main, Criterion, BatchSize};

fn bench_allocation_patterns(c: &mut Criterion) {
    let mut group = c.benchmark_group("allocation_impact");
    
    // ── Caso 1: allocation dentro del loop ─────────────────
    // Cada iteración alloca y libera un Vec
    group.bench_function("alloc_per_iter", |b| {
        b.iter(|| {
            let v: Vec<u64> = (0..10_000).collect(); // alloc
            let sum: u64 = v.iter().sum();
            black_box(sum)
            // v se dropea aquí → free
        })
        // Mide: alloc + llenado + suma + free
        // El allocator domina para inputs pequeños
    });
    
    // ── Caso 2: reutilizar allocation ──────────────────────
    // El Vec se crea una vez, se reutiliza
    group.bench_function("reuse_alloc", |b| {
        let mut v: Vec<u64> = Vec::with_capacity(10_000);
        b.iter(|| {
            v.clear(); // NO libera memoria, solo resetea len
            v.extend(0..10_000u64);
            let sum: u64 = v.iter().sum();
            black_box(sum)
        })
        // Mide: llenado + suma (sin alloc/free)
        // Mucho más rápido y estable
    });
    
    // ── Caso 3: pre-allocated con tamaño exacto ────────────
    group.bench_function("preallocated", |b| {
        let data: Vec<u64> = (0..10_000).collect();
        b.iter(|| {
            let sum: u64 = data.iter().sum();
            black_box(sum)
        })
        // Mide: solo la suma
        // Lo más rápido posible
    });
    
    // ── Caso 4: benchmark del allocator mismo ──────────────
    // A veces QUIERES medir el allocation
    group.bench_function("benchmark_allocation", |b| {
        b.iter(|| {
            let v: Vec<u8> = vec![0u8; black_box(4096)];
            black_box(v)
        })
        // Mide: alloc + zero-fill + free
        // Útil para comparar allocators
    });
    
    group.finish();
}

// ── Benchmark de diferentes estrategias de allocation ────────────
fn bench_string_strategies(c: &mut Criterion) {
    let mut group = c.benchmark_group("string_concat");
    
    let words: Vec<&str> = vec!["hello"; 1_000];
    
    // Estrategia 1: String::new() + push_str
    group.bench_function("push_str", |b| {
        b.iter(|| {
            let mut s = String::new();
            for word in &words {
                s.push_str(word);
            }
            black_box(s)
        })
        // Múltiples re-allocations conforme el String crece
    });
    
    // Estrategia 2: with_capacity + push_str
    group.bench_function("with_capacity", |b| {
        b.iter(|| {
            let mut s = String::with_capacity(5 * words.len());
            for word in &words {
                s.push_str(word);
            }
            black_box(s)
        })
        // Una sola allocation
    });
    
    // Estrategia 3: collect con join
    group.bench_function("join", |b| {
        b.iter(|| {
            let s: String = words.join("");
            black_box(s)
        })
        // Internamente calcula el tamaño total primero
    });
    
    group.finish();
}

criterion_group!(benches, bench_allocation_patterns, bench_string_strategies);
criterion_main!(benches);
```

### Impacto de allocations en la medición

```
┌──────────────────────────────────────────────────────────────────────┐
│       ¿Las allocations pertenecen al benchmark?                      │
│                                                                      │
│  Pregunta clave: ¿Qué estás midiendo?                               │
│                                                                      │
│  "¿Qué tan rápido es mi algoritmo?"                                 │
│  → Excluir allocations (pre-allocar fuera del loop)                  │
│  → Medir solo el cómputo                                            │
│                                                                      │
│  "¿Qué tan rápido es mi sistema end-to-end?"                        │
│  → Incluir allocations (son parte del costo real)                    │
│  → El usuario experimenta alloc + cómputo + free                     │
│                                                                      │
│  "¿Qué allocator es mejor para mi workload?"                        │
│  → Las allocations SON el benchmark                                  │
│  → Comparar jemalloc, mimalloc, sistema                              │
│                                                                      │
│  Recomendación: reportar AMBOS cuando sea relevante                  │
│                                                                      │
│  bench("sort_with_alloc", || vec![...].sort())     → 150ns          │
│  bench("sort_preallocated", || prepared.sort())    →  80ns          │
│                                                                      │
│  El lector entiende: 80ns es el cómputo, 70ns es el allocator.      │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 26. Errores comunes con Criterion

```
┌──────────────────────────────────────────────────────────────────────┐
│            10 errores comunes con Criterion                          │
│                                                                      │
│  1. Olvidar harness = false                                          │
│  ──────────────────────────                                          │
│  Error: "cannot find function `main`" o test framework errors        │
│                                                                      │
│  [[bench]]                                                           │
│  name = "my_bench"                                                   │
│  harness = false    ← OBLIGATORIO                                    │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  2. No usar black_box                                                │
│  ──────────────────────                                              │
│  El benchmark reporta ~0ns porque el compilador eliminó todo.        │
│  SIEMPRE usar black_box en inputs constantes o resultados no usados. │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  3. Setup dentro de iter()                                           │
│  ──────────────────────────                                          │
│  b.iter(|| {                                                         │
│      let data = expensive_setup();  ← SE MIDE                       │
│      process(data)                  ← SE MIDE                       │
│  })                                                                  │
│  El tiempo reportado incluye el setup.                               │
│  Solución: mover setup fuera o usar iter_batched.                    │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  4. Olvidar group.finish()                                           │
│  ──────────────────────────                                          │
│  Sin finish(), el grupo no genera reportes ni comparaciones.         │
│  Criterion 0.5 da un warning, pero no error.                        │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  5. Benchmark de función que muta estado global                      │
│  ──────────────────────────────────────────────                      │
│  static mut COUNTER: u64 = 0;                                        │
│  b.iter(|| unsafe { COUNTER += 1; })                                 │
│  El estado se acumula entre iteraciones → resultados inconsistentes  │
│  Solución: usar iter_batched con estado fresco por batch.            │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  6. Comparar benchmarks de diferentes ejecuciones sin baseline       │
│  ──────────────────────────────────────────────────────────────       │
│  Ejecutas cargo bench, anotas "7.2ns", haces cambios,                │
│  ejecutas de nuevo, ves "7.0ns" → ¡3% de mejora!                    │
│  Pero la variación entre ejecuciones puede ser > 3%.                 │
│  Solución: --save-baseline + --baseline para comparar                │
│  en la misma sesión con las mismas condiciones.                      │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  7. Ignorar los outliers                                             │
│  ──────────────────────────                                          │
│  "Found 15 outliers among 100 measurements (15%)"                   │
│  15% de outliers indica un entorno ruidoso.                          │
│  Cerrar apps, desactivar turbo boost, usar CPU pinning.              │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  8. sample_size demasiado bajo para funciones rápidas                │
│  ──────────────────────────────────────────────────                  │
│  Funciones < 10ns necesitan sample_size alto (200+).                 │
│  Con solo 10 muestras, el IC es enorme y no informativo.             │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  9. Benchmark en modo debug                                          │
│  ──────────────────────────                                          │
│  cargo bench ya compila con optimizaciones (profile = bench).        │
│  Pero si usas cargo run --release para ejecutar el binario           │
│  de benchmark manualmente, asegúrate de que sea release.             │
│  debug puede ser 10-100x más lento.                                  │
│                                                                      │
│  ─────────────────────────────────────────────────────────────────   │
│  10. Asumir que más rápido = mejor sin context                       │
│  ──────────────────────────────────────────────                      │
│  fibonacci_iterative: 7ns, fibonacci_recursive: 24µs                 │
│  "¡La iterativa es 3000x mejor!"                                     │
│  Pero la iterativa usa O(1) memoria y la recursiva O(n) stack.       │
│  El benchmark no captura uso de memoria, ni errores de stack.        │
│  Siempre contextualizar con el perfil completo de la función.        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 27. Comparativa: Criterion vs benchmark manual del T01

Comparemos cómo se ve el mismo benchmark con el framework manual del T01 versus Criterion:

```rust
// ═══════════════════════════════════════════════════════════
// Versión manual (estilo T01)
// ═══════════════════════════════════════════════════════════

use std::time::Instant;
use std::hint::black_box;

struct BenchResult {
    mean_ns: f64,
    median_ns: f64,
    stddev_ns: f64,
    cv: f64,
    ci95_lower: f64,
    ci95_upper: f64,
    samples: usize,
}

fn bench_manual(f: impl Fn(), warmup: usize, samples: usize, iters: usize) -> BenchResult {
    // Warmup
    for _ in 0..warmup {
        black_box(f());
    }
    
    // Medir
    let mut times = Vec::with_capacity(samples);
    for _ in 0..samples {
        let start = Instant::now();
        for _ in 0..iters {
            black_box(f());
        }
        let elapsed = start.elapsed().as_nanos() as f64 / iters as f64;
        times.push(elapsed);
    }
    
    // Estadísticas
    times.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let mean = times.iter().sum::<f64>() / times.len() as f64;
    let median = times[times.len() / 2];
    let variance = times.iter().map(|t| (t - mean).powi(2)).sum::<f64>() / (times.len() - 1) as f64;
    let stddev = variance.sqrt();
    let cv = stddev / mean;
    let sem = stddev / (times.len() as f64).sqrt();
    
    BenchResult {
        mean_ns: mean,
        median_ns: median,
        stddev_ns: stddev,
        cv,
        ci95_lower: mean - 1.96 * sem,
        ci95_upper: mean + 1.96 * sem,
        samples,
    }
}

fn main() {
    let result = bench_manual(
        || fibonacci(black_box(20)),
        100,     // warmup
        50,      // samples  
        100_000, // iters per sample
    );
    
    println!("fibonacci(20):");
    println!("  mean:   {:.2} ns", result.mean_ns);
    println!("  median: {:.2} ns", result.median_ns);
    println!("  stddev: {:.2} ns", result.stddev_ns);
    println!("  CV:     {:.2}%", result.cv * 100.0);
    println!("  IC95:   [{:.2}, {:.2}] ns", result.ci95_lower, result.ci95_upper);
}

fn fibonacci(n: u64) -> u64 {
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 0..n { let t = a + b; a = b; b = t; }
    b
}
// ~50 líneas de infraestructura, sin reportes, sin comparaciones,
// sin baselines, sin detección de regresiones.
```

```rust
// ═══════════════════════════════════════════════════════════
// Versión Criterion (equivalente)
// ═══════════════════════════════════════════════════════════

// benches/fibonacci.rs
use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn fibonacci(n: u64) -> u64 {
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 0..n { let t = a + b; a = b; b = t; }
    b
}

fn bench(c: &mut Criterion) {
    c.bench_function("fibonacci_20", |b| {
        b.iter(|| fibonacci(black_box(20)))
    });
}

criterion_group!(benches, bench);
criterion_main!(benches);
// 12 líneas. Warmup automático, estadísticas superiores,
// HTML reports, baselines, detección de regresiones.
```

### Comparativa detallada

```
┌──────────────────────────────────────────────────────────────────────┐
│       Manual (T01) vs Criterion: análisis completo                   │
│                                                                      │
│  Aspecto                Manual           Criterion                   │
│  ───────                ──────           ─────────                   │
│  Líneas de código       ~50+ por bench   ~12 por bench               │
│  Warmup                 # fijo           Adaptativo (tiempo)         │
│  Iteraciones            # fijo           Adaptativo + lineal         │
│  Estimador              Media simple     Slope de regresión          │
│  Overhead del timer     Incluido         Excluido (intercepto)       │
│  IC                     Normal asumido   Bootstrap (no asume)        │
│  Outliers               Manual           Tukey automático            │
│  Regresión detection    No               Automático con p-value      │
│  Baselines              No               Save/compare                │
│  HTML reports            No               Sí                          │
│  Throughput              Manual           Integrado                   │
│  Parametrización        Manual           BenchmarkId + groups        │
│  Mantenimiento          Tú               La comunidad Rust           │
│                                                                      │
│  ¿Cuándo usar manual?                                                │
│  - Aprender cómo funciona el benchmarking (T01)                      │
│  - En C (donde no hay Criterion)                                     │
│  - Mediciones muy específicas que Criterion no soporta               │
│                                                                      │
│  ¿Cuándo usar Criterion?                                             │
│  - Siempre que trabajes en Rust (la respuesta por defecto)           │
│  - Necesitas comparaciones entre implementaciones                    │
│  - Necesitas detectar regresiones                                    │
│  - Necesitas reportes visuales                                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 28. Programa de práctica: bench_collections

Vamos a construir un benchmark completo que compare diferentes colecciones de Rust para operaciones comunes. Este programa ejercita todas las funcionalidades de Criterion que hemos visto.

### Estructura del proyecto

```
bench_collections/
├── Cargo.toml
├── src/
│   └── lib.rs
└── benches/
    └── collections.rs
```

### Cargo.toml

```toml
[package]
name = "bench_collections"
version = "0.1.0"
edition = "2021"

[dependencies]

[dev-dependencies]
criterion = { version = "0.5", features = ["html_reports"] }
rand = "0.8"
rand_chacha = "0.3"

[[bench]]
name = "collections"
harness = false
```

### src/lib.rs

```rust
// src/lib.rs
//
// Operaciones sobre colecciones que vamos a benchmarkear.
// Cada función representa un patrón de uso diferente.

use std::collections::{BTreeMap, HashMap, LinkedList, VecDeque};

// ═══════════════════════════════════════════════════════════════
// OPERACIÓN 1: Inserción secuencial
// Insertar N elementos con claves 0..N
// ═══════════════════════════════════════════════════════════════

pub fn insert_vec(n: usize) -> Vec<(u64, u64)> {
    let mut v = Vec::new();
    for i in 0..n as u64 {
        v.push((i, i * 31));
    }
    v
}

pub fn insert_vec_preallocated(n: usize) -> Vec<(u64, u64)> {
    let mut v = Vec::with_capacity(n);
    for i in 0..n as u64 {
        v.push((i, i * 31));
    }
    v
}

pub fn insert_vecdeque(n: usize) -> VecDeque<(u64, u64)> {
    let mut vd = VecDeque::new();
    for i in 0..n as u64 {
        vd.push_back((i, i * 31));
    }
    vd
}

pub fn insert_linkedlist(n: usize) -> LinkedList<(u64, u64)> {
    let mut ll = LinkedList::new();
    for i in 0..n as u64 {
        ll.push_back((i, i * 31));
    }
    ll
}

pub fn insert_hashmap(n: usize) -> HashMap<u64, u64> {
    let mut map = HashMap::new();
    for i in 0..n as u64 {
        map.insert(i, i * 31);
    }
    map
}

pub fn insert_hashmap_preallocated(n: usize) -> HashMap<u64, u64> {
    let mut map = HashMap::with_capacity(n);
    for i in 0..n as u64 {
        map.insert(i, i * 31);
    }
    map
}

pub fn insert_btreemap(n: usize) -> BTreeMap<u64, u64> {
    let mut map = BTreeMap::new();
    for i in 0..n as u64 {
        map.insert(i, i * 31);
    }
    map
}

// ═══════════════════════════════════════════════════════════════
// OPERACIÓN 2: Búsqueda
// Buscar un elemento que existe en la colección
// ═══════════════════════════════════════════════════════════════

pub fn search_vec_linear(data: &[(u64, u64)], key: u64) -> Option<u64> {
    data.iter().find(|(k, _)| *k == key).map(|(_, v)| *v)
}

pub fn search_vec_binary(data: &[(u64, u64)], key: u64) -> Option<u64> {
    data.binary_search_by_key(&key, |(k, _)| *k)
        .ok()
        .map(|idx| data[idx].1)
}

pub fn search_hashmap(data: &HashMap<u64, u64>, key: u64) -> Option<u64> {
    data.get(&key).copied()
}

pub fn search_btreemap(data: &BTreeMap<u64, u64>, key: u64) -> Option<u64> {
    data.get(&key).copied()
}

// ═══════════════════════════════════════════════════════════════
// OPERACIÓN 3: Iteración con transformación
// Sumar todos los valores multiplicados por un factor
// ═══════════════════════════════════════════════════════════════

pub fn iterate_vec(data: &[(u64, u64)]) -> u64 {
    data.iter().map(|(_, v)| v.wrapping_mul(7)).sum()
}

pub fn iterate_linkedlist(data: &LinkedList<(u64, u64)>) -> u64 {
    data.iter().map(|(_, v)| v.wrapping_mul(7)).sum()
}

pub fn iterate_hashmap(data: &HashMap<u64, u64>) -> u64 {
    data.values().map(|v| v.wrapping_mul(7)).sum()
}

pub fn iterate_btreemap(data: &BTreeMap<u64, u64>) -> u64 {
    data.values().map(|v| v.wrapping_mul(7)).sum()
}

// ═══════════════════════════════════════════════════════════════
// OPERACIÓN 4: Push/pop al frente (front-heavy workload)
// Insertar y remover del frente de la colección
// ═══════════════════════════════════════════════════════════════

pub fn front_ops_vec(n: usize) {
    let mut v: Vec<u64> = Vec::new();
    for i in 0..n as u64 {
        v.insert(0, i); // O(n) — desplaza todo
    }
    for _ in 0..n {
        v.remove(0); // O(n) — desplaza todo
    }
}

pub fn front_ops_vecdeque(n: usize) {
    let mut vd: VecDeque<u64> = VecDeque::new();
    for i in 0..n as u64 {
        vd.push_front(i); // O(1) amortizado
    }
    for _ in 0..n {
        vd.pop_front(); // O(1)
    }
}

pub fn front_ops_linkedlist(n: usize) {
    let mut ll: LinkedList<u64> = LinkedList::new();
    for i in 0..n as u64 {
        ll.push_front(i); // O(1)
    }
    for _ in 0..n {
        ll.pop_front(); // O(1)
    }
}
```

### benches/collections.rs

```rust
// benches/collections.rs
//
// Benchmark comparativo de colecciones de Rust.
// Ejercita: groups, parametrización, throughput, iter_batched, black_box.

use criterion::{
    black_box, criterion_group, criterion_main, BenchmarkId, Criterion, Throughput,
};
use std::collections::{BTreeMap, HashMap, LinkedList};
use std::time::Duration;

use bench_collections::*;

// ═══════════════════════════════════════════════════════════════
// BENCHMARK 1: Inserción
// Compara Vec, Vec (pre-alloc), VecDeque, LinkedList,
// HashMap, HashMap (pre-alloc), BTreeMap
// ═══════════════════════════════════════════════════════════════

fn bench_insertion(c: &mut Criterion) {
    let mut group = c.benchmark_group("insertion");
    group.warm_up_time(Duration::from_secs(2));
    group.measurement_time(Duration::from_secs(5));

    for size in [100, 1_000, 10_000, 100_000] {
        group.throughput(Throughput::Elements(size as u64));

        group.bench_with_input(
            BenchmarkId::new("Vec", size),
            &size,
            |b, &size| b.iter(|| insert_vec(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("Vec_prealloc", size),
            &size,
            |b, &size| b.iter(|| insert_vec_preallocated(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("VecDeque", size),
            &size,
            |b, &size| b.iter(|| insert_vecdeque(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("LinkedList", size),
            &size,
            |b, &size| b.iter(|| insert_linkedlist(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("HashMap", size),
            &size,
            |b, &size| b.iter(|| insert_hashmap(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("HashMap_prealloc", size),
            &size,
            |b, &size| b.iter(|| insert_hashmap_preallocated(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("BTreeMap", size),
            &size,
            |b, &size| b.iter(|| insert_btreemap(black_box(size))),
        );
    }

    group.finish();
}

// ═══════════════════════════════════════════════════════════════
// BENCHMARK 2: Búsqueda
// Compara Vec (lineal), Vec (binaria), HashMap, BTreeMap
// Busca el elemento del medio (peor caso para lineal)
// ═══════════════════════════════════════════════════════════════

fn bench_search(c: &mut Criterion) {
    let mut group = c.benchmark_group("search");
    group.warm_up_time(Duration::from_secs(2));
    group.measurement_time(Duration::from_secs(5));

    for size in [100, 1_000, 10_000, 100_000] {
        // Preparar datos
        let vec_data: Vec<(u64, u64)> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let hash_data: HashMap<u64, u64> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let btree_data: BTreeMap<u64, u64> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let target = size as u64 / 2; // buscar el del medio

        group.bench_with_input(
            BenchmarkId::new("Vec_linear", size),
            &size,
            |b, _| b.iter(|| search_vec_linear(black_box(&vec_data), black_box(target))),
        );

        group.bench_with_input(
            BenchmarkId::new("Vec_binary", size),
            &size,
            |b, _| b.iter(|| search_vec_binary(black_box(&vec_data), black_box(target))),
        );

        group.bench_with_input(
            BenchmarkId::new("HashMap", size),
            &size,
            |b, _| b.iter(|| search_hashmap(black_box(&hash_data), black_box(target))),
        );

        group.bench_with_input(
            BenchmarkId::new("BTreeMap", size),
            &size,
            |b, _| b.iter(|| search_btreemap(black_box(&btree_data), black_box(target))),
        );
    }

    group.finish();
}

// ═══════════════════════════════════════════════════════════════
// BENCHMARK 3: Iteración
// Compara Vec, LinkedList, HashMap, BTreeMap
// Itera todos los elementos y acumula un resultado
// ═══════════════════════════════════════════════════════════════

fn bench_iteration(c: &mut Criterion) {
    let mut group = c.benchmark_group("iteration");
    group.warm_up_time(Duration::from_secs(2));
    group.measurement_time(Duration::from_secs(5));

    for size in [100, 1_000, 10_000, 100_000] {
        group.throughput(Throughput::Elements(size as u64));

        // Preparar datos
        let vec_data: Vec<(u64, u64)> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let ll_data: LinkedList<(u64, u64)> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let hash_data: HashMap<u64, u64> = (0..size as u64).map(|i| (i, i * 31)).collect();
        let btree_data: BTreeMap<u64, u64> = (0..size as u64).map(|i| (i, i * 31)).collect();

        group.bench_with_input(
            BenchmarkId::new("Vec", size),
            &size,
            |b, _| b.iter(|| iterate_vec(black_box(&vec_data))),
        );

        group.bench_with_input(
            BenchmarkId::new("LinkedList", size),
            &size,
            |b, _| b.iter(|| iterate_linkedlist(black_box(&ll_data))),
        );

        group.bench_with_input(
            BenchmarkId::new("HashMap", size),
            &size,
            |b, _| b.iter(|| iterate_hashmap(black_box(&hash_data))),
        );

        group.bench_with_input(
            BenchmarkId::new("BTreeMap", size),
            &size,
            |b, _| b.iter(|| iterate_btreemap(black_box(&btree_data))),
        );
    }

    group.finish();
}

// ═══════════════════════════════════════════════════════════════
// BENCHMARK 4: Operaciones front (push_front/pop_front)
// Compara Vec (O(n) shift), VecDeque (O(1)), LinkedList (O(1))
// ═══════════════════════════════════════════════════════════════

fn bench_front_ops(c: &mut Criterion) {
    let mut group = c.benchmark_group("front_ops");
    group.warm_up_time(Duration::from_secs(2));
    group.measurement_time(Duration::from_secs(5));

    // Tamaños más pequeños porque Vec.insert(0,x) es O(n²) total
    for size in [100, 500, 1_000, 5_000] {
        group.throughput(Throughput::Elements(size as u64));

        group.bench_with_input(
            BenchmarkId::new("Vec", size),
            &size,
            |b, &size| b.iter(|| front_ops_vec(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("VecDeque", size),
            &size,
            |b, &size| b.iter(|| front_ops_vecdeque(black_box(size))),
        );

        group.bench_with_input(
            BenchmarkId::new("LinkedList", size),
            &size,
            |b, &size| b.iter(|| front_ops_linkedlist(black_box(size))),
        );
    }

    group.finish();
}

// ═══════════════════════════════════════════════════════════════
// BENCHMARK 5: Sort (iter_batched porque sort muta el input)
// Vec::sort vs Vec::sort_unstable
// ═══════════════════════════════════════════════════════════════

fn bench_sort(c: &mut Criterion) {
    use criterion::BatchSize;
    use rand::prelude::*;
    use rand_chacha::ChaCha8Rng;

    let mut group = c.benchmark_group("sort");
    group.warm_up_time(Duration::from_secs(2));
    group.measurement_time(Duration::from_secs(5));

    for size in [100, 1_000, 10_000, 100_000] {
        group.throughput(Throughput::Elements(size as u64));

        // sort_unstable: no preserva orden de elementos iguales
        // generalmente más rápido (no necesita memoria auxiliar)
        group.bench_with_input(
            BenchmarkId::new("sort_unstable_random", size),
            &size,
            |b, &size| {
                b.iter_batched(
                    || {
                        let mut rng = ChaCha8Rng::seed_from_u64(42);
                        let mut data: Vec<u64> = (0..size as u64).collect();
                        data.shuffle(&mut rng);
                        data
                    },
                    |mut data| {
                        data.sort_unstable();
                        black_box(data)
                    },
                    BatchSize::LargeInput,
                )
            },
        );

        // sort: preserva orden de elementos iguales (merge sort)
        group.bench_with_input(
            BenchmarkId::new("sort_stable_random", size),
            &size,
            |b, &size| {
                b.iter_batched(
                    || {
                        let mut rng = ChaCha8Rng::seed_from_u64(42);
                        let mut data: Vec<u64> = (0..size as u64).collect();
                        data.shuffle(&mut rng);
                        data
                    },
                    |mut data| {
                        data.sort();
                        black_box(data)
                    },
                    BatchSize::LargeInput,
                )
            },
        );

        // sort en datos ya ordenados (mejor caso)
        group.bench_with_input(
            BenchmarkId::new("sort_unstable_sorted", size),
            &size,
            |b, &size| {
                b.iter_batched(
                    || (0..size as u64).collect::<Vec<u64>>(),
                    |mut data| {
                        data.sort_unstable();
                        black_box(data)
                    },
                    BatchSize::LargeInput,
                )
            },
        );

        // sort en datos invertidos (caso adverso típico)
        group.bench_with_input(
            BenchmarkId::new("sort_unstable_reversed", size),
            &size,
            |b, &size| {
                b.iter_batched(
                    || (0..size as u64).rev().collect::<Vec<u64>>(),
                    |mut data| {
                        data.sort_unstable();
                        black_box(data)
                    },
                    BatchSize::LargeInput,
                )
            },
        );
    }

    group.finish();
}

// ═══════════════════════════════════════════════════════════════
// Configuración global
// ═══════════════════════════════════════════════════════════════

criterion_group!(
    benches,
    bench_insertion,
    bench_search,
    bench_iteration,
    bench_front_ops,
    bench_sort,
);
criterion_main!(benches);
```

### Ejecución y análisis

```bash
# ═══════════════════════════════════════════════════════════
# Ejecutar todos los benchmarks
# ═══════════════════════════════════════════════════════════
cargo bench

# ═══════════════════════════════════════════════════════════
# Ejecutar solo un grupo
# ═══════════════════════════════════════════════════════════
cargo bench -- "insertion"
cargo bench -- "search"
cargo bench -- "iteration"
cargo bench -- "front_ops"
cargo bench -- "sort"

# ═══════════════════════════════════════════════════════════
# Solo inserción con tamaño 10000
# ═══════════════════════════════════════════════════════════
cargo bench -- "insertion/.*/10000"

# ═══════════════════════════════════════════════════════════
# Guardar baseline y comparar
# ═══════════════════════════════════════════════════════════
cargo bench -- --save-baseline initial

# (hacer cambios a las funciones en lib.rs)

cargo bench -- --baseline initial

# ═══════════════════════════════════════════════════════════
# Ver reportes HTML
# ═══════════════════════════════════════════════════════════
xdg-open target/criterion/report/index.html
```

### Resultados esperados (aproximados)

```
┌──────────────────────────────────────────────────────────────────────┐
│          Resultados esperados del benchmark                          │
│          (valores aproximados, varían por hardware)                   │
│                                                                      │
│  ═══ INSERCIÓN (10,000 elementos) ═══                                │
│                                                                      │
│  Vec               ~120 µs   (re-allocs conforme crece)              │
│  Vec (pre-alloc)    ~60 µs   (sin re-allocs)                        │
│  VecDeque          ~130 µs   (similar a Vec sin pre-alloc)           │
│  LinkedList        ~250 µs   (allocation por nodo)                   │
│  HashMap           ~350 µs   (hashing + table management)           │
│  HashMap (prealloc) ~200 µs  (sin rehashing)                        │
│  BTreeMap          ~400 µs   (tree balancing)                        │
│                                                                      │
│  ═══ BÚSQUEDA (10,000 elementos, buscar el del medio) ═══           │
│                                                                      │
│  Vec (lineal)     ~15 µs    O(n) — recorre la mitad                  │
│  Vec (binaria)    ~20 ns    O(log n) — ~13 comparaciones             │
│  HashMap           ~15 ns    O(1) — un hash + lookup                 │
│  BTreeMap          ~30 ns    O(log n) — ~13 nodos                    │
│                                                                      │
│  ═══ ITERACIÓN (10,000 elementos) ═══                                │
│                                                                      │
│  Vec                ~3 µs    Cache-friendly, datos contiguos         │
│  LinkedList        ~12 µs    Cache-hostile, punteros dispersos       │
│  HashMap            ~8 µs    Semi-contiguo, huecos en tabla          │
│  BTreeMap           ~6 µs    Nodos B-tree, razonablemente compacto   │
│                                                                      │
│  ═══ FRONT OPS (1,000 push_front + 1,000 pop_front) ═══             │
│                                                                      │
│  Vec              ~200 µs    O(n) por operación → O(n²) total        │
│  VecDeque           ~5 µs    O(1) amortizado                        │
│  LinkedList         ~25 µs   O(1) pero allocation por nodo           │
│                                                                      │
│  ═══ SORT (10,000 elementos, random) ═══                             │
│                                                                      │
│  sort_unstable     ~180 µs   Pattern-defeating quicksort             │
│  sort (stable)     ~200 µs   Merge sort, usa memoria auxiliar        │
│  sort_unstable     ~10 µs    Datos ya ordenados (detección O(n))     │
│  (already sorted)                                                    │
│  sort_unstable     ~15 µs    Datos invertidos (patrón detectado)     │
│  (reversed)                                                          │
│                                                                      │
│  Lecciones clave:                                                    │
│  • Vec gana en iteración gracias a localidad de caché                │
│  • LinkedList pierde en casi todo excepto push_front                 │
│  • HashMap gana en búsqueda pero pierde en iteración                 │
│  • Pre-allocation (with_capacity) reduce el costo ~2x                │
│  • sort_unstable es más rápido que sort (menos memoria auxiliar)      │
│  • Datos pre-ordenados aceleran sort 10-20x (detección O(n))         │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 29. Ejercicios

### Ejercicio 1: benchmark de funciones de hash

**Objetivo**: Comparar el rendimiento de diferentes estrategias de hashing.

**Tareas**:

**a)** Crea un benchmark que compare:
   - `std::collections::hash_map::DefaultHasher`
   - Hashing manual simple (multiplicación + XOR)
   - Hashing de strings de diferentes longitudes

**b)** Usa `BenchmarkId` para parametrizar por longitud de string: 8, 32, 128, 512, 2048 bytes.

**c)** Configura `Throughput::Bytes` para obtener resultados en GB/s.

**d)** Genera el reporte HTML y analiza: ¿a qué tamaño el throughput se estabiliza? ¿A qué tamaño empiezan los efectos de caché?

---

### Ejercicio 2: benchmark con iter_batched

**Objetivo**: Practicar `iter_batched` con funciones destructivas.

**Tareas**:

**a)** Crea un benchmark que compare tres formas de construir un `HashMap<String, Vec<u32>>`:
   - Insertar uno por uno con `entry().or_insert_with(Vec::new).push()`
   - Construir Vec primero, luego crear HashMap con `from_iter()`
   - Usar `HashMap::from` con un array de tuplas

**b)** Cada construcción consume los datos de entrada, así que usa `iter_batched` con `BatchSize::LargeInput`.

**c)** Parametriza por número de claves únicas: 10, 100, 1000, con 10 valores por clave.

**d)** ¿Cuál método es más rápido? ¿Cambia el ranking con el número de claves?

---

### Ejercicio 3: detección de regresiones con baseline

**Objetivo**: Practicar el flujo de trabajo de baselines.

**Tareas**:

**a)** Implementa una función `sum_of_squares(data: &[f64]) -> f64` que calcule Σ(x²).

**b)** Benchmarkea la función con datos de tamaño 100,000. Guarda baseline con `--save-baseline v1`.

**c)** "Optimiza" la función usando `.map(|x| x * x).sum()` en lugar de `.map(|x| x.powi(2)).sum()`.

**d)** Ejecuta de nuevo con `--baseline v1` y observa el `change:` en la salida. ¿Hay mejora significativa? ¿El p-value es < 0.05?

**e)** Ahora "rompe" la función cambiando a un bucle for manual sin iteradores. ¿Hay regresión? ¿Por qué sí o no?

---

### Ejercicio 4: benchmark group con violin plots

**Objetivo**: Generar un reporte HTML con gráficas comparativas.

**Tareas**:

**a)** Crea un benchmark group llamado `"string_search"` que compare:
   - `str::contains()` (búsqueda de substring)
   - `str::find()` (misma búsqueda, retorna Option<usize>)
   - Búsqueda manual con `str::as_bytes()` y loop

**b)** Parametriza con strings de longitudes: 100, 1000, 10000, 100000 caracteres. El patrón a buscar debe estar al final del string (peor caso).

**c)** Genera el reporte HTML y abre `target/criterion/string_search/report/index.html`.

**d)** Analiza el violin plot: ¿las distribuciones se solapan? ¿Alguna implementación tiene outliers consistentes? ¿El throughput escala linealmente con el tamaño del input?

---

## Navegación

- **Anterior**: [T01 - Benchmarking correcto](../T01_Benchmarking_correcto/README.md)
- **Siguiente**: [T03 - Benchmark en C](../T03_Benchmark_en_C/README.md)

---

> **Capítulo 4: Benchmarking y Profiling** — Sección 1: Microbenchmarking — Tópico 2 de 4
