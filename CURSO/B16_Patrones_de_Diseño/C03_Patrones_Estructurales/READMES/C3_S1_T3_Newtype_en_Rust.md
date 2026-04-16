# T03 — Newtype Pattern en Rust

---

## 1. Que es un newtype

Un newtype es un struct tuple de un solo campo que envuelve
un tipo existente para darle una identidad diferente:

```rust
struct Meters(f64);
struct Seconds(f64);
struct Kilograms(f64);
```

Los tres contienen `f64`, pero son tipos distintos:

```rust
fn calculate_speed(distance: Meters, time: Seconds) -> f64 {
    distance.0 / time.0
}

fn main() {
    let d = Meters(100.0);
    let t = Seconds(9.58);
    let speed = calculate_speed(d, t);

    // ERROR DE COMPILACION:
    // calculate_speed(t, d);
    // expected `Meters`, found `Seconds`
}
```

```
  Sin newtype:                     Con newtype:
  ────────────                     ────────────
  fn speed(d: f64, t: f64)        fn speed(d: Meters, t: Seconds)
  speed(9.58, 100.0)  ← compila   speed(Seconds(9.58), Meters(100.0))
  Argumentos invertidos            ← ERROR de compilacion
  Bug descubierto en produccion    Bug descubierto en compilacion
```

### 1.1 Zero cost

```rust
use std::mem::size_of;

struct UserId(u64);
struct OrderId(u64);

fn main() {
    assert_eq!(size_of::<u64>(), 8);
    assert_eq!(size_of::<UserId>(), 8);
    assert_eq!(size_of::<OrderId>(), 8);
    // Mismo tamano. El wrapper no existe en el binario.
}
```

El compilador elimina el struct wrapper completamente.
`Meters(100.0)` y `100.0_f64` generan el mismo codigo
maquina. El costo es **cero en runtime**, solo existe
en compile time.

---

## 2. Los cinco usos del newtype

### 2.1 Type safety (evitar confundir valores)

```rust
struct UserId(u64);
struct OrderId(u64);
struct ProductId(u64);

fn find_order(user: UserId, order: OrderId) -> Option<Order> {
    // ...
    todo!()
}

fn main() {
    let user = UserId(42);
    let order = OrderId(1001);
    let product = ProductId(555);

    find_order(user, order);      // ✓ Compila

    // find_order(order, user);   // ✗ ERROR: OrderId != UserId
    // find_order(product, user); // ✗ ERROR: ProductId != UserId
}
```

### 2.2 Orphan rule bypass (T02 seccion 4)

```rust
// No puedes: impl Display for Vec<u8>
// Si puedes:
struct HexBytes(Vec<u8>);
impl std::fmt::Display for HexBytes { /* ... */ }
```

### 2.3 Ocultar implementacion

```rust
// API publica: el caller solo ve "Email"
pub struct Email(String);

impl Email {
    pub fn new(s: &str) -> Result<Self, &'static str> {
        if s.contains('@') && s.contains('.') {
            Ok(Email(s.to_string()))
        } else {
            Err("invalid email")
        }
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }
}

// El caller NO puede hacer Email("invalid".into())
// Porque el campo 0 es privado (por defecto en otro modulo)
// Solo puede usar Email::new() que valida
```

### 2.4 Documentar la intencion

```rust
// f64 puede ser cualquier cosa. Newtypes documentan:
struct Percentage(f64);   // 0.0 a 100.0
struct Probability(f64);  // 0.0 a 1.0
struct Radians(f64);      // angulo
struct Pixels(f64);       // distancia en pantalla

// La firma dice exactamente que espera:
fn set_opacity(value: Probability) { /* ... */ }
// vs
fn set_opacity(value: f64) { /* 0-1? 0-100? 0-255? */ }
```

### 2.5 Agregar invariantes

```rust
/// Entero no-negativo (>= 0)
struct NonNegative(i64);

impl NonNegative {
    pub fn new(value: i64) -> Option<Self> {
        if value >= 0 { Some(Self(value)) } else { None }
    }

    pub fn get(&self) -> i64 { self.0 }
}

/// String no vacia
struct NonEmpty(String);

impl NonEmpty {
    pub fn new(s: &str) -> Option<Self> {
        if s.is_empty() { None } else { Some(Self(s.to_string())) }
    }

    pub fn as_str(&self) -> &str { &self.0 }
}

// Una vez construido, el invariante se mantiene
// (porque el campo es privado)
fn process(name: NonEmpty, count: NonNegative) {
    // Garantizado: name no esta vacio, count >= 0
    // Sin necesidad de verificar
}
```

---

## 3. Acceder al valor interno

### 3.1 Acceso directo al campo .0

```rust
struct Meters(f64);

let m = Meters(100.0);
let raw: f64 = m.0;  // Acceso directo
```

Funciona cuando el campo es pub (o dentro del mismo modulo).
Es la forma mas simple pero no siempre deseable:
- `.0` no es descriptivo
- Si el campo es privado (API publica), no funciona

