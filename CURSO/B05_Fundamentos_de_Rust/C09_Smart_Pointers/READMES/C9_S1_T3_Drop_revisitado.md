# T03 — Drop revisitado

## Repaso rapido del trait Drop

El trait `Drop` permite ejecutar codigo cuando un valor sale
de scope. Rust lo llama automaticamente — nunca llamas `drop()`
directamente sobre el trait:

```rust
struct FileHandle {
    name: String,
}

impl Drop for FileHandle {
    fn drop(&mut self) {
        println!("Closing file: {}", self.name);
    }
}

fn main() {
    let f = FileHandle { name: "data.txt".into() };
    println!("Using file...");
}  // f sale de scope → Drop::drop() se llama automaticamente

// Salida:
// Using file...
// Closing file: data.txt
```

```text
¿Que hace Drop?

  1. Rust llama Drop::drop(&mut self) justo antes de liberar la memoria
  2. Es el equivalente de un destructor en C++ o un finalizer en Java
  3. Se usa para limpiar recursos: cerrar archivos, liberar memoria
     del heap, cerrar conexiones de red, liberar locks, etc.
  4. Rust GARANTIZA que Drop se ejecuta — no hay leaks accidentales
     (con excepciones: mem::forget, ciclos con Rc)
```

---

## Orden de destruccion

El orden en que Rust ejecuta Drop sigue reglas deterministas.
Entender este orden es importante cuando los destructores tienen
efectos secundarios (ej: cerrar archivos, liberar locks).

### Variables locales — orden inverso de declaracion

```rust
struct Named(&'static str);

impl Drop for Named {
    fn drop(&mut self) {
        println!("dropping {}", self.0);
    }
}

fn main() {
    let a = Named("first");
    let b = Named("second");
    let c = Named("third");
}
// Salida:
// dropping third    ← c se creo ultimo, se destruye primero
// dropping second
// dropping first

// Es como una PILA (LIFO): el ultimo en entrar es el primero en salir.
// Esto tiene sentido: las variables declaradas despues pueden
// referenciar a las anteriores, asi que deben destruirse primero.
```

```rust
// Ejemplo practico — lock y recurso protegido:
use std::sync::Mutex;

fn example() {
    let data = Mutex::new(vec![1, 2, 3]);
    let guard = data.lock().unwrap();  // guard referencia data
    println!("{guard:?}");
}
// guard se destruye ANTES que data (orden inverso)
// Si fuera al reves: data se destruiria mientras guard aun la referencia → UB
// Rust previene esto con el orden inverso.
```

### Campos de un struct — orden de declaracion

```rust
struct Container {
    first: Named,
    second: Named,
    third: Named,
}

fn main() {
    let c = Container {
        first: Named("field-1"),
        second: Named("field-2"),
        third: Named("field-3"),
    };
}
// Salida:
// dropping field-1   ← orden de DECLARACION, no inverso
// dropping field-2
// dropping field-3

// Los campos se destruyen en el orden en que se DECLARAN en el struct.
// Esto es diferente de las variables locales (que son orden inverso).
```

```text
Resumen del orden de Drop:

  Variables locales:  orden INVERSO de declaracion (LIFO)
  Campos de struct:   orden de declaracion (primero al ultimo)
  Elementos de Vec:   orden de indice (0, 1, 2, ...)
  Elementos de tupla: orden de posicion (0, 1, 2, ...)
```

```rust
// Vec: elementos se destruyen en orden de indice
fn main() {
    let v = vec![Named("v[0]"), Named("v[1]"), Named("v[2]")];
}
// dropping v[0]
// dropping v[1]
// dropping v[2]

// Tupla: orden de posicion
fn main() {
    let t = (Named("t.0"), Named("t.1"), Named("t.2"));
}
// dropping t.0
// dropping t.1
// dropping t.2
```

### Scopes anidados

```rust
fn main() {
    let a = Named("outer");
    {
        let b = Named("inner");
        println!("inside block");
    }  // b se destruye aqui
    println!("after block");
}  // a se destruye aqui

// Salida:
// inside block
// dropping inner
// after block
// dropping outer
```

---

## std::mem::drop — drop anticipado

No puedes llamar `obj.drop()` directamente — el compilador lo
prohibe para evitar double-free. En su lugar, usas `std::mem::drop()`:

```rust
struct Lock { name: String }

impl Drop for Lock {
    fn drop(&mut self) {
        println!("Released lock: {}", self.name);
    }
}

fn main() {
    let lock = Lock { name: "db".into() };
    println!("Holding lock...");

    // lock.drop();  // error: explicit destructor calls not allowed
    drop(lock);      // OK: std::mem::drop() toma ownership y lo destruye
    // Equivalente a: { lock };  // mover a un scope que termina inmediatamente

    println!("Lock released, doing other work...");
}
// Salida:
// Holding lock...
// Released lock: db
// Lock released, doing other work...
```

```rust
// ¿Que es std::mem::drop realmente?
// Es la funcion mas simple posible:
//
// pub fn drop<T>(_x: T) { }
//
// Toma ownership del valor (por move) y no hace nada.
// Cuando la funcion termina, el valor sale de scope → Drop se ejecuta.
// Es elegantemente simple.
```

### Cuando usar drop anticipado

```rust
// Caso 1: liberar un lock antes de adquirir otro
use std::sync::Mutex;

fn transfer(from: &Mutex<i32>, to: &Mutex<i32>, amount: i32) {
    let mut from_guard = from.lock().unwrap();
    *from_guard -= amount;
    drop(from_guard);  // liberar lock de 'from' ANTES de adquirir 'to'

    let mut to_guard = to.lock().unwrap();
    *to_guard += amount;
    // to_guard se libera al final de la funcion
}
// Sin el drop(), ambos locks estarian activos simultaneamente
// → riesgo de deadlock si otro thread hace transfer(to, from, ...).
```

```rust
// Caso 2: liberar memoria grande que ya no necesitas
fn process() {
    let huge_data: Vec<u8> = load_from_file();  // 500 MB
    let summary = compute_summary(&huge_data);
    drop(huge_data);  // liberar 500 MB inmediatamente

    // Ahora trabajar con el resumen usando menos memoria:
    generate_report(&summary);  // puede necesitar mucha RAM tambien
}

fn load_from_file() -> Vec<u8> { vec![0; 500_000_000] }
fn compute_summary(_data: &[u8]) -> String { String::new() }
fn generate_report(_summary: &str) {}
```

```rust
// Caso 3: forzar el orden de destruccion
fn main() {
    let a = Named("a");
    let b = Named("b");

    // Sin drop: b se destruye antes que a (orden inverso)
    // Con drop: controlamos el orden
    drop(a);  // a se destruye aqui
    println!("a is gone, b still alive");
}  // b se destruye aqui

// Salida:
// dropping a
// a is gone, b still alive
// dropping b
```

### drop vs scope blocks

```rust
// Alternativa a drop(): crear un scope explicito
fn example() {
    {
        let lock = acquire_lock();
        // usar lock...
    }  // lock se destruye al final del bloque

    // equivalente a:
    let lock = acquire_lock();
    // usar lock...
    drop(lock);

    // Ambos son validos. El scope block es mas visual.
    // drop() es mas explicito sobre la intencion.
}

fn acquire_lock() -> Named { Named("lock") }
```

---

## ManuallyDrop — desactivar Drop automatico

`ManuallyDrop<T>` es un wrapper que impide que Rust ejecute
Drop automaticamente. Tu decides cuando (o si) destruir el valor:

```rust
use std::mem::ManuallyDrop;

let mut val = ManuallyDrop::new(Named("manual"));
println!("val created");

// val NO se destruye automaticamente al salir de scope.
// Debes destruirlo explicitamente:
unsafe {
    ManuallyDrop::drop(&mut val);
}
// Salida:
// val created
// dropping manual

// Si no llamas ManuallyDrop::drop(), el valor NUNCA se destruye.
// Esto es un memory leak intencional.
```

```rust
// ¿Por que es unsafe?
// Porque si llamas ManuallyDrop::drop() dos veces → double-free (UB).
// Rust no puede verificar en compilacion que solo lo llames una vez.

use std::mem::ManuallyDrop;

let mut val = ManuallyDrop::new(String::from("hello"));

unsafe {
    ManuallyDrop::drop(&mut val);  // OK: primera vez
    // ManuallyDrop::drop(&mut val);  // UB: double-free!
}
```

### Cuando usar ManuallyDrop

