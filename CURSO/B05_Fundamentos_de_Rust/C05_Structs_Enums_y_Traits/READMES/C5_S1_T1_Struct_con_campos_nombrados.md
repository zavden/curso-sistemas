# T01 — Struct con campos nombrados

## Que es un struct

Un struct es un tipo de dato compuesto que agrupa valores
relacionados bajo un solo nombre. Cada valor dentro del struct
se llama **campo** (field) y tiene un nombre y un tipo. Es el
equivalente en Rust a las estructuras de C, pero con reglas
de ownership integradas.

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

// User       → nombre del tipo (CamelCase)
// name       → campo (snake_case)
// String     → tipo del campo
// La coma despues del ultimo campo es opcional, pero se usa por convencion.

// Convenciones de nombres:
//   Structs  → CamelCase:  User, HttpRequest, DatabaseConnection
//   Campos   → snake_case: first_name, created_at, is_active
//   Rust emite warnings si no se siguen estas convenciones.
```

## Definicion de un struct

La definicion declara el tipo, sus campos y sus tipos.
No reserva memoria: solo describe la forma de los datos.

```rust
struct Config {
    max_connections: u32,
    timeout_seconds: u64,
    verbose: bool,
    host: String,
    port: u16,
}

// Un struct puede contener otros structs:
struct Address {
    street: String,
    city: String,
    zip: String,
}

struct Employee {
    name: String,
    address: Address,    // Employee contiene un Address completo
    salary: f64,
}
// Acceso anidado: employee.address.city
```

## Instanciacion

Para crear una instancia, se especifica el nombre del tipo
seguido de llaves con los valores de cada campo:

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

fn main() {
    let user = User {
        name: String::from("Alice"),
        age: 30,
        active: true,
    };

    // Todos los campos son obligatorios.
    // No hay valores por defecto (a menos que implementes Default).

    // El orden de los campos no importa:
    let user2 = User {
        active: false,
        name: String::from("Bob"),
        age: 25,
    };

    // Error si falta un campo:
    // let user3 = User { name: String::from("X"), age: 1 };
    // error[E0063]: missing field `active` in initializer of `User`

    // Error si sobra un campo:
    // let user4 = User { name: String::from("X"), age: 1, active: true, email: String::from("x") };
    // error[E0560]: struct `User` has no field named `email`
}
```

## Acceso a campos

Se usa la **notacion de punto** para leer y modificar campos:

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

fn main() {
    let user = User {
        name: String::from("Alice"),
        age: 30,
        active: true,
    };

    println!("Name: {}", user.name);     // Name: Alice
    println!("Age: {}", user.age);       // Age: 30

    // Para modificar campos, la variable debe ser mut:
    let mut mutable_user = User {
        name: String::from("Bob"),
        age: 25,
        active: true,
    };

    mutable_user.age = 26;
    mutable_user.active = false;
    mutable_user.name = String::from("Bob Smith");

    // Rust no permite marcar campos individuales como mut.
    // Si la instancia es mut, TODOS los campos son mutables.
    // Si la instancia es inmutable, NINGUN campo es mutable.

    // Sin mut, modificar falla:
    // user.age = 31;
    // error[E0594]: cannot assign to `user.age`, as `user` is not
    //              declared as mutable
}
```

## Field init shorthand

Cuando el nombre de una variable coincide con el nombre de un
campo, se puede omitir la repeticion:

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

fn build_user(name: String, age: u32) -> User {
    // Sin shorthand — repetitivo:
    // User { name: name, age: age, active: true }

    // Con field init shorthand:
    User {
        name,          // equivale a name: name
        age,           // equivale a age: age
        active: true,  // no puede usar shorthand (valor literal, no variable)
    }
}

fn main() {
    let x = 3.0;
    let y = 4.0;

    struct Point { x: f64, y: f64 }

    let p = Point { x, y };        // shorthand
    let p = Point { x: x, y: y };  // equivalente explicito

    // Se puede mezclar shorthand con asignacion explicita:
    let z = 5.0;
    let p = Point { x: z, y };     // x usa valor explicito, y usa shorthand
}
```

## Struct update syntax

La sintaxis `..otra_instancia` crea un nuevo struct copiando
o moviendo los campos restantes de otra instancia:

```rust
struct User {
    name: String,
    age: u32,
    active: bool,
}

fn main() {
    let user1 = User {
        name: String::from("Alice"),
        age: 30,
        active: true,
    };

    // Crear user2 cambiando solo name, tomando el resto de user1:
    let user2 = User {
        name: String::from("Bob"),
        ..user1
    };
    // user2.age = 30, user2.active = true (tomados de user1)

    // ..user1 DEBE ir al final, despues de todos los campos explicitos.
}
```