### 3.2 Metodo getter

```rust
struct Email(String);

impl Email {
    pub fn as_str(&self) -> &str { &self.0 }
    pub fn into_inner(self) -> String { self.0 }
}

let email = Email::new("user@test.com").unwrap();
let s: &str = email.as_str();       // Borrow
let owned: String = email.into_inner(); // Move (consume email)
```

Convencion de nombres:
- `as_*()` → retorna referencia (`&self → &Inner`)
- `into_inner()` → consume el wrapper (`self → Inner`)
- `get()` → retorna copia si el tipo es Copy

### 3.3 impl AsRef

```rust
struct FilePath(String);

impl AsRef<str> for FilePath {
    fn as_ref(&self) -> &str { &self.0 }
}

impl AsRef<std::path::Path> for FilePath {
    fn as_ref(&self) -> &std::path::Path {
        std::path::Path::new(&self.0)
    }
}

// Funciona con cualquier funcion que acepte AsRef
fn read_file(path: &impl AsRef<std::path::Path>) -> std::io::Result<String> {
    std::fs::read_to_string(path)
}

let path = FilePath("/etc/hosts".into());
let content = read_file(&path);  // Funciona via AsRef<Path>
```

---

## 4. Deref y DerefMut: cuando SI usarlos

`Deref` permite que el newtype se comporte "como" el tipo
interno en ciertos contextos:

```rust
use std::ops::Deref;

struct Username(String);

impl Deref for Username {
    type Target = str;

    fn deref(&self) -> &str {
        &self.0
    }
}

fn main() {
    let user = Username("alice".into());

    // Deref coercion: Username se comporta como &str
    println!("Length: {}", user.len());     // str::len()
    println!("Upper: {}", user.to_uppercase()); // str::to_uppercase()

    // Funciona con funciones que esperan &str
    fn greet(name: &str) {
        println!("Hello, {}!", name);
    }
    greet(&user);  // &Username → &str via Deref
}
```

### 4.1 Como funciona deref coercion

```
  greet(&user)
  │
  ├─ greet espera &str
  ├─ &user es &Username
  ├─ ¿Username implementa Deref<Target = str>? SI
  ├─ El compilador inserta .deref() automaticamente
  └─ &user → user.deref() → &str

  Cadena de coercion (puede encadenar):
  &Username → &str (via Deref)
  &String → &str (via Deref built-in)
  &Vec<T> → &[T] (via Deref built-in)
```

### 4.2 Cuando SI implementar Deref

Regla: implementar Deref cuando el newtype es un
**smart pointer** o **wrapper transparente** cuyo unico
proposito es envolver el tipo interno:

```rust
// ✓ CORRECTO: wrapper transparente de String
struct Name(String);
impl Deref for Name {
    type Target = str;
    fn deref(&self) -> &str { &self.0 }
}
// Name "es" un string con nombre semantico
// Todos los metodos de str tienen sentido en Name

// ✓ CORRECTO: wrapper de Vec con acceso read-only
struct ReadOnlyVec<T>(Vec<T>);
impl<T> Deref for ReadOnlyVec<T> {
    type Target = [T];
    fn deref(&self) -> &[T] { &self.0 }
}
// Expone slice (read-only), oculta push/pop
```

---

## 5. Deref: cuando NO usarlo

### 5.1 Cuando el newtype agrega invariantes

```rust
// ✗ INCORRECTO: SortedVec tiene invariante de orden
struct SortedVec<T: Ord>(Vec<T>);

// Si implementas Deref<Target = Vec<T>>:
impl<T: Ord> Deref for SortedVec<T> {
    type Target = Vec<T>;
    fn deref(&self) -> &Vec<T> { &self.0 }
}
// PROBLEMA: sorted_vec.push(0) rompe el invariante!
// push() es un metodo de Vec, accesible via Deref

// Solucion: Deref a slice (no a Vec)
impl<T: Ord> Deref for SortedVec<T> {
    type Target = [T];
    fn deref(&self) -> &[T] { &self.0 }
}
// [T] no tiene push → invariante protegido
```

### 5.2 Cuando el newtype tiene semantica diferente

```rust
// ✗ INCORRECTO: Deref para tipos semanticamente diferentes
struct Meters(f64);
struct Seconds(f64);

impl Deref for Meters {
    type Target = f64;
    fn deref(&self) -> &f64 { &self.0 }
}

// PROBLEMA:
let m = Meters(100.0);
let s = Seconds(9.58);
let result = *m + *s;  // Suma metros + segundos → sin sentido!
// Deref permite deref coercion que pierde type safety
```

