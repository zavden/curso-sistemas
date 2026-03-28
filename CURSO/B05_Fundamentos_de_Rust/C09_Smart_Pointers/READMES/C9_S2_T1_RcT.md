# T01 — Rc\<T\>

## El problema: ownership compartido

En Rust, cada valor tiene **un solo dueño**. Pero hay situaciones
donde multiples partes del programa necesitan leer el mismo dato
sin que ninguna lo "posea" exclusivamente:

```rust
// Esto no compila — ownership exclusivo:
let data = vec![1, 2, 3];
let a = data;  // data se mueve a 'a'
let b = data;  // error: value used after move

// Opciones sin Rc:
// 1. Clone — copia los datos (costo de memoria y tiempo)
// 2. Referencias — pero el dato debe vivir mas que todas las refs
//    (a veces no puedes garantizar eso)
```

```text
¿Cuando necesitas ownership compartido?

  - Grafos: un nodo puede tener multiples padres
  - Cachés: multiples consumidores leen el mismo dato cacheado
  - Observers/callbacks: multiples handlers referencian datos compartidos
  - ASTs/IRs: un nodo del arbol es referenciado desde multiples lugares

En todos estos casos, no hay un unico "dueño" natural.
```

---

## Que es Rc\<T\>

`Rc<T>` (Reference Counted) es un smart pointer que permite
**multiples dueños** del mismo dato. Mantiene un contador de
cuantos Rc apuntan al valor. Cuando el ultimo Rc se destruye,
el dato se libera:

```rust
use std::rc::Rc;

let a = Rc::new(vec![1, 2, 3]);  // counter = 1
let b = Rc::clone(&a);            // counter = 2 (NO clona los datos)
let c = Rc::clone(&a);            // counter = 3

println!("{:?}", a);  // [1, 2, 3]
println!("{:?}", b);  // [1, 2, 3] — mismo dato
println!("{:?}", c);  // [1, 2, 3] — mismo dato

// Verificar el contador:
println!("count: {}", Rc::strong_count(&a));  // 3

drop(c);  // counter = 2
drop(b);  // counter = 1
drop(a);  // counter = 0 → el Vec se libera
```

```text
Layout en memoria:

  Rc::new(vec![1, 2, 3]) crea:

  Stack:          Heap (bloque de Rc):
  ┌─────────┐    ┌──────────────────────────┐
  │ a: ptr ──│───▶│ strong_count: 1           │
  └─────────┘    │ weak_count: 1 (interno)   │
  ┌─────────┐    │ data: Vec { ptr, len, cap}│──▶ [1, 2, 3]
  │ b: ptr ──│───▶│                            │
  └─────────┘    └──────────────────────────┘
  ┌─────────┐         ▲
  │ c: ptr ──│─────────┘
  └─────────┘

  Los tres Rc (a, b, c) apuntan al MISMO bloque en el heap.
  Solo hay UNA copia del Vec.
```

---

## Rc::clone vs .clone()

`Rc::clone(&rc)` **no clona los datos** — solo incrementa
el contador de referencias. Es O(1) y no aloca memoria:

```rust
use std::rc::Rc;

let original = Rc::new(String::from("hello"));

// Rc::clone — SOLO incrementa el contador (barato):
let shared = Rc::clone(&original);  // O(1), no copia "hello"

// .clone() en Rc hace LO MISMO que Rc::clone():
let also_shared = original.clone();  // identico a Rc::clone(&original)

// CONVENCION: usar Rc::clone(&x) en vez de x.clone()
// para dejar claro que NO estas clonando los datos.
// Si ves x.clone() en un Rc, alguien podria pensar que
// esta clonando el String internamente — Rc::clone() es mas explicito.
```

```rust
// Verificar que es la misma alocacion:
use std::rc::Rc;

let a = Rc::new(42);
let b = Rc::clone(&a);

// Ambos apuntan a la misma direccion:
assert!(Rc::ptr_eq(&a, &b));

// strong_count refleja cuantos Rc existen:
assert_eq!(Rc::strong_count(&a), 2);
assert_eq!(Rc::strong_count(&b), 2);  // misma cuenta — mismo bloque
```

---

## Rc es solo lectura

`Rc<T>` solo provee acceso de lectura (`&T`) al dato interno.
No puedes obtener `&mut T` porque multiples Rc comparten el dato
— mutarlo violaria las reglas de borrowing:

