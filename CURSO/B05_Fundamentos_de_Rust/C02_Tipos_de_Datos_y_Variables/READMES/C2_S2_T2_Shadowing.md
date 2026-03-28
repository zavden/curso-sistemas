# T02 — Shadowing

## Que es shadowing

Shadowing es re-declarar una variable con el mismo nombre usando `let`.
La nueva variable "oculta" (shadows) a la anterior. La variable
anterior sigue existiendo en memoria, pero es inaccesible desde
el punto de la re-declaracion en adelante (dentro de ese scope).

```rust
let x = 5;
let x = x + 1;     // x es 6; el x anterior (5) ya no es accesible
let x = x * 2;     // x es 12; hay tres "x" en la pila, solo la ultima es visible
```

```rust
// Shadowing NO es lo mismo que mutacion.
// Con shadowing usas let cada vez:
let x = 5;
let x = 10;    // nueva variable, shadow de la anterior

// Con mutacion usas mut y reasignas sin let:
let mut y = 5;
y = 10;        // misma variable, valor modificado
```

## Shadowing basico

Cada `let` con el mismo nombre crea una variable completamente
nueva. El compilador las trata como variables distintas:

```rust
fn main() {
    let x = 5;
    println!("x = {x}");           // x = 5

    let x = x + 1;
    println!("x = {x}");           // x = 6

    let x = x * 2;
    println!("x = {x}");           // x = 12

    let x = format!("result: {x}");
    println!("{x}");                // result: 12
}

// El compilador ve internamente algo como:
// let x_0 = 5;
// let x_1 = x_0 + 1;
// let x_2 = x_1 * 2;
// let x_3 = format!("result: {x_2}");
// Son variables distintas. "x" es solo un nombre que se reutiliza.
```

```rust
// Cada let puede usar el valor anterior o ignorarlo:
let count = "three";
let count = count.len();    // usa el &str anterior
let count = count + 1;      // usa el usize anterior

let data = vec![1, 2, 3];
let data = "something else"; // ignora el Vec anterior; este se droppea aqui
```

## Shadowing vs mutacion

Shadowing y mutacion sirven para "cambiar" el valor de un nombre,
pero funcionan de forma fundamentalmente diferente:

```rust
// --- Shadowing ---
// Crea una NUEVA variable. Puede cambiar de tipo.
let x = 5;
let x = "hello";   // OK: nueva variable, tipo distinto
let x = vec![1];   // OK: nueva variable, otro tipo mas

// --- Mutacion ---
// Modifica la MISMA variable. El tipo NO puede cambiar.
let mut y = 5;
y = 10;             // OK: mismo tipo (i32)
// y = "hello";     // ERROR: expected integer, found &str
```

```rust
// Diferencia en ownership y drop:

// Con shadowing, la variable anterior se droppea al ser inaccesible:
let s = String::from("hello");
let s = String::from("world");   // "hello" se droppea aqui

// Con mutacion, el valor anterior se sobreescribe en el mismo lugar:
let mut s = String::from("hello");
s = String::from("world");       // "hello" se droppea, "world" toma su lugar
```

```rust
// Shadowing permite pasar de mutable a inmutable:
let mut config = String::new();
config.push_str("debug=true\n");
config.push_str("level=3\n");

let config = config;   // ahora es inmutable
// config.push_str("extra");  // ERROR: cannot borrow as mutable
```

## Cambio de tipo con shadowing

La ventaja principal de shadowing sobre mutacion es que permite
cambiar el tipo de la variable:

```rust
// Caso clasico del libro oficial de Rust:
let spaces = "   ";          // tipo: &str
let spaces = spaces.len();   // tipo: usize

// Con mut esto NO funciona:
// let mut spaces = "   ";
// spaces = spaces.len();    // ERROR: expected &str, found usize
```

```rust
// Leer entrada del usuario y parsear:
use std::io;

let mut input = String::new();
io::stdin().read_line(&mut input).unwrap();

let input: u32 = input.trim().parse().unwrap();
// input paso de String a u32
```

