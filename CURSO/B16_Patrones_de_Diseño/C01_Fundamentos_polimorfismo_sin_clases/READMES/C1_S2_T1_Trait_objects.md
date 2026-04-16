# T01 — Trait objects (dyn Trait)

---

## 1. De vtables manuales a trait objects

En S01 construiste vtables a mano en C:

```c
// C: vtable manual
typedef struct {
    double (*area)(const void *self);
    void   (*destroy)(void *self);
} ShapeVtable;

typedef struct {
    const ShapeVtable *vt;
} Shape;

// Dispatch: s->vt->area(s)
```

Tres problemas de este enfoque:
1. Sin type safety — casts a `void *` pueden fallar silenciosamente
2. Sin garantia de completitud — puedes olvidar un campo en la vtable
3. Sin lifetime management — dangling pointers posibles

Rust resuelve los tres con **trait objects**: el compilador genera la
vtable, verifica los tipos, y el borrow checker protege los lifetimes.

```rust
// Rust: trait object — el compilador genera todo lo anterior
trait Shape {
    fn area(&self) -> f64;
}

// Dispatch: s.area()  — mismo mecanismo, sin void*, sin casts
```

Un trait object (`dyn Shape`) es la version de Rust de "puntero a vtable"
— pero type-safe, memory-safe, y generado automaticamente.

---

## 2. Que es un trait object

### 2.1 Definicion

Un **trait object** es un valor de tipo `dyn Trait` que permite dispatch
dinamico: la funcion concreta se elige en runtime, no en compile time.

```rust
trait Shape {
    fn area(&self) -> f64;
    fn name(&self) -> &str;
}

struct Circle { radius: f64 }
struct Rect { width: f64, height: f64 }

impl Shape for Circle {
    fn area(&self) -> f64 {
        std::f64::consts::PI * self.radius.powi(2)
    }
    fn name(&self) -> &str { "Circle" }
}

impl Shape for Rect {
    fn area(&self) -> f64 { self.width * self.height }
    fn name(&self) -> &str { "Rect" }
}
```

`dyn Shape` no se puede usar directamente — no tiene tamano conocido
(`Sized`). Siempre va detras de un puntero:

```rust
// Estas son las tres formas de usar trait objects:
let a: &dyn Shape = &Circle { radius: 5.0 };
let b: &mut dyn Shape = &mut Rect { width: 3.0, height: 4.0 };
let c: Box<dyn Shape> = Box::new(Circle { radius: 2.0 });
```

### 2.2 Por que `dyn`

La keyword `dyn` (obligatoria desde Rust 2021) marca explicitamente que
el dispatch es dinamico:

```rust
// Dispatch DINAMICO — decidido en runtime
fn print_area(s: &dyn Shape) {
    println!("{}", s.area());  // llama a Circle::area o Rect::area
}                              // segun lo que s apunte en runtime

// Dispatch ESTATICO — decidido en compile time
fn print_area_static<T: Shape>(s: &T) {
    println!("{}", s.area());  // el compilador sabe exactamente
}                              // que T es y genera codigo directo
```

`dyn` te dice "aqui hay una indirection en runtime". Sin `dyn`, el
compilador usa monomorphization (copia de la funcion por tipo).

---

## 3. El fat pointer

### 3.1 Anatomia

Un `&dyn Shape` no es un puntero normal de 8 bytes. Es un **fat pointer**
de 16 bytes que contiene:

```
  &dyn Shape (16 bytes):

  ┌──────────────────┬──────────────────┐
  │  data_ptr (8B)   │  vtable_ptr (8B) │
  └──────────────────┴──────────────────┘
        │                     │
        ▼                     ▼
  ┌───────────┐        ┌─────────────────────┐
  │ Circle    │        │ drop_in_place       │
  │ radius: 5 │        │ size: 8             │
  └───────────┘        │ align: 8            │
                       │ area: Circle::area  │
                       │ name: Circle::name  │
                       └─────────────────────┘
                       vtable generada por el compilador
```

| Componente | Contenido | Tamano |
|-----------|-----------|--------|
| data_ptr | Puntero a los datos de la instancia | 8 bytes |
| vtable_ptr | Puntero a la vtable del tipo concreto | 8 bytes |

Esto es la vtable **externa** (fat pointer) que mencionamos en S01/T03:
la vtable no vive dentro de la instancia sino en el puntero.

### 3.2 Comparacion con C

```
  C (vtable interna):              Rust (vtable externa / fat pointer):

  Circle en memoria:               Circle en memoria:
  ┌──────────────┐                 ┌──────────────┐
  │ vt ──→ vtable│  16 bytes       │ radius: 5.0  │  8 bytes
  │ radius: 5.0  │  (vt + data)    └──────────────┘  (solo data)
  └──────────────┘
                                   &dyn Shape:
  Shape *s (8 bytes):              ┌──────────────┬──────────────┐
  ┌──────────────┐                 │ data_ptr     │ vtable_ptr   │
  │ → Circle     │                 └──────────────┴──────────────┘
  └──────────────┘                 16 bytes (fat pointer)
```

| Aspecto | C (vtable interna) | Rust (fat pointer) |
|---------|-------------------|-------------------|
| Tamano de la instancia | +8 bytes (vptr) | Sin overhead |
| Tamano del puntero | 8 bytes | 16 bytes |
| Instancias × punteros | Muchas inst, pocos ptrs → C gana | Pocos inst, muchos ptrs → C gana |
| Pocas inst, muchos ptrs | C: overhead en cada inst | Rust: overhead en cada ptr |

