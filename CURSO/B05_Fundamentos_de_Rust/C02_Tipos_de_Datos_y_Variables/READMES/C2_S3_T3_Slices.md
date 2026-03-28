# T03 — Slices

## Que es un slice

Un slice es una **vista de tamano dinamico** sobre una secuencia
contigua de elementos. No es una estructura de datos independiente:
es una referencia a datos que ya existen en otro lugar. El slice
**no posee** los datos a los que apunta.

```rust
// &[T]     — slice inmutable (solo lectura)
// &mut [T] — slice mutable (lectura y escritura)

// Un slice siempre es una referencia.
// No puedes tener un [T] suelto — es un tipo sin tamano conocido.
// Siempre lo usas detras de & o de un puntero inteligente.
```

```rust
fn main() {
    let numbers = [10, 20, 30, 40, 50];

    // &numbers[1..3] crea un slice que apunta a los elementos 20 y 30.
    // El array sigue siendo el dueno de los datos.
    let slice: &[i32] = &numbers[1..3];

    println!("{:?}", slice);
    // [20, 30]

    // El array original no se modifico ni se copio:
    println!("{:?}", numbers);
    // [10, 20, 30, 40, 50]
}
```

## Crear slices desde arrays

Se usa la sintaxis de rangos para indicar que porcion del array
se quiere ver. El indice inicial es inclusivo y el final es
exclusivo:

```rust
fn main() {
    let arr = [1, 2, 3, 4, 5];

    // [start..end] — desde start hasta end-1 (end es exclusivo):
    let a: &[i32] = &arr[1..3];
    println!("{:?}", a);   // [2, 3]

    // [start..=end] — desde start hasta end (end es inclusivo):
    let b: &[i32] = &arr[1..=3];
    println!("{:?}", b);   // [2, 3, 4]

    // [start..] — desde start hasta el final del array:
    let c: &[i32] = &arr[2..];
    println!("{:?}", c);   // [3, 4, 5]

    // [..end] — desde el inicio hasta end-1:
    let d: &[i32] = &arr[..3];
    println!("{:?}", d);   // [1, 2, 3]

    // [..] — todo el array como slice:
    let e: &[i32] = &arr[..];
    println!("{:?}", e);   // [1, 2, 3, 4, 5]
}
```

```rust
// Indices fuera de rango provocan panic en tiempo de ejecucion:
fn main() {
    let arr = [1, 2, 3];

    // let bad = &arr[0..10];
    // panic: 'range end index 10 out of range for slice of length 3'

    // let also_bad = &arr[2..1];
    // panic: 'slice index starts at 2 but ends at 1'

    // Los rangos se validan en runtime, no en compilacion.
    // Rust garantiza que no accedes memoria invalida,
    // pero lo hace con panic, no con error de compilacion.
}
```

## Fat pointers

Una referencia normal (`&i32`) ocupa una palabra de maquina:
8 bytes en sistemas de 64 bits. Un slice (`&[i32]`) ocupa
**dos palabras**: un puntero al primer elemento + la longitud.
A esto se le llama **fat pointer**:

```rust
use std::mem::size_of;

fn main() {
    // Referencia normal — 1 palabra (8 bytes en 64-bit):
    println!("size_of::<&i32>()       = {}", size_of::<&i32>());
    // 8

    // Slice — 2 palabras (16 bytes en 64-bit):
    println!("size_of::<&[i32]>()     = {}", size_of::<&[i32]>());
    // 16

    // &str tambien es un fat pointer:
    println!("size_of::<&str>()       = {}", size_of::<&str>());
    // 16

    // Referencia a array de tamano fijo — 1 palabra:
    println!("size_of::<&[i32; 5]>()  = {}", size_of::<&[i32; 5]>());
    // 8  — el compilador ya sabe que N = 5, no necesita guardar len
}
```

