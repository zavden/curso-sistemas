# T01 — Funciones genericas

## El problema que resuelven los genericos

Sin genericos, tendrias que escribir funciones duplicadas para
cada tipo:

```rust
fn max_i32(a: i32, b: i32) -> i32 {
    if a > b { a } else { b }
}

fn max_f64(a: f64, b: f64) -> f64 {
    if a > b { a } else { b }
}

fn max_char(a: char, b: char) -> char {
    if a > b { a } else { b }
}

// La logica es identica — solo cambia el tipo.
// Con genericos, escribes UNA sola version:
```

```rust
fn max<T: PartialOrd>(a: T, b: T) -> T {
    if a > b { a } else { b }
}

// Funciona con cualquier tipo que implemente PartialOrd:
let m = max(3, 7);           // T = i32
let m = max(3.14, 2.71);     // T = f64
let m = max('z', 'a');        // T = char
let m = max("hello", "world"); // T = &str
```

## Sintaxis basica

```rust
//    parametro de tipo
//         ↓
fn foo<T>(value: T) -> T {
    value
}
// T es un placeholder que se reemplaza por un tipo concreto
// al momento de la llamada.

// Multiples parametros de tipo:
fn pair<A, B>(first: A, second: B) -> (A, B) {
    (first, second)
}

let p = pair(42, "hello");     // A = i32, B = &str
let p = pair(true, 3.14);     // A = bool, B = f64
```

```rust
// Convencion de nombres:
// T, U, V   → tipos genericos (el mas comun)
// K, V      → clave, valor (en colecciones)
// E         → error (en Result)
// S         → estado, source
// R         → retorno

// Usa nombres descriptivos cuando hay muchos parametros:
fn convert<Input, Output>(val: Input) -> Output {
    todo!()
}
```

## Inferencia — Rust adivina el tipo

```rust
fn identity<T>(x: T) -> T {
    x
}

// Rust infiere T del argumento:
let a = identity(42);         // T = i32 (inferido)
let b = identity("hello");    // T = &str (inferido)
let c = identity(vec![1, 2]); // T = Vec<i32> (inferido)

// A veces Rust no puede inferir y necesitas ayudarlo:
let v = Vec::new();   // error: cannot infer type
let v: Vec<i32> = Vec::new();   // anotacion en la variable
let v = Vec::<i32>::new();       // turbofish en la funcion
```

### Turbofish ::\<\>

```rust
// Turbofish especifica el tipo generico explicitamente:
let x = "42".parse::<i32>().unwrap();   // turbofish
let x: i32 = "42".parse().unwrap();     // anotacion equivalente

// Necesario cuando Rust no puede inferir:
let numbers: Vec<i32> = vec![1, 2, 3];
let sum = numbers.iter().sum::<i32>();   // turbofish necesario
// sin turbofish, Rust no sabe si sum debe ser i32, i64, f64...

// Turbofish en funciones:
fn make_pair<T: Default>() -> (T, T) {
    (T::default(), T::default())
}
let p = make_pair::<i32>();   // (0, 0)
let p = make_pair::<String>(); // ("", "")
```

## Trait bounds — restringir T

Un `T` sin bounds acepta **cualquier** tipo, pero no puedes
hacer nada con el (ni sumarlo, ni compararlo, ni imprimirlo):

```rust
// Sin bound — T puede ser cualquier cosa
fn first<T>(a: T, _b: T) -> T {
    a   // lo unico que podemos hacer es moverlo/retornarlo
}

// Si intentas operaciones, el compilador se queja:
fn add<T>(a: T, b: T) -> T {
    a + b  // error: T no implementa Add
}

// Solucion: agregar trait bounds
use std::ops::Add;

fn add<T: Add<Output = T>>(a: T, b: T) -> T {
    a + b  // OK — T implementa Add
}

let sum = add(3, 4);       // 7
let sum = add(1.5, 2.5);   // 4.0
```

```rust
// Multiples bounds con +
fn print_and_compare<T: std::fmt::Display + PartialOrd>(a: T, b: T) {
    if a > b {
        println!("{a} es mayor");
    } else {
        println!("{b} es mayor o igual");
    }
}

// Clausula where — mas legible con bounds complejos
fn process<T, U>(a: T, b: U) -> String
where
    T: std::fmt::Display + Clone,
    U: std::fmt::Debug + Default,
{
    format!("{a} {:?}", b)
}

// where es equivalente a bounds inline, pero mas facil de leer
// cuando hay multiples parametros con multiples bounds.
```

```text
Regla practica:
- 1 bound simple → inline: fn foo<T: Clone>(x: T)
- 2+ bounds o 2+ params → where: fn foo<T, U>(x: T) where T: ..., U: ...
```

## Monomorphization

