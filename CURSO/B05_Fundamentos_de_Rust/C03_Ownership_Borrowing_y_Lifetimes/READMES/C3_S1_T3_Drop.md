# T03 — Drop

## El trait Drop

Cuando un valor sale de scope, Rust ejecuta su **destructor**
automaticamente. El mecanismo se define a traves del trait `Drop`,
que tiene un unico metodo:

```rust
// Definicion del trait en la libreria estandar:
pub trait Drop {
    fn drop(&mut self);
}
// &mut self — recibe una referencia mutable al valor.
// Se invoca automaticamente; NO se puede llamar directamente.
```

```rust
// RAII — Resource Acquisition Is Initialization.
// Patron heredado de C++ que Rust adopta:
//   1. Adquirir el recurso al crear el valor (constructor).
//   2. Liberar el recurso al destruir el valor (destructor).
//
// En C:
//   FILE *f = fopen("data.txt", "r");
//   fclose(f);                          // manual, se puede olvidar
//
// En Rust:
//   let f = File::open("data.txt")?;
//   // al salir de scope, Drop cierra el archivo automaticamente.
```

```rust
// NO se puede llamar drop() directamente:
struct MyStruct;

impl Drop for MyStruct {
    fn drop(&mut self) { println!("dropping"); }
}

fn main() {
    let s = MyStruct;
    // s.drop();  // ERROR: explicit destructor calls not allowed [E0040]
    // El compilador lo prohibe porque Rust llamaria drop()
    // de nuevo al final del scope → double-free.
    // Si necesitas destruir antes, usa std::mem::drop().
}
```

## Implementando Drop

Se implementa `Drop` cuando un tipo necesita logica de limpieza
personalizada: cerrar archivos, liberar locks, liberar memoria
externa, vaciar buffers, enviar mensajes de cierre.

```rust
struct DatabaseConnection {
    name: String,
    connected: bool,
}

impl DatabaseConnection {
    fn new(name: &str) -> Self {
        println!("  [OPEN] connection to '{}'", name);
        DatabaseConnection {
            name: String::from(name),
            connected: true,
        }
    }
}

impl Drop for DatabaseConnection {
    fn drop(&mut self) {
        if self.connected {
            println!("  [CLOSE] connection to '{}'", self.name);
            self.connected = false;
        }
    }
}

fn main() {
    let _db = DatabaseConnection::new("postgres");
    println!("  doing work...");
}
// Salida:
//   [OPEN] connection to 'postgres'
//   doing work...
//   [CLOSE] connection to 'postgres'
```

## Orden de destruccion

Rust tiene reglas deterministas sobre el orden en que se
destruyen los valores. No depende de un garbage collector.

```rust
// Regla 1: variables locales se destruyen en orden INVERSO (LIFO).

struct Named(&'static str);

impl Drop for Named {
    fn drop(&mut self) { println!("  drop: {}", self.0); }
}

fn main() {
    let _a = Named("a — first");
    let _b = Named("b — second");
    let _c = Named("c — third");
    println!("  --- end of main ---");
}
// Salida:
//   --- end of main ---
//   drop: c — third         <- ultimo declarado, primero destruido
//   drop: b — second
//   drop: a — first         <- primero declarado, ultimo destruido
//
// Logica: las variables posteriores pueden depender de las anteriores.
// Destruir en orden inverso garantiza que las dependencias existen.
```

```rust
// Regla 2: campos de struct se destruyen en orden de DECLARACION.

struct Container {
    first: Named,
    second: Named,
    third: Named,
}

impl Drop for Container {
    fn drop(&mut self) { println!("  drop: Container"); }
}

fn main() {
    let _c = Container {
        first: Named("field-1"),
        second: Named("field-2"),
        third: Named("field-3"),
    };
}
// Salida:
//   drop: Container         <- primero el struct
//   drop: field-1           <- luego campos en orden de declaracion
//   drop: field-2
//   drop: field-3
```

```rust
// Regla 3: Vec y tuplas destruyen elementos en orden de posicion (0, 1, 2...).

fn main() {
    let _v = vec![Named("v[0]"), Named("v[1]"), Named("v[2]")];
}
// drop: v[0], v[1], v[2]
```

```rust
// Resumen:
//   Variables locales  → INVERSO (LIFO)
//   Campos de struct   → Declaracion (primero a ultimo)
//   Elementos Vec      → Indice (0, 1, 2...)
//   Elementos tupla    → Posicion (0, 1, 2...)
//   Temporales         → Al final del statement donde se crearon
```

## std::mem::drop

La funcion libre `std::mem::drop()` permite destruir un valor
antes de que salga de scope:

```rust
// Firma:
pub fn drop<T>(_x: T) {}

// Es una funcion vacia. Recibe el valor por ownership,
// y al terminar, el valor sale de scope → se ejecuta Drop.
// La "magia" esta en el sistema de ownership, no en la funcion.
```

