# T04 — Inferencia de tipos

## Como funciona la inferencia de tipos

Rust tiene un sistema de inferencia de tipos basado en
Hindley-Milner. El compilador analiza el contexto (asignaciones,
retornos de funciones, como se usa una variable) para deducir
los tipos sin que el programador los escriba explicitamente.

```rust
// El compilador infiere el tipo a partir del valor asignado:
let x = 5;           // i32 (tipo entero por defecto)
let y = 3.14;        // f64 (tipo flotante por defecto)
let active = true;   // bool
let letter = 'A';    // char

// Estos dos son equivalentes — la anotacion es redundante:
let x = 5;
let x: i32 = 5;
```

Restriccion fundamental: la inferencia funciona **dentro del
cuerpo de una funcion**, pero **no cruza las firmas de funciones**.
Los parametros y el tipo de retorno siempre requieren anotacion:

```rust
// Las firmas SIEMPRE requieren tipos explicitos:
fn add(a: i32, b: i32) -> i32 {
    a + b
}

// ESTO NO COMPILA:
// fn add(a, b) -> i32 { a + b }
// error[E0282]: type annotations needed

// Dentro del cuerpo, la inferencia funciona libremente:
fn example() -> Vec<String> {
    let mut results = Vec::new();       // Vec<???> por ahora
    results.push(String::from("one"));  // ahora sabe: Vec<String>
    results
}
// Razon de diseno: las firmas son el contrato publico.
// Si se infirieran, un cambio en el cuerpo podria cambiar
// la firma sin que el programador lo note.
```

## Cuando la inferencia funciona

```rust
// A partir de constructores y funciones:
let s = String::new();              // String
let s = String::from("hello");     // String
let v = vec![1, 2, 3];             // Vec<i32>
let v = vec![1.0, 2.0, 3.0];      // Vec<f64>
```

```rust
// A partir del uso posterior de la variable:
let mut v = Vec::new();     // tipo aun desconocido
v.push(42);                 // ahora el compilador sabe: Vec<i32>

let mut m = HashMap::new(); // tipo aun desconocido
m.insert("key", 10);        // ahora sabe: HashMap<&str, i32>
```

```rust
// A partir del tipo de retorno de funciones:
fn get_count() -> usize { 42 }
let count = get_count();    // count es usize

// En closures — los parametros se infieren del uso:
let numbers = vec![1, 2, 3, 4, 5];
let doubled: Vec<i32> = numbers.iter().map(|x| x * 2).collect();
// x se infiere como &i32 (iter() produce referencias)
// No hace falta escribir |x: &i32| x * 2
```

## Cuando se DEBE anotar

### Firmas de funciones — siempre

```rust
fn calculate(base: f64, exponent: u32) -> f64 {
    base.powi(exponent as i32)
}
```

### Contenedores vacios sin contexto

```rust
// Vec::new() sin push posterior — no hay de donde inferir:
let v: Vec<i32> = Vec::new();

// Sin la anotacion:
// let v = Vec::new();
// error[E0282]: type annotations needed
//   |
//   |     let v = Vec::new();
//   |         ^ consider giving `v` a type

// Lo mismo con otros contenedores:
let m: HashMap<String, Vec<i32>> = HashMap::new();
let s: HashSet<u64> = HashSet::new();
```

### Ambigedad entre tipos numericos

```rust
// parse() puede devolver cualquier tipo numerico:
let n: i32 = "42".parse().unwrap();
let n: f64 = "42".parse().unwrap();
let n: u8 = "42".parse().unwrap();

// Sin anotacion:
// let n = "42".parse().unwrap();
// error[E0284]: type annotations needed
//   cannot infer type of the type parameter `F`
```

### Constantes, estaticas y definiciones de tipos

```rust
// const y static SIEMPRE requieren tipo explicito:
const MAX_SIZE: usize = 1024;
const PI: f64 = 3.14159265358979;
static COUNTER: u32 = 0;

// Sin tipo:
// const MAX_SIZE = 1024;
// error: missing type for `const` item

// Los campos de struct y enum tambien:
struct Point {
    x: f64,
    y: f64,
}

// No hay inferencia en definiciones de tipos.
// El compilador necesita saber el layout en memoria
// en el momento de la definicion.
```

## Sintaxis turbofish `::<>`

Turbofish es una forma alternativa de especificar tipos
sin usar una anotacion en la variable. Se escribe directamente
en la llamada a la funcion o metodo:

```rust
// Estas dos formas son equivalentes:
let n: i32 = "42".parse().unwrap();       // anotacion en la variable
let n = "42".parse::<i32>().unwrap();     // turbofish en el metodo

// La sintaxis es:
//   valor.metodo::<Tipo>()
//   funcion::<Tipo>(argumentos)
//
// Los ::<> van ANTES de los parentesis.
// Se llama "turbofish" porque ::<> parece un pez:
//   ::<>  →  un pez nadando hacia la derecha
//   ::     los ojos
//   <>     la cola
```

### Ejemplos comunes de turbofish

```rust
// parse — convertir strings:
let x = "42".parse::<i32>().unwrap();
let y = "3.14".parse::<f64>().unwrap();
let z = "255".parse::<u8>().unwrap();
let flag = "true".parse::<bool>().unwrap();
```

```rust
// collect — convertir un iterador a una coleccion:
let numbers = vec![1, 2, 3, 4, 5];
let doubled = numbers.iter().map(|x| x * 2).collect::<Vec<i32>>();

use std::collections::HashSet;
let unique = numbers.iter().collect::<HashSet<&i32>>();

use std::collections::HashMap;
let pairs = vec![("a", 1), ("b", 2)];
let map = pairs.into_iter().collect::<HashMap<&str, i32>>();
```

```rust
// Constructores de contenedores — turbofish va en el tipo:
let v = Vec::<i32>::new();
let m = HashMap::<String, i32>::new();

//   Vec::<i32>::new()    CORRECTO
//   Vec::new::<i32>()    INCORRECTO — new() no tiene parametro generico propio

// Funciones genericas independientes:
use std::mem;
let size = mem::size_of::<i32>();       // 4
let size = mem::size_of::<f64>();       // 8
```

### Turbofish vs anotacion de tipo

```rust
// Anotacion de tipo — mejor cuando se declara una variable:
let numbers: Vec<i32> = vec![1, 2, 3];

// Turbofish — mejor en cadenas de metodos o sin variable:
let sum = vec![1, 2, 3].iter().sum::<i32>();
println!("{}", "42".parse::<i32>().unwrap());

// En cadenas largas, turbofish evita romper la cadena:
let result = data
    .iter()
    .filter(|x| x.is_valid())
    .map(|x| x.value())
    .collect::<Vec<_>>();

// Con anotacion, habria que separar la declaracion:
let result: Vec<_> = data
    .iter()
    .filter(|x| x.is_valid())
    .map(|x| x.value())
    .collect();
```

## El placeholder `_` para inferencia parcial

El guion bajo `_` le dice al compilador: "infiere este tipo
por mi, pero yo especifico la estructura exterior":

```rust
// Especificar Vec, dejar que el compilador infiera el elemento:
let v: Vec<_> = vec![1, 2, 3];           // _ → i32
let v: Vec<_> = vec!["hello", "world"];  // _ → &str

// Muy util con collect:
let doubled: Vec<_> = numbers.iter().map(|x| x * 2).collect();
// Equivalente con turbofish:
let doubled = numbers.iter().map(|x| x * 2).collect::<Vec<_>>();
```

```rust
// Con HashMap — especificar estructura, inferir tipos:
use std::collections::HashMap;
let pairs = vec![("a", 1), ("b", 2), ("c", 3)];
let map: HashMap<_, _> = pairs.into_iter().collect();
// HashMap<&str, i32> inferido automaticamente

// _ funciona en cualquier posicion de tipo generico:
let result: Result<_, String> = Ok(42);   // _ es i32
```