En la practica, la diferencia rara vez importa. Lo que si importa: en
Rust la vtable nunca se puede corromper, en C un cast incorrecto la
destruye.

### 3.3 Verificar el tamano

```rust
use std::mem::size_of;

println!("{}", size_of::<&Circle>());       // 8 — puntero normal
println!("{}", size_of::<&dyn Shape>());    // 16 — fat pointer
println!("{}", size_of::<Box<dyn Shape>>()); // 16 — fat pointer
println!("{}", size_of::<Circle>());        // 8 — solo el f64
```

---

## 4. Formas de usar trait objects

### 4.1 Referencia inmutable: &dyn Trait

La forma mas comun. El dato vive en otro lado (stack, heap, static);
el trait object es un borrow.

```rust
fn describe(s: &dyn Shape) {
    println!("{}: area = {:.2}", s.name(), s.area());
}

let circle = Circle { radius: 5.0 };
let rect = Rect { width: 3.0, height: 4.0 };

describe(&circle);  // Circle: area = 78.54
describe(&rect);    // Rect: area = 12.00
```

Lifetime: `&dyn Shape` tiene un lifetime implicito — el dato debe vivir
al menos tanto como la referencia:

```rust
fn describe<'a>(s: &'a dyn Shape) { /* ... */ }
// Equivalente — el lifetime se elide normalmente
```

### 4.2 Referencia mutable: &mut dyn Trait

Para modificar el dato a traves del trait object. El trait debe tener
metodos que reciban `&mut self`:

```rust
trait Resizable: Shape {
    fn scale(&mut self, factor: f64);
}

impl Resizable for Circle {
    fn scale(&mut self, factor: f64) {
        self.radius *= factor;
    }
}

fn double_size(s: &mut dyn Resizable) {
    s.scale(2.0);
}

let mut c = Circle { radius: 5.0 };
double_size(&mut c);
// c.radius == 10.0
```

### 4.3 Box<dyn Trait>: ownership en heap

`Box<dyn Trait>` es un trait object que **posee** el dato en heap. Es
la forma de almacenar trait objects con lifetime independiente:

```rust
fn create_shape(kind: &str) -> Box<dyn Shape> {
    match kind {
        "circle" => Box::new(Circle { radius: 5.0 }),
        "rect"   => Box::new(Rect { width: 3.0, height: 4.0 }),
        _        => Box::new(Circle { radius: 1.0 }),
    }
}

let shapes: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle { radius: 5.0 }),
    Box::new(Rect { width: 3.0, height: 4.0 }),
    Box::new(Circle { radius: 2.0 }),
];

for s in &shapes {
    println!("{}: {:.2}", s.name(), s.area());
}
```

`Vec<Box<dyn Shape>>` es el equivalente directo del `Shape **shapes`
de S01/T03 — un array de punteros a tipos heterogeneos.

### 4.4 Tabla de formas

| Forma | Tamano | Ownership | Lifetime | Uso tipico |
|-------|--------|-----------|----------|-----------|
| `&dyn T` | 16 bytes | Prestamo | Atado al dato | Parametros de funcion |
| `&mut dyn T` | 16 bytes | Prestamo mutable | Atado al dato | Mutacion temporal |
| `Box<dyn T>` | 16 bytes | Owned (heap) | Independiente | Colecciones, retorno |
| `Rc<dyn T>` | 16 bytes | Compartido | Ref counted | Grafos, caches |
| `Arc<dyn T>` | 16 bytes | Compartido thread-safe | Atomic ref count | Multi-hilo |

---

## 5. Contenido de la vtable

### 5.1 Layout de la vtable

El compilador genera una vtable por cada combinacion `(Tipo, Trait)`.
La vtable contiene:

```
  vtable para (Circle, Shape):
  ┌─────────────────────────────────┐
  │ drop_in_place: fn(*mut Circle)  │  ← destructor
  │ size:          8                │  ← sizeof(Circle)
  │ align:         8                │  ← alignof(Circle)
  │ area:          Circle::area     │  ← metodos del trait
  │ name:          Circle::name     │
  └─────────────────────────────────┘

  vtable para (Rect, Shape):
  ┌─────────────────────────────────┐
  │ drop_in_place: fn(*mut Rect)    │
  │ size:          16               │  ← sizeof(Rect) = 2 * f64
  │ align:         8                │
  │ area:          Rect::area       │
  │ name:          Rect::name       │
  └─────────────────────────────────┘
```

La vtable siempre incluye `drop_in_place`, `size`, y `align`, ademas de
los metodos del trait. Esto permite que `Box<dyn Shape>` sepa como
liberar la memoria cuando sale de scope — sin que el usuario llame
`destroy` explicitamente.

### 5.2 Comparacion: vtable C vs vtable Rust

| Campo | C (manual) | Rust (generado) |
|-------|-----------|----------------|
| Metodos del trait | Definidos por el programador | Generados por el compilador |
| drop/destroy | Opcional (puedes olvidarlo) | Siempre incluido |
| size | Opcional (si lo agregas) | Siempre incluido |
| align | No se incluye normalmente | Siempre incluido |
| Garantia de completitud | Ninguna (campo NULL posible) | Compilador verifica todo |

---

## 6. Object safety

