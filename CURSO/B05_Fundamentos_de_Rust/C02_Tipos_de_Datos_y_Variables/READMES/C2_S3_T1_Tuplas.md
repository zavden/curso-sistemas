# T01 — Tuplas

## Que es una tupla

Una tupla es una coleccion de tamano fijo y ordenada que puede contener
valores de tipos diferentes. Una vez creada, su longitud no puede
cambiar. Es el tipo compuesto mas simple de Rust.

```rust
// Tupla con dos elementos del mismo tipo:
let point: (f64, f64) = (1.0, 2.0);

// Tupla con elementos de tipos distintos:
let mixed = (42, "hello", true);
// El compilador infiere el tipo: (i32, &str, bool)
```

```rust
// El tipo de una tupla es la tupla de los tipos de sus elementos.
// Dos tuplas son del mismo tipo SOLO si tienen la misma cantidad
// de elementos Y cada posicion tiene el mismo tipo:
//
//   (i32, f64)  !=  (f64, i32)       — mismo contenido, distinto orden
//   (i32, i32)  !=  (i32, i32, i32)  — distinta cantidad
```

## Anotacion de tipo

Cada elemento tiene su propio tipo. La anotacion de tipo de
la tupla es la tupla de los tipos:

```rust
// Anotacion explicita:
let t: (i32, f64, &str) = (1, 2.0, "hi");

// Sin anotacion, el compilador infiere:
let inferred = (1, 2.0, "hi");
// Tipo inferido: (i32, f64, &str)
// i32 es el tipo entero por defecto, f64 el flotante por defecto.

// Si necesitas un tipo especifico, hay que anotarlo:
let specific: (u8, f32, String) = (1, 2.0, String::from("hi"));
```

## Acceso por indice

Se accede a los elementos con la notacion de punto seguida de un
literal numerico. No se usan corchetes como en arrays:

```rust
let point: (f64, f64) = (1.0, 2.0);

let x = point.0;    // 1.0
let y = point.1;    // 2.0

let mixed = (42, "hello", true);
let number = mixed.0;    // 42
let text   = mixed.1;    // "hello"
let flag   = mixed.2;    // true
```

```rust
// El indice DEBE ser un literal numerico.
// No se puede usar una variable como indice:

let index = 0;
// let value = point.index;    // ERROR: no field `index` on type `(f64, f64)`

// Cada posicion puede tener un tipo distinto, asi que el compilador
// necesita saber EN COMPILACION cual posicion se accede
// para determinar el tipo del resultado.

// Acceso fuera de rango es un error de compilacion, no de ejecucion:
let pair = (10, 20);
// let bad = pair.2;    // ERROR: no field `2` on type `(i32, i32)`
```

## Destructuring

Destructuring permite extraer todos los elementos de una tupla
en variables individuales con un solo `let`:

```rust
let point = (1.0, 2.0);
let (x, y) = point;

println!("x = {x}, y = {y}");
// x = 1.0, y = 2.0
```

```rust
// Se puede usar _ para ignorar elementos que no interesan:

let mixed = (42, "hello", true);

let (first, _, third) = mixed;
// first = 42, third = true — el segundo se descarta

let (_, second, _) = mixed;
// second = "hello"
```

```rust
// Destructuring en parametros de funciones:

fn print_point((x, y): (f64, f64)) {
    println!("({x}, {y})");
}

fn main() {
    let point = (3.0, 4.0);
    print_point(point);
    // (3.0, 4.0)
}
```

```rust
// Destructuring en un for loop con enumerate:

let names = ["Alice", "Bob", "Charlie"];

for (index, name) in names.iter().enumerate() {
    println!("{index}: {name}");
}
// 0: Alice
// 1: Bob
// 2: Charlie
// enumerate() retorna tuplas (usize, &T).
```

## Tupla como retorno de funcion

Las tuplas son el mecanismo natural en Rust para retornar
multiples valores desde una funcion:

```rust
fn min_max(v: &[i32]) -> (i32, i32) {
    let mut min = v[0];
    let mut max = v[0];

    for &val in &v[1..] {
        if val < min { min = val; }
        if val > max { max = val; }
    }

    (min, max)
}

fn main() {
    let numbers = [3, 7, 1, 9, 4];
    let (min, max) = min_max(&numbers);
    println!("min = {min}, max = {max}");
    // min = 1, max = 9
}
```

```rust
// La biblioteca estandar usa tuplas de retorno en varios lugares:

let s = String::from("hello world");

// str::split_once retorna Option<(&str, &str)>:
if let Some((left, right)) = s.split_once(' ') {
    println!("left = '{left}', right = '{right}'");
}
// left = 'hello', right = 'world'
```

