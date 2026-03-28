# T03 — Lifetime en structs

## Por que los structs necesitan anotaciones de lifetime

Cuando un struct contiene una referencia, el compilador necesita
saber cuanto tiempo vive el dato referenciado. Sin esa informacion,
no puede garantizar que el struct no sobreviva al dato que
referencia. **Sin la anotacion, el compilador rechaza el codigo.**

```rust
// ESTO NO COMPILA:
struct Excerpt {
    part: &str,
}
// error[E0106]: missing lifetime specifier
//     part: &str,
//           ^ expected named lifetime parameter
```

```rust
// Sin la anotacion, seria posible:
//   1. Crear un struct que apunta a un String
//   2. Destruir el String
//   3. Usar el struct → acceso a memoria liberada (dangling reference)
//
// En C, esto es un bug comun (use-after-free).
// En Rust, el compilador lo previene con lifetimes.
```

## Sintaxis basica

La solucion: agregar un parametro de lifetime al struct,
entre angulares despues del nombre:

```rust
struct Excerpt<'a> {
    part: &'a str,
}

// 'a es el parametro de lifetime.
// Lectura: "un Excerpt con lifetime 'a contiene una referencia
//           a un str que vive al menos 'a".
// Cada instancia concreta esta atada a un lifetime especifico
// determinado por el compilador.
```

```rust
struct Excerpt<'a> {
    part: &'a str,
}

fn main() {
    let novel = String::from("Call me Ishmael. Some years ago...");
    let first = novel.split('.').next().expect("No '.'");

    let excerpt = Excerpt { part: first };
    println!("Excerpt: {}", excerpt.part);
    // Excerpt: Call me Ishmael

    // El compilador asigna a 'a el lifetime de novel.
    // No escribimos Excerpt<'algo> — se infiere automaticamente.
}
```

## Que significa la anotacion

La anotacion establece un **contrato**: la instancia del struct
no puede sobrevivir al dato referenciado. El compilador verifica
esto en cada sitio de uso.

```rust
struct Excerpt<'a> {
    part: &'a str,
}

fn main() {
    let excerpt;
    {
        let text = String::from("Hello world");
        excerpt = Excerpt { part: &text };
    } // text se destruye aqui
    println!("{}", excerpt.part); // ERROR
}
// error[E0597]: `text` does not live long enough
//         excerpt = Excerpt { part: &text };
//                                   ^^^^^ borrowed value does not live long enough
```

```rust
// CORRECTO — excerpt no sobrevive a text:
struct Excerpt<'a> {
    part: &'a str,
}

fn main() {
    let text = String::from("Hello world");
    let excerpt = Excerpt { part: &text };
    println!("{}", excerpt.part);
    // Hello world
    // Ambos se destruyen al final de main.
    // excerpt se destruye antes que text (orden inverso).
}
```

## Multiples referencias en structs

Un struct puede contener varias referencias con un lifetime
compartido o lifetimes independientes:

```rust
// Un solo lifetime — ambas referencias comparten 'a:
struct Pair<'a> {
    first: &'a str,
    second: &'a str,
}

fn main() {
    let x = String::from("hello");
    {
        let y = String::from("world");
        let pair = Pair { first: &x, second: &y };
        println!("{} {}", pair.first, pair.second); // OK aqui
    }
    // pair no puede vivir fuera del bloque: y ya murio.
    // Un solo lifetime 'a = el mas corto de los dos.
}
```

```rust
// Dos lifetimes — el compilador rastrea cada referencia por separado:
struct Pair<'a, 'b> {
    first: &'a str,
    second: &'b str,
}

// Cuando usar uno vs multiples:
//
// UN lifetime ('a):
//   - Las referencias estan relacionadas
//   - Quieres retornar cualquiera desde un metodo
//   - Mas simple
//
// Multiples lifetimes ('a, 'b):
//   - Las referencias son independientes
//   - Necesitas mas flexibilidad
//
// Regla practica: empieza con uno. Si el compilador se queja, agrega otro.
```