```
  Regla de Deref:
  ───────────────

  ¿El newtype "es-un" caso especial del tipo interno?
  ├── SI (Name es un String especial)     → Deref OK
  ├── SI (ReadOnlyVec es un [T] especial) → Deref OK
  └── NO (Meters NO es un f64)            → Deref INCORRECTO

  ¿Todos los metodos del Target tienen sentido en el newtype?
  ├── SI (str.len() tiene sentido en Name)        → Deref OK
  ├── NO (Vec.push() no tiene sentido en SortedVec) → No Deref a Vec
  └── NO (f64 + f64 no tiene sentido para Meters + Seconds) → No Deref

  En caso de duda: NO implementar Deref.
  Preferir metodos explicitos (as_str(), get(), etc.)
```

### 5.3 Tabla de decision

```
  Newtype                Target    Deref?   Por que
  ───────                ──────    ──────   ───────
  Name(String)           str       SI       "es-un" string
  Email(String)          str       QUIZA    len() ok, pero ¿replace()?
  UserId(u64)            u64       NO       semantica diferente
  Meters(f64)            f64       NO       type safety se pierde
  SortedVec(Vec<T>)      [T]      SI       slice read-only ok
  SortedVec(Vec<T>)      Vec<T>   NO       push() rompe invariante
  NonEmpty(String)       str       SI       todos los metodos de str ok
  Percentage(f64)        f64       NO       0.0-100.0 invariante
  Password(String)       str       NO       Display/Debug no deberia mostrar
```

---

## 6. DerefMut: aun mas peligroso

```rust
use std::ops::DerefMut;

struct Name(String);

impl Deref for Name {
    type Target = str;
    fn deref(&self) -> &str { &self.0 }
}

// DerefMut para String (no str — str no es Sized)
// Esto es raramente correcto para newtypes:
impl DerefMut for Name {
    fn deref_mut(&mut self) -> &mut str {
        &mut self.0
    }
}
// Ahora: name.make_ascii_uppercase() modifica directamente
// ¿Es eso deseable? Depende del caso.
```

**Regla**: si dudas sobre Deref, definitivamente no implementes
DerefMut. DerefMut expone mutacion que puede romper invariantes.

---

## 7. Derivar traits para newtypes

Problema comun: el newtype pierde todos los traits del tipo interno:

```rust
struct UserId(u64);

let a = UserId(1);
let b = UserId(1);
// a == b;  ERROR: UserId no implementa PartialEq
// println!("{}", a); ERROR: UserId no implementa Display
// HashMap::insert(a, ...); ERROR: no implementa Hash
```

### 7.1 derive para traits standard

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord)]
struct UserId(u64);

#[derive(Debug, Clone, PartialEq)]
struct Email(String);
// No Copy (String no es Copy)
// No Eq (String es Eq, pero quiza Email no deberia
//         ser case-sensitive — decision de diseno)
```

### 7.2 Implementar Display manualmente

`derive` no funciona con Display — hay que implementarlo:

```rust
use std::fmt;

struct UserId(u64);

impl fmt::Display for UserId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "user#{}", self.0)
    }
}

struct Email(String);

impl fmt::Display for Email {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Deliberadamente NO delegar a String::fmt
        // para controlar el formato
        write!(f, "{}", self.0)
    }
}
```

### 7.3 Implementar operaciones aritmeticas

Para newtypes numericos donde las operaciones tienen sentido:

```rust
use std::ops::{Add, Sub, Mul};

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
struct Meters(f64);

// Meters + Meters = Meters
impl Add for Meters {
    type Output = Self;
    fn add(self, rhs: Self) -> Self {
        Meters(self.0 + rhs.0)
    }
}

// Meters - Meters = Meters
impl Sub for Meters {
    type Output = Self;
    fn sub(self, rhs: Self) -> Self {
        Meters(self.0 - rhs.0)
    }
}

// Meters * f64 = Meters (escalar)
impl Mul<f64> for Meters {
    type Output = Self;
    fn mul(self, rhs: f64) -> Self {
        Meters(self.0 * rhs)
    }
}

// NOTA: Meters * Meters NO implementado — eso seria Meters²
// NOTA: Meters + Seconds NO existe — son tipos diferentes

fn main() {
    let a = Meters(100.0);
    let b = Meters(50.0);
    let total = a + b;            // Meters(150.0) ✓
    let scaled = a * 2.0;         // Meters(200.0) ✓
    // let wrong = a + Seconds(5.0); // ERROR ✓
    // let wrong = a * b;            // ERROR ✓ (no Meters²)
}
```

---

## 8. Patron completo: newtype con validacion

Combinar newtype + constructor privado + validacion:

```rust
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Email(String);

#[derive(Debug)]
pub enum EmailError {
    Empty,
    NoAtSign,
    NoDomain,
    InvalidChars,
}

