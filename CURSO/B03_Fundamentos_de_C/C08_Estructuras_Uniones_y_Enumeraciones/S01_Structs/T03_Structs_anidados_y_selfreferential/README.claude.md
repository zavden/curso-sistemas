# T03 — Structs anidados y self-referential

> Sin erratas detectadas en el material original.

---

## 1. Structs anidados (composición)

Un struct puede contener otros structs como miembros. Esto es **composición**, no herencia — los datos del struct interno viven **dentro** del struct externo, contiguos en memoria:

```c
struct Date {
    int year;
    int month;
    int day;
};

struct Address {
    char street[100];
    char city[50];
    int zip_code;
};

struct Employee {
    char name[50];
    struct Date birthday;      // 12 bytes embebidos aquí
    struct Date hire_date;     // otros 12 bytes embebidos
    struct Address address;    // ~156 bytes embebidos
};
```

La diferencia con un **puntero** a otro struct es fundamental:

| Aspecto | Struct embebido | Puntero a struct |
|---------|-----------------|-----------------|
| Memoria | Datos dentro del struct externo | Datos en otra parte del heap |
| sizeof | Incluye el tamaño del interno | Solo 8 bytes (puntero) |
| Inicialización | Un solo bloque | Requiere malloc separado |
| Copia con `=` | Copia todos los datos | Copia solo la dirección (shallow) |
| Lifetime | Muere con el struct externo | Necesita free separado |

```c
struct Employee emp = {
    .name = "Alice",
    .birthday = { .year = 1990, .month = 5, .day = 15 },
    .hire_date = { .year = 2020, .month = 3, .day = 1 },
    .address = {
        .street = "742 Evergreen Terrace",
        .city = "Springfield",
        .zip_code = 62704,
    },
};
```

---

## 2. Acceso con doble punto y `ptr->miembro.campo`

### Variable directa: doble punto

```c
printf("Born: %d-%02d-%02d\n",
       emp.birthday.year,      // struct.miembro_struct.campo
       emp.birthday.month,
       emp.birthday.day);

printf("City: %s\n", emp.address.city);
```

### Puntero al struct externo: flecha + punto

```c
struct Employee *p = &emp;
printf("Born: %d\n", p->birthday.year);     // ptr->miembro.campo
printf("City: %s\n", p->address.city);
printf("Street: %s\n", p->address.street);
```

**Regla**: se usa `->` solo para el primer nivel (porque `p` es puntero). Los niveles internos usan `.` porque `birthday` y `address` no son punteros — son structs embebidos.

### Puntero al struct interno

Si tienes un puntero **al miembro interno**, usas `->` para ese nivel:

```c
struct Date *dp = &emp.birthday;
printf("Year: %d\n", dp->year);    // -> porque dp es puntero a Date
```

### Resumen de operadores por nivel

```c
emp.birthday.year          // variable.miembro.campo
p->birthday.year           // ptr->miembro.campo
dp->year                   // ptr_interno->campo
p->address.street          // ptr->miembro.campo (char array)
```

---

## 3. Structs self-referential (puntero a sí mismo)

Un struct puede contener un **puntero** a su propio tipo. No puede contener una **instancia** de sí mismo (tamaño infinito):

```c
// IMPOSIBLE — tamaño infinito:
// struct Node {
//     int value;
//     struct Node next;    // ERROR: incomplete type, tamaño desconocido
// };

// CORRECTO — puntero de tamaño fijo:
struct Node {
    int value;
    struct Node *next;    // OK — un puntero siempre es 8 bytes (64-bit)
};
```

¿Por qué funciona? Cuando el compilador ve `struct Node *next`, no necesita conocer el tamaño de `struct Node` — solo necesita saber que es un puntero (8 bytes). El tipo completo se resuelve después.

```c
// sizeof(struct Node) en 64-bit:
//   int value:         4 bytes
//   padding:           4 bytes (alinear puntero a 8)
//   struct Node *next: 8 bytes
//   Total:            16 bytes
```

Esto es la base de **todas** las estructuras de datos enlazadas: listas, árboles, grafos.

---

## 4. Lista enlazada simple: crear, insertar, recorrer

### Crear un nodo

Cada nodo se aloca individualmente en el heap:

```c
struct Node *node_create(int value) {
    struct Node *n = malloc(sizeof(struct Node));
    if (!n) return NULL;
    n->value = value;
    n->next = NULL;
    return n;
}
```

### Insertar al inicio (push) — O(1)

```c
void list_push(struct Node **head, int value) {
    struct Node *n = node_create(value);
    if (!n) return;
    n->next = *head;    // nuevo nodo apunta al head actual
    *head = n;          // head ahora es el nuevo nodo
}
```

Push es O(1) porque solo modifica punteros, sin recorrer la lista.

### Insertar al final (append) — O(n)

```c
void list_append(struct Node **head, int value) {
    struct Node *n = node_create(value);
    if (!n) return;

    if (*head == NULL) {
        *head = n;
        return;
    }

    struct Node *current = *head;
    while (current->next != NULL) {    // recorre hasta el último
        current = current->next;
    }
    current->next = n;
}
```

Append es O(n) porque debe recorrer toda la lista para encontrar el último nodo.

### Recorrer e imprimir

```c
void list_print(const struct Node *head) {
    for (const struct Node *n = head; n != NULL; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}
```

### Ejemplo de uso

```c
struct Node *list = NULL;

list_push(&list, 30);    // 30 -> NULL
list_push(&list, 20);    // 20 -> 30 -> NULL
list_push(&list, 10);    // 10 -> 20 -> 30 -> NULL

list_append(&list, 40);  // 10 -> 20 -> 30 -> 40 -> NULL
```

Push inserta al inicio → orden **inverso** al de inserción. Append inserta al final → mismo orden.

---

## 5. El doble puntero para modificar head

`list_push` recibe `struct Node **head` — un **puntero a puntero**. ¿Por qué?

### El problema con un solo puntero

```c
// MAL — no funciona:
void list_push_wrong(struct Node *head, int value) {
    struct Node *n = node_create(value);
    n->next = head;
    head = n;    // modifica la copia LOCAL de head
}
// El caller sigue apuntando al head anterior. El nuevo nodo se pierde.
```

En C, los argumentos se pasan **por valor**. `head` es una copia del puntero del caller. Modificar la copia no afecta al original.

### La solución con doble puntero

```c
// BIEN — modifica el puntero del caller:
void list_push(struct Node **head, int value) {
    struct Node *n = node_create(value);
    n->next = *head;    // *head = el valor del puntero del caller
    *head = n;          // modifica el puntero del caller
}

// Llamada:
struct Node *list = NULL;
list_push(&list, 10);    // &list = dirección del puntero list
// Ahora list apunta al nuevo nodo
```

La analogía: para modificar un `int` desde una función, pasas `int *`. Para modificar un `struct Node *` desde una función, pasas `struct Node **`.

---

## 6. Liberar una lista: guardar next antes de free

### El error clásico

```c
// MAL — use-after-free:
void list_free_wrong(struct Node *head) {
    while (head) {
        free(head);          // libera el nodo
        head = head->next;   // ACCEDE a memoria ya liberada → UB
    }
}
```

Después de `free(head)`, la memoria del nodo ya no pertenece al programa. Acceder a `head->next` es **use-after-free** — comportamiento indefinido que puede corromper datos o crashear.

### La solución: guardar antes de liberar

```c
void list_free(struct Node *head) {
    while (head) {
        struct Node *next = head->next;    // guardar ANTES de free
        free(head);
        head = next;                       // usar la copia guardada
    }
}
```

Este patrón aplica a **toda** estructura enlazada: siempre guardar las referencias que necesitas **antes** de liberar el nodo actual.

### Regla general

Al liberar una estructura enlazada, el orden es:
1. Guardar punteros a nodos vecinos
2. Liberar el nodo actual
3. Avanzar usando los punteros guardados

---

## 7. Lista doblemente enlazada

Cada nodo tiene punteros a su **anterior** y **siguiente**:

```c
struct DNode {
    int value;
    struct DNode *prev;
    struct DNode *next;
};
```

### Insertar después de un nodo

Requiere actualizar 4 punteros:

```c
void dlist_insert_after(struct DNode *node, int value) {
    struct DNode *new = malloc(sizeof(struct DNode));
    if (!new) return;

    new->value = value;
    new->next = node->next;       // 1. nuevo apunta al siguiente
    new->prev = node;             // 2. nuevo apunta al anterior

    if (node->next) {
        node->next->prev = new;   // 3. el siguiente apunta hacia atrás al nuevo
    }
    node->next = new;             // 4. el anterior apunta hacia adelante al nuevo
}
```

