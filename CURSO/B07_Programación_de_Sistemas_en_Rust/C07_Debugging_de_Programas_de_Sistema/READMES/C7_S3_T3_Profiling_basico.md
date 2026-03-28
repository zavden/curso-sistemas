# Profiling básico: perf, flame graphs e inferno

## Índice

1. [¿Qué es profiling?](#qué-es-profiling)
2. [perf stat: contadores de rendimiento](#perf-stat-contadores-de-rendimiento)
3. [perf record y perf report: muestreo de CPU](#perf-record-y-perf-report-muestreo-de-cpu)
4. [Flame graphs: visualización de perfiles](#flame-graphs-visualización-de-perfiles)
5. [Inferno: flame graphs en Rust](#inferno-flame-graphs-en-rust)
6. [CPU-bound vs I/O-bound](#cpu-bound-vs-io-bound)
7. [Profiling de memoria](#profiling-de-memoria)
8. [Profiling en Rust: particularidades](#profiling-en-rust-particularidades)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es profiling?

Profiling es **medir** dónde un programa gasta su tiempo o recursos. No es
debugging (buscar qué está mal) sino **optimización** (buscar qué es lento).

```
┌──────────────────────────────────────────────────────────────┐
│        Debugging vs Profiling                                │
│                                                              │
│  Debugging:  "¿Por qué crashea?"   → Corregir el bug        │
│  Profiling:  "¿Por qué es lento?"  → Optimizar el cuello    │
│                                       de botella             │
│                                                              │
│  Regla de oro:                                               │
│  1. Primero hacer que funcione (debugging)                   │
│  2. Luego hacer que sea rápido (profiling)                   │
│  3. "Premature optimization is the root of all evil" — Knuth │
└──────────────────────────────────────────────────────────────┘
```

### Tipos de profilers

```
┌──────────────────────────────────────────────────────────────┐
│  Tipo           │ Cómo funciona        │ Ejemplo             │
├─────────────────┼──────────────────────┼─────────────────────┤
│ Sampling        │ Toma "fotos" de la   │ perf record         │
│                 │ pila de llamadas     │                     │
│                 │ cada N microsegundos │                     │
├─────────────────┼──────────────────────┼─────────────────────┤
│ Instrumentación │ Inserta código en    │ callgrind (Valgrind)│
│                 │ cada función para    │ gprof               │
│                 │ medir entradas y     │                     │
│                 │ salidas              │                     │
├─────────────────┼──────────────────────┼─────────────────────┤
│ Contadores HW   │ Lee contadores del   │ perf stat           │
│                 │ procesador (cache    │                     │
│                 │ misses, branches...) │                     │
├─────────────────┼──────────────────────┼─────────────────────┤
│ Tracing         │ Registra cada evento │ strace, ltrace,     │
│                 │ individualmente      │ perf trace          │
└─────────────────┴──────────────────────┴─────────────────────┘
```

### Cuándo usar cada uno

```
┌──────────────────────────────────────────────────────────────┐
│  Pregunta                            │ Herramienta           │
├──────────────────────────────────────┼───────────────────────┤
│ "¿Mi programa es CPU o I/O bound?"   │ perf stat             │
│ "¿Qué función consume más CPU?"      │ perf record + report  │
│ "¿Dónde está el cuello de botella    │ Flame graph           │
│  visualmente?"                       │                       │
│ "¿Cuántos cache misses tengo?"       │ perf stat -e cache-*  │
│ "¿Cuánta memoria usa mi programa?"   │ Valgrind massif,      │
│                                      │ heaptrack             │
│ "¿Qué syscalls son lentas?"          │ strace -T, perf trace │
│ "¿Mi código Rust es tan rápido       │ cargo bench,          │
│  como debería?"                      │ criterion             │
└──────────────────────────────────────┴───────────────────────┘
```

---

## perf stat: contadores de rendimiento

`perf stat` ejecuta un programa y reporta **contadores de hardware** del
procesador: cuántas instrucciones ejecutó, cuántos ciclos, cuántos cache
misses, etc.

### Uso básico

```bash
# Contadores por defecto
perf stat ./target/release/my_program < input.txt
```

```
 Performance counter stats for './target/release/my_program':

       1,234.56 msec  task-clock                # 0.987 CPUs utilized    ←①
              3       context-switches           # 2.430 /sec            ←②
              0       cpu-migrations             # 0.000 /sec
          4,521       page-faults                # 3.662 K/sec           ←③
  3,456,789,012       cycles                     # 2.800 GHz             ←④
  5,678,901,234       instructions               # 1.64  insn per cycle  ←⑤
  1,234,567,890       branches                   # 1.000 G/sec
     12,345,678       branch-misses              # 1.00% of all branches ←⑥

       1.25034 seconds time elapsed                                      ←⑦
       1.20012 seconds user time                                         ←⑧
       0.04890 seconds sys time                                          ←⑨
```

### Interpretar los números

```
┌────────────────────────────────────────────────────────────────┐
│  Métrica               │ Qué significa                        │
├────────────────────────┼──────────────────────────────────────┤
│ ① task-clock           │ Tiempo de CPU usado. Si CPUs         │
│   # 0.987 CPUs         │ utilized ≈ 1.0 → CPU-bound.         │
│                        │ Si << 1.0 → I/O-bound o idle        │
├────────────────────────┼──────────────────────────────────────┤
│ ② context-switches     │ Cuántas veces el kernel sacó el      │
│                        │ proceso del CPU. Muchos = contención │
├────────────────────────┼──────────────────────────────────────┤
│ ③ page-faults          │ Accesos a memoria que requirieron    │
│                        │ cargar la página del disco. Alto      │
│                        │ al inicio (normal), alto constante   │
│                        │ (posible problema de memoria)        │
├────────────────────────┼──────────────────────────────────────┤
│ ④ cycles               │ Ciclos de reloj del CPU. GHz es      │
│                        │ la frecuencia efectiva               │
├────────────────────────┼──────────────────────────────────────┤
│ ⑤ instructions         │ Instrucciones ejecutadas. IPC        │
│   # 1.64 insn/cycle    │ (Instructions Per Cycle):            │
│                        │ > 2.0 → excelente                   │
│                        │ 1.0-2.0 → normal                    │
│                        │ < 1.0 → stalls (cache misses,       │
│                        │ branch mispredictions)               │
├────────────────────────┼──────────────────────────────────────┤
│ ⑥ branch-misses        │ % de predicciones de branch          │
│   # 1.00%              │ incorrectas. > 5% → datos            │
│                        │ impredecibles o muchos ifs           │
├────────────────────────┼──────────────────────────────────────┤
│ ⑦ time elapsed         │ Tiempo real (wall clock)             │
│ ⑧ user time            │ Tiempo en código de usuario          │
│ ⑨ sys time             │ Tiempo en el kernel (syscalls)       │
│                        │ sys >> user → demasiadas syscalls    │
└────────────────────────┴──────────────────────────────────────┘
```

### Contadores específicos

```bash
# Contadores de caché
perf stat -e cache-references,cache-misses,L1-dcache-load-misses \
    ./target/release/my_program

# Contadores de branch prediction
perf stat -e branches,branch-misses \
    ./target/release/my_program

# Contadores de TLB (Translation Lookaside Buffer)
perf stat -e dTLB-loads,dTLB-load-misses \
    ./target/release/my_program

# Ver todos los eventos disponibles
perf list
```

### Comparar antes y después de una optimización

```bash
# Antes
perf stat -r 5 ./target/release/my_program_v1 < input.txt
#  -r 5 = ejecutar 5 veces y promediar

# Después
perf stat -r 5 ./target/release/my_program_v2 < input.txt

# Comparar IPC, cache-misses, y tiempo total
```

---

## perf record y perf report: muestreo de CPU

`perf record` toma muestras de la pila de llamadas mientras el programa corre,
y `perf report` muestra dónde se gastó el tiempo.

### Grabar un perfil

```bash
# Compilar con símbolos de debug (necesario para ver nombres de funciones)
# En Rust: release con debug symbols
# Cargo.toml:
# [profile.release]
# debug = true

cargo build --release

# Grabar perfil
perf record -g ./target/release/my_program < input.txt
# -g = incluir call graphs (pila de llamadas)

# Se crea perf.data en el directorio actual
ls -lh perf.data
```

### Analizar con perf report

```bash
perf report
```

Se abre una interfaz interactiva en terminal:

```
Overhead  Command     Shared Object      Symbol
  45.23%  my_program  my_program         [.] parse_record
  23.11%  my_program  my_program         [.] normalize_value
  12.45%  my_program  libc.so.6          [.] __memcpy_avx2
   8.67%  my_program  my_program         [.] write_output
   5.34%  my_program  [kernel.kallsyms]  [k] copy_user_enhanced
   3.20%  my_program  my_program         [.] main
   2.00%  my_program  libc.so.6          [.] malloc
```

Interpretación:
- **Overhead**: porcentaje de muestras en esa función (más = más tiempo)
- **`parse_record` al 45%**: esta función consume casi la mitad del tiempo
  → primer candidato para optimizar
- **`__memcpy_avx2` al 12%**: copias de memoria significativas → buscar
  dónde se copian datos innecesariamente
- **`[kernel.kallsyms]`**: tiempo en el kernel → syscalls
- **`malloc` al 2%**: alocaciones frecuentes

### Navegación en perf report

```
┌──────────────────────────────────────────────────────────────┐
│  Tecla        │ Acción                                       │
├───────────────┼──────────────────────────────────────────────┤
│ Enter         │ Expandir la función (ver callers/callees)    │
│ +/-           │ Expandir/colapsar                            │
│ a             │ Anotar: ver el código fuente con porcentajes │
│ /             │ Buscar un símbolo                            │
│ q             │ Salir                                        │
│ H             │ Mostrar ayuda                                │
└───────────────┴──────────────────────────────────────────────┘
```

### Anotar código fuente

La vista más útil: `perf annotate` muestra el **porcentaje de tiempo por línea**:

```bash
perf annotate parse_record
```

```
       │    fn parse_record(line: &str) -> Record {
       │        let fields: Vec<&str> = line.split(',').collect();
 15.3% │        let name = fields[0].to_string();          ← allocación
  2.1% │        let value: f64 = fields[1].parse().unwrap();
 27.8% │        let normalized = normalize(value);         ← función costosa
       │        Record { name, value: normalized }
       │    }
```

Ahora sabes que `normalize()` consume el 27.8% del tiempo en `parse_record`,
y la conversión a String consume el 15.3%.

### Filtrar por período de tiempo

```bash
# Grabar con timestamps
perf record -g -T ./target/release/my_program

# Solo analizar un rango temporal (si sabes cuándo fue la parte lenta)
perf report --time 10%/5  # El quinto 10% del tiempo total
```

---

## Flame graphs: visualización de perfiles

Un flame graph es una visualización donde:
- El **eje X** es el porcentaje del tiempo total
- El **eje Y** es la profundidad de la pila de llamadas
- Cada **rectángulo** es una función; su ancho es proporcional al tiempo

```
┌──────────────────────────────────────────────────────────────┐
│  Lectura de un flame graph                                   │
│                                                              │
│  ┌─────────────────────────────┐                             │
│  │        normalize()          │  ← Esta función             │
│  ├──────────────┬──────────────┤     consume ~50%            │
│  │  math_op_1() │  math_op_2() │                             │
│  ├──────────────┴──────┬───────┤                             │
│  │   parse_record()    │write()│                             │
│  ├─────────────────────┴───────┤                             │
│  │          main()             │                             │
│  └─────────────────────────────┘                             │
│                                                              │
│  Lo que buscas:                                              │
│  • Rectángulos ANCHOS → funciones costosas                   │
│  • "Mesetas" en la parte superior → trabajo real del CPU     │
│  • Torres altas y estrechas → recursión profunda             │
│  • Ancho del padre ≠ suma de hijos → tiempo en la función    │
│    misma (no delegado a subfunciones)                        │
└──────────────────────────────────────────────────────────────┘
```

### Generar flame graphs con las herramientas de Brendan Gregg

```bash
# Instalar las herramientas
git clone https://github.com/brendangregg/FlameGraph.git

# 1. Grabar el perfil
perf record -g --call-graph dwarf \
    ./target/release/my_program < input.txt

# 2. Convertir a formato de texto
perf script > perf_output.txt

# 3. Colapsar las pilas
./FlameGraph/stackcollapse-perf.pl perf_output.txt > collapsed.txt

# 4. Generar el SVG
./FlameGraph/flamegraph.pl collapsed.txt > flamegraph.svg

# 5. Abrir en el navegador
firefox flamegraph.svg
# o
xdg-open flamegraph.svg
```

El SVG es **interactivo**: puedes hacer clic en una función para hacer zoom,
y pasar el ratón para ver porcentajes exactos.

---

## Inferno: flame graphs en Rust

`inferno` es una reimplementación en Rust de las herramientas de FlameGraph de
Brendan Gregg. Es más rápido y se instala con cargo.

### Instalación

```bash
cargo install inferno
```

### Generar flame graphs

```bash
# Grabar el perfil
perf record -g --call-graph dwarf \
    ./target/release/my_program < input.txt

# Pipeline directo: perf script → inferno
perf script | inferno-collapse-perf | inferno-flamegraph > flamegraph.svg
```

### Opciones útiles de inferno

```bash
# Filtrar por nombre de función
perf script | inferno-collapse-perf \
    | inferno-flamegraph --title "My Program Profile" \
    > flamegraph.svg

# Colores personalizados
perf script | inferno-collapse-perf \
    | inferno-flamegraph --colors rust \
    > flamegraph.svg
# Esquemas de color: hot, mem, io, rust, java, js, perl, ...

# Flame graph invertido (icicle graph)
# Útil para ver qué funciones acumulan más tiempo
perf script | inferno-collapse-perf \
    | inferno-flamegraph --reverse > icicle.svg

# Flame graph diferencial (comparar antes/después)
# 1. Grabar perfil A
perf record -g -o perf_before.data ./target/release/program_v1
# 2. Grabar perfil B
perf record -g -o perf_after.data ./target/release/program_v2
# 3. Colapsar ambos
perf script -i perf_before.data | inferno-collapse-perf > before.txt
perf script -i perf_after.data | inferno-collapse-perf > after.txt
# 4. Generar diff flame graph
inferno-diff-folded before.txt after.txt | inferno-flamegraph > diff.svg
# Rojo = más lento, azul = más rápido
```

### cargo-flamegraph: atajo para proyectos Rust

```bash
# Instalar
cargo install flamegraph

# Generar flame graph directamente
cargo flamegraph -- < input.txt
# Genera flamegraph.svg automáticamente

# Para un benchmark
cargo flamegraph --bench my_benchmark

# Para tests
cargo flamegraph --test integration_test -- test_name

# Con opciones de perf
cargo flamegraph --freq 5000 -- < input.txt
# --freq: frecuencia de muestreo (default 997 Hz)
```

---

## CPU-bound vs I/O-bound

La primera pregunta al hacer profiling es: **¿el programa es lento por CPU
o por I/O?** La respuesta cambia completamente la estrategia de optimización.

### Diagnosticar con perf stat

```bash
perf stat ./target/release/my_program < input.txt
```

```
┌──────────────────────────────────────────────────────────────┐
│  CPU-bound                    │  I/O-bound                   │
├───────────────────────────────┼──────────────────────────────┤
│  task-clock: # 0.99 CPUs     │  task-clock: # 0.15 CPUs     │
│  (utilización cercana a 1.0) │  (mucho tiempo idle)          │
│                               │                              │
│  user time >> sys time        │  sys time >> user time        │
│  (trabajo en tu código)       │  (esperando I/O del kernel)  │
│                               │                              │
│  IPC alto (> 1.0)             │  IPC bajo (< 0.5)            │
│  (CPU trabajando bien)        │  (CPU parado esperando)      │
│                               │                              │
│  Pocos context switches       │  Muchos context switches     │
│                               │  (el kernel suspende el      │
│                               │  proceso mientras espera I/O)│
└───────────────────────────────┴──────────────────────────────┘
```

### Estrategias según el tipo de bottleneck

```
┌──────────────────────────────────────────────────────────────┐
│  CPU-bound: optimizar algoritmo y datos                      │
│  ────────────────────────────────────────                    │
│  • Usar algoritmo más eficiente (O(n²) → O(n log n))        │
│  • Reducir alocaciones (Vec::with_capacity, reutilizar)      │
│  • Mejorar localidad de caché (SoA vs AoS)                   │
│  • Paralelizar (rayon, threads)                              │
│  • SIMD (std::arch, packed_simd)                             │
│  • Evitar copias innecesarias (&str en vez de String)        │
│                                                              │
│  I/O-bound: optimizar acceso a disco/red                     │
│  ────────────────────────────────────────                    │
│  • Buffered I/O (BufReader, BufWriter)                       │
│  • Reducir número de syscalls (leer en bloques grandes)      │
│  • Async I/O (tokio, async-std)                              │
│  • Memory-mapped files (mmap)                                │
│  • Caché de datos leídos frecuentemente                      │
│  • Compresión (si el cuello es ancho de banda)               │
│                                                              │
│  Memory-bound: optimizar uso de memoria                      │
│  ────────────────────────────────────────                    │
│  • Reducir tamaño de estructuras (packing, tipos menores)    │
│  • Mejorar localidad de datos (acceso secuencial vs random)  │
│  • Reducir cache misses (prefetching, layout de datos)       │
│  • Evitar fragmentación (arenas, pools)                      │
└──────────────────────────────────────────────────────────────┘
```

### Verificar I/O con strace

```bash
# ¿Cuánto tiempo se gasta en syscalls?
strace -c ./target/release/my_program < input.txt

# Output:
# % time     seconds  usecs/call     calls    errors syscall
# ------ ----------- ----------- --------- --------- --------
#  78.45    0.234567          12     19538           read
#  15.23    0.045678           8      5846           write
#   3.12    0.009345          93       100           open
# → 78% del tiempo en read() → I/O bound en lectura
```

```bash
# ¿Las lecturas son de tamaño razonable?
strace -e read -s 0 ./target/release/my_program < input.txt 2>&1 \
    | head -20

# Si ves muchas lecturas de 1 byte:
# read(0, "", 1) = 1
# read(0, "", 1) = 1
# → No está usando BufReader!

# Con BufReader deberías ver:
# read(0, "", 8192) = 8192
```

---

## Profiling de memoria

### Valgrind massif: profiling de heap

```bash
# Grabar perfil de memoria
valgrind --tool=massif ./target/debug/my_program < input.txt

# Analizar
ms_print massif.out.12345
```

```
    MB
12.50^                                  #
     |                              @@@@#:::
     |                          @@@@    #:::
     |                      @@@@        #:::
     |                  @@@@            #:::
     |              @@@@                #:::
     |          @@@@                    #:::
     |      @@@@                        #:::
     |  @@@@                            #:::
     | @                                #:::
   0 +----------------------------------------------->Mi
     0                                            1.234

     ← Crecimiento lineal constante = posible memory leak
     ← Picos y valles = uso normal con alocación/liberación
     ← Meseta alta = memoria retenida (¿necesaria?)
```

### heaptrack: alternativa más moderna

```bash
# Instalar (Fedora)
sudo dnf install heaptrack

# Grabar
heaptrack ./target/debug/my_program < input.txt

# Analizar en terminal
heaptrack_print heaptrack.my_program.12345.gz

# Analizar con GUI
heaptrack_gui heaptrack.my_program.12345.gz
```

heaptrack muestra:
- Pico de uso de memoria
- Número de alocaciones por función
- Dónde se aloca memoria que nunca se libera (leaks)
- Flame graph de alocaciones

### DHAT: profiling de datos de heap (Valgrind)

```bash
# DHAT muestra cómo se USAN los datos alocados
valgrind --tool=dhat ./target/debug/my_program < input.txt

# Genera dhat.out.12345 — abrir en:
# https://nnethercote.github.io/dh_view/dh_view.html
```

DHAT responde preguntas como:
- "¿Se alocan buffers que se usan poco?"
- "¿Hay alocaciones de vida muy corta?" (candidatas para stack allocation)
- "¿Se accede a los datos de forma secuencial o aleatoria?"

---

## Profiling en Rust: particularidades

### Compilar para profiling

```toml
# Cargo.toml

# Opción 1: release con debug symbols (recomendado para profiling)
[profile.release]
debug = true
# No afecta el rendimiento, pero permite ver nombres de funciones
# y líneas de código en el profiler

# Opción 2: perfil dedicado para profiling
[profile.profiling]
inherits = "release"
debug = true
```

```bash
# Compilar con el perfil de profiling
cargo build --profile profiling

# O simplemente release con debug=true
cargo build --release
```

### Símbolos mangled vs demangled

Rust "mangla" los nombres de funciones para incluir información del módulo,
generics, y lifetimes. Los profilers necesitan desmanglarlos:

```bash
# perf report ya demangliza automáticamente
perf report

# Si ves nombres como _ZN9my_module12parse_record17h1234567890abcdefE
# en vez de my_module::parse_record, usar:
perf report --demangle=rust

# Para inferno, el demangling es automático
```

### Evitar que el compilador optimice el código de benchmark

```rust
// En benchmarks, el compilador puede eliminar código "sin efecto"
use std::hint::black_box;

fn benchmark_parse() {
    let data = "test,42,3.14";
    for _ in 0..1_000_000 {
        // Sin black_box: el compilador puede eliminar esto
        // porque el resultado no se usa
        let result = parse_record(data);

        // Con black_box: el compilador no puede eliminar la llamada
        black_box(parse_record(data));
    }
}
```

### criterion: benchmarks estadísticos

```toml
# Cargo.toml
[dev-dependencies]
criterion = { version = "0.5", features = ["html_reports"] }

[[bench]]
name = "my_benchmark"
harness = false
```

```rust
// benches/my_benchmark.rs
use criterion::{criterion_group, criterion_main, Criterion};

fn benchmark_parse(c: &mut Criterion) {
    let data = "name,42,3.14,true,hello world";

    c.bench_function("parse_record", |b| {
        b.iter(|| parse_record(criterion::black_box(data)))
    });
}

fn benchmark_parse_sizes(c: &mut Criterion) {
    let mut group = c.benchmark_group("parse_by_size");

    for size in [10, 100, 1000, 10000] {
        let data: String = (0..size)
            .map(|i| format!("field{}", i))
            .collect::<Vec<_>>()
            .join(",");

        group.bench_with_input(
            criterion::BenchmarkId::from_parameter(size),
            &data,
            |b, data| b.iter(|| parse_record(criterion::black_box(data))),
        );
    }
    group.finish();
}

criterion_group!(benches, benchmark_parse, benchmark_parse_sizes);
criterion_main!(benches);
```

```bash
# Ejecutar benchmarks
cargo bench

# Genera reporte HTML en target/criterion/
# Incluye gráficos de distribución, regresión, y comparación
```

### Profiling de compilación

A veces lo lento no es el programa sino la **compilación**:

```bash
# ¿Qué tarda más en compilar?
cargo build --timings
# Genera un reporte HTML: target/cargo-timings/cargo-timing.html

# Muestra:
# - Tiempo de compilación por crate
# - Dependencias en paralelo vs secuenciales
# - Cuello de botella de la compilación
```

---

## Errores comunes

### 1. Profiling en modo debug en vez de release

```bash
# ❌ Profiling en debug: las optimizaciones cambian DÓNDE está
# el cuello de botella
perf record cargo run  # Compila en debug por defecto

# En debug:
# - bounds checks activos (los elimina release)
# - sin inlining (cambia completamente la pila de llamadas)
# - sin auto-vectorización
# Los resultados no reflejan el rendimiento real

# ✅ Profiling en release CON debug symbols
# Cargo.toml: [profile.release] debug = true
perf record ./target/release/my_program
```

### 2. Optimizar sin medir primero

```rust
// ❌ "Los HashMap son lentos, voy a usar un BTreeMap"
// Sin evidencia de que HashMap es el cuello de botella

// ✅ Proceso correcto:
// 1. Medir con perf record → flame graph
// 2. Identificar que el 40% del tiempo está en HashMap::get
// 3. Investigar: ¿es el hash costoso? ¿hay muchas colisiones?
// 4. Probar alternativa (FxHashMap, array, etc.)
// 5. Medir de nuevo para verificar mejora
```

### 3. No considerar la varianza en las mediciones

```bash
# ❌ Una sola medición
perf stat ./my_program
# "1.234 seconds" → ¿es estable?

# ✅ Múltiples mediciones con estadísticas
perf stat -r 10 ./my_program
#          1.234 +- 0.012 seconds time elapsed  ( +-  0.98% )
# La desviación nos dice si la medición es confiable

# Si la varianza es alta (> 5%):
# - Otro proceso consume CPU → cerrar aplicaciones
# - Frecuencia de CPU variable → fijar con cpupower
# - I/O variable → usar ramdisk para datos de test
```

### 4. Flame graph sin call graph = datos inútiles

```bash
# ❌ Sin -g: no sabes QUIÉN llama a la función costosa
perf record ./my_program
# El flame graph será plano (solo funciones top-level)

# ✅ Con call graph completo
perf record -g --call-graph dwarf ./my_program
# Ahora ves la cadena: main → process → parse → normalize

# --call-graph dwarf: usa DWARF debug info (más preciso)
# --call-graph fp: usa frame pointers (más rápido pero
#   requiere compilar con -fno-omit-frame-pointer)
```

### 5. Ignorar el tiempo en el kernel

```bash
# perf report muestra funciones de [kernel.kallsyms]
# Si consumen > 10% del tiempo:

# ❌ Ignorarlas porque "no es mi código"

# ✅ Investigar qué syscalls causan el overhead
strace -c ./my_program  # ¿Cuáles y cuántas?
perf stat -e syscalls:sys_enter_read,syscalls:sys_enter_write \
    ./my_program         # Contadores específicos

# Causa común: muchas syscalls pequeñas
# Fix: BufReader/BufWriter, batch I/O, mmap
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              PROFILING BÁSICO CHEATSHEET                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  PERF STAT (contadores de hardware)                          │
│  perf stat PROGRAMA               Contadores por defecto    │
│  perf stat -r N PROGRAMA          N repeticiones + promedio  │
│  perf stat -e EVENTOS PROGRAMA    Eventos específicos        │
│  Eventos: cache-misses, branch-misses, page-faults,         │
│           L1-dcache-load-misses, instructions, cycles        │
│                                                              │
│  PERF RECORD/REPORT (muestreo de CPU)                        │
│  perf record -g PROGRAMA          Grabar con call graph      │
│  perf record -g --call-graph dwarf PROGRAMA  (más preciso)  │
│  perf report                      Analizar interactivamente  │
│  perf annotate FUNCION            Ver código con porcentajes │
│  perf script                      Output para flame graphs   │
│                                                              │
│  FLAME GRAPHS (con inferno)                                  │
│  cargo install inferno            Instalar                   │
│  perf script | inferno-collapse-perf \                       │
│    | inferno-flamegraph > fg.svg  Generar flame graph        │
│                                                              │
│  CARGO-FLAMEGRAPH (atajo)                                    │
│  cargo install flamegraph                                    │
│  cargo flamegraph -- ARGS         Flame graph directo        │
│  cargo flamegraph --bench NAME    De un benchmark            │
│                                                              │
│  COMPILAR PARA PROFILING                                     │
│  [profile.release]                                           │
│  debug = true                     Símbolos sin afectar perf  │
│                                                              │
│  CPU vs I/O BOUND                                            │
│  perf stat → CPUs utilized:                                  │
│    ≈ 1.0 → CPU-bound                                        │
│    << 1.0 → I/O-bound                                       │
│  strace -c → % time en read/write:                          │
│    alto → I/O-bound                                         │
│                                                              │
│  MEMORIA                                                     │
│  valgrind --tool=massif PROG      Perfil de heap             │
│  ms_print massif.out.PID          Analizar                   │
│  heaptrack PROG                   Alternativa moderna        │
│                                                              │
│  BENCHMARKS EN RUST                                          │
│  criterion (crate)                Benchmarks estadísticos    │
│  std::hint::black_box(val)        Prevenir optimización      │
│  cargo build --timings            Tiempo de compilación      │
│                                                              │
│  FLUJO DE OPTIMIZACIÓN                                       │
│  1. Medir (perf stat → CPU o I/O?)                          │
│  2. Perfilar (perf record → flame graph)                    │
│  3. Identificar cuello de botella (función con más %)        │
│  4. Optimizar UN cambio                                      │
│  5. Medir de nuevo (¿mejoró?)                               │
│  6. Repetir                                                  │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Diagnosticar CPU vs I/O bound

Dado este programa:

```rust
use std::io::{self, BufRead, Write};

fn process_line(line: &str) -> String {
    // Simular procesamiento costoso
    let mut result = String::new();
    for word in line.split_whitespace() {
        let reversed: String = word.chars().rev().collect();
        result.push_str(&reversed);
        result.push(' ');
    }
    result
}

fn main() -> io::Result<()> {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut out = stdout.lock();

    for line in stdin.lock().lines() {
        let line = line?;
        let processed = process_line(&line);
        writeln!(out, "{}", processed)?;
    }
    Ok(())
}
```

**Tareas:**
1. Genera un archivo de prueba grande: `yes "hello world foo bar baz" | head -1000000 > big_input.txt`
2. Ejecuta con `perf stat` y determina si es CPU-bound o I/O-bound
3. Ejecuta con `strace -c` y observa qué syscalls dominan
4. Genera un flame graph. ¿Qué función consume más tiempo?
5. Si es I/O-bound, aplica `BufWriter`. Si es CPU-bound, optimiza `process_line`.
   Mide la mejora con `perf stat`

**Pregunta de reflexión:** Este programa ya usa `BufRead` (via `lines()`) para
la entrada. ¿Por qué la salida podría seguir siendo un cuello de botella aunque
`stdout.lock()` ya evita el locking?

---

### Ejercicio 2: Flame graph y optimización dirigida

```rust
use std::collections::HashMap;

fn count_words(text: &str) -> HashMap<String, usize> {
    let mut counts = HashMap::new();
    for word in text.split_whitespace() {
        let normalized = word.to_lowercase();
        let cleaned: String = normalized
            .chars()
            .filter(|c| c.is_alphanumeric())
            .collect();
        if !cleaned.is_empty() {
            *counts.entry(cleaned).or_insert(0) += 1;
        }
    }
    counts
}

fn top_words(counts: &HashMap<String, usize>, n: usize) -> Vec<(&str, usize)> {
    let mut sorted: Vec<_> = counts.iter()
        .map(|(k, &v)| (k.as_str(), v))
        .collect();
    sorted.sort_by(|a, b| b.1.cmp(&a.1));
    sorted.truncate(n);
    sorted
}

fn main() {
    let text = std::fs::read_to_string("big_text.txt").unwrap();

    for _ in 0..10 {  // Repetir para amplificar
        let counts = count_words(&text);
        let top = top_words(&counts, 10);
        println!("Top word: {:?}", top[0]);
    }
}
```

**Tareas:**
1. Descarga un texto grande (ej: `curl -o big_text.txt https://www.gutenberg.org/cache/epub/1342/pg1342.txt` o genera uno sintético)
2. Compila con `[profile.release] debug = true` y genera un flame graph
3. Identifica las 3 funciones que consumen más tiempo
4. Optimiza `count_words`: ¿puedes evitar `to_lowercase` + `filter` + `collect`
   en cada palabra? ¿Puedes usar `entry` de forma más eficiente?
5. Genera un flame graph después de la optimización y compara visualmente

**Pregunta de reflexión:** ¿Qué pasa si usas `FxHashMap` (del crate `rustc-hash`)
en vez de `HashMap`? ¿Por qué el hash por defecto de Rust (`SipHash`) es más
lento pero se elige como default?

---

### Ejercicio 3: Profiling de un programa C vs Rust

Escribe el mismo programa en C y Rust: sumar todos los bytes de un archivo.

```c
// sum_bytes.c
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *f = fopen(argv[1], "rb");
    unsigned long long sum = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        sum += c;
    }
    fclose(f);
    printf("Sum: %llu\n", sum);
    return 0;
}
```

```rust
// sum_bytes.rs (main.rs)
use std::io::Read;

fn main() {
    let path = std::env::args().nth(1).unwrap();
    let data = std::fs::read(&path).unwrap();
    let sum: u64 = data.iter().map(|&b| b as u64).sum();
    println!("Sum: {}", sum);
}
```

**Tareas:**
1. Genera un archivo de prueba de 100MB: `dd if=/dev/urandom of=test_100mb.bin bs=1M count=100`
2. Compila ambos: `gcc -O2 -o sum_c sum_bytes.c` y `cargo build --release`
3. Ejecuta `perf stat` en ambos. Compara IPC, cache misses, y tiempo
4. ¿Por qué la versión C con `fgetc` es mucho más lenta? (pista: `strace -c`)
5. Optimiza la versión C usando `fread` con un buffer de 64KB. ¿Se acerca al rendimiento de Rust?
6. Genera flame graphs de las dos versiones de C (fgetc vs fread) y compáralos

**Pregunta de reflexión:** La versión Rust usa `fs::read` que carga todo el
archivo en memoria. ¿Es esto siempre mejor que leer en bloques? ¿Qué pasa
con un archivo de 8GB en una máquina con 4GB de RAM?