# Datos compartidos: Arc, Mutex, RwLock

## Índice
1. [El problema: compartir datos entre hilos](#1-el-problema-compartir-datos-entre-hilos)
2. [Arc: shared ownership entre hilos](#2-arc-shared-ownership-entre-hilos)
3. [Mutex: acceso exclusivo](#3-mutex-acceso-exclusivo)
4. [Arc + Mutex: el patrón idiomático](#4-arcmutex-el-patrón-idiomático)
5. [MutexGuard y RAII](#5-mutexguard-y-raii)
6. [RwLock: lectores múltiples, escritor exclusivo](#6-rwlock-lectores-múltiples-escritor-exclusivo)
7. [Cuándo usar Mutex vs RwLock](#7-cuándo-usar-mutex-vs-rwlock)
8. [Deadlocks](#8-deadlocks)
9. [Patrones comunes](#9-patrones-comunes)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema: compartir datos entre hilos

En T01 vimos que `move` transfiere ownership al hilo. Pero
¿qué pasa si **múltiples hilos** necesitan acceder al **mismo**
dato?

```rust
use std::thread;

fn main() {
    let mut counter = 0;

    let h1 = thread::spawn(move || {
        counter += 1;   // h1 tiene ownership de counter
    });

    let h2 = thread::spawn(move || {
        counter += 1;   // ERROR: counter ya fue movido a h1
    });
}
```

`move` transfiere ownership a **un** hilo. No puedes mover
el mismo valor a dos hilos. Y tampoco puedes compartir
referencias `&mut` — el borrow checker lo impide:

```
Problema 1: move transfiere a UN solo dueño
Problema 2: &mut solo puede existir UNA vez
Problema 3: & (shared ref) no permite mutación

¿Solución? Ownership compartido + interior mutability
             Arc<T>          +     Mutex<T>
```

---

## 2. Arc: shared ownership entre hilos

`Arc<T>` (Atomically Reference Counted) permite que múltiples
dueños apunten al mismo dato. Cada `clone()` incrementa un
contador atómico. Cuando el último Arc se dropea, el dato se
libera:

```rust
use std::sync::Arc;
use std::thread;

fn main() {
    let data = Arc::new(vec![1, 2, 3, 4, 5]);

    let mut handles = Vec::new();

    for i in 0..3 {
        let data = Arc::clone(&data);  // incrementa refcount, no copia el Vec
        let handle = thread::spawn(move || {
            println!("Thread {i}: sum = {}", data.iter().sum::<i32>());
        });
        handles.push(handle);
    }

    for h in handles {
        h.join().unwrap();
    }
    // Aquí el refcount llega a 0 → Vec se libera
}
```

### ¿Por qué Arc y no Rc?

`Rc<T>` (Reference Counted) hace lo mismo pero con un contador
**no atómico**. Es más rápido, pero no es thread-safe:

```
Rc<T>:
  refcount: counter += 1  (no atómico)
  Si dos hilos hacen clone() simultáneamente:
    hilo A lee counter=1, hilo B lee counter=1
    hilo A escribe counter=2, hilo B escribe counter=2
    → counter debería ser 3, pero es 2 → use-after-free

Arc<T>:
  refcount: AtomicUsize::fetch_add(1)  (atómico)
  Operación atómica garantiza correctitud con hilos
```

El compilador impide usar `Rc` entre hilos:

```rust
use std::rc::Rc;
let data = Rc::new(42);
thread::spawn(move || {
    println!("{data}");
});
// ERROR: `Rc<i32>` cannot be sent between threads safely
//        the trait `Send` is not implemented for `Rc<i32>`
```

### Diagrama de Arc

```
Arc::clone no copia los datos — comparte un puntero:

  Arc::clone(&data)     Arc::clone(&data)
       │                     │
       ▼                     ▼
  ┌─────────┐           ┌─────────┐
  │ Arc ptr ─┼──────────▶│ Arc ptr ─┼──┐
  └─────────┘    │       └─────────┘   │
     hilo 1      │          hilo 2     │
                 │                     │
                 ▼                     │
            ┌──────────────────┐       │
            │  refcount: 3     │◀──────┘
            │  data: [1,2,3,4] │
            └──────────────────┘
                 main thread
                 (también tiene un Arc)
```

### Arc solo da acceso de lectura

`Arc<T>` da `&T` (shared reference) a través de `Deref`.
No puedes mutar el contenido:

```rust
let data = Arc::new(vec![1, 2, 3]);
let data2 = Arc::clone(&data);

// OK: lectura
println!("{}", data[0]);

// ERROR: cannot borrow as mutable
// data.push(4);

// Para mutar necesitamos: Arc<Mutex<T>>
```

---

## 3. Mutex: acceso exclusivo

`Mutex<T>` (Mutual Exclusion) permite que **un solo hilo** a
la vez acceda al dato interior. Los demás esperan:

```rust
use std::sync::Mutex;

fn main() {
    let counter = Mutex::new(0);

    // Adquirir el lock
    {
        let mut num = counter.lock().unwrap();
        *num += 1;
    } // lock se libera aquí (MutexGuard se dropea)

    // Adquirir de nuevo
    {
        let mut num = counter.lock().unwrap();
        *num += 1;
    }

    println!("counter = {}", counter.lock().unwrap());  // 2
}
```

### ¿Qué retorna lock()?

`lock()` retorna `Result<MutexGuard<T>, PoisonError>`:

```rust
let mutex = Mutex::new(42);

let guard: MutexGuard<i32> = mutex.lock().unwrap();
// guard se comporta como &mut i32 (implementa DerefMut)
// El lock se mantiene mientras guard exista
```

- `Ok(guard)` — adquirimos el lock
- `Err(PoisonError)` — otro hilo panicked mientras tenía el lock
  (lo veremos en T04)

### Comparación con C

```
C (pthreads):                          Rust (std::sync::Mutex):
  pthread_mutex_t mutex;                 let mutex = Mutex::new(data);
  int data = 0;                          // data está DENTRO del mutex

  pthread_mutex_lock(&mutex);            let mut guard = mutex.lock().unwrap();
  data++;                                *guard += 1;
  pthread_mutex_unlock(&mutex);          drop(guard);  // o scope

  // Puedes acceder data sin lock:       // NO puedes acceder data sin lock:
  data++;  // compila, data race          // *mutex no existe sin lock()
```

La diferencia clave: en C, el mutex y los datos son **separados** —
nada te impide acceder a los datos sin el lock. En Rust, los datos
están **dentro** del Mutex — la única forma de acceder es a través
de `lock()`.

---

## 4. Arc + Mutex: el patrón idiomático

Para compartir datos mutables entre hilos:

```rust
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let counter = Arc::new(Mutex::new(0));
    let mut handles = Vec::new();

    for _ in 0..10 {
        let counter = Arc::clone(&counter);
        let handle = thread::spawn(move || {
            let mut num = counter.lock().unwrap();
            *num += 1;
            // MutexGuard se dropea al salir del closure
        });
        handles.push(handle);
    }

    for h in handles {
        h.join().unwrap();
    }

    println!("Result: {}", *counter.lock().unwrap());  // 10
}
```

### ¿Por qué Arc + Mutex y no solo Mutex?

```
Solo Mutex<T>:
  Un solo dueño → no puedes moverlo a múltiples hilos

Solo Arc<T>:
  Múltiples dueños → pero sin mutación

Arc<Mutex<T>>:
  Arc  → múltiples dueños (shared ownership entre hilos)
  Mutex → mutación segura (un hilo a la vez)
  Juntos → compartir Y mutar entre hilos
```

```
┌────────────────────────────────────┐
│              Arc                    │
│  ┌──────────────────────────────┐  │
│  │            Mutex              │  │
│  │  ┌────────────────────────┐  │  │
│  │  │     T (tus datos)      │  │  │
│  │  │     counter: 0         │  │  │
│  │  └────────────────────────┘  │  │
│  └──────────────────────────────┘  │
│  refcount: 3                       │
└────────────────────────────────────┘
  ▲          ▲          ▲
  │          │          │
hilo 1    hilo 2    hilo 3
```

### Patrón típico

```rust
// 1. Crear Arc<Mutex<T>>
let shared = Arc::new(Mutex::new(initial_value));

// 2. Clonar Arc para cada hilo
for _ in 0..N {
    let shared = Arc::clone(&shared);
    thread::spawn(move || {

        // 3. Lock para acceder
        let mut data = shared.lock().unwrap();

        // 4. Usar data
        *data = modify(*data);

        // 5. Lock se libera al salir del scope
    });
}
```

---

## 5. MutexGuard y RAII

`MutexGuard<T>` implementa RAII: adquiere el lock al crearse
y lo libera al dropearse. Esto hace imposible olvidar desbloquear:

```rust
// C: puedes olvidar unlock
pthread_mutex_lock(&mutex);
if (error) return;  // ← leak del lock: deadlock
pthread_mutex_unlock(&mutex);

// Rust: el lock se libera automáticamente
{
    let guard = mutex.lock().unwrap();
    if error { return; }  // guard se dropea → lock liberado
}  // guard se dropea → lock liberado
```

### Controlar la duración del lock

El lock se mantiene mientras el `MutexGuard` viva. Para minimizar
la contención, reduce el scope del guard:

```rust
// MAL: lock mantenido demasiado tiempo
let mut data = shared.lock().unwrap();
let result = expensive_computation(*data);  // lock held durante el cálculo
*data = result;
// guard se dropea aquí

// BIEN: lock solo para acceder, liberar antes de calcular
let value = {
    let data = shared.lock().unwrap();
    *data  // copiar el valor
};  // lock liberado

let result = expensive_computation(value);  // sin lock

{
    let mut data = shared.lock().unwrap();
    *data = result;  // lock solo para escribir
}
```

### drop explícito

Si necesitas liberar el lock antes del fin del scope:

```rust
let mut guard = mutex.lock().unwrap();
*guard += 1;
drop(guard);  // liberar ahora, no al final del scope

// Aquí el lock ya está liberado
// Otros hilos pueden adquirirlo
do_something_else();
```

---

## 6. RwLock: lectores múltiples, escritor exclusivo

`RwLock<T>` permite múltiples lectores simultáneos, pero solo
un escritor exclusivo:

```rust
use std::sync::{Arc, RwLock};
use std::thread;

fn main() {
    let config = Arc::new(RwLock::new(String::from("initial")));
    let mut handles = Vec::new();

    // 5 lectores
    for i in 0..5 {
        let config = Arc::clone(&config);
        handles.push(thread::spawn(move || {
            let val = config.read().unwrap();
            println!("Reader {i}: {val}");
            // Múltiples lectores simultáneos OK
        }));
    }

    // 1 escritor
    {
        let config = Arc::clone(&config);
        handles.push(thread::spawn(move || {
            let mut val = config.write().unwrap();
            *val = String::from("updated");
            println!("Writer: updated config");
            // Escritor exclusivo: ningún lector puede acceder
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
}
```

### Dos tipos de guard

```rust
let rwlock = RwLock::new(42);

// Lectura: múltiples simultáneos
let r1 = rwlock.read().unwrap();   // RwLockReadGuard
let r2 = rwlock.read().unwrap();   // OK: dos lectores a la vez
println!("{r1} {r2}");
drop(r1);
drop(r2);

// Escritura: exclusivo
let mut w = rwlock.write().unwrap();  // RwLockWriteGuard
*w = 100;
// let r = rwlock.read().unwrap();   // DEADLOCK: write lock held
drop(w);
```

### Diagrama de acceso

```
Mutex<T>:
  ┌────────┐
  │ lock() │ → solo UN hilo a la vez (lectura o escritura)
  └────────┘

RwLock<T>:
  ┌─────────┐
  │ read()  │ → MUCHOS lectores simultáneos
  └─────────┘
  ┌─────────┐
  │ write() │ → UN solo escritor, sin lectores
  └─────────┘

Acceso concurrente:
  Reader + Reader = OK       (RwLock)
  Reader + Writer = BLOCKED  (RwLock)
  Writer + Writer = BLOCKED  (RwLock y Mutex)
  Any    + Any    = BLOCKED  (Mutex)
```

---

## 7. Cuándo usar Mutex vs RwLock

### Mutex

```
✓ Escrituras frecuentes
✓ Locks de corta duración
✓ Menos overhead que RwLock
✓ Más simple (un solo tipo de guard)
✓ Default: cuando no estás seguro, usa Mutex
```

### RwLock

```
✓ Muchas lecturas, pocas escrituras
✓ Lecturas largas (parsear config, buscar en cache)
✓ Múltiples lectores realmente necesitan acceso simultáneo

✗ Writer starvation: si lectores nunca paran, el escritor espera
  indefinidamente (depende de la implementación del OS)
✗ Más overhead por lock (mantener contadores de lectores)
```

### Ejemplo práctico

```rust
// Config del servidor: se lee en cada request, se cambia raramente
// → RwLock ideal
let config = Arc::new(RwLock::new(ServerConfig::default()));

// worker: lectura frecuente
let timeout = config.read().unwrap().request_timeout;

// admin endpoint: escritura rara
let mut cfg = config.write().unwrap();
cfg.request_timeout = Duration::from_secs(30);
```

```rust
// Contador de requests: se escribe en cada request
// → Mutex (o AtomicUsize)
let request_count = Arc::new(Mutex::new(0u64));

// worker: escritura frecuente
let mut count = request_count.lock().unwrap();
*count += 1;
```

---

## 8. Deadlocks

Rust previene **data races** en compile time, pero **no**
previene deadlocks. Los deadlocks son un error lógico, no de
tipos.

### Deadlock 1: lock del mismo mutex dos veces

```rust
let mutex = Mutex::new(0);

let guard1 = mutex.lock().unwrap();
let guard2 = mutex.lock().unwrap();  // DEADLOCK: guard1 tiene el lock
// El segundo lock espera que el primero se libere
// El primero nunca se libera porque estamos esperando el segundo
```

### Deadlock 2: orden inconsistente (A-B vs B-A)

```rust
let mutex_a = Arc::new(Mutex::new(0));
let mutex_b = Arc::new(Mutex::new(0));

// Hilo 1: lock A, luego B
let a = Arc::clone(&mutex_a);
let b = Arc::clone(&mutex_b);
thread::spawn(move || {
    let _ga = a.lock().unwrap();     // adquiere A
    thread::sleep(Duration::from_millis(10));
    let _gb = b.lock().unwrap();     // espera B (hilo 2 lo tiene)
});

// Hilo 2: lock B, luego A
let a = Arc::clone(&mutex_a);
let b = Arc::clone(&mutex_b);
thread::spawn(move || {
    let _gb = b.lock().unwrap();     // adquiere B
    thread::sleep(Duration::from_millis(10));
    let _ga = a.lock().unwrap();     // espera A (hilo 1 lo tiene)
});

// Hilo 1 tiene A, espera B
// Hilo 2 tiene B, espera A
// → ninguno avanza → DEADLOCK
```

### Prevenir deadlocks

```
Regla 1: orden consistente
  Siempre adquirir locks en el mismo orden (A antes que B)

Regla 2: minimizar locks anidados
  Evitar tener dos locks al mismo tiempo
  Si es necesario, agrupar datos en un solo Mutex

Regla 3: try_lock
  mutex.try_lock() retorna Err si el lock está ocupado
  No bloquea → no puede deadlockear
  Pero necesitas manejar el caso de fallo
```

```rust
// try_lock: intentar sin bloquear
match mutex.try_lock() {
    Ok(guard) => { /* adquirimos el lock */ },
    Err(_)    => { /* ocupado — reintentar o skip */ },
}
```

---

## 9. Patrones comunes

### Contador compartido

```rust
let counter = Arc::new(Mutex::new(0u64));
let mut handles = Vec::new();

for _ in 0..100 {
    let counter = Arc::clone(&counter);
    handles.push(thread::spawn(move || {
        for _ in 0..1000 {
            *counter.lock().unwrap() += 1;
        }
    }));
}

for h in handles { h.join().unwrap(); }
println!("{}", counter.lock().unwrap());  // 100_000
```

### HashMap compartido

```rust
use std::collections::HashMap;

let map = Arc::new(Mutex::new(HashMap::new()));

let map_clone = Arc::clone(&map);
thread::spawn(move || {
    map_clone.lock().unwrap().insert("key1", "value1");
});

let map_clone = Arc::clone(&map);
thread::spawn(move || {
    map_clone.lock().unwrap().insert("key2", "value2");
});
```

### Vec compartido para recolectar resultados

```rust
let results = Arc::new(Mutex::new(Vec::new()));
let mut handles = Vec::new();

for i in 0..10 {
    let results = Arc::clone(&results);
    handles.push(thread::spawn(move || {
        let result = expensive_work(i);
        results.lock().unwrap().push((i, result));
    }));
}

for h in handles { h.join().unwrap(); }

let results = results.lock().unwrap();
for (id, val) in results.iter() {
    println!("Task {id}: {val}");
}
```

### Cache read-heavy con RwLock

```rust
use std::collections::HashMap;

let cache: Arc<RwLock<HashMap<String, String>>> =
    Arc::new(RwLock::new(HashMap::new()));

// Lectura (frecuente): muchos lectores simultáneos
let cache = Arc::clone(&cache);
thread::spawn(move || {
    let c = cache.read().unwrap();
    if let Some(val) = c.get("key") {
        println!("cache hit: {val}");
    }
});

// Escritura (rara): un escritor exclusivo
let cache = Arc::clone(&cache);
thread::spawn(move || {
    cache.write().unwrap()
        .insert("key".to_string(), "value".to_string());
});
```

---

## 10. Errores comunes

### Error 1: intentar usar Rc entre hilos

```rust
use std::rc::Rc;

let data = Rc::new(42);
thread::spawn(move || {
    println!("{data}");
});
// ERROR: `Rc<i32>` cannot be sent between threads safely
```

**Solución**: usar Arc:

```rust
use std::sync::Arc;

let data = Arc::new(42);
thread::spawn(move || {
    println!("{data}");
});
```

### Error 2: mantener el lock demasiado tiempo

```rust
// MAL: lock mantenido durante I/O
let guard = shared.lock().unwrap();
let result = reqwest::blocking::get(&url)?;  // puede tardar segundos
*guard = result;
// Todos los demás hilos bloqueados durante la descarga
```

**Solución**: copiar datos, liberar lock, procesar, re-lock:

```rust
let url = {
    let guard = shared.lock().unwrap();
    guard.url.clone()  // copiar lo necesario
};  // lock liberado

let result = reqwest::blocking::get(&url)?;  // sin lock

shared.lock().unwrap().result = result;  // lock breve para escribir
```

### Error 3: deadlock por lock anidado

```rust
let m = Mutex::new(0);
let guard = m.lock().unwrap();
// ...
let guard2 = m.lock().unwrap();  // DEADLOCK
```

**Solución**: dropear el primer guard antes:

```rust
let m = Mutex::new(0);
{
    let guard = m.lock().unwrap();
    // usar guard
}  // guard dropeado, lock libre

let guard2 = m.lock().unwrap();  // OK
```

### Error 4: Arc::clone vs .clone() en el dato

```rust
let data = Arc::new(vec![1, 2, 3]);

// BIEN: clonar el Arc (barato — solo incrementa refcount)
let data2 = Arc::clone(&data);

// CUIDADO: esto clona el Vec DENTRO del Arc (caro — copia datos)
let data3 = (*data).clone();  // o data.as_ref().clone()
```

La convención en Rust es escribir `Arc::clone(&x)` en vez de
`x.clone()` para hacer explícito que es un clone de Arc (barato),
no del dato interior (potencialmente caro).

### Error 5: olvidar que MutexGuard no es Send

```rust
let mutex = Arc::new(Mutex::new(0));
let guard = mutex.lock().unwrap();

thread::spawn(move || {
    // ERROR: MutexGuard no es Send
    // No puedes mover un lock adquirido a otro hilo
    println!("{}", *guard);
});
```

**Solución**: adquirir el lock dentro del hilo:

```rust
let mutex = Arc::clone(&mutex);
thread::spawn(move || {
    let guard = mutex.lock().unwrap();
    println!("{}", *guard);
});
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│         Datos Compartidos entre Hilos                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Arc<T>  (Atomic Reference Counted):                         │
│    Shared ownership entre hilos                              │
│    Arc::new(data)           crear                            │
│    Arc::clone(&arc)         clonar (barato, solo refcount)   │
│    Solo lectura (&T via Deref)                               │
│    Rc no es Send → usar Arc para hilos                       │
│                                                              │
│  Mutex<T>  (Mutual Exclusion):                               │
│    Un solo hilo a la vez                                     │
│    mutex.lock().unwrap()    → MutexGuard<T> (&mut T)         │
│    mutex.try_lock()         → Result (no bloquea)            │
│    Guard se dropea → lock liberado (RAII)                    │
│    Datos DENTRO del mutex: imposible acceder sin lock        │
│                                                              │
│  RwLock<T>  (Read-Write Lock):                               │
│    Muchos lectores O un escritor                             │
│    rwlock.read().unwrap()   → RwLockReadGuard (&T)           │
│    rwlock.write().unwrap()  → RwLockWriteGuard (&mut T)      │
│    Ideal: muchas lecturas, pocas escrituras                  │
│                                                              │
│  PATRÓN IDIOMÁTICO:                                          │
│    let shared = Arc::new(Mutex::new(data));                  │
│    let shared = Arc::clone(&shared);  // para cada hilo      │
│    thread::spawn(move || {                                   │
│        let mut guard = shared.lock().unwrap();               │
│        *guard = new_value;                                   │
│    });                                                       │
│                                                              │
│  MINIMIZAR CONTENCIÓN:                                       │
│    Scope del guard lo más pequeño posible                    │
│    Copiar datos dentro del lock, procesar fuera              │
│    drop(guard) para liberar antes del fin del scope          │
│                                                              │
│  DEADLOCKS (Rust NO los previene):                           │
│    Lock mismo mutex dos veces → deadlock                     │
│    Lock A→B en hilo 1, B→A en hilo 2 → deadlock             │
│    Prevenir: orden consistente, try_lock, minimizar nesting  │
│                                                              │
│  ELEGIR:                                                     │
│    Mutex    → default, escrituras frecuentes, simple         │
│    RwLock   → muchas lecturas, pocas escrituras              │
│    Atomic   → contadores simples (ver T02 de S02)            │
│    Channel  → message passing (ver T03)                      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: contador con Arc + Mutex

Implementa un contador compartido incrementado por 10 hilos.

**Pasos**:
1. Crea `Arc::new(Mutex::new(0u64))`
2. Spawn 10 hilos, cada uno incrementa el counter 10_000 veces
3. Join todos los hilos
4. Imprime el resultado final (debe ser 100_000)
5. Verifica que sin Mutex el compilador no permite la mutación

**Predicción antes de ejecutar**: ¿el resultado siempre es
exactamente 100_000, o puede variar entre ejecuciones? ¿Qué
pasaría en C sin mutex con la misma lógica? ¿El orden de
incrementos entre hilos está garantizado?

> **Pregunta de reflexión**: cada incremento adquiere y libera
> el lock. Con 10 hilos × 10_000 = 100_000 lock/unlock, ¿cuánta
> contención hay? ¿Sería más eficiente hacer
> `lock → incrementar 10_000 veces → unlock` en cada hilo?
> ¿Cuál es el trade-off entre granularidad del lock y
> contención?

### Ejercicio 2: HashMap compartido de resultados

Implementa un map donde múltiples hilos insertan resultados.

**Pasos**:
1. Crea `Arc::new(Mutex::new(HashMap::<u32, u64>::new()))`
2. Spawn 8 hilos, cada uno calcula el factorial de su id (0..8)
3. Cada hilo inserta `(id, factorial)` en el HashMap
4. Join todos, luego itera el HashMap e imprime los resultados
   ordenados por clave

**Predicción antes de ejecutar**: ¿los resultados en el HashMap
están en el orden 0..7 o en orden arbitrario? ¿Dos hilos pueden
insertar al mismo tiempo? ¿Qué pasa si usas `BTreeMap` en vez
de `HashMap` — los resultados se imprimen ordenados sin sort
explícito?

> **Pregunta de reflexión**: cada hilo adquiere el lock solo
> para insertar (operación rápida). ¿Sería diferente si cada
> hilo leyera un archivo grande y luego insertara el resultado?
> ¿Conviene adquirir el lock antes o después de leer el archivo?
> ¿Qué patrón usarías si los hilos necesitaran leer Y escribir
> el HashMap frecuentemente? (Pista: RwLock.)

### Ejercicio 3: config compartida con RwLock

Implementa una configuración compartida que se lee frecuentemente
y se actualiza raramente.

**Pasos**:
1. Define `struct Config { max_connections: u32, timeout_ms: u64 }`
2. Crea `Arc::new(RwLock::new(Config { ... }))`
3. Spawn 5 hilos "reader" que leen la config cada 100ms y la
   imprimen (10 lecturas cada uno)
4. Spawn 1 hilo "writer" que actualiza `timeout_ms` cada 500ms
   (3 actualizaciones)
5. Join todos y verifica que los readers ven los cambios del writer

**Predicción antes de ejecutar**: ¿los 5 readers pueden leer
simultáneamente? ¿Qué pasa con los readers cuando el writer
está escribiendo? ¿Todos los readers ven el mismo valor de
timeout_ms, o puede haber inconsistencia?

> **Pregunta de reflexión**: si el writer hace updates muy
> frecuentes (cada 1ms) y hay muchos readers, el writer podría
> sufrir "starvation" — nunca obtiene el write lock porque
> siempre hay lectores. ¿Cómo se comporta la implementación
> de RwLock de la std en este caso? ¿Prioriza lectores o
> escritores? ¿Qué crate usarías si necesitaras garantías
> de fairness? (Pista: `parking_lot::RwLock`.)
