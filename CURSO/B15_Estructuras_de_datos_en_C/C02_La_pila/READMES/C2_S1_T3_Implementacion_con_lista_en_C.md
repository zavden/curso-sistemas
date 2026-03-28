# T03 — Implementación con lista enlazada en C

---

## 1. La idea

En vez de un array contiguo, cada elemento vive en su propio **nodo** asignado
con `malloc`. Cada nodo contiene el dato y un puntero al nodo de abajo:

```
Array:
  [ 10 | 20 | 30 ]    ← contiguo en memoria, top = 3

Lista enlazada:
  top → [30|→] → [20|→] → [10|NULL]
        nodo3     nodo2     nodo1

  Push = insertar al inicio (nuevo nodo apunta al top actual)
  Pop  = eliminar del inicio (top avanza al siguiente)
```

El top de la pila es siempre el **primer nodo** de la lista. No hace falta
recorrer nada — push y pop operan solo en la cabeza.

---

## 2. El nodo y el struct

```c
typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *top;      // puntero al primer nodo (tope de la pila)
    size_t size;    // cantidad de elementos
} Stack;
```

### Layout en memoria

```
Stack s:
  ┌───────────┬──────┐
  │ top (*)───┼──┐   │ size   │
  └───────────┴──┼───┘
                 │
                 ▼
  heap:  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
         │ data: 30     │    │ data: 20     │    │ data: 10     │
         │ next: ───────┼──►│ next: ───────┼──►│ next: NULL   │
         └──────────────┘    └──────────────┘    └──────────────┘
         nodo más reciente                       nodo más antiguo
```

Cada nodo es una asignación independiente de `malloc`. No están necesariamente
contiguos en memoria.

---

## 3. Implementación completa

### stack.h

```c
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

Stack  *stack_create(void);
void    stack_destroy(Stack *s);
bool    stack_push(Stack *s, int elem);
bool    stack_pop(Stack *s, int *out);
bool    stack_peek(const Stack *s, int *out);
bool    stack_is_empty(const Stack *s);
size_t  stack_size(const Stack *s);

#endif
```

Nota: `stack_create` no recibe `capacity` — la lista no tiene límite fijo.

### stack.c

```c
#include "stack.h"
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

struct Stack {
    Node *top;
    size_t size;
};

Stack *stack_create(void) {
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;
    s->top = NULL;
    s->size = 0;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    Node *current = s->top;
    while (current) {
        Node *next = current->next;
        free(current);
        current = next;
    }
    free(s);
}

bool stack_push(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;

    node->data = elem;
    node->next = s->top;   // nuevo nodo apunta al top actual
    s->top = node;          // top ahora es el nuevo nodo
    s->size++;
    return true;
}

bool stack_pop(Stack *s, int *out) {
    if (!s->top) return false;

    Node *old_top = s->top;
    *out = old_top->data;
    s->top = old_top->next;   // top avanza al siguiente
    free(old_top);
    s->size--;
    return true;
}

bool stack_peek(const Stack *s, int *out) {
    if (!s->top) return false;
    *out = s->top->data;
    return true;
}

bool stack_is_empty(const Stack *s) {
    return s->top == NULL;
}

size_t stack_size(const Stack *s) {
    return s->size;
}
```

### main.c

```c
#include <stdio.h>
#include "stack.h"

int main(void) {
    Stack *s = stack_create();

    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);

    int val;
    stack_peek(s, &val);
    printf("peek: %d\n", val);            // 30
    printf("size: %zu\n", stack_size(s));  // 3

    while (stack_pop(s, &val)) {
        printf("pop: %d\n", val);
    }
    // pop: 30, 20, 10

    printf("empty: %d\n", stack_is_empty(s)); // 1

    stack_destroy(s);
    return 0;
}
```

---

## 4. Anatomía de push y pop

### push — insertar al inicio

```c
bool stack_push(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));    // 1. Crear nodo en el heap
    if (!node) return false;

    node->data = elem;                    // 2. Guardar el dato
    node->next = s->top;                  // 3. Enlazar al top actual
    s->top = node;                        // 4. El nuevo nodo es el top
    s->size++;
    return true;
}
```

Visualización paso a paso:

```
Estado inicial:
  top → [20|→] → [10|NULL]

push(30):

  1. malloc → nuevo nodo [30|???]

  2. node->data = 30
     [30|???]

  3. node->next = s->top
     [30|→] → [20|→] → [10|NULL]
              ↑
              s->top todavía apunta aquí

  4. s->top = node
     top → [30|→] → [20|→] → [10|NULL]
```