impl Email {
    pub fn new(input: &str) -> Result<Self, EmailError> {
        let input = input.trim();
        if input.is_empty() {
            return Err(EmailError::Empty);
        }
        let parts: Vec<&str> = input.split('@').collect();
        if parts.len() != 2 {
            return Err(EmailError::NoAtSign);
        }
        if !parts[1].contains('.') {
            return Err(EmailError::NoDomain);
        }
        if input.contains(' ') {
            return Err(EmailError::InvalidChars);
        }
        // Campo privado → solo este constructor puede crear Email
        Ok(Email(input.to_lowercase()))
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn domain(&self) -> &str {
        self.0.split('@').nth(1).unwrap()
    }

    pub fn local_part(&self) -> &str {
        self.0.split('@').next().unwrap()
    }
}

impl std::fmt::Display for Email {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

// From<Email> for String — para cuando necesitas el String
impl From<Email> for String {
    fn from(e: Email) -> Self { e.0 }
}

fn send_email(to: &Email, subject: &str) {
    // `to` es GARANTIZADO valido — fue validado en Email::new()
    // No necesito re-validar
    println!("Sending '{}' to {}", subject, to);
}

fn main() {
    match Email::new("Alice@Example.COM") {
        Ok(email) => {
            println!("Valid: {}", email);         // alice@example.com
            println!("Domain: {}", email.domain()); // example.com
            send_email(&email, "Hello");
        }
        Err(e) => println!("Invalid: {:?}", e),
    }

    // No se puede crear Email invalido:
    assert!(Email::new("").is_err());
    assert!(Email::new("no-at-sign").is_err());
    assert!(Email::new("user@").is_err());
}
```

```
  Validacion una vez, seguridad siempre:
  ──────────────────────────────────────

  Input (String) ──> Email::new() ──> Email
                     │                  │
                     ├── Valida         ├── Garantizado valido
                     ├── Normaliza      ├── Campo privado
                     └── Err si invalido└── No se puede falsificar

  fn send_email(to: &Email)
  → El tipo GARANTIZA que `to` es un email valido
  → Sin validacion redundante en cada uso
  → "Parse, don't validate" (Alexis King)
```

---

## 9. Newtype en C: no existe (y por que importa)

En C, los typedefs no crean tipos nuevos:

```c
/* C: typedef NO es newtype */
typedef uint64_t UserId;
typedef uint64_t OrderId;

void find_order(UserId user, OrderId order);

find_order(order_id, user_id);  /* COMPILA — tipos iguales */
/* typedef es alias, no tipo nuevo */
```

La unica forma de "newtype" en C es un struct wrapper, pero
tiene costo ergonomico:

```c
/* C: struct wrapper (newtype manual) */
typedef struct { uint64_t value; } UserId;
typedef struct { uint64_t value; } OrderId;

void find_order(UserId user, OrderId order);

UserId u = { .value = 42 };
OrderId o = { .value = 1001 };
find_order(u, o);      /* OK */
/* find_order(o, u);  ERROR de compilacion — tipos diferentes */

/* Pero cada acceso necesita .value */
printf("User: %lu\n", u.value);
```

```
  Comparacion:

  C typedef:     typedef uint64_t UserId;
                 → alias, no type safety

  C struct:      typedef struct { uint64_t value; } UserId;
                 → type safety, pero acceso feo (.value)
                 → no zero-cost garantizado (depende de ABI)

  Rust newtype:  struct UserId(u64);
                 → type safety, .0 o metodos
                 → GARANTIZADO zero-cost
                 → puede implementar traits
```

---

## 10. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Deref para newtype con invariante | Expone metodos que rompen invariante | No Deref, o Deref a tipo read-only |
| Deref para newtype semantico (Meters→f64) | Pierde type safety | Metodos explicitos, no Deref |
| DerefMut sin pensar | Mutacion directa del interior | Casi nunca correcto para newtypes |
| Campo pub en newtype validado | Bypass del constructor | Campo privado + constructor con Result |
| Olvidar derive Debug/Clone/PartialEq | El newtype no se puede comparar/imprimir | `#[derive(...)]` los traits necesarios |
| Newtype para todo | Boilerplate excesivo | Solo cuando type safety vale la pena |
| From/Into circular | From<A> for B + From<B> for A = ambiguedad | Solo una direccion, o TryFrom |
| Display que expone datos sensibles | Password(String) mostraria la password | impl Display con masking |

---

## 11. Ejercicios

### Ejercicio 1 — Newtype basico para type safety

Define newtypes para un sistema de e-commerce:
- `CustomerId(u64)` — identificador de cliente
- `ProductId(u64)` — identificador de producto
- `Quantity(u32)` — cantidad no-negativa

**Prediccion**: ¿que traits necesita cada uno? ¿Quantity necesita
operaciones aritmeticas?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CustomerId(u64);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ProductId(u64);

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Quantity(u32);

impl CustomerId {
    pub fn new(id: u64) -> Self { Self(id) }
    pub fn get(&self) -> u64 { self.0 }
}

impl ProductId {
    pub fn new(id: u64) -> Self { Self(id) }
    pub fn get(&self) -> u64 { self.0 }
}

impl Quantity {
    pub fn new(n: u32) -> Self { Self(n) }
    pub fn get(&self) -> u32 { self.0 }
    pub fn is_zero(&self) -> bool { self.0 == 0 }
}

// Quantity + Quantity = Quantity (tiene sentido)
impl std::ops::Add for Quantity {
    type Output = Self;
    fn add(self, rhs: Self) -> Self { Quantity(self.0 + rhs.0) }
}

// Quantity - Quantity = Option<Quantity> (puede ser negativo)
impl Quantity {
    pub fn checked_sub(self, rhs: Self) -> Option<Self> {
        self.0.checked_sub(rhs.0).map(Quantity)
    }
}

impl std::fmt::Display for CustomerId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "CUST-{}", self.0)
    }
}

