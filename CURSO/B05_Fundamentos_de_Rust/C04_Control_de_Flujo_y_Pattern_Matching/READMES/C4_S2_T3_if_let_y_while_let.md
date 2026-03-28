# T03 — if let y while let

## El problema con match de una sola rama

Cuando solo te interesa un caso de un `match` y quieres ignorar
el resto, el bloque completo resulta verboso:

```rust
let config_max: Option<u8> = Some(3);

// Verboso — solo nos interesa Some, pero debemos escribir el wildcard:
match config_max {
    Some(max) => println!("The maximum is configured to be {max}"),
    _ => {}
}
// La rama _ => {} no hace nada pero es obligatoria.
// Con muchas de estas en el codigo, el ruido se acumula.
```

## if let — syntactic sugar para match de una rama

`if let` permite extraer un valor cuando el patron coincide,
sin necesidad de escribir la rama wildcard:

```rust
let config_max: Option<u8> = Some(3);

// Equivalente al match anterior, pero sin ruido:
if let Some(max) = config_max {
    println!("The maximum is configured to be {max}");
}
// Si config_max es None, no pasa nada — el bloque se omite.

// La sintaxis es:
//   if let PATTERN = EXPRESSION { body }
//
// El patron va a la IZQUIERDA del =, la expresion a la DERECHA.
// Es lo opuesto a una asignacion normal, pero consistente con match.
```

```rust
// Equivalencia exacta:

// Esto:
if let Some(x) = val {
    use_value(x);
}

// Es identico a esto:
match val {
    Some(x) => use_value(x),
    _ => {}
}

// El compilador genera el mismo codigo para ambos.
// La diferencia es puramente ergonomica.
```

## if let con else

`if let` puede tener un bloque `else` que maneja todos los casos
que no coinciden con el patron:

```rust
let config_max: Option<u8> = None;

if let Some(max) = config_max {
    println!("Maximum: {max}");
} else {
    println!("No maximum configured, using default");
}
// Salida: No maximum configured, using default

// Equivale a:
// match config_max {
//     Some(max) => println!("Maximum: {max}"),
//     _ => println!("No maximum configured, using default"),
// }
```

```rust
// Funciona igual con Result:
if let Ok(contents) = std::fs::read_to_string("config.toml") {
    println!("Config loaded: {contents}");
} else {
    println!("Could not read config, using defaults");
}
```

## if let con else if y else if let — encadenamiento

Se pueden encadenar multiples patrones con `else if let`,
y mezclar con condiciones booleanas normales:

```rust
let a: Option<i32> = None;
let b: Result<i32, &str> = Ok(42);
let c: Option<i32> = Some(99);

if let Some(x) = a {
    println!("a contains: {x}");
} else if let Ok(y) = b {
    println!("b is Ok: {y}");
} else if let Some(z) = c {
    println!("c contains: {z}");
} else {
    println!("Nothing matched");
}
// Salida: b is Ok: 42
// Se evaluan en orden. La primera rama que coincide se ejecuta.
```

```rust
// Se puede mezclar else if (condicion booleana)
// con else if let (pattern matching):
let maybe_name: Option<&str> = None;
let use_default = true;

if let Some(name) = maybe_name {
    println!("Hello, {name}");
} else if use_default {
    println!("Hello, default user");
} else {
    println!("No user found");
}
// Salida: Hello, default user
```

## while let — loop con pattern matching

`while let` repite un bloque mientras el patron coincida.
Cuando deja de coincidir, el loop termina automaticamente:

```rust
let mut stack: Vec<i32> = vec![1, 2, 3, 4, 5];

// pop() devuelve Some(value) si quedan elementos, None si esta vacio.
while let Some(top) = stack.pop() {
    println!("Popped: {top}");
}
// Salida: 5, 4, 3, 2, 1 (orden inverso)
// Cuando pop() devuelve None, el patron no coincide → loop termina.

// Equivale a:
// loop {
//     match stack.pop() {
//         Some(top) => println!("{top}"),
//         _ => break,
//     }
// }
```

