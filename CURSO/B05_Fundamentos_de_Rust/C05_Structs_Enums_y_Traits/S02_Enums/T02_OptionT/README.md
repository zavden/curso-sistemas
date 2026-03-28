# T02 — Option\<T\>

## Por que no existe null

En la mayoria de lenguajes (C, Java, Python, Go) existe un valor
especial llamado `null` (o `nil`, `None`, `NULL`) que representa
la ausencia de un valor. Tony Hoare, el inventor de las referencias
nulas, lo llamo su "error de mil millones de dolares" en 2009:

```text
La referencia nula causa:
  - NullPointerException en Java
  - Segfault / undefined behavior en C (desreferenciar NULL)
  - TypeError: cannot read property of null en JavaScript
  - panic: runtime error: invalid memory address en Go

El problema fundamental: el compilador no te obliga a verificar
si un valor puede ser nulo. Puedes escribir codigo que asume
que un valor existe, y el error aparece en tiempo de ejecucion.
```

```c
// En C, el compilador no te avisa del peligro:
char *name = get_user_name();  // puede retornar NULL
printf("Hello, %s\n", name);   // segfault si name es NULL
// Compila sin warnings. Falla en ejecucion.
```

Rust elimina el problema desde la raiz: **no existe null**.
Ningun valor en Rust puede ser "nada" de forma implicita.
Si una funcion puede no retornar un valor, su tipo de retorno
lo dice explicitamente usando `Option<T>`:

```rust
// En Rust, la ausencia es parte del tipo:
fn find_user(id: u32) -> Option<String> {
    // Retorna Some(name) si existe, None si no.
    // El compilador OBLIGA a quien llama a manejar ambos casos.
}

// Esto NO compila:
// let name: String = find_user(42);
// Error: expected String, found Option<String>

// DEBES manejar la posibilidad de ausencia:
let name: String = match find_user(42) {
    Some(n) => n,
    None => String::from("unknown"),
};
```

## Definicion de Option

`Option<T>` es un enum generico definido en la biblioteca estandar:

```rust
// Definicion real en std (simplificada):
enum Option<T> {
    Some(T),    // hay un valor de tipo T
    None,       // no hay valor
}

// T es un parametro de tipo — Option funciona con cualquier tipo.
// Option<i32>    → puede contener un i32 o nada
// Option<String> → puede contener un String o nada
// Option<Vec<u8>> → puede contener un Vec<u8> o nada
```

`Option`, `Some` y `None` estan en el preludio de Rust. No necesitas
importar nada — estan disponibles en todo programa:

```rust
// Usas directamente, sin use:
let x: Option<i32> = Some(42);
let y: Option<i32> = None;

// Rust infiere el tipo de Some automaticamente:
let x = Some(42);       // Option<i32>
let s = Some("hello");  // Option<&str>

// Para None, a veces necesitas anotar el tipo:
let n: Option<i32> = None;
let n = None::<i32>;    // alternativa con turbofish
```

## Crear valores Option

```rust
// Funciones de la biblioteca estandar que retornan Option:

// Vec::get — acceso seguro por indice:
let nums = vec![10, 20, 30];
let second = nums.get(1);   // Some(&20)
let tenth = nums.get(9);    // None (fuera de rango)
// Compara con nums[9] que causaria panic.

// str::find — buscar un caracter:
let pos = "hello".find('l');   // Some(2)
let pos = "hello".find('z');   // None

// HashMap::get — buscar una clave:
use std::collections::HashMap;
let mut scores = HashMap::new();
scores.insert("Alice", 95);
let alice = scores.get("Alice");  // Some(&95)
let bob = scores.get("Bob");      // None
```

```rust
// Convertir Result a Option con .ok():
let num: Option<i32> = "42".parse().ok();    // Some(42)
let bad: Option<i32> = "abc".parse().ok();   // None
// .ok() descarta el error y retorna Option<T>.
```

## Pattern matching con Option

La forma mas explicita de manejar `Option<T>` es `match`.
El compilador te obliga a cubrir ambas variantes:

```rust
fn describe(opt: Option<i32>) -> String {
    match opt {
        Some(x) => format!("Value: {}", x),
        None => String::from("No value"),
    }
}

// match es exhaustivo — si olvidas una variante, no compila:
// match opt {
//     Some(x) => format!("Value: {}", x),
//     // Error: non-exhaustive patterns: `None` not covered
// }
```

```rust
// if let — cuando solo te interesa el caso Some:
let config_max = Some(100);
if let Some(max) = config_max {
    println!("Maximum configured to {}", max);
}
// Si es None, el bloque simplemente no se ejecuta.

// if let con else:
let opt: Option<i32> = None;
if let Some(x) = opt {
    println!("Got: {}", x);
} else {
    println!("Got nothing");
}
```

