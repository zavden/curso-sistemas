# T02 — Deref y DerefMut

## Que es Deref

El trait `Deref` permite que un tipo se comporte como una referencia.
Cuando implementas `Deref` para tu tipo, el compilador puede
convertirlo automaticamente a una referencia del tipo interno
— esto se llama **deref coercion**:

```rust
use std::ops::Deref;

// El trait:
// pub trait Deref {
//     type Target: ?Sized;
//     fn deref(&self) -> &Self::Target;
// }

// Box<T> implementa Deref<Target = T>:
let b = Box::new(5);
let val: &i32 = &b;     // deref coercion: &Box<i32> → &i32
assert_eq!(*b, 5);      // operador * llama a Deref::deref
```

```text
¿Que hace Deref?

  Deref convierte &MiTipo en &Target.

  Cuando escribes *x (donde x: MiTipo), Rust lo traduce a:
    *(x.deref())

  Cuando pasas &x donde se espera &Target, Rust inserta
  la conversion automaticamente. Esto es deref coercion.
```

---

## Deref coercion

Deref coercion es la conversion automatica que el compilador
inserta cuando los tipos no coinciden pero existe una cadena
de Deref que los conecta:

```rust
let s = String::from("hello");

// String implementa Deref<Target = str>
// Entonces &String se convierte automaticamente a &str:

fn greet(name: &str) {
    println!("Hello, {name}!");
}

greet(&s);   // &String → &str via deref coercion
// No necesitas escribir greet(&s[..]) ni greet(s.as_str())
```

```rust
// Deref coercion funciona en estos contextos:

let b = Box::new(String::from("hello"));

// 1. Paso de argumentos a funciones:
fn print_len(s: &str) { println!("{}", s.len()); }
print_len(&b);  // &Box<String> → &String → &str

// 2. Llamada a metodos:
println!("{}", b.len());  // Box → String → str, llama str::len()

// 3. Comparaciones:
assert_eq!(*b, "hello");  // *Box<String> → String, compara con &str

// 4. let bindings con tipo explicito:
let r: &str = &b;  // &Box<String> → &String → &str
```

### La cadena de coercion

El compilador aplica Deref repetidamente hasta encontrar
el tipo esperado. Esto forma una **cadena**:

```rust
// Box<String> implementa:
//   Deref<Target = String>   →  &Box<String> → &String
//
// String implementa:
//   Deref<Target = str>      →  &String → &str
//
// Cadena completa:
//   &Box<String> → &String → &str

let b: Box<String> = Box::new(String::from("world"));

// Todas estas son validas gracias a la cadena:
let a: &Box<String> = &b;
let c: &String = &b;       // un paso de deref
let d: &str = &b;          // dos pasos de deref
let e: &[u8] = b.as_bytes(); // str::as_bytes() accesible via cadena
```

```text
Cadenas de deref comunes en la stdlib:

  &Box<T>      → &T
  &String      → &str
  &Vec<T>      → &[T]
  &Box<String> → &String → &str
  &Box<Vec<T>> → &Vec<T> → &[T]
  &Rc<T>       → &T
  &Arc<T>      → &T
  &MutexGuard<T> → &T
```

### Deref coercion y metodos

```rust
// Cuando llamas un metodo, Rust busca en esta secuencia:
// 1. Metodos de T
// 2. Metodos de Deref::Target de T
// 3. Metodos de Deref::Target del Target
// ... y asi sucesivamente

let b = Box::new(String::from("HELLO"));

// Box<String> no tiene metodo to_lowercase()
// String no tiene metodo to_lowercase()
// str SI tiene to_lowercase()
//
// Rust aplica: Box<String> → String → str → str::to_lowercase()
let lower = b.to_lowercase();
assert_eq!(lower, "hello");

// Metodos disponibles en b sin .deref() explicito:
b.len();           // str::len()
b.is_empty();      // str::is_empty()
b.contains("ELL"); // str::contains()
b.as_bytes();      // str::as_bytes()
b.push_str("!");   // NO — push_str requiere &mut String, b no es mut
```

