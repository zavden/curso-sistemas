# T02 — Implementación con array en C

---

## 1. La idea

La implementación más directa de un stack usa un **array** como almacenamiento
y un **índice** (`top`) que marca la siguiente posición libre. Push escribe en
`data[top]` e incrementa `top`. Pop decrementa `top` y retorna `data[top]`.

```
Capacidad = 5

Estado vacío:
  top = 0
  data: [ _ | _ | _ | _ | _ ]
              posiciones 0-4

push(10):
  data[0] = 10, top = 1
  data: [ 10 | _ | _ | _ | _ ]
          ↑
         top=1

push(20):
  data[1] = 20, top = 2
  data: [ 10 | 20 | _ | _ | _ ]
                ↑
               top=2

push(30):
  data[2] = 30, top = 3
  data: [ 10 | 20 | 30 | _ | _ ]
                      ↑
                     top=3

pop() → 30:
  top = 2, retorna data[2]
  data: [ 10 | 20 | 30 | _ | _ ]
                ↑
               top=2  (30 sigue ahí pero es inaccesible)

peek() → 20:
  retorna data[top-1] = data[1]
  data: [ 10 | 20 | 30 | _ | _ ]
                ↑
               top=2
```

### Convención de `top`

Hay dos convenciones comunes:

```
Convención A (usaremos esta):
  top = número de elementos = índice de la siguiente posición libre
  Pila vacía: top = 0
  Último elemento: data[top - 1]

Convención B:
  top = índice del último elemento insertado
  Pila vacía: top = -1
  Último elemento: data[top]
```

La convención A es más natural porque `top` coincide con el tamaño y no
necesita valores negativos.

---

## 2. El struct

```c
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int *data;         // array de elementos
    size_t top;        // número de elementos (índice del próximo libre)
    size_t capacity;   // tamaño máximo del array
} Stack;
```

Layout en memoria:

```
Stack s:
  ┌──────────┬─────┬──────────┐
  │ data (*)─┼──►  │ top      │  capacity  │
  └──────────┴─────┴──────────┘
                │
                ▼
  heap:  [ elem0 | elem1 | elem2 | ... | elemN-1 ]
         ◄──────── capacity × sizeof(int) ────────►
```

---

## 3. Implementación completa

### stack.h

```c
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

Stack  *stack_create(size_t capacity);
void    stack_destroy(Stack *s);
bool    stack_push(Stack *s, int elem);
bool    stack_pop(Stack *s, int *out);
bool    stack_peek(const Stack *s, int *out);
bool    stack_is_empty(const Stack *s);
size_t  stack_size(const Stack *s);

#endif
```

### stack.c

```c
#include "stack.h"
#include <stdlib.h>
#include <assert.h>

struct Stack {
    int *data;
    size_t top;
    size_t capacity;
};

Stack *stack_create(size_t capacity) {
    assert(capacity > 0);
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;

    s->data = malloc(capacity * sizeof(int));
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->top = 0;
    s->capacity = capacity;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) return false;  // llena
    s->data[s->top++] = elem;
    return true;
}

bool stack_pop(Stack *s, int *out) {
    if (s->top == 0) return false;  // vacía
    *out = s->data[--s->top];
    return true;
}

bool stack_peek(const Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[s->top - 1];
    return true;
}

bool stack_is_empty(const Stack *s) {
    return s->top == 0;
}

size_t stack_size(const Stack *s) {
    return s->top;
}
```

### main.c

```c
#include <stdio.h>
#include "stack.h"

int main(void) {
    Stack *s = stack_create(10);
    if (!s) {
        fprintf(stderr, "Failed to create stack\n");
        return 1;
    }

    stack_push(s, 10);
    stack_push(s, 20);
    stack_push(s, 30);

    int val;
    stack_peek(s, &val);
    printf("peek: %d\n", val);          // 30

    printf("size: %zu\n", stack_size(s)); // 3

    while (stack_pop(s, &val)) {
        printf("pop: %d\n", val);
    }
    // pop: 30, pop: 20, pop: 10

    printf("empty: %d\n", stack_is_empty(s)); // 1

    stack_destroy(s);
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -Wall -Wextra -std=c11 -o stack_demo stack.c main.c
./stack_demo
```

### Verificar con Valgrind

```bash
gcc -g -Wall -Wextra -std=c11 -o stack_demo stack.c main.c
valgrind --leak-check=full ./stack_demo
```

Salida esperada:

```
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```

---

## 4. Anatomía de cada operación

