# T02 — Arc\<T\>

## De Rc a Arc

`Rc<T>` no puede cruzar fronteras de threads porque su contador
no es atomico. `Arc<T>` (Atomic Reference Counted) resuelve esto
usando operaciones atomicas para incrementar y decrementar el
contador, haciendolo seguro para multi-thread:

```rust
use std::sync::Arc;
use std::thread;

let data = Arc::new(vec![1, 2, 3]);

let handle = {
    let data = Arc::clone(&data);  // incrementa contador atomicamente
    thread::spawn(move || {
        println!("thread: {data:?}");
    })
};

println!("main: {data:?}");
handle.join().unwrap();
```

```rust
// Comparacion directa — mismo codigo, diferente smart pointer:

// Single-thread (Rc):
use std::rc::Rc;
let a = Rc::new(42);
let b = Rc::clone(&a);
// No puede enviarse a otro thread

// Multi-thread (Arc):
use std::sync::Arc;
let a = Arc::new(42);
let b = Arc::clone(&a);
// Puede enviarse a otro thread
```

```text
¿Que es una operacion atomica?

  Una operacion que se completa en un solo paso indivisible
  desde la perspectiva de otros threads. No puede ser
  interrumpida a la mitad.

  Rc:  counter += 1   →  load, add, store (3 pasos, no atomico)
  Arc: counter += 1   →  fetch_add atomico (1 paso indivisible)

  Las atomicas usan instrucciones especiales del CPU:
  x86: lock xadd / lock cmpxchg
  ARM: ldxr / stxr (load-link/store-conditional)
```

---

## API de Arc — identica a Rc

La API de `Arc` es practicamente identica a la de `Rc`.
Si sabes usar Rc, ya sabes usar Arc:

```rust
use std::sync::Arc;

// Crear:
let a = Arc::new(String::from("shared data"));

// Clonar (incrementa contador atomicamente):
let b = Arc::clone(&a);

// Consultar:
println!("strong count: {}", Arc::strong_count(&a));  // 2
println!("same pointer? {}", Arc::ptr_eq(&a, &b));    // true

// Acceso via Deref:
println!("len: {}", a.len());       // Deref → String → len()
println!("upper: {}", a.to_uppercase());

// Extraer si eres el unico:
drop(b);
let s = Arc::try_unwrap(a).unwrap();  // OK: count era 1
assert_eq!(s, "shared data");
```

```text
Metodos compartidos entre Rc y Arc:

  ::new(val)              crear
  ::clone(&arc)           incrementar contador
  ::strong_count(&arc)    numero de clones activos
  ::weak_count(&arc)      numero de Weak activos
  ::ptr_eq(&a, &b)        ¿mismo bloque de memoria?
  ::try_unwrap(arc)       extraer valor si count == 1
  ::make_mut(&mut arc)    clone-on-write

  La diferencia es INTERNA: atomico vs no-atomico.
  La API externa es la misma.
```

---

## Arc con threads

### Patron basico — compartir datos de solo lectura

```rust
use std::sync::Arc;
use std::thread;

let config = Arc::new(vec![
    ("host", "localhost"),
    ("port", "8080"),
    ("workers", "4"),
]);

let mut handles = vec![];

for i in 0..3 {
    let config = Arc::clone(&config);
    handles.push(thread::spawn(move || {
        // Cada thread lee la misma config:
        println!("thread {i}: host = {}", config[0].1);
    }));
}

for h in handles {
    h.join().unwrap();
}
// config original sigue viva — los threads ya la soltaron
println!("main: {} entries", config.len());
```

### Arc + Mutex — lectura Y escritura compartida

```rust
// Arc<T> solo da &T (lectura).
// Para mutar datos compartidos entre threads, combina Arc con Mutex:

use std::sync::{Arc, Mutex};
use std::thread;

let counter = Arc::new(Mutex::new(0));

let mut handles = vec![];

for _ in 0..10 {
    let counter = Arc::clone(&counter);
    handles.push(thread::spawn(move || {
        let mut num = counter.lock().unwrap();
        *num += 1;
        // MutexGuard se destruye aqui → libera el lock
    }));
}

for h in handles {
    h.join().unwrap();
}

println!("final count: {}", *counter.lock().unwrap());  // 10
```

```text
Patron comun:

  Arc<Mutex<T>>   → multiples threads leen Y escriben T
  Arc<RwLock<T>>  → multiples lectores O un escritor (mas eficiente si hay muchas lecturas)
  Arc<T>          → multiples threads SOLO leen T (no necesitas lock)

  Arc provee ownership compartido entre threads.
  Mutex/RwLock provee la sincronizacion para mutar.
  Ninguno funciona sin el otro en multi-thread mutable.
```

