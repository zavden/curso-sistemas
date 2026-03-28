# T02 — Arrays

## Que es un array

Un array es una coleccion de tamano fijo de elementos del mismo tipo,
almacenada en el stack. En Rust el tipo de un array es `[T; N]`,
donde `T` es el tipo de los elementos y `N` es la cantidad, conocida
en tiempo de compilacion. `N` es parte del tipo: `[i32; 5]` y
`[i32; 3]` son tipos **diferentes** e incompatibles entre si.

```rust
// Declaracion con tipo explicito:
let arr: [i32; 5] = [1, 2, 3, 4, 5];

// El compilador infiere el tipo si hay suficiente informacion:
let arr = [1, 2, 3, 4, 5]; // inferido como [i32; 5]

// Tipo explicito con otros tipos:
let floats: [f64; 3] = [1.0, 2.5, 3.7];
let bytes: [u8; 4] = [0xFF, 0xAB, 0x00, 0x13];
let flags: [bool; 3] = [true, false, true];
```

```rust
// El tamano es parte del tipo.
// [i32; 3] y [i32; 5] son tipos DISTINTOS:

fn takes_five(arr: [i32; 5]) {
    println!("{:?}", arr);
}

let three = [1, 2, 3];       // [i32; 3]
let five = [1, 2, 3, 4, 5];  // [i32; 5]

takes_five(five);   // OK
// takes_five(three); // ERROR: expected [i32; 5], found [i32; 3]

// Esto es muy diferente de C, donde int arr[3] y int arr[5]
// se pasan como int* y se pierde la informacion de tamano.
// En Rust el compilador garantiza que el tamano sea correcto.
```

## Inicializacion

Hay dos formas de inicializar un array: listando los elementos
o usando la sintaxis de repeticion.

```rust
// 1. Literal — listar cada elemento:
let arr = [10, 20, 30, 40, 50];

// 2. Repeticion — [valor; cantidad]:
let zeros = [0; 100];        // 100 elementos, todos 0
let ones = [1u8; 256];       // 256 bytes, todos 1
let blank = [' '; 80];       // 80 espacios

// La sintaxis de repeticion es [expr; N].
// expr se evalua una vez y se copia N veces.
// El tipo de expr debe implementar Copy.
```

```rust
// String NO implementa Copy — no se puede usar con repeticion:
// let strings = [String::from("hello"); 3]; // ERROR

// Para tipos sin Copy, usar std::array::from_fn:
let strings: [String; 3] = std::array::from_fn(|i| format!("item_{}", i));
// strings = ["item_0", "item_1", "item_2"]

// Arrays vacios (tamano 0) y de un solo elemento son validos:
let empty: [i32; 0] = [];
let single = [42]; // [i32; 1]
```

## Acceso por indice

Los arrays se acceden con `arr[i]` donde `i` empieza en 0.
Rust verifica los limites en tiempo de ejecucion y hace panic
si el indice esta fuera de rango. Esto previene bugs de acceso
a memoria invalida que en C causan undefined behavior.

```rust
let arr = [10, 20, 30, 40, 50];

// Acceso valido:
println!("{}", arr[0]); // 10
println!("{}", arr[4]); // 50

// Acceso fuera de limites — panic en runtime:
// println!("{}", arr[5]);
// thread 'main' panicked at 'index out of bounds:
//   the len is 5 but the index is 5'

// En C esto seria undefined behavior:
// int arr[5] = {10, 20, 30, 40, 50};
// printf("%d\n", arr[5]); // UB: lee basura, o crashea, o parece funcionar
// En Rust el programa se detiene con un mensaje claro.
```

```rust
// Acceso seguro con .get() — devuelve Option<&T>:
let arr = [10, 20, 30, 40, 50];

match arr.get(2) {
    Some(val) => println!("arr[2] = {}", val), // arr[2] = 30
    None => println!("Index out of bounds"),
}

match arr.get(99) {
    Some(val) => println!("arr[99] = {}", val),
    None => println!("Index out of bounds"),    // se ejecuta esto
}

// .get() nunca hace panic. Devuelve None si el indice es invalido.
// Util cuando el indice viene de entrada del usuario u otra fuente externa.
```

## Mutabilidad

Para modificar elementos de un array, el binding debe ser `mut`:

```rust
let arr = [1, 2, 3];
// arr[0] = 10; // ERROR: cannot assign to `arr[0]`, as `arr` is not mutable

let mut arr = [1, 2, 3];
arr[0] = 10;
arr[2] = 30;
println!("{:?}", arr); // [10, 2, 30]

// Modificar con un loop:
let mut arr = [0; 5];
for i in 0..arr.len() {
    arr[i] = (i * i) as i32;
}
println!("{:?}", arr); // [0, 1, 4, 9, 16]

// Tambien con iter_mut:
let mut arr = [1, 2, 3, 4, 5];
for x in arr.iter_mut() {
    *x *= 2;
}
println!("{:?}", arr); // [2, 4, 6, 8, 10]
```

