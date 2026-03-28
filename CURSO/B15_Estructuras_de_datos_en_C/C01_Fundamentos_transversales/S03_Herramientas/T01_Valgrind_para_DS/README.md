# T01 — Valgrind para estructuras de datos

---

## 1. Por qué Valgrind es imprescindible para estructuras dinámicas

En C, toda estructura de datos dinámica (listas, árboles, hash tables, grafos)
depende de `malloc`/`free`. Cada `malloc` sin su `free` correspondiente es un
**memory leak**. Cada `free` seguido de un acceso es un **use-after-free**. Cada
`free` doble es un **double free**.

El compilador no detecta ninguno de estos errores. El programa puede funcionar
"bien" durante meses y explotar en producción cuando $n$ crece lo suficiente.

**Valgrind** (específicamente su herramienta **Memcheck**) intercepta cada
alocación y liberación, construyendo un mapa completo de la memoria del programa.
Al terminar, reporta exactamente qué se alocó y no se liberó, dónde se accedió
a memoria inválida, y dónde se usaron valores no inicializados.

### Lo que Valgrind detecta

| Clase de error | Ejemplo en DS |
|---------------|--------------|
| Memory leak | No liberar nodos al destruir una lista |
| Use-after-free | Acceder a `node->next` después de `free(node)` |
| Double free | Llamar `free(node)` dos veces |
| Invalid read/write | Leer `node->data` de un nodo ya liberado |
| Uninitialized value | Usar `node->next` sin asignarle `NULL` |
| Invalid free | Llamar `free` con un puntero no alocado |
| Overlap in `memcpy` | Copiar regiones solapadas (debe ser `memmove`) |

### Invocación básica

```bash
gcc -g -O0 -o program program.c
valgrind --leak-check=full --show-leak-kinds=all ./program
```

Flags esenciales:
- `-g`: información de depuración (nombres de archivo y línea)
- `-O0`: sin optimizaciones (evita que el compilador reordene o elimine código)
- `--leak-check=full`: reportar cada leak individualmente
- `--show-leak-kinds=all`: incluir leaks "still reachable" (memoria alcanzable
  pero no liberada al terminar)

---

## 2. Estructura base: lista enlazada con bugs

Para este tópico usaremos una lista enlazada simple como campo de batalla.
Primero la versión correcta, luego introduciremos bugs uno por uno:

```c
// list.h
#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    int size;
} List;

List *list_create(void);
void  list_destroy(List *list);
void  list_push_front(List *list, int data);
int   list_pop_front(List *list);
bool  list_is_empty(const List *list);
void  list_print(const List *list);

#endif
```

```c
// list.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "list.h"

List *list_create(void) {
    List *list = malloc(sizeof(List));
    if (!list) return NULL;
    list->head = NULL;
    list->size = 0;
    return list;
}

void list_destroy(List *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

void list_push_front(List *list, int data) {
    assert(list);
    Node *node = malloc(sizeof(Node));
    if (!node) return;
    node->data = data;
    node->next = list->head;
    list->head = node;
    list->size++;
}

int list_pop_front(List *list) {
    assert(list && list->head);
    Node *old = list->head;
    int data = old->data;
    list->head = old->next;
    free(old);
    list->size--;
    return data;
}

bool list_is_empty(const List *list) {
    return list == NULL || list->head == NULL;
}

void list_print(const List *list) {
    if (!list) return;
    for (Node *cur = list->head; cur; cur = cur->next)
        printf("%d -> ", cur->data);
    printf("NULL\n");
}
```

---

## 3. Patrón 1 — Memory leak por destrucción incompleta

### El bug

```c
// BUG: solo libera la struct List, no los nodos
void list_destroy_buggy(List *list) {
    free(list);
}
```

### main.c de prueba

```c
#include "list.h"

int main(void) {
    List *list = list_create();
    list_push_front(list, 10);
    list_push_front(list, 20);
    list_push_front(list, 30);

    list_destroy_buggy(list);
    return 0;
}
```

### Compilar y ejecutar con Valgrind

```bash
gcc -g -O0 -o test_list list.c main.c
valgrind --leak-check=full ./test_list
```