### push — escritura en el tope

```c
bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) return false;
    s->data[s->top++] = elem;
    //       ↑     ↑
    //       │     └─ post-incremento: top se incrementa DESPUÉS de la asignación
    //       └─ escribe en data[top], luego top++
    return true;
}
```

Desglose de `s->data[s->top++] = elem`:

```
1. Evalúa s->top          (e.g., top = 2)
2. Asigna s->data[2] = elem
3. Incrementa s->top a 3
```

Es equivalente a:

```c
s->data[s->top] = elem;
s->top = s->top + 1;
```

### pop — lectura del tope

```c
bool stack_pop(Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];
    //              ↑
    //              └─ pre-decremento: top se decrementa ANTES de la lectura
    return true;
}
```

Desglose de `*out = s->data[--s->top]`:

```
1. Decrementa s->top      (e.g., top = 3 → 2)
2. Lee s->data[2]
3. Asigna a *out
```

### Por qué pre-decremento en pop y post-incremento en push

```
top apunta a la SIGUIENTE posición libre.

push: escribe en top, luego avanza
  data[top] = elem;   // escribe donde top apunta
  top++;               // avanza al siguiente libre

pop: retrocede, luego lee
  top--;               // retrocede al último elemento
  return data[top];    // lee lo que estaba ahí
```

Si usaras post-decremento en pop (`data[top--]`), leerías la posición vacía
un lugar después del último elemento.

---

## 5. Stack overflow — pila llena

Con un array de tamaño fijo, hay un límite:

```c
bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) return false;  // ← stack overflow
    // ...
}
```

### Alternativa: crecimiento dinámico

En vez de retornar `false`, el stack puede crecer:

```c
static bool stack_grow(Stack *s) {
    size_t new_cap = s->capacity * 2;
    int *new_data = realloc(s->data, new_cap * sizeof(int));
    if (!new_data) return false;

    s->data = new_data;
    s->capacity = new_cap;
    return true;
}

bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) {
        if (!stack_grow(s)) return false;
    }
    s->data[s->top++] = elem;
    return true;
}
```

Con duplicación de capacidad, `push` es $O(1)$ amortizado (como vimos en
el análisis amortizado de C01/S02/T03).

### Cuándo NO crecer

En sistemas embebidos o de tiempo real, `realloc` no es aceptable:
- **Tiempo impredecible**: `realloc` puede copiar todo el array ($O(n)$)
- **Fragmentación**: en sistemas con poca memoria, `realloc` puede fallar
- **Requisitos de tiempo real**: cada operación debe ser $O(1)$ estricto

En estos casos, se usa array de tamaño fijo con verificación de overflow.

---

## 6. Errores comunes

### Error 1: off-by-one en pop

```c
// MAL: lee basura
bool stack_pop_bad(Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[s->top];    // top apunta al SIGUIENTE libre, no al tope
    s->top--;
    return true;
}

// BIEN:
bool stack_pop(Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];  // primero decrementa, luego lee
    return true;
}
```

### Error 2: olvidar verificar overflow en push

```c
// MAL: buffer overflow si la pila está llena
void stack_push_bad(Stack *s, int elem) {
    s->data[s->top++] = elem;  // sin verificación de capacidad
}
// Si top == capacity, escribe fuera del array → UB
```

### Error 3: memory leak en destroy

```c
// MAL: solo libera el struct, no el array
void stack_destroy_bad(Stack *s) {
    free(s);   // s->data queda en el heap sin liberar
}

// BIEN: libera ambos
void stack_destroy(Stack *s) {
    if (!s) return;
    free(s->data);   // primero el array
    free(s);         // luego el struct
}
```

El orden importa: si liberas `s` primero, ya no puedes acceder a `s->data`.

### Error 4: use-after-free

```c
Stack *s = stack_create(10);
stack_push(s, 42);
stack_destroy(s);
int val;
stack_pop(s, &val);   // UB: s ya fue liberado
```

### Error 5: olvidar verificar retorno de malloc

```c
// MAL: no verifica
Stack *stack_create(size_t cap) {
    Stack *s = malloc(sizeof(Stack));
    s->data = malloc(cap * sizeof(int));   // si malloc falla, s->data = NULL
    s->top = 0;                             // y el próximo push crashea
    s->capacity = cap;
    return s;
}
```

---

## 7. Versión con array estático (stack allocation)

Para stacks de tamaño conocido en compile time, se puede evitar `malloc`
completamente:

```c
#define STACK_CAPACITY 100

typedef struct {
    int data[STACK_CAPACITY];   // array en el struct, no en el heap
    size_t top;
} StaticStack;

void static_stack_init(StaticStack *s) {
    s->top = 0;
}

bool static_stack_push(StaticStack *s, int elem) {
    if (s->top >= STACK_CAPACITY) return false;
    s->data[s->top++] = elem;
    return true;
}

bool static_stack_pop(StaticStack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];
    return true;
}
```

Uso:

```c
StaticStack s;
static_stack_init(&s);
static_stack_push(&s, 42);
// No necesita destroy — el array está en el struct
```

### Tradeoffs

| Aspecto | Heap (malloc) | Stack/static |
|---------|--------------|-------------|
| Necesita `free` | Sí | No |
| Tamaño flexible | Sí (parámetro) | No (constante de compilación) |
| Dónde vive | Heap | Stack del programa o static |
| Riesgo de stack overflow (del programa) | No | Sí, si el array es grande |

La versión con `malloc` es más flexible. La estática es más simple y no
puede tener leaks.

---

## 8. Versión genérica con void *

Para almacenar cualquier tipo, se usa `void *` con `memcpy`:

```c
#include <string.h>

typedef struct {
    void *data;         // buffer genérico
    size_t top;
    size_t capacity;
    size_t elem_size;   // tamaño de cada elemento
} GenericStack;

GenericStack *gstack_create(size_t capacity, size_t elem_size) {
    GenericStack *s = malloc(sizeof(GenericStack));
    if (!s) return NULL;
    s->data = malloc(capacity * elem_size);
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    s->elem_size = elem_size;
    return s;
}

void gstack_destroy(GenericStack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

bool gstack_push(GenericStack *s, const void *elem) {
    if (s->top >= s->capacity) return false;
    // Calcular offset en bytes y copiar
    void *dest = (char *)s->data + s->top * s->elem_size;
    memcpy(dest, elem, s->elem_size);
    s->top++;
    return true;
}

bool gstack_pop(GenericStack *s, void *out) {
    if (s->top == 0) return false;
    s->top--;
    void *src = (char *)s->data + s->top * s->elem_size;
    memcpy(out, src, s->elem_size);
    return true;
}
```

### Uso

```c
// Stack de doubles
GenericStack *sd = gstack_create(100, sizeof(double));
double pi = 3.14159;
gstack_push(sd, &pi);
double out;
gstack_pop(sd, &out);  // out = 3.14159
gstack_destroy(sd);

// Stack de structs
typedef struct { int x, y; } Point;
GenericStack *sp = gstack_create(50, sizeof(Point));
Point p = {10, 20};
gstack_push(sp, &p);
Point q;
gstack_pop(sp, &q);  // q = {10, 20}
gstack_destroy(sp);
```

### La aritmética de `void *`

C no permite aritmética directa sobre `void *` (el estándar no define el
tamaño de `void`). Por eso se castea a `char *`:

```c
// void *dest = s->data + offset;         // NO: aritmética sobre void *
void *dest = (char *)s->data + offset;    // SÍ: char * avanza 1 byte por unidad

// offset = s->top * s->elem_size (en bytes)
// Si top=2 y elem_size=8 (double): offset = 16 bytes desde el inicio
```

---

## 9. Complejidad y rendimiento

### Complejidad temporal

| Operación | Array fijo | Array dinámico |
|-----------|-----------|---------------|
| `push` | $O(1)$ | $O(1)$ amortizado |
| `pop` | $O(1)$ | $O(1)$ |
| `peek` | $O(1)$ | $O(1)$ |
| `is_empty` | $O(1)$ | $O(1)$ |
| `size` | $O(1)$ | $O(1)$ |
| `create` | $O(1)$ + malloc | $O(1)$ + malloc |
| `destroy` | $O(1)$ + free | $O(1)$ + free |

### Complejidad espacial

```
Array fijo:     O(capacity) — siempre usa capacity × sizeof(T)
Array dinámico: O(n) a O(2n) — entre n y 2n posiciones asignadas
```

### Rendimiento real

La implementación con array tiene excelente rendimiento por **cache locality**:
los elementos están contiguos en memoria, lo que maximiza los hits de cache.
Push y pop acceden siempre a posiciones contiguas.

```
Array:
  cache line:  [ elem0 | elem1 | elem2 | elem3 ]
  push/pop acceden secuencialmente → cache-friendly

Lista enlazada (comparación):
  nodo1 → nodo2 → nodo3
  (cada nodo puede estar en cualquier lugar del heap → cache-unfriendly)
```

