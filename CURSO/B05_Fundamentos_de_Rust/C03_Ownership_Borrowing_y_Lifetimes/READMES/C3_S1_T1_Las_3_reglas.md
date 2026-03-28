# T01 --- Las 3 reglas de ownership

## Que es ownership

Ownership es el mecanismo central de Rust para garantizar seguridad
de memoria sin necesidad de un garbage collector. Cada valor en Rust
tiene exactamente un **dueno** (owner). El dueno es responsable de
liberar la memoria cuando ya no se necesita.

```rust
// En Rust, la variable que recibe el valor es su duena:
let s = String::from("hello");
// s es la duena de este String.
// Cuando s deje de existir, la memoria del String se libera.
```

```text
Comparacion con otros lenguajes:

C:
  char *s = malloc(6);       // el programador reserva memoria
  strcpy(s, "hello");
  free(s);                   // el programador libera manualmente
  // Si olvidas free() → memory leak.
  // Si llamas free() dos veces → double-free (undefined behavior).
  // Si usas s despues de free() → use-after-free (undefined behavior).

Java / Python:
  String s = "hello";        // el garbage collector maneja la memoria
  // No hay free(). El GC recorre periodicamente los objetos
  // y libera los que ya no tienen referencias.
  // Costo: pausas impredecibles, uso extra de CPU y memoria.

C++ (RAII):
  std::string s = "hello";   // el destructor libera al salir de scope
  // Rust se inspira en RAII de C++, pero lo refuerza con
  // el sistema de ownership en el compilador.
  // C++ permite copias implicitas y punteros colgantes.
  // Rust los prohibe en tiempo de compilacion.

Rust (ownership):
  let s = String::from("hello");  // s es duena
  // Al salir de scope, Rust llama drop() automaticamente.
  // No hay GC. No hay free() manual.
  // El compilador verifica las reglas de ownership.
  // Errores de memoria → errores de compilacion, no crashes en runtime.
```

```text
Las tres reglas de ownership:

1. Cada valor tiene una variable que es su duena (owner).
2. Solo puede haber un dueno a la vez.
3. Cuando el dueno sale de scope, el valor se elimina (drop).

Estas tres reglas son la base de todo el sistema de memoria de Rust.
El compilador las verifica y rechaza programas que las violen.
```

## Regla 1 --- Cada valor tiene un dueno

Todo valor en Rust tiene exactamente una variable que lo posee.
El binding de la variable ES el dueno. Ownership equivale a
responsabilidad de limpieza.

```rust
fn main() {
    // s es la duena de este String.
    // El String vive en el heap; s (puntero, longitud, capacidad) vive en el stack.
    let s = String::from("hello");

    // x es el dueno de este entero. El i32 vive enteramente en el stack.
    let x: i32 = 42;

    // v es la duena de este Vec.
    // El Vec (puntero, longitud, capacidad) vive en el stack;
    // los elementos [1, 2, 3] viven en el heap.
    let v: Vec<i32> = vec![1, 2, 3];

    // f es la duena de este File handle.
    let f = std::fs::File::open("/etc/hostname").unwrap();

    println!("{} {} {:?}", s, x, v);
}
// Al llegar aqui, f, v, x y s se eliminan (en orden inverso).
// Un valor SIEMPRE tiene dueno. No existen valores "huerfanos".
// Si nadie es dueno, el valor ya fue eliminado.
```

## Regla 2 --- Solo un dueno a la vez

Asignar un valor a otra variable **mueve** (move) la propiedad.
La variable original deja de ser valida. Esto previene el
double-free: solo un dueno llama drop.

```rust
fn main() {
    let s1 = String::from("hello");
    let s2 = s1; // s1 MUEVE su ownership a s2.
                  // s1 ya no es valida.
                  // s2 es la nueva (y unica) duena.

    println!("{}", s2); // OK: s2 es la duena.
    // println!("{}", s1); // ERROR de compilacion.
}
```

```text
Error del compilador al usar s1 despues del move:

error[E0382]: borrow of moved value: `s1`
 --> src/main.rs:5:20
  |
2 |     let s1 = String::from("hello");
  |         -- move occurs because `s1` has type `String`,
  |            which does not implement the `Copy` trait
3 |     let s2 = s1;
  |              -- value moved here
5 |     println!("{}", s1);
  |                    ^^ value borrowed here after move
```

