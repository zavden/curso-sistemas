# T03 — Reglas del borrow checker

## Las dos reglas fundamentales

El borrow checker de Rust aplica exactamente dos reglas en
tiempo de compilacion. No hay excepciones:

```
Regla 1: Puedes tener CUALQUIER CANTIDAD de referencias compartidas (&T)
Regla 2: Puedes tener EXACTAMENTE UNA referencia mutable (&mut T)

Nunca ambas al mismo tiempo.
```

```rust
// VALIDO: multiples &T
let data = vec![1, 2, 3];
let r1 = &data;
let r2 = &data;
println!("{r1:?} {r2:?}"); // OK: dos lectores simultaneos

// VALIDO: exactamente un &mut T
let mut data = vec![1, 2, 3];
let r = &mut data;
r.push(4); // OK: un solo escritor
```

```rust
// INVALIDO: &T y &mut T coexisten
let mut data = vec![1, 2, 3];
let r1 = &data;           // borrow compartido
let r2 = &mut data;       // borrow mutable
println!("{r1:?}");       // r1 todavia esta activo
// ERROR: cannot borrow `data` as mutable because it is
//        also borrowed as immutable

// INVALIDO: dos &mut T
let mut data = vec![1, 2, 3];
let r1 = &mut data;
let r2 = &mut data;
println!("{r1:?} {r2:?}");
// ERROR: cannot borrow `data` as mutable more than once at a time
```

Esto funciona como un **readers-writer lock en tiempo de compilacion**.
Un RWLock permite multiples lectores O un escritor, nunca ambos.
Rust aplica la misma logica pero sin costo en runtime: el compilador
lo verifica antes de generar codigo.

```
&T      = read lock   → multiples lectores permitidos
&mut T  = write lock  → un solo escritor, nadie mas lee

Diferencia: un RWLock verifica en runtime (con overhead).
El borrow checker verifica en compilacion (cero overhead).
```

## Por que existen estas reglas

Las reglas previenen tres categorias de bugs que en C y C++
causan vulnerabilidades y crashes silenciosos.

### Invalidacion de iteradores

```rust
// Rust rechaza esto en compilacion:
fn iterator_invalidation() {
    let mut v = vec![1, 2, 3];
    for x in &v {        // borrow compartido de v (el iterador)
        v.push(*x * 2);  // intento de borrow mutable de v
    }
    // ERROR: cannot borrow `v` as mutable because it is also
    //        borrowed as immutable
}
// Por que es peligroso: push() puede realocar el buffer del Vec.
// El iterador apuntaria al buffer viejo → use-after-free.
```

```c
// En C esto compila sin warnings y es undefined behavior:
int *buf = malloc(3 * sizeof(int));
size_t len = 3, cap = 3;
for (size_t i = 0; i < len; i++) {
    if (len + 1 > cap) {
        cap *= 2;
        buf = realloc(buf, cap * sizeof(int));
        // Si realloc movio la memoria, cualquier puntero
        // al buffer viejo es invalido.
    }
    buf[len++] = buf[i] * 2; // posible use-after-free
}
```

### Data races

```rust
// Las reglas del borrow checker se integran con Send/Sync:
// &T  implementa Sync → multiples hilos pueden leer
// &mut T es exclusivo → solo un hilo puede escribir
// No se puede tener &T y &mut T al mismo tiempo →
// lectores y escritores nunca se solapan → data races imposibles.
```

### Mutacion con alias

```rust
fn aliased_mutation() {
    let mut v = vec![1, 2, 3];
    let first = &v[0];  // referencia al primer elemento
    v.clear();           // borra todos los elementos
    println!("{first}"); // first apunta a memoria liberada
    // ERROR en Rust. En C++ esto compila y es undefined behavior.
}
```

## Errores clasicos del borrow checker

### Iterar y mutar una coleccion