```rust
// NO se puede usar _ en firmas de funciones:
// fn process(data: Vec<_>) -> _ {  }
// error: the placeholder `_` is not allowed within types on item signatures

// _ es exclusivo para variables locales dentro de cuerpos de funcion.
```

## Propagacion de la inferencia

El compilador infiere tipos en ambas direcciones: hacia
adelante (del valor al uso) y hacia atras (del uso al valor).

### Propagacion hacia adelante

```rust
// El tipo se establece en la asignacion y se propaga:
let x = 5u32;           // x es u32
let y = x + 10;         // y es u32 (u32 + u32 = u32)
let z = y * 2;          // z es u32
// Cadena: 5u32 → x: u32 → y: u32 → z: u32
```

### Propagacion hacia atras

```rust
// El tipo se determina por como se usa la variable MAS ADELANTE:
let mut v = Vec::new();                 // tipo desconocido aun
v.push(String::from("hello"));         // ahora sabe: Vec<String>
// El compilador "retrocede" y asigna el tipo a v.

// Otro ejemplo:
let x = Default::default();   // tipo desconocido
let y: i64 = x;               // ahora sabe que x es i64
```

```rust
// Inferencia hacia atras con retorno de funcion:
fn get_default() -> String {
    let value = Default::default();  // inferido como String por el retorno
    value
}
// El compilador ve que value se retorna, y la funcion retorna String,
// entonces Default::default() se resuelve como String::default().
```

### Inferencia bidireccional combinada

```rust
// El compilador usa toda la informacion disponible:
let mut items = Vec::new();          // tipo desconocido
items.push(10);                      // ahora sabe: Vec<i32>
let first: u8 = items[0];           // ERROR: esperaba u8, tiene i32
// error[E0308]: mismatched types — expected `u8`, found `i32`

// Si cambias la linea del push:
let mut items = Vec::new();
items.push(10u8);                    // Vec<u8>
let first: u8 = items[0];           // OK
```

## Ambigedad y como resolverla

Cuando el compilador no puede decidir el tipo, emite un error.
Hay varias formas de resolverlo:

```rust
// Ejemplo 1: parse sin contexto
// let n = "42".parse().unwrap();
// error[E0284]: type annotations needed

let n: i32 = "42".parse().unwrap();          // solucion: anotacion
let n = "42".parse::<i32>().unwrap();        // solucion: turbofish
```

```rust
// Ejemplo 2: collect sin destino claro
let numbers = vec![1, 2, 3];
// let result = numbers.iter().collect();
// error[E0282]: type annotations needed

let result: Vec<&i32> = numbers.iter().collect();          // anotacion
let result = numbers.iter().collect::<Vec<&i32>>();         // turbofish
let result = numbers.iter().collect::<Vec<_>>();            // turbofish + placeholder
```

```rust
// Ejemplo 3: Default sin contexto
// let x = Default::default();
// error[E0282]: type annotations needed

let x: i32 = Default::default();
let x: String = Default::default();
let x: Vec<u8> = Default::default();
```

## Patrones comunes

```rust
// Anotar solo el contenedor, inferir el contenido:
let numbers: Vec<_> = (0..10).collect();
// El compilador sabe que Range<i32>::Item es i32.

let map: HashMap<_, _> = data.into_iter().collect();
// Inferencia automatica de clave y valor.
```

```rust
// Sufijos literales en vez de anotaciones:
let x = 255u8;
let y = 1_000_000i64;
let z = 3.14f32;
// Mas compactos que let x: u8 = 255;
```

