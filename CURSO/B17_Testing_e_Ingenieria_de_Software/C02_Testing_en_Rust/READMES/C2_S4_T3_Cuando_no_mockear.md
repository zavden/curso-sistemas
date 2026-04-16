# T03 - Cuándo no mockear: el arte de elegir la estrategia correcta

## Índice

1. [El problema del over-mocking](#1-el-problema-del-over-mocking)
2. [La pirámide de test doubles](#2-la-pirámide-de-test-doubles)
3. [Regla fundamental: mockear solo lo que duele](#3-regla-fundamental-mockear-solo-lo-que-duele)
4. [Criterio 1: velocidad — ¿es lento?](#4-criterio-1-velocidad--es-lento)
5. [Criterio 2: determinismo — ¿es no-determinístico?](#5-criterio-2-determinismo--es-no-determinístico)
6. [Criterio 3: efectos secundarios — ¿tiene side effects?](#6-criterio-3-efectos-secundarios--tiene-side-effects)
7. [Criterio 4: disponibilidad — ¿está siempre disponible?](#7-criterio-4-disponibilidad--está-siempre-disponible)
8. [El diagrama de decisión completo](#8-el-diagrama-de-decisión-completo)
9. [Preferir la implementación real cuando es rápido](#9-preferir-la-implementación-real-cuando-es-rápido)
10. [Fake implementations: la alternativa superior al mock](#10-fake-implementations-la-alternativa-superior-al-mock)
11. [InMemoryDatabase: el fake más común](#11-inmemorydatabase-el-fake-más-común)
12. [InMemoryFileSystem: abstracción del filesystem](#12-inmemoryfilesystem-abstracción-del-filesystem)
13. [FakeHttpClient: servidor HTTP controlado](#13-fakehttpclient-servidor-http-controlado)
14. [FakeClock: control determinístico del tiempo](#14-fakeclock-control-determinístico-del-tiempo)
15. [Fakes vs Mocks: análisis profundo](#15-fakes-vs-mocks-análisis-profundo)
16. [El espectro de fidelidad](#16-el-espectro-de-fidelidad)
17. [Contract tests: verificar que el fake es fiel](#17-contract-tests-verificar-que-el-fake-es-fiel)
18. [Test containers: la dependencia real en Docker](#18-test-containers-la-dependencia-real-en-docker)
19. [Cuándo los mocks SÍ son la mejor opción](#19-cuándo-los-mocks-sí-son-la-mejor-opción)
20. [El anti-patrón del mock que replica la implementación](#20-el-anti-patrón-del-mock-que-replica-la-implementación)
21. [El anti-patrón del test frágil por over-specification](#21-el-anti-patrón-del-test-frágil-por-over-specification)
22. [El anti-patrón del mock omnisciente](#22-el-anti-patrón-del-mock-omnisciente)
23. [Estrategias por tipo de dependencia](#23-estrategias-por-tipo-de-dependencia)
24. [Comparación con C y Go](#24-comparación-con-c-y-go)
25. [Errores comunes](#25-errores-comunes)
26. [Ejemplo completo: sistema con fakes y mocks combinados](#26-ejemplo-completo-sistema-con-fakes-y-mocks-combinados)
27. [Programa de práctica](#27-programa-de-práctica)
28. [Ejercicios](#28-ejercicios)

---

## 1. El problema del over-mocking

Después de aprender mockall (T02), es tentador mockear **todo**. Pero el over-mocking — mockear más de lo necesario — es uno de los anti-patrones más destructivos en testing:

```
El ciclo del over-mocking:

  Aprendes mockall
       │
       ▼
  Mockeas cada dependencia ──► Tests pasan
       │                            │
       │                            ▼
       │                     Refactorizas código interno
       │                     (sin cambiar comportamiento)
       │                            │
       │                            ▼
       │                     50% de los tests fallan
       │                            │
       │                            ▼
       │                     "Los tests son una carga"
       │                            │
       │                            ▼
       └──────────────────── Dejas de escribir tests
```

### Síntomas del over-mocking

```
┌────────────────────────────────────────────────────────────────────┐
│ ⚠ Señales de alarma                                               │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│ 1. El setup del test tiene más líneas que el test en sí            │
│                                                                    │
│ 2. Cada refactorización interna rompe tests aunque                 │
│    el comportamiento no cambió                                     │
│                                                                    │
│ 3. El test verifica CÓMO se hace algo, no QUÉ resultado produce   │
│                                                                    │
│ 4. Tienes mocks de mocks (un mock retorna otro mock)               │
│                                                                    │
│ 5. El test pasa pero la integración falla en producción            │
│                                                                    │
│ 6. Cambiar un mensaje de log rompe 20 tests                       │
│                                                                    │
│ 7. No puedes describir qué comportamiento verifica el test        │
│    sin mencionar implementación interna                            │
│                                                                    │
│ 8. El mock es más complejo que la implementación real              │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### El costo real

```rust
// Ejemplo: función pura que no necesita mock
pub fn calculate_total(items: &[CartItem], tax_rate: f64) -> f64 {
    let subtotal: f64 = items.iter().map(|i| i.price * i.quantity as f64).sum();
    subtotal * (1.0 + tax_rate)
}

// ✗ Over-mocking: abstraer algo que no lo necesita
#[automock]
pub trait TaxCalculator {
    fn calculate(&self, subtotal: f64) -> f64;
}

#[automock]
pub trait SubtotalCalculator {
    fn sum_items(&self, items: &[CartItem]) -> f64;
}

// Ahora tienes 2 traits, 2 mocks, y el test es:
#[test]
fn test_with_unnecessary_mocks() {
    let mut tax_mock = MockTaxCalculator::new();
    tax_mock.expect_calculate()
        .with(predicate::eq(100.0))
        .returning(|s| s * 1.08);

    let mut sum_mock = MockSubtotalCalculator::new();
    sum_mock.expect_sum_items()
        .returning(|_| 100.0);

    // ¿Qué estamos testeando? La secuencia de llamadas.
    // No estamos testeando lógica.
}

// ✓ Sin mocks: testear directamente la función pura
#[test]
fn test_total_calculation() {
    let items = vec![
        CartItem { price: 25.0, quantity: 2 },
        CartItem { price: 50.0, quantity: 1 },
    ];
    assert_eq!(calculate_total(&items, 0.08), 108.0);
}
```

---

## 2. La pirámide de test doubles

No todos los test doubles son iguales. Hay un espectro de fidelidad:

```
                          ▲ Fidelidad
                          │
                          │
     ┌────────────────────┤
     │  Implementación    │  100% fiel a producción
     │  REAL              │  Pero puede ser lenta, no determinística
     │  (ej: PostgreSQL)  │  o tener side effects
     ├────────────────────┤
     │  Test container    │  ~99% fiel (misma tecnología en Docker)
     │  (ej: testcontainers│  Más lento que in-memory
     │   + PostgreSQL)    │  Determinístico si se controla bien
     ├────────────────────┤
     │  Fake              │  ~90% fiel (misma semántica, otra impl)
     │  (ej: HashMap      │  Rápido, determinístico
     │   como "DB")       │  Pero no reproduce edge cases del real
     ├────────────────────┤
     │  Stub              │  ~50% fiel (retorna valores fijos)
     │  (ej: return_const │  Muy rápido, muy simple
     │   42)              │  No verifica interacciones
     ├────────────────────┤
     │  Mock              │  ~30% fiel (verifica llamadas)
     │  (ej: mockall      │  Rápido, verifica interacciones
     │   expect + times)  │  Pero acopla test a implementación
     ├────────────────────┤
     │  Dummy             │  0% fiel (existe para compilar)
     │  (ej: struct que   │  Solo satisface el sistema de tipos
     │   hace panic!())   │  No se debe llamar
     └────────────────────┘
              │
              ▼ Fidelidad
```

### La regla de oro

> **Usa el test double con la MAYOR fidelidad que sea práctico.**
>
> Si la implementación real es rápida y determinística → úsala.
> Si no, baja al siguiente nivel: container → fake → stub → mock.
> Solo usa mocks cuando necesitas verificar *interacciones*, no *resultados*.

---

## 3. Regla fundamental: mockear solo lo que duele

Las dependencias se mockean para eliminar **dolor**, no por dogma. Los 4 tipos de dolor que justifican un mock:

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  ┌─────────┐  ┌──────────────┐  ┌──────────┐  ┌──────────┐ │
│  │  LENTO   │  │ NO DETERMI-  │  │  SIDE     │  │ NO DISPO-│ │
│  │          │  │ NÍSTICO      │  │  EFFECTS  │  │ NIBLE    │ │
│  │ >100ms   │  │              │  │           │  │          │ │
│  │ por test │  │ Resultados   │  │ Cobra $,  │  │ Requiere │ │
│  │          │  │ diferentes   │  │ envía     │  │ VPN, HW, │ │
│  │ Red, DB, │  │ cada vez     │  │ emails,   │  │ licencia │ │
│  │ FS lento │  │              │  │ borra     │  │ externa  │ │
│  │          │  │ Tiempo,      │  │ datos     │  │          │ │
│  │          │  │ random,      │  │           │  │ CI/CD no │ │
│  │          │  │ race conds   │  │           │  │ tiene    │ │
│  │          │  │              │  │           │  │ acceso   │ │
│  └─────────┘  └──────────────┘  └──────────┘  └──────────┘ │
│      │              │                │              │        │
│      └──────────────┴────────────────┴──────────────┘        │
│                           │                                  │
│               Si al menos UNO aplica:                        │
│               → justificado usar test double                 │
│                                                              │
│               Si NINGUNO aplica:                             │
│               → usa la implementación real                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. Criterio 1: velocidad — ¿es lento?

### Qué es "lento" para un test

```
┌──────────────────────────┬─────────────────────────────────────┐
│ Tiempo por test          │ Veredicto                           │
├──────────────────────────┼─────────────────────────────────────┤
│ < 1 ms                   │ Ideal. Usa implementación real.     │
│ 1–10 ms                  │ Aceptable. Usa real si es simple.   │
│ 10–100 ms                │ Tolerable para pocos tests.         │
│                          │ Fake para muchos tests.             │
│ 100 ms – 1 s             │ Lento. Fake o container.            │
│ > 1 s                    │ Inaceptable. Fake obligatorio.      │
│ > 10 s                   │ Solo en test de integración aislado.│
└──────────────────────────┴─────────────────────────────────────┘

Contexto: 500 tests × 100ms = 50 segundos
          500 tests × 1ms  = 0.5 segundos
```

### Qué es rápido y no necesita mock

```rust
// ✓ Funciones puras: NUNCA mockear
pub fn validate_email(email: &str) -> bool {
    let parts: Vec<&str> = email.split('@').collect();
    parts.len() == 2 && !parts[0].is_empty() && parts[1].contains('.')
}

// ✓ Colecciones estándar: NUNCA mockear
pub fn dedup_and_sort(mut items: Vec<String>) -> Vec<String> {
    items.sort();
    items.dedup();
    items
}

// ✓ Parsing: NUNCA mockear
pub fn parse_config(content: &str) -> Result<Config, ParseError> {
    // parsear TOML/JSON/YAML
    toml::from_str(content).map_err(|e| ParseError(e.to_string()))
}

// ✓ Criptografía: NUNCA mockear (excepto para inyectar claves de test)
pub fn hash_password(password: &str, salt: &[u8]) -> Vec<u8> {
    // argon2, bcrypt, etc.
    argon2::hash_encoded(password.as_bytes(), salt, &argon2::Config::default())
        .unwrap()
        .into_bytes()
}

// ✓ Serialización: NUNCA mockear
pub fn to_json<T: Serialize>(value: &T) -> String {
    serde_json::to_string(value).unwrap()
}
```

### Qué es lento y se beneficia de mock/fake

```rust
// ✗ Base de datos: latencia de red + disco
// Una query simple: 1-50ms. Con setup: 100ms+
pub fn get_user(conn: &PgConnection, id: i64) -> Option<User> {
    // SELECT * FROM users WHERE id = $1
    // → mockear con InMemoryUserStore
}

// ✗ HTTP externo: latencia de red impredecible
// Una request: 50-5000ms
pub fn fetch_weather(city: &str) -> Result<Weather, Error> {
    reqwest::blocking::get(&format!("https://api.weather.com/{}", city))
    // → mockear con FakeWeatherApi o stub
}

// ✗ Filesystem lento (NFS, disco rotatorio)
// Leer un archivo: 1-100ms
pub fn read_large_config(path: &str) -> Config {
    // → fake con InMemoryFileSystem
}

// ✗ Proceso externo
// Invocar un binario: 50-500ms por spawn
pub fn run_linter(file: &str) -> Vec<LintError> {
    Command::new("clippy").arg(file).output()
    // → stub con resultado predefinido
}
```

---

## 5. Criterio 2: determinismo — ¿es no-determinístico?

### Fuentes de no-determinismo

```
┌──────────────────┬────────────────────────────────┬─────────────────────┐
│ Fuente           │ Ejemplo                        │ Solución            │
├──────────────────┼────────────────────────────────┼─────────────────────┤
│ Tiempo           │ SystemTime::now()              │ Inyectar Clock      │
│ Aleatorio        │ rand::random()                 │ Inyectar Rng con    │
│                  │                                │ seed fijo           │
│ Concurrencia     │ Race conditions en threads     │ Tests secuenciales  │
│                  │                                │ o sync primitives   │
│ Red              │ DNS resolution, timeouts       │ Fake/stub/container │
│ Estado global    │ Variables de entorno,           │ Isolar en setup/    │
│                  │ archivos compartidos            │ teardown            │
│ Orden de HashMap │ HashMap::iter() no es estable  │ BTreeMap o sort     │
│ Flotantes        │ Errores de redondeo            │ Epsilon comparison  │
└──────────────────┴────────────────────────────────┴─────────────────────┘
```

### Ejemplo: tiempo como dependencia

```rust
// ✗ Dependencia directa al reloj: no determinístico
pub struct TokenService;

impl TokenService {
    pub fn create_token(&self, user_id: u64, ttl_secs: u64) -> Token {
        let now = SystemTime::now();
        Token {
            value: format!("tok_{}", user_id),
            expires_at: now + Duration::from_secs(ttl_secs),
        }
    }

    pub fn is_valid(&self, token: &Token) -> bool {
        SystemTime::now() < token.expires_at
    }
}

// El test de is_valid es flaky:
#[test]
fn test_is_valid_flaky() {
    let service = TokenService;
    let token = service.create_token(1, 1); // TTL = 1 segundo
    assert!(service.is_valid(&token)); // ¿pasan 0ms o 999ms aquí?
    // A veces falla si la máquina está cargada
}

// ✓ Inyectar el reloj
pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub struct RealClock;
impl Clock for RealClock {
    fn now(&self) -> SystemTime { SystemTime::now() }
}

pub struct FakeClock {
    current: std::cell::Cell<SystemTime>,
}

impl FakeClock {
    pub fn new(time: SystemTime) -> Self {
        FakeClock { current: std::cell::Cell::new(time) }
    }

    pub fn advance(&self, duration: Duration) {
        let new = self.current.get() + duration;
        self.current.set(new);
    }
}

impl Clock for FakeClock {
    fn now(&self) -> SystemTime { self.current.get() }
}

pub struct TokenService<C: Clock> {
    clock: C,
}

impl<C: Clock> TokenService<C> {
    pub fn new(clock: C) -> Self { TokenService { clock } }

    pub fn create_token(&self, user_id: u64, ttl_secs: u64) -> Token {
        let now = self.clock.now();
        Token {
            value: format!("tok_{}", user_id),
            expires_at: now + Duration::from_secs(ttl_secs),
        }
    }

    pub fn is_valid(&self, token: &Token) -> bool {
        self.clock.now() < token.expires_at
    }
}

// Test determinístico con FakeClock (NO mock — es un fake)
#[test]
fn test_is_valid_deterministic() {
    let epoch = SystemTime::UNIX_EPOCH + Duration::from_secs(1_000_000);
    let clock = FakeClock::new(epoch);
    let service = TokenService::new(&clock);

    let token = service.create_token(1, 60); // TTL = 60 segundos

    // Token recién creado: válido
    assert!(service.is_valid(&token));

    // Avanzar 30 segundos: aún válido
    clock.advance(Duration::from_secs(30));
    assert!(service.is_valid(&token));

    // Avanzar otros 31 segundos (total 61): expirado
    clock.advance(Duration::from_secs(31));
    assert!(!service.is_valid(&token));
}
```

**Nota**: `FakeClock` no es un mock. No tiene `expect_*`. Tiene **lógica interna** (mantener y avanzar el tiempo). Es un fake — una implementación alternativa con comportamiento real pero controlable.

---

## 6. Criterio 3: efectos secundarios — ¿tiene side effects?

### Side effects que justifican un double

```
┌──────────────────────────────────────────────────────────────────┐
│ Side effects peligrosos en tests:                                │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  COBRA $$$   │  │ ENVÍA COSAS  │  │ MODIFICA     │          │
│  │              │  │              │  │ ESTADO       │          │
│  │ Stripe API   │  │ Emails       │  │              │          │
│  │ AWS services │  │ SMS          │  │ Archivos     │          │
│  │ Twilio       │  │ Webhooks     │  │ BD compart.  │          │
│  │ Payment GW   │  │ Push notifs  │  │ Registros    │          │
│  │              │  │ Slack msgs   │  │ DNS          │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│         │                 │                 │                    │
│         └─────────────────┴─────────────────┘                    │
│                           │                                      │
│              SIEMPRE usar test double                             │
│              (fake, stub, o mock)                                │
│                                                                  │
│  Nunca correr un test que cobra dinero real,                     │
│  envía un email real a un usuario real,                          │
│  o modifica datos de producción.                                 │
└──────────────────────────────────────────────────────────────────┘
```

### Side effects aceptables en tests

```rust
// ✓ Escribir archivos temporales: ok con tempdir
use tempfile::TempDir;

#[test]
fn test_file_creation() {
    let dir = TempDir::new().unwrap();
    let path = dir.path().join("test.txt");

    create_report(&path, &data);

    let content = std::fs::read_to_string(&path).unwrap();
    assert!(content.contains("Total: $100"));
    // TempDir se limpia automáticamente al salir del scope
}

// ✓ SQLite in-memory: ok, rápido y aislado
#[test]
fn test_with_sqlite_memory() {
    let conn = Connection::open_in_memory().unwrap();
    conn.execute("CREATE TABLE users (id INTEGER, name TEXT)", []).unwrap();

    // Usar la implementación REAL del repositorio
    let repo = SqliteUserRepo::new(conn);
    repo.insert(&User { id: 1, name: "Alice".into() });

    assert_eq!(repo.find_by_id(1).unwrap().name, "Alice");
    // SQLite in-memory desaparece al salir del scope
}

// ✓ Log a stdout/stderr: ok, no tiene consecuencias
#[test]
fn test_with_logging() {
    env_logger::init();
    let service = MyService::new();
    service.process(); // puede hacer log::info!()
    // Los logs a stdout no son side effects peligrosos
}
```

### La pregunta clave

```
¿Puedo ejecutar este test 1000 veces en CI sin consecuencias?

  SÍ  → usa la implementación real
  NO  → ¿por qué no?
         │
         ├── Cobra dinero       → SIEMPRE mockear/fake
         ├── Envía mensajes     → SIEMPRE mockear/fake
         ├── Modifica estado
         │   compartido         → SIEMPRE mockear/fake o aislar
         ├── Es lento           → Fake o container
         └── Es flaky           → Fake con semilla fija
```

---

## 7. Criterio 4: disponibilidad — ¿está siempre disponible?

```
┌──────────────────────────────────────────────────────────────────┐
│ Dependencias que pueden no estar disponibles:                    │
│                                                                  │
│  En desarrollo local:                                            │
│  - Base de datos corporativa (requiere VPN)                      │
│  - API de terceros (requiere API key, rate limits)               │
│  - Hardware específico (sensor, HSM, GPU)                        │
│  - Licencia de software (Oracle DB, servicio pago)               │
│                                                                  │
│  En CI/CD:                                                       │
│  - Red interna de la empresa                                     │
│  - Servicios en staging (pueden estar caídos)                    │
│  - Bases de datos compartidas (otro dev las está usando)         │
│  - Recursos cloud (permisos IAM del runner)                      │
│                                                                  │
│  Solución:                                                       │
│  - Fake para tests unitarios (siempre disponible)                │
│  - Container para tests de integración (Docker en CI)            │
│  - Tests de humo contra el real (solo en staging, no bloquean)   │
└──────────────────────────────────────────────────────────────────┘
```

### Ejemplo: API externa con rate limit

```rust
// La API real tiene rate limit de 100 req/min
// Un `cargo test` con 50 tests que la usan → puede fallar

// ✗ Dependencia directa
pub fn get_exchange_rate(from: &str, to: &str) -> Result<f64, Error> {
    let url = format!("https://api.exchangerate.host/convert?from={}&to={}", from, to);
    let resp: serde_json::Value = reqwest::blocking::get(&url)?.json()?;
    Ok(resp["result"].as_f64().unwrap())
}

// ✓ Abstraer con trait + fake
pub trait ExchangeRateProvider {
    fn get_rate(&self, from: &str, to: &str) -> Result<f64, String>;
}

pub struct FixedRateProvider {
    rates: HashMap<(String, String), f64>,
}

impl FixedRateProvider {
    pub fn new() -> Self {
        let mut rates = HashMap::new();
        rates.insert(("USD".into(), "EUR".into()), 0.85);
        rates.insert(("EUR".into(), "USD".into()), 1.18);
        rates.insert(("USD".into(), "GBP".into()), 0.73);
        FixedRateProvider { rates }
    }
}

impl ExchangeRateProvider for FixedRateProvider {
    fn get_rate(&self, from: &str, to: &str) -> Result<f64, String> {
        self.rates
            .get(&(from.to_string(), to.to_string()))
            .copied()
            .ok_or(format!("no rate for {}/{}", from, to))
    }
}
```

---

## 8. El diagrama de decisión completo

```
¿Necesitas testear código que usa una dependencia externa?
                    │
                    ▼
            ┌───────────────┐
            │ ¿Es una       │
            │ función pura? │
            │ (sin estado,  │
            │  sin I/O)     │
            └───┬───────┬───┘
               SÍ       NO
                │        │
                ▼        ▼
          Testear     ┌───────────────┐
          directa-    │ ¿Es rápido?   │
          mente       │ (<10ms)       │
          sin mock    └───┬───────┬───┘
                         SÍ       NO
                          │        │
                          ▼        │
                   ┌────────────┐  │
                   │ ¿Es deter- │  │
                   │ minístico? │  │
                   └──┬─────┬──┘  │
                     SÍ     NO    │
                      │     │     │
                      ▼     │     │
               ┌───────────┐│     │
               │¿Tiene side││     │
               │ effects   ││     │
               │ peligrosos?│     │
               └─┬──────┬──┘│     │
                NO      SÍ  │     │
                 │      │   │     │
                 ▼      │   │     │
            USAR REAL   │   │     │
            (sin mock)  │   │     │
                        │   │     │
                        ▼   ▼     ▼
                 ┌───────────────────┐
                 │ Necesita test     │
                 │ double. ¿Cuál?   │
                 └───────┬──────────┘
                         │
              ┌──────────┼──────────────┐
              │          │              │
              ▼          ▼              ▼
     ┌──────────┐  ┌──────────┐  ┌──────────┐
     │ ¿La dep  │  │ ¿Solo    │  │ ¿Necesitas│
     │ tiene    │  │ necesitas│  │ verificar │
     │ semántica│  │ que "no  │  │ cuántas   │
     │ compleja?│  │ falle"?  │  │ veces se  │
     │ (queries,│  │          │  │ llamó?    │
     │ state)   │  │          │  │           │
     └──┬───┬──┘  └──┬───┬──┘  └──┬────┬──┘
       SÍ   NO      SÍ   NO     SÍ    NO
        │    │       │    │      │     │
        ▼    ▼       ▼    │      ▼     │
      FAKE  STUB   STUB   │    MOCK    │
                          │            │
                          ▼            ▼
                    Evaluar más      STUB
                    los requisitos   o FAKE
```

### Tabla resumen de decisión

```
┌────────────────────────────┬───────────┬───────────┬───────────┬───────────┐
│ Tipo de dependencia        │ Rápido    │ Determi-  │ Sin side  │ Estrategia│
│                            │           │ nístico   │ effects   │           │
├────────────────────────────┼───────────┼───────────┼───────────┼───────────┤
│ Función pura               │ ✓         │ ✓         │ ✓         │ REAL      │
│ Colección estándar         │ ✓         │ ✓         │ ✓         │ REAL      │
│ Regex, parsing             │ ✓         │ ✓         │ ✓         │ REAL      │
│ Crypto (hash, sign)        │ ✓         │ ✓         │ ✓         │ REAL      │
│ Serialización (JSON, etc.) │ ✓         │ ✓         │ ✓         │ REAL      │
│ SQLite in-memory           │ ✓         │ ✓         │ ✓         │ REAL      │
│ Filesystem local (tempdir) │ ✓         │ ✓         │ ~ (temp)  │ REAL      │
├────────────────────────────┼───────────┼───────────┼───────────┼───────────┤
│ SystemTime::now()          │ ✓         │ ✗         │ ✓         │ FAKE      │
│ rand::random()             │ ✓         │ ✗         │ ✓         │ FAKE seed │
│ UUID generation            │ ✓         │ ✗         │ ✓         │ FAKE seq  │
├────────────────────────────┼───────────┼───────────┼───────────┼───────────┤
│ PostgreSQL/MySQL           │ ✗         │ ~ (txn)   │ ✗ (datos) │ FAKE o    │
│                            │           │           │           │ CONTAINER │
│ Redis/Memcached            │ ~         │ ✓         │ ✗         │ FAKE o    │
│                            │           │           │           │ CONTAINER │
│ Elasticsearch              │ ✗         │ ~         │ ✗         │ CONTAINER │
├────────────────────────────┼───────────┼───────────┼───────────┼───────────┤
│ API HTTP externa           │ ✗         │ ✗         │ ✗         │ FAKE/STUB │
│ Servicio de email          │ ✗         │ ✓         │ ✗ (envío) │ STUB/MOCK │
│ Payment gateway            │ ✗         │ ✗         │ ✗ ($$$)   │ STUB/MOCK │
│ SMS provider               │ ✗         │ ✓         │ ✗ (envío) │ STUB/MOCK │
│ Push notifications         │ ✗         │ ✓         │ ✗ (envío) │ STUB/MOCK │
└────────────────────────────┴───────────┴───────────┴───────────┴───────────┘
```

---

## 9. Preferir la implementación real cuando es rápido

### Caso 1: SQLite in-memory en lugar de mock de DB

```rust
// La dependencia
pub trait UserRepository {
    fn insert(&self, user: &User) -> Result<u64, DbError>;
    fn find_by_id(&self, id: u64) -> Result<Option<User>, DbError>;
    fn find_by_email(&self, email: &str) -> Result<Option<User>, DbError>;
    fn update(&self, user: &User) -> Result<(), DbError>;
    fn delete(&self, id: u64) -> Result<(), DbError>;
}

// ✗ Con mock: solo verifica que se llaman los métodos
#[test]
fn test_register_user_with_mock() {
    let mut mock = MockUserRepository::new();
    mock.expect_find_by_email()
        .returning(|_| Ok(None)); // "no existe"
    mock.expect_insert()
        .returning(|_| Ok(1));    // "insertado con id=1"

    let service = UserService::new(mock);
    let result = service.register("alice@test.com", "Alice");
    assert!(result.is_ok());
    // ¿Qué testeamos? Que register() llama find_by_email y luego insert.
    // NO testeamos que el usuario realmente se persiste.
    // Si la query SQL tiene un bug, este test pasa.
}

// ✓ Con SQLite in-memory: verifica la integración real
use rusqlite::Connection;

pub struct SqliteUserRepo {
    conn: Connection,
}

impl SqliteUserRepo {
    pub fn new(conn: Connection) -> Self {
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE
            )"
        ).unwrap();
        SqliteUserRepo { conn }
    }
}

impl UserRepository for SqliteUserRepo {
    fn insert(&self, user: &User) -> Result<u64, DbError> {
        self.conn.execute(
            "INSERT INTO users (name, email) VALUES (?1, ?2)",
            rusqlite::params![user.name, user.email],
        ).map_err(|e| DbError::InsertFailed(e.to_string()))?;
        Ok(self.conn.last_insert_rowid() as u64)
    }

    fn find_by_email(&self, email: &str) -> Result<Option<User>, DbError> {
        let mut stmt = self.conn.prepare(
            "SELECT id, name, email FROM users WHERE email = ?1"
        ).map_err(|e| DbError::QueryFailed(e.to_string()))?;

        let user = stmt.query_row(rusqlite::params![email], |row| {
            Ok(User {
                id: row.get(0)?,
                name: row.get(1)?,
                email: row.get(2)?,
            })
        }).optional()
        .map_err(|e| DbError::QueryFailed(e.to_string()))?;

        Ok(user)
    }

    // ... otros métodos
}

#[test]
fn test_register_user_with_real_sqlite() {
    let conn = Connection::open_in_memory().unwrap();
    let repo = SqliteUserRepo::new(conn);
    let service = UserService::new(repo);

    // Registro exitoso
    let result = service.register("alice@test.com", "Alice");
    assert!(result.is_ok());

    // Verificar que realmente se guardó
    let user = service.find_by_email("alice@test.com");
    assert_eq!(user.unwrap().name, "Alice");

    // Registrar duplicado → error
    let result = service.register("alice@test.com", "Alice2");
    assert!(matches!(result, Err(RegisterError::EmailAlreadyExists)));
}
// Este test es ~1ms (SQLite in-memory) y verifica la integración REAL
```

### Caso 2: HashMap como caché real

```rust
use std::collections::HashMap;

// No necesitas un mock de caché — usa un HashMap real
pub struct CacheService {
    cache: HashMap<String, CachedValue>,
    ttl_secs: u64,
}

#[derive(Clone)]
struct CachedValue {
    data: String,
    inserted_at: std::time::Instant,
}

impl CacheService {
    pub fn new(ttl_secs: u64) -> Self {
        CacheService {
            cache: HashMap::new(),
            ttl_secs,
        }
    }

    pub fn get(&self, key: &str) -> Option<&str> {
        self.cache.get(key).and_then(|v| {
            if v.inserted_at.elapsed().as_secs() < self.ttl_secs {
                Some(v.data.as_str())
            } else {
                None
            }
        })
    }

    pub fn set(&mut self, key: String, value: String) {
        self.cache.insert(key, CachedValue {
            data: value,
            inserted_at: std::time::Instant::now(),
        });
    }
}

// Los tests usan el CacheService REAL — es un HashMap en memoria
#[test]
fn test_cache_hit() {
    let mut cache = CacheService::new(3600);
    cache.set("key1".into(), "value1".into());
    assert_eq!(cache.get("key1"), Some("value1"));
}

#[test]
fn test_cache_miss() {
    let cache = CacheService::new(3600);
    assert_eq!(cache.get("nonexistent"), None);
}
```

### Caso 3: regex y parsing real

```rust
use regex::Regex;

// No mockees la regex — es rápida y determinística
pub struct LogParser {
    pattern: Regex,
}

impl LogParser {
    pub fn new() -> Self {
        LogParser {
            pattern: Regex::new(
                r"^\[(\d{4}-\d{2}-\d{2})\] \[(\w+)\] (.+)$"
            ).unwrap(),
        }
    }

    pub fn parse(&self, line: &str) -> Option<LogEntry> {
        let caps = self.pattern.captures(line)?;
        Some(LogEntry {
            date: caps[1].to_string(),
            level: caps[2].to_string(),
            message: caps[3].to_string(),
        })
    }
}

// Tests usan el parser REAL
#[test]
fn test_parse_info_log() {
    let parser = LogParser::new();
    let entry = parser.parse("[2024-01-15] [INFO] Server started").unwrap();
    assert_eq!(entry.level, "INFO");
    assert_eq!(entry.message, "Server started");
}

#[test]
fn test_parse_invalid_line() {
    let parser = LogParser::new();
    assert!(parser.parse("not a log line").is_none());
}
```

---

## 10. Fake implementations: la alternativa superior al mock

Un **fake** es una implementación alternativa que tiene lógica real pero simplificada. A diferencia de un mock, un fake no verifica interacciones — proporciona **comportamiento**.

```
┌──────────────────────────────────────────────────────────────────┐
│                    Mock vs Fake                                  │
├──────────────────────────────┬───────────────────────────────────┤
│         MOCK                 │            FAKE                   │
├──────────────────────────────┼───────────────────────────────────┤
│ Programa para UN test        │ Reutilizable en MUCHOS tests     │
│                              │                                   │
│ mock.expect_find()           │ let db = InMemoryDb::new();       │
│   .with(eq(42))              │ db.insert(user);                  │
│   .returning(|_| user)       │ // find_by_id funciona de verdad │
│                              │                                   │
│ Configuración por test       │ Configuración por inserción      │
│ (expectativas)               │ de datos reales                   │
│                              │                                   │
│ Verifica: "¿se llamó?"      │ Verifica: "¿el resultado es      │
│ "¿con qué args?"            │ correcto?"                        │
│ "¿cuántas veces?"           │                                   │
│                              │                                   │
│ Fragilidad: ALTA             │ Fragilidad: BAJA                  │
│ (acoplado a implementación)  │ (solo al contrato del trait)      │
│                              │                                   │
│ Mantenimiento: POR TEST     │ Mantenimiento: UNA VEZ            │
│                              │ (el fake se escribe una vez       │
│                              │  y se usa en todos los tests)     │
└──────────────────────────────┴───────────────────────────────────┘
```

### Cuándo un fake es mejor que un mock

```
Fake > Mock cuando:

  1. La dependencia tiene ESTADO
     (bases de datos, caches, colas, sesiones)

  2. Los tests necesitan INSERTAR DATOS y luego LEERLOS
     (no tiene sentido mockear "insert retorna Ok" si no verificas el read)

  3. Múltiples tests usan la MISMA DEPENDENCIA
     (escribir 50 expect_find() vs 1 InMemoryDb)

  4. La semántica es COMPLEJA
     (queries con filtros, ordenamiento, paginación)

  5. Quieres verificar COMPORTAMIENTO END-TO-END
     (el dato pasa por insert, se modifica, se lee correctamente)
```

---

## 11. InMemoryDatabase: el fake más común

```rust
use std::collections::HashMap;
use std::sync::Mutex;

// ── El trait ──────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct User {
    pub id: u64,
    pub name: String,
    pub email: String,
    pub active: bool,
}

#[derive(Debug, PartialEq)]
pub enum DbError {
    NotFound,
    DuplicateEmail,
    ConnectionError(String),
}

pub trait UserRepository: Send + Sync {
    fn insert(&self, user: &User) -> Result<u64, DbError>;
    fn find_by_id(&self, id: u64) -> Result<Option<User>, DbError>;
    fn find_by_email(&self, email: &str) -> Result<Option<User>, DbError>;
    fn find_active(&self) -> Result<Vec<User>, DbError>;
    fn update(&self, user: &User) -> Result<(), DbError>;
    fn delete(&self, id: u64) -> Result<bool, DbError>;
    fn count(&self) -> Result<usize, DbError>;
}

// ── El fake ──────────────────────────────────────────────

pub struct InMemoryUserRepo {
    users: Mutex<HashMap<u64, User>>,
    next_id: Mutex<u64>,
}

impl InMemoryUserRepo {
    pub fn new() -> Self {
        InMemoryUserRepo {
            users: Mutex::new(HashMap::new()),
            next_id: Mutex::new(1),
        }
    }

    /// Insertar datos de setup para tests
    pub fn with_users(users: Vec<User>) -> Self {
        let repo = Self::new();
        let mut store = repo.users.lock().unwrap();
        let mut next_id = repo.next_id.lock().unwrap();

        for user in users {
            let id = user.id;
            store.insert(id, user);
            if id >= *next_id {
                *next_id = id + 1;
            }
        }

        drop(store);
        drop(next_id);
        repo
    }
}

impl UserRepository for InMemoryUserRepo {
    fn insert(&self, user: &User) -> Result<u64, DbError> {
        let mut store = self.users.lock().unwrap();

        // Verificar email único (como lo haría una BD real)
        if store.values().any(|u| u.email == user.email) {
            return Err(DbError::DuplicateEmail);
        }

        let mut next_id = self.next_id.lock().unwrap();
        let id = *next_id;
        *next_id += 1;

        let mut new_user = user.clone();
        new_user.id = id;
        store.insert(id, new_user);

        Ok(id)
    }

    fn find_by_id(&self, id: u64) -> Result<Option<User>, DbError> {
        let store = self.users.lock().unwrap();
        Ok(store.get(&id).cloned())
    }

    fn find_by_email(&self, email: &str) -> Result<Option<User>, DbError> {
        let store = self.users.lock().unwrap();
        Ok(store.values().find(|u| u.email == email).cloned())
    }

    fn find_active(&self) -> Result<Vec<User>, DbError> {
        let store = self.users.lock().unwrap();
        Ok(store.values().filter(|u| u.active).cloned().collect())
    }

    fn update(&self, user: &User) -> Result<(), DbError> {
        let mut store = self.users.lock().unwrap();

        // Verificar email único excluyendo el usuario actual
        if store.values().any(|u| u.email == user.email && u.id != user.id) {
            return Err(DbError::DuplicateEmail);
        }

        if store.contains_key(&user.id) {
            store.insert(user.id, user.clone());
            Ok(())
        } else {
            Err(DbError::NotFound)
        }
    }

    fn delete(&self, id: u64) -> Result<bool, DbError> {
        let mut store = self.users.lock().unwrap();
        Ok(store.remove(&id).is_some())
    }

    fn count(&self) -> Result<usize, DbError> {
        let store = self.users.lock().unwrap();
        Ok(store.len())
    }
}

// ── Tests con el fake ─────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    fn alice() -> User {
        User { id: 0, name: "Alice".into(), email: "alice@test.com".into(), active: true }
    }

    fn bob() -> User {
        User { id: 0, name: "Bob".into(), email: "bob@test.com".into(), active: false }
    }

    #[test]
    fn insert_and_find() {
        let repo = InMemoryUserRepo::new();
        let id = repo.insert(&alice()).unwrap();
        let found = repo.find_by_id(id).unwrap().unwrap();
        assert_eq!(found.name, "Alice");
        assert_eq!(found.email, "alice@test.com");
    }

    #[test]
    fn duplicate_email_rejected() {
        let repo = InMemoryUserRepo::new();
        repo.insert(&alice()).unwrap();

        let mut alice2 = alice();
        alice2.name = "Alice 2".into();
        assert_eq!(repo.insert(&alice2), Err(DbError::DuplicateEmail));
    }

    #[test]
    fn find_active_filters_correctly() {
        let repo = InMemoryUserRepo::new();
        repo.insert(&alice()).unwrap(); // active: true
        repo.insert(&bob()).unwrap();   // active: false

        let active = repo.find_active().unwrap();
        assert_eq!(active.len(), 1);
        assert_eq!(active[0].name, "Alice");
    }

    #[test]
    fn update_changes_data() {
        let repo = InMemoryUserRepo::new();
        let id = repo.insert(&alice()).unwrap();

        let mut updated = alice();
        updated.id = id;
        updated.name = "Alice Updated".into();
        repo.update(&updated).unwrap();

        let found = repo.find_by_id(id).unwrap().unwrap();
        assert_eq!(found.name, "Alice Updated");
    }

    #[test]
    fn delete_removes_user() {
        let repo = InMemoryUserRepo::new();
        let id = repo.insert(&alice()).unwrap();

        assert!(repo.delete(id).unwrap());
        assert!(repo.find_by_id(id).unwrap().is_none());
        assert_eq!(repo.count().unwrap(), 0);
    }

    #[test]
    fn delete_nonexistent_returns_false() {
        let repo = InMemoryUserRepo::new();
        assert!(!repo.delete(999).unwrap());
    }

    // Ahora los tests del SERVICIO usan el mismo fake:

    #[test]
    fn service_register_and_find() {
        let repo = InMemoryUserRepo::new();
        let service = UserService::new(repo);

        let id = service.register("alice@test.com", "Alice").unwrap();
        let user = service.get_user(id).unwrap();
        assert_eq!(user.email, "alice@test.com");
    }

    #[test]
    fn service_rejects_duplicate_email() {
        let repo = InMemoryUserRepo::new();
        let service = UserService::new(repo);

        service.register("alice@test.com", "Alice").unwrap();
        let result = service.register("alice@test.com", "Alice 2");
        assert!(result.is_err());
    }
}
```

**Ventajas sobre mock**:
- El fake se escribe **una vez** y sirve para todos los tests
- Los tests verifican **resultados reales**, no interacciones
- Si el trait cambia, solo actualizas el fake (no 50 `expect_*`)
- El fake puede descubrir bugs que un mock enmascara (ej: email duplicado)

---

## 12. InMemoryFileSystem: abstracción del filesystem

```rust
use std::collections::HashMap;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::Mutex;

// ── Trait ─────────────────────────────────────────────────

pub trait FileSystem {
    fn read_to_string(&self, path: &Path) -> io::Result<String>;
    fn write(&self, path: &Path, content: &str) -> io::Result<()>;
    fn exists(&self, path: &Path) -> bool;
    fn remove(&self, path: &Path) -> io::Result<()>;
    fn list_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>>;
    fn create_dir_all(&self, path: &Path) -> io::Result<()>;
}

// ── Implementación real ───────────────────────────────────

pub struct OsFileSystem;

impl FileSystem for OsFileSystem {
    fn read_to_string(&self, path: &Path) -> io::Result<String> {
        std::fs::read_to_string(path)
    }

    fn write(&self, path: &Path, content: &str) -> io::Result<()> {
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        std::fs::write(path, content)
    }

    fn exists(&self, path: &Path) -> bool {
        path.exists()
    }

    fn remove(&self, path: &Path) -> io::Result<()> {
        std::fs::remove_file(path)
    }

    fn list_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>> {
        let mut entries = Vec::new();
        for entry in std::fs::read_dir(path)? {
            entries.push(entry?.path());
        }
        Ok(entries)
    }

    fn create_dir_all(&self, path: &Path) -> io::Result<()> {
        std::fs::create_dir_all(path)
    }
}

// ── Fake ──────────────────────────────────────────────────

pub struct InMemoryFileSystem {
    files: Mutex<HashMap<PathBuf, String>>,
    dirs: Mutex<Vec<PathBuf>>,
}

impl InMemoryFileSystem {
    pub fn new() -> Self {
        InMemoryFileSystem {
            files: Mutex::new(HashMap::new()),
            dirs: Mutex::new(vec![PathBuf::from("/")]),
        }
    }

    /// Pre-cargar archivos para el test
    pub fn with_files(entries: Vec<(&str, &str)>) -> Self {
        let fs = Self::new();
        for (path, content) in entries {
            fs.write(Path::new(path), content).unwrap();
        }
        fs
    }

    /// Ver todos los archivos (para debugging)
    pub fn snapshot(&self) -> HashMap<PathBuf, String> {
        self.files.lock().unwrap().clone()
    }
}

impl FileSystem for InMemoryFileSystem {
    fn read_to_string(&self, path: &Path) -> io::Result<String> {
        self.files
            .lock()
            .unwrap()
            .get(path)
            .cloned()
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, format!("{:?} not found", path)))
    }

    fn write(&self, path: &Path, content: &str) -> io::Result<()> {
        // Auto-crear directorios padre
        if let Some(parent) = path.parent() {
            self.create_dir_all(parent)?;
        }
        self.files.lock().unwrap().insert(path.to_path_buf(), content.to_string());
        Ok(())
    }

    fn exists(&self, path: &Path) -> bool {
        let files = self.files.lock().unwrap();
        let dirs = self.dirs.lock().unwrap();
        files.contains_key(path) || dirs.iter().any(|d| d == path)
    }

    fn remove(&self, path: &Path) -> io::Result<()> {
        self.files
            .lock()
            .unwrap()
            .remove(path)
            .map(|_| ())
            .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "file not found"))
    }

    fn list_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>> {
        let files = self.files.lock().unwrap();
        let entries: Vec<PathBuf> = files
            .keys()
            .filter(|p| p.parent() == Some(path))
            .cloned()
            .collect();
        Ok(entries)
    }

    fn create_dir_all(&self, path: &Path) -> io::Result<()> {
        let mut dirs = self.dirs.lock().unwrap();
        let mut current = PathBuf::new();
        for component in path.components() {
            current.push(component);
            if !dirs.iter().any(|d| d == &current) {
                dirs.push(current.clone());
            }
        }
        Ok(())
    }
}

// ── Tests que usan el fake ──────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn write_and_read() {
        let fs = InMemoryFileSystem::new();
        fs.write(Path::new("/tmp/hello.txt"), "Hello, World!").unwrap();
        let content = fs.read_to_string(Path::new("/tmp/hello.txt")).unwrap();
        assert_eq!(content, "Hello, World!");
    }

    #[test]
    fn read_nonexistent_returns_error() {
        let fs = InMemoryFileSystem::new();
        let result = fs.read_to_string(Path::new("/no/such/file"));
        assert!(result.is_err());
    }

    #[test]
    fn with_files_preloads() {
        let fs = InMemoryFileSystem::with_files(vec![
            ("/etc/config.toml", "[server]\nport = 8080"),
            ("/etc/secrets.env", "API_KEY=abc123"),
        ]);

        assert!(fs.exists(Path::new("/etc/config.toml")));
        let config = fs.read_to_string(Path::new("/etc/config.toml")).unwrap();
        assert!(config.contains("port = 8080"));
    }

    // Test de un servicio que usa el filesystem:
    #[test]
    fn config_service_reads_from_filesystem() {
        let fs = InMemoryFileSystem::with_files(vec![
            ("/app/config.toml", "[server]\nport = 3000\nhost = \"localhost\""),
        ]);

        let config_service = ConfigService::new(fs);
        let config = config_service.load("/app/config.toml").unwrap();
        assert_eq!(config.port, 3000);
        assert_eq!(config.host, "localhost");
    }
}
```

---

## 13. FakeHttpClient: servidor HTTP controlado

Para dependencias HTTP, hay tres estrategias progresivas:

```
┌──────────────────────────────────────────────────────────────────┐
│ Estrategias para testear código HTTP (de menor a mayor fidelidad)│
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. STUB: retorna respuestas fijas                               │
│     returning(|_| Ok(Response { status: 200, body: "{}" }))     │
│     → Rápido, frágil, no verifica la request                    │
│                                                                  │
│  2. FAKE: HashMap de route → response, verifica URL/método       │
│     FakeHttpClient::new()                                        │
│       .on_get("/users/1", json!({ "name": "Alice" }))           │
│       .on_post("/users", json!({ "id": 2 }))                    │
│     → Bueno para la mayoría de tests                            │
│                                                                  │
│  3. MOCK SERVER: servidor HTTP real en localhost                  │
│     wiremock::MockServer::start().await                          │
│     Mock::given(method("GET").and(path("/users/1")))             │
│       .respond_with(ResponseTemplate::new(200).set_body_json()) │
│     → Máxima fidelidad sin depender del servicio externo         │
│                                                                  │
│  4. REAL (staging): contra el servicio real en staging           │
│     → Solo para tests de humo, no para CI                       │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Fake HTTP client con HashMap

```rust
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct HttpResponse {
    pub status: u16,
    pub body: String,
    pub headers: HashMap<String, String>,
}

pub trait HttpClient {
    fn get(&self, url: &str) -> Result<HttpResponse, String>;
    fn post(&self, url: &str, body: &str) -> Result<HttpResponse, String>;
}

pub struct FakeHttpClient {
    get_responses: HashMap<String, HttpResponse>,
    post_responses: HashMap<String, HttpResponse>,
    default_response: HttpResponse,
}

impl FakeHttpClient {
    pub fn new() -> Self {
        FakeHttpClient {
            get_responses: HashMap::new(),
            post_responses: HashMap::new(),
            default_response: HttpResponse {
                status: 404,
                body: r#"{"error": "not found"}"#.to_string(),
                headers: HashMap::new(),
            },
        }
    }

    pub fn on_get(mut self, url: &str, status: u16, body: &str) -> Self {
        self.get_responses.insert(url.to_string(), HttpResponse {
            status,
            body: body.to_string(),
            headers: HashMap::new(),
        });
        self
    }

    pub fn on_post(mut self, url: &str, status: u16, body: &str) -> Self {
        self.post_responses.insert(url.to_string(), HttpResponse {
            status,
            body: body.to_string(),
            headers: HashMap::new(),
        });
        self
    }
}

impl HttpClient for FakeHttpClient {
    fn get(&self, url: &str) -> Result<HttpResponse, String> {
        Ok(self.get_responses
            .get(url)
            .cloned()
            .unwrap_or_else(|| self.default_response.clone()))
    }

    fn post(&self, url: &str, _body: &str) -> Result<HttpResponse, String> {
        Ok(self.post_responses
            .get(url)
            .cloned()
            .unwrap_or_else(|| self.default_response.clone()))
    }
}

// ── Uso en tests ──────────────────────────────────────────

#[test]
fn test_weather_service_with_fake_http() {
    let http = FakeHttpClient::new()
        .on_get(
            "https://api.weather.com/v1/current?city=Madrid",
            200,
            r#"{"temp": 25.5, "humidity": 60, "condition": "sunny"}"#,
        )
        .on_get(
            "https://api.weather.com/v1/current?city=Unknown",
            404,
            r#"{"error": "city not found"}"#,
        );

    let service = WeatherService::new(http);

    let weather = service.get_current("Madrid").unwrap();
    assert_eq!(weather.temp, 25.5);
    assert_eq!(weather.condition, "sunny");

    let result = service.get_current("Unknown");
    assert!(result.is_err());
}
```

### Mock server con wiremock (para mayor fidelidad)

```rust
// Con wiremock — un servidor HTTP real en localhost
// Cargo.toml: wiremock = "0.6" (dev-dependency)

use wiremock::{MockServer, Mock, ResponseTemplate};
use wiremock::matchers::{method, path, query_param};

#[tokio::test]
async fn test_with_mock_server() {
    // Inicia un servidor HTTP real en un puerto aleatorio
    let server = MockServer::start().await;

    // Configurar respuesta
    Mock::given(method("GET"))
        .and(path("/v1/current"))
        .and(query_param("city", "Madrid"))
        .respond_with(
            ResponseTemplate::new(200)
                .set_body_json(serde_json::json!({
                    "temp": 25.5,
                    "humidity": 60,
                    "condition": "sunny"
                }))
        )
        .mount(&server)
        .await;

    // El servicio usa la URL del mock server
    let service = WeatherService::new_with_base_url(&server.uri());
    let weather = service.get_current("Madrid").await.unwrap();

    assert_eq!(weather.temp, 25.5);
}
```

**wiremock** es un fake a nivel de red: crea un servidor HTTP real que responde a requests. Esto verifica que tu código HTTP (headers, URL encoding, método, body) es correcto.

---

## 14. FakeClock: control determinístico del tiempo

Ya vimos FakeClock en la sección 5. Aquí profundizamos en patrones avanzados:

```rust
use std::time::{Duration, SystemTime};
use std::cell::Cell;

pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub struct RealClock;
impl Clock for RealClock {
    fn now(&self) -> SystemTime { SystemTime::now() }
}

/// Fake clock con control total del tiempo
pub struct FakeClock {
    time: Cell<SystemTime>,
}

impl FakeClock {
    pub fn at(year: i32, month: u32, day: u32, hour: u32, min: u32) -> Self {
        // Construir un SystemTime específico
        // (simplificado — en producción usar chrono)
        use std::time::UNIX_EPOCH;
        // Calcular segundos desde epoch (simplificado)
        let days_since_epoch = (year as u64 - 1970) * 365 + month as u64 * 30 + day as u64;
        let secs = days_since_epoch * 86400 + hour as u64 * 3600 + min as u64 * 60;
        FakeClock {
            time: Cell::new(UNIX_EPOCH + Duration::from_secs(secs)),
        }
    }

    pub fn from_epoch(secs: u64) -> Self {
        FakeClock {
            time: Cell::new(SystemTime::UNIX_EPOCH + Duration::from_secs(secs)),
        }
    }

    pub fn advance(&self, duration: Duration) {
        self.time.set(self.time.get() + duration);
    }

    pub fn advance_secs(&self, secs: u64) {
        self.advance(Duration::from_secs(secs));
    }

    pub fn advance_minutes(&self, mins: u64) {
        self.advance(Duration::from_secs(mins * 60));
    }

    pub fn advance_hours(&self, hours: u64) {
        self.advance(Duration::from_secs(hours * 3600));
    }

    pub fn set(&self, time: SystemTime) {
        self.time.set(time);
    }
}

impl Clock for FakeClock {
    fn now(&self) -> SystemTime {
        self.time.get()
    }
}

// ── Ejemplo: rate limiter con fake clock ─────────────────

pub struct RateLimiter<C: Clock> {
    clock: C,
    requests: std::cell::RefCell<Vec<SystemTime>>,
    max_requests: usize,
    window_secs: u64,
}

impl<C: Clock> RateLimiter<C> {
    pub fn new(clock: C, max_requests: usize, window_secs: u64) -> Self {
        RateLimiter {
            clock,
            requests: std::cell::RefCell::new(Vec::new()),
            max_requests,
            window_secs,
        }
    }

    pub fn check(&self, _key: &str) -> bool {
        let now = self.clock.now();
        let window_start = now - Duration::from_secs(self.window_secs);

        let mut reqs = self.requests.borrow_mut();
        reqs.retain(|&t| t > window_start);

        if reqs.len() < self.max_requests {
            reqs.push(now);
            true  // allowed
        } else {
            false // rate limited
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn allows_within_limit() {
        let clock = FakeClock::from_epoch(1_000_000);
        let limiter = RateLimiter::new(&clock, 3, 60); // 3 req/min

        assert!(limiter.check("user:1")); // 1/3
        assert!(limiter.check("user:1")); // 2/3
        assert!(limiter.check("user:1")); // 3/3
    }

    #[test]
    fn blocks_over_limit() {
        let clock = FakeClock::from_epoch(1_000_000);
        let limiter = RateLimiter::new(&clock, 3, 60);

        assert!(limiter.check("user:1")); // 1/3
        assert!(limiter.check("user:1")); // 2/3
        assert!(limiter.check("user:1")); // 3/3
        assert!(!limiter.check("user:1")); // BLOCKED
    }

    #[test]
    fn window_expires_and_allows_again() {
        let clock = FakeClock::from_epoch(1_000_000);
        let limiter = RateLimiter::new(&clock, 3, 60);

        // Agotar el límite
        assert!(limiter.check("user:1"));
        assert!(limiter.check("user:1"));
        assert!(limiter.check("user:1"));
        assert!(!limiter.check("user:1"));

        // Avanzar 61 segundos → la ventana expiró
        clock.advance_secs(61);

        // Ahora se puede de nuevo
        assert!(limiter.check("user:1"));
        assert!(limiter.check("user:1"));
        assert!(limiter.check("user:1"));
        assert!(!limiter.check("user:1"));
    }

    #[test]
    fn partial_window_expiry() {
        let clock = FakeClock::from_epoch(1_000_000);
        let limiter = RateLimiter::new(&clock, 3, 60);

        limiter.check("user:1"); // t=0
        clock.advance_secs(20);
        limiter.check("user:1"); // t=20
        clock.advance_secs(20);
        limiter.check("user:1"); // t=40
        assert!(!limiter.check("user:1")); // t=40: BLOCKED

        // Avanzar a t=61: el primer request expiró, los otros no
        clock.advance_secs(21);
        assert!(limiter.check("user:1")); // 1 slot libre (el de t=0 expiró)
        assert!(!limiter.check("user:1")); // los de t=20 y t=40 aún cuentan
    }
}
```

**Nota**: FakeClock es un **fake**, no un mock. Tiene lógica real (mantener estado del tiempo). Nunca necesitas `expect_now().times(1).returning(...)` — eso sería un mock, y sería inferior.

---

## 15. Fakes vs Mocks: análisis profundo

### Escenario: servicio de usuarios con 10 tests

```
CON MOCKS (mockall):

  Test 1: register_user
    mock.expect_find_by_email().returning(|_| Ok(None));
    mock.expect_insert().returning(|_| Ok(1));

  Test 2: register_duplicate
    mock.expect_find_by_email().returning(|_| Ok(Some(user)));

  Test 3: get_user
    mock.expect_find_by_id().returning(|_| Ok(Some(user)));

  Test 4: get_nonexistent
    mock.expect_find_by_id().returning(|_| Ok(None));

  Test 5: update_user
    mock.expect_find_by_id().returning(|_| Ok(Some(user)));
    mock.expect_update().returning(|_| Ok(()));

  Test 6: update_nonexistent
    mock.expect_find_by_id().returning(|_| Ok(None));

  Test 7: delete_user
    mock.expect_delete().returning(|_| Ok(true));

  Test 8: list_active_users
    mock.expect_find_active().returning(|| Ok(vec![user1, user2]));

  Test 9: count_users
    mock.expect_count().returning(|| Ok(5));

  Test 10: register_then_find
    mock.expect_find_by_email().returning(|_| Ok(None));
    mock.expect_insert().returning(|_| Ok(1));
    mock.expect_find_by_id()  // ← ¿retorna el mismo usuario?
        .returning(|_| Ok(Some(/*copy-paste del user de arriba*/)));

  Total: ~60 líneas de expect_* setup
  Problemas:
  - Test 10 es frágil: el mock no vincula insert con find
  - Si cambias el trait → actualizar 10 tests
  - Los tests no detectan bugs en las queries
```

```
CON FAKE (InMemoryUserRepo):

  // El fake se define UNA VEZ (ver sección 11)

  Test 1-10: usan InMemoryUserRepo::new()

  Test 10: register_then_find
    let repo = InMemoryUserRepo::new();
    let service = UserService::new(repo);
    let id = service.register("alice@test.com", "Alice").unwrap();
    let user = service.find(id).unwrap();
    assert_eq!(user.email, "alice@test.com");
    // ← REALMENTE verifica que lo insertado se puede leer

  Total: ~30 líneas de setup (el fake)
  Ventajas:
  - El fake vincula insert↔find (como la DB real)
  - Si cambias el trait → solo actualizas el fake
  - Detecta bugs lógicos (ej: email duplicado)
```

### Métricas comparativas

```
┌──────────────────────┬─────────────────────┬─────────────────────┐
│ Métrica              │ 10 tests con mocks  │ 10 tests con fake   │
├──────────────────────┼─────────────────────┼─────────────────────┤
│ Líneas de setup      │ ~60 (6/test)        │ ~30 (3/test + fake) │
│ Cambio en trait:     │ Actualizar 10 tests │ Actualizar 1 fake   │
│ Bugs detectados      │ Solo de flujo       │ Flujo + semántica   │
│ Test 10 (round-trip) │ Frágil (copy-paste) │ Robusto (integrado) │
│ Legibilidad          │ Media (expect_*)    │ Alta (parece real)  │
│ Velocidad            │ ~0.01ms/test        │ ~0.01ms/test        │
│ Mantenimiento        │ Alto                │ Bajo                │
└──────────────────────┴─────────────────────┴─────────────────────┘
```

---

## 16. El espectro de fidelidad

```
                    Baja fidelidad ◄────────────────► Alta fidelidad
                    Baja confianza                     Alta confianza
                    Bajo costo                         Alto costo

  ┌─────┐    ┌─────┐    ┌─────┐    ┌──────────┐    ┌──────────┐
  │Dummy│    │Stub │    │Mock │    │  Fake    │    │Container │
  │     │    │     │    │     │    │          │    │          │
  │Solo │    │Retor│    │Veri-│    │HashMap   │    │PostgreSQL│
  │comp-│    │na   │    │fica │    │con lógi- │    │en Docker │
  │ila  │    │fijos│    │lla- │    │ca real   │    │          │
  └─────┘    └─────┘    │madas│    │          │    │          │
                        └─────┘    └──────────┘    └──────────┘
     │          │          │            │               │
     ▼          ▼          ▼            ▼               ▼
  Compila    "funciona"  "se llama   "funciona      "funciona
  sin error  en el happy correcta-    como la DB     como en
             path        mente"       pero más       producción"
                                      rápido"
```

### ¿Cuánta fidelidad necesitas?

```
┌────────────────────────────────────────────────────────────────┐
│ Pregunta: ¿qué pasa si el test double                          │
│ se comporta diferente del real?                                │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│ Dummy → "¿alguien lo llamó? No debería."                       │
│          Riesgo: ninguno si realmente no se llama               │
│                                                                │
│ Stub  → "El resultado fue OK, ¿pero el real también lo es?"    │
│          Riesgo: medio. Si el real retorna algo diferente,      │
│          el bug pasa desapercibido.                             │
│                                                                │
│ Mock  → "Se llamó 1 vez con arg=42, ¿pero eso es correcto?"   │
│          Riesgo: alto acoplamiento a implementación.            │
│          Refactorización rompe tests innecesariamente.          │
│                                                                │
│ Fake  → "El HashMap se comporta como la DB... ¿siempre?"      │
│          Riesgo: bajo pero real. El fake puede no implementar  │
│          un edge case (NULL handling, collation, triggers).     │
│                                                                │
│ Container → "PostgreSQL real, ¿pero la config es la misma?"    │
│          Riesgo: muy bajo. Solo difiere en config y datos       │
│          iniciales.                                             │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### La estrategia combinada

```
┌──────────────────────────────────────────────────────────────┐
│               Estrategia por capa de test                    │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  Tests unitarios (95% de los tests)                  │    │
│  │  → Fake o implementación real                        │    │
│  │  → Sub-segundo por test                              │    │
│  │  → Verifican lógica de negocio                       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌─────────────────────────────────────────────┐            │
│  │  Tests de integración (4% de los tests)      │            │
│  │  → Container o staging                       │            │
│  │  → Segundos por test                         │            │
│  │  → Verifican que las piezas encajan          │            │
│  └─────────────────────────────────────────────┘            │
│                                                              │
│  ┌─────────────────────────────────┐                        │
│  │  Tests de humo (1%)             │                        │
│  │  → Servicio REAL (staging)      │                        │
│  │  → Minutos por test             │                        │
│  │  → Verifican que prod funciona  │                        │
│  └─────────────────────────────────┘                        │
│                                                              │
│  Mocks con mockall: solo cuando necesitas verificar          │
│  INTERACCIONES (¿se llamó? ¿cuántas veces? ¿en qué orden?) │
│  que NO son verificables a través del resultado.             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 17. Contract tests: verificar que el fake es fiel

Un fake es tan útil como su fidelidad al contrato. Si el fake se desvía del comportamiento real, los tests dan una falsa sensación de seguridad. La solución: **contract tests**.

```
Contract test: un conjunto de tests que corren contra
AMBAS implementaciones (real y fake) y verifican que
se comportan igual.

  ┌──────────────────────┐
  │   Contract tests     │
  │                      │
  │ test_insert_and_find │
  │ test_duplicate_email │
  │ test_update          │
  │ test_delete          │
  │ test_count           │
  │ ...                  │
  └──────┬────────┬──────┘
         │        │
    ┌────┘        └────┐
    ▼                  ▼
┌──────────┐    ┌──────────┐
│ InMemory │    │ Postgres │
│ UserRepo │    │ UserRepo │
│ (fake)   │    │ (real)   │
└──────────┘    └──────────┘
    │                  │
    ▼                  ▼
  Ambos deben pasar los mismos tests
  Si uno pasa y el otro no → el fake es infiel
```

### Implementación en Rust

```rust
// ── El contrato: tests parametrizados ─────────────────────

#[cfg(test)]
mod contract_tests {
    use super::*;

    // Función genérica que corre los tests de contrato contra cualquier impl
    fn run_contract_tests<R: UserRepository>(repo: R) {
        // Test 1: insert y find
        let user = User {
            id: 0,
            name: "Alice".into(),
            email: "alice@test.com".into(),
            active: true,
        };
        let id = repo.insert(&user).unwrap();
        let found = repo.find_by_id(id).unwrap().unwrap();
        assert_eq!(found.name, "Alice");
        assert_eq!(found.email, "alice@test.com");

        // Test 2: find_by_email funciona
        let by_email = repo.find_by_email("alice@test.com").unwrap().unwrap();
        assert_eq!(by_email.id, id);

        // Test 3: duplicate email rechazado
        let duplicate = User {
            id: 0,
            name: "Alice2".into(),
            email: "alice@test.com".into(),
            active: true,
        };
        assert_eq!(repo.insert(&duplicate), Err(DbError::DuplicateEmail));

        // Test 4: update
        let mut updated = found.clone();
        updated.name = "Alice Updated".into();
        repo.update(&updated).unwrap();
        let after_update = repo.find_by_id(id).unwrap().unwrap();
        assert_eq!(after_update.name, "Alice Updated");

        // Test 5: delete
        assert!(repo.delete(id).unwrap());
        assert!(repo.find_by_id(id).unwrap().is_none());

        // Test 6: count
        assert_eq!(repo.count().unwrap(), 0);
    }

    // Correr contra el fake (siempre, en CI)
    #[test]
    fn contract_in_memory() {
        let repo = InMemoryUserRepo::new();
        run_contract_tests(repo);
    }

    // Correr contra la implementación real (solo en CI con DB)
    #[test]
    #[ignore] // cargo test -- --ignored para correr manualmente
    fn contract_postgres() {
        let conn = PgConnection::establish("postgres://test:test@localhost/test_db")
            .expect("Failed to connect to test database");
        let repo = PostgresUserRepo::new(conn);
        // Limpiar datos de tests anteriores
        repo.delete_all().unwrap();
        run_contract_tests(repo);
    }
}
```

### La regla de los contract tests

> Si escribes un fake, escribe contract tests.
> Si no tienes contract tests, tu fake es una apuesta.

---

## 18. Test containers: la dependencia real en Docker

Cuando el fake no es suficiente y necesitas la implementación real, pero no quieres instalar la dependencia en tu máquina:

```
┌──────────────────────────────────────────────────────────────┐
│ testcontainers: ejecutar la dependencia real en Docker       │
│                                                              │
│  ┌─────────┐    ┌──────────────┐    ┌───────────────┐      │
│  │  Test    │    │  Test-       │    │  Docker       │      │
│  │  Rust    │───►│  containers  │───►│  PostgreSQL   │      │
│  │          │    │  crate       │    │  Redis, etc.  │      │
│  └─────────┘    └──────────────┘    └───────────────┘      │
│                                                              │
│  Flujo:                                                      │
│  1. Test inicia → testcontainers crea container Docker       │
│  2. Container expone un puerto aleatorio                     │
│  3. Test se conecta a localhost:{port}                       │
│  4. Test corre con la DB real                                │
│  5. Test termina → container se destruye                     │
│                                                              │
│  Cargo.toml:                                                 │
│  [dev-dependencies]                                          │
│  testcontainers = "0.15"                                     │
│  testcontainers-modules = { version = "0.3",                │
│                              features = ["postgres"] }       │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo con PostgreSQL

```rust
use testcontainers::{clients, images};
use testcontainers_modules::postgres::Postgres;

#[test]
#[ignore] // solo correr en CI con Docker disponible
fn test_with_real_postgres() {
    let docker = clients::Cli::default();
    let postgres = docker.run(Postgres::default());

    let port = postgres.get_host_port_ipv4(5432);
    let conn_str = format!("postgres://postgres:postgres@localhost:{}/postgres", port);

    // Conectar con la implementación REAL
    let conn = PgConnection::establish(&conn_str).unwrap();
    let repo = PostgresUserRepo::new(conn);

    // Crear tabla
    repo.migrate().unwrap();

    // Test con la DB real
    let id = repo.insert(&User {
        id: 0,
        name: "Alice".into(),
        email: "alice@test.com".into(),
        active: true,
    }).unwrap();

    let user = repo.find_by_id(id).unwrap().unwrap();
    assert_eq!(user.name, "Alice");

    // Al salir del scope, el container se destruye
}
```

### Cuándo usar containers vs fakes

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Usa FAKE cuando:                         Usa CONTAINER cuando:  │
│                                                                  │
│  - Tests unitarios (95%)                  - Tests integración    │
│  - Velocidad importa                      - Queries SQL complejas│
│  - La semántica es simple                 - Triggers/procedures  │
│  - CI rápido                              - Collation/encoding   │
│  - No necesitas features                  - Transacciones        │
│    específicas de la DB                     concurrentes         │
│                                           - Índices/performance  │
│  Velocidad: <1ms/test                     Velocidad: ~1-5s setup │
│                                           + <100ms/test          │
│                                                                  │
│  Cobertura:                               Cobertura:             │
│  Lógica de negocio ✓                      Lógica de negocio ✓    │
│  Queries correctas ✗                      Queries correctas ✓    │
│  Edge cases de DB ✗                       Edge cases de DB ✓     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 19. Cuándo los mocks SÍ son la mejor opción

Los mocks no son malos — tienen usos legítimos. Un mock con mockall es la mejor opción cuando:

### 1. Necesitas verificar que una interacción OCURRE

```rust
// ¿El sistema envía un email cuando se registra un usuario?
// No puedes verificar esto con un fake (el fake recibe el email pero
// ¿cómo compruebas que se llamó?)
// → Use a spy (mock que registra llamadas)

#[test]
fn registration_sends_welcome_email() {
    let repo = InMemoryUserRepo::new();  // fake para la DB
    let mut email = MockEmailService::new();  // mock para verificar

    email.expect_send_welcome()
        .with(mockall::predicate::eq("alice@test.com"))
        .times(1)           // ← ESTO es lo que verificamos
        .returning(|_| Ok(()));

    let service = UserService::new(repo, email);
    service.register("alice@test.com", "Alice").unwrap();
}
```

### 2. Necesitas verificar que una interacción NO ocurre

```rust
// Si la validación falla, el email NO debe enviarse
#[test]
fn invalid_registration_does_not_send_email() {
    let repo = InMemoryUserRepo::new();
    let mut email = MockEmailService::new();

    email.expect_send_welcome()
        .never();  // ← NUNCA debe llamarse

    let service = UserService::new(repo, email);
    let _ = service.register("", ""); // email inválido
}
```

### 3. Necesitas verificar el ORDEN de operaciones

```rust
// La transacción debe: begin → execute → commit
// Si commit ocurre antes de execute, hay un bug
// Solo un mock con Sequence puede verificar esto

use mockall::Sequence;

#[test]
fn transaction_order_is_correct() {
    let mut tx = MockTransaction::new();
    let mut seq = Sequence::new();

    tx.expect_begin().in_sequence(&mut seq).returning(|| Ok(()));
    tx.expect_execute().in_sequence(&mut seq).returning(|_| Ok(()));
    tx.expect_commit().in_sequence(&mut seq).returning(|| Ok(()));

    process_with_transaction(&mut tx);
}
```

### 4. La dependencia tiene side effects costosos o irreversibles

```rust
// Payment gateway: NO quieres cobrar dinero real en tests
#[test]
fn payment_is_charged_correctly() {
    let mut gateway = MockPaymentGateway::new();

    gateway.expect_charge()
        .with(
            mockall::predicate::eq(42),       // user_id
            mockall::predicate::eq(9999),     // amount_cents = $99.99
        )
        .times(1)
        .returning(|_, _| Ok("tx-123".into()));

    let service = CheckoutService::new(gateway);
    service.checkout(42, 9999).unwrap();
}
```

### 5. Simular errores específicos difíciles de reproducir

```rust
// ¿Qué pasa cuando la DB retorna "connection timeout"?
// Es difícil reproducir esto con una DB real
#[test]
fn handles_database_timeout() {
    let mut repo = MockUserRepository::new();

    repo.expect_find_by_id()
        .returning(|_| Err(DbError::ConnectionError("timeout after 5s".into())));

    let service = UserService::new(repo);
    let result = service.get_user(42);
    assert!(matches!(result, Err(ServiceError::TemporaryFailure(_))));
}
```

### Resumen: mock cuando...

```
┌──────────────────────────────────────────────────────────────┐
│  Usar mock (mockall) cuando:                                 │
│                                                              │
│  ✓ Verificar que algo SE LLAMA (times(1))                    │
│  ✓ Verificar que algo NO se llama (never())                  │
│  ✓ Verificar ORDEN de llamadas (Sequence)                    │
│  ✓ Verificar ARGUMENTOS exactos (withf/with)                 │
│  ✓ Simular ERRORES difíciles de reproducir                   │
│  ✓ Side effects costosos ($$, emails, SMS)                   │
│                                                              │
│  NO usar mock cuando:                                        │
│                                                              │
│  ✗ Solo necesitas un valor de retorno → use stub/fake        │
│  ✗ La dependencia es rápida y determinística → use real      │
│  ✗ Quieres verificar RESULTADO, no interacción → use fake    │
│  ✗ La dependencia tiene lógica compleja → use fake           │
│  ✗ Necesitas round-trip (insert→find) → use fake             │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 20. El anti-patrón del mock que replica la implementación

```rust
// ✗ ANTI-PATRÓN: el mock ES la implementación
pub fn calculate_shipping(weight_kg: f64, distance_km: f64) -> f64 {
    let base = 5.0;
    let per_kg = 0.50;
    let per_km = 0.02;
    base + weight_kg * per_kg + distance_km * per_km
}

// Test con mock que replica la fórmula:
#[test]
fn test_shipping_with_mock_that_duplicates_logic() {
    let mut mock = MockShippingCalculator::new();
    mock.expect_calculate()
        .returning(|weight, distance| {
            let base = 5.0;
            let per_kg = 0.50;
            let per_km = 0.02;
            base + weight * per_kg + distance * per_km
        });

    let result = mock.calculate(10.0, 100.0);
    assert_eq!(result, 12.0);
    // ¿Qué testeamos? ¡EL MOCK! No el código real.
    // Si hay un bug en la fórmula real, este test NO lo detecta.
}

// ✓ CORRECTO: testear la función real directamente
#[test]
fn test_shipping_directly() {
    assert_eq!(calculate_shipping(10.0, 100.0), 12.0);
    assert_eq!(calculate_shipping(0.0, 0.0), 5.0);   // solo base
    assert_eq!(calculate_shipping(1.0, 0.0), 5.50);   // base + 1kg
    assert_eq!(calculate_shipping(0.0, 100.0), 7.0);  // base + 100km
}
```

### La prueba del mock infiel

```
Si cambias el mock para retornar un valor INCORRECTO
y el test sigue pasando...
→ el mock está duplicando la lógica, no verificando el sistema.

Señal:
  mock.expect_foo().returning(|x| x * 2 + 1);
  // vs
  mock.expect_foo().return_const(42);

  Si return_const(42) funciona igual de bien para tu test,
  el returning con lógica es complejidad innecesaria.
```

---

## 21. El anti-patrón del test frágil por over-specification

```rust
// ✗ ANTI-PATRÓN: especificar demasiado en el mock
#[test]
fn test_user_registration_overspecified() {
    let mut repo = MockUserRepository::new();
    let mut email = MockEmailService::new();
    let mut audit = MockAuditLog::new();

    // Verificar el argumento EXACTO de cada llamada
    repo.expect_find_by_email()
        .with(predicate::eq("alice@example.com"))
        .times(1)
        .returning(|_| Ok(None));

    repo.expect_insert()
        .withf(|user| {
            user.name == "Alice"
                && user.email == "alice@example.com"
                && user.active == true
        })
        .times(1)
        .returning(|_| Ok(42));

    email.expect_send()
        .with(
            predicate::eq("alice@example.com"),
            predicate::eq("Welcome to our platform"),
            predicate::str::contains("Dear Alice"),
        )
        .times(1)
        .returning(|_, _, _| Ok(()));

    audit.expect_log()
        .with(predicate::eq("user_registered: alice@example.com"))
        .times(1)
        .returning(|_| ());

    let service = UserService::new(repo, email, audit);
    service.register("alice@example.com", "Alice").unwrap();
}

// Problemas:
// - Cambiar el subject del email rompe el test
// - Cambiar el formato del audit log rompe el test
// - Añadir un log extra rompe el test
// - Cambiar el orden interno de las llamadas rompe el test
// - El test verifica CÓMO, no QUÉ
```

```rust
// ✓ CORRECTO: especificar solo lo esencial
#[test]
fn test_user_registration_minimal() {
    let repo = InMemoryUserRepo::new(); // fake, no mock

    let mut email = MockEmailService::new();
    email.expect_send()
        .times(1)  // verificar que SE ENVÍA, no el contenido exacto
        .returning(|_, _, _| Ok(()));

    let mut audit = MockAuditLog::new();
    audit.expect_log()
        .times(1..)  // al menos un log, no importa cuántos
        .returning(|_| ());

    let service = UserService::new(repo, email, audit);
    let result = service.register("alice@example.com", "Alice");

    // Verificar el RESULTADO, no la implementación
    assert!(result.is_ok());
    let user = service.find_by_email("alice@example.com").unwrap();
    assert_eq!(user.name, "Alice");
}
```

### La regla de fragilidad

```
Fragilidad ∝ (número de with/withf) × (número de times exactos)
                                     × (número de in_sequence)

  Más restricciones → más puntos donde el test puede romperse
  sin que el comportamiento real cambie.

  Guía:
  - times(1)       : ok si es crucial que se llame exactamente 1 vez
  - times(1..)     : mejor si solo importa que se llame
  - with(eq(...))  : ok si el argumento es parte del contrato
  - withf(|..| ..) : ok si verificas invariantes, no detalles
  - Sequence       : ok si el orden es parte del contrato (transacciones)
  - never()        : excelente para verificar que algo NO ocurre
```

---

## 22. El anti-patrón del mock omnisciente

```rust
// ✗ ANTI-PATRÓN: el test "sabe demasiado" sobre la implementación
#[test]
fn test_process_order_knows_too_much() {
    let mut db = MockDatabase::new();
    let mut cache = MockCache::new();
    let mut metrics = MockMetrics::new();

    // El test sabe que internamente:
    // 1. Primero consulta la caché
    cache.expect_get()
        .with(predicate::eq("order:42"))
        .returning(|_| None);

    // 2. Luego consulta la DB
    db.expect_find_order()
        .with(predicate::eq(42))
        .returning(|_| Ok(Some(order)));

    // 3. Guarda en caché para la próxima vez
    cache.expect_set()
        .with(predicate::eq("order:42"), predicate::always())
        .returning(|_, _| Ok(()));

    // 4. Incrementa un counter de métricas
    metrics.expect_increment()
        .with(predicate::eq("orders.fetched"))
        .returning(|_| ());

    let service = OrderService::new(db, cache, metrics);
    let result = service.get_order(42);
    assert!(result.is_ok());
}

// Si mañana cambias:
// - El formato de la cache key ("order:42" → "ord-42") → test falla
// - La estrategia de caching (write-through → lazy) → test falla
// - El nombre de la métrica → test falla
// - El orden cache→db → test falla
// NINGUNO de estos cambios afecta el comportamiento observable
```

```rust
// ✓ CORRECTO: el test solo verifica lo observable
#[test]
fn test_get_order_returns_order() {
    // Fake DB con datos reales, caché y métricas no importan
    let db = InMemoryOrderStore::with_orders(vec![
        Order { id: 42, total: 99.99, status: "pending".into() },
    ]);
    let cache = InMemoryCache::new(); // fake, no mock
    let metrics = NullMetrics;        // dummy, no registra nada

    let service = OrderService::new(db, cache, metrics);
    let order = service.get_order(42).unwrap();

    assert_eq!(order.id, 42);
    assert_eq!(order.total, 99.99);
}
```

---

## 23. Estrategias por tipo de dependencia

### Tabla de referencia definitiva

```
┌───────────────────────┬────────────────┬─────────────────────────────────┐
│ Dependencia           │ Estrategia     │ Justificación                   │
│                       │ recomendada    │                                 │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Función pura          │ REAL           │ Rápida, determinística, sin     │
│ (hash, sort, parse)   │                │ side effects                    │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Colección estándar    │ REAL           │ Vec, HashMap: son ya el fake    │
│ (Vec, HashMap)        │                │ más perfecto posible            │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Base de datos SQL     │ FAKE (InMemory)│ Para lógica de negocio.         │
│                       │ + CONTAINER    │ Container para queries complejas│
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Base de datos NoSQL   │ FAKE (HashMap) │ Misma lógica que SQL            │
│ (Redis, Mongo)        │ + CONTAINER    │                                 │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Cola de mensajes      │ FAKE (VecDeque)│ Simular la cola con una         │
│ (RabbitMQ, Kafka)     │                │ colección en memoria            │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ API HTTP externa      │ FAKE (HashMap  │ Para la mayoría de tests.       │
│                       │ route→response)│ wiremock para tests HTTP        │
│                       │ + WIREMOCK     │ específicos                     │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Email/SMS provider    │ MOCK/SPY       │ Verificar que SE ENVÍA.         │
│                       │                │ Nunca enviar realmente          │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Payment gateway       │ STUB/MOCK      │ Nunca cobrar dinero real.       │
│                       │                │ Stub para happy path,           │
│                       │                │ Mock para verificar monto       │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Sistema de archivos   │ REAL (tempdir) │ Tempdir es rápido y aislado.    │
│                       │ o FAKE         │ Fake si el FS es lento (NFS)    │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Reloj (tiempo)        │ FAKE (FakeClock│ Inyectar tiempo controlable     │
│                       │ con advance()) │ para tests determinísticos      │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Random number gen     │ FAKE (seed fijo│ StdRng::seed_from_u64(42)       │
│                       │ o secuencia)   │ para reproducibilidad           │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Logger                │ DUMMY (null)   │ Los tests no necesitan logs.    │
│                       │ o REAL (stdout)│ NullLogger o ignorar output     │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Metrics collector     │ DUMMY (null)   │ NullMetrics para tests.         │
│                       │ o FAKE (Vec)   │ Fake si necesitas verificar     │
│                       │                │ qué métricas se emiten          │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Feature flags         │ FAKE (HashMap) │ FixedFeatureFlags con values    │
│                       │                │ predefinidos                    │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Audit log             │ SPY (Vec)      │ Registrar eventos en un Vec     │
│                       │                │ para verificar después          │
├───────────────────────┼────────────────┼─────────────────────────────────┤
│ Config/env vars       │ FAKE (HashMap) │ InMemoryConfig con valores      │
│                       │                │ controlados                     │
└───────────────────────┴────────────────┴─────────────────────────────────┘
```

### Patrones comunes para cada tipo

```rust
// ── NullLogger (dummy) ────────────────────────────────────
pub struct NullLogger;
impl Logger for NullLogger {
    fn info(&self, _msg: &str) {}
    fn warn(&self, _msg: &str) {}
    fn error(&self, _msg: &str) {}
}

// ── SpyLogger (spy/fake) ──────────────────────────────────
pub struct SpyLogger {
    messages: std::cell::RefCell<Vec<(String, String)>>, // (level, msg)
}
impl SpyLogger {
    pub fn new() -> Self {
        SpyLogger { messages: std::cell::RefCell::new(Vec::new()) }
    }
    pub fn messages(&self) -> Vec<(String, String)> {
        self.messages.borrow().clone()
    }
}
impl Logger for SpyLogger {
    fn info(&self, msg: &str) {
        self.messages.borrow_mut().push(("INFO".into(), msg.into()));
    }
    fn warn(&self, msg: &str) {
        self.messages.borrow_mut().push(("WARN".into(), msg.into()));
    }
    fn error(&self, msg: &str) {
        self.messages.borrow_mut().push(("ERROR".into(), msg.into()));
    }
}

// ── NullMetrics (dummy) ───────────────────────────────────
pub struct NullMetrics;
impl Metrics for NullMetrics {
    fn increment(&self, _name: &str) {}
    fn gauge(&self, _name: &str, _value: f64) {}
    fn histogram(&self, _name: &str, _value: f64) {}
}

// ── InMemoryMetrics (fake/spy) ────────────────────────────
pub struct InMemoryMetrics {
    counters: std::cell::RefCell<HashMap<String, u64>>,
    gauges: std::cell::RefCell<HashMap<String, f64>>,
}
impl InMemoryMetrics {
    pub fn new() -> Self {
        InMemoryMetrics {
            counters: std::cell::RefCell::new(HashMap::new()),
            gauges: std::cell::RefCell::new(HashMap::new()),
        }
    }
    pub fn counter(&self, name: &str) -> u64 {
        *self.counters.borrow().get(name).unwrap_or(&0)
    }
}
impl Metrics for InMemoryMetrics {
    fn increment(&self, name: &str) {
        *self.counters.borrow_mut().entry(name.into()).or_insert(0) += 1;
    }
    fn gauge(&self, name: &str, value: f64) {
        self.gauges.borrow_mut().insert(name.into(), value);
    }
    fn histogram(&self, _name: &str, _value: f64) {}
}

// ── FixedFeatureFlags (fake) ──────────────────────────────
pub struct FixedFeatureFlags {
    flags: HashMap<String, bool>,
}
impl FixedFeatureFlags {
    pub fn new(flags: Vec<(&str, bool)>) -> Self {
        FixedFeatureFlags {
            flags: flags.into_iter().map(|(k, v)| (k.into(), v)).collect(),
        }
    }
    pub fn all_enabled() -> Self {
        Self::new(vec![])  // default true
    }
}
impl FeatureFlags for FixedFeatureFlags {
    fn is_enabled(&self, flag: &str) -> bool {
        *self.flags.get(flag).unwrap_or(&true)
    }
}

// ── InMemoryQueue (fake) ──────────────────────────────────
use std::collections::VecDeque;

pub struct InMemoryQueue {
    messages: std::cell::RefCell<VecDeque<String>>,
}
impl InMemoryQueue {
    pub fn new() -> Self {
        InMemoryQueue { messages: std::cell::RefCell::new(VecDeque::new()) }
    }
    pub fn pending(&self) -> usize {
        self.messages.borrow().len()
    }
    pub fn drain(&self) -> Vec<String> {
        self.messages.borrow_mut().drain(..).collect()
    }
}
impl MessageQueue for InMemoryQueue {
    fn publish(&self, message: &str) -> Result<(), String> {
        self.messages.borrow_mut().push_back(message.into());
        Ok(())
    }
    fn consume(&self) -> Result<Option<String>, String> {
        Ok(self.messages.borrow_mut().pop_front())
    }
}
```

---

## 24. Comparación con C y Go

### C: sin ecosistema estándar de doubles

```c
/* C: los "fakes" son implementaciones manuales con arrays */

/* Fake de una "base de datos" en C */
typedef struct {
    int id;
    char name[64];
    char email[128];
} User;

#define MAX_USERS 100
static User fake_db[MAX_USERS];
static int fake_db_count = 0;

int fake_insert(const User* user) {
    if (fake_db_count >= MAX_USERS) return -1;
    /* Verificar email duplicado */
    for (int i = 0; i < fake_db_count; i++) {
        if (strcmp(fake_db[i].email, user->email) == 0) return -2;
    }
    fake_db[fake_db_count] = *user;
    fake_db[fake_db_count].id = fake_db_count + 1;
    fake_db_count++;
    return fake_db[fake_db_count - 1].id;
}

User* fake_find(int id) {
    for (int i = 0; i < fake_db_count; i++) {
        if (fake_db[i].id == id) return &fake_db[i];
    }
    return NULL;
}

void fake_reset(void) {
    fake_db_count = 0;
    memset(fake_db, 0, sizeof(fake_db));
}

/* En C, la decisión "mock vs fake" es la misma,
   pero la implementación es más laboriosa.
   No hay framework estándar de test doubles.
   Los "mocks" son function pointers (ver T01).
   Los "fakes" son arrays/structs globales. */

/* La misma regla aplica:
   - Función pura → testear directamente (qsort, strlen)
   - Dependencia con estado → fake manual (array)
   - Side effect → function pointer stub
   - Verificar interacción → function pointer + counter global */
```

### Go: interfaces implícitas facilitan los fakes

```go
// Go: las interfaces implícitas hacen trivial crear fakes

// El "trait" (interface)
type UserRepository interface {
    FindByID(id uint64) (*User, error)
    Insert(user *User) (uint64, error)
    FindByEmail(email string) (*User, error)
}

// Fake con map (equivalente al InMemoryUserRepo de Rust)
type InMemoryRepo struct {
    users  map[uint64]*User
    nextID uint64
    mu     sync.Mutex
}

func NewInMemoryRepo() *InMemoryRepo {
    return &InMemoryRepo{
        users:  make(map[uint64]*User),
        nextID: 1,
    }
}

func (r *InMemoryRepo) Insert(user *User) (uint64, error) {
    r.mu.Lock()
    defer r.mu.Unlock()

    // Verificar email único
    for _, u := range r.users {
        if u.Email == user.Email {
            return 0, ErrDuplicateEmail
        }
    }

    user.ID = r.nextID
    r.nextID++
    r.users[user.ID] = user
    return user.ID, nil
}

func (r *InMemoryRepo) FindByID(id uint64) (*User, error) {
    r.mu.Lock()
    defer r.mu.Unlock()
    user, ok := r.users[id]
    if !ok { return nil, ErrNotFound }
    return user, nil
}

// Test con fake — la filosofía de Go favorece fakes sobre mocks
func TestRegisterUser(t *testing.T) {
    repo := NewInMemoryRepo()
    service := NewUserService(repo)

    id, err := service.Register("alice@test.com", "Alice")
    assert.NoError(t, err)

    user, err := repo.FindByID(id)
    assert.NoError(t, err)
    assert.Equal(t, "Alice", user.Name)
}

// La comunidad Go tiene un principio fuerte:
// "Accept interfaces, return structs"
// y favorece fakes sobre frameworks de mocking (gomock/testify).
// Artículo famoso: "Don't mock, use fakes" de Mitchell Hashimoto (HashiCorp)
```

### Tabla comparativa de filosofías

```
┌─────────────────────┬────────────────────┬────────────────────┬────────────────┐
│ Concepto            │ Rust               │ Go                 │ C              │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Filosofía           │ Traits explícitos  │ Interfaces          │ Function       │
│ dominante           │ + fakes/mockall    │ implícitas + fakes │ pointers       │
│                     │                    │                    │ + arrays       │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Framework de mock   │ mockall (popular)  │ gomock (oficial)   │ CMock (externo)│
│ más usado           │                    │ testify (popular)  │                │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Actitud hacia mocks │ Disponible pero    │ "Prefiere fakes"   │ "Lo que        │
│                     │ se recomienda      │ (cultura Go)       │ funcione"      │
│                     │ evaluar alternativa│                    │ (no hay guía)  │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ InMemory fakes      │ HashMap + Mutex    │ map + sync.Mutex   │ Array estático │
│                     │ (idiomático)       │ (idiomático)       │ + contador     │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Contract tests      │ Tests genéricos    │ Test functions     │ Macros o       │
│                     │ sobre trait bound  │ con interface arg  │ function ptrs  │
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Test containers     │ testcontainers-rs  │ testcontainers-go  │ No estándar    │
│                     │ (crate)            │ (módulo)           │ (Docker manual)│
├─────────────────────┼────────────────────┼────────────────────┼────────────────┤
│ Tipo de verificación│ Compile-time       │ Runtime            │ Runtime (si    │
│ de interfaces       │ (trait bounds)     │ (duck typing)      │ compila, "ok") │
└─────────────────────┴────────────────────┴────────────────────┴────────────────┘
```

---

## 25. Errores comunes

### Error 1: Mockear dependencias internas (no de frontera)

```rust
// ✗ Mockear una función privada de tu propio módulo
mod internal {
    pub(super) fn validate(x: i32) -> bool { x > 0 }
}

// Crear un trait para "mockear" validate
// NO — validate es lógica interna, no una frontera
// Testear directamente la función que la usa

// ✓ Mockear solo las fronteras del sistema:
//   - I/O (archivos, red, BD)
//   - Tiempo
//   - Aleatoriedad
//   - Servicios externos
```

### Error 2: Confundir fakes con mocks

```rust
// ✗ "Fake" que en realidad es un mock disfrazado
pub struct FakeEmailService {
    expected_to: String,
    expected_subject: String,
    was_called: bool,
}
// Esto no es un fake — es un mock manual con verificaciones

// ✓ Un fake real tiene lógica, no verificaciones
pub struct InboxEmailService {
    inbox: RefCell<Vec<Email>>,
}
impl InboxEmailService {
    pub fn new() -> Self { InboxEmailService { inbox: RefCell::new(Vec::new()) } }
    pub fn inbox(&self) -> Vec<Email> { self.inbox.borrow().clone() }
}
impl EmailService for InboxEmailService {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), String> {
        self.inbox.borrow_mut().push(Email {
            to: to.into(), subject: subject.into(), body: body.into(),
        });
        Ok(())
    }
}
// Verificar en el test: assert_eq!(service.inbox().len(), 1);
```

### Error 3: Testear el mock en lugar del código

```rust
// ✗ El test solo verifica que el mock retorna lo que le dijiste
#[test]
fn test_get_user() {
    let mut mock = MockUserRepo::new();
    mock.expect_find_by_id()
        .returning(|_| Ok(Some(User { id: 1, name: "Alice".into() })));

    let user = mock.find_by_id(1).unwrap().unwrap();
    assert_eq!(user.name, "Alice");
    // ¡Estás testeando mockall, no tu código!
}

// ✓ Testear el SERVICIO que usa el mock/fake
#[test]
fn test_service_formats_display_name() {
    let repo = InMemoryUserRepo::with_users(vec![
        User { id: 1, name: "Alice Smith".into(), email: "a@t.com".into(), active: true },
    ]);
    let service = UserService::new(repo);

    // Testear la LÓGICA del servicio, no el repositorio
    let display = service.get_display_name(1).unwrap();
    assert_eq!(display, "A. Smith"); // lógica de formateo
}
```

### Error 4: Usar mock para lógica pura por hábito

```rust
// ✗ Todo es "testable" con mocks... pero no todo debe serlo
#[automock]
pub trait MathOperations {
    fn add(&self, a: i32, b: i32) -> i32;
    fn multiply(&self, a: i32, b: i32) -> i32;
}

// Una función que usa MathOperations...
// NO. add y multiply son funciones puras.
// Testear directamente: assert_eq!(add(2, 3), 5);
```

### Error 5: Fake que diverge silenciosamente del real

```rust
// ✗ El fake no implementa una restricción del real
impl UserRepository for InMemoryUserRepo {
    fn insert(&self, user: &User) -> Result<u64, DbError> {
        // Falta: verificar que name no es vacío (la DB real tiene NOT NULL)
        // Falta: verificar longitud máxima de email (la DB real tiene VARCHAR(255))
        // Falta: verificar charset (la DB real rechaza emojis en ciertos campos)
        let mut store = self.users.lock().unwrap();
        // ... insert sin validaciones
        Ok(id)
    }
}

// ✓ Solución: contract tests (ver sección 17)
// Y documentar las limitaciones conocidas del fake
```

---

## 26. Ejemplo completo: sistema con fakes y mocks combinados

```rust
// ============================================================
// Sistema de gestión de tareas: combinando fakes y mocks
// ============================================================

use std::collections::HashMap;
use std::sync::Mutex;
use std::time::{Duration, SystemTime};

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct Task {
    pub id: u64,
    pub title: String,
    pub assignee_email: String,
    pub due_at: SystemTime,
    pub completed: bool,
}

#[derive(Debug, PartialEq)]
pub enum TaskError {
    NotFound,
    AlreadyCompleted,
    StorageError(String),
    NotificationError(String),
}

// ── Traits ───────────────────────────────────────────────────

pub trait TaskStore {
    fn insert(&self, task: &Task) -> Result<u64, String>;
    fn find_by_id(&self, id: u64) -> Result<Option<Task>, String>;
    fn find_overdue(&self, now: SystemTime) -> Result<Vec<Task>, String>;
    fn mark_completed(&self, id: u64) -> Result<(), String>;
    fn count_by_assignee(&self, email: &str) -> Result<usize, String>;
}

pub trait Notifier {
    fn send_reminder(&self, email: &str, task_title: &str) -> Result<(), String>;
    fn send_completion(&self, email: &str, task_title: &str) -> Result<(), String>;
}

pub trait Clock {
    fn now(&self) -> SystemTime;
}

pub trait AuditLog {
    fn record(&self, event: &str);
}

// ── Servicio ─────────────────────────────────────────────────

pub struct TaskManager<S, N, C, A>
where
    S: TaskStore,
    N: Notifier,
    C: Clock,
    A: AuditLog,
{
    store: S,
    notifier: N,
    clock: C,
    audit: A,
    max_tasks_per_user: usize,
}

impl<S, N, C, A> TaskManager<S, N, C, A>
where
    S: TaskStore,
    N: Notifier,
    C: Clock,
    A: AuditLog,
{
    pub fn new(store: S, notifier: N, clock: C, audit: A) -> Self {
        TaskManager {
            store,
            notifier,
            clock,
            audit,
            max_tasks_per_user: 50,
        }
    }

    pub fn create_task(
        &self,
        title: &str,
        assignee: &str,
        due_in: Duration,
    ) -> Result<u64, TaskError> {
        // Verificar límite de tareas por usuario
        let count = self.store.count_by_assignee(assignee)
            .map_err(|e| TaskError::StorageError(e))?;

        if count >= self.max_tasks_per_user {
            return Err(TaskError::StorageError(
                format!("user {} has too many tasks ({})", assignee, count)
            ));
        }

        let task = Task {
            id: 0,
            title: title.to_string(),
            assignee_email: assignee.to_string(),
            due_at: self.clock.now() + due_in,
            completed: false,
        };

        let id = self.store.insert(&task)
            .map_err(|e| TaskError::StorageError(e))?;

        self.audit.record(&format!("task_created: {} for {}", id, assignee));
        Ok(id)
    }

    pub fn complete_task(&self, id: u64) -> Result<(), TaskError> {
        let task = self.store.find_by_id(id)
            .map_err(|e| TaskError::StorageError(e))?
            .ok_or(TaskError::NotFound)?;

        if task.completed {
            return Err(TaskError::AlreadyCompleted);
        }

        self.store.mark_completed(id)
            .map_err(|e| TaskError::StorageError(e))?;

        // Best effort: notificar
        let _ = self.notifier.send_completion(&task.assignee_email, &task.title);
        self.audit.record(&format!("task_completed: {}", id));

        Ok(())
    }

    pub fn send_reminders(&self) -> Result<usize, TaskError> {
        let now = self.clock.now();
        let overdue = self.store.find_overdue(now)
            .map_err(|e| TaskError::StorageError(e))?;

        let mut sent = 0;
        for task in &overdue {
            if self.notifier.send_reminder(&task.assignee_email, &task.title).is_ok() {
                sent += 1;
            }
        }

        self.audit.record(&format!("reminders_sent: {} of {}", sent, overdue.len()));
        Ok(sent)
    }
}

// ── Fakes ────────────────────────────────────────────────────

/// FAKE: TaskStore con HashMap (lógica real, in-memory)
pub struct InMemoryTaskStore {
    tasks: Mutex<HashMap<u64, Task>>,
    next_id: Mutex<u64>,
}

impl InMemoryTaskStore {
    pub fn new() -> Self {
        InMemoryTaskStore {
            tasks: Mutex::new(HashMap::new()),
            next_id: Mutex::new(1),
        }
    }

    pub fn with_tasks(tasks: Vec<Task>) -> Self {
        let store = Self::new();
        for task in tasks {
            store.insert(&task).unwrap();
        }
        store
    }
}

impl TaskStore for InMemoryTaskStore {
    fn insert(&self, task: &Task) -> Result<u64, String> {
        let mut store = self.tasks.lock().unwrap();
        let mut next = self.next_id.lock().unwrap();
        let id = *next;
        *next += 1;

        let mut new_task = task.clone();
        new_task.id = id;
        store.insert(id, new_task);
        Ok(id)
    }

    fn find_by_id(&self, id: u64) -> Result<Option<Task>, String> {
        Ok(self.tasks.lock().unwrap().get(&id).cloned())
    }

    fn find_overdue(&self, now: SystemTime) -> Result<Vec<Task>, String> {
        let store = self.tasks.lock().unwrap();
        Ok(store.values()
            .filter(|t| !t.completed && t.due_at < now)
            .cloned()
            .collect())
    }

    fn mark_completed(&self, id: u64) -> Result<(), String> {
        let mut store = self.tasks.lock().unwrap();
        match store.get_mut(&id) {
            Some(task) => { task.completed = true; Ok(()) }
            None => Err("not found".into()),
        }
    }

    fn count_by_assignee(&self, email: &str) -> Result<usize, String> {
        let store = self.tasks.lock().unwrap();
        Ok(store.values().filter(|t| t.assignee_email == email && !t.completed).count())
    }
}

/// FAKE: Clock con tiempo controlable
pub struct FakeClock {
    time: std::cell::Cell<SystemTime>,
}

impl FakeClock {
    pub fn at_epoch(secs: u64) -> Self {
        FakeClock {
            time: std::cell::Cell::new(
                SystemTime::UNIX_EPOCH + Duration::from_secs(secs)
            ),
        }
    }

    pub fn advance(&self, d: Duration) {
        self.time.set(self.time.get() + d);
    }
}

impl Clock for FakeClock {
    fn now(&self) -> SystemTime { self.time.get() }
}

/// SPY: AuditLog que registra eventos (fake con inspección)
pub struct SpyAuditLog {
    events: std::cell::RefCell<Vec<String>>,
}

impl SpyAuditLog {
    pub fn new() -> Self {
        SpyAuditLog { events: std::cell::RefCell::new(Vec::new()) }
    }

    pub fn events(&self) -> Vec<String> {
        self.events.borrow().clone()
    }

    pub fn has_event_containing(&self, substring: &str) -> bool {
        self.events.borrow().iter().any(|e| e.contains(substring))
    }
}

impl AuditLog for SpyAuditLog {
    fn record(&self, event: &str) {
        self.events.borrow_mut().push(event.to_string());
    }
}

/// FAKE: InboxNotifier (acumula notificaciones para inspección)
pub struct InboxNotifier {
    reminders: std::cell::RefCell<Vec<(String, String)>>,
    completions: std::cell::RefCell<Vec<(String, String)>>,
}

impl InboxNotifier {
    pub fn new() -> Self {
        InboxNotifier {
            reminders: std::cell::RefCell::new(Vec::new()),
            completions: std::cell::RefCell::new(Vec::new()),
        }
    }

    pub fn reminders_sent(&self) -> Vec<(String, String)> {
        self.reminders.borrow().clone()
    }

    pub fn completions_sent(&self) -> Vec<(String, String)> {
        self.completions.borrow().clone()
    }
}

impl Notifier for InboxNotifier {
    fn send_reminder(&self, email: &str, task_title: &str) -> Result<(), String> {
        self.reminders.borrow_mut().push((email.into(), task_title.into()));
        Ok(())
    }

    fn send_completion(&self, email: &str, task_title: &str) -> Result<(), String> {
        self.completions.borrow_mut().push((email.into(), task_title.into()));
        Ok(())
    }
}

// ── Tests ────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    // Helper
    fn base_epoch() -> u64 { 1_700_000_000 }

    // ── Tests con FAKES (sin mockall) ─────────────────────

    #[test]
    fn create_task_stores_correctly() {
        let store = InMemoryTaskStore::new();
        let clock = FakeClock::at_epoch(base_epoch());
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(store, notifier, clock, audit);
        let id = manager.create_task(
            "Fix bug #123",
            "alice@dev.com",
            Duration::from_secs(3600), // due in 1 hour
        ).unwrap();

        // Verificar resultado (no interacción)
        assert!(id > 0);
    }

    #[test]
    fn complete_task_marks_as_done() {
        let store = InMemoryTaskStore::new();
        let clock = FakeClock::at_epoch(base_epoch());
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(&store, &notifier, &clock, &audit);
        let id = manager.create_task("Task", "bob@dev.com", Duration::from_secs(3600)).unwrap();

        manager.complete_task(id).unwrap();

        // Verificar con el fake directamente
        let task = store.find_by_id(id).unwrap().unwrap();
        assert!(task.completed);

        // Verificar que se notificó (usando el spy)
        let completions = notifier.completions_sent();
        assert_eq!(completions.len(), 1);
        assert_eq!(completions[0].0, "bob@dev.com");
    }

    #[test]
    fn complete_already_completed_returns_error() {
        let store = InMemoryTaskStore::new();
        let clock = FakeClock::at_epoch(base_epoch());
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(&store, &notifier, &clock, &audit);
        let id = manager.create_task("Task", "bob@dev.com", Duration::from_secs(3600)).unwrap();

        manager.complete_task(id).unwrap();
        let result = manager.complete_task(id);
        assert_eq!(result, Err(TaskError::AlreadyCompleted));
    }

    #[test]
    fn send_reminders_for_overdue_tasks() {
        let clock = FakeClock::at_epoch(base_epoch());
        let store = InMemoryTaskStore::new();
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(&store, &notifier, &clock, &audit);

        // Crear tareas con vencimiento en 1 hora
        manager.create_task("Task 1", "alice@dev.com", Duration::from_secs(3600)).unwrap();
        manager.create_task("Task 2", "bob@dev.com", Duration::from_secs(3600)).unwrap();
        manager.create_task("Task 3", "charlie@dev.com", Duration::from_secs(7200)).unwrap();

        // Avanzar 2 horas → Task 1 y Task 2 están overdue, Task 3 no
        clock.advance(Duration::from_secs(3601));

        let sent = manager.send_reminders().unwrap();
        assert_eq!(sent, 2); // solo 2 de 3

        let reminders = notifier.reminders_sent();
        assert_eq!(reminders.len(), 2);
    }

    #[test]
    fn audit_log_records_events() {
        let store = InMemoryTaskStore::new();
        let clock = FakeClock::at_epoch(base_epoch());
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(&store, &notifier, &clock, &audit);
        let id = manager.create_task("Fix", "a@b.com", Duration::from_secs(3600)).unwrap();
        manager.complete_task(id).unwrap();

        // Verificar audit con el spy
        assert!(audit.has_event_containing("task_created"));
        assert!(audit.has_event_containing("task_completed"));
    }

    #[test]
    fn task_not_found() {
        let store = InMemoryTaskStore::new();
        let clock = FakeClock::at_epoch(base_epoch());
        let notifier = InboxNotifier::new();
        let audit = SpyAuditLog::new();

        let manager = TaskManager::new(&store, &notifier, &clock, &audit);
        let result = manager.complete_task(999);
        assert_eq!(result, Err(TaskError::NotFound));
    }

    // ── Test con MOCK (solo para verificar interacción) ───

    // NOTA: este test usa mockall SOLO para el Notifier porque
    // necesitamos verificar que una notificación NO se envía
    // cuando la tarea no existe.

    use mockall::automock;

    // Re-definir solo para este test
    #[automock]
    trait NotifierMock {
        fn send_reminder(&self, email: &str, task_title: &str) -> Result<(), String>;
        fn send_completion(&self, email: &str, task_title: &str) -> Result<(), String>;
    }

    // En un proyecto real, usarías #[cfg_attr(test, automock)]
    // directamente en el trait Notifier. Aquí separamos por claridad.

    // El punto es: solo mockeas cuando verificas interacciones.
    // Para todo lo demás, fakes.
}
```

### Resumen del ejemplo

```
┌──────────────────────────────────────────────────────────────┐
│ Dependencia     │ Test double usado │ Por qué                │
├─────────────────┼───────────────────┼────────────────────────┤
│ TaskStore       │ FAKE (InMemory)   │ Tiene estado + queries │
│                 │                   │ Verificar resultados   │
│ Clock           │ FAKE (FakeClock)  │ Controlar tiempo       │
│                 │                   │ No-determinístico      │
│ AuditLog        │ SPY (SpyAuditLog) │ Inspeccionar eventos   │
│                 │                   │ Sin verificar orden    │
│ Notifier        │ FAKE (Inbox)      │ Acumular notificaciones│
│                 │ o MOCK (mockall)  │ Mock solo si verificar │
│                 │                   │ que NO se llamó        │
└─────────────────┴───────────────────┴────────────────────────┘

Mocks usados: 0 de 6 tests (solo mencionado como opción)
Fakes usados: 4 de 4 dependencias
Resultado: tests robustos que no rompen al refactorizar
```

---

## 27. Programa de práctica

### Proyecto: `inventory_system` — sistema de inventario con fakes

Implementa un sistema de inventario donde todas las dependencias externas tienen **fakes** (no mocks).

### src/lib.rs

```rust
use std::time::{Duration, SystemTime};

// ── Modelos ──────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub struct Product {
    pub sku: String,
    pub name: String,
    pub price_cents: u64,
    pub stock: u32,
    pub min_stock: u32,  // umbral para reorder
}

#[derive(Debug, Clone, PartialEq)]
pub struct StockMovement {
    pub sku: String,
    pub quantity: i32,  // positivo = entrada, negativo = salida
    pub reason: String,
    pub timestamp: SystemTime,
}

#[derive(Debug, PartialEq)]
pub enum InventoryError {
    ProductNotFound(String),
    InsufficientStock { sku: String, available: u32, requested: u32 },
    DuplicateSku(String),
    StorageError(String),
}

// ── Traits ───────────────────────────────────────────────────

pub trait ProductStore {
    fn insert(&self, product: &Product) -> Result<(), InventoryError>;
    fn find_by_sku(&self, sku: &str) -> Result<Option<Product>, String>;
    fn update_stock(&self, sku: &str, new_stock: u32) -> Result<(), String>;
    fn find_below_min_stock(&self) -> Result<Vec<Product>, String>;
    fn all_products(&self) -> Result<Vec<Product>, String>;
}

pub trait MovementLog {
    fn record(&self, movement: &StockMovement) -> Result<(), String>;
    fn history(&self, sku: &str) -> Result<Vec<StockMovement>, String>;
}

pub trait ReorderNotifier {
    fn notify_low_stock(&self, product: &Product) -> Result<(), String>;
}

pub trait Clock {
    fn now(&self) -> SystemTime;
}

// ── Servicio ─────────────────────────────────────────────────

pub struct InventoryService<P, M, R, C>
where
    P: ProductStore,
    M: MovementLog,
    R: ReorderNotifier,
    C: Clock,
{
    products: P,
    movements: M,
    reorder: R,
    clock: C,
}

impl<P, M, R, C> InventoryService<P, M, R, C>
where
    P: ProductStore,
    M: MovementLog,
    R: ReorderNotifier,
    C: Clock,
{
    pub fn new(products: P, movements: M, reorder: R, clock: C) -> Self {
        InventoryService { products, movements, reorder, clock }
    }

    /// Registrar entrada de stock
    pub fn receive_stock(&self, sku: &str, quantity: u32) -> Result<u32, InventoryError> {
        let product = self.products.find_by_sku(sku)
            .map_err(|e| InventoryError::StorageError(e))?
            .ok_or_else(|| InventoryError::ProductNotFound(sku.into()))?;

        let new_stock = product.stock + quantity;
        self.products.update_stock(sku, new_stock)
            .map_err(|e| InventoryError::StorageError(e))?;

        let _ = self.movements.record(&StockMovement {
            sku: sku.into(),
            quantity: quantity as i32,
            reason: "received".into(),
            timestamp: self.clock.now(),
        });

        Ok(new_stock)
    }

    /// Registrar salida de stock (venta, pérdida, etc.)
    pub fn dispatch_stock(
        &self,
        sku: &str,
        quantity: u32,
        reason: &str,
    ) -> Result<u32, InventoryError> {
        let product = self.products.find_by_sku(sku)
            .map_err(|e| InventoryError::StorageError(e))?
            .ok_or_else(|| InventoryError::ProductNotFound(sku.into()))?;

        if product.stock < quantity {
            return Err(InventoryError::InsufficientStock {
                sku: sku.into(),
                available: product.stock,
                requested: quantity,
            });
        }

        let new_stock = product.stock - quantity;
        self.products.update_stock(sku, new_stock)
            .map_err(|e| InventoryError::StorageError(e))?;

        let _ = self.movements.record(&StockMovement {
            sku: sku.into(),
            quantity: -(quantity as i32),
            reason: reason.into(),
            timestamp: self.clock.now(),
        });

        // Verificar si hay que reordenar
        if new_stock < product.min_stock {
            let mut updated = product;
            updated.stock = new_stock;
            let _ = self.reorder.notify_low_stock(&updated);
        }

        Ok(new_stock)
    }

    /// Verificar qué productos necesitan reorder
    pub fn check_reorder(&self) -> Result<Vec<Product>, InventoryError> {
        let low = self.products.find_below_min_stock()
            .map_err(|e| InventoryError::StorageError(e))?;

        for product in &low {
            let _ = self.reorder.notify_low_stock(product);
        }

        Ok(low)
    }
}
```

### El estudiante debe implementar

1. **`InMemoryProductStore`**: fake con `HashMap<String, Product>`, verificando SKU duplicado, actualizando stock, filtrando por `min_stock`

2. **`InMemoryMovementLog`**: fake con `Vec<StockMovement>`, filtrando por SKU

3. **`InboxReorderNotifier`**: spy que acumula `Vec<Product>` para inspección

4. **`FakeClock`**: con `advance()` (puede reutilizar el patrón de la sección 14)

5. **Tests**:
   - Recibir stock: el stock aumenta correctamente
   - Despachar stock: el stock disminuye correctamente
   - Despacho insuficiente: error con cantidades correctas
   - Producto no encontrado: error para SKU inexistente
   - SKU duplicado al insertar: error
   - Movimientos registrados correctamente (verificar historial)
   - Reorder se notifica cuando stock < min_stock
   - Reorder NO se notifica cuando stock >= min_stock
   - `check_reorder` encuentra todos los productos bajo mínimo
   - Flujo completo: insertar producto → recibir stock → despachar → verificar historial

6. **Contract tests opcionales**: si el estudiante tiene SQLite disponible, escribir `SqliteProductStore` y verificar que los mismos tests pasan contra ambas implementaciones.

---

## 28. Ejercicios

### Ejercicio 1: Elegir la estrategia correcta

**Objetivo**: Practicar la toma de decisiones sobre qué tipo de test double usar.

Para cada dependencia, indica qué usarías (real/fake/stub/mock/dummy) y justifica:

**a)** Un servicio que calcula el hash SHA-256 de un archivo.

**b)** Un servicio que envía SMS vía Twilio API.

**c)** Un repositorio de usuarios con PostgreSQL (queries simples: insert, find_by_id, find_by_email).

**d)** Un servicio que genera UUIDs.

**e)** Un logger que escribe a stdout.

**f)** Un rate limiter que usa Redis para contar requests.

**g)** Un servicio que lee variables de entorno (`std::env::var`).

**h)** Un servicio de traducción que llama a Google Translate API (pago por uso).

**Formato de respuesta**:

```
Dependencia: [nombre]
Estrategia: [REAL / FAKE / STUB / MOCK / DUMMY]
Justificación: [1-2 frases]
```

---

### Ejercicio 2: Construir un fake completo

**Objetivo**: Practicar la construcción de un fake con lógica real.

**Contexto**: una cola de tareas prioritarias.

```rust
pub trait PriorityQueue {
    fn enqueue(&self, item: &str, priority: u8) -> Result<(), String>;
    fn dequeue(&self) -> Result<Option<(String, u8)>, String>; // item + priority
    fn peek(&self) -> Result<Option<(String, u8)>, String>;
    fn len(&self) -> Result<usize, String>;
    fn is_empty(&self) -> Result<bool, String>;
}
```

**Tareas**:

**a)** Implementa `InMemoryPriorityQueue` que use un `BTreeMap<u8, VecDeque<String>>` internamente (mayor priority = sale primero).

**b)** Verifica con tests que:
- Los items de mayor prioridad salen primero
- Items de igual prioridad salen en orden FIFO
- `peek` no consume el item
- `dequeue` en cola vacía retorna `None`
- `len` refleja el número total de items

**c)** Implementa un `TaskDispatcher` que dequeue y "ejecute" tareas. Escribe tests usando solo el fake (sin mockall).

---

### Ejercicio 3: Contract tests

**Objetivo**: Verificar que un fake se comporta igual que la implementación real.

**Contexto**:

```rust
pub trait KeyValueStore {
    fn get(&self, key: &str) -> Option<String>;
    fn set(&mut self, key: &str, value: &str);
    fn delete(&mut self, key: &str) -> bool;
    fn keys(&self) -> Vec<String>;
}
```

**Tareas**:

**a)** Implementa `InMemoryKvStore` (fake con HashMap).

**b)** Implementa `FileKvStore` que persiste en un archivo JSON (`tempfile` + serde_json).

**c)** Escribe una función genérica `fn run_kv_contract<K: KeyValueStore>(store: &mut K)` que verifica:
- Set y get retornan el valor correcto
- Overwrite actualiza el valor
- Delete retorna `true` si existía, `false` si no
- Keys retorna todas las claves activas
- Get de clave inexistente retorna `None`

**d)** Corre los contract tests contra ambas implementaciones y verifica que ambas pasan.

---

### Ejercicio 4: Refactorizar de mocks a fakes

**Objetivo**: Practicar la transición de tests basados en mocks a tests basados en fakes.

**Contexto**: dado este test con mockall:

```rust
#[test]
fn test_checkout_with_mocks() {
    let mut cart_repo = MockCartRepository::new();
    let mut product_repo = MockProductRepository::new();
    let mut payment = MockPaymentService::new();

    cart_repo.expect_get_items()
        .with(predicate::eq(1u64))
        .returning(|_| Ok(vec![
            CartItem { product_id: 10, quantity: 2 },
            CartItem { product_id: 20, quantity: 1 },
        ]));

    product_repo.expect_get_price()
        .with(predicate::eq(10u64))
        .returning(|_| Ok(1000));
    product_repo.expect_get_price()
        .with(predicate::eq(20u64))
        .returning(|_| Ok(2500));

    payment.expect_charge()
        .with(predicate::eq(1u64), predicate::eq(4500u64))
        .times(1)
        .returning(|_, _| Ok("tx-123".into()));

    cart_repo.expect_clear()
        .with(predicate::eq(1u64))
        .times(1)
        .returning(|_| Ok(()));

    let service = CheckoutService::new(cart_repo, product_repo, payment);
    let result = service.checkout(1).unwrap();
    assert_eq!(result.total_cents, 4500);
    assert_eq!(result.transaction_id, "tx-123");
}
```

**Tareas**:

**a)** Implementa `InMemoryCartRepository` y `InMemoryProductRepository` como fakes.

**b)** Reescribe el test usando los fakes en lugar de mocks. ¿Cuál es más legible?

**c)** Añade 3 tests más que serían verbosos con mocks pero concisos con fakes:
- Checkout de carrito vacío → error
- Producto en carrito pero no en catálogo → error
- Checkout del mismo carrito dos veces → segundo falla (carrito ya vacío)

**d)** El `PaymentService` sigue siendo un mock. ¿Por qué es la dependencia correcta para mockear? Escribe 1 párrafo justificando.

---

## Navegación

- **Anterior**: [T02 - mockall crate](../T02_Mockall/README.md)
- **Siguiente**: [T04 - Test doubles sin crates externos](../T04_Test_doubles_sin_crates/README.md)

---

> **Sección 4: Mocking en Rust** — Tópico 3 de 4 completado
>
> - T01: Trait-based dependency injection ✓
> - T02: mockall crate ✓
> - T03: Cuándo no mockear — preferir integración real, fakes sobre mocks ✓
> - T04: Test doubles sin crates externos (siguiente)