Cuando Rust compila una funcion generica, genera una **copia
especializada** para cada tipo concreto que se usa. Este proceso
se llama monomorphization:

```rust
fn double<T: std::ops::Add<Output = T> + Copy>(x: T) -> T {
    x + x
}

let a = double(5_i32);    // compila como: fn double_i32(x: i32) -> i32
let b = double(3.14_f64); // compila como: fn double_f64(x: f64) -> f64
let c = double(2_u8);     // compila como: fn double_u8(x: u8) -> u8
```

```text
Lo que escribes:

    fn double<T: Add + Copy>(x: T) -> T { x + x }
    double(5_i32);
    double(3.14_f64);

Lo que el compilador genera (simplificado):

    fn double_i32(x: i32) -> i32 { x + x }
    fn double_f64(x: f64) -> f64 { x + x }
    double_i32(5);
    double_f64(3.14);

Cada version esta optimizada para su tipo concreto.
No hay indirecciones, no hay vtables, no hay overhead en runtime.
```

### Costo: tamaño del binario

```text
Ventajas de monomorphization:
+ Zero-cost abstraction — sin overhead en runtime
+ El compilador optimiza cada version por separado
+ Inlining posible (la funcion concreta es conocida)
+ Mismo rendimiento que si escribieras la funcion a mano

Desventajas:
- Tamaño del binario crece con cada tipo instanciado
- Tiempos de compilacion mas largos (mas codigo que generar)
- Posible "code bloat" si usas muchos tipos distintos
```

```rust
// Ejemplo de code bloat:
fn process<T: std::fmt::Debug>(items: &[T]) {
    for item in items {
        println!("{item:?}");
    }
}

// Si lo llamas con 10 tipos distintos, el compilador genera 10
// copias de process, cada una con su propio codigo de formateo.

process(&[1_i32, 2, 3]);
process(&[1_u64, 2, 3]);
process(&["a", "b"]);
process(&[true, false]);
// ... 10 copias del mismo cuerpo, con tipos distintos
```

```text
Para reducir code bloat:
1. Funciones internas no-genericas:
   fn process<T: Debug>(items: &[T]) {
       fn inner(formatted: &[String]) {  // no generica
           for s in formatted { println!("{s}"); }
       }
       let strings: Vec<String> = items.iter().map(|i| format!("{i:?}")).collect();
       inner(&strings);
   }
   // Solo la parte generica (collect) se duplica; inner se comparte.

2. dyn Trait (dynamic dispatch):
   fn process(items: &[&dyn Debug]) { ... }
   // Una sola copia, pero con overhead de vtable.

En la practica, el code bloat rara vez es problema para la
mayoria de aplicaciones. Solo optimizalo si lo mides.
```

## Genericos en la stdlib

```rust
// La stdlib usa genericos extensamente:

// Option<T> — un valor que puede no existir
let x: Option<i32> = Some(42);
let y: Option<String> = None;

// Result<T, E> — exito o error
let ok: Result<i32, String> = Ok(42);
let err: Result<i32, String> = Err("fallo".to_string());

// Vec<T> — array dinamico
let v: Vec<f64> = vec![1.0, 2.0, 3.0];

// HashMap<K, V> — tabla hash
use std::collections::HashMap;
let m: HashMap<String, Vec<i32>> = HashMap::new();

// Funciones genericas de la stdlib:
let v = vec![3, 1, 4, 1, 5];
let min = v.iter().min();   // min<T: Ord>() -> Option<&T>
let max = v.iter().max();   // max<T: Ord>() -> Option<&T>

let a = std::cmp::min(3, 7);  // fn min<T: Ord>(a: T, b: T) -> T
let b = std::mem::swap(&mut x, &mut y); // fn swap<T>(a: &mut T, b: &mut T)
```

## Funciones genericas con referencias

```rust
// Con referencias, el lifetime se infiere normalmente:
fn longest<T: PartialOrd>(a: &T, b: &T) -> &T {
    // error: missing lifetime specifier
    // Rust no sabe si el retorno vive tanto como a o como b
    if a > b { a } else { b }
}

// Solucion: lifetime explicito
fn longest<'a, T: PartialOrd>(a: &'a T, b: &'a T) -> &'a T {
    if a > b { a } else { b }
}

let result;
{
    let x = 5;
    let y = 10;
    result = longest(&x, &y);
    println!("{result}");  // 10
}
```

```rust
// Patron comun: T es el tipo, la funcion toma &T
fn find_first<T: PartialEq>(haystack: &[T], needle: &T) -> Option<usize> {
    for (i, item) in haystack.iter().enumerate() {
        if item == needle {
            return Some(i);
        }
    }
    None
}

let v = vec![10, 20, 30, 40];
assert_eq!(find_first(&v, &30), Some(2));
assert_eq!(find_first(&v, &99), None);

// Funciona con cualquier tipo que implemente PartialEq:
let words = vec!["hello", "world"];
assert_eq!(find_first(&words, &"world"), Some(1));
```

