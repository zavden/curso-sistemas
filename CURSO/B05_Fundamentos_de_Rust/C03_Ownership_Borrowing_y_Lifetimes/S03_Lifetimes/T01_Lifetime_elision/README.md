# T01 — Lifetime elision

## Que son los lifetimes

Cada referencia en Rust tiene un **lifetime**: el ambito (scope) durante
el cual esa referencia es valida. El compilador rastrea los lifetimes
para garantizar que ninguna referencia sobreviva al dato al que apunta.
La mayoria de las veces el compilador los infiere; en ciertos casos
necesitas anotarlos a mano.

```rust
// Toda referencia tiene un lifetime, aunque no lo escribas:

fn main() {
    let r;                      // ---------+-- 'a
    {                           //          |
        let x = 5;             // -+-- 'b  |
        r = &x;                //  |       |
    }                           // -+       |
    // println!("{}", r);       // ERROR: x ya no existe
    //                          // r tiene lifetime 'a, pero x solo vive 'b
    //                          // 'b es mas corto que 'a → referencia colgante
}                               // ---------+
```

```rust
// Lifetime annotation syntax — apostrofo + nombre: 'a, 'b, 'input
// Por convencion se usa 'a, 'b, etc. Son genericos de lifetime.

fn example<'a>(s: &'a str) -> &'a str {
    s
}
// <'a>       → declara el lifetime generico 'a
// &'a str    → referencia a str con lifetime 'a
// -> &'a str → el retorno vive al menos tanto como 'a
```

## Por que existen los lifetimes

El proposito es **prevenir dangling references**. Cuando una funcion
recibe referencias y devuelve una referencia, el compilador necesita
saber a cual de las entradas esta ligada la salida:

```rust
// El compilador no puede saber cual de los dos devuelve:
fn longest(x: &str, y: &str) -> &str {
    if x.len() > y.len() { x } else { y }
}

// error[E0106]: missing lifetime specifier
//  --> src/main.rs:1:33
//   |
// 1 | fn longest(x: &str, y: &str) -> &str {
//   |               ----     ----      ^ expected named lifetime parameter
//   |
//   = help: this function's return type contains a borrowed value,
//           but the signature does not say whether it is borrowed from `x` or `y`
```

```rust
// Por que importa: sin saber de donde viene la referencia,
// el compilador no puede verificar que es valida:
fn main() {
    let string1 = String::from("long string");
    let result;
    {
        let string2 = String::from("xyz");
        result = longest(string1.as_str(), string2.as_str());
    }
    // Si result apunta a string2, aqui ya es invalida.
    // Si result apunta a string1, sigue siendo valida.
    // El compilador necesita saberlo EN TIEMPO DE COMPILACION.
}

// Solucion: anotar lifetimes para indicar la relacion:
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
// 'a dice: "el retorno vive al menos tanto como el menor
// de los lifetimes de x e y".
```

## Las 3 reglas de elision

Antes de Rust 1.0, habia que anotar lifetimes en **todas** las funciones
que usaran referencias. Los desarrolladores notaron que los mismos
patrones se repetian, asi que se codificaron tres reglas que el
compilador aplica automaticamente. Si despues de las tres reglas
el compilador determina todos los lifetimes, no necesitas anotarlos.

Terminologia: los lifetimes en parametros son **input lifetimes**;
los lifetimes en valores de retorno son **output lifetimes**.

### Regla 1 — Cada parametro referencia recibe su propio lifetime

```rust
// Un parametro referencia → un lifetime:
fn first_word(s: &str) -> &str { /* ... */ }
// → fn first_word<'a>(s: &'a str) -> &str

// Dos parametros referencia → dos lifetimes:
fn longest(x: &str, y: &str) -> &str { /* ... */ }
// → fn longest<'a, 'b>(x: &'a str, y: &'b str) -> &str

// Parametros no-referencia no reciben lifetime:
fn mixed(x: &str, y: i32, z: &str) -> &str { /* ... */ }
// → fn mixed<'a, 'b>(x: &'a str, y: i32, z: &'b str) -> &str
```

