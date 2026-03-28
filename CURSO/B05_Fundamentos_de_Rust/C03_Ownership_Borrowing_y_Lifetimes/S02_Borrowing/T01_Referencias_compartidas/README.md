# T01 — Referencias compartidas (&T)

## Que es borrowing

Borrowing es acceder a datos sin tomar ownership. El propietario
original conserva la propiedad del valor. La referencia es un
prestamo: puedes leer el dato, pero no lo posees y no puedes
modificarlo.

```rust
// Analogia: prestar un libro.
// Tu amigo te presta un libro.
// Puedes leerlo, pero:
//   - no eres el duenio (no puedes venderlo)
//   - no puedes escribir en el (es inmutable para ti)
//   - cuando terminas, se lo devuelves (la referencia deja de existir)
//   - tu amigo sigue siendo el duenio todo el tiempo
```

```rust
fn main() {
    let book = String::from("The Rust Programming Language");

    // read_title toma prestado book — no toma ownership.
    read_title(&book);

    // book sigue siendo valido aqui porque solo fue prestado.
    println!("I still own: {}", book);
}

fn read_title(title: &String) {
    println!("Reading: {}", title);
}
// Cuando read_title termina, la referencia desaparece.
// Ningun recurso se libera porque la referencia no es propietaria.
```

## Crear referencias

Una referencia se crea con el operador `&`. Es un puntero
con garantias de seguridad verificadas en tiempo de compilacion:

```rust
fn main() {
    let x: i32 = 42;
    let r: &i32 = &x; // r es una referencia a x

    // En memoria:
    //  Stack:
    //  ┌──────┬───────┐
    //  │  x   │  42   │
    //  ├──────┼───────┤
    //  │  r   │  &x ──┼──→ apunta a x
    //  └──────┴───────┘
    //
    // r contiene la direccion de memoria de x.
    // A diferencia de C, el compilador garantiza que x sigue vivo
    // mientras r exista.

    println!("x = {}", x);
    println!("r = {}", r); // imprime 42, no la direccion
}
```

```rust
// Desreferenciar con *. Auto-deref con el operador . y metodos.
fn main() {
    let x = 10;
    let r = &x;

    let y = *r + 5;       // *r accede al valor a traves de la referencia
    println!("y = {}", y); // 15
    assert_eq!(*r, 10);

    // En la mayoria de casos no necesitas * explicitamente.
    // Rust aplica auto-deref en muchos contextos:
    println!("{}", r); // println! desreferencia automaticamente

    // Auto-deref con el operador . :
    let s = String::from("hello");
    let rs = &s;
    let len1 = rs.len();    // auto-deref: Rust convierte a (*rs).len()
    let len2 = (*rs).len(); // desreferencia explicita
    println!("{} {}", len1, len2); // 5 5

    // El operador . sigue cadenas de referencias:
    let rrs = &rs;   // &&String
    println!("{}", rrs.len()); // 5 — Rust desreferencia 2 veces
}
```

## Multiples referencias compartidas

Puedes tener tantas referencias `&T` como quieras al mismo
dato, simultaneamente. Todas son de solo lectura:

```rust
fn main() {
    let x = 42;

    let r1 = &x;
    let r2 = &x;
    let r3 = &x;

    // Tres referencias activas al mismo tiempo — perfectamente valido.
    println!("{} {} {}", r1, r2, r3); // 42 42 42

    // Seguro porque nadie puede modificar x mientras existan
    // las referencias. No hay data races: solo hay lectores.
}
```

```rust
// Regla de borrowing:
//
// En cualquier momento dado, puedes tener:
//   - N referencias compartidas (&T)  ← este topico
//   - O 1 referencia mutable (&mut T) ← siguiente topico
//   - Pero NO ambas al mismo tiempo.
//
// Multiples lectores: seguro.
// Un escritor exclusivo: seguro.
// Lectores + escritor simultaneos: prohibido.
```

## Referencias en funciones

Pasar referencias a funciones es el uso mas comun de borrowing.
La funcion toma prestado el dato, el caller conserva ownership:

```rust
fn calculate_length(s: &String) -> usize {
    s.len()
} // s sale de scope. Como no tiene ownership, nada se libera.

fn main() {
    let greeting = String::from("hello");
    let len = calculate_length(&greeting);

    // greeting sigue siendo valida:
    println!("'{}' has {} bytes", greeting, len);
    // 'hello' has 5 bytes
}
```

```rust
// Devolver una referencia — debe apuntar a datos que sobrevivan
// a la funcion:
fn first_word(s: &str) -> &str {
    let bytes = s.as_bytes();
    for (i, &byte) in bytes.iter().enumerate() {
        if byte == b' ' {
            return &s[..i];
        }
    }
    s
}

fn main() {
    let sentence = String::from("hello world");
    let word = first_word(&sentence);
    println!("First word: {}", word); // "hello"
    // word es una referencia a una porcion de sentence.
    // sentence debe seguir viva mientras word exista.
}
```

