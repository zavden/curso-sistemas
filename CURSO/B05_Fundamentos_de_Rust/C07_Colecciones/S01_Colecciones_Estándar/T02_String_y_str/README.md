# T02 — String y &str

## Dos tipos de string

Rust tiene dos tipos principales de string, y entender la
diferencia es fundamental:

```text
String  → buffer de bytes UTF-8 en el heap, con ownership
          Es como Vec<u8> pero garantiza que el contenido es UTF-8 valido.

&str    → referencia (slice) a bytes UTF-8, sin ownership
          Es un fat pointer: puntero + longitud.
          Puede apuntar al heap, al stack, o a datos estaticos.
```

```rust
// String — dueña de sus datos, puede crecer y mutar
let mut s = String::from("hello");
s.push_str(" world");
// s es dueña del buffer "hello world" en el heap

// &str — prestamo, solo lectura
let greeting: &str = "hello";  // apunta a datos estaticos (binario)
let slice: &str = &s[0..5];    // apunta al heap (dentro de s)
```

```text
Layout en memoria:

String:
  Stack                    Heap
 ┌──────────┐            ┌───┬───┬───┬───┬───┬───┬───┐
 │ ptr ──────────────────▶│ h │ e │ l │ l │ o │   │   │
 │ len: 5   │            └───┴───┴───┴───┴───┴───┴───┘
 │ cap: 7   │
 └──────────┘

&str:
 ┌──────────┐
 │ ptr ──────────────────▶ apunta a bytes UTF-8 (en cualquier lugar)
 │ len: 5   │
 └──────────┘
 (sin capacidad — no puede crecer)
```

```rust
use std::mem::size_of;

assert_eq!(size_of::<String>(), 24);  // ptr + len + cap
assert_eq!(size_of::<&str>(), 16);    // ptr + len (fat pointer)
```

## String es un Vec\<u8\> con garantia UTF-8

```rust
// Internamente, String = Vec<u8> con invariante UTF-8:
let s = String::from("hello");

// Acceder a los bytes subyacentes:
let bytes: &[u8] = s.as_bytes();
assert_eq!(bytes, &[104, 101, 108, 108, 111]);

// Convertir Vec<u8> a String (verificando UTF-8):
let bytes = vec![104, 101, 108, 108, 111];
let s = String::from_utf8(bytes).unwrap();  // "hello"

// Bytes invalidos → error:
let bad = vec![0xFF, 0xFE];
assert!(String::from_utf8(bad).is_err());

// Convertir sin verificar (unsafe):
let bytes = vec![104, 101, 108, 108, 111];
let s = unsafe { String::from_utf8_unchecked(bytes) };
// Si los bytes no son UTF-8 valido → comportamiento indefinido
```

```text
Consecuencia: todas las operaciones sobre String mantienen la
invariante UTF-8. No puedes meter bytes arbitrarios.
Esto es diferente de C (char* puede contener cualquier cosa)
y de Python 2 (str mezclaba bytes y texto).
```

## UTF-8: por que importa

UTF-8 es una codificacion de longitud variable: cada caracter
Unicode ocupa entre 1 y 4 bytes:

```text
Rango Unicode        Bytes   Ejemplo        Bytes en UTF-8
───────────────────────────────────────────────────────────
U+0000 — U+007F     1       'A' (U+0041)   [0x41]
U+0080 — U+07FF     2       'ñ' (U+00F1)   [0xC3, 0xB1]
U+0800 — U+FFFF     3       '中' (U+4E2D)   [0xE4, 0xB8, 0xAD]
U+10000 — U+10FFFF  4       '🦀' (U+1F980)  [0xF0, 0x9F, 0xA6, 0x80]
```

```rust
let s = "Añ中🦀";
println!("chars: {}", s.chars().count());  // 4 caracteres
println!("bytes: {}", s.len());            // 10 bytes
//  'A' = 1 byte, 'ñ' = 2, '中' = 3, '🦀' = 4  → total 10

// Visualizar los bytes:
for b in s.bytes() {
    print!("{b:02X} ");
}
// 41 C3 B1 E4 B8 AD F0 9F A6 80
```

```rust
// Incluso caracteres "simples" pueden sorprender:
let cafe = "café";
println!("len: {}", cafe.len());           // 5 bytes (no 4)
println!("chars: {}", cafe.chars().count()); // 4 caracteres
// 'é' ocupa 2 bytes en UTF-8

// Peor aun — algunos "caracteres" son multiples code points:
let flag = "🇪🇸";  // bandera de España
println!("len: {}", flag.len());            // 8 bytes
println!("chars: {}", flag.chars().count()); // 2 chars (U+1F1EA + U+1F1F8)
// Un "caracter visual" (grapheme cluster) puede ser multiples code points
```