El orden de los pasos 3 y 4 importa: si hicieras `s->top = node` antes de
`node->next = s->top`, perderías la referencia al nodo anterior.

### pop — eliminar del inicio

```c
bool stack_pop(Stack *s, int *out) {
    if (!s->top) return false;

    Node *old_top = s->top;           // 1. Guardar puntero al nodo actual
    *out = old_top->data;             // 2. Extraer el dato
    s->top = old_top->next;           // 3. Avanzar top al siguiente
    free(old_top);                    // 4. Liberar el nodo antiguo
    s->size--;
    return true;
}
```

Visualización:

```
Estado inicial:
  top → [30|→] → [20|→] → [10|NULL]

pop():

  1. old_top = s->top
     old_top → [30|→] → [20|→] → [10|NULL]
               ↑
               s->top

  2. *out = 30

  3. s->top = old_top->next
     old_top → [30|→]    top → [20|→] → [10|NULL]

  4. free(old_top)
                          top → [20|→] → [10|NULL]
```

El paso 1 es esencial: si hicieras `s->top = s->top->next` directamente y
luego intentaras `free(???)`, ya no tendrías el puntero al nodo antiguo.

---

## 5. Destroy — liberar todos los nodos

El destructor es un recorrido completo de la lista:

```c
void stack_destroy(Stack *s) {
    if (!s) return;
    Node *current = s->top;
    while (current) {
        Node *next = current->next;   // guardar next ANTES de free
        free(current);
        current = next;
    }
    free(s);
}
```

### Error típico: liberar sin guardar next

```c
// MAL: use-after-free
void stack_destroy_bad(Stack *s) {
    Node *current = s->top;
    while (current) {
        free(current);          // libera el nodo
        current = current->next; // ACCEDE a nodo ya liberado → UB
    }
    free(s);
}
```

Después de `free(current)`, el campo `current->next` está en memoria liberada.
Leerlo es use-after-free. Puede "funcionar" por casualidad si la memoria no fue
reasignada, o puede crashear.

### Visualización del destroy correcto

```
top → [30|→] → [20|→] → [10|NULL]

Iteración 1:
  current = [30|→]
  next = [20|→]          ← guardado ANTES del free
  free([30|→])
  current = [20|→]

Iteración 2:
  current = [20|→]
  next = [10|NULL]
  free([20|→])
  current = [10|NULL]

Iteración 3:
  current = [10|NULL]
  next = NULL
  free([10|NULL])
  current = NULL          ← fin del loop

free(s)                   ← liberar el struct
```

---

## 6. Comparación: array vs lista enlazada

| Aspecto | Array | Lista enlazada |
|---------|-------|---------------|
| Push | Escribe en `data[top++]` | `malloc` + enlazar |
| Pop | Lee `data[--top]` | Desenlazar + `free` |
| Costo de push | $O(1)$ ($O(n)$ si realloc) | $O(1)$ (siempre) |
| Costo de pop | $O(1)$ | $O(1)$ |
| Memoria por elemento | `sizeof(int)` | `sizeof(int) + sizeof(Node *)` |
| Cache locality | Excelente (contiguo) | Mala (disperso) |
| Límite de tamaño | Fijo (o realloc) | Solo RAM disponible |
| Overhead total | Puede desperdiciar capacidad no usada | Overhead por nodo |
| Create | 1 malloc (struct + array) | 1 malloc (struct) |
| Destroy | 1 free (array) + 1 free (struct) | N frees (nodos) + 1 free (struct) |

### Cuándo elegir cada uno

```
Array cuando:
  - Sabes el tamaño máximo aproximado
  - El rendimiento importa (cache locality)
  - Quieres implementación simple
  - La mayoría de los usos del stack

Lista enlazada cuando:
  - El tamaño es completamente impredecible
  - Necesitas O(1) estricto (sin reallocs)
  - Estás aprendiendo punteros y listas
  - El overhead por nodo es aceptable
```

### Overhead de memoria

Para almacenar 1000 enteros ($n = 1000$):

```
Array (capacidad justa):
  Struct:  sizeof(Stack) ≈ 24 bytes
  Array:   1000 × sizeof(int) = 4000 bytes
  Total:   ~4024 bytes

Lista enlazada (64-bit):
  Struct:  sizeof(Stack) ≈ 16 bytes
  Nodos:   1000 × (sizeof(int) + sizeof(Node *)) = 1000 × 12 bytes
           + padding = 1000 × 16 bytes = 16000 bytes
  Total:   ~16016 bytes
```

