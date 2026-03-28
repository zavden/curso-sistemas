# T01 — Ownership manual

> Sin erratas detectadas en el material base.

---

## 1. El problema del ownership en C

C no tiene un mecanismo de ownership integrado en el lenguaje. El programador debe decidir y documentar **quién es responsable de liberar** cada bloque de memoria alocada:

```c
char *result = get_data();
process(result);
// ¿Quién hace free de result?
// ¿get_data espera que el caller haga free?
// ¿O result apunta a memoria interna que no debe liberarse?
// Sin leer la documentación o el código: imposible saber.
```

Este problema genera cuatro tipos de bugs:

| Bug | Causa | Consecuencia |
|-----|-------|-------------|
| Memory leak | Nadie libera | Consumo creciente |
| Double free | Dos partes liberan | Corrupción del heap |
| Use-after-free | Se usa después de que otro liberó | Comportamiento indefinido |
| Free de memoria no-malloc | Se libera algo que no fue alocado | Crash |

En Rust, el compilador verifica el ownership en tiempo de compilación. En C, es **responsabilidad exclusiva del programador**, apoyado por convenciones y herramientas (Valgrind, ASan).

---

## 2. Caller owns: la función retorna, el caller libera

Es el patrón más común. La función aloca memoria y retorna un puntero. El caller recibe ownership y **debe** hacer `free`:

```c
// @return: newly allocated string, caller must free()
char *string_duplicate(const char *s) {
    if (!s) return NULL;
    char *copy = malloc(strlen(s) + 1);
    if (!copy) return NULL;
    strcpy(copy, s);
    return copy;
}

int main(void) {
    char *s = string_duplicate("hello");
    if (!s) return 1;
    printf("%s\n", s);
    free(s);  // Responsabilidad del caller
    return 0;
}
```

### Técnica: medir con snprintf(NULL, 0, ...)

Para construir strings formateados sin riesgo de buffer overflow:

```c
// @return: newly allocated formatted string, caller must free()
char *make_greeting(const char *name, int age) {
    // Paso 1: medir cuántos bytes se necesitan
    int len = snprintf(NULL, 0, "Hello, %s! You are %d years old.", name, age);
    if (len < 0) return NULL;

    // Paso 2: alocar exactamente lo necesario
    char *buf = malloc((size_t)len + 1);
    if (!buf) return NULL;

    // Paso 3: escribir
    snprintf(buf, (size_t)len + 1, "Hello, %s! You are %d years old.", name, age);
    return buf;
}
```

`snprintf(NULL, 0, fmt, ...)` retorna el número de caracteres que se **escribirían** (sin contar el `\0`), sin escribir nada. Esto permite alocar el tamaño exacto.

### Funciones estándar que siguen este patrón

```c
char *strdup(const char *s);            // caller debe free
void *malloc(size_t size);              // caller debe free
FILE *fopen(const char *, const char *); // caller debe fclose
```

---

## 3. Callee owns: datos internos que no debes liberar

La función retorna un puntero a **datos internos** (estáticos o globales). El caller **no debe** liberar:

```c
// @return: pointer to internal static buffer, do NOT free
const char *get_hostname(void) {
    static char buf[256];
    gethostname(buf, sizeof(buf));
    return buf;  // Puntero a buffer estático — NO fue alocado con malloc
}

const char *host = get_hostname();
printf("Host: %s\n", host);
// free(host);    // INCORRECTO — buf es static, no se puede hacer free
```

### Peligro: la siguiente llamada sobreescribe

```c
const char *h1 = get_hostname();
// h1 apunta al buffer estático, que contiene "server1"
const char *h2 = get_hostname();
// h2 apunta al MISMO buffer, ahora contiene "server2"
// h1 también muestra "server2" — apuntan al mismo lugar
```

Si necesitas retener el valor, **copia** con `strdup`.

### Funciones estándar que siguen este patrón

```c
const char *strerror(int errnum);       // buffer estático interno
struct tm *localtime(const time_t *t);  // struct estático interno
char *getenv(const char *name);         // puntero a datos del entorno
// NINGUNA de estas debe liberarse con free.
```

La pista suele ser el tipo de retorno `const char *` — indica "mira pero no toques ni liberes".

---

## 4. Transferencia de ownership (move)