```rust
// Estructura interna de un fat pointer:
//
// &[i32] internamente es:
//   ptr: *const i32,   // 8 bytes — puntero al primer elemento
//   len: usize,        // 8 bytes — cantidad de elementos
//
// Ejemplo en memoria:
//   let arr = [10, 20, 30, 40, 50];
//   let slice = &arr[1..4];
//
//   arr:   [10] [20] [30] [40] [50]
//   slice: ptr = &arr[1], len = 3  →  ve [20, 30, 40]
//
// Esto permite que slice.len() funcione en O(1).
// A diferencia de arrays [T; N], donde N es parte del tipo
// y se conoce en compilacion, el len de un slice se conoce
// en tiempo de ejecucion (Dynamically Sized Type, DST).
```

## &str — string slices

`&str` es un slice de bytes UTF-8. Los string literals son
`&'static str` (viven en la memoria del programa durante
toda la ejecucion):

```rust
fn main() {
    // Un string literal es &'static str:
    let greeting: &str = "Hello, world!";
    // Internamente es un fat pointer: ptr a los bytes + len = 13.
    println!("{}, len = {}", greeting, greeting.len());
    // Hello, world!, len = 13
}
```

```rust
fn main() {
    // String (heap-allocated) vs &str (slice):
    let owned: String = String::from("Hello");

    // &String se convierte automaticamente a &str (Deref coercion):
    let slice: &str = &owned;

    // Tambien puedes tomar un sub-slice:
    let sub: &str = &owned[0..3];
    println!("{}", sub);   // Hel
}
```

```rust
// &str es el tipo preferido para parametros que leen strings.

// BIEN — acepta &str y &String (por Deref coercion):
fn greet(name: &str) {
    println!("Hello, {}!", name);
}

// MAL — solo acepta &String, innecesariamente restrictivo:
// fn greet(name: &String) { ... }

fn main() {
    let literal = "Alice";           // &str
    let owned = String::from("Bob"); // String

    greet(literal);   // &str → &str, directo
    greet(&owned);    // &String → &str, por Deref coercion
}
```

## Slices desde Vec

`Vec<T>` implementa `Deref<Target = [T]>`, lo que significa
que `&Vec<T>` se convierte automaticamente a `&[T]`:

```rust
fn sum(data: &[i32]) -> i32 {
    data.iter().sum()
}

fn main() {
    let v: Vec<i32> = vec![1, 2, 3, 4, 5];

    // &v se convierte a &[i32] automaticamente:
    println!("sum = {}", sum(&v));         // sum = 15

    // Sub-slices tambien funcionan:
    println!("partial = {}", sum(&v[1..4]));  // partial = 9

    // Funciona con arrays tambien:
    let arr = [10, 20, 30];
    println!("array = {}", sum(&arr));     // array = 60
}
```

```rust
// Por que fn process(data: &[i32]) y no fn process(data: &Vec<i32>)?
//
// 1. &[i32] es mas general: acepta arrays, Vecs y otros slices.
// 2. Es la convencion idiomatica — Clippy advierte si usas &Vec<T>.
// 3. No hay costo: &Vec<T> → &[T] es una conversion gratuita (Deref).
//
// Regla: si tu funcion solo necesita leer una secuencia de T,
// el parametro debe ser &[T], no &Vec<T>.
```

## Slices mutables

`&mut [T]` permite modificar los elementos del slice en su
lugar. Las reglas de borrowing aplican: no puedes tener
`&[T]` y `&mut [T]` sobre datos que se solapan al mismo tiempo:

```rust
fn fill_zeros(data: &mut [i32]) {
    for elem in data.iter_mut() {
        *elem = 0;
    }
}

fn main() {
    let mut arr = [1, 2, 3, 4, 5];

    fill_zeros(&mut arr[1..4]);
    println!("{:?}", arr);
    // [1, 0, 0, 0, 5]
}
```

```rust
fn main() {
    let mut data = [1, 2, 3, 4, 5];

    // No puedes tener &[T] y &mut [T] sobre datos que se solapen:
    // let immutable = &data[0..3];
    // let mutable = &mut data[2..5];
    // ERROR: cannot borrow `data` as mutable because it is
    // also borrowed as immutable

    // Pero si los rangos NO se solapan, puedes usar split_at_mut:
    let (left, right) = data.split_at_mut(3);
    // left  = &mut [1, 2, 3]
    // right = &mut [4, 5]
    left[0] = 10;
    right[0] = 40;
    println!("{:?}", data);
    // [10, 2, 3, 40, 5]
}
```

