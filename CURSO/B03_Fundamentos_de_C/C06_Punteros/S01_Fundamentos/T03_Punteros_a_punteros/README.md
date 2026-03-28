# T03 — Punteros a punteros

## Doble indirección

Un puntero a puntero almacena la dirección de **otro puntero**:

```c
#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;       // p apunta a x
    int **pp = &p;     // pp apunta a p

    printf("x    = %d\n", x);       // 42
    printf("*p   = %d\n", *p);      // 42 — una indirección
    printf("**pp = %d\n", **pp);    // 42 — doble indirección

    // Modificar x a través de pp:
    **pp = 100;
    printf("x = %d\n", x);          // 100

    return 0;
}
```

```c
// Modelo mental:
//
// pp          p           x
// ┌────────┐  ┌────────┐  ┌────────┐
// │ &p     │→ │ &x     │→ │  42    │
// └────────┘  └────────┘  └────────┘
// int **      int *        int
//
// **pp: seguir pp → llegar a p → seguir p → llegar a x → 42
// *pp:  seguir pp → llegar a p → valor de p (dirección de x)
// pp:   dirección de p
```

```c
// Tipos y sizeof:
int x = 42;
int *p = &x;
int **pp = &p;
int ***ppp = &pp;    // triple puntero — raro pero legal

sizeof(x);      // 4  (int)
sizeof(p);      // 8  (puntero)
sizeof(pp);     // 8  (puntero — todos los punteros son 8 bytes en 64-bit)
sizeof(ppp);    // 8  (puntero)
```

## Uso principal: modificar un puntero del caller

```c
#include <stdio.h>
#include <stdlib.h>

// Para que una función modifique un int → pasar int*
// Para que una función modifique un int* → pasar int**

// INCORRECTO — modifica la copia del puntero:
void alloc_wrong(int *p) {
    p = malloc(sizeof(int));    // modifica la copia local de p
    if (p) *p = 42;
    // El caller no ve el cambio — memory leak
}

// CORRECTO — modifica el puntero original:
void alloc_correct(int **pp) {
    *pp = malloc(sizeof(int));  // modifica el puntero del caller
    if (*pp) **pp = 42;
}

int main(void) {
    int *data = NULL;

    alloc_wrong(data);
    printf("data = %p\n", (void *)data);    // NULL — no cambió

    alloc_correct(&data);
    printf("data = %p\n", (void *)data);    // dirección válida
    printf("*data = %d\n", *data);          // 42

    free(data);
    return 0;
}
```

## Patrón: función que aloca y retorna por puntero

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Retornar éxito/error, valor por puntero a puntero:
int read_line(char **out) {
    char buf[256];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return -1;
    }
    buf[strcspn(buf, "\n")] = '\0';

    *out = strdup(buf);    // aloca y copia
    if (*out == NULL) {
        return -1;
    }
    return 0;
}

int main(void) {
    char *line = NULL;

    if (read_line(&line) == 0) {
        printf("Read: '%s'\n", line);
        free(line);
    }

    return 0;
}
```

```c
// Otro patrón: linked list con puntero a puntero para inserción:

struct Node {
    int value;
    struct Node *next;
};

// Sin **: necesita caso especial para insertar al inicio
void insert_head_v1(struct Node **head, int val) {
    struct Node *new = malloc(sizeof(struct Node));
    new->value = val;
    new->next = *head;
    *head = new;    // modifica el puntero head del caller
}

int main(void) {
    struct Node *list = NULL;    // lista vacía
    insert_head_v1(&list, 10);  // list → [10]
    insert_head_v1(&list, 20);  // list → [20] → [10]
    // Funciona porque podemos modificar list a través de &list
    return 0;
}
```

## int** como array de strings

```c
// argv es el ejemplo más conocido de int** (bueno, char**):
int main(int argc, char **argv) {
    // argc = número de argumentos
    // argv = array de punteros a strings

    // argv:
    // ┌──────┐
    // │ ptr0 │→ "./program\0"
    // ├──────┤
    // │ ptr1 │→ "arg1\0"
    // ├──────┤
    // │ ptr2 │→ "arg2\0"
    // ├──────┤
    // │ NULL │   (terminador)
    // └──────┘

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    // char **argv es equivalente a char *argv[] como parámetro.
    return 0;
}
```

```c
// Crear un array de strings dinámico:
#include <stdlib.h>
#include <string.h>

