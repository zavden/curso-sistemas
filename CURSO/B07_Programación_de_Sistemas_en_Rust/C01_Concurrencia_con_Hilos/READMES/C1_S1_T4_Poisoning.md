# Poisoning

## Índice
1. [¿Qué es mutex poisoning?](#1-qué-es-mutex-poisoning)
2. [Cómo se produce el poisoning](#2-cómo-se-produce-el-poisoning)
3. [PoisonError y recuperación](#3-poisonerror-y-recuperación)
4. [into_inner: acceder a datos envenenados](#4-into_inner-acceder-a-datos-envenenados)
5. [¿Es el poisoning realmente útil?](#5-es-el-poisoning-realmente-útil)
6. [RwLock y poisoning](#6-rwlock-y-poisoning)
7. [Alternativas al poisoning](#7-alternativas-al-poisoning)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. ¿Qué es mutex poisoning?

Cuando un hilo paniquea mientras tiene un `MutexGuard`, el
Mutex queda marcado como **envenenado** (poisoned). Los
intentos siguientes de adquirir el lock retornan `Err`:

```rust
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let data = Arc::new(Mutex::new(vec![1, 2, 3]));

    let data_clone = Arc::clone(&data);
    let handle = thread::spawn(move || {
        let mut guard = data_clone.lock().unwrap();
        guard.push(4);
        panic!("crash!");  // panic con el lock held
        // guard se dropea durante el unwind → Mutex se envenena
    });

    let _ = handle.join();  // el hilo panicked

    // Intentar adquirir el lock
    match data.lock() {
        Ok(guard) => println!("data: {:?}", *guard),
        Err(poisoned) => {
            println!("Mutex poisoned!");
            // Los datos pueden estar en un estado inconsistente
            let guard = poisoned.into_inner();
            println!("data (possibly corrupt): {:?}", *guard);
        }
    }
}
```

### ¿Por qué existe el poisoning?

El poisoning es un mecanismo de **seguridad**. Si un hilo
paniquea a mitad de una modificación, los datos pueden haber
quedado en un estado parcial o inconsistente:

```
Operación prevista: mover elemento de vec_a a vec_b

  1. guard_a.remove(index)   ← se ejecutó (elemento removido de A)
  2. guard_b.push(element)   ← PANIC antes de esto

Estado resultante:
  vec_a: sin el elemento (ya fue removido)
  vec_b: sin el elemento (nunca se insertó)
  → elemento perdido → estado inconsistente
```

Sin poisoning, el siguiente hilo adquiriría el lock y operaría
sobre datos corruptos sin saberlo. Con poisoning, el Mutex
dice: "un hilo murió aquí — ten cuidado".

---

## 2. Cómo se produce el poisoning

El envenenamiento ocurre **solo** cuando un `MutexGuard` se
dropea durante un panic unwind. El flujo:

```
Hilo:
  1. let guard = mutex.lock().unwrap()  → adquiere lock
  2. modificar datos a través de guard
  3. panic!("error")                     → comienza stack unwinding
  4. Drop de guard (parte del unwind)    → MutexGuard::drop()
     → detecta que estamos en unwind (std::thread::panicking())
     → marca el Mutex como poisoned
  5. Hilo muere

Otro hilo:
  6. mutex.lock()                        → retorna Err(PoisonError)
```

### ¿Qué detecta el guard en drop?

```rust
// Simplificación conceptual de MutexGuard::drop
impl<T> Drop for MutexGuard<'_, T> {
    fn drop(&mut self) {
        if std::thread::panicking() {
            // Estamos en unwind → marcar como poisoned
            self.mutex.set_poisoned(true);
        }
        // Siempre liberar el lock
        self.mutex.unlock();
    }
}
```

El lock siempre se libera — no hay deadlock por panic. Pero
el flag de poisoned queda activado permanentemente.

### Cuándo NO se produce poisoning

```rust
// Caso 1: panic sin lock held → no poisoning
let mutex = Mutex::new(42);
// mutex.lock() nunca fue llamado
panic!("crash");  // el mutex NO se envenena

// Caso 2: guard dropeado antes del panic → no poisoning
let mutex = Mutex::new(42);
{
    let guard = mutex.lock().unwrap();
    // usar guard
}  // guard dropeado aquí (scope termina)
panic!("crash");  // el mutex NO se envenena (guard ya no existe)

// Caso 3: drop explícito antes del panic → no poisoning
let guard = mutex.lock().unwrap();
drop(guard);  // liberar explícitamente
panic!("crash");  // no poisoning
```

---

## 3. PoisonError y recuperación

`mutex.lock()` retorna `Result<MutexGuard<T>, PoisonError<MutexGuard<T>>>`:

```rust
use std::sync::PoisonError;

match mutex.lock() {
    Ok(guard) => {
        // Lock adquirido, datos presumiblemente consistentes
        println!("{}", *guard);
    }
    Err(poisoned) => {
        // El Mutex fue envenenado
        // poisoned: PoisonError<MutexGuard<T>>

        // Opción 1: propagar el error
        // return Err(...)

        // Opción 2: acceder a los datos de todos modos
        let guard = poisoned.into_inner();
        println!("recovered: {}", *guard);

        // Opción 3: re-panic
        // panic!("mutex poisoned");
    }
}
```

### Los tres enfoques

```
1. unwrap() / expect() — paniquear si poisoned
   mutex.lock().unwrap()
   → Si el mutex está envenenado, main también paniquea
   → Apropiado cuando un poison indica un bug fatal

2. into_inner() — ignorar el poison, acceder a los datos
   mutex.lock().unwrap_or_else(|e| e.into_inner())
   → Acceder a los datos aunque puedan ser inconsistentes
   → Apropiado cuando puedes validar/reparar los datos

3. Limpiar y continuar
   mutex.clear_poison()  (Rust 1.77+)
   → Desmarcar el poison para futuros accesos
   → Apropiado después de verificar que los datos son válidos
```

---

## 4. into_inner: acceder a datos envenenados

`PoisonError::into_inner()` extrae el `MutexGuard`, dando
acceso a los datos a pesar del envenenamiento:

```rust
use std::sync::{Arc, Mutex};
use std::thread;

fn main() {
    let counter = Arc::new(Mutex::new(0u32));

    // Hilo que paniquea después de modificar
    {
        let counter = Arc::clone(&counter);
        let _ = thread::spawn(move || {
            let mut guard = counter.lock().unwrap();
            *guard = 42;
            panic!("oops");
        }).join();
    }

    // Acceder a pesar del poison
    let value = counter.lock()
        .unwrap_or_else(|poisoned| {
            println!("Mutex was poisoned, recovering...");
            poisoned.into_inner()
        });

    println!("Value: {}", *value);  // 42
}
```

### El atajo: unwrap_or_else

El patrón `unwrap_or_else(|e| e.into_inner())` es tan común
que se convierte en idiomático para "ignorar el poison":

```rust
// Largo
let guard = match mutex.lock() {
    Ok(g) => g,
    Err(e) => e.into_inner(),
};

// Corto (equivalente)
let guard = mutex.lock().unwrap_or_else(|e| e.into_inner());
```

### clear_poison (Rust 1.77+)

Después de verificar que los datos son válidos, puedes limpiar
el flag de poison:

```rust
let guard = mutex.lock().unwrap_or_else(|e| {
    let g = e.into_inner();
    // Verificar/reparar datos...
    mutex.clear_poison();
    g
});
// Futuros lock() retornan Ok, no Err
```

---

## 5. ¿Es el poisoning realmente útil?

El poisoning es un tema debatido en la comunidad Rust. Hay
argumentos a favor y en contra:

### A favor

```
1. Señala estado potencialmente corrupto
   Si un hilo crasheó a mitad de una operación, los datos
   pueden ser inválidos. El poison te obliga a decidir
   conscientemente si quieres continuar.

2. Propaga errores
   Un panic en un worker thread se convierte en un Err
   que el hilo principal puede detectar y manejar.

3. Fail-fast
   Si usas unwrap(), un poison causa panic en cadena —
   rápidamente detienes el programa en vez de operar
   con datos corruptos silenciosamente.
```

### En contra

```
1. Falsos positivos
   Muchos panics no corrompen datos del mutex.
   Ejemplo: panic por un assert en código que ni toca el mutex.
   El mutex se envenena aunque sus datos son perfectos.

2. Ruido
   Obliga a manejar PoisonError en cada .lock(), incluso
   cuando la probabilidad de poison es prácticamente cero.
   El 99% del código usa .unwrap() y nunca ve un poison.

3. parking_lot::Mutex no tiene poisoning
   El crate parking_lot decidió eliminar poisoning
   porque lo consideraron más noise que valor.
```

### La posición práctica

```rust
// En la mayoría del código: unwrap y olvidarse
let guard = mutex.lock().unwrap();
// Si hay un poison, el programa paniquea — que es lo que
// probablemente quieres de todos modos

// En código robusto (servidores, daemons): manejar
let guard = mutex.lock().unwrap_or_else(|e| e.into_inner());
// O verificar los datos y clear_poison
```

---

## 6. RwLock y poisoning

`RwLock<T>` también soporta poisoning, pero con una diferencia
importante:

```
Mutex:
  lock() durante panic → poisoned

RwLock:
  write() durante panic → poisoned
  read() durante panic  → NO poisoned
```

Un panic durante una lectura no puede corromper los datos (el
lector no los modifica), así que el RwLock solo se envenena
si un **escritor** paniquea:

```rust
use std::sync::RwLock;

let lock = RwLock::new(42);

// Panic durante read → NO poison
let _ = std::panic::catch_unwind(|| {
    let _guard = lock.read().unwrap();
    panic!("reader panic");
});
assert!(lock.read().is_ok());   // OK: no poisoned
assert!(lock.write().is_ok());  // OK: no poisoned

// Panic durante write → poison
let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
    let _guard = lock.write().unwrap();
    panic!("writer panic");
}));
assert!(lock.read().is_err());   // Err: poisoned
assert!(lock.write().is_err());  // Err: poisoned
```

### ¿Por qué read no envenena?

```
read():  el guard es &T (shared reference, inmutable)
         → no puede modificar los datos
         → panic no puede dejar datos inconsistentes
         → no tiene sentido envenenar

write(): el guard es &mut T (exclusive reference, mutable)
         → puede modificar los datos
         → panic puede dejar datos a medio modificar
         → envenenar tiene sentido
```

---

## 7. Alternativas al poisoning

### parking_lot::Mutex (sin poisoning)

El crate `parking_lot` ofrece primitivas de sincronización más
rápidas y sin poisoning:

```rust
// Cargo.toml: parking_lot = "0.12"
use parking_lot::Mutex;

let mutex = Mutex::new(42);
let guard = mutex.lock();  // retorna MutexGuard directamente, no Result
// Sin .unwrap() necesario
// Si otro hilo panicked con el lock, el mutex simplemente se desbloquea
// sin marcar poison
```

```
std::sync::Mutex:                parking_lot::Mutex:
  lock() → Result<Guard, Poison>   lock() → Guard
  unwrap() en cada acceso          sin unwrap
  Poisoning: sí                    Poisoning: no
  OS mutex (syscall)               Spinning + futex (más rápido)
```

### Atomics (sin lock)

Para datos simples (contadores, flags), los atomics evitan
el problema por completo:

```rust
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

let counter = Arc::new(AtomicU64::new(0));
let c = Arc::clone(&counter);

thread::spawn(move || {
    c.fetch_add(1, Ordering::Relaxed);
    panic!("crash");
    // No hay mutex → no hay poisoning
    // El increment atómico ya se completó
});

// El counter se incrementó correctamente
println!("{}", counter.load(Ordering::Relaxed));  // 1
```

### catch_unwind para prevenir poisoning

`std::panic::catch_unwind` captura un panic antes de que
envenene el mutex:

```rust
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::sync::Mutex;

let mutex = Mutex::new(vec![1, 2, 3]);

let result = catch_unwind(AssertUnwindSafe(|| {
    let mut guard = mutex.lock().unwrap();
    guard.push(4);
    panic!("error");
}));
// catch_unwind capturó el panic
// El guard fue dropeado durante el unwind DENTRO de catch_unwind
// → el mutex SE envenena (catch_unwind no previene poison)

// Nota: catch_unwind NO previene el poisoning. El guard sigue
// dropeándose en contexto de panic. Lo que catch_unwind hace es
// evitar que el HILO muera — puedes continuar ejecutando código.
```

En realidad, `catch_unwind` **no** previene el poisoning del
mutex. El guard se dropea durante el unwind y detecta
`panicking() == true`. Lo que sí hace es evitar que el hilo
muera, permitiéndote manejar el error.

---

## 8. Errores comunes

### Error 1: ignorar ciegamente el PoisonError

```rust
// MAL: siempre ignorar el poison sin verificar datos
let guard = mutex.lock().unwrap_or_else(|e| e.into_inner());
*guard += 1;  // ¿y si los datos están corruptos?
```

**Solución**: al menos loguear y considerar el estado:

```rust
let guard = match mutex.lock() {
    Ok(g) => g,
    Err(e) => {
        eprintln!("WARNING: mutex was poisoned, recovering");
        let g = e.into_inner();
        // Validar datos si es posible
        g
    }
};
```

### Error 2: confundir panic con deadlock

```rust
// El Mutex NO queda locked después de un panic
// (el lock siempre se libera en drop)
// Pero sí queda POISONED

let mutex = Mutex::new(0);
// Hilo paniquea con el lock → lock se libera → poison flag se activa
// Otro hilo: lock() → no deadlock, pero sí Err(PoisonError)
```

El lock se libera siempre. Poisoning no es un deadlock — es un
flag informativo.

### Error 3: asumir que unwrap siempre paniquea por datos corruptos

```rust
// El hilo panicked por un error de I/O, no por datos del mutex
thread::spawn(move || {
    let _guard = data.lock().unwrap();
    let file = File::open("noexiste.txt").unwrap();  // panic aquí
    // Los datos del mutex están intactos, pero el mutex se envenena
});

// mutex.lock().unwrap() → panic, aunque los datos están bien
```

**Solución**: en servidores donde quieres robustez ante panics
en workers, siempre manejar el poison:

```rust
let guard = data.lock().unwrap_or_else(|e| e.into_inner());
```

### Error 4: no saber que el poison es permanente

```rust
// El poison no se limpia solo
// Cada lock() futuro retorna Err hasta que se limpie manualmente

mutex.lock();  // Err(PoisonError)
mutex.lock();  // Err(PoisonError) — sigue envenenado
mutex.lock();  // Err(PoisonError) — para siempre

// Limpiar (Rust 1.77+):
mutex.clear_poison();
mutex.lock();  // Ok(guard) — limpio de nuevo
```

### Error 5: poisoning en Condvar

`Condvar::wait()` también propaga poisoning. Si el mutex
asociado está envenenado, `wait()` retorna `Err`:

```rust
let pair = Arc::new((Mutex::new(false), Condvar::new()));

// Si el mutex se envenena:
let (lock, cvar) = &*pair;
let result = cvar.wait(lock.lock().unwrap());
// Si el mutex fue poisoned entre wait calls → Err
```

---

## 9. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                   Mutex Poisoning                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ¿QUÉ ES?                                                   │
│    Hilo paniquea con MutexGuard held                         │
│    → Mutex queda marcado como "poisoned"                     │
│    → Futuros lock() retornan Err(PoisonError)                │
│                                                              │
│  ¿POR QUÉ?                                                  │
│    Datos pueden estar en estado inconsistente                │
│    (modificación parcial interrumpida por panic)             │
│                                                              │
│  ¿CÓMO MANEJAR?                                             │
│    mutex.lock().unwrap()                                     │
│      → panic si poisoned (fail-fast)                         │
│                                                              │
│    mutex.lock().unwrap_or_else(|e| e.into_inner())           │
│      → ignorar poison, acceder a datos                       │
│                                                              │
│    match mutex.lock() { Ok(g) => ..., Err(e) => ... }       │
│      → decidir caso por caso                                │
│                                                              │
│    mutex.clear_poison()   (Rust 1.77+)                       │
│      → desmarcar el poison                                   │
│                                                              │
│  CUÁNDO SE PRODUCE:                                          │
│    ✓ Panic con MutexGuard vivo                               │
│    ✓ Panic con RwLockWriteGuard vivo                         │
│    ✗ Panic con RwLockReadGuard (NO envenena)                 │
│    ✗ Panic sin guard (mutex no afectado)                     │
│    ✗ Guard dropeado antes del panic (no envenena)            │
│                                                              │
│  EL LOCK SIEMPRE SE LIBERA:                                  │
│    Poisoning ≠ deadlock                                      │
│    El guard se dropea durante unwind → unlock                │
│    El flag de poison es independiente del estado del lock     │
│                                                              │
│  ALTERNATIVAS SIN POISONING:                                 │
│    parking_lot::Mutex    lock() retorna Guard directamente   │
│    Atomics               sin lock, sin poison posible        │
│                                                              │
│  EN LA PRÁCTICA:                                             │
│    99% del código: .unwrap() → panic si poison               │
│    Servidores: unwrap_or_else(|e| e.into_inner())            │
│    El poisoning rara vez es el error que te preocupa         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 10. Ejercicios

### Ejercicio 1: observar el poisoning

Provoca y observa un mutex poisoning, luego recupérate.

**Pasos**:
1. Crea `Arc::new(Mutex::new(vec![1, 2, 3]))`
2. Spawn un hilo que adquiera el lock, haga push(4), y luego
   `panic!("crash")`
3. Join el hilo (captura el Err de join)
4. En el main thread, intenta `mutex.lock()` — observa el
   `Err(PoisonError)`
5. Usa `into_inner()` para acceder al Vec y verificar su
   contenido
6. ¿El Vec tiene [1,2,3] o [1,2,3,4]?

**Predicción antes de ejecutar**: el push(4) se ejecutó
**antes** del panic. ¿El Vec tiene el 4 o no? ¿El poison
indica que los datos cambiaron, o solo que un panic ocurrió?

> **Pregunta de reflexión**: en este caso, el push completó
> antes del panic — los datos son consistentes. ¿Cómo
> distinguirías un poison donde los datos SÍ quedaron
> corruptos de uno donde los datos están bien? ¿El mecanismo
> de poisoning te da esa información?

### Ejercicio 2: RwLock y cuándo se envenena

Demuestra que un panic en read() no envenena pero en write() sí.

**Pasos**:
1. Crea `Arc::new(RwLock::new(42))`
2. Spawn un hilo que haga `read().unwrap()` y luego panic
3. Join el hilo, luego verifica que `rwlock.read()` retorna Ok
4. Spawn otro hilo que haga `write().unwrap()` y luego panic
5. Join el hilo, luego verifica que `rwlock.read()` retorna Err
6. Recupera el valor con `into_inner()` y verifica que es 42

**Predicción antes de ejecutar**: después del panic en read,
¿el RwLock sigue usable? ¿Después del panic en write, tanto
read() como write() retornan Err? ¿El writer modificó el
valor antes de panickear?

> **Pregunta de reflexión**: ¿tiene sentido que un reader panic
> no envenene? Un reader tiene `&T` — no puede modificar datos.
> Pero ¿qué pasaría si el reader usara `unsafe` para mutar a
> través de `&T`? (Interior mutability via UnsafeCell.) ¿El
> mecanismo de poisoning cubre ese caso?

### Ejercicio 3: servidor robusto con poison handling

Implementa un "servidor" (simulado) que sobrevive al crash
de un worker sin perder su estado compartido.

**Pasos**:
1. Crea `Arc::new(Mutex::new(Vec::<String>::new()))` como log
   compartido
2. Spawn 5 workers: cada uno adquiere el lock, pushea
   `"worker N: processed"`, y libera el lock
3. El worker 2 paniquea DESPUÉS de pushear su mensaje
4. Los workers 3 y 4 deben detectar el poison y continuar
   operando (usar `unwrap_or_else`)
5. Al final, imprime el contenido del log — ¿están los
   mensajes de todos los workers que completaron?

**Predicción antes de ejecutar**: ¿el worker 3 ve el poison
que causó el worker 2? ¿Los workers 3, 4, 5 pueden seguir
operando si usan `unwrap_or_else`? ¿El log contiene el
mensaje del worker 2 (que sí completó su push)?

> **Pregunta de reflexión**: en un servidor real, ¿qué harías
> al detectar un mutex envenenado: ignorar, reiniciar el
> estado, o detener el servidor? ¿Depende de qué datos protege
> el mutex? ¿Qué estrategia usa un servidor como nginx cuando
> un worker process crashea? (Pista: nginx usa procesos, no
> hilos — ¿elimina esto el problema del poisoning?)
