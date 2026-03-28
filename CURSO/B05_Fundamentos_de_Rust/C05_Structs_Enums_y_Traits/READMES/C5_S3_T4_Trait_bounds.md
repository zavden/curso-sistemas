# T04 — Trait bounds

> **Bloque**: 05 — Fundamentos de Rust
> **Capitulo**: 05 — Structs, Enums y Traits
> **Seccion**: 03 — Traits
> **Tema**: 04 — Trait bounds: fn foo<T: Trait>, where clauses, multiples bounds (+)

---

## Tabla de contenidos

1. [Que son los trait bounds](#que-son-los-trait-bounds)
2. [Sintaxis inline: fn foo\<T: Trait\>](#sintaxis-inline)
3. [impl Trait en posicion de parametro](#impl-trait-en-posicion-de-parametro)
4. [Multiples bounds con +](#multiples-bounds-con-)
5. [Clausulas where](#clausulas-where)
6. [Bounds en impl blocks](#bounds-en-impl-blocks)
7. [Implementaciones condicionales](#implementaciones-condicionales)
8. [Bounds en tipos de retorno: -> impl Trait](#bounds-en-tipos-de-retorno)
9. [Supertraits: trait Foo: Bar](#supertraits)
10. [Bounds con lifetimes](#bounds-con-lifetimes)
11. [El bound Sized y ?Sized](#el-bound-sized-y-sized)
12. [Trait bounds vs trait objects](#trait-bounds-vs-trait-objects)
13. [Patrones comunes de la stdlib](#patrones-comunes-de-la-stdlib)
14. [Bounds combinados — ejemplo integrador](#bounds-combinados--ejemplo-integrador)
15. [Errores comunes](#errores-comunes)
16. [Resumen y cheatsheet](#resumen-y-cheatsheet)
17. [Ejercicios](#ejercicios)

---

## Que son los trait bounds

Un trait bound es una restriccion sobre un tipo generico. Dice:
"este tipo generico T debe implementar tal trait". Sin bounds,
un tipo generico no puede hacer nada util porque el compilador
no sabe que operaciones soporta.

```rust
// Sin bound — no compila:
fn print<T>(item: &T) {
    println!("{}", item);
//  ^^^^^^^^^^^^^^ error: T doesn't implement Display
}

// Con bound — compila:
fn print<T: std::fmt::Display>(item: &T) {
    println!("{}", item);
// Ahora el compilador sabe que T implementa Display,
// asi que puede usar {} en println!.
}
```

```rust
// La sintaxis basica de un trait bound:
//
//   fn nombre<T: Trait>(parametro: &T)
//             ^^^^^^^^
//             |
//             bound: T debe implementar Trait
//
// Esto es un contrato:
// - El llamador debe pasar un tipo que implemente Trait.
// - La funcion puede usar cualquier metodo de Trait sobre T.
```

```rust
// Un tipo generico sin bounds solo puede ser movido, dropeado
// o pasado a otra funcion generica. No puedes hacer nada mas:

fn useless<T>(item: T) {
    // No puedes imprimir item — no sabes si T implementa Display.
    // No puedes clonar item — no sabes si T implementa Clone.
    // No puedes comparar item — no sabes si T implementa PartialEq.
    // Solo puedes:
    drop(item);  // siempre valido (Drop es implicito)
}

// Los bounds son la forma de decirle al compilador
// que capacidades tiene T, para poder usarlas.
```

---

## Sintaxis inline

La forma clasica de escribir un trait bound es directamente
en la declaracion del parametro generico, despues de dos puntos:

```rust
use std::fmt::Display;

// Un solo bound:
fn print_item<T: Display>(item: &T) {
    println!("{}", item);
}

// Multiples parametros genericos, cada uno con su bound:
fn print_pair<T: Display, U: Display>(first: &T, second: &U) {
    println!("{} and {}", first, second);
}

fn main() {
    print_item(&42);
    print_item(&"hello");
    print_pair(&42, &"world");  // T = i32, U = &str
}
```

```rust
// Los bounds inline son buenos para:
// - Funciones con 1-2 genericos.
// - Bounds simples (un solo trait).
// - Codigo donde la firma es corta.

// Ejemplo: funcion que encuentra el maximo en un slice.
fn largest<T: PartialOrd>(list: &[T]) -> &T {
    let mut max = &list[0];
    for item in &list[1..] {
        if item > max {
            max = item;
        }
    }
    max
}

fn main() {
    let numbers = vec![34, 50, 25, 100, 65];
    println!("Largest: {}", largest(&numbers));  // 100

    let chars = vec!['y', 'm', 'a', 'q'];
    println!("Largest: {}", largest(&chars));    // y
}
```

```rust
// Ventaja clave de la forma explicita sobre impl Trait:
// puedes nombrar el tipo generico y usarlo en multiples posiciones.

// Ambos parametros DEBEN ser el mismo tipo:
fn print_same<T: Display>(a: &T, b: &T) {
    println!("{} = {}", a, b);
}

fn main() {
    print_same(&42, &99);       // ok: ambos i32
    // print_same(&42, &"hello");  // error: i32 != &str
}
```

```rust
// Turbofish: especificar el tipo generico explicitamente.
// Solo funciona con la forma explicita, no con impl Trait.

fn parse_number<T: std::str::FromStr>(s: &str) -> Option<T> {
    s.parse::<T>().ok()
}

fn main() {
    // Inferencia por el tipo de la variable:
    let n: Option<i32> = parse_number("42");

    // Turbofish — especifica T directamente en la llamada:
    let f = parse_number::<f64>("3.14");
    println!("{:?} {:?}", n, f);  // Some(42) Some(3.14)
}
```

```rust
// Multiples genericos con bounds distintos:
fn convert_and_print<T: std::str::FromStr, U: Display>(input: &str, label: U)
where
    T: Display,
    <T as std::str::FromStr>::Err: std::fmt::Debug,
{
    match input.parse::<T>() {
        Ok(val) => println!("{}: {}", label, val),
        Err(e) => println!("{}: parse error: {:?}", label, e),
    }
}
```

---

## impl Trait en posicion de parametro

La forma mas simple de escribir un trait bound es `impl Trait`
en la posicion del tipo del parametro. Es azucar sintactico:

```rust
use std::fmt::Display;

// impl Trait en posicion de parametro:
fn print_item(item: &impl Display) {
    println!("{}", item);
}

// Uso:
fn main() {
    print_item(&42);
    print_item(&"hello");
    print_item(&3.14);
}
```

```rust
// impl Trait es azucar sintactico para un generico con bound.
// Estas dos firmas son equivalentes:

fn print_a(item: &impl Display) {
    println!("{}", item);
}

fn print_b<T: Display>(item: &T) {
    println!("{}", item);
}

// Ambas generan el mismo codigo. El compilador crea una version
// especializada (monomorfizada) para cada tipo concreto que se use.
```

```rust
// Cada parametro con impl Trait es un tipo generico INDEPENDIENTE:

fn print_two(a: &impl Display, b: &impl Display) {
    println!("{} {}", a, b);
}

// Equivale a:
fn print_two_explicit<T: Display, U: Display>(a: &T, b: &U) {
    println!("{} {}", a, b);
}

// a y b pueden ser tipos DISTINTOS:
fn main() {
    print_two(&42, &"hello");  // T = i32, U = &str — ok
    print_two(&1, &2);         // T = i32, U = i32 — ok
}
```

```rust
// LIMITACION: con impl Trait no puedes forzar que dos
// parametros sean del mismo tipo. Para eso, usa la forma explicita.

// Esto NO garantiza que a y b sean el mismo tipo:
fn compare_bad(a: &impl PartialOrd, b: &impl PartialOrd) -> bool {
    // a.partial_cmp(b)  // error: tipos potencialmente distintos
    false
}

// Esto SI garantiza que sean el mismo tipo:
fn compare_good<T: PartialOrd>(a: &T, b: &T) -> bool {
    a > b
}
```

```rust
// Cuadro comparativo:
//
// | Caracteristica               | impl Trait | T: Trait   |
// |------------------------------|-----------|------------|
// | Legibilidad                  | Alta      | Media      |
// | Forzar mismo tipo            | No        | Si         |
// | Turbofish                    | No        | Si         |
// | Multiples bounds             | Si (con +)| Si (con +) |
// | where clause                 | No        | Si         |
// | Genera el mismo codigo       | Si        | Si         |
```

---

## Multiples bounds con +

Cuando un tipo generico debe implementar mas de un trait,
se combinan con `+`:

```rust
use std::fmt::{Display, Debug};

// T debe implementar Display Y Debug:
fn inspect<T: Display + Debug>(item: &T) {
    println!("Display: {}", item);
    println!("Debug:   {:?}", item);
}

// Se pueden combinar cuantos traits se necesiten:
fn process<T: Display + Debug + Clone>(item: &T) {
    let copy = item.clone();    // Clone
    println!("{}", item);       // Display
    println!("{:?}", copy);     // Debug
}

fn main() {
    inspect(&42);
    // Display: 42
    // Debug:   42

    process(&"hello");
    // hello
    // "hello"
}
```

```rust
// + tambien funciona con impl Trait:
fn inspect(item: &(impl Display + Debug)) {
    println!("Display: {}", item);
    println!("Debug:   {:?}", item);
}

// Nota los parentesis alrededor de impl Display + Debug.
// Son necesarios cuando se usa como referencia:
//   &(impl Display + Debug)
// Sin parentesis:
//   &impl Display + Debug   // error de parseo
```

```rust
// Ejemplo practico: funcion que ordena y muestra resultados.
fn sort_and_display<T: PartialOrd + Display>(list: &mut Vec<T>) {
    list.sort_by(|a, b| a.partial_cmp(b).unwrap());
    for item in list {
        println!("{}", item);
    }
}

fn main() {
    let mut nums = vec![5, 2, 8, 1, 9];
    sort_and_display(&mut nums);
    // 1
    // 2
    // 5
    // 8
    // 9
}
```

```rust
// Bounds con traits de la stdlib que se usan juntos frecuentemente:

// Eq + Hash — para claves de HashMap:
use std::collections::HashMap;
use std::hash::Hash;

fn count_items<T: Eq + Hash>(items: &[T]) -> HashMap<&T, usize> {
    let mut map = HashMap::new();
    for item in items {
        *map.entry(item).or_insert(0) += 1;
    }
    map
}

// PartialOrd + PartialEq — para ordenar y comparar:
fn clamp<T: PartialOrd>(value: T, min: T, max: T) -> T {
    if value < min {
        min
    } else if value > max {
        max
    } else {
        value
    }
}

// Send + Sync + 'static — para datos compartidos entre threads:
fn spawn_with<T: Send + Sync + 'static>(data: T) {
    std::thread::spawn(move || {
        drop(data);
    });
}
```

```rust
// No hay limite en la cantidad de bounds:
fn extreme<T: Display + Debug + Clone + PartialOrd + PartialEq + Hash + Send + Sync>(
    item: &T,
) {
    println!("{}", item);
}

// Pero si hay tantos bounds, es senal de que:
// 1. La funcion hace demasiado.
// 2. Deberias usar un supertrait que agrupe los bounds.
// 3. O al menos usar where para legibilidad.
```

---

## Clausulas where

Cuando los bounds se vuelven largos, la firma de la funcion se
vuelve ilegible. Las clausulas `where` mueven los bounds despues
de la firma:

```rust
use std::fmt::{Display, Debug};

// Sin where — dificil de leer:
fn process<T: Display + Clone + Debug, U: Debug + PartialEq>(t: &T, u: &U) -> String {
    format!("{}: {:?}", t, u)
}

// Con where — mucho mas claro:
fn process_clean<T, U>(t: &T, u: &U) -> String
where
    T: Display + Clone + Debug,
    U: Debug + PartialEq,
{
    format!("{}: {:?}", t, u)
}

// La firma muestra los parametros limpiamente.
// Los bounds van debajo, organizados uno por linea.
```

```rust
// Las clausulas where son equivalentes a los bounds inline.
// Estas tres formas son identicas:

// Forma 1: bound inline
fn foo1<T: Display>(item: &T) {}

// Forma 2: where
fn foo2<T>(item: &T)
where
    T: Display,
{}

// Forma 3: impl Trait
fn foo3(item: &impl Display) {}

// Las tres generan el mismo codigo compilado.
```

```rust
// Cuadro de decision — cuando usar where:
//
// Usa bounds inline cuando:
//   - Solo hay 1 generico con 1-2 bounds.
//   - La firma cabe en una linea.
//
// Usa where cuando:
//   - Hay multiples genericos.
//   - Un generico tiene 3+ bounds.
//   - Los bounds involucran tipos asociados.
//   - La firma ya es compleja (lifetimes, muchos parametros).
//
// Usa impl Trait cuando:
//   - Solo necesitas un parametro simple.
//   - No necesitas nombrar el tipo generico.
//   - No necesitas turbofish.
```

```rust
// where permite bounds sobre tipos asociados que no se pueden
// expresar facilmente inline:

use std::iter::Iterator;

fn sum_items<I>(iter: I) -> i32
where
    I: Iterator<Item = i32>,
{
    iter.sum()
}

// El bound I: Iterator<Item = i32> especifica que el tipo
// asociado Item del Iterator debe ser i32.
```

```rust
// where con tipos complejos:

fn apply_to_pair<F, T, R>(f: F, a: T, b: T) -> (R, R)
where
    F: Fn(T) -> R,
    T: Clone,
{
    let r1 = f(a);
    let r2 = f(b);
    (r1, r2)
}

fn main() {
    let result = apply_to_pair(|x: i32| x * 2, 3, 7);
    println!("{:?}", result);  // (6, 14)
}
```

```rust
// where en metodos de impl:
struct Processor<T> {
    data: T,
}

impl<T> Processor<T>
where
    T: Display + Clone,
{
    fn process(&self) -> String {
        format!("Processing: {}", self.data)
    }

    fn duplicate(&self) -> T {
        self.data.clone()
    }
}
```

```rust
// where permite bounds que referencian otros genericos:

fn ensure_same_debug<T, U>(t: &T, u: &U) -> String
where
    T: Debug,
    U: Debug + Into<T>,
    T: Clone,
{
    format!("{:?} and {:?}", t, u)
}

// El bound U: Into<T> relaciona los dos genericos entre si.
// Esto NO se puede expresar con impl Trait.
```

```rust
// where con bounds sobre el tipo de error asociado:
use std::str::FromStr;

fn parse_and_display<T>(input: &str) -> Result<T, T::Err>
where
    T: FromStr + Display,
    T::Err: Debug,
{
    let value = input.parse::<T>()?;
    println!("Parsed: {}", value);
    Ok(value)
}

// T::Err es el tipo asociado de FromStr.
// T::Err: Debug requiere que el error sea imprimible.
// Esto solo se puede expresar con where.

fn main() {
    let n: i32 = parse_and_display("42").unwrap();
    let f: f64 = parse_and_display("3.14").unwrap();
    println!("{} {}", n, f);
}
```

---

## Bounds en impl blocks

Se puede poner un trait bound en la definicion de un struct
generico o en un bloque `impl`. La practica idiomatica es
preferir bounds en los bloques `impl`:

```rust
use std::fmt::Display;

// Bound en la definicion del struct — NO recomendado:
struct ConstrainedWrapper<T: Display> {
    value: T,
}

// Esto fuerza a que Wrapper SOLO contenga tipos que
// implementen Display. No puedes crear ConstrainedWrapper<Vec<i32>>
// porque Vec<i32> no implementa Display.
```

```rust
// La practica idiomatica: bounds en impl, no en struct.
// Razon: el struct define la ESTRUCTURA de datos.
// Los bounds definen que OPERACIONES se pueden hacer.

// Preferido — struct sin bounds:
struct Wrapper<T> {
    value: T,
}

// Bound solo donde se necesita:
impl<T: Display> Wrapper<T> {
    fn show(&self) {
        println!("{}", self.value);
    }
}

// Metodos disponibles para TODOS los T (sin bound):
impl<T> Wrapper<T> {
    fn new(value: T) -> Self {
        Wrapper { value }
    }

    fn into_inner(self) -> T {
        self.value
    }
}

fn main() {
    let w = Wrapper::new(vec![1, 2, 3]);
    // w.show();  // error: Vec<i32> no implementa Display
    let v = w.into_inner();  // ok: into_inner no requiere Display

    let w2 = Wrapper::new(42);
    w2.show();  // ok: i32 implementa Display
}
```

```rust
// Consecuencia de poner bounds en el struct:
// Cada lugar que use el struct tambien necesita repetir el bound.

struct Constrained<T: Clone> {
    value: T,
}

// Ahora CADA impl debe repetir Clone:
impl<T: Clone> Constrained<T> {
    fn new(value: T) -> Self {
        Constrained { value }
    }
}

// Incluso funciones que no usan clone:
fn take_constrained<T: Clone>(c: Constrained<T>) {
//                     ^^^^^ obligatorio aunque no lo uses
    drop(c);
}

// Por esto es mejor dejar el struct sin bounds y poner bounds
// solo en los impl que realmente los necesitan.
```

```rust
// Multiples bloques impl con distintos bounds:

struct Container<T> {
    value: T,
}

// Disponible para todos los T:
impl<T> Container<T> {
    fn new(value: T) -> Self {
        Container { value }
    }

    fn into_inner(self) -> T {
        self.value
    }
}

// Solo si T implementa Display:
impl<T: Display> Container<T> {
    fn display(&self) {
        println!("{}", self.value);
    }
}

// Solo si T implementa Clone:
impl<T: Clone> Container<T> {
    fn duplicate(&self) -> Self {
        Container {
            value: self.value.clone(),
        }
    }
}

// Solo si T implementa Display + PartialOrd:
impl<T: Display + PartialOrd> Container<T> {
    fn max_of(self, other: Self) -> Self {
        if self.value >= other.value {
            self
        } else {
            other
        }
    }
}
```

```rust
// Implementar un trait para tu tipo solo cuando T cumple bounds:

impl<T: Display> std::fmt::Display for Container<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Container({})", self.value)
    }
}

// Container<i32> implementa Display (porque i32 lo implementa).
// Container<Vec<i32>> NO implementa Display.
```

```rust
// impl para un tipo concreto especifico (sin genericos):

struct Pair<T> {
    x: T,
    y: T,
}

// Solo para Pair<f64>:
impl Pair<f64> {
    fn distance(&self) -> f64 {
        ((self.x - self.y).powi(2)).sqrt()
    }
}

// Solo para Pair<String>:
impl Pair<String> {
    fn concatenate(&self) -> String {
        format!("{}{}", self.x, self.y)
    }
}

fn main() {
    let p = Pair { x: 3.0, y: 7.0 };
    println!("Distance: {}", p.distance());  // 4.0

    let s = Pair {
        x: String::from("hello"),
        y: String::from("world"),
    };
    println!("{}", s.concatenate());  // helloworld

    let pi = Pair { x: 1, y: 2 };
    // pi.distance();     // error: no existe para Pair<i32>
    // pi.concatenate();  // error: no existe para Pair<i32>
}
```

---

## Implementaciones condicionales

Los trait bounds permiten implementar traits o metodos
solo para ciertos tipos genericos. Este es uno de los
mecanismos mas poderosos de Rust:

```rust
use std::fmt::Display;

struct Pair<T> {
    x: T,
    y: T,
}

// Disponible para todos los T:
impl<T> Pair<T> {
    fn new(x: T, y: T) -> Self {
        Pair { x, y }
    }
}

// Solo disponible si T implementa Display + PartialOrd:
impl<T: Display + PartialOrd> Pair<T> {
    fn cmp_display(&self) {
        if self.x >= self.y {
            println!("The largest member is x = {}", self.x);
        } else {
            println!("The largest member is y = {}", self.y);
        }
    }
}

fn main() {
    let pair = Pair::new(5, 10);
    pair.cmp_display();  // ok: i32 implementa Display + PartialOrd

    let pair2 = Pair::new(vec![1], vec![2]);
    // pair2.cmp_display();  // error: Vec<i32> no implementa Display
}
```

```rust
// Blanket implementation: implementar un trait para TODOS los tipos
// que cumplan cierto bound. La biblioteca estandar usa esto mucho.

// Ejemplo de la stdlib: ToString esta implementado para todo
// tipo que implemente Display.
//
// impl<T: Display> ToString for T {
//     fn to_string(&self) -> String {
//         // usa Display para generar el String
//     }
// }
//
// Gracias a esto, cualquier tipo con Display automaticamente
// obtiene .to_string():
fn main() {
    let s = 42.to_string();       // i32 implementa Display
    let s2 = 3.14.to_string();    // f64 implementa Display
    println!("{} {}", s, s2);
}
```

```rust
// Blanket implementation propio:
trait Describable {
    fn describe(&self) -> String;
}

// Todo tipo que implemente Display + Debug automaticamente
// obtiene Describable:
impl<T: Display + std::fmt::Debug> Describable for T {
    fn describe(&self) -> String {
        format!("Display: {} | Debug: {:?}", self, self)
    }
}

fn main() {
    println!("{}", 42.describe());
    // Display: 42 | Debug: 42
    println!("{}", "hello".describe());
    // Display: hello | Debug: "hello"
}
```

```rust
// Las blanket implementations son la razon por la que a veces
// no puedes implementar un trait para tu tipo:

trait MyTrait {
    fn do_thing(&self);
}

// Blanket impl para todo Display:
impl<T: Display> MyTrait for T {
    fn do_thing(&self) {
        println!("Thing: {}", self);
    }
}

struct MyStruct;
impl Display for MyStruct {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "MyStruct")
    }
}

// No puedes implementar MyTrait para MyStruct directamente:
// impl MyTrait for MyStruct { ... }
// error: conflicting implementations — ya esta cubierto por el blanket impl.
//
// MyStruct implementa Display, asi que el blanket impl ya le da MyTrait.
```

---

## Bounds en tipos de retorno

`impl Trait` en posicion de retorno declara que la funcion
devuelve algun tipo que implementa ese trait, sin revelar
el tipo concreto (tipo opaco):

```rust
use std::fmt::Display;

// Retornar impl Trait:
fn make_greeting() -> impl Display {
    "Hello, world!"
    // El tipo concreto es &str, pero el llamador solo sabe
    // que implementa Display.
}

fn main() {
    let greeting = make_greeting();
    println!("{}", greeting);  // usa Display
    // No puedes usar metodos de &str directamente — solo Display.
}
```

```rust
// Restriccion importante: solo se puede retornar UN tipo concreto.
// Todos los caminos de retorno deben devolver el mismo tipo.

// ESTO NO COMPILA:
fn make_displayable(use_number: bool) -> impl Display {
    if use_number {
        42          // i32
    } else {
        "hello"     // &str
    }
    // error: `if` and `else` have incompatible types
    // impl Trait en retorno = un tipo concreto opaco, no dynamic dispatch.
}

// Para retornar tipos distintos, se necesita Box<dyn Trait>:
fn make_displayable_dyn(use_number: bool) -> Box<dyn Display> {
    if use_number {
        Box::new(42)
    } else {
        Box::new("hello")
    }
}
```

```rust
// impl Trait en retorno es especialmente util con closures
// e iteradores, cuyos tipos concretos son imposibles de escribir:

fn make_adder(x: i32) -> impl Fn(i32) -> i32 {
    move |y| x + y
}

fn even_numbers(limit: i32) -> impl Iterator<Item = i32> {
    (0..limit).filter(|n| n % 2 == 0)
}

fn main() {
    let add_five = make_adder(5);
    println!("{}", add_five(3));  // 8

    for n in even_numbers(10) {
        print!("{} ", n);  // 0 2 4 6 8
    }
    println!();
}
```

```rust
// Por que es util: los tipos de closures e iteradores complejos
// son inescribibles. Por ejemplo, el tipo real de even_numbers seria:
//
//   std::iter::Filter<std::ops::Range<i32>, [closure@src/main.rs:2:29]>
//
// Cada closure tiene un tipo anonimo unico generado por el compilador.
// impl Trait te permite abstraer sobre estos tipos sin Box.
```

```rust
// impl Trait en retorno con bounds adicionales:

fn make_sorted_iter(v: Vec<i32>) -> impl Iterator<Item = i32> + Clone {
    let mut sorted = v;
    sorted.sort();
    sorted.into_iter()
}

// El llamador sabe que el retorno es un Iterator<Item = i32>
// que ademas implementa Clone.
```

```rust
// Comparacion de enfoques para retorno:
//
// 1. Tipo concreto:
//    fn foo() -> Vec<i32>
//    El llamador conoce el tipo exacto.
//    Maximo rendimiento, maximo acoplamiento.
//
// 2. impl Trait (tipo opaco):
//    fn foo() -> impl Iterator<Item = i32>
//    El tipo es opaco. El llamador solo ve el trait.
//    Sigue siendo monomorfizado (resuelto en compilacion, zero-cost).
//    Solo un tipo concreto posible por ruta de retorno.
//
// 3. Box<dyn Trait> (dispatch dinamico):
//    fn foo() -> Box<dyn Iterator<Item = i32>>
//    Permite retornar tipos distintos desde distintas ramas.
//    Tiene costo en runtime (allocacion en heap + vtable).
```

```rust
// Limitacion: impl Trait en retorno no funcionaba en trait definitions
// antes de Rust 1.75. A partir de Rust 1.75, return-position impl
// Trait in traits (RPITIT) es estable:

trait Factory {
    // Rust 1.75+: esto compila
    fn create(&self) -> impl Display;
}

struct NumberFactory;
impl Factory for NumberFactory {
    fn create(&self) -> impl Display {
        42
    }
}

struct StringFactory;
impl Factory for StringFactory {
    fn create(&self) -> impl Display {
        "hello"
    }
}
```

---

## Supertraits

Un supertrait es un trait que requiere la implementacion
de otro trait como prerequisito. Se define con la sintaxis
`trait Foo: Bar`, que significa "para implementar Foo,
tambien debes implementar Bar":

```rust
use std::fmt::Display;

// Supertrait: Printable requiere Display.
// Todo tipo que implemente Printable DEBE implementar Display.
trait Printable: Display {
    fn print(&self) {
        println!("[Printable] {}", self);
        //                        ^^^^ usa Display — garantizado por el supertrait
    }
}

struct Point {
    x: f64,
    y: f64,
}

// Primero implementar Display (el supertrait):
impl Display for Point {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "({}, {})", self.x, self.y)
    }
}

// Ahora puedes implementar Printable:
impl Printable for Point {}
// Usa la implementacion default de print().

fn main() {
    let p = Point { x: 1.0, y: 2.0 };
    p.print();  // [Printable] (1.0, 2.0)
}
```

```rust
// Sin implementar Display, no puedes implementar Printable:

struct Secret {
    data: Vec<u8>,
}

// impl Printable for Secret {}
// error[E0277]: `Secret` doesn't implement `std::fmt::Display`
//
// El compilador dice: para implementar Printable,
// Secret necesita implementar Display primero.
```

```rust
// Multiples supertraits con +:
use std::fmt::{Display, Debug};

trait Loggable: Display + Debug + Clone {
    fn log(&self) {
        println!("[LOG] Display: {} | Debug: {:?}", self, self);
    }

    fn log_clone(&self) {
        let copy = self.clone();
        println!("[LOG] Cloned: {:?}", copy);
    }
}

// Todo tipo que quiera implementar Loggable debe implementar
// Display, Debug y Clone.
```

```rust
// Los supertraits son transitivos:

trait A {
    fn method_a(&self);
}

trait B: A {
    fn method_b(&self);
}

trait C: B {
    fn method_c(&self);
}

// Implementar C requiere implementar B, que requiere implementar A.
// Un tipo que implementa C tiene acceso a method_a, method_b y method_c.

struct MyType;

impl A for MyType {
    fn method_a(&self) { println!("A"); }
}

impl B for MyType {
    fn method_b(&self) { println!("B"); }
}

impl C for MyType {
    fn method_c(&self) { println!("C"); }
}

// Si usas C como bound, automaticamente tienes acceso a A y B:
fn use_c<T: C>(item: &T) {
    item.method_a();  // disponible via A (supertrait de B, supertrait de C)
    item.method_b();  // disponible via B (supertrait de C)
    item.method_c();  // disponible via C directamente
}
```

```rust
// Supertrait vs bound: son cosas distintas.
//
// Supertrait (en la definicion del trait):
//   trait Printable: Display { ... }
//   "Todo tipo que implemente Printable DEBE implementar Display."
//   Es una restriccion permanente sobre el trait.
//
// Bound (en una funcion o impl):
//   fn foo<T: Display>(x: &T) { ... }
//   "Este parametro T debe implementar Display."
//   Es una restriccion local, solo para esta funcion.
//
// Un supertrait afecta a TODOS los implementadores.
// Un bound afecta solo al uso puntual.
```

```rust
// Ejemplo real: std::error::Error tiene supertraits.
//
// pub trait Error: Display + Debug {
//     fn source(&self) -> Option<&(dyn Error + 'static)> { None }
// }
//
// Esto significa que todo tipo de error debe poder:
// - Mostrarse al usuario (Display).
// - Mostrarse para debugging (Debug).
// - Opcionalmente, revelar la causa subyacente (source).
```

```rust
// Patron comun: crear un supertrait que agrupe bounds frecuentes.

trait Entity: Display + Debug + Clone + PartialEq {
    fn id(&self) -> u64;
}

// Ahora en vez de escribir:
//   fn process<T: Display + Debug + Clone + PartialEq + HasId>(item: &T)
// Escribes:
//   fn process<T: Entity>(item: &T)

fn process_entity<T: Entity>(item: &T) {
    println!("Processing entity {}: {}", item.id(), item);
    let backup = item.clone();
    if &backup == item {
        println!("Backup verified: {:?}", backup);
    }
}
```

---

## Bounds con lifetimes

Los trait bounds pueden incluir lifetimes. `T: 'a` significa
que todas las referencias dentro de T viven al menos tanto
como `'a`:

```rust
// T: 'a — T no contiene referencias que expiren antes de 'a.
fn longest_with_announcement<'a, T>(x: &'a str, y: &'a str, ann: T) -> &'a str
where
    T: Display + 'a,
{
    println!("Announcement: {}", ann);
    if x.len() > y.len() { x } else { y }
}

// El bound T: Display + 'a dice:
// 1. T implementa Display.
// 2. Si T contiene referencias, estas viven al menos 'a.
```

```rust
// T: 'static — T no contiene referencias no-'static.
// Esto incluye tipos sin referencias (i32, String, Vec<T>)
// y tipos con referencias 'static (&'static str).

fn spawn_task<T: Send + 'static>(data: T) {
    std::thread::spawn(move || {
        // data vive aqui dentro del thread.
        // Necesita 'static porque el thread puede vivir
        // mas que la funcion que lo creo.
        drop(data);
    });
}

fn main() {
    spawn_task(String::from("hello"));   // ok: String es 'static
    spawn_task(42);                      // ok: i32 es 'static

    let local = String::from("local");
    // spawn_task(&local);  // error: &String no es 'static
    // La referencia apunta a un valor local que podria
    // destruirse antes de que el thread termine.
}
```

```rust
// Diferencia entre 'a y 'static en bounds:
//
// T: 'a
//   "T vive al menos tanto como 'a."
//   El lifetime concreto de 'a lo determina el compilador
//   en cada punto de uso. Es flexible.
//
// T: 'static
//   "T no contiene referencias prestadas de corta duracion."
//   Es el bound mas restrictivo. Muchos tipos owned lo cumplen:
//   i32, String, Vec<i32>, Box<dyn Error>, &'static str.
//   Pero &String (referencia a String local) NO lo cumple.
//
// IMPORTANTE: T: 'static NO significa "T vive para siempre".
// Significa "T no contiene referencias prestadas".
// Un String es 'static, pero se puede dropear cuando quieras.

fn takes_static<T: 'static>(item: T) {
    drop(item);  // item se destruye aqui — no "vive para siempre"
}

fn main() {
    let s = String::from("hello");
    takes_static(s);  // ok: String es 'static (no tiene refs)
    // s ya fue movido y dropeado
}
```

```rust
// Lifetime bounds en structs:
struct Ref<'a, T: 'a> {
    value: &'a T,
}

// T: 'a garantiza que T vive al menos tanto como la referencia.
// Sin este bound, el compilador no puede verificar que
// &'a T sea valida.
//
// Nota: a partir de Rust 2018, este bound se infiere automaticamente
// cuando un struct contiene &'a T. No necesitas escribir T: 'a
// explicitamente en la mayoria de los casos.

struct RefModern<'a, T> {
    value: &'a T,
    // El compilador infiere T: 'a automaticamente.
}

fn main() {
    let val = 42;
    let r = Ref { value: &val };
    let r2 = RefModern { value: &val };
    println!("{} {}", r.value, r2.value);
}
```

```rust
// Combinacion de lifetime bounds con trait bounds:

fn process_ref<'a, T>(items: &'a [T]) -> Vec<&'a T>
where
    T: Display + 'a,
{
    items.iter().filter(|item| {
        let s = format!("{}", item);
        s.len() > 3
    }).collect()
}

// En este caso T: 'a es redundante (el compilador lo infiere
// desde &'a [T]), pero es explicito para claridad.
```

```rust
// Higher-Rank Trait Bounds (HRTB) — for<'a>:
// Un bound avanzado que dice "para CUALQUIER lifetime 'a".

fn apply_to_ref<F>(f: F, value: &str) -> String
where
    F: for<'a> Fn(&'a str) -> String,
{
    f(value)
}

// for<'a> Fn(&'a str) -> String dice:
// "F debe funcionar con una referencia de CUALQUIER lifetime."
//
// Esto se usa internamente en closures que toman referencias.
// Raramente se escribe manualmente — el compilador suele inferirlo.
// Pero es util saberlo para leer mensajes de error.

fn main() {
    let result = apply_to_ref(|s| format!("Got: {}", s), "hello");
    println!("{}", result);  // Got: hello
}
```

---

## El bound Sized y ?Sized

Todos los tipos genericos en Rust tienen un bound implicito:
`T: Sized`. Esto significa que el compilador debe conocer el
tamano de T en tiempo de compilacion:

```rust
// Estas dos firmas son equivalentes:
fn foo<T>(item: T) {}
fn foo_explicit<T: Sized>(item: T) {}

// Sized es un "auto trait" — el compilador lo agrega automaticamente.
// Casi todos los tipos son Sized: i32, String, Vec<T>, structs, enums.
```

```rust
// Tipos que NO son Sized (dynamically sized types / DST):
// - str (string slice sin &)
// - [T] (slice sin &)
// - dyn Trait (trait object sin &/Box)
//
// Estos tipos no tienen tamano conocido en compilacion.
// Solo se pueden usar detras de un puntero: &str, &[T], &dyn Trait.

// fn takes_str(s: str) {}
// error: the size for values of type `str` cannot be known at
// compilation time
```

```rust
// ?Sized opta fuera del bound Sized implicito.
// Permite que T sea un tipo sin tamano conocido.

fn print_ref<T: ?Sized + std::fmt::Display>(item: &T) {
    println!("{}", item);
}

// Con ?Sized, T puede ser str:
fn main() {
    let s: &str = "hello";
    print_ref(s);       // T = str (unsized), item = &str

    let n: &i32 = &42;
    print_ref(n);       // T = i32 (sized), item = &i32
}

// Sin ?Sized:
fn print_sized<T: std::fmt::Display>(item: &T) {
    println!("{}", item);
}
// T solo puede ser tipos Sized. Sigue aceptando &str como
// parametro (porque &str es Sized), pero T seria &str, no str.
```

```rust
// ?Sized en structs — permite almacenar DSTs detras de punteros:
use std::fmt::Display;

struct Wrapper<T: ?Sized> {
    value: Box<T>,
}

fn main() {
    let w1 = Wrapper { value: Box::new(42) };            // T = i32
    let w2: Wrapper<dyn Display> = Wrapper {              // T = dyn Display
        value: Box::new("hello"),
    };
}

// La biblioteca estandar usa ?Sized extensivamente.
// Ejemplo: la definicion de Box:
// pub struct Box<T: ?Sized>(/* ... */);
// Esto permite Box<dyn Trait>, Box<str>, etc.
```

```rust
// Nota: ?Sized es el UNICO "bound negativo" en Rust estable.
// No existe ?Clone ni ?Send. Solo ?Sized.
//
// La razon es que Sized es el unico trait que se agrega como
// bound implicito, asi que es el unico que necesita opt-out.
```

---

## Trait bounds vs trait objects

Los trait bounds (generics) y los trait objects (dyn Trait)
son dos formas de polimorfismo en Rust. La diferencia
fundamental es CUANDO se resuelve el tipo:

```rust
use std::fmt::Display;

// 1. Trait bound (generics) — resuelto en COMPILACION:
fn print_generic<T: Display>(item: &T) {
    println!("{}", item);
}

// 2. Trait object (dyn) — resuelto en RUNTIME:
fn print_dynamic(item: &dyn Display) {
    println!("{}", item);
}

fn main() {
    print_generic(&42);       // genera version para i32
    print_generic(&"hello");  // genera version para &str

    print_dynamic(&42);       // usa vtable en runtime
    print_dynamic(&"hello");  // usa vtable en runtime
}
```

```rust
// Monomorphization (generics / trait bounds):
// El compilador genera una copia de la funcion para cada tipo concreto.
//
// print_generic::<i32>(&42)     → version especializada para i32
// print_generic::<&str>(&"hi")  → version especializada para &str
//
// Ventajas:
//   - Zero-cost abstraction: no hay indirection en runtime.
//   - El compilador puede hacer inlining y optimizaciones especificas.
//   - Mas rapido en runtime.
//
// Desventajas:
//   - Mas tiempo de compilacion (genera mas codigo).
//   - Binario mas grande (code bloat).
//   - No puede almacenar tipos heterogeneos en una coleccion.
```

```rust
// Dynamic dispatch (dyn Trait / trait objects):
// El compilador usa una vtable (tabla de funciones virtuales).
//
// &dyn Display es un "fat pointer": contiene:
//   - Puntero a los datos.
//   - Puntero a la vtable (tabla con punteros a las funciones del trait).
//
// Ventajas:
//   - Un solo cuerpo de funcion (binario mas compacto).
//   - Permite colecciones heterogeneas: Vec<Box<dyn Display>>.
//   - Permite retornar tipos distintos desde distintas ramas.
//
// Desventajas:
//   - Indirection en cada llamada a metodo (sigue el puntero de la vtable).
//   - No se puede hacer inlining.
//   - Allocacion en heap si usas Box<dyn Trait>.
```

```rust
// Ejemplo: coleccion heterogenea (solo posible con dyn):
fn main() {
    // Con generics, todos los elementos deben ser del mismo tipo:
    let homogeneous: Vec<i32> = vec![1, 2, 3];

    // Con dyn, puedes mezclar tipos:
    let heterogeneous: Vec<Box<dyn Display>> = vec![
        Box::new(42),
        Box::new("hello"),
        Box::new(3.14),
    ];

    for item in &heterogeneous {
        println!("{}", item);
    }
    // 42
    // hello
    // 3.14
}
```

```rust
// Cuadro de decision — cuando usar cada uno:
//
// Usa GENERICS (trait bounds) cuando:
//   - El rendimiento es critico (hot path).
//   - El tipo se conoce en compilacion.
//   - No necesitas colecciones heterogeneas.
//   - Quieres que el compilador optimice agresivamente.
//
// Usa DYN TRAIT (trait objects) cuando:
//   - Necesitas colecciones de tipos distintos.
//   - El tipo se decide en runtime (plugins, configuracion, etc.).
//   - Quieres reducir el tamano del binario.
//   - La funcion se usa con muchos tipos y no quieres code bloat.
//   - Trabajas con APIs que requieren object safety.
```

```rust
// Object safety: no todos los traits pueden usarse como dyn.
// Un trait es "object safe" si:
//   1. Todos sus metodos toman &self, &mut self, o self: Box<Self>.
//   2. Ningun metodo retorna Self.
//   3. Ningun metodo tiene parametros genericos.
//   4. No tiene bound Self: Sized.

// Object safe:
trait Drawable {
    fn draw(&self);
    fn area(&self) -> f64;
}

// NO object safe (retorna Self):
trait Clonable {
    fn clone_self(&self) -> Self;
}
// No puedes tener &dyn Clonable.

// NO object safe (metodo generico):
trait Converter {
    fn convert<T>(&self) -> T;
}
// No puedes tener &dyn Converter.
```

```rust
// Patron comun: usar generics internamente, dyn en la API publica:

trait Handler {
    fn handle(&self, request: &str) -> String;
}

struct Router {
    handlers: Vec<Box<dyn Handler>>,  // dyn para heterogeneidad
}

impl Router {
    fn new() -> Self {
        Router { handlers: Vec::new() }
    }

    // Generics para aceptar cualquier Handler sin Box explicito:
    fn add_handler<H: Handler + 'static>(&mut self, handler: H) {
        self.handlers.push(Box::new(handler));
    }

    fn route(&self, request: &str) -> Vec<String> {
        self.handlers.iter().map(|h| h.handle(request)).collect()
    }
}
```

---

## Patrones comunes de la stdlib

La biblioteca estandar define traits que se usan constantemente
como bounds. Conocerlos es esencial para escribir Rust idiomatico:

```rust
// T: Into<String> — acepta cualquier cosa convertible a String.
// Patron muy comun para parametros de tipo String.

fn greet(name: impl Into<String>) {
    let name: String = name.into();
    println!("Hello, {}!", name);
}

fn main() {
    greet("world");                    // &str -> String
    greet(String::from("Rust"));       // String -> String (identidad)
    greet(format!("{}-{}", "a", "b")); // String -> String

    // Sin Into<String>, la firma seria:
    // fn greet(name: &str)     — solo acepta &str
    // fn greet(name: String)   — fuerza a pasar String con .to_string()
    // Con Into<String>, ambos funcionan sin conversion explicita.
}
```

```rust
// T: AsRef<Path> — acepta cualquier tipo convertible a &Path.
// La biblioteca estandar usa esto en funciones de filesystem.

use std::path::Path;

fn file_exists(path: impl AsRef<Path>) -> bool {
    path.as_ref().exists()
}

fn main() {
    // Acepta &str, String, PathBuf, &Path:
    println!("{}", file_exists("/etc/hosts"));
    println!("{}", file_exists(String::from("/tmp")));

    let path = Path::new("/usr/bin");
    println!("{}", file_exists(path));
}
```

```rust
// T: AsRef<str> — acepta cualquier cosa convertible a &str.
fn count_words(text: impl AsRef<str>) -> usize {
    text.as_ref().split_whitespace().count()
}

fn main() {
    println!("{}", count_words("hello world"));           // &str
    println!("{}", count_words(String::from("one two"))); // String
}
```

```rust
// Iterator con associated types en bounds:
fn sum_items<I>(iter: I) -> i32
where
    I: Iterator<Item = i32>,
{
    iter.sum()
}

// Item = i32 restringe el tipo asociado del iterador.
// Solo acepta iteradores que produzcan i32.

fn print_items<I, T>(iter: I)
where
    I: Iterator<Item = T>,
    T: Display,
{
    for item in iter {
        println!("{}", item);
    }
}

fn main() {
    let total = sum_items(vec![1, 2, 3].into_iter());
    println!("Sum: {}", total);  // 6

    print_items(vec!["a", "b", "c"].into_iter());
}
```

```rust
// T: Default — el tipo tiene un valor por defecto.
fn make_pair_default<T: Default>() -> (T, T) {
    (T::default(), T::default())
}

fn main() {
    let (a, b): (i32, i32) = make_pair_default();
    println!("{} {}", a, b);  // 0 0

    let (s1, s2): (String, String) = make_pair_default();
    println!("'{}' '{}'", s1, s2);  // '' ''
}
```

```rust
// Resumen de bounds comunes en la stdlib:
//
// Display        — se puede formatear con {}
// Debug          — se puede formatear con {:?}
// Clone          — se puede duplicar con .clone()
// Copy           — se puede duplicar implicitamente (requiere Clone)
// Default        — tiene un valor por defecto
// PartialEq, Eq  — se puede comparar con == y !=
// PartialOrd, Ord — se puede comparar con <, >, <=, >=
// Hash           — se puede usar como clave en HashMap
// Send           — se puede enviar entre threads
// Sync           — se puede compartir entre threads (&T es Send)
// Into<U>        — se puede convertir a U
// From<U>        — se puede crear desde U
// AsRef<U>       — se puede obtener &U
// Iterator       — produce una secuencia de valores
// FromStr        — se puede crear desde &str (.parse())
// Sized          — tamano conocido en compilacion (implicito)
// ?Sized         — tamano posiblemente desconocido (opt-out)
```

---

## Bounds combinados — ejemplo integrador

```rust
use std::collections::HashMap;
use std::fmt::Display;
use std::hash::Hash;

// Contar ocurrencias de elementos. El tipo de los elementos
// necesita: Eq + Hash (para usar HashMap) y Display (para imprimir).
fn count_and_display<T>(items: &[T])
where
    T: Eq + Hash + Display,
{
    let mut counts: HashMap<&T, usize> = HashMap::new();
    for item in items {
        *counts.entry(item).or_insert(0) += 1;
    }
    for (item, count) in &counts {
        println!("{}: {} times", item, count);
    }
}

fn main() {
    let words = vec!["hello", "world", "hello", "rust", "hello", "world"];
    count_and_display(&words);
    // hello: 3 times
    // world: 2 times
    // rust: 1 times

    let nums = vec![1, 2, 3, 1, 2, 1];
    count_and_display(&nums);
}
```

```rust
// Struct con multiples genericos y bounds complejos:
use std::fmt::{Display, Debug};

struct Registry<K, V> {
    entries: Vec<(K, V)>,
}

impl<K, V> Registry<K, V>
where
    K: Display + PartialEq,
    V: Debug + Clone,
{
    fn new() -> Self {
        Registry { entries: Vec::new() }
    }

    fn insert(&mut self, key: K, value: V) {
        self.entries.push((key, value));
    }

    fn get(&self, key: &K) -> Option<&V> {
        self.entries
            .iter()
            .find(|(k, _)| k == key)
            .map(|(_, v)| v)
    }

    fn dump(&self) {
        for (key, value) in &self.entries {
            println!("{} => {:?}", key, value);
        }
    }
}

fn main() {
    let mut reg = Registry::new();
    reg.insert("name", "Alice");
    reg.insert("lang", "Rust");
    reg.dump();
    // name => "Alice"
    // lang => "Rust"

    if let Some(v) = reg.get(&"name") {
        println!("Found: {:?}", v);
    }
}
```

```rust
// Ejemplo avanzado: trait generico con bounds en tipo asociado.

trait Processor {
    type Input: Display;
    type Output: Debug;

    fn process(&self, input: Self::Input) -> Self::Output;
}

struct UpperCase;

impl Processor for UpperCase {
    type Input = String;
    type Output = String;

    fn process(&self, input: String) -> String {
        input.to_uppercase()
    }
}

fn run_processor<P>(proc: &P, input: P::Input)
where
    P: Processor,
    P::Output: Display,  // bound adicional sobre el tipo asociado
{
    let result = proc.process(input);
    println!("Result: {}", result);
}

fn main() {
    let p = UpperCase;
    run_processor(&p, String::from("hello"));
    // Result: HELLO
}
```

---

## Errores comunes

### Bound que falta

El error mas frecuente. El compilador te dice exactamente
que bound necesitas:

```rust
// Error: bound faltante
fn print_item<T>(item: &T) {
    println!("{}", item);
}
// error[E0277]: `T` doesn't implement `std::fmt::Display`
//  --> src/main.rs:2:20
//   |
// 2 |     println!("{}", item);
//   |                    ^^^^ `T` cannot be formatted with the default formatter
//   |
// help: consider restricting type parameter `T`
//   |
// 1 | fn print_item<T: std::fmt::Display>(item: &T) {
//   |                +++++++++++++++++++++

// Solucion: seguir la sugerencia del compilador.
fn print_item<T: std::fmt::Display>(item: &T) {
    println!("{}", item);
}
```

### Over-constraining (bounds innecesarios)

```rust
// MAL: pedir mas bounds de los que necesitas.
fn just_print<T: Display + Debug + Clone + PartialEq>(item: &T) {
    println!("{}", item);
    // Solo usa Display, pero pide Debug, Clone y PartialEq.
}

// BIEN: solo los bounds que usas.
fn just_print_clean<T: Display>(item: &T) {
    println!("{}", item);
}

// Over-constraining reduce la reutilizacion:
// un tipo que implementa Display pero no Clone
// no puede usar just_print, pero si just_print_clean.
```

### Bounds en struct vs bounds en impl

```rust
// Antipatron: bounds en el struct.
struct Bad<T: Clone + Debug> {
    value: T,
}

// Ahora TODO lo que use Bad necesita repetir Clone + Debug:
fn takes_bad<T: Clone + Debug>(b: Bad<T>) {}  // obligatorio

// Patron correcto: sin bounds en el struct.
struct Good<T> {
    value: T,
}

impl<T: Clone + Debug> Good<T> {
    fn clone_and_debug(&self) {
        let c = self.value.clone();
        println!("{:?}", c);
    }
}

fn takes_good<T>(g: Good<T>) {}  // no necesita bounds extra
```

### Conflicto con blanket implementations

```rust
// Si existe un blanket impl, no puedes implementar el trait manualmente:

trait MyTrait {}

// Blanket impl:
impl<T: Display> MyTrait for T {}

// Intentar implementar para un tipo especifico:
struct MyStruct;
impl Display for MyStruct {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "MyStruct")
    }
}

// impl MyTrait for MyStruct {}
// error[E0119]: conflicting implementations of trait `MyTrait` for type `MyStruct`
//
// MyStruct ya implementa MyTrait via el blanket impl
// (porque MyStruct implementa Display).
```

### Olvidar bounds en el tipo de retorno

```rust
// Error: intentar usar impl Trait para retornar tipos distintos.

fn make_thing(flag: bool) -> impl Display {
    if flag {
        42
    } else {
        "hello"
    }
}
// error[E0308]: `if` and `else` have incompatible types
//
// impl Trait en retorno es UN tipo concreto opaco.
// No es dynamic dispatch. Para tipos distintos, usa Box<dyn Trait>.

fn make_thing_fixed(flag: bool) -> Box<dyn Display> {
    if flag {
        Box::new(42)
    } else {
        Box::new("hello")
    }
}
```

### Error con lifetime bounds

```rust
// Error: referencia local que no cumple 'static.

fn spawn<T: Send + 'static>(data: T) {
    std::thread::spawn(move || {
        drop(data);
    });
}

fn bad_example() {
    let local = String::from("hello");
    let reference = &local;
    // spawn(reference);
    // error[E0597]: `local` does not live long enough
    //
    // &local tiene lifetime limitado a bad_example.
    // El thread necesita 'static.

    // Solucion: pasar el String directamente (move, no referencia):
    spawn(local);
}
```

### Bounds circulares o imposibles

```rust
// Cuidado con bounds que crean restricciones demasiado fuertes:

// Copy y Drop manual son incompatibles:
// un tipo que implementa Drop manualmente no puede ser Copy.
//
// struct Problematic;
// impl Copy for Problematic {}
// impl Clone for Problematic {
//     fn clone(&self) -> Self { *self }
// }
// impl Drop for Problematic {
//     fn drop(&mut self) {}
// }
// error[E0184]: the trait `Copy` cannot be implemented
// for this type; the type has a destructor

// La leccion: conocer las incompatibilidades entre traits.
// Copy y Drop manual no pueden coexistir.
```

### No usar where cuando mejora la legibilidad

```rust
// MAL: firma ilegible
fn process<T: Display + Debug + Clone + PartialOrd + Hash, U: Into<String> + AsRef<str> + Clone>(
    item: &T,
    name: U,
) -> String {
    format!("{}: {}", name.as_ref(), item)
}

// BIEN: misma funcion con where
fn process_clean<T, U>(item: &T, name: U) -> String
where
    T: Display + Debug + Clone + PartialOrd + Hash,
    U: Into<String> + AsRef<str> + Clone,
{
    format!("{}: {}", name.as_ref(), item)
}
```

### Error al usar traits no object-safe como dyn

```rust
// Clone no es object-safe (su metodo retorna Self):

// fn take_clonable(item: &dyn Clone) {}
// error[E0038]: the trait `Clone` cannot be made into an object
//
// Motivo: clone() retorna Self, y con dyn el compilador
// no sabe que tipo concreto es Self.
//
// Solucion 1: usar generics en vez de dyn.
fn take_clonable<T: Clone>(item: &T) {
    let _copy = item.clone();
}
//
// Solucion 2: crear un trait object-safe wrapper.
trait CloneBox {
    fn clone_box(&self) -> Box<dyn CloneBox>;
}
```

---

## Resumen y cheatsheet

```text
SINTAXIS DE TRAIT BOUNDS
========================

Forma inline:
  fn foo<T: Trait>(x: T)
  fn foo<T: TraitA + TraitB>(x: T)
  fn foo<T: TraitA, U: TraitB>(x: T, y: U)

Forma impl Trait (azucar sintactico):
  fn foo(x: impl Trait)
  fn foo(x: &(impl TraitA + TraitB))

Clausula where:
  fn foo<T, U>(x: T, y: U) -> R
  where
      T: TraitA + TraitB,
      U: TraitC,
  { ... }

Bounds en impl blocks:
  impl<T: Trait> MyStruct<T> { ... }
  impl<T> MyStruct<T> where T: Trait { ... }

Bounds en retorno:
  fn foo() -> impl Trait
  fn foo() -> impl TraitA + TraitB

Supertraits:
  trait Child: Parent { ... }
  trait Child: ParentA + ParentB { ... }

Lifetime bounds:
  T: 'a                  — T vive al menos 'a
  T: 'static             — T no tiene refs prestadas
  T: Trait + 'a           — T implementa Trait y vive al menos 'a

Sized bounds:
  T                      — implicito: T: Sized
  T: ?Sized              — T puede ser unsized (str, [T], dyn Trait)

HRTB:
  F: for<'a> Fn(&'a str)  — F funciona con cualquier lifetime


CUANDO USAR QUE
================

impl Trait:
  Parametros simples, una sola aparicion del tipo.

T: Trait (inline):
  1-2 genericos, bounds cortos, necesitas turbofish o mismo tipo.

where:
  3+ bounds, multiples genericos, tipos asociados.

Supertraits:
  Cuando un trait SIEMPRE necesita otro como prerequisito.

Generics vs dyn Trait:
  Generics -> compilacion, zero-cost, mismo tipo.
  dyn Trait -> runtime, tipos heterogeneos, object safety.


REGLAS DE ORO
=============

1. Pide solo los bounds que realmente usas (no over-constrain).
2. Pon bounds en impl, no en struct.
3. Usa where cuando la firma se vuelva larga.
4. Prefiere generics sobre dyn salvo que necesites heterogeneidad.
5. Si el compilador pide un bound, agregalo.
6. Si una coleccion necesita tipos distintos, usa Box<dyn Trait>.
7. Recuerda: impl Trait en retorno = un solo tipo concreto.
```

---

## Ejercicios

### Ejercicio 1 — Bounds basicos

```rust
// Implementar una funcion `print_largest` que:
// - Reciba un slice &[T].
// - Encuentre el elemento mas grande.
// - Imprima: "The largest is: <value>".
// - Devuelva una referencia al elemento mas grande.
//
// Determinar que trait bounds necesita T:
// - Para comparar elementos: PartialOrd
// - Para imprimir: Display
//
// Probar con:
//   print_largest(&[3, 1, 4, 1, 5, 9]);
//   print_largest(&['z', 'a', 'm']);
//   print_largest(&[1.1, 2.2, 0.5]);
```

### Ejercicio 2 — where con multiples genericos

```rust
// Implementar una funcion `merge_and_show` que:
// - Reciba dos vectores: Vec<T> y Vec<U>.
// - Imprima los elementos de ambos vectores (Display).
// - Devuelva un tuple (Vec<T>, Vec<U>) con los vectores originales.
//
// Usar clausula where para los bounds.
// Verificar que compila con tipos distintos:
//   merge_and_show(vec![1, 2, 3], vec!["a", "b"]);
```

### Ejercicio 3 — Implementaciones condicionales

```rust
// Crear un struct `Container<T>` con un campo value: T.
//
// Implementar:
// 1. new() y into_inner() sin bounds (para cualquier T).
// 2. display() solo si T: Display.
// 3. duplicate() solo si T: Clone.
// 4. compare() que recibe otro Container<T> y dice cual es mayor,
//    solo si T: Display + PartialOrd.
//
// Probar con:
// - Container::new(vec![1,2,3]) — puede usar new/into_inner pero no display.
// - Container::new(42) — puede usar todos los metodos.
```

### Ejercicio 4 — Patron Into y AsRef

```rust
// Crear una funcion `log_message` que:
// - Reciba un "level" (impl Into<String>) y un "message" (impl AsRef<str>).
// - Imprima: "[LEVEL] message".
//
// Verificar que acepta todas estas combinaciones:
//   log_message("INFO", "Server started");
//   log_message(String::from("ERROR"), "Connection failed");
//   log_message("DEBUG", String::from("Processing request"));
//
// Luego crear una version con where clause que acepte
// los mismos tipos pero con parametros genericos nombrados.
```

### Ejercicio 5 — Supertraits

```rust
// Definir un supertrait `Entity: Display + Debug` con un metodo id() -> u64.
//
// Crear dos structs: User y Product, ambos implementando Entity
// (y por tanto Display y Debug).
//
// Crear una funcion `log_entities` que reciba un &[Box<dyn Entity>]
// y para cada uno imprima:
//   "[ID] Display: {} | Debug: {:?}"
//
// Verificar que Entity es object-safe y se puede usar con dyn.
```

### Ejercicio 6 — Generics vs dyn

```rust
// Crear un trait `Shape` con metodos area() -> f64 y name() -> &str.
//
// Implementar para Circle y Rectangle.
//
// Crear dos versiones de una funcion que imprima la informacion:
//   1. `print_shape_generic<T: Shape>(shape: &T)` — generics
//   2. `print_shape_dynamic(shape: &dyn Shape)` — dynamic dispatch
//
// Crear un Vec<Box<dyn Shape>> con circulos y rectangulos mezclados.
// Iterar e imprimir cada uno.
//
// Pregunta: por que no puedes usar Vec<T: Shape> para esto?
```
