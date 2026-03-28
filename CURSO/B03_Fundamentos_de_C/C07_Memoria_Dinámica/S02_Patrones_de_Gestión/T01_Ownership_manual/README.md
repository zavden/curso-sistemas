# T01 — Ownership manual

## El problema del ownership en C

C no tiene un mecanismo de ownership. El programador debe
decidir y documentar **quién es responsable de liberar** cada
bloque de memoria alocada:

```c
// ¿Quién hace free?
char *result = get_data();
process(result);
// ¿get_data espera que el caller haga free?
// ¿O result apunta a memoria interna que no debe liberarse?
// Sin leer la documentación/código: imposible saber.

// En Rust, el sistema de tipos resuelve esto:
// - Ownership es parte del tipo
// - El compilador verifica en compilación
// En C, es responsabilidad del programador.
```

## Convenciones de ownership

### 1. El caller es dueño (caller owns)

La función aloca, el caller libera. Es el patrón más común:

```c
// La función retorna memoria alocada → el caller debe hacer free:

// @return: newly allocated string, caller must free()
char *string_duplicate(const char *s) {
    char *copy = malloc(strlen(s) + 1);
    if (!copy) return NULL;
    strcpy(copy, s);
    return copy;
}

int main(void) {
    char *s = string_duplicate("hello");
    // ... usar s ...
    free(s);    // responsabilidad del caller
    return 0;
}
```

```c
// Funciones estándar que siguen este patrón:
char *strdup(const char *s);        // caller debe free
void *malloc(size_t size);           // caller debe free
FILE *fopen(const char *, const char *);  // caller debe fclose
```

### 2. La función es dueña (callee owns)

La función retorna un puntero a datos internos.
El caller **no debe** liberar:

```c
// @return: pointer to internal buffer, do NOT free
const char *get_hostname(void) {
    static char buf[256];
    gethostname(buf, sizeof(buf));
    return buf;    // puntero a buffer estático
}

// El caller NO debe hacer free:
const char *host = get_hostname();
printf("Host: %s\n", host);
// free(host);    // INCORRECTO — buf es static, no fue alocado con malloc

// Problema: la siguiente llamada a get_hostname sobrescribe buf.
// El caller debe copiar si necesita retener el valor.
```

```c
// Funciones estándar que siguen este patrón:
const char *strerror(int errnum);      // buffer estático interno
struct tm *localtime(const time_t *t); // struct estático interno
char *getenv(const char *name);        // puntero a datos del entorno
// NINGUNA de estas debe hacerse free.
```

### 3. Transferencia de ownership (move)

La función **toma posesión** del puntero. El caller no debe
usar ni liberar la memoria después:

```c
// @param data: this function takes ownership, do not use after call
void queue_push(struct Queue *q, void *data) {
    // q ahora es dueño de data
    struct Node *n = malloc(sizeof(struct Node));
    n->data = data;
    n->next = q->head;
    q->head = n;
}

int main(void) {
    struct Queue q = {0};
    int *val = malloc(sizeof(int));
    *val = 42;

    queue_push(&q, val);
    // val fue "movido" al queue
    // *val = 100;    // peligroso — el queue podría haber liberado val

    queue_destroy(&q);    // libera todos los datos internos
    return 0;
}
```

### 4. Préstamo (borrow)

La función **usa** el puntero temporalmente pero no toma ownership.
El caller sigue siendo responsable:

```c
// @param arr: borrowed, not freed by this function
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
    // arr no se modifica ni se libera
}

int main(void) {
    int *data = calloc(10, sizeof(int));
    // ... llenar data ...

    int s = sum(data, 10);    // sum "presta" data
    // data sigue siendo válido y es responsabilidad de main

    free(data);
    return 0;
}
```

## Patrón: constructor / destructor

```c
// El patrón más robusto: emparejar create/destroy:

// --- persona.h ---
struct Person {
    char *name;
    char *email;
    int age;
};

// Constructor — aloca e inicializa:
struct Person *person_create(const char *name, const char *email, int age);

// Destructor — libera todo:
void person_destroy(struct Person *p);

// --- persona.c ---
struct Person *person_create(const char *name, const char *email, int age) {
    struct Person *p = malloc(sizeof(struct Person));
    if (!p) return NULL;

    p->name = strdup(name);
    p->email = strdup(email);
    p->age = age;

    if (!p->name || !p->email) {
        person_destroy(p);
        return NULL;
    }
    return p;
}

void person_destroy(struct Person *p) {
    if (!p) return;
    free(p->name);
    free(p->email);
    free(p);
}
```