### Salida de Valgrind

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 48 bytes in 3 blocks
==12345==   total heap usage: 4 allocs, 1 frees, 64 bytes allocated
==12345==
==12345== 48 bytes in 3 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x4C2FB55: malloc (in /usr/lib/valgrind/...)
==12345==    by 0x4005E3: list_push_front (list.c:27)
==12345==    by 0x40066A: main (main.c:5)
```

### Cómo leer el reporte

```
4 allocs, 1 frees → se hicieron 4 malloc y solo 1 free
                     (1 List + 3 Node = 4 allocs, solo se liberó la List)

48 bytes in 3 blocks → 3 nodos × 16 bytes cada uno = 48 bytes perdidos
                        (sizeof(Node) = sizeof(int) + sizeof(Node*) = 4 + 8 = 12
                         → padded a 16 bytes por alineación)

definitely lost → no queda ningún puntero que apunte a esta memoria
                  (la List fue liberada, así que list->head ya no existe)
```

### Tipos de leak en Valgrind

| Tipo | Significado | Acción |
|------|-----------|--------|
| **definitely lost** | Memoria alocada sin puntero que la alcance | Bug seguro — corregir |
| **indirectly lost** | Memoria alcanzable solo a través de un bloque "definitely lost" | Se corrige al corregir el "definitely" |
| **possibly lost** | Hay un puntero que podría apuntar al interior del bloque | Investigar — posible bug |
| **still reachable** | Hay puntero al bloque pero no se llamó `free` | Generalmente aceptable al salir |

### La corrección

```c
// CORRECTO: recorrer y liberar cada nodo, luego la lista
void list_destroy(List *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;   // salvar siguiente ANTES de liberar
        free(cur);
        cur = next;
    }
    free(list);
}
```

Patrón fundamental: **siempre guardar `next` antes de `free`**. Después de
`free(cur)`, el campo `cur->next` es memoria inválida.

---

## 4. Patrón 2 — Use-after-free en destrucción

### El bug

```c
// BUG: accede a cur->next DESPUÉS de free(cur)
void list_destroy_uaf(List *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        free(cur);
        cur = cur->next;   // ERROR: cur ya fue liberado
    }
    free(list);
}
```

### Salida de Valgrind

```
==12345== Invalid read of size 8
==12345==    at 0x4005C8: list_destroy_uaf (list.c:19)
==12345==    by 0x40066F: main (main.c:9)
==12345==  Address 0x5205048 is 8 bytes inside a block of size 16 free'd
==12345==    at 0x4C30D3B: free (in /usr/lib/valgrind/...)
==12345==    by 0x4005BF: list_destroy_uaf (list.c:18)
```

### Cómo leer este error

```
Invalid read of size 8
→ Se leyeron 8 bytes (un puntero en 64-bit) de memoria inválida

Address 0x5205048 is 8 bytes inside a block of size 16 free'd
→ La dirección leída está a offset 8 dentro de un bloque de 16 bytes
   que ya fue liberado
→ Offset 8 en un Node de 16 bytes = el campo "next"
   (data ocupa bytes 0-3, padding 4-7, next ocupa 8-15)

list_destroy_uaf (list.c:19)
→ La línea exacta donde ocurre el read: cur = cur->next
```

### Por qué este bug es peligroso

El programa puede "funcionar" porque `free` no siempre borra la memoria
inmediatamente. El valor de `cur->next` puede seguir ahí por casualidad.
Pero en producción, con carga, el allocator puede reutilizar ese bloque
para otra cosa, y `cur->next` ahora apunta a datos corruptos.

Valgrind detecta esto **siempre**, independientemente de si "funciona" o no.

---

## 5. Patrón 3 — Double free

### El bug

```c
// BUG: liberar la misma lista dos veces
int main(void) {
    List *list = list_create();
    list_push_front(list, 10);

    list_destroy(list);
    list_destroy(list);   // ERROR: list ya fue liberada
    return 0;
}
```

### Salida de Valgrind

```
==12345== Invalid free() / delete / delete[] / realloc()
==12345==    at 0x4C30D3B: free (in /usr/lib/valgrind/...)
==12345==    by 0x4005F2: list_destroy (list.c:22)
==12345==    by 0x400672: main (main.c:7)
==12345==  Address 0x5205040 is 16 bytes inside a block of size 24 free'd
==12345==    at 0x4C30D3B: free (in /usr/lib/valgrind/...)
==12345==    by 0x4005F2: list_destroy (list.c:22)
==12345==    by 0x40066B: main (main.c:6)
```

### Patrón defensivo: nullificar después de destroy

```c
list_destroy(list);
list = NULL;           // previene double-free