```rust
// let-else (Rust 1.65+) — extraer o divergir:
fn process(opt: Option<i32>) {
    let Some(x) = opt else {
        println!("No value, returning early");
        return;  // el else DEBE divergir (return, break, continue, panic!)
    };
    // Aqui x es un i32, no Option<i32>.
    println!("Processing: {}", x);
}

process(Some(10));  // "Processing: 10"
process(None);      // "No value, returning early"
```

## La familia unwrap

Los metodos `unwrap` extraen el valor interior de un `Some`,
pero tienen diferentes comportamientos cuando el valor es `None`:

```rust
// unwrap() — extrae el valor o hace panic:
let x = Some(42);
let val = x.unwrap();   // 42

let y: Option<i32> = None;
// y.unwrap();  →  panic: called `Option::unwrap()` on a `None` value

// expect("msg") — como unwrap, pero con mensaje personalizado:
let port: Option<u16> = None;
// port.expect("PORT must be configured");
// panic: PORT must be configured
```

```rust
// unwrap_or(default) — retorna un valor por defecto si es None:
let timeout: Option<u64> = None;
let t = timeout.unwrap_or(30);   // 30 — no hace panic

let timeout: Option<u64> = Some(60);
let t = timeout.unwrap_or(30);   // 60 — usa el valor existente

// unwrap_or_else(|| ...) — calcula el default de forma lazy:
let config: Option<String> = None;
let val = config.unwrap_or_else(|| {
    // Este closure solo se ejecuta si config es None.
    expensive_default_calculation()
});

// unwrap_or_default() — usa el valor Default del tipo:
let count: Option<i32> = None;
let c = count.unwrap_or_default();   // 0 (Default de i32)
let name: Option<String> = None;
let n = name.unwrap_or_default();    // "" (Default de String)
// Solo funciona si T implementa el trait Default.
```

```rust
// Resumen de cuando usar cada uno:
//
// unwrap()             → prototipado rapido, o cuando None es imposible
// expect("msg")        → cuando None es un bug y quieres un mensaje claro
// unwrap_or(val)       → cuando tienes un default simple y barato
// unwrap_or_else(f)    → cuando calcular el default es costoso
// unwrap_or_default()  → cuando el Default del tipo es un buen fallback
```

## Transformaciones: map, and_then, filter

Estos metodos transforman el valor dentro de un `Option`
sin extraerlo manualmente con `match`:

```rust
// map — transforma el valor interior, None pasa sin cambios:
let num: Option<i32> = Some(5);
let doubled = num.map(|x| x * 2);          // Some(10)

let nothing: Option<i32> = None;
let doubled = nothing.map(|x| x * 2);      // None
// El closure nunca se ejecuta si es None.

// map encadena bien:
let name: Option<String> = Some(String::from("  Alice  "));
let clean = name
    .map(|s| s.trim().to_string())
    .map(|s| s.to_uppercase());
// Some("ALICE")
```

```rust
// and_then — como map, pero el closure retorna Option (flat map).
// Evita Option<Option<T>> anidados.
fn parse_port(s: &str) -> Option<u16> {
    s.parse::<u16>().ok()
}

let input: Option<&str> = Some("8080");
let port = input.and_then(parse_port);   // Some(8080)

let input: Option<&str> = Some("abc");
let port = input.and_then(parse_port);   // None (parse fallo)

let input: Option<&str> = None;
let port = input.and_then(parse_port);   // None (input era None)
```

```rust
// Diferencia clave entre map y and_then:
let input = Some("42");

// map — el closure retorna T, map lo envuelve en Some:
let result = input.map(|s| s.parse::<i32>());
// Option<Result<i32, ParseIntError>> — tipo anidado

// and_then — el closure retorna Option<T> directamente:
let result = input.and_then(|s| s.parse::<i32>().ok());
// Option<i32> — tipo plano
```

```rust
// or — retorna self si es Some, el otro Option si es None:
let a: Option<i32> = Some(1);
let c: Option<i32> = None;
assert_eq!(a.or(Some(2)), Some(1));   // a es Some, usa a
assert_eq!(c.or(Some(2)), Some(2));   // c es None, usa alternativa

// or_else — version lazy:
let cached: Option<String> = None;
let value = cached.or_else(|| fetch_from_database());

// filter — mantiene el valor solo si cumple una condicion:
let x = Some(10);
let filtered = x.filter(|v| *v > 5);    // Some(10)
let filtered = x.filter(|v| *v > 20);   // None
```

