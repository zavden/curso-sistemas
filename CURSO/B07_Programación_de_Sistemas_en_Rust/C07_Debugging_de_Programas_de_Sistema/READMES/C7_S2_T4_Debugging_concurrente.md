# Debugging concurrente en Rust

## Índice

1. [El problema de los bugs concurrentes](#el-problema-de-los-bugs-concurrentes)
2. [ThreadSanitizer (TSan)](#threadsanitizer-tsan)
3. [Interpretar errores de TSan](#interpretar-errores-de-tsan)
4. [Data races vs race conditions](#data-races-vs-race-conditions)
5. [Miri para concurrencia](#miri-para-concurrencia)
6. [Debugging de deadlocks](#debugging-de-deadlocks)
7. [Herramientas de logging para concurrencia](#herramientas-de-logging-para-concurrencia)
8. [Patrones de debugging concurrente](#patrones-de-debugging-concurrente)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El problema de los bugs concurrentes

Los bugs de concurrencia son los más difíciles de debuggear porque son
**no-determinísticos**: pueden aparecer una vez cada mil ejecuciones, solo
bajo carga, o solo en una máquina específica.

```
┌──────────────────────────────────────────────────────────────┐
│         ¿Por qué los bugs concurrentes son difíciles?        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Bug secuencial:                                             │
│  ┌──────┐ → ┌──────┐ → ┌──────┐ → ┌──────┐                │
│  │  A   │   │  B   │   │  C   │   │ BUG  │                │
│  └──────┘   └──────┘   └──────┘   └──────┘                │
│  Reproducible: siempre A→B→C→BUG                            │
│                                                              │
│  Bug concurrente:                                            │
│  Thread 1: ┌──A──┐     ┌──C──┐                              │
│  Thread 2:    ┌──B──┐     ┌──D──┐                           │
│                                                              │
│  Ejecución 1: A B C D  → OK                                 │
│  Ejecución 2: A B D C  → OK                                 │
│  Ejecución 3: B A C D  → OK                                 │
│  Ejecución 4: A C B D  → BUG  ← 1 de cada ~1000            │
│                                                              │
│  El bug solo aparece con un interleaving específico          │
└──────────────────────────────────────────────────────────────┘
```

### Tipos de bugs concurrentes

```
┌────────────────────┬────────────────────────────────────────┐
│ Tipo               │ Descripción                            │
├────────────────────┼────────────────────────────────────────┤
│ Data race          │ Dos threads acceden al mismo dato,     │
│                    │ al menos uno escribe, sin sync.        │
│                    │ → UB en Rust (unsafe necesario)        │
├────────────────────┼────────────────────────────────────────┤
│ Race condition     │ Resultado depende del orden de         │
│                    │ ejecución. Bug lógico, no UB.          │
│                    │ → Posible incluso en safe Rust         │
├────────────────────┼────────────────────────────────────────┤
│ Deadlock           │ Dos+ threads esperan mutuamente        │
│                    │ por locks que el otro tiene.            │
│                    │ → El programa se congela               │
├────────────────────┼────────────────────────────────────────┤
│ Starvation         │ Un thread nunca obtiene acceso al      │
│                    │ recurso que necesita.                   │
│                    │ → Degradación de rendimiento           │
├────────────────────┼────────────────────────────────────────┤
│ Use-after-free     │ Un thread libera memoria que otro      │
│ concurrente        │ thread aún está usando.                │
│                    │ → Crash o corrupción                   │
└────────────────────┴────────────────────────────────────────┘
```

Rust previene data races en código safe gracias a `Send`/`Sync` y el borrow
checker. Pero **race conditions y deadlocks** son posibles incluso sin `unsafe`.
Y en código `unsafe`, todos los tipos de bug son posibles.

---

## ThreadSanitizer (TSan)

ThreadSanitizer es un detector de data races en tiempo de ejecución, integrado
en el compilador de Rust a través de LLVM. A diferencia de Miri, TSan ejecuta
tu programa de forma **nativa** (no interpretada), con una sobrecarga moderada.

### Instalación y uso

TSan requiere el componente `rust-src` del toolchain nightly:

```bash
# Instalar nightly con rust-src
rustup toolchain install nightly --component rust-src

# Compilar y ejecutar con TSan
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu
```

```
┌──────────────────────────────────────────────────────────────┐
│  Desglose del comando                                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  RUSTFLAGS="-Zsanitizer=thread"                              │
│  └── Habilita ThreadSanitizer en el compilador               │
│                                                              │
│  cargo +nightly test                                         │
│  └── Usa nightly (TSan es unstable)                          │
│                                                              │
│  -Zbuild-std                                                 │
│  └── Recompila la stdlib con TSan instrumentado              │
│      (necesario para detectar races en std)                  │
│                                                              │
│  --target x86_64-unknown-linux-gnu                           │
│  └── Target explícito (requerido por -Zbuild-std)            │
│      En macOS: x86_64-apple-darwin o aarch64-apple-darwin    │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo básico: detectar un data race

```rust
// src/lib.rs
use std::thread;

static mut COUNTER: i32 = 0;

fn increment_unsafe() {
    for _ in 0..1000 {
        unsafe {
            COUNTER += 1;  // Data race: lectura + escritura sin sync
        }
    }
}

#[test]
fn test_data_race() {
    let handles: Vec<_> = (0..4)
        .map(|_| thread::spawn(|| increment_unsafe()))
        .collect();

    for h in handles {
        h.join().unwrap();
    }

    unsafe {
        // El valor "debería" ser 4000, pero con data races
        // puede ser cualquier cosa
        println!("Counter: {}", COUNTER);
    }
}
```

```bash
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test test_data_race \
    -Zbuild-std --target x86_64-unknown-linux-gnu
```

### Otros sanitizers disponibles

```
┌────────────────┬────────────────────────────────────────────┐
│ Sanitizer      │ Detecta                                    │
├────────────────┼────────────────────────────────────────────┤
│ thread (TSan)  │ Data races                                 │
│ address (ASan) │ Buffer overflow, use-after-free, leaks     │
│ memory (MSan)  │ Lectura de memoria no inicializada         │
│ leak           │ Memory leaks (más ligero que ASan)         │
├────────────────┼────────────────────────────────────────────┤
│ Uso general:                                                │
│ RUSTFLAGS="-Zsanitizer=NOMBRE"                              │
│     cargo +nightly test -Zbuild-std --target TARGET         │
└────────────────┴────────────────────────────────────────────┘
```

**Importante:** no puedes combinar sanitizers en la misma ejecución. TSan y ASan
son mutuamente excluyentes.

---

## Interpretar errores de TSan

### Anatomía de un reporte TSan

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)           ← ①
  Write of size 4 at 0x5629a8b01000 by thread T2:         ← ②
    #0 test_data_race::increment_unsafe                    ← ③
           src/lib.rs:8:9
    #1 test_data_race::test_data_race::{{closure}}
           src/lib.rs:15:24

  Previous read of size 4 at 0x5629a8b01000 by thread T3: ← ④
    #0 test_data_race::increment_unsafe
           src/lib.rs:8:9
    #1 test_data_race::test_data_race::{{closure}}
           src/lib.rs:15:24

  Location is global 'test_data_race::COUNTER'             ← ⑤
           (test_data_race+0x000000a01000)

  Thread T2 (tid=12348, running) created at:               ← ⑥
    #0 pthread_create
    #1 std::thread::Builder::spawn_unchecked_
    #2 test_data_race::test_data_race
           src/lib.rs:14:14

  Thread T3 (tid=12349, running) created at:
    #0 pthread_create
    #1 std::thread::Builder::spawn_unchecked_
    #2 test_data_race::test_data_race
           src/lib.rs:14:14
==================
```

```
┌────────────────────────────────────────────────────────────┐
│  Elemento          │ Significado                           │
├────────────────────┼───────────────────────────────────────┤
│ ① "data race"      │ Tipo de error detectado               │
│ ② Write of size 4  │ Un thread hizo una escritura de 4     │
│   by thread T2     │ bytes (i32) en el thread T2           │
│ ③ Backtrace        │ Pila de llamadas del acceso           │
│   src/lib.rs:8:9   │ conflictivo — línea exacta            │
│ ④ Previous read    │ El OTRO acceso en conflicto:          │
│   by thread T3     │ lectura simultánea en T3              │
│ ⑤ Location         │ Qué variable está en conflicto:       │
│                    │ COUNTER (variable global)             │
│ ⑥ Thread created   │ Dónde se creó cada thread             │
│                    │ involucrado                           │
└────────────────────────────────────────────────────────────┘
```

### Leer múltiples reportes

TSan puede reportar decenas de races en una ejecución. La estrategia es:

```bash
# Redirigir output para análisis
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu \
    2>&1 | tee tsan_output.txt

# Contar reportes únicos
grep -c "WARNING: ThreadSanitizer" tsan_output.txt

# Filtrar por archivo fuente
grep "src/lib.rs" tsan_output.txt | sort -u
```

### Suprimir falsos positivos

En casos raros, TSan puede reportar falsos positivos (por ejemplo, con
primitivas de sincronización custom). Puedes suprimir reportes específicos:

```bash
# Crear archivo de supresiones
cat > tsan_suppressions.txt << 'EOF'
race:third_party_lib::internal_function
race:legacy_module::known_benign_race
EOF

# Usar las supresiones
TSAN_OPTIONS="suppressions=tsan_suppressions.txt" \
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu
```

**Precaución:** solo suprime lo que realmente hayas verificado como falso positivo.
Suprimir data races reales oculta bugs.

---

## Data races vs race conditions

Esta distinción es fundamental para elegir la herramienta correcta.

### Data race (UB — detectable por TSan y Miri)

Un data race es una violación de las reglas de memoria: dos threads acceden a la
misma ubicación de memoria simultáneamente y al menos uno escribe, sin
sincronización.

```rust
use std::thread;
use std::sync::Arc;

// Data race: requiere unsafe y es UB
fn data_race_example() {
    let data = Arc::new(std::cell::UnsafeCell::new(0i32));
    let d1 = data.clone();
    let d2 = data.clone();

    let t1 = thread::spawn(move || {
        unsafe { *d1.get() += 1; }  // Escritura sin sync
    });
    let t2 = thread::spawn(move || {
        unsafe { *d2.get() += 1; }  // Escritura sin sync
    });

    t1.join().unwrap();
    t2.join().unwrap();
    // Resultado: indefinido (podría ser 1 o 2 o basura)
}
```

TSan detecta este caso directamente. Es **Undefined Behavior**.

### Race condition (bug lógico — NO detectable por TSan)

Una race condition es un bug lógico donde el resultado depende del orden de
ejecución, pero no hay acceso no sincronizado a memoria.

```rust
use std::sync::{Arc, Mutex};
use std::thread;

// Race condition: todo es safe, pero el resultado es no-determinístico
fn race_condition_example() {
    let balance = Arc::new(Mutex::new(100));

    let b1 = balance.clone();
    let b2 = balance.clone();

    // Thread 1: retirar si hay suficiente
    let t1 = thread::spawn(move || {
        let current = *b1.lock().unwrap();    // Lee: 100
        // ← Otro thread puede modificar balance aquí
        if current >= 80 {
            *b1.lock().unwrap() -= 80;        // Retira 80
        }
    });

    // Thread 2: retirar si hay suficiente
    let t2 = thread::spawn(move || {
        let current = *b2.lock().unwrap();    // Lee: 100
        if current >= 80 {
            *b2.lock().unwrap() -= 80;        // Retira 80
        }
    });

    t1.join().unwrap();
    t2.join().unwrap();
    // Ambos leyeron 100, ambos retiran 80 → balance = -60
    // ¡Pero no hay data race! Cada acceso usa Mutex correctamente.
    // El bug es que check y modify no son atómicos.
}

// Fix: check-and-modify dentro del mismo lock
fn fixed_race_condition() {
    let balance = Arc::new(Mutex::new(100));
    let b1 = balance.clone();
    let b2 = balance.clone();

    let t1 = thread::spawn(move || {
        let mut bal = b1.lock().unwrap();
        if *bal >= 80 {
            *bal -= 80;  // Check y modify bajo el mismo lock
        }
    });

    let t2 = thread::spawn(move || {
        let mut bal = b2.lock().unwrap();
        if *bal >= 80 {
            *bal -= 80;
        }
    });

    t1.join().unwrap();
    t2.join().unwrap();
    // Ahora solo uno de los dos puede retirar
}
```

```
┌──────────────────────────────────────────────────────────────┐
│              Data Race vs Race Condition                      │
├──────────────┬──────────────────┬────────────────────────────┤
│              │ Data Race        │ Race Condition             │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿Qué es?     │ Acceso no        │ Bug lógico por orden      │
│              │ sincronizado a   │ de ejecución               │
│              │ memoria          │                            │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿Es UB?      │ Sí               │ No                         │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿Posible en  │ No (necesita     │ Sí                         │
│ safe Rust?   │ unsafe)          │                            │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿TSan lo     │ Sí               │ No                         │
│ detecta?     │                  │                            │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿Miri lo     │ Sí               │ No                         │
│ detecta?     │                  │                            │
├──────────────┼──────────────────┼────────────────────────────┤
│ ¿Cómo        │ TSan, Miri       │ Tests de estrés,           │
│ detectarlo?  │                  │ revisión de código,        │
│              │                  │ model checking (loom)      │
└──────────────┴──────────────────┴────────────────────────────┘
```

---

## Miri para concurrencia

Miri también detecta data races, con un enfoque diferente al de TSan.

### Miri vs TSan para concurrencia

```
┌──────────────────────────────────────────────────────────────┐
│                   Miri vs TSan                                │
├──────────────┬──────────────────┬────────────────────────────┤
│              │ Miri             │ TSan                        │
├──────────────┼──────────────────┼────────────────────────────┤
│ Ejecución    │ Interpretada     │ Nativa (instrumentada)     │
│ Velocidad    │ 100x más lento   │ 5-15x más lento            │
│ FFI          │ No               │ Sí                         │
│ Syscalls     │ No               │ Sí                         │
│ Aliasing     │ Sí (Stacked/     │ No                         │
│              │ Tree Borrows)    │                            │
│ Seeds        │ Sí (-Zmiri-seed) │ No                         │
│ Plataforma   │ Cualquiera       │ Linux, macOS               │
│ Setup        │ rustup component │ -Zbuild-std + nightly      │
└──────────────┴──────────────────┴────────────────────────────┘
```

**Regla práctica:**
- Tests unitarios pequeños con `unsafe` → **Miri**
- Tests de integración con I/O, FFI, o muchos threads → **TSan**

### Usar Miri con seeds para explorar interleavings

```rust
use std::sync::atomic::{AtomicI32, Ordering};
use std::thread;

static SHARED: AtomicI32 = AtomicI32::new(0);
static mut UNPROTECTED: i32 = 0;

#[test]
fn test_mixed_atomic_and_raw() {
    let t1 = thread::spawn(|| {
        SHARED.store(1, Ordering::Release);
        unsafe { UNPROTECTED = 1; }  // Data race en UNPROTECTED
    });

    let t2 = thread::spawn(|| {
        if SHARED.load(Ordering::Acquire) == 1 {
            unsafe { let _ = UNPROTECTED; }  // Lectura en conflicto
        }
    });

    t1.join().unwrap();
    t2.join().unwrap();
}
```

```bash
# Probar múltiples interleavings con Miri
for seed in $(seq 0 50); do
    MIRIFLAGS="-Zmiri-seed=$seed" cargo +nightly miri test \
        test_mixed_atomic 2>&1 | grep -q "Undefined Behavior" \
        && echo "Race detectada con seed $seed" && break
done
```

El data race en `UNPROTECTED` solo se manifiesta cuando los threads se
intercalan de cierta manera. El seed controla ese orden.

---

## Debugging de deadlocks

Los deadlocks no son UB, así que ni TSan ni Miri los detectan directamente.
Pero hay técnicas específicas para diagnosticarlos.

### Detectar un deadlock en ejecución

Cuando un programa se congela, el primer paso es inspeccionar qué están
haciendo los threads:

```bash
# 1. Encontrar el PID del proceso congelado
ps aux | grep my_program

# 2. Obtener backtrace de TODOS los threads
# Opción A: GDB
gdb -p PID -batch -ex "thread apply all bt"

# Opción B: enviar señal SIGABRT para core dump
kill -ABRT PID
# Luego analizar con GDB:
gdb ./my_program core

# Opción C: en Rust, usar RUST_BACKTRACE antes de ejecutar
RUST_BACKTRACE=1 ./my_program
# Ctrl+\ (SIGQUIT) para obtener backtrace
```

### Patrón clásico: lock ordering

```rust
use std::sync::{Arc, Mutex};
use std::thread;

fn deadlock_example() {
    let lock_a = Arc::new(Mutex::new(0));
    let lock_b = Arc::new(Mutex::new(0));

    let a1 = lock_a.clone();
    let b1 = lock_b.clone();
    let a2 = lock_a.clone();
    let b2 = lock_b.clone();

    // Thread 1: toma A, luego B
    let t1 = thread::spawn(move || {
        let _a = a1.lock().unwrap();    // Toma A
        thread::sleep(std::time::Duration::from_millis(10));
        let _b = b1.lock().unwrap();    // Espera B ← BLOQUEADO
    });

    // Thread 2: toma B, luego A (orden inverso)
    let t2 = thread::spawn(move || {
        let _b = b2.lock().unwrap();    // Toma B
        thread::sleep(std::time::Duration::from_millis(10));
        let _a = a2.lock().unwrap();    // Espera A ← BLOQUEADO
    });

    t1.join().unwrap();  // Nunca retorna
    t2.join().unwrap();
}
```

```
┌──────────────────────────────────────────────────────┐
│              Diagrama de Deadlock                      │
│                                                       │
│  Thread 1          Thread 2                           │
│  ────────          ────────                           │
│  lock(A) ✓         lock(B) ✓                          │
│  │                  │                                 │
│  │ tiene A          │ tiene B                         │
│  │                  │                                 │
│  lock(B) ←─ espera ─┤                                │
│  │          B       │                                 │
│  ├─ espera ────────→ lock(A)                          │
│  │  A               │                                 │
│  ▼                  ▼                                 │
│  DEADLOCK: ciclo de espera mutua                      │
└──────────────────────────────────────────────────────┘
```

### Fix: orden consistente de locks

```rust
// ✅ Siempre tomar locks en el mismo orden (A antes que B)
fn no_deadlock() {
    let lock_a = Arc::new(Mutex::new(0));
    let lock_b = Arc::new(Mutex::new(0));

    let a1 = lock_a.clone();
    let b1 = lock_b.clone();
    let a2 = lock_a.clone();
    let b2 = lock_b.clone();

    let t1 = thread::spawn(move || {
        let _a = a1.lock().unwrap();
        let _b = b1.lock().unwrap();
    });

    let t2 = thread::spawn(move || {
        let _a = a2.lock().unwrap();  // Mismo orden: A primero
        let _b = b2.lock().unwrap();  // B después
    });

    t1.join().unwrap();
    t2.join().unwrap();
}
```

### Usar `try_lock` para detectar deadlocks potenciales

```rust
use std::sync::Mutex;
use std::time::Duration;

fn try_lock_pattern(lock_a: &Mutex<i32>, lock_b: &Mutex<i32>) {
    let _a = lock_a.lock().unwrap();

    // En vez de bloquearse indefinidamente:
    match lock_b.try_lock() {
        Ok(_b) => {
            // Tenemos ambos locks, proceder
        }
        Err(_) => {
            // No pudimos obtener B → posible deadlock
            eprintln!("WARNING: could not acquire lock_b, \
                       potential deadlock avoided");
            // Liberar A (sale del scope) y reintentar
        }
    }
}
```

### Parking-lot con detección de deadlocks

El crate `parking_lot` ofrece detección de deadlocks integrada:

```toml
# Cargo.toml
[dependencies]
parking_lot = { version = "0.12", features = ["deadlock_detection"] }
```

```rust
use parking_lot::Mutex;
use std::thread;
use std::time::Duration;

fn setup_deadlock_detector() {
    // Spawn un thread que verifica deadlocks periódicamente
    thread::spawn(|| {
        loop {
            thread::sleep(Duration::from_secs(5));
            let deadlocks = parking_lot::deadlock::check_deadlock();
            if deadlocks.is_empty() {
                continue;
            }

            eprintln!("{} deadlocks detected!", deadlocks.len());
            for (i, threads) in deadlocks.iter().enumerate() {
                eprintln!("Deadlock #{}:", i + 1);
                for t in threads {
                    eprintln!("  Thread Id {:#?}", t.thread_id());
                    eprintln!("  Backtrace:\n{:#?}", t.backtrace());
                }
            }
        }
    });
}
```

---

## Herramientas de logging para concurrencia

Cuando TSan y Miri no son suficientes (race conditions lógicas), el logging
estructurado es tu mejor herramienta.

### Logging con identificación de thread

```rust
use std::thread;
use std::sync::{Arc, Mutex};

fn debug_with_thread_ids() {
    env_logger::init();

    let data = Arc::new(Mutex::new(Vec::new()));

    let handles: Vec<_> = (0..4).map(|i| {
        let data = data.clone();
        thread::Builder::new()
            .name(format!("worker-{}", i))
            .spawn(move || {
                log::debug!(
                    "[{:?}] worker-{} acquiring lock",
                    thread::current().id(),
                    i
                );

                let mut vec = data.lock().unwrap();

                log::debug!(
                    "[{:?}] worker-{} got lock, vec.len() = {}",
                    thread::current().id(),
                    i,
                    vec.len()
                );

                vec.push(i);

                log::debug!(
                    "[{:?}] worker-{} releasing lock",
                    thread::current().id(),
                    i
                );
            })
            .unwrap()
    }).collect();

    for h in handles {
        h.join().unwrap();
    }
}
```

```bash
RUST_LOG=debug cargo test debug_with_thread_ids -- --nocapture
```

### Tracing con spans por thread

El crate `tracing` es superior a `log` para concurrencia:

```rust
use tracing::{info, instrument, span, Level};
use std::sync::{Arc, Mutex};
use std::thread;

#[instrument]
fn process_item(id: usize, data: &Mutex<Vec<usize>>) {
    info!("waiting for lock");
    let mut vec = data.lock().unwrap();
    info!(vec_len = vec.len(), "acquired lock");
    vec.push(id);
    info!("releasing lock");
}

fn traced_concurrent_work() {
    tracing_subscriber::fmt()
        .with_thread_ids(true)
        .with_thread_names(true)
        .init();

    let data = Arc::new(Mutex::new(Vec::new()));

    let handles: Vec<_> = (0..4).map(|i| {
        let data = data.clone();
        thread::Builder::new()
            .name(format!("worker-{}", i))
            .spawn(move || {
                let span = span!(Level::INFO, "worker", id = i);
                let _guard = span.enter();
                process_item(i, &data);
            })
            .unwrap()
    }).collect();

    for h in handles {
        h.join().unwrap();
    }
}
```

Output con timestamps y thread IDs permite reconstruir el interleaving exacto
que causó el bug.

---

## Patrones de debugging concurrente

### Patrón 1: El test de estrés

Cuando sospechas de una race condition pero no puedes reproducirla:

```rust
#[test]
fn stress_test_concurrent_counter() {
    // Ejecutar muchas veces para aumentar probabilidad
    // de capturar el interleaving problemático
    for iteration in 0..1000 {
        let counter = Arc::new(Mutex::new(0));
        let mut handles = vec![];

        for _ in 0..8 {
            let c = counter.clone();
            handles.push(thread::spawn(move || {
                for _ in 0..100 {
                    let mut val = c.lock().unwrap();
                    *val += 1;
                }
            }));
        }

        for h in handles {
            h.join().unwrap();
        }

        let final_val = *counter.lock().unwrap();
        assert_eq!(
            final_val, 800,
            "Race condition detected on iteration {}!",
            iteration
        );
    }
}
```

### Patrón 2: Loom para model checking

El crate `loom` explora **todas** las posibles intercalaciones de threads,
no solo las que ocurren aleatoriamente:

```toml
# Cargo.toml
[dev-dependencies]
loom = "0.7"
```

```rust
#[cfg(test)]
mod tests {
    use loom::sync::atomic::{AtomicUsize, Ordering};
    use loom::sync::Arc;
    use loom::thread;

    #[test]
    fn test_with_loom() {
        loom::model(|| {
            let counter = Arc::new(AtomicUsize::new(0));
            let c1 = counter.clone();
            let c2 = counter.clone();

            let t1 = thread::spawn(move || {
                c1.fetch_add(1, Ordering::SeqCst);
            });

            let t2 = thread::spawn(move || {
                c2.fetch_add(1, Ordering::SeqCst);
            });

            t1.join().unwrap();
            t2.join().unwrap();

            assert_eq!(counter.load(Ordering::SeqCst), 2);
        });
    }
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  Loom explora todas las intercalaciones posibles             │
│                                                              │
│  Con 2 threads y 2 operaciones cada uno:                     │
│                                                              │
│  Interleaving 1: T1.op1 → T1.op2 → T2.op1 → T2.op2        │
│  Interleaving 2: T1.op1 → T2.op1 → T1.op2 → T2.op2        │
│  Interleaving 3: T1.op1 → T2.op1 → T2.op2 → T1.op2        │
│  Interleaving 4: T2.op1 → T1.op1 → T1.op2 → T2.op2        │
│  Interleaving 5: T2.op1 → T1.op1 → T2.op2 → T1.op2        │
│  Interleaving 6: T2.op1 → T2.op2 → T1.op1 → T1.op2        │
│                                                              │
│  Loom prueba TODAS → si alguna falla, tienes un bug         │
│                                                              │
│  ⚠️ Explosión combinatoria: más threads/operaciones =       │
│  exponencialmente más intercalaciones. Mantener los tests    │
│  con loom PEQUEÑOS.                                          │
└──────────────────────────────────────────────────────────────┘
```

### Patrón 3: Barrier para sincronizar inicio

Para maximizar la probabilidad de contención:

```rust
use std::sync::{Arc, Barrier, Mutex};
use std::thread;

#[test]
fn test_with_barrier() {
    let num_threads = 8;
    let barrier = Arc::new(Barrier::new(num_threads));
    let shared = Arc::new(Mutex::new(Vec::new()));

    let handles: Vec<_> = (0..num_threads).map(|i| {
        let barrier = barrier.clone();
        let shared = shared.clone();

        thread::spawn(move || {
            // Todos los threads esperan aquí hasta que
            // TODOS estén listos → máxima contención
            barrier.wait();

            // Ahora todos compiten por el lock simultáneamente
            let mut data = shared.lock().unwrap();
            data.push(i);
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    let data = shared.lock().unwrap();
    assert_eq!(data.len(), num_threads);
}
```

### Patrón 4: Inyección de delays para provocar interleavings

```rust
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

// Solo en configuración de test
#[cfg(test)]
fn debug_delay(label: &str) {
    eprintln!(
        "[{:?}] {} - sleeping",
        thread::current().id(),
        label
    );
    thread::sleep(Duration::from_millis(10));
}

#[cfg(not(test))]
fn debug_delay(_label: &str) {}

fn transfer(
    from: &Mutex<i32>,
    to: &Mutex<i32>,
    amount: i32,
) {
    let mut from_bal = from.lock().unwrap();
    debug_delay("between locks");  // Amplifica la ventana de contención
    let mut to_bal = to.lock().unwrap();

    if *from_bal >= amount {
        *from_bal -= amount;
        *to_bal += amount;
    }
}
```

---

## Errores comunes

### 1. Usar `static mut` en vez de atomics para "rendimiento"

```rust
// ❌ Data race: TSan lo detectará
static mut REQUEST_COUNT: u64 = 0;

fn handle_request() {
    unsafe { REQUEST_COUNT += 1; }
}

// ✅ Atómico: sin data race, rendimiento similar
use std::sync::atomic::{AtomicU64, Ordering};

static REQUEST_COUNT: AtomicU64 = AtomicU64::new(0);

fn handle_request_safe() {
    REQUEST_COUNT.fetch_add(1, Ordering::Relaxed);
}
```

### 2. Creer que Mutex previene race conditions automáticamente

```rust
// ❌ Race condition: check y act no son atómicos
fn check_and_insert(map: &Mutex<HashMap<String, i32>>, key: &str) {
    let contains = map.lock().unwrap().contains_key(key);
    // ← Lock liberado aquí, otro thread puede insertar
    if !contains {
        map.lock().unwrap().insert(key.to_string(), 0);
        // Posible inserción duplicada
    }
}

// ✅ Check y act dentro del mismo lock
fn check_and_insert_fixed(map: &Mutex<HashMap<String, i32>>, key: &str) {
    let mut guard = map.lock().unwrap();
    guard.entry(key.to_string()).or_insert(0);
}
```

### 3. No probar con suficientes threads

```rust
// ❌ Test con 2 threads: poca probabilidad de encontrar races
#[test]
fn weak_test() {
    let data = Arc::new(Mutex::new(0));
    let d1 = data.clone();
    let t = thread::spawn(move || { *d1.lock().unwrap() += 1; });
    *data.lock().unwrap() += 1;
    t.join().unwrap();
}

// ✅ Más threads + más iteraciones = más interleavings
#[test]
fn strong_test() {
    for _ in 0..100 {
        let data = Arc::new(Mutex::new(0));
        let handles: Vec<_> = (0..16).map(|_| {
            let d = data.clone();
            thread::spawn(move || {
                for _ in 0..100 {
                    *d.lock().unwrap() += 1;
                }
            })
        }).collect();
        for h in handles { h.join().unwrap(); }
        assert_eq!(*data.lock().unwrap(), 1600);
    }
}
```

### 4. Confundir "el test pasó" con "no hay data race"

```bash
# ❌ "cargo test pasó → no hay data races"
cargo test  # Un data race puede no manifestarse

# ✅ Usar la herramienta correcta
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test -Zbuild-std --target x86_64-unknown-linux-gnu
# O
cargo +nightly miri test
```

### 5. Ignorar el orden de drop para locks

```rust
// ❌ El orden de drop es inverso al de declaración
fn subtle_deadlock() {
    let a = lock_a.lock().unwrap();  // Se dropea SEGUNDO
    let b = lock_b.lock().unwrap();  // Se dropea PRIMERO
    // Si otro thread toma b→a, deadlock posible
}

// ✅ Drop explícito cuando el orden importa
fn explicit_drop() {
    let a = lock_a.lock().unwrap();
    let b = lock_b.lock().unwrap();
    // ... usar ambos ...
    drop(b);  // Liberar B primero explícitamente
    drop(a);  // Luego A
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          DEBUGGING CONCURRENTE CHEATSHEET                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  THREADSANITIZER                                             │
│  RUSTFLAGS="-Zsanitizer=thread" \                            │
│    cargo +nightly test -Zbuild-std \                         │
│    --target x86_64-unknown-linux-gnu   Detectar data races   │
│                                                              │
│  OTROS SANITIZERS                                            │
│  -Zsanitizer=address                  Buffer overflows       │
│  -Zsanitizer=memory                   Uninit memory          │
│  -Zsanitizer=leak                     Memory leaks           │
│                                                              │
│  MIRI PARA CONCURRENCIA                                      │
│  cargo +nightly miri test             Detectar data races    │
│  MIRIFLAGS="-Zmiri-seed=N"           Probar interleavings   │
│                                                              │
│  DEBUGGING DE DEADLOCKS                                      │
│  gdb -p PID -batch \                                         │
│    -ex "thread apply all bt"          Backtrace de threads   │
│  parking_lot con deadlock_detection   Detección automática   │
│  try_lock() en vez de lock()          Evitar bloqueo         │
│                                                              │
│  HERRAMIENTAS COMPLEMENTARIAS                                │
│  loom                                 Model checking         │
│  tracing con thread_ids               Logging estructurado   │
│  Barrier para sincronizar             Maximizar contención   │
│                                                              │
│  TSAN SUPPRESSIONS                                           │
│  TSAN_OPTIONS="suppressions=file.txt"  Suprimir FP           │
│                                                              │
│  DIAGNÓSTICO DE PROCESO CONGELADO                            │
│  ps aux | grep NOMBRE                 Encontrar PID          │
│  gdb -p PID                           Attach al proceso      │
│  thread apply all bt                  Ver todos los threads  │
│  kill -ABRT PID                       Generar core dump      │
│                                                              │
│  REGLAS DE ORO                                               │
│  1. Data race ≠ race condition                               │
│  2. TSan detecta data races, NO race conditions              │
│  3. Mutex previene data races, NO race conditions            │
│  4. "cargo test pasó" ≠ "no hay races"                       │
│  5. Siempre tomar locks en orden consistente                 │
│  6. Mantener el scope del lock mínimo                        │
│  7. Check-and-act debe ser atómico                           │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Detectar un data race con TSan

Dado este código:

```rust
use std::thread;

struct SharedBuffer {
    data: Vec<u8>,
}

// Implementación manual de Send (¡insegura!)
unsafe impl Send for *mut SharedBuffer {}
unsafe impl Sync for *mut SharedBuffer {}

fn write_to_buffer(ptr: *mut SharedBuffer, offset: usize, value: u8) {
    unsafe {
        (*ptr).data[offset] = value;
    }
}

fn read_from_buffer(ptr: *mut SharedBuffer) -> Vec<u8> {
    unsafe { (*ptr).data.clone() }
}

#[test]
fn test_shared_buffer() {
    let mut buf = SharedBuffer {
        data: vec![0u8; 100],
    };
    let ptr = &mut buf as *mut SharedBuffer;

    let writer = thread::spawn(move || {
        for i in 0..100 {
            write_to_buffer(ptr, i, 42);
        }
    });

    // Lectura simultánea sin sincronización
    let result = read_from_buffer(ptr);

    writer.join().unwrap();
    println!("Read {} bytes", result.len());
}
```

**Tareas:**
1. Predice cuántos data races reportará TSan
2. Ejecuta con TSan y analiza el output
3. Corrige el código usando `Arc<Mutex<SharedBuffer>>` en vez de raw pointers
4. Verifica que TSan ya no reporta errores

**Pregunta de reflexión:** ¿Por qué `unsafe impl Send/Sync for *mut T` es una
señal de alarma inmediata durante code review? ¿Qué garantías estás prometiendo
al compilador que probablemente no puedes cumplir?

---

### Ejercicio 2: Diagnosticar un deadlock

```rust
use std::sync::{Arc, Mutex};
use std::thread;
use std::collections::HashMap;

struct UserDb {
    users: Mutex<HashMap<u64, String>>,
    sessions: Mutex<HashMap<u64, u64>>,  // session_id → user_id
}

impl UserDb {
    fn new() -> Self {
        UserDb {
            users: Mutex::new(HashMap::new()),
            sessions: Mutex::new(HashMap::new()),
        }
    }

    fn create_user(&self, id: u64, name: String) -> u64 {
        let mut users = self.users.lock().unwrap();
        users.insert(id, name);
        let session_id = id * 1000;
        let mut sessions = self.sessions.lock().unwrap();
        sessions.insert(session_id, id);
        session_id
    }

    fn get_user_by_session(&self, session_id: u64) -> Option<String> {
        let sessions = self.sessions.lock().unwrap();  // Lock sessions primero
        let user_id = sessions.get(&session_id)?;
        let users = self.users.lock().unwrap();        // Luego users
        users.get(user_id).cloned()
    }
}

#[test]
fn test_concurrent_user_db() {
    let db = Arc::new(UserDb::new());
    let db1 = db.clone();
    let db2 = db.clone();

    let t1 = thread::spawn(move || {
        for i in 0..100 {
            db1.create_user(i, format!("User {}", i));
        }
    });

    let t2 = thread::spawn(move || {
        for i in 0..100 {
            db2.get_user_by_session(i * 1000);
        }
    });

    t1.join().unwrap();
    t2.join().unwrap();
}
```

**Tareas:**
1. Dibuja el diagrama de lock ordering para `create_user` y `get_user_by_session`
2. Explica por qué hay riesgo de deadlock
3. Corrige el código para que ambos métodos tomen los locks en el mismo orden
4. Opción avanzada: reescribe usando un solo `Mutex<(HashMap, HashMap)>` y evalúa
   el trade-off

**Pregunta de reflexión:** ¿Cuál es el trade-off entre un lock granular (un Mutex
por campo) y un lock grueso (un Mutex para toda la estructura)? ¿En qué escenarios
preferirías cada uno?

---

### Ejercicio 3: Loom para verificar un lock-free counter

Implementa un counter lock-free simple y verifícalo con loom:

```rust
use std::sync::atomic::{AtomicU64, Ordering};

struct LockFreeCounter {
    count: AtomicU64,
    max: u64,
}

impl LockFreeCounter {
    fn new(max: u64) -> Self {
        LockFreeCounter {
            count: AtomicU64::new(0),
            max,
        }
    }

    /// Incrementa el counter si no ha alcanzado el máximo.
    /// Retorna Ok(valor_anterior) o Err(()) si está lleno.
    fn try_increment(&self) -> Result<u64, ()> {
        loop {
            let current = self.count.load(Ordering::Relaxed);
            if current >= self.max {
                return Err(());
            }
            // ¿Es este compare_exchange correcto?
            // ¿Los orderings son suficientes?
            match self.count.compare_exchange(
                current,
                current + 1,
                Ordering::Relaxed,  // ← ¿Suficiente?
                Ordering::Relaxed,
            ) {
                Ok(prev) => return Ok(prev),
                Err(_) => continue,  // Reintentar
            }
        }
    }

    fn get(&self) -> u64 {
        self.count.load(Ordering::Relaxed)
    }
}
```

**Tareas:**
1. Escribe un test con `loom` que lance 3 threads incrementando un counter con max=5
2. Verifica que el counter nunca excede el máximo en ningún interleaving
3. ¿Los orderings `Relaxed` son suficientes para este caso? Justifica
4. Cambia a `Ordering::SeqCst` y ejecuta loom de nuevo — ¿cambia el resultado?

**Pregunta de reflexión:** En este caso específico (counter con máximo), ¿por qué
`Relaxed` es suficiente para el `compare_exchange` aunque `Relaxed` no ofrece
garantías de ordenamiento entre threads? ¿En qué situación necesitarías
`Acquire`/`Release`?