# T01 — Raw pointers

---

## 1. Por qué raw pointers en Rust

Rust tiene `&T` y `&mut T` — referencias seguras con lifetime checking. Pero para
implementar estructuras de datos como listas enlazadas, árboles o grafos, las
referencias seguras son insuficientes:

```
Problema: un nodo de lista doblemente enlazada necesita que el nodo anterior
y el siguiente apunten mutuamente. Eso crea ciclos de ownership que &T/&mut T
no pueden expresar.

Solución safe:  Rc<RefCell<Node<T>>>  — funciona pero tiene overhead de runtime
Solución unsafe: *mut Node<T>         — como punteros de C, sin overhead
```

Los raw pointers (`*const T`, `*mut T`) son el mecanismo de Rust para
**optar fuera del borrow checker** en situaciones donde las garantías estáticas
son demasiado restrictivas. Son fundamentales para implementar estructuras de
datos eficientes.

### Filosofía

```
C:     Todo es punteros → los bugs son tu problema
Rust:  Todo son referencias seguras → usa raw pointers solo cuando necesites

Raw pointers en Rust ≠ "Rust inseguro"
Raw pointers en Rust = "El programador asume la responsabilidad aquí"
```

---

## 2. Los dos tipos: `*const T` y `*mut T`

```rust
*const T    // puntero inmutable (lectura)
*mut T      // puntero mutable  (lectura + escritura)
```

### Diferencias con referencias

| Propiedad | `&T` / `&mut T` | `*const T` / `*mut T` |
|-----------|-----------------|----------------------|
| Garantiza validez | Sí (compilador) | No |
| Puede ser null | No | Sí |
| Tiene lifetime | Sí | No |
| Permite aliasing mutable | No | Sí |
| Se puede desreferenciar en safe code | Sí | No |
| Implementa `Send`/`Sync` | Según `T` | No (por defecto) |
| Aritmética de punteros | No | Sí |

La diferencia clave: **crear** un raw pointer es safe, **desreferenciarlo** requiere
`unsafe`. Esto separa el momento de "construir la estructura" del momento de
"acceder a los datos".

---

## 3. Creación de raw pointers (safe)

Crear raw pointers no requiere `unsafe`. Se pueden crear de múltiples formas:

### Desde referencias

```rust
let x: i32 = 42;
let r: &i32 = &x;

// Coerción implícita: &T → *const T
let p: *const i32 = &x;

// Coerción explícita con as
let p: *const i32 = &x as *const i32;

// Mutable
let mut y: i32 = 100;
let pm: *mut i32 = &mut y as *mut i32;
```

### Desde `*const` a `*mut` y viceversa

```rust
let x = 42;
let p_const: *const i32 = &x;

// const → mut (peligroso: si x no es realmente mutable, UB al escribir)
let p_mut: *mut i32 = p_const as *mut i32;

// mut → const (siempre seguro conceptualmente)
let mut y = 10;
let pm: *mut i32 = &mut y;
let pc: *const i32 = pm as *const i32;
```

### Null pointers

```rust
use std::ptr;

let p: *const i32 = ptr::null();
let pm: *mut i32 = ptr::null_mut();

// Verificar null
assert!(p.is_null());
assert!(pm.is_null());
```

`ptr::null()` y `ptr::null_mut()` son el equivalente de `NULL` en C. Se usan
para inicializar punteros que aún no apuntan a nada — por ejemplo, el `next`
del último nodo de una lista.

### Desde direcciones numéricas

```rust
// usize → *const T (raro, pero posible)
let addr: usize = 0xDEADBEEF;
let p = addr as *const i32;
// Desreferenciar esto sería UB — la dirección probablemente no es válida
```

Esto es infrecuente en estructuras de datos, pero aparece en sistemas embebidos
y FFI donde se trabaja con direcciones fijas de hardware.

---

## 4. Desreferencia unsafe

El acto de **leer o escribir** a través de un raw pointer requiere `unsafe`:

```rust
let mut x = 42;
let p: *mut i32 = &mut x;

unsafe {
    // Leer
    let value = *p;      // value = 42
    println!("{value}");

    // Escribir
    *p = 100;
}
assert_eq!(x, 100);
```

### Por qué es unsafe

El compilador no puede verificar que:

1. **El puntero no es null**
2. **El puntero apunta a memoria válida** (no fue liberada, no es dangling)
3. **La memoria contiene un `T` válido** (alineación, inicialización)
4. **No hay aliasing prohibido** (no hay `&mut T` activo simultáneamente)

