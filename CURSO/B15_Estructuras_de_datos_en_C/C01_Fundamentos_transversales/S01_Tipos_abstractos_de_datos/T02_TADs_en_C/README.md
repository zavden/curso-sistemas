# T02 — TADs en C

---

## 1. El problema: C no tiene clases

C no tiene `class`, `private`, `public` ni `interface`. Tiene structs con todos
los campos visibles, funciones sueltas, y headers. Sin embargo, con tres
mecanismos se puede implementar una barrera de abstracción completa:

```
Mecanismo C              Equivalente OOP       Rol en el TAD
─────────────────────────────────────────────────────────────
Header (.h)              Interfaz / API        Contrato público
Archivo fuente (.c)      Implementación        Detalles ocultos
Struct opaco             Campos privados       Encapsulación
```

El resultado es un TAD con la misma separación interfaz/implementación que
verías en Java o Rust, pero usando las herramientas de C.

---

## 2. El header como interfaz pública

El header (`.h`) es lo único que el usuario del TAD necesita incluir.
Declara las operaciones disponibles sin revelar cómo funcionan.

### Anatomía de un header para un TAD

```c
// set.h — interfaz pública del TAD Conjunto
#ifndef SET_H
#define SET_H

#include <stdbool.h>
#include <stddef.h>

// Declaración adelantada — struct opaco
// El usuario sabe que existe "Set" pero no qué tiene dentro
typedef struct Set Set;

// Constructor
Set *set_create(void);

// Destructor
void set_destroy(Set *s);

// Modificadores
bool set_insert(Set *s, int elem);
bool set_remove(Set *s, int elem);

// Observadores
bool set_contains(const Set *s, int elem);
size_t set_size(const Set *s);
bool set_empty(const Set *s);

// Iteración
void set_print(const Set *s);

#endif // SET_H
```

### Lo que el header comunica

Sin leer una sola línea de implementación, el header ya dice:

| Elemento | Información que comunica |
|----------|--------------------------|
| `typedef struct Set Set` | Existe un tipo `Set`, pero su contenido es privado |
| `Set *set_create(void)` | Se crea por puntero (memoria dinámica), sin argumentos |
| `void set_destroy(Set *s)` | El usuario es responsable de liberar — hay recursos internos |
| `bool set_insert(...)` | Retorna bool — posiblemente indica si se insertó o ya existía |
| `const Set *s` | Los observadores no modifican el conjunto — `const` lo garantiza |
| Prefijo `set_` | Namespace manual — todas las funciones del TAD comparten prefijo |

### Include guards

```c
#ifndef SET_H
#define SET_H
// ...
#endif
```

Previenen inclusión múltiple. Si `main.c` incluye `set.h` y también incluye
`utils.h` que a su vez incluye `set.h`, sin guards habría redefiniciones.

Alternativa: `#pragma once` (no estándar pero soportado por GCC, Clang, MSVC).

---

## 3. Structs opacos — la clave de la encapsulación

### El patrón: declaración adelantada en el header

```c
// set.h
typedef struct Set Set;   // declaración adelantada
```

El compilador sabe que `Set` es un struct, pero **no conoce sus campos ni su
tamaño**. Esto tiene consecuencias fundamentales:

```c
// main.c — el usuario del TAD
#include "set.h"

int main(void) {
    Set *s = set_create();   // OK — maneja un puntero

    // s->data;              // ERROR — no sabe qué campos tiene
    // Set local;            // ERROR — no conoce el tamaño
    // sizeof(Set);          // ERROR — tamaño desconocido

    set_insert(s, 42);       // OK — usa la interfaz pública
    set_destroy(s);          // OK
    return 0;
}
```

El usuario **solo puede** trabajar con punteros a `Set` y llamar a las funciones
del header. No puede:
- Acceder a campos internos
- Crear instancias en el stack (no conoce el tamaño)
- Copiar con `=` (no conoce el tamaño)
- Modificar la representación interna

Esto es encapsulación total — equivalente a `private` en otros lenguajes.

### La definición completa: solo en el `.c`

```c
// set.c — implementación privada
#include "set.h"
#include <stdlib.h>
#include <stdio.h>

// Definición completa — solo visible en este archivo
struct Set {
    int *data;       // array dinámico de elementos
    size_t size;     // elementos actuales
    size_t capacity; // capacidad del array
};
```

Solo `set.c` (y cualquier otro `.c` que defina el struct) conoce los campos.
El resto del programa trabaja con punteros opacos.

### ¿Por qué no se puede crear en el stack?

Para alocar una variable en el stack, el compilador necesita conocer su tamaño
en tiempo de compilación:

```c
// stack allocation requiere sizeof
int x;              // compilador sabe: sizeof(int) == 4
double arr[10];     // compilador sabe: sizeof(double) * 10 == 80

Set s;              // ERROR: sizeof(Set) desconocido — el compilador no
                    // sabe cuántos bytes reservar en el stack frame
```

Por eso los TADs opacos siempre se manejan con punteros (`Set *`), alocados
en el heap con `malloc`.

---