### Regla 2 — Si hay exactamente UN input lifetime, se asigna a todos los output lifetimes

```rust
// Un solo input lifetime → se asigna al output:
fn first_word(s: &str) -> &str { /* ... */ }
// Regla 1: fn first_word<'a>(s: &'a str) -> &str
// Regla 2: fn first_word<'a>(s: &'a str) -> &'a str
// RESUELTO — no necesitas anotar nada.

// Dos input lifetimes → Regla 2 NO aplica:
fn longest(x: &str, y: &str) -> &str { /* ... */ }
// Regla 1: fn longest<'a, 'b>(x: &'a str, y: &'b str) -> &str
// Regla 2: no aplica (hay dos lifetimes, no uno).
// Output sigue sin resolver → pasa a Regla 3.
```

### Regla 3 — Si uno de los parametros es &self o &mut self, el lifetime de self se asigna a todos los output lifetimes

Esta regla existe para metodos. En la mayoria de los metodos que
devuelven referencias, la referencia apunta a datos del struct:

```rust
struct ImportantExcerpt<'a> {
    part: &'a str,
}

impl<'a> ImportantExcerpt<'a> {
    // Lo que escribes:
    fn announce_and_return_part(&self, announcement: &str) -> &str {
        println!("Attention: {}", announcement);
        self.part
    }
    // Regla 1: fn announce_and_return_part<'b, 'c>(&'b self, announcement: &'c str) -> &str
    // Regla 2: dos input lifetimes → no aplica.
    // Regla 3: &self presente → asignar 'b al output:
    //          fn announce_and_return_part<'b, 'c>(&'b self, announcement: &'c str) -> &'b str
    // RESUELTO.
}
```

```rust
// Si devuelves algo que NO es de self, necesitas anotar:
impl<'a> ImportantExcerpt<'a> {
    fn return_announcement<'b>(&self, announcement: &'b str) -> &'b str {
        announcement
    }
    // Regla 3 asignaria lifetime de self al retorno,
    // pero el retorno viene de announcement → conflicto.
    // Anotacion manual necesaria.
}
```

## Cuando la elision funciona

No necesitas anotar lifetimes en estas situaciones:

```rust
// Caso 1: una sola referencia de entrada con retorno referencia.
// Regla 1 + Regla 2 resuelven todo.
fn first_char(s: &str) -> &str { &s[..1] }
fn trim(s: &str) -> &str { s.trim() }

// Caso 2: metodos que devuelven referencias.
// Regla 1 + Regla 3 resuelven todo.
struct Config { name: String }
impl Config {
    fn name(&self) -> &str { &self.name }
    fn name_or_default(&self, _default: &str) -> &str { &self.name }
}

// Caso 3: funciones que reciben referencia pero no devuelven referencia.
// Solo Regla 1, no hay output lifetimes que resolver.
fn print_length(s: &str) { println!("{}", s.len()); }
fn count_chars(s: &str) -> usize { s.chars().count() }
```

## Cuando la elision falla

La elision falla cuando hay **multiples parametros referencia** y
el retorno es una referencia:

```rust
// NO compila — elision no puede resolver el output:
fn longest(x: &str, y: &str) -> &str {
    if x.len() > y.len() { x } else { y }
}

// error[E0106]: missing lifetime specifier
// help: consider introducing a named lifetime parameter
//   |
// 1 | fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
//   |           ++++     ++          ++           ++

// Paso a paso del fallo:
// Regla 1: fn longest<'a, 'b>(x: &'a str, y: &'b str) -> &str
// Regla 2: dos input lifetimes → no aplica.
// Regla 3: no hay &self → no aplica.
// Output sin resolver → ERROR.
```

```rust
// Solucion:
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
// "el resultado vive al menos tanto como el menor de los lifetimes de x e y".
```

```rust
// Cuando el retorno SIEMPRE viene de uno solo de los parametros:
fn always_first(x: &str, y: &str) -> &str { x }
// Tampoco compila — la elision falla igual.

// Solucion mas precisa — anotar solo lo necesario:
fn always_first<'a>(x: &'a str, _y: &str) -> &'a str { x }
// 'a solo en x y en el retorno. Le dice al compilador que
// el resultado solo depende de x, no de y.
```

