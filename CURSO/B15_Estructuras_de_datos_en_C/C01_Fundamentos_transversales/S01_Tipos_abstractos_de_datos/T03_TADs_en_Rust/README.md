# T03 — TADs en Rust

---

## 1. Rust tiene TADs nativos

A diferencia de C (donde el patrón TAD se logra con convenciones: header +
struct opaco + prefijos), Rust tiene mecanismos del lenguaje que implementan
directamente los componentes de un TAD:

```
Componente del TAD      En C                      En Rust
────────────────────────────────────────────────────────────
Contrato (interfaz)     Header .h                 trait
Implementación          Archivo .c                impl block
Encapsulación           Struct opaco              Campos privados (default)
Namespace               Prefijo manual (set_)     Métodos con self
Destructor              xxx_destroy() manual      Drop automático
Genericidad             void * + callbacks        Generics <T> + trait bounds
Manejo de errores       Retorno int/-1/NULL       Option<T>, Result<T, E>
```

El resultado: menos código boilerplate, más garantías en compilación,
mismo nivel de abstracción.

---

## 2. Traits como contrato

Un trait en Rust es el equivalente directo a la interfaz de un TAD:
declara operaciones sin fijar la implementación.

### Definición del trait

```rust
pub trait Set<T> {
    /// Create an empty set.
    fn new() -> Self;

    /// Insert elem. Returns true if it was new, false if it already existed.
    fn insert(&mut self, elem: T) -> bool;

    /// Remove elem. Returns true if it existed.
    fn remove(&mut self, elem: T) -> bool;

    /// Check membership.
    fn contains(&self, elem: &T) -> bool;

    /// Number of elements.
    fn len(&self) -> usize;

    /// Check if empty.
    fn is_empty(&self) -> bool {
        self.len() == 0     // implementación por defecto
    }
}
```

### Lo que el trait comunica

| Elemento | Información |
|----------|-------------|
| `<T>` | Genérico — funciona con cualquier tipo |
| `fn new() -> Self` | Constructor que retorna la implementación concreta |
| `&mut self` | Modificador — necesita acceso mutable |
| `&self` | Observador — solo lectura |
| `elem: T` vs `elem: &T` | `insert` toma ownership, `contains` solo observa |
| `-> bool` | Retorna información sobre el resultado |
| `fn is_empty` con cuerpo | Implementación por defecto — las implementaciones pueden override |

### Comparación con el header de C

```c
// C — set.h
typedef struct Set Set;
Set  *set_create(void);
void  set_destroy(Set *s);
bool  set_insert(Set *s, int elem);
bool  set_remove(Set *s, int elem);
bool  set_contains(const Set *s, int elem);
size_t set_size(const Set *s);
bool  set_empty(const Set *s);
```

```rust
// Rust — trait Set<T>
pub trait Set<T> {
    fn new() -> Self;
    fn insert(&mut self, elem: T) -> bool;
    fn remove(&mut self, elem: T) -> bool;
    fn contains(&self, elem: &T) -> bool;
    fn len(&self) -> usize;
    fn is_empty(&self) -> bool { self.len() == 0 }
}
```

Diferencias clave:
- C: el "contrato" está en comentarios y convención. Nada impide escribir un
  `set_insert` que no cumpla el contrato.
- Rust: el trait **fuerza** la firma. Si una implementación no define `insert`
  con la firma exacta `(&mut self, T) -> bool`, no compila.

Pero: el trait no puede expresar invariantes ("sin duplicados") ni
postcondiciones ("después de insert, contains retorna true"). Eso sigue
siendo responsabilidad de tests y documentación.

---

## 3. Structs con campos privados

En Rust, los campos de un struct son **privados por defecto**:

```rust
// src/set.rs (o en un módulo)
pub struct VecSet<T> {
    data: Vec<T>,   // privado — nadie fuera del módulo puede acceder
}
```

Desde otro módulo:

```rust
use crate::set::VecSet;

fn main() {
    let s = VecSet::<i32>::new();

    // s.data;              // ERROR: field `data` of struct `VecSet` is private
    // s.data.push(42);     // ERROR: mismo motivo
    // s.data.len();        // ERROR

    s.len();                // OK — método público
    s.contains(&42);        // OK
}
```

Esto es el equivalente del struct opaco de C, pero con mejor ergonomía:
- No necesitas `typedef struct X X` + declaración adelantada
- Se puede alocar en stack (Rust conoce el tamaño aunque los campos sean privados)
- Los errores de compilación son claros: "field is private"

### Visibilidad granular

```rust
pub struct Config {
    pub name: String,           // público — cualquiera accede
    pub(crate) debug: bool,     // visible solo dentro del crate
    pub(super) internal: u32,   // visible solo en el módulo padre
    secret: Vec<u8>,            // privado — solo dentro de este módulo
}
```

Para TADs, la regla es simple: **todos los campos privados**. Solo se
exponen a través de métodos.

