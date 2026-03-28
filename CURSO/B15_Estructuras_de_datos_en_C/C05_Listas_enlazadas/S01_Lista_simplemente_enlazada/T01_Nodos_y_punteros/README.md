# Nodos y punteros

## Qué es una lista enlazada

Una **lista enlazada** (*linked list*) es una colección de elementos donde cada
elemento (llamado **nodo**) contiene un dato y un **puntero** al siguiente nodo.
Los nodos no están contiguos en memoria — cada uno se asigna individualmente en
el heap y se conecta al siguiente mediante su puntero.

```
 head
  ↓
┌──────────┐     ┌──────────┐     ┌──────────┐
│ data: 10 │     │ data: 20 │     │ data: 30 │
│ next: ───┼────▶│ next: ───┼────▶│ next: NULL│
└──────────┘     └──────────┘     └──────────┘
 Nodo 1           Nodo 2           Nodo 3
```

- **head**: puntero al primer nodo.  Es lo único que la estructura "principal"
  necesita almacenar.
- **NULL** al final: indica que no hay más nodos.
- Si `head == NULL`, la lista está vacía.

### Contraste con el array

| Aspecto | Array | Lista enlazada |
|---------|-------|----------------|
| Memoria | Bloque contiguo | Nodos dispersos en el heap |
| Acceso por índice | $O(1)$ — aritmética de punteros | $O(n)$ — recorrer desde head |
| Insertar al inicio | $O(n)$ — desplazar todo | $O(1)$ — redirigir head |
| Insertar al final | $O(1)$ amortizado (con Vec) | $O(n)$ — recorrer hasta el final (o $O(1)$ con puntero tail) |
| Insertar en medio | $O(n)$ — desplazar | $O(1)$ — si ya tienes puntero al nodo anterior |
| Eliminar | $O(n)$ — desplazar | $O(1)$ — redirigir punteros |
| Espacio extra | Ninguno (o sobreasignación) | Un puntero por nodo (8 bytes en 64-bit) |
| Localidad de caché | Excelente | Pobre |

La lista enlazada sacrifica acceso aleatorio y localidad de caché a cambio de
**inserción y eliminación eficientes** en cualquier posición (si se tiene
puntero al nodo).

---

## El struct Node en C

```c
typedef struct Node {
    int data;
    struct Node *next;
} Node;
```

### ¿Por qué `struct Node *next` y no `Node *next`?

Dentro de la definición de `struct Node`, el nombre `Node` (del typedef) aún
no existe — el typedef no se completa hasta el cierre de la llave.  Pero
`struct Node` (la etiqueta de la struct) sí existe desde la apertura.  Por eso
el campo `next` debe ser `struct Node *next`.

Después de la definición, ambos nombres son válidos:

```c
Node *p;            /* correcto — typedef ya completado */
struct Node *q;     /* también correcto */
```

### Autorreferencia

`Node` contiene un puntero a otro `Node` — esto es una **estructura
autorreferencial**.  No es recursión infinita porque el campo es un *puntero*
(tamaño fijo: 8 bytes), no una instancia completa (tamaño infinito):

```c
/* Imposible — tamaño infinito */
typedef struct Bad {
    int data;
    struct Bad next;    /* ERROR: struct contiene instancia de sí misma */
} Bad;

/* Correcto — puntero tiene tamaño fijo */
typedef struct Good {
    int data;
    struct Good *next;  /* OK: solo un puntero (8 bytes) */
} Good;
```

### Tamaño del nodo

En un sistema de 64 bits:

```
Node:
  ┌─────────────────────┐
  │ data: int   (4 bytes)│
  │ padding     (4 bytes)│  ← alineación para el puntero
  │ next: Node* (8 bytes)│
  └─────────────────────┘
  Total: 16 bytes
```

`sizeof(Node)` es típicamente 16 (no 12), porque el puntero de 8 bytes
requiere alineación a 8.  El compilador inserta 4 bytes de **padding** entre
`data` y `next`.

Para minimizar padding, se pueden reordenar los campos o usar tipos del
mismo tamaño:

```c
typedef struct NodeCompact {
    struct NodeCompact *next;   /* 8 bytes — primero */
    int data;                   /* 4 bytes */
    /* 4 bytes padding al final (para alineación de arrays) */
} NodeCompact;
/* sizeof: 16 — igual, pero el padding está al final, no en medio */
```