## Por que no se puede indexar por posicion

```rust
let s = String::from("hello");

// ESTO NO COMPILA:
// let c = s[0];
// error[E0277]: the type `str` cannot be indexed by `{integer}`

// ¿Por que? Porque en UTF-8, el byte N no es necesariamente
// el caracter N:

let s = "café";
// Bytes: [99, 97, 102, 195, 169]
//         c    a    f    ←é→
//         [0]  [1]  [2]  [3][4]
//
// s[3] seria 195 — la mitad del caracter 'é'
// Eso no es un caracter valido. Rust se niega a hacerlo.
```

```text
El problema fundamental:
- s[i] en O(1) solo tiene sentido si cada elemento tiene el mismo tamaño
- En UTF-8, los caracteres tienen tamaño variable (1-4 bytes)
- Para llegar al caracter N, hay que recorrer los N-1 anteriores → O(n)
- Rust prefiere ser explicito: si quieres O(n), escribelo tu

Tres "niveles" de un string:
  bytes()         → secuencia de u8 (lo que hay en memoria)
  chars()         → secuencia de char (Unicode scalar values)
  grapheme clusters → lo que un humano ve como "un caracter"
                      (requiere crate unicode-segmentation)
```

```rust
// Lo que SI puedes hacer:

let s = "hello";

// 1. Slice por rango de bytes (panic si cae en medio de un caracter):
let h = &s[0..1];   // "h" — OK, 'h' es 1 byte
let he = &s[0..2];  // "he"

let cafe = "café";
let ca = &cafe[0..2];  // "ca" — OK
// let bad = &cafe[0..4]; // panic: byte index 4 is not a char boundary
// El byte 4 cae en medio de 'é' (bytes 3-4)

// 2. chars() para iterar por caracteres:
for c in "café".chars() {
    print!("{c} ");  // c a f é
}

// 3. char_indices() para saber la posicion en bytes de cada char:
for (i, c) in "café".char_indices() {
    println!("byte {i}: '{c}'");
}
// byte 0: 'c'
// byte 1: 'a'
// byte 2: 'f'
// byte 3: 'é'   ← empieza en byte 3, ocupa 2 bytes
```

## chars() vs bytes()

```rust
let s = "Hola 🌍";

// bytes() — los bytes crudos (u8)
let byte_count = s.bytes().count();  // 8
for b in s.bytes() {
    print!("{b:02X} ");
}
// 48 6F 6C 61 20 F0 9F 8C 8D
// H  o  l  a  _  ←──🌍───→

// chars() — Unicode scalar values (char, 4 bytes cada uno en memoria)
let char_count = s.chars().count();  // 6
for c in s.chars() {
    print!("'{}' ", c);
}
// 'H' 'o' 'l' 'a' ' ' '🌍'
```

```text
¿Cual usar?

bytes()  → procesamiento de bajo nivel, buscar bytes ASCII,
           protocolos binarios, rendimiento critico

chars()  → la mayoria de casos: contar caracteres, buscar,
           transformar texto

Ninguno  → si necesitas "lo que el usuario ve" (grapheme clusters),
           usa el crate unicode-segmentation
```

```rust
// Ejemplo: contar caracteres visibles vs bytes
fn stats(s: &str) {
    println!("texto: {s:?}");
    println!("  bytes: {}", s.len());           // s.len() son BYTES, no chars
    println!("  chars: {}", s.chars().count());
}

stats("hello");     // bytes: 5,  chars: 5
stats("café");      // bytes: 5,  chars: 4
stats("日本語");     // bytes: 9,  chars: 3
stats("🦀🦀🦀");   // bytes: 12, chars: 3
```

## Crear Strings

```rust
// Desde literal &str:
let s = String::from("hello");
let s = "hello".to_string();   // equivalente (via Display → ToString)
let s: String = "hello".into(); // via Into<String>

// String vacia:
let s = String::new();
let s = String::with_capacity(100);  // pre-alocar

// Desde formato:
let s = format!("nombre: {}, edad: {}", "Ana", 30);
// format! siempre retorna String, nunca falla

// Desde caracteres:
let s: String = ['h', 'e', 'l', 'l', 'o'].iter().collect();

// Desde bytes (verificando UTF-8):
let s = String::from_utf8(vec![104, 101, 108, 108, 111]).unwrap();

// Repetir:
let s = "ha".repeat(3);  // "hahaha"
```

## Concatenar strings

