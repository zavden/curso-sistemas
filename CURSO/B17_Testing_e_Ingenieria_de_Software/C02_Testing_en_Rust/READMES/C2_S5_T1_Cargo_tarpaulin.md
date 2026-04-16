# T01 - cargo-tarpaulin: cobertura de código para Rust

## Índice

1. [Qué es la cobertura de código](#1-qué-es-la-cobertura-de-código)
2. [Tipos de cobertura](#2-tipos-de-cobertura)
3. [Por qué medir cobertura](#3-por-qué-medir-cobertura)
4. [Panorama de herramientas de cobertura en Rust](#4-panorama-de-herramientas-de-cobertura-en-rust)
5. [cargo-tarpaulin: el estándar de facto](#5-cargo-tarpaulin-el-estándar-de-facto)
6. [Instalación](#6-instalación)
7. [Primer uso: cargo tarpaulin](#7-primer-uso-cargo-tarpaulin)
8. [Interpretación del output en terminal](#8-interpretación-del-output-en-terminal)
9. [Output HTML: reporte visual](#9-output-html-reporte-visual)
10. [Output JSON: integración con herramientas](#10-output-json-integración-con-herramientas)
11. [Output Lcov: compatibilidad universal](#11-output-lcov-compatibilidad-universal)
12. [Output XML (Cobertura): CI/CD](#12-output-xml-cobertura-cicd)
13. [Múltiples formatos simultáneos](#13-múltiples-formatos-simultáneos)
14. [Exclusiones: ignorar código](#14-exclusiones-ignorar-código)
15. [Configuración en tarpaulin.toml](#15-configuración-en-tarpaulintoml)
16. [Filtrar por archivos y directorios](#16-filtrar-por-archivos-y-directorios)
17. [Workspaces: cobertura multi-crate](#17-workspaces-cobertura-multi-crate)
18. [Integración con CI/CD](#18-integración-con-cicd)
19. [Integración con Codecov](#19-integración-con-codecov)
20. [Integración con Coveralls](#20-integración-con-coveralls)
21. [Umbrales de cobertura: --fail-under](#21-umbrales-de-cobertura---fail-under)
22. [Modos de instrumentación: Ptrace vs Llvm](#22-modos-de-instrumentación-ptrace-vs-llvm)
23. [Flags y opciones avanzadas](#23-flags-y-opciones-avanzadas)
24. [Debugging: cuando tarpaulin reporta datos incorrectos](#24-debugging-cuando-tarpaulin-reporta-datos-incorrectos)
25. [Comparación con C y Go](#25-comparación-con-c-y-go)
26. [Errores comunes](#26-errores-comunes)
27. [Ejemplo completo: proyecto con cobertura configurada](#27-ejemplo-completo-proyecto-con-cobertura-configurada)
28. [Programa de práctica](#28-programa-de-práctica)
29. [Ejercicios](#29-ejercicios)

---

## 1. Qué es la cobertura de código

La cobertura de código mide qué porcentaje del código fuente se ejecuta durante los tests. Es una **métrica**, no una garantía de calidad.

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Código fuente              Tests                                │
│  ┌────────────────────┐     ┌────────────────────┐              │
│  │ fn process(x: i32) │     │ #[test]            │              │
│  │   -> Result<...>   │     │ fn test_positive() │              │
│  │ {                  │     │ {                  │              │
│  │   if x > 0 {      │ ◄───│   process(5)       │              │
│  │ ✓   Ok(x * 2)     │     │   // cubre rama >0 │              │
│  │   } else if x == 0│     │ }                  │              │
│  │ ✗   Err("zero")   │     │                    │              │
│  │   } else {        │     │ // No hay test      │              │
│  │ ✗   Err("neg")    │     │ // para x==0 ni x<0│              │
│  │   }               │     │                    │              │
│  │ }                  │     └────────────────────┘              │
│  └────────────────────┘                                          │
│                                                                  │
│  Cobertura de líneas: 1 de 3 ramas ejecutadas = 33%              │
│  Cobertura de líneas: ~4 de 8 líneas ejecutadas = 50%            │
│                                                                  │
│  ¿El código funciona? No lo sabemos. Solo sabemos                │
│  que NO hemos testeado el 67% de las ramas.                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Lo que la cobertura dice y no dice

```
┌────────────────────────────────────┬──────────────────────────────────┐
│ La cobertura DICE                  │ La cobertura NO dice             │
├────────────────────────────────────┼──────────────────────────────────┤
│ "Esta línea se ejecutó             │ "Esta línea funciona             │
│  durante los tests"               │  correctamente"                  │
├────────────────────────────────────┼──────────────────────────────────┤
│ "Esta rama nunca se probó"        │ "Si la pruebo, encontraré        │
│                                    │  un bug"                         │
├────────────────────────────────────┼──────────────────────────────────┤
│ "El 80% del código se ejecutó"    │ "El código tiene 80% de          │
│                                    │  probabilidad de funcionar"      │
├────────────────────────────────────┼──────────────────────────────────┤
│ "Este módulo tiene 0% de          │ "Este módulo es de mala          │
│  cobertura"                        │  calidad"                        │
│                                    │  (puede ser untestable by design)│
└────────────────────────────────────┴──────────────────────────────────┘
```

### La metáfora del mapa

```
Cobertura = mapa de exploración

  ┌─────────────────────────────────────┐
  │  ██████████░░░░░░░░░░░░░░░░░░░░░░░ │  25% explorado
  │  Terreno explorado  │  Inexplorado  │
  │                     │               │
  │  "Sé que este       │  "No sé si    │
  │   terreno es        │   hay minas   │
  │   transitable"      │   aquí"       │
  └─────────────────────────────────────┘

  Cobertura alta = has explorado mucho terreno
  Cobertura baja = hay mucho terreno desconocido

  Pero explorar terreno NO es lo mismo que
  haberlo limpiado de minas (verificado).
```

---

## 2. Tipos de cobertura

### Line coverage (cobertura de líneas)

La más común y la que tarpaulin reporta por defecto.

```rust
pub fn categorize(x: i32) -> &'static str {
    if x > 100 {           // línea 2: ejecutada si se llama
        "high"             // línea 3: ejecutada si x > 100
    } else if x > 0 {     // línea 4: ejecutada si x <= 100
        "medium"           // línea 5: ejecutada si 0 < x <= 100
    } else if x == 0 {    // línea 6: ejecutada si x <= 0
        "zero"             // línea 7: ejecutada si x == 0
    } else {               // línea 8: ejecutada si x < 0
        "negative"         // línea 9: ejecutada si x < 0
    }                      // línea 10
}

// Test: categorize(50)
// Líneas ejecutadas: 2, 4, 5
// Líneas NO ejecutadas: 3, 6, 7, 8, 9
// Line coverage: 3/8 líneas de código = 37.5%
```

### Branch coverage (cobertura de ramas)

Cada `if` tiene 2 ramas (true/false). Branch coverage verifica ambas.

```rust
pub fn safe_divide(a: f64, b: f64) -> Option<f64> {
    if b != 0.0 {     // rama true + rama false
        Some(a / b)
    } else {
        None
    }
}

// Test 1: safe_divide(10.0, 2.0) → cubre rama true
// Branch coverage: 1/2 = 50%

// Test 2: safe_divide(10.0, 0.0) → cubre rama false
// Branch coverage con ambos tests: 2/2 = 100%
```

### Function coverage (cobertura de funciones)

¿Se llamó la función al menos una vez?

```rust
pub fn add(a: i32, b: i32) -> i32 { a + b }      // ¿llamada? Sí/No
pub fn subtract(a: i32, b: i32) -> i32 { a - b }  // ¿llamada? Sí/No
pub fn multiply(a: i32, b: i32) -> i32 { a * b }  // ¿llamada? Sí/No

// Si solo testeas add() → function coverage = 1/3 = 33%
```

### Region coverage (cobertura de regiones)

Nivel más fino: cada subexpresión o bloque.

```rust
let result = if condition_a() && condition_b() {
    //  ▲ región 1       ▲ región 2
    do_something()  // región 3
} else {
    do_other()      // región 4
};
```

### Tabla comparativa

```
┌───────────────────┬──────────────────┬──────────────────────────────┐
│ Tipo              │ Granularidad     │ Soporte en tarpaulin         │
├───────────────────┼──────────────────┼──────────────────────────────┤
│ Line coverage     │ Por línea        │ ✓ (default)                  │
│ Branch coverage   │ Por rama if/else │ ✓ (--branch, experimental)   │
│ Function coverage │ Por función      │ Parcial (derivable del line) │
│ Region coverage   │ Por subexpresión │ ✗ (requiere llvm-cov)        │
└───────────────────┴──────────────────┴──────────────────────────────┘
```

---

## 3. Por qué medir cobertura

### Beneficios reales

```
1. Detectar código no testeado
   "Nadie testó el path de error de parse_config()"
   → Escribir test antes de que un bug llegue a producción

2. Guiar el esfuerzo de testing
   "El módulo auth tiene 30% de cobertura, payment tiene 90%"
   → Priorizar tests en auth

3. Prevenir regresiones de cobertura
   "El PR bajó la cobertura de 85% a 72%"
   → Revisar si el nuevo código tiene tests

4. Documentar la confianza
   "Las utilidades de crypto están al 95%"
   → El equipo puede confiar en ese módulo

5. Encontrar código muerto
   "Esta función tiene 0% de cobertura y ningún caller"
   → ¿Es código muerto? ¿Podemos eliminarlo?
```

### Trampas de la cobertura

```
┌──────────────────────────────────────────────────────────────────┐
│  TRAMPA: 100% de cobertura ≠ código correcto                    │
│                                                                  │
│  fn add(a: i32, b: i32) -> i32 {                                │
│      a * b  // BUG: multiplica en vez de sumar                  │
│  }                                                              │
│                                                                  │
│  #[test]                                                         │
│  fn test_add() {                                                 │
│      let result = add(2, 3);                                    │
│      // 100% line coverage, 100% function coverage              │
│      // Pero NO verificamos el resultado                        │
│      // assert_eq!(result, 5); ← FALTA ESTO                     │
│  }                                                              │
│                                                                  │
│  La cobertura dice "la línea se ejecutó"                         │
│  No dice "el resultado es correcto"                              │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  TRAMPA: Gaming the metric (inflar la cobertura)                │
│                                                                  │
│  // Código para inflar cobertura sin valor real:                │
│  #[test]                                                         │
│  fn touch_all_code() {                                          │
│      let _ = process(1);  // no assert                          │
│      let _ = process(0);  // no assert                          │
│      let _ = process(-1); // no assert                          │
│  }                                                              │
│  // 100% de cobertura, 0% de verificación                       │
│                                                                  │
│  Solución: la cobertura es una métrica de EXPLORACIÓN,           │
│  no de VERIFICACIÓN. Complementar con asserts en cada test.      │
└──────────────────────────────────────────────────────────────────┘
```

### La guía de cobertura pragmática

```
┌──────────────┬──────────────────────────────────────────────────┐
│ % Cobertura  │ Interpretación                                   │
├──────────────┼──────────────────────────────────────────────────┤
│   0 - 20%    │ Prácticamente sin tests. Alto riesgo.            │
│  20 - 50%    │ Tests básicos, muchas ramas sin probar.          │
│  50 - 70%    │ Cobertura aceptable para muchos proyectos.       │
│  70 - 85%    │ Buena cobertura. Balance esfuerzo/valor.         │
│  85 - 95%    │ Muy buena. Los últimos % son los más costosos.   │
│  95 - 100%   │ ¿Justificado? Solo para código crítico           │
│              │ (crypto, parsing de protocolos, seguridad).      │
└──────────────┴──────────────────────────────────────────────────┘

Regla de oro: apuntar a 70-85% en la mayoría de proyectos.
El esfuerzo de 85% → 100% suele ser desproporcionado.
```

---

## 4. Panorama de herramientas de cobertura en Rust

```
┌──────────────────────────────────────────────────────────────────┐
│                Herramientas de cobertura para Rust               │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │ cargo-tarpaulin  │  El más usado. Linux only.                │
│  │                  │  Instrumentación ptrace o llvm.            │
│  │  cargo tarpaulin │  Output: terminal, HTML, JSON, XML, Lcov. │
│  │                  │  ~60M descargas.                           │
│  └──────────────────┘                                           │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │ cargo-llvm-cov   │  Basado en LLVM source-based coverage.    │
│  │                  │  Cross-platform (Linux, macOS, Windows).   │
│  │  cargo llvm-cov  │  Más preciso, requiere llvm-tools.        │
│  │                  │  ~15M descargas.                           │
│  └──────────────────┘                                           │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │ grcov (Mozilla)  │  Procesador de datos de cobertura.        │
│  │                  │  No instrumenta — consume datos de         │
│  │                  │  -C instrument-coverage.                   │
│  │                  │  Output: HTML, Lcov, Coveralls.            │
│  └──────────────────┘                                           │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │ kcov             │  Herramienta genérica (no solo Rust).     │
│  │                  │  Usa ptrace. Menos preciso.                │
│  │                  │  En desuso para Rust.                      │
│  └──────────────────┘                                           │
│                                                                  │
│  Recomendación:                                                  │
│  - Linux: cargo-tarpaulin (simplicidad) o cargo-llvm-cov        │
│  - macOS/Windows: cargo-llvm-cov (tarpaulin no disponible)      │
│  - CI: cargo-tarpaulin + Codecov/Coveralls                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. cargo-tarpaulin: el estándar de facto

```
┌──────────────────────────────────────────────────────────────────┐
│                      cargo-tarpaulin                             │
│                                                                  │
│  Autor: Daniel McKenna (@xd009642)                               │
│  Repo: github.com/xd009642/tarpaulin                            │
│  Licencia: MIT/Apache-2.0                                        │
│  Plataforma: Linux only (usa ptrace o LLVM)                     │
│  MSRV: Rust 1.60+                                               │
│                                                                  │
│  Flujo:                                                          │
│  ┌──────────┐    ┌──────────────┐    ┌──────────┐              │
│  │ Compile  │───►│ Instrument   │───►│ Run tests│              │
│  │ tests    │    │ (ptrace/llvm)│    │ & collect│              │
│  └──────────┘    └──────────────┘    │ coverage │              │
│                                      └────┬─────┘              │
│                                           │                     │
│                                      ┌────▼─────┐              │
│                                      │ Generate │              │
│                                      │ report   │              │
│                                      │ (HTML/   │              │
│                                      │  JSON/   │              │
│                                      │  Lcov/   │              │
│                                      │  XML)    │              │
│                                      └──────────┘              │
│                                                                  │
│  Características:                                                │
│  ✓ Line coverage                                                │
│  ✓ Branch coverage (experimental)                               │
│  ✓ Exclusiones con atributo o config                            │
│  ✓ Múltiples formatos de output                                 │
│  ✓ Integración con Codecov/Coveralls                            │
│  ✓ Soporte para workspaces                                      │
│  ✓ Umbrales configurables (--fail-under)                        │
│  ✓ Configuración en archivo (.tarpaulin.toml)                   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 6. Instalación

### Desde crates.io (recomendado)

```bash
# Instalar la última versión estable
cargo install cargo-tarpaulin

# Verificar
cargo tarpaulin --version
# cargo-tarpaulin version: 0.27.x
```

### Tiempo de compilación

La primera instalación compila tarpaulin desde fuente (~2-5 minutos dependiendo de la máquina). Las actualizaciones posteriores son más rápidas.

### Con cargo-binstall (más rápido)

```bash
# Si tienes cargo-binstall
cargo binstall cargo-tarpaulin

# Descarga el binario precompilado, no compila
```

### Desde el package manager del sistema

```bash
# Fedora/RHEL (si está disponible)
# No hay paquete oficial — usar cargo install

# Arch Linux (AUR)
# yay -S cargo-tarpaulin

# Nix
# nix-env -iA nixpkgs.cargo-tarpaulin
```

### Dependencias del sistema

tarpaulin en modo ptrace necesita:

```bash
# Fedora/RHEL
sudo dnf install libssl-devel pkg-config

# Ubuntu/Debian
sudo apt install libssl-dev pkg-config

# Estas suelen ya estar instaladas si tienes el toolchain de Rust
```

### Verificar que funciona

```bash
# Crear un proyecto de prueba
cargo new --lib tarpaulin_test
cd tarpaulin_test

# Ejecutar tarpaulin (usa el test por defecto que cargo new genera)
cargo tarpaulin
```

---

## 7. Primer uso: cargo tarpaulin

### El proyecto de ejemplo

```
my_project/
├── Cargo.toml
├── src/
│   └── lib.rs
└── tests/
    └── integration.rs
```

```rust
// src/lib.rs
pub fn fizzbuzz(n: u32) -> String {
    if n % 15 == 0 {
        "FizzBuzz".to_string()
    } else if n % 3 == 0 {
        "Fizz".to_string()
    } else if n % 5 == 0 {
        "Buzz".to_string()
    } else {
        n.to_string()
    }
}

pub fn is_palindrome(s: &str) -> bool {
    let cleaned: String = s.chars()
        .filter(|c| c.is_alphanumeric())
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();
    let reversed: String = cleaned.chars().rev().collect();
    cleaned == reversed
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_fizz() {
        assert_eq!(fizzbuzz(3), "Fizz");
        assert_eq!(fizzbuzz(6), "Fizz");
    }

    #[test]
    fn test_buzz() {
        assert_eq!(fizzbuzz(5), "Buzz");
        assert_eq!(fizzbuzz(10), "Buzz");
    }

    #[test]
    fn test_fizzbuzz() {
        assert_eq!(fizzbuzz(15), "FizzBuzz");
        assert_eq!(fizzbuzz(30), "FizzBuzz");
    }

    #[test]
    fn test_number() {
        assert_eq!(fizzbuzz(1), "1");
        assert_eq!(fizzbuzz(7), "7");
    }

    // Nota: NO hay tests para is_palindrome
}
```

### Ejecutar

```bash
cargo tarpaulin
```

### Output típico

```
Apr 05 12:00:00.000 INFO cargo_tarpaulin::config: Creating config
Apr 05 12:00:01.000 INFO cargo_tarpaulin: Running tarpaulin
Apr 05 12:00:02.000 INFO cargo_tarpaulin: Building project
   Compiling my_project v0.1.0 (/home/user/my_project)
    Finished test [unoptimized + debuginfo] target(s) in 0.50s
Apr 05 12:00:03.000 INFO cargo_tarpaulin: Launching test
Apr 05 12:00:03.100 INFO cargo_tarpaulin: running /home/user/my_project/target/debug/deps/my_project-abc123

running 4 tests
test tests::test_buzz ... ok
test tests::test_fizz ... ok
test tests::test_fizzbuzz ... ok
test tests::test_number ... ok

test result: ok. 4 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out

Apr 05 12:00:03.200 INFO cargo_tarpaulin::report: Coverage Results:
|| Tested/Total Lines:
|| src/lib.rs: 8/14 (57.14%)
||
57.14% coverage, 8/14 lines covered
```

---

## 8. Interpretación del output en terminal

### Línea por línea

```
|| src/lib.rs: 8/14 (57.14%)

  src/lib.rs  → archivo analizado
  8/14        → 8 líneas ejecutadas de 14 líneas de código
  (57.14%)    → porcentaje de cobertura
```

### ¿Por qué 8 de 14?

```rust
// Líneas de CÓDIGO (no comentarios, no en blanco):
pub fn fizzbuzz(n: u32) -> String {       // 1  ✓ ejecutada
    if n % 15 == 0 {                      // 2  ✓ ejecutada
        "FizzBuzz".to_string()            // 3  ✓ ejecutada
    } else if n % 3 == 0 {               // 4  ✓ ejecutada
        "Fizz".to_string()               // 5  ✓ ejecutada
    } else if n % 5 == 0 {               // 6  ✓ ejecutada
        "Buzz".to_string()               // 7  ✓ ejecutada
    } else {                              //    (no cuenta como "línea")
        n.to_string()                     // 8  ✓ ejecutada
    }
}

pub fn is_palindrome(s: &str) -> bool {   // 9  ✗ nunca llamada
    let cleaned: String = s.chars()       // 10 ✗
        .filter(|c| c.is_alphanumeric())  // 11 ✗
        .map(|c| c.to_lowercase()...      // 12 ✗
        .collect();                        // 13 ✗
    let reversed: String = ...            // 14 ✗
    cleaned == reversed                    //    (parte de la línea anterior)
}

// 8 líneas ejecutadas (fizzbuzz completo)
// 6 líneas NO ejecutadas (is_palindrome completo)
// Total: 8/14 = 57.14%
```

### Verbose mode para ver cada línea

```bash
cargo tarpaulin -v
```

Muestra cada línea con su estado:

```
|| src/lib.rs: 8/14
   1|      1|pub fn fizzbuzz(n: u32) -> String {
   2|      4|    if n % 15 == 0 {
   3|      2|        "FizzBuzz".to_string()
   4|      2|    } else if n % 3 == 0 {
   5|      2|        "Fizz".to_string()
   6|      2|    } else if n % 5 == 0 {
   7|      2|        "Buzz".to_string()
   8|      0|    } else {
   9|      2|        n.to_string()
  10|       |    }
  11|       |}
  12|       |
  13|      0|pub fn is_palindrome(s: &str) -> bool {
  14|      0|    let cleaned: String = s.chars()
  ...
```

El número después de `|` es el **hit count** (cuántas veces se ejecutó la línea):
- `0` = nunca ejecutada (no cubierta)
- `1+` = ejecutada (cubierta)
- vacío = no es una línea de código (declaración, cierre de bloque, etc.)

---

## 9. Output HTML: reporte visual

```bash
cargo tarpaulin --out html
```

Genera `tarpaulin-report.html` en el directorio raíz del proyecto.

### Qué contiene el HTML

```
┌──────────────────────────────────────────────────────────────────┐
│  tarpaulin-report.html                                           │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  Coverage Summary                                          │  │
│  │                                                            │  │
│  │  Total coverage: 57.14% (8/14 lines)                      │  │
│  │                                                            │  │
│  │  File                    Coverage    Lines                 │  │
│  │  ────                    ────────    ─────                 │  │
│  │  src/lib.rs              57.14%      8/14                  │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Al hacer click en un archivo:                                   │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  src/lib.rs                                                │  │
│  │                                                            │  │
│  │  ██ pub fn fizzbuzz(n: u32) -> String {                    │  │
│  │  ██     if n % 15 == 0 {                                   │  │
│  │  ██         "FizzBuzz".to_string()                         │  │
│  │  ██     } else if n % 3 == 0 {                             │  │
│  │  ██         "Fizz".to_string()                             │  │
│  │  ██     } else if n % 5 == 0 {                             │  │
│  │  ██         "Buzz".to_string()                             │  │
│  │  ██     } else {                                            │  │
│  │  ██         n.to_string()                                  │  │
│  │       }                                                     │  │
│  │  }                                                         │  │
│  │                                                            │  │
│  │  ░░ pub fn is_palindrome(s: &str) -> bool {                │  │
│  │  ░░     let cleaned: String = s.chars()                    │  │
│  │  ░░         .filter(|c| c.is_alphanumeric())               │  │
│  │  ░░         .map(|c| c.to_lowercase().next().unwrap())     │  │
│  │  ░░         .collect();                                    │  │
│  │  ░░     let reversed: String = cleaned.chars().rev()...    │  │
│  │  ░░     cleaned == reversed                                │  │
│  │  }                                                         │  │
│  │                                                            │  │
│  │  ██ = cubierta (verde)    ░░ = no cubierta (rojo)         │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Personalizar el path del HTML

```bash
# Guardar en un directorio específico
cargo tarpaulin --out html --output-dir coverage/

# El archivo se creará en coverage/tarpaulin-report.html
```

### Abrir automáticamente

```bash
# Generar y abrir en el navegador
cargo tarpaulin --out html && xdg-open tarpaulin-report.html
```

---

## 10. Output JSON: integración con herramientas

```bash
cargo tarpaulin --out json
```

Genera `tarpaulin-report.json`:

```json
{
  "coverage": [
    {
      "file": "src/lib.rs",
      "traces": [
        {
          "line": 1,
          "address": "0x...",
          "length": 1,
          "stats": {
            "Line": 4
          }
        },
        {
          "line": 2,
          "address": "0x...",
          "length": 1,
          "stats": {
            "Line": 4
          }
        },
        {
          "line": 13,
          "address": "0x...",
          "length": 1,
          "stats": {
            "Line": 0
          }
        }
      ],
      "covered": 8,
      "coverable": 14
    }
  ]
}
```

### Campos del JSON

```
coverage[]          → array de archivos
  .file             → path del archivo fuente
  .traces[]         → array de líneas instrumentadas
    .line           → número de línea
    .stats.Line     → hit count (0 = no cubierta)
  .covered          → total de líneas cubiertas
  .coverable        → total de líneas cubribles
```

### Procesamiento con jq

```bash
# Extraer archivos con cobertura < 80%
cargo tarpaulin --out json 2>/dev/null
cat tarpaulin-report.json | jq '
  .coverage[]
  | select((.covered / .coverable * 100) < 80)
  | {file: .file, coverage: (.covered / .coverable * 100 | floor)}
'

# Listar líneas no cubiertas
cat tarpaulin-report.json | jq '
  .coverage[]
  | select(.file == "src/lib.rs")
  | .traces[]
  | select(.stats.Line == 0)
  | .line
'
```

---

## 11. Output Lcov: compatibilidad universal

Lcov es un formato estándar de cobertura originado en el mundo Linux/C. Muchas herramientas lo entienden.

```bash
cargo tarpaulin --out lcov
```

Genera `lcov.info`:

```
TN:
SF:src/lib.rs
DA:1,4
DA:2,4
DA:3,2
DA:4,2
DA:5,2
DA:6,2
DA:7,2
DA:8,2
DA:13,0
DA:14,0
DA:15,0
DA:16,0
DA:17,0
DA:18,0
LH:8
LF:14
end_of_record
```

### Formato Lcov decodificado

```
TN:              → Test Name (vacío = default)
SF:src/lib.rs    → Source File
DA:1,4           → line 1, hit 4 times
DA:13,0          → line 13, hit 0 times (no cubierta)
LH:8             → Lines Hit: 8
LF:14            → Lines Found (cubribles): 14
end_of_record    → fin del archivo
```

### Usar con genhtml (visualización)

```bash
# Instalar lcov (incluye genhtml)
sudo dnf install lcov    # Fedora
sudo apt install lcov    # Ubuntu

# Generar reporte HTML desde lcov
cargo tarpaulin --out lcov
genhtml lcov.info -o coverage_html/
xdg-open coverage_html/index.html
```

genhtml produce un reporte HTML más detallado que el HTML nativo de tarpaulin, con navegación por directorios y resúmenes.

### Usar con IDE (VS Code)

```
La extensión "Coverage Gutters" para VS Code puede leer lcov.info
y mostrar cobertura directamente en el editor:

  Línea cubierta:    █ verde en el margen
  Línea no cubierta: █ rojo en el margen

Configuración en settings.json:
{
  "coverage-gutters.coverageFileNames": ["lcov.info"]
}
```

---

## 12. Output XML (Cobertura): CI/CD

El formato Cobertura XML es estándar en sistemas CI como Jenkins, GitLab CI, Azure DevOps:

```bash
cargo tarpaulin --out xml
```

Genera `cobertura.xml`:

```xml
<?xml version="1.0" ?>
<coverage version="0.1"
          timestamp="1700000000"
          lines-valid="14"
          lines-covered="8"
          line-rate="0.5714"
          branches-valid="0"
          branches-covered="0"
          branch-rate="0"
          complexity="0">
  <packages>
    <package name="my_project"
             line-rate="0.5714"
             branch-rate="0"
             complexity="0">
      <classes>
        <class name="lib.rs"
               filename="src/lib.rs"
               line-rate="0.5714"
               branch-rate="0">
          <lines>
            <line number="1" hits="4"/>
            <line number="2" hits="4"/>
            <line number="3" hits="2"/>
            <!-- ... -->
            <line number="13" hits="0"/>
            <line number="14" hits="0"/>
          </lines>
        </class>
      </classes>
    </package>
  </packages>
</coverage>
```

### Uso en GitLab CI

```yaml
# .gitlab-ci.yml
test:
  stage: test
  image: rust:latest
  script:
    - cargo install cargo-tarpaulin
    - cargo tarpaulin --out xml
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: cobertura.xml
```

---

## 13. Múltiples formatos simultáneos

```bash
# Generar terminal + HTML + JSON + Lcov en una sola ejecución
cargo tarpaulin --out html --out json --out lcov

# O con la flag --out-dir para organizar
cargo tarpaulin --out html --out json --out lcov --output-dir coverage/
```

Esto ejecuta los tests una sola vez y genera todos los reportes.

### Estructura típica de output

```
project/
├── tarpaulin-report.html      ← para revisión local
├── tarpaulin-report.json      ← para procesamiento programático
├── lcov.info                  ← para IDE y genhtml
├── cobertura.xml              ← para CI/CD
└── src/
    └── lib.rs
```

---

## 14. Exclusiones: ignorar código

No todo el código necesita cobertura. Excluir código que no tiene sentido testear:

### Atributo #[cfg(not(tarpaulin_include))]

```rust
// Excluir una función completa
#[cfg(not(tarpaulin_include))]
fn main() {
    // main() no se testea unitariamente
    let config = load_config();
    start_server(config);
}

// Excluir un bloque impl
#[cfg(not(tarpaulin_include))]
impl std::fmt::Display for MyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MyError::NotFound => write!(f, "not found"),
            MyError::InvalidInput(msg) => write!(f, "invalid: {}", msg),
        }
    }
}
```

### Atributo #[tarpaulin::skip]

```rust
// Forma más concisa (requiere tarpaulin 0.18+)
#[tarpaulin::skip]
fn main() {
    // excluida de la cobertura
}

// En un método
impl Config {
    #[tarpaulin::skip]
    pub fn from_env() -> Self {
        // Lee variables de entorno — no testeable sin setup
        Config {
            host: std::env::var("HOST").unwrap_or("localhost".into()),
            port: std::env::var("PORT").unwrap_or("8080".into()).parse().unwrap(),
        }
    }
}
```

### Comentarios inline (tarpaulin 0.25+)

```rust
pub fn process(data: &str) -> Result<Output, Error> {
    let parsed = parse(data)?;

    if parsed.is_empty() {
        return Err(Error::Empty); // tarpaulin:skip — never happens in practice
    }

    // resto del código...
    Ok(transform(parsed))
}
```

### Excluir módulos completos por path

```bash
# Excluir tests/ y examples/
cargo tarpaulin --exclude-files "tests/*" --exclude-files "examples/*"

# Excluir un módulo específico
cargo tarpaulin --exclude-files "src/generated/*"
```

### Qué excluir y qué no

```
┌──────────────────────────────────┬─────────────────────────────────────┐
│ EXCLUIR                          │ NO EXCLUIR                          │
├──────────────────────────────────┼─────────────────────────────────────┤
│ main() / entry points            │ Lógica de negocio                   │
│ Display/Debug impls triviales    │ Parsing / validación                │
│ Código generado (derive, macros) │ Error handling significativo        │
│ Bindings FFI                     │ Algoritmos                          │
│ Código de infraestructura        │ Transformaciones de datos           │
│ (logging setup, signal handlers) │ State machines                      │
│ Código unreachable (panic paths  │ Integración de componentes          │
│ detrás de invariantes probadas)  │                                     │
│ Tests mismos                     │ Servicios y lógica de dominio       │
│ Benchmarks                       │ Utilidades usadas en producción     │
└──────────────────────────────────┴─────────────────────────────────────┘
```

---

## 15. Configuración en tarpaulin.toml

En lugar de pasar flags en la línea de comandos, configura tarpaulin en un archivo:

### .tarpaulin.toml (raíz del proyecto)

```toml
[default]
# Formatos de output
out = ["html", "json", "lcov"]

# Directorio de output
output-dir = "coverage"

# Umbral mínimo (falla si la cobertura es menor)
fail-under = 70.0

# Excluir archivos
exclude-files = [
    "tests/*",
    "examples/*",
    "src/generated/*",
    "benches/*",
]

# Excluir paths de código (regex sobre el path del archivo)
# exclude-path = ["target"]

# Tiempo máximo por test
timeout = "120"

# Ejecutar tests en release mode
# release = true

# Verbose output
# verbose = true

# Ignorar tests que panic (no contarlos como fallo de cobertura)
# ignore-panics = true

# Engine: ptrace (default en x86) o llvm
# engine = "llvm"

# Correr solo tests, no doc-tests ni benchmarks
# run-types = ["tests"]
```

### Ubicaciones del archivo de configuración

tarpaulin busca configuración en este orden:

1. `./tarpaulin.toml`
2. `./.tarpaulin.toml`
3. `./Cargo.toml` (sección `[package.metadata.tarpaulin]`)

### Configuración en Cargo.toml

```toml
[package]
name = "my_project"
version = "0.1.0"
edition = "2021"

[package.metadata.tarpaulin]
out = ["html", "lcov"]
fail-under = 75.0
exclude-files = ["src/generated/*"]
```

### Múltiples perfiles

```toml
# .tarpaulin.toml

[default]
out = ["html"]
fail-under = 70.0

[ci]
# Perfil para CI: más estricto, formatos para reportes
out = ["xml", "lcov"]
fail-under = 80.0
timeout = "300"

[quick]
# Perfil para desarrollo rápido: solo terminal
out = ["stdout"]
timeout = "60"
```

```bash
# Usar un perfil específico
cargo tarpaulin --config .tarpaulin.toml --profile ci
```

---

## 16. Filtrar por archivos y directorios

### --files: incluir solo ciertos archivos

```bash
# Solo medir cobertura de src/lib.rs
cargo tarpaulin --files "src/lib.rs"

# Solo archivos en src/services/
cargo tarpaulin --files "src/services/*"

# Múltiples patterns
cargo tarpaulin --files "src/models/*" --files "src/services/*"
```

### --exclude-files: excluir archivos

```bash
# Excluir tests y examples
cargo tarpaulin --exclude-files "tests/*" --exclude-files "examples/*"

# Excluir un archivo específico
cargo tarpaulin --exclude-files "src/generated_bindings.rs"
```

### --packages: filtrar en workspaces

```bash
# Solo medir un crate del workspace
cargo tarpaulin --packages my_core_lib

# Múltiples crates
cargo tarpaulin --packages my_core_lib --packages my_api

# Excluir un crate
cargo tarpaulin --exclude my_benchmark_crate
```

### Combinaciones útiles

```bash
# Cobertura de la lógica de negocio, excluyendo infra
cargo tarpaulin \
    --files "src/domain/*" \
    --files "src/services/*" \
    --exclude-files "src/infrastructure/*" \
    --exclude-files "src/main.rs"

# Solo cobertura del crate core en un workspace
cargo tarpaulin \
    --packages my_project_core \
    --exclude-files "tests/*"
```

---

## 17. Workspaces: cobertura multi-crate

### Estructura de workspace

```
my_workspace/
├── Cargo.toml           ← [workspace]
├── core/
│   ├── Cargo.toml
│   └── src/lib.rs
├── api/
│   ├── Cargo.toml
│   └── src/lib.rs
├── cli/
│   ├── Cargo.toml
│   └── src/main.rs
└── tests/
    └── integration.rs
```

### Cobertura del workspace completo

```bash
# Medir todo el workspace
cargo tarpaulin --workspace

# Output:
# || core/src/lib.rs: 45/50 (90.00%)
# || api/src/lib.rs: 30/40 (75.00%)
# || cli/src/main.rs: 5/20 (25.00%)
# ||
# || 80/110 (72.73%) coverage
```

### Excluir crates del workspace

```bash
# Todo excepto el CLI (que tiene main)
cargo tarpaulin --workspace --exclude cli

# Solo core y api
cargo tarpaulin --packages core --packages api
```

### Configuración para workspace

```toml
# .tarpaulin.toml en la raíz del workspace
[default]
workspace = true
exclude = ["cli"]  # excluir el binario
out = ["html", "lcov"]
fail-under = 75.0
```

---

## 18. Integración con CI/CD

### GitHub Actions

```yaml
# .github/workflows/coverage.yml
name: Coverage

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable

      - name: Install tarpaulin
        run: cargo install cargo-tarpaulin

      - name: Run coverage
        run: cargo tarpaulin --out xml --out lcov --fail-under 70

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: lcov.info
          fail_ci_if_error: false

      - name: Archive coverage report
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: |
            cobertura.xml
            lcov.info
```

### GitLab CI

```yaml
# .gitlab-ci.yml
coverage:
  stage: test
  image: rust:latest
  before_script:
    - cargo install cargo-tarpaulin
  script:
    - cargo tarpaulin --out xml --fail-under 70
  coverage: '/\d+\.\d+% coverage/'
  artifacts:
    reports:
      coverage_report:
        coverage_format: cobertura
        path: cobertura.xml
```

### Flujo CI con cobertura

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   PR abierto │───►│ CI ejecuta   │───►│ ¿Cobertura   │
│              │    │ tarpaulin    │    │  >= umbral?  │
│              │    │              │    │              │
└──────────────┘    └──────────────┘    └──┬───────┬──┘
                                           │       │
                                          SÍ       NO
                                           │       │
                                           ▼       ▼
                                      ┌────────┐ ┌────────┐
                                      │ Upload │ │ CI     │
                                      │ a      │ │ FALLA  │
                                      │ Codecov│ │        │
                                      │        │ │ "cover-│
                                      │ Badge: │ │ age    │
                                      │ 82%    │ │ below  │
                                      │        │ │ 70%"   │
                                      └────────┘ └────────┘
```

---

## 19. Integración con Codecov

Codecov es el servicio más popular para visualizar cobertura en PRs.

### Setup

```bash
# 1. Generar lcov
cargo tarpaulin --out lcov

# 2. Upload manual (con token)
bash <(curl -s https://codecov.io/bash) -f lcov.info

# 3. O usar la GitHub Action (recomendado)
```

### codecov.yml (configuración del servicio)

```yaml
# codecov.yml en la raíz del repo
coverage:
  status:
    project:
      default:
        target: 75%          # umbral del proyecto
        threshold: 2%        # tolerancia (permite bajar 2%)
    patch:
      default:
        target: 80%          # código nuevo debe tener 80%+

ignore:
  - "src/main.rs"
  - "tests/"
  - "examples/"
  - "benches/"

comment:
  layout: "reach, diff, flags, files"
  behavior: default
  require_changes: true       # solo comentar si hay cambios de cobertura
```

### Qué muestra Codecov en un PR

```
┌──────────────────────────────────────────────────────────────┐
│  Codecov Report                                              │
│                                                              │
│  Merging #42 (abc1234) into main (def5678)                  │
│                                                              │
│  Coverage: 82.3% (+1.2%)  ▲                                │
│                                                              │
│  ┌────────────────────────┬────────┬────────┬────────┐      │
│  │ File                   │ Lines  │ Stmts  │ Cover  │      │
│  ├────────────────────────┼────────┼────────┼────────┤      │
│  │ src/services/auth.rs   │ +15    │ +12    │ 93.3%  │      │
│  │ src/models/user.rs     │ +5     │ +5     │ 100%   │      │
│  │ src/handlers/api.rs    │ +20    │ +14    │ 70.0%  │ ⚠   │
│  └────────────────────────┴────────┴────────┴────────┘      │
│                                                              │
│  ⚠ api.rs: new code has 70% coverage (target: 80%)          │
└──────────────────────────────────────────────────────────────┘
```

---

## 20. Integración con Coveralls

Alternativa a Codecov:

```bash
# Generar y upload en un paso
cargo tarpaulin --coveralls $COVERALLS_TOKEN
```

### GitHub Actions con Coveralls

```yaml
coverage:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - uses: dtolnay/rust-toolchain@stable
    - run: cargo install cargo-tarpaulin
    - run: cargo tarpaulin --coveralls ${{ secrets.COVERALLS_TOKEN }}
```

### Comparación Codecov vs Coveralls

```
┌──────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto          │ Codecov              │ Coveralls            │
├──────────────────┼──────────────────────┼──────────────────────┤
│ Gratuito         │ Sí (open source)     │ Sí (open source)     │
│ Formato input    │ lcov, xml, json      │ Propio (tarpaulin    │
│                  │                      │ genera directamente) │
│ PR comments      │ Detallados           │ Más simples          │
│ Badges           │ ✓                    │ ✓                    │
│ GitHub App       │ ✓                    │ ✓                    │
│ Branch diff      │ ✓ (excelente)        │ ✓ (bueno)            │
│ Popularidad Rust │ Más popular          │ Popular              │
│ Flag support     │ ✓ (multi-flag)       │ Limitado             │
└──────────────────┴──────────────────────┴──────────────────────┘
```

---

## 21. Umbrales de cobertura: --fail-under

```bash
# Fallar si la cobertura es menor al 70%
cargo tarpaulin --fail-under 70

# Exit code:
# 0 → cobertura >= 70%
# 1 → cobertura < 70%
```

### Uso en CI

```bash
# En un script de CI
cargo tarpaulin --fail-under 75 --out xml || {
    echo "Coverage below 75%! Failing the build."
    exit 1
}
```

### Umbrales recomendados por tipo de proyecto

```
┌────────────────────────────┬───────────┬────────────────────────────┐
│ Tipo de proyecto           │ Umbral    │ Justificación              │
├────────────────────────────┼───────────┼────────────────────────────┤
│ Librería pública (crate)   │ 80-90%    │ Usuarios dependen de ella │
│ Microservicio              │ 70-85%    │ Balance esfuerzo/valor    │
│ CLI tool                   │ 60-75%    │ main() y I/O difíciles    │
│ Prototipo / MVP            │ 50-60%    │ Velocidad sobre cobertura │
│ Código de seguridad/crypto │ 90-95%    │ Errores tienen alto costo │
│ Código generado            │ Excluir   │ No tiene sentido medir    │
│ Infraestructura / glue     │ 40-60%    │ Mayormente wiring         │
└────────────────────────────┴───────────┴────────────────────────────┘
```

---

## 22. Modos de instrumentación: Ptrace vs Llvm

tarpaulin tiene dos engines de instrumentación:

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Ptrace engine (default en x86_64 Linux)                        │
│  ─────────────                                                  │
│  - Usa la syscall ptrace para interceptar la ejecución          │
│  - No modifica el binario                                       │
│  - Funciona con cualquier código Rust                           │
│  - Más lento (overhead de ptrace en cada breakpoint)            │
│  - Puede tener falsos positivos en optimizaciones               │
│  - Linux only, x86_64 mainly                                    │
│                                                                  │
│  cargo tarpaulin --engine ptrace                                │
│  cargo tarpaulin  # default                                     │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  LLVM engine                                                    │
│  ───────────                                                    │
│  - Usa LLVM source-based coverage instrumentation               │
│  - Compila con -C instrument-coverage                           │
│  - Más preciso (instrumentación a nivel de IR)                  │
│  - Más rápido (sin overhead de ptrace)                          │
│  - Requiere nightly o rustc reciente con llvm-tools             │
│  - Soporta branch coverage real                                 │
│                                                                  │
│  cargo tarpaulin --engine llvm                                  │
│                                                                  │
│  Requiere:                                                       │
│  rustup component add llvm-tools-preview                        │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Comparación

```
┌──────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto          │ Ptrace               │ LLVM                 │
├──────────────────┼──────────────────────┼──────────────────────┤
│ Precisión        │ Buena                │ Excelente            │
│ Velocidad        │ Más lento            │ Más rápido           │
│ Branch coverage  │ Estimada             │ Real (IR-based)      │
│ Setup            │ Ninguno extra        │ llvm-tools-preview   │
│ Compatibilidad   │ Todo Rust            │ Puede fallar con     │
│                  │                      │ proc macros complejas│
│ Plataforma       │ Linux x86_64         │ Linux x86_64         │
│ Debug info       │ Requerido (default)  │ Requerido            │
│ Recomendado para │ Uso general, CI      │ Precisión, branch cov│
└──────────────────┴──────────────────────┴──────────────────────┘
```

### Activar branch coverage

```bash
# Branch coverage con engine LLVM (más preciso)
cargo tarpaulin --engine llvm --branch

# Branch coverage con ptrace (estimado)
cargo tarpaulin --branch
```

---

## 23. Flags y opciones avanzadas

### Referencia rápida

```bash
# ── Output ────────────────────────────────────────────────
cargo tarpaulin --out html           # reporte HTML
cargo tarpaulin --out json           # reporte JSON
cargo tarpaulin --out lcov           # formato lcov
cargo tarpaulin --out xml            # Cobertura XML
cargo tarpaulin --out stdout         # solo terminal (default)
cargo tarpaulin --output-dir dir/    # directorio para reportes

# ── Filtros ───────────────────────────────────────────────
cargo tarpaulin --files "src/*.rs"          # incluir archivos
cargo tarpaulin --exclude-files "tests/*"   # excluir archivos
cargo tarpaulin --packages crate_name       # solo este crate
cargo tarpaulin --exclude pkg_name          # excluir crate
cargo tarpaulin --workspace                 # todo el workspace
cargo tarpaulin --lib                       # solo library tests
cargo tarpaulin --test test_name            # solo un test binary
cargo tarpaulin --ignore-tests              # no contar líneas de test

# ── Umbral ────────────────────────────────────────────────
cargo tarpaulin --fail-under 75             # falla si < 75%

# ── Engine ────────────────────────────────────────────────
cargo tarpaulin --engine ptrace             # ptrace (default)
cargo tarpaulin --engine llvm               # LLVM instrumentation
cargo tarpaulin --branch                    # branch coverage

# ── Ejecución ────────────────────────────────────────────
cargo tarpaulin --release                   # compilar en release
cargo tarpaulin --timeout 300               # timeout por test (seg)
cargo tarpaulin --jobs 4                    # threads de compilación
cargo tarpaulin --ignore-panics             # ignorar tests que panic
cargo tarpaulin --follow-exec               # seguir procesos hijo
cargo tarpaulin --force-clean               # limpiar antes de compilar

# ── Debugging ────────────────────────────────────────────
cargo tarpaulin -v                          # verbose
cargo tarpaulin -vv                         # muy verbose
cargo tarpaulin --debug                     # máximo detalle

# ── Servicios ────────────────────────────────────────────
cargo tarpaulin --coveralls TOKEN           # upload a Coveralls
cargo tarpaulin --ciserver github-actions   # indicar CI server

# ── Tests ─────────────────────────────────────────────────
cargo tarpaulin --run-types tests           # solo #[test]
cargo tarpaulin --run-types doctests        # solo doc tests
cargo tarpaulin --run-types tests doctests  # ambos
cargo tarpaulin -- --test-threads=1         # serial (después de --)
```

### Pasar flags a cargo test

Todo después de `--` se pasa a `cargo test`:

```bash
# Ejecutar solo tests que matchean un nombre
cargo tarpaulin -- --test-threads=1 test_auth

# Ejecutar en un solo thread (útil para tests con estado global)
cargo tarpaulin -- --test-threads=1

# Incluir tests ignorados
cargo tarpaulin -- --include-ignored
```

### --ignore-tests: no contar el código de test

```bash
# Por defecto, tarpaulin cuenta las líneas de #[cfg(test)] mod tests
# como parte del código cubrirle. Con --ignore-tests, las excluye.

cargo tarpaulin --ignore-tests
```

Esto cambia la métrica: si tienes 100 líneas de lógica y 50 de tests, sin `--ignore-tests` el denominador es 150, con `--ignore-tests` es 100.

---

## 24. Debugging: cuando tarpaulin reporta datos incorrectos

### Problema: cobertura 0% en código que sí se ejecuta

```
Causas comunes:
1. Optimizaciones inline eliminan el código
   → Solución: tarpaulin compila con debug por defecto, pero verifica:
     cargo tarpaulin --force-clean

2. El código está en una macro que genera código en otro lugar
   → Solución: la cobertura se reporta donde se define la macro,
     no donde se expande. Verificar el archivo correcto.

3. Problemas con async/await
   → Solución: usar --engine llvm que maneja mejor async

4. Código detrás de #[cfg(...)]
   → Solución: verificar que cfg se activa en el perfil de test
```

### Problema: líneas marcadas como "no cubribles"

```
tarpaulin no siempre puede instrumentar todas las líneas:

- Declaraciones de tipo (struct, enum) → no son ejecutables
- use statements → no son ejecutables
- Closures inline → a veces se marcan incorrectamente
- Match arms con patterns complejos → puede perder algunas

Solución: usar --engine llvm para mejor precisión
```

### Problema: tests pasan pero tarpaulin reporta fallo

```bash
# Los tests pueden tener un timeout diferente
cargo tarpaulin --timeout 300  # aumentar timeout a 5 minutos

# Los tests pueden necesitar un solo thread
cargo tarpaulin -- --test-threads=1

# Limpiar artefactos previos
cargo tarpaulin --force-clean
```

### Verificar con verbose

```bash
# Ver exactamente qué ejecuta tarpaulin
cargo tarpaulin -vv 2>&1 | head -50

# Ver qué archivos incluye
cargo tarpaulin --debug 2>&1 | grep "Instrumenting"
```

---

## 25. Comparación con C y Go

### C: gcov + lcov

```bash
# C: cobertura con GCC

# 1. Compilar con flags de cobertura
gcc -fprofile-arcs -ftest-coverage -o my_test my_test.c my_lib.c

# 2. Ejecutar
./my_test

# 3. Generar datos
gcov my_lib.c
# Genera my_lib.c.gcov con anotaciones de cobertura

# 4. Generar reporte HTML con lcov
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# Estructura del flujo:
# source.c → gcc -fprofile-arcs → executable → run → .gcda files
#                                                       ↓
#                                             gcov → .gcov files
#                                                       ↓
#                                              lcov → coverage.info
#                                                       ↓
#                                            genhtml → HTML report
```

```c
/* my_lib.c.gcov (output de gcov) */
        -:    1:// Función de ejemplo
        1:    2:int factorial(int n) {
        1:    3:    if (n <= 1) {
        1:    4:        return 1;
        -:    5:    }
    #####:    6:    return n * factorial(n - 1);
        -:    7:}
/* ##### = línea nunca ejecutada */
/* 1 = ejecutada 1 vez */
/* - = no es código ejecutable */
```

### Go: go test -cover (built-in)

```bash
# Go: cobertura integrada en el toolchain

# 1. Cobertura rápida en terminal
go test -cover ./...
# ok  my_project/pkg  0.005s  coverage: 82.3% of statements

# 2. Perfil de cobertura detallado
go test -coverprofile=coverage.out ./...

# 3. Ver en terminal qué líneas están cubiertas
go tool cover -func=coverage.out
# my_project/pkg/auth.go:10:    Login       100.0%
# my_project/pkg/auth.go:25:    Logout      75.0%
# total:                                     82.3%

# 4. Reporte HTML
go tool cover -html=coverage.out -o coverage.html

# 5. Cobertura con race detector
go test -cover -race ./...
```

```go
// Go: ver código anotado con cobertura
// go tool cover -html=coverage.out muestra:

// Verde: cubierto
func Login(user, pass string) (*Session, error) {
    if user == "" {           // cubierto (verde)
        return nil, ErrEmpty  // cubierto
    }
    // ...
}

// Rojo: no cubierto
func RecoverAccount(email string) error {
    // Esta función nunca se llamó en los tests
    return nil
}
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────┬──────────────────┬──────────────────┐
│ Aspecto              │ Rust (tarpaulin) │ Go (go test)     │ C (gcov/lcov)    │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Built-in             │ No (crate)       │ SÍ (integrado)   │ No (gcc flag +   │
│                      │                  │                  │ herramientas)    │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Comando              │ cargo tarpaulin  │ go test -cover   │ gcc -fprofile-   │
│                      │                  │                  │ arcs + gcov      │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Formato nativo       │ HTML, JSON,      │ coverprofile     │ .gcov, .gcda     │
│                      │ Lcov, XML        │                  │ (lcov para HTML) │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ HTML report          │ --out html       │ go tool cover    │ genhtml          │
│                      │                  │ -html            │ (externo)        │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Branch coverage      │ --branch         │ No nativo        │ gcov --branch    │
│                      │ (experimental)   │                  │                  │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Exclusiones          │ #[tarpaulin::    │ //go:nocover     │ #pragma GCC      │
│                      │ skip]            │ (1.22+)          │ diagnostic       │
│                      │ o config file    │                  │ (indirecto)      │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ CI integration       │ Codecov, Cover-  │ Codecov, Cover-  │ Codecov, Cover-  │
│                      │ alls             │ alls             │ alls             │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Umbral               │ --fail-under     │ -covermode=count │ lcov --fail-     │
│                      │                  │ + script         │ under-lines      │
├──────────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Facilidad de uso     │ Media            │ Excelente        │ Baja             │
│                      │ (instalar crate) │ (built-in)       │ (múltiples       │
│                      │                  │                  │ herramientas)    │
└──────────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

---

## 26. Errores comunes

### Error 1: "tarpaulin is not available on this platform"

```
$ cargo tarpaulin
error: tarpaulin currently only supports x86_64 linux

Causa: tarpaulin solo funciona en Linux x86_64.
Solución: usar cargo-llvm-cov en macOS/Windows.

  # macOS/Windows:
  cargo install cargo-llvm-cov
  cargo llvm-cov
```

### Error 2: "thread panicked" durante cobertura

```
Causa: un test que panic no siempre se maneja bien con ptrace.
Solución:
  cargo tarpaulin --ignore-panics
  # O usar engine LLVM:
  cargo tarpaulin --engine llvm
```

### Error 3: Cobertura inconsistente entre ejecuciones

```
Causa: tests con orden aleatorio o estado compartido.
Solución:
  cargo tarpaulin -- --test-threads=1
  # Ejecutar tests en un solo thread para consistencia
```

### Error 4: Timeout en tests largos

```
$ cargo tarpaulin
Error: test timed out after 60 seconds

Solución:
  cargo tarpaulin --timeout 300  # 5 minutos por test
```

### Error 5: Cobertura muy baja por incluir tests en la métrica

```
Sin --ignore-tests, el código de los tests mismos
se cuenta como "cubrirle". Si un test helper no se llama,
baja la cobertura.

Solución:
  cargo tarpaulin --ignore-tests
```

### Error 6: Archivos generados inflan/distorsionan la métrica

```
Archivos como build.rs, bindings generados, o proc macros
pueden distorsionar la cobertura total.

Solución:
  cargo tarpaulin --exclude-files "src/generated/*" --exclude-files "build.rs"
```

### Error 7: Doc-tests no se incluyen

```
Por defecto, tarpaulin solo ejecuta tests unitarios.
Para incluir doc-tests:

  cargo tarpaulin --run-types tests doctests
```

### Error 8: "Error: Failed to report coverage"

```
Causa: problema de permisos con ptrace (en containers Docker o CI).
Solución:
  # Opción 1: usar --engine llvm
  cargo tarpaulin --engine llvm

  # Opción 2: en Docker, ejecutar con:
  docker run --security-opt seccomp=unconfined ...

  # Opción 3: en CI, verificar que ptrace está permitido
```

---

## 27. Ejemplo completo: proyecto con cobertura configurada

### Estructura del proyecto

```
calculator/
├── Cargo.toml
├── .tarpaulin.toml
├── .github/
│   └── workflows/
│       └── coverage.yml
├── src/
│   ├── lib.rs
│   ├── math.rs
│   ├── parser.rs
│   └── error.rs
└── tests/
    └── integration.rs
```

### Cargo.toml

```toml
[package]
name = "calculator"
version = "0.1.0"
edition = "2021"

[dev-dependencies]
# sin dependencias de testing externas para este ejemplo
```

### .tarpaulin.toml

```toml
[default]
out = ["html", "lcov"]
output-dir = "coverage"
fail-under = 80.0
ignore-tests = true
exclude-files = ["tests/*"]
timeout = "120"
```

### src/error.rs

```rust
#[derive(Debug, PartialEq)]
pub enum CalcError {
    DivisionByZero,
    InvalidExpression(String),
    Overflow,
    UnknownOperator(char),
}

impl std::fmt::Display for CalcError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CalcError::DivisionByZero => write!(f, "division by zero"),
            CalcError::InvalidExpression(s) => write!(f, "invalid expression: {}", s),
            CalcError::Overflow => write!(f, "numeric overflow"),
            CalcError::UnknownOperator(c) => write!(f, "unknown operator: {}", c),
        }
    }
}
```

### src/math.rs

```rust
use crate::error::CalcError;

pub fn add(a: f64, b: f64) -> f64 {
    a + b
}

pub fn subtract(a: f64, b: f64) -> f64 {
    a - b
}

pub fn multiply(a: f64, b: f64) -> f64 {
    a * b
}

pub fn divide(a: f64, b: f64) -> Result<f64, CalcError> {
    if b == 0.0 {
        Err(CalcError::DivisionByZero)
    } else {
        Ok(a / b)
    }
}

pub fn power(base: f64, exp: u32) -> f64 {
    base.powi(exp as i32)
}

pub fn factorial(n: u64) -> Result<u64, CalcError> {
    if n > 20 {
        return Err(CalcError::Overflow);
    }
    Ok((1..=n).product())
}

pub fn modulo(a: f64, b: f64) -> Result<f64, CalcError> {
    if b == 0.0 {
        Err(CalcError::DivisionByZero)
    } else {
        Ok(a % b)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(2.0, 3.0), 5.0);
        assert_eq!(add(-1.0, 1.0), 0.0);
        assert_eq!(add(0.0, 0.0), 0.0);
    }

    #[test]
    fn test_subtract() {
        assert_eq!(subtract(5.0, 3.0), 2.0);
        assert_eq!(subtract(3.0, 5.0), -2.0);
    }

    #[test]
    fn test_multiply() {
        assert_eq!(multiply(3.0, 4.0), 12.0);
        assert_eq!(multiply(-2.0, 3.0), -6.0);
        assert_eq!(multiply(0.0, 999.0), 0.0);
    }

    #[test]
    fn test_divide_ok() {
        assert_eq!(divide(10.0, 2.0).unwrap(), 5.0);
        assert_eq!(divide(7.0, 2.0).unwrap(), 3.5);
    }

    #[test]
    fn test_divide_by_zero() {
        assert_eq!(divide(10.0, 0.0), Err(CalcError::DivisionByZero));
    }

    #[test]
    fn test_power() {
        assert_eq!(power(2.0, 10), 1024.0);
        assert_eq!(power(3.0, 0), 1.0);
        assert_eq!(power(5.0, 1), 5.0);
    }

    #[test]
    fn test_factorial() {
        assert_eq!(factorial(0).unwrap(), 1);
        assert_eq!(factorial(1).unwrap(), 1);
        assert_eq!(factorial(5).unwrap(), 120);
        assert_eq!(factorial(10).unwrap(), 3_628_800);
    }

    #[test]
    fn test_factorial_overflow() {
        assert_eq!(factorial(21), Err(CalcError::Overflow));
    }

    #[test]
    fn test_modulo() {
        assert_eq!(modulo(10.0, 3.0).unwrap(), 1.0);
        assert_eq!(modulo(10.0, 0.0), Err(CalcError::DivisionByZero));
    }
}
```

### src/parser.rs

```rust
use crate::error::CalcError;

#[derive(Debug, PartialEq)]
pub enum Token {
    Number(f64),
    Operator(char),
    LeftParen,
    RightParen,
}

pub fn tokenize(input: &str) -> Result<Vec<Token>, CalcError> {
    let mut tokens = Vec::new();
    let mut chars = input.chars().peekable();

    while let Some(&c) = chars.peek() {
        match c {
            ' ' | '\t' => { chars.next(); }
            '0'..='9' | '.' => {
                let mut num_str = String::new();
                while let Some(&d) = chars.peek() {
                    if d.is_ascii_digit() || d == '.' {
                        num_str.push(d);
                        chars.next();
                    } else {
                        break;
                    }
                }
                let num: f64 = num_str.parse()
                    .map_err(|_| CalcError::InvalidExpression(
                        format!("invalid number: {}", num_str)
                    ))?;
                tokens.push(Token::Number(num));
            }
            '+' | '-' | '*' | '/' | '%' | '^' => {
                tokens.push(Token::Operator(c));
                chars.next();
            }
            '(' => { tokens.push(Token::LeftParen); chars.next(); }
            ')' => { tokens.push(Token::RightParen); chars.next(); }
            _ => return Err(CalcError::UnknownOperator(c)),
        }
    }

    Ok(tokens)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tokenize_simple_expression() {
        let tokens = tokenize("2 + 3").unwrap();
        assert_eq!(tokens, vec![
            Token::Number(2.0),
            Token::Operator('+'),
            Token::Number(3.0),
        ]);
    }

    #[test]
    fn tokenize_with_parens() {
        let tokens = tokenize("(1 + 2) * 3").unwrap();
        assert_eq!(tokens.len(), 7);
        assert_eq!(tokens[0], Token::LeftParen);
        assert_eq!(tokens[6], Token::Number(3.0));
    }

    #[test]
    fn tokenize_decimal() {
        let tokens = tokenize("3.14").unwrap();
        assert_eq!(tokens, vec![Token::Number(3.14)]);
    }

    #[test]
    fn tokenize_unknown_char() {
        let result = tokenize("2 @ 3");
        assert_eq!(result, Err(CalcError::UnknownOperator('@')));
    }

    #[test]
    fn tokenize_all_operators() {
        let tokens = tokenize("+ - * / % ^").unwrap();
        assert_eq!(tokens.len(), 6);
    }
}
```

### src/lib.rs

```rust
pub mod error;
pub mod math;
pub mod parser;

use error::CalcError;
use parser::{Token, tokenize};

pub fn evaluate(expression: &str) -> Result<f64, CalcError> {
    let tokens = tokenize(expression)?;

    if tokens.is_empty() {
        return Err(CalcError::InvalidExpression("empty expression".into()));
    }

    // Evaluación simple: solo "number operator number"
    if tokens.len() == 3 {
        if let (Token::Number(a), Token::Operator(op), Token::Number(b)) =
            (&tokens[0], &tokens[1], &tokens[2])
        {
            return apply_operator(*a, *op, *b);
        }
    }

    // Solo un número
    if tokens.len() == 1 {
        if let Token::Number(n) = tokens[0] {
            return Ok(n);
        }
    }

    Err(CalcError::InvalidExpression(
        "only simple expressions (a op b) are supported".into()
    ))
}

fn apply_operator(a: f64, op: char, b: f64) -> Result<f64, CalcError> {
    match op {
        '+' => Ok(math::add(a, b)),
        '-' => Ok(math::subtract(a, b)),
        '*' => Ok(math::multiply(a, b)),
        '/' => math::divide(a, b),
        '%' => math::modulo(a, b),
        '^' => Ok(math::power(a, b as u32)),
        _ => Err(CalcError::UnknownOperator(op)),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn evaluate_addition() {
        assert_eq!(evaluate("2 + 3").unwrap(), 5.0);
    }

    #[test]
    fn evaluate_subtraction() {
        assert_eq!(evaluate("10 - 4").unwrap(), 6.0);
    }

    #[test]
    fn evaluate_multiplication() {
        assert_eq!(evaluate("3 * 7").unwrap(), 21.0);
    }

    #[test]
    fn evaluate_division() {
        assert_eq!(evaluate("10 / 4").unwrap(), 2.5);
    }

    #[test]
    fn evaluate_division_by_zero() {
        assert_eq!(evaluate("5 / 0"), Err(CalcError::DivisionByZero));
    }

    #[test]
    fn evaluate_single_number() {
        assert_eq!(evaluate("42").unwrap(), 42.0);
    }

    #[test]
    fn evaluate_empty() {
        assert!(evaluate("").is_err());
    }

    #[test]
    fn evaluate_power() {
        assert_eq!(evaluate("2 ^ 10").unwrap(), 1024.0);
    }

    #[test]
    fn evaluate_modulo() {
        assert_eq!(evaluate("10 % 3").unwrap(), 1.0);
    }
}
```

### Ejecutar cobertura

```bash
$ cargo tarpaulin

running 23 tests
test math::tests::test_add ... ok
test math::tests::test_subtract ... ok
test math::tests::test_multiply ... ok
test math::tests::test_divide_ok ... ok
test math::tests::test_divide_by_zero ... ok
test math::tests::test_power ... ok
test math::tests::test_factorial ... ok
test math::tests::test_factorial_overflow ... ok
test math::tests::test_modulo ... ok
test parser::tests::tokenize_simple_expression ... ok
test parser::tests::tokenize_with_parens ... ok
test parser::tests::tokenize_decimal ... ok
test parser::tests::tokenize_unknown_char ... ok
test parser::tests::tokenize_all_operators ... ok
test tests::evaluate_addition ... ok
test tests::evaluate_subtraction ... ok
test tests::evaluate_multiplication ... ok
test tests::evaluate_division ... ok
test tests::evaluate_division_by_zero ... ok
test tests::evaluate_single_number ... ok
test tests::evaluate_empty ... ok
test tests::evaluate_power ... ok
test tests::evaluate_modulo ... ok

test result: ok. 23 passed; 0 failed; 0 ignored

|| Tested/Total Lines:
|| src/error.rs: 5/5 (100.00%)
|| src/lib.rs: 18/20 (90.00%)
|| src/math.rs: 18/18 (100.00%)
|| src/parser.rs: 25/26 (96.15%)
||
95.65% coverage, 66/69 lines covered
```

---

## 28. Programa de práctica

### Proyecto: `text_stats` — analizador de texto con cobertura

Implementa un analizador de texto y configura tarpaulin para medir y mejorar la cobertura.

### src/lib.rs

```rust
pub mod analyzer;
pub mod formatter;

pub use analyzer::TextStats;
pub use formatter::format_report;
```

### src/analyzer.rs

```rust
use std::collections::HashMap;

#[derive(Debug, Clone, PartialEq)]
pub struct TextStats {
    pub char_count: usize,
    pub word_count: usize,
    pub line_count: usize,
    pub sentence_count: usize,
    pub paragraph_count: usize,
    pub avg_word_length: f64,
    pub most_common_words: Vec<(String, usize)>,
    pub unique_word_count: usize,
    pub reading_time_secs: u64,
}

pub fn analyze(text: &str) -> TextStats {
    let char_count = text.chars().count();
    let words = extract_words(text);
    let word_count = words.len();
    let line_count = if text.is_empty() { 0 } else { text.lines().count() };
    let sentence_count = count_sentences(text);
    let paragraph_count = count_paragraphs(text);
    let avg_word_length = if word_count > 0 {
        words.iter().map(|w| w.len() as f64).sum::<f64>() / word_count as f64
    } else {
        0.0
    };
    let word_freq = word_frequency(&words);
    let unique_word_count = word_freq.len();
    let most_common_words = top_n_words(&word_freq, 5);
    let reading_time_secs = (word_count as f64 / 200.0 * 60.0) as u64;

    TextStats {
        char_count,
        word_count,
        line_count,
        sentence_count,
        paragraph_count,
        avg_word_length,
        most_common_words,
        unique_word_count,
        reading_time_secs,
    }
}

fn extract_words(text: &str) -> Vec<String> {
    text.split_whitespace()
        .map(|w| w.trim_matches(|c: char| !c.is_alphanumeric()).to_lowercase())
        .filter(|w| !w.is_empty())
        .collect()
}

fn count_sentences(text: &str) -> usize {
    text.chars()
        .filter(|c| *c == '.' || *c == '!' || *c == '?')
        .count()
}

fn count_paragraphs(text: &str) -> usize {
    if text.trim().is_empty() {
        return 0;
    }
    text.split("\n\n")
        .filter(|p| !p.trim().is_empty())
        .count()
}

fn word_frequency(words: &[String]) -> HashMap<String, usize> {
    let mut freq = HashMap::new();
    for word in words {
        *freq.entry(word.clone()).or_insert(0) += 1;
    }
    freq
}

fn top_n_words(freq: &HashMap<String, usize>, n: usize) -> Vec<(String, usize)> {
    let mut sorted: Vec<(String, usize)> = freq.iter()
        .map(|(k, v)| (k.clone(), *v))
        .collect();
    sorted.sort_by(|a, b| b.1.cmp(&a.1));
    sorted.truncate(n);
    sorted
}
```

### src/formatter.rs

```rust
use crate::TextStats;

pub fn format_report(stats: &TextStats) -> String {
    let mut report = String::new();

    report.push_str("=== Text Analysis Report ===\n\n");
    report.push_str(&format!("Characters:    {}\n", stats.char_count));
    report.push_str(&format!("Words:         {}\n", stats.word_count));
    report.push_str(&format!("Lines:         {}\n", stats.line_count));
    report.push_str(&format!("Sentences:     {}\n", stats.sentence_count));
    report.push_str(&format!("Paragraphs:    {}\n", stats.paragraph_count));
    report.push_str(&format!("Unique words:  {}\n", stats.unique_word_count));
    report.push_str(&format!("Avg word len:  {:.1}\n", stats.avg_word_length));
    report.push_str(&format!("Reading time:  {}s\n", stats.reading_time_secs));

    if !stats.most_common_words.is_empty() {
        report.push_str("\nMost common words:\n");
        for (word, count) in &stats.most_common_words {
            report.push_str(&format!("  {:15} {}\n", word, count));
        }
    }

    report
}
```

### Tareas para el estudiante

1. **Escribir tests** para `analyzer.rs` y `formatter.rs` intentando alcanzar al menos 85% de cobertura.

2. **Configurar tarpaulin** con `.tarpaulin.toml`:
   - Output HTML y lcov
   - Umbral 80%
   - Excluir tests/ del conteo
   - Output en directorio `coverage/`

3. **Ejecutar `cargo tarpaulin`** y analizar el reporte HTML. Identificar las líneas no cubiertas.

4. **Mejorar la cobertura** escribiendo tests adicionales para los edge cases encontrados.

5. **Verificar con `--fail-under 85`** que el umbral se cumple.

6. **Añadir al `.gitignore`**: `coverage/`, `tarpaulin-report.*`, `lcov.info`, `cobertura.xml`.

---

## 29. Ejercicios

### Ejercicio 1: Analizar cobertura de un proyecto existente

**Objetivo**: Practicar la interpretación de reportes de cobertura.

**Tareas**:

**a)** Crea un proyecto con estas funciones:

```rust
pub fn classify_triangle(a: f64, b: f64, c: f64) -> Result<&'static str, &'static str> {
    if a <= 0.0 || b <= 0.0 || c <= 0.0 {
        return Err("sides must be positive");
    }
    if a + b <= c || a + c <= b || b + c <= a {
        return Err("invalid triangle");
    }
    if a == b && b == c {
        Ok("equilateral")
    } else if a == b || b == c || a == c {
        Ok("isosceles")
    } else {
        Ok("scalene")
    }
}
```

**b)** Escribe solo UN test: `classify_triangle(3.0, 4.0, 5.0)`. Ejecuta `cargo tarpaulin --out html` y analiza qué ramas NO están cubiertas.

**c)** Escribe tests adicionales hasta alcanzar 100% de line coverage. ¿Cuántos tests necesitas como mínimo?

**d)** ¿100% de line coverage garantiza que la función es correcta? ¿Qué input podría causar un bug que los tests no detectan? (Pista: floating point)

---

### Ejercicio 2: Configurar tarpaulin para un workspace

**Objetivo**: Practicar la configuración multi-crate.

**Tareas**:

**a)** Crea un workspace con 3 crates:
- `core` (librería con lógica de negocio)
- `api` (librería con handlers HTTP simulados)
- `cli` (binario con main)

**b)** Escribe tests en `core` y `api`. No escribas tests para `cli`.

**c)** Configura `.tarpaulin.toml` para:
- Excluir `cli`
- Medir `core` y `api`
- Umbral 75%
- Output HTML en `coverage/`

**d)** Ejecuta y verifica que la cobertura total cumple el umbral.

---

### Ejercicio 3: Integrar cobertura en CI

**Objetivo**: Practicar la integración con GitHub Actions.

**Tareas**:

**a)** Escribe un workflow `.github/workflows/coverage.yml` que:
- Se ejecute en push a main y en PRs
- Instale tarpaulin
- Ejecute con `--fail-under 70`
- Genere lcov.info como artefacto
- (Opcional) Upload a Codecov

**b)** ¿Qué pasa si un PR baja la cobertura de 80% a 65%? ¿Cómo se comporta el workflow?

**c)** Añade una badge de cobertura al README del proyecto.

---

### Ejercicio 4: Mejorar cobertura estratégicamente

**Objetivo**: Practicar la mejora de cobertura enfocada en valor.

**Contexto**: dado un módulo con 55% de cobertura y estas funciones no testeadas:

```rust
pub fn parse_csv_header(line: &str) -> Vec<String> { /* ... */ }     // crítico
pub fn format_currency(cents: u64) -> String { /* ... */ }            // útil
pub fn log_startup_banner() { /* ... */ }                              // trivial
fn internal_cache_key(prefix: &str, id: u64) -> String { /* ... */ } // interno
impl Display for AppError { /* ... */ }                                // boilerplate
```

**Tareas**:

**a)** Ordena estas funciones por prioridad de testing (justifica).

**b)** ¿Cuáles excluirías con `#[tarpaulin::skip]`? ¿Por qué?

**c)** Escribe tests para las 2 funciones de mayor prioridad.

**d)** ¿Cuánto subiría la cobertura? ¿Vale la pena llegar al 100%?

---

## Navegación

- **Anterior**: [S04/T04 - Test doubles sin crates externos](../../S04_Mocking_en_Rust/T04_Test_doubles_sin_crates/README.md)
- **Siguiente**: [T02 - llvm-cov](../T02_Llvm_cov/README.md)

---

> **Sección 5: Cobertura en Rust** — Tópico 1 de 3 completado
>
> - T01: cargo-tarpaulin — instalación, cargo tarpaulin, output HTML/JSON, exclusiones ✓
> - T02: llvm-cov (siguiente)
> - T03: Cobertura realista
