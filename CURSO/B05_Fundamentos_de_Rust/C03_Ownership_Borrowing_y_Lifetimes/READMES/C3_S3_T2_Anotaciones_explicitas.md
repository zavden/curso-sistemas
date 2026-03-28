# T02 — Anotaciones explicitas

## Sintaxis de anotacion de lifetime

Las anotaciones de lifetime empiezan con un apostrofo seguido de un
nombre corto en minusculas. Por convencion se usa `'a`, `'b`, `'c`.
Las anotaciones van en la **firma** de la funcion, no en el cuerpo:

```rust
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() {
        x
    } else {
        y
    }
}

// Desglose de la firma:
//   fn longest     — nombre de la funcion
//   <'a>           — declara un parametro de lifetime llamado 'a
//   x: &'a str     — x es una referencia con lifetime 'a
//   y: &'a str     — y es una referencia con lifetime 'a
//   -> &'a str     — el retorno es una referencia con lifetime 'a
```

```rust
// Las anotaciones van entre < > despues del nombre de la funcion,
// igual que los parametros genericos de tipo:

fn foo<'a>(x: &'a i32) -> &'a i32 { x }

// Con genericos de tipo Y lifetimes, lifetimes van primero:
fn bar<'a, T>(x: &'a T) -> &'a T { x }

// Multiples lifetimes:
fn baz<'a, 'b>(x: &'a str, y: &'b str) -> &'a str { x }

// Convenciones de nombre:
//   'a, 'b, 'c     — los mas comunes
//   'src, 'input   — descriptivos, cuando aportan claridad
//   'static        — lifetime especial reservado (vive toda la ejecucion)
```

## Que significan las anotaciones

Las anotaciones de lifetime NO cambian cuanto vive una referencia.
No extienden ni acortan la vida de ningun dato. Lo que hacen es
**describir relaciones** entre los lifetimes de varias referencias
para que el compilador pueda verificar que el codigo es seguro:

```rust
// Esta firma:
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}

// Dice: "el valor de retorno vive AL MENOS tanto como el
// mas corto de los lifetimes de x e y".
//
// El compilador usa esta informacion en los CALL SITES
// (donde se llama a la funcion) para verificar que el
// resultado no se use despues de que x o y dejen de ser validos.
//
// Las anotaciones son un CONTRATO:
//   - La funcion promete devolver algo que vive al menos 'a
//   - El caller debe asegurar que los inputs vivan al menos 'a
//   - El compilador verifica ambas partes
```

```rust
fn main() {
    let string1 = String::from("long string");
    {
        let string2 = String::from("xyz");
        let result = longest(string1.as_str(), string2.as_str());
        println!("{}", result); // OK: string2 todavia vive aqui
    }
    // Si intentaras usar result aqui, el compilador rechazaria el codigo.
    // string2 ya fue destruido, y 'a se reduce al mas corto: el de string2.
}
```

```rust
// Las anotaciones NO crean lifetimes:
fn make_ref<'a>() -> &'a str {
    let s = String::from("hello");
    // &s  // ERROR: s se destruye al final de la funcion.
         // La anotacion 'a no puede "salvar" a s.
    "hello" // Esto SI funciona: los string literals tienen 'static
}
// Moraleja: las anotaciones describen, no crean.
```

## Un solo parametro de lifetime

Cuando una funcion toma una sola referencia y devuelve una referencia,
el lifetime del output esta ligado al del input:

```rust
// Explicito:
fn first_word<'a>(s: &'a str) -> &'a str {
    let bytes = s.as_bytes();
    for (i, &item) in bytes.iter().enumerate() {
        if item == b' ' {
            return &s[0..i];
        }
    }
    &s[..]
}

// El compilador infiere esto automaticamente (elision, regla 2):
fn first_word_elided(s: &str) -> &str {
    // ... mismo cuerpo ...
    s
}

// Ambas firmas son IDENTICAS para el compilador.
// Con un solo input y un output, la anotacion explicita
// no aporta informacion nueva.
```

```rust
// Cuando el retorno solo depende de UNO de los inputs:
fn trim_prefix<'a>(s: &'a str, prefix: &str) -> &'a str {
    // prefix no necesita el mismo lifetime que s.
    // El retorno solo depende de s.
    // La elision asigna lifetimes distintos a s y prefix,
    // y liga el output al primero.
    if s.starts_with(prefix) {
        &s[prefix.len()..]
    } else {
        s
    }
}
```

## Multiples parametros de lifetime

