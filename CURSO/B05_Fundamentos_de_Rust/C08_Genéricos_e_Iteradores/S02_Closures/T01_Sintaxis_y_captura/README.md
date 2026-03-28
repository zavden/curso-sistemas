# T01 — Sintaxis y captura

## Que es una closure

Una closure es una funcion anonima que puede **capturar variables
del entorno** donde se define. A diferencia de una funcion normal
(`fn`), una closure "ve" las variables locales que la rodean:

```rust
// Funcion normal — NO puede acceder a variables externas:
fn add_one(x: i32) -> i32 {
    // y no existe aqui — las funciones solo ven sus parametros
    x + 1
}

// Closure — SI puede capturar variables del entorno:
let y = 10;
let add_y = |x| x + y;  // captura y del entorno
//           ^     ^
//           |     └── usa y (capturada)
//           └── parametro de la closure
assert_eq!(add_y(5), 15);
```

## Sintaxis

```rust
// Forma completa:
let add = |a: i32, b: i32| -> i32 { a + b };

// Forma minima — tipos y llaves se infieren:
let add = |a, b| a + b;

// Sin parametros:
let greet = || println!("hello");

// Con bloque de multiples lineas:
let process = |x: i32| {
    let doubled = x * 2;
    let result = doubled + 1;
    result  // ultima expresion = valor de retorno
};
```

```text
Comparacion de sintaxis:

Funcion:   fn add(a: i32, b: i32) -> i32 { a + b }
Closure:   let add = |a: i32, b: i32| -> i32 { a + b };
Minima:    let add = |a, b| a + b;

Diferencias:
- Closure usa || en vez de () para parametros
- Tipos se infieren (no obligatorio anotarlos)
- Si el cuerpo es una sola expresion, no necesita {} ni return
- Se asigna a una variable (es un valor, no una declaracion)
```

```rust
// La inferencia de tipos se fija en el primer uso:
let identity = |x| x;

let s = identity("hello");  // Rust infiere: closure toma &str, retorna &str
// let n = identity(42);     // error: expected &str, found integer
// Una vez inferido, el tipo queda fijo.
```

## Captura del entorno

Las closures capturan variables del scope donde se definen.
Rust elige automaticamente la forma **menos restrictiva** de captura:

```rust
let name = String::from("Alice");
let age = 30;

// Captura por referencia compartida (&T) — solo lectura:
let greet = || println!("Hello, {name}! Age: {age}");
greet();
println!("{name}");  // name sigue disponible — solo se presto

// Captura por referencia mutable (&mut T) — modifica:
let mut count = 0;
let mut increment = || {
    count += 1;  // necesita &mut count
};
increment();
increment();
println!("{count}");  // 2

// Captura por valor (move/copy) — toma ownership:
let name = String::from("Alice");
let consume = || {
    let _s = name;  // mueve name dentro de la closure
    // name ya no es accesible fuera
};
consume();
// println!("{name}");  // error: name fue movido
```

### Como decide Rust el modo de captura

```text
Rust analiza el CUERPO de la closure y elige la captura minima:

1. &T  (referencia compartida) — si la variable solo se LEE
2. &mut T (referencia mutable)  — si la variable se MODIFICA
3. T     (por valor / move)     — si la variable se CONSUME
                                   (ej: se pasa a una funcion que toma ownership)

Rust SIEMPRE elige la opcion menos restrictiva posible.
Si solo lees, captura por &T. Si modificas, &mut T. Si mueves, T.
```

```rust
let s = String::from("hello");

// Solo lectura → captura &s:
let len = || s.len();    // .len() toma &self → solo necesita &s
println!("{}", len());   // 5
println!("{s}");         // s sigue viva

// Modificacion → captura &mut s:
let mut s = String::from("hello");
let mut append = || s.push_str(" world");  // .push_str toma &mut self
append();
println!("{s}");  // "hello world"

// Consumo → captura s (move):
let s = String::from("hello");
let consume = || drop(s);  // drop() toma ownership → mueve s
consume();
// println!("{s}");  // error: s fue movido dentro de la closure
```

```rust
// Con tipos Copy, la "captura por valor" copia, no mueve:
let x = 42;  // i32 es Copy
let add = || x + 1;  // captura x por valor (copia)
println!("{}", add());  // 43
println!("{x}");        // 42 — x sigue disponible (se copio)

// Esto es porque:
// - Para tipos Copy (i32, f64, bool, char, &T): la captura por valor COPIA
// - Para tipos non-Copy (String, Vec, Box): la captura por valor MUEVE
```

