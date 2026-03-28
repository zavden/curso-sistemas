# T02 — Fn, FnMut, FnOnce

## Los tres traits de closure

Cada closure en Rust implementa automaticamente uno o mas de
estos tres traits, dependiendo de **como usa** las variables
que captura:

```text
Trait       Firma del metodo          Captura       ¿Cuantas veces?
────────────────────────────────────────────────────────────────────
FnOnce      fn call_once(self, ...)   Por valor     Una sola vez
FnMut       fn call_mut(&mut self, ...)  &mut       Multiples
Fn          fn call(&self, ...)       &             Multiples
```

```rust
// FnOnce — consume las capturas. Solo puede llamarse una vez.
let name = String::from("Alice");
let consume = || drop(name);  // mueve name → FnOnce
consume();
// consume();  // error: ya fue consumida

// FnMut — modifica las capturas. Puede llamarse multiples veces.
let mut count = 0;
let mut increment = || count += 1;  // &mut count → FnMut
increment();
increment();
increment();
println!("{count}");  // 3

// Fn — solo lee las capturas. Puede llamarse multiples veces.
let name = String::from("Alice");
let greet = || println!("Hello, {name}");  // &name → Fn
greet();
greet();
greet();  // sin limite
```

## La jerarquia: Fn ⊂ FnMut ⊂ FnOnce

Los tres traits forman una jerarquia de subtipos:

```text
        FnOnce          ← todo closure implementa FnOnce
          ↑
        FnMut           ← si puede llamarse multiples veces (mutable)
          ↑
         Fn             ← si puede llamarse multiples veces (inmutable)

Fn    implementa: Fn + FnMut + FnOnce
FnMut implementa: FnMut + FnOnce
FnOnce implementa: solo FnOnce
```

```rust
// Definicion simplificada en la stdlib:
// trait FnOnce<Args> {
//     type Output;
//     fn call_once(self, args: Args) -> Self::Output;
// }
//
// trait FnMut<Args>: FnOnce<Args> {
//     fn call_mut(&mut self, args: Args) -> Self::Output;
// }
//
// trait Fn<Args>: FnMut<Args> {
//     fn call(&self, args: Args) -> Self::Output;
// }

// Consecuencia: una funcion que acepta FnOnce acepta CUALQUIER closure.
// Una funcion que acepta Fn solo acepta closures que no mutan ni consumen.
```

```rust
// Demostrar la jerarquia:
fn takes_fn_once(f: impl FnOnce()) { f(); }
fn takes_fn_mut(mut f: impl FnMut()) { f(); }
fn takes_fn(f: impl Fn()) { f(); }

let name = String::from("Alice");

// Fn closure — funciona en TODAS las funciones:
let greet = || println!("{name}");
takes_fn(&greet);
takes_fn_mut(&greet);
takes_fn_once(greet);

// FnMut closure — funciona en FnMut y FnOnce, pero NO en Fn:
let mut count = 0;
let mut increment = || count += 1;
// takes_fn(&increment);     // error: no implementa Fn
takes_fn_mut(&mut increment);
// takes_fn_once(increment);  // OK pero consume la closure

// FnOnce closure — SOLO funciona en FnOnce:
let s = String::from("bye");
let consume = || drop(s);
// takes_fn(&consume);       // error
// takes_fn_mut(&mut consume); // error
takes_fn_once(consume);     // OK
```

## Como Rust decide el trait

Rust analiza el cuerpo de la closure y asigna el trait mas
permisivo posible:

```rust
// ── Fn: solo lectura ──
let x = 10;
let read = || println!("{x}");
// x solo se lee → captura &x → implementa Fn (+ FnMut + FnOnce)

// ── FnMut: modifica capturas ──
let mut v = vec![1, 2, 3];
let mut push = || v.push(4);
// v se modifica → captura &mut v → implementa FnMut (+ FnOnce, NO Fn)

// ── FnOnce: consume capturas ──
let s = String::from("hello");
let consume = || {
    let _owned = s;  // mueve s dentro de la closure
};
// s se mueve → captura s por valor → implementa solo FnOnce

// ── Fn: sin capturas (tambien es fn pointer) ──
let add = |a: i32, b: i32| a + b;
// Nada que capturar → implementa Fn (+ FnMut + FnOnce)
// Tambien convertible a fn(i32, i32) -> i32
```