// Si list_destroy es NULL-safe, la segunda llamada es inocua:
list_destroy(list);    // list == NULL → return inmediato
```

Este es el motivo por el que `list_destroy` debe empezar con `if (!list) return;`.

---

## 6. Patrón 4 — Leak en árboles por destrucción incorrecta

Los árboles requieren destrucción **recursiva** (o iterativa con stack).
Olvidar una rama es un leak parcial:

### Estructura del árbol

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

typedef struct {
    TreeNode *root;
} Tree;
```

### Bug: solo liberar la rama izquierda

```c
// BUG: solo destruye la rama izquierda
void tree_destroy_buggy(TreeNode *node) {
    if (!node) return;
    tree_destroy_buggy(node->left);
    // FALTA: tree_destroy_buggy(node->right);
    free(node);
}
```

### Visualización del leak

```
        10          ← se libera (después de su hijo izquierdo)
       /  \
      5    15       ← 5 se libera, 15 se PIERDE
     / \   / \
    2   7 12  20    ← 2,7 se liberan, 12,20 se PIERDEN

Nodos liberados: 10, 5, 2, 7 (rama izquierda completa)
Nodos perdidos:  15, 12, 20  (rama derecha completa)
```

### Salida de Valgrind

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 72 bytes in 3 blocks
==12345==
==12345== 24 bytes in 1 blocks are definitely lost in loss record 1 of 2
==12345==    at 0x4C2FB55: malloc (...)
==12345==    by 0x4006A2: tree_insert (tree.c:15)
==12345==
==12345== 48 bytes in 2 blocks are indirectly lost in loss record 2 of 2
```

Nota: el nodo 15 es **definitely lost** (la raíz fue liberada, el puntero
`root->right` ya no existe). Los nodos 12 y 20 son **indirectly lost**
(solo eran alcanzables a través de 15).

### Destrucción correcta de un árbol

```c
void tree_destroy(TreeNode *node) {
    if (!node) return;
    tree_destroy(node->left);    // destruir subárbol izquierdo
    tree_destroy(node->right);   // destruir subárbol derecho
    free(node);                  // liberar el nodo actual (post-order)
}
```

El orden **post-order** es esencial: primero liberar los hijos, luego el padre.
Si se libera el padre primero, los punteros a los hijos se pierden.

---

## 7. Patrón 5 — Leak al eliminar un nodo del medio

### El bug

```c
// BUG: eliminar nodo sin reconectar la cadena
void list_remove_buggy(List *list, int value) {
    Node *cur = list->head;
    while (cur) {
        if (cur->data == value) {
            free(cur);          // se libera el nodo...
            list->size--;
            return;             // ...pero prev->next sigue apuntando aquí
        }
        cur = cur->next;
    }
}
```

### Qué pasa en memoria

```
Antes: head -> [10] -> [20] -> [30] -> NULL
                        ^^^^
                        eliminar 20

Después del free:
       head -> [10] -> [????] -> [30] -> NULL
                        ^^^^^
                        memoria liberada, pero [10]->next
                        sigue apuntando aquí (dangling pointer)

Siguiente acceso a la lista: use-after-free
```

### Eliminación correcta

```c
void list_remove(List *list, int value) {
    assert(list);
    Node **pp = &list->head;        // puntero a puntero (doble indirección)
    while (*pp) {
        if ((*pp)->data == value) {
            Node *to_free = *pp;
            *pp = (*pp)->next;      // reconectar: prev->next = cur->next
            free(to_free);
            list->size--;
            return;
        }
        pp = &(*pp)->next;
    }
}
```

La técnica del **puntero a puntero** (`Node **pp`) elimina el caso especial
de borrar la cabeza: `pp` empieza apuntando a `list->head`, y luego a cada
`node->next`. `*pp = (*pp)->next` reconecta correctamente en ambos casos.

---

## 8. `--track-origins=yes` — Encontrar valores no inicializados

### El problema

```c
Node *node = malloc(sizeof(Node));
node->data = 42;
// BUG: no se asignó node->next = NULL
```

Sin `node->next = NULL`, el campo `next` contiene basura. Si alguien recorre
la lista y llega a este nodo, `node->next` es un puntero aleatorio, y
`node->next->data` es un crash o corrupción silenciosa.

### Valgrind sin `--track-origins`

```
==12345== Conditional jump or move depends on uninitialised value(s)
==12345==    at 0x400612: list_print (list.c:45)
```

Esto dice *qué* valor no está inicializado, pero no *dónde se alocó*.

### Valgrind con `--track-origins=yes`

```bash
valgrind --leak-check=full --track-origins=yes ./program
```

```
==12345== Conditional jump or move depends on uninitialised value(s)
==12345==    at 0x400612: list_print (list.c:45)
==12345==  Uninitialised value was created by a heap allocation
==12345==    at 0x4C2FB55: malloc (in /usr/lib/valgrind/...)
==12345==    by 0x4005D3: list_push_front (list.c:27)
==12345==    by 0x400660: main (main.c:5)
```

Ahora Valgrind dice exactamente dónde se alocó el bloque que contiene el
valor sin inicializar: `list_push_front` en `list.c:27`. El fix es agregar
`node->next = NULL` (o usar `calloc`).

### Costo de `--track-origins`

Esta flag hace a Valgrind aún más lento (típicamente $2\times$ más sobre la ya
significativa ralentización de $10\text{-}20\times$). Usarla solo cuando hay
errores de valores no inicializados y el origen no es obvio.

---

## 9. Valgrind en la práctica: flujo de trabajo

### Paso 1 — Compilar para depuración

```bash
gcc -g -O0 -Wall -Wextra -o test_ds main.c list.c
```

### Paso 2 — Ejecutar con Valgrind

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_ds
```

### Paso 3 — Interpretar el resumen final

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 10 allocs, 10 frees, 200 bytes allocated
==12345==
==12345== All heap blocks were freed -- no leaks are possible
==12345==
==12345== ERROR SUMMARY: 0 errors from 0 contexts
```

El objetivo es siempre:
- `in use at exit: 0 bytes in 0 blocks`
- `All heap blocks were freed`
- `ERROR SUMMARY: 0 errors`

### Paso 4 — Script de verificación (Makefile target)

```makefile
CFLAGS = -g -O0 -Wall -Wextra
VFLAGS = --leak-check=full --show-leak-kinds=all --error-exitcode=1

test: test_ds
	valgrind $(VFLAGS) ./test_ds

test_ds: main.c list.c list.h
	$(CC) $(CFLAGS) -o $@ main.c list.c
```

`--error-exitcode=1` hace que Valgrind retorne exit code 1 si encuentra errores,
permitiendo integración en CI/CD o scripts de testing.

---

## 10. Errores frecuentes y sus firmas en Valgrind

### Tabla de referencia rápida

| Error | Firma en Valgrind | Causa típica en DS |
|-------|-------------------|-------------------|
| Leak de $n$ nodos | `n blocks definitely lost` | `destroy` no recorre la estructura |
| Use-after-free | `Invalid read of size X` + `block of size Y free'd` | `free(node); ... node->next` |
| Double free | `Invalid free()` + `block of size Y free'd` | Destruir dos veces sin nullificar |
| Buffer overflow | `Invalid write of size X` + `block of size Y alloc'd` | Escribir más allá del bloque alocado |
| No inicializado | `Conditional jump depends on uninitialised value` | `malloc` sin asignar campos |
| Leak indirecto | `indirectly lost` | Nodos hijos de un nodo "definitely lost" |
| Leak al remover | `1 blocks definitely lost` | `free` sin reconectar `prev->next` |

### Regla mnemotécnica para destrucción

```
Lista:   while → next → free → advance
Árbol:   post-order → free hijos antes que padre
Hash:    for each bucket → destruir lista del bucket → free array de buckets
Grafo:   for each vertex → destruir lista de adyacencia → free array de vértices
```

---

## Ejercicios

### Ejercicio 1 — Identificar el tipo de error

¿Qué tipo de error reportará Valgrind para cada fragmento?

```c
// A
Node *n = malloc(sizeof(Node));
n->data = 5;
n->next = NULL;
// programa termina sin free(n)

// B
Node *n = malloc(sizeof(Node));
free(n);
printf("%d\n", n->data);

// C
Node *n = malloc(sizeof(Node));
free(n);
free(n);

// D
Node *n = malloc(sizeof(Node));
n->data = 10;
if (n->next != NULL) {   // next nunca fue asignado
    printf("has next\n");
}
free(n);
```