No todos los traits pueden usarse como `dyn Trait`. Para que un trait
sea **object-safe**, debe cumplir reglas:

### 6.1 Reglas principales

```rust
// OBJECT SAFE — se puede usar como dyn Shape
trait Shape {
    fn area(&self) -> f64;
    fn name(&self) -> &str;
}

// NO object safe — tiene metodo generico
trait BadGeneric {
    fn convert<T>(&self) -> T;  // T desconocido en runtime
}

// NO object safe — retorna Self
trait BadSelf {
    fn clone_me(&self) -> Self;  // Self desconocido en runtime
}

// NO object safe — requiere Sized
trait BadSized: Sized {
    fn do_thing(&self);
}
```

| Regla | Por que | Ejemplo violacion |
|-------|---------|-------------------|
| No metodos genericos | La vtable no puede tener infinitas entradas (una por T) | `fn convert<T>(&self) -> T` |
| No retornar `Self` | El tamano de Self es desconocido tras type erasure | `fn clone(&self) -> Self` |
| No bound `Self: Sized` | `dyn Trait` es `!Sized` por definicion | `trait T: Sized` |
| Primer parametro debe ser receptor | Sin `self`, no hay data pointer | `fn static_method()` |

### 6.2 Workarounds

#### Self en retorno → Box<Self> o where Self: Sized

```rust
trait Cloneable {
    // Retorna Box en vez de Self — object safe
    fn clone_boxed(&self) -> Box<dyn Cloneable>;
}

impl Cloneable for Circle {
    fn clone_boxed(&self) -> Box<dyn Cloneable> {
        Box::new(Circle { radius: self.radius })
    }
}
```

#### Excluir metodos no-safe con where Self: Sized

```rust
trait Shape {
    fn area(&self) -> f64;

    // Este metodo NO estara en la vtable, pero el trait sigue
    // siendo object-safe para los demas metodos
    fn into_pair(self) -> (f64, f64) where Self: Sized;
}
```

Metodos con `where Self: Sized` se excluyen del trait object. Puedes
llamarlos sobre el tipo concreto, pero no via `&dyn Shape`.

### 6.3 Diagnostico rapido

```
  ¿Puedo usar dyn MiTrait?

  ┌─ Algun metodo tiene parametros genericos?  → NO object safe
  ├─ Algun metodo retorna Self?                → NO object safe
  ├─ El trait requiere Self: Sized?             → NO object safe
  └─ Algun metodo no tiene receiver (&self)?   → NO object safe
       (metodos asociados sin self)

  Si ninguno aplica → SI, es object safe
```

---

## 7. Trait objects en colecciones

El uso mas comun de trait objects: almacenar tipos heterogeneos en una
misma coleccion.

### 7.1 Vec<Box<dyn Trait>>

```rust
let shapes: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle { radius: 5.0 }),
    Box::new(Rect { width: 3.0, height: 4.0 }),
    Box::new(Circle { radius: 2.0 }),
];

// Iterar — cada s es &Box<dyn Shape>, que deref a &dyn Shape
let total: f64 = shapes.iter().map(|s| s.area()).sum();
println!("Total area: {:.2}", total);  // 130.81

// Encontrar el mas grande
let largest = shapes.iter()
    .max_by(|a, b| a.area().partial_cmp(&b.area()).unwrap());
```

### 7.2 Comparacion con C

```
  C:                                Rust:
  Shape *shapes[3];                 Vec<Box<dyn Shape>>

  shapes[0] = (Shape *)             shapes.push(
    circle_create(5.0);               Box::new(Circle{radius:5.0})
  shapes[1] = (Shape *)             );
    rect_create(3.0, 4.0);          shapes.push(
                                       Box::new(Rect{w:3.0,h:4.0})
  for (i = 0; i < 3; i++)           );
    shape_area(shapes[i]);
                                     for s in &shapes {
  for (i = 0; i < 3; i++)               s.area();
    shape_destroy(shapes[i]);        }
                                     // drop automatico al salir
```

Diferencias clave:
- C necesita `destroy` explicito; Rust hace `drop` automatico
- C permite mezclar punteros invalidos; Rust garantiza validez
- C usa `Shape *` (8 bytes); Rust usa `Box<dyn Shape>` (16 bytes fat)

---

## 8. Dispatch: como se ejecuta una llamada

Cuando llamas `s.area()` sobre un `&dyn Shape`, el compilador genera:

```rust
// Lo que escribes:
let s: &dyn Shape = &circle;
let a = s.area();

// Lo que el compilador genera (pseudocodigo):
let (data_ptr, vtable_ptr) = s;   // desempacar fat pointer
let area_fn = vtable_ptr.area;     // leer puntero a funcion
let a = area_fn(data_ptr);         // invocar con data como self
```

En assembly (simplificado x86-64):

```
  ; s esta en dos registros: rdi (data), rsi (vtable)
  mov  rax, [rsi + 24]     ; cargar vtable[3] = area fn ptr
  mov  rdi, rdi            ; pasar data_ptr como primer arg (&self)
  call rax                 ; invocar la funcion
```

Dos loads de memoria + un call indirecto. El costo es identico al de
C (`s->vt->area(s)`): dos indirecciones.

---

## 9. Metodos por defecto y trait objects

Un trait puede tener metodos con implementacion por defecto. Estos
**si** aparecen en la vtable (el tipo puede overridearlos):