## impl Trait en parametros (azucar)

```rust
// En vez de:
fn print_item<T: std::fmt::Display>(item: T) {
    println!("{item}");
}

// Puedes escribir:
fn print_item(item: impl std::fmt::Display) {
    println!("{item}");
}
// Son equivalentes — ambos generan monomorphization.

// PERO hay una diferencia sutil:
// Con <T>, puedes forzar que ambos sean el mismo tipo:
fn same_type<T: Display>(a: T, b: T) { }
same_type(1, 2);     // OK — ambos i32
// same_type(1, "a"); // error — i32 != &str

// Con impl Trait, cada parametro puede ser un tipo distinto:
fn different_types(a: impl Display, b: impl Display) { }
different_types(1, "a");  // OK — i32 y &str
```

## Errores comunes

```rust
// ERROR 1: olvidar trait bounds
fn print_value<T>(val: T) {
    println!("{val}");  // error: T doesn't implement Display
}
// Solucion: fn print_value<T: std::fmt::Display>(val: T)

// ERROR 2: devolver T sin que el llamador pueda determinarlo
fn make_default<T: Default>() -> T {
    T::default()
}
let x = make_default();  // error: type annotations needed
// Solucion:
let x: i32 = make_default();
let x = make_default::<String>();

// ERROR 3: confundir monomorphization con dynamic dispatch
fn foo<T: Display>(x: T) { }     // monomorphization — sin overhead
fn bar(x: &dyn Display) { }       // dynamic dispatch — con vtable
// Ambos son validos, pero tienen tradeoffs distintos

// ERROR 4: bounds demasiado restrictivos
fn process<T: Display + Debug + Clone + Send + Sync>(x: T) { }
// Si solo necesitas Display, ¿para que pedir Clone + Send + Sync?
// Usa solo los bounds que realmente necesitas.
```

## Cheatsheet

```text
Sintaxis:
  fn foo<T>(x: T)                 sin bounds (T puede ser cualquier cosa)
  fn foo<T: Trait>(x: T)          con bound
  fn foo<T: A + B>(x: T)          multiples bounds
  fn foo<T, U>(x: T, y: U)       multiples parametros
  fn foo<T>(x: T) where T: Trait  clausula where
  fn foo(x: impl Trait)           azucar (equivalente a generics)
  foo::<Type>(val)                turbofish (especificar tipo)

Monomorphization:
  - El compilador genera una version por tipo concreto
  - Zero-cost en runtime
  - Aumenta tamaño del binario
  - Permite inlining y optimizaciones

Bounds comunes:
  Display          → puede imprimirse con {}
  Debug            → puede imprimirse con {:?}
  Clone            → puede clonarse (.clone())
  Copy             → se copia implicitamente
  PartialEq / Eq   → puede compararse con ==
  PartialOrd / Ord  → puede compararse con <, >, etc.
  Default          → tiene valor por defecto
  Send / Sync      → puede cruzar threads
  Hash             → puede hashearse (HashMap keys)
```

---

## Ejercicios

### Ejercicio 1 — Funcion generica basica

```rust
// Escribe fn min_of_three<T: PartialOrd>(a: T, b: T, c: T) -> T
// que retorne el menor de tres valores.
//
// Prueba con:
//   min_of_three(5, 2, 8)       → 2
//   min_of_three(3.14, 2.71, 1.41) → 1.41
//   min_of_three("banana", "apple", "cherry") → "apple"
//
// ¿Necesitas algun bound adicional ademas de PartialOrd?
// Predice si compila con solo PartialOrd y verifica.
```

### Ejercicio 2 — Monomorphization

```rust
// Dada esta funcion:
fn describe<T: std::fmt::Display>(label: &str, value: T) {
    println!("{label}: {value}");
}

// Llamala con 4 tipos distintos:
//   describe("entero", 42_i32);
//   describe("flotante", 3.14_f64);
//   describe("texto", "hello");
//   describe("booleano", true);
//
// Predice: ¿cuantas versiones genera el compilador?
// ¿Que pasa si agregas describe("otro_entero", 99_i32)?
// ¿Genera una quinta version?
```

### Ejercicio 3 — Filtro generico

```rust
// Escribe fn filter_greater<T>(items: &[T], threshold: &T) -> Vec<&T>
// que retorne los elementos mayores que threshold.
//
// Determina que bounds necesita T.
//
// Prueba con:
//   filter_greater(&[1, 5, 3, 8, 2], &3)       → [&5, &8]
//   filter_greater(&[1.0, 0.5, 2.0], &0.7)     → [&1.0, &2.0]
//   filter_greater(&["cat", "ant", "dog"], &"bat") → [&"cat", &"dog"]
```
