# T03 — Booleanos y char

## El tipo bool

Rust tiene un tipo booleano llamado `bool`. Solo acepta dos
valores: `true` y `false`. Ocupa **1 byte** en memoria (no 1 bit),
porque la unidad minima direccionable en las arquitecturas modernas
es el byte.

```rust
fn main() {
    let active: bool = true;
    let deleted = false; // el compilador infiere bool

    println!("active: {active}");   // active: true
    println!("deleted: {deleted}"); // deleted: false
    println!("{}", std::mem::size_of::<bool>()); // 1 (byte)
}
```

## Operadores logicos y cortocircuito

Los operadores logicos son `&&` (AND), `||` (OR) y `!` (NOT).
Solo aceptan operandos `bool` (a diferencia de C, donde
cualquier entero sirve):

```rust
fn main() {
    let a = true;
    let b = false;

    println!("{}", a && b);  // false  — AND: ambos deben ser true
    println!("{}", a || b);  // true   — OR: al menos uno true
    println!("{}", !a);      // false  — NOT: invierte el valor

    // Precedencia: ! > && > ||
    let x = true;
    let y = false;
    let z = true;
    println!("{}", x && y || z);   // true  — se evalua como (x && y) || z
    println!("{}", !x || y);       // false — se evalua como (!x) || y
}
```

```rust
// Short-circuit evaluation:
// Si el resultado ya se puede determinar con el primer operando,
// el segundo NO se evalua.

fn side_effect() -> bool {
    println!("side_effect() was called");
    true
}

fn main() {
    // false && ... → ya es false, side_effect() NO se ejecuta:
    let _r = false && side_effect();
    // (no imprime nada)

    // true || ... → ya es true, side_effect() NO se ejecuta:
    let _r = true || side_effect();

    // Uso practico — verificar antes de acceder:
    let items: Vec<i32> = vec![10, 20, 30];
    if !items.is_empty() && items[0] > 5 {
        println!("First item is greater than 5");
    }
    // Si items esta vacio, items[0] nunca se evalua → no hay panic.
}
```

## bool en condiciones — sin conversion implicita

En C, cualquier entero distinto de cero se trata como verdadero.
Rust **no** permite esto. Las condiciones requieren `bool` explicito:

```rust
fn main() {
    let number = 42;

    // ESTO NO COMPILA en Rust:
    // if number { println!("nonzero"); }
    // Error: expected `bool`, found `{integer}`

    // En C esto seria legal: if (number) { ... }
    // En Rust debes ser explicito:
    if number != 0 {
        println!("nonzero");
    }

    // Tampoco hay conversion implicita en asignaciones:
    // let flag: bool = 1;   // Error
    // let n: i32 = true;    // Error
    // Esto elimina bugs de C como if (x = 0) (asignacion en vez de comparacion).
}
```

## bool como entero — conversion explicita con as

```rust
fn main() {
    println!("{}", true as u8);   // 1
    println!("{}", false as u8);  // 0
    println!("{}", true as i32);  // 1
    println!("{}", false as i32); // 0

    // Util para contar cuantos cumplen una condicion:
    let values = [3, 7, 2, 9, 1, 8];
    let count: u32 = values.iter().map(|&v| (v > 5) as u32).sum();
    println!("Values greater than 5: {count}"); // 3

    // La conversion inversa (entero → bool) NO se permite con as:
    // let b: bool = 1u8 as bool;  // Error: cannot cast `u8` as `bool`
    // Solucion: usar comparacion:
    let b: bool = 1u8 != 0;
    println!("{b}"); // true
}
```

## El tipo char

En Rust, `char` representa un **Unicode Scalar Value**. Ocupa
**4 bytes** (32 bits) en memoria, sin importar que caracter
represente. Esto es radicalmente distinto al `char` de C, que
ocupa 1 byte y solo cubre ASCII.

```rust
fn main() {
    let letter: char = 'a';
    let emoji: char = '\u{1F600}'; // U+1F600 = grinning face
    let kanji: char = '\u{4E16}';  // U+4E16 = "world" in Japanese

    println!("{letter} {emoji} {kanji}");
    println!("{}", std::mem::size_of::<char>()); // 4
}
```