La función **toma posesión** del puntero. Después de la llamada, el caller **no debe** usar ni liberar la memoria:

```c
// @param data: this function TAKES OWNERSHIP. Do not use after call.
void stack_push(struct Stack *s, void *data) {
    struct Node *n = malloc(sizeof(struct Node));
    if (!n) {
        free(data);  // Aceptamos ownership → debemos liberar si fallamos
        return;
    }
    n->data = data;
    n->next = s->top;
    s->top = n;
    s->count++;
}

int main(void) {
    struct Stack s;
    stack_init(&s);

    char *name = strdup("Alice");
    stack_push(&s, name);
    // name fue MOVIDO al stack
    // name sigue siendo un puntero válido como variable,
    // pero la memoria a la que apunta ya no nos pertenece.
    // printf("%s\n", name);  // PELIGROSO — el stack podría haber liberado name

    stack_destroy(&s);  // Libera todos los datos internos
    return 0;
}
```

### Detalle crucial: manejo de errores en la transferencia

Si `stack_push` acepta ownership pero falla internamente (ej. `malloc` para el nodo falla), **debe liberar `data`** porque ya es responsable de él. Si no lo hiciera, sería un leak — el caller ya no tiene ownership y no hará `free`.

### La operación inversa: stack_pop retorna ownership

```c
// @return: data pointer. TRANSFERS OWNERSHIP to caller.
//          Caller must free() the returned pointer.
void *stack_pop(struct Stack *s) {
    if (!s->top) return NULL;
    struct Node *n = s->top;
    void *data = n->data;
    s->top = n->next;
    s->count--;
    free(n);    // Libera el NODO, NO el dato
    return data; // Ownership transferido al caller
}

// Uso:
char *item = stack_pop(&s);
if (item) {
    printf("%s\n", item);
    free(item);  // Ahora nosotros somos dueños → debemos liberar
}
```

---

## 5. Préstamo (borrow)

La función **usa** el puntero temporalmente pero no toma ownership. El caller sigue siendo responsable:

```c
// @param arr: borrowed, not freed by this function
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
    // arr no se modifica ni se libera — es un préstamo de solo lectura
}

int main(void) {
    int *data = calloc(10, sizeof(int));
    // ... llenar data ...

    int s = sum(data, 10);  // sum "presta" data temporalmente
    // data sigue siendo válido y es responsabilidad de main

    free(data);
    return 0;
}
```

### Señal de préstamo: `const` en el parámetro

`const int *arr` comunica que la función **no modificará** los datos. Si además no toma ownership, es un préstamo puro. Sin embargo, `const` en un parámetro no garantiza que no se retenga el puntero internamente — es convención, no garantía del lenguaje.

### Préstamo mutable

```c
// @param buf: borrowed (mutable), caller retains ownership
void to_uppercase(char *buf) {
    for (; *buf; buf++) {
        if (*buf >= 'a' && *buf <= 'z') {
            *buf -= 32;
        }
    }
    // buf no se libera — es un préstamo mutable
}
```

---

## 6. Patrón constructor / destructor (create/destroy)

El patrón más robusto para structs con recursos internos:

```c
// --- person.h ---
struct Person {
    char *name;   // alocado dinámicamente
    char *email;  // alocado dinámicamente
    int age;
};

struct Person *person_create(const char *name, const char *email, int age);
void person_destroy(struct Person *p);

// --- person.c ---
struct Person *person_create(const char *name, const char *email, int age) {
    struct Person *p = malloc(sizeof(struct Person));
    if (!p) return NULL;

    // Inicializar a NULL ANTES de alocar — crucial para cleanup seguro
    p->name = NULL;
    p->email = NULL;
    p->age = age;

    p->name = malloc(strlen(name) + 1);
    p->email = malloc(strlen(email) + 1);

    if (!p->name || !p->email) {
        // Si cualquier malloc falla, limpiar todo
        free(p->name);   // free(NULL) es no-op si no se alocó
        free(p->email);
        free(p);
        return NULL;
    }

    strcpy(p->name, name);
    strcpy(p->email, email);
    return p;
}

void person_destroy(struct Person *p) {
    if (!p) return;       // Idempotente: person_destroy(NULL) es seguro
    free(p->name);
    free(p->email);
    free(p);              // Miembros primero, struct al final
}
```