La lista usa ~4× más memoria que el array para `int`. La proporción mejora
para tipos más grandes: si cada elemento es un struct de 100 bytes, el
overhead del puntero (8 bytes) es solo 8% extra.

---

## 7. Rendimiento: malloc/free vs array write

El costo principal de la lista enlazada no es el puntero extra — es la
**llamada a malloc/free** en cada push/pop:

```
Array push:
  s->data[s->top++] = elem;
  → 1 escritura en memoria

Lista push:
  Node *node = malloc(sizeof(Node));   → llamada al allocator (costoso)
  node->data = elem;                   → 1 escritura
  node->next = s->top;                 → 1 escritura
  s->top = node;                       → 1 escritura
  → 3 escrituras + 1 malloc
```

`malloc` es ordenes de magnitud más lento que una escritura en array:

```
Escritura en array:          ~1-2 ns
malloc + free:               ~50-100 ns
Ratio:                       ~25-100×
```

Para aplicaciones donde push/pop es frecuente (miles de operaciones por
segundo), la diferencia es significativa.

### Cache locality

```
Array:
  push 10, push 20, push 30:
  Memoria: [10][20][30][__][__]...
  → Misma línea de cache → hits consecutivos → rápido

Lista:
  push 10, push 20, push 30:
  Memoria: [10|→] .......... [20|→] .... [30|→]
  → Direcciones arbitrarias → cache misses probables → lento
```

Los nodos de la lista están dispersos por el heap porque cada `malloc`
retorna la dirección que el allocator considere conveniente. Recorrer la
lista salta entre direcciones distantes, causando cache misses.

---

## 8. Versión genérica con void *

```c
typedef struct Node {
    void *data;          // puntero al dato (almacena una copia)
    struct Node *next;
} Node;

typedef struct {
    Node *top;
    size_t size;
    size_t elem_size;    // tamaño de cada elemento
} GenericStack;

GenericStack *gstack_create(size_t elem_size) {
    GenericStack *s = malloc(sizeof(GenericStack));
    if (!s) return NULL;
    s->top = NULL;
    s->size = 0;
    s->elem_size = elem_size;
    return s;
}

bool gstack_push(GenericStack *s, const void *elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;

    node->data = malloc(s->elem_size);
    if (!node->data) {
        free(node);
        return false;
    }

    memcpy(node->data, elem, s->elem_size);
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}

bool gstack_pop(GenericStack *s, void *out) {
    if (!s->top) return false;

    Node *old_top = s->top;
    memcpy(out, old_top->data, s->elem_size);
    s->top = old_top->next;
    free(old_top->data);
    free(old_top);
    s->size--;
    return true;
}

void gstack_destroy(GenericStack *s) {
    if (!s) return;
    Node *current = s->top;
    while (current) {
        Node *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    free(s);
}
```

### Overhead de la versión genérica

Cada push hace **2 mallocs** (nodo + copia del dato) y cada pop hace **2 frees**.
Esto duplica el overhead del allocator respecto a la versión con `int` directo.

Alternativa: almacenar el dato inline en el nodo usando un **flexible array
member**:

```c
typedef struct Node {
    struct Node *next;
    char data[];           // flexible array — tamaño variable
} Node;

bool gstack_push(GenericStack *s, const void *elem) {
    Node *node = malloc(sizeof(Node) + s->elem_size);  // 1 solo malloc
    if (!node) return false;
    memcpy(node->data, elem, s->elem_size);
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}
```

Esto reduce a 1 malloc por push y mejora la cache locality porque el dato
está contiguo al puntero `next`.

---

## 9. Errores comunes con listas

### Error 1: no guardar next antes de free

Ya cubierto en la sección 5. Es el error más común y más difícil de depurar
porque puede parecer que funciona.

### Error 2: memory leak — no liberar todos los nodos

```c
// MAL: solo libera el struct
void stack_destroy_bad(Stack *s) {
    free(s);  // todos los nodos quedan en el heap
}
```

Valgrind reporta:

```
LEAK SUMMARY:
   definitely lost: N bytes in N blocks
```

donde N es el número de nodos que no se liberaron.

### Error 3: perder la cabeza de la lista

```c
// MAL: sobreescribe top sin guardar referencia
bool stack_push_bad(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    node->data = elem;
    s->top = node;         // ← perdió la referencia al top anterior
    node->next = s->top;   // ← ahora node->next apunta a sí mismo (ciclo)
    s->size++;
    return true;
}
```