### Eliminar un nodo — O(1)

La ventaja clave sobre la lista simple: eliminar es O(1) si ya tienes el puntero al nodo, porque puedes acceder al anterior directamente con `node->prev`:

```c
void dlist_remove(struct DNode **head, struct DNode *node) {
    if (node->prev) {
        node->prev->next = node->next;    // bypass hacia adelante
    } else {
        *head = node->next;               // era el head
    }

    if (node->next) {
        node->next->prev = node->prev;    // bypass hacia atrás
    }

    free(node);
}
```

En una lista simple, eliminar un nodo requiere encontrar el anterior recorriendo la lista desde el inicio — O(n).

### Costo

Cada nodo en una lista doble ocupa 8 bytes más (un puntero extra). En 64-bit: `sizeof(DNode)` = 4 (int) + 4 (pad) + 8 (prev) + 8 (next) = **24 bytes** vs 16 bytes para un nodo simple.

---

## 8. Árbol binario de búsqueda (BST)

Un árbol binario es un struct con dos punteros self-referential: `left` (hijo izquierdo) y `right` (hijo derecho):

```c
struct TreeNode {
    int value;
    struct TreeNode *left;
    struct TreeNode *right;
};
```

### Inserción recursiva

En un BST, los valores menores van a la izquierda, los mayores a la derecha:

```c
struct TreeNode *tree_insert(struct TreeNode *root, int value) {
    if (!root) {
        struct TreeNode *n = malloc(sizeof(struct TreeNode));
        if (!n) return NULL;
        n->value = value;
        n->left = NULL;
        n->right = NULL;
        return n;
    }

    if (value < root->value) {
        root->left = tree_insert(root->left, value);
    } else if (value > root->value) {
        root->right = tree_insert(root->right, value);
    }
    // Duplicados se ignoran (no se insertan)
    return root;
}
```

### Recorrido inorder — orden ascendente

Visita: izquierda, raíz, derecha. En un BST, esto produce los valores **ordenados**:

```c
void tree_inorder(const struct TreeNode *root) {
    if (!root) return;
    tree_inorder(root->left);      // izquierda primero
    printf("%d ", root->value);    // raíz
    tree_inorder(root->right);     // derecha después
}
```

### Liberar un árbol (post-order)

Libera hijos **antes** que el padre — si liberas el padre primero, pierdes los punteros a los hijos:

```c
void tree_free(struct TreeNode *root) {
    if (!root) return;
    tree_free(root->left);     // libera subárbol izquierdo
    tree_free(root->right);    // libera subárbol derecho
    free(root);                // libera la raíz
}
```

### Ejemplo visual

```
// Insertar: 50, 30, 70, 20, 40
//
//        50
//       /  \
//     30    70
//    /  \
//  20    40
//
// Inorder: 20 30 40 50 70 (ordenado ascendente)
```

---

## 9. Structs mutuamente referenciados y forward declarations

Cuando dos structs necesitan punteros **entre sí**, se necesita una forward declaration:

```c
struct Department;    // forward declaration — "existe, pero aún no sé su contenido"

struct Employee {
    char name[50];
    struct Department *dept;    // OK — puntero a tipo incompleto
};

struct Department {
    char name[50];
    struct Employee *manager;   // OK — Employee ya está definido
    int employee_count;
};
```

### Por qué funciona

Un puntero tiene tamaño fijo (8 bytes en 64-bit) sin importar a qué tipo apunte. El compilador no necesita conocer el **contenido** de `struct Department` para saber el tamaño de `struct Department *`.

### Referencia circular

Los punteros pueden formar ciclos sin recursión infinita — el programa solo sigue los punteros que explícitamente accede:

```c
struct Employee alice = { .name = "Alice" };
struct Department eng = { .name = "Engineering", .manager = &alice };
alice.dept = &eng;

// Navegación circular:
printf("%s\n", alice.dept->manager->name);           // "Alice"
printf("%s\n", alice.dept->manager->dept->name);     // "Engineering"
// Podríamos seguir infinitamente, pero solo accedemos lo que pedimos
```

### Nota sobre forward declarations en C

