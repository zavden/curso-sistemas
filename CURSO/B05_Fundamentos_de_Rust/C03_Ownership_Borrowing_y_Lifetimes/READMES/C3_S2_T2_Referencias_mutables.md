# T02 — Referencias mutables (&mut T)

## Crear una referencia mutable

Una referencia mutable permite **modificar** el dato al que apunta.
Se crea con `&mut` y requiere que la variable original sea `let mut`.
Es la forma de prestar acceso de escritura sin transferir ownership:

```rust
fn main() {
    let mut x = 5;
    let r = &mut x;  // r is a mutable reference to x

    *r = 10;          // modify x through the reference
    // The * operator dereferences r to access the value it points to.
    // Writing ALWAYS requires explicit *.
    // Reading auto-derefs in many contexts (println, field access).

    println!("{}", r); // 10 — reading auto-derefs
}
```

```rust
// The variable MUST be declared with let mut.
// A mutable reference to an immutable variable does not compile:

fn main() {
    let x = 5;          // immutable binding
    let r = &mut x;     // ERROR
}

// error[E0596]: cannot borrow `x` as mutable,
//               as it is not declared as mutable
//  --> src/main.rs:3:13
//   |
// 2 |     let x = 5;
//   |         - help: consider changing this to be mutable: `mut x`
// 3 |     let r = &mut x;
//   |             ^^^^^^ cannot borrow as mutable
```

```rust
// Two things must be true:
// 1. The variable must be let mut (the binding allows mutation).
// 2. The reference must be &mut (the borrow allows write access).
//
// let mut x = 5;
//     ^^^         the binding is mutable
// let r = &mut x;
//         ^^^^    the borrow is mutable
//
// Both are required. Neither alone is sufficient.
```

```rust
// With compound types, field access auto-derefs:
struct Point { x: i32, y: i32 }

fn main() {
    let mut p = Point { x: 1, y: 2 };
    let r = &mut p;

    r.x = 10;   // auto-deref: equivalent to (*r).x = 10
    r.y = 20;   // no need for explicit *

    println!("({}, {})", r.x, r.y); // (10, 20)
}
```

## Regla de exclusividad

Esta es LA regla fundamental del borrowing mutable en Rust:
**solo puede existir UNA referencia mutable a un dato a la vez**.
No puede haber ninguna otra referencia (ni mutable ni compartida)
mientras la referencia mutable esta activa:

```rust
fn main() {
    let mut s = String::from("hello");

    let r1 = &mut s;
    let r2 = &mut s; // ERROR: cannot borrow `s` as mutable
                      //        more than once at a time

    println!("{}, {}", r1, r2);
}

// error[E0499]: cannot borrow `s` as mutable more than once at a time
//  --> src/main.rs:5:14
//   |
// 4 |     let r1 = &mut s;
//   |              ------ first mutable borrow occurs here
// 5 |     let r2 = &mut s;
//   |              ^^^^^^ second mutable borrow occurs here
// 6 |
// 7 |     println!("{}, {}", r1, r2);
//   |                        -- first borrow later used here
```

```rust
// The rule stated precisely:
//
// At any given point in the program, for a particular piece of data,
// you can have EITHER:
//   - exactly ONE mutable reference (&mut T)
//   - OR any number of shared references (&T)
//
// But NEVER both at the same time.
//
// "At the same time" means: both references are alive
// (i.e., both are created and at least one is used later).
```

```rust
// Sequential mutable borrows are fine.
// The key is that their lifetimes must not OVERLAP:

fn main() {
    let mut s = String::from("hello");

    {
        let r1 = &mut s;
        r1.push_str(" world");
    } // r1 goes out of scope here — borrow ends

    let r2 = &mut s;  // OK: r1 is already gone
    r2.push_str("!");

    println!("{}", s); // "hello world!"
}
```

## Por que exclusividad

La regla de exclusividad existe para prevenir **data races**
en tiempo de compilacion. Un data race ocurre cuando se
cumplen tres condiciones simultaneamente:

```rust
// A data race requires ALL THREE conditions:
//
// 1. Two or more pointers access the same data at the same time.
// 2. At least one of them is writing.
// 3. There is no mechanism to synchronize access.
//
// Rust's borrow rules make condition (1) impossible when
// condition (2) is true:
//
//   - If someone is writing (&mut T), nobody else can read or write.
//   - If multiple readers exist (&T), nobody can write.
//
// Therefore: data races are impossible in safe Rust.
// This guarantee is enforced at COMPILE TIME, not runtime.
// There is zero runtime cost.
```

```rust
// In C, nothing prevents two pointers to the same data:
//
//   int x = 5;
//   int *a = &x;
//   int *b = &x;
//   *a = 10;             // write through a
//   printf("%d\n", *b);  // read through b — potential race
//
// In Rust, the equivalent is rejected by the compiler:

fn main() {
    let mut x = 5;
    let a = &mut x;
    let b = &x;      // ERROR: cannot borrow as immutable
                      //        because also borrowed as mutable
    *a = 10;
    println!("{}", b);
}
```