---

## 4. impl blocks — la implementación

El `impl` block es donde vive la implementación del TAD.

### impl directo (métodos inherentes)

```rust
pub struct VecSet<T> {
    data: Vec<T>,
}

impl<T: PartialEq> VecSet<T> {
    pub fn new() -> Self {
        VecSet { data: Vec::new() }
    }

    pub fn insert(&mut self, elem: T) -> bool {
        if self.contains(&elem) {
            return false;   // ya existe
        }
        self.data.push(elem);
        true
    }

    pub fn remove(&mut self, elem: &T) -> bool {
        if let Some(pos) = self.data.iter().position(|x| x == elem) {
            self.data.swap_remove(pos);   // O(1) — swap con último
            true
        } else {
            false
        }
    }

    pub fn contains(&self, elem: &T) -> bool {
        self.data.iter().any(|x| x == elem)
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }
}
```

### impl trait — implementar el contrato

```rust
impl<T: PartialEq> Set<T> for VecSet<T> {
    fn new() -> Self {
        VecSet { data: Vec::new() }
    }

    fn insert(&mut self, elem: T) -> bool {
        if self.contains(&elem) {
            return false;
        }
        self.data.push(elem);
        true
    }

    fn remove(&mut self, elem: T) -> bool {
        if let Some(pos) = self.data.iter().position(|x| x == &elem) {
            self.data.swap_remove(pos);
            true
        } else {
            false
        }
    }

    fn contains(&self, elem: &T) -> bool {
        self.data.iter().any(|x| x == elem)
    }

    fn len(&self) -> usize {
        self.data.len()
    }

    // is_empty usa la implementación por defecto del trait
}
```

### ¿Cuándo usar trait vs impl directo?

| Situación | Usar |
|-----------|------|
| Solo hay una implementación | `impl` directo — sin trait |
| Hay o habrá múltiples implementaciones | Trait + `impl Trait for X` |
| Quieres polimorfismo (aceptar cualquier Set) | Trait |
| API interna de un módulo | `impl` directo |
| Biblioteca pública donde el usuario puede implementar | Trait |

Para las estructuras de este curso: usaremos `impl` directo para la
implementación principal y traits cuando haya múltiples implementaciones
del mismo TAD (ej: Set con Vec vs Set con HashSet vs Set con BTreeSet).

---

## 5. self, &self, &mut self

El primer parámetro de un método determina cómo se accede al TAD:

```rust
impl<T: PartialEq> VecSet<T> {
    // Constructor — no tiene self, es una "associated function"
    pub fn new() -> Self { ... }

    // Observador — &self = referencia inmutable
    pub fn contains(&self, elem: &T) -> bool { ... }
    pub fn len(&self) -> usize { ... }

    // Modificador — &mut self = referencia mutable
    pub fn insert(&mut self, elem: T) -> bool { ... }
    pub fn remove(&mut self, elem: &T) -> bool { ... }

    // Consumidor — self = toma ownership
    pub fn into_vec(self) -> Vec<T> { self.data }
}
```

| Receptor | Significado | Equivalente C |
|----------|-------------|---------------|
| (ninguno) | Función asociada (constructor) | `Set *set_create(void)` |
| `&self` | Solo lectura | `const Set *s` |
| `&mut self` | Lectura y escritura | `Set *s` |
| `self` | Consume la instancia | `Set *s` + libera al terminar |

El compilador verifica esto estáticamente:

```rust
let s = VecSet::<i32>::new();  // s es inmutable
s.contains(&5);                // OK — &self
s.insert(5);                   // ERROR: cannot borrow `s` as mutable

let mut s = VecSet::<i32>::new();  // s es mutable
s.contains(&5);                     // OK — &self
s.insert(5);                        // OK — &mut self
```

En C, `const Set *s` es una sugerencia (se puede cast away). En Rust,
`&self` es una **garantía** — el compilador no permite mutación.

---

## 6. Drop: el destructor automático

En C, el destructor es manual (`set_destroy(s)`). Si se olvida: memory leak.

En Rust, `Drop` se llama automáticamente cuando la variable sale del scope:

```rust
{
    let s = VecSet::<i32>::new();
    s.insert(10);
    s.insert(20);
    // ... usar s ...
}   // s sale del scope → Drop se llama automáticamente
    // Vec<T> dentro de VecSet libera su memoria
```

### Drop explícito vs implícito

La mayoría de los TADs no necesitan implementar `Drop` manualmente — el
compilador genera un destructor que llama `Drop` recursivamente para cada campo:

```rust
pub struct VecSet<T> {
    data: Vec<T>,    // Vec implementa Drop → se libera automáticamente
}
// No se necesita impl Drop — el compilador destruye data automáticamente
```

Se implementa `Drop` manualmente cuando hay recursos que el compilador no
puede manejar (raw pointers, file handles, conexiones de red):

