# T02 — Generics + trait bounds

---

## 1. El otro lado del polimorfismo

En T01 viste `dyn Trait`: dispatch dinamico, decidido en runtime via
vtable. Ahora vemos la otra cara: **generics con trait bounds**, donde
el compilador genera codigo especializado para cada tipo en compile time.

```
  dyn Trait (T01):          Generics (T02):

  UNA funcion              UNA funcion fuente
  despacha via vtable      el compilador genera N copias
  en runtime               una por tipo concreto

  fn process(s: &dyn Shape)   fn process<T: Shape>(s: &T)
       │                           │
       ▼                           ├─→ process::<Circle>
  vtable lookup                    ├─→ process::<Rect>
       │                           └─→ process::<Triangle>
       ▼                           (compile time, zero-cost)
  Circle::area o
  Rect::area
  (runtime)
```

Generics es el equivalente de los templates de C++ y de las macros
generadoras de C (seccion 9.2 de T02/S01), pero con type safety
completa.

---

## 2. Sintaxis basica

### 2.1 Funcion generica sin bound

Una funcion generica funciona con cualquier tipo, pero no puede hacer
nada con el — no sabe que operaciones tiene:

```rust
fn identity<T>(x: T) -> T {
    x
}

let a = identity(42);        // T = i32
let b = identity("hello");   // T = &str
let c = identity(3.14);      // T = f64
```

Sin trait bound, solo puedes mover, almacenar, o retornar `T`. No
puedes imprimirlo, compararlo, ni llamar metodos.

### 2.2 Trait bound: la restriccion

Un trait bound dice "T debe implementar este trait":

```rust
fn print_area<T: Shape>(s: &T) {
    println!("Area: {:.2}", s.area());
}

// Funciona con cualquier tipo que implemente Shape:
let c = Circle { radius: 5.0 };
let r = Rect { width: 3.0, height: 4.0 };

print_area(&c);  // T = Circle, llama Circle::area directamente
print_area(&r);  // T = Rect, llama Rect::area directamente
```

El bound `T: Shape` garantiza que `s.area()` existe. El compilador
genera dos versiones de `print_area`:

```rust
// El compilador genera (conceptualmente):
fn print_area_circle(s: &Circle) {
    println!("Area: {:.2}", s.area());  // Circle::area, directo
}

fn print_area_rect(s: &Rect) {
    println!("Area: {:.2}", s.area());  // Rect::area, directo
}
```

No hay vtable, no hay indirection. La llamada a `area()` se resuelve
en compile time y puede ser inlineada.

### 2.3 Syntaxis where

Para bounds complejos, la clausula `where` es mas legible:

```rust
// Inline — aceptable para bounds simples
fn process<T: Shape + Clone + Debug>(s: &T) { /* ... */ }

// Where — preferido cuando hay varios parametros o bounds largos
fn process<T, U>(s: &T, logger: &U) -> String
where
    T: Shape + Clone,
    U: Logger + Send,
{
    // ...
}
```

Ambas formas son equivalentes. `where` es obligatoria en algunos
contextos (ej. bounds sobre tipos asociados).

---

## 3. Monomorphization

### 3.1 Que es

Monomorphization es el proceso por el cual el compilador genera una
**copia especializada** de la funcion generica para cada tipo concreto
con el que se usa:

```rust
fn double<T: std::ops::Mul<Output = T> + Copy>(x: T) -> T {
    x * x
}

// En el codigo fuente: UNA funcion
double(3i32);
double(2.5f64);
double(7u8);

// En el binario: TRES funciones
// double::<i32>  — usa instruccion IMUL de enteros
// double::<f64>  — usa instruccion MULSD de punto flotante
// double::<u8>   — usa instruccion MUL de byte
```

### 3.2 Diagrama del proceso

```
  Codigo fuente:

  fn max<T: Ord>(a: T, b: T) -> T {
      if a >= b { a } else { b }
  }

  max(3i32, 5i32);
  max("hello", "world");

                    │
            compilador (monomorphization)
                    │
                    ▼

  Codigo generado:

  fn max_i32(a: i32, b: i32) -> i32 {
      if a >= b { a } else { b }     // comparacion de enteros
  }

  fn max_str(a: &str, b: &str) -> &str {
      if a >= b { a } else { b }     // comparacion lexicografica
  }
```

Cada version es optimizada independientemente. El optimizador puede
inlinear, vectorizar, y eliminar codigo muerto especifico para cada
tipo.

### 3.3 Costo: code bloat

La desventaja: mas tipos = mas copias = binario mas grande.