## No aliasing

La referencia mutable `&mut T` garantiza que **no existen aliases**
al mismo dato. Un alias es otro puntero o referencia que apunta
a la misma memoria. Esta garantia de no-aliasing permite al
compilador optimizar agresivamente:

```rust
fn add_to_both(a: &mut i32, b: &mut i32) {
    *a += 1;
    *b += 1;
}

// The compiler KNOWS that a and b point to different memory.
// Why? Because two &mut references to the same data cannot coexist.
//
// This means the compiler can:
//   - Reorder the writes
//   - Keep values in registers instead of reloading from memory
//   - Vectorize operations
//
// In C, the compiler cannot assume this unless you use `restrict`:
//   void add_to_both(int *restrict a, int *restrict b) { ... }
//
// But in C, `restrict` is a promise the programmer makes.
// If the programmer lies, the behavior is undefined.
// In Rust, the compiler ENFORCES it. You cannot call:
//   add_to_both(&mut x, &mut x);  // ERROR at compile time
```

```rust
// The swap pattern: the compiler knows a and b are disjoint.

fn swap(a: &mut i32, b: &mut i32) {
    let tmp = *a;
    *a = *b;
    *b = tmp;
}

fn main() {
    let mut x = 1;
    let mut y = 2;
    swap(&mut x, &mut y);
    println!("x={}, y={}", x, y); // x=2, y=1

    // This works because x and y are different variables.
    // swap(&mut x, &mut x) would NOT compile.
}
```

## Conflicto entre &T y &mut T

No se puede tener una referencia compartida `&T` y una
referencia mutable `&mut T` al mismo dato al mismo tiempo:

```rust
fn main() {
    let mut s = String::from("hello");

    let r1 = &s;        // shared borrow — OK
    let r2 = &s;        // another shared borrow — OK
    let r3 = &mut s;    // mutable borrow — ERROR

    println!("{}", r1);
}

// error[E0502]: cannot borrow `s` as mutable because it is
//               also borrowed as immutable
//  --> src/main.rs:6:14
//   |
// 4 |     let r1 = &s;
//   |              -- immutable borrow occurs here
// 6 |     let r3 = &mut s;
//   |              ^^^^^^ mutable borrow occurs here
// 8 |     println!("{}", r1);
//   |                    -- immutable borrow later used here
```

```rust
// The conflict exists because &T promises the data won't change.
// If &mut T existed at the same time, it could change the data,
// violating the promise made to &T holders.
//
// Coexistence rules:
//
//   &T  + &T       → OK    (multiple readers, no writer)
//   &T  + &mut T   → ERROR (reader + writer = potential data race)
//   &mut T + &mut T → ERROR (two writers = potential data race)
//   &mut T alone    → OK    (single writer, no readers)
```

## Non-Lexical Lifetimes (NLL)

Desde Rust 2018, el compilador usa **Non-Lexical Lifetimes** (NLL).
La vida de una referencia termina en su **ultimo uso**, no al
final del scope (bloque `{}`). Esto permite codigo que Rust 2015
rechazaba:

```rust
// This code works in Rust 2018+ (NLL):

fn main() {
    let mut s = String::from("hello");

    let r1 = &s;
    let r2 = &s;
    println!("{} and {}", r1, r2);
    // r1 and r2 are no longer used after this point.
    // NLL determines their lifetimes end HERE.

    let r3 = &mut s;  // OK: r1 and r2 are already "dead"
    r3.push_str(" world");
    println!("{}", r3); // "hello world"
}

// In Rust 2015, this would fail because r1 and r2's lifetimes
// extended to the closing brace of main(), overlapping with r3.
```

```rust
// NLL still catches actual conflicts:

fn main() {
    let mut v = vec![1, 2, 3];

    let first = &v[0];  // shared borrow of v
    v.push(4);           // mutable borrow of v — ERROR
    println!("{}", first);
}

// error[E0502]: cannot borrow `v` as mutable because it is
//               also borrowed as immutable
//
// first is still alive at the println!, so the push() conflicts.
// This is intentional: push() might reallocate the Vec's buffer,
// invalidating the pointer that first holds.
// Rust prevents the use-after-free at compile time.
```

## Referencias mutables en funciones

El patron mas comun para `&mut T` es pasarlo como argumento
a una funcion que necesita modificar un dato in-place, sin
tomar ownership:

```rust
fn push_val(v: &mut Vec<i32>, val: i32) {
    v.push(val);
    // v is a mutable reference to the caller's Vec.
    // push() modifies the Vec in place.
    // The Vec is NOT moved — the caller retains ownership.
}

fn main() {
    let mut numbers = vec![1, 2, 3];
    push_val(&mut numbers, 4);
    push_val(&mut numbers, 5);
    println!("{:?}", numbers); // [1, 2, 3, 4, 5]
}
```