### Arc + RwLock — muchos lectores, pocos escritores

```rust
use std::sync::{Arc, RwLock};
use std::thread;

let data = Arc::new(RwLock::new(vec![1, 2, 3]));

let mut handles = vec![];

// 5 threads lectores:
for i in 0..5 {
    let data = Arc::clone(&data);
    handles.push(thread::spawn(move || {
        let read = data.read().unwrap();  // multiples lectores simultanos OK
        println!("reader {i}: {read:?}");
    }));
}

// 1 thread escritor:
{
    let data = Arc::clone(&data);
    handles.push(thread::spawn(move || {
        let mut write = data.write().unwrap();  // exclusivo
        write.push(4);
        println!("writer: added 4");
    }));
}

for h in handles {
    h.join().unwrap();
}
```

---

## Overhead de Arc vs Rc

Las operaciones atomicas tienen un costo. Cuanto cuesta Arc
en comparacion con Rc depende de la plataforma y el patron de uso:

```text
Costo de clone (incrementar contador):

  Rc::clone    ~1 ns    (load + add + store, no atomico)
  Arc::clone   ~5-20 ns (fetch_add atomico con barrera de memoria)

  ~5-20x mas lento que Rc por cada clone/drop.

¿Cuando importa?

  - Si clonas/dropeas miles de veces por segundo en un hot loop → si
  - Para la mayoria del codigo → no
  - El acceso a los datos (*arc, arc.method()) tiene el mismo costo
    en Rc y Arc — la diferencia es SOLO en clone y drop
```

```rust
// El acceso a datos NO tiene overhead extra en Arc:

use std::sync::Arc;
use std::rc::Rc;

let arc = Arc::new(vec![1, 2, 3]);
let rc = Rc::new(vec![1, 2, 3]);

// Estas dos operaciones tienen el MISMO costo:
let sum_arc: i32 = arc.iter().sum();   // Deref → &Vec → iter
let sum_rc: i32 = rc.iter().sum();     // Deref → &Vec → iter
// Deref es un puntero raw en ambos casos — sin atomicas.
```

---

## Por que no usar Arc siempre

Si Arc es mas general (funciona single-thread Y multi-thread),
¿por que existe Rc?

```text
Razones para NO usar Arc por defecto:

1. Performance: clone/drop es 5-20x mas lento en Arc.
   En codigo con muchos clones (ej: arboles, grafos, caches),
   la diferencia se acumula.

2. Semantica: Rc documenta "esto es single-thread".
   Arc documenta "esto puede cruzar threads".
   El tipo comunica la INTENCION del diseño.

3. Compilador como guardian: si usas Rc y luego intentas
   enviar a otro thread, el compilador te avisa. Con Arc,
   no tendrias esa proteccion — podrias compartir datos
   sin la sincronizacion necesaria (Mutex/RwLock).

4. Composicion: Rc<RefCell<T>> funciona para mutabilidad
   single-thread. RefCell NO es Send/Sync, asi que
   Arc<RefCell<T>> no compila — te fuerza a usar
   Arc<Mutex<T>> que si es thread-safe.

Regla: usa Rc por defecto. Cambia a Arc solo cuando necesites
enviar datos entre threads.
```

```rust
// El compilador te guia:

use std::rc::Rc;
use std::cell::RefCell;

let data = Rc::new(RefCell::new(42));

// Si intentas enviar a otro thread:
// std::thread::spawn(move || {
//     println!("{}", data.borrow());
// });
// error: `Rc<RefCell<i32>>` cannot be sent between threads safely
//
// El error te dice exactamente que hacer:
// Cambiar Rc → Arc y RefCell → Mutex

use std::sync::{Arc, Mutex};
let data = Arc::new(Mutex::new(42));
std::thread::spawn(move || {
    println!("{}", data.lock().unwrap());
});
```

---

## Send y Sync — por que Arc funciona entre threads

```text
Rust tiene dos traits que controlan que tipos pueden cruzar threads:

  Send: el tipo puede MOVERSE a otro thread (transferir ownership)
  Sync: el tipo puede ser REFERENCIADO desde multiples threads (&T es Send)

  Rc<T>:   NO Send, NO Sync  → no puede cruzar threads de ninguna forma
  Arc<T>:  Send si T: Send+Sync,  Sync si T: Send+Sync

  ¿Por que Arc requiere T: Send+Sync?
  - Send: porque Arc puede ser el ultimo en destruirse en cualquier thread
    (Drop de T podria ejecutarse en un thread diferente al que lo creo)
  - Sync: porque multiples threads pueden acceder a &T simultaneamente
    (via Deref en los distintos Arc clones)
```