---

## 10. Makefile

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
LDFLAGS =

TARGET  = stack_demo
SRCS    = stack.c main.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c stack.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-reachable=yes ./$(TARGET)

.PHONY: all clean valgrind
```

---

## Ejercicios

### Ejercicio 1 — Implementar y probar

Crea los archivos `stack.h`, `stack.c` y `main.c` con la implementación de
la sección 3. Compila, ejecuta y verifica con Valgrind que no hay leaks.

```bash
gcc -g -Wall -Wextra -std=c11 -o stack_demo stack.c main.c
valgrind --leak-check=full ./stack_demo
```

**Prediccion**: Valgrind reportará 2 allocs y 2 frees (el struct + el array)?

<details><summary>Respuesta</summary>

```
==12345== HEAP SUMMARY:
==12345==   in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 2 allocs, 2 frees, 56 bytes allocated
==12345==
==12345== All heap blocks were freed -- no leaks are possible
```

Sí, exactamente 2 allocs y 2 frees:
1. `malloc(sizeof(Stack))` — el struct (16 o 24 bytes dependiendo del padding)
2. `malloc(capacity * sizeof(int))` — el array (e.g., 10 × 4 = 40 bytes)

Y los correspondientes `free(s->data)` + `free(s)` en `stack_destroy`.

Si Valgrind reporta leaks, revisa que `stack_destroy` libere ambos, y que
`main` llame a `stack_destroy` antes de retornar.

</details>

---

### Ejercicio 2 — Provocar y detectar overflow

Modifica `main.c` para crear un stack de capacidad 3 e intentar hacer 5
pushes. Verifica que los últimos 2 retornan `false`:

```c
int main(void) {
    Stack *s = stack_create(3);
    for (int i = 1; i <= 5; i++) {
        bool ok = stack_push(s, i * 10);
        printf("push(%d): %s\n", i * 10, ok ? "ok" : "OVERFLOW");
    }
    stack_destroy(s);
    return 0;
}
```

**Prediccion**: Qué pasa si quitas la verificación de capacidad del push?

<details><summary>Respuesta</summary>

Salida esperada:

```
push(10): ok
push(20): ok
push(30): ok
push(40): OVERFLOW
push(50): OVERFLOW
```

Si quitas la verificación de capacidad:

```c
// SIN verificación
bool stack_push(Stack *s, int elem) {
    s->data[s->top++] = elem;  // escribe fuera del array si top >= capacity
    return true;
}
```

Lo que pasa:
- `push(40)` escribe en `s->data[3]`, que está fuera del array de 3 elementos
- **Buffer overflow** — escribe en memoria que no le pertenece
- Puede sobreescribir metadatos de malloc, otros datos del heap, o causar
  un crash
- Es **undefined behavior** — puede parecer que funciona o crashear
  impredeciblemente

Valgrind con `--tool=memcheck` o AddressSanitizer (`-fsanitize=address`)
detectarían la escritura fuera de rango.

```bash
gcc -g -fsanitize=address -o stack_demo stack.c main.c
./stack_demo
# ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
```

</details>

---

### Ejercicio 3 — Implementar crecimiento dinámico

Agrega la función `stack_grow` de la sección 5 y modifica `stack_push` para
que crezca automáticamente. Verifica que un stack creado con capacidad 2
puede aceptar 1000 pushes:

```c
int main(void) {
    Stack *s = stack_create(2);
    for (int i = 0; i < 1000; i++) {
        stack_push(s, i);
    }
    printf("size: %zu\n", stack_size(s));   // 1000

    int val;
    stack_pop(s, &val);
    printf("top: %d\n", val);              // 999

    stack_destroy(s);
    return 0;
}
```

**Prediccion**: Cuántas veces se llama `realloc` para llegar de capacidad 2
a 1000? (con duplicación).

<details><summary>Respuesta</summary>

Modifica `stack.c`:

```c
static bool stack_grow(Stack *s) {
    size_t new_cap = s->capacity * 2;
    int *new_data = realloc(s->data, new_cap * sizeof(int));
    if (!new_data) return false;
    s->data = new_data;
    s->capacity = new_cap;
    return true;
}

