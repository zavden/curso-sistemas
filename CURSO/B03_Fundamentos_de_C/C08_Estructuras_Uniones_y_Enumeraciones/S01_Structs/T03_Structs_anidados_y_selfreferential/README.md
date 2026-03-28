# T03 — Structs anidados y self-referential

## Structs anidados

Un struct puede contener otros structs como miembros:

```c
#include <stdio.h>

struct Date {
    int year;
    int month;
    int day;
};

struct Person {
    char name[50];
    struct Date birthday;    // struct anidado
    struct Date hire_date;
};

int main(void) {
    struct Person emp = {
        .name = "Alice",
        .birthday = { .year = 1990, .month = 5, .day = 15 },
        .hire_date = { .year = 2020, .month = 3, .day = 1 },
    };

    // Acceso con doble punto:
    printf("Born: %d-%02d-%02d\n",
           emp.birthday.year,
           emp.birthday.month,
           emp.birthday.day);

    // Con puntero:
    struct Person *p = &emp;
    printf("Hired: %d-%02d-%02d\n",
           p->hire_date.year,
           p->hire_date.month,
           p->hire_date.day);

    return 0;
}
```

```c
// Structs anónimos anidados (C11):
struct Vector3 {
    union {
        struct {
            float x, y, z;    // struct anónimo dentro de union
        };
        float data[3];
    };
};

struct Vector3 v = { .x = 1.0f, .y = 2.0f, .z = 3.0f };
// Los campos x, y, z se acceden directamente:
printf("x = %f\n", v.x);
// También como array:
printf("y = %f\n", v.data[1]);
```

## Structs self-referential

Un struct puede contener un puntero **a sí mismo**.
No puede contener una instancia de sí mismo (tamaño infinito):

```c
// INCORRECTO — tamaño infinito:
// struct Node {
//     int value;
//     struct Node next;    // ERROR: incomplete type
// };

// CORRECTO — puntero a sí mismo:
struct Node {
    int value;
    struct Node *next;    // OK — un puntero tiene tamaño fijo (8 bytes)
};
```

## Lista enlazada simple (singly linked list)

```c
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

// Crear un nodo:
struct Node *node_create(int value) {
    struct Node *n = malloc(sizeof(struct Node));
    if (!n) return NULL;
    n->value = value;
    n->next = NULL;
    return n;
}

// Insertar al inicio:
void list_push(struct Node **head, int value) {
    struct Node *n = node_create(value);
    if (!n) return;
    n->next = *head;
    *head = n;
}

// Imprimir la lista:
void list_print(const struct Node *head) {
    for (const struct Node *n = head; n != NULL; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}

// Liberar toda la lista:
void list_free(struct Node *head) {
    while (head) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    struct Node *list = NULL;

    list_push(&list, 30);
    list_push(&list, 20);
    list_push(&list, 10);

    list_print(list);    // 10 -> 20 -> 30 -> NULL

    list_free(list);
    return 0;
}
```

```c
// Visualización en memoria:
//
// list → [10|next] → [20|next] → [30|next] → NULL
//         ↑ head
//
// Cada nodo está en una dirección diferente del heap.
// El campo next conecta los nodos.
```

## Lista doblemente enlazada

```c
struct DNode {
    int value;
    struct DNode *prev;
    struct DNode *next;
};

// Insertar después de un nodo:
void dlist_insert_after(struct DNode *node, int value) {
    struct DNode *new = malloc(sizeof(struct DNode));
    if (!new) return;

    new->value = value;
    new->next = node->next;
    new->prev = node;

    if (node->next) {
        node->next->prev = new;
    }
    node->next = new;
}

// Eliminar un nodo:
void dlist_remove(struct DNode **head, struct DNode *node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        *head = node->next;    // era el head
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    free(node);
}

// Ventaja sobre lista simple: eliminar un nodo es O(1)
// si ya tienes el puntero (no necesitas buscar el anterior).
```

## Árbol binario

```c
#include <stdio.h>
#include <stdlib.h>

struct TreeNode {
    int value;
    struct TreeNode *left;
    struct TreeNode *right;
};

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
    return root;
}

// Recorrido inorder (izquierda, raíz, derecha) → orden ascendente:
void tree_inorder(const struct TreeNode *root) {
    if (!root) return;
    tree_inorder(root->left);
    printf("%d ", root->value);
    tree_inorder(root->right);
}

// Liberar todo el árbol:
void tree_free(struct TreeNode *root) {
    if (!root) return;
    tree_free(root->left);
    tree_free(root->right);
    free(root);
}

int main(void) {
    struct TreeNode *tree = NULL;

    tree = tree_insert(tree, 50);
    tree_insert(tree, 30);
    tree_insert(tree, 70);
    tree_insert(tree, 20);
    tree_insert(tree, 40);

    tree_inorder(tree);    // 20 30 40 50 70
    printf("\n");

    tree_free(tree);
    return 0;
}
```

```c
// Visualización:
//
//        50
//       /  \
//     30    70
//    /  \
//  20    40
//
// Inorder: 20, 30, 40, 50, 70 (ordenado)
```

## Structs mutuamente referenciados

```c
// Dos structs que se refieren entre sí:
// Necesitan forward declarations.

struct Department;    // forward declaration

struct Employee {
    char name[50];
    struct Department *dept;    // puntero a Department
};

struct Department {
    char name[50];
    struct Employee *manager;  // puntero a Employee
    struct Employee **staff;
    int staff_count;
};

// Ahora ambos tipos están completos:
struct Employee alice = { .name = "Alice", .dept = NULL };
struct Department eng = { .name = "Engineering", .manager = &alice };
alice.dept = &eng;
```

## Intrusive linked list (patrón del kernel)

```c
// En el kernel de Linux, la estructura de lista NO contiene datos.
// El struct del dato contiene el nodo de lista:

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

// El dato contiene un list_head como campo:
struct Task {
    int pid;
    char name[16];
    struct list_head list;    // nodo de lista dentro del dato
};

// Para obtener el Task a partir del list_head, usar container_of:
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Ventaja: un mismo struct puede estar en múltiples listas.
// No hay una alocación extra por nodo.
// Es el patrón fundamental del kernel de Linux.
```

---

## Ejercicios

### Ejercicio 1 — Lista enlazada completa

```c
// Implementar una lista enlazada simple con:
// - list_push(head, value) — insertar al inicio
// - list_append(head, value) — insertar al final
// - list_find(head, value) — buscar un valor (retorna Node* o NULL)
// - list_remove(head, value) — eliminar por valor
// - list_reverse(head) — invertir la lista in-place
// - list_print(head)
// - list_free(head)
// Verificar con valgrind.
```

### Ejercicio 2 — Árbol BST

```c
// Extender el árbol binario con:
// - tree_find(root, value) — retorna el nodo o NULL
// - tree_min(root) — retorna el valor mínimo
// - tree_max(root) — retorna el valor máximo
// - tree_height(root) — retorna la altura
// Probar con: insertar 50, 30, 70, 20, 40, 60, 80.
```

### Ejercicio 3 — Struct anidado complejo

```c
// Modelar una universidad con structs anidados:
// - struct Course { char name[100]; int credits; }
// - struct Student { char name[50]; struct Course *courses; int num_courses; }
// - struct Department { char name[50]; struct Student *students; int num_students; }
// Crear datos de ejemplo (2 departamentos, 3 estudiantes, 4 cursos).
// Alocar todo dinámicamente. Implementar department_free.
// Verificar con valgrind.
```