```rust
// ERROR: iterar y mutar simultaneamente
fn filter_wrong(v: &mut Vec<i32>) {
    for (i, &x) in v.iter().enumerate() {
        if x < 0 {
            v.remove(i); // mutable borrow con iter() activo
        }
    }
}

// SOLUCION 1: retain (idiomatico)
fn filter_retain(v: &mut Vec<i32>) {
    v.retain(|&x| x >= 0);
}

// SOLUCION 2: indices en reversa (sin iterador activo)
fn filter_manual(v: &mut Vec<i32>) {
    let mut i = v.len();
    while i > 0 {
        i -= 1;
        if v[i] < 0 { v.remove(i); }
    }
    // No hay borrow activo: v[i] es una operacion puntual.
}
```

### Borrowing de campos de un struct

```rust
struct Player {
    name: String,
    health: i32,
}

// Esto SI compila — el compilador ve campos disjuntos:
fn heal_direct(player: &mut Player) {
    let name = &player.name;     // borrow compartido de player.name
    player.health += 10;         // borrow mutable de player.health
    println!("{name} healed to {}", player.health); // OK
}
```

```rust
// Pero falla cuando se usa a traves de metodos:
impl Player {
    fn name(&self) -> &str { &self.name }
    fn heal(&mut self, amount: i32) { self.health += amount; }
}

fn heal_method(player: &mut Player) {
    let name = player.name();  // &self → borrow de TODO player
    player.heal(10);           // &mut self → segundo borrow de TODO player
    println!("{name}");
    // ERROR: cannot borrow `*player` as mutable because it is also
    //        borrowed as immutable
}
// name() toma &self → borrow compartido de TODA la estructura.
// El compilador no puede ver dentro del metodo que solo toca self.name.

// SOLUCION: acceder a los campos directamente
fn heal_fixed(player: &mut Player) {
    player.health += 10;
    println!("{} healed to {}", player.name, player.health);
}
```

### Referencia dangling

```rust
// ERROR: retornar referencia a dato que se destruye
fn dangling() -> &str {
    let s = String::from("hello");
    &s  // s se destruye al salir → referencia dangling
    // ERROR: missing lifetime specifier
}

// SOLUCION 1: retornar el dato owned
fn not_dangling() -> String {
    String::from("hello")  // mueve al caller
}

// SOLUCION 2: retornar &'static str
fn static_str() -> &'static str {
    "hello"  // string literal → vive en el binario
}
```

```c
// En C esto compila y es undefined behavior:
char *dangling(void) {
    char buf[64];
    strcpy(buf, "hello");
    return buf;  // puntero al stack frame destruido
}
```

### Borrow que se extiende demasiado

```rust
fn extended_borrow() {
    let mut data = vec![1, 2, 3];
    let r = &data[0];
    println!("{r}");
    // Pre-NLL (Rust 2015): el borrow duraba hasta } → ERROR aqui abajo.
    // Post-NLL (Rust 2018+): el borrow termina en el println → OK.
    data.push(4);
}
```

## NLL (Non-Lexical Lifetimes)

Antes de Rust 2018, los lifetimes de las referencias se extendian
hasta el final del scope lexico (el cierre de llave `}`). NLL
cambio esto: ahora una referencia "muere" en su ultimo punto de
uso, no al final del scope.

### Antes y despues de NLL

```rust
// Rust 2015 (sin NLL): esto NO compilaba
fn pre_nll() {
    let mut map = std::collections::HashMap::new();
    map.insert("key", 1);

    let value = map.get("key");  // borrow compartido
    println!("{value:?}");
    // Sin NLL: borrow dura hasta }. ERROR en la linea siguiente.
    // Con NLL: borrow termina aqui. OK.

    map.insert("other", 2);
}
```

```rust
// NLL entiende branches:
fn nll_branches(condition: bool) {
    let mut data = String::from("hello");
    let r = &data;
    if condition {
        println!("{r}");
    }
    // r ya no se usa en ninguna rama
    data.push_str(" world"); // OK con NLL
}
```