```
  fn process<T: Shape>(s: &T)

  Usado con 10 tipos → 10 copias de process en el binario
  Usado con 100 tipos → 100 copias

  fn process(s: &dyn Shape)

  Usado con 10 tipos → 1 copia + 10 vtables
  Usado con 100 tipos → 1 copia + 100 vtables
```

Las vtables son pequenas (tabla de punteros). Las copias de funciones
pueden ser grandes. Para funciones pequenas, el compilador puede
inlinear y eliminar la copia. Para funciones grandes, el bloat se
nota.

---

## 4. Multiples bounds

### 4.1 Suma de traits

Un tipo puede necesitar implementar varios traits:

```rust
use std::fmt::{Debug, Display};

fn log_shape<T: Shape + Display + Debug>(s: &T) {
    println!("Debug: {:?}", s);
    println!("Display: {}", s);
    println!("Area: {:.2}", s.area());
}
```

`T` debe implementar los tres traits. Si falta uno, error de
compilacion.

### 4.2 Bounds en multiples parametros

```rust
fn merge_and_log<S, L>(shape: &S, logger: &L)
where
    S: Shape + Clone,
    L: Logger,
{
    let copy = shape.clone();
    logger.log(&format!("Cloned shape, area: {:.2}", copy.area()));
}
```

### 4.3 Lifetime bounds

Generics pueden tener bounds de lifetime:

```rust
fn longest<'a, T: Display>(a: &'a T, b: &'a T) -> &'a T {
    println!("Comparing {} and {}", a, b);
    a  // simplificado
}
```

### 4.4 Comparacion con C y dyn

```
  C:       void process(void *data, size_t size,
                         int (*cmp)(const void*, const void*))
           // El programador "sabe" que data soporta cmp
           // Ningun chequeo

  dyn:     fn process(s: &dyn Shape)
           // Verificado: s implementa Shape
           // Pero: solo UNA interfaz por fat pointer

  Generics: fn process<T: Shape + Clone + Debug>(s: &T)
           // Verificado: T implementa los tres traits
           // Compile time, zero-cost
```

---

## 5. impl Trait

### 5.1 En posicion de argumento

`impl Trait` en argumentos es azucar sintatico para generics:

```rust
// Estas dos son equivalentes:
fn print_area(s: &impl Shape) {
    println!("{:.2}", s.area());
}

fn print_area<T: Shape>(s: &T) {
    println!("{:.2}", s.area());
}
```

`impl Trait` es mas conciso cuando no necesitas nombrar el tipo `T`
(ej. no lo usas en otro parametro ni en el retorno).

### 5.2 En posicion de retorno

`impl Trait` en retorno dice "retorno algun tipo que implementa este
trait, pero no te digo cual":

```rust
fn make_shape(kind: &str) -> impl Shape {
    match kind {
        "circle" => Circle { radius: 5.0 },
        // "rect" => Rect { ... },  // ERROR: no puedes retornar
        //                          // tipos distintos con impl Trait
        _ => Circle { radius: 1.0 },
    }
}
```

Diferencia critica con `dyn Trait` en retorno:

```rust
// impl Trait: UN solo tipo concreto (el compilador lo sabe)
fn make() -> impl Shape {
    Circle { radius: 5.0 }  // siempre Circle
}

// dyn Trait: CUALQUIER tipo (decidido en runtime)
fn make(kind: &str) -> Box<dyn Shape> {
    match kind {
        "circle" => Box::new(Circle { radius: 5.0 }),
        "rect"   => Box::new(Rect { width: 3.0, height: 4.0 }),
        _ => Box::new(Circle { radius: 1.0 }),
    }
}
```

`impl Trait` en retorno es dispatch estatico — el compilador sabe
exactamente que tipo es, pero el llamador no lo ve. Util para ocultar
tipos complejos (ej. iteradores con `.map().filter().take()`).

### 5.3 Tabla de impl Trait

| Posicion | Significado | Equivalente | Dispatch |
|----------|-------------|-------------|----------|
| Argumento `s: &impl Shape` | "cualquier tipo que implemente Shape" | `<T: Shape>(s: &T)` | Estatico |
| Retorno `-> impl Shape` | "un tipo concreto que implementa Shape" | No tiene (opaque type) | Estatico |

---

## 6. Turbofish

Cuando el compilador no puede inferir el tipo generico, usas la
sintaxis turbofish (`::<Type>`):

```rust
let x = "42".parse::<i32>().unwrap();    // T = i32
let y = "3.14".parse::<f64>().unwrap();  // T = f64

let v = Vec::<i32>::new();               // Vec de i32

// Sin turbofish, el compilador infiere si puede:
let x: i32 = "42".parse().unwrap();      // infiere T = i32 del tipo de x
```

