# T04 - Test doubles sin crates externos: el poder del Rust puro

## Índice

1. [Por qué evitar dependencias externas para testing](#1-por-qué-evitar-dependencias-externas-para-testing)
2. [Las herramientas del Rust estándar](#2-las-herramientas-del-rust-estándar)
3. [Struct con campos configurables: el patrón fundamental](#3-struct-con-campos-configurables-el-patrón-fundamental)
4. [Closures como stubs: comportamiento inyectable](#4-closures-como-stubs-comportamiento-inyectable)
5. [Box\<dyn Fn\> para stubs dinámicos](#5-boxdyn-fn-para-stubs-dinámicos)
6. [Closures con captura: spy pattern](#6-closures-con-captura-spy-pattern)
7. [RefCell para estado mutable en test doubles](#7-refcell-para-estado-mutable-en-test-doubles)
8. [Rc\<RefCell\> para estado compartido](#8-rcrefcell-para-estado-compartido)
9. [Arc\<Mutex\> para tests async y multi-thread](#9-arcmutex-para-tests-async-y-multi-thread)
10. [Generics + Default para doubles configurables](#10-generics--default-para-doubles-configurables)
11. [Enum-based doubles: múltiples comportamientos](#11-enum-based-doubles-múltiples-comportamientos)
12. [PhantomData y marker types para type-safe doubles](#12-phantomdata-y-marker-types-para-type-safe-doubles)
13. [Patrón Builder para construir doubles](#13-patrón-builder-para-construir-doubles)
14. [Stub que falla N veces y luego tiene éxito](#14-stub-que-falla-n-veces-y-luego-tiene-éxito)
15. [Stub con respuestas secuenciales](#15-stub-con-respuestas-secuenciales)
16. [Fake con invariantes verificables](#16-fake-con-invariantes-verificables)
17. [Mock manual con verificación en Drop](#17-mock-manual-con-verificación-en-drop)
18. [Patrón: test double genérico reutilizable](#18-patrón-test-double-genérico-reutilizable)
19. [Combinando técnicas: el toolkit completo](#19-combinando-técnicas-el-toolkit-completo)
20. [Comparación: manual vs mockall](#20-comparación-manual-vs-mockall)
21. [Comparación con C y Go](#21-comparación-con-c-y-go)
22. [Errores comunes](#22-errores-comunes)
23. [Ejemplo completo: sistema de logging distribuido](#23-ejemplo-completo-sistema-de-logging-distribuido)
24. [Programa de práctica](#24-programa-de-práctica)
25. [Ejercicios](#25-ejercicios)

---

## 1. Por qué evitar dependencias externas para testing

mockall es excelente, pero tiene costos:

```
┌─────────────────────────────────────────────────────────────────┐
│ Costos de mockall (o cualquier crate de mocking):               │
│                                                                 │
│ 1. Tiempo de compilación: proc macros son lentas                │
│    - mockall añade ~5-15s a la primera compilación              │
│    - Cada #[automock] genera código significativo               │
│                                                                 │
│ 2. Dependencia transitiva: mockall trae                         │
│    - mockall (proc macro)                                       │
│    - cfg-if, predicates, predicates-core, predicates-tree       │
│    - downcast, fragile                                          │
│    ≈ 10+ crates adicionales en el dependency tree               │
│                                                                 │
│ 3. Errores de compilación crípticos                             │
│    - Los errores de proc macros son difíciles de interpretar    │
│    - "expected X, found MockFoo::expect_bar::Expectation"       │
│                                                                 │
│ 4. Acoplamiento al framework                                    │
│    - La API de mockall cambia entre versiones                   │
│    - Actualizar de 0.11 a 0.12 requirió cambios en tests       │
│                                                                 │
│ 5. Restricciones                                                │
│    - Algunos patterns (métodos genéricos) no se soportan        │
│    - Lifetimes en retornos requieren workarounds                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Cuándo el Rust puro es suficiente

```
┌────────────────────────────────┬─────────────────────────────────┐
│ Rust puro suficiente           │ mockall justificado             │
├────────────────────────────────┼─────────────────────────────────┤
│ Traits con 1-3 métodos         │ Traits con 5+ métodos           │
│ No necesitas times/sequence    │ Necesitas times(n), Sequence    │
│ No necesitas matchers complejos│ Necesitas predicate composition │
│ El proyecto es pequeño/mediano │ Proyecto grande con muchos mocks│
│ Tiempo de compilación importa  │ La expresividad es más valiosa  │
│ Quieres zero dependencies      │ Ya tienes muchas dependencies   │
│ Aprendizaje (entender qué      │ Productividad (ya sabes qué     │
│ genera mockall por debajo)     │ pasa debajo)                    │
└────────────────────────────────┴─────────────────────────────────┘
```

---

## 2. Las herramientas del Rust estándar

Todo lo que necesitas para test doubles está en `std`:

```
┌──────────────────────────────────────────────────────────────────┐
│                  Toolkit de std para test doubles                │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │    Traits     │  │   Closures   │  │  Generics    │          │
│  │              │  │              │  │              │          │
│  │  trait Foo   │  │  Fn, FnMut,  │  │  <T: Trait>  │          │
│  │  impl for   │  │  FnOnce      │  │  where T:... │          │
│  │  TestStruct  │  │  move ||     │  │              │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   RefCell    │  │  Rc / Arc    │  │  Mutex       │          │
│  │              │  │              │  │              │          │
│  │  Interior    │  │  Shared      │  │  Thread-safe │          │
│  │  mutability  │  │  ownership   │  │  mutability  │          │
│  │  para spies  │  │  test + mock │  │  async tests │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  Box<dyn>    │  │  HashMap     │  │  Vec/VecDeque│          │
│  │              │  │              │  │              │          │
│  │  Dynamic     │  │  Lookup      │  │  Recording   │          │
│  │  dispatch    │  │  tables para │  │  calls para  │          │
│  │  para stubs  │  │  responses   │  │  spies       │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  Cell        │  │  AtomicUsize │  │  Default     │          │
│  │              │  │              │  │              │          │
│  │  Counters    │  │  Thread-safe │  │  Easy setup  │          │
│  │  sin RefCell │  │  counters    │  │  con valores │          │
│  │              │  │              │  │  por defecto │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Struct con campos configurables: el patrón fundamental

El patrón más simple y poderoso: un struct cuyos campos definen el comportamiento del test double.

```rust
// ── El trait ──────────────────────────────────────────────

pub trait UserRepository {
    fn find_by_id(&self, id: u64) -> Result<Option<User>, String>;
    fn save(&self, user: &User) -> Result<u64, String>;
}

#[derive(Debug, Clone, PartialEq)]
pub struct User {
    pub id: u64,
    pub name: String,
    pub email: String,
}

// ── Test double con campos configurables ──────────────────

pub struct StubUserRepo {
    pub find_result: Option<User>,
    pub save_result: Result<u64, String>,
}

impl StubUserRepo {
    /// Stub que retorna "no encontrado" para find y Ok(1) para save
    pub fn default() -> Self {
        StubUserRepo {
            find_result: None,
            save_result: Ok(1),
        }
    }

    /// Stub con un usuario predefinido
    pub fn with_user(user: User) -> Self {
        StubUserRepo {
            find_result: Some(user),
            save_result: Ok(1),
        }
    }

    /// Stub donde save falla
    pub fn save_fails(error: &str) -> Self {
        StubUserRepo {
            find_result: None,
            save_result: Err(error.to_string()),
        }
    }
}

impl UserRepository for StubUserRepo {
    fn find_by_id(&self, _id: u64) -> Result<Option<User>, String> {
        Ok(self.find_result.clone())
    }

    fn save(&self, _user: &User) -> Result<u64, String> {
        self.save_result.clone()
    }
}

// ── Uso en tests ──────────────────────────────────────────

pub struct UserService<R: UserRepository> {
    repo: R,
}

impl<R: UserRepository> UserService<R> {
    pub fn new(repo: R) -> Self { UserService { repo } }

    pub fn get_display_name(&self, id: u64) -> String {
        match self.repo.find_by_id(id) {
            Ok(Some(user)) => user.name,
            _ => "Unknown".to_string(),
        }
    }

    pub fn register(&self, name: &str, email: &str) -> Result<u64, String> {
        let user = User { id: 0, name: name.into(), email: email.into() };
        self.repo.save(&user)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_name_when_user_exists() {
        let repo = StubUserRepo::with_user(User {
            id: 42,
            name: "Alice".into(),
            email: "alice@test.com".into(),
        });
        let service = UserService::new(repo);
        assert_eq!(service.get_display_name(42), "Alice");
    }

    #[test]
    fn display_name_when_user_not_found() {
        let repo = StubUserRepo::default(); // find_result: None
        let service = UserService::new(repo);
        assert_eq!(service.get_display_name(999), "Unknown");
    }

    #[test]
    fn register_propagates_save_error() {
        let repo = StubUserRepo::save_fails("connection refused");
        let service = UserService::new(repo);
        let result = service.register("Bob", "bob@test.com");
        assert_eq!(result, Err("connection refused".into()));
    }
}
```

### Anatomía del patrón

```
┌──────────────────────────────────────────────────────────┐
│  StubUserRepo                                            │
│  ┌──────────────┐  ┌──────────────┐                    │
│  │ find_result: │  │ save_result: │                    │
│  │ Option<User> │  │ Result<u64,  │                    │
│  │              │  │ String>      │                    │
│  └──────┬───────┘  └──────┬───────┘                    │
│         │                 │                             │
│  impl UserRepository:     │                             │
│  fn find_by_id → retorna find_result.clone()            │
│  fn save       → retorna save_result.clone()            │
│                                                          │
│  Constructores con nombre semántico:                     │
│  ::default()     → no encontrado, save ok               │
│  ::with_user(u)  → usuario encontrado, save ok          │
│  ::save_fails(e) → no encontrado, save falla            │
│                                                          │
│  Ventajas:                                               │
│  - 0 dependencias externas                               │
│  - Los campos son pub → configurable por test            │
│  - Los constructores con nombre documentan la intención  │
│  - Trivial de leer y debuggear                           │
└──────────────────────────────────────────────────────────┘
```

### Limitación: el resultado es fijo

Este patrón retorna siempre el **mismo** resultado independientemente del argumento. `find_by_id(1)` y `find_by_id(999)` retornan lo mismo. Para comportamiento dinámico basado en argumentos, usamos closures (siguiente sección).

---

## 4. Closures como stubs: comportamiento inyectable

En lugar de campos fijos, inyectar closures permite que el comportamiento dependa de los argumentos:

```rust
// ── Stub con closures como campos ─────────────────────────

pub struct ConfigurableUserRepo {
    find_handler: Box<dyn Fn(u64) -> Result<Option<User>, String>>,
    save_handler: Box<dyn Fn(&User) -> Result<u64, String>>,
}

impl ConfigurableUserRepo {
    pub fn new<F, S>(find_handler: F, save_handler: S) -> Self
    where
        F: Fn(u64) -> Result<Option<User>, String> + 'static,
        S: Fn(&User) -> Result<u64, String> + 'static,
    {
        ConfigurableUserRepo {
            find_handler: Box::new(find_handler),
            save_handler: Box::new(save_handler),
        }
    }
}

impl UserRepository for ConfigurableUserRepo {
    fn find_by_id(&self, id: u64) -> Result<Option<User>, String> {
        (self.find_handler)(id)
    }

    fn save(&self, user: &User) -> Result<u64, String> {
        (self.save_handler)(user)
    }
}

// ── Uso: comportamiento dinámico por argumento ────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn find_different_users_by_id() {
        let repo = ConfigurableUserRepo::new(
            // find: retorna diferentes usuarios según el ID
            |id| match id {
                1 => Ok(Some(User { id: 1, name: "Alice".into(), email: "a@t.com".into() })),
                2 => Ok(Some(User { id: 2, name: "Bob".into(), email: "b@t.com".into() })),
                _ => Ok(None),
            },
            // save: siempre ok
            |_| Ok(1),
        );

        let service = UserService::new(repo);
        assert_eq!(service.get_display_name(1), "Alice");
        assert_eq!(service.get_display_name(2), "Bob");
        assert_eq!(service.get_display_name(99), "Unknown");
    }

    #[test]
    fn save_validates_email() {
        let repo = ConfigurableUserRepo::new(
            |_| Ok(None),
            |user| {
                if user.email.contains('@') {
                    Ok(42)
                } else {
                    Err("invalid email in stub".into())
                }
            },
        );

        let service = UserService::new(repo);
        assert!(service.register("Alice", "alice@test.com").is_ok());
        assert!(service.register("Bob", "invalid-email").is_err());
    }
}
```

---

## 5. Box\<dyn Fn\> para stubs dinámicos

El patrón más flexible: cada método del trait es un `Box<dyn Fn>`:

```rust
pub trait PaymentGateway {
    fn charge(&self, user_id: u64, amount_cents: u64) -> Result<String, String>;
    fn refund(&self, transaction_id: &str) -> Result<(), String>;
    fn status(&self, transaction_id: &str) -> Result<String, String>;
}

/// Stub totalmente configurable sin crates externos
pub struct StubPaymentGateway {
    charge_fn: Box<dyn Fn(u64, u64) -> Result<String, String>>,
    refund_fn: Box<dyn Fn(&str) -> Result<(), String>>,
    status_fn: Box<dyn Fn(&str) -> Result<String, String>>,
}

impl StubPaymentGateway {
    /// Todos los métodos tienen éxito por defecto
    pub fn always_ok() -> Self {
        StubPaymentGateway {
            charge_fn: Box::new(|_, _| Ok("tx-default".into())),
            refund_fn: Box::new(|_| Ok(())),
            status_fn: Box::new(|_| Ok("completed".into())),
        }
    }

    /// Builder: configurar charge
    pub fn with_charge<F>(mut self, f: F) -> Self
    where
        F: Fn(u64, u64) -> Result<String, String> + 'static,
    {
        self.charge_fn = Box::new(f);
        self
    }

    /// Builder: configurar refund
    pub fn with_refund<F>(mut self, f: F) -> Self
    where
        F: Fn(&str) -> Result<(), String> + 'static,
    {
        self.refund_fn = Box::new(f);
        self
    }

    /// Builder: configurar status
    pub fn with_status<F>(mut self, f: F) -> Self
    where
        F: Fn(&str) -> Result<String, String> + 'static,
    {
        self.status_fn = Box::new(f);
        self
    }
}

impl PaymentGateway for StubPaymentGateway {
    fn charge(&self, user_id: u64, amount_cents: u64) -> Result<String, String> {
        (self.charge_fn)(user_id, amount_cents)
    }

    fn refund(&self, transaction_id: &str) -> Result<(), String> {
        (self.refund_fn)(transaction_id)
    }

    fn status(&self, transaction_id: &str) -> Result<String, String> {
        (self.status_fn)(transaction_id)
    }
}

// ── Tests con el builder ──────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn charge_with_dynamic_transaction_id() {
        let gateway = StubPaymentGateway::always_ok()
            .with_charge(|user_id, amount| {
                Ok(format!("tx-{}-{}", user_id, amount))
            });

        assert_eq!(
            gateway.charge(42, 9999).unwrap(),
            "tx-42-9999"
        );
    }

    #[test]
    fn charge_fails_for_high_amounts() {
        let gateway = StubPaymentGateway::always_ok()
            .with_charge(|_, amount| {
                if amount > 100_000 {
                    Err("amount exceeds limit".into())
                } else {
                    Ok("tx-ok".into())
                }
            });

        assert!(gateway.charge(1, 50_000).is_ok());
        assert!(gateway.charge(1, 200_000).is_err());
    }

    #[test]
    fn refund_fails_for_unknown_transactions() {
        let gateway = StubPaymentGateway::always_ok()
            .with_refund(|tx_id| {
                if tx_id.starts_with("tx-") {
                    Ok(())
                } else {
                    Err(format!("unknown transaction: {}", tx_id))
                }
            });

        assert!(gateway.refund("tx-123").is_ok());
        assert!(gateway.refund("invalid").is_err());
    }
}
```

### Patrón visual

```
StubPaymentGateway::always_ok()     ← defaults razonables
    .with_charge(|..| ...)           ← override solo lo que necesitas
    .with_refund(|..| ...)           ← encadenamiento tipo builder

  Resultado:
  ┌────────────────────────────┐
  │ charge_fn: [custom]        │  ← overridden
  │ refund_fn: [custom]        │  ← overridden
  │ status_fn: [default ok]    │  ← no tocado
  └────────────────────────────┘
```

---

## 6. Closures con captura: spy pattern

Un **spy** registra las llamadas para inspección posterior. Con closures y captura, esto es natural en Rust:

```rust
use std::cell::RefCell;
use std::rc::Rc;

pub trait EmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String>;
}

// ── Spy: registra todas las llamadas ──────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct EmailCall {
    pub to: String,
    pub subject: String,
    pub body: String,
}

pub struct SpyEmailSender {
    calls: RefCell<Vec<EmailCall>>,
    should_fail: bool,
}

impl SpyEmailSender {
    pub fn new() -> Self {
        SpyEmailSender {
            calls: RefCell::new(Vec::new()),
            should_fail: false,
        }
    }

    pub fn failing() -> Self {
        SpyEmailSender {
            calls: RefCell::new(Vec::new()),
            should_fail: true,
        }
    }

    /// Inspeccionar las llamadas registradas
    pub fn calls(&self) -> Vec<EmailCall> {
        self.calls.borrow().clone()
    }

    pub fn call_count(&self) -> usize {
        self.calls.borrow().len()
    }

    pub fn was_called(&self) -> bool {
        !self.calls.borrow().is_empty()
    }

    pub fn was_called_with_to(&self, email: &str) -> bool {
        self.calls.borrow().iter().any(|c| c.to == email)
    }

    pub fn last_call(&self) -> Option<EmailCall> {
        self.calls.borrow().last().cloned()
    }
}

impl EmailSender for SpyEmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        self.calls.borrow_mut().push(EmailCall {
            to: to.into(),
            subject: subject.into(),
            body: body.into(),
        });

        if self.should_fail {
            Err("SMTP connection failed".into())
        } else {
            Ok(())
        }
    }
}

// ── Tests con el spy ──────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registration_sends_welcome_email() {
        let spy = SpyEmailSender::new();
        let service = RegistrationService::new(spy);

        service.register("alice@test.com", "Alice").unwrap();

        let spy = &service.email_sender; // acceder al spy
        assert_eq!(spy.call_count(), 1);
        assert!(spy.was_called_with_to("alice@test.com"));

        let call = spy.last_call().unwrap();
        assert!(call.subject.contains("Welcome"));
    }

    #[test]
    fn failed_email_does_not_block_registration() {
        let spy = SpyEmailSender::failing();
        let service = RegistrationService::new(spy);

        // El registro debe completarse aunque el email falle
        let result = service.register("bob@test.com", "Bob");
        assert!(result.is_ok());

        // Pero el intento de enviar se registró
        assert!(service.email_sender.was_called());
    }
}
```

### Spy con closure genérica

Un patrón más flexible: la closure captura un `Rc<RefCell<Vec<...>>>` que el test también tiene:

```rust
use std::cell::RefCell;
use std::rc::Rc;

#[test]
fn spy_with_shared_vec() {
    let calls: Rc<RefCell<Vec<String>>> = Rc::new(RefCell::new(Vec::new()));
    let calls_clone = calls.clone();

    let repo = ConfigurableUserRepo::new(
        |_| Ok(None),
        move |user| {
            calls_clone.borrow_mut().push(format!("save:{}", user.email));
            Ok(1)
        },
    );

    let service = UserService::new(repo);
    service.register("a@t.com", "A").unwrap();
    service.register("b@t.com", "B").unwrap();

    let recorded = calls.borrow();
    assert_eq!(recorded.len(), 2);
    assert_eq!(recorded[0], "save:a@t.com");
    assert_eq!(recorded[1], "save:b@t.com");
}
```

```
Flujo de datos del spy con Rc<RefCell<>>:

  Test                          Closure dentro del stub
  ────                          ───────────────────────
  let calls = Rc<RefCell<Vec>>
       │
       ├── calls.clone() ─────► move |user| {
       │                            calls_clone.borrow_mut().push(...)
       │                        }
       │                             │
       │                             │ (cada llamada al stub)
       │                             ▼
       │                        Vec crece: ["save:a@t.com", "save:b@t.com"]
       │                             ▲
       └─── calls.borrow() ─────────┘
             (inspección al final del test)
```

---

## 7. RefCell para estado mutable en test doubles

`RefCell` proporciona interior mutability: modificar datos dentro de una referencia `&self`. Esencial porque la mayoría de los traits tienen métodos que toman `&self`, no `&mut self`.

```rust
use std::cell::RefCell;
use std::collections::HashMap;

pub trait Cache {
    fn get(&self, key: &str) -> Option<String>;
    fn set(&self, key: &str, value: &str);  // &self, no &mut self
    fn invalidate(&self, key: &str);
}

/// Fake cache con RefCell para mutabilidad interior
pub struct InMemoryCache {
    data: RefCell<HashMap<String, String>>,
}

impl InMemoryCache {
    pub fn new() -> Self {
        InMemoryCache {
            data: RefCell::new(HashMap::new()),
        }
    }

    pub fn with_entries(entries: Vec<(&str, &str)>) -> Self {
        let cache = Self::new();
        for (k, v) in entries {
            cache.set(k, v);
        }
        cache
    }

    /// Inspeccionar el estado interno (para tests)
    pub fn len(&self) -> usize {
        self.data.borrow().len()
    }

    pub fn contains(&self, key: &str) -> bool {
        self.data.borrow().contains_key(key)
    }

    pub fn snapshot(&self) -> HashMap<String, String> {
        self.data.borrow().clone()
    }
}

impl Cache for InMemoryCache {
    fn get(&self, key: &str) -> Option<String> {
        self.data.borrow().get(key).cloned()
    }

    fn set(&self, key: &str, value: &str) {
        self.data.borrow_mut().insert(key.to_string(), value.to_string());
    }

    fn invalidate(&self, key: &str) {
        self.data.borrow_mut().remove(key);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cache_stores_and_retrieves() {
        let cache = InMemoryCache::new();
        cache.set("key1", "value1");
        assert_eq!(cache.get("key1"), Some("value1".to_string()));
    }

    #[test]
    fn cache_invalidation() {
        let cache = InMemoryCache::with_entries(vec![("a", "1"), ("b", "2")]);
        assert_eq!(cache.len(), 2);

        cache.invalidate("a");
        assert_eq!(cache.len(), 1);
        assert!(cache.get("a").is_none());
        assert!(cache.get("b").is_some());
    }
}
```

### RefCell: reglas de seguridad

```
┌──────────────────────────────────────────────────────────────┐
│  RefCell<T> — runtime borrow checking                        │
│                                                              │
│  .borrow()     → devuelve Ref<T> (lectura)                  │
│  .borrow_mut() → devuelve RefMut<T> (escritura)             │
│                                                              │
│  Reglas (verificadas en runtime, panic si se violan):        │
│  ✓ Múltiples .borrow() simultáneos → ok                     │
│  ✓ Un solo .borrow_mut() sin .borrow() → ok                 │
│  ✗ .borrow() + .borrow_mut() simultáneos → PANIC            │
│  ✗ Múltiples .borrow_mut() simultáneos → PANIC              │
│                                                              │
│  En la práctica, raro que cause problemas en tests porque    │
│  cada método del trait toma prestado y suelta antes de que   │
│  el siguiente método se llame.                               │
│                                                              │
│  Patrón seguro:                                              │
│  fn get(&self, key: &str) -> Option<String> {                │
│      self.data.borrow().get(key).cloned() // borrow se suelta│
│  }                                                            │
│  fn set(&self, key: &str, value: &str) {                     │
│      self.data.borrow_mut().insert(...);  // borrow_mut se   │
│  }                                         // suelta aquí     │
└──────────────────────────────────────────────────────────────┘
```

### Cell para contadores simples

Si solo necesitas un contador (no una colección), `Cell` es más simple que `RefCell`:

```rust
use std::cell::Cell;

pub struct CountingLogger {
    info_count: Cell<u32>,
    warn_count: Cell<u32>,
    error_count: Cell<u32>,
}

impl CountingLogger {
    pub fn new() -> Self {
        CountingLogger {
            info_count: Cell::new(0),
            warn_count: Cell::new(0),
            error_count: Cell::new(0),
        }
    }

    pub fn info_count(&self) -> u32 { self.info_count.get() }
    pub fn warn_count(&self) -> u32 { self.warn_count.get() }
    pub fn error_count(&self) -> u32 { self.error_count.get() }
    pub fn total(&self) -> u32 {
        self.info_count.get() + self.warn_count.get() + self.error_count.get()
    }
}

impl Logger for CountingLogger {
    fn info(&self, _msg: &str) {
        self.info_count.set(self.info_count.get() + 1);
    }
    fn warn(&self, _msg: &str) {
        self.warn_count.set(self.warn_count.get() + 1);
    }
    fn error(&self, _msg: &str) {
        self.error_count.set(self.error_count.get() + 1);
    }
}

#[test]
fn test_error_handling_logs_correctly() {
    let logger = CountingLogger::new();
    let service = MyService::new(&logger);

    service.process_invalid_data("bad input");

    assert_eq!(logger.error_count(), 1);
    assert_eq!(logger.info_count(), 0);
}
```

---

## 8. Rc\<RefCell\> para estado compartido

Cuando el test y el stub necesitan acceder al mismo estado mutable:

```rust
use std::cell::RefCell;
use std::rc::Rc;

pub trait EventBus {
    fn publish(&self, event: &str) -> Result<(), String>;
    fn subscribe(&self, pattern: &str) -> Result<(), String>;
}

/// Estado compartido entre test y stub
pub struct SharedEventLog {
    inner: Rc<RefCell<EventLogInner>>,
}

struct EventLogInner {
    published: Vec<String>,
    subscriptions: Vec<String>,
    should_fail: bool,
}

impl SharedEventLog {
    pub fn new() -> Self {
        SharedEventLog {
            inner: Rc::new(RefCell::new(EventLogInner {
                published: Vec::new(),
                subscriptions: Vec::new(),
                should_fail: false,
            })),
        }
    }

    pub fn failing() -> Self {
        SharedEventLog {
            inner: Rc::new(RefCell::new(EventLogInner {
                published: Vec::new(),
                subscriptions: Vec::new(),
                should_fail: true,
            })),
        }
    }

    /// Clonar el handle (compartir ownership)
    pub fn handle(&self) -> Self {
        SharedEventLog {
            inner: self.inner.clone(),
        }
    }

    /// Inspección
    pub fn published_events(&self) -> Vec<String> {
        self.inner.borrow().published.clone()
    }

    pub fn subscription_count(&self) -> usize {
        self.inner.borrow().subscriptions.len()
    }
}

impl EventBus for SharedEventLog {
    fn publish(&self, event: &str) -> Result<(), String> {
        let mut inner = self.inner.borrow_mut();
        if inner.should_fail {
            return Err("bus unavailable".into());
        }
        inner.published.push(event.to_string());
        Ok(())
    }

    fn subscribe(&self, pattern: &str) -> Result<(), String> {
        self.inner.borrow_mut().subscriptions.push(pattern.to_string());
        Ok(())
    }
}

// ── Uso: el test inspecciona lo que el servicio publicó ───

pub struct OrderProcessor<E: EventBus> {
    bus: E,
}

impl<E: EventBus> OrderProcessor<E> {
    pub fn new(bus: E) -> Self { OrderProcessor { bus } }

    pub fn process_order(&self, order_id: u64) -> Result<(), String> {
        // ... lógica ...
        self.bus.publish(&format!("order.created:{}", order_id))?;
        self.bus.publish(&format!("order.payment_pending:{}", order_id))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn order_processing_publishes_events() {
        let log = SharedEventLog::new();
        let log_handle = log.handle(); // mismo estado compartido

        let processor = OrderProcessor::new(log);
        processor.process_order(42).unwrap();

        // Inspeccionar desde el handle
        let events = log_handle.published_events();
        assert_eq!(events.len(), 2);
        assert_eq!(events[0], "order.created:42");
        assert_eq!(events[1], "order.payment_pending:42");
    }
}
```

```
Patrón Rc<RefCell<>>:

  Test code              SharedEventLog            Service code
  ─────────              ──────────────            ────────────
  let log = new()
       │
       ├── log.handle() ──► log_handle
       │                       │
       │   Rc::clone ──────────┤
       │                       │
       └── processor = new(log)│
                │              │
                │ .process()   │
                │   │          │
                │   ├─ bus.publish("order.created:42")
                │   │    └─► inner.published.push(...)
                │   │                  │
                │   └─ bus.publish("order.payment_pending:42")
                │        └─► inner.published.push(...)
                │                      │
                │                      │
       log_handle.published_events() ──┘
         │
         └─► ["order.created:42", "order.payment_pending:42"]
```

---

## 9. Arc\<Mutex\> para tests async y multi-thread

`Rc<RefCell>` no es `Send` ni `Sync`, así que no funciona en tests async o multi-thread. Para esos escenarios, usa `Arc<Mutex>`:

```rust
use std::sync::{Arc, Mutex};

pub trait MetricsCollector: Send + Sync {
    fn increment(&self, name: &str);
    fn gauge(&self, name: &str, value: f64);
}

pub struct InMemoryMetrics {
    counters: Arc<Mutex<HashMap<String, u64>>>,
    gauges: Arc<Mutex<HashMap<String, f64>>>,
}

impl InMemoryMetrics {
    pub fn new() -> Self {
        InMemoryMetrics {
            counters: Arc::new(Mutex::new(HashMap::new())),
            gauges: Arc::new(Mutex::new(HashMap::new())),
        }
    }

    /// Clonar para compartir entre test y servicio
    pub fn handle(&self) -> Self {
        InMemoryMetrics {
            counters: self.counters.clone(),
            gauges: self.gauges.clone(),
        }
    }

    pub fn counter(&self, name: &str) -> u64 {
        *self.counters.lock().unwrap().get(name).unwrap_or(&0)
    }

    pub fn gauge_value(&self, name: &str) -> Option<f64> {
        self.gauges.lock().unwrap().get(name).copied()
    }
}

impl MetricsCollector for InMemoryMetrics {
    fn increment(&self, name: &str) {
        *self.counters.lock().unwrap().entry(name.to_string()).or_insert(0) += 1;
    }

    fn gauge(&self, name: &str, value: f64) {
        self.gauges.lock().unwrap().insert(name.to_string(), value);
    }
}

use std::collections::HashMap;

// ── Test async con tokio ──────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn async_service_emits_metrics() {
        let metrics = InMemoryMetrics::new();
        let metrics_handle = metrics.handle();

        let service = AsyncService::new(metrics);
        service.process().await;

        assert_eq!(metrics_handle.counter("requests.processed"), 1);
    }

    #[tokio::test]
    async fn concurrent_access() {
        let metrics = InMemoryMetrics::new();
        let handle = metrics.handle();

        // Múltiples tasks concurrentes
        let mut tasks = Vec::new();
        for i in 0..10 {
            let m = metrics.handle();
            tasks.push(tokio::spawn(async move {
                m.increment("concurrent.counter");
            }));
        }

        for task in tasks {
            task.await.unwrap();
        }

        assert_eq!(handle.counter("concurrent.counter"), 10);
    }
}
```

### Tabla de decisión: RefCell vs Mutex

```
┌────────────────────┬──────────────────────────┬──────────────────────────┐
│ Criterio           │ RefCell                  │ Mutex                    │
├────────────────────┼──────────────────────────┼──────────────────────────┤
│ Thread safety      │ ✗ (single-thread only)   │ ✓ (multi-thread safe)    │
│ Send + Sync        │ ✗                        │ ✓                        │
│ async compatible   │ ✗                        │ ✓                        │
│ Performance        │ Más rápido (sin lock)    │ Más lento (lock/unlock)  │
│ Panic on misuse    │ Sí (double borrow)       │ Sí (poisoned mutex)      │
│ Shared ownership   │ Rc<RefCell<T>>           │ Arc<Mutex<T>>            │
│ Overhead           │ Mínimo                   │ Bajo                     │
│ Usar cuando        │ Tests síncronos          │ Tests async/multi-thread │
│                    │ #[test]                  │ #[tokio::test]           │
└────────────────────┴──────────────────────────┴──────────────────────────┘
```

---

## 10. Generics + Default para doubles configurables

El trait `Default` permite crear test doubles con configuración mínima:

```rust
pub trait Validator {
    fn validate(&self, input: &str) -> Result<(), Vec<String>>;
}

pub trait Sanitizer {
    fn sanitize(&self, input: &str) -> String;
}

pub trait Persister {
    fn persist(&self, data: &str) -> Result<u64, String>;
}

// ── Test doubles con Default ──────────────────────────────

pub struct StubValidator {
    pub result: Result<(), Vec<String>>,
}

impl Default for StubValidator {
    fn default() -> Self {
        StubValidator { result: Ok(()) } // por defecto: siempre válido
    }
}

impl Validator for StubValidator {
    fn validate(&self, _input: &str) -> Result<(), Vec<String>> {
        self.result.clone()
    }
}

pub struct StubSanitizer;

impl Default for StubSanitizer {
    fn default() -> Self { StubSanitizer }
}

impl Sanitizer for StubSanitizer {
    fn sanitize(&self, input: &str) -> String {
        input.to_string() // por defecto: no sanitiza
    }
}

pub struct StubPersister {
    pub result: Result<u64, String>,
}

impl Default for StubPersister {
    fn default() -> Self {
        StubPersister { result: Ok(1) } // por defecto: siempre Ok(1)
    }
}

impl Persister for StubPersister {
    fn persist(&self, _data: &str) -> Result<u64, String> {
        self.result.clone()
    }
}

// ── Servicio genérico ─────────────────────────────────────

pub struct Pipeline<V: Validator, S: Sanitizer, P: Persister> {
    validator: V,
    sanitizer: S,
    persister: P,
}

impl<V: Validator, S: Sanitizer, P: Persister> Pipeline<V, S, P> {
    pub fn new(validator: V, sanitizer: S, persister: P) -> Self {
        Pipeline { validator, sanitizer, persister }
    }

    pub fn process(&self, input: &str) -> Result<u64, String> {
        self.validator.validate(input)
            .map_err(|errors| errors.join(", "))?;
        let clean = self.sanitizer.sanitize(input);
        self.persister.persist(&clean)
    }
}

// ── Tests: solo configurar lo relevante, rest = default ───

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn valid_input_is_persisted() {
        // Solo necesitamos defaults: todo funciona
        let pipeline = Pipeline::new(
            StubValidator::default(),
            StubSanitizer::default(),
            StubPersister::default(),
        );
        assert_eq!(pipeline.process("hello").unwrap(), 1);
    }

    #[test]
    fn invalid_input_returns_errors() {
        // Solo personalizar el validator
        let pipeline = Pipeline::new(
            StubValidator { result: Err(vec!["too short".into(), "no digits".into()]) },
            StubSanitizer::default(),
            StubPersister::default(),
        );

        let err = pipeline.process("x").unwrap_err();
        assert!(err.contains("too short"));
        assert!(err.contains("no digits"));
    }

    #[test]
    fn persist_failure_propagates() {
        // Solo personalizar el persister
        let pipeline = Pipeline::new(
            StubValidator::default(),
            StubSanitizer::default(),
            StubPersister { result: Err("disk full".into()) },
        );

        assert_eq!(pipeline.process("hello").unwrap_err(), "disk full");
    }
}
```

### Ventaja del Default

```
Sin Default:
  Pipeline::new(
      StubValidator { result: Ok(()) },       // boilerplate
      StubSanitizer,                           // boilerplate
      StubPersister { result: Ok(1) },         // boilerplate
  );

Con Default:
  Pipeline::new(
      StubValidator::default(),                // 1 palabra
      StubSanitizer::default(),                // 1 palabra
      StubPersister { result: Err("fail") },   // solo lo relevante
  );
```

---

## 11. Enum-based doubles: múltiples comportamientos

Cuando un test double necesita alternar entre varios comportamientos predefinidos:

```rust
pub trait HttpClient {
    fn get(&self, url: &str) -> Result<String, String>;
}

/// Enum que modela diferentes comportamientos del HTTP client
pub enum StubHttp {
    /// Siempre retorna la misma respuesta
    AlwaysOk(String),

    /// Siempre falla con el error dado
    AlwaysFail(String),

    /// Retorna diferentes respuestas según la URL
    ByUrl(HashMap<String, String>),

    /// Respuestas secuenciales (consume una por llamada)
    Sequential(RefCell<Vec<String>>),

    /// Falla N veces, luego tiene éxito
    FailThenSucceed {
        failures: Cell<u32>,
        max_failures: u32,
        success_response: String,
        error_message: String,
    },
}

use std::cell::{Cell, RefCell};
use std::collections::HashMap;

impl StubHttp {
    pub fn ok(body: &str) -> Self {
        StubHttp::AlwaysOk(body.to_string())
    }

    pub fn fail(error: &str) -> Self {
        StubHttp::AlwaysFail(error.to_string())
    }

    pub fn by_url(routes: Vec<(&str, &str)>) -> Self {
        StubHttp::ByUrl(
            routes.into_iter()
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect()
        )
    }

    pub fn sequential(responses: Vec<&str>) -> Self {
        StubHttp::Sequential(RefCell::new(
            responses.into_iter().rev().map(|s| s.to_string()).collect()
        ))
    }

    pub fn fail_then_succeed(failures: u32, error: &str, success: &str) -> Self {
        StubHttp::FailThenSucceed {
            failures: Cell::new(0),
            max_failures: failures,
            success_response: success.to_string(),
            error_message: error.to_string(),
        }
    }
}

impl HttpClient for StubHttp {
    fn get(&self, url: &str) -> Result<String, String> {
        match self {
            StubHttp::AlwaysOk(body) => Ok(body.clone()),

            StubHttp::AlwaysFail(error) => Err(error.clone()),

            StubHttp::ByUrl(routes) => {
                routes.get(url)
                    .cloned()
                    .ok_or_else(|| format!("404: {}", url))
            }

            StubHttp::Sequential(responses) => {
                responses.borrow_mut().pop()
                    .ok_or_else(|| "no more responses".to_string())
                    .map(Ok)?
            }

            StubHttp::FailThenSucceed {
                failures, max_failures, success_response, error_message
            } => {
                let count = failures.get();
                failures.set(count + 1);
                if count < *max_failures {
                    Err(error_message.clone())
                } else {
                    Ok(success_response.clone())
                }
            }
        }
    }
}

// ── Tests ─────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_always_ok() {
        let client = StubHttp::ok(r#"{"status":"ok"}"#);
        assert!(client.get("https://api.example.com").is_ok());
    }

    #[test]
    fn test_by_url() {
        let client = StubHttp::by_url(vec![
            ("https://api.example.com/users/1", r#"{"name":"Alice"}"#),
            ("https://api.example.com/users/2", r#"{"name":"Bob"}"#),
        ]);

        let alice = client.get("https://api.example.com/users/1").unwrap();
        assert!(alice.contains("Alice"));

        let unknown = client.get("https://api.example.com/users/999");
        assert!(unknown.is_err());
    }

    #[test]
    fn test_sequential_responses() {
        let client = StubHttp::sequential(vec![
            r#"{"page":1}"#,
            r#"{"page":2}"#,
            r#"{"page":3}"#,
        ]);

        assert!(client.get("/page").unwrap().contains("page\":1"));
        assert!(client.get("/page").unwrap().contains("page\":2"));
        assert!(client.get("/page").unwrap().contains("page\":3"));
        assert!(client.get("/page").is_err()); // agotado
    }

    #[test]
    fn test_retry_behavior() {
        let client = StubHttp::fail_then_succeed(
            2, "connection timeout", r#"{"ok":true}"#
        );

        assert!(client.get("/api").is_err());   // fallo 1
        assert!(client.get("/api").is_err());   // fallo 2
        assert!(client.get("/api").is_ok());    // éxito
        assert!(client.get("/api").is_ok());    // sigue ok
    }
}
```

### Ventaja del enum sobre closures

```
Closures:
  - Cada test construye una closure ad-hoc
  - El comportamiento es opaco (no puedes inspeccionarlo)
  - Difícil de reutilizar entre tests

Enum:
  - Comportamientos predefinidos con nombres descriptivos
  - Constructores semánticos: ::fail_then_succeed(2, ...)
  - Fácil de combinar y reutilizar
  - El match en la impl documenta todos los modos posibles
```

---

## 12. PhantomData y marker types para type-safe doubles

Usar el sistema de tipos para evitar errores en los test doubles:

```rust
use std::marker::PhantomData;

// ── Marker types para estados ─────────────────────────────

pub struct NotConfigured;
pub struct Configured;

/// Builder type-safe: no puedes usar el stub sin configurarlo
pub struct StubBuilder<State> {
    find_result: Option<Box<dyn Fn(u64) -> Option<User>>>,
    save_result: Option<Box<dyn Fn(&User) -> Result<u64, String>>>,
    _state: PhantomData<State>,
}

impl StubBuilder<NotConfigured> {
    pub fn new() -> Self {
        StubBuilder {
            find_result: None,
            save_result: None,
            _state: PhantomData,
        }
    }

    pub fn with_find<F>(mut self, f: F) -> Self
    where F: Fn(u64) -> Option<User> + 'static
    {
        self.find_result = Some(Box::new(f));
        self
    }

    pub fn with_save<F>(mut self, f: F) -> Self
    where F: Fn(&User) -> Result<u64, String> + 'static
    {
        self.save_result = Some(Box::new(f));
        self
    }

    /// Solo se puede llamar build() con ambos configurados
    pub fn build(self) -> StubBuilder<Configured> {
        // Validar que ambos están configurados
        assert!(self.find_result.is_some(), "find must be configured");
        assert!(self.save_result.is_some(), "save must be configured");

        StubBuilder {
            find_result: self.find_result,
            save_result: self.save_result,
            _state: PhantomData,
        }
    }
}

// Solo Configured puede usarse como UserRepository
impl UserRepository for StubBuilder<Configured> {
    fn find_by_id(&self, id: u64) -> Result<Option<User>, String> {
        Ok((self.find_result.as_ref().unwrap())(id))
    }

    fn save(&self, user: &User) -> Result<u64, String> {
        (self.save_result.as_ref().unwrap())(user)
    }
}

// ── Uso ───────────────────────────────────────────────────

#[test]
fn test_with_type_safe_builder() {
    let repo = StubBuilder::new()
        .with_find(|id| {
            if id == 1 { Some(User { id: 1, name: "Alice".into(), email: "a@t.com".into() }) }
            else { None }
        })
        .with_save(|_| Ok(42))
        .build(); // ← transición de estado: NotConfigured → Configured

    let service = UserService::new(repo);
    assert_eq!(service.get_display_name(1), "Alice");
}
```

Este patrón es más complejo de lo necesario para la mayoría de cases. Mencionarlo como técnica avanzada disponible cuando la seguridad de tipos en el setup del test importa (ej: doubles con muchas dependencias que son fáciles de configurar incorrectamente).

---

## 13. Patrón Builder para construir doubles

El builder pattern es la forma más ergonómica de construir test doubles complejos:

```rust
pub trait NotificationService {
    fn send_email(&self, to: &str, subject: &str, body: &str) -> Result<(), String>;
    fn send_sms(&self, phone: &str, message: &str) -> Result<(), String>;
    fn send_push(&self, device_id: &str, title: &str, body: &str) -> Result<(), String>;
}

pub struct StubNotifier {
    email_fn: Box<dyn Fn(&str, &str, &str) -> Result<(), String>>,
    sms_fn: Box<dyn Fn(&str, &str) -> Result<(), String>>,
    push_fn: Box<dyn Fn(&str, &str, &str) -> Result<(), String>>,
    calls: RefCell<Vec<NotificationCall>>,
}

#[derive(Debug, Clone)]
pub enum NotificationCall {
    Email { to: String, subject: String },
    Sms { phone: String },
    Push { device_id: String, title: String },
}

use std::cell::RefCell;

impl StubNotifier {
    /// Builder: empieza con todo exitoso
    pub fn builder() -> StubNotifierBuilder {
        StubNotifierBuilder {
            email_fn: Box::new(|_, _, _| Ok(())),
            sms_fn: Box::new(|_, _| Ok(())),
            push_fn: Box::new(|_, _, _| Ok(())),
        }
    }

    /// Acceso a las llamadas registradas
    pub fn calls(&self) -> Vec<NotificationCall> {
        self.calls.borrow().clone()
    }

    pub fn email_calls(&self) -> Vec<NotificationCall> {
        self.calls().into_iter()
            .filter(|c| matches!(c, NotificationCall::Email { .. }))
            .collect()
    }

    pub fn sms_calls(&self) -> Vec<NotificationCall> {
        self.calls().into_iter()
            .filter(|c| matches!(c, NotificationCall::Sms { .. }))
            .collect()
    }
}

pub struct StubNotifierBuilder {
    email_fn: Box<dyn Fn(&str, &str, &str) -> Result<(), String>>,
    sms_fn: Box<dyn Fn(&str, &str) -> Result<(), String>>,
    push_fn: Box<dyn Fn(&str, &str, &str) -> Result<(), String>>,
}

impl StubNotifierBuilder {
    pub fn email_fails(mut self, error: &str) -> Self {
        let err = error.to_string();
        self.email_fn = Box::new(move |_, _, _| Err(err.clone()));
        self
    }

    pub fn sms_fails(mut self, error: &str) -> Self {
        let err = error.to_string();
        self.sms_fn = Box::new(move |_, _| Err(err.clone()));
        self
    }

    pub fn push_fails(mut self, error: &str) -> Self {
        let err = error.to_string();
        self.push_fn = Box::new(move |_, _, _| Err(err.clone()));
        self
    }

    pub fn email_handler<F>(mut self, f: F) -> Self
    where F: Fn(&str, &str, &str) -> Result<(), String> + 'static
    {
        self.email_fn = Box::new(f);
        self
    }

    pub fn build(self) -> StubNotifier {
        StubNotifier {
            email_fn: self.email_fn,
            sms_fn: self.sms_fn,
            push_fn: self.push_fn,
            calls: RefCell::new(Vec::new()),
        }
    }
}

impl NotificationService for StubNotifier {
    fn send_email(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        self.calls.borrow_mut().push(NotificationCall::Email {
            to: to.into(), subject: subject.into(),
        });
        (self.email_fn)(to, subject, body)
    }

    fn send_sms(&self, phone: &str, message: &str) -> Result<(), String> {
        self.calls.borrow_mut().push(NotificationCall::Sms {
            phone: phone.into(),
        });
        (self.sms_fn)(phone, message)
    }

    fn send_push(&self, device_id: &str, title: &str, body: &str) -> Result<(), String> {
        self.calls.borrow_mut().push(NotificationCall::Push {
            device_id: device_id.into(), title: title.into(),
        });
        (self.push_fn)(device_id, title, body)
    }
}

// ── Tests ─────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn all_channels_succeed() {
        let notifier = StubNotifier::builder().build();

        notifier.send_email("a@t.com", "Hi", "Body").unwrap();
        notifier.send_sms("+1234567890", "Hello").unwrap();

        assert_eq!(notifier.calls().len(), 2);
        assert_eq!(notifier.email_calls().len(), 1);
        assert_eq!(notifier.sms_calls().len(), 1);
    }

    #[test]
    fn email_fails_but_sms_works() {
        let notifier = StubNotifier::builder()
            .email_fails("SMTP down")
            .build();

        assert!(notifier.send_email("a@t.com", "Hi", "Body").is_err());
        assert!(notifier.send_sms("+123", "Hello").is_ok());
    }

    #[test]
    fn custom_email_handler() {
        let notifier = StubNotifier::builder()
            .email_handler(|to, _, _| {
                if to.ends_with("@blocked.com") {
                    Err("blocked domain".into())
                } else {
                    Ok(())
                }
            })
            .build();

        assert!(notifier.send_email("a@ok.com", "Hi", "Body").is_ok());
        assert!(notifier.send_email("b@blocked.com", "Hi", "Body").is_err());
    }
}
```

---

## 14. Stub que falla N veces y luego tiene éxito

Patrón esencial para testear lógica de retry:

```rust
use std::cell::Cell;

pub trait Fetcher {
    fn fetch(&self, url: &str) -> Result<String, String>;
}

pub struct FailNThenSucceed {
    failures_remaining: Cell<u32>,
    error_message: String,
    success_response: String,
}

impl FailNThenSucceed {
    pub fn new(n_failures: u32, error: &str, success: &str) -> Self {
        FailNThenSucceed {
            failures_remaining: Cell::new(n_failures),
            error_message: error.to_string(),
            success_response: success.to_string(),
        }
    }
}

impl Fetcher for FailNThenSucceed {
    fn fetch(&self, _url: &str) -> Result<String, String> {
        let remaining = self.failures_remaining.get();
        if remaining > 0 {
            self.failures_remaining.set(remaining - 1);
            Err(self.error_message.clone())
        } else {
            Ok(self.success_response.clone())
        }
    }
}

// ── Retry client que usa el Fetcher ───────────────────────

pub struct RetryClient<F: Fetcher> {
    fetcher: F,
    max_retries: u32,
}

impl<F: Fetcher> RetryClient<F> {
    pub fn new(fetcher: F, max_retries: u32) -> Self {
        RetryClient { fetcher, max_retries }
    }

    pub fn get(&self, url: &str) -> Result<String, String> {
        let mut last_error = String::new();
        for attempt in 0..=self.max_retries {
            match self.fetcher.fetch(url) {
                Ok(response) => return Ok(response),
                Err(e) => {
                    last_error = e;
                    if attempt < self.max_retries {
                        // En producción: sleep con backoff
                        // En tests: el stub controla el comportamiento
                    }
                }
            }
        }
        Err(format!("failed after {} retries: {}", self.max_retries + 1, last_error))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn succeeds_on_first_try() {
        let fetcher = FailNThenSucceed::new(0, "", r#"{"ok":true}"#);
        let client = RetryClient::new(fetcher, 3);
        assert!(client.get("/api").is_ok());
    }

    #[test]
    fn succeeds_after_two_failures() {
        let fetcher = FailNThenSucceed::new(2, "timeout", r#"{"ok":true}"#);
        let client = RetryClient::new(fetcher, 3);

        let result = client.get("/api");
        assert!(result.is_ok());
    }

    #[test]
    fn fails_when_all_retries_exhausted() {
        let fetcher = FailNThenSucceed::new(5, "timeout", r#"{"ok":true}"#);
        let client = RetryClient::new(fetcher, 3); // max 3 retries = 4 attempts

        let result = client.get("/api");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("failed after 4 retries"));
    }

    #[test]
    fn succeeds_on_last_retry() {
        // 3 retries = 4 attempts. Fail 3 times, succeed on 4th.
        let fetcher = FailNThenSucceed::new(3, "timeout", r#"{"ok":true}"#);
        let client = RetryClient::new(fetcher, 3);
        assert!(client.get("/api").is_ok());
    }
}
```

---

## 15. Stub con respuestas secuenciales

Para cuando el orden de las respuestas importa:

```rust
use std::cell::RefCell;

pub struct SequentialStub<T: Clone> {
    responses: RefCell<Vec<T>>,
    exhausted_response: T,
}

impl<T: Clone> SequentialStub<T> {
    pub fn new(responses: Vec<T>, exhausted: T) -> Self {
        // Invertir para usar pop() eficientemente
        let mut reversed = responses;
        reversed.reverse();
        SequentialStub {
            responses: RefCell::new(reversed),
            exhausted_response: exhausted,
        }
    }

    pub fn next(&self) -> T {
        self.responses.borrow_mut().pop()
            .unwrap_or_else(|| self.exhausted_response.clone())
    }

    pub fn remaining(&self) -> usize {
        self.responses.borrow().len()
    }
}

// ── Aplicación: paginator ─────────────────────────────────

pub trait DataSource {
    fn fetch_page(&self, page: u32) -> Result<Vec<String>, String>;
}

pub struct SequentialDataSource {
    pages: SequentialStub<Result<Vec<String>, String>>,
}

impl SequentialDataSource {
    pub fn with_pages(pages: Vec<Vec<&str>>) -> Self {
        let results: Vec<Result<Vec<String>, String>> = pages.into_iter()
            .map(|p| Ok(p.into_iter().map(|s| s.to_string()).collect()))
            .collect();

        SequentialDataSource {
            pages: SequentialStub::new(results, Ok(vec![])),
        }
    }

    pub fn with_error_on_page(mut pages: Vec<Vec<&str>>, error_page: usize, error: &str) -> Self {
        let mut results: Vec<Result<Vec<String>, String>> = pages.into_iter()
            .map(|p| Ok(p.into_iter().map(|s| s.to_string()).collect()))
            .collect();

        if error_page < results.len() {
            results[error_page] = Err(error.to_string());
        }

        SequentialDataSource {
            pages: SequentialStub::new(results, Ok(vec![])),
        }
    }
}

impl DataSource for SequentialDataSource {
    fn fetch_page(&self, _page: u32) -> Result<Vec<String>, String> {
        self.pages.next()
    }
}

pub struct Paginator<D: DataSource> {
    source: D,
}

impl<D: DataSource> Paginator<D> {
    pub fn new(source: D) -> Self { Paginator { source } }

    pub fn fetch_all(&self) -> Result<Vec<String>, String> {
        let mut all = Vec::new();
        let mut page = 0;
        loop {
            let items = self.source.fetch_page(page)?;
            if items.is_empty() { break; }
            all.extend(items);
            page += 1;
        }
        Ok(all)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fetches_all_pages() {
        let source = SequentialDataSource::with_pages(vec![
            vec!["a", "b", "c"],   // page 0
            vec!["d", "e"],         // page 1
            vec!["f"],              // page 2
            // page 3 → empty → stop
        ]);

        let paginator = Paginator::new(source);
        let all = paginator.fetch_all().unwrap();
        assert_eq!(all, vec!["a", "b", "c", "d", "e", "f"]);
    }

    #[test]
    fn handles_empty_first_page() {
        let source = SequentialDataSource::with_pages(vec![]);
        let paginator = Paginator::new(source);
        assert_eq!(paginator.fetch_all().unwrap(), Vec::<String>::new());
    }

    #[test]
    fn error_on_page_propagates() {
        let source = SequentialDataSource::with_error_on_page(
            vec![vec!["a", "b"], vec!["c"]],
            1,
            "connection lost",
        );

        let paginator = Paginator::new(source);
        let result = paginator.fetch_all();
        assert_eq!(result, Err("connection lost".to_string()));
    }
}
```

---

## 16. Fake con invariantes verificables

Los fakes más robustos mantienen invariantes que puedes verificar:

```rust
use std::cell::RefCell;
use std::collections::HashMap;

pub trait BankAccount {
    fn deposit(&self, account_id: &str, amount: u64) -> Result<u64, String>;
    fn withdraw(&self, account_id: &str, amount: u64) -> Result<u64, String>;
    fn balance(&self, account_id: &str) -> Result<u64, String>;
    fn transfer(&self, from: &str, to: &str, amount: u64) -> Result<(), String>;
}

pub struct InMemoryBank {
    accounts: RefCell<HashMap<String, u64>>,
}

impl InMemoryBank {
    pub fn new() -> Self {
        InMemoryBank { accounts: RefCell::new(HashMap::new()) }
    }

    pub fn with_accounts(accounts: Vec<(&str, u64)>) -> Self {
        let bank = Self::new();
        for (id, balance) in accounts {
            bank.accounts.borrow_mut().insert(id.to_string(), balance);
        }
        bank
    }

    // ── Invariant checkers (para los tests) ───────────────

    /// La suma total de dinero en el sistema no debe cambiar
    /// (conservación de dinero en transfers)
    pub fn total_money(&self) -> u64 {
        self.accounts.borrow().values().sum()
    }

    /// Ninguna cuenta debe tener balance negativo
    /// (u64 ya lo garantiza, pero documentamos la intención)
    pub fn all_balances_non_negative(&self) -> bool {
        true // u64 garantiza esto, pero en un fake más realista con i64...
    }

    /// Número de cuentas
    pub fn account_count(&self) -> usize {
        self.accounts.borrow().len()
    }
}

impl BankAccount for InMemoryBank {
    fn deposit(&self, account_id: &str, amount: u64) -> Result<u64, String> {
        let mut accounts = self.accounts.borrow_mut();
        let balance = accounts.entry(account_id.to_string()).or_insert(0);
        *balance += amount;
        Ok(*balance)
    }

    fn withdraw(&self, account_id: &str, amount: u64) -> Result<u64, String> {
        let mut accounts = self.accounts.borrow_mut();
        let balance = accounts.get_mut(account_id)
            .ok_or_else(|| format!("account {} not found", account_id))?;
        if *balance < amount {
            return Err(format!("insufficient funds: have {}, need {}", balance, amount));
        }
        *balance -= amount;
        Ok(*balance)
    }

    fn balance(&self, account_id: &str) -> Result<u64, String> {
        self.accounts.borrow()
            .get(account_id)
            .copied()
            .ok_or_else(|| format!("account {} not found", account_id))
    }

    fn transfer(&self, from: &str, to: &str, amount: u64) -> Result<(), String> {
        self.withdraw(from, amount)?;
        self.deposit(to, amount)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn transfer_conserves_total_money() {
        let bank = InMemoryBank::with_accounts(vec![
            ("alice", 1000),
            ("bob", 500),
        ]);

        let initial_total = bank.total_money();
        assert_eq!(initial_total, 1500);

        bank.transfer("alice", "bob", 300).unwrap();

        // Invariante: el dinero total no cambió
        assert_eq!(bank.total_money(), initial_total);

        assert_eq!(bank.balance("alice").unwrap(), 700);
        assert_eq!(bank.balance("bob").unwrap(), 800);
    }

    #[test]
    fn insufficient_funds_does_not_corrupt_state() {
        let bank = InMemoryBank::with_accounts(vec![
            ("alice", 100),
            ("bob", 500),
        ]);

        let initial_total = bank.total_money();

        // Transfer que falla
        let result = bank.transfer("alice", "bob", 200);
        assert!(result.is_err());

        // Invariante: el estado no cambió
        assert_eq!(bank.total_money(), initial_total);
        assert_eq!(bank.balance("alice").unwrap(), 100); // sin cambios
        assert_eq!(bank.balance("bob").unwrap(), 500);   // sin cambios
    }
}
```

---

## 17. Mock manual con verificación en Drop

Si necesitas verificación tipo mockall pero sin la dependencia:

```rust
use std::cell::Cell;

pub trait Auditor {
    fn log_action(&self, user: &str, action: &str);
}

pub struct MockAuditor {
    expected_calls: Vec<(String, String)>,
    actual_calls: RefCell<Vec<(String, String)>>,
    verified: Cell<bool>,
}

use std::cell::RefCell;

impl MockAuditor {
    pub fn new() -> Self {
        MockAuditor {
            expected_calls: Vec::new(),
            actual_calls: RefCell::new(Vec::new()),
            verified: Cell::new(false),
        }
    }

    /// Configurar una expectativa
    pub fn expect(mut self, user: &str, action: &str) -> Self {
        self.expected_calls.push((user.to_string(), action.to_string()));
        self
    }

    /// Verificación manual (alternativa al Drop)
    pub fn verify(&self) {
        self.verified.set(true);
        let actual = self.actual_calls.borrow();

        assert_eq!(
            actual.len(),
            self.expected_calls.len(),
            "Expected {} calls, got {}.\nExpected: {:?}\nActual: {:?}",
            self.expected_calls.len(),
            actual.len(),
            self.expected_calls,
            *actual,
        );

        for (i, (expected, actual)) in
            self.expected_calls.iter().zip(actual.iter()).enumerate()
        {
            assert_eq!(
                expected, actual,
                "Call {} mismatch.\nExpected: {:?}\nActual: {:?}",
                i, expected, actual,
            );
        }
    }
}

impl Auditor for MockAuditor {
    fn log_action(&self, user: &str, action: &str) {
        self.actual_calls.borrow_mut().push((user.to_string(), action.to_string()));
    }
}

impl Drop for MockAuditor {
    fn drop(&mut self) {
        // Solo verificar si no se llamó verify() manualmente
        // y no estamos en un panic (evitar double-panic)
        if !self.verified.get() && !std::thread::panicking() {
            let actual = self.actual_calls.borrow();
            if actual.len() != self.expected_calls.len() {
                panic!(
                    "MockAuditor: expected {} calls, got {}",
                    self.expected_calls.len(),
                    actual.len(),
                );
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn audit_logs_in_correct_order() {
        let auditor = MockAuditor::new()
            .expect("alice", "login")
            .expect("alice", "view_dashboard")
            .expect("alice", "logout");

        auditor.log_action("alice", "login");
        auditor.log_action("alice", "view_dashboard");
        auditor.log_action("alice", "logout");

        auditor.verify(); // verificación explícita
    }

    #[test]
    #[should_panic(expected = "Expected 2 calls, got 1")]
    fn detects_missing_call() {
        let auditor = MockAuditor::new()
            .expect("alice", "login")
            .expect("alice", "logout");

        auditor.log_action("alice", "login");
        // Falta "logout" → panic en verify/drop
        auditor.verify();
    }

    #[test]
    fn auto_verify_on_drop() {
        let auditor = MockAuditor::new()
            .expect("bob", "signup");

        auditor.log_action("bob", "signup");
        // No llamamos verify() → Drop verifica automáticamente
    }
}
```

### Comparación con mockall

```
┌─────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Feature                 │ MockAuditor (manual)     │ mockall MockAuditor      │
├─────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Código de setup         │ ~50 líneas (una vez)     │ #[automock] (1 línea)    │
│ Expresividad            │ .expect("user", "act")   │ .expect_log_action()     │
│                         │                          │  .with(eq, eq)           │
│                         │                          │  .times(1)               │
│                         │                          │  .returning(|_,_| ())    │
│ Matchers                │ Solo igualdad exacta     │ eq, gt, str::contains... │
│ Times                   │ Solo "exactamente N"     │ ranges, never(), ...     │
│ Sequences               │ Orden fijo               │ Flexible, multi-sequence │
│ Verificación en Drop    │ ✓                        │ ✓                        │
│ Dependencias externas   │ 0                        │ ~10 crates               │
│ Tiempo de compilación   │ Normal                   │ +5-15s                   │
│ Mantenimiento           │ Manual                   │ Automático               │
└─────────────────────────┴──────────────────────────┴──────────────────────────┘
```

---

## 18. Patrón: test double genérico reutilizable

Un test double que sirve para CUALQUIER trait con un solo método `&str -> Result<String, String>`:

```rust
use std::cell::RefCell;
use std::collections::HashMap;

/// Test double genérico reutilizable para traits tipo "lookup"
pub struct LookupStub {
    responses: HashMap<String, Result<String, String>>,
    default: Result<String, String>,
    calls: RefCell<Vec<String>>,
}

impl LookupStub {
    pub fn new() -> Self {
        LookupStub {
            responses: HashMap::new(),
            default: Err("not configured".into()),
            calls: RefCell::new(Vec::new()),
        }
    }

    pub fn on(mut self, key: &str, response: Result<&str, &str>) -> Self {
        self.responses.insert(
            key.to_string(),
            response.map(|s| s.to_string()).map_err(|e| e.to_string()),
        );
        self
    }

    pub fn default_ok(mut self, value: &str) -> Self {
        self.default = Ok(value.to_string());
        self
    }

    pub fn default_err(mut self, error: &str) -> Self {
        self.default = Err(error.to_string());
        self
    }

    pub fn lookup(&self, key: &str) -> Result<String, String> {
        self.calls.borrow_mut().push(key.to_string());
        self.responses.get(key).cloned().unwrap_or_else(|| self.default.clone())
    }

    pub fn calls(&self) -> Vec<String> {
        self.calls.borrow().clone()
    }
}

// Implementar para diferentes traits:

pub trait ConfigProvider {
    fn get_config(&self, key: &str) -> Result<String, String>;
}

impl ConfigProvider for LookupStub {
    fn get_config(&self, key: &str) -> Result<String, String> {
        self.lookup(key)
    }
}

pub trait SecretManager {
    fn get_secret(&self, name: &str) -> Result<String, String>;
}

impl SecretManager for LookupStub {
    fn get_secret(&self, name: &str) -> Result<String, String> {
        self.lookup(name)
    }
}

// ── Reutilización ─────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn as_config_provider() {
        let config = LookupStub::new()
            .on("database.host", Ok("localhost"))
            .on("database.port", Ok("5432"))
            .default_err("key not found");

        assert_eq!(config.get_config("database.host").unwrap(), "localhost");
        assert!(config.get_config("unknown").is_err());
    }

    #[test]
    fn as_secret_manager() {
        let secrets = LookupStub::new()
            .on("api_key", Ok("secret-123"))
            .on("db_password", Ok("hunter2"))
            .default_err("secret not found");

        assert_eq!(secrets.get_secret("api_key").unwrap(), "secret-123");
        assert_eq!(secrets.calls(), vec!["api_key"]);
    }
}
```

---

## 19. Combinando técnicas: el toolkit completo

Un resumen de cuándo usar cada técnica:

```
┌──────────────────────────────────────────────────────────────────┐
│               Decisión de técnica                                │
│                                                                  │
│  ¿Qué necesitas?           Técnica                               │
│  ───────────────           ───────                               │
│  Valor fijo                Struct con campos pub (sección 3)     │
│                                                                  │
│  Valor según argumentos    Closures como campos (sección 4-5)    │
│                                                                  │
│  Registrar llamadas        Spy con RefCell<Vec> (sección 6-7)    │
│                                                                  │
│  Estado mutable            RefCell para &self (sección 7)        │
│  (caché, DB in-memory)     Mutex para Send+Sync (sección 9)     │
│                                                                  │
│  Estado compartido         Rc<RefCell> (sección 8)               │
│  test ↔ double             Arc<Mutex> si async (sección 9)       │
│                                                                  │
│  Setup mínimo              Default trait (sección 10)            │
│                                                                  │
│  Múltiples modos           Enum variants (sección 11)            │
│                                                                  │
│  API ergonómica            Builder pattern (sección 13)          │
│                                                                  │
│  Fallo-luego-éxito         Cell<u32> counter (sección 14)        │
│                                                                  │
│  Respuestas en orden       RefCell<Vec> + pop (sección 15)       │
│                                                                  │
│  Invariantes               Métodos de inspección (sección 16)    │
│                                                                  │
│  Verificar en Drop         Drop trait manual (sección 17)        │
│                                                                  │
│  Reutilizar entre traits   LookupStub genérico (sección 18)     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Combinación en un ejemplo real

```rust
use std::cell::{Cell, RefCell};

pub trait Database {
    fn query(&self, sql: &str) -> Result<Vec<String>, String>;
    fn execute(&self, sql: &str) -> Result<u64, String>;
}

/// Test double que combina: spy + configurable + fail-after-N
pub struct TestDatabase {
    // Spy: registra todas las queries
    queries: RefCell<Vec<String>>,

    // Configurable: respuestas por query
    query_responses: std::collections::HashMap<String, Vec<String>>,

    // Fail-after-N: simular conexión que muere
    calls_until_failure: Cell<Option<u32>>,
    call_count: Cell<u32>,
}

impl TestDatabase {
    pub fn new() -> Self {
        TestDatabase {
            queries: RefCell::new(Vec::new()),
            query_responses: std::collections::HashMap::new(),
            calls_until_failure: Cell::new(None),
            call_count: Cell::new(0),
        }
    }

    pub fn with_response(mut self, sql_prefix: &str, rows: Vec<&str>) -> Self {
        self.query_responses.insert(
            sql_prefix.to_string(),
            rows.into_iter().map(|s| s.to_string()).collect(),
        );
        self
    }

    pub fn fail_after(self, n: u32) -> Self {
        self.calls_until_failure.set(Some(n));
        self
    }

    // Spy inspection
    pub fn executed_queries(&self) -> Vec<String> {
        self.queries.borrow().clone()
    }

    pub fn query_count(&self) -> usize {
        self.queries.borrow().len()
    }

    fn check_failure(&self) -> Result<(), String> {
        if let Some(limit) = self.calls_until_failure.get() {
            let count = self.call_count.get();
            self.call_count.set(count + 1);
            if count >= limit {
                return Err("connection lost".into());
            }
        }
        Ok(())
    }
}

impl Database for TestDatabase {
    fn query(&self, sql: &str) -> Result<Vec<String>, String> {
        self.check_failure()?;
        self.queries.borrow_mut().push(sql.to_string());

        // Buscar respuesta por prefijo
        for (prefix, rows) in &self.query_responses {
            if sql.starts_with(prefix) {
                return Ok(rows.clone());
            }
        }
        Ok(vec![]) // default: sin resultados
    }

    fn execute(&self, sql: &str) -> Result<u64, String> {
        self.check_failure()?;
        self.queries.borrow_mut().push(sql.to_string());
        Ok(1)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn combined_spy_and_configurable() {
        let db = TestDatabase::new()
            .with_response("SELECT", vec!["alice", "bob"])
            .with_response("INSERT", vec![]);

        let rows = db.query("SELECT * FROM users").unwrap();
        assert_eq!(rows, vec!["alice", "bob"]);

        db.execute("INSERT INTO users VALUES (...)").unwrap();

        // Spy: verificar qué se ejecutó
        let queries = db.executed_queries();
        assert_eq!(queries.len(), 2);
        assert!(queries[0].starts_with("SELECT"));
        assert!(queries[1].starts_with("INSERT"));
    }

    #[test]
    fn connection_failure_after_n_calls() {
        let db = TestDatabase::new()
            .with_response("SELECT", vec!["data"])
            .fail_after(2);

        assert!(db.query("SELECT 1").is_ok());  // call 0
        assert!(db.query("SELECT 2").is_ok());  // call 1
        assert!(db.query("SELECT 3").is_err()); // call 2: FAIL
        assert!(db.query("SELECT 4").is_err()); // call 3: still failing
    }
}
```

---

## 20. Comparación: manual vs mockall

### Para un trait con 3 métodos

```rust
pub trait FileStorage {
    fn read(&self, path: &str) -> Result<Vec<u8>, String>;
    fn write(&self, path: &str, data: &[u8]) -> Result<(), String>;
    fn delete(&self, path: &str) -> Result<(), String>;
}
```

**Con mockall** (5 líneas para el mock + N líneas por test):

```rust
#[automock]
pub trait FileStorage { /* ... */ }

#[test]
fn test_with_mockall() {
    let mut mock = MockFileStorage::new();
    mock.expect_read()
        .with(predicate::eq("/config.json"))
        .times(1)
        .returning(|_| Ok(b"{}".to_vec()));
    mock.expect_write().never();
    mock.expect_delete().never();

    let service = ConfigLoader::new(mock);
    let config = service.load().unwrap();
    assert_eq!(config, "{}");
}
```

**Con Rust puro** (30 líneas para el double + N líneas por test):

```rust
pub struct StubFileStorage {
    read_fn: Box<dyn Fn(&str) -> Result<Vec<u8>, String>>,
    write_fn: Box<dyn Fn(&str, &[u8]) -> Result<(), String>>,
    delete_fn: Box<dyn Fn(&str) -> Result<(), String>>,
}

impl StubFileStorage {
    pub fn new() -> Self {
        StubFileStorage {
            read_fn: Box::new(|_| Ok(vec![])),
            write_fn: Box::new(|_, _| Ok(())),
            delete_fn: Box::new(|_| Ok(())),
        }
    }

    pub fn with_read<F: Fn(&str) -> Result<Vec<u8>, String> + 'static>(
        mut self, f: F,
    ) -> Self {
        self.read_fn = Box::new(f);
        self
    }

    pub fn with_write<F: Fn(&str, &[u8]) -> Result<(), String> + 'static>(
        mut self, f: F,
    ) -> Self {
        self.write_fn = Box::new(f);
        self
    }

    pub fn with_delete<F: Fn(&str) -> Result<(), String> + 'static>(
        mut self, f: F,
    ) -> Self {
        self.delete_fn = Box::new(f);
        self
    }
}

impl FileStorage for StubFileStorage {
    fn read(&self, path: &str) -> Result<Vec<u8>, String> { (self.read_fn)(path) }
    fn write(&self, path: &str, data: &[u8]) -> Result<(), String> { (self.write_fn)(path, data) }
    fn delete(&self, path: &str) -> Result<(), String> { (self.delete_fn)(path) }
}

#[test]
fn test_with_manual_stub() {
    let storage = StubFileStorage::new()
        .with_read(|path| {
            if path == "/config.json" {
                Ok(b"{}".to_vec())
            } else {
                Err("not found".into())
            }
        });

    let service = ConfigLoader::new(storage);
    let config = service.load().unwrap();
    assert_eq!(config, "{}");
}
```

### Tabla comparativa final

```
┌──────────────────────────┬────────────────────────┬──────────────────────────┐
│ Aspecto                  │ Manual (Rust puro)     │ mockall                  │
├──────────────────────────┼────────────────────────┼──────────────────────────┤
│ Boilerplate del double   │ ~10 líneas/método      │ 1 línea (#[automock])    │
│ Boilerplate por test     │ Similar                │ Similar                  │
│ Dependencias             │ 0                      │ ~10 crates               │
│ Tiempo compilación       │ Normal                 │ +5-15s                   │
│ Verificación times       │ Manual (Cell counter)  │ .times(n) built-in       │
│ Verificación args        │ assert! en closure     │ .with(predicate) built-in│
│ Sequences                │ Manual (posible pero   │ .in_sequence() built-in  │
│                          │ laborioso)             │                          │
│ Errores de compilación   │ Claros                 │ A veces crípticos        │
│ Depuración               │ Directo (step into)    │ Paso por macro           │
│ Flexibilidad             │ Total                  │ Limitado a la API        │
│ Aprendizaje              │ Solo Rust idiomático   │ API específica de mockall│
│ Fakes con lógica         │ Natural                │ No soportado             │
│ Traits genéricos         │ Sin restricción        │ Requiere 'static        │
│ Métodos genéricos        │ Sin restricción        │ No soportado             │
│ Traits de crates externos│ impl directamente      │ Requiere mock!{ }        │
├──────────────────────────┼────────────────────────┼──────────────────────────┤
│ Mejor para               │ Proyectos pequeños,    │ Proyectos grandes,       │
│                          │ traits con 1-3 métodos,│ traits con 4+ métodos,   │
│                          │ fakes con lógica,      │ necesidad de times/seq,  │
│                          │ zero-dependency policy │ productividad sobre todo │
└──────────────────────────┴────────────────────────┴──────────────────────────┘
```

---

## 21. Comparación con C y Go

### C: test doubles manuales es la ÚNICA opción

```c
/* C: no hay closures, traits ni generics.
   Los test doubles son function pointers y structs. */

/* "Trait" en C: struct de function pointers */
typedef struct {
    int (*read)(const char* path, char* buf, size_t size);
    int (*write)(const char* path, const char* data, size_t len);
    int (*delete)(const char* path);
    void* context; /* estado del double */
} FileStorage;

/* Stub: retorna valores fijos */
static int stub_read(const char* path, char* buf, size_t size) {
    (void)path;
    const char* data = "{}";
    size_t len = strlen(data);
    if (len > size) len = size;
    memcpy(buf, data, len);
    return (int)len;
}

static int stub_write(const char* path, const char* data, size_t len) {
    (void)path; (void)data; (void)len;
    return 0; /* success */
}

static int stub_delete(const char* path) {
    (void)path;
    return 0;
}

FileStorage create_stub_storage(void) {
    return (FileStorage){
        .read = stub_read,
        .write = stub_write,
        .delete = stub_delete,
        .context = NULL,
    };
}

/* Spy: function pointer + contador global */
static int spy_write_count = 0;
static char spy_write_last_path[256] = {0};

static int spy_write(const char* path, const char* data, size_t len) {
    spy_write_count++;
    strncpy(spy_write_last_path, path, sizeof(spy_write_last_path) - 1);
    (void)data; (void)len;
    return 0;
}

void test_spy_example(void) {
    spy_write_count = 0; /* reset antes de cada test */
    memset(spy_write_last_path, 0, sizeof(spy_write_last_path));

    FileStorage storage = create_stub_storage();
    storage.write = spy_write; /* override solo write */

    /* ... usar storage ... */
    storage.write("/tmp/test.txt", "hello", 5);

    assert(spy_write_count == 1);
    assert(strcmp(spy_write_last_path, "/tmp/test.txt") == 0);
}

/* Configurable stub en C: usando context pointer */
typedef struct {
    int should_fail;
    int fail_after_n;
    int call_count;
    char responses[10][256]; /* hasta 10 respuestas preconfiguradas */
    int response_count;
} StubContext;

static int configurable_read(const char* path, char* buf, size_t size) {
    /* Acceder al context a través de una global (feo pero común en C) */
    extern StubContext* current_stub_ctx;
    StubContext* ctx = current_stub_ctx;

    ctx->call_count++;
    if (ctx->should_fail) return -1;
    if (ctx->fail_after_n > 0 && ctx->call_count > ctx->fail_after_n) return -1;

    /* Retornar respuesta preconfigurada */
    int idx = ctx->call_count - 1;
    if (idx < ctx->response_count) {
        size_t len = strlen(ctx->responses[idx]);
        if (len > size) len = size;
        memcpy(buf, ctx->responses[idx], len);
        return (int)len;
    }
    return 0;
}
```

### Go: test doubles manuales como práctica idiomática

```go
// Go: interfaces implícitas hacen que los test doubles manuales
// sean la práctica RECOMENDADA (no gomock)

// Interface
type FileStorage interface {
    Read(path string) ([]byte, error)
    Write(path string, data []byte) error
    Delete(path string) error
}

// Stub con campos configurables (equivalente exacto del patrón Rust)
type StubStorage struct {
    ReadFn   func(string) ([]byte, error)
    WriteFn  func(string, []byte) error
    DeleteFn func(string) error
}

func (s *StubStorage) Read(path string) ([]byte, error) {
    if s.ReadFn != nil { return s.ReadFn(path) }
    return nil, nil
}
func (s *StubStorage) Write(path string, data []byte) error {
    if s.WriteFn != nil { return s.WriteFn(path, data) }
    return nil
}
func (s *StubStorage) Delete(path string) error {
    if s.DeleteFn != nil { return s.DeleteFn(path) }
    return nil
}

// Test: solo configurar lo necesario
func TestConfigLoader(t *testing.T) {
    storage := &StubStorage{
        ReadFn: func(path string) ([]byte, error) {
            if path == "/config.json" {
                return []byte("{}"), nil
            }
            return nil, fmt.Errorf("not found: %s", path)
        },
    }

    loader := NewConfigLoader(storage)
    config, err := loader.Load()
    assert.NoError(t, err)
    assert.Equal(t, "{}", string(config))
}

// Spy con slice de llamadas (equivalente al Rc<RefCell<Vec>>)
type SpyStorage struct {
    Calls []string
    mu    sync.Mutex
}

func (s *SpyStorage) Read(path string) ([]byte, error) {
    s.mu.Lock()
    s.Calls = append(s.Calls, "read:"+path)
    s.mu.Unlock()
    return nil, nil
}
// ...

// En Go, esta práctica se llama "table-driven test doubles"
// y es la forma idiomática — gomock se considera over-engineering
// para la mayoría de casos.
```

### Tabla comparativa trilingüe

```
┌──────────────────────┬───────────────────────┬───────────────────┬────────────────────┐
│ Técnica              │ Rust                  │ Go                │ C                  │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Struct con campos    │ struct + pub fields   │ struct + func     │ struct + function  │
│ configurables        │ impl Trait for Struct │ fields            │ pointers           │
│                      │                       │ (idiomático)      │ (el ÚNICO patrón)  │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Closures como stubs  │ Box<dyn Fn(...)>      │ func(...) fields  │ No hay closures    │
│                      │ move || captura       │ closure literal   │ (function pointer  │
│                      │                       │                   │ + void* context)   │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Spy (registrar calls)│ RefCell<Vec<Call>>     │ []Call + mutex    │ Variable global    │
│                      │ o Rc<RefCell<Vec>>     │                   │ + array estático   │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Interior mutability  │ Cell/RefCell           │ No necesario      │ Puntero mutable    │
│ (para &self methods) │ (compile-time ergon.) │ (interfaces usan  │ (void* context)    │
│                      │                       │ punteros mutables)│                    │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Respuestas           │ RefCell<Vec> + pop()  │ []response + idx  │ Array + contador   │
│ secuenciales         │                       │                   │ global             │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Fail-then-succeed    │ Cell<u32> counter     │ atomic.Int32      │ int counter global │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Builder pattern      │ ::new().with_x().     │ Menos común       │ No existe          │
│                      │ build()               │ (struct literal)  │ (init functions)   │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Type safety          │ Completa (genéricos + │ Runtime (duck     │ Ninguna (void*,    │
│                      │ trait bounds)         │ typing)           │ casts)             │
├──────────────────────┼───────────────────────┼───────────────────┼────────────────────┤
│ Filosofía de la      │ "Evalúa si necesitas  │ "Mocks manuales   │ "Test doubles      │
│ comunidad            │ mockall o manual"     │ siempre, gomock   │ manuales porque    │
│                      │                       │ solo si complejo" │ no hay alternativa"│
└──────────────────────┴───────────────────────┴───────────────────┴────────────────────┘
```

---

## 22. Errores comunes

### Error 1: Olvidar el 'static bound en closures

```rust
// ✗ Error: closure con referencia a variable local
fn make_stub(name: &str) -> StubUserRepo {
    let name_ref = name; // referencia local
    StubUserRepo::new()
        .with_find(move |_| {
            // ERROR: name_ref no vive lo suficiente
            Ok(Some(User { id: 1, name: name_ref.to_string(), email: "".into() }))
        })
}
// error[E0621]: explicit lifetime required in the type of `name`

// ✓ Solución: clonar el string antes de capturar
fn make_stub(name: &str) -> StubUserRepo {
    let name_owned = name.to_string(); // owned
    StubUserRepo::new()
        .with_find(move |_| {
            Ok(Some(User { id: 1, name: name_owned.clone(), email: "".into() }))
        })
}
```

### Error 2: Borrow conflicts con RefCell

```rust
// ✗ Error: borrow y borrow_mut simultáneos
impl MyFake {
    fn process(&self) {
        let data = self.state.borrow(); // borrow activo
        if data.is_empty() {
            self.state.borrow_mut().push("default".into()); // PANIC: ya hay borrow
        }
    }
}

// ✓ Solución: soltar el borrow antes de mutar
impl MyFake {
    fn process(&self) {
        let is_empty = self.state.borrow().is_empty(); // borrow se suelta aquí
        if is_empty {
            self.state.borrow_mut().push("default".into()); // ok
        }
    }
}

// ✓ Solución alternativa: scope explícito
impl MyFake {
    fn process(&self) {
        let is_empty = {
            let data = self.state.borrow();
            data.is_empty()
        }; // borrow se suelta al salir del bloque
        if is_empty {
            self.state.borrow_mut().push("default".into());
        }
    }
}
```

### Error 3: Test double que no implementa todos los métodos del trait

```rust
// ✗ Trait con 5 métodos pero el test solo necesita 1
pub trait ComplexService {
    fn method_a(&self) -> i32;
    fn method_b(&self, x: &str) -> String;
    fn method_c(&self) -> Vec<u8>;
    fn method_d(&self, a: i32, b: i32) -> i32;
    fn method_e(&self) -> bool;
}

// Tienes que implementar TODOS los métodos aunque solo uses 1
struct StubForTestX;
impl ComplexService for StubForTestX {
    fn method_a(&self) -> i32 { 42 }
    fn method_b(&self, _: &str) -> String { unimplemented!() } // peligro
    fn method_c(&self) -> Vec<u8> { unimplemented!() }
    fn method_d(&self, _: i32, _: i32) -> i32 { unimplemented!() }
    fn method_e(&self) -> bool { unimplemented!() }
}

// ✓ Solución 1: panic con mensaje descriptivo
struct StubForTestX;
impl ComplexService for StubForTestX {
    fn method_a(&self) -> i32 { 42 }
    fn method_b(&self, _: &str) -> String {
        panic!("method_b should not be called in this test")
    }
    fn method_c(&self) -> Vec<u8> {
        panic!("method_c should not be called in this test")
    }
    fn method_d(&self, _: i32, _: i32) -> i32 {
        panic!("method_d should not be called in this test")
    }
    fn method_e(&self) -> bool {
        panic!("method_e should not be called in this test")
    }
}

// ✓ Solución 2: default implementation en el trait (si lo controlas)
pub trait ComplexService {
    fn method_a(&self) -> i32;
    fn method_b(&self, _x: &str) -> String { String::new() }
    fn method_c(&self) -> Vec<u8> { vec![] }
    fn method_d(&self, _a: i32, _b: i32) -> i32 { 0 }
    fn method_e(&self) -> bool { false }
}
// Ahora solo implementas method_a

// ✓ Solución 3: Interface Segregation — dividir el trait
pub trait ServiceA { fn method_a(&self) -> i32; }
pub trait ServiceB { fn method_b(&self, x: &str) -> String; }
// Solo inyectar lo que necesitas
```

### Error 4: Usar Rc<RefCell<>> en tests async

```rust
// ✗ Rc no es Send — no puede cruzar boundaries de async
#[tokio::test]
async fn test_async() {
    let calls = Rc::new(RefCell::new(Vec::<String>::new()));
    // ERROR: Rc no es Send, no puede moverse entre tasks
}

// ✓ Usar Arc<Mutex<>> para async
#[tokio::test]
async fn test_async() {
    let calls = Arc::new(Mutex::new(Vec::<String>::new()));
    let calls_clone = calls.clone();

    tokio::spawn(async move {
        calls_clone.lock().unwrap().push("called".into());
    }).await.unwrap();

    assert_eq!(calls.lock().unwrap().len(), 1);
}
```

### Error 5: Box\<dyn Fn\> que necesita FnMut

```rust
// ✗ Fn no puede mutar estado capturado
pub struct Stub {
    handler: Box<dyn Fn() -> i32>, // Fn, no FnMut
}

// No puedes mutar un counter dentro de un Fn
// let mut count = 0;
// Box::new(move || { count += 1; count }) // ERROR: Fn no permite mutación

// ✓ Solución: Cell para contadores simples (no necesita FnMut)
let count = Cell::new(0);
let handler: Box<dyn Fn() -> i32> = Box::new(move || {
    let c = count.get() + 1;
    count.set(c);
    c as i32
});

// ✓ Solución alternativa: RefCell
let count = RefCell::new(0);
let handler: Box<dyn Fn() -> i32> = Box::new(move || {
    let mut c = count.borrow_mut();
    *c += 1;
    *c
});
```

---

## 23. Ejemplo completo: sistema de logging distribuido

```rust
// ============================================================
// Sistema de logging distribuido: test doubles sin crates
// ============================================================

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::time::{Duration, SystemTime};

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct LogEntry {
    pub timestamp: SystemTime,
    pub level: LogLevel,
    pub service: String,
    pub message: String,
    pub metadata: HashMap<String, String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
}

#[derive(Debug, PartialEq)]
pub enum LogError {
    TransportFailed(String),
    BufferFull,
    InvalidEntry(String),
    Shutdown,
}

// ── Traits ───────────────────────────────────────────────────

pub trait LogTransport {
    fn send(&self, entries: &[LogEntry]) -> Result<usize, String>;
    fn flush(&self) -> Result<(), String>;
    fn is_healthy(&self) -> bool;
}

pub trait LogFilter {
    fn should_log(&self, entry: &LogEntry) -> bool;
}

pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub trait AlertSender {
    fn send_alert(&self, message: &str) -> Result<(), String>;
}

// ── Servicio: LogAggregator ──────────────────────────────────

pub struct LogAggregator<T, F, C, A>
where
    T: LogTransport,
    F: LogFilter,
    C: Clock,
    A: AlertSender,
{
    transport: T,
    filter: F,
    clock: C,
    alerter: A,
    buffer: RefCell<Vec<LogEntry>>,
    buffer_capacity: usize,
    error_threshold: u32,
    error_count: Cell<u32>,
}

impl<T, F, C, A> LogAggregator<T, F, C, A>
where
    T: LogTransport,
    F: LogFilter,
    C: Clock,
    A: AlertSender,
{
    pub fn new(transport: T, filter: F, clock: C, alerter: A) -> Self {
        LogAggregator {
            transport,
            filter,
            clock,
            alerter,
            buffer: RefCell::new(Vec::new()),
            buffer_capacity: 100,
            error_threshold: 5,
            error_count: Cell::new(0),
        }
    }

    pub fn with_capacity(mut self, capacity: usize) -> Self {
        self.buffer_capacity = capacity;
        self
    }

    pub fn log(
        &self,
        level: LogLevel,
        service: &str,
        message: &str,
        metadata: HashMap<String, String>,
    ) -> Result<(), LogError> {
        let entry = LogEntry {
            timestamp: self.clock.now(),
            level,
            service: service.to_string(),
            message: message.to_string(),
            metadata,
        };

        // Filtrar
        if !self.filter.should_log(&entry) {
            return Ok(());
        }

        // Verificar buffer
        let mut buffer = self.buffer.borrow_mut();
        if buffer.len() >= self.buffer_capacity {
            return Err(LogError::BufferFull);
        }

        // Alertar si es Fatal
        if entry.level == LogLevel::Fatal {
            let _ = self.alerter.send_alert(
                &format!("[FATAL] {}: {}", entry.service, entry.message)
            );
        }

        buffer.push(entry);

        // Flush si buffer está al 80%
        if buffer.len() >= self.buffer_capacity * 80 / 100 {
            drop(buffer); // soltar el borrow
            self.flush()?;
        }

        Ok(())
    }

    pub fn flush(&self) -> Result<(), LogError> {
        let entries: Vec<LogEntry> = {
            let mut buffer = self.buffer.borrow_mut();
            buffer.drain(..).collect()
        };

        if entries.is_empty() {
            return Ok(());
        }

        match self.transport.send(&entries) {
            Ok(_sent) => {
                self.error_count.set(0);
                Ok(())
            }
            Err(e) => {
                let count = self.error_count.get() + 1;
                self.error_count.set(count);

                // Devolver entries al buffer
                self.buffer.borrow_mut().extend(entries);

                // Alertar si excede umbral
                if count >= self.error_threshold {
                    let _ = self.alerter.send_alert(
                        &format!("Log transport failing: {} consecutive errors", count)
                    );
                }

                Err(LogError::TransportFailed(e))
            }
        }
    }

    pub fn pending_count(&self) -> usize {
        self.buffer.borrow().len()
    }
}

// ── Test Doubles (0 dependencias externas) ───────────────────

/// FAKE transport: almacena los logs en memoria
pub struct InMemoryTransport {
    entries: RefCell<Vec<LogEntry>>,
    should_fail: Cell<bool>,
    fail_count: Cell<u32>,
    max_failures: Cell<u32>,
}

impl InMemoryTransport {
    pub fn new() -> Self {
        InMemoryTransport {
            entries: RefCell::new(Vec::new()),
            should_fail: Cell::new(false),
            fail_count: Cell::new(0),
            max_failures: Cell::new(0),
        }
    }

    pub fn failing() -> Self {
        let t = Self::new();
        t.should_fail.set(true);
        t
    }

    pub fn fail_n_times(n: u32) -> Self {
        let t = Self::new();
        t.max_failures.set(n);
        t
    }

    pub fn entries(&self) -> Vec<LogEntry> {
        self.entries.borrow().clone()
    }

    pub fn entry_count(&self) -> usize {
        self.entries.borrow().len()
    }

    pub fn entries_by_level(&self, level: &LogLevel) -> Vec<LogEntry> {
        self.entries.borrow().iter()
            .filter(|e| e.level == *level)
            .cloned()
            .collect()
    }
}

impl LogTransport for InMemoryTransport {
    fn send(&self, entries: &[LogEntry]) -> Result<usize, String> {
        // Fail-then-succeed
        if self.max_failures.get() > 0 {
            let count = self.fail_count.get();
            if count < self.max_failures.get() {
                self.fail_count.set(count + 1);
                return Err(format!("transport error (failure {})", count + 1));
            }
        }

        if self.should_fail.get() {
            return Err("transport permanently down".into());
        }

        let count = entries.len();
        self.entries.borrow_mut().extend_from_slice(entries);
        Ok(count)
    }

    fn flush(&self) -> Result<(), String> {
        Ok(())
    }

    fn is_healthy(&self) -> bool {
        !self.should_fail.get()
    }
}

/// CONFIGURABLE filter: closure-based
pub struct ConfigurableFilter {
    predicate: Box<dyn Fn(&LogEntry) -> bool>,
}

impl ConfigurableFilter {
    pub fn allow_all() -> Self {
        ConfigurableFilter { predicate: Box::new(|_| true) }
    }

    pub fn min_level(min: LogLevel) -> Self {
        let levels = vec![
            LogLevel::Debug,
            LogLevel::Info,
            LogLevel::Warn,
            LogLevel::Error,
            LogLevel::Fatal,
        ];
        let min_idx = levels.iter().position(|l| *l == min).unwrap_or(0);

        ConfigurableFilter {
            predicate: Box::new(move |entry| {
                let entry_idx = levels.iter().position(|l| *l == entry.level).unwrap_or(0);
                entry_idx >= min_idx
            }),
        }
    }

    pub fn custom<F: Fn(&LogEntry) -> bool + 'static>(f: F) -> Self {
        ConfigurableFilter { predicate: Box::new(f) }
    }
}

impl LogFilter for ConfigurableFilter {
    fn should_log(&self, entry: &LogEntry) -> bool {
        (self.predicate)(entry)
    }
}

/// FAKE clock: controlable
pub struct FakeClock {
    time: Cell<SystemTime>,
}

impl FakeClock {
    pub fn at_epoch(secs: u64) -> Self {
        FakeClock {
            time: Cell::new(SystemTime::UNIX_EPOCH + Duration::from_secs(secs)),
        }
    }

    pub fn advance(&self, d: Duration) {
        self.time.set(self.time.get() + d);
    }
}

impl Clock for FakeClock {
    fn now(&self) -> SystemTime { self.time.get() }
}

/// SPY alerter: registra alertas para inspección
pub struct SpyAlerter {
    alerts: RefCell<Vec<String>>,
    should_fail: bool,
}

impl SpyAlerter {
    pub fn new() -> Self {
        SpyAlerter { alerts: RefCell::new(Vec::new()), should_fail: false }
    }

    pub fn failing() -> Self {
        SpyAlerter { alerts: RefCell::new(Vec::new()), should_fail: true }
    }

    pub fn alerts(&self) -> Vec<String> { self.alerts.borrow().clone() }
    pub fn alert_count(&self) -> usize { self.alerts.borrow().len() }

    pub fn has_alert_containing(&self, text: &str) -> bool {
        self.alerts.borrow().iter().any(|a| a.contains(text))
    }
}

impl AlertSender for SpyAlerter {
    fn send_alert(&self, message: &str) -> Result<(), String> {
        self.alerts.borrow_mut().push(message.to_string());
        if self.should_fail {
            Err("alert service down".into())
        } else {
            Ok(())
        }
    }
}

// ── Tests ────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    fn meta(pairs: Vec<(&str, &str)>) -> HashMap<String, String> {
        pairs.into_iter().map(|(k, v)| (k.into(), v.into())).collect()
    }

    #[test]
    fn basic_logging() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Info, "api", "request received", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "api", "response sent", meta(vec![])).unwrap();

        assert_eq!(agg.pending_count(), 2);

        agg.flush().unwrap();

        assert_eq!(agg.pending_count(), 0);
        assert_eq!(transport.entry_count(), 2);
    }

    #[test]
    fn filter_excludes_debug() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::min_level(LogLevel::Info);
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Debug, "api", "debug msg", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "api", "info msg", meta(vec![])).unwrap();

        // Debug fue filtrado
        assert_eq!(agg.pending_count(), 1);

        agg.flush().unwrap();
        assert_eq!(transport.entry_count(), 1);
        assert_eq!(transport.entries()[0].level, LogLevel::Info);
    }

    #[test]
    fn fatal_triggers_alert() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Fatal, "db", "connection lost", meta(vec![])).unwrap();

        assert_eq!(alerter.alert_count(), 1);
        assert!(alerter.has_alert_containing("[FATAL]"));
        assert!(alerter.has_alert_containing("db"));
    }

    #[test]
    fn buffer_full_returns_error() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter)
            .with_capacity(3);

        agg.log(LogLevel::Info, "a", "1", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "a", "2", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "a", "3", meta(vec![])).unwrap();

        let result = agg.log(LogLevel::Info, "a", "4", meta(vec![]));
        assert_eq!(result, Err(LogError::BufferFull));
    }

    #[test]
    fn transport_failure_preserves_entries() {
        let transport = InMemoryTransport::failing();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Info, "api", "msg", meta(vec![])).unwrap();
        assert_eq!(agg.pending_count(), 1);

        let result = agg.flush();
        assert!(matches!(result, Err(LogError::TransportFailed(_))));

        // Entries devueltas al buffer
        assert_eq!(agg.pending_count(), 1);
    }

    #[test]
    fn consecutive_transport_errors_trigger_alert() {
        let transport = InMemoryTransport::failing();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter)
            .with_capacity(100);

        // Generar logs
        for i in 0..5 {
            agg.log(LogLevel::Info, "api", &format!("msg {}", i), meta(vec![])).unwrap();
        }

        // 5 intentos de flush fallidos
        for _ in 0..5 {
            let _ = agg.flush();
        }

        // Al 5to error, se envía alerta de transport failing
        assert!(alerter.has_alert_containing("consecutive errors"));
    }

    #[test]
    fn transport_recovers_after_failures() {
        let transport = InMemoryTransport::fail_n_times(2);
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Info, "api", "msg", meta(vec![])).unwrap();

        // Primer flush: falla
        assert!(agg.flush().is_err());
        assert_eq!(agg.pending_count(), 1); // entries preservadas

        // Segundo flush: falla
        assert!(agg.flush().is_err());
        assert_eq!(agg.pending_count(), 1);

        // Tercer flush: éxito
        assert!(agg.flush().is_ok());
        assert_eq!(agg.pending_count(), 0);
        assert_eq!(transport.entry_count(), 1);
    }

    #[test]
    fn timestamps_from_fake_clock() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::allow_all();
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Info, "api", "first", meta(vec![])).unwrap();
        clock.advance(Duration::from_secs(60));
        agg.log(LogLevel::Info, "api", "second", meta(vec![])).unwrap();

        agg.flush().unwrap();

        let entries = transport.entries();
        let t1 = entries[0].timestamp;
        let t2 = entries[1].timestamp;

        assert_eq!(
            t2.duration_since(t1).unwrap(),
            Duration::from_secs(60)
        );
    }

    #[test]
    fn custom_filter() {
        let transport = InMemoryTransport::new();
        let filter = ConfigurableFilter::custom(|entry| {
            entry.service == "api" && entry.level != LogLevel::Debug
        });
        let clock = FakeClock::at_epoch(1_000_000);
        let alerter = SpyAlerter::new();

        let agg = LogAggregator::new(&transport, filter, &clock, &alerter);

        agg.log(LogLevel::Debug, "api", "debug", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "api", "info", meta(vec![])).unwrap();
        agg.log(LogLevel::Info, "db", "other service", meta(vec![])).unwrap();

        agg.flush().unwrap();
        assert_eq!(transport.entry_count(), 1); // solo "api" + "Info"
    }
}
```

---

## 24. Programa de práctica

### Proyecto: `job_queue` — cola de trabajos con test doubles manuales

Implementa una cola de trabajos con 4 dependencias, todas testeadas con test doubles escritos a mano (sin mockall).

### src/lib.rs

```rust
use std::collections::HashMap;
use std::time::{Duration, SystemTime};

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct Job {
    pub id: Option<u64>,
    pub name: String,
    pub payload: String,
    pub priority: u8,      // 0-255, mayor = más urgente
    pub max_retries: u32,
    pub retry_count: u32,
    pub status: JobStatus,
    pub created_at: SystemTime,
    pub started_at: Option<SystemTime>,
    pub finished_at: Option<SystemTime>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum JobStatus {
    Pending,
    Running,
    Completed,
    Failed(String),
    Retrying,
    Dead, // excedió max_retries
}

#[derive(Debug, PartialEq)]
pub enum QueueError {
    JobNotFound(u64),
    HandlerNotFound(String),
    StorageError(String),
    QueueEmpty,
}

// ── Traits ───────────────────────────────────────────────────

pub trait JobStore {
    fn enqueue(&self, job: &Job) -> Result<u64, String>;
    fn dequeue(&self, now: SystemTime) -> Result<Option<Job>, String>;
    fn update_status(&self, job_id: u64, status: JobStatus) -> Result<(), String>;
    fn increment_retry(&self, job_id: u64) -> Result<u32, String>;
    fn get_by_id(&self, id: u64) -> Result<Option<Job>, String>;
    fn count_by_status(&self, status: &JobStatus) -> Result<usize, String>;
}

pub trait JobHandler {
    fn execute(&self, job_name: &str, payload: &str) -> Result<String, String>;
}

pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub trait DeadLetterNotifier {
    fn notify_dead_job(&self, job: &Job) -> Result<(), String>;
}

// ── Servicio ─────────────────────────────────────────────────

pub struct JobQueue<S, H, C, D>
where
    S: JobStore,
    H: JobHandler,
    C: Clock,
    D: DeadLetterNotifier,
{
    store: S,
    handler: H,
    clock: C,
    dead_letter: D,
}

impl<S, H, C, D> JobQueue<S, H, C, D>
where
    S: JobStore,
    H: JobHandler,
    C: Clock,
    D: DeadLetterNotifier,
{
    pub fn new(store: S, handler: H, clock: C, dead_letter: D) -> Self {
        JobQueue { store, handler, clock, dead_letter }
    }

    pub fn submit(&self, name: &str, payload: &str, priority: u8, max_retries: u32)
        -> Result<u64, QueueError>
    {
        let job = Job {
            id: None,
            name: name.to_string(),
            payload: payload.to_string(),
            priority,
            max_retries,
            retry_count: 0,
            status: JobStatus::Pending,
            created_at: self.clock.now(),
            started_at: None,
            finished_at: None,
        };

        self.store.enqueue(&job)
            .map_err(|e| QueueError::StorageError(e))
    }

    pub fn process_next(&self) -> Result<Option<Job>, QueueError> {
        let now = self.clock.now();
        let job = self.store.dequeue(now)
            .map_err(|e| QueueError::StorageError(e))?;

        let mut job = match job {
            Some(j) => j,
            None => return Ok(None),
        };

        let job_id = job.id.unwrap();

        // Marcar como running
        self.store.update_status(job_id, JobStatus::Running)
            .map_err(|e| QueueError::StorageError(e))?;
        job.started_at = Some(now);

        // Ejecutar
        match self.handler.execute(&job.name, &job.payload) {
            Ok(_output) => {
                job.status = JobStatus::Completed;
                job.finished_at = Some(self.clock.now());
                let _ = self.store.update_status(job_id, JobStatus::Completed);
            }
            Err(e) => {
                let retries = self.store.increment_retry(job_id)
                    .unwrap_or(job.retry_count + 1);
                job.retry_count = retries;

                if retries >= job.max_retries {
                    job.status = JobStatus::Dead;
                    let _ = self.store.update_status(job_id, JobStatus::Dead);
                    let _ = self.dead_letter.notify_dead_job(&job);
                } else {
                    job.status = JobStatus::Retrying;
                    let _ = self.store.update_status(job_id, JobStatus::Pending);
                }
            }
        }

        Ok(Some(job))
    }
}
```

### El estudiante debe implementar (sin mockall)

1. **`InMemoryJobStore`**: fake con `RefCell<HashMap<u64, Job>>` + `Cell<u64>` para auto-increment. Implementar dequeue que retorna el job pending de mayor prioridad.

2. **`ConfigurableHandler`**: stub con `Box<dyn Fn(&str, &str) -> Result<String, String>>` usando builder pattern.

3. **`FakeClock`**: con `advance()`.

4. **`SpyDeadLetter`**: spy con `RefCell<Vec<Job>>` para registrar notificaciones.

5. **Tests** (todos sin mockall):
   - Submit y process: happy path
   - Process de cola vacía: retorna None
   - Handler falla: retry count incrementa
   - Handler falla y excede max_retries: job va a Dead + notificación
   - Prioridad: jobs de mayor prioridad se procesan primero
   - Múltiples jobs: procesar en orden de prioridad
   - Timestamps correctos con FakeClock
   - Dead letter spy registra el job muerto correctamente
   - Handler configurable: diferentes comportamientos por job name

---

## 25. Ejercicios

### Ejercicio 1: Spy genérico reutilizable

**Objetivo**: Construir un spy genérico que registre llamadas para cualquier trait con un solo método.

**Tareas**:

**a)** Implementa un `GenericSpy<Args, Ret>`:

```rust
pub struct GenericSpy<Args: Clone, Ret: Clone> {
    calls: RefCell<Vec<Args>>,
    return_fn: Box<dyn Fn(&Args) -> Ret>,
}
```

**b)** Implementa métodos: `new(return_fn)`, `calls()`, `call_count()`, `last_call()`, `was_called_with(predicate)`.

**c)** Usa el spy para implementar 3 traits diferentes y escribe tests que verifiquen las llamadas.

---

### Ejercicio 2: Fake de cola de mensajes con prioridades

**Objetivo**: Construir un fake complejo que mantenga invariantes.

**Tareas**:

**a)** Implementa `InMemoryPriorityQueue` que use `BTreeMap<u8, VecDeque<Message>>`.

**b)** Implementa el trait:

```rust
pub trait MessageQueue {
    fn publish(&self, topic: &str, payload: &str, priority: u8) -> Result<u64, String>;
    fn consume(&self, topic: &str) -> Result<Option<Message>, String>;
    fn ack(&self, message_id: u64) -> Result<(), String>;
    fn nack(&self, message_id: u64) -> Result<(), String>; // requeue
    fn pending_count(&self, topic: &str) -> usize;
}
```

**c)** Invariantes a verificar en tests:
- Los mensajes se consumen en orden de prioridad (mayor primero)
- `nack` devuelve el mensaje a la cola
- `ack` lo elimina permanentemente
- `pending_count` es consistente con publish/consume/ack/nack

**d)** Escribe un `MessageProcessor` que consume y procesa mensajes, y tests que usen el fake.

---

### Ejercicio 3: Test double con múltiples modos (enum)

**Objetivo**: Practicar el patrón enum-based double.

**Tareas**:

**a)** Implementa un `StubDatabase` con 5 variantes:

```rust
pub enum StubDatabase {
    Empty,                              // siempre retorna vacío
    WithData(HashMap<String, String>),  // datos estáticos
    FailAfter(Cell<u32>, u32),          // falla después de N queries
    Latency(Duration),                  // simula latencia (no sleep, solo registra)
    Sequence(RefCell<Vec<Result<...>>>), // respuestas predefinidas
}
```

**b)** Implementa el trait `Database` para cada variante.

**c)** Escribe tests que demuestren cada modo y documenta cuándo usar cada uno.

---

### Ejercicio 4: Mock manual con verificación de orden

**Objetivo**: Implementar verificación de secuencia sin mockall.

**Tareas**:

**a)** Implementa un `SequenceVerifier`:

```rust
pub struct SequenceVerifier {
    expected: Vec<String>,
    actual: RefCell<Vec<String>>,
}
```

**b)** Implementa: `new(expected)`, `record(event)`, `verify()` (en Drop también).

**c)** Úsalo para verificar que un `TransactionManager` ejecuta: `begin → execute → commit` en el orden correcto.

**d)** Escribe tests para: orden correcto pasa, orden incorrecto falla, llamadas extra fallan, llamadas faltantes fallan.

---

## Navegación

- **Anterior**: [T03 - Cuándo no mockear](../T03_Cuando_no_mockear/README.md)
- **Siguiente**: [S05/T01 - cargo-tarpaulin](../../S05_Cobertura_en_Rust/T01_Cargo_tarpaulin/README.md)

---

> **Sección 4: Mocking en Rust** — Completada (4 de 4 tópicos)
>
> **Resumen de S04:**
> - T01: Trait-based dependency injection — definir trait, implementación real + mock, inyectar en tests ✓
> - T02: mockall crate — #[automock], expect_*, returning, times, sequences ✓
> - T03: Cuándo no mockear — preferir integración real cuando es rápido, fake implementations sobre mocks complejos ✓
> - T04: Test doubles sin crates externos — struct con campos configurables, closures como stubs ✓
>
> **Fin de la Sección 4: Mocking en Rust**