## Tupla unitaria — ()

La tupla vacia `()` es el tipo unit. Es el tipo que tienen las
expresiones que no retornan un valor significativo:

```rust
let unit: () = ();
// Es el unico valor posible del tipo ().
// Ocupa cero bytes en memoria.

// Funciones que "no retornan nada" en realidad retornan ():
fn greet(name: &str) {
    println!("Hello, {name}!");
}

// La firma completa equivalente es:
fn greet_explicit(name: &str) -> () {
    println!("Hello, {name}!");
}
// Ambas son identicas para el compilador.
```

El tipo unit se cubre en detalle en
`S02_Variables_y_Mutabilidad/T04_Tipo_unit`.

## Tupla de un solo elemento

Para crear una tupla de un solo elemento se necesita una coma
despues del valor. Sin la coma, los parentesis se interpretan
como una expresion agrupada:

```rust
// Esto es una tupla de un elemento:
let single = (42,);
// Tipo: (i32,)

// Esto NO es una tupla — es el entero 42 entre parentesis:
let not_a_tuple = (42);
// Tipo: i32

println!("{}", single.0);    // 42 — acceso por indice funciona
// println!("{}", not_a_tuple.0);  // ERROR: no field `0` on type `{integer}`

// La coma es necesaria tambien en anotaciones de tipo:
let x: (i32,) = (42,);     // tupla de un elemento
let y: i32    = 42;         // entero

// En la practica, las tuplas de un elemento son poco comunes.
// Si solo necesitas un valor, usa el tipo directamente.
```

## Tuplas y traits

Las tuplas implementan varios traits de la biblioteca estandar,
pero solo hasta un maximo de 12 elementos:

```rust
// Debug — permite imprimir con {:?}
let point = (1, 2, 3);
println!("{:?}", point);
// (1, 2, 3)

// Clone y Copy (si todos los elementos los implementan):
let a = (1, 2.0, true);
let b = a;         // Copy: a sigue siendo valido
let c = a.clone(); // Clone: copia explicita
```

```rust
// PartialEq / Eq — comparacion de igualdad:
let t1 = (1, "hello");
let t2 = (1, "hello");
assert!(t1 == t2);
// La comparacion es elemento a elemento, en orden.

// PartialOrd / Ord — comparacion de orden (lexicografica):
let a = (1, 2);
let b = (1, 3);
let c = (2, 0);

assert!(a < b);    // primer elemento igual, segundo 2 < 3
assert!(b < c);    // primer elemento 1 < 2, segundo no importa
// Se compara el primer elemento. Si es igual, el segundo. Y asi.
```

```rust
// Default — valor por defecto (si todos los elementos lo implementan):
let defaults: (i32, f64, bool) = Default::default();
println!("{:?}", defaults);
// (0, 0.0, false)

// Hash — se puede usar como clave en HashMap:
use std::collections::HashMap;
let mut grid: HashMap<(u8, u8), &str> = HashMap::new();
grid.insert((1, 1), "top-left");
```

```rust
// Limite de 12 elementos:
// Las tuplas de 13+ elementos NO implementan Debug, Clone, etc.
// Esto es una limitacion tecnica: Rust no tiene generics variadicos,
// asi que cada implementacion se escribe para cada aridad (de 0 a 12).

let twelve = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
println!("{:?}", twelve);    // funciona

// let thirteen = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13);
// println!("{:?}", thirteen);  // ERROR: doesn't implement Debug

// En la practica, si necesitas mas de 12 campos, usa un struct.
```

## Tuplas anidadas

Las tuplas pueden contener otras tuplas como elementos:

```rust
let nested = ((1, 2), (3, 4));

// Acceso encadenado:
let x = nested.0.0;    // 1
let y = nested.0.1;    // 2
let z = nested.1.0;    // 3

// Destructuring anidado:
let ((a, b), (c, d)) = nested;
println!("{a} {b} {c} {d}");
// 1 2 3 4
```

```rust
// Caso practico: segmento de linea como par de puntos.

let segment: ((f64, f64), (f64, f64)) = ((0.0, 0.0), (3.0, 4.0));

let ((x1, y1), (x2, y2)) = segment;
let length = ((x2 - x1).powi(2) + (y2 - y1).powi(2)).sqrt();
println!("length = {length}");
// length = 5

// Si este patron se repite, es mejor usar un struct Point { x, y }.
```