El turbofish es necesario cuando hay ambiguedad:

```rust
fn create<T: Default>() -> T {
    T::default()
}

// Sin turbofish: el compilador no sabe que T quieres
// let x = create();  // ERROR: type annotations needed

let x = create::<i32>();     // T = i32
let y = create::<String>();  // T = String
```

---

## 7. Generics en structs e impl

### 7.1 Struct generico

```rust
struct Pair<T> {
    first: T,
    second: T,
}

impl<T> Pair<T> {
    fn new(first: T, second: T) -> Self {
        Pair { first, second }
    }
}

// Metodos que requieren bounds adicionales
impl<T: Display> Pair<T> {
    fn print(&self) {
        println!("({}, {})", self.first, self.second);
    }
}

impl<T: PartialOrd> Pair<T> {
    fn max(&self) -> &T {
        if self.first >= self.second { &self.first }
        else { &self.second }
    }
}

let p = Pair::new(3, 7);
p.print();       // (3, 7) — i32 implementa Display
println!("{}", p.max());  // 7 — i32 implementa PartialOrd
```

### 7.2 Multiples parametros de tipo

```rust
struct KeyValue<K, V> {
    key: K,
    value: V,
}

impl<K: Display, V: Debug> KeyValue<K, V> {
    fn describe(&self) {
        println!("key={}, value={:?}", self.key, self.value);
    }
}
```

### 7.3 Comparacion: generico vs void* en C

```
  C:                              Rust:

  typedef struct {                struct Pair<T> {
      void *first;                    first: T,
      void *second;                   second: T,
      size_t elem_size;           }
  } Pair;
                                  // sizeof conocido en compile time
  // sizeof desconocido           // tipo verificado
  // tipo desconocido             // no hay casts
  // casts manuales
```

El `Pair<T>` de Rust genera `Pair<i32>` (8 bytes), `Pair<f64>` (16
bytes), etc. Cada uno con tamano y alignment correctos, sin `void *`,
sin `elem_size`.

---

## 8. Trait bounds avanzados

### 8.1 Bounds sobre tipos asociados

```rust
// Iterator tiene un tipo asociado Item
fn sum_all<I>(iter: I) -> i32
where
    I: Iterator<Item = i32>,
{
    iter.sum()
}

let total = sum_all(vec![1, 2, 3].into_iter());  // 6
```

### 8.2 HRTB (Higher-Ranked Trait Bounds)

Para bounds que involucran lifetimes:

```rust
fn apply<F>(f: F) -> i32
where
    F: for<'a> Fn(&'a str) -> i32,  // F funciona con cualquier lifetime
{
    let s = String::from("hello");
    f(&s)
}
```

Esto es avanzado y rara vez necesario — lo mencionamos para completitud.

### 8.3 Bound negativo implicito: ?Sized

Por defecto, todo parametro generico tiene bound `Sized` implicito.
Para aceptar tipos sin tamano conocido (slices, `dyn Trait`):

```rust
// Por defecto: T: Sized (implicito)
fn process<T>(s: &T) { /* ... */ }

// Relajar el bound:
fn process<T: ?Sized>(s: &T) { /* ... */ }
// Ahora acepta &str (str es !Sized), &dyn Trait, etc.
```

Esto es por que `&dyn Trait` funciona como argumento a funciones
genericas con `?Sized`.

---

## 9. Generics vs dyn Trait: cuando usar cada uno

### 9.1 Tabla de decision

| Criterio | Generics (`T: Trait`) | dyn Trait |
|----------|----------------------|-----------|
| Tipo conocido en compile time | Si | No necesariamente |
| Coleccion heterogenea | No (Vec\<T\> = un solo tipo) | Si (Vec\<Box\<dyn T\>\>) |
| Performance | Zero-cost (inlining) | Indirection (vtable) |
| Tamano del binario | Crece con cada tipo | Constante |
| Compile time | Crece con cada tipo | Constante |
| Object safety requerida | No | Si |
| Turbofish / inferencia | Si | No aplica |

### 9.2 Regla practica

```
  ¿El tipo concreto se conoce en el punto de uso?
    │
    ├─ SI  → generics
    │       Ejemplo: sort<T: Ord>(arr: &mut [T])
    │
    └─ NO  → dyn Trait
            Ejemplo: plugins, callbacks registrados en runtime

  ¿Necesitas Vec con tipos mezclados?
    │
    ├─ SI  → Vec<Box<dyn Trait>>
    │
    └─ NO  → Vec<T> con generics
             (todos los elementos son del mismo tipo)

  ¿Estas en un hot path?
    │
    ├─ SI  → generics (permite inlining)
    │
    └─ NO  → cualquiera, prefiere legibilidad
```