```rust
pub struct RawSet {
    ptr: *mut i32,
    len: usize,
    cap: usize,
}

impl Drop for RawSet {
    fn drop(&mut self) {
        unsafe {
            // Reconstruir el Vec para que lo libere correctamente
            let _ = Vec::from_raw_parts(self.ptr, self.len, self.cap);
        }
    }
}
```

### Comparación con C

```c
// C — olvidar destroy = leak silencioso
void process(void) {
    Set *s = set_create();
    set_insert(s, 42);
    if (error_condition) return;    // LEAK — s no se destruye
    set_destroy(s);
}
```

```rust
// Rust — Drop se ejecuta en TODOS los caminos de salida
fn process() {
    let mut s = VecSet::new();
    s.insert(42);
    if error_condition { return; }  // s se destruye aquí automáticamente
    // s se destruye aquí si no hubo return
}
```

En Rust es **imposible** tener el leak del ejemplo C con tipos safe.
El compilador inserta la llamada a `drop()` en cada punto de salida.

---

## 7. Módulos como barrera de abstracción

En C, la barrera es el par `.h`/`.c`. En Rust, es el sistema de módulos:

### Estructura de archivos

```
src/
├── main.rs
└── set.rs      // módulo "set"
```

### set.rs — el módulo como TAD

```rust
// src/set.rs

/// A set of unique elements backed by a Vec.
pub struct VecSet<T> {
    data: Vec<T>,           // privado — no visible fuera de este archivo
}

impl<T: PartialEq> VecSet<T> {
    pub fn new() -> Self {
        VecSet { data: Vec::new() }
    }

    pub fn insert(&mut self, elem: T) -> bool {
        if self.contains(&elem) {
            return false;
        }
        self.data.push(elem);
        true
    }

    pub fn remove(&mut self, elem: &T) -> bool {
        if let Some(pos) = self.data.iter().position(|x| x == elem) {
            self.data.swap_remove(pos);
            true
        } else {
            false
        }
    }

    pub fn contains(&self, elem: &T) -> bool {
        self.data.iter().any(|x| x == elem)
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    // Método privado — no visible fuera del módulo
    fn find_index(&self, elem: &T) -> Option<usize> {
        self.data.iter().position(|x| x == elem)
    }
}
```

### main.rs — el usuario del TAD

```rust
// src/main.rs
mod set;

use set::VecSet;

fn main() {
    let mut s = VecSet::new();

    s.insert(10);
    s.insert(20);
    s.insert(30);
    s.insert(20);    // duplicado — retorna false

    println!("len: {}", s.len());           // 3
    println!("contains 20: {}", s.contains(&20));  // true
    println!("contains 99: {}", s.contains(&99));  // false

    s.remove(&20);
    println!("len after remove: {}", s.len());  // 2

    // s.data;          // ERROR: field `data` is private
    // s.find_index(&5); // ERROR: method `find_index` is private
}
```

### Equivalencia con C

```
C                               Rust
─────────────────────────────────────────────
set.h (interfaz pública)    →   pub fn ... en set.rs
set.c (implementación)      →   fn sin pub en set.rs + campos privados
static set_find()           →   fn find_index() (sin pub)
#include "set.h"            →   mod set; use set::VecSet;
set_create() / set_destroy() → VecSet::new() / Drop automático
```

---

## 8. Implementación completa: TAD Set con trait

Ejemplo con trait + dos implementaciones para mostrar polimorfismo:

### El trait (contrato)

```rust
pub trait Set<T> {
    fn new() -> Self;
    fn insert(&mut self, elem: T) -> bool;
    fn remove(&mut self, elem: T) -> bool;
    fn contains(&self, elem: &T) -> bool;
    fn len(&self) -> usize;
    fn is_empty(&self) -> bool { self.len() == 0 }
}
```

### Implementación 1: VecSet (array dinámico)

```rust
pub struct VecSet<T> {
    data: Vec<T>,
}

impl<T: PartialEq> Set<T> for VecSet<T> {
    fn new() -> Self {
        VecSet { data: Vec::new() }
    }

    fn insert(&mut self, elem: T) -> bool {
        if self.contains(&elem) { return false; }
        self.data.push(elem);
        true
    }

    fn remove(&mut self, elem: T) -> bool {
        if let Some(pos) = self.data.iter().position(|x| x == &elem) {
            self.data.swap_remove(pos);
            true
        } else {
            false
        }
    }

    fn contains(&self, elem: &T) -> bool {
        self.data.contains(elem)
    }

    fn len(&self) -> usize {
        self.data.len()
    }
}
```

### Implementación 2: SortedVecSet (array ordenado + búsqueda binaria)

