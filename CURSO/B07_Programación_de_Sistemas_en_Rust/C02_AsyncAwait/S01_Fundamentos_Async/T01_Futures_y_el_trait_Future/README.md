# Futures y el trait Future

## Indice
1. [Por que async](#1-por-que-async)
2. [Que es un Future](#2-que-es-un-future)
3. [El trait Future](#3-el-trait-future)
4. [Poll, Pending y Ready](#4-poll-pending-y-ready)
5. [Lazy evaluation: un Future no hace nada hasta que lo poleas](#5-lazy-evaluation-un-future-no-hace-nada-hasta-que-lo-poleas)
6. [Implementar un Future manualmente](#6-implementar-un-future-manualmente)
7. [Waker: como se despierta un Future](#7-waker-como-se-despierta-un-future)
8. [Composicion de Futures](#8-composicion-de-futures)
9. [Futures vs threads: modelo mental](#9-futures-vs-threads-modelo-mental)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Por que async

En C01 aprendimos concurrencia con hilos. Los hilos funcionan
bien para tareas CPU-bound, pero tienen un problema con tareas
**I/O-bound** (red, disco, bases de datos):

```
    Hilo con I/O bloqueante:

    Hilo 1: [=====]  espera I/O  [==]  espera I/O  [===]
    Hilo 2: [===]  espera I/O  [====]  espera I/O  [==]
    Hilo 3: [==]  espera I/O  [===]  espera I/O  [====]

    3 hilos, la mayor parte del tiempo esperando.
    Cada hilo usa ~2-8 MB de stack.
    10,000 conexiones = 10,000 hilos = 20-80 GB de memoria.
```

Async resuelve esto con un modelo diferente:

```
    Async con un solo hilo:

    Hilo 1: [tarea A][tarea B][tarea C][A continua][B continua]...

    Mientras tarea A espera I/O, ejecuta tarea B.
    Mientras tarea B espera I/O, ejecuta tarea C.
    Un hilo maneja miles de tareas concurrentes.
    Cada tarea usa ~pocos KB (no MB).
```

### Comparacion de costes

```
+-------------------+------------------+------------------+
| Recurso           | Thread (OS)      | Future (async)   |
+-------------------+------------------+------------------+
| Memoria por tarea | 2-8 MB (stack)   | ~KB (estado)     |
| Creacion          | ~microsegundos   | ~nanosegundos    |
| Context switch    | ~microsegundos   | ~nanosegundos    |
| Tareas simultaneas| Miles            | Millones         |
| Overhead          | syscall (kernel) | Ninguno (user)   |
+-------------------+------------------+------------------+
```

---

## 2. Que es un Future

Un **Future** es un valor que **todavia no esta disponible**
pero lo estara en algun momento. Representa una computacion
asincrona en progreso:

```
    Analogia: pedir comida en un restaurante

    Sincrono (bloqueante):
    1. Pides al mesero
    2. Te sientas a esperar... (no puedes hacer nada mas)
    3. El mesero trae la comida
    4. Comes

    Asincrono (Future):
    1. Pides al mesero --> recibes un "ticket" (Future)
    2. Mientras esperas, lees el periodico, hablas, etc.
    3. Cuando la comida esta lista, el mesero te avisa (Waker)
    4. Recoges la comida (Poll::Ready) y comes
```

En Rust, un Future es cualquier tipo que implemente el trait
`std::future::Future`. No es un hilo — es una **maquina de
estados** que avanza un paso cada vez que la poleas.

---

## 3. El trait Future

```rust
// Definicion en std::future (simplificada):
pub trait Future {
    type Output;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output>;
}
```

Desglosemos cada parte:

### `type Output`

El tipo del valor que el Future produce cuando se completa.
Es como el tipo de retorno de una funcion, pero diferido:

```rust
// Funcion sincrona: retorna String inmediatamente
fn get_name() -> String { ... }

// Future: eventualmente producira un String
// impl Future<Output = String>
```

### `fn poll(...) -> Poll<Self::Output>`

El metodo central. El runtime llama a `poll` para preguntar:
"ya terminaste?"

- Si el Future esta listo: retorna `Poll::Ready(valor)`
- Si aun no: retorna `Poll::Pending`

### `self: Pin<&mut Self>`

`Pin` garantiza que el Future no se mueve en memoria entre
llamadas a `poll`. Es necesario porque un Future puede
contener auto-referencias (veremos esto en T04_Pin_y_Unpin).
Por ahora, puedes pensarlo como `&mut self` con restricciones.

### `cx: &mut Context<'_>`

Contiene un **Waker** — el mecanismo para que el Future le
diga al runtime "despiertame cuando haya progreso". Sin el
Waker, el runtime tendria que hacer polling continuo (busy
wait), lo cual seria ineficiente.

---

## 4. Poll, Pending y Ready

```rust
pub enum Poll<T> {
    Ready(T),
    Pending,
}
```

El flujo de vida de un Future:

```
    Creacion          poll()         poll()         poll()
    +--------+     +---------+    +---------+    +---------+
    | Future | --> | Pending | -> | Pending | -> | Ready(v)|
    +--------+     +---------+    +---------+    +---------+
                       |              |              |
                   "no listo"     "no listo"      "terminado"
                   (registra      (registra       (valor v
                    waker)         waker)          disponible)
```

### Reglas del contrato de poll

1. Despues de retornar `Ready(v)`, el Future **no debe** ser
   poleado de nuevo. Hacerlo es comportamiento indefinido logico
   (puede paniquear, retornar basura, etc.)

2. Al retornar `Pending`, el Future **debe** haber registrado
   el Waker de `cx` para que alguien lo despierte cuando haya
   progreso. Si no lo hace, el Future nunca sera poleado de
   nuevo y se queda "dormido" para siempre.

3. `poll` debe ser **non-blocking**: retornar rapidamente,
   nunca bloquear el hilo. Si no hay progreso, retornar
   `Pending` y registrar un waker.

```
    MAL (bloqueante):                BIEN (non-blocking):
    fn poll(...) -> Poll<T> {        fn poll(...) -> Poll<T> {
        loop {                           if self.ready() {
            if self.ready() {                return Poll::Ready(self.value());
                return Ready(v);         }
            }                            cx.waker().wake_by_ref();
            sleep(10ms);  // BLOQUEA!    Poll::Pending
        }                           }
    }
```

---

## 5. Lazy evaluation: un Future no hace nada hasta que lo poleas

Esta es una diferencia fundamental con otros lenguajes.
En JavaScript, `fetch(url)` inicia la peticion inmediatamente.
En Rust, crear un Future **no ejecuta nada**:

```rust
async fn fetch_data() -> String {
    println!("fetching...");
    "data".to_string()
}

fn main() {
    let future = fetch_data();
    // "fetching..." NO se imprime!
    // future es solo una maquina de estados dormida.
    // No se ejecuta hasta que alguien la polee.

    println!("future created, but nothing happened");
}
```

```
    JavaScript:                     Rust:
    let p = fetch(url);            let f = fetch_data();
    // peticion ya enviada!         // NADA ejecutado!
    // p es una Promise activa      // f es una maquina de estados inerte

    p.then(data => ...)            // Necesitas un runtime para pollear f
```

### Por que lazy evaluation

- **Control**: tu decides cuando empieza la ejecucion
- **Composicion**: puedes construir pipelines de Futures sin
  ejecutarlos
- **Cancelacion**: dropear un Future sin polearlo lo cancela
  gratis (no hay trabajo que parar)
- **Zero-cost**: si no poleas, no pagas nada

### Para ejecutar un Future necesitas un runtime

```rust
// Esto no hace nada (future nunca se polea):
fn main() {
    let _future = async { 42 };
}

// Necesitas un runtime como Tokio:
#[tokio::main]
async fn main() {
    let value = async { 42 }.await;  // .await polea el future
    println!("{}", value);           // 42
}
```

El `.await` le dice al runtime: "polea este Future; si retorna
`Pending`, suspendeme y ejecuta otra tarea; cuando este listo,
continuame desde aqui."

---

## 6. Implementar un Future manualmente

Para entender como funciona internamente, implementemos un
Future a mano. Empecemos con el caso mas simple:

### Future que esta listo inmediatamente

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct Ready<T> {
    value: Option<T>,
}

impl<T> Future for Ready<T> {
    type Output = T;

    fn poll(mut self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<T> {
        // Tomamos el valor (Option -> None) para no retornarlo dos veces
        match self.value.take() {
            Some(v) => Poll::Ready(v),
            None => panic!("Ready polled after completion"),
        }
    }
}

fn ready<T>(value: T) -> Ready<T> {
    Ready { value: Some(value) }
}

// Uso:
#[tokio::main]
async fn main() {
    let value = ready(42).await;
    println!("{}", value);  // 42
}
```

> **Nota**: `std::future::ready(42)` ya existe en la libreria
> estandar — este ejemplo es pedagogico.

### Future con delay (requiere un timer)

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};
use std::time::{Duration, Instant};

struct Delay {
    when: Instant,
}

impl Delay {
    fn new(duration: Duration) -> Self {
        Delay {
            when: Instant::now() + duration,
        }
    }
}

impl Future for Delay {
    type Output = ();

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        if Instant::now() >= self.when {
            Poll::Ready(())
        } else {
            // Registrar el waker para que nos despierten
            // PROBLEMA: quien nos despierta? (ver seccion 7)
            cx.waker().wake_by_ref();
            Poll::Pending
        }
    }
}
```

> **Atencion**: este `Delay` tiene un problema — llama
> `wake_by_ref()` inmediatamente, lo que causa busy polling.
> Un timer real registraria el waker con el sistema de timers
> del runtime. Tokio tiene `tokio::time::sleep` que hace esto
> correctamente.

### Future con estado: contador

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct CountDown {
    remaining: u32,
}

impl CountDown {
    fn new(count: u32) -> Self {
        CountDown { remaining: count }
    }
}

impl Future for CountDown {
    type Output = String;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<String> {
        if self.remaining == 0 {
            Poll::Ready("liftoff!".to_string())
        } else {
            println!("countdown: {}", self.remaining);
            self.remaining -= 1;
            // Despertar inmediatamente para continuar el countdown
            cx.waker().wake_by_ref();
            Poll::Pending
        }
    }
}

#[tokio::main]
async fn main() {
    let message = CountDown::new(3).await;
    println!("{}", message);
    // countdown: 3
    // countdown: 2
    // countdown: 1
    // liftoff!
}
```

Este Future se polea 4 veces: 3 retornan `Pending` (con
wake inmediato) y la 4ta retorna `Ready("liftoff!")`.

---

## 7. Waker: como se despierta un Future

Cuando un Future retorna `Pending`, necesita una forma de
decirle al runtime "ya estoy listo, poleame de nuevo". Eso
es el **Waker**:

```
    Future retorna Pending
         |
         v
    Registra el Waker con la fuente de eventos:
    - Timer del OS (para sleep)
    - epoll/kqueue (para I/O de red)
    - Channel (para mensajes)
         |
         v
    El Future "duerme" — no consume CPU
         |
    (tiempo pasa, datos llegan, etc.)
         |
         v
    La fuente de eventos llama waker.wake()
         |
         v
    El runtime pone el Future en la cola de ejecucion
         |
         v
    El runtime llama poll() de nuevo
         |
         v
    Poll::Ready(value) — terminado!
```

### Estructura del Waker

```rust
// En std::task:
pub struct Context<'a> {
    waker: &'a Waker,
    // ...
}

impl Context<'_> {
    pub fn waker(&self) -> &Waker { ... }
}

impl Waker {
    pub fn wake(self) { ... }           // consume el waker
    pub fn wake_by_ref(&self) { ... }   // no lo consume
}
```

### Ejemplo: como funciona un timer real (conceptual)

```rust
// Pseudocodigo — asi funciona tokio::time::sleep internamente:

struct Sleep {
    deadline: Instant,
    registered: bool,
}

impl Future for Sleep {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        if Instant::now() >= self.deadline {
            return Poll::Ready(());
        }

        if !self.registered {
            // Registrar con el timer wheel del runtime:
            // "cuando llegue self.deadline, llama cx.waker().wake()"
            RUNTIME.register_timer(self.deadline, cx.waker().clone());
            self.registered = true;
        }

        Poll::Pending
        // El Future duerme aqui.
        // NO se polea de nuevo hasta que el timer dispare el waker.
    }
}
```

### Waker vs busy poll

```
    SIN waker (busy poll):           CON waker (eficiente):
    poll() -> Pending                poll() -> Pending
    poll() -> Pending                  (duerme...)
    poll() -> Pending                  (duerme...)
    poll() -> Pending                  (duerme...)
    poll() -> Ready                  waker.wake() -> poll() -> Ready

    CPU: 100% quemando ciclos        CPU: 0% mientras duerme
```

---

## 8. Composicion de Futures

Los Futures se componen para crear operaciones mas complejas.
Las dos formas principales son **secuencial** y **concurrente**:

### Secuencial: await uno tras otro

```rust
async fn sequential() {
    let a = fetch_a().await;  // espera a que termine
    let b = fetch_b().await;  // luego espera a que termine
    println!("{} {}", a, b);
}
```

```
    Timeline:
    |-- fetch_a --|-- fetch_b --|-- print --|
    Tiempo total: tiempo_a + tiempo_b
```

### Concurrente: join (ejecutar en paralelo)

```rust
use tokio::join;

async fn concurrent() {
    let (a, b) = join!(fetch_a(), fetch_b());
    // Ambos se ejecutan concurrentemente
    println!("{} {}", a, b);
}
```

```
    Timeline:
    |-- fetch_a --|
    |-- fetch_b ------|
    |                  |-- print --|
    Tiempo total: max(tiempo_a, tiempo_b)
```

### Select: el primero que termine

```rust
use tokio::select;

async fn race() {
    select! {
        a = fetch_a() => println!("a won: {}", a),
        b = fetch_b() => println!("b won: {}", b),
    }
    // El otro Future se cancela (drop)
}
```

```
    Timeline:
    |-- fetch_a --|  <-- este gana
    |-- fetch_b --x     este se cancela (drop)
```

### Internamente: maquina de estados

El compilador transforma `async fn` en una maquina de estados.
Cada `.await` es un punto donde el Future puede suspenderse:

```rust
// Esto:
async fn example() -> i32 {
    let a = step_one().await;    // punto de suspension 1
    let b = step_two(a).await;  // punto de suspension 2
    a + b
}

// El compilador genera algo conceptualmente similar a:
enum ExampleFuture {
    Step0,                              // estado inicial
    Step1 { future: StepOneFuture },    // esperando step_one
    Step2 { a: i32, future: StepTwoFuture }, // esperando step_two
    Done,
}

impl Future for ExampleFuture {
    type Output = i32;

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<i32> {
        loop {
            match self.state {
                Step0 => {
                    self.state = Step1 { future: step_one() };
                }
                Step1 { future } => {
                    match future.poll(cx) {
                        Ready(a) => self.state = Step2 { a, future: step_two(a) },
                        Pending => return Pending,
                    }
                }
                Step2 { a, future } => {
                    match future.poll(cx) {
                        Ready(b) => return Ready(a + b),
                        Pending => return Pending,
                    }
                }
                Done => panic!("polled after done"),
            }
        }
    }
}
```

```
    step_one().await      step_two(a).await
         |                     |
    +----+----+          +-----+-----+
    |         |          |           |
    v         v          v           v
  Step0 --> Step1 --> Step2 --> Ready(a+b)
            Pending?  Pending?
            Si: return Si: return
            Pending    Pending
```

Este es el **zero-cost** de async en Rust: no hay heap
allocation por defecto, no hay runtime overhead — solo una
maquina de estados generada por el compilador.

---

## 9. Futures vs threads: modelo mental

```
    THREADS:                         FUTURES:
    +------------------+             +------------------+
    | Sistema operativo|             | Runtime (Tokio)  |
    | gestiona hilos   |             | gestiona tareas  |
    +------------------+             +------------------+
    | Cada hilo tiene  |             | Cada tarea es un |
    | su propio stack  |             | Future (enum)    |
    | (2-8 MB)         |             | (~pocos KB)      |
    +------------------+             +------------------+
    | Preemptive:      |             | Cooperative:     |
    | el OS interrumpe |             | la tarea cede    |
    | en cualquier     |             | control en cada  |
    | momento          |             | .await           |
    +------------------+             +------------------+
    | Sincronizacion:  |             | Sincronizacion:  |
    | Mutex, condvar,  |             | Menos necesaria  |
    | atomics          |             | (un hilo a la    |
    |                  |             | vez por tarea)   |
    +------------------+             +------------------+
```

### Preemptive vs cooperative

- **Threads (preemptive)**: el OS puede interrumpir tu hilo
  en cualquier punto para ejecutar otro. No necesitas cooperar.
- **Futures (cooperative)**: tu tarea debe ceder control
  explicitamente (en cada `.await`). Si no cedes, bloqueas
  el runtime.

```rust
// MAL: bloquea el runtime — sin puntos de suspension
async fn hog_cpu() {
    // Calculo pesado sin .await
    let mut sum = 0u64;
    for i in 0..1_000_000_000 {
        sum += i;
    }
    println!("{}", sum);
}

// BIEN: ceder control periodicamente
async fn cooperative_cpu() {
    let mut sum = 0u64;
    for i in 0..1_000_000_000 {
        sum += i;
        if i % 1_000_000 == 0 {
            tokio::task::yield_now().await;  // ceder
        }
    }
    println!("{}", sum);
}
```

### Cuando usar cada modelo

```
    CPU-bound (calculo intensivo):
    --> Threads (rayon, thread::scope)
    --> El OS reparte tiempo de CPU justamente

    I/O-bound (red, disco, DB):
    --> Futures (async/await con Tokio)
    --> Miles de conexiones con minima memoria

    Mixto:
    --> Tokio con spawn_blocking para partes CPU-bound
```

---

## 10. Errores comunes

### Error 1: crear un Future y olvidar .await

```rust
async fn send_email() {
    println!("email sent!");
}

async fn handler() {
    send_email();  // WARNING: future is not awaited!
    // El email NUNCA se envia — el Future se crea y se droppea
}
```

```rust
// BIEN: usar .await
async fn handler() {
    send_email().await;  // ahora si se ejecuta
}
```

El compilador emite un warning: `unused implementor of
Future that must be used`. **Nunca** ignores este warning.

### Error 2: confundir Future con Promise (JavaScript)

```rust
// En JavaScript, esto inicia fetch inmediatamente:
// let p = fetch(url);  // ya esta ejecutando
// p.then(data => ...);

// En Rust, esto NO inicia nada:
let f = fetch_data();  // nada se ejecuta
// f es inerte hasta que alguien la polee con .await
```

**Regla en Rust**: un Future es lazy — no hace nada hasta
`.await` (o hasta que un runtime lo polee explicitamente).

### Error 3: bloquear el hilo dentro de un Future

```rust
// MAL: std::thread::sleep bloquea el hilo del runtime
async fn bad_sleep() {
    std::thread::sleep(std::time::Duration::from_secs(5));
    // Bloquea todo el runtime durante 5 segundos!
    // Ninguna otra tarea puede ejecutarse.
}
```

```rust
// BIEN: usar la version async
async fn good_sleep() {
    tokio::time::sleep(std::time::Duration::from_secs(5)).await;
    // Suspende esta tarea; otras tareas pueden ejecutarse.
}
```

**Regla**: dentro de `async fn`, nunca uses operaciones
bloqueantes de `std` (sleep, read, write). Usa sus equivalentes
async (`tokio::time::sleep`, `tokio::fs::read`, etc.) o
`tokio::task::spawn_blocking`.

### Error 4: asumir que poll se llama solo una vez

```rust
struct MyFuture {
    started: bool,
}

impl Future for MyFuture {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<()> {
        if !self.started {
            self.started = true;
            println!("starting...");
            // Iniciar operacion async...
            cx.waker().wake_by_ref();
            Poll::Pending
        } else {
            println!("done!");
            Poll::Ready(())
        }
    }
}
// poll se llama DOS veces: primero Pending, luego Ready
```

Un Future puede ser poleado **muchas** veces. Cada llamada
debe verificar el estado actual y avanzar la maquina de estados.

### Error 5: no registrar el Waker al retornar Pending

```rust
impl Future for MyFuture {
    type Output = i32;

    fn poll(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<i32> {
        if self.is_ready() {
            Poll::Ready(self.value())
        } else {
            // MAL: retorna Pending sin registrar waker
            // Nadie despertara este Future — se queda dormido para siempre
            Poll::Pending
        }
    }
}
```

```rust
// BIEN: registrar waker o despertar inmediatamente
fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<i32> {
    if self.is_ready() {
        Poll::Ready(self.value())
    } else {
        // Opcion A: registrar waker con la fuente de eventos
        self.register_waker(cx.waker().clone());
        // Opcion B: despertar inmediatamente (para reintentar)
        // cx.waker().wake_by_ref();
        Poll::Pending
    }
}
```

---

## 11. Cheatsheet

```
FUTURES - REFERENCIA RAPIDA
============================

El trait:
  trait Future {
      type Output;
      fn poll(self: Pin<&mut Self>, cx: &mut Context) -> Poll<Self::Output>;
  }

Poll enum:
  Poll::Ready(value)   -- Future completado, aqui esta el valor
  Poll::Pending        -- no listo, despiertame con el Waker

Lazy evaluation:
  let f = async_fn();   // no ejecuta nada
  let v = async_fn().await;  // ejecuta y espera resultado
  // Un Future sin .await es como una funcion que nunca llamas

Waker:
  cx.waker().wake()          -- despertar (consume el waker)
  cx.waker().wake_by_ref()   -- despertar (no consume)
  cx.waker().clone()         -- clonar para registrar en otro lugar

Contrato de poll:
  - Retornar rapido, NUNCA bloquear
  - Al retornar Pending, registrar el Waker
  - Despues de Ready, no pollear de nuevo

Composicion:
  Secuencial:  a.await; b.await;        (uno tras otro)
  Concurrente: join!(a, b)              (ambos en paralelo)
  Race:        select! { a => .., b => ..}  (primero que termine)

Maquina de estados:
  async fn genera un enum con un estado por cada .await
  Cada poll() avanza la maquina de estados un paso
  Zero-cost: sin heap allocation por defecto

Modelo cooperative:
  La tarea cede control en cada .await
  Si no hay .await, bloqueas el runtime
  CPU-bound en async -> usar spawn_blocking

Cuando NO usar async:
  - Calculos pesados sin I/O -> threads/rayon
  - Aplicaciones simples sin concurrencia
  - Cuando la complejidad no justifica el beneficio

Equivalencias std -> async:
  std::thread::sleep    ->  tokio::time::sleep
  std::fs::read         ->  tokio::fs::read
  std::net::TcpStream   ->  tokio::net::TcpStream
  std::sync::Mutex      ->  tokio::sync::Mutex*
  * Solo si el lock cruza un .await
```

---

## 12. Ejercicios

### Ejercicio 1: implementar un Future que retorna tras N polls

Crea un Future que retorne un valor solo despues de ser
poleado exactamente N veces:

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

struct PollN {
    remaining: u32,
    value: String,
}

impl PollN {
    fn new(n: u32, value: impl Into<String>) -> Self {
        PollN {
            remaining: n,
            value: value.into(),
        }
    }
}

impl Future for PollN {
    type Output = String;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<String> {
        // TODO:
        // Si remaining > 0: decrementar, registrar waker, retornar Pending
        // Si remaining == 0: retornar Ready con el valor
        // Imprimir "poll #{n}" en cada llamada para observar el comportamiento
        todo!()
    }
}

#[tokio::main]
async fn main() {
    let result = PollN::new(5, "hello after 5 polls").await;
    println!("Result: {}", result);
    // Esperado:
    // poll #5
    // poll #4
    // poll #3
    // poll #2
    // poll #1
    // Result: hello after 5 polls
}
```

Tareas:
1. Implementa el metodo `poll`
2. Ejecuta y verifica que se polea exactamente N+1 veces
   (N Pending + 1 Ready)
3. Que pasa si cambias `wake_by_ref()` por `wake()`? Necesitas
   clonar el waker?
4. Que pasa si **no** llamas al waker? Ejecuta y observa

**Prediccion**: si no llamas al waker, el programa terminara
o se quedara colgado para siempre?

> **Pregunta de reflexion**: este Future llama `wake_by_ref()`
> inmediatamente — es efectivamente busy polling. En un runtime
> real con miles de tareas, esto seria terrible para el
> rendimiento. Pero `tokio::time::sleep` NO hace busy polling —
> registra el waker con un timer del runtime. Cual es la
> diferencia practica? Que pasa con el consumo de CPU en cada
> caso?

### Ejercicio 2: composicion secuencial vs concurrente

Crea dos Futures lentos y compara el tiempo de ejecucion
en modo secuencial vs concurrente:

```rust
use std::time::{Duration, Instant};
use tokio::time::sleep;

async fn slow_task(name: &str, duration: Duration) -> String {
    println!("[{}] starting", name);
    sleep(duration).await;
    println!("[{}] done", name);
    format!("{} result", name)
}

#[tokio::main]
async fn main() {
    // Version 1: secuencial
    let start = Instant::now();
    let a = slow_task("A", Duration::from_secs(2)).await;
    let b = slow_task("B", Duration::from_secs(3)).await;
    println!("Sequential: {} + {} in {:?}", a, b, start.elapsed());

    // Version 2: concurrente con join!
    let start = Instant::now();
    let (a, b) = tokio::join!(
        slow_task("A", Duration::from_secs(2)),
        slow_task("B", Duration::from_secs(3)),
    );
    println!("Concurrent: {} + {} in {:?}", a, b, start.elapsed());

    // TODO Version 3: race con select!
    // Cual termina primero?
    let start = Instant::now();
    tokio::select! {
        a = slow_task("A", Duration::from_secs(2)) => {
            println!("A won: {} in {:?}", a, start.elapsed());
        }
        b = slow_task("B", Duration::from_secs(3)) => {
            println!("B won: {} in {:?}", b, start.elapsed());
        }
    }
    // Pregunta: el perdedor se cancela. Ves su "done" message?
}
```

Tareas:
1. Ejecuta y compara los tiempos
2. Secuencial deberia tardar ~5s, concurrente ~3s, select ~2s
3. En el `select!`, verifica que el Future perdedor **no**
   imprime "done" — fue cancelado (dropped)
4. Agrega un tercer Future "C" de 1 segundo al `select!`

> **Pregunta de reflexion**: `join!` ejecuta Futures
> **concurrentemente pero en el mismo hilo** (no en paralelo).
> Si uno de los Futures hiciera CPU pesado sin `.await`, las
> otras tareas se bloquearian. Como se diferencia esto de
> `rayon::join` que si ejecuta en hilos separados?

### Ejercicio 3: observar la maquina de estados

Implementa un Future que simule una operacion en tres fases
y observa las transiciones de estado:

```rust
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

enum State {
    Init,
    Connecting,
    Reading { connection_id: u32 },
    Done,
}

struct ThreePhaseOp {
    state: State,
}

impl ThreePhaseOp {
    fn new() -> Self {
        ThreePhaseOp { state: State::Init }
    }
}

impl Future for ThreePhaseOp {
    type Output = String;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<String> {
        loop {
            match &self.state {
                State::Init => {
                    println!("  [state] Init -> Connecting");
                    self.state = State::Connecting;
                    // No retornamos Pending aqui — continuamos al siguiente estado
                    // (un Future puede avanzar multiples estados en un solo poll)
                }
                State::Connecting => {
                    println!("  [state] Connecting -> Reading");
                    // TODO: simular que la conexion toma tiempo
                    // Transicionar a Reading con connection_id = 42
                    // Registrar waker y retornar Pending
                    todo!()
                }
                State::Reading { connection_id } => {
                    println!("  [state] Reading (conn={}) -> Done", connection_id);
                    // TODO: retornar el resultado
                    todo!()
                }
                State::Done => {
                    panic!("polled after completion");
                }
            }
        }
    }
}

#[tokio::main]
async fn main() {
    println!("Starting three-phase operation...");
    let result = ThreePhaseOp::new().await;
    println!("Result: {}", result);
    // Esperado:
    //   Starting three-phase operation...
    //   [state] Init -> Connecting
    //   [state] Connecting -> Reading
    //   [state] Reading (conn=42) -> Done
    //   Result: data from connection 42
}
```

Tareas:
1. Completa los `TODO` para las transiciones de estado
2. Decide: la transicion `Connecting -> Reading` debe retornar
   `Pending` (simulando I/O async) o avanzar directamente?
3. Agrega un campo `polls: u32` al struct y cuenta cuantas
   veces se llama `poll`
4. Modifica para que `Connecting` requiera 2 polls antes de
   transicionar a `Reading`

> **Pregunta de reflexion**: esta maquina de estados manual es
> exactamente lo que el compilador genera automaticamente cuando
> escribes `async fn` con multiples `.await`. La diferencia es
> que el compilador lo hace sin errores y con optimizaciones
> (como eliminar estados innecesarios). Por que entonces es
> util entender la implementacion manual? En que situaciones
> escribirias un Future a mano en vez de usar `async fn`?
