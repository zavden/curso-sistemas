# Operaciones en C

## El problema de modificar head

Muchas operaciones (insertar al inicio, eliminar el primer nodo) necesitan
cambiar el puntero `head`.  Si la función recibe `Node *head`, recibe una
**copia** del puntero — los cambios no se reflejan en el llamador:

```c
/* MAL — head es una copia local */
void insert_front_bad(Node *head, int value) {
    Node *node = create_node(value);
    node->next = head;
    head = node;    /* modifica la copia local, no el original */
}

int main(void) {
    Node *head = NULL;
    insert_front_bad(head, 10);
    /* head sigue siendo NULL */
}
```

### Solución 1: retornar el nuevo head

```c
Node *insert_front(Node *head, int value) {
    Node *node = create_node(value);
    node->next = head;
    return node;
}

/* Uso: */
head = insert_front(head, 10);
```

Simple, pero el llamador puede olvidar `head = ...`.

### Solución 2: puntero a puntero (Node **)

```c
void insert_front(Node **head, int value) {
    Node *node = create_node(value);
    node->next = *head;
    *head = node;    /* modifica el puntero original */
}

/* Uso: */
insert_front(&head, 10);
```

`Node **head` es un puntero al puntero `head` del llamador.  `*head`
desreferencia para acceder (y modificar) el puntero original.

### Visualización del puntero a puntero

```
En main:
  head: [0x2040]  ← variable en el stack, contiene dirección del primer nodo

Llamada insert_front(&head, 99):
  head_ptr: [dirección de head en main]
            │
            ▼
  *head_ptr = head de main = [0x2040]

  Después de *head_ptr = node:
  head de main cambia a la dirección del nuevo nodo
```

### ¿Cuál usar?

| Estilo | Ventaja | Desventaja |
|--------|---------|-----------|
| Retornar `Node *` | Más simple de leer | Fácil olvidar `head = ...` |
| `Node **` | Modifica in-place, imposible olvidar | Sintaxis `*head` y `&head` confusa |

Usaremos `Node **` en este tópico — es la convención más robusta en C y es
fundamental para entender punteros a punteros (concepto que aparece en árboles,
tablas hash, kernel Linux, etc.).

---

## Estructura base

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

static Node *create_node(int value) {
    Node *node = malloc(sizeof(Node));
    if (!node) { perror("malloc"); exit(1); }
    node->data = value;
    node->next = NULL;
    return node;
}
```

---

## Insertar al inicio — $O(1)$

```c
void list_push_front(Node **head, int value) {
    Node *node = create_node(value);
    node->next = *head;
    *head = node;
}
```

```
Antes:  head → [10] → [20] → [30] → NULL

list_push_front(&head, 5):
  1. Crear nodo [5|NULL]
  2. nodo->next = *head = [10]     →  [5] → [10] → [20] → [30]
  3. *head = nodo                  →  head ahora apunta a [5]

Después:  head → [5] → [10] → [20] → [30] → NULL
```

Funciona incluso con lista vacía (`*head == NULL`): el nuevo nodo queda con
`next = NULL` y se convierte en el único nodo.

---

## Insertar al final — $O(n)$

```c
void list_push_back(Node **head, int value) {
    Node *node = create_node(value);

    if (*head == NULL) {
        *head = node;
        return;
    }

    Node *current = *head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = node;
}
```

Debe recorrer toda la lista para encontrar el último nodo.  Con un puntero
`tail` se reduce a $O(1)$, pero requiere mantener `tail` actualizado en todas
las operaciones.

```
Antes:  head → [10] → [20] → NULL

list_push_back(&head, 30):
  1. Crear nodo [30|NULL]
  2. Recorrer: current = [10], current = [20] (next == NULL → parar)
  3. current->next = nodo