### Lo que NLL no resuelve

```rust
// NLL no puede ver dentro de funciones opacas:
fn nll_limitation(map: &mut std::collections::HashMap<String, Vec<i32>>) {
    let entry = map.get("key");  // borrow compartido
    match entry {
        Some(v) => println!("{v:?}"),
        None => {
            map.insert("key".to_string(), vec![]);
            // ERROR incluso con NLL: entry todavia esta en el match
        }
    }
}

// SOLUCION: usar la API entry, disenada para este patron
fn nll_entry_fix(map: &mut std::collections::HashMap<String, Vec<i32>>) {
    map.entry("key".to_string()).or_insert_with(Vec::new);
}
```

```rust
// NLL no ayuda con conflictos genuinos:
fn genuine_conflict() {
    let mut v = vec![1, 2, 3];
    let first = &v[0];   // borrow compartido
    v.push(4);            // push puede realocar → invalida first
    println!("{first}");  // first todavia se necesita
    // ERROR: conflicto real, no conservadurismo del compilador.
}
```

## Split borrowing

El compilador puede razonar sobre **partes disjuntas** de un dato.

### Campos de structs

```rust
struct Database {
    users: Vec<String>,
    logs: Vec<String>,
}

fn update_and_log(db: &mut Database) {
    db.logs.push(format!("User count: {}", db.users.len()));
    // OK: users y logs son campos disjuntos.
}
```

### Slices con split_at_mut

```rust
// El compilador NO puede probar que dos indices no se solapan:
fn two_elements() {
    let mut v = vec![1, 2, 3, 4, 5];
    let a = &mut v[0];
    let b = &mut v[1];
    // ERROR: en general los indices podrian ser variables iguales.
}

// SOLUCION: split_at_mut divide en dos sub-slices disjuntos
fn split_borrowing() {
    let mut v = vec![1, 2, 3, 4, 5];
    let (left, right) = v.split_at_mut(2);
    // left  = &mut [1, 2]      (indices 0..2)
    // right = &mut [3, 4, 5]   (indices 2..5)
    left[0] = 10;
    right[0] = 30;
    println!("{v:?}"); // [10, 2, 30, 4, 5]
}

// split_at_mut internamente usa unsafe para crear dos sub-slices
// a partir de un solo puntero. El unsafe esta encapsulado detras
// de una API segura que garantiza la particion.
```

```rust
// Otros metodos para split borrowing en slices:
fn slice_methods() {
    let mut data = vec![1, 2, 3, 4, 5];

    // split_first_mut: primer elemento + resto
    if let Some((first, rest)) = data.split_first_mut() {
        *first = rest.iter().sum();
    }

    // chunks_mut: sub-slices mutables de tamano fijo
    for chunk in data.chunks_mut(2) {
        chunk[0] *= 10;
    }
}
```

### Cuando split borrowing falla

```rust
// A traves de metodos, el compilador no puede ver los campos:
struct Game {
    world: World,
    renderer: Renderer,
}
struct World { entities: Vec<String> }
struct Renderer { buffer: Vec<u8> }

impl Game {
    fn world_mut(&mut self) -> &mut World { &mut self.world }
    fn renderer_mut(&mut self) -> &mut Renderer { &mut self.renderer }
}

fn update_wrong(game: &mut Game) {
    let world = game.world_mut();       // &mut self
    let renderer = game.renderer_mut(); // segundo &mut self
    // ERROR: dos borrows mutables de game a traves de &mut self.
}

// SOLUCION 1: acceso directo a campos
fn update_direct(game: &mut Game) {
    game.renderer.buffer.clear();
    game.world.entities.push("new".to_string());
}

// SOLUCION 2: metodo que retorna ambos campos
impl Game {
    fn world_and_renderer(&mut self) -> (&mut World, &mut Renderer) {
        (&mut self.world, &mut self.renderer)
    }
}

fn update_split(game: &mut Game) {
    let (world, renderer) = game.world_and_renderer();
    renderer.buffer.clear();
    world.entities.push("new".to_string());
}
```

