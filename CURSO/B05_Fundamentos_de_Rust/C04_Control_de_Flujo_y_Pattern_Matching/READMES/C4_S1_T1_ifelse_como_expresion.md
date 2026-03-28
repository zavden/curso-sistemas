# T01 — if/else como expresion

## Que es if/else en Rust

En Rust, `if`/`else` es una **expresion**, no solo una sentencia.
Produce un valor y se puede usar en el lado derecho de un `let`.
Esto es una diferencia fundamental respecto a C.

```rust
// if/else retorna un valor en Rust:
let x = if true { 5 } else { 10 };
// x vale 5

// En C, if es una sentencia — se usa el ternario:
// int x = 1 ? 5 : 10;
// Rust NO tiene operador ternario — usa if/else directamente.
```

## if/else basico

```rust
// Sintaxis: if condition { block } else { block }
// Tres reglas clave:
// 1. NO se usan parentesis alrededor de la condicion
// 2. La condicion DEBE ser de tipo bool (sin conversion implicita)
// 3. Las llaves {} son SIEMPRE obligatorias

let temperature = 35;

if temperature > 30 {
    println!("Hot");
}
```

```rust
// Parentesis innecesarios — el compilador emite warning:
if (temperature > 30) {     // warning: unnecessary parentheses
    println!("Hot");
}
// Compila, pero no es idiomatico.
```

