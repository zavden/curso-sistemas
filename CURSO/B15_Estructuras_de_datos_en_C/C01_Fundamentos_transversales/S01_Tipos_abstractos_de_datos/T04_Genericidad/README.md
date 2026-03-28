# T04 — Genericidad

---

## 1. El problema: estructuras de datos para cualquier tipo

Una pila de `int`, una pila de `double`, una pila de `struct Persona` — la
lógica es idéntica (push, pop, top). Escribir una versión por cada tipo
es inmantenible:

```c
// Sin genericidad — duplicación:
void stack_int_push(StackInt *s, int elem);
void stack_double_push(StackDouble *s, double elem);
void stack_string_push(StackString *s, char *elem);
// ... misma lógica, distinto tipo, N versiones
```

La **genericidad** permite escribir la estructura una vez y usarla con
cualquier tipo. C y Rust resuelven esto de formas radicalmente distintas.

---

## 2. Genericidad en C: void *

### El mecanismo

`void *` es un puntero genérico — puede apuntar a cualquier tipo. El TAD
almacena `void *` y delega al usuario la responsabilidad de saber qué tipo
hay detrás:

```c
// stack genérica con void *
typedef struct Stack Stack;

Stack *stack_create(void);
void   stack_destroy(Stack *s);

void   stack_push(Stack *s, void *elem);
void  *stack_pop(Stack *s);
void  *stack_top(const Stack *s);
bool   stack_empty(const Stack *s);
```

El usuario almacena lo que quiera:

```c
// Pila de int (requiere alocar en heap o usar trucos)
int *x = malloc(sizeof(int));
*x = 42;
stack_push(s, x);

int *top = (int *)stack_top(s);
printf("top = %d\n", *top);
```

### Implementación completa

```c
// generic_stack.h
#ifndef GENERIC_STACK_H
#define GENERIC_STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

// free_fn: función para liberar un elemento (puede ser NULL)
Stack *stack_create(void (*free_fn)(void *));
void   stack_destroy(Stack *s);

void   stack_push(Stack *s, void *elem);
void  *stack_pop(Stack *s);
void  *stack_top(const Stack *s);

bool   stack_empty(const Stack *s);
size_t stack_size(const Stack *s);

#endif
```

```c
// generic_stack.c
#include "generic_stack.h"
#include <stdlib.h>
#include <assert.h>

#define STACK_INIT_CAP 16

struct Stack {
    void  **data;           // array de void *
    size_t  size;
    size_t  capacity;
    void  (*free_fn)(void *);  // función para liberar elementos
};

Stack *stack_create(void (*free_fn)(void *)) {
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;

    s->data = malloc(STACK_INIT_CAP * sizeof(void *));
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->size     = 0;
    s->capacity = STACK_INIT_CAP;
    s->free_fn  = free_fn;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    if (s->free_fn) {
        for (size_t i = 0; i < s->size; i++) {
            s->free_fn(s->data[i]);
        }
    }
    free(s->data);
    free(s);
}

void stack_push(Stack *s, void *elem) {
    assert(s != NULL);
    if (s->size == s->capacity) {
        size_t new_cap = s->capacity * 2;
        void **new_data = realloc(s->data, new_cap * sizeof(void *));
        if (!new_data) return;  // silently fail — or assert/return bool
        s->data     = new_data;
        s->capacity = new_cap;
    }
    s->data[s->size] = elem;
    s->size++;
}

void *stack_pop(Stack *s) {
    assert(s != NULL && !stack_empty(s));
    s->size--;
    return s->data[s->size];
}

void *stack_top(const Stack *s) {
    assert(s != NULL && !stack_empty(s));
    return s->data[s->size - 1];
}

bool stack_empty(const Stack *s) {
    assert(s != NULL);
    return s->size == 0;
}

size_t stack_size(const Stack *s) {
    assert(s != NULL);
    return s->size;
}
```

### Uso con diferentes tipos

```c
#include "generic_stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // --- Pila de int ---
    Stack *int_stack = stack_create(free);  // free libera cada int *
    for (int i = 0; i < 5; i++) {
        int *val = malloc(sizeof(int));
        *val = i * 10;
        stack_push(int_stack, val);
    }

    while (!stack_empty(int_stack)) {
        int *val = (int *)stack_pop(int_stack);
        printf("%d ", *val);     // 40 30 20 10 0
        free(val);               // el caller liberó, no el stack
    }
    printf("\n");
    stack_destroy(int_stack);

    // --- Pila de strings ---
    Stack *str_stack = stack_create(free);  // free libera cada strdup
    stack_push(str_stack, strdup("hello"));
    stack_push(str_stack, strdup("world"));

    printf("%s\n", (char *)stack_top(str_stack));  // world
    stack_destroy(str_stack);   // free_fn libera strings restantes

    return 0;
}
```

---

## 3. Los problemas de void *

### 3.1 Sin type-safety

El compilador no puede verificar que el tipo que insertas es el que extraes:

```c
Stack *s = stack_create(free);

int *x = malloc(sizeof(int));
*x = 42;
stack_push(s, x);

// BUG: cast incorrecto — nadie lo detecta
double *d = (double *)stack_pop(s);
printf("%f\n", *d);    // comportamiento indefinido
```

No hay error de compilación, no hay warning. El bug se descubre (quizás) en
runtime con datos corruptos o crash.

### 3.2 Indirección obligatoria

`void *` es un puntero. Para almacenar un `int`, hay que alocarlo en el heap:

```c
// Almacenar int en stack void * — requiere malloc para cada entero
int *x = malloc(sizeof(int));
*x = 42;
stack_push(s, x);

// vs pila tipada:
stack_push(s, 42);   // directo, sin malloc
```

Cada `malloc` tiene overhead: llamada al allocator, metadata del bloque (~16-32
bytes), fragmentación. Para una pila de 10⁶ enteros, eso son 10⁶ llamadas a
`malloc` adicionales.

### 3.3 El usuario debe recordar el tipo

No hay nada en la API que diga "esta pila contiene ints". Es responsabilidad
del programador documentar y recordar qué tipo almacenó:

```c
Stack *temperatures = stack_create(free);  // ¿int? ¿double? ¿struct?
// Solo un comentario o el nombre de la variable indican el tipo
```

### 3.4 Función de liberación

El destructor no sabe qué tipo contiene, así que necesita un callback:

```c
// Para int *: free
Stack *s = stack_create(free);

// Para struct con punteros internos: función custom
void person_free(void *p) {
    Person *person = (Person *)p;
    free(person->name);
    free(person);
}
Stack *people = stack_create(person_free);

// Para datos en stack (no heap): NULL
int local_arr[] = {1, 2, 3};
Stack *refs = stack_create(NULL);   // NO liberar — son punteros a stack
stack_push(refs, &local_arr[0]);
```

Pasar el `free_fn` incorrecto produce double-free, use-after-free o leaks.

---

## 4. qsort: el ejemplo canónico de genericidad en C

`qsort` de `<stdlib.h>` es el ejemplo más conocido de código genérico en C:

```c
void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
```

| Parámetro | Rol |
|-----------|-----|
| `void *base` | Array de cualquier tipo |
| `size_t nmemb` | Número de elementos |
| `size_t size` | Tamaño de cada elemento (`sizeof`) |
| `compar` | Función de comparación (callback) |

### Funcionamiento interno

`qsort` no conoce el tipo de los elementos. Usa `size` para calcular
posiciones y `compar` para ordenar:

```
base: [ elem0 | elem1 | elem2 | elem3 | ... ]
       ╰─size─╯╰─size─╯╰─size─╯

Para acceder a elem[i]:  (char *)base + i * size
Para comparar:           compar(&elem[i], &elem[j])
Para intercambiar:       memcpy temporal de `size` bytes
```

Usa aritmética de `char *` (1 byte) porque `void *` no permite aritmética.

### Uso con int

```c
int compare_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    // No usar ia - ib: overflow si ia = INT_MAX, ib = -1
    if (ia < ib) return -1;
    if (ia > ib) return  1;
    return 0;
}

int arr[] = {30, 10, 50, 20, 40};
qsort(arr, 5, sizeof(int), compare_int);
// arr = {10, 20, 30, 40, 50}
```

### Uso con strings

```c
int compare_str(const void *a, const void *b) {
    // a y b son punteros a char* — es decir, char **
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

const char *words[] = {"banana", "apple", "cherry"};
qsort(words, 3, sizeof(char *), compare_str);
// words = {"apple", "banana", "cherry"}
```

### Uso con structs

```c
typedef struct {
    char name[64];
    int  age;
} Person;

// Ordenar por edad
int compare_by_age(const void *a, const void *b) {
    const Person *pa = (const Person *)a;
    const Person *pb = (const Person *)b;
    if (pa->age < pb->age) return -1;
    if (pa->age > pb->age) return  1;
    return 0;
}

// Ordenar por nombre
int compare_by_name(const void *a, const void *b) {
    const Person *pa = (const Person *)a;
    const Person *pb = (const Person *)b;
    return strcmp(pa->name, pb->name);
}

Person people[] = {{"Alice", 30}, {"Bob", 25}, {"Carol", 35}};
qsort(people, 3, sizeof(Person), compare_by_age);
// Resultado: Bob(25), Alice(30), Carol(35)
```

### Errores comunes con qsort

```c
// ERROR 1: sizeof incorrecto
qsort(arr, 5, sizeof(char *), compare_int);
// Debería ser sizeof(int) — corrupción silenciosa

// ERROR 2: comparador incorrecto
qsort(words, 3, sizeof(char *), compare_int);
// Interpreta punteros como int — basura

// ERROR 3: overflow en comparador
int bad_compare(const void *a, const void *b) {
    return *(int *)a - *(int *)b;    // overflow si a=INT_MAX, b=-1
}

// ERROR 4: cast incorrecto de strings
int bad_str_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
    // MAL: a es char **, no char *
    // Correcto: *(const char **)a
}
```

Ninguno de estos errores produce un error de compilación. Todos son bugs
en runtime.

---

## 5. Genericidad en Rust: genéricos + trait bounds

### El mecanismo

Rust usa **parámetros de tipo** (`<T>`) con **trait bounds** que restringen
qué tipos son válidos:

```rust
pub struct Stack<T> {
    data: Vec<T>,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { data: Vec::new() }
    }

    pub fn push(&mut self, elem: T) {
        self.data.push(elem);
    }

    pub fn pop(&mut self) -> Option<T> {
        self.data.pop()
    }

    pub fn top(&self) -> Option<&T> {
        self.data.last()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }
}
```

### Uso con diferentes tipos