Cuando los inputs tienen relaciones diferentes con el output,
se necesitan lifetimes separados:

```rust
// Un solo lifetime: ambos inputs deben vivir tanto como el output.
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}

// Dos lifetimes: solo x necesita vivir tanto como el output.
fn first_of<'a, 'b>(x: &'a str, y: &'b str) -> &'a str {
    x // solo devolvemos x, nunca y
}

// La diferencia importa para el caller:
fn main() {
    let x = String::from("hello");
    let result;
    {
        let y = String::from("world");
        result = first_of(x.as_str(), y.as_str());
    }
    // y fue destruido, pero result sigue valido porque
    // su lifetime esta ligado a x, que sigue vivo.
    println!("{}", result);
}
```

```rust
// Error comun: usar un solo lifetime cuando deberian ser distintos.

// MAL — obliga a que config viva tanto como data, sin razon:
fn process_bad<'a>(data: &'a [u8], config: &'a str) -> &'a [u8] {
    &data[..1]
}

// BIEN — config tiene su propio lifetime, no restringe a data:
fn process_good<'a, 'b>(data: &'a [u8], config: &'b str) -> &'a [u8] {
    &data[..1]
}
// Con process_bad, el caller no puede destruir config antes de
// terminar de usar el resultado, aunque el resultado no depende
// de config. Es una restriccion innecesaria.
```

```rust
// Cuando NO necesitas multiples lifetimes:
// Si el retorno puede venir de CUALQUIERA de los inputs,
// necesitan el MISMO lifetime:

// ESTO NO COMPILA:
// fn longest_bad<'a, 'b>(x: &'a str, y: &'b str) -> &'a str {
//     if x.len() > y.len() {
//         x   // OK: tipo &'a str
//     } else {
//         y   // ERROR: tipo &'b str, pero se espera &'a str
//     }
// }

// Necesitas el mismo lifetime porque el retorno puede ser cualquiera:
fn longest_good<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
```

## Lifetime bounds

Un lifetime bound `'a: 'b` declara que `'a` vive al menos tanto
como `'b` (se lee: "'a outlives 'b"):

```rust
// Sintaxis: 'a: 'b en la declaracion generica o en clausula where.

fn foo<'a: 'b, 'b>(x: &'a str, y: &'b str) -> &'b str {
    x // 'a vive al menos tanto como 'b, asi que esto es valido
}

// Equivalente con where:
fn bar<'a, 'b>(x: &'a str, y: &'b str) -> &'b str
where
    'a: 'b,
{
    x
}
```

```rust
// Sin el bound, el compilador no sabe que 'a dura al menos tanto como 'b:

// SIN bound — no compila:
// fn bad<'a, 'b>(x: &'a str, _y: &'b str) -> &'b str {
//     x // ERROR: 'a podria ser mas corto que 'b
// }

// CON bound — compila:
fn good<'a: 'b, 'b>(x: &'a str, _y: &'b str) -> &'b str {
    x // OK: 'a >= 'b, asi que x vive lo suficiente
}
```

```rust
// Lifetime bounds con genericos de tipo:
// T: 'a significa "T no contiene referencias con lifetime mas corto que 'a"

struct Important<'a, T: 'a> {
    value: &'a T,
}
// T: 'a asegura que lo que sea T, sus datos internos
// viven al menos tanto como 'a.
```

## La funcion longest — paso a paso

La funcion `longest` es el ejemplo canonico para entender
anotaciones de lifetime:

```rust
// Paso 1: intentar sin anotaciones — NO COMPILA
// fn longest(x: &str, y: &str) -> &str {
//     if x.len() > y.len() { x } else { y }
// }
//
// error[E0106]: missing lifetime specifier
//   |
//   | fn longest(x: &str, y: &str) -> &str {
//   |               ----     ----      ^ expected named lifetime parameter
//   |
// = help: this function's return type contains a borrowed value,
//         but the signature does not say whether it is borrowed from `x` or `y`
//
// El compilador no puede aplicar elision:
//   - Hay dos parametros de referencia, no hay &self
//   - No puede decidir a cual input ligar el output
```

```rust
// Paso 2: anotar con 'a — COMPILA
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}
// El retorno vive al menos tanto como el mas corto de x e y.
```

```rust
// Paso 3: call site valido
fn main() {
    let string1 = String::from("long string is long");
    {
        let string2 = String::from("xyz");
        let result = longest(string1.as_str(), string2.as_str());
        println!("The longest string is {}", result);
        // OK: result se usa dentro del scope donde ambos strings viven.
    }
}
```