En C, usar `struct Department *dept` sin declaración previa es técnicamente válido — el compilador crea implícitamente el tag como incomplete type. Pero la forward declaration explícita es la **práctica correcta** porque documenta la intención y evita problemas de scope.

---

## 10. Listas intrusivas (patrón del kernel) — Conexión con Rust

### Lista intrusiva vs lista convencional

En una lista **convencional**, el nodo contiene los datos:

```c
struct Node {
    int data;              // dato
    struct Node *next;     // enlace
};
```

En una lista **intrusiva**, el dato contiene el nodo de lista:

```c
struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

struct Task {
    int pid;
    char name[16];
    struct list_head list;    // nodo de lista DENTRO del dato
};
```

### Recuperar el dato con container_of

Para obtener el `struct Task` a partir de un `struct list_head *`, se usa `container_of`:

```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct list_head *current = /* algún nodo */;
struct Task *task = container_of(current, struct Task, list);
printf("PID: %d\n", task->pid);
```

### Ventajas

- **Un struct puede estar en múltiples listas** simultáneamente (con varios `list_head`)
- **Sin alocación extra** por nodo — no hay wrapper alrededor del dato
- **Genérico** — la misma implementación de lista funciona para cualquier struct

### Conexión con Rust

Rust toma un enfoque diferente a las estructuras enlazadas:

```rust
// En Rust, las listas enlazadas son "infames" por chocar con el ownership:
// Cada nodo tiene UN dueño. ¿Quién es dueño de cada nodo en una lista?

// Opción 1: Box (ownership único, como C con malloc):
struct Node {
    value: i32,
    next: Option<Box<Node>>,    // Box = puntero con ownership
}

// Opción 2: Rc (reference counting, como shared_ptr):
use std::rc::Rc;
use std::cell::RefCell;
struct DNode {
    value: i32,
    next: Option<Rc<RefCell<DNode>>>,
    prev: Option<Rc<RefCell<DNode>>>,   // necesita Rc para referencias circulares
}
```

| Concepto | C | Rust |
|----------|---|------|
| Lista simple | `struct Node *next` — trivial | `Option<Box<Node>>` — ownership claro |
| Lista doble | `*prev` + `*next` — trivial | Requiere `Rc<RefCell<>>` — complejo |
| Árbol | `*left` + `*right` — trivial | `Option<Box<TreeNode>>` — OK para árboles |
| Lista intrusiva | `container_of` + `list_head` | No idiomático — se usa `Vec` |
| Liberación | Manual, error-prone | Automática con `Drop` |
| Use-after-free | Posible | Imposible (compile-time check) |

**Lección clave**: C hace trivial crear estructuras enlazadas pero difícil gestionarlas sin errores. Rust hace difícil crear estructuras enlazadas pero imposible cometer errores de memoria. La mayoría del código Rust usa `Vec` en lugar de listas enlazadas — mejor cache locality y más simple.

---

## Ejercicios

### Ejercicio 1 — Struct anidado con inicialización

```c
// Declara:
//   struct Color { uint8_t r, g, b; };
//   struct Vertex { float x, y; struct Color color; };
//   struct Triangle { struct Vertex v[3]; };
//
// 1. Crea un Triangle con designated initializers:
//    v[0] = (0,0) rojo, v[1] = (10,0) verde, v[2] = (5,8) azul
// 2. Imprime los 3 vértices con su color: "v0 = (0.0, 0.0) color=(255,0,0)"
// 3. Accede via puntero: struct Triangle *tp = &tri;
//    Imprime tp->v[1].color.g
// 4. Imprime sizeof de cada struct
```

<details><summary>Predicción</summary>

- Acceso directo: `tri.v[0].color.r` → 255 (rojo)
- Acceso via puntero: `tp->v[1].color.g` → 255 (verde)
- sizeof(Color) = 3 bytes (3 × uint8_t). sizeof(Vertex) = 4(float x) + 4(float y) + 3(Color) + 1 pad = **12 bytes** (align 4). sizeof(Triangle) = 3 × 12 = **36 bytes**
- El padding de 1 byte en Vertex aparece porque el struct necesita alineación 4 (por los floats) y 8+3=11 → redondeado a 12
</details>

### Ejercicio 2 — Array de structs anidados dinámico