```rust
// Fallo en metodos — cuando el retorno NO viene de self:
struct Parser { input: String }
impl Parser {
    // Compila — Regla 3 asigna lifetime de self:
    fn input(&self) -> &str { &self.input }

    // NO compila — el retorno viene de data, no de self:
    // fn process(&self, data: &str) -> &str { data }
    // Regla 3 asigna lifetime de self al retorno, pero
    // el retorno realmente viene de data → conflicto.

    // Solucion:
    fn process<'b>(&self, data: &'b str) -> &'b str { data }
}
```

## Leyendo la elision del compilador

Paso a paso para varias firmas:

```rust
// Ejemplo 1: fn foo(x: &str) -> &str
// Regla 1: fn foo<'a>(x: &'a str) -> &str
// Regla 2: un input lifetime → asignar: fn foo<'a>(x: &'a str) -> &'a str
// RESUELTO.

// Ejemplo 2: fn bar(x: &str, y: &str)
// Regla 1: fn bar<'a, 'b>(x: &'a str, y: &'b str)
// No hay output lifetimes → RESUELTO.

// Ejemplo 3: fn baz(x: &str, y: &str) -> &str
// Regla 1: fn baz<'a, 'b>(x: &'a str, y: &'b str) -> &str
// Regla 2: dos input lifetimes → no aplica.
// Regla 3: no hay &self → no aplica.
// NO RESUELTO → error.

// Ejemplo 4: fn method(&self, x: &str) -> &str
// Regla 1: fn method<'a, 'b>(&'a self, x: &'b str) -> &str
// Regla 2: dos input lifetimes → no aplica.
// Regla 3: &self → asignar 'a: fn method<'a, 'b>(&'a self, x: &'b str) -> &'a str
// RESUELTO.

// Ejemplo 5: fn convert(x: &str) -> (&str, &str)
// Regla 1: fn convert<'a>(x: &'a str) -> (&str, &str)
// Regla 2: un input → asignar a TODOS: fn convert<'a>(x: &'a str) -> (&'a str, &'a str)
// RESUELTO.

// Ejemplo 6: fn pick(x: &str, y: &str) -> (&str, &str)
// Regla 1: fn pick<'a, 'b>(x: &'a str, y: &'b str) -> (&str, &str)
// Regla 2: dos input → no aplica.  Regla 3: no &self → no aplica.
// NO RESUELTO → error. Necesitas anotacion manual.
```

## Elision en structs, traits y 'static

Las reglas de elision aplican a **firmas de funciones**. En otros
contextos:

```rust
// En structs: NO hay elision. Debes anotar siempre.
struct Excerpt<'a> {
    part: &'a str,
}
// Esto NO compila:
// struct Excerpt {
//     part: &str,    // error: missing lifetime specifier
// }

// En impl blocks: los metodos se benefician de las reglas normales.
impl<'a> Excerpt<'a> {
    fn part(&self) -> &str { self.part }       // Regla 3
    fn new(text: &str) -> Excerpt { Excerpt { part: text } }  // Regla 2
}

// En traits: las firmas siguen las mismas reglas.
trait Summary {
    fn first_line(&self) -> &str;              // Regla 3
    fn excerpt(&self, max_len: usize) -> &str; // Regla 3
}
```

```rust
// 'static — lifetime especial: "vive toda la ejecucion del programa".
// No forma parte de las reglas de elision.

let s: &'static str = "hello";    // string literal → embebido en el binario

fn static_str() -> &'static str {
    "I live forever"
}
// Sin input lifetimes, las reglas no pueden asignar nada al output.
// 'static es necesario aqui.

// Error comun: el compilador sugiere 'static cuando no deberia.
// "consider using the 'static lifetime" → muchas veces la solucion
// correcta es devolver String, no forzar 'static:
fn get_name() -> String {      // BIEN
    String::from("Alice")
}
// fn get_name() -> &'static str {   // MAL si el dato no es estatico
//     let name = String::from("Alice");
//     &name   // ERROR: name se destruye al final de la funcion
// }
```