**Prediccion**: Clasifica cada uno como leak, use-after-free, double free,
o uso de valor no inicializado.

<details><summary>Respuesta</summary>

| Fragmento | Error | Firma Valgrind |
|-----------|-------|---------------|
| A | **Memory leak** | `16 bytes in 1 blocks definitely lost` |
| B | **Use-after-free** | `Invalid read of size 4` (lee `data`, 4 bytes) |
| C | **Double free** | `Invalid free()` + `block of size 16 free'd` |
| D | **Valor no inicializado** | `Conditional jump depends on uninitialised value` |

En D, `n->next` fue alocado con `malloc` (que no inicializa). Comparar con
`NULL` usa un valor indeterminado. El fix: usar `calloc(1, sizeof(Node))` o
asignar `n->next = NULL` explícitamente.

</details>

---

### Ejercicio 2 — Corregir destrucción de lista

Este `list_destroy` tiene un bug. Identifícalo, predice qué reporta Valgrind,
y corrígelo:

```c
void list_destroy(List *list) {
    Node *cur = list->head;
    while (cur) {
        free(cur);
        cur = cur->next;
    }
    free(list);
}
```

**Prediccion**: ¿Valgrind reportará leak, use-after-free, o ambos?

<details><summary>Respuesta</summary>

**Bug**: `cur->next` se accede después de `free(cur)` — use-after-free.

Valgrind reportará:
```
Invalid read of size 8
   at list_destroy (list.c:XX)
 Address is 8 bytes inside a block of size 16 free'd
```

**Solo use-after-free**, no leak, porque a pesar del bug los nodos sí se
liberan (el valor de `cur->next` puede seguir intacto por casualidad). Pero
es comportamiento indefinido — en otro entorno podría causar crash o loop
infinito.

**Corrección**:

```c
void list_destroy(List *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;   // guardar ANTES de free
        free(cur);
        cur = next;
    }
    free(list);
}
```

</details>

---

### Ejercicio 3 — Destruir un árbol

Escribe `tree_destroy` para este árbol binario. Luego verifica con Valgrind
que no hay leaks:

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

TreeNode *tree_new_node(int data) {
    TreeNode *n = calloc(1, sizeof(TreeNode));
    n->data = data;
    return n;
}

int main(void) {
    TreeNode *root = tree_new_node(10);
    root->left = tree_new_node(5);
    root->right = tree_new_node(15);
    root->left->left = tree_new_node(2);
    root->left->right = tree_new_node(7);

    tree_destroy(root);   // implementar esta función
    return 0;
}
```

**Prediccion**: ¿Qué orden de recorrido debe usar `tree_destroy`?
¿Pre-order, in-order, o post-order?

<details><summary>Respuesta</summary>

Debe usar **post-order**: destruir hijos antes que el padre.

```c
void tree_destroy(TreeNode *node) {
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}
```

Si se usa **pre-order** (`free(node)` primero), se pierden los punteros
a los hijos → leak + use-after-free. Si se usa **in-order**, se libera
el nodo después del hijo izquierdo pero antes del derecho → leak del
subárbol derecho.

Verificación con Valgrind:

```bash
gcc -g -O0 -o tree_test tree_test.c
valgrind --leak-check=full ./tree_test
```

Resultado esperado:
```
total heap usage: 5 allocs, 5 frees, 120 bytes allocated
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

5 allocs = 5 nodos × `calloc`. 5 frees = 5 nodos liberados. $120 = 5 \times 24$
bytes (cada `TreeNode` tiene `int` + 2 punteros = $4 + 8 + 8 = 20$, padded a 24).

</details>

---

### Ejercicio 4 — Leak al eliminar del medio

Esta función elimina un nodo por valor pero tiene un leak. Encuentra el bug:

```c
bool list_remove(List *list, int value) {
    Node *prev = NULL;
    Node *cur = list->head;
    while (cur) {
        if (cur->data == value) {
            if (prev)
                prev->next = cur->next;
            else
                list->head = cur->next;
            list->size--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}
```

**Prediccion**: ¿Qué falta? ¿Valgrind reportará `definitely lost` o
`still reachable`?

<details><summary>Respuesta</summary>