```c
// Declara:
//   struct Course { char name[50]; int credits; };
//   struct Student {
//       char name[50];
//       struct Course *courses;   // array dinámico
//       int num_courses;
//   };
//
// 1. Implementa student_create(name, num_courses) que:
//    - aloca el struct Student con malloc
//    - aloca el array de courses con malloc
//    - retorna el puntero (o NULL si falla)
// 2. Implementa student_free(s) que:
//    - libera courses primero, luego el struct
// 3. Crea 2 estudiantes con 3 cursos cada uno
// 4. Imprime nombre y total de créditos de cada uno
// 5. Verifica con valgrind: valgrind --leak-check=full ./students
```

<details><summary>Predicción</summary>

- `student_create` hace 2 mallocs: uno para el struct, otro para el array de courses
- `student_free` debe hacer `free(s->courses)` primero, luego `free(s)` (inside-out)
- Si se invierte el orden (free struct primero), `s->courses` es use-after-free
- Valgrind con liberación correcta: `All heap blocks were freed -- no leaks are possible`
- Cada Student usa: sizeof(Student) + num_courses × sizeof(Course) bytes en heap
</details>

### Ejercicio 3 — Lista enlazada: push, print, count, free

```c
// Implementa desde cero (sin copiar):
//   struct Node { int value; struct Node *next; };
//   struct Node *node_create(int value);
//   void list_push(struct Node **head, int value);
//   void list_print(const struct Node *head);
//   int  list_count(const struct Node *head);
//   void list_free(struct Node *head);
//
// En main:
// 1. Crea lista vacía: struct Node *list = NULL;
// 2. Push: 50, 40, 30, 20, 10 (en ese orden)
// 3. Imprime → debería ser: 10 -> 20 -> 30 -> 40 -> 50 -> NULL
// 4. Imprime count → 5
// 5. Libera y verifica con valgrind
```

<details><summary>Predicción</summary>

- Push inserta al inicio → el último en ser pushed (10) queda primero
- Resultado: `10 -> 20 -> 30 -> 40 -> 50 -> NULL`
- Count: 5
- `list_push` necesita `**head` porque modifica qué nodo es el head
- `list_free` debe guardar `next` antes de `free` en cada iteración
- Valgrind: 5 allocs, 5 frees, 0 leaks
</details>

### Ejercicio 4 — Lista enlazada: append

```c
// Agrega a tu implementación del ejercicio 3:
//   void list_append(struct Node **head, int value);
//
// En main:
// 1. Crea lista vacía
// 2. Append: 10, 20, 30, 40, 50
// 3. Imprime → debería ser: 10 -> 20 -> 30 -> 40 -> 50 -> NULL
// 4. Push 5 al inicio de esa lista
// 5. Append 60 al final
// 6. Imprime → 5 -> 10 -> 20 -> 30 -> 40 -> 50 -> 60 -> NULL
// 7. Count → 7
//
// ¿Por qué append necesita **head? ¿En qué caso especial?
```

<details><summary>Predicción</summary>

- Append conserva el orden de inserción: 10, 20, 30, 40, 50
- Con push(5) + append(60): `5 -> 10 -> 20 -> 30 -> 40 -> 50 -> 60 -> NULL`
- Count: 7
- `list_append` necesita `**head` por el caso especial de **lista vacía**: cuando `*head == NULL`, debe modificar `*head` para que apunte al nuevo nodo. Si la lista ya tiene elementos, solo modifica `current->next` y no necesitaría doble puntero
</details>

### Ejercicio 5 — Lista enlazada: find y remove

```c
// Agrega:
//   struct Node *list_find(const struct Node *head, int value);
//       → retorna puntero al nodo o NULL si no existe
//   int list_remove(struct Node **head, int value);
//       → elimina el primer nodo con ese valor, retorna 1 si eliminó, 0 si no
//
// En main:
// 1. Crea lista: 10 -> 20 -> 30 -> 40 -> 50
// 2. list_find(list, 30) → debería retornar puntero != NULL
// 3. list_find(list, 99) → debería retornar NULL
// 4. list_remove(&list, 30) → elimina el medio
//    Imprime: 10 -> 20 -> 40 -> 50 -> NULL
// 5. list_remove(&list, 10) → elimina el head
//    Imprime: 20 -> 40 -> 50 -> NULL
// 6. list_remove(&list, 50) → elimina el último
//    Imprime: 20 -> 40 -> NULL
// 7. list_remove(&list, 99) → retorna 0 (no existe)
```