```rust
pub struct SortedVecSet<T> {
    data: Vec<T>,
}

impl<T: Ord> Set<T> for SortedVecSet<T> {
    fn new() -> Self {
        SortedVecSet { data: Vec::new() }
    }

    fn insert(&mut self, elem: T) -> bool {
        match self.data.binary_search(&elem) {
            Ok(_) => false,                          // ya existe
            Err(pos) => { self.data.insert(pos, elem); true }
        }
    }

    fn remove(&mut self, elem: T) -> bool {
        match self.data.binary_search(&elem) {
            Ok(pos) => { self.data.remove(pos); true }
            Err(_) => false
        }
    }

    fn contains(&self, elem: &T) -> bool {
        self.data.binary_search(elem).is_ok()
    }

    fn len(&self) -> usize {
        self.data.len()
    }
}
```

### Uso polimórfico

```rust
fn print_set_info<T, S: Set<T>>(set: &S) {
    println!("len: {}, empty: {}", set.len(), set.is_empty());
}

fn main() {
    let mut vs: VecSet<i32> = Set::new();
    vs.insert(3); vs.insert(1); vs.insert(2);
    print_set_info(&vs);   // funciona con VecSet

    let mut svs: SortedVecSet<i32> = Set::new();
    svs.insert(3); svs.insert(1); svs.insert(2);
    print_set_info(&svs);  // funciona con SortedVecSet
}
```

Misma función `print_set_info` trabaja con cualquier implementación del
trait `Set` — equivalente a que `main.c` funcione con cualquier `set.c`
en C, pero verificado en compilación.

### Trait bounds: la diferencia

`VecSet<T>` requiere `T: PartialEq` (solo necesita comparar igualdad).
`SortedVecSet<T>` requiere `T: Ord` (necesita comparar orden para búsqueda binaria).

Esto es información que el trait bound codifica y el compilador verifica:

```rust
let mut s = SortedVecSet::<f64>::new();
// ERROR: the trait `Ord` is not implemented for `f64`
// f64 tiene NaN → no tiene orden total → no puede usarse en SortedVecSet
```

En C, usar `qsort` con un comparador incorrecto para floats (NaN) produce
comportamiento indefinido silenciosamente. En Rust, el compilador lo rechaza.

---

## 9. Display y Debug: observadores estándar

En C, la función `set_print` es manual. En Rust, se implementan traits
estándar:

### Debug (para desarrollo)

```rust
use std::fmt;

impl<T: fmt::Debug> fmt::Debug for VecSet<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_set().entries(self.data.iter()).finish()
    }
}
```

```rust
let mut s = VecSet::new();
s.insert(10); s.insert(20);
println!("{:?}", s);   // {10, 20}
```

### Display (para el usuario)

```rust
impl<T: fmt::Display> fmt::Display for VecSet<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{{")?;
        for (i, elem) in self.data.iter().enumerate() {
            if i > 0 { write!(f, ", ")?; }
            write!(f, "{}", elem)?;
        }
        write!(f, "}}")
    }
}
```

```rust
println!("{}", s);     // {10, 20}
```

### derive automático

Para tipos simples, `#[derive]` genera implementaciones automáticas:

```rust
#[derive(Debug, Clone, PartialEq)]
pub struct VecSet<T> {
    data: Vec<T>,
}
```

Esto genera `Debug`, `Clone` y `PartialEq` sin código manual. No funciona para
`Display` (siempre manual) ni para traits custom como nuestro `Set<T>`.

---

## 10. Comparación directa: el mismo TAD en C y Rust

### Constructor

```c
// C
Set *s = set_create();
if (!s) { /* manejar error */ }
```

```rust
// Rust
let mut s = VecSet::new();
// no puede fallar (Vec::new no aloca hasta el primer push)
```

### Insertar

```c
// C
if (!set_insert(s, 42)) {
    fprintf(stderr, "insert failed\n");
}
```

```rust
// Rust
let was_new = s.insert(42);
// true = nuevo, false = ya existía
// no puede fallar por memoria (panic en OOM)
```

### Verificar pertenencia

```c
// C
if (set_contains(s, 42)) {
    printf("found\n");
}
```

```rust
// Rust
if s.contains(&42) {
    println!("found");
}
```

### Destructor

```c
// C
set_destroy(s);    // obligatorio — si se olvida: leak
s = NULL;          // buena práctica — evitar dangling pointer
```

```rust
// Rust
// nada — Drop se llama automáticamente al salir del scope
```

### Encapsulación

```c
// C — intento de acceder a internals
s->data[0] = 999;
// Compila SI main.c tiene la definición completa del struct
// No compila si el struct es opaco (incomplete type)
```

```rust
// Rust — intento de acceder a internals
s.data[0] = 999;
// No compila NUNCA desde fuera del módulo:
// error: field `data` of struct `VecSet` is private
```

### Tabla resumen