```text
Regla para determinar el trait:

¿La closure MUEVE/CONSUME alguna captura?
├── SI → FnOnce
└── NO → ¿La closure MODIFICA (&mut) alguna captura?
         ├── SI → FnMut
         └── NO → Fn
```

```rust
// Caso sutil: tipos Copy
let x = 42_i32;  // i32 es Copy
let f = || {
    let _y = x;  // "mueve" x, pero i32 es Copy → en realidad copia
};
// f implementa Fn (no FnOnce) porque copiar no es consumir
f();
f();  // OK — se puede llamar multiples veces
```

## Funciones que aceptan closures

Cuando escribes una funcion que recibe una closure, el bound
determina que closures acepta:

```rust
// Acepta CUALQUIER closure (la mas flexible para el llamador):
fn call_once<F: FnOnce() -> String>(f: F) -> String {
    f()  // consume f — solo puede llamarla una vez
}

// Acepta closures que pueden llamarse multiples veces (mutando):
fn call_twice<F: FnMut()>(mut f: F) {
    f();
    f();
}

// Acepta closures que pueden llamarse multiples veces (sin mutar):
fn call_many<F: Fn() -> i32>(f: F) -> i32 {
    f() + f() + f()  // llama 3 veces
}
```

```text
¿Que bound elegir en tu funcion?

Regla: usa el bound MAS GENERAL que tu funcion necesita.

- Si solo llamas la closure UNA vez → FnOnce
  (acepta todas las closures)

- Si la llamas multiples veces y necesitas mutar → FnMut
  (acepta FnMut y Fn, rechaza FnOnce)

- Si la llamas multiples veces sin mutar → Fn
  (solo acepta Fn — el mas restrictivo)

Mas general = acepta mas closures del llamador.
FnOnce es el mas general, Fn el mas restrictivo.
```

```rust
// La stdlib sigue esta regla:

// map llama la closure una vez por elemento → FnMut
// fn map<B, F: FnMut(Self::Item) -> B>(self, f: F) -> Map<Self, F>
vec![1, 2, 3].iter().map(|x| x * 2);

// for_each llama una vez por elemento → FnMut
// fn for_each<F: FnMut(Self::Item)>(self, f: F)
vec![1, 2, 3].iter().for_each(|x| println!("{x}"));

// thread::spawn necesita mover la closure al hilo → FnOnce + Send + 'static
// fn spawn<F: FnOnce() + Send + 'static>(f: F) -> JoinHandle<()>

// sort_by compara multiples veces → FnMut
// fn sort_by<F: FnMut(&T, &T) -> Ordering>(&mut self, compare: F)

// unwrap_or_else evalua solo si es Err/None → FnOnce
// fn unwrap_or_else<F: FnOnce(E) -> T>(self, op: F) -> T
```

## Fn traits con parametros y retorno

```rust
// Los traits Fn tienen la forma Fn(Args) -> Return:

fn apply<F: Fn(i32) -> i32>(f: F, x: i32) -> i32 {
    f(x)
}

let result = apply(|x| x * 2, 5);    // 10
let result = apply(|x| x + 100, 5);  // 105

// Multiples parametros:
fn combine<F: Fn(i32, i32) -> i32>(f: F, a: i32, b: i32) -> i32 {
    f(a, b)
}

let sum = combine(|a, b| a + b, 3, 4);   // 7
let prod = combine(|a, b| a * b, 3, 4);  // 12

// Sin retorno (retorna ()):
fn execute<F: FnMut()>(mut f: F) {
    f();
}

// Con where clause (mas legible):
fn transform<T, F>(items: Vec<T>, f: F) -> Vec<T>
where
    F: FnMut(T) -> T,
{
    items.into_iter().map(f).collect()
}
```

## Funciones regulares como closures

Las funciones normales (`fn`) implementan los tres traits Fn,
FnMut y FnOnce (porque no capturan nada):

