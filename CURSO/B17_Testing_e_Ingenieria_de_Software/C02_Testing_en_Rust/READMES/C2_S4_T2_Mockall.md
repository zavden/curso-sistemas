# T02 - mockall crate: mocking automático y expresivo en Rust

## Índice

1. [Por qué automatizar los mocks](#1-por-qué-automatizar-los-mocks)
2. [mockall: panorama del crate](#2-mockall-panorama-del-crate)
3. [Instalación y configuración](#3-instalación-y-configuración)
4. [#[automock]: generación automática de mocks](#4-automock-generación-automática-de-mocks)
5. [Anatomía de un mock generado](#5-anatomía-de-un-mock-generado)
6. [expect_*: configurar expectativas](#6-expect_-configurar-expectativas)
7. [returning: definir valores de retorno](#7-returning-definir-valores-de-retorno)
8. [times: controlar cuántas veces se invoca](#8-times-controlar-cuántas-veces-se-invoca)
9. [withf: predicados sobre argumentos](#9-withf-predicados-sobre-argumentos)
10. [with: matchers predefinidos](#10-with-matchers-predefinidos)
11. [Sequences: orden de invocación](#11-sequences-orden-de-invocación)
12. [Checkpoints: verificación parcial](#12-checkpoints-verificación-parcial)
13. [returning vs return_once vs return_const](#13-returning-vs-return_once-vs-return_const)
14. [Mocking de traits genéricos](#14-mocking-de-traits-genéricos)
15. [Mocking de traits con tipos asociados](#15-mocking-de-traits-con-tipos-asociados)
16. [Mocking de traits async](#16-mocking-de-traits-async)
17. [Mocking de structs concretos (#[automock] en impl)](#17-mocking-de-structs-concretos-automock-en-impl)
18. [Mocking de funciones libres (mock!)](#18-mocking-de-funciones-libres-mock)
19. [mock!: definición manual de mocks](#19-mock-definición-manual-de-mocks)
20. [Múltiples traits en un mock](#20-múltiples-traits-en-un-mock)
21. [Mocking con lifetimes y referencias](#21-mocking-con-lifetimes-y-referencias)
22. [Errores comunes y soluciones](#22-errores-comunes-y-soluciones)
23. [Patrones avanzados](#23-patrones-avanzados)
24. [mockall vs alternativas](#24-mockall-vs-alternativas)
25. [Comparación con C y Go](#25-comparación-con-c-y-go)
26. [Anti-patrones con mockall](#26-anti-patrones-con-mockall)
27. [Ejemplo completo: sistema de e-commerce](#27-ejemplo-completo-sistema-de-e-commerce)
28. [Programa de práctica](#28-programa-de-práctica)
29. [Ejercicios](#29-ejercicios)

---

## 1. Por qué automatizar los mocks

En T01 construimos mocks manuales. Para un trait con 2 métodos, esto es manejable. Pero en un proyecto real:

```
Trait UserRepository         → 8 métodos
Trait PaymentGateway         → 5 métodos
Trait NotificationService    → 4 métodos
Trait AuditLog               → 6 métodos
────────────────────────────────────
Total de boilerplate manual: 23 implementaciones mock
```

El costo del mocking manual escala **linealmente** con cada método y cada variante de comportamiento:

```
┌─────────────────────────────────────────────────────────┐
│   Mock manual para 1 trait con 5 métodos:               │
│                                                         │
│   struct MockFoo {                                      │
│       field_1: Box<dyn Fn(...) -> ...>,  // método 1    │
│       field_2: Box<dyn Fn(...) -> ...>,  // método 2    │
│       field_3: Box<dyn Fn(...) -> ...>,  // método 3    │
│       field_4: Box<dyn Fn(...) -> ...>,  // método 4    │
│       field_5: Box<dyn Fn(...) -> ...>,  // método 5    │
│       calls: RefCell<Vec<CallRecord>>,   // spy          │
│   }                                                     │
│                                                         │
│   + impl Foo for MockFoo { ... }     // 5 métodos       │
│   + impl MockFoo { new(), ... }      // constructores   │
│   ≈ 80-120 líneas de boilerplate                        │
└─────────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│   Con mockall:                                          │
│                                                         │
│   #[automock]                                           │
│   trait Foo {                                           │
│       fn method_1(...) -> ...;                          │
│       fn method_2(...) -> ...;                          │
│       fn method_3(...) -> ...;                          │
│       fn method_4(...) -> ...;                          │
│       fn method_5(...) -> ...;                          │
│   }                                                     │
│                                                         │
│   // MockFoo generado automáticamente                   │
│   // 0 líneas de boilerplate extra                      │
└─────────────────────────────────────────────────────────┘
```

### Las 5 razones para automatizar

| # | Razón | Manual | mockall |
|---|-------|--------|---------|
| 1 | Boilerplate | ~20 líneas/método | 0 |
| 2 | Verificación de llamadas | `RefCell<Vec<...>>` manual | `times()` built-in |
| 3 | Verificación de argumentos | `assert!` en closure | `withf()` declarativo |
| 4 | Orden de llamadas | Propio, error-prone | `Sequence` built-in |
| 5 | Mantenimiento al cambiar trait | Actualizar mock a mano | Recompilación automática |

### Cuándo NO automatizar

Los mocks manuales siguen siendo mejores cuando:

- **Fakes con lógica real**: un `InMemoryDatabase` que simula queries reales es un fake, no un mock. mockall no genera lógica de negocio.
- **Traits triviales**: un trait con 1 método simple — el mock manual son 10 líneas.
- **Tests donde el mock ES la documentación**: a veces ver la implementación explícita del mock comunica mejor la intención del test.

---

## 2. mockall: panorama del crate

```
┌──────────────────────────────────────────────────────────────────┐
│                        mockall crate                             │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Macro principal              Macro auxiliar                     │
│  ┌──────────────┐            ┌──────────────┐                   │
│  │  #[automock]  │            │   mock! { }  │                   │
│  └──────┬───────┘            └──────┬───────┘                   │
│         │                           │                            │
│    Para traits                Para structs,                      │
│    definidos por              funciones libres,                  │
│    ti                         traits de otros                    │
│                               crates                             │
│                                                                  │
│  Genera:  Mock{TraitName}                                        │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Cada método `foo` genera:                               │    │
│  │                                                          │    │
│  │  mock.expect_foo()                                       │    │
│  │      .withf(|arg| predicate)     // verificar args       │    │
│  │      .times(n)                    // cuántas veces        │    │
│  │      .returning(|args| value)     // qué retornar        │    │
│  │      .in_sequence(&seq)           // en qué orden        │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Versión: 0.13.x (Rust 1.71+)                                   │
│  MSRV: Rust 1.71                                                 │
│  Licencia: MIT/Apache-2.0                                        │
│  Descargas: >60M en crates.io                                    │
│  Autor: Alan Somers (@asomers)                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Arquitectura interna (simplificada)

```
#[automock]                    Código generado
trait Foo {                    ┌──────────────────────────────────────┐
    fn bar(&self,              │ pub struct MockFoo {                 │
        x: i32                 │     // Campo interno por método     │
    ) -> String;               │     _bar: Expectation<(i32,), String>│
}                              │ }                                    │
     │                         │                                      │
     │   proc macro            │ impl MockFoo {                      │
     └────────────►            │     pub fn new() -> Self { ... }    │
                               │     pub fn expect_bar(&mut self)    │
                               │         -> &mut Expectation<...>    │
                               │     { ... }                          │
                               │ }                                    │
                               │                                      │
                               │ impl Foo for MockFoo {              │
                               │     fn bar(&self, x: i32) -> String │
                               │     { self._bar.call((x,)) }       │
                               │ }                                    │
                               └──────────────────────────────────────┘
```

Internamente, cada `Expectation` almacena:

```
Expectation<Args, Return>
├── matcher: Option<Box<dyn Fn(&Args) -> bool>>    // withf
├── returning: Option<Box<dyn Fn(Args) -> Return>> // returning
├── times: TimesRange                               // times
├── sequence: Option<(SequenceHandle, usize)>       // in_sequence
├── call_count: AtomicUsize                         // cuántas veces se llamó
└── description: String                             // para mensajes de error
```

---

## 3. Instalación y configuración

### Cargo.toml

```toml
[package]
name = "my_project"
version = "0.1.0"
edition = "2021"

[dependencies]
# (tus dependencias de producción)

[dev-dependencies]
mockall = "0.13"
```

**Importante**: mockall solo va en `[dev-dependencies]` porque los mocks solo se usan en tests. El atributo `#[automock]` se aplica al trait en el código fuente, pero el struct generado (`MockFoo`) solo existe cuando se compila con `cfg(test)`.

### Compilación condicional

```rust
// El trait existe en producción Y tests
#[cfg_attr(test, mockall::automock)]
pub trait Database {
    fn get(&self, key: &str) -> Option<String>;
}
```

Con `#[cfg_attr(test, ...)]`:
- En **producción** (`cargo build`): solo existe el trait `Database`
- En **tests** (`cargo test`): existe el trait `Database` Y el struct `MockDatabase`

Sin `cfg_attr` (usando `#[automock]` directamente):
- El mock se genera siempre, pero como mockall está en dev-dependencies, la compilación de producción falla con un `unresolved import`

### Alternativa: importar solo en tests

```rust
// src/traits.rs
pub trait Database {
    fn get(&self, key: &str) -> Option<String>;
}

// tests/integration.rs
use mockall::automock;

// Redefinir el trait con #[automock] solo aquí
// (poco práctico — duplica la definición)
```

La convención más limpia:

```rust
// src/traits.rs
#[cfg_attr(test, mockall::automock)]
pub trait Database {
    fn get(&self, key: &str) -> Option<String>;
    fn set(&self, key: &str, value: &str) -> Result<(), String>;
    fn delete(&self, key: &str) -> Result<(), String>;
}
```

### Estructura de proyecto recomendada

```
my_project/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── traits.rs          // traits con #[cfg_attr(test, automock)]
│   ├── services.rs        // lógica de negocio genérica sobre traits
│   └── implementations.rs // implementaciones reales
└── tests/
    └── integration.rs     // tests de integración
```

---

## 4. #[automock]: generación automática de mocks

### Uso básico

```rust
use mockall::automock;

#[automock]
pub trait UserRepository {
    fn find_by_id(&self, id: u64) -> Option<User>;
    fn save(&mut self, user: &User) -> Result<u64, DbError>;
    fn delete(&mut self, id: u64) -> Result<(), DbError>;
    fn find_by_email(&self, email: &str) -> Option<User>;
    fn count(&self) -> usize;
}
```

Esto genera `MockUserRepository` con:

```
MockUserRepository
├── ::new()                → constructor
├── .expect_find_by_id()   → configurar expectativa
├── .expect_save()         → configurar expectativa
├── .expect_delete()       → configurar expectativa
├── .expect_find_by_email()→ configurar expectativa
└── .expect_count()        → configurar expectativa
```

### Regla de nombres

```
trait Foo          →  MockFoo
trait UserStore    →  MockUserStore
trait HttpClient   →  MockHttpClient
```

El prefijo `Mock` se añade al nombre del trait. Si el trait ya empieza con `Mock`, se genera `MockMockFoo` (evitar esto).

### Test básico

```rust
#[derive(Debug, Clone, PartialEq)]
pub struct User {
    pub id: u64,
    pub name: String,
    pub email: String,
}

#[derive(Debug, PartialEq)]
pub enum DbError {
    NotFound,
    DuplicateKey,
    ConnectionFailed,
}

#[automock]
pub trait UserRepository {
    fn find_by_id(&self, id: u64) -> Option<User>;
    fn save(&mut self, user: &User) -> Result<u64, DbError>;
}

pub struct UserService<R: UserRepository> {
    repo: R,
}

impl<R: UserRepository> UserService<R> {
    pub fn new(repo: R) -> Self {
        UserService { repo }
    }

    pub fn get_display_name(&self, id: u64) -> String {
        match self.repo.find_by_id(id) {
            Some(user) => user.name,
            None => "Unknown User".to_string(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_name_when_user_exists() {
        let mut mock = MockUserRepository::new();
        mock.expect_find_by_id()
            .with(mockall::predicate::eq(42))
            .times(1)
            .returning(|_| Some(User {
                id: 42,
                name: "Alice".into(),
                email: "alice@example.com".into(),
            }));

        let service = UserService::new(mock);
        assert_eq!(service.get_display_name(42), "Alice");
    }

    #[test]
    fn display_name_when_user_not_found() {
        let mut mock = MockUserRepository::new();
        mock.expect_find_by_id()
            .with(mockall::predicate::eq(999))
            .times(1)
            .returning(|_| None);

        let service = UserService::new(mock);
        assert_eq!(service.get_display_name(999), "Unknown User");
    }
}
```

### Verificación automática al drop

**Comportamiento crítico**: cuando un `MockFoo` se destruye (sale de scope), mockall automáticamente verifica que todas las expectativas se cumplieron:

```rust
#[test]
#[should_panic(expected = "MockUserRepository::find_by_id")]
fn panics_if_expectation_not_satisfied() {
    let mut mock = MockUserRepository::new();
    mock.expect_find_by_id()
        .times(1)           // esperamos exactamente 1 llamada
        .returning(|_| None);

    // No llamamos find_by_id en absoluto
    // Al salir del scope, mock se destruye → panic
}
```

```
Flujo de verificación:
                                        
  mock creado ──► expectativas ──► uso en test ──► mock dropped
       │         configuradas          │                │
       │              │                │                ▼
       │              │                │         ¿Se cumplieron
       │              │                │          todas las
       │              │                │         expectativas?
       │              │                │           │       │
       │              │                │          Sí      No
       │              │                │           │       │
       │              │                │          ok    PANIC!
       │              │                │                  │
       │              │                │          "Expectation
       │              │                │           unsatisfied"
```

---

## 5. Anatomía de un mock generado

Para entender cómo usar mockall, veamos qué genera. Dado:

```rust
#[automock]
pub trait Calculator {
    fn add(&self, a: i32, b: i32) -> i32;
    fn memory_store(&mut self, value: i32);
    fn memory_recall(&self) -> i32;
}
```

mockall genera (simplificado conceptualmente):

```rust
pub struct MockCalculator {
    // Campo interno para cada método
    add_expectations: Vec<Expectation_add>,
    memory_store_expectations: Vec<Expectation_memory_store>,
    memory_recall_expectations: Vec<Expectation_memory_recall>,
}

impl MockCalculator {
    /// Constructor: crea un mock sin expectativas
    pub fn new() -> Self { ... }

    /// Configura una expectativa para `add`
    pub fn expect_add(&mut self) -> &mut Expectation_add { ... }

    /// Configura una expectativa para `memory_store`
    pub fn expect_memory_store(&mut self) -> &mut Expectation_memory_store { ... }

    /// Configura una expectativa para `memory_recall`
    pub fn expect_memory_recall(&mut self) -> &mut Expectation_memory_recall { ... }

    /// Borra todas las expectativas de todos los métodos
    pub fn checkpoint(&mut self) { ... }
}

impl Calculator for MockCalculator {
    fn add(&self, a: i32, b: i32) -> i32 {
        // Busca la primera expectativa que matchea (a, b)
        // Si la encuentra, llama su `returning`
        // Si no, panic: "unexpected call"
        ...
    }

    fn memory_store(&mut self, value: i32) {
        ...
    }

    fn memory_recall(&self) -> i32 {
        ...
    }
}

// Verificación en Drop
impl Drop for MockCalculator {
    fn drop(&mut self) {
        // Verifica que todas las expectativas se satisficieron
        // Si alguna expectation.times no se cumplió → panic
    }
}
```

### Múltiples expectativas por método

```rust
let mut mock = MockCalculator::new();

// Primera expectativa: si a=1, b=2 → retorna 3
mock.expect_add()
    .with(predicate::eq(1), predicate::eq(2))
    .returning(|_, _| 3);

// Segunda expectativa: si a=10, b=20 → retorna 30
mock.expect_add()
    .with(predicate::eq(10), predicate::eq(20))
    .returning(|_, _| 30);

// mockall evalúa las expectativas en orden LIFO (la última añadida primero)
// Esto permite "override" de expectativas generales con específicas
```

```
Orden de evaluación de expectativas:

  expect_add #1: with(eq(1), eq(2))    ← añadida primero
  expect_add #2: with(eq(10), eq(20))  ← añadida después

  Llamada: add(10, 20)

  Evalúa #2 primero → ¿matchea? SÍ → retorna 30
  
  Llamada: add(1, 2)
  
  Evalúa #2 primero → ¿matchea? NO
  Evalúa #1         → ¿matchea? SÍ → retorna 3
  
  Llamada: add(5, 5)
  
  Evalúa #2 → NO
  Evalúa #1 → NO
  → PANIC: "No matching expectation found"
```

---

## 6. expect_*: configurar expectativas

Cada método `foo` del trait genera un `expect_foo` en el mock. El `expect_foo` retorna una referencia mutable a un `Expectation` que se configura con un patrón builder:

```rust
mock.expect_foo()                     // inicia configuración
    .with(...)                        // qué argumentos aceptar
    .withf(...)                       // predicado custom sobre args
    .times(...)                       // cuántas veces debe llamarse
    .returning(...)                   // qué retornar
    .return_once(...)                 // retornar una vez (para tipos no-Clone)
    .return_const(...)                // retornar un valor constante clonable
    .in_sequence(...)                 // orden relativo a otros expects
    .never()                          // sinónimo de times(0)
```

### Flujo de una llamada

```
Código bajo test llama mock.foo(args)
            │
            ▼
    ┌───────────────────┐
    │ Buscar expectativa │
    │ que matchee args   │
    │ (LIFO order)       │
    └───────┬───────────┘
            │
     ┌──────┴──────┐
     │  ¿Encontró? │
     └──┬──────┬───┘
        │      │
       Sí      No ──► PANIC "No matching expectation"
        │
        ▼
    ┌───────────────┐
    │ ¿times ok?    │
    │ (no excedido) │
    └──┬────────┬───┘
       │        │
      Sí        No ──► PANIC "Called more than N times"
       │
       ▼
    ┌───────────────┐
    │ ¿sequence ok? │
    │ (turno correcto)│
    └──┬────────┬───┘
       │        │
      Sí        No ──► PANIC "Method called out of sequence"
       │
       ▼
    ┌───────────────┐
    │ Ejecutar      │
    │ returning()   │
    │ Retornar valor│
    └───────────────┘
```

### Expectativa mínima

```rust
// Lo mínimo que necesitas: returning
mock.expect_foo()
    .returning(|x| x + 1);

// Sin `with` → acepta cualquier argumento
// Sin `times` → acepta cualquier número de llamadas ≥ 1
// Sin `in_sequence` → sin restricción de orden
```

### Expectativa sin returning

Si no configuras `returning` y el método se llama → panic:

```rust
let mut mock = MockFoo::new();
mock.expect_bar(); // sin returning

// mock.bar(42); → panic: "No matching expectation found"
// mockall requiere que la expectativa tenga un returning configurado
```

Excepción: si el tipo de retorno es `()` (void), mockall genera automáticamente un `returning(|| ())`.

```rust
#[automock]
pub trait Logger {
    fn log(&self, message: &str);  // retorna ()
}

let mut mock = MockLogger::new();
mock.expect_log()
    .times(1);  // sin returning — ok porque retorna ()
```

---

## 7. returning: definir valores de retorno

### returning: closure que toma los argumentos

```rust
#[automock]
pub trait MathService {
    fn multiply(&self, a: f64, b: f64) -> f64;
    fn divide(&self, a: f64, b: f64) -> Result<f64, String>;
}

#[test]
fn test_returning() {
    let mut mock = MockMathService::new();

    // returning recibe los argumentos del método
    mock.expect_multiply()
        .returning(|a, b| a * b);  // comportamiento "real"

    mock.expect_divide()
        .returning(|a, b| {
            if b == 0.0 {
                Err("division by zero".into())
            } else {
                Ok(a / b)
            }
        });

    assert_eq!(mock.multiply(3.0, 4.0), 12.0);
    assert_eq!(mock.divide(10.0, 2.0), Ok(5.0));
    assert_eq!(mock.divide(1.0, 0.0), Err("division by zero".into()));
}
```

### return_const: valor fijo clonable

```rust
#[test]
fn test_return_const() {
    let mut mock = MockMathService::new();

    // Retorna siempre el mismo valor (el tipo debe ser Clone)
    mock.expect_multiply()
        .return_const(42.0);

    assert_eq!(mock.multiply(1.0, 1.0), 42.0);
    assert_eq!(mock.multiply(999.0, 999.0), 42.0); // siempre 42
}
```

### return_once: para tipos no-Clone

```rust
#[automock]
pub trait FileProducer {
    fn open(&self, path: &str) -> std::fs::File;
}

#[test]
fn test_return_once() {
    let mut mock = MockFileProducer::new();

    // File no es Clone, así que usamos return_once
    let temp = tempfile::NamedTempFile::new().unwrap();
    let file = temp.into_file();

    mock.expect_open()
        .return_once(move |_| file);  // consume el File

    let _ = mock.open("/tmp/test.txt"); // ok
    // mock.open("/tmp/other.txt");     // PANIC: ya se consumió
}
```

### Comparación de las tres variantes

```
┌─────────────────┬──────────────────┬──────────────────┬──────────────────┐
│                 │   returning      │   return_const   │   return_once    │
├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Tipo closure    │ Fn(Args)->Ret    │ (ninguno)        │ FnOnce(Args)->Ret│
│ Clone requerido │ Ret: no          │ Ret: Clone       │ Ret: no          │
│ Llamadas        │ Múltiples        │ Múltiples        │ Exactamente 1    │
│ Args accesibles │ Sí               │ No               │ Sí               │
│ Uso principal   │ Lógica dinámica  │ Valor fijo       │ Mover ownership  │
├─────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Ejemplo         │ .returning(      │ .return_const(   │ .return_once(    │
│                 │   |x| x + 1)    │   42)            │   move |_| file) │
└─────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

### returning con estado (closure captura variables)

```rust
use std::sync::{Arc, Mutex};

#[test]
fn test_returning_with_state() {
    let mut mock = MockUserRepository::new();

    // Simular un auto-increment de IDs
    let counter = Arc::new(Mutex::new(0u64));
    let counter_clone = counter.clone();

    mock.expect_save()
        .returning(move |_user| {
            let mut c = counter_clone.lock().unwrap();
            *c += 1;
            Ok(*c)
        });

    let user = User {
        id: 0,
        name: "Alice".into(),
        email: "alice@test.com".into(),
    };

    assert_eq!(mock.save(&user), Ok(1));
    assert_eq!(mock.save(&user), Ok(2));
    assert_eq!(mock.save(&user), Ok(3));
}
```

### Cadena de return_once para respuestas secuenciales

```rust
#[automock]
pub trait TokenGenerator {
    fn generate(&self) -> String;
}

#[test]
fn sequential_returns() {
    let mut mock = MockTokenGenerator::new();

    // La tercera llamada y siguientes usan returning (LIFO)
    mock.expect_generate()
        .returning(|| "default-token".to_string());

    // La segunda llamada usa este return_once
    mock.expect_generate()
        .times(1)
        .return_once(|| "second-token".to_string());

    // La primera llamada usa este return_once (LIFO: último añadido, primero evaluado)
    mock.expect_generate()
        .times(1)
        .return_once(|| "first-token".to_string());

    assert_eq!(mock.generate(), "first-token");
    assert_eq!(mock.generate(), "second-token");
    assert_eq!(mock.generate(), "default-token");
    assert_eq!(mock.generate(), "default-token");
}
```

```
Evaluación LIFO para generate():

  Expectativas (orden de adición):
  [0] returning("default-token")     ← sin times, fallback
  [1] return_once("second-token")    ← times(1)
  [2] return_once("first-token")     ← times(1), LIFO top

  Llamada 1: evalúa [2] → activa, times ok → "first-token" (consumido)
  Llamada 2: evalúa [2] → consumido, skip
             evalúa [1] → activa, times ok → "second-token" (consumido)
  Llamada 3: evalúa [2] → consumido
             evalúa [1] → consumido
             evalúa [0] → activa → "default-token"
```

---

## 8. times: controlar cuántas veces se invoca

`times` es la herramienta de **verificación** en mockall. Te permite especificar exactamente cuántas veces un método debe ser llamado.

### Valores exactos

```rust
mock.expect_foo().times(0);     // NUNCA debe llamarse
mock.expect_foo().times(1);     // exactamente 1 vez
mock.expect_foo().times(5);     // exactamente 5 veces
```

### Rangos

```rust
mock.expect_foo().times(1..);     // al menos 1 vez (1, 2, 3, ...)
mock.expect_foo().times(..3);     // como máximo 2 veces (0, 1, 2)
mock.expect_foo().times(2..5);    // entre 2 y 4 veces
mock.expect_foo().times(..=3);    // como máximo 3 veces (0, 1, 2, 3)
mock.expect_foo().times(2..=5);   // entre 2 y 5 veces
```

### Aliases semánticos

```rust
mock.expect_foo().never();        // equivale a times(0)
// No hay .once() ni .at_least() — usa times(1) y times(1..)
```

### Comportamiento por defecto (sin times)

```rust
mock.expect_foo()
    .returning(|| 42);
// Sin times → equivale a times(1..)
// Debe llamarse AL MENOS una vez
// Si no se llama → panic en drop
```

### Tabla de verificación

```
┌─────────────────────┬──────────────┬─────────────────────────────────┐
│ Configuración       │ Llamadas = 0 │ Llamadas = 1 │ Llamadas = 3    │
├─────────────────────┼──────────────┼──────────────┼─────────────────┤
│ times(1)            │ PANIC (drop) │ OK           │ PANIC (call 2)  │
│ times(3)            │ PANIC (drop) │ PANIC (drop) │ OK              │
│ times(0) / never()  │ OK           │ PANIC (call) │ PANIC (call 1)  │
│ times(1..)          │ PANIC (drop) │ OK           │ OK              │
│ times(..3)          │ OK           │ OK           │ PANIC (call 3)  │
│ times(1..=3)        │ PANIC (drop) │ OK           │ OK              │
│ (sin times)         │ PANIC (drop) │ OK           │ OK              │
└─────────────────────┴──────────────┴──────────────┴─────────────────┘

PANIC (drop)  = el test falla cuando el mock se destruye
PANIC (call)  = el test falla en el momento de la llamada
PANIC (call N)= falla en la llamada N-ésima
```

### Ejemplo práctico: verificar caching

```rust
#[automock]
pub trait ExpensiveComputation {
    fn compute(&self, input: &str) -> u64;
}

pub struct CachedComputer<C: ExpensiveComputation> {
    computer: C,
    cache: std::collections::HashMap<String, u64>,
}

impl<C: ExpensiveComputation> CachedComputer<C> {
    pub fn new(computer: C) -> Self {
        CachedComputer {
            computer,
            cache: std::collections::HashMap::new(),
        }
    }

    pub fn get(&mut self, input: &str) -> u64 {
        if let Some(&cached) = self.cache.get(input) {
            return cached;
        }
        let result = self.computer.compute(input);
        self.cache.insert(input.to_string(), result);
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn caching_avoids_recomputation() {
        let mut mock = MockExpensiveComputation::new();

        // La computación solo debe ocurrir UNA vez
        mock.expect_compute()
            .with(mockall::predicate::eq("hello"))
            .times(1)              // ← EXACTAMENTE 1 vez
            .returning(|_| 42);

        let mut cached = CachedComputer::new(mock);

        // Primera llamada: computa
        assert_eq!(cached.get("hello"), 42);

        // Segunda llamada: usa caché, NO llama a compute
        assert_eq!(cached.get("hello"), 42);

        // Tercera llamada: sigue en caché
        assert_eq!(cached.get("hello"), 42);

        // Si compute se llamara más de 1 vez, el test falla
    }
}
```

### never(): verificar que algo NO ocurre

```rust
#[automock]
pub trait AuditLog {
    fn record(&self, event: &str);
}

#[automock]
pub trait Validator {
    fn validate(&self, data: &str) -> Result<(), String>;
}

pub struct Processor<V: Validator, A: AuditLog> {
    validator: V,
    audit: A,
}

impl<V: Validator, A: AuditLog> Processor<V, A> {
    pub fn new(validator: V, audit: A) -> Self {
        Processor { validator, audit }
    }

    pub fn process(&self, data: &str) -> Result<(), String> {
        self.validator.validate(data)?;
        // Solo audita si la validación pasa
        self.audit.record(&format!("processed: {}", data));
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn failed_validation_does_not_audit() {
        let mut validator = MockValidator::new();
        let mut audit = MockAuditLog::new();

        validator.expect_validate()
            .returning(|_| Err("invalid".into()));

        // El audit NUNCA debe llamarse
        audit.expect_record()
            .never();   // ← times(0)

        let proc = Processor::new(validator, audit);
        let _ = proc.process("bad data");
    }
}
```

---

## 9. withf: predicados sobre argumentos

`withf` permite verificar que los argumentos pasados al método cumplen una condición:

```rust
#[automock]
pub trait EmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String>;
}

#[test]
fn send_welcome_email() {
    let mut mock = MockEmailSender::new();

    mock.expect_send()
        .withf(|to, subject, body| {
            to == "alice@example.com"
                && subject.contains("Welcome")
                && body.len() > 0
        })
        .times(1)
        .returning(|_, _, _| Ok(()));

    // En el test, el código bajo test llama send()
    // Si los argumentos no satisfacen el predicado → "No matching expectation"
    mock.send("alice@example.com", "Welcome to our platform!", "Hello Alice...").unwrap();
}
```

### withf vs with

```rust
use mockall::predicate;

// with: usa matchers predefinidos
mock.expect_send()
    .with(
        predicate::eq("alice@example.com"),
        predicate::str::starts_with("Welcome"),
        predicate::always(),
    )
    .returning(|_, _, _| Ok(()));

// withf: usa un closure libre
mock.expect_send()
    .withf(|to, subject, _body| {
        to == "alice@example.com" && subject.starts_with("Welcome")
    })
    .returning(|_, _, _| Ok(()));

// Ambos son equivalentes en este caso
```

### withf con lógica compleja

```rust
#[automock]
pub trait OrderStore {
    fn save_order(&self, order: &Order) -> Result<u64, String>;
}

#[derive(Debug, Clone)]
pub struct Order {
    pub user_id: u64,
    pub items: Vec<OrderItem>,
    pub total_cents: u64,
}

#[derive(Debug, Clone)]
pub struct OrderItem {
    pub product_id: u64,
    pub quantity: u32,
    pub price_cents: u64,
}

#[test]
fn test_order_validation_in_mock() {
    let mut mock = MockOrderStore::new();

    mock.expect_save_order()
        .withf(|order| {
            // Verificar invariantes del pedido
            !order.items.is_empty()
                && order.total_cents > 0
                && order.total_cents == order.items.iter()
                    .map(|i| i.price_cents * i.quantity as u64)
                    .sum::<u64>()
                && order.items.iter().all(|i| i.quantity > 0)
        })
        .returning(|_| Ok(1));

    let order = Order {
        user_id: 1,
        items: vec![
            OrderItem { product_id: 10, quantity: 2, price_cents: 500 },
            OrderItem { product_id: 20, quantity: 1, price_cents: 1000 },
        ],
        total_cents: 2000,
    };

    assert_eq!(mock.save_order(&order), Ok(1));
}
```

### withf y captura de argumentos (spy-like)

```rust
use std::sync::{Arc, Mutex};

#[test]
fn capture_arguments_with_withf() {
    let mut mock = MockEmailSender::new();
    let captured_emails = Arc::new(Mutex::new(Vec::<String>::new()));
    let captured = captured_emails.clone();

    mock.expect_send()
        .withf(move |to, _, _| {
            captured.lock().unwrap().push(to.to_string());
            true  // siempre acepta (no filtra)
        })
        .returning(|_, _, _| Ok(()));

    mock.send("alice@test.com", "Hi", "Body").unwrap();
    mock.send("bob@test.com", "Hi", "Body").unwrap();

    let emails = captured_emails.lock().unwrap();
    assert_eq!(emails.len(), 2);
    assert_eq!(emails[0], "alice@test.com");
    assert_eq!(emails[1], "bob@test.com");
}
```

---

## 10. with: matchers predefinidos

mockall provee el módulo `mockall::predicate` con matchers composables:

### Matchers básicos

```rust
use mockall::predicate::*;

// Igualdad exacta
mock.expect_foo().with(eq(42));

// Siempre matchea
mock.expect_foo().with(always());

// Nunca matchea (útil para "default" expectation que no debe activarse)
mock.expect_foo().with(never());
```

### Matchers de string

```rust
use mockall::predicate::*;

// Contiene substring
mock.expect_log().with(str::contains("ERROR"));

// Empieza con
mock.expect_log().with(str::starts_with("[INFO]"));

// Termina con
mock.expect_log().with(str::ends_with(".rs"));

// Regex
mock.expect_log().with(str::is_match(r"^\[\d{4}-\d{2}-\d{2}\]").unwrap());
```

### Matchers de comparación

```rust
use mockall::predicate::*;

// Mayor que, menor que, etc.
mock.expect_set_age().with(gt(0));      // > 0
mock.expect_set_age().with(ge(18));     // >= 18
mock.expect_set_age().with(lt(150));    // < 150
mock.expect_set_age().with(le(120));    // <= 120
mock.expect_set_age().with(ne(0));      // != 0
```

### Matchers composables (booleanos)

```rust
use mockall::predicate::*;

// AND: ambos deben cumplirse
mock.expect_set_age()
    .with(ge(18).and(le(120)));

// OR: al menos uno
mock.expect_set_name()
    .with(str::starts_with("A").or(str::starts_with("B")));

// NOT: negación
mock.expect_set_status()
    .with(eq("active").not()); // cualquier cosa excepto "active"
```

### Matcher con función

```rust
use mockall::predicate::*;

// function: convierte un Fn(&T) -> bool en un Predicate
mock.expect_save_order()
    .with(function(|order: &Order| {
        order.total_cents > 0 && !order.items.is_empty()
    }));
```

### Matchers con in_iter

```rust
use mockall::predicate::*;

// El valor debe estar en el conjunto
mock.expect_set_status()
    .with(in_iter(vec!["active", "inactive", "pending"]));
```

### Tabla resumen de predicados

```
┌───────────────────────────────────┬──────────────────────────────────────┐
│ Predicado                         │ Qué verifica                         │
├───────────────────────────────────┼──────────────────────────────────────┤
│ eq(val)                           │ arg == val                           │
│ ne(val)                           │ arg != val                           │
│ gt(val) / ge(val)                 │ arg > val / arg >= val               │
│ lt(val) / le(val)                 │ arg < val / arg <= val               │
│ always()                          │ siempre true                         │
│ never()                           │ siempre false                        │
│ in_iter(collection)               │ arg está en la colección             │
│ function(|arg| -> bool)           │ closure personalizado                │
│ str::contains(s)                  │ arg contiene s                       │
│ str::starts_with(s)               │ arg empieza con s                    │
│ str::ends_with(s)                 │ arg termina con s                    │
│ str::is_match(regex)              │ arg matchea regex                    │
│ p1.and(p2)                        │ p1 Y p2                              │
│ p1.or(p2)                         │ p1 O p2                              │
│ p.not()                           │ NO p                                 │
└───────────────────────────────────┴──────────────────────────────────────┘
```

### Ejemplo: matchers compuestos en un test real

```rust
use mockall::predicate::*;

#[automock]
pub trait NotificationService {
    fn send_notification(
        &self,
        user_id: u64,
        channel: &str,
        message: &str,
        priority: u8,
    ) -> Result<(), String>;
}

#[test]
fn test_high_priority_notification() {
    let mut mock = MockNotificationService::new();

    mock.expect_send_notification()
        .with(
            gt(0),                                    // user_id > 0
            in_iter(vec!["email", "sms", "push"]),    // canal válido
            str::contains("URGENT"),                   // mensaje contiene URGENT
            ge(8),                                     // prioridad >= 8
        )
        .times(1)
        .returning(|_, _, _, _| Ok(()));

    mock.send_notification(42, "sms", "[URGENT] Server down!", 9).unwrap();
}
```

---

## 11. Sequences: orden de invocación

Las `Sequence` garantizan que las llamadas ocurren en un orden específico:

```rust
use mockall::Sequence;

#[automock]
pub trait TransactionManager {
    fn begin(&mut self) -> Result<(), String>;
    fn execute(&mut self, sql: &str) -> Result<(), String>;
    fn commit(&mut self) -> Result<(), String>;
    fn rollback(&mut self) -> Result<(), String>;
}

#[test]
fn transaction_order() {
    let mut mock = MockTransactionManager::new();
    let mut seq = Sequence::new();

    // Las llamadas DEBEN ocurrir en este orden exacto
    mock.expect_begin()
        .times(1)
        .in_sequence(&mut seq)              // paso 1
        .returning(|| Ok(()));

    mock.expect_execute()
        .times(1)
        .in_sequence(&mut seq)              // paso 2
        .returning(|_| Ok(()));

    mock.expect_commit()
        .times(1)
        .in_sequence(&mut seq)              // paso 3
        .returning(|| Ok(()));

    // Si el código llama commit() antes de begin() → PANIC
    mock.begin().unwrap();
    mock.execute("INSERT INTO users ...").unwrap();
    mock.commit().unwrap();
}
```

### Anatomía de Sequence

```
Sequence::new()
    │
    ├── seq_position: AtomicUsize (empieza en 0)
    │
    └── Al registrar expect.in_sequence(&mut seq):
        └── le asigna un número de orden: 0, 1, 2, ...

  Verificación en cada llamada:
  ┌─────────────────────────────────────────────┐
  │ Expectation con seq_order = N               │
  │                                             │
  │ ¿seq.position == N?                         │
  │    SÍ → ejecutar, avanzar position a N+1    │
  │    NO → PANIC "called out of sequence"      │
  └─────────────────────────────────────────────┘
```

### Múltiples sequences independientes

```rust
use mockall::Sequence;

#[automock]
pub trait Logger { fn log(&self, msg: &str); }

#[automock]
pub trait Database { fn query(&self, sql: &str) -> Vec<String>; }

#[test]
fn parallel_sequences() {
    let mut mock_log = MockLogger::new();
    let mut mock_db = MockDatabase::new();

    // Los logs deben ocurrir en orden entre sí
    let mut log_seq = Sequence::new();
    mock_log.expect_log()
        .with(mockall::predicate::eq("starting"))
        .times(1)
        .in_sequence(&mut log_seq)
        .returning(|_| ());
    mock_log.expect_log()
        .with(mockall::predicate::eq("done"))
        .times(1)
        .in_sequence(&mut log_seq)
        .returning(|_| ());

    // Las queries deben ocurrir en orden entre sí
    let mut db_seq = Sequence::new();
    mock_db.expect_query()
        .with(mockall::predicate::str::starts_with("SELECT"))
        .times(1)
        .in_sequence(&mut db_seq)
        .returning(|_| vec!["row1".into()]);
    mock_db.expect_query()
        .with(mockall::predicate::str::starts_with("UPDATE"))
        .times(1)
        .in_sequence(&mut db_seq)
        .returning(|_| vec![]);

    // Pero log_seq y db_seq son independientes entre sí
    // "starting" puede ocurrir antes o después del SELECT
    mock_log.log("starting");
    mock_db.query("SELECT * FROM users");
    mock_db.query("UPDATE users SET active=true");
    mock_log.log("done");
}
```

```
Dos sequences independientes:

  log_seq:   "starting" ──→ "done"
  db_seq:    SELECT ──→ UPDATE

  Entrelazados válidos:
  ✓ starting, SELECT, UPDATE, done
  ✓ SELECT, starting, UPDATE, done
  ✓ SELECT, starting, done, UPDATE
  ✗ done, starting, ...     (viola log_seq)
  ✗ UPDATE, SELECT, ...     (viola db_seq)
```

### Sequence cross-object

Una sola `Sequence` puede abarcar múltiples mocks:

```rust
#[test]
fn cross_object_ordering() {
    let mut auth = MockAuthService::new();
    let mut db = MockDatabase::new();
    let mut seq = Sequence::new();

    // Primero auth, luego db
    auth.expect_authenticate()
        .times(1)
        .in_sequence(&mut seq)
        .returning(|_, _| Ok(true));

    db.expect_query()
        .times(1)
        .in_sequence(&mut seq)
        .returning(|_| vec![]);

    // El código DEBE autenticar antes de hacer queries
    auth.authenticate("user", "pass").unwrap();
    db.query("SELECT ...");
}
```

---

## 12. Checkpoints: verificación parcial

`checkpoint()` verifica todas las expectativas actuales y las borra, permitiendo reconfigurar el mock para una nueva fase del test:

```rust
#[automock]
pub trait Counter {
    fn increment(&mut self);
    fn get(&self) -> u64;
}

#[test]
fn test_with_checkpoint() {
    let mut mock = MockCounter::new();

    // === Fase 1: inicialización ===
    mock.expect_get()
        .times(1)
        .return_const(0u64);

    assert_eq!(mock.get(), 0);

    // checkpoint: verifica fase 1 y limpia expectativas
    mock.checkpoint();

    // === Fase 2: después de incrementos ===
    mock.expect_get()
        .times(1)
        .return_const(5u64);

    assert_eq!(mock.get(), 5);

    // Al drop: verifica fase 2
}
```

### Flujo de checkpoint

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│ Config   │     │  Usa    │     │ Check-  │     │ Config   │
│ Fase 1   │────►│ Fase 1  │────►│ point   │────►│ Fase 2   │──► ...
│          │     │         │     │         │     │          │
└─────────┘     └─────────┘     └────┬────┘     └─────────┘
                                     │
                              ┌──────┴──────┐
                              │  Verifica:  │
                              │  ¿Todas las │
                              │  expectati- │
                              │  vas de     │
                              │  fase 1 ok? │
                              │             │
                              │  SÍ → limpia│
                              │  NO → PANIC │
                              └─────────────┘
```

### Caso de uso: simular cambios de estado

```rust
#[automock]
pub trait ConfigProvider {
    fn get(&self, key: &str) -> Option<String>;
}

pub struct AppConfig<C: ConfigProvider> {
    provider: C,
}

impl<C: ConfigProvider> AppConfig<C> {
    pub fn new(provider: C) -> Self {
        AppConfig { provider }
    }

    pub fn is_feature_enabled(&self, feature: &str) -> bool {
        self.provider
            .get(&format!("feature.{}.enabled", feature))
            .map(|v| v == "true")
            .unwrap_or(false)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use mockall::predicate::*;

    #[test]
    fn feature_flag_changes() {
        let mut mock = MockConfigProvider::new();

        // Fase 1: feature desactivada
        mock.expect_get()
            .with(eq("feature.dark_mode.enabled"))
            .return_const(Some("false".to_string()));

        let config = AppConfig::new(mock);
        assert!(!config.is_feature_enabled("dark_mode"));

        // Recuperamos el mock para reconfigurar
        // (en la práctica, tendrías que usar interior mutability
        // o crear un nuevo AppConfig — ver nota abajo)
    }
}
```

**Nota**: `checkpoint()` requiere `&mut self`, así que necesitas acceso mutable al mock. Si ya está dentro de una struct, necesitas diseñar el test para tener acceso directo o usar `Rc<RefCell<MockFoo>>`.

---

## 13. returning vs return_once vs return_const

Esta sección profundiza en las diferencias sutiles que causan errores de compilación:

### El problema fundamental: ownership

```rust
#[automock]
pub trait Producer {
    fn produce(&self) -> Vec<String>;
}
```

```rust
// ✓ returning: el closure debe ser Fn (no FnOnce)
// Significa que debe poder ejecutarse múltiples veces
mock.expect_produce()
    .returning(|| vec!["a".into(), "b".into()]);
// Cada llamada crea un nuevo Vec — ok

// ✗ ERROR: mover un valor dentro de returning
let data = vec!["a".into(), "b".into()];
mock.expect_produce()
    .returning(move || data);  // ERROR: data se movería cada vez
//                                 pero returning es Fn, no FnOnce

// ✓ Solución 1: clonar
let data = vec!["a".into(), "b".into()];
mock.expect_produce()
    .returning(move || data.clone());

// ✓ Solución 2: return_const (si el tipo es Clone)
mock.expect_produce()
    .return_const(vec!["a".into(), "b".into()]);

// ✓ Solución 3: return_once (si solo necesitas una llamada)
let data = vec!["a".into(), "b".into()];
mock.expect_produce()
    .return_once(move || data);
```

### Tipos no-Clone

```rust
#[automock]
pub trait ResourceFactory {
    fn create(&self) -> TcpStream;  // TcpStream no es Clone
}

// ✗ return_const: TcpStream no es Clone
// mock.expect_create().return_const(stream); // ERROR

// ✗ returning con move: FnOnce no es Fn
// mock.expect_create().returning(move || stream); // ERROR

// ✓ return_once: se ejecuta una sola vez
mock.expect_create()
    .times(1)
    .return_once(move || stream);

// ¿Y si necesitas múltiples llamadas con tipos no-Clone?
// Usa returning con una fábrica:
mock.expect_create()
    .returning(|| {
        TcpStream::connect("127.0.0.1:8080").unwrap()
    });
```

### Árbol de decisión

```
¿Qué returning usar?
        │
        ▼
┌───────────────────┐
│ ¿El valor de      │
│ retorno es siempre│
│ el mismo?         │
└───┬──────────┬────┘
    │          │
   SÍ          NO
    │          │
    ▼          ▼
┌────────┐  ┌──────────────┐
│¿Clone? │  │ ¿Depende de  │
└─┬───┬──┘  │ los args?    │
  │   │     └──┬───────┬───┘
 SÍ   NO      │       │
  │   │       SÍ       NO
  ▼   ▼       │       │
return return  ▼       ▼
_const _once  returning returning
              (con args) (sin args)
              │
              ▼
         ¿El closure
          mueve algo
          no-Clone?
           │      │
          SÍ      NO
           │      │
           ▼      ▼
        return  returning
        _once
```

---

## 14. Mocking de traits genéricos

### Trait con parámetros de tipo

```rust
#[automock]
pub trait Repository<T: Clone + 'static> {
    fn find(&self, id: u64) -> Option<T>;
    fn save(&mut self, item: &T) -> Result<u64, String>;
}
```

mockall genera `MockRepository<T>`. Debes especificar el tipo concreto al crear el mock:

```rust
#[derive(Debug, Clone, PartialEq)]
pub struct User { pub id: u64, pub name: String }

#[test]
fn test_generic_repo() {
    // Especificar el tipo concreto
    let mut mock = MockRepository::<User>::new();

    mock.expect_find()
        .with(mockall::predicate::eq(1))
        .returning(|_| Some(User { id: 1, name: "Alice".into() }));

    assert_eq!(
        mock.find(1),
        Some(User { id: 1, name: "Alice".into() })
    );
}
```

### Trait con métodos genéricos

```rust
// ⚠️ mockall NO puede generar automock para métodos genéricos
// porque no puede generar vtables para funciones con parámetros de tipo

// ESTO NO FUNCIONA:
// #[automock]
// pub trait Serializer {
//     fn serialize<T: serde::Serialize>(&self, value: &T) -> String;
// }

// Solución: usar tipos concretos o trait objects
#[automock]
pub trait Serializer {
    fn serialize_json(&self, value: &dyn std::fmt::Debug) -> String;
}

// O mover el genérico al trait:
#[automock]
pub trait TypedSerializer<T: Clone + 'static> {
    fn serialize(&self, value: &T) -> String;
}
```

### Trait con bounds complejos

```rust
use std::fmt::Debug;
use std::hash::Hash;

#[automock]
pub trait Cache<K, V>
where
    K: Eq + Hash + Clone + Debug + 'static,
    V: Clone + Debug + 'static,
{
    fn get(&self, key: &K) -> Option<V>;
    fn set(&mut self, key: K, value: V);
    fn invalidate(&mut self, key: &K) -> bool;
}

#[test]
fn test_string_cache() {
    let mut mock = MockCache::<String, Vec<u8>>::new();

    mock.expect_get()
        .with(mockall::predicate::eq("key1".to_string()))
        .return_const(Some(vec![1, 2, 3]));

    mock.expect_set()
        .withf(|k, v| k == "key2" && !v.is_empty())
        .times(1)
        .returning(|_, _| ());

    assert_eq!(mock.get(&"key1".to_string()), Some(vec![1, 2, 3]));
    mock.set("key2".to_string(), vec![4, 5]);
}
```

---

## 15. Mocking de traits con tipos asociados

```rust
#[automock(type Item = String; type Error = std::io::Error;)]
pub trait Stream {
    type Item;
    type Error;

    fn next(&mut self) -> Result<Option<Self::Item>, Self::Error>;
    fn reset(&mut self) -> Result<(), Self::Error>;
}
```

La clave es el argumento de `#[automock]` que especifica los tipos concretos:

```rust
#[test]
fn test_stream_mock() {
    let mut mock = MockStream::new();

    mock.expect_next()
        .times(3)
        .returning(|| Ok(Some("line".to_string())));

    mock.expect_next()
        .times(1)
        .returning(|| Ok(None)); // EOF

    for _ in 0..3 {
        assert_eq!(mock.next().unwrap(), Some("line".to_string()));
    }
    assert_eq!(mock.next().unwrap(), None);
}
```

### Iterator trait (tipos asociados estándar)

```rust
// Para traits de la stdlib, usa mock! en lugar de #[automock]
mock! {
    pub StringIterator {}

    impl Iterator for StringIterator {
        type Item = String;
        fn next(&mut self) -> Option<String>;
    }
}

#[test]
fn test_iterator_mock() {
    let mut mock = MockStringIterator::new();

    mock.expect_next()
        .times(1)
        .return_once(|| Some("first".to_string()));
    mock.expect_next()
        .times(1)
        .return_once(|| Some("second".to_string()));
    mock.expect_next()
        .times(1)
        .return_once(|| None);

    let results: Vec<String> = mock.collect();
    assert_eq!(results, vec!["first", "second"]);
}
```

---

## 16. Mocking de traits async

### Rust 1.75+: async fn in traits nativo

```rust
#[automock]
pub trait AsyncUserRepository {
    async fn find_by_id(&self, id: u64) -> Option<User>;
    async fn save(&self, user: &User) -> Result<u64, String>;
    async fn delete(&self, id: u64) -> Result<(), String>;
}

#[tokio::test]
async fn test_async_mock() {
    let mut mock = MockAsyncUserRepository::new();

    mock.expect_find_by_id()
        .with(mockall::predicate::eq(42))
        .times(1)
        .returning(|_| Some(User {
            id: 42,
            name: "Alice".into(),
            email: "alice@test.com".into(),
        }));

    // Usar .await como con la implementación real
    let user = mock.find_by_id(42).await;
    assert_eq!(user.unwrap().name, "Alice");
}
```

### Con async-trait crate (pre-1.75 o con dyn dispatch)

```rust
use async_trait::async_trait;

#[automock]
#[async_trait]
pub trait AsyncDatabase {
    async fn query(&self, sql: &str) -> Result<Vec<Row>, DbError>;
    async fn execute(&self, sql: &str) -> Result<u64, DbError>;
}

// El orden importa: #[automock] ANTES de #[async_trait]
```

```rust
#[tokio::test]
async fn test_with_async_trait() {
    let mut mock = MockAsyncDatabase::new();

    mock.expect_query()
        .with(mockall::predicate::str::starts_with("SELECT"))
        .returning(|_| Ok(vec![Row::new()]));

    mock.expect_execute()
        .with(mockall::predicate::str::starts_with("INSERT"))
        .returning(|_| Ok(1));

    let rows = mock.query("SELECT * FROM users").await.unwrap();
    assert_eq!(rows.len(), 1);

    let affected = mock.execute("INSERT INTO users ...").await.unwrap();
    assert_eq!(affected, 1);
}
```

### Diferencias async

```
┌──────────────────────────┬────────────────────────┬──────────────────────┐
│                          │ async fn nativo (1.75+)│ async-trait crate    │
├──────────────────────────┼────────────────────────┼──────────────────────┤
│ Orden de atributos       │ #[automock] solo       │ #[automock] primero  │
│                          │                        │ #[async_trait] después│
│ Tipo retorno interno     │ impl Future            │ Pin<Box<dyn Future>> │
│ Requiere dep extra       │ No                     │ Sí (async-trait)     │
│ dyn dispatch             │ No (por defecto)       │ Sí                   │
│ returning closure        │ Fn normal, mockall     │ Fn normal, mockall   │
│                          │ envuelve en async      │ envuelve en async    │
│ Soporte mockall          │ 0.12+                  │ Desde 0.9            │
└──────────────────────────┴────────────────────────┴──────────────────────┘
```

---

## 17. Mocking de structs concretos (#[automock] en impl)

A veces necesitas mockear un struct que no usa un trait. mockall puede generar mocks para bloques `impl` directos:

```rust
pub struct RealClock;

#[cfg_attr(test, automock)]
impl RealClock {
    pub fn now(&self) -> u64 {
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs()
    }

    pub fn elapsed_since(&self, timestamp: u64) -> u64 {
        self.now() - timestamp
    }
}

// Genera MockRealClock
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mock_struct() {
        let mut mock = MockRealClock::new();

        mock.expect_now()
            .return_const(1_700_000_000u64);

        mock.expect_elapsed_since()
            .with(mockall::predicate::eq(1_699_999_900))
            .return_const(100u64);

        assert_eq!(mock.now(), 1_700_000_000);
        assert_eq!(mock.elapsed_since(1_699_999_900), 100);
    }
}
```

**Limitación importante**: el código que usa `RealClock` directamente no puede recibir `MockRealClock` sin genéricos o trait objects. Este approach solo funciona si el test crea y usa el mock directamente, no a través de inyección.

La recomendación sigue siendo: **usar traits para las abstracciones** y `#[automock]` en los traits.

---

## 18. Mocking de funciones libres (mock!)

Para funciones libres (fuera de impl blocks), mockall ofrece `mock!` con un módulo mock:

```rust
// La función real
pub fn fetch_temperature(city: &str) -> Result<f64, String> {
    // llamada HTTP real
    todo!()
}

// Mock de la función libre
#[cfg(test)]
mod tests {
    use mockall::mock;

    mock! {
        pub TempFetcher {}
        impl TempFetcher {
            pub fn fetch_temperature(city: &str) -> Result<f64, String>;
        }
    }

    // Ahora usas MockTempFetcher::fetch_temperature
    // Pero necesitas refactorizar el código para usar el mock
    // (funciones libres no se pueden inyectar directamente)
}
```

**Realidad práctica**: mockear funciones libres requiere refactorizar el código para inyectar la dependencia. mockall no puede "interceptar" llamadas a funciones libres. La solución idiomática: convertir la función libre en un trait method.

---

## 19. mock!: definición manual de mocks

`mock!` permite definir mocks cuando `#[automock]` no es suficiente:

### Sintaxis de mock!

```rust
use mockall::mock;

mock! {
    pub NombreDelMock {
        // Métodos inherentes (no de un trait)
        pub fn foo(&self, x: i32) -> String;
        pub fn bar(&mut self, data: Vec<u8>);
    }

    // Implementación de un trait
    impl SomeTrait for NombreDelMock {
        fn trait_method(&self, arg: &str) -> bool;
    }

    // Implementación de otro trait
    impl AnotherTrait for NombreDelMock {
        type Output = String;
        fn produce(&self) -> String;
    }
}
```

Esto genera `MockNombreDelMock`.

### Caso de uso: traits de crates externos

```rust
// No puedes poner #[automock] en un trait que no defines tú
// Por ejemplo, el trait Read de std::io

use mockall::mock;
use std::io::{self, Read};

mock! {
    pub FileReader {}

    impl Read for FileReader {
        fn read(&mut self, buf: &mut [u8]) -> io::Result<usize>;
    }
}

#[test]
fn test_read_mock() {
    let mut mock = MockFileReader::new();

    mock.expect_read()
        .times(1)
        .returning(|buf| {
            let data = b"hello";
            let len = data.len().min(buf.len());
            buf[..len].copy_from_slice(&data[..len]);
            Ok(len)
        });

    let mut buf = [0u8; 10];
    let n = mock.read(&mut buf).unwrap();
    assert_eq!(n, 5);
    assert_eq!(&buf[..5], b"hello");
}
```

### Caso de uso: traits con macros que #[automock] no procesa

```rust
use mockall::mock;

// Cuando un trait usa macros complejas en su definición
// y #[automock] no puede expandirlo correctamente

mock! {
    pub HttpClient {}

    impl HttpClient {
        pub fn get(&self, url: &str) -> Result<Response, Error>;
        pub fn post(&self, url: &str, body: &[u8]) -> Result<Response, Error>;
        pub fn put(&self, url: &str, body: &[u8]) -> Result<Response, Error>;
        pub fn delete(&self, url: &str) -> Result<Response, Error>;
    }
}
```

---

## 20. Múltiples traits en un mock

Un mock puede implementar varios traits simultáneamente:

### Con mock!

```rust
use mockall::mock;

pub trait Readable {
    fn read_line(&self) -> Option<String>;
}

pub trait Writable {
    fn write_line(&mut self, line: &str) -> Result<(), String>;
}

pub trait Seekable {
    fn seek(&mut self, position: u64) -> Result<(), String>;
    fn position(&self) -> u64;
}

mock! {
    pub File {}

    impl Readable for File {
        fn read_line(&self) -> Option<String>;
    }

    impl Writable for File {
        fn write_line(&mut self, line: &str) -> Result<(), String>;
    }

    impl Seekable for File {
        fn seek(&mut self, position: u64) -> Result<(), String>;
        fn position(&self) -> u64;
    }
}

#[test]
fn test_multi_trait_mock() {
    let mut mock = MockFile::new();

    // Configurar expectativas de Readable
    mock.expect_read_line()
        .returning(|| Some("first line".to_string()));

    // Configurar expectativas de Writable
    mock.expect_write_line()
        .returning(|_| Ok(()));

    // Configurar expectativas de Seekable
    mock.expect_seek()
        .returning(|_| Ok(()));
    mock.expect_position()
        .return_const(0u64);

    // Usar el mock donde se necesite cualquiera de los traits
    assert_eq!(mock.read_line(), Some("first line".to_string()));
    mock.write_line("new line").unwrap();
    mock.seek(100).unwrap();
    assert_eq!(mock.position(), 0);
}
```

### Usar donde se requieren múltiples bounds

```rust
pub fn copy_data<F>(file: &mut F) -> Result<String, String>
where
    F: Readable + Seekable,
{
    file.seek(0)?;
    let mut content = String::new();
    while let Some(line) = file.read_line() {
        content.push_str(&line);
        content.push('\n');
    }
    Ok(content)
}

#[test]
fn test_copy_data() {
    let mut mock = MockFile::new();
    let mut call_count = 0u32;

    mock.expect_seek()
        .with(mockall::predicate::eq(0))
        .times(1)
        .returning(|_| Ok(()));

    mock.expect_read_line()
        .times(3)
        .returning(move || {
            call_count += 1;
            match call_count {
                1 => Some("line 1".to_string()),
                2 => Some("line 2".to_string()),
                _ => None,
            }
        });

    let result = copy_data(&mut mock).unwrap();
    assert_eq!(result, "line 1\nline 2\n");
}
```

---

## 21. Mocking con lifetimes y referencias

### Métodos que retornan referencias

```rust
// ⚠️ mockall tiene limitaciones con métodos que retornan referencias
// porque el mock necesita ser dueño del dato al que apunta la referencia

#[automock]
pub trait Config {
    fn get(&self, key: &str) -> Option<&str>;  // retorna &str
}

// mockall genera:
// MockConfig con expect_get que usa returning
// PERO el returning closure retorna &str — ¿owned por quién?
```

Para manejar esto, mockall tiene soporte especial:

```rust
#[test]
fn test_returning_references() {
    let mut mock = MockConfig::new();

    // return_const funciona con tipos estáticos
    mock.expect_get()
        .with(mockall::predicate::eq("host"))
        .return_const(Some("localhost"));

    // Para valores dinámicos, la referencia debe vivir lo suficiente
    // mockall almacena el valor internamente

    assert_eq!(mock.get("host"), Some("localhost"));
}
```

### Métodos con parámetros de lifetime explícitos

```rust
#[automock]
pub trait Parser<'a> {
    fn parse(&self, input: &'a str) -> Vec<&'a str>;
}

// mockall genera MockParser<'a>
// Los tests necesitan que el input viva lo suficiente

#[test]
fn test_parser_with_lifetime() {
    let mut mock = MockParser::new();
    let input = "hello world".to_string(); // vive todo el test

    mock.expect_parse()
        .returning(|s: &str| s.split_whitespace().collect());

    let result = mock.parse(&input);
    assert_eq!(result, vec!["hello", "world"]);
}
```

### La regla de oro: evitar referencias en retornos de traits mockeables

```
Diseño difícil de mockear:

  trait UserRepo {
      fn find(&self, id: u64) -> Option<&User>;  // referencia al repo
  }

Diseño fácil de mockear:

  trait UserRepo {
      fn find(&self, id: u64) -> Option<User>;  // owned value
  }

  // O con Cow para flexibilidad:
  trait UserRepo {
      fn find(&self, id: u64) -> Option<Cow<'_, User>>;
  }
```

---

## 22. Errores comunes y soluciones

### Error 1: "No matching expectation found"

```
thread 'tests::my_test' panicked at 'MockFoo::bar(42):
No matching expectation found'
```

**Causas**:
- No configuraste `expect_bar()` para el argumento 42
- El predicado `with` no matchea
- La expectativa ya se consumió (times agotado)

```rust
// ✗ Problema: olvidar configurar para el arg correcto
mock.expect_bar()
    .with(predicate::eq(10))   // solo acepta 10
    .returning(|_| "ok");

mock.bar(42);  // PANIC: no hay expectativa para 42

// ✓ Solución
mock.expect_bar()
    .with(predicate::eq(42))
    .returning(|_| "ok");
```

### Error 2: "Expectation unsatisfied" en drop

```
thread 'tests::my_test' panicked at 'MockFoo::bar:
Expectation(<anything>) called 0 times, expected at least 1'
```

**Causa**: configuraste una expectativa pero nunca se llamó.

```rust
// ✗ Problema
mock.expect_bar().returning(|_| 42);  // espera ≥1 llamada
// test termina sin llamar bar() → PANIC

// ✓ Solución 1: marcar como opcional
mock.expect_bar().times(0..).returning(|_| 42);

// ✓ Solución 2: usar times(0) si no debe llamarse
mock.expect_bar().never();
```

### Error 3: "Returning a reference from a mock is not yet supported"

**Causa**: el trait retorna una referencia y mockall no puede manejarla con `returning`.

```rust
// ✗ Problema
#[automock]
pub trait Store {
    fn get(&self, key: &str) -> &[u8];
}
// returning(|_| &[1,2,3])  — ¿quién es el dueño de [1,2,3]?

// ✓ Solución: cambiar el trait para retornar owned
pub trait Store {
    fn get(&self, key: &str) -> Vec<u8>;
}
```

### Error 4: Errores de compilación con genéricos

```
error[E0277]: the trait bound `T: 'static` is not satisfied
```

**Causa**: mockall requiere `'static` en los parámetros de tipo.

```rust
// ✗ Problema
#[automock]
pub trait Processor<T> {
    fn process(&self, item: T) -> T;
}

// ✓ Solución: añadir bounds
#[automock]
pub trait Processor<T: 'static> {
    fn process(&self, item: T) -> T;
}
```

### Error 5: "returning() requires a Clone type"

```rust
// ✗ Problema
mock.expect_produce()
    .returning(|| TcpStream::connect("localhost:8080").unwrap());
// Error: TcpStream no implementa Clone — pero returning es Fn, no FnOnce

// ✓ El closure se ejecuta cada vez, creando un nuevo TcpStream
// El error real sería otro. Pero si tienes un valor no-Clone precreado:
let stream = TcpStream::connect("localhost:8080").unwrap();
// mock.expect_produce().returning(move || stream); // ERROR: mueve stream

// ✓ Solución: return_once
mock.expect_produce()
    .times(1)
    .return_once(move || stream);
```

### Error 6: Tests no determinísticos con threads

```rust
// ✗ Problema: mockall mocks no son Send/Sync por defecto
// si usas closure con RefCell, Arc<Mutex<...>>, etc.

// ✓ Solución: los mocks generados por mockall SÍ son Send
// El problema suele ser que returning captura algo no-Send

// Usar Arc<Mutex<...>> en lugar de Rc<RefCell<...>> para tests async
```

### Error 7: Conflicto de nombres con #[automock]

```rust
// ✗ Si ya tienes un struct llamado MockFoo
pub struct MockFoo;  // conflicto

#[automock]
pub trait Foo {
    fn bar(&self);
}
// Genera MockFoo → conflicto de nombres

// ✓ Solución: renombrar tu struct o usar mock! con nombre diferente
```

### Error 8: #[automock] con #[cfg(test)]

```rust
// ✗ No funciona: el trait desaparece en producción
#[cfg(test)]
#[automock]
pub trait Foo { fn bar(&self); }

// ✓ Correcto: usar cfg_attr
#[cfg_attr(test, mockall::automock)]
pub trait Foo { fn bar(&self); }
```

---

## 23. Patrones avanzados

### Patrón: expectativas condicionales con closures

```rust
#[automock]
pub trait RateLimiter {
    fn check(&self, key: &str) -> bool; // true = allowed
}

#[test]
fn test_rate_limiting_behavior() {
    let mut mock = MockRateLimiter::new();

    let call_count = std::sync::Arc::new(std::sync::atomic::AtomicU32::new(0));
    let count_clone = call_count.clone();

    // Permitir las primeras 3 llamadas, bloquear las siguientes
    mock.expect_check()
        .returning(move |_| {
            let n = count_clone.fetch_add(1, std::sync::atomic::Ordering::SeqCst);
            n < 3
        });

    assert!(mock.check("user:1"));   // llamada 1: ok
    assert!(mock.check("user:1"));   // llamada 2: ok
    assert!(mock.check("user:1"));   // llamada 3: ok
    assert!(!mock.check("user:1"));  // llamada 4: bloqueada
    assert!(!mock.check("user:1"));  // llamada 5: bloqueada
}
```

### Patrón: respuestas diferentes por argumento

```rust
use mockall::predicate::*;

#[automock]
pub trait ConfigStore {
    fn get(&self, key: &str) -> Option<String>;
}

#[test]
fn test_multiple_config_values() {
    let mut mock = MockConfigStore::new();

    // Cada key retorna un valor diferente
    mock.expect_get()
        .with(eq("database.host"))
        .return_const(Some("localhost".to_string()));

    mock.expect_get()
        .with(eq("database.port"))
        .return_const(Some("5432".to_string()));

    mock.expect_get()
        .with(eq("database.name"))
        .return_const(Some("mydb".to_string()));

    // Cualquier otra key retorna None
    mock.expect_get()
        .return_const(None::<String>);

    assert_eq!(mock.get("database.host"), Some("localhost".into()));
    assert_eq!(mock.get("database.port"), Some("5432".into()));
    assert_eq!(mock.get("database.name"), Some("mydb".into()));
    assert_eq!(mock.get("unknown.key"), None);
}
```

**Nota**: recuerda el orden LIFO. Las expectativas más específicas (con `with`) deben ir **después** de las generales (sin `with`), o la general capturará todo primero.

```
Orden correcto (LIFO):

  Añadida primero:  expect_get().return_const(None)     ← fallback general
  Añadida después:  expect_get().with(eq("host"))...     ← específica

  Evaluación: específica primero (LIFO) → si no matchea → general
```

```
Orden INCORRECTO (LIFO):

  Añadida primero:  expect_get().with(eq("host"))...     ← específica
  Añadida después:  expect_get().return_const(None)      ← general

  Evaluación: general primero (LIFO) → SIEMPRE matchea → nunca llega a la específica
  
  ⚠️ Todas las llamadas retornan None
```

### Patrón: mock como state machine

```rust
use std::sync::{Arc, Mutex};

#[automock]
pub trait Connection {
    fn connect(&mut self) -> Result<(), String>;
    fn send(&self, data: &[u8]) -> Result<usize, String>;
    fn disconnect(&mut self) -> Result<(), String>;
    fn is_connected(&self) -> bool;
}

#[derive(Clone, Debug, PartialEq)]
enum ConnState { Disconnected, Connected }

fn create_stateful_connection_mock() -> MockConnection {
    let state = Arc::new(Mutex::new(ConnState::Disconnected));

    let mut mock = MockConnection::new();

    let s = state.clone();
    mock.expect_connect()
        .returning(move || {
            let mut st = s.lock().unwrap();
            match *st {
                ConnState::Disconnected => {
                    *st = ConnState::Connected;
                    Ok(())
                }
                ConnState::Connected => Err("already connected".into()),
            }
        });

    let s = state.clone();
    mock.expect_send()
        .returning(move |data| {
            let st = s.lock().unwrap();
            match *st {
                ConnState::Connected => Ok(data.len()),
                ConnState::Disconnected => Err("not connected".into()),
            }
        });

    let s = state.clone();
    mock.expect_disconnect()
        .returning(move || {
            let mut st = s.lock().unwrap();
            *st = ConnState::Disconnected;
            Ok(())
        });

    let s = state.clone();
    mock.expect_is_connected()
        .returning(move || {
            *s.lock().unwrap() == ConnState::Connected
        });

    mock
}

#[test]
fn test_connection_lifecycle() {
    let mut mock = create_stateful_connection_mock();

    assert!(!mock.is_connected());

    // Enviar sin conectar → error
    assert!(mock.send(b"hello").is_err());

    // Conectar
    mock.connect().unwrap();
    assert!(mock.is_connected());

    // Conectar de nuevo → error
    assert!(mock.connect().is_err());

    // Enviar datos
    assert_eq!(mock.send(b"hello").unwrap(), 5);

    // Desconectar
    mock.disconnect().unwrap();
    assert!(!mock.is_connected());
}
```

### Patrón: combinación de expectations con valores de retorno calculados

```rust
#[automock]
pub trait PricingEngine {
    fn get_price(&self, product_id: u64, quantity: u32) -> f64;
    fn get_discount(&self, user_tier: &str, total: f64) -> f64;
}

fn setup_realistic_pricing_mock() -> MockPricingEngine {
    let mut mock = MockPricingEngine::new();

    // Precios basados en producto
    let prices = std::collections::HashMap::from([
        (1u64, 10.0),
        (2, 25.0),
        (3, 5.50),
    ]);

    mock.expect_get_price()
        .returning(move |product_id, quantity| {
            let base = prices.get(&product_id).copied().unwrap_or(0.0);
            base * quantity as f64
        });

    mock.expect_get_discount()
        .returning(|tier, total| {
            match tier {
                "gold" if total > 100.0 => total * 0.15,
                "gold" => total * 0.10,
                "silver" if total > 100.0 => total * 0.08,
                "silver" => total * 0.05,
                _ => 0.0,
            }
        });

    mock
}
```

---

## 24. mockall vs alternativas

### Panorama del ecosistema de mocking en Rust

```
┌─────────────────────────────────────────────────────────────────────┐
│                   Crates de mocking en Rust                         │
├──────────────┬─────────────┬────────────────┬──────────────────────┤
│   mockall    │  mockito    │  faux          │  unimock             │
│              │             │                │                      │
│  Proc macro  │  HTTP mock  │  Proc macro    │  Proc macro          │
│  General     │  Específico │  #[faux]       │  #[unimock]          │
│  purpose     │  para HTTP  │  en struct     │  API unificada       │
│              │             │                │                      │
│  El más      │  Para tests │  Mockea struct │  Todos los mocks     │
│  popular     │  de clientes│  directamente  │  implementan un      │
│  (~60M dl)   │  HTTP       │  (no traits)   │  trait Unimock       │
├──────────────┼─────────────┼────────────────┼──────────────────────┤
│ 62M descargas│ 20M         │ 4M             │ 1M                   │
│ Madurez: Alta│ Alta        │ Media          │ Media                │
└──────────────┴─────────────┴────────────────┴──────────────────────┘
```

### Comparación detallada: mockall vs faux

```
┌────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Característica         │ mockall                  │ faux                     │
├────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Target                 │ traits                   │ structs                  │
│ Macro principal        │ #[automock]              │ #[faux::create]          │
│ Verificación de args   │ with(), withf()          │ when_then()              │
│ Verificación de times  │ times()                  │ No built-in             │
│ Sequences              │ Sí                       │ No                       │
│ Checkpoints            │ Sí                       │ No                       │
│ Traits genéricos       │ Sí                       │ N/A (structs)            │
│ Async                  │ Sí                       │ Sí                       │
│ Complejidad setup      │ Media                    │ Baja                     │
│ Mensajes de error      │ Buenos                   │ Buenos                   │
│ Requiere DI con traits │ Sí                       │ No (reemplaza struct)    │
└────────────────────────┴──────────────────────────┴──────────────────────────┘
```

### Comparación: mockall vs mocks manuales

```
┌────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Característica         │ mockall                  │ Manual                   │
├────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Boilerplate            │ Mínimo                   │ Alto                     │
│ Flexibilidad           │ API predefinida          │ Total                    │
│ Fakes con lógica       │ No (solo retornos)       │ Sí (InMemoryDB, etc.)    │
│ Curva de aprendizaje   │ Media (API grande)       │ Baja (solo Rust normal)  │
│ Verificación de args   │ Built-in                 │ Manual con assert        │
│ Mantenimiento          │ Automático               │ Manual al cambiar trait  │
│ Errores de compilación │ Proc macro (a veces      │ Claros                   │
│                        │ crípticos)               │                          │
│ Tiempo de compilación  │ Más lento (proc macros)  │ Normal                   │
│ Depuración             │ Paso por macro            │ Directo                  │
└────────────────────────┴──────────────────────────┴──────────────────────────┘
```

### Diagrama de decisión

```
¿Necesitas mock?
      │
      ▼
┌─────────────┐
│ ¿Cuántos    │
│ métodos     │
│ tiene el    │
│ trait?      │
└──┬──────┬───┘
   │      │
  1-2    3+
   │      │
   ▼      ▼
 Manual  ┌─────────────┐
 mock    │ ¿Necesitas   │
         │ verificar    │
         │ orden/times/ │
         │ args?        │
         └─┬────────┬───┘
           │        │
          Sí        No
           │        │
           ▼        ▼
        mockall    Manual mock
                   (más simple)

¿Necesitas un fake con lógica? ──► Manual siempre
¿Es un trait de crate externo? ──► mock! { }
¿Es un struct sin trait?       ──► faux o refactorizar a trait
```

---

## 25. Comparación con C y Go

### C: no hay sistema de mocking estándar

```c
/* C: mocking requiere trucos de preprocesador o link-time replacement */

/* Opción 1: function pointers (manual DI, como vimos en T01) */
typedef struct {
    int (*query)(const char* sql, void* result);
    int (*execute)(const char* sql);
} DatabaseOps;

/* "Mock" manual */
static int mock_query_result = 0;
static int mock_query(const char* sql, void* result) {
    (void)sql; (void)result;
    return mock_query_result;
}

DatabaseOps create_mock_db(void) {
    return (DatabaseOps){
        .query = mock_query,
        .execute = mock_execute,
    };
}

/* No hay:
   - Verificación automática de llamadas
   - Matchers predefinidos
   - Sequences
   - Mensajes de error descriptivos
   Todo se hace manualmente con contadores globales y asserts */

/* Opción 2: CMock (framework de terceros para C) */
/* Genera mocks a partir de headers con un script Ruby */
/* cmock_generate.rb myheader.h */
/* Genera: Mockmyheader.c con expect_*, verify, etc. */
/* Similar conceptualmente a mockall pero más limitado */

/* Opción 3: link-time substitution */
/* Compilar con -Wl,--wrap=real_function */
/* Define __wrap_real_function en el test */
/* El linker redirige llamadas */
```

### Go: interfaces implícitas + gomock/testify

```go
// Go: interfaces implícitas simplifican el mocking
package main

// El trait (interface en Go)
type UserRepository interface {
    FindByID(id uint64) (*User, error)
    Save(user *User) (uint64, error)
    Delete(id uint64) error
}

// --- Con gomock (equivalente más cercano a mockall) ---

//go:generate mockgen -source=repo.go -destination=mock_repo.go -package=main

// Genera MockUserRepository automáticamente

func TestFindUser(t *testing.T) {
    ctrl := gomock.NewController(t)
    defer ctrl.Finish() // equivale al Drop de mockall

    mock := NewMockUserRepository(ctrl)

    // expect + returning
    mock.EXPECT().
        FindByID(gomock.Eq(uint64(42))).    // with(eq(42))
        Return(&User{ID: 42, Name: "Alice"}, nil). // returning
        Times(1)                             // times(1)

    // sequence
    seq := gomock.InOrder(
        mock.EXPECT().FindByID(gomock.Any()),
        mock.EXPECT().Save(gomock.Any()),
    )
    _ = seq

    service := NewUserService(mock)
    name := service.GetDisplayName(42)
    assert.Equal(t, "Alice", name)
}

// --- Con testify/mock (más popular, menos estricto) ---

type MockRepo struct {
    mock.Mock
}

func (m *MockRepo) FindByID(id uint64) (*User, error) {
    args := m.Called(id) // registra la llamada
    return args.Get(0).(*User), args.Error(1)
}

func TestWithTestify(t *testing.T) {
    m := new(MockRepo)
    m.On("FindByID", uint64(42)).
        Return(&User{ID: 42, Name: "Alice"}, nil).
        Once() // times(1)

    service := NewUserService(m)
    name := service.GetDisplayName(42)
    assert.Equal(t, "Alice", name)

    m.AssertExpectations(t) // verificar al final
}
```

### Tabla comparativa trilingüe

```
┌─────────────────────┬───────────────────┬───────────────────┬───────────────────┐
│ Concepto            │ Rust (mockall)    │ Go (gomock)       │ C                 │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Generación          │ #[automock] proc  │ mockgen code gen  │ CMock script Ruby │
│                     │ macro en compile  │ (pre-compilación) │ (pre-compilación) │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Matchers            │ predicate::eq,    │ gomock.Eq,        │ Manual con ==     │
│                     │ gt, str::contains │ gomock.Any        │ y if              │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Verificar times     │ .times(n)         │ .Times(n)         │ Contador global   │
│                     │ auto en Drop      │ ctrl.Finish()     │ assert manual     │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Sequences           │ Sequence::new()   │ gomock.InOrder()  │ No existe         │
│                     │ .in_sequence()    │                   │                   │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Retorno dinámico    │ .returning(       │ .DoAndReturn(     │ Variable global   │
│                     │   |args| ...)     │   func() {...})   │ consultada        │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Async               │ Sí (nativo 1.75+) │ N/A (goroutines)  │ N/A               │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Type safety         │ Compile-time      │ Runtime           │ Ninguna           │
│                     │ (proc macro)      │ (reflection)      │ (void pointers)   │
├─────────────────────┼───────────────────┼───────────────────┼───────────────────┤
│ Error si falla match│ Panic descriptivo │ Fatal + msg       │ Segfault o        │
│                     │ con args y expect │ con args          │ comportamiento    │
│                     │                   │                   │ indefinido        │
└─────────────────────┴───────────────────┴───────────────────┴───────────────────┘
```

---

## 26. Anti-patrones con mockall

### Anti-patrón 1: Mockear todo

```rust
// ✗ Mockear cada dependencia, incluso las triviales
#[automock]
pub trait StringFormatter {
    fn to_uppercase(&self, s: &str) -> String;
    fn trim(&self, s: &str) -> String;
}

// La lógica de String es determinística y rápida
// No necesita mock — usa la implementación real
```

**Regla**: si la dependencia es **pura, rápida y determinística**, no la mockees.

### Anti-patrón 2: Tests que solo verifican la secuencia de llamadas

```rust
// ✗ El test solo verifica QUE se llamaron los métodos, no QUÉ hace el sistema
#[test]
fn test_registration() {
    let mut mock_db = MockDatabase::new();
    let mut mock_email = MockEmailService::new();

    mock_db.expect_save_user().times(1).returning(|_| Ok(1));
    mock_email.expect_send_welcome().times(1).returning(|_| Ok(()));

    register_user(&mut mock_db, &mut mock_email, "alice@test.com");
    // ¿Qué verificamos? Solo que se llamaron dos funciones
    // Si alguien reordena las llamadas internas, el test rompe
    // pero el sistema sigue funcionando correctamente
}

// ✓ Mejor: verificar el RESULTADO observable
#[test]
fn test_registration_returns_user_id() {
    let mut mock_db = MockDatabase::new();
    let mut mock_email = MockEmailService::new();

    mock_db.expect_save_user()
        .returning(|_| Ok(42));
    mock_email.expect_send_welcome()
        .returning(|_| Ok(()));

    let result = register_user(&mut mock_db, &mut mock_email, "alice@test.com");
    assert_eq!(result.unwrap().id, 42); // verifica resultado, no implementación
}
```

### Anti-patrón 3: Mocks que replican la implementación

```rust
// ✗ El mock tiene tanta lógica como el código real
mock.expect_calculate_tax()
    .withf(|amount, state| *amount > 0.0 && !state.is_empty())
    .returning(|amount, state| {
        match state {
            "CA" => amount * 0.0725,
            "NY" => amount * 0.08,
            "TX" => amount * 0.0625,
            _ => amount * 0.05,
        }
    });
// Si hay un bug en estas tasas, ¿está en el mock o en el código real?

// ✓ Mejor: valor fijo que prueba un comportamiento
mock.expect_calculate_tax()
    .return_const(7.25); // para este test, el impuesto es $7.25
```

### Anti-patrón 4: Verificación excesiva de argumentos

```rust
// ✗ Verificar cada byte del argumento
mock.expect_send_email()
    .withf(|to, subject, body| {
        to == "alice@example.com"
            && subject == "Welcome to Our Platform - Your Account is Ready"
            && body == "Dear Alice,\n\nWelcome to our platform...\n\nBest regards,\nThe Team"
    })
    .returning(|_, _, _| Ok(()));
// Si cambias una coma en el template, el test rompe

// ✓ Verificar solo lo esencial
mock.expect_send_email()
    .withf(|to, subject, _body| {
        to == "alice@example.com" && subject.contains("Welcome")
    })
    .returning(|_, _, _| Ok(()));
```

### Anti-patrón 5: Ignorar la semántica del mock al drop

```rust
// ✗ Ignorar el panic del mock con catch_unwind
let result = std::panic::catch_unwind(|| {
    let mut mock = MockFoo::new();
    mock.expect_bar().times(1).returning(|| 42);
    // sin llamar bar()
}); // el mock hace panic en drop, pero lo ignoramos

// ✓ Si no necesitas verificar, usa times(0..)
let mut mock = MockFoo::new();
mock.expect_bar().times(0..).returning(|| 42);
```

---

## 27. Ejemplo completo: sistema de e-commerce

```rust
// ============================================================
// Sistema de e-commerce: procesamiento de pedidos con mockall
// ============================================================

use mockall::automock;
use std::collections::HashMap;

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct Product {
    pub id: u64,
    pub name: String,
    pub price_cents: u64,
    pub stock: u32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct OrderItem {
    pub product_id: u64,
    pub quantity: u32,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Order {
    pub id: Option<u64>,
    pub user_id: u64,
    pub items: Vec<OrderItem>,
    pub total_cents: u64,
    pub status: OrderStatus,
}

#[derive(Debug, Clone, PartialEq)]
pub enum OrderStatus {
    Pending,
    Confirmed,
    Paid,
    Shipped,
    Cancelled,
}

#[derive(Debug, PartialEq)]
pub enum OrderError {
    EmptyOrder,
    ProductNotFound(u64),
    InsufficientStock { product_id: u64, available: u32, requested: u32 },
    PaymentFailed(String),
    NotificationFailed(String),
    StorageError(String),
}

// ── Traits (contratos) ──────────────────────────────────────

#[automock]
pub trait ProductCatalog {
    fn get_product(&self, id: u64) -> Result<Option<Product>, String>;
    fn reserve_stock(&self, product_id: u64, quantity: u32) -> Result<(), String>;
    fn release_stock(&self, product_id: u64, quantity: u32) -> Result<(), String>;
}

#[automock]
pub trait OrderStore {
    fn save(&self, order: &Order) -> Result<u64, String>;
    fn update_status(&self, order_id: u64, status: OrderStatus) -> Result<(), String>;
}

#[automock]
pub trait PaymentProcessor {
    fn charge(&self, user_id: u64, amount_cents: u64) -> Result<String, String>;
    fn refund(&self, transaction_id: &str) -> Result<(), String>;
}

#[automock]
pub trait NotificationSender {
    fn send_order_confirmation(&self, user_id: u64, order_id: u64) -> Result<(), String>;
    fn send_payment_receipt(&self, user_id: u64, transaction_id: &str) -> Result<(), String>;
}

// ── Servicio: OrderService ──────────────────────────────────

pub struct OrderService<C, S, P, N>
where
    C: ProductCatalog,
    S: OrderStore,
    P: PaymentProcessor,
    N: NotificationSender,
{
    catalog: C,
    store: S,
    payment: P,
    notifier: N,
}

impl<C, S, P, N> OrderService<C, S, P, N>
where
    C: ProductCatalog,
    S: OrderStore,
    P: PaymentProcessor,
    N: NotificationSender,
{
    pub fn new(catalog: C, store: S, payment: P, notifier: N) -> Self {
        OrderService { catalog, store, payment, notifier }
    }

    pub fn place_order(
        &self,
        user_id: u64,
        items: Vec<OrderItem>,
    ) -> Result<Order, OrderError> {
        // 1. Validar que hay items
        if items.is_empty() {
            return Err(OrderError::EmptyOrder);
        }

        // 2. Verificar productos y calcular total
        let mut total_cents = 0u64;
        let mut verified_items = Vec::new();

        for item in &items {
            let product = self.catalog
                .get_product(item.product_id)
                .map_err(|e| OrderError::StorageError(e))?
                .ok_or(OrderError::ProductNotFound(item.product_id))?;

            if product.stock < item.quantity {
                return Err(OrderError::InsufficientStock {
                    product_id: item.product_id,
                    available: product.stock,
                    requested: item.quantity,
                });
            }

            total_cents += product.price_cents * item.quantity as u64;
            verified_items.push(item.clone());
        }

        // 3. Reservar stock
        for item in &verified_items {
            self.catalog
                .reserve_stock(item.product_id, item.quantity)
                .map_err(|e| OrderError::StorageError(e))?;
        }

        // 4. Guardar pedido
        let mut order = Order {
            id: None,
            user_id,
            items: verified_items.clone(),
            total_cents,
            status: OrderStatus::Pending,
        };

        let order_id = self.store
            .save(&order)
            .map_err(|e| OrderError::StorageError(e))?;
        order.id = Some(order_id);

        // 5. Cobrar
        match self.payment.charge(user_id, total_cents) {
            Ok(tx_id) => {
                // Actualizar estado a Paid
                let _ = self.store.update_status(order_id, OrderStatus::Paid);
                order.status = OrderStatus::Paid;

                // 6. Notificar (best-effort: no falla el pedido)
                let _ = self.notifier.send_order_confirmation(user_id, order_id);
                let _ = self.notifier.send_payment_receipt(user_id, &tx_id);
            }
            Err(e) => {
                // Rollback: liberar stock
                for item in &verified_items {
                    let _ = self.catalog.release_stock(item.product_id, item.quantity);
                }
                let _ = self.store.update_status(order_id, OrderStatus::Cancelled);
                return Err(OrderError::PaymentFailed(e));
            }
        }

        Ok(order)
    }
}

// ── Tests ────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use mockall::predicate::*;
    use mockall::Sequence;

    // Helper: crear producto
    fn product(id: u64, name: &str, price_cents: u64, stock: u32) -> Product {
        Product {
            id,
            name: name.to_string(),
            price_cents,
            stock,
        }
    }

    // Helper: crear servicio con mocks default
    fn setup() -> (
        MockProductCatalog,
        MockOrderStore,
        MockPaymentProcessor,
        MockNotificationSender,
    ) {
        (
            MockProductCatalog::new(),
            MockOrderStore::new(),
            MockPaymentProcessor::new(),
            MockNotificationSender::new(),
        )
    }

    // ── Test 1: Happy path ────────────────────────────────

    #[test]
    fn place_order_happy_path() {
        let (mut catalog, mut store, mut payment, mut notifier) = setup();

        // Catálogo tiene el producto
        catalog.expect_get_product()
            .with(eq(1))
            .returning(|_| Ok(Some(product(1, "Widget", 1000, 50))));

        // Reservar stock exitoso
        catalog.expect_reserve_stock()
            .with(eq(1), eq(2))
            .times(1)
            .returning(|_, _| Ok(()));

        // Guardar pedido retorna ID
        store.expect_save()
            .times(1)
            .returning(|_| Ok(100));

        // Actualizar estado
        store.expect_update_status()
            .with(eq(100), eq(OrderStatus::Paid))
            .times(1)
            .returning(|_, _| Ok(()));

        // Cobrar exitoso
        payment.expect_charge()
            .with(eq(42), eq(2000))  // 2 widgets × $10.00
            .times(1)
            .returning(|_, _| Ok("tx-abc-123".to_string()));

        // Notificaciones (best-effort)
        notifier.expect_send_order_confirmation()
            .times(1)
            .returning(|_, _| Ok(()));
        notifier.expect_send_payment_receipt()
            .times(1)
            .returning(|_, _| Ok(()));

        let service = OrderService::new(catalog, store, payment, notifier);
        let order = service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 2 },
        ]).unwrap();

        assert_eq!(order.id, Some(100));
        assert_eq!(order.user_id, 42);
        assert_eq!(order.total_cents, 2000);
        assert_eq!(order.status, OrderStatus::Paid);
    }

    // ── Test 2: Pedido vacío ──────────────────────────────

    #[test]
    fn empty_order_rejected() {
        let (catalog, store, payment, notifier) = setup();

        let service = OrderService::new(catalog, store, payment, notifier);
        let result = service.place_order(42, vec![]);

        assert_eq!(result.unwrap_err(), OrderError::EmptyOrder);
    }

    // ── Test 3: Producto no existe ────────────────────────

    #[test]
    fn product_not_found() {
        let (mut catalog, store, payment, notifier) = setup();

        catalog.expect_get_product()
            .with(eq(999))
            .returning(|_| Ok(None));

        let service = OrderService::new(catalog, store, payment, notifier);
        let result = service.place_order(42, vec![
            OrderItem { product_id: 999, quantity: 1 },
        ]);

        assert_eq!(result.unwrap_err(), OrderError::ProductNotFound(999));
    }

    // ── Test 4: Stock insuficiente ────────────────────────

    #[test]
    fn insufficient_stock() {
        let (mut catalog, store, payment, notifier) = setup();

        catalog.expect_get_product()
            .with(eq(1))
            .returning(|_| Ok(Some(product(1, "Widget", 1000, 3))));

        let service = OrderService::new(catalog, store, payment, notifier);
        let result = service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 10 },
        ]);

        assert_eq!(result.unwrap_err(), OrderError::InsufficientStock {
            product_id: 1,
            available: 3,
            requested: 10,
        });
    }

    // ── Test 5: Pago falla → rollback de stock ────────────

    #[test]
    fn payment_failure_releases_stock() {
        let (mut catalog, mut store, mut payment, notifier) = setup();

        catalog.expect_get_product()
            .returning(|_| Ok(Some(product(1, "Widget", 1000, 50))));

        catalog.expect_reserve_stock()
            .returning(|_, _| Ok(()));

        // release_stock DEBE llamarse (rollback)
        catalog.expect_release_stock()
            .with(eq(1), eq(2))
            .times(1)
            .returning(|_, _| Ok(()));

        store.expect_save()
            .returning(|_| Ok(100));

        store.expect_update_status()
            .with(eq(100), eq(OrderStatus::Cancelled))
            .times(1)
            .returning(|_, _| Ok(()));

        // Pago FALLA
        payment.expect_charge()
            .returning(|_, _| Err("card declined".to_string()));

        let service = OrderService::new(catalog, store, payment, notifier);
        let result = service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 2 },
        ]);

        assert!(matches!(result, Err(OrderError::PaymentFailed(_))));
    }

    // ── Test 6: Múltiples productos ───────────────────────

    #[test]
    fn order_with_multiple_products() {
        let (mut catalog, mut store, mut payment, mut notifier) = setup();

        // Dos productos diferentes
        catalog.expect_get_product()
            .with(eq(1))
            .returning(|_| Ok(Some(product(1, "Widget", 1000, 50))));
        catalog.expect_get_product()
            .with(eq(2))
            .returning(|_| Ok(Some(product(2, "Gadget", 2500, 20))));

        catalog.expect_reserve_stock()
            .times(2) // una vez por cada producto
            .returning(|_, _| Ok(()));

        store.expect_save()
            .withf(|order| {
                // Verificar que el total es correcto
                // 3 × $10.00 + 1 × $25.00 = $55.00
                order.total_cents == 5500
                    && order.items.len() == 2
            })
            .returning(|_| Ok(200));

        store.expect_update_status()
            .returning(|_, _| Ok(()));

        payment.expect_charge()
            .with(eq(42), eq(5500))
            .returning(|_, _| Ok("tx-multi".to_string()));

        notifier.expect_send_order_confirmation().returning(|_, _| Ok(()));
        notifier.expect_send_payment_receipt().returning(|_, _| Ok(()));

        let service = OrderService::new(catalog, store, payment, notifier);
        let order = service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 3 },
            OrderItem { product_id: 2, quantity: 1 },
        ]).unwrap();

        assert_eq!(order.total_cents, 5500);
        assert_eq!(order.items.len(), 2);
    }

    // ── Test 7: Orden de operaciones con Sequence ─────────

    #[test]
    fn operations_happen_in_correct_order() {
        let (mut catalog, mut store, mut payment, mut notifier) = setup();
        let mut seq = Sequence::new();

        // 1. Primero: verificar producto
        catalog.expect_get_product()
            .times(1)
            .in_sequence(&mut seq)
            .returning(|_| Ok(Some(product(1, "Widget", 1000, 10))));

        // 2. Segundo: reservar stock
        catalog.expect_reserve_stock()
            .times(1)
            .in_sequence(&mut seq)
            .returning(|_, _| Ok(()));

        // 3. Tercero: guardar pedido
        store.expect_save()
            .times(1)
            .in_sequence(&mut seq)
            .returning(|_| Ok(300));

        // 4. Cuarto: cobrar
        payment.expect_charge()
            .times(1)
            .in_sequence(&mut seq)
            .returning(|_, _| Ok("tx-seq".to_string()));

        // 5. Quinto: actualizar estado
        store.expect_update_status()
            .times(1)
            .in_sequence(&mut seq)
            .returning(|_, _| Ok(()));

        // Notificaciones al final (sin sequence estricta entre ellas)
        notifier.expect_send_order_confirmation().returning(|_, _| Ok(()));
        notifier.expect_send_payment_receipt().returning(|_, _| Ok(()));

        let service = OrderService::new(catalog, store, payment, notifier);
        service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 1 },
        ]).unwrap();
    }

    // ── Test 8: Notificación falla pero pedido sigue ok ───

    #[test]
    fn notification_failure_does_not_fail_order() {
        let (mut catalog, mut store, mut payment, mut notifier) = setup();

        catalog.expect_get_product()
            .returning(|_| Ok(Some(product(1, "Widget", 500, 100))));
        catalog.expect_reserve_stock()
            .returning(|_, _| Ok(()));

        store.expect_save().returning(|_| Ok(400));
        store.expect_update_status().returning(|_, _| Ok(()));

        payment.expect_charge()
            .returning(|_, _| Ok("tx-notif-fail".to_string()));

        // Notificaciones FALLAN
        notifier.expect_send_order_confirmation()
            .returning(|_, _| Err("SMTP down".to_string()));
        notifier.expect_send_payment_receipt()
            .returning(|_, _| Err("SMTP down".to_string()));

        let service = OrderService::new(catalog, store, payment, notifier);
        let order = service.place_order(42, vec![
            OrderItem { product_id: 1, quantity: 1 },
        ]);

        // El pedido debe completarse a pesar del fallo de notificación
        assert!(order.is_ok());
        assert_eq!(order.unwrap().status, OrderStatus::Paid);
    }
}
```

### Estructura del ejemplo

```
place_order() pipeline:

  items ──► validate ──► verify ──► reserve ──► save ──► charge ──► notify
   │          │          products    stock      order     payment    email
   │          │            │          │          │          │         │
   │        Empty?    Exists?     Available?   Get ID    Success?   Best
   │          │       Stock?         │          │          │       effort
   │          ▼          ▼           ▼          ▼          ▼         ▼
   │       EmptyOrder  NotFound  Insufficient  Store    PayFail   Ignore
   │                              Stock        Error    →Rollback  errors
   │
   └── 8 tests cubren todas las ramas:
       1. Happy path completo
       2. Pedido vacío
       3. Producto no encontrado
       4. Stock insuficiente
       5. Pago falla → rollback
       6. Múltiples productos
       7. Orden de operaciones (Sequence)
       8. Notificación falla sin afectar pedido
```

---

## 28. Programa de práctica

### Proyecto: `task_scheduler` — planificador de tareas con mockall

Implementa un planificador de tareas que usa 4 dependencias mockeables con mockall.

### src/lib.rs

```rust
use mockall::automock;
use std::time::SystemTime;

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct Task {
    pub id: Option<u64>,
    pub name: String,
    pub cron_expression: String,
    pub handler_name: String,
    pub enabled: bool,
    pub last_run: Option<SystemTime>,
    pub next_run: Option<SystemTime>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct TaskExecution {
    pub task_id: u64,
    pub started_at: SystemTime,
    pub finished_at: Option<SystemTime>,
    pub status: ExecutionStatus,
    pub output: String,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ExecutionStatus {
    Running,
    Success,
    Failed(String),
    TimedOut,
}

#[derive(Debug, PartialEq)]
pub enum SchedulerError {
    TaskNotFound(u64),
    TaskDisabled(u64),
    HandlerNotFound(String),
    ExecutionFailed(String),
    StorageError(String),
    AlreadyRunning(u64),
}

// ── Traits ───────────────────────────────────────────────────

#[automock]
pub trait TaskStore {
    fn get_task(&self, id: u64) -> Result<Option<Task>, String>;
    fn get_due_tasks(&self, now: SystemTime) -> Result<Vec<Task>, String>;
    fn save_task(&self, task: &Task) -> Result<u64, String>;
    fn update_last_run(&self, task_id: u64, time: SystemTime) -> Result<(), String>;
}

#[automock]
pub trait TaskExecutor {
    fn execute(&self, handler_name: &str, task_id: u64) -> Result<String, String>;
    fn is_running(&self, task_id: u64) -> bool;
}

#[automock]
pub trait ExecutionLog {
    fn record_start(&self, task_id: u64, started_at: SystemTime) -> Result<u64, String>;
    fn record_finish(
        &self,
        execution_id: u64,
        finished_at: SystemTime,
        status: ExecutionStatus,
        output: &str,
    ) -> Result<(), String>;
    fn get_recent(&self, task_id: u64, limit: usize) -> Result<Vec<TaskExecution>, String>;
}

#[automock]
pub trait Clock {
    fn now(&self) -> SystemTime;
}

// ── Servicio ─────────────────────────────────────────────────

pub struct TaskScheduler<S, E, L, C>
where
    S: TaskStore,
    E: TaskExecutor,
    L: ExecutionLog,
    C: Clock,
{
    store: S,
    executor: E,
    log: L,
    clock: C,
    max_concurrent: usize,
}

impl<S, E, L, C> TaskScheduler<S, E, L, C>
where
    S: TaskStore,
    E: TaskExecutor,
    L: ExecutionLog,
    C: Clock,
{
    pub fn new(store: S, executor: E, log: L, clock: C) -> Self {
        TaskScheduler {
            store,
            executor,
            log,
            clock,
            max_concurrent: 10,
        }
    }

    pub fn with_max_concurrent(mut self, max: usize) -> Self {
        self.max_concurrent = max;
        self
    }

    /// Ejecuta una tarea específica por ID
    pub fn run_task(&self, task_id: u64) -> Result<TaskExecution, SchedulerError> {
        // 1. Obtener tarea
        let task = self.store.get_task(task_id)
            .map_err(|e| SchedulerError::StorageError(e))?
            .ok_or(SchedulerError::TaskNotFound(task_id))?;

        // 2. Verificar que está habilitada
        if !task.enabled {
            return Err(SchedulerError::TaskDisabled(task_id));
        }

        // 3. Verificar que no está corriendo
        if self.executor.is_running(task_id) {
            return Err(SchedulerError::AlreadyRunning(task_id));
        }

        // 4. Registrar inicio
        let now = self.clock.now();
        let exec_id = self.log.record_start(task_id, now)
            .map_err(|e| SchedulerError::StorageError(e))?;

        // 5. Ejecutar
        let (status, output) = match self.executor.execute(&task.handler_name, task_id) {
            Ok(output) => (ExecutionStatus::Success, output),
            Err(e) => (ExecutionStatus::Failed(e.clone()), e),
        };

        // 6. Registrar fin
        let finished_at = self.clock.now();
        let _ = self.log.record_finish(exec_id, finished_at, status.clone(), &output);

        // 7. Actualizar last_run
        let _ = self.store.update_last_run(task_id, finished_at);

        Ok(TaskExecution {
            task_id,
            started_at: now,
            finished_at: Some(finished_at),
            status,
            output,
        })
    }

    /// Ejecuta todas las tareas que están pendientes según el reloj
    pub fn tick(&self) -> Vec<Result<TaskExecution, SchedulerError>> {
        let now = self.clock.now();

        let tasks = match self.store.get_due_tasks(now) {
            Ok(tasks) => tasks,
            Err(e) => return vec![Err(SchedulerError::StorageError(e))],
        };

        tasks.iter()
            .take(self.max_concurrent)
            .map(|task| {
                if let Some(id) = task.id {
                    self.run_task(id)
                } else {
                    Err(SchedulerError::StorageError("task without id".into()))
                }
            })
            .collect()
    }
}
```

### Tests que el estudiante debe implementar

Usando `mockall`, escribe tests para:

1. **`run_task` happy path**: tarea existe, está habilitada, no está corriendo → se ejecuta exitosamente, se registra inicio y fin, se actualiza `last_run`
2. **Tarea no encontrada**: `run_task(999)` → `TaskNotFound(999)`
3. **Tarea deshabilitada**: tarea con `enabled: false` → `TaskDisabled`
4. **Tarea ya corriendo**: `is_running` retorna `true` → `AlreadyRunning`
5. **Ejecución falla**: `executor.execute` retorna `Err` → el status es `Failed`, pero se registra el fin
6. **Orden de operaciones con Sequence**: verificar que las operaciones ocurren en orden (get → is_running → record_start → execute → record_finish → update_last_run)
7. **`tick` con múltiples tareas**: 3 tareas pendientes, todas se ejecutan
8. **`tick` respeta `max_concurrent`**: 5 tareas pendientes con `max_concurrent=2` → solo se ejecutan 2
9. **`tick` con storage error**: `get_due_tasks` falla → retorna error
10. **Verificar argumentos con withf**: verificar que `record_start` recibe el `task_id` correcto y que `record_finish` recibe el `execution_id` retornado por `record_start`

### Pistas

- Usa `times(1)` para verificar que cada operación se llama exactamente una vez en el happy path
- Usa `never()` para verificar que `execute` no se llama cuando la tarea está deshabilitada
- Usa `Sequence` para el test 6
- Para el test 8, configura `get_due_tasks` para retornar 5 tareas y verifica que `execute` se llama solo 2 veces
- Crea un helper `fn make_task(id: u64, name: &str, handler: &str, enabled: bool) -> Task` para evitar repetición

---

## 29. Ejercicios

### Ejercicio 1: Cache con expiración

**Objetivo**: Practicar `times`, `returning` con estado, y verificación de argumentos.

**Contexto**: implementa un `CacheService` que consulta un backend lento solo si la caché no tiene el valor o expiró.

```rust
#[automock]
pub trait Backend {
    fn fetch(&self, key: &str) -> Result<String, String>;
}

#[automock]
pub trait CacheStore {
    fn get(&self, key: &str) -> Option<(String, SystemTime)>; // (valor, inserted_at)
    fn set(&self, key: &str, value: &str, ttl_secs: u64) -> Result<(), String>;
}

#[automock]
pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub struct CacheService<B: Backend, C: CacheStore, K: Clock> {
    backend: B,
    cache: C,
    clock: K,
    ttl_secs: u64,
}
```

**Tareas**:

**a)** Implementa `CacheService::get(key)` que:
- Consulta la caché primero
- Si hay hit y no expiró → retorna sin tocar el backend
- Si hay miss o expiró → consulta el backend, almacena en caché, retorna

**b)** Escribe tests con mockall para:
- Cache hit (backend NUNCA se llama — usa `never()`)
- Cache miss (backend se llama exactamente 1 vez)
- Cache expirado (backend se llama porque el TTL pasó)
- Backend falla (el error se propaga)
- Múltiples gets del mismo key (backend se llama solo la primera vez)

---

### Ejercicio 2: Pipeline de procesamiento con Sequences

**Objetivo**: Practicar `Sequence` y verificación de orden estricto.

**Contexto**: un pipeline que debe ejecutar fases en orden estricto.

```rust
#[automock]
pub trait Validator {
    fn validate(&self, data: &str) -> Result<(), Vec<String>>;
}

#[automock]
pub trait Transformer {
    fn transform(&self, data: &str) -> Result<String, String>;
}

#[automock]
pub trait Persister {
    fn persist(&self, data: &str) -> Result<u64, String>;
}

#[automock]
pub trait Notifier {
    fn notify(&self, record_id: u64) -> Result<(), String>;
}
```

**Tareas**:

**a)** Implementa `Pipeline::process(data)` que ejecuta: validate → transform → persist → notify.

**b)** Escribe tests con `Sequence` que verifiquen:
- El orden completo en happy path
- Si validate falla, transform NUNCA se llama
- Si transform falla, persist NUNCA se llama
- Si persist falla, notify NUNCA se llama
- notify recibe el `record_id` retornado por persist

---

### Ejercicio 3: Retry con backoff usando mockall

**Objetivo**: Practicar `returning` con estado y respuestas secuenciales.

**Contexto**:

```rust
#[automock]
pub trait HttpClient {
    fn request(&self, url: &str) -> Result<String, String>;
}

#[automock]
pub trait Sleeper {
    fn sleep_ms(&self, ms: u64);
}
```

**Tareas**:

**a)** Implementa `RetryClient::get(url)` que reintenta hasta 3 veces con backoff exponencial (100ms, 200ms, 400ms).

**b)** Escribe tests para:
- Éxito en el primer intento (sleep NUNCA se llama)
- Fallo en 1er intento, éxito en 2do (sleep se llama 1 vez con 100ms)
- Fallo 3 veces → retorna el último error (sleep se llama 3 veces)
- Verificar los valores de backoff exactos con `withf` (100, 200, 400)
- Verificar el orden con `Sequence`: request → sleep → request → sleep → ...

---

### Ejercicio 4: Mock con múltiples traits (mock!)

**Objetivo**: Practicar `mock!` para combinar traits y mockear traits de crates externos.

**Tareas**:

**a)** Define 3 traits: `Readable`, `Writable`, `Closeable`. Usa `mock!` para crear un `MockFileHandle` que implemente los 3.

**b)** Implementa un `FileCopier` que lee de un `Readable`, escribe en un `Writable`, y cierra ambos al final.

**c)** Escribe tests que verifiquen:
- La copia funciona correctamente
- Close se llama en ambos handles (source y destination)
- Si read falla, write NUNCA se llama
- Si write falla, ambos se cierran de todas formas (cleanup)

**d)** Usa `mock!` para mockear `std::io::Read` y `std::io::Write`. Escribe un test que verifique una función que lee de `Read` y escribe en `Write`.

---

## Navegación

- **Anterior**: [T01 - Trait-based dependency injection](../T01_Trait_based_DI/README.md)
- **Siguiente**: [T03 - Cuándo no mockear](../T03_Cuando_no_mockear/README.md)

---

> **Sección 4: Mocking en Rust** — Tópico 2 de 4 completado
>
> - T01: Trait-based dependency injection ✓
> - T02: mockall crate — #[automock], expect_*, returning, times, sequences ✓
> - T03: Cuándo no mockear (siguiente)
> - T04: Test doubles sin crates externos