### Captura parcial de structs

```rust
struct Person {
    name: String,
    age: u32,
}

let p = Person { name: String::from("Alice"), age: 30 };

// Rust captura cada campo por separado (captura parcial):
let name_ref = || println!("{}", p.name);  // captura &p.name
// p.age sigue accesible porque no fue capturado (o solo &p.age)
println!("age: {}", p.age);  // OK

// Pero si mueves un campo:
let take_name = || {
    let _n = p.name;  // mueve p.name
};
take_name();
// println!("{}", p.name);  // error: p.name fue movido
println!("{}", p.age);      // OK — age no fue movido
```

```text
Captura parcial (desde Rust 2021 edition):
- Rust captura solo los campos que la closure usa
- Cada campo puede tener su propio modo de captura
- Esto permite que otros campos sigan accesibles
- En edition 2015/2018, se capturaba el struct completo
```

## Closures como valores

Las closures son valores — puedes almacenarlas en variables,
pasarlas como argumentos, o retornarlas:

```rust
// En una variable:
let double = |x: i32| x * 2;
let result = double(5);  // 10

// En un Vec (todas del mismo tipo — mismo cuerpo y capturas):
// Nota: cada closure tiene un tipo UNICO, asi que no puedes
// mezclar closures distintas en un Vec<???> directamente.
// Necesitas trait objects (dyn Fn) o Box — tema posterior.

// Como argumento (la forma mas comun):
let numbers = vec![1, 2, 3, 4, 5];
let doubled: Vec<i32> = numbers.iter().map(|x| x * 2).collect();
assert_eq!(doubled, vec![2, 4, 6, 8, 10]);

let evens: Vec<&i32> = numbers.iter().filter(|x| **x % 2 == 0).collect();
assert_eq!(evens, vec![&2, &4]);
```

```rust
// Closures con la stdlib:
let mut v = vec![5, 3, 1, 4, 2];

// sort_by — closure como comparador
v.sort_by(|a, b| a.cmp(b));
assert_eq!(v, vec![1, 2, 3, 4, 5]);

// retain — closure como filtro
let mut v = vec![1, 2, 3, 4, 5, 6];
v.retain(|&x| x % 2 == 0);
assert_eq!(v, vec![2, 4, 6]);

// map, filter, for_each — operaciones sobre iteradores
vec![1, 2, 3].iter()
    .map(|x| x * 10)
    .filter(|x| *x > 15)
    .for_each(|x| println!("{x}"));
// 20
// 30

// thread::spawn — closure como tarea
// std::thread::spawn(|| {
//     println!("en otro hilo");
// });
```

## Cada closure tiene un tipo unico

```rust
let add_one = |x: i32| x + 1;
let add_two = |x: i32| x + 2;

// add_one y add_two tienen tipos DISTINTOS, aunque la firma sea igual.
// Cada closure genera un tipo anonimo unico en compilacion.

// No puedes hacer esto:
// let closures = vec![add_one, add_two];
// error: mismatched types

// Puedes con trait objects:
let closures: Vec<Box<dyn Fn(i32) -> i32>> = vec![
    Box::new(add_one),
    Box::new(add_two),
];

for f in &closures {
    println!("{}", f(10));
}
// 11
// 12
```

```text
¿Por que cada closure tiene tipo unico?

Porque el tipo de una closure INCLUYE las variables capturadas.
Dos closures que capturan variables distintas (o ninguna) tienen
layouts de memoria distintos → tipos distintos.

Esto permite monomorphization: el compilador conoce el tipo exacto
y puede hacer inline, optimizar, etc. Sin overhead.
```

## Closures que no capturan nada

```rust
// Una closure sin capturas es basicamente una funcion:
let add = |a: i32, b: i32| a + b;

// Puede convertirse a un puntero a funcion (fn):
let add_fn: fn(i32, i32) -> i32 = add;
// Esto solo funciona si la closure NO captura nada.

let x = 10;
let add_x = |a: i32| a + x;  // captura x
// let add_fn: fn(i32) -> i32 = add_x;  // error: closure captures x
```

```text
Jerarquia:
  fn(i32) -> i32         — puntero a funcion (sin capturas)
  impl Fn(i32) -> i32    — closure que puede llamarse multiples veces
  impl FnMut(i32) -> i32 — closure que puede mutar capturas
  impl FnOnce(i32) -> i32 — closure que consume capturas (una sola vez)

Los traits Fn, FnMut, FnOnce se cubren en el siguiente tema.
```