## Metodos en structs con lifetimes

Para implementar metodos, debes declarar el lifetime en el
bloque `impl`:

```rust
struct Excerpt<'a> {
    part: &'a str,
}

impl<'a> Excerpt<'a> {
    // Metodo sin referencias en el retorno — sin lifetime adicional:
    fn level(&self) -> i32 {
        3
    }

    // Metodo que retorna referencia.
    // Regla de elision 3: &self → el retorno recibe su lifetime.
    fn first_word(&self) -> &str {
        self.part.split_whitespace().next().unwrap_or("")
    }
    // Equivale a: fn first_word(&'a self) -> &'a str

    // Retorno con lifetime 'a explicito — atado al dato original,
    // no al lifetime de &self:
    fn get_part(&self) -> &'a str {
        self.part
    }
}

fn main() {
    let text = String::from("Hello beautiful world");
    let excerpt = Excerpt { part: &text };

    println!("Level: {}", excerpt.level());       // Level: 3
    println!("First: {}", excerpt.first_word());   // First: Hello
}
```

```rust
// Diferencia entre &str y &'a str en metodos:
struct Excerpt<'a> {
    part: &'a str,
}

impl<'a> Excerpt<'a> {
    fn get_part(&self) -> &'a str { self.part }
}

fn main() {
    let novel = String::from("Call me Ishmael. Some years ago...");
    let part;
    {
        let excerpt = Excerpt { part: novel.split('.').next().unwrap() };
        part = excerpt.get_part();
    } // excerpt se destruye aqui
    // part sigue valido: tiene lifetime 'a (el de novel), no el de excerpt.
    println!("{}", part); // Call me Ishmael
}
```

## Struct con datos owned y borrowed

Un struct puede mezclar datos propios (owned) con referencias
(borrowed). Las reglas de lifetime solo aplican a las referencias:

```rust
struct Config<'a> {
    name: String,       // owned — Config es dueno
    data: &'a [u8],     // borrowed — referencia datos externos
}

fn main() {
    let raw_data: Vec<u8> = vec![0x48, 0x65, 0x6C, 0x6C, 0x6F];
    let config = Config {
        name: String::from("my_config"),
        data: &raw_data,
    };
    println!("Config: {}, data len: {}", config.name, config.data.len());
    // Config: my_config, data len: 5
}
```

```rust
struct Config<'a> {
    name: String,
    data: &'a [u8],
}

fn main() {
    let config;
    {
        let raw = vec![1, 2, 3];
        config = Config { name: String::from("test"), data: &raw };
    }
    // ERROR: raw ya no existe, config.data es invalido.
    // Aunque config.name es owned, el struct completo
    // esta atado al lifetime de data. No se puede usar parcialmente.
}
```

```rust
// Patron comun — parser zero-copy:
struct Request<'a> {
    method: String,                   // owned — se normaliza
    path: &'a str,                    // borrowed — buffer original
    headers: Vec<(&'a str, &'a str)>, // borrowed — pares clave-valor
    body: Option<&'a [u8]>,           // borrowed — slice del buffer
}
// El Request no puede vivir mas que el buffer que parseo.
```

## Patrones comunes

Dos patrones frecuentes: parsers que toman prestado el input
e iteradores que toman prestada una coleccion.

```rust
struct Parser<'input> {
    source: &'input str,
    pos: usize,
}

// 'input es descriptivo — indica de donde viene el lifetime.

impl<'input> Parser<'input> {
    fn new(source: &'input str) -> Self {
        Parser { source, pos: 0 }
    }

    fn remaining(&self) -> &'input str {
        &self.source[self.pos..]
    }

    fn advance(&mut self, n: usize) {
        self.pos = (self.pos + n).min(self.source.len());
    }

    fn take_while<F>(&mut self, predicate: F) -> &'input str
    where
        F: Fn(char) -> bool,
    {
        let start = self.pos;
        for ch in self.remaining().chars() {
            if predicate(ch) { self.pos += ch.len_utf8(); }
            else { break; }
        }
        &self.source[start..self.pos]
    }
}

fn main() {
    let input = "hello world";
    let mut parser = Parser::new(input);

    let word = parser.take_while(|c| c.is_alphabetic());
    println!("{}", word);   // hello
    parser.advance(1);
    let word2 = parser.take_while(|c| c.is_alphabetic());
    println!("{}", word2);  // world
}
```