```rust
// Caso 1: evitar drop en union (tipos con drop no permitidos en union)
// Los unions en Rust no pueden contener tipos con Drop.
// ManuallyDrop desactiva Drop, permitiendo el uso en unions:

use std::mem::ManuallyDrop;

union MyUnion {
    i: i32,
    s: ManuallyDrop<String>,  // String tiene Drop, pero ManuallyDrop no
}

let mut u = MyUnion { s: ManuallyDrop::new("hello".into()) };

// Acceder al campo activo:
unsafe {
    println!("{}", *u.s);        // Deref: ManuallyDrop<String> → String
    ManuallyDrop::drop(&mut u.s);  // limpiar manualmente
}
```

```rust
// Caso 2: extraer un valor de un contenedor sin ejecutar drop
use std::mem::ManuallyDrop;

fn take_inner(mut wrapper: ManuallyDrop<String>) -> String {
    unsafe {
        // Extraer el valor sin ejecutar drop del wrapper:
        ManuallyDrop::take(&mut wrapper)
        // ManuallyDrop::take mueve el valor fuera.
        // El wrapper (ahora vacio) sale de scope sin ejecutar drop.
    }
}
```

```rust
// Caso 3: optimizar Drop en un Vec custom
// Cuando destruyes un Vec<T> donde T: Drop,
// Rust llama drop en cada elemento, luego libera el buffer.
// Si T no necesita Drop (ej: i32), ManuallyDrop evita iterarlos:

// (Esto es un patron interno de la stdlib, raramente lo necesitas)
```

---

## mem::forget — la alternativa insegura

`std::mem::forget` consume un valor sin ejecutar Drop.
A diferencia de ManuallyDrop, es una operacion de una sola vez:

```rust
use std::mem;

let s = String::from("hello");
mem::forget(s);
// s fue consumido pero Drop NO se ejecuto.
// La memoria del heap NUNCA se libera → memory leak.
// s ya no es accesible (fue movido a forget).
```

```rust
// forget es safe (no es unsafe) desde Rust 1.0
// ¿Por que no es unsafe si causa leaks?
//
// Porque un memory leak no es "memory unsafe" en el sentido de Rust:
// - No hay acceso a memoria liberada
// - No hay undefined behavior
// - No hay data races
// - Solo se pierde memoria — el programa sigue siendo seguro
//
// Rust garantiza memory safety, no ausencia de leaks.
```

```text
drop vs forget vs ManuallyDrop:

  drop(val):
  - Ejecuta Drop::drop(), libera recursos
  - El valor se destruye correctamente
  - Uso comun: destruccion anticipada

  mem::forget(val):
  - NO ejecuta Drop::drop()
  - El valor nunca se destruye → memory leak
  - Uso: transferir ownership a codigo C (FFI), evitar drop indeseado

  ManuallyDrop::new(val):
  - Envuelve el valor, desactivando drop automatico
  - Puedes destruirlo despues con ManuallyDrop::drop() (unsafe)
  - O extraerlo con ManuallyDrop::take() (unsafe)
  - Mas control que forget — puedes decidir despues
```

---

## Drop y Copy son mutuamente exclusivos

Un tipo no puede implementar ambos Drop y Copy. La razon es
que Copy implica duplicacion de bits, y si el tipo tiene Drop,
ambas copias ejecutarian Drop al salir de scope (double-free):

```rust
#[derive(Copy, Clone)]  // OK: i32 no tiene Drop
struct Point { x: i32, y: i32 }

// struct Handle { fd: i32 }
// impl Drop for Handle { fn drop(&mut self) { close(self.fd); } }
// impl Copy for Handle {}  // error: Copy and Drop are mutually exclusive
//
// Si Handle fuera Copy:
//   let a = Handle { fd: 3 };
//   let b = a;  // copia bits: b.fd == 3
//   // Ambos a y b ejecutarian drop() → close(3) dos veces!
```

```rust
// Solucion: usa Clone sin Copy
#[derive(Clone)]
struct Handle { fd: i32 }

impl Drop for Handle {
    fn drop(&mut self) {
        println!("closing fd {}", self.fd);
    }
}

// Clone requiere llamada explicita — no hay duplicacion accidental:
let a = Handle { fd: 3 };
let b = a.clone();  // clone explicito — podrías duplicar el fd aqui
// a fue movido por el diseño sin Copy, pero con clone tienes ambos

// En la practica, Clone para handles deberia duplicar el recurso
// (ej: dup(fd)) o usar Rc/Arc para compartir.
```

