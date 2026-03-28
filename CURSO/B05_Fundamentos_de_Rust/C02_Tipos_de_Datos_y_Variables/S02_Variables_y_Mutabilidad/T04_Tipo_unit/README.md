# T04 — Tipo unit ()

## Que es el tipo unit

El tipo unit se escribe `()`. Es un tipo que tiene **un solo valor
posible**, tambien escrito `()`. La misma sintaxis representa el
tipo y su unico valor. Ocupa **cero bytes** en memoria.

```rust
let x: () = ();
// x tiene tipo (), contiene el valor (), ocupa 0 bytes.

use std::mem::size_of;
println!("{}", size_of::<()>());     // 0
println!("{}", size_of::<i32>());    // 4
println!("{}", size_of::<bool>());   // 1
```

En C, `void` cumple un rol similar pero no es un tipo real. No se
puede declarar una variable de tipo `void`. En Rust, `()` **si** es
un tipo completo con un valor concreto:

```c
// C — void no es un valor:
void greet(void) { printf("Hello\n"); }
// void x = greet();   // error: variable has incomplete type 'void'
```

```rust
// Rust — () si es un valor:
fn greet() {
    println!("Hello");
}

fn main() {
    let x = greet();       // compila perfectamente
    println!("{:?}", x);   // ()
}
```

## Donde aparece implicitamente

### Funciones sin valor de retorno

Cuando una funcion no declara tipo de retorno, Rust entiende
que retorna `()`:

```rust
// Estas dos firmas son identicas:
fn greet() { println!("Hello"); }
fn greet_explicit() -> () { println!("Hello"); }
```

### El punto y coma convierte expresiones en statements

Un bloque retorna el valor de su **ultima expresion**. Si esa
expresion termina con `;`, se convierte en statement y el bloque
retorna `()`:

```rust
fn main() {
    let a = { 5 };     // a = 5, tipo i32
    let b = { 5; };    // b = (), tipo ()
    // El ; convierte la expresion 5 (i32) en statement (()).

    let x = println!("hi");   // x es () — println! retorna ()

    let y = {
        let a = 10;
        let b = 20;
        a + b       // sin ; → el bloque retorna 30 (i32)
    };

    let z = {
        let a = 10;
        let b = 20;
        a + b;      // con ; → el bloque retorna ()
    };
}
```

```rust
fn returns_i32() -> i32 {
    42          // sin ; → expresion de retorno
}

// fn broken() -> i32 {
//     42;      // con ; → retorna () en vez de i32
// }
// Error: expected `i32`, found `()`
```

### if/else y bloques

Cuando `if/else` no produce un valor, ambas ramas retornan `()`.
Cualquier bloque cuya ultima linea sea un statement retorna `()`:

```rust
fn main() {
    let result = if 5 > 3 {
        println!("big");      // println! retorna ()
    } else {
        println!("small");
    };
    // result tiene tipo ()

    // Si las ramas difieren en tipo:
    // let bad = if true { 10 } else { println!("no"); };
    // Error: incompatible types — i32 vs ()

    let x = {
        let msg = String::from("hello");
        println!("{}", msg);
        // El bloque retorna ()
    };
}
```

## Por que importa

### Lenguaje orientado a expresiones

Rust es un lenguaje orientado a expresiones: **todo** tiene un
tipo y produce un valor. No existen "vacios" en el sistema de
tipos. Donde otros lenguajes tienen void o "nada", Rust usa `()`:

```rust
fn main() {
    let a = 5;                    // i32
    let b = true;                 // bool
    let c = println!("hi");      // ()
    let d = {};                   // ()
    let e = if true {} else {};   // ()
    let f = { 5; };              // ()
}
```

### Codigo generico

Porque `()` es un tipo real, se puede usar como parametro
de tipo en cualquier estructura generica:

```rust
use std::collections::HashMap;

fn main() {
    // HashMap<K, ()> es esencialmente un HashSet:
    let mut seen: HashMap<String, ()> = HashMap::new();
    seen.insert("alice".to_string(), ());
    seen.insert("bob".to_string(), ());
    // De hecho, HashSet<K> internamente usa HashMap<K, ()>.
}
```

```rust
// Result<(), Error> — funciones que pueden fallar pero no
// retornan valor util en caso de exito:
use std::io;

fn create_dir(path: &str) -> Result<(), io::Error> {
    std::fs::create_dir(path)?;
    Ok(())
}

fn main() {
    match create_dir("/tmp/my_dir") {
        Ok(()) => println!("Done"),
        Err(e) => println!("Error: {}", e),
    }
}
```

### Implementaciones de traits

`()` implementa varios traits de la biblioteca estandar:

```rust
fn main() {
    println!("{:?}", ());            // Debug
    let _: () = Default::default();  // Default
    assert_eq!((), ());              // PartialEq, Eq
    assert!(!(()  < ()));            // PartialOrd, Ord
}
```

## () vs never type (!)

Dos tipos que representan "ausencia", con significados opuestos:

- `()` — la funcion **retorna**, pero sin un valor util.
- `!` — la funcion **nunca retorna** (diverge).

```rust
fn greet() -> () { println!("Hello"); }       // retorna ()
fn crash() -> ! { panic!("fatal error"); }    // nunca retorna
```

```rust
// Funciones que producen !:
fn never_loop() -> ! { loop {} }
fn never_exit() -> ! { std::process::exit(1); }

// todo!() y unimplemented!() tambien retornan !
fn not_yet() -> i32 {
    todo!()   // compila porque ! se coerce a cualquier tipo
}
```

```rust
// ! es el bottom type: se coerce a cualquier tipo.
fn main() {
    let x: i32 = if true {
        42
    } else {
        panic!("never reached")   // ! coerced to i32
    };
}
```

```rust
// Comparacion:
//
// ()                           !
// ──────────────────────────   ──────────────────────────────
// Un valor: ()                 Cero valores (no se instancia)
// La funcion retorna            La funcion nunca retorna
// Tamano: 0 bytes              No tiene tamano
// Se puede crear: let x = (); No se puede crear un valor de !
// Tipo de println!()           Tipo de panic!()
```

## () en genericos

```rust
fn main() {
    // Vec<()> — tiene longitud pero los elementos no ocupan espacio:
    let v: Vec<()> = vec![(); 1_000];
    println!("len: {}", v.len());   // 1000
    // Los elementos () no ocupan espacio en el heap.
    // En la practica, Vec<()> funciona como un contador.
}
```

```rust
fn main() {
    // Option<()> — solo dos estados:
    let a: Option<()> = Some(());   // "presente"
    let b: Option<()> = None;       // "ausente"

    // Some solo puede contener (), asi que Option<()>
    // es esencialmente un bool:
    //   Some(()) ~ true
    //   None     ~ false
    println!("{}", std::mem::size_of::<Option<()>>());   // 1
}
```

```rust
// Result<(), E> — el patron mas comun con () en genericos:
use std::io::{self, Write};

fn write_log(msg: &str) -> Result<(), io::Error> {
    let mut file = std::fs::OpenOptions::new()
        .append(true).create(true)
        .open("/tmp/app.log")?;
    writeln!(file, "{}", msg)?;
    Ok(())
}

fn main() {
    if let Err(e) = write_log("started") {
        eprintln!("Failed: {}", e);
    }
}
```

## Zero-sized types (ZSTs)

`()` pertenece a una categoria llamada **zero-sized types**
(tipos de tamano cero). No ocupan espacio en memoria en
tiempo de ejecucion. El compilador los optimiza completamente:

```rust
use std::mem::size_of;

struct Marker;           // struct vacio, tambien ZST
struct Phantom { _a: (), _b: () }

fn main() {
    println!("{}", size_of::<()>());        // 0
    println!("{}", size_of::<Marker>());    // 0
    println!("{}", size_of::<Phantom>());   // 0
}
```