```c
// Variante: init/deinit para structs en el stack:

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
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

int main(void) {
    struct Buffer buf;
    buffer_init(&buf, 1024);
    // ... usar buf ...
    buffer_deinit(&buf);
    // buf está en el stack — no necesita free del struct
    return 0;
}
```

## Documentar ownership

```c
// En ausencia de mecanismos del lenguaje, DOCUMENTAR es esencial:

// Opción 1: Comentarios en la declaración
/**
 * Read entire file into memory.
 * @param path: borrowed, not modified
 * @param out_size: output parameter, receives file size
 * @return: newly allocated buffer, caller must free()
 *          NULL on error (errno set)
 */
char *read_file(const char *path, size_t *out_size);

// Opción 2: Convención de nombres
char *str_dup(const char *s);          // "dup" implica alocación → caller frees
const char *str_find(const char *s);   // "find" retorna puntero interno → no free
void str_free(char *s);                // función de liberación explícita

// Opción 3: const en el retorno
const char *get_name(int id);   // const → probablemente no debes hacer free
char *create_name(int id);      // no const → probablemente debes hacer free
// No es una regla del lenguaje, pero es una convención útil.
```

## Ownership de arrays de punteros

```c
// Cuando un array contiene punteros a memoria alocada,
// el destructor debe liberar CADA elemento:

struct StringList {
    char **items;
    size_t len;
    size_t cap;
};

void string_list_destroy(struct StringList *list) {
    // Liberar cada string individual:
    for (size_t i = 0; i < list->len; i++) {
        free(list->items[i]);
    }
    // Liberar el array de punteros:
    free(list->items);

    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

// Si los strings son prestados (borrowed), NO liberarlos:
struct StringView {
    const char **items;    // const = no somos dueños
    size_t len;
};

void string_view_destroy(struct StringView *sv) {
    free(sv->items);    // solo liberar el array de punteros
    // NO free de cada item — son prestados
}
```

## Errores de ownership

```c
// 1. Double free — liberar dos veces:
char *s = strdup("hello");
free(s);
free(s);    // UB — corrupción del heap

// 2. Use after free:
char *s = strdup("hello");
free(s);
printf("%s\n", s);    // UB — dangling pointer

// 3. Liberar memoria que no es tuya:
const char *s = "hello";    // string literal en .rodata
free((char *)s);             // UB — no fue alocado con malloc

// 4. Olvidar liberar (leak):
char *s = strdup("hello");
s = strdup("world");    // leak — "hello" se perdió

// 5. Ownership ambiguo:
char *shared = strdup("data");
struct A *a = create_a(shared);
struct B *b = create_b(shared);
// ¿Quién hace free de shared? ¿a_destroy? ¿b_destroy? ¿main?
// Si dos destructores liberan shared → double free
```

## Comparación con Rust

```c
// C — ownership manual:
char *s = strdup("hello");
process(s);         // ¿process toma ownership?
printf("%s\n", s);  // ¿s sigue siendo válido?
free(s);            // ¿es seguro? ¿ya fue liberado?

// Rust — ownership en el tipo:
// let s = String::from("hello");
// process(s);         // s se MOVIÓ a process (ya no es válido)
// println!("{}", s);  // ERROR DE COMPILACIÓN: use of moved value
//
// O con préstamo:
// let s = String::from("hello");
// process(&s);        // prestado — s sigue siendo válido
// println!("{}", s);  // OK

// C++ — smart pointers:
// std::unique_ptr<int> p = std::make_unique<int>(42);
// process(std::move(p));  // ownership transferido
// *p;  // UB pero no error de compilación
```

---

## Ejercicios

### Ejercicio 1 — Documentar ownership

```c
// Para cada función, documentar el ownership de cada parámetro
// y del valor de retorno:
char *concat(const char *a, const char *b);
void list_add(struct List *list, void *item);
const char *map_get(struct Map *map, const char *key);
void map_set(struct Map *map, const char *key, char *value);
```

### Ejercicio 2 — Create/destroy pair

```c
// Implementar struct Config con:
// - char *filename
// - char **options (array dinámico de strings)
// - int num_options
// Implementar config_create(filename) y config_destroy(cfg).
// Implementar config_add_option(cfg, option).
// Verificar con valgrind que no hay leaks.
```

### Ejercicio 3 — Ownership transfer

```c
// Implementar un stack (LIFO) que tome ownership de los datos:
// - stack_push(stack, data) — toma ownership de data
// - stack_pop(stack) — retorna data, transfiere ownership al caller
// - stack_destroy(stack) — libera el stack Y todos los datos restantes
// Probar: push 5 strings, pop 2 (y liberarlos), luego destroy.
// Valgrind debe reportar 0 leaks.
```
