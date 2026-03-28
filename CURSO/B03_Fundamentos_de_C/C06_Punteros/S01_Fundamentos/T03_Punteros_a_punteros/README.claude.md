# T03 — Punteros a punteros

> Sin erratas detectadas en el material base.

---

## 1 — Doble indirección: int **pp

Un puntero a puntero almacena la dirección de **otro puntero**:

```c
int x = 42;
int *p = &x;       // p apunta a x
int **pp = &p;     // pp apunta a p

printf("x    = %d\n", x);       // 42
printf("*p   = %d\n", *p);      // 42 — una indirección
printf("**pp = %d\n", **pp);    // 42 — doble indirección

**pp = 100;        // modifica x a través de dos niveles
printf("x = %d\n", x);          // 100
```

```
pp           p            x
┌────────┐   ┌────────┐   ┌────────┐
│  &p    │ → │  &x    │ → │   42   │
└────────┘   └────────┘   └────────┘
 int **       int *         int
```

Cada desreferencia "sigue una flecha":
- `pp` = dirección de `p`
- `*pp` = valor de `p` = dirección de `x`
- `**pp` = valor de `x` = 42

---

## 2 — Modelo mental y sizeof

```c
int x = 42;
int *p = &x;
int **pp = &p;
int ***ppp = &pp;    // triple puntero — legal pero raro

sizeof(x);      // 4 (int)
sizeof(p);      // 8 (puntero — 64-bit)
sizeof(pp);     // 8 (puntero)
sizeof(ppp);    // 8 (puntero)
```

Todos los punteros miden 8 bytes en 64-bit, independientemente del nivel de indirección. El tipo (`int *`, `int **`, `int ***`) determina qué pasa al desreferenciar, no el tamaño del puntero.

---

## 3 — Uso principal: modificar un puntero del caller

La regla fundamental: para modificar un valor de tipo `T` en el caller, la función necesita recibir `T *`:
- Modificar un `int` → recibir `int *`
- Modificar un `int *` → recibir `int **`

```c
// INCORRECTO — modifica la copia local del puntero:
void alloc_wrong(int *p) {
    p = malloc(sizeof(int));  // modifica la copia local
    if (p) *p = 42;
    // El caller no ve el cambio → memory leak
}

// CORRECTO — modifica el puntero original:
void alloc_correct(int **pp) {
    *pp = malloc(sizeof(int));  // modifica el puntero del caller
    if (*pp) **pp = 42;
}

int main(void) {
    int *data = NULL;

    alloc_wrong(data);
    // data sigue siendo NULL — la copia local fue modificada y descartada

    alloc_correct(&data);
    // data apunta a memoria válida con valor 42

    free(data);
    return 0;
}
```

`alloc_wrong` es un error muy común: pasar un puntero a una función que necesita modificarlo. La función recibe una **copia** del puntero — modificar la copia no afecta al original.

---

## 4 — Patrón: función que aloca y retorna por puntero

```c
#include <string.h>

// Retorna éxito/error, valor por puntero a puntero:
int read_line(char **out) {
    char buf[256];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return -1;
    }
    buf[strcspn(buf, "\n")] = '\0';

    *out = strdup(buf);    // aloca y copia
    if (*out == NULL) return -1;
    return 0;
}

int main(void) {
    char *line = NULL;
    if (read_line(&line) == 0) {
        printf("Read: '%s'\n", line);
        free(line);    // el caller libera
    }
    return 0;
}
```

Este patrón separa el resultado de la operación (`return 0/-1`) del valor producido (`*out`). Es el estándar en C para funciones que alocan memoria internamente.

---

## 5 — char **argv: el ejemplo canónico

`argv` es el ejemplo más conocido de puntero a puntero:

```c
int main(int argc, char **argv) {
    // argc = número de argumentos (incluyendo el programa)
    // argv = array de punteros a strings

    // argv:
    // ┌──────┐
    // │ ptr0 │→ "./program\0"
    // ├──────┤
    // │ ptr1 │→ "arg1\0"
    // ├──────┤
    // │ ptr2 │→ "arg2\0"
    // ├──────┤
    // │ NULL │   (terminador — argv[argc] siempre es NULL)
    // └──────┘

    // Recorrer con índice:
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    // Recorrer con puntero a puntero (usando el terminador NULL):
    for (char **p = argv; *p != NULL; p++) {
        printf("%s\n", *p);
    }

    // Acceso carácter por carácter:
    argv[0][0];   // primer carácter del nombre del programa
}
```

