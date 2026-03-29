# T04 — Closures como estrategia

---

## 1. De function pointers a closures

En S01/T01 viste el patron Strategy en C: pasar un function pointer
como argumento para inyectar comportamiento. El problema: las funciones
en C no pueden capturar variables del entorno — necesitas el truco de
`void *userdata`.

```c
// C: callback + userdata manual
typedef void (*ForEachFn)(int element, void *userdata);

void for_each(const int *arr, int n, ForEachFn fn, void *userdata) {
    for (int i = 0; i < n; i++)
        fn(arr[i], userdata);
}

// El contexto se pasa manualmente via void*
int total = 0;
for_each(data, 5, sum_callback, &total);
```

Rust resuelve esto con **closures**: funciones anonimas que capturan
variables del entorno automaticamente.

```rust
// Rust: closure captura 'total' sin void*
let mut total = 0;
data.iter().for_each(|x| total += x);
// total contiene la suma
```

La closure `|x| total += x` captura `total` por referencia mutable.
No hay `void *`, no hay casts, no hay parametro de contexto extra.

---

## 2. Sintaxis de closures

### 2.1 Formas basicas

```rust
// Closure sin argumentos
let greet = || println!("hello");
greet();

// Closure con un argumento
let double = |x| x * 2;
println!("{}", double(5));  // 10

// Closure con tipo explicito
let add = |a: i32, b: i32| -> i32 { a + b };
println!("{}", add(3, 4));  // 7

// Closure multi-linea
let classify = |x: i32| {
    if x > 0 { "positive" }
    else if x < 0 { "negative" }
    else { "zero" }
};
```

### 2.2 Comparacion con funciones y function pointers

```rust
// Funcion normal
fn add_fn(a: i32, b: i32) -> i32 { a + b }

// Function pointer (tipo: fn(i32, i32) -> i32)
let fp: fn(i32, i32) -> i32 = add_fn;

// Closure sin captura (compatible con fn pointer)
let cl = |a: i32, b: i32| a + b;
let fp2: fn(i32, i32) -> i32 = cl;  // OK: sin captura → coerce a fn

// Closure CON captura (NO compatible con fn pointer)
let offset = 10;
let cl2 = |x: i32| x + offset;
// let fp3: fn(i32) -> i32 = cl2;  // ERROR: captura entorno
```

Una closure sin capturas puede convertirse a function pointer. Una con
capturas no puede — porque necesita el contexto adicional.

---

## 3. Captura del entorno

### 3.1 Tres modos de captura

El compilador decide automaticamente como capturar cada variable:

```rust
let name = String::from("Ana");
let mut counter = 0;
let data = vec![1, 2, 3];

// Captura por referencia inmutable (&name)
let greet = || println!("Hello, {}", name);

// Captura por referencia mutable (&mut counter)
let mut increment = || counter += 1;

// Captura por valor (move data)
let consumer = move || {
    println!("{:?}", data);
    // data se movio a la closure — ya no existe afuera
};
```

### 3.2 Reglas de captura

El compilador elige el modo **menos restrictivo** que satisfaga el uso:

```
  ¿La closure solo lee la variable?
    → captura por &T (referencia inmutable)

  ¿La closure modifica la variable?
    → captura por &mut T (referencia mutable)

  ¿La closure consume la variable (move, drop)?
    → captura por valor (T)
```

```rust
let s = String::from("hello");

let f1 = || println!("{}", s);        // &s (solo lee)
let f2 = || { let _ = s.len(); };     // &s (solo lee)
// let f3 = || { drop(s); };          // mueve s (consume)
```

### 3.3 Keyword move

`move` fuerza captura por valor de **todas** las variables:

```rust
let name = String::from("Ana");

// Sin move: captura por &name
let greet = || println!("{}", name);
println!("{}", name);  // OK: name sigue vivo

// Con move: captura por valor (name se mueve a la closure)
let greet = move || println!("{}", name);
// println!("{}", name);  // ERROR: name fue movido
```

`move` es esencial cuando la closure debe vivir mas que las variables
capturadas:

```rust
fn make_greeter(name: String) -> impl Fn() {
    // Sin move: &name referencia una variable local que sera destruida
    // Con move: la closure es duena de name
    move || println!("Hello, {}", name)
}

let greet = make_greeter("Ana".into());
greet();  // Hello, Ana — name vive dentro de la closure
```

### 3.4 Diagrama de captura

```
  Sin move:                         Con move:

  stack frame                       closure (owns data)
  ┌──────────┐                      ┌───────────────────────┐
  │ name ────────→ "Ana" (heap)     │ name ──→ "Ana" (heap) │
  └──────────┘         ↑            └───────────────────────┘
                       │
  closure              │            stack frame
  ┌──────────┐         │            ┌──────────┐
  │ &name ─────────────┘            │ (name ya no existe)  │
  └──────────┘                      └──────────┘
  name sigue en el stack            name se movio a la closure
```

---

## 4. Los tres traits: Fn, FnMut, FnOnce

Cada closure implementa automaticamente uno o mas de estos traits segun
como use las variables capturadas:

### 4.1 Definiciones