## Luchando contra el borrow checker

Estrategias cuando el borrow checker rechaza codigo que parece
valido, ordenadas de mas idiomatica a menos.

### Reestructurar el codigo

```rust
// PROBLEMA: borrow compartido y mutable se solapan
fn problematic(v: &mut Vec<i32>) {
    let last = v.last().unwrap(); // &i32 — borrow compartido
    v.push(*last);                // borrow mutable
    // ERROR
}

// SOLUCION: copiar el valor antes de mutar
fn fixed(v: &mut Vec<i32>) {
    let last = *v.last().unwrap(); // copia el i32 (no referencia)
    v.push(last);                  // OK: no hay borrow activo
}
```

```rust
// PROBLEMA: iterar y acumular en el mismo Vec
fn process_same_vec(data: &mut Vec<String>) {
    // SOLUCION: recolectar primero, agregar despues
    let to_add: Vec<String> = data
        .iter()
        .filter(|s| s.starts_with("OK"))
        .cloned()
        .collect();
    data.extend(to_add);
}
```

### Clone para romper conflictos

```rust
fn clone_approach(map: &mut std::collections::HashMap<String, String>) {
    let keys: Vec<String> = map.keys().cloned().collect();
    for key in &keys {
        if let Some(value) = map.get(key) {
            let new_value = value.to_uppercase();
            map.insert(key.clone(), new_value);
        }
    }
}

// Cuando clone es aceptable:
// - Tipo pequeno (i32, bool, structs Copy)
// - No es hot path
// - La alternativa es unsafe o mucho mas compleja
//
// Cuando clone es problematico:
// - Tipos grandes (Vec<Vec<String>>, arboles)
// - En loops con millones de iteraciones
```

### Interior mutability: Cell y RefCell

Cuando el borrow checker es demasiado conservador, se puede mover
la verificacion al runtime.

```rust
use std::cell::RefCell;

// Caso tipico: cache dentro de una estructura "inmutable"
struct CachedSum {
    data: Vec<i32>,
    cache: RefCell<Option<i32>>,
}

impl CachedSum {
    fn sum(&self) -> i32 {
        // self es &self, pero podemos mutar cache
        let mut cached = self.cache.borrow_mut();
        if let Some(value) = *cached {
            return value;
        }
        let result: i32 = self.data.iter().sum();
        *cached = Some(result);
        result
    }
}
// borrow() → panic si hay un borrow_mut activo
// borrow_mut() → panic si hay cualquier otro borrow activo
// Mismas reglas del borrow checker, verificadas en runtime.
```

```rust
use std::cell::Cell;

// Cell es para tipos Copy — get/set copian el valor:
struct Counter {
    count: Cell<u32>,
}

impl Counter {
    fn increment(&self) {
        self.count.set(self.count.get() + 1);
        // &self, no &mut self — pero podemos mutar count
    }
}

// Cell<T>    — T: Copy. Sin overhead de runtime. No da referencias.
// RefCell<T> — Cualquier T. Overhead de runtime (contador de borrows).
//              Puede hacer panic si se violan las reglas.
```

```rust
// ADVERTENCIA: RefCell puede hacer panic
use std::cell::RefCell;

fn refcell_panic() {
    let data = RefCell::new(vec![1, 2, 3]);
    let r1 = data.borrow();     // OK
    let r2 = data.borrow_mut(); // PANIC: "already borrowed: BorrowMutError"
    // Prefiere el borrow checker estatico cuando sea posible.
}
```

## El borrow checker es tu amigo

Cada error del borrow checker corresponde a un bug real que
en C produce undefined behavior.