## Errores comunes

```rust
// ERROR 1: closure que captura &mut pero no se declara mut
let mut count = 0;
// let increment = || count += 1;
// increment();  // error: cannot borrow as mutable
let mut increment = || count += 1;  // la VARIABLE de la closure debe ser mut
increment();

// ERROR 2: lifetime de capturas — closure vive mas que la referencia
// fn make_closure() -> impl Fn() {
//     let s = String::from("hello");
//     || println!("{s}")  // error: s no vive lo suficiente
// }
// La closure captura &s, pero s se destruye al salir de make_closure.
// Solucion: move closure (siguiente tema)

// ERROR 3: inferencia de tipos fijada
let f = |x| x;
let a = f(42);       // T inferido como i32
// let b = f("hello"); // error: expected i32, found &str

// ERROR 4: doble referencia en filter
let v = vec![1, 2, 3, 4];
// v.iter() produce &i32, filter recibe &&i32:
let evens: Vec<&i32> = v.iter().filter(|x| **x % 2 == 0).collect();
// Alternativa mas limpia con pattern destructuring:
let evens: Vec<&i32> = v.iter().filter(|&&x| x % 2 == 0).collect();
//                                      ^^ destructura la doble referencia

// ERROR 5: captura de loop variable
let mut closures = Vec::new();
for i in 0..3 {
    closures.push(|| println!("{i}"));
    // Cada closure captura su propia copia de i (Rust 2021)
    // En otros lenguajes (JS, Python), esto es una trampa comun
}
for c in &closures {
    c();  // 0, 1, 2 — correcto en Rust
}
```

## Cheatsheet

```text
Sintaxis:
  |x| x + 1                 un parametro, una expresion
  |x, y| x + y              multiples parametros
  || println!("hi")         sin parametros
  |x: i32| -> i32 { x + 1 } tipos explicitos, bloque

Captura (automatica):
  &T      si solo se lee la variable
  &mut T  si se modifica
  T       si se mueve/consume (Copy: copia; non-Copy: mueve)

Reglas:
  - Rust elige la captura menos restrictiva
  - Cada closure tiene un tipo unico anonimo
  - Closure sin capturas → convertible a fn pointer
  - Edition 2021: captura parcial de campos de struct
  - Variable de closure debe ser mut si captura &mut

Uso comun:
  iter.map(|x| ...)          transformar
  iter.filter(|x| ...)       filtrar
  iter.for_each(|x| ...)     ejecutar efecto
  v.sort_by(|a, b| ...)      comparar
  v.retain(|x| ...)          filtrar in-place
```

---

## Ejercicios

### Ejercicio 1 — Captura automatica

```rust
// Para cada closure, predice el modo de captura (& / &mut / move)
// y si la variable original sigue disponible despues:
//
// let s = String::from("hello");
// let a = || println!("{s}");        // a) modo? s disponible?
// let b = || s.len();                // b) modo? s disponible?
//
// let mut v = vec![1, 2, 3];
// let c = || v.push(4);             // c) modo? v disponible?
//
// let s = String::from("hello");
// let d = || { let _ = s; };        // d) modo? s disponible?
//
// let x = 42_i32;
// let e = || x + 1;                 // e) modo? x disponible?
//
// Ejecuta y verifica cada prediccion.
```

### Ejercicio 2 — Closures con iteradores

```rust
// Dado: let words = vec!["hello", "world", "rust", "is", "great"];
//
// Usando closures con metodos de iterador, produce:
// 1. Un Vec con las longitudes de cada palabra
// 2. Un Vec con solo las palabras de mas de 3 letras
// 3. La palabra mas larga (usa max_by_key)
// 4. Un String con todas las palabras unidas por " - "
//    (pista: collect o fold)
//
// Intenta resolver cada uno en una sola cadena de metodos.
```

### Ejercicio 3 — Closures como configuracion

```rust
// Escribe una funcion apply_all que reciba un Vec<i32>
// y un slice de closures, y aplique cada closure al Vec.
//
// fn apply_all(data: &mut Vec<i32>, ops: &[&dyn Fn(&mut Vec<i32>)]) {
//     for op in ops {
//         op(data);
//     }
// }
//
// Crea 3 closures:
//   - Una que multiplique cada elemento por 2
//   - Una que elimine los numeros impares
//   - Una que ordene el Vec
//
// Aplicalas a vec![5, 3, 8, 1, 4] y predice el resultado.
```