```rust
// Otro uso comun: channels en concurrencia.
// recv() devuelve Ok(value) mientras haya mensajes,
// Err cuando el canal se cierra → loop termina.
use std::sync::mpsc;

fn main() {
    let (tx, rx) = mpsc::channel();
    tx.send(1).unwrap();
    tx.send(2).unwrap();
    drop(tx);

    while let Ok(value) = rx.recv() {
        println!("Received: {value}");
    }
}
```

## Cuando usar if let vs match

La eleccion depende de cuantos patrones necesitas manejar
y si la exhaustividad importa:

```rust
// REGLA GENERAL:
//   - 1 patron (con o sin else)  → if let
//   - 2 patrones con if let/else → ambos son validos
//   - 3 o mas patrones           → match
//   - Necesitas exhaustividad    → match
```

```rust
// EXHAUSTIVIDAD — la razon principal para preferir match.
enum Status {
    Active,
    Inactive,
    Suspended,
}

// Si agregamos Banned a Status, match genera error de compilacion:
match status {
    Status::Active => println!("active"),
    Status::Inactive => println!("inactive"),
    Status::Suspended => println!("suspended"),
    // Falta Banned → el compilador lo rechaza.
}

// Con if let, el compilador NO avisa si olvidas un caso:
if let Status::Active = status {
    println!("active");
}
// Agregas Banned y este codigo compila sin advertencias.
```

## if let con enums propios

`if let` funciona con cualquier enum, no solo `Option` y `Result`.
Permite destructurar campos y patrones anidados:

```rust
enum Shape {
    Circle { radius: f64 },
    Rectangle { width: f64, height: f64 },
    Triangle { base: f64, height: f64 },
}

let shape = Shape::Circle { radius: 5.0 };

if let Shape::Circle { radius } = shape {
    let area = std::f64::consts::PI * radius * radius;
    println!("Circle area: {area:.2}");
}
// Salida: Circle area: 78.54
```

```rust
// Patrones anidados — destructurar campos internos:
enum Message {
    Text(String),
    Image { url: String, dimensions: (u32, u32) },
    Quit,
}

let msg = Message::Image {
    url: String::from("photo.png"),
    dimensions: (1920, 1080),
};

if let Message::Image { url, dimensions: (w, h) } = &msg {
    println!("Image: {url} ({w}x{h})");
}
// Salida: Image: photo.png (1920x1080)
```

## if let y ownership

Cuando `if let` extrae un valor de un `Option<String>` u otro tipo
que posee datos en el heap, el valor se **mueve** fuera del contenedor:

```rust
let option_string: Option<String> = Some(String::from("hello"));

// Esto MUEVE el String fuera del Option:
if let Some(s) = option_string {
    println!("Got: {s}");
}
// option_string ya no es usable. El String se movio a s.
// println!("{:?}", option_string); // Error: value used after move
```

```rust
// Para PRESTAR sin mover, tres formas:

let opt = Some(String::from("hello"));

// Forma 1: ref en el patron (estilo clasico)
if let Some(ref s) = opt {
    println!("Borrowed: {s}");
}

// Forma 2: match ergonomics (la mas idiomatica en Rust moderno)
if let Some(s) = &opt {
    println!("Borrowed: {s}"); // s es &String automaticamente
}

// Forma 3: as_ref()
if let Some(s) = opt.as_ref() {
    println!("Borrowed: {s}");
}

println!("Still valid: {:?}", opt); // OK en las tres formas
```

```rust
// Mutabilidad: ref mut permite modificar el valor en su lugar.
let mut option_data: Option<Vec<i32>> = Some(vec![1, 2, 3]);

if let Some(ref mut data) = option_data {
    data.push(4); // modifica el Vec dentro del Option
}
println!("{option_data:?}"); // Some([1, 2, 3, 4])
```

## Comparacion con otros lenguajes

```swift
// Swift — tiene if let con sintaxis similar:
// let name: String? = "Alice"
// if let n = name {
//     print("Hello, \(n)")
// }
//
// Diferencias con Rust:
// - Swift: if let n = name (desenvuelve Optional implicitamente)
// - Rust: if let Some(n) = name (patron Some() explicito)
// - Swift usa optional chaining (?) como alternativa comun
```

