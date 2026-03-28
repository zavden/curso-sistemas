# T04 — Copy y Clone

## Que es Copy

Copy es un **marker trait**. No tiene metodos propios, pero le
indica al compilador que un valor se puede duplicar bit a bit al
asignarlo a otra variable, en vez de moverlo. La copia es
**implicita** — no necesitas llamar ningun metodo:

```rust
let x: i32 = 42;
let y = x;      // copia implicita (bitwise copy)
println!("{x}"); // x sigue valido — no hubo move
println!("{y}"); // y es una copia independiente

// Sin Copy, la asignacion seria un move:
let s = String::from("hello");
let t = s;       // move — s ya no es valido
// println!("{s}"); // ERROR: value used after move
```

```rust
// Copy actua en cualquier contexto donde normalmente habria move:
fn show(n: i32) {
    println!("{n}");
}

let x = 10;
show(x);         // x se copia al parametro n
show(x);         // x sigue valido — se puede usar de nuevo
println!("{x}"); // OK
```

```rust
// Definicion de Copy en la libreria estandar:
//
// pub trait Copy: Clone { }
//
// - Marker trait: cuerpo vacio, sin metodos.
// - Requiere Clone como supertrait.
// - El compilador implementa la copia bitwise.
// - No puedes implementar Copy con logica custom:
//   la copia SIEMPRE es bit a bit.
```

## Que tipos implementan Copy

```rust
// Escalares — todos implementan Copy:
// i8, i16, i32, i64, i128, isize
// u8, u16, u32, u64, u128, usize
// f32, f64
// bool, char

// Referencias compartidas (&T) — siempre Copy:
let s = String::from("hello");
let r1 = &s;
let r2 = r1;     // copia la referencia (no el String)
println!("{r1}"); // r1 sigue valido
// NOTA: &T es Copy, pero &mut T NO es Copy.
```

```rust
// Tuplas de tipos Copy (hasta 12 elementos):
let t1: (i32, f64) = (1, 2.0);
let t2 = t1;
println!("{:?}", t1); // OK — tupla de Copy types es Copy

// Pero una tupla con un campo no-Copy NO es Copy:
let t3: (i32, String) = (1, String::from("hello"));
let t4 = t3;
// println!("{:?}", t3); // ERROR: String no es Copy → move
```

```rust
// Arrays [T; N] donde T: Copy:
let arr1: [i32; 5] = [1, 2, 3, 4, 5];
let arr2 = arr1;
println!("{:?}", arr1); // OK — [i32; 5] es Copy

// Function pointers y raw pointers son Copy:
fn add(a: i32, b: i32) -> i32 { a + b }
let fp: fn(i32, i32) -> i32 = add;
let fp2 = fp; // Copy
```

```rust
// Tipos que NO implementan Copy:
//
// String, Vec<T>, Box<T>     — datos en el heap
// HashMap<K,V>, HashSet<T>   — datos en el heap
// Rc<T>, Arc<T>              — reference counting
// File, Mutex                — recursos del SO
// &mut T                     — violaria exclusividad
//
// Regla general: si posee memoria en el heap
// o gestiona un recurso, NO puede ser Copy.
```

## Derivar Copy

Para que un tipo propio implemente Copy, se usa `#[derive(Copy, Clone)]`.
Copy requiere Clone como supertrait, asi que siempre se derivan juntos:

```rust
#[derive(Debug, Copy, Clone)]
struct Point {
    x: f64,
    y: f64,
}

let p1 = Point { x: 1.0, y: 2.0 };
let p2 = p1;       // copia implicita
println!("{:?}", p1); // OK — p1 sigue valido
```

```rust
// Requisito: TODOS los campos deben ser Copy.

#[derive(Debug, Copy, Clone)]
struct Color { r: u8, g: u8, b: u8, a: u8 }
// OK — u8 es Copy.

// #[derive(Debug, Copy, Clone)]
// struct Named {
//     name: String,  // String NO es Copy
//     id: u32,
// }
// ERROR: the trait `Copy` may not be implemented for this type
//     name: String,
//     ^^^^^^^^^^^^ this field does not implement `Copy`
```

```rust
// Enums tambien pueden derivar Copy si todas las variantes son Copy:
#[derive(Debug, Copy, Clone)]
enum Direction { North, South, East, West }

#[derive(Debug, Copy, Clone)]
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
}

let d1 = Direction::North;
let d2 = d1;
println!("{:?}", d1); // OK

// Si una sola variante tiene un campo no-Copy, no compila:
// enum Token { Number(i64), Text(String) }  // no puede ser Copy
```

