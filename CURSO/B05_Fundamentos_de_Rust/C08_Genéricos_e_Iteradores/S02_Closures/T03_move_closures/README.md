# T03 — move closures

## El problema: closures que viven mas que sus capturas

Por defecto, Rust captura variables por referencia cuando puede.
Pero a veces la closure necesita **vivir mas** que el scope donde
se creo — y las referencias dejarian de ser validas:

```rust
fn make_greeter() -> impl Fn() {
    let name = String::from("Alice");
    || println!("Hello, {name}")
    // error: `name` does not live long enough
    // La closure captura &name, pero name se destruye al salir de
    // make_greeter. La referencia seria dangling.
}
```

```rust
// Solucion: move
fn make_greeter() -> impl Fn() {
    let name = String::from("Alice");
    move || println!("Hello, {name}")
    //^^^^
    // move FUERZA que la closure tome ownership de name.
    // name se mueve dentro de la closure → la closure es dueña.
    // La closure puede vivir tanto como quiera.
}

let greet = make_greeter();
greet();  // "Hello, Alice"
greet();  // "Hello, Alice" — se puede llamar multiples veces
```

## Que hace move

La keyword `move` antes de `||` fuerza a la closure a capturar
**todas** sus variables por valor (moviendo o copiando):

```rust
let s = String::from("hello");  // non-Copy
let x = 42;                     // Copy

// Sin move — Rust captura por referencia (&s, &x):
let closure = || println!("{s} {x}");
println!("{s}");  // OK — s sigue disponible (solo se presto)

// Con move — Rust captura por valor (mueve s, copia x):
let closure = move || println!("{s} {x}");
// println!("{s}");  // error: s fue movida a la closure
println!("{x}");     // OK — x es Copy, se copio (no se movio)
```

```text
Reglas de move:

1. move afecta a TODAS las capturas — no puedes elegir campo por campo
2. Tipos Copy se COPIAN (el original sigue disponible)
3. Tipos non-Copy se MUEVEN (el original ya no esta disponible)
4. move NO cambia el trait de la closure (Fn/FnMut/FnOnce)
   — solo cambia COMO se capturan, no como se USAN
```

```rust
// move + Copy = la variable original sigue viva:
let x = 10;
let y = 20;
let sum = move || x + y;  // copia x e y dentro de la closure
println!("{x} {y}");       // OK — x e y son Copy
println!("{}", sum());     // 30

// move + non-Copy = la variable original se mueve:
let v = vec![1, 2, 3];
let show = move || println!("{v:?}");  // mueve v
// println!("{v:?}");  // error: v fue movida
show();
show();  // OK — la closure es dueña de v
```

## move no cambia el trait

Un error conceptual comun es pensar que `move` hace la closure
`FnOnce`. No es asi — move cambia la captura, no el uso:

```rust
// move + solo lectura = Fn (no FnOnce)
let name = String::from("Alice");
let greet = move || println!("{name}");
// name fue movida AL CREAR la closure.
// Pero la closure solo LEE name → implementa Fn.
greet();
greet();
greet();  // puede llamarse multiples veces

// move + mutacion = FnMut
let mut v = vec![1, 2, 3];
let mut push = move || v.push(4);
// v fue movida. La closure la modifica → FnMut.
push();
push();  // multiples veces

// move + consumo = FnOnce
let s = String::from("hello");
let consume = move || {
    let _owned = s;  // mueve s FUERA de la closure struct
};
consume();
// consume();  // error: FnOnce — s fue consumida dentro del cuerpo
```

```text
Resumen:
  move determina COMO se captura (por valor)
  El CUERPO determina QUE TRAIT implementa (Fn/FnMut/FnOnce)

  move + solo leer capturas     → Fn
  move + mutar capturas         → FnMut
  move + consumir capturas      → FnOnce
```

## Uso con threads

El caso de uso mas importante de `move` es con `thread::spawn`.
Un hilo nuevo puede vivir mas que el scope donde se creo, asi
que las referencias no son validas:

```rust
use std::thread;

let name = String::from("worker-1");

// Sin move — no compila:
// let handle = thread::spawn(|| {
//     println!("soy {name}");
//     // error: closure may outlive the current function,
//     //        but it borrows `name`
// });

// Con move — OK:
let handle = thread::spawn(move || {
    println!("soy {name}");
    // name fue movida al hilo — el hilo es dueño
});
handle.join().unwrap();
// name ya no existe aqui
```