```rust
struct Resource { name: String }

impl Drop for Resource {
    fn drop(&mut self) { println!("  [FREE] {}", self.name); }
}

fn main() {
    let a = Resource { name: String::from("A") };
    let b = Resource { name: String::from("B") };

    drop(a);  // a se destruye AQUI, no al final de main
    println!("  between drops");
    // b se destruye al final de main
}
// Salida:
//   [FREE] A
//   between drops
//   [FREE] B
```

```rust
// Caso de uso 1: liberar un lock antes de tiempo.
use std::sync::Mutex;

fn main() {
    let data = Mutex::new(vec![1, 2, 3]);

    let guard = data.lock().unwrap();
    println!("  data: {:?}", *guard);
    drop(guard);  // libera el lock AHORA
    // Otros threads pueden tomar el lock durante la operacion larga.
    do_long_operation();
}

fn do_long_operation() { println!("  long operation..."); }
```

```rust
// Caso de uso 2: liberar memoria grande antes de una operacion larga.
fn process() {
    let large_data: Vec<u8> = vec![0u8; 100_000_000]; // ~100 MB
    let result = large_data.len();
    drop(large_data);  // libera ~100 MB ahora
    long_computation(result);  // se ejecuta con 100 MB menos en memoria
}

fn long_computation(n: usize) { println!("  result: {}", n); }
```

```rust
// Despues de drop(), el valor ya no es accesible:
fn main() {
    let s = String::from("hello");
    drop(s);
    // println!("{}", s);  // ERROR: use of moved value: `s`
}
```

## Drop y Copy son mutuamente exclusivos

Un tipo no puede implementar `Drop` y `Copy` al mismo tiempo:

```rust
// Copy = duplicar bits. Drop = limpieza especial al destruir.
// Si un tipo tuviera ambos:
//   let a = MyType::new();
//   let b = a;    // Copy: copia bit a bit
//   // drop() en a Y en b → double-free.

#[derive(Copy, Clone)]
struct Bad { value: i32 }

impl Drop for Bad {
    fn drop(&mut self) { println!("dropping"); }
}
// ERROR: the trait `Copy` cannot be implemented for this type;
//        the type has a destructor
```

```rust
// Si necesitas duplicar un tipo con Drop, implementa Clone:
#[derive(Clone)]
struct MyBuffer { data: Vec<u8> }

impl Drop for MyBuffer {
    fn drop(&mut self) { println!("  freeing {} bytes", self.data.len()); }
}

fn main() {
    let a = MyBuffer { data: vec![1, 2, 3] };
    let b = a.clone();  // copia independiente con su propia memoria
    println!("  a: {}, b: {}", a.data.len(), b.data.len());
}
// Salida:
//   a: 3, b: 3
//   freeing 3 bytes    <- drop de b (LIFO)
//   freeing 3 bytes    <- drop de a
```

## Drop en codigo unsafe

En escenarios como FFI o estructuras de bajo nivel se necesita
controlar manualmente cuando se ejecuta o se previene el destructor.

```rust
use std::mem::ManuallyDrop;

// ManuallyDrop<T> envuelve un valor y PREVIENE el drop automatico.

struct Important { name: String }

impl Drop for Important {
    fn drop(&mut self) { println!("  dropping: {}", self.name); }
}

fn main() {
    let _normal = Important { name: String::from("normal") };
    let _wrapped = ManuallyDrop::new(Important {
        name: String::from("wrapped"),
    });
    println!("  end of main");
}
// Salida:
//   end of main
//   dropping: normal
// "wrapped" nunca se destruye — su memoria se pierde (leak).
```

```rust
use std::mem::ManuallyDrop;

// Para destruir manualmente se usa ManuallyDrop::drop() — unsafe:
fn main() {
    let mut wrapped = ManuallyDrop::new(String::from("manual"));
    println!("  value: {}", *wrapped);  // Deref permite usarlo

    unsafe {
        ManuallyDrop::drop(&mut wrapped);
        // Usar wrapped despues es undefined behavior.
    }
}
```

```rust
// std::mem::forget — mueve un valor sin ejecutar su destructor.

use std::mem;

struct FileHandle { fd: i32 }

impl Drop for FileHandle {
    fn drop(&mut self) { println!("  closing fd {}", self.fd); }
}

fn main() {
    let handle = FileHandle { fd: 42 };
    let fd = handle.fd;
    mem::forget(handle);  // NO llama drop()
    println!("  transferred fd {} to C code", fd);
}
// Salida (sin "closing fd"):
//   transferred fd 42 to C code
//
// Uso tipico: FFI. Crear un recurso en Rust, transferirlo a C
// con mem::forget, y C llama a una funcion Rust para liberarlo.
```