---

## Drop en la practica — patrones comunes

### Guard pattern (RAII)

```rust
// El patron guard: adquirir recurso en new(), liberar en drop()

struct Timer {
    label: String,
    start: std::time::Instant,
}

impl Timer {
    fn new(label: &str) -> Self {
        Timer {
            label: label.to_string(),
            start: std::time::Instant::now(),
        }
    }
}

impl Drop for Timer {
    fn drop(&mut self) {
        let elapsed = self.start.elapsed();
        println!("{}: {:.2?}", self.label, elapsed);
    }
}

fn expensive_computation() {
    let _timer = Timer::new("computation");
    // ... hacer trabajo ...
    std::thread::sleep(std::time::Duration::from_millis(100));
}  // _timer se destruye → imprime "computation: 100.XXms"
```

### Cleanup en caso de panic

```rust
// Drop se ejecuta INCLUSO durante unwinding de un panic:
struct Cleanup;

impl Drop for Cleanup {
    fn drop(&mut self) {
        println!("cleanup executed!");
    }
}

fn might_panic() {
    let _guard = Cleanup;
    panic!("something went wrong");
}
// Salida:
// cleanup executed!     ← Drop se ejecuta durante unwinding
// thread '...' panicked at 'something went wrong'
//
// Excepcion: si compilas con panic=abort, Drop NO se ejecuta
// porque el proceso termina inmediatamente.
```

### Drop con condicion

```rust
// A veces quieres ejecutar cleanup solo bajo ciertas condiciones:
struct TempFile {
    path: String,
    keep: bool,
}

impl TempFile {
    fn new(path: &str) -> Self {
        // crear el archivo...
        TempFile { path: path.to_string(), keep: false }
    }

    fn persist(&mut self) {
        self.keep = true;  // marcar para no borrar
    }
}

impl Drop for TempFile {
    fn drop(&mut self) {
        if !self.keep {
            println!("Deleting temp file: {}", self.path);
            // std::fs::remove_file(&self.path).ok();
        } else {
            println!("Keeping file: {}", self.path);
        }
    }
}

fn main() {
    let mut tmp = TempFile::new("/tmp/data.txt");
    // ... usar el archivo ...

    // Si todo salio bien, persistir:
    tmp.persist();
}  // Drop verifica keep y decide
```

---

## Errores comunes

```rust
// ERROR 1: llamar .drop() directamente
struct Res;
impl Drop for Res { fn drop(&mut self) { println!("dropped"); } }

let r = Res;
// r.drop();  // error: explicit use of destructor method
// Solucion:
drop(r);  // usa la funcion libre std::mem::drop
```

```rust
// ERROR 2: usar un valor despues de drop()
let s = String::from("hello");
drop(s);
// println!("{s}");  // error: borrow of moved value: `s`
// drop() toma ownership — el valor ya no existe.
```

```rust
// ERROR 3: asumir orden de drop en destructuring
let (a, b) = (Named("a"), Named("b"));
// El orden de drop de a y b depende de que son variables locales
// (orden inverso de declaracion), no del orden de la tupla.
// En este caso: b se destruye primero, luego a.
```

```rust
// ERROR 4: Drop con panic dentro de drop
struct BadDrop;

impl Drop for BadDrop {
    fn drop(&mut self) {
        panic!("panic in drop!");
    }
}

// Si un panic ocurre durante unwinding (otro panic ya activo),
// Rust llama abort() — el proceso termina inmediatamente.
// NUNCA hagas panic dentro de drop(). Usa Result o ignora errores:

struct SafeFile { path: String }

impl Drop for SafeFile {
    fn drop(&mut self) {
        // .ok() ignora el error silenciosamente — mejor que panic:
        // std::fs::remove_file(&self.path).ok();
        println!("cleanup for {}", self.path);
    }
}
```