```rust
// Patron comun: clonar antes de move cuando necesitas el dato en ambos lados
use std::thread;

let shared_data = String::from("datos importantes");

let data_for_thread = shared_data.clone();  // clon para el hilo
let handle = thread::spawn(move || {
    println!("hilo: {data_for_thread}");
});

println!("main: {shared_data}");  // el original sigue aqui
handle.join().unwrap();
```

```rust
// Multiples hilos — clonar para cada uno:
use std::thread;

let config = String::from("host=localhost;port=8080");

let mut handles = vec![];
for i in 0..3 {
    let config = config.clone();  // clon para cada hilo
    let handle = thread::spawn(move || {
        println!("hilo {i}: config = {config}");
        // Cada hilo tiene su propia copia de config
        // i es Copy — se copia automaticamente con move
    });
    handles.push(handle);
}

for h in handles {
    h.join().unwrap();
}
```

```text
¿Por que thread::spawn requiere move?

La firma de spawn es:
  fn spawn<F: FnOnce() + Send + 'static>(f: F) -> JoinHandle<()>

  FnOnce  → el hilo ejecuta la closure una vez
  Send    → la closure (y sus capturas) pueden enviarse a otro hilo
  'static → la closure no contiene referencias a datos locales
            (debe ser dueña de todo lo que usa)

move garantiza que la closure es dueña de todas sus capturas,
satisfaciendo el bound 'static.
```

## Uso con async

En codigo async, las closures y futures a menudo necesitan
`move` por las mismas razones que los threads:

```rust
// Ejemplo conceptual (requiere runtime async como tokio):

// async move — mueve capturas al Future
async fn process() {
    let data = vec![1, 2, 3];

    // Sin move, data se presta al future — pero el future puede
    // ejecutarse despues de que data se destruya
    let future = async move {
        println!("{data:?}");
        // data fue movida al future
    };

    // data ya no esta disponible aqui
    future.await;
}
```

```rust
// Patron con tokio::spawn (similar a thread::spawn):
// tokio::spawn requiere: Future + Send + 'static

async fn run() {
    let message = String::from("hola async");

    tokio::spawn(async move {
        // move mueve message al Future
        println!("{message}");
    }).await.unwrap();
}
```

```text
Regla para async:
- Si el future se ejecuta "en otro lugar" (spawn) → move
- Si el future se ejecuta inline (await inmediato) → move no necesario
- Los mismos principios que con threads aplican
```

## move con campos de structs

```rust
struct Config {
    host: String,
    port: u16,
}

let config = Config {
    host: String::from("localhost"),
    port: 8080,
};

// move mueve TODO el struct:
let show = move || println!("{}:{}", config.host, config.port);
// config ya no esta disponible
show();

// Si solo necesitas algunos campos, clona o extrae antes:
let config = Config {
    host: String::from("localhost"),
    port: 8080,
};

let host = config.host.clone();  // clonar solo lo que necesitas
let port = config.port;          // Copy

let show = move || println!("{host}:{port}");
println!("config.port: {}", config.port);  // config.port sigue accesible
// config.host tambien, porque clonamos en vez de mover
```

```rust
// En Rust 2021, sin move, la captura es parcial:
let config = Config {
    host: String::from("localhost"),
    port: 8080,
};

let get_port = || config.port;  // captura solo &config.port
println!("{}", config.host);     // OK — host no fue capturada

// Con move, se mueve TODO el struct:
let get_port = move || config.port;
// println!("{}", config.host);  // error: config fue movida
```

## Patrones comunes

```rust
// 1. Closure factory — retornar closures
fn make_adder(n: i32) -> impl Fn(i32) -> i32 {
    move |x| x + n  // n es Copy, pero move es necesario para 'static
}

let add5 = make_adder(5);
let add10 = make_adder(10);
assert_eq!(add5(3), 8);
assert_eq!(add10(3), 13);

// 2. Closure con estado — contador
fn make_counter() -> impl FnMut() -> i32 {
    let mut count = 0;
    move || {
        count += 1;
        count
    }
}

let mut counter = make_counter();
assert_eq!(counter(), 1);
assert_eq!(counter(), 2);
assert_eq!(counter(), 3);

// 3. Event handlers / callbacks
struct Button {
    on_click: Box<dyn Fn()>,
}

impl Button {
    fn new(label: String) -> Self {
        Button {
            on_click: Box::new(move || {
                println!("clicked: {label}");
            }),
        }
    }

    fn click(&self) {
        (self.on_click)();
    }
}

let btn = Button::new("Submit".to_string());
btn.click();  // "clicked: Submit"
btn.click();
```