```rust
// Cuando el retorno puede venir de multiples parametros,
// el compilador necesita lifetime annotations:
//
// fn longest(a: &str, b: &str) -> &str {
//     if a.len() >= b.len() { a } else { b }
// }
// Error: missing lifetime specifier
//
// La solucion requiere lifetime annotations (otro topico):
//   fn longest<'a>(a: &'a str, b: &'a str) -> &'a str
```

```rust
// Patron comun: funciones que analizan sin modificar.
fn contains_word(text: &str, word: &str) -> bool {
    text.contains(word)
}

fn count_chars(s: &str) -> usize {
    s.chars().count()
}

fn main() {
    let text = String::from("hello world");
    println!("contains 'world': {}", contains_word(&text, "world"));
    println!("char count: {}", count_chars(&text));
    // text nunca fue movido — todas las funciones tomaron prestado.
}
```

## Las referencias son inmutables

A traves de una referencia compartida `&T` no se puede
modificar el dato al que apunta. Esta garantia permite
tener multiples lectores simultaneos de forma segura:

```rust
fn main() {
    let x = 10;
    let r = &x;

    // *r = 20;
    // Error: cannot assign to `*r`, which is behind a `&` reference

    println!("{}", r); // solo lectura — siempre funciona
}
```

```rust
fn try_to_modify(s: &String) {
    // s.push_str(" world");
    // Error: cannot borrow `*s` as mutable, as it is behind a `&` reference
    // push_str necesita &mut self, pero s es &String (inmutable).
}

fn main() {
    let greeting = String::from("hello");
    try_to_modify(&greeting);
    println!("{}", greeting); // "hello" — no cambio
}
```

```rust
// Por que la inmutabilidad hace seguro el acceso compartido:
//
//   let mut v = vec![1, 2, 3];
//   let r = &v[0];       // r apunta al primer elemento
//   v.push(4);           // push podria reasignar el buffer interno
//   println!("{}", r);   // r apuntaria a memoria liberada → crash
//
// Rust PREVIENE esto:
//   - Mientras exista r (&v[0]), v no puede ser modificado.
//   - v.push(4) falla: "cannot borrow `v` as mutable because it
//     is also borrowed as immutable".
//
// "Aliasing XOR mutation":
//   - Muchos alias (referencias compartidas), O
//   - Mutacion (una referencia mutable exclusiva),
//   - Nunca ambos a la vez.
```

```rust
fn main() {
    let mut v = vec![1, 2, 3];
    let first = &v[0]; // referencia compartida activa
    // v.push(4);       // Error: no se puede mutar mientras first exista

    println!("first = {}", first); // ultimo uso de first

    // Despues del ultimo uso, la referencia "muere"
    // (Non-Lexical Lifetimes — NLL). Ahora si podemos mutar:
    v.push(4); // OK — first ya no esta en uso
    println!("{:?}", v); // [1, 2, 3, 4]
}
```

## Tamanio de las referencias

Una referencia `&T` ocupa un machine word (tamanio de un puntero).
`&str` y `&[T]` son "fat pointers" que ocupan dos words:

```rust
use std::mem::size_of;

fn main() {
    // En una plataforma de 64 bits (8 bytes por word):

    // Referencias simples — un word (8 bytes):
    println!("&i32:    {} bytes", size_of::<&i32>());    // 8
    println!("&String: {} bytes", size_of::<&String>());  // 8

    // Fat pointers — dos words (16 bytes):
    println!("&str:    {} bytes", size_of::<&str>());    // 16
    println!("&[i32]:  {} bytes", size_of::<&[i32]>());  // 16

    // Fat pointer = puntero + longitud:
    //  ┌──────────┬──────────┐
    //  │ pointer  │ length   │
    //  │ (8 bytes)│ (8 bytes)│
    //  └──────────┴──────────┘
    //
    // &str  → puntero al primer byte + longitud en bytes
    // &[T]  → puntero al primer elemento + numero de elementos
    //
    // En C, char* no sabe la longitud. En Rust, &[u8] ya la
    // contiene — no se puede olvidar pasarla.
}
```

## Lifetime de las referencias

Una referencia no puede vivir mas que el dato al que apunta.
El compilador verifica esto en tiempo de compilacion, previniendo
dangling references (punteros colgantes):

```rust
// Dangling reference — el compilador lo rechaza:
//
// fn create_dangling() -> &String {
//     let s = String::from("hello");
//     &s
// }
// Error: cannot return reference to local variable `s`
//
// s se destruye al final de la funcion.
// Si se permitiera devolver &s, el caller tendria un puntero
// a memoria ya liberada — undefined behavior en C, error de
// compilacion en Rust.
```

