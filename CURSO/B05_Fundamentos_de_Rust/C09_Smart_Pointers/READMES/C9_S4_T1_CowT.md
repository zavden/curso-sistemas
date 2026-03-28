# T01 — Cow\<T\>

## El problema: clonar solo cuando es necesario

A veces una funcion recibe datos prestados y los retorna sin
modificar la mayoria de las veces. Pero ocasionalmente necesita
modificarlos, lo que requiere una copia propia. Sin Cow, tienes
que elegir entre clonar siempre (ineficiente) o complicar la API:

```rust
// Opcion 1: siempre clonar (ineficiente si no se modifica)
fn normalize(input: &str) -> String {
    if input.contains('\t') {
        input.replace('\t', "    ")  // necesita clonar
    } else {
        input.to_string()  // clona innecesariamente
    }
}

// Opcion 2: retornar enum manual (complicado)
enum MaybeOwned<'a> {
    Borrowed(&'a str),
    Owned(String),
}
// Esto es exactamente lo que Cow ya hace.
```

---

## Que es Cow

`Cow<'a, B>` (Clone-on-Write) es un enum que puede contener
datos **prestados** o datos **propios**. Solo clona cuando
necesitas mutar:

```rust
use std::borrow::Cow;

// La definicion (simplificada):
// enum Cow<'a, B: ToOwned + ?Sized> {
//     Borrowed(&'a B),       // referencia prestada
//     Owned(<B as ToOwned>::Owned),  // valor propio
// }

// Para str: Cow<'a, str>
//   Borrowed(&'a str)   — un &str prestado
//   Owned(String)       — un String propio

// Para [T]: Cow<'a, [T]>
//   Borrowed(&'a [T])   — un slice prestado
//   Owned(Vec<T>)       — un Vec propio
```

```rust
use std::borrow::Cow;

// Cow::Borrowed — sin alocacion:
let borrowed: Cow<str> = Cow::Borrowed("hello");

// Cow::Owned — con alocacion:
let owned: Cow<str> = Cow::Owned(String::from("hello"));

// Ambos se usan como &str via Deref:
println!("len: {}", borrowed.len());  // 5
println!("len: {}", owned.len());     // 5
```

---

## El patron clone-on-write

La idea es: empieza con datos prestados. Si necesitas modificar,
clona en ese momento. Si no, nunca clona:

```rust
use std::borrow::Cow;

fn normalize_whitespace(input: &str) -> Cow<str> {
    if input.contains('\t') {
        // Necesita modificar → clona y transforma:
        Cow::Owned(input.replace('\t', "    "))
    } else {
        // No necesita modificar → retorna prestado (sin clonar):
        Cow::Borrowed(input)
    }
}

fn main() {
    let clean = "hello world";
    let dirty = "hello\tworld";

    let result1 = normalize_whitespace(clean);
    // result1 es Borrowed — zero allocation

    let result2 = normalize_whitespace(dirty);
    // result2 es Owned — se aloco un String nuevo

    // Ambos se pueden usar como &str:
    println!("{result1}");  // hello world
    println!("{result2}");  // hello    world

    // Verificar:
    match &result1 {
        Cow::Borrowed(_) => println!("no allocation needed"),
        Cow::Owned(_) => println!("had to allocate"),
    }
    // "no allocation needed"
}
```

```text
Ventaja de rendimiento:

  Sin Cow (siempre clona):
    normalize("hello")     →  aloca String "hello" (innecesario)
    normalize("he\tllo")   →  aloca String "he    llo"

  Con Cow:
    normalize("hello")     →  Cow::Borrowed("hello") (ZERO allocation)
    normalize("he\tllo")   →  Cow::Owned("he    llo") (aloca solo si necesario)

  Si el 90% de los inputs no necesitan modificacion,
  Cow evita el 90% de las alocaciones.
```

---

## Cow\<str\> en APIs

`Cow<'_, str>` es ideal para funciones que a veces retornan
un string literal y a veces un string computado:

```rust
use std::borrow::Cow;

fn greet(name: &str) -> Cow<str> {
    if name.is_empty() {
        Cow::Borrowed("Hello, stranger!")  // literal, sin alocacion
    } else {
        Cow::Owned(format!("Hello, {name}!"))  // computado
    }
}

fn main() {
    println!("{}", greet(""));       // Hello, stranger!  (Borrowed)
    println!("{}", greet("Alice"));  // Hello, Alice!     (Owned)
}
```