`char **argv` es equivalente a `char *argv[]` como parámetro de función. Sin argumentos, `argc` es 1 (solo el nombre del programa).

---

## 6 — char ** dinámico: crear y liberar

```c
char **create_string_array(int count) {
    char **arr = malloc(count * sizeof(char *));
    if (!arr) return NULL;
    for (int i = 0; i < count; i++) {
        arr[i] = NULL;
    }
    return arr;
}

void free_string_array(char **arr, int count) {
    for (int i = 0; i < count; i++) {
        free(arr[i]);    // liberar cada string
    }
    free(arr);           // liberar el array de punteros
}
```

**Orden de liberación**: siempre desde lo más interno (las strings individuales) hacia lo más externo (el array de punteros). Si se libera `arr` primero, se pierden las direcciones de las strings → memory leak.

Para un jagged array (`int **`), la misma lógica aplica:
- **Alocar**: 1 `malloc` para el array de punteros + 1 `malloc` por fila
- **Liberar**: primero cada fila, luego el array de punteros

---

## 7 — Puntero a puntero en linked lists

### Inserción al inicio

```c
struct Node {
    int value;
    struct Node *next;
};

void push(struct Node **head, int val) {
    struct Node *new_node = malloc(sizeof(struct Node));
    new_node->value = val;
    new_node->next = *head;
    *head = new_node;    // modifica el puntero head del caller
}

struct Node *list = NULL;
push(&list, 10);    // list → [10]
push(&list, 20);    // list → [20] → [10]
```

### Eliminación elegante (patrón Torvalds)

Sin `**`, eliminar un nodo requiere caso especial para el head. Con `**`:

```c
void remove_val(struct Node **pp, int val) {
    while (*pp) {
        if ((*pp)->value == val) {
            struct Node *tmp = *pp;
            *pp = (*pp)->next;   // modifica el enlace directamente
            free(tmp);
            return;
        }
        pp = &(*pp)->next;   // pp ahora apunta al campo next
    }
}
```

`pp` siempre apunta al "enlace" que hay que modificar — sea `head` o el `->next` de algún nodo. No hay caso especial porque `head` y `->next` se tratan igual a través de la doble indirección.

Linus Torvalds ha señalado este patrón como ejemplo de "buen gusto" en programación: elimina la duplicación de lógica entre el caso del head y el caso general.

---

## 8 — Triple puntero y más

```c
// Triple puntero: la función aloca una matriz 2D
int create_matrix(int ***mat, int rows, int cols) {
    *mat = malloc(rows * sizeof(int *));
    if (!*mat) return -1;
    for (int i = 0; i < rows; i++) {
        (*mat)[i] = calloc(cols, sizeof(int));
    }
    return 0;
}

int **matrix = NULL;
create_matrix(&matrix, 3, 4);  // &matrix es int ***
matrix[1][2] = 42;
```

Más de 3 niveles de indirección (`int ****`) es un anti-patrón conocido como "three-star programmer" — señal de que el diseño necesita reestructuración, probablemente usando structs.

---

## 9 — Comparación con Rust

| Aspecto | C (`int **`) | Rust |
|---------|-------------|------|
| Modificar puntero del caller | `void f(int **pp)` | `fn f(p: &mut Box<i32>)` o retornar el valor |
| Array de strings | `char **argv` | `Vec<String>` o `&[&str]` |
| Linked list insert | `push(struct Node **head, ...)` | `list.push_front(val)` (ownership) |
| Jagged array | `int **` + malloc por fila | `Vec<Vec<i32>>` (automático) |
| NULL/leak | Responsabilidad del programador | Ownership system previene leaks |

```rust
// En Rust, no se necesitan punteros a punteros para los casos comunes:

// Modificar un valor del caller:
fn set_value(val: &mut Option<i32>) {
    *val = Some(42);
}

// Array de strings:
let args: Vec<String> = std::env::args().collect();

// Vector de vectores (jagged):
let matrix: Vec<Vec<i32>> = vec![
    vec![10, 11],
    vec![20, 21, 22, 23, 24],
    vec![30, 31, 32],
    vec![40],
];
// La liberación es automática — no hay orden de free que recordar
```

