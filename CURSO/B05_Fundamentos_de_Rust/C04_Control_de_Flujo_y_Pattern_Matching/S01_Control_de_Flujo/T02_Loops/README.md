# T02 — Loops

## Tipos de bucle en Rust

Rust tiene tres construcciones de bucle: `loop`, `while` y `for`.
Las tres son expresiones, pero solo `loop` puede devolver un valor
a traves de `break`. El mas comun en la practica es `for`, que
itera sobre cualquier cosa que implemente `IntoIterator`.

```rust
// Las tres formas basicas:

loop {
    // bucle infinito — necesita break para salir
    break;
}

while condition {
    // se ejecuta mientras condition sea true
}

for item in collection {
    // itera sobre cada elemento de collection
}

// Las tres son expresiones:
//   loop  → retorna el valor del break
//   while → retorna ()
//   for   → retorna ()
//
// Solo loop puede devolver un valor util. La razon:
// while y for podrian ejecutar cero iteraciones,
// y el compilador no puede garantizar que un break
// con valor se alcance.
```

## loop — bucle infinito

`loop` crea un bucle que se repite indefinidamente. Es la
construccion de bucle mas basica de Rust. Sin un `break`,
el tipo de retorno es `!` (never type) porque nunca termina:

```rust
fn main() {
    let mut count = 0;

    loop {
        count += 1;
        println!("Iteration {count}");

        if count >= 5 {
            break;
        }
    }

    println!("Finished after {count} iterations");
}
// Iteration 1
// ...
// Iteration 5
// Finished after 5 iterations

// loop sin break tiene tipo ! (never):
// fn infinite() -> ! { loop {} }
// Util para servidores, event loops, etc.
// El tipo ! se puede coercer a cualquier otro tipo.
```

```rust
// continue salta a la siguiente iteracion:
fn main() {
    let mut count = 0;
    loop {
        count += 1;
        if count % 2 == 0 { continue; } // salta pares
        println!("{count}");
        if count >= 9 { break; }
    }
}
// 1, 3, 5, 7, 9
```

## break con valor — loop como expresion

`loop` es una expresion: el valor pasado a `break` se convierte
en el valor de retorno del loop. Solo `loop` soporta esto
(no `while` ni `for`):

```rust
fn main() {
    let mut counter = 0;

    let result = loop {
        counter += 1;
        if counter == 10 {
            break counter * 2; // retorna 20
        }
    };

    println!("result = {result}"); // result = 20
}

// Todos los break deben retornar el mismo tipo:
// break 0;     // i32
// break -1;    // i32 — ok, mismo tipo
// break "text" // ERROR: &str no es i32
```

```rust
// while y for NO soportan break-con-valor:
//
// let x = while condition { break 42; };
// ERROR: `break` with value from a `while` loop
//
// let x = for i in 0..10 { break 42; };
// ERROR: `break` with value from a `for` loop
//
// La razon: podrian no ejecutar ninguna iteracion.
// Si la condicion es false o el iterador esta vacio,
// no hay break que produzca un valor.
// loop siempre ejecuta al menos una vez.
```

## while — bucle con condicion

`while` evalua una condicion booleana antes de cada iteracion.
Si es `false` desde el inicio, el cuerpo no se ejecuta nunca:

```rust
fn main() {
    let mut number = 3;

    while number != 0 {
        println!("{number}");
        number -= 1;
    }

    println!("Launch!");
}
// 3
// 2
// 1
// Launch!

// while es mas claro que loop cuando la condicion de salida es simple.
// loop es mejor cuando la condicion esta en medio del cuerpo
// o cuando necesitas retornar un valor.
```

## while let — destructuracion en la condicion

`while let` combina `while` con pattern matching. El bucle
continua mientras el patron coincida:

```rust
fn main() {
    let mut stack = vec![1, 2, 3, 4, 5];

    while let Some(top) = stack.pop() {
        println!("Popped: {top}");
    }

    println!("Stack is empty");
}
// Popped: 5
// ...
// Popped: 1
// Stack is empty

// while let es azucar sintactico para:
// loop {
//     match stack.pop() {
//         Some(val) => println!("{val}"),
//         None => break,
//     }
// }
```

## for — iteracion sobre colecciones

`for` es el bucle mas comun en Rust. Funciona con cualquier
tipo que implemente el trait `IntoIterator`:

```rust
fn main() {
    let names = ["Alice", "Bob", "Charlie"];
    for name in names {
        println!("Hello, {name}!");
    }
}
// Hello, Alice!
// Hello, Bob!
// Hello, Charlie!
```

```rust
// Rangos (ranges):
fn main() {
    for i in 0..5 { print!("{i} "); }
    println!();  // 0 1 2 3 4         (exclusivo)

    for i in 0..=5 { print!("{i} "); }
    println!();  // 0 1 2 3 4 5       (inclusivo)

    for i in (1..=5).rev() { print!("{i} "); }
    println!();  // 5 4 3 2 1         (inverso)

    for i in (0..20).step_by(5) { print!("{i} "); }
    println!();  // 0 5 10 15         (con step)
}
```

```rust
// Iteracion sobre diferentes colecciones:
fn main() {
    let numbers = vec![10, 20, 30];
    for n in &numbers { println!("{n}"); }

    for c in "hello".chars() { print!("{c} "); }
    println!(); // h e l l o

    use std::collections::HashMap;
    let mut scores = HashMap::new();
    scores.insert("Alice", 100);
    scores.insert("Bob", 85);
    for (name, score) in &scores {
        println!("{name}: {score}");
    }
}
```

## for con referencias — ownership en la iteracion

La forma en que invocas `for` sobre una coleccion determina
si se consume, se presta o se presta mutablemente:

```rust
fn main() {
    let mut data = vec![1, 2, 3, 4, 5];

    // Borrow inmutable — x: &i32. data sigue usable.
    for x in &data {
        println!("Read: {x}");
    }

    // Borrow mutable — x: &mut i32. Puedes modificar.
    for x in &mut data {
        *x *= 2;
    }
    println!("{data:?}"); // [2, 4, 6, 8, 10]

    // Move (consumir) — x: i32. data ya no existe.
    for x in data {
        println!("Consumed: {x}");
    }
    // println!("{data:?}"); // ERROR: data fue movida
}
```

```rust
// Lo que ocurre internamente — el compilador traduce for-in
// a IntoIterator::into_iter():
//
// for x in &collection    → .iter()      → Item = &T
// for x in &mut collection → .iter_mut() → Item = &mut T
// for x in collection     → .into_iter() → Item = T
```

## Labels para loops anidados

Cuando tienes bucles anidados, `break` y `continue` solo afectan
al bucle mas interno. Para controlar un bucle externo, usas
**labels** (empiezan con `'`):

```rust
fn main() {
    'outer: for i in 0..5 {
        for j in 0..5 {
            if i + j > 4 {
                break 'outer; // sale de ambos loops
            }
            print!("({i},{j}) ");
        }
        println!();
    }
    println!("Done");
}
// (0,0) (0,1) (0,2) (0,3) (0,4)
// (1,0) (1,1) (1,2) (1,3)
// ...
// Done
```

```rust
// continue con label — salta a la siguiente iteracion
// del bucle externo:
fn main() {
    'outer: for i in 0..4 {
        for j in 0..4 {
            if j == 2 { continue 'outer; } // salta al siguiente i
            print!("({i},{j}) ");
        }
    }
    println!();
}
// (0,0) (0,1) (1,0) (1,1) (2,0) (2,1) (3,0) (3,1)
// j=2 y j=3 nunca se procesan.
```

## break con label y valor

Se puede combinar un label con un valor en `break` para retornar
un valor desde un loop externo, terminando desde uno interno:

```rust
fn main() {
    let result = 'outer: loop {
        let mut count = 0;
        loop {
            count += 1;
            if count == 3 {
                break 'outer count * 100; // sale de ambos loops con valor
            }
            if count == 5 {
                break; // solo sale del loop interno
            }
        }
        break 'outer -1;
    };
    println!("result = {result}"); // result = 300
}
```

```rust
// Ejemplo practico: buscar en una matriz
fn find_in_matrix(matrix: &[Vec<i32>], target: i32) -> Option<(usize, usize)> {
    'search: loop {
        for (i, row) in matrix.iter().enumerate() {
            for (j, &val) in row.iter().enumerate() {
                if val == target {
                    break 'search Some((i, j));
                }
            }
        }
        break 'search None;
    }
}

// Nota: en una funcion, return suele ser mas claro que labels.
// Los labels brillan cuando necesitas salir de loops anidados
// sin retornar de la funcion completa.
```

## Loop como expresion — valores de retorno

```rust
fn main() {
    // loop retorna el valor del break:
    let found = loop { break 42; };
    println!("found = {found}"); // 42

    // while y for retornan ():
    let r1 = while false {};       // ()
    let r2 = for _ in 0..0 {};     // ()
}

// Por que while y for retornan () y no un valor:
//   let x = while condition { break 42; };
//   Si condition es false, el cuerpo nunca se ejecuta.
//   No hay break, no hay valor. El compilador lo prohibe.
//
//   loop no tiene este problema: siempre ejecuta al menos
//   una iteracion.
```

## Performance — abstracciones de costo cero

Los bucles `for` sobre iteradores son **abstracciones de costo cero**.
El compilador genera el mismo codigo maquina que un bucle con indices:

```rust
// Estas dos funciones generan codigo equivalente:
fn sum_for(data: &[i32]) -> i32 {
    let mut total = 0;
    for &x in data { total += x; }
    total
}

fn sum_index(data: &[i32]) -> i32 {
    let mut total = 0;
    let mut i = 0;
    while i < data.len() {
        total += data[i]; // bounds check en cada acceso
        i += 1;
    }
    total
}

// for es mas seguro (sin riesgo de indice fuera de rango),
// igual de rapido, y mas idiomatico.
// .iter() permite al compilador eliminar bounds checks.
```

```rust
// Iteradores encadenados tambien son zero-cost:
fn main() {
    let data = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

    let result: i32 = data.iter()
        .filter(|&&x| x % 2 == 0)
        .map(|&x| x * x)
        .sum();

    println!("{result}"); // 220 (4 + 16 + 36 + 64 + 100)
    // El compilador fusiona filter, map y sum en un solo
    // bucle. No crea colecciones intermedias.
}
```

## Patrones comunes con for

```rust
// enumerate — indice y valor:
fn main() {
    let fruits = vec!["apple", "banana", "cherry"];
    for (i, fruit) in fruits.iter().enumerate() {
        println!("{i}: {fruit}");
    }
}
// 0: apple
// 1: banana
// 2: cherry
```

```rust
// zip — combinar dos iteradores:
fn main() {
    let names = vec!["Alice", "Bob", "Charlie"];
    let scores = vec![95, 87, 92];

    for (name, score) in names.iter().zip(scores.iter()) {
        println!("{name}: {score}");
    }
    // zip se detiene cuando el iterador mas corto se agota.
}
```

```rust
// windows y chunks:
fn main() {
    let data = [1, 2, 3, 4, 5];

    // windows(n) — ventana deslizante:
    for w in data.windows(3) { println!("{w:?}"); }
    // [1, 2, 3], [2, 3, 4], [3, 4, 5]

    // chunks(n) — porciones sin solapamiento:
    for c in data.chunks(2) { println!("{c:?}"); }
    // [1, 2], [3, 4], [5]
}
```