El orden correcto es: primero enlazar (`node->next = s->top`), luego
actualizar top (`s->top = node`).

### Error 4: desreferenciar NULL

```c
bool stack_pop_bad(Stack *s, int *out) {
    *out = s->top->data;     // si top es NULL → segfault
    Node *old = s->top;
    s->top = old->next;
    free(old);
    s->size--;
    return true;
}
// Falta la verificación: if (!s->top) return false;
```

### Error 5: off-by-one en size

```c
bool stack_push_bad(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    node->data = elem;
    node->next = s->top;
    s->top = node;
    // Olvidó s->size++
    return true;
}
// stack_size retorna valores incorrectos
```

---

## 10. Misma interfaz, diferente implementación

Un punto clave del TAD: el **header** (`stack.h`) es idéntico para la versión
array y la versión lista. El usuario no sabe (ni necesita saber) cuál se usa:

```c
// Este código funciona SIN CAMBIOS con ambas implementaciones:
Stack *s = stack_create();    // ← puede ser array o lista
stack_push(s, 10);
stack_push(s, 20);
int val;
stack_pop(s, &val);
stack_destroy(s);
```

La única diferencia observable es:
- Array: `stack_create(capacity)` recibe capacidad
- Lista: `stack_create(void)` no la necesita

Esto se puede unificar haciendo que el array ignore el parámetro cuando no
lo necesita, o que la lista lo acepte sin usarlo. Lo importante es que el
**comportamiento** es idéntico — los axiomas del TAD se cumplen en ambos casos.

### Cómo intercambiar implementaciones

```
Proyecto con array:
  stack.h    (interfaz)
  stack_array.c  (implementación con array)

Proyecto con lista:
  stack.h    (interfaz, la misma)
  stack_list.c   (implementación con lista)

Compilar:
  gcc -o prog main.c stack_array.c    ← usa array
  gcc -o prog main.c stack_list.c     ← usa lista
```

El `main.c` no cambia. Solo se cambia qué `.c` se linka.

---

## Ejercicios

### Ejercicio 1 — Implementar y verificar

Crea `stack.h`, `stack.c` (con lista) y `main.c`. Compila, ejecuta, y
verifica con Valgrind:

```bash
gcc -g -Wall -Wextra -std=c11 -o stack_list stack.c main.c
valgrind --leak-check=full ./stack_list
```

**Prediccion**: Si haces 3 pushes y 3 pops antes de destroy, cuántos allocs
y frees reporta Valgrind?

<details><summary>Respuesta</summary>

```
==12345== HEAP SUMMARY:
==12345==     total heap usage: 4 allocs, 4 frees
```

4 allocs:
1. `malloc(sizeof(Stack))` — el struct
2. `malloc(sizeof(Node))` — push(10)
3. `malloc(sizeof(Node))` — push(20)
4. `malloc(sizeof(Node))` — push(30)

4 frees:
1. `free(node)` — pop(30)
2. `free(node)` — pop(20)
3. `free(node)` — pop(10)
4. `free(s)` — stack_destroy

Si haces los 3 pops, destroy no libera nodos (la lista ya está vacía),
solo libera el struct. Total: 3 allocs de nodo + 1 alloc de struct = 4.

Si **no** haces pop antes de destroy, Valgrind reporta los mismos 4 allocs
y 4 frees — pero los 3 frees de nodos vienen de `stack_destroy` en vez de
`stack_pop`.

</details>

---

### Ejercicio 2 — Visualizar la memoria

Modifica `stack_push` y `stack_pop` para que impriman la dirección del nodo
que se crea o libera. Observa que las direcciones no son consecutivas:

```c
bool stack_push(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    printf("  push: node at %p, data = %d\n", (void *)node, elem);
    // ...
}
```

**Prediccion**: Las direcciones están separadas por exactamente `sizeof(Node)`
bytes?

<details><summary>Respuesta</summary>

```c
bool stack_push(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    printf("  push: node at %p, data = %d\n", (void *)node, elem);
    node->data = elem;
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}

bool stack_pop(Stack *s, int *out) {
    if (!s->top) return false;
    Node *old_top = s->top;
    printf("  pop:  node at %p, data = %d\n", (void *)old_top, old_top->data);
    *out = old_top->data;
    s->top = old_top->next;
    free(old_top);
    s->size--;
    return true;
}
```

Salida típica:

```
  push: node at 0x5575a3e002a0, data = 10
  push: node at 0x5575a3e002c0, data = 20
  push: node at 0x5575a3e002e0, data = 30
```