### Reglas del patrón

1. **Inicializar todo a NULL** antes de alocar — permite `free(NULL)` seguro en cleanup
2. **Si falla a mitad**: limpiar todo lo ya alocado y retornar NULL
3. **Destructor idempotente**: `person_destroy(NULL)` debe ser seguro
4. **Liberar de adentro hacia afuera**: miembros primero, contenedor después
5. **El caller nunca libera miembros individuales** — el destructor lo hace todo

---

## 7. Patrón init / deinit (struct en stack)

Variante donde el struct vive en el stack y solo sus datos internos están en el heap:

```c
struct Buffer {
    char *data;
    size_t len;
    size_t cap;
};

int buffer_init(struct Buffer *buf, size_t cap) {
    buf->data = malloc(cap);
    if (!buf->data) return -1;
    buf->len = 0;
    buf->cap = cap;
    return 0;
}

void buffer_deinit(struct Buffer *buf) {
    free(buf->data);
    buf->data = NULL;   // Prevenir use-after-deinit
    buf->len = 0;
    buf->cap = 0;
}

int main(void) {
    struct Buffer buf;              // Stack — no necesita malloc
    if (buffer_init(&buf, 1024) != 0) return 1;
    // ... usar buf ...
    buffer_deinit(&buf);            // Libera solo buf.data
    // buf sigue en el stack — no necesita free del struct
    return 0;
}
```

### create/destroy vs init/deinit

| Aspecto | create/destroy | init/deinit |
|---------|---------------|-------------|
| Struct vive en | Heap (`malloc`) | Stack (variable local) |
| Constructor | Retorna `StructType *` | Recibe `StructType *`, retorna status |
| Destructor | `free` del struct incluido | Solo libera datos internos |
| Ejemplo de API | `person_create() / person_destroy()` | `buffer_init() / buffer_deinit()` |
| Cuándo usar | Cuando el objeto necesita sobrevivir al scope | Cuando el objeto es local a la función |

---

## 8. Ownership de arrays y estructuras recursivas

### Arrays de punteros: liberar cada elemento

```c
struct StringList {
    char **items;    // Cada items[i] fue alocado con malloc
    size_t len;
    size_t cap;
};

void string_list_destroy(struct StringList *list) {
    for (size_t i = 0; i < list->len; i++) {
        free(list->items[i]);   // Liberar cada string individual
    }
    free(list->items);          // Liberar el array de punteros
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}
```

### Arrays de punteros prestados: NO liberar elementos

```c
struct StringView {
    const char **items;   // const = no somos dueños de los strings
    size_t len;
};

void string_view_destroy(struct StringView *sv) {
    free(sv->items);   // Solo liberar el array de punteros
    // NO free de cada item — son prestados (pertenecen a otro)
}
```

La diferencia la marca `const char **` vs `char **`. Si los punteros son `const`, es señal de que son prestados.

### Listas enlazadas: guardar next ANTES de free

```c
struct Node {
    int value;
    struct Node *next;
};

void free_list(struct Node *head) {
    while (head) {
        struct Node *next = head->next;  // GUARDAR antes de free
        free(head);
        head = next;
    }
}
```

### Árboles: recursión post-order

```c
void tree_free(struct TreeNode *node) {
    if (!node) return;
    tree_free(node->left);    // Hijos primero
    tree_free(node->right);
    free(node);               // Padre al final
}
```

---

## 9. Errores de ownership y cómo detectarlos

### Los cinco errores clásicos

```c
// 1. Double free:
char *s = strdup("hello");
free(s);
free(s);     // UB — corrupción del heap
// Valgrind: "Invalid free"
// Prevención: s = NULL después de free

// 2. Use after free:
char *s = strdup("hello");
free(s);
printf("%s\n", s);  // UB — dangling pointer
// Valgrind: "Invalid read of size 1"
// GCC: -Wuse-after-free

// 3. Free de memoria no-malloc:
const char *s = "hello";     // String literal en .rodata
free((char *)s);              // UB — no fue alocado con malloc
// Prevención: nunca hacer free de un string literal

// 4. Leak por reasignación:
char *s = strdup("hello");
s = strdup("world");  // LEAK — "hello" se perdió
// Valgrind: "definitely lost"
// Prevención: free(s) antes de reasignar

// 5. Ownership ambiguo (shared ownership):
char *shared = strdup("data");
struct A *a = create_a(shared);  // ¿a toma ownership?
struct B *b = create_b(shared);  // ¿b también?
// Si a_destroy y b_destroy ambos hacen free(shared) → double free
// Si ninguno hace free → leak
// Prevención: documentar quién es dueño, o copiar para cada uno
```