```rust
// Que es un Unicode Scalar Value:
//
// Unicode asigna un numero (code point) a cada caracter.
// Rango total: U+0000 a U+10FFFF (1,114,112 code points).
//
// Un "scalar value" es un code point que NO es un surrogate.
// Surrogates: U+D800 a U+DFFF (reservados para UTF-16).
//
// Rango valido de char en Rust:
//   U+0000  a U+D7FF
//   U+E000  a U+10FFFF
//   Total: 1,112,064 scalar values posibles.
//
// 4 bytes son suficientes (U+10FFFF cabe en 21 bits).
```

## Secuencias de escape

Rust soporta las siguientes secuencias de escape en literales
`char` y en cadenas (`str`/`String`):

```rust
fn main() {
    // \n  — nueva linea (line feed, U+000A)
    // \r  — retorno de carro (carriage return, U+000D)
    // \t  — tabulador horizontal (U+0009)
    // \\  — backslash literal
    // \'  — comilla simple (necesario en literales char)
    // \"  — comilla doble (necesario en strings)
    // \0  — null character (U+0000)
    // \xNN — byte hex (rango 00-7F, solo ASCII)
    // \u{NNNN} — code point Unicode (1 a 6 digitos hex)

    let newline = '\n';
    let backslash = '\\';
    let quote = '\'';
    let null_char = '\0';
    let bell: char = '\x07';
    let smiley = '\u{1F600}';
    let sigma = '\u{03A3}';

    println!("path: C:\\Users\\name");
    println!("She said \"hello\"");
    println!("{smiley} {sigma}");
    println!("null code: {}", null_char as u32); // 0
}
```

```rust
// \xNN en char vs strings:
//   En char y string normal: NN <= 7F (ASCII solamente).
//   En byte string (b"..."): NN puede ser 00-FF.
//   '\x80' no compila — fuera del rango ASCII para char.
```

## char vs u8

`char` y `u8` son tipos fundamentalmente distintos. `char` es
un Unicode scalar value de 4 bytes. `u8` es un byte sin signo
(0-255). No son intercambiables:

```rust
fn main() {
    // char: 4 bytes, Unicode scalar value
    let c: char = 'A';
    println!("size of char: {}", std::mem::size_of::<char>()); // 4

    // u8: 1 byte, valor 0-255
    let b: u8 = 65;
    println!("size of u8: {}", std::mem::size_of::<u8>());     // 1

    // ASCII (0-127) cabe en ambos:
    let c: char = 'A';   // U+0041
    let b: u8 = b'A';    // 65 (byte literal con b'...')
    println!("{c} = {b}"); // A = 65

    // Pero char va mucho mas alla:
    let emoji: char = '\u{1F600}'; // code point 128512 — no cabe en u8
}
```

```rust
// char vs u8 en el contexto de strings:
//
// String / &str → secuencia de bytes UTF-8 (ancho variable: 1-4 bytes por caracter).
// char → siempre 4 bytes (ancho fijo).
//
// Cuando iteras un String:
//   .bytes() → iterador de u8 (bytes crudos UTF-8)
//   .chars() → iterador de char (Unicode scalar values)
//
// Ejemplo con "cafe" (con e acentuada, U+00E9):
//   "caf\u{00E9}".bytes().count() → 5 (la e ocupa 2 bytes en UTF-8)
//   "caf\u{00E9}".chars().count() → 4
//
// char NO es lo mismo que "un caracter visible" en un String.
```

## Conceptos Unicode

Para entender `char` en Rust hay que distinguir tres niveles:

```rust
// Nivel 1: Code point — un numero asignado por Unicode.
// Nivel 2: Scalar value — code point que no es surrogate. Esto es char en Rust.
// Nivel 3: Grapheme cluster — lo que un humano percibe como "una letra".
//          Puede ser uno o mas scalar values.
//
// Ejemplo: "e" puede ser:
//   U+00E9            (un solo scalar value, precompuesta)
//   U+0065 + U+0301   (dos scalar values: 'e' + combining accent)
// Ambos se ven igual pero tienen distinta representacion.
//
// Rust char = scalar value (nivel 2).
// Rust NO tiene tipo nativo para grapheme clusters.

fn main() {
    let s1 = String::from("\u{00E9}");         // 1 char, 2 bytes UTF-8
    let s2 = String::from("\u{0065}\u{0301}"); // 2 chars, 3 bytes UTF-8

    println!("s1 chars: {}", s1.chars().count()); // 1
    println!("s2 chars: {}", s2.chars().count()); // 2
    // Ambos se ven como "e" pero tienen distinta longitud.
}
```