## La garantia de Drop

Rust garantiza que `drop()` se llama cuando un valor sale de
scope, **salvo** en situaciones donde se previene explicitamente:

```rust
// Drop SE ejecuta:  salida normal, return anticipado, panic (unwind),
//                   salida de bloque interno.
//
// Drop NO se ejecuta:
//   mem::forget()       — leak intencional
//   ManuallyDrop        — prevencion explicita
//   process::abort()    — termina el proceso inmediatamente
//   Bucle infinito      — el valor nunca sale de scope
//   Double panic        — panic durante unwind causa abort
```

```rust
// Double panic — panic dentro de drop() durante unwind:

struct Bomb;

impl Drop for Bomb {
    fn drop(&mut self) { panic!("panic in drop!"); }
}

fn main() {
    let _bomb = Bomb;
    panic!("first panic");
    // 1. "first panic" inicia unwinding.
    // 2. Rust llama drop() en _bomb → segundo panic.
    // 3. Panic durante panic → proceso aborta.
}
// fatal runtime error: thread panicked while panicking. aborting.
```

```rust
// Por esta razon, drop() NO deberia hacer panic:

impl Drop for MyResource {
    fn drop(&mut self) {
        // MAL:  self.flush().unwrap();
        // BIEN:
        if let Err(e) = self.flush() {
            eprintln!("warning: flush failed during drop: {}", e);
        }
    }
}
```

## Ejemplos practicos

### Archivos temporales

```rust
use std::fs;

struct TempFile { path: String }

impl TempFile {
    fn new(path: &str) -> std::io::Result<Self> {
        fs::write(path, "")?;
        Ok(TempFile { path: String::from(path) })
    }
    fn path(&self) -> &str { &self.path }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        if let Err(e) = fs::remove_file(&self.path) {
            eprintln!("  warning: failed to delete {}: {}", self.path, e);
        }
    }
}

fn process_data() -> std::io::Result<()> {
    let tmp = TempFile::new("/tmp/processing.tmp")?;
    println!("  working with {}", tmp.path());
    Ok(())
    // Al salir (return normal o error con ?), el archivo se borra.
}
```

### RAII guards — Timer

```rust
use std::time::Instant;

struct Timer { label: String, start: Instant }

impl Timer {
    fn new(label: &str) -> Self {
        Timer { label: String::from(label), start: Instant::now() }
    }
}

impl Drop for Timer {
    fn drop(&mut self) {
        println!("  [TIMER] {}: {:.2?}", self.label, self.start.elapsed());
    }
}

fn compute() {
    let _timer = Timer::new("compute");
    let sum: u64 = (0..1_000_000).sum();
    println!("  sum = {}", sum);
}
// Salida:
//   sum = 499999500000
//   [TIMER] compute: 1.23ms
```

### RAII guards — MutexGuard

```rust
use std::sync::Mutex;

fn main() {
    let counter = Mutex::new(0);

    {
        let mut guard = counter.lock().unwrap();
        *guard += 1;
        println!("  counter = {}", *guard);
    } // guard sale de scope → Drop libera el lock

    let guard2 = counter.lock().unwrap();
    println!("  counter = {}", *guard2);
}
```

---

## Ejercicios

### Ejercicio 1 — Implementar Drop basico

```rust
// Crear un struct Logger con un campo name: String.
// Implementar Drop para que imprima "[LOG] closing: {name}".
// En main(), crear tres instancias y verificar que se destruyen
// en orden inverso al de declaracion.
//
// Salida esperada:
//   created a, created b, created c
//   --- end of main ---
//   [LOG] closing: c
//   [LOG] closing: b
//   [LOG] closing: a
```

### Ejercicio 2 — Orden de destruccion en structs

```rust
// Crear un struct Server con tres campos:
//   database: Named, cache: Named, logger: Named
// Implementar Drop para Server ("shutting down server").
// Verificar: primero drop de Server, luego campos en orden de declaracion.
```

### Ejercicio 3 — Drop explicito con std::mem::drop

```rust
// Crear un struct HeavyData con Vec<u8> de 10 millones de bytes.
// Implementar Drop para que imprima el tamanio liberado.
// En main():
//   1. Crear HeavyData y extraer un resultado.
//   2. Usar drop() para liberar la memoria antes de una "operacion larga".
//   3. Verificar que HeavyData no se puede usar despues del drop.
//   4. Verificar que drop NO se ejecuta dos veces al final del scope.
```

### Ejercicio 4 — Drop y Copy mutuamente exclusivos

```rust
// Intentar crear un struct con #[derive(Copy, Clone)] que
// tambien implemente Drop. Observar el error del compilador.
// Luego: implementar solo Clone + Drop.
// Verificar que clone() crea copia independiente y
// que ambos valores (original y clon) ejecutan drop().
```