**Falta `free(cur)`**. El nodo se desconecta de la lista (prev->next se
reconecta) pero nunca se libera.

```
definitely lost — ningún puntero apunta al nodo desconectado
```

**Corrección**:

```c
if (cur->data == value) {
    if (prev)
        prev->next = cur->next;
    else
        list->head = cur->next;
    free(cur);              // AGREGAR: liberar el nodo
    list->size--;
    return true;
}
```

Nota: `free(cur)` debe ir **después** de reconectar (`prev->next = cur->next`)
pero **antes** del `return`. Si se pone antes de reconectar, `cur->next` es
use-after-free.

</details>

---

### Ejercicio 5 — Valgrind con hash table

Una hash table con chaining usa un array de listas enlazadas. Escribe la
función de destrucción y verifica con Valgrind:

```c
typedef struct Entry {
    char *key;       // strdup'd (alocada con malloc)
    int value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    int capacity;
    int size;
} HashTable;

HashTable *ht_create(int capacity) {
    HashTable *ht = malloc(sizeof(HashTable));
    ht->buckets = calloc(capacity, sizeof(Entry *));
    ht->capacity = capacity;
    ht->size = 0;
    return ht;
}

void ht_insert(HashTable *ht, const char *key, int value) {
    int idx = hash(key) % ht->capacity;
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = value;
    e->next = ht->buckets[idx];
    ht->buckets[idx] = e;
    ht->size++;
}
```

**Prediccion**: ¿Cuántos `free` distintos necesita `ht_destroy` y en
qué orden?

<details><summary>Respuesta</summary>

Se necesitan 3 tipos de `free`, en este orden por cada entrada:

1. `free(entry->key)` — la clave fue alocada con `strdup`
2. `free(entry)` — la entrada misma
3. Después de recorrer todos los buckets: `free(ht->buckets)` y `free(ht)`

```c
void ht_destroy(HashTable *ht) {
    if (!ht) return;
    for (int i = 0; i < ht->capacity; i++) {
        Entry *cur = ht->buckets[i];
        while (cur) {
            Entry *next = cur->next;
            free(cur->key);     // 1. liberar la clave (strdup'd)
            free(cur);          // 2. liberar la entrada
            cur = next;
        }
    }
    free(ht->buckets);          // 3. liberar el array de buckets
    free(ht);                   // 4. liberar la struct HashTable
}
```

Si `ht` tiene `capacity = 16` y 10 entradas, los allocs/frees son:

| Alocación | Cantidad | Free necesario |
|-----------|---------|---------------|
| `malloc(HashTable)` | 1 | `free(ht)` |
| `calloc(buckets)` | 1 | `free(ht->buckets)` |
| `malloc(Entry)` | 10 | `free(entry)` por cada uno |
| `strdup(key)` | 10 | `free(entry->key)` por cada uno |
| **Total** | **22** | **22 frees** |

Valgrind esperado: `22 allocs, 22 frees, ... All heap blocks were freed`.

Olvidar `free(cur->key)` produce: `10 blocks definitely lost` (las claves).
Olvidar `free(ht->buckets)` produce: `1 block definitely lost` (el array).

</details>

---

### Ejercicio 6 — Interpretar reporte de Valgrind

Dado este reporte, responde: ¿cuántos nodos tiene la lista? ¿Cuántos se
liberaron? ¿Dónde está el bug?

```
==5678== HEAP SUMMARY:
==5678==     in use at exit: 32 bytes in 2 blocks
==5678==   total heap usage: 6 allocs, 4 frees, 96 bytes allocated
==5678==
==5678== 16 bytes in 1 blocks are definitely lost in loss record 1 of 2
==5678==    at 0x4C2FB55: malloc (...)
==5678==    by 0x4005E3: list_push_front (list.c:27)
==5678==    by 0x40068A: main (main.c:7)
==5678==
==5678== 16 bytes in 1 blocks are indirectly lost in loss record 2 of 2
==5678==    at 0x4C2FB55: malloc (...)
==5678==    by 0x4005E3: list_push_front (list.c:27)
==5678==    by 0x400680: main (main.c:6)
```

**Prediccion**: ¿Por qué hay 1 bloque `definitely lost` y 1 `indirectly lost`?

<details><summary>Respuesta</summary>