No, las direcciones **no** están separadas por exactamente `sizeof(Node)`.
El allocator agrega metadatos (tamaño del bloque, flags) entre asignaciones.
En este ejemplo la separación es 0x20 (32 bytes), pero `sizeof(Node)` es
probablemente 16 bytes (4 data + 4 padding + 8 next en 64-bit).

Los 16 bytes extra son overhead del allocator (`malloc` header).

Además, si hay frees y mallocs intercalados, el allocator puede reusar
bloques liberados, haciendo que las direcciones sean aún más impredecibles.

</details>

---

### Ejercicio 3 — Bug: orden incorrecto en push

Este push tiene un bug sutil. Encuéntralo y explica qué pasa:

```c
bool stack_push_buggy(Stack *s, int elem) {
    Node *node = malloc(sizeof(Node));
    if (!node) return false;
    node->data = elem;
    s->top = node;           // ← línea A
    node->next = s->top;     // ← línea B
    s->size++;
    return true;
}
```

**Prediccion**: Con push(10), push(20), push(30) seguido de 3 pops, qué
valores salen?

<details><summary>Respuesta</summary>

El bug: las líneas A y B están **invertidas**.

```c
s->top = node;           // A: top ahora apunta al nuevo nodo
node->next = s->top;     // B: node->next = node (¡apunta a sí mismo!)
```

En la línea B, `s->top` ya fue actualizado a `node`, así que `node->next`
apunta al propio `node`. Esto crea un **ciclo de un nodo**:

```
push(10):
  top → [10|→] → [10|→] → [10|→] → ... (ciclo infinito)

push(20):
  top → [20|→] → [20|→] → [20|→] → ... (ciclo, nodo de 10 perdido)
```

Cada push sobreescribe `top` y crea un ciclo. Los nodos anteriores se pierden
(memory leak).

Con 3 pops:
- `pop()` retorna 30 (el último pushado)
- `pop()` sigue `node->next` que apunta al mismo nodo → retorna 30 otra vez
- `pop()` retorna 30 otra vez
- El loop nunca termina — es un **loop infinito** porque `next` nunca es NULL

Versión correcta:

```c
node->next = s->top;     // B primero: enlazar al top actual
s->top = node;           // A después: actualizar top
```

</details>

---

### Ejercicio 4 — Bug: use-after-free en destroy

Identifica el error y explica cómo lo detectarías con Valgrind:

```c
void stack_destroy_buggy(Stack *s) {
    if (!s) return;
    Node *current = s->top;
    while (current) {
        free(current);
        current = current->next;   // ← acceso después de free
    }
    free(s);
}
```

**Prediccion**: Este bug siempre crashea o puede parecer que funciona?

<details><summary>Respuesta</summary>

El error: `current->next` se lee **después** de `free(current)`. Esto es
use-after-free.

```
Iteración 1:
  current = [30|→] (dirección 0x2c0)
  free(0x2c0)              ← nodo liberado
  current = 0x2c0->next    ← lee memoria liberada → UB
```

**Puede parecer que funciona** en muchos casos. Después de `free`, la memoria
no se borra inmediatamente — los bytes siguen ahí hasta que el allocator los
reutilice. Si `current->next` se lee antes de que eso pase, obtiene el valor
correcto por casualidad.

Esto hace que el bug sea especialmente peligroso: funciona en pruebas locales
y falla en producción cuando el patrón de memoria es diferente.

Detección con Valgrind:

```bash
valgrind --leak-check=full ./stack_list
```

```
==12345== Invalid read of size 8
==12345==    at 0x...: stack_destroy_buggy (stack.c:XX)
==12345==  Address 0x2c0 is 8 bytes inside a block of size 16 free'd
==12345==    at 0x...: free (vg_replace_malloc.c:...)
==12345==    by 0x...: stack_destroy_buggy (stack.c:XX-1)
```

Valgrind detecta la lectura (`Invalid read`) de memoria liberada (`free'd`).
El fix: guardar `next` antes del `free`.

```c
Node *next = current->next;   // guardar ANTES
free(current);
current = next;
```

También se detecta con AddressSanitizer:

```bash
gcc -g -fsanitize=address -o stack_list stack.c main.c
./stack_list
# ERROR: AddressSanitizer: heap-use-after-free
```

</details>

---

### Ejercicio 5 — Comparar rendimiento

Escribe un programa que mida el tiempo de push + pop de N elementos para
la versión array y la versión lista. Prueba con $N = 10^5$ y $N = 10^6$:

```c
#include <time.h>

double measure_array(int n) {
    clock_t start = clock();
    // crear stack array, push n, pop n, destroy
    return 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
}

double measure_list(int n) {
    clock_t start = clock();
    // crear stack lista, push n, pop n, destroy
    return 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
}
```

**Prediccion**: La lista será 2×, 5×, o 10× más lenta que el array?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --- Array stack (inline) ---
typedef struct {
    int *data;
    size_t top, cap;
} AStack;

AStack *astack_create(size_t cap) {
    AStack *s = malloc(sizeof(AStack));
    s->data = malloc(cap * sizeof(int));
    s->top = 0;
    s->cap = cap;
    return s;
}
void astack_push(AStack *s, int v) { s->data[s->top++] = v; }
int  astack_pop(AStack *s) { return s->data[--s->top]; }
void astack_destroy(AStack *s) { free(s->data); free(s); }

// --- List stack (inline) ---
typedef struct LNode { int data; struct LNode *next; } LNode;
typedef struct { LNode *top; } LStack;

LStack *lstack_create(void) {
    LStack *s = malloc(sizeof(LStack));
    s->top = NULL;
    return s;
}
void lstack_push(LStack *s, int v) {
    LNode *n = malloc(sizeof(LNode));
    n->data = v;
    n->next = s->top;
    s->top = n;
}
int lstack_pop(LStack *s) {
    LNode *old = s->top;
    int v = old->data;
    s->top = old->next;
    free(old);
    return v;
}
void lstack_destroy(LStack *s) {
    while (s->top) { LNode *n = s->top; s->top = n->next; free(n); }
    free(s);
}

int main(void) {
    int sizes[] = {100000, 1000000};

    for (int si = 0; si < 2; si++) {
        int n = sizes[si];

        clock_t start = clock();
        AStack *a = astack_create(n);
        for (int i = 0; i < n; i++) astack_push(a, i);
        for (int i = 0; i < n; i++) astack_pop(a);
        astack_destroy(a);
        double ta = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;

        start = clock();
        LStack *l = lstack_create();
        for (int i = 0; i < n; i++) lstack_push(l, i);
        for (int i = 0; i < n; i++) lstack_pop(l);
        lstack_destroy(l);
        double tl = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;

        printf("n=%d:  array=%.1fms  list=%.1fms  ratio=%.1fx\n",
               n, ta, tl, tl / ta);
    }
    return 0;
}
```

```bash
gcc -O2 -o bench bench.c && ./bench
```

Resultados típicos:

```
n=100000:   array=0.3ms   list=2.1ms   ratio=7.0x
n=1000000:  array=2.8ms   list=25.3ms  ratio=9.0x
```

La lista es **~5-10× más lenta**. El factor dominante es `malloc`/`free`:
cada push y pop de la lista hace una llamada al allocator, mientras que el
array solo escribe/lee una posición de memoria.

El ratio empeora con $n$ grande porque:
- Más nodos dispersos → más cache misses
- El allocator tiene más trabajo gestionando bloques

</details>

---

### Ejercicio 6 — Contar nodos manualmente

Implementa una función `stack_count_nodes` que recorra la lista contando
nodos, y verifica que coincide con `size`:

```c
size_t stack_count_nodes(const Stack *s) {
    // Tu código aquí
}
```

**Prediccion**: Si size y el conteo real difieren, cuál es más confiable?

<details><summary>Respuesta</summary>

```c
size_t stack_count_nodes(const Stack *s) {
    size_t count = 0;
    Node *current = s->top;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

// Verificación:
void stack_verify(const Stack *s) {
    size_t counted = stack_count_nodes(s);
    if (counted != s->size) {
        fprintf(stderr, "INTEGRITY ERROR: counted %zu nodes but size = %zu\n",
                counted, s->size);
    }
}
```

Si difieren, **el conteo real es más confiable** — es lo que realmente existe
en memoria. `size` es un campo que se actualiza manualmente en push/pop y
puede desincronizarse por un bug (olvidar incrementar/decrementar).

Sin embargo, el conteo puede fallar si:
- La lista tiene un ciclo (loop infinito) — necesitarías un guard con límite
- Un nodo está corrupto (next apunta a basura) — el recorrido crashea

Esta función es una **verificación de invariante**: si la implementación es
correcta, `size` siempre coincide con el conteo real. Si no coincide, hay un
bug en push, pop, o destroy.

</details>

---

### Ejercicio 7 — Stack con free list

Implementa un stack que reutilice nodos liberados en vez de llamar a `free`
y `malloc` cada vez. Mantén una "free list" de nodos usados:

```c
typedef struct {
    Node *top;
    Node *free_list;   // nodos liberados, listos para reusar
    size_t size;
} PoolStack;
```

**Prediccion**: El rendimiento con free list se acercará al del array?

<details><summary>Respuesta</summary>

```c
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *top;
    Node *free_list;
    size_t size;
} PoolStack;