```rust
// Copy es un compromiso de API publica.
// Si publicas un tipo con Copy y luego lo quitas,
// el codigo downstream que dependia de la copia implicita
// dejara de compilar. Piensa antes de agregar Copy.
```

## El trait Clone

Clone tiene un metodo explicito: `fn clone(&self) -> Self`.
A diferencia de Copy, Clone requiere una llamada explicita
y puede tener logica custom (incluyendo asignaciones de memoria):

```rust
// Clone con String — crea una copia independiente en el heap:
let s1 = String::from("hello");
let s2 = s1.clone(); // copia explicita — asigna nueva memoria
println!("{s1}");     // OK — s1 no se movio
println!("{s2}");     // s2 tiene su propia copia en el heap

// Clone con Vec:
let v1 = vec![1, 2, 3, 4, 5];
let v2 = v1.clone(); // copia el Vec y todos sus elementos
```

```rust
// Derivar Clone para tipos propios:
#[derive(Debug, Clone)]
struct User {
    name: String,
    age: u32,
}

let u1 = User { name: String::from("Alice"), age: 30 };
let u2 = u1.clone(); // clona el String interno
println!("{:?}", u1); // OK — u1 no se movio

// El derive genera algo equivalente a:
// impl Clone for User {
//     fn clone(&self) -> Self {
//         User {
//             name: self.name.clone(),
//             age: self.age.clone(),
//         }
//     }
// }
```

```rust
// Implementacion manual de Clone — logica custom:
#[derive(Debug)]
struct Buffer {
    data: Vec<u8>,
    position: usize,
}

impl Clone for Buffer {
    fn clone(&self) -> Self {
        Buffer {
            data: self.data.clone(),
            position: 0, // reiniciar posicion en la copia
        }
    }
}

let b1 = Buffer { data: vec![1, 2, 3], position: 2 };
let b2 = b1.clone();
// b2.data es [1, 2, 3], b2.position es 0 (logica custom)
```

```rust
// Clone puede ser costoso:
let big_vec: Vec<i32> = (0..1_000_000).collect();
let copy = big_vec.clone(); // asigna 4 MB y copia todo
// Estructuras anidadas multiplican el costo.
```

## Copy vs Clone — comparacion

```rust
// Copy: implicito, bitwise, siempre barato (O(1) fijo).
let a: i32 = 42;
let b = a;         // Copy implicito — sin llamada a metodo

// Clone: explicito, potencialmente costoso, logica arbitraria.
let s = String::from("hello");
let t = s.clone();  // Clone explicito — llamada visible

// Copy implica Clone — todo tipo Copy tambien es Clone:
let x: i32 = 42;
let y = x.clone();  // valido, aunque innecesario
// Clippy advierte: "using `clone` on a `Copy` type"

// No todo Clone es Copy. String es Clone pero NO Copy.
// Copy es un subconjunto de Clone.
```

## Cuando derivar Copy

Derivar Copy es apropiado para tipos pequenos, que viven
enteramente en el stack, donde la duplicacion implicita es
el comportamiento esperado:

```rust
// BUENOS candidatos para Copy:
#[derive(Debug, Copy, Clone, PartialEq)]
struct Point { x: f64, y: f64 }

#[derive(Debug, Copy, Clone)]
struct Rgba { r: u8, g: u8, b: u8, a: u8 }

#[derive(Debug, Copy, Clone, PartialEq)]
enum Suit { Hearts, Diamonds, Clubs, Spades }

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
struct UserId(u64);
```

```rust
// MALOS candidatos — NO derivar Copy:
//
// Tipos con datos en el heap:
//   struct Person { name: String, age: u32 }
//
// Tipos que gestionan recursos:
//   struct FileHandle { fd: RawFd }
//   Copiar un file descriptor → dos dueños del mismo recurso.
//
// Tipos donde la duplicacion implicita seria sorprendente:
//   struct DatabaseConnection { ... }
//
// Tipos grandes:
//   struct Matrix { data: [[f64; 100]; 100] }
//   80,000 bytes copiados en cada asignacion — demasiado.
```

```rust
// Pregunta clave antes de derivar Copy:
//
// "Si alguien escribe let b = a; y despues modifica b,
//  seria sorprendente que a no cambie?"
//
// NO seria sorprendente → derivar Copy (Point, Color, Direction).
// SI seria sorprendente → NO derivar (DatabaseConnection, FileHandle).
```

## Cuando derivar Clone

Clone deberia derivarse en casi todos los tipos publicos.
Permite al usuario decidir cuando crear copias:

```rust
#[derive(Debug, Clone)]
pub struct Config {
    pub host: String,
    pub port: u16,
    pub tags: Vec<String>,
}

let base = Config {
    host: String::from("localhost"),
    port: 8080,
    tags: vec![String::from("dev")],
};

let mut production = base.clone();
production.host = String::from("prod.example.com");
production.port = 443;
// base no se modifico.
```

```rust
// Implementacion manual — cuando necesitas logica custom:
use std::sync::Arc;

#[derive(Debug)]
struct SharedCache {
    data: Arc<Vec<String>>,
    local_hits: u32,
}

impl Clone for SharedCache {
    fn clone(&self) -> Self {
        SharedCache {
            data: Arc::clone(&self.data), // compartir, no copiar
            local_hits: 0,                // reiniciar contador
        }
    }
}
```

## Tipos que no pueden ser Copy

Hay categorias de tipos que **nunca** pueden implementar Copy.
El compilador lo prohibe:

```rust
// 1. Tipos con implementacion de Drop.
// Si Copy copiara bit a bit, drop() se llamaria dos veces → doble free.
struct Resource { id: u32 }

impl Drop for Resource {
    fn drop(&mut self) {
        println!("Releasing resource {}", self.id);
    }
}

// impl Copy for Resource {} // ERROR:
// the trait `Copy` cannot be implemented for this type;
// the type has a destructor
```

```rust
// 2. String, Vec<T>, Box<T> — poseen memoria en el heap.
let s1 = String::from("hello");
// s1 en el stack: { ptr: 0x..., len: 5, cap: 5 }
// s1 en el heap:  [h, e, l, l, o]
//
// Copia bitwise → s2 tendria el MISMO ptr que s1
// Ambos pensarian ser dueños → doble free al salir de scope.

// 3. HashMap, HashSet, BTreeMap — datos en el heap. Ninguno es Copy.

// 4. &mut T — referencias mutables NO son Copy.
// Copiar &mut violaria la exclusividad.
let mut x = 42;
let r = &mut x;
// let r2 = r; // MOVE, no copy.
// &T es Copy (multiples lectores OK).
// &mut T NO es Copy (solo un escritor).
```

```rust
// 5. Cualquier struct/enum con un campo no-Copy:
#[derive(Debug, Clone)]
struct Entry {
    key: String,   // no-Copy → Entry tampoco puede ser Copy
    value: i32,
}

// 6. Closures que capturan por valor tipos no-Copy:
let name = String::from("Alice");
let greet = move || println!("Hello, {name}");
// El closure NO es Copy porque String no es Copy.
```

## Quitar Copy es un breaking change

Una vez que un tipo publico implementa Copy, eliminarlo
rompe el codigo downstream:

```rust
// Version 1.0:
#[derive(Debug, Copy, Clone)]
pub struct Dimensions { pub width: u32, pub height: u32 }

// Codigo de un usuario:
fn layout(dims: Dimensions) -> String {
    format!("{}x{}", dims.width, dims.height)
}

let d = Dimensions { width: 800, height: 600 };
println!("{}", layout(d));
println!("{:?}", d); // funciona porque Dimensions es Copy
```

```rust
// Version 2.0 — agregas un campo String:
#[derive(Debug, Clone)]  // ya no puede ser Copy
pub struct Dimensions {
    pub width: u32,
    pub height: u32,
    pub label: String,   // nuevo campo no-Copy
}

// El codigo del usuario DEJA DE COMPILAR:
// println!("{:?}", d);  // ERROR: value used after move
```

```rust
// Recomendaciones:
//
// 1. Solo derivar Copy si el tipo permanecera stack-only para siempre.
// 2. Point, Color, Direction, Id numerico → seguros para Copy.
// 3. Config, Options, Metadata → mejor solo Clone.
// 4. Si dudas, no derives Copy. Agregarlo despues NO rompe nada.
//    Quitarlo SI rompe.
```

## Clone::clone_from

El trait Clone define un segundo metodo, `clone_from`, que puede
reutilizar la memoria existente en vez de asignar nueva:

```rust
// Firma de Clone:
// pub trait Clone {
//     fn clone(&self) -> Self;
//     fn clone_from(&mut self, source: &Self) { ... }
// }
//
// La implementacion por defecto de clone_from simplemente
// clona y asigna. Pero tipos como String y Vec la optimizan.
```

