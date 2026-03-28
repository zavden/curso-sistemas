# Unit tests: #[test], #[cfg(test)], assert!

## Índice
1. [Testing en Rust: filosofía](#1-testing-en-rust-filosofía)
2. [Tu primer test](#2-tu-primer-test)
3. [El atributo #[test]](#3-el-atributo-test)
4. [assert!, assert_eq!, assert_ne!](#4-assert-assert_eq-assert_ne)
5. [El módulo #[cfg(test)]](#5-el-módulo-cfgtest)
6. [Tests con Result](#6-tests-con-result)
7. [Ejecutar tests con cargo test](#7-ejecutar-tests-con-cargo-test)
8. [Filtrar y organizar tests](#8-filtrar-y-organizar-tests)
9. [Test helpers y fixtures](#9-test-helpers-y-fixtures)
10. [Testing de funciones privadas](#10-testing-de-funciones-privadas)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Testing en Rust: filosofía

Rust tiene soporte de testing **integrado en el lenguaje y en Cargo**. No necesitas frameworks externos ni configuración especial — `cargo test` funciona desde el primer momento:

```bash
cargo new mi_proyecto --lib
cd mi_proyecto
cargo test
# running 1 test
# test tests::it_works ... ok
```

El template por defecto ya incluye un test. Rust trata los tests como ciudadanos de primera clase:

- **Unit tests**: junto al código que testean, en el mismo archivo.
- **Integration tests**: en el directorio `tests/`, testean la API pública.
- **Doc tests**: ejemplos en la documentación que se ejecutan como tests.
- **Benchmarks**: medición de rendimiento (nightly o con criterion).

En este tópico nos centramos en **unit tests**.

---

## 2. Tu primer test

```rust
// src/lib.rs
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_add_positive() {
        assert_eq!(add(2, 3), 5);
    }

    #[test]
    fn test_add_negative() {
        assert_eq!(add(-1, -1), -2);
    }

    #[test]
    fn test_add_zero() {
        assert_eq!(add(0, 0), 0);
    }
}
```

```bash
$ cargo test
running 3 tests
test tests::test_add_positive ... ok
test tests::test_add_negative ... ok
test tests::test_add_zero ... ok

test result: ok. 3 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

### Anatomía

1. `#[cfg(test)]` — el módulo solo se compila cuando ejecutas `cargo test`.
2. `mod tests` — convención de nombre (puede ser cualquier nombre).
3. `use super::*` — importa todo del módulo padre para acceder a las funciones.
4. `#[test]` — marca una función como test.
5. `assert_eq!` — verifica igualdad, muestra ambos valores si falla.

---

## 3. El atributo #[test]

`#[test]` marca una función como test. Las reglas:

```rust
// Un test es una función sin argumentos que no retorna (o retorna Result)
#[test]
fn test_basic() {
    // Si no hace panic → pasa
    // Si hace panic → falla
    assert!(true);
}
```

### Un test pasa si no hace panic

```rust
#[test]
fn this_passes() {
    let x = 2 + 2;
    // No panic → test pasa
    // No necesitas assert — pero sin él no verificas nada útil
}

#[test]
fn this_also_passes() {
    assert_eq!(4, 4);
    // assert_eq! no hace panic si son iguales → test pasa
}

#[test]
fn this_fails() {
    assert_eq!(4, 5);
    // assert_eq! hace panic porque 4 ≠ 5 → test falla
}
```

### Output de un test fallido

```
test tests::this_fails ... FAILED

failures:

---- tests::this_fails stdout ----
thread 'tests::this_fails' panicked at 'assertion `left == right` failed
  left: 4
 right: 5', src/lib.rs:15:9

failures:
    tests::this_fails

test result: FAILED. 2 passed; 1 failed; 0 ignored
```

El output muestra:
- Qué test falló.
- Los valores de ambos lados.
- El archivo y línea exactos.

---

## 4. assert!, assert_eq!, assert_ne!

### assert!

Verifica que una expresión booleana sea `true`:

```rust
#[test]
fn test_contains() {
    let text = "hello world";
    assert!(text.contains("world"));
    assert!(!text.is_empty());
}

// Con mensaje personalizado:
#[test]
fn test_range() {
    let value = 42;
    assert!(
        value >= 0 && value <= 100,
        "Value {} should be between 0 and 100",
        value
    );
}
```

### assert_eq!

Verifica que dos valores sean iguales. Requiere `PartialEq + Debug`:

```rust
#[test]
fn test_parse() {
    let result: i32 = "42".parse().unwrap();
    assert_eq!(result, 42);

    // Con mensaje:
    assert_eq!(result, 42, "Parsing '42' should return 42, got {}", result);
}
```

Al fallar, muestra ambos valores:

```
assertion `left == right` failed
  left: 41
 right: 42
```

### assert_ne!

Verifica que dos valores sean **distintos**:

```rust
#[test]
fn test_unique_ids() {
    let id1 = generate_id();
    let id2 = generate_id();
    assert_ne!(id1, id2, "IDs should be unique");
}
```

### Comparaciones avanzadas

Para verificaciones más complejas, usa `assert!` con la expresión completa:

```rust
#[test]
fn test_approximate() {
    let result = 0.1 + 0.2;
    // assert_eq! falla por precisión de floats:
    // assert_eq!(result, 0.3);  // FALLA: 0.30000000000000004 ≠ 0.3

    // Usa tolerancia:
    assert!((result - 0.3).abs() < f64::EPSILON * 4.0,
            "Expected ~0.3, got {}", result);
}

#[test]
fn test_starts_with() {
    let msg = format!("Error: {}", "not found");
    assert!(msg.starts_with("Error:"), "Expected error message, got: {}", msg);
}

#[test]
fn test_vector_sorted() {
    let mut v = vec![3, 1, 4, 1, 5];
    v.sort();
    assert!(v.windows(2).all(|w| w[0] <= w[1]),
            "Vector should be sorted: {:?}", v);
}
```

---

## 5. El módulo #[cfg(test)]

`#[cfg(test)]` es un atributo de compilación condicional que significa "solo compilar esto cuando se ejecuta `cargo test`":

```rust
// src/lib.rs
pub fn factorial(n: u64) -> u64 {
    (1..=n).product()
}

// Este módulo NO existe en el binario final
// Solo se compila con cargo test
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_factorial_zero() {
        assert_eq!(factorial(0), 1);
    }

    #[test]
    fn test_factorial_five() {
        assert_eq!(factorial(5), 120);
    }
}
```

### ¿Por qué #[cfg(test)]?

1. **No infla el binario**: los tests y sus helpers no se incluyen en release.
2. **Dependencias de test**: puedes importar crates de test sin afectar el binario.
3. **Acceso a funciones privadas**: el módulo de tests está *dentro* del módulo padre, así que puede acceder a items privados.

### Qué incluir en #[cfg(test)]

```rust
pub fn process(data: &str) -> Vec<String> {
    data.lines()
        .map(|line| line.trim().to_uppercase())
        .filter(|line| !line.is_empty())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    // Test helpers — solo existen en test
    fn sample_data() -> &'static str {
        "  hello  \n  world  \n\n  rust  "
    }

    #[test]
    fn test_process_basic() {
        let result = process(sample_data());
        assert_eq!(result, vec!["HELLO", "WORLD", "RUST"]);
    }

    #[test]
    fn test_process_empty() {
        let result = process("");
        assert!(result.is_empty());
    }

    #[test]
    fn test_process_whitespace_only() {
        let result = process("   \n  \n   ");
        assert!(result.is_empty());
    }
}
```

### #[cfg(test)] fuera del módulo de tests

También puedes usar `#[cfg(test)]` en items individuales:

```rust
// Helper que solo existe en test mode
#[cfg(test)]
fn create_test_database() -> Database {
    Database::in_memory()
}

// Implementación alternativa solo para tests
#[cfg(test)]
impl MyStruct {
    fn mock() -> Self {
        MyStruct { data: vec![] }
    }
}
```

---

## 6. Tests con Result

En lugar de `assert!` + panic, los tests pueden retornar `Result`:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_config() -> Result<(), String> {
        let config = parse("port=8080").map_err(|e| format!("{}", e))?;
        assert_eq!(config.port, 8080);
        Ok(())
    }

    #[test]
    fn test_file_read() -> Result<(), Box<dyn std::error::Error>> {
        let content = std::fs::read_to_string("test_data.txt")?;
        assert!(content.contains("expected"));
        Ok(())
    }
}
```

### Ventajas de Result en tests

```rust
// CON ASSERT (panic en cada error):
#[test]
fn test_complex() {
    let data = read_file("data.json").unwrap();  // panic si falla
    let parsed = parse_json(&data).unwrap();     // panic si falla
    let result = process(parsed).unwrap();       // panic si falla
    assert_eq!(result.status, "ok");
}

// CON RESULT (propagación con ?):
#[test]
fn test_complex() -> Result<(), Box<dyn std::error::Error>> {
    let data = read_file("data.json")?;          // propaga el error
    let parsed = parse_json(&data)?;             // limpio y claro
    let result = process(parsed)?;               // sin unwrap
    assert_eq!(result.status, "ok");
    Ok(())
}
```

Un test que retorna `Result` falla si retorna `Err`. El mensaje de error se muestra automáticamente.

---

## 7. Ejecutar tests con cargo test

### Comandos básicos

```bash
# Ejecutar todos los tests
cargo test

# Ejecutar con output (println! de tests que pasan)
cargo test -- --show-output

# Ejecutar un test específico por nombre
cargo test test_add_positive

# Ejecutar tests que contengan una cadena
cargo test add         # ejecuta test_add_positive, test_add_negative, test_add_zero

# Ejecutar tests de un módulo
cargo test tests::     # todos los tests del módulo tests

# Ejecutar un solo thread (útil para tests con estado compartido)
cargo test -- --test-threads=1

# Modo quiet (menos output)
cargo test -- -q
```

### Output de cargo test

```bash
$ cargo test
   Compiling mi_proyecto v0.1.0
    Finished `test` profile [unoptimized + debuginfo] target(s)
     Running unittests src/lib.rs (target/debug/deps/mi_proyecto-abc123)

running 5 tests
test tests::test_add_positive ... ok
test tests::test_add_negative ... ok
test tests::test_add_zero ... ok
test tests::test_overflow ... ok
test tests::test_identity ... ok

test result: ok. 5 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s

   Doc-tests mi_proyecto

running 0 tests

test result: ok. 0 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.00s
```

Cargo ejecuta tres tipos de tests automáticamente:
1. **Unit tests** (`src/`) — lo que estamos viendo.
2. **Integration tests** (`tests/`) — siguiente tópico.
3. **Doc tests** — ejemplos en documentación.

### println! en tests

Por defecto, `cargo test` **captura** la salida estándar de tests que pasan. Solo muestra stdout de tests que fallan:

```rust
#[test]
fn test_with_output() {
    println!("This is debug output");
    assert!(true);
}
// El println! NO se muestra al ejecutar cargo test
// (porque el test pasa)
```

Para ver el output de todos los tests:

```bash
cargo test -- --show-output
```

O para ver output solo de tests fallidos (comportamiento por defecto), simplemente:

```bash
cargo test
```

---

## 8. Filtrar y organizar tests

### #[ignore]: saltar tests lentos

```rust
#[test]
#[ignore]
fn test_slow_integration() {
    // Test que tarda 30 segundos
    std::thread::sleep(std::time::Duration::from_secs(30));
    assert!(true);
}

#[test]
fn test_fast() {
    assert_eq!(1 + 1, 2);
}
```

```bash
# Ejecutar solo tests no-ignorados (rápidos):
cargo test

# Ejecutar SOLO los ignorados:
cargo test -- --ignored

# Ejecutar TODOS (incluidos los ignorados):
cargo test -- --include-ignored
```

### Organizar por módulos

```rust
// src/lib.rs
pub mod math {
    pub fn add(a: i32, b: i32) -> i32 { a + b }
    pub fn multiply(a: i32, b: i32) -> i32 { a * b }
}

pub mod strings {
    pub fn reverse(s: &str) -> String {
        s.chars().rev().collect()
    }
}

#[cfg(test)]
mod tests {
    mod math_tests {
        use crate::math::*;

        #[test]
        fn test_add() { assert_eq!(add(2, 3), 5); }

        #[test]
        fn test_multiply() { assert_eq!(multiply(3, 4), 12); }
    }

    mod string_tests {
        use crate::strings::*;

        #[test]
        fn test_reverse() {
            assert_eq!(reverse("hello"), "olleh");
        }

        #[test]
        fn test_reverse_empty() {
            assert_eq!(reverse(""), "");
        }
    }
}
```

```bash
# Solo tests de math:
cargo test math_tests

# Solo tests de strings:
cargo test string_tests
```

### Tests en archivos de módulo

Los tests no tienen que estar solo en `lib.rs`. Cada archivo de módulo puede tener sus propios tests:

```rust
// src/parser.rs
pub fn parse_number(s: &str) -> Result<i64, std::num::ParseIntError> {
    s.trim().parse()
}

pub fn parse_bool(s: &str) -> Option<bool> {
    match s.trim().to_lowercase().as_str() {
        "true" | "yes" | "1" => Some(true),
        "false" | "no" | "0" => Some(false),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_number() {
        assert_eq!(parse_number("42"), Ok(42));
        assert_eq!(parse_number("  -7  "), Ok(-7));
        assert!(parse_number("abc").is_err());
    }

    #[test]
    fn test_parse_bool() {
        assert_eq!(parse_bool("true"), Some(true));
        assert_eq!(parse_bool("YES"), Some(true));
        assert_eq!(parse_bool("0"), Some(false));
        assert_eq!(parse_bool("maybe"), None);
    }
}
```

---

## 9. Test helpers y fixtures

### Funciones helper

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // Helper: crea datos de prueba
    fn sample_users() -> Vec<User> {
        vec![
            User::new("Alice", 30),
            User::new("Bob", 25),
            User::new("Carol", 35),
        ]
    }

    // Helper: crea un estado inicial limpio
    fn empty_database() -> Database {
        Database::in_memory()
    }

    #[test]
    fn test_find_oldest() {
        let users = sample_users();
        let oldest = find_oldest(&users).unwrap();
        assert_eq!(oldest.name, "Carol");
    }

    #[test]
    fn test_insert_and_retrieve() {
        let db = empty_database();
        let users = sample_users();
        for user in &users {
            db.insert(user);
        }
        assert_eq!(db.count(), 3);
    }
}
```

### Setup y teardown

Rust no tiene `setUp`/`tearDown` como JUnit. En cambio, usa RAII y funciones helper:

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    // "Setup": crea el entorno
    fn setup_test_dir() -> tempfile::TempDir {
        let dir = tempfile::tempdir().unwrap();
        fs::write(dir.path().join("test.txt"), "hello").unwrap();
        dir
        // TempDir implementa Drop → limpia automáticamente
    }

    #[test]
    fn test_read_file() {
        let dir = setup_test_dir();  // setup
        let content = fs::read_to_string(dir.path().join("test.txt")).unwrap();
        assert_eq!(content, "hello");
        // dir se dropea aquí → teardown automático (borra archivos)
    }

    #[test]
    fn test_file_exists() {
        let dir = setup_test_dir();
        assert!(dir.path().join("test.txt").exists());
        // teardown automático
    }
}
```

### Macro para tests repetitivos

```rust
#[cfg(test)]
mod tests {
    use super::*;

    macro_rules! parse_test {
        ($name:ident, $input:expr, $expected:expr) => {
            #[test]
            fn $name() {
                assert_eq!(parse_number($input), Ok($expected));
            }
        };
    }

    parse_test!(parse_zero, "0", 0);
    parse_test!(parse_positive, "42", 42);
    parse_test!(parse_negative, "-7", -7);
    parse_test!(parse_large, "1000000", 1_000_000);
    parse_test!(parse_with_spaces, "  99  ", 99);
}
```

---

## 10. Testing de funciones privadas

En Rust, los unit tests pueden acceder a funciones privadas porque el módulo `#[cfg(test)]` es un **hijo** del módulo que testea:

```rust
// src/lib.rs

// Función PRIVADA — no pub
fn validate_email(email: &str) -> bool {
    email.contains('@') && email.contains('.')
}

// Función pública que usa la privada
pub fn create_user(name: &str, email: &str) -> Result<User, String> {
    if !validate_email(email) {
        return Err(format!("Invalid email: {}", email));
    }
    Ok(User { name: name.to_string(), email: email.to_string() })
}

#[cfg(test)]
mod tests {
    use super::*;

    // Podemos testear la función privada directamente
    #[test]
    fn test_validate_email_valid() {
        assert!(validate_email("alice@example.com"));
    }

    #[test]
    fn test_validate_email_invalid() {
        assert!(!validate_email("not-an-email"));
        assert!(!validate_email("missing@dot"));
        assert!(!validate_email("missing.at"));
    }

    // Y también la función pública
    #[test]
    fn test_create_user_valid() {
        let result = create_user("Alice", "alice@example.com");
        assert!(result.is_ok());
    }

    #[test]
    fn test_create_user_invalid() {
        let result = create_user("Alice", "bad-email");
        assert!(result.is_err());
    }
}
```

### ¿Es buena práctica testear funciones privadas?

Hay dos perspectivas:

**Sí, testea privadas cuando:**
- La función tiene lógica compleja que merece tests directos.
- Testear solo la API pública requeriría setup excesivo para alcanzar la función interna.
- Es más claro testear el componente aislado.

**No, testea solo públicas cuando:**
- La función privada es trivial.
- Testear la API pública cubre la privada indirectamente.
- Los tests de la privada se acoplan demasiado a la implementación (se rompen al refactorizar).

La posición pragmática: **testea lo que tenga sentido**. Rust te da la opción — úsala cuando aporte valor.

---

## 11. Errores comunes

### Error 1: Olvidar #[cfg(test)] en el módulo de tests

```rust
// SIN #[cfg(test)]:
mod tests {
    use super::*;

    #[test]
    fn test_something() {
        assert!(true);
    }
}
```

El código **funciona**, pero el módulo de tests se compila también en release, inflando el binario innecesariamente. Siempre usa `#[cfg(test)]`.

### Error 2: Olvidar use super::*

```rust
pub fn add(a: i32, b: i32) -> i32 { a + b }

#[cfg(test)]
mod tests {
    // Olvidó: use super::*;

    #[test]
    fn test_add() {
        assert_eq!(add(2, 3), 5);
        // ERROR: cannot find function `add` in this scope
    }
}
```

El módulo de tests es un submódulo — necesita importar los items del padre.

### Error 3: Tests que dependen del orden de ejecución

```rust
static mut COUNTER: i32 = 0;

#[test]
fn test_first() {
    unsafe { COUNTER = 1; }
}

#[test]
fn test_second() {
    // PELIGRO: asume que test_first ya se ejecutó
    unsafe { assert_eq!(COUNTER, 1); }  // puede fallar
}
```

Los tests se ejecutan en paralelo y en orden no determinista. Cada test debe ser **independiente**. Si necesitas estado compartido, usa `--test-threads=1` o mejor, reestructura para que cada test cree su propio estado.

### Error 4: Comparar floats con assert_eq!

```rust
#[test]
fn test_float() {
    let result = 0.1 + 0.2;
    assert_eq!(result, 0.3);  // FALLA: 0.30000000000000004 ≠ 0.3
}
```

**Solución**: usar tolerancia:

```rust
#[test]
fn test_float() {
    let result = 0.1 + 0.2;
    let expected = 0.3;
    assert!(
        (result - expected).abs() < 1e-10,
        "Expected {}, got {}", expected, result
    );
}
```

### Error 5: Tests que no verifican nada

```rust
#[test]
fn test_process() {
    let result = process("input");
    // ¡No hay assert! El test siempre pasa.
    // Solo verifica que no hay panic, no que el resultado sea correcto.
}
```

Un test sin `assert` solo verifica que la función no hace panic. A veces es útil, pero generalmente es un test incompleto. Añade verificaciones explícitas.

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                      UNIT TESTS CHEATSHEET                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA                                                        ║
║  #[cfg(test)]                    Solo compila en cargo test         ║
║  mod tests {                     Convención de nombre               ║
║      use super::*;               Importa todo del módulo padre     ║
║      #[test]                     Marca función como test            ║
║      fn test_name() { ... }      Test que pasa si no hace panic    ║
║  }                                                                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ASSERTIONS                                                        ║
║  assert!(expr)                   Verifica que expr es true         ║
║  assert!(expr, "msg {}", x)     Con mensaje personalizado         ║
║  assert_eq!(a, b)                a == b (requiere Debug+PartialEq) ║
║  assert_ne!(a, b)                a != b (requiere Debug+PartialEq) ║
║  debug_assert!()                 Solo en debug builds              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TESTS CON RESULT                                                  ║
║  #[test]                                                           ║
║  fn test() -> Result<(), Box<dyn Error>> {                         ║
║      let x = might_fail()?;     Propaga errores con ?              ║
║      assert_eq!(x, expected);                                      ║
║      Ok(())                      Pasa si retorna Ok                ║
║  }                                                                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CARGO TEST                                                        ║
║  cargo test                      Todos los tests                   ║
║  cargo test nombre               Filtrar por nombre                ║
║  cargo test -- --show-output     Mostrar println! de tests OK      ║
║  cargo test -- --test-threads=1  Un hilo (secuencial)              ║
║  cargo test -- --ignored         Solo tests #[ignore]              ║
║  cargo test -- --include-ignored Todos incluidos ignorados         ║
║  cargo test -- -q                Output mínimo                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ORGANIZACIÓN                                                      ║
║  #[ignore]                       Saltar test (para lentos)         ║
║  mod subtests { ... }            Submódulos para agrupar           ║
║  cargo test modulo::             Filtrar por módulo                ║
║  fn helper() → datos de prueba   Funciones helper (en cfg(test))   ║
║  macro para tests repetitivos    Generar múltiples tests           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS                                                            ║
║  ✓ Cada test debe ser independiente (sin orden)                    ║
║  ✓ Tests se ejecutan en paralelo por defecto                       ║
║  ✓ #[cfg(test)] evita compilar tests en release                    ║
║  ✓ Funciones privadas son accesibles desde el módulo de tests      ║
║  ✓ Siempre verifica con assert (no solo "no panic")                ║
║  ✗ No comparar floats con assert_eq! (usar tolerancia)             ║
║  ✗ No depender del orden de ejecución                              ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Escribir tests para una función

Dada esta función, escribe al menos 6 tests que cubran casos normales, edge cases, y errores:

```rust
pub fn parse_csv_line(line: &str) -> Vec<String> {
    if line.is_empty() {
        return Vec::new();
    }
    line.split(',')
        .map(|field| field.trim().to_string())
        .collect()
}
```

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // Escribe tests para:
    // 1. Línea normal: "a,b,c" → ["a", "b", "c"]
    // 2. Línea vacía: "" → []
    // 3. Un solo campo: "hello" → ["hello"]
    // 4. Espacios alrededor: " a , b , c " → ["a", "b", "c"]
    // 5. Campos vacíos: "a,,c" → ["a", "", "c"]
    // 6. Solo comas: ",," → ["", "", ""]
    // 7. (Opcional) ¿Qué pasa con comillas? "a,\"b,c\",d"
}
```

**Preguntas:**
1. ¿El caso 5 es un bug o el comportamiento esperado?
2. ¿Cuántos tests son "suficientes"? ¿Cómo lo decides?
3. Si la función retornara `Result`, ¿cambiarían tus tests?

### Ejercicio 2: Tests con estado

Implementa la función y sus tests:

```rust
pub struct Counter {
    count: i32,
    max: i32,
}

impl Counter {
    pub fn new(max: i32) -> Self {
        Counter { count: 0, max }
    }

    pub fn increment(&mut self) -> Result<i32, String> {
        if self.count >= self.max {
            Err(format!("Counter at max ({})", self.max))
        } else {
            self.count += 1;
            Ok(self.count)
        }
    }

    pub fn reset(&mut self) {
        self.count = 0;
    }

    pub fn value(&self) -> i32 {
        self.count
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Escribe tests para:
    // 1. new() crea un counter con valor 0
    // 2. increment() incrementa el valor
    // 3. increment() retorna el nuevo valor
    // 4. increment() falla al llegar al max
    // 5. reset() pone el valor a 0
    // 6. Secuencia: new → increment × N → max → error → reset → increment OK
    // 7. Counter con max = 0 (no puede incrementar nunca)

    // Extra: usa un helper para crear counters de prueba
    // Extra: usa Result<(), ...> en algún test con ?
}
```

### Ejercicio 3: Macro de tests parametrizados

Crea una macro que genere múltiples tests a partir de una tabla de datos:

```rust
pub fn is_palindrome(s: &str) -> bool {
    let cleaned: String = s.chars()
        .filter(|c| c.is_alphanumeric())
        .map(|c| c.to_lowercase().next().unwrap())
        .collect();
    cleaned == cleaned.chars().rev().collect::<String>()
}

// Crea una macro test_cases! que genere tests así:
// test_cases! {
//     palindrome_racecar: ("racecar", true),
//     palindrome_hello: ("hello", false),
//     palindrome_empty: ("", true),
//     palindrome_single: ("a", true),
//     palindrome_spaces: ("a man a plan a canal panama", true),
//     palindrome_mixed_case: ("RaceCar", true),
//     palindrome_punctuation: ("A man, a plan, a canal: Panama!", true),
// }

// Cada entrada genera un #[test] fn nombre() { assert_eq!(is_palindrome(input), expected); }
```

**Implementa la macro y verifica que todos los tests pasen.**

**Preguntas:**
1. ¿Qué ventaja tiene la macro sobre copiar y pegar 7 funciones de test?
2. ¿Y sobre un solo test con un loop sobre los datos?
3. Si un test individual falla, ¿sabes cuál sin mirar el output detallado?

---

> **Nota**: los unit tests en Rust son la primera línea de defensa contra bugs. Gracias a `#[cfg(test)]`, viven junto al código que testean sin coste en el binario final. Gracias a `use super::*`, pueden acceder a funciones privadas para testear la lógica interna. Y gracias al sistema de tipos de Rust, muchos bugs que en otros lenguajes requieren tests (null checks, type errors) simplemente no pueden existir — lo que te permite enfocar tus tests en lógica de negocio, no en plumbing.