## Tuple structs vs tuplas

Rust tiene un tipo intermedio entre tuplas y structs llamado
tuple struct. Se parece a una tupla pero tiene un nombre propio:

```rust
// Tupla anonima:
let color = (255, 128, 0);

// Tuple struct con nombre:
struct Color(u8, u8, u8);
let color = Color(255, 128, 0);

// El acceso es identico:
let red = color.0;
let green = color.1;
```

```rust
// La diferencia clave: el nombre crea un tipo distinto.
struct Celsius(f64);
struct Fahrenheit(f64);

// No se pueden mezclar por error:
// let bad = Celsius(100.0).0 + Fahrenheit(212.0);  // ERROR de tipo

// Con tuplas simples (f64,) y (f64,) serian el mismo tipo.
// Los tuple structs dan seguridad de tipos sin overhead en runtime.
```

Los tuple structs se cubren en detalle en
`C05_Structs_Enums_y_Traits/S01_Structs/T02_Tuple_structs_y_unit_structs`.

## Cuando usar tuplas vs structs

```rust
// TUPLA — agrupacion ad-hoc, temporal, local:

// Retorno de funcion con pocos valores relacionados:
fn parse_header(line: &str) -> (&str, &str) {
    let (key, value) = line.split_once(':').unwrap();
    (key.trim(), value.trim())
}

// Iteracion con enumerate:
for (i, val) in vec.iter().enumerate() { /* ... */ }

// Swap de variables:
let (a, b) = (b, a);
```

```rust
// STRUCT — datos con nombre y significado semantico:

struct Point { x: f64, y: f64 }

// Cuando los campos necesitan nombres para ser comprensibles.
// Cuando el tipo se usa en multiples partes del codigo.
// Cuando necesitas implementar metodos o traits propios.
```

```rust
// Guia practica:
//
// Usar tupla cuando:
//   - Son 2-3 elementos cuyo significado es obvio por contexto
//   - El tipo se usa localmente (dentro de una funcion)
//   - Es un valor de retorno efimero
//
// Usar struct cuando:
//   - Los campos necesitan nombres para ser comprensibles
//   - El tipo se usa en multiples partes del codigo
//   - Necesitas implementar metodos o traits propios
//   - Hay mas de 3 campos
//
// Regla general: si dudas, usa un struct.
// El costo de definir un struct es minimo y la claridad que aporta
// es significativa. Los nombres de campo sirven como documentacion.
```

---

## Ejercicios

### Ejercicio 1 — Acceso y destructuring

```rust
// Crear una tupla con los datos de un libro:
//   titulo: &str, paginas: u32, precio: f64, disponible: bool
//
// 1. Acceder a cada campo con notacion de punto (.0, .1, .2, .3)
//    e imprimir cada valor.
//
// 2. Usar destructuring para extraer los cuatro campos en variables
//    con nombre (titulo, paginas, precio, disponible) e imprimirlos.
//
// 3. Usar destructuring con _ para extraer SOLO titulo y precio,
//    ignorando los demas campos.
//
// Verificar que compila y que los valores impresos son correctos.
```

### Ejercicio 2 — Funcion con retorno de tupla

```rust
// Escribir una funcion stats(data: &[f64]) -> (f64, f64, f64) que
// reciba un slice de f64 y retorne una tupla con:
//   - el valor minimo
//   - el valor maximo
//   - el promedio
//
// En main:
//   1. Llamar a stats con un array de al menos 5 elementos.
//   2. Destructurar el resultado en (min, max, avg).
//   3. Imprimir los tres valores.
//
// Nota: asumir que el slice nunca esta vacio.
```

### Ejercicio 3 — Tuplas anidadas y comparacion

```rust
// Crear dos segmentos de linea como tuplas anidadas:
//   let seg1: ((f64, f64), (f64, f64)) = ((0.0, 0.0), (3.0, 4.0));
//   let seg2: ((f64, f64), (f64, f64)) = ((1.0, 1.0), (4.0, 5.0));
//
// 1. Escribir una funcion length(seg: ((f64, f64), (f64, f64))) -> f64
//    que calcule la longitud del segmento usando la formula de distancia.
//    Usar destructuring en los parametros o en el cuerpo de la funcion.
//
// 2. Comparar ambos segmentos: imprimir cual es mas largo.
//
// 3. Crear un Vec de 4 segmentos y usar .sort_by() con length()
//    para ordenarlos de menor a mayor longitud. Imprimir el resultado.
//
// Pista: sort_by recibe un closure que compara con partial_cmp.
```