---

## DerefMut

`DerefMut` es el equivalente mutable. Permite que `&mut MiTipo`
se convierta en `&mut Target`:

```rust
use std::ops::DerefMut;

// El trait:
// pub trait DerefMut: Deref {
//     fn deref_mut(&mut self) -> &mut Self::Target;
// }
// Nota: DerefMut requiere que Deref ya este implementado.

let mut b = Box::new(String::from("hello"));

// DerefMut: &mut Box<String> → &mut String
b.push_str(" world");  // &mut Box<String> → &mut String → String::push_str
assert_eq!(*b, "hello world");

// Tambien funciona con funciones que esperan &mut Target:
fn append_exclamation(s: &mut String) {
    s.push('!');
}
append_exclamation(&mut b);  // &mut Box<String> → &mut String
assert_eq!(*b, "hello world!");
```

### Reglas de coercion mutable

```text
El compilador aplica deref coercion en 3 casos:

  1. &T      → &U      cuando T: Deref<Target = U>
  2. &mut T  → &mut U  cuando T: DerefMut<Target = U>
  3. &mut T  → &U      cuando T: Deref<Target = U>

Nota la ASIMETRIA:
  - &mut T puede convertirse a &U (perdiendo mutabilidad) ✓
  - &T NO puede convertirse a &mut U (ganando mutabilidad) ✗

Esto respeta las reglas de borrowing:
  - De mutable a inmutable siempre es seguro
  - De inmutable a mutable violaria la exclusividad
```

```rust
// Ejemplo de regla 3 (&mut T → &U):
let mut v = vec![1, 2, 3];

fn print_slice(s: &[i32]) {
    println!("{s:?}");
}

// &mut Vec<i32> → &[i32] (pierde mutabilidad, dos coerciones):
// &mut Vec<i32> → &Vec<i32> → &[i32]
print_slice(&mut v);

// Despues de la llamada, v sigue siendo mutable:
v.push(4);
```

---

## Implementar Deref para tipos propios

El uso mas comun de Deref custom es el **newtype pattern** —
un struct que envuelve otro tipo para darle una identidad
diferente, pero quieres que se comporte como el tipo interno:

```rust
use std::ops::{Deref, DerefMut};

// Newtype: Email es un String con validacion
struct Email(String);

impl Email {
    fn new(s: &str) -> Result<Email, String> {
        if s.contains('@') {
            Ok(Email(s.to_string()))
        } else {
            Err(format!("'{s}' is not a valid email"))
        }
    }
}

impl Deref for Email {
    type Target = str;  // Email se comporta como &str

    fn deref(&self) -> &str {
        &self.0
    }
}

fn main() {
    let email = Email::new("alice@example.com").unwrap();

    // Todos los metodos de str estan disponibles:
    println!("length: {}", email.len());           // str::len()
    println!("domain: {}", email.contains("@"));   // str::contains()
    println!("upper: {}", email.to_uppercase());   // str::to_uppercase()

    // Pasar donde se espera &str:
    fn print_address(addr: &str) { println!("To: {addr}"); }
    print_address(&email);  // deref coercion: &Email → &str
}
```