```rust
use std::sync::Arc;

// i32 es Send + Sync → Arc<i32> es Send + Sync:
let a: Arc<i32> = Arc::new(42);
std::thread::spawn(move || println!("{a}"));  // OK

// Rc<i32> NO es Send → Arc<Rc<i32>> NO es Send:
// use std::rc::Rc;
// let a = Arc::new(Rc::new(42));
// std::thread::spawn(move || println!("{}", **a));
// error: Rc<i32> cannot be sent between threads safely

// La seguridad es transitiva: si T no es thread-safe,
// Arc<T> tampoco lo es.
```

---

## Arc en la practica

### Thread pool con datos compartidos

```rust
use std::sync::Arc;
use std::thread;

fn parallel_search(data: &[String], query: &str) -> Vec<String> {
    let data = Arc::new(data.to_vec());
    let query = Arc::new(query.to_string());
    let chunk_size = (data.len() + 3) / 4;  // dividir en 4

    let mut handles = vec![];

    for chunk_start in (0..data.len()).step_by(chunk_size) {
        let data = Arc::clone(&data);
        let query = Arc::clone(&query);

        handles.push(thread::spawn(move || {
            let end = (chunk_start + chunk_size).min(data.len());
            data[chunk_start..end]
                .iter()
                .filter(|s| s.contains(query.as_str()))
                .cloned()
                .collect::<Vec<_>>()
        }));
    }

    let mut results = vec![];
    for h in handles {
        results.extend(h.join().unwrap());
    }
    results
}
```

### Configuracion inmutable compartida

```rust
use std::sync::Arc;
use std::thread;

#[derive(Debug)]
struct AppConfig {
    db_url: String,
    max_connections: usize,
    log_level: String,
}

fn start_workers(config: Arc<AppConfig>, num_workers: usize) {
    let mut handles = vec![];

    for id in 0..num_workers {
        let config = Arc::clone(&config);
        handles.push(thread::spawn(move || {
            println!(
                "Worker {id}: connecting to {} (max: {})",
                config.db_url, config.max_connections
            );
            // ... hacer trabajo ...
        }));
    }

    for h in handles {
        h.join().unwrap();
    }
}

fn main() {
    let config = Arc::new(AppConfig {
        db_url: "postgres://localhost/mydb".into(),
        max_connections: 10,
        log_level: "info".into(),
    });

    start_workers(config, 4);
    // 4 threads comparten la misma config sin copiarla
}
```

---

## Errores comunes

```rust
// ERROR 1: Arc sin Mutex para datos mutables
use std::sync::Arc;
use std::thread;

let data = Arc::new(vec![1, 2, 3]);
let data_clone = Arc::clone(&data);

// thread::spawn(move || {
//     data_clone.push(4);  // error: cannot borrow as mutable
// });

// Arc<T> da &T, no &mut T. Para mutar:
use std::sync::Mutex;
let data = Arc::new(Mutex::new(vec![1, 2, 3]));
let data_clone = Arc::clone(&data);
thread::spawn(move || {
    data_clone.lock().unwrap().push(4);  // OK
});
```

```rust
// ERROR 2: clonar Arc dentro del closure en vez de antes
use std::sync::Arc;
use std::thread;

let data = Arc::new(42);

// MAL — mueve data al primer thread, el segundo no puede usarla:
// let h1 = thread::spawn(move || println!("{}", data));
// let h2 = thread::spawn(move || println!("{}", data));  // error: value used after move

// BIEN — clonar ANTES de mover al thread:
let d1 = Arc::clone(&data);
let d2 = Arc::clone(&data);
let h1 = thread::spawn(move || println!("{d1}"));
let h2 = thread::spawn(move || println!("{d2}"));
h1.join().unwrap();
h2.join().unwrap();
```

```rust
// ERROR 3: usar Arc cuando no necesitas ownership compartido
use std::sync::Arc;
use std::thread;

// MAL — Arc para datos que un solo thread usa:
let data = Arc::new(vec![1, 2, 3]);
thread::spawn(move || {
    println!("{data:?}");
}).join().unwrap();
// Solo un thread usa data — Arc es innecesario.

// BIEN — mover directamente:
let data = vec![1, 2, 3];
thread::spawn(move || {
    println!("{data:?}");
}).join().unwrap();
```

```rust
// ERROR 4: deadlock con Arc<Mutex<T>>
use std::sync::{Arc, Mutex};

let a = Arc::new(Mutex::new(1));
let b = Arc::new(Mutex::new(2));

// Thread 1: lock a, luego lock b
// Thread 2: lock b, luego lock a
// → DEADLOCK si ambos adquieren el primer lock simultaneamente

// Solucion: siempre adquirir locks en el mismo orden.
// O usar try_lock() con timeout.
```