```rust
trait Shape {
    fn area(&self) -> f64;
    fn name(&self) -> &str;

    // Metodo por defecto — usa area() y name()
    fn describe(&self) -> String {
        format!("{}: area = {:.2}", self.name(), self.area())
    }
}

impl Shape for Circle {
    fn area(&self) -> f64 { /* ... */ }
    fn name(&self) -> &str { "Circle" }
    // describe usa la implementacion por defecto
}

impl Shape for Rect {
    fn area(&self) -> f64 { /* ... */ }
    fn name(&self) -> &str { "Rect" }

    // Override del default
    fn describe(&self) -> String {
        format!("{}({}x{})", self.name(), self.width, self.height)
    }
}

let shapes: Vec<&dyn Shape> = vec![&circle, &rect];
for s in &shapes {
    println!("{}", s.describe());
}
// Circle: area = 78.54         ← default
// Rect(3x4)                    ← override
```

En la vtable de Circle, `describe` apunta a la implementacion por
defecto. En la vtable de Rect, apunta al override. El dispatch es
identico para ambos — la vtable ya contiene el puntero correcto.

Equivalente en C: setear el campo de la vtable a una funcion default o
a un override. Pero en C no hay "default automatico" — lo haces a mano.

---

## 10. Supertraits

Un trait puede requerir que quien lo implemente tambien implemente otro
trait:

```rust
trait Named {
    fn name(&self) -> &str;
}

// Shape requiere Named — supertrait
trait Shape: Named {
    fn area(&self) -> f64;
}

// Implementar Shape OBLIGA a implementar Named tambien
impl Named for Circle {
    fn name(&self) -> &str { "Circle" }
}

impl Shape for Circle {
    fn area(&self) -> f64 { std::f64::consts::PI * self.radius.powi(2) }
}
```

Con `dyn Shape`, puedes llamar metodos de `Named` tambien:

```rust
fn describe(s: &dyn Shape) {
    // Puedes llamar metodos del supertrait
    println!("{}: {:.2}", s.name(), s.area());
}
```

La vtable de `dyn Shape` incluye los metodos de `Named` + los de
`Shape`. Es como si la vtable de C tuviera campos de ambas interfaces.

---

## 11. Comparacion final: mecanismos de polimorfismo

```
  Mecanismo          Resolucion     Type safe   Overhead
  ──────────         ──────────     ─────────   ────────
  C void* + fn ptr   Runtime        No          Minimo
  C vtable manual    Runtime        No          Un vptr por inst
  C++ virtual        Runtime        Parcial     Un vptr por inst
  Rust dyn Trait     Runtime        Completa    Fat pointer (16B)
  Rust generics      Compile time   Completa    Zero (monomorphized)
```

Cuando usar `dyn Trait` vs generics es el tema de T02 (Generics +
trait bounds). La regla rapida:

```
  ¿Conoces el tipo en compile time?
       SI  → generics (T: Trait)     → zero-cost, mas rapido
       NO  → trait object (dyn Trait) → flexible, un poco mas lento

  ¿Necesitas coleccion heterogenea?
       SI  → dyn Trait (Vec<Box<dyn T>>)
       NO  → generics probablemente basta
```

---

## Errores comunes

### Error 1 — Usar dyn Trait sin puntero

```rust
// ERROR: dyn Shape no tiene tamano conocido
let s: dyn Shape = circle;
// error[E0277]: the size for values of type `dyn Shape`
//              cannot be known at compilation time
```

`dyn Trait` es `!Sized`. Siempre debe ir detras de un puntero:
`&dyn Shape`, `Box<dyn Shape>`, `Rc<dyn Shape>`.

### Error 2 — Trait no object-safe

```rust
trait Cloneable {
    fn clone(&self) -> Self;  // retorna Self
}

// ERROR: no se puede crear trait object
let c: &dyn Cloneable = &circle;
// error[E0038]: the trait `Cloneable` cannot be made into an object
```

Solucion: retornar `Box<dyn Cloneable>` en vez de `Self`, o excluir el
metodo con `where Self: Sized`.

### Error 3 — Perder el tipo concreto

```rust
let s: Box<dyn Shape> = Box::new(Circle { radius: 5.0 });

// No puedes acceder a campos del tipo concreto:
// s.radius;  // ERROR: dyn Shape no tiene campo radius

// Necesitas downcast (requiere Any):
use std::any::Any;

// Solo funciona si el trait extiende Any
```

El type erasure de `dyn Trait` borra el tipo concreto. Si necesitas
recuperarlo, debes usar `Any` y downcast — analogo al tagged union en
C. Pero si necesitas downcast frecuentemente, probablemente debias usar
un enum en vez de `dyn Trait`.

### Error 4 — Lifetime ambiguo en retorno

```rust
// ERROR: lifetime no claro
fn get_shape() -> &dyn Shape {
    let c = Circle { radius: 5.0 };
    &c  // c se destruye al salir — dangling reference
}
```

Si necesitas retornar un trait object con ownership, usa `Box`:

```rust
fn get_shape() -> Box<dyn Shape> {
    Box::new(Circle { radius: 5.0 })  // ownership transferida
}
```

---

## Ejercicios

### Ejercicio 1 — Primer trait object

Define un trait `Describable` con un metodo `describe(&self) -> String`.
Implementalo para al menos tres tipos (`Person`, `Book`, `Color`).
Luego escribe una funcion que reciba `&[&dyn Describable]` e imprima
todas las descripciones.