```rust
// Pila de i32 — el compilador infiere T = i32
let mut int_stack = Stack::new();
int_stack.push(10);
int_stack.push(20);
int_stack.push(30);
println!("{:?}", int_stack.pop());   // Some(30)

// Pila de String
let mut str_stack = Stack::new();
str_stack.push("hello".to_string());
str_stack.push("world".to_string());
println!("{:?}", str_stack.top());   // Some("world")

// Pila de structs custom
#[derive(Debug)]
struct Point { x: f64, y: f64 }

let mut point_stack = Stack::new();
point_stack.push(Point { x: 1.0, y: 2.0 });
point_stack.push(Point { x: 3.0, y: 4.0 });
```

No hay casts, no hay `malloc` extra, no hay callbacks. El tipo es verificado
en compilación.

### Type-safety: el compilador rechaza errores

```rust
let mut s: Stack<i32> = Stack::new();
s.push(42);
s.push("hello");
//     ^^^^^^^ expected `i32`, found `&str`
```

El equivalente en C con `void *` compilaría sin error.

---

## 6. Trait bounds: restricciones sobre T

No todas las operaciones funcionan con cualquier tipo. Los trait bounds
expresan qué necesita `T`:

### Sin restricción: funciona con todo

```rust
impl<T> Stack<T> {
    pub fn push(&mut self, elem: T) { ... }  // solo mover — no requiere nada
    pub fn pop(&mut self) -> Option<T> { ... }
    pub fn len(&self) -> usize { ... }
}
```

`push`, `pop` y `len` no necesitan nada especial de `T`. Funcionan con
cualquier tipo.

### Con restricciones: solo tipos que cumplen

```rust
impl<T: PartialEq> Stack<T> {
    pub fn contains(&self, elem: &T) -> bool {
        self.data.contains(elem)   // requiere == → PartialEq
    }
}

impl<T: std::fmt::Display> Stack<T> {
    pub fn print(&self) {
        for elem in &self.data {
            print!("{} ", elem);   // requiere Display
        }
        println!();
    }
}

impl<T: Ord> Stack<T> {
    pub fn sort(&mut self) {
        self.data.sort();          // requiere orden total → Ord
    }
}
```

Estos métodos **existen condicionalmente**: `contains` solo está disponible
si `T` implementa `PartialEq`. Intentar usar `contains` con un tipo sin
`PartialEq` da error de compilación claro:

```rust
struct NoEq;

let mut s = Stack::new();
s.push(NoEq);
s.push(NoEq);
// s.contains(&NoEq);
// ERROR: the trait `PartialEq` is not implemented for `NoEq`
```

### Comparación con C

En C, la restricción equivalente a `T: PartialEq` es un callback:

```c
// C: el usuario pasa la función de comparación
bool stack_contains(Stack *s, void *elem,
                    int (*cmp)(const void *, const void *)) {
    for (size_t i = 0; i < s->size; i++) {
        if (cmp(s->data[i], elem) == 0) return true;
    }
    return false;
}

// Uso:
stack_contains(s, &target, compare_int);
```

Diferencias:
- C: el callback se pasa en cada llamada (se puede pasar el incorrecto)
- Rust: el trait bound es parte del tipo (se verifica una vez, en compilación)
- C: error de comparador → bug silencioso en runtime
- Rust: error de trait → error de compilación

---

## 7. Monomorphization: cómo Rust genera código

Cuando escribes `Stack<i32>` y `Stack<String>`, el compilador genera **dos
versiones separadas** del código — una especializada para cada tipo:

```
Código fuente:
  Stack<T> { push, pop, top, ... }

Después de monomorphization:
  Stack<i32>    { push(i32), pop() -> i32, ... }
  Stack<String> { push(String), pop() -> String, ... }
```

Esto es como si hubieras escrito las versiones especializadas a mano, pero
sin la duplicación en el código fuente.

### Consecuencias

| Aspecto | void * (C) | Genéricos (Rust) |
|---------|-----------|-----------------|
| Código generado | Una versión | N versiones (una por tipo usado) |
| Rendimiento | Indirección por void * + callback | Directo, sin indirección, inlineable |
| Tamaño binario | Menor | Mayor (código duplicado) |
| Tiempo compilación | Menor | Mayor |

### Por qué qsort es más lento que .sort()

```c
// C — qsort
qsort(arr, n, sizeof(int), compare_int);
// Cada comparación: llamada a función vía puntero (no inlineable)
// Cada swap: memcpy genérico de `size` bytes
```

```rust
// Rust — .sort()
arr.sort();
// Comparación: código inline especializado para i32
// Swap: swap optimizado para el tamaño exacto
```

En benchmarks, `.sort_unstable()` de Rust es típicamente 2-5x más rápido
que `qsort` para tipos primitivos, precisamente por monomorphization.

---

## 8. Genericidad con void * para TADs completos

### Set genérico en C

```c
// generic_set.h
#ifndef GENERIC_SET_H
#define GENERIC_SET_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Set Set;

// cmp: comparador (como qsort). Retorna 0 si iguales.
// free_fn: liberador de elementos (NULL si no se necesita liberar).
Set  *set_create(int (*cmp)(const void *, const void *),
                 void (*free_fn)(void *));
void  set_destroy(Set *s);

bool  set_insert(Set *s, void *elem);
bool  set_remove(Set *s, const void *elem);
bool  set_contains(const Set *s, const void *elem);
size_t set_size(const Set *s);

#endif
```