```rust
fn double(x: i32) -> i32 {
    x * 2
}

// Puedes pasar fn donde se espera una closure:
let v: Vec<i32> = vec![1, 2, 3].iter().map(|x| x * 2).collect();
let v: Vec<i32> = vec![1, 2, 3].iter().map(double).collect(); // sin closure

// Funciones como valores:
fn apply(f: fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}
let result = apply(double, 5);  // 10

// Diferencia: fn pointer vs Fn trait
fn with_fn_pointer(f: fn(i32) -> i32, x: i32) -> i32 { f(x) }
fn with_fn_trait<F: Fn(i32) -> i32>(f: F, x: i32) -> i32 { f(x) }

// fn pointer: solo acepta funciones y closures sin capturas
// Fn trait: acepta cualquier closure que implemente Fn (con o sin capturas)
```

```rust
// Metodos como closures:
let v = vec!["hello", "world", "rust"];

// Equivalentes:
let upper: Vec<String> = v.iter().map(|s| s.to_uppercase()).collect();
let upper: Vec<String> = v.iter().map(str::to_uppercase).collect();

// Funciones de la stdlib como closures:
let parsed: Result<Vec<i32>, _> = vec!["1", "2", "3"]
    .iter()
    .map(|s| s.parse::<i32>())
    .collect();

// Constructores de enum como closures:
let options: Vec<Option<i32>> = vec![1, 2, 3]
    .into_iter()
    .map(Some)   // Some es una funcion! fn(T) -> Option<T>
    .collect();
```

## Ejemplo integrador

```rust
/// Aplica una serie de transformaciones a un Vec.
fn pipeline<T>(
    mut data: Vec<T>,
    transforms: &[&dyn Fn(&mut Vec<T>)],  // Fn porque solo lee/modifica el Vec pasado
) -> Vec<T> {
    for transform in transforms {
        transform(&mut data);
    }
    data
}

fn main() {
    let data = vec![5, 3, 8, 1, 9, 2, 7, 4, 6];

    // Cada closure captura nada o solo lee — todas son Fn
    let sort: Box<dyn Fn(&mut Vec<i32>)> = Box::new(|v| v.sort());
    let reverse: Box<dyn Fn(&mut Vec<i32>)> = Box::new(|v| v.reverse());
    let keep_top_3: Box<dyn Fn(&mut Vec<i32>)> = Box::new(|v| v.truncate(3));

    let result = pipeline(data, &[&*sort, &*reverse, &*keep_top_3]);
    assert_eq!(result, vec![9, 8, 7]);
}
```

```rust
// Ejemplo con los tres traits:
fn demo() {
    let greeting = String::from("Hello");

    // Fn — solo lee greeting
    let say_hello: Box<dyn Fn(&str)> = Box::new(|name| {
        println!("{greeting}, {name}!");
    });
    say_hello("Alice");
    say_hello("Bob");

    // FnMut — modifica un contador
    let mut log = Vec::new();
    let mut logger = |msg: &str| {
        log.push(msg.to_string());  // modifica log → FnMut
    };
    logger("started");
    logger("processing");
    logger("done");
    println!("{log:?}");

    // FnOnce — consume un recurso
    let data = vec![1, 2, 3, 4, 5];
    let processor = || {
        let sum: i32 = data.into_iter().sum();  // consume data
        println!("sum: {sum}");
    };
    processor();
    // processor();  // error: ya se consumio data
}
```

## Errores comunes

```rust
// ERROR 1: llamar FnOnce mas de una vez
fn call_twice(f: impl FnOnce()) {
    f();
    // f();  // error: f fue consumida en la primera llamada
}
// Solucion: cambiar a FnMut o Fn si necesitas multiples llamadas

// ERROR 2: bound demasiado restrictivo (Fn cuando bastaba FnOnce)
fn run(f: impl Fn()) { f(); }

let s = String::from("hello");
// run(|| drop(s));  // error: closure no implementa Fn (solo FnOnce)
// Solucion: cambiar a FnOnce si solo llamas f una vez

// ERROR 3: olvidar mut en la variable de closure FnMut
let mut count = 0;
let increment = || count += 1;  // closure es FnMut
// increment();  // error: cannot borrow as mutable
let mut increment = || count += 1;  // mut en la variable
increment();  // OK

// ERROR 4: olvidar mut en el parametro de FnMut
fn call<F: FnMut()>(f: F) {
    // f();  // error: f no es mut
}
fn call<F: FnMut()>(mut f: F) {  // mut en el parametro
    f();  // OK
}

// ERROR 5: confundir fn pointer con Fn trait
fn takes_fn_ptr(f: fn(i32) -> i32) { }
let x = 10;
let closure = |n| n + x;  // captura x
// takes_fn_ptr(closure);  // error: closures con capturas no son fn pointers
// Solucion: usar impl Fn(i32) -> i32
```