### Herramientas de detección

```bash
# Compilar siempre con -g para stack traces legibles:
gcc -g -O0 prog.c -o prog

# Valgrind — detecta todo (double free, UAF, leaks):
valgrind --leak-check=full --show-leak-kinds=all ./prog

# ASan — más rápido, necesita recompilación:
gcc -fsanitize=address -g prog.c -o prog
./prog

# GCC warnings útiles:
gcc -Wall -Wextra -Wuse-after-free prog.c
```

---

## 10. Comparación con Rust y C++

### C: ownership manual

```c
char *s = strdup("hello");
process(s);          // ¿process toma ownership?
printf("%s\n", s);   // ¿s sigue siendo válido?
free(s);             // ¿es seguro? ¿ya fue liberado?
// Todas estas preguntas dependen de la documentación.
```

### Rust: ownership en el tipo

```rust
// Ownership es parte del sistema de tipos:
let s = String::from("hello");
process(s);              // s se MOVIÓ a process
// println!("{}", s);    // ERROR DE COMPILACIÓN: use of moved value

// Préstamo (borrow) — acceso temporal sin transferir ownership:
let s = String::from("hello");
process(&s);             // Préstamo inmutable
println!("{}", s);       // OK — s nunca dejó de ser nuestro

// Préstamo mutable:
let mut s = String::from("hello");
modify(&mut s);          // Préstamo mutable (exclusivo)
println!("{}", s);       // OK — el préstamo terminó

// RAII: Drop automático al salir del scope
{
    let data = vec![1, 2, 3];
    // ... usar data ...
}  // drop(data) se llama automáticamente aquí → free interno
```

### C++: smart pointers (intermedio)

```cpp
// unique_ptr: ownership exclusivo (como Rust's Box<T>)
auto p = std::make_unique<int>(42);
process(std::move(p));   // Ownership transferido
// *p;  // UB — p ahora es nullptr (pero compila sin error)

// shared_ptr: ownership compartido (reference counting)
auto p = std::make_shared<int>(42);
auto q = p;  // Ambos son dueños — se libera cuando el último muere
```

### Resumen comparativo

| Aspecto | C | C++ | Rust |
|---------|---|-----|------|
| Ownership | Manual (convención) | Smart pointers (opt-in) | Sistema de tipos (obligatorio) |
| Move | Copia del puntero (manual) | `std::move` (runtime) | Move semántico (compilación) |
| Borrow | Pasar puntero (sin verificación) | Referencia (sin verificación) | `&T`/`&mut T` (verificado) |
| Error de ownership | UB en runtime | UB (unique_ptr) o panic (shared_ptr) | Error de compilación |
| Free | Manual (`free()`) | Automático (RAII) | Automático (Drop) |

---

## Ejercicios

### Ejercicio 1 — Caller owns: strdup manual

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *my_strdup(const char *s) {
    char *copy = malloc(strlen(s) + 1);
    if (!copy) return NULL;
    strcpy(copy, s);
    return copy;
}

int main(void) {
    char *a = my_strdup("hello");
    char *b = my_strdup("world");
    printf("%s %s\n", a, b);

    free(a);
    // ¿b se libera?
    return 0;
}
```

Compila con `gcc -g -O0` y ejecuta con `valgrind --leak-check=full`.

**Predicción**: ¿Valgrind reportará leaks? ¿Cuántos bytes y en qué categoría?

<details><summary>Respuesta</summary>

Sí, 1 leak. `b` nunca se libera → 6 bytes ("world" + '\0') "definitely lost" en 1 bloque. `a` se libera correctamente. Corrección: agregar `free(b);` antes del `return 0`.

</details>

### Ejercicio 2 — Create/destroy incompleto

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Config {
    char *filename;
    char *mode;
    int  *values;
    int   num_values;
};

struct Config *config_create(const char *filename, const char *mode, int n) {
    struct Config *c = malloc(sizeof(*c));
    if (!c) return NULL;
    c->filename = malloc(strlen(filename) + 1);
    c->mode = malloc(strlen(mode) + 1);
    c->values = calloc((size_t)n, sizeof(int));
    c->num_values = n;
    if (!c->filename || !c->mode || !c->values) {
        free(c->filename); free(c->mode); free(c->values);
        free(c);
        return NULL;
    }
    strcpy(c->filename, filename);
    strcpy(c->mode, mode);
    return c;
}

void config_destroy(struct Config *c) {
    if (!c) return;
    free(c->filename);
    free(c);
}

int main(void) {
    struct Config *cfg = config_create("app.conf", "rw", 5);
    if (!cfg) return 1;
    printf("Config: %s (%s), %d values\n", cfg->filename, cfg->mode, cfg->num_values);
    config_destroy(cfg);
    return 0;
}
```