```c
// C — no tiene pattern matching.
// int *ptr = get_value();
// if (ptr != NULL) {
//     printf("Got: %d\n", *ptr);
// }
//
// En C el programador debe recordar verificar NULL.
// Olvidarlo compila sin error y causa undefined behavior.
// En Rust, Option<T> fuerza el manejo del caso None.
```

```python
# Python — no tiene equivalente directo.
# Lo mas parecido:
#   value = get_value()
#   if value is not None:
#       use(value)
#
# Con walrus operator (3.8+):
#   if (m := pattern.match(text)) is not None:
#       print(m.group(1))
#
# Pero ninguno destructura. Python 3.10 tiene match/case
# pero no tiene if let.
```

## Anti-patron: if let cuando match seria mas claro

Usar `if let` con demasiadas ramas encadenadas produce codigo
dificil de leer y pierde la verificacion de exhaustividad:

```rust
enum Command { Start, Stop, Pause, Resume, Restart }

// ANTI-PATRON: cadena larga de if let
fn handle_command_bad(cmd: Command) {
    if let Command::Start = cmd {
        println!("Starting...");
    } else if let Command::Stop = cmd {
        println!("Stopping...");
    } else if let Command::Pause = cmd {
        println!("Pausing...");
    } else if let Command::Resume = cmd {
        println!("Resuming...");
    } else {
        println!("Unknown command");
    }
    // Si agregas Restart, el else lo atrapa silenciosamente.
    // No hay error de compilacion. Mas largo que match.
}

// CORRECTO: usar match
fn handle_command_good(cmd: Command) {
    match cmd {
        Command::Start => println!("Starting..."),
        Command::Stop => println!("Stopping..."),
        Command::Pause => println!("Pausing..."),
        Command::Resume => println!("Resuming..."),
        Command::Restart => println!("Restarting..."),
    }
    // Agregar una variante nueva genera error de compilacion.
}
```

```rust
// RESUMEN:
//
// if let  → una variante te interesa, el resto se ignora o tiene un else simple.
//           Tipico con Option y Result en flujos sencillos.
//
// while let → consumir una secuencia hasta que un patron deje de coincidir.
//             Tipico con iteradores, pop(), recv().
//
// match   → multiples variantes, necesitas exhaustividad,
//           o el codigo tiene logica distinta por caso.
```

---

## Ejercicios

### Ejercicio 1 — if let basico

```rust
// Crear una funcion parse_port(input: &str) -> Option<u16>
// que intente parsear un string como numero de puerto (1-65535).
//
// En main:
//   - Llamar parse_port con "8080", "abc", "0", "70000".
//   - Usar if let para imprimir el puerto solo cuando sea valido.
//   - Usar if let con else para imprimir un mensaje de error cuando no lo sea.
//
// Verificar:
//   "8080"  → "Port: 8080"
//   "abc"   → "Invalid port"
//   "0"     → "Invalid port" (fuera de rango)
//   "70000" → "Invalid port" (fuera de rango)
```

### Ejercicio 2 — while let con stack

```rust
// Crear un Vec<&str> con: ["compile", "link", "test", "deploy"].
// Usar while let con pop() para procesar cada paso en orden inverso.
//
// Luego, crear un Vec<Option<&str>>:
//   [Some("a"), None, Some("b"), Some("c"), None]
// Usar while let con pop() y dentro, un if let para filtrar los None.
//
// Pregunta: si el vector contiene un None en el medio,
// while let Some(item) = stack.pop() NO se detiene en ese None. Por que?
// (Pista: pop() devuelve Option<Option<&str>>, no Option<&str>.)
```

### Ejercicio 3 — if let vs match

```rust
// Dado este enum:
//
// enum HttpStatus {
//     Ok,
//     NotFound,
//     Unauthorized,
//     InternalError,
//     Custom(u16),
// }
//
// Escribir DOS funciones:
//   1. is_success(status: &HttpStatus) -> bool
//      Usar if let. Devuelve true solo para Ok.
//
//   2. status_message(status: &HttpStatus) -> &str
//      Usar match. Devuelve un mensaje para cada variante.
//
// Justificar por que if let es apropiado para is_success
// y por que match es apropiado para status_message.
```