```rust
// clone() vs clone_from() con String:
let source = String::from("hello world");

let mut target = String::with_capacity(100);
target.push_str("old data");

// Con clone() — desperdicia la capacidad existente:
// target = source.clone();
// Drop del target viejo (libera 100 bytes) + asigna nueva memoria.

// Con clone_from() — reutiliza la capacidad existente:
target.clone_from(&source);
// "hello world" cabe en el buffer de 100 bytes.
// 0 asignaciones de memoria.
```

```rust
// Patron comun: reutilizar buffers en loops.
fn process_lines(lines: &[String]) {
    let mut buffer = String::new();

    for line in lines {
        // MAL: asigna memoria nueva en cada iteracion
        // buffer = line.clone();

        // BIEN: reutiliza la memoria del buffer
        buffer.clone_from(line);
        println!("Processing: {buffer}");
    }
}
// El buffer crece una vez y se reutiliza en cada iteracion.
```

```rust
// Implementar clone_from manualmente:
#[derive(Debug)]
struct DataStore {
    items: Vec<String>,
    metadata: String,
}

impl Clone for DataStore {
    fn clone(&self) -> Self {
        DataStore {
            items: self.items.clone(),
            metadata: self.metadata.clone(),
        }
    }

    fn clone_from(&mut self, source: &Self) {
        self.items.clone_from(&source.items);
        self.metadata.clone_from(&source.metadata);
        // Reutiliza las capacidades existentes de ambos campos.
    }
}
```

## Resumen de decisiones

```rust
// Guia rapida:
//
// Tipo                        │ Copy │ Clone │ Razon
// ────────────────────────────┼──────┼───────┼──────────────────────
// Point { x: f64, y: f64 }   │  si  │  si   │ stack-only, pequeno
// Color { r: u8, ... }       │  si  │  si   │ stack-only, 4 bytes
// enum Direction              │  si  │  si   │ sin datos en heap
// UserId(u64)                 │  si  │  si   │ wrapper de primitivo
// Config { host: String, .. } │  no  │  si   │ tiene String (heap)
// Vec<T>                      │  no  │  si   │ datos en heap
// DbConnection                │  no  │  no*  │ recurso no clonable
//
// * O Clone manual con logica custom si tiene sentido.
```

```rust
// Checklist antes de derivar Copy:
//
// 1. Todos los campos son Copy?           No → no puedes.
// 2. El tipo implementa Drop?             Si → no puedes.
// 3. Podria necesitar campos no-Copy?      Si → no derives Copy.
// 4. Copia implicita seria sorprendente?   Si → no derives Copy.
// 5. El tipo es grande (>= cientos bytes)? Si → mejor no.
//
// Si pasas todas → #[derive(Copy, Clone)]
```

---

## Ejercicios

### Ejercicio 1 — Identificar Copy y Clone

```rust
// Para cada tipo, indicar si puede derivar Copy, Clone, ambos, o ninguno.
// Justificar. Verificar compilando.
//
// 1. struct Pair(i32, i32);
// 2. struct Wrapper(String);
// 3. enum Status { Active, Inactive, Suspended }
// 4. struct Mixed { id: u64, label: String, score: f32 }
// 5. struct Matrix { data: [[f64; 4]; 4] }
//
// Para cada uno:
//   a) Intentar #[derive(Copy, Clone)] — compila?
//   b) Intentar #[derive(Clone)] — compila?
//   c) Escribir un main que demuestre el comportamiento
//      (asignar y usar el original despues).
```

### Ejercicio 2 — Copy vs Move en practica

```rust
// Crear dos structs:
//   - Pixel { r: u8, g: u8, b: u8 } con #[derive(Copy, Clone)]
//   - Image { pixels: Vec<Pixel>, width: u32, height: u32 } con #[derive(Clone)]
//
// Escribir funciones:
//   fn brighten_pixel(p: Pixel) -> Pixel   // recibe por valor (copy)
//   fn brighten_image(img: Image) -> Image // recibe por valor (move)
//
// En main:
//   1. Crear un Pixel y pasarlo a brighten_pixel dos veces
//      (posible porque Pixel es Copy).
//   2. Crear una Image y pasarla a brighten_image.
//      Intentar usarla despues → observar el error.
//   3. Usar .clone() para poder pasar Image dos veces.
//   4. Refactorizar brighten_image para recibir &Image → evitar clone.
```

### Ejercicio 3 — clone_from en un loop

```rust
// Crear un Vec<String> con 1000 strings de longitud variable.
// Implementar dos versiones que copian cada string a un buffer:
//
// Version A:  buf = s.clone();       // asigna en cada iteracion
// Version B:  buf.clone_from(s);     // reutiliza el buffer
//
// Medir con std::time::Instant cual es mas rapida.
// Explicar por que clone_from es mas eficiente.
```