### 9.3 Ejemplo: mismo problema, dos soluciones

```rust
trait Validator {
    fn validate(&self, input: &str) -> bool;
}

struct LengthValidator { min: usize }
struct RegexValidator { pattern: String }

impl Validator for LengthValidator {
    fn validate(&self, input: &str) -> bool {
        input.len() >= self.min
    }
}

impl Validator for RegexValidator {
    fn validate(&self, input: &str) -> bool {
        input.contains(&self.pattern)  // simplificado
    }
}
```

```rust
// Solucion A: generics — UN tipo de validador por vez
fn validate_all_generic<V: Validator>(inputs: &[&str], v: &V) -> Vec<bool> {
    inputs.iter().map(|s| v.validate(s)).collect()
}

// Uso: todos los inputs usan el MISMO validador
let results = validate_all_generic(
    &["hi", "hello", "hey"],
    &LengthValidator { min: 3 },
);
```

```rust
// Solucion B: dyn Trait — MULTIPLES validadores
fn validate_pipeline(input: &str, validators: &[&dyn Validator]) -> bool {
    validators.iter().all(|v| v.validate(input))
}

// Uso: el input pasa por VARIOS validadores de tipos distintos
let len_v = LengthValidator { min: 3 };
let regex_v = RegexValidator { pattern: "@".into() };

let ok = validate_pipeline("user@mail", &[&len_v, &regex_v]);
```

Solucion A: rapida, pero todos los inputs usan el mismo validador.
Solucion B: flexible, multiples validadores mezclados.

---

## 10. Generics en la standard library

La stdlib de Rust usa generics extensivamente. Reconocer los patrones
ayuda a leerlos:

### 10.1 Option y Result

```rust
enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

### 10.2 Vec e Iterator

```rust
struct Vec<T> { /* ... */ }

trait Iterator {
    type Item;  // tipo asociado — equivalente a parametro generico
    fn next(&mut self) -> Option<Self::Item>;
}
```

### 10.3 HashMap

```rust
struct HashMap<K, V> { /* ... */ }

impl<K: Eq + Hash, V> HashMap<K, V> {
    fn insert(&mut self, key: K, value: V) -> Option<V>;
    fn get(&self, key: &K) -> Option<&V>;
}
```

`K: Eq + Hash` es un trait bound: las keys deben ser comparables y
hasheables.

### 10.4 Patron comun: From/Into

```rust
trait From<T> {
    fn from(value: T) -> Self;
}

// Si implementas From<T> para U, obtienes Into<U> para T gratis:
let s: String = String::from("hello");
let s: String = "hello".into();  // usa Into, derivado de From
```

---

## Errores comunes

### Error 1 — Bound faltante

```rust
fn print_it<T>(x: T) {
    println!("{}", x);
//  ^^^ error: `T` doesn't implement `std::fmt::Display`
}
```

Fix: agregar el bound.

```rust
fn print_it<T: std::fmt::Display>(x: T) {
    println!("{}", x);
}
```

El compilador te dice exactamente que trait falta. En C, un `void *`
no te dice nada — simplemente crashea.

### Error 2 — Retornar tipos distintos con impl Trait

```rust
fn make(circle: bool) -> impl Shape {
    if circle {
        Circle { radius: 5.0 }
    } else {
        Rect { width: 3.0, height: 4.0 }
//      ^^^ error: expected Circle, found Rect
    }
}
```

`impl Trait` en retorno debe ser **un solo tipo**. Para tipos distintos,
usa `Box<dyn Trait>` o un enum.

### Error 3 — Mover un valor con bound generico

```rust
fn consume<T: Shape>(s: T) {
    println!("{:.2}", s.area());
    // s se mueve (drop) al final
}

let c = Circle { radius: 5.0 };
consume(c);
// c ya no existe — fue movido a consume
// println!("{}", c.radius);  // ERROR: use of moved value
```

Si necesitas preservar el valor, usa referencia:

```rust
fn inspect<T: Shape>(s: &T) {
    println!("{:.2}", s.area());
}
```

### Error 4 — Conflicto de bounds en impl blocks

```rust
struct Wrapper<T>(T);

// Este impl requiere Display
impl<T: Display> Wrapper<T> {
    fn show(&self) { println!("{}", self.0); }
}

// Este impl requiere Debug
impl<T: Debug> Wrapper<T> {
    fn debug(&self) { println!("{:?}", self.0); }
}