```rust
fn main() {
    // Vec<()> no asigna memoria en el heap para elementos:
    let v: Vec<()> = vec![(); 1_000_000];
    println!("len: {}", v.len());   // 1000000
    // Solo mantiene el contador de longitud. Cero costo en runtime.
}
```

## Error comun: el punto y coma cambia el tipo de retorno

Un solo `;` de mas cambia el tipo de retorno de una funcion:

```rust
// CORRECTO — retorna i32:
fn double(x: i32) -> i32 {
    x * 2
}

// INCORRECTO — retorna ():
// fn double(x: i32) -> i32 {
//     x * 2;
// }
// Error:
//   |     x * 2;
//   |          - help: remove this semicolon to return this value
//   = note: expected type `i32`, found type `()`
```

```rust
fn main() {
    let numbers = vec![1, 2, 3, 4, 5];

    // CORRECTO:
    let doubled: Vec<i32> = numbers.iter().map(|x| x * 2).collect();

    // INCORRECTO — el ; en el closure hace que retorne ():
    // let doubled: Vec<i32> = numbers.iter().map(|x| { x * 2; }).collect();
}
```

```rust
// Regla mnemotecnica:
//
//   { expresion }   →  retorna el valor de la expresion
//   { expresion; }  →  descarta el valor, retorna ()
//
// Aplica a: funciones, closures, if/else, match, cualquier bloque {}.
```

---

## Ejercicios

### Ejercicio 1 — Identificar tipos

```rust
// Sin ejecutar el codigo, determinar el tipo de cada variable.
// Luego verificar compilando con {:?} en println!:

fn foo() {}
fn bar() -> i32 { 42 }

fn main() {
    let a = foo();
    let b = bar();
    let c = { 10 };
    let d = { 10; };
    let e = println!("test");
    let f = if true { 1 } else { 2 };
    let g = if true { println!("yes"); } else { println!("no"); };
    let h = {};
    let i = { { { () } } };

    // Para cada variable, responder:
    //   - Que tipo tiene?
    //   - Por que?
}
```

### Ejercicio 2 — Corregir el punto y coma

```rust
// El siguiente codigo no compila. Corregir los errores
// sin cambiar la logica, solo ajustando punto y coma:

// fn add(a: i32, b: i32) -> i32 {
//     a + b;
// }
//
// fn is_positive(n: i32) -> bool {
//     if n > 0 {
//         true;
//     } else {
//         false;
//     }
// }
//
// fn describe(n: i32) -> &'static str {
//     match n {
//         0 => { "zero"; },
//         _ => { "nonzero"; },
//     }
// }
//
// fn main() {
//     let x = add(3, 4);
//     let y = is_positive(x);
//     let z = describe(x);
//     println!("{} {} {}", x, y, z);
// }
```

### Ejercicio 3 — Result con unit

```rust
// Implementar las siguientes funciones que retornan Result<(), String>:
//
// 1. validate_age(age: i32) -> Result<(), String>
//    - Ok(()) si age esta entre 0 y 150
//    - Err con mensaje descriptivo si no
//
// 2. validate_name(name: &str) -> Result<(), String>
//    - Ok(()) si name no esta vacio y tiene menos de 100 caracteres
//    - Err con mensaje descriptivo si no
//
// 3. validate_user(name: &str, age: i32) -> Result<(), String>
//    - Usa ? para llamar a validate_name y validate_age
//    - Retorna Ok(()) si ambas pasan
//
// fn main() {
//     match validate_user("Alice", 30) {
//         Ok(()) => println!("Valid user"),
//         Err(e) => println!("Invalid: {}", e),
//     }
//
//     match validate_user("", 200) {
//         Ok(()) => println!("Valid user"),
//         Err(e) => println!("Invalid: {}", e),
//     }
// }
```
