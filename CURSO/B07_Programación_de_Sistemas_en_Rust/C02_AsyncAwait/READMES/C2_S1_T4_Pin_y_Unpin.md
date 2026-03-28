# Pin y Unpin

## Indice
1. [El problema: self-referential structs](#1-el-problema-self-referential-structs)
2. [Por que los Futures son self-referential](#2-por-que-los-futures-son-self-referential)
3. [Que es Pin](#3-que-es-pin)
4. [Que es Unpin](#4-que-es-unpin)
5. [Pin\<Box\<T\>\>: pinear en el heap](#5-pinboxt-pinear-en-el-heap)
6. [pin! macro: pinear en el stack](#6-pin-macro-pinear-en-el-stack)
7. [Pin en la practica: cuando te importa](#7-pin-en-la-practica-cuando-te-importa)
8. [Implementar Future con Pin](#8-implementar-future-con-pin)
9. [Pin projection](#9-pin-projection)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema: self-referential structs

Para entender Pin, primero hay que entender el problema que
resuelve. Considera un struct que contiene un campo que
apunta a otro campo del mismo struct:

```rust
struct SelfRef {
    value: String,
    ptr: *const String,  // apunta a self.value
}

impl SelfRef {
    fn new(text: &str) -> Self {
        let mut s = SelfRef {
            value: text.to_string(),
            ptr: std::ptr::null(),
        };
        s.ptr = &s.value as *const String;
        s
    }

    fn get_ref(&self) -> &String {
        unsafe { &*self.ptr }
    }
}
```

Esto parece funcionar, pero tiene un bug fatal:

```rust
let mut a = SelfRef::new("hello");
let b = a;  // MOVE: 'a' se mueve a 'b'

// a.value se copio a b.value (nueva direccion en memoria)
// PERO b.ptr todavia apunta a la VIEJA direccion de a.value
// b.ptr es un dangling pointer!

println!("{}", b.get_ref());  // UNDEFINED BEHAVIOR
```

```
    Antes del move:                 Despues del move:
    a: SelfRef                      a: (invalidado)
    +------------------+
    | value: "hello"   | <--+       b: SelfRef
    | ptr:   ----------|-+  |      +------------------+
    +------------------+ |  |      | value: "hello"   |
                         |  |      | ptr:   ----------|---> ??? (a.value ya no existe!)
                         +--+      +------------------+
    ptr apunta a value OK          ptr apunta a memoria invalida
```

**El problema**: en Rust, cualquier valor puede moverse en
memoria (asignacion, paso a funciones, retorno, etc.). Si un
struct tiene punteros internos a si mismo, moverlo los invalida.

---

## 2. Por que los Futures son self-referential

El compilador genera structs self-referential cuando una
variable local **referencia** a otra variable local a traves
de un `.await`:

```rust
async fn problematic() {
    let data = vec![1, 2, 3];
    let reference = &data;       // referencia a data
    some_io().await;             // punto de suspension
    println!("{:?}", reference); // usa reference despues del await
}
```

El compilador genera un enum como:

```rust
enum ProblematicFuture {
    // Despues del primer await, necesita guardar AMBOS:
    State1 {
        data: Vec<i32>,          // el valor
        reference: &Vec<i32>,    // referencia a data ^^^ MISMO STRUCT!
        io_future: SomeIoFuture,
    },
    // ...
}
```

`reference` apunta a `data`, que esta en el mismo enum.
Si este enum se moviera en memoria, `reference` seria un
dangling pointer.

```
    ProblematicFuture (State1):
    +-----------------------+
    | data: [1, 2, 3]       | <---+
    | reference: -----------|-----+  (apunta a data, mismo struct)
    | io_future: ...        |
    +-----------------------+

    Si se mueve, reference apunta a la vieja ubicacion = UB
```

**Pin resuelve esto**: garantiza que el Future **no se mueve**
despues de ser poleado por primera vez.

---

## 3. Que es Pin

`Pin<P>` es un wrapper alrededor de un puntero `P` (como
`&mut T` o `Box<T>`) que **garantiza** que el valor apuntado
no se movera en memoria:

```rust
use std::pin::Pin;

// Pin<&mut T>: referencia mutable que promete no mover T
// Pin<Box<T>>: box que promete no mover T
```

### La firma de Future::poll

```rust
trait Future {
    type Output;
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>;
    //      ^^^^^^^^^^^^^^^^
    //      "self esta pineado: no me muevas"
}
```

El runtime recibe un `Pin<&mut Future>`, que le permite pollear
el Future pero le **prohibe** moverlo. Esto protege las
auto-referencias internas.

### Que garantiza Pin

```
    Sin Pin:                         Con Pin:
    let mut future = ...;           let mut future = ...;
    let pinned = Pin::new(&mut f);  // (solo si T: Unpin)
                                    // o Pin::new_unchecked

    let moved = future;             // NO PERMITIDO: no puedes sacar
    // future se movio!             // el &mut T de un Pin<&mut T>
    // auto-referencias rotas       // para moverlo
```

Pin **no** previene el movimiento fisicamente — no puede,
es una abstraccion de tipos. Lo que hace es **no exponer**
una `&mut T` desde la cual podrias hacer `std::mem::swap` o
mover el valor.

### La API de Pin

```rust
impl<P: Deref> Pin<P> {
    // Crear Pin (solo si T: Unpin — ver seccion 4)
    pub fn new(pointer: P) -> Pin<P>
    where P::Target: Unpin { ... }

    // Crear Pin sin verificar Unpin (unsafe)
    pub unsafe fn new_unchecked(pointer: P) -> Pin<P> { ... }
}

impl<T: ?Sized> Pin<&mut T> {
    // Obtener referencia compartida: siempre OK
    pub fn as_ref(&self) -> Pin<&T> { ... }

    // Obtener &mut T: solo si T: Unpin
    pub fn get_mut(self) -> &mut T
    where T: Unpin { ... }

    // Obtener &mut T sin verificar (unsafe)
    pub unsafe fn get_unchecked_mut(self) -> &mut T { ... }
}
```

---

## 4. Que es Unpin

`Unpin` es un auto-trait que dice: "este tipo es seguro de
mover incluso estando pineado". Es decir, `Pin` no tiene
efecto sobre tipos `Unpin`:

```rust
// La mayoria de tipos son Unpin:
// i32: Unpin
// String: Unpin
// Vec<T>: Unpin
// Box<T>: Unpin
// &T: Unpin
// &mut T: Unpin

// Los tipos que NO son Unpin:
// Los Futures generados por async fn (pueden ser self-referential)
// Tipos que contienen PhantomPinned
```

### Unpin significa "Pin no importa"

```rust
use std::pin::Pin;

fn example() {
    let mut x: i32 = 42;

    // i32 es Unpin, asi que Pin no hace nada especial:
    let pinned: Pin<&mut i32> = Pin::new(&mut x);

    // Puedes obtener &mut libremente:
    let r: &mut i32 = Pin::into_inner(pinned);
    *r = 100;

    // Puedes mover x libremente — Pin no lo impide
    let y = x;  // OK: i32 es Unpin
}
```

### !Unpin: Pin realmente importa

```rust
use std::marker::PhantomPinned;

struct NotUnpin {
    data: String,
    _pin: PhantomPinned,  // hace el struct !Unpin
}

fn example() {
    let mut x = NotUnpin {
        data: "hello".to_string(),
        _pin: PhantomPinned,
    };

    // NO puedes crear Pin::new porque NotUnpin: !Unpin
    // let pinned = Pin::new(&mut x);  // ERROR

    // Necesitas unsafe:
    let pinned = unsafe { Pin::new_unchecked(&mut x) };

    // NO puedes obtener &mut:
    // let r = pinned.get_mut();  // ERROR: NotUnpin: !Unpin
}
```

### Tabla resumen

```
+------------+---------------------------+---------------------------+
| Tipo       | Unpin?                    | Pin tiene efecto?         |
+------------+---------------------------+---------------------------+
| i32        | Si (auto)                 | No (ignorado)             |
| String     | Si (auto)                 | No (ignorado)             |
| Vec<T>     | Si (auto)                 | No (ignorado)             |
| Box<T>     | Si (auto)                 | No (ignorado)             |
| async { }  | No (puede ser self-ref)   | Si (previene move)        |
| PhantomPinned | No (opt-out explicito) | Si (previene move)        |
+------------+---------------------------+---------------------------+
```

---

## 5. Pin\<Box\<T\>\>: pinear en el heap

La forma mas comun de pinear un valor es ponerlo en el heap
con `Box::pin`:

```rust
use std::pin::Pin;

async fn my_async_fn() -> i32 { 42 }

fn example() -> Pin<Box<dyn Future<Output = i32>>> {
    Box::pin(my_async_fn())
}
```

### Por que Box::pin funciona

```
    Stack:                    Heap:
    +------------------+     +------------------+
    | Pin<Box<Future>>  |---->| Future (pineado) |
    | (puede moverse)  |     | (NO se mueve)    |
    +------------------+     +------------------+

    El Box (puntero) puede moverse en el stack.
    Pero el Future en el heap NO se mueve.
    Las auto-referencias dentro del Future son estables.
```

Mover el `Pin<Box<T>>` mueve el puntero, no el valor.
El valor permanece en la misma direccion del heap.

### Uso comun: type alias BoxFuture

```rust
use std::pin::Pin;
use std::future::Future;

type BoxFuture<'a, T> = Pin<Box<dyn Future<Output = T> + Send + 'a>>;

// Util para:
// 1. Retornar diferentes Futures del mismo tipo
fn choose(flag: bool) -> BoxFuture<'static, i32> {
    if flag {
        Box::pin(async { 1 })
    } else {
        Box::pin(async { 2 })
    }
}

// 2. Recursion async (visto en T02)
fn recursive(n: u32) -> BoxFuture<'static, u32> {
    Box::pin(async move {
        if n == 0 { 0 } else { n + recursive(n - 1).await }
    })
}

// 3. dyn Future en traits
trait Service {
    fn call(&self) -> BoxFuture<'_, String>;
}
```

---

## 6. pin! macro: pinear en el stack

Desde Rust 1.68, `std::pin::pin!` permite pinear en el
stack sin heap allocation:

```rust
use std::pin::pin;
use std::future::Future;

async fn example() {
    let future = pin!(async {
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
        42
    });

    // future es Pin<&mut impl Future<Output = i32>>
    // Pineado en el stack — sin Box, sin heap allocation
}
```

### Antes de pin!: el truco unsafe

```rust
// Antes de Rust 1.68, tenias que hacer esto:
let mut future = async { 42 };
// SAFETY: future no se mueve despues de esta linea
let future = unsafe { Pin::new_unchecked(&mut future) };
```

`pin!` hace esto de forma segura — garantiza que el valor
no se mueve porque la macro toma ownership y no te devuelve
el valor original.

### pin! vs Box::pin

```
+------------------+-------------------+---------------------+
| Aspecto          | pin!              | Box::pin             |
+------------------+-------------------+---------------------+
| Allocation       | Stack (gratis)    | Heap (allocation)    |
| Tipo resultado   | Pin<&mut T>       | Pin<Box<T>>          |
| Puede retornarse | No (referencia    | Si (owned)           |
|                  | local)            |                      |
| Uso tipico       | Dentro de una fn  | Retornar, almacenar  |
|                  | (temporal)        | en colecciones       |
+------------------+-------------------+---------------------+
```

### Uso con select!

`pin!` es util con `select!` para pinear Futures en el stack:

```rust
use std::pin::pin;
use tokio::time::{sleep, Duration};

async fn example() {
    let sleep1 = pin!(sleep(Duration::from_secs(1)));
    let sleep2 = pin!(sleep(Duration::from_secs(2)));

    tokio::select! {
        _ = sleep1 => println!("1 second"),
        _ = sleep2 => println!("2 seconds"),
    }
}
```

---

## 7. Pin en la practica: cuando te importa

La buena noticia: **la mayoria del tiempo no necesitas pensar
en Pin**. `async/await` maneja todo automaticamente:

```rust
// Esto funciona sin pensar en Pin:
async fn normal_code() {
    let data = fetch_data().await;
    process(data).await;
}
```

### Cuando SI necesitas Pin

**1. Implementar Future manualmente:**

```rust
impl Future for MyFuture {
    type Output = i32;
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<i32> {
        //     ^^^^^^^^^^^^^^^^ recibes un Pin
        // ...
    }
}
```

**2. Almacenar Futures en colecciones:**

```rust
use std::pin::Pin;
use std::future::Future;

// Necesitas Pin<Box<dyn Future>> para colecciones heterogeneas
let mut futures: Vec<Pin<Box<dyn Future<Output = i32>>>> = vec![
    Box::pin(async { 1 }),
    Box::pin(async { 2 }),
    Box::pin(async { 3 }),
];
```

**3. Escribir combinadores de Futures:**

```rust
// Un combinador que ejecuta dos Futures concurrentemente
struct Join<A, B> {
    a: Pin<Box<A>>,
    b: Pin<Box<B>>,
    a_done: Option<A::Output>,
    b_done: Option<B::Output>,
}
```

**4. Pasar Futures a funciones que esperan Pin:**

Algunas APIs requieren `Pin<&mut F>`:

```rust
use std::pin::pin;

async fn example() {
    let mut future = pin!(some_async_fn());
    // Ahora puedes pasarlo a APIs que esperan Pin<&mut F>
    poll_fn(|cx| future.as_mut().poll(cx)).await;
}
```

### Cuando NO necesitas Pin

- Usar `async/await` normalmente
- `tokio::spawn`, `join!`, `select!` — manejan Pin internamente
- Llamar funciones async y hacer `.await`
- La gran mayoria del codigo async diario

---

## 8. Implementar Future con Pin

Cuando implementas `Future` manualmente, recibes
`Pin<&mut Self>`. Si tu struct es `Unpin`, puedes acceder
a `&mut self` libremente:

### Caso simple: struct Unpin

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct Counter {
    count: u32,
    target: u32,
}

// Counter es Unpin automaticamente (todos sus campos son Unpin)
// Asi que Pin<&mut Counter> se comporta como &mut Counter

impl Future for Counter {
    type Output = u32;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<u32> {
        self.count += 1;  // acceso directo — Counter es Unpin
        if self.count >= self.target {
            Poll::Ready(self.count)
        } else {
            cx.waker().wake_by_ref();
            Poll::Pending
        }
    }
}
```

### Caso complejo: struct !Unpin

Si tu struct contiene un Future interno (que es !Unpin),
necesitas mas cuidado:

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct Timeout<F> {
    future: F,      // F es !Unpin si es un Future async
    deadline: std::time::Instant,
}

// Timeout<F> es !Unpin si F es !Unpin

impl<F: Future> Future for Timeout<F> {
    type Output = Option<F::Output>;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if std::time::Instant::now() >= self.deadline {
            return Poll::Ready(None);
        }

        // Para acceder a self.future como Pin<&mut F>,
        // necesitamos "pin projection" (ver seccion 9)
        // SAFETY: no movemos self.future fuera de self
        let future = unsafe { self.map_unchecked_mut(|s| &mut s.future) };
        match future.poll(cx) {
            Poll::Ready(v) => Poll::Ready(Some(v)),
            Poll::Pending => Poll::Pending,
        }
    }
}
```

---

## 9. Pin projection

**Pin projection** es acceder a un campo de un struct pineado
de forma segura. Si tienes `Pin<&mut Struct>`, como accedes
a `Pin<&mut Struct.field>`?

### El problema

```rust
struct MyFuture {
    inner: SomeOtherFuture,  // !Unpin
    count: u32,              // Unpin
}

impl Future for MyFuture {
    type Output = ();

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        // Quiero acceder a self.inner como Pin<&mut SomeOtherFuture>
        // y a self.count como &mut u32
        // Como?
    }
}
```

### Solucion manual (unsafe)

```rust
impl Future for MyFuture {
    type Output = ();

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        // SAFETY: no movemos inner fuera de self.
        // count es Unpin, asi que podemos acceder libremente.
        let this = unsafe { self.get_unchecked_mut() };

        // count es Unpin — acceso directo OK
        this.count += 1;

        // inner es !Unpin — re-pinear para pollear
        let inner = unsafe { Pin::new_unchecked(&mut this.inner) };
        inner.poll(cx)
    }
}
```

### Solucion con pin-project (crate)

El crate `pin-project` automatiza la projection de forma segura:

```rust
use pin_project::pin_project;
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

#[pin_project]
struct MyFuture<F> {
    #[pin]      // este campo se proyecta como Pin<&mut F>
    inner: F,
    count: u32, // sin #[pin] — se proyecta como &mut u32
}

impl<F: Future> Future for MyFuture<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<F::Output> {
        let this = self.project();  // pin projection segura

        *this.count += 1;           // &mut u32
        this.inner.poll(cx)         // Pin<&mut F>
    }
}
```

```
    #[pin_project] genera un metodo project() que retorna:

    Pin<&mut MyFuture<F>>
         |
         v project()
    MyFutureProjection {
        inner: Pin<&mut F>,   // campo marcado con #[pin]
        count: &mut u32,      // campo sin #[pin]
    }
```

### Reglas de pin projection

```
+-----------------------------+----------------------------------+
| Campo marcado #[pin]        | Campo sin #[pin]                 |
+-----------------------------+----------------------------------+
| Se proyecta como Pin<&mut F>| Se proyecta como &mut T          |
| Para campos !Unpin (Futures)| Para campos Unpin (datos simples)|
| No se puede mover           | Se puede mover                   |
+-----------------------------+----------------------------------+
```

---

## 10. Errores comunes

### Error 1: intentar mover un Future despues de pinearlo

```rust
use std::pin::pin;

async fn example() {
    let fut = pin!(async { 42 });

    // MAL: no puedes mover un valor pineado
    // let moved = fut;  // ERROR: cannot move out of `fut`

    // BIEN: usar el Future in-place
    let result = fut.await;
}
```

Una vez pineado, el valor no se puede mover. Usa `Box::pin`
si necesitas moverlo (el Box se mueve, el valor no).

### Error 2: confundir Pin con inmutabilidad

```rust
use std::pin::Pin;

fn example() {
    let mut x = 42i32;
    let pinned = Pin::new(&mut x);

    // Pin NO hace inmutable — i32 es Unpin
    // Puedes obtener &mut y modificar:
    let r = Pin::into_inner(pinned);
    *r = 100;
}
```

Pin **no** es inmutabilidad. Pin dice "no me **muevas** a otra
ubicacion en memoria", no "no me modifiques".

### Error 3: unsafe Pin::new_unchecked sin garantizar no-move

```rust
// MAL: crear Pin y luego mover el valor
let mut data = MyFuture::new();
let pinned = unsafe { Pin::new_unchecked(&mut data) };
// pollear pinned...
let moved = data;  // MOVER DESPUES DE PINEAR = UB
// Las auto-referencias de data son invalidas
```

```rust
// BIEN: usar Box::pin (el valor nunca se mueve del heap)
let pinned = Box::pin(MyFuture::new());
// El Future vive en el heap, el Box puede moverse
```

**Regla**: si usas `Pin::new_unchecked`, eres responsable de
nunca mover el valor. Prefiere `Box::pin` o `pin!`.

### Error 4: agregar PhantomPinned sin necesidad

```rust
use std::marker::PhantomPinned;

// MAL: no necesitas PhantomPinned si no tienes auto-referencias
struct SimpleData {
    name: String,
    age: u32,
    _pin: PhantomPinned,  // innecesario y molesto
}
// Ahora SimpleData es !Unpin y necesitas Pin para todo
```

```rust
// BIEN: solo tipos simples — automaticamente Unpin
struct SimpleData {
    name: String,
    age: u32,
}
// Unpin automatico — Pin no tiene efecto
```

**Regla**: solo usa `PhantomPinned` si tu struct tiene
auto-referencias reales que se rompen al mover.

### Error 5: olvidar que async blocks son !Unpin

```rust
fn example() {
    let future = async {
        let x = 42;
        some_io().await;
        println!("{}", x);  // x cruza await → self-referential
    };

    // future es !Unpin — no puedes hacer Pin::new
    // let pinned = Pin::new(&mut future);  // ERROR

    // BIEN: usar Box::pin o pin!
    let pinned = Box::pin(future);
}
```

---

## 11. Cheatsheet

```
PIN Y UNPIN - REFERENCIA RAPIDA
=================================

El problema:
  Async Futures pueden ser self-referential (campo referencia a campo)
  Mover un self-referential struct invalida las auto-referencias
  Pin previene el movimiento

Pin<P> (donde P es un puntero a T):
  Pin<&mut T>    referencia mutable pineada
  Pin<Box<T>>    box pineado (valor en heap)
  Garantiza: T no se mueve mientras este pineado

Unpin (auto trait):
  T: Unpin     "Pin no importa, puedes moverme libremente"
  Casi todo es Unpin: i32, String, Vec, Box, &T, &mut T
  !Unpin: async {}, tipos con PhantomPinned

Crear Pin:
  Pin::new(&mut t)              solo si T: Unpin
  Box::pin(value)               heap, siempre funciona
  pin!(value)                   stack (Rust 1.68+), no se puede retornar
  unsafe Pin::new_unchecked()   tu garantizas no-move

Acceder al valor:
  pin.as_ref()                  -> Pin<&T>     (siempre OK)
  pin.get_mut()                 -> &mut T      (solo si T: Unpin)
  unsafe pin.get_unchecked_mut()-> &mut T      (tu responsabilidad)

Pin projection:
  Acceder a campos de un struct pineado
  Manual: unsafe { self.get_unchecked_mut() }
  Con crate: #[pin_project] + #[pin] en campos !Unpin

Cuando te importa Pin:
  Implementar Future manualmente
  Almacenar Futures en colecciones
  Escribir combinadores de Futures
  Usar APIs que requieren Pin<&mut F>

Cuando NO te importa:
  Usar async/await normalmente
  tokio::spawn, join!, select!
  La gran mayoria del codigo async diario

Reglas de oro:
  1. Para el 95% del codigo async: ignora Pin, usa async/await
  2. Si necesitas Pin: usa Box::pin() o pin!()
  3. Si implementas Future manual: usa pin-project crate
  4. Nunca uses Pin::new_unchecked sin entender las garantias
  5. Pin != inmutable. Pin = "no me muevas de ubicacion"
```

---

## 12. Ejercicios

### Ejercicio 1: Pin con tipos Unpin vs !Unpin

Experimenta con Pin y Unpin para entender la diferencia:

```rust
use std::pin::Pin;
use std::marker::PhantomPinned;

struct Movable {
    data: String,
}

struct NotMovable {
    data: String,
    _pin: PhantomPinned,
}

fn main() {
    // Parte 1: tipo Unpin
    let mut m = Movable { data: "hello".into() };

    // TODO: crear Pin::new(&mut m) — funciona?
    // TODO: obtener &mut con pin.get_mut() — funciona?
    // TODO: mover m despues de crear el Pin — funciona?
    //       (pista: el Pin se droppea antes del move)

    // Parte 2: tipo !Unpin
    let mut nm = NotMovable {
        data: "hello".into(),
        _pin: PhantomPinned,
    };

    // TODO: intentar Pin::new(&mut nm) — funciona?
    // TODO: intentar crear con Pin::new_unchecked (unsafe)
    // TODO: intentar get_mut() — funciona?

    // Parte 3: Box::pin
    let pinned = Box::pin(NotMovable {
        data: "hello".into(),
        _pin: PhantomPinned,
    });

    // TODO: acceder a pinned.data via as_ref()
    // TODO: intentar get_mut() — funciona?
    // TODO: mover pinned (el Box, no el valor) — funciona?
}
```

Tareas:
1. Completa cada TODO y anota si compila o no
2. Para cada caso que no compila, explica por que
3. Demuestra que `Box::pin` se puede mover pero el valor
   interno no

**Prediccion**: puedes mover un `Pin<Box<T>>` a otra variable?
Y un `Pin<&mut T>`?

> **Pregunta de reflexion**: `Vec<T>` es `Unpin` aunque sus
> datos viven en el heap y pueden moverse (cuando el Vec crece
> y realoca). Por que es seguro? La clave es que Vec no tiene
> punteros internos a si mismo — los datos estan en el heap y
> el Vec solo tiene un puntero al heap. Mover el Vec mueve
> el puntero, no los datos. Como se compara esto con el
> problema de los Futures self-referential?

### Ejercicio 2: implementar un Future con pin-project

Implementa un combinador `WithLogging` que wrappea un Future
y loguea cuando se polea y cuando termina:

```rust
// Cargo.toml: pin-project = "1"
use pin_project::pin_project;
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

#[pin_project]
struct WithLogging<F> {
    #[pin]
    inner: F,
    name: String,
    poll_count: u32,
}

impl<F> WithLogging<F> {
    fn new(name: impl Into<String>, future: F) -> Self {
        WithLogging {
            inner: future,
            name: name.into(),
            poll_count: 0,
        }
    }
}

impl<F: Future> Future for WithLogging<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<F::Output> {
        let this = self.project();

        *this.poll_count += 1;
        println!("[{}] poll #{}", this.name, this.poll_count);

        // TODO: pollear this.inner y, si retorna Ready,
        // imprimir "[name] completed after N polls"
        todo!()
    }
}

#[tokio::main]
async fn main() {
    let result = WithLogging::new("my-task", async {
        tokio::time::sleep(std::time::Duration::from_millis(100)).await;
        42
    }).await;

    println!("Result: {}", result);
    // Esperado:
    // [my-task] poll #1
    // [my-task] poll #2
    // [my-task] completed after 2 polls
    // Result: 42
}
```

Tareas:
1. Completa el metodo `poll`
2. Ejecuta y observa cuantas veces se polea
3. Sin `pin-project`, intenta hacer la projection manualmente
   con unsafe — aprecia la ergonomia del crate
4. Agrega un timestamp a cada log para ver el timing entre polls

> **Pregunta de reflexion**: `#[pin]` en `inner` genera una
> projection a `Pin<&mut F>`, mientras que `name` y `poll_count`
> se proyectan como `&mut String` y `&mut u32`. Por que es
> seguro acceder a `name` como `&mut` (no pineado) pero `inner`
> debe ser `Pin<&mut F>`? Que pasaria si marcaras `name` con
> `#[pin]`? Funcionaria, pero seria innecesario — por que?

### Ejercicio 3: Box::pin para Futures heterogeneos

Crea una coleccion de Futures de diferentes tipos y ejecutalos:

```rust
use std::future::Future;
use std::pin::Pin;

type BoxFuture<T> = Pin<Box<dyn Future<Output = T> + Send>>;

async fn fast() -> String {
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;
    "fast".to_string()
}

async fn medium() -> String {
    tokio::time::sleep(std::time::Duration::from_millis(500)).await;
    "medium".to_string()
}

async fn slow() -> String {
    tokio::time::sleep(std::time::Duration::from_secs(1)).await;
    "slow".to_string()
}

#[tokio::main]
async fn main() {
    // Crear vector de Futures heterogeneos
    let futures: Vec<BoxFuture<String>> = vec![
        Box::pin(fast()),
        Box::pin(medium()),
        Box::pin(slow()),
    ];

    // TODO: ejecutar todos concurrentemente y recoger resultados
    // Opcion A: con JoinSet + spawn
    // Opcion B: con FuturesUnordered del crate futures

    // Preguntas:
    // - Por que necesitas Box::pin y no puedes hacer Vec<impl Future>?
    // - Que pasa si intentas usar pin!() en vez de Box::pin()?
}
```

Tareas:
1. Ejecuta los Futures concurrentemente (usa el enfoque que
   prefieras)
2. Imprime cada resultado conforme llega (no esperar a todos)
3. Agrega un Future que retorna un tipo diferente — necesitas
   otro Vec? O puedes adaptar?
4. Intenta reemplazar `Box::pin` con `pin!` — compila?
   Explica por que no

> **Pregunta de reflexion**: `Box::pin` tiene un coste: una
> heap allocation por Future. En un servidor con millones de
> Futures, esto puede importar. `pin!` evita la allocation
> pero no se puede usar en colecciones (produce `Pin<&mut T>`,
> una referencia local). Hay un tradeoff entre flexibilidad
> (Box::pin, heap) y rendimiento (pin!, stack). En la practica,
> la allocation de Box es ~nanosegundos. Para que escala
> empezaria a ser un problema medible?