```rust
// Puede llamarse multiples veces, no modifica capturas
trait Fn: FnMut {
    fn call(&self, args: Args) -> Output;
}

// Puede llamarse multiples veces, puede modificar capturas
trait FnMut: FnOnce {
    fn call_mut(&mut self, args: Args) -> Output;
}

// Se puede llamar al menos una vez, puede consumir capturas
trait FnOnce {
    fn call_once(self, args: Args) -> Output;
}
```

### 4.2 Jerarquia

```
  FnOnce        ← toda closure lo implementa
    ↑
  FnMut         ← si no consume capturas
    ↑
  Fn            ← si no modifica capturas

  Fn ⊂ FnMut ⊂ FnOnce
```

Toda `Fn` tambien es `FnMut` y `FnOnce`. Pero no al reves.

### 4.3 Ejemplos

```rust
let name = String::from("Ana");

// Fn: solo lee
let greet = || println!("Hello, {}", name);
// Puede llamarse infinitas veces, no modifica nada

let mut count = 0;

// FnMut: modifica captura
let mut increment = || { count += 1; count };
// Puede llamarse multiples veces, modifica count

let data = vec![1, 2, 3];

// FnOnce: consume captura
let consumer = || {
    let owned = data;  // mueve data dentro
    println!("{:?}", owned);
    // data ya no existe — solo se puede llamar una vez
};
consumer();
// consumer();  // ERROR: ya consumio data
```

### 4.4 Que trait usar como parametro

```rust
// Fn: si necesitas llamar multiples veces sin mutacion
fn apply_twice(f: impl Fn(i32) -> i32, x: i32) -> i32 {
    f(f(x))
}

// FnMut: si la closure necesita mutar su estado
fn apply_n_times(mut f: impl FnMut(), n: u32) {
    for _ in 0..n {
        f();
    }
}

// FnOnce: si solo la llamas una vez (mas flexible — acepta todo)
fn apply_once(f: impl FnOnce() -> String) -> String {
    f()
}
```

Regla: **pide el trait mas general** que necesites:

```
  Solo necesitas llamar una vez     → FnOnce (acepta todo)
  Necesitas llamar multiples veces  → FnMut (excluye FnOnce puro)
  No puedes mutar                   → Fn (excluye FnMut puro)
```

---

## 5. Closures como Strategy

### 5.1 Strategy basico

```rust
fn calculate_price(price: f64, qty: u32,
                   strategy: impl Fn(f64, u32) -> f64) -> f64 {
    strategy(price, qty)
}

// Estrategias como closures:
let no_discount = |p: f64, q: u32| p * q as f64;
let bulk = |p: f64, q: u32| {
    let total = p * q as f64;
    if q > 10 { total * 0.9 } else { total }
};
let flat = |_: f64, _: u32| 99.99;

println!("{:.2}", calculate_price(15.0, 20, no_discount)); // 300.00
println!("{:.2}", calculate_price(15.0, 20, bulk));        // 270.00
println!("{:.2}", calculate_price(15.0, 20, flat));        // 99.99
```

Compara con C:

```c
// C: function pointer + funciones nombradas
typedef double (*PricingStrategy)(double, int);
double no_discount(double p, int q) { return p * q; }
double result = calculate_price(15.0, 20, no_discount);
```

En Rust, la estrategia se define **inline** sin necesidad de una
funcion nombrada.

### 5.2 Strategy con captura (lo que C no puede)

```rust
fn apply_discount(prices: &[f64], discount_pct: f64) -> Vec<f64> {
    let factor = 1.0 - discount_pct / 100.0;

    // La closure CAPTURA 'factor' del entorno
    prices.iter().map(|p| p * factor).collect()
}

let prices = vec![100.0, 200.0, 50.0];
let discounted = apply_discount(&prices, 15.0);
// [85.0, 170.0, 42.5]
```

En C, `factor` tendria que pasar como `void *userdata`. En Rust, la
closure lo captura automaticamente.

### 5.3 Strategy almacenado en struct

```rust
struct Processor {
    transform: Box<dyn Fn(f64) -> f64>,
}

impl Processor {
    fn new(transform: impl Fn(f64) -> f64 + 'static) -> Self {
        Processor { transform: Box::new(transform) }
    }

    fn process(&self, value: f64) -> f64 {
        (self.transform)(value)
    }
}

let doubler = Processor::new(|x| x * 2.0);
let offsetter = Processor::new({
    let offset = 42.0;
    move |x| x + offset  // captura offset por valor
});

println!("{}", doubler.process(5.0));    // 10.0
println!("{}", offsetter.process(5.0));  // 47.0
```

`Box<dyn Fn(f64) -> f64>` es el equivalente Rust del function pointer
en un struct de C — pero con soporte para capturas.

---

## 6. Closures en la stdlib

### 6.1 Iteradores

Casi toda la API de iteradores usa closures:

```rust
let numbers = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

let result: Vec<i32> = numbers.iter()
    .filter(|&&x| x % 2 == 0)    // closure: predicado
    .map(|&x| x * x)             // closure: transformacion
    .take(3)
    .collect();

// [4, 16, 36]
```

Cada `.filter()`, `.map()`, `.for_each()` recibe una closure como
estrategia. Comparado con C:

```c
// C: para filtrar + transformar necesitas un loop manual
// o pasar function pointers + void* para cada operacion
int result[10];
int count = 0;
for (int i = 0; i < 10; i++) {
    if (numbers[i] % 2 == 0) {
        result[count++] = numbers[i] * numbers[i];
        if (count == 3) break;
    }
}
```

### 6.2 sort_by, sort_by_key

```rust
let mut students = vec![
    ("Ana", 85),
    ("Bob", 92),
    ("Cat", 78),
];

// Closure como comparador (equivalente al comparador de qsort)
students.sort_by(|a, b| a.1.cmp(&b.1));
// Por nota ascendente

students.sort_by_key(|s| std::cmp::Reverse(s.1));
// Por nota descendente
```

En C, `qsort` recibe `int (*cmp)(const void*, const void*)`. En Rust,
`sort_by` recibe `impl FnMut(&T, &T) -> Ordering`. Sin casts, sin
`void *`.

### 6.3 Option y Result

```rust
let maybe: Option<i32> = Some(42);

let doubled = maybe.map(|x| x * 2);            // Some(84)
let filtered = maybe.filter(|&x| x > 50);      // None
let or_else = maybe.unwrap_or_else(|| expensive_default());
```

### 6.4 thread::spawn

```rust
use std::thread;

let data = vec![1, 2, 3];

let handle = thread::spawn(move || {
    // move: la closure toma ownership de data
    let sum: i32 = data.iter().sum();
    sum
});

let result = handle.join().unwrap();  // 6
```

`thread::spawn` requiere `FnOnce + Send + 'static` — la closure debe
poder ejecutarse una vez, ser enviable entre hilos, y no tener
referencias a datos locales (de ahi el `move`).

---

## 7. Closures vs function pointers

### 7.1 fn vs Fn vs FnMut vs FnOnce

```
  fn(i32) -> i32        Function pointer — sin estado, tamano fijo (8B)
  impl Fn(i32) -> i32   Trait bound — zero-cost, monomorphized
  &dyn Fn(i32) -> i32   Trait object — dispatch dinamico (16B)
  Box<dyn Fn(i32) -> i32>  Owned trait object — heap (16B)
```

| Tipo | Estado | Tamano | Dispatch | Uso tipico |
|------|--------|--------|----------|-----------|
| `fn(T) -> U` | Sin estado | 8 bytes | Directo | FFI, APIs de C |
| `impl Fn(T) -> U` | Con capturas | Variable | Estatico | Parametro de funcion |
| `&dyn Fn(T) -> U` | Con capturas | 16 bytes | Dinamico | Almacenar en coleccion |
| `Box<dyn Fn(T) -> U>` | Con capturas | 16 bytes | Dinamico | Campo de struct |

### 7.2 Tamano de una closure

```rust
use std::mem::size_of_val;

let a = 42i32;
let b = String::from("hello");

let c1 = || {};                        // 0 bytes (sin capturas)
let c2 = || println!("{}", a);         // 8 bytes (captura &i32 = ptr)
let c3 = move || println!("{}", a);    // 4 bytes (captura i32 = copia)
let c4 = move || println!("{}", b);    // 24 bytes (captura String = 3 words)

println!("{}", size_of_val(&c1));  // 0
println!("{}", size_of_val(&c2));  // 8
println!("{}", size_of_val(&c3));  // 4
println!("{}", size_of_val(&c4));  // 24
```

Cada closure es un struct anonimo que contiene las variables capturadas.
El compilador genera un tipo unico para cada closure — por eso tienen
tamanos distintos y no se pueden nombrar.

### 7.3 Diagrama: closure como struct anonimo

```
  let offset = 10;
  let scale = 2.0;
  let f = move |x: i32| (x + offset) as f64 * scale;

  El compilador genera (conceptualmente):

  struct ClosureF {
      offset: i32,     // 4 bytes
      scale: f64,      // 8 bytes
  }                    // total: 16 bytes (con padding)

  impl Fn(i32) -> f64 for ClosureF {
      fn call(&self, x: i32) -> f64 {
          (x + self.offset) as f64 * self.scale
      }
  }
```

Esto es exactamente lo que en C harias con un struct + function pointer:

```c
typedef struct {
    int offset;
    double scale;
} ClosureCtx;

double closure_fn(int x, void *ctx) {
    ClosureCtx *c = (ClosureCtx *)ctx;
    return (x + c->offset) * c->scale;
}
```

Rust automatiza lo que C obliga a hacer a mano.

---

## 8. Closures como parametro: impl vs dyn vs fn

### 8.1 Tres formas de aceptar una closure

```rust
// 1. Generico con impl (zero-cost, monomorphized)
fn apply_impl(f: impl Fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}

// 2. Generico explicito (equivalente, mas flexible)
fn apply_generic<F: Fn(i32) -> i32>(f: F, x: i32) -> i32 {
    f(x)
}

// 3. Trait object (dispatch dinamico, una sola copia)
fn apply_dyn(f: &dyn Fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}

// 4. Function pointer (solo sin capturas)
fn apply_fn(f: fn(i32) -> i32, x: i32) -> i32 {
    f(x)
}
```

### 8.2 Cual elegir

