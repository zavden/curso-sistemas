# T01 - Trait-based Dependency Injection: la base del mocking en Rust

## Índice

1. [El problema: código acoplado a implementaciones concretas](#1-el-problema-código-acoplado-a-implementaciones-concretas)
2. [Qué es Dependency Injection](#2-qué-es-dependency-injection)
3. [Traits como contratos de dependencia](#3-traits-como-contratos-de-dependencia)
4. [Patrón completo: trait + implementación real + mock](#4-patrón-completo-trait--implementación-real--mock)
5. [Inyección por genéricos vs por trait objects](#5-inyección-por-genéricos-vs-por-trait-objects)
6. [Inyección por constructor (lo más común)](#6-inyección-por-constructor-lo-más-común)
7. [Inyección por método (menos común)](#7-inyección-por-método-menos-común)
8. [Diseñar traits testeables: principios](#8-diseñar-traits-testeables-principios)
9. [Ejemplo real: servicio con base de datos](#9-ejemplo-real-servicio-con-base-de-datos)
10. [Ejemplo real: servicio con HTTP client](#10-ejemplo-real-servicio-con-http-client)
11. [Ejemplo real: servicio con reloj (tiempo)](#11-ejemplo-real-servicio-con-reloj-tiempo)
12. [Ejemplo real: servicio con sistema de archivos](#12-ejemplo-real-servicio-con-sistema-de-archivos)
13. [Múltiples dependencias: composición](#13-múltiples-dependencias-composición)
14. [Mocks manuales: struct que implementa el trait](#14-mocks-manuales-struct-que-implementa-el-trait)
15. [Mocks con estado: verificar llamadas](#15-mocks-con-estado-verificar-llamadas)
16. [Mocks con closures: comportamiento dinámico](#16-mocks-con-closures-comportamiento-dinámico)
17. [Fakes vs Mocks vs Stubs: vocabulario preciso](#17-fakes-vs-mocks-vs-stubs-vocabulario-preciso)
18. [Errores de diseño que dificultan el testing](#18-errores-de-diseño-que-dificultan-el-testing)
19. [async traits y mocking](#19-async-traits-y-mocking)
20. [Comparación con C y Go](#20-comparación-con-c-y-go)
21. [Errores comunes](#21-errores-comunes)
22. [Ejemplo completo: sistema de notificaciones](#22-ejemplo-completo-sistema-de-notificaciones)
23. [Programa de práctica](#23-programa-de-práctica)
24. [Ejercicios](#24-ejercicios)

---

## 1. El problema: código acoplado a implementaciones concretas

Cuando el código depende directamente de implementaciones concretas (base de datos real, API HTTP, sistema de archivos, reloj del sistema), los tests se vuelven lentos, frágiles e impredecibles.

### Código acoplado: imposible de testear en aislamiento

```rust
// ❌ PROBLEMA: UserService depende DIRECTAMENTE de una base de datos PostgreSQL
use postgres::Client;

pub struct UserService {
    db: Client,  // ← dependencia concreta
}

impl UserService {
    pub fn new(connection_string: &str) -> Self {
        let client = Client::connect(connection_string, postgres::NoTls)
            .expect("failed to connect");
        UserService { db: client }
    }

    pub fn get_user(&mut self, id: i64) -> Option<User> {
        let row = self.db
            .query_one("SELECT id, name, email FROM users WHERE id = $1", &[&id])
            .ok()?;
        Some(User {
            id: row.get(0),
            name: row.get(1),
            email: row.get(2),
        })
    }

    pub fn create_user(&mut self, name: &str, email: &str) -> Result<i64, String> {
        // Lógica de negocio + acceso a BD mezclados
        if name.is_empty() {
            return Err("name cannot be empty".to_string());
        }
        if !email.contains('@') {
            return Err("invalid email".to_string());
        }
        
        let row = self.db
            .query_one(
                "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
                &[&name, &email],
            )
            .map_err(|e| e.to_string())?;
        
        Ok(row.get(0))
    }
}
```

### Por qué esto es un problema

```
┌─────────────────────────────────────────────────────────────────┐
│         PROBLEMAS DEL CÓDIGO ACOPLADO                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. NECESITA INFRAESTRUCTURA REAL                               │
│     Para testear UserService necesitas PostgreSQL corriendo     │
│     con el esquema correcto y datos de prueba                   │
│                                                                 │
│  2. TESTS LENTOS                                                │
│     Cada test conecta a la BD, ejecuta queries reales           │
│     100 tests × 50ms/query = 5 segundos (vs 5ms con mocks)     │
│                                                                 │
│  3. TESTS FRÁGILES                                              │
│     Si la BD está caída → todos los tests fallan                │
│     Si otro test modificó datos → resultados inconsistentes     │
│     Si la red tiene lag → tests intermitentemente lentos        │
│                                                                 │
│  4. TESTS NO AISLADOS                                           │
│     Test A inserta un usuario, Test B lo ve                     │
│     El orden de ejecución afecta los resultados                 │
│     Tests en paralelo colisionan                                │
│                                                                 │
│  5. DIFÍCIL SIMULAR ERRORES                                     │
│     ¿Cómo testeas "qué pasa si la BD retorna timeout"?         │
│     ¿Cómo testeas "qué pasa si el disco está lleno"?           │
│     Con la BD real es muy difícil provocar estos escenarios     │
│                                                                 │
│  6. CI COMPLICADO                                               │
│     El pipeline necesita servicios Docker extras                │
│     Más configuración, más puntos de fallo                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### La solución: invertir la dependencia

```
  ANTES (dependencia directa):
  
  UserService ───────→ PostgreSQL Client
  (lógica)              (infraestructura)
  
  "UserService SABE que usa PostgreSQL"
  "Para testear UserService, NECESITAS PostgreSQL"
  
  
  DESPUÉS (dependencia invertida):
  
  UserService ───────→ trait UserRepository
  (lógica)              (contrato abstracto)
                              │
                   ┌──────────┴──────────┐
                   │                     │
           PostgresUserRepo        MockUserRepo
           (producción)            (tests)
  
  "UserService solo SABE que tiene un UserRepository"
  "Para testear UserService, inyectas un MockUserRepo"
  "Para producción, inyectas un PostgresUserRepo"
```

---

## 2. Qué es Dependency Injection

**Dependency Injection (DI)** es un patrón donde un componente NO crea sus propias dependencias, sino que las RECIBE del exterior.

### Las tres formas de obtener una dependencia

```
  1. CREACIÓN DIRECTA (anti-patrón para testing):
     
     struct Service {
         fn new() {
             self.db = Database::connect("localhost");  // ← crea su propia dep
         }
     }
     
     
  2. INYECCIÓN POR CONSTRUCTOR (la más común):
     
     struct Service<D: Database> {
         db: D,
     }
     
     impl<D: Database> Service<D> {
         fn new(db: D) -> Self {           // ← recibe la dependencia
             Service { db }
         }
     }
     
     // En producción:
     let service = Service::new(PostgresDb::connect("..."));
     
     // En tests:
     let service = Service::new(MockDb::new());
     
     
  3. INYECCIÓN POR MÉTODO (ocasionalmente útil):
     
     impl Service {
         fn process(&self, db: &dyn Database) {   // ← recibe como argumento
             db.query("...");
         }
     }
```

### DI en Rust: traits como el mecanismo natural

```
  En Java/C#: interfaces + frameworks de DI (Spring, Guice)
  En Go: interfaces implícitas
  En Rust: TRAITS
  
  Los traits de Rust son ideales para DI porque:
  ├── Definen un contrato explícito (métodos requeridos)
  ├── Permiten múltiples implementaciones (real + mock)
  ├── Se pueden usar con genéricos (zero-cost) o trait objects (dinámico)
  ├── El compilador verifica que los contratos se cumplan
  └── No hay runtime overhead con genéricos (monomorphization)
```

---

## 3. Traits como contratos de dependencia

### El trait define QUÉ necesita el código, no CÓMO se implementa

```rust
// El trait es el CONTRATO: define las operaciones necesarias
pub trait UserRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<User>, RepositoryError>;
    fn create(&self, name: &str, email: &str) -> Result<i64, RepositoryError>;
    fn update(&self, user: &User) -> Result<(), RepositoryError>;
    fn delete(&self, id: i64) -> Result<bool, RepositoryError>;
    fn find_by_email(&self, email: &str) -> Result<Option<User>, RepositoryError>;
}

#[derive(Debug, Clone, PartialEq)]
pub struct User {
    pub id: i64,
    pub name: String,
    pub email: String,
}

#[derive(Debug)]
pub enum RepositoryError {
    NotFound,
    DuplicateEmail,
    ConnectionError(String),
    Unknown(String),
}
```

### Reglas para diseñar buenos traits de dependencia

```
┌─────────────────────────────────────────────────────────────────┐
│         REGLAS PARA TRAITS DE DEPENDENCIA                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. OPERACIONES, NO IMPLEMENTACIÓN                              │
│     ✅ fn find_by_id(&self, id: i64) -> Result<Option<User>>   │
│     ❌ fn execute_sql(&self, query: &str) -> Rows              │
│     El trait no debe exponer detalles de la implementación      │
│                                                                 │
│  2. GRANULARIDAD CORRECTA                                       │
│     ✅ Un trait por responsabilidad (UserRepo, EmailSender)     │
│     ❌ Un mega-trait con 30 métodos para todo                   │
│     Interface Segregation: mejor muchos traits pequeños         │
│                                                                 │
│  3. ERRORES PROPIOS                                             │
│     ✅ Result<T, RepositoryError> (error propio del dominio)    │
│     ❌ Result<T, postgres::Error> (acoplado a implementación)   │
│     Los errores no deben filtrar detalles de implementación     │
│                                                                 │
│  4. TIPOS DE DOMINIO                                            │
│     ✅ Parámetros y retornos son tipos del dominio (User, etc.) │
│     ❌ Parámetros tipo Row, Connection, Statement               │
│                                                                 │
│  5. &self O &mut self, NUNCA DATOS DE CONEXIÓN                  │
│     ✅ fn find(&self, id: i64) → ...                            │
│     ❌ fn find(&self, conn: &Connection, id: i64) → ...         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Patrón completo: trait + implementación real + mock

### Paso 1: Definir el trait

```rust
// src/repository.rs

#[derive(Debug, Clone, PartialEq)]
pub struct User {
    pub id: i64,
    pub name: String,
    pub email: String,
}

#[derive(Debug, PartialEq)]
pub enum RepoError {
    NotFound,
    Duplicate,
    Internal(String),
}

impl std::fmt::Display for RepoError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            RepoError::NotFound => write!(f, "not found"),
            RepoError::Duplicate => write!(f, "duplicate entry"),
            RepoError::Internal(msg) => write!(f, "internal error: {}", msg),
        }
    }
}

pub trait UserRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError>;
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError>;
    fn delete(&mut self, id: i64) -> Result<bool, RepoError>;
}
```

### Paso 2: Implementación real (producción)

```rust
// src/postgres_repo.rs

use crate::repository::{User, UserRepository, RepoError};

pub struct PostgresUserRepository {
    // En un proyecto real: pool de conexiones
    connection_string: String,
}

impl PostgresUserRepository {
    pub fn new(connection_string: &str) -> Self {
        PostgresUserRepository {
            connection_string: connection_string.to_string(),
        }
    }
}

impl UserRepository for PostgresUserRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError> {
        // Implementación real con PostgreSQL
        // let client = Client::connect(&self.connection_string, ...)?;
        // let row = client.query_one("SELECT ...", &[&id])?;
        // ...
        todo!("implementación real con PostgreSQL")
    }
    
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        // INSERT INTO users ...
        todo!("implementación real")
    }
    
    fn delete(&mut self, id: i64) -> Result<bool, RepoError> {
        // DELETE FROM users WHERE id = $1
        todo!("implementación real")
    }
}
```

### Paso 3: Mock para tests

```rust
// src/repository.rs (o en tests/)

#[cfg(test)]
pub mod mock {
    use super::*;
    use std::collections::HashMap;
    use std::sync::atomic::{AtomicI64, Ordering};
    
    pub struct MockUserRepository {
        users: HashMap<i64, User>,
        next_id: AtomicI64,
        emails: std::collections::HashSet<String>,
    }
    
    impl MockUserRepository {
        pub fn new() -> Self {
            MockUserRepository {
                users: HashMap::new(),
                next_id: AtomicI64::new(1),
                emails: std::collections::HashSet::new(),
            }
        }
        
        /// Pre-poblar con datos de prueba
        pub fn with_users(mut self, users: Vec<User>) -> Self {
            for user in users {
                self.emails.insert(user.email.clone());
                let id = user.id;
                self.users.insert(id, user);
                // Actualizar next_id si es necesario
                if id >= self.next_id.load(Ordering::Relaxed) {
                    self.next_id.store(id + 1, Ordering::Relaxed);
                }
            }
            self
        }
    }
    
    impl UserRepository for MockUserRepository {
        fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError> {
            Ok(self.users.get(&id).cloned())
        }
        
        fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
            if self.emails.contains(email) {
                return Err(RepoError::Duplicate);
            }
            let id = self.next_id.fetch_add(1, Ordering::Relaxed);
            let user = User {
                id,
                name: name.to_string(),
                email: email.to_string(),
            };
            self.emails.insert(email.to_string());
            self.users.insert(id, user);
            Ok(id)
        }
        
        fn delete(&mut self, id: i64) -> Result<bool, RepoError> {
            if let Some(user) = self.users.remove(&id) {
                self.emails.remove(&user.email);
                Ok(true)
            } else {
                Ok(false)
            }
        }
    }
}
```

### Paso 4: El servicio usa el trait (no la implementación concreta)

```rust
// src/service.rs

use crate::repository::{User, UserRepository, RepoError};

pub struct UserService<R: UserRepository> {
    repo: R,
}

impl<R: UserRepository> UserService<R> {
    pub fn new(repo: R) -> Self {
        UserService { repo }
    }
    
    pub fn register_user(&mut self, name: &str, email: &str) -> Result<User, String> {
        // Lógica de negocio: validación
        let name = name.trim();
        if name.is_empty() {
            return Err("name cannot be empty".to_string());
        }
        if name.len() > 100 {
            return Err("name too long".to_string());
        }
        
        let email = email.trim().to_lowercase();
        if !email.contains('@') || !email.contains('.') {
            return Err("invalid email format".to_string());
        }
        
        // Delegar al repositorio
        let id = self.repo.create(name, &email)
            .map_err(|e| match e {
                RepoError::Duplicate => "email already registered".to_string(),
                other => format!("failed to create user: {}", other),
            })?;
        
        Ok(User {
            id,
            name: name.to_string(),
            email,
        })
    }
    
    pub fn get_user(&self, id: i64) -> Result<User, String> {
        self.repo.find_by_id(id)
            .map_err(|e| format!("repository error: {}", e))?
            .ok_or_else(|| format!("user {} not found", id))
    }
    
    pub fn delete_user(&mut self, id: i64) -> Result<bool, String> {
        self.repo.delete(id)
            .map_err(|e| format!("failed to delete: {}", e))
    }
}
```

### Paso 5: Tests usando el mock

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use crate::repository::mock::MockUserRepository;
    
    fn setup() -> UserService<MockUserRepository> {
        UserService::new(MockUserRepository::new())
    }
    
    fn setup_with_users() -> UserService<MockUserRepository> {
        let repo = MockUserRepository::new().with_users(vec![
            User { id: 1, name: "Alice".to_string(), email: "alice@example.com".to_string() },
            User { id: 2, name: "Bob".to_string(), email: "bob@example.com".to_string() },
        ]);
        UserService::new(repo)
    }
    
    #[test]
    fn register_valid_user() {
        let mut service = setup();
        let user = service.register_user("Alice", "alice@example.com").unwrap();
        assert_eq!(user.name, "Alice");
        assert_eq!(user.email, "alice@example.com");
        assert!(user.id > 0);
    }
    
    #[test]
    fn register_empty_name_fails() {
        let mut service = setup();
        let result = service.register_user("", "alice@example.com");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("empty"));
    }
    
    #[test]
    fn register_invalid_email_fails() {
        let mut service = setup();
        let result = service.register_user("Alice", "not-an-email");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("invalid email"));
    }
    
    #[test]
    fn register_duplicate_email_fails() {
        let mut service = setup();
        service.register_user("Alice", "alice@example.com").unwrap();
        let result = service.register_user("Bob", "alice@example.com");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("already registered"));
    }
    
    #[test]
    fn get_existing_user() {
        let service = setup_with_users();
        let user = service.get_user(1).unwrap();
        assert_eq!(user.name, "Alice");
    }
    
    #[test]
    fn get_nonexistent_user_fails() {
        let service = setup_with_users();
        let result = service.get_user(999);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("not found"));
    }
    
    #[test]
    fn delete_existing_user() {
        let mut service = setup_with_users();
        assert!(service.delete_user(1).unwrap());
    }
    
    #[test]
    fn delete_nonexistent_user() {
        let mut service = setup_with_users();
        assert!(!service.delete_user(999).unwrap());
    }
    
    #[test]
    fn email_is_normalized_to_lowercase() {
        let mut service = setup();
        let user = service.register_user("Alice", "Alice@EXAMPLE.COM").unwrap();
        assert_eq!(user.email, "alice@example.com");
    }
    
    #[test]
    fn name_is_trimmed() {
        let mut service = setup();
        let user = service.register_user("  Alice  ", "alice@example.com").unwrap();
        assert_eq!(user.name, "Alice");
    }
}
```

### Diagrama del flujo completo

```
  ┌──────────────────────────────────────────────────────────────┐
  │                    PATRÓN COMPLETO                           │
  │                                                              │
  │  trait UserRepository          ← contrato abstracto          │
  │       │                                                      │
  │       ├── PostgresUserRepo     ← implementación real         │
  │       │   (usa BD real)           (producción)               │
  │       │                                                      │
  │       └── MockUserRepository   ← implementación de test      │
  │           (usa HashMap)           (tests)                    │
  │                                                              │
  │  UserService<R: UserRepository> ← lógica de negocio          │
  │       │                           (genérico sobre R)         │
  │       │                                                      │
  │       ├── Producción: UserService<PostgresUserRepo>          │
  │       │   service = UserService::new(PostgresUserRepo::new())│
  │       │                                                      │
  │       └── Tests: UserService<MockUserRepository>             │
  │           service = UserService::new(MockUserRepository::new)│
  │                                                              │
  │  Los tests verifican la LÓGICA DE NEGOCIO                    │
  │  sin necesitar infraestructura real.                          │
  └──────────────────────────────────────────────────────────────┘
```

---

## 5. Inyección por genéricos vs por trait objects

Rust ofrece dos formas de usar traits como dependencias: **genéricos** (estáticos, monomorphization) y **trait objects** (dinámicos, vtable).

### Genéricos: `<R: UserRepository>`

```rust
// El tipo concreto se resuelve en COMPILACIÓN
pub struct UserService<R: UserRepository> {
    repo: R,
}

impl<R: UserRepository> UserService<R> {
    pub fn new(repo: R) -> Self {
        UserService { repo }
    }
    
    pub fn get_user(&self, id: i64) -> Result<User, String> {
        // El compilador sabe exactamente qué tipo es R
        // → llamada directa (no virtual), inlining posible
        self.repo.find_by_id(id)
            .map_err(|e| format!("{}", e))?
            .ok_or_else(|| "not found".to_string())
    }
}

// En producción: el compilador genera UserService<PostgresUserRepo>
let service = UserService::new(PostgresUserRepo::new("..."));

// En tests: el compilador genera UserService<MockUserRepository>
let service = UserService::new(MockUserRepository::new());
```

### Trait objects: `dyn UserRepository`

```rust
// El tipo concreto se resuelve en EJECUCIÓN
pub struct UserService {
    repo: Box<dyn UserRepository>,
}

impl UserService {
    pub fn new(repo: Box<dyn UserRepository>) -> Self {
        UserService { repo }
    }
    
    pub fn get_user(&self, id: i64) -> Result<User, String> {
        // Llamada virtual a través de vtable (indirección)
        self.repo.find_by_id(id)
            .map_err(|e| format!("{}", e))?
            .ok_or_else(|| "not found".to_string())
    }
}

// En producción:
let service = UserService::new(Box::new(PostgresUserRepo::new("...")));

// En tests:
let service = UserService::new(Box::new(MockUserRepository::new()));
```

### Comparación

```
┌──────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto              │ Genéricos <R: Trait>     │ Trait objects dyn Trait   │
├──────────────────────┼──────────────────────────┼──────────────────────────┤
│ Resolución           │ Compilación (estático)   │ Ejecución (dinámico)     │
│ Performance          │ Zero-cost (inlining)     │ Indirección vtable       │
│ Tamaño binario       │ Mayor (monomorphization) │ Menor                    │
│ Flexibilidad         │ Tipo fijo después de     │ Puede cambiar en runtime │
│                      │ construir                │                          │
│ Sintaxis             │ struct<R: Trait>          │ Box<dyn Trait>           │
│ Object safety        │ No requiere              │ Requiere object safety   │
│ Ergonomía para tests │ Excelente                │ Buena                    │
│                      │                          │                          │
│ RECOMENDACIÓN        │ DEFAULT para la mayoría  │ Cuando necesitas         │
│                      │ de casos                 │ polimorfismo en runtime  │
│                      │                          │ o colecciones de traits  │
└──────────────────────┴──────────────────────────┴──────────────────────────┘
```

### Cuándo usar cada uno

```
  GENÉRICOS (preferir por defecto):
  ├── La dependencia se conoce en compilación
  ├── Solo hay una dependencia del mismo tipo
  ├── Quieres máxima performance
  └── Ejemplo: Service<R: Repository>
  
  TRAIT OBJECTS (cuando genéricos no bastan):
  ├── Necesitas una colección de implementaciones diferentes
  │   Vec<Box<dyn Handler>>
  ├── Necesitas cambiar la implementación en runtime
  │   (plugins, configuración dinámica)
  ├── El trait no es object-safe con genéricos
  │   (métodos con Self en retorno necesitan workarounds)
  └── Quieres reducir tamaño del binario
```

---

## 6. Inyección por constructor (lo más común)

El patrón más idiomático en Rust: la dependencia se pasa al constructor (`new()`).

```rust
pub trait EmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String>;
}

pub trait Logger {
    fn info(&self, message: &str);
    fn error(&self, message: &str);
}

// Servicio con DOS dependencias inyectadas por constructor
pub struct NotificationService<E: EmailSender, L: Logger> {
    email: E,
    logger: L,
}

impl<E: EmailSender, L: Logger> NotificationService<E, L> {
    // Constructor recibe las dependencias
    pub fn new(email: E, logger: L) -> Self {
        NotificationService { email, logger }
    }
    
    pub fn notify_user(&self, email: &str, message: &str) -> Result<(), String> {
        self.logger.info(&format!("Sending notification to {}", email));
        
        self.email.send(email, "Notification", message)
            .map_err(|e| {
                self.logger.error(&format!("Failed to send to {}: {}", email, e));
                e
            })
    }
}
```

### Producción vs Test

```rust
// ── Producción ───────────────────────────────────────────────

struct SmtpEmailSender { /* configuración SMTP */ }
impl EmailSender for SmtpEmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        // Enviar email real por SMTP
        todo!()
    }
}

struct StdoutLogger;
impl Logger for StdoutLogger {
    fn info(&self, message: &str) { println!("[INFO] {}", message); }
    fn error(&self, message: &str) { eprintln!("[ERROR] {}", message); }
}

// let service = NotificationService::new(SmtpEmailSender::new(), StdoutLogger);


// ── Tests ────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    
    struct FakeEmailSender {
        sent: RefCell<Vec<(String, String, String)>>,
        should_fail: bool,
    }
    
    impl FakeEmailSender {
        fn new() -> Self {
            FakeEmailSender {
                sent: RefCell::new(Vec::new()),
                should_fail: false,
            }
        }
        fn failing() -> Self {
            FakeEmailSender {
                sent: RefCell::new(Vec::new()),
                should_fail: true,
            }
        }
        fn sent_count(&self) -> usize {
            self.sent.borrow().len()
        }
    }
    
    impl EmailSender for FakeEmailSender {
        fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
            if self.should_fail {
                return Err("SMTP connection refused".to_string());
            }
            self.sent.borrow_mut().push((
                to.to_string(),
                subject.to_string(),
                body.to_string(),
            ));
            Ok(())
        }
    }
    
    struct NullLogger;
    impl Logger for NullLogger {
        fn info(&self, _: &str) {}
        fn error(&self, _: &str) {}
    }
    
    #[test]
    fn sends_notification() {
        let email = FakeEmailSender::new();
        let service = NotificationService::new(email, NullLogger);
        
        service.notify_user("alice@example.com", "Hello!").unwrap();
        // No podemos acceder a email aquí porque se movió a service
        // Solución: usar Rc<RefCell<>> o verificar efectos de otra forma
    }
    
    #[test]
    fn handles_email_failure() {
        let service = NotificationService::new(
            FakeEmailSender::failing(),
            NullLogger,
        );
        
        let result = service.notify_user("alice@example.com", "Hello!");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("SMTP"));
    }
}
```

---

## 7. Inyección por método (menos común)

En lugar de pasar la dependencia al constructor, se pasa como argumento a cada método que la necesita.

```rust
// La dependencia se pasa al método, no al constructor
pub struct ReportGenerator;

impl ReportGenerator {
    pub fn generate<R: UserRepository>(
        &self,
        repo: &R,     // ← dependencia como argumento
        user_id: i64,
    ) -> Result<String, String> {
        let user = repo.find_by_id(user_id)
            .map_err(|e| format!("{}", e))?
            .ok_or("user not found")?;
        
        Ok(format!("Report for: {} ({})", user.name, user.email))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn generates_report() {
        let repo = MockUserRepository::new().with_users(vec![
            User { id: 1, name: "Alice".into(), email: "alice@test.com".into() },
        ]);
        let generator = ReportGenerator;
        
        let report = generator.generate(&repo, 1).unwrap();
        assert!(report.contains("Alice"));
    }
}
```

### Cuándo usar inyección por método

```
  POR CONSTRUCTOR (preferir):
  ├── La dependencia se usa en múltiples métodos
  ├── La dependencia no cambia entre llamadas
  ├── Configuración inicial pesada (pool de conexiones)
  └── Es "la regla" — úsalo por defecto
  
  POR MÉTODO (ocasionalmente):
  ├── La dependencia solo se usa en UN método
  ├── La dependencia puede cambiar entre llamadas
  ├── Funciones utilitarias stateless
  └── Cuando agregar un genérico al struct es excesivo
```

---

## 8. Diseñar traits testeables: principios

### Principio 1: Interface Segregation — traits pequeños y enfocados

```rust
// ❌ MALO: un mega-trait con todo
pub trait Database {
    fn query_users(&self) -> Vec<User>;
    fn insert_user(&self, user: &User) -> i64;
    fn query_products(&self) -> Vec<Product>;
    fn insert_product(&self, product: &Product) -> i64;
    fn query_orders(&self) -> Vec<Order>;
    fn insert_order(&self, order: &Order) -> i64;
    fn begin_transaction(&self);
    fn commit(&self);
    fn rollback(&self);
    fn execute_raw_sql(&self, sql: &str) -> Vec<Row>;
    // ... 20 métodos más
}
// El mock necesita implementar los 20+ métodos aunque solo uses 2

// ✅ BUENO: traits separados por responsabilidad
pub trait UserRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError>;
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError>;
}

pub trait ProductRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<Product>, RepoError>;
    fn list_all(&self) -> Result<Vec<Product>, RepoError>;
}

pub trait OrderRepository {
    fn create(&mut self, order: &NewOrder) -> Result<i64, RepoError>;
    fn find_by_user(&self, user_id: i64) -> Result<Vec<Order>, RepoError>;
}
// Cada mock solo implementa lo que necesita
```

### Principio 2: Depender del dominio, no de la infraestructura

```rust
// ❌ MALO: el trait expone detalles de implementación
pub trait DataStore {
    fn execute_query(&self, sql: &str, params: &[&dyn ToSql]) -> Vec<Row>;
    fn get_connection(&self) -> &Connection;
}

// ✅ BUENO: el trait habla en términos del dominio
pub trait UserRepository {
    fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError>;
    fn find_active_users(&self) -> Result<Vec<User>, RepoError>;
    fn count_by_role(&self, role: &str) -> Result<usize, RepoError>;
}
```

### Principio 3: Errores del dominio, no de la implementación

```rust
// ❌ MALO: error acoplado a PostgreSQL
pub trait UserRepository {
    fn find(&self, id: i64) -> Result<User, postgres::Error>;
    //                                      ^^^^^^^^^^^^^^^ dependencia filtrada
}

// ✅ BUENO: error propio del dominio
#[derive(Debug)]
pub enum RepoError {
    NotFound,
    Duplicate,
    Internal(String),
}

pub trait UserRepository {
    fn find(&self, id: i64) -> Result<Option<User>, RepoError>;
    //                                               ^^^^^^^^^ error del dominio
}

// La implementación de PostgreSQL convierte errores internamente:
impl UserRepository for PostgresRepo {
    fn find(&self, id: i64) -> Result<Option<User>, RepoError> {
        self.client.query_opt("...", &[&id])
            .map(|opt_row| opt_row.map(|r| User { /* ... */ }))
            .map_err(|e| RepoError::Internal(e.to_string()))
            //       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            //       convierte postgres::Error → RepoError
    }
}
```

### Principio 4: No asumir estado mutable si no es necesario

```rust
// ❌ Forzar &mut self cuando &self basta
pub trait Cache {
    fn get(&mut self, key: &str) -> Option<String>;  // ¿por qué mut?
}
// Un mock inmutable es más simple que uno mutable

// ✅ &self cuando la operación no muta estado lógico
pub trait Cache {
    fn get(&self, key: &str) -> Option<String>;
    fn set(&self, key: &str, value: &str);  // &self con interior mutability
}
// En la implementación real, usar Mutex/RwLock internamente
// En el mock, usar RefCell
```

---

## 9. Ejemplo real: servicio con base de datos

```rust
// ── Trait ─────────────────────────────────────────────────────

pub trait TodoRepository {
    fn list(&self) -> Result<Vec<Todo>, RepoError>;
    fn get(&self, id: i64) -> Result<Option<Todo>, RepoError>;
    fn create(&mut self, title: &str) -> Result<Todo, RepoError>;
    fn complete(&mut self, id: i64) -> Result<bool, RepoError>;
    fn delete(&mut self, id: i64) -> Result<bool, RepoError>;
}

#[derive(Debug, Clone, PartialEq)]
pub struct Todo {
    pub id: i64,
    pub title: String,
    pub completed: bool,
}

// ── Servicio (lógica de negocio) ─────────────────────────────

pub struct TodoService<R: TodoRepository> {
    repo: R,
}

impl<R: TodoRepository> TodoService<R> {
    pub fn new(repo: R) -> Self {
        TodoService { repo }
    }
    
    pub fn add_todo(&mut self, title: &str) -> Result<Todo, String> {
        let title = title.trim();
        if title.is_empty() {
            return Err("title cannot be empty".into());
        }
        if title.len() > 200 {
            return Err("title too long (max 200 chars)".into());
        }
        
        self.repo.create(title)
            .map_err(|e| format!("failed to create: {}", e))
    }
    
    pub fn complete_todo(&mut self, id: i64) -> Result<(), String> {
        let found = self.repo.complete(id)
            .map_err(|e| format!("failed to complete: {}", e))?;
        if !found {
            return Err(format!("todo {} not found", id));
        }
        Ok(())
    }
    
    pub fn pending_todos(&self) -> Result<Vec<Todo>, String> {
        let todos = self.repo.list()
            .map_err(|e| format!("failed to list: {}", e))?;
        Ok(todos.into_iter().filter(|t| !t.completed).collect())
    }
    
    pub fn stats(&self) -> Result<(usize, usize), String> {
        let todos = self.repo.list()
            .map_err(|e| format!("{}", e))?;
        let total = todos.len();
        let completed = todos.iter().filter(|t| t.completed).count();
        Ok((total, completed))
    }
}

// ── Mock ──────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    
    struct MockTodoRepo {
        todos: HashMap<i64, Todo>,
        next_id: i64,
    }
    
    impl MockTodoRepo {
        fn new() -> Self {
            MockTodoRepo { todos: HashMap::new(), next_id: 1 }
        }
    }
    
    impl TodoRepository for MockTodoRepo {
        fn list(&self) -> Result<Vec<Todo>, RepoError> {
            let mut v: Vec<Todo> = self.todos.values().cloned().collect();
            v.sort_by_key(|t| t.id);
            Ok(v)
        }
        fn get(&self, id: i64) -> Result<Option<Todo>, RepoError> {
            Ok(self.todos.get(&id).cloned())
        }
        fn create(&mut self, title: &str) -> Result<Todo, RepoError> {
            let todo = Todo {
                id: self.next_id,
                title: title.to_string(),
                completed: false,
            };
            self.next_id += 1;
            self.todos.insert(todo.id, todo.clone());
            Ok(todo)
        }
        fn complete(&mut self, id: i64) -> Result<bool, RepoError> {
            if let Some(todo) = self.todos.get_mut(&id) {
                todo.completed = true;
                Ok(true)
            } else {
                Ok(false)
            }
        }
        fn delete(&mut self, id: i64) -> Result<bool, RepoError> {
            Ok(self.todos.remove(&id).is_some())
        }
    }
    
    #[test]
    fn add_valid_todo() {
        let mut service = TodoService::new(MockTodoRepo::new());
        let todo = service.add_todo("Buy milk").unwrap();
        assert_eq!(todo.title, "Buy milk");
        assert!(!todo.completed);
    }
    
    #[test]
    fn add_empty_title_fails() {
        let mut service = TodoService::new(MockTodoRepo::new());
        assert!(service.add_todo("").is_err());
        assert!(service.add_todo("   ").is_err());
    }
    
    #[test]
    fn add_long_title_fails() {
        let mut service = TodoService::new(MockTodoRepo::new());
        let long = "a".repeat(201);
        assert!(service.add_todo(&long).is_err());
    }
    
    #[test]
    fn complete_existing_todo() {
        let mut service = TodoService::new(MockTodoRepo::new());
        let todo = service.add_todo("Test").unwrap();
        service.complete_todo(todo.id).unwrap();
    }
    
    #[test]
    fn complete_nonexistent_fails() {
        let mut service = TodoService::new(MockTodoRepo::new());
        assert!(service.complete_todo(999).is_err());
    }
    
    #[test]
    fn pending_excludes_completed() {
        let mut service = TodoService::new(MockTodoRepo::new());
        let t1 = service.add_todo("Task 1").unwrap();
        service.add_todo("Task 2").unwrap();
        service.complete_todo(t1.id).unwrap();
        
        let pending = service.pending_todos().unwrap();
        assert_eq!(pending.len(), 1);
        assert_eq!(pending[0].title, "Task 2");
    }
    
    #[test]
    fn stats_counts_correctly() {
        let mut service = TodoService::new(MockTodoRepo::new());
        service.add_todo("A").unwrap();
        let b = service.add_todo("B").unwrap();
        service.add_todo("C").unwrap();
        service.complete_todo(b.id).unwrap();
        
        let (total, completed) = service.stats().unwrap();
        assert_eq!(total, 3);
        assert_eq!(completed, 1);
    }
}
```

---

## 10. Ejemplo real: servicio con HTTP client

```rust
pub trait HttpClient {
    fn get(&self, url: &str) -> Result<HttpResponse, HttpError>;
    fn post(&self, url: &str, body: &str) -> Result<HttpResponse, HttpError>;
}

#[derive(Debug)]
pub struct HttpResponse {
    pub status: u16,
    pub body: String,
}

#[derive(Debug)]
pub enum HttpError {
    Timeout,
    ConnectionRefused,
    InvalidUrl(String),
    Other(String),
}

impl std::fmt::Display for HttpError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            HttpError::Timeout => write!(f, "request timed out"),
            HttpError::ConnectionRefused => write!(f, "connection refused"),
            HttpError::InvalidUrl(u) => write!(f, "invalid URL: {}", u),
            HttpError::Other(msg) => write!(f, "{}", msg),
        }
    }
}

// ── Servicio que consume una API externa ─────────────────────

pub struct WeatherService<C: HttpClient> {
    client: C,
    api_base: String,
}

impl<C: HttpClient> WeatherService<C> {
    pub fn new(client: C, api_base: &str) -> Self {
        WeatherService {
            client,
            api_base: api_base.to_string(),
        }
    }
    
    pub fn get_temperature(&self, city: &str) -> Result<f64, String> {
        let url = format!("{}/weather?city={}", self.api_base, city);
        let response = self.client.get(&url)
            .map_err(|e| format!("HTTP error: {}", e))?;
        
        if response.status != 200 {
            return Err(format!("API returned status {}", response.status));
        }
        
        // Parse simple: esperamos un JSON como {"temp": 22.5}
        self.parse_temperature(&response.body)
    }
    
    fn parse_temperature(&self, body: &str) -> Result<f64, String> {
        // Parsing simplificado para el ejemplo
        let body = body.trim();
        if let Some(start) = body.find("\"temp\":") {
            let rest = &body[start + 7..];
            let end = rest.find(|c: char| !c.is_numeric() && c != '.' && c != '-')
                .unwrap_or(rest.len());
            rest[..end].trim().parse::<f64>()
                .map_err(|e| format!("parse error: {}", e))
        } else {
            Err("temperature not found in response".to_string())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    struct MockHttpClient {
        responses: std::collections::HashMap<String, Result<HttpResponse, HttpError>>,
    }
    
    impl MockHttpClient {
        fn new() -> Self {
            MockHttpClient { responses: std::collections::HashMap::new() }
        }
        
        fn respond(mut self, url: &str, response: HttpResponse) -> Self {
            self.responses.insert(url.to_string(), Ok(response));
            self
        }
        
        fn fail(mut self, url: &str, error: HttpError) -> Self {
            self.responses.insert(url.to_string(), Err(error));
            self
        }
    }
    
    impl HttpClient for MockHttpClient {
        fn get(&self, url: &str) -> Result<HttpResponse, HttpError> {
            self.responses.get(url)
                .cloned()
                .unwrap_or(Err(HttpError::Other(format!("unexpected URL: {}", url))))
        }
        fn post(&self, _url: &str, _body: &str) -> Result<HttpResponse, HttpError> {
            Err(HttpError::Other("not configured".into()))
        }
    }
    
    // Necesitamos Clone para HttpResponse y HttpError
    impl Clone for HttpResponse {
        fn clone(&self) -> Self {
            HttpResponse { status: self.status, body: self.body.clone() }
        }
    }
    impl Clone for HttpError {
        fn clone(&self) -> Self {
            match self {
                HttpError::Timeout => HttpError::Timeout,
                HttpError::ConnectionRefused => HttpError::ConnectionRefused,
                HttpError::InvalidUrl(u) => HttpError::InvalidUrl(u.clone()),
                HttpError::Other(m) => HttpError::Other(m.clone()),
            }
        }
    }
    
    #[test]
    fn successful_weather_request() {
        let client = MockHttpClient::new()
            .respond(
                "https://api.weather.test/weather?city=Madrid",
                HttpResponse { status: 200, body: r#"{"temp":22.5}"#.into() },
            );
        
        let service = WeatherService::new(client, "https://api.weather.test");
        let temp = service.get_temperature("Madrid").unwrap();
        assert!((temp - 22.5).abs() < 0.001);
    }
    
    #[test]
    fn api_returns_error_status() {
        let client = MockHttpClient::new()
            .respond(
                "https://api.weather.test/weather?city=Unknown",
                HttpResponse { status: 404, body: "not found".into() },
            );
        
        let service = WeatherService::new(client, "https://api.weather.test");
        let result = service.get_temperature("Unknown");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("404"));
    }
    
    #[test]
    fn network_timeout() {
        let client = MockHttpClient::new()
            .fail("https://api.weather.test/weather?city=Madrid", HttpError::Timeout);
        
        let service = WeatherService::new(client, "https://api.weather.test");
        let result = service.get_temperature("Madrid");
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("timed out"));
    }
    
    #[test]
    fn malformed_response() {
        let client = MockHttpClient::new()
            .respond(
                "https://api.weather.test/weather?city=Madrid",
                HttpResponse { status: 200, body: "not json".into() },
            );
        
        let service = WeatherService::new(client, "https://api.weather.test");
        let result = service.get_temperature("Madrid");
        assert!(result.is_err());
    }
}
```

---

## 11. Ejemplo real: servicio con reloj (tiempo)

Inyectar el tiempo permite testear lógica temporal sin esperar o depender de `SystemTime::now()`.

```rust
use std::time::{Duration, SystemTime, UNIX_EPOCH};

// ── Trait para el reloj ──────────────────────────────────────

pub trait Clock {
    fn now(&self) -> SystemTime;
}

// Implementación real
pub struct SystemClock;

impl Clock for SystemClock {
    fn now(&self) -> SystemTime {
        SystemTime::now()
    }
}

// Mock: reloj controlable
pub struct MockClock {
    time: std::cell::Cell<SystemTime>,
}

impl MockClock {
    pub fn new(time: SystemTime) -> Self {
        MockClock { time: std::cell::Cell::new(time) }
    }
    
    pub fn from_secs(secs: u64) -> Self {
        MockClock::new(UNIX_EPOCH + Duration::from_secs(secs))
    }
    
    pub fn advance(&self, duration: Duration) {
        let current = self.time.get();
        self.time.set(current + duration);
    }
}

impl Clock for MockClock {
    fn now(&self) -> SystemTime {
        self.time.get()
    }
}

// ── Rate limiter con reloj inyectable ────────────────────────

use std::collections::HashMap;

pub struct RateLimiter<C: Clock> {
    clock: C,
    max_requests: usize,
    window: Duration,
    requests: HashMap<String, Vec<SystemTime>>,
}

impl<C: Clock> RateLimiter<C> {
    pub fn new(clock: C, max_requests: usize, window: Duration) -> Self {
        RateLimiter {
            clock,
            max_requests,
            window,
            requests: HashMap::new(),
        }
    }
    
    pub fn allow(&mut self, user: &str) -> bool {
        let now = self.clock.now();
        let window_start = now - self.window;
        
        let timestamps = self.requests.entry(user.to_string()).or_default();
        
        // Eliminar timestamps fuera de la ventana
        timestamps.retain(|&t| t >= window_start);
        
        if timestamps.len() < self.max_requests {
            timestamps.push(now);
            true
        } else {
            false
        }
    }
    
    pub fn remaining(&self, user: &str) -> usize {
        let now = self.clock.now();
        let window_start = now - self.window;
        
        let count = self.requests.get(user)
            .map(|ts| ts.iter().filter(|&&t| t >= window_start).count())
            .unwrap_or(0);
        
        self.max_requests.saturating_sub(count)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn allows_within_limit() {
        let clock = MockClock::from_secs(1000);
        let mut limiter = RateLimiter::new(clock, 3, Duration::from_secs(60));
        
        assert!(limiter.allow("alice"));
        assert!(limiter.allow("alice"));
        assert!(limiter.allow("alice"));
    }
    
    #[test]
    fn blocks_over_limit() {
        let clock = MockClock::from_secs(1000);
        let mut limiter = RateLimiter::new(clock, 2, Duration::from_secs(60));
        
        assert!(limiter.allow("alice"));
        assert!(limiter.allow("alice"));
        assert!(!limiter.allow("alice"));  // bloqueado
    }
    
    #[test]
    fn resets_after_window() {
        let clock = MockClock::from_secs(1000);
        let mut limiter = RateLimiter::new(clock, 2, Duration::from_secs(60));
        
        assert!(limiter.allow("alice"));
        assert!(limiter.allow("alice"));
        assert!(!limiter.allow("alice"));  // bloqueado
        
        // Avanzar el reloj 61 segundos
        limiter.clock.advance(Duration::from_secs(61));
        
        assert!(limiter.allow("alice"));  // permitido de nuevo
    }
    
    #[test]
    fn users_are_independent() {
        let clock = MockClock::from_secs(1000);
        let mut limiter = RateLimiter::new(clock, 1, Duration::from_secs(60));
        
        assert!(limiter.allow("alice"));
        assert!(!limiter.allow("alice"));  // alice bloqueada
        assert!(limiter.allow("bob"));     // bob independiente
    }
    
    #[test]
    fn remaining_count_correct() {
        let clock = MockClock::from_secs(1000);
        let mut limiter = RateLimiter::new(clock, 5, Duration::from_secs(60));
        
        assert_eq!(limiter.remaining("alice"), 5);
        limiter.allow("alice");
        assert_eq!(limiter.remaining("alice"), 4);
        limiter.allow("alice");
        limiter.allow("alice");
        assert_eq!(limiter.remaining("alice"), 2);
    }
}
```

---

## 12. Ejemplo real: servicio con sistema de archivos

```rust
use std::path::Path;

pub trait FileSystem {
    fn read_to_string(&self, path: &Path) -> Result<String, std::io::Error>;
    fn write(&self, path: &Path, contents: &str) -> Result<(), std::io::Error>;
    fn exists(&self, path: &Path) -> bool;
    fn remove(&self, path: &Path) -> Result<(), std::io::Error>;
}

// Implementación real
pub struct RealFileSystem;

impl FileSystem for RealFileSystem {
    fn read_to_string(&self, path: &Path) -> Result<String, std::io::Error> {
        std::fs::read_to_string(path)
    }
    fn write(&self, path: &Path, contents: &str) -> Result<(), std::io::Error> {
        std::fs::write(path, contents)
    }
    fn exists(&self, path: &Path) -> bool {
        path.exists()
    }
    fn remove(&self, path: &Path) -> Result<(), std::io::Error> {
        std::fs::remove_file(path)
    }
}

// Mock: filesystem en memoria
pub struct InMemoryFileSystem {
    files: std::cell::RefCell<std::collections::HashMap<String, String>>,
}

impl InMemoryFileSystem {
    pub fn new() -> Self {
        InMemoryFileSystem {
            files: std::cell::RefCell::new(std::collections::HashMap::new()),
        }
    }
}

impl FileSystem for InMemoryFileSystem {
    fn read_to_string(&self, path: &Path) -> Result<String, std::io::Error> {
        self.files.borrow().get(path.to_str().unwrap())
            .cloned()
            .ok_or_else(|| std::io::Error::new(
                std::io::ErrorKind::NotFound,
                format!("file not found: {:?}", path),
            ))
    }
    
    fn write(&self, path: &Path, contents: &str) -> Result<(), std::io::Error> {
        self.files.borrow_mut()
            .insert(path.to_str().unwrap().to_string(), contents.to_string());
        Ok(())
    }
    
    fn exists(&self, path: &Path) -> bool {
        self.files.borrow().contains_key(path.to_str().unwrap())
    }
    
    fn remove(&self, path: &Path) -> Result<(), std::io::Error> {
        self.files.borrow_mut()
            .remove(path.to_str().unwrap())
            .ok_or_else(|| std::io::Error::new(
                std::io::ErrorKind::NotFound,
                "file not found",
            ))?;
        Ok(())
    }
}

// ── Servicio de configuración ────────────────────────────────

pub struct ConfigService<F: FileSystem> {
    fs: F,
    config_path: String,
}

impl<F: FileSystem> ConfigService<F> {
    pub fn new(fs: F, config_path: &str) -> Self {
        ConfigService {
            fs,
            config_path: config_path.to_string(),
        }
    }
    
    pub fn get(&self, key: &str) -> Result<Option<String>, String> {
        let path = Path::new(&self.config_path);
        if !self.fs.exists(path) {
            return Ok(None);
        }
        let contents = self.fs.read_to_string(path)
            .map_err(|e| format!("failed to read config: {}", e))?;
        
        for line in contents.lines() {
            if let Some((k, v)) = line.split_once('=') {
                if k.trim() == key {
                    return Ok(Some(v.trim().to_string()));
                }
            }
        }
        Ok(None)
    }
    
    pub fn set(&self, key: &str, value: &str) -> Result<(), String> {
        let path = Path::new(&self.config_path);
        let mut lines: Vec<String> = if self.fs.exists(path) {
            self.fs.read_to_string(path)
                .map_err(|e| e.to_string())?
                .lines()
                .map(|l| l.to_string())
                .collect()
        } else {
            Vec::new()
        };
        
        let mut found = false;
        for line in &mut lines {
            if let Some((k, _)) = line.split_once('=') {
                if k.trim() == key {
                    *line = format!("{} = {}", key, value);
                    found = true;
                    break;
                }
            }
        }
        if !found {
            lines.push(format!("{} = {}", key, value));
        }
        
        self.fs.write(path, &lines.join("\n"))
            .map_err(|e| e.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn get_nonexistent_config() {
        let fs = InMemoryFileSystem::new();
        let config = ConfigService::new(fs, "/app/config.ini");
        assert_eq!(config.get("key").unwrap(), None);
    }
    
    #[test]
    fn set_and_get_value() {
        let fs = InMemoryFileSystem::new();
        let config = ConfigService::new(fs, "/app/config.ini");
        config.set("db_host", "localhost").unwrap();
        assert_eq!(config.get("db_host").unwrap(), Some("localhost".into()));
    }
    
    #[test]
    fn update_existing_value() {
        let fs = InMemoryFileSystem::new();
        let config = ConfigService::new(fs, "/app/config.ini");
        config.set("port", "8080").unwrap();
        config.set("port", "9090").unwrap();
        assert_eq!(config.get("port").unwrap(), Some("9090".into()));
    }
    
    #[test]
    fn multiple_keys() {
        let fs = InMemoryFileSystem::new();
        let config = ConfigService::new(fs, "/app/config.ini");
        config.set("host", "localhost").unwrap();
        config.set("port", "8080").unwrap();
        config.set("debug", "true").unwrap();
        
        assert_eq!(config.get("host").unwrap(), Some("localhost".into()));
        assert_eq!(config.get("port").unwrap(), Some("8080".into()));
        assert_eq!(config.get("debug").unwrap(), Some("true".into()));
        assert_eq!(config.get("missing").unwrap(), None);
    }
}
```

---

## 13. Múltiples dependencias: composición

Cuando un servicio tiene más de una dependencia, cada una se inyecta como parámetro genérico o trait object separado.

```rust
pub struct OrderService<R, E, C>
where
    R: OrderRepository,
    E: EmailSender,
    C: Clock,
{
    repo: R,
    email: E,
    clock: C,
}

impl<R: OrderRepository, E: EmailSender, C: Clock> OrderService<R, E, C> {
    pub fn new(repo: R, email: E, clock: C) -> Self {
        OrderService { repo, email, clock }
    }
    
    pub fn place_order(&mut self, user_email: &str, items: Vec<Item>) -> Result<Order, String> {
        if items.is_empty() {
            return Err("order must have at least one item".into());
        }
        
        let total: f64 = items.iter().map(|i| i.price).sum();
        let now = self.clock.now();
        
        let order = self.repo.create(user_email, &items, total, now)
            .map_err(|e| format!("failed to create order: {}", e))?;
        
        self.email.send(
            user_email,
            "Order Confirmation",
            &format!("Your order #{} has been placed. Total: ${:.2}", order.id, total),
        ).map_err(|e| format!("failed to send confirmation: {}", e))?;
        
        Ok(order)
    }
}

// En tests: inyectar mocks para las tres dependencias
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn place_order_success() {
        let repo = MockOrderRepo::new();
        let email = FakeEmailSender::new();
        let clock = MockClock::from_secs(1000);
        
        let mut service = OrderService::new(repo, email, clock);
        
        let order = service.place_order(
            "alice@test.com",
            vec![Item { name: "Widget".into(), price: 9.99 }],
        ).unwrap();
        
        assert!(order.id > 0);
    }
    
    #[test]
    fn place_empty_order_fails() {
        let service = OrderService::new(
            MockOrderRepo::new(),
            FakeEmailSender::new(),
            MockClock::from_secs(1000),
        );
        // ...
    }
}
```

### ¿Demasiados genéricos? Considerar trait objects

```rust
// Si tienes 5+ dependencias, los genéricos se vuelven incómodos:
// struct Service<A, B, C, D, E> where A: ..., B: ..., C: ..., D: ..., E: ...

// Alternativa con trait objects para servicios complejos:
pub struct OrderService {
    repo: Box<dyn OrderRepository>,
    email: Box<dyn EmailSender>,
    clock: Box<dyn Clock>,
}

impl OrderService {
    pub fn new(
        repo: Box<dyn OrderRepository>,
        email: Box<dyn EmailSender>,
        clock: Box<dyn Clock>,
    ) -> Self {
        OrderService { repo, email, clock }
    }
}

// Más simple pero con overhead de indirección
```

---

## 14. Mocks manuales: struct que implementa el trait

El mock más simple es un struct que implementa el trait con comportamiento predefinido.

### Tipos de mocks manuales

```
┌────────────────────┬──────────────────────────────────────────┐
│ Tipo               │ Descripción                              │
├────────────────────┼──────────────────────────────────────────┤
│ Stub              │ Retorna valores fijos predefinidos        │
│                    │ "siempre retorna Ok(User{...})"          │
│                    │                                          │
│ Fake              │ Implementación funcional simplificada     │
│                    │ "HashMap en lugar de PostgreSQL"          │
│                    │                                          │
│ Mock              │ Registra llamadas y verifica expectativas │
│                    │ "verificar que se llamó send() 2 veces"  │
│                    │                                          │
│ Spy               │ Wrapper que registra llamadas al real     │
│                    │ "llamar al real pero grabar los args"     │
└────────────────────┴──────────────────────────────────────────┘
```

### Stub: valor fijo

```rust
struct AlwaysSuccessRepo;

impl UserRepository for AlwaysSuccessRepo {
    fn find_by_id(&self, _id: i64) -> Result<Option<User>, RepoError> {
        Ok(Some(User {
            id: 1,
            name: "Test User".into(),
            email: "test@test.com".into(),
        }))
    }
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        Ok(1)
    }
    fn delete(&mut self, _id: i64) -> Result<bool, RepoError> {
        Ok(true)
    }
}

struct AlwaysFailsRepo;

impl UserRepository for AlwaysFailsRepo {
    fn find_by_id(&self, _id: i64) -> Result<Option<User>, RepoError> {
        Err(RepoError::Internal("database is down".into()))
    }
    fn create(&mut self, _: &str, _: &str) -> Result<i64, RepoError> {
        Err(RepoError::Internal("database is down".into()))
    }
    fn delete(&mut self, _: i64) -> Result<bool, RepoError> {
        Err(RepoError::Internal("database is down".into()))
    }
}
```

### Fake: implementación simplificada pero funcional

El fake es la opción más común y la más útil para la mayoría de los tests. Ya lo mostramos en secciones anteriores con `MockTodoRepo` usando `HashMap`.

---

## 15. Mocks con estado: verificar llamadas

```rust
use std::cell::RefCell;

pub struct SpyEmailSender {
    calls: RefCell<Vec<EmailCall>>,
}

#[derive(Debug, Clone)]
pub struct EmailCall {
    pub to: String,
    pub subject: String,
    pub body: String,
}

impl SpyEmailSender {
    pub fn new() -> Self {
        SpyEmailSender { calls: RefCell::new(Vec::new()) }
    }
    
    pub fn call_count(&self) -> usize {
        self.calls.borrow().len()
    }
    
    pub fn last_call(&self) -> Option<EmailCall> {
        self.calls.borrow().last().cloned()
    }
    
    pub fn calls(&self) -> Vec<EmailCall> {
        self.calls.borrow().clone()
    }
    
    pub fn was_called_with(&self, to: &str) -> bool {
        self.calls.borrow().iter().any(|c| c.to == to)
    }
}

impl EmailSender for SpyEmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        self.calls.borrow_mut().push(EmailCall {
            to: to.to_string(),
            subject: subject.to_string(),
            body: body.to_string(),
        });
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn notification_sends_email() {
        let spy = SpyEmailSender::new();
        // Necesitamos Rc para compartir entre service y test
        let spy_ref = std::rc::Rc::new(spy);
        
        // ... usar spy_ref en el service y verificar después
        // (En la práctica, se usa el patrón de la sección 16 con closures)
    }
    
    #[test]
    fn verify_email_content() {
        let spy = SpyEmailSender::new();
        spy.send("alice@test.com", "Hello", "World").unwrap();
        
        assert_eq!(spy.call_count(), 1);
        let call = spy.last_call().unwrap();
        assert_eq!(call.to, "alice@test.com");
        assert_eq!(call.subject, "Hello");
        assert!(call.body.contains("World"));
    }
}
```

---

## 16. Mocks con closures: comportamiento dinámico

```rust
pub struct ConfigurableEmailSender {
    handler: Box<dyn Fn(&str, &str, &str) -> Result<(), String>>,
}

impl ConfigurableEmailSender {
    pub fn always_ok() -> Self {
        ConfigurableEmailSender {
            handler: Box::new(|_, _, _| Ok(())),
        }
    }
    
    pub fn always_fail(error: String) -> Self {
        ConfigurableEmailSender {
            handler: Box::new(move |_, _, _| Err(error.clone())),
        }
    }
    
    pub fn with_handler<F>(handler: F) -> Self
    where
        F: Fn(&str, &str, &str) -> Result<(), String> + 'static,
    {
        ConfigurableEmailSender {
            handler: Box::new(handler),
        }
    }
}

impl EmailSender for ConfigurableEmailSender {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        (self.handler)(to, subject, body)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::rc::Rc;
    
    #[test]
    fn test_with_custom_behavior() {
        let calls = Rc::new(RefCell::new(Vec::<String>::new()));
        let calls_clone = calls.clone();
        
        let sender = ConfigurableEmailSender::with_handler(move |to, _, _| {
            calls_clone.borrow_mut().push(to.to_string());
            if to.ends_with("@blocked.com") {
                Err("blocked domain".into())
            } else {
                Ok(())
            }
        });
        
        assert!(sender.send("alice@ok.com", "Hi", "Body").is_ok());
        assert!(sender.send("bob@blocked.com", "Hi", "Body").is_err());
        
        let recorded = calls.borrow();
        assert_eq!(recorded.len(), 2);
        assert_eq!(recorded[0], "alice@ok.com");
    }
    
    #[test]
    fn test_failure_scenario() {
        let sender = ConfigurableEmailSender::always_fail("SMTP timeout".into());
        let result = sender.send("alice@test.com", "Hi", "Body");
        assert_eq!(result.unwrap_err(), "SMTP timeout");
    }
}
```

---

## 17. Fakes vs Mocks vs Stubs: vocabulario preciso

```
┌─────────────────────────────────────────────────────────────────┐
│          VOCABULARIO DE TEST DOUBLES (Gerard Meszaros)          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌── DUMMY ─────────────────────────────────────────────────┐   │
│  │  Solo ocupa espacio. Nunca se usa realmente.             │   │
│  │  Ejemplo: pasar un logger que nunca se invoca            │   │
│  │  struct NullLogger;                                      │   │
│  │  impl Logger for NullLogger { fn log(&self, _: &str) {} }│   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌── STUB ──────────────────────────────────────────────────┐   │
│  │  Retorna valores predefinidos. Sin lógica.               │   │
│  │  Ejemplo: repositorio que siempre retorna el mismo User  │   │
│  │  fn find(&self, _id: i64) → Some(fixed_user.clone())    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌── SPY ───────────────────────────────────────────────────┐   │
│  │  Registra cómo fue invocado. Se verifica DESPUÉS.        │   │
│  │  Ejemplo: email sender que guarda los emails en un Vec   │   │
│  │  Verificar: assert_eq!(spy.call_count(), 2)              │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌── MOCK ──────────────────────────────────────────────────┐   │
│  │  Tiene EXPECTATIVAS programadas. Falla si no se cumplen. │   │
│  │  Ejemplo: "espero que send() se llame 2 veces con estos  │   │
│  │  argumentos, en este orden"                               │   │
│  │  (mockall genera estos automáticamente)                   │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌── FAKE ──────────────────────────────────────────────────┐   │
│  │  Implementación funcional simplificada. Tiene lógica.    │   │
│  │  Ejemplo: HashMap en lugar de PostgreSQL                  │   │
│  │  Diferencia con mock: el fake FUNCIONA, no solo simula   │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                 │
│  En Rust, los más usados son FAKES y STUBS.                     │
│  Los MOCKS con expectativas se generan con mockall (T02).       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 18. Errores de diseño que dificultan el testing

### Error 1: funciones que crean sus propias dependencias

```rust
// ❌ IMPOSIBLE de testear sin BD real
pub fn get_user_name(id: i64) -> String {
    let client = postgres::Client::connect("host=localhost", NoTls).unwrap();
    let row = client.query_one("SELECT name FROM users WHERE id=$1", &[&id]).unwrap();
    row.get(0)
}

// ✅ Testeable: la dependencia se pasa como argumento
pub fn get_user_name<R: UserRepository>(repo: &R, id: i64) -> Result<String, String> {
    repo.find_by_id(id)
        .map_err(|e| e.to_string())?
        .map(|u| u.name)
        .ok_or("not found".into())
}
```

### Error 2: usar tipos concretos de infraestructura en firmas públicas

```rust
// ❌ La firma expone PostgreSQL
pub fn process(client: &postgres::Client) -> Vec<Record> { ... }

// ✅ La firma usa una abstracción
pub fn process<R: RecordRepository>(repo: &R) -> Vec<Record> { ... }
```

### Error 3: dependencias globales (statics mutables)

```rust
// ❌ Estado global: imposible de aislar
static mut COUNTER: u64 = 0;

pub fn next_id() -> u64 {
    unsafe {
        COUNTER += 1;
        COUNTER
    }
}

// ✅ Inyectar un generador de IDs
pub trait IdGenerator {
    fn next_id(&self) -> u64;
}
```

### Error 4: traits demasiado grandes

```rust
// ❌ Mega-trait: el mock necesita implementar 15 métodos
pub trait Everything {
    fn get_user(&self, id: i64) -> User;
    fn get_product(&self, id: i64) -> Product;
    fn send_email(&self, to: &str, body: &str);
    fn log(&self, msg: &str);
    fn get_config(&self, key: &str) -> String;
    // ... 10 métodos más
}

// ✅ Traits separados: cada mock es simple
pub trait UserRepository { fn get_user(&self, id: i64) -> User; }
pub trait EmailSender { fn send_email(&self, to: &str, body: &str); }
pub trait Config { fn get(&self, key: &str) -> String; }
```

### Error 5: lógica de negocio dentro de la implementación del trait

```rust
// ❌ La validación está en el repositorio (implementación)
impl UserRepository for PostgresRepo {
    fn create(&self, name: &str, email: &str) -> Result<i64, Error> {
        if name.is_empty() { return Err("name empty"); }  // ← lógica de negocio
        if !email.contains('@') { return Err("bad email"); } // ← lógica de negocio
        self.client.execute("INSERT...", &[&name, &email])
    }
}
// El mock tendría que DUPLICAR esta validación

// ✅ La validación está en el servicio (que usa el trait)
impl<R: UserRepository> UserService<R> {
    fn create(&mut self, name: &str, email: &str) -> Result<i64, Error> {
        if name.is_empty() { return Err("name empty"); }     // lógica en el servicio
        if !email.contains('@') { return Err("bad email"); } // lógica en el servicio
        self.repo.create(name, email)                         // repo solo persiste
    }
}
```

---

## 19. async traits y mocking

A partir de Rust 1.75+, los async traits son nativos. Antes se necesitaba el crate `async-trait`.

### async trait nativo (Rust 1.75+)

```rust
pub trait AsyncUserRepository {
    async fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError>;
    async fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError>;
}

// Mock para async trait
struct MockAsyncRepo {
    users: std::collections::HashMap<i64, User>,
}

impl AsyncUserRepository for MockAsyncRepo {
    async fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError> {
        // async pero sin esperar nada (inmediato)
        Ok(self.users.get(&id).cloned())
    }
    
    async fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        let id = self.users.len() as i64 + 1;
        self.users.insert(id, User {
            id,
            name: name.to_string(),
            email: email.to_string(),
        });
        Ok(id)
    }
}

// Tests async
#[cfg(test)]
mod tests {
    use super::*;
    
    #[tokio::test]
    async fn test_async_service() {
        let repo = MockAsyncRepo { users: std::collections::HashMap::new() };
        let mut service = AsyncUserService::new(repo);
        let user = service.register("Alice", "alice@test.com").await.unwrap();
        assert_eq!(user.name, "Alice");
    }
}
```

### Con el crate async-trait (compatibilidad con Rust < 1.75)

```rust
use async_trait::async_trait;

#[async_trait]
pub trait UserRepository {
    async fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError>;
    async fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError>;
}

// El mock implementa el async_trait de la misma forma
#[async_trait]
impl UserRepository for MockRepo {
    async fn find_by_id(&self, id: i64) -> Result<Option<User>, RepoError> {
        Ok(self.users.get(&id).cloned())
    }
    async fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        // ...
        Ok(1)
    }
}
```

---

## 20. Comparación con C y Go

### C: DI con punteros a función

```c
// C no tiene traits ni interfaces. La DI se hace con punteros a función
// o structs de punteros a función (vtable manual).

// "Trait" como struct de punteros a función
typedef struct {
    int (*find_by_id)(void *ctx, int64_t id, User *out);
    int (*create)(void *ctx, const char *name, const char *email, int64_t *out_id);
    void *context;  // datos del "objeto"
} UserRepository;

// Implementación real
int postgres_find(void *ctx, int64_t id, User *out) {
    PgConnection *conn = (PgConnection *)ctx;
    // ... query real
    return 0;
}

// Mock
typedef struct {
    User users[100];
    int count;
} MockData;

int mock_find(void *ctx, int64_t id, User *out) {
    MockData *data = (MockData *)ctx;
    for (int i = 0; i < data->count; i++) {
        if (data->users[i].id == id) {
            *out = data->users[i];
            return 0;
        }
    }
    return -1;  // not found
}

// Servicio que usa la "interfaz"
int get_user_name(UserRepository *repo, int64_t id, char *out, size_t max) {
    User user;
    if (repo->find_by_id(repo->context, id, &user) != 0) {
        return -1;
    }
    strncpy(out, user.name, max);
    return 0;
}

// Test
void test_get_user_name(void) {
    MockData data = { .count = 1 };
    data.users[0] = (User){ .id = 1, .name = "Alice" };
    
    UserRepository mock_repo = {
        .find_by_id = mock_find,
        .context = &data,
    };
    
    char name[100];
    assert(get_user_name(&mock_repo, 1, name, sizeof(name)) == 0);
    assert(strcmp(name, "Alice") == 0);
}
```

### Go: interfaces implícitas

```go
// Go: las interfaces son implícitas (structural typing)
// No necesitas declarar que un tipo implementa una interfaz

type UserRepository interface {
    FindByID(id int64) (*User, error)
    Create(name, email string) (int64, error)
}

// Implementación real
type PostgresRepo struct {
    db *sql.DB
}

func (r *PostgresRepo) FindByID(id int64) (*User, error) {
    // query real
    return nil, nil
}

func (r *PostgresRepo) Create(name, email string) (int64, error) {
    // insert real
    return 0, nil
}

// Mock (Go no necesita declarar "implements")
type MockRepo struct {
    Users map[int64]*User
    NextID int64
}

func (m *MockRepo) FindByID(id int64) (*User, error) {
    user, ok := m.Users[id]
    if !ok {
        return nil, fmt.Errorf("not found")
    }
    return user, nil
}

func (m *MockRepo) Create(name, email string) (int64, error) {
    m.NextID++
    m.Users[m.NextID] = &User{ID: m.NextID, Name: name, Email: email}
    return m.NextID, nil
}

// Servicio
type UserService struct {
    repo UserRepository
}

func NewUserService(repo UserRepository) *UserService {
    return &UserService{repo: repo}
}

// Test
func TestRegisterUser(t *testing.T) {
    mock := &MockRepo{Users: make(map[int64]*User), NextID: 0}
    service := NewUserService(mock)
    // ...
}
```

### Tabla comparativa

```
┌───────────────────────┬──────────────┬──────────────┬───────────────┐
│ Aspecto               │ C            │ Go           │ Rust          │
├───────────────────────┼──────────────┼──────────────┼───────────────┤
│ Mecanismo de          │ Punteros a   │ Interfaces   │ Traits        │
│ abstracción           │ función      │ (implícitas) │ (explícitos)  │
│                       │              │              │               │
│ Declarar que impl     │ Asignar      │ Implícito    │ impl Trait    │
│ satisface contrato    │ punteros     │ (structural) │ for Type      │
│                       │              │              │               │
│ Type safety           │ Baja (void*) │ Alta         │ Máxima        │
│                       │              │              │ (compilación) │
│                       │              │              │               │
│ Zero-cost             │ Sí (punteros │ No (siempre  │ Sí (genéricos)│
│                       │ son directos)│ interfaz)    │               │
│                       │              │              │               │
│ Ergonomía del mock    │ Muy baja     │ Alta         │ Media-alta    │
│                       │ (manual)     │ (implícito)  │ (explícito)   │
│                       │              │              │               │
│ Errores en compilación│ Pocos        │ Algunos      │ Máximos       │
│ si el mock no cumple  │              │              │               │
│                       │              │              │               │
│ Frameworks de mock    │ cmocka, fff  │ gomock,      │ mockall,      │
│                       │ (manual)     │ testify      │ manual        │
│                       │              │              │               │
│ Líneas para un mock   │ ~30-50       │ ~10-15       │ ~15-25        │
│ básico                │              │              │               │
└───────────────────────┴──────────────┴──────────────┴───────────────┘
```

---

## 21. Errores comunes

### Error 1: no abstraer la dependencia

```rust
// ❌ Usa el tipo concreto directamente
pub struct Service {
    client: reqwest::Client,  // sin trait
}
// Imposible reemplazar en tests

// ✅ Abstraer detrás de un trait
pub trait HttpClient { ... }
pub struct Service<C: HttpClient> { client: C }
```

### Error 2: trait que expone detalles internos

```rust
// ❌ El trait revela que es SQL
pub trait Storage {
    fn execute_sql(&self, query: &str) -> Vec<Row>;
}

// ✅ El trait habla en términos del dominio
pub trait UserStorage {
    fn find_user(&self, id: i64) -> Option<User>;
}
```

### Error 3: mock demasiado complejo (reimplementa la lógica)

```rust
// ❌ El mock tiene la misma complejidad que la implementación real
impl UserRepository for MockRepo {
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        // Validar formato de email con regex
        // Verificar unicidad contra lista interna
        // Generar ID con UUID
        // Normalizar nombre
        // ... 50 líneas de lógica
    }
}

// ✅ El mock es simple — la lógica de negocio está en el servicio
impl UserRepository for MockRepo {
    fn create(&mut self, name: &str, email: &str) -> Result<i64, RepoError> {
        if self.emails.contains(email) { return Err(RepoError::Duplicate); }
        let id = self.next_id; self.next_id += 1;
        self.users.insert(id, User { id, name: name.into(), email: email.into() });
        self.emails.insert(email.into());
        Ok(id)
    }
}
```

### Error 4: no testear los caminos de error

```rust
// ❌ Solo testear el happy path
#[test]
fn create_user_success() {
    let mut service = setup();
    assert!(service.register("Alice", "a@b.com").is_ok());
}

// ✅ Testear errores inyectando mocks que fallan
#[test]
fn handles_db_failure() {
    let mut service = UserService::new(AlwaysFailsRepo);
    let result = service.register("Alice", "a@b.com");
    assert!(result.is_err());
    assert!(result.unwrap_err().contains("failed"));
}
```

### Error 5: testear el mock en lugar del servicio

```rust
// ❌ Esto testea que HashMap funciona, no tu código
#[test]
fn mock_works() {
    let mut repo = MockRepo::new();
    repo.create("Alice", "a@b.com").unwrap();
    assert!(repo.find_by_id(1).unwrap().is_some());
}

// ✅ Testear el SERVICIO que usa el mock
#[test]
fn service_validates_email() {
    let mut service = UserService::new(MockRepo::new());
    assert!(service.register("Alice", "not-email").is_err());
}
```

---

## 22. Ejemplo completo: sistema de notificaciones

### Estructura del proyecto

```
notification_service/
├── Cargo.toml
├── src/
│   ├── lib.rs
│   ├── traits.rs       # Contratos (traits)
│   ├── service.rs       # Lógica de negocio
│   └── models.rs        # Tipos de dominio
└── tests/
    └── service_tests.rs # Tests con mocks
```

### src/models.rs

```rust
#[derive(Debug, Clone, PartialEq)]
pub struct Notification {
    pub id: u64,
    pub user_id: u64,
    pub channel: Channel,
    pub message: String,
    pub sent: bool,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Channel {
    Email(String),    // dirección de email
    Sms(String),      // número de teléfono
    Push(String),     // device token
}

#[derive(Debug, Clone, PartialEq)]
pub struct UserPreferences {
    pub user_id: u64,
    pub email: Option<String>,
    pub phone: Option<String>,
    pub push_token: Option<String>,
    pub email_enabled: bool,
    pub sms_enabled: bool,
    pub push_enabled: bool,
}

#[derive(Debug, PartialEq)]
pub enum NotifyError {
    UserNotFound,
    NoChannelAvailable,
    SendFailed(String),
    StorageError(String),
}

impl std::fmt::Display for NotifyError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            NotifyError::UserNotFound => write!(f, "user not found"),
            NotifyError::NoChannelAvailable => write!(f, "no notification channel available"),
            NotifyError::SendFailed(msg) => write!(f, "send failed: {}", msg),
            NotifyError::StorageError(msg) => write!(f, "storage error: {}", msg),
        }
    }
}
```

### src/traits.rs

```rust
use crate::models::*;

pub trait UserPreferencesRepository {
    fn get_preferences(&self, user_id: u64) -> Result<Option<UserPreferences>, NotifyError>;
}

pub trait NotificationStore {
    fn save(&self, notification: &Notification) -> Result<u64, NotifyError>;
    fn mark_sent(&self, id: u64) -> Result<(), NotifyError>;
}

pub trait EmailSender {
    fn send_email(&self, to: &str, subject: &str, body: &str) -> Result<(), String>;
}

pub trait SmsSender {
    fn send_sms(&self, to: &str, message: &str) -> Result<(), String>;
}

pub trait PushSender {
    fn send_push(&self, device_token: &str, title: &str, body: &str) -> Result<(), String>;
}
```

### src/service.rs

```rust
use crate::models::*;
use crate::traits::*;

pub struct NotificationService<P, S, E, Sm, Pu>
where
    P: UserPreferencesRepository,
    S: NotificationStore,
    E: EmailSender,
    Sm: SmsSender,
    Pu: PushSender,
{
    prefs: P,
    store: S,
    email: E,
    sms: Sm,
    push: Pu,
}

impl<P, S, E, Sm, Pu> NotificationService<P, S, E, Sm, Pu>
where
    P: UserPreferencesRepository,
    S: NotificationStore,
    E: EmailSender,
    Sm: SmsSender,
    Pu: PushSender,
{
    pub fn new(prefs: P, store: S, email: E, sms: Sm, push: Pu) -> Self {
        NotificationService { prefs, store, email, sms, push }
    }
    
    pub fn notify(&self, user_id: u64, message: &str) -> Result<Vec<Channel>, NotifyError> {
        // 1. Obtener preferencias del usuario
        let prefs = self.prefs.get_preferences(user_id)?
            .ok_or(NotifyError::UserNotFound)?;
        
        // 2. Determinar canales disponibles
        let channels = self.resolve_channels(&prefs);
        if channels.is_empty() {
            return Err(NotifyError::NoChannelAvailable);
        }
        
        // 3. Enviar por cada canal
        let mut sent_channels = Vec::new();
        for channel in &channels {
            let notification = Notification {
                id: 0, // se asigna al guardar
                user_id,
                channel: channel.clone(),
                message: message.to_string(),
                sent: false,
            };
            
            let notif_id = self.store.save(&notification)
                .map_err(|e| NotifyError::StorageError(format!("{}", e)))?;
            
            let send_result = match channel {
                Channel::Email(addr) => {
                    self.email.send_email(addr, "Notification", message)
                }
                Channel::Sms(phone) => {
                    self.sms.send_sms(phone, message)
                }
                Channel::Push(token) => {
                    self.push.send_push(token, "Notification", message)
                }
            };
            
            match send_result {
                Ok(()) => {
                    let _ = self.store.mark_sent(notif_id);
                    sent_channels.push(channel.clone());
                }
                Err(_) => {
                    // Log error but continue with other channels
                }
            }
        }
        
        if sent_channels.is_empty() {
            Err(NotifyError::SendFailed("all channels failed".into()))
        } else {
            Ok(sent_channels)
        }
    }
    
    fn resolve_channels(&self, prefs: &UserPreferences) -> Vec<Channel> {
        let mut channels = Vec::new();
        
        if prefs.email_enabled {
            if let Some(ref email) = prefs.email {
                channels.push(Channel::Email(email.clone()));
            }
        }
        if prefs.sms_enabled {
            if let Some(ref phone) = prefs.phone {
                channels.push(Channel::Sms(phone.clone()));
            }
        }
        if prefs.push_enabled {
            if let Some(ref token) = prefs.push_token {
                channels.push(Channel::Push(token.clone()));
            }
        }
        
        channels
    }
}
```

### tests/service_tests.rs

```rust
use notification_service::models::*;
use notification_service::traits::*;
use notification_service::service::*;
use std::cell::RefCell;
use std::collections::HashMap;

// ── Mocks ────────────────────────────────────────────────────

struct MockPrefsRepo {
    prefs: HashMap<u64, UserPreferences>,
}

impl MockPrefsRepo {
    fn new() -> Self { MockPrefsRepo { prefs: HashMap::new() } }
    fn with(mut self, p: UserPreferences) -> Self {
        self.prefs.insert(p.user_id, p);
        self
    }
}

impl UserPreferencesRepository for MockPrefsRepo {
    fn get_preferences(&self, user_id: u64) -> Result<Option<UserPreferences>, NotifyError> {
        Ok(self.prefs.get(&user_id).cloned())
    }
}

struct MockStore {
    next_id: RefCell<u64>,
    notifications: RefCell<Vec<Notification>>,
}

impl MockStore {
    fn new() -> Self {
        MockStore { next_id: RefCell::new(1), notifications: RefCell::new(Vec::new()) }
    }
    fn saved_count(&self) -> usize { self.notifications.borrow().len() }
}

impl NotificationStore for MockStore {
    fn save(&self, notif: &Notification) -> Result<u64, NotifyError> {
        let id = *self.next_id.borrow();
        *self.next_id.borrow_mut() += 1;
        let mut n = notif.clone();
        n.id = id;
        self.notifications.borrow_mut().push(n);
        Ok(id)
    }
    fn mark_sent(&self, id: u64) -> Result<(), NotifyError> {
        for n in self.notifications.borrow_mut().iter_mut() {
            if n.id == id { n.sent = true; }
        }
        Ok(())
    }
}

struct SpyEmailSender { calls: RefCell<Vec<(String, String, String)>> }
impl SpyEmailSender {
    fn new() -> Self { SpyEmailSender { calls: RefCell::new(Vec::new()) } }
    fn call_count(&self) -> usize { self.calls.borrow().len() }
}
impl EmailSender for SpyEmailSender {
    fn send_email(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        self.calls.borrow_mut().push((to.into(), subject.into(), body.into()));
        Ok(())
    }
}

struct SpySmsSender { calls: RefCell<Vec<(String, String)>> }
impl SpySmsSender {
    fn new() -> Self { SpySmsSender { calls: RefCell::new(Vec::new()) } }
    fn call_count(&self) -> usize { self.calls.borrow().len() }
}
impl SmsSender for SpySmsSender {
    fn send_sms(&self, to: &str, message: &str) -> Result<(), String> {
        self.calls.borrow_mut().push((to.into(), message.into()));
        Ok(())
    }
}

struct NullPushSender;
impl PushSender for NullPushSender {
    fn send_push(&self, _: &str, _: &str, _: &str) -> Result<(), String> { Ok(()) }
}

struct FailingEmailSender;
impl EmailSender for FailingEmailSender {
    fn send_email(&self, _: &str, _: &str, _: &str) -> Result<(), String> {
        Err("SMTP connection refused".into())
    }
}

struct FailingSmsSender;
impl SmsSender for FailingSmsSender {
    fn send_sms(&self, _: &str, _: &str) -> Result<(), String> {
        Err("SMS gateway unavailable".into())
    }
}

// ── Helpers ──────────────────────────────────────────────────

fn all_channels_user() -> UserPreferences {
    UserPreferences {
        user_id: 1,
        email: Some("alice@test.com".into()),
        phone: Some("+1234567890".into()),
        push_token: Some("device_abc".into()),
        email_enabled: true,
        sms_enabled: true,
        push_enabled: true,
    }
}

fn email_only_user() -> UserPreferences {
    UserPreferences {
        user_id: 2,
        email: Some("bob@test.com".into()),
        phone: None,
        push_token: None,
        email_enabled: true,
        sms_enabled: false,
        push_enabled: false,
    }
}

fn disabled_user() -> UserPreferences {
    UserPreferences {
        user_id: 3,
        email: Some("charlie@test.com".into()),
        phone: Some("+9876543210".into()),
        push_token: None,
        email_enabled: false,
        sms_enabled: false,
        push_enabled: false,
    }
}

// ── Tests ────────────────────────────────────────────────────

#[test]
fn notify_sends_to_all_enabled_channels() {
    let prefs = MockPrefsRepo::new().with(all_channels_user());
    let store = MockStore::new();
    let email = SpyEmailSender::new();
    let sms = SpySmsSender::new();
    
    let service = NotificationService::new(prefs, store, email, sms, NullPushSender);
    let channels = service.notify(1, "Hello!").unwrap();
    
    assert_eq!(channels.len(), 3);
}

#[test]
fn notify_email_only_user() {
    let prefs = MockPrefsRepo::new().with(email_only_user());
    let store = MockStore::new();
    let email = SpyEmailSender::new();
    
    let service = NotificationService::new(
        prefs, store, email, SpySmsSender::new(), NullPushSender,
    );
    let channels = service.notify(2, "Hello!").unwrap();
    
    assert_eq!(channels.len(), 1);
    assert!(matches!(channels[0], Channel::Email(_)));
}

#[test]
fn notify_unknown_user_returns_error() {
    let prefs = MockPrefsRepo::new(); // vacío
    let service = NotificationService::new(
        prefs, MockStore::new(), SpyEmailSender::new(),
        SpySmsSender::new(), NullPushSender,
    );
    
    let result = service.notify(999, "Hello!");
    assert_eq!(result.unwrap_err(), NotifyError::UserNotFound);
}

#[test]
fn notify_disabled_user_returns_no_channel() {
    let prefs = MockPrefsRepo::new().with(disabled_user());
    let service = NotificationService::new(
        prefs, MockStore::new(), SpyEmailSender::new(),
        SpySmsSender::new(), NullPushSender,
    );
    
    let result = service.notify(3, "Hello!");
    assert_eq!(result.unwrap_err(), NotifyError::NoChannelAvailable);
}

#[test]
fn notify_continues_when_one_channel_fails() {
    let prefs = MockPrefsRepo::new().with(all_channels_user());
    let store = MockStore::new();
    let sms = SpySmsSender::new();
    
    // Email falla, pero SMS y Push siguen
    let service = NotificationService::new(
        prefs, store, FailingEmailSender, sms, NullPushSender,
    );
    let channels = service.notify(1, "Hello!").unwrap();
    
    // Al menos SMS y Push deben haber funcionado
    assert!(channels.len() >= 1);
    assert!(!channels.iter().any(|c| matches!(c, Channel::Email(_))));
}

#[test]
fn notify_all_channels_fail_returns_error() {
    let prefs = MockPrefsRepo::new().with(UserPreferences {
        user_id: 1,
        email: Some("a@b.com".into()),
        phone: Some("+123".into()),
        push_token: None,
        email_enabled: true,
        sms_enabled: true,
        push_enabled: false,
    });
    
    // Ambos canales habilitados fallan
    let service = NotificationService::new(
        prefs, MockStore::new(), FailingEmailSender,
        FailingSmsSender, NullPushSender,
    );
    
    let result = service.notify(1, "Hello!");
    assert!(matches!(result, Err(NotifyError::SendFailed(_))));
}

#[test]
fn notification_is_stored_before_sending() {
    let prefs = MockPrefsRepo::new().with(email_only_user());
    let store = MockStore::new();
    let email = SpyEmailSender::new();
    
    let service = NotificationService::new(
        prefs, store, email, SpySmsSender::new(), NullPushSender,
    );
    service.notify(2, "Hello!").unwrap();
    
    // Verificar que se guardó en el store
    // (accedemos al store a través del service — en un test real
    // usaríamos Rc<MockStore> para acceso compartido)
}
```

---

## 23. Programa de práctica

### Proyecto: `payment_processor` — procesador de pagos con DI completa

Implementa un procesador de pagos donde todas las dependencias externas están abstraídas con traits.

### src/lib.rs

```rust
pub mod models {
    #[derive(Debug, Clone, PartialEq)]
    pub struct PaymentRequest {
        pub user_id: u64,
        pub amount_cents: u64,
        pub currency: String,
        pub description: String,
    }
    
    #[derive(Debug, Clone, PartialEq)]
    pub struct PaymentResult {
        pub transaction_id: String,
        pub status: PaymentStatus,
        pub amount_cents: u64,
    }
    
    #[derive(Debug, Clone, PartialEq)]
    pub enum PaymentStatus {
        Approved,
        Declined(String),
        Error(String),
    }
    
    #[derive(Debug, PartialEq)]
    pub enum PaymentError {
        InvalidAmount,
        UserNotFound,
        InsufficientFunds,
        GatewayError(String),
        InternalError(String),
    }
    
    impl std::fmt::Display for PaymentError {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            match self {
                PaymentError::InvalidAmount => write!(f, "invalid amount"),
                PaymentError::UserNotFound => write!(f, "user not found"),
                PaymentError::InsufficientFunds => write!(f, "insufficient funds"),
                PaymentError::GatewayError(msg) => write!(f, "gateway: {}", msg),
                PaymentError::InternalError(msg) => write!(f, "internal: {}", msg),
            }
        }
    }
}

pub mod traits {
    use crate::models::*;
    
    pub trait PaymentGateway {
        fn charge(
            &self,
            amount_cents: u64,
            currency: &str,
            token: &str,
        ) -> Result<String, String>; // retorna transaction_id
    }
    
    pub trait UserStore {
        fn get_payment_token(&self, user_id: u64) -> Result<Option<String>, String>;
        fn get_balance(&self, user_id: u64) -> Result<Option<u64>, String>;
    }
    
    pub trait TransactionLog {
        fn record(
            &self,
            user_id: u64,
            transaction_id: &str,
            amount_cents: u64,
            status: &str,
        ) -> Result<(), String>;
    }
    
    pub trait FraudChecker {
        fn is_suspicious(&self, user_id: u64, amount_cents: u64) -> Result<bool, String>;
    }
}

pub mod service {
    use crate::models::*;
    use crate::traits::*;
    
    pub struct PaymentProcessor<G, U, T, F>
    where
        G: PaymentGateway,
        U: UserStore,
        T: TransactionLog,
        F: FraudChecker,
    {
        gateway: G,
        users: U,
        log: T,
        fraud: F,
        max_amount_cents: u64,
    }
    
    impl<G, U, T, F> PaymentProcessor<G, U, T, F>
    where
        G: PaymentGateway,
        U: UserStore,
        T: TransactionLog,
        F: FraudChecker,
    {
        pub fn new(gateway: G, users: U, log: T, fraud: F) -> Self {
            PaymentProcessor {
                gateway,
                users,
                log,
                fraud,
                max_amount_cents: 1_000_000, // $10,000 max
            }
        }
        
        pub fn with_max_amount(mut self, max: u64) -> Self {
            self.max_amount_cents = max;
            self
        }
        
        pub fn process(&self, request: &PaymentRequest) -> Result<PaymentResult, PaymentError> {
            // 1. Validar monto
            if request.amount_cents == 0 {
                return Err(PaymentError::InvalidAmount);
            }
            if request.amount_cents > self.max_amount_cents {
                return Err(PaymentError::InvalidAmount);
            }
            
            // 2. Verificar fraude
            let suspicious = self.fraud.is_suspicious(request.user_id, request.amount_cents)
                .map_err(|e| PaymentError::InternalError(e))?;
            if suspicious {
                self.log_transaction(request.user_id, "FRAUD_CHECK", request.amount_cents, "blocked");
                return Ok(PaymentResult {
                    transaction_id: "BLOCKED".into(),
                    status: PaymentStatus::Declined("fraud check failed".into()),
                    amount_cents: request.amount_cents,
                });
            }
            
            // 3. Obtener token de pago del usuario
            let token = self.users.get_payment_token(request.user_id)
                .map_err(|e| PaymentError::InternalError(e))?
                .ok_or(PaymentError::UserNotFound)?;
            
            // 4. Verificar balance (si aplica)
            if let Ok(Some(balance)) = self.users.get_balance(request.user_id) {
                if balance < request.amount_cents {
                    return Err(PaymentError::InsufficientFunds);
                }
            }
            
            // 5. Cobrar
            match self.gateway.charge(request.amount_cents, &request.currency, &token) {
                Ok(tx_id) => {
                    self.log_transaction(request.user_id, &tx_id, request.amount_cents, "approved");
                    Ok(PaymentResult {
                        transaction_id: tx_id,
                        status: PaymentStatus::Approved,
                        amount_cents: request.amount_cents,
                    })
                }
                Err(e) => {
                    self.log_transaction(request.user_id, "FAILED", request.amount_cents, "declined");
                    Err(PaymentError::GatewayError(e))
                }
            }
        }
        
        fn log_transaction(&self, user_id: u64, tx_id: &str, amount: u64, status: &str) {
            let _ = self.log.record(user_id, tx_id, amount, status);
        }
    }
}
```

### Implementar mocks y tests

El ejercicio para el estudiante es crear:

1. `MockPaymentGateway` — configurable para aprobar o rechazar
2. `MockUserStore` — con tokens y balances predefinidos
3. `MockTransactionLog` — spy que registra llamadas
4. `MockFraudChecker` — configurable para marcar como sospechoso

Y escribir tests para:
- Pago exitoso (happy path)
- Monto inválido (0, negativo, excede máximo)
- Usuario no encontrado
- Fondos insuficientes
- Fraude detectado
- Gateway rechaza
- Log se invoca correctamente
- Múltiples pagos del mismo usuario

---

## 24. Ejercicios

### Ejercicio 1: Abstraer un servicio de caché

**Objetivo**: Practicar la extracción de un trait a partir de código acoplado.

**Contexto**: tienes este código que usa Redis directamente:

```rust
pub struct SessionManager {
    redis: redis::Client,
}

impl SessionManager {
    pub fn create_session(&self, user_id: u64) -> String {
        let token = generate_token();
        let _: () = self.redis.set_ex(&token, user_id, 3600).unwrap();
        token
    }
    
    pub fn get_session(&self, token: &str) -> Option<u64> {
        self.redis.get(token).ok()
    }
    
    pub fn destroy_session(&self, token: &str) {
        let _: () = self.redis.del(token).unwrap();
    }
}
```

**Tareas**:

**a)** Define un trait `SessionStore` que abstraiga las operaciones de almacenamiento.

**b)** Refactoriza `SessionManager` para usar el trait (con genéricos).

**c)** Implementa un `InMemorySessionStore` para tests.

**d)** Escribe al menos 5 tests: crear sesión, obtener sesión válida, sesión inexistente, destruir sesión, crear múltiples sesiones.

---

### Ejercicio 2: Servicio con múltiples dependencias

**Objetivo**: Practicar DI con 3+ dependencias.

**Tareas**:

**a)** Diseña un `ReportService` que dependa de:
- `DataSource` (obtener datos)
- `Formatter` (formatear el reporte: PDF, CSV, HTML)
- `Storage` (guardar el reporte generado)

**b)** Implementa los 3 traits con operaciones mínimas.

**c)** Implementa fakes/mocks para los 3.

**d)** Escribe tests que verifiquen:
- Reporte se genera correctamente
- Fallo en DataSource se propaga
- Fallo en Storage se maneja
- El formato correcto se usa