El programador debe garantizar todo esto manualmente. Si falla alguna, el
comportamiento es **undefined behavior (UB)** — el mismo problema que C, con
las mismas consecuencias.

### Ejemplo: UB por dangling pointer

```rust
let p: *mut i32;

{
    let mut x = 42;
    p = &mut x as *mut i32;
}   // x se destruye aquí

unsafe {
    println!("{}", *p);  // UB: p apunta a memoria ya liberada
}
```

En C sería exactamente lo mismo:

```c
int *p;
{
    int x = 42;
    p = &x;
}   // x fuera de scope
printf("%d\n", *p);  // UB: dangling pointer
```

### `ptr::read` y `ptr::write`

Para operaciones más controladas que la desreferencia directa:

```rust
let mut x = 42;
let p: *mut i32 = &mut x;

unsafe {
    // Equivalente a: let val = *p;
    let val = ptr::read(p);

    // Equivalente a: *p = 100;
    ptr::write(p, 100);
}
```

`ptr::read` y `ptr::write` son útiles cuando el tipo `T` no implementa `Copy`,
porque permiten mover valores sin que el compilador asuma que el origen sigue
siendo válido:

```rust
let mut s = String::from("hello");
let p: *mut String = &mut s;

unsafe {
    // Mueve el String fuera de *p
    let taken = ptr::read(p);
    // Ahora *p contiene un String "hueco" (su drop sería doble)
    // Debemos escribir algo válido o evitar el drop
    ptr::write(p, String::from("replaced"));
}
```

---

## 5. Null, comparación e identidad

### Comparación de punteros

```rust
let x = 42;
let y = 42;
let p1: *const i32 = &x;
let p2: *const i32 = &x;
let p3: *const i32 = &y;

// Compara DIRECCIONES, no valores
assert!(p1 == p2);     // true:  misma dirección
assert!(p1 != p3);     // true:  distinta dirección (aunque mismo valor)

// Para comparar valores, desreferenciar:
unsafe {
    assert!(*p1 == *p3);  // true: mismo valor
}
```

### Patrón para null checks

```rust
fn safe_deref(p: *const i32) -> Option<i32> {
    if p.is_null() {
        None
    } else {
        unsafe { Some(*p) }
    }
}
```

Este patrón aparece constantemente en estructuras de datos. El `next` de un nodo
puede ser null (fin de la lista) o válido:

```rust
struct Node {
    data: i32,
    next: *mut Node,
}

impl Node {
    fn next_data(&self) -> Option<i32> {
        if self.next.is_null() {
            None
        } else {
            unsafe { Some((*self.next).data) }
        }
    }
}
```

### `ptr::eq` para comparación explícita

```rust
use std::ptr;

let x = 10;
let p1: *const i32 = &x;
let p2: *const i32 = &x;

assert!(ptr::eq(p1, p2));   // equivale a p1 == p2
```

`ptr::eq` es preferible cuando la comparación de punteros podría confundirse
con comparación de valores (especialmente con tipos que implementan `PartialEq`).

---

## 6. Aritmética de punteros

A diferencia de `&T`, los raw pointers soportan aritmética — avanzar o retroceder
por posiciones de memoria. Esto es esencial para recorrer arrays contiguos.

### `.add()` y `.sub()`

```rust
let arr = [10, 20, 30, 40, 50];
let base: *const i32 = arr.as_ptr();

unsafe {
    // base.add(n) avanza n elementos (no bytes)
    assert_eq!(*base.add(0), 10);
    assert_eq!(*base.add(1), 20);
    assert_eq!(*base.add(2), 30);
    assert_eq!(*base.add(4), 50);

    // .sub() retrocede
    let end = base.add(4);
    assert_eq!(*end.sub(1), 40);
}
```

### `.offset()`

Similar a `.add()` pero acepta `isize` (positivo o negativo):

```rust
let arr = [10, 20, 30, 40, 50];
let mid: *const i32 = unsafe { arr.as_ptr().add(2) };  // apunta a 30

unsafe {
    assert_eq!(*mid.offset(-1), 20);   // retrocede 1
    assert_eq!(*mid.offset(0), 30);    // actual
    assert_eq!(*mid.offset(2), 50);    // avanza 2
}
```

### Unidades: elementos, no bytes

A diferencia de C donde la aritmética es implícita, Rust es explícito:

```rust
// En C:
// int arr[5];
// int *p = arr;
// p + 2  →  avanza 2 * sizeof(int) bytes

// En Rust: .add(2) avanza 2 * size_of::<T>() bytes
// Mismo resultado, pero Rust es más explícito

let arr: [i32; 5] = [1, 2, 3, 4, 5];
let p = arr.as_ptr();

unsafe {
    // Si necesitas avanzar por bytes (raro):
    let byte_ptr = p as *const u8;
    let p2 = byte_ptr.add(8) as *const i32;  // 8 bytes = 2 i32s
    assert_eq!(*p2, 3);
}
```