## Historia de la elision

En las primeras versiones de Rust (pre-1.0), era obligatorio anotar
lifetimes en **todas** las firmas con referencias:

```rust
// Rust antes de lifetime elision (pre-2014):
fn first_word<'a>(s: &'a str) -> &'a str { /* ... */ }
fn print_str<'a>(s: &'a str) { println!("{}", s); }
fn len<'a>(s: &'a str) -> usize { s.len() }
// Incluso las triviales necesitaban anotacion.

// El equipo de Rust analizo el codigo existente:
// - ~87% de funciones con referencias tenian un solo parametro
//   referencia → Regla 2 las cubria.
// - Los metodos casi siempre devolvian datos de self → Regla 3.
// - Los casos de anotacion manual eran la minoria.
//
// RFC 141 (2014) introdujo las reglas de elision.
// Desde entonces:
fn first_word(s: &str) -> &str {
    let bytes = s.as_bytes();
    for (i, &item) in bytes.iter().enumerate() {
        if item == b' ' { return &s[..i]; }
    }
    s
}

// Punto clave: la elision es azucar sintactico.
// El compilador SIEMPRE trabaja con lifetimes explicitos internamente.
// El binario resultante es identico con o sin anotaciones.
// Las reglas no han cambiado desde que se introdujeron.
```

---

## Ejercicios

### Ejercicio 1 — Identificar la elision

```rust
// Para cada firma, aplicar las 3 reglas mentalmente y determinar:
//   a) Los lifetimes que asigna el compilador tras cada regla.
//   b) Si la elision resuelve todos los lifetimes o si hay error.
//
// 1. fn extract(s: &str) -> &str
// 2. fn compare(a: &str, b: &str) -> bool
// 3. fn select(a: &str, b: &str) -> &str
// 4. fn method(&self) -> &str
// 5. fn method(&self, other: &str) -> &str
// 6. fn method(&mut self, data: &str) -> &str
// 7. fn take(s: &str) -> (&str, usize)
// 8. fn split(s: &str) -> (&str, &str)
// 9. fn nothing(x: i32) -> i32
// 10. fn to_ref(s: &String) -> &str
//
// Para cada una, escribir la firma completa con lifetimes explicitos
// tal como la ve el compilador internamente.
```

### Ejercicio 2 — Corregir errores de lifetime

```rust
// Cada funcion tiene un error de lifetime. Corregir la firma
// agregando las anotaciones necesarias.
//
// 1.
// fn longer(a: &str, b: &str) -> &str {
//     if a.len() >= b.len() { a } else { b }
// }
//
// 2.
// fn first_or_second(first: &str, second: &str, use_first: bool) -> &str {
//     if use_first { first } else { second }
// }
//
// 3.
// struct Wrapper { data: String }
// impl Wrapper {
//     fn data_or_alt(&self, alt: &str) -> &str {
//         if self.data.is_empty() { alt } else { &self.data }
//     }
// }
// // Nota: la Regla 3 asigna el lifetime de self al retorno,
// // pero el retorno a veces viene de alt.
// // Pensar en que lifetime annotation permite que ambos caminos funcionen.
//
// Para cada caso, explicar por que la elision no es suficiente.
```

### Ejercicio 3 — Predecir el comportamiento

```rust
// Para cada bloque, predecir si compila o no.
// Si no compila, identificar la linea exacta del error y por que.
//
// Bloque A:
// fn first(s: &str) -> &str { &s[..1] }
// fn main() {
//     let r;
//     {
//         let s = String::from("hello");
//         r = first(&s);
//     }
//     println!("{}", r);
// }
//
// Bloque B:
// fn first(s: &str) -> &str { &s[..1] }
// fn main() {
//     let s = String::from("hello");
//     let r = first(&s);
//     println!("{}", r);
// }
//
// Bloque C:
// fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
//     if x.len() > y.len() { x } else { y }
// }
// fn main() {
//     let s1 = String::from("long string");
//     let result;
//     {
//         let s2 = String::from("hi");
//         result = longest(s1.as_str(), s2.as_str());
//     }
//     println!("{}", result);
// }
```
