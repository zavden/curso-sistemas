# T02 — Benchmark comparativo

> **Bloque 17 · Capítulo 4 · Sección 3 · Tópico 2**
> Comparaciones con rigor: mismo algoritmo en C vs Rust, AoS vs SoA, estructuras distintas. Metodología, presentación y honestidad intelectual.

---

## Índice

1. [Qué es un benchmark comparativo](#1-qué-es-un-benchmark-comparativo)
2. [Por qué los comparativos son peligrosos](#2-por-qué-los-comparativos-son-peligrosos)
3. [El principio de equidad](#3-el-principio-de-equidad)
4. [Qué se puede comparar legítimamente](#4-qué-se-puede-comparar-legítimamente)
5. [Qué NO se puede comparar (y por qué)](#5-qué-no-se-puede-comparar-y-por-qué)
6. [Diseñar un benchmark comparativo](#6-diseñar-un-benchmark-comparativo)
7. [C vs Rust: reglas del combate justo](#7-c-vs-rust-reglas-del-combate-justo)
8. [Implementar el mismo algoritmo en ambos](#8-implementar-el-mismo-algoritmo-en-ambos)
9. [Comparar flags de compilación](#9-comparar-flags-de-compilación)
10. [Ejecutar: infraestructura común](#10-ejecutar-infraestructura-común)
11. [Recoger datos: qué métricas capturar](#11-recoger-datos-qué-métricas-capturar)
12. [Estadística: media, mediana, IC, percentiles](#12-estadística-media-mediana-ic-percentiles)
13. [Visualizar resultados: tablas y gráficos](#13-visualizar-resultados-tablas-y-gráficos)
14. [Comparar AoS vs SoA con rigor](#14-comparar-aos-vs-soa-con-rigor)
15. [Comparar Vec vs LinkedList](#15-comparar-vec-vs-linkedlist)
16. [Comparar algoritmos: quicksort vs mergesort vs radix](#16-comparar-algoritmos-quicksort-vs-mergesort-vs-radix)
17. [Interpretar resultados: qué concluyes y qué no](#17-interpretar-resultados-qué-concluyes-y-qué-no)
18. [Red flags en benchmarks publicados](#18-red-flags-en-benchmarks-publicados)
19. [Trampas clásicas de las comparaciones](#19-trampas-clásicas-de-las-comparaciones)
20. [Reproducibilidad: qué documentar](#20-reproducibilidad-qué-documentar)
21. [Presentación honesta: el buen reporte](#21-presentación-honesta-el-buen-reporte)
22. [Errores comunes al publicar comparativos](#22-errores-comunes-al-publicar-comparativos)
23. [Herramientas auxiliares: hyperfine, poop, cargo-show-asm](#23-herramientas-auxiliares-hyperfine-poop-cargo-show-asm)
24. [Benchmark games: lecciones del Computer Language Benchmarks Game](#24-benchmark-games-lecciones-del-computer-language-benchmarks-game)
25. [Caso de estudio: suma de vector C vs Rust](#25-caso-de-estudio-suma-de-vector-c-vs-rust)
26. [Programa de práctica: comparative_lab/](#26-programa-de-práctica-comparative_lab)
27. [Ejercicios](#27-ejercicios)
28. [Checklist del comparativo honesto](#28-checklist-del-comparativo-honesto)
29. [Navegación](#29-navegación)

---

## 1. Qué es un benchmark comparativo

Un **benchmark comparativo** mide el rendimiento relativo de **dos o más
implementaciones** del mismo concepto. Ejemplos típicos:

- Mismo algoritmo, distintos lenguajes (C vs Rust).
- Misma estructura de datos, distinto layout (AoS vs SoA).
- Mismo problema, distintos algoritmos (quicksort vs mergesort).
- Misma librería, distintas versiones (regression tests).
- Distintos allocators (glibc vs jemalloc vs mimalloc).
- Distintos compiladores (GCC vs Clang).
- Distintos flags (`-O2` vs `-O3` vs `-Os`).

El objetivo no es solo "¿cuál es más rápido?" sino **entender por qué** y
**bajo qué condiciones**.

Un comparativo útil responde:

1. **En qué contexto específico** uno es más rápido que otro.
2. **Cuánta** diferencia hay (magnitud concreta).
3. **Por qué** existe esa diferencia (mecanismo).
4. **Qué trade-offs** tiene el ganador (memoria, complejidad, portabilidad).

Si no respondes esas cuatro preguntas, el comparativo no es útil; es propaganda.

---

## 2. Por qué los comparativos son peligrosos

Los benchmarks comparativos son los **más propensos a conclusiones engañosas**
de todos los tipos de benchmark. Razones:

### 2.1. Incentivos sesgados

El autor del comparativo suele tener una preferencia previa. "Rust es mejor que
C" o "mi librería es mejor que la tuya". Ese sesgo se cuela en las decisiones
sutiles del benchmark.

### 2.2. Asimetría de esfuerzo

El autor conoce su lado favorito y sabe optimizarlo. El otro lado se implementa
superficialmente. El resultado: una comparación entre código optimizado y código
naive que parece "C vs Rust" pero es realmente "C bien escrito vs Rust mal
escrito" o viceversa.

### 2.3. Cherry-picking de benchmarks

Si una implementación gana en 7 de 10 benchmarks, publicas los 7. El lector ve
"70% mejor" cuando en realidad es "mejor en 7 casos específicos, peor en 3".

### 2.4. Sistemas complejos

El resultado depende de factores no obvios: compilador, flags, CPU, allocator,
kernel, carga del sistema. Pequeñas diferencias en cualquiera pueden cambiar
quién gana.

### 2.5. Generalización ilegítima

Medir "Rust vs C" para un algoritmo y concluir "Rust es más rápido que C" es
una generalización masiva no justificada. Solo puedes concluir sobre **ese
algoritmo, con esos compiladores, en ese hardware**.

### 2.6. El benchmark determina el ganador

Distintas elecciones de benchmark favorecen distintas implementaciones. Si
eliges trabajo CPU-bound puro, ganan las que vectorizan bien. Si eliges
memory-bound, ganan las cache-friendly. Si eliges latency, ganan las sin GC.

No existe el benchmark "neutral". Cada elección es una decisión sesgada.

**Conclusión**: los comparativos honestos requieren disciplina extra. Todo lo
visto en T01 (formular hipótesis) aplica, más las reglas específicas de esta
sección.

---

## 3. El principio de equidad

> **Todas las implementaciones deben tener las mismas oportunidades de optimización.**

Equidad significa:

1. **Mismo nivel de esfuerzo**: si el autor invirtió 10 horas en C, debe invertir
   10 horas en Rust. Si pasa 2h en uno y 10h en el otro, el resultado refleja
   esfuerzo, no lenguaje.

2. **Mismo nivel de expertise**: el autor debe ser igual de competente en ambos,
   o consultar a alguien que lo sea. Un novato en Rust comparando contra un
   experto en C falsea el resultado.

3. **Mismas optimizaciones disponibles**: si usas SIMD manual en uno, úsalo en
   el otro. Si uno tiene `-O3 -march=native`, el otro también.

4. **Mismo problema resuelto**: ambas implementaciones deben hacer **exactamente**
   lo mismo. Si una omite validación de input que la otra hace, no es equitativo.

5. **Mismo ecosistema de librerías**: si usas `std::sort` en C++ y `sort()` en
   Rust, ambos usan su biblioteca estándar. Está bien. Si usas un quicksort
   custom en C y `sort()` de Rust, la comparación no es entre C y Rust sino
   entre "mi quicksort" y "el sort de std de Rust".

6. **Mismas garantías**: ¿hay bounds checking? ¿Hay overflow checking? ¿El
   resultado es exacto en los dos? Garantías distintas = problemas distintos.

### Casos donde la equidad es imposible

- C no tiene bounds checking por default; Rust sí. Comparar suma de array en
  release de Rust es equitativo (bounds eliminados por optimizer). Comparar en
  debug de Rust no es equitativo.
- C no tiene borrow checker. Rust sí. Comparar el coste en runtime: el borrow
  checker no tiene coste runtime, la comparación es válida.
- Rust tiene `Option<T>` con niche optimization. C no. Comparar APIs nulables
  no es del todo equitativo, pero es lo mismo que existe en la práctica.

Cuando la equidad total es imposible, **documéntalo**. Di: "C se mide sin
bounds checking (comportamiento default), Rust en release (bounds eliminados
por optimizer en la mayoría de los casos)". El lector decide si le interesa.

---

## 4. Qué se puede comparar legítimamente

Comparaciones **válidas** (con las precauciones de §3):

1. **Mismo algoritmo en dos lenguajes**: `fibonacci(40)` en C y en Rust. Útil
   para entender el overhead de lenguaje (debería ser ~0 en release).
2. **Dos algoritmos que resuelven el mismo problema**: `quicksort` vs
   `mergesort` en C. Útil para elegir algoritmo.
3. **Dos layouts de la misma estructura**: AoS vs SoA. Útil para entender
   impacto de cache.
4. **Dos versiones de la misma librería**: `serde 1.0.100` vs `1.0.150`. Útil
   para detectar regresiones.
5. **Dos allocators con el mismo programa**: glibc vs jemalloc. Útil para
   elegir allocator en producción.
6. **Dos niveles de optimización**: `-O2` vs `-O3` para el mismo código. Útil
   para configurar builds.
7. **Dos hardware distintos con el mismo programa**: AMD Zen4 vs Intel Raptor
   Lake. Útil para capacity planning.
8. **Dos estrategias de concurrencia**: `Mutex` vs `RwLock` vs `Atomic` para el
   mismo workload.

En todos estos casos, la comparación es **local y acotada**: te dice algo
específico, no "X es universalmente mejor que Y".

---

## 5. Qué NO se puede comparar (y por qué)

Comparaciones **inválidas** (o que requieren enormes cuidados):

### 5.1. "Lenguaje X vs lenguaje Y" en abstracto

No existe "Rust" en abstracto, existe "código Rust específico compilado con
`rustc` versión N". Generalizar a "Rust es lento/rápido" es ilegítimo.

Tampoco existe "C". Existe "C compilado con GCC 13 con `-O2`". Otro compilador,
otro nivel, otro flag, otros resultados.

### 5.2. Benchmarks sintéticos como proxy de aplicaciones reales

`fibonacci(40)` no predice el rendimiento de un servidor web. Cada aplicación
tiene su propio perfil. Los micro-benchmarks son útiles para entender
componentes específicos, **no para predecir el todo**.

### 5.3. Workloads sesgados hacia una implementación

Si mides "suma de 1 millón de floats" y una de las implementaciones usa
auto-vectorización con SIMD mientras la otra no, la comparación no es del
código sino del compilador.

### 5.4. Tiempo de "hello world"

Comparar el tiempo de startup de dos runtimes (JVM vs CLR vs Go vs Node vs Rust
release binary) es técnicamente válido, pero la conclusión "lenguaje X es más
rápido que Y" a partir de ese benchmark es absurda: mide runtime init, no el
lenguaje.

### 5.5. Tamaño de binario como proxy de rendimiento

`ls -l target/release/hello_world` no te dice nada sobre el rendimiento.

### 5.6. Comparaciones sin control de versión

"C es más rápido que Rust" compilado con GCC 4.9 contra `rustc 1.10` (2016) no
es comparable a la realidad actual. Los compiladores cambian año a año.

### 5.7. Medir "usabilidad" o "productividad" como benchmark

"Lenguaje X tiene mejor rendimiento que Y porque mi equipo escribió el programa
en 2h" no es un benchmark de rendimiento. Es un benchmark de familiaridad del
equipo.

---

## 6. Diseñar un benchmark comparativo

Proceso de diseño paso a paso:

### Paso 1: definir el problema

Un enunciado único, específico y preciso de **qué se va a medir**.

- ✓ "Tiempo de suma de 10 millones de ints uniformes aleatorios en un array
  contiguo."
- ✗ "Rendimiento de arrays."

### Paso 2: definir las variantes

Qué implementaciones vas a comparar. Número entre 2 y 5 idealmente. Más de 5 y
la comparativa se vuelve ilegible.

### Paso 3: definir los inputs

Inputs representativos, preferentemente varios tamaños para ver escalabilidad.

- Tamaño pequeño (cabe en L1): ~N=4096 ints.
- Tamaño medio (cabe en L2/L3): ~N=100K.
- Tamaño grande (no cabe en cache): ~N=10M.

### Paso 4: definir las métricas

- **Wall clock time** por operación.
- **Throughput** (ops/s o bytes/s).
- **Instrucciones retiradas** (IPC), si vas a profundizar.
- **Cache misses** (si el análisis lo requiere).
- **Memoria consumida** (peak heap).

### Paso 5: definir las condiciones

- Hardware específico (CPU, RAM, frecuencia fija).
- OS y versión.
- Compilador y versión.
- Flags de compilación.
- Ambiente: taskset, nice, no otras cargas.

### Paso 6: definir cómo presentar

- Tabla con números.
- Gráfico de barras con error bars.
- Gráfico de escala (x=N, y=tiempo, log-log).

### Paso 7: escribir el código

Implementa cada variante con **el mismo esfuerzo**. Si inviertes 4 horas en la
primera, invierte 4 horas en las otras.

### Paso 8: revisión previa

Antes de ejecutar, **pregúntate**: ¿estoy siendo justo con cada variante? ¿Un
experto imparcial aprobaría esta comparación?

Idealmente, pide a alguien que no conoce el resultado esperado que revise el
código. Si ve algo sesgado, ajusta.

### Paso 9: ejecutar con disciplina

Aplica todo lo del T01 (warmup, múltiples runs, control de entorno).

### Paso 10: analizar y presentar

No cherry-pick. Si una variante pierde en un escenario, reportalo. Si ganó pero
por un factor que no esperabas (auto-vectorización, por ejemplo), menciona eso
como caveat.

---

## 7. C vs Rust: reglas del combate justo

Comparar C y Rust es particularmente difícil porque hay tentaciones de cada
lado. Reglas para hacerlo bien:

### 7.1. Release builds, ambos

- C: `-O2` o `-O3`, con `-march=native` si es razonable.
- Rust: `--release` como mínimo, con `lto = true` si el proyecto lo permite.

Nunca compares debug de un lado con release del otro.

### 7.2. Mismas garantías runtime

- Rust elimina bounds checking en la mayoría de los casos con `--release` y
  optimizer. Si el código tiene `unsafe { *p }` o `get_unchecked`, es
  equivalente a C. Si usa `[i]`, es equivalente a C con manual bounds check.
- C con `-fsanitize=bounds` es equivalente a Rust sin `unsafe`.

Decide qué nivel de garantías estás comparando, y aplica el mismo a ambos.

### 7.3. Mismas bibliotecas conceptuales

- C: `qsort` de libc ↔ Rust: `slice::sort` del std.
- C: custom quicksort ↔ Rust: custom quicksort.
- C: vector dinámico manual ↔ Rust: `Vec<T>`.

No compares libc `qsort` con `slice::sort_unstable` sin mencionar que usan
algoritmos distintos (qsort es un quicksort genérico con comparador function
pointer, sort_unstable es pdqsort con comparador monomórfico).

### 7.4. Comparador y genericidad

C `qsort` toma `int (*cmp)(const void*, const void*)` → llamada indirecta, no
se puede inline. Rust `sort_by` toma un closure → monomorfizado, se inlinea.

Esto es una **ventaja estructural de Rust** para comparadores. Mencionarla en
el reporte es honesto. Ocultarla es trampa.

### 7.5. Allocator

Por default:
- C usa `glibc malloc`.
- Rust usa `glibc malloc` también (vía libstdc++).

Mismo allocator → equitativo. Si uno usa jemalloc (via `#[global_allocator]`)
y el otro no, documéntalo.

### 7.6. Unsafe Rust vs C

Usar `unsafe` en Rust permite muchas de las optimizaciones que en C son
default. Comparar Rust 100% safe vs C idiomatic puede inclinar el resultado
artificialmente.

Opciones:
- Comparar Rust 100% safe vs C con `-fsanitize=address,bounds` (equivalentes
  en garantías).
- Comparar Rust con `unsafe` donde necesario vs C idiomatic.

**Documenta la elección** y por qué.

### 7.7. Iterators vs raw loops

Rust `iter().sum()` a menudo genera código igual que un for loop C bien
escrito. Pero no siempre. Antes de comparar, **lee el ensamblador** de ambos
con `cargo asm` y `objdump -d`.

Si el ensamblador es diferente, la comparación revela eso, no "Rust vs C". Si
es idéntico, la comparación se vuelve aburrida pero honesta.

### 7.8. Librería estándar

Rust `BTreeMap` y `std::map` de C++ son conceptualmente similares pero
algorithmically distintos (B-tree vs red-black tree). Los resultados reflejan
la elección de estructura interna, no el lenguaje.

Si tu objetivo es "¿cuál estructura es más rápida para mi uso?", la comparación
es válida. Si tu objetivo es "¿C++ o Rust tienen mejores map?", es un
simplismo.

---

## 8. Implementar el mismo algoritmo en ambos

Supongamos que quieres comparar: "suma de elementos en un array de 10M floats".

### C

```c
/* sum.c */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 10000000

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static float sum_array(const float *arr, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}

int main(void) {
    float *arr = aligned_alloc(64, sizeof(float) * N);
    for (int i = 0; i < N; i++) arr[i] = (float)i / N;

    /* warmup */
    volatile float sink = 0;
    for (int w = 0; w < 5; w++) sink += sum_array(arr, N);

    double t0 = now_sec();
    for (int r = 0; r < 100; r++) sink += sum_array(arr, N);
    double dt = (now_sec() - t0) / 100.0;

    printf("sum=%.6f   time=%.3f ms\n", (double)sink, dt * 1000.0);
    free(arr);
    return 0;
}
```

Compilar: `gcc -O3 -march=native -o sum_c sum.c`

### Rust

```rust
// sum/src/main.rs
use std::time::Instant;

const N: usize = 10_000_000;

fn sum_array(arr: &[f32]) -> f32 {
    let mut s = 0.0_f32;
    for &x in arr {
        s += x;
    }
    s
}

fn main() {
    let mut arr = vec![0.0_f32; N];
    for (i, x) in arr.iter_mut().enumerate() {
        *x = i as f32 / N as f32;
    }

    // warmup
    let mut sink = 0.0_f32;
    for _ in 0..5 {
        sink += sum_array(&arr);
    }

    let t0 = Instant::now();
    for _ in 0..100 {
        sink = std::hint::black_box(sink + sum_array(&arr));
    }
    let dt = t0.elapsed().as_secs_f64() / 100.0;

    println!("sum={}   time={:.3} ms", sink, dt * 1000.0);
}
```

Cargo.toml:

```toml
[package]
name = "sum_rust"
version = "0.1.0"
edition = "2021"

[profile.release]
lto = "thin"
codegen-units = 1
```

Compilar: `cargo build --release`

### Diferencias a notar

1. **Inicialización**: ambos hacen lo mismo.
2. **sum_array**: en C un `for` con `for (int i...)`, en Rust un `for &x in arr`.
   Ambos se optimizan a un loop vectorizado con `-O3`/release.
3. **Warmup**: ambos hacen 5 iteraciones previas.
4. **Medición**: ambos miden 100 iteraciones.
5. **black_box en Rust**: evita que el optimizador elimine el trabajo. En C usamos
   `volatile`. Equivalente funcional.

### ¿Son justos?

Casi. Diferencias sutiles:
- C `aligned_alloc(64, ...)` alinea a 64 bytes (cache line). Rust `Vec::new()`
  no garantiza eso (glibc suele dar 16 bytes). Para equidad total, usar
  `Vec::with_capacity` con allocator aligned, o en la versión C usar `malloc`
  (no aligned) para matchar el comportamiento default de Rust.
- C `clock_gettime(CLOCK_MONOTONIC)` vs Rust `Instant::now()`. Ambos usan el
  mismo clock bajo Linux, equivalente.

### Medir ambos

```bash
# Compilar
gcc -O3 -march=native -o sum_c sum.c
cd sum_rust && cargo build --release && cd ..

# Ejecutar cada uno múltiples veces
for i in 1 2 3 4 5; do ./sum_c; done
for i in 1 2 3 4 5; do ./sum_rust/target/release/sum_rust; done
```

### Qué esperar

En un laptop moderno, ambos alrededor de ~15-20 ms para sumar 10M floats. La
diferencia entre C y Rust debería ser **<5%**, frecuentemente dentro del
margen de ruido. Esto es porque ambos se compilan a prácticamente el mismo
ensamblador con loops vectorizados.

Si ves una diferencia grande (>20%), algo está mal. Investiga:
- ¿Una versión está vectorizando y la otra no?
- ¿Una tiene bounds checking activo?
- ¿Los arrays están alineados igual?

---

## 9. Comparar flags de compilación

Otro tipo de comparativo útil: **mismo código, distintos flags**.

### GCC

```bash
gcc -O0 -o sum_O0 sum.c
gcc -O1 -o sum_O1 sum.c
gcc -O2 -o sum_O2 sum.c
gcc -O3 -o sum_O3 sum.c
gcc -Os -o sum_Os sum.c
gcc -Ofast -o sum_Ofast sum.c
gcc -O3 -march=native -o sum_O3_native sum.c
```

### Mismo programa con cada uno

```bash
for bin in sum_O0 sum_O1 sum_O2 sum_O3 sum_Os sum_Ofast sum_O3_native; do
    echo -n "$bin: "
    ./$bin
done
```

Output típico:

```
sum_O0:  time=60.4 ms
sum_O1:  time=30.1 ms
sum_O2:  time=12.3 ms
sum_O3:  time=8.2 ms
sum_Os:  time=14.7 ms
sum_Ofast: time=8.0 ms
sum_O3_native: time=5.4 ms
```

Lecciones:
- `-O0` vs `-O2`: ~5× de diferencia. El compilador hace mucho.
- `-O2` vs `-O3`: ~1.5× adicional. Vectorización y optimizaciones agresivas.
- `-O3` vs `-O3 -march=native`: ~1.5×. AVX y otras extensiones específicas.
- `-Os` es más lento que `-O2`: optimiza tamaño, no velocidad.
- `-Ofast` es igual a `-O3` en este caso: solo afecta programas con FP
  agresivo.

Estos números son relevantes para **decidir flags de build**. No para concluir
"el compilador es más rápido": estás midiendo cómo el mismo código corre con
distintas optimizaciones.

### Rust equivalente

Rust tiene menos niveles explícitos:

```toml
# release default
[profile.release]
opt-level = 3
lto = false
codegen-units = 16

# max performance
[profile.release-max]
inherits = "release"
opt-level = 3
lto = "fat"
codegen-units = 1

# min size
[profile.release-small]
inherits = "release"
opt-level = "s"

# debug
[profile.dev]
opt-level = 0
```

Variables:
- `opt-level`: 0, 1, 2, 3, "s" (tamaño), "z" (tamaño mínimo).
- `lto`: false, "thin", "fat".
- `codegen-units`: 1 (mejor optimización), 16 (build más rápido).
- `target-cpu`: en `.cargo/config.toml`:
  ```toml
  [build]
  rustflags = ["-C", "target-cpu=native"]
  ```

Comparación típica:

```
opt-level=0:       80 ms  (debug)
opt-level=2:       12 ms
opt-level=3:       8 ms
opt-level=3 + lto="thin": 7.5 ms
opt-level=3 + lto="fat" + cgu=1: 6.8 ms
+ target-cpu=native: 5.3 ms
```

---

## 10. Ejecutar: infraestructura común

Para que un benchmark comparativo sea reproducible, necesitas una
**infraestructura de ejecución común**. Plantilla:

```bash
#!/bin/bash
# bench_runner.sh — runner común para benchmarks comparativos
set -euo pipefail

# ---- configuración del sistema ----
echo "=== System configuration ==="
uname -a
cat /proc/cpuinfo | grep "model name" | head -1
lscpu | grep "CPU(s)" | head -2
free -h | head -2
echo ""

# ---- fijar CPU frequency ----
if command -v cpupower &>/dev/null; then
    sudo cpupower frequency-set -g performance >/dev/null
    echo "Governor set to performance"
fi

# ---- disable turbo boost (opcional, para reproducibilidad máxima) ----
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null
    echo "Turbo disabled"
fi

# ---- CPU aislada ----
CORE=3

run_bench() {
    local name="$1"
    local cmd="$2"
    local runs=5
    echo "=== $name ==="
    for i in $(seq 1 $runs); do
        taskset -c $CORE nice -n -10 $cmd
    done
    echo ""
}

# ---- ejecutar cada variante ----
run_bench "C variant"     "./sum_c"
run_bench "Rust variant"  "./sum_rust/target/release/sum_rust"
run_bench "C++ variant"   "./sum_cpp"

# ---- restaurar ----
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null
fi
```

Ejecutar:

```bash
./bench_runner.sh | tee results.txt
```

El archivo `results.txt` contiene **todo**: config del sistema, versiones,
resultados de cada run. Es el artefacto reproducible.

---

## 11. Recoger datos: qué métricas capturar

No midas solo un número. Captura **todo lo que puedas** para análisis posterior:

### Métricas básicas

- **Tiempo mínimo**: el mejor caso observado.
- **Tiempo mediano (p50)**: caso típico.
- **Tiempo medio (mean)**: promedio, sensible a outliers.
- **Tiempo máximo**: peor caso.
- **Desviación estándar**: dispersión.
- **Coefficient of variation (CV)**: `stddev / mean * 100`. < 2% = estable.

### Percentiles

- **p50, p90, p95, p99**: relevantes para sistemas con variabilidad.

### Métricas derivadas

- **Throughput**: operaciones por segundo = `1 / tiempo_por_op`.
- **Bandwidth**: bytes por segundo = `bytes_procesados / tiempo`.
- **Speedup**: `baseline / variant`.

### Métricas hardware (con perf)

Si `perf stat` está disponible:

```bash
perf stat -r 10 -d ./sum_c
perf stat -r 10 -d ./sum_rust
```

Captura:
- `cycles`
- `instructions`
- `IPC` (instructions per cycle)
- `cache-references, cache-misses`
- `branch-instructions, branch-misses`
- `L1-dcache-loads, L1-dcache-load-misses`

Comparación IPC es reveladora: dos programas que terminan en el mismo tiempo
pueden tener IPC muy distinto (uno memory-bound con IPC 0.5, otro compute-bound
con IPC 3.0), y eso te dice dónde está el bottleneck.

---

## 12. Estadística: media, mediana, IC, percentiles

### Qué reportar

**Nunca** reportes un solo número. Como mínimo: mediana y stddev (o IQR).

### Media vs mediana

- **Media**: sensible a outliers. Si un run se interrumpe por algo externo,
  la media se dispara.
- **Mediana**: robusta a outliers. Representativa del comportamiento "típico".

En benchmarks, **prefiere mediana o mínimo**. El mínimo representa "lo mejor
posible" que el sistema puede hacer, sin interferencia externa. Es la
métrica más estable.

### Intervalo de confianza (IC)

Un IC 95% dice "la media real está entre X e Y con 95% de probabilidad
(asumiendo distribución normal)".

Formula simple:
```
IC = media ± 1.96 * stddev / sqrt(N)
```

Criterion calcula esto automáticamente con bootstrap (no asume normalidad).

**Cómo presentarlo**:
```
C variant:    12.3 ms ± 0.4 ms (95% CI)
Rust variant: 12.5 ms ± 0.5 ms (95% CI)
Speedup:      0.98× (95% CI: [0.93, 1.03])
```

Si el intervalo de confianza del speedup incluye 1.0, **no hay diferencia
significativa**. En el ejemplo de arriba, `[0.93, 1.03]` incluye 1.0 → C y
Rust son estadísticamente indistinguibles.

### Percentiles

Para sistemas donde la **cola** importa (latencia de request), reporta p99:

```
             p50      p95      p99     p99.9
C:          12.3 ms  13.1 ms  14.2 ms  18.7 ms
Rust:       12.5 ms  12.8 ms  13.3 ms  14.1 ms
```

Esto revela algo crucial: las medianas son casi iguales pero la **cola de C
es peor**. Rust tiene menos variabilidad. En un servidor web, eso puede ser
más importante que el p50.

---

## 13. Visualizar resultados: tablas y gráficos

### Tabla simple

```
| Variant     | Time (ms)     | Ratio | IPC  | L1 miss rate |
|-------------|---------------|-------|------|--------------|
| C O2        | 12.3 ± 0.4    | 1.00× | 2.1  | 2.5%         |
| C O3 native | 5.4 ± 0.2     | 2.28× | 3.4  | 2.5%         |
| Rust release| 8.2 ± 0.3     | 1.50× | 2.8  | 2.4%         |
| Rust target-cpu=native | 5.3 ± 0.2 | 2.32× | 3.5 | 2.4% |
```

### Gráfico de barras con error bars

```
            time (ms)
C O2        │████████████ 12.3 ± 0.4
C O3 native │█████ 5.4 ± 0.2
Rust rel    │████████ 8.2 ± 0.3
Rust native │█████ 5.3 ± 0.2
            └────────────────→
            0    5    10   15
```

### Gráfico de escalabilidad (log-log)

Para comparar cómo escalan los algoritmos con N:

```
  time (ms)
     │
100  │                      ★  linear
     │                    / ▲  linear
  10 │                 ★ /
     │              ★  ▲              ● hashmap
   1 │          ★  ▲   ●
     │       ★  ▲      ●
 0.1 │    ★  ▲●
     │   ▲●
0.01 │  ●
     └─────────────────────────────→
     10  100  1K  10K  100K  1M
                    N
```

En escala log-log:
- Una **línea recta** indica escala polinomial (pendiente = exponente).
- O(N) tiene pendiente 1.
- O(N²) tiene pendiente 2.
- O(log N) tiene curva cóncava (va aplanándose).

Para producirlos, Criterion genera PNG/SVG automáticamente en `target/criterion/
*/report/`.

### Gráfico de speedup relativo

Base = variante "baseline". Otras variantes relativas:

```
          Speedup vs C O2
  3.0 ×   │                     ▲ C -O3 -march=native
          │                     ▲ Rust release + target-cpu=native
  2.0 ×   │
          │     ▲ Rust release
  1.5 ×   │
  1.0 ×   │  ━━━━━━━ C O2 (baseline)
          │
  0.5 ×   │
          └──────────────────→
```

El baseline está en 1.0 como referencia horizontal. Todas las demás se leen
relativas a él. Más arriba = mejor.

---

## 14. Comparar AoS vs SoA con rigor

Vamos a hacer una comparación completa siguiendo todas las reglas, para
ilustrar el método.

### Problema

Calcular el módulo de velocidad para N=1M partículas.

### Hipótesis

- **Cambio**: AoS → SoA.
- **Efecto**: Reducir tiempo de `compute_speed` en ≥2×.
- **Mecanismo**: AoS lee 12/32 bytes útiles (37.5%) por cache line. SoA lee
  100%. Ratio teórico 100/37.5 ≈ 2.67×.
- **Condición**: N=1M, L3=25MB (datos caben), compilado con `-O3 -march=native`.
- **Umbral**: confirmo si ratio ≥ 2×, refuto si ≤ 1.5×, investigo entre 1.5 y 2.

### Implementación

```c
/* aos_soa.c */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define N 1000000

/* AoS */
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float mass;
    float _pad;  /* align to 32 bytes */
} Particle;

/* SoA */
typedef struct {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    float *mass;
} ParticleSystem;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void compute_speed_aos(const Particle *p, float *speed, int n) {
    for (int i = 0; i < n; i++) {
        speed[i] = sqrtf(p[i].vx*p[i].vx + p[i].vy*p[i].vy + p[i].vz*p[i].vz);
    }
}

static void compute_speed_soa(const float *vx, const float *vy,
                              const float *vz, float *speed, int n) {
    for (int i = 0; i < n; i++) {
        speed[i] = sqrtf(vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i]);
    }
}

int main(void) {
    /* allocate */
    Particle *p = aligned_alloc(64, sizeof(Particle) * N);
    float *vx = aligned_alloc(64, sizeof(float) * N);
    float *vy = aligned_alloc(64, sizeof(float) * N);
    float *vz = aligned_alloc(64, sizeof(float) * N);
    float *speed = aligned_alloc(64, sizeof(float) * N);

    /* fill with deterministic data */
    srand(42);
    for (int i = 0; i < N; i++) {
        p[i].x = p[i].y = p[i].z = 0.0f;
        p[i].mass = 1.0f;
        p[i].vx = vx[i] = (float)rand() / RAND_MAX;
        p[i].vy = vy[i] = (float)rand() / RAND_MAX;
        p[i].vz = vz[i] = (float)rand() / RAND_MAX;
    }

    const int WARMUP = 10;
    const int RUNS = 100;

    /* warmup */
    for (int w = 0; w < WARMUP; w++) {
        compute_speed_aos(p, speed, N);
        compute_speed_soa(vx, vy, vz, speed, N);
    }

    /* measure AoS */
    double times_aos[RUNS], times_soa[RUNS];
    for (int r = 0; r < RUNS; r++) {
        double t0 = now_sec();
        compute_speed_aos(p, speed, N);
        times_aos[r] = now_sec() - t0;
    }
    for (int r = 0; r < RUNS; r++) {
        double t0 = now_sec();
        compute_speed_soa(vx, vy, vz, speed, N);
        times_soa[r] = now_sec() - t0;
    }

    /* compute stats: min, median, mean, stddev */
    auto_report(times_aos, RUNS, "AoS");
    auto_report(times_soa, RUNS, "SoA");

    free(p); free(vx); free(vy); free(vz); free(speed);
    return 0;
}
```

(Asumiendo `auto_report` hace prints con min/median/mean/stddev.)

### Ejecución

```bash
gcc -O3 -march=native -o aos_soa aos_soa.c -lm

taskset -c 3 ./aos_soa
```

### Resultado típico (laptop moderno)

```
AoS: min=4.123 ms  median=4.234 ms  mean=4.281 ms  stddev=0.087 ms
SoA: min=1.812 ms  median=1.856 ms  mean=1.872 ms  stddev=0.034 ms

Speedup (median): 4.234 / 1.856 = 2.28×
```

**Confirmado**: el ratio (2.28×) está dentro del rango esperado (~2.67×), con
una ligera desviación hacia abajo que podemos atribuir a:
- Prefetcher ayuda algo en AoS (datos secuenciales).
- Overhead de `sqrtf` similar en ambos.

### Verificación adicional con perf stat

```bash
perf stat -d -r 10 ./aos_soa 2>&1 | grep -E "cache|instructions"
```

Debería mostrar:
- `L1-dcache-load-misses`: más bajo en SoA que en AoS.
- `instructions`: similares en ambos (mismo número de operaciones).
- `IPC`: más alto en SoA (bottleneck en memoria para AoS).

### Caveat

Este benchmark mide un workflow donde la estructura AoS tiene **campos
innecesarios**. En una aplicación real, puede que todas las iteraciones usen
también `x`, `y`, `z`, `mass`. En ese caso, el beneficio de SoA es menor
porque cada línea de cache se usa más. El comparativo debe incluir
**ambos casos**:

1. Caso donde SoA brilla (solo vx, vy, vz).
2. Caso menos favorable (todos los campos).

Ocultar el segundo caso es propaganda.

---

## 15. Comparar Vec vs LinkedList

### Problema

Sumar todos los elementos en una colección de 1M enteros, construida
insertando al final.

### Variantes

- `std::vector<int>` (C++)
- `std::list<int>` (C++)
- Custom linked list en C
- Custom contiguous vector en C
- `Vec<i32>` (Rust)
- `LinkedList<i32>` (Rust)

### Hipótesis

- **Vec vs LinkedList** (ambas en C++): Vec 20-50× más rápido para iteración.
- **Mecanismo**: nodos de LinkedList dispersos en heap, cada acceso miss de
  cache. Vec contiguo, prefetcher ayuda.
- **Condición**: 1M elementos, sum total.

### Cuidado especial

- **Construcción**: ambas deben construirse de la misma forma (push_back en
  orden). LinkedList tiene ventaja en inserción al medio, no en al final.
- **Memoria inicial**: Vec con reserva preallocada vs sin reserva cambia
  mucho. Vec con reserva es lo que usas en producción.
- **Allocator**: ambas usan el mismo allocator. Si cambias uno, documéntalo.

### Implementación C++

```cpp
// vec_vs_list.cpp
#include <vector>
#include <list>
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>

constexpr int N = 1'000'000;
constexpr int RUNS = 100;

template<typename Container>
long long sum_container(const Container& c) {
    long long s = 0;
    for (auto x : c) s += x;
    return s;
}

template<typename Container>
double measure(Container& c, const char* name) {
    using clock = std::chrono::high_resolution_clock;
    std::vector<double> times;
    times.reserve(RUNS);

    // warmup
    for (int w = 0; w < 5; w++) (void)sum_container(c);

    for (int r = 0; r < RUNS; r++) {
        auto t0 = clock::now();
        volatile long long s = sum_container(c);
        auto t1 = clock::now();
        (void)s;
        double dt = std::chrono::duration<double>(t1 - t0).count();
        times.push_back(dt);
    }

    std::sort(times.begin(), times.end());
    double median = times[RUNS/2];
    double min = times[0];
    std::cout << name << ": min=" << (min*1000) << "ms median="
              << (median*1000) << "ms\n";
    return median;
}

int main() {
    std::vector<int> v;
    std::list<int> l;
    v.reserve(N);
    for (int i = 0; i < N; i++) {
        v.push_back(i);
        l.push_back(i);
    }

    double t_v = measure(v, "std::vector");
    double t_l = measure(l, "std::list");

    std::cout << "Speedup: " << (t_l / t_v) << "x\n";
    return 0;
}
```

Compilar:
```bash
g++ -O3 -march=native -std=c++17 -o vec_vs_list vec_vs_list.cpp
```

### Resultado típico

```
std::vector: min=0.52 ms median=0.55 ms
std::list:   min=12.3 ms median=13.1 ms
Speedup: 23.8×
```

Confirmado: ~24× de diferencia.

### Por qué tanta diferencia

`std::list` nodos:
- Cada nodo = int (4) + 2 pointers (16) + header allocator (~16) = ~40 bytes.
- 1M nodos = 40 MB, no cabe en L3 (25 MB).
- Cada acceso es miss → ~100 ns DRAM.
- Prefetcher no puede anticipar.

`std::vector`:
- 4M bytes contiguos.
- Cabe en L3.
- Prefetcher trae los siguientes elementos → casi 0 misses efectivos.
- BW efectivo: ~30 GB/s.
- 4 MB / 30 GB/s = 130 µs. Cerca de 0.5 ms incluyendo overhead del loop.

Cálculo teórico: `std::list` ~ 100 ns × 1M = 100 ms (pero en la práctica, la
mitad de los accesos hit en L3/L2 por allocator locality, así que ~50 ms).
`std::vector` ~ 0.5 ms. Ratio teórico ~100×.

Real 24×. ¿Por qué más bajo? Porque el allocator de libstdc++ agrupa los nodos
iniciales, muchos están cerca. En una aplicación real con allocations
intercaladas, la ratio sería mayor.

**Lección**: el benchmark "mejor caso para list" (nodos consecutivos por
allocator fresco) subestima el problema. Un benchmark más realista
intercalaría otras allocations entre cada push_back.

---

## 16. Comparar algoritmos: quicksort vs mergesort vs radix

### Problema

Ordenar 10M enteros uniformes aleatorios.

### Variantes

1. `qsort` de libc (quicksort genérico con function pointer).
2. `std::sort` de libstdc++ (pdqsort = pattern-defeating quicksort).
3. `std::stable_sort` (mergesort).
4. Radix sort custom en C.
5. `slice::sort` de Rust (stable merge sort).
6. `slice::sort_unstable` de Rust (pdqsort).

### Hipótesis

- **qsort más lento**: overhead de function pointer (no se inlinea) → ~2-3×
  más lento que `std::sort`.
- **Radix más rápido para ints**: O(N × k) donde k es el número de dígitos;
  para 32-bit ints son ~4 passes de 8 bits = 4 × N = ~40M operaciones, vs
  N × log N = 10M × 23 = 230M. Esperado ~5× más rápido.
- **stable vs unstable**: unstable ~10-20% más rápido.

### Caveat

Radix sort solo funciona para tipos con dígitos (ints, floats con bit-trick).
`std::sort` y sort de Rust funcionan con cualquier tipo comparable. El
comparativo "radix vs std::sort" es válido **solo si especificas que ambos se
usan para ints**.

### Código

Muy largo para poner aquí. Esqueleto:

```c
// C version
qsort(arr, N, sizeof(int), compare_int);  // libc qsort
```

```cpp
// C++
std::sort(arr, arr + N);
std::stable_sort(arr, arr + N);
```

```rust
// Rust
arr.sort();                // stable
arr.sort_unstable();       // unstable
```

Radix sort custom (C):
```c
void radix_sort_u32(unsigned *arr, int n) {
    unsigned *tmp = malloc(sizeof(unsigned) * n);
    int count[256];

    for (int shift = 0; shift < 32; shift += 8) {
        memset(count, 0, sizeof count);
        for (int i = 0; i < n; i++) count[(arr[i] >> shift) & 0xFF]++;
        for (int i = 1; i < 256; i++) count[i] += count[i-1];
        for (int i = n - 1; i >= 0; i--) {
            int bucket = (arr[i] >> shift) & 0xFF;
            tmp[--count[bucket]] = arr[i];
        }
        memcpy(arr, tmp, sizeof(unsigned) * n);
    }
    free(tmp);
}
```

### Resultado esperado (ordenar 10M uints aleatorios)

```
qsort (libc):          1200 ms
std::stable_sort:      680 ms
std::sort (pdqsort):   540 ms
slice::sort (Rust):    650 ms
slice::sort_unstable:  510 ms
radix_sort (C):        150 ms
```

Speedups relativos a `std::sort`:
- qsort libc: 2.22× más lento.
- std::stable_sort: 1.26× más lento.
- Rust sort: 1.20× más lento.
- Rust sort_unstable: 0.94× (marginal mejora).
- radix custom: 3.6× más rápido.

### Interpretación honesta

1. **qsort libc es ~2× más lento** por la llamada indirecta. Es una **elección
   de API**, no una limitación del lenguaje C. Con un quicksort custom inlined,
   C iguala a std::sort de C++.
2. **std::sort y sort_unstable son equivalentes** entre C++ y Rust (ambos usan
   pdqsort).
3. **Radix es 3.6× más rápido** pero solo porque el tipo permite radix. No
   es una ventaja de C sobre C++, es una ventaja del algoritmo.
4. **stable vs unstable**: ~20% diferencia, consistente con teoría.

Lo que **no** puedes concluir:
- "C es 3.6× más rápido que Rust" → no, radix no es parte de C.
- "Rust es más lento que C++" → diferencias están dentro del margen de
  implementación del pdqsort específico.

---

## 17. Interpretar resultados: qué concluyes y qué no

Una vez tienes números, toca interpretarlos. Regla: **concluye lo específico,
no lo general**.

### Conclusiones válidas (ejemplo AoS vs SoA)

- ✓ "Para N=1M partículas, calcular solo velocidad, SoA es ~2.3× más rápido
  que AoS."
- ✓ "La ventaja de SoA viene de mejor uso de cache lines: lee 100% vs ~37%."
- ✓ "En casos donde todos los campos se usan, la ventaja de SoA desaparece."

### Conclusiones inválidas

- ✗ "SoA es siempre mejor que AoS."
- ✗ "Cualquier programa debería usar SoA."
- ✗ "Reorganizar estructuras siempre da 2×."
- ✗ "Rust/C/C++ es mejor porque permite hacer SoA."

### Generalizar con cuidado

A veces una conclusión general es apropiada **si se formula con precisión**:

- "Para workloads memory-bound donde solo se usa un subset de campos,
  reorganizar en SoA reduce el bandwidth consumido proporcionalmente a la
  fracción de campos usados. En nuestro experimento, usando 3/7 campos, vimos
  2.3× speedup. Resultados similares son esperables en contextos análogos."

Esto es generalización **condicionada**. Es ok porque especifica cuándo
aplica y cuándo no.

---

## 18. Red flags en benchmarks publicados

Cuando leas benchmarks comparativos de otros (blog posts, papers, README de
librerías), busca estos red flags:

### 18.1. Un solo número sin dispersión

```
"Library X es 3× más rápido que library Y."
```

¿3× con qué varianza? ¿3× en el mejor caso o en la media? Sin dispersión, no
hay info.

### 18.2. Solo un input size

```
"Algoritmo X is más rápido en N=100."
```

¿Y en N=10, N=1000, N=100K? Los algoritmos cambian de régimen. Un solo punto
no te dice nada.

### 18.3. Sin especificar hardware

```
"El benchmark muestra 5× mejor en X."
```

¿En qué CPU? Intel Ice Lake y AMD Zen4 pueden dar resultados muy distintos.
Sin hardware, no es reproducible.

### 18.4. Sin especificar compilador/flags

```
"C vs Rust: Rust gana."
```

¿Qué GCC? ¿Qué rustc? ¿`-O2` o `-O3`? ¿LTO? Cada variable puede cambiar el
resultado.

### 18.5. No comparan lo que dicen

Un benchmark "C vs Rust" que en realidad mide "libc qsort vs slice::sort". No
está comparando lenguajes; está comparando API defaults.

### 18.6. Código no publicado

"Mi librería es 2× más rápida." Sin código, imposible verificar. Publicar el
código es obligatorio para que el benchmark sea útil.

### 18.7. Sin margen de error / IC

Si no reportan desviación estándar o intervalo de confianza, no sabes si las
diferencias son ruido.

### 18.8. Comparaciones desiguales

Debug vs release, O0 vs O3, con sanitizers vs sin. Si las condiciones no son
iguales, el resultado no es comparable.

### 18.9. Micro-benchmarks generalizados

"Fibonacci es 10× más rápido en X, por lo que todo el lenguaje X es más rápido."
Micro-benchmarks son útiles para entender componentes, no para generalizar al
lenguaje.

### 18.10. Sin mencionar trade-offs

"X es más rápido" sin mencionar: más memoria, menos seguro, menos legible,
peor soporte, etc. Si no hay trade-offs mencionados, probablemente los hay y
están ocultos.

---

## 19. Trampas clásicas de las comparaciones

### 19.1. "La misma implementación pero..."

"Es el mismo algoritmo, solo la sintaxis cambia." Pero una versión usa `i32` y
la otra `i64`, o una hace copy-on-write y la otra no, o una tiene bounds
checking y la otra no. Pequeñas diferencias tienen grandes efectos.

### 19.2. Cachear resultados

Una versión tiene un cache que la otra no. "X es más rápido" porque X cachea y
no paga el trabajo real. No es comparable.

### 19.3. Evaluación lazy vs eager

Iteradores Rust vs Python: Python ejecuta eagerly en cada paso, Rust lazy con
monomorfización y fusión. No comparable sin detallar.

### 19.4. JIT warmup

Java/JavaScript JIT empiezan lentos y aceleran con warmup. Si mides tiempo
total incluyendo warmup, el resultado es pesimista. Si mides post-warmup, es
optimista. Ambos son válidos pero distintos.

### 19.5. GC pauses

Lenguajes con GC tienen pausas periódicas. Un benchmark de 10 segundos puede
incluir 0 o 10 GCs dependiendo del workload. La mediana es engañosa; el p99 y
el peor caso son más representativos.

### 19.6. IO buffering

Comparar "imprimir 1M líneas" en distintos lenguajes: uno usa buffer grande
por default, otro pequeño. La diferencia medida no es del lenguaje sino del
buffer size default.

### 19.7. Different startup cost

Un binario C tarda 1 ms en startup, un programa Java tarda 500 ms por JVM
init. Para benchmarks de 10 ms, Java parece 50× peor. Para benchmarks de 10 s,
la diferencia se diluye a 5%.

### 19.8. Comparar compilers de distinto año

GCC 13 vs GCC 4.9: incluso el mismo código da resultados muy distintos. Fija
versiones.

### 19.9. Sin inicialización caliente

Primera ejecución de cualquier cosa es más lenta por cold cache, first-touch
page, lazy linking. Sin warmup, mides setup, no trabajo.

### 19.10. Benchmarks que midieron distinto

Implementación A mide con `clock()` (CPU time), B con `clock_gettime(MONOTONIC)`
(wall clock). Son métricas distintas. A puede excluir sleep time, B no.

---

## 20. Reproducibilidad: qué documentar

Para que un comparativo sea reproducible, documenta **todo**:

### Mínimo indispensable

- **Versión de los compiladores**: `gcc --version`, `rustc --version`.
- **Flags de compilación** completos.
- **OS y versión del kernel**: `uname -a`, `cat /etc/os-release`.
- **CPU model**: `cat /proc/cpuinfo | grep 'model name' | head -1`.
- **RAM total**: `free -h`.
- **CPU governor**: `cpupower frequency-info`.
- **Turbo/boost habilitado**: sí/no.
- **Proceso en segundo plano**: si hay otras cargas.

### Código

Publica el código completo, no fragmentos. Idealmente en un repositorio git
con un tag específico.

### Datos crudos

No publiques solo las medias. Publica los runs individuales. Otros pueden
re-analizar con otras estadísticas.

### Scripts de ejecución

Un `benchmark.sh` que cualquiera puede ejecutar para reproducir. Debe incluir:
- Cómo compilar.
- Cómo ejecutar.
- Cómo recoger resultados.

### Scripts de análisis

Si hiciste procesamiento especial (outliers removidos, bootstrap), publica el
código. Si no, el lector no puede verificar.

### Ejemplo de bloque de reproducibilidad

```markdown
## Reproducibility

All benchmarks were run on:
- **CPU**: Intel Core i7-12700H @ 2.3 GHz (base), 4.7 GHz (boost)
- **RAM**: 32 GB DDR4 3200 MHz
- **OS**: Fedora 40 (kernel 6.8.10-300.fc40.x86_64)
- **CPU governor**: performance
- **Turbo**: disabled (for reproducibility)
- **Taskset**: pinned to CPU 3

Compilers:
- **gcc** (GCC) 14.0.1
- **rustc** 1.78.0
- **clang** 18.1.2

Compile commands:
```bash
gcc -O3 -march=native -flto -o bench_c bench.c -lm
rustc -C opt-level=3 -C target-cpu=native -C lto bench.rs
```

Raw data: see `results/raw/` in the repo.

To reproduce: clone the repo, run `./scripts/benchmark.sh`, and compare with
`results/reference.json`.
```

---

## 21. Presentación honesta: el buen reporte

Un buen reporte de benchmark comparativo tiene la siguiente estructura:

### 21.1. Resumen (TL;DR)

Una frase con el resultado principal y sus condiciones.

> "Para sumar 10M floats, C y Rust dan rendimiento equivalente (±3%) en
> builds release con `-O3 -march=native`. Compilados sin vectorización
> (`-O1`), C es 1.4× más rápido por mejor loop unrolling del compilador
> en este caso específico."

### 21.2. Contexto y motivación

Por qué hiciste la comparación. Qué pregunta responde. Por qué importa.

### 21.3. Metodología

- Qué se midió.
- Cómo se midió.
- Qué se mantuvo constante.
- Qué se varió.

### 21.4. Resultados

Tablas, gráficos, números crudos. No interpretar aquí, solo presentar.

### 21.5. Análisis

Interpretar los resultados. Por qué son así. Qué mecanismos están en juego.

### 21.6. Caveats y limitaciones

**Esta sección es crítica**. Lista honestamente:
- Qué no cubre el benchmark.
- Qué variables no controlaste.
- Qué resultados no son generalizables.
- Qué habría que investigar más.

Un reporte sin sección de caveats es sospechoso.

### 21.7. Reproducibilidad

Todo lo del §20.

### 21.8. Conclusión

Qué aprendimos. Qué recomendación (si aplica) damos, especificando bajo qué
condiciones. Qué no podemos concluir.

Un error típico: concluir más de lo que el experimento sostiene. Si mediste
un solo N con un solo compilador, no digas "X es mejor que Y en general".
Di "X fue mejor que Y en N=1M con gcc 14.0 en Intel i7-12700H".

---

## 22. Errores comunes al publicar comparativos

### Error 1 — título clickbait

"Rust es 50× más rápido que C en X" para un micro-benchmark artificial. Clickbait
es enemigo de la honestidad. El título debe reflejar el hallazgo real, no su
versión exagerada.

### Error 2 — mostrar solo el mejor caso

Si mediste 10 variantes y solo publicas la que ganó, ocultas información. Publica
todas las variantes que probaste, incluso las perdedoras.

### Error 3 — unidades inconsistentes

Gráfico con "tiempo (ms)" y otro con "ops/s" en el mismo reporte. Difícil
comparar visualmente. Elige una unidad consistente.

### Error 4 — escalas engañosas

Gráfico de barras con eje Y empezando en 10 en lugar de 0. Una diferencia de
"10%" parece "el doble" porque la barra más pequeña no empieza en cero. Es
manipulación visual.

### Error 5 — no mencionar errores cometidos

Si durante el benchmark descubriste un bug en tu implementación, menciónalo.
"Inicialmente vi 10× diferencia, pero era por un bug en mi versión X. Tras
corregirlo, la diferencia es 1.5×." Esto es información valiosa.

### Error 6 — falsa precisión

"X es 2.347× más rápido". ¿Tres cifras significativas? ¿Puedes distinguir
2.347 de 2.348? Probablemente no con tu varianza. Usa 2 cifras significativas
máximo.

### Error 7 — ignorar empeoras en otros ejes

"X mejora latency 2×". Pero ocupa 5× más memoria. Si no lo mencionas,
engañas. Todos los trade-offs deben ser visibles.

### Error 8 — no reproducir en otro hardware

Benchmark corrido en una sola máquina. Si es posible, corre en 2-3 máquinas
distintas para verificar que el resultado es robusto.

### Error 9 — benchmark de componentes aislados sin integración

"Mi sort es 2× más rápido." Pero integrado en una aplicación real donde sort
es solo el 5% del tiempo, la mejora global es 1%. Si la aplicación real no
se menciona, el lector asume mejora global.

### Error 10 — no invitar a refutación

Un buen comparativo dice "aquí están mis números, aquí está el código, por
favor intenten reproducir y avísenme si encuentran errores". Eso invita a
verificación y mejora el conocimiento colectivo.

---

## 23. Herramientas auxiliares: hyperfine, poop, cargo-show-asm

### hyperfine

Herramienta CLI para benchmark rápido de comandos. Perfecta para comparar
binarios sin escribir runners custom.

```bash
cargo install hyperfine

hyperfine \
  --warmup 3 \
  --runs 50 \
  './sum_c' \
  './sum_rust/target/release/sum_rust'
```

Output:
```
Benchmark 1: ./sum_c
  Time (mean ± σ):      12.3 ms ±   0.4 ms    [User: 12.1 ms, System: 0.2 ms]
  Range (min … max):    11.8 ms …  13.1 ms    50 runs

Benchmark 2: ./sum_rust/target/release/sum_rust
  Time (mean ± σ):      12.5 ms ±   0.5 ms    [User: 12.3 ms, System: 0.2 ms]
  Range (min … max):    11.9 ms …  13.4 ms    50 runs

Summary
  ./sum_c ran
    1.02 ± 0.05 times faster than ./sum_rust/target/release/sum_rust
```

Exporta a JSON, CSV, Markdown:

```bash
hyperfine --export-json results.json './bin1' './bin2'
hyperfine --export-markdown results.md './bin1' './bin2'
```

### poop

Alternativa moderna escrita en Zig. Muestra **deltas** en lugar de solo tiempo:

```bash
poop './sum_c' './sum_rust/target/release/sum_rust'
```

Output con `peak_rss`, `cpu_cycles`, `instructions`, `cache_references`,
`cache_misses`, `branch_misses`. Mucha más información que hyperfine para
comparativas de performance.

### cargo-show-asm

Para ver el ensamblador generado por Rust sin manualmente extraerlo:

```bash
cargo install cargo-show-asm

cargo asm my_crate::my_function
```

Útil para **verificar** que tu hipótesis sobre qué genera el compilador es
correcta.

### compiler explorer (godbolt.org)

No es CLI, pero es la herramienta web para comparar lo que distintos
compiladores/flags/versiones generan para un mismo código. Para comparativos
**pequeños**, inmejorable.

### criterion.rs con Python plots

Criterion genera SVGs automáticamente. Si quieres customizarlos, el directorio
`target/criterion/my_bench/` tiene CSVs que puedes cargar en pandas/matplotlib.

---

## 24. Benchmark games: lecciones del Computer Language Benchmarks Game

El **Computer Language Benchmarks Game** (https://benchmarksgame-team.pages.debian.net/)
es un comparativo público entre lenguajes para ~10 benchmarks clásicos
(fannkuch-redux, mandelbrot, n-body, etc.).

Lecciones útiles:

### 24.1. Multiple implementations per language

Cada lenguaje tiene múltiples implementaciones: idiomática, optimizada,
ultra-optimizada. Muestra que "el rendimiento del lenguaje" es un concepto
vacío sin especificar cuál de las muchas implementaciones.

### 24.2. Source code publicado

Todo el código está en GitHub. Cualquiera puede reproducir, verificar,
proponer mejoras. Modelo de benchmark honesto.

### 24.3. Reproducibility strict

Scripts de build y ejecución exactos, versiones fijas, hardware documentado.
No hay "trust me".

### 24.4. Pero... también limitaciones

- Los benchmarks son sintéticos. No reflejan aplicaciones reales.
- La optimización extrema puede llevar a código "ilegible" en producción. Una
  implementación que gana el benchmark puede ser impráctica para mantener.
- Algunos problemas favorecen ciertos paradigmas. Mandelbrot favorece SIMD,
  que ciertos lenguajes usan mejor por default.

### 24.5. Aprendizaje

Leer el código ganador de distintos lenguajes para un mismo problema enseña
mucho sobre cómo optimizar en cada uno. Es un ejercicio pedagógico excelente.

### 24.6. Qué NO concluir

"Lenguaje X es el más rápido." No. "Para estos ~10 problemas específicos,
con estas implementaciones optimizadas, X ganó K veces."

---

## 25. Caso de estudio: suma de vector C vs Rust

Vamos a hacer un caso completo, end-to-end, para ilustrar la metodología.

### Pregunta

¿Es Rust `Vec<f32>::iter().sum()` tan rápido como un loop manual en C para
sumar 10M floats?

### Hipótesis

- **Predicción**: Rust y C dentro de ±10%.
- **Mecanismo**: ambos se compilan a loops vectorizados con AVX en release.
- **Umbral**: confirmo si la diferencia es <10%. Si >20%, investigo.

### Implementación C

```c
/* sum_c.c */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 10000000
#define RUNS 100

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static float sum_array(const float *arr, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}

int main(void) {
    float *arr = aligned_alloc(64, sizeof(float) * N);
    for (int i = 0; i < N; i++) arr[i] = (float)i / N;

    volatile float sink = 0;
    for (int w = 0; w < 10; w++) sink += sum_array(arr, N);

    double times[RUNS];
    for (int r = 0; r < RUNS; r++) {
        double t0 = now_sec();
        sink += sum_array(arr, N);
        times[r] = now_sec() - t0;
    }

    /* min, mean, stddev */
    double min = times[0], sum = 0, sum2 = 0;
    for (int i = 0; i < RUNS; i++) {
        if (times[i] < min) min = times[i];
        sum += times[i];
        sum2 += times[i] * times[i];
    }
    double mean = sum / RUNS;
    double var = sum2 / RUNS - mean * mean;
    double stddev = sqrt(var);

    printf("C  min=%.3f ms mean=%.3f ms stddev=%.3f ms\n",
           min*1000, mean*1000, stddev*1000);
    (void)sink;
    free(arr);
    return 0;
}
```

### Implementación Rust

```rust
// sum_rust/src/main.rs
use std::time::Instant;

const N: usize = 10_000_000;
const RUNS: usize = 100;

fn sum_array(arr: &[f32]) -> f32 {
    arr.iter().sum()
}

fn main() {
    let arr: Vec<f32> = (0..N).map(|i| i as f32 / N as f32).collect();

    let mut sink = 0.0f32;
    for _ in 0..10 {
        sink += sum_array(&arr);
    }

    let mut times = Vec::with_capacity(RUNS);
    for _ in 0..RUNS {
        let t0 = Instant::now();
        sink = std::hint::black_box(sink + sum_array(&arr));
        times.push(t0.elapsed().as_secs_f64());
    }

    let min = times.iter().cloned().fold(f64::INFINITY, f64::min);
    let mean: f64 = times.iter().sum::<f64>() / RUNS as f64;
    let var: f64 = times.iter().map(|t| (t - mean).powi(2)).sum::<f64>() / RUNS as f64;
    let stddev = var.sqrt();

    println!("Rust min={:.3} ms mean={:.3} ms stddev={:.3} ms",
             min*1000.0, mean*1000.0, stddev*1000.0);
    let _ = sink;
}
```

### Compilación

```bash
# C
gcc -O3 -march=native -o sum_c sum_c.c -lm

# Rust
cd sum_rust
RUSTFLAGS="-C target-cpu=native" cargo build --release
cd ..
```

### Ejecución

```bash
sudo cpupower frequency-set -g performance
taskset -c 3 ./sum_c
taskset -c 3 ./sum_rust/target/release/sum_rust
```

### Resultado (laptop moderno típico)

```
C  min=5.412 ms mean=5.528 ms stddev=0.102 ms
Rust min=5.389 ms mean=5.547 ms stddev=0.118 ms
```

**Speedup**: 5.547 / 5.528 ≈ 1.003 (dentro del ruido).

**Interpretación**: equivalentes. Diferencia es <1%, dentro del margen de
error. Confirma la hipótesis.

### Verificación con `perf stat`

```bash
perf stat -d ./sum_c 2>&1 | grep instructions
perf stat -d ./sum_rust/target/release/sum_rust 2>&1 | grep instructions
```

Número de instrucciones debería ser casi idéntico. Si lo es, es evidencia
adicional de que ambos compiladores generan código equivalente.

### Verificación con `objdump` / `cargo-asm`

```bash
objdump -d sum_c | grep -A 30 "sum_array"
cargo asm --target-dir sum_rust/target sum_rust::sum_array
```

Ambos deberían mostrar loops con `vaddps` (AVX add packed singles), `vmovups`
(unaligned load), y un unrolling similar.

### Reporte final

```markdown
# Sum of 10M f32 array: C vs Rust

## TL;DR
C and Rust perform equivalently (±2%) for summing 10M f32 values in a
contiguous array, compiled with `-O3 -march=native` / `target-cpu=native`.

## Setup
- CPU: Intel i7-12700H @ 4.7 GHz (boost)
- OS: Fedora 40
- gcc 14.0.1, rustc 1.78.0

## Measurement
- 10 warmup runs + 100 measurement runs.
- Pinned to core 3, governor performance.
- Median, min, stddev reported.

## Results
| Variant | min (ms) | mean (ms) | stddev (ms) |
|---------|----------|-----------|-------------|
| C       | 5.412    | 5.528     | 0.102       |
| Rust    | 5.389    | 5.547     | 0.118       |

## Analysis
Both compiled to equivalent AVX2 vectorized loops. See `sum_c.asm` and
`sum_rust.asm` for the generated code.

## Caveats
- Only tested on Intel i7-12700H. Different CPUs may show different ratios.
- Only f32 sum; other operations may differ.
- Both used default allocator (glibc). Custom allocators not tested.

## Reproduction
See `./bench.sh`. Raw data in `results/`.

## Conclusion
For this specific workload, choice of language (C vs Rust) does not impact
performance. Other factors (maintainability, safety, etc.) are likely more
important for the decision.
```

Esto es lo que un comparativo honesto se ve.

---

## 26. Programa de práctica: comparative_lab/

Un proyecto con varios comparativos listos para ejecutar y documentar.

### Estructura propuesta

```
comparative_lab/
├── README.md                 # descripción general
├── bench.sh                  # runner principal
├── results/
│   └── .gitkeep
├── aos_vs_soa/
│   ├── aos_soa.c
│   ├── Makefile
│   └── hypothesis.md
├── vec_vs_list/
│   ├── vec_list.cpp
│   ├── Makefile
│   └── hypothesis.md
├── c_vs_rust_sum/
│   ├── sum_c.c
│   ├── sum_rust/
│   │   ├── Cargo.toml
│   │   └── src/main.rs
│   ├── Makefile
│   └── hypothesis.md
└── sort_algorithms/
    ├── qsort_bench.c
    ├── stdsort_bench.cpp
    ├── rust_sort/
    └── hypothesis.md
```

Cada subdirectorio es un comparativo autocontenido con:
- Código fuente de todas las variantes.
- `Makefile` para compilar.
- `hypothesis.md` con la hipótesis pre-registrada.
- `results/` (generada al ejecutar).

### bench.sh

```bash
#!/bin/bash
set -euo pipefail

# CPU setup
sudo cpupower frequency-set -g performance >/dev/null 2>&1 || true
CORE=3

run_subdir() {
    local dir="$1"
    echo "=== $dir ==="
    (cd "$dir" && make && taskset -c $CORE ./bench_runner)
    echo ""
}

run_subdir aos_vs_soa
run_subdir vec_vs_list
run_subdir c_vs_rust_sum
run_subdir sort_algorithms
```

### hypothesis.md template

```markdown
# Hypothesis: <título>

**Date**: YYYY-MM-DD
**Author**: tu nombre

## Question
Una frase con la pregunta concreta.

## Hypothesis
- Change:
- Effect:
- Magnitude:
- Mechanism:
- Condition:
- Threshold:

## Napkin math
[cálculo de orden de magnitud]

## Prediction
| Variant | Expected time | Expected ratio |
|---------|---------------|----------------|
| ...     | ...           | ...            |

## Experiment plan
- Inputs:
- Tools:
- Metrics:

## Results (to be filled)

## Analysis (to be filled)

## Conclusion (to be filled)
```

Flujo:
1. Estudia el código.
2. Llena `hypothesis.md` antes de ejecutar.
3. Ejecuta `./bench.sh`.
4. Compara resultados con tu predicción.
5. Documenta en `hypothesis.md` las secciones de resultados, análisis y
   conclusión.

Esto es **el método completo** aplicado a comparativos.

---

## 27. Ejercicios

### Ejercicio 1 — AoS vs SoA full

1. Implementa el código del §14.
2. Escribe la hipótesis **antes** de ejecutar.
3. Ejecuta y compara con la predicción.
4. Si no coincide, profilea con `perf stat -d` y busca la causa.
5. Redacta un reporte con la estructura del §21.

### Ejercicio 2 — C vs Rust para una operación nueva

Elige una operación distinta a la suma (por ejemplo, calcular el hash MD5 o
SHA-256 de un archivo, o resolver el problema de N-queens). Implementa en C y
en Rust, aplicando todo el rigor del T02.

Preguntas a responder:
- ¿Cuál es más rápido?
- ¿Dentro de qué margen?
- ¿Por qué? (análisis con perf y ensamblador).

### Ejercicio 3 — Comparar sort algorithms

Implementa:
- `qsort` de libc.
- `std::sort` de C++.
- `slice::sort_unstable` de Rust.
- Radix sort custom (solo para ints).

Mide para N ∈ {1K, 10K, 100K, 1M, 10M} con enteros uniformes aleatorios.

Grafica los resultados en escala log-log. ¿Los algoritmos escalan como Big-O
predice?

### Ejercicio 4 — Leer un benchmark publicado

Busca un blog post con un benchmark "X vs Y" (por ejemplo, "actix vs axum" o
"tokio vs async-std"). Lee con ojo crítico y anota:
- ¿Qué falta? (setup, caveats, código, etc.)
- ¿Qué red flags del §18 aparecen?
- ¿Es reproducible?
- ¿Qué conclusiones están justificadas y cuáles no?

### Ejercicio 5 — Reproducir un benchmark publicado

Elige un benchmark del Benchmarks Game que te interese. Descarga el código.
Reproduce en tu máquina. ¿Coinciden los números con los publicados? Si no,
¿por qué?

### Ejercicio 6 — Escribir un reporte honesto

Toma cualquiera de los experimentos anteriores y escribe un reporte completo
siguiendo la estructura del §21. Revisa críticamente:
- ¿Mi TL;DR es específico (no exagerado)?
- ¿Mencioné todos los caveats?
- ¿Mis conclusiones son conservadoras?
- ¿Cualquier persona puede reproducir con lo documentado?

### Ejercicio 7 — Peer review

Intercambia tu reporte del Ejercicio 6 con un compañero. Cada uno lee el del
otro y busca:
- ¿Alguna conclusión no sostenida?
- ¿Algún caveat faltante?
- ¿Algún red flag?

### Ejercicio 8 — hyperfine wrapper

Usa hyperfine para comparar 2-3 binarios de `comparative_lab`. Exporta a JSON
y crea un gráfico con Python/matplotlib. Compara el resultado con lo que te
daría criterion o un runner custom.

---

## 28. Checklist del comparativo honesto

Antes de empezar:

- [ ] La pregunta es **específica** (no general).
- [ ] Definí **todas** las variantes a comparar.
- [ ] Definí los inputs (tamaños, tipos, distribuciones).
- [ ] Definí las métricas a capturar.
- [ ] Definí las condiciones de hardware/software.
- [ ] Escribí la hipótesis **antes** de ver cualquier resultado.

Durante implementación:

- [ ] Mismo esfuerzo invertido en cada variante.
- [ ] Mismos flags de compilación (o documentada la diferencia).
- [ ] Mismas librerías (o documentada la diferencia).
- [ ] Mismas garantías runtime (o documentada la diferencia).
- [ ] Código revisado para equidad (idealmente por alguien imparcial).

Durante medición:

- [ ] CPU governor fijo (performance).
- [ ] Taskset a core dedicado.
- [ ] Warmup suficiente.
- [ ] Múltiples runs (≥30).
- [ ] Variantes intercaladas (no todas A luego todas B).
- [ ] Recogí min, media, mediana, stddev, p95, p99.

Después:

- [ ] No cherry-pick: publiqué todos los resultados.
- [ ] Reporté con IC o stddev, no solo un número.
- [ ] Mencioné la varianza.
- [ ] Interpretación conservadora: solo concluí lo específico.
- [ ] Mencioné explícitamente qué NO concluir.
- [ ] Listé caveats y limitaciones.
- [ ] Proporcioné código, scripts y raw data para reproducir.
- [ ] Verifiqué con ensamblador y/o perf stat si la diferencia es sospechosa.
- [ ] Si la hipótesis fue refutada: documenté el aprendizaje.

---

## 29. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T01 — Formular hipótesis](../T01_Formular_hipotesis/README.md) | [S03 — Benchmark guiado](../) | [T03 — Regression benchmarks en CI](../T03_Regression_benchmarks_en_CI/README.md) |

**Capítulo 4 — Sección 3: Benchmarking Guiado por Hipótesis — Tópico 2 de 3**

Has aprendido cómo diseñar, ejecutar y presentar benchmarks comparativos con
rigor e integridad intelectual. La regla de oro: **el resultado del benchmark
no debe estar predeterminado por tus sesgos**. Si pones el mismo cuidado en
cada variante, eliminas confirmation bias, publicas el código, documentas las
limitaciones, y concluyes solo lo específico, has producido algo útil para
la comunidad. En el siguiente tópico (T03 Regression benchmarks en CI)
llevaremos estas herramientas al terreno de los **procesos continuos**:
cómo detectar automáticamente cuando una PR introduce una regresión de
rendimiento, cómo integrar criterion/iai con GitHub Actions, y cómo alertar
sin generar ruido.
