# Cuándo escribir una macro vs una función genérica

## Índice
1. [El principio: preferir funciones](#1-el-principio-preferir-funciones)
2. [Qué pueden hacer las macros que las funciones no](#2-qué-pueden-hacer-las-macros-que-las-funciones-no)
3. [Qué pueden hacer las funciones que las macros no](#3-qué-pueden-hacer-las-funciones-que-las-macros-no)
4. [Árbol de decisión](#4-árbol-de-decisión)
5. [Caso: número variable de argumentos](#5-caso-número-variable-de-argumentos)
6. [Caso: generación de código repetitivo](#6-caso-generación-de-código-repetitivo)
7. [Caso: DSLs y sintaxis personalizada](#7-caso-dsls-y-sintaxis-personalizada)
8. [Debugging de macros](#8-debugging-de-macros)
9. [Errores crípticos y cómo mejorarlos](#9-errores-crípticos-y-cómo-mejorarlos)
10. [Alternativas a macros](#10-alternativas-a-macros)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. El principio: preferir funciones

La regla general en Rust es clara:

> **Si puedes hacerlo con una función, hazlo con una función.**

Las funciones son más fáciles de leer, depurar, documentar, y refactorizar. Las macros son una herramienta de último recurso para cuando las funciones genuinamente no pueden expresar lo que necesitas.

```rust
// INNECESARIO como macro:
macro_rules! add {
    ($a:expr, $b:expr) => { $a + $b };
}
let x = add!(2, 3);

// Mejor como función:
fn add(a: i32, b: i32) -> i32 { a + b }
let x = add(2, 3);

// O como función genérica:
fn add<T: std::ops::Add<Output = T>>(a: T, b: T) -> T { a + b }
let x = add(2, 3);
let y = add(1.5, 2.5);
```

---

## 2. Qué pueden hacer las macros que las funciones no

### 1. Número variable de argumentos

```rust
// Las funciones tienen aridad fija:
fn print_two(a: impl Display, b: impl Display) {
    println!("{} {}", a, b);
}
// ¿Y si necesitas 3? ¿O 10? ¿O 0?

// Las macros aceptan cualquier cantidad:
macro_rules! print_all {
    ($($x:expr),*) => {
        $( println!("{}", $x); )*
    };
}
print_all!();                    // 0 args
print_all!(1);                   // 1 arg
print_all!(1, "two", 3.0, true); // 4 args de distintos tipos
```

### 2. Generar nuevos items (structs, funciones, impls)

```rust
// Una función no puede crear un struct:
// fn create_struct(name: ???) { struct name { ... } }  // imposible

// Una macro sí:
macro_rules! make_struct {
    ($name:ident { $($field:ident : $ty:ty),* }) => {
        #[derive(Debug)]
        pub struct $name {
            $( pub $field: $ty, )*
        }
    };
}
make_struct!(Point { x: f64, y: f64 });
```

### 3. Trabajar con identificadores como datos

```rust
// Una función no puede recibir un nombre de variable y usarlo:
// fn set_variable(name: ???, value: i32) { name = value; }

// Una macro sí:
macro_rules! declare {
    ($name:ident = $val:expr) => {
        let $name = $val;
    };
}
declare!(x = 42);
println!("{}", x);  // 42
```

### 4. Capturar información del call site

```rust
// Una función no sabe dónde fue llamada:
fn log(msg: &str) {
    println!("[???] {}", msg);  // no sabe archivo/línea
}

// Una macro sí:
macro_rules! log {
    ($msg:expr) => {
        println!("[{}:{}] {}", file!(), line!(), $msg);
    };
}
log!("something happened");
// [src/main.rs:15] something happened
```

### 5. Evaluación condicional / lazy

```rust
// Una función evalúa TODOS los argumentos antes de ejecutarse:
fn assert_fn(cond: bool, msg: String) {
    // msg YA fue construido, incluso si cond es true
    if !cond { panic!("{}", msg); }
}
assert_fn(true, format!("expensive: {}", compute()));  // compute() se ejecuta siempre

// Una macro puede evitar evaluar argumentos:
macro_rules! assert_lazy {
    ($cond:expr, $($arg:tt)+) => {
        if !$cond {
            panic!($($arg)+);  // format! solo se ejecuta si cond es false
        }
    };
}
assert_lazy!(true, "expensive: {}", compute());  // compute() NO se ejecuta
```

### 6. Aceptar distintas sintaxis

```rust
// Una función tiene una sola forma de llamarse:
fn config(key: &str, value: &str) { }

// Una macro puede aceptar múltiples formas:
macro_rules! config {
    ($key:ident = $val:expr) => { /* key = value */ };
    ($key:ident : $val:expr) => { /* key: value */ };
    ($key:ident => $val:expr) => { /* key => value */ };
}
config!(port = 8080);
config!(host: "localhost");
config!(debug => true);
```

---

## 3. Qué pueden hacer las funciones que las macros no

### 1. Ser valores de primera clase

```rust
// Las funciones se pueden pasar como argumentos:
fn apply(f: fn(i32) -> i32, x: i32) -> i32 { f(x) }
fn double(x: i32) -> i32 { x * 2 }
let result = apply(double, 5);

// Las macros NO se pueden pasar:
// apply(my_macro!, 5);  // ERROR: las macros no son valores
// let f = println!;     // ERROR: no se puede almacenar una macro
```

### 2. Ser llamadas dinámicamente

```rust
// En runtime puedes elegir qué función llamar:
let operations: Vec<fn(i32) -> i32> = vec![double, triple, negate];
for op in &operations {
    println!("{}", op(5));
}

// Con macros no puedes hacer esto:
// Las macros se resuelven en compilación, no en runtime
```

### 3. Implementar traits

```rust
// Una función puede satisfacer un trait:
trait Transformer {
    fn transform(&self, input: i32) -> i32;
}

struct Doubler;
impl Transformer for Doubler {
    fn transform(&self, input: i32) -> i32 { input * 2 }
}

// Las macros no implementan traits, no son tipos
```

### 4. Tener tipado verificable y documentable

```rust
// La firma de una función es clara y verificable:
fn parse_port(s: &str) -> Result<u16, ParseIntError> {
    s.parse()
}
// El compilador verifica que el caller pasa &str y maneja Result<u16, ...>
// La documentación muestra la firma

// Una macro no tiene firma:
macro_rules! parse_port {
    ($s:expr) => { $s.parse::<u16>().unwrap() };
}
// ¿Qué tipo acepta? ¿Qué retorna? Hay que leer la implementación
```

### 5. Autocompletado y tooling

Los IDEs entienden funciones mucho mejor que macros:
- **Funciones**: autocompletado de parámetros, go-to-definition, refactoring, inline hints.
- **Macros**: soporte limitado. `rust-analyzer` entiende muchas macros, pero no todas.

---

## 4. Árbol de decisión

```text
¿Necesitas hacer X?
│
├── Número variable de argumentos
│   ├── ¿Todos del mismo tipo? → función con slice: fn f(items: &[T])
│   ├── ¿Distintos tipos, pocos? → función con tupla o múltiples params
│   └── ¿Distintos tipos, variable? → MACRO
│
├── Generar structs/enums/impls
│   └── MACRO (o macro procedural / build script)
│
├── Capturar file!/line!/stringify!
│   └── MACRO
│
├── Evaluación lazy de argumentos
│   ├── ¿Se puede con closures? → fn f(msg: impl FnOnce() -> String)
│   └── ¿Necesitas sintaxis de format!? → MACRO
│
├── Comportamiento polimórfico
│   ├── ¿Basado en tipos? → función genérica con trait bounds
│   ├── ¿Basado en forma sintáctica? → MACRO
│   └── ¿Basado en runtime? → enum + match o dyn Trait
│
├── Reducir código repetitivo
│   ├── ¿Lógica con distintos tipos? → genéricos
│   ├── ¿Mismo patrón, distintos nombres? → MACRO
│   └── ¿Pocas repeticiones (2-3)? → copiar y pegar (en serio)
│
└── Ninguno de los anteriores → FUNCIÓN
```

---

## 5. Caso: número variable de argumentos

### Cuándo una función basta

Si todos los argumentos son del mismo tipo, usa un slice:

```rust
// Función: todos i32
fn sum(values: &[i32]) -> i32 {
    values.iter().sum()
}
let total = sum(&[1, 2, 3, 4, 5]);

// Función genérica: todos del mismo tipo T
fn print_all<T: Display>(values: &[T]) {
    for v in values {
        println!("{}", v);
    }
}
print_all(&[1, 2, 3]);
print_all(&["a", "b", "c"]);
```

### Cuándo necesitas una macro

Si los argumentos son de **distintos tipos** o la cantidad varía sin un contenedor natural:

```rust
// Cada argumento puede ser de tipo distinto:
macro_rules! print_all {
    ($($x:expr),* $(,)?) => {
        $( println!("{}", $x); )*
    };
}
print_all!(1, "hello", 3.14, true);
// No puedes hacer esto con un slice: &[???]
```

### Alternativa: tuplas (si es un número pequeño y fijo)

```rust
fn print_pair(a: impl Display, b: impl Display) {
    println!("{} {}", a, b);
}
print_pair(42, "hello");  // tipos distintos, pero exactamente 2
```

Para 2-4 argumentos de tipos distintos, múltiples funciones o impl Trait pueden bastar. Para N arbitrario, se necesita macro.

---

## 6. Caso: generación de código repetitivo

### El problema

```rust
// Implementar el mismo trait para 10 tipos numéricos:
impl MyTrait for u8 {
    fn process(&self) -> String { format!("u8: {}", self) }
}
impl MyTrait for u16 {
    fn process(&self) -> String { format!("u16: {}", self) }
}
impl MyTrait for u32 {
    fn process(&self) -> String { format!("u32: {}", self) }
}
// ... 7 más, idénticos excepto por el nombre del tipo
```

### Solución con genéricos (si es posible)

```rust
// Si la implementación es idéntica para todos los tipos:
impl<T: Display> MyTrait for T {
    fn process(&self) -> String { format!("{}", self) }
}
// Una sola implementación cubre todos los tipos que implementan Display
```

### Solución con macro (cuando genéricos no bastan)

A veces necesitas implementaciones ligeramente distintas o el trait system no permite la blanket impl:

```rust
macro_rules! impl_my_trait {
    ($($t:ty),*) => {
        $(
            impl MyTrait for $t {
                fn process(&self) -> String {
                    format!("{}: {}", stringify!($t), self)
                }
            }
        )*
    };
}

impl_my_trait!(u8, u16, u32, u64, u128, i8, i16, i32, i64, i128, f32, f64);
```

### Cuándo la macro es mejor

```rust
// La std usa este patrón extensivamente:
// Implementar From<u8> for u16, From<u8> for u32, etc.
// No se puede con una blanket impl por conflictos con coherencia

macro_rules! impl_from_for_wider {
    ($from:ty => $($to:ty),+) => {
        $(
            impl From<$from> for $to {
                fn from(val: $from) -> $to {
                    val as $to
                }
            }
        )+
    };
}

impl_from_for_wider!(u8 => u16, u32, u64, u128);
impl_from_for_wider!(u16 => u32, u64, u128);
```

### La regla

> Si puedes expresarlo con una blanket impl (`impl<T: Bound> Trait for T`), usa genéricos. Si necesitas **enumerar** los tipos, la macro es más práctica.

---

## 7. Caso: DSLs y sintaxis personalizada

Los **Domain-Specific Languages** dentro de Rust son territorio exclusivo de macros:

### HTML builder

```rust
macro_rules! html {
    ($tag:ident { $($inner:tt)* }) => {
        format!("<{}>{}</{}>", stringify!($tag), html!($($inner)*), stringify!($tag))
    };
    ($text:expr) => {
        $text.to_string()
    };
    () => {
        String::new()
    };
}

let page = html!(div { "Hello, world!" });
// "<div>Hello, world!</div>"
```

### SQL builder (simplificado)

```rust
macro_rules! sql {
    (SELECT $($col:ident),+ FROM $table:ident) => {
        format!(
            "SELECT {} FROM {}",
            vec![$( stringify!($col) ),+].join(", "),
            stringify!($table)
        )
    };
    (SELECT $($col:ident),+ FROM $table:ident WHERE $cond:expr) => {
        format!(
            "SELECT {} FROM {} WHERE {}",
            vec![$( stringify!($col) ),+].join(", "),
            stringify!($table),
            $cond
        )
    };
}

let q = sql!(SELECT name, age FROM users);
// "SELECT name, age FROM users"
```

### ¿Es un buen uso?

Los DSLs con macros son poderosos pero tienen costes:
- Los errores de compilación son difíciles de entender.
- Los IDEs no entienden la sintaxis personalizada.
- La curva de aprendizaje para contribuidores es alta.

**Úsalos solo cuando**: la sintaxis personalizada proporciona un beneficio significativo en legibilidad o seguridad (como `sqlx::query!` que verifica SQL contra la base de datos en compilación).

---

## 8. Debugging de macros

### El problema

Cuando una macro genera código incorrecto, el error del compilador apunta a la **invocación** de la macro, no al código generado:

```rust
macro_rules! make_fn {
    ($name:ident, $body:expr) => {
        fn $name() -> String {
            $body  // si $body no es String, el error es confuso
        }
    };
}

make_fn!(greet, 42);
// ERROR: expected `String`, found `{integer}`
//  --> src/main.rs:8:1
//   |
// 8 | make_fn!(greet, 42);
//   | ^^^^^^^^^^^^^^^^^^^^ expected `String`, found `{integer}`
```

### Herramientas de debugging

#### 1. cargo expand (la mejor herramienta)

```bash
cargo expand
# Muestra el código después de expansión de macros
# Puedes ver exactamente qué generó la macro
```

#### 2. trace_macros! (nightly)

```rust
#![feature(trace_macros)]

trace_macros!(true);
let v = vec![1, 2, 3];
trace_macros!(false);

// Output en compilación:
// note: trace_macro
//   --> src/main.rs:4:9
//    |
// 4  |     let v = vec![1, 2, 3];
//    |             ^^^^^^^^^^^^^^
//    |
//    = note: expanding `vec! { 1, 2, 3 }`
//    = note: to `<[_]>::into_vec(Box::new([1, 2, 3]))`
```

#### 3. log_syntax! (nightly)

```rust
#![feature(log_syntax)]

macro_rules! debug_macro {
    ($($x:expr),*) => {
        log_syntax!($($x),*);  // imprime los tokens capturados durante compilación
        $( println!("{}", $x); )*
    };
}
```

#### 4. Compilar con macro expandida manualmente

La técnica más básica: expande la macro a mano, pega el resultado, y compila para encontrar el error:

```rust
// En vez de:
// make_fn!(greet, 42);

// Expande manualmente:
fn greet() -> String {
    42  // ← ahora el error es claro: 42 no es String
}
```

#### 5. compile_error! para validar branches

```rust
macro_rules! config {
    (port = $val:expr) => { /* ok */ };
    (host = $val:expr) => { /* ok */ };
    ($key:ident = $val:expr) => {
        compile_error!(concat!(
            "Unknown config key: `",
            stringify!($key),
            "`. Valid keys: port, host"
        ));
    };
}

// config!(unknown = 42);
// ERROR: "Unknown config key: `unknown`. Valid keys: port, host"
```

---

## 9. Errores crípticos y cómo mejorarlos

### Problema: errores que apuntan a la invocación

```rust
macro_rules! create_map {
    ($($k:expr => $v:expr),*) => {
        {
            let mut m = std::collections::HashMap::new();
            $( m.insert($k, $v); )*
            m
        }
    };
}

// Tipos incompatibles:
let m = create_map!("a" => 1, "b" => "two");
// ERROR: expected integer, found `&str`
// El error apunta a create_map!(...), no a "two"
```

### Estrategia 1: tipo explícito en la macro

```rust
macro_rules! create_map {
    ($($k:expr => $v:expr),*) => {
        {
            let mut m = std::collections::HashMap::new();
            $(
                // Si los tipos no coinciden, el error apunta a insert
                // que es más informativo
                let _: () = {
                    m.insert($k, $v);
                };
            )*
            m
        }
    };
}
```

### Estrategia 2: funciones helper

Delegar la lógica a una función — los errores de la función son más claros:

```rust
fn insert_into<K: Eq + std::hash::Hash, V>(
    map: &mut std::collections::HashMap<K, V>,
    key: K,
    value: V,
) {
    map.insert(key, value);
}

macro_rules! create_map {
    ($($k:expr => $v:expr),*) => {
        {
            let mut m = std::collections::HashMap::new();
            $( insert_into(&mut m, $k, $v); )*
            m
        }
    };
}
// Ahora los errores de tipo apuntan a insert_into, con tipos claros
```

### Estrategia 3: documentar con ejemplos

```rust
/// Creates a HashMap from key-value pairs.
///
/// # Examples
/// ```
/// let m = create_map!("a" => 1, "b" => 2);
/// assert_eq!(m["a"], 1);
/// ```
///
/// # Compile errors
/// All keys must be the same type, and all values must be the same type:
/// ```compile_fail
/// let m = create_map!("a" => 1, "b" => "two"); // ERROR: mixed value types
/// ```
macro_rules! create_map { /* ... */ }
```

---

## 10. Alternativas a macros

Antes de escribir una macro, considera estas alternativas:

### Genéricos en lugar de macros para tipos

```rust
// MACRO (innecesaria):
macro_rules! new_wrapper {
    ($name:ident, $inner:ty) => {
        struct $name($inner);
        impl $name {
            fn new(val: $inner) -> Self { $name(val) }
            fn value(&self) -> &$inner { &self.0 }
        }
    };
}

// GENÉRICO (mejor si la lógica es idéntica):
struct Wrapper<T>(T);
impl<T> Wrapper<T> {
    fn new(val: T) -> Self { Wrapper(val) }
    fn value(&self) -> &T { &self.0 }
}
```

### Closures en lugar de macros para lazy evaluation

```rust
// MACRO:
macro_rules! log_if_debug {
    ($msg:expr) => {
        if cfg!(debug_assertions) {
            eprintln!("{}", $msg);
        }
    };
}

// CLOSURE (si no necesitas format syntax):
fn log_if_debug(msg: impl FnOnce() -> String) {
    if cfg!(debug_assertions) {
        eprintln!("{}", msg());
    }
}
log_if_debug(|| format!("expensive: {}", compute()));
```

### Builder pattern en lugar de macro DSL

```rust
// MACRO DSL:
macro_rules! server {
    (port: $p:expr, host: $h:expr, workers: $w:expr) => {
        Server { port: $p, host: $h.to_string(), workers: $w }
    };
}

// BUILDER (mejor tooling, mejor errors):
let server = Server::builder()
    .port(8080)
    .host("localhost")
    .workers(4)
    .build();
```

### Trait con default methods en lugar de macro para impls

```rust
// MACRO:
macro_rules! impl_printable {
    ($t:ty) => {
        impl Printable for $t {
            fn print(&self) { println!("{}", self); }
        }
    };
}

// TRAIT CON DEFAULT (si el método es idéntico para todos):
trait Printable: Display {
    fn print(&self) {
        println!("{}", self);
    }
}
// Blanket impl:
impl<T: Display> Printable for T {}
```

### Build scripts para generación de código

Si necesitas generar muchos archivos o el código depende de datos externos:

```rust
// build.rs — genera código antes de compilar
fn main() {
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let path = std::path::Path::new(&out_dir).join("generated.rs");

    let code = generate_from_schema("schema.json");
    std::fs::write(path, code).unwrap();
}

// src/lib.rs
include!(concat!(env!("OUT_DIR"), "/generated.rs"));
```

---

## 11. Errores comunes

### Error 1: Escribir una macro cuando una función genérica basta

```rust
// MACRO innecesaria:
macro_rules! max {
    ($a:expr, $b:expr) => {
        if $a > $b { $a } else { $b }
    };
}
// Problema: $a se evalúa DOS veces si se usa en ambas ramas

// FUNCIÓN (correcta, clara, sin doble evaluación):
fn max<T: PartialOrd>(a: T, b: T) -> T {
    if a > b { a } else { b }
}
// O simplemente: std::cmp::max(a, b)
```

La doble evaluación en macros es un bug clásico:

```rust
let result = max!(expensive_compute(), 0);
// Se expande a: if expensive_compute() > 0 { expensive_compute() } else { 0 }
// expensive_compute() se llama DOS veces
```

### Error 2: Macro excesivamente compleja

```rust
// Si tu macro tiene más de ~30 líneas, necesitas un paso atrás
macro_rules! super_complex {
    // 15 ramas con repeticiones anidadas y recursión...
    // Nadie puede leer ni mantener esto
}

// Solución: dividir en macros helper + funciones
macro_rules! outer {
    ($($x:tt)*) => {
        helper_fn(inner_macro!($($x)*))
    };
}
```

### Error 3: No testear la macro con distintas combinaciones

```rust
macro_rules! my_vec {
    ($($x:expr),*) => {
        { let mut v = Vec::new(); $( v.push($x); )* v }
    };
}

// Testear TODAS las formas:
let empty: Vec<i32> = my_vec![];          // cero args
let one = my_vec![1];                     // un arg
let many = my_vec![1, 2, 3, 4, 5];       // muchos args
// let trailing = my_vec![1, 2,];         // ¿funciona trailing comma?
// ¡Probablemente no! Falta $(,)? en el patrón
```

### Error 4: Olvidar que las macros no son type-safe

```rust
macro_rules! add {
    ($a:expr, $b:expr) => { $a + $b };
}

// Compila (la macro no verifica tipos):
let x = add!("hello", "world");  // concatenación de &str... ¡que no compila en +!
// El error aparece DESPUÉS de la expansión, y puede ser confuso
```

### Error 5: Reinventar macros de la std

```rust
// No escribas:
macro_rules! my_assert_eq {
    ($a:expr, $b:expr) => {
        if $a != $b { panic!("not equal"); }
    };
}

// Usa: assert_eq!($a, $b) — ya existe, es mejor, muestra valores

// No escribas:
macro_rules! my_dbg {
    ($val:expr) => {
        eprintln!("{:?}", $val);
    };
}

// Usa: dbg!($val) — ya existe, muestra file:line, retorna el valor
```

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║               MACRO vs FUNCIÓN: DECISIÓN CHEATSHEET                ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  USA FUNCIÓN CUANDO                                                ║
║  ✓ Los argumentos son de tipos conocidos                           ║
║  ✓ La aridad es fija o puedes usar slices                          ║
║  ✓ No necesitas generar items (struct, impl, fn)                   ║
║  ✓ No necesitas información del call site (file, line)             ║
║  ✓ El tooling (autocomplete, refactor) importa                     ║
║  ✓ Siempre que puedas                                              ║
║                                                                    ║
║  USA GENÉRICOS CUANDO                                              ║
║  ✓ La misma lógica aplica a múltiples tipos                        ║
║  ✓ Puedes expresar los requisitos con trait bounds                 ║
║  ✓ Blanket impl cubre todos los tipos necesarios                   ║
║                                                                    ║
║  USA MACRO CUANDO                                                  ║
║  ✓ Necesitas aridad variable con tipos heterogéneos                ║
║  ✓ Generas structs, enums, funciones, impls                        ║
║  ✓ Necesitas file!(), line!(), stringify!()                        ║
║  ✓ Necesitas evaluación lazy con sintaxis format!                  ║
║  ✓ Implementas un trait para una lista enumerada de tipos          ║
║  ✓ Creas un DSL con beneficio claro de legibilidad/seguridad       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TRAMPAS DE MACROS                                                 ║
║  ✗ Doble evaluación: $x se evalúa cada vez que aparece             ║
║  ✗ Errores apuntan a la invocación, no al código generado          ║
║  ✗ Sin type-checking hasta después de la expansión                 ║
║  ✗ IDEs tienen soporte limitado                                    ║
║  ✗ Difíciles de debuggear sin cargo expand                         ║
║  ✗ Curva de aprendizaje alta para mantenedores                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DEBUGGING DE MACROS                                               ║
║  1. cargo expand          Ver código expandido                     ║
║  2. Expandir manualmente  Copiar resultado, compilar aparte        ║
║  3. compile_error!        Validar ramas en compilación             ║
║  4. Funciones helper      Mover lógica fuera de la macro           ║
║  5. trace_macros!         Ver expansión paso a paso (nightly)      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ALTERNATIVAS A MACROS                                             ║
║  Genéricos + traits       Para polimorfismo basado en tipos        ║
║  Closures                 Para evaluación lazy                     ║
║  Builder pattern          Para configuración con muchas opciones   ║
║  Default methods          Para impls idénticas                     ║
║  Build scripts            Para generación masiva de código         ║
║  Copiar y pegar           Para 2-3 repeticiones (en serio)         ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: ¿Macro o función?

Para cada caso, decide si necesitas una macro o si basta una función (genérica o no). Implementa la solución correcta:

```rust
// A) Función que retorna el mayor de dos valores de cualquier tipo comparable.
//    ¿Macro o función genérica?

// B) "Función" que acepta 1, 2, o N strings y las concatena con ", ".
//    ¿Puedes hacerlo sin macro? ¿Con slice? ¿Con macro?

// C) Generar structs Point2D, Point3D, Point4D que tengan campos
//    f64 llamados (x,y), (x,y,z), (x,y,z,w) respectivamente.
//    ¿Macro o genéricos?

// D) Logging que muestre archivo y línea donde se llamó.
//    ¿Macro o función?

// E) Función que aplica una operación a cada elemento de un slice
//    y retorna un Vec con los resultados.
//    ¿Macro o función genérica?

// F) Implementar Display para 5 enums distintos donde cada variante
//    simplemente imprime su nombre.
//    ¿Macro o implementación manual? ¿O derive con un crate?
```

### Ejercicio 2: Refactorizar macro a función

Esta macro funciona pero debería ser una función. Refactorízala:

```rust
macro_rules! clamp {
    ($val:expr, $min:expr, $max:expr) => {
        if $val < $min {
            $min
        } else if $val > $max {
            $max
        } else {
            $val
        }
    };
}

fn main() {
    println!("{}", clamp!(15, 0, 10));   // 10
    println!("{}", clamp!(-5, 0, 10));   // 0
    println!("{}", clamp!(5, 0, 10));    // 5
}
```

**Tareas:**
1. Identifica el bug de doble evaluación. ¿Qué pasa con `clamp!(expensive(), 0, 10)`?
2. Escribe una función genérica `clamp` equivalente. ¿Qué trait bounds necesitas?
3. ¿Existe `clamp` en la std? ¿Dónde?
4. ¿Hay algún caso donde la versión macro sería preferible a la función?

### Ejercicio 3: Macro justificada

Escribe una macro `timed!` que ejecute un bloque de código y reporte cuánto tardó, incluyendo el código fuente y la ubicación:

```rust
// Uso esperado:
let result = timed!({
    let mut sum = 0;
    for i in 0..1_000_000 {
        sum += i;
    }
    sum
});
// Debe imprimir algo como:
// [src/main.rs:5] { let mut sum = 0; ... } took 2.34ms → 499999500000
```

**Requisitos:**
1. Imprime archivo y línea (`file!`, `line!`).
2. Imprime una representación del código (`stringify!`).
3. Mide el tiempo con `std::time::Instant`.
4. **Retorna** el valor del bloque (como `dbg!`).

**Preguntas:**
1. ¿Por qué esto DEBE ser una macro y no una función?
2. Si eliminas los requisitos de `file!`/`line!`/`stringify!`, ¿podrías hacerlo con una closure? Escribe la versión con closure.
3. ¿Cuál es más legible en el punto de uso — la macro o la closure?

---

> **Nota**: la tentación de usar macros crece con la experiencia — una vez que entiendes `macro_rules!`, empiezas a ver patrones en todas partes. Resiste la tentación. Las mejores macros son las que **no escribes** — porque encontraste una forma más simple con funciones, genéricos, o traits. Las macros que sí escribes deberían sentirse como la **única** opción razonable, no como la primera.