| Aspecto | C | Rust |
|---------|---|------|
| Encapsulación | Convención (struct opaco) | Lenguaje (campos privados) |
| Destructor | Manual, fácil olvidar | Automático (Drop) |
| Error de memoria | UB, descubierto con Valgrind | No compila, o panic |
| Genéricos | `void *` (sin type-safety) | `<T>` (verificado en compilación) |
| Polimorfismo | Punteros a función | Traits (dispatch estático o dinámico) |
| Stack allocation | No con struct opaco | Sí (el compilador conoce el tamaño) |
| Namespace | Prefijo manual `set_` | Módulos + métodos `s.insert()` |

---

## Ejercicios

### Ejercicio 1 — Implementar VecSet desde cero

Crea un proyecto Rust e implementa `VecSet<T>` con los métodos `new`, `insert`,
`remove`, `contains`, `len`, `is_empty`. Testea con:

```rust
fn main() {
    let mut s = VecSet::new();
    s.insert(10);
    s.insert(20);
    s.insert(30);
    s.insert(20);    // duplicado

    println!("len: {}", s.len());
    println!("contains 20: {}", s.contains(&20));
    println!("contains 99: {}", s.contains(&99));

    s.remove(&20);
    println!("len after remove: {}", s.len());
}
```

**Predicción**: ¿Cuál será la salida? ¿Qué trait bound necesita `T`
como mínimo para que `contains` funcione?

<details><summary>Respuesta</summary>

```
len: 3
contains 20: true
contains 99: false
len after remove: 2
```

`T` necesita como mínimo `PartialEq` para poder comparar elementos con `==`.
Sin `T: PartialEq`, la llamada a `iter().any(|x| x == elem)` o
`Vec::contains` no compila:

```
error[E0369]: binary operation `==` cannot be applied to type `T`
```

Si además se quisiera `SortedVecSet`, `T` necesitaría `Ord` (orden total).

</details>

---

### Ejercicio 2 — Verificar encapsulación

Intenta acceder al campo `data` desde `main.rs`:

```rust
mod set;
use set::VecSet;

fn main() {
    let s = VecSet::<i32>::new();
    println!("{:?}", s.data);        // línea problemática
}
```

**Predicción**: ¿Qué error exacto da el compilador? ¿Cambia algo si usas
`pub data: Vec<T>` en la definición del struct?

<details><summary>Respuesta</summary>

Error con campos privados:

```
error[E0616]: field `data` of struct `VecSet` is private
 --> src/main.rs:5:27
  |
5 |     println!("{:?}", s.data);
  |                           ^^^^ private field
```

Si cambias a `pub data: Vec<T>`, compila — pero rompes la barrera de abstracción.
Cualquiera podría hacer `s.data.push(42)` sin verificar duplicados, violando
la invariante del conjunto.

Diferencia con C: en C, si el struct no es opaco, el acceso a campos compila
sin warning. En Rust, la privacidad es el default y hay que optar explícitamente
por exponerlo con `pub`. Esto hace que sea más difícil romper encapsulación
accidentalmente.

</details>

---

### Ejercicio 3 — Trait con dos implementaciones

Define el trait `Set<T>` y dos implementaciones:

1. `VecSet<T>` — array sin ordenar (`T: PartialEq`)
2. `SortedVecSet<T>` — array ordenado con `binary_search` (`T: Ord`)

Testea ambas con la misma función:

```rust
fn test_set<S: Set<i32>>(name: &str) {
    let mut s = S::new();
    s.insert(30);
    s.insert(10);
    s.insert(20);
    s.insert(10);
    println!("[{}] len: {}, contains 10: {}", name, s.len(), s.contains(&10));
}

fn main() {
    test_set::<VecSet<i32>>("VecSet");
    test_set::<SortedVecSet<i32>>("SortedVecSet");
}
```

**Predicción**: ¿Ambas imprimen lo mismo? ¿Cuál tiene mejor complejidad
para `contains`?

<details><summary>Respuesta</summary>

Ambas imprimen:

```
[VecSet] len: 3, contains 10: true
[SortedVecSet] len: 3, contains 10: true
```

La salida es idéntica — el comportamiento observable (contrato del TAD) es
el mismo. La diferencia es interna:

| Operación | VecSet | SortedVecSet |
|-----------|--------|--------------|
| `insert` | O(n) búsqueda + O(1) push | O(log n) búsqueda + O(n) shift |
| `contains` | **O(n)** lineal | **O(log n)** binaria |
| `remove` | O(n) búsqueda + O(1) swap_remove | O(log n) búsqueda + O(n) shift |

`SortedVecSet` gana en `contains` (O(log n) vs O(n)) pero pierde en
`insert` y `remove` por el shift de elementos.

Este es exactamente el punto de T01: el TAD define el contrato, la
estructura de datos determina la complejidad.

</details>

---

### Ejercicio 4 — Drop explícito vs implícito

Crea un struct que imprima un mensaje cuando se destruye:

```rust
struct Noisy {
    name: String,
}

impl Drop for Noisy {
    fn drop(&mut self) {
        println!("dropping {}", self.name);
    }
}
```