```rust
// ERROR 5: Arc<Mutex<T>> cuando Arc<RwLock<T>> seria mejor
use std::sync::{Arc, Mutex, RwLock};

// Si la mayoria de accesos son lectura:
// MAL: Mutex serializa todos los accesos (lectores se bloquean entre si)
let data = Arc::new(Mutex::new(vec![1, 2, 3]));

// BIEN: RwLock permite multiples lectores simultaneos
let data = Arc::new(RwLock::new(vec![1, 2, 3]));
// data.read().unwrap()  → multiples threads a la vez
// data.write().unwrap() → exclusivo, bloquea lectores y escritores
```

---

## Cheatsheet

```text
Crear:
  Arc::new(value)            crear con strong_count = 1
  Arc::clone(&arc)           incrementar contador (atomico, O(1))

API (identica a Rc):
  Arc::strong_count(&arc)    numero de clones activos
  Arc::weak_count(&arc)      numero de Weak activos
  Arc::ptr_eq(&a, &b)        ¿mismo bloque?
  Arc::try_unwrap(arc)       extraer si count == 1
  Arc::make_mut(&mut arc)    clone-on-write

Patrones con threads:
  Arc<T>              solo lectura compartida (sin lock)
  Arc<Mutex<T>>       lectura + escritura compartida
  Arc<RwLock<T>>      muchos lectores, pocos escritores

Rc vs Arc:
  Rc   → single-thread, rapido (~1 ns clone)
  Arc  → multi-thread, mas lento (~5-20 ns clone)
  Ambos → misma API, diferente implementacion interna

Send/Sync:
  Arc<T>: Send  si T: Send + Sync
  Arc<T>: Sync  si T: Send + Sync
  Rc<T>:  nunca Send, nunca Sync

Regla de decision:
  ¿Un solo thread?        → Rc<T>
  ¿Multiples threads?     → Arc<T>
  ¿Necesitas mutar?       → Arc<Mutex<T>> o Arc<RwLock<T>>
  ¿Solo lectura?          → Arc<T> (sin lock)
  ¿Un solo thread + mutar? → Rc<RefCell<T>>
```

---

## Ejercicios

### Ejercicio 1 — Compartir entre threads

```rust
use std::sync::Arc;
use std::thread;

// a) Crea un Vec<String> con 5 palabras.
//    Envia un Arc del Vec a 3 threads, donde cada thread
//    imprime las palabras en mayusculas.
//    ¿Cuantos Arc::clone necesitas?
//
// b) Modifica para que cada thread solo procese una porcion:
//    thread 0: palabras 0-1
//    thread 1: palabras 2-3
//    thread 2: palabra 4
//    Pista: cada thread recibe el mismo Arc pero usa un rango diferente.
//
// c) ¿Que pasa si intentas hacer push a la Vec desde un thread?
//    ¿Que necesitas agregar para que funcione?
```

### Ejercicio 2 — Contador compartido

```rust
use std::sync::{Arc, Mutex};
use std::thread;

// a) Crea un Arc<Mutex<HashMap<&str, i32>>> para contar
//    cuantas veces cada thread incrementa un contador.
//    Lanza 4 threads, cada uno incrementa su entrada
//    ("thread-0", "thread-1", etc.) 100 veces.
//
// b) Al final, imprime el HashMap.
//    ¿La suma total es siempre 400? ¿Por que?
//
// c) Cambia Mutex por RwLock. ¿Funciona igual?
//    ¿Tiene sentido en este caso donde todos escriben?
//
// d) ¿Que pasa si un thread hace panic mientras tiene el lock?
//    Investiga "poisoned mutex".
```

### Ejercicio 3 — Rc a Arc

```rust
// Dado este codigo single-thread con Rc:

use std::rc::Rc;
use std::cell::RefCell;

struct Cache {
    data: Rc<RefCell<Vec<String>>>,
}

impl Cache {
    fn new() -> Self {
        Cache { data: Rc::new(RefCell::new(vec![])) }
    }

    fn add(&self, item: String) {
        self.data.borrow_mut().push(item);
    }

    fn get_all(&self) -> Vec<String> {
        self.data.borrow().clone()
    }
}

// a) Convierte este codigo para que funcione en multi-thread.
//    ¿Que cambios necesitas? (Rc→?, RefCell→?)
//
// b) Crea 3 threads que agreguen elementos al cache.
//    Imprime el contenido final desde el thread principal.
//
// c) ¿Por que no puedes usar Arc<RefCell<T>>?
//    ¿Que error da el compilador?
```