```rust
// Paso 4: call site invalido
// fn main() {
//     let string1 = String::from("long string is long");
//     let result;
//     {
//         let string2 = String::from("xyz");
//         result = longest(string1.as_str(), string2.as_str());
//     }
//     println!("{}", result); // ERROR
// }
//
// error[E0597]: `string2` does not live long enough
// result se usa despues de que string2 fue destruido.
// 'a debe cubrir el uso de result, pero string2 muere antes.
```

```rust
// Paso 5: anotaciones incorrectas

// Retorno ligado solo a x, pero el cuerpo puede devolver y:
// fn longest_wrong<'a, 'b>(x: &'a str, y: &'b str) -> &'a str {
//     if x.len() > y.len() {
//         x    // OK: &'a str
//     } else {
//         y    // ERROR: &'b str no es &'a str
//     }
// }

// Lifetime solo en x, pero el cuerpo puede devolver y:
// fn longest_partial<'a>(x: &'a str, y: &str) -> &'a str {
//     if x.len() > y.len() {
//         x    // OK
//     } else {
//         y    // ERROR: y tiene un lifetime anonimo, no 'a
//     }
// }
// Para devolver CUALQUIERA de los dos, ambos necesitan el mismo 'a.
```

## Anotaciones en bloques impl

Cuando un struct tiene un lifetime, el bloque `impl` debe
declararlo. Los metodos heredan ese lifetime:

```rust
struct Excerpt<'a> {
    text: &'a str,
}

impl<'a> Excerpt<'a> {
    // Retorno con lifetime implicito (elision liga a &self):
    fn first_sentence(&self) -> &str {
        let bytes = self.text.as_bytes();
        for (i, &item) in bytes.iter().enumerate() {
            if item == b'.' {
                return &self.text[..=i];
            }
        }
        self.text
    }

    // Retorno con lifetime explicito 'a (ligado al dato original):
    fn first_word(&self) -> &'a str {
        let bytes = self.text.as_bytes();
        for (i, &item) in bytes.iter().enumerate() {
            if item == b' ' {
                return &self.text[..i];
            }
        }
        self.text
    }

    // Metodo con lifetime adicional para otro parametro:
    fn announce(&self, announcement: &str) -> &str {
        println!("Attention: {}", announcement);
        self.text
    }
}
```

```rust
// Struct con multiples lifetimes:
struct TwoRefs<'a, 'b> {
    first: &'a str,
    second: &'b str,
}

impl<'a, 'b> TwoRefs<'a, 'b> {
    fn get_first(&self) -> &'a str { self.first }
    fn get_second(&self) -> &'b str { self.second }

    // Devolver cualquiera requiere un bound:
    fn get_longer(&self) -> &'a str
    where
        'b: 'a,
    {
        if self.first.len() >= self.second.len() {
            self.first
        } else {
            self.second // OK porque 'b: 'a
        }
    }
}
```

## Errores comunes

```rust
// ERROR 1: Usar el mismo lifetime cuando deberian ser distintos.
// (Ya visto en la seccion de multiples lifetimes.)
// Solucion: solo compartir lifetime entre parametros que realmente
// afectan el lifetime del retorno.
```

```rust
// ERROR 2: Intentar devolver referencia a un dato local.
// Ninguna anotacion arregla esto:

// fn create_string<'a>() -> &'a str {
//     let s = String::from("hello");
//     s.as_str()
//     // ERROR: `s` does not live long enough
// }

// Solucion: devolver el dato owned:
fn create_string() -> String {
    String::from("hello") // mueve el ownership al caller
}
```

```rust
// ERROR 3: Anotar 'a en todo "por si acaso".
// fn do_stuff<'a>(x: &'a str, y: &'a str, z: &'a str) -> &'a str { ... }
//
// Si el retorno solo depende de x, no pongas 'a en y ni z.
// Cada lifetime compartido es una RESTRICCION para el caller.
// Mejor:
// fn do_stuff<'a>(x: &'a str, y: &str, z: &str) -> &'a str { ... }
```

```rust
// ERROR 4: Confundir lifetimes con ownership.
// Los lifetimes solo aplican a REFERENCIAS (&T, &mut T).
// Los tipos owned (String, Vec<T>) no necesitan anotaciones de lifetime.

fn takes_owned(s: String) -> String {
    s // no hay lifetime que anotar — ownership se mueve
}
```