Ejecuta:

```rust
fn main() {
    let a = Noisy { name: "A".into() };
    let b = Noisy { name: "B".into() };
    println!("before scope end");
}
```

**Predicción**: ¿En qué orden se imprimen los mensajes? ¿Se destruye
primero A o B?

<details><summary>Respuesta</summary>

```
before scope end
dropping B
dropping A
```

Las variables se destruyen en **orden inverso** a su declaración (LIFO —
como una pila). `b` se declaró después de `a`, así que se destruye primero.

Esto es idéntico a la regla de C "destruir en orden inverso a la
construcción" — pero en C hay que hacerlo manualmente, y en Rust es
automático.

¿Por qué orden inverso? Porque una variable declarada después podría
referenciar a una declarada antes. Destruir en orden inverso garantiza
que las referencias son válidas durante la destrucción:

```rust
let data = vec![1, 2, 3];
let slice = &data[..];    // slice referencia a data
// Al salir: slice se destruye primero (OK)
// Luego data se destruye (OK — nadie la referencia)
// Si fuera al revés: data se destruye → slice apunta a memoria liberada
```

</details>

---

### Ejercicio 5 — impl sin trait vs con trait

Implementa un TAD `Counter` de dos formas:

**Forma A** — solo `impl`:

```rust
pub struct Counter {
    counts: std::collections::HashMap<String, usize>,
}

impl Counter {
    pub fn new() -> Self { ... }
    pub fn add(&mut self, key: &str) { ... }
    pub fn get(&self, key: &str) -> usize { ... }
    pub fn total(&self) -> usize { ... }
}
```

**Forma B** — con trait `FrequencyCounter`:

```rust
pub trait FrequencyCounter {
    fn new() -> Self;
    fn add(&mut self, key: &str);
    fn get(&self, key: &str) -> usize;
    fn total(&self) -> usize;
}
```

Implementa ambas versiones del `Counter`. Luego responde:

**Predicción**: ¿Cuándo elegirías la forma A y cuándo la B? ¿Qué obtienes
con B que A no da?

<details><summary>Respuesta</summary>

Forma A — impl directo:

```rust
use std::collections::HashMap;

pub struct Counter {
    counts: HashMap<String, usize>,
}

impl Counter {
    pub fn new() -> Self {
        Counter { counts: HashMap::new() }
    }

    pub fn add(&mut self, key: &str) {
        *self.counts.entry(key.to_string()).or_insert(0) += 1;
    }

    pub fn get(&self, key: &str) -> usize {
        self.counts.get(key).copied().unwrap_or(0)
    }

    pub fn total(&self) -> usize {
        self.counts.values().sum()
    }
}
```

Forma B — trait:

```rust
pub trait FrequencyCounter {
    fn new() -> Self;
    fn add(&mut self, key: &str);
    fn get(&self, key: &str) -> usize;
    fn total(&self) -> usize;
}

impl FrequencyCounter for Counter {
    fn new() -> Self { Counter { counts: HashMap::new() } }
    fn add(&mut self, key: &str) { *self.counts.entry(key.to_string()).or_insert(0) += 1; }
    fn get(&self, key: &str) -> usize { self.counts.get(key).copied().unwrap_or(0) }
    fn total(&self) -> usize { self.counts.values().sum() }
}
```

**Cuándo elegir cada una**:

- **Forma A**: si `Counter` es la única implementación posible. Simple, directo,
  menos boilerplate. Es lo más común.
- **Forma B**: si podrías querer un `BTreeCounter` (ordenado), un `ApproxCounter`
  (Count-Min Sketch), o un `MockCounter` (para tests). El trait permite
  escribir código genérico: `fn process<C: FrequencyCounter>(counter: &mut C)`.

Lo que B da y A no: **polimorfismo**. Con B puedes escribir funciones que
aceptan cualquier implementación del trait sin conocerla.

En la práctica, se empieza con A y se extrae el trait (B) cuando surge la
necesidad. "Make the change easy, then make the easy change."

</details>

---

### Ejercicio 6 — pub(crate) y visibilidad granular

Crea un proyecto con esta estructura:

```
src/
├── main.rs
├── set.rs
└── set_utils.rs
```

En `set.rs` define `VecSet` con un campo `pub(crate) data: Vec<T>`.
En `set_utils.rs` intenta acceder a `s.data`.
En `main.rs` intenta acceder a `s.data`.

**Predicción**: ¿Desde cuál de los dos archivos se puede acceder a `data`?
¿Cuál es la diferencia entre `pub(crate)` y `pub`?

<details><summary>Respuesta</summary>

`pub(crate)` permite acceso desde **cualquier archivo del mismo crate** pero
no desde crates externos.

- `set_utils.rs`: **Sí** puede acceder — está en el mismo crate
- `main.rs`: **Sí** puede acceder — está en el mismo crate

¿Entonces cuándo importa `pub(crate)` vs `pub`?