```rust
// Iterador que toma prestada una coleccion:
struct WindowIter<'a, T> {
    data: &'a [T],
    size: usize,
    pos: usize,
}

impl<'a, T> Iterator for WindowIter<'a, T> {
    type Item = &'a [T];

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos + self.size <= self.data.len() {
            let window = &self.data[self.pos..self.pos + self.size];
            self.pos += 1;
            Some(window)
        } else {
            None
        }
    }
}

fn main() {
    let nums = vec![1, 2, 3, 4, 5];
    let iter = WindowIter { data: &nums, size: 3, pos: 0 };
    for w in iter { println!("{:?}", w); }
    // [1, 2, 3]
    // [2, 3, 4]
    // [3, 4, 5]
}
```

## Cuando usar referencias vs datos owned

```rust
// REFERENCIAS — zero-copy, eficiente, pero restringido:
struct LogEntry<'a> {
    timestamp: &'a str,
    message: &'a str,
}
// No copia datos, menos memoria (solo punteros).
// Pero el struct no puede vivir mas que el dato original.

// DATOS OWNED — flexible, pero mas asignaciones:
struct LogEntryOwned {
    timestamp: String,
    message: String,
}
// Sin restricciones de lifetime, puedes mover libremente.
// Pero copia datos y usa mas memoria.
```

```rust
// Guia:
//
// Usa REFERENCIAS cuando:
//   - Parseas datos temporalmente (parsers, tokenizers)
//   - El rendimiento importa y el dato ya esta en memoria
//   - El struct es de vida corta (mismo scope)
//
// Usa DATOS OWNED cuando:
//   - El struct debe vivir independientemente del dato
//   - Retornas el struct de una funcion
//   - Almacenas en colecciones de larga duracion
//
// MEZCLA ambos cuando algunos campos necesitan transformacion
// y otros solo leen el dato original.
```

## Variantes de enum con lifetimes

Los enums siguen las mismas reglas. Si alguna variante contiene
una referencia, el enum necesita un parametro de lifetime:

```rust
enum Token<'a> {
    Word(&'a str),     // referencia — necesita lifetime
    Number(i64),       // owned — no afecta
    Punctuation(char), // owned — no afecta
    Whitespace,        // sin datos — no afecta
}

// Solo Word tiene referencia, pero el enum completo lleva 'a.

fn main() {
    let source = "hello 42!";
    let tokens: Vec<Token> = vec![
        Token::Word(&source[0..5]),
        Token::Whitespace,
        Token::Number(42),
        Token::Punctuation('!'),
    ];
    for t in &tokens {
        match t {
            Token::Word(w) => println!("WORD: {}", w),
            Token::Number(n) => println!("NUM: {}", n),
            Token::Punctuation(p) => println!("PUNCT: {}", p),
            Token::Whitespace => println!("SPACE"),
        }
    }
    // WORD: hello  SPACE  NUM: 42  PUNCT: !
}
```

```rust
// Enum con metodos — mismo patron que structs:
enum Value<'a> {
    Text(&'a str),
    Integer(i64),
}

impl<'a> Value<'a> {
    fn as_text(&self) -> Option<&'a str> {
        match self { Value::Text(s) => Some(s), _ => None }
    }

    fn type_name(&self) -> &'static str {
        match self { Value::Text(_) => "text", Value::Integer(_) => "integer" }
    }
}
```

## Derived traits y lifetimes