## Combinar Options

```rust
// zip — combina dos Options en un Option de tupla:
let name: Option<&str> = Some("Alice");
let age: Option<u32> = Some(30);
let none: Option<u32> = None;

let pair = name.zip(age);    // Some(("Alice", 30))
let pair = name.zip(none);   // None (si cualquiera es None)

// unzip — operacion inversa:
let pair: Option<(i32, &str)> = Some((1, "one"));
let (a, b) = pair.unzip();   // (Some(1), Some("one"))
```

```rust
// flatten — desanida Option<Option<T>> a Option<T>:
let nested: Option<Option<i32>> = Some(Some(42));
let flat = nested.flatten();   // Some(42)

let nested: Option<Option<i32>> = Some(None);
let flat = nested.flatten();   // None

// Si llegas a tener muchos niveles anidados, and_then suele ser mejor.
```

## Option como iterador

`Option<T>` implementa `IntoIterator`, produciendo 0 o 1 elementos.
Esto permite usar Option en contextos donde se espera un iterador:

```rust
// Iterar sobre un Option:
let x = Some(42);
for val in x {
    println!("{}", val);  // imprime 42
}
// None no produce elementos — el for no ejecuta el cuerpo.

// Incluir valores opcionales en cadenas de iteradores:
let required = vec![1, 2, 3];
let optional: Option<i32> = Some(4);
let extra: Option<i32> = None;

let all: Vec<i32> = required
    .into_iter()
    .chain(optional)    // agrega 4
    .chain(extra)       // agrega nada
    .collect();
// [1, 2, 3, 4]
```

```rust
// Filtrar None de una coleccion de Options:
let items: Vec<Option<i32>> = vec![Some(1), None, Some(3), None, Some(5)];
let values: Vec<i32> = items.into_iter().flatten().collect();
// [1, 3, 5]

// Equivalente con filter_map:
let items = vec![Some(1), None, Some(3), None, Some(5)];
let values: Vec<i32> = items.into_iter().filter_map(|x| x).collect();
// [1, 3, 5]
```

## El operador ? con Option

En funciones que retornan `Option`, el operador `?` propaga `None`
automaticamente — igual que propaga errores en funciones que retornan
`Result`:

```rust
// Sin el operador ?:
fn get_first_char_len(s: Option<&str>) -> Option<usize> {
    match s {
        Some(text) => match text.chars().next() {
            Some(c) => Some(c.len_utf8()),
            None => None,
        },
        None => None,
    }
}

// Con el operador ?:
fn get_first_char_len(s: Option<&str>) -> Option<usize> {
    let text = s?;                   // retorna None si s es None
    let first = text.chars().next()?; // retorna None si string vacio
    Some(first.len_utf8())
}
```

```rust
// Ejemplo practico — navegacion de estructuras anidadas:
struct Config {
    database: Option<DatabaseConfig>,
}
struct DatabaseConfig {
    port: Option<u16>,
}

fn get_db_port(config: &Config) -> Option<u16> {
    let db = config.database.as_ref()?;  // None si no hay config de DB
    let port = db.port?;                  // None si no hay puerto
    Some(port)
}

// ? solo funciona en funciones que retornan Option (o Result).
// En main() sin tipo de retorno compatible, no compila.
```

## Patrones comunes

```rust
// Encadenamiento de metodos — estilo funcional fluido:
fn process_input(input: Option<String>) -> Option<u64> {
    input
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .and_then(|s| s.parse::<u64>().ok())
        .map(|n| n * 2)
}

assert_eq!(process_input(Some("  21  ".into())), Some(42));
assert_eq!(process_input(Some("  ".into())), None);
assert_eq!(process_input(None), None);
```

```rust
// Convertir Option a Result con ok_or y ok_or_else:
let opt: Option<i32> = Some(42);
let result: Result<i32, &str> = opt.ok_or("value missing");   // Ok(42)

let opt: Option<i32> = None;
let result: Result<i32, &str> = opt.ok_or("value missing");   // Err("value missing")

// ok_or_else — calcula el error de forma lazy:
let opt: Option<i32> = None;
let result = opt.ok_or_else(|| format!("missing at line {}", line!()));
```

```rust
// is_some() e is_none() — inspeccionar sin extraer:
let x: Option<i32> = Some(10);
assert!(x.is_some());
assert!(!x.is_none());

// Pero si vas a usar el valor, if let es mejor:
// Mal:
// if config.timeout.is_some() { let t = config.timeout.unwrap(); }
// Bien:
// if let Some(t) = config.timeout { /* usar t */ }
```