## 4. Implementación completa: TAD Conjunto

### set.h (interfaz)

```c
#ifndef SET_H
#define SET_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Set Set;

// Lifecycle
Set  *set_create(void);
Set  *set_clone(const Set *s);
void  set_destroy(Set *s);

// Modifiers
bool  set_insert(Set *s, int elem);
bool  set_remove(Set *s, int elem);
void  set_clear(Set *s);

// Observers
bool   set_contains(const Set *s, int elem);
size_t set_size(const Set *s);
bool   set_empty(const Set *s);

// Output
void   set_print(const Set *s);

#endif // SET_H
```

### set.c (implementación)

```c
#include "set.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define SET_INIT_CAP 8

struct Set {
    int    *data;
    size_t  size;
    size_t  capacity;
};

// ---------- helpers (static = privadas al archivo) ----------

static int set_find(const Set *s, int elem) {
    for (size_t i = 0; i < s->size; i++) {
        if (s->data[i] == elem) return (int)i;
    }
    return -1;
}

static bool set_grow(Set *s) {
    size_t new_cap = s->capacity * 2;
    int *new_data = realloc(s->data, new_cap * sizeof(int));
    if (!new_data) return false;
    s->data     = new_data;
    s->capacity = new_cap;
    return true;
}

// ---------- lifecycle ----------

Set *set_create(void) {
    Set *s = malloc(sizeof(Set));       // solo aquí se conoce sizeof(Set)
    if (!s) return NULL;

    s->data = malloc(SET_INIT_CAP * sizeof(int));
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->size     = 0;
    s->capacity = SET_INIT_CAP;
    return s;
}

Set *set_clone(const Set *s) {
    assert(s != NULL);

    Set *copy = set_create();
    if (!copy) return NULL;

    for (size_t i = 0; i < s->size; i++) {
        if (!set_insert(copy, s->data[i])) {
            set_destroy(copy);
            return NULL;
        }
    }
    return copy;
}

void set_destroy(Set *s) {
    if (!s) return;         // destroy(NULL) es seguro — convención
    free(s->data);
    free(s);
}

// ---------- modifiers ----------

bool set_insert(Set *s, int elem) {
    assert(s != NULL);

    if (set_find(s, elem) >= 0) return true;  // ya existe — no duplicar

    if (s->size == s->capacity) {
        if (!set_grow(s)) return false;        // sin memoria
    }

    s->data[s->size] = elem;
    s->size++;
    return true;
}

bool set_remove(Set *s, int elem) {
    assert(s != NULL);

    int idx = set_find(s, elem);
    if (idx < 0) return false;     // no existe

    // Swap con último — O(1) eliminación (el orden no importa en un conjunto)
    s->data[idx] = s->data[s->size - 1];
    s->size--;
    return true;
}

void set_clear(Set *s) {
    assert(s != NULL);
    s->size = 0;   // no libera memoria — reutiliza el buffer
}

// ---------- observers ----------

bool set_contains(const Set *s, int elem) {
    assert(s != NULL);
    return set_find(s, elem) >= 0;
}

size_t set_size(const Set *s) {
    assert(s != NULL);
    return s->size;
}

bool set_empty(const Set *s) {
    assert(s != NULL);
    return s->size == 0;
}

// ---------- output ----------

void set_print(const Set *s) {
    assert(s != NULL);
    printf("{");
    for (size_t i = 0; i < s->size; i++) {
        if (i > 0) printf(", ");
        printf("%d", s->data[i]);
    }
    printf("}\n");
}
```

### main.c (usuario del TAD)

```c
#include "set.h"
#include <stdio.h>

int main(void) {
    Set *s = set_create();
    if (!s) {
        fprintf(stderr, "Error: no memory\n");
        return 1;
    }

    set_insert(s, 10);
    set_insert(s, 20);
    set_insert(s, 30);
    set_insert(s, 20);    // duplicado — no se agrega

    printf("size: %zu\n", set_size(s));   // 3
    printf("contains 20: %d\n", set_contains(s, 20));  // 1
    printf("contains 99: %d\n", set_contains(s, 99));  // 0

    set_print(s);         // {10, 20, 30}

    set_remove(s, 20);
    set_print(s);         // {10, 30}

    set_destroy(s);
    return 0;
}
```

### Compilación

```bash
gcc -Wall -Wextra -std=c11 -c set.c -o set.o
gcc -Wall -Wextra -std=c11 -c main.c -o main.o
gcc set.o main.o -o set_demo
./set_demo
```

`main.c` solo incluye `set.h`. No tiene acceso a los campos de `struct Set`.
Si cambias la representación interna (ej: de array a lista enlazada), solo
recompilas `set.c` — `main.c` no cambia.

---

## 5. Convenciones de nombres

C no tiene namespaces. Para evitar colisiones, se usa un **prefijo** consistente:

```c
// TAD Stack
Stack *stack_create(void);
void   stack_destroy(Stack *s);
void   stack_push(Stack *s, int elem);
int    stack_pop(Stack *s);
int    stack_top(const Stack *s);
bool   stack_empty(const Stack *s);

// TAD Queue
Queue *queue_create(void);
void   queue_destroy(Queue *q);
void   queue_enqueue(Queue *q, int elem);
int    queue_dequeue(Queue *q);
int    queue_front(const Queue *q);
bool   queue_empty(const Queue *q);
```

### El patrón `prefijo_operación`

```
nombre_del_tad + _ + operación

set_create      stack_push      queue_enqueue
set_destroy     stack_pop       queue_dequeue
set_insert      stack_top       queue_front
set_contains    stack_empty     queue_empty
```

Esto simula métodos: `set_insert(s, 42)` es el equivalente C de `s.insert(42)`.
El primer argumento siempre es el "self" — el puntero al TAD.

### ¿Por qué no `create_set`, `destroy_set`?

Con `set_create`, `set_destroy`, `set_insert`, todas las funciones del TAD
aparecen juntas en autocompletado y documentación (están ordenadas
alfabéticamente por prefijo). Con `create_set`, `destroy_set`, `insert_set`,
se dispersan.

### static para funciones privadas

Las funciones auxiliares internas al TAD deben ser `static`:

```c
// set.c
static int  set_find(const Set *s, int elem);  // NO visible fuera de set.c
static bool set_grow(Set *s);                   // NO visible fuera de set.c
```

`static` en funciones (file scope) significa "enlace interno" — la función
solo existe dentro de esa unidad de compilación. Esto es el equivalente
de `private` para métodos auxiliares.

Sin `static`, si otro archivo define su propia función `set_find`, habrá
un error de enlace (símbolo duplicado).

---

## 6. Patrones de manejo de errores en TADs

### 6.1 Constructor que puede fallar

`malloc` puede retornar `NULL`. El constructor debe manejarlo:

```c
Set *set_create(void) {
    Set *s = malloc(sizeof(Set));
    if (!s) return NULL;              // fallo en struct

    s->data = malloc(INIT_CAP * sizeof(int));
    if (!s->data) {
        free(s);                      // limpiar lo ya alocado
        return NULL;
    }

    s->size = 0;
    s->capacity = INIT_CAP;
    return s;
}
```

El patrón crítico: si la segunda alocación falla, **liberar la primera**.
Sin esto, hay un memory leak.

El llamador verifica:

```c
Set *s = set_create();
if (!s) {
    fprintf(stderr, "Error: out of memory\n");
    exit(EXIT_FAILURE);
}
```

### 6.2 Destructor NULL-safe

Convención: `destroy(NULL)` no hace nada (igual que `free(NULL)`):

```c
void set_destroy(Set *s) {
    if (!s) return;
    free(s->data);
    free(s);
}
```

Esto simplifica el código de limpieza:

```c
// Sin NULL-safe:
if (a) set_destroy(a);
if (b) set_destroy(b);

// Con NULL-safe:
set_destroy(a);
set_destroy(b);
```

### 6.3 Retorno booleano para operaciones que pueden fallar

```c
bool set_insert(Set *s, int elem) {
    // ...
    if (needs_grow && !set_grow(s)) return false;  // fallo
    // ...
    return true;  // éxito
}
```

`true` = operación exitosa, `false` = fallo (típicamente falta de memoria).

### 6.4 Assert para precondiciones en debug

```c
#include <assert.h>

int stack_pop(Stack *s) {
    assert(s != NULL);            // precondición: puntero válido
    assert(!stack_empty(s));      // precondición: no vacía
    s->size--;
    return s->data[s->size];
}
```

`assert` se elimina con `-DNDEBUG`:

```bash
# Debug — asserts activos:
gcc -Wall -std=c11 -g -o prog prog.c stack.c

# Release — asserts eliminados:
gcc -Wall -std=c11 -O2 -DNDEBUG -o prog prog.c stack.c
```

---

## 7. El destructor y la responsabilidad de liberación

### Regla fundamental

> Quien crea, destruye.

El código que llama a `set_create()` es responsable de llamar a `set_destroy()`.

### Destrucción en orden inverso

Si un TAD contiene otro TAD, el destructor debe liberar en orden inverso a
la construcción:

```c
struct Graph {
    Set **adj;        // array de punteros a Set (lista de adyacencia)
    size_t vertices;
};

void graph_destroy(Graph *g) {
    if (!g) return;
    for (size_t i = 0; i < g->vertices; i++) {
        set_destroy(g->adj[i]);   // primero: liberar cada Set
    }
    free(g->adj);                  // segundo: liberar el array de punteros
    free(g);                       // tercero: liberar el struct
}
```

Si liberas `g` antes que `g->adj`, ya no puedes acceder a `g->adj` → use-after-free.
Si liberas `g->adj` antes que los Sets, los Sets se vuelven inalcanzables → memory leak.

### Patrón para errores durante la construcción

Cuando un constructor complejo falla a mitad, debe limpiar todo lo que ya alocó:

```c
Graph *graph_create(size_t vertices) {
    Graph *g = malloc(sizeof(Graph));
    if (!g) return NULL;

    g->vertices = vertices;
    g->adj = malloc(vertices * sizeof(Set *));
    if (!g->adj) {
        free(g);
        return NULL;
    }

    for (size_t i = 0; i < vertices; i++) {
        g->adj[i] = set_create();
        if (!g->adj[i]) {
            // Limpiar todo lo ya construido
            for (size_t j = 0; j < i; j++) {
                set_destroy(g->adj[j]);
            }
            free(g->adj);
            free(g);
            return NULL;
        }
    }

    return g;
}
```

---

## 8. Struct opaco vs struct expuesto

No todos los TADs en C usan structs opacos. Hay dos estilos:

### Estilo opaco (recomendado para TADs)

```c
// stack.h
typedef struct Stack Stack;
Stack *stack_create(void);
void   stack_push(Stack *s, int elem);
// ...
```

**Ventajas**:
- Encapsulación total — el usuario no puede romper invariantes
- Cambiar la implementación no requiere recompilar el código cliente
- Claridad: la interfaz es solo el header

**Desventajas**:
- No se puede alocar en el stack (siempre `malloc`)
- Indirección obligatoria (siempre a través de puntero)
- No se puede copiar con `=` — necesitas `clone()`

### Estilo expuesto (para tipos simples o rendimiento)

```c
// vec2.h
typedef struct {
    float x;
    float y;
} Vec2;

Vec2 vec2_add(Vec2 a, Vec2 b);
float vec2_dot(Vec2 a, Vec2 b);
```

**Ventajas**:
- Se puede alocar en stack — sin `malloc`
- Se puede copiar con `=`
- Se puede pasar por valor — sin punteros
- Inline posible — el compilador ve todo

**Desventajas**:
- Sin encapsulación — el usuario accede a `.x`, `.y` directamente
- Cambiar campos requiere recompilar todo el código que incluye el header

### Cuándo usar cada uno

| Criterio | Opaco | Expuesto |
|----------|-------|----------|
| El tipo tiene invariantes complejas | Si | — |
| La implementación puede cambiar | Si | — |
| Rendimiento crítico (hot path, sin heap) | — | Si |
| Tipo simple sin invariantes (punto, color) | — | Si |
| Biblioteca pública con estabilidad ABI | Si | — |

Para las estructuras de datos de este curso (pilas, colas, listas, árboles,
hash tables, grafos), el estilo opaco es el apropiado — todas tienen
invariantes que proteger.

---

## 9. Compilación separada y la barrera

El sistema de compilación refuerza la barrera de abstracción:

```
      main.c                set.h                set.c
    ┌─────────┐          ┌─────────┐          ┌─────────┐
    │#include  │─────────▶│typedef  │◀─────────│#include  │
    │ "set.h"  │          │struct   │          │ "set.h"  │
    │          │          │Set Set; │          │          │
    │set_create│          │         │          │struct Set│
    │set_insert│          │set_*()  │          │{ data,  │
    │set_print │          │decls    │          │  size,  │
    │set_destro│          │         │          │  cap }  │
    └────┬─────┘          └─────────┘          └────┬────┘
         │                                          │
         ▼                                          ▼
      main.o                                     set.o
    ┌─────────┐                               ┌─────────┐
    │ llama a  │                               │ define   │
    │set_create│──────── enlazador ───────────▶│set_create│
    │set_insert│          (linker)             │set_insert│
    │etc.      │                               │etc.      │
    └─────────┘                               └─────────┘
         │                                          │
         └──────────────┬───────────────────────────┘
                        ▼
                    set_demo
                   (ejecutable)
```

`main.c` se compila **solo con el header**. No necesita ver `set.c`.
Si cambias `set.c` (ej: de array a lista enlazada), solo recompilas `set.c`:

```bash
# Solo recompilar la implementación:
gcc -c set.c -o set.o

# Re-enlazar:
gcc main.o set.o -o set_demo
```

`main.o` no se recompila. Esto es posible porque la interfaz (header) no cambió.
En proyectos grandes con decenas de TADs, esto ahorra minutos de compilación.

---

## 10. Ejemplo alterno: TAD Stack

Para reforzar el patrón, un segundo TAD completo:

### stack.h

```c
#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Stack Stack;

Stack *stack_create(void);
void   stack_destroy(Stack *s);

void   stack_push(Stack *s, int elem);
int    stack_pop(Stack *s);
int    stack_top(const Stack *s);

bool   stack_empty(const Stack *s);
size_t stack_size(const Stack *s);

void   stack_print(const Stack *s);

#endif // STACK_H
```

### stack.c