Después:  head → [10] → [20] → [30] → NULL
```

---

## Insertar en posición — $O(n)$

Insertar en la posición `pos` (0-indexed):

```c
bool list_insert_at(Node **head, int pos, int value) {
    if (pos < 0) return false;

    if (pos == 0) {
        list_push_front(head, value);
        return true;
    }

    Node *current = *head;
    for (int i = 0; i < pos - 1 && current != NULL; i++) {
        current = current->next;
    }

    if (current == NULL) return false;   /* posición fuera de rango */

    Node *node = create_node(value);
    node->next = current->next;
    current->next = node;
    return true;
}
```

Para insertar en posición `pos`, necesitamos acceder al nodo en posición
`pos - 1` (el nodo anterior) y redirigir su `next`:

```
Insertar 25 en posición 2:

Antes:  [10] → [20] → [30] → NULL
              ↑ pos-1=1

  1. current = nodo en pos 1 = [20]
  2. node = [25|NULL]
  3. node->next = current->next = [30]
  4. current->next = node = [25]

Después: [10] → [20] → [25] → [30] → NULL
```

---

## Insertar con Node ** elegante

El puntero a puntero permite eliminar el caso especial de `pos == 0`:

```c
bool list_insert_at_v2(Node **head, int pos, int value) {
    if (pos < 0) return false;

    Node **current = head;
    for (int i = 0; i < pos && *current != NULL; i++) {
        current = &(*current)->next;
    }

    if (pos > 0 && *current == NULL) return false;

    Node *node = create_node(value);
    node->next = *current;
    *current = node;
    return true;
}
```

`current` es un `Node **` que apunta al puntero `next` del nodo anterior (o
a `head` si estamos al inicio).  Al escribir `*current = node`, modificamos
directamente ese puntero — sin distinguir si es `head` o un `next`.

```
Insertar en posición 0:
  current = &head (apunta al puntero head)
  *current = node → head cambia

Insertar en posición 2:
  current = &head → &(*head)->next → &(nodo1)->next
  *current = node → nodo1->next cambia
```

Este patrón es idiomático en C para manipulación de listas.  Linus Torvalds lo
ha citado como ejemplo de buen uso de punteros.

---

## Buscar — $O(n)$

### Buscar por valor

```c
Node *list_find(Node *head, int value) {
    Node *current = head;
    while (current != NULL) {
        if (current->data == value) return current;
        current = current->next;
    }
    return NULL;   /* no encontrado */
}
```

### Buscar por posición

```c
Node *list_get(Node *head, int pos) {
    if (pos < 0) return NULL;
    Node *current = head;
    for (int i = 0; i < pos && current != NULL; i++) {
        current = current->next;
    }
    return current;   /* NULL si pos fuera de rango */
}
```

---

## Eliminar del inicio — $O(1)$

```c
bool list_pop_front(Node **head, int *out) {
    if (*head == NULL) return false;

    Node *old = *head;
    *out = old->data;
    *head = old->next;
    free(old);
    return true;
}
```

```
Antes:  head → [10] → [20] → [30] → NULL

list_pop_front:
  1. old = *head = [10]
  2. *out = 10
  3. *head = old->next = [20]
  4. free(old)

Después:  head → [20] → [30] → NULL
```

---

## Eliminar del final — $O(n)$

```c
bool list_pop_back(Node **head, int *out) {
    if (*head == NULL) return false;

    /* Caso: un solo nodo */
    if ((*head)->next == NULL) {
        *out = (*head)->data;
        free(*head);
        *head = NULL;
        return true;
    }

    /* Buscar penúltimo */
    Node *current = *head;
    while (current->next->next != NULL) {
        current = current->next;
    }

    *out = current->next->data;
    free(current->next);
    current->next = NULL;
    return true;
}
```

Necesita el nodo **penúltimo** (`current->next->next == NULL`) para redirigir
su `next` a `NULL` después de liberar el último.

---

## Eliminar por valor — $O(n)$

Eliminar la **primera** ocurrencia de un valor:

```c
bool list_remove(Node **head, int value) {
    Node **current = head;

    while (*current != NULL) {
        if ((*current)->data == value) {
            Node *old = *current;
            *current = old->next;   /* redirigir: prev->next = old->next */
            free(old);
            return true;
        }
        current = &(*current)->next;
    }
    return false;   /* no encontrado */
}
```

De nuevo el patrón `Node **`: `current` apunta al puntero que referencia al
nodo actual (sea `head` o `prev->next`).  Al escribir `*current = old->next`,
se redirige ese puntero para saltear el nodo eliminado — sin necesidad de una
variable `prev` separada.

```
Eliminar 20:

head → [10] → [20] → [30] → NULL
              ↑
        *current apunta a este nodo
        current apunta a [10].next

  1. old = *current = [20]
  2. *current = old->next = [30]    →  [10].next = [30]
  3. free(old)

head → [10] → [30] → NULL
```

### Versión con prev explícito

Si `Node **` resulta confuso, la versión con `prev` es equivalente pero más
verbosa:

```c
bool list_remove_with_prev(Node **head, int value) {
    /* Caso especial: eliminar head */
    if (*head != NULL && (*head)->data == value) {
        Node *old = *head;
        *head = old->next;
        free(old);
        return true;
    }

    Node *prev = *head;
    while (prev != NULL && prev->next != NULL) {
        if (prev->next->data == value) {
            Node *old = prev->next;
            prev->next = old->next;
            free(old);
            return true;
        }
        prev = prev->next;
    }
    return false;
}
```

Nota cómo el caso especial de eliminar el head es inevitable sin `Node **` en
el recorrido.

---

## Eliminar por posición — $O(n)$

```c
bool list_remove_at(Node **head, int pos, int *out) {
    if (pos < 0 || *head == NULL) return false;

    Node **current = head;
    for (int i = 0; i < pos && *current != NULL; i++) {
        current = &(*current)->next;
    }

    if (*current == NULL) return false;

    Node *old = *current;
    *out = old->data;
    *current = old->next;
    free(old);
    return true;
}
```

---

## Liberar toda la lista — $O(n)$

```c
void list_free(Node **head) {
    Node *current = *head;
    while (current != NULL) {
        Node *next = current->next;
        free(current);
        current = next;
    }
    *head = NULL;   /* evitar dangling pointer */
}
```

Poner `*head = NULL` después de liberar es importante — evita que el llamador
use accidentalmente el puntero viejo.

---

## Imprimir — $O(n)$

```c
void list_print(const Node *head) {
    printf("[");
    const Node *current = head;
    while (current != NULL) {
        printf("%d", current->data);
        if (current->next) printf(", ");
        current = current->next;
    }
    printf("]\n");
}
```

---

## Programa completo

```c
int main(void) {
    Node *head = NULL;

    /* Insertar */
    list_push_front(&head, 30);
    list_push_front(&head, 20);
    list_push_front(&head, 10);
    list_push_back(&head, 40);
    list_push_back(&head, 50);
    printf("After pushes: ");
    list_print(head);           // [10, 20, 30, 40, 50]

    /* Insertar en posición */
    list_insert_at(&head, 2, 25);
    printf("Insert 25 at 2: ");
    list_print(head);           // [10, 20, 25, 30, 40, 50]

    /* Buscar */
    Node *found = list_find(head, 30);
    printf("Find 30: %s\n", found ? "found" : "not found");  // found

    /* Eliminar por valor */
    list_remove(&head, 25);
    printf("Remove 25: ");
    list_print(head);           // [10, 20, 30, 40, 50]

    /* Eliminar del inicio */
    int val;
    list_pop_front(&head, &val);
    printf("Pop front (%d): ", val);
    list_print(head);           // [20, 30, 40, 50]

    /* Eliminar del final */
    list_pop_back(&head, &val);
    printf("Pop back (%d): ", val);
    list_print(head);           // [20, 30, 40]

    /* Eliminar por posición */
    list_remove_at(&head, 1, &val);
    printf("Remove at 1 (%d): ", val);
    list_print(head);           // [20, 40]

    /* Liberar */
    list_free(&head);
    printf("After free: ");
    list_print(head);           // []

    return 0;
}
```

---

## Resumen de complejidades

| Operación | Complejidad | Nota |
|-----------|-------------|------|
| `push_front` | $O(1)$ | Solo redirigir head |
| `push_back` | $O(n)$ | Recorrer hasta el final ($O(1)$ con tail) |
| `insert_at(pos)` | $O(n)$ | Recorrer hasta pos-1 |
| `find(value)` | $O(n)$ | Búsqueda lineal |
| `get(pos)` | $O(n)$ | Recorrer hasta pos |
| `pop_front` | $O(1)$ | Redirigir head |
| `pop_back` | $O(n)$ | Buscar penúltimo |
| `remove(value)` | $O(n)$ | Buscar + redirigir |
| `remove_at(pos)` | $O(n)$ | Recorrer + redirigir |
| `free` | $O(n)$ | Liberar cada nodo |
| `print` | $O(n)$ | Recorrer |

---

## Ejercicios

### Ejercicio 1 — Traza de push_front

Traza `list_push_front` para insertar 30, 20, 10 (en ese orden) partiendo
de una lista vacía.  Muestra `*head` y la lista después de cada inserción.

<details>
<summary>Resultado</summary>

```
Inicio: head = NULL