En la práctica, 16 bytes por nodo es el mínimo para datos de tipo `int`.

---

## Visualización en memoria

### Array vs lista enlazada en el heap

```
Array (contiguo):
  Dirección   0x1000  0x1004  0x1008  0x100C
              ┌───────┬───────┬───────┬───────┐
              │  10   │  20   │  30   │  40   │
              └───────┴───────┴───────┴───────┘
              arr[0]   arr[1]   arr[2]   arr[3]

  Acceso: arr[2] = *(arr + 2) = dirección 0x1008  → O(1)


Lista enlazada (dispersa):
  Variable head = 0x2040

  Dirección   0x2040          0x20A0          0x2010
              ┌──────────┐    ┌──────────┐    ┌──────────┐
              │ data: 10 │    │ data: 20 │    │ data: 30 │
              │ next:20A0│───▶│ next:2010│───▶│ next:NULL│
              └──────────┘    └──────────┘    └──────────┘

  Acceso al 3er elemento:
    head → 0x2040 (10) → 0x20A0 (20) → 0x2010 (30)  → O(n)
```

Notar que los nodos no están en orden de dirección (0x2040, 0x20A0, 0x2010) —
`malloc` los coloca donde haya espacio.  El orden lógico está determinado
únicamente por los punteros `next`.

---

## Crear un nodo

```c
#include <stdlib.h>

Node *create_node(int value) {
    Node *node = malloc(sizeof(Node));
    if (!node) return NULL;    /* fallo de asignación */
    node->data = value;
    node->next = NULL;
    return node;
}
```

### El operador `->`

`node->data` es azúcar sintáctico para `(*node).data`:

```c
Node *node = malloc(sizeof(Node));
(*node).data = 42;       /* desreferenciar, luego acceder al campo */
node->data = 42;         /* equivalente — más legible */
```

### malloc(sizeof(Node)) vs malloc(sizeof(*node))

```c
Node *node = malloc(sizeof(Node));     /* correcto */
Node *node = malloc(sizeof(*node));    /* también correcto, más robusto */
```

La segunda forma es inmune a cambios de tipo — si mañana `node` pasa de
`Node *` a `BigNode *`, la asignación sigue siendo correcta.

---

## Construir una lista manualmente

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