```
  ¿La closure captura variables?
    │
    ├─ NO → fn(T) -> U puede funcionar (mas simple, FFI compatible)
    │
    └─ SI → ¿Necesitas almacenar la closure?
             │
             ├─ NO → impl Fn/FnMut/FnOnce (zero-cost, mas comun)
             │
             └─ SI → ¿En struct o coleccion?
                      │
                      ├─ Un solo tipo → impl Fn (con generics en el struct)
                      └─ Tipos mezclados → Box<dyn Fn> (heap, dynamic)
```

---

## 9. Retornar closures

### 9.1 Con impl Fn (un solo tipo)

```rust
fn make_adder(n: i32) -> impl Fn(i32) -> i32 {
    move |x| x + n
}

let add5 = make_adder(5);
let add10 = make_adder(10);

println!("{}", add5(3));   // 8
println!("{}", add10(3));  // 13
```

### 9.2 Con Box<dyn Fn> (tipos distintos)

```rust
fn make_transform(kind: &str) -> Box<dyn Fn(f64) -> f64> {
    match kind {
        "double" => Box::new(|x| x * 2.0),
        "square" => Box::new(|x| x * x),
        "negate" => Box::new(|x| -x),
        _ => Box::new(|x| x),
    }
}

let t = make_transform("square");
println!("{}", t(5.0));  // 25.0
```

Recuerda de S01/T01 que en C esto es imposible: no puedes retornar una
funcion que captura variables locales. En Rust, `move` + `Box` lo
resuelve.

### 9.3 Composicion de closures

```rust
fn compose<F, G>(f: F, g: G) -> impl Fn(f64) -> f64
where
    F: Fn(f64) -> f64,
    G: Fn(f64) -> f64,
{
    move |x| f(g(x))
}

let double_then_inc = compose(|x| x + 1.0, |x| x * 2.0);
println!("{}", double_then_inc(5.0));  // 11.0 = (5*2) + 1
```

En S01/T01 implementaste `compose` en C — pero no podias retornar la
composicion como funcion. En Rust, la closure captura `f` y `g` por
valor y vive tanto como necesites.

---

## 10. Comparacion final: C callbacks vs Rust closures

| Aspecto | C callback + void* | Rust closure |
|---------|-------------------|-------------|
| Sintaxis | Verbose (typedef + funcion + cast) | Concisa (`\|x\| x + 1`) |
| Captura | Manual via void* | Automatica |
| Type safety | Ninguna (void* cast) | Completa |
| Retornar closure | Imposible (sin JIT) | `impl Fn` o `Box<dyn Fn>` |
| Componer | Manual (struct de contexto) | `move \|x\| f(g(x))` |
| Performance | Call indirecto | Zero-cost (con impl) o call indirecto (con dyn) |
| Memoria | void* puede ser dangling | Borrow checker / ownership |

El closure de Rust es, mecanicamente, lo mismo que el callback+void*
de C: un puntero a funcion + un puntero a datos capturados. La
diferencia es que Rust genera el struct de capturas automaticamente,
verifica los tipos, y gestiona el lifetime.

---

## Errores comunes

### Error 1 — Closure que vive mas que sus capturas

```rust
fn make_closure() -> impl Fn() {
    let s = String::from("hello");
    || println!("{}", s)  // captura &s, pero s se destruye
//  ^^ ERROR: `s` does not live long enough
}
```

Fix: `move` para que la closure tome ownership.

```rust
fn make_closure() -> impl Fn() {
    let s = String::from("hello");
    move || println!("{}", s)
}
```

### Error 2 — FnMut donde se espera Fn

```rust
fn call_twice(f: impl Fn()) {
    f(); f();
}

let mut count = 0;
call_twice(|| count += 1);
//         ^^ ERROR: closure implements FnMut, not Fn
```

Fix: cambiar el bound a `FnMut`.

```rust
fn call_twice(mut f: impl FnMut()) {
    f(); f();
}
```

### Error 3 — Llamar FnOnce dos veces

```rust
fn use_twice(f: impl FnOnce() -> String) {
    let a = f();
    let b = f();  // ERROR: f ya fue consumida
}
```

`FnOnce` solo se puede llamar una vez. Si necesitas multiples llamadas,
usa `Fn` o `FnMut`.

### Error 4 — Tipos de closure distintos en match

```rust
let f = if condition {
    |x: i32| x + 1
} else {
    |x: i32| x * 2
//  ^^^ ERROR: different closure types
};
```

Cada closure tiene un tipo anonimo unico. Aun con la misma firma, son
tipos distintos. Fix: `Box<dyn Fn>`.

```rust
let f: Box<dyn Fn(i32) -> i32> = if condition {
    Box::new(|x| x + 1)
} else {
    Box::new(|x| x * 2)
};
```

---

## Ejercicios

### Ejercicio 1 — Closures basicas

Sin ejecutar, predice la salida y el trait (Fn/FnMut/FnOnce) de cada
closure:

```rust
let x = 10;
let a = || x + 1;

let mut count = 0;
let b = || { count += 1; count };

let name = String::from("Ana");
let c = move || format!("Hello, {}", name);

let data = vec![1, 2, 3];
let d = || drop(data);
```

**Prediccion**: Cual se puede llamar multiples veces? Cual consume?