PoolStack *pstack_create(void) {
    PoolStack *s = malloc(sizeof(PoolStack));
    if (!s) return NULL;
    s->top = NULL;
    s->free_list = NULL;
    s->size = 0;
    return s;
}

static Node *pstack_alloc_node(PoolStack *s) {
    if (s->free_list) {
        // Reusar nodo de la free list
        Node *node = s->free_list;
        s->free_list = node->next;
        return node;
    }
    // No hay nodos libres: malloc nuevo
    return malloc(sizeof(Node));
}

static void pstack_free_node(PoolStack *s, Node *node) {
    // En vez de free, agregar a la free list
    node->next = s->free_list;
    s->free_list = node;
}

bool pstack_push(PoolStack *s, int elem) {
    Node *node = pstack_alloc_node(s);
    if (!node) return false;
    node->data = elem;
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}

bool pstack_pop(PoolStack *s, int *out) {
    if (!s->top) return false;
    Node *old_top = s->top;
    *out = old_top->data;
    s->top = old_top->next;
    pstack_free_node(s, old_top);   // reciclar, no free
    s->size--;
    return true;
}

void pstack_destroy(PoolStack *s) {
    if (!s) return;
    // Liberar nodos activos
    Node *cur = s->top;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    // Liberar nodos en free list
    cur = s->free_list;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    free(s);
}
```

Rendimiento comparado (push $10^6$ + pop $10^6$ + push $10^6$ otra vez):

```
Array:      ~3 ms    (baseline)
Free list:  ~8 ms    (primeros mallocs + luego reusos)
Lista pura: ~25 ms   (malloc/free cada vez)
```

La free list es **~3× más rápida que la lista pura** para operaciones
repetidas, porque después del primer ciclo de push+pop, todos los nodos
se reusan sin llamar al allocator.

Pero **no alcanza al array** (~2-3× más lenta) porque:
- Sigue teniendo mala cache locality (nodos dispersos)
- El primer llenado todavía hace mallocs individuales
- Cada push/pop sigue manipulando punteros (más instrucciones que array)

La free list es un buen compromiso cuando necesitas $O(1)$ estricto pero
quieres evitar el costo de malloc/free frecuente.

</details>

---

### Ejercicio 8 — Destroy recursivo vs iterativo

Implementa una versión recursiva de destroy y compárala con la iterativa:

```c
void stack_destroy_recursive(Stack *s) {
    if (!s) return;
    destroy_nodes(s->top);
    free(s);
}

static void destroy_nodes(Node *node) {
    // Tu código aquí
}
```

**Prediccion**: Para un stack de $10^6$ elementos, la versión recursiva funciona?

<details><summary>Respuesta</summary>

```c
static void destroy_nodes(Node *node) {
    if (!node) return;
    destroy_nodes(node->next);   // primero destruir el resto
    free(node);                  // luego liberar este nodo
}

void stack_destroy_recursive(Stack *s) {
    if (!s) return;
    destroy_nodes(s->top);
    free(s);
}
```

Para $10^6$ elementos: **probablemente no funciona**. Cada llamada recursiva
agrega un frame al call stack (~100-200 bytes). Con $10^6$ nodos:

```
$10^6 \times 200$ bytes $= 200$ MB de call stack

El call stack típico es 8 MB → stack overflow a ~40,000-80,000 nodos.
```

La versión recursiva es elegante pero impráctica para listas largas. La
versión iterativa funciona para cualquier longitud:

```c
// Iterativo: O(n) tiempo, O(1) espacio extra
void stack_destroy(Stack *s) {
    if (!s) return;
    Node *current = s->top;
    while (current) {
        Node *next = current->next;
        free(current);
        current = next;
    }
    free(s);
}
```

Regla: para recorrer listas enlazadas, **siempre preferir iterativo sobre
recursivo**. La recursión se reserva para estructuras con profundidad
logarítmica (árboles balanceados), donde la profundidad máxima es
$\log_2(n) \approx 20$ para $n = 10^6$.

</details>

---

### Ejercicio 9 — Implementar print sin modificar

Implementa `stack_print` que imprima los elementos del stack de bottom a top
sin modificar el stack:

```c
void stack_print(const Stack *s);
// push(10), push(20), push(30)
// stack_print → [10, 20, 30] (top = 30)
```

**Prediccion**: Es más difícil imprimir de bottom a top con lista que con
array? Por qué?

<details><summary>Respuesta</summary>

```c
// Auxiliar: imprimir de forma recursiva (primero el fondo, luego el tope)
static void print_nodes(const Node *node) {
    if (!node) return;
    print_nodes(node->next);    // primero imprimir el resto (más profundo)
    if (node->next) printf(", ");
    printf("%d", node->data);
}