```rust
// 1. push_str — agrega un &str al final
let mut s = String::from("hello");
s.push_str(" world");  // "hello world"

// 2. push — agrega un solo char
s.push('!');  // "hello world!"

// 3. + operator — consume el lado izquierdo
let s1 = String::from("hello");
let s2 = String::from(" world");
let s3 = s1 + &s2;   // s1 fue consumido (movido), s2 prestado
// println!("{s1}");   // error: s1 fue movido
println!("{s2}");      // OK — s2 sigue disponible
println!("{s3}");      // "hello world"

// El + opera como: fn add(self, s: &str) -> String
// Toma ownership del izquierdo, presta el derecho.

// 4. format! — no consume nada, mas legible para multiples partes
let name = "Ana";
let age = 30;
let s = format!("{name} tiene {age} años");
// name y age siguen disponibles
```

```rust
// Concatenar multiples strings — evitar cascada de +
let a = String::from("uno");
let b = String::from("dos");
let c = String::from("tres");

// Tedioso con +:
let result = a + " " + &b + " " + &c;

// Mejor con format!:
let a = String::from("uno");
let result = format!("{a} {b} {c}");

// O con join:
let parts = vec!["uno", "dos", "tres"];
let result = parts.join(", ");  // "uno, dos, tres"
```

```text
Guia de concatenacion:
  push_str / push  → agregar a un String existente (mutable)
  +                 → combinar dos (consume el izquierdo)
  format!           → combinar varios con formato (no consume nada)
  join              → unir con separador
```

## Buscar en strings

```rust
let s = "hello beautiful world";

// contains — ¿contiene un substring?
assert!(s.contains("beautiful"));
assert!(s.contains('w'));

// starts_with / ends_with
assert!(s.starts_with("hello"));
assert!(s.ends_with("world"));

// find / rfind — posicion (en bytes) de la primera/ultima coincidencia
assert_eq!(s.find("beautiful"), Some(6));
assert_eq!(s.find("xyz"), None);
assert_eq!(s.rfind('l'), Some(20));  // ultima 'l'

// matches — iterador sobre todas las coincidencias
let count = s.matches('l').count();  // 4 letras 'l'

// match_indices — posicion + coincidencia
for (pos, matched) in s.match_indices('l') {
    println!("byte {pos}: '{matched}'");
}
```

## Transformar strings

```rust
let s = "  Hello, World!  ";

// trim — quitar whitespace
let trimmed = s.trim();          // "Hello, World!"
let left = s.trim_start();       // "Hello, World!  "
let right = s.trim_end();        // "  Hello, World!"

// to_uppercase / to_lowercase
let upper = "café".to_uppercase();     // "CAFÉ"
let lower = "HELLO".to_lowercase();    // "hello"

// replace
let new = "hello world".replace("world", "rust");  // "hello rust"
let new = "aabbaabb".replacen("aa", "XX", 1);      // "XXbbaabb" (solo 1)

// split
let parts: Vec<&str> = "a,b,c".split(',').collect();     // ["a", "b", "c"]
let parts: Vec<&str> = "a  b  c".split_whitespace().collect(); // ["a", "b", "c"]

// lines
let text = "linea 1\nlinea 2\nlinea 3";
for line in text.lines() {
    println!("{line}");
}
```

```rust
// Todas estas operaciones retornan NUEVOS strings (o &str):
let s = String::from("hello");
let upper = s.to_uppercase();  // upper es un nuevo String
// s sigue intacto

// Para modificar in-place, necesitas operar sobre los bytes
// (y mantener UTF-8 valido), lo cual es raro.
// La mayoria del tiempo: transforma y asigna al mismo binding.
let mut s = String::from("hello");
s = s.to_uppercase();  // reasigna
```

## &str en parametros de funciones

```rust
// BIEN — acepta String (via Deref), &str, slices
fn greet(name: &str) {
    println!("Hello, {name}!");
}

let owned = String::from("Ana");
let borrowed = "Bob";

greet(&owned);     // &String → &str (Deref coercion)
greet(borrowed);   // &str directamente
greet(&owned[0..3]); // slice

// MAL — demasiado restrictivo
fn greet_bad(name: &String) {
    println!("Hello, {name}!");
}
// Solo acepta &String, no &str ni slices
```

```text
Regla: en parametros, usa &str (no &String).
En retornos, depende:
  - String si la funcion crea el string
  - &str si retorna un slice del input
  - Cow<'_, str> si a veces crea y a veces no
```

## Strings y ownership — patrones comunes

```rust
// Patron 1: funcion que necesita guardar el string
struct User {
    name: String,  // dueño del string
}

impl User {
    // Acepta cualquier cosa convertible a String
    fn new(name: impl Into<String>) -> Self {
        User { name: name.into() }
    }
}

let u = User::new("Alice");          // &str → String (copia)
let u = User::new(String::from("Bob")); // String → String (move, sin copia)

// Patron 2: funcion que solo lee el string
fn is_valid(name: &str) -> bool {
    !name.is_empty() && name.len() < 100
}

// Patron 3: retornar String vs &str
fn to_greeting(name: &str) -> String {
    format!("Hello, {name}!")  // crea un nuevo String
}

fn first_word(s: &str) -> &str {
    s.split_whitespace().next().unwrap_or("")  // retorna parte del input
}
```