```rust
// Otro ejemplo: wrapper que agrega funcionalidad

use std::ops::{Deref, DerefMut};

struct SortedVec<T: Ord> {
    inner: Vec<T>,
}

impl<T: Ord> SortedVec<T> {
    fn new() -> Self {
        SortedVec { inner: Vec::new() }
    }

    fn insert(&mut self, val: T) {
        let pos = self.inner.binary_search(&val).unwrap_or_else(|p| p);
        self.inner.insert(pos, val);
    }
}

// Deref para leer como un slice:
impl<T: Ord> Deref for SortedVec<T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        &self.inner
    }
}

// NO implementamos DerefMut — no queremos que modifiquen
// el Vec directamente (romperia el invariante de ordenacion).

fn main() {
    let mut sv = SortedVec::new();
    sv.insert(3);
    sv.insert(1);
    sv.insert(2);

    // Metodos de &[T] disponibles:
    assert_eq!(sv.len(), 3);
    assert_eq!(sv[0], 1);    // siempre ordenado
    assert_eq!(sv.first(), Some(&1));
    assert!(sv.contains(&2));

    // sv.push(0);  // error: no metodo push en &[T]
    // sv[0] = 99;  // error: no DerefMut implementado
    // Esto es intencional — solo insert mantiene el orden.
}
```

---

## Cuando NO implementar Deref

Deref coercion es poderosa pero puede causar confusion si se
abusa. La regla general es:

```text
Implementa Deref SOLO cuando tu tipo es conceptualmente un
"smart pointer" o un "wrapper transparente" sobre Target.

  ✓ Box<T>    →  T      (es un puntero a T)
  ✓ String    →  str    (es un wrapper sobre str)
  ✓ Vec<T>    →  [T]    (es un wrapper sobre [T])
  ✓ Email     →  str    (newtype sobre String, semanticamente un string)
  ✓ Rc<T>     →  T      (es un puntero a T)

  ✗ Database  →  Connection    (Database no ES una Connection)
  ✗ User      →  String        (User no ES un String — tiene un campo name)
  ✗ File      →  Vec<u8>       (File no ES bytes — tiene comportamiento propio)
```

```rust
// MAL: Deref para acceder a un campo
struct User {
    name: String,
    age: u32,
}

// NO hagas esto:
// impl Deref for User {
//     type Target = String;
//     fn deref(&self) -> &String { &self.name }
// }
// Problema: user.len() retornaria el largo del nombre, no del User.
// Es confuso — User no ES un String.

// BIEN: metodo explicito
impl User {
    fn name(&self) -> &str { &self.name }
}
```

```rust
// MAL: Deref para "herencia"
struct Animal { name: String }
struct Dog { animal: Animal, breed: String }

// NO hagas esto para simular herencia:
// impl Deref for Dog {
//     type Target = Animal;
//     fn deref(&self) -> &Animal { &self.animal }
// }
// Problema: los metodos de Animal se mezclan con los de Dog
// de forma implicita. Rust no tiene herencia — usa composicion explicita.

// BIEN: delegar explicitamente
impl Dog {
    fn name(&self) -> &str { &self.animal.name }
}
```

---

## El operador * (desreferencia)

El operador `*` tiene una relacion directa con Deref:

```rust
// Para tipos que implementan Deref:
//   *x  es equivalente a  *(Deref::deref(&x))

let b = Box::new(42);
let val = *b;  // desreferenciar Box<i32> → i32
assert_eq!(val, 42);

// Para tipos Copy, * copia el valor:
let b = Box::new(42);
let a = *b;   // copia 42
let c = *b;   // copia 42 otra vez — b sigue vivo

// Para tipos non-Copy, * mueve el valor fuera del contenedor:
let b = Box::new(String::from("hello"));
let s = *b;   // mueve el String fuera del Box
// b ya no es valido
```

```rust
// * con referencias normales (no smart pointers):
let x = 5;
let r = &x;
assert_eq!(*r, 5);  // desreferenciar &i32 → i32

// * con multiples niveles de referencia:
let x = 5;
let r = &x;
let rr = &r;      // &&i32
assert_eq!(**rr, 5);  // dos desreferencias

// Pero con deref coercion, rara vez necesitas ** explicito:
fn print_val(n: &i32) { println!("{n}"); }
let x = 5;
let rr = &&x;
print_val(rr);  // &&i32 → &i32 via deref coercion
```

---

## Deref coercion con genericos y trait bounds

