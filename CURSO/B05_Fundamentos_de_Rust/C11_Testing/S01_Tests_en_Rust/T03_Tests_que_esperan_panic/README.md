# Tests que esperan panic: #[should_panic] y expected message

## Índice
1. [Por qué testear panics](#1-por-qué-testear-panics)
2. [#[should_panic] básico](#2-should_panic-básico)
3. [expected: verificar el mensaje](#3-expected-verificar-el-mensaje)
4. [Alternativa: std::panic::catch_unwind](#4-alternativa-stdpaniccatch_unwind)
5. [Panic vs Result: qué testear con cada uno](#5-panic-vs-result-qué-testear-con-cada-uno)
6. [Patrones avanzados](#6-patrones-avanzados)
7. [Errores comunes](#7-errores-comunes)
8. [Cheatsheet](#8-cheatsheet)
9. [Ejercicios](#9-ejercicios)

---

## 1. Por qué testear panics

Algunas funciones **deben** hacer panic bajo ciertas condiciones. En esos casos, el test correcto verifica que el panic ocurre:

```rust
pub fn divide(a: i32, b: i32) -> i32 {
    if b == 0 {
        panic!("division by zero");
    }
    a / b
}

// Queremos verificar que divide(10, 0) efectivamente hace panic
// Un test normal fallaría (panic = test fallido)
// Necesitamos decirle a cargo test: "este panic es esperado"
```

Casos comunes donde el panic es el comportamiento correcto:

- **Precondiciones violadas**: índice fuera de rango, argumento inválido.
- **Invariantes rotos**: estado interno corrupto que no debería ser posible.
- **Constructores con validación**: `fn new(size: usize)` donde `size == 0` es inválido.
- **Operaciones que documentan panic**: como `Vec::remove` con índice fuera de rango.

---

## 2. #[should_panic] básico

El atributo `#[should_panic]` invierte la lógica del test: **pasa si la función hace panic, falla si no**:

```rust
pub fn factorial(n: i64) -> i64 {
    if n < 0 {
        panic!("factorial of negative number");
    }
    (1..=n).product()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_factorial_normal() {
        assert_eq!(factorial(5), 120);
    }

    #[test]
    #[should_panic]
    fn test_factorial_negative() {
        factorial(-1);  // debe hacer panic
    }
}
```

### Cómo funciona

```rust
#[test]
#[should_panic]
fn test_panics() {
    panic!("boom");
    // ✅ PASA: el panic es esperado
}

#[test]
#[should_panic]
fn test_no_panic() {
    let x = 2 + 2;
    // ❌ FALLA: se esperaba panic pero no ocurrió
}
```

Output cuando falla (porque no hubo panic):

```
test tests::test_no_panic ... FAILED

failures:

---- tests::test_no_panic stdout ----
note: test did not panic as expected

failures:
    tests::test_no_panic
```

### Ejemplos prácticos

```rust
pub struct BoundedBuffer {
    data: Vec<u8>,
    max_size: usize,
}

impl BoundedBuffer {
    pub fn new(max_size: usize) -> Self {
        assert!(max_size > 0, "Buffer size must be positive");
        BoundedBuffer {
            data: Vec::new(),
            max_size,
        }
    }

    pub fn push(&mut self, byte: u8) {
        if self.data.len() >= self.max_size {
            panic!("Buffer overflow: max size is {}", self.max_size);
        }
        self.data.push(byte);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic]
    fn test_zero_size_buffer() {
        BoundedBuffer::new(0);
    }

    #[test]
    #[should_panic]
    fn test_buffer_overflow() {
        let mut buf = BoundedBuffer::new(2);
        buf.push(1);
        buf.push(2);
        buf.push(3);  // overflow → panic
    }

    #[test]
    fn test_buffer_within_limits() {
        let mut buf = BoundedBuffer::new(2);
        buf.push(1);
        buf.push(2);
        // no panic → test pasa normalmente
    }
}
```

---

## 3. expected: verificar el mensaje

`#[should_panic]` sin más acepta **cualquier** panic. Esto es peligroso — el test podría pasar por un panic distinto al esperado:

```rust
#[test]
#[should_panic]
fn test_too_broad() {
    let v: Vec<i32> = vec![];
    // Queremos verificar que nuestra función hace panic...
    // pero v[0] hace panic primero con "index out of bounds"
    // y el test pasa por la razón equivocada
    let _ = v[0];
    our_function_that_should_panic();
}
```

### expected = "substring"

Para verificar que el panic tiene el mensaje correcto, usa `expected`:

```rust
pub fn divide(a: i32, b: i32) -> i32 {
    if b == 0 {
        panic!("division by zero: cannot divide {} by 0", a);
    }
    a / b
}

#[test]
#[should_panic(expected = "division by zero")]
fn test_divide_by_zero() {
    divide(10, 0);
}
```

`expected` verifica que el mensaje de panic **contiene** el substring especificado. No necesita ser el mensaje completo:

```rust
// El panic dice: "division by zero: cannot divide 10 by 0"

#[test]
#[should_panic(expected = "division by zero")]  // ✅ substring encontrado
fn test_a() { divide(10, 0); }

#[test]
#[should_panic(expected = "cannot divide")]     // ✅ substring encontrado
fn test_b() { divide(10, 0); }

#[test]
#[should_panic(expected = "division by zero: cannot divide 10 by 0")]  // ✅ exacto
fn test_c() { divide(10, 0); }

#[test]
#[should_panic(expected = "overflow")]  // ❌ FALLA: "overflow" no está en el mensaje
fn test_d() { divide(10, 0); }
```

Output cuando el expected no coincide:

```
test tests::test_d ... FAILED

failures:

---- tests::test_d stdout ----
thread 'tests::test_d' panicked at 'division by zero: cannot divide 10 by 0'
note: panic did not contain expected string
      panic message: `"division by zero: cannot divide 10 by 0"`,
 expected substring: `"overflow"`
```

### Buena práctica: siempre usar expected

```rust
// EVITAR: acepta cualquier panic
#[test]
#[should_panic]
fn test_vague() {
    risky_operation();
}

// PREFERIR: verifica el panic específico
#[test]
#[should_panic(expected = "invalid input")]
fn test_specific() {
    risky_operation();
}
```

Sin `expected`, un bug que cause un panic diferente (como un index out of bounds accidental) haría que el test pase erróneamente.

---

## 4. Alternativa: std::panic::catch_unwind

Para verificaciones más complejas que un simple substring, puedes capturar el panic con `std::panic::catch_unwind`:

```rust
use std::panic;

#[test]
fn test_panic_with_catch() {
    let result = panic::catch_unwind(|| {
        divide(10, 0);
    });

    // Verificar que hubo panic
    assert!(result.is_err());

    // Extraer y verificar el mensaje
    let err = result.unwrap_err();
    let message = err.downcast_ref::<String>()
        .map(|s| s.as_str())
        .or_else(|| err.downcast_ref::<&str>().copied())
        .unwrap_or("unknown panic");

    assert!(message.contains("division by zero"));
    assert!(message.contains("10"));
}
```

### Cuándo usar catch_unwind vs should_panic

```rust
// CASO 1: verificar que hay panic con un mensaje → #[should_panic(expected)]
#[test]
#[should_panic(expected = "out of bounds")]
fn simple_case() {
    vec![1, 2, 3][10];
}

// CASO 2: verificar el tipo del panic payload → catch_unwind
#[test]
fn complex_case() {
    let result = std::panic::catch_unwind(|| {
        custom_panic_function();
    });
    let err = result.unwrap_err();

    // Verificar que el panic payload es de un tipo específico
    assert!(err.downcast_ref::<MyCustomError>().is_some());
}

// CASO 3: verificar que NO hay panic → función normal + assert
#[test]
fn should_not_panic() {
    let result = std::panic::catch_unwind(|| {
        potentially_dangerous(valid_input);
    });
    assert!(result.is_ok(), "Should not panic with valid input");
}

// CASO 4: verificar panic Y el estado después → catch_unwind
#[test]
fn test_state_after_panic() {
    let shared = std::sync::Arc::new(std::sync::Mutex::new(0));
    let shared_clone = shared.clone();

    let result = std::panic::catch_unwind(move || {
        let mut val = shared_clone.lock().unwrap();
        *val = 42;
        panic!("intentional");
    });

    assert!(result.is_err());
    // El mutex puede estar "poisoned" después del panic
    let val = shared.lock();
    assert!(val.is_err());  // mutex poisoned
}
```

### Limitaciones de catch_unwind

```rust
// catch_unwind requiere UnwindSafe
use std::panic::{catch_unwind, AssertUnwindSafe};

let mut data = vec![1, 2, 3];

// ERROR: &mut Vec no es UnwindSafe
// catch_unwind(|| { data.push(4); });

// Solución: AssertUnwindSafe (tú garantizas que es seguro)
let result = catch_unwind(AssertUnwindSafe(|| {
    data.push(4);
    panic!("oops");
}));
// Cuidado: data puede estar en estado inconsistente después del panic
```

---

## 5. Panic vs Result: qué testear con cada uno

### Funciones que deberían usar panic

```rust
// Precondiciones que son BUG del programador (no input del usuario):
pub fn get_unchecked(slice: &[i32], index: usize) -> i32 {
    slice[index]  // panic si index >= len → bug del caller
}

// Invariantes que nunca deberían romperse:
pub fn process(data: &ValidatedData) {
    assert!(data.is_valid(), "BUG: data should have been validated");
    // ...
}

// Constructores con restricciones fundamentales:
pub struct NonEmpty<T> {
    items: Vec<T>,
}
impl<T> NonEmpty<T> {
    pub fn new(items: Vec<T>) -> Self {
        assert!(!items.is_empty(), "NonEmpty requires at least one element");
        NonEmpty { items }
    }
}
```

Testear con `#[should_panic]`:

```rust
#[test]
#[should_panic(expected = "index out of bounds")]
fn test_out_of_bounds() {
    get_unchecked(&[1, 2, 3], 10);
}

#[test]
#[should_panic(expected = "at least one element")]
fn test_empty_nonempty() {
    NonEmpty::<i32>::new(vec![]);
}
```

### Funciones que deberían usar Result

```rust
// Input del usuario o datos externos:
pub fn parse_port(s: &str) -> Result<u16, String> {
    s.parse::<u16>().map_err(|e| format!("Invalid port: {}", e))
}

// Operaciones que pueden fallar legítimamente:
pub fn read_config(path: &str) -> Result<Config, io::Error> {
    let content = fs::read_to_string(path)?;
    // ...
}
```

Testear con assert sobre el Result:

```rust
#[test]
fn test_invalid_port() {
    let result = parse_port("abc");
    assert!(result.is_err());
    assert!(result.unwrap_err().contains("Invalid port"));
}

#[test]
fn test_valid_port() {
    assert_eq!(parse_port("8080"), Ok(8080));
}
```

### La regla

> Si el error es **culpa del programador** (bug) → panic → testear con `#[should_panic]`.
> Si el error es **esperable en runtime** (input, I/O, red) → `Result` → testear con `assert!` sobre el `Result`.

---

## 6. Patrones avanzados

### Testear panic en código genérico

```rust
pub struct Stack<T> {
    items: Vec<T>,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { items: Vec::new() }
    }

    pub fn push(&mut self, item: T) {
        self.items.push(item);
    }

    pub fn pop(&mut self) -> T {
        self.items.pop().expect("Cannot pop from empty stack")
    }

    pub fn peek(&self) -> &T {
        self.items.last().expect("Cannot peek at empty stack")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic(expected = "Cannot pop from empty stack")]
    fn test_pop_empty() {
        let mut stack: Stack<i32> = Stack::new();
        stack.pop();
    }

    #[test]
    #[should_panic(expected = "Cannot peek at empty stack")]
    fn test_peek_empty() {
        let stack: Stack<String> = Stack::new();
        stack.peek();
    }

    #[test]
    fn test_pop_after_push() {
        let mut stack = Stack::new();
        stack.push(42);
        assert_eq!(stack.pop(), 42);
    }
}
```

### Testear panic con múltiples condiciones

```rust
pub fn matrix_multiply(a: &[Vec<f64>], b: &[Vec<f64>]) -> Vec<Vec<f64>> {
    if a.is_empty() || b.is_empty() {
        panic!("matrices must not be empty");
    }
    let cols_a = a[0].len();
    let rows_b = b.len();
    if cols_a != rows_b {
        panic!(
            "incompatible dimensions: {}x{} * {}x{}",
            a.len(), cols_a, rows_b, b[0].len()
        );
    }
    // ... multiplicación real
    todo!()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic(expected = "must not be empty")]
    fn test_empty_matrix_a() {
        matrix_multiply(&[], &[vec![1.0]]);
    }

    #[test]
    #[should_panic(expected = "must not be empty")]
    fn test_empty_matrix_b() {
        matrix_multiply(&[vec![1.0]], &[]);
    }

    #[test]
    #[should_panic(expected = "incompatible dimensions")]
    fn test_incompatible_dimensions() {
        let a = vec![vec![1.0, 2.0]];          // 1x2
        let b = vec![vec![1.0], vec![2.0], vec![3.0]];  // 3x1
        matrix_multiply(&a, &b);  // 2 ≠ 3 → panic
    }
}
```

### Macro para generar tests de panic

```rust
macro_rules! panic_tests {
    ($($name:ident: $expr:expr => $expected:expr),* $(,)?) => {
        $(
            #[test]
            #[should_panic(expected = $expected)]
            fn $name() {
                $expr;
            }
        )*
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    panic_tests! {
        empty_stack_pop:  Stack::<i32>::new().pop()  => "Cannot pop from empty",
        empty_stack_peek: Stack::<i32>::new().peek() => "Cannot peek at empty",
        div_by_zero:      divide(10, 0)              => "division by zero",
        negative_sqrt:    sqrt(-1.0)                  => "negative number",
    }
}
```

---

## 7. Errores comunes

### Error 1: should_panic sin expected acepta cualquier panic

```rust
#[test]
#[should_panic]  // ← demasiado amplio
fn test_validation() {
    let data = get_test_data();  // ← puede hacer panic aquí por un bug
    validate(data);              // ← queríamos verificar que ESTE panic ocurre
}
```

El test pasa aunque el panic venga de `get_test_data()` en vez de `validate()`. Usa `expected` para ser preciso.

### Error 2: expected con el mensaje exacto incorrecto

```rust
pub fn check(x: i32) {
    assert!(x > 0, "Value must be positive, got {}", x);
}

#[test]
#[should_panic(expected = "Value must be positive, got -5")]
fn test_check() {
    check(-5);
    // ✅ pasa: el panic dice exactamente "Value must be positive, got -5"
}

#[test]
#[should_panic(expected = "Value must be positive, got -3")]
fn test_check_wrong_number() {
    check(-5);
    // ❌ FALLA: el panic dice "got -5", no "got -3"
}
```

Usa substrings genéricos que no dependan del valor exacto:

```rust
#[test]
#[should_panic(expected = "Value must be positive")]
fn test_check_robust() {
    check(-5);
    // ✅ funciona con cualquier número negativo
}
```

### Error 3: Testear panic cuando Result sería más apropiado

```rust
// MALO: la función hace panic con input inválido del usuario
pub fn parse_age(s: &str) -> u32 {
    s.parse().expect("Invalid age")  // panic con input del usuario
}

#[test]
#[should_panic(expected = "Invalid age")]
fn test_invalid_age() {
    parse_age("abc");
}

// MEJOR: la función retorna Result
pub fn parse_age(s: &str) -> Result<u32, String> {
    s.parse().map_err(|_| format!("Invalid age: {}", s))
}

#[test]
fn test_invalid_age() {
    assert!(parse_age("abc").is_err());
}
```

Antes de escribir `#[should_panic]`, pregúntate: ¿esta función *debería* hacer panic, o debería retornar `Result`?

### Error 4: should_panic con tests que retornan Result

```rust
// ERROR: no puedes combinar should_panic con Result
#[test]
#[should_panic]
fn test_conflicting() -> Result<(), String> {
    // ¿Cómo maneja cargo test esto?
    // Si retorna Err → ¿es un fallo o un "panic esperado"?
    Err("error".into())
}
// ERROR de compilación o comportamiento indefinido
```

`#[should_panic]` y `-> Result` son mutuamente excluyentes. Usa uno u otro.

### Error 5: No testear el caso donde NO debería haber panic

```rust
// Solo testeas el caso de panic:
#[test]
#[should_panic(expected = "empty")]
fn test_pop_empty() {
    Stack::<i32>::new().pop();
}

// ¡Pero no verificas que pop funciona normalmente!
// Siempre acompaña con el caso positivo:
#[test]
fn test_pop_normal() {
    let mut stack = Stack::new();
    stack.push(42);
    assert_eq!(stack.pop(), 42);  // no panic, valor correcto
}
```

---

## 8. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                    TESTS QUE ESPERAN PANIC                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  BÁSICO                                                            ║
║  #[test]                                                           ║
║  #[should_panic]                Pasa si hay panic (cualquiera)     ║
║  fn test() { panic_fn(); }                                         ║
║                                                                    ║
║  CON EXPECTED                                                      ║
║  #[test]                                                           ║
║  #[should_panic(expected = "substring")]                           ║
║  fn test() { panic_fn(); }     Pasa si panic contiene substring   ║
║                                                                    ║
║  CATCH_UNWIND                                                      ║
║  let r = std::panic::catch_unwind(|| { panic_fn(); });             ║
║  assert!(r.is_err());          Captura panic para análisis         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CUÁNDO USAR                                                       ║
║  ✓ Precondiciones: assert! en constructores                        ║
║  ✓ Invariantes: estados que nunca deberían ocurrir                 ║
║  ✓ Documentación: la función documenta "panics if..."              ║
║  ✓ Index bounds: slice[i] fuera de rango                           ║
║                                                                    ║
║  CUÁNDO NO USAR                                                    ║
║  ✗ Input del usuario → usar Result + assert!(result.is_err())      ║
║  ✗ Errores de I/O → usar Result                                    ║
║  ✗ Cualquier error recuperable → usar Result                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS                                                            ║
║  1. Siempre usar expected para evitar falsos positivos             ║
║  2. No combinar #[should_panic] con -> Result                      ║
║  3. Usar substrings genéricos (no depender de valores dinámicos)   ║
║  4. Acompañar cada test de panic con un test de caso normal        ║
║  5. catch_unwind para verificaciones complejas del panic           ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 9. Ejercicios

### Ejercicio 1: should_panic básico

Escribe la función y los tests correspondientes:

```rust
/// Returns the element at index, panics if out of bounds.
///
/// # Panics
/// Panics if `index >= slice.len()` with message "index {index} out of bounds for length {len}".
pub fn safe_get(slice: &[i32], index: usize) -> i32 {
    // Implementa: si index es válido retorna el valor,
    // si no, panic con el mensaje documentado
    todo!()
}

#[cfg(test)]
mod tests {
    use super::*;

    // Escribe:
    // 1. Test que verifica que safe_get funciona con índice válido
    // 2. Test should_panic para índice = len (boundary)
    // 3. Test should_panic para índice muy grande
    // 4. Test should_panic para slice vacío
    // 5. Todos los should_panic deben usar expected
}
```

### Ejercicio 2: Distinguir panic de Result

Para cada función, decide si debería usar panic o Result. Implementa la función y escribe tests apropiados (`#[should_panic]` o `assert!(result.is_err())`):

```rust
// A) Crear un tablero de ajedrez NxN donde N debe ser > 0
//    ¿Panic o Result? ¿Por qué?
pub fn create_board(size: usize) -> ??? {
    todo!()
}

// B) Leer un archivo de configuración que puede no existir
//    ¿Panic o Result? ¿Por qué?
pub fn load_config(path: &str) -> ??? {
    todo!()
}

// C) Acceder al primer elemento de un Vec que tú construiste
//    y que SABES que no está vacío (invariante interno)
//    ¿Panic o Result? ¿Por qué?
fn get_first_internal(data: &[i32]) -> ??? {
    todo!()
}

// D) Convertir un string que el usuario escribió a un número
//    ¿Panic o Result? ¿Por qué?
pub fn user_input_to_number(input: &str) -> ??? {
    todo!()
}
```

### Ejercicio 3: catch_unwind avanzado

Escribe una función `run_safely` que ejecute un closure y retorne información sobre si hubo panic:

```rust
pub enum RunResult<T> {
    Ok(T),
    Panicked(String),  // mensaje del panic
}

pub fn run_safely<F, T>(f: F) -> RunResult<T>
where
    F: FnOnce() -> T + std::panic::UnwindSafe,
{
    todo!()
    // Usa std::panic::catch_unwind
    // Si Ok → RunResult::Ok(valor)
    // Si Err → extrae el mensaje del panic payload
    //   - Puede ser &str o String
    //   - Si no es ninguno, usa "unknown panic"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_no_panic() {
        match run_safely(|| 42) {
            RunResult::Ok(v) => assert_eq!(v, 42),
            RunResult::Panicked(_) => panic!("should not panic"),
        }
    }

    #[test]
    fn test_with_panic_str() {
        match run_safely(|| panic!("boom")) {
            RunResult::Ok(_) => panic!("should have panicked"),
            RunResult::Panicked(msg) => assert!(msg.contains("boom")),
        }
    }

    #[test]
    fn test_with_panic_format() {
        match run_safely(|| panic!("error: {}", 42)) {
            RunResult::Panicked(msg) => assert!(msg.contains("42")),
            _ => panic!("should have panicked"),
        }
    }

    // Extra: ¿qué pasa con panic!(42) (panic con un número)?
    // ¿Tu implementación lo maneja?
}
```

---

> **Nota**: `#[should_panic]` es una herramienta sencilla pero con un propósito claro: verificar que tu código falla *correctamente* ante condiciones que jamás deberían ocurrir en uso normal. Úsala con moderación — si te encuentras escribiendo muchos tests con `#[should_panic]`, probablemente tus funciones deberían retornar `Result` en vez de hacer panic.