### Límites y UB

```rust
let arr = [10, 20, 30];
let p = arr.as_ptr();

unsafe {
    // Válido: apuntar a one-past-end (pero no desreferenciar)
    let end = p.add(3);  // OK: posición válida para comparar

    // UB: desreferenciar one-past-end
    // let _ = *end;  // ← UB

    // UB: apuntar más allá de one-past-end
    // let _ = p.add(4);  // ← UB incluso sin desreferenciar
}
```

La regla es la misma que en C: se puede apuntar a la posición **justo después
del último elemento** (one-past-end) para comparaciones de rango, pero no
desreferenciarla. Apuntar más allá es UB incluso sin desreferenciar.

---

## 7. Raw pointers en nodos de estructuras de datos

El uso principal de raw pointers en este curso es construir nodos que se apuntan
entre sí:

### Nodo de lista simplemente enlazada

```rust
struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

impl<T> Node<T> {
    fn new(data: T) -> *mut Node<T> {
        let node = Box::new(Node {
            data,
            next: ptr::null_mut(),
        });
        Box::into_raw(node)  // convierte Box → raw pointer (heap-allocated)
    }
}
```

`Box::into_raw` transfiere el ownership del `Box` al raw pointer. La memoria
**no se libera** hasta que alguien llame `Box::from_raw` o la libere manualmente.
Esto se cubre en detalle en T03.

### Nodo de lista doblemente enlazada

```rust
struct DNode<T> {
    data: T,
    prev: *mut DNode<T>,
    next: *mut DNode<T>,
}

impl<T> DNode<T> {
    fn new(data: T) -> *mut DNode<T> {
        Box::into_raw(Box::new(DNode {
            data,
            prev: ptr::null_mut(),
            next: ptr::null_mut(),
        }))
    }
}
```

Aquí se ve por qué los raw pointers son necesarios: `prev` y `next` crean
referencias mutables circulares. Con `&mut T` esto es imposible — violaría la
regla de "un solo `&mut` a la vez". Con `*mut T`, el programador gestiona la
validez manualmente.

### Nodo de árbol binario

```rust
struct TreeNode<T> {
    data: T,
    left: *mut TreeNode<T>,
    right: *mut TreeNode<T>,
    parent: *mut TreeNode<T>,   // puntero al padre (opcional)
}
```

El puntero `parent` es especialmente problemático para referencias safe — crearía
un ciclo de ownership padre ↔ hijo. Con raw pointers es directo.

### Recorrido de una lista con raw pointers

```rust
fn print_list(head: *mut Node<i32>) {
    let mut current = head;
    while !current.is_null() {
        unsafe {
            print!("{} -> ", (*current).data);
            current = (*current).next;
        }
    }
    println!("NULL");
}
```

Comparación con C:

```c
void print_list(Node *head) {
    Node *current = head;
    while (current != NULL) {
        printf("%d -> ", current->data);
        current = current->next;
    }
    printf("NULL\n");
}
```

La estructura es idéntica. La diferencia es que en Rust, cada acceso a `(*current)`
está marcado con `unsafe`, haciendo visible dónde están los riesgos.

---

## 8. Patrones y errores comunes

### Error: olvidar que `*const`/`*mut` no tiene `->` de C

```rust
let node = Node::new(42);

// Error de sintaxis:
// node->data   ← no existe en Rust

// Correcto:
unsafe {
    (*node).data    // desreferenciar, luego acceder al campo
}
```

En Rust, la desreferencia es siempre explícita con `*`. No existe el operador
`->` de C — se usa `(*ptr).field`.

### Error: crear raw pointer a variable local y retornarla

```rust
// MAL: retorna puntero a variable local
fn bad() -> *const i32 {
    let x = 42;
    &x as *const i32   // x se destruye al salir de bad()
}   // el puntero retornado es dangling

// BIEN: usar heap
fn good() -> *mut i32 {
    Box::into_raw(Box::new(42))  // vive en el heap
}
```

### Error: asumir que `*mut T` es `Send`

```rust
struct List {
    head: *mut Node<i32>,
}

// *mut T no es Send ni Sync por defecto
// Si necesitas enviar entre threads:
unsafe impl Send for List {}
// Solo si REALMENTE garantizas que es seguro
```

### Patrón: wrapper safe sobre raw pointers