## El tamano como parte del tipo

El hecho de que `N` sea parte del tipo tiene consecuencias
importantes. No se puede escribir una funcion que acepte arrays
de cualquier tamano sin usar generics o slices.

```rust
// Esta funcion SOLO acepta arrays de exactamente 3 elementos:
fn sum_three(arr: [i32; 3]) -> i32 {
    arr[0] + arr[1] + arr[2]
}

let a = [1, 2, 3];
println!("{}", sum_three(a)); // 6

let b = [1, 2, 3, 4];
// println!("{}", sum_three(b)); // ERROR: expected [i32; 3], found [i32; 4]

// Solucion 1: slices (&[T]) para abstraer sobre el tamano:
fn sum_all(slice: &[i32]) -> i32 {
    slice.iter().sum()
}

let a = [1, 2, 3];
let b = [1, 2, 3, 4, 5];
println!("{}", sum_all(&a)); // 6  — &[i32; 3] se coerce a &[i32]
println!("{}", sum_all(&b)); // 15 — &[i32; 5] se coerce a &[i32]

// Solucion 2: const generics (Rust 1.51+):
fn sum_array<const N: usize>(arr: [i32; N]) -> i32 {
    arr.iter().sum()
}
println!("{}", sum_array([1, 2, 3]));       // 6
println!("{}", sum_array([10, 20, 30, 40])); // 100
```

## Arrays en el stack

Los arrays se almacenan en el stack. Su tamano es fijo y conocido
en compilacion, asi que no necesitan asignacion de memoria en el
heap. Esto los hace rapidos de crear y destruir.

```rust
use std::mem;

let arr = [0i32; 10];
println!("{}", mem::size_of_val(&arr)); // 40 (10 * 4 bytes)

// Cuidado: arrays muy grandes causan stack overflow (stack tipico = 8 MB).
// Para arrays grandes, mover al heap:
let boxed: Box<[i32; 1000]> = Box::new([0i32; 1000]);
// O usar Vec:
let vec = vec![0i32; 1_000_000]; // 1 millon de elementos en el heap
```

## Iteracion

Hay varias formas de iterar sobre un array. La mas comun es
`for x in &arr` para leer elementos sin consumir el array.

```rust
let arr = [10, 20, 30, 40, 50];

// for in referencia — no consume el array:
for x in &arr {
    print!("{} ", x); // 10 20 30 40 50
}

// for in el array — consume el array (Rust 2021+):
// for x in arr { ... } // arr ya no se puede usar despues

// Con .enumerate() — indice y valor:
for (i, x) in arr.iter().enumerate() {
    println!("[{}] = {}", i, x);
}

// Iteracion mutable:
let mut arr = [1, 2, 3, 4, 5];
for x in arr.iter_mut() {
    *x += 10;
}
println!("{:?}", arr); // [11, 12, 13, 14, 15]
```

```rust
// Nota sobre IntoIterator para arrays:
//
// Antes de Rust 2021:
//   for x in arr      → iteraba por referencia (&T)
//   arr.into_iter()   → iteraba por referencia (&T)
//
// Desde Rust 2021:
//   for x in arr      → consume el array, itera por valor (T)
//   arr.into_iter()   → consume el array, itera por valor (T)
//
// for x in &arr siempre itera por referencia en ambas ediciones.
// Si necesitas compatibilidad con ediciones anteriores, usar arr.iter().
```

## Metodos utiles

Los arrays exponen metodos de slices a traves de `Deref`
y tienen algunos metodos propios.

```rust
let arr = [3, 1, 4, 1, 5, 9, 2, 6];

// Informacion basica:
println!("{}", arr.len());          // 8
println!("{}", arr.is_empty());     // false
println!("{}", arr.contains(&4));   // true
println!("{}", arr.contains(&7));   // false

// Ordenar y revertir (requiere mut):
let mut arr = [3, 1, 4, 1, 5, 9, 2, 6];
arr.sort();
println!("{:?}", arr); // [1, 1, 2, 3, 4, 5, 6, 9]

arr.reverse();
println!("{:?}", arr); // [9, 6, 5, 4, 3, 2, 1, 1]

// sort_unstable() es mas rapido pero no preserva
// el orden relativo de elementos iguales.
```