## Metodos de slice

Los slices tienen una amplia coleccion de metodos. Los mas
usados, agrupados por categoria:

```rust
fn main() {
    let data = [10, 20, 30, 40, 50];
    let slice: &[i32] = &data;

    // --- Informacion basica ---
    println!("len        = {}", slice.len());          // 5
    println!("is_empty   = {}", slice.is_empty());     // false
    println!("first      = {:?}", slice.first());      // Some(10)
    println!("last       = {:?}", slice.last());       // Some(50)

    // get() retorna Option — no hace panic si el indice no existe:
    println!("get(2)     = {:?}", slice.get(2));       // Some(30)
    println!("get(99)    = {:?}", slice.get(99));      // None

    println!("contains 30 = {}", slice.contains(&30)); // true
}
```

```rust
fn main() {
    let data = [1, 2, 3, 4, 5, 6, 7, 8];
    let slice: &[i32] = &data;

    // --- Prefijos, sufijos y particion ---
    println!("starts_with [1,2] = {}", slice.starts_with(&[1, 2])); // true
    println!("ends_with [7,8]   = {}", slice.ends_with(&[7, 8]));   // true

    let (left, right) = slice.split_at(3);
    println!("left  = {:?}", left);    // [1, 2, 3]
    println!("right = {:?}", right);   // [4, 5, 6, 7, 8]
}
```

```rust
fn main() {
    let data = [3, 1, 4, 1, 5, 9, 2, 6];

    // --- Ventanas y bloques ---

    // windows(n) produce sub-slices solapados de tamano n:
    let wins: Vec<&[i32]> = data.windows(3).collect();
    println!("{:?}", wins);
    // [[3, 1, 4], [1, 4, 1], [4, 1, 5], [1, 5, 9], [5, 9, 2], [9, 2, 6]]

    // chunks(n) produce sub-slices NO solapados de tamano n:
    let chs: Vec<&[i32]> = data.chunks(3).collect();
    println!("{:?}", chs);
    // [[3, 1, 4], [1, 5, 9], [2, 6]]
    // El ultimo chunk puede tener menos de n elementos.
}
```

```rust
fn main() {
    let mut data = [5, 3, 8, 1, 9, 2];

    // --- Ordenacion y busqueda ---
    data.sort();
    println!("sorted = {:?}", data);   // [1, 2, 3, 5, 8, 9]

    match data.binary_search(&5) {
        Ok(i) => println!("found 5 at index {}", i),      // index 3
        Err(i) => println!("5 not found, insert at {}", i),
    }

    // --- Transformaciones in-place ---
    data.reverse();
    println!("reversed = {:?}", data);  // [9, 8, 5, 3, 2, 1]

    data.swap(0, 5);
    println!("swapped  = {:?}", data);  // [1, 8, 5, 3, 2, 9]
}
```

```rust
fn main() {
    let data = [1, 0, 2, 0, 3, 0, 4];

    // split() divide por un predicado:
    let parts: Vec<&[i32]> = data.split(|&x| x == 0).collect();
    println!("{:?}", parts);
    // [[1], [2], [3], [4]]

    let sum: i32 = data.iter().sum();
    println!("sum = {}", sum);   // 10
}
```

## Slice patterns

Rust permite hacer pattern matching sobre slices. Util para
procesar listas de argumentos y manejar casos base en recursion:

```rust
fn describe(slice: &[i32]) {
    match slice {
        [] => println!("empty"),
        [single] => println!("one element: {}", single),
        [a, b] => println!("two elements: {} and {}", a, b),
        [first, .., last] => {
            println!("first={}, last={}, len={}",
                     first, last, slice.len());
        }
    }
}

fn main() {
    describe(&[]);              // empty
    describe(&[42]);            // one element: 42
    describe(&[1, 2]);          // two elements: 1 and 2
    describe(&[10, 20, 30, 40]);// first=10, last=40, len=4
}
```

