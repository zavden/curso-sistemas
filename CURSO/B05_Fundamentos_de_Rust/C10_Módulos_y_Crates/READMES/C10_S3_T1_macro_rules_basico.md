# macro_rules! básico: sintaxis, matchers y repetición

## Índice
1. [Qué son las macros declarativas](#1-qué-son-las-macros-declarativas)
2. [Sintaxis de macro_rules!](#2-sintaxis-de-macro_rules)
3. [Fragment specifiers (matchers)](#3-fragment-specifiers-matchers)
4. [Múltiples ramas (arms)](#4-múltiples-ramas-arms)
5. [Repetición con $(...)*](#5-repetición-con-)
6. [Repetición anidada](#6-repetición-anidada)
7. [Macros que generan items](#7-macros-que-generan-items)
8. [Higiene de macros](#8-higiene-de-macros)
9. [Scope y visibilidad](#9-scope-y-visibilidad)
10. [Macros recursivas](#10-macros-recursivas)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué son las macros declarativas

Las macros declarativas (`macro_rules!`) son **transformaciones de código en compilación**. Reciben tokens de entrada, los emparejan con patrones, y producen nuevo código Rust que el compilador inserta en su lugar:

```rust
// Definir una macro:
macro_rules! say_hello {
    () => {
        println!("Hello, world!");
    };
}

// Usar la macro:
fn main() {
    say_hello!();  // se expande a: println!("Hello, world!");
}
```

### Macros vs funciones

| | Funciones | Macros |
|---|---|---|
| Se ejecutan en | Runtime | Compilación (expansión) |
| Reciben | Valores tipados | Tokens (fragmentos de código) |
| Pueden generar | Valores | Cualquier código Rust válido |
| Número de argumentos | Fijo | Variable |
| Pueden crear | Nada nuevo | Structs, funciones, impls, módulos |
| Debugging | Claro | Puede ser confuso |

Las macros existen para hacer cosas que las funciones **no pueden**: aceptar cantidad variable de argumentos, generar código repetitivo, crear nuevos items (structs, impls), y trabajar con patrones sintácticos.

---

## 2. Sintaxis de macro_rules!

### Estructura básica

```rust
macro_rules! nombre_macro {
    // patrón => { código expandido };
    ( patrón1 ) => {
        // código que se genera cuando el input coincide con patrón1
    };
    ( patrón2 ) => {
        // código alternativo
    };
}
```

- Los **patrones** (matchers) describen qué forma de input acepta la macro.
- Las **llaves de expansión** contienen el código Rust que se genera.
- Se pueden tener múltiples ramas, separadas por `;`.
- La macro se invoca con `nombre_macro!(...)`, `nombre_macro![...]`, o `nombre_macro!{...}`.

### Los tres estilos de invocación

```rust
macro_rules! my_macro {
    ($val:expr) => { println!("{}", $val) };
}

// Los tres son equivalentes:
my_macro!("hello");     // paréntesis — más común para expresiones
my_macro!["hello"];     // corchetes — convención para "crear colección" (vec![])
my_macro!{"hello"}      // llaves — convención para "generar items" (struct, impl)
// Nota: con llaves no lleva punto y coma al final
```

### Macro más simple posible

```rust
macro_rules! noop {
    () => {};  // no recibe nada, no produce nada
}

noop!();  // legal, no hace nada
```

---

## 3. Fragment specifiers (matchers)

Los **fragment specifiers** le dicen a la macro qué tipo de fragmento de código esperar. Se escriben como `$nombre:tipo`:

```rust
macro_rules! example {
    ($x:expr) => {     // $x captura una expresión
        println!("{}", $x);
    };
}

example!(2 + 3);       // $x = "2 + 3"
example!("hello");     // $x = "\"hello\""
example!(vec![1,2,3]); // $x = "vec![1,2,3]"
```

### Tabla de fragment specifiers

| Specifier | Captura | Ejemplo |
|-----------|---------|---------|
| `$x:expr` | Cualquier expresión | `2 + 3`, `"hello"`, `foo()`, `if a { b } else { c }` |
| `$x:ident` | Un identificador | `foo`, `my_var`, `String` |
| `$x:ty` | Un tipo | `i32`, `Vec<String>`, `&str`, `Box<dyn Trait>` |
| `$x:pat` | Un patrón | `Some(x)`, `_`, `1..=5`, `(a, b)` |
| `$x:stmt` | Una sentencia | `let x = 5`, `x += 1` |
| `$x:block` | Un bloque | `{ let x = 1; x + 2 }` |
| `$x:item` | Un item | `fn foo() {}`, `struct Bar;`, `impl Trait for X {}` |
| `$x:path` | Una ruta | `std::io::Read`, `crate::models::User` |
| `$x:literal` | Un literal | `42`, `"text"`, `true`, `3.14` |
| `$x:lifetime` | Un lifetime | `'a`, `'static` |
| `$x:meta` | Contenido de atributo | `derive(Debug)`, `allow(unused)` |
| `$x:vis` | Visibilidad (puede ser vacío) | `pub`, `pub(crate)`, ε (vacío) |
| `$x:tt` | Un **token tree** | Cualquier token individual o grupo `(...)`, `[...]`, `{...}` |

### Ejemplos de cada specifier

```rust
// $x:ident — captura un identificador
macro_rules! create_function {
    ($name:ident) => {
        fn $name() {
            println!("Called {}", stringify!($name));
        }
    };
}
create_function!(hello);
// Genera: fn hello() { println!("Called hello"); }

// $x:ty — captura un tipo
macro_rules! type_of {
    ($t:ty) => {
        std::any::type_name::<$t>()
    };
}
let name = type_of!(Vec<String>);

// $x:expr — captura una expresión
macro_rules! double {
    ($e:expr) => {
        $e * 2
    };
}
let x = double!(5 + 3);  // se expande a: (5 + 3) * 2 = 16
// Nota: la macro envuelve $e en paréntesis implícitos

// $x:literal — solo literales (más restrictivo que expr)
macro_rules! greet {
    ($name:literal) => {
        println!("Hello, {}!", $name);
    };
}
greet!("Alice");   // OK
// greet!(name);   // ERROR: name no es un literal

// $x:pat — captura un patrón
macro_rules! match_it {
    ($val:expr, $pat:pat => $body:expr) => {
        match $val {
            $pat => $body,
            _ => panic!("no match"),
        }
    };
}
match_it!(Some(42), Some(x) => x);  // retorna 42
```

### $x:tt — el comodín universal

`tt` (token tree) captura **cualquier token individual** o un grupo delimitado. Es el specifier más flexible:

```rust
macro_rules! print_tt {
    ($t:tt) => {
        println!("{}", stringify!($t));
    };
}

print_tt!(hello);     // un ident es un tt
print_tt!(42);        // un literal es un tt
print_tt!((a, b));    // un grupo (...) es un tt
print_tt!([1, 2]);    // un grupo [...] es un tt
```

`tt` es útil cuando necesitas pasar código "opaco" que la macro no necesita interpretar.

---

## 4. Múltiples ramas (arms)

Una macro puede tener múltiples patrones. Se prueban **en orden**, de arriba a abajo, y la primera coincidencia gana:

```rust
macro_rules! describe {
    // Rama 1: sin argumentos
    () => {
        println!("Nothing to describe");
    };
    // Rama 2: una expresión
    ($val:expr) => {
        println!("Value: {}", $val);
    };
    // Rama 3: dos expresiones
    ($a:expr, $b:expr) => {
        println!("Pair: {} and {}", $a, $b);
    };
}

describe!();           // "Nothing to describe"
describe!(42);         // "Value: 42"
describe!(1, 2);       // "Pair: 1 and 2"
```

### Delimitadores literales en patrones

Los patrones pueden incluir tokens literales (palabras clave, puntuación) que deben aparecer exactamente:

```rust
macro_rules! hash_map {
    // Patrón: clave => valor, separados por comas
    ( $($key:expr => $val:expr),* ) => {
        {
            let mut map = std::collections::HashMap::new();
            $( map.insert($key, $val); )*
            map
        }
    };
}

let ages = hash_map! {
    "Alice" => 30,
    "Bob" => 25
};
```

Los tokens `=>` y `,` son literales en el patrón — el usuario debe escribirlos.

### Ramas con keywords personalizadas

```rust
macro_rules! command {
    (run $name:ident) => {
        $name();
    };
    (print $val:expr) => {
        println!("{}", $val);
    };
    (create $name:ident : $t:ty = $val:expr) => {
        let $name: $t = $val;
    };
}

fn greet() { println!("Hello!"); }

command!(run greet);                    // greet();
command!(print "hello");                // println!("hello");
command!(create x : i32 = 42);         // let x: i32 = 42;
```

---

## 5. Repetición con $(...)*

La repetición permite que una macro acepte una **cantidad variable** de argumentos:

### Sintaxis

```text
$( patrón )separador*      cero o más repeticiones
$( patrón )separador+      una o más repeticiones
$( patrón )?               cero o una (opcional)
```

El **separador** es un token que aparece entre repeticiones (típicamente `,` o `;`).

### Ejemplo básico: cero o más

```rust
macro_rules! print_all {
    // $( ... ),* → captura cero o más expresiones separadas por coma
    ( $( $x:expr ),* ) => {
        $(
            println!("{}", $x);
        )*
        // El bloque $( ... )* se repite por cada $x capturado
    };
}

print_all!();                    // nada (cero repeticiones)
print_all!(1);                   // println!("{}", 1);
print_all!(1, 2, 3);            // tres println!
print_all!("a", "b", "c", "d"); // cuatro println!
```

### Una o más con +

```rust
macro_rules! sum {
    // Al menos un argumento:
    ( $first:expr $(, $rest:expr )* ) => {
        {
            let mut total = $first;
            $( total += $rest; )*
            total
        }
    };
}

let s = sum!(1);           // 1
let s = sum!(1, 2, 3, 4);  // 10
// sum!();                  // ERROR: no coincide con ningún patrón
```

### Opcional con ?

```rust
macro_rules! log {
    // Mensaje obligatorio, nivel opcional
    ( $msg:expr $(, level = $lvl:expr )? ) => {
        $(
            print!("[{}] ", $lvl);
        )?
        println!("{}", $msg);
    };
}

log!("hello");                   // "hello"
log!("hello", level = "WARN");   // "[WARN] hello"
```

### Trailing comma

Un patrón útil es aceptar opcionalmente una coma final:

```rust
macro_rules! my_vec {
    // $(,)? al final acepta una coma trailing opcional
    ( $( $x:expr ),* $(,)? ) => {
        {
            let mut v = Vec::new();
            $( v.push($x); )*
            v
        }
    };
}

let a = my_vec![1, 2, 3];    // sin trailing comma
let b = my_vec![1, 2, 3,];   // con trailing comma — ambos OK
```

---

## 6. Repetición anidada

Las repeticiones pueden anidarse para patrones más complejos:

```rust
macro_rules! matrix {
    // Filas separadas por ; , elementos dentro de cada fila separados por ,
    ( $( [ $( $elem:expr ),* ] );* ) => {
        vec![
            $( vec![ $( $elem ),* ] ),*
        ]
    };
}

let m = matrix![
    [1, 2, 3];
    [4, 5, 6];
    [7, 8, 9]
];
// Genera: vec![vec![1,2,3], vec![4,5,6], vec![7,8,9]]
```

### Múltiples capturas en la misma repetición

Cada variable capturada en una repetición debe usarse en una repetición del mismo nivel:

```rust
macro_rules! key_values {
    ( $( $key:expr => $val:expr ),* ) => {
        {
            let mut map = std::collections::HashMap::new();
            // $key y $val se repiten juntos — capturados en la misma $(...)*
            $( map.insert($key, $val); )*
            map
        }
    };
}

let m = key_values!("a" => 1, "b" => 2, "c" => 3);
```

---

## 7. Macros que generan items

Las macros no se limitan a expresiones — pueden generar structs, enums, funciones, impls:

### Generar structs

```rust
macro_rules! make_struct {
    ($name:ident { $( $field:ident : $ty:ty ),* $(,)? }) => {
        #[derive(Debug)]
        pub struct $name {
            $( pub $field: $ty, )*
        }
    };
}

make_struct!(Point { x: f64, y: f64 });
make_struct!(Color { r: u8, g: u8, b: u8 });

// Genera:
// #[derive(Debug)]
// pub struct Point { pub x: f64, pub y: f64 }
// etc.
```

### Generar impls

```rust
macro_rules! impl_display {
    ($t:ty, $fmt:expr) => {
        impl std::fmt::Display for $t {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                write!(f, $fmt, self)
            }
        }
    };
}

struct Meters(f64);
impl_display!(Meters, "{:.2}m");
// No compila directamente así — es un ejemplo conceptual
// El patrón real necesita más control sobre los campos
```

### Generar enums con métodos

```rust
macro_rules! define_errors {
    ( $( $variant:ident => $msg:expr ),* $(,)? ) => {
        #[derive(Debug)]
        pub enum AppError {
            $( $variant, )*
        }

        impl std::fmt::Display for AppError {
            fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
                match self {
                    $( AppError::$variant => write!(f, $msg), )*
                }
            }
        }
    };
}

define_errors! {
    NotFound => "Resource not found",
    Unauthorized => "Unauthorized access",
    Timeout => "Request timed out",
}

// Genera el enum Y su impl Display
```

### Generar funciones

```rust
macro_rules! make_test {
    ($name:ident, $body:expr) => {
        #[test]
        fn $name() {
            assert!($body);
        }
    };
}

make_test!(test_positive, 5 > 0);
make_test!(test_nonempty, !String::new().is_empty() == false);
```

---

## 8. Higiene de macros

Rust tiene **macros higiénicas** — las variables definidas dentro de una macro no colisionan con variables del código que la invoca:

```rust
macro_rules! make_x {
    () => {
        let x = 42;  // este 'x' es distinto del 'x' exterior
    };
}

fn main() {
    let x = 10;
    make_x!();
    println!("{}", x);  // imprime 10, no 42
    // El 'x' de la macro vive en su propio "contexto higiénico"
}
```

### Pasar variables explícitamente

Si necesitas que la macro interactúe con variables del contexto, pásalas como argumentos:

```rust
macro_rules! set_var {
    ($var:ident, $val:expr) => {
        $var = $val;  // $var se refiere al identificador pasado por el usuario
    };
}

fn main() {
    let mut x = 0;
    set_var!(x, 42);
    println!("{}", x);  // 42
}
```

### Límites de la higiene

La higiene se aplica a **variables locales**, pero no a items como funciones, structs, o tipos:

```rust
macro_rules! define_greet {
    () => {
        fn greet() {
            println!("Hello!");
        }
    };
}

fn main() {
    define_greet!();
    greet();  // funciona: las funciones generadas SÍ son visibles
}
```

---

## 9. Scope y visibilidad

### Macros definidas en el mismo crate

Las macros siguen un orden textual — deben definirse **antes** de usarse en el mismo archivo:

```rust
// ERROR: macro usada antes de definirse
// say_hello!();

macro_rules! say_hello {
    () => { println!("Hello!"); };
}

say_hello!();  // OK: definida arriba
```

### #[macro_export]: macros públicas de una library

Para que una macro sea accesible desde otros crates, usa `#[macro_export]`:

```rust
// src/lib.rs de mi_crate
#[macro_export]
macro_rules! my_vec {
    ( $( $x:expr ),* $(,)? ) => {
        {
            let mut v = Vec::new();
            $( v.push($x); )*
            v
        }
    };
}
```

```rust
// En otro crate:
use mi_crate::my_vec;  // edición 2018+

let v = my_vec![1, 2, 3];
```

**Importante**: `#[macro_export]` coloca la macro en la **raíz** del crate, independientemente de en qué módulo se definió:

```rust
// src/utils.rs
#[macro_export]
macro_rules! helper {
    () => {};
}

// Desde otro crate:
use mi_crate::helper;  // NO mi_crate::utils::helper
// #[macro_export] siempre exporta a la raíz del crate
```

### macro_use en módulos

Dentro del mismo crate, para que una macro definida en un módulo esté disponible en otros, usa `#[macro_use]`:

```rust
// Opción 1: #[macro_use] en el módulo
#[macro_use]
mod utils {
    macro_rules! log {
        ($msg:expr) => { println!("[LOG] {}", $msg); };
    }
}

// log! ahora disponible aquí y en todos los módulos posteriores
log!("hello");
```

En la edición 2018+, también puedes simplemente definir la macro antes de los módulos que la usan, gracias al orden textual dentro de un mismo crate.

---

## 10. Macros recursivas

Las macros pueden invocarse a sí mismas para procesar argumentos de forma recursiva:

```rust
macro_rules! count {
    // Caso base: sin argumentos
    () => { 0usize };
    // Caso recursivo: un elemento + el resto
    ($first:tt $( $rest:tt )*) => {
        1usize + count!($( $rest )*)
    };
}

let n = count!(a b c d e);  // 5
// Expansión:
// 1 + count!(b c d e)
// 1 + 1 + count!(c d e)
// 1 + 1 + 1 + count!(d e)
// 1 + 1 + 1 + 1 + count!(e)
// 1 + 1 + 1 + 1 + 1 + count!()
// 1 + 1 + 1 + 1 + 1 + 0 = 5
```

### Ejemplo: stringify list

```rust
macro_rules! join_strings {
    // Caso base: un solo elemento
    ($single:expr) => {
        $single.to_string()
    };
    // Recursivo: primer elemento + separador + resto
    ($first:expr, $( $rest:expr ),+) => {
        format!("{}, {}", $first, join_strings!($( $rest ),+))
    };
}

let s = join_strings!("a", "b", "c");
// "a, b, c"
```

### Límite de recursión

Rust tiene un límite de recursión de macros (128 por defecto) para evitar expansiones infinitas:

```rust
// Si necesitas más:
#![recursion_limit = "256"]
```

En la práctica, si necesitas más de 128 niveles de recursión, probablemente hay una forma mejor de estructurar tu macro.

---

## 11. Errores comunes

### Error 1: Variable no usada en la misma profundidad de repetición

```rust
macro_rules! bad {
    ( $( $key:expr => $val:expr ),* ) => {
        // ERROR: $key y $val están a profundidad 1,
        // pero se usan fuera de un $(...)*
        println!("{}", $key);  // ¿cuál $key? hay cero o más
    };
}
```

**Solución**: usar las variables dentro de un bloque de repetición `$(...)*`:

```rust
macro_rules! good {
    ( $( $key:expr => $val:expr ),* ) => {
        $( println!("{} = {}", $key, $val); )*
    };
}
```

### Error 2: Ambigüedad en el patrón

```rust
macro_rules! ambiguous {
    ($($x:expr),* $y:expr) => {};
    // ¿Dónde termina la repetición $x y empieza $y?
    // El parser no puede decidir
}
```

**Solución**: usar un separador claro entre las partes:

```rust
macro_rules! clear {
    ($($x:expr),* ; $y:expr) => {};
    // El ; separa claramente la repetición del argumento final
}
```

### Error 3: Tipo no coincide con specifier

```rust
macro_rules! take_type {
    ($t:ty) => { std::mem::size_of::<$t>() };
}

// OK:
take_type!(i32);
take_type!(Vec<String>);

// ERROR: un valor no es un tipo
// take_type!(42);  // 42 es expr, no ty
```

Cada fragment specifier es estricto — `$x:ty` solo acepta tipos, `$x:expr` solo expresiones.

### Error 4: Olvidar stringify! para debug

```rust
macro_rules! debug_val {
    ($val:expr) => {
        // Solo imprime el valor, no sabes QUÉ expresión era
        println!("= {}", $val);
    };
}

debug_val!(2 + 3);  // imprime "= 5" — ¿qué era?
```

**Solución**: usar `stringify!` para obtener el texto del código:

```rust
macro_rules! debug_val {
    ($val:expr) => {
        println!("{} = {}", stringify!($val), $val);
    };
}

debug_val!(2 + 3);  // imprime "2 + 3 = 5"
```

`stringify!` convierte tokens en un string literal en compilación. Es como funciona `dbg!` internamente.

### Error 5: Macro que genera expresión sin bloque envolvente

```rust
macro_rules! bad_swap {
    ($a:ident, $b:ident) => {
        let temp = $a;
        $a = $b;
        $b = temp;
    };
}

// Problema: si se usa como una expresión o en if sin llaves,
// las sentencias sueltas causan errores
```

**Solución**: envolver en un bloque:

```rust
macro_rules! swap {
    ($a:ident, $b:ident) => {
        {
            let temp = $a;
            $a = $b;
            $b = temp;
        }
    };
}
```

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                   macro_rules! CHEATSHEET                          ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ESTRUCTURA                                                        ║
║  macro_rules! nombre {                                             ║
║      (patrón1) => { expansión1 };                                  ║
║      (patrón2) => { expansión2 };                                  ║
║  }                                                                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  FRAGMENT SPECIFIERS                                               ║
║  $x:expr       Expresión        2+3, foo(), "hello"                ║
║  $x:ident      Identificador    foo, my_var                        ║
║  $x:ty         Tipo             i32, Vec<T>, &str                  ║
║  $x:pat        Patrón           Some(x), _, 1..=5                  ║
║  $x:stmt       Sentencia        let x = 5                          ║
║  $x:block      Bloque           { expr }                           ║
║  $x:item       Item             fn, struct, impl                   ║
║  $x:path       Ruta             std::io::Read                      ║
║  $x:literal    Literal          42, "text", true                   ║
║  $x:lifetime   Lifetime         'a, 'static                        ║
║  $x:meta       Attr content     derive(Debug)                      ║
║  $x:vis        Visibilidad      pub, pub(crate), (vacío)           ║
║  $x:tt         Token tree       cualquier token o grupo            ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REPETICIÓN                                                        ║
║  $( $x:expr ),*        Cero o más, separados por coma              ║
║  $( $x:expr ),+        Una o más, separados por coma               ║
║  $( $x:expr )?         Cero o una (opcional)                       ║
║  $( $x:expr );*        Con punto y coma como separador             ║
║                                                                    ║
║  En la expansión:                                                  ║
║  $( código con $x )*   Se repite por cada $x capturado             ║
║                                                                    ║
║  Trailing comma: $( $x:expr ),* $(,)?                              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  VISIBILIDAD Y EXPORT                                              ║
║  #[macro_export]        Pública para otros crates (va a la raíz)   ║
║  #[macro_use] mod ...   Disponible en módulos posteriores          ║
║  use crate::macro;      Import en edición 2018+                    ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  UTILIDADES                                                        ║
║  stringify!($x)         Tokens → string literal ("2 + 3")         ║
║  concat!("a", "b")     Concatenar literals → "ab"                  ║
║  file!(), line!()       Ubicación del código fuente                 ║
║  compile_error!("msg")  Forzar error de compilación                ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Macros básicas

Implementa las siguientes macros y verifica que funcionen:

```rust
// A) Macro que crea una constante con su nombre como string
// Uso: make_const!(PI, f64, 3.14159);
// Genera: const PI: f64 = 3.14159;
//         fn PI_name() -> &'static str { "PI" }
macro_rules! make_const {
    // Tu implementación aquí
    // Pista: usa $name:ident, $ty:ty, $val:expr
    // Pista: usa stringify!($name) para el nombre
}

// B) Macro que implementa un match con default
// Uso: match_or!(color, "red" => 1, "blue" => 2, _ => 0)
// Genera: match color { "red" => 1, "blue" => 2, _ => 0 }
macro_rules! match_or {
    // Tu implementación aquí
    // Pista: $val:expr, $( $pat:pat => $result:expr ),+, _ => $default:expr
}

// C) Macro que repite un bloque N veces (N debe ser un literal)
// Uso: repeat!(3, { println!("hello"); });
// Genera: 3 llamadas a println
macro_rules! repeat {
    // Tu implementación aquí
    // Pista: macro recursiva con un contador
    // Caso base: (0, $body:block) => {};
    // Caso recursivo más complejo — piensa en cómo "decrementar" un literal
    // Alternativa más fácil: aceptar 1,2,3 como ramas separadas
}
```

**Preguntas:**
1. En (A), ¿por qué `stringify!` y no `format!`? ¿Cuándo se evalúa cada uno?
2. En (B), ¿por qué usamos `$pat:pat` y no `$pat:expr` para los brazos del match?
3. En (C), ¿es posible hacer una repetición N veces con un número arbitrario? ¿Por qué es difícil?

### Ejercicio 2: HashMap literal

Implementa una macro `map!` que cree un `HashMap` con sintaxis literal:

```rust
// Uso:
let scores = map! {
    "Alice" => 95,
    "Bob" => 87,
    "Carol" => 92,
};

// También debe funcionar vacío:
let empty: HashMap<&str, i32> = map!{};

// Y con trailing comma:
let m = map! { "x" => 1, };
```

```rust
macro_rules! map {
    // Tu implementación aquí
    // Pista: $( $key:expr => $val:expr ),* $(,)?
    // No olvides el bloque envolvente { let mut m = ...; m }
}
```

**Extensiones opcionales:**
1. ¿Puedes hacer que la macro acepte también la sintaxis `"key": "value"` (con `:` en vez de `=>`)?
2. ¿Puedes pre-reservar capacidad con `HashMap::with_capacity(count!(...))` usando una macro `count!`?

### Ejercicio 3: Generar un enum con Display

Crea una macro que genere un enum con `Display` automático:

```rust
// Uso:
display_enum! {
    pub enum HttpStatus {
        Ok => "200 OK",
        NotFound => "404 Not Found",
        ServerError => "500 Internal Server Error",
    }
}

// Debe generar:
// #[derive(Debug, Clone, Copy, PartialEq, Eq)]
// pub enum HttpStatus { Ok, NotFound, ServerError }
// impl Display for HttpStatus { ... match self { Ok => "200 OK", ... } }

fn main() {
    let status = HttpStatus::NotFound;
    println!("{}", status);  // "404 Not Found"
    println!("{:?}", status); // "NotFound"
}
```

```rust
macro_rules! display_enum {
    // Tu implementación aquí
    // Pista: captura $vis:vis para la visibilidad
    // Pista: $name:ident para el nombre del enum
    // Pista: $( $variant:ident => $display:expr ),* para variantes
}
```

**Preguntas:**
1. ¿Por qué usamos `$vis:vis` en vez de hardcodear `pub`?
2. ¿Podrías añadir soporte para variantes con datos, como `Custom(u16, String) => "{}"`? ¿Qué complicaciones traería?
3. ¿Esta macro podría ser una función genérica? ¿Por qué no?

---

> **Nota**: `macro_rules!` es una herramienta de metaprogramación poderosa pero con una curva de aprendizaje pronunciada. Para la mayoría del código, funciones genéricas y traits son preferibles. Las macros declarativas brillan cuando necesitas generar código repetitivo, aceptar variedad de sintaxis, o crear DSLs. En el próximo tópico veremos cómo leer y entender macros del ecosistema que ya usas diariamente.