Cuando el código es una **biblioteca** (crate con `lib.rs`):

```
mi_lib/src/
├── lib.rs
├── set.rs         // pub(crate) data accesible aquí
└── set_utils.rs   // pub(crate) data accesible aquí

otro_proyecto/src/
└── main.rs        // usa mi_lib como dependencia
    // pub(crate) data: NO accesible (otro crate)
    // pub data: SÍ accesible
```

Resumen de visibilidad:

| Visibilidad | Mismo módulo | Mismo crate | Crate externo |
|-------------|-------------|-------------|---------------|
| (privado) | Sí | No | No |
| `pub(super)` | Sí | Módulo padre | No |
| `pub(crate)` | Sí | Sí | No |
| `pub` | Sí | Sí | Sí |

Para TADs en un binario (no biblioteca), `pub(crate)` y `pub` se comportan
igual. La distinción importa al publicar crates.

</details>

---

### Ejercicio 7 — Display personalizado

Implementa `Display` para `VecSet<T>` que imprima en formato `{a, b, c}`.
Luego implementa `Debug` manualmente (sin `#[derive]`) con formato
`VecSet([a, b, c], len=N)`.

Testea con:

```rust
let mut s = VecSet::new();
s.insert(10); s.insert(20); s.insert(30);
println!("Display: {}", s);     // {10, 20, 30}
println!("Debug:   {:?}", s);   // VecSet([10, 20, 30], len=3)
```

**Predicción**: ¿Qué pasa si `T` no implementa `Display`? ¿Compila el
impl de `Display` para `VecSet<T>`?

<details><summary>Respuesta</summary>

```rust
use std::fmt;

impl<T: fmt::Display> fmt::Display for VecSet<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{{")?;
        for (i, elem) in self.data.iter().enumerate() {
            if i > 0 { write!(f, ", ")?; }
            write!(f, "{}", elem)?;
        }
        write!(f, "}}")
    }
}

impl<T: fmt::Debug> fmt::Debug for VecSet<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "VecSet({:?}, len={})", self.data, self.data.len())
    }
}
```

Si `T` no implementa `Display`, el `impl Display for VecSet<T>` **existe
pero no se puede usar** con ese `T` concreto:

```rust
struct NoDisplay;

let mut s = VecSet::new();
s.insert(NoDisplay);
// println!("{}", s);   // ERROR: `NoDisplay` doesn't implement `Display`
println!("{:?}", s);    // También ERROR si NoDisplay no tiene Debug
```

Los trait bounds (`T: Display`) actúan como filtro: la implementación está
disponible **condicionalmente**, solo para tipos que cumplen el bound. Esto
es imposible en C — no hay forma de decir "esta función existe solo si T
tiene cierta propiedad".

</details>

---

### Ejercicio 8 — Equivalente de const correctness

Traduce estas firmas de C a Rust, decidiendo si el parámetro es `&self` o
`&mut self`:

```c
size_t  stack_size(const Stack *s);
void    stack_push(Stack *s, int elem);
int     stack_top(const Stack *s);
int     stack_pop(Stack *s);
void    stack_clear(Stack *s);
bool    stack_empty(const Stack *s);
Stack  *stack_create(void);
void    stack_destroy(Stack *s);
```

**Predicción**: ¿Cuáles son `&self`, cuáles `&mut self`, y cuáles no
tienen `self` (associated function)?

<details><summary>Respuesta</summary>

```rust
impl Stack {
    fn new() -> Self;                       // stack_create — no self (constructor)
    fn size(&self) -> usize;                // stack_size — const → &self
    fn push(&mut self, elem: i32);          // stack_push — no const → &mut self
    fn top(&self) -> i32;                   // stack_top — const → &self
    fn pop(&mut self) -> i32;               // stack_pop — no const → &mut self
    fn clear(&mut self);                    // stack_clear — no const → &mut self
    fn is_empty(&self) -> bool;             // stack_empty — const → &self
    // stack_destroy → no hay equivalente, Drop automático
}
```

Regla de traducción:

| C | Rust | Razón |
|---|------|-------|
| `const T *` | `&self` | Solo lectura |
| `T *` (modifica) | `&mut self` | Lectura + escritura |
| `T *xxx_create()` | `fn new() -> Self` | Constructor, sin self |
| `xxx_destroy(T *)` | `impl Drop` o nada | Destructor automático |

`stack_destroy` no tiene equivalente directo porque `Drop` se ejecuta
automáticamente. Si necesitas destrucción explícita antes del fin del
scope, usas `drop(s)` (función de la biblioteca estándar).

</details>

---

### Ejercicio 9 — Error de ownership al usar el TAD

¿Qué error da este código? Corrígelo de dos formas diferentes:

```rust
fn print_size(s: VecSet<i32>) {
    println!("size: {}", s.len());
}

fn main() {
    let mut s = VecSet::new();
    s.insert(42);
    print_size(s);
    println!("contains 42: {}", s.contains(&42));  // ERROR
}
```