**Predicción**: ¿Cuántos bloques reportará Valgrind como perdidos? ¿Cuáles son?

<details><summary>Respuesta</summary>

2 bloques "definitely lost": `c->mode` y `c->values`. El destructor solo libera `c->filename` y `c` (el struct), pero olvida `free(c->mode)` y `free(c->values)`. Tras `free(c)`, los punteros a `mode` y `values` desaparecen → "definitely lost". Corrección:

```c
void config_destroy(struct Config *c) {
    if (!c) return;
    free(c->filename);
    free(c->mode);
    free(c->values);
    free(c);
}
```

</details>

### Ejercicio 3 — Transferencia de ownership

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Container {
    char *data;
};

// Takes ownership of data
void container_set(struct Container *c, char *data) {
    c->data = data;
}

void container_destroy(struct Container *c) {
    free(c->data);
    c->data = NULL;
}

int main(void) {
    struct Container c = {0};

    char *msg = strdup("hello");
    printf("Before transfer: msg=%p\n", (void *)msg);

    container_set(&c, msg);
    printf("After transfer: c.data=%p\n", (void *)c.data);

    // ¿msg y c.data apuntan al mismo lugar?
    printf("Same address: %s\n", (msg == c.data) ? "YES" : "NO");

    container_destroy(&c);
    // ¿Qué pasa si ahora hacemos free(msg)?
    return 0;
}
```

**Predicción**: ¿`msg` y `c.data` apuntan a la misma dirección? Si se agregara `free(msg)` después de `container_destroy`, ¿qué ocurriría?

<details><summary>Respuesta</summary>

Sí, apuntan a la misma dirección — `container_set` no copia, solo asigna el puntero. Si se agrega `free(msg)` después de `container_destroy(&c)`, sería **double free**: `container_destroy` ya liberó la memoria a la que apunta `c.data` (que es la misma que `msg`). Valgrind reportaría "Invalid free". La regla: tras transferir ownership, el caller no debe usar ni liberar `msg`.

</details>

### Ejercicio 4 — Leak por reasignación

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *name = strdup("Alice");
    printf("1: %s (%p)\n", name, (void *)name);

    name = strdup("Bob");
    printf("2: %s (%p)\n", name, (void *)name);

    name = strdup("Charlie");
    printf("3: %s (%p)\n", name, (void *)name);

    free(name);
    return 0;
}
```

**Predicción**: ¿Cuántos bloques se pierden? ¿Las tres direcciones impresas serán iguales o diferentes?

<details><summary>Respuesta</summary>

2 bloques perdidos: "Alice" (6 bytes) y "Bob" (4 bytes). Cada `strdup` retorna una dirección diferente (son alocaciones independientes). Solo el último (`name` apuntando a "Charlie") se libera. Las dos reasignaciones previas perdieron las referencias sin `free`. Corrección: `free(name)` antes de cada `name = strdup(...)`.

</details>

### Ejercicio 5 — Borrow vs ownership

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ¿Esta función toma ownership o presta?
void print_upper(const char *s) {
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        putchar(c);
    }
    putchar('\n');
}

