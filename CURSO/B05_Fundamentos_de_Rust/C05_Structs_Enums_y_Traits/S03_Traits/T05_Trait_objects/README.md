# T05 — Trait Objects

> Bloque 5 / Capitulo 5: Structs, Enums y Traits / Seccion 3: Traits / Tema 5

---

## Tabla de contenidos

1. [Que es un trait object](#que-es-un-trait-object)
2. [dyn Trait — sintaxis y uso basico](#dyn-trait--sintaxis-y-uso-basico)
3. [Static dispatch vs dynamic dispatch](#static-dispatch-vs-dynamic-dispatch)
4. [vtable — como funciona internamente](#vtable--como-funciona-internamente)
5. [&dyn Trait — referencias a trait objects](#dyn-trait--referencias-a-trait-objects)
6. [Box\<dyn Trait\> — trait objects en el heap](#boxdyn-trait--trait-objects-en-el-heap)
7. [Object safety](#object-safety)
8. [Comparacion con genericos](#comparacion-con-genericos)
9. [Multiples traits](#multiples-traits)
10. [Downcasting con Any](#downcasting-con-any)
11. [Patrones comunes](#patrones-comunes)
12. [Errores comunes](#errores-comunes)
13. [Resumen y cheatsheet](#resumen-y-cheatsheet)

---

## Que es un trait object

Un trait object es un mecanismo de Rust para lograr **polimorfismo
en tiempo de ejecucion** (dynamic dispatch). Permite tratar valores
de tipos distintos de manera uniforme, siempre que todos implementen
el mismo trait.

Cuando escribis `dyn Trait`, le estas diciendo al compilador:
"no se cual es el tipo concreto en tiempo de compilacion, pero se
que implementa este trait".

```rust
// Dos tipos distintos que implementan el mismo trait:

trait Animal {
    fn speak(&self) -> &str;
}

struct Dog;
struct Cat;

impl Animal for Dog {
    fn speak(&self) -> &str {
        "Woof!"
    }
}

impl Animal for Cat {
    fn speak(&self) -> &str {
        "Meow!"
    }
}

// Sin trait objects, necesitarias una funcion por cada tipo:
fn dog_speaks(d: &Dog) -> &str { d.speak() }
fn cat_speaks(c: &Cat) -> &str { c.speak() }

// Con trait objects, una sola funcion maneja ambos:
fn animal_speaks(a: &dyn Animal) -> &str {
    a.speak()
}
```

```rust
// La motivacion principal: colecciones heterogeneas.
// Un Vec<T> solo puede contener elementos del mismo tipo T.
// Pero con trait objects:

fn main() {
    let dog = Dog;
    let cat = Cat;

    // Todos son &dyn Animal — tipos distintos detras, interfaz comun:
    let animals: Vec<&dyn Animal> = vec![&dog, &cat];

    for animal in &animals {
        println!("{}", animal.speak());
    }
    // Woof!
    // Meow!
}
```

La necesidad de trait objects aparece cuando:
- No conoces todos los tipos posibles en tiempo de compilacion
  (plugins, extensiones de usuario).
- Necesitas almacenar valores de distintos tipos en una misma
  coleccion.
- Queres reducir el tamano del binario evitando la monomorphization.
- El tipo concreto se decide en tiempo de ejecucion (factory pattern,
  configuracion dinamica).

---

## dyn Trait — sintaxis y uso basico

La keyword `dyn` se antepone al nombre del trait para indicar
dynamic dispatch. Desde Rust 2021 (y en la practica desde 2018),
`dyn` es obligatorio — antes se podia escribir solo `&Trait`, pero
eso quedo deprecado:

```rust
trait Draw {
    fn draw(&self);
}

struct Circle {
    radius: f64,
}

struct Square {
    side: f64,
}

impl Draw for Circle {
    fn draw(&self) {
        println!("Drawing circle with radius {}", self.radius);
    }
}

impl Draw for Square {
    fn draw(&self) {
        println!("Drawing square with side {}", self.side);
    }
}

// dyn Draw = "algun tipo que implementa Draw, resuelto en runtime"
fn render(shape: &dyn Draw) {
    shape.draw();
}

fn main() {
    let c = Circle { radius: 5.0 };
    let s = Square { side: 3.0 };

    render(&c); // Drawing circle with radius 5
    render(&s); // Drawing square with side 3

    // &c se convierte automaticamente a &dyn Draw
    // &s se convierte automaticamente a &dyn Draw
}
```

```rust
// dyn Trait NO es un tipo de tamano conocido.
// No podes tener una variable de tipo dyn Draw directamente:

// ESTO NO COMPILA:
// let shape: dyn Draw = Circle { radius: 1.0 };
// Error: the size for values of type `dyn Draw` cannot be known
//        at compilation time

// Siempre necesitas un nivel de indireccion:
// &dyn Draw       — referencia
// Box<dyn Draw>   — puntero al heap
// Rc<dyn Draw>    — reference counting
// Arc<dyn Draw>   — reference counting atomico
```

```rust
// dyn Trait es un "unsized type" (tipo sin tamano conocido).
// En la terminologia de Rust, dyn Trait: !Sized.
// Esto explica por que siempre va detras de un puntero.

use std::mem;

fn main() {
    // Esto si funciona — el tamano del puntero es conocido:
    println!("Size of &dyn Draw: {}", mem::size_of::<&dyn Draw>());
    // 16 bytes en 64-bit (8 data ptr + 8 vtable ptr)

    println!("Size of Box<dyn Draw>: {}", mem::size_of::<Box<dyn Draw>>());
    // 16 bytes en 64-bit (8 data ptr + 8 vtable ptr)

    // Comparar con punteros a tipos concretos:
    println!("Size of &Circle: {}", mem::size_of::<&Circle>());
    // 8 bytes (solo data ptr, el tipo se conoce en compilacion)
}
```

---

## Static dispatch vs dynamic dispatch

Para entender trait objects hay que entender la diferencia
fundamental entre los dos mecanismos de dispatch en Rust:

```rust
trait Greet {
    fn hello(&self) -> String;
}

struct English;
struct Spanish;

impl Greet for English {
    fn hello(&self) -> String {
        String::from("Hello!")
    }
}

impl Greet for Spanish {
    fn hello(&self) -> String {
        String::from("Hola!")
    }
}

// STATIC DISPATCH — genericos con trait bounds:
// El compilador genera una copia de la funcion para cada tipo concreto.
fn greet_static<T: Greet>(g: &T) {
    println!("{}", g.hello());
}
// En el binario final existen:
//   greet_static::<English>  — version para English
//   greet_static::<Spanish>  — version para Spanish
// Cada llamada va directamente a la funcion correcta (sin indireccion).

// DYNAMIC DISPATCH — trait objects:
// Una sola copia de la funcion. El tipo concreto se resuelve en runtime
// consultando una vtable.
fn greet_dynamic(g: &dyn Greet) {
    println!("{}", g.hello());
}
// En el binario final existe UNA sola funcion greet_dynamic.
// Cada llamada pasa por una indireccion: leer la vtable, buscar el
// puntero a la implementacion concreta, saltar ahi.

fn main() {
    let en = English;
    let es = Spanish;

    // Static dispatch — resuelto en compilacion:
    greet_static(&en); // llama directamente a English::hello
    greet_static(&es); // llama directamente a Spanish::hello

    // Dynamic dispatch — resuelto en runtime:
    greet_dynamic(&en); // consulta la vtable de English, luego llama
    greet_dynamic(&es); // consulta la vtable de Spanish, luego llama
}
```

```rust
// impl Trait en posicion de argumento es azucar sintatico para genericos.
// Es STATIC dispatch, no dynamic:

// Estas dos firmas son equivalentes:
fn print_greeting_a(g: &impl Greet) {
    println!("{}", g.hello());
}

fn print_greeting_b<T: Greet>(g: &T) {
    println!("{}", g.hello());
}

// IMPORTANTE: impl Trait != dyn Trait
// impl Trait = static dispatch (monomorphization)
// dyn Trait  = dynamic dispatch (vtable)
//
// Confusion comun: pensar que impl Trait es dynamic.
// No lo es. El compilador conoce el tipo concreto en cada call site.
```

---

## vtable — como funciona internamente

Cuando creas un trait object, Rust construye un **fat pointer**
(puntero gordo) que contiene dos punteros:

1. **Data pointer**: apunta a los datos del valor concreto.
2. **Vtable pointer**: apunta a una tabla de funciones (vtable).

```
   Trait object (&dyn Animal):
   +-----------------+
   | data_ptr    (8B)|----> [ datos del Dog/Cat/etc. en memoria ]
   +-----------------+
   | vtable_ptr  (8B)|----> vtable
   +-----------------+

   vtable para Dog que implementa Animal:
   +------------------+
   | drop_fn      ptr |----> <Dog as Drop>::drop  (o drop glue)
   +------------------+
   | size         u64 |     sizeof(Dog)
   +------------------+
   | align        u64 |     alignof(Dog)
   +------------------+
   | speak_fn     ptr |----> <Dog as Animal>::speak
   +------------------+
```

```rust
// Diagrama conceptual del layout en memoria:
//
// Stack:
//   dog: Dog { ... }            <-- dato concreto
//   trait_obj: &dyn Animal      <-- fat pointer (16 bytes)
//     .data  -> &dog
//     .vtable -> VTABLE_DOG_ANIMAL
//
// Static memory (segmento de datos del binario):
//   VTABLE_DOG_ANIMAL:
//     drop:  ptr a Dog::drop
//     size:  tamanio de Dog
//     align: alineamiento de Dog
//     speak: ptr a <Dog as Animal>::speak
//
// Cuando llamas trait_obj.speak():
//   1. Lee trait_obj.vtable  (primer acceso a memoria)
//   2. Lee vtable.speak      (segundo acceso a memoria)
//   3. Llama a la funcion con trait_obj.data como &self
//
// Esto es la "indireccion" del dynamic dispatch:
// dos accesos a memoria extra vs una llamada directa.
```

```rust
// Cada combinacion (tipo concreto, trait) genera una vtable distinta.
// Si Dog implementa Animal y Display, hay dos vtables:
//   - VTABLE_DOG_ANIMAL (con metodos de Animal)
//   - VTABLE_DOG_DISPLAY (con metodos de Display)
//
// La vtable se genera en tiempo de compilacion y vive en la
// seccion read-only del binario. No hay costo en runtime para crearla.

// Podemos verificar el tamano del fat pointer:
use std::mem;

trait Foo {
    fn bar(&self);
}

impl Foo for i32 {
    fn bar(&self) {
        println!("{}", self);
    }
}

fn main() {
    let x: i32 = 42;
    let thin: &i32 = &x;
    let fat: &dyn Foo = &x;

    println!("&i32 size:     {}", mem::size_of_val(&thin)); // 8
    println!("&dyn Foo size: {}", mem::size_of_val(&fat));  // 16

    // El fat pointer es exactamente el doble: data + vtable.
}
```

```rust
// La vtable incluye siempre:
//   - Puntero a la funcion drop (para destruir el valor)
//   - Tamano del tipo (size)
//   - Alineamiento del tipo (align)
//   - Un puntero a funcion por cada metodo del trait
//
// Esto permite que Box<dyn Trait> sepa como liberar la memoria
// del valor concreto sin conocer su tipo en tiempo de compilacion:
// lee size de la vtable para saber cuantos bytes liberar.
```

---

## &dyn Trait — referencias a trait objects

La forma mas simple de usar trait objects es con referencias
prestadas. El valor concreto vive en el stack (o donde sea que
ya este) y el trait object es solo un fat pointer temporal:

```rust
trait Summary {
    fn summarize(&self) -> String;
}

struct Article {
    title: String,
    content: String,
}

struct Tweet {
    username: String,
    text: String,
}

impl Summary for Article {
    fn summarize(&self) -> String {
        format!("{}: {}...", self.title, &self.content[..20.min(self.content.len())])
    }
}

impl Summary for Tweet {
    fn summarize(&self) -> String {
        format!("@{}: {}", self.username, self.text)
    }
}

// &dyn Summary: referencia prestada a un trait object.
// El valor original sigue siendo dueno de la memoria.
fn print_summary(item: &dyn Summary) {
    println!("{}", item.summarize());
}

fn main() {
    let article = Article {
        title: String::from("Rust Rocks"),
        content: String::from("Rust is a systems programming language..."),
    };

    let tweet = Tweet {
        username: String::from("rustlang"),
        text: String::from("Rust 1.75 released!"),
    };

    print_summary(&article);
    print_summary(&tweet);

    // article y tweet siguen siendo accesibles aqui:
    println!("Title: {}", article.title);
}
```

```rust
// Lifetimes con &dyn Trait:
// Un &dyn Trait es una referencia, asi que tiene un lifetime.
// Normalmente el compilador lo infiere con elision:

fn first_summary<'a>(items: &'a [&'a dyn Summary]) -> &'a dyn Summary {
    items[0]
}
// El retorno vive tanto como los items pasados.

// Cuando el lifetime no se puede elidir, hay que anotarlo:
struct Notifier<'a> {
    target: &'a dyn Summary,
}
// target vive al menos tanto como Notifier.

impl<'a> Notifier<'a> {
    fn notify(&self) {
        println!("Notification: {}", self.target.summarize());
    }
}

fn main() {
    let tweet = Tweet {
        username: String::from("user"),
        text: String::from("hello"),
    };

    let notifier = Notifier { target: &tweet };
    notifier.notify();
    // tweet debe vivir mas que notifier — el borrow checker lo garantiza.
}
```

```rust
// &mut dyn Trait tambien existe:
trait Counter {
    fn increment(&mut self);
    fn value(&self) -> u32;
}

struct SimpleCounter {
    count: u32,
}

impl Counter for SimpleCounter {
    fn increment(&mut self) {
        self.count += 1;
    }

    fn value(&self) -> u32 {
        self.count
    }
}

fn increment_twice(counter: &mut dyn Counter) {
    counter.increment();
    counter.increment();
}

fn main() {
    let mut c = SimpleCounter { count: 0 };
    increment_twice(&mut c);
    println!("Count: {}", c.value()); // Count: 2
}
```

---

## Box\<dyn Trait\> — trait objects en el heap

Cuando necesitas **ownership** del trait object (no solo una
referencia prestada), usas `Box<dyn Trait>`. El valor concreto
se mueve al heap y Box es dueno de esa memoria:

```rust
trait Shape {
    fn area(&self) -> f64;
    fn name(&self) -> &str;
}

struct Circle {
    radius: f64,
}

struct Rectangle {
    width: f64,
    height: f64,
}

impl Shape for Circle {
    fn area(&self) -> f64 {
        std::f64::consts::PI * self.radius * self.radius
    }
    fn name(&self) -> &str {
        "Circle"
    }
}

impl Shape for Rectangle {
    fn area(&self) -> f64 {
        self.width * self.height
    }
    fn name(&self) -> &str {
        "Rectangle"
    }
}

fn main() {
    // Box<dyn Shape>: el valor concreto vive en el heap.
    // Box es el dueno. Cuando Box se destruye, libera la memoria.
    let shapes: Vec<Box<dyn Shape>> = vec![
        Box::new(Circle { radius: 5.0 }),
        Box::new(Rectangle { width: 4.0, height: 6.0 }),
    ];

    for shape in &shapes {
        println!("{}: area = {:.2}", shape.name(), shape.area());
    }
    // Circle: area = 78.54
    // Rectangle: area = 24.00

    // Al salir de scope, Vec se destruye, cada Box se destruye,
    // y cada Box llama drop del tipo concreto via la vtable.
}
```

```rust
// Box<dyn Trait> como campo de un struct:
// Esto es muy comun para lograr polimorfismo en structs.

struct Canvas {
    shapes: Vec<Box<dyn Shape>>,
}

impl Canvas {
    fn new() -> Self {
        Canvas { shapes: Vec::new() }
    }

    fn add(&mut self, shape: Box<dyn Shape>) {
        self.shapes.push(shape);
    }

    // Tambien se puede aceptar impl Into<Box<dyn Shape>>
    // pero es mas comun usar Box directamente o genericos.

    fn total_area(&self) -> f64 {
        self.shapes.iter().map(|s| s.area()).sum()
    }

    fn render(&self) {
        for shape in &self.shapes {
            println!("Rendering {} (area: {:.2})", shape.name(), shape.area());
        }
    }
}

fn main() {
    let mut canvas = Canvas::new();
    canvas.add(Box::new(Circle { radius: 3.0 }));
    canvas.add(Box::new(Rectangle { width: 2.0, height: 5.0 }));

    canvas.render();
    println!("Total area: {:.2}", canvas.total_area());
}
```

```rust
// Retornar Box<dyn Trait> desde funciones:
// Util como factory pattern.

fn create_shape(kind: &str) -> Box<dyn Shape> {
    match kind {
        "circle" => Box::new(Circle { radius: 1.0 }),
        "rectangle" => Box::new(Rectangle { width: 2.0, height: 3.0 }),
        _ => panic!("Unknown shape: {}", kind),
    }
}

fn main() {
    // El tipo concreto se decide en runtime:
    let shape = create_shape("circle");
    println!("{}: {:.2}", shape.name(), shape.area());

    // Aqui el compilador no sabe si shape es Circle o Rectangle.
    // Solo sabe que implementa Shape.
}
```

```rust
// Box<dyn Trait> vs &dyn Trait — cuando usar cada uno:
//
// &dyn Trait:
//   - Prestamos temporal. El llamador sigue siendo dueno.
//   - Sin allocacion en el heap.
//   - Requiere que el valor original viva mas tiempo.
//   - Ideal para parametros de funcion.
//
// Box<dyn Trait>:
//   - Ownership transferido. Box es el nuevo dueno.
//   - Alloca en el heap (costo de malloc/free).
//   - No hay restricciones de lifetime (excepto 'static si es necesario).
//   - Ideal para campos de structs, retornos, colecciones.
//
// Rc<dyn Trait> / Arc<dyn Trait>:
//   - Ownership compartido (multiples duenos).
//   - Rc para single-thread, Arc para multi-thread.
//   - Costo de reference counting.
```

---

## Object safety

No todos los traits pueden usarse como trait objects. Un trait
debe ser **object-safe** para poder escribir `dyn Trait`. Las
reglas de object safety existen porque el compilador necesita
poder construir una vtable para el trait.

### Regla 1: el trait no puede requerir Self: Sized

```rust
// Si un trait tiene el bound Self: Sized, no es object-safe:

trait NotObjectSafe: Sized {
    fn do_thing(&self);
}

// Error al intentar usar como trait object:
// fn use_it(x: &dyn NotObjectSafe) { ... }
// Error: the trait `NotObjectSafe` cannot be made into an object
//        because it requires `Self: Sized`

// Por que? dyn Trait es !Sized. Si el trait requiere Sized,
// dyn Trait no puede satisfacer ese bound.
```

### Regla 2: los metodos no pueden retornar Self

```rust
trait Clonable {
    fn clone_self(&self) -> Self;
}

// No se puede hacer: &dyn Clonable
// Por que? Si tenes un &dyn Clonable que apunta a un Dog,
// clone_self() retornaria un Dog. Pero el compilador no sabe
// en tiempo de compilacion que tamano tiene el retorno (Dog, Cat, etc.).
// No puede reservar espacio en el stack para un tipo desconocido.

// SOLUCION 1: retornar Box<dyn Trait> en vez de Self:
trait ClonableBoxed {
    fn clone_boxed(&self) -> Box<dyn ClonableBoxed>;
}

struct MyType {
    value: i32,
}

impl ClonableBoxed for MyType {
    fn clone_boxed(&self) -> Box<dyn ClonableBoxed> {
        Box::new(MyType { value: self.value })
    }
}

// SOLUCION 2: excluir el metodo problematico del trait object
// usando where Self: Sized en el metodo individual:
trait MixedSafety {
    fn safe_method(&self) -> String;  // OK para trait objects

    fn clone_self(&self) -> Self
    where
        Self: Sized;  // Este metodo NO estara disponible via dyn MixedSafety
}

// Ahora dyn MixedSafety es valido, pero solo expone safe_method.
fn use_it(x: &dyn MixedSafety) {
    println!("{}", x.safe_method());
    // x.clone_self() — NO COMPILA: no disponible en trait objects
}
```

### Regla 3: los metodos no pueden tener parametros de tipo generico

```rust
trait Serializer {
    fn serialize<T: std::fmt::Display>(&self, value: &T) -> String;
}

// No se puede hacer: &dyn Serializer
// Por que? La vtable necesita un puntero a funcion por cada metodo.
// Pero serialize es generico — hay infinitas versiones posibles
// (una por cada T). No se puede almacenar infinitos punteros en la vtable.

// SOLUCION 1: reemplazar el generico con un trait object:
trait SerializerFixed {
    fn serialize(&self, value: &dyn std::fmt::Display) -> String;
}

// SOLUCION 2: excluir el metodo generico con where Self: Sized:
trait SerializerMixed {
    fn name(&self) -> &str;

    fn serialize<T: std::fmt::Display>(&self, value: &T) -> String
    where
        Self: Sized;
}
```

### Regla 4: el trait no puede tener funciones asociadas sin receiver (&self)

```rust
trait Factory {
    fn create() -> Self;
}

// No se puede hacer: &dyn Factory
// Por que? create() no tiene &self, asi que no hay un receiver
// del que obtener la vtable. El compilador no sabe a que
// implementacion concreta llamar.

// SOLUCION: agregar where Self: Sized al metodo sin receiver:
trait FactoryFixed {
    fn create() -> Self
    where
        Self: Sized;

    fn describe(&self) -> String;
}
// Ahora dyn FactoryFixed es valido.
// Solo describe() esta disponible via trait objects.
```

### Resumen de reglas de object safety

```rust
// Un trait es object-safe si TODOS sus metodos cumplen:
//
// 1. No retornan Self (a menos que tengan where Self: Sized)
// 2. No tienen parametros de tipo generico (a menos que where Self: Sized)
// 3. Tienen un receiver (&self, &mut self, self, self: Box<Self>, etc.)
//    (o tienen where Self: Sized)
//
// Y ademas el trait NO requiere Self: Sized como supertrait.
//
// Metodos con where Self: Sized se excluyen del trait object
// (no estan en la vtable) pero no rompen la object safety del trait.

// Ejemplo completo de trait object-safe con metodos mixtos:
trait Storage {
    // Disponible via dyn Storage:
    fn read(&self, key: &str) -> Option<String>;
    fn write(&mut self, key: &str, value: &str);
    fn keys(&self) -> Vec<String>;

    // NO disponible via dyn Storage (excluido):
    fn merge<T: Storage>(&mut self, other: &T)
    where
        Self: Sized;

    // NO disponible via dyn Storage (excluido):
    fn duplicate(&self) -> Self
    where
        Self: Sized;
}
```

---

## Comparacion con genericos

La decision entre genericos (static dispatch) y trait objects
(dynamic dispatch) es una de las mas importantes en el diseno
de APIs en Rust:

```rust
trait Processor {
    fn process(&self, data: &str) -> String;
}

struct Upper;
struct Lower;

impl Processor for Upper {
    fn process(&self, data: &str) -> String {
        data.to_uppercase()
    }
}

impl Processor for Lower {
    fn process(&self, data: &str) -> String {
        data.to_lowercase()
    }
}

// GENERICOS — static dispatch (monomorphization):
fn process_static<P: Processor>(processor: &P, data: &str) -> String {
    processor.process(data)
}
// El compilador genera:
//   process_static::<Upper>(...)
//   process_static::<Lower>(...)
// Ventajas: llamada directa, posible inlining, maximo rendimiento.
// Desventajas: binario mas grande, mas tiempo de compilacion.

// TRAIT OBJECTS — dynamic dispatch:
fn process_dynamic(processor: &dyn Processor, data: &str) -> String {
    processor.process(data)
}
// El compilador genera UNA sola funcion.
// Ventajas: binario mas chico, compilacion mas rapida.
// Desventajas: indireccion en cada llamada, no se puede hacer inline.
```

```rust
// Comparativa detallada:
//
// +-------------------------+---------------------+---------------------+
// | Aspecto                 | Genericos           | Trait Objects       |
// +-------------------------+---------------------+---------------------+
// | Dispatch                | Compilacion         | Runtime             |
// | Rendimiento             | Maximo (inline)     | Indireccion (vtable)|
// | Tamano binario          | Mayor (N copias)    | Menor (1 copia)     |
// | Tiempo compilacion      | Mayor               | Menor               |
// | Heterogeneidad          | No (un T por vez)   | Si (mezcla tipos)   |
// | Object safety necesaria | No                  | Si                  |
// | Tipo conocido en        | Compilacion         | Runtime             |
// | Inlining posible        | Si                  | No                  |
// +-------------------------+---------------------+---------------------+
//
// Cuando elegir genericos:
//   - Hot paths donde el rendimiento importa.
//   - Todos los tipos son conocidos en tiempo de compilacion.
//   - Necesitas metodos que no son object-safe.
//   - Queres que el compilador optimice agresivamente.
//
// Cuando elegir trait objects:
//   - Colecciones heterogeneas (Vec<Box<dyn Trait>>).
//   - Plugin systems o extensibilidad en runtime.
//   - Queres reducir el tamano del binario.
//   - La performance de la indireccion es aceptable.
```

```rust
// Un patron comun es ofrecer ambas interfaces:

trait Logger {
    fn log(&self, message: &str);
}

// API generica para cuando el rendimiento importa:
fn run_with_logger<L: Logger>(logger: &L) {
    logger.log("Starting process");
    // ... logica ...
    logger.log("Done");
}

// API con trait object para cuando la flexibilidad importa:
fn run_with_any_logger(logger: &dyn Logger) {
    logger.log("Starting process");
    // ... logica ...
    logger.log("Done");
}

// El llamador elige segun su contexto.
```

```rust
// impl Trait en posicion de retorno vs Box<dyn Trait>:

trait Widget {
    fn render(&self) -> String;
}

struct Button { label: String }
struct TextBox { placeholder: String }

impl Widget for Button {
    fn render(&self) -> String {
        format!("<button>{}</button>", self.label)
    }
}

impl Widget for TextBox {
    fn render(&self) -> String {
        format!("<input placeholder='{}'>", self.placeholder)
    }
}

// impl Trait en retorno: el compilador conoce el tipo concreto.
// Pero solo puede retornar UN tipo:
fn make_button() -> impl Widget {
    Button { label: String::from("OK") }
}
// Esto es static dispatch. No hay vtable. El tipo es Button
// aunque el llamador solo ve "impl Widget".

// Box<dyn Trait> en retorno: puede retornar distintos tipos:
fn make_widget(kind: &str) -> Box<dyn Widget> {
    match kind {
        "button" => Box::new(Button { label: String::from("OK") }),
        "text"   => Box::new(TextBox { placeholder: String::from("Enter text") }),
        _        => panic!("Unknown widget"),
    }
}
// Esto es dynamic dispatch. Se necesita cuando la decision
// depende de datos de runtime.

// ESTO NO COMPILA con impl Trait:
// fn make_widget_static(kind: &str) -> impl Widget {
//     match kind {
//         "button" => Button { label: String::from("OK") },
//         "text"   => TextBox { placeholder: String::from("Enter text") },
//         _ => panic!(),
//     }
// }
// Error: `match` arms have incompatible types
// impl Trait espera UN solo tipo concreto.
```

---

## Multiples traits

A veces necesitas que un trait object implemente mas de un trait.
Rust permite combinaciones, pero con restricciones:

```rust
use std::fmt;

trait Drawable {
    fn draw(&self);
}

struct Widget {
    name: String,
}

impl Drawable for Widget {
    fn draw(&self) {
        println!("Drawing: {}", self.name);
    }
}

impl fmt::Display for Widget {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Widget({})", self.name)
    }
}

// dyn Trait1 + Trait2: el trait object implementa ambos traits.
fn show_and_draw(item: &(dyn Drawable + fmt::Display)) {
    println!("Display: {}", item);
    item.draw();
}

fn main() {
    let w = Widget { name: String::from("menu") };
    show_and_draw(&w);
    // Display: Widget(menu)
    // Drawing: menu
}
```

```rust
// Auto traits (Send, Sync) se combinan frecuentemente:

trait Task {
    fn execute(&self);
}

// dyn Task + Send: el trait object se puede enviar entre threads.
// dyn Task + Send + Sync: se puede enviar Y compartir entre threads.

fn spawn_task(task: Box<dyn Task + Send>) {
    std::thread::spawn(move || {
        task.execute();
    });
}

// Esto es MUY comun en codigo async:
// Box<dyn Future<Output = ()> + Send>
// Pin<Box<dyn Future<Output = ()> + Send>>
```

```rust
// Limitaciones con multiples traits no-auto:
// Solo podes combinar UN trait principal con auto traits.

trait Readable {
    fn read(&self) -> Vec<u8>;
}

trait Writable {
    fn write(&mut self, data: &[u8]);
}

// ESTO NO COMPILA (dos traits no-auto):
// fn process(rw: &dyn Readable + Writable) { ... }
// Error: only auto traits can be used as additional traits in a trait object

// SOLUCION: crear un supertrait que combine ambos:
trait ReadWrite: Readable + Writable {
    // No necesita metodos propios — solo combina.
}

// Blanket impl: cualquier tipo que implemente ambos, implementa ReadWrite:
impl<T: Readable + Writable> ReadWrite for T {}

// Ahora si funciona:
fn process(rw: &mut dyn ReadWrite) {
    let data = rw.read();
    rw.write(&data);
}
```

```rust
// Lifetime bounds en trait objects:
// dyn Trait tiene un lifetime implicito que depende del contexto.

// En una referencia, el lifetime es explicito:
// &'a dyn Trait = referencia con lifetime 'a a un trait object

// En Box, el lifetime default es 'static:
// Box<dyn Trait> = Box<dyn Trait + 'static>
// Esto significa que el tipo concreto no puede contener referencias
// temporales (a menos que especifiques otro lifetime).

// Para Box con lifetime no-'static:
struct Wrapper<'a> {
    inner: Box<dyn fmt::Display + 'a>,
    // El tipo concreto puede contener referencias con lifetime 'a.
}

fn wrap_ref<'a>(s: &'a str) -> Wrapper<'a> {
    Wrapper { inner: Box::new(s) }
    // &str implementa Display. El lifetime de s se propaga.
}
```

---

## Downcasting con Any

Normalmente, un trait object oculta el tipo concreto. Pero a veces
necesitas recuperar el tipo original. Para eso existe `std::any::Any`:

```rust
use std::any::Any;

trait Plugin: Any {
    fn name(&self) -> &str;
    fn execute(&self);

    // Metodo helper para downcasting:
    fn as_any(&self) -> &dyn Any;
}

struct AudioPlugin {
    volume: f32,
}

struct VideoPlugin {
    resolution: (u32, u32),
}

impl Plugin for AudioPlugin {
    fn name(&self) -> &str { "audio" }
    fn execute(&self) {
        println!("Playing audio at volume {}", self.volume);
    }
    fn as_any(&self) -> &dyn Any { self }
}

impl Plugin for VideoPlugin {
    fn name(&self) -> &str { "video" }
    fn execute(&self) {
        println!("Rendering {}x{}", self.resolution.0, self.resolution.1);
    }
    fn as_any(&self) -> &dyn Any { self }
}

fn main() {
    let plugins: Vec<Box<dyn Plugin>> = vec![
        Box::new(AudioPlugin { volume: 0.8 }),
        Box::new(VideoPlugin { resolution: (1920, 1080) }),
    ];

    for plugin in &plugins {
        plugin.execute();

        // Intentar downcast a AudioPlugin:
        if let Some(audio) = plugin.as_any().downcast_ref::<AudioPlugin>() {
            println!("  Audio volume: {}", audio.volume);
        }

        // Intentar downcast a VideoPlugin:
        if let Some(video) = plugin.as_any().downcast_ref::<VideoPlugin>() {
            println!("  Resolution: {}x{}", video.resolution.0, video.resolution.1);
        }
    }
}
// Playing audio at volume 0.8
//   Audio volume: 0.8
// Rendering 1920x1080
//   Resolution: 1920x1080
```

```rust
// Any::downcast_ref<T>() retorna Option<&T>.
// Any::downcast_mut<T>() retorna Option<&mut T>.
//
// Para Box<dyn Any>:
// Box::<dyn Any>::downcast::<T>() retorna Result<Box<T>, Box<dyn Any>>.

use std::any::Any;

fn process_value(value: Box<dyn Any>) {
    // Intentar convertir a String:
    if let Ok(s) = value.downcast::<String>() {
        println!("Got string: {}", s);
        return;
    }

    // Si no era String, fallo y recuperamos el Box<dyn Any>:
    // (en este ejemplo no hacemos nada porque value ya se consumio)
    println!("Unknown type");
}

fn main() {
    process_value(Box::new(String::from("hello")));
    process_value(Box::new(42_i32));
}
// Got string: hello
// Unknown type
```

```rust
// ADVERTENCIA: el downcasting rompe la abstraccion del trait object.
// Es una senal de que quiz el diseno necesita replantearse.
// Usalo solo cuando realmente no hay alternativa.
//
// Alternativas al downcasting:
//   1. Agregar metodos al trait para cubrir la funcionalidad necesaria.
//   2. Usar un enum en vez de trait objects (si los tipos son conocidos).
//   3. Visitor pattern.
//
// Any solo funciona con tipos 'static (sin referencias prestadas).
// No podes hacer downcast de un &'a str a traves de Any.
```

---

## Patrones comunes

### Colecciones heterogeneas

```rust
// El caso de uso mas clasico de trait objects:
// almacenar valores de distintos tipos en un mismo Vec.

trait Event {
    fn timestamp(&self) -> u64;
    fn description(&self) -> String;
}

struct ClickEvent {
    ts: u64,
    x: i32,
    y: i32,
}

struct KeyEvent {
    ts: u64,
    key: char,
}

struct ResizeEvent {
    ts: u64,
    width: u32,
    height: u32,
}

impl Event for ClickEvent {
    fn timestamp(&self) -> u64 { self.ts }
    fn description(&self) -> String {
        format!("Click at ({}, {})", self.x, self.y)
    }
}

impl Event for KeyEvent {
    fn timestamp(&self) -> u64 { self.ts }
    fn description(&self) -> String {
        format!("Key press: '{}'", self.key)
    }
}

impl Event for ResizeEvent {
    fn timestamp(&self) -> u64 { self.ts }
    fn description(&self) -> String {
        format!("Resize to {}x{}", self.width, self.height)
    }
}

fn main() {
    let events: Vec<Box<dyn Event>> = vec![
        Box::new(ClickEvent { ts: 1, x: 100, y: 200 }),
        Box::new(KeyEvent { ts: 2, key: 'a' }),
        Box::new(ResizeEvent { ts: 3, width: 800, height: 600 }),
    ];

    // Procesamiento uniforme de eventos heterogeneos:
    for event in &events {
        println!("[{}] {}", event.timestamp(), event.description());
    }
    // [1] Click at (100, 200)
    // [2] Key press: 'a'
    // [3] Resize to 800x600
}
```

### Strategy pattern

```rust
// Seleccionar un algoritmo en runtime.

trait Compressor {
    fn compress(&self, data: &[u8]) -> Vec<u8>;
    fn name(&self) -> &str;
}

struct NoCompression;
struct RunLengthEncoding;
struct DummyZip;

impl Compressor for NoCompression {
    fn compress(&self, data: &[u8]) -> Vec<u8> {
        data.to_vec()
    }
    fn name(&self) -> &str { "none" }
}

impl Compressor for RunLengthEncoding {
    fn compress(&self, data: &[u8]) -> Vec<u8> {
        // Implementacion simplificada
        let mut result = Vec::new();
        let mut i = 0;
        while i < data.len() {
            let byte = data[i];
            let mut count: u8 = 1;
            while i + (count as usize) < data.len()
                && data[i + count as usize] == byte
                && count < 255
            {
                count += 1;
            }
            result.push(count);
            result.push(byte);
            i += count as usize;
        }
        result
    }
    fn name(&self) -> &str { "rle" }
}

impl Compressor for DummyZip {
    fn compress(&self, data: &[u8]) -> Vec<u8> {
        // Simulacion
        let mut result = vec![0x50, 0x4B]; // magic bytes
        result.extend_from_slice(data);
        result
    }
    fn name(&self) -> &str { "zip" }
}

struct FileWriter {
    compressor: Box<dyn Compressor>,
}

impl FileWriter {
    fn new(compressor: Box<dyn Compressor>) -> Self {
        FileWriter { compressor }
    }

    fn write(&self, data: &[u8]) {
        let compressed = self.compressor.compress(data);
        println!(
            "Writing {} bytes with {} compression (was {} bytes)",
            compressed.len(),
            self.compressor.name(),
            data.len()
        );
    }
}

fn select_compressor(name: &str) -> Box<dyn Compressor> {
    match name {
        "none" => Box::new(NoCompression),
        "rle"  => Box::new(RunLengthEncoding),
        "zip"  => Box::new(DummyZip),
        _      => Box::new(NoCompression),
    }
}

fn main() {
    // El algoritmo se elige en runtime (podria venir de config, CLI, etc.):
    let algo = "rle";
    let compressor = select_compressor(algo);
    let writer = FileWriter::new(compressor);

    writer.write(b"AAABBBCCCC");
}
```

### Plugin system

```rust
// Registro dinamico de funcionalidad.

trait Plugin {
    fn name(&self) -> &str;
    fn on_start(&self);
    fn on_event(&self, event: &str);
    fn on_stop(&self);
}

struct PluginManager {
    plugins: Vec<Box<dyn Plugin>>,
}

impl PluginManager {
    fn new() -> Self {
        PluginManager { plugins: Vec::new() }
    }

    fn register(&mut self, plugin: Box<dyn Plugin>) {
        println!("Registered plugin: {}", plugin.name());
        self.plugins.push(plugin);
    }

    fn start_all(&self) {
        for plugin in &self.plugins {
            plugin.on_start();
        }
    }

    fn broadcast(&self, event: &str) {
        for plugin in &self.plugins {
            plugin.on_event(event);
        }
    }

    fn stop_all(&self) {
        for plugin in &self.plugins {
            plugin.on_stop();
        }
    }
}

struct LoggerPlugin;
struct MetricsPlugin;

impl Plugin for LoggerPlugin {
    fn name(&self) -> &str { "logger" }
    fn on_start(&self) { println!("[LOG] System started"); }
    fn on_event(&self, event: &str) { println!("[LOG] Event: {}", event); }
    fn on_stop(&self) { println!("[LOG] System stopped"); }
}

impl Plugin for MetricsPlugin {
    fn name(&self) -> &str { "metrics" }
    fn on_start(&self) { println!("[METRICS] Counters initialized"); }
    fn on_event(&self, event: &str) { println!("[METRICS] Recorded: {}", event); }
    fn on_stop(&self) { println!("[METRICS] Flushing counters"); }
}

fn main() {
    let mut manager = PluginManager::new();
    manager.register(Box::new(LoggerPlugin));
    manager.register(Box::new(MetricsPlugin));

    manager.start_all();
    manager.broadcast("user_login");
    manager.broadcast("page_view");
    manager.stop_all();
}
```

### Trait object con closures

```rust
// Las closures implementan Fn/FnMut/FnOnce.
// Se pueden usar como trait objects:

type Callback = Box<dyn Fn(i32) -> i32>;

struct Pipeline {
    steps: Vec<Callback>,
}

impl Pipeline {
    fn new() -> Self {
        Pipeline { steps: Vec::new() }
    }

    fn add_step(&mut self, step: Callback) {
        self.steps.push(step);
    }

    fn execute(&self, input: i32) -> i32 {
        let mut value = input;
        for step in &self.steps {
            value = step(value);
        }
        value
    }
}

fn main() {
    let mut pipeline = Pipeline::new();

    pipeline.add_step(Box::new(|x| x * 2));
    pipeline.add_step(Box::new(|x| x + 10));
    pipeline.add_step(Box::new(|x| x / 3));

    let result = pipeline.execute(5);
    println!("Result: {}", result); // (5 * 2 + 10) / 3 = 6
}
```

---

## Errores comunes

### Error 1: violar object safety sin darse cuenta

```rust
// Un trait que parece inofensivo pero no es object-safe:

trait BadClone {
    fn clone(&self) -> Self;  // retorna Self — NO object-safe
}

// No compila:
// fn clone_it(x: &dyn BadClone) -> ??? {
//     x.clone()
// }

// El error del compilador:
// error[E0038]: the trait `BadClone` cannot be made into an object
//   because method `clone` references the `Self` type in its return type

// Solucion: retornar Box<dyn BadClone>:
trait GoodClone {
    fn clone_boxed(&self) -> Box<dyn GoodClone>;
}
```

### Error 2: confundir impl Trait con dyn Trait

```rust
// impl Trait y dyn Trait NO son intercambiables:

trait Speak {
    fn speak(&self) -> String;
}

struct Dog;
struct Cat;

impl Speak for Dog {
    fn speak(&self) -> String { String::from("Woof") }
}

impl Speak for Cat {
    fn speak(&self) -> String { String::from("Meow") }
}

// impl Trait: el compilador elige UN tipo concreto.
// ESTO NO COMPILA:
// fn choose(flag: bool) -> impl Speak {
//     if flag { Dog } else { Cat }
//     // Error: `if` and `else` have incompatible types
// }

// dyn Trait: permite retornar distintos tipos:
fn choose(flag: bool) -> Box<dyn Speak> {
    if flag { Box::new(Dog) } else { Box::new(Cat) }
}
```

### Error 3: olvidar que dyn Trait es !Sized

```rust
// No se puede tener un valor de tipo dyn Trait directamente:

trait Foo {}
struct Bar;
impl Foo for Bar {}

// NO COMPILA:
// let x: dyn Foo = Bar;
// Error: the size for values of type `dyn Foo` cannot be known
//        at compilation time

// Siempre necesitas un puntero:
let x: Box<dyn Foo> = Box::new(Bar);
let y: &dyn Foo = &Bar;
```

### Error 4: trait no object-safe por metodo generico

```rust
trait Converter {
    fn convert<T: std::fmt::Display>(&self, value: T) -> String {
        format!("{}", value)
    }
}

// NO se puede usar como &dyn Converter.
// El error:
// error[E0038]: the trait `Converter` cannot be made into an object
//   because method `convert` has generic type parameters

// Solucion: eliminar el generico o excluirlo:
trait ConverterFixed {
    fn convert_str(&self, value: &str) -> String;

    fn convert_generic<T: std::fmt::Display>(&self, value: T) -> String
    where
        Self: Sized,  // excluido de la vtable
    {
        format!("{}", value)
    }
}
```

### Error 5: lifetimes implicitos en Box\<dyn Trait\>

```rust
trait Process {
    fn run(&self);
}

struct Ref<'a> {
    data: &'a str,
}

impl<'a> Process for Ref<'a> {
    fn run(&self) {
        println!("{}", self.data);
    }
}

// Box<dyn Process> implica 'static:
// fn make_boxed(s: &str) -> Box<dyn Process> {
//     Box::new(Ref { data: s })
//     // Error: borrowed data escapes outside of function
// }

// Solucion: anotar el lifetime explicitamente:
fn make_boxed<'a>(s: &'a str) -> Box<dyn Process + 'a> {
    Box::new(Ref { data: s })
}

fn main() {
    let text = String::from("hello");
    let boxed = make_boxed(&text);
    boxed.run(); // hello
}
```

### Error 6: intentar clonar Box\<dyn Trait\>

```rust
trait Animal {
    fn speak(&self) -> String;
}

struct Dog;
impl Animal for Dog {
    fn speak(&self) -> String { String::from("Woof") }
}

fn main() {
    let a: Box<dyn Animal> = Box::new(Dog);

    // NO COMPILA:
    // let b = a.clone();
    // Error: the method `clone` exists for struct `Box<dyn Animal>`,
    //        but its trait bounds were not satisfied

    // Box<dyn Animal> no implementa Clone porque dyn Animal no lo hace.
    // Clone::clone retorna Self, que no es object-safe.
}

// Solucion: patron "clone box":
trait CloneableAnimal: Animal {
    fn clone_box(&self) -> Box<dyn CloneableAnimal>;
}

impl<T: Animal + Clone + 'static> CloneableAnimal for T {
    fn clone_box(&self) -> Box<dyn CloneableAnimal> {
        Box::new(self.clone())
    }
}

impl Clone for Box<dyn CloneableAnimal> {
    fn clone(&self) -> Self {
        self.clone_box()
    }
}

// Ahora si se puede clonar:
#[derive(Clone)]
struct Cat;
impl Animal for Cat {
    fn speak(&self) -> String { String::from("Meow") }
}

fn demo() {
    let a: Box<dyn CloneableAnimal> = Box::new(Cat);
    let b = a.clone(); // Funciona
    println!("{}", b.speak());
}
```

### Error 7: trait objects y comparacion con ==

```rust
trait Value {
    fn get(&self) -> i32;
}

struct Num(i32);
impl Value for Num {
    fn get(&self) -> i32 { self.0 }
}

fn main() {
    let a: Box<dyn Value> = Box::new(Num(42));
    let b: Box<dyn Value> = Box::new(Num(42));

    // NO COMPILA:
    // if a == b { ... }
    // dyn Value no implementa PartialEq.

    // Solucion: comparar via los metodos del trait:
    if a.get() == b.get() {
        println!("Equal values");
    }

    // O agregar un metodo de comparacion al trait:
    // fn equals(&self, other: &dyn Value) -> bool;
}
```

---

## Resumen y cheatsheet

```rust
// =====================================================================
// CHEATSHEET: Trait Objects en Rust
// =====================================================================

// --- Sintaxis basica ---
// &dyn Trait          referencia a trait object (borrow)
// &mut dyn Trait      referencia mutable a trait object
// Box<dyn Trait>      trait object owned en el heap
// Rc<dyn Trait>       trait object con reference counting
// Arc<dyn Trait>      trait object con ref counting atomico (thread-safe)

// --- Fat pointer ---
// Un trait object siempre es un fat pointer (16 bytes en 64-bit):
//   [data_ptr | vtable_ptr]
// data_ptr   -> datos del valor concreto
// vtable_ptr -> tabla de punteros a funciones (generada en compilacion)

// --- vtable contiene ---
// - drop function pointer
// - size of concrete type
// - alignment of concrete type
// - one pointer per trait method

// --- Object safety: un trait es object-safe si ---
// 1. No tiene Self: Sized como supertrait
// 2. Todos los metodos:
//    a. Tienen receiver (&self, &mut self, self, Box<Self>, etc.)
//    b. No retornan Self
//    c. No tienen parametros de tipo generico
//    (o el metodo tiene where Self: Sized, excluyendolo de la vtable)

// --- Comparacion rapida ---
//
// fn foo(x: &impl Trait)    → static dispatch (monomorphization)
// fn foo<T: Trait>(x: &T)   → static dispatch (equivalente al anterior)
// fn foo(x: &dyn Trait)     → dynamic dispatch (vtable)
//
// fn bar() -> impl Trait    → static dispatch (UN solo tipo concreto)
// fn bar() -> Box<dyn Trait> → dynamic dispatch (multiples tipos posibles)

// --- Multiples traits ---
// dyn Trait1 + Send + Sync   OK: un trait principal + auto traits
// dyn Trait1 + Trait2         ERROR: no se permiten dos traits no-auto
//   Solucion: crear supertrait: trait Combined: Trait1 + Trait2 {}

// --- Lifetime defaults ---
// &'a dyn Trait           lifetime 'a explicito
// Box<dyn Trait>          equivale a Box<dyn Trait + 'static>
// Box<dyn Trait + 'a>     lifetime 'a (puede contener referencias)

// --- Downcasting ---
// Requiere: trait MyTrait: Any { fn as_any(&self) -> &dyn Any; }
// Uso: obj.as_any().downcast_ref::<ConcreteType>()
// Retorna: Option<&ConcreteType>

// --- Patrones comunes ---
// Vec<Box<dyn Trait>>           coleccion heterogenea
// Box<dyn Fn(Args) -> Ret>      closure como trait object
// struct S { field: Box<dyn T> } polimorfismo en structs
// fn factory() -> Box<dyn T>    factory pattern

// --- Cuando usar cada approach ---
//
// Genericos (impl Trait / T: Trait):
//   - Rendimiento critico
//   - Tipo conocido en compilacion
//   - El trait no es object-safe
//   - Queres inlining
//
// Trait objects (dyn Trait):
//   - Colecciones heterogeneas
//   - Tipo decidido en runtime
//   - Plugin systems
//   - Reducir tamano del binario
//   - El overhead de vtable es aceptable
```

```rust
// =====================================================================
// TABLA DE DECISION RAPIDA
// =====================================================================
//
// Necesitas almacenar distintos tipos en un Vec? → dyn Trait
// El tipo se decide en runtime?                  → dyn Trait
// Rendimiento es critico (hot loop)?             → genericos
// El trait tiene metodos genericos?              → genericos (no object-safe)
// El trait retorna Self?                         → genericos (no object-safe)
// Queres reducir tamano del binario?             → dyn Trait
// Necesitas enviar entre threads?                → dyn Trait + Send
// Tipos conocidos y finitos?                     → considerar enum
```