```rust
// 4. move con iteradores — recolectar en un hilo
use std::thread;

fn parallel_sum(chunks: Vec<Vec<i32>>) -> i32 {
    let handles: Vec<_> = chunks.into_iter()
        .map(|chunk| {
            thread::spawn(move || {
                chunk.iter().sum::<i32>()
            })
        })
        .collect();

    handles.into_iter()
        .map(|h| h.join().unwrap())
        .sum()
}

let data = vec![vec![1, 2, 3], vec![4, 5, 6], vec![7, 8, 9]];
assert_eq!(parallel_sum(data), 45);
```

## Errores comunes

```rust
// ERROR 1: pensar que move hace la closure FnOnce
let s = String::from("hello");
let f = move || println!("{s}");
f();
f();  // OK — move no es FnOnce. Solo lee s → Fn.

// ERROR 2: olvidar move con thread::spawn
// let s = String::from("hello");
// thread::spawn(|| println!("{s}"));
// error: closure may outlive the current function
// Solucion: thread::spawn(move || println!("{s}"));

// ERROR 3: mover una variable y luego intentar usarla
let data = vec![1, 2, 3];
let handle = std::thread::spawn(move || {
    println!("{data:?}");
});
// println!("{data:?}");  // error: data fue movida
// Solucion: clonar antes de move
handle.join().unwrap();

// ERROR 4: move cuando no es necesario (innecesario pero inofensivo)
let x = 42;
let f = move || x + 1;  // move copia x (es Copy)
let g = || x + 1;        // sin move, captura &x
// Ambas funcionan. move es redundante aqui, pero no daña.

// ERROR 5: querer move parcial
let a = String::from("hello");
let b = String::from("world");
// No puedes: move || { usa a pero no b } sin mover b tambien
// move afecta TODAS las capturas.
// Solucion: extraer lo que necesitas antes:
let a_clone = a;  // "renombrar" para que la closure capture solo esto
let f = move || println!("{a_clone}");
println!("{b}");  // b sigue disponible
```

## Cheatsheet

```text
Sintaxis:
  || expr              captura automatica (ref o move segun uso)
  move || expr         fuerza captura por valor

Que hace move:
  Tipos Copy     → copia el valor (original sigue disponible)
  Tipos non-Copy → mueve el valor (original ya no disponible)

move NO cambia el trait:
  move + solo lectura     → Fn
  move + mutacion         → FnMut
  move + consumo          → FnOnce

Cuando usar move:
  ✓ thread::spawn        (closure debe ser 'static)
  ✓ tokio::spawn         (future debe ser 'static)
  ✓ retornar closure     (closure debe vivir mas que el scope)
  ✓ almacenar en struct  (campo debe ser dueño de sus datos)

Patron clone-then-move:
  let data_clone = data.clone();
  thread::spawn(move || { usa data_clone });
  // data original sigue disponible
```

---

## Ejercicios

### Ejercicio 1 — move y ownership

```rust
// Para cada caso, predice:
//   - ¿La variable original sigue disponible despues de la closure?
//   - ¿Que trait implementa la closure?
// Luego verifica.

// a)
let x = 42;
let f = move || x + 1;

// b)
let s = String::from("hello");
let f = move || println!("{s}");

// c)
let s = String::from("hello");
let f = move || { let _ = s; };

// d)
let v = vec![1, 2, 3];
let f = move || v.len();

// e)
let mut count = 0;
let mut f = move || { count += 1; count };
```

### Ejercicio 2 — Closure factory

```rust
// Implementa estas funciones que retornan closures:
//
// a) fn make_multiplier(factor: i32) -> impl Fn(i32) -> i32
//    make_multiplier(3)(5) == 15
//
// b) fn make_accumulator(initial: i32) -> impl FnMut(i32) -> i32
//    Cada llamada suma el argumento al acumulador y retorna el total.
//    let mut acc = make_accumulator(10);
//    acc(5)  == 15
//    acc(3)  == 18
//    acc(2)  == 20
//
// c) fn make_once_logger(prefix: String) -> impl FnOnce(String)
//    Imprime "{prefix}: {message}" y consume prefix.
//
// ¿Cual necesita move? ¿Por que?
```

### Ejercicio 3 — Threads con move

```rust
// Crea 5 hilos. Cada hilo recibe:
//   - Un indice (0..5) — i32, Copy
//   - Una copia de un Vec<String> con 3 palabras
//
// Cada hilo debe imprimir: "Hilo {i}: {palabras:?}"
//
// Recolecta los JoinHandle y espera a que todos terminen.
//
// Preguntas:
//   ¿Necesitas clonar el Vec para cada hilo? ¿Por que?
//   ¿Necesitas clonar el indice? ¿Por que no?
//   ¿Que pasa si olvidas move?
```