impl std::fmt::Display for ProductId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "PROD-{}", self.0)
    }
}

impl std::fmt::Display for Quantity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}
```

Traits comunes a los tres: Debug, Clone, Copy, PartialEq, Eq, Hash.
- IDs necesitan Hash (para HashMap keys)
- Quantity necesita Ord (para comparar cantidades) y Add
- Quantity NO usa Sub directo porque u32 - u32 puede underflow
  → usa `checked_sub` que retorna Option

</details>

---

### Ejercicio 2 — Newtype con validacion

Implementa `Port(u16)` que solo acepta valores 1-65535 (no 0).

**Prediccion**: ¿como evitas que alguien cree `Port(0)` directamente?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Port(u16);

#[derive(Debug)]
pub struct InvalidPort;

impl Port {
    pub fn new(value: u16) -> Result<Self, InvalidPort> {
        if value == 0 {
            Err(InvalidPort)
        } else {
            Ok(Port(value))
        }
    }

    pub fn get(&self) -> u16 { self.0 }

    /// Port conocidos
    pub fn http() -> Self { Port(80) }
    pub fn https() -> Self { Port(443) }
    pub fn ssh() -> Self { Port(22) }
}

impl std::fmt::Display for Port {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl TryFrom<u16> for Port {
    type Error = InvalidPort;
    fn try_from(value: u16) -> Result<Self, Self::Error> {
        Port::new(value)
    }
}
```

El campo `0` (primer campo del tuple struct) es privado por
defecto cuando `Port` esta en un modulo diferente al caller.
Si esta en el mismo modulo, agregar `pub(crate)` o reorganizar
modulos. La clave: el unico camino para crear un `Port` es via
`new()` (que valida) o los constructores constantes como `http()`.

```rust
// En otro modulo:
// Port(0)  → ERROR: field `0` of `Port` is private
// Port::new(0) → Err(InvalidPort)
// Port::new(8080) → Ok(Port(8080))
```

</details>

---

### Ejercicio 3 — Deref correcto

Implementa `NonEmptyString` con Deref a `str`.

**Prediccion**: ¿es correcto implementar Deref aqui? ¿Que
metodos de `str` podrian ser problematicos?

<details>
<summary>Respuesta</summary>

```rust
use std::ops::Deref;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct NonEmptyString(String);

impl NonEmptyString {
    pub fn new(s: &str) -> Option<Self> {
        if s.is_empty() { None } else { Some(Self(s.to_string())) }
    }

    pub fn into_inner(self) -> String { self.0 }
}

impl Deref for NonEmptyString {
    type Target = str;
    fn deref(&self) -> &str { &self.0 }
}

impl std::fmt::Display for NonEmptyString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

fn greet(name: &str) {
    println!("Hello, {}!", name);
}

fn main() {
    let name = NonEmptyString::new("Alice").unwrap();
    greet(&name);  // Deref coercion: &NonEmptyString → &str
    println!("Length: {}", name.len());  // str::len() via Deref
}
```

Deref a `str` es **correcto** aqui porque:
- `str` es inmutable → no puedes vaciarlo via Deref
- Todos los metodos de `str` son read-only
- `NonEmptyString` "es-un" `str` con invariante extra

**No implementar DerefMut** porque `&mut str` permite
`make_ascii_lowercase()` etc., pero no permite truncar a vacio.
Sin embargo, si implementaramos `Deref<Target = String>`, el
caller podria hacer `name.clear()` → rompe el invariante.

Metodos de `str` potencialmente confusos:
- `name.is_empty()` → siempre false (por invariante)
- No es problematico, solo redundante

</details>

---

### Ejercicio 4 — Deref incorrecto

Explica por que este Deref es incorrecto:

```rust
struct Percentage(f64);  // 0.0 a 100.0

impl Deref for Percentage {
    type Target = f64;
    fn deref(&self) -> &f64 { &self.0 }
}
```

**Prediccion**: da un ejemplo de codigo que compilaria gracias
a este Deref pero seria logicamente incorrecto.

<details>
<summary>Respuesta</summary>