```c
// generic_set.c
#include "generic_set.h"
#include <stdlib.h>
#include <assert.h>

#define SET_INIT_CAP 8

struct Set {
    void **data;
    size_t  size;
    size_t  capacity;
    int   (*cmp)(const void *, const void *);
    void  (*free_fn)(void *);
};

static int set_find(const Set *s, const void *elem) {
    for (size_t i = 0; i < s->size; i++) {
        if (s->cmp(s->data[i], elem) == 0) return (int)i;
    }
    return -1;
}

Set *set_create(int (*cmp)(const void *, const void *),
                void (*free_fn)(void *)) {
    assert(cmp != NULL);   // comparador es obligatorio

    Set *s = malloc(sizeof(Set));
    if (!s) return NULL;

    s->data = malloc(SET_INIT_CAP * sizeof(void *));
    if (!s->data) { free(s); return NULL; }

    s->size     = 0;
    s->capacity = SET_INIT_CAP;
    s->cmp      = cmp;
    s->free_fn  = free_fn;
    return s;
}

void set_destroy(Set *s) {
    if (!s) return;
    if (s->free_fn) {
        for (size_t i = 0; i < s->size; i++) {
            s->free_fn(s->data[i]);
        }
    }
    free(s->data);
    free(s);
}

bool set_insert(Set *s, void *elem) {
    assert(s != NULL);
    if (set_find(s, elem) >= 0) return true;  // ya existe

    if (s->size == s->capacity) {
        size_t new_cap = s->capacity * 2;
        void **new_data = realloc(s->data, new_cap * sizeof(void *));
        if (!new_data) return false;
        s->data     = new_data;
        s->capacity = new_cap;
    }

    s->data[s->size] = elem;
    s->size++;
    return true;
}

bool set_remove(Set *s, const void *elem) {
    assert(s != NULL);
    int idx = set_find(s, elem);
    if (idx < 0) return false;

    if (s->free_fn) s->free_fn(s->data[idx]);
    s->data[idx] = s->data[s->size - 1];
    s->size--;
    return true;
}

bool set_contains(const Set *s, const void *elem) {
    assert(s != NULL);
    return set_find(s, elem) >= 0;
}

size_t set_size(const Set *s) {
    assert(s != NULL);
    return s->size;
}
```

### Uso del Set genérico

```c
#include "generic_set.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Comparadores ---

int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int main(void) {
    // Set de int
    Set *int_set = set_create(cmp_int, free);

    int *a = malloc(sizeof(int)); *a = 10;
    int *b = malloc(sizeof(int)); *b = 20;
    int *c = malloc(sizeof(int)); *c = 10;  // duplicado

    set_insert(int_set, a);
    set_insert(int_set, b);
    set_insert(int_set, c);    // no inserta (10 ya existe)
    // ⚠ c no fue insertado pero tampoco liberado → caller debe free(c)

    int key = 10;
    printf("contains 10: %d\n", set_contains(int_set, &key));  // 1
    printf("size: %zu\n", set_size(int_set));                    // 2

    set_destroy(int_set);  // libera a y b, pero c sigue vivo si no fue freed
    free(c);               // necesario — set_insert rechazó c

    // Set de strings
    Set *str_set = set_create(cmp_str, free);
    set_insert(str_set, strdup("hello"));
    set_insert(str_set, strdup("world"));

    printf("contains 'hello': %d\n",
           set_contains(str_set, "hello"));  // 1
    set_destroy(str_set);

    return 0;
}
```

Observa la complejidad: malloc para cada int, casts en cada comparador,
responsabilidad de liberar elementos rechazados. Todo esto desaparece con
genéricos de Rust.

---

## 9. Alternativa en C: macros para genericidad

Otra técnica de genericidad en C es usar macros del preprocesador para
generar código tipado:

```c
// stack_generic.h — macro que genera un stack tipado
#define DEFINE_STACK(TYPE, PREFIX)                                   \
                                                                     \
typedef struct {                                                     \
    TYPE  *data;                                                     \
    size_t size;                                                     \
    size_t capacity;                                                 \
} PREFIX##Stack;                                                     \
                                                                     \
static inline PREFIX##Stack *PREFIX##_stack_create(void) {           \
    PREFIX##Stack *s = malloc(sizeof(PREFIX##Stack));                 \
    if (!s) return NULL;                                             \
    s->data = malloc(16 * sizeof(TYPE));                             \
    if (!s->data) { free(s); return NULL; }                          \
    s->size = 0;                                                     \
    s->capacity = 16;                                                \
    return s;                                                        \
}                                                                    \
                                                                     \
static inline void PREFIX##_stack_destroy(PREFIX##Stack *s) {        \
    if (!s) return;                                                  \
    free(s->data);                                                   \
    free(s);                                                         \
}                                                                    \
                                                                     \
static inline void PREFIX##_stack_push(PREFIX##Stack *s, TYPE elem) {\
    if (s->size == s->capacity) {                                    \
        size_t new_cap = s->capacity * 2;                            \
        TYPE *new_data = realloc(s->data, new_cap * sizeof(TYPE));   \
        if (!new_data) return;                                       \
        s->data = new_data;                                          \
        s->capacity = new_cap;                                       \
    }                                                                \
    s->data[s->size++] = elem;                                       \
}                                                                    \
                                                                     \
static inline TYPE PREFIX##_stack_pop(PREFIX##Stack *s) {            \
    assert(!PREFIX##_stack_empty(s));                                 \
    return s->data[--s->size];                                       \
}                                                                    \
                                                                     \
static inline TYPE PREFIX##_stack_top(const PREFIX##Stack *s) {      \
    assert(!PREFIX##_stack_empty(s));                                 \
    return s->data[s->size - 1];                                     \
}                                                                    \
                                                                     \
static inline bool PREFIX##_stack_empty(const PREFIX##Stack *s) {    \
    return s->size == 0;                                             \
}
```

