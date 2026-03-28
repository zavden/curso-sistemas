# T04 — let-else

## Que es let-else

`let-else` es una construccion de Rust que combina pattern matching
con early return en una sola sentencia. Fue estabilizada en
**Rust 1.65** (noviembre 2022). Permite extraer valores de un
patron y salir inmediatamente si el patron no coincide:

```rust
let Some(x) = some_option else {
    return;     // si some_option es None, sale de la funcion
};
// x esta disponible aqui con el valor extraido.
// El bloque else DEBE divergir (return, break, continue, panic!, etc.).

fn process_config(config: Option<&str>) -> String {
    let Some(cfg) = config else {
        return String::from("default");
    };
    // cfg es &str, disponible directamente, sin nesting.
    format!("Using config: {}", cfg)
}
```

## El problema que resuelve

Antes de `let-else`, para extraer un valor y salir temprano
habia que usar `match` o `if let`. Ambos acumulan niveles
de indentacion con cada extraccion consecutiva:

```rust
// Antes de let-else: la "piramide del doom" con if let.
fn process(
    headers: Option<&str>,
    body: Option<&str>,
    token: Option<&str>,
) -> Result<String, &'static str> {
    if let Some(h) = headers {
        if let Some(b) = body {
            if let Some(t) = token {
                // Tres niveles de indentacion para el happy path.
                Ok(format!("{}: {} (auth: {})", h, b, t))
            } else { Err("missing token") }
        } else { Err("missing body") }
    } else { Err("missing headers") }
}

// Con let-else: todo al mismo nivel.
fn process(
    headers: Option<&str>,
    body: Option<&str>,
    token: Option<&str>,
) -> Result<String, &'static str> {
    let Some(h) = headers else { return Err("missing headers"); };
    let Some(b) = body else { return Err("missing body"); };
    let Some(t) = token else { return Err("missing token"); };
    Ok(format!("{}: {} (auth: {})", h, b, t))
}
```

## Sintaxis

```rust
// let PATTERN = EXPRESSION else { DIVERGING_BLOCK };
//
// PATTERN     — patron refutable (Some(x), Ok(v), etc.)
// EXPRESSION  — la expresion a evaluar
// DIVERGING   — bloque que DEBE divergir (return, break, continue, panic!)
//
// Las variables del patron quedan disponibles DESPUES de la sentencia,
// en el scope que la contiene (no dentro de un bloque anidado).

fn example(val: Option<i32>) -> i32 {
    let Some(x) = val else { return -1; };
    x * 2   // x disponible aqui, al nivel superior
}
```

```rust
// Patrones complejos: destructuring de structs y tuplas.
struct Config { port: u16, host: String }

fn start_server(config: Option<Config>) {
    let Some(Config { port, host }) = config else {
        eprintln!("No config provided");
        return;
    };
    println!("Starting server on {}:{}", host, port);
}

fn parse_pair(input: Option<(i32, i32)>) -> i32 {
    let Some((a, b)) = input else { return 0; };
    a + b
}
```

## El bloque else debe divergir

El bloque `else` **debe** contener una expresion que nunca
retorne normalmente (tipo `!`). El compilador lo verifica.
Si no diverge, el codigo no compila:

```rust
// CORRECTO — return diverge:
let Some(x) = val else { return -1; };

// INCORRECTO — println! no diverge:
let Some(x) = val else {
    println!("no value");
    // error[E0308]: `else` clause of `let...else` must diverge
};
```

```rust
// Formas validas de divergir:
//   return              — sale de la funcion
//   break               — sale de un loop
//   continue            — salta a la siguiente iteracion
//   panic!()            — aborta el programa
//   unreachable!()      — indica codigo inalcanzable
//   std::process::exit() — termina el proceso
//   loop {}             — loop infinito (nunca retorna)

// Ejemplo con continue en un loop:
fn find_first_even(numbers: &[Option<i32>]) -> Option<i32> {
    for item in numbers {
        let Some(n) = item else { continue; };
        if n % 2 == 0 { return Some(*n); }
    }
    None
}
```

