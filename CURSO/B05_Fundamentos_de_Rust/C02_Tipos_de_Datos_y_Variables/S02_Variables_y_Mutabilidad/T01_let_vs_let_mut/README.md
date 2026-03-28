# T01 — let vs let mut

## Inmutabilidad por defecto

En Rust, toda variable declarada con `let` es **inmutable**. No se
puede cambiar su valor despues de la asignacion inicial:

```rust
fn main() {
    let x = 5;
    println!("{x}");
    // x = 6;   // Error: no se puede asignar dos veces a variable inmutable
}
```

La inmutabilidad por defecto es una decision deliberada:

```text
1. Seguridad: no hay bugs por mutacion inesperada.
2. Concurrencia: datos inmutables se comparten entre hilos sin data races.
3. Claridad de intencion: let mut dice "este valor va a cambiar".
4. Optimizaciones: el compilador optimiza mas cuando sabe que un valor no cambiara.
```

```c
// En C, la situacion es la opuesta: mutable por defecto, const es opt-in.
int x = 5;
x = 6;          // Perfectamente valido en C

const int y = 5;
// y = 6;       // Error de compilacion
// Nadie escribe const en todos lados. Es facil olvidarlo.
```

```rust
// Rust invierte la convencion:
let x = 5;      // inmutable (equivale a const en C)
let mut y = 5;  // mutable  (equivale a int y en C)
```

## let mut

Para declarar una variable mutable se usa `let mut`:

```rust
fn main() {
    let mut x = 5;
    println!("x = {x}");   // x = 5

    x = 6;
    println!("x = {x}");   // x = 6

    x = x + 1;
    println!("x = {x}");   // x = 7
}
```

```rust
// mut permite cambiar el VALOR, no el TIPO:
fn main() {
    let mut x = 5;      // x es i32
    x = 10;             // OK: mismo tipo
    // x = "hello";     // Error: expected integer, found &str
}
```

```rust
// mut se aplica al binding (la variable), no al valor.
// El compilador rastrea que variables son mutables y cuales no:
fn main() {
    let a = 1;       // inmutable
    let mut b = 2;   // mutable
    let c = 3;       // inmutable

    // a = 10;       // error[E0384]
    b = 20;          // OK
    // c = 30;       // error[E0384]
    println!("{a} {b} {c}");
}
```

## El error de compilacion

Cuando se intenta mutar una variable inmutable, el compilador
produce el error E0384:

```text
error[E0384]: cannot assign twice to immutable variable `x`
 --> src/main.rs:3:5
  |
2 |     let x = 5;
  |         - first assignment to `x`
3 |     x = 6;
  |     ^^^^^ cannot assign twice to immutable variable
  |
help: consider making this binding mutable
  |
2 |     let mut x = 5;
  |         +++
```

```bash
# El compilador muestra donde fue la primera asignacion, donde se
# intento la segunda, y sugiere la solucion exacta.

# Para ver la explicacion completa de cualquier error:
rustc --explain E0384
# Funciona para todos los codigos de error de Rust (E0XXX).
```

## Re-binding vs mutacion

Rust permite **re-declarar** una variable con el mismo nombre
usando un nuevo `let`. Esto se llama **shadowing** (o re-binding).
Es fundamentalmente distinto de la mutacion:

```rust
// Re-binding (shadowing): crea una NUEVA variable
fn main() {
    let x = 5;
    let x = x + 1;   // nueva variable que "oculta" la anterior
    let x = x * 2;   // otra nueva variable
    println!("{x}");  // 12
}
```

```rust
// Mutacion: modifica la MISMA variable
fn main() {
    let mut x = 5;
    x = x + 1;       // modifica el mismo x
    x = x * 2;       // modifica el mismo x
    println!("{x}");  // 12
}
```

```rust
// Diferencia 1: shadowing puede cambiar el TIPO.
fn main() {
    let spaces = "   ";         // &str (3 espacios)
    let spaces = spaces.len();  // usize (el numero 3)
    println!("{spaces}");       // 3

    // Con mut NO se puede:
    // let mut spaces = "   ";
    // spaces = spaces.len();   // Error: tipos incompatibles
}
```

```rust
// Diferencia 2: despues de shadowing, el re-binding es inmutable
// (a menos que el nuevo let tambien sea mut).
fn main() {
    let x = 5;
    let x = x + 1;   // x ahora es 6, e INMUTABLE
    // x = 7;         // Error: x no es mut

    let mut y = 5;
    y = y + 1;        // y ahora es 6, y sigue siendo MUTABLE
    y = 7;            // OK
    println!("{x} {y}");
}
```

```rust
// Diferencia 3: shadowing crea una nueva variable en memoria.
// La anterior sigue existiendo si algo la referencia.
fn main() {
    let x = 5;
    let r = &x;       // r referencia al primer x
    let x = 10;       // nuevo x, el primero sigue vivo por r
    println!("{r}");   // 5 — r apunta al primer x
    println!("{x}");   // 10
}
```