### Uso

```c
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "stack_generic.h"

DEFINE_STACK(int, int)       // genera IntStack, int_stack_create, etc.
DEFINE_STACK(double, dbl)    // genera DblStack, dbl_stack_create, etc.

int main(void) {
    IntStack *is = int_stack_create();
    int_stack_push(is, 42);
    int_stack_push(is, 99);
    printf("top: %d\n", int_stack_top(is));  // 99 — tipado, sin cast
    int_stack_destroy(is);

    DblStack *ds = dbl_stack_create();
    dbl_stack_push(ds, 3.14);
    printf("top: %f\n", dbl_stack_top(ds));  // 3.14
    dbl_stack_destroy(ds);

    return 0;
}
```

### Tradeoffs

| Aspecto | void * | Macros | Rust generics |
|---------|--------|--------|---------------|
| Type-safety | No | Sí | Sí |
| Rendimiento | Indirección | Directo (inline) | Directo (monomorphization) |
| Errores de compilación | Legibles | Ilegibles (macro-expanded) | Legibles |
| Debugging | Normal | Difícil (código expandido) | Normal |
| Encapsulación | struct opaco | Difícil (macros exponen todo) | Campos privados |
| Complejidad de uso | Media | Alta (macros arcanas) | Baja |

Las macros son la forma más cercana a los genéricos de Rust en C: generan
código especializado por tipo. Pero los errores de compilación apuntan al
código expandido (no al original), y el debugging muestra la versión
expandida. En la práctica, `void *` es más usado que macros en C.

---

## 10. Tabla resumen: C void * vs Rust generics

| Criterio | C (void *) | Rust (generics) |
|----------|-----------|-----------------|
| **Type-safety** | Ninguna — cast manual | Total — compilador verifica |
| **Almacenamiento** | Punteros (heap) | Directo (stack o heap) |
| **Comparación** | Callback en cada llamada | Trait bound, resuelto en compilación |
| **Liberación** | Callback `free_fn` | Drop automático |
| **Errores** | Runtime (crash, UB) | Compilación |
| **Rendimiento** | Indirección en cada acceso | Monomorphization, inlineable |
| **Binario** | Una copia del código | N copias (una por tipo) |
| **Curva aprendizaje** | Media (casts, callbacks) | Media (trait bounds, lifetimes) |
| **Flexibilidad** | Total (void * acepta todo) | Controlada (trait bounds) |

### Cuándo cada uno es apropiado

**void * en C**:
- Es lo único que hay en C (sin contar macros)
- Bibliotecas como GLib usan void * extensivamente y funcionan bien
- Para este curso: se implementa con void * para entender el mecanismo

**Genéricos en Rust**:
- Cualquier código nuevo en Rust debería usar genéricos
- Los genéricos eliminan una clase entera de bugs (type confusion)
- Para este curso: se usa `<T>` para la versión Rust de cada estructura

---

## Ejercicios

### Ejercicio 1 — Stack genérica con void *

Crea los archivos `generic_stack.h`, `generic_stack.c` y un `main.c` que:

1. Crea una pila de `int *` (con `free` como `free_fn`)
2. Hace push de 5 enteros
3. Hace pop e imprime todos
4. Destruye la pila

Compila y ejecuta.

**Predicción**: ¿En qué orden se imprimen los enteros respecto al orden
de push? Si olvidas el cast `(int *)` en el pop, ¿compila?

<details><summary>Respuesta</summary>

Los enteros se imprimen en orden **inverso** (LIFO):

```c
// push: 0, 10, 20, 30, 40
// pop:  40, 30, 20, 10, 0
```

Si olvidas el cast:

```c
void *val = stack_pop(s);
printf("%d\n", *val);      // WARNING o ERROR dependiendo del compilador
```

Con `-Wall -Wextra`, GCC puede advertir sobre desreferenciar `void *`
directamente. Pero si escribes `printf("%d\n", *(int *)stack_pop(s))` con
el cast incorrecto (ej: `*(double *)`), **compila sin warning** — es un
bug silencioso.

</details>

Limpieza:

```bash
rm -f generic_stack.o main.o generic_stack_demo
```

---

### Ejercicio 2 — qsort con tres tipos

Usa `qsort` para ordenar:

1. Un array de `int` en orden ascendente
2. Un array de `double` en orden descendente
3. Un array de `Person` (struct con `name` y `age`) por edad

```c
typedef struct {
    char name[32];
    int  age;
} Person;

int arr_int[] = {50, 10, 40, 20, 30};
double arr_dbl[] = {3.14, 1.41, 2.72, 0.58};
Person arr_person[] = {{"Alice", 30}, {"Bob", 25}, {"Carol", 35}};
```

**Predicción**: Para el orden descendente de doubles, ¿qué cambia en el
comparador respecto al ascendente? ¿Basta con negar el retorno?

<details><summary>Respuesta</summary>

```c
// Ascendente int
int cmp_int_asc(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

// Descendente double — invertir el orden de comparación
int cmp_dbl_desc(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (db > da) - (db < da);   // b antes que a = descendente
}

// Por edad
int cmp_person_age(const void *a, const void *b) {
    int aa = ((const Person *)a)->age;
    int ab = ((const Person *)b)->age;
    return (aa > ab) - (aa < ab);
}
```

Para descendente, se intercambian `a` y `b` en la comparación. Negar el
retorno (`return -cmp(a, b)`) también funciona, pero hay un caso borde:
si el comparador retorna `INT_MIN`, `-INT_MIN` produce overflow
(comportamiento indefinido). Invertir los operandos es más seguro.

