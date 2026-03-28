# Operaciones en C

## Estructuras base

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

typedef struct DList {
    DNode *head;
    DNode *tail;
    int count;
} DList;

DNode *create_dnode(int value) {
    DNode *node = malloc(sizeof(DNode));
    if (!node) {
        fprintf(stderr, "malloc failed\n");
        exit(EXIT_FAILURE);
    }
    node->data = value;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

DList dlist_new(void) {
    return (DList){ .head = NULL, .tail = NULL, .count = 0 };
}
```

Todas las operaciones reciben `DList *list` para poder modificar `head`,
`tail` y `count`.

---

## Insertar al frente — $O(1)$

```c
void dlist_push_front(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->next = list->head;

    if (list->head) {
        list->head->prev = node;    // viejo head retrocede al nuevo
    } else {
        list->tail = node;          // lista estaba vacía
    }

    list->head = node;
    list->count++;
}
```

Diagrama con lista `[20, 30]`, insertando 10:

```
Antes:
  head ──→ [null|20|·]⟷[·|30|null] ←── tail

node = [null|10|null]

Paso 1: node->next = head
  [null|10|·]──→ [null|20|·]⟷[·|30|null]

Paso 2: head->prev = node
  [null|10|·]⟷[·|20|·]⟷[·|30|null]

Paso 3: head = node
  head ──→ [null|10|·]⟷[·|20|·]⟷[·|30|null] ←── tail
```

Tres punteros modificados: `node->next`, `head->prev`, `list->head`.
`node->prev` ya es `NULL` (correcto para el nuevo head).

---

## Insertar al final — $O(1)$

```c
void dlist_push_back(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->prev = list->tail;

    if (list->tail) {
        list->tail->next = node;    // viejo tail avanza al nuevo
    } else {
        list->head = node;          // lista estaba vacía
    }

    list->tail = node;
    list->count++;
}
```

Simétrico a `push_front`: intercambiamos `head↔tail`, `next↔prev`.

---

## Insertar en posición — $O(n)$

```c
int dlist_insert_at(DList *list, int pos, int value) {
    if (pos < 0 || pos > list->count) return 0;

    if (pos == 0) {
        dlist_push_front(list, value);
        return 1;
    }
    if (pos == list->count) {
        dlist_push_back(list, value);
        return 1;
    }

    // Optimización: recorrer desde el extremo más cercano
    DNode *cur;
    if (pos <= list->count / 2) {
        cur = list->head;
        for (int i = 0; i < pos; i++)
            cur = cur->next;
    } else {
        cur = list->tail;
        for (int i = list->count - 1; i > pos; i--)
            cur = cur->prev;
    }

    // Insertar ANTES de cur
    DNode *node = create_dnode(value);
    node->next = cur;
    node->prev = cur->prev;
    cur->prev->next = node;    // seguro: pos > 0 → prev existe
    cur->prev = node;
    list->count++;
    return 1;
}
```

La optimización de recorrer desde el extremo más cercano reduce el recorrido
promedio de $\frac{n}{2}$ a $\frac{n}{4}$ pasos — sigue siendo $O(n)$, pero
con constante 2× menor.

### Traza: insertar 25 en posición 2 de `[10, 20, 30, 40]`

```
pos=2, count=4, pos <= count/2 → recorrer desde head
  cur = head (10), i=0 → cur = 20, i=1 → cur = 30
  cur apunta al nodo [30]

Insertar [25] ANTES de [30]:
  node->next = cur (30)
  node->prev = cur->prev (20)
  cur->prev->next = node    // 20->next = 25
  cur->prev = node          // 30->prev = 25

Resultado: [10]⟷[20]⟷[25]⟷[30]⟷[40]
```

---

## Insertar antes / después de un nodo — $O(1)$

Cuando ya tenemos un puntero al nodo de referencia:

```c
void dlist_insert_after(DList *list, DNode *ref, int value) {
    DNode *node = create_dnode(value);
    node->prev = ref;
    node->next = ref->next;

    if (ref->next) {
        ref->next->prev = node;
    } else {
        list->tail = node;      // ref era el tail
    }

    ref->next = node;
    list->count++;
}

void dlist_insert_before(DList *list, DNode *ref, int value) {
    DNode *node = create_dnode(value);
    node->next = ref;
    node->prev = ref->prev;

    if (ref->prev) {
        ref->prev->next = node;
    } else {
        list->head = node;      // ref era el head
    }

    ref->prev = node;
    list->count++;
}
```

Ambas son $O(1)$ — no recorren nada.  En una lista simple, `insert_before`
sería $O(n)$ porque necesita encontrar el nodo anterior.

---

## Eliminar al frente — $O(1)$

```c
int dlist_pop_front(DList *list) {
    if (!list->head) {
        fprintf(stderr, "pop_front on empty list\n");
        exit(EXIT_FAILURE);
    }

    DNode *old = list->head;
    int value = old->data;

    list->head = old->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;      // lista quedó vacía
    }

    free(old);
    list->count--;
    return value;
}
```

---

## Eliminar al final — $O(1)$

```c
int dlist_pop_back(DList *list) {
    if (!list->tail) {
        fprintf(stderr, "pop_back on empty list\n");
        exit(EXIT_FAILURE);
    }

    DNode *old = list->tail;
    int value = old->data;

    list->tail = old->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;      // lista quedó vacía
    }

    free(old);
    list->count--;
    return value;
}
```

Esta es la operación que justifica la lista doble: `pop_back` es $O(1)$
gracias a `tail->prev`.  En lista simple era $O(n)$.

---

## Eliminar un nodo dado su puntero — $O(1)$

```c
int dlist_remove_node(DList *list, DNode *node) {
    int value = node->data;

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    free(node);
    list->count--;
    return value;
}
```

Cuatro casos según la posición del nodo:

| `prev` | `next` | Posición | Actualiza |
|--------|--------|----------|-----------|
| NULL | no-NULL | head | `list->head` |
| no-NULL | NULL | tail | `list->tail` |
| no-NULL | no-NULL | medio | solo vecinos |
| NULL | NULL | único | `list->head` y `list->tail` |

---

## Eliminar por valor — $O(n)$

```c
int dlist_remove_value(DList *list, int value) {
    DNode *cur = list->head;
    while (cur) {
        if (cur->data == value) {
            dlist_remove_node(list, cur);
            return 1;
        }
        cur = cur->next;
    }
    return 0;   // no encontrado
}
```

La búsqueda es $O(n)$, pero la eliminación en sí es $O(1)$ gracias a
`dlist_remove_node`.

---

## Eliminar en posición — $O(n)$

```c
int dlist_remove_at(DList *list, int pos) {
    if (pos < 0 || pos >= list->count) return -1;

    // Optimización: recorrer desde el extremo más cercano
    DNode *cur;
    if (pos <= list->count / 2) {
        cur = list->head;
        for (int i = 0; i < pos; i++)
            cur = cur->next;
    } else {
        cur = list->tail;
        for (int i = list->count - 1; i > pos; i--)
            cur = cur->prev;
    }

    return dlist_remove_node(list, cur);
}
```

---

## Recorrer en ambas direcciones

```c
void dlist_print_forward(const DList *list) {
    printf("Forward:  [");
    DNode *cur = list->head;
    while (cur) {
        printf("%d", cur->data);
        if (cur->next) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}

void dlist_print_backward(const DList *list) {
    printf("Backward: [");
    DNode *cur = list->tail;
    while (cur) {
        printf("%d", cur->data);
        if (cur->prev) printf(", ");
        cur = cur->prev;
    }
    printf("]\n");
}
```

---

## Buscar

```c
DNode *dlist_find(const DList *list, int value) {
    DNode *cur = list->head;
    while (cur) {
        if (cur->data == value) return cur;
        cur = cur->next;
    }
    return NULL;
}

DNode *dlist_get(const DList *list, int pos) {
    if (pos < 0 || pos >= list->count) return NULL;

    DNode *cur;
    if (pos <= list->count / 2) {
        cur = list->head;
        for (int i = 0; i < pos; i++)
            cur = cur->next;
    } else {
        cur = list->tail;
        for (int i = list->count - 1; i > pos; i--)
            cur = cur->prev;
    }
    return cur;
}
```

`dlist_get` retorna un `DNode *` que el usuario puede pasar a
`dlist_remove_node` o `dlist_insert_after` para operaciones $O(1)$.

---

## Destruir la lista

```c
void dlist_destroy(DList *list) {
    DNode *cur = list->head;
    while (cur) {
        DNode *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
```

Patrón estándar: guardar `next` antes de liberar.  `prev` no necesita
limpiarse — cada nodo se libera y sus campos desaparecen.

---

## Nodo sentinela (dummy head/tail)

Una técnica que elimina todos los chequeos de `NULL` en los bordes: usar
dos nodos *falsos* que siempre están presentes.

```
Sin sentinelas:
  head ──→ [null|A|·]⟷[·|B|·]⟷[·|C|null] ←── tail

Con sentinelas:
  head ──→ [null|DUMMY|·]⟷[·|A|·]⟷[·|B|·]⟷[·|C|·]⟷[·|DUMMY|null] ←── tail
            sentinela                                    sentinela
```

Los sentinelas nunca se eliminan y nunca contienen datos reales.  La lista
"vacía" contiene dos sentinelas enlazados entre sí:

```c
DList dlist_new_sentinel(void) {
    DNode *sentinel_head = create_dnode(0);   // valor ignorado
    DNode *sentinel_tail = create_dnode(0);

    sentinel_head->next = sentinel_tail;
    sentinel_tail->prev = sentinel_head;

    return (DList){
        .head = sentinel_head,
        .tail = sentinel_tail,
        .count = 0,
    };
}
```

```
Lista vacía con sentinelas:
  head ──→ [null|S|·]⟷[·|S|null] ←── tail
```

### push_front con sentinela

```c
void dlist_push_front_s(DList *list, int value) {
    DNode *node = create_dnode(value);

    // Insertar entre head (sentinela) y head->next (primer dato o tail sentinela)
    node->prev = list->head;
    node->next = list->head->next;
    list->head->next->prev = node;
    list->head->next = node;
    list->count++;
}
```

Sin `if`, sin chequeos de `NULL`.  El sentinela garantiza que `head->next`
siempre existe (es el sentinela tail cuando la lista está vacía).

### push_back con sentinela

```c
void dlist_push_back_s(DList *list, int value) {
    DNode *node = create_dnode(value);

    // Insertar entre tail->prev (último dato o head sentinela) y tail (sentinela)
    node->next = list->tail;
    node->prev = list->tail->prev;
    list->tail->prev->next = node;
    list->tail->prev = node;
    list->count++;
}
```

### remove_node con sentinela

```c
int dlist_remove_node_s(DList *list, DNode *node) {
    // Nunca eliminar un sentinela
    int value = node->data;
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node);
    list->count--;
    return value;
}
```

Dos líneas.  Sin `if`.  `node->prev` y `node->next` siempre existen porque los
sentinelas están siempre presentes.

### Comparación: sin vs con sentinelas

| Aspecto | Sin sentinelas | Con sentinelas |
|---------|---------------|----------------|
| push_front | 1 `if` (lista vacía) | 0 `if` |
| push_back | 1 `if` (lista vacía) | 0 `if` |
| remove_node | 2 `if` (head, tail) | 0 `if` |
| insert_after | 1 `if` (ref es tail) | 0 `if` |
| insert_before | 1 `if` (ref es head) | 0 `if` |
| Lista vacía | `head == NULL` | `head->next == tail` |
| Primer dato | `head` | `head->next` |
| Último dato | `tail` | `tail->prev` |
| Memoria extra | 0 | 2 nodos (48 bytes en 64-bit) |
| Complejidad conceptual | Baja | Media (hay que recordar los sentinelas) |

Los sentinelas brillan cuando el código tiene muchas inserciones/eliminaciones
y la reducción de ramas mejora legibilidad y reduce errores.  El kernel de Linux
usa sentinelas en su implementación de `list_head`.

### Recorrido con sentinelas

```c
void dlist_print_forward_s(const DList *list) {
    printf("[");
    DNode *cur = list->head->next;   // saltar sentinela head
    while (cur != list->tail) {      // parar antes del sentinela tail
        printf("%d", cur->data);
        if (cur->next != list->tail) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}
```

La condición de parada cambia: ya no es `cur != NULL` sino `cur != tail`.

### destroy con sentinelas

```c
void dlist_destroy_s(DList *list) {
    DNode *cur = list->head->next;   // primer dato
    while (cur != list->tail) {
        DNode *next = cur->next;
        free(cur);
        cur = next;
    }
    // Liberar los sentinelas
    free(list->head);
    free(list->tail);
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
```

---

## Reverse — $O(n)$

```c
void dlist_reverse(DList *list) {
    DNode *cur = list->head;
    while (cur) {
        // Intercambiar prev y next de cada nodo
        DNode *temp = cur->prev;
        cur->prev = cur->next;
        cur->next = temp;
        cur = cur->prev;    // avanzar (next original, ahora en prev)
    }
    // Intercambiar head y tail
    DNode *temp = list->head;
    list->head = list->tail;
    list->tail = temp;
}
```

La idea: invertir los punteros `prev` y `next` de **cada nodo** y luego
intercambiar `head` con `tail`.  Después del swap, `cur->prev` es lo que
*era* `cur->next` — por eso avanzamos con `cur = cur->prev`.

```
Antes:   [null|A|·]⟷[·|B|·]⟷[·|C|null]    head=A, tail=C

Nodo A: swap(prev,next) → [·|A|null]  prev=B, next=null
Nodo B: swap(prev,next) → [·|B|·]     prev=C, next=A
Nodo C: swap(prev,next) → [null|C|·]  prev=null, next=B

Swap head/tail:

Después: [null|C|·]⟷[·|B|·]⟷[·|A|null]    head=C, tail=A
```

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

typedef struct DList {
    DNode *head;
    DNode *tail;
    int count;
} DList;

DNode *create_dnode(int value) {
    DNode *node = malloc(sizeof(DNode));
    if (!node) { fprintf(stderr, "malloc failed\n"); exit(1); }
    node->data = value;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

DList dlist_new(void) {
    return (DList){ .head = NULL, .tail = NULL, .count = 0 };
}

void dlist_push_front(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->next = list->head;
    if (list->head) list->head->prev = node;
    else list->tail = node;
    list->head = node;
    list->count++;
}

void dlist_push_back(DList *list, int value) {
    DNode *node = create_dnode(value);
    node->prev = list->tail;
    if (list->tail) list->tail->next = node;
    else list->head = node;
    list->tail = node;
    list->count++;
}

int dlist_pop_front(DList *list) {
    DNode *old = list->head;
    int value = old->data;
    list->head = old->next;
    if (list->head) list->head->prev = NULL;
    else list->tail = NULL;
    free(old);
    list->count--;
    return value;
}

int dlist_pop_back(DList *list) {
    DNode *old = list->tail;
    int value = old->data;
    list->tail = old->prev;
    if (list->tail) list->tail->next = NULL;
    else list->head = NULL;
    free(old);
    list->count--;
    return value;
}

int dlist_remove_node(DList *list, DNode *node) {
    int value = node->data;
    if (node->prev) node->prev->next = node->next;
    else list->head = node->next;
    if (node->next) node->next->prev = node->prev;
    else list->tail = node->prev;
    free(node);
    list->count--;
    return value;
}

DNode *dlist_find(const DList *list, int value) {
    DNode *cur = list->head;
    while (cur) {
        if (cur->data == value) return cur;
        cur = cur->next;
    }
    return NULL;
}

void dlist_print_forward(const DList *list) {
    printf("Forward:  [");
    DNode *cur = list->head;
    while (cur) {
        printf("%d", cur->data);
        if (cur->next) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}

void dlist_print_backward(const DList *list) {
    printf("Backward: [");
    DNode *cur = list->tail;
    while (cur) {
        printf("%d", cur->data);
        if (cur->prev) printf(", ");
        cur = cur->prev;
    }
    printf("]\n");
}

void dlist_reverse(DList *list) {
    DNode *cur = list->head;
    while (cur) {
        DNode *temp = cur->prev;
        cur->prev = cur->next;
        cur->next = temp;
        cur = cur->prev;
    }
    DNode *temp = list->head;
    list->head = list->tail;
    list->tail = temp;
}

void dlist_destroy(DList *list) {
    DNode *cur = list->head;
    while (cur) {
        DNode *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

int main(void) {
    DList list = dlist_new();

    // Insertar
    dlist_push_back(&list, 10);
    dlist_push_back(&list, 20);
    dlist_push_back(&list, 30);
    dlist_push_front(&list, 5);
    dlist_print_forward(&list);     // Forward:  [5, 10, 20, 30]
    dlist_print_backward(&list);    // Backward: [30, 20, 10, 5]

    // pop_front / pop_back
    printf("pop_front: %d\n", dlist_pop_front(&list));  // 5
    printf("pop_back:  %d\n", dlist_pop_back(&list));   // 30
    dlist_print_forward(&list);     // Forward:  [10, 20]

    // Buscar y eliminar por puntero
    dlist_push_back(&list, 30);
    dlist_push_back(&list, 40);
    dlist_print_forward(&list);     // Forward:  [10, 20, 30, 40]

    DNode *found = dlist_find(&list, 30);
    if (found) {
        printf("remove 30: %d\n", dlist_remove_node(&list, found));
    }
    dlist_print_forward(&list);     // Forward:  [10, 20, 40]

    // Reverse
    dlist_reverse(&list);
    dlist_print_forward(&list);     // Forward:  [40, 20, 10]
    dlist_print_backward(&list);    // Backward: [10, 20, 40]

    printf("count: %d\n", list.count);  // 3

    dlist_destroy(&list);
    return 0;
}
```

Salida:

```
Forward:  [5, 10, 20, 30]
Backward: [30, 20, 10, 5]
pop_front: 5
pop_back:  30
Forward:  [10, 20]
Forward:  [10, 20, 30, 40]
remove 30: 30
Forward:  [10, 20, 40]
Forward:  [40, 20, 10]
Backward: [10, 20, 40]
count: 3
```

---

## Tabla de complejidades

| Operación | Complejidad | Nota |
|-----------|-------------|------|
| push_front | $O(1)$ | |
| push_back | $O(1)$ | |
| pop_front | $O(1)$ | |
| pop_back | $O(1)$ | Imposible en $O(1)$ con lista simple |
| insert_after(ref) | $O(1)$ | Dado puntero al nodo |
| insert_before(ref) | $O(1)$ | Dado puntero al nodo — $O(n)$ en simple |
| insert_at(pos) | $O(n)$ | Recorre desde el extremo más cercano |
| remove_node(ptr) | $O(1)$ | Dado puntero al nodo — $O(n)$ en simple |
| remove_at(pos) | $O(n)$ | |
| remove_value | $O(n)$ | Búsqueda $O(n)$ + eliminación $O(1)$ |
| find | $O(n)$ | |
| get(pos) | $O(n)$ | Desde extremo más cercano: $\leq \frac{n}{2}$ pasos |
| reverse | $O(n)$ | Swap prev/next de cada nodo |
| destroy | $O(n)$ | |

Las operaciones en **negrita conceptual** que pasan de $O(n)$ (simple) a $O(1)$
(doble) son: pop_back, insert_before, remove_node.

---

## Ejercicios

### Ejercicio 1 — Construir y recorrer

Escribe un programa que construya la lista `[5, 10, 15, 20, 25]` usando solo
`push_back`.  Imprímela forward y backward.

<details>
<summary>Salida esperada</summary>

```
Forward:  [5, 10, 15, 20, 25]
Backward: [25, 20, 15, 10, 5]
```
</details>

### Ejercicio 2 — pop_back secuencia

Sobre la lista `[10, 20, 30, 40]`, ejecuta `pop_back` 4 veces.  ¿En qué
orden salen los valores?  ¿Qué pasa si intentas un quinto pop_back?

<details>
<summary>Respuesta</summary>

Salen: 40, 30, 20, 10 (orden inverso — LIFO desde el final).  Un quinto
`pop_back` accede `list->tail` que es `NULL` — comportamiento indefinido
(segfault probable).  La implementación debería verificar si la lista está
vacía antes de operar.
</details>

### Ejercicio 3 — insert_at con optimización

Implementa `dlist_insert_at` que recorra desde el extremo más cercano.
Inserta el valor 99 en posición 3 de la lista `[10, 20, 30, 40, 50]`.
¿Desde qué extremo recorre?

<details>
<summary>Análisis</summary>

```
count=5, pos=3, count/2=2
pos (3) > count/2 (2) → recorrer desde tail

tail (50), i=4 → prev (40), i=3 → parar
cur = nodo 40

Insertar 99 antes de 40:
  [10]⟷[20]⟷[30]⟷[99]⟷[40]⟷[50]
```

Recorre 2 pasos desde tail en vez de 3 desde head.
</details>

### Ejercicio 4 — find + remove_node

Busca el valor 30 en `[10, 20, 30, 40, 50]` con `dlist_find`.  Elimina el
nodo retornado con `dlist_remove_node`.  Imprime forward y backward para
verificar consistencia bidireccional.

<details>
<summary>Verificación</summary>

```
Forward:  [10, 20, 40, 50]
Backward: [50, 40, 20, 10]
```

La clave: después de eliminar, `20->next == 40` y `40->prev == 20`.
Si cualquiera de estos es incorrecto, uno de los recorridos falla.
</details>

### Ejercicio 5 — Sentinela: push y remove sin if

Implementa `dlist_new_sentinel`, `dlist_push_front_s`, `dlist_push_back_s`,
y `dlist_remove_node_s`.  Verifica que ninguna función tiene `if` (excepto
`create_dnode` para verificar `malloc`).

<details>
<summary>Pista para verificar que funciona</summary>

Lista vacía con sentinelas: `head->next == tail` y `tail->prev == head`.
Después de `push_front_s(42)`: `head->next` es el nodo 42, cuyo `next` es
`tail`.  `tail->prev` es el nodo 42, cuyo `prev` es `head`.

Recorrer: desde `head->next` hasta llegar a `tail` (no `NULL`).
</details>

### Ejercicio 6 — Reverse en detalle

Traza manualmente `dlist_reverse` sobre la lista `[A, B, C]`.  Muestra
`prev` y `next` de cada nodo después de cada iteración del `while`.

<details>
<summary>Traza</summary>

```
Inicio: A(prev=null,next=B)  B(prev=A,next=C)  C(prev=B,next=null)

Iteración 1 (cur=A):
  swap: A(prev=B, next=null)
  cur = cur->prev = B

Iteración 2 (cur=B):
  swap: B(prev=C, next=A)
  cur = cur->prev = C

Iteración 3 (cur=C):
  swap: C(prev=null, next=B)
  cur = cur->prev = null → fin

Swap head/tail: head=C, tail=A

Resultado: C(prev=null,next=B)  B(prev=C,next=A)  A(prev=B,next=null)
```
</details>

### Ejercicio 7 — insert_before vs lista simple

Implementa `dlist_insert_before(list, ref, value)`.  Luego intenta
implementar la misma función para una lista simple.  ¿Cuántos pasos
requiere cada una para la misma operación?

<details>
<summary>Comparación</summary>

Lista doble — $O(1)$:
```c
node->next = ref;
node->prev = ref->prev;
if (ref->prev) ref->prev->next = node;
else list->head = node;
ref->prev = node;
```

Lista simple — $O(n)$:
```c
// Necesitamos encontrar el nodo anterior a ref
Node *cur = list->head;
if (cur == ref) { push_front(list, value); return; }
while (cur->next != ref) cur = cur->next;  // O(n)
// Ahora insertar entre cur y ref
```

Para una lista de 1 millón de nodos, insertar antes del penúltimo:
- Doble: 4 asignaciones de punteros.
- Simple: ~999,998 pasos de recorrido + 2 asignaciones.
</details>

### Ejercicio 8 — Eliminar todos los pares

Escribe `dlist_remove_if(list, predicate)` que recorra la lista y elimine
todos los nodos cuyo dato cumpla `predicate`.  Usa `dlist_remove_node`
internamente.  ¿Por qué es seguro avanzar `cur` después de eliminar el nodo
actual?

<details>
<summary>Implementación</summary>

```c
void dlist_remove_if(DList *list, int (*pred)(int)) {
    DNode *cur = list->head;
    while (cur) {
        DNode *next = cur->next;   // guardar ANTES de eliminar
        if (pred(cur->data)) {
            dlist_remove_node(list, cur);
        }
        cur = next;
    }
}

int is_even(int x) { return x % 2 == 0; }
dlist_remove_if(&list, is_even);
```

Es seguro porque guardamos `next` antes de liberar `cur`.  Si no, `cur->next`
después del `free` sería use-after-free.
</details>

### Ejercicio 9 — Sentinela: recorrido y destroy

Con sentinelas, la lista vacía no tiene `head == NULL`.  Implementa
`dlist_is_empty_s`, `dlist_print_forward_s`, y `dlist_destroy_s`.  ¿Cuál
es la condición de lista vacía?

<details>
<summary>Condición y código</summary>

Lista vacía: `list->head->next == list->tail`.

```c
int dlist_is_empty_s(const DList *list) {
    return list->head->next == list->tail;
}

void dlist_print_forward_s(const DList *list) {
    printf("[");
    DNode *cur = list->head->next;    // saltar sentinela
    while (cur != list->tail) {       // parar ante sentinela
        printf("%d", cur->data);
        if (cur->next != list->tail) printf(", ");
        cur = cur->next;
    }
    printf("]\n");
}

void dlist_destroy_s(DList *list) {
    DNode *cur = list->head->next;
    while (cur != list->tail) {
        DNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list->head);
    free(list->tail);
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}
```
</details>

### Ejercicio 10 — Concatenar dos listas

Implementa `dlist_concat(a, b)` que añada todos los nodos de `b` al final
de `a`.  Después de la operación, `b` queda vacía y `a` contiene todos los
elementos.  ¿Cuál es la complejidad?

<details>
<summary>Implementación</summary>

```c
void dlist_concat(DList *a, DList *b) {
    if (b->head == NULL) return;      // b vacía → nada que hacer

    if (a->head == NULL) {
        *a = *b;                       // a vacía → copiar b
    } else {
        a->tail->next = b->head;       // enlazar
        b->head->prev = a->tail;
        a->tail = b->tail;
        a->count += b->count;
    }

    b->head = NULL;
    b->tail = NULL;
    b->count = 0;
}
```

$O(1)$ — solo 4 asignaciones de punteros más actualización de contadores.
No se recorre ningún nodo.  Esto es imposible con arrays (requeriría copiar
todos los elementos, $O(n)$).
</details>