```rust
// Cow como parametro — aceptar &str O String:
use std::borrow::Cow;

fn log_message(msg: Cow<str>) {
    println!("[LOG] {msg}");
}

fn main() {
    // Pasar &str (sin alocacion):
    log_message(Cow::Borrowed("static message"));

    // Pasar String (ya alocado):
    let dynamic = format!("error at line {}", 42);
    log_message(Cow::Owned(dynamic));

    // Con Into<Cow<str>> es mas ergonomico:
    log_message("static".into());            // &str → Cow::Borrowed
    log_message(String::from("dynamic").into()); // String → Cow::Owned
}
```

### Cow en structs

```rust
use std::borrow::Cow;

// Un struct que puede contener datos prestados o propios:
#[derive(Debug, Clone)]
struct LogEntry<'a> {
    level: &'static str,
    message: Cow<'a, str>,
}

impl<'a> LogEntry<'a> {
    fn info(msg: &'a str) -> Self {
        LogEntry {
            level: "INFO",
            message: Cow::Borrowed(msg),
        }
    }

    fn error(msg: String) -> Self {
        LogEntry {
            level: "ERROR",
            message: Cow::Owned(msg),
        }
    }

    // Convertir a version con ownership (sin lifetime):
    fn into_owned(self) -> LogEntry<'static> {
        LogEntry {
            level: self.level,
            message: Cow::Owned(self.message.into_owned()),
        }
    }
}

fn main() {
    let info = LogEntry::info("server started");  // no aloca message
    let error = LogEntry::error(format!("port {} in use", 8080));  // aloca

    println!("{:?}", info);   // LogEntry { level: "INFO", message: "server started" }
    println!("{:?}", error);  // LogEntry { level: "ERROR", message: "port 8080 in use" }

    // into_owned permite guardar sin lifetime:
    let owned: LogEntry<'static> = info.into_owned();
    // owned puede vivir indefinidamente — tiene su propio String
}
```

---

## Cow\<[T]\> — slices

Cow funciona igual con slices:

```rust
use std::borrow::Cow;

fn remove_negatives(data: &[i32]) -> Cow<[i32]> {
    if data.iter().any(|&x| x < 0) {
        // Tiene negativos → filtrar (necesita alocar Vec):
        Cow::Owned(data.iter().copied().filter(|&x| x >= 0).collect())
    } else {
        // Sin negativos → retornar el slice original:
        Cow::Borrowed(data)
    }
}

fn main() {
    let clean = [1, 2, 3, 4, 5];
    let dirty = [1, -2, 3, -4, 5];

    let result1 = remove_negatives(&clean);
    // Cow::Borrowed(&[1, 2, 3, 4, 5])  — zero allocation

    let result2 = remove_negatives(&dirty);
    // Cow::Owned(vec![1, 3, 5])  — aloco un Vec

    println!("{:?}", &*result1);  // [1, 2, 3, 4, 5]
    println!("{:?}", &*result2);  // [1, 3, 5]
}
```

---

## to_mut — clonar solo cuando mutes

`to_mut()` retorna `&mut` al valor interno. Si el Cow es
Borrowed, clona en ese momento y se convierte en Owned:

```rust
use std::borrow::Cow;

let mut cow: Cow<str> = Cow::Borrowed("hello");

// to_mut() convierte Borrowed → Owned si es necesario:
let s: &mut String = cow.to_mut();
s.push_str(" world");

// cow ahora es Owned:
assert_eq!(cow, "hello world");
match cow {
    Cow::Owned(_) => println!("now owned"),
    Cow::Borrowed(_) => println!("still borrowed"),
}
// "now owned"
```

```rust
// Si ya es Owned, to_mut() no clona:
use std::borrow::Cow;

let mut cow: Cow<str> = Cow::Owned(String::from("hello"));
cow.to_mut().push_str(" world");  // no clona — ya es Owned
assert_eq!(cow, "hello world");
```

```rust
// Patron: construir resultado incrementalmente:
use std::borrow::Cow;

fn escape_html(input: &str) -> Cow<str> {
    // Buscar si hay algo que escapar:
    if !input.contains(['<', '>', '&', '"']) {
        return Cow::Borrowed(input);
    }

    // Solo clona si es necesario:
    let mut result = String::with_capacity(input.len());
    for ch in input.chars() {
        match ch {
            '<' => result.push_str("&lt;"),
            '>' => result.push_str("&gt;"),
            '&' => result.push_str("&amp;"),
            '"' => result.push_str("&quot;"),
            _ => result.push(ch),
        }
    }
    Cow::Owned(result)
}

fn main() {
    let safe = "hello world";
    let unsafe_html = "<script>alert('xss')</script>";

    println!("{}", escape_html(safe));
    // "hello world" (Borrowed, sin alocacion)

    println!("{}", escape_html(unsafe_html));
    // "&lt;script&gt;alert('xss')&lt;/script&gt;" (Owned)
}
```