<details><summary>Predicción</summary>

- `list_remove` necesita rastrear el nodo **anterior** al que se elimina para re-enlazar
- Caso especial: eliminar el head → `*head = (*head)->next`
- Caso general: `prev->next = current->next` y luego `free(current)`
- Importante: guardar `current->next` en variable antes de `free(current)` (aunque aquí no es necesario porque ya re-enlazamos con `prev->next`)
- Truco alternativo: usar `struct Node **pp = &head` que apunta al puntero `next` del anterior, evitando el caso especial del head
</details>

### Ejercicio 6 — Invertir una lista in-place

```c
// Implementa:
//   void list_reverse(struct Node **head);
//       → invierte la lista sin alocar memoria nueva
//
// En main:
// 1. Crea lista: 10 -> 20 -> 30 -> 40 -> 50
// 2. list_reverse(&list)
// 3. Imprime: 50 -> 40 -> 30 -> 20 -> 10 -> NULL
// 4. list_reverse(&list) de nuevo
// 5. Imprime: 10 -> 20 -> 30 -> 40 -> 50 -> NULL (original)
//
// Pista: necesitas 3 punteros: prev, current, next_node.
// En cada paso: guardar next, invertir el enlace, avanzar.
```

<details><summary>Predicción</summary>

Algoritmo:
```
prev = NULL, current = head
En cada paso:
  1. next_node = current->next     (guardar antes de perderlo)
  2. current->next = prev          (invertir el enlace)
  3. prev = current                (avanzar prev)
  4. current = next_node           (avanzar current)
Al final: *head = prev
```

- Es O(n) en tiempo, O(1) en espacio (solo 3 punteros auxiliares)
- Doble reverse devuelve al orden original
- Funciona con lista vacía (current == NULL, no entra al loop)
- Funciona con un solo nodo (un paso: 10->NULL se convierte en NULL<-10)
</details>

### Ejercicio 7 — Lista doblemente enlazada

```c
// Implementa desde cero:
//   struct DNode { int value; struct DNode *prev; struct DNode *next; };
//   void dlist_push(struct DNode **head, int value);
//   void dlist_insert_after(struct DNode *node, int value);
//   void dlist_remove(struct DNode **head, struct DNode *node);
//   void dlist_print_forward(const struct DNode *head);
//   void dlist_print_backward(const struct DNode *head);
//       → recorre hasta el final, luego imprime con prev
//   void dlist_free(struct DNode *head);
//
// En main:
// 1. Push: 30, 20, 10 → 10 <-> 20 <-> 30
// 2. print_forward: 10 -> 20 -> 30 -> NULL
// 3. print_backward: 30 -> 20 -> 10 -> NULL
// 4. Inserta 25 después del nodo con valor 20
// 5. print_forward: 10 -> 20 -> 25 -> 30 -> NULL
// 6. Remove el nodo con valor 20
// 7. print_forward: 10 -> 25 -> 30 -> NULL
// 8. Valgrind: --leak-check=full
```

<details><summary>Predicción</summary>

- `dlist_push` debe actualizar `prev` del antiguo head: `(*head)->prev = new`
- `dlist_insert_after` actualiza 4 punteros (como se muestra en la teoría)
- `dlist_remove` del medio: re-enlaza prev y next del nodo eliminado
- `print_backward`: primero recorre hasta el final (`while (n->next)`), luego retrocede con `n->prev`
- sizeof(DNode) = 4(int) + 4(pad) + 8(prev) + 8(next) = **24 bytes** vs 16 del nodo simple
- La ventaja: remove es O(1) dado el puntero al nodo (no necesita buscar el anterior)
</details>

### Ejercicio 8 — Árbol BST con búsqueda y estadísticas

```c
// Implementa:
//   struct TreeNode { int value; struct TreeNode *left, *right; };
//   struct TreeNode *tree_insert(struct TreeNode *root, int value);
//   void tree_inorder(const struct TreeNode *root);
//   struct TreeNode *tree_find(const struct TreeNode *root, int value);
//   int tree_min(const struct TreeNode *root);   // valor mínimo
//   int tree_max(const struct TreeNode *root);   // valor máximo
//   int tree_height(const struct TreeNode *root); // altura
//   int tree_count(const struct TreeNode *root);  // número de nodos
//   void tree_free(struct TreeNode *root);
//
// En main, inserta: 50, 30, 70, 20, 40, 60, 80
// Imprime inorder, min, max, height, count
// Busca 40 (existe) y 45 (no existe)
```