<details><summary>Respuesta</summary>

| Closure | Captura | Trait | Veces llamable |
|---------|---------|-------|---------------|
| `a` | `&x` (lee) | `Fn` | Infinitas |
| `b` | `&mut count` (modifica) | `FnMut` | Multiples (con `let mut b`) |
| `c` | `name` por valor (move, pero solo lee) | `Fn` | Infinitas |
| `d` | `data` por valor (consume con drop) | `FnOnce` | Una sola vez |

Nota sobre `c`: aunque usa `move`, la closure solo lee `name` (lo usa
en `format!` que toma `&self`). El `move` transfiere ownership pero el
uso interno es solo lectura, asi que implementa `Fn` (no solo FnOnce).

Nota sobre `b`: para llamar una closure FnMut, la variable que la
contiene debe ser `let mut b`:

```rust
let mut count = 0;
let mut b = || { count += 1; count };
println!("{}", b());  // 1
println!("{}", b());  // 2
```

</details>

---

### Ejercicio 2 — Strategy con closures

Implementa un `TextProcessor` que aplica una pipeline de
transformaciones a un string. Cada transformacion es una closure.

```rust
struct TextProcessor {
    transforms: Vec<Box<dyn Fn(&str) -> String>>,
}
```

Implementa `add_transform`, `process`, y usalo para: uppercase, trim,
agregar prefijo "[PROC]".

**Prediccion**: Por que `Vec<Box<dyn Fn>>` y no `Vec<impl Fn>`?

<details><summary>Respuesta</summary>

```rust
struct TextProcessor {
    transforms: Vec<Box<dyn Fn(&str) -> String>>,
}

impl TextProcessor {
    fn new() -> Self {
        TextProcessor { transforms: vec![] }
    }

    fn add_transform(&mut self, f: impl Fn(&str) -> String + 'static) {
        self.transforms.push(Box::new(f));
    }

    fn process(&self, input: &str) -> String {
        let mut result = input.to_string();
        for t in &self.transforms {
            result = t(&result);
        }
        result
    }
}

fn main() {
    let mut proc = TextProcessor::new();

    proc.add_transform(|s| s.trim().to_string());
    proc.add_transform(|s| s.to_uppercase());

    let prefix = "[PROC]".to_string();
    proc.add_transform(move |s| format!("{} {}", prefix, s));

    println!("{}", proc.process("  hello world  "));
    // [PROC] HELLO WORLD
}
```

`Vec<impl Fn>` no es valido porque `impl Fn` no es un tipo concreto —
es un bound generico. Un Vec necesita un tipo concreto para todos sus
elementos. Como cada closure tiene un tipo anonimo distinto, la unica
forma de mezclarlas en un Vec es `Box<dyn Fn>` (type erasure).

</details>

---

### Ejercicio 3 — Composicion

Implementa `pipe` que recibe un vector de closures y las aplica en
secuencia:

```rust
fn pipe(fns: Vec<Box<dyn Fn(f64) -> f64>>, x: f64) -> f64
```

Luego implementa `compose2` que recibe dos closures y retorna una
nueva closure que es su composicion:

```rust
fn compose2(f: impl Fn(f64) -> f64 + 'static,
            g: impl Fn(f64) -> f64 + 'static)
    -> impl Fn(f64) -> f64
```

**Prediccion**: `pipe` necesita `Box<dyn Fn>`. `compose2` puede usar
`impl Fn`. Por que la diferencia?

<details><summary>Respuesta</summary>

```rust
fn pipe(fns: Vec<Box<dyn Fn(f64) -> f64>>, x: f64) -> f64 {
    fns.iter().fold(x, |acc, f| f(acc))
}

fn compose2(
    f: impl Fn(f64) -> f64 + 'static,
    g: impl Fn(f64) -> f64 + 'static,
) -> impl Fn(f64) -> f64 {
    move |x| f(g(x))
}

fn main() {
    // pipe
    let transforms: Vec<Box<dyn Fn(f64) -> f64>> = vec![
        Box::new(|x| x * 2.0),
        Box::new(|x| x + 1.0),
        Box::new(|x| x * x),
    ];
    println!("{}", pipe(transforms, 3.0));  // ((3*2)+1)^2 = 49

    // compose2
    let f = compose2(|x| x + 1.0, |x| x * 2.0);
    println!("{}", f(3.0));  // (3*2)+1 = 7

    // Encadenar composiciones
    let g = compose2(|x| x * x, compose2(|x| x + 1.0, |x| x * 2.0));
    println!("{}", g(3.0));  // ((3*2)+1)^2 = 49
}
```

`pipe` necesita `Box<dyn Fn>` porque almacena multiples closures de
tipos distintos en un Vec (misma razon que ejercicio 2).

`compose2` puede usar `impl Fn` porque solo tiene dos parametros
genericos `F` y `G` — cada uno con su propio tipo. No los mezcla en
una coleccion. El compilador monomorphiza `compose2` para cada par
concreto de closures.

Comparacion con C (S01/T01):

```c
// C: pipe funciona, compose permanente es imposible
int pipe(IntTransform *fns, int n, int x);  // OK
// IntTransform compose(f, g);  // IMPOSIBLE — no hay closures
```