int main(void) {
    char *msg = strdup("hello world");
    print_upper(msg);
    printf("Original: %s\n", msg);
    free(msg);
    return 0;
}
```

**Predicción**: ¿`msg` sigue siendo válido después de `print_upper`? ¿El original se modifica? ¿Hay leaks?

<details><summary>Respuesta</summary>

`msg` sigue siendo válido — `print_upper` es un **préstamo** (borrow). Señales: parámetro `const char *` (no modifica), no llama `free`, no almacena el puntero. El original no se modifica (la función usa una copia local `c`). No hay leaks: el `free(msg)` al final libera correctamente. Valgrind reportará 0 leaks.

</details>

### Ejercicio 6 — Error path con goto cleanup

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Report {
    char *title;
    char *body;
    int  *data;
};

struct Report *report_create(const char *title, const char *body, int n) {
    struct Report *r = malloc(sizeof(*r));
    if (!r) return NULL;

    r->title = strdup(title);
    if (!r->title) { free(r); return NULL; }

    r->body = strdup(body);
    if (!r->body) { free(r); return NULL; }   // ¿Leak?

    r->data = calloc((size_t)n, sizeof(int));
    if (!r->data) { free(r); return NULL; }   // ¿Leak?

    return r;
}

void report_destroy(struct Report *r) {
    if (!r) return;
    free(r->title);
    free(r->body);
    free(r->data);
    free(r);
}
```

**Predicción**: Si `strdup(body)` retorna NULL, ¿cuántos bloques se pierden? Reescribe `report_create` con `goto cleanup`.

<details><summary>Respuesta</summary>

Si `strdup(body)` falla: `r->title` ya fue alocado pero no se libera → 1 bloque perdido. Si `calloc` falla: `r->title` y `r->body` se pierden → 2 bloques. Solución con goto:

```c
struct Report *report_create(const char *title, const char *body, int n) {
    struct Report *r = malloc(sizeof(*r));
    if (!r) return NULL;

    r->title = NULL;
    r->body = NULL;
    r->data = NULL;

    r->title = strdup(title);
    if (!r->title) goto fail;

    r->body = strdup(body);
    if (!r->body) goto fail;

    r->data = calloc((size_t)n, sizeof(int));
    if (!r->data) goto fail;

    return r;

fail:
    report_destroy(r);
    return NULL;
}
```

Reutilizar `report_destroy` en el cleanup es ideal — ya sabe liberar todo, y `free(NULL)` es seguro para los miembros no alocados.

</details>

### Ejercicio 7 — Stack con transferencia de ownership

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node { void *data; struct Node *next; };
struct Stack { struct Node *top; int count; };

void stack_init(struct Stack *s) { s->top = NULL; s->count = 0; }

void stack_push(struct Stack *s, void *data) {
    struct Node *n = malloc(sizeof(*n));
    if (!n) { free(data); return; }
    n->data = data;
    n->next = s->top;
    s->top = n;
    s->count++;
}

void *stack_pop(struct Stack *s) {
    if (!s->top) return NULL;
    struct Node *n = s->top;
    void *data = n->data;
    s->top = n->next;
    s->count--;
    free(n);
    return data;
}

void stack_destroy(struct Stack *s) {
    while (s->top) {
        struct Node *n = s->top;
        s->top = n->next;
        free(n->data);
        free(n);
    }
    s->count = 0;
}

int main(void) {
    struct Stack s;
    stack_init(&s);

    stack_push(&s, strdup("A"));
    stack_push(&s, strdup("B"));
    stack_push(&s, strdup("C"));

    char *top = stack_pop(&s);
    printf("Popped: %s\n", top);
    free(top);

    stack_destroy(&s);
    return 0;
}
```

**Predicción**: ¿Cuántas alocaciones hay en total (strings + nodos)? ¿Cuántas liberaciones? ¿Hay leaks?

<details><summary>Respuesta</summary>

Alocaciones: 3 strings (`strdup`) + 3 nodos (`stack_push`) = 6. Liberaciones: 1 nodo (`stack_pop`) + 1 string (`free(top)` en main) + 2 nodos + 2 strings (`stack_destroy`) = 6. Total: 6 allocs, 6 frees → 0 leaks. El ownership fluye: `strdup` crea → `stack_push` toma ownership → `stack_pop` retorna ownership a main o `stack_destroy` libera los restantes.

</details>

### Ejercicio 8 — Ownership ambiguo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Logger {
    char *prefix;
};

void logger_init(struct Logger *log, char *prefix) {
    log->prefix = prefix;  // ¿Copia o referencia?
}

void logger_log(struct Logger *log, const char *msg) {
    printf("[%s] %s\n", log->prefix, msg);
}

void logger_deinit(struct Logger *log) {
    free(log->prefix);     // ¿Es correcto?
    log->prefix = NULL;
}

int main(void) {
    char *pfx = strdup("APP");
    struct Logger log;
    logger_init(&log, pfx);

    logger_log(&log, "started");
    logger_deinit(&log);

    // ¿Es seguro usar pfx aquí?
    printf("pfx = %s\n", pfx);

    return 0;
}
```

