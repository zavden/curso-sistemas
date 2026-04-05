# T01 — Unit tests con `#[test]`: el sistema de testing integrado de Rust

> **Bloque 17 — Testing e Ingeniería de Software → C02 — Testing en Rust → S01 — Testing Built-in → T01**

---

## Índice

1. [Testing integrado: la diferencia fundamental con C](#1-testing-integrado-la-diferencia-fundamental-con-c)
2. [El atributo #[test]](#2-el-atributo-test)
3. [Ejecutar tests con cargo test](#3-ejecutar-tests-con-cargo-test)
4. [assert!, assert_eq!, assert_ne!: las tres macros fundamentales](#4-assert-assert_eq-assert_ne-las-tres-macros-fundamentales)
5. [Mensajes custom en assertions](#5-mensajes-custom-en-assertions)
6. [El módulo mod tests y #[cfg(test)]](#6-el-módulo-mod-tests-y-cfgtest)
7. [Anatomía completa de un test en Rust](#7-anatomía-completa-de-un-test-en-rust)
8. [Tests de funciones privadas](#8-tests-de-funciones-privadas)
9. [#[ignore]: tests que no corren por defecto](#9-ignore-tests-que-no-corren-por-defecto)
10. [Salida de cargo test: leer los resultados](#10-salida-de-cargo-test-leer-los-resultados)
11. [Captura de stdout: --nocapture](#11-captura-de-stdout---nocapture)
12. [Filtrar tests por nombre](#12-filtrar-tests-por-nombre)
13. [Ejecución en paralelo y --test-threads](#13-ejecución-en-paralelo-y---test-threads)
14. [debug_assert! y cfg(debug_assertions)](#14-debug_assert-y-cfgdebug_assertions)
15. [Patrones de testing en Rust](#15-patrones-de-testing-en-rust)
16. [Comparación con C: assert.h, Unity, CMocka](#16-comparación-con-c-asserth-unity-cmocka)
17. [Errores comunes](#17-errores-comunes)
18. [Ejemplo completo: biblioteca de fracciones](#18-ejemplo-completo-biblioteca-de-fracciones)
19. [Programa de práctica](#19-programa-de-práctica)
20. [Ejercicios](#20-ejercicios)

---

## 1. Testing integrado: la diferencia fundamental con C

En C, el testing es **externo** al lenguaje: necesitas elegir un framework (Unity, CMocka, Check, assert.h), configurar compilación separada, y gestionar la ejecución manualmente. En Rust, el testing es **parte del lenguaje y del toolchain**.

### Qué incluye Rust de serie

```
Característica                  C                              Rust
──────────────────────────────────────────────────────────────────────────────
Atributo para marcar tests      No existe                      #[test]
Macros de aserción              assert.h (muy básico)          assert!, assert_eq!, assert_ne!
Runner de tests                 Externo (framework)            cargo test (integrado)
Compilación condicional tests   Manual (Makefile)              #[cfg(test)] automático
Test discovery                  Manual o framework             Automático (escanea #[test])
Resultado por test              Depende del framework          PASS/FAIL estándar
Ejecución paralela              Manual (fork en Check)         Automática (threads)
Captura de stdout               No estándar                    Automática (--nocapture)
Filtrado de tests               Framework-dependent            cargo test nombre_filtro
```

### Flujo de trabajo en Rust

```
  ┌────────────────────────────────────────────────────────────────────┐
  │                     TESTING EN RUST                                │
  │                                                                    │
  │  1. Escribir test en el MISMO archivo que el código               │
  │     ┌──────────────────────────────────────────────────────────┐  │
  │     │  fn add(a: i32, b: i32) -> i32 { a + b }                │  │
  │     │                                                          │  │
  │     │  #[cfg(test)]                                            │  │
  │     │  mod tests {                                             │  │
  │     │      use super::*;                                       │  │
  │     │      #[test]                                             │  │
  │     │      fn test_add() {                                     │  │
  │     │          assert_eq!(add(2, 3), 5);                       │  │
  │     │      }                                                   │  │
  │     │  }                                                       │  │
  │     └──────────────────────────────────────────────────────────┘  │
  │                                                                    │
  │  2. Ejecutar con un comando                                        │
  │     $ cargo test                                                   │
  │                                                                    │
  │  3. Ver resultados                                                 │
  │     running 1 test                                                 │
  │     test tests::test_add ... ok                                    │
  │     test result: ok. 1 passed; 0 failed; 0 ignored                │
  │                                                                    │
  │  No hay:                                                           │
  │    - Makefile especial                                             │
  │    - Framework externo                                             │
  │    - Binario de test separado                                      │
  │    - Configuración de runner                                       │
  │                                                                    │
  └────────────────────────────────────────────────────────────────────┘
```

### Qué compila cargo test

Cuando ejecutas `cargo test`, Cargo:

1. Compila tu crate con `cfg(test)` activado
2. Incluye el código dentro de `#[cfg(test)]` que normalmente se excluye
3. Genera un binario de test con un `main()` que descubre y ejecuta todas las funciones marcadas con `#[test]`
4. Ejecuta ese binario

```
  cargo test
      │
      ▼
  rustc --test src/lib.rs     ← Compila con el test harness
      │
      ▼
  target/debug/deps/mylib-xxxx   ← Binario de test generado
      │
      ▼
  Ejecuta el binario ← El harness descubre #[test] y los corre
      │
      ▼
  Resultados en stdout
```

---

## 2. El atributo `#[test]`

### Sintaxis básica

```rust
#[test]
fn nombre_del_test() {
    // Código del test
    // Si la función retorna sin panic → PASS
    // Si la función hace panic → FAIL
}
```

### Reglas

```
Regla                                     Detalle
──────────────────────────────────────────────────────────────────────────
La función debe tener #[test]             Sin él, no se detecta como test
La función no puede tener argumentos      fn test() — sin parámetros
La función puede retornar ()              fn test() { ... }
La función puede retornar Result<(), E>   fn test() -> Result<(), String> (ver T03)
Un panic = FAIL                           assert! que falla → panic → test falla
Sin panic = PASS                          La función retorna normalmente → test pasa
El nombre es libre                        Convención: test_lo_que_verifica
```

### Ejemplo mínimo

```rust
// src/lib.rs

pub fn is_even(n: i32) -> bool {
    n % 2 == 0
}

#[test]
fn test_is_even() {
    assert!(is_even(4));
    assert!(!is_even(7));
}
```

```bash
$ cargo test
   Compiling mylib v0.1.0
    Finished test [unoptimized + debuginfo]
     Running unittests src/lib.rs

running 1 test
test test_is_even ... ok

test result: ok. 1 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

### Múltiples tests

```rust
pub fn abs(n: i32) -> i32 {
    if n < 0 { -n } else { n }
}

#[test]
fn test_abs_positive() {
    assert_eq!(abs(5), 5);
}

#[test]
fn test_abs_negative() {
    assert_eq!(abs(-3), 3);
}

#[test]
fn test_abs_zero() {
    assert_eq!(abs(0), 0);
}
```

Cada `#[test]` es un test independiente. Si uno falla, los demás siguen ejecutándose (a diferencia de `assert()` en C que termina el proceso).

---

## 3. Ejecutar tests con `cargo test`

### Comandos principales

```bash
# Ejecutar TODOS los tests (unit, integration, doc)
cargo test

# Solo tests unitarios (lib)
cargo test --lib

# Solo un test específico por nombre (o substring)
cargo test test_abs_positive

# Todos los tests que contengan "abs" en el nombre
cargo test abs

# Tests en modo release (optimizado)
cargo test --release

# Tests con output visible (no capturado)
cargo test -- --nocapture

# Ejecutar también los #[ignore]
cargo test -- --ignored

# Solo los #[ignore]
cargo test -- --ignored --exact

# Tests en un solo thread (secuencial)
cargo test -- --test-threads=1

# Verbose: mostrar nombres de todos los tests
cargo test -- --format=terse   # Compacto (default)
cargo test -- --format=pretty  # Nombres completos (nightly)

# Lista de tests sin ejecutar
cargo test -- --list
```

### Anatomía del comando

```
cargo test [OPTIONS] [TESTNAME] [-- TEST_OPTIONS]
           │                │       │
           │                │       └── Opciones para el binario de test
           │                │           (después del --)
           │                └── Filtro de nombre (substring match)
           └── Opciones para Cargo (--lib, --release, etc.)
```

La separación `--` es importante:

```bash
# Esto es una opción de Cargo:
cargo test --lib

# Esto es una opción del test runner:
cargo test -- --nocapture

# Combinación:
cargo test --lib -- --nocapture --test-threads=1
```

### Qué ejecuta cargo test

```
cargo test ejecuta 3 tipos de tests:

1. Unit tests:      Tests en src/ marcados con #[test]
                    Compilados como parte del crate
                    Acceden a funciones privadas

2. Integration tests: Archivos en tests/
                      Cada uno es un crate separado
                      Solo acceden a la API pública
                      (Ver S02/T01)

3. Doc tests:       Bloques ```rust en doc comments
                    Se compilan y ejecutan
                    (Ver S02/T02)
```

```bash
$ cargo test
   Compiling mylib v0.1.0

     Running unittests src/lib.rs (target/debug/deps/mylib-abc123)
running 3 tests
test tests::test_abs_positive ... ok       ← Unit tests
test tests::test_abs_negative ... ok
test tests::test_abs_zero ... ok
test result: ok. 3 passed

     Running tests/integration.rs (target/debug/deps/integration-def456)
running 1 test
test test_public_api ... ok                ← Integration test
test result: ok. 1 passed

   Doc-tests mylib
running 2 tests
test src/lib.rs - abs (line 5) ... ok      ← Doc tests
test src/lib.rs - is_even (line 15) ... ok
test result: ok. 2 passed
```

---

## 4. `assert!`, `assert_eq!`, `assert_ne!`: las tres macros fundamentales

### `assert!(expression)`

Verifica que la expresión es `true`. Si es `false`, hace panic con un mensaje indicando la expresión que falló.

```rust
#[test]
fn test_assert() {
    let x = 5;
    assert!(x > 0);           // Pasa
    assert!(x > 10);          // Falla: panic!
}
```

Mensaje de error:

```
thread 'test_assert' panicked at 'assertion failed: x > 10'
```

### `assert_eq!(left, right)`

Verifica que `left == right`. Si no son iguales, hace panic mostrando **ambos valores**.

```rust
#[test]
fn test_assert_eq() {
    let result = 2 + 3;
    assert_eq!(result, 5);     // Pasa
    assert_eq!(result, 6);     // Falla: muestra ambos valores
}
```

Mensaje de error:

```
thread 'test_assert_eq' panicked at 'assertion `left == right` failed
  left: 5
 right: 6'
```

La ventaja sobre `assert!(a == b)` es que `assert_eq!` **muestra los valores** cuando falla. Con `assert!(a == b)` solo verías "assertion failed: a == b" sin saber qué valores tenían `a` y `b`.

### `assert_ne!(left, right)`

Verifica que `left != right`. Si son iguales, hace panic mostrando los valores.

```rust
#[test]
fn test_assert_ne() {
    let a = "hello";
    let b = "world";
    assert_ne!(a, b);         // Pasa: son diferentes
    assert_ne!(a, "hello");   // Falla: son iguales
}
```

Mensaje de error:

```
thread 'test_assert_ne' panicked at 'assertion `left != right` failed
  left: "hello"
 right: "hello"'
```

### Requisitos de tipos

```
Macro           Requisitos del tipo
──────────────────────────────────────────────────────────────────────
assert!(expr)   expr debe implementar bool (ser booleana)
assert_eq!(a,b) a y b deben implementar PartialEq + Debug
assert_ne!(a,b) a y b deben implementar PartialEq + Debug
```

`PartialEq` es necesario para la comparación, y `Debug` es necesario para mostrar los valores cuando falla (usa `{:?}` internamente).

```rust
// Esto NO compila: MyStruct no implementa PartialEq ni Debug
struct MyStruct {
    value: i32,
}

#[test]
fn test_my_struct() {
    let a = MyStruct { value: 5 };
    let b = MyStruct { value: 5 };
    assert_eq!(a, b);  // ERROR: PartialEq not implemented for MyStruct
}

// Solución: derivar los traits necesarios
#[derive(Debug, PartialEq)]
struct MyStruct {
    value: i32,
}

#[test]
fn test_my_struct() {
    let a = MyStruct { value: 5 };
    let b = MyStruct { value: 5 };
    assert_eq!(a, b);  // OK: compara con PartialEq, muestra con Debug
}
```

### Tabla de uso

```
Situación                              Macro recomendada
──────────────────────────────────────────────────────────────────────
Verificar condición booleana           assert!(condition)
Verificar valor esperado               assert_eq!(actual, expected)
Verificar que dos valores difieren     assert_ne!(a, b)
Verificar que es Some/Ok/Err           assert!(result.is_ok())
Verificar contenido de Option          assert_eq!(opt, Some(42))
Verificar contenido de Result          assert_eq!(res, Ok(42))
Verificar string contiene              assert!(s.contains("hello"))
Verificar longitud                     assert_eq!(v.len(), 3)
Verificar rango                        assert!(x > 0 && x < 100)
Verificar patrón con matches!          assert!(matches!(val, Enum::A(_)))
Verificar float con tolerancia         assert!((a - b).abs() < 1e-9)
```

### assert con floats: cuidado con igualdad

```rust
#[test]
fn test_float_wrong() {
    let result = 0.1 + 0.2;
    assert_eq!(result, 0.3);  // FALLA: 0.30000000000000004 != 0.3
}

#[test]
fn test_float_correct() {
    let result = 0.1 + 0.2;
    let expected = 0.3;
    let epsilon = 1e-10;
    assert!(
        (result - expected).abs() < epsilon,
        "Expected {expected} ± {epsilon}, got {result}"
    );
}
```

Para comparaciones de floats frecuentes, puedes crear un helper:

```rust
#[cfg(test)]
fn assert_float_eq(actual: f64, expected: f64, epsilon: f64) {
    assert!(
        (actual - expected).abs() < epsilon,
        "Float comparison failed:\n  actual:   {actual}\n  expected: {expected}\n  diff:     {}\n  epsilon:  {epsilon}",
        (actual - expected).abs()
    );
}

#[test]
fn test_with_helper() {
    assert_float_eq(0.1 + 0.2, 0.3, 1e-10);
}
```

---

## 5. Mensajes custom en assertions

Las tres macros aceptan un mensaje custom como argumento adicional, usando la misma sintaxis que `format!()`.

### Sintaxis

```rust
assert!(condition, "mensaje");
assert!(condition, "formato {} con {}", var1, var2);
assert_eq!(a, b, "mensaje");
assert_eq!(a, b, "esperaba {} pero obtuve {}", expected, actual);
assert_ne!(a, b, "mensaje");
```

### Ejemplos

```rust
#[test]
fn test_with_messages() {
    let password = "abc";
    let min_len = 8;

    assert!(
        password.len() >= min_len,
        "Password '{}' is too short: {} chars, minimum is {}",
        password, password.len(), min_len
    );
}
```

Error producido:

```
thread 'test_with_messages' panicked at
  'Password 'abc' is too short: 3 chars, minimum is 8'
```

### Mensajes en assert_eq!

El mensaje custom se añade **después** de la información estándar de left/right:

```rust
#[test]
fn test_tax() {
    let income = 50_000.0;
    let tax = calculate_tax(income);

    assert_eq!(
        tax, 10_000.0,
        "Tax for income {income} should be 10000"
    );
}
```

Error producido:

```
thread 'test_tax' panicked at 'assertion `left == right` failed:
  Tax for income 50000 should be 10000
  left: 8000.0
 right: 10000.0'
```

### Cuándo usar mensajes custom

```
Situación                                    Mensaje custom útil?
──────────────────────────────────────────────────────────────────────
assert_eq!(add(2,3), 5)                     No — el error ya es claro
assert!(password.len() >= 8)                Sí — "¿qué password? ¿cuánto mide?"
assert!(is_valid(input))                    Sí — "¿qué input falló?"
Test en un loop con datos                   Sí — "¿en qué iteración falló?"
assert_eq!(result, expected) con cálculos   Sí — "¿con qué inputs se calculó?"
```

### Mensajes en loops de test

```rust
#[test]
fn test_squares() {
    let cases = vec![
        (0, 0),
        (1, 1),
        (2, 4),
        (3, 9),
        (4, 16),
        (5, 25),
    ];

    for (input, expected) in &cases {
        assert_eq!(
            input * input,
            *expected,
            "square({input}) should be {expected}, got {}",
            input * input
        );
    }
}
```

Si el caso `(3, 9)` fallara, verías exactamente cuál:

```
assertion `left == right` failed:
  square(3) should be 9, got 10
  left: 10
 right: 9
```

---

## 6. El módulo `mod tests` y `#[cfg(test)]`

### La convención estándar

```rust
// src/lib.rs

// ══════════════════════════════════════
// Código de producción (siempre compilado)
// ══════════════════════════════════════

pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

pub fn multiply(a: i32, b: i32) -> i32 {
    a * b
}

// ══════════════════════════════════════
// Tests (solo compilados con cargo test)
// ══════════════════════════════════════

#[cfg(test)]          // ← Solo compilar este módulo en modo test
mod tests {           // ← Módulo hijo llamado "tests"
    use super::*;     // ← Importar todo del módulo padre

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    fn test_multiply() {
        assert_eq!(multiply(3, 4), 12);
    }
}
```

### `#[cfg(test)]`: compilación condicional

`#[cfg(test)]` es un atributo de compilación condicional. El código marcado con él **solo se compila cuando estás ejecutando tests** (`cargo test`). En un build normal (`cargo build`), este código no existe — no añade tamaño al binario, no afecta el rendimiento.

```rust
// Esto SOLO existe cuando corres cargo test:
#[cfg(test)]
mod tests {
    // Todo lo que esté aquí se compila solo en modo test
}

// Esto SIEMPRE existe:
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

### ¿Por qué `#[cfg(test)]`?

```
Sin #[cfg(test)]:
  cargo build → compila código + tests → binario más grande, imports innecesarios
  cargo test  → funciona igual

Con #[cfg(test)]:
  cargo build → compila SOLO código → binario limpio
  cargo test  → compila código + tests → funciona igual

Beneficios:
  - El binario de producción no contiene código de test
  - Las dependencias de test (dev-dependencies) no se compilan en producción
  - Imports específicos de test no contaminan el namespace de producción
```

### `use super::*`: acceso al módulo padre

`mod tests` es un módulo **hijo** del módulo donde está definido. Por la jerarquía de módulos de Rust, no tiene acceso automático a los items del padre. `use super::*` importa todo lo público y privado del módulo padre:

```rust
// src/lib.rs
pub fn public_fn() -> i32 { 42 }
fn private_fn() -> i32 { 7 }      // ← Privada

#[cfg(test)]
mod tests {
    use super::*;  // Importa public_fn Y private_fn

    #[test]
    fn test_public() {
        assert_eq!(public_fn(), 42);   // OK
    }

    #[test]
    fn test_private() {
        assert_eq!(private_fn(), 7);   // OK — tests acceden a lo privado
    }
}
```

### Sin `use super::*`: importar selectivamente

```rust
#[cfg(test)]
mod tests {
    use super::add;       // Solo importar add
    use super::multiply;  // Y multiply

    #[test]
    fn test_add() {
        assert_eq!(add(1, 2), 3);
    }
}
```

### `#[cfg(test)]` fuera del módulo

Puedes usar `#[cfg(test)]` en cualquier item, no solo en módulos:

```rust
// Función helper que solo existe en tests
#[cfg(test)]
fn make_test_data() -> Vec<i32> {
    vec![1, 2, 3, 4, 5]
}

// Import que solo existe en tests
#[cfg(test)]
use std::collections::HashMap;

// Constante que solo existe en tests
#[cfg(test)]
const TEST_TIMEOUT: u64 = 5;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_with_helper() {
        let data = make_test_data();
        assert_eq!(data.len(), 5);
    }
}
```

---

## 7. Anatomía completa de un test en Rust

### Las tres fases: Arrange, Act, Assert

```rust
#[test]
fn test_user_full_name() {
    // ── Arrange (preparar) ──
    let user = User {
        first_name: String::from("Alice"),
        last_name: String::from("Smith"),
    };

    // ── Act (ejecutar) ──
    let full_name = user.full_name();

    // ── Assert (verificar) ──
    assert_eq!(full_name, "Alice Smith");
}
```

### Un test falla por panic

Un test de Rust falla cuando la función de test hace **panic**. Las macros `assert!` producen panics cuando la condición no se cumple. Pero cualquier panic falla el test:

```rust
#[test]
fn test_index_out_of_bounds() {
    let v = vec![1, 2, 3];
    let _ = v[10];  // panic: index out of bounds → test FALLA
}

#[test]
fn test_unwrap_none() {
    let x: Option<i32> = None;
    let _ = x.unwrap();  // panic: called unwrap() on None → test FALLA
}

#[test]
fn test_divide_by_zero() {
    let _ = 1 / 0;  // panic: attempt to divide by zero → test FALLA
}
```

### Un test pasa cuando retorna sin panic

```rust
#[test]
fn test_that_passes() {
    let x = 42;
    // No panic → test pasa
    // No necesitas assert! si solo quieres verificar que no crashea
}

#[test]
fn test_construction_doesnt_panic() {
    // Verificar que la construcción no hace panic
    let _config = Config::new("valid_path.toml");
    // Si Config::new hace panic por path inválido,
    // el test falla automáticamente
}
```

### Setup y teardown

Rust no tiene anotaciones `setup`/`teardown` como JUnit. En su lugar, usas funciones helper y RAII:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // ── Setup como función helper ──
    fn setup_database() -> Database {
        let db = Database::new(":memory:");
        db.execute("CREATE TABLE users (id INTEGER, name TEXT)");
        db.execute("INSERT INTO users VALUES (1, 'Alice')");
        db
    }

    #[test]
    fn test_find_user() {
        let db = setup_database();  // ← Setup
        let user = db.find_user(1);
        assert_eq!(user.name, "Alice");
        // ← Teardown: db se destruye aquí por RAII (Drop)
    }

    #[test]
    fn test_count_users() {
        let db = setup_database();  // ← Cada test tiene su propio setup
        assert_eq!(db.count_users(), 1);
        // ← Drop automático
    }
}
```

### RAII como teardown automático

```
  En C:                              En Rust:
  ──────                             ──────

  void test_file(void) {             #[test]
      FILE *f = fopen("t", "w");     fn test_file() {
      // ... test ...                    let f = File::create("t").unwrap();
      fclose(f);  // ← MANUAL           // ... test ...
      remove("t"); // ← MANUAL      }  // ← f se cierra aquí (Drop)
  }                                  // Pero "t" queda en disco
                                     // Para limpiar, usar tempfile crate
                                     // o un struct con Drop custom

  Si el test falla (panic) en C,     En Rust, Drop se ejecuta incluso
  fclose/remove NO se ejecutan.      durante unwinding de un panic.
  → Recursos no liberados.           → Cleanup garantizado.
```

---

## 8. Tests de funciones privadas

### En C: imposible sin trucos

En C, si una función es `static`, no puedes llamarla desde otro translation unit (archivo de test). Necesitas:
- Incluir el `.c` directamente (`#include "module.c"`)
- Remover `static`
- Usar un header interno
- Hacer el test en el mismo archivo

### En Rust: acceso directo

Los tests unitarios en Rust están **dentro del mismo módulo** que el código, por lo que tienen acceso a todo, incluyendo funciones privadas:

```rust
// src/lib.rs

pub struct Stack<T> {
    elements: Vec<T>,     // Campo privado
    max_size: usize,      // Campo privado
}

impl<T> Stack<T> {
    pub fn new(max_size: usize) -> Self {
        Stack {
            elements: Vec::new(),
            max_size,
        }
    }

    pub fn push(&mut self, item: T) -> bool {
        if self.is_full() {
            return false;
        }
        self.elements.push(item);
        true
    }

    pub fn pop(&mut self) -> Option<T> {
        self.elements.pop()
    }

    pub fn len(&self) -> usize {
        self.elements.len()
    }

    pub fn is_empty(&self) -> bool {
        self.elements.is_empty()
    }

    // ── Función PRIVADA ──
    fn is_full(&self) -> bool {
        self.elements.len() >= self.max_size
    }

    // ── Método PRIVADO ──
    fn grow(&mut self) {
        self.max_size *= 2;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_is_full_private() {
        // Podemos testear la función privada directamente
        let mut stack = Stack::new(2);
        assert!(!stack.is_full());

        stack.push(1);
        assert!(!stack.is_full());

        stack.push(2);
        assert!(stack.is_full());  // ← Testeando función privada
    }

    #[test]
    fn test_grow_private() {
        let mut stack = Stack::new(2);
        stack.push(1);
        stack.push(2);
        assert!(stack.is_full());

        stack.grow();  // ← Llamando método privado
        assert!(!stack.is_full());
        assert!(stack.push(3));
    }

    #[test]
    fn test_internal_state() {
        let mut stack = Stack::new(10);
        stack.push(42);

        // Acceder a campos privados
        assert_eq!(stack.elements.len(), 1);
        assert_eq!(stack.elements[0], 42);
        assert_eq!(stack.max_size, 10);
    }
}
```

### ¿Deberías testear funciones privadas?

```
Argumento a favor                    Argumento en contra
──────────────────────────────────────────────────────────────────────
Más granularidad                     Se acopla a la implementación
Detecta bugs internos                Los tests se rompen al refactorizar
Verifica invariantes internos        La API pública debería ser suficiente
Rust lo facilita naturalmente        Hace el test suite más frágil
```

**Regla práctica**: testea funciones privadas cuando tienen lógica compleja que no se puede ejercitar completamente a través de la API pública. No testees getters/setters privados triviales.

---

## 9. `#[ignore]`: tests que no corren por defecto

### Uso

```rust
#[test]
#[ignore]
fn test_slow_network_operation() {
    // Este test tarda 30 segundos
    // No queremos ejecutarlo en cada cargo test
    let response = http_client::get("https://api.example.com/data");
    assert!(response.is_ok());
}

#[test]
#[ignore = "requires database server running"]
fn test_database_migration() {
    // Este test necesita un PostgreSQL corriendo
    let db = Database::connect("postgres://localhost/test");
    db.migrate();
    assert_eq!(db.version(), 42);
}
```

### Ejecutar tests ignorados

```bash
# Solo los tests normales (sin #[ignore])
cargo test

# Solo los tests #[ignore]
cargo test -- --ignored

# TODOS (normales + ignorados)
cargo test -- --include-ignored
```

### Casos de uso para `#[ignore]`

```
Caso de uso                          Ejemplo
──────────────────────────────────────────────────────────────────────
Tests lentos                         Tests de red, criptografía, I/O pesado
Tests con dependencias externas      Base de datos, API externa, hardware
Tests de performance                 Benchmarks informales
Tests flaky (intermitentes)          Tests con timing-dependent logic
Tests en progreso (WIP)              Test que estás escribiendo
Tests de plataforma específica       Test que solo funciona en Linux
```

### Patrón: CI corre los #[ignore]

```yaml
# En CI: ejecutar todo, incluyendo los lentos
- name: Run all tests
  run: cargo test -- --include-ignored
```

---

## 10. Salida de cargo test: leer los resultados

### Salida normal (todos pasan)

```
$ cargo test
   Compiling mylib v0.1.0
    Finished test [unoptimized + debuginfo] target(s)
     Running unittests src/lib.rs (target/debug/deps/mylib-a1b2c3)

running 5 tests
test tests::test_add ... ok
test tests::test_multiply ... ok
test tests::test_abs_negative ... ok
test tests::test_abs_positive ... ok
test tests::test_abs_zero ... ok

test result: ok. 5 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
```

### Salida con fallo

```
$ cargo test
running 5 tests
test tests::test_add ... ok
test tests::test_multiply ... FAILED
test tests::test_abs_negative ... ok
test tests::test_abs_positive ... ok
test tests::test_abs_zero ... ok

failures:

---- tests::test_multiply stdout ----
thread 'tests::test_multiply' panicked at 'assertion `left == right` failed
  left: 12
 right: 13', src/lib.rs:25:9
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

failures:
    tests::test_multiply

test result: FAILED. 4 passed; 1 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
```

### Anatomía de la salida

```
  running 5 tests                              ← Total de tests encontrados
  test tests::test_add ... ok                  ← nombre_completo ... resultado
  test tests::test_multiply ... FAILED         ← FAILED indica panic
  ...

  failures:                                    ← Sección de detalles de fallos

  ---- tests::test_multiply stdout ----        ← Output del test que falló
  thread 'tests::test_multiply' panicked at    ← Mensaje de panic
  'assertion `left == right` failed            ← Tipo de aserción
    left: 12                                   ← Valor actual
   right: 13'                                  ← Valor esperado
  , src/lib.rs:25:9                            ← Archivo:línea:columna

  failures:                                    ← Lista de tests fallidos
      tests::test_multiply

  test result: FAILED.                         ← Resultado global
  4 passed; 1 failed;                          ← Contadores
  0 ignored;                                   ← Tests con #[ignore]
  0 measured;                                  ← Benchmarks (nightly)
  0 filtered out;                              ← Excluidos por filtro de nombre
  finished in 0.00s                            ← Tiempo total
```

### Backtrace

Para obtener un backtrace completo cuando un test falla:

```bash
$ RUST_BACKTRACE=1 cargo test

# Backtrace completo (más detallado)
$ RUST_BACKTRACE=full cargo test
```

---

## 11. Captura de stdout: `--nocapture`

### Comportamiento por defecto

Por defecto, `cargo test` **captura** todo el stdout/stderr de los tests. Si un test pasa, no verás su output. Si falla, verás el output capturado en la sección `failures`.

```rust
#[test]
fn test_with_println() {
    println!("Debugging: calculating result...");
    let result = 2 + 3;
    println!("Result is: {result}");
    assert_eq!(result, 5);
}
```

```bash
$ cargo test
running 1 test
test test_with_println ... ok
# No se ve ningún println — fue capturado y descartado (test pasó)
```

### `--nocapture`: ver todo el output

```bash
$ cargo test -- --nocapture
running 1 test
Debugging: calculating result...
Result is: 5
test test_with_println ... ok
```

### `--show-output`: ver output solo de tests que pasaron

```bash
$ cargo test -- --show-output
```

Esta opción (Rust 1.39+) muestra el output capturado de tests que pasaron, pero de forma organizada (después del resultado, no mezclado).

### Cuándo usar `--nocapture`

```
Escenario                            Usar --nocapture?
──────────────────────────────────────────────────────────────────
Debugging un test específico         Sí
CI pipeline normal                   No (demasiado output)
Test con logging detallado           Sí (temporalmente)
Test de performance con timing       Sí
Tests normales                       No (el default es mejor)
```

---

## 12. Filtrar tests por nombre

### Substring matching

`cargo test` acepta un argumento posicional que filtra tests por **substring** del nombre completo:

```rust
#[cfg(test)]
mod tests {
    #[test]
    fn test_add_positive() { /* ... */ }

    #[test]
    fn test_add_negative() { /* ... */ }

    #[test]
    fn test_add_zero() { /* ... */ }

    #[test]
    fn test_multiply_positive() { /* ... */ }

    #[test]
    fn test_multiply_negative() { /* ... */ }
}
```

```bash
# Ejecutar TODOS
$ cargo test
running 5 tests

# Solo tests con "add" en el nombre
$ cargo test add
running 3 tests
test tests::test_add_positive ... ok
test tests::test_add_negative ... ok
test tests::test_add_zero ... ok
test result: ok. 3 passed; 0 failed; 0 ignored; 0 measured; 2 filtered out

# Solo tests con "negative" en el nombre
$ cargo test negative
running 2 tests
test tests::test_add_negative ... ok
test tests::test_multiply_negative ... ok

# Test exacto
$ cargo test test_add_zero -- --exact
running 1 test
test tests::test_add_zero ... ok
test result: ok. 1 passed; 0 filtered out

# El módulo también cuenta para el filtro
$ cargo test tests::test_add
running 3 tests
```

### Listar tests sin ejecutar

```bash
$ cargo test -- --list
tests::test_add_positive: test
tests::test_add_negative: test
tests::test_add_zero: test
tests::test_multiply_positive: test
tests::test_multiply_negative: test
```

### Combinar filtros con opciones

```bash
# Tests de "add" pero con output visible
cargo test add -- --nocapture

# Tests de "add" en un solo thread
cargo test add -- --test-threads=1

# Solo tests ignorados que contengan "slow"
cargo test slow -- --ignored
```

---

## 13. Ejecución en paralelo y `--test-threads`

### Comportamiento por defecto

`cargo test` ejecuta tests en **paralelo** usando threads. El número de threads por defecto es igual al número de CPUs lógicas.

```
  cargo test (4 CPUs):

  Thread 1: test_add_positive ──► test_add_zero ──► ...
  Thread 2: test_add_negative ──► test_multiply_negative ──► ...
  Thread 3: test_multiply_positive ──► ...
  Thread 4: (idle)

  Los tests se ejecutan en orden NO determinístico.
```

### Implicaciones de la paralelización

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  Los tests NO deben compartir estado mutable global.            ║
 ║                                                                  ║
 ║  Si dos tests escriben al mismo archivo, acceden al mismo       ║
 ║  directorio temporal, o modifican variables globales,           ║
 ║  pueden interferir entre sí (race condition).                   ║
 ║                                                                  ║
 ║  Rust previene data races en memoria (ownership), pero NO       ║
 ║  previene races en recursos externos (archivos, red, etc).      ║
 ╚══════════════════════════════════════════════════════════════════╝
```

### Forzar ejecución secuencial

```bash
# Un solo thread: tests se ejecutan uno a uno
cargo test -- --test-threads=1
```

### Cuándo usar `--test-threads=1`

```
Escenario                              Necesita --test-threads=1?
──────────────────────────────────────────────────────────────────────
Tests puramente funcionales            No — paralelo es seguro y rápido
Tests que escriben al mismo archivo    Sí — race condition
Tests con base de datos compartida     Sí — transacciones conflictan
Tests con puertos de red               Sí — port already in use
Tests con variables de entorno         Sí — env::set_var es global
Debugging de test failures             A veces — output más claro
```

### Alternativa: aislamiento sin sacrificar paralelismo

En vez de serializar todos los tests, aísla los recursos:

```rust
use std::fs;
use std::path::PathBuf;

#[cfg(test)]
fn unique_test_dir(test_name: &str) -> PathBuf {
    let dir = PathBuf::from(format!("/tmp/tests/{}", test_name));
    fs::create_dir_all(&dir).unwrap();
    dir
}

#[test]
fn test_file_write() {
    let dir = unique_test_dir("test_file_write");
    let path = dir.join("output.txt");
    fs::write(&path, "hello").unwrap();
    assert_eq!(fs::read_to_string(&path).unwrap(), "hello");
    fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn test_file_append() {
    let dir = unique_test_dir("test_file_append");
    let path = dir.join("output.txt");
    fs::write(&path, "world").unwrap();
    assert_eq!(fs::read_to_string(&path).unwrap(), "world");
    fs::remove_dir_all(&dir).unwrap();
}
// Ambos tests pueden correr en paralelo: cada uno usa su propio directorio
```

---

## 14. `debug_assert!` y `cfg(debug_assertions)`

### `debug_assert!`

`debug_assert!` es como `assert!` pero **solo se ejecuta en builds de debug**. En builds de release (`--release`), se compila como no-op:

```rust
pub fn divide(a: f64, b: f64) -> f64 {
    debug_assert!(b != 0.0, "Division by zero in divide()");
    a / b
}
```

```
  cargo build         → debug_assert! ACTIVO (panic si b==0)
  cargo build --release → debug_assert! ELIMINADO (no existe en el binario)
  cargo test          → debug_assert! ACTIVO (tests son debug)
  cargo test --release → debug_assert! ELIMINADO
```

### Variantes

```rust
debug_assert!(condition);
debug_assert!(condition, "message {}", var);
debug_assert_eq!(a, b);
debug_assert_eq!(a, b, "message");
debug_assert_ne!(a, b);
debug_assert_ne!(a, b, "message");
```

### Cuándo usar cada uno

```
Macro               Cuándo usar
──────────────────────────────────────────────────────────────────────
assert!             Invariantes que SIEMPRE deben cumplirse
                    → En producción también verificar
                    Ejemplo: assert!(index < self.len())

debug_assert!       Verificaciones caras que solo queremos en desarrollo
                    → No afectar performance en producción
                    Ejemplo: debug_assert!(self.is_sorted())

#[test] + assert!   Verificaciones en tests
                    → Solo en cargo test
```

### `cfg(debug_assertions)` para código condicional

```rust
pub fn process(data: &[u8]) -> Vec<u8> {
    #[cfg(debug_assertions)]
    {
        // Solo en debug: verificar precondiciones costosas
        assert!(data.len() <= MAX_SIZE, "Data too large");
        assert!(is_valid_format(data), "Invalid format");
    }

    // Código de producción...
    transform(data)
}
```

---

## 15. Patrones de testing en Rust

### Patrón 1: Test table (data-driven tests)

```rust
#[test]
fn test_fibonacci() {
    let cases = [
        (0, 0),
        (1, 1),
        (2, 1),
        (3, 2),
        (4, 3),
        (5, 5),
        (10, 55),
        (20, 6765),
    ];

    for (input, expected) in &cases {
        assert_eq!(
            fibonacci(*input),
            *expected,
            "fibonacci({input}) should be {expected}"
        );
    }
}
```

### Patrón 2: Builder para test data

```rust
#[cfg(test)]
struct TestUserBuilder {
    name: String,
    email: String,
    age: u32,
    active: bool,
}

#[cfg(test)]
impl TestUserBuilder {
    fn new() -> Self {
        Self {
            name: "Test User".into(),
            email: "test@example.com".into(),
            age: 25,
            active: true,
        }
    }

    fn name(mut self, name: &str) -> Self {
        self.name = name.into();
        self
    }

    fn age(mut self, age: u32) -> Self {
        self.age = age;
        self
    }

    fn inactive(mut self) -> Self {
        self.active = false;
        self
    }

    fn build(self) -> User {
        User {
            name: self.name,
            email: self.email,
            age: self.age,
            active: self.active,
        }
    }
}

#[test]
fn test_user_greeting() {
    let user = TestUserBuilder::new().name("Alice").build();
    assert_eq!(user.greeting(), "Hello, Alice!");
}

#[test]
fn test_inactive_user_cannot_login() {
    let user = TestUserBuilder::new().inactive().build();
    assert!(!user.can_login());
}
```

### Patrón 3: Wrapper para verificaciones comunes

```rust
#[cfg(test)]
mod tests {
    use super::*;

    fn assert_sorted(v: &[i32]) {
        for i in 1..v.len() {
            assert!(
                v[i - 1] <= v[i],
                "Not sorted at index {}: {} > {}",
                i, v[i - 1], v[i]
            );
        }
    }

    #[test]
    fn test_sort_empty() {
        let mut v: Vec<i32> = vec![];
        my_sort(&mut v);
        assert_sorted(&v);
    }

    #[test]
    fn test_sort_already_sorted() {
        let mut v = vec![1, 2, 3, 4, 5];
        my_sort(&mut v);
        assert_sorted(&v);
    }

    #[test]
    fn test_sort_reverse() {
        let mut v = vec![5, 4, 3, 2, 1];
        my_sort(&mut v);
        assert_sorted(&v);
        assert_eq!(v, vec![1, 2, 3, 4, 5]);
    }
}
```

### Patrón 4: matches! para enums

```rust
#[derive(Debug)]
enum Token {
    Number(f64),
    Plus,
    Minus,
    Star,
    Slash,
    LParen,
    RParen,
    Eof,
}

#[test]
fn test_tokenize_number() {
    let tokens = tokenize("42.5");
    assert_eq!(tokens.len(), 2);
    assert!(matches!(tokens[0], Token::Number(n) if (n - 42.5).abs() < 1e-10));
    assert!(matches!(tokens[1], Token::Eof));
}

#[test]
fn test_tokenize_expression() {
    let tokens = tokenize("3 + 4");
    assert!(matches!(tokens[0], Token::Number(_)));
    assert!(matches!(tokens[1], Token::Plus));
    assert!(matches!(tokens[2], Token::Number(_)));
}
```

### Patrón 5: Fixture con Drop

```rust
struct TempFile {
    path: std::path::PathBuf,
}

impl TempFile {
    fn new(name: &str, content: &str) -> Self {
        let path = std::env::temp_dir().join(name);
        std::fs::write(&path, content).unwrap();
        TempFile { path }
    }

    fn path(&self) -> &std::path::Path {
        &self.path
    }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

#[test]
fn test_read_config() {
    let file = TempFile::new("test_config.toml", "key = \"value\"\n");

    let config = Config::load(file.path()).unwrap();
    assert_eq!(config.get("key"), Some("value"));

    // file se destruye aquí → archivo temporal eliminado
    // Incluso si el test falla (panic), Drop se ejecuta
}
```

---

## 16. Comparación con C: assert.h, Unity, CMocka

### Tabla comparativa

```
Característica          C (assert.h)        C (Unity)            Rust (#[test])
──────────────────────────────────────────────────────────────────────────────────
Descubrimiento          Manual              Manual (RUN_TEST)    Automático (#[test])
Aislamiento             Ninguno (abort)     Setjmp/longjmp       Thread por test
Compilación             Manual              Manual               cargo test
Output                  stderr + abort      PASS/FAIL formateado PASS/FAIL estándar
Mensajes de error       Solo la expresión   Valores esperados    Valores + Debug trait
Tests privados          #include .c         #include .c          Acceso directo (mod)
Setup/teardown          Manual              setUp()/tearDown()   Helper fns + RAII
Falso positivo en oom   abort()             Framework crash      panic + catch (safe)
Filtrado                No                  No nativo            cargo test NOMBRE
Paralelismo             No (sin framework)  No                   Automático (threads)
Condicional compile     Manual #ifdef       Manual               #[cfg(test)]
Macros de aserción      assert()            TEST_ASSERT_EQUAL    assert_eq!
                                            TEST_ASSERT_TRUE     assert!
                                            (60+ macros)
Tipos custom            No (solo int)       Limitado             PartialEq + Debug
Float comparison        Manual              TEST_ASSERT_FLOAT    Manual (pero fácil)
```

### Equivalencias de código

**C con assert.h:**

```c
#include <assert.h>

int add(int a, int b) { return a + b; }

int main(void) {
    assert(add(2, 3) == 5);
    assert(add(-1, 1) == 0);
    printf("All tests passed\n");
    return 0;
}
```

**C con Unity:**

```c
#include "unity.h"

int add(int a, int b) { return a + b; }

void setUp(void) {}
void tearDown(void) {}

void test_add_positive(void) {
    TEST_ASSERT_EQUAL_INT(5, add(2, 3));
}

void test_add_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, add(-1, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_positive);
    RUN_TEST(test_add_zero);
    return UNITY_END();
}
```

**Rust:**

```rust
fn add(a: i32, b: i32) -> i32 { a + b }

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_positive() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    fn test_add_zero() {
        assert_eq!(add(-1, 1), 0);
    }
}
```

### Lo que Rust simplifica respecto a C

```
En C necesitas:                      En Rust:
──────────────────────────────────────────────────────────────────
1. Elegir framework                  Integrado
2. Descargar/compilar framework      cargo test
3. Crear main() con runner           Automático
4. Registrar cada test               Automático (#[test])
5. Makefile con targets de test      cargo test
6. Separar binario de test           Mismo binario, condicional
7. Gestionar memoria en tests        Ownership + RAII
8. Cerrar recursos en cleanup        Drop automático
9. Compilar con flags especiales     cargo test (ya lo hace)
10. Formatear output                 Estándar
```

---

## 17. Errores comunes

### Error 1: olvidar `use super::*`

```rust
#[cfg(test)]
mod tests {
    // Falta use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);  // ERROR: cannot find function `add`
    }
}
```

**Solución**: añadir `use super::*;` al inicio del módulo tests.

### Error 2: assert_eq! con tipos que no implementan Debug

```rust
struct Point { x: f64, y: f64 }

#[test]
fn test_point() {
    let p = Point { x: 1.0, y: 2.0 };
    assert_eq!(p.x, 1.0);  // OK — f64 implementa Debug
    assert_eq!(p, Point { x: 1.0, y: 2.0 });  // ERROR: Point no implementa PartialEq ni Debug
}
```

**Solución**: `#[derive(Debug, PartialEq)]` en la struct.

### Error 3: comparar floats con assert_eq!

```rust
#[test]
fn test_float() {
    assert_eq!(0.1 + 0.2, 0.3);  // FALLA: imprecisión de punto flotante
}
```

**Solución**: usar tolerancia `assert!((a - b).abs() < epsilon)`.

### Error 4: tests que dependen del orden de ejecución

```rust
use std::sync::atomic::{AtomicI32, Ordering};

static COUNTER: AtomicI32 = AtomicI32::new(0);

#[test]
fn test_first() {
    COUNTER.fetch_add(1, Ordering::SeqCst);
    assert_eq!(COUNTER.load(Ordering::SeqCst), 1);  // Puede fallar si test_second corre primero
}

#[test]
fn test_second() {
    COUNTER.fetch_add(1, Ordering::SeqCst);
    assert_eq!(COUNTER.load(Ordering::SeqCst), 2);  // Asume que test_first corrió antes
}
```

**Solución**: cada test debe ser independiente. No usar estado global mutable compartido.

### Error 5: confundir `#[cfg(test)]` con `#[test]`

```rust
// INCORRECTO: #[test] en un módulo
#[test]                     // ← Esto marca una FUNCIÓN como test, no un módulo
mod tests {
    // ERROR: #[test] attribute is only allowed on non-associated functions
}

// CORRECTO: #[cfg(test)] para el módulo, #[test] para cada función
#[cfg(test)]                // ← Compilación condicional del módulo
mod tests {
    use super::*;

    #[test]                 // ← Marca la función como test
    fn test_something() {
        // ...
    }
}
```

### Error 6: test que compila pero no hace nada útil

```rust
#[test]
fn test_process() {
    let data = vec![1, 2, 3];
    let result = process(&data);
    // ← Sin assert! → siempre pasa, no verifica nada
}
```

**Solución**: siempre incluir al menos un assert.

### Error 7: panic esperado pero sin `#[should_panic]`

```rust
#[test]
fn test_divide_by_zero() {
    divide(10, 0);  // Esto hace panic → test FALLA
    // Pero queríamos verificar que SÍ hace panic
}

// Solución: (ver T02 — #[should_panic])
#[test]
#[should_panic]
fn test_divide_by_zero() {
    divide(10, 0);  // Panic → test PASA (porque esperamos el panic)
}
```

---

## 18. Ejemplo completo: biblioteca de fracciones

### `src/lib.rs`

```rust
use std::fmt;

/// Una fracción representada como numerador/denominador.
/// Siempre almacenada en forma reducida con denominador positivo.
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Fraction {
    num: i64,
    den: i64,
}

impl Fraction {
    /// Crear una nueva fracción. Panic si den == 0.
    pub fn new(num: i64, den: i64) -> Self {
        if den == 0 {
            panic!("Fraction denominator cannot be zero");
        }

        let g = gcd(num.unsigned_abs(), den.unsigned_abs()) as i64;
        let sign = if den < 0 { -1 } else { 1 };

        Fraction {
            num: sign * num / g,
            den: sign * den / g,
        }
    }

    /// Fracción desde un entero: n/1
    pub fn from_integer(n: i64) -> Self {
        Fraction { num: n, den: 1 }
    }

    /// Numerador
    pub fn numerator(&self) -> i64 {
        self.num
    }

    /// Denominador
    pub fn denominator(&self) -> i64 {
        self.den
    }

    /// ¿Es cero?
    pub fn is_zero(&self) -> bool {
        self.num == 0
    }

    /// ¿Es positivo?
    pub fn is_positive(&self) -> bool {
        self.num > 0
    }

    /// ¿Es negativo?
    pub fn is_negative(&self) -> bool {
        self.num < 0
    }

    /// Valor absoluto
    pub fn abs(&self) -> Self {
        Fraction {
            num: self.num.abs(),
            den: self.den,
        }
    }

    /// Recíproco (invertir). Panic si es cero.
    pub fn reciprocal(&self) -> Self {
        if self.num == 0 {
            panic!("Cannot take reciprocal of zero");
        }
        Fraction::new(self.den, self.num)
    }

    /// Sumar dos fracciones
    pub fn add(&self, other: &Fraction) -> Fraction {
        Fraction::new(
            self.num * other.den + other.num * self.den,
            self.den * other.den,
        )
    }

    /// Restar
    pub fn sub(&self, other: &Fraction) -> Fraction {
        Fraction::new(
            self.num * other.den - other.num * self.den,
            self.den * other.den,
        )
    }

    /// Multiplicar
    pub fn mul(&self, other: &Fraction) -> Fraction {
        Fraction::new(
            self.num * other.num,
            self.den * other.den,
        )
    }

    /// Dividir. Panic si other es cero.
    pub fn div(&self, other: &Fraction) -> Fraction {
        if other.is_zero() {
            panic!("Division by zero fraction");
        }
        Fraction::new(
            self.num * other.den,
            self.den * other.num,
        )
    }

    /// Convertir a f64
    pub fn to_f64(&self) -> f64 {
        self.num as f64 / self.den as f64
    }
}

impl fmt::Debug for Fraction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.den == 1 {
            write!(f, "{}", self.num)
        } else {
            write!(f, "{}/{}", self.num, self.den)
        }
    }
}

impl fmt::Display for Fraction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.den == 1 {
            write!(f, "{}", self.num)
        } else {
            write!(f, "{}/{}", self.num, self.den)
        }
    }
}

/// Máximo común divisor (algoritmo de Euclides)
fn gcd(mut a: u64, mut b: u64) -> u64 {
    while b != 0 {
        let t = b;
        b = a % b;
        a = t;
    }
    a
}

// ════════════════════════════════════════════════════
// TESTS
// ════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    // ── Helpers ──

    fn frac(n: i64, d: i64) -> Fraction {
        Fraction::new(n, d)
    }

    // ── GCD (función privada) ──

    #[test]
    fn test_gcd_basic() {
        assert_eq!(gcd(12, 8), 4);
        assert_eq!(gcd(7, 13), 1);
        assert_eq!(gcd(100, 75), 25);
    }

    #[test]
    fn test_gcd_with_zero() {
        assert_eq!(gcd(5, 0), 5);
        assert_eq!(gcd(0, 5), 5);
        assert_eq!(gcd(0, 0), 0);
    }

    #[test]
    fn test_gcd_same() {
        assert_eq!(gcd(7, 7), 7);
    }

    // ── Construcción ──

    #[test]
    fn test_new_basic() {
        let f = frac(3, 4);
        assert_eq!(f.numerator(), 3);
        assert_eq!(f.denominator(), 4);
    }

    #[test]
    fn test_new_reduces() {
        let f = frac(6, 8);
        assert_eq!(f.numerator(), 3);
        assert_eq!(f.denominator(), 4);
    }

    #[test]
    fn test_new_negative_denominator() {
        let f = frac(3, -4);
        assert_eq!(f.numerator(), -3);
        assert_eq!(f.denominator(), 4);
    }

    #[test]
    fn test_new_both_negative() {
        let f = frac(-3, -4);
        assert_eq!(f.numerator(), 3);
        assert_eq!(f.denominator(), 4);
    }

    #[test]
    fn test_new_zero_numerator() {
        let f = frac(0, 5);
        assert_eq!(f.numerator(), 0);
        assert_eq!(f.denominator(), 1);
    }

    #[test]
    #[should_panic(expected = "denominator cannot be zero")]
    fn test_new_zero_denominator() {
        frac(1, 0);
    }

    #[test]
    fn test_from_integer() {
        let f = Fraction::from_integer(7);
        assert_eq!(f.numerator(), 7);
        assert_eq!(f.denominator(), 1);
    }

    // ── Predicados ──

    #[test]
    fn test_is_zero() {
        assert!(frac(0, 3).is_zero());
        assert!(!frac(1, 3).is_zero());
    }

    #[test]
    fn test_is_positive() {
        assert!(frac(1, 3).is_positive());
        assert!(!frac(-1, 3).is_positive());
        assert!(!frac(0, 3).is_positive());
    }

    #[test]
    fn test_is_negative() {
        assert!(frac(-1, 3).is_negative());
        assert!(frac(1, -3).is_negative());
        assert!(!frac(1, 3).is_negative());
        assert!(!frac(0, 3).is_negative());
    }

    // ── Operaciones unarias ──

    #[test]
    fn test_abs() {
        assert_eq!(frac(-3, 4).abs(), frac(3, 4));
        assert_eq!(frac(3, 4).abs(), frac(3, 4));
        assert_eq!(frac(0, 1).abs(), frac(0, 1));
    }

    #[test]
    fn test_reciprocal() {
        assert_eq!(frac(3, 4).reciprocal(), frac(4, 3));
        assert_eq!(frac(-2, 5).reciprocal(), frac(-5, 2));
    }

    #[test]
    #[should_panic(expected = "reciprocal of zero")]
    fn test_reciprocal_of_zero() {
        frac(0, 1).reciprocal();
    }

    // ── Aritmética ──

    #[test]
    fn test_add() {
        assert_eq!(frac(1, 4).add(&frac(1, 4)), frac(1, 2));
        assert_eq!(frac(1, 3).add(&frac(1, 6)), frac(1, 2));
        assert_eq!(frac(1, 2).add(&frac(-1, 2)), frac(0, 1));
    }

    #[test]
    fn test_sub() {
        assert_eq!(frac(3, 4).sub(&frac(1, 4)), frac(1, 2));
        assert_eq!(frac(1, 3).sub(&frac(1, 3)), frac(0, 1));
    }

    #[test]
    fn test_mul() {
        assert_eq!(frac(2, 3).mul(&frac(3, 4)), frac(1, 2));
        assert_eq!(frac(1, 2).mul(&frac(0, 1)), frac(0, 1));
    }

    #[test]
    fn test_div() {
        assert_eq!(frac(1, 2).div(&frac(1, 4)), frac(2, 1));
        assert_eq!(frac(3, 4).div(&frac(3, 4)), frac(1, 1));
    }

    #[test]
    #[should_panic(expected = "Division by zero")]
    fn test_div_by_zero() {
        frac(1, 2).div(&frac(0, 1));
    }

    // ── Conversión ──

    #[test]
    fn test_to_f64() {
        assert!((frac(1, 4).to_f64() - 0.25).abs() < 1e-10);
        assert!((frac(1, 3).to_f64() - 0.333333333).abs() < 1e-6);
    }

    // ── Display y Debug ──

    #[test]
    fn test_display() {
        assert_eq!(format!("{}", frac(3, 4)), "3/4");
        assert_eq!(format!("{}", frac(5, 1)), "5");
        assert_eq!(format!("{}", frac(-1, 3)), "-1/3");
    }

    #[test]
    fn test_debug() {
        assert_eq!(format!("{:?}", frac(3, 4)), "3/4");
    }

    // ── Igualdad ──

    #[test]
    fn test_equality() {
        assert_eq!(frac(1, 2), frac(2, 4));
        assert_eq!(frac(1, 2), frac(3, 6));
        assert_ne!(frac(1, 2), frac(1, 3));
    }

    // ── Data-driven ──

    #[test]
    fn test_addition_table() {
        let cases = [
            // (a_num, a_den, b_num, b_den, expected_num, expected_den)
            (1, 2, 1, 2, 1, 1),
            (1, 3, 2, 3, 1, 1),
            (1, 4, 1, 4, 1, 2),
            (1, 6, 1, 3, 1, 2),
            (0, 1, 3, 4, 3, 4),
            (-1, 2, 1, 2, 0, 1),
        ];

        for (an, ad, bn, bd, en_, ed) in &cases {
            let result = frac(*an, *ad).add(&frac(*bn, *bd));
            let expected = frac(*en_, *ed);
            assert_eq!(
                result, expected,
                "{}/{} + {}/{} = {:?}, expected {:?}",
                an, ad, bn, bd, result, expected
            );
        }
    }
}
```

### Ejecutar y ver resultados

```bash
$ cargo test
   Compiling fraction v0.1.0
    Finished test [unoptimized + debuginfo]
     Running unittests src/lib.rs

running 27 tests
test tests::test_abs ... ok
test tests::test_add ... ok
test tests::test_addition_table ... ok
test tests::test_debug ... ok
test tests::test_display ... ok
test tests::test_div ... ok
test tests::test_div_by_zero - should panic ... ok
test tests::test_equality ... ok
test tests::test_from_integer ... ok
test tests::test_gcd_basic ... ok
test tests::test_gcd_same ... ok
test tests::test_gcd_with_zero ... ok
test tests::test_is_negative ... ok
test tests::test_is_positive ... ok
test tests::test_is_zero ... ok
test tests::test_mul ... ok
test tests::test_new_basic ... ok
test tests::test_new_both_negative ... ok
test tests::test_new_negative_denominator ... ok
test tests::test_new_reduces ... ok
test tests::test_new_zero_denominator - should panic ... ok
test tests::test_new_zero_numerator ... ok
test tests::test_reciprocal ... ok
test tests::test_reciprocal_of_zero - should panic ... ok
test tests::test_sub ... ok
test tests::test_to_f64 ... ok

test result: ok. 27 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
```

---

## 19. Programa de práctica

### Enunciado

Implementa una biblioteca `RangeSet` que represente un conjunto de rangos de enteros no solapados:

```rust
pub struct RangeSet {
    // Almacena rangos [start, end) internamente, ordenados, sin solapamiento
}

impl RangeSet {
    /// Crear un RangeSet vacío
    pub fn new() -> Self;

    /// Insertar un rango [start, end). Si se solapa con rangos existentes, fusionarlos.
    pub fn insert(&mut self, start: i64, end: i64);

    /// ¿Contiene el valor x?
    pub fn contains(&self, x: i64) -> bool;

    /// ¿Está vacío?
    pub fn is_empty(&self) -> bool;

    /// Número de rangos internos (después de fusión)
    pub fn range_count(&self) -> usize;

    /// Total de valores enteros cubiertos
    pub fn total_size(&self) -> u64;

    /// Eliminar un valor del set (puede partir un rango en dos)
    pub fn remove(&mut self, x: i64);

    /// Iterador sobre todos los rangos como tuplas (start, end)
    pub fn ranges(&self) -> impl Iterator<Item = (i64, i64)> + '_;
}
```

### Tareas

1. Implementa `RangeSet` en `src/lib.rs`
2. Escribe tests unitarios en `mod tests` cubriendo:
   - Construcción y `is_empty()`
   - Inserción de un rango y `contains()`
   - Inserción de rangos no solapados
   - Inserción de rangos solapados (fusión)
   - Inserción de rangos adyacentes (fusión)
   - `remove()` de valor al inicio, fin, medio, fuera del rango
   - `total_size()` después de varias operaciones
   - Edge cases: rango vacío (start >= end), rango de un solo elemento
3. Usa data-driven tests (tablas de casos) para al menos una operación
4. Usa mensajes custom en los assert para facilitar debugging
5. Verifica que `cargo test` pasa todos los tests
6. Usa `cargo test -- --list` para listar tus tests
7. Verifica que puedes filtrar por nombre: `cargo test insert`, `cargo test remove`

---

## 20. Ejercicios

### Ejercicio 1: Primeros tests (15 min)

Implementa y testea esta función:

```rust
/// Retorna true si la cadena es un palíndromo (ignorando case y espacios)
pub fn is_palindrome(s: &str) -> bool;
```

**Tareas**:
- a) Implementa `is_palindrome`
- b) Escribe al menos 8 tests cubriendo: string vacío, un carácter, palíndromo simple, no palíndromo, con mayúsculas, con espacios, con números, con caracteres especiales
- c) Usa `assert!` para las verificaciones booleanas y mensajes custom con el input
- d) Ejecuta `cargo test` y verifica que todos pasan

### Ejercicio 2: assert_eq! con tipos custom (15 min)

```rust
#[derive(Debug, PartialEq)]
pub struct Money {
    cents: i64,
    currency: String,
}
```

**Tareas**:
- a) Implementa `Money::new(amount_str: &str, currency: &str) -> Result<Money, String>` que parsee strings como "10.50", "0.99", "100"
- b) Implementa `Money::add(&self, other: &Money) -> Result<Money, String>` que falle si las monedas son diferentes
- c) Escribe tests usando `assert_eq!` para verificar que la struct se compara correctamente
- d) Escribe un test que verifique que sumar monedas diferentes retorna `Err`
- e) ¿Qué pasa si quitas `#[derive(Debug)]`? ¿Y `#[derive(PartialEq)]`?

### Ejercicio 3: tests de funciones privadas (15 min)

```rust
pub struct Password(String);

impl Password {
    pub fn new(input: &str) -> Result<Self, Vec<&'static str>> {
        let errors = Self::validate(input);
        if errors.is_empty() {
            Ok(Password(input.to_string()))
        } else {
            Err(errors)
        }
    }

    // ── Función PRIVADA ──
    fn validate(input: &str) -> Vec<&'static str> {
        let mut errors = vec![];
        if input.len() < 8 { errors.push("too short"); }
        if !input.chars().any(|c| c.is_uppercase()) { errors.push("no uppercase"); }
        if !input.chars().any(|c| c.is_ascii_digit()) { errors.push("no digit"); }
        errors
    }
}
```

**Tareas**:
- a) Escribe tests para la función privada `validate()` directamente, verificando cada regla
- b) Escribe tests para `Password::new()` a través de la API pública
- c) ¿Cuáles tests son más útiles? ¿Los que testean validate() directamente o los que usan new()? Justifica
- d) Añade una regla más a validate (ejemplo: "no special char") y verifica que tus tests de validate() detectan el cambio

### Ejercicio 4: paralelismo y aislamiento (20 min)

Escribe un módulo que lea y escriba a un archivo. Luego:

**Tareas**:
- a) Escribe 4 tests que lean/escriban archivos
- b) Ejecútalos con `cargo test` (paralelo) — ¿fallan intermitentemente?
- c) Ejecútalos con `cargo test -- --test-threads=1` — ¿ahora pasan?
- d) Refactoriza para que cada test use su propio archivo temporal (directorios únicos o nombres únicos)
- e) Verifica que con la refactorización pasan tanto en paralelo como en secuencial
- f) Implementa un struct con `Drop` que limpie los archivos temporales

---

**Navegación**:
- ← Anterior: [C01/S04/T03 — Cobertura en CI](../../C01_Testing_en_C/S04_Cobertura_en_C/T03_Cobertura_en_CI/README.md)
- → Siguiente: [T02 — Tests que deben fallar](../T02_Tests_que_deben_fallar/README.md)
- ↑ Sección: [S01 — Testing Built-in](../README.md)