```rust
// Por que se exige la divergencia:
// Despues de let-else, las variables del patron se usan libremente.
// Si el else no divergiera, habria un camino donde las variables
// no estan inicializadas. El compilador lo impide exigiendo que
// el else SIEMPRE salga del flujo normal.
```

## Casos de uso comunes

```rust
use std::collections::HashMap;

// 1. Desenvolver Option:
fn load_username(db: &HashMap<u32, String>, id: u32) -> String {
    let Some(name) = db.get(&id) else {
        return String::from("anonymous");
    };
    format!("User: {}", name)
}

// 2. Desenvolver Result:
fn read_config(path: &str) -> String {
    let Ok(contents) = std::fs::read_to_string(path) else {
        return String::from("{}");
    };
    contents
}

// 3. Conversion de tipos:
fn parse_port(input: &str) -> u16 {
    let Ok(port) = input.parse::<u16>() else {
        eprintln!("Invalid port: '{}', using default", input);
        return 8080;
    };
    port
}

// 4. Guard clauses al inicio de funciones:
fn handle_request(
    method: Option<&str>,
    path: Option<&str>,
    auth: Option<&str>,
) -> Result<String, &'static str> {
    let Some(method) = method else { return Err("missing method"); };
    let Some(path) = path else { return Err("missing path"); };
    let Some(auth) = auth else { return Err("unauthorized"); };
    Ok(format!("{} {} (auth: {})", method, path, auth))
}
```

## let-else vs if let

`let-else` es para early exit (salir cuando el patron no
coincide). `if let` es para ejecutar codigo cuando coincide:

```rust
// if let — ejecutar algo si coincide:
fn maybe_greet(name: Option<&str>) {
    if let Some(n) = name {
        println!("Hello, {}!", n);
    }
    // n NO disponible aqui. Si no coincide, no hace nada.
}

// let-else — early exit si NO coincide:
fn must_greet(name: Option<&str>) -> String {
    let Some(n) = name else {
        return String::from("Hello, stranger!");
    };
    format!("Hello, {}!", n)  // n disponible aqui
}
```

```rust
// if let:
//   - Happy path DENTRO del bloque
//   - Variables solo viven dentro del bloque
//   - No requiere divergencia
//   - Ideal para "haz algo si coincide, sino continua"
//
// let-else:
//   - Happy path DESPUES de la sentencia
//   - Variables viven en el scope exterior
//   - El else DEBE divergir
//   - Ideal para "sal si no coincide, sino continua"
```

## let-else vs match

`let-else` maneja un solo patron con early exit.
`match` maneja multiples patrones o cuando necesitas
el valor que no coincidio:

```rust
// let-else — un patron, early exit:
fn get_value(opt: Option<i32>) -> i32 {
    let Some(v) = opt else { return 0; };
    v
}

// match — multiples patrones (let-else no aplica):
fn describe(opt: Option<i32>) -> String {
    match opt {
        Some(v) if v > 0  => format!("positive: {}", v),
        Some(v) if v == 0 => String::from("zero"),
        Some(v)           => format!("negative: {}", v),
        None              => String::from("nothing"),
    }
}

// match — cuando necesitas el valor no coincidente:
fn parse_or_report(input: &str) -> i32 {
    match input.parse::<i32>() {
        Ok(n) => n,
        Err(e) => {
            eprintln!("Parse error: {}", e);  // acceso a 'e'
            0
        }
    }
}
// Con let-else NO hay acceso al Err(e) en el bloque else.
// Solo sabemos que el patron no coincidio.
//
// Resumen:
// let-else  → un patron, early exit, no necesitas el valor no coincidente
// match     → multiples patrones, guards, o acceso al valor no coincidente
```

## Encadenamiento de let-else

Multiples `let-else` consecutivos crean guard clauses
limpios y legibles:

```rust
use std::collections::HashMap;

fn process_user_score(
    users: &HashMap<String, u32>,
    name: Option<&str>,
    bonus: Option<u32>,
) -> String {
    let Some(name) = name else {
        return String::from("Error: no name");
    };
    let Some(score) = users.get(name) else {
        return format!("Error: user '{}' not found", name);
    };
    let Some(bonus) = bonus else {
        return format!("{}: {} points (no bonus)", name, score);
    };
    let total = score + bonus;
    format!("{}: {} + {} = {} points", name, score, bonus, total)
}
```