**Prediccion**: Cual es el tamano de `&dyn Describable`? Y de
`&[&dyn Describable]`?

<details><summary>Respuesta</summary>

```rust
trait Describable {
    fn describe(&self) -> String;
}

struct Person { name: String, age: u32 }
struct Book { title: String, pages: u32 }
struct Color { r: u8, g: u8, b: u8 }

impl Describable for Person {
    fn describe(&self) -> String {
        format!("{}, {} years old", self.name, self.age)
    }
}

impl Describable for Book {
    fn describe(&self) -> String {
        format!("\"{}\" ({} pages)", self.title, self.pages)
    }
}

impl Describable for Color {
    fn describe(&self) -> String {
        format!("rgb({}, {}, {})", self.r, self.g, self.b)
    }
}

fn describe_all(items: &[&dyn Describable]) {
    for (i, item) in items.iter().enumerate() {
        println!("{}. {}", i + 1, item.describe());
    }
}

fn main() {
    let p = Person { name: "Ana".into(), age: 30 };
    let b = Book { title: "APUE".into(), pages: 960 };
    let c = Color { r: 255, g: 128, b: 0 };

    let items: Vec<&dyn Describable> = vec![&p, &b, &c];
    describe_all(&items);
}
```

Salida:

```
1. Ana, 30 years old
2. "APUE" (960 pages)
3. rgb(255, 128, 0)
```

Tamanos:
- `&dyn Describable` = 16 bytes (fat pointer: data + vtable)
- `&[&dyn Describable]` = 16 bytes (fat pointer de slice: ptr + len)
- Cada elemento del slice = 16 bytes

</details>

---

### Ejercicio 2 — Vec de Box<dyn Trait>

Crea un trait `Animal` con metodos `speak(&self) -> &str` y
`legs(&self) -> u32`. Implementa para `Dog`, `Cat`, `Spider`.

Almacena instancias en un `Vec<Box<dyn Animal>>` y escribe funciones:
- `noisiest`: retorna referencia al animal cuyo `speak` es mas largo
- `total_legs`: suma de patas de todos los animales

**Prediccion**: `Box<dyn Animal>` va al heap. Cuantas allocations hace
`Box::new(Dog { ... })` si `Dog` tiene un solo campo `String`?

<details><summary>Respuesta</summary>

```rust
trait Animal {
    fn speak(&self) -> &str;
    fn legs(&self) -> u32;
}

struct Dog { name: String }
struct Cat { name: String }
struct Spider;

impl Animal for Dog {
    fn speak(&self) -> &str { "Woof woof woof!" }
    fn legs(&self) -> u32 { 4 }
}

impl Animal for Cat {
    fn speak(&self) -> &str { "Meow" }
    fn legs(&self) -> u32 { 4 }
}

impl Animal for Spider {
    fn speak(&self) -> &str { "" }
    fn legs(&self) -> u32 { 8 }
}

fn noisiest(animals: &[Box<dyn Animal>]) -> Option<&dyn Animal> {
    animals.iter()
        .max_by_key(|a| a.speak().len())
        .map(|b| b.as_ref())
}

fn total_legs(animals: &[Box<dyn Animal>]) -> u32 {
    animals.iter().map(|a| a.legs()).sum()
}

fn main() {
    let zoo: Vec<Box<dyn Animal>> = vec![
        Box::new(Dog { name: "Rex".into() }),
        Box::new(Cat { name: "Mia".into() }),
        Box::new(Spider),
    ];

    if let Some(loud) = noisiest(&zoo) {
        println!("Noisiest: {}", loud.speak());  // Woof woof woof!
    }
    println!("Total legs: {}", total_legs(&zoo));  // 16
}
```

Allocations de `Box::new(Dog { name: "Rex".into() })`:
1. `String::from("Rex")` — allocation para el string en heap
2. `Box::new(...)` — allocation para el Dog en heap

Total: **2 allocations**. Si Dog no tuviera String (solo tipos Copy),
seria 1 allocation (solo el Box).

Para Spider (zero-sized type en la practica tiene 0 bytes de datos, pero
Box siempre alloca al menos 1 byte para tener una direccion unica en
versiones recientes), es 1 allocation.

</details>

---

### Ejercicio 3 — Metodos por defecto

Crea un trait `Logger` con:
- `log(&self, msg: &str)` — requerido
- `warn(&self, msg: &str)` — default: llama `log` con prefijo "[WARN]"
- `error(&self, msg: &str)` — default: llama `log` con prefijo "[ERROR]"

Implementa `StdoutLogger` (solo implementa `log`). Implementa
`PrefixLogger` que override `warn` para agregar timestamp ficticio.

Usa ambos como `&dyn Logger`.

**Prediccion**: Si `StdoutLogger` no implementa `warn`, que metodo
se ejecuta cuando llamas `logger.warn("test")` via `&dyn Logger`?

<details><summary>Respuesta</summary>