```text
Resumen:
  Shadowing (let x = ...): nueva variable, puede cambiar tipo, inmutable por defecto.
  Mutacion (x = ...): misma variable, mismo tipo, requiere let mut.
```

## Mutabilidad de referencias

La mutabilidad de una variable determina que tipo de referencias
se pueden tomar de ella:

```rust
fn main() {
    let x = 5;
    let r = &x;          // OK: referencia inmutable a variable inmutable
    println!("{r}");
    // let r2 = &mut x;  // Error: no se puede tomar &mut de variable inmutable

    let mut y = 10;
    let r3 = &mut y;     // OK: referencia mutable a variable mutable
    *r3 = 20;            // modifica y a traves de la referencia
    println!("{r3}");    // 20
}
```

```rust
// Reglas de referencias (se veran en detalle en ownership):
// 1. Para tomar &mut x, la variable x DEBE ser let mut.
// 2. Se puede tener multiples &x O exactamente un &mut x, pero no ambos.

fn main() {
    let mut x = 5;
    let r1 = &x;
    let r2 = &x;         // OK: multiples & inmutables
    println!("{r1} {r2}");

    let r3 = &mut x;     // OK: r1 y r2 ya no se usan despues
    *r3 = 20;
    println!("{r3}");
}
```

```c
// En C, los punteros no tienen restricciones de mutabilidad.
int x = 5;
int *p1 = &x;
int *p2 = &x;
*p1 = 10;   // modifica x
*p2 = 20;   // tambien — el compilador no dice nada
// Si fuera multithreaded, seria un data race silencioso.
```

## Mutabilidad y structs

La mutabilidad se aplica al **binding completo**. Si un struct
es `let`, **todos** sus campos son inmutables. Si es `let mut`,
**todos** son mutables. Rust no tiene mutabilidad por campo:

```rust
struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let p = Point { x: 1, y: 2 };
    // p.x = 10;   // Error: p no es mut

    let mut q = Point { x: 1, y: 2 };
    q.x = 10;      // OK: q es mut, todos los campos son mutables
    q.y = 20;      // OK
    println!("({}, {})", q.x, q.y);
}
```

```rust
// Si se necesita mutabilidad selectiva (un campo mutable dentro
// de un struct inmutable), existe el patron de "interior mutability"
// con tipos como Cell o RefCell:

use std::cell::Cell;

struct Config {
    name: String,               // inmutable desde fuera
    access_count: Cell<u32>,    // mutable incluso con &Config
}

fn main() {
    let config = Config {
        name: String::from("app"),
        access_count: Cell::new(0),
    };

    // config.name = String::from("other");  // Error: config no es mut
    config.access_count.set(1);              // OK: Cell permite mutacion interior
    println!("{}", config.access_count.get());
}

// Interior mutability es un tema avanzado que se cubrira mas adelante.
```

## Pattern matching con let

`let` en Rust es una operacion de **pattern matching**. La palabra
`mut` se puede aplicar a bindings individuales dentro del patron:

```rust
fn main() {
    // Desestructurar una tupla:
    let (x, y) = (1, 2);
    println!("{x} {y}");  // 1 2
    // Ambos son inmutables.

    // mut en bindings individuales:
    let (mut a, b) = (1, 2);
    a = 10;     // OK: a es mut
    // b = 20;  // Error: b no es mut
    println!("{a} {b}");  // 10 2
}
```

```rust
// Con structs:
struct Point { x: i32, y: i32 }

fn main() {
    let p = Point { x: 1, y: 2 };
    let Point { x: mut a, y: b } = p;
    a = 100;    // OK: a es mut
    // b = 200; // Error: b no es mut
    println!("{a} {b}");  // 100 2

    // Util para funciones que retornan tuplas:
    let (mut q, r) = (17 / 5, 17 % 5);
    q = q + 1;
    println!("{q} {r}");
}
```

## Cuando usar mut

La regla general: **empezar sin mut**. Agregar `mut` solo cuando
el compilador lo pida o cuando la logica lo requiera:

```rust
// El compilador avisa si falta mut:
fn main() {
    let x = 5;
    // x = 6;
    // error[E0384]: cannot assign twice to immutable variable
    // help: consider making this binding mutable: `let mut x`
}
```

```rust
// El compilador TAMBIEN avisa si mut es innecesario:
fn main() {
    let mut x = 5;  // warning: variable does not need to be mutable
    println!("{x}");
}
```

```text
warning: variable does not need to be mutable
 --> src/main.rs:2:9
  |
2 |     let mut x = 5;
  |         ----^
  |         |
  |         help: remove this `mut`
  |
  = note: `#[warn(unused_mut)]` on by default
```

```rust
// Flujo de trabajo correcto:
// 1. Declarar todo como let (inmutable).
// 2. Escribir la logica.
// 3. Si el compilador dice que necesitas mut, agregarlo.
// 4. Si el compilador dice que mut no se usa, quitarlo.