```text
Diagrama stack/heap --- que pasa internamente con el move:

ANTES del move (let s2 = s1;):

  Stack                        Heap
  +-----------+               +---+---+---+---+---+
  | s1        |               |   |   |   |   |   |
  | ptr  -----+------------> | h | e | l | l | o |
  | len: 5    |               +---+---+---+---+---+
  | cap: 5    |
  +-----------+

DESPUES del move (let s2 = s1;):

  Stack                        Heap
  +-----------+
  | s1        |
  | (invalid) |               +---+---+---+---+---+
  +-----------+               |   |   |   |   |   |
  | s2        |               | h | e | l | l | o |
  | ptr  -----+------------> +---+---+---+---+---+
  | len: 5    |
  | cap: 5    |
  +-----------+

  Los datos del stack (ptr, len, cap) se COPIAN de s1 a s2.
  Los datos del heap NO se copian --- siguen en el mismo lugar.
  s1 se marca como invalida. El compilador prohibe usarla.

  Si Rust copiara el puntero sin invalidar s1 (como hace C),
  tendriamos dos variables apuntando al mismo heap.
  Al salir de scope, ambas intentarian liberar → double-free.
  El move evita esto: solo s2 llamara drop.
```

```rust
// El move aplica tambien con reasignacion:
fn main() {
    let mut s = String::from("hello");
    let s2 = s;         // s se mueve a s2, s queda invalida
    s = String::from("world"); // s recibe un NUEVO String, es valida otra vez
    println!("{}", s);   // OK: "world"
    println!("{}", s2);  // OK: "hello"
}
// s2 es duena de "hello", s es duena de "world". Ambos se dropean al final.
```

## Regla 3 --- Drop al salir de scope

Cuando el dueno de un valor sale de scope (llega al `}` que
cierra su bloque), Rust llama automaticamente la funcion `drop`
sobre ese valor. Esto libera los recursos asociados. Este patron
se llama RAII (Resource Acquisition Is Initialization), inspirado
en C++ pero obligatorio y verificado por el compilador en Rust.

```rust
// Drop con diferentes tipos de recursos:
use std::fs::File;
use std::sync::Mutex;

fn main() {
    // String: libera memoria del heap
    {
        let s = String::from("hello");
        println!("{}", s);
    } // drop(s) → libera los bytes del heap

    // File: cierra el file descriptor
    {
        let f = File::open("/etc/hostname").unwrap();
    } // drop(f) → cierra el file descriptor del SO
      // Equivalente a fclose() en C, pero automatico.

    // MutexGuard: libera el lock
    {
        let data = Mutex::new(42);
        let guard = data.lock().unwrap();
        println!("locked: {}", *guard);
    } // drop(guard) → desbloquea el Mutex automaticamente
      // En C, olvidar pthread_mutex_unlock() causa deadlock.
}
```

```rust
// El drop se llama en orden inverso de declaracion (LIFO):
fn main() {
    let a = String::from("primero");
    let b = String::from("segundo");
    let c = String::from("tercero");
    println!("{} {} {}", a, b, c);
}
// drop(c) → "tercero"
// drop(b) → "segundo"
// drop(a) → "primero"
// Orden inverso porque los valores posteriores podrian depender de los anteriores.

// Drop tambien se llama si la funcion retorna anticipadamente:
fn example() {
    let s = String::from("hello");
    if true {
        return; // s se dropea aqui, antes de llegar al } final.
    }
    println!("{}", s);
} // Si no hubo return, s se dropearia aqui.
```

```rust
// drop() explicito: liberar un valor antes de que termine su scope.
// Util para liberar locks o archivos tempranamente.
fn main() {
    let s = String::from("hello");
    println!("{}", s);
    drop(s); // liberar ahora, no al final del scope
    // println!("{}", s); // ERROR: use of moved value

    // drop() es simplemente: pub fn drop<T>(_x: T) {}
    // Toma ownership (lo mueve) y no hace nada.
    // Al terminar, _x sale de scope → se llama Drop::drop.
    //
    // No se puede llamar Drop::drop directamente:
    //   s.drop(); // ERROR: explicit use of destructor method
    // Se debe usar std::mem::drop() o dejar que el scope lo haga.
}
```

## Move semantics en detalle

La transferencia de ownership ocurre en tres contextos
principales: asignacion, llamadas a funciones y retorno
de funciones.