```rust
trait Logger {
    fn log(&self, msg: &str);

    fn warn(&self, msg: &str) {
        self.log(&format!("[WARN] {}", msg));
    }

    fn error(&self, msg: &str) {
        self.log(&format!("[ERROR] {}", msg));
    }
}

struct StdoutLogger;

impl Logger for StdoutLogger {
    fn log(&self, msg: &str) {
        println!("{}", msg);
    }
    // warn y error usan la implementacion por defecto
}

struct PrefixLogger {
    prefix: String,
}

impl Logger for PrefixLogger {
    fn log(&self, msg: &str) {
        println!("[{}] {}", self.prefix, msg);
    }

    // Override de warn con timestamp ficticio
    fn warn(&self, msg: &str) {
        println!("[{}] [WARN 2026-03-28T10:00:00] {}", self.prefix, msg);
    }
    // error usa el default (que llama a self.log)
}

fn run_logging(logger: &dyn Logger) {
    logger.log("System started");
    logger.warn("Low disk space");
    logger.error("Connection failed");
}

fn main() {
    let stdout = StdoutLogger;
    let prefix = PrefixLogger { prefix: "APP".into() };

    println!("--- StdoutLogger ---");
    run_logging(&stdout);

    println!("\n--- PrefixLogger ---");
    run_logging(&prefix);
}
```

Salida:

```
--- StdoutLogger ---
System started
[WARN] System started     ← default warn: llama self.log con prefijo
[ERROR] Connection failed  ← default error: llama self.log con prefijo

--- PrefixLogger ---
[APP] System started
[APP] [WARN 2026-03-28T10:00:00] Low disk space  ← override
[ERROR] [APP] Connection failed  ← default error → self.log → agrega [APP]
```

Para `StdoutLogger`, `warn` ejecuta la implementacion por defecto del
trait, que internamente llama `self.log()` — y `self.log()` despacha
via vtable a `StdoutLogger::log`. Es dispatch dinamico incluso dentro
del metodo por defecto.

</details>

---

### Ejercicio 4 — Object safety

Para cada trait, indica si es object-safe. Si no lo es, explica por que
y propone un fix:

```rust
// A
trait Printable {
    fn print(&self);
}

// B
trait Transformer {
    fn transform<T>(&self, input: T) -> T;
}

// C
trait Duplicable {
    fn duplicate(&self) -> Self;
}

// D
trait Validator {
    fn validate(&self, data: &str) -> bool;
    fn name() -> &'static str;
}

// E
trait Processor: Sized {
    fn process(&self);
}
```

**Prediccion**: Cuantos de los 5 son object-safe?

<details><summary>Respuesta</summary>

| Trait | Object safe | Problema | Fix |
|-------|-------------|---------|-----|
| A Printable | Si | Ninguno | — |
| B Transformer | No | Metodo generico `<T>` | Fijar T concreto o usar `Box<dyn Any>` |
| C Duplicable | No | Retorna `Self` | Retornar `Box<dyn Duplicable>` |
| D Validator | No | `name()` sin receiver `self` | Agregar `&self` o `where Self: Sized` |
| E Processor | No | Bound `Sized` en el trait | Quitar `: Sized` |

Solo **1 de 5** (A) es object-safe.

Fixes:

```rust
// B: fijar el tipo
trait Transformer {
    fn transform(&self, input: String) -> String;
}

// C: retornar Box
trait Duplicable {
    fn duplicate(&self) -> Box<dyn Duplicable>;
}

// D: excluir el metodo estatico
trait Validator {
    fn validate(&self, data: &str) -> bool;
    fn name() -> &'static str where Self: Sized;
    // O mejor: fn name(&self) -> &'static str
}

// E: quitar Sized
trait Processor {
    fn process(&self);
}
```

</details>

---

### Ejercicio 5 — Downcast con Any

Implementa un sistema donde `Vec<Box<dyn Any>>` almacena valores de
distintos tipos, y una funcion intenta extraer todos los `i32`:

```rust
use std::any::Any;

fn extract_ints(items: &[Box<dyn Any>]) -> Vec<i32> { todo!() }
```

**Prediccion**: `downcast_ref::<T>` retorna `Option<&T>`. Que retorna
si el tipo real no es `T`?

<details><summary>Respuesta</summary>

```rust
use std::any::Any;

fn extract_ints(items: &[Box<dyn Any>]) -> Vec<i32> {
    items.iter()
        .filter_map(|item| item.downcast_ref::<i32>())
        .copied()
        .collect()
}

fn main() {
    let items: Vec<Box<dyn Any>> = vec![
        Box::new(42i32),
        Box::new("hello"),
        Box::new(3.14f64),
        Box::new(17i32),
        Box::new(true),
        Box::new(99i32),
    ];

    let ints = extract_ints(&items);
    println!("{:?}", ints);  // [42, 17, 99]
}
```

`downcast_ref::<T>()` retorna `None` si el tipo real no es `T`. Nunca
hay panic ni UB — es type-safe.

Internamente, `Any` almacena un `TypeId` y el downcast compara el
`TypeId` pedido con el almacenado. Es el equivalente seguro del tagged
union en C:

```
  C:     if (v->type == TYPE_INT) { int x = v->as_int; }
  Rust:  if let Some(x) = v.downcast_ref::<i32>() { ... }
```

Pero `dyn Any` es rara vez la mejor opcion. Si sabes los tipos posibles,
un enum es mejor (exhaustivo, sin overhead de TypeId). `dyn Any` es el
ultimo recurso cuando realmente no puedes saber los tipos en compile
time (plugins, scripting engines, etc.).

</details>

---

### Ejercicio 6 — Traducir C a Rust

Traduce este sistema de vtables de C a trait objects de Rust:

```c
typedef struct {
    double (*area)(const void *self);
    void   (*draw)(const void *self);
    void   (*destroy)(void *self);
} ShapeVtable;

typedef struct { const ShapeVtable *vt; } Shape;
typedef struct { Shape base; double radius; } Circle;
typedef struct { Shape base; double w, h; } Rect;

double total_area(Shape **shapes, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += shapes[i]->vt->area(shapes[i]);
    return sum;
}
```

**Prediccion**: Cuantas lineas de Rust necesitas vs las de C? Que
desaparece completamente?

<details><summary>Respuesta</summary>

```rust
use std::f64::consts::PI;

trait Shape {
    fn area(&self) -> f64;
    fn draw(&self);
    // destroy no es necesario — Drop es automatico
}

struct Circle { radius: f64 }
struct Rect { w: f64, h: f64 }

impl Shape for Circle {
    fn area(&self) -> f64 { PI * self.radius.powi(2) }
    fn draw(&self) { println!("Circle(r={})", self.radius); }
}

impl Shape for Rect {
    fn area(&self) -> f64 { self.w * self.h }
    fn draw(&self) { println!("Rect({}x{})", self.w, self.h); }
}

fn total_area(shapes: &[Box<dyn Shape>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}

fn main() {
    let shapes: Vec<Box<dyn Shape>> = vec![
        Box::new(Circle { radius: 5.0 }),
        Box::new(Rect { w: 3.0, h: 4.0 }),
    ];
    println!("Total: {:.2}", total_area(&shapes));
}
```

Lo que desaparece completamente:
- **ShapeVtable struct** — el compilador la genera
- **Shape base struct** — no hay "herencia manual"
- **void\* casts** — el compilador sabe los tipos
- **destroy/free** — Drop automatico
- **static const vtable** — generada por el compilador
- **Constructor que cablea vtable** — no hay vtable que cablear
- **sizeof en malloc** — Box lo maneja

El codigo C (con headers, constructores, vtable) seria ~60 lineas. El
Rust equivalente es ~25. Mas importante: 0 oportunidades de crash por
cast incorrecto, NULL vtable, o memoria no liberada.

</details>

---

### Ejercicio 7 — Trait object como campo

Crea un struct `Notifier` que tenga un campo `logger: Box<dyn Logger>`
(reutiliza el trait Logger del ejercicio 3). El Notifier debe tener un
metodo `notify(&self, event: &str)` que loguee el evento.

Crea dos Notifiers con distinto logger (stdout y prefix) y demuestra
que se comportan diferente.

**Prediccion**: Si `Notifier` tiene un `Box<dyn Logger>`, implementa
`Clone` automaticamente? Por que si o por que no?

<details><summary>Respuesta</summary>

```rust
// Reutilizando Logger del ejercicio 3
trait Logger {
    fn log(&self, msg: &str);
}

struct StdoutLogger;
impl Logger for StdoutLogger {
    fn log(&self, msg: &str) { println!("{}", msg); }
}

struct FileLogger { path: String }
impl Logger for FileLogger {
    fn log(&self, msg: &str) {
        println!("[→ {}] {}", self.path, msg);
    }
}

struct Notifier {
    name: String,
    logger: Box<dyn Logger>,
}

impl Notifier {
    fn new(name: &str, logger: Box<dyn Logger>) -> Self {
        Notifier { name: name.to_string(), logger }
    }

    fn notify(&self, event: &str) {
        self.logger.log(&format!("[{}] {}", self.name, event));
    }
}

fn main() {
    let n1 = Notifier::new("Auth", Box::new(StdoutLogger));
    let n2 = Notifier::new("DB", Box::new(FileLogger {
        path: "db.log".into(),
    }));

    n1.notify("User logged in");
    // [Auth] User logged in

    n2.notify("Query executed");
    // [→ db.log] [DB] Query executed
}
```

`Notifier` **no** implementa `Clone` automaticamente porque `Box<dyn
Logger>` no implementa `Clone`. `dyn Logger` no tiene un metodo `clone`
en su vtable.

Para hacerlo clonable, necesitas agregar `clone_box` al trait (como
vimos en la seccion 6.2) o usar un crate como `dyn-clone`.

</details>

---

### Ejercicio 8 — Comparar tamanos

Sin ejecutar, predice los tamanos de cada tipo:

```rust
use std::mem::size_of;

struct A;
struct B(f64);
struct C(f64, f64);

trait T { fn f(&self); }
impl T for A { fn f(&self) {} }
impl T for B { fn f(&self) {} }
impl T for C { fn f(&self) {} }
```

| Expresion | Tamano (bytes) |
|-----------|---------------|
| `size_of::<A>()` | ? |
| `size_of::<B>()` | ? |
| `size_of::<C>()` | ? |
| `size_of::<&A>()` | ? |
| `size_of::<&dyn T>()` | ? |
| `size_of::<Box<dyn T>>()` | ? |
| `size_of::<Box<A>>()` | ? |
| `size_of::<Option<Box<dyn T>>>()` | ? |

**Prediccion**: Cual es el unico caso donde `Option<X>` no agrega bytes
extra?

<details><summary>Respuesta</summary>

| Expresion | Tamano | Razon |
|-----------|--------|-------|
| `size_of::<A>()` | 0 | Zero-sized type (ZST) |
| `size_of::<B>()` | 8 | Un f64 |
| `size_of::<C>()` | 16 | Dos f64 |
| `size_of::<&A>()` | 8 | Puntero normal (thin) |
| `size_of::<&dyn T>()` | 16 | Fat pointer (data + vtable) |
| `size_of::<Box<dyn T>>()` | 16 | Fat pointer (data + vtable) |
| `size_of::<Box<A>>()` | 8 | Thin pointer (tipo concreto conocido) |
| `size_of::<Option<Box<dyn T>>>()` | 16 | Niche optimization: None = null |