---

## into_owned — forzar ownership

```rust
use std::borrow::Cow;

let cow: Cow<str> = Cow::Borrowed("hello");

// into_owned() consume el Cow y retorna el tipo Owned:
let s: String = cow.into_owned();
// Si era Borrowed → clona
// Si era Owned → mueve (sin clonar)

assert_eq!(s, "hello");
```

```rust
// Util para convertir Cow a String cuando necesitas ownership:
use std::borrow::Cow;

fn process(input: &str) -> Cow<str> {
    if input.is_empty() {
        Cow::Borrowed("default")
    } else {
        Cow::Borrowed(input)
    }
}

fn store_result(result: String) {
    println!("storing: {result}");
}

fn main() {
    let result = process("hello");
    // store_result espera String, no Cow:
    store_result(result.into_owned());
}
```

---

## Cow y el trait ToOwned

Cow funciona con cualquier tipo que implemente `ToOwned`.
Este trait define la relacion entre la version prestada y
la version propia:

```text
Prestado (B)     →  Propio (B::Owned)     →  Cow<'a, B>
────────────────────────────────────────────────────────
str              →  String                →  Cow<'a, str>
[T]              →  Vec<T>                →  Cow<'a, [T]>
CStr             →  CString              →  Cow<'a, CStr>
OsStr            →  OsString             →  Cow<'a, OsStr>
Path             →  PathBuf              →  Cow<'a, Path>

En cada caso:
  Cow::Borrowed(&B)     — sin alocacion
  Cow::Owned(B::Owned)  — con alocacion
```

```rust
use std::borrow::Cow;
use std::path::{Path, PathBuf};

// Cow<Path> — evitar clonar paths:
fn ensure_extension(path: &Path) -> Cow<Path> {
    if path.extension().is_some() {
        Cow::Borrowed(path)
    } else {
        let mut owned = path.to_path_buf();
        owned.set_extension("txt");
        Cow::Owned(owned)
    }
}

fn main() {
    let with_ext = Path::new("file.rs");
    let without_ext = Path::new("readme");

    println!("{:?}", ensure_extension(with_ext));    // "file.rs" (Borrowed)
    println!("{:?}", ensure_extension(without_ext)); // "readme.txt" (Owned)
}
```

---

## Cow vs alternativas

```text
¿Cuando usar Cow?

  ✓ Funcion que retorna datos sin modificar la mayoria de veces
  ✓ API que acepta &str O String (sin duplicar funciones)
  ✓ Structs que pueden contener datos prestados o propios
  ✓ Optimizar hot paths evitando alocaciones innecesarias

¿Cuando NO usar Cow?

  ✗ Siempre necesitas ownership → usa String / Vec directamente
  ✗ Nunca necesitas ownership → usa &str / &[T] directamente
  ✗ La complejidad no justifica la optimizacion
  ✗ El 99% de los inputs necesitan modificacion → clonar siempre es mas simple

Alternativas:

  fn foo(s: &str) → &str        si nunca necesitas alocar
  fn foo(s: &str) → String      si siempre necesitas alocar
  fn foo(s: &str) → Cow<str>    si a veces alocar, a veces no
  fn foo(s: impl Into<String>)  si el caller decide si clonar
```

---

## Errores comunes

```rust
// ERROR 1: Cow donde &str basta
use std::borrow::Cow;

// MAL — la funcion nunca retorna Owned:
fn get_name(user: &User) -> Cow<str> {
    Cow::Borrowed(&user.name)  // siempre Borrowed
}

// BIEN — retornar &str directamente:
fn get_name2(user: &User) -> &str {
    &user.name
}

struct User { name: String }
```

```rust
// ERROR 2: Cow donde String basta
use std::borrow::Cow;

// MAL — la funcion siempre aloca:
fn format_greeting(name: &str) -> Cow<str> {
    Cow::Owned(format!("Hello, {name}!"))  // siempre Owned
}

// BIEN — retornar String directamente:
fn format_greeting2(name: &str) -> String {
    format!("Hello, {name}!")
}
```

```rust
// ERROR 3: olvidar que Cow implementa Deref
use std::borrow::Cow;

let cow: Cow<str> = Cow::Borrowed("hello");

// MAL — match innecesario para acceder al contenido:
let len = match &cow {
    Cow::Borrowed(s) => s.len(),
    Cow::Owned(s) => s.len(),
};

// BIEN — Deref hace el trabajo:
let len = cow.len();  // Deref: Cow<str> → &str → len()
```

