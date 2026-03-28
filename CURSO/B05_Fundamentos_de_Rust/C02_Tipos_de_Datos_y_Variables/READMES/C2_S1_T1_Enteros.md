# T01 — Enteros

## Tipos enteros en Rust

Rust tiene enteros de tamano fijo, tanto con signo como sin signo,
y dos tipos cuyo tamano depende de la plataforma. Cada tipo tiene
un tamano en bits, un rango definido y un comportamiento estricto
ante desbordamiento.

```rust
// Enteros con signo (signed): i8, i16, i32, i64, i128
// Enteros sin signo (unsigned): u8, u16, u32, u64, u128
// Dependientes de plataforma: isize, usize

let x: i32 = -42;       // entero con signo de 32 bits
let y: u8 = 255;        // entero sin signo de 8 bits
let z: usize = 100;     // sin signo, tamano de puntero

// El tipo por defecto cuando no se anota es i32:
let a = 42;             // tipo inferido: i32
let b = -10;            // tipo inferido: i32
```

## Tipos enteros con signo

Los tipos con signo usan complemento a dos (two's complement)
para representar numeros negativos. El bit mas significativo
indica el signo: 0 = positivo, 1 = negativo.

```rust
// Rangos de enteros con signo:
// Tipo   Bits   Minimo                        Maximo
// i8       8    -128                          127
// i16     16    -32_768                       32_767
// i32     32    -2_147_483_648                2_147_483_647
// i64     64    -9_223_372_036_854_775_808    9_223_372_036_854_775_807
// i128   128    -(2^127)                      2^127 - 1

// Formula general: iN almacena desde -(2^(N-1)) hasta 2^(N-1) - 1
// El rango negativo tiene un valor mas que el positivo.
```

```rust
// Acceder a los limites con constantes asociadas:
let min_i8: i8 = i8::MIN;     // -128
let max_i8: i8 = i8::MAX;     // 127
let min_i32: i32 = i32::MIN;  // -2_147_483_648
let max_i32: i32 = i32::MAX;  // 2_147_483_647
let bits: u32 = i32::BITS;    // 32
```

```rust
// Complemento a dos — como funciona internamente (para i8):
//   0000_0000 =   0
//   0000_0001 =   1
//   0111_1111 = 127    (maximo positivo)
//   1000_0000 = -128   (minimo negativo)
//   1111_1111 =  -1
//   1111_1110 =  -2
//
// Para negar un numero: invertir todos los bits (NOT) y sumar 1.
// Ejemplo: representar -5 en i8:
//   5 en binario:      0000_0101
//   Invertir bits:     1111_1010
//   Sumar 1:           1111_1011 = -5
//
// Ventaja: la suma funciona igual para positivos y negativos.
```

## Tipos enteros sin signo

Los tipos sin signo usan todos los bits para la magnitud.
No pueden almacenar valores negativos.

```rust
// Rangos de enteros sin signo:
// Tipo   Bits   Minimo   Maximo
// u8       8    0        255
// u16     16    0        65_535
// u32     32    0        4_294_967_295
// u64     64    0        18_446_744_073_709_551_615
// u128   128    0        2^128 - 1

// Formula general: uN almacena desde 0 hasta 2^N - 1
```

```rust
// Constantes asociadas:
let min_u8: u8 = u8::MIN;     // 0
let max_u8: u8 = u8::MAX;     // 255
let max_u64: u64 = u64::MAX;  // 18_446_744_073_709_551_615
```

```rust
// Representacion binaria (para u8):
//   0000_0000 =   0
//   0111_1111 = 127
//   1000_0000 = 128    (en i8 esto seria -128)
//   1111_1111 = 255    (en i8 esto seria -1)
//
// u8 es particularmente comun:
// - Bytes crudos (datos binarios, buffers de red)
// - Valores ASCII (un char ASCII cabe en u8)
// - Componentes de color RGB (0-255)
```

```rust
// Error comun: restar cuando el resultado seria negativo:
let a: u8 = 5;
let b: u8 = 10;
let c = a - b;
// En debug: panic con 'attempt to subtract with overflow'
// En release: wrapping → 251 (256 - 5)
```

## isize y usize

`isize` y `usize` tienen el tamano del puntero de la plataforma.
En un sistema de 64 bits son de 64 bits; en uno de 32 son de 32.

```rust
// En 64 bits: usize = u64, isize = i64
// En 32 bits: usize = u32, isize = i32

// usize es EL tipo para indices y tamanos:
let v = vec![10, 20, 30, 40, 50];
let len: usize = v.len();       // Vec::len() retorna usize
let element = v[2];              // el indice es usize

// Por que usize? Un indice debe poder direccionar toda la memoria.
// En 32 bits 4 GB max → u32 basta. En 64 bits → se necesita u64.
// usize se adapta automaticamente.
```

```rust
// No se puede usar i32 directamente como indice:
let v = vec![1, 2, 3];
let i: i32 = 1;
// let x = v[i];    // error: cannot be indexed by `i32`
let x = v[i as usize];  // correcto: convertir a usize

// isize se usa para offsets (desplazamientos que pueden ser negativos):
// unsafe { let p2 = ptr.offset(-1); }  // offset() usa isize

// Verificar el tamano en la plataforma actual:
println!("usize: {} bits", usize::BITS);  // 64 en x86_64, 32 en armv7
```

## Literales enteros

Rust permite escribir literales en distintas bases y con
separadores visuales para mejorar la legibilidad.

```rust
// Decimal (base 10):
let decimal = 1_000_000;       // _ como separador visual (ignorado)
let small = 42;
let negative = -273;

// Hexadecimal (base 16) — prefijo 0x:
let hex = 0xFF;                // 255
let hex_color = 0x00_FF_00;    // verde en RGB

// Octal (base 8) — prefijo 0o:
let octal = 0o777;             // 511 — permisos Unix
let octal2 = 0o644;            // 420

// Binario (base 2) — prefijo 0b:
let binary = 0b1111_0000;     // 240
let flags = 0b0000_0101;      // bits 0 y 2 activados
```

```rust
// Literal de byte — b'caracter':
let byte_a: u8 = b'A';        // 65 (codigo ASCII de 'A')
let byte_zero: u8 = b'0';     // 48 (codigo ASCII de '0')
let byte_nl: u8 = b'\n';      // 10 (newline)

// b'...' produce un u8. Solo acepta caracteres ASCII.
// Diferencia: 'A' es char (4 bytes, Unicode), b'A' es u8 (1 byte, ASCII).
```

```rust
// Sufijo de tipo — especificar el tipo en el literal:
let a = 42u8;                  // u8
let b = 42_u8;                 // u8 — el _ antes del sufijo es valido
let c = 100_i64;               // i64
let d = 0xFF_u16;              // u16
let e = 1_000_000_usize;      // usize

// Sin sufijo, se infiere del contexto o es i32 por defecto:
let f = 42;                    // i32 (default)
let g: u64 = 42;              // u64 por la anotacion
```

## Tipo por defecto

Cuando no hay anotacion ni contexto, el literal entero es `i32`.

```rust
let x = 42;           // i32
let y = -10;          // i32
let z = 0xFF;         // i32 (no u8, a pesar de ser hex)

// El contexto cambia la inferencia:
fn takes_u16(n: u16) {}
takes_u16(42);                 // 42 se interpreta como u16

let d: u8 = 10;
let e = d + 5;                 // 5 se interpreta como u8, e es u8

// Una vez inferido, el tipo no cambia:
let x = 42;                   // i32
// let y: u8 = x;             // error: expected `u8`, found `i32`
```

## Conversion entre tipos

Rust no hace conversiones implicitas entre tipos enteros.
Toda conversion debe ser explicita. Hay tres mecanismos.

```rust
// as — casting explicito. Siempre compila, nunca panic.
// El programador es responsable de la semantica.

// Widening (tipo pequeno → grande): seguro, preserva valor.
let a: u8 = 200;
let b: u32 = a as u32;        // 200
let c: i8 = -5;               // 0b1111_1011
let d: i16 = c as i16;        // -5 (sign extension: 0b1111_1111_1111_1011)

// Narrowing (tipo grande → pequeno): trunca bits superiores.
let e: u16 = 256;
let f: u8 = e as u8;          // 0 (256 mod 256 = 0)
let g: i32 = 300;
let h: u8 = g as u8;          // 44 (300 mod 256 = 44)

// Signed ↔ unsigned (mismo tamano): reinterpreta bits.
let i: i8 = -1;
let j: u8 = i as u8;          // 255 (0b1111_1111 reinterpretado)
let k: u8 = 200;
let l: i8 = k as i8;          // -56 (0b1100_1000 como i8)

// Signed → unsigned GRANDE: sign-extends primero, luego reinterpreta.
let m: i8 = -1;
let n: u32 = m as u32;        // 4_294_967_295 (i8→i32 sign ext → u32 reinterpret)
```

```rust
// Gotchas con as:
// 1. Truncacion silenciosa (sin error ni warning):
let big: u32 = 100_000;
let small: u8 = big as u8;    // 160 — sin aviso

// 2. Conversion de negativo a unsigned:
let neg: i32 = -1;
let pos: u32 = neg as u32;    // 4_294_967_295

// 3. bool → entero funciona, pero entero → bool no:
let t = true as u8;            // 1
// let b = 1u8 as bool;        // error. Usar: let b = x != 0;
```

```rust
// From/Into — conversiones seguras (infalibles).
// Solo existen cuando la conversion NUNCA pierde datos.

let x: u8 = 42;
let y: u16 = u16::from(x);    // u8 → u16: siempre seguro
let z: u32 = x.into();        // equivalente a u32::from(x)

// From NO existe para conversiones con posible perdida:
// let y: u8 = u8::from(300u32);  // error: no existe From<u32> for u8
```

```rust
// TryFrom/TryInto — conversiones falibles. Retornan Result.

use std::convert::TryFrom;

let big: i32 = 300;
let attempt = u8::try_from(big);   // Err — 300 no cabe en u8

let small: i32 = 42;
let attempt = u8::try_from(small); // Ok(42)

// Patron con match:
match u8::try_from(value) {
    Ok(n) => println!("Fits: {}", n),
    Err(e) => println!("Does not fit: {}", e),
}

// Resumen:
// as          → rapido, puede truncar. Usar con cuidado.
// From/Into   → seguro, infalible. Opcion preferida.
// TryFrom     → seguro, falible. Cuando puede no caber.
```

## Overflow — desbordamiento

Cuando una operacion produce un resultado que no cabe en el tipo.

```rust
// En debug (cargo build): panic en tiempo de ejecucion.
let x: u8 = 255;
let y = x + 1;    // panic: 'attempt to add with overflow'

let a: i8 = 127;
let b = a + 1;    // panic: 'attempt to add with overflow'

// En release (cargo build --release): wrapping (aritmetica modular).
// u8: 255 + 1 → 0,   0 - 1 → 255
// i8: 127 + 1 → -128
```

```rust
// Comparacion con C:
// - C signed overflow: UNDEFINED BEHAVIOR. El compilador puede asumir
//   que nunca ocurre y optimizar. Bugs extremadamente dificiles.
//   Ejemplo: if (x + 1 > x) puede ser eliminado por el compilador.
// - C unsigned overflow: wrapping definido.
// - Rust: AMBOS tienen comportamiento definido (panic o wrapping).
//   No hay undefined behavior.

// Trade-off:
// - Debug: detectar bugs temprano. Costo del check aceptable.
// - Release: ~2-5% de costo por verificacion. Se omite por rendimiento.
//   Si se necesita checking en release, usar checked_add(), etc.
```

## Metodos para controlar overflow

Metodos explicitos que no dependen del modo de compilacion.

```rust
// wrapping_* — siempre envuelve (aritmetica modular):
let x: u8 = 250;
let y = x.wrapping_add(10);       // 4  (260 mod 256)
let z = 0u8.wrapping_sub(1);      // 255
// Util para: hashes, aritmetica modular, contadores circulares.

// checked_* — retorna Option: Some(resultado) o None:
let a = 250u8.checked_add(5);     // Some(255)
let b = 250u8.checked_add(10);    // None
let c = 200u8.checked_add(100).unwrap_or(u8::MAX);  // 255

// saturating_* — clamp al limite (MAX o MIN):
let d = 250u8.saturating_add(10);  // 255 (se queda en MAX)
let e = 5u8.saturating_sub(10);    // 0 (se queda en MIN)
// Util para: colores (clamp 0-255), contadores sin wrap.

// overflowing_* — retorna tupla (resultado_wrapping, bool):
let (result, did_overflow) = 250u8.overflowing_add(10);
// result = 4, did_overflow = true
```

```rust
// Resumen de metodos de overflow:
//
// Metodo            Retorno       Cuando hay overflow
// ─────────────────────────────────────────────────────
// wrapping_add      T             Wrapping (modular)
// checked_add       Option<T>     None
// saturating_add    T             Clamp al limite
// overflowing_add   (T, bool)     (resultado wrapping, true)
//
// Existen para: add, sub, mul, div, rem, neg, shl, shr, abs, pow.
```

## Operaciones aritmeticas

```rust
let a: i32 = 17;
let b: i32 = 5;

let sum = a + b;       // 22
let diff = a - b;      // 12
let prod = a * b;      // 85
let quot = a / b;      // 3 (division entera, trunca hacia cero)
let rem = a % b;       // 2 (resto)

// Division trunca hacia cero (no hacia -infinito):
let c = -17 / 5;      // -3 (no -4)
let d = -17 % 5;      // -2 (signo del resto sigue al dividendo)

// Division por cero: panic en TODOS los modos.
// let e = 10 / 0;    // panic: attempt to divide by zero

// i32::MIN / -1: panic (resultado no cabe en i32).
// Porque -(-2_147_483_648) = 2_147_483_648 > i32::MAX
```

```rust
// Operadores de asignacion compuesta:
let mut x: i32 = 10;
x += 5;   // 15
x -= 3;   // 12
x *= 2;   // 24
x /= 4;   // 6
x %= 4;   // 2

// Negacion unaria (-): solo para tipos con signo.
let a: i32 = 42;
let b = -a;   // -42
// let c = -42u32;  // error: cannot apply `-` to type `u32`
```

## Operaciones bitwise

```rust
// AND, OR, XOR, NOT:
let a: u8 = 0b1100_1010;
let b: u8 = 0b1010_1100;

let and = a & b;    // 0b1000_1000 = 136  — extraer bits con mascara
let or  = a | b;    // 0b1110_1110 = 238  — activar bits
let xor = a ^ b;    // 0b0110_0110 = 102  — toggle de bits
let not = !a;       // 0b0011_0101 = 53   — invertir (en Rust es !, no ~)
```

```rust
// Shift left (<<): desplazar bits a la izquierda (multiplica por 2^n).
let x: u32 = 5;
let y = x << 3;    // 40 (5 * 2^3)

// Shift right (>>): desplazar a la derecha (divide por 2^n).
let m: u32 = 40;
let n = m >> 3;    // 5 (40 / 2^3)

// >> en signed es aritmetico (preserva signo):
let a: i8 = -128;
let b = a >> 1;     // -64 (el bit de signo se replica)

// >> en unsigned es logico (rellena con ceros):
let c: u8 = 128;
let d = c >> 1;     // 64

// Shift >= al numero de bits: panic en debug.
// let e = 1u8 << 8;  // panic
```

```rust
// Operadores de asignacion bitwise:
let mut flags: u8 = 0b0000_0000;
flags |= 0b0000_0001;   // activar bit 0    → 0b0000_0001
flags |= 0b0000_0100;   // activar bit 2    → 0b0000_0101
flags &= !0b0000_0001;  // desactivar bit 0 → 0b0000_0100
flags ^= 0b0000_0100;   // toggle bit 2     → 0b0000_0000
```

## Operaciones de comparacion

```rust
let a: i32 = 10;
let b: i32 = 20;

a == b;   // false      a != b;  // true
a < b;    // true       a <= b;  // true
a > b;    // false      a >= b;  // false

// No se pueden comparar tipos distintos directamente:
let x: i32 = 42;
let y: i64 = 42;
// let z = x == y;       // error: mismatched types
let z = (x as i64) == y;  // true
```

## Metodos utiles

```rust
// Valor absoluto (solo signed):
let a = (-42i32).abs();                // 42
// i32::MIN.abs() → panic. Alternativas:
let b = i32::MIN.checked_abs();        // None
let c = i32::MIN.saturating_abs();     // i32::MAX

// Potencia:
let d = 2u32.pow(10);                  // 1024
let e = 10u32.checked_pow(10);         // None (10^10 > u32::MAX)

// Contar bits:
let x: u32 = 0b1010_1100;
x.count_ones();       // 4
x.count_zeros();      // 28
x.leading_zeros();    // 24
x.trailing_zeros();   // 2

// min, max, clamp:
10i32.min(20);                 // 10
150i32.clamp(0, 100);          // 100

// Endianness y bytes:
let x: u32 = 0x12_34_56_78;
x.swap_bytes();                        // 0x78_56_34_12
x.to_be_bytes();                       // [0x12, 0x34, 0x56, 0x78]
x.to_le_bytes();                       // [0x78, 0x56, 0x34, 0x12]
u32::from_be_bytes([0x12, 0x34, 0x56, 0x78]);  // 0x12345678
```

---

## Ejercicios

### Ejercicio 1 — Rangos y limites

```rust
// Escribir un programa que imprima MIN, MAX y BITS
// de todos los tipos enteros: i8, i16, i32, i64, i128,
// u8, u16, u32, u64, u128, isize, usize.
//
// Formato:  i8: MIN = -128, MAX = 127, BITS = 8
//
// Verificar:
//   1. i32::MAX.wrapping_add(1) == i32::MIN
//   2. u8::MAX.wrapping_add(1) == 0
//   3. isize::BITS == (std::mem::size_of::<isize>() * 8) as u32
```

### Ejercicio 2 — Literales y bases

```rust
// Crear variables con estos valores usando distintas bases:
//   - 255 como hex, octal y binario
//   - Permiso Unix 755 como octal (verificar que decimal = 493)
//   - Color RGB #FF8800 como u32 hexadecimal
//   - Letra 'Z' como byte literal
//   - Un millon con separadores
//
// Imprimir cada valor con:
//   println!("{} = 0x{:X} = 0b{:b}", value, value, value);
//
// Verificar que 0xFF == 255 == 0o377 == 0b1111_1111
```

### Ejercicio 3 — Overflow controlado

```rust
// Implementar:
//   fn safe_add(a: i32, b: i32) -> i32
//   Retorna a + b, o i32::MAX/MIN si hay overflow. (Usar saturating_add.)
//
//   fn checked_multiply(a: u8, b: u8) -> Option<u8>
//   Retorna Some(a*b) si cabe, None si no. (Usar checked_mul.)
//
// Probar:
//   safe_add(i32::MAX, 1) → i32::MAX
//   safe_add(i32::MIN, -1) → i32::MIN
//   safe_add(100, 200) → 300
//   checked_multiply(10, 25) → Some(250)
//   checked_multiply(100, 3) → None
//   checked_multiply(16, 16) → None (256 no cabe)
```

### Ejercicio 4 — Conversiones seguras

```rust
// Escribir fn convert_three_ways(value: i64) que convierta a u8 de tres formas:
//   1. Con as (mostrar truncacion)
//   2. Con TryFrom (mostrar Ok o Err)
//   3. Saturating: clamp a 0..=255 y luego as u8
//
// Probar con: 42, 256, -1, 1000, 0, 255, i64::MAX
//
// Verificar:
//   as: 42→42, 256→0, -1→255 (truncacion silenciosa)
//   try_from: 42→Ok(42), 256→Err, -1→Err
//   saturating: 256→255, -1→0, 1000→255
```

### Ejercicio 5 — Operaciones bitwise

```rust
// Implementar usando SOLO operaciones bitwise:
//
//   fn is_even(n: u32) -> bool           // Pista: bit 0
//   fn is_power_of_two(n: u32) -> bool   // Pista: n & (n-1), n != 0
//   fn set_bit(value: u32, bit: u32) -> u32
//   fn clear_bit(value: u32, bit: u32) -> u32
//   fn toggle_bit(value: u32, bit: u32) -> u32
//   fn get_bit(value: u32, bit: u32) -> bool
//
// Probar:
//   is_even(4) → true, is_even(7) → false
//   is_power_of_two(8) → true, is_power_of_two(6) → false
//   set_bit(0, 3) → 8, clear_bit(0xFF, 0) → 0xFE
//   toggle_bit(0b1010, 1) → 0b1000, get_bit(0b1010, 3) → true
```