Análisis:
- **6 allocs**: 1 `List` + 5 `Node` (5 push_front)
- **4 frees**: 1 `List` + 3 `Node` (se liberaron 3 de 5 nodos)
- **2 bloques perdidos**: los últimos 2 nodos de la lista

El bloque **definitely lost** (main.c:7) es el nodo que estaba conectado al
último nodo liberado. Cuando `destroy` se detuvo prematuramente (o el loop
tenía un off-by-one), el puntero `next` del último nodo liberado ya no existe.

El bloque **indirectly lost** (main.c:6) es el nodo siguiente al definitely
lost. Solo era alcanzable a través del nodo definitely lost.

```
head -> [A] -> [B] -> [C] -> [D] -> [E] -> NULL
                               ↑       ↑
                         definitely   indirectly
                           lost        lost

El destroy liberó A, B, C y se detuvo (bug en el loop).
D es definitely lost (ningún puntero lo alcanza).
E es indirectly lost (solo alcanzable a través de D).
```

Probable bug: un `for` con condición incorrecta, ej:
```c
for (int i = 0; i < list->size - 2; i++) { ... }  // off-by-2
```

</details>

---

### Ejercicio 7 — `calloc` vs `malloc` para nodos

Compara estos dos enfoques y predice cuál genera errores con Valgrind:

```c
// Versión A: malloc
Node *node_a = malloc(sizeof(Node));
node_a->data = 42;
// no se asigna node_a->next

// Versión B: calloc
Node *node_b = calloc(1, sizeof(Node));
node_b->data = 42;
// no se asigna node_b->next explícitamente
```

**Prediccion**: ¿Cuál produce "Conditional jump depends on uninitialised
value" cuando se recorre la lista?

<details><summary>Respuesta</summary>

**Solo la versión A** produce el error.

- `malloc` no inicializa la memoria — `node_a->next` contiene basura
- `calloc` inicializa todo a cero — `node_b->next` es `NULL` (0x0)

Cuando la lista se recorre con `while (cur) { cur = cur->next; }`:
- Versión A: `cur->next` es un valor indeterminado → Valgrind reporta error
- Versión B: `cur->next` es `NULL` → el loop termina correctamente

**Recomendación para estructuras de datos**: usar `calloc` para nodos, o
siempre inicializar todos los campos explícitamente con `malloc`:

```c
Node *node = malloc(sizeof(Node));
if (!node) return NULL;
node->data = data;
node->next = NULL;   // SIEMPRE inicializar
```

`calloc` es más seguro como default, pero tiene un costo: inicializa todo
el bloque a cero, incluso campos que serán asignados inmediatamente. Para
bloques grandes (ej: `calloc(1000000, sizeof(int))`), esto puede importar.
Para nodos individuales, el costo es despreciable.

</details>

---

### Ejercicio 8 — Leak en lista circular

Una lista circular tiene `tail->next = head`. Escribe `destroy` para ella:

```c
typedef struct CNode {
    int data;
    struct CNode *next;
} CNode;

typedef struct {
    CNode *head;
    int size;
} CircularList;
```

La lista se ve así:

```
head -> [A] -> [B] -> [C] -+
         ^                  |
         +------------------+
```

**Prediccion**: ¿Por qué un `while (cur != NULL)` loop no funciona para
destruir una lista circular?

<details><summary>Respuesta</summary>

`while (cur != NULL)` nunca termina porque `cur` nunca es `NULL` — la lista
no tiene final, `tail->next` apunta de vuelta a `head`.

Dos estrategias correctas:

**Estrategia 1: contar nodos**

```c
void circular_destroy(CircularList *cl) {
    if (!cl) return;
    if (!cl->head) { free(cl); return; }

    CNode *cur = cl->head;
    for (int i = 0; i < cl->size; i++) {
        CNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(cl);
}
```

**Estrategia 2: romper el ciclo primero**

```c
void circular_destroy(CircularList *cl) {
    if (!cl) return;
    if (!cl->head) { free(cl); return; }

    // Encontrar el último nodo y romper el ciclo
    CNode *cur = cl->head;
    while (cur->next != cl->head)
        cur = cur->next;
    cur->next = NULL;           // romper: ahora es una lista lineal

    // Destruir como lista lineal normal
    cur = cl->head;
    while (cur) {
        CNode *next = cur->next;
        free(cur);
        cur = next;
    }
    free(cl);
}
```