```rust
// Pasar un valor a una funcion MUEVE la ownership:
fn takes_ownership(s: String) {
    println!("Recibi: {}", s);
} // s se dropea aqui. El String se libera.

fn main() {
    let greeting = String::from("hello");
    takes_ownership(greeting);
    // greeting ya no es valida. Se movio a la funcion.
    // println!("{}", greeting); // ERROR: value used after move
}
```

```rust
// Retornar un valor DEVUELVE la ownership al llamador:
fn creates_string() -> String {
    let s = String::from("created inside");
    s // s se mueve al llamador. No se dropea aqui.
}

// Patron ida y vuelta: dar ownership y recibirla de regreso.
fn process_and_return(s: String) -> String {
    println!("Procesando: {}", s);
    s // devolver el mismo String
}

fn main() {
    let s = creates_string();
    let s = process_and_return(s);
    println!("De vuelta: {}", s);
}
// Este patron es incomodo. Para evitarlo, se usan references (borrowing).
```

```rust
// Move en structs, tuplas y campos parciales:
struct User {
    name: String,
    email: String,
    age: u32,
}

fn main() {
    let name = String::from("Alice");
    let email = String::from("alice@example.com");
    let user = User { name, email, age: 30 }; // name y email se mueven a los campos
    // println!("{}", name);  // ERROR: name se movio
    println!("User: {} <{}>, {}", user.name, user.email, user.age);

    // Move parcial: mover solo un campo del struct
    let extracted = user.name; // move parcial: solo name se mueve
    println!("Name: {}", extracted);
    println!("Age: {}", user.age);   // OK: u32 es Copy
    // println!("{}", user.email);   // OK si se usa solo, pero:
    // let u2 = user;               // ERROR: user esta parcialmente movido

    // Move en tuplas:
    let s1 = String::from("hello");
    let pair = (s1, 42); // s1 se mueve DENTRO de la tupla, 42 se copia
    // println!("{}", s1); // ERROR: s1 se movio
    println!("{} {}", pair.0, pair.1); // OK
}
```

```rust
// Move dentro de un vector:
fn main() {
    let mut names = vec![
        String::from("Alice"),
        String::from("Bob"),
        String::from("Carol"),
    ];

    // No se puede mover un elemento fuera de un Vec con indexacion:
    // let first = names[0]; // ERROR: cannot move out of index

    // Formas validas de extraer elementos:
    let first = names.remove(0);        // saca el elemento del Vec
    let last = names.pop().unwrap();     // saca el ultimo elemento
    let second = names.swap_remove(0);  // saca y reemplaza con el ultimo
    println!("{} {} {}", first, second, last);
}
```

## Ownership y el stack --- tipos Copy

Los tipos que viven enteramente en el stack implementan el trait
`Copy`. Al asignarlos, se **copian** en lugar de moverse.
La variable original sigue siendo valida.

```rust
fn main() {
    let x: i32 = 42;
    let y = x;      // x se COPIA a y (no se mueve).
    println!("x = {}, y = {}", x, y); // OK: ambos son validos

    let a: f64 = 3.14;
    let b = a;       // Copy
    println!("a = {}, b = {}", a, b); // OK

    let flag = true; // bool: Copy
    let c = 'R';     // char: Copy
    let other = flag;
    let d = c;
    println!("{} {} {} {}", flag, other, c, d); // OK: todos validos
}
```

```rust
// Tipos que implementan Copy:
//   - Enteros: i8, i16, i32, i64, i128, isize, u8..u128, usize
//   - Flotantes: f32, f64
//   - bool, char
//   - Tuplas SI todos sus elementos son Copy: (i32, f64) → Copy
//   - Arrays de tamano fijo SI el elemento es Copy: [i32; 5] → Copy
//   - References compartidas: &T siempre es Copy
//
// NO son Copy (tienen datos en el heap):
//   - String, Vec<T>, Box<T>
//   - (i32, String) — la tupla contiene un tipo no-Copy

fn main() {
    let point = (3, 4);    // tupla Copy
    let other = point;
    println!("{:?} {:?}", point, other); // OK: ambos validos

    let data = (42, String::from("hello")); // tupla NO Copy
    let data2 = data;  // move
    // println!("{:?}", data); // ERROR: data se movio
    println!("{:?}", data2);

    // Si necesitas duplicar un String, usas .clone() explicitamente:
    let s1 = String::from("hello");
    let s2 = s1.clone(); // copia profunda explicita
    println!("{} {}", s1, s2); // OK: s1 y s2 son independientes
    // clone() se detalla en T04_Copy_y_Clone.
}
```