bool stack_push(Stack *s, int elem) {
    if (s->top >= s->capacity) {
        if (!stack_grow(s)) return false;
    }
    s->data[s->top++] = elem;
    return true;
}
```

Reallocs necesarios: la capacidad crece 2 → 4 → 8 → 16 → 32 → 64 → 128 →
256 → 512 → 1024. Eso es **9 reallocs** para pasar de 2 a 1024 (que es ≥ 1000).

```
$\log_2(1000/2) = \log_2(500) \approx 8.97 → 9$ reallocs
```

De los 1000 pushes, solo 9 causan realloc. Los otros 991 son $O(1)$ estricto.
El costo amortizado es $O(1)$ por push.

Para verificar, puedes agregar un `printf` en `stack_grow`:

```c
static bool stack_grow(Stack *s) {
    size_t new_cap = s->capacity * 2;
    printf("  grow: %zu → %zu\n", s->capacity, new_cap);
    // ...
}
```

</details>

---

### Ejercicio 4 — Destroy con datos dinámicos

Modifica el stack para almacenar `char *` (strings). El problema: `stack_destroy`
necesita liberar cada string además del array y el struct.

```c
// Stack de strings
typedef struct {
    char **data;
    size_t top;
    size_t capacity;
} StringStack;
```

Implementa `create`, `push` (que haga `strdup`), `pop`, y `destroy`
(que libere todas las strings restantes).

**Prediccion**: Si haces pop de algunos elementos antes de destroy, las
strings popeadas también se liberan en destroy?

<details><summary>Respuesta</summary>

```c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    char **data;
    size_t top;
    size_t capacity;
} StringStack;

StringStack *sstack_create(size_t capacity) {
    StringStack *s = malloc(sizeof(StringStack));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(char *));
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    return s;
}

bool sstack_push(StringStack *s, const char *str) {
    if (s->top >= s->capacity) return false;
    s->data[s->top] = strdup(str);   // copia el string
    if (!s->data[s->top]) return false;
    s->top++;
    return true;
}

// Pop retorna ownership del string al llamador
char *sstack_pop(StringStack *s) {
    if (s->top == 0) return NULL;
    return s->data[--s->top];   // el llamador debe hacer free
}

void sstack_destroy(StringStack *s) {
    if (!s) return;
    // Liberar strings restantes (las que no se poppearon)
    for (size_t i = 0; i < s->top; i++) {
        free(s->data[i]);
    }
    free(s->data);
    free(s);
}

int main(void) {
    StringStack *s = sstack_create(10);

    sstack_push(s, "hello");
    sstack_push(s, "world");
    sstack_push(s, "foo");

    char *popped = sstack_pop(s);
    printf("popped: %s\n", popped);   // foo
    free(popped);   // el llamador libera

    // destroy libera "hello" y "world" (las 2 restantes)
    sstack_destroy(s);
    return 0;
}
```

No, las strings popeadas **no se liberan en destroy**. `pop` retorna el
puntero al string y transfiere el ownership al llamador. Es responsabilidad
del llamador hacer `free(popped)`.

`destroy` solo libera las strings que **aún están en el stack** (desde
`data[0]` hasta `data[top-1]`). Las strings ya popeadas están por encima
de `top` y no se recorren.

Si el llamador olvida hacer `free(popped)`, eso es un leak — pero no del
stack, sino del código que llamó a `pop`.

Valgrind verifica que cada `strdup` (que internamente hace `malloc`) tenga
su `free` correspondiente.

</details>

---

### Ejercicio 5 — Comparar convenciones de top

Implementa el mismo stack con la convención B (`top = -1` cuando vacío,
`top` = índice del último elemento). Compara `push` y `pop` con la
convención A:

```c
typedef struct {
    int *data;
    int top;            // -1 cuando vacío
    size_t capacity;
} StackB;
```

**Prediccion**: En la convención B, push usa pre-incremento o post-incremento?

<details><summary>Respuesta</summary>

```c
// Convención B: top = índice del último elemento, -1 si vacío

typedef struct {
    int *data;
    int top;            // -1 = vacío
    size_t capacity;
} StackB;

StackB *stackb_create(size_t capacity) {
    StackB *s = malloc(sizeof(StackB));
    if (!s) return NULL;
    s->data = malloc(capacity * sizeof(int));
    if (!s->data) { free(s); return NULL; }
    s->top = -1;          // ← diferencia: -1 en vez de 0
    s->capacity = capacity;
    return s;
}

bool stackb_push(StackB *s, int elem) {
    if ((size_t)(s->top + 1) >= s->capacity) return false;
    s->data[++s->top] = elem;     // ← pre-incremento: avanza y luego escribe
    return true;
}