Rust elimina la necesidad de doble indirección en la mayoría de casos mediante ownership, borrowing, y tipos como `Option<T>` y `Box<T>`.

---

## Ejercicios

### Ejercicio 1 — Rastrear doble indirección

```c
// ¿Qué imprime? Rastrea pp, p, y x en cada paso.
#include <stdio.h>

int main(void) {
    int x = 10, y = 20;
    int *p = &x;
    int **pp = &p;

    printf("**pp = %d\n", **pp);

    *pp = &y;          // ¿qué modifica esto?
    printf("*p = %d\n", *p);
    printf("x = %d, y = %d\n", x, y);

    **pp = 99;         // ¿qué modifica esto?
    printf("x = %d, y = %d\n", x, y);
    return 0;
}
```

<details><summary>Predicción</summary>

```
**pp = 10
*p = 20
x = 10, y = 20
x = 10, y = 99
```

Paso a paso:
- `**pp = 10`: `pp→p→x`, `x = 10`
- `*pp = &y`: modifica `p` (no `pp`). Ahora `p = &y`. `pp` sigue apuntando a `p`.
- `*p = 20`: ahora `p` apunta a `y`, así que `*p = 20`
- `x = 10, y = 20`: `x` no fue modificada (solo se redirigió `p`)
- `**pp = 99`: `pp→p→y`, modifica `y` a 99. `x` no se toca.
- `x = 10, y = 99`

El punto clave: `*pp = &y` no modifica `x` ni `y` — modifica el **puntero `p`**, haciéndolo apuntar a `y` en lugar de a `x`.

</details>

### Ejercicio 2 — alloc_wrong vs alloc_correct

```c
// ¿Qué imprime? ¿Hay memory leaks?
#include <stdio.h>
#include <stdlib.h>

void alloc_v1(int *p) {
    p = malloc(sizeof(int));
    if (p) *p = 42;
}

void alloc_v2(int **pp) {
    *pp = malloc(sizeof(int));
    if (*pp) **pp = 42;
}

int *alloc_v3(void) {
    int *p = malloc(sizeof(int));
    if (p) *p = 42;
    return p;
}

int main(void) {
    int *a = NULL, *b = NULL, *c = NULL;

    alloc_v1(a);
    printf("a = %p\n", (void *)a);

    alloc_v2(&b);
    printf("b = %p, *b = %d\n", (void *)b, *b);

    c = alloc_v3();
    printf("c = %p, *c = %d\n", (void *)c, *c);

    free(b);
    free(c);
    return 0;
}
```

<details><summary>Predicción</summary>

```
a = (nil)
b = 0x..., *b = 42
c = 0x..., *c = 42
```

- **alloc_v1**: recibe una **copia** de `a` (que es NULL). Modifica la copia local, aloca memoria y escribe 42. Pero `a` en `main` sigue siendo NULL. La memoria alocada se pierde → **memory leak**.
- **alloc_v2**: recibe `&b` (la dirección del puntero `b`). `*pp = malloc(...)` modifica `b` directamente. Después de la llamada, `b` apunta a memoria válida con valor 42. Sin leak (se hace `free(b)`).
- **alloc_v3**: aloca internamente y **retorna** el puntero. `c` recibe el valor retornado. Funciona correctamente. Sin leak (se hace `free(c)`).

Tres formas de que una función proporcione memoria:
1. `void f(T **out)` — output parameter (modifica puntero del caller)
2. `T *f(void)` — retornar el puntero (más simple pero solo retorna un valor)
3. `void f(T *p)` — **NO FUNCIONA** para alocar (modifica copia local)

</details>

### Ejercicio 3 — argv exploración

```c
// Si se ejecuta como: ./prog hello world 42
// ¿Qué imprime?
#include <stdio.h>

int main(int argc, char **argv) {
    printf("argc = %d\n", argc);

    for (char **p = argv; *p != NULL; p++) {
        printf("[%td] \"%s\" (len=%d)\n",
               p - argv, *p, 0);  // [A] ¿cómo calcular len?
    }

    // Acceso a caracteres individuales:
    if (argc > 1) {
        printf("\nFirst arg chars: ");
        for (int i = 0; argv[1][i] != '\0'; i++) {
            printf("'%c' ", argv[1][i]);
        }
        printf("\n");
    }

    printf("argv[argc] = %p\n", (void *)argv[argc]);
    return 0;
}
```