## Cheatsheet

```text
Traits:
  FnOnce(Args) -> R   consume capturas, llamar 1 vez   call_once(self)
  FnMut(Args) -> R    muta capturas, llamar N veces     call_mut(&mut self)
  Fn(Args) -> R       lee capturas, llamar N veces      call(&self)

Jerarquia:
  Fn ⊂ FnMut ⊂ FnOnce
  (Fn implementa los tres, FnOnce solo uno)

¿Que trait tiene mi closure?
  Solo lee capturas         → Fn
  Modifica capturas         → FnMut
  Mueve/consume capturas    → FnOnce
  Sin capturas              → Fn (+ convertible a fn pointer)
  Tipos Copy movidos        → Fn (copiar no es consumir)

¿Que bound poner en mi funcion?
  Llamo f una vez           → FnOnce (acepta todo)
  Llamo f multiples veces   → FnMut (acepta FnMut y Fn)
  Llamo f sin mutar nada    → Fn (solo acepta Fn)
  Regla: el bound mas general que funcione

stdlib:
  map/filter/for_each  → FnMut
  unwrap_or_else       → FnOnce
  sort_by              → FnMut
  thread::spawn        → FnOnce + Send + 'static

Sintaxis:
  fn foo(f: impl Fn(i32) -> i32)           azucar
  fn foo<F: Fn(i32) -> i32>(f: F)          generico
  fn foo(f: &dyn Fn(i32) -> i32)           trait object
  fn foo(f: Box<dyn Fn(i32) -> i32>)       boxed trait object
```

---

## Ejercicios

### Ejercicio 1 — Identificar el trait

```rust
// Para cada closure, predice si implementa Fn, FnMut, o FnOnce.
// Luego verifica pasandola a funciones con distintos bounds.

fn needs_fn(f: impl Fn()) { f(); f(); }
fn needs_fn_mut(mut f: impl FnMut()) { f(); f(); }
fn needs_fn_once(f: impl FnOnce()) { f(); }

// a)
let x = 42;
let a = || println!("{x}");

// b)
let mut v = Vec::new();
let b = || v.push(1);

// c)
let s = String::from("hello");
let c = || { let _ = s; };

// d)
let d = || 42;

// e)
let s = String::from("hello");
let e = || println!("{}", s.len());

// ¿Cuales pasan a needs_fn? ¿A needs_fn_mut? ¿A needs_fn_once?
```

### Ejercicio 2 — Elegir el bound correcto

```rust
// Para cada funcion, elige el bound mas general que funcione:

// a) Llama f una vez y retorna el resultado
fn apply_once<F: ???>(f: F) -> i32 { f(10) }

// b) Llama f en un loop 100 veces
fn repeat<F: ???>(mut f: F) { for _ in 0..100 { f(); } }

// c) Guarda f en un struct y la llama despues (multiples veces, sin mutar)
struct Callback<F: ???> { f: F }

// d) Pasa f a dos hilos distintos
fn parallel<F: ???>(f: F) { /* ... */ }

// Justifica cada eleccion.
```

### Ejercicio 3 — Pipeline de transformaciones

```rust
// Implementa fn pipeline(input: i32, steps: Vec<Box<dyn Fn(i32) -> i32>>) -> i32
// que aplique cada paso al resultado del anterior.
//
// Ejemplo:
//   pipeline(5, vec![
//       Box::new(|x| x * 2),    // 10
//       Box::new(|x| x + 3),    // 13
//       Box::new(|x| x * x),    // 169
//   ])
//   → 169
//
// ¿Por que se usa Fn y no FnOnce aqui?
// ¿Podrias cambiar a FnOnce? ¿Que implicaciones tendria?
```