```rust
// as_ref() y as_mut() — prestar sin consumir:
let name: Option<String> = Some(String::from("Alice"));

// Sin as_ref, map consumiria name (move):
// let len = name.map(|s| s.len());  // name ya no es usable

// Con as_ref, solo presta:
let len = name.as_ref().map(|s| s.len());   // Some(5)
println!("{:?}", name);   // name sigue disponible

// as_mut: &mut Option<T> → Option<&mut T>
let mut data: Option<Vec<i32>> = Some(vec![1, 2, 3]);
if let Some(v) = data.as_mut() {
    v.push(4);
}
// data es ahora Some([1, 2, 3, 4])
```

```rust
// take() — extrae el valor y deja None en su lugar:
let mut slot: Option<String> = Some(String::from("hello"));
let taken = slot.take();   // taken = Some("hello")
assert_eq!(slot, None);    // slot ahora es None

// replace() — inserta un nuevo valor y retorna el anterior:
let mut x = Some(42);
let old = x.replace(100);  // old = Some(42), x = Some(100)

// get_or_insert() — inicializa si es None, retorna &mut al valor:
let mut cache: Option<Vec<String>> = None;
let items = cache.get_or_insert_with(Vec::new);
items.push(String::from("first"));
// cache ahora es Some(["first"])
```

## Option vs punteros nulos — resumen

```text
Lenguaje     Ausencia        Verificacion    Fallo
-----------  --------------  --------------  -------------------
C            NULL            manual          segfault / UB
Java         null            manual          NullPointerException
Python       None            manual          AttributeError
Go           nil             manual          panic: nil pointer
JavaScript   null/undefined  manual          TypeError
Rust         Option<T>       compilador      no compila

En Rust, el compilador rechaza codigo que no maneja None.
No hay forma de olvidarse de verificar — el tipo lo exige.

Ademas, Option<T> tiene costo cero en muchos casos:
Option<&T> y Option<Box<T>> ocupan el mismo espacio que un puntero,
usando la optimizacion de "niche" (el valor 0x0 representa None).
```

---

## Ejercicios

### Ejercicio 1 — Manejo basico de Option

```rust
// Implementar una funcion que reciba un slice de enteros y un indice,
// y retorne el doble del valor en ese indice, o None si no existe:
//
// fn double_at(items: &[i32], index: usize) -> Option<i32>
//
// Usar .get() y .map()
//
// Verificar:
//   double_at(&[10, 20, 30], 1) == Some(40)
//   double_at(&[10, 20, 30], 5) == None
//   double_at(&[], 0)           == None
```

### Ejercicio 2 — Encadenar con and_then

```rust
// Dada una estructura:
// struct User {
//     name: String,
//     email: Option<String>,
// }
//
// Y un vector de usuarios, implementar:
// fn find_email(users: &[User], name: &str) -> Option<String>
//
// Debe buscar el usuario por nombre y retornar su email.
// Si el usuario no existe O si su email es None, retornar None.
//
// Usar .iter().find() y .and_then()
//
// Verificar con al menos:
//   - Un usuario con email → Some(email)
//   - Un usuario sin email → None
//   - Un nombre que no existe → None
```

### Ejercicio 3 — El operador ? con Option

```rust
// Implementar una funcion que extraiga un valor de una configuracion
// anidada usando el operador ?:
//
// struct App { config: Option<Config> }
// struct Config { database: Option<DbConfig> }
// struct DbConfig { connection_string: Option<String> }
//
// fn get_connection_string(app: &App) -> Option<&str>
//
// Usar ? para propagar None en cada nivel.
// Verificar con todas las combinaciones:
//   - Todo presente → Some("postgres://...")
//   - database es None → None
//   - connection_string es None → None
//   - config es None → None
```

### Ejercicio 4 — Cadena de transformaciones

```rust
// Implementar una funcion que procese un input opcional:
//
// fn parse_and_validate(input: Option<&str>) -> Option<u32>
//
// 1. Si input es None, retornar None
// 2. Hacer trim del string
// 3. Si esta vacio despues del trim, retornar None
// 4. Parsear a u32 — si falla, retornar None
// 5. Si el numero es mayor que 1000, retornar None
// 6. Retornar el numero multiplicado por 10
//
// Resolver con una cadena: map, filter, and_then (sin match ni if let)
//
// Verificar:
//   parse_and_validate(Some("  42  "))  == Some(420)
//   parse_and_validate(Some("  "))      == None
//   parse_and_validate(Some("abc"))     == None
//   parse_and_validate(Some("2000"))    == None
//   parse_and_validate(None)            == None
```