<details><summary>Predicción</summary>

```
argc = 4

[0] "./prog" (len=0)
[1] "hello" (len=0)
[2] "world" (len=0)
[3] "42" (len=0)

First arg chars: 'h' 'e' 'l' 'l' 'o'
argv[argc] = (nil)
```

- `argc = 4`: el programa + 3 argumentos
- El recorrido con `char **p` funciona porque `argv[argc]` es siempre `NULL`
- `p - argv` da el índice (diferencia de punteros a `char *`)
- `argv[1][i]` accede carácter por carácter al segundo argumento
- `argv[argc]` = `argv[4]` = `NULL` (garantizado por el estándar C)

Para calcular `len` en [A]: reemplazar `0` por `(int)strlen(*p)` (necesita `<string.h>`). O recorrer `*p` hasta `'\0'` manualmente.

</details>

### Ejercicio 4 — Jagged array

```c
// ¿Qué imprime? ¿Cuántos malloc y free se ejecutan?
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int sizes[] = {3, 1, 4};
    int rows = 3;

    int **mat = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        mat[i] = malloc(sizes[i] * sizeof(int));
        for (int j = 0; j < sizes[i]; j++) {
            mat[i][j] = i * 10 + j;
        }
    }

    for (int i = 0; i < rows; i++) {
        printf("Row %d (%d): ", i, sizes[i]);
        for (int j = 0; j < sizes[i]; j++) {
            printf("%d ", mat[i][j]);
        }
        printf("\n");
    }

    for (int i = 0; i < rows; i++) free(mat[i]);
    free(mat);

    return 0;
}
```

<details><summary>Predicción</summary>

```
Row 0 (3): 0 1 2
Row 1 (1): 10
Row 2 (4): 20 21 22 23
```

- **malloc**: 4 en total (1 para `mat` + 3 para las filas)
- **free**: 4 en total (3 filas + 1 para `mat`)
- `mat[i][j] = i * 10 + j`: fila 0 = {0, 1, 2}, fila 1 = {10}, fila 2 = {20, 21, 22, 23}

La notación `mat[i][j]` funciona pero la mecánica es diferente a un array 2D estático:
- Estático `int m[3][4]`: el compilador calcula `*(m + i * 4 + j)` en un bloque contiguo
- Dinámico `int **mat`: `mat[i]` sigue un puntero, luego `[j]` accede dentro de esa fila

</details>

### Ejercicio 5 — Linked list con **

```c
// ¿Qué imprime?
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

void push(struct Node **head, int val) {
    struct Node *n = malloc(sizeof(struct Node));
    n->value = val;
    n->next = *head;
    *head = n;
}

void print_list(struct Node *head) {
    for (struct Node *n = head; n; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}

int main(void) {
    struct Node *list = NULL;

    push(&list, 10);
    push(&list, 20);
    push(&list, 30);

    print_list(list);

    // Liberar
    while (list) {
        struct Node *tmp = list;
        list = list->next;
        free(tmp);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
30 -> 20 -> 10 -> NULL
```

`push` inserta al **inicio** de la lista. El orden de inserción es 10, 20, 30, pero cada nuevo nodo se convierte en el head:
1. `push(&list, 10)`: list → [10] → NULL
2. `push(&list, 20)`: list → [20] → [10] → NULL
3. `push(&list, 30)`: list → [30] → [20] → [10] → NULL

`push` necesita `**head` porque modifica el puntero `list` del caller. `n->next = *head` enlaza el nodo nuevo al head actual, y `*head = n` actualiza el head al nodo nuevo.

La liberación recorre la lista guardando `tmp` antes de avanzar, porque después de `free(tmp)` no se puede acceder a `tmp->next`.

</details>

### Ejercicio 6 — Eliminación con patrón Torvalds