```rust
use std::rc::Rc;

let a = Rc::new(vec![1, 2, 3]);
let b = Rc::clone(&a);

// Leer — OK:
println!("len: {}", a.len());  // Deref: Rc<Vec> → Vec → len()
println!("first: {}", a[0]);   // Deref: Rc<Vec> → Vec → Index

// Mutar — ERROR:
// a.push(4);  // error: cannot borrow as mutable
// Rc<Vec<i32>> implementa Deref, no DerefMut.
// No hay forma de obtener &mut Vec<i32> desde un Rc.
```

```text
¿Por que Rc es immutable?

  Rc<T> puede tener multiples clones activos simultaneamente.
  Si uno pudiera mutar el dato, los otros lo verian cambiar
  sin aviso — esto viola la regla de Rust:

    "Multiples lectores (&T) O un escritor (&mut T), nunca ambos"

  Rc tiene multiples lectores → no puede haber escritor.

  ¿Necesitas Rc + mutabilidad? Usa Rc<RefCell<T>> (tema de S03).
```

### Rc::make_mut — clone-on-write

```rust
use std::rc::Rc;

let mut a = Rc::new(vec![1, 2, 3]);
let b = Rc::clone(&a);

// Rc::make_mut clona los datos SI hay mas de un Rc:
Rc::make_mut(&mut a).push(4);
// Como b tambien apuntaba al dato, Rc::make_mut:
// 1. Clona el Vec (a obtiene su propia copia)
// 2. Luego ejecuta push(4) en la copia

assert_eq!(*a, vec![1, 2, 3, 4]);  // a tiene la version modificada
assert_eq!(*b, vec![1, 2, 3]);     // b sigue con la original

// Si a fuera el unico Rc (strong_count == 1):
drop(b);
Rc::make_mut(&mut a).push(5);
// No clona — modifica in-place (ya es el unico dueño)
assert_eq!(*a, vec![1, 2, 3, 4, 5]);
```

---

## Rc::strong_count

```rust
use std::rc::Rc;

let a = Rc::new("hello");
assert_eq!(Rc::strong_count(&a), 1);

let b = Rc::clone(&a);
assert_eq!(Rc::strong_count(&a), 2);
assert_eq!(Rc::strong_count(&b), 2);  // ambos ven la misma cuenta

{
    let c = Rc::clone(&a);
    assert_eq!(Rc::strong_count(&a), 3);
}  // c se destruye → counter baja a 2
assert_eq!(Rc::strong_count(&a), 2);

drop(b);  // counter baja a 1
assert_eq!(Rc::strong_count(&a), 1);

drop(a);  // counter llega a 0 → el dato se libera
```

```rust
// strong_count es util para debugging, no para logica de negocio.
// Si tu codigo depende del valor de strong_count para decidir
// que hacer, probablemente hay un diseño mas claro.
```

---

## Ejemplo: grafo con nodos compartidos

```rust
use std::rc::Rc;

#[derive(Debug)]
struct Node {
    value: i32,
    children: Vec<Rc<Node>>,
}

impl Node {
    fn new(value: i32) -> Rc<Node> {
        Rc::new(Node { value, children: vec![] })
    }

    fn with_children(value: i32, children: Vec<Rc<Node>>) -> Rc<Node> {
        Rc::new(Node { value, children })
    }
}

fn main() {
    // Nodo compartido — referenciado por dos padres:
    let shared = Node::new(3);

    //     1       2
    //      \     /
    //        3       ← nodo compartido
    let node1 = Node::with_children(1, vec![Rc::clone(&shared)]);
    let node2 = Node::with_children(2, vec![Rc::clone(&shared)]);

    assert_eq!(Rc::strong_count(&shared), 3);  // shared + node1 + node2

    println!("node1 children: {:?}", node1.children[0].value);  // 3
    println!("node2 children: {:?}", node2.children[0].value);  // 3
    // Ambos apuntan al MISMO nodo 3
}
```

## Ejemplo: datos compartidos en una cache

```rust
use std::rc::Rc;
use std::collections::HashMap;

struct ImageCache {
    cache: HashMap<String, Rc<Vec<u8>>>,
}

impl ImageCache {
    fn new() -> Self {
        ImageCache { cache: HashMap::new() }
    }

    fn get_or_load(&mut self, path: &str) -> Rc<Vec<u8>> {
        if let Some(data) = self.cache.get(path) {
            // Ya cacheado — retornar un Rc que comparte los datos:
            Rc::clone(data)
        } else {
            // Simular carga:
            let data = Rc::new(vec![0u8; 1024]);  // "cargar" imagen
            self.cache.insert(path.to_string(), Rc::clone(&data));
            data
        }
    }
}

fn main() {
    let mut cache = ImageCache::new();

    let img1 = cache.get_or_load("photo.png");
    let img2 = cache.get_or_load("photo.png");  // misma imagen

    // img1 e img2 comparten los mismos bytes — sin duplicacion:
    assert!(Rc::ptr_eq(&img1, &img2));
    println!("refs: {}", Rc::strong_count(&img1));  // 3 (cache + img1 + img2)
}
```