## Por que char::len_utf8() varia

Aunque `char` siempre ocupa 4 bytes en memoria, en UTF-8
(el encoding de `String`/`&str`) puede ocupar de 1 a 4 bytes:

```rust
fn main() {
    // UTF-8 usa ancho variable:
    // U+0000   a U+007F   → 1 byte  (ASCII)
    // U+0080   a U+07FF   → 2 bytes
    // U+0800   a U+FFFF   → 3 bytes
    // U+10000  a U+10FFFF → 4 bytes

    println!("{}", 'A'.len_utf8());          // 1
    println!("{}", '\u{00F1}'.len_utf8());   // 2 — n
    println!("{}", '\u{4E16}'.len_utf8());   // 3 — CJK character
    println!("{}", '\u{1F600}'.len_utf8());  // 4 — emoji

    // Por eso String::len() devuelve bytes, no caracteres:
    let s = "Hola";
    println!("{}", s.len());           // 5 bytes (n ocupa 2)
    println!("{}", s.chars().count()); // 4 caracteres
}
```

```rust
// Comparacion con C:
//
// C:    char → 1 byte, solo ASCII. Para Unicode usa wchar_t
//       (4 bytes en Linux, 2 bytes en Windows — inconsistente).
//
// Rust: char → 4 bytes, siempre un Unicode scalar value valido.
//       u8   → 1 byte, equivalente al char de C.
//       String/&str → UTF-8 (variable width).
```

## Metodos de char

```rust
fn main() {
    // Clasificacion:
    println!("{}", 'a'.is_alphabetic());    // true
    println!("{}", '3'.is_alphabetic());    // false
    println!("{}", '7'.is_numeric());       // true
    println!("{}", 'a'.is_alphanumeric());  // true
    println!("{}", '!'.is_alphanumeric());  // false
    println!("{}", ' '.is_whitespace());    // true
    println!("{}", '\n'.is_whitespace());   // true

    // Mayusculas/minusculas:
    println!("{}", 'A'.is_uppercase()); // true
    println!("{}", 'a'.is_lowercase()); // true

    // to_uppercase()/to_lowercase() devuelven iteradores (no un char),
    // porque en Unicode una conversion puede producir multiples chars:
    for c in 'a'.to_uppercase() { print!("{c}"); } // A
    println!();
    for c in '\u{00DF}'.to_uppercase() { print!("{c}"); } // SS (sharp s → dos chars)
    println!();
}
```

```rust
fn main() {
    // Deteccion y clasificacion ASCII:
    println!("{}", 'A'.is_ascii());            // true
    println!("{}", '\u{00E9}'.is_ascii());     // false
    println!("{}", 'A'.is_ascii_uppercase());  // true
    println!("{}", '5'.is_ascii_digit());      // true
    println!("{}", 'f'.is_ascii_hexdigit());   // true

    // to_digit(radix) — char a valor numerico. Devuelve Option<u32>:
    println!("{:?}", '7'.to_digit(10));  // Some(7)
    println!("{:?}", 'a'.to_digit(16));  // Some(10)
    println!("{:?}", 'g'.to_digit(16));  // None
    println!("{:?}", '8'.to_digit(8));   // None — 8 no es octal
    println!("{:?}", '2'.to_digit(2));   // None — 2 no es binario

    // len_utf8() — bytes que ocupa en UTF-8:
    println!("{}", 'A'.len_utf8());         // 1
    println!("{}", '\u{1F600}'.len_utf8()); // 4
}
```

## Conversiones entre char, u32 y u8