push_front(30): head → [30|NULL]
push_front(20): head → [20] → [30|NULL]
push_front(10): head → [10] → [20] → [30|NULL]
```

El último push_front produce el primer elemento — la lista queda en orden
inverso al de inserción.
</details>

### Ejercicio 2 — Traza de insert_at con Node **

Traza `list_insert_at_v2` (la versión con `Node **`) para insertar 25 en
posición 1 de la lista `[10, 20, 30]`.  Muestra el valor de `current` (a qué
puntero apunta) en cada iteración del for.

<details>
<summary>Traza</summary>

```
current = &head           (apunta al puntero head)
i=0: *current = [10], avanzar
current = &([10]->next)   (apunta al campo next del nodo 10)

Fin del for (i=1 == pos=1).
*current = [20]

node = [25|NULL]
node->next = *current = [20]    → [25] → [20]
*current = node                 → [10]->next = [25]

Resultado: [10] → [25] → [20] → [30] → NULL
```
</details>

### Ejercicio 3 — Eliminar todas las ocurrencias

Escribe `list_remove_all(Node **head, int value)` que elimine **todas** las
ocurrencias de `value` (no solo la primera).  Retorna el número de nodos
eliminados.

<details>
<summary>Implementación con Node **</summary>

```c
int list_remove_all(Node **head, int value) {
    int removed = 0;
    Node **current = head;
    while (*current != NULL) {
        if ((*current)->data == value) {
            Node *old = *current;
            *current = old->next;
            free(old);
            removed++;
            /* NO avanzar current — el nuevo *current puede ser otro match */
        } else {
            current = &(*current)->next;
        }
    }
    return removed;
}
```

Solo avanza `current` cuando el nodo actual NO se elimina.  Si se elimina,
`*current` ya apunta al siguiente — hay que verificarlo de nuevo.
</details>

### Ejercicio 4 — Invertir la lista

Escribe `list_reverse(Node **head)` que invierta la lista in-place
redirigiendo los punteros `next`.  Complejidad: $O(n)$ tiempo, $O(1)$ espacio.

<details>
<summary>Algoritmo con 3 punteros</summary>

```c
void list_reverse(Node **head) {
    Node *prev = NULL;
    Node *current = *head;
    while (current != NULL) {
        Node *next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    *head = prev;
}
```

En cada paso: guardar next, invertir el enlace (`current->next = prev`),
avanzar prev y current.  Al final, `prev` es el nuevo head.
</details>

### Ejercicio 5 — Insertar ordenado

Escribe `list_insert_sorted(Node **head, int value)` que inserte en una lista
que se mantiene ordenada (ascendente).  Usa `Node **` para evitar caso especial.

<details>
<summary>Implementación</summary>

```c
void list_insert_sorted(Node **head, int value) {
    Node **current = head;
    while (*current != NULL && (*current)->data < value) {
        current = &(*current)->next;
    }
    Node *node = create_node(value);
    node->next = *current;
    *current = node;
}
```

Misma lógica que la cola de prioridad con lista (C04/S02/T03), pero más limpia
con `Node **`.
</details>

### Ejercicio 6 — Detectar ciclo (Floyd)

Escribe una función que detecte si una lista tiene un ciclo (el último nodo
apunta a algún nodo anterior).  Usa el algoritmo de Floyd (tortuga y liebre):
dos punteros, uno avanza de 1 en 1 y otro de 2 en 2.  Si se encuentran, hay
ciclo.

<details>
<summary>Implementación</summary>

```c
bool list_has_cycle(Node *head) {
    Node *slow = head;
    Node *fast = head;
    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;
    }
    return false;
}
```

Si no hay ciclo, `fast` llega a NULL en $O(n)$.  Si hay ciclo, `fast` alcanza
a `slow` en $O(n)$ iteraciones.  Espacio: $O(1)$.
</details>

### Ejercicio 7 — Encontrar el nodo medio

Escribe una función que retorne el nodo del medio de la lista en una sola
pasada.  Usa dos punteros: uno avanza de 1 en 1, otro de 2 en 2.  Cuando el
rápido llega al final, el lento está en el medio.

<details>
<summary>Implementación</summary>

```c
Node *list_middle(Node *head) {
    if (head == NULL) return NULL;
    Node *slow = head;
    Node *fast = head;
    while (fast->next != NULL && fast->next->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}
```

Para lista de largo par, retorna el primero de los dos nodos centrales.
</details>

### Ejercicio 8 — Concatenar dos listas

Escribe `list_concat(Node **a, Node **b)` que añada la lista `b` al final de
`a`, dejando `b` vacía.

<details>
<summary>Implementación</summary>

```c
void list_concat(Node **a, Node **b) {
    if (*b == NULL) return;
    if (*a == NULL) {
        *a = *b;
    } else {
        Node *current = *a;
        while (current->next != NULL) current = current->next;
        current->next = *b;
    }
    *b = NULL;
}
```

$O(n)$ donde $n$ es el largo de `a` (para encontrar el final).  $O(1)$ si
se mantiene un puntero `tail`.
</details>

### Ejercicio 9 — Lista con head y tail

Crea un struct `List` que mantenga `head`, `tail` y `count`.  Reimplementa
`push_front`, `push_back`, `pop_front` para que todas sean $O(1)$.  ¿Es
posible hacer `pop_back` en $O(1)$ con lista simple?

<details>
<summary>Respuesta sobre pop_back</summary>

```c
typedef struct {
    Node *head;
    Node *tail;
    int count;
} List;
```

`push_back` con `tail` es $O(1)$: `tail->next = node; tail = node`.
`pop_back` **no** es $O(1)$ con lista simple — necesitas el penúltimo nodo,
y no hay `prev`.  Para `pop_back` en $O(1)$ se necesita lista **doblemente**
enlazada (S02).
</details>

### Ejercicio 10 — Merge de dos listas ordenadas

Escribe `list_merge(Node **a, Node **b)` que mezcle dos listas ordenadas en
una sola lista ordenada.  No crear nodos nuevos — solo redirigir punteros.
Dejar `*b = NULL`.

<details>
<summary>Implementación con Node **</summary>

```c
void list_merge(Node **a, Node **b) {
    Node **current = a;
    while (*current != NULL && *b != NULL) {
        if ((*b)->data < (*current)->data) {
            Node *tmp = *b;
            *b = (*b)->next;
            tmp->next = *current;
            *current = tmp;
        }
        current = &(*current)->next;
    }
    if (*b != NULL) {
        *current = *b;
        *b = NULL;
    }
}
```

Misma lógica que el merge de merge sort, pero sin copiar datos — solo
redirigir punteros `next`.  $O(n + m)$.
</details>