```rust
// Deref coercion funciona con trait bounds:
fn count_words(text: &str) -> usize {
    text.split_whitespace().count()
}

// Todos estos compilan gracias a deref coercion:
let s1 = "hello world";                     // &str
let s2 = String::from("hello world");       // String
let s3 = Box::new(String::from("hello world")); // Box<String>

assert_eq!(count_words(s1), 2);    // &str directamente
assert_eq!(count_words(&s2), 2);   // &String → &str
assert_eq!(count_words(&s3), 2);   // &Box<String> → &String → &str
```

```rust
// Patron comun: aceptar impl AsRef<str> o &str
// ambos funcionan con String, &str, Box<String>, etc.

// Opcion 1: &str con deref coercion (simple, comun):
fn process(s: &str) { /* ... */ }

// Opcion 2: generico con AsRef (mas explicito, mas flexible):
fn process_generic(s: impl AsRef<str>) {
    let s: &str = s.as_ref();
    // ...
}

// ¿Cual preferir?
// &str con deref coercion es suficiente en la mayoria de casos.
// AsRef es util cuando NO quieres forzar al caller a tener &:
// process_generic(my_string)  vs  process(&my_string)
```

---

## Errores comunes

```rust
// ERROR 1: esperar que Deref permita conversion de ownership
struct Wrapper(String);

impl Deref for Wrapper {
    type Target = str;
    fn deref(&self) -> &str { &self.0 }
}

let w = Wrapper("hello".into());
// let s: String = w;  // error: Wrapper no es String
// Deref solo convierte &Wrapper → &str, no Wrapper → String.
// Para conversion de ownership, implementa From o Into.
```

```rust
// ERROR 2: implementar DerefMut cuando no deberias
// Si tu tipo tiene invariantes (ej: SortedVec siempre ordenado),
// NO implementes DerefMut — permitiria romper el invariante:

struct SortedVec<T: Ord> { inner: Vec<T> }

// Si implementas DerefMut:
// impl<T: Ord> DerefMut for SortedVec<T> {
//     fn deref_mut(&mut self) -> &mut [T] { &mut self.inner }
// }
// Ahora alguien puede hacer:
// sorted.swap(0, 2);  // rompe el orden!
```

```rust
// ERROR 3: confundir Deref con AsRef
// Deref: conversion IMPLICITA via coercion (el compilador la inserta)
// AsRef: conversion EXPLICITA via .as_ref() (tu la llamas)
//
// Deref es para "este tipo ES conceptualmente un Target"
// AsRef es para "este tipo puede verse COMO un &Target"
//
// Ejemplo: String implementa ambos:
let s = String::from("hello");
let a: &str = &s;          // Deref (implicito)
let b: &str = s.as_ref();  // AsRef (explicito)
```

```rust
// ERROR 4: Deref loop infinito
struct Bad(i32);

// impl Deref for Bad {
//     type Target = Bad;  // Target es el mismo tipo!
//     fn deref(&self) -> &Bad { self }
// }
// Esto crea un loop infinito si el compilador intenta resolver coercion.
// Target siempre debe ser un tipo DIFERENTE.
```

```rust
// ERROR 5: olvidar que deref coercion no funciona para el receiver de metodos en todos los casos
let v = vec![1, 2, 3];
let b = Box::new(v);

// Esto funciona (metodo de Vec via deref):
b.len();      // OK: Box → Vec → slice → len()
b.iter();     // OK: Box → Vec → slice → iter()

// Pero metodos que consumen self no se aplican automaticamente:
// b.into_iter();  // llama Box::into_iter, NO Vec::into_iter
// Para el explicitamente: (*b).into_iter() o Vec::into_iter(*b)
```

---

## Cheatsheet