Resultados:
- int: `{10, 20, 30, 40, 50}`
- double: `{3.14, 2.72, 1.41, 0.58}`
- Person: `Bob(25), Alice(30), Carol(35)`

</details>

---

### Ejercicio 3 — Error de tipo con void *

Ejecuta este código y analiza la salida:

```c
#include <stdio.h>
#include <stdlib.h>
#include "generic_stack.h"

int main(void) {
    Stack *s = stack_create(free);

    // Push un int
    int *x = malloc(sizeof(int));
    *x = 42;
    stack_push(s, x);

    // Pop como double (BUG)
    double *d = (double *)stack_pop(s);
    printf("value: %f\n", *d);

    free(d);
    stack_destroy(s);
    return 0;
}
```

**Predicción**: ¿Compila? ¿Qué valor imprime? ¿Por qué no es 42.0?

<details><summary>Respuesta</summary>

Compila sin errores ni warnings (incluso con `-Wall -Wextra`).

El valor impreso es **basura** — no es 42.0. Posibles salidas:
```
value: 0.000000
value: 2078764449792.000000
value: -nan
```

¿Por qué? `x` apunta a 4 bytes con el patrón de bits de `42` como `int`.
Al hacer `*(double *)d`, se leen **8 bytes** (sizeof(double)) empezando
en esa dirección:
- Los primeros 4 son el int 42
- Los siguientes 4 son basura (lo que haya después en el heap)
- Esos 8 bytes se interpretan como IEEE 754 double — resultado impredecible

Además, `free(d)` libera el bloque de 4 bytes (`malloc(sizeof(int))`) con
un puntero `double *` — funciona porque `free` solo usa la dirección, no
el tipo. Pero el acceso de 8 bytes sobre un bloque de 4 es buffer overread.

Valgrind detectaría: "Invalid read of size 8" y "Address is 0 bytes after
a block of size 4 alloc'd".

En Rust, este error es **imposible**: `Stack<i32>::pop()` retorna `i32`,
no se puede confundir con `f64`.

</details>

---

### Ejercicio 4 — Stack genérica en Rust

Implementa `Stack<T>` en Rust con `new`, `push`, `pop`, `top`, `is_empty`,
`len`. Úsala con `i32`, `String` y un struct custom.

**Predicción**: ¿Necesitas especificar `T` explícitamente al crear la
pila? ¿Qué pasa si haces push de un `i32` y luego de un `String` en
la misma pila?

<details><summary>Respuesta</summary>

```rust
struct Stack<T> {
    data: Vec<T>,
}

impl<T> Stack<T> {
    fn new() -> Self { Stack { data: Vec::new() } }
    fn push(&mut self, elem: T) { self.data.push(elem); }
    fn pop(&mut self) -> Option<T> { self.data.pop() }
    fn top(&self) -> Option<&T> { self.data.last() }
    fn is_empty(&self) -> bool { self.data.is_empty() }
    fn len(&self) -> usize { self.data.len() }
}

fn main() {
    let mut s = Stack::new();   // T se infiere del primer push
    s.push(42);                 // T = i32
    s.push(99);
    println!("{:?}", s.pop());  // Some(99)

    let mut ss = Stack::new();
    ss.push("hello".to_string());  // T = String
    println!("{:?}", ss.top());    // Some("hello")
}
```

No necesitas especificar `T` — el compilador lo infiere del primer `push`.
Alternativamente: `let mut s: Stack<i32> = Stack::new()` o
`let mut s = Stack::<i32>::new()`.

Si intentas mezclar tipos:

```rust
let mut s = Stack::new();
s.push(42);          // T = i32
s.push("hello");     // ERROR: expected `i32`, found `&str`
```

El compilador fija `T` con el primer uso y rechaza tipos incompatibles.
Esto es imposible de verificar con `void *` en C.

</details>

---

### Ejercicio 5 — Trait bound condicional

Agrega un método `contains` a `Stack<T>` que solo esté disponible cuando
`T: PartialEq`, y un método `sort` que solo esté disponible cuando `T: Ord`.

Testea:

```rust
let mut s = Stack::new();
s.push(30); s.push(10); s.push(20);
println!("contains 10: {}", s.contains(&10));  // true
s.sort();
// ¿Cómo verificar que está ordenado?
```

**Predicción**: ¿Qué pasa si creas `Stack<f64>` e intentas llamar `sort()`?
¿Por qué `f64` no implementa `Ord`?

<details><summary>Respuesta</summary>

```rust
impl<T: PartialEq> Stack<T> {
    fn contains(&self, elem: &T) -> bool {
        self.data.contains(elem)
    }
}

impl<T: Ord> Stack<T> {
    fn sort(&mut self) {
        self.data.sort();
    }
}
```

Con `f64`:

```rust
let mut s = Stack::new();
s.push(3.14);
s.push(1.41);
s.contains(&3.14);   // OK — f64 implementa PartialEq
s.sort();             // ERROR: `f64` does not implement `Ord`
```

`f64` no implementa `Ord` porque `NaN != NaN` y `NaN` no es comparable
con ningún valor. `Ord` requiere **orden total** (todo par es comparable),
pero `NaN` rompe esa propiedad:

```rust
let nan = f64::NAN;
println!("{}", nan == nan);   // false
println!("{}", nan < 1.0);    // false
println!("{}", nan > 1.0);    // false
// NaN no es ni igual, ni menor, ni mayor que nada
```