// Para un tipo que implementa AMBOS, los dos estan disponibles
// Para un tipo que implementa solo uno, solo ese bloque esta disponible
```

Esto no es un error per se, pero puede confundir: `show()` y `debug()`
existen en bloques separados con bounds distintos.

---

## Ejercicios

### Ejercicio 1 — Funcion generica basica

Escribe una funcion `max_of_three<T: PartialOrd>(a: T, b: T, c: T) -> T`
que retorne el mayor de tres valores. Pruebala con `i32`, `f64`, y
`char`.

**Prediccion**: Puedes llamarla con `String`? Y con un tipo custom
que no implementa `PartialOrd`?

<details><summary>Respuesta</summary>

```rust
fn max_of_three<T: PartialOrd>(a: T, b: T, c: T) -> T {
    let mut max = a;
    if b > max { max = b; }
    if c > max { max = c; }
    max
}

fn main() {
    println!("{}", max_of_three(3, 7, 5));           // 7
    println!("{}", max_of_three(1.1, 3.3, 2.2));     // 3.3
    println!("{}", max_of_three('a', 'z', 'm'));      // z
    println!("{}", max_of_three(
        "apple".to_string(),
        "zebra".to_string(),
        "mango".to_string()
    ));  // zebra — String implementa PartialOrd (lexicografico)
}
```

Si, funciona con `String` (implementa `PartialOrd`). No funciona con
un tipo custom que no lo implementa:

```rust
struct Point { x: f64, y: f64 }
// max_of_three(p1, p2, p3);
// ERROR: Point doesn't implement PartialOrd
```

El compilador te dice exactamente que falta. En C, pasarias
`void *` + comparador y si olvidas el comparador: UB.

</details>

---

### Ejercicio 2 — Struct generico con bounds

Crea un struct `SortedPair<T: Ord>` que almacene dos valores siempre
en orden (el menor primero). El constructor debe ordenarlos.

```rust
struct SortedPair<T: Ord> { /* ... */ }

impl<T: Ord> SortedPair<T> {
    fn new(a: T, b: T) -> Self { todo!() }
    fn min(&self) -> &T { todo!() }
    fn max(&self) -> &T { todo!() }
}
```

**Prediccion**: Puedes crear un `SortedPair<f64>`? (`f64` implementa
`PartialOrd` pero no `Ord` — por que?)

<details><summary>Respuesta</summary>

```rust
struct SortedPair<T: Ord> {
    lo: T,
    hi: T,
}

impl<T: Ord> SortedPair<T> {
    fn new(a: T, b: T) -> Self {
        if a <= b {
            SortedPair { lo: a, hi: b }
        } else {
            SortedPair { lo: b, hi: a }
        }
    }

    fn min(&self) -> &T { &self.lo }
    fn max(&self) -> &T { &self.hi }
}

fn main() {
    let p = SortedPair::new(7, 3);
    println!("min={}, max={}", p.min(), p.max());  // min=3, max=7

    let p = SortedPair::new("zebra", "apple");
    println!("min={}, max={}", p.min(), p.max());  // min=apple, max=zebra
}
```

`SortedPair<f64>` **no compila**. `f64` no implementa `Ord` porque
`NaN != NaN` — la comparacion no es total. `NaN` no es ni mayor, ni
menor, ni igual a nada (incluyendo a si mismo).

```rust
// SortedPair::new(1.0, 2.0);
// ERROR: f64 doesn't implement Ord

// Para f64, podrias usar PartialOrd con unwrap:
struct SortedPairPartial<T: PartialOrd> { lo: T, hi: T }
// Pero: partial_cmp retorna Option, y con NaN obtienes None
```

Esta es una diferencia que C ignora completamente — en C, comparar
NaN es UB silencioso.

</details>

---

### Ejercicio 3 — impl Trait en retorno

Escribe una funcion `fibonacci() -> impl Iterator<Item = u64>` que
retorne un iterador infinito de Fibonacci.

**Prediccion**: El tipo concreto del iterador es complejo (closure
state machine). Sin `impl Iterator`, como lo expresarias?

<details><summary>Respuesta</summary>

```rust
fn fibonacci() -> impl Iterator<Item = u64> {
    let mut a = 0u64;
    let mut b = 1u64;

    std::iter::from_fn(move || {
        let current = a;
        let next = a.checked_add(b)?;  // None en overflow
        a = b;
        b = next;
        Some(current)
    })
}