---

## Rc NO es thread-safe

`Rc<T>` usa un contador **no atomico**. No puedes enviar un Rc
entre threads:

```rust
use std::rc::Rc;

let data = Rc::new(42);

// std::thread::spawn(move || {
//     println!("{data}");
// });
// error: `Rc<i32>` cannot be sent between threads safely
// Rc no implementa Send

// Para multi-thread, usa Arc<T> (Atomic Reference Counted).
// Arc es identico a Rc pero con contadores atomicos (mas lento).
// (siguiente topico)
```

```text
¿Por que Rc no es atomico?

  Incrementar/decrementar un contador atomico es mas lento
  (~10-100x) que un contador normal. La mayoria del codigo
  es single-thread, asi que Rc evita ese costo.

  Rc → single-thread, rapido
  Arc → multi-thread, mas lento (atomico)

  Rust te obliga a elegir: si intentas usar Rc en multi-thread,
  el compilador te detiene con un error de Send/Sync.
```

---

## Rc y ciclos — memory leaks

`Rc` puede causar memory leaks si creas ciclos de referencias.
El contador nunca llega a 0 porque cada Rc mantiene vivo al otro:

```rust
use std::rc::Rc;
use std::cell::RefCell;

#[derive(Debug)]
struct Node {
    value: i32,
    next: RefCell<Option<Rc<Node>>>,
}

fn main() {
    let a = Rc::new(Node { value: 1, next: RefCell::new(None) });
    let b = Rc::new(Node { value: 2, next: RefCell::new(None) });

    // Crear ciclo: a → b → a
    *a.next.borrow_mut() = Some(Rc::clone(&b));
    *b.next.borrow_mut() = Some(Rc::clone(&a));

    // a tiene strong_count = 2 (variable a + b.next)
    // b tiene strong_count = 2 (variable b + a.next)

    // Cuando a y b salen de scope:
    // a.strong_count baja a 1 (b.next aun lo referencia)
    // b.strong_count baja a 1 (a.next aun lo referencia)
    // Ninguno llega a 0 → MEMORY LEAK
}
// Los nodos NUNCA se liberan — Rust no tiene garbage collector.
```

```text
¿Como evitar ciclos?

  1. Weak<T> — referencias debiles que no incrementan el contador
     (siguiente topico: T03_WeakT)

  2. Diseño sin ciclos — usar indices en vez de referencias:
     Vec<Node> donde cada nodo referencia otros por indice (usize)

  3. Arena pattern — allocar todos los nodos en un Vec y dejarlos
     vivir hasta que el Vec entero se destruya
```

---

## Rc::try_unwrap — extraer el valor

```rust
use std::rc::Rc;

// Si strong_count == 1, puedes extraer el valor:
let a = Rc::new(String::from("hello"));
let s: String = Rc::try_unwrap(a).unwrap();  // OK: count era 1
assert_eq!(s, "hello");

// Si strong_count > 1, retorna Err con el Rc original:
let a = Rc::new(String::from("hello"));
let b = Rc::clone(&a);
let result = Rc::try_unwrap(a);
assert!(result.is_err());  // no puede extraer — b aun existe

// Util para "recuperar" ownership cuando sabes que eres el ultimo:
fn process_and_extract(data: Rc<Vec<i32>>) -> Option<Vec<i32>> {
    Rc::try_unwrap(data).ok()  // Some si eramos el ultimo, None si no
}
```

---

## Errores comunes

```rust
// ERROR 1: intentar mutar a traves de Rc
use std::rc::Rc;

let data = Rc::new(vec![1, 2, 3]);
// data.push(4);  // error: cannot borrow as mutable
// Rc solo da &T, nunca &mut T.
// Solucion: Rc<RefCell<Vec<i32>>> (tema S03)
```

```rust
// ERROR 2: clonar datos cuando querias compartir
use std::rc::Rc;

let original = Rc::new(vec![1, 2, 3]);

// MAL — esto clona el Vec completo (ineficiente):
let copy = Rc::new((*original).clone());
// copy es un Rc NUEVO con un Vec NUEVO

// BIEN — compartir el mismo dato:
let shared = Rc::clone(&original);
// shared apunta al MISMO Vec que original
```

```rust
// ERROR 3: crear ciclos sin darse cuenta
use std::rc::Rc;
use std::cell::RefCell;

struct Parent {
    children: Vec<Rc<Child>>,
}

struct Child {
    parent: Rc<Parent>,  // ciclo: Parent → Child → Parent
}

// Solucion: el hijo usa Weak<Parent> en vez de Rc<Parent>
// (ver T03_WeakT)
```