`Option<Box<dyn T>>` no agrega bytes extra porque Rust usa **niche
optimization**: `Box` nunca es null, asi que `None` se representa como
null pointer. El compilador reutiliza el valor "imposible" (null) para
representar `None` sin bits extra.

Esto aplica a todos los tipos que tienen un "nicho" (valor invalido):
`Box<T>`, `&T`, `NonZeroU32`, etc. `Option<&dyn T>` tambien es 16
bytes, no 24.

</details>

---

### Ejercicio 9 — Encontrar errores

Cada fragmento tiene un error. Identificalo sin compilar:

```rust
// Error A
trait Greet {
    fn greet(&self) -> String;
}
let g: dyn Greet = todo!();

// Error B
trait Serializable {
    fn serialize(&self) -> Vec<u8>;
    fn deserialize(data: &[u8]) -> Self;
}
fn save(s: &dyn Serializable) { /* ... */ }

// Error C
trait Counter {
    fn count(&mut self) -> u32;
}
fn use_counter(c: &dyn Counter) {
    println!("{}", c.count());
}
```

**Prediccion**: Cual es error de tipo, cual es object safety, y cual
es mutabilidad?

<details><summary>Respuesta</summary>

**Error A: `dyn Greet` sin puntero.**

`dyn Greet` es `!Sized` — no puede existir como valor directo en stack.
Error: `the size for values of type dyn Greet cannot be known at
compilation time`.

**Fix**: `let g: &dyn Greet = &something;` o `let g: Box<dyn Greet>`.

**Error B: Trait no object-safe.**

`deserialize` retorna `Self` y no tiene receiver (`&self`). Dos
violaciones: retorno `Self` y metodo estatico (sin `self`).

**Fix**:
```rust
trait Serializable {
    fn serialize(&self) -> Vec<u8>;
    fn deserialize(data: &[u8]) -> Self where Self: Sized;
}
```

O mejor, separar en dos traits:
```rust
trait Serializable {
    fn serialize(&self) -> Vec<u8>;
}
trait Deserializable: Sized {
    fn deserialize(data: &[u8]) -> Self;
}
```

**Error C: Mutabilidad incorrecta.**

`count(&mut self)` requiere `&mut self`, pero `c` es `&dyn Counter`
(inmutable). No puedes llamar un metodo `&mut self` sobre una
referencia `&`.

**Fix**: `fn use_counter(c: &mut dyn Counter)`.

</details>

---

### Ejercicio 10 — Reflexion: cuando dyn Trait

Responde sin ver la respuesta:

1. Da un ejemplo concreto donde `Vec<Box<dyn Trait>>` es la unica
   opcion razonable (un enum no funcionaria bien).

2. `dyn Trait` borra el tipo concreto. Generics lo conserva. Cual
   produce binarios mas pequenos y por que?

3. En C, toda genericidad via `void *` es dispatch dinamico. En Rust,
   puedes elegir entre dinamico (`dyn`) y estatico (generics). Cual es
   el criterio de decision?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. Cuando `dyn Trait` es necesario:**

Un sistema de plugins donde terceros pueden agregar tipos en runtime.
Por ejemplo, un editor de texto con plugins:

```rust
trait Plugin {
    fn name(&self) -> &str;
    fn on_keystroke(&mut self, key: char);
}

// Los plugins se cargan en runtime (ej. dylib)
// No puedes enumerar todos los tipos en un enum
let plugins: Vec<Box<dyn Plugin>> = load_plugins("plugins/");
```

Un enum requiere conocer todas las variantes en compile time. Si los
tipos vienen de bibliotecas externas o plugins, solo `dyn Trait`
funciona.

Otros casos: callbacks registrados por el usuario, strategy pattern con
estrategias configurables en runtime, middleware chains.

**2. Tamano de binario:**

`dyn Trait` produce binarios **mas pequenos**. Generics produce binarios
**mas grandes**.

Con generics, el compilador genera una copia de la funcion para cada
tipo concreto (monomorphization):

```
  fn process<T: Shape>(s: &T) → genera:
    process::<Circle>
    process::<Rect>
    process::<Triangle>
    ... una copia por tipo
```

Con `dyn Trait`, hay una sola copia de la funcion:

```
  fn process(s: &dyn Shape) → una sola funcion
  // despacha via vtable en runtime
```

El trade-off: generics es mas rapido (inlining, sin indirection),
`dyn` es mas compacto (una sola copia de codigo).

**3. Criterio de decision:**

```
  ¿El tipo se conoce en compile time?
    SI  → generics (zero-cost)
    NO  → dyn Trait (flexible)

  ¿Necesitas coleccion heterogenea (mezclar tipos)?
    SI  → dyn Trait
    NO  → generics

  ¿El rendimiento es critico (hot path)?
    SI  → generics (permite inlining)
    NO  → cualquiera, prefiere el mas legible

  ¿El binario debe ser pequeno (embedded)?
    SI  → dyn Trait (evita code bloat)
    NO  → generics
```

La mayoria del codigo Rust usa generics. `dyn Trait` se reserva para
cuando necesitas heterogeneidad o quieres limitar el tamano del binario.

</details>