```rust
fn main() {
    let r;
    {
        let x = 5;
        r = &x;
    }
    // println!("{}", r);
    // Error: `x` does not live long enough
    // r intenta referenciar x, pero x ya fue destruido.
}
```

```rust
// Solucion: el dato debe vivir al menos tanto como la referencia:
fn main() {
    let x = 5;       // x vive hasta el final de main
    let r = &x;      // r vive hasta el final de main
    println!("{}", r); // OK — x sigue vivo
}
```

```rust
// Comparacion con C — el clasico dangling pointer:
//
//   int* create_dangling(void) {
//       int x = 42;
//       return &x;  // warning, pero compila
//   }
//   int *p = create_dangling();
//   printf("%d\n", *p); // podria imprimir 42, basura, o crashear
//
// En Rust, esto es un error de compilacion — no un warning,
// no undefined behavior.
```

## Referencias vs raw pointers

Las referencias `&T` son la abstraccion segura de Rust sobre
punteros. Los raw pointers `*const T` y `*mut T` existen
pero requieren `unsafe`:

```rust
fn main() {
    let x = 42;
    let safe_ref: &i32 = &x;
    let raw_ptr: *const i32 = &x as *const i32;

    println!("ref: {}", safe_ref);               // OK
    println!("raw: {}", unsafe { *raw_ptr });     // requiere unsafe
}
```

```rust
// Garantias de &T que *const T no tiene:
//
// 1. Non-null: &T nunca es null.
// 2. Aligned: apunta a memoria correctamente alineada.
// 3. Valid: apunta a un valor valido de tipo T.
// 4. Aliasing: &T garantiza que nadie modifica el dato.
//    &mut T garantiza acceso exclusivo.
//    Esto permite optimizaciones similares a restrict en C99.
//
// Analogia con C:
//   Rust &T       ≈  C const T *restrict (verificado en compilacion)
//   Rust &mut T   ≈  C T *restrict       (verificado en compilacion)
//   Rust *const T ≈  C const T *         (sin garantias)
//   Rust *mut T   ≈  C T *              (sin garantias)
//
// En C, restrict es una promesa del programador.
// En Rust, las reglas de aliasing se verifican en compilacion.
```

## Coercion y auto-ref

Rust convierte automaticamente entre ciertos tipos de referencias.
Esto se llama Deref coercion:

```rust
fn print_str(s: &str) {
    println!("{}", s);
}

fn main() {
    let s = String::from("hello");

    // &String se convierte automaticamente a &str
    // porque String implementa Deref<Target = str>:
    print_str(&s);        // &String → &str (coercion automatica)
    print_str("literal"); // &str directamente

    // Cadenas de coercion:
    let boxed = Box::new(String::from("world"));
    print_str(&boxed); // &Box<String> → &String → &str

    // Vec<T> → &[T]:
    let v = vec![1, 2, 3];
    let slice: &[i32] = &v; // &Vec<i32> → &[i32]
    println!("{:?}", slice);
}
```

```rust
// Auto-ref en llamadas a metodos:
fn main() {
    let s = String::from("hello");

    // Estas llamadas son equivalentes:
    let len1 = s.len();      // Rust inserta: (&s).len()
    let len2 = (&s).len();   // referencia explicita
    let len3 = (&&s).len();  // Rust desreferencia hasta encontrar el metodo

    assert_eq!(len1, len2);
    assert_eq!(len2, len3);

    // El operador . automaticamente:
    // 1. Agrega & o &mut si es necesario (auto-ref)
    // 2. Desreferencia tantas veces como necesite (auto-deref)
}
```

```rust
// Coerciones comunes:
//
// &String    → &str        (String: Deref<Target = str>)
// &Vec<T>    → &[T]        (Vec<T>: Deref<Target = [T]>)
// &Box<T>    → &T          (Box<T>: Deref<Target = T>)
// &Arc<T>    → &T          (Arc<T>: Deref<Target = T>)
// &Rc<T>     → &T          (Rc<T>: Deref<Target = T>)
// &PathBuf   → &Path       (PathBuf: Deref<Target = Path>)
//
// Patron consistente: el tipo owned implementa Deref
// hacia su tipo slice/borrowed.
```

## Patrones practicos

### Preferir &str sobre &String

Cuando una funcion solo necesita leer texto, aceptar `&str`
es mas flexible que `&String`:

```rust
// MAL — solo acepta &String:
fn greet_string(name: &String) {
    println!("Hello, {}!", name);
}

// BIEN — acepta &str, &String, y string literals:
fn greet_str(name: &str) {
    println!("Hello, {}!", name);
}

fn main() {
    let owned = String::from("Alice");

    greet_str(&owned);     // &String → &str (Deref coercion)
    greet_str("Bob");      // &str directamente

    greet_string(&owned);  // funciona
    // greet_string("Bob"); // Error: expected &String, found &str
}
```