```c
// ¿Qué imprime? Rastrea pp en cada iteración.
#include <stdio.h>
#include <stdlib.h>

struct Node { int value; struct Node *next; };

void push(struct Node **head, int val) {
    struct Node *n = malloc(sizeof(struct Node));
    n->value = val; n->next = *head; *head = n;
}

void remove_val(struct Node **pp, int val) {
    while (*pp) {
        if ((*pp)->value == val) {
            struct Node *tmp = *pp;
            *pp = (*pp)->next;
            free(tmp);
            return;
        }
        pp = &(*pp)->next;
    }
}

void print_list(struct Node *h) {
    for (; h; h = h->next) printf("%d -> ", h->value);
    printf("NULL\n");
}

int main(void) {
    struct Node *list = NULL;
    push(&list, 10); push(&list, 20); push(&list, 30);

    print_list(list);       // [A]
    remove_val(&list, 30);  // eliminar head
    print_list(list);       // [B]
    remove_val(&list, 10);  // eliminar tail
    print_list(list);       // [C]
    remove_val(&list, 99);  // no existe
    print_list(list);       // [D]

    // Liberar
    while (list) { struct Node *t = list; list = list->next; free(t); }
    return 0;
}
```

<details><summary>Predicción</summary>

```
30 -> 20 -> 10 -> NULL
20 -> 10 -> NULL
20 -> NULL
20 -> NULL
```

- **[A]**: lista original: 30 → 20 → 10 → NULL
- **Eliminar 30** (head): `pp = &list`, `*pp` es el nodo 30. Se enlaza `list` al siguiente (20). Sin caso especial.
- **[B]**: 20 → 10 → NULL
- **Eliminar 10** (tail): `pp` avanza a `&nodo20->next`. `*pp` es el nodo 10. Se enlaza `nodo20->next` a NULL.
- **[C]**: 20 → NULL
- **Eliminar 99**: `pp` recorre toda la lista sin encontrarlo. No modifica nada.
- **[D]**: 20 → NULL (sin cambios)

El patrón funciona uniformemente para head, middle, tail, y no-encontrado sin necesitar casos especiales.

</details>

### Ejercicio 7 — Triple puntero: función que crea matriz

```c
// ¿Qué imprime? ¿Por qué se necesita int ***?
#include <stdio.h>
#include <stdlib.h>

int create_matrix(int ***mat, int rows, int cols) {
    *mat = malloc(rows * sizeof(int *));
    if (!*mat) return -1;
    for (int i = 0; i < rows; i++) {
        (*mat)[i] = calloc(cols, sizeof(int));
        if (!(*mat)[i]) return -1;  // simplificado, no libera parcial
    }
    return 0;
}

int main(void) {
    int **m = NULL;
    if (create_matrix(&m, 2, 3) == 0) {
        m[0][0] = 1; m[0][1] = 2; m[0][2] = 3;
        m[1][0] = 4; m[1][1] = 5; m[1][2] = 6;

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                printf("%d ", m[i][j]);
            }
            printf("\n");
        }

        for (int i = 0; i < 2; i++) free(m[i]);
        free(m);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
1 2 3
4 5 6
```

Se necesita `int ***` porque:
- `m` en main es `int **`
- `create_matrix` necesita **modificar** `m` (asignarle la memoria alocada)
- Para modificar un `int **`, se necesita pasar `int ***` (un nivel más de indirección)

Dentro de `create_matrix`:
- `*mat = malloc(...)`: asigna el array de punteros a filas (`int **`)
- `(*mat)[i] = calloc(...)`: asigna cada fila (`int *`)

`calloc` inicializa a 0, así que las celdas no asignadas explícitamente serían 0.

La alternativa sin triple puntero: retornar `int **` directamente con `return`.

</details>

### Ejercicio 8 — Array de strings dinámico

```c
// ¿Qué imprime? ¿Cuántas alocaciones hay?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *src[] = {"hello", "beautiful", "world"};
    int count = 3;

    // Crear copia dinámica
    char **copy = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) {
        copy[i] = strdup(src[i]);
    }

    // Modificar la copia
    free(copy[1]);
    copy[1] = strdup("wonderful");

    // Imprimir
    for (int i = 0; i < count; i++) {
        printf("copy[%d] = \"%s\"\n", i, copy[i]);
    }

    // Liberar
    for (int i = 0; i < count; i++) free(copy[i]);
    free(copy);
    return 0;
}
```

<details><summary>Predicción</summary>

```
copy[0] = "hello"
copy[1] = "wonderful"
copy[2] = "world"
```