int main(void) {
    /* Crear tres nodos */
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    Node *c = malloc(sizeof(Node));

    /* Asignar datos */
    a->data = 10;
    b->data = 20;
    c->data = 30;

    /* Enlazar */
    a->next = b;
    b->next = c;
    c->next = NULL;

    /* head apunta al primero */
    Node *head = a;

    /* Recorrer */
    Node *current = head;
    while (current != NULL) {
        printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");   /* 10 20 30 */

    /* Liberar */
    free(a);
    free(b);
    free(c);

    return 0;
}
```

### El patrón de recorrido

```c
Node *current = head;
while (current != NULL) {
    /* procesar current->data */
    current = current->next;
}
```

Este patrón aparece en **todas** las operaciones de listas: imprimir, buscar,
contar, insertar al final, etc.  `current = current->next` es el equivalente
de `i++` en un array — avanza al siguiente elemento.

---

## La lista vacía

Una lista vacía se representa con `head == NULL`:

```c
Node *head = NULL;     /* lista vacía */

if (head == NULL) {
    printf("Lista vacía\n");
}
```

Esta convención simplifica la lógica: el final de cualquier lista se detecta
con `current == NULL`, y una lista vacía es simplemente el caso donde el
primer nodo ya es `NULL`.

No confundir:
- `head == NULL` → lista vacía (0 nodos).
- `head->next == NULL` → lista con **un** nodo.

---

## Nodos en el stack vs en el heap

Los nodos de una lista deben estar en el **heap** (con `malloc`), no en el
stack:

```c
/* MAL — nodos en el stack */
void bad_example(void) {
    Node a = { .data = 10, .next = NULL };
    Node b = { .data = 20, .next = NULL };
    a.next = &b;

    /* head apunta a &a */
}   /* ← a y b se destruyen al salir de la función */
    /* head ahora es un dangling pointer */

/* BIEN — nodos en el heap */
void good_example(void) {
    Node *a = create_node(10);
    Node *b = create_node(20);
    a->next = b;
    /* Los nodos persisten después de esta función */
    /* (el llamador debe guardar el puntero y eventualmente free) */
}
```

Excepción: si los nodos solo se necesitan dentro de una función y no escapan,
pueden estar en el stack.  Pero esto es raro — las listas son estructuras
dinámicas que crecen y decrecen.

---

## Equivalente en Rust

### El nodo

```rust
struct Node<T> {
    data: T,
    next: Option<Box<Node<T>>>,
}
```

- `Box<Node<T>>`: nodo en el heap (como `malloc` en C).
- `Option<...>`: puede ser `Some(nodo)` o `None` (como `NULL` en C).
- `T`: genérico — la lista funciona con cualquier tipo.

### Construir una lista manualmente

```rust
fn main() {
    let list = Some(Box::new(Node {
        data: 10,
        next: Some(Box::new(Node {
            data: 20,
            next: Some(Box::new(Node {
                data: 30,
                next: None,
            })),
        })),
    }));

    // Recorrer
    let mut current = &list;
    while let Some(node) = current {
        print!("{} ", node.data);
        current = &node.next;
    }
    println!();   // 10 20 30
}
```

### Comparación de representación

| Concepto | C | Rust |
|----------|---|------|
| Nodo en heap | `Node *p = malloc(sizeof(Node))` | `Box::new(Node { ... })` |
| Puntero nulo | `NULL` | `None` |
| Puntero válido | `p` (dirección) | `Some(Box<Node>)` |
| Acceso al dato | `p->data` | `node.data` |
| Siguiente nodo | `p->next` | `node.next` |
| Tipo del campo next | `struct Node *` | `Option<Box<Node<T>>>` |
| Liberar | `free(p)` — manual | Automático (`Drop` de `Box`) |
| Nulo accidental | Posible (UB si se desreferencia) | Imposible (compilador obliga `match`/`if let`) |

### Tamaño en Rust

```rust
use std::mem::size_of;

// Box<Node<i32>>: solo un puntero (8 bytes)
// Option<Box<Node<i32>>>: también 8 bytes (optimización de nicho nulo)
println!("{}", size_of::<Option<Box<Node<i32>>>>());   // 8
```

Rust optimiza `Option<Box<T>>` para que `None` se represente como el puntero
nulo, sin overhead — exactamente como `NULL` en C.  Esta **null pointer
optimization** (NPO) hace que `Option<Box<T>>` tenga el mismo tamaño que
`Box<T>` (8 bytes), no 8 + 1.

---

## Ownership en la lista

En C, los punteros no tienen semántica de ownership — el programador decide
quién es responsable de liberar cada nodo.  En Rust, el ownership es explícito:

```
head: Option<Box<Node>>
         │
         ▼ (owns)
    Box<Node { data: 10, next: ─── }>
                                │
                                ▼ (owns)
                          Box<Node { data: 20, next: ─── }>
                                                      │
                                                      ▼ (owns)
                                                Box<Node { data: 30, next: None }>
```

Cada `Box` **posee** el siguiente nodo.  Cuando se destruye el head, se
destruye toda la cadena automáticamente:

```rust
{
    let list = Some(Box::new(Node { data: 10, next: /* ... */ }));
}   // Drop se ejecuta aquí: destruye el Box, que destruye su Node,
    // que destruye el campo next (otro Box), recursivamente.
```

En C, olvidar `free` en cualquier punto de la cadena causa un memory leak.  En
Rust, `Drop` recorre la cadena completa.  (Para listas muy largas, esto puede
causar stack overflow por la recursión del Drop — se soluciona con un `Drop`
manual iterativo.)

---

## Por qué estudiar listas enlazadas

Las listas enlazadas son menos eficientes que los arrays/vectores para la
mayoría de las tareas en hardware moderno (debido a la pobre localidad de
caché).  Entonces, ¿por qué estudiarlas?

1. **Fundamento de punteros**: son el ejercicio definitivo para entender
   punteros, `malloc`/`free`, y estructuras autorreferenciales.

2. **Base de otras estructuras**: árboles, grafos, tablas hash (encadenamiento),
   skip lists — todas usan nodos enlazados.

3. **Inserción/eliminación en medio**: $O(1)$ si tienes puntero al nodo, vs
   $O(n)$ en arrays.  Útil para LRU caches, editores de texto (buffers de
   líneas), allocators.

4. **Caso de estudio Rust vs C**: la lista doblemente enlazada es el ejemplo
   clásico donde el ownership lineal de Rust se vuelve incómodo y C es más
   natural.

5. **Ubicuidad en kernels**: el kernel Linux usa listas intrusivas doblemente
   enlazadas (`list_head`) en cientos de subsistemas.

---

## Errores comunes con nodos y punteros

### 1. Desreferenciar NULL

```c
Node *head = NULL;
printf("%d\n", head->data);   /* SIGSEGV — crash */
```

Siempre verificar `if (head != NULL)` antes de acceder.

### 2. Memory leak (olvidar free)

```c
Node *node = create_node(42);
node = create_node(99);   /* el nodo con 42 se perdió — leak */
```

### 3. Use-after-free

```c
Node *node = create_node(42);
free(node);
printf("%d\n", node->data);   /* UB — el nodo ya fue liberado */
```

### 4. sizeof incorrecto

```c
Node *node = malloc(sizeof(Node*));   /* MAL: 8 bytes (un puntero) */
Node *node = malloc(sizeof(Node));    /* BIEN: 16 bytes (un nodo) */
```

### 5. No inicializar next

```c
Node *node = malloc(sizeof(Node));
node->data = 42;
/* node->next no inicializado — basura */
/* Recorrer esta lista podría seguir un puntero basura → crash */
```

Siempre: `node->next = NULL;` después de `malloc`.

---

## Ejercicios

### Ejercicio 1 — Crear y recorrer

Crea manualmente una lista de 5 nodos con los valores [10, 20, 30, 40, 50].
Recórrela e imprime cada valor.  Luego libera todos los nodos.

<details>
<summary>Estructura del código</summary>

```c
Node *head = create_node(10);
head->next = create_node(20);
head->next->next = create_node(30);
/* ... */
```

Para liberar: recorrer con `current` y `next`, `free(current)` en cada paso.
No usar `current->next` después de `free(current)`.
</details>

### Ejercicio 2 — sizeof y padding

Escribe un programa que imprima `sizeof(Node)` para estas variantes:

```c
struct A { int data; struct A *next; };
struct B { char data; struct B *next; };
struct C { double data; struct C *next; };
struct D { char data; int extra; struct D *next; };
```

<details>
<summary>Predicciones (64-bit)</summary>

- `A`: int(4) + pad(4) + ptr(8) = **16**.
- `B`: char(1) + pad(7) + ptr(8) = **16**.
- `C`: double(8) + ptr(8) = **16** (sin padding — ambos alineados a 8).
- `D`: char(1) + pad(3) + int(4) + ptr(8) = **16**.

En sistemas de 64 bits, casi todos los nodos simples son de 16 bytes por la
alineación del puntero a 8 bytes.
</details>

### Ejercicio 3 — Dibujar la memoria

Dado el siguiente código, dibuja el estado del heap con direcciones ficticias:

```c
Node *a = create_node(5);    /* supón dirección 0x100 */
Node *b = create_node(10);   /* supón dirección 0x120 */
Node *c = create_node(15);   /* supón dirección 0x110 */
a->next = c;
c->next = b;
Node *head = a;
```

<details>
<summary>Diagrama</summary>

```
head = 0x100

0x100: [data=5,  next=0x110]  → nodo a
0x110: [data=15, next=0x120]  → nodo c
0x120: [data=10, next=NULL]   → nodo b

Orden lógico: 5 → 15 → 10
Orden de dirección: 0x100, 0x110, 0x120
```

El orden lógico (5, 15, 10) no coincide con el orden de creación (5, 10, 15)
ni con el orden de direcciones.  Solo los punteros `next` definen la secuencia.
</details>

### Ejercicio 4 — Detectar errores

Identifica todos los errores en este código:

```c
Node *build_list(void) {
    Node a;
    a.data = 10;
    a.next = NULL;

    Node *b = malloc(sizeof(Node*));
    b->data = 20;
    a.next = b;

    return &a;
}
```

<details>
<summary>Errores</summary>

1. `Node a` está en el stack — al retornar, `&a` es un dangling pointer.
2. `malloc(sizeof(Node*))` solo asigna 8 bytes — debería ser `sizeof(Node)`.
3. `b->next` no se inicializa — basura.
4. Se retorna `&a` que apunta al stack — UB al usarlo fuera de la función.
</details>

### Ejercicio 5 — Contar nodos

Escribe una función `int list_length(Node *head)` que retorne el número de
nodos en la lista.

<details>
<summary>Implementación</summary>

```c
int list_length(Node *head) {
    int count = 0;
    Node *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}
```

Para lista vacía (`head == NULL`): retorna 0.  $O(n)$ — debe recorrer toda la
lista.
</details>

### Ejercicio 6 — Lista en Rust

Construye una lista de 4 nodos [1, 2, 3, 4] en Rust usando `Option<Box<Node<i32>>>`.
Recórrela con `while let` e imprime cada valor.

<details>
<summary>Pista</summary>

Construir de atrás hacia adelante es más natural en Rust (ownership lineal):

```rust
let mut list: Option<Box<Node<i32>>> = None;
for &val in [4, 3, 2, 1].iter() {
    list = Some(Box::new(Node { data: val, next: list }));
}
```

Cada nuevo nodo toma ownership del anterior mediante `next: list`.
</details>

### Ejercicio 7 — Null pointer optimization

Verifica en Rust que `size_of::<Option<Box<Node<i32>>>>()` es igual a
`size_of::<Box<Node<i32>>>()` (ambos 8 bytes).  Explica por qué esta
optimización es posible.

<details>
<summary>Explicación</summary>

`Box<T>` es un puntero no nulo (Rust garantiza que `Box` siempre apunta a
memoria válida).  `Option<Box<T>>` puede ser `None`, que se representa como
el puntero nulo (0x0).  Como `Box` nunca es nulo, el compilador usa el
valor 0 para representar `None` sin añadir un byte extra.  Esta optimización
aplica a `Box`, `&T`, `&mut T`, y otros NonNull wrappers.
</details>

### Ejercicio 8 — Liberar la lista

Escribe una función `void list_free(Node *head)` que libere todos los nodos.
¿Por qué no puedes simplemente hacer `free(head)`?

<details>
<summary>Implementación</summary>

```c
void list_free(Node *head) {
    Node *current = head;
    while (current != NULL) {
        Node *next = current->next;   /* guardar ANTES de free */
        free(current);
        current = next;
    }
}
```

`free(head)` solo libera el primer nodo — los demás quedan en el heap sin
referencia (memory leak).  Hay que recorrer y liberar uno a uno.  Guardar
`next` antes de `free` es esencial — acceder a `current->next` después de
`free(current)` es use-after-free.
</details>

### Ejercicio 9 — Nodo con string

Define un nodo que almacene un string dinámico:

```c
typedef struct SNode {
    char *text;         /* string en el heap (strdup) */
    struct SNode *next;
} SNode;
```

Escribe `create_snode(const char *text)` y `snode_free(SNode *node)` que
libere tanto el string como el nodo.

<details>
<summary>Pista</summary>

```c
SNode *create_snode(const char *text) {
    SNode *node = malloc(sizeof(SNode));
    if (!node) return NULL;
    node->text = strdup(text);   /* copia el string al heap */
    if (!node->text) { free(node); return NULL; }
    node->next = NULL;
    return node;
}

void snode_free(SNode *node) {
    free(node->text);   /* liberar el string primero */
    free(node);          /* luego el nodo */
}
```

Orden de liberación: **primero** los recursos internos (`text`), **luego** el
contenedor (`node`).  Si haces `free(node)` primero, `node->text` es
use-after-free.
</details>

### Ejercicio 10 — Drop recursivo en Rust

Crea una lista de 100,000 nodos en Rust.  ¿Se destruye correctamente al salir
del scope?  Para listas muy largas ($> 100,000$), el `Drop` recursivo de `Box`
puede causar stack overflow.  Implementa un `Drop` manual iterativo para
evitarlo.

<details>
<summary>Drop iterativo</summary>

```rust
impl<T> Drop for Node<T> {
    fn drop(&mut self) {
        let mut current = self.next.take();
        while let Some(mut node) = current {
            current = node.next.take();
            // node se destruye aquí (Box::drop), pero su next ya es None
            // → no hay recursión
        }
    }
}
```

`take()` reemplaza el campo con `None` y retorna el valor previo.  Al hacer
`node.next.take()` antes de que `node` se destruya, el `Drop` de `node` no
recursará — su `next` ya es `None`.  Profundidad de stack: $O(1)$.
</details>