bool stackb_pop(StackB *s, int *out) {
    if (s->top < 0) return false;
    *out = s->data[s->top--];     // ← post-decremento: lee y luego retrocede
    return true;
}

bool stackb_is_empty(const StackB *s) {
    return s->top < 0;
}

size_t stackb_size(const StackB *s) {
    return (size_t)(s->top + 1);
}
```

Comparación:

| Aspecto | Convención A (top = size) | Convención B (top = last index) |
|---------|-------------------------|-------------------------------|
| Vacío | `top == 0` | `top == -1` |
| Push | `data[top++]` (post-incr) | `data[++top]` (pre-incr) |
| Pop | `data[--top]` (pre-decr) | `data[top--]` (post-decr) |
| Size | `top` directamente | `top + 1` |
| Tipo de top | `size_t` (siempre ≥ 0) | `int` (necesita -1) |
| Peek | `data[top - 1]` | `data[top]` |

En la convención B, push usa **pre-incremento** (`++top`): primero avanza
`top` de -1 a 0 (o de la posición anterior a la siguiente), y luego escribe.

La convención A es preferible porque:
- `top` como `size_t` no puede ser negativo (más seguro)
- `top` coincide con `size()` (más intuitivo)
- No necesita la comparación `top < 0` con un tipo signed

</details>

---

### Ejercicio 6 — Stack genérico con void *

Implementa el stack genérico de la sección 8 y pruébalo con tres tipos
distintos: `int`, `double`, y un struct `Point {int x, y;}`.

```c
int main(void) {
    // Stack de int
    GenericStack *si = gstack_create(10, sizeof(int));
    int a = 42;
    gstack_push(si, &a);
    int b;
    gstack_pop(si, &b);
    printf("int: %d\n", b);    // 42

    // Stack de double
    // ...

    // Stack de Point
    // ...

    return 0;
}
```

**Prediccion**: Si accidentalmente creas un stack con `sizeof(int)` pero
haces push de un `double`, qué pasa?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    void *data;
    size_t top;
    size_t capacity;
    size_t elem_size;
} GenericStack;

GenericStack *gstack_create(size_t capacity, size_t elem_size) {
    GenericStack *s = malloc(sizeof(GenericStack));
    if (!s) return NULL;
    s->data = malloc(capacity * elem_size);
    if (!s->data) { free(s); return NULL; }
    s->top = 0;
    s->capacity = capacity;
    s->elem_size = elem_size;
    return s;
}

void gstack_destroy(GenericStack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

bool gstack_push(GenericStack *s, const void *elem) {
    if (s->top >= s->capacity) return false;
    void *dest = (char *)s->data + s->top * s->elem_size;
    memcpy(dest, elem, s->elem_size);
    s->top++;
    return true;
}

bool gstack_pop(GenericStack *s, void *out) {
    if (s->top == 0) return false;
    s->top--;
    void *src = (char *)s->data + s->top * s->elem_size;
    memcpy(out, src, s->elem_size);
    return true;
}

typedef struct { int x, y; } Point;

int main(void) {
    // Stack de int
    GenericStack *si = gstack_create(10, sizeof(int));
    int a = 42;
    gstack_push(si, &a);
    int b;
    gstack_pop(si, &b);
    printf("int: %d\n", b);          // 42

    // Stack de double
    GenericStack *sd = gstack_create(10, sizeof(double));
    double pi = 3.14159;
    gstack_push(sd, &pi);
    double d;
    gstack_pop(sd, &d);
    printf("double: %.5f\n", d);     // 3.14159

    // Stack de Point
    GenericStack *sp = gstack_create(10, sizeof(Point));
    Point p1 = {10, 20};
    Point p2 = {30, 40};
    gstack_push(sp, &p1);
    gstack_push(sp, &p2);
    Point out;
    gstack_pop(sp, &out);
    printf("point: (%d, %d)\n", out.x, out.y);  // (30, 40)
    gstack_pop(sp, &out);
    printf("point: (%d, %d)\n", out.x, out.y);  // (10, 20)

    gstack_destroy(si);
    gstack_destroy(sd);
    gstack_destroy(sp);
    return 0;
}
```

Si creas con `sizeof(int)` (4 bytes) pero haces push de un `double` (8 bytes):

```c
GenericStack *bad = gstack_create(10, sizeof(int));  // elem_size = 4
double pi = 3.14159;
gstack_push(bad, &pi);   // memcpy copia solo 4 bytes de un double de 8
```

Lo que pasa:
- `memcpy` copia solo los primeros 4 bytes del `double` — trunca la
  representación IEEE 754