```rust
// ERROR 5: Pensar que 'a restringe el CUERPO de la funcion.
// Las anotaciones restringen a los CALLERS.
// Dentro de la funcion, el compilador verifica que lo que devuelves
// realmente tiene el lifetime declarado.
// Fuera de la funcion, verifica que los argumentos reales satisfacen 'a.
```

## Lifetime subtyping

Un lifetime mas largo puede usarse donde se espera uno mas corto.
Esto se llama **covarianza** para referencias compartidas:

```rust
// 'static es el lifetime mas largo posible.
// Un &'static str se puede usar donde se espera &'a str para
// cualquier 'a, porque 'static: 'a siempre se cumple.

fn needs_ref<'a>(s: &'a str) -> &'a str { s }

fn main() {
    let s: &'static str = "I am a string literal";
    let result = needs_ref(s);
    // 'static se "acorta" a lo que 'a necesite ser.
    println!("{}", result);
}
```

```rust
// Subtyping con la funcion longest:
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}

fn main() {
    let s1 = String::from("hello");     // lifetime: todo main
    {
        let s2 = String::from("hi");    // lifetime: solo este bloque
        // 'a se resuelve al lifetime mas corto (s2).
        // s1 tiene lifetime largo, que se "acorta" a 'a (covarianza).
        let result = longest(s1.as_str(), s2.as_str());
        println!("{}", result); // OK: dentro del scope de s2
    }
}
```

```rust
// Covarianza vs invarianza:
//
// &'a T      es COVARIANTE en 'a:
//   'long: 'short => &'long T puede usarse como &'short T
//
// &'a mut T  es INVARIANTE en 'a:
//   las reglas son mas estrictas con referencias mutables
//
// Esta distincion rara vez es un problema en codigo normal,
// pero importa en APIs genericas avanzadas.
```

```rust
// Ejemplo practico de subtyping con bounds:
fn pick_first<'a: 'b, 'b>(x: &'a str, _y: &'b str) -> &'b str {
    x
    // x tiene lifetime 'a, que vive al menos tanto como 'b.
    // Podemos devolver x como &'b str porque 'a: 'b.
    // El lifetime se acorta de 'a a 'b.
}
```

---

## Ejercicios

### Ejercicio 1 — Anotar lifetimes basicos

```rust
// Las siguientes funciones no compilan por falta de anotaciones.
// Agregar las anotaciones de lifetime correctas.
//
// 1.
// fn longer(x: &str, y: &str) -> &str {
//     if x.len() > y.len() { x } else { y }
// }
//
// 2.
// fn first(x: &str, y: &str) -> &str {
//     x
// }
//
// 3.
// fn identity(x: &i32) -> &i32 {
//     x
// }
//
// Para cada una:
//   a. Agregar las anotaciones minimas necesarias
//   b. Explicar por que elegiste esos lifetimes
//   c. Compilar y verificar con cargo check
//   d. Escribir un main() que llame a cada funcion
//      con referencias de distintos scopes
```

### Ejercicio 2 — Multiples lifetimes

```rust
// Implementar una funcion que reciba dos slices de &str
// y devuelva el primer elemento del primer slice.
//
// 1. Escribir la firma con un solo lifetime 'a en ambos parametros
//    y verificar que compila.
//
// 2. Reescribir con dos lifetimes 'a y 'b (uno por parametro).
//    El retorno debe ligarse al primer parametro.
//
// 3. Escribir un main() donde:
//    - El primer slice vive en un scope externo
//    - El segundo slice vive en un scope interno
//    - El resultado se usa FUERA del scope interno
//    - Verificar que la version con dos lifetimes compila
//      pero la version con un solo lifetime NO compila en este escenario
```

### Ejercicio 3 — Struct con lifetime y metodos

```rust
// 1. Definir un struct TextAnalyzer<'a> que contenga un campo text: &'a str
//
// 2. Implementar los siguientes metodos:
//    - word_count(&self) -> usize — cuenta palabras separadas por espacios
//    - longest_word(&self) -> &str — devuelve la palabra mas larga
//    - contains(&self, needle: &str) -> bool — busca un substring
//
// 3. Para longest_word, decidir:
//    - El retorno debe tener lifetime 'a (ligado al texto original)?
//    - O el lifetime implicito de &self basta?
//    - Escribir ambas versiones y razonar cual es correcta.
//
// 4. Escribir un main() que demuestre que el resultado de
//    longest_word puede sobrevivir al struct (si tiene lifetime 'a)
//    o no (si tiene el lifetime de &self).
```