void stack_print(const Stack *s) {
    printf("[");
    print_nodes(s->top);
    printf("]");
    if (s->top) {
        printf(" (top = %d)", s->top->data);
    }
    printf("\n");
}
```

Sí, es **más difícil** con lista que con array:

- **Array**: iterar de `data[0]` a `data[top-1]` — directo, bottom a top
- **Lista**: la lista va de top a bottom (top → ... → bottom). Imprimir de
  bottom a top requiere invertir el orden. Las opciones son:
  1. **Recursión** (como arriba) — imprime durante el retorno, invirtiendo
     el orden. Simple pero $O(n)$ en el call stack.
  2. **Stack auxiliar** — recorrer la lista, push cada elemento en un stack
     temporal, pop para imprimir. $O(n)$ memoria extra.
  3. **Array auxiliar** — recorrer la lista guardando punteros en un array,
     luego iterar el array en reversa. $O(n)$ memoria extra.

Todas las opciones de bottom-a-top con lista enlazada son $O(n)$ extra (ya
sea en el call stack o en memoria). Con array, es $O(1)$ extra.

Esta es una desventaja real de la lista enlazada: el acceso es solo de top
a bottom (forward), no bidireccional.

</details>

---

### Ejercicio 10 — Test comparativo array vs lista

Escribe un programa de test que verifique que ambas implementaciones (array
y lista) producen exactamente los mismos resultados para la misma secuencia
de operaciones:

```c
int main(void) {
    // Crear ambos stacks
    // Ejecutar la misma secuencia de push/pop/peek en ambos
    // Verificar que retornan lo mismo en cada paso
    // Verificar que ambos terminan vacíos
}
```

**Prediccion**: Existe alguna secuencia de operaciones donde las dos
implementaciones podrían dar resultados diferentes (asumiendo que ambas
son correctas)?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <assert.h>

// Asumiendo dos headers o dos implementaciones con prefijos distintos:
// astack_* para array, lstack_* para lista

int main(void) {
    AStack *a = astack_create(100);
    LStack *l = lstack_create();
    int va, vl;

    // Push idénticos
    for (int i = 0; i < 20; i++) {
        astack_push(a, i * 10);
        lstack_push(l, i * 10);
    }

    // Verificar size
    assert(astack_size(a) == lstack_size(l));

    // Pop parcial: verificar mismos valores
    for (int i = 0; i < 10; i++) {
        bool oa = astack_pop(a, &va);
        bool ol = lstack_pop(l, &vl);
        assert(oa == ol);
        assert(va == vl);
    }

    // Peek: mismo valor
    astack_peek(a, &va);
    lstack_peek(l, &vl);
    assert(va == vl);

    // Push más
    astack_push(a, 999);
    lstack_push(l, 999);

    // Pop todo: verificar mismos valores
    while (astack_pop(a, &va)) {
        bool ol = lstack_pop(l, &vl);
        assert(ol);
        assert(va == vl);
    }
    // Ambos vacíos
    assert(astack_is_empty(a));
    assert(lstack_is_empty(l));

    // Pop en vacío: ambos retornan false
    assert(!astack_pop(a, &va));
    assert(!lstack_pop(l, &vl));

    astack_destroy(a);
    lstack_destroy(l);
    printf("Both implementations match!\n");
    return 0;
}
```

No, **no existe** ninguna secuencia de operaciones válidas donde dos
implementaciones correctas del TAD Stack den resultados diferentes. Los
axiomas del TAD definen completamente el comportamiento observable — si
ambas implementaciones cumplen los axiomas, son indistinguibles desde fuera.

Esto es exactamente el propósito de un TAD: definir una **interfaz abstracta**
que múltiples implementaciones puedan satisfacer, garantizando que el código
que usa la interfaz funcione igual sin importar la implementación subyacente.

La única diferencia observable es el **rendimiento** (velocidad, memoria),
que no forma parte del contrato del TAD.

</details>