- Al hacer pop con `double out`, `memcpy` lee 4 bytes y deja los otros 4
  con basura
- El valor recuperado es **incorrecto** — no es el `double` original
- No hay crash, no hay warning — es un **bug silencioso**

Este es el problema fundamental de `void *` en C: no hay type safety. En
Rust, `Stack<T>` hace imposible mezclar tipos porque el compilador lo verifica.

</details>

---

### Ejercicio 7 — Múltiples frees

Predice qué error detecta Valgrind con este código:

```c
int main(void) {
    Stack *s = stack_create(10);
    stack_push(s, 42);
    stack_destroy(s);
    stack_destroy(s);   // segunda vez
    return 0;
}
```

**Prediccion**: Es un double free, use-after-free, o ambos?

<details><summary>Respuesta</summary>

Es **ambos**:

1. **Use-after-free**: la segunda llamada a `stack_destroy(s)` accede a `s`,
   que ya fue liberado. Al ejecutar `free(s->data)`, accede a `s->data`
   dentro de memoria ya liberada.

2. **Double free**: si el primer `free(s->data)` liberó la memoria del array,
   y el segundo `stack_destroy` intenta `free(s->data)` otra vez, es un
   double free. Lo mismo para `free(s)`.

Valgrind reporta:

```
==12345== Invalid read of size 8
==12345==    at 0x...: stack_destroy (stack.c:22)
==12345==  Address 0x... is 0 bytes inside a block of size 24 free'd
==12345==    at 0x...: free (vg_replace_malloc.c:...)
==12345==    by 0x...: stack_destroy (stack.c:23)
==12345==
==12345== Invalid free() / delete / delete[]
==12345==    at 0x...: free (vg_replace_malloc.c:...)
==12345==    by 0x...: stack_destroy (stack.c:22)
==12345==  Address 0x... is 0 bytes inside a block of size 40 free'd
```

Solución: establecer `s = NULL` después de destroy, y verificar null al inicio:

```c
void stack_destroy(Stack *s) {
    if (!s) return;    // protege contra doble destroy
    free(s->data);
    free(s);
}

// En main:
stack_destroy(s);
s = NULL;          // previene uso accidental
stack_destroy(s);  // ahora es seguro (if (!s) return)
```

</details>

---

### Ejercicio 8 — Shrink automático

Implementa una función `stack_shrink` que reduzca la capacidad a la mitad
cuando el stack tiene menos del 25% de ocupación. Llámala dentro de `pop`:

```c
static void stack_shrink(Stack *s) {
    // Solo reducir si:
    // 1. top < capacity / 4
    // 2. capacity > capacidad mínima inicial (e.g., 4)
    // Tu código aquí
}
```

**Prediccion**: Si shrinkeas siempre que `top < capacity / 2` y groweas siempre
que `top >= capacity`, puedes crear un caso de thrashing (grow-shrink-grow)?

<details><summary>Respuesta</summary>

```c
#define MIN_CAPACITY 4

static void stack_shrink(Stack *s) {
    if (s->capacity <= MIN_CAPACITY) return;     // no reducir debajo del mínimo
    if (s->top >= s->capacity / 4) return;       // más del 25% ocupado, no reducir

    size_t new_cap = s->capacity / 2;
    if (new_cap < MIN_CAPACITY) new_cap = MIN_CAPACITY;

    int *new_data = realloc(s->data, new_cap * sizeof(int));
    if (!new_data) return;  // si falla, quedarse con el tamaño actual

    s->data = new_data;
    s->capacity = new_cap;
}

bool stack_pop(Stack *s, int *out) {
    if (s->top == 0) return false;
    *out = s->data[--s->top];
    stack_shrink(s);
    return true;
}
```

Sobre el thrashing: **sí**, si usas 50% como umbral de shrink:

```
capacity = 8, top = 8  → push → grow a 16
capacity = 16, top = 8 → pop  → top = 7, 7 < 16/2 → shrink a 8
capacity = 8, top = 7  → push → top = 8, 8 >= 8 → grow a 16
capacity = 16, top = 8 → pop  → shrink a 8
... ciclo infinito de grow/shrink
```

La solución es usar **umbrales asimétricos**:
- Grow cuando `top >= capacity` (100%)
- Shrink cuando `top < capacity / 4` (25%)

Con estos umbrales, después de un shrink (capacity / 2), necesitas llenar
hasta capacity antes de que ocurra un grow. Eso son capacity / 2 pushes
entre cada grow/shrink, lo que mantiene el costo amortizado $O(1)$.