```rust
fn main() {
    // char → u32: siempre seguro (as)
    let c = 'A';
    let code: u32 = c as u32;
    println!("'A' = U+{:04X} = {}", code, code); // 'A' = U+0041 = 65

    // u32 → char: NO siempre seguro.
    // No todos los u32 son scalar values validos.
    // char::from_u32() devuelve Option<char>:
    println!("{:?}", char::from_u32(65));      // Some('A')
    println!("{:?}", char::from_u32(0x1F600)); // Some('\u{1F600}')
    println!("{:?}", char::from_u32(0xD800));  // None — surrogate
    println!("{:?}", char::from_u32(0x110000)); // None — fuera de rango

    // u8 → char: siempre seguro.
    // Todo u8 (0-255) es un code point Unicode valido.
    let c: char = char::from(65u8);
    println!("{c}"); // A
    let c: char = 233u8.into(); // U+00E9 = e
    println!("{c}"); // e
}
```

```rust
fn main() {
    // char → u8: cuidado, trunca silenciosamente si > 255:
    let c = 'A';
    let byte: u8 = c as u8;
    println!("{byte}"); // 65 — OK

    let c = '\u{1F600}'; // code point 128512
    let byte: u8 = c as u8;
    println!("{byte}"); // 0 — truncado silenciosamente (128512 % 256 = 0)

    // Conversion segura: verificar primero:
    let c = '\u{1F600}';
    if c.is_ascii() {
        println!("safe: {}", c as u8);
    } else {
        println!("Not ASCII, code point: {}", c as u32);
    }
    // Not ASCII, code point: 128512
}
```

```rust
// Tabla de conversiones char / u8 / u32 / bool:
//
// Origen → Destino    Metodo               Seguridad
// ──────────────────────────────────────────────────────
// char   → u32        c as u32             siempre seguro
// char   → u8         c as u8              trunca si > 255
// u32    → char       char::from_u32(n)    devuelve Option<char>
// u8     → char       char::from(b)        siempre seguro
// bool   → u8/i32     b as u8              siempre seguro (0 o 1)
// u8     → bool       no permitido con as  usar n != 0
```

---

## Ejercicios

### Ejercicio 1 — Clasificador de caracteres

```rust
// Escribir una funcion classify_char(c: char) que imprima
// todas las propiedades del caracter recibido:
//   - Si es alfabetico, numerico, alfanumerico
//   - Si es mayuscula o minuscula
//   - Si es ASCII, si es whitespace
//   - Su code point en hexadecimal (U+XXXX)
//   - Cuantos bytes ocupa en UTF-8
//
// Probar con: 'A', 'z', '7', ' ', '\n', '\u{00F1}', '\u{1F600}'
//
// Ejemplo de salida para 'A':
//   char: 'A'
//   alphabetic: true, numeric: false, alphanumeric: true
//   uppercase: true, lowercase: false
//   ascii: true, whitespace: false
//   code point: U+0041, utf-8 length: 1 byte(s)
```

### Ejercicio 2 — Conversion hexadecimal manual

```rust
// Escribir hex_char_to_value(c: char) -> Option<u32>
// que convierta un caracter hex a su valor numerico.
// Aceptar '0'-'9', 'a'-'f', 'A'-'F'. None para invalidos.
//
// Implementar DOS versiones:
//   1. Usando char::to_digit(16)
//   2. Usando match y aritmetica con as u32
//
// Verificar: '0'→Some(0), '9'→Some(9), 'a'→Some(10),
//            'F'→Some(15), 'g'→None, 'Z'→None
```

### Ejercicio 3 — Diferencia entre chars y bytes

```rust
// Dado el string "Munoz" (con n):
//   1. Imprimir cuantos bytes tiene (.len())
//   2. Imprimir cuantos chars tiene (.chars().count())
//   3. Iterar con .bytes() e imprimir cada byte en decimal y hex
//   4. Iterar con .chars() e imprimir cada char, su code point y len_utf8()
//
// Repetir con "Hello" (solo ASCII) y "\u{1F600}\u{1F601}\u{1F602}" (tres emojis).
// Explicar en comentarios por que "Munoz" tiene mas bytes que chars.
```

### Ejercicio 4 — Contador booleano

```rust
// Dado un Vec<char>, contar cuantos caracteres cumplen cada condicion
// usando la tecnica (condicion) as u32 (sin if/else):
//   - Son alfabeticos, numericos, whitespace, ASCII
//
// Probar con:
//   vec!['H', 'e', 'l', 'l', 'o', ' ', '4', '2', '\n', '\u{00F1}']
//
// Resultado esperado:
//   alphabetic: 6, numeric: 2, whitespace: 2, ascii: 9
```
