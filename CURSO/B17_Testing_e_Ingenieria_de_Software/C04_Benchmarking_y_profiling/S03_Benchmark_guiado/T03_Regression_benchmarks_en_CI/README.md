# T03 — Regression benchmarks en CI

> **Capítulo 4 — Sección 3 — Tópico 3 de 3**
>
> *"Un benchmark que corres una vez es una curiosidad. Un benchmark que corres en cada commit es un contrato con el rendimiento."*

---

## Tabla de contenidos

1. [¿Qué es un regression benchmark?](#1-qué-es-un-regression-benchmark)
2. [El problema que resuelve: la muerte por mil cortes](#2-el-problema-que-resuelve-la-muerte-por-mil-cortes)
3. [Regression tests vs regression benchmarks](#3-regression-tests-vs-regression-benchmarks)
4. [Anatomía de un sistema de regression benchmarking](#4-anatomía-de-un-sistema-de-regression-benchmarking)
5. [¿Qué mide un regression benchmark y qué NO?](#5-qué-mide-un-regression-benchmark-y-qué-no)
6. [Baseline: el concepto clave](#6-baseline-el-concepto-clave)
7. [Tres modelos de baseline](#7-tres-modelos-de-baseline)
8. [Criterion.rs: save-baseline y compare](#8-criterionrs-save-baseline-y-compare)
9. [cargo-criterion: el runner externo](#9-cargo-criterion-el-runner-externo)
10. [Iai: la alternativa determinista](#10-iai-la-alternativa-determinista)
11. [Regression benchmarking en C puro](#11-regression-benchmarking-en-c-puro)
12. [El problema del ruido en CI](#12-el-problema-del-ruido-en-ci)
13. [Estrategias para reducir ruido](#13-estrategias-para-reducir-ruido)
14. [Runners dedicados (self-hosted) para estabilidad](#14-runners-dedicados-self-hosted-para-estabilidad)
15. [Umbrales de alerta: evitando el boy-who-cried-wolf](#15-umbrales-de-alerta-evitando-el-boy-who-cried-wolf)
16. [Detección estadística de regresiones](#16-detección-estadística-de-regresiones)
17. [Integración con GitHub Actions](#17-integración-con-github-actions)
18. [Workflow completo: PR comparando contra main](#18-workflow-completo-pr-comparando-contra-main)
19. [Persistencia de resultados históricos](#19-persistencia-de-resultados-históricos)
20. [Visualización: dashboards y tendencias](#20-visualización-dashboards-y-tendencias)
21. [Servicios externos: Bencher, Codspeed, nyrkiö](#21-servicios-externos-bencher-codspeed-nyrkiö)
22. [Bisect asistido para cazar regresiones](#22-bisect-asistido-para-cazar-regresiones)
23. [Microbenchmarks vs macrobenchmarks en CI](#23-microbenchmarks-vs-macrobenchmarks-en-ci)
24. [Errores comunes al montar regression CI](#24-errores-comunes-al-montar-regression-ci)
25. [Proyecto de práctica: regression\_lab/](#25-proyecto-de-práctica-regression_lab)
26. [Ejercicios](#26-ejercicios)
27. [Checklist del tópico](#27-checklist-del-tópico)
28. [Cierre del Capítulo 4](#28-cierre-del-capítulo-4)
29. [Navegación](#29-navegación)

---

## 1. ¿Qué es un regression benchmark?

Un **regression benchmark** (o *performance regression test*) es un benchmark cuyo
propósito principal **no es descubrir el rendimiento de una pieza de código**, sino
**detectar cuándo ese rendimiento empeora** respecto a un estado previo conocido.

La diferencia con un benchmark "normal" es sutil pero fundamental:

| Benchmark exploratorio       | Regression benchmark                     |
|------------------------------|------------------------------------------|
| *¿Qué tan rápido es esto?*   | *¿Es esto más lento que antes?*          |
| Lo corres una vez            | Lo corres en cada commit / cada PR       |
| Te importa el número         | Te importa el **delta** respecto al baseline |
| Lo publicas en un blog       | Lo integras en CI y alerta si falla      |
| Un humano interpreta         | Un sistema automatizado decide           |

El verbo clave es **"regress"**: volver a un estado peor. Un regression benchmark
es un **sensor** que te avisa cuando el rendimiento del sistema ha retrocedido
por encima de un umbral aceptable, **antes** de que ese retroceso llegue a
producción.

La propuesta subyacente: **el rendimiento es una propiedad del código tan real
como la corrección**, y por tanto merece el mismo nivel de protección automática.
Si un test unitario fallando bloquea un merge, ¿por qué no debería hacerlo una
regresión del 15% en tu función más caliente?

---

## 2. El problema que resuelve: la muerte por mil cortes

Imagina un proyecto con miles de commits al año. Cada commit individual añade
una funcionalidad o arregla un bug, y ninguno degrada el rendimiento de forma
dramática. Pero cada commit introduce pequeñas ineficiencias: un `clone()` de
más aquí, un `Vec` en lugar de un `&[T]` allá, un `HashMap<String, _>` donde
bastaría un `HashMap<&str, _>`.

Individualmente, ninguno de esos cambios dispararía una alarma. El autor del PR
dirá: *"es solo un 1%, dentro del ruido de medición"*. El reviewer lo aprobará
sin pensarlo. El test suite pasa. Todo está bien.

Tras **doscientos commits** así, el sistema es **2.7× más lento** (1.01²⁰⁰ ≈ 7.3×
lento), pero nadie puede señalar **qué commit** lo rompió, porque **ninguno** lo
rompió. Todos lo rompieron un poco.

Esto es lo que Martin Fowler llama *"performance rot"* y lo que Brendan Gregg
describe como *"death by a thousand cuts"*. Es el fallo más común, y el más
difícil de revertir, porque para cuando alguien se da cuenta, hay que revisar
cientos de commits y **no hay baseline limpio al que volver**.

### La paradoja del rendimiento gradual

```
ITERACIÓN 1:  "El cambio X hace esto un 2% más lento, pero es aceptable"
ITERACIÓN 2:  "El cambio Y hace esto un 1.5% más lento, pero es aceptable"
...
ITERACIÓN 50: "El código está 2× más lento que hace un año, ¿qué pasó?"
```

Cada decisión individual es racional. La agregación es desastrosa. La única
forma de romper el ciclo es **comparar contra una historia**, no solo contra el
commit anterior inmediato.

### El regression benchmark como freno

Un sistema de regression benchmarks bien configurado hace dos cosas:

1. **Compara contra el commit anterior**: "¿Este PR introduce una regresión?"
2. **Compara contra una baseline de largo plazo**: "¿Hemos retrocedido respecto
   a donde estábamos hace un mes?"

La primera comparación atrapa los delitos individuales. La segunda atrapa la
acumulación de delitos menores que individualmente pasaron el umbral.

---

## 3. Regression tests vs regression benchmarks

Es fácil confundir estos dos conceptos:

| Concepto             | ¿Qué verifica?                          | ¿Qué falla?           |
|----------------------|-----------------------------------------|-----------------------|
| Regression **test**  | *Corrección*: la salida es la esperada  | Assertion `eq()` falla|
| Regression **benchmark** | *Rendimiento*: el tiempo es aceptable | Delta excede umbral   |

Un regression test es determinista: 2+2 debe ser 4, punto. Si falla, hay un bug.

Un regression benchmark es **estadístico**: 1000 ejecuciones de `sort(vec)`
tienen una distribución de tiempos, y lo que medimos es si esa distribución
se ha desplazado significativamente respecto a la del baseline. Si falla, puede
haber una regresión o puede ser ruido. Y distinguirlos es el arte de esta
disciplina.

**Consecuencia práctica**: mientras los regression tests pueden vivir en cualquier
runner de CI (su resultado es determinista), los regression benchmarks necesitan
consideraciones especiales sobre el entorno de ejecución, tamaños de muestra,
y criterios de decisión.

---

## 4. Anatomía de un sistema de regression benchmarking

Un sistema completo de regression benchmarking tiene estos componentes:

```
┌───────────────────────────────────────────────────────────────────┐
│                  SISTEMA DE REGRESSION BENCHMARKING               │
├───────────────────────────────────────────────────────────────────┤
│                                                                   │
│  1. SUITE DE BENCHMARKS                                           │
│     ├── Microbenchmarks (funciones puras, <1ms)                   │
│     └── Macrobenchmarks (end-to-end, >100ms)                      │
│                                                                   │
│  2. RUNNER REPRODUCIBLE                                           │
│     ├── Entorno aislado (CPU pinning, no turbo, no ASLR)          │
│     ├── Nº de iteraciones estable                                 │
│     └── Warm-up configurado                                       │
│                                                                   │
│  3. ALMACÉN DE BASELINES                                          │
│     ├── Baseline actual (main / trunk)                            │
│     ├── Histórico (N commits atrás)                               │
│     └── Metadatos (commit hash, fecha, runner, flags)             │
│                                                                   │
│  4. COMPARADOR ESTADÍSTICO                                        │
│     ├── Delta bruto (%)                                           │
│     ├── Significación (p-value, CI, Mann-Whitney U)               │
│     └── Umbral de alerta (ruido vs señal)                         │
│                                                                   │
│  5. INTEGRACIÓN CON CI                                            │
│     ├── Trigger (push a main, PR, cron)                           │
│     ├── Reporter (comentario en PR, log)                          │
│     └── Gating (¿bloquea merge o solo avisa?)                     │
│                                                                   │
│  6. VISUALIZACIÓN HISTÓRICA                                       │
│     └── Dashboard, gráficas, alertas fuera de banda               │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

Cada uno de estos bloques tiene decisiones de diseño no triviales. Los
cubriremos uno por uno en las secciones siguientes.

---

## 5. ¿Qué mide un regression benchmark y qué NO?

**Lo que típicamente miden**:

- Tiempo de ejecución (wall-clock, CPU time)
- Número de instrucciones (deterministic, usando `perf` o `iai`)
- Peak memory / allocations
- Throughput (ops/sec, bytes/sec)
- Latencia p50/p95/p99

**Lo que NO deberían medir en un regression benchmark de CI**:

- **Tiempo de compilación** (es otra cosa, otra disciplina)
- **Tamaño de binario** (aunque es válido trackearlo aparte)
- **Rendimiento en hardware de producción** (salvo si tienes un runner en ese
  hardware exacto)
- **Experiencia de usuario final** (latencia de red, rendering, etc — esos son
  E2E tests)

**Regla de oro**: un regression benchmark debe medir *una cosa* con *mucha
precisión*, no *muchas cosas* con poca precisión. La tentación de meter "un
benchmark por clase" es el camino a un CI lento, ruidoso e inútil.

---

## 6. Baseline: el concepto clave

La palabra **baseline** es central. Un regression benchmark sin baseline no
tiene sentido: ¿comparado contra qué decimos que ha regresado?

**Definición operativa**: un baseline es un conjunto de mediciones de rendimiento
asociadas a un estado conocido del código (normalmente un commit), que sirve
como punto de referencia contra el cual comparar mediciones futuras.

Un buen baseline tiene estas propiedades:

| Propiedad        | Por qué importa                                               |
|------------------|---------------------------------------------------------------|
| **Reproducible** | Si vuelves a correr el mismo commit en el mismo runner, debes obtener resultados comparables |
| **Versionado**   | Sabes exactamente qué commit produjo esos números             |
| **Fechado**      | Sabes cuándo se midió                                         |
| **Anotado**      | Sabes en qué runner, con qué flags, con qué alocador          |
| **Actualizable** | Puedes decidir "ahora el nuevo baseline es este commit"       |
| **Comparable**   | Tiene la misma forma (mismos benchmarks, misma configuración) que las mediciones contra las que lo comparas |

Sin versionado y fechado, no puedes diagnosticar: "esta regresión apareció entre
el commit A y el commit B". Sin anotación del runner, no puedes distinguir entre
"el código regresó" y "cambiamos la máquina de CI".

---

## 7. Tres modelos de baseline

Existen básicamente tres estrategias para mantener baselines:

### Modelo A: Baseline móvil (last-commit)

```
Cada commit compara contra el commit anterior.
```

- ✅ Detecta regresiones inmediatas (el PR que rompió algo)
- ❌ No detecta acumulación gradual
- ❌ Muy ruidoso (cada commit tiene su propio baseline)

**Uso típico**: CI rápido donde solo te importa que el PR no degrade nada.

### Modelo B: Baseline fijo (pinned)

```
Todos los commits comparan contra un commit "de referencia" elegido manualmente.
```

- ✅ Detecta acumulación gradual
- ✅ Estable (mismo baseline durante semanas o meses)
- ❌ Requiere actualización manual periódica
- ❌ Si tu código mejora legítimamente, el baseline queda obsoleto

**Uso típico**: versión v1.0.0, release candidate, o "hace un mes".

### Modelo C: Baseline de trunk (main-tracking)

```
Cada PR compara contra el último commit verde de main/master.
El baseline se actualiza automáticamente cuando main avanza.
```

- ✅ Detecta regresiones inmediatas
- ✅ Evoluciona con el proyecto
- ✅ Es lo que hace criterion con `--save-baseline main`
- ❌ No detecta acumulación (cada merge "olvida" el pasado)

**Uso típico**: el más común en proyectos OSS modernos.

### Estrategia híbrida (recomendada)

Lo que funciona en la práctica es **combinar B y C**:

- **Trunk-tracking** para detectar que ningún PR individual empeora más del umbral
- **Baseline fijo** (por ejemplo, última release) para un job cron semanal que
  verifica que no hemos ido a peor en los últimos meses

De este modo atrapas tanto los errores agudos como los crónicos.

---

## 8. Criterion.rs: save-baseline y compare

[Criterion.rs](https://bheisler.github.io/criterion.rs/book/) es el estándar de
facto para benchmarking en Rust y soporta nativamente el workflow de baselines.
Los comandos clave son:

### 8.1 Establecer un baseline

```bash
# Estando en el commit que quieres usar como referencia:
git checkout main
cargo bench --bench my_bench -- --save-baseline main
```

Esto corre todos los benchmarks y guarda sus resultados en
`target/criterion/<bench>/main/` como el baseline llamado `main`.

### 8.2 Comparar contra un baseline

```bash
# Ahora cambias al branch del PR:
git checkout feature/optimize-parser
cargo bench --bench my_bench -- --baseline main
```

Criterion corre los benchmarks y los compara automáticamente con el baseline
`main`, mostrando algo como:

```
parse_simple/small      time:   [120.45 ns 121.23 ns 122.14 ns]
                        change: [-2.3421% -1.8765% -1.4102%] (p = 0.00 < 0.05)
                        Performance has improved.

parse_simple/large      time:   [45.123 µs 45.432 µs 45.789 µs]
                        change: [+8.1234% +9.3456% +10.512%] (p = 0.00 < 0.05)
                        Performance has regressed.
```

La línea `change:` tiene **tres valores**: límite inferior del IC, punto estimado,
límite superior del IC. El `p = 0.00 < 0.05` indica que el cambio es
estadísticamente significativo (criterion hace un test de hipótesis internamente).

### 8.3 Los cuatro veredictos de criterion

Criterion clasifica cada cambio en uno de cuatro estados:

| Veredicto              | Condición                                       |
|------------------------|-------------------------------------------------|
| `No change in performance detected.` | Cambio dentro del ruido                   |
| `Change within noise threshold.`     | Cambio significativo estadísticamente pero pequeño |
| `Performance has improved.`          | Mejora estadísticamente significativa y grande |
| `Performance has regressed.`         | Regresión estadísticamente significativa y grande |

El umbral del "noise threshold" es **configurable**:

```rust
use criterion::{criterion_group, criterion_main, Criterion};

fn bench_parse(c: &mut Criterion) {
    let mut group = c.benchmark_group("parse");
    group.noise_threshold(0.05); // 5% — cambios menores se clasifican como ruido
    group.significance_level(0.01); // α = 0.01 en lugar del 0.05 por defecto
    group.sample_size(200); // Más muestras = menor ruido
    group.bench_function("simple", |b| {
        b.iter(|| parse_simple(black_box(SAMPLE_INPUT)))
    });
    group.finish();
}

criterion_group!(benches, bench_parse);
criterion_main!(benches);
```

### 8.4 Flags útiles de criterion

| Flag                       | Efecto                                                |
|----------------------------|-------------------------------------------------------|
| `--save-baseline <name>`   | Guarda resultado como baseline `<name>`               |
| `--baseline <name>`        | Compara contra baseline `<name>` (no actualiza)       |
| `--baseline-lenient <name>`| Compara pero si el baseline no existe, lo crea       |
| `--load-baseline <name>`   | Carga baseline sin correr benchmarks (útil en análisis offline) |
| `--sample-size <N>`        | Número de muestras por benchmark (default: 100)       |
| `--measurement-time <s>`   | Tiempo total para medir (default: 5 seg)             |
| `--warm-up-time <s>`       | Tiempo de warm-up (default: 3 seg)                   |
| `--noise-threshold <f>`    | Umbral bajo el cual el cambio es ruido               |
| `--significance-level <f>` | Nivel α del test estadístico                         |

### 8.5 El workflow en CI con criterion

```bash
# En un job de CI:

# 1. Checkout main y mide
git checkout main
cargo bench --bench my_bench -- --save-baseline main-latest

# 2. Checkout PR y compara
git checkout $PR_SHA
cargo bench --bench my_bench -- --baseline main-latest \
    --output-format bencher 2>&1 | tee bench_output.txt

# 3. Parsea y decide
if grep -q "Performance has regressed" bench_output.txt; then
    echo "REGRESIÓN DETECTADA"
    exit 1
fi
```

Este es el esqueleto. En la práctica querrás algo más sofisticado: comentar en
el PR, persistir los datos, manejar benchmarks nuevos, etc.

---

## 9. cargo-criterion: el runner externo

`cargo-criterion` es un **runner separado** (se instala con `cargo install
cargo-criterion`) que entiende el formato interno de criterion pero provee:

- **Salida en formato machine-readable** (JSON) por defecto
- **Reportes HTML más ricos** (históricos, no solo el último run)
- **Persistencia de todos los runs**, no solo los baselines
- **Integración con herramientas externas** más sencilla

Uso básico:

```bash
cargo install cargo-criterion
cargo criterion --message-format=json > results.json
```

Cada línea del JSON es un evento: `benchmark-start`, `benchmark-complete`,
`group-complete`, etc. Esto lo hace ideal para parsear en un script de CI.

Ejemplo de un evento `benchmark-complete`:

```json
{
  "reason": "benchmark-complete",
  "id": "parse_simple/small",
  "report_directory": "target/criterion/parse_simple/small/report",
  "iteration_count": [100, 200, ...],
  "measured_values": [120.45, 121.23, ...],
  "unit": "ns",
  "throughput": [],
  "typical": { "estimate": 121.23, "lower_bound": 120.45, "upper_bound": 122.14, "unit": "ns" },
  "mean": { "estimate": 121.30, "lower_bound": 120.50, "upper_bound": 122.20, "unit": "ns" },
  "median": { "estimate": 121.20, "lower_bound": 120.40, "upper_bound": 122.10, "unit": "ns" },
  "median_abs_dev": { "estimate": 0.85, "lower_bound": 0.60, "upper_bound": 1.10, "unit": "ns" },
  "slope": { "estimate": 121.23, "lower_bound": 120.45, "upper_bound": 122.14, "unit": "ns" },
  "change": {
    "mean": { "estimate": -0.018, "lower_bound": -0.023, "upper_bound": -0.014, "unit": "%" },
    "median": { "estimate": -0.019, "lower_bound": -0.024, "upper_bound": -0.015, "unit": "%" },
    "change": "NoChange"
  }
}
```

Parseando esto puedes construir cualquier dashboard o sistema de alertas que
necesites. Es la forma "correcta" de integrar criterion con un sistema de CI
propio.

---

## 10. Iai: la alternativa determinista

Criterion mide **tiempo real**, y el tiempo real tiene ruido. Incluso con todas
las precauciones, hay una varianza mínima (del orden del 1-3%) que proviene del
sistema operativo, SMT, turbo, ASLR, interrupciones, etc.

**Iai** es una biblioteca de benchmarking desarrollada por el mismo autor de
criterion (Brook Heisler) que toma una aproximación radicalmente distinta:
**no mide tiempo, cuenta instrucciones**. Usa cachegrind (parte de Valgrind)
por debajo para obtener métricas deterministas:

- Instrucciones ejecutadas (Ir)
- L1 data cache accesses / misses (Dr, D1mr)
- LL cache misses (DLmr)
- Ciclos estimados

Estas métricas son **perfectamente deterministas**: si corres el mismo binario
sobre el mismo input, obtienes exactamente los mismos números, siempre. Esto
elimina completamente el problema del ruido en CI.

### Ejemplo con iai

```toml
# Cargo.toml
[dev-dependencies]
iai = "0.1"

[[bench]]
name = "iai_parse"
harness = false
```

```rust
// benches/iai_parse.rs
use iai::{black_box, main};

fn parse_simple() -> usize {
    let input = black_box("123,456,789,012");
    input.split(',').map(|s| s.parse::<u32>().unwrap()).sum::<u32>() as usize
}

fn parse_large() -> usize {
    let input = black_box(include_str!("../testdata/large.csv"));
    input.split('\n').count()
}

iai::main!(parse_simple, parse_large);
```

Ejecución:

```bash
cargo bench --bench iai_parse
```

Salida:

```
parse_simple
  Instructions:                1247
  L1 Accesses:                 1872
  L2 Accesses:                    5
  RAM Accesses:                   1
  Estimated Cycles:            1912

parse_large
  Instructions:              431521
  L1 Accesses:               589234
  L2 Accesses:                 1234
  RAM Accesses:                  87
  Estimated Cycles:          598679
```

Si corres esto otra vez sin cambiar el código, obtendrás **exactamente los mismos
números**. Y si cambias el código aunque sea para añadir un `println!("debug")`
comentado, el contador de instrucciones cambiará inmediatamente.

### ¿Por qué no usar siempre iai?

Parece la panacea, pero tiene limitaciones importantes:

| Limitación                        | Consecuencia                                          |
|-----------------------------------|-------------------------------------------------------|
| **Corre sobre cachegrind**        | ~20-100× más lento que el runtime normal             |
| **Simulación de cache ≠ hardware real** | El número de "L1 misses" es una aproximación       |
| **No mide paralelismo real**      | SIMD es contado como instrucciones individuales      |
| **Sin branch predictor realista** | Los misses de branch son simulados (2-bit counter)   |
| **No captura efectos NUMA, prefetcher agresivo, etc.** | Son invisibles para cachegrind                   |

**Regla práctica**: **iai** es excelente para regression benchmarks en CI porque
detecta cambios microscópicos de forma determinista. **criterion** es mejor para
caracterizar el rendimiento real del código. **Los dos son complementarios**: en
un proyecto serio tendrás ambos.

### iai-callgrind: el sucesor moderno

[`iai-callgrind`](https://github.com/iai-callgrind/iai-callgrind) es una
evolución activa de iai que usa callgrind (más rico que cachegrind) y soporta:

- Separación de costo entre funciones
- Múltiples baselines
- Mejor integración con cargo-bench
- Setup/teardown fuera de la medición

Para proyectos nuevos, es la elección recomendada sobre iai tradicional.

---

## 11. Regression benchmarking en C puro

En C no hay criterion, pero el workflow es exactamente el mismo. Las piezas
suelen montarse a mano:

### 11.1 Con hyperfine (nivel binario)

```bash
# Baseline
git checkout main
make build
hyperfine --warmup 3 --runs 20 --export-json baseline.json './build/my_prog input.dat'

# Candidate
git checkout feature/optimize
make build
hyperfine --warmup 3 --runs 20 --export-json candidate.json './build/my_prog input.dat'

# Compare
python3 compare.py baseline.json candidate.json
```

Donde `compare.py` es un script propio que parsea los JSON, calcula el delta
y emite un veredicto. Es artesanal pero perfectamente funcional.

### 11.2 Con poop (Performance Optimizer Observation Platform)

```bash
poop -- './main-bin input.dat' './candidate-bin input.dat'
```

`poop` ya hace la comparación estadística y reporta algo como:

```
Benchmark 1 (10 runs): ./main-bin input.dat
  measurement          mean ± σ            min … max
  wall_time          1.23s  ± 12.5ms     1.21s  … 1.26s
  cpu_cycles         4.56G  ± 45.2M      4.50G  … 4.62G

Benchmark 2 (10 runs): ./candidate-bin input.dat
  measurement          mean ± σ            min … max           delta
  wall_time          1.31s  ± 14.1ms     1.29s  … 1.35s        +6.5% ±  1.1%
  cpu_cycles         4.89G  ± 52.3M      4.81G  … 4.97G        +7.2% ±  0.9%
```

Para scripts de CI, `poop` puede correrse en modo batch y su salida puede
parsearse con `awk` o un script de python.

### 11.3 Con harness custom

Para casos donde quieres medir funciones individuales (no procesos completos),
lo normal es escribir un pequeño harness:

```c
// bench_harness.c
#include <stdio.h>
#include <time.h>
#include <stdint.h>

typedef struct {
    const char *name;
    void (*fn)(void);
} bench_t;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void run_bench(bench_t *b, int iters) {
    // Warm-up
    for (int i = 0; i < 3; i++) b->fn();
    uint64_t t0 = now_ns();
    for (int i = 0; i < iters; i++) b->fn();
    uint64_t t1 = now_ns();
    double per_iter = (double)(t1 - t0) / iters;
    // Salida formato bencher, parseable
    printf("test %s ... bench: %12.0f ns/iter (+/- 0)\n", b->name, per_iter);
}
```

Luego el script de CI corre este harness en dos commits, parsea la salida y
decide. La ventaja es control total; la desventaja es que tú mismo tienes que
implementar todo lo que criterion ya hace: warm-up, sample size adaptativo,
estadística, formato, etc.

### 11.4 Google Benchmark (C++)

Si el proyecto es C++, [Google Benchmark](https://github.com/google/benchmark)
tiene exactamente la misma capacidad que criterion:

```bash
./my_benchmark --benchmark_format=json --benchmark_out=main.json
# ... checkout PR ...
./my_benchmark --benchmark_format=json --benchmark_out=pr.json
compare.py benchmarks main.json pr.json
```

El script `compare.py` viene con Google Benchmark y hace tests de hipótesis
tipo U-test.

---

## 12. El problema del ruido en CI

Si pones un benchmark en GitHub Actions con runners compartidos, vas a odiar
los regression benchmarks. La razón es simple: **los runners compartidos tienen
ruido del 10-30% entre corridas**, y con ese ruido, el sistema de alertas grita
todo el tiempo.

Fuentes de ruido en un runner compartido:

```
┌────────────────────────────────────────────────────────────────┐
│                   FUENTES DE RUIDO EN CI                       │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  1. HARDWARE VARIABLE                                          │
│     ├── GitHub Actions rota entre hosts físicos                │
│     ├── CPUs distintas (Xeon, EPYC, ARM)                       │
│     └── Generaciones distintas (Skylake vs Ice Lake)           │
│                                                                │
│  2. NOISY NEIGHBORS (virtualización)                           │
│     ├── Otras VMs usando el mismo socket                       │
│     ├── Cache L3 compartida, bandwidth de memoria disputado    │
│     └── Thermal throttling cuando el host está cargado         │
│                                                                │
│  3. FEATURES DEL PROCESADOR NO CONTROLADAS                     │
│     ├── Turbo boost activo                                     │
│     ├── Frecuencia dinámica (governor = schedutil)             │
│     ├── SMT (hyperthreading) con otro proceso contendiendo     │
│     └── ASLR aleatorizando layout de memoria                   │
│                                                                │
│  4. SOFTWARE BACKGROUND                                        │
│     ├── systemd, journald, snapd escribiendo a disco           │
│     ├── Actualización del runner a mitad del job               │
│     └── Docker overhead, cgroups                               │
│                                                                │
│  5. ENTORNO DEL BENCHMARK                                      │
│     ├── Caches fríos vs calientes entre runs                   │
│     ├── Memoria fragmentada en runs consecutivos               │
│     └── Distintas versiones de glibc / kernel según imagen     │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

En un runner como `ubuntu-latest` de GitHub Actions, he visto el mismo
benchmark variar ±25% entre corridas consecutivas del mismo commit. Con ese
ruido, cualquier umbral de alerta razonable (digamos, 10%) genera falsos
positivos constantemente.

---

## 13. Estrategias para reducir ruido

Hay un conjunto de técnicas que, aplicadas juntas, reducen el ruido de ±25% a
±2% en muchos casos:

### 13.1 Pinning de CPU

```bash
taskset -c 2 ./my_bench
```

Fija el proceso al core 2 (evita migraciones entre cores → cache fría).

### 13.2 Desactivar turbo boost

```bash
# Intel:
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
# AMD:
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost
```

El turbo hace que la frecuencia dependa de la temperatura, la carga del vecino
y la duración del job. Imposible de reproducir.

### 13.3 Governor a `performance`

```bash
sudo cpupower frequency-set --governor performance
```

Fija la frecuencia al máximo sin turbo. Esto elimina la variabilidad de
`schedutil`/`powersave`.

### 13.4 Aislar cores (isolcpus)

En el kernel boot:

```
isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3
```

Reserva cores 2 y 3 para que el scheduler no los use para nada más. Los
benchmarks corren ahí.

### 13.5 Desactivar SMT (hyperthreading)

```bash
echo off | sudo tee /sys/devices/system/cpu/smt/control
```

SMT hace que dos hilos compitan por la misma unidad de ejecución → ruido
enorme si el otro hilo está activo.

### 13.6 Desactivar ASLR

```bash
setarch $(uname -m) -R ./my_bench
```

ASLR aleatoriza el layout de memoria, que **sí afecta al rendimiento** (por
alineamientos diferentes, cache sets distintos). Desactívalo para medir.

### 13.7 Múltiples muestras + estadística robusta

En lugar de "correr el benchmark una vez", corre 20-50 veces y usa la **mediana**
o el **mínimo** (no la media). El mínimo es el "mejor caso físicamente posible
hoy" y es sorprendentemente estable.

### 13.8 Interleaving de runs

En lugar de:

```
baseline × 20, candidate × 20
```

haz:

```
baseline, candidate, baseline, candidate, ... (20 × 2)
```

Si la máquina se calienta o el thermal throttling entra en juego, afecta a
ambos por igual.

Esto es lo que hace `poop` por defecto.

### 13.9 Combinando todo: script de preparación

```bash
#!/usr/bin/env bash
# bench_env.sh - prepara el sistema para un benchmark estable

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Este script debe correrse como root (usa sudo)"
    exit 1
fi

# Governor a performance
cpupower frequency-set --governor performance >/dev/null

# Desactivar turbo
if [ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo
elif [ -f /sys/devices/system/cpu/cpufreq/boost ]; then
    echo 0 > /sys/devices/system/cpu/cpufreq/boost
fi

# Desactivar SMT si existe
if [ -f /sys/devices/system/cpu/smt/control ]; then
    echo off > /sys/devices/system/cpu/smt/control
fi

# Desactivar ASLR global
echo 0 > /proc/sys/kernel/randomize_va_space

# Desactivar NMI watchdog (puede interrumpir el benchmark)
echo 0 > /proc/sys/kernel/nmi_watchdog

# Reducir ruido de transparent huge pages
echo never > /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null || true

# Flush caches para punto de partida limpio
sync && echo 3 > /proc/sys/vm/drop_caches

echo "Sistema preparado para benchmarks estables"
```

Y un script de restauración (`bench_env_restore.sh`) que revierte todo esto al
acabar el job.

**Advertencia**: estas opciones **no están disponibles** en runners compartidos
de GitHub Actions. Requieren **self-hosted runners** o servicios específicos.

---

## 14. Runners dedicados (self-hosted) para estabilidad

La única forma de tener regression benchmarks fiables en CI es ejecutarlos en
hardware **controlado**, **dedicado** y **estable**. Opciones:

### 14.1 Self-hosted GitHub Actions runner

Montas un servidor físico (o una VM dedicada con CPU pinning a nivel hypervisor)
y registras un runner de GitHub Actions sobre él. Todas tus precauciones del
apartado anterior se aplican una vez y persisten.

**Pros**: control total, barato a largo plazo, reproducibilidad alta.
**Contras**: hay que mantenerlo (parches, espacio, red), hay que securizarlo
(los workflows pueden ejecutar código arbitrario).

### 14.2 bare-metal services

- **Equinix Metal**, **Hetzner**, **Scaleway**: VPS bare-metal que puedes dedicar
- **Oxidecomputer**: hardware controlado de extremo a extremo

### 14.3 Servicios especializados

- **Codspeed.io**: runner específico para benchmarks que corre en contenedores
  con instrumentación determinista (usa Valgrind bajo el capó, similar a iai)
- **Bencher.dev**: servicio de tracking de benchmarks sobre tus propios runners

### 14.4 Trade-off: velocidad vs estabilidad

| Opción                  | Tiempo por run | Estabilidad  | Coste operativo |
|-------------------------|----------------|--------------|-----------------|
| GitHub-hosted (shared)  | Rápido         | Muy baja     | Cero            |
| Self-hosted pinned      | Igual          | Alta         | Mantenimiento   |
| Codspeed (virtualizado, instruments) | Medio (5-10× lento) | Perfecta     | Servicio        |
| iai sobre runner shared | Lento (callgrind) | Perfecta    | Ninguno         |

**Elección pragmática para proyectos OSS pequeños**: usa **iai** (o
iai-callgrind) sobre runners compartidos. Paga el coste de 5-20× en tiempo
de ejecución a cambio de determinismo total, sin necesidad de mantener hardware.

---

## 15. Umbrales de alerta: evitando el boy-who-cried-wolf

Un sistema de alertas que falsos-positiviza constantemente no solo es inútil:
es **activamente dañino**, porque entrena a los humanos a ignorar las alertas.
Cuando llega la regresión real, nadie la mira.

La configuración de umbrales es, por tanto, tan importante como el benchmark
en sí. Algunas reglas:

### Regla 1: el umbral debe ser mayor que el ruido real

Mide primero **cuánto ruido tiene tu runner** corriendo el mismo commit N veces.
Si el σ (desviación estándar) del runner es 3%, no pongas un umbral del 5% —
pon mínimo 3σ = 9%, o mejor aún medir varias veces y decidir estadísticamente.

### Regla 2: distintos umbrales por benchmark

No todos los benchmarks tienen el mismo ruido. Un benchmark que corre en 10 ns
tiene **mucho más** ruido relativo que uno que corre en 10 ms. Configura
umbrales por benchmark, no globales.

### Regla 3: umbrales por criticidad

Benchmarks críticos (ruta caliente del producto) → umbral estricto (5%).
Benchmarks secundarios (utilidades) → umbral laxo (20%).

### Regla 4: ventanas deslizantes vs puntos aislados

En lugar de "este commit es 10% más lento", usa "los últimos 5 commits son
consistentemente 10% más lentos que los 5 anteriores". Más robusto contra
glitches.

### Regla 5: alertas como warnings antes de errors

Un sistema maduro tiene tres niveles:

| Nivel    | Delta          | Acción                          |
|----------|----------------|---------------------------------|
| Verde    | < 5%           | Nada, silencioso                |
| Amarillo | 5% — 10%       | Comentario en PR, no bloquea    |
| Rojo     | > 10%          | Comentario en PR, bloquea merge |

El nivel amarillo es crítico: es donde se atrapan las regresiones antes de
que se acumulen.

### Regla 6: mejoras también se reportan

Si el PR **mejora** el rendimiento en 15%, eso también merece un comentario.
Por dos razones:

1. **Celebra** al autor por la optimización
2. Es una oportunidad para **actualizar el baseline** (el próximo commit no
   debe perder esa mejora)

---

## 16. Detección estadística de regresiones

La pregunta "¿este PR regresó?" es, en realidad, la pregunta "¿la distribución
de tiempos del candidato es significativamente distinta de la del baseline?".
Esto es un problema clásico de **comparación de dos muestras**.

### 16.1 El test de Welch

Asume que las muestras son aproximadamente normales (centrales: media, varianza).
Calcula un *t-statistic* y un *p-value*:

```
t = (mean_A - mean_B) / sqrt(var_A/n_A + var_B/n_B)
```

Si `p < α` (típicamente α = 0.05), rechazas la hipótesis nula de "son iguales"
y declaras regresión.

**Problema**: las distribuciones de tiempos de benchmark **no son normales**.
Tienen colas largas (siempre positivas, con outliers a la derecha).

### 16.2 Mann-Whitney U (Wilcoxon rank-sum)

Es un test **no paramétrico**: no asume normalidad. En lugar de comparar medias,
compara las **posiciones (ranks)** de las muestras combinadas. Si la mayoría
de muestras del candidato tienen rangos mayores que las del baseline, hay
evidencia de que el candidato es más lento.

Es lo que usa `compare.py` de Google Benchmark. Es el test **recomendado** para
regression benchmarking.

### 16.3 Bootstrap

Alternativa moderna: *resamplea* las dos muestras con reemplazo N=10000 veces,
calcula la diferencia de medianas en cada resample, y construye un IC empírico.
Si ese IC no incluye 0, hay regresión.

Es lo que hace **criterion** por dentro. Robusto, sin supuestos.

### 16.4 Effect size, no solo significancia

Un test puede decir "estadísticamente significativo" con p < 0.001 para un
cambio del 0.5%. ¿Te importa ese 0.5%? Probablemente no. Por eso necesitas
combinar **dos criterios**:

1. **Significancia estadística**: el cambio es real (p < 0.05)
2. **Tamaño del efecto**: el cambio es práctico (|delta| > threshold%)

Solo cuando **ambos** se cumplen, declaras regresión. Si es significativo pero
pequeño, lo reportas como "improvement/regression dentro del ruido". Si es
grande pero no significativo, pides más muestras.

---

## 17. Integración con GitHub Actions

GitHub Actions es la plataforma CI más común, y tiene soporte razonable para
regression benchmarks, siempre que aceptes las limitaciones de los runners
compartidos.

### 17.1 Anatomía de un workflow de regression benchmark

```yaml
# .github/workflows/bench.yml
name: Benchmarks

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  bench:
    runs-on: ubuntu-latest
    # runs-on: [self-hosted, linux, bench]  # <-- para runner dedicado

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0  # Necesitamos historia completa para comparar

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable
        with:
          toolchain: stable

      - name: Cache cargo
        uses: actions/cache@v4
        with:
          path: |
            ~/.cargo/registry
            ~/.cargo/git
            target
          key: ${{ runner.os }}-bench-${{ hashFiles('**/Cargo.lock') }}

      # Baseline: el commit base del PR (o main para push directo)
      - name: Determine base
        id: base
        run: |
          if [ "${{ github.event_name }}" = "pull_request" ]; then
            echo "base_sha=${{ github.event.pull_request.base.sha }}" >> $GITHUB_OUTPUT
          else
            echo "base_sha=${{ github.event.before }}" >> $GITHUB_OUTPUT
          fi

      - name: Save baseline
        run: |
          git checkout ${{ steps.base.outputs.base_sha }}
          cargo bench --bench my_bench -- --save-baseline base
          git checkout ${{ github.sha }}

      - name: Run benchmarks against baseline
        run: |
          cargo bench --bench my_bench -- --baseline base 2>&1 | tee bench.txt

      - name: Check for regression
        run: |
          if grep -q "Performance has regressed" bench.txt; then
            echo "::error::Performance regression detected"
            grep -B1 "Performance has regressed" bench.txt
            exit 1
          fi
```

Este es el esqueleto. En la práctica querrás extenderlo.

### 17.2 Problemas con este workflow ingenuo

1. **El baseline se mide en el mismo runner** que el candidato, pero eso no
   implica **la misma condición** del runner (thermal throttling entre runs).
2. **GitHub Actions reinicia el runner** para cada job → se pierde cache.
3. **Los runners compartidos tienen ruido enorme** → falsos positivos.
4. **`cargo bench` tarda mucho** en debug → el job entero puede tardar 20+ min.

Soluciones:

1. **Interleaving**: corre baseline y candidato mezclados, no secuencialmente.
2. **Warm-up del runner**: corre algo neutro primero para estabilizar el core.
3. **Umbral amplio** o usa iai.
4. **Cache agresivo** de dependencias y benchmarks que no cambian.

---

## 18. Workflow completo: PR comparando contra main

Aquí va un workflow más realista, que comenta en el PR con los resultados y
que soporta múltiples benchmarks:

```yaml
name: Performance Regression Check

on:
  pull_request:
    branches: [main]

permissions:
  pull-requests: write
  contents: read

jobs:
  bench:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout PR
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable

      - name: Cache cargo
        uses: Swatinem/rust-cache@v2

      - name: Setup benchmark environment
        run: |
          echo "Preparando entorno (best-effort en runner compartido)"
          # En self-hosted runner iría aquí cpupower, turbo off, etc.

      - name: Checkout base and build
        run: |
          git checkout ${{ github.event.pull_request.base.sha }}
          cargo build --release --benches

      - name: Run baseline benchmarks
        run: |
          cargo bench --bench critical_bench -- --save-baseline base --sample-size 50 \
            --measurement-time 10

      - name: Checkout PR head
        run: |
          git checkout ${{ github.event.pull_request.head.sha }}

      - name: Run PR benchmarks
        run: |
          cargo bench --bench critical_bench -- --baseline base --sample-size 50 \
            --measurement-time 10 2>&1 | tee bench.txt

      - name: Parse results
        id: parse
        run: |
          python3 .ci/parse_criterion_output.py bench.txt > report.md
          echo "has_regression=$(grep -q 'regressed' bench.txt && echo true || echo false)" >> $GITHUB_OUTPUT

      - name: Comment PR
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('report.md', 'utf8');
            const body = `## Benchmark Results\n\n${report}\n\n_Runner: ubuntu-latest (shared, noisy)_`;
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: body
            });

      - name: Fail on regression
        if: steps.parse.outputs.has_regression == 'true'
        run: |
          echo "::error::Regression detected. See PR comment for details."
          exit 1
```

Y el script `parse_criterion_output.py`:

```python
#!/usr/bin/env python3
"""Parsea la salida de criterion --baseline y genera un reporte markdown."""
import re
import sys

def parse_criterion(text):
    # Criterion imprime algo como:
    #   bench_name   time:   [120.45 ns 121.23 ns 122.14 ns]
    #                change: [-2.3% -1.8% -1.4%] (p = 0.00 < 0.05)
    #                Performance has improved.
    blocks = re.split(r'\n(?=\S)', text)
    results = []
    for block in blocks:
        name_match = re.match(r'(\S+)', block)
        time_match = re.search(r'time:\s+\[([\d.]+)\s+(\w+)', block)
        change_match = re.search(r'change:\s+\[([-+\d.]+)%\s+([-+\d.]+)%\s+([-+\d.]+)%\]', block)
        verdict_match = re.search(r'Performance has (improved|regressed)', block)
        noise_match = re.search(r'Change within noise threshold', block)
        nochange_match = re.search(r'No change in performance detected', block)

        if name_match and time_match:
            name = name_match.group(1)
            time_val = float(time_match.group(1))
            time_unit = time_match.group(2)
            if change_match:
                change = float(change_match.group(2))
            else:
                change = 0.0
            if verdict_match:
                verdict = verdict_match.group(1)
            elif noise_match:
                verdict = 'noise'
            elif nochange_match:
                verdict = 'nochange'
            else:
                verdict = 'unknown'
            results.append((name, time_val, time_unit, change, verdict))
    return results

def emit_markdown(results):
    print("| Benchmark | Time | Change | Verdict |")
    print("|-----------|------|--------|---------|")
    icons = {
        'improved': ':green_heart: improved',
        'regressed': ':red_circle: **REGRESSED**',
        'noise': ':white_circle: within noise',
        'nochange': ':white_circle: no change',
        'unknown': ':question: unknown',
    }
    for name, t, u, ch, v in results:
        sign = '+' if ch >= 0 else ''
        print(f"| `{name}` | {t:.2f} {u} | {sign}{ch:.2f}% | {icons[v]} |")

if __name__ == '__main__':
    text = open(sys.argv[1]).read()
    results = parse_criterion(text)
    emit_markdown(results)
```

Ahora cada PR recibe un comentario como:

```markdown
## Benchmark Results

| Benchmark | Time | Change | Verdict |
|-----------|------|--------|---------|
| `parse_simple/small` | 121.23 ns | -1.87% | :green_heart: improved |
| `parse_simple/large` | 45.43 µs | +9.35% | :red_circle: **REGRESSED** |
| `sort/random_1k` | 12.45 µs | +0.12% | :white_circle: within noise |

_Runner: ubuntu-latest (shared, noisy)_
```

Esto es lo que buscas: información accionable en cada PR, antes del merge.

---

## 19. Persistencia de resultados históricos

Comparar PR contra `main` es útil, pero **no te protege contra la acumulación
gradual** de regresiones pequeñas (sección 2). Para eso necesitas **historial**.

La forma más simple de historial: **una rama dedicada** (`benchmarks-history`)
donde pusheas los resultados de `main` después de cada merge.

```yaml
# .github/workflows/bench-main.yml
name: Main Benchmark Tracking

on:
  push:
    branches: [main]

jobs:
  track:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable

      - name: Run benchmarks
        run: |
          cargo bench --bench critical_bench -- --output-format bencher \
            > bench_result.txt

      - name: Commit to tracking branch
        run: |
          git config user.name "bench-bot"
          git config user.email "bot@example.com"
          git fetch origin benchmarks-history || true
          git checkout -B benchmarks-history origin/benchmarks-history 2>/dev/null || \
            git checkout --orphan benchmarks-history
          mkdir -p history
          cp bench_result.txt history/$(date -u +%Y%m%d_%H%M%S)_${{ github.sha }}.txt
          git add history
          git commit -m "bench: ${{ github.sha }}"
          git push origin benchmarks-history
```

Ahora tienes un directorio `history/` con timestamps y shas. Puedes:

- Graficar tendencias históricas
- Detectar acumulación ("los últimos 10 commits suman +12%")
- Hacer bisect sobre una ventana de regresión

### Alternativa: archivos JSON estructurados

En lugar de texto, guarda JSON:

```json
{
  "timestamp": "2026-04-06T12:34:56Z",
  "commit": "a1b2c3d",
  "runner": "ubuntu-22.04-shared",
  "benchmarks": [
    { "name": "parse_simple/small", "ns": 121.23, "stddev": 0.85 },
    { "name": "parse_simple/large", "ns": 45430.12, "stddev": 312.5 }
  ]
}
```

Un archivo por commit. Es trivial de parsear luego para cualquier análisis.

---

## 20. Visualización: dashboards y tendencias

Los números sin gráficas son casi inútiles para ver tendencias. Tres niveles
de visualización, de menos a más sofisticado:

### Nivel 1: GitHub Pages con gráfica simple

Un workflow que, tras cada merge a main, regenera `docs/index.html` con una
gráfica (usando Chart.js o similar) de los últimos N commits. Pusheado a la
rama `gh-pages`, accesible en `https://<user>.github.io/<repo>/`.

Muy básico pero suficiente para proyectos pequeños.

### Nivel 2: github-action-benchmark

Existe una [GitHub Action](https://github.com/benchmark-action/github-action-benchmark)
que hace exactamente esto: toma la salida de tu benchmark tool (criterion,
Google Benchmark, pytest-benchmark, etc.), la pushea a gh-pages, y mantiene
una gráfica histórica automáticamente.

```yaml
- name: Store benchmark result
  uses: benchmark-action/github-action-benchmark@v1
  with:
    tool: 'cargo'
    output-file-path: bench_output.txt
    github-token: ${{ secrets.GITHUB_TOKEN }}
    auto-push: true
    alert-threshold: '110%'
    comment-on-alert: true
    fail-on-alert: true
```

Con eso tienes dashboard + alertas + comentarios en PRs, prácticamente gratis.

### Nivel 3: servicio dedicado

Para proyectos grandes (TiDB, Materialize, Ruff, Polars), los servicios como
**bencher.dev**, **codspeed**, o dashboards internos (Grafana + Prometheus con
datos de benchmarks) son la solución natural.

---

## 21. Servicios externos: Bencher, Codspeed, nyrkiö

Pagar por un servicio a veces es la solución correcta.

### Bencher.dev

[Bencher](https://bencher.dev/) es un servicio open-source (hosted o self-hosted)
para tracking de benchmarks. Sus características:

- Soporta criterion, Google Benchmark, pytest-benchmark, JMH, etc.
- CLI que ingiere la salida de tu benchmark y la pushea a Bencher
- Threshold management (statistical, percentage, etc.)
- PR comments automáticos
- Dashboard con historia, alertas, comparaciones

Ejemplo de uso:

```bash
bencher run --project my-project \
    --token $BENCHER_TOKEN \
    --branch main \
    --testbed ci-runner \
    --adapter rust_criterion \
    'cargo bench'
```

Bencher captura la salida, la parsea con el adaptador indicado, y la sube a su
backend. Muy ergonómico.

### Codspeed

[Codspeed.io](https://codspeed.io/) adopta un enfoque distinto: **corre tus
benchmarks en sus propios runners con instrumentación determinista** (usando
Valgrind, similar a iai). El resultado: ruido casi cero, reportes perfectos,
pero tus benchmarks son 5-20× más lentos.

Configuración:

```yaml
- uses: CodSpeedHQ/action@v3
  with:
    token: ${{ secrets.CODSPEED_TOKEN }}
    run: cargo codspeed run
```

Pros: sin necesidad de mantener runners, determinismo total.
Contras: coste, lentitud, solo Rust/Python/JS por ahora.

### Nyrkiö

[Nyrkiö](https://nyrkio.com/) es un servicio que analiza **tus datos históricos**
(los que ya guardas en JSON) y detecta **change points** automáticamente usando
algoritmos como PELT (Pruned Exact Linear Time). Esto es muy potente porque:

- Ignora ruido por muy grande que sea
- Detecta "el rendimiento cambió **aquí**, en este commit" automáticamente
- Funciona sobre cualquier benchmark tool

La filosofía: "acepta el ruido como inevitable, pero aplica estadística para
encontrar el momento exacto en que hay un change point real".

### ¿Cuál elegir?

| Si...                                      | Usa...                 |
|--------------------------------------------|------------------------|
| Tienes hardware dedicado y quieres control | Self-hosted + criterion + tu propio dashboard |
| Quieres determinismo sin hardware          | iai / iai-callgrind    |
| Quieres algo "just works" y no te importa pagar | Codspeed           |
| Quieres OSS y bueno                        | Bencher                |
| Ya tienes historial y quieres análisis automático | Nyrkiö           |

---

## 22. Bisect asistido para cazar regresiones

Cuando el sistema de CI detecta una regresión, o peor, cuando notas una
regresión semanas más tarde, necesitas localizar **qué commit** la introdujo.
`git bisect` es la herramienta, pero con benchmarks hay un matiz: el test que
bisect ejecuta debe **ser determinista** para decidir "bueno/malo".

### 22.1 Bisect con un benchmark binario

```bash
git bisect start
git bisect good v1.0.0       # último commit bueno conocido
git bisect bad HEAD          # actual, malo

git bisect run bash -c '
  cargo build --release --bench my_bench && \
  RESULT=$(./target/release/deps/my_bench_* --bench --exact critical_fn 2>/dev/null | \
           grep -oP "\\d+\\s+ns/iter") && \
  # Extrae el número y decide
  NS=$(echo $RESULT | cut -d" " -f1) && \
  if [ $NS -gt 1000 ]; then exit 1; else exit 0; fi
'
```

Este script sirve de oráculo para bisect: exit 0 = bueno (rápido), exit 1 = malo
(lento). Bisect encuentra el commit que cruzó el umbral en ~log₂(N) pasos.

### 22.2 Bisect ruidoso: múltiples muestras

Si el ruido es alto, una sola medición puede dar un veredicto falso. La
solución es correr **N veces por commit** y usar la mediana:

```bash
#!/usr/bin/env bash
# bisect_oracle.sh
set -e
cargo build --release --bench my_bench
RESULTS=()
for i in 1 2 3 4 5; do
    NS=$(./target/release/deps/my_bench_* --bench --exact critical_fn | grep -oP '\d+')
    RESULTS+=($NS)
done
# Mediana de 5 muestras
SORTED=($(printf "%d\n" "${RESULTS[@]}" | sort -n))
MEDIAN=${SORTED[2]}
if [ "$MEDIAN" -gt 1000 ]; then
    exit 1
else
    exit 0
fi
```

### 22.3 Bisect automatizado como workflow

Para proyectos grandes, tener un botón "¿cuándo regresó?" es oro. Un workflow
de GitHub Actions puede correr bisect automáticamente tras una alerta:

```yaml
name: Bisect Regression

on:
  workflow_dispatch:
    inputs:
      good_sha:
        required: true
      bad_sha:
        required: true
      benchmark_name:
        required: true

jobs:
  bisect:
    runs-on: [self-hosted, bench]
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Run bisect
        run: |
          git bisect start
          git bisect good ${{ inputs.good_sha }}
          git bisect bad ${{ inputs.bad_sha }}
          git bisect run ./.ci/bisect_oracle.sh ${{ inputs.benchmark_name }} | tee bisect.log
          CULPRIT=$(grep "first bad commit" bisect.log | awk '{print $1}')
          echo "Culprit: $CULPRIT"
```

---

## 23. Microbenchmarks vs macrobenchmarks en CI

La tentación es poner **todos** los benchmarks en CI. Resistirla. El trade-off
es:

| Tipo              | Tiempo          | Señal    | Ruido  | Valor en CI |
|-------------------|-----------------|----------|--------|-------------|
| Microbenchmark (< 1 µs) | Rápido  | Alta precisión teórica | Muy alto (%) | Medio |
| Microbenchmark (~ 1 ms) | Rápido  | Alta     | Medio  | Alto    |
| Macrobenchmark (~ 1 s)  | Medio   | Realista | Bajo   | Muy alto|
| End-to-end (> 10 s)    | Lento   | Muy realista | Muy bajo | Alto pero lento |

### Regla práctica

Para CI on-every-PR:

- **Macrobenchmarks** (10 — 100 ms cada uno) de las rutas más críticas
- Máximo ~20 benchmarks
- Tiempo total del job: < 10 minutos

Para CI diario/semanal (cron):

- **Todos** los benchmarks, incluidos los micro
- Macro-benchmarks más pesados (workloads completos)
- Puede tardar horas en hardware dedicado

### El error clásico

Poner **cientos** de microbenchmarks en el CI-on-PR. El job tarda 45 minutos,
nadie puede mergear rápido, y el ruido individual de cada microbenchmark genera
tantos falsos positivos que todo el mundo ignora los resultados.

Mejor: 10-15 macrobenchmarks cuidadosamente elegidos que reflejen los paths
reales del producto.

---

## 24. Errores comunes al montar regression CI

Después de todo este recorrido, un catálogo compacto de errores que vas a
cometer (o a heredar) y cómo arreglarlos:

1. **Medir en runner compartido sin ajustar umbrales**
   → falsos positivos constantes. Mide primero el ruido de tu runner, ajusta
   umbrales.

2. **Un solo umbral global**
   → benchmarks rápidos disparan siempre, benchmarks lentos nunca. Umbrales
   por benchmark.

3. **No guardar historial**
   → solo comparas contra el commit anterior, nunca detectas drift gradual.
   Añade un job de tracking sobre main.

4. **Comparar contra baseline obsoleto**
   → si el baseline es de hace 6 meses, el código moderno siempre "regresa"
   por razones legítimas (nuevas features). Actualiza el baseline periódicamente.

5. **Bench sin warm-up**
   → la primera iteración siempre es más lenta (cold cache, lazy init, TLB
   frío). Usa `--warm-up-time` o haz tu propio warm-up explícito.

6. **Medir con debug build**
   → un benchmark debug puede ser 10-100× más lento. Siempre `--release`.

7. **Olvidar `black_box`**
   → el compilador optimiza el benchmark a `return 0`. Usa `black_box` en Rust
   o `asm volatile("" : : "r"(x) : "memory")` en C.

8. **Bench de funciones triviales**
   → "bench_add" es 1 ns, 100% ruido, 0% señal. Bench funciones que hagan algo
   real.

9. **Sample size = 3**
   → estadísticamente inútil. Mínimo 20-50 muestras por benchmark.

10. **No documentar el runner**
    → luego no puedes explicar las diferencias. Registra: modelo CPU, frecuencia,
    flags del scheduler, compilador, glibc, kernel.

11. **Cambiar el benchmark y compararlo consigo mismo**
    → si modificas el benchmark Y el código, no sabes si el cambio viene del
    código o del benchmark. Benchmarks son **código** y deben versionarse con
    cuidado.

12. **Sin gating**
    → si el CI solo avisa pero nunca bloquea, nadie lo arregla. Define un
    threshold que bloquea el merge.

13. **Gating demasiado agresivo**
    → si el CI bloquea por cambios legítimos, se desactiva "temporalmente" y
    nunca se reactiva. Equilibrio.

14. **No tener override**
    → a veces una regresión es aceptable (un trade-off por seguridad, corrección,
    etc.). Tener un label `[perf-regression-approved]` que salta el gate es
    pragmático.

---

## 25. Proyecto de práctica: regression_lab/

Vamos a montar un pequeño proyecto que integra todo lo anterior. La estructura
propuesta:

```
regression_lab/
├── Cargo.toml
├── src/
│   └── lib.rs
├── benches/
│   ├── criterion_bench.rs
│   └── iai_bench.rs
├── .ci/
│   ├── parse_criterion_output.py
│   ├── compare_history.py
│   └── bench_env.sh
├── .github/
│   └── workflows/
│       ├── bench-pr.yml
│       └── bench-main.yml
└── README.md
```

### 25.1 Cargo.toml

```toml
[package]
name = "regression_lab"
version = "0.1.0"
edition = "2021"

[dependencies]

[dev-dependencies]
criterion = { version = "0.5", features = ["html_reports"] }
iai = "0.1"

[[bench]]
name = "criterion_bench"
harness = false

[[bench]]
name = "iai_bench"
harness = false

[profile.bench]
opt-level = 3
lto = "thin"
codegen-units = 1
debug = true   # Para símbolos en perf
```

### 25.2 Código bajo test: src/lib.rs

```rust
//! Una pequeña biblioteca con funciones "críticas" que vamos a monitorizar.

/// Suma los elementos de una slice de f64.
pub fn sum_slice(data: &[f64]) -> f64 {
    let mut acc = 0.0;
    for &x in data {
        acc += x;
    }
    acc
}

/// Filtra y cuenta elementos positivos.
pub fn count_positive(data: &[i64]) -> usize {
    data.iter().filter(|&&x| x > 0).count()
}

/// Versión branchless del anterior (suma 1 si positivo, 0 si no).
pub fn count_positive_branchless(data: &[i64]) -> usize {
    data.iter().map(|&x| (x > 0) as usize).sum()
}

/// Búsqueda lineal en un slice no ordenado.
pub fn linear_search(data: &[i32], target: i32) -> Option<usize> {
    for (i, &x) in data.iter().enumerate() {
        if x == target {
            return Some(i);
        }
    }
    None
}
```

### 25.3 Benchmarks criterion

```rust
// benches/criterion_bench.rs
use criterion::{black_box, criterion_group, criterion_main, Criterion, BenchmarkId};
use regression_lab::*;

fn bench_sum(c: &mut Criterion) {
    let mut group = c.benchmark_group("sum");
    for size in [100, 1_000, 10_000].iter() {
        let data: Vec<f64> = (0..*size).map(|i| i as f64 * 0.5).collect();
        group.bench_with_input(BenchmarkId::from_parameter(size), size, |b, _| {
            b.iter(|| sum_slice(black_box(&data)))
        });
    }
    group.finish();
}

fn bench_count_positive(c: &mut Criterion) {
    let mut group = c.benchmark_group("count_positive");
    group.noise_threshold(0.05);
    group.sample_size(100);

    let data: Vec<i64> = (0..10_000).map(|i| (i as i64 * 1327 + 17) % 200 - 100).collect();

    group.bench_function("branchy", |b| {
        b.iter(|| count_positive(black_box(&data)))
    });
    group.bench_function("branchless", |b| {
        b.iter(|| count_positive_branchless(black_box(&data)))
    });
    group.finish();
}

fn bench_linear_search(c: &mut Criterion) {
    let data: Vec<i32> = (0..1_000).collect();
    c.bench_function("linear_search_found", |b| {
        b.iter(|| linear_search(black_box(&data), black_box(500)))
    });
    c.bench_function("linear_search_notfound", |b| {
        b.iter(|| linear_search(black_box(&data), black_box(-1)))
    });
}

criterion_group!(benches, bench_sum, bench_count_positive, bench_linear_search);
criterion_main!(benches);
```

### 25.4 Benchmarks iai (deterministas)

```rust
// benches/iai_bench.rs
use iai::{black_box, main};
use regression_lab::*;

fn iai_sum_1000() -> f64 {
    let data: Vec<f64> = (0..1_000).map(|i| i as f64 * 0.5).collect();
    sum_slice(black_box(&data))
}

fn iai_count_branchy() -> usize {
    let data: Vec<i64> = (0..10_000).map(|i| (i as i64 * 1327 + 17) % 200 - 100).collect();
    count_positive(black_box(&data))
}

fn iai_count_branchless() -> usize {
    let data: Vec<i64> = (0..10_000).map(|i| (i as i64 * 1327 + 17) % 200 - 100).collect();
    count_positive_branchless(black_box(&data))
}

fn iai_linear_search() -> Option<usize> {
    let data: Vec<i32> = (0..1_000).collect();
    linear_search(black_box(&data), black_box(500))
}

iai::main!(iai_sum_1000, iai_count_branchy, iai_count_branchless, iai_linear_search);
```

### 25.5 Workflow bench-pr.yml (comparativo en PR)

```yaml
name: Benchmark PR

on:
  pull_request:
    branches: [main]

permissions:
  pull-requests: write
  contents: read

jobs:
  bench:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable

      - name: Cache
        uses: Swatinem/rust-cache@v2

      - name: Install Valgrind (for iai)
        run: sudo apt-get update && sudo apt-get install -y valgrind

      - name: Run iai on base (deterministic)
        run: |
          git checkout ${{ github.event.pull_request.base.sha }}
          cargo bench --bench iai_bench 2>&1 | tee iai_base.txt

      - name: Run iai on PR (deterministic)
        run: |
          git checkout ${{ github.event.pull_request.head.sha }}
          cargo bench --bench iai_bench 2>&1 | tee iai_pr.txt

      - name: Compare iai results
        id: compare
        run: |
          python3 .ci/compare_iai.py iai_base.txt iai_pr.txt > report.md
          if grep -q "REGRESSION" report.md; then
            echo "has_regression=true" >> $GITHUB_OUTPUT
          else
            echo "has_regression=false" >> $GITHUB_OUTPUT
          fi

      - name: Comment PR with results
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('report.md', 'utf8');
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: `## Deterministic Benchmark Results (iai)\n\n${report}`
            });

      - name: Fail on regression
        if: steps.compare.outputs.has_regression == 'true'
        run: |
          echo "::error::Instruction count regression detected"
          exit 1
```

### 25.6 Script compare_iai.py

```python
#!/usr/bin/env python3
"""Compara dos salidas de iai y emite un reporte markdown con umbral por métrica."""
import re
import sys

THRESHOLDS = {
    'instructions': 0.01,   # 1% regresión bloquea
    'l1_accesses': 0.02,
    'estimated_cycles': 0.02,
}

def parse_iai(text):
    results = {}
    current = None
    for line in text.splitlines():
        # Nombre de benchmark es una línea sola, sin indent
        m = re.match(r'^(\w+)$', line)
        if m:
            current = m.group(1)
            results[current] = {}
            continue
        if current:
            m = re.match(r'\s+(Instructions|L1 Accesses|L2 Accesses|RAM Accesses|Estimated Cycles):\s+(\d+)', line)
            if m:
                key = m.group(1).lower().replace(' ', '_')
                val = int(m.group(2))
                results[current][key] = val
    return results

def main():
    base = parse_iai(open(sys.argv[1]).read())
    pr = parse_iai(open(sys.argv[2]).read())
    has_regression = False

    print("| Benchmark | Metric | Base | PR | Delta | Verdict |")
    print("|-----------|--------|------|----|----|---------|")
    for name in pr:
        for metric, pr_val in pr[name].items():
            base_val = base.get(name, {}).get(metric)
            if base_val is None:
                continue
            delta = (pr_val - base_val) / base_val if base_val > 0 else 0
            sign = '+' if delta >= 0 else ''
            threshold = THRESHOLDS.get(metric, 0.05)
            if delta > threshold:
                verdict = ':red_circle: **REGRESSION**'
                has_regression = True
            elif delta < -threshold:
                verdict = ':green_heart: improvement'
            else:
                verdict = ':white_circle: nochange'
            print(f"| `{name}` | {metric} | {base_val} | {pr_val} | {sign}{delta*100:.2f}% | {verdict} |")

    if has_regression:
        print("\n> **REGRESSION** detected in at least one metric above threshold.")

if __name__ == '__main__':
    main()
```

### 25.7 Workflow bench-main.yml (tracking histórico)

```yaml
name: Benchmark Main Tracking

on:
  push:
    branches: [main]

jobs:
  track:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - uses: dtolnay/rust-toolchain@stable

      - uses: Swatinem/rust-cache@v2

      - name: Install Valgrind
        run: sudo apt-get update && sudo apt-get install -y valgrind

      - name: Run iai
        run: cargo bench --bench iai_bench 2>&1 | tee iai_main.txt

      - name: Extract JSON
        run: python3 .ci/iai_to_json.py iai_main.txt > result.json

      - name: Push to history branch
        run: |
          git config user.name "bench-bot"
          git config user.email "bot@noreply.github.com"
          git fetch origin benchmarks-history:benchmarks-history 2>/dev/null || true
          git checkout benchmarks-history 2>/dev/null || git checkout --orphan benchmarks-history
          git rm -rf . 2>/dev/null || true
          mkdir -p history
          cp result.json history/$(date -u +%Y%m%dT%H%M%S)_${{ github.sha }}.json
          git add history
          git commit -m "bench: ${{ github.sha }}"
          git push origin benchmarks-history
```

### 25.8 README.md del proyecto

```markdown
# regression_lab

Laboratorio de regression benchmarking.

## Correr localmente

```bash
# Criterion (tiempo real)
cargo bench --bench criterion_bench

# Iai (instrucciones, determinista)
cargo bench --bench iai_bench
```

## CI

- `bench-pr.yml` corre iai comparando base vs PR y comenta en el PR
- `bench-main.yml` registra resultados de cada commit a main en rama `benchmarks-history`

## Umbrales

Definidos en `.ci/compare_iai.py`:
- Instructions: 1%
- L1 accesses: 2%
- Estimated cycles: 2%
```

### 25.9 Cómo probar el sistema localmente

1. Clonar el repo
2. `cargo bench --bench iai_bench` y anotar los números
3. Editar `src/lib.rs` para introducir una regresión intencional (por ejemplo,
   cambiar `count_positive_branchless` para añadir un if innecesario)
4. `cargo bench --bench iai_bench` y comparar: las instrucciones deben haber
   aumentado
5. Ejecutar el script de comparación manualmente:
   ```bash
   python3 .ci/compare_iai.py iai_base.txt iai_pr.txt
   ```
6. Verificar que el reporte marca la regresión

### 25.10 Extensiones propuestas

Si quieres ir más lejos, añade al proyecto:

- Un workflow de **bisect automático** disparable manualmente
- Un script `.ci/compare_history.py` que calcula el drift sobre los últimos N
  commits de `benchmarks-history`
- Un **dashboard** con github-action-benchmark que se publique en GitHub Pages
- Un equivalente en C puro usando hyperfine + un script compare.py

---

## 26. Ejercicios

1. **Medir el ruido de tu runner**: corre el mismo benchmark (con criterion) 10
   veces en el mismo commit, en tu máquina local. Calcula media y σ. Luego haz
   lo mismo en ubuntu-latest de GitHub Actions. Compara. ¿Cuál sería tu umbral
   mínimo razonable en cada uno?

2. **Regresión intencional**: sobre `regression_lab/`, crea un PR que:
   (a) añada un `clone()` innecesario en `sum_slice`. ¿Detecta iai la regresión?
   ¿La detecta criterion?
   (b) renombre una variable sin cambiar comportamiento. ¿Detectan algo los dos?

3. **Workflow de bisect**: monta un workflow_dispatch que acepte good/bad SHAs,
   corra `git bisect run` con un oráculo que mide con iai, y reporte el commit
   culpable como output del job.

4. **Umbrales por benchmark**: modifica `compare_iai.py` para que los umbrales
   sean específicos por benchmark (JSON externo). Benchmarks marcados como
   "critical" tienen umbral del 1%, el resto del 5%.

5. **Detector de drift**: escribe un script que lea 30 snapshots del directorio
   `history/` (rama `benchmarks-history`) y compare la mediana de los últimos
   10 contra la mediana de los primeros 10. Si el delta supera un 3%, emite
   una alerta.

6. **Bench en C puro**: reproduce un regression harness equivalente en C usando
   hyperfine para el binario + un script bash que compare dos JSON de hyperfine
   y emita verdict.

7. **Iai vs criterion en el mismo PR**: crea un PR que optimice una función
   cambiando el algoritmo (p. ej. usando SIMD con `std::simd`). ¿Qué reporta
   iai? ¿Qué reporta criterion? ¿Cuál es más "verdad"? Escribe un párrafo
   explicando la discrepancia.

8. **Evitando false positives**: toma el workflow de `bench-pr.yml` y añádele
   retry logic: si detecta una regresión, corre el benchmark **tres veces más**
   y solo marca regresión si **todas** lo confirman. Discute por qué esto es
   mejor o peor que aumentar `--sample-size`.

---

## 27. Checklist del tópico

### Conceptual

- [ ] Entiendo la diferencia entre regression test y regression benchmark
- [ ] Sé por qué "death by a thousand cuts" motiva tener regression bench en CI
- [ ] Conozco los tres modelos de baseline (móvil, fijo, trunk-tracking)
- [ ] Sé qué significa baseline fijo vs trunk-tracking y cuándo usar cada uno
- [ ] Entiendo por qué los runners compartidos son ruidosos

### Criterion y herramientas

- [ ] Puedo usar `cargo bench --save-baseline` y `--baseline`
- [ ] Entiendo los 4 veredictos de criterion
- [ ] Sé configurar `noise_threshold`, `significance_level`, `sample_size`
- [ ] Conozco `cargo-criterion` y su salida JSON
- [ ] Puedo montar un benchmark con iai y entender su output
- [ ] Conozco iai-callgrind y por qué es sucesor de iai
- [ ] Sé cuándo usar iai vs criterion

### CI

- [ ] Puedo escribir un workflow de GitHub Actions que corra benchmarks en PR
- [ ] Sé extraer la salida de criterion/iai y comentarla en el PR
- [ ] Conozco la acción `github-action-benchmark`
- [ ] Entiendo los trade-offs de runners compartidos vs self-hosted
- [ ] Puedo preparar un runner self-hosted con governor, turbo off, isolcpus

### Estadística y umbrales

- [ ] Sé que los umbrales deben exceder el ruido del runner
- [ ] Puedo configurar umbrales distintos por benchmark
- [ ] Entiendo el trade-off entre gating estricto y falsos positivos
- [ ] Conozco Mann-Whitney U y bootstrap como alternativas al t-test
- [ ] Distingo significancia estadística del tamaño del efecto

### Operacional

- [ ] Sé montar tracking histórico con rama dedicada
- [ ] Puedo usar git bisect con un oráculo basado en benchmark
- [ ] Conozco Bencher, Codspeed y Nyrkiö y sé cuándo considerarlos
- [ ] Puedo visualizar tendencias con gh-pages o dashboard
- [ ] Sé distinguir micro/macro benchmarks y cuándo poner cada uno en CI

---

## 28. Cierre del Capítulo 4

Con este tópico cerramos el **Capítulo 4: Benchmarking y Profiling**. El
recorrido completo ha sido:

```
┌─────────────────────────────────────────────────────────────────┐
│                  CAPÍTULO 4: BENCHMARKING Y PROFILING           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Sección 1 — Microbenchmarking                                  │
│    T01: Qué es un microbenchmark                                │
│    T02: Medir con precisión                                     │
│    T03: Criterion (Rust)                                        │
│    T04: Google Benchmark (C++)                                  │
│                                                                 │
│  Sección 2 — Profiling de aplicaciones                          │
│    T01: perf en Linux                                           │
│    T02: Flamegraphs                                             │
│    T03: Valgrind/Callgrind                                      │
│    T04: Profiling de memoria                                    │
│                                                                 │
│  Sección 3 — Benchmarking guiado por hipótesis                  │
│    T01: Formular hipótesis                                      │
│    T02: Benchmark comparativo                                   │
│    T03: Regression benchmarks en CI  ← ESTÁS AQUÍ               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Qué has aprendido en el capítulo completo

- A **medir** con la precisión adecuada: con criterion/Google Benchmark,
  entendiendo sample size, warm-up, varianza
- A **explorar** con perf + flamegraphs, entender stacks, identificar hotspots
- A **simular** con callgrind/cachegrind y obtener métricas deterministas
  cuando el hardware real miente
- A **diagnosticar** memoria con heaptrack, DHAT, massif, jemalloc profiling
- A **razonar** sobre rendimiento con el método científico: hipótesis, predicción,
  medición, conclusión, sin saltar a optimizaciones especulativas
- A **comparar** de forma honesta, sin hacer trampa, con el principio de
  equidad y la conciencia de los sesgos
- A **defender** el rendimiento a lo largo del tiempo con CI que detecta
  regresiones antes de que lleguen a producción

### La mentalidad de rendimiento

Por encima de las herramientas concretas, el capítulo ha intentado transmitir
una **mentalidad**:

> El rendimiento no es un número que mides una vez y publicas. Es una
> **propiedad del código** que evoluciona con cada commit, y que requiere
> disciplina continua para preservarla. Si no la monitorizas, se degrada. Si
> no la razonas, optimizas en la dirección equivocada. Si no la mides con
> cuidado, te mientes a ti mismo.

Las tres secciones del capítulo corresponden a los tres verbos que necesitas
dominar:

- **Medir** (S01)
- **Diagnosticar** (S02)
- **Defender** (S03)

Un ingeniero que domina los tres es un ingeniero al que le importa el
rendimiento **en serio**, no como un adorno al final del ciclo de desarrollo.

### Qué viene ahora

En el **Capítulo 5: Ingeniería de Software**, cambiamos de frecuencia. Dejamos
atrás los microsegundos y los flamegraphs para hablar de las **prácticas
colectivas** que sostienen proyectos reales a lo largo del tiempo: git para
proyectos de verdad, code review, pull requests efectivos. Son temas más
"suaves" en apariencia, pero igual de determinantes para la calidad del software
que cualquier optimización de cache.

---

## 29. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T02 — Benchmark comparativo](../T02_Benchmark_comparativo/README.md) | [S03 — Benchmark guiado](../) | [C05 — Ingeniería de Software](../../../C05_Ingenieria_de_Software/) |

---

**Capítulo 4 — Sección 3: Benchmarking Guiado por Hipótesis — Tópico 3 de 3**

**Cierre del Capítulo 4: Benchmarking y Profiling**