**Predicción**: ¿Qué imprime la línea `printf("pfx = %s\n", pfx)`? ¿Es use-after-free?

<details><summary>Respuesta</summary>

**Use-after-free**. `logger_init` almacena el puntero sin copiar. `logger_deinit` hace `free(log->prefix)`, que libera la misma memoria que `pfx` apunta. Luego `printf("pfx = %s\n", pfx)` lee memoria liberada → UB. Valgrind reporta "Invalid read". Hay dos soluciones: (1) `logger_init` copia el string con `strdup`, o (2) `logger_deinit` no hace free y se documenta que el caller es responsable del lifetime de `prefix`.

</details>

### Ejercicio 9 — Documentar ownership

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *concat(const char *a, const char *b);
void list_add(struct List *list, void *item);
const char *map_get(struct Map *map, const char *key);
void map_set(struct Map *map, const char *key, char *value);
```

Para cada función, responde: ¿quién es dueño de cada parámetro? ¿Y del valor de retorno?

**Predicción**: Basándote en los nombres y tipos, ¿qué ownership asignarías a cada una?

<details><summary>Respuesta</summary>

- `concat(const char *a, const char *b)` → `a` y `b` son **borrowed** (`const` = no se modifican ni liberan). El retorno `char *` (no const) → **caller owns**, debe hacer `free`.
- `list_add(struct List *list, void *item)` → `list` es borrowed (mutable). `item` probablemente **transferido** al list (convención: "add" suele tomar ownership). El list liberaría `item` en su destroy.
- `map_get(struct Map *map, const char *key)` → `map` y `key` son borrowed. Retorno `const char *` → **callee owns** (puntero a dato interno del map). El caller NO debe hacer free.
- `map_set(struct Map *map, const char *key, char *value)` → `map` borrowed, `key` es `const` (borrowed, el map debería copiar internamente). `value` es `char *` (no const) → probablemente **transferido** al map. Ambiguo sin documentación — podría copiar o tomar ownership.

</details>

### Ejercicio 10 — Destructor de lista con datos owned

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Entry { char *key; char *value; };

struct Dict {
    struct Entry *entries;
    int len;
    int cap;
};

void dict_init(struct Dict *d, int cap) {
    d->entries = calloc((size_t)cap, sizeof(struct Entry));
    d->len = 0;
    d->cap = cap;
}

int dict_set(struct Dict *d, const char *key, const char *value) {
    if (d->len >= d->cap) return -1;
    d->entries[d->len].key = strdup(key);
    d->entries[d->len].value = strdup(value);
    if (!d->entries[d->len].key || !d->entries[d->len].value) {
        free(d->entries[d->len].key);
        free(d->entries[d->len].value);
        return -1;
    }
    d->len++;
    return 0;
}

void dict_deinit(struct Dict *d) {
    free(d->entries);
    d->entries = NULL;
    d->len = d->cap = 0;
}

int main(void) {
    struct Dict d;
    dict_init(&d, 10);
    dict_set(&d, "name", "Alice");
    dict_set(&d, "city", "Madrid");
    dict_set(&d, "lang", "C");
    dict_deinit(&d);
    return 0;
}
```

**Predicción**: ¿Cuántos bloques reportará Valgrind como perdidos? ¿Qué falta en `dict_deinit`?

<details><summary>Respuesta</summary>

6 bloques perdidos: 3 keys + 3 values, cada uno alocado con `strdup` en `dict_set`. `dict_deinit` solo libera el array `entries`, pero no los strings individuales de cada entry. Corrección:

```c
void dict_deinit(struct Dict *d) {
    for (int i = 0; i < d->len; i++) {
        free(d->entries[i].key);
        free(d->entries[i].value);
    }
    free(d->entries);
    d->entries = NULL;
    d->len = d->cap = 0;
}
```

</details>