fn main() {
    let fibs: Vec<u64> = fibonacci().take(10).collect();
    println!("{:?}", fibs);
    // [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]

    let sum: u64 = fibonacci().take(20).sum();
    println!("Sum of first 20: {}", sum);  // 10945
}
```

Sin `impl Iterator`, el tipo seria algo como:

```rust
std::iter::FromFn<[closure@src/main.rs:2:24: 2:31]>
```

Este tipo es **inexpresable** — contiene un tipo de closure anonimo
generado por el compilador. `impl Iterator` te permite retornarlo sin
nombrarlo. Es el caso de uso principal de `impl Trait` en retorno:
ocultar tipos complejos que no tienen nombre util.

La alternativa seria `Box<dyn Iterator<Item = u64>>`, pero eso agrega
una allocation y un indirection innecesarios.

</details>

---

### Ejercicio 4 — Multiples bounds

Escribe una funcion `summarize` que reciba una slice de elementos y
retorne un String con el formato: `"[a, b, c] (n items, min=X, max=Y)"`.

Determina que bounds necesita `T`.

```rust
fn summarize<T: ???>(items: &[T]) -> String { todo!() }
```

**Prediccion**: Necesitas Display, Ord, y algo mas?

<details><summary>Respuesta</summary>

```rust
use std::fmt::Display;

fn summarize<T: Display + Ord>(items: &[T]) -> String {
    if items.is_empty() {
        return "[] (0 items)".to_string();
    }

    let joined: Vec<String> = items.iter().map(|x| x.to_string()).collect();
    let list = joined.join(", ");

    let min = items.iter().min().unwrap();
    let max = items.iter().max().unwrap();

    format!("[{}] ({} items, min={}, max={})",
            list, items.len(), min, max)
}