### Preferir &[T] sobre &Vec\<T\>

```rust
// MAL — solo acepta &Vec<i32>:
fn sum_vec(numbers: &Vec<i32>) -> i32 {
    numbers.iter().sum()
}

// BIEN — acepta &Vec<i32>, &[i32], arrays, etc.:
fn sum_slice(numbers: &[i32]) -> i32 {
    numbers.iter().sum()
}

fn main() {
    let v = vec![1, 2, 3, 4, 5];
    let arr = [10, 20, 30];

    println!("{}", sum_slice(&v));       // &Vec<i32> → &[i32]
    println!("{}", sum_slice(&arr));     // &[i32; 3] → &[i32]
    println!("{}", sum_slice(&v[1..4])); // slice directo
}
```

### Referencia a campos de un struct

```rust
struct Config {
    hostname: String,
    port: u16,
    database: String,
}

impl Config {
    fn hostname(&self) -> &str {
        &self.hostname
    }

    fn database(&self) -> &str {
        &self.database
    }
    // &self es azucar sintactica para self: &Config.
    // El metodo toma prestado el struct, no lo consume.
}

fn main() {
    let config = Config {
        hostname: String::from("localhost"),
        port: 5432,
        database: String::from("mydb"),
    };

    // Los metodos devuelven referencias — no clonan los String:
    println!("{}:{}/{}", config.hostname(), config.port, config.database());
}
```

### Resumen de buenas practicas

```rust
// Reglas generales para referencias compartidas:
//
// 1. Aceptar &str en vez de &String.
// 2. Aceptar &[T] en vez de &Vec<T>.
// 3. Usar referencias para funciones que solo leen datos.
// 4. Solo tomar ownership (T en vez de &T) cuando la funcion
//    necesita almacenar o modificar el dato.
// 5. Devolver referencias cuando el dato referenciado vive
//    suficiente tiempo (campos de structs, slices del input).
// 6. Devolver valores owned cuando la funcion crea datos nuevos.
```

---

## Ejercicios

### Ejercicio 1 — Funciones con referencias

```rust
// Implementar las siguientes funciones que toman prestado sus datos:
//
//   fn longest_str(a: &str, b: &str) -> &str
//     → devuelve el string mas largo (por bytes).
//       Si tienen la misma longitud, devolver a.
//       Nota: necesitaras lifetime annotations.
//
//   fn word_count(text: &str) -> usize
//     → cuenta las palabras separadas por espacios.
//       Usar split_whitespace().
//
//   fn starts_with_upper(s: &str) -> bool
//     → devuelve true si el primer caracter es mayuscula.
//       Usar s.chars().next() y char::is_uppercase().
//
// Verificar:
//   1. Llamar cada funcion multiples veces con el mismo &String.
//   2. Verificar que el String original sigue disponible despues.
//   3. Probar con string literals (&str) y con &String.
```

### Ejercicio 2 — Multiples referencias simultaneas

```rust
// Crear un struct Book con campos title: String y pages: u32.
//
// Implementar estas funciones (todas toman &[Book]):
//
//   fn total_pages(books: &[Book]) -> u32
//     → suma de todas las paginas.
//
//   fn longest_title(books: &[Book]) -> Option<&str>
//     → titulo mas largo (por caracteres).
//       Devolver None si el slice esta vacio.
//
//   fn books_over(books: &[Book], min_pages: u32) -> Vec<&str>
//     → titulos de libros con mas de min_pages paginas.
//
// Verificar:
//   1. Crear un Vec<Book> con al menos 4 libros.
//   2. Llamar las tres funciones sobre el mismo &[Book].
//   3. Verificar que el Vec<Book> sigue accesible despues de las llamadas.
//   4. Verificar que las funciones aceptan &[Book], no solo &Vec<Book>.
```

### Ejercicio 3 — Diagnostico de errores del compilador

```rust
// Para cada fragmento, predecir el error del compilador.
// Luego compilar para verificar. Explicar POR QUE falla.
//
// Fragmento A:
//   fn dangle() -> &String {
//       let s = String::from("hello");
//       &s
//   }
//
// Fragmento B:
//   fn main() {
//       let r;
//       {
//           let x = 42;
//           r = &x;
//       }
//       println!("{}", r);
//   }
//
// Fragmento C:
//   fn main() {
//       let mut v = vec![1, 2, 3];
//       let first = &v[0];
//       v.push(4);
//       println!("{}", first);
//   }
//
// Para cada fragmento:
//   1. Escribir la prediccion del error ANTES de compilar.
//   2. Compilar con rustc y comparar con la prediccion.
//   3. Escribir la version corregida que compila.
```