```rust
let p1 = Percentage(50.0);
let p2 = Percentage(30.0);

// Deref coercion convierte &Percentage → &f64
// Luego * desreferencia: *(&f64) → f64
let sum: f64 = *p1 + *p2;  // 80.0 — ¿es Percentage o f64?

// Peor: mezclar con cosas sin sentido
let meters = 100.0_f64;
let result = *p1 + meters;  // 150.0 — ¿porcentaje + metros?

// Aun peor: funciones que esperan &f64
fn average(values: &[f64]) -> f64 {
    values.iter().sum::<f64>() / values.len() as f64
}
// average(&[*p1, meters, 42.0]) ← compila, sin sentido
```

El Deref permite escapar del newtype silenciosamente con `*`.
El proposito del newtype era evitar exactamente esto: que
un porcentaje se mezcle con otros f64.

**Correcto**: metodos explicitos sin Deref:

```rust
impl Percentage {
    pub fn value(&self) -> f64 { self.0 }

    pub fn of(&self, total: f64) -> f64 {
        total * self.0 / 100.0
    }

    pub fn add(self, other: Self) -> Option<Self> {
        let sum = self.0 + other.0;
        if sum <= 100.0 { Some(Percentage(sum)) } else { None }
    }
}

let p = Percentage(20.0);
let amount = p.of(500.0);  // 100.0 — claro y tipado
```

</details>

---

### Ejercicio 5 — Newtype con operaciones tipadas

Implementa `Meters` y `Seconds` con operaciones que producen
tipos correctos:
- `Meters / Seconds = MetersPerSecond`
- `Meters + Meters = Meters`
- `Meters + Seconds` = error de compilacion

**Prediccion**: ¿cuantos `impl` necesitas?

<details>
<summary>Respuesta</summary>

```rust
use std::ops::{Add, Sub, Div, Mul};

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
struct Meters(f64);

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
struct Seconds(f64);

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
struct MetersPerSecond(f64);

// Meters + Meters = Meters
impl Add for Meters {
    type Output = Self;
    fn add(self, rhs: Self) -> Self { Meters(self.0 + rhs.0) }
}

// Meters - Meters = Meters
impl Sub for Meters {
    type Output = Self;
    fn sub(self, rhs: Self) -> Self { Meters(self.0 - rhs.0) }
}

// Seconds + Seconds = Seconds
impl Add for Seconds {
    type Output = Self;
    fn add(self, rhs: Self) -> Self { Seconds(self.0 + rhs.0) }
}

// Meters / Seconds = MetersPerSecond
impl Div<Seconds> for Meters {
    type Output = MetersPerSecond;
    fn div(self, rhs: Seconds) -> MetersPerSecond {
        MetersPerSecond(self.0 / rhs.0)
    }
}

// MetersPerSecond * Seconds = Meters
impl Mul<Seconds> for MetersPerSecond {
    type Output = Meters;
    fn mul(self, rhs: Seconds) -> Meters {
        Meters(self.0 * rhs.0)
    }
}

// Meters * f64 = Meters (escalar)
impl Mul<f64> for Meters {
    type Output = Self;
    fn mul(self, rhs: f64) -> Self { Meters(self.0 * rhs) }
}

fn main() {
    let distance = Meters(100.0);
    let time = Seconds(9.58);
    let speed = distance / time;
    println!("{:?}", speed);  // MetersPerSecond(10.438...)

    let predicted = speed * Seconds(10.0);
    println!("{:?}", predicted);  // Meters(104.38...)

    let total = Meters(100.0) + Meters(50.0);
    println!("{:?}", total);  // Meters(150.0)

    // ERROR DE COMPILACION:
    // let wrong = Meters(100.0) + Seconds(9.58);
    // → no implementation for `Meters + Seconds`
}
```

7 impls: Add<Meters>, Sub<Meters>, Add<Seconds>,
Div<Seconds> for Meters, Mul<Seconds> for MetersPerSecond,
Mul<f64> for Meters. Cada uno define que operaciones
tienen sentido fisicamente. El compilador rechaza las que no.

</details>

---

### Ejercicio 6 — Password newtype

Implementa `Password(String)` que:
- No muestra el contenido en Debug ni Display
- Permite verificar longitud minima
- Tiene metodo `verify(&self, input: &str) -> bool`

**Prediccion**: ¿deberias implementar Deref<Target = str>?

<details>
<summary>Respuesta</summary>

```rust
use std::fmt;

pub struct Password(String);

impl Password {
    pub fn new(input: &str) -> Result<Self, &'static str> {
        if input.len() < 8 {
            Err("password must be at least 8 characters")
        } else {
            Ok(Password(input.to_string()))
        }
    }

    pub fn verify(&self, input: &str) -> bool {
        self.0 == input
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }
}

// Debug: NO mostrar contenido
impl fmt::Debug for Password {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Password([REDACTED])")
    }
}

// Display: NO mostrar contenido
impl fmt::Display for Password {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "********")
    }
}

// Clone: permitir, pero no exponemos el contenido
impl Clone for Password {
    fn clone(&self) -> Self {
        Password(self.0.clone())
    }
}

// PartialEq: comparar sin exponer
impl PartialEq for Password {
    fn eq(&self, other: &Self) -> bool {
        self.0 == other.0
    }
}

fn main() {
    let pw = Password::new("supersecret123").unwrap();
    println!("{:?}", pw);      // Password([REDACTED])
    println!("{}", pw);        // ********
    println!("Correct: {}", pw.verify("supersecret123"));  // true
    println!("Wrong: {}", pw.verify("wrong"));             // false
}
```