```rust
// Rust: error de compilacion
fn use_after_free_rust() {
    let r;
    {
        let s = String::from("hello");
        r = &s;
    } // s se destruye
    // println!("{r}"); // ERROR: `s` does not live long enough
}
```

```c
// C: compila, undefined behavior
char *r;
{
    char *s = malloc(6);
    strcpy(s, "hello");
    r = s;
    free(s);
}
printf("%s\n", r);  // use-after-free
```

```rust
// Rust: error de compilacion
fn buffer_invalidation() {
    let mut v = vec![1, 2, 3];
    let ptr = &v[0];
    for i in 0..100 { v.push(i); }
    // println!("{ptr}"); // ERROR
}
```

```c
// C: compila, undefined behavior
int *buf = malloc(3 * sizeof(int));
int *ptr = &buf[0];
for (int i = 0; i < 100; i++)
    buf = realloc(buf, (4 + i) * sizeof(int));
printf("%d\n", *ptr);  // ptr puede ser invalido
```

```
Bug en C/C++                  | Deteccion    | En Rust
------------------------------|--------------|---------------------------
Use-after-free                | Runtime/CVE  | Error de compilacion
Iterator invalidation         | Runtime/CVE  | Error de compilacion
Data race                     | Runtime/CVE  | Error de compilacion
Dangling pointer              | Runtime/CVE  | Error de compilacion
Buffer realloc invalidation   | Runtime/CVE  | Error de compilacion
Double mutable alias          | Logica/CVE   | Error de compilacion

Estos bugs representan ~70% de las vulnerabilidades de seguridad
en proyectos C/C++ grandes (dato de Microsoft y Google Chromium).
```

---

## Ejercicios

### Ejercicio 1 — Diagnosticar errores del borrow checker

```rust
// Para cada fragmento, predecir si compila o no.
// Si no compila, identificar que regla se viola y proponer una solucion.

// Fragmento A:
fn exercise_a() {
    let mut s = String::from("hello");
    let r1 = &s;
    let r2 = &s;
    println!("{r1} {r2}");
    s.push_str(" world");
    println!("{s}");
}

// Fragmento B:
fn exercise_b() {
    let mut v = vec![1, 2, 3];
    let first = &v[0];
    v.push(4);
    println!("{first}");
}

// Fragmento C:
fn exercise_c() {
    let mut s = String::from("hello");
    let r = &s;
    println!("{r}");
    s.push_str(" world");
}

// Fragmento D:
fn exercise_d() -> &'static str {
    let s = String::from("hello");
    let r: &str = &s;
    r
}

// Para cada uno: compila? Por que si o por que no?
// Si no compila, escribir la version corregida.
```

### Ejercicio 2 — Split borrowing

```rust
// Dado este struct:
struct Document {
    title: String,
    body: String,
    metadata: Vec<String>,
}

// Escribir una funcion que:
// 1. Lea title para generar un log entry
// 2. Modifique body (agregar un parrafo)
// 3. Agregue el log entry a metadata
//
// Implementar DOS versiones:
// a) Acceso directo a campos (debe compilar)
// b) A traves de metodos get_title(), get_body_mut(), add_metadata()
//    (analizar por que no compila y proponer solucion)
```

### Ejercicio 3 — De C inseguro a Rust seguro

```c
// Traducir este codigo C a Rust seguro:
char *find_longest(char **strings, int count) {
    char *longest = strings[0];
    for (int i = 1; i < count; i++) {
        if (strlen(strings[i]) > strlen(longest)) {
            longest = strings[i];
        }
    }
    return longest;
}

int main(void) {
    char *words[] = {"short", "medium length", "the longest string"};
    char *result = find_longest(words, 3);
    printf("Longest: %s\n", result);
    // Bug potencial: si words se libera antes de usar result,
    // result es un dangling pointer.
    return 0;
}

// En Rust: la firma de find_longest necesitara lifetime annotations.
// Escribir la version Rust y explicar por que los lifetimes hacen
// que el bug potencial sea imposible.
```