```rust
// Patron de reintento con loop + break:
fn try_connect() -> Result<String, String> {
    Err("Connection refused".to_string())
}

fn main() {
    let mut attempts = 0;
    let connection = loop {
        attempts += 1;
        match try_connect() {
            Ok(conn) => break conn,
            Err(e) => {
                println!("Attempt {attempts}: {e}");
                if attempts >= 3 { break String::from("gave up"); }
            }
        }
    };
    println!("Result: {connection}");
}
```

```rust
// Transformacion: preferir iteradores sobre for + push:
fn main() {
    let numbers = vec![1, 2, 3, 4, 5];

    // Menos idiomatico:
    let mut doubled = Vec::new();
    for n in &numbers { doubled.push(n * 2); }

    // Mas idiomatico:
    let doubled: Vec<i32> = numbers.iter().map(|n| n * 2).collect();
    println!("{doubled:?}"); // [2, 4, 6, 8, 10]

    // Regla: for para efectos secundarios (I/O, mutacion).
    // Iteradores + collect para transformar datos.
}
```

## Resumen comparativo

```rust
// Cuando usar cada tipo de loop:
//
// loop:  bucles infinitos, reintentos, cuando necesitas un valor
//        de retorno, cuando la condicion de salida esta en medio.
// while: condicion simple al inicio, while let para Option/Result.
// for:   iteracion sobre colecciones y rangos. El mas comun.
//        Siempre preferir for sobre while con indice manual.
//
// Tabla de capacidades:
//
// Caracteristica          | loop | while | for
// ------------------------|------|-------|-----
// break                   |  si  |  si   |  si
// continue                |  si  |  si   |  si
// break con valor         |  si  |  no   |  no
// Tipo de retorno         |  T   |  ()   |  ()
// Labels                  |  si  |  si   |  si
// Iteracion sobre rangos  |  no  |  no   |  si
```

---

## Ejercicios

### Ejercicio 1 — Fibonacci con loop

```rust
// Usar loop con break-con-valor para encontrar el primer
// numero de Fibonacci que exceda 1000.
//
// Fibonacci: 1, 1, 2, 3, 5, 8, 13, 21, ...
// Cada numero es la suma de los dos anteriores.
//
// El loop debe:
//   - Mantener dos variables (a, b) para los dos ultimos valores
//   - Verificar si b > 1000
//   - Si si, hacer break con el valor de b
//   - Si no, calcular el siguiente: (a, b) = (b, a + b)
//
// Resultado esperado: 1597
```

### Ejercicio 2 — Busqueda en matriz con labels

```rust
// Dada una matriz 5x5 de i32 (Vec<Vec<i32>>), escribir una
// funcion que busque un valor y retorne su posicion (fila, columna)
// usando un loop con label ('search) y break con label y valor.
//
// fn find(matrix: &Vec<Vec<i32>>, target: i32) -> Option<(usize, usize)>
//
// Probar con:
//   let m = vec![
//       vec![1, 2, 3, 4, 5],
//       vec![6, 7, 8, 9, 10],
//       vec![11, 12, 13, 14, 15],
//       vec![16, 17, 18, 19, 20],
//       vec![21, 22, 23, 24, 25],
//   ];
//   find(&m, 18) → Some((3, 2))
//   find(&m, 99) → None
```

### Ejercicio 3 — Comparar ownership en for

```rust
// Crear un Vec<String> con 5 palabras.
// Escribir tres funciones que la recorran con for:
//
// 1. fn print_borrowed(words: &Vec<String>)
//    Usa for word in words (itera con &String).
//    Verificar que el Vec original sigue usable despues.
//
// 2. fn print_uppercase(words: &mut Vec<String>)
//    Usa for word in words (itera con &mut String).
//    Convierte cada palabra a mayusculas con word.make_ascii_uppercase().
//    Verificar que el Vec original fue modificado.
//
// 3. fn print_consumed(words: Vec<String>)
//    Usa for word in words (consume el Vec).
//    Intentar usar el Vec despues → error de compilacion.
//    Comentar la linea que causa el error y explicar por que.
//
// Ejecutar las tres en orden: borrowed, uppercase, consumed.
```
