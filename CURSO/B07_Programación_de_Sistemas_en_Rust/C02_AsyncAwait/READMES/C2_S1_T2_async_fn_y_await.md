# async fn y .await

## Indice
1. [De Future manual a async fn](#1-de-future-manual-a-async-fn)
2. [Sintaxis de async fn](#2-sintaxis-de-async-fn)
3. [Que genera el compilador](#3-que-genera-el-compilador)
4. [El operador .await](#4-el-operador-await)
5. [async blocks](#5-async-blocks)
6. [Lifetimes en async fn](#6-lifetimes-en-async-fn)
7. [async fn en traits](#7-async-fn-en-traits)
8. [Recursion en async fn](#8-recursion-en-async-fn)
9. [Cancelacion: drop de un Future](#9-cancelacion-drop-de-un-future)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. De Future manual a async fn

En T01 implementamos Futures manualmente con enums de estado
y `fn poll()`. Funciona, pero es tedioso y propenso a errores.
`async fn` hace que el compilador genere todo eso por ti:

```rust
// MANUAL: ~30 lineas de maquina de estados
enum MyFuture {
    Step0,
    Step1 { fut: SomeFuture },
    Step2 { a: i32, fut: OtherFuture },
    Done,
}
impl Future for MyFuture { ... }

// ASYNC FN: el compilador genera lo mismo
async fn my_function() -> i32 {
    let a = some_operation().await;
    let b = other_operation(a).await;
    a + b
}
```

El resultado es identico — `async fn` es **sugar sintactico**
sobre el trait `Future`. No hay magia, no hay overhead, solo
comodidad.

---

## 2. Sintaxis de async fn

### Declaracion basica

```rust
// Funcion sincrona: ejecuta y retorna inmediatamente
fn sync_greet(name: &str) -> String {
    format!("Hello, {}", name)
}

// Funcion async: retorna un Future que, cuando se polea,
// produce un String
async fn async_greet(name: &str) -> String {
    format!("Hello, {}", name)
}
```

El tipo de retorno real de `async fn async_greet` NO es
`String` — es un tipo anonimo que implementa
`Future<Output = String>`:

```rust
// Estas dos firmas son equivalentes:
async fn greet() -> String { ... }

fn greet() -> impl Future<Output = String> { ... }
```

### Con parametros y retorno

```rust
async fn fetch_user(id: u64) -> Result<User, Error> {
    let response = http_get(format!("/users/{}", id)).await?;
    let user: User = parse_json(response).await?;
    Ok(user)
}
```

El `?` funciona normalmente dentro de async fn — propaga
errores como en funciones sincronas.

### async fn como metodo

```rust
struct Client {
    base_url: String,
}

impl Client {
    async fn get(&self, path: &str) -> String {
        let url = format!("{}{}", self.base_url, path);
        // ... fetch ...
        url
    }

    async fn post(&self, path: &str, body: &str) -> Result<(), Error> {
        // ...
        Ok(())
    }
}
```

---

## 3. Que genera el compilador

Cuando escribes `async fn`, el compilador genera una
**maquina de estados** como un enum anonimo. Cada `.await`
crea un nuevo estado:

```rust
async fn example() -> i32 {
    println!("start");           // estado 0: antes del primer await
    let a = step_one().await;    // estado 1: esperando step_one
    println!("got a = {}", a);   // transicion entre estados
    let b = step_two(a).await;   // estado 2: esperando step_two
    a + b                        // retorno final
}
```

El compilador genera algo equivalente a:

```rust
enum ExampleFuture {
    // Estado 0: inicio
    State0,
    // Estado 1: esperando step_one(), no hay variables guardadas aun
    State1 {
        step_one_future: StepOneFuture,
    },
    // Estado 2: esperando step_two(), guardamos 'a' para usarlo despues
    State2 {
        a: i32,
        step_two_future: StepTwoFuture,
    },
    // Terminado
    Completed,
}
```

### Que se guarda en cada estado

Solo las variables que **cruzan** un punto de suspension
(`.await`) se almacenan en el enum. Las variables temporales
que no cruzan un await **no** ocupan espacio:

```rust
async fn efficient() {
    let big = vec![0u8; 1_000_000]; // 1 MB
    process(&big);                   // usa big ANTES del await
    drop(big);                       // libera big ANTES del await

    do_io().await;                   // big NO esta en el enum
                                     // (ya fue dropeada)
    let small = 42u32;
    more_io().await;                 // solo 'small' (4 bytes) en el enum
}
```

```rust
async fn wasteful() {
    let big = vec![0u8; 1_000_000]; // 1 MB

    do_io().await;                   // big CRUZA el await
                                     // el Future ocupa >1 MB!
    process(&big);
}
```

**Regla**: dropea los datos grandes antes de `.await` si no
los necesitas despues.

### Tamano del Future

Puedes inspeccionar el tamano con `std::mem::size_of_val`:

```rust
async fn small() -> i32 {
    42
}

async fn medium() -> i32 {
    let a = step().await;
    let b = step().await;
    a + b
}

fn main() {
    let f1 = small();
    let f2 = medium();
    println!("small: {} bytes", std::mem::size_of_val(&f1));
    println!("medium: {} bytes", std::mem::size_of_val(&f2));
}
```

---

## 4. El operador .await

`.await` es un operador de suspension: "polea este Future;
si retorna `Pending`, suspende esta tarea y deja que otras
se ejecuten; cuando retorne `Ready`, continua desde aqui."

### Solo funciona dentro de contexto async

```rust
// OK: dentro de async fn
async fn ok() {
    tokio::time::sleep(std::time::Duration::from_secs(1)).await;
}

// OK: dentro de async block
fn also_ok() -> impl Future<Output = ()> {
    async {
        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
    }
}

// ERROR: await fuera de contexto async
fn bad() {
    tokio::time::sleep(std::time::Duration::from_secs(1)).await;
    // error: `await` is only allowed inside `async` functions and blocks
}
```

### .await es un punto de suspension

Cada `.await` es un punto donde la tarea puede suspenderse
y reanudarse. Esto tiene implicaciones:

```rust
async fn demonstration() {
    println!("A");          // se ejecuta inmediatamente
    sleep(1s).await;        // SUSPENSION: puede pasar tiempo aqui
    println!("B");          // se ejecuta cuando sleep termina
    sleep(1s).await;        // SUSPENSION
    println!("C");          // se ejecuta cuando sleep termina
}
```

```
    Hilo del runtime:
    |--A--| sleep |--otras tareas--|--B--| sleep |--otras tareas--|--C--|

    Un solo hilo ejecuta muchas tareas intercaladamente.
    Cada .await es una oportunidad para cambiar de tarea.
```

### .await con Result (combinando con ?)

```rust
async fn fetch_data() -> Result<String, Box<dyn std::error::Error>> {
    let response = client.get("https://api.example.com")
        .send()
        .await?;       // await el Future, luego ? propaga error

    let text = response
        .text()
        .await?;       // otro await + ?

    Ok(text)
}
```

El orden es: primero `.await` (obtiene el `Result`), luego
`?` (propaga el `Err` si hay error). Se lee de izquierda a
derecha.

### .await en cadenas

```rust
// Esto NO paraleliza — es secuencial:
let a = op_a().await;
let b = op_b().await;
let c = op_c().await;

// Para concurrencia, usa join!:
let (a, b, c) = tokio::join!(op_a(), op_b(), op_c());
```

---

## 5. async blocks

Ademas de `async fn`, puedes crear Futures con **async blocks**:

```rust
// async fn: nombrado, reutilizable
async fn named() -> i32 { 42 }

// async block: anonimo, inline
let future = async {
    let x = some_op().await;
    x + 1
};

let result = future.await;
```

### Casos de uso de async blocks

**1. Capturar variables del scope:**

```rust
async fn process(data: Vec<u8>) {
    let len = data.len();

    // async block captura 'data' por referencia o move
    let future = async {
        println!("processing {} bytes", data.len());
        // ... usar data ...
    };

    future.await;
}
```

**2. async move block (transfiere ownership):**

```rust
fn spawn_task(data: Vec<u8>) {
    // move: transfiere ownership de data al Future
    let future = async move {
        println!("got {} bytes", data.len());
        // data vive dentro del Future
    };

    tokio::spawn(future);
    // data ya no existe aqui
}
```

**3. Condicionar el tipo de Future:**

```rust
async fn conditional(flag: bool) -> i32 {
    if flag {
        async { expensive_a().await }.await
    } else {
        async { expensive_b().await }.await
    }
}
```

**4. Crear Futures para join!/select!:**

```rust
async fn example() {
    let task_a = async {
        sleep(Duration::from_secs(1)).await;
        "a done"
    };

    let task_b = async {
        sleep(Duration::from_secs(2)).await;
        "b done"
    };

    let (a, b) = tokio::join!(task_a, task_b);
    println!("{}, {}", a, b);
}
```

---

## 6. Lifetimes en async fn

Los lifetimes en async fn son mas complicados que en funciones
normales porque el Future generado **captura** las referencias
de los parametros:

### El problema

```rust
// Esta firma:
async fn greet(name: &str) -> String {
    format!("Hello, {}", name)
}

// Es equivalente a:
fn greet<'a>(name: &'a str) -> impl Future<Output = String> + 'a {
    async move {
        format!("Hello, {}", name)
    }
}
// El Future vive tanto como 'a — necesita que name siga vivo
```

El Future **retiene** la referencia `name` porque podria
necesitarla en cualquier punto de suspension. Esto significa
que `name` debe vivir al menos tanto como el Future:

```rust
async fn example() {
    let future;
    {
        let name = String::from("Alice");
        future = greet(&name);  // Future captura &name
    }
    // name dropeada aqui!
    // future.await;  // ERROR: name ya no existe
}
```

### Solucion: ownership en vez de referencia

```rust
// Si el Future va a vivir independientemente (ej: tokio::spawn),
// usa ownership:
async fn greet_owned(name: String) -> String {
    format!("Hello, {}", name)
}

fn spawn_greeting(name: String) {
    tokio::spawn(greet_owned(name));  // OK: el Future posee name
}
```

### tokio::spawn requiere 'static

```rust
// tokio::spawn requiere que el Future sea 'static:
// pub fn spawn<F>(future: F) -> JoinHandle<F::Output>
// where F: Future + Send + 'static

async fn example() {
    let data = String::from("hello");

    // MAL: el closure captura &data, no es 'static
    // tokio::spawn(async {
    //     println!("{}", data);
    // });

    // BIEN: move ownership al Future
    tokio::spawn(async move {
        println!("{}", data);
    });
    // data ya no existe aqui
}
```

---

## 7. async fn en traits

### Antes de Rust 1.75: no se podia directamente

```rust
// ERROR en Rust < 1.75:
trait MyService {
    async fn process(&self) -> Result<(), Error>;
    // error: `async fn` is not allowed in trait definitions
}
```

La razon tecnica: el tipo del Future generado es anonimo y
diferente para cada implementacion. El trait no puede expresar
"cada implementacion retorna un Future diferente" sin
`impl Trait` en posicion de retorno.

### Desde Rust 1.75: funciona directamente

```rust
// OK en Rust >= 1.75:
trait MyService {
    async fn process(&self) -> Result<(), Error>;
}

struct ConcreteService;

impl MyService for ConcreteService {
    async fn process(&self) -> Result<(), Error> {
        do_work().await;
        Ok(())
    }
}
```

### Limitacion: no es object-safe

`async fn` en traits no permite `dyn Trait` directamente:

```rust
trait MyService {
    async fn process(&self);
}

// ERROR: no puedes usar dyn MyService
// fn use_service(s: &dyn MyService) { ... }
```

Para `dyn Trait`, necesitas el crate **async-trait** o
retornar un `Box<dyn Future>` manualmente:

```rust
// Con el crate async-trait:
use async_trait::async_trait;

#[async_trait]
trait MyService {
    async fn process(&self);
}

#[async_trait]
impl MyService for ConcreteService {
    async fn process(&self) {
        do_work().await;
    }
}

// Ahora puedes usar dyn MyService
fn use_service(s: &dyn MyService) { ... }
```

### Alternativa manual: retornar BoxFuture

```rust
use std::future::Future;
use std::pin::Pin;

type BoxFuture<'a, T> = Pin<Box<dyn Future<Output = T> + Send + 'a>>;

trait MyService {
    fn process(&self) -> BoxFuture<'_, ()>;
}

impl MyService for ConcreteService {
    fn process(&self) -> BoxFuture<'_, ()> {
        Box::pin(async move {
            do_work().await;
        })
    }
}
```

---

## 8. Recursion en async fn

Las funciones async no pueden ser recursivas directamente
porque el tamano del Future generado seria infinito:

```rust
// ERROR: infinite size
async fn factorial(n: u64) -> u64 {
    if n <= 1 {
        1
    } else {
        n * factorial(n - 1).await
        // El tipo de factorial incluye a factorial,
        // que incluye a factorial, que incluye...
    }
}
```

### Solucion: Box::pin

Envolver la llamada recursiva en `Box::pin` la pone en el
heap, rompiendo la recursion de tamano:

```rust
use std::future::Future;
use std::pin::Pin;

fn factorial(n: u64) -> Pin<Box<dyn Future<Output = u64> + Send>> {
    Box::pin(async move {
        if n <= 1 {
            1
        } else {
            n * factorial(n - 1).await
        }
    })
}

#[tokio::main]
async fn main() {
    let result = factorial(10).await;
    println!("10! = {}", result);  // 3628800
}
```

```
    Sin Box::pin:                Con Box::pin:
    factorial -> Future          factorial -> Box<Future>
      contains factorial           contiene puntero (8 bytes)
        contains factorial           al heap, tamano fijo
          contains...
    Tamano: infinito             Tamano: finito
```

> **Nota**: la recursion async tiene overhead (heap allocation
> por llamada). Para recursion simple como factorial, una funcion
> sincrona es mas eficiente. Usa recursion async solo cuando
> la recursion involucra operaciones genuinamente async (como
> recorrer un arbol de directorios con I/O).

---

## 9. Cancelacion: drop de un Future

En Rust, cancelar un Future es simplemente dropearlo.
Si un Future es dropeado antes de completarse, se cancela
y todos sus recursos se liberan:

```rust
use tokio::time::{sleep, Duration};
use tokio::select;

async fn long_operation() -> String {
    println!("starting long operation");
    sleep(Duration::from_secs(60)).await;
    println!("this never prints if cancelled");
    "done".to_string()
}

#[tokio::main]
async fn main() {
    select! {
        result = long_operation() => {
            println!("completed: {}", result);
        }
        _ = sleep(Duration::from_secs(1)) => {
            println!("timeout! long_operation was cancelled");
            // long_operation's Future fue dropeado aqui
        }
    }
}
```

### Drop es cancelacion

```
    Timeline:
    long_operation: [start]----->----->----->
    timeout:        [start]-->|
                              |
                              v
                         select! elige timeout
                         long_operation.drop()  <-- cancelado
                         "this never prints" nunca se ejecuta
```

### Cancelation safety

No todos los Futures son "seguros" de cancelar en cualquier
punto. Si un Future es cancelado entre dos operaciones que
deben ser atomicas, puede dejar el estado inconsistente:

```rust
async fn transfer(from: &mut Account, to: &mut Account, amount: u64) {
    from.withdraw(amount).await;
    // Si se cancela AQUI (entre withdraw y deposit),
    // el dinero desaparecio — withdraw ocurrio pero deposit no
    to.deposit(amount).await;
}
```

```rust
// Solucion: hacer la operacion atomica (transaccion)
async fn transfer_safe(db: &Database, from: u64, to: u64, amount: u64) {
    let tx = db.begin_transaction().await;
    tx.withdraw(from, amount).await;
    tx.deposit(to, amount).await;
    tx.commit().await;
    // Si se cancela antes de commit, la transaccion hace rollback
}
```

**Regla**: piensa en que pasa si tu Future es cancelado en
cada punto de `.await`. Si la cancelacion puede dejar estado
corrupto, necesitas transacciones o cleanup en `Drop`.

---

## 10. Errores comunes

### Error 1: olvidar .await (el warning mas importante)

```rust
async fn send_notification() {
    println!("notification sent!");
}

async fn handler() {
    send_notification();  // WARNING: unused future
    // La notificacion NUNCA se envia
}
```

```rust
// BIEN:
async fn handler() {
    send_notification().await;
}
```

El compilador emite: `warning: unused implementor of Future
that must be used`. Trata este warning como un **error**.

### Error 2: usar std bloqueante dentro de async

```rust
use std::fs;

// MAL: fs::read_to_string bloquea el hilo del runtime
async fn read_config() -> String {
    fs::read_to_string("config.toml").unwrap()
    // Mientras lee del disco, NINGUNA otra tarea puede ejecutarse
}
```

```rust
// BIEN: opcion 1 — usar version async
async fn read_config() -> String {
    tokio::fs::read_to_string("config.toml").await.unwrap()
}

// BIEN: opcion 2 — spawn_blocking para funciones sync pesadas
async fn read_config() -> String {
    tokio::task::spawn_blocking(|| {
        std::fs::read_to_string("config.toml").unwrap()
    }).await.unwrap()
}
```

### Error 3: mantener datos grandes cruzando .await

```rust
// MAL: buffer de 1 MB vive en el Future mientras duerme
async fn wasteful() {
    let buffer = vec![0u8; 1_000_000];
    process(&buffer);

    sleep(Duration::from_secs(60)).await;
    // buffer esta en el enum del Future durante 60 segundos
    // ocupando 1 MB por cada tarea que ejecuta esta funcion

    println!("done");
}
```

```rust
// BIEN: dropear antes del await
async fn efficient() {
    {
        let buffer = vec![0u8; 1_000_000];
        process(&buffer);
    }  // buffer dropeado aqui

    sleep(Duration::from_secs(60)).await;
    // El Future ocupa solo bytes, no megabytes

    println!("done");
}
```

### Error 4: intentar llamar .await fuera de async

```rust
fn main() {
    let result = async_function().await;
    // ERROR: `await` is only allowed inside `async` functions and blocks
}
```

```rust
// BIEN: usar #[tokio::main] o block_on
#[tokio::main]
async fn main() {
    let result = async_function().await;
}

// O manualmente:
fn main() {
    let rt = tokio::runtime::Runtime::new().unwrap();
    let result = rt.block_on(async_function());
}
```

### Error 5: async fn recursiva sin Box::pin

```rust
// ERROR: recursive async fn has infinite size
async fn traverse(node: &Node) {
    process(node).await;
    for child in &node.children {
        traverse(child).await;  // recursion infinita de tipos
    }
}
```

```rust
// BIEN: Box::pin para recursion
fn traverse(node: &Node) -> Pin<Box<dyn Future<Output = ()> + Send + '_>> {
    Box::pin(async move {
        process(node).await;
        for child in &node.children {
            traverse(child).await;
        }
    })
}
```

---

## 11. Cheatsheet

```
ASYNC FN Y .AWAIT - REFERENCIA RAPIDA
=======================================

Declarar:
  async fn name() -> T { ... }
  // equivale a:
  fn name() -> impl Future<Output = T> { ... }

Ejecutar:
  let value = name().await;          // dentro de async context
  let value = rt.block_on(name());   // desde sync context

async blocks:
  let f = async { expr };           // captura por referencia
  let f = async move { expr };      // captura por ownership
  let v = f.await;

.await:
  - Solo dentro de async fn/block
  - Punto de suspension (cede control al runtime)
  - Con Result: future.await?  (await luego propagate)

Lazy evaluation:
  let f = async_fn();    // NADA se ejecuta
  f.await;               // AHORA se ejecuta

Lo que genera el compilador:
  - Enum con un estado por cada .await
  - Solo guarda variables que CRUZAN un .await
  - Dropear datos grandes antes de .await reduce el tamano del Future

tokio::spawn requiere:
  F: Future + Send + 'static
  -> usa async move {} para transferir ownership
  -> no captures referencias a datos locales

async fn en traits:
  Rust >= 1.75: funciona directamente
  Rust < 1.75: usar crate async-trait
  dyn Trait: necesita async-trait o Box<dyn Future>

Recursion:
  async fn directa: ERROR (tamano infinito)
  Solucion: fn name() -> Pin<Box<dyn Future<Output = T>>>
            { Box::pin(async move { ... }) }

Cancelacion:
  Drop del Future = cancelacion
  select! cancela el perdedor (drop)
  Cuidado con cancelar entre operaciones no atomicas

Operaciones bloqueantes en async:
  std::thread::sleep    -> tokio::time::sleep
  std::fs::read         -> tokio::fs::read
  Sync pesado           -> tokio::task::spawn_blocking

Tamano del Future:
  Variables que cruzan .await se almacenan en el enum
  Dropear antes de .await reduce tamano
  std::mem::size_of_val(&future) para inspeccionar
```

---

## 12. Ejercicios

### Ejercicio 1: comparar tamano de Futures

Crea varias funciones async con diferentes cantidades de
estado cruzando `.await` y compara sus tamanos:

```rust
use std::mem::size_of_val;
use tokio::time::{sleep, Duration};

async fn tiny() -> i32 {
    42
}

async fn small() -> i32 {
    let a = sleep(Duration::from_millis(1)).await;
    42
}

async fn medium() -> i32 {
    let x = 1i32;
    let y = 2i32;
    let z = 3i32;
    sleep(Duration::from_millis(1)).await;
    x + y + z
}

async fn big_before_await() -> i32 {
    let data = vec![0u8; 1024]; // 1 KB en el heap, pero Vec es ~24 bytes
    sleep(Duration::from_millis(1)).await;
    // data cruza el await — Vec (ptr, len, cap) esta en el Future
    data.len() as i32
}

async fn big_dropped_before_await() -> i32 {
    let len = {
        let data = vec![0u8; 1024];
        data.len()
    };
    // data dropeada antes del await
    sleep(Duration::from_millis(1)).await;
    len as i32
}

// TODO: crear big_array que tenga [u8; 1024] cruzando un .await

fn main() {
    let f1 = tiny();
    let f2 = small();
    let f3 = medium();
    let f4 = big_before_await();
    let f5 = big_dropped_before_await();

    println!("tiny:                  {} bytes", size_of_val(&f1));
    println!("small:                 {} bytes", size_of_val(&f2));
    println!("medium:                {} bytes", size_of_val(&f3));
    println!("big_before_await:      {} bytes", size_of_val(&f4));
    println!("big_dropped_before:    {} bytes", size_of_val(&f5));
    // TODO: imprimir big_array
}
```

Tareas:
1. Ejecuta y anota los tamanos
2. Crea `big_array` con `[u8; 1024]` cruzando un await —
   compara con `big_before_await` (que usa Vec)
3. Predice: `big_dropped_before_await` sera mas grande o
   mas chico que `big_before_await`?

**Prediccion**: `Vec<u8>` en el stack son 24 bytes (ptr + len +
cap), pero `[u8; 1024]` son 1024 bytes. Cual hace el Future
mas grande?

> **Pregunta de reflexion**: si tienes un servidor con 10,000
> conexiones concurrentes, cada una ejecutando una async fn,
> la diferencia entre un Future de 100 bytes y uno de 1100 bytes
> es 10 MB. A escala, el tamano del Future importa. Como
> afecta esto tus decisiones de diseno? Cuando vale la pena
> usar `spawn_blocking` para mover datos grandes fuera del
> Future?

### Ejercicio 2: async fn con lifetimes

Explora como los lifetimes interactuan con async fn:

```rust
use tokio::time::{sleep, Duration};

// Caso 1: referencia que NO cruza await — funciona
async fn case1(data: &[u8]) -> usize {
    data.len()  // usa data inmediatamente, sin await
}

// Caso 2: referencia que cruza await — tambien funciona,
// pero el Future retiene la referencia
async fn case2(data: &[u8]) -> usize {
    sleep(Duration::from_millis(10)).await;
    data.len()  // usa data DESPUES del await
}

// Caso 3: intentar spawn con referencia
async fn case3() {
    let data = vec![1, 2, 3];

    // TODO: esto compila? Por que?
    // tokio::spawn(async {
    //     sleep(Duration::from_millis(10)).await;
    //     println!("{:?}", data);
    // });

    // TODO: y con async move?
    // tokio::spawn(async move {
    //     sleep(Duration::from_millis(10)).await;
    //     println!("{:?}", data);
    // });
}

#[tokio::main]
async fn main() {
    let data = vec![1u8, 2, 3];

    // Usar case1 y case2
    println!("case1: {}", case1(&data).await);
    println!("case2: {}", case2(&data).await);

    // case3
    case3().await;
}
```

Tareas:
1. Descomenta cada bloque de case3 uno por uno y verifica si
   compila
2. Explica por que `tokio::spawn(async { ... })` con una
   referencia no compila
3. Explica por que `tokio::spawn(async move { ... })` si compila
4. Escribe una funcion que tome `&str` y lance un spawn —
   necesitaras clonar el string a `String`

> **Pregunta de reflexion**: `tokio::spawn` requiere `'static`
> porque la tarea puede vivir mas que la funcion que la creo
> (se ejecuta independientemente). `thread::scope` de C01
> resolvia esto garantizando que los hilos terminan dentro del
> scope. Existe algo similar en async? (Pista: investiga
> `tokio::task::LocalSet` y la propuesta de "scoped async
> tasks".)

### Ejercicio 3: cancelacion con select

Implementa un patron de timeout usando `select!` y observa
la cancelacion:

```rust
use tokio::time::{sleep, Duration, Instant};

struct ImportantWork {
    name: String,
}

impl ImportantWork {
    fn new(name: &str) -> Self {
        println!("[{}] created", name);
        ImportantWork { name: name.to_string() }
    }
}

impl Drop for ImportantWork {
    fn drop(&mut self) {
        println!("[{}] DROPPED (cancelled?)", self.name);
    }
}

async fn slow_work(name: &str, duration: Duration) -> String {
    let _work = ImportantWork::new(name);
    // Cuando este Future se cancela (drop), _work.drop() se ejecuta

    sleep(duration).await;
    println!("[{}] completed!", name);
    format!("{} result", name)
}

#[tokio::main]
async fn main() {
    // Ejercicio A: timeout
    println!("=== Timeout ===");
    let start = Instant::now();
    tokio::select! {
        result = slow_work("task", Duration::from_secs(5)) => {
            println!("Task finished: {}", result);
        }
        _ = sleep(Duration::from_secs(1)) => {
            println!("Timeout after {:?}", start.elapsed());
        }
    }
    println!();

    // Ejercicio B: race de tres tareas
    println!("=== Race ===");
    // TODO: usar select! con tres slow_work de diferentes duraciones
    // Verificar que los perdedores imprimen DROPPED
    // Verificar que solo el ganador imprime "completed!"

    println!();

    // Ejercicio C: loop con select (cancelable)
    println!("=== Cancellable loop ===");
    // TODO: implementar un loop que hace trabajo cada 500ms
    // pero se cancela despues de 2 segundos usando select!
}
```

Tareas:
1. Ejecuta el ejercicio A y observa que `ImportantWork` se
   dropea cuando el timeout gana
2. Implementa el ejercicio B con tres tareas
3. Implementa el ejercicio C con un loop cancelable
4. Verifica que `Drop` se ejecuta para cada tarea cancelada

> **Pregunta de reflexion**: la cancelacion via `drop` es
> "estructural" — los destructores se ejecutan para todas las
> variables del Future. Pero que pasa si el Future ha escrito
> datos parciales a un socket o archivo antes de ser cancelado?
> El `Drop` no puede "deshacer" esas escrituras. Como diseñarias
> una operacion async que sea segura de cancelar en cualquier
> punto?