```rust
// Copy y funciones:
fn takes_i32(n: i32) {
    println!("Recibi: {}", n);
}

fn main() {
    let num = 42;
    takes_i32(num);
    println!("num sigue valido: {}", num); // OK: i32 es Copy

    let text = String::from("hello");
    // takes_string(text);
    // println!("{}", text); // ERROR: String no es Copy, se movio
}
```

## Por que ownership importa

Ownership elimina clases enteras de bugs en tiempo de compilacion.
No hay costo en runtime --- todas las verificaciones ocurren antes
de que el programa se ejecute.

```rust
// 1. Previene use-after-free:
fn example_uaf() {
    let s = String::from("hello");
    drop(s); // liberacion explicita (equivalente a free en C)
    // println!("{}", s); // ERROR de compilacion
}
// En C, usar un puntero despues de free() compila y causa UB.

// 2. Previene double-free:
fn example_df() {
    let s1 = String::from("hello");
    let s2 = s1; // move: s1 ya no es valida
    // Al salir de scope, solo s2 llama drop. El move lo previene.
}

// 3. Previene dangling pointers:
// fn dangling() -> &String {
//     let s = String::from("hello");
//     &s  // ERROR: devolver referencia a valor local
// }
// Solucion: devolver el valor por ownership (no por referencia).
fn safe() -> String {
    let s = String::from("hello");
    s // se mueve al llamador
}
```

```text
Resumen de garantias de ownership:

  Bug                    C             Rust
  ─────────────────────────────────────────────────
  Memory leak            comun         raro (posible con Rc cycles)
  Use-after-free         comun         imposible (error de compilacion)
  Double-free            comun         imposible (error de compilacion)
  Dangling pointer       comun         imposible (error de compilacion)
  Buffer overflow        comun         imposible con safe Rust
  Data race              comun         imposible (error de compilacion)

  "Imposible" = en safe Rust. El bloque unsafe permite saltarse
  las verificaciones, pero eso es explicito y auditable.
```

## Visualizando ownership

Diagramas de scope que muestran cuando cada valor se crea y
se destruye. Leer de arriba abajo como linea de tiempo.

```rust
fn main() {
    // ───────────────────────────── scope de main ─────
    let a = String::from("alpha");       // a NACE aqui
    {
        // ─────────────────────── scope interno ───
        let b = String::from("beta");   // b NACE aqui
        println!("{} {}", a, b);        // a y b en uso
        // ─────────────────────── fin scope ───────
    } // b MUERE aqui: drop("beta"). a sigue viva.
    println!("{}", a);
    // ───────────────────────────── fin scope main ────
} // a MUERE aqui: drop("alpha")
```

```rust
fn main() {
    // ───────────────────────────── scope de main ─────
    let s1 = String::from("hello");     // s1 NACE
    let s2 = s1;                        // s1 MUERE (move), s2 NACE
    println!("{}", s2);                 // s2 en uso
    // ───────────────────────────── fin scope main ────
} // s2 MUERE: drop("hello")
  // s1 no se dropea (ya fue invalidada por el move).
```

```rust
fn create() -> String {
    let s = String::from("created");   // s NACE
    s // s se MUEVE al llamador (no se dropea aqui)
}

fn consume(s: String) {
    // s NACE aqui (recibida del llamador)
    println!("{}", s);
} // s MUERE: drop("created")

fn main() {
    let data = create();               // data recibe ownership
    consume(data);                     // data se MUEVE a consume()
    // data ya no es valida aqui.
} // data NO se dropea (ya se movio a consume).
```

```rust
// Ownership con condicionales y loops:
fn main() {
    let s = String::from("hello");
    let result = if true {
        s   // s se mueve aqui
    } else {
        String::from("default")
    };
    // El compilador sabe que s PODRIA haberse movido.
    // Aunque la condicion sea siempre true, analiza AMBAS ramas.
    println!("{}", result);
    // println!("{}", s); // ERROR: s posiblemente movida

    // Mover dentro de un loop es error:
    let s = String::from("hello");
    // for _ in 0..3 {
    //     let moved = s; // ERROR en la segunda iteracion
    // }

    // Solucion: clonar en cada iteracion:
    for i in 0..3 {
        let copy = s.clone();
        println!("Iteracion {}: {}", i, copy);
    } // copy se dropea en cada iteracion
    println!("Original: {}", s); // s sigue valida (nunca se movio)
}
```