```rust
use std::ptr;

pub struct Stack<T> {
    head: *mut Node<T>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack {
            head: ptr::null_mut(),
            len: 0,
        }
    }

    pub fn push(&mut self, data: T) {
        let new_node = Node::new(data);
        unsafe {
            (*new_node).next = self.head;
        }
        self.head = new_node;
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let old_head = self.head;
            self.head = (*old_head).next;
            let node = Box::from_raw(old_head);  // retoma ownership para liberar
            self.len -= 1;
            Some(node.data)
        }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_null()
    }

    pub fn len(&self) -> usize {
        self.len
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}
```

Observa el patrón: la **interfaz pública es completamente safe** (`push`, `pop`,
`is_empty`, `len` — ninguno requiere que el usuario escriba `unsafe`). El `unsafe`
está **encapsulado** dentro de la implementación. Esto es el patrón correcto para
estructuras de datos con raw pointers en Rust.

---

## 9. Tabla de referencia rápida

### Funciones de `std::ptr`

| Función | Uso |
|---------|-----|
| `ptr::null()` | Crear `*const T` nulo |
| `ptr::null_mut()` | Crear `*mut T` nulo |
| `p.is_null()` | Verificar si es null |
| `ptr::eq(p, q)` | Comparar direcciones |
| `ptr::read(p)` | Leer valor (unsafe), mueve si no es `Copy` |
| `ptr::write(p, val)` | Escribir valor (unsafe) |
| `p.add(n)` | Avanzar `n` elementos (unsafe) |
| `p.sub(n)` | Retroceder `n` elementos (unsafe) |
| `p.offset(n)` | Avanzar/retroceder `n` elementos con `isize` (unsafe) |
| `p.as_ref()` | `*const T` → `Option<&T>` (unsafe) — None si null |
| `p.as_mut()` | `*mut T` → `Option<&mut T>` (unsafe) — None si null |

### Conversiones

| De → A | Cómo | Safe? |
|--------|------|-------|
| `&T` → `*const T` | `&x as *const T` o coerción | Sí |
| `&mut T` → `*mut T` | `&mut x as *mut T` o coerción | Sí |
| `*const T` → `*mut T` | `p as *mut T` | Sí (el cast; escribir puede ser UB) |
| `*mut T` → `*const T` | `p as *const T` | Sí |
| `*mut T` → `&T` | `&*p` o `p.as_ref()` | Unsafe |
| `*mut T` → `&mut T` | `&mut *p` o `p.as_mut()` | Unsafe |
| `Box<T>` → `*mut T` | `Box::into_raw(b)` | Sí (pero debes liberar) |
| `*mut T` → `Box<T>` | `Box::from_raw(p)` | Unsafe |
| `usize` → `*const T` | `addr as *const T` | Sí (el cast; usar puede ser UB) |

---

## 10. Raw pointers vs punteros de C

| Aspecto | C | Rust |
|---------|---|------|
| Sintaxis de tipo | `int *p` | `*mut i32` / `*const i32` |
| Null | `NULL` / `0` | `ptr::null()` / `ptr::null_mut()` |
| Desreferenciar | `*p` (siempre permitido) | `*p` (solo en `unsafe`) |
| Acceso a campo | `p->field` | `(*p).field` |
| Aritmética | `p + n` | `p.add(n)` (unsafe) |
| Diferencia entre punteros | `p - q` | `p.offset_from(q)` (unsafe) |
| Verificar null | `if (p != NULL)` | `if !p.is_null()` |
| Crear null | `int *p = NULL;` | `let p: *mut i32 = ptr::null_mut();` |
| Cast | `(int *)p` | `p as *mut i32` |
| Void pointer | `void *` | `*mut c_void` o `*mut u8` |
| Liberar | `free(p)` | `drop(Box::from_raw(p))` |
| Allocar | `malloc(sizeof(T))` | `Box::into_raw(Box::new(val))` |

Conceptualmente son lo mismo — una dirección de memoria sin garantías del
compilador. La diferencia está en la **visibilidad del peligro**: en C todo es
implícitamente unsafe, en Rust solo lo marcado con `unsafe`.

---

## Ejercicios

### Ejercicio 1 — Crear y desreferenciar

Escribe un programa que:
1. Cree una variable `i32` en el stack
2. Obtenga un `*const i32` hacia ella
3. Obtenga un `*mut i32` hacia ella
4. Lea el valor a través del `*const`
5. Modifique el valor a través del `*mut`
6. Verifique que la variable original cambió

```rust
fn main() {
    // Tu código aquí
}
```

**Prediccion**: Antes de compilar, responde: la creación de los punteros
requiere `unsafe`? Y la desreferencia?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