```c
#include "stack.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define STACK_INIT_CAP 16

struct Stack {
    int    *data;
    size_t  size;
    size_t  capacity;
};

Stack *stack_create(void) {
    Stack *s = malloc(sizeof(Stack));
    if (!s) return NULL;

    s->data = malloc(STACK_INIT_CAP * sizeof(int));
    if (!s->data) {
        free(s);
        return NULL;
    }

    s->size     = 0;
    s->capacity = STACK_INIT_CAP;
    return s;
}

void stack_destroy(Stack *s) {
    if (!s) return;
    free(s->data);
    free(s);
}

void stack_push(Stack *s, int elem) {
    assert(s != NULL);
    if (s->size == s->capacity) {
        size_t new_cap = s->capacity * 2;
        int *new_data = realloc(s->data, new_cap * sizeof(int));
        if (!new_data) {
            fprintf(stderr, "stack_push: out of memory\n");
            return;
        }
        s->data     = new_data;
        s->capacity = new_cap;
    }
    s->data[s->size] = elem;
    s->size++;
}

int stack_pop(Stack *s) {
    assert(s != NULL);
    assert(!stack_empty(s));   // precondición: no vacía
    s->size--;
    return s->data[s->size];
}

int stack_top(const Stack *s) {
    assert(s != NULL);
    assert(!stack_empty(s));
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

void stack_print(const Stack *s) {
    assert(s != NULL);
    printf("[");
    for (size_t i = 0; i < s->size; i++) {
        if (i > 0) printf(", ");
        printf("%d", s->data[i]);
    }
    printf("] <- top\n");
}
```

---

## Ejercicios

### Ejercicio 1 — Compilar el TAD Set

Crea los tres archivos (`set.h`, `set.c`, `main.c`) del ejemplo de la sección 4.
Compila y ejecuta:

```bash
gcc -Wall -Wextra -std=c11 -c set.c -o set.o
gcc -Wall -Wextra -std=c11 -c main.c -o main.o
gcc set.o main.o -o set_demo
./set_demo
```

**Predicción**: Antes de ejecutar, responde: ¿cuál será la salida exacta?
Recuerda que `set_insert(s, 20)` se llama dos veces y que `set_remove`
hace swap con el último.

<details><summary>Respuesta</summary>

```
size: 3
contains 20: 1
contains 99: 0
{10, 20, 30}
{10, 30}
```

Detalles:
- `insert(10)`, `insert(20)`, `insert(30)` → tamaño 3
- `insert(20)` de nuevo: `set_find` encuentra 20, retorna sin insertar → tamaño sigue en 3
- `contains(20)` → true (1), `contains(99)` → false (0)
- `print` → `{10, 20, 30}` (orden de inserción)
- `remove(20)`: swap con último → `data = [10, 30]`, size = 2
- `print` → `{10, 30}`

</details>

Limpieza:

```bash
rm -f set.o main.o set_demo
```

---

### Ejercicio 2 — Verificar opacidad

Agrega esta línea dentro de `main.c` (después de `set_create`):

```c
printf("capacity = %zu\n", s->capacity);
```

Intenta compilar.

**Predicción**: ¿Compilará? ¿Qué error exacto dará? ¿Por qué?

<details><summary>Respuesta</summary>

No compila. El error es:

```
main.c: error: dereferencing pointer to incomplete type 'Set' {aka 'struct Set'}
```

`main.c` solo ve la declaración adelantada `typedef struct Set Set` del header.
No conoce los campos del struct — es un "incomplete type". El compilador sabe
que `s` es un `Set *`, pero no puede desreferenciar porque no sabe qué hay
dentro.

Este error **es el objetivo**: la barrera de abstracción funciona. Si el usuario
necesita la capacidad, debe haber un `set_capacity()` en el header. Si no lo
hay, es información interna que el TAD no expone.

</details>

---

### Ejercicio 3 — Cambiar implementación sin cambiar main

Modifica `set.c` para usar una **lista enlazada** en vez de array dinámico,
sin cambiar `set.h` ni `main.c`:

```c
// Nuevo struct Set en set.c:
typedef struct SetNode {
    int data;
    struct SetNode *next;
} SetNode;

struct Set {
    SetNode *head;
    size_t   size;
};
```

Reimplementa `set_create`, `set_destroy`, `set_insert`, `set_remove`,
`set_contains`, `set_size`, `set_empty`, `set_print`.

Compila y ejecuta con el mismo `main.c`.

**Predicción**: ¿La salida de `main.c` será exactamente igual que con la
versión array? ¿En qué podría diferir?

<details><summary>Respuesta</summary>

La salida será **funcionalmente equivalente** pero puede diferir en el
**orden de impresión**:

Versión array: `{10, 20, 30}` (orden de inserción)

Versión lista (si se inserta al inicio): `{30, 20, 10}` (orden inverso)
Versión lista (si se inserta al final): `{10, 20, 30}` (mismo orden)

Ambas son correctas — el TAD "Conjunto" no garantiza orden. La postcondición
de `set_print` solo dice que imprime todos los elementos, no en qué orden.

Las operaciones booleanas (`contains`, `empty`) y numéricas (`size`) dan
exactamente el mismo resultado. `remove` se comporta igual (quita el elemento).

Este es el poder del struct opaco: la implementación cambió completamente
(de array a lista) pero `main.c` no se enteró. Solo hubo que recompilar
`set.c`.

Implementación clave de la versión lista:

```c
Set *set_create(void) {
    Set *s = malloc(sizeof(Set));
    if (!s) return NULL;
    s->head = NULL;
    s->size = 0;
    return s;
}

void set_destroy(Set *s) {
    if (!s) return;
    SetNode *cur = s->head;
    while (cur) {
        SetNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    free(s);
}

bool set_insert(Set *s, int elem) {
    assert(s != NULL);
    if (set_contains(s, elem)) return true;

    SetNode *node = malloc(sizeof(SetNode));
    if (!node) return false;
    node->data = elem;
    node->next = s->head;
    s->head = node;
    s->size++;
    return true;
}
```

</details>

---

### Ejercicio 4 — Agregar operación al TAD

Agrega la operación `set_union` que retorna un nuevo conjunto con los
elementos de ambos:

```c
// En set.h:
Set *set_union(const Set *a, const Set *b);
```

Implementa en `set.c` y testea en `main.c`:

```c
Set *a = set_create();
Set *b = set_create();
set_insert(a, 1); set_insert(a, 2); set_insert(a, 3);
set_insert(b, 2); set_insert(b, 3); set_insert(b, 4);

Set *u = set_union(a, b);
set_print(u);  // {1, 2, 3, 4} (algún orden)
printf("size: %zu\n", set_size(u));  // 4

set_destroy(u);
set_destroy(b);
set_destroy(a);
```

**Predicción**: ¿Cuántos `set_insert` se ejecutan internamente? ¿Cuántos
de esos son inserciones de duplicados que no modifican el conjunto?

<details><summary>Respuesta</summary>

```c
Set *set_union(const Set *a, const Set *b) {
    assert(a != NULL && b != NULL);

    Set *result = set_clone(a);    // copia todos los de a
    if (!result) return NULL;

    // Insertar todos los de b (set_insert ignora duplicados)
    for (size_t i = 0; i < b->size; i++) {
        set_insert(result, b->data[i]);
    }
    return result;
}
```

Inserciones internas:
- `set_clone(a)` ejecuta 3 inserciones: {1, 2, 3}
- Luego `set_insert` para cada elemento de b:
  - `insert(2)` → ya existe, no modifica (duplicado)
  - `insert(3)` → ya existe, no modifica (duplicado)
  - `insert(4)` → se agrega

Total: 6 llamadas a `set_insert`, de las cuales **2 son duplicados** que
no modifican el conjunto. El resultado tiene 4 elementos.

Nota: `set_union` accede a `b->data[i]` directamente. Esto funciona porque
está dentro de `set.c`, donde la definición de `struct Set` es visible.
Si `set_union` estuviera en otro archivo, necesitaría un iterador público.

</details>

---

### Ejercicio 5 — Memory leaks deliberados

Modifica `main.c` para introducir tres tipos de leak y verifica con Valgrind:

```c
// Leak 1: olvidar destroy
Set *s1 = set_create();
set_insert(s1, 42);
// no set_destroy(s1)

// Leak 2: sobrescribir puntero
Set *s2 = set_create();
s2 = set_create();   // el primer Set se pierde
set_destroy(s2);

// Leak 3: olvidar destroy del resultado
Set *a = set_create();
Set *b = set_create();
set_union(a, b);     // resultado no capturado
set_destroy(b);
set_destroy(a);
```

```bash
valgrind --leak-check=full ./set_demo
```

**Predicción**: ¿Cuántos bloques "definitely lost" reportará Valgrind?
¿Cuántos bytes por cada leak?

<details><summary>Respuesta</summary>

Valgrind reportará 3 fuentes de leak:

**Leak 1** (olvidar destroy): 2 bloques perdidos
- `malloc(sizeof(Set))` = probablemente 24 bytes (struct con puntero + 2 size_t)
- `malloc(8 * sizeof(int))` = 32 bytes (INIT_CAP = 8, sizeof(int) = 4)
- Total: ~56 bytes, 2 bloques definitely lost

**Leak 2** (sobrescribir puntero): 2 bloques perdidos
- El primer `set_create()` alocó struct + array
- Al hacer `s2 = set_create()` sin destroy, los 2 bloques del primer Set se pierden
- Total: ~56 bytes, 2 bloques definitely lost

**Leak 3** (resultado no capturado): 2 bloques perdidos
- `set_union` hace `set_create` internamente (struct + array)
- El retorno no se asigna a variable → no hay forma de liberar
- Total: ~56 bytes, 2 bloques definitely lost

Total esperado: **6 bloques definitely lost**, ~168 bytes.

Valgrind muestra exactamente dónde se alocó cada bloque perdido (en cuál
`malloc` dentro de `set_create`), lo que facilita encontrar cuál `destroy`
falta.

</details>

Limpieza:

```bash
rm -f set.o main.o set_demo
```

---

### Ejercicio 6 — Prefijos y colisiones de nombres

Supón que tienes dos TADs en el mismo proyecto: un `Set` de enteros y un
`StringSet` de strings. Ambos necesitan `create`, `destroy`, `insert`,
`contains`.

Muestra cómo se nombrarían las funciones de ambos TADs para evitar colisiones.
Luego muestra qué pasaría si ambos usaran el nombre `create` sin prefijo.