```rust
// Conversion de tipos en cadena:
let value = "42";                           // &str
let value = value.parse::<i64>().unwrap();  // i64
let value = value as f64;                   // f64
let value = value * 2.5;                    // f64

// Sin shadowing necesitarias nombres distintos:
// let value_str = "42";
// let value_i64 = value_str.parse::<i64>().unwrap();
// let value_f64 = value_i64 as f64;
// Shadowing es mas limpio cuando los intermedios no se reusan.
```

## Scope y shadowing

Shadowing interactua con los scopes (bloques `{}`). Una variable
declarada en un bloque interno oculta la externa solo dentro
de ese bloque. Al salir, la variable externa vuelve a ser visible:

```rust
fn main() {
    let x = 5;

    {
        let x = "hello";    // shadow de x solo dentro de este bloque
        println!("{x}");     // hello
    }

    println!("{x}");         // 5 — el x original vuelve a ser visible
}
```

```rust
// Scopes anidados con multiples niveles:
fn main() {
    let x = 1;
    println!("A: {x}");     // A: 1

    {
        println!("B: {x}"); // B: 1 — el x de afuera es visible
        let x = 2;
        println!("C: {x}"); // C: 2 — shadow dentro del bloque

        {
            let x = 3;
            println!("D: {x}"); // D: 3 — shadow mas interno
        }

        println!("E: {x}"); // E: 2 — vuelve al shadow del bloque medio
    }

    println!("F: {x}");     // F: 1 — vuelve al x original
}
```

```rust
// Block expressions y shadowing:
// Un bloque {} puede devolver un valor, combinado con shadowing:
let x = 5;
let x = {
    let y = x * 2;   // y = 10
    let z = y + 3;   // z = 13
    z                 // el bloque devuelve 13 (sin punto y coma)
};
// x ahora es 13
// y y z no existen fuera del bloque
```

## Shadowing en match e if let

Un patron comun en Rust es usar shadowing para transformar
un valor manteniendo el mismo nombre:

```rust
// Shadow en match — desenvolver un Option:
let value: Option<i32> = Some(42);

let value = match value {
    Some(v) => v * 2,
    None => 0,
};
// value paso de Option<i32> a i32
println!("{value}");  // 84
```

```rust
// Shadow en if let:
let config: Option<String> = Some("debug".to_string());

let config = if let Some(c) = config {
    c.to_uppercase()
} else {
    "DEFAULT".to_string()
};
// config paso de Option<String> a String
println!("{config}");  // DEBUG
```

```rust
// Validar y transformar en cadena:
let input = "  42  ";
let input = input.trim();                // &str sin espacios
let input: i32 = input.parse().unwrap(); // i32
let input = input.abs();                 // i32 positivo
// El nombre "input" sigue siendo claro en cada etapa.
```

## Shadowing en parametros de funcion

Los parametros de una funcion se pueden shadow dentro
del cuerpo. Esto es util para conversiones de tipo:

```rust
fn process(x: i32) {
    let x = x as f64;       // a partir de aqui x es f64
    let x = x * 3.14;
    println!("{x}");
}

fn set_timeout(seconds: u64) {
    let seconds = std::time::Duration::from_secs(seconds);
    // Ahora seconds es Duration, no u64
    println!("Timeout: {seconds:?}");
}

fn count_chars(text: &str) -> usize {
    let text = text.trim();  // shadow: no necesitamos el original
    text.chars().count()
}
```

## Cuando shadowing es util vs confuso

Shadowing es una herramienta idiomatica en Rust, pero
usarla mal puede hacer el codigo dificil de leer.

```rust
// BUENO — Parsing: el nombre refleja el mismo concepto
let guess = "42";
let guess: u32 = guess.trim().parse().expect("Not a number");
// Este patron sale directamente del libro oficial de Rust.

// BUENO — Inmutabilizar despues de construir
let mut buffer = Vec::new();
buffer.push(1);
buffer.push(2);
let buffer = buffer;  // ahora es inmutable

// BUENO — Limpiar un valor
let path = "  /home/user/file.txt  ";
let path = path.trim();
let path = std::path::Path::new(path);
```