`f64` implementa `PartialOrd` (orden parcial — funciona excepto con NaN)
pero no `Ord`. Para ordenar floats se usa `sort_by` con
`a.partial_cmp(b).unwrap()` (panic si hay NaN) o
`a.total_cmp(b)` (C23/IEEE 754 total ordering).

En C, `qsort` con floats que incluyen NaN produce **orden indeterminado**
sin ningún error — un bug mucho más difícil de encontrar.

</details>

---

### Ejercicio 6 — Ownership en push: move vs copy

Analiza qué pasa con ownership en cada caso:

```rust
let mut s = Stack::new();

// Caso A: tipo Copy
let x = 42;
s.push(x);
println!("x = {}", x);     // ¿compila?

// Caso B: tipo no-Copy
let mut s2 = Stack::new();
let name = String::from("hello");
s2.push(name);
println!("name = {}", name);   // ¿compila?
```

**Predicción**: ¿Cuál de los dos compila y cuál no? ¿Por qué?

<details><summary>Respuesta</summary>

**Caso A: compila**. `i32` implementa `Copy`. Cuando se hace `s.push(x)`,
se copia el valor — `x` sigue siendo válido.

**Caso B: no compila**:
```
error[E0382]: borrow of moved value: `name`
  --> src/main.rs:8:28
   |
6  | let name = String::from("hello");
   |     ---- move occurs because `name` has type `String`
7  | s2.push(name);
   |         ---- value moved here
8  | println!("name = {}", name);
   |                        ^^^^ value borrowed here after move
```

`String` no implementa `Copy`. `s2.push(name)` **mueve** el ownership al
stack — `name` queda invalidado.

Soluciones:
```rust
// Opción 1: clonar
s2.push(name.clone());
println!("name = {}", name);   // OK — name no se movió

// Opción 2: aceptar el move y no usar name después
s2.push(name);
// name ya no se usa
```

En C con `void *`, este problema no existe de la misma forma — los punteros
se copian libremente. Pero eso causa el problema opuesto: dos partes del
código pueden tener punteros al mismo dato, y si uno hace `free`, el otro
tiene un dangling pointer. Rust previene esto con ownership.

</details>

---

### Ejercicio 7 — Comparadores incorrectos en qsort

Identifica el bug en cada comparador y explica la consecuencia:

```c
// Comparador A: para int
int cmp_a(const void *a, const void *b) {
    return *(int *)a - *(int *)b;
}

// Comparador B: para strings
int cmp_b(const void *a, const void *b) {
    return strcmp((char *)a, (char *)b);
}

// Comparador C: para doubles
int cmp_c(const void *a, const void *b) {
    double da = *(double *)a;
    double db = *(double *)b;
    return (int)(da - db);
}
```

**Predicción**: ¿Cuáles producen resultados incorrectos? ¿Para qué
valores fallan?

<details><summary>Respuesta</summary>

**Comparador A — overflow**:

```c
return *(int *)a - *(int *)b;
```

Falla si `a - b` produce overflow. Ejemplo: `a = INT_MAX (2147483647)`,
`b = -1`. `a - b = 2147483648` → overflow → resultado negativo (en
complemento a dos) → qsort cree que `a < b` cuando `a > b`.

Corrección:
```c
int ia = *(const int *)a, ib = *(const int *)b;
return (ia > ib) - (ia < ib);
```

**Comparador B — cast incorrecto**:

```c
return strcmp((char *)a, (char *)b);
```

Si se usa con `const char *words[]`, cada elemento del array es un `char *`.
`qsort` pasa **punteros a los elementos**, es decir `char **`. El comparador
debería hacer:

```c
return strcmp(*(const char **)a, *(const char **)b);
```

Sin la doble indirección, `strcmp` recibe la **dirección del puntero**
(interpretar los bytes del puntero como caracteres) en vez del string.
Resultado: orden basura o crash (acceso a dirección inválida).

**Comparador C — truncamiento**:

```c
return (int)(da - db);
```

Falla para diferencias menores que 1: si `da = 1.1` y `db = 1.9`,
`da - db = -0.8`, `(int)(-0.8) = 0` → qsort cree que son iguales.

Corrección:
```c
return (da > db) - (da < db);
```

Los tres bugs compilan sin warning. En Rust, los dos primeros son
imposibles (type system) y el tercero requiere escribir un comparador
explícito que el compilador tipifica.

</details>

---

### Ejercicio 8 — Set genérico en C: strings

Usando la implementación del Set genérico de la sección 8, crea un Set de
strings. Inserta "hello", "world", "hello" (duplicado). Verifica `contains`
y `size`.

**Predicción**: ¿Qué `free_fn` debes pasar? ¿Qué comparador? Si haces
`set_insert(s, "literal")` en vez de `set_insert(s, strdup("literal"))`,
¿qué pasa al destruir?

<details><summary>Respuesta</summary>

```c
int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

Set *s = set_create(cmp_str, free);

set_insert(s, strdup("hello"));
set_insert(s, strdup("world"));

char *dup = strdup("hello");
if (!set_insert(s, dup) || set_contains(s, dup)) {
    // "hello" ya existía — el duplicado no se insertó
    // pero strdup alocó memoria → debemos liberar dup
    // En realidad set_insert retorna true (ya existe),
    // pero no insertó dup → dup debe ser freed por el caller
}
free(dup);  // liberar el duplicado rechazado

printf("size: %zu\n", set_size(s));            // 2
printf("contains hello: %d\n",
       set_contains(s, "hello"));               // 1

set_destroy(s);
```