```rust
// ERROR 5: olvidar que mem::forget causa leaks
use std::mem;

let v = vec![1, 2, 3];  // Vec aloca en el heap
mem::forget(v);           // Drop no se ejecuta
// Los 3 elementos (12 bytes) en el heap nunca se liberan.
// En un loop, esto acumula leaks hasta agotar la memoria.

// forget es util solo en casos especificos (FFI, ManuallyDrop interno).
// Para el 99% de los casos, usa drop() en su lugar.
```

---

## Cheatsheet

```text
Orden de destruccion:
  Variables locales:   INVERSO de declaracion (LIFO)
  Campos de struct:    orden de declaracion
  Elementos de Vec:    orden de indice (0, 1, 2, ...)
  Elementos de tupla:  orden de posicion

Drop anticipado:
  drop(val)            ejecuta Drop y libera (funcion libre)
  { val; }             scope block — destruye al cerrar
  val.drop()           PROHIBIDO — error de compilacion

Evitar Drop:
  mem::forget(val)     consume sin ejecutar Drop (safe, causa leak)
  ManuallyDrop::new(v) desactiva Drop automatico, control manual

ManuallyDrop:
  ManuallyDrop::new(v)          envolver (desactiva Drop)
  ManuallyDrop::drop(&mut v)    destruir (unsafe, una sola vez)
  ManuallyDrop::take(&mut v)    extraer valor (unsafe)
  ManuallyDrop::into_inner(v)   extraer valor (safe, consume wrapper)

Reglas clave:
  - Drop y Copy son mutuamente exclusivos
  - Drop se ejecuta durante panic unwinding (excepto panic=abort)
  - Nunca panic dentro de drop() — causa abort si ya hay unwinding
  - mem::forget es safe — leaks no son memory-unsafe
  - El compilador inserta drop automaticamente — no lo llames tu

Patron RAII (guard):
  Adquirir recurso en new() → liberar en drop()
  Ejemplos: MutexGuard, File, TempDir, Timer
```

---

## Ejercicios

### Ejercicio 1 — Orden de destruccion

```rust
// Predice el orden de salida ANTES de ejecutar:

struct D(&'static str);
impl Drop for D {
    fn drop(&mut self) { println!("drop {}", self.0); }
}

fn main() {
    let a = D("a");
    let b = D("b");
    {
        let c = D("c");
        let d = D("d");
    }
    let e = D("e");
    drop(b);
    println!("end of main");
}

// Escribe las lineas de salida en orden.
// Pistas:
// - ¿Cuando se destruyen c y d?
// - ¿Cuando se destruye b?
// - ¿En que orden se destruyen a y e?
```

### Ejercicio 2 — Guard pattern

```rust
// Implementa un struct DatabaseTransaction que:
//
// a) En new() imprime "BEGIN TRANSACTION"
//
// b) Tiene un metodo commit(&mut self) que marca la transaccion
//    como exitosa
//
// c) En Drop:
//    - Si fue committed → imprime "COMMIT"
//    - Si NO fue committed → imprime "ROLLBACK"
//
// d) Verifica estos escenarios:
//
//    // Caso 1: commit exitoso
//    let mut tx = DatabaseTransaction::new();
//    tx.commit();
//    drop(tx);
//    // BEGIN TRANSACTION, COMMIT
//
//    // Caso 2: rollback automatico (olvidamos commit)
//    let tx = DatabaseTransaction::new();
//    drop(tx);
//    // BEGIN TRANSACTION, ROLLBACK
//
//    // Caso 3: rollback en caso de error (simulado)
//    let tx = DatabaseTransaction::new();
//    if true /* simular error */ {
//        // tx sale de scope sin commit
//    }
//    // BEGIN TRANSACTION, ROLLBACK
```

### Ejercicio 3 — ManuallyDrop

```rust
// a) Crea un Vec<String> con 3 elementos.
//    Envuelvelo en ManuallyDrop.
//    Verifica que al final de main NO se imprima nada
//    (agrega impl Drop a un wrapper si necesitas verificar).
//
// b) Ahora usa ManuallyDrop::take() (unsafe) para extraer
//    el Vec y luego usarlo normalmente.
//    ¿Se ejecuta Drop al final?
//
// c) ¿Que pasa si llamas ManuallyDrop::drop() y luego
//    intentas acceder al valor? ¿Compila? ¿Es safe?
//
// d) Explica con tus palabras: ¿por que ManuallyDrop::drop()
//    es unsafe pero mem::forget() es safe?
```