```rust
// windows() — ventanas deslizantes:
let arr = [1, 2, 3, 4, 5];
for w in arr.windows(3) {
    print!("{:?} ", w); // [1, 2, 3] [2, 3, 4] [3, 4, 5]
}

// chunks() — dividir en grupos de N (ultimo chunk puede ser menor):
let arr = [1, 2, 3, 4, 5, 6, 7];
for chunk in arr.chunks(3) {
    print!("{:?} ", chunk); // [1, 2, 3] [4, 5, 6] [7]
}

// split() — dividir por un predicado:
let arr = [1, 0, 2, 0, 3];
let parts: Vec<&[i32]> = arr.split(|x| *x == 0).collect();
println!("{:?}", parts); // [[1], [2], [3]]
```

```rust
// map() — metodo propio de arrays (Rust 1.55+):
// Devuelve un nuevo array del mismo tamano, sin asignacion de heap.
let arr = [1, 2, 3, 4];
let squared: [i32; 4] = arr.map(|x| x * x);
println!("{:?}", squared); // [1, 4, 9, 16]

// A diferencia de iter().map().collect(), no necesita Vec.
let names = ["alice", "bob", "carol"];
let upper: [String; 3] = names.map(|s| s.to_uppercase());
println!("{:?}", upper); // ["ALICE", "BOB", "CAROL"]

// Metodos de iterador via .iter():
let arr = [1, 2, 3, 4, 5];
let sum: i32 = arr.iter().sum();           // 15
let doubled: Vec<i32> = arr.iter().map(|x| x * 2).collect();
```

## Arrays multidimensionales

Los arrays se pueden anidar para crear matrices. Un array 2D
es un array de arrays.

```rust
// Matriz 2x3 (2 filas, 3 columnas):
let matrix: [[i32; 3]; 2] = [
    [1, 2, 3],
    [4, 5, 6],
];

// Acceso: matrix[fila][columna]
println!("{}", matrix[0][1]); // 2  (fila 0, columna 1)
println!("{}", matrix[1][2]); // 6  (fila 1, columna 2)

// Inicializacion con repeticion:
let grid: [[i32; 4]; 3] = [[0; 4]; 3]; // 3 filas de 4 ceros
```

```rust
// Iteracion sobre una matriz:
let matrix = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];
for row in &matrix {
    for val in row {
        print!("{:>3}", val);
    }
    println!();
}
//   1  2  3
//   4  5  6
//   7  8  9

// Modificar la diagonal:
let mut matrix = [[0i32; 3]; 3];
for i in 0..3 {
    matrix[i][i] = 1;
}
// [[1, 0, 0], [0, 1, 0], [0, 0, 1]]  — matriz identidad

// Tres dimensiones:
let cube: [[[i32; 2]; 2]; 2] = [
    [[1, 2], [3, 4]],
    [[5, 6], [7, 8]],
];
println!("{}", cube[1][0][1]); // 6
```

## Arrays y pattern matching

Rust permite destructurar arrays con patrones. Esto funciona
en `let`, `match`, `if let` y parametros de funciones.

```rust
// Destructurar todos los elementos:
let arr = [1, 2, 3];
let [a, b, c] = arr;
println!("{} {} {}", a, b, c); // 1 2 3

// Patrones parciales con ..:
let arr = [1, 2, 3, 4, 5];
let [first, ..] = arr;              // first = 1
let [first, .., last] = arr;        // first = 1, last = 5
let [first, second, ..] = arr;      // first = 1, second = 2

// Capturar el resto con @:
let [first, rest @ ..] = arr;
println!("{}", first);         // 1
println!("{:?}", rest);        // [2, 3, 4, 5]

let [head @ .., last] = arr;
println!("{:?}", head);        // [1, 2, 3, 4]
println!("{}", last);          // 5
```

```rust
// Pattern matching en match:
let arr = [1, 2, 3];
match arr {
    [1, _, _] => println!("Starts with 1"),
    [_, _, 3] => println!("Ends with 3"),
    _ => println!("Something else"),
}
// Starts with 1

// Con guardas:
let point = [3, 4];
match point {
    [x, y] if x == y => println!("On diagonal"),
    [x, y] if x == 0 => println!("On Y axis"),
    [x, y] if y == 0 => println!("On X axis"),
    [x, y] => println!("Point at ({}, {})", x, y),
}
// Point at (3, 4)
```

## Comparacion con Vec

Los arrays tienen tamano fijo en compilacion. `Vec<T>` es la
alternativa cuando el tamano no se conoce hasta runtime o puede
cambiar.

```rust
// Array — tamano fijo, stack, sin asignacion de heap:
let arr = [1, 2, 3, 4, 5];
// No se puede agregar ni quitar elementos.

// Vec — tamano dinamico, heap, puede crecer y encoger:
let mut vec = vec![1, 2, 3, 4, 5];
vec.push(6);       // agregar al final
vec.pop();          // quitar del final
vec.insert(0, 0);   // insertar en posicion
vec.remove(0);      // quitar en posicion
```