```rust
// CUIDADO con ownership y struct update syntax:

struct User {
    name: String,    // String NO implementa Copy
    age: u32,        // u32 SI implementa Copy
    active: bool,    // bool SI implementa Copy
}

fn main() {
    let user1 = User {
        name: String::from("Alice"),
        age: 30,
        active: true,
    };

    // Si solo sobreescribimos age, name se MUEVE desde user1:
    let user2 = User {
        age: 25,
        ..user1
    };
    // name: movido (String no es Copy → se mueve)
    // active: copiado (bool es Copy → se copia)

    // user1.name ya no es valido:
    // println!("{}", user1.name);
    // error[E0382]: borrow of moved value: `user1.name`

    // Pero los campos Copy siguen validos:
    println!("{}", user1.age);    // 30 — OK
    println!("{}", user1.active); // true — OK

    // Cuando TODOS los campos tomados del original son Copy,
    // el original sigue siendo completamente valido.
    struct Point { x: f64, y: f64 }
    let p1 = Point { x: 1.0, y: 2.0 };
    let p2 = Point { x: 5.0, ..p1 };
    println!("p1: ({}, {})", p1.x, p1.y);  // OK — f64 es Copy
}
```

## Ownership de campos

Un struct es dueno de todos sus datos. Cuando sale de scope,
todos sus campos se dropean automaticamente:

```rust
struct User {
    name: String,
    age: u32,
}

fn main() {
    let user = User {
        name: String::from("Alice"),
        age: 30,
    };

    // Mover un campo fuera del struct:
    let name = user.name;
    // name ahora es dueno del String.
    // user.name ya no es valido:
    // println!("{}", user.name);
    // error[E0382]: borrow of partially moved value: `user`

    // user.age sigue valido (u32 es Copy):
    println!("{}", user.age);  // 30
}
// <- user sale de scope, se libera lo que quede por dropear.
```

```rust
// Para almacenar referencias en un struct, se necesitan lifetimes:

// NO compila:
// struct UserRef { name: &str, age: u32 }
// error[E0106]: missing lifetime specifier

// SI compila (lifetimes se cubren en C03):
struct UserRef<'a> {
    name: &'a str,
    age: u32,
}

// Por ahora, usa String para campos de texto en structs.
// Es mas sencillo y evita complicaciones con lifetimes.
```

## Printing de structs

Por defecto, `println!` no puede imprimir structs. Se necesita
derivar el trait `Debug` con `#[derive(Debug)]`:

```rust
#[derive(Debug)]
struct User {
    name: String,
    age: u32,
    active: bool,
}

fn main() {
    let user = User {
        name: String::from("Alice"),
        age: 30,
        active: true,
    };

    // {:?} — formato Debug en una linea:
    println!("{:?}", user);
    // User { name: "Alice", age: 30, active: true }

    // {:#?} — formato Debug pretty-print (multilinea):
    println!("{:#?}", user);
    // User {
    //     name: "Alice",
    //     age: 30,
    //     active: true,
    // }

    // dbg! macro — imprime a stderr con archivo y numero de linea:
    dbg!(&user);
    // [src/main.rs:18] &user = User { name: "Alice", ... }
    // Usa &user para no mover el valor.
}
```

```rust
// #[derive(Debug)] requiere que TODOS los campos implementen Debug.
// Los tipos primitivos, String, Vec, etc. ya lo implementan.

#[derive(Debug)]
struct Address { city: String, zip: String }

#[derive(Debug)]
struct Person {
    name: String,
    address: Address,    // Address debe ser Debug para que Person sea Debug
}

fn main() {
    let person = Person {
        name: String::from("Alice"),
        address: Address {
            city: String::from("Madrid"),
            zip: String::from("28001"),
        },
    };
    println!("{:#?}", person);
    // Person {
    //     name: "Alice",
    //     address: Address { city: "Madrid", zip: "28001" },
    // }
}
```

## Structs y layout en memoria

El compilador decide como disponer los campos en memoria.
Puede reordenarlos para optimizar alineamiento y reducir
padding (bytes de relleno):

```rust
// Alineamiento: cada tipo debe estar en una direccion de memoria
// que sea multiplo de su alineamiento.
//   u8/bool → 1    u16 → 2    u32/f32 → 4    u64/f64 → 8

struct Example {
    a: u8,     // 1 byte
    b: u32,    // 4 bytes, alineamiento 4
    c: u8,     // 1 byte
}

// Sin reordenar (layout de C):
//   [a:1][pad:3][b:4][c:1][pad:3] = 12 bytes
//
// Rust puede reordenar a: b, a, c
//   [b:4][a:1][c:1][pad:2] = 8 bytes
```