```text
Traits:
  Deref     → convierte &T en &Target  (lectura)
  DerefMut  → convierte &mut T en &mut Target  (escritura)
  DerefMut requiere Deref

Operador *:
  *x  →  *(x.deref())  →  acceder al valor interno

Deref coercion (automatica):
  &T      → &U       cuando T: Deref<Target = U>
  &mut T  → &mut U   cuando T: DerefMut<Target = U>
  &mut T  → &U       cuando T: Deref<Target = U>  (pierde mut)
  &T      → &mut U   NUNCA (violaria exclusividad)

Cadenas comunes:
  &Box<T>      → &T
  &String      → &str
  &Vec<T>      → &[T]
  &Box<String> → &String → &str

Cuando implementar Deref:
  ✓ Smart pointers (Box, Rc, Arc)
  ✓ Newtype wrappers transparentes (Email → str)
  ✗ Structs con campos (User → String)
  ✗ Simular herencia (Dog → Animal)

Cuando implementar DerefMut:
  ✓ Si el tipo no tiene invariantes que proteger
  ✗ Si modificar Target romperia garantias del tipo

Deref vs AsRef:
  Deref  → conversion implicita, para smart pointers
  AsRef  → conversion explicita via .as_ref(), para APIs genericas
```

---

## Ejercicios

### Ejercicio 1 — Newtype con Deref

```rust
// Crea un newtype Username(String) que:
//
// a) Solo se pueda construir con Username::new() que valide
//    que el string tiene entre 3 y 20 caracteres y solo
//    contiene alfanumericos. Retorna Result<Username, String>.
//
// b) Implemente Deref<Target = str> para poder usar
//    todos los metodos de &str.
//
// c) NO implemente DerefMut — el username no debe ser
//    modificable despues de creado.
//
// d) Verifica:
//    let u = Username::new("alice42").unwrap();
//    assert_eq!(u.len(), 7);        // via Deref → str
//    assert!(u.contains("alice"));  // via Deref → str
//    fn greet(name: &str) { ... }
//    greet(&u);                     // deref coercion
```

### Ejercicio 2 — Cadena de deref

```rust
// Predice que tipo tiene cada variable y si compila.
// Verifica ejecutando:
//
// let a: Box<String> = Box::new(String::from("hello"));
//
// let b: &Box<String> = &a;     // ¿tipo? ¿compila?
// let c: &String = &a;          // ¿tipo? ¿compila?
// let d: &str = &a;             // ¿tipo? ¿compila?
// let e: &[u8] = a.as_bytes();  // ¿tipo? ¿compila?
//
// fn takes_str(s: &str) {}
// takes_str(&a);                // ¿compila?
//
// fn takes_string(s: &String) {}
// takes_string(&a);             // ¿compila?
//
// fn takes_box(s: &Box<String>) {}
// takes_box(&a);                // ¿compila?
//
// Pregunta extra: ¿cuantos pasos de deref coercion
// se necesitan para cada caso?
```

### Ejercicio 3 — DerefMut selectivo

```rust
// Implementa un tipo BoundedVec<T> que:
// - Internamente tiene un Vec<T> y un max_len: usize
// - Solo permite agregar elementos si len < max_len
//
// a) Implementa Deref<Target = [T]> para poder leer
//    como un slice (len, iter, contains, indexing).
//
// b) ¿Deberias implementar DerefMut? ¿Por que si o por que no?
//    (Piensa: ¿que metodos de &mut [T] podrian romper el limite?)
//
// c) Implementa un metodo push(&mut self, val: T) -> Result<(), T>
//    que retorne Err(val) si el vector esta lleno.
//
// d) Verifica:
//    let mut bv = BoundedVec::new(3);
//    assert!(bv.push(1).is_ok());
//    assert!(bv.push(2).is_ok());
//    assert!(bv.push(3).is_ok());
//    assert!(bv.push(4).is_err());  // lleno
//    assert_eq!(bv.len(), 3);       // via Deref
//    assert_eq!(&bv[..], &[1, 2, 3]); // via Deref
```
