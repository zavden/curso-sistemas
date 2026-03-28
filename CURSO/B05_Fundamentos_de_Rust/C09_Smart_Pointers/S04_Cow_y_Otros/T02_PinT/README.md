# Pin\<T\>: por qué existe y cómo funciona

## Índice
1. [El problema: structs auto-referenciales](#1-el-problema-structs-auto-referenciales)
2. [Qué es Pin\<P\>](#2-qué-es-pinp)
3. [Unpin: el trait de escape](#3-unpin-el-trait-de-escape)
4. [Pin en la práctica](#4-pin-en-la-práctica)
5. [Self-referential structs paso a paso](#5-self-referential-structs-paso-a-paso)
6. [Pin y async/Future](#6-pin-y-asyncfuture)
7. [APIs que requieren Pin](#7-apis-que-requieren-pin)
8. [pin! macro y pin_mut!](#8-pin-macro-y-pin_mut)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema: structs auto-referenciales

Para entender `Pin`, primero hay que entender el problema que resuelve. Consideremos una estructura que contiene un dato y un puntero que apunta *dentro de sí misma*:

```rust
struct SelfRef {
    data: String,
    ptr: *const String,  // apunta a `data`
}
```

Después de inicializar `ptr` para que apunte a `data`, todo funciona — *mientras la estructura no se mueva en memoria*:

```rust
fn main() {
    let mut s = SelfRef {
        data: String::from("hello"),
        ptr: std::ptr::null(),
    };
    s.ptr = &s.data as *const String;

    // Funciona: ptr apunta a data
    unsafe {
        println!("{}", *s.ptr); // "hello"
    }

    // PROBLEMA: movemos s a otra ubicación
    let s2 = s;  // s se mueve, ptr sigue apuntando a la ubicación VIEJA

    unsafe {
        // ¡¡UNDEFINED BEHAVIOR!!
        // s2.ptr apunta a memoria inválida — s ya no existe ahí
        println!("{}", *s2.ptr);
    }
}
```

El problema es fundamental en Rust: **mover** un valor (`let s2 = s`, pasar a una función, retornar, hacer `swap`, `Vec::push` que realoja) copia los bytes a una nueva dirección. Si el valor contiene punteros hacia sí mismo, esos punteros quedan *dangling*.

### ¿Por qué Rust no prohíbe los movimientos?

Rust mueve valores constantemente — es la base de su sistema de ownership. Prohibir todos los movimientos rompería el lenguaje. La solución es más sutil: **permitir que ciertos valores declaren "no me muevas"**, y proporcionar una API segura alrededor de esta garantía.

Eso es exactamente lo que hace `Pin`.

---

## 2. Qué es Pin\<P\>

`Pin<P>` es un wrapper alrededor de un puntero `P` (típicamente `P = &mut T`, `Box<T>`, `Rc<T>`, `Arc<T>`) que **garantiza que el valor apuntado no será movido de su ubicación en memoria**.

```rust
use std::pin::Pin;

// Pin envuelve un puntero, NO el valor directamente
let pinned: Pin<&mut MyStruct> = ...;
let pinned: Pin<Box<MyStruct>> = ...;
```

### La definición simplificada

```rust
// En la std (simplificada):
pub struct Pin<Ptr> {
    pointer: Ptr,  // campo privado — nadie puede extraer Ptr
}
```

El campo es **privado**. Esto es crucial: si pudieras extraer el `&mut T` interno, podrías usar `std::mem::swap` o `std::mem::replace` para mover el valor.

### Las garantías de Pin

1. **No se puede obtener `&mut T`** del `Pin<&mut T>` (excepto con `unsafe` o si `T: Unpin`).
2. **No se puede mover el valor** apuntado fuera del Pin.
3. **El valor permanece en su dirección de memoria** hasta que es `drop`eado.

### Crear un Pin

```rust
use std::pin::Pin;

// Desde Box (Pin toma ownership del heap allocation):
let boxed = Box::new(42);
let pinned: Pin<Box<i32>> = Box::pin(42);
// Equivalente a: Pin::new(Box::new(42)) — funciona porque i32: Unpin

// Desde una referencia mutable (solo si T: Unpin):
let mut val = 42;
let pinned: Pin<&mut i32> = Pin::new(&mut val);

// Con la macro pin! (stack pinning, desde Rust 1.68):
let pinned: Pin<&mut i32> = std::pin::pin!(42);
```

---

## 3. Unpin: el trait de escape

La mayoría de tipos en Rust **no necesitan** la garantía de Pin — no contienen auto-referencias. Para estos tipos, `Pin` es transparente y no añade restricciones:

```rust
// La mayoría de tipos implementan Unpin automáticamente:
// i32, String, Vec<T>, HashMap<K,V>, Box<T>, &T, etc.
// Todos son Unpin.
```

### Qué significa Unpin

```rust
pub auto trait Unpin {}
// auto trait: se implementa automáticamente para casi todo
```

Si `T: Unpin`, entonces `Pin<&mut T>` se comporta **exactamente igual** que `&mut T`:

```rust
use std::pin::Pin;

let mut value = String::from("hello");
let mut pinned = Pin::new(&mut value);  // OK: String es Unpin

// Podemos obtener &mut T libremente:
let r: &mut String = pinned.as_mut().get_mut();
r.push_str(" world");

// Podemos mover el valor:
let moved = Pin::into_inner(pinned);
```

### Qué tipos NO son Unpin

Muy pocos tipos optan por no ser `Unpin`:

```rust
use std::marker::PhantomPinned;

struct NotUnpin {
    data: String,
    _pin: PhantomPinned,  // hace que el tipo sea !Unpin
}

// PhantomPinned es el mecanismo estándar para declarar !Unpin
```

Los tipos `!Unpin` más importantes:

| Tipo | Por qué es `!Unpin` |
|------|---------------------|
| Futures generados por `async fn` | Pueden contener auto-referencias entre `.await` |
| `PhantomPinned` | Marcador explícito |
| Structs con `PhantomPinned` | Se propaga por composición |
| Algunos tipos de `futures` crate | Internamente auto-referenciales |

### El flujo de decisión

```
¿T: Unpin?
├── Sí → Pin<&mut T> ≈ &mut T (sin restricciones)
└── No → Pin<&mut T> impide acceso a &mut T
         └── Solo puedes acceder con unsafe (si sabes que no moverás T)
```

---

## 4. Pin en la práctica

### Métodos de Pin

```rust
use std::pin::Pin;

// === Crear ===
Pin::new(ptr)           // solo si T: Unpin
unsafe { Pin::new_unchecked(ptr) }  // para !Unpin

// === Acceder al valor ===
let pinned: Pin<&mut T> = ...;

// Referencia inmutable — siempre permitida:
let r: &T = pinned.as_ref().get_ref();

// Referencia mutable — solo si T: Unpin:
let r: &mut T = pinned.as_mut().get_mut();  // requiere T: Unpin

// Referencia mutable unsafe — para !Unpin:
let r: &mut T = unsafe { pinned.as_mut().get_unchecked_mut() };

// === Extraer el puntero ===
Pin::into_inner(pinned)  // solo si T: Unpin

// === Mapear a un campo (pin projection) ===
// Requiere unsafe o macros (pin-project)
```

### Pin con Box (heap pinning)

La forma más práctica de pinear un valor es en el heap con `Box::pin`:

```rust
use std::pin::Pin;
use std::marker::PhantomPinned;

struct Immovable {
    data: String,
    _pin: PhantomPinned,
}

fn main() {
    let pinned: Pin<Box<Immovable>> = Box::pin(Immovable {
        data: String::from("fixed in place"),
        _pin: PhantomPinned,
    });

    // Podemos leer:
    println!("{}", pinned.data);  // OK vía Deref

    // No podemos obtener &mut Immovable (no es Unpin):
    // pinned.as_mut().get_mut();  // ERROR de compilación

    // No podemos mover fuera del Pin<Box<>>:
    // let moved = *pinned;  // ERROR
}
```

### Stack pinning con pin!

Desde Rust 1.68, la macro `std::pin::pin!` permite pinear en el stack:

```rust
use std::pin::{Pin, pin};

let mut pinned = pin!(Immovable {
    data: String::from("on the stack"),
    _pin: PhantomPinned,
});

// pinned es Pin<&mut Immovable>
// El valor vive en el stack frame actual
// No puede ser movido mientras pinned exista
```

**Limitación importante**: el valor pineado en el stack no puede ser retornado de la función (viviría más que el frame).

```rust
fn create_pinned() -> Pin<&mut Immovable> {
    let pinned = pin!(Immovable { ... });
    pinned  // ERROR: no se puede retornar referencia a local
}

// Solución: usar Box::pin para retornar ownership
fn create_pinned() -> Pin<Box<Immovable>> {
    Box::pin(Immovable { ... })
}
```

---

## 5. Self-referential structs paso a paso

Veamos cómo crear una estructura auto-referencial segura con `Pin`:

```rust
use std::marker::PhantomPinned;
use std::pin::Pin;
use std::ptr::NonNull;

struct SelfRef {
    data: String,
    // Puntero a data (inicialmente nulo, se fija al pinear)
    ptr_to_data: Option<NonNull<String>>,
    _pin: PhantomPinned,
}

impl SelfRef {
    fn new(data: String) -> Self {
        SelfRef {
            data,
            ptr_to_data: None,
            _pin: PhantomPinned,
        }
    }

    /// Inicializa el puntero interno. Requiere que el valor ya esté pineado.
    fn init(self: Pin<&mut Self>) {
        unsafe {
            let this = self.get_unchecked_mut();
            let ptr = NonNull::from(&this.data);
            this.ptr_to_data = Some(ptr);
        }
    }

    /// Lee el dato a través del puntero interno.
    fn data_via_ptr(self: Pin<&Self>) -> &str {
        unsafe {
            let this = self.get_ref();
            match &this.ptr_to_data {
                Some(ptr) => ptr.as_ref().as_str(),
                None => panic!("SelfRef no inicializado"),
            }
        }
    }

    fn data(self: Pin<&Self>) -> &str {
        &self.get_ref().data
    }
}

fn main() {
    // 1. Crear y pinear
    let mut pinned = Box::pin(SelfRef::new(String::from("hello")));

    // 2. Inicializar la auto-referencia
    pinned.as_mut().init();

    // 3. Usar — el puntero interno es válido
    println!("Direct:  {}", pinned.as_ref().data());
    println!("Via ptr: {}", pinned.as_ref().data_via_ptr());

    // 4. No se puede mover — el compilador lo impide:
    // let moved = *pinned;  // ERROR
}
```

### Por qué funciona

1. `PhantomPinned` hace que `SelfRef` sea `!Unpin`.
2. `Box::pin` aloja en el heap y devuelve `Pin<Box<SelfRef>>`.
3. Una vez pineado, **nadie puede obtener `&mut SelfRef`** sin `unsafe`.
4. Sin `&mut SelfRef`, no se puede usar `std::mem::swap` ni mover el valor.
5. El puntero interno permanece válido porque `data` no se mueve.

---

## 6. Pin y async/Future

La razón principal por la que `Pin` existe en Rust es **async/await**. Cuando el compilador transforma una función `async fn` en una máquina de estados (un `Future`), esa máquina puede contener auto-referencias:

```rust
async fn example() {
    let data = String::from("hello");
    let reference = &data;    // referencia a `data`
    some_async_op().await;    // punto de suspensión
    println!("{}", reference); // `reference` sigue apuntando a `data`
}
```

El compilador genera algo conceptualmente similar a:

```rust
enum ExampleFuture {
    // Estado antes del .await
    State0 {
        data: String,
    },
    // Estado después del .await — contiene data Y una referencia a data
    State1 {
        data: String,
        reference: *const String,  // ¡auto-referencia!
    },
    Done,
}
```

Si este `Future` se moviera entre un `.await` y su reanudación, `reference` quedaría *dangling*. Por eso el trait `Future` requiere `Pin`:

```rust
pub trait Future {
    type Output;

    // self es Pin<&mut Self>, NO &mut Self
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>;
}
```

### Implicaciones prácticas

Para el usuario de `async/await`, Pin es invisible — el compilador y el runtime se encargan:

```rust
// Esto "simplemente funciona":
async fn fetch_data(url: &str) -> String {
    let response = reqwest::get(url).await.unwrap();
    let body = response.text().await.unwrap();
    body
}

#[tokio::main]
async fn main() {
    let data = fetch_data("https://example.com").await;
    println!("{}", data);
}
```

Solo necesitas pensar en Pin cuando:
- Implementas `Future` manualmente
- Usas `poll` directamente
- Almacenas futures en structs
- Trabajas con streams

### Implementar Future manualmente

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct Countdown {
    remaining: u32,
}

impl Future for Countdown {
    type Output = &'static str;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        if self.remaining == 0 {
            Poll::Ready("done!")
        } else {
            self.remaining -= 1;
            cx.waker().wake_by_ref();  // re-poll
            Poll::Pending
        }
    }
}

// Countdown es Unpin (no tiene auto-referencias),
// así que Pin no añade restricciones aquí.
// self.remaining -= 1 funciona directamente.
```

### Future con estado interno complejo

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct DelayedValue<F: Future> {
    inner: F,         // otro Future (posiblemente !Unpin)
    ready: bool,
}

impl<F: Future> Future for DelayedValue<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        // No podemos hacer self.inner directamente (F puede ser !Unpin)
        // Necesitamos "pin projection":
        unsafe {
            let this = self.get_unchecked_mut();
            if this.ready {
                // Proyectar Pin al campo inner
                let inner = Pin::new_unchecked(&mut this.inner);
                inner.poll(cx)
            } else {
                this.ready = true;
                cx.waker().wake_by_ref();
                Poll::Pending
            }
        }
    }
}
```

---

## 7. APIs que requieren Pin

### Future::poll (ya visto)

```rust
fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>;
```

### Stream (iterador asíncrono)

```rust
pub trait Stream {
    type Item;
    fn poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>>;
}
```

### tokio::select! y Box::pin para almacenar futures

```rust
use tokio::time::{sleep, Duration};

async fn example() {
    let fut1 = sleep(Duration::from_secs(1));
    let fut2 = sleep(Duration::from_secs(2));

    // pin! para stack-pinear (necesario para select!)
    tokio::pin!(fut1);
    tokio::pin!(fut2);

    tokio::select! {
        _ = &mut fut1 => println!("fut1 completed first"),
        _ = &mut fut2 => println!("fut2 completed first"),
    }
}
```

### Almacenar futures heterogéneos

```rust
use std::pin::Pin;
use std::future::Future;

// Vec de futures con distintos tipos concretos:
let mut futures: Vec<Pin<Box<dyn Future<Output = i32>>>> = vec![
    Box::pin(async { 1 }),
    Box::pin(async { 2 }),
    Box::pin(async { 3 }),
];
```

---

## 8. pin! macro y pin_mut!

### std::pin::pin! (estable desde Rust 1.68)

```rust
use std::pin::pin;
use std::future::Future;

async fn work() -> i32 { 42 }

fn poll_once(fut: Pin<&mut dyn Future<Output = i32>>) {
    // ...
}

fn main() {
    let fut = pin!(work());
    // fut: Pin<&mut impl Future<Output = i32>>
    // El future vive en el stack, pineado in-place
}
```

La macro expande conceptualmente a:

```rust
// pin!(expr) es equivalente a:
let mut __value = expr;
let fut = unsafe { Pin::new_unchecked(&mut __value) };
// El unsafe es seguro porque __value no se puede mover
// (la macro controla el binding)
```

### tokio::pin! (antes de Rust 1.68)

```rust
// Misma idea, distinta sintaxis:
let fut = work();
tokio::pin!(fut);
// Ahora fut es Pin<&mut _>
```

### pin-project y pin-project-lite

Para proyectar `Pin` a los campos de un struct de forma segura (sin `unsafe` manual):

```rust
use pin_project_lite::pin_project;

pin_project! {
    struct TimedFuture<F> {
        #[pin]    // este campo se proyecta como Pin<&mut F>
        inner: F,
        started: bool,  // este campo se proyecta como &mut bool
    }
}

use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

impl<F: Future> Future for TimedFuture<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let this = self.project();
        // this.inner: Pin<&mut F>  — pineado, seguro
        // this.started: &mut bool  — acceso directo, no pineado

        if !*this.started {
            *this.started = true;
            println!("Future polled for the first time");
        }

        this.inner.poll(cx)
    }
}
```

La macro `pin_project!` genera el código unsafe necesario y verifica en compilación que la proyección sea segura. Reglas que impone:

- Campos `#[pin]` se proyectan como `Pin<&mut FieldType>`.
- Campos sin `#[pin]` se proyectan como `&mut FieldType`.
- Si un campo tiene `#[pin]`, el struct **no puede implementar `Unpin`** a menos que el campo sea `Unpin`.
- No se puede implementar `Drop` directamente (hay que usar `PinnedDrop`).

---

## 9. Errores comunes

### Error 1: Intentar mover un valor !Unpin fuera de Pin

```rust
use std::marker::PhantomPinned;

struct Pinned {
    data: u32,
    _pin: PhantomPinned,
}

let pinned = Box::pin(Pinned { data: 42, _pin: PhantomPinned });

// ERROR: cannot move out of `Pin<Box<Pinned>>`
let inner = *pinned;
```

**Solución**: accede al valor a través de `Pin` sin moverlo.

```rust
// Lectura:
println!("{}", pinned.data);  // OK: Deref funciona

// Modificación (requiere unsafe para !Unpin):
unsafe {
    let p = Pin::into_inner_unchecked(pinned);
    // Usar p, pero NO moverlo a otra ubicación
}
```

### Error 2: Usar Pin::new con tipo !Unpin

```rust
use std::marker::PhantomPinned;

struct NotUnpin {
    _pin: PhantomPinned,
}

let mut val = NotUnpin { _pin: PhantomPinned };

// ERROR: `NotUnpin` doesn't implement `Unpin`
// Pin::new requiere T: Unpin
let pinned = Pin::new(&mut val);
```

**Solución**: usar `Box::pin` o `unsafe { Pin::new_unchecked(&mut val) }`.

```rust
// Opción segura: heap pinning
let pinned = Box::pin(NotUnpin { _pin: PhantomPinned });

// Opción stack: macro pin!
use std::pin::pin;
let pinned = pin!(NotUnpin { _pin: PhantomPinned });
```

### Error 3: Olvidar que async blocks/fn generan tipos !Unpin

```rust
fn takes_unpin<T: Unpin>(val: T) {}

async fn my_future() -> i32 { 42 }

// ERROR: el tipo generado por async fn no implementa Unpin
takes_unpin(my_future());
```

**Solución**: no asumas `Unpin` para futures. Usa `Pin<Box<dyn Future>>` para almacenarlos.

### Error 4: Proyección de Pin manual incorrecta

```rust
use std::pin::Pin;

struct Wrapper<F> {
    inner: F,
}

impl<F: Future> Future for Wrapper<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        // INCORRECTO: crear un Pin al campo sin unsafe
        // let inner = Pin::new(&mut self.inner);  // ERROR

        // INCORRECTO: get_mut no disponible si Self es !Unpin
        // let this = self.get_mut();  // ERROR si F es !Unpin
    }
}
```

**Solución**: usa `pin-project-lite` o `unsafe` con las garantías correctas.

```rust
fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
    // Con unsafe (debes garantizar que inner no se mueva):
    let inner = unsafe { self.map_unchecked_mut(|s| &mut s.inner) };
    inner.poll(cx)
}

// O mejor: usa pin_project! (ver sección 8)
```

### Error 5: Confundir "pinear" con "inmovilizar para siempre"

```rust
use std::pin::Pin;

let mut x = 42;
let pinned = Pin::new(&mut x);

// Pin no impide nada con tipos Unpin:
let val = Pin::into_inner(pinned);  // Perfectamente legal
*val = 100;  // Mutable
let y = *val;  // Movido — no pasa nada, i32 es Copy
```

`Pin` solo tiene efecto real con tipos `!Unpin`. Para todo lo demás, es un no-op semántico.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                          Pin<P> CHEATSHEET                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONCEPTO CENTRAL                                                  ║
║  Pin<P> garantiza que el valor apuntado por P no será movido       ║
║  Pin envuelve un PUNTERO, no un valor                              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR                                                             ║
║  Box::pin(val)                  Heap pin (más común)               ║
║  std::pin::pin!(val)            Stack pin (Rust 1.68+)             ║
║  Pin::new(&mut val)             Solo si T: Unpin                   ║
║  unsafe { Pin::new_unchecked }  Para !Unpin (tú garantizas)       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ACCEDER                                                           ║
║  pin.as_ref().get_ref()         → &T       siempre OK              ║
║  pin.as_mut().get_mut()         → &mut T   solo T: Unpin           ║
║  pin.get_unchecked_mut()        → &mut T   unsafe, para !Unpin     ║
║  Pin::into_inner(pin)           → P        solo T: Unpin           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  Unpin vs !Unpin                                                   ║
║  ┌──────────────┬──────────────────────────────────────────────┐    ║
║  │ T: Unpin     │ Pin<&mut T> ≈ &mut T (sin restricciones)    │    ║
║  │ T: !Unpin    │ Pin impide mover T, no se puede get_mut     │    ║
║  └──────────────┴──────────────────────────────────────────────┘    ║
║                                                                    ║
║  Unpin = auto trait, casi todo lo implementa                       ║
║  !Unpin: agregar PhantomPinned al struct                           ║
║  !Unpin: futures generados por async fn/block                      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  POR QUÉ EXISTE                                                    ║
║  1. async fn genera structs con auto-referencias                   ║
║  2. Mover auto-referencias invalida punteros internos              ║
║  3. Pin previene el movimiento, haciendo async seguro              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  PIN PROJECTION                                                    ║
║  pin_project! { struct S { #[pin] f: F, g: G } }                  ║
║  ├── self.project().f → Pin<&mut F>  (pineado)                     ║
║  └── self.project().g → &mut G      (no pineado)                  ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CUÁNDO PENSAR EN PIN                                              ║
║  ✓ Implementar Future/Stream manualmente                           ║
║  ✓ Almacenar futures en structs                                    ║
║  ✓ Usar tokio::select! / poll directamente                         ║
║  ✓ Crear tipos auto-referenciales                                  ║
║  ✗ Usar async/await normal (el runtime se encarga)                 ║
║  ✗ Código síncrono normal                                          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Comprender Unpin

Predice si cada operación compila o no. Luego verifica con `cargo check`:

```rust
use std::pin::Pin;
use std::marker::PhantomPinned;

struct Movable {
    data: String,
}

struct Unmovable {
    data: String,
    _pin: PhantomPinned,
}

fn main() {
    // Caso A: Pin con tipo Unpin
    let mut m = Movable { data: String::from("a") };
    let pin_m = Pin::new(&mut m);
    let _ref = pin_m.get_mut();          // ¿Compila?

    // Caso B: Pin::new con tipo !Unpin
    let mut u = Unmovable { data: String::from("b"), _pin: PhantomPinned };
    let pin_u = Pin::new(&mut u);        // ¿Compila?

    // Caso C: Box::pin con tipo !Unpin
    let pinned = Box::pin(Unmovable { data: String::from("c"), _pin: PhantomPinned });
    println!("{}", pinned.data);          // ¿Compila?

    // Caso D: Mover fuera de Box::pin
    let pinned = Box::pin(Unmovable { data: String::from("d"), _pin: PhantomPinned });
    let _moved = *pinned;                // ¿Compila?

    // Caso E: Mover fuera de Pin con Unpin
    let pinned = Box::pin(Movable { data: String::from("e") });
    let _moved = Pin::into_inner(pinned);  // ¿Compila?
}
```

**Preguntas adicionales:**
1. ¿Por qué `PhantomPinned` no tiene campos ni ocupa espacio?
2. Si `String` es `Unpin`, ¿un `Vec<String>` también lo es? ¿Por qué?

### Ejercicio 2: Future manual con Pin

Implementa un `Future` llamado `RepeatN` que devuelve `Poll::Pending` exactamente `n` veces antes de completar con un valor:

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct RepeatN<T> {
    value: Option<T>,
    remaining: u32,
}

impl<T> RepeatN<T> {
    fn new(value: T, times: u32) -> Self {
        RepeatN {
            value: Some(value),
            remaining: times,
        }
    }
}

impl<T: Unpin> Future for RepeatN<T> {
    type Output = T;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        todo!("Implementa: decrementa remaining, devuelve Pending si > 0, Ready con value si = 0")
        // Pista: usa self.get_mut() (T es Unpin) para acceder a los campos
        // No olvides llamar a cx.waker().wake_by_ref() antes de Pending
    }
}

// Test con:
// #[tokio::main]
// async fn main() {
//     let result = RepeatN::new("done!", 3).await;
//     println!("{}", result);  // "done!" tras 3 polls pendientes
// }
```

**Preguntas adicionales:**
1. ¿Por qué el bound es `T: Unpin` y no `T: Clone`?
2. ¿Qué pasa si no llamas a `wake_by_ref()`? ¿El future se completa eventualmente?
3. ¿`RepeatN<T>` es `Unpin`? ¿Depende de `T`?

### Ejercicio 3: Pin projection con pin-project-lite

Crea un combinator `TimeoutFuture` que envuelve otro `Future` y cuenta cuántas veces es polleado:

```rust
use pin_project_lite::pin_project;
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

pin_project! {
    struct CountedFuture<F> {
        #[pin]
        inner: F,
        poll_count: u32,
    }
}

impl<F: Future> CountedFuture<F> {
    fn new(inner: F) -> Self {
        todo!("Inicializa con poll_count = 0")
    }
}

impl<F: Future> Future for CountedFuture<F> {
    type Output = (F::Output, u32);  // resultado + número de polls

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        todo!("Usa self.project() para acceder a inner (Pin) y poll_count (&mut)")
        // 1. Obtén la proyección
        // 2. Incrementa *poll_count
        // 3. Poll inner
        // 4. Si Ready(val), devuelve Ready((val, *poll_count))
        // 5. Si Pending, devuelve Pending
    }
}

// Test:
// async fn slow_op() -> &'static str {
//     tokio::time::sleep(Duration::from_millis(10)).await;
//     "result"
// }
//
// #[tokio::main]
// async fn main() {
//     let (result, polls) = CountedFuture::new(slow_op()).await;
//     println!("Result: {}, polled {} times", result, polls);
// }
```

**Preguntas adicionales:**
1. ¿Por qué `inner` necesita `#[pin]` pero `poll_count` no?
2. ¿Qué pasaría si intentas acceder a `this.inner` como `&mut F` en vez de `Pin<&mut F>`?
3. Si quitaras `#[pin]` de `inner`, ¿qué bound adicional necesitarías en `F`?

---

> **Nota**: `Pin` es uno de los conceptos más abstractos de Rust. La mayoría de desarrolladores solo lo encuentran al implementar `Future` o `Stream` manualmente, o al trabajar con crates de bajo nivel. Para uso cotidiano de `async/await`, el compilador y el runtime manejan `Pin` automáticamente. No te preocupes si no lo dominas inmediatamente — su comprensión se profundiza con la práctica en async Rust.