## Errores comunes

```rust
// ERROR 1: confundir len() con numero de caracteres
let s = "café";
assert_eq!(s.len(), 5);           // BYTES, no caracteres
assert_eq!(s.chars().count(), 4); // caracteres

// ERROR 2: slice en boundary invalido
let s = "café";
// let bad = &s[0..4]; // panic: byte 4 no es boundary de char
// Solucion: usar char_indices para encontrar boundaries validos
let end = s.char_indices().nth(3).map(|(i, _)| i).unwrap();
let first_three = &s[..end];  // "caf"

// ERROR 3: comparar con == funciona (pero es case-sensitive)
assert_eq!("hello", "hello");   // true
assert_ne!("Hello", "hello");   // diferentes
// Para case-insensitive: .to_lowercase() o .eq_ignore_ascii_case()
assert!("Hello".eq_ignore_ascii_case("hello"));

// ERROR 4: + con dos &str
// let s = "hello" + " world";  // error: + requiere String a la izquierda
let s = "hello".to_string() + " world";     // OK
let s = format!("{} {}", "hello", "world");  // mejor

// ERROR 5: iterar bytes pensando que son chars
for b in "café".bytes() {
    // b es u8, NO char
    // El byte 195 no es un caracter imprimible — es parte de 'é'
}
// Usar chars() si quieres caracteres
```

## Cheatsheet

```text
Crear:
  String::from("x")         desde &str
  "x".to_string()           equivalente
  format!("{} {}", a, b)    con formato
  String::new()              vacia
  String::with_capacity(n)   pre-alocar

Agregar:
  s.push('c')               un char
  s.push_str("text")        un &str
  s1 + &s2                  concatenar (consume s1)
  format!("{s1}{s2}")        concatenar (no consume)

Acceso:
  s.len()                    bytes (no chars)
  s.is_empty()               len == 0
  s.chars().count()          numero de caracteres
  s.chars().nth(n)           caracter n (O(n))
  &s[a..b]                   slice (bytes, panic si invalido)
  s.as_bytes()               &[u8]

Buscar:
  s.contains("x")            substring
  s.starts_with / ends_with  prefijo/sufijo
  s.find("x")                posicion (bytes)

Transformar:
  s.trim()                   sin whitespace
  s.to_uppercase()           mayusculas
  s.replace("a", "b")        reemplazar
  s.split(',')               dividir → iterador
  s.lines()                  por lineas

Iterar:
  s.bytes()                  → u8
  s.chars()                  → char
  s.char_indices()           → (byte_pos, char)
  s.split_whitespace()       → &str

Convertir:
  &String → &str             automatico (Deref)
  &str → String              .to_string() o String::from()
  String → &str              &s o s.as_str()
```

---

## Ejercicios

### Ejercicio 1 — Anatomia de un string

```rust
// Para cada string, predice ANTES de ejecutar:
//   1. s.len()           (bytes)
//   2. s.chars().count() (caracteres)
//   3. ¿Coinciden?
//
// a) "hello"
// b) "café"
// c) "日本語"
// d) "🦀 Rust"
// e) ""
// f) "a\nb\tc"
//
// Luego ejecuta y verifica.
```

### Ejercicio 2 — Procesamiento de texto

```rust
// Escribe una funcion que reciba un &str y retorne un String con:
//   1. Eliminar whitespace al inicio y final
//   2. Convertir a minusculas
//   3. Reemplazar espacios multiples por uno solo
//   4. Capitalizar la primera letra de cada palabra
//
// fn title_case(input: &str) -> String
//
// Ejemplos:
//   "  hello   WORLD  " → "Hello World"
//   "rust IS great"     → "Rust Is Great"
//
// Pista: split_whitespace + collect con join,
//        o iterar chars manualmente.
```

### Ejercicio 3 — Conteo de palabras

```rust
// Escribe fn word_count(text: &str) -> HashMap<String, usize>
// que cuente cuantas veces aparece cada palabra (case-insensitive).
//
// Reglas:
//   - Las palabras se separan por whitespace
//   - Ignorar puntuacion al inicio/final de cada palabra
//     (trim_matches con |c: char| !c.is_alphanumeric())
//   - Todo en minusculas para la comparacion
//
// Ejemplo:
//   "Hello, hello! World."
//   → {"hello": 2, "world": 1}
//
// Predice: ¿que tipo necesita la clave del HashMap? ¿&str o String? ¿Por que?
```