```rust
// A function can take multiple &mut references,
// as long as they refer to different data:

fn swap_values(a: &mut i32, b: &mut i32) {
    let tmp = *a;
    *a = *b;
    *b = tmp;
}

fn main() {
    let mut x = 10;
    let mut y = 20;
    swap_values(&mut x, &mut y); // OK: x and y are different
    println!("x={}, y={}", x, y); // x=20, y=10
}
```

## Reborrowing (adelanto)

Cuando una funcion espera `&mut T`, se puede pasar una
referencia mutable existente sin moverla. Rust crea un
**reborrow** implicito: una nueva referencia mutable temporal
derivada de la original. Esto se cubrira en detalle en T04,
pero conviene saber que existe:

```rust
fn increment(val: &mut i32) {
    *val += 1;
}

fn main() {
    let mut x = 0;
    let r = &mut x;

    increment(r);  // reborrow: r is NOT moved
    increment(r);  // r can be used again
    increment(r);

    println!("{}", r); // 3
}

// When you call increment(r), Rust implicitly creates a
// temporary &mut i32 derived from r (a reborrow).
// While the reborrow is alive (during the function call),
// r cannot be used. But once increment() returns, r is
// usable again.
//
// Without reborrowing, r would be moved into increment()
// on the first call, and the second call would fail.
// This topic is expanded in T04.
```

## Llamadas a metodos con &mut self

Muchos metodos de la libreria estandar toman `&mut self`.
Cuando escribes `v.push(5)`, Rust traduce esto a
`Vec::push(&mut v, 5)` automaticamente. El `.` realiza
un auto-borrow mutable:

```rust
fn main() {
    let mut v = vec![1, 2, 3];

    // These two lines are equivalent:
    v.push(4);                // method syntax (auto &mut self)
    Vec::push(&mut v, 5);    // function syntax (explicit &mut)

    println!("{:?}", v); // [1, 2, 3, 4, 5]
}

// The signature of Vec::push is:
//   pub fn push(&mut self, value: T)
//
// &mut self means the method borrows the Vec mutably.
// The dot operator automatically creates the &mut borrow.
```

```rust
// The auto-borrow can conflict with existing references:

fn main() {
    let mut v = vec![1, 2, 3];

    let first = &v[0];  // shared borrow of v
    v.push(4);           // push takes &mut self — ERROR
    println!("{}", first);

    // ERROR: cannot borrow `v` as mutable because it is
    //        also borrowed as immutable
    //
    // Fix: use first before calling push:
    //   let first = &v[0];
    //   println!("{}", first);  // last use of first
    //   v.push(4);              // now OK — first is dead (NLL)
}
```

---

## Ejercicios

### Ejercicio 1 — Modificacion basica

```rust
// Create a function increment_all that takes &mut Vec<i32>
// and adds 1 to every element.
//
// fn increment_all(v: &mut Vec<i32>) {
//     // your code here
// }
//
// fn main() {
//     let mut numbers = vec![10, 20, 30];
//     increment_all(&mut numbers);
//     println!("{:?}", numbers);
//     // Expected: [11, 21, 31]
//
//     increment_all(&mut numbers);
//     println!("{:?}", numbers);
//     // Expected: [12, 22, 32]
// }
//
// Verify:
//   1. The function modifies the Vec in place.
//   2. After the call, main still owns numbers.
//   3. You can call increment_all multiple times.
```

### Ejercicio 2 — Predecir errores del compilador

```rust
// For each snippet, predict whether it compiles or not.
// If it fails, identify the error code (E0499 or E0502)
// and explain why. Then verify with rustc.

// Snippet A:
// fn main() {
//     let mut x = 10;
//     let a = &mut x;
//     let b = &mut x;
//     *a += 1;
//     *b += 1;
// }

// Snippet B:
// fn main() {
//     let mut x = 10;
//     let a = &mut x;
//     *a += 1;
//     let b = &mut x;
//     *b += 1;
// }

// Snippet C:
// fn main() {
//     let mut s = String::from("hello");
//     let r = &s;
//     s.push_str(" world");
//     println!("{}", r);
// }

// Snippet D:
// fn main() {
//     let mut s = String::from("hello");
//     let r = &s;
//     println!("{}", r);
//     s.push_str(" world");
//     println!("{}", s);
// }
```

### Ejercicio 3 — Funcion con multiples &mut

```rust
// Write a function that takes two mutable references to i32
// and sets both to zero:
//
// fn zero_both(a: &mut i32, b: &mut i32) {
//     // your code here
// }
//
// fn main() {
//     let mut x = 42;
//     let mut y = 99;
//     zero_both(&mut x, &mut y);
//     assert_eq!(x, 0);
//     assert_eq!(y, 0);
//     println!("Both zeroed: x={}, y={}", x, y);
// }
//
// Then try calling: zero_both(&mut x, &mut x);
// Predict the error. Why does Rust prevent this?
// What could go wrong if it were allowed?
```