```rust
// Caso practico: liberar un lock antes de hacer trabajo lento.
use std::sync::Mutex;

fn main() {
    let data = Mutex::new(vec![1, 2, 3]);

    // Opcion 1: scope explicito para controlar el drop
    let sum = {
        let guard = data.lock().unwrap();
        guard.iter().sum::<i32>()
    }; // guard se dropea al salir de este bloque → unlock

    // Opcion 2: drop() explicito
    let guard = data.lock().unwrap();
    let sum2: i32 = guard.iter().sum();
    drop(guard); // unlock explicito AHORA

    // Trabajo lento que no necesita el lock:
    println!("Sumas: {} {}", sum, sum2);
}
```

---

## Ejercicios

### Ejercicio 1 --- Predecir el comportamiento

```rust
// Para cada caso, predecir si compila o no.
// Si no compila, identificar la linea del error y la regla violada.
// Luego verificar con cargo check.

// Caso A:
fn main() {
    let a = String::from("hello");
    let b = a;
    let c = b;
    println!("{}", c);
}

// Caso B:
fn main() {
    let a = String::from("hello");
    let b = a;
    println!("{} {}", a, b);
}

// Caso C:
fn main() {
    let a = 42;
    let b = a;
    println!("{} {}", a, b);
}

// Caso D:
fn greet(name: String) {
    println!("Hello, {}!", name);
}

fn main() {
    let name = String::from("Alice");
    greet(name);
    greet(name);
}
```

### Ejercicio 2 --- Scope y drop

```rust
// Escribir un programa que cree tres String en scopes anidados.
// Usar println! para mostrar un mensaje ANTES y DESPUES de cada }.
// Predecir el orden exacto de los drops.
//
// Esqueleto:
fn main() {
    let s1 = String::from("outer");
    println!("1: s1 creado");
    {
        let s2 = String::from("middle");
        println!("2: s2 creado");
        {
            let s3 = String::from("inner");
            println!("3: s3 creado");
        } // ← que se dropea aqui?
        println!("4: despues de scope interno");
    } // ← que se dropea aqui?
    println!("5: despues de scope medio");
} // ← que se dropea aqui?
//
// Implementar Drop manualmente para verificar el orden.
// Pista: crear un wrapper struct con un campo String
// e implementar el trait Drop para imprimir al destruirse.
```

### Ejercicio 3 --- Transferencia de ownership en funciones

```rust
// Implementar las siguientes funciones:
//
// 1. create_greeting(name: &str) -> String
//    Crea y retorna un String con "Hello, <name>!".
//
// 2. append_exclamation(s: String) -> String
//    Toma ownership de un String, le agrega "!" y lo retorna.
//
// 3. print_and_return(s: String) -> String
//    Imprime el String y lo devuelve al llamador.
//
// En main:
//   a. Crear un greeting con create_greeting
//   b. Pasarlo por append_exclamation
//   c. Pasarlo por print_and_return
//   d. Imprimir el resultado final
//
// Verificar que el ownership se transfiere correctamente
// en cada paso. Intentar usar una variable despues de moverla
// para ver el error del compilador.
```

### Ejercicio 4 --- Move vs Copy

```rust
// Crear una funcion que reciba diferentes tipos y demostrar
// cuales se copian y cuales se mueven:
//
// fn test_copy_or_move() {
//     let integer: i32 = 42;
//     let float: f64 = 3.14;
//     let boolean: bool = true;
//     let character: char = 'R';
//     let tuple_copy: (i32, bool) = (1, true);
//     let array_copy: [i32; 3] = [1, 2, 3];
//     let string: String = String::from("hello");
//     let vec: Vec<i32> = vec![1, 2, 3];
//     let tuple_move: (i32, String) = (1, String::from("hi"));
//
//     // Para cada variable:
//     // 1. Asignar a otra variable
//     // 2. Intentar usar la variable original
//     // 3. Anotar como comentario si fue Copy o Move
//     // 4. Verificar con cargo check
// }
```
