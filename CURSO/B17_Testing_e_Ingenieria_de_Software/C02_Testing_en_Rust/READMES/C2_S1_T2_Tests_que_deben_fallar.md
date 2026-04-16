# T02 — Tests que deben fallar: `#[should_panic]`, expected message

> **Bloque 17 — Testing e Ingeniería de Software → C02 — Testing en Rust → S01 — Testing Built-in → T02**

---

## Índice

1. [El problema: verificar que el código falla correctamente](#1-el-problema-verificar-que-el-código-falla-correctamente)
2. [#[should_panic]: el atributo básico](#2-should_panic-el-atributo-básico)
3. [expected: verificar el mensaje de panic](#3-expected-verificar-el-mensaje-de-panic)
4. [Mecánica interna: cómo funciona should_panic](#4-mecánica-interna-cómo-funciona-should_panic)
5. [Patrones comunes con should_panic](#5-patrones-comunes-con-should_panic)
6. [Limitaciones de should_panic](#6-limitaciones-de-should_panic)
7. [Alternativa: std::panic::catch_unwind](#7-alternativa-stdpaniccatch_unwind)
8. [Cuándo usar should_panic vs Result](#8-cuándo-usar-should_panic-vs-result)
9. [panic! vs Result en el diseño de APIs](#9-panic-vs-result-en-el-diseño-de-apis)
10. [Comparación con C: señales, setjmp, abort](#10-comparación-con-c-señales-setjmp-abort)
11. [Errores comunes](#11-errores-comunes)
12. [Ejemplo completo: biblioteca de matrices](#12-ejemplo-completo-biblioteca-de-matrices)
13. [Programa de práctica](#13-programa-de-práctica)
14. [Ejercicios](#14-ejercicios)

---

## 1. El problema: verificar que el código falla correctamente

A veces, el comportamiento **correcto** de una función es hacer panic. Por ejemplo:

```rust
pub fn divide(a: f64, b: f64) -> f64 {
    if b == 0.0 {
        panic!("Division by zero");  // ← Este es el comportamiento correcto
    }
    a / b
}
```

Queremos un test que verifique: "si llamo a `divide(10.0, 0.0)`, el programa debe hacer panic". Pero un test normal **falla** cuando hay panic:

```rust
#[test]
fn test_divide_by_zero() {
    divide(10.0, 0.0);
    // El test FALLA porque panic! se propagó
    // Pero nosotros QUERÍAMOS que hiciera panic
}
```

### El diagrama del problema

```
  Test normal:
  ┌────────────────────────────────┐
  │  fn test() {                   │
  │      funcion_que_paniquea();   │
  │  }                             │
  │                                │
  │  Resultado: FAIL               │
  │  (panic = test fallido)        │
  └────────────────────────────────┘

  Lo que necesitamos:
  ┌────────────────────────────────┐
  │  #[should_panic]               │
  │  fn test() {                   │
  │      funcion_que_paniquea();   │
  │  }                             │
  │                                │
  │  Resultado: PASS               │
  │  (panic = comportamiento       │
  │   esperado = test exitoso)     │
  └────────────────────────────────┘
```

---

## 2. `#[should_panic]`: el atributo básico

### Sintaxis

```rust
#[test]
#[should_panic]
fn nombre_del_test() {
    // Código que DEBE hacer panic
    // Si hace panic → test PASA
    // Si NO hace panic → test FALLA
}
```

### Semántica invertida

```
Test normal:              #[should_panic]:
  panic → FAIL              panic → PASS
  no panic → PASS           no panic → FAIL
```

### Ejemplo básico

```rust
pub fn factorial(n: u32) -> u64 {
    if n > 20 {
        panic!("factorial overflow: n={n} exceeds maximum of 20");
    }
    (1..=n as u64).product()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_factorial_normal() {
        assert_eq!(factorial(0), 1);
        assert_eq!(factorial(5), 120);
        assert_eq!(factorial(20), 2_432_902_008_176_640_000);
    }

    #[test]
    #[should_panic]
    fn test_factorial_overflow() {
        factorial(21);  // Debe hacer panic → test PASA
    }
}
```

```bash
$ cargo test
running 2 tests
test tests::test_factorial_normal ... ok
test tests::test_factorial_overflow - should panic ... ok

test result: ok. 2 passed; 0 failed
```

Nota en la salida: el test muestra `- should panic` para indicar que se espera un panic.

### ¿Qué pasa si el test NO hace panic?

```rust
#[test]
#[should_panic]
fn test_that_doesnt_panic() {
    let x = 42;  // No panic → test FALLA
}
```

```
running 1 test
test test_that_doesnt_panic - should panic ... FAILED

failures:

---- test_that_doesnt_panic stdout ----
note: test did not panic as expected

failures:
    test_that_doesnt_panic

test result: FAILED. 0 passed; 1 failed
```

El mensaje `test did not panic as expected` indica claramente que el test esperaba un panic pero no lo obtuvo.

---

## 3. `expected`: verificar el mensaje de panic

### El problema de `#[should_panic]` sin expected

Con `#[should_panic]` solo, **cualquier** panic hace pasar el test. Esto puede enmascarar bugs:

```rust
pub fn get_item(items: &[i32], index: usize) -> i32 {
    if index >= items.len() {
        panic!("Index {} out of bounds (len={})", index, items.len());
    }
    items[index]
}

#[test]
#[should_panic]
fn test_out_of_bounds() {
    let items: Vec<i32> = vec![];   // Vector vacío
    get_item(&items, 5);
    // ¿Hizo panic por nuestro check "out of bounds"?
    // ¿O por un bug interno completamente diferente?
    // Con solo #[should_panic], no sabemos.
}
```

### Sintaxis de expected

```rust
#[test]
#[should_panic(expected = "substring del mensaje")]
fn test_con_expected() {
    // Solo pasa si el panic message CONTIENE el substring
}
```

### Ejemplo

```rust
pub fn get_item(items: &[i32], index: usize) -> i32 {
    if index >= items.len() {
        panic!("Index {} out of bounds (len={})", index, items.len());
    }
    items[index]
}

#[test]
#[should_panic(expected = "out of bounds")]
fn test_out_of_bounds() {
    let items = vec![1, 2, 3];
    get_item(&items, 10);
    // Pasa: el panic message contiene "out of bounds"
}

#[test]
#[should_panic(expected = "Index 10 out of bounds (len=3)")]
fn test_out_of_bounds_exact() {
    let items = vec![1, 2, 3];
    get_item(&items, 10);
    // Pasa: el panic message contiene exactamente ese substring
}
```

### Qué pasa si el expected no coincide

```rust
#[test]
#[should_panic(expected = "division by zero")]
fn test_wrong_expected() {
    let items = vec![1, 2, 3];
    get_item(&items, 10);
    // FALLA: el panic dice "out of bounds", no "division by zero"
}
```

```
running 1 test
test test_wrong_expected - should panic ... FAILED

failures:

---- test_wrong_expected stdout ----
thread 'test_wrong_expected' panicked at 'Index 10 out of bounds (len=3)'
note: panic did not contain expected string
      panic message: `"Index 10 out of bounds (len=3)"`,
 expected substring: `"division by zero"`

failures:
    test_wrong_expected
```

El error es muy descriptivo: muestra el mensaje real del panic y el substring esperado.

### Reglas de coincidencia de expected

```
Regla                                     Ejemplo
──────────────────────────────────────────────────────────────────────────
expected es un SUBSTRING, no exact match  expected = "bounds" coincide con
                                          "Index 10 out of bounds (len=3)"

Case-sensitive                            expected = "Bounds" NO coincide con
                                          "out of bounds"

Puede ser cualquier parte del mensaje     expected = "len=3" coincide con
                                          "Index 10 out of bounds (len=3)"

Vacío es válido (pero inútil)             expected = "" coincide con todo
```

### Buenas prácticas para expected

```rust
// ── BUENO: substring específico que identifica el error ──
#[should_panic(expected = "denominator cannot be zero")]
#[should_panic(expected = "index out of bounds")]
#[should_panic(expected = "stack overflow")]
#[should_panic(expected = "capacity exceeded")]

// ── MALO: demasiado genérico ──
#[should_panic(expected = "error")]        // Demasiado vago
#[should_panic(expected = "failed")]       // Podría ser cualquier fallo
#[should_panic(expected = "0")]            // Podría coincidir accidentalmente

// ── MALO: demasiado específico (frágil) ──
#[should_panic(expected = "Index 10 out of bounds for slice of length 3 at src/lib.rs:42")]
// Se rompe si cambias el número de línea, el formato, etc.

// ── IDEAL: parte estable y descriptiva del mensaje ──
#[should_panic(expected = "out of bounds")]  // Específico pero no frágil
```

---

## 4. Mecánica interna: cómo funciona `should_panic`

### El test harness

Cuando `cargo test` compila tu código, el test harness envuelve cada función `#[test]` en un wrapper. Para tests con `#[should_panic]`, el wrapper usa `std::panic::catch_unwind`:

```rust
// Pseudocódigo de lo que hace el test harness internamente:

// Para un test normal:
fn run_test_normal(test_fn: fn()) -> TestResult {
    test_fn();  // Si panic → FAIL
    TestResult::Ok
}

// Para un test con #[should_panic]:
fn run_test_should_panic(test_fn: fn(), expected: Option<&str>) -> TestResult {
    let result = std::panic::catch_unwind(|| {
        test_fn();
    });

    match result {
        Ok(()) => {
            // No hubo panic → FAIL (esperábamos panic)
            TestResult::Failed("test did not panic as expected")
        }
        Err(panic_value) => {
            // Hubo panic → verificar expected
            match expected {
                None => TestResult::Ok,  // Solo #[should_panic], cualquier panic vale
                Some(expected_msg) => {
                    let msg = extract_panic_message(&panic_value);
                    if msg.contains(expected_msg) {
                        TestResult::Ok
                    } else {
                        TestResult::Failed("panic did not contain expected string")
                    }
                }
            }
        }
    }
}
```

### Qué tipos puede tener un panic

`panic!()` puede llevar diferentes tipos de payload:

```rust
// String (lo más común)
panic!("something went wrong");
// Payload: &str

// String formateada
panic!("error at index {}", 42);
// Payload: String (format! genera String)

// Tipo custom (raro pero posible)
panic!(MyError { code: 42 });
// Payload: MyError

// panic con std::panic::panic_any
std::panic::panic_any(42_i32);
// Payload: i32
```

`expected` en `#[should_panic]` solo funciona con payloads que sean `&str` o `String`. Si el panic tiene un tipo custom, `expected` no puede verificar el contenido (el test pasa igualmente si no especificas `expected`).

### Unwinding vs abort

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  IMPORTANTE: #[should_panic] requiere panic=unwind              ║
 ║                                                                  ║
 ║  Si tu Cargo.toml tiene:                                        ║
 ║    [profile.test]                                               ║
 ║    panic = "abort"                                              ║
 ║                                                                  ║
 ║  Los tests #[should_panic] NO funcionarán.                      ║
 ║  panic=abort termina el proceso inmediatamente, sin dar chance  ║
 ║  a catch_unwind de capturar el panic.                           ║
 ║                                                                  ║
 ║  El perfil test usa panic=unwind por defecto. No lo cambies.    ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 5. Patrones comunes con `should_panic`

### Patrón 1: Verificar precondiciones

```rust
pub struct BoundedVec<T> {
    items: Vec<T>,
    max_size: usize,
}

impl<T> BoundedVec<T> {
    pub fn new(max_size: usize) -> Self {
        assert!(max_size > 0, "max_size must be positive");
        BoundedVec {
            items: Vec::new(),
            max_size,
        }
    }

    pub fn push(&mut self, item: T) {
        if self.items.len() >= self.max_size {
            panic!(
                "BoundedVec overflow: capacity {} reached",
                self.max_size
            );
        }
        self.items.push(item);
    }

    pub fn get(&self, index: usize) -> &T {
        if index >= self.items.len() {
            panic!(
                "BoundedVec index {} out of bounds (len={})",
                index, self.items.len()
            );
        }
        &self.items[index]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic(expected = "max_size must be positive")]
    fn test_zero_capacity() {
        BoundedVec::<i32>::new(0);
    }

    #[test]
    #[should_panic(expected = "overflow: capacity 3 reached")]
    fn test_push_overflow() {
        let mut bv = BoundedVec::new(3);
        bv.push(1);
        bv.push(2);
        bv.push(3);
        bv.push(4);  // Overflow
    }

    #[test]
    #[should_panic(expected = "index 5 out of bounds")]
    fn test_get_out_of_bounds() {
        let bv = BoundedVec::<i32>::new(10);
        bv.get(5);  // Vacío, cualquier index es out of bounds
    }
}
```

### Patrón 2: Verificar unwrap de None/Err

```rust
pub fn find_user(users: &[User], id: u64) -> &User {
    users
        .iter()
        .find(|u| u.id == id)
        .expect(&format!("User with id {} not found", id))
}

#[test]
#[should_panic(expected = "User with id 999 not found")]
fn test_find_nonexistent_user() {
    let users = vec![
        User { id: 1, name: "Alice".into() },
        User { id: 2, name: "Bob".into() },
    ];
    find_user(&users, 999);
}
```

### Patrón 3: Verificar invariantes de constructor

```rust
pub struct Percentage(f64);

impl Percentage {
    pub fn new(value: f64) -> Self {
        if value < 0.0 || value > 100.0 {
            panic!("Percentage must be between 0 and 100, got {value}");
        }
        if value.is_nan() {
            panic!("Percentage cannot be NaN");
        }
        Percentage(value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_percentages() {
        let _ = Percentage::new(0.0);
        let _ = Percentage::new(50.0);
        let _ = Percentage::new(100.0);
    }

    #[test]
    #[should_panic(expected = "between 0 and 100")]
    fn test_negative() {
        Percentage::new(-1.0);
    }

    #[test]
    #[should_panic(expected = "between 0 and 100")]
    fn test_over_100() {
        Percentage::new(100.1);
    }

    #[test]
    #[should_panic(expected = "cannot be NaN")]
    fn test_nan() {
        Percentage::new(f64::NAN);
    }
}
```

### Patrón 4: Verificar operaciones con tipos indexables

```rust
impl<T> Index<usize> for MyVec<T> {
    type Output = T;

    fn index(&self, idx: usize) -> &T {
        if idx >= self.len {
            panic!("index out of bounds: the len is {} but the index is {}", self.len, idx);
        }
        unsafe { &*self.ptr.add(idx) }
    }
}

#[test]
#[should_panic(expected = "index out of bounds")]
fn test_index_panics() {
    let v = MyVec::from_slice(&[1, 2, 3]);
    let _ = v[10];
}
```

### Patrón 5: Múltiples tests para diferentes panics de la misma función

```rust
pub fn parse_color(s: &str) -> (u8, u8, u8) {
    if s.is_empty() {
        panic!("Color string cannot be empty");
    }
    if !s.starts_with('#') {
        panic!("Color must start with '#', got '{s}'");
    }
    if s.len() != 7 {
        panic!("Color must be 7 characters (#RRGGBB), got {} chars", s.len());
    }

    let r = u8::from_str_radix(&s[1..3], 16)
        .unwrap_or_else(|_| panic!("Invalid red component in '{s}'"));
    let g = u8::from_str_radix(&s[3..5], 16)
        .unwrap_or_else(|_| panic!("Invalid green component in '{s}'"));
    let b = u8::from_str_radix(&s[5..7], 16)
        .unwrap_or_else(|_| panic!("Invalid blue component in '{s}'"));

    (r, g, b)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_colors() {
        assert_eq!(parse_color("#FF0000"), (255, 0, 0));
        assert_eq!(parse_color("#00FF00"), (0, 255, 0));
        assert_eq!(parse_color("#0000FF"), (0, 0, 255));
        assert_eq!(parse_color("#000000"), (0, 0, 0));
        assert_eq!(parse_color("#FFFFFF"), (255, 255, 255));
    }

    #[test]
    #[should_panic(expected = "cannot be empty")]
    fn test_empty() {
        parse_color("");
    }

    #[test]
    #[should_panic(expected = "must start with '#'")]
    fn test_no_hash() {
        parse_color("FF0000");
    }

    #[test]
    #[should_panic(expected = "must be 7 characters")]
    fn test_wrong_length() {
        parse_color("#FFF");
    }

    #[test]
    #[should_panic(expected = "Invalid red component")]
    fn test_invalid_hex_red() {
        parse_color("#GG0000");
    }

    #[test]
    #[should_panic(expected = "Invalid green component")]
    fn test_invalid_hex_green() {
        parse_color("#00GG00");
    }

    #[test]
    #[should_panic(expected = "Invalid blue component")]
    fn test_invalid_hex_blue() {
        parse_color("#0000GG");
    }
}
```

---

## 6. Limitaciones de `should_panic`

### Limitación 1: no se puede verificar qué línea hizo panic

```rust
#[test]
#[should_panic(expected = "overflow")]
fn test_overflow() {
    let mut v = BoundedVec::new(2);
    v.push(compute_value_a());  // ¿Y si ESTA función hace panic con "overflow"?
    v.push(compute_value_b());  // ¿O esta?
    v.push(42);                  // Solo queremos que ESTA haga panic
}
```

El test pasa si **cualquier** línea hace panic con "overflow". No puedes verificar que el panic vino de la línea específica que esperabas.

### Limitación 2: no se puede capturar el valor del panic

```rust
// No puedes hacer esto con #[should_panic]:
#[test]
#[should_panic]
fn test_and_check_panic_value() {
    let result = some_function();
    // Quiero verificar que el panic tiene un código de error específico
    // Pero #[should_panic] solo verifica el mensaje como string
}
```

### Limitación 3: no se puede continuar después del panic

```rust
#[test]
#[should_panic]
fn test_only_first_panic_matters() {
    operation_that_panics();
    // Todo lo que esté aquí NUNCA se ejecuta
    // No puedes verificar estado post-panic
    // No puedes hacer cleanup explícito
    assert_eq!(counter, 0);  // Nunca se ejecuta
}
```

### Limitación 4: un solo panic por test

```rust
// No puedes verificar múltiples panics en un solo test
#[test]
#[should_panic]
fn test_multiple_panics() {
    first_operation_that_panics();   // Panic aquí → test pasa
    second_operation_that_panics();  // Nunca se ejecuta
}
// Solución: un test separado por cada panic esperado
```

### Limitación 5: no funciona con panic=abort

Como se explicó en la sección 4, si el perfil de compilación usa `panic = "abort"`, `catch_unwind` no puede capturar panics y los tests `#[should_panic]` causan que el test runner se termine abruptamente.

### Resumen de limitaciones

```
Limitación                           Alternativa
──────────────────────────────────────────────────────────────────────
No sé qué línea paniceó             std::panic::catch_unwind
No puedo capturar el valor           std::panic::catch_unwind
No puedo continuar post-panic        std::panic::catch_unwind
Solo verifico substring del msg      std::panic::catch_unwind
Un panic por test                    Tests separados o catch_unwind
No funciona con panic=abort          Usar panic=unwind (default)
No puedo verificar side effects      catch_unwind + verificaciones post
```

---

## 7. Alternativa: `std::panic::catch_unwind`

### Uso básico

```rust
use std::panic;

#[test]
fn test_with_catch_unwind() {
    let result = panic::catch_unwind(|| {
        divide(10.0, 0.0);
    });

    assert!(result.is_err(), "Expected panic but function returned normally");
}
```

### Capturar y verificar el mensaje

```rust
use std::panic;

#[test]
fn test_verify_panic_message() {
    let result = panic::catch_unwind(|| {
        divide(10.0, 0.0);
    });

    match result {
        Ok(_) => panic!("Expected panic but function returned normally"),
        Err(panic_value) => {
            // El valor del panic puede ser &str o String
            let msg = if let Some(s) = panic_value.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = panic_value.downcast_ref::<String>() {
                s.clone()
            } else {
                panic!("Panic value was not a string");
            };

            assert!(
                msg.contains("Division by zero"),
                "Expected 'Division by zero' in panic message, got: {msg}"
            );
        }
    }
}
```

### Helper para tests con catch_unwind

```rust
#[cfg(test)]
fn assert_panics_with(f: impl FnOnce() + std::panic::UnwindSafe, expected: &str) {
    let result = std::panic::catch_unwind(f);

    match result {
        Ok(_) => panic!("Expected panic with '{expected}' but function returned normally"),
        Err(panic_value) => {
            let msg = if let Some(s) = panic_value.downcast_ref::<&str>() {
                s.to_string()
            } else if let Some(s) = panic_value.downcast_ref::<String>() {
                s.clone()
            } else {
                panic!("Panic value was not a string type");
            };

            assert!(
                msg.contains(expected),
                "Panic message '{}' did not contain '{}'",
                msg, expected
            );
        }
    }
}

#[test]
fn test_multiple_panics_one_test() {
    // Verificar múltiples panics en UN solo test
    assert_panics_with(|| divide(1.0, 0.0), "Division by zero");
    assert_panics_with(|| divide(-5.0, 0.0), "Division by zero");

    // También podemos verificar que NO hace panic
    let result = std::panic::catch_unwind(|| divide(10.0, 2.0));
    assert!(result.is_ok());
}
```

### Verificar estado post-panic

```rust
use std::panic;
use std::sync::{Arc, Mutex};

#[test]
fn test_state_after_panic() {
    let log = Arc::new(Mutex::new(Vec::new()));
    let log_clone = Arc::clone(&log);

    let result = panic::catch_unwind(move || {
        let mut l = log_clone.lock().unwrap();
        l.push("operation started");
        // Simular operación que falla
        panic!("operation failed");
        // l.push("operation completed");  // Nunca se ejecuta
    });

    assert!(result.is_err());

    // Podemos verificar el estado parcial
    let log = log.lock().unwrap();
    assert_eq!(log.len(), 1);
    assert_eq!(log[0], "operation started");
    // "operation completed" no está porque el panic interrumpió
}
```

### catch_unwind vs should_panic: cuándo usar cada uno

```
Escenario                                  Recomendación
──────────────────────────────────────────────────────────────────────
Verificar que una sola llamada paniquea    #[should_panic] (más simple)
Verificar el mensaje del panic             #[should_panic(expected)]
Múltiples panics en un test                catch_unwind
Verificar estado post-panic                catch_unwind
Capturar y analizar el valor del panic     catch_unwind
Verificar que NO hace panic                catch_unwind + is_ok()
Test simple y legible                      #[should_panic]
Test complejo con múltiples verificaciones catch_unwind
```

---

## 8. Cuándo usar `should_panic` vs `Result`

### El dilema

Rust ofrece dos formas de manejar errores:

```
1. panic!("error") → para errores de programación (bugs)
   - Precondiciones violadas
   - Estado imposible alcanzado
   - Invariantes rotas
   → Testar con #[should_panic]

2. Result<T, E>   → para errores esperados (operaciones que pueden fallar)
   - Archivo no encontrado
   - Input inválido del usuario
   - Red no disponible
   → Testar con assert!(result.is_err()) o assert_eq!(result, Err(...))
```

### Decidir qué usar en la API

```rust
// ── CORRECTO: panic para precondiciones (error del programador) ──
pub fn get(slice: &[i32], index: usize) -> i32 {
    if index >= slice.len() {
        panic!("index out of bounds");
    }
    slice[index]
}
// Test: #[should_panic(expected = "out of bounds")]

// ── CORRECTO: Result para operaciones falibles (error esperado) ──
pub fn parse_int(s: &str) -> Result<i32, ParseError> {
    s.parse::<i32>().map_err(|e| ParseError::Invalid(e.to_string()))
}
// Test: assert!(parse_int("abc").is_err())
// Test: assert_eq!(parse_int("42"), Ok(42))
```

### Tabla de decisión

```
La función...                              Usar        Testar con
──────────────────────────────────────────────────────────────────────
Recibe input del usuario                   Result      assert_eq!(f(), Err(...))
Recibe input de red/archivo                Result      assert!(f().is_err())
Tiene precondición que el caller garantiza panic       #[should_panic]
Opera en estado que nunca debería existir  panic       #[should_panic]
Implementa Index/IndexMut                  panic       #[should_panic]
Divide por cero (error del programador)    panic       #[should_panic]
Divide por cero (input del usuario)        Result      assert_eq!(...)
Constructor con parámetros inválidos       Depende:
  - Si el caller controla los params      panic       #[should_panic]
  - Si los params vienen de I/O           Result      assert_eq!(...)
```

### Ejemplo: la misma operación con ambos enfoques

```rust
// Enfoque 1: panic (para uso interno, el caller garantiza b != 0)
pub fn divide_or_panic(a: f64, b: f64) -> f64 {
    assert!(b != 0.0, "Division by zero");
    a / b
}

// Enfoque 2: Result (para uso público, el input puede ser cualquiera)
#[derive(Debug, PartialEq)]
pub enum MathError {
    DivisionByZero,
}

pub fn divide_safe(a: f64, b: f64) -> Result<f64, MathError> {
    if b == 0.0 {
        return Err(MathError::DivisionByZero);
    }
    Ok(a / b)
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── Tests para el enfoque panic ──

    #[test]
    fn test_divide_panic_normal() {
        assert_eq!(divide_or_panic(10.0, 2.0), 5.0);
    }

    #[test]
    #[should_panic(expected = "Division by zero")]
    fn test_divide_panic_zero() {
        divide_or_panic(10.0, 0.0);
    }

    // ── Tests para el enfoque Result ──

    #[test]
    fn test_divide_safe_normal() {
        assert_eq!(divide_safe(10.0, 2.0), Ok(5.0));
    }

    #[test]
    fn test_divide_safe_zero() {
        assert_eq!(divide_safe(10.0, 0.0), Err(MathError::DivisionByZero));
    }
}
```

### Preferencia general de la comunidad Rust

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  En Rust, la preferencia es:                                     ║
 ║                                                                  ║
 ║  1. Usar Result<T, E> siempre que sea posible                   ║
 ║  2. Reservar panic! para bugs del programador                   ║
 ║  3. Si el caller NO puede prevenir el error → Result            ║
 ║  4. Si el caller DEBERÍA haber prevenido el error → panic       ║
 ║                                                                  ║
 ║  Esto significa que #[should_panic] se usa relativamente poco.  ║
 ║  Los tests con Result<(), E> (ver T03) son más comunes.         ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 9. `panic!` vs `Result` en el diseño de APIs

### El espectro de diseño

```
                panic!                                    Result<T,E>
  ←─────────────────────────────────────────────────────────────────→
  "El caller tiene un bug"              "La operación puede fallar"

  Ejemplos:                              Ejemplos:
  - Index out of bounds                  - File not found
  - Null pointer dereference             - Parse error
  - Invariante rota                      - Network timeout
  - Capacity overflow                    - Permission denied
  - Division by zero (math)              - Invalid input from user

  Test: #[should_panic]                  Test: assert_eq!(f(), Err(...))
                                         Test: fn test() -> Result<()>
```

### La regla de la biblioteca estándar

La std de Rust sigue esta convención consistentemente:

```rust
// panic: el caller tiene un bug
let v = vec![1, 2, 3];
let x = v[10];           // panic: index out of bounds

// Result: la operación puede fallar legítimamente
let f = File::open("x"); // Result: el archivo puede no existir

// Ambas versiones disponibles:
let x = v[10];            // panic!  — cuando SABES que el index es válido
let x = v.get(10);        // Option  — cuando NO sabes si es válido

let s = "42".parse::<i32>().unwrap();  // panic si falla (caller garantiza)
let s = "42".parse::<i32>();            // Result (caller no sabe si es válido)
```

### Implicaciones para testing

```
Si tu API usa panic! para errores:
  - Necesitas #[should_panic] tests
  - Los tests son binarios: panic o no panic
  - No puedes verificar detalles del error (solo substring del mensaje)
  - El error no tiene tipo (es un string)

Si tu API usa Result para errores:
  - Usas assert_eq!/assert! normales
  - Puedes verificar el TIPO exacto de error
  - Puedes verificar los DATOS del error
  - Puedes usar ? en tests (ver T03)
  - Puedes componer errores
  - Puedes pattern-match en el error
```

### Ejemplo: verificar tipo de error con Result vs panic

```rust
// Con Result: puedes verificar exactamente qué error
#[derive(Debug, PartialEq)]
pub enum ConfigError {
    FileNotFound(String),
    ParseError { line: usize, message: String },
    MissingKey(String),
}

pub fn load_config(path: &str) -> Result<Config, ConfigError> {
    // ...
}

#[test]
fn test_missing_file() {
    let result = load_config("/nonexistent");
    assert!(matches!(
        result,
        Err(ConfigError::FileNotFound(ref path)) if path == "/nonexistent"
    ));
}

#[test]
fn test_parse_error_on_line() {
    let result = load_config("bad_syntax.toml");
    match result {
        Err(ConfigError::ParseError { line, ref message }) => {
            assert_eq!(line, 5);
            assert!(message.contains("unexpected token"));
        }
        other => panic!("Expected ParseError, got {:?}", other),
    }
}

// Con panic: solo puedes verificar un substring del mensaje
#[test]
#[should_panic(expected = "parse error")]
fn test_parse_error_panic() {
    load_config_panicking("bad_syntax.toml");
    // No puedes verificar en qué línea, ni el tipo de error
}
```

---

## 10. Comparación con C: señales, setjmp, abort

### Cómo C maneja "tests que deben fallar"

En C no hay un mecanismo estándar para capturar fallos. Las opciones son:

```
Mecanismo C              Cómo funciona                        Equivalente Rust
──────────────────────────────────────────────────────────────────────────────────
assert() + abort()       Termina el proceso                   panic! (sin catch)
setjmp/longjmp           Salto no local (peligroso)           catch_unwind
signal(SIGABRT, handler) Interceptar abort()                  panic handler
fork() + waitpid()       Proceso hijo crashea, padre verifica No necesario
Check framework          fork() por test, verifica exit code  #[should_panic]
```

### setjmp/longjmp en C (peligroso)

```c
#include <setjmp.h>
#include <stdio.h>

static jmp_buf jump_buffer;
static int did_panic = 0;

#define TEST_SHOULD_PANIC(code) do { \
    did_panic = 0; \
    if (setjmp(jump_buffer) == 0) { \
        code; \
    } else { \
        did_panic = 1; \
    } \
    assert(did_panic && "Expected panic but code returned normally"); \
} while(0)

// "panic" en C: hacer longjmp
void my_panic(const char *msg) {
    fprintf(stderr, "PANIC: %s\n", msg);
    longjmp(jump_buffer, 1);  // Saltar de vuelta a setjmp
}

void divide(int a, int b) {
    if (b == 0) my_panic("division by zero");
    printf("%d / %d = %d\n", a, b, a / b);
}

int main(void) {
    TEST_SHOULD_PANIC(divide(10, 0));  // Pasa: divide paniquea
    printf("Test passed!\n");
    return 0;
}
```

Problemas con este enfoque en C:
- longjmp no ejecuta destructores (C++ RAII se rompe)
- No limpia la pila correctamente
- Variables locales pueden quedar en estado inconsistente
- No es thread-safe
- No funciona con abort() real (solo con longjmp custom)

### fork() en C (Check framework)

```c
// Check framework usa fork() para aislar cada test
// Si el test hace abort()/SIGSEGV, solo muere el proceso hijo
// El padre (test runner) detecta la señal y reporta FAIL

// En Rust, esto no es necesario porque:
// 1. Los panics son capturables (catch_unwind)
// 2. El ownership system previene muchos crashes
// 3. Cada test corre en su propio thread (no proceso)
```

### Tabla comparativa

```
Aspecto                 C                              Rust
──────────────────────────────────────────────────────────────────────
Capturar fallo         setjmp/longjmp (unsafe)         catch_unwind (safe)
                       fork + waitpid (heavyweight)    #[should_panic] (simple)

Verificar mensaje      Comparar con fprintf buffer     expected = "substring"

Cleanup post-fallo     No automático                   Drop se ejecuta
                       (longjmp salta destructores)     durante unwind

Aislamiento            fork() (proceso separado)       Thread por test
                       o nada

Seguridad              Peligroso (UB posible)          Safe (catch_unwind es safe)

Facilidad              Complejo (framework necesario)  Un atributo (#[should_panic])
```

---

## 11. Errores comunes

### Error 1: should_panic sin expected permite cualquier panic

```rust
#[test]
#[should_panic]  // ← Demasiado permisivo
fn test_division() {
    let data = get_data();       // ← ¿Y si ESTA función paniquea por un bug?
    let result = divide(data, 0.0);  // Queríamos que ESTA paniquee
}
```

**Solución**: siempre usar `expected` para verificar que el panic correcto ocurrió:

```rust
#[test]
#[should_panic(expected = "Division by zero")]
fn test_division() {
    let data = get_data();
    divide(data, 0.0);
}
```

### Error 2: expected demasiado genérico

```rust
#[test]
#[should_panic(expected = "error")]
fn test_parse() {
    parse("invalid input");
    // "error" coincide con CUALQUIER panic que contenga "error"
    // Poca utilidad como verificación
}
```

**Solución**: usar un substring más específico.

### Error 3: confundir should_panic con tests de Result

```rust
pub fn parse(s: &str) -> Result<i32, String> {
    s.parse::<i32>().map_err(|e| e.to_string())
}

#[test]
#[should_panic]
fn test_parse_error() {
    parse("abc");
    // ¡Este test FALLA! parse() retorna Err(...), no hace panic
    // Err no es panic
}

// Correcto:
#[test]
fn test_parse_error() {
    assert!(parse("abc").is_err());
}
```

### Error 4: poner should_panic en test con Result return type

```rust
#[test]
#[should_panic]
fn test_something() -> Result<(), String> {
    // ERROR DE COMPILACIÓN:
    // #[should_panic] tests cannot return Result
    // Elige uno: should_panic O Result return type, no ambos
    Ok(())
}
```

### Error 5: expected con formato incorrecto

```rust
// INCORRECTO: expected no usa comillas correctamente
#[should_panic(expected = 'single quotes')]  // ERROR: debe ser "double quotes"

// INCORRECTO: expected con formato
#[should_panic(expected = format!("error {}", 42))]  // ERROR: no es const

// CORRECTO:
#[should_panic(expected = "error 42")]  // Literal string
```

### Error 6: olvidar que expected es substring, no regex

```rust
// Esto NO es regex:
#[should_panic(expected = "index \\d+ out of bounds")]
// expected es LITERAL: busca la cadena "index \d+ out of bounds"

// Si el panic dice "index 42 out of bounds", NO coincide
// Debes usar un substring literal:
#[should_panic(expected = "out of bounds")]
```

### Error 7: test que paniquea antes de llegar al código que quieres testear

```rust
#[test]
#[should_panic(expected = "overflow")]
fn test_bounded_vec_overflow() {
    let mut v = BoundedVec::new(0);  // ← Panic aquí: "max_size must be positive"
    v.push(1);                        // Nunca se ejecuta
    v.push(2);                        // Nunca se ejecuta (queríamos overflow aquí)
}
// El test PASA pero por la razón equivocada si "positive" contiene... no,
// en este caso FALLA porque el panic dice "positive", no "overflow".
// Pero si el expected fuera genérico, pasaría erróneamente.
```

**Solución**: cuidar que el setup no paniquee, o usar expected específico.

---

## 12. Ejemplo completo: biblioteca de matrices

### `src/lib.rs`

```rust
use std::fmt;

/// Matriz de f64 con dimensiones fijas.
#[derive(Clone, PartialEq)]
pub struct Matrix {
    rows: usize,
    cols: usize,
    data: Vec<f64>,
}

impl Matrix {
    /// Crear una matriz de ceros.
    /// Panic si rows o cols es 0.
    pub fn zeros(rows: usize, cols: usize) -> Self {
        if rows == 0 || cols == 0 {
            panic!(
                "Matrix dimensions must be positive, got {}x{}",
                rows, cols
            );
        }
        Matrix {
            rows,
            cols,
            data: vec![0.0; rows * cols],
        }
    }

    /// Crear una matriz identidad.
    /// Panic si n es 0.
    pub fn identity(n: usize) -> Self {
        if n == 0 {
            panic!("Identity matrix size must be positive");
        }
        let mut m = Matrix::zeros(n, n);
        for i in 0..n {
            m.data[i * n + i] = 1.0;
        }
        m
    }

    /// Crear desde un slice de slices.
    /// Panic si las filas tienen longitudes diferentes.
    pub fn from_rows(rows: &[&[f64]]) -> Self {
        if rows.is_empty() {
            panic!("Cannot create matrix from empty rows");
        }

        let num_cols = rows[0].len();
        if num_cols == 0 {
            panic!("Cannot create matrix with zero columns");
        }

        for (i, row) in rows.iter().enumerate() {
            if row.len() != num_cols {
                panic!(
                    "Row {} has {} elements, expected {} (inconsistent dimensions)",
                    i, row.len(), num_cols
                );
            }
        }

        let data: Vec<f64> = rows.iter().flat_map(|r| r.iter().copied()).collect();

        Matrix {
            rows: rows.len(),
            cols: num_cols,
            data,
        }
    }

    pub fn rows(&self) -> usize {
        self.rows
    }

    pub fn cols(&self) -> usize {
        self.cols
    }

    /// Acceder a un elemento. Panic si fuera de rango.
    pub fn get(&self, row: usize, col: usize) -> f64 {
        if row >= self.rows || col >= self.cols {
            panic!(
                "Matrix index ({}, {}) out of bounds for {}x{} matrix",
                row, col, self.rows, self.cols
            );
        }
        self.data[row * self.cols + col]
    }

    /// Modificar un elemento. Panic si fuera de rango.
    pub fn set(&mut self, row: usize, col: usize, value: f64) {
        if row >= self.rows || col >= self.cols {
            panic!(
                "Matrix index ({}, {}) out of bounds for {}x{} matrix",
                row, col, self.rows, self.cols
            );
        }
        self.data[row * self.cols + col] = value;
    }

    /// Multiplicar dos matrices. Panic si dimensiones incompatibles.
    pub fn multiply(&self, other: &Matrix) -> Matrix {
        if self.cols != other.rows {
            panic!(
                "Cannot multiply {}x{} matrix by {}x{} matrix: \
                 left cols ({}) must equal right rows ({})",
                self.rows, self.cols, other.rows, other.cols,
                self.cols, other.rows
            );
        }

        let mut result = Matrix::zeros(self.rows, other.cols);

        for i in 0..self.rows {
            for j in 0..other.cols {
                let mut sum = 0.0;
                for k in 0..self.cols {
                    sum += self.get(i, k) * other.get(k, j);
                }
                result.set(i, j, sum);
            }
        }

        result
    }

    /// Transponer la matriz.
    pub fn transpose(&self) -> Matrix {
        let mut result = Matrix::zeros(self.cols, self.rows);
        for i in 0..self.rows {
            for j in 0..self.cols {
                result.set(j, i, self.get(i, j));
            }
        }
        result
    }

    /// Sumar dos matrices. Panic si dimensiones diferentes.
    pub fn add(&self, other: &Matrix) -> Matrix {
        if self.rows != other.rows || self.cols != other.cols {
            panic!(
                "Cannot add {}x{} matrix to {}x{} matrix: dimensions must match",
                self.rows, self.cols, other.rows, other.cols
            );
        }

        let data: Vec<f64> = self
            .data
            .iter()
            .zip(other.data.iter())
            .map(|(a, b)| a + b)
            .collect();

        Matrix {
            rows: self.rows,
            cols: self.cols,
            data,
        }
    }

    /// Multiplicar por escalar.
    pub fn scale(&self, scalar: f64) -> Matrix {
        let data: Vec<f64> = self.data.iter().map(|x| x * scalar).collect();
        Matrix {
            rows: self.rows,
            cols: self.cols,
            data,
        }
    }

    /// Determinante (solo para 2x2). Panic si no es 2x2.
    pub fn det_2x2(&self) -> f64 {
        if self.rows != 2 || self.cols != 2 {
            panic!(
                "det_2x2 requires a 2x2 matrix, got {}x{}",
                self.rows, self.cols
            );
        }
        self.data[0] * self.data[3] - self.data[1] * self.data[2]
    }
}

impl fmt::Debug for Matrix {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "Matrix({}x{})", self.rows, self.cols)?;
        for i in 0..self.rows {
            write!(f, "  [")?;
            for j in 0..self.cols {
                if j > 0 {
                    write!(f, ", ")?;
                }
                write!(f, "{:8.3}", self.get(i, j))?;
            }
            writeln!(f, "]")?;
        }
        Ok(())
    }
}

// ════════════════════════════════════════════════════
// TESTS
// ════════════════════════════════════════════════════

#[cfg(test)]
mod tests {
    use super::*;

    // ── Helpers ──

    fn approx_eq(a: f64, b: f64) -> bool {
        (a - b).abs() < 1e-10
    }

    fn assert_matrix_eq(a: &Matrix, b: &Matrix) {
        assert_eq!(a.rows(), b.rows(), "Row count mismatch");
        assert_eq!(a.cols(), b.cols(), "Column count mismatch");
        for i in 0..a.rows() {
            for j in 0..a.cols() {
                assert!(
                    approx_eq(a.get(i, j), b.get(i, j)),
                    "Mismatch at ({}, {}): {} != {}",
                    i, j, a.get(i, j), b.get(i, j)
                );
            }
        }
    }

    // ══════════════════════════════════════════
    // Construcción: tests que DEBEN hacer panic
    // ══════════════════════════════════════════

    #[test]
    #[should_panic(expected = "dimensions must be positive")]
    fn test_zeros_zero_rows() {
        Matrix::zeros(0, 5);
    }

    #[test]
    #[should_panic(expected = "dimensions must be positive")]
    fn test_zeros_zero_cols() {
        Matrix::zeros(5, 0);
    }

    #[test]
    #[should_panic(expected = "dimensions must be positive")]
    fn test_zeros_both_zero() {
        Matrix::zeros(0, 0);
    }

    #[test]
    #[should_panic(expected = "size must be positive")]
    fn test_identity_zero() {
        Matrix::identity(0);
    }

    #[test]
    #[should_panic(expected = "empty rows")]
    fn test_from_rows_empty() {
        let rows: &[&[f64]] = &[];
        Matrix::from_rows(rows);
    }

    #[test]
    #[should_panic(expected = "zero columns")]
    fn test_from_rows_empty_row() {
        let rows: &[&[f64]] = &[&[]];
        Matrix::from_rows(rows);
    }

    #[test]
    #[should_panic(expected = "inconsistent dimensions")]
    fn test_from_rows_inconsistent() {
        Matrix::from_rows(&[&[1.0, 2.0], &[3.0]]);
    }

    // ══════════════════════════════════════════
    // Indexado: tests que DEBEN hacer panic
    // ══════════════════════════════════════════

    #[test]
    #[should_panic(expected = "out of bounds")]
    fn test_get_row_oob() {
        let m = Matrix::zeros(3, 3);
        m.get(3, 0);
    }

    #[test]
    #[should_panic(expected = "out of bounds")]
    fn test_get_col_oob() {
        let m = Matrix::zeros(3, 3);
        m.get(0, 3);
    }

    #[test]
    #[should_panic(expected = "out of bounds for 2x3")]
    fn test_set_oob_with_dimensions() {
        let mut m = Matrix::zeros(2, 3);
        m.set(5, 5, 1.0);
    }

    // ══════════════════════════════════════════
    // Operaciones: tests que DEBEN hacer panic
    // ══════════════════════════════════════════

    #[test]
    #[should_panic(expected = "Cannot multiply")]
    fn test_multiply_incompatible() {
        let a = Matrix::zeros(2, 3);
        let b = Matrix::zeros(4, 5);
        a.multiply(&b);
    }

    #[test]
    #[should_panic(expected = "left cols (3) must equal right rows (4)")]
    fn test_multiply_incompatible_detail() {
        let a = Matrix::zeros(2, 3);
        let b = Matrix::zeros(4, 2);
        a.multiply(&b);
    }

    #[test]
    #[should_panic(expected = "dimensions must match")]
    fn test_add_incompatible() {
        let a = Matrix::zeros(2, 3);
        let b = Matrix::zeros(3, 2);
        a.add(&b);
    }

    #[test]
    #[should_panic(expected = "requires a 2x2 matrix")]
    fn test_det_not_2x2() {
        let m = Matrix::zeros(3, 3);
        m.det_2x2();
    }

    #[test]
    #[should_panic(expected = "got 2x3")]
    fn test_det_not_square() {
        let m = Matrix::zeros(2, 3);
        m.det_2x2();
    }

    // ══════════════════════════════════════════
    // Operaciones: tests normales (no panic)
    // ══════════════════════════════════════════

    #[test]
    fn test_zeros() {
        let m = Matrix::zeros(2, 3);
        assert_eq!(m.rows(), 2);
        assert_eq!(m.cols(), 3);
        for i in 0..2 {
            for j in 0..3 {
                assert_eq!(m.get(i, j), 0.0);
            }
        }
    }

    #[test]
    fn test_identity() {
        let m = Matrix::identity(3);
        assert_eq!(m.get(0, 0), 1.0);
        assert_eq!(m.get(1, 1), 1.0);
        assert_eq!(m.get(2, 2), 1.0);
        assert_eq!(m.get(0, 1), 0.0);
        assert_eq!(m.get(1, 0), 0.0);
    }

    #[test]
    fn test_from_rows() {
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        assert_eq!(m.rows(), 2);
        assert_eq!(m.cols(), 2);
        assert_eq!(m.get(0, 0), 1.0);
        assert_eq!(m.get(0, 1), 2.0);
        assert_eq!(m.get(1, 0), 3.0);
        assert_eq!(m.get(1, 1), 4.0);
    }

    #[test]
    fn test_set_and_get() {
        let mut m = Matrix::zeros(2, 2);
        m.set(0, 0, 42.0);
        m.set(1, 1, 99.0);
        assert_eq!(m.get(0, 0), 42.0);
        assert_eq!(m.get(1, 1), 99.0);
        assert_eq!(m.get(0, 1), 0.0);
    }

    #[test]
    fn test_multiply() {
        // [1 2] * [5 6] = [1*5+2*7  1*6+2*8] = [19 22]
        // [3 4]   [7 8]   [3*5+4*7  3*6+4*8]   [43 50]
        let a = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let b = Matrix::from_rows(&[&[5.0, 6.0], &[7.0, 8.0]]);
        let c = a.multiply(&b);

        let expected = Matrix::from_rows(&[&[19.0, 22.0], &[43.0, 50.0]]);
        assert_matrix_eq(&c, &expected);
    }

    #[test]
    fn test_multiply_identity() {
        let a = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let i = Matrix::identity(2);
        assert_matrix_eq(&a.multiply(&i), &a);
        assert_matrix_eq(&i.multiply(&a), &a);
    }

    #[test]
    fn test_transpose() {
        let m = Matrix::from_rows(&[&[1.0, 2.0, 3.0], &[4.0, 5.0, 6.0]]);
        let t = m.transpose();
        assert_eq!(t.rows(), 3);
        assert_eq!(t.cols(), 2);
        assert_eq!(t.get(0, 0), 1.0);
        assert_eq!(t.get(0, 1), 4.0);
        assert_eq!(t.get(2, 0), 3.0);
        assert_eq!(t.get(2, 1), 6.0);
    }

    #[test]
    fn test_double_transpose_is_identity() {
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0], &[5.0, 6.0]]);
        assert_matrix_eq(&m.transpose().transpose(), &m);
    }

    #[test]
    fn test_add() {
        let a = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let b = Matrix::from_rows(&[&[10.0, 20.0], &[30.0, 40.0]]);
        let c = a.add(&b);
        let expected = Matrix::from_rows(&[&[11.0, 22.0], &[33.0, 44.0]]);
        assert_matrix_eq(&c, &expected);
    }

    #[test]
    fn test_scale() {
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let s = m.scale(3.0);
        let expected = Matrix::from_rows(&[&[3.0, 6.0], &[9.0, 12.0]]);
        assert_matrix_eq(&s, &expected);
    }

    #[test]
    fn test_scale_zero() {
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let z = m.scale(0.0);
        assert_matrix_eq(&z, &Matrix::zeros(2, 2));
    }

    #[test]
    fn test_det_2x2() {
        // det([1 2; 3 4]) = 1*4 - 2*3 = -2
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        assert!(approx_eq(m.det_2x2(), -2.0));
    }

    #[test]
    fn test_det_identity() {
        let i = Matrix::identity(2);
        assert!(approx_eq(i.det_2x2(), 1.0));
    }

    #[test]
    fn test_det_singular() {
        // Matriz singular: filas proporcionales
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[2.0, 4.0]]);
        assert!(approx_eq(m.det_2x2(), 0.0));
    }

    #[test]
    fn test_debug_format() {
        let m = Matrix::from_rows(&[&[1.0, 2.0], &[3.0, 4.0]]);
        let s = format!("{:?}", m);
        assert!(s.contains("Matrix(2x2)"));
        assert!(s.contains("1.000"));
    }
}
```

### Ejecución

```bash
$ cargo test
running 33 tests
test tests::test_add ... ok
test tests::test_add_incompatible - should panic ... ok
test tests::test_debug_format ... ok
test tests::test_det_2x2 ... ok
test tests::test_det_identity ... ok
test tests::test_det_not_2x2 - should panic ... ok
test tests::test_det_not_square - should panic ... ok
test tests::test_det_singular ... ok
test tests::test_double_transpose_is_identity ... ok
test tests::test_from_rows ... ok
test tests::test_from_rows_empty - should panic ... ok
test tests::test_from_rows_empty_row - should panic ... ok
test tests::test_from_rows_inconsistent - should panic ... ok
test tests::test_get_col_oob - should panic ... ok
test tests::test_get_row_oob - should panic ... ok
test tests::test_identity ... ok
test tests::test_identity_zero - should panic ... ok
test tests::test_multiply ... ok
test tests::test_multiply_identity ... ok
test tests::test_multiply_incompatible - should panic ... ok
test tests::test_multiply_incompatible_detail - should panic ... ok
test tests::test_scale ... ok
test tests::test_scale_zero ... ok
test tests::test_set_and_get ... ok
test tests::test_set_oob_with_dimensions - should panic ... ok
test tests::test_transpose ... ok
test tests::test_zeros ... ok
test tests::test_zeros_both_zero - should panic ... ok
test tests::test_zeros_zero_cols - should panic ... ok
test tests::test_zeros_zero_rows - should panic ... ok

test result: ok. 33 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

Nota: 14 tests `should panic` + 19 tests normales = 33 tests. La separación es clara entre "verificar que funciona" y "verificar que falla correctamente".

---

## 13. Programa de práctica

### Enunciado

Implementa un tipo `SafeArray<T>` que sea un array de tamaño fijo con bounds checking:

```rust
pub struct SafeArray<T> {
    // Internamente usa un Vec<T> con capacidad fija
}

impl<T: Clone + Default> SafeArray<T> {
    /// Crear con tamaño fijo. Panic si size == 0.
    pub fn new(size: usize) -> Self;

    /// Obtener elemento. Panic si fuera de rango.
    pub fn get(&self, index: usize) -> &T;

    /// Modificar elemento. Panic si fuera de rango.
    pub fn set(&mut self, index: usize, value: T);

    /// Tamaño del array.
    pub fn len(&self) -> usize;

    /// Intercambiar dos elementos. Panic si cualquier index fuera de rango.
    pub fn swap(&mut self, i: usize, j: usize);

    /// Llenar todo el array con un valor.
    pub fn fill(&mut self, value: T);

    /// Crear desde un slice. Panic si el slice está vacío.
    pub fn from_slice(data: &[T]) -> Self;
}
```

### Tareas

1. Implementa `SafeArray<T>`
2. Escribe tests normales para cada método (al menos 10 tests)
3. Escribe tests `#[should_panic]` para cada caso de panic (al menos 7 tests):
   - `new(0)` → panic
   - `get` fuera de rango → panic
   - `set` fuera de rango → panic
   - `swap` con primer index fuera de rango → panic
   - `swap` con segundo index fuera de rango → panic
   - `from_slice` con slice vacío → panic
4. Usa `expected` en todos los `#[should_panic]` con substrings específicos
5. Usa `catch_unwind` para verificar que `swap(i, j)` paniquea con el index correcto en el mensaje
6. Verifica que `cargo test` pasa todos los tests

---

## 14. Ejercicios

### Ejercicio 1: should_panic básico (10 min)

Implementa:

```rust
pub fn nth_fibonacci(n: u32) -> u64 {
    // Panic si n > 93 (overflow de u64)
    // Retorna el n-ésimo número de Fibonacci
}
```

**Tareas**:
- a) Implementa la función con panic explícito para n > 93
- b) Escribe 3 tests normales (n=0, n=10, n=93)
- c) Escribe 1 test `#[should_panic(expected = "...")]` para n=94
- d) ¿Qué pasa si no pones expected y el panic viene de un overflow de u64 en vez de tu check? Demuéstralo

### Ejercicio 2: expected como documentación (15 min)

Dado:

```rust
pub struct Config {
    // Campos privados
}

impl Config {
    pub fn new(port: u16, host: &str, max_connections: usize) -> Self {
        // Panics posibles:
        // - port == 0: "Port must be non-zero"
        // - host vacío: "Host cannot be empty"
        // - max_connections == 0: "max_connections must be positive"
        // - max_connections > 10000: "max_connections exceeds limit of 10000"
    }
}
```

**Tareas**:
- a) Implementa `Config::new` con los 4 panics documentados
- b) Escribe 4 tests `#[should_panic]` con expected, uno por cada panic
- c) Escribe 1 test normal que verifique que una configuración válida se crea correctamente
- d) Añade un 5to panic: si host contiene espacios → "Host cannot contain whitespace"
- e) Escribe el test para el nuevo panic

### Ejercicio 3: catch_unwind avanzado (20 min)

**Tareas**:
- a) Implementa un helper `assert_panics<F: FnOnce() + UnwindSafe>(f: F) -> String` que:
  - Ejecuta `f` con `catch_unwind`
  - Si no hay panic, falla el test con un mensaje claro
  - Si hay panic, retorna el mensaje como String
- b) Usa el helper para escribir tests que verifiquen:
  - El mensaje exacto del panic (no substring)
  - Que el mensaje contiene ciertos datos dinámicos
  - Que dos funciones diferentes producen mensajes diferentes
- c) Escribe un test que use `catch_unwind` para verificar estado parcial: una función que modifica un vector y luego paniquea — verifica que las modificaciones parciales se hicieron

### Ejercicio 4: panic vs Result — rediseñar API (20 min)

Dado:

```rust
pub fn parse_date(s: &str) -> (u32, u32, u32) {
    // Formato esperado: "YYYY-MM-DD"
    // Panic si el formato es inválido
    // Panic si el mes no está en 1-12
    // Panic si el día no está en 1-31
}
```

**Tareas**:
- a) Implementa `parse_date` con panics y escribe 3 tests `#[should_panic]`
- b) Rediseña como `parse_date_safe(s: &str) -> Result<(u32, u32, u32), DateError>` con un enum `DateError`
- c) Escribe los mismos tests pero usando `assert_eq!` y `assert!(matches!(...))` en vez de `#[should_panic]`
- d) Compara ambas versiones: ¿cuál produce tests más informativos cuando fallan? ¿Cuál es más fácil de mantener?
- e) ¿En qué caso usarías la versión con panic? ¿Y en cuál la versión con Result?

---

**Navegación**:
- ← Anterior: [T01 — Unit tests con #[test]](../T01_Unit_tests_con_test/README.md)
- → Siguiente: [T03 — Tests con Result<(), E>](../T03_Tests_con_Result/README.md)
- ↑ Sección: [S01 — Testing Built-in](../README.md)