fn main() {
    let mut x: i32 = 42;

    // Crear punteros: safe
    let pc: *const i32 = &x;
    let pm: *mut i32 = &mut x;

    unsafe {
        // Leer a través de *const
        println!("read via *const: {}", *pc);   // 42

        // Escribir a través de *mut
        *pm = 100;
    }

    // La variable original cambió
    assert_eq!(x, 100);
    println!("x = {x}");  // 100
}
```

**Crear** punteros no requiere `unsafe` — es solo calcular una dirección.
**Desreferenciar** (leer o escribir) sí requiere `unsafe` porque el compilador
no puede garantizar que el puntero siga siendo válido.

Nota: este ejemplo tiene una sutileza — técnicamente crear `pc` y `pm` a la
misma variable viola las reglas de aliasing si ambos se usan como referencia
activa. Con raw pointers directos no pasa por el borrow checker, pero en código
real debes tener cuidado de no mezclar `&T` y `*mut T` activos simultáneamente.

</details>

---

### Ejercicio 2 — Null checks

Implementa una función `safe_read` que:
- Reciba un `*const i32`
- Retorne `Some(valor)` si no es null
- Retorne `None` si es null

Pruébala con un puntero válido y con `ptr::null()`.

```rust
fn safe_read(p: *const i32) -> Option<i32> {
    // Tu código aquí
}
```

**Prediccion**: El check `is_null()` necesita `unsafe`?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

fn safe_read(p: *const i32) -> Option<i32> {
    if p.is_null() {
        None
    } else {
        unsafe { Some(*p) }
    }
}

fn main() {
    let x = 42;
    let valid: *const i32 = &x;
    let null: *const i32 = ptr::null();

    assert_eq!(safe_read(valid), Some(42));
    assert_eq!(safe_read(null), None);
    println!("valid: {:?}", safe_read(valid));
    println!("null:  {:?}", safe_read(null));
}
```

`is_null()` **no** necesita `unsafe` — solo compara la dirección con cero,
no accede a memoria. Solo la desreferencia `*p` dentro del `else` es unsafe.

</details>

---

### Ejercicio 3 — Aritmética de punteros

Dado un array `[10, 20, 30, 40, 50]`, usa aritmética de raw pointers para:
1. Obtener un puntero al primer elemento con `.as_ptr()`
2. Leer el tercer elemento usando `.add()`
3. Desde el último elemento, retroceder al segundo usando `.sub()`
4. Recorrer todo el array con un loop y `.add()`

```rust
fn main() {
    let arr = [10, 20, 30, 40, 50];
    // Tu código aquí
}
```

**Prediccion**: `.add()` y `.sub()` requieren `unsafe`?

<details><summary>Respuesta</summary>

```rust
fn main() {
    let arr = [10, 20, 30, 40, 50];
    let base: *const i32 = arr.as_ptr();

    unsafe {
        // Tercer elemento (índice 2)
        let third = *base.add(2);
        assert_eq!(third, 30);
        println!("third: {third}");

        // Desde el último, retroceder al segundo
        let last = base.add(4);
        let second = *last.sub(3);
        assert_eq!(second, 20);
        println!("second: {second}");

        // Recorrer con loop
        print!("array: ");
        for i in 0..arr.len() {
            print!("{} ", *base.add(i));
        }
        println!();
    }
}
```

Sí, `.add()` y `.sub()` requieren `unsafe` porque el compilador no puede
verificar que el offset resultante siga dentro de la asignación de memoria
válida. Apuntar fuera del array (más allá de one-past-end) es UB incluso
sin desreferenciar.

</details>

---

### Ejercicio 4 — Nodo simple con raw pointers

Implementa un nodo de lista enlazada simple con raw pointers. Luego:
1. Crea 3 nodos en el heap con valores 10, 20, 30
2. Enlázalos: 10 → 20 → 30 → NULL
3. Recorre la lista e imprime los valores
4. Libera todos los nodos

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

fn main() {
    // Tu código aquí
}
```

**Prediccion**: Si olvidas el paso 4 (liberar), qué pasa? Rust te avisa?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

fn main() {
    // 1. Crear nodos en el heap
    let n1 = Box::into_raw(Box::new(Node { data: 10, next: ptr::null_mut() }));
    let n2 = Box::into_raw(Box::new(Node { data: 20, next: ptr::null_mut() }));
    let n3 = Box::into_raw(Box::new(Node { data: 30, next: ptr::null_mut() }));

    // 2. Enlazar
    unsafe {
        (*n1).next = n2;
        (*n2).next = n3;
        // n3.next ya es null
    }

    // 3. Recorrer
    let mut current = n1;
    while !current.is_null() {
        unsafe {
            print!("{} -> ", (*current).data);
            current = (*current).next;
        }
    }
    println!("NULL");
    // Salida: 10 -> 20 -> 30 -> NULL

    // 4. Liberar (recorrer de nuevo)
    let mut current = n1;
    while !current.is_null() {
        unsafe {
            let next = (*current).next;        // guardar next ANTES de liberar
            let _ = Box::from_raw(current);    // retoma ownership → drop
            current = next;
        }
    }
}
```