<details><summary>Predicción</summary>

Árbol resultante:
```
        50
       /  \
     30    70
    / \   / \
  20  40 60  80
```

- Inorder: `20 30 40 50 60 70 80`
- Min: 20 (nodo más a la izquierda)
- Max: 80 (nodo más a la derecha)
- Height: 3 (3 niveles: raíz, hijos, nietos). Nota: la convención varía — si la raíz sola tiene height 1, entonces 3; si tiene height 0, entonces 2
- Count: 7 nodos
- tree_find(40) → puntero al nodo. tree_find(45) → NULL
- tree_min: recorre `root->left` hasta NULL. tree_max: recorre `root->right` hasta NULL
- tree_free usa post-order (hijos antes que padre)
</details>

### Ejercicio 9 — Structs mutuamente referenciados: universidad

```c
// Modela:
//   struct Professor; // forward declaration
//   struct Course {
//       char name[50];
//       struct Professor *instructor;
//   };
//   struct Professor {
//       char name[50];
//       struct Course **courses;    // array de punteros
//       int num_courses;
//   };
//
// 1. Crea 2 profesores en el stack
// 2. Crea 4 cursos en el stack
// 3. Asigna: prof1 enseña cursos 1 y 2, prof2 enseña cursos 3 y 4
// 4. Los cursos apuntan a su instructor
// 5. Navega: desde un curso, imprime el nombre del profesor
// 6. Navega: desde un profesor, imprime sus cursos
// 7. Navega circular: curso → profesor → curso[0] → nombre
```

<details><summary>Predicción</summary>

- `courses` en Professor es `struct Course **` — un array de punteros a Course (no un array de Course)
- Para Prof1: `prof1.courses = (struct Course *[]){&c1, &c2}` o usar un array local
- Navegación: `c1.instructor->name` → nombre del prof1
- Navegación: `prof1.courses[0]->name` → nombre del curso 1
- Circular: `c1.instructor->courses[0]->name` → nombre del curso 1 (vuelve al inicio)
- Todo está en el stack → no se necesita malloc ni free
- La forward declaration de Professor es necesaria porque Course lo referencia antes de su definición
</details>

### Ejercicio 10 — Lista intrusiva simplificada

```c
// Implementa una lista intrusiva simplificada:
//
// struct list_node { struct list_node *next; };
//
// #define container_of(ptr, type, member) \
//     ((type *)((char *)(ptr) - offsetof(type, member)))
//
// struct Task {
//     int pid;
//     char name[32];
//     struct list_node node;    // nodo intrusivo
// };
//
// Implementa usando struct list_node (NO struct Task):
//   void list_push(struct list_node **head, struct list_node *new_node);
//   void list_print_tasks(const struct list_node *head);
//       → usa container_of para obtener cada Task
//   void list_free_tasks(struct list_node *head);
//       → usa container_of para obtener cada Task y hacer free
//
// En main:
// 1. Crea 3 Tasks con malloc: {1,"init"}, {42,"bash"}, {100,"nginx"}
// 2. Push los 3 usando &task->node
// 3. Imprime con list_print_tasks
// 4. Libera con list_free_tasks
// 5. Verifica con valgrind
```

<details><summary>Predicción</summary>

- `list_push` es genérica — solo manipula `struct list_node`, no sabe nada de Task
- `list_print_tasks` recorre `list_node` y usa `container_of(current, struct Task, node)` para obtener el Task completo
- Push invierte el orden: nginx(100) → bash(42) → init(1) → NULL
- `offsetof(struct Task, node)` = 4(pid) + 32(name) = 36. Con padding para alinear el puntero dentro de list_node: probablemente 40 (36 → 40 para alinear a 8)
- container_of resta ese offset del puntero a `node` para obtener el inicio del Task
- Valgrind: 3 allocs (uno por Task), 3 frees, 0 leaks
- Ventaja: la misma `list_push` funciona para cualquier struct que contenga un `list_node`
</details>