char **create_string_array(int count) {
    char **arr = malloc(count * sizeof(char *));
    if (!arr) return NULL;

    for (int i = 0; i < count; i++) {
        arr[i] = NULL;    // inicializar a NULL
    }
    return arr;
}

void free_string_array(char **arr, int count) {
    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

int main(void) {
    char **names = create_string_array(3);
    names[0] = strdup("Alice");
    names[1] = strdup("Bob");
    names[2] = strdup("Charlie");

    for (int i = 0; i < 3; i++) {
        printf("%s\n", names[i]);
    }

    free_string_array(names, 3);
    return 0;
}
```

## Patrón: puntero a puntero para iterar linked list

```c
// Truco avanzado: usar int** para eliminar casos especiales
// en operaciones de linked list:

struct Node {
    int value;
    struct Node *next;
};

// Eliminar un nodo por valor — sin puntero a puntero:
void remove_v1(struct Node **head, int val) {
    // Caso especial: eliminar el head
    if (*head && (*head)->value == val) {
        struct Node *tmp = *head;
        *head = (*head)->next;
        free(tmp);
        return;
    }
    // Caso general: buscar en el resto
    for (struct Node *prev = *head; prev && prev->next; prev = prev->next) {
        if (prev->next->value == val) {
            struct Node *tmp = prev->next;
            prev->next = tmp->next;
            free(tmp);
            return;
        }
    }
}

// Eliminar un nodo por valor — con puntero a puntero:
void remove_v2(struct Node **pp, int val) {
    // pp apunta al "enlace" que queremos modificar
    // (sea head o el ->next de algún nodo)
    while (*pp) {
        if ((*pp)->value == val) {
            struct Node *tmp = *pp;
            *pp = (*pp)->next;    // modifica el enlace directamente
            free(tmp);
            return;
        }
        pp = &(*pp)->next;    // pp ahora apunta al campo next
    }
}
// v2 es más elegante: sin caso especial para head.
// Linus Torvalds ha señalado este patrón como ejemplo
// de "buen gusto" en programación.
```

## Triple puntero y más

```c
// Triple puntero (int ***) es raro. Ejemplo: array 2D dinámico
// donde la función aloca la matriz:

int create_matrix(int ***mat, int rows, int cols) {
    *mat = malloc(rows * sizeof(int *));
    if (!*mat) return -1;

    for (int i = 0; i < rows; i++) {
        (*mat)[i] = calloc(cols, sizeof(int));
        if (!(*mat)[i]) {
            for (int j = 0; j < i; j++) free((*mat)[j]);
            free(*mat);
            *mat = NULL;
            return -1;
        }
    }
    return 0;
}

int main(void) {
    int **matrix = NULL;
    create_matrix(&matrix, 3, 4);    // &matrix es int ***
    matrix[1][2] = 42;
    // ...liberar...
    return 0;
}

// Más de 3 niveles de indirección es señal de diseño incorrecto.
// "Three star programmer" es un anti-patrón.
```

---

## Ejercicios

### Ejercicio 1 — Alocar array en función

```c
// Implementar int create_array(int **arr, int n)
// que aloque un array de n ints y lo llene con valores 0..n-1.
// Retorna 0 si éxito, -1 si error.
// En main, llamar create_array, imprimir el array, y hacer free.
```

### Ejercicio 2 — Split string

```c
// Implementar int split(const char *str, char delim, char ***out)
// que divida str por delim y retorne un array de strings en *out.
// Retorna el número de tokens.
// Ejemplo: split("a,b,c", ',', &tokens) → 3, tokens = {"a","b","c"}
// El caller hace free de cada string y del array.
```

### Ejercicio 3 — Linked list con pp

```c
// Implementar una linked list con:
// - void push(struct Node **head, int val) — insertar al inicio
// - void remove_val(struct Node **head, int val) — eliminar por valor
// - void print_list(struct Node *head)
// Usar el patrón de puntero a puntero (v2 del ejemplo).
// Probar insertando y eliminando varios valores.
```