```rust
// Llaves SIEMPRE obligatorias — a diferencia de C:
// En C: if (x > 0) printf("positive");    // valido
// En Rust:
if temperature > 30
    println!("Hot");
// error: expected `{`
```

## La condicion debe ser bool

No existe conversion implicita desde enteros, punteros o cualquier
otro tipo. Esto elimina una clase entera de bugs comunes en C:

```rust
// En C: if (1) {...} if (0) {...} if (ptr) {...} — todo valido.
// En Rust, NADA de esto compila:

let count = 5;
if count { println!("has items"); }
// error[E0308]: mismatched types — expected `bool`, found `{integer}`

let s = String::from("hello");
if s { println!("not empty"); }
// error — String no es bool

let opt: Option<i32> = Some(5);
if opt { println!("has value"); }
// error — Option no es bool
```

```rust
// Solucion: comparar explicitamente:
if count > 0 { println!("has items"); }
if !s.is_empty() { println!("not empty"); }
if opt.is_some() { println!("has value"); }
// O mejor: if let para Option (se ve mas adelante).

// Esto tambien previene el bug clasico de C:
// if (x = 5) { ... }  // asignacion en vez de comparacion
// En Rust: if x = 5 { } → error, porque x = 5 retorna (), no bool.
```

## if/else como expresion

`if`/`else` retorna el valor de la ultima expresion de la rama
que se ejecuta:

```rust
let condition = true;
let number = if condition { 5 } else { 10 };
println!("{number}");  // 5

// Reemplaza al ternario de C:
// C:    int number = condition ? 5 : 10;
// Rust: let number = if condition { 5 } else { 10 };
```

```rust
// Punto y coma: va DESPUES de todo el if/else (cierra el let).
// Dentro de las ramas, la ultima expresion NO lleva punto y coma:

let x = if true {
    5        // sin ; — expresion, retorna 5
} else {
    10       // sin ; — expresion, retorna 10
};           // ; del let

// Si pones ; dentro, el bloque retorna () en vez del valor:
let x = if true { 5; } else { 10; };
// x es (), no i32. El ; convierte la expresion en sentencia.
```

```rust
// Las ramas pueden tener multiples lineas — solo la ULTIMA cuenta:
let score = 85;
let grade = if score >= 90 {
    println!("Excellent!");   // efecto secundario — con ;
    'A'                       // valor de retorno — sin ;
} else if score >= 80 {
    println!("Good job!");
    'B'
} else {
    println!("Needs work");
    'F'
};
// grade es 'B'
```

```rust
// AMBAS ramas deben retornar el MISMO tipo:
let x = if true { 5 } else { "hello" };
// error[E0308]: `if` and `else` have incompatible types
//   expected `i32`, found `&str`

let x = if true { 5 } else { 10.0 };
// error — integer vs floating-point
```

## Usar if en let — patrones comunes

```rust
// Asignar basado en condicion:
let status = if score >= 60 { "pass" } else { "fail" };

// Usar directamente en expresiones:
println!("Found {} {}", count, if count == 1 { "item" } else { "items" });

// Retornar desde funciones:
fn classify(n: i32) -> &'static str {
    if n > 0 { "positive" } else if n < 0 { "negative" } else { "zero" }
    // El if/else ES la ultima expresion de la funcion — no hace falta return.
}
```

## else if — cadenas de condiciones

```rust
// else if encadena multiples condiciones.
// No existe elif (Python) ni elsif (Perl).
let temp = 25;
let category = if temp > 35 {
    "very hot"
} else if temp > 25 {
    "warm"
} else if temp > 15 {
    "mild"
} else {
    "cold"
};
// category es "mild"
```

```rust
// La PRIMERA condicion verdadera gana — el orden importa:
let x = 15;
if x > 5 {
    println!("A");       // se ejecuta — primera verdadera
} else if x > 10 {
    println!("B");       // NO se ejecuta, aunque tambien es true
}
// Para condiciones mas estrictas primero, reordenar:
if x > 10 { println!("B"); } else if x > 5 { println!("A"); }
```

```rust
// Cuando hay muchas ramas, considerar match:
// Esto:
let name = if day == 1 { "Mon" } else if day == 2 { "Tue" } else { "Other" };
// Es mas legible con match:
let name = match day {
    1 => "Mon",
    2 => "Tue",
    _ => "Other",
};
```

## if sin else

Un `if` sin `else` se puede usar como sentencia, pero NO como
expresion asignada a un `let`:

```rust
// Como sentencia — correcto:
if x > 5 {
    println!("big");    // efecto secundario, valor descartado
}
```

```rust
// Como expresion — ERROR:
let x = if true { 5 };
// error[E0317]: `if` may be missing an `else` clause
//
// Si la condicion es false, que valor tiene x?
// El bloque retorna i32, sin else retorna ().
// i32 != () → error de tipos.
//
// Solucion: agregar else con valor por defecto:
let x = if true { 5 } else { 0 };
```

```rust
// Detalle tecnico: if sin else siempre retorna ().
// Esto compila porque ambos lados son ():
let result: () = if true { println!("ok"); };
// Pero no es util en la practica.
```

## if let — pattern matching para un solo patron

`if let` abrevia un `match` cuando solo interesa **un patron**:

```rust
// Con match — verboso:
let some_value: Option<i32> = Some(42);
match some_value {
    Some(x) => println!("Got: {x}"),
    _ => (),   // obligatorio pero inutil
}

// Con if let — conciso:
if let Some(x) = some_value {
    println!("Got: {x}");
}
// Si es Some, extrae el valor. Si es None, no hace nada.
```

```rust
// Sintaxis: if let PATTERN = EXPRESSION { block }
// Nota el = simple (no ==). El patron va a la IZQUIERDA.

// Con Option:
fn find_user(id: u32) -> Option<String> {
    if id == 1 { Some(String::from("Alice")) } else { None }
}
if let Some(name) = find_user(1) {
    println!("Found: {name}");
} else {
    println!("Not found");
}
// Found: Alice
```

```rust
// Con Result:
let parsed: Result<i32, _> = "42".parse();
if let Ok(number) = parsed {
    println!("Parsed: {number}");
} else {
    println!("Parse failed");
}
```

```rust
// Con enums personalizados:
enum Command {
    Quit,
    Echo(String),
    Move { x: i32, y: i32 },
}
let cmd = Command::Echo(String::from("hello"));
if let Command::Echo(msg) = cmd {
    println!("Echo: {msg}");
}

// Con tuplas:
let pair = (1, "hello");
if let (1, greeting) = pair {
    println!("Greeting: {greeting}");
}
```

```rust
// if let con else y else if:
let first: Option<i32> = None;
let second: Option<i32> = Some(42);

if let Some(x) = first {
    println!("First: {x}");
} else if let Some(y) = second {
    println!("Second: {y}");       // se ejecuta
} else {
    println!("Both None");
}

// if let como expresion (retorna valor):
let port = if let Some(p) = config { p } else { 3000 };

// Para Option, unwrap_or es mas idiomatico en este caso:
let port = config.unwrap_or(3000);
```

## if let chains (Rust 1.87.0+)

Las **let chains** combinan `if let` con condiciones booleanas
usando `&&`. Se estabilizaron gradualmente:

```rust
// Sin let chains — anidado:
if let Some(x) = value {
    if x > 10 {
        println!("Big: {x}");
    }
}

// Con let chains — una sola condicion:
if let Some(x) = value && x > 10 {
    println!("Big: {x}");
}
```

```rust
// Multiples let en cadena:
let first: Option<i32> = Some(5);
let second: Option<i32> = Some(10);

if let Some(a) = first && let Some(b) = second && a + b > 10 {
    println!("Sum: {}", a + b);
}
// Sum: 15
```

```rust
// Nota de compatibilidad:
// - Versiones anteriores a la estabilizacion necesitan:
//   #![feature(let_chains)]
// - Alternativa compatible con todas las versiones:
if matches!(value, Some(x) if x > 10) {
    println!("Big number");
}
// Pero matches! no extrae el valor en una variable accesible.
```

## if let anidado

```rust
// Extraer valores de estructuras anidadas:
let outer: Option<&str> = Some("42");

if let Some(text) = outer {
    if let Ok(number) = text.parse::<i32>() {
        println!("Parsed: {number}");
    }
}

// Option dentro de Option:
let nested: Option<Option<i32>> = Some(Some(42));
if let Some(inner) = nested {
    if let Some(value) = inner {
        println!("Value: {value}");
    }
}
// Alternativa: nested.flatten() convierte Option<Option<T>> en Option<T>.
```

```rust
// Evitar anidacion excesiva con early returns:

// Demasiado anidado:
fn process(input: Option<String>) -> Result<i32, String> {
    if let Some(s) = input {
        if let Ok(n) = s.parse::<i32>() {
            if n > 0 { Ok(n * 2) } else { Err(String::from("not positive")) }
        } else { Err(String::from("not a number")) }
    } else { Err(String::from("no input")) }
}

// Mejor con early returns y el operador ?:
fn process(input: Option<String>) -> Result<i32, String> {
    let s = input.ok_or(String::from("no input"))?;
    let n: i32 = s.parse().map_err(|_| String::from("not a number"))?;
    if n <= 0 { return Err(String::from("not positive")); }
    Ok(n * 2)
}
```

## Comparacion con C y Python

```rust
// Tabla de diferencias:
//
// Caracteristica          | C           | Python        | Rust
// ----------------------- | ----------- | ------------- | ---------------
// if es expresion         | No          | No (*)        | Si
// Parentesis condicion    | Obligatorio | No            | No (warning)
// Llaves / delimitadores  | Opcionales  | Indentacion   | Obligatorias
// Conversion a bool       | Implicita   | Truthy/falsy  | Solo explicita
// Ternario                | ? :         | x if c else y | No tiene
// if let (pattern match)  | No          | No            | Si
// elif / else if          | else if     | elif          | else if
//
// (*) Python tiene x = a if cond else b, pero el if de bloque es sentencia.
//
// Rust es mas estricto: condicion debe ser bool, llaves siempre,
// sin ternario, sin truthy/falsy. A cambio, if es una expresion
// y tiene if let para pattern matching.
```

---

## Ejercicios

### Ejercicio 1 — if/else como expresion

```rust
// Escribir una funcion classify_temperature(temp: i32) -> &'static str
// que retorne usando if/else como expresion:
//   > 35 → "very hot"    > 25 → "warm"    > 15 → "mild"
//   > 5  → "cold"        <= 5 → "freezing"
//
// Requisitos:
//   - Usar if/else if/else como expresion (asignar a let)
//   - Probar con: -10, 5, 16, 26, 40
//   - Verificar que cada valor retorna la categoria correcta.
```

### Ejercicio 2 — if let con Option

```rust
// Dada:
fn find_item(items: &[&str], target: &str) -> Option<usize> {
    items.iter().position(|&item| item == target)
}
//
// Escribir un programa que:
//   1. Defina un array: ["apple", "banana", "cherry", "date"]
//   2. Busque "cherry" — use if let para imprimir "Found at index N" o "Not found"
//   3. Busque "grape" y maneje el caso None
//   4. Busque "banana" — si el indice es > 0, imprima "Not the first item"
//      (usar if let anidado o let chains)
```

### Ejercicio 3 — Expresion vs sentencia

```rust
// Predecir el tipo y valor de cada variable SIN ejecutar.
// Luego verificar compilando:
//
// let a = if true { 1 } else { 2 };
// let b = if false { 1 } else { 2 };
// let c = if true { 1; } else { 2; };
// let d = { let x = 5; x + 1 };
// let e = { let x = 5; x + 1; };
//
// Para cada variable indicar: tipo (i32 o ()) y valor.
// Imprimir con println!("{:?}", variable) para verificar.
```

### Ejercicio 4 — Reescribir estilo C a Rust

```rust
// Reescribir este pseudocodigo "estilo C" a Rust idiomatico:
//
//   count = get_count();
//   if (count) { print("has items"); }          // conversion implicita
//
//   name = get_name();         // retorna Option<String>
//   if (name) { print(name); }                  // conversion implicita
//
//   result = parse_number(input);  // retorna Result<i32, Error>
//   if (result) { use(result); } else { print("invalid"); }
//
// Pistas:
//   - count: usar count > 0
//   - name: usar if let Some(n) = name
//   - result: usar if let Ok(n) = result
// Definir funciones auxiliares para que compile y ejecute.
```