**NO implementar Deref<Target = str>** porque:
- `&str` expone el contenido de la password
- `pw.as_bytes()` via Deref expone los bytes
- Cualquier funcion que acepte `&str` veria la password
- Incluso `format!("{}", *pw)` mostraria el texto real

El newtype PROTEGE el contenido. Deref destruiria esa proteccion.
Solo `verify()` compara contra el input, sin exponer el valor.

</details>

---

### Ejercicio 7 — Newtype generico

Implementa `Validated<T, V>` donde V es un validador:

```rust
trait Validator<T> {
    fn validate(value: &T) -> Result<(), String>;
}
```

**Prediccion**: ¿como evitas que alguien cree `Validated`
sin pasar por la validacion?

<details>
<summary>Respuesta</summary>

```rust
use std::marker::PhantomData;

pub trait Validator<T> {
    fn validate(value: &T) -> Result<(), String>;
}

pub struct Validated<T, V: Validator<T>> {
    value: T,          // Privado
    _marker: PhantomData<V>,
}

impl<T, V: Validator<T>> Validated<T, V> {
    pub fn new(value: T) -> Result<Self, String> {
        V::validate(&value)?;
        Ok(Self { value, _marker: PhantomData })
    }

    pub fn get(&self) -> &T { &self.value }
    pub fn into_inner(self) -> T { self.value }
}

// Validador: string no vacia
struct NonEmptyValidator;
impl Validator<String> for NonEmptyValidator {
    fn validate(value: &String) -> Result<(), String> {
        if value.is_empty() { Err("must not be empty".into()) }
        else { Ok(()) }
    }
}

// Validador: numero positivo
struct PositiveValidator;
impl Validator<f64> for PositiveValidator {
    fn validate(value: &f64) -> Result<(), String> {
        if *value > 0.0 { Ok(()) }
        else { Err("must be positive".into()) }
    }
}

// Type aliases para ergonomia
type NonEmptyString = Validated<String, NonEmptyValidator>;
type PositiveFloat = Validated<f64, PositiveValidator>;

fn main() {
    let name = NonEmptyString::new("Alice".into()).unwrap();
    println!("Name: {}", name.get());

    let bad = NonEmptyString::new("".into());
    assert!(bad.is_err());

    let price = PositiveFloat::new(9.99).unwrap();
    println!("Price: {}", price.get());

    let bad_price = PositiveFloat::new(-1.0);
    assert!(bad_price.is_err());
}
```

El campo `value` es privado → `Validated` solo se puede crear
via `new()` que llama `V::validate()`. PhantomData<V> asegura
que el tipo del validador es parte del tipo Validated, sin
ocupar memoria extra.

</details>

---

### Ejercicio 8 — Newtype para unidades compuestas

Extiende el ejercicio 5: agrega `Acceleration` (m/s²) y verifica
que `MetersPerSecond / Seconds = Acceleration`.

**Prediccion**: ¿`Acceleration * Seconds` deberia retornar que tipo?

<details>
<summary>Respuesta</summary>

```rust
use std::ops::{Div, Mul};

#[derive(Debug, Clone, Copy)]
struct Meters(f64);

#[derive(Debug, Clone, Copy)]
struct Seconds(f64);

#[derive(Debug, Clone, Copy)]
struct MetersPerSecond(f64);

#[derive(Debug, Clone, Copy)]
struct Acceleration(f64);  // m/s²

// velocity / time = acceleration
impl Div<Seconds> for MetersPerSecond {
    type Output = Acceleration;
    fn div(self, rhs: Seconds) -> Acceleration {
        Acceleration(self.0 / rhs.0)
    }
}

// acceleration * time = velocity
impl Mul<Seconds> for Acceleration {
    type Output = MetersPerSecond;
    fn mul(self, rhs: Seconds) -> MetersPerSecond {
        MetersPerSecond(self.0 * rhs.0)
    }
}

// velocity * time = distance
impl Mul<Seconds> for MetersPerSecond {
    type Output = Meters;
    fn mul(self, rhs: Seconds) -> Meters {
        Meters(self.0 * rhs.0)
    }
}

// distance / time = velocity
impl Div<Seconds> for Meters {
    type Output = MetersPerSecond;
    fn div(self, rhs: Seconds) -> MetersPerSecond {
        MetersPerSecond(self.0 / rhs.0)
    }
}

fn main() {
    let v0 = MetersPerSecond(0.0);          // Velocidad inicial
    let a = Acceleration(9.81);              // Gravedad
    let t = Seconds(3.0);                    // Tiempo

    // v = v0 + a*t
    let v = MetersPerSecond(v0.0 + (a * t).0);
    println!("v = {:?}", v);                 // 29.43 m/s

    // d = v * t
    let d = v * t;
    println!("d = {:?}", d);                // 88.29 m

    // a = v / t
    let a_check = v / t;
    println!("a = {:?}", a_check);          // 9.81 m/s²

    // ERROR: Meters + Seconds → no compila
    // ERROR: Acceleration + MetersPerSecond → no compila
}
```