La regla general: el umbral de shrink debe ser **al menos la mitad** del
factor de crecimiento. Si creces ×2, shrink debe ser al ≤ 25%.

</details>

---

### Ejercicio 9 — Print stack

Implementa una función `stack_print` que imprima el contenido del stack
de bottom a top, sin modificar el stack:

```c
void stack_print(const Stack *s);
// Ejemplo: stack_print → [10, 20, 30] (top = 30)
```

**Prediccion**: Puedes recorrer el array directamente sin hacer pop? Por qué
o por qué no con la implementación con array?

<details><summary>Respuesta</summary>

```c
void stack_print(const Stack *s) {
    printf("[");
    for (size_t i = 0; i < s->top; i++) {
        if (i > 0) printf(", ");
        printf("%d", s->data[i]);
    }
    printf("]");
    if (s->top > 0) {
        printf(" (top = %d)", s->data[s->top - 1]);
    }
    printf("\n");
}
```

Uso:

```c
Stack *s = stack_create(10);
stack_push(s, 10);
stack_push(s, 20);
stack_push(s, 30);
stack_print(s);   // [10, 20, 30] (top = 30)
```

Sí, **puedes recorrer directamente** sin hacer pop. La implementación con
array permite acceso por índice a cualquier posición — `data[0]` hasta
`data[top-1]`. Esto es una ventaja del array sobre la lista enlazada.

Sin embargo, esta función **rompe la abstracción del TAD**. Un stack
como TAD solo permite acceso al tope. `stack_print` necesita acceso interno
al array, por eso:

1. Se define en `stack.c` (donde conoce la estructura interna)
2. El header expone solo la firma `void stack_print(const Stack *s)`
3. El usuario la ve como "print the whole stack" sin saber que internamente
   recorre un array

Si la implementación fuera una lista enlazada, `stack_print` recorrería
nodos en vez de índices — misma interfaz, distinta implementación.

Nota: recibe `const Stack *s` porque no modifica el stack.

</details>

---

### Ejercicio 10 — Test exhaustivo

Escribe un programa que haga un test exhaustivo del stack:
1. Push 1000 elementos (valores 0 a 999)
2. Verifica que `size` = 1000
3. Peek debe retornar 999
4. Pop los 1000 elementos y verifica que salen en orden inverso
5. Verifica que está vacío
6. Verifica que pop en vacío retorna `false`
7. Verifica con Valgrind

```c
int main(void) {
    Stack *s = stack_create(1000);
    // Tu código aquí
    stack_destroy(s);
    return 0;
}
```

**Prediccion**: Si un bug causa que pop retorne los valores en el orden incorrecto
(FIFO en vez de LIFO), en qué elemento lo detectará tu test?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <assert.h>
#include "stack.h"

int main(void) {
    Stack *s = stack_create(1000);
    assert(s != NULL);

    // 1. Push 1000 elementos
    for (int i = 0; i < 1000; i++) {
        bool ok = stack_push(s, i);
        assert(ok);
    }

    // 2. Size = 1000
    assert(stack_size(s) == 1000);

    // 3. Peek = 999 (último pushado)
    int val;
    assert(stack_peek(s, &val));
    assert(val == 999);

    // 4. Pop en orden inverso
    for (int i = 999; i >= 0; i--) {
        assert(stack_pop(s, &val));
        if (val != i) {
            fprintf(stderr, "FAIL: expected %d, got %d\n", i, val);
            stack_destroy(s);
            return 1;
        }
    }

    // 5. Vacío
    assert(stack_is_empty(s));
    assert(stack_size(s) == 0);

    // 6. Pop en vacío retorna false
    assert(!stack_pop(s, &val));
    assert(!stack_peek(s, &val));

    stack_destroy(s);
    printf("All 1000-element tests passed!\n");
    return 0;
}
```

```bash
gcc -g -Wall -Wextra -std=c11 -o stack_test stack.c test.c
valgrind --leak-check=full ./stack_test
```

Si el stack retornara en orden FIFO (0, 1, 2, ...) en vez de LIFO (999, 998, ...),
el test lo detectaría en el **primer pop**: espera 999 pero recibe 0.

```
FAIL: expected 999, got 0
```

El loop `for (int i = 999; i >= 0; i--)` verifica que cada elemento sale
exactamente en el orden inverso al que entró. Cualquier desviación se detecta
inmediatamente.

</details>