</details>

---

### Ejercicio 4 — Closure como campo: comparador configurable

Crea un struct `SortedVec<T>` que mantenga sus elementos ordenados
segun un comparador custom (closure):

```rust
struct SortedVec<T> {
    data: Vec<T>,
    cmp: Box<dyn Fn(&T, &T) -> std::cmp::Ordering>,
}
```

Implementa `new`, `insert` (mantiene el orden), e `iter`.

**Prediccion**: Por que el comparador es `Box<dyn Fn>` y no `impl Fn`
como campo del struct?

<details><summary>Respuesta</summary>

```rust
use std::cmp::Ordering;

struct SortedVec<T> {
    data: Vec<T>,
    cmp: Box<dyn Fn(&T, &T) -> Ordering>,
}

impl<T> SortedVec<T> {
    fn new(cmp: impl Fn(&T, &T) -> Ordering + 'static) -> Self {
        SortedVec {
            data: Vec::new(),
            cmp: Box::new(cmp),
        }
    }

    fn insert(&mut self, value: T) {
        let pos = self.data.iter()
            .position(|item| (self.cmp)(&value, item) == Ordering::Less)
            .unwrap_or(self.data.len());
        self.data.insert(pos, value);
    }

    fn iter(&self) -> impl Iterator<Item = &T> {
        self.data.iter()
    }
}

fn main() {
    // Ascendente
    let mut asc = SortedVec::new(|a: &i32, b: &i32| a.cmp(b));
    asc.insert(5);
    asc.insert(2);
    asc.insert(8);
    asc.insert(1);
    let v: Vec<_> = asc.iter().collect();
    println!("{:?}", v);  // [1, 2, 5, 8]

    // Descendente
    let mut desc = SortedVec::new(|a: &i32, b: &i32| b.cmp(a));
    desc.insert(5);
    desc.insert(2);
    desc.insert(8);
    let v: Vec<_> = desc.iter().collect();
    println!("{:?}", v);  // [8, 5, 2]

    // Por longitud de string
    let mut by_len = SortedVec::new(|a: &String, b: &String| a.len().cmp(&b.len()));
    by_len.insert("hello".into());
    by_len.insert("hi".into());
    by_len.insert("hey".into());
    let v: Vec<_> = by_len.iter().collect();
    println!("{:?}", v);  // ["hi", "hey", "hello"]
}
```

El comparador es `Box<dyn Fn>` porque si fuera un generico (`F: Fn`),
el tipo del SortedVec cambiaria con cada closure distinta:

```rust
// Con generico: SortedVec<i32, ClosureA> ≠ SortedVec<i32, ClosureB>
// No podrias almacenar distintos SortedVec en un mismo Vec

// Con Box<dyn Fn>: SortedVec<i32> es un solo tipo,
// independiente del comparador
```

Alternativa: usar generico si no necesitas mezclar SortedVecs:

```rust
struct SortedVec<T, F: Fn(&T, &T) -> Ordering> {
    data: Vec<T>,
    cmp: F,  // sin Box, zero-cost
}
```

</details>

---

### Ejercicio 5 — move y lifetimes

Para cada caso, indica si compila. Si no, explica por que y da el fix:

```rust
// A
fn make_greeter(name: &str) -> impl Fn() + '_ {
    || println!("Hello, {}", name)
}

// B
fn make_greeter_owned(name: String) -> impl Fn() {
    || println!("Hello, {}", name)
}

// C
fn make_counter() -> impl FnMut() -> u32 {
    let mut n = 0;
    || { n += 1; n }
}

// D
fn spawn_printer(msg: String) {
    std::thread::spawn(|| println!("{}", msg));
}
```

**Prediccion**: Cuales necesitan `move`?

<details><summary>Respuesta</summary>

**A: Compila.** El lifetime `'_` ata la closure al lifetime de `name`.
La closure captura `&name` y es valida mientras `name` viva.

```rust
let name = String::from("Ana");
let greet = make_greeter(&name);
greet();  // OK: name sigue vivo
// drop(name);  // si haces esto, greet ya no es valida
```

**B: No compila.** La closure captura `&name`, pero `name` es un
parametro owned que se destruye al final de la funcion.

```
error: `name` does not live long enough
```

Fix: `move` para que la closure tome ownership.

```rust
fn make_greeter_owned(name: String) -> impl Fn() {
    move || println!("Hello, {}", name)
}
```

**C: No compila.** Mismo problema: `n` es local, la closure captura
`&mut n`, pero `n` se destruye.

Fix: `move`.

```rust
fn make_counter() -> impl FnMut() -> u32 {
    let mut n = 0;
    move || { n += 1; n }
}
```

**D: No compila.** `thread::spawn` requiere `'static` — la closure no
puede tener referencias a datos del stack. Captura `&msg` pero `msg`
se destruye al final de `spawn_printer`.

Fix: `move`.

```rust
fn spawn_printer(msg: String) {
    std::thread::spawn(move || println!("{}", msg));
}
```

Regla practica: si retornas una closure o la pasas a otro hilo,
**casi siempre necesitas `move`**.

</details>

---

### Ejercicio 6 — Traducir C callback a Rust