`Acceleration * Seconds = MetersPerSecond` (velocidad). Esto
modela correctamente la fisica: aceleracion por tiempo da
velocidad. El sistema de tipos verifica dimensional analysis
en compile time.

</details>

---

### Ejercicio 9 — Newtype con serde

Implementa `UserId(u64)` que se serializa/deserializa como
un numero plano (no como objeto `{"0": 42}`).

**Prediccion**: ¿`#[derive(Serialize, Deserialize)]` produce
el formato correcto por defecto?

<details>
<summary>Respuesta</summary>

```rust
use serde::{Serialize, Deserialize};

// derive en tuple struct de un campo: serde usa el formato
// del campo interno, NO como objeto
#[derive(Debug, Serialize, Deserialize, PartialEq)]
struct UserId(u64);

#[derive(Debug, Serialize, Deserialize)]
struct Order {
    id: u64,
    user: UserId,
    total: f64,
}

fn main() {
    let order = Order {
        id: 1001,
        user: UserId(42),
        total: 99.99,
    };

    let json = serde_json::to_string(&order).unwrap();
    println!("{}", json);
    // {"id":1001,"user":42,"total":99.99}
    //                  ↑ numero plano, NO {"0":42}

    let back: Order = serde_json::from_str(&json).unwrap();
    assert_eq!(back.user, UserId(42));
}
```

Si, `#[derive(Serialize, Deserialize)]` en un tuple struct de
un campo serializa/deserializa como el tipo interno (transparente).
Esto es el comportamiento por defecto de serde para newtypes.

Si quisieras cambiar el formato:
```rust
// Como string: "42" en vez de 42
#[derive(Serialize, Deserialize)]
#[serde(transparent)]  // Explicito (ya es el default para 1 campo)
struct UserId(u64);

// Como objeto: {"value": 42}
#[derive(Serialize, Deserialize)]
struct UserId { value: u64 }  // Named field en vez de tuple
```

`#[serde(transparent)]` es explicito pero redundante para
tuple structs de un campo — serde ya lo hace por defecto.

</details>

---

### Ejercicio 10 — Decision: ¿newtype o no?

Para cada caso, decide si un newtype vale la pena:

1. `age: u8` en un struct `Person`
2. `user_id` y `order_id` como parametros de la misma funcion
3. `error_message: String` en un log
4. `latitude: f64` y `longitude: f64` en la misma funcion
5. `password: String` en un struct `Credentials`

**Prediccion**: ¿cuales SI y cuales NO? Justifica cada uno.

<details>
<summary>Respuesta</summary>

**1. `age: u8` → NO newtype**
- Solo hay un campo de "edad" en Person
- No hay otro u8 con el que confundirlo en la misma funcion
- u8 ya restringe a 0-255 (razonable para edad)

**2. `user_id` y `order_id` en misma funcion → SI newtype**
```rust
// Sin newtype: ambos son u64, facil confundir
fn find_order(user_id: u64, order_id: u64) -> ...
find_order(order_id, user_id)  // Compila, bug

// Con newtype: el compilador detecta el error
fn find_order(user: UserId, order: OrderId) -> ...
```
Regla: si dos parametros del mismo tipo primitivo tienen
semanticas diferentes → newtype.

**3. `error_message: String` → NO newtype**
- Un solo string de error, no hay confusion posible
- No necesita validacion ni invariantes
- Seria boilerplate sin beneficio

**4. `latitude: f64` y `longitude: f64` → SI newtype**
```rust
// Sin newtype:
fn distance(lat1: f64, lng1: f64, lat2: f64, lng2: f64) -> f64
// ¿Cual es lat y cual es lng? Facil confundir

// Con newtype:
struct Latitude(f64);
struct Longitude(f64);
fn distance(a: (Latitude, Longitude), b: (Latitude, Longitude)) -> f64
```
Ademas, Latitude tiene rango [-90, 90] y Longitude [-180, 180]
→ validacion en constructor.

**5. `password: String` → SI newtype**
- Necesita proteccion: no debe mostrarse en Debug/Display/logs
- Necesita validacion: longitud minima, complejidad
- Semantica especial: verify(), no acceso directo al contenido
- Este es un caso clasico de newtype (ejercicio 6)

Resumen: newtype cuando hay riesgo de confusion entre valores
del mismo tipo, necesidad de validacion/invariantes, o
semantica especial (Password, Email, etc.). No cuando un tipo
primitivo ya es suficientemente descriptivo en contexto.

</details>