```rust
// Inferencia dentro de patrones (if let, match):
let value: Result<i32, String> = Ok(42);

if let Ok(n) = value {
    println!("{}", n);   // n es i32, inferido del tipo de value
}

match value {
    Ok(n) => println!("Got: {}", n),    // n: i32
    Err(e) => println!("Error: {}", e), // e: String
}
```

```rust
// Iteradores propagan tipos a traves de la cadena:
let names = vec!["Alice", "Bob", "Charlie"];

let upper: Vec<String> = names
    .iter()                             // Iter<&str>
    .map(|s| s.to_uppercase())         // produce String
    .collect();                         // Vec<String>
// El compilador infiere todo a partir de names y la anotacion en upper.
```

## Resumen de reglas

```rust
// Donde se REQUIERE anotacion de tipo:
//   1. Parametros de funcion          → fn foo(x: i32)
//   2. Tipo de retorno de funcion     → fn foo() -> i32
//   3. Constantes                     → const X: i32 = 5
//   4. Estaticas                      → static X: i32 = 5
//   5. Campos de struct/enum          → struct S { x: i32 }
//   6. Cuando el compilador no puede  → let v: Vec<i32> = Vec::new()
//      inferir
//
// Donde la inferencia funciona:
//   1. Variables locales con valor     → let x = 5
//   2. Variables inferidas por uso     → let mut v = Vec::new(); v.push(1)
//   3. Closures (parametros y retorno) → |x| x + 1
//   4. Tipos genericos con contexto   → vec![1, 2, 3]
//
// Herramientas para ayudar al compilador:
//   1. Anotacion de tipo              → let x: i32 = ...
//   2. Turbofish                      → .parse::<i32>()
//   3. Sufijo literal                 → 42u8, 3.14f32
//   4. Placeholder parcial            → Vec<_>, HashMap<_, _>
```

---

## Ejercicios

### Ejercicio 1 — Inferencia basica

```rust
// Para cada let, determinar que tipo infiere el compilador.
// Escribir el tipo al lado como comentario.
// Despues, verificar compilando con anotaciones explicitas.
//
// Ejemplo:
//   let x = 5;           // i32
//   let x: i32 = 5;      // verificacion: compila OK
//
// Variables a analizar:
//   let a = 42;
//   let b = 42.0;
//   let c = 42u8;
//   let d = true;
//   let e = 'R';
//   let f = "hello";
//   let g = String::from("hello");
//   let h = vec![1, 2, 3];
//   let i = vec![1.0, 2.0];
//   let j = (1, "two", 3.0);
//   let k = [0; 5];
//   let l = &42;
//   let m = Box::new(7);
```

### Ejercicio 2 — Resolver errores de inferencia

```rust
// Cada uno de estos fragmentos NO compila por falta de informacion
// de tipo. Corregirlos de DOS formas distintas:
//   a) con anotacion de tipo en la variable
//   b) con turbofish
//
// Fragmento 1:
//   let v = Vec::new();
//
// Fragmento 2:
//   let n = "100".parse().unwrap();
//
// Fragmento 3:
//   let result = (0..10).collect();
//
// Fragmento 4:
//   let default_val = Default::default();
//   let doubled: i32 = default_val * 2;
//
// Para el fragmento 4, explicar por que la segunda linea
// no es suficiente para que el compilador infiera el tipo
// de default_val (pista: la multiplicacion es ambigua
// porque multiples tipos implementan Mul).
```

### Ejercicio 3 — Turbofish y collect

```rust
// Dado este vector:
//   let words = vec!["hello", "world", "rust", "hello", "rust"];
//
// Usar iteradores con collect y turbofish para producir:
//   1. Un Vec<String> con las palabras en mayusculas
//   2. Un HashSet<&str> con las palabras unicas
//   3. Un Vec<(usize, &&str)> con indice y palabra (usar enumerate)
//   4. Un String con todas las palabras unidas por ", "
//      (pista: no es collect, es join o fold)
//
// Para cada caso, escribir la version con turbofish
// Y la version con anotacion de tipo.
```