## Limitaciones

```rust
// 1. No se puede usar a nivel top-level (fuera de una funcion).
//    return, break, continue solo existen dentro de funciones/loops.
// INCORRECTO: let Some(x) = some_value() else { return; };  // a nivel modulo
// CORRECTO:   fn f() { let Some(x) = some_value() else { return; }; }

// 2. El bloque else NO puede acceder a las variables del patron.
//    Si el patron no coincidio, las variables no existen.
//    Si necesitas el valor no coincidente (ej: Err), usa match.

// 3. Solo funciona con patrones refutables.
//    Un patron irrefutable no necesita else:
// ADVERTENCIA: let (a, b) = (1, 2) else { return; };
// CORRECTO:    let (a, b) = (1, 2);

// 4. El bloque else debe usar llaves, no una expresion simple:
// INCORRECTO: let Some(x) = val else return 0;
// CORRECTO:   let Some(x) = val else { return 0; };

// 5. let-else solo extrae UN patron. Para manejar multiples
//    variantes por separado, match es la herramienta correcta.
enum Command { Run(String), Stop, Pause(u32) }

fn handle(cmd: Command) {
    let Command::Run(program) = cmd else {
        return;  // fue Stop o Pause, pero no sabemos cual
    };
    println!("Running: {}", program);
}
```

---

## Ejercicios

### Ejercicio 1 — Reescribir if let anidados

```rust
// Reescribir esta funcion usando let-else para eliminar el anidamiento.
// La logica debe ser identica: mismos valores de retorno para cada caso.
//
// fn extract_data(
//     name: Option<&str>,
//     age: Option<u32>,
//     email: Option<&str>,
// ) -> Result<String, &'static str> {
//     if let Some(n) = name {
//         if let Some(a) = age {
//             if let Some(e) = email {
//                 if a >= 18 {
//                     Ok(format!("{} ({}) <{}>", n, a, e))
//                 } else { Err("too young") }
//             } else { Err("missing email") }
//         } else { Err("missing age") }
//     } else { Err("missing name") }
// }
//
// Verificar:
//   extract_data(Some("Ana"), Some(25), Some("ana@x.com"))
//     -> Ok("Ana (25) <ana@x.com>")
//   extract_data(None, Some(25), Some("a@x.com"))
//     -> Err("missing name")
//   extract_data(Some("Ana"), Some(10), Some("a@x.com"))
//     -> Err("too young")
```

### Ejercicio 2 — Guard clauses con let-else

```rust
// Escribir una funcion parse_and_validate que:
//   1. Reciba tres &str: host, port_str, max_conn_str
//   2. Verifique que host no este vacio (si lo esta, return Err)
//   3. Parsee port_str a u16 con let-else (si falla, return Err)
//   4. Parsee max_conn_str a u32 con let-else (si falla, return Err)
//   5. Verifique que port > 0 y max_conn > 0
//   6. Retorne Ok(format!("{}:{} (max: {})", host, port, max_conn))
//
// Verificar:
//   parse_and_validate("localhost", "8080", "100")
//     -> Ok("localhost:8080 (max: 100)")
//   parse_and_validate("localhost", "abc", "100")
//     -> Err (parseo de port fallo)
//   parse_and_validate("", "8080", "100")
//     -> Err (host vacio)
```

### Ejercicio 3 — let-else vs match

```rust
// Para cada situacion, decidir si es mejor let-else o match.
// Implementar la funcion con la herramienta correcta y comentar por que.
//
// Situacion A: Extraer el valor de Some o retornar un default.
// fn unwrap_or_return(val: Option<i32>) -> i32
//
// Situacion B: Actuar diferente segun Ok(n) donde n>0, Ok(n) donde n<=0,
//              o Err(e) mostrando el error.
// fn classify_result(val: Result<i32, String>) -> String
//
// Situacion C: En un loop, saltar iteraciones donde el valor es None.
// fn sum_valid(values: &[Option<i32>]) -> i32
```