Si olvidas el paso 4: **memory leak**. Rust **no te avisa** — los raw pointers
creados con `Box::into_raw` son responsabilidad del programador. El compilador
no rastrea su lifetime. `cargo miri test` lo detectaría, y Valgrind también si
compilas como C-compatible.

El detalle más importante en la liberación: guardar `(*current).next` **antes**
de hacer `Box::from_raw(current)`, porque `Box::from_raw` toma ownership y
puede invalidar el nodo al hacer drop.

</details>

---

### Ejercicio 5 — Dangling pointer

Predice qué pasa en cada caso. Luego verifica con `cargo miri run`:

```rust
// Caso A
fn case_a() -> *const i32 {
    let x = 42;
    &x as *const i32
}

// Caso B
fn case_b() -> *mut i32 {
    Box::into_raw(Box::new(42))
}

fn main() {
    let pa = case_a();
    let pb = case_b();

    unsafe {
        println!("A: {}", *pa);   // ???
        println!("B: {}", *pb);   // ???
        drop(Box::from_raw(pb));
    }
}
```

**Prediccion**: Cuál de los dos casos es UB y por qué? El otro es safe?

<details><summary>Respuesta</summary>

- **Caso A**: **UB** — `x` vive en el stack de `case_a`. Cuando la función
  retorna, `x` se destruye y `pa` es dangling. Desreferenciar `*pa` lee
  memoria inválida. Puede "funcionar" por casualidad (el stack frame puede
  no haberse sobreescrito aún), pero es UB.

- **Caso B**: **Safe** — `Box::into_raw` crea un valor en el heap que persiste
  hasta que alguien lo libere con `Box::from_raw`. El puntero es válido
  después de retornar de `case_b()`.

Si ejecutas con `cargo miri run`:

```
error: Undefined Behavior: using dangling pointer
  --> src/main.rs (línea del *pa)
```

Miri detecta el caso A como UB. El caso B funciona correctamente.

La lección: en Rust (como en C), **los punteros a variables locales son
inválidos fuera de su scope**. Solo los valores en el heap sobreviven al
retorno de función.

</details>

---

### Ejercicio 6 — `ptr::read` vs desreferencia directa

Explica la diferencia entre estos dos fragmentos y cuándo usar cada uno:

```rust
// Fragmento 1
let s = String::from("hello");
let p: *const String = &s;
unsafe {
    let copy = (*p).clone();
    println!("{copy}");
}

// Fragmento 2
let s = String::from("hello");
let p: *const String = &s;
unsafe {
    let moved = ptr::read(p);
    println!("{moved}");
    // ¿qué pasa con s aquí?
}
```

**Prediccion**: El fragmento 2 causa double free? Por qué o por qué no?

<details><summary>Respuesta</summary>

**Fragmento 1**: `(*p).clone()` desreferencia el puntero para acceder al
`String`, y luego llama `.clone()` que crea una **copia independiente** en el
heap. El `String` original `s` sigue intacto. Al salir del scope, `s` se
libera y `copy` se libera — dos deallocations de dos buffers distintos.
No hay problema.

**Fragmento 2**: `ptr::read(p)` hace una **copia bit-a-bit** del `String`
(puntero al buffer, longitud, capacidad). Ahora tanto `s` como `moved` apuntan
al **mismo buffer en el heap**. Cuando `moved` se destruye, libera el buffer.
Cuando `s` se destruye al final del scope, intenta liberar el mismo buffer
otra vez → **double free**.

```
Fragmento 1:
  s     → [buffer A]
  copy  → [buffer B]    ← clone crea nuevo buffer
  ✓ drop s libera A, drop copy libera B

Fragmento 2:
  s     → [buffer A]
  moved → [buffer A]    ← ptr::read copia los bits, mismo buffer
  ✗ drop moved libera A, drop s intenta liberar A otra vez → double free
```