```rust
// Capturar el resto con @ ..
fn process(data: &[i32]) {
    match data {
        [first, rest @ ..] => {
            println!("first = {}, rest = {:?}", first, rest);
            if !rest.is_empty() { process(rest); }
        }
        [] => println!("done"),
    }
}

fn main() {
    process(&[10, 20, 30]);
    // first = 10, rest = [20, 30]
    // first = 20, rest = [30]
    // first = 30, rest = []
    // done
}
```

```rust
// Slice patterns con guardas:
fn classify(data: &[i32]) {
    match data {
        [] => println!("empty"),
        [x] if *x > 0 => println!("single positive: {}", x),
        [x] => println!("single non-positive: {}", x),
        [a, b] if a == b => println!("pair of equals: {}", a),
        _ => println!("other: len={}", data.len()),
    }
}

fn main() {
    classify(&[5]);       // single positive: 5
    classify(&[-3]);      // single non-positive: -3
    classify(&[7, 7]);    // pair of equals: 7
}
```

## El tipo sin tamano [T]

`[T]` (sin `&`) es un tipo **unsized** (DST: Dynamically
Sized Type). No puedes crear una variable de tipo `[T]`
directamente — solo puedes usarlo detras de una referencia
o puntero inteligente:

```rust
// ESTO NO COMPILA:
// let x: [i32] = ???;
// ERROR: the size for values of type `[i32]` cannot be known
// at compilation time

// Siempre se usa detras de un indirection:
// &[T]      — referencia inmutable (fat pointer en stack)
// &mut [T]  — referencia mutable (fat pointer en stack)
// Box<[T]>  — propiedad en heap (fat pointer en stack)
// Rc<[T]>   — referencia contada en heap
// Arc<[T]>  — referencia contada atomica en heap

// Lo mismo aplica a str (sin &):
// [T]  : str     :: tipo unsized de secuencia
// &[T] : &str    :: fat pointer a secuencia
// Vec<T> : String :: coleccion que posee la secuencia en heap
```

```rust
// El trait Sized y el marker ?Sized:
//
// Por defecto, los parametros genericos tienen un bound implicito T: Sized.
// Para aceptar tipos unsized, usas ?Sized:

fn print_debug<T: std::fmt::Debug + ?Sized>(value: &T) {
    println!("{:?}", value);
}

fn main() {
    let arr = [1, 2, 3];
    let slice: &[i32] = &arr;
    let text: &str = "hello";

    print_debug(slice);    // [1, 2, 3]
    print_debug(text);     // "hello"
    print_debug(&42_i32);  // 42
    // Sin ?Sized, no podrias pasar &[i32] ni &str.
}
```

## Comparacion con C

En C, los punteros no llevan informacion de longitud. Un
puntero `int *` apunta a un entero, pero no dice cuantos
hay a partir de esa direccion. Esto causa buffer overflows:

```c
// C — puntero sin longitud:
#include <stdio.h>

void print_array(int *arr, int len) {
    // El programador DEBE pasar len correctamente.
    // Si len es incorrecto → undefined behavior.
    for (int i = 0; i < len; i++) {
        printf("%d ", arr[i]);
    }
}

int main(void) {
    int data[] = {10, 20, 30};
    print_array(data, 3);   // correcto
    print_array(data, 10);  // BUG: lee memoria fuera del array
    // No hay error de compilacion. Undefined behavior.
    return 0;
}
```

```rust
// Rust — slice con longitud:
fn print_slice(data: &[i32]) {
    // data.len() siempre es correcto — es parte del fat pointer.
    for val in data {
        print!("{} ", val);
    }
    println!();
}

fn main() {
    let data = [10, 20, 30];
    print_slice(&data);       // correcto
    print_slice(&data[0..3]); // correcto

    // No hay forma de crear un slice con longitud incorrecta.
    // Intento de acceso fuera de rango:
    // let bad = data[10];
    // panic en runtime: index out of bounds
}
```

```rust
// Resumen de diferencias:
//
// | Aspecto            | C (int *)        | Rust (&[i32])        |
// |--------------------|------------------|----------------------|
// | Lleva longitud     | No               | Si (fat pointer)     |
// | Bounds checking    | No (manual)      | Si (automatico)      |
// | Buffer overflow    | Posible          | Panic (controlado)   |
// | Tamano del puntero | 8 bytes          | 16 bytes             |
// | Costo runtime      | Ninguno          | Minimo (1 compare)   |
// | Seguridad memoria  | Responsabilidad  | Garantizada por      |
// |                    | del programador  | el compilador        |
```