**Predicción**: ¿Por qué no compila la última línea? ¿Cuál es la
diferencia entre `s: VecSet<i32>` y `s: &VecSet<i32>` como parámetro?

<details><summary>Respuesta</summary>

Error:

```
error[E0382]: borrow of moved value: `s`
 --> src/main.rs:8:40
  |
5 |     let mut s = VecSet::new();
  |         ----- move occurs because `s` has type `VecSet<i32>`
7 |     print_size(s);
  |                - value moved here
8 |     println!("contains 42: {}", s.contains(&42));
  |                                  ^ value borrowed here after move
```

`print_size(s)` toma `s` **por valor** (`self`, no `&self`). Esto **mueve**
el ownership de `s` a la función. Después del move, `s` ya no es válido.

**Corrección 1** — pasar por referencia:

```rust
fn print_size(s: &VecSet<i32>) {
    println!("size: {}", s.len());
}

fn main() {
    let mut s = VecSet::new();
    s.insert(42);
    print_size(&s);                            // préstamo, no move
    println!("contains 42: {}", s.contains(&42));  // OK
}
```

**Corrección 2** — clonar:

```rust
fn print_size(s: VecSet<i32>) { ... }  // sigue tomando ownership

fn main() {
    let mut s = VecSet::new();
    s.insert(42);
    print_size(s.clone());                     // pasa una copia
    println!("contains 42: {}", s.contains(&42));  // OK
}
```

La corrección 1 es preferible — no tiene costo extra. La corrección 2
copia todos los datos, es O(n).

En C este problema no existe de la misma forma: todos los punteros son
"prestados" implícitamente. Pero el problema inverso sí: en C puedes
usar un puntero después de `free` y no hay error de compilación.

</details>

---

### Ejercicio 10 — C vs Rust: mismo TAD, diferente ergonomía

Implementa el mismo TAD "Stack de strings" en C y en Rust. En C debe
copiar los strings con `strdup`. En Rust debe tomar ownership de `String`.

Requisitos mínimos: `create`/`new`, `push`, `pop`, `top`/`peek`,
`destroy`/Drop.

Después de implementar ambos, responde:

**Predicción**: ¿Cuántas líneas tiene cada versión? ¿Cuántas llamadas a
`free`/`drop` son manuales en cada caso? ¿Cuál es más fácil de equivocarse?

<details><summary>Respuesta</summary>

**C (~70 líneas)**:

```c
// strstack.h
typedef struct StrStack StrStack;
StrStack   *strstack_create(void);
void        strstack_destroy(StrStack *s);
bool        strstack_push(StrStack *s, const char *str);
char       *strstack_pop(StrStack *s);      // caller must free
const char *strstack_peek(const StrStack *s);
bool        strstack_empty(const StrStack *s);

// strstack.c
struct StrStack { char **data; size_t size; size_t cap; };

void strstack_destroy(StrStack *s) {
    if (!s) return;
    for (size_t i = 0; i < s->size; i++) free(s->data[i]); // N frees
    free(s->data);   // 1 free
    free(s);         // 1 free
}

bool strstack_push(StrStack *s, const char *str) {
    // ... grow if needed ...
    s->data[s->size] = strdup(str);     // copiar string
    if (!s->data[s->size]) return false;
    s->size++;
    return true;
}

char *strstack_pop(StrStack *s) {
    assert(!strstack_empty(s));
    s->size--;
    return s->data[s->size];    // caller owns this string now
}
```

**Rust (~30 líneas)**:

```rust
pub struct StrStack {
    data: Vec<String>,
}

impl StrStack {
    pub fn new() -> Self { StrStack { data: Vec::new() } }
    pub fn push(&mut self, s: String) { self.data.push(s); }
    pub fn pop(&mut self) -> Option<String> { self.data.pop() }
    pub fn peek(&self) -> Option<&str> { self.data.last().map(|s| s.as_str()) }
    pub fn is_empty(&self) -> bool { self.data.is_empty() }
    // Drop: automático — Vec<String> libera todo
}
```

Comparación:

| Aspecto | C | Rust |
|---------|---|------|
| Líneas | ~70 | ~30 |
| `free`/`drop` manuales | N+2 en destroy + caller free en pop | 0 |
| Puede olvidar free | Sí (leak) | No (Drop automático) |
| Puede double-free | Sí | No (ownership) |
| Puede use-after-free | Sí | No (borrow checker) |
| Pop retorna | `char *` (caller debe free) | `Option<String>` (ownership transferido) |

La versión C tiene 3 puntos de error común:
1. Olvidar `free` de los strings individuales en `destroy`
2. Olvidar `free` del string retornado por `pop`
3. Usar el string después de `free`

La versión Rust no tiene ninguno de estos riesgos.

</details>