fn main() {
    println!("{}", summarize(&[3, 1, 4, 1, 5]));
    // [3, 1, 4, 1, 5] (5 items, min=1, max=5)

    println!("{}", summarize(&["banana", "apple", "cherry"]));
    // [banana, apple, cherry] (3 items, min=apple, max=cherry)

    println!("{}", summarize::<i32>(&[]));
    // [] (0 items)
}
```

Bounds necesarios:
- `Display` — para convertir cada elemento a String
- `Ord` — para `min()` y `max()`

No necesitas `Clone` ni `Copy` porque solo tomamos referencias (`&T`).
Los metodos de `Iterator` como `min()` y `max()` trabajan con
referencias cuando iteras sobre `&[T]`.

</details>

---

### Ejercicio 5 — Generics vs dyn: traducir en ambas direcciones

Dado este codigo con `dyn Trait`:

```rust
fn total_area(shapes: &[Box<dyn Shape>]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

Reescribelo con generics. Luego explica por que la version generica
**no** puede reemplazar a la `dyn` version en todos los casos.

**Prediccion**: Con generics, puedes meter un Circle y un Rect en la
misma slice?

<details><summary>Respuesta</summary>

Version con generics:

```rust
fn total_area_generic<T: Shape>(shapes: &[T]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

Uso:

```rust
let circles = vec![
    Circle { radius: 5.0 },
    Circle { radius: 3.0 },
];
let total = total_area_generic(&circles);  // OK: todos son Circle

let rects = vec![
    Rect { width: 3.0, height: 4.0 },
];
let total = total_area_generic(&rects);  // OK: todos son Rect
```

Pero:

```rust
// ESTO NO COMPILA con generics:
// let mixed = vec![circle, rect];  // ERROR: tipos distintos

// Con dyn Trait, SI compila:
let mixed: Vec<Box<dyn Shape>> = vec![
    Box::new(Circle { radius: 5.0 }),
    Box::new(Rect { width: 3.0, height: 4.0 }),
];
let total = total_area(&mixed);  // OK: heterogeneo
```

La version generica requiere que **todos** los elementos sean del mismo
tipo `T`. La version `dyn` permite mezclar tipos. Ambas son validas —
la eleccion depende de si necesitas heterogeneidad.

</details>

---

### Ejercicio 6 — Monomorphization: predecir copias

Dado este codigo, cuantas versiones de cada funcion genera el compilador?

```rust
fn process<T: Debug>(x: &T) {
    println!("{:?}", x);
}

fn transform<T: Clone, U: Display>(a: T, b: U) -> T {
    println!("{}", b);
    a.clone()
}

fn main() {
    process(&42i32);
    process(&"hello");
    process(&3.14f64);
    process(&42i32);       // repetido con el mismo tipo

    transform(1i32, "a");
    transform(1i32, 2u8);
    transform("x", "y");
}
```

**Prediccion**: La segunda llamada a `process(&42i32)` genera una copia
adicional?

<details><summary>Respuesta</summary>

`process`:
- `process::<i32>` — para `&42i32` (primera y cuarta llamada)
- `process::<&str>` — para `&"hello"`
- `process::<f64>` — para `&3.14f64`

**3 versiones.** La segunda llamada con `i32` reutiliza la version ya
generada. Monomorphization genera una copia por **tipo unico**, no por
**llamada**.

`transform`:
- `transform::<i32, &str>` — para `(1i32, "a")`
- `transform::<i32, u8>` — para `(1i32, 2u8)`
- `transform::<&str, &str>` — para `("x", "y")`

**3 versiones.** Cada combinacion unica de `(T, U)` genera una copia.

Total en el binario: **6 funciones** generadas a partir de 2 funciones
fuente.

</details>

---

### Ejercicio 7 — Comparar con C: swap generico

En S01/T02 implementaste `generic_swap` en C:

```c
void generic_swap(void *a, void *b, size_t size) {
    unsigned char tmp[size];
    memcpy(tmp, a, size);
    memcpy(a, b, size);
    memcpy(b, tmp, size);
}
```

Implementa el equivalente en Rust con generics. Compara linea por linea.

**Prediccion**: La version Rust necesita `size`? Necesita `unsafe`?

<details><summary>Respuesta</summary>

```rust
fn generic_swap<T>(a: &mut T, b: &mut T) {
    std::mem::swap(a, b);
}

// O manualmente:
fn generic_swap_manual<T>(a: &mut T, b: &mut T) {
    unsafe {
        let mut tmp: T = std::ptr::read(a);
        std::ptr::copy_nonoverlapping(b, a, 1);
        std::ptr::write(b, tmp);
    }
}

// Uso:
let mut x = 10;
let mut y = 20;
generic_swap(&mut x, &mut y);
// x == 20, y == 10
```

Comparacion linea por linea:

```
  C:                                  Rust:
  ──                                  ────
  void generic_swap(                  fn generic_swap<T>(
      void *a,                            a: &mut T,
      void *b,                            b: &mut T,
      size_t size)                    ) {                   ← sin size
  {
      unsigned char tmp[size];            std::mem::swap(a, b);
      memcpy(tmp, a, size);           }
      memcpy(a, b, size);
      memcpy(b, tmp, size);
  }

  Parametros: 3                       Parametros: 2
  Type safety: ninguna                Type safety: completa
  Errores posibles:                   Errores posibles:
    - size incorrecto                   (ninguno en compile time)
    - tipos distintos
    - buffer overflow
```

La version Rust:
- **No necesita `size`** — el compilador sabe `size_of::<T>()`
- **No necesita `unsafe`** — `std::mem::swap` es safe
- **No puede mezclar tipos** — `swap(&mut i32, &mut f64)` no compila
- Monomorphization genera una version optimizada por tipo (puede usar
  registros en vez de memcpy para tipos pequenos)

La stdlib ya provee `std::mem::swap`. En C no hay equivalente generico
en la stdlib — cada proyecto reimplementa su propio swap.

</details>

---

### Ejercicio 8 — Bound detective

Para cada funcion de la stdlib, indica que bounds tienen sus parametros
genericos (sin mirar la documentacion):

```rust
// A — ¿que bounds necesita T?
fn Vec::contains(&self, x: &T) -> bool

// B — ¿que bounds necesitan K y V?
fn HashMap::insert(&mut self, key: K, value: V) -> Option<V>

// C — ¿que bounds necesita T?
fn Vec::sort(&mut self)

// D — ¿que bounds necesita T?
fn Vec::binary_search(&self, x: &T) -> Result<usize, usize>
```

**Prediccion**: Para cada uno, piensa que operacion necesita hacer sobre
T y de ahi deduce el bound.

<details><summary>Respuesta</summary>

| Funcion | Bound | Razon |
|---------|-------|-------|
| A `contains` | `T: PartialEq` | Necesita comparar `x == element` |
| B `insert` | `K: Eq + Hash`, V: (ninguno) | Key necesita hash y comparacion; Value puede ser cualquier cosa |
| C `sort` | `T: Ord` | Necesita orden total para comparar elementos |
| D `binary_search` | `T: Ord` | Necesita orden total para decidir izquierda/derecha |

Notas:
- `sort` usa `Ord` (orden total), no `PartialOrd`. Existe `sort_by`
  que acepta un closure comparador para tipos sin `Ord` (como `f64`).
- `HashMap` necesita `Eq` (no `PartialEq`) porque la igualdad debe
  ser reflexiva (`x == x` siempre true). `NaN != NaN` rompe esto,
  por eso `f64` no puede ser key de HashMap.
- `binary_search` asume que la slice esta ordenada. Si no lo esta, el
  resultado es indeterminado (pero no UB — Rust no tiene UB en safe).

En C, `qsort` y `bsearch` reciben un comparador como function pointer.
En Rust, el bound esta en el tipo system — si `T: Ord`, no necesitas
pasar un comparador.

</details>

---

### Ejercicio 9 — Encontrar errores

Cada fragmento tiene un error. Identificalo:

```rust
// Error A
fn biggest<T>(list: &[T]) -> &T {
    let mut max = &list[0];
    for item in list {
        if item > max { max = item; }
    }
    max
}

// Error B
fn make_pair<T>(a: T, b: T) -> (T, T) {
    println!("Creating pair: {} and {}", a, b);
    (a, b)
}

// Error C
fn first_or_default<T>(items: &[T]) -> T {
    if items.is_empty() {
        T::default()
    } else {
        items[0]
    }
}
```

**Prediccion**: En cada caso, que bound falta?

<details><summary>Respuesta</summary>

**Error A: Falta `T: PartialOrd`.**

```rust
// item > max requiere comparacion
fn biggest<T: PartialOrd>(list: &[T]) -> &T { /* ... */ }
```

**Error B: Falta `T: Display`.**

```rust
// println!("{}", a) requiere Display
fn make_pair<T: Display>(a: T, b: T) -> (T, T) { /* ... */ }
```

**Error C: Faltan `T: Default` y `T: Copy` (o `Clone`).**

```rust
// T::default() requiere Default
// items[0] sin & mueve el valor fuera de la slice — requiere Copy
fn first_or_default<T: Default + Copy>(items: &[T]) -> T {
    if items.is_empty() {
        T::default()
    } else {
        items[0]  // Copy: copia el valor
    }
}

// Alternativa sin Copy: retornar referencia
fn first_or_default_ref<T>(items: &[T]) -> Option<&T> {
    items.first()
}
```

El compilador de Rust te dice exactamente que bound falta y te sugiere
agregarlo. En C con `void *`, no hay compilador que te avise — solo
crash en runtime.

</details>

---

### Ejercicio 10 — Reflexion: monomorphization vs type erasure

Responde sin ver la respuesta:

1. Monomorphization genera N copias de una funcion. Type erasure
   (dyn Trait) genera una. Si una funcion generica se usa con 50 tipos,
   cuantas veces mas codigo genera monomorphization? Esto siempre
   importa?

2. C usa type erasure (void*) por defecto, Rust usa monomorphization
   por defecto. Cual es mas seguro? Cual es mas rapido? Cual produce
   binarios mas pequenos?

3. Podrias tener lo mejor de ambos mundos? Alguna tecnica combina
   la safety de generics con el tamano de binario de dyn?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. 50x mas codigo?**

En teoria, 50 tipos generan 50 copias. En la practica:

- El compilador elimina copias identicas (si dos tipos generan el
  mismo assembly, se deduplican).
- El linker elimina funciones no usadas (dead code elimination).
- Las funciones inlineadas desaparecen como funciones independientes.

El impacto real depende del tamano de la funcion:
- Funcion de 5 lineas × 50 tipos: ~250 lineas de assembly, probable
  inlining, impacto minimo.
- Funcion de 500 lineas × 50 tipos: ~25000 lineas, impacto real en
  tamano de binario y icache pressure.

Para la mayoria del codigo, no importa. Para binarios embedded o WASM,
puede importar mucho.

**2. C void* vs Rust generics:**

| Aspecto | C void* (type erasure) | Rust generics (monomorphization) |
|---------|----------------------|--------------------------------|
| Safety | Ninguna | Completa |
| Velocidad | Indirecciones | Zero-cost (inlining) |
| Binario | Compacto | Puede crecer |

Rust es mas seguro **y** mas rapido, a costa de binarios potencialmente
mas grandes. C es compacto pero peligroso.

**3. Lo mejor de ambos mundos:**

Si. La tecnica se llama **outline generics** o **thin wrapper**:

```rust
// La logica pesada usa dyn (una sola copia):
fn process_inner(s: &dyn Shape, config: &Config) -> f64 {
    // 500 lineas de logica compleja
    // Una sola copia en el binario
    s.area() * config.factor
}

// El wrapper generico es minimo (inline):
fn process<T: Shape>(s: &T, config: &Config) -> f64 {
    process_inner(s as &dyn Shape, config)
}
```

El wrapper generico solo convierte `&T` a `&dyn Shape` (una operacion
trivial). La logica real esta en la version `dyn`, que existe una sola
vez. El usuario tiene la ergonomia de generics, pero el binario tiene
el tamano de `dyn`.

La stdlib de Rust usa esta tecnica internamente (ej. `format!` delega a
funciones `dyn fmt::Write`).

</details>