**Predicción**: ¿En qué fase de compilación se detecta la colisión:
preprocesado, compilación o enlace?

<details><summary>Respuesta</summary>

Nombres correctos:

```c
// set.h
Set *set_create(void);
void set_destroy(Set *s);
bool set_insert(Set *s, int elem);
bool set_contains(const Set *s, int elem);

// strset.h
StringSet *strset_create(void);
void       strset_destroy(StringSet *ss);
bool       strset_insert(StringSet *ss, const char *str);
bool       strset_contains(const StringSet *ss, const char *str);
```

Sin prefijo:

```c
// set.h
Set *create(void);
// strset.h
StringSet *create(void);
```

La colisión se detecta en la fase de **enlace** (linker), no de compilación.

- `set.c` compila bien — define `create` que retorna `Set *`
- `strset.c` compila bien — define `create` que retorna `StringSet *`
- Cada `.c` se compila como unidad independiente, sin conflicto

Pero al enlazar:

```
ld: error: multiple definition of `create'
  set.o: first defined here
  strset.o: ... also defined here
```

El linker ve dos símbolos globales con el mismo nombre `create` y no sabe
cuál usar. El prefijo (`set_create` vs `strset_create`) genera símbolos
distintos y elimina la ambigüedad.

</details>

---

### Ejercicio 7 — const correctness en observadores

Observa estas firmas y determina cuáles son correctas para funciones
observadoras (que no modifican el TAD):

```c
// Versión A
bool set_contains(Set *s, int elem);

// Versión B
bool set_contains(const Set *s, int elem);

// Versión C
size_t set_size(Set *s);

// Versión D
size_t set_size(const Set *s);

// Versión E
void set_print(const Set *s);

// Versión F
Set *set_clone(Set *s);
```

**Predicción**: ¿Cuáles usan `const` correctamente? ¿Cuál es el riesgo
de NO usar `const` en un observador?

<details><summary>Respuesta</summary>

| Versión | ¿Correcta? | Razón |
|---------|------------|-------|
| A | No | `set_contains` es observador — debería ser `const Set *` |
| B | **Sí** | Correcta — promete no modificar el conjunto |
| C | No | `set_size` es observador — debería ser `const Set *` |
| D | **Sí** | Correcta |
| E | **Sí** | Correcta — imprimir no modifica |
| F | Depende | `set_clone` no modifica el original → `const Set *s` es correcto. Pero la versión sin `const` también funciona (solo pierde documentación) |

Riesgos de no usar `const`:

1. **Documentación perdida**: la firma no comunica que la función es segura
   (no modifica estado). El usuario tiene que leer la implementación.

2. **Compilación rechazada con punteros const**:

```c
void process(const Set *s) {
    // s es const — promesa de no modificar
    set_contains(s, 42);     // ERROR si set_contains toma Set* (no const)
                              // OK si toma const Set*
}
```

Si `set_contains` no toma `const`, no se puede llamar con un `const Set *`.
Esto rompe la composición y obliga al usuario a hacer cast inseguro.

3. **Bug silencioso**: sin `const`, un observador podría accidentalmente
   modificar estado interno (ej: un campo de caché) y el compilador no
   advertiría.

</details>

---

### Ejercicio 8 — Destrucción en orden inverso

Dado este código:

```c
typedef struct GraphNode {
    int id;
    Set *neighbors;   // TAD Set para vecinos
} GraphNode;

typedef struct Graph {
    GraphNode *nodes;
    size_t count;
};
```

Escribe `graph_destroy` que libere todo sin leaks ni use-after-free.
Luego muestra qué pasa si se libera en el orden incorrecto.

**Predicción**: ¿Cuántas llamadas a `free` / `set_destroy` necesitas?
¿Cuál es el orden correcto?

<details><summary>Respuesta</summary>

```c
void graph_destroy(Graph *g) {
    if (!g) return;

    // 1. Primero: destruir cada Set dentro de cada nodo
    for (size_t i = 0; i < g->count; i++) {
        set_destroy(g->nodes[i].neighbors);
    }

    // 2. Segundo: liberar el array de nodos
    free(g->nodes);

    // 3. Tercero: liberar el struct Graph
    free(g);
}
```

Para un grafo con `n` nodos: `n` llamadas a `set_destroy` + 1 `free(nodes)` + 1 `free(g)`.

**Orden incorrecto — ejemplo 1** (liberar `g` primero):

```c
void graph_destroy_WRONG1(Graph *g) {
    free(g);                           // g liberado
    for (size_t i = 0; i < g->count; i++) {  // USE-AFTER-FREE: g->count
        set_destroy(g->nodes[i].neighbors);   // USE-AFTER-FREE: g->nodes
    }
}
```

Acceder a `g->count` y `g->nodes` después de `free(g)` es use-after-free.

**Orden incorrecto — ejemplo 2** (liberar `nodes` antes que los Sets):

```c
void graph_destroy_WRONG2(Graph *g) {
    free(g->nodes);                          // nodes liberado
    for (size_t i = 0; i < g->count; i++) {
        set_destroy(g->nodes[i].neighbors);  // USE-AFTER-FREE: g->nodes[i]
    }
    free(g);
}
```

Después de `free(g->nodes)`, acceder a `g->nodes[i]` es use-after-free.

La regla: liberar **de adentro hacia afuera** — primero lo más profundo
(Sets), luego el contenedor (array de nodos), luego el struct raíz.

</details>

---

### Ejercicio 9 — TAD con múltiples alocaciones

Implementa un TAD "Pair" que almacena dos strings (copias propias):

```c
// pair.h
typedef struct Pair Pair;