---

### Ejercicio 3: Inyección de tiempo para tests determinísticos

**Objetivo**: Practicar la inyección de un reloj.

**Tareas**:

**a)** Implementa un `TokenService` que genere tokens con expiración:

```rust
pub struct Token {
    pub value: String,
    pub expires_at: SystemTime,
}
```

**b)** El servicio necesita un trait `Clock` para obtener el tiempo actual.

**c)** Implementa `is_valid(token)` que verifica si un token no ha expirado.

**d)** Escribe tests que:
- Verifiquen que un token recién creado es válido
- Avancen el reloj mock para que expire, y verifiquen que `is_valid` retorna false
- Verifiquen que tokens con diferentes TTL expiran en momentos correctos

---

### Ejercicio 4: Refactorizar código legacy sin traits

**Objetivo**: Practicar la extracción incremental de traits.

**Contexto**: te dan este código que funciona pero es imposible de testear:

```rust
pub fn process_orders() {
    let db = Database::connect("postgresql://localhost/orders");
    let orders = db.query("SELECT * FROM orders WHERE status='pending'");
    
    for order in orders {
        let result = reqwest::blocking::Client::new()
            .post("https://payment.api/charge")
            .json(&order)
            .send()
            .unwrap();
        
        if result.status().is_success() {
            db.execute("UPDATE orders SET status='paid' WHERE id=$1", &[&order.id]);
            println!("[INFO] Order {} paid", order.id);
        } else {
            eprintln!("[ERROR] Order {} failed", order.id);
        }
    }
}
```

**Tareas**:

**a)** Identifica las 3 dependencias implícitas.

**b)** Define traits para cada una.

**c)** Refactoriza a un struct `OrderProcessor<D, P, L>` con las dependencias inyectadas.

**d)** Implementa mocks y escribe tests para: procesar pedidos exitosamente, manejar fallos de pago, manejar fallo de BD, verificar que el log se invoca.

---

## Navegación

- **Anterior**: [S03/T04 - proptest vs quickcheck](../../S03_Property_Based_Testing/T04_Proptest_vs_Quickcheck/README.md)
- **Siguiente**: [T02 - mockall crate](../T02_Mockall/README.md)

---

> **Sección 4: Mocking en Rust** — Tópico 1 de 4 completado
>
> - T01: Trait-based dependency injection — definir trait, implementación real + mock, inyectar en tests ✓
> - T02: mockall crate (siguiente)
> - T03: Cuándo no mockear
> - T04: Test doubles sin crates externos