```rust
// ERROR 4: usar Rc cuando una referencia es suficiente
// MAL — Rc innecesario:
fn process(data: Rc<Vec<i32>>) {
    println!("{:?}", data);
}
let v = Rc::new(vec![1, 2, 3]);
process(Rc::clone(&v));

// BIEN — referencia simple:
fn process(data: &[i32]) {
    println!("{:?}", data);
}
let v = vec![1, 2, 3];
process(&v);

// Rc es para cuando NECESITAS ownership compartido.
// Si solo necesitas leer temporalmente, usa &T.
```

```rust
// ERROR 5: usar Rc en multi-thread
use std::rc::Rc;

let data = Rc::new(42);
// std::thread::spawn(move || println!("{data}"));
// error: Rc<i32> cannot be sent between threads safely

// Solucion: usar Arc<T> para multi-thread
use std::sync::Arc;
let data = Arc::new(42);
std::thread::spawn(move || println!("{data}"));
```

---

## Cheatsheet

```text
Crear:
  Rc::new(value)             crea Rc con strong_count = 1
  Rc::clone(&rc)             incrementa strong_count (NO clona datos)

Consultar:
  Rc::strong_count(&rc)      numero de Rc activos
  Rc::weak_count(&rc)        numero de Weak activos
  Rc::ptr_eq(&a, &b)         ¿apuntan al mismo bloque?

Extraer:
  Rc::try_unwrap(rc)         extraer valor si count == 1 → Result<T, Rc<T>>
  Rc::make_mut(&mut rc)      clone-on-write si count > 1

Acceso:
  *rc                        Deref → &T (solo lectura)
  rc.method()                Deref coercion a metodos de T

Reglas clave:
  - Rc<T> da &T, nunca &mut T — es solo lectura
  - Rc::clone es O(1) — solo incrementa contador
  - Rc NO es thread-safe — usa Arc para multi-thread
  - Ciclos de Rc causan memory leaks — usa Weak para romperlos
  - Rc<RefCell<T>> para ownership compartido + mutabilidad

Arbol de decision:
  ¿Necesitas ownership compartido? ─No─▶ usa &T o Box<T>
              │
             Sí
              │
  ¿Es multi-thread? ─Sí─▶ Arc<T>
              │
             No
              │
          Rc<T>
```

---

## Ejercicios

### Ejercicio 1 — Compartir datos

```rust
use std::rc::Rc;

// Simula una playlist compartida entre usuarios:
//
// struct Playlist { songs: Vec<String> }
// struct User { name: String, playlist: Rc<Playlist> }
//
// a) Crea una Playlist con 3 canciones.
//    Crea 3 Users que compartan la MISMA playlist (via Rc).
//    Verifica con Rc::strong_count que el count es 4
//    (1 original + 3 usuarios).
//
// b) Dropea un usuario. ¿Cuanto es el strong_count ahora?
//    ¿La playlist sigue viva? ¿Por que?
//
// c) Dropea los 3 usuarios y el Rc original.
//    ¿Cuando se libera la Playlist?
//
// d) ¿Puedes modificar la playlist desde un User?
//    ¿Por que si o por que no?
```

### Ejercicio 2 — Grafo con nodos compartidos

```rust
use std::rc::Rc;

// Modela un DAG (grafo dirigido aciclico):
//
//     A
//    / \
//   B   C
//    \ /
//     D
//
// a) Crea un struct Node { value: char, deps: Vec<Rc<Node>> }
//
// b) Construye el DAG donde D es compartido por B y C.
//    ¿Cual es el strong_count de D?
//
// c) Implementa una funcion que recorra el grafo en
//    profundidad (DFS) e imprima los valores.
//    ¿Se imprime D una o dos veces? ¿Por que?
//
// d) ¿Podrias agregar un campo parent: Rc<Node> a cada nodo?
//    ¿Que problema causaria?
```

### Ejercicio 3 — Rc::try_unwrap

```rust
use std::rc::Rc;

// a) Crea un Rc<String> y verifica que Rc::try_unwrap
//    retorna Ok cuando el count es 1.
//
// b) Clona el Rc e intenta try_unwrap.
//    ¿Que retorna? ¿Por que?
//
// c) Implementa esta funcion:
//    fn extract_or_clone(rc: Rc<String>) -> String
//    Si el Rc es el unico (count == 1), extrae el String.
//    Si no, clona el String.
//    Pista: try_unwrap retorna Err(rc) con el Rc original.
//
// d) ¿En que situacion real usarias esta funcion?
```