```rust
use std::mem;

// #[repr(C)] — fuerza layout compatible con C (sin reordenar).
// Util para FFI (interop con C).

#[repr(C)]
struct WithReprC { a: u8, b: u64, c: u8 }

struct WithoutReprC { a: u8, b: u64, c: u8 }

fn main() {
    println!("repr(C): size={}, align={}",
        mem::size_of::<WithReprC>(),     // 24
        mem::align_of::<WithReprC>(),    // 8
    );
    // [a:1][pad:7][b:8][c:1][pad:7] = 24 bytes

    println!("default: size={}, align={}",
        mem::size_of::<WithoutReprC>(),  // 16
        mem::align_of::<WithoutReprC>(), // 8
    );
    // Compilador reordena: [b:8][a:1][c:1][pad:6] = 16 bytes

    // Tamanos de tipos comunes:
    println!("bool:   {}", mem::size_of::<bool>());    // 1
    println!("u32:    {}", mem::size_of::<u32>());     // 4
    println!("u64:    {}", mem::size_of::<u64>());     // 8
    println!("f64:    {}", mem::size_of::<f64>());     // 8
    println!("char:   {}", mem::size_of::<char>());    // 4 (Unicode)
    println!("String: {}", mem::size_of::<String>());  // 24 (ptr+len+cap)
    println!("&str:   {}", mem::size_of::<&str>());    // 16 (ptr+len)
}
```

## Ejemplo completo

```rust
#[derive(Debug)]
struct Employee {
    name: String,
    department: String,
    salary: f64,
    active: bool,
}

fn create_employee(name: String, department: String, salary: f64) -> Employee {
    Employee { name, department, salary, active: true }
}

fn promote(employee: &Employee, raise: f64) -> Employee {
    Employee {
        salary: employee.salary + raise,
        name: employee.name.clone(),
        department: employee.department.clone(),
        ..*employee
    }
}

fn main() {
    let emp1 = create_employee(
        String::from("Alice"),
        String::from("Engineering"),
        75000.0,
    );

    println!("{:#?}", emp1);

    let emp2 = promote(&emp1, 5000.0);
    println!("Promoted: {} -> {:.2}", emp2.name, emp2.salary);
    // Promoted: Alice -> 80000.00

    // emp1 sigue valido porque usamos clone() en promote:
    println!("Original: {}", emp1.name);

    println!("Size of Employee: {} bytes", std::mem::size_of::<Employee>());
}
```

```bash
cargo run
# Employee {
#     name: "Alice",
#     department: "Engineering",
#     salary: 75000.0,
#     active: true,
# }
# Promoted: Alice -> 80000.00
# Original: Alice
# Size of Employee: 80 bytes
```

---

## Ejercicios

### Ejercicio 1 — Definir e instanciar un struct

```rust
// Definir un struct llamado Book con los campos:
//   title: String, author: String, pages: u32, available: bool
//
// En main:
// 1. Crear una instancia de Book con datos de tu eleccion.
// 2. Imprimir cada campo con println! usando notacion de punto.
// 3. Crear una segunda instancia con mut, modificar pages y available,
//    e imprimir los valores antes y despues de la modificacion.
//
// Verificar:
//   cargo run compila y muestra los datos correctamente.
//   Intentar modificar un campo sin mut y observar el error del compilador.
```

### Ejercicio 2 — Field init shorthand y funciones

```rust
// Crear un struct Rectangle con campos width: f64 y height: f64.
//
// 1. Escribir new_rectangle(width, height) -> Rectangle con field init shorthand.
// 2. Escribir area(rect: &Rectangle) -> f64 que calcule el area.
// 3. Escribir scale(rect: &Rectangle, factor: f64) -> Rectangle que devuelva
//    un nuevo Rectangle con dimensiones multiplicadas por factor.
// 4. En main, crear un rectangulo, imprimir su area, escalarlo por 2.0,
//    e imprimir el area del nuevo rectangulo.
//
// Verificar:
//   El area del rectangulo escalado es 4x la del original.
//   Derivar Debug y usar {:#?} para imprimir ambos rectangulos.
```

### Ejercicio 3 — Struct update syntax y ownership

```rust
// Definir un struct ServerConfig con:
//   host: String, port: u16, max_connections: u32, tls_enabled: bool
//
// 1. Crear una instancia default_config con valores base.
// 2. Usar struct update syntax para crear dev_config que solo cambie port.
// 3. Intentar acceder a default_config.host despues de crear dev_config.
//    Observar el error y entender por que ocurre (host es String → se mueve).
// 4. Corregir usando clone() en host y verificar que default_config sigue valido.
// 5. Crear un struct Point { x: f64, y: f64 } (todos los campos Copy).
//    Verificar que struct update syntax NO invalida el original.
//
// Verificar:
//   Documentar como comentarios: que campos se mueven y cuales se copian.
//   Usar std::mem::size_of para imprimir el tamano de ServerConfig.
```
