# T02 - Doc Tests en Rust

## Índice

1. [El problema: documentación que miente](#1-el-problema-documentación-que-miente)
2. [Qué son los doc tests](#2-qué-son-los-doc-tests)
3. [Doc comments: la base de los doc tests](#3-doc-comments-la-base-de-los-doc-tests)
4. [Anatomía de un doc test](#4-anatomía-de-un-doc-test)
5. [Cómo rustdoc compila y ejecuta doc tests](#5-cómo-rustdoc-compila-y-ejecuta-doc-tests)
6. [El código oculto: líneas con #](#6-el-código-oculto-líneas-con-)
7. [Doc tests con Result y el operador ?](#7-doc-tests-con-result-y-el-operador-)
8. [Atributos de bloque: ignore, no_run, compile_fail, should_panic](#8-atributos-de-bloque-ignore-no_run-compile_fail-should_panic)
9. [Cuándo usar cada atributo](#9-cuándo-usar-cada-atributo)
10. [Doc tests en funciones](#10-doc-tests-en-funciones)
11. [Doc tests en structs y enums](#11-doc-tests-en-structs-y-enums)
12. [Doc tests en traits e implementaciones](#12-doc-tests-en-traits-e-implementaciones)
13. [Doc tests en módulos y crates](#13-doc-tests-en-módulos-y-crates)
14. [Múltiples bloques de código en un doc comment](#14-múltiples-bloques-de-código-en-un-doc-comment)
15. [Doc tests y visibilidad](#15-doc-tests-y-visibilidad)
16. [Doc tests con dependencias externas](#16-doc-tests-con-dependencias-externas)
17. [Debugging de doc tests que fallan](#17-debugging-de-doc-tests-que-fallan)
18. [Doc tests vs unit tests vs integration tests](#18-doc-tests-vs-unit-tests-vs-integration-tests)
19. [Comparación con C y Go](#19-comparación-con-c-y-go)
20. [Errores comunes](#20-errores-comunes)
21. [Ejemplo completo: biblioteca de formateo de texto](#21-ejemplo-completo-biblioteca-de-formateo-de-texto)
22. [Programa de práctica](#22-programa-de-práctica)
23. [Ejercicios](#23-ejercicios)

---

## 1. El problema: documentación que miente

La documentación se pudre. Es un hecho universal del software. El código evoluciona, las APIs cambian, los parámetros se renombran, pero los comentarios y los ejemplos se quedan atrás:

```
La vida de un ejemplo en documentación:

  Día 1:     Se escribe el código y el ejemplo
             Ejemplo funciona ✓

  Día 30:    Se renombra un parámetro
             Ejemplo desactualizado ✗ (nadie lo notó)

  Día 90:    Se cambia el tipo de retorno
             Ejemplo ni compila ✗ (nadie lo notó)

  Día 180:   Un usuario nuevo copia el ejemplo
             No funciona. Abre un issue.
             "La documentación miente."

  Día 181:   El mantenedor arregla el ejemplo
             (o lo borra porque es más fácil)
```

En C, esto es el estado normal de las cosas. Los comentarios Doxygen son texto inerte — el compilador nunca los verifica:

```c
/**
 * @brief Concatena dos strings.
 *
 * Ejemplo:
 * @code
 * char *result = str_concat("hello", "world");
 * printf("%s\n", result);  // imprime "helloworld"
 * free(result);
 * @endcode
 *
 * MENTIRA: str_concat fue renombrado a str_join hace 3 meses.
 * Este ejemplo ya no compila pero nadie lo sabe.
 */
char *str_join(const char *a, const char *b);
```

Go mejoró esto con sus `Example` functions, que son tests ejecutables:

```go
func ExampleReverse() {
    fmt.Println(Reverse("hello"))
    // Output: olleh
}
```

Rust fue un paso más allá: **cualquier bloque de código en documentación es un test automáticamente**. No necesitas escribir funciones especiales ni registrar nada. Si escribes un ejemplo en un doc comment, `cargo test` lo compila y ejecuta.

```
Estrategias contra documentación obsoleta:

  C:      Confiar en que alguien lo actualice manualmente
          Efectividad: ~10%

  Go:     Example functions (explícitas, separadas de la doc)
          Efectividad: ~70%

  Rust:   Doc tests (implícitas, SON la documentación)
          Efectividad: ~95%

  ¿Por qué ~95% y no 100%?
  Porque puedes marcar bloques como `no_run` o `ignore`,
  y esos no se ejecutan. Pero al menos la decisión es explícita.
```

---

## 2. Qué son los doc tests

Un doc test es un bloque de código Rust dentro de un doc comment (`///` o `//!`) que se extrae, compila y ejecuta automáticamente por `cargo test`:

```rust
/// Suma dos números.
///
/// ```
/// let result = mi_crate::add(2, 3);
/// assert_eq!(result, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

El bloque entre los triple backticks (` ``` `) es el doc test. Cuando ejecutas `cargo test`, rustdoc lo extrae, lo envuelve en un `fn main()`, lo compila contra tu crate, y lo ejecuta.

```
Flujo de un doc test:

  Tu código fuente:
  ┌──────────────────────────────────┐
  │  /// Suma dos números.           │
  │  ///                             │
  │  /// ```                         │
  │  /// let r = mi_crate::add(2,3); │
  │  /// assert_eq!(r, 5);           │
  │  /// ```                         │
  │  pub fn add(a: i32, b: i32) ->   │
  └──────────────────────────────────┘
            │
            │ rustdoc extrae el bloque
            ▼
  ┌──────────────────────────────────┐
  │  // Código generado por rustdoc  │
  │  extern crate mi_crate;         │
  │  fn main() {                     │
  │      let r = mi_crate::add(2,3); │
  │      assert_eq!(r, 5);           │
  │  }                               │
  └──────────────────────────────────┘
            │
            │ rustc compila
            ▼
  ┌──────────────────────────────────┐
  │  Binario temporal de test        │
  │  → Se ejecuta                    │
  │  → Si no paniquea: PASS          │
  │  → Si paniquea: FAIL             │
  └──────────────────────────────────┘
```

### Características clave

| Característica | Detalle |
|---------------|---------|
| **Activación** | Automática — cualquier ` ``` ` en doc comments |
| **Compilación** | Como crate externo (igual que integration tests) |
| **Visibilidad** | Solo API pública (`pub`) |
| **Fase en cargo test** | Tercera, después de unit e integration tests |
| **Requiere anotación** | No — por defecto todo bloque se ejecuta |
| **Framework** | Ninguno — `assert!` de la librería estándar |
| **Producen documentación** | Sí — `cargo doc` los muestra como ejemplos |

### Doble propósito

Los doc tests tienen un rol dual que ningún otro tipo de test tiene:

```
                 Doc tests: doble propósito

  ┌───────────────────────┐    ┌───────────────────────┐
  │    Como TESTS          │    │   Como DOCUMENTACIÓN   │
  │                        │    │                        │
  │  cargo test            │    │  cargo doc             │
  │  ↓                     │    │  ↓                     │
  │  Se compilan y         │    │  Se renderizan como    │
  │  ejecutan              │    │  ejemplos en HTML      │
  │                        │    │                        │
  │  Verifican que el      │    │  Muestran al usuario   │
  │  ejemplo funciona      │    │  cómo usar la API      │
  │                        │    │                        │
  │  Fallan si la API      │    │  Siempre están         │
  │  cambia                │    │  actualizados           │
  └───────────────────────┘    └───────────────────────┘
                 │                        │
                 └────────┬───────────────┘
                          │
                   MISMO CÓDIGO
                   (no se puede desincronizar)
```

---

## 3. Doc comments: la base de los doc tests

Rust tiene dos tipos de doc comments. Los doc tests funcionan en ambos:

### `///` — Doc comment exterior (documenta el item que sigue)

```rust
/// Esta función hace algo importante.
///
/// # Ejemplo
///
/// ```
/// let x = mi_crate::something();
/// assert!(x > 0);
/// ```
pub fn something() -> i32 {
    42
}
```

### `//!` — Doc comment interior (documenta el item que lo contiene)

```rust
// src/lib.rs

//! # Mi Crate
//!
//! Una biblioteca para hacer cosas útiles.
//!
//! ```
//! let result = mi_crate::do_thing("hello");
//! assert_eq!(result, "HELLO");
//! ```

pub fn do_thing(s: &str) -> String {
    s.to_uppercase()
}
```

### Dónde se pueden poner doc comments

```
Locations válidas para doc comments (y por tanto doc tests):

  //! en lib.rs/main.rs     → Documenta la crate completa
  //! en mod.rs              → Documenta un módulo

  /// antes de pub fn        → Documenta una función
  /// antes de pub struct    → Documenta un struct
  /// antes de pub enum      → Documenta un enum
  /// antes de pub trait     → Documenta un trait
  /// antes de impl Block    → Documenta un bloque de implementación
  /// antes de pub type      → Documenta un type alias
  /// antes de pub const     → Documenta una constante
  /// antes de pub static    → Documenta un static
  /// antes de campo de struct → Documenta un campo
  /// antes de variante de enum → Documenta una variante
  /// antes de método        → Documenta un método
```

### Secciones convencionales en doc comments

La comunidad Rust usa secciones con `#` dentro de los doc comments. Algunas tienen significado especial para `cargo doc`:

```rust
/// Divide dos números.
///
/// # Arguments
///
/// * `dividend` - El número a dividir
/// * `divisor` - El número por el que dividir
///
/// # Returns
///
/// El resultado de la división como `f64`.
///
/// # Examples              ← Esta sección es la convencional para doc tests
///
/// ```
/// let result = mi_crate::divide(10.0, 3.0);
/// assert!((result - 3.333).abs() < 0.01);
/// ```
///
/// # Panics               ← Documenta cuándo la función hace panic
///
/// Panics si `divisor` es cero.
///
/// # Errors               ← Para funciones que retornan Result
///
/// No aplica — esta función paniquea en vez de retornar error.
///
/// # Safety               ← Para funciones unsafe
///
/// No aplica — esta función es safe.
pub fn divide(dividend: f64, divisor: f64) -> f64 {
    if divisor == 0.0 {
        panic!("Division by zero");
    }
    dividend / divisor
}
```

Las secciones convencionales son:

| Sección | Cuándo usarla |
|---------|--------------|
| `# Examples` | Siempre — muestra cómo usar el item |
| `# Panics` | Si la función puede hacer panic |
| `# Errors` | Si retorna `Result` — documenta las variantes de error |
| `# Safety` | Si es `unsafe` — documenta las invariantes que el caller debe garantizar |
| `# Arguments` | Para funciones con múltiples parámetros |
| `# Returns` | Cuando el retorno no es obvio |

---

## 4. Anatomía de un doc test

Veamos un doc test completo, pieza por pieza:

```rust
/// Busca un elemento en un slice ordenado usando búsqueda binaria.
///
/// Retorna `Some(index)` si el elemento se encuentra, `None` si no.
///
/// # Examples
///
/// Búsqueda exitosa:
///
/// ```
/// let data = vec![1, 3, 5, 7, 9, 11];
/// let result = mi_crate::binary_search(&data, &7);
/// assert_eq!(result, Some(3));
/// ```
///
/// Elemento no encontrado:
///
/// ```
/// let data = vec![1, 3, 5, 7, 9, 11];
/// let result = mi_crate::binary_search(&data, &4);
/// assert_eq!(result, None);
/// ```
///
/// Slice vacío:
///
/// ```
/// let empty: Vec<i32> = vec![];
/// let result = mi_crate::binary_search(&empty, &1);
/// assert_eq!(result, None);
/// ```
pub fn binary_search<T: Ord>(slice: &[T], target: &T) -> Option<usize> {
    let mut low = 0;
    let mut high = slice.len();

    while low < high {
        let mid = low + (high - low) / 2;
        match slice[mid].cmp(target) {
            std::cmp::Ordering::Equal => return Some(mid),
            std::cmp::Ordering::Less => low = mid + 1,
            std::cmp::Ordering::Greater => high = mid,
        }
    }
    None
}
```

### Cada bloque es un test independiente

En el ejemplo anterior hay **3 doc tests** — uno por cada bloque ` ``` ... ``` `. Cada uno se compila y ejecuta independientemente:

```
cargo test extrae 3 tests:

  Test 1: "mi_crate::binary_search - search exitosa (line 9)"
  ┌─────────────────────────────────────┐
  │ fn main() {                         │
  │   let data = vec![1, 3, 5, 7, 9];  │
  │   let r = mi_crate::binary_search(  │
  │       &data, &7);                   │
  │   assert_eq!(r, Some(3));           │
  │ }                                   │
  └─────────────────────────────────────┘

  Test 2: "mi_crate::binary_search - no encontrado (line 16)"
  ┌─────────────────────────────────────┐
  │ fn main() {                         │
  │   let data = vec![1, 3, 5, 7, 9];  │
  │   let r = mi_crate::binary_search(  │
  │       &data, &4);                   │
  │   assert_eq!(r, None);             │
  └─────────────────────────────────────┘

  Test 3: "mi_crate::binary_search - vacío (line 23)"
  ┌─────────────────────────────────────┐
  │ fn main() {                         │
  │   let empty: Vec<i32> = vec![];     │
  │   let r = mi_crate::binary_search(  │
  │       &empty, &1);                  │
  │   assert_eq!(r, None);             │
  └─────────────────────────────────────┘
```

### Qué debe contener un doc test

```rust
/// Un buen doc test cumple estos criterios:
///
/// 1. Es AUTOCONTENIDO — se puede copiar y ejecutar tal cual
/// 2. Es BREVE — muestra el uso típico, no todos los edge cases
/// 3. Es LEGIBLE — un usuario nuevo entiende qué hace
/// 4. VERIFICA — usa assert! para confirmar el resultado
///
/// ```
/// // ✓ Buen doc test:
/// // - Autocontenido (no depende de estado externo)
/// // - Breve (3 líneas de código)
/// // - Legible (el flujo es obvio)
/// // - Verifica (assert_eq! confirma resultado)
/// let encoded = mi_crate::base64_encode("hello");
/// assert_eq!(encoded, "aGVsbG8=");
///
/// let decoded = mi_crate::base64_decode(&encoded).unwrap();
/// assert_eq!(decoded, "hello");
/// ```
```

```rust
/// Ejemplo de doc test MALO (no hagas esto):
///
/// ```
/// // ✗ Demasiado largo — parece un integration test
/// let mut items = Vec::new();
/// for i in 0..100 {
///     items.push(mi_crate::Item::new(i));
/// }
/// let mut processed = 0;
/// for item in &items {
///     if item.is_valid() {
///         mi_crate::process(item);
///         processed += 1;
///     }
/// }
/// assert_eq!(processed, 100);
/// // ... 20 líneas más de setup y verificación
/// ```
```

---

## 5. Cómo rustdoc compila y ejecuta doc tests

Entender la transformación que rustdoc aplica a tu código es crucial para diagnosticar errores.

### Transformación paso a paso

Lo que tú escribes:

```rust
/// ```
/// let x = mi_crate::add(2, 3);
/// assert_eq!(x, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

Lo que rustdoc genera internamente:

```rust
// Archivo temporal generado por rustdoc
#![allow(unused)]
extern crate mi_crate;
fn main() {
    let x = mi_crate::add(2, 3);
    assert_eq!(x, 5);
}
```

### Reglas de la transformación

```
Reglas que rustdoc aplica:

  1. Agrega `extern crate mi_crate;` automáticamente
     → Tu crate es importada como dependencia externa
     → Puedes usar `mi_crate::` sin declarar nada

  2. Envuelve todo en `fn main() { ... }`
     → A menos que tu código ya tenga un `fn main()`
     → Si detecta `fn main`, NO envuelve

  3. Agrega `#![allow(unused)]`
     → Las variables sin usar no generan warnings

  4. Si el código contiene `use mi_crate::algo;`
     → Funciona directamente (extern crate ya está)

  5. Si una línea empieza con `# `
     → Se compila pero NO se muestra en la documentación
```

### Demostración de la transformación

```
Lo que escribes:             Lo que rustdoc compila:

/// ```                      #![allow(unused)]
/// use mi_crate::Config;    extern crate mi_crate;
///                          fn main() {
/// let c = Config::new();       use mi_crate::Config;
/// assert!(c.is_valid());
/// ```                          let c = Config::new();
                                 assert!(c.is_valid());
                             }
```

```
Con fn main explícito:

/// ```                      #![allow(unused)]
/// fn main() {              extern crate mi_crate;
///     let x = 5;           fn main() {        ← NO envuelve
///     assert!(x > 0);          let x = 5;
/// }                            assert!(x > 0);
/// ```                      }
```

```
Con líneas ocultas (#):

/// ```                      #![allow(unused)]
/// # use mi_crate::Config;  extern crate mi_crate;
/// let c = Config::new();   fn main() {
/// assert!(c.is_valid());       use mi_crate::Config;  ← Se compila
/// ```                          let c = Config::new();
                                 assert!(c.is_valid());
                             }

En la documentación HTML solo se ve:
  let c = Config::new();
  assert!(c.is_valid());
```

### El binario resultante

```
Cada doc test genera un binario temporal separado:

  cargo test
    ├── Unit tests     → 1 binario con todos los #[test] de src/
    ├── Integration    → 1 binario por archivo en tests/
    └── Doc tests      → 1 binario POR CADA bloque ```rust
                          (potencialmente muchos!)

  Si tienes 50 bloques de código en tus doc comments:
    → 50 compilaciones separadas
    → 50 binarios ejecutados
    → Puede ser LENTO en proyectos grandes
```

### Output de cargo test para doc tests

```
$ cargo test --doc

   Doc-tests mi_crate

running 6 tests
test src/lib.rs - add (line 5) ... ok
test src/lib.rs - binary_search (line 15) ... ok
test src/lib.rs - binary_search (line 22) ... ok
test src/lib.rs - binary_search (line 29) ... ok
test src/lib.rs - Config (line 8) ... ok
test src/parser.rs - parser::parse (line 12) ... ok
test result: ok. 6 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

Observa el formato del nombre: `archivo - item (line N)`. Esto te dice exactamente dónde está el doc test que falló.

---

## 6. El código oculto: líneas con #

Esta es una de las features más útiles de los doc tests. Las líneas que empiezan con `# ` (hash + espacio) se **compilan y ejecutan** pero **no se muestran** en la documentación generada:

### Por qué es necesario

Un doc test debe ser autocontenido (compilable), pero la documentación debe ser concisa (no mostrar boilerplate). Estas dos necesidades están en conflicto:

```rust
/// Convierte un string a mayúsculas.
///
/// ```
/// // Sin # — el usuario ve TODO:
/// use mi_crate::to_upper;        // ← Boilerplate de import
///
/// let result = to_upper("hello");
/// assert_eq!(result, "HELLO");
/// ```
pub fn to_upper(s: &str) -> String {
    s.to_uppercase()
}
```

En la documentación HTML, el usuario ve el `use mi_crate::to_upper;` que es obvio y añade ruido. Con `#`:

```rust
/// Convierte un string a mayúsculas.
///
/// ```
/// # use mi_crate::to_upper;     // ← Se compila pero NO se muestra
/// let result = to_upper("hello");
/// assert_eq!(result, "HELLO");
/// ```
pub fn to_upper(s: &str) -> String {
    s.to_uppercase()
}
```

Ahora la documentación HTML solo muestra:

```rust
let result = to_upper("hello");
assert_eq!(result, "HELLO");
```

### Casos de uso para #

```rust
/// Procesa una configuración y retorna el resultado.
///
/// ```
/// # // Setup que el usuario no necesita ver:
/// # use mi_crate::{Config, Processor, ProcessResult};
/// # use std::collections::HashMap;
/// #
/// # let mut settings = HashMap::new();
/// # settings.insert("mode".to_string(), "fast".to_string());
/// # settings.insert("threads".to_string(), "4".to_string());
/// # let config = Config::from_map(settings).unwrap();
/// #
/// // El usuario solo ve la parte importante:
/// let processor = Processor::new(config);
/// let result = processor.run().unwrap();
/// assert_eq!(result.status(), "success");
/// ```
pub struct Processor { /* ... */ }
```

### Más ejemplos de uso de #

**Ocultar manejo de errores:**

```rust
/// Lee un archivo y cuenta las líneas.
///
/// ```
/// # use std::io::Write;
/// # let dir = tempfile::tempdir().unwrap();
/// # let path = dir.path().join("test.txt");
/// # std::fs::write(&path, "line1\nline2\nline3\n").unwrap();
/// #
/// let count = mi_crate::count_lines(&path).unwrap();
/// assert_eq!(count, 3);
/// ```
pub fn count_lines(path: &std::path::Path) -> std::io::Result<usize> {
    let content = std::fs::read_to_string(path)?;
    Ok(content.lines().count())
}
```

**Ocultar fn main para usar ?:**

```rust
/// Parsea una dirección IP.
///
/// ```
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let addr = mi_crate::parse_ip("192.168.1.1")?;
/// assert_eq!(addr.octets(), [192, 168, 1, 1]);
/// # Ok(())
/// # }
/// ```
pub fn parse_ip(s: &str) -> Result<std::net::Ipv4Addr, std::net::AddrParseError> {
    s.parse()
}
```

**Ocultar definiciones auxiliares:**

```rust
/// Ordena una lista de items por prioridad.
///
/// ```
/// # #[derive(Debug, PartialEq)]
/// # struct Task { name: String, priority: u32 }
/// # impl Task {
/// #     fn new(name: &str, priority: u32) -> Self {
/// #         Task { name: name.to_string(), priority }
/// #     }
/// # }
/// #
/// let mut tasks = vec![
///     Task::new("low", 3),
///     Task::new("high", 1),
///     Task::new("medium", 2),
/// ];
///
/// mi_crate::sort_by_priority(&mut tasks);
///
/// assert_eq!(tasks[0].name, "high");
/// assert_eq!(tasks[1].name, "medium");
/// assert_eq!(tasks[2].name, "low");
/// ```
pub fn sort_by_priority<T: HasPriority>(items: &mut [T]) {
    items.sort_by_key(|item| item.priority());
}
```

### Regla de sintaxis para #

```
# al inicio de línea = ocultar la línea

  "# "  (hash + espacio)        → Oculta la línea, compila el resto
  "#"   (hash solo, sin espacio) → Oculta la línea vacía (solo cosmético)
  "## " (doble hash)            → Muestra "# " en la documentación
                                   (para headings dentro de bloques de código)

  Ejemplos:
    /// ```
    /// # use mi_crate::Foo;      ← Se oculta "use mi_crate::Foo;"
    /// #                          ← Se oculta (línea vacía en compilación)
    /// let f = Foo::new();        ← Se muestra
    /// ## Heading                 ← Se muestra como "# Heading"
    /// ```
```

---

## 7. Doc tests con Result y el operador ?

Muchas funciones en Rust retornan `Result`. Los doc tests necesitan manejar estos `Result` de forma idiomática:

### Problema: ? necesita fn main() -> Result

```rust
/// ```
/// let value = mi_crate::parse("42")?;  // ERROR: ? no se puede usar aquí
/// assert_eq!(value, 42);
/// ```
```

El error es:

```
error[E0277]: the `?` operator can only be used in a function
              that returns `Result` or `Option`
```

Esto ocurre porque rustdoc envuelve el código en `fn main()` que retorna `()`, y `?` requiere que la función retorne `Result`.

### Solución 1: fn main() -> Result explícito (oculto)

```rust
/// Parsea un número de un string.
///
/// ```
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let value = mi_crate::parse_number("42")?;
/// assert_eq!(value, 42);
///
/// let negative = mi_crate::parse_number("-17")?;
/// assert_eq!(negative, -17);
/// # Ok(())
/// # }
/// ```
pub fn parse_number(s: &str) -> Result<i32, std::num::ParseIntError> {
    s.parse()
}
```

El usuario ve en la documentación:

```rust
let value = mi_crate::parse_number("42")?;
assert_eq!(value, 42);

let negative = mi_crate::parse_number("-17")?;
assert_eq!(negative, -17);
```

### Solución 2: .unwrap() explícito

```rust
/// Parsea un número de un string.
///
/// ```
/// let value = mi_crate::parse_number("42").unwrap();
/// assert_eq!(value, 42);
/// ```
pub fn parse_number(s: &str) -> Result<i32, std::num::ParseIntError> {
    s.parse()
}
```

Esto es más simple pero tiene una desventaja: si el doc test falla, el mensaje de error es un panic genérico, no un error tipado.

### Cuándo usar cada enfoque

```
¿unwrap() o ?  en doc tests?

  unwrap():
    + Más simple, menos boilerplate
    + El usuario nuevo entiende qué pasa
    - Panic genérico si falla
    - No muestra el patrón idiomático de manejo de errores

  ? con fn main() -> Result:
    + Muestra el uso idiomático
    + Error más informativo si falla
    + El usuario aprende el patrón correcto
    - Requiere # fn main() oculto (más complejo)

  Recomendación:
    → Para funciones cuyo punto ES el manejo de errores: usa ?
    → Para funciones donde el error es improbable en el ejemplo: usa unwrap()
    → Si el doc test es de 1-2 líneas: usa unwrap()
    → Si el doc test muestra un flujo con múltiples ?: usa fn main() -> Result
```

### Ejemplo comparativo

```rust
/// Carga y parsea un archivo de configuración.
///
/// # Examples
///
/// Uso con `unwrap()` (simple, para ejemplos cortos):
///
/// ```no_run
/// let config = mi_crate::load_config("config.toml").unwrap();
/// println!("Name: {}", config.name());
/// ```
///
/// Uso con `?` (idiomático, para flujos completos):
///
/// ```no_run
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let config = mi_crate::load_config("config.toml")?;
/// let db = mi_crate::connect_database(&config)?;
/// let users = db.query_users()?;
/// println!("Found {} users", users.len());
/// # Ok(())
/// # }
/// ```
pub fn load_config(path: &str) -> Result<Config, ConfigError> {
    todo!()
}
```

### Retorno implícito de Result en doc tests

Desde Rust 1.40+, rustdoc puede inferir automáticamente el tipo de retorno si el doc test usa `?` y termina con `Ok(())`:

```rust
/// ```
/// let value: i32 = "42".parse()?;
/// assert_eq!(value, 42);
/// Ok::<(), Box<dyn std::error::Error>>(())
/// ```
```

Esto funciona, pero la línea `Ok::<(), Box<dyn std::error::Error>>(())` es fea. La mayoría de la comunidad prefiere el patrón `# fn main() -> Result` oculto.

---

## 8. Atributos de bloque: ignore, no_run, compile_fail, should_panic

No todos los bloques de código deben ejecutarse. Rust ofrece atributos para controlar el comportamiento de cada bloque:

### Resumen de atributos

```
Atributos de bloques de código:

  ```              → Compila Y ejecuta (default)
  ```rust          → Igual que ``` (explícito)
  ```ignore        → NO compila, NO ejecuta (ignorado completamente)
  ```no_run        → SÍ compila, NO ejecuta
  ```compile_fail  → Debe FALLAR al compilar (verifica errores)
  ```should_panic  → Compila, ejecuta, debe hacer PANIC
  ```text          → NO es código Rust (no se procesa)
  ```python        → NO es código Rust (no se procesa)
  ```c             → NO es código Rust (no se procesa)
```

### `ignore` — No compilar ni ejecutar

```rust
/// # Examples
///
/// ```ignore
/// // Este código no se compilará ni ejecutará.
/// // Útil para ejemplos incompletos o pseudocódigo.
/// let result = funcion_que_no_existe_aun();
/// process(result);
/// ```
pub fn something() {}
```

Cuándo usar `ignore`:
- Código que es pseudocódigo o incompleto
- Ejemplos que dependen de features no siempre disponibles
- Código de ejemplo de una versión futura que aún no está implementada

### `no_run` — Compilar pero no ejecutar

```rust
/// Conecta a una base de datos remota.
///
/// ```no_run
/// // Este código COMPILA (se verifica que la sintaxis es correcta)
/// // pero NO SE EJECUTA (no hay base de datos en el entorno de test)
/// let db = mi_crate::Database::connect("postgres://localhost/mydb").unwrap();
/// let users = db.query("SELECT * FROM users").unwrap();
/// println!("Found {} users", users.len());
/// ```
pub fn connect(url: &str) -> Result<Database, DbError> {
    todo!()
}
```

Cuándo usar `no_run`:
- Código que requiere recursos externos (bases de datos, APIs, archivos del sistema)
- Código que haría efectos secundarios (enviar emails, escribir en disco)
- Código que entraría en un loop infinito (servidores)
- Código que requiere input del usuario

```rust
/// Inicia un servidor HTTP.
///
/// ```no_run
/// // Compilable pero no ejecutable: bloquearía infinitamente
/// let server = mi_crate::Server::new("0.0.0.0:8080");
/// server.run();  // ← Bloquea el thread forever
/// ```
pub fn run(&self) { todo!() }
```

### `compile_fail` — Debe fallar al compilar

```rust
/// Los valores de `Color` no se pueden crear con valores fuera de rango.
///
/// ```compile_fail
/// // Este código DEBE NO compilar
/// let color = mi_crate::Color {
///     r: 256,  // u8 solo permite 0-255
///     g: 0,
///     b: 0,
/// };
/// ```
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}
```

Cuándo usar `compile_fail`:
- Demostrar que el sistema de tipos previene errores
- Mostrar qué NO se puede hacer (educativo)
- Verificar que restricciones de lifetime/ownership funcionan

```rust
/// Las referencias no pueden outlive al dato original.
///
/// ```compile_fail
/// let reference;
/// {
///     let data = mi_crate::Data::new(42);
///     reference = data.borrow();  // Borrow de data
/// }   // data se dropea aquí
/// println!("{}", reference);  // ERROR: data ya no existe
/// ```
pub struct Data { value: i32 }
```

### `should_panic` — Debe hacer panic

```rust
/// Acceder a un índice fuera de rango causa panic.
///
/// ```should_panic
/// let v = mi_crate::SafeVec::from(vec![1, 2, 3]);
/// let _x = v.get_unchecked(10);  // Panic: index out of bounds
/// ```
pub struct SafeVec<T> { inner: Vec<T> }
```

### Combinación de atributos con `rust`

Los atributos se pueden combinar con la anotación de lenguaje:

```
```rust,no_run       → Rust, compilar pero no ejecutar
```rust,ignore       → Rust, ignorar completamente
```rust,compile_fail → Rust, debe fallar compilación
```rust,should_panic → Rust, debe hacer panic
```

La forma `rust,atributo` es equivalente a solo `atributo`, pero hace explícito que es código Rust.

---

## 9. Cuándo usar cada atributo

### Tabla de decisión

```
¿Qué atributo necesito?

  ¿El código es Rust válido?
  ├─ NO → ¿Es pseudocódigo o ejemplo conceptual?
  │       ├─ SÍ → ```ignore
  │       └─ NO → ¿Es otro lenguaje?
  │               └─ SÍ → ```text, ```c, ```python, etc.
  │
  └─ SÍ → ¿Debe compilar?
          ├─ NO, quiero mostrar un ERROR de compilación
          │   └─ ```compile_fail
          │
          └─ SÍ → ¿Debe ejecutarse?
                  ├─ NO → ¿Por qué no?
                  │       ├─ Necesita recursos externos → ```no_run
                  │       ├─ Bloquearía (servidor, loop) → ```no_run
                  │       ├─ Haría efectos secundarios → ```no_run
                  │       └─ Es solo demostración → ```no_run
                  │
                  └─ SÍ → ¿Debe hacer panic?
                          ├─ SÍ → ```should_panic
                          └─ NO → ``` (default, sin atributo)
```

### Tabla comparativa

| Atributo | Compila | Ejecuta | Pasa si... | Uso típico |
|----------|---------|---------|-----------|------------|
| ` ``` ` (default) | Sí | Sí | No panic | Ejemplo normal |
| ` ```no_run ` | Sí | No | Compila | Código con I/O externo |
| ` ```ignore ` | No | No | Siempre | Pseudocódigo |
| ` ```compile_fail ` | Sí* | No | NO compila | Mostrar errores de tipos |
| ` ```should_panic ` | Sí | Sí | Panic | Demostrar panics |
| ` ```text ` | No | No | N/A | No es código Rust |

*`compile_fail` invierte la lógica: pasa si la compilación **falla**.

### Ejemplo con todos los atributos en un solo item

```rust
/// Una división segura que retorna `Result`.
///
/// # Uso normal
///
/// ```
/// let result = mi_crate::safe_div(10, 3).unwrap();
/// assert_eq!(result, 3);
/// ```
///
/// # Uso con cero retorna error
///
/// ```
/// let err = mi_crate::safe_div(10, 0);
/// assert!(err.is_err());
/// ```
///
/// # Ejemplo con unwrap de error (panic)
///
/// ```should_panic
/// mi_crate::safe_div(10, 0).unwrap(); // Panic: division by zero
/// ```
///
/// # El tipo de retorno impide ignorar errores
///
/// ```compile_fail
/// // No puedes usar el resultado directamente como i32
/// let x: i32 = mi_crate::safe_div(10, 3);
/// ```
///
/// # Uso en un programa real
///
/// ```no_run
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let input = std::env::args().nth(1).unwrap();
/// let divisor: i32 = input.parse()?;
/// let result = mi_crate::safe_div(100, divisor)?;
/// println!("100 / {} = {}", divisor, result);
/// # Ok(())
/// # }
/// ```
///
/// # Pseudocódigo del algoritmo
///
/// ```ignore
/// si divisor == 0:
///     retornar Error("division by zero")
/// sino:
///     retornar Ok(dividendo / divisor)
/// ```
pub fn safe_div(a: i32, b: i32) -> Result<i32, String> {
    if b == 0 {
        Err("division by zero".to_string())
    } else {
        Ok(a / b)
    }
}
```

---

## 10. Doc tests en funciones

Las funciones son el lugar más común para doc tests. Aquí están los patrones por tipo de función:

### Función simple (sin Result, sin genéricos)

```rust
/// Invierte un string.
///
/// ```
/// assert_eq!(mi_crate::reverse("hello"), "olleh");
/// assert_eq!(mi_crate::reverse(""), "");
/// assert_eq!(mi_crate::reverse("a"), "a");
/// ```
pub fn reverse(s: &str) -> String {
    s.chars().rev().collect()
}
```

### Función que retorna Result

```rust
/// Parsea un color hexadecimal.
///
/// # Examples
///
/// ```
/// let (r, g, b) = mi_crate::parse_hex_color("#FF8800").unwrap();
/// assert_eq!(r, 255);
/// assert_eq!(g, 136);
/// assert_eq!(b, 0);
/// ```
///
/// # Errors
///
/// Retorna error si el string no es un color hex válido:
///
/// ```
/// assert!(mi_crate::parse_hex_color("not-a-color").is_err());
/// assert!(mi_crate::parse_hex_color("#GG0000").is_err());
/// ```
pub fn parse_hex_color(s: &str) -> Result<(u8, u8, u8), String> {
    let s = s.strip_prefix('#').ok_or("Must start with #")?;
    if s.len() != 6 {
        return Err("Must be 6 hex digits".to_string());
    }
    let r = u8::from_str_radix(&s[0..2], 16).map_err(|e| e.to_string())?;
    let g = u8::from_str_radix(&s[2..4], 16).map_err(|e| e.to_string())?;
    let b = u8::from_str_radix(&s[4..6], 16).map_err(|e| e.to_string())?;
    Ok((r, g, b))
}
```

### Función genérica

```rust
/// Encuentra el valor máximo en un slice.
///
/// Retorna `None` si el slice está vacío.
///
/// ```
/// assert_eq!(mi_crate::max_of(&[3, 1, 4, 1, 5, 9]), Some(&9));
/// assert_eq!(mi_crate::max_of(&[42]), Some(&42));
/// assert_eq!(mi_crate::max_of::<i32>(&[]), None);
/// ```
///
/// Funciona con cualquier tipo que implemente `Ord`:
///
/// ```
/// assert_eq!(mi_crate::max_of(&["banana", "apple", "cherry"]), Some(&"cherry"));
/// ```
pub fn max_of<T: Ord>(slice: &[T]) -> Option<&T> {
    slice.iter().max()
}
```

### Función unsafe

```rust
/// Reinterpreta los bytes de un `u32` como un `f32`.
///
/// # Safety
///
/// Esta función es safe en todas las plataformas, ya que todos los
/// patrones de bits son válidos para `f32`. Se marca `unsafe` solo
/// como ejemplo educativo.
///
/// ```
/// let bits: u32 = 0x40490FDB;  // Representación IEEE 754 de PI
/// let value = unsafe { mi_crate::bits_to_float(bits) };
/// assert!((value - std::f64::consts::PI as f32).abs() < 0.0001);
/// ```
pub unsafe fn bits_to_float(bits: u32) -> f32 {
    std::mem::transmute(bits)
}
```

### Función con closure como parámetro

```rust
/// Filtra elementos de un slice según un predicado.
///
/// ```
/// let numbers = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
/// let evens = mi_crate::filter(&numbers, |n| n % 2 == 0);
/// assert_eq!(evens, vec![&2, &4, &6, &8, &10]);
/// ```
///
/// ```
/// let words = vec!["hello", "hi", "hey", "world"];
/// let h_words = mi_crate::filter(&words, |w| w.starts_with('h'));
/// assert_eq!(h_words, vec![&"hello", &"hi", &"hey"]);
/// ```
pub fn filter<'a, T>(slice: &'a [T], predicate: impl Fn(&T) -> bool) -> Vec<&'a T> {
    slice.iter().filter(|item| predicate(item)).collect()
}
```

---

## 11. Doc tests en structs y enums

### Struct: doc test en la definición

```rust
/// Un punto 2D con coordenadas flotantes.
///
/// # Examples
///
/// ```
/// let origin = mi_crate::Point::new(0.0, 0.0);
/// let p = mi_crate::Point::new(3.0, 4.0);
///
/// assert_eq!(p.distance_to(&origin), 5.0);
/// ```
pub struct Point {
    /// Coordenada X.
    ///
    /// ```
    /// let p = mi_crate::Point::new(3.0, 4.0);
    /// assert_eq!(p.x, 3.0);
    /// ```
    pub x: f64,

    /// Coordenada Y.
    ///
    /// ```
    /// let p = mi_crate::Point::new(3.0, 4.0);
    /// assert_eq!(p.y, 4.0);
    /// ```
    pub y: f64,
}

impl Point {
    /// Crea un nuevo punto.
    ///
    /// ```
    /// let p = mi_crate::Point::new(1.5, 2.5);
    /// assert_eq!(p.x, 1.5);
    /// assert_eq!(p.y, 2.5);
    /// ```
    pub fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }

    /// Calcula la distancia euclidiana a otro punto.
    ///
    /// ```
    /// let a = mi_crate::Point::new(0.0, 0.0);
    /// let b = mi_crate::Point::new(3.0, 4.0);
    /// assert_eq!(a.distance_to(&b), 5.0);
    /// ```
    ///
    /// La distancia es simétrica:
    ///
    /// ```
    /// let a = mi_crate::Point::new(1.0, 2.0);
    /// let b = mi_crate::Point::new(4.0, 6.0);
    /// assert_eq!(a.distance_to(&b), b.distance_to(&a));
    /// ```
    pub fn distance_to(&self, other: &Point) -> f64 {
        ((self.x - other.x).powi(2) + (self.y - other.y).powi(2)).sqrt()
    }
}
```

### Enum: doc test en cada variante

```rust
/// Resultado de una operación de parsing.
///
/// ```
/// use mi_crate::Token;
///
/// let tokens = vec![
///     Token::Number(42),
///     Token::Operator('+'),
///     Token::Number(8),
/// ];
/// assert_eq!(tokens.len(), 3);
/// ```
pub enum Token {
    /// Un valor numérico.
    ///
    /// ```
    /// let t = mi_crate::Token::Number(42);
    /// assert!(matches!(t, mi_crate::Token::Number(42)));
    /// ```
    Number(i32),

    /// Un operador aritmético.
    ///
    /// ```
    /// let t = mi_crate::Token::Operator('+');
    /// if let mi_crate::Token::Operator(op) = t {
    ///     assert_eq!(op, '+');
    /// }
    /// ```
    Operator(char),

    /// Un identificador (nombre de variable).
    ///
    /// ```
    /// let t = mi_crate::Token::Identifier("x".to_string());
    /// if let mi_crate::Token::Identifier(ref name) = t {
    ///     assert_eq!(name, "x");
    /// }
    /// ```
    Identifier(String),
}
```

### Struct con builder pattern

```rust
/// Configuración para el procesador de texto.
///
/// Se construye con el patrón builder:
///
/// ```
/// let config = mi_crate::TextConfig::new()
///     .max_width(80)
///     .indent(4)
///     .trim_whitespace(true);
///
/// assert_eq!(config.max_width(), 80);
/// assert_eq!(config.indent(), 4);
/// assert!(config.trim_whitespace());
/// ```
///
/// Valores por defecto:
///
/// ```
/// let default = mi_crate::TextConfig::new();
/// assert_eq!(default.max_width(), 120);
/// assert_eq!(default.indent(), 0);
/// assert!(!default.trim_whitespace());
/// ```
pub struct TextConfig {
    max_width_val: usize,
    indent_val: usize,
    trim: bool,
}

impl TextConfig {
    pub fn new() -> Self {
        TextConfig {
            max_width_val: 120,
            indent_val: 0,
            trim: false,
        }
    }

    /// Establece el ancho máximo de línea.
    ///
    /// ```
    /// let config = mi_crate::TextConfig::new().max_width(80);
    /// assert_eq!(config.max_width(), 80);
    /// ```
    pub fn max_width(mut self, width: usize) -> Self {
        self.max_width_val = width;
        self
    }

    /// Establece la indentación en espacios.
    ///
    /// ```
    /// let config = mi_crate::TextConfig::new().indent(4);
    /// assert_eq!(config.indent(), 4);
    /// ```
    pub fn indent(mut self, spaces: usize) -> Self {
        self.indent_val = spaces;
        self
    }

    /// Activa o desactiva el recorte de espacios.
    ///
    /// ```
    /// let config = mi_crate::TextConfig::new().trim_whitespace(true);
    /// assert!(config.trim_whitespace());
    /// ```
    pub fn trim_whitespace(mut self, trim: bool) -> Self {
        self.trim = trim;
        self
    }

    pub fn max_width(&self) -> usize { self.max_width_val }
    pub fn indent(&self) -> usize { self.indent_val }
    pub fn trim_whitespace(&self) -> bool { self.trim }
}
```

---

## 12. Doc tests en traits e implementaciones

### Doc test en definición de trait

```rust
/// Una colección que soporta operaciones LIFO (Last In, First Out).
///
/// # Examples
///
/// ```
/// use mi_crate::Stackable;
///
/// // Vec implementa Stackable
/// let mut stack: Vec<i32> = Vec::new();
/// stack.push_item(1);
/// stack.push_item(2);
/// stack.push_item(3);
///
/// assert_eq!(stack.pop_item(), Some(3));
/// assert_eq!(stack.peek_item(), Some(&2));
/// assert_eq!(stack.stack_size(), 2);
/// ```
pub trait Stackable<T> {
    /// Agrega un elemento al tope.
    ///
    /// ```
    /// use mi_crate::Stackable;
    /// let mut stack: Vec<i32> = Vec::new();
    /// stack.push_item(42);
    /// assert_eq!(stack.stack_size(), 1);
    /// ```
    fn push_item(&mut self, item: T);

    /// Remueve y retorna el elemento del tope.
    ///
    /// ```
    /// use mi_crate::Stackable;
    /// let mut stack = vec![1, 2, 3];
    /// assert_eq!(stack.pop_item(), Some(3));
    /// ```
    fn pop_item(&mut self) -> Option<T>;

    /// Retorna referencia al elemento del tope sin removerlo.
    ///
    /// ```
    /// use mi_crate::Stackable;
    /// let stack = vec![1, 2, 3];
    /// assert_eq!(stack.peek_item(), Some(&3));
    /// ```
    fn peek_item(&self) -> Option<&T>;

    /// Retorna el número de elementos.
    fn stack_size(&self) -> usize;
}
```

### Doc test en impl de trait

```rust
/// Implementación de `Display` para `Point`.
///
/// ```
/// let p = mi_crate::Point::new(3.5, -2.1);
/// assert_eq!(format!("{}", p), "(3.5, -2.1)");
/// ```
///
/// Funciona con `println!`:
///
/// ```
/// let p = mi_crate::Point::new(0.0, 0.0);
/// let s = format!("{}", p);
/// assert_eq!(s, "(0, 0)");
/// ```
impl std::fmt::Display for Point {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if self.x.fract() == 0.0 && self.y.fract() == 0.0 {
            write!(f, "({}, {})", self.x as i64, self.y as i64)
        } else {
            write!(f, "({}, {})", self.x, self.y)
        }
    }
}
```

### Doc test en impl genérico

```rust
/// Implementación para cualquier tipo que sea `Display`.
///
/// ```
/// use mi_crate::Wrapper;
///
/// let w = Wrapper::new(42);
/// assert_eq!(w.display_value(), "42");
///
/// let w = Wrapper::new("hello");
/// assert_eq!(w.display_value(), "hello");
///
/// let w = Wrapper::new(3.14);
/// assert_eq!(w.display_value(), "3.14");
/// ```
impl<T: std::fmt::Display> Wrapper<T> {
    pub fn display_value(&self) -> String {
        format!("{}", self.value)
    }
}
```

---

## 13. Doc tests en módulos y crates

### Doc test en nivel de crate (lib.rs)

```rust
// src/lib.rs

//! # Mi Crate
//!
//! Una biblioteca para procesamiento de texto.
//!
//! ## Quick Start
//!
//! ```
//! use mi_crate::TextProcessor;
//!
//! let processor = TextProcessor::new();
//! let result = processor.process("  hello world  ");
//! assert_eq!(result, "Hello World");
//! ```
//!
//! ## Features
//!
//! - Capitalización automática
//! - Recorte de espacios
//! - Normalización de whitespace
//!
//! ## Ejemplo completo
//!
//! ```
//! use mi_crate::{TextProcessor, ProcessorConfig};
//!
//! let config = ProcessorConfig::new()
//!     .capitalize(true)
//!     .trim(true)
//!     .normalize_spaces(true);
//!
//! let processor = TextProcessor::with_config(config);
//!
//! let texts = vec![
//!     "  hello   world  ",
//!     "ALREADY CAPS",
//!     "   spaces   everywhere   ",
//! ];
//!
//! for text in texts {
//!     let result = processor.process(text);
//!     assert!(!result.starts_with(' '));
//!     assert!(!result.ends_with(' '));
//!     assert!(!result.contains("  "));  // No doble espacio
//! }
//! ```

pub mod processor;
pub mod config;

pub use processor::TextProcessor;
pub use config::ProcessorConfig;
```

### Doc test en nivel de módulo

```rust
// src/parser.rs

//! Módulo de parsing para expresiones matemáticas.
//!
//! Este módulo convierte strings como `"2 + 3 * 4"` en árboles
//! de sintaxis abstracta (AST).
//!
//! # Examples
//!
//! ```
//! use mi_crate::parser;
//!
//! let ast = parser::parse("2 + 3").unwrap();
//! assert_eq!(ast.evaluate(), 5);
//! ```
//!
//! Soporta precedencia de operadores:
//!
//! ```
//! use mi_crate::parser;
//!
//! let ast = parser::parse("2 + 3 * 4").unwrap();
//! assert_eq!(ast.evaluate(), 14);  // 3*4=12, 2+12=14
//! ```

pub fn parse(input: &str) -> Result<Ast, ParseError> {
    todo!()
}
```

### Ubicación del doc test en el output

```
$ cargo test --doc

   Doc-tests mi_crate

running 7 tests
test src/lib.rs - (line 8) ... ok            ← Doc test del crate (//!)
test src/lib.rs - (line 22) ... ok           ← Doc test del crate (//!)
test src/parser.rs - parser (line 10) ... ok ← Doc test del módulo (//!)
test src/parser.rs - parser (line 18) ... ok ← Doc test del módulo (//!)
test src/lib.rs - Point (line 5) ... ok      ← Doc test del struct
test src/lib.rs - Point::new (line 3) ... ok ← Doc test del método
test src/lib.rs - reverse (line 4) ... ok    ← Doc test de función
```

El nombre del test indica:
- **Archivo** donde está el doc comment
- **Item** que documenta (función, struct, módulo)
- **Línea** donde empieza el bloque de código

---

## 14. Múltiples bloques de código en un doc comment

Un mismo doc comment puede tener varios bloques de código. Cada uno es un test independiente:

```rust
/// Convierte temperatura entre escalas.
///
/// # De Celsius a Fahrenheit
///
/// ```
/// let f = mi_crate::celsius_to_fahrenheit(100.0);
/// assert_eq!(f, 212.0);
///
/// let f = mi_crate::celsius_to_fahrenheit(0.0);
/// assert_eq!(f, 32.0);
/// ```
///
/// # Valores negativos
///
/// ```
/// let f = mi_crate::celsius_to_fahrenheit(-40.0);
/// assert_eq!(f, -40.0);  // -40 es igual en ambas escalas
/// ```
///
/// # Nota sobre precisión
///
/// Usa `f64` internamente, así que puede haber imprecisiones mínimas:
///
/// ```
/// let f = mi_crate::celsius_to_fahrenheit(37.0);
/// assert!((f - 98.6).abs() < 0.01);
/// ```
pub fn celsius_to_fahrenheit(c: f64) -> f64 {
    c * 9.0 / 5.0 + 32.0
}
```

Esto genera **3 doc tests** separados, cada uno compilado y ejecutado independientemente:

```
test src/lib.rs - celsius_to_fahrenheit (line 5) ... ok    ← Bloque 1
test src/lib.rs - celsius_to_fahrenheit (line 13) ... ok   ← Bloque 2
test src/lib.rs - celsius_to_fahrenheit (line 19) ... ok   ← Bloque 3
```

### Bloques que muestran diferentes aspectos

```rust
/// Un buffer circular de tamaño fijo.
///
/// # Creación
///
/// ```
/// let buf = mi_crate::RingBuffer::<i32>::new(5);
/// assert!(buf.is_empty());
/// assert_eq!(buf.capacity(), 5);
/// ```
///
/// # Inserción y lectura
///
/// ```
/// let mut buf = mi_crate::RingBuffer::new(3);
/// buf.push(1);
/// buf.push(2);
/// buf.push(3);
/// assert_eq!(buf.pop(), Some(1));  // FIFO
/// ```
///
/// # Overflow (sobrescribe los más antiguos)
///
/// ```
/// let mut buf = mi_crate::RingBuffer::new(3);
/// buf.push(1);
/// buf.push(2);
/// buf.push(3);
/// buf.push(4);  // Sobrescribe el 1
///
/// assert_eq!(buf.pop(), Some(2));  // El 1 fue sobrescrito
/// assert_eq!(buf.pop(), Some(3));
/// assert_eq!(buf.pop(), Some(4));
/// ```
///
/// # Iteración
///
/// ```
/// let mut buf = mi_crate::RingBuffer::new(4);
/// buf.push(10);
/// buf.push(20);
/// buf.push(30);
///
/// let values: Vec<_> = buf.iter().collect();
/// assert_eq!(values, vec![&10, &20, &30]);
/// ```
pub struct RingBuffer<T> {
    data: Vec<Option<T>>,
    head: usize,
    tail: usize,
    len: usize,
}
```

### Mezcla de bloques ejecutables y no ejecutables

```rust
/// Cliente HTTP simplificado.
///
/// # Uso básico
///
/// ```no_run
/// let client = mi_crate::HttpClient::new();
/// let response = client.get("https://api.example.com/data").unwrap();
/// println!("Status: {}", response.status());
/// ```
///
/// # Configuración
///
/// ```
/// let client = mi_crate::HttpClient::builder()
///     .timeout(30)
///     .retries(3)
///     .build();
///
/// assert_eq!(client.timeout(), 30);
/// assert_eq!(client.retries(), 3);
/// ```
///
/// # Manejo de errores
///
/// ```
/// # use mi_crate::HttpClient;
/// let client = HttpClient::new();
///
/// // URLs inválidas retornan error inmediatamente
/// let result = client.get("");
/// assert!(result.is_err());
/// ```
pub struct HttpClient { /* ... */ }
```

Aquí el primer bloque es `no_run` (requiere red), pero los otros dos sí se ejecutan y verifican la API.

---

## 15. Doc tests y visibilidad

### Solo items `pub` generan doc tests ejecutables

```rust
// src/lib.rs

/// Este doc test SÍ se ejecuta porque la función es pub.
///
/// ```
/// assert_eq!(mi_crate::public_fn(), 42);
/// ```
pub fn public_fn() -> i32 { 42 }

/// Este doc test NO se ejecuta porque la función es privada.
/// rustdoc lo ignora completamente para testing
/// (aunque sí lo incluye con --document-private-items).
///
/// ```
/// assert_eq!(mi_crate::private_fn(), 99);
/// ```
fn private_fn() -> i32 { 99 }

/// Este doc test NO se ejecuta: pub(crate) no es visible
/// para crates externos, y los doc tests son crates externos.
///
/// ```
/// assert_eq!(mi_crate::crate_fn(), 77);
/// ```
pub(crate) fn crate_fn() -> i32 { 77 }
```

```
Visibilidad y doc tests:

  pub fn foo()           → Doc test SÍ se ejecuta
  pub(crate) fn bar()    → Doc test NO se ejecuta ← Atención
  pub(super) fn baz()    → Doc test NO se ejecuta
  fn private()           → Doc test NO se ejecuta
  pub(in path) fn qux()  → Doc test NO se ejecuta

  Solo `pub` (sin restricciones) genera doc tests ejecutables.
  Esto es consistente: los doc tests son como integration tests,
  que son crates externos.
```

### Items pub dentro de módulos privados

```rust
// src/lib.rs
mod internal {
    /// Esta función es `pub` pero está en un módulo privado.
    /// El doc test NO se ejecuta porque no es accesible desde fuera.
    ///
    /// ```
    /// // Este bloque NO se ejecutará como test
    /// let x = mi_crate::internal::helper();
    /// ```
    pub fn helper() -> i32 { 42 }
}

// Si re-exportas, SÍ se vuelve accesible:
pub use internal::helper;
// Ahora el doc test en `helper` SÍ se ejecutará,
// y el path correcto será mi_crate::helper (no mi_crate::internal::helper)
```

### Consecuencia para re-exports

```rust
// src/lib.rs
pub mod math;

// Re-export para conveniencia
pub use math::Vector;
pub use math::Matrix;
```

```rust
// src/math.rs

/// Un vector 2D.
///
/// ```
/// // El doc test usa el path desde donde fue definido
/// let v = mi_crate::math::Vector::new(3.0, 4.0);
/// assert_eq!(v.magnitude(), 5.0);
/// ```
///
/// O via re-export:
///
/// ```
/// // También funciona gracias al pub use en lib.rs
/// let v = mi_crate::Vector::new(3.0, 4.0);
/// assert_eq!(v.magnitude(), 5.0);
/// ```
pub struct Vector {
    pub x: f64,
    pub y: f64,
}
```

Ambos paths funcionan en el doc test porque ambos son accesibles públicamente.

---

## 16. Doc tests con dependencias externas

### Usando dev-dependencies

Las dev-dependencies declaradas en `Cargo.toml` están disponibles en doc tests:

```toml
# Cargo.toml
[package]
name = "mi_crate"
version = "0.1.0"

[dev-dependencies]
tempfile = "3"
serde_json = "1"
```

```rust
/// Serializa datos a JSON.
///
/// ```
/// # use serde_json;  // dev-dependency disponible
/// let data = mi_crate::UserData::new("Alice", 30);
/// let json = data.to_json();
///
/// // Verificar con serde_json (dev-dependency)
/// let parsed: serde_json::Value = serde_json::from_str(&json).unwrap();
/// assert_eq!(parsed["name"], "Alice");
/// assert_eq!(parsed["age"], 30);
/// ```
pub struct UserData { /* ... */ }
```

### Usando la librería estándar

```rust
/// Cuenta las líneas de un string.
///
/// ```
/// use std::collections::HashMap;
///
/// let text = "hello\nworld\nhello\nrust\nhello";
/// let counts = mi_crate::count_word_lines(text);
///
/// let expected: HashMap<&str, usize> = [
///     ("hello", 3),
///     ("world", 1),
///     ("rust", 1),
/// ].iter().cloned().collect();
///
/// assert_eq!(counts, expected);
/// ```
pub fn count_word_lines(text: &str) -> std::collections::HashMap<&str, usize> {
    let mut counts = std::collections::HashMap::new();
    for line in text.lines() {
        *counts.entry(line.trim()).or_insert(0) += 1;
    }
    counts
}
```

### Usando features condicionales

```rust
/// Serializa a formato binario.
///
/// Solo disponible con la feature `binary`:
///
/// ```
/// #[cfg(feature = "binary")]
/// {
///     let data = mi_crate::Data::new(42);
///     let bytes = data.to_binary();
///     assert_eq!(bytes.len(), 4);
/// }
/// ```
#[cfg(feature = "binary")]
pub fn to_binary(&self) -> Vec<u8> {
    todo!()
}
```

---

## 17. Debugging de doc tests que fallan

### Mensaje de error típico

```
$ cargo test --doc

   Doc-tests mi_crate

running 5 tests
test src/lib.rs - add (line 5) ... FAILED

failures:

---- src/lib.rs - add (line 5) stdout ----
error[E0433]: failed to resolve: use of undeclared crate or module `mi_crate`
 --> src/lib.rs:7:9
  |
3 | let result = mi_crate::add(2, 3);
  |              ^^^^^^^^ use of undeclared crate or module `mi_crate`

error: aborting due to previous error
```

### Problemas comunes y soluciones

**Problema 1: Nombre del crate incorrecto**

```rust
/// ```
/// // Cargo.toml dice: name = "my-awesome-crate"
/// // Los guiones se convierten en underscores:
/// let x = my_awesome_crate::foo();  // ✓ Correcto
/// let x = my-awesome-crate::foo();  // ✗ Syntax error
/// ```
```

**Problema 2: No usar el path completo**

```rust
/// ```
/// // ✗ ERROR: `add` no está en scope
/// let x = add(2, 3);
///
/// // ✓ CORRECTO: usar path completo
/// let x = mi_crate::add(2, 3);
///
/// // ✓ TAMBIÉN CORRECTO: importar primero
/// use mi_crate::add;
/// let x = add(2, 3);
/// ```
```

**Problema 3: Trait no importado**

```rust
/// ```
/// let mut v = vec![3, 1, 2];
/// // Si mi_crate define un trait Sortable con método sort_desc:
///
/// // ✗ ERROR: método no encontrado
/// v.sort_desc();
///
/// // ✓ CORRECTO: importar el trait
/// use mi_crate::Sortable;
/// v.sort_desc();
/// ```
```

**Problema 4: Tipo inferido incorrectamente**

```rust
/// ```
/// // ✗ ERROR: tipo ambiguo
/// let v = vec![];
/// mi_crate::process(&v);
///
/// // ✓ CORRECTO: anotar el tipo
/// let v: Vec<i32> = vec![];
/// mi_crate::process(&v);
/// ```
```

### Técnica de debug: expandir el doc test

Para debuggear un doc test, puedes "expandirlo" manualmente. Crea un archivo temporal que simule lo que rustdoc genera:

```rust
// temp_debug.rs — Archivo temporal para debuggear

extern crate mi_crate;

fn main() {
    // Pega aquí el contenido del doc test (incluyendo líneas #):
    use mi_crate::Config;

    let c = Config::new();
    assert!(c.is_valid());
}
```

```bash
# Compilar y ejecutar manualmente:
$ rustc --edition 2021 --extern mi_crate=target/debug/libmi_crate.rlib temp_debug.rs
$ ./temp_debug
```

### Técnica: ver el output con --nocapture

```bash
$ cargo test --doc -- --nocapture

# Muestra println! y eprintln! de los doc tests
```

### Técnica: ejecutar un solo doc test

```bash
# Filtrar por nombre del item documentado
$ cargo test --doc add

# Filtrar por archivo
$ cargo test --doc parser

# Filtrar por línea (útil si hay múltiples bloques)
$ cargo test --doc "line 15"
```

---

## 18. Doc tests vs unit tests vs integration tests

### Tabla comparativa completa

| Aspecto | Unit test | Integration test | Doc test |
|---------|-----------|-----------------|----------|
| **Ubicación** | `src/`, en `mod tests` | `tests/` directorio | Doc comments (`///`, `//!`) |
| **Visibilidad** | Todo (pub + privado) | Solo `pub` | Solo `pub` |
| **Compilación** | 1 binario para todo src/ | 1 binario por archivo | 1 binario por bloque |
| **Necesita #[cfg(test)]** | Sí | No | No |
| **Necesita mod tests** | Sí | No | No |
| **Produce documentación** | No | No | **Sí** |
| **Propósito primario** | Verificar implementación | Verificar API | **Ejemplos vivos** |
| **Extensión típica** | 5-30 líneas | 10-100 líneas | 3-10 líneas |
| **Cantidad** | Muchos | Moderados | 1-3 por item público |
| **Velocidad** | Más rápido | Medio | Más lento (muchos binarios) |
| **Se rompe con refactor interno** | Sí | No | No |
| **Se rompe con cambio de API** | Sí | Sí | Sí |
| **Ejecutado con** | `cargo test --lib` | `cargo test --test X` | `cargo test --doc` |

### Cuándo usar cada uno

```
¿Qué tipo de test necesito?

  ¿Estoy probando lógica INTERNA (algoritmo, helper privado)?
  └─ SÍ → UNIT TEST
       (dentro del módulo, accede a privados)

  ¿Estoy probando un FLUJO que cruza módulos?
  └─ SÍ → INTEGRATION TEST
       (en tests/, usa solo API pública, tests complejos)

  ¿Estoy mostrando CÓMO USAR una función/struct/trait?
  └─ SÍ → DOC TEST
       (en doc comments, breve, sirve como documentación)

  Los tres NO son excluyentes — un item público debería tener:
    1. Unit tests para su lógica interna
    2. Integration tests para sus interacciones con otros módulos
    3. Doc tests para mostrar al usuario cómo usarlo
```

### La relación complementaria

```
Ejemplo: una función pub fn parse(input: &str) -> Result<Ast, Error>

  Doc test (en /// del item):
  ┌─────────────────────────────────────────────────┐
  │ /// ```                                         │
  │ /// let ast = mi_crate::parse("2 + 3").unwrap();│
  │ /// assert_eq!(ast.eval(), 5);                  │
  │ /// ```                                         │
  │                                                 │
  │ Propósito: Mostrar al USUARIO cómo llamar parse │
  │ Extensión: 2-3 líneas                           │
  │ Verifica: Caso feliz básico                     │
  └─────────────────────────────────────────────────┘

  Unit test (en mod tests del mismo archivo):
  ┌─────────────────────────────────────────────────┐
  │ #[test]                                         │
  │ fn test_tokenizer_handles_whitespace() {        │
  │     let tokens = tokenize("  2  +  3  ");       │
  │     assert_eq!(tokens.len(), 3);                │
  │ }                                               │
  │                                                 │
  │ Propósito: Verificar detalles de implementación │
  │ Extensión: 5-15 líneas                          │
  │ Verifica: Edge cases internos                   │
  └─────────────────────────────────────────────────┘

  Integration test (en tests/parsing.rs):
  ┌─────────────────────────────────────────────────┐
  │ #[test]                                         │
  │ fn test_parse_then_evaluate_complex() {         │
  │     let ast = parse("(2 + 3) * 4 - 1").unwrap()│
  │     let result = evaluate(&ast);                │
  │     assert_eq!(result, 19);                     │
  │ }                                               │
  │                                                 │
  │ Propósito: Verificar flujo parse→evaluate       │
  │ Extensión: 10-50 líneas                         │
  │ Verifica: Interacción entre componentes         │
  └─────────────────────────────────────────────────┘
```

---

## 19. Comparación con C y Go

### C: Sin doc tests (solo comentarios inertes)

```c
/**
 * @brief Suma dos enteros.
 *
 * @code
 * int result = add(2, 3);
 * assert(result == 5);  // Nadie ejecuta esto
 * @endcode
 *
 * Este ejemplo puede ser INCORRECTO.
 * El compilador nunca lo verifica.
 * Doxygen lo muestra en HTML pero no lo prueba.
 */
int add(int a, int b) {
    return a + b;
}
```

Para lograr algo similar en C, necesitarías:

1. Escribir el ejemplo en un archivo `.c` separado
2. Añadir una regla en el Makefile para compilar y ejecutar los ejemplos
3. Mantener sincronizados el ejemplo y el comentario Doxygen

Nadie hace esto en la práctica.

### Go: Example functions (explícitas, con output check)

```go
package mathlib

import "fmt"

// Add suma dos enteros.
func Add(a, b int) int {
    return a + b
}

// ExampleAdd demuestra el uso de Add.
// El comentario "// Output:" al final es verificado por go test.
func ExampleAdd() {
    result := Add(2, 3)
    fmt.Println(result)
    // Output: 5
}

// ExampleAdd_negative demuestra con números negativos.
func ExampleAdd_negative() {
    result := Add(-1, -2)
    fmt.Println(result)
    // Output: -3
}
```

Go verifica el **output estándar** — si `fmt.Println(result)` produce algo diferente a `5`, el test falla. Esto es más limitado que los `assert!` de Rust:

```
Go vs Rust doc tests:

  Go:
    + Las Example functions aparecen en godoc
    + Se ejecutan como tests
    + Convención clara (ExampleFoo, ExampleFoo_caso)
    - Solo pueden verificar OUTPUT (stdout)
    - Son funciones separadas de la documentación
    - Necesitan import "fmt" para imprimir
    - No verifican valores internos, solo output visible

  Rust:
    + Los ejemplos SON la documentación (misma ubicación)
    + Pueden usar assert!, assert_eq!, assert_ne!
    + Verifican valores directamente, no solo output
    + Código oculto con # para esconder boilerplate
    + compile_fail para verificar errores de tipo
    + Atributos no_run, ignore, should_panic
    - Cada bloque es un binario separado (más lento)
    - No pueden verificar output stdout fácilmente
```

### Tabla comparativa

| Aspecto | C (Doxygen) | Go (Example) | Rust (Doc test) |
|---------|-------------|--------------|-----------------|
| **Ubicación** | Comentario `@code` | Función `ExampleX()` | Bloque ` ``` ` en `///` |
| **Se ejecuta** | No | Sí | Sí |
| **Verificación** | Ninguna | Output (stdout) | Asserts directos |
| **Genera docs** | Sí (HTML) | Sí (godoc) | Sí (rustdoc) |
| **Sincronizado** | Manual | Automático | Automático |
| **Puede ocultar código** | No | No | Sí (con `#`) |
| **Verifica errores de tipo** | No | No | Sí (`compile_fail`) |
| **Velocidad** | N/A | Rápido | Lento (muchos binarios) |
| **Adopción** | Rara | Común | Universal |

---

## 20. Errores comunes

### Error 1: Olvidar el path completo del crate

```rust
/// ```
/// // ✗ ERROR: `add` no está en scope
/// let x = add(2, 3);
/// ```
///
/// ```
/// // ✓ CORRECTO:
/// let x = mi_crate::add(2, 3);
/// assert_eq!(x, 5);
/// ```
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

Este es el error más frecuente. Recuerda: el doc test es un crate externo, necesita `mi_crate::` o `use mi_crate::item`.

### Error 2: Usar ? sin fn main() -> Result

```rust
/// ```
/// // ✗ ERROR: ? requiere fn que retorne Result
/// let value = mi_crate::parse("42")?;
/// assert_eq!(value, 42);
/// ```
///
/// ```
/// // ✓ CORRECTO:
/// # fn main() -> Result<(), Box<dyn std::error::Error>> {
/// let value = mi_crate::parse("42")?;
/// assert_eq!(value, 42);
/// # Ok(())
/// # }
/// ```
pub fn parse(s: &str) -> Result<i32, std::num::ParseIntError> {
    s.parse()
}
```

### Error 3: Doc test en item privado (no se ejecuta)

```rust
/// Este doc test PARECE que funciona pero nunca se ejecuta.
///
/// ```
/// assert_eq!(2 + 2, 5);  // ← Esto está MAL pero cargo test NO lo detecta
/// ```
fn private_function() -> i32 { 42 }
// El test no se ejecuta porque la función es privada.
// cargo test reporta 0 doc tests y no muestra error.
```

Esto es insidioso porque no hay warning. El doc test simplemente no existe para rustdoc.

### Error 4: Asumir que los bloques comparten estado

```rust
/// ```
/// let mut x = 0;
/// x += 1;
/// ```
///
/// ```
/// // ✗ ERROR: x no existe aquí
/// // Cada bloque es un programa SEPARADO
/// assert_eq!(x, 1);
/// ```
```

Cada bloque ` ``` ` es un binario independiente. Las variables de un bloque no existen en otro.

### Error 5: Bloques de otros lenguajes ejecutados como Rust

```rust
/// Equivalente en Python:
///
/// ```
/// # Esto SE COMPILARÁ como Rust y FALLARÁ
/// x = 42
/// print(x)
/// ```
///
/// ```python
/// # ✓ CORRECTO: marcar como python
/// x = 42
/// print(x)
/// ```
///
/// ```text
/// # ✓ TAMBIÉN CORRECTO: marcar como text
/// x = 42
/// print(x)
/// ```
```

Un bloque ` ``` ` sin anotación de lenguaje se asume Rust. Para otros lenguajes, usa ` ```python `, ` ```c `, ` ```text `, etc.

### Error 6: # sin espacio (no oculta la línea)

```rust
/// ```
/// #use mi_crate::Foo;    // ✗ No se oculta — "#use" no es válido en Rust
/// # use mi_crate::Foo;   // ✓ Se oculta — "# " (hash + espacio) es la sintaxis
///
/// #[derive(Debug)]       // ✓ Esto es un atributo Rust, NO una línea oculta
///                        //   (no empieza con "# ", empieza con "#[")
/// # #[derive(Debug)]     // ✓ Esto OCULTA el #[derive(Debug)]
/// ```
```

La regla es: `# ` (hash seguido de espacio) al inicio de la línea oculta esa línea. `#` seguido de cualquier otra cosa (`[`, letra, etc.) es código Rust normal.

### Error 7: Doc test que no verifica nada

```rust
/// ```
/// let result = mi_crate::process("data");
/// // ¡No hay assert! Este test pasa siempre que no haya panic.
/// // No verifica que el resultado sea correcto.
/// ```
///
/// ```
/// // ✓ MEJOR: siempre verificar el resultado
/// let result = mi_crate::process("data");
/// assert_eq!(result, "processed_data");
/// ```
```

Un doc test sin `assert!` solo verifica que el código no crashea. Eso es mejor que nada, pero mucho peor que verificar el resultado.

### Error 8: Confundir compile_fail con should_panic

```rust
/// ```compile_fail
/// // Este test pasa si la COMPILACIÓN falla
/// // El código ni siquiera se ejecuta
/// let x: i32 = "not a number";  // Error de tipos → test pasa ✓
/// ```
///
/// ```should_panic
/// // Este test pasa si la EJECUCIÓN hace panic
/// // El código SÍ se compila y ejecuta
/// mi_crate::divide(10, 0);  // Panic en runtime → test pasa ✓
/// ```
```

`compile_fail` = error en tiempo de compilación. `should_panic` = panic en tiempo de ejecución.

### Error 9: Asumir que `cargo doc` ejecuta los tests

```bash
# ✗ cargo doc NO ejecuta doc tests — solo genera HTML
$ cargo doc

# ✓ cargo test ejecuta doc tests (entre otras cosas)
$ cargo test

# ✓ cargo test --doc ejecuta SOLO doc tests
$ cargo test --doc
```

`cargo doc` y `cargo test --doc` son comandos completamente diferentes:
- `cargo doc` → Genera documentación HTML
- `cargo test --doc` → Compila y ejecuta los bloques de código como tests

---

## 21. Ejemplo completo: biblioteca de formateo de texto

Vamos a construir una biblioteca donde cada item público tiene doc tests apropiados, demostrando todos los patrones vistos.

### Estructura del proyecto

```
text_fmt/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── case.rs
│   ├── wrap.rs
│   └── pad.rs
```

### Código fuente completo con doc tests

```rust
// src/lib.rs

//! # text_fmt
//!
//! Biblioteca de formateo de texto con funciones para cambiar
//! capitalización, envolver líneas, y alinear texto.
//!
//! # Quick Start
//!
//! ```
//! use text_fmt::{to_title_case, wrap, pad_center};
//!
//! let title = to_title_case("hello world");
//! assert_eq!(title, "Hello World");
//!
//! let wrapped = wrap("a long line of text here", 10);
//! assert_eq!(wrapped.len(), 3);  // 3 líneas
//!
//! let centered = pad_center("hi", 10);
//! assert_eq!(centered, "    hi    ");
//! ```
//!
//! # Módulos
//!
//! - [`case`] — Conversiones de capitalización
//! - [`wrap`] — Envolver texto por ancho
//! - [`pad`] — Alineación y padding

pub mod case;
pub mod wrap;
pub mod pad;

// Re-exports de conveniencia
pub use case::{to_title_case, to_snake_case, to_camel_case, to_kebab_case};
pub use wrap::wrap;
pub use pad::{pad_left, pad_right, pad_center};
```

```rust
// src/case.rs

//! Funciones de conversión de capitalización.
//!
//! ```
//! use text_fmt::case;
//!
//! assert_eq!(case::to_title_case("hello world"), "Hello World");
//! assert_eq!(case::to_snake_case("helloWorld"), "hello_world");
//! assert_eq!(case::to_camel_case("hello_world"), "helloWorld");
//! assert_eq!(case::to_kebab_case("hello_world"), "hello-world");
//! ```

/// Convierte un string a Title Case (primera letra de cada palabra en mayúscula).
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::to_title_case("hello world"), "Hello World");
/// assert_eq!(text_fmt::to_title_case("HELLO WORLD"), "Hello World");
/// assert_eq!(text_fmt::to_title_case(""), "");
/// ```
///
/// Maneja múltiples espacios:
///
/// ```
/// assert_eq!(text_fmt::to_title_case("hello   world"), "Hello   World");
/// ```
pub fn to_title_case(s: &str) -> String {
    let mut result = String::with_capacity(s.len());
    let mut capitalize_next = true;

    for ch in s.chars() {
        if ch.is_whitespace() {
            result.push(ch);
            capitalize_next = true;
        } else if capitalize_next {
            result.extend(ch.to_uppercase());
            capitalize_next = false;
        } else {
            result.extend(ch.to_lowercase());
        }
    }

    result
}

/// Convierte a snake_case.
///
/// Maneja camelCase, PascalCase, y strings con espacios.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::to_snake_case("helloWorld"), "hello_world");
/// assert_eq!(text_fmt::to_snake_case("HelloWorld"), "hello_world");
/// assert_eq!(text_fmt::to_snake_case("hello world"), "hello_world");
/// assert_eq!(text_fmt::to_snake_case("hello_world"), "hello_world");
/// ```
///
/// Maneja acrónimos:
///
/// ```
/// assert_eq!(text_fmt::to_snake_case("parseHTTPResponse"), "parse_http_response");
/// ```
pub fn to_snake_case(s: &str) -> String {
    let mut result = String::with_capacity(s.len() + 4);
    let mut prev_was_upper = false;
    let mut prev_was_separator = true;

    for (i, ch) in s.chars().enumerate() {
        if ch == ' ' || ch == '-' || ch == '_' {
            if !result.is_empty() && !prev_was_separator {
                result.push('_');
            }
            prev_was_separator = true;
            prev_was_upper = false;
        } else if ch.is_uppercase() {
            let next_is_lower = s.chars().nth(i + 1).map_or(false, |c| c.is_lowercase());
            if !prev_was_separator && (!prev_was_upper || next_is_lower) {
                result.push('_');
            }
            result.extend(ch.to_lowercase());
            prev_was_upper = true;
            prev_was_separator = false;
        } else {
            result.push(ch);
            prev_was_upper = false;
            prev_was_separator = false;
        }
    }

    result
}

/// Convierte a camelCase.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::to_camel_case("hello_world"), "helloWorld");
/// assert_eq!(text_fmt::to_camel_case("hello world"), "helloWorld");
/// assert_eq!(text_fmt::to_camel_case("Hello World"), "helloWorld");
/// assert_eq!(text_fmt::to_camel_case("hello-world"), "helloWorld");
/// ```
///
/// Un solo palabra queda en minúsculas:
///
/// ```
/// assert_eq!(text_fmt::to_camel_case("HELLO"), "hello");
/// assert_eq!(text_fmt::to_camel_case("hello"), "hello");
/// ```
pub fn to_camel_case(s: &str) -> String {
    let mut result = String::with_capacity(s.len());
    let mut capitalize_next = false;
    let mut first_word = true;

    for ch in s.chars() {
        if ch == '_' || ch == '-' || ch == ' ' {
            capitalize_next = true;
        } else if capitalize_next {
            if first_word {
                result.extend(ch.to_lowercase());
                first_word = false;
            } else {
                result.extend(ch.to_uppercase());
            }
            capitalize_next = false;
        } else if first_word {
            result.extend(ch.to_lowercase());
            first_word = false;
        } else {
            result.push(ch);
        }
    }

    result
}

/// Convierte a kebab-case.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::to_kebab_case("hello_world"), "hello-world");
/// assert_eq!(text_fmt::to_kebab_case("helloWorld"), "hello-world");
/// assert_eq!(text_fmt::to_kebab_case("Hello World"), "hello-world");
/// ```
///
/// ```
/// assert_eq!(text_fmt::to_kebab_case("already-kebab"), "already-kebab");
/// ```
pub fn to_kebab_case(s: &str) -> String {
    to_snake_case(s).replace('_', "-")
}
```

```rust
// src/wrap.rs

//! Envolver texto a un ancho máximo de línea.
//!
//! ```
//! let text = "The quick brown fox jumps over the lazy dog";
//! let lines = text_fmt::wrap(text, 20);
//! assert_eq!(lines.len(), 3);
//! ```

/// Envuelve texto dividiéndolo en líneas de ancho máximo `width`.
///
/// Divide por palabras — nunca corta una palabra a la mitad
/// (a menos que la palabra sea más larga que `width`).
///
/// # Examples
///
/// ```
/// let lines = text_fmt::wrap("hello world foo bar", 11);
/// assert_eq!(lines, vec!["hello world", "foo bar"]);
/// ```
///
/// Palabras más largas que el ancho se mantienen intactas:
///
/// ```
/// let lines = text_fmt::wrap("superlongword ok", 5);
/// assert_eq!(lines, vec!["superlongword", "ok"]);
/// ```
///
/// String vacío retorna un vector vacío:
///
/// ```
/// let lines = text_fmt::wrap("", 80);
/// assert!(lines.is_empty());
/// ```
///
/// # Panics
///
/// Panics si `width` es 0:
///
/// ```should_panic
/// text_fmt::wrap("hello", 0);
/// ```
pub fn wrap(text: &str, width: usize) -> Vec<String> {
    if width == 0 {
        panic!("wrap width must be greater than 0");
    }

    if text.is_empty() {
        return Vec::new();
    }

    let words: Vec<&str> = text.split_whitespace().collect();
    let mut lines = Vec::new();
    let mut current_line = String::new();

    for word in words {
        if current_line.is_empty() {
            current_line.push_str(word);
        } else if current_line.len() + 1 + word.len() <= width {
            current_line.push(' ');
            current_line.push_str(word);
        } else {
            lines.push(current_line);
            current_line = word.to_string();
        }
    }

    if !current_line.is_empty() {
        lines.push(current_line);
    }

    lines
}
```

```rust
// src/pad.rs

//! Funciones de alineación con padding.
//!
//! ```
//! assert_eq!(text_fmt::pad_left("hi", 10), "        hi");
//! assert_eq!(text_fmt::pad_right("hi", 10), "hi        ");
//! assert_eq!(text_fmt::pad_center("hi", 10), "    hi    ");
//! ```

/// Alinea a la derecha rellenando con espacios a la izquierda.
///
/// Si el texto ya es más largo que `width`, se retorna sin cambios.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::pad_left("hello", 10), "     hello");
/// assert_eq!(text_fmt::pad_left("hello", 5), "hello");
/// assert_eq!(text_fmt::pad_left("hello", 3), "hello");  // No trunca
/// ```
///
/// Útil para alinear números:
///
/// ```
/// let numbers = vec![1, 42, 100, 7];
/// let formatted: Vec<String> = numbers
///     .iter()
///     .map(|n| text_fmt::pad_left(&n.to_string(), 5))
///     .collect();
///
/// assert_eq!(formatted[0], "    1");
/// assert_eq!(formatted[1], "   42");
/// assert_eq!(formatted[2], "  100");
/// assert_eq!(formatted[3], "    7");
/// ```
pub fn pad_left(s: &str, width: usize) -> String {
    if s.len() >= width {
        s.to_string()
    } else {
        format!("{:>width$}", s, width = width)
    }
}

/// Alinea a la izquierda rellenando con espacios a la derecha.
///
/// Si el texto ya es más largo que `width`, se retorna sin cambios.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::pad_right("hello", 10), "hello     ");
/// assert_eq!(text_fmt::pad_right("hello", 5), "hello");
/// ```
///
/// Útil para columnas de texto:
///
/// ```
/// let labels = vec!["Name", "Age", "Email"];
/// let formatted: Vec<String> = labels
///     .iter()
///     .map(|l| text_fmt::pad_right(l, 10))
///     .collect();
///
/// assert_eq!(formatted[0], "Name      ");
/// assert_eq!(formatted[1], "Age       ");
/// assert_eq!(formatted[2], "Email     ");
/// ```
pub fn pad_right(s: &str, width: usize) -> String {
    if s.len() >= width {
        s.to_string()
    } else {
        format!("{:<width$}", s, width = width)
    }
}

/// Centra el texto rellenando con espacios a ambos lados.
///
/// Si el ancho sobrante es impar, el espacio extra va a la derecha.
///
/// # Examples
///
/// ```
/// assert_eq!(text_fmt::pad_center("hi", 10), "    hi    ");
/// assert_eq!(text_fmt::pad_center("hi", 11), "    hi     ");  // Extra a la derecha
/// assert_eq!(text_fmt::pad_center("hello", 5), "hello");
/// assert_eq!(text_fmt::pad_center("hello", 3), "hello");  // No trunca
/// ```
///
/// Útil para títulos:
///
/// ```
/// let title = text_fmt::pad_center("= Report =", 40);
/// assert_eq!(title.len(), 40);
/// assert!(title.starts_with(' '));
/// assert!(title.ends_with(' '));
/// ```
pub fn pad_center(s: &str, width: usize) -> String {
    if s.len() >= width {
        s.to_string()
    } else {
        format!("{:^width$}", s, width = width)
    }
}
```

### Output de cargo test --doc

```
$ cargo test --doc

   Doc-tests text_fmt

running 27 tests
test src/case.rs - case (line 5) ... ok
test src/case.rs - case::to_camel_case (line 5) ... ok
test src/case.rs - case::to_camel_case (line 14) ... ok
test src/case.rs - case::to_kebab_case (line 5) ... ok
test src/case.rs - case::to_kebab_case (line 12) ... ok
test src/case.rs - case::to_snake_case (line 7) ... ok
test src/case.rs - case::to_snake_case (line 16) ... ok
test src/case.rs - case::to_title_case (line 5) ... ok
test src/case.rs - case::to_title_case (line 13) ... ok
test src/lib.rs - (line 8) ... ok
test src/pad.rs - pad (line 4) ... ok
test src/pad.rs - pad::pad_center (line 5) ... ok
test src/pad.rs - pad::pad_center (line 14) ... ok
test src/pad.rs - pad::pad_left (line 5) ... ok
test src/pad.rs - pad::pad_left (line 13) ... ok
test src/pad.rs - pad::pad_right (line 5) ... ok
test src/pad.rs - pad::pad_right (line 12) ... ok
test src/wrap.rs - wrap (line 4) ... ok
test src/wrap.rs - wrap::wrap (line 5) ... ok
test src/wrap.rs - wrap::wrap (line 12) ... ok
test src/wrap.rs - wrap::wrap (line 19) ... ok
test src/wrap.rs - wrap::wrap (line 27) ... ok
test result: ok. 22 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

### Lo que el usuario ve en `cargo doc`

```
cargo doc --open genera:

  ┌──────────────────────────────────────────────────┐
  │  text_fmt                                        │
  │  ────────                                        │
  │  Biblioteca de formateo de texto con funciones   │
  │  para cambiar capitalización, envolver líneas,   │
  │  y alinear texto.                                │
  │                                                  │
  │  Quick Start                                     │
  │  ───────────                                     │
  │  ┌────────────────────────────────────────────┐  │
  │  │ use text_fmt::{to_title_case, wrap, ...};  │  │
  │  │                                            │  │
  │  │ let title = to_title_case("hello world");  │  │
  │  │ assert_eq!(title, "Hello World");           │  │
  │  │                                            │  │
  │  │ let wrapped = wrap("a long line...", 10);  │  │
  │  │ assert_eq!(wrapped.len(), 3);              │  │
  │  └────────────────────────────────────────────┘  │
  │                                                  │
  │  Módulos                                         │
  │  ─────────                                       │
  │  case — Conversiones de capitalización            │
  │  wrap — Envolver texto por ancho                  │
  │  pad  — Alineación y padding                      │
  └──────────────────────────────────────────────────┘

  Los bloques de código aparecen con syntax highlighting,
  un botón "Run" (en playground), y siempre están actualizados.
```

---

## 22. Programa de práctica

Implementa una biblioteca **`duration_fmt`** que convierta duraciones numéricas a formatos legibles para humanos, y viceversa. **Cada item público debe tener doc tests**.

### API a implementar

```rust
// src/lib.rs

//! # duration_fmt
//!
//! Formato legible para duraciones.
//!
//! (escribe doc tests de nivel de crate aquí)

pub mod format;
pub mod parse;

pub use format::{format_duration, format_duration_short, DurationParts};
pub use parse::{parse_duration, ParseError};
```

```rust
// src/format.rs

/// Las partes descompuestas de una duración.
///
/// (doc test: crear DurationParts y verificar campos)
pub struct DurationParts {
    pub days: u64,
    pub hours: u64,
    pub minutes: u64,
    pub seconds: u64,
}

impl DurationParts {
    /// Descompone segundos totales en días, horas, minutos, segundos.
    ///
    /// (doc test con valores conocidos: 90061 = 1d 1h 1m 1s)
    pub fn from_seconds(total: u64) -> Self { todo!() }

    /// Retorna los segundos totales.
    ///
    /// (doc test: round-trip con from_seconds)
    pub fn total_seconds(&self) -> u64 { todo!() }
}

/// Formatea segundos en formato largo: "1 day, 2 hours, 3 minutes, 4 seconds".
///
/// (doc test: entrada simple, entrada con ceros, entrada 0)
///
/// Reglas:
/// - Omitir componentes que son 0 (3600 → "1 hour", no "1 hour, 0 minutes, 0 seconds")
/// - Singular vs plural ("1 hour" vs "2 hours")
/// - 0 segundos → "0 seconds"
///
/// (doc test: verificar singular/plural)
/// (doc test: verificar omisión de ceros)
pub fn format_duration(seconds: u64) -> String { todo!() }

/// Formatea en formato corto: "1d 2h 3m 4s".
///
/// (doc test: entradas simples)
///
/// Reglas:
/// - Omitir componentes que son 0
/// - 0 → "0s"
///
/// (doc test: verificar omisión de ceros)
pub fn format_duration_short(seconds: u64) -> String { todo!() }
```

```rust
// src/parse.rs

/// Error de parsing de duración.
///
/// (doc test con Display: verificar mensaje de error)
#[derive(Debug, PartialEq)]
pub enum ParseError {
    EmptyInput,
    InvalidNumber(String),
    InvalidUnit(String),
    InvalidFormat(String),
}

/// Parsea una duración en formato humano a segundos.
///
/// Acepta formatos:
/// - "30s", "5m", "2h", "1d"
/// - "1h 30m", "2d 12h", "1d 2h 3m 4s"
/// - "1 hour", "2 days", "30 minutes"
/// - "1 hour, 30 minutes"
///
/// (doc test: formato corto)
/// (doc test: formato largo)
/// (doc test: formato mixto)
/// (doc test: errores)
pub fn parse_duration(input: &str) -> Result<u64, ParseError> { todo!() }
```

### Requisitos de doc tests

1. **Cada función pública** debe tener al menos 2 bloques de doc test
2. **Cada struct/enum público** debe tener al menos 1 doc test
3. **El módulo `lib.rs`** debe tener un doc test de nivel de crate con Quick Start
4. **Cada submódulo** (`format.rs`, `parse.rs`) debe tener un doc test de módulo con `//!`
5. **Al menos 1 bloque `should_panic`** (por ejemplo, algún caso de panic documentado)
6. **Al menos 1 bloque `compile_fail`** (por ejemplo, mostrar que no se puede crear un ParseError sin usar el constructor)
7. **Al menos 3 líneas ocultas con `#`** en doc tests (ocultar imports, fn main, etc.)
8. **Al menos 1 doc test con `?`** y `fn main() -> Result` oculto
9. **`DurationParts::from_seconds` → `total_seconds`** debe tener un doc test de round-trip
10. **`parse_duration` → `format_duration`** debe tener un doc test de round-trip

### Valores de referencia para doc tests

```
Segundos → Formato largo                → Formato corto
0        → "0 seconds"                  → "0s"
1        → "1 second"                   → "1s"
60       → "1 minute"                   → "1m"
61       → "1 minute, 1 second"         → "1m 1s"
3600     → "1 hour"                     → "1h"
3661     → "1 hour, 1 minute, 1 second" → "1h 1m 1s"
86400    → "1 day"                      → "1d"
90061    → "1 day, 1 hour, 1 minute, 1 second" → "1d 1h 1m 1s"
172800   → "2 days"                     → "2d"
```

### Verificación

```bash
# Los doc tests deben pasar
$ cargo test --doc

# Debe generar documentación limpia
$ cargo doc --no-deps

# Verificar que hay suficientes doc tests
$ cargo test --doc 2>&1 | grep "running"
# Debe mostrar al menos 20 doc tests
```

---

## 23. Ejercicios

### Ejercicio 1: Análisis de doc tests

Dado el siguiente código, responde las preguntas:

```rust
// src/lib.rs
pub mod math;
pub use math::clamp;

// src/math.rs

/// Limita un valor al rango [min, max].
///
/// ```
/// assert_eq!(mi_crate::clamp(5, 0, 10), 5);
/// assert_eq!(mi_crate::clamp(-5, 0, 10), 0);
/// assert_eq!(mi_crate::clamp(15, 0, 10), 10);
/// ```
///
/// ```
/// assert_eq!(mi_crate::math::clamp(5, 0, 10), 5);
/// ```
///
/// ```compile_fail
/// let x: String = mi_crate::clamp(5, 0, 10);
/// ```
///
/// ```
/// let result = clamp(5, 0, 10);
/// ```
pub fn clamp(value: i32, min: i32, max: i32) -> i32 {
    if value < min { min }
    else if value > max { max }
    else { value }
}

fn internal_helper() -> i32 { 42 }

/// ```
/// assert_eq!(mi_crate::math::internal_helper(), 42);
/// ```

pub(crate) fn crate_only() -> i32 { 99 }

/// ```
/// assert_eq!(mi_crate::math::crate_only(), 99);
/// ```
```

**a)** ¿Cuántos doc tests se ejecutarán con `cargo test --doc`? Identifica cada uno y di si pasa o falla.

**b)** El cuarto bloque (el que usa `clamp(5, 0, 10)` sin path) — ¿compila o falla? ¿Por qué?

**c)** ¿Los doc tests de `internal_helper` y `crate_only` se ejecutan? ¿Por qué?

**d)** Si agregaras `pub use math::internal_helper;` en `lib.rs`, ¿cambiaría la respuesta de (c)?

---

### Ejercicio 2: Escribir doc tests para API existente

Dada esta API sin documentación, escribe doc tests apropiados:

```rust
pub struct Matrix {
    data: Vec<Vec<f64>>,
    rows: usize,
    cols: usize,
}

impl Matrix {
    pub fn new(rows: usize, cols: usize) -> Self;
    pub fn identity(size: usize) -> Self;
    pub fn get(&self, row: usize, col: usize) -> f64;
    pub fn set(&mut self, row: usize, col: usize, value: f64);
    pub fn rows(&self) -> usize;
    pub fn cols(&self) -> usize;
    pub fn transpose(&self) -> Matrix;
    pub fn multiply(&self, other: &Matrix) -> Result<Matrix, String>;
}

impl std::fmt::Display for Matrix {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result;
}
```

**a)** Escribe doc tests para `new`, `identity`, `get`, `set`, `transpose`. Al menos 2 bloques por método.

**b)** Escribe doc tests para `multiply` que incluyan:
- Caso exitoso
- Caso de error (dimensiones incompatibles)
- Un bloque con `should_panic` (unwrap del error)

**c)** Escribe un doc test para `Display` que verifique el formato de salida.

**d)** Escribe un doc test de nivel de crate (`//!` en lib.rs) que muestre un ejemplo completo de crear una matriz, modificarla, transponerla, y multiplicarla.

---

### Ejercicio 3: De doc tests rotos a doc tests correctos

Cada uno de estos doc tests tiene un error. Identifícalo y escribe la versión corregida:

**Fragmento A:**
```rust
/// ```
/// let v = Vec::new();
/// v.push(1);
/// assert_eq!(v.len(), 1);
/// ```
pub fn example_a() {}
```

**Fragmento B:**
```rust
/// ```
/// let result = mi_crate::parse("42")?;
/// assert_eq!(result, 42);
/// ```
pub fn parse(s: &str) -> Result<i32, std::num::ParseIntError> {
    s.parse()
}
```

**Fragmento C:**
```rust
/// ```
/// let s = mi_crate::greet("world");
/// assert_eq!(s, "Hello, world!");
///
/// let s2 = mi_crate::greet("Rust");
/// assert_eq!(s, "Hello, Rust!");
/// ```
pub fn greet(name: &str) -> String {
    format!("Hello, {}!", name)
}
```

**Fragmento D:**
```rust
/// Uso en Python:
/// ```
/// x = my_module.process("hello")
/// print(x)
/// ```
pub fn process(s: &str) -> String {
    s.to_uppercase()
}
```

**Fragmento E:**
```rust
/// ```
/// use mi_crate::Config;
///
/// let c = Config::builder()
///     .name("test")
///     .timeout(30)
///     .build();
///
/// assert_eq!(c.name(), "test")
/// assert_eq!(c.timeout(), 30);
/// ```
```

---

### Ejercicio 4: Diseño de documentación completa

Diseña la documentación completa (con doc tests) para este trait y una implementación:

```rust
pub trait Summarizable {
    fn summary(&self) -> String;
    fn word_count(&self) -> usize;
    fn truncate(&self, max_words: usize) -> String;
}
```

**a)** Escribe doc tests para el trait `Summarizable` que muestren un ejemplo genérico de uso (asumiendo que `Article` lo implementa).

**b)** Implementa `Summarizable` para un struct `Article` con campos `title`, `author`, `body`. Escribe doc tests para cada método de la implementación.

**c)** Añade un doc test con `compile_fail` que demuestre que un struct que no implementa `Summarizable` no puede ser usado donde se espera uno.

**d)** Añade un doc test de nivel de módulo (`//!`) que muestre un "mini tutorial" de 10 líneas usando el trait y la implementación.

---

## Navegación

- **Anterior**: [T01 - Integration tests](../T01_Integration_tests/README.md)
- **Siguiente**: [T03 - Test helpers compartidos](../T03_Test_helpers_compartidos/README.md)

---

> **Sección 2: Integration Tests y Doc Tests** — Tópico 2 de 4 completado
>
> - T01: Integration tests — directorio tests/, crate separado, API pública, helpers compartidos ✓
> - T02: Doc tests — ` ``` ` en doc comments, compilación por rustdoc, no_run, ignore, compile_fail ✓
> - T03: Test helpers compartidos (siguiente)
> - T04: cargo test flags