## String slices y UTF-8

`&str` es un slice de bytes UTF-8 valido. Los caracteres
UTF-8 pueden ocupar entre 1 y 4 bytes. Indexar por posicion
de byte puede caer en medio de un caracter multibyte y causar
panic:

```rust
fn main() {
    let emoji = "Hi ";
    println!("bytes = {}", emoji.len());   // 7  (H=1, i=1, space=1, emoji=4)

    // Indexar en medio de un caracter multibyte:
    // let bad = &emoji[3..4];
    // panic: byte index 4 is not a char boundary;
    // it is inside '' (bytes 3..7)
}
```

```rust
fn main() {
    let text = "Hola mundo";

    // chars() — iterador sobre caracteres Unicode:
    for ch in text.chars() { print!("{} ", ch); }
    println!();   // H o l a   m u n d o

    // char_indices() — iterador de (byte_index, char):
    for (i, ch) in text.char_indices() {
        println!("byte {} = '{}'", i, ch);
    }

    // bytes() — iterador sobre bytes individuales:
    for b in text.bytes() { print!("{:02x} ", b); }
    println!();
}
```

```rust
fn main() {
    let emoji = "";   // 4 bytes

    // is_char_boundary() verifica si un indice es inicio de caracter:
    println!("{}", emoji.is_char_boundary(0));  // true
    println!("{}", emoji.is_char_boundary(1));  // false (medio del emoji)

    // get() en vez de indexar para evitar panics:
    println!("{:?}", emoji.get(0..4));  // Some("")
    println!("{:?}", emoji.get(0..2));  // None  (no panic, solo None)
}
```

```rust
fn main() {
    let text = "cafe\u{0301}";  // cafe + combining accent
    println!("text  = {}", text);                  // cafe con acento visual
    println!("bytes = {}", text.len());            // 6
    println!("chars = {}", text.chars().count());  // 5

    // .len() cuenta bytes, .chars().count() cuenta code points.
    // Ninguno cuenta grafemas (caracteres visibles) correctamente.
    // Para grafemas, necesitas el crate unicode-segmentation.
}
```

---

## Ejercicios

### Ejercicio 1 — Fat pointers y tamanos

```rust
// Usar std::mem::size_of para verificar los tamanos de:
//   &i32, &[i32], &str, &[u8; 10], Box<[i32]>
//
// Para cada uno, predecir si es 8 o 16 bytes antes de ejecutar.
//
// Explicar por que &[i32; 10] es 8 bytes pero &[i32] es 16 bytes,
// aunque ambos pueden referirse a 10 enteros.
```

### Ejercicio 2 — Funcion generica con slices

```rust
// Escribir una funcion:
//   fn stats(data: &[i32]) -> Option<(i32, i32, i32)>
// que retorne (maximo, minimo, suma). None si el slice esta vacio.
//
// Probar con un array, un Vec, un sub-slice y &[].
// Verificar que la misma funcion acepta todas estas entradas.
```

### Ejercicio 3 — Slice patterns para parseo

```rust
// Escribir una funcion que reciba &[&str] (simulando argv)
// y use slice patterns para manejar:
//
//   [] => imprimir mensaje de uso
//   ["help"] => imprimir ayuda detallada
//   ["greet", name] => imprimir "Hello, <name>!"
//   ["add", a, b] => parsear a y b como i32, imprimir la suma
//   [cmd, ..] => imprimir "unknown command: <cmd>"
```

### Ejercicio 4 — UTF-8 y limites de caracter

```rust
// Dado el string "cafe\u{0301} con leche":
//
// 1. Imprimir el numero de bytes (.len())
// 2. Imprimir el numero de chars (.chars().count())
// 3. Intentar &text[0..5] — explicar por que funciona o no
// 4. Usar .char_indices() para encontrar los limites de cada caracter
// 5. Usar .get() para extraer sub-strings de forma segura
```