Alocaciones:
1. `malloc(3 * sizeof(char *))` — array de 3 punteros
2. `strdup("hello")` — copia "hello" en el heap
3. `strdup("beautiful")` — copia "beautiful" en el heap
4. `strdup("world")` — copia "world" en el heap
5. `strdup("wonderful")` — copia "wonderful" (después de liberar "beautiful")

Total: 5 `malloc` (incluyendo los de `strdup`), 5 `free` (3 strings + 1 reemplazo + el array).

`strdup` equivale a `malloc(strlen(s) + 1)` + `strcpy`. Es POSIX, no estándar C (hasta C23). Cada string es una copia independiente en el heap — modificar `copy[1]` no afecta a `src[1]`.

</details>

### Ejercicio 9 — Orden de liberación

```c
// ¿Qué ocurre si se invierte el orden de free?
// ¿Compila? ¿Funciona? ¿Es correcto?
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rows = 3, cols = 4;

    int **mat = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        mat[i] = malloc(cols * sizeof(int));
    }

    // Orden INCORRECTO de liberación:
    free(mat);           // [A] libera el array de punteros PRIMERO
    for (int i = 0; i < rows; i++) {
        free(mat[i]);    // [B] intenta acceder a mat[i] después de free(mat)
    }

    return 0;
}
```

<details><summary>Predicción</summary>

El código compila sin errores ni warnings. Pero es **comportamiento indefinido**.

En [A], `free(mat)` libera el bloque que contiene los punteros `mat[0]`, `mat[1]`, `mat[2]`. Después de `free(mat)`, acceder a `mat[i]` en [B] es **use-after-free** — la memoria ya fue devuelta al allocator.

Posibles resultados:
- **Puede funcionar** silenciosamente (la memoria aún no fue reutilizada)
- **Puede crashear** (la memoria fue reescrita por el allocator)
- **Puede liberar direcciones basura** (el allocator sobreescribió los punteros)

El **orden correcto** siempre es:
```c
for (int i = 0; i < rows; i++) free(mat[i]);  // primero las filas
free(mat);                                       // luego el array
```

Regla: liberar desde lo más interno hacia lo más externo — porque cada nivel externo contiene los punteros necesarios para acceder al siguiente nivel.

</details>

### Ejercicio 10 — Función que agranda un array

```c
// ¿Qué imprime? ¿Por qué se necesita int **?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int grow_array(int **arr, int *cap, int new_cap) {
    int *tmp = realloc(*arr, new_cap * sizeof(int));
    if (!tmp) return -1;

    // Inicializar nuevos elementos a 0:
    if (new_cap > *cap) {
        memset(tmp + *cap, 0, (new_cap - *cap) * sizeof(int));
    }

    *arr = tmp;
    *cap = new_cap;
    return 0;
}

int main(void) {
    int *data = malloc(3 * sizeof(int));
    int cap = 3;

    data[0] = 10; data[1] = 20; data[2] = 30;

    printf("Before: ");
    for (int i = 0; i < cap; i++) printf("%d ", data[i]);
    printf("(cap=%d)\n", cap);

    grow_array(&data, &cap, 6);

    data[3] = 40; data[4] = 50; data[5] = 60;

    printf("After:  ");
    for (int i = 0; i < cap; i++) printf("%d ", data[i]);
    printf("(cap=%d)\n", cap);

    free(data);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Before: 10 20 30 (cap=3)
After:  10 20 30 40 50 60 (cap=6)
```

`grow_array` necesita `int **arr` porque `realloc` puede mover el bloque de memoria a una dirección diferente. Si retorna una dirección nueva, `*arr` debe actualizarse — de lo contrario, el caller seguiría usando la dirección vieja (dangling pointer / use-after-free).

Paso a paso:
1. `realloc(*arr, 6 * sizeof(int))`: intenta expandir el bloque de 12 a 24 bytes. Si no puede expandir in-situ, aloca un nuevo bloque, copia los datos, y libera el viejo.
2. `memset(tmp + 3, 0, 3 * sizeof(int))`: inicializa los 3 nuevos ints a 0.
3. `*arr = tmp`: actualiza el puntero del caller a la nueva dirección.
4. `*cap = 6`: actualiza la capacidad del caller.

Si `grow_array` recibiera `int *arr` (sin `**`), el caller no vería la nueva dirección después de `realloc` → accedería a memoria liberada.

</details>