```rust
// CONFUSO — Tipos y significados completamente distintos
let x = calculate_price();
let x = get_username();       // x ya no tiene nada que ver con precio
let x = open_connection();    // aun peor
// Mejor usar nombres descriptivos: price, username, conn.

// CONFUSO — Shadow en funciones largas
fn process_data(data: Vec<i32>) {
    let data = filter(data);
    // ... 50 lineas ...
    let data = transform(data);
    // ... 30 lineas ...
    // No sabes cual "data" es al leer una linea en medio.
    // Mejor: let filtered, let transformed, etc.
}

// CONFUSO — Ocultar errores
let result = compute();
let result = 42;  // ignora el resultado del calculo sin warning
// Para ignorar un valor explicitamente, usa el prefijo _:
let _result = compute();
```

## Comparacion con otros lenguajes

```c
// C — No permite shadowing en el mismo scope

// int x = 5;
// int x = 10;  // error: redefinition of 'x'

// En bloques anidados: permitido
int x = 5;
{
    int x = 10;
    printf("%d\n", x);  // 10
}
printf("%d\n", x);      // 5
```

```python
# Python — Rebinding (reasignacion libre, sin declaracion especial)
x = 5
x = "hello"    # OK: Python no tiene declaracion de tipo
x = [1, 2, 3]  # OK: cualquier tipo
# No hay "let", no hay distincion entre declarar y reasignar.
```

```javascript
// JavaScript — let en el mismo scope es error
// let x = 5;
// let x = 10;  // SyntaxError: Identifier 'x' has already been declared

// En bloques anidados con let:
let x = 5;
{
    let x = "hello";   // OK: shadow en bloque interno
    console.log(x);     // "hello"
}
console.log(x);         // 5
```

```rust
// Rust — Shadowing permitido en el mismo scope y en scopes anidados
let x = 5;
let x = "hello";    // OK en el mismo scope
let x = vec![1];    // OK: tercer shadow
{
    let x = 42.0;   // OK en scope anidado
}
// x vuelve a ser vec![1] aqui
// Rust es el unico de estos lenguajes que permite
// shadowing con cambio de tipo en el mismo scope.
```

```
Resumen:
+-------------+--------------------+---------------------+
| Lenguaje    | Mismo scope        | Scope anidado       |
+-------------+--------------------+---------------------+
| C           | No                 | Si                  |
| Python      | Si (rebinding)     | Si (rebinding)      |
| JavaScript  | No (con let)       | Si                  |
| Rust        | Si (con let)       | Si                  |
+-------------+--------------------+---------------------+
```

---

## Ejercicios

### Ejercicio 1 — Shadowing basico y tipo

```rust
// Predecir el output de este programa sin ejecutarlo.
// Luego compilar con rustc y verificar.

fn main() {
    let x = 5;
    let x = x + 1;
    let x = x * 3;
    println!("x = {x}");

    let y = "hello";
    let y = y.len();
    let y = y > 3;
    println!("y = {y}");
}

// Preguntas:
// 1. Cuantas variables "x" se crean en total?
// 2. Cual es el tipo de cada una?
// 3. Cuantas variables "y" se crean en total?
// 4. Cual es el tipo de la ultima y?
```

### Ejercicio 2 — Shadowing vs mutacion

```rust
// Corregir el siguiente codigo. Tiene DOS errores.
// Un error es de tipo (shadowing arregla el problema).
// El otro es de mutabilidad.

fn main() {
    let count = "five";
    count = count.len();

    let mut total = 0;
    total = total + count;
    println!("total = {total}");
}

// Pistas:
// 1. La primera reasignacion cambia de tipo: necesita let
// 2. La segunda reasignacion funciona porque total es mut,
//    pero hay un problema de tipo: count es &str, no usize.
//    Hay que arreglar el orden.
```

### Ejercicio 3 — Scope y visibilidad

```rust
// Predecir el output. Dibujar un diagrama de scopes
// indicando que variable "x" es visible en cada punto.

fn main() {
    let x = 1;
    println!("A: {x}");

    let x = x + 10;
    println!("B: {x}");

    {
        println!("C: {x}");
        let x = x + 100;
        println!("D: {x}");

        {
            let x = x + 1000;
            println!("E: {x}");
        }

        println!("F: {x}");
    }

    println!("G: {x}");
}

// Verificar compilando con rustc.
// Pregunta adicional: en el punto F, cual es el valor de x
// y por que no es el mismo que en el punto E?
```