// Casos tipicos donde mut es necesario:
// contadores, acumuladores, buffers, variables de estado en loops.

fn sum_list(numbers: &[i32]) -> i32 {
    let mut total = 0;    // mut necesario: total cambia en cada iteracion
    for n in numbers {
        total += n;
    }
    total
}

fn build_greeting(name: &str, formal: bool) -> String {
    let mut greeting = String::new();  // mut: se va construyendo
    if formal {
        greeting.push_str("Estimado/a ");
    } else {
        greeting.push_str("Hola ");
    }
    greeting.push_str(name);
    greeting.push('!');
    greeting
}

fn main() {
    println!("sum = {}", sum_list(&[1, 2, 3, 4, 5]));
    println!("{}", build_greeting("Ana", true));
}
```

## Parametros de funcion

Los parametros se pueden declarar con `mut`. Esto no afecta la
firma de la funcion (el caller no lo ve), solo permite mutar la
**copia local** dentro de la funcion:

```rust
// Sin mut: el parametro es inmutable dentro de la funcion
fn print_double(x: i32) {
    // x = x * 2;  // Error: x no es mut
    println!("{}", x * 2);
}

// Con mut: se puede mutar la copia local
fn print_double_v2(mut x: i32) {
    x = x * 2;     // OK: muta la copia local
    println!("{x}");
}

fn main() {
    let val = 5;
    print_double(val);     // 10
    print_double_v2(val);  // 10
    println!("{val}");     // 5 — val no cambio en ninguno de los dos casos
}
```

```rust
// Esto es diferente de recibir una referencia mutable:

fn increment_copy(mut x: i32) {
    x += 1;           // muta la copia local, no afecta al caller
    println!("{x}");
}

fn increment_ref(x: &mut i32) {
    *x += 1;          // muta el dato ORIGINAL del caller
}

fn main() {
    let mut val = 5;

    increment_copy(val);
    println!("{val}");    // 5 — no cambio

    increment_ref(&mut val);
    println!("{val}");    // 6 — cambio
}
```

```rust
// Uso practico: modificar un parametro como variable de trabajo.

fn factorial(mut n: u64) -> u64 {
    let mut result = 1;
    while n > 1 {
        result *= n;
        n -= 1;       // muta la copia local de n
    }
    result
}

fn main() {
    let x = 5;
    println!("{x}! = {}", factorial(x));  // 5! = 120
    // x sigue siendo 5.

    // Sin mut en el parametro, habria que crear una variable local:
    // let mut current = n; ... current -= 1;
    // Ambas formas son equivalentes.
}
```

---

## Ejercicios

### Ejercicio 1 --- Inmutabilidad y errores del compilador

```rust
// Sin ejecutar, predecir que lineas produciran error de compilacion.
// Luego compilar y verificar las predicciones.

fn main() {
    let a = 10;
    let mut b = 20;
    let c = 30;

    a = 11;         // linea 1
    b = 21;         // linea 2
    b = b + c;      // linea 3
    c = c + a;      // linea 4
    let c = c + a;  // linea 5

    println!("{a} {b} {c}");
}

// Preguntas:
//   - Cuales lineas dan error E0384?
//   - La linea 5 compila? Por que si o por que no?
//   - Si se comentan las lineas con error, cual es la salida?
```

### Ejercicio 2 --- Shadowing vs mutacion

```rust
// Analizar que imprime cada bloque. Predecir antes de ejecutar.

fn main() {
    // Bloque A: shadowing con cambio de tipo
    let x = "hello";
    let x = x.len();
    let x = x * 2;
    println!("A: {x}");

    // Bloque B: shadowing en scope interno
    let y = 5;
    {
        let y = y + 10;
        println!("B inner: {y}");
    }
    println!("B outer: {y}");

    // Bloque C: mut vs shadowing
    let mut z = 1;
    z = z + 1;
    let z = z + 1;
    // z = z + 1;   // esta linea compila? por que?
    println!("C: {z}");
}
```

### Ejercicio 3 --- Parametros mutables

```rust
// Implementar las siguientes funciones:
//
// 1. count_down(mut n: u32) — imprime los numeros de n a 1,
//    uno por linea, usando el parametro mutable como contador.
//
// 2. truncate_to_length(s: &str, max_len: usize) -> String
//    — retorna los primeros max_len caracteres del string.
//    Pensar: se necesita mut en algun parametro? Por que?
//
// 3. swap_and_sum(mut a: i32, mut b: i32) -> i32
//    — intercambia a y b, luego retorna a + b.
//    El resultado deberia ser el mismo sin intercambiar.
//    Reflexion: tiene sentido usar mut aqui?

fn main() {
    count_down(5);

    let result = truncate_to_length("Rust programming", 4);
    println!("{result}");  // "Rust"

    let total = swap_and_sum(10, 20);
    println!("{total}");  // 30
}
```