La estrategia 1 es más simple y eficiente ($O(n)$ una pasada).
La estrategia 2 hace dos pasadas pero reutiliza la lógica de destrucción lineal.

</details>

---

### Ejercicio 9 — Suppressions: ignorar leaks de bibliotecas

A veces Valgrind reporta leaks que no son tu culpa (ej: leaks internos de
`glibc`, `dlopen`, o bibliotecas de terceros). Dado este reporte:

```
==9999== 72 bytes in 1 blocks are still reachable in loss record 1 of 1
==9999==    at 0x4C2FB55: malloc (...)
==9999==    by 0x4E6F12C: __nss_database_lookup (...)
==9999==    by 0x4E6DE94: getpwuid_r@@GLIBC_2.2.5 (...)
==9999==    by 0x400789: main (main.c:3)
```

**Prediccion**: ¿Es este un bug en tu código? ¿Cómo lo suprimirías?

<details><summary>Respuesta</summary>

**No es un bug en tu código**. Es memoria alocada por `glibc` internamente
(para cachear resultados de NSS/passwd). `glibc` no la libera al salir porque
el OS la recupera de todas formas. El tipo `still reachable` confirma que hay
punteros válidos apuntándola.

Para suprimirlo, crear un archivo de suppressions:

```
# glibc_nss.supp
{
   glibc_nss_leak
   Memcheck:Leak
   match-leak-kinds: reachable
   fun:malloc
   fun:__nss_database_lookup
   ...
}
```

Usar con:

```bash
valgrind --leak-check=full --suppressions=glibc_nss.supp ./program
```

O, más práctico, usar `--show-leak-kinds=definite,indirect` en vez de `all`
para ignorar `still reachable`:

```bash
valgrind --leak-check=full --show-leak-kinds=definite,indirect ./program
```

Esto muestra solo los leaks que son realmente tu responsabilidad.

</details>

---

### Ejercicio 10 — Auditoría completa con Valgrind

Dado este programa con múltiples bugs, predice cuántos errores reportará
Valgrind y de qué tipo:

```c
#include <stdlib.h>
#include <stdio.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

int main(void) {
    // Bug 1: ¿qué pasa aquí?
    Node *a = malloc(sizeof(Node));
    a->data = 1;

    // Bug 2: ¿y aquí?
    Node *b = malloc(sizeof(Node));
    b->data = 2;
    b->next = NULL;
    a->next = b;

    // Bug 3: ¿y aquí?
    free(a);
    printf("b->data = %d\n", a->next->data);

    // Bug 4: ¿y aquí?
    free(b);
    free(b);

    return 0;
}
```

**Prediccion**: Enumera cada error en orden de ejecución. ¿Cuántos errores
totales reportará Valgrind?

<details><summary>Respuesta</summary>

Errores en orden de ejecución:

1. **Valor no inicializado** (línea de `a`): `a->next` no fue asignado antes
   de `a->next = b` (línea 16). Pero espera — sí se asigna en la línea 16.
   El bug real es que `a->next` no fue inicializado **entre las líneas 12 y 16**,
   pero nadie lo lee en ese intervalo. Así que en este programa concreto, no hay
   error de no-inicialización (no se lee antes de asignar).

2. **Use-after-free** (línea 20): `a->next->data` accede a `a` después de
   `free(a)`. Valgrind reporta `Invalid read of size 8` (leer `a->next`, un
   puntero de 8 bytes).

3. **Use-after-free** (línea 20, segunda parte): si `a->next` logra leerse
   (y por casualidad sigue siendo `b`), entonces `a->next->data` lee `b->data`,
   que todavía es válido. Pero el primer acceso ya es inválido.

4. **Double free** (línea 23): `free(b)` se llama dos veces.

Valgrind reportará al menos:
- 1 `Invalid read of size 8` (use-after-free de `a->next`)
- 1 `Invalid free()` (double free de `b`)
- Posiblemente el programa crashe en el double free, previniendo la ejecución
  completa

Total: **2-3 errores** dependiendo de si el programa crashea en el double free.

Nota: `a` no genera leak porque fue liberado (línea 19). `b` no genera leak
porque fue liberado (línea 22). Los bugs son de acceso, no de leak.

</details>

Limpieza de todos los ejercicios:

```bash
rm -f test_list test_ds tree_test
```