Cuándo usar cada uno:
- `(*p).clone()` → cuando quieres una copia independiente y `T: Clone`
- `ptr::read(p)` → cuando quieres **mover** el valor fuera del puntero y
  garantizas que la fuente no será usada ni destruida después (usas
  `mem::forget`, `ptr::write` para reemplazar, o gestionas manualmente)

En la práctica, `ptr::read` se usa en `pop()` de una lista: lees el dato del
nodo y luego liberas el nodo sin intentar drop del dato (porque ya lo moviste).

</details>

---

### Ejercicio 7 — Recorrido con offset negativo

Dado un puntero al **último** elemento de un array de 5 elementos, recorre
el array **hacia atrás** usando `.offset()` con valores negativos:

```rust
fn main() {
    let arr = [1, 2, 3, 4, 5];
    let end: *const i32 = unsafe { arr.as_ptr().add(4) };  // apunta a 5

    // Imprime: 5 4 3 2 1
    // Tu código aquí
}
```

**Prediccion**: `.offset(-5)` desde `end` es válido? Y `.offset(-6)`?

<details><summary>Respuesta</summary>

```rust
fn main() {
    let arr = [1, 2, 3, 4, 5];
    let end: *const i32 = unsafe { arr.as_ptr().add(4) };

    unsafe {
        for i in 0..5 {
            print!("{} ", *end.offset(-(i as isize)));
        }
        println!();
        // Salida: 5 4 3 2 1
    }
}
```

- `.offset(-4)` desde `end` apunta al primer elemento (`arr[0]`) → **válido**
- `.offset(-5)` apunta una posición **antes** del array. No existe regla de
  "one-before-start" como existe la de one-past-end → **UB** incluso sin
  desreferenciar
- `.offset(-6)` → **UB**, aún más fuera de rango

La asimetría: puedes apuntar a one-past-end (para iteradores de rango), pero
**no** puedes apuntar a one-before-start. Esta regla viene de C y se mantiene
en Rust.

</details>

---

### Ejercicio 8 — Comparación de punteros

Predice la salida de este programa:

```rust
use std::ptr;

fn main() {
    let a = 42;
    let b = 42;
    let c = &a;

    let p1: *const i32 = &a;
    let p2: *const i32 = &b;
    let p3: *const i32 = c;
    let p4: *const i32 = ptr::null();

    println!("p1 == p2: {}", p1 == p2);
    println!("p1 == p3: {}", p1 == p3);
    println!("p4.is_null(): {}", p4.is_null());
    println!("p1.is_null(): {}", p1.is_null());

    unsafe {
        println!("*p1 == *p2: {}", *p1 == *p2);
    }
}
```

**Prediccion**: `p1 == p2` es `true` o `false`? Y `*p1 == *p2`?

<details><summary>Respuesta</summary>

```
p1 == p2: false
p1 == p3: true
p4.is_null(): true
p1.is_null(): false
*p1 == *p2: true
```

- `p1 == p2`: **false** — `a` y `b` son variables distintas con direcciones
  distintas, aunque tengan el mismo valor. La comparación de punteros compara
  **direcciones**, no valores.

- `p1 == p3`: **true** — `c` es una referencia a `a`, y `p3` se crea desde `c`.
  Apuntan a la misma dirección.

- `*p1 == *p2`: **true** — aquí desreferenciamos ambos punteros y comparamos
  los **valores** (42 == 42).

Regla: `==` entre raw pointers compara direcciones. Para comparar valores,
siempre desreferenciar primero.

</details>

---

### Ejercicio 9 — Stack completo con raw pointers

Completa la implementación de un stack genérico con raw pointers. Incluye
`push`, `pop`, `peek`, `len`, `is_empty`, y `Drop`:

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct Stack<T> {
    head: *mut Node<T>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        todo!()
    }

    pub fn push(&mut self, data: T) {
        todo!()
    }

    pub fn pop(&mut self) -> Option<T> {
        todo!()
    }

    pub fn peek(&self) -> Option<&T> {
        todo!()
    }

    pub fn len(&self) -> usize {
        todo!()
    }

    pub fn is_empty(&self) -> bool {
        todo!()
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) {
        todo!()
    }
}