```rust
// Cuando usar array:
// - Tamano conocido en compilacion que no cambia
// - Rendimiento critico (sin asignacion de heap)
// - Datos pequenos y fijos (coordenadas, colores, buffers)
let rgb: [u8; 3] = [255, 128, 0];
let pos: [f64; 3] = [1.0, 2.0, 3.0];

// Cuando usar Vec:
// - Tamano determinado en runtime
// - Necesitas agregar o quitar elementos
// - Datos de archivo, red o entrada del usuario
// - Tamano potencialmente grande (> unos pocos KB)
let mut results: Vec<i32> = Vec::new();
for i in 0..10 {
    if i % 2 == 0 {
        results.push(i * i);
    }
}
```

```rust
// Convertir entre array y Vec:
let arr = [1, 2, 3, 4, 5];
let vec: Vec<i32> = arr.to_vec();            // Array a Vec

let vec = vec![1, 2, 3];
let arr: [i32; 3] = vec.try_into().unwrap(); // Vec a array (Err si tamano no coincide)
```

```rust
// Resumen de diferencias:
//
// | Caracteristica     | Array [T; N]       | Vec<T>             |
// |--------------------|--------------------|--------------------|
// | Tamano             | Fijo (compilacion) | Dinamico (runtime) |
// | Almacenamiento     | Stack              | Heap               |
// | Asignacion         | Ninguna            | malloc/realloc     |
// | Agregar elementos  | No                 | Si (push, insert)  |
// | Quitar elementos   | No                 | Si (pop, remove)   |
// | Rendimiento        | Mas rapido         | Leve overhead      |
// | Copy               | Si, si T: Copy     | No (se clona)      |
// | Tipo de tamano     | Parte del tipo     | Solo en runtime    |
```

---

## Ejercicios

### Ejercicio 1 — Operaciones basicas

```rust
// Crear un array de 10 enteros (i32) con los valores del 1 al 10.
//
// 1. Imprimir el primer y ultimo elemento.
// 2. Calcular la suma de todos los elementos usando un for loop.
// 3. Calcular la suma usando .iter().sum().
// 4. Crear un segundo array con los mismos valores al cuadrado
//    usando .map().
// 5. Verificar que el segundo array contiene 100 con .contains().
//
// Salida esperada:
//   First: 1, Last: 10
//   Sum (loop): 55
//   Sum (iter): 55
//   Squared: [1, 4, 9, 16, 25, 36, 49, 64, 81, 100]
//   Contains 100: true
```

### Ejercicio 2 — Acceso seguro

```rust
// Crear un array de 5 elementos: [10, 20, 30, 40, 50].
//
// 1. Escribir una funcion safe_get(arr: &[i32], index: usize)
//    que use .get() para devolver el valor o imprimir un mensaje
//    de error si el indice esta fuera de limites.
// 2. Llamar a safe_get con indices 0, 3, 5 y 99.
// 3. Observar que nunca hace panic, a diferencia de arr[i].
//
// Salida esperada:
//   arr[0] = 10
//   arr[3] = 40
//   Error: index 5 out of bounds
//   Error: index 99 out of bounds
```

### Ejercicio 3 — Pattern matching con arrays

```rust
// Crear una funcion classify(arr: &[i32; 3]) que use match para
// clasificar un array de 3 elementos:
//
//   [0, 0, 0]       -> "Origin"
//   [x, 0, 0]       -> "On X axis"
//   [0, y, 0]       -> "On Y axis"
//   [0, 0, z]       -> "On Z axis"
//   [x, y, 0]       -> "On XY plane"
//   [x, 0, z]       -> "On XZ plane"
//   [0, y, z]       -> "On YZ plane"
//   [x, y, z]       -> "General point"
//
// Probar con: [0,0,0], [5,0,0], [0,3,0], [1,2,3], [1,2,0]
// Los patrones se prueban en orden. El primero que coincide gana.
```

### Ejercicio 4 — Matriz e iteracion

```rust
// Crear una funcion que reciba una matriz [[i32; 3]; 3] e imprima:
//   1. La matriz formateada como tabla (alineada a la derecha, 4 chars).
//   2. La suma de cada fila.
//   3. La suma de cada columna.
//   4. La suma de la diagonal principal.
//
// Usar la matriz:
//   [[1, 2, 3],
//    [4, 5, 6],
//    [7, 8, 9]]
//
// Salida esperada:
//      1   2   3  | row sum =  6
//      4   5   6  | row sum = 15
//      7   8   9  | row sum = 24
//   col sums: 12, 15, 18
//   diagonal sum: 15
```