Traduce esta API de C con callbacks a Rust con closures:

```c
typedef void (*EventHandler)(int event_type, void *ctx);

typedef struct {
    EventHandler handlers[8];
    void *contexts[8];
    int count;
} EventBus;

void bus_register(EventBus *bus, EventHandler h, void *ctx);
void bus_emit(EventBus *bus, int event_type);
```

**Prediccion**: En Rust, necesitas los arrays separados de handlers y
contexts? Que los reemplaza?

<details><summary>Respuesta</summary>

```rust
struct EventBus {
    handlers: Vec<Box<dyn Fn(i32)>>,
}

impl EventBus {
    fn new() -> Self {
        EventBus { handlers: vec![] }
    }

    fn register(&mut self, handler: impl Fn(i32) + 'static) {
        self.handlers.push(Box::new(handler));
    }

    fn emit(&self, event_type: i32) {
        for handler in &self.handlers {
            handler(event_type);
        }
    }
}

fn main() {
    let mut bus = EventBus::new();

    // Handler sin contexto
    bus.register(|event| println!("Event: {}", event));

    // Handler con contexto capturado (reemplaza void* ctx)
    let prefix = String::from("LOG");
    bus.register(move |event| {
        println!("[{}] Event: {}", prefix, event);
    });

    // Handler con estado mutable (requiere interior mutability)
    use std::cell::Cell;
    let count = std::rc::Rc::new(Cell::new(0));
    let count_clone = count.clone();
    bus.register(move |_event| {
        count_clone.set(count_clone.get() + 1);
    });

    bus.emit(42);
    bus.emit(99);

    println!("Events processed: {}", count.get());  // 2
}
```

Los arrays separados `handlers[]` y `contexts[]` desaparecen. En Rust,
cada `Box<dyn Fn(i32)>` encapsula tanto la funcion como su contexto
(variables capturadas). No necesitas `void *ctx` porque la closure
captura las variables automaticamente.

Salida:

```
Event: 42
[LOG] Event: 42
Event: 99
[LOG] Event: 99
Events processed: 2
```

</details>

---

### Ejercicio 7 — Tamano de closures

Sin ejecutar, predice el tamano de cada closure:

```rust
use std::mem::size_of_val;

let a = 42u8;
let b = 42u64;
let c = [0u8; 100];
let s = String::from("hello");

let c1 = || {};
let c2 = || a;
let c3 = move || a;
let c4 = || b;
let c5 = move || b;
let c6 = || println!("{:?}", c);
let c7 = move || println!("{:?}", c);
let c8 = || println!("{}", s);
let c9 = move || println!("{}", s);
```

**Prediccion**: `c6` (borrow del array) vs `c7` (move del array) —
cual es mas grande?

<details><summary>Respuesta</summary>

| Closure | Captura | Tamano |
|---------|---------|--------|
| `c1` | Nada | 0 bytes |
| `c2` | `&a` (ref a u8) | 8 bytes (un puntero) |
| `c3` | `a` por valor (u8) | 1 byte |
| `c4` | `&b` (ref a u64) | 8 bytes (un puntero) |
| `c5` | `b` por valor (u64) | 8 bytes |
| `c6` | `&c` (ref al array) | 8 bytes (un puntero) |
| `c7` | `c` por valor (100 u8) | 100 bytes |
| `c8` | `&s` (ref a String) | 8 bytes (un puntero) |
| `c9` | `s` por valor (String) | 24 bytes (3 words: ptr+len+cap) |

`c6` captura un puntero al array (8 bytes).
`c7` copia el array completo a la closure (100 bytes).

Por eso el compilador prefiere captura por referencia: copia un puntero
de 8 bytes en vez de los datos completos. `move` solo cuando necesitas
ownership (retornar la closure, pasarla a otro hilo, etc.).

</details>

---

### Ejercicio 8 — Fn trait bounds: elegir el correcto

Para cada funcion, decide el trait minimo necesario (Fn, FnMut, o
FnOnce):

```rust
fn a(f: impl ???()) { f(); f(); f(); }

fn b(f: impl ???() -> String) -> String { f() }

fn c(f: impl ???()) {
    for _ in 0..10 { f(); }
}

fn d(f: impl ???(i32) -> i32) -> Vec<i32> {
    (0..5).map(f).collect()
}

fn e(f: impl ???()) {
    std::thread::spawn(f);
}
```

<details><summary>Respuesta</summary>

| Funcion | Trait | Razon |
|---------|-------|-------|
| `a` | `Fn` | Llama 3 veces sin mutar — necesita &self |
| `b` | `FnOnce` | Solo llama una vez — FnOnce es suficiente y mas flexible |
| `c` | `FnMut` | Llama 10 veces — necesita multiples llamadas. `Fn` tambien funciona pero `FnMut` es mas flexible |
| `d` | `FnMut` | `map` toma `FnMut` internamente |
| `e` | `FnOnce + Send + 'static` | `thread::spawn` requiere los tres |

Notas:
- `b` podria pedir `Fn`, pero `FnOnce` es mas flexible: acepta closures
  que consumen su estado.
- `c` podria pedir `Fn`, pero `FnMut` acepta mas closures (incluyendo
  las que mutan estado). Usa `Fn` solo si necesitas garantizar
  inmutabilidad.