Comparador: `strcmp` — pero aquí NO se necesita doble indirección porque el
Set almacena `void *` que es directamente el `char *` del string (no un
array de `char *` como en `qsort`).

Si usas `set_insert(s, "literal")` (string literal, no `strdup`):
- Los string literals viven en el segmento de datos de solo lectura
- `set_destroy` llama `free("literal")` → **comportamiento indefinido**
- `free` de un puntero que no vino de `malloc` corrompe el heap
- Puede crashear, corromper datos, o parecer funcionar (y fallar después)

Solución: siempre `strdup` para strings que irán al Set, o pasar
`free_fn = NULL` si solo guardas referencias a datos que se liberan
en otro lugar.

</details>

---

### Ejercicio 9 — Macro vs void * vs Rust

Implementa una pila de `int` de las tres formas:

1. `void *` con `malloc` para cada int
2. Macro `DEFINE_STACK(int, int)` (de la sección 9)
3. Rust `Stack<i32>`

Haz push de 10⁶ enteros y mide el tiempo de cada versión.

**Predicción**: Ordena las tres del más rápido al más lento. ¿Cuántas
veces más lenta crees que es la versión `void *` respecto a la de Rust?

<details><summary>Respuesta</summary>

Orden esperado (más rápido a más lento):

1. **Macro C / Rust** — empate aproximado
2. **void * C** — significativamente más lento

¿Por qué?

| Versión | Alocaciones para 10⁶ push | Acceso |
|---------|--------------------------|--------|
| void * | 10⁶ malloc(sizeof(int)) + ~20 reallocs del array | Indirecto: `*(int *)data[i]` |
| Macro | ~20 reallocs del array (solo) | Directo: `data[i]` |
| Rust | ~20 reallocs del Vec (solo) | Directo: `data[i]` |

La versión `void *` es ~3-10x más lenta por:
- 10⁶ llamadas extra a `malloc` (cada una tiene overhead: búsqueda en free list, metadata)
- Cache misses: los int están dispersos en el heap en vez de contiguos
- Indirección: cada acceso pasa por un puntero extra

Las versiones macro y Rust almacenan los int contiguos en un array — cache
friendly, sin `malloc` individual. La diferencia entre macro y Rust es
mínima (ambos generan código similar al compilar).

Medición en C (void *):
```c
clock_t start = clock();
for (int i = 0; i < 1000000; i++) {
    int *val = malloc(sizeof(int));
    *val = i;
    stack_push(s, val);
}
double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
```

Medición en Rust:
```rust
let start = std::time::Instant::now();
for i in 0..1_000_000 {
    s.push(i);
}
let elapsed = start.elapsed();
```

</details>

---

### Ejercicio 10 — Diseñar interfaz genérica: C y Rust

Diseña la interfaz (solo firmas, no implementación) para un TAD genérico
"Diccionario" (clave → valor) en ambos lenguajes:

- `create`/`new` — constructor
- `insert(key, value)` — inserta o actualiza
- `get(key)` → valor (o indicador de no encontrado)
- `remove(key)` → si existía
- `contains_key(key)` → bool
- `size` → cantidad de pares
- `destroy` → liberar

**Predicción**: ¿Cuántos callbacks necesita la versión C? (comparador
de claves, liberador de claves, liberador de valores...) ¿Cuántos trait
bounds necesita la versión Rust?

<details><summary>Respuesta</summary>

**C — 3 callbacks**:

```c
typedef struct Dict Dict;

Dict *dict_create(
    int  (*key_cmp)(const void *, const void *),  // comparar claves
    void (*key_free)(void *),                      // liberar clave
    void (*val_free)(void *)                       // liberar valor
);
void   dict_destroy(Dict *d);

// Retorna valor anterior si existía (caller debe liberar), NULL si nuevo
void  *dict_insert(Dict *d, void *key, void *value);
// Retorna puntero al valor (no transferir ownership), NULL si no existe
void  *dict_get(const Dict *d, const void *key);
bool   dict_remove(Dict *d, const void *key);
bool   dict_contains_key(const Dict *d, const void *key);
size_t dict_size(const Dict *d);
```

3 callbacks, más la complejidad de quién posee qué: ¿`insert` copia la
clave o toma ownership? ¿`get` retorna copia o referencia? ¿`remove`
libera clave y valor o los retorna? Cada decisión afecta al caller.

**Rust — 1-2 trait bounds**:

```rust
pub struct Dict<K, V> {
    // ... campos privados ...
}

impl<K: Eq, V> Dict<K, V> {
    pub fn new() -> Self;
    pub fn insert(&mut self, key: K, value: V) -> Option<V>;
    pub fn get(&self, key: &K) -> Option<&V>;
    pub fn remove(&mut self, key: &K) -> Option<V>;
    pub fn contains_key(&self, key: &K) -> bool;
    pub fn len(&self) -> usize;
    pub fn is_empty(&self) -> bool;
    // destroy: Drop automático
}
```

1 trait bound (`K: Eq`, o `K: Eq + Hash` si se usa hash table). No hay
callbacks de liberación — `Drop` se encarga. No hay ambigüedad de ownership:
`insert` toma ownership de key y value, `get` retorna `&V` (referencia),
`remove` retorna `Option<V>` (devuelve el ownership del valor).

El tipo de retorno `Option<V>` de `insert` comunica exactamente: "si ya
había un valor con esa clave, te lo devuelvo (el viejo); si no, `None`".
En C, esta semántica requiere documentación cuidadosa.

</details>