Pair       *pair_create(const char *first, const char *second);
void        pair_destroy(Pair *p);
const char *pair_first(const Pair *p);
const char *pair_second(const Pair *p);
```

La implementación debe hacer `strdup` de ambos strings. El destructor
debe liberar todo.

**Predicción**: Si `strdup` del segundo string falla (retorna NULL),
¿qué debe hacer el constructor? ¿Cuántos `free` necesitas en ese
caso de error?

<details><summary>Respuesta</summary>

```c
// pair.c
#include "pair.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct Pair {
    char *first;
    char *second;
};

Pair *pair_create(const char *first, const char *second) {
    Pair *p = malloc(sizeof(Pair));
    if (!p) return NULL;

    p->first = strdup(first);
    if (!p->first) {
        free(p);                   // liberar 1: el struct
        return NULL;
    }

    p->second = strdup(second);
    if (!p->second) {
        free(p->first);            // liberar 1: el primer string
        free(p);                   // liberar 2: el struct
        return NULL;
    }

    return p;
}

void pair_destroy(Pair *p) {
    if (!p) return;
    free(p->first);
    free(p->second);
    free(p);
}

const char *pair_first(const Pair *p) {
    assert(p != NULL);
    return p->first;
}

const char *pair_second(const Pair *p) {
    assert(p != NULL);
    return p->second;
}
```

Si `strdup` del segundo falla, necesitas **2 free**: `free(p->first)` + `free(p)`.
Si no liberas `p->first`, es un leak. Si no liberas `p`, es otro leak.

El patrón general: en un constructor con N alocaciones, el fallo en la
alocación K requiere liberar las K-1 alocaciones previas.

Nota: `pair_first` y `pair_second` retornan `const char *` — el usuario
puede leer pero no modificar los strings. Si se retornara `char *`, el
usuario podría hacer `pair_first(p)[0] = 'X'`, rompiendo el encapsulamiento.

</details>

---

### Ejercicio 10 — Reconocer el patrón en código real

La biblioteca estándar de C usa el patrón de TADs opacos. Identifica el
patrón `crear/usar/destruir` en estos ejemplos reales:

```c
// Ejemplo A
FILE *f = fopen("data.txt", "r");
fgets(buf, sizeof(buf), f);
fclose(f);

// Ejemplo B
DIR *d = opendir("/tmp");
struct dirent *entry = readdir(d);
closedir(d);

// Ejemplo C (POSIX threads)
pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
pthread_mutex_init(m, NULL);
pthread_mutex_lock(m);
pthread_mutex_unlock(m);
pthread_mutex_destroy(m);
free(m);
```

Para cada ejemplo identifica: ¿qué es el constructor? ¿El destructor?
¿Qué observadores/modificadores hay? ¿El struct es opaco?

**Predicción**: ¿En cuál de los tres ejemplos `FILE` es realmente un struct
opaco y en cuál puedes ver sus campos (dependiendo de la implementación)?

<details><summary>Respuesta</summary>

| | Constructor | Destructor | Operaciones | ¿Opaco? |
|---|-----------|-----------|-------------|---------|
| A: FILE | `fopen()` | `fclose()` | `fgets`, `fprintf`, `fread`, `fseek`... | **Sí** en la práctica |
| B: DIR | `opendir()` | `closedir()` | `readdir`, `seekdir`, `telldir` | **Sí** |
| C: pthread_mutex | `pthread_mutex_init()` | `pthread_mutex_destroy()` | `lock`, `unlock`, `trylock` | Parcial |

Detalles:

**FILE**: Técnicamente, `stdio.h` en glibc define `struct _IO_FILE` con campos
visibles, pero el estándar C solo dice que `FILE` es un tipo — no garantiza
qué campos tiene. En la práctica, nadie accede a sus campos directamente.
Es "opaco por convención" aunque no por mecanismo del lenguaje.

**DIR**: Similar — definido internamente en `<dirent.h>`, campos dependientes
de la implementación. Uso exclusivamente a través de la API.

**pthread_mutex_t**: No es un puntero opaco — es un struct que se puede poner
en el stack. Tiene dos fases: `init` (configura) y `destroy` (limpia).
El `malloc`/`free` del ejemplo C son para el struct mismo, no parte del
TAD. En stack: `pthread_mutex_t m; pthread_mutex_init(&m, NULL);`.

El ejemplo C muestra un patrón diferente: **init/destroy en lugar de
create/destroy**. El usuario aloca la memoria, el TAD solo inicializa.
Esto permite alocación en stack (sin heap).

</details>