- `d` usa `Iterator::map` que toma `FnMut` — no tienes opcion.
- `e` es el caso especial de threads: necesita `Send` (cruzar hilo) y
  `'static` (vivir independiente).

</details>

---

### Ejercicio 9 — Encontrar errores

Cada fragmento tiene un error. Identificalo:

```rust
// Error A
fn apply(f: impl Fn(i32) -> i32, x: i32) -> i32 { f(x) }

let mut total = 0;
apply(|x| { total += x; total }, 5);

// Error B
let closures: Vec<impl Fn()> = vec![
    || println!("a"),
    || println!("b"),
];

// Error C
fn make_fn() -> impl Fn() {
    let name = String::from("hello");
    || println!("{}", name)
}
```

<details><summary>Respuesta</summary>

**Error A: FnMut donde se espera Fn.**

La closure modifica `total` (`total += x`), asi que implementa `FnMut`,
no `Fn`. `apply` pide `Fn`.

Fix: cambiar a `FnMut`:
```rust
fn apply(mut f: impl FnMut(i32) -> i32, x: i32) -> i32 { f(x) }
```

**Error B: `impl Trait` no es un tipo concreto para Vec.**

`Vec<impl Fn()>` no es sintaxis valida. `impl Fn()` es un bound, no un
tipo. Ademas, cada closure tiene tipo distinto — no caben en el mismo
Vec.

Fix:
```rust
let closures: Vec<Box<dyn Fn()>> = vec![
    Box::new(|| println!("a")),
    Box::new(|| println!("b")),
];
```

**Error C: Closure captura referencia a local.**

`name` es local de `make_fn`. La closure captura `&name`, pero `name`
se destruye al retornar.

Fix: `move`.
```rust
fn make_fn() -> impl Fn() {
    let name = String::from("hello");
    move || println!("{}", name)
}
```

</details>

---

### Ejercicio 10 — Reflexion: closures como patron universal

Responde sin ver la respuesta:

1. En S01/T01 viste que C no puede retornar "composiciones permanentes"
   de funciones. Rust puede, gracias a closures. Que mecanismo interno
   lo permite (que tiene Rust que C no tiene)?

2. Un `Box<dyn Fn(i32) -> i32>` almacena una closure con su contexto
   en heap. En C, el equivalente seria un struct con function pointer +
   datos + destructor. Cuantas piezas separadas gestiona C que Rust
   unifica en un solo valor?

3. Los closures de Rust implementan Fn/FnMut/FnOnce automaticamente.
   Si tuvieras que implementar esto en C, como lo harias? (Piensa en
   vtable + struct de capturas.)

<details><summary>Respuesta</summary>

**1. Que mecanismo lo permite:**

La capacidad de generar **tipos anonimos con datos** en compile time.
Cada closure es un struct anonimo que contiene las variables capturadas.
El compilador genera este struct, implementa el trait Fn para el, y el
sistema de ownership/lifetimes garantiza que las capturas sean validas.

C no puede porque:
- No tiene tipos anonimos (cada struct necesita nombre o typedef)
- No tiene ownership/lifetimes (no puede garantizar que las capturas
  son validas)
- No tiene un mecanismo para "generar codigo" por cada closure
  (sin macros complejas o JIT)

**2. Piezas en C vs Rust:**

En C, un "closure manual" requiere:
1. Un `typedef` para la firma del function pointer
2. Un struct para los datos capturados
3. Una funcion de "creacion" (malloc + init del struct)
4. Una funcion de "invocacion" (desempacar struct + llamar)
5. Una funcion de "destruccion" (free del struct)
6. El `void *ctx` en cada API que lo use

En Rust, `Box<dyn Fn(i32) -> i32>` unifica todo:
1. El tipo de la closure (anonimo, generado)
2. Los datos capturados (dentro del struct anonimo)
3. La creacion (`Box::new(|| ...)`)
4. La invocacion (`f(42)`)
5. La destruccion (`Drop` automatico)
6. No necesita `void *ctx`

**6 piezas separadas → 1 valor.**

**3. Closures en C (vtable + struct):**

```c
// "Closure" manual en C
typedef struct {
    int (*call)(void *self, int arg);   // vtable de 1 metodo
    void (*drop)(void *self);           // destructor
} ClosureVtable;

typedef struct {
    const ClosureVtable *vt;
    // datos capturados van aqui (o en memoria extra)
} Closure;

// Cada "closure" concreta seria:
typedef struct {
    Closure base;
    int offset;    // variable capturada
} AddOffsetClosure;

int add_offset_call(void *self, int arg) {
    AddOffsetClosure *c = (AddOffsetClosure *)self;
    return arg + c->offset;
}

void add_offset_drop(void *self) { free(self); }

static const ClosureVtable add_offset_vt = {
    .call = add_offset_call,
    .drop = add_offset_drop,
};
```

Esto es exactamente una vtable de un solo metodo (`call`) con un
struct de capturas — lo que Rust genera automaticamente por cada
`|x| x + offset`. El Fn/FnMut/FnOnce serian tres vtables con
`call(&self)`, `call_mut(&mut self)`, y `call_once(self)`.

</details>
