# T02 - llvm-cov: cobertura precisa con instrumentación LLVM

## Índice

1. [Qué es la instrumentación LLVM](#1-qué-es-la-instrumentación-llvm)
2. [Source-based coverage vs ptrace](#2-source-based-coverage-vs-ptrace)
3. [El pipeline manual: RUSTFLAGS → profdata → llvm-cov](#3-el-pipeline-manual-rustflags--profdata--llvm-cov)
4. [Paso 1: compilar con -C instrument-coverage](#4-paso-1-compilar-con--c-instrument-coverage)
5. [Paso 2: ejecutar y generar archivos .profraw](#5-paso-2-ejecutar-y-generar-archivos-profraw)
6. [Paso 3: llvm-profdata merge](#6-paso-3-llvm-profdata-merge)
7. [Paso 4: llvm-cov show — reporte detallado](#7-paso-4-llvm-cov-show--reporte-detallado)
8. [Paso 5: llvm-cov report — resumen](#8-paso-5-llvm-cov-report--resumen)
9. [llvm-cov export — datos en JSON](#9-llvm-cov-export--datos-en-json)
10. [Generar HTML con llvm-cov show](#10-generar-html-con-llvm-cov-show)
11. [Branch coverage: la ventaja de LLVM](#11-branch-coverage-la-ventaja-de-llvm)
12. [Region coverage: el nivel más fino](#12-region-coverage-el-nivel-más-fino)
13. [cargo-llvm-cov: el wrapper ergonómico](#13-cargo-llvm-cov-el-wrapper-ergonómico)
14. [Instalación de cargo-llvm-cov](#14-instalación-de-cargo-llvm-cov)
15. [cargo llvm-cov: uso básico](#15-cargo-llvm-cov-uso-básico)
16. [cargo llvm-cov --html](#16-cargo-llvm-cov---html)
17. [cargo llvm-cov --lcov](#17-cargo-llvm-cov---lcov)
18. [cargo llvm-cov --json](#18-cargo-llvm-cov---json)
19. [Exclusiones y filtros](#19-exclusiones-y-filtros)
20. [cargo llvm-cov en workspaces](#20-cargo-llvm-cov-en-workspaces)
21. [Integración con CI/CD](#21-integración-con-cicd)
22. [--fail-under-lines y --fail-under-branches](#22---fail-under-lines-y---fail-under-branches)
23. [cargo llvm-cov show-env: scripting avanzado](#23-cargo-llvm-cov-show-env-scripting-avanzado)
24. [Comparación tarpaulin vs llvm-cov](#24-comparación-tarpaulin-vs-llvm-cov)
25. [Comparación con C y Go](#25-comparación-con-c-y-go)
26. [Errores comunes](#26-errores-comunes)
27. [Ejemplo completo: pipeline manual + cargo-llvm-cov](#27-ejemplo-completo-pipeline-manual--cargo-llvm-cov)
28. [Programa de práctica](#28-programa-de-práctica)
29. [Ejercicios](#29-ejercicios)

---

## 1. Qué es la instrumentación LLVM

LLVM (Low Level Virtual Machine) es la infraestructura de compilación que usa `rustc`. Cuando compilas Rust, el flujo es:

```
Código Rust → rustc frontend → LLVM IR → LLVM backend → binario
                                  ▲
                                  │
                     Aquí se inserta la
                     instrumentación de cobertura
```

La instrumentación LLVM (source-based coverage) inserta contadores directamente en el LLVM IR (Intermediate Representation) durante la compilación. Cada región de código ejecutable recibe un contador atómico que se incrementa cuando esa región se ejecuta.

```
┌──────────────────────────────────────────────────────────────────┐
│  Sin instrumentación:                                            │
│                                                                  │
│  fn divide(a: f64, b: f64) -> Result<f64, String> {             │
│      if b == 0.0 {                                               │
│          Err("division by zero".into())                          │
│      } else {                                                    │
│          Ok(a / b)                                               │
│      }                                                           │
│  }                                                               │
│                                                                  │
│  Con instrumentación (-C instrument-coverage):                   │
│                                                                  │
│  fn divide(a: f64, b: f64) -> Result<f64, String> {             │
│      __llvm_profile_counter[0] += 1;  // entrada a la función   │
│      if b == 0.0 {                                               │
│          __llvm_profile_counter[1] += 1;  // rama true           │
│          Err("division by zero".into())                          │
│      } else {                                                    │
│          __llvm_profile_counter[2] += 1;  // rama false          │
│          Ok(a / b)                                               │
│      }                                                           │
│  }                                                               │
│                                                                  │
│  Los contadores son atómicos → thread-safe                       │
│  Se escriben a disco al finalizar el programa (.profraw)         │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Ventajas de la instrumentación a nivel LLVM IR

```
1. PRECISIÓN: los contadores están en el IR, antes de optimizaciones
   que eliminan código. Cada región del fuente tiene su contador.

2. VELOCIDAD: los contadores atómicos tienen overhead mínimo (~2-5%)
   comparado con ptrace (~10-50%).

3. BRANCH COVERAGE REAL: LLVM sabe exactamente qué ramas tiene cada
   if/match/while, no las infiere del binario.

4. REGION COVERAGE: puede medir subexpresiones dentro de una línea:
   let x = if cond { a } else { b };
              ▲ región 1   ▲ región 2

5. CROSS-PLATFORM: funciona en Linux, macOS, Windows (donde
   tarpaulin solo funciona en Linux).
```

---

## 2. Source-based coverage vs ptrace

```
┌──────────────────────────────┬──────────────────────────────────┐
│     Ptrace (tarpaulin)       │     Source-based (LLVM)          │
├──────────────────────────────┼──────────────────────────────────┤
│                              │                                  │
│  Programa se ejecuta         │  Programa se COMPILA con         │
│  normalmente.                │  contadores insertados.          │
│                              │                                  │
│  ptrace intercepta cada      │  Los contadores se incrementan   │
│  instrucción o breakpoint    │  automáticamente durante la      │
│  para registrar ejecución.   │  ejecución normal.               │
│                              │                                  │
│  ┌────────┐   ┌─────────┐   │  ┌────────┐   ┌─────────┐      │
│  │Programa│◄──│ ptrace  │   │  │Programa│   │ archivo │      │
│  │ test   │──►│ monitor │   │  │ instru-│──►│.profraw │      │
│  └────────┘   └─────────┘   │  │ mentado│   │         │      │
│       ▲           │          │  └────────┘   └─────────┘      │
│       │     interrumpir      │       │                         │
│       │     en cada línea    │       │ contadores              │
│       └──── overhead alto    │       └─ overhead bajo           │
│                              │                                  │
│  Analogía: un espía que      │  Analogía: un pedómetro que     │
│  te sigue y anota cada       │  llevas puesto — registra       │
│  paso que das                │  pasos automáticamente           │
│                              │                                  │
└──────────────────────────────┴──────────────────────────────────┘
```

---

## 3. El pipeline manual: RUSTFLAGS → profdata → llvm-cov

El pipeline completo sin herramientas wrapper:

```
┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│  1. Compilar │   │ 2. Ejecutar │   │ 3. Merge    │   │ 4. Reportar │
│  con -C      │──►│ tests →     │──►│ profdata    │──►│ llvm-cov    │
│  instrument- │   │ .profraw    │   │ → .profdata │   │ show/report │
│  coverage    │   │             │   │             │   │ /export     │
└─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘

  RUSTFLAGS=        cargo test      llvm-profdata     llvm-cov show
  "-C instrument-                   merge             -instr-profile=
  coverage"                         -sparse           coverage.profdata
                                    *.profraw         binario
                                    -o coverage.       --format=html
                                    profdata
```

### Requisitos previos

```bash
# 1. Instalar componente llvm-tools
rustup component add llvm-tools

# 2. Verificar que las herramientas están disponibles
# Las herramientas están en el sysroot de Rust
$(rustc --print sysroot)/lib/rustlib/x86_64-unknown-linux-gnu/bin/llvm-profdata --help
$(rustc --print sysroot)/lib/rustlib/x86_64-unknown-linux-gnu/bin/llvm-cov --help

# 3. Para facilidad, crear aliases o añadir al PATH
LLVM_TOOLS_DIR="$(rustc --print sysroot)/lib/rustlib/$(rustc -vV | grep host | cut -d' ' -f2)/bin"
export PATH="$LLVM_TOOLS_DIR:$PATH"

# Ahora llvm-profdata y llvm-cov están disponibles directamente
llvm-profdata --version
llvm-cov --version
```

---

## 4. Paso 1: compilar con -C instrument-coverage

```bash
# Limpiar artefactos anteriores (importante para evitar mezclar)
cargo clean

# Compilar tests con instrumentación
RUSTFLAGS="-C instrument-coverage" cargo test --no-run

# --no-run: solo compila, no ejecuta todavía
# Esto permite controlar la ejecución en el paso siguiente
```

### Qué hace -C instrument-coverage

```
rustc recibe la flag y le dice a LLVM:
"Inserta contadores de cobertura en el IR"

LLVM:
1. Analiza el control flow graph (CFG) del programa
2. Identifica todas las regiones de código (bloques básicos)
3. Inserta contadores atómicos mínimos
   (usa spanning tree algorithm para minimizar contadores)
4. Embebe un mapa de regiones en el binario
   (mapping: counter_id → (file, line, col))
5. Genera código para volcar contadores a .profraw al exit
```

### Verificar que la compilación fue exitosa

```bash
# Los binarios de test están en target/debug/deps/
ls target/debug/deps/my_project-*

# El binario tiene instrumentación embebida
# (es más grande que sin instrumentación, ~5-10% más)
```

### Variables de entorno relevantes

```bash
# Controlar dónde se guardan los .profraw
export LLVM_PROFILE_FILE="target/coverage/%p-%m.profraw"

# %p = PID del proceso
# %m = hash del binario
# Esto evita que múltiples test binaries sobreescriban el mismo archivo
```

---

## 5. Paso 2: ejecutar y generar archivos .profraw

```bash
# Definir dónde guardar los perfiles
export LLVM_PROFILE_FILE="target/coverage/%p-%m.profraw"

# Crear directorio
mkdir -p target/coverage

# Ejecutar los tests
RUSTFLAGS="-C instrument-coverage" cargo test

# Verificar que se generaron archivos .profraw
ls target/coverage/
# 12345-abc123.profraw
# 12346-def456.profraw
# (un archivo por cada binary de test ejecutado)
```

### Formato .profraw

```
.profraw es un formato binario de LLVM que contiene:

┌──────────────────────────────────────┐
│ Header                               │
│ - Magic number                       │
│ - Version                            │
│ - Número de contadores               │
├──────────────────────────────────────┤
│ Counter data                         │
│ - counter[0] = 5  (función divide,  │
│                     entrada)         │
│ - counter[1] = 2  (rama true del if)│
│ - counter[2] = 3  (rama false)      │
│ - ...                                │
├──────────────────────────────────────┤
│ Names section                        │
│ - Mapping de counter → función       │
└──────────────────────────────────────┘

No es legible directamente — necesita llvm-profdata merge
```

### Múltiples binarios de test

```
Si tu proyecto tiene:
- src/lib.rs con #[cfg(test)] mod tests
- tests/integration.rs

Se generan DOS binarios de test y DOS .profraw:
  target/coverage/12345-abc123.profraw  (unit tests)
  target/coverage/12346-def456.profraw  (integration tests)

llvm-profdata merge combina ambos en uno solo.
```

---

## 6. Paso 3: llvm-profdata merge

```bash
# Combinar todos los .profraw en un solo .profdata
llvm-profdata merge \
    -sparse \
    target/coverage/*.profraw \
    -o target/coverage/coverage.profdata

# -sparse: usar formato comprimido (más eficiente)
```

### Qué hace llvm-profdata merge

```
Entrada: múltiples .profraw (datos crudos de diferentes ejecuciones)
Salida: un .profdata (datos consolidados)

  ┌──────────────┐     ┌──────────────┐
  │ unit_tests   │     │ integration  │
  │ .profraw     │     │ .profraw     │
  │              │     │              │
  │ divide: 5    │     │ divide: 3    │
  │ add: 10      │     │ add: 0       │
  │ parse: 0     │     │ parse: 7     │
  └──────┬───────┘     └──────┬───────┘
         │                    │
         └────────┬───────────┘
                  │
           llvm-profdata merge
                  │
                  ▼
         ┌──────────────┐
         │ coverage     │
         │ .profdata    │
         │              │
         │ divide: 8    │  ← sumados
         │ add: 10      │
         │ parse: 7     │
         └──────────────┘
```

### Verificar el profdata

```bash
# Ver estadísticas del profdata
llvm-profdata show target/coverage/coverage.profdata

# Debe mostrar funciones instrumentadas y sus contadores
```

---

## 7. Paso 4: llvm-cov show — reporte detallado

```bash
# Encontrar el binario de test
TEST_BIN=$(find target/debug/deps -name 'my_project-*' -not -name '*.d' -type f | head -1)

# Generar reporte en terminal
llvm-cov show \
    "$TEST_BIN" \
    -instr-profile=target/coverage/coverage.profdata \
    -show-line-counts-or-regions \
    -show-instantiations=false \
    --Xdemangler=rustfilt
```

### Output de llvm-cov show

```
    1|      1|pub fn divide(a: f64, b: f64) -> Result<f64, String> {
    2|      1|    if b == 0.0 {
    3|      0|        Err("division by zero".into())
    4|      1|    } else {
    5|      1|        Ok(a / b)
    6|      1|    }
    7|      1|}
    8|       |
    9|      3|pub fn add(a: f64, b: f64) -> f64 {
   10|      3|    a + b
   11|      3|}
   12|       |
   13|      0|pub fn unused_function() {
   14|      0|    println!("never called");
   15|      0|}
```

### Columnas del output

```
Columna 1: número de línea del archivo fuente
Columna 2: hit count (cuántas veces se ejecutó)
           vacío = no es línea ejecutable
           0 = ejecutable pero nunca ejecutada
           N = ejecutada N veces
Columna 3: código fuente
```

### Opciones de show

```bash
# Solo mostrar líneas no cubiertas
llvm-cov show "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    -show-line-counts-or-regions \
    -show-instantiations=false \
    --Xdemangler=rustfilt \
    -region-coverage-lt=1    # solo regiones con 0 hits

# Mostrar archivos específicos
llvm-cov show "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    src/lib.rs src/math.rs   # solo estos archivos

# Con colores (ANSI terminal)
llvm-cov show "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    -use-color
```

### Demangling de nombres de función

```bash
# Los nombres de función de Rust están "mangled":
# _ZN10my_project6divide17h1234567890abcdefE

# --Xdemangler=rustfilt los convierte a nombres legibles:
# my_project::divide

# Instalar rustfilt:
cargo install rustfilt
```

---

## 8. Paso 5: llvm-cov report — resumen

```bash
llvm-cov report \
    "$TEST_BIN" \
    -instr-profile=target/coverage/coverage.profdata \
    --Xdemangler=rustfilt
```

### Output de report

```
Filename                   Regions    Missed Regions     Cover   Functions  Missed Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches     Cover
---                        ---        ---                ---     ---        ---               ---            ---        ---              ---      ---        ---                 ---
src/lib.rs                 12         3                  75.00%  4          1                 75.00%         18         5                72.22%   8          3                   62.50%
src/math.rs                20         2                  90.00%  7          0                 100.00%        25         2                92.00%   10         2                   80.00%
src/parser.rs              15         1                  93.33%  3          0                 100.00%        22         1                95.45%   6          1                   83.33%
---                        ---        ---                ---     ---        ---               ---            ---        ---              ---      ---        ---                 ---
TOTAL                      47         6                  87.23%  14         1                 92.86%         65         8                87.69%   24         6                   75.00%
```

### Columnas del report

```
┌───────────────────┬─────────────────────────────────────────────────┐
│ Columna           │ Significado                                     │
├───────────────────┼─────────────────────────────────────────────────┤
│ Regions           │ Total de regiones de código                     │
│ Missed Regions    │ Regiones nunca ejecutadas                       │
│ Cover (regions)   │ % de regiones cubiertas                         │
│ Functions         │ Total de funciones                              │
│ Missed Functions  │ Funciones nunca llamadas                        │
│ Executed          │ % de funciones ejecutadas                       │
│ Lines             │ Total de líneas ejecutables                     │
│ Missed Lines      │ Líneas nunca ejecutadas                         │
│ Cover (lines)     │ % de líneas cubiertas                           │
│ Branches          │ Total de ramas (if/else/match)                  │
│ Missed Branches   │ Ramas nunca tomadas                             │
│ Cover (branches)  │ % de ramas cubiertas                            │
└───────────────────┴─────────────────────────────────────────────────┘
```

### Filtrar por archivo

```bash
# Report solo de un archivo
llvm-cov report "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    src/math.rs
```

---

## 9. llvm-cov export — datos en JSON

```bash
llvm-cov export \
    "$TEST_BIN" \
    -instr-profile=target/coverage/coverage.profdata \
    --format=text \
    --Xdemangler=rustfilt
```

### Estructura del JSON exportado

```json
{
  "version": "2.0.1",
  "type": "llvm.coverage.json.export",
  "data": [
    {
      "files": [
        {
          "filename": "src/lib.rs",
          "segments": [
            [1, 1, 5, true, true, false],
            [2, 5, 3, true, true, false],
            [3, 9, 0, true, false, false]
          ],
          "branches": [
            [2, 5, 3, 2, 0, 0]
          ],
          "summary": {
            "lines": {
              "count": 18,
              "covered": 15,
              "percent": 83.33
            },
            "functions": {
              "count": 4,
              "covered": 3,
              "percent": 75.0
            },
            "regions": {
              "count": 12,
              "covered": 9,
              "notcovered": 3,
              "percent": 75.0
            },
            "branches": {
              "count": 8,
              "covered": 5,
              "notcovered": 3,
              "percent": 62.5
            }
          }
        }
      ],
      "totals": {
        "lines": { "count": 65, "covered": 57, "percent": 87.69 },
        "functions": { "count": 14, "covered": 13, "percent": 92.86 },
        "regions": { "count": 47, "covered": 41, "percent": 87.23 },
        "branches": { "count": 24, "covered": 18, "percent": 75.0 }
      }
    }
  ]
}
```

### Formato lcov con export

```bash
# Exportar en formato lcov (para Codecov, genhtml, etc.)
llvm-cov export \
    "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    --format=lcov \
    > lcov.info
```

---

## 10. Generar HTML con llvm-cov show

```bash
llvm-cov show \
    "$TEST_BIN" \
    -instr-profile=target/coverage/coverage.profdata \
    -show-line-counts-or-regions \
    -show-instantiations=false \
    --Xdemangler=rustfilt \
    --format=html \
    -output-dir=coverage_html

# Abrir el reporte
xdg-open coverage_html/index.html
```

### Estructura del HTML generado

```
coverage_html/
├── index.html              ← resumen general con tabla de archivos
├── coverage/               ← directorio con archivos fuente anotados
│   ├── src/
│   │   ├── lib.rs.html     ← código fuente con colores de cobertura
│   │   ├── math.rs.html
│   │   └── parser.rs.html
├── style.css               ← estilos
└── functions.html          ← lista de funciones con cobertura
```

### Contenido del HTML

```
El HTML de llvm-cov es más detallado que el de tarpaulin:

┌──────────────────────────────────────────────────────────────┐
│  index.html                                                  │
│                                                              │
│  Coverage Report                                             │
│                                                              │
│  Created: 2024-01-15                                        │
│                                                              │
│  ┌────────────┬────────┬────────┬────────┬────────┐         │
│  │ File       │Line Cov│Func Cov│Region  │Branch  │         │
│  ├────────────┼────────┼────────┼────────┼────────┤         │
│  │ src/lib.rs │ 72.2%  │ 75.0%  │ 75.0%  │ 62.5%  │         │
│  │ src/math.rs│ 92.0%  │ 100%   │ 90.0%  │ 80.0%  │         │
│  │ src/parser │ 95.4%  │ 100%   │ 93.3%  │ 83.3%  │         │
│  ├────────────┼────────┼────────┼────────┼────────┤         │
│  │ TOTAL      │ 87.7%  │ 92.9%  │ 87.2%  │ 75.0%  │         │
│  └────────────┴────────┴────────┴────────┴────────┘         │
│                                                              │
│  4 tipos de cobertura en un solo reporte                     │
│  (tarpaulin solo muestra line coverage)                      │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Branch coverage: la ventaja de LLVM

Branch coverage es la principal ventaja de llvm-cov sobre tarpaulin. Mide si TODAS las ramas de cada decisión se ejecutaron:

```rust
pub fn classify(x: i32, y: i32) -> &'static str {
    if x > 0 && y > 0 {          // branch: true | false
        "both positive"            //   true: x>0 AND y>0
    } else if x > 0 || y > 0 {   //   false: NOT(x>0 AND y>0)
        "one positive"             // branch: true | false
    } else {
        "none positive"
    }
}

// Test: classify(1, 1)
// Ejecuta: if x>0 && y>0 → true → "both positive"
//
// Line coverage: 3 de 5 líneas = 60%
// Branch coverage:
//   if #1: true ✓, false ✗  →  1/2
//   if #2: true ✗, false ✗  →  0/2
//   Total: 1/4 = 25% branch coverage
//
// Para 100% branch coverage necesitas:
//   classify(1, 1)   → if#1 true
//   classify(-1, 1)  → if#1 false, if#2 true
//   classify(-1, -1) → if#1 false, if#2 false
```

### Branch coverage en llvm-cov show

```bash
llvm-cov show "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    -show-branches=count

# Output:
#     1|      3|pub fn classify(x: i32, y: i32) -> &'static str {
#     2|      3|    if x > 0 && y > 0 {
#   ------------------
#   |  Branch (2:8): [True: 1, False: 2]
#   |  Branch (2:17): [True: 1, False: 1]
#   ------------------
#     3|      1|        "both positive"
#     4|      2|    } else if x > 0 || y > 0 {
#   ------------------
#   |  Branch (4:16): [True: 0, False: 2]
#   |  Branch (4:25): [True: 1, False: 1]
#   ------------------
#     5|      1|        "one positive"
#     6|      1|    } else {
#     7|      1|        "none positive"
#     8|      3|    }
#     9|      3|}
```

### Interpretar las ramas

```
Branch (2:8): [True: 1, False: 2]
       │  │         │         │
       │  │         │         └── 2 veces fue false (x <= 0)
       │  │         └── 1 vez fue true (x > 0 && y > 0)
       │  └── columna 8 de la línea 2
       └── línea 2

Esto muestra que:
- La condición completa "x > 0 && y > 0" fue:
  - true 1 vez (ambos positivos)
  - false 2 veces (al menos uno no positivo)

- El subexpresión "y > 0" (solo evaluada si x > 0):
  - true 1 vez
  - false 1 vez

Esto es información que line coverage NO puede dar.
```

### Por qué branch coverage importa

```rust
// Ejemplo donde line coverage es 100% pero branch coverage no:

pub fn process(value: Option<&str>) -> String {
    value.unwrap_or("default").to_uppercase()
}

#[test]
fn test_process() {
    assert_eq!(process(Some("hello")), "HELLO");
    // Line coverage: 100% (la única línea se ejecutó)
    // Branch coverage: 50% (solo probamos Some, no None)
    //
    // El caso None → "DEFAULT" nunca se verificó.
    // Si unwrap_or tuviera un bug con None, no lo detectaríamos.
}
```

```
┌────────────────────────────────────────────────────────────────┐
│  Line coverage 100% pero branch coverage 50%:                  │
│                                                                │
│  ✓ value = Some("hello") → "HELLO"    (rama Some)             │
│  ✗ value = None           → "DEFAULT"  (rama None)             │
│                                                                │
│  Branch coverage revela que hay un path no probado.            │
│  Line coverage no lo detecta porque toda la línea se ejecutó.  │
└────────────────────────────────────────────────────────────────┘
```

---

## 12. Region coverage: el nivel más fino

Region coverage mide cada subexpresión o bloque independiente:

```rust
pub fn complex_condition(a: bool, b: bool, c: bool) -> i32 {
    if a && (b || c) {
    //  ^     ^    ^
    //  |     |    └─ región 3: evaluación de c
    //  |     └────── región 2: evaluación de b
    //  └──────────── región 1: evaluación de a
        42
    } else {
        0
    }
}
```

```bash
llvm-cov show "$TEST_BIN" \
    -instr-profile=coverage.profdata \
    -show-regions

# Output con regiones:
#     1|     5|pub fn complex_condition(a: bool, b: bool, c: bool) -> i32 {
#     2|     5|    if a && (b || c) {
#                     ^3    ^2   ^1
#     3|     3|        42
#     4|     2|    } else {
#     5|     2|        0
#     6|     5|    }
#     7|     5|}
#
# Regiones marcadas con contadores:
# ^3 = la evaluación de 'a' se hizo 5 veces
# ^2 = la evaluación de 'b' se hizo 3 veces (solo cuando a=true)
# ^1 = la evaluación de 'c' se hizo 1 vez (solo cuando a=true y b=false)
```

### Region vs Line vs Branch

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  let result = if x > 0 { x * 2 } else { -x };                  │
│               ▲          ▲              ▲                        │
│               │          │              │                        │
│  Line coverage: 1 línea → hit o no hit (binario)                │
│                                                                  │
│  Branch coverage: 2 ramas → true branch / false branch          │
│                                                                  │
│  Region coverage: 3 regiones:                                    │
│    - la condición "x > 0"                                        │
│    - el bloque "x * 2"                                           │
│    - el bloque "-x"                                              │
│    Cada una con su propio hit count                              │
│                                                                  │
│  Region > Branch > Line (en granularidad)                        │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 13. cargo-llvm-cov: el wrapper ergonómico

El pipeline manual (RUSTFLAGS → profdata merge → llvm-cov show) es educativo pero tedioso para el uso diario. `cargo-llvm-cov` automatiza todo:

```
Pipeline manual:                     cargo-llvm-cov:

  RUSTFLAGS="-C instrument-          cargo llvm-cov
    coverage" cargo test
  llvm-profdata merge ...             (un solo comando)
  llvm-cov show ...
  llvm-cov report ...

  6+ comandos                         1 comando
  Variables de entorno manuales       Automático
  Encontrar binarios manualmente      Automático
  Limpiar .profraw manualmente        Automático
```

---

## 14. Instalación de cargo-llvm-cov

```bash
# Instalar el crate
cargo install cargo-llvm-cov

# Esto también instala llvm-tools automáticamente:
rustup component add llvm-tools-preview

# Verificar
cargo llvm-cov --version
# cargo-llvm-cov 0.6.x
```

### Con cargo-binstall (más rápido)

```bash
cargo binstall cargo-llvm-cov
```

### Requisitos

```
- Rust 1.60+ (stable)
- llvm-tools-preview component (instalado automáticamente)
- No requiere nightly
- Funciona en Linux, macOS, Windows
```

---

## 15. cargo llvm-cov: uso básico

```bash
# Ejecutar tests y mostrar cobertura en terminal
cargo llvm-cov
```

### Output típico

```
   Compiling my_project v0.1.0 (/home/user/my_project)
    Finished `test` profile [unoptimized + debuginfo] target(s) in 0.50s
     Running unittests src/lib.rs (target/llvm-cov-target/debug/deps/my_project-abc123)

running 15 tests
test math::tests::test_add ... ok
test math::tests::test_divide ... ok
...
test result: ok. 15 passed; 0 failed; 0 ignored

Filename                      Regions    Missed Regions     Cover   Functions  Missed Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches     Cover
---                           ---        ---                ---     ---        ---               ---            ---        ---              ---      ---        ---                 ---
src/lib.rs                    12         2                  83.33%  4          0                 100.00%        18         3                83.33%   8          2                   75.00%
src/math.rs                   20         1                  95.00%  7          0                 100.00%        25         1                96.00%   10         1                   90.00%
src/parser.rs                 15         0                  100.0%  3          0                 100.00%        22         0                100.0%   6          0                   100.0%
---                           ---        ---                ---     ---        ---               ---            ---        ---              ---      ---        ---                 ---
TOTAL                         47         3                  93.62%  14         0                 100.00%        65         4                93.85%   24         3                   87.50%
```

### Diferencia con tarpaulin

```
cargo tarpaulin output:
  || src/lib.rs: 15/18 (83.33%)
  → Solo line coverage

cargo llvm-cov output:
  src/lib.rs  83.33% regions  100% functions  83.33% lines  75% branches
  → 4 tipos de cobertura simultáneamente
```

---

## 16. cargo llvm-cov --html

```bash
# Generar reporte HTML
cargo llvm-cov --html

# Se abre automáticamente el navegador (si disponible)
# El reporte está en target/llvm-cov/html/index.html

# Abrir manualmente
cargo llvm-cov --html --open

# Directorio personalizado
cargo llvm-cov --html --output-dir coverage/
```

### Estructura del HTML

```
target/llvm-cov/html/
├── index.html           ← resumen con tabla (4 tipos de cobertura)
├── style.css
├── coverage/
│   └── src/
│       ├── lib.rs.html  ← código anotado con colores + contadores
│       ├── math.rs.html
│       └── parser.rs.html
└── functions.html       ← lista de todas las funciones
```

### Contenido del archivo fuente anotado

```
El HTML muestra cada línea con:
- Número de línea
- Hit count (cuántas veces se ejecutó)
- Color de fondo:
  - Verde: cubierta (hit > 0)
  - Rojo: no cubierta (hit = 0)
  - Blanco: no ejecutable
- Ramas expandibles mostrando true/false counts
```

---

## 17. cargo llvm-cov --lcov

```bash
# Generar lcov.info para herramientas externas
cargo llvm-cov --lcov --output-path lcov.info

# Usar con genhtml
genhtml lcov.info -o coverage_html/

# Usar con VS Code Coverage Gutters
# (automáticamente lee lcov.info en el directorio raíz)
```

### Uso con Codecov

```bash
# Generar lcov y subir a Codecov
cargo llvm-cov --lcov --output-path lcov.info
bash <(curl -s https://codecov.io/bash) -f lcov.info
```

---

## 18. cargo llvm-cov --json

```bash
# Exportar datos en JSON de LLVM
cargo llvm-cov --json --output-path coverage.json

# Procesar con jq
cat coverage.json | jq '.data[0].totals'
# {
#   "lines": { "count": 65, "covered": 61, "percent": 93.85 },
#   "functions": { "count": 14, "covered": 14, "percent": 100 },
#   "regions": { "count": 47, "covered": 44, "percent": 93.62 },
#   "branches": { "count": 24, "covered": 21, "percent": 87.5 }
# }

# Archivos con baja cobertura de branches
cat coverage.json | jq '
  .data[0].files[]
  | select(.summary.branches.percent < 80)
  | {file: .filename, branch_cov: .summary.branches.percent}
'
```

---

## 19. Exclusiones y filtros

### Excluir archivos

```bash
# Excluir por pattern
cargo llvm-cov --ignore-filename-regex "tests/.*"
cargo llvm-cov --ignore-filename-regex "examples/.*|benches/.*"

# Incluir solo ciertos archivos
cargo llvm-cov --ignore-filename-regex "^(?!src/(lib|math|parser)\.rs)"
```

### Excluir funciones en el código

```rust
// No hay un #[llvm_cov::skip] nativo.
// Usar la compilación condicional:

#[cfg(not(coverage_nightly))]  // requiere nightly + -Z coverage-options
fn main() { /* ... */ }

// O la forma pragmática: simplemente ignorar la cobertura de esas
// funciones en el análisis del reporte.
```

### Excluir en la línea de comandos

```bash
# Solo medir el crate actual (no dependencias)
cargo llvm-cov  # por defecto solo mide tu código

# Filtrar en report
cargo llvm-cov --ignore-filename-regex "target/.*"
```

---

## 20. cargo llvm-cov en workspaces

```bash
# Todo el workspace
cargo llvm-cov --workspace

# Paquetes específicos
cargo llvm-cov --package my_core --package my_api

# Excluir paquetes
cargo llvm-cov --workspace --exclude my_cli

# Incluir todos los targets de test
cargo llvm-cov --workspace --doctests
```

### Configuración por workspace

```bash
# Ver cobertura combinada de unit + integration tests
cargo llvm-cov --workspace --html

# Solo integration tests
cargo llvm-cov --workspace --test integration
```

---

## 21. Integración con CI/CD

### GitHub Actions

```yaml
name: Coverage

on: [push, pull_request]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable
        with:
          components: llvm-tools-preview

      - name: Install cargo-llvm-cov
        uses: taiki-e/install-action@cargo-llvm-cov

      - name: Generate coverage
        run: cargo llvm-cov --lcov --output-path lcov.info

      - name: Upload to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: lcov.info

      - name: Check coverage threshold
        run: cargo llvm-cov --fail-under-lines 75
```

### GitLab CI

```yaml
coverage:
  image: rust:latest
  before_script:
    - rustup component add llvm-tools-preview
    - cargo install cargo-llvm-cov
  script:
    - cargo llvm-cov --cobertura --output-path cobertura.xml
    - cargo llvm-cov --fail-under-lines 70
  coverage: '/TOTAL.*?(\d+\.\d+%)/'
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: cobertura.xml
```

### Optimización: usar taiki-e/install-action

```yaml
# En GitHub Actions, esta action instala binarios precompilados
# en lugar de compilar desde fuente (~10 segundos vs ~3 minutos)

- uses: taiki-e/install-action@cargo-llvm-cov
```

---

## 22. --fail-under-lines y --fail-under-branches

```bash
# Fallar si line coverage < 75%
cargo llvm-cov --fail-under-lines 75

# Fallar si branch coverage < 60%
cargo llvm-cov --fail-under-branches 60

# Fallar si function coverage < 90%
cargo llvm-cov --fail-under-functions 90

# Fallar si region coverage < 80%
cargo llvm-cov --fail-under-regions 80

# Combinar múltiples umbrales
cargo llvm-cov \
    --fail-under-lines 75 \
    --fail-under-branches 60

# Si cualquier umbral no se cumple → exit code 1
```

### Tabla de umbrales recomendados

```
┌──────────────────────┬─────────────┬─────────────┬──────────────┐
│ Tipo de cobertura    │ Mínimo      │ Recomendado │ Aspiracional │
├──────────────────────┼─────────────┼─────────────┼──────────────┤
│ Line coverage        │ 60%         │ 75-85%      │ 90%+         │
│ Branch coverage      │ 40%         │ 60-75%      │ 80%+         │
│ Function coverage    │ 70%         │ 85-95%      │ 100%         │
│ Region coverage      │ 50%         │ 70-80%      │ 85%+         │
└──────────────────────┴─────────────┴─────────────┴──────────────┘

Branch coverage siempre es menor que line coverage
porque es más granular (2+ paths por decisión).
```

---

## 23. cargo llvm-cov show-env: scripting avanzado

`show-env` muestra las variables de entorno que cargo-llvm-cov configuraría, sin ejecutar nada. Útil para integrar con scripts personalizados:

```bash
# Ver las variables que se configurarían
cargo llvm-cov show-env

# Output:
# CARGO_INCREMENTAL=0
# RUSTFLAGS=-C instrument-coverage --cfg coverage_nightly
# LLVM_PROFILE_FILE=/home/user/project/target/llvm-cov-target/.../%p-%m.profraw
# CARGO_TARGET_DIR=/home/user/project/target/llvm-cov-target

# Usarlo en un script
eval $(cargo llvm-cov show-env --export-prefix)
cargo test
cargo llvm-cov report --lcov --output-path lcov.info
```

### Caso de uso: ejecutar tests custom + cobertura

```bash
#!/bin/bash
# script: coverage-with-custom-tests.sh

# Configurar el entorno
eval $(cargo llvm-cov show-env --export-prefix)

# Limpiar
cargo llvm-cov clean --workspace

# Compilar
cargo build --tests

# Ejecutar tests con lógica custom
cargo test --lib    # unit tests
cargo test --test integration_auth  # solo auth integration tests

# Generar reporte (usa los .profraw generados arriba)
cargo llvm-cov report --lcov --output-path lcov.info
cargo llvm-cov report --html --output-dir coverage/

echo "Coverage report generated in coverage/"
```

### Caso de uso: cobertura de integration tests separada

```bash
# Solo cobertura de unit tests
cargo llvm-cov --lib --lcov --output-path unit-lcov.info

# Solo cobertura de integration tests
cargo llvm-cov --test integration --lcov --output-path integration-lcov.info

# Combinar manualmente con lcov
# (si tienes lcov instalado)
lcov -a unit-lcov.info -a integration-lcov.info -o combined-lcov.info
```

---

## 24. Comparación tarpaulin vs llvm-cov

```
┌──────────────────────────┬─────────────────────────┬─────────────────────────┐
│ Aspecto                  │ cargo-tarpaulin         │ cargo-llvm-cov          │
├──────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Plataforma               │ Linux only              │ Linux, macOS, Windows   │
│ Engine                   │ Ptrace o LLVM           │ Solo LLVM               │
│ Precisión                │ Buena (ptrace)          │ Excelente (source-based)│
│                          │ Excelente (llvm engine) │                         │
│ Line coverage            │ ✓                       │ ✓                       │
│ Branch coverage          │ Experimental            │ ✓ (real, LLVM IR-based) │
│ Region coverage          │ ✗                       │ ✓                       │
│ Function coverage        │ Parcial                 │ ✓                       │
│ Velocidad                │ Más lento (ptrace)      │ Más rápido              │
│ Formatos output          │ HTML, JSON, Lcov,       │ HTML, JSON, Lcov,       │
│                          │ XML (Cobertura)         │ XML (Cobertura), text   │
│ Exclusiones              │ #[tarpaulin::skip]      │ --ignore-filename-regex │
│                          │ --exclude-files         │                         │
│ Configuración archivo    │ .tarpaulin.toml         │ Flags en CLI            │
│ Umbral                   │ --fail-under            │ --fail-under-lines/     │
│                          │ (solo lines)            │ branches/functions/     │
│                          │                         │ regions                 │
│ Coveralls directo        │ --coveralls TOKEN       │ No (vía lcov)           │
│ Doc tests                │ --run-types doctests    │ --doctests              │
│ Descargas (crates.io)    │ ~60M                    │ ~15M                    │
│ Madurez                  │ Alta (2018+)            │ Alta (2021+)            │
│ Dependencia de nightly   │ No                      │ No                      │
│ show-env para scripting  │ No                      │ ✓                       │
├──────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Usar cuando              │ Linux, simplicidad,     │ Cross-platform, branch  │
│                          │ config en archivo,      │ coverage real, precisión│
│                          │ Coveralls directo       │ máxima, scripting       │
└──────────────────────────┴─────────────────────────┴─────────────────────────┘
```

### Diagrama de decisión

```
¿Qué herramienta de cobertura?
            │
            ▼
    ┌───────────────┐
    │ ¿Solo Linux?  │
    └───┬───────┬───┘
       SÍ       NO
        │        │
        │        └──► cargo-llvm-cov (única opción cross-platform)
        │
        ▼
    ┌───────────────┐
    │ ¿Necesitas    │
    │ branch        │
    │ coverage      │
    │ real?         │
    └───┬───────┬───┘
       SÍ       NO
        │        │
        ▼        ▼
  cargo-llvm-cov  ┌────────────────┐
                  │ ¿Config en     │
                  │ archivo?       │
                  └───┬────────┬───┘
                     SÍ        NO
                      │        │
                      ▼        ▼
                 tarpaulin   Cualquiera
                 (.toml)     (ambas ok)
```

---

## 25. Comparación con C y Go

### C: instrumentación con GCC/Clang

```bash
# === GCC: -fprofile-arcs -ftest-coverage ===

# Compilar con instrumentación
gcc -fprofile-arcs -ftest-coverage -o test_program test.c lib.c

# Ejecutar
./test_program

# Genera: lib.gcda, lib.gcno

# Reporte
gcov lib.c
# Genera: lib.c.gcov

# Branch coverage con gcov
gcov -b lib.c
# Muestra branches taken/not taken

# === Clang: -fprofile-instr-generate -fcoverage-mapping ===
# (equivalente directo a lo que hace Rust con LLVM)

clang -fprofile-instr-generate -fcoverage-mapping -o test_program test.c lib.c
LLVM_PROFILE_FILE="test.profraw" ./test_program
llvm-profdata merge test.profraw -o test.profdata
llvm-cov show ./test_program -instr-profile=test.profdata
# ← Exactamente el mismo pipeline que Rust usa
# Porque Rust usa el mismo backend LLVM
```

```c
/* Clang source-based coverage output (igual que Rust) */
/*     1|      5|int factorial(int n) {          */
/*     2|      5|    if (n <= 1) {               */
/*   ------------------                          */
/*   |  Branch (2:9): [True: 2, False: 3]       */
/*   ------------------                          */
/*     3|      2|        return 1;               */
/*     4|      3|    }                           */
/*     5|      3|    return n * factorial(n - 1); */
/*     6|      5|}                               */

/* Es EXACTAMENTE el mismo formato que llvm-cov show en Rust
   porque ambos usan el mismo backend LLVM */
```

### Go: go test -cover (built-in)

```bash
# Go tiene cobertura built-in — no necesita herramientas externas

# Coverage básica
go test -cover ./...
# ok  mypackage  0.005s  coverage: 82.3% of statements

# Profile detallado
go test -coverprofile=coverage.out ./...

# Visualizar por función
go tool cover -func=coverage.out
# mypackage/auth.go:10:    Login       100.0%
# mypackage/auth.go:25:    Logout       75.0%
# total:                   (statements)  82.3%

# HTML
go tool cover -html=coverage.out -o coverage.html

# Cover modes (equivalentes a line/branch)
go test -covermode=set ./...     # binario: cubierta o no (line coverage)
go test -covermode=count ./...   # hit count (como llvm-cov)
go test -covermode=atomic ./...  # thread-safe hit count

# Go 1.20+: cobertura de aplicaciones (no solo tests)
# GOCOVERDIR=/tmp/cover go build -cover -o myapp
# ./myapp
# go tool covdata textfmt -i=/tmp/cover -o coverage.out
```

### Tabla comparativa trilingüe

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Rust (llvm-cov)      │ Go                   │ C (gcc/clang)        │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Built-in             │ No (crate +          │ SÍ                   │ No (flags + tools)   │
│                      │ llvm-tools)          │                      │                      │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Backend              │ LLVM source-based    │ Compiler-inserted    │ GCC: gcov            │
│                      │ instrumentation      │ counters             │ Clang: LLVM (mismo)  │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Comando mínimo       │ cargo llvm-cov       │ go test -cover       │ gcc -fprofile-arcs + │
│                      │                      │                      │ gcov                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Line coverage        │ ✓                    │ ✓ (-covermode=set)   │ ✓                    │
│ Branch coverage      │ ✓ (excelente)        │ ✗ (no nativo)        │ ✓ (gcov -b)          │
│ Region coverage      │ ✓                    │ ✗                    │ ✓ (clang only)       │
│ Function coverage    │ ✓                    │ ✓ (go tool cover     │ ✓ (gcov -f)          │
│                      │                      │  -func)              │                      │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ HTML report          │ --html               │ go tool cover -html  │ genhtml (lcov)       │
│ JSON export          │ --json               │ No nativo            │ gcovr --json         │
│ Lcov format          │ --lcov               │ Converters externos  │ lcov nativo          │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Umbral en CLI        │ --fail-under-lines   │ Script manual        │ lcov --fail-under    │
│                      │ --fail-under-branches│                      │                      │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Archivos intermedios │ .profraw → .profdata │ coverage.out         │ .gcno + .gcda        │
│                      │ (binarios LLVM)      │ (texto plano)        │ (binarios GCC)       │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Overhead runtime     │ ~2-5%                │ ~3-5%                │ GCC: ~10-15%         │
│                      │                      │                      │ Clang: ~2-5%         │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Cross-platform       │ ✓ (Linux, macOS, Win)│ ✓ (todas)            │ Depende del          │
│                      │                      │                      │ compilador           │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

### Conexión Rust-C vía LLVM

```
Hecho clave: Rust y Clang usan el MISMO backend LLVM.

Por eso:
- El formato .profraw es idéntico
- llvm-profdata y llvm-cov son las MISMAS herramientas
- El pipeline es idéntico:
  1. Compilar con instrumentación
  2. Ejecutar → .profraw
  3. llvm-profdata merge → .profdata
  4. llvm-cov show/report/export

La única diferencia es el frontend:
- Rust: rustc con -C instrument-coverage
- C: clang con -fprofile-instr-generate -fcoverage-mapping

Si trabajas con FFI (Rust + C), puedes combinar cobertura
de ambos lenguajes en un solo reporte porque usan el mismo
formato de instrumentación.
```

---

## 26. Errores comunes

### Error 1: "Failed to load coverage: No coverage data found"

```
$ cargo llvm-cov
error: No coverage data found

Causa: los tests no generaron archivos .profraw.

Soluciones:
1. Verificar que hay tests que pasen:
   cargo test

2. Limpiar y recompilar:
   cargo llvm-cov clean --workspace
   cargo llvm-cov

3. Verificar que llvm-tools está instalado:
   rustup component add llvm-tools-preview
```

### Error 2: "instrument-coverage is not supported"

```
Causa: versión de Rust muy antigua.

Solución:
  rustup update stable
  # -C instrument-coverage requiere Rust 1.60+
```

### Error 3: Resultados diferentes entre tarpaulin y llvm-cov

```
Causa: usan engines diferentes con definiciones distintas
de "línea cubrirle". LLVM es más preciso.

Ejemplo:
  match x {
      1 => "one",       // tarpaulin: 1 línea
      2 => "two",       // llvm-cov: 1 línea + 1 rama
      _ => "other",     // diferencia en conteo
  }

Solución: elegir UNA herramienta como fuente de verdad.
No mezclar métricas de diferentes herramientas.
```

### Error 4: Cobertura de 0% en funciones que usan proc macros

```
Causa: algunas proc macros generan código que confunde al
mapeo de cobertura source-based.

Solución:
  # Excluir archivos problemáticos
  cargo llvm-cov --ignore-filename-regex "generated_.*\.rs"

  # O usar --ignore-run-fail si los tests fallan por esto
```

### Error 5: "No such file or directory: llvm-profdata"

```
Causa: llvm-tools-preview no instalado o no en PATH.

Solución:
  rustup component add llvm-tools-preview

  # cargo-llvm-cov encuentra las tools automáticamente.
  # Si usas el pipeline manual, añadir al PATH:
  export PATH="$(rustc --print sysroot)/lib/rustlib/$(rustc -vV | grep host | cut -d' ' -f2)/bin:$PATH"
```

### Error 6: El HTML muestra archivos de dependencias

```
Causa: llvm-cov instrumenta todo lo compilado.

Solución:
  # cargo-llvm-cov ya filtra dependencias por defecto.
  # Si usas el pipeline manual, filtrar:
  llvm-cov show "$BIN" \
      -instr-profile=coverage.profdata \
      --ignore-filename-regex="\.cargo/registry|/rustc/"
```

### Error 7: Cobertura incorrecta con #[inline]

```
Causa: funciones inline pueden tener contadores duplicados
o incorrectos en ciertas versiones de LLVM.

Solución:
  # Usar --show-instantiations=false para agrupar
  llvm-cov show "$BIN" \
      -instr-profile=coverage.profdata \
      -show-instantiations=false

  # cargo-llvm-cov hace esto por defecto
```

---

## 27. Ejemplo completo: pipeline manual + cargo-llvm-cov

### El proyecto

```rust
// src/lib.rs
pub mod validator;
pub mod transformer;

// src/validator.rs
#[derive(Debug, PartialEq)]
pub enum ValidationError {
    Empty,
    TooLong(usize),
    InvalidChars(char),
    InvalidFormat(String),
}

pub fn validate_username(username: &str) -> Result<(), Vec<ValidationError>> {
    let mut errors = Vec::new();

    if username.is_empty() {
        errors.push(ValidationError::Empty);
        return Err(errors);
    }

    if username.len() > 32 {
        errors.push(ValidationError::TooLong(username.len()));
    }

    for c in username.chars() {
        if !c.is_alphanumeric() && c != '_' && c != '-' {
            errors.push(ValidationError::InvalidChars(c));
        }
    }

    if username.starts_with('-') || username.starts_with('_') {
        errors.push(ValidationError::InvalidFormat(
            "cannot start with - or _".into()
        ));
    }

    if username.ends_with('-') || username.ends_with('_') {
        errors.push(ValidationError::InvalidFormat(
            "cannot end with - or _".into()
        ));
    }

    if errors.is_empty() { Ok(()) } else { Err(errors) }
}

pub fn validate_email(email: &str) -> Result<(), ValidationError> {
    if email.is_empty() {
        return Err(ValidationError::Empty);
    }

    let parts: Vec<&str> = email.split('@').collect();
    if parts.len() != 2 {
        return Err(ValidationError::InvalidFormat("missing @".into()));
    }

    let (local, domain) = (parts[0], parts[1]);

    if local.is_empty() {
        return Err(ValidationError::InvalidFormat("empty local part".into()));
    }

    if !domain.contains('.') {
        return Err(ValidationError::InvalidFormat("domain must have dot".into()));
    }

    Ok(())
}

// src/transformer.rs
pub fn to_slug(input: &str) -> String {
    input.chars()
        .map(|c| {
            if c.is_alphanumeric() {
                c.to_lowercase().next().unwrap()
            } else {
                '-'
            }
        })
        .collect::<String>()
        .split('-')
        .filter(|s| !s.is_empty())
        .collect::<Vec<&str>>()
        .join("-")
}

pub fn truncate(input: &str, max_len: usize) -> String {
    if input.len() <= max_len {
        input.to_string()
    } else if max_len <= 3 {
        input.chars().take(max_len).collect()
    } else {
        let truncated: String = input.chars().take(max_len - 3).collect();
        format!("{}...", truncated)
    }
}

pub fn capitalize_words(input: &str) -> String {
    input.split_whitespace()
        .map(|word| {
            let mut chars = word.chars();
            match chars.next() {
                None => String::new(),
                Some(first) => {
                    let upper: String = first.to_uppercase().collect();
                    format!("{}{}", upper, chars.as_str())
                }
            }
        })
        .collect::<Vec<String>>()
        .join(" ")
}

#[cfg(test)]
mod tests {
    use super::*;

    // validator tests
    #[test]
    fn valid_username() {
        assert!(validator::validate_username("alice").is_ok());
        assert!(validator::validate_username("bob_123").is_ok());
        assert!(validator::validate_username("user-name").is_ok());
    }

    #[test]
    fn empty_username() {
        let errs = validator::validate_username("").unwrap_err();
        assert!(errs.contains(&validator::ValidationError::Empty));
    }

    #[test]
    fn long_username() {
        let long = "a".repeat(50);
        let errs = validator::validate_username(&long).unwrap_err();
        assert!(matches!(errs[0], validator::ValidationError::TooLong(50)));
    }

    #[test]
    fn invalid_chars_username() {
        let errs = validator::validate_username("user@name").unwrap_err();
        assert!(matches!(errs[0], validator::ValidationError::InvalidChars('@')));
    }

    #[test]
    fn username_starts_with_special() {
        let errs = validator::validate_username("-alice").unwrap_err();
        assert!(matches!(errs[0], validator::ValidationError::InvalidFormat(_)));
    }

    #[test]
    fn valid_email() {
        assert!(validator::validate_email("alice@example.com").is_ok());
    }

    #[test]
    fn empty_email() {
        assert_eq!(
            validator::validate_email(""),
            Err(validator::ValidationError::Empty)
        );
    }

    #[test]
    fn email_without_at() {
        assert!(validator::validate_email("alice.example.com").is_err());
    }

    #[test]
    fn email_without_domain_dot() {
        assert!(validator::validate_email("alice@localhost").is_err());
    }

    // transformer tests
    #[test]
    fn test_to_slug() {
        assert_eq!(to_slug("Hello World!"), "hello-world");
        assert_eq!(to_slug("  Multiple   Spaces  "), "multiple-spaces");
        assert_eq!(to_slug("Already-a-Slug"), "already-a-slug");
    }

    #[test]
    fn test_truncate_short() {
        assert_eq!(truncate("hello", 10), "hello");
    }

    #[test]
    fn test_truncate_long() {
        assert_eq!(truncate("hello world", 8), "hello...");
    }

    #[test]
    fn test_truncate_tiny() {
        assert_eq!(truncate("hello", 2), "he");
    }

    #[test]
    fn test_capitalize() {
        assert_eq!(capitalize_words("hello world"), "Hello World");
        assert_eq!(capitalize_words("a b c"), "A B C");
    }
}
```

### Pipeline manual paso a paso

```bash
# 0. Prerequisitos
rustup component add llvm-tools-preview
cargo install rustfilt

# Obtener el path de las herramientas LLVM
LLVM_BIN="$(rustc --print sysroot)/lib/rustlib/$(rustc -vV | grep host | cut -d' ' -f2)/bin"

# 1. Limpiar
cargo clean

# 2. Compilar con instrumentación
RUSTFLAGS="-C instrument-coverage" \
LLVM_PROFILE_FILE="target/coverage/%p-%m.profraw" \
    cargo test

# 3. Merge profraw
"$LLVM_BIN/llvm-profdata" merge \
    -sparse \
    target/coverage/*.profraw \
    -o target/coverage/merged.profdata

# 4. Encontrar el binario de test
TEST_BIN=$(find target/debug/deps -name 'my_project-*' -not -name '*.d' -type f -executable | head -1)

# 5. Reporte en terminal
"$LLVM_BIN/llvm-cov" report \
    "$TEST_BIN" \
    -instr-profile=target/coverage/merged.profdata \
    --Xdemangler=rustfilt

# 6. Código anotado
"$LLVM_BIN/llvm-cov" show \
    "$TEST_BIN" \
    -instr-profile=target/coverage/merged.profdata \
    -show-line-counts-or-regions \
    -show-branches=count \
    --Xdemangler=rustfilt

# 7. HTML
"$LLVM_BIN/llvm-cov" show \
    "$TEST_BIN" \
    -instr-profile=target/coverage/merged.profdata \
    -show-line-counts-or-regions \
    -show-branches=count \
    --Xdemangler=rustfilt \
    --format=html \
    -output-dir=coverage_manual/
```

### Mismo resultado con cargo-llvm-cov (1 comando)

```bash
# Todo lo anterior en un solo comando:
cargo llvm-cov --html --open

# Con branch coverage visible:
cargo llvm-cov --html --branch

# Con umbrales:
cargo llvm-cov --fail-under-lines 80 --fail-under-branches 60

# Con lcov para CI:
cargo llvm-cov --lcov --output-path lcov.info
```

---

## 28. Programa de práctica

### Proyecto: comparar tarpaulin vs llvm-cov

Usa el proyecto `calculator` de T01 (o uno similar) y ejecuta ambas herramientas para comparar los resultados.

### Tareas

1. **Ejecutar ambas herramientas** contra el mismo proyecto:

```bash
# tarpaulin
cargo tarpaulin --out html --output-dir coverage_tarpaulin/

# llvm-cov
cargo llvm-cov --html --output-dir coverage_llvm/
```

2. **Comparar los resultados**:
   - ¿Cuántas líneas reporta cada herramienta como "cubribles"?
   - ¿Difiere el porcentaje de cobertura? ¿Por cuánto?
   - ¿Qué líneas difieren entre ambas herramientas?

3. **Analizar branch coverage** (solo llvm-cov):
   - Ejecutar `cargo llvm-cov --html --branch`
   - Identificar las ramas no cubiertas
   - Escribir tests adicionales para cubrirlas
   - ¿Cuánto subió el branch coverage?

4. **Pipeline manual**:
   - Ejecutar los 7 pasos del pipeline manual (sección 27)
   - Verificar que el resultado es idéntico al de `cargo llvm-cov`
   - Documentar cualquier diferencia encontrada

5. **CI workflow**:
   - Escribir un script `coverage.sh` que ejecute `cargo llvm-cov` con:
     - Umbral de 80% lines y 60% branches
     - Output lcov para upload
     - Output HTML para artefactos
   - Verificar que falla cuando la cobertura es insuficiente

---

## 29. Ejercicios

### Ejercicio 1: Pipeline manual completo

**Objetivo**: Entender cada paso del pipeline sin wrapper.

**Tareas**:

**a)** Crea un proyecto con 3 funciones y tests que cubran ~70% de las líneas.

**b)** Ejecuta el pipeline manual completo (pasos 1-7 de la sección 3-10).

**c)** Genera reporte en terminal, HTML y lcov.

**d)** Identifica qué función tiene la cobertura más baja. Escribe tests adicionales.

**e)** Re-ejecuta el pipeline y verifica que la cobertura subió.

---

### Ejercicio 2: Branch coverage

**Objetivo**: Practicar el análisis de branch coverage.

**Contexto**:

```rust
pub fn grade(score: u32) -> &'static str {
    match score {
        90..=100 => "A",
        80..=89 => "B",
        70..=79 => "C",
        60..=69 => "D",
        0..=59 => "F",
        _ => "Invalid",
    }
}
```

**Tareas**:

**a)** Escribe UN test: `assert_eq!(grade(95), "A")`.

**b)** Ejecuta `cargo llvm-cov --html --branch` y analiza el reporte. ¿Cuántas ramas hay? ¿Cuántas están cubiertas?

**c)** Escribe el número mínimo de tests para alcanzar 100% de branch coverage. ¿Cuántos necesitas?

**d)** ¿Existe algún input que cause un bug que estos tests no detectan? (Pista: score > 100 pero no u32::MAX)

---

### Ejercicio 3: Comparar precisión tarpaulin vs llvm-cov

**Objetivo**: Entender las diferencias prácticas entre ambas herramientas.

**Contexto**:

```rust
pub fn process(items: &[i32]) -> Vec<i32> {
    items.iter()
        .filter(|&&x| x > 0)
        .map(|&x| if x > 100 { 100 } else { x })
        .collect()
}
```

**Tareas**:

**a)** Escribe tests que cubran la función con inputs `[1, 2, 3]` y `[-1, 0, 5]`.

**b)** Ejecuta tarpaulin y llvm-cov. Anota los resultados de cada uno:
   - Line coverage (%)
   - Líneas marcadas como no cubiertas

**c)** ¿Detecta alguna herramienta que la rama `x > 100 → 100` no está cubierta? ¿Cuál?

**d)** Añade un test con `[150]` y re-ejecuta ambas. ¿Cambian ambos resultados?

---

### Ejercicio 4: CI con umbrales múltiples

**Objetivo**: Configurar CI con umbrales de cobertura por tipo.

**Tareas**:

**a)** Escribe un workflow de GitHub Actions que:
   - Ejecute `cargo llvm-cov` con umbrales:
     - Line coverage >= 75%
     - Branch coverage >= 50%
     - Function coverage >= 90%
   - Genere lcov.info como artefacto
   - Suba cobertura a Codecov

**b)** Simula qué pasa cuando un PR baja el branch coverage de 60% a 45%:
   - ¿Qué flag de `cargo llvm-cov` causa el fallo?
   - ¿Cuál es el exit code?
   - ¿El mensaje de error es claro?

**c)** Compara la experiencia CI de `cargo tarpaulin --fail-under 75` vs `cargo llvm-cov --fail-under-lines 75 --fail-under-branches 50`. ¿Cuál da más información al desarrollador que necesita arreglar la cobertura?

---

## Navegación

- **Anterior**: [T01 - cargo-tarpaulin](../T01_Cargo_tarpaulin/README.md)
- **Siguiente**: [T03 - Cobertura realista](../T03_Cobertura_realista/README.md)

---

> **Sección 5: Cobertura en Rust** — Tópico 2 de 3 completado
>
> - T01: cargo-tarpaulin ✓
> - T02: llvm-cov — RUSTFLAGS, llvm-profdata, llvm-cov show, branch coverage ✓
> - T03: Cobertura realista (siguiente)