Los derive macros estandar funcionan con parametros de lifetime:

```rust
#[derive(Debug, Clone, PartialEq)]
struct Excerpt<'a> {
    part: &'a str,
}

fn main() {
    let text = String::from("Hello world");
    let e1 = Excerpt { part: &text };
    let e2 = e1.clone();
    println!("{:?}", e1);            // Excerpt { part: "Hello world" }
    println!("Equal: {}", e1 == e2); // Equal: true
}
```

```rust
// Copy funciona si todos los campos son Copy.
// Las referencias (&T) son Copy:
#[derive(Debug, Clone, Copy, PartialEq)]
struct Span<'a> {
    text: &'a str,
    line: usize,
    column: usize,
}

fn main() {
    let span = Span { text: "main", line: 1, column: 4 };
    let span2 = span;          // copia, no move
    println!("{:?}", span);    // span sigue valido
    println!("{:?}", span2);   // Span { text: "main", line: 1, column: 4 }
}
```

```rust
// Hash — util para claves en HashSet/HashMap:
use std::collections::HashSet;

#[derive(Debug, Hash, PartialEq, Eq)]
struct Symbol<'a> {
    name: &'a str,
    scope: usize,
}

fn main() {
    let mut symbols = HashSet::new();
    symbols.insert(Symbol { name: "x", scope: 0 });
    symbols.insert(Symbol { name: "y", scope: 0 });
    symbols.insert(Symbol { name: "x", scope: 0 }); // duplicado
    println!("Unique: {}", symbols.len()); // Unique: 2
}
```

```rust
// Default no se puede derivar con &str, pero se implementa manualmente:
#[derive(Debug)]
struct Config<'a> {
    name: &'a str,
    verbose: bool,
}

impl<'a> Default for Config<'a> {
    fn default() -> Self {
        Config {
            name: "default",  // &'static str valido para cualquier 'a
            verbose: false,
        }
    }
}

fn main() {
    let config = Config::default();
    println!("{:?}", config); // Config { name: "default", verbose: false }
}
```

---

## Ejercicios

### Ejercicio 1 — Struct basico con lifetime

```rust
// Crear un struct Highlight con:
//   - text: &str
//   - color: &str
//
// Implementar Debug con derive y un metodo display_tag() que
// retorne un String: "[COLOR: text]" (ej: "[red: error found]")
//
// En main():
//   1. Crear un String "error found" y un Highlight que lo referencie
//   2. Imprimir con {:?} y con display_tag()
//   3. Mover la creacion del String a un bloque interno y usar
//      el Highlight fuera → verificar que el compilador lo rechaza
```

### Ejercicio 2 — Struct con owned y borrowed

```rust
// Crear un struct Record con:
//   - id: u64 (owned), name: String (owned), tags: Vec<&str> (borrowed)
//
// Implementar:
//   - new(id, name, tags)
//   - has_tag(&self, tag: &str) -> bool
//   - summary(&self) -> String: "Record #id: name [tag1, tag2, ...]"
//
// En main():
//   1. Crear Vec<String> con tags ["urgent", "backend", "bug"]
//   2. Crear Record referenciando los elementos del Vec
//   3. Verificar has_tag("urgent") → true, has_tag("frontend") → false
//   4. Imprimir summary()
```

### Ejercicio 3 — Parser simple

```rust
// Crear un struct KeyValueParser que toma prestado un string.
// Formato del input: "key1=val1;key2=val2;key3=val3"
//
// Implementar:
//   - new(input: &str) -> KeyValueParser
//   - parse(&self) -> Vec<(&str, &str)>  (zero-copy)
//   - get(&self, key: &str) -> Option<&str>
//
// En main():
//   1. Input: "host=localhost;port=8080;debug=true"
//   2. Parsear e imprimir cada par
//   3. get("port") → Some("8080"), get("missing") → None
//   4. Verificar zero-copy imprimiendo punteros con {:p}
```