fn main() {
    let mut s = Stack::new();
    s.push(1);
    s.push(2);
    s.push(3);
    assert_eq!(s.peek(), Some(&3));
    assert_eq!(s.len(), 3);
    assert_eq!(s.pop(), Some(3));
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert_eq!(s.pop(), None);
    assert!(s.is_empty());
    println!("All tests passed!");
}
```

**Prediccion**: En `pop`, por qué necesitas `ptr::read` en vez de simplemente
`(*node).data`?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

struct Node<T> {
    data: T,
    next: *mut Node<T>,
}

pub struct Stack<T> {
    head: *mut Node<T>,
    len: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack {
            head: ptr::null_mut(),
            len: 0,
        }
    }

    pub fn push(&mut self, data: T) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let old_head = self.head;
            self.head = (*old_head).next;
            self.len -= 1;
            // Retomar ownership del nodo completo
            let node = Box::from_raw(old_head);
            Some(node.data)
        }
    }

    pub fn peek(&self) -> Option<&T> {
        if self.head.is_null() {
            None
        } else {
            unsafe { Some(&(*self.head).data) }
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_null()
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}

fn main() {
    let mut s = Stack::new();
    s.push(1);
    s.push(2);
    s.push(3);
    assert_eq!(s.peek(), Some(&3));
    assert_eq!(s.len(), 3);
    assert_eq!(s.pop(), Some(3));
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert_eq!(s.pop(), None);
    assert!(s.is_empty());
    println!("All tests passed!");
}
```

Sobre la predicción: en este caso usamos `Box::from_raw` para retomar el
ownership del nodo completo, y luego accedemos a `node.data` — esto mueve
el dato fuera del `Box` antes de que el `Box` se destruya. El `Box` solo
libera la memoria del nodo, no del dato (porque fue movido).

Si intentaras `(*old_head).data` directamente, estarías moviendo `data` fuera
de memoria que nadie va a liberar después — el nodo quedaría leaked. Al usar
`Box::from_raw`, el nodo se libera al salir del scope del `let node`.

La alternativa con `ptr::read` sería:

```rust
let data = ptr::read(&(*old_head).data);
// Luego liberar la memoria del nodo sin hacer drop de data:
let layout = std::alloc::Layout::new::<Node<T>>();
std::alloc::dealloc(old_head as *mut u8, layout);
Some(data)
```

Pero `Box::from_raw` + mover `node.data` es más simple e idiomático.

</details>

---

### Ejercicio 10 — Detectar errores con Miri

Cada uno de estos fragmentos tiene un bug. Identifica cuál es **antes** de
ejecutar, y luego verifica con `cargo miri run`:

```rust
// Bug A: ¿cuál es el problema?
fn bug_a() {
    let mut v = vec![1, 2, 3];
    let p: *mut i32 = v.as_mut_ptr();
    v.push(4);   // ← realloc potencial
    unsafe {
        println!("{}", *p);
    }
}

// Bug B: ¿cuál es el problema?
fn bug_b() {
    let p: *mut i32 = Box::into_raw(Box::new(42));
    unsafe {
        drop(Box::from_raw(p));
        drop(Box::from_raw(p));   // ???
    }
}

// Bug C: ¿cuál es el problema?
fn bug_c() {
    let arr = [1, 2, 3];
    let p = arr.as_ptr();
    unsafe {
        println!("{}", *p.add(3));
    }
}
```

**Prediccion**: Para cada uno, di si el error es use-after-free, double free,
o out-of-bounds.

<details><summary>Respuesta</summary>

**Bug A — Use-after-free (invalidación por realloc)**

`v.push(4)` puede disparar una reallocation del `Vec` si no tiene capacidad
suficiente. Cuando eso ocurre, los datos se mueven a un nuevo buffer y el
anterior se libera. `p` sigue apuntando al buffer antiguo → dangling.

```
Antes del push:  p → [1, 2, 3]  (buffer en 0x1000)
Después del push: Vec → [1, 2, 3, 4]  (buffer en 0x2000)
                  p → [freed]  (0x1000 ya no es válido)
```

Este es uno de los bugs más sutiles con raw pointers + `Vec`.

**Bug B — Double free**

`Box::from_raw(p)` retoma ownership y libera la memoria cuando el `Box` se
destruye. La segunda llamada intenta liberar la misma memoria otra vez.

```
Primera from_raw: libera 0x1000 ✓
Segunda from_raw: libera 0x1000 otra vez ✗ → double free
```

**Bug C — Out-of-bounds (desreferencia de one-past-end)**

`arr` tiene 3 elementos (índices 0, 1, 2). `p.add(3)` apunta a one-past-end,
que es válido para aritmética pero **no para desreferenciar**. `*p.add(3)` lee
memoria fuera del array.

Resumen:

| Bug | Tipo | Miri detecta? |
|-----|------|---------------|
| A | Use-after-free | Sí |
| B | Double free | Sí |
| C | Out-of-bounds | Sí |

Los tres son bugs que existen idénticamente en C con `malloc`/`free` y
aritmética de punteros. La diferencia es que en Rust tienes Miri para
detectarlos automáticamente.

</details>