```rust
// ERROR 4: usar to_mut() cuando into_owned() es mas apropiado
use std::borrow::Cow;

let cow: Cow<str> = Cow::Borrowed("hello");

// Si solo necesitas el String y no vas a reusar el Cow:
// MAL:
// let s: &mut String = cow.to_mut();  // cow debe ser mut

// BIEN:
let s: String = cow.into_owned();  // consume el Cow
```

```rust
// ERROR 5: lifetime confuso en Cow dentro de structs
use std::borrow::Cow;

// Este struct necesita un lifetime:
struct Message<'a> {
    text: Cow<'a, str>,
}

// Si quieres guardarlo sin lifetime, necesitas 'static o into_owned:
fn create_message() -> Message<'static> {
    Message {
        text: Cow::Borrowed("static string"),  // OK: string literal es 'static
    }
}

// O con Owned (sin restriccion de lifetime):
fn create_dynamic_message(input: &str) -> Message<'static> {
    Message {
        text: Cow::Owned(input.to_string()),  // Owned no necesita lifetime
    }
}
```

---

## Cheatsheet

```text
Crear:
  Cow::Borrowed(&val)       referencia prestada (sin alocacion)
  Cow::Owned(val)            valor propio (con alocacion)
  Cow::from(&val)            auto-detecta Borrowed
  Cow::from(owned_val)       auto-detecta Owned
  "hello".into()             &str → Cow::Borrowed
  String::from("x").into()   String → Cow::Owned

Acceder:
  *cow / cow.method()        Deref a &B (&str, &[T], etc.)
  cow.as_ref()               → &B

Mutar (clone-on-write):
  cow.to_mut()               → &mut Owned (clona si Borrowed)

Convertir:
  cow.into_owned()           → Owned (clona si Borrowed, mueve si Owned)

Verificar:
  matches!(cow, Cow::Borrowed(_))   ¿es prestado?
  matches!(cow, Cow::Owned(_))      ¿es propio?

Tipos comunes:
  Cow<'a, str>      = Borrowed(&'a str) | Owned(String)
  Cow<'a, [T]>      = Borrowed(&'a [T]) | Owned(Vec<T>)
  Cow<'a, Path>     = Borrowed(&'a Path) | Owned(PathBuf)

Cuando usar:
  A veces retornar sin modificar, a veces modificar → Cow
  Siempre sin modificar → &T
  Siempre modificar → T::Owned (String, Vec, etc.)
```

---

## Ejercicios

### Ejercicio 1 — escape_html

```rust
use std::borrow::Cow;

// Implementa fn escape_html(input: &str) -> Cow<str> que:
//
// a) Si el input no contiene <, >, &, ni ", retorna
//    Cow::Borrowed (sin alocacion).
//
// b) Si contiene alguno, retorna Cow::Owned con los
//    caracteres reemplazados:
//    < → &lt;   > → &gt;   & → &amp;   " → &quot;
//
// c) Verifica:
//    let safe = "hello world";
//    let result = escape_html(safe);
//    assert!(matches!(result, Cow::Borrowed(_)));
//
//    let unsafe_input = "<b>bold</b>";
//    let result = escape_html(unsafe_input);
//    assert_eq!(&*result, "&lt;b&gt;bold&lt;/b&gt;");
//    assert!(matches!(result, Cow::Owned(_)));
```

### Ejercicio 2 — normalize_path

```rust
use std::borrow::Cow;

// Implementa fn normalize_path(path: &str) -> Cow<str> que:
//
// a) Si el path no empieza con '/', lo agrega (Cow::Owned).
// b) Si el path termina con '/', lo quita (Cow::Owned).
// c) Si ambos estan bien, retorna Cow::Borrowed.
//
// Ejemplos:
//   normalize_path("/api/users")  → Borrowed("/api/users")
//   normalize_path("api/users")   → Owned("/api/users")
//   normalize_path("/api/users/") → Owned("/api/users")
//   normalize_path("api/")        → Owned("/api")
//
// ¿Cuantas alocaciones hay en cada caso?
```

### Ejercicio 3 — Cow en struct

```rust
use std::borrow::Cow;

// Diseña un struct Error con:
//
// a) code: u32
//    message: Cow<'static, str>
//
//    Los errores conocidos usan Cow::Borrowed con strings estaticos.
//    Los errores dinamicos usan Cow::Owned con format!.
//
// b) Implementa constructores:
//    Error::not_found()         → message: "Not Found" (Borrowed)
//    Error::internal()          → message: "Internal Server Error" (Borrowed)
//    Error::custom(msg: String) → message: msg (Owned)
//    Error::with_detail(code, context: &str) → message con format! (Owned)
//
// c) ¿Por que el lifetime es 'static y no 'a?
//    ¿Que pasaria si usaras 'a?
//
// d) Implementa Display para Error.
//    Verifica que funciona con println!("{}", error).
```
