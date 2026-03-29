# T04 — Opaque pointer (handle pattern)

---

## 1. El problema: exponer la implementacion

En T03 definimos structs como `Circle` con todos sus campos visibles
en el header:

```c
// circle.h — todo expuesto
typedef struct {
    Shape base;
    double radius;
} Circle;
```

Cualquier archivo que incluya `circle.h` puede hacer:

```c
Circle *c = circle_create(5.0);
c->radius = -1.0;    // acceso directo — rompe invariantes
c->base.vt = NULL;   // corrompe la vtable
```

El problema no es solo que alguien haga algo malintencionado — es que
el compilador **no puede ayudarte** a prevenir accesos accidentales.
Ademas, si cambias la estructura interna de `Circle` (ej. renombrar
`radius` a `r`, agregar un campo `color`), **todo archivo que incluya
circle.h debe recompilarse**.

```
  Struct visible:              Opaque pointer:

  circle.h expone              circle.h solo expone
  ┌──────────────┐             ┌──────────────┐
  │ Shape base   │             │ typedef      │
  │ double radius│             │   struct     │
  └──────────────┘             │   Circle     │
  ↓ cualquiera accede          │   Circle;    │
  c->radius = -1;             └──────────────┘
                               ↓ nadie ve los campos
                               c->radius = ???
                               // ERROR: incomplete type
```

La solucion: **opaque pointer** — declaras que el struct existe, pero
no revelas sus campos. Solo el .c que implementa el tipo conoce la
definicion completa.

---

## 2. Mecanismo: forward declaration + incomplete type

### 2.1 Forward declaration

C permite **declarar** un struct sin **definirlo**:

```c
// Esto es una forward declaration — dice "Circle existe"
// pero NO dice que campos tiene
typedef struct Circle Circle;
```

A esto se le llama **tipo incompleto** (incomplete type). Con un tipo
incompleto puedes:

- Declarar **punteros** al tipo: `Circle *c;`
- Pasar punteros como argumentos: `void f(Circle *c);`
- Retornar punteros: `Circle *create(void);`

Pero **no puedes**:

- Acceder a campos: `c->radius` → error
- Calcular sizeof: `sizeof(Circle)` → error
- Declarar variables por valor: `Circle c;` → error
- Hacer memcpy del struct: necesitas sizeof

```c
// En el header (publico):
typedef struct Circle Circle;      // tipo incompleto

Circle *circle_create(double r);   // OK: puntero
void    circle_destroy(Circle *c); // OK: puntero
double  circle_area(Circle *c);    // OK: puntero

// Esto NO compila con tipo incompleto:
// Circle c;                       // ERROR: tamano desconocido
// c.radius;                       // ERROR: campos desconocidos
// sizeof(Circle);                 // ERROR: tipo incompleto
```

### 2.2 La definicion completa (privada)

La definicion del struct vive **solo** en el .c:

```c
// circle.c — privado, nunca incluido por otros archivos
struct Circle {
    Shape base;
    double radius;
    // Puedes agregar campos sin afectar a nadie:
    // unsigned int color;
    // char name[32];
};
```

Solo `circle.c` puede acceder a `c->radius`. Cualquier otro archivo
que incluya `circle.h` solo ve un puntero opaco — sabe que `Circle`
existe pero no que contiene.

### 2.3 Diagrama de visibilidad

```
  circle.h (publico)              circle.c (privado)
  ─────────────────               ─────────────────
  typedef struct Circle Circle;   struct Circle {
                                      Shape base;
  Circle *circle_create(double);      double radius;
  void    circle_destroy(Circle*);};
  double  circle_area(Circle*);
                                  Circle *circle_create(double r) {
  // No conoce los campos             Circle *c = malloc(sizeof(*c));
  // No conoce el tamano               c->radius = r;
  // Solo opera via funciones           return c;
                                  }

  main.c (usuario)
  ────────────────
  #include "circle.h"

  Circle *c = circle_create(5.0);
  double a = circle_area(c);     // OK: usa la API
  // c->radius;                  // ERROR: incomplete type
  circle_destroy(c);
```

---

## 3. Implementacion completa

### 3.1 El header publico

```c
// circle.h — API publica, opaque
#ifndef CIRCLE_H
#define CIRCLE_H

typedef struct Circle Circle;

// Constructor / destructor
Circle *circle_create(double radius);
void    circle_destroy(Circle *c);

// Operaciones
double  circle_area(const Circle *c);
double  circle_perimeter(const Circle *c);
void    circle_print(const Circle *c);

// Accessor (getter) — acceso controlado
double  circle_get_radius(const Circle *c);

// Mutator (setter) — puede validar
int     circle_set_radius(Circle *c, double r);

#endif
```

Observa:
- No hay `#include` de nada interno — el header es autocontenido.
- Los accessors (`get`/`set`) reemplazan el acceso directo a campos.
- El setter retorna `int` para indicar error (ej. radio negativo).

### 3.2 La implementacion privada

```c
// circle.c
#include "circle.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct Circle {
    double radius;
};

Circle *circle_create(double radius) {
    if (radius < 0) return NULL;
    Circle *c = malloc(sizeof(*c));
    if (!c) return NULL;
    c->radius = radius;
    return c;
}

void circle_destroy(Circle *c) {
    free(c);
}

double circle_area(const Circle *c) {
    return M_PI * c->radius * c->radius;
}

double circle_perimeter(const Circle *c) {
    return 2.0 * M_PI * c->radius;
}

void circle_print(const Circle *c) {
    printf("Circle(r=%.2f)\n", c->radius);
}

double circle_get_radius(const Circle *c) {
    return c->radius;
}

int circle_set_radius(Circle *c, double r) {
    if (r < 0) return -1;  // error: radio negativo
    c->radius = r;
    return 0;  // exito
}
```

### 3.3 Uso

```c
// main.c
#include "circle.h"
#include <stdio.h>

int main(void) {
    Circle *c = circle_create(5.0);
    if (!c) return 1;

    printf("Area: %.2f\n", circle_area(c));
    printf("Radius: %.2f\n", circle_get_radius(c));

    if (circle_set_radius(c, -3.0) != 0) {
        printf("Error: radio invalido\n");
    }

    circle_set_radius(c, 7.0);
    printf("New area: %.2f\n", circle_area(c));

    circle_destroy(c);

    // Estos NO compilan:
    // Circle local;            // sizeof desconocido
    // c->radius;               // campos desconocidos
    // sizeof(Circle);          // tipo incompleto
}
```

---

## 4. Handle pattern

El opaque pointer se usa frecuentemente como **handle**: un
identificador opaco que el usuario pasa a las funciones de la API
sin saber que hay detras.

```
  Handle pattern:

  Usuario          API                   Implementacion
  ───────          ───                   ──────────────
  Circle *c;       circle_create() ──→   malloc + init
  circle_area(c)   circle_area()   ──→   c->radius * ...
  circle_destroy(c)circle_destroy()──→   free
                   ↑
                   El usuario solo tiene el handle (puntero)
                   Toda operacion pasa por la API
```

### 4.1 Handles en la vida real

Este patron esta en todas partes:

| API | Handle | Operaciones |
|-----|--------|-------------|
| stdio | `FILE *` | fopen, fread, fwrite, fclose |
| POSIX files | `int fd` | open, read, write, close |
| POSIX threads | `pthread_t` | pthread_create, pthread_join |
| OpenGL | `GLuint` (texture ID) | glGenTextures, glBindTexture |
| SQLite | `sqlite3 *` | sqlite3_open, sqlite3_exec, sqlite3_close |
| OpenSSL | `SSL_CTX *` | SSL_CTX_new, SSL_CTX_free |

`FILE *` es el ejemplo canonico en C:

```c
FILE *f = fopen("data.txt", "r");
// ¿Que hay dentro de FILE? No lo sabes — y no necesitas saberlo.
// La struct FILE esta definida en los .c de la libc, no en stdio.h
// (en la practica, muchas libc SI exponen FILE, pero conceptualmente
// es un opaque handle)

fread(buf, 1, 100, f);   // operas a traves de la API
fclose(f);                // la API limpia
```

### 4.2 Handle numerico vs puntero

A veces el handle no es un puntero sino un **entero**:

```c
// POSIX file descriptors — el handle es un int
int fd = open("data.txt", O_RDONLY);
read(fd, buf, 100);
close(fd);

// El int es un indice en una tabla interna del kernel
// El usuario no tiene acceso a la tabla — solo al indice
```

```
  Handle puntero (FILE*):         Handle numerico (int fd):

  Usuario tiene:                  Usuario tiene:
  ┌──────────┐                    ┌────┐
  │ FILE *f ───→ struct FILE      │ 3  │ ← solo un numero
  └──────────┘   (en libc)        └────┘
                                       ↓ (indice en tabla del kernel)
                                  ┌────────────────────────┐
                                  │ fd 0: stdin            │
                                  │ fd 1: stdout           │
                                  │ fd 2: stderr           │
                                  │ fd 3: data.txt (aqui)  │
                                  └────────────────────────┘
```

| Aspecto | Handle puntero | Handle numerico |
|---------|---------------|----------------|
| Ejemplo | `FILE *`, `Circle *` | `int fd`, `GLuint` |
| Acceso a internos | No (opaque) | No (indice en tabla) |
| Puede ser NULL | Si (error comun) | Usa -1 o 0 como invalido |
| Valido tras free? | No — dangling | Depende (fd se recicla) |
| Puede cruzar procesos | No (direcciones distintas) | Si (fd heredados) |

---

## 5. Opaque pointer + vtable

El opaque pointer se combina naturalmente con las vtables de T03. La
interfaz generica se mantiene igual, pero los tipos concretos esconden
sus detalles:

```c
// shape.h — interfaz publica (igual que T03)
typedef struct ShapeVtable ShapeVtable;
typedef struct Shape Shape;

struct ShapeVtable {
    double (*area)(const void *self);
    void   (*destroy)(void *self);
};

struct Shape {
    const ShapeVtable *vt;
};

static inline double shape_area(const Shape *s) {
    return s->vt->area(s);
}

static inline void shape_destroy(Shape *s) {
    s->vt->destroy(s);
}
```

```c
// circle.h — tipo concreto, OPACO
typedef struct Circle Circle;
Circle *circle_create(double radius);
// NO expone Shape — el usuario no necesita saber de vtables
```

```c
// circle.c — implementacion privada
#include "circle.h"
#include "shape.h"

struct Circle {
    Shape base;        // solo visible aqui
    double radius;     // solo visible aqui
};

static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return 3.14159265 * c->radius * c->radius;
}

static void circle_destroy(void *self) { free(self); }

static const ShapeVtable circle_vtable = {
    .area    = circle_area,
    .destroy = circle_destroy,
};

Circle *circle_create(double radius) {
    Circle *c = malloc(sizeof(*c));
    c->base.vt = &circle_vtable;
    c->radius = radius;
    return c;
}
```

```c
// main.c — usa ambas interfaces
#include "shape.h"
#include "circle.h"

Circle *c = circle_create(5.0);

// Via API de Circle:
// (no hay getters en este ejemplo, pero podrian existir)

// Via API generica de Shape:
Shape *s = (Shape *)c;
printf("%.2f\n", shape_area(s));
shape_destroy(s);
```

Dos niveles de abstraccion:
1. **circle.h**: opaque pointer — esconde los campos
2. **shape.h**: vtable — permite tratar Circle como Shape generico

---

## 6. Beneficios del opaque pointer

### 6.1 Encapsulacion: proteger invariantes

```c
// Sin opaque: el usuario puede romper invariantes
Circle *c = circle_create(5.0);
c->radius = -100.0;   // radio negativo — area negativa, bugs cascading

// Con opaque: solo la API puede modificar
int err = circle_set_radius(c, -100.0);
// Retorna -1, radius no cambia — invariante protegido
```

### 6.2 Estabilidad de ABI

ABI (Application Binary Interface) es como se representan los datos en
binario. Si cambias los campos de un struct publico, cambias el ABI:

```c
// Version 1:
typedef struct { double radius; } Circle;           // sizeof = 8

// Version 2: agregaste un campo
typedef struct { double radius; int color; } Circle; // sizeof = 16
```

Todo codigo compilado con la version 1 espera `sizeof(Circle) == 8`.
Si linkeas contra la version 2, **crash o corrupcion silenciosa**.

Con opaque pointer, sizeof no es visible — el usuario nunca lo usa.
Puedes agregar campos libremente:

```c
// circle.c v1:
struct Circle { double radius; };

// circle.c v2: cambio interno, nadie lo nota
struct Circle { double radius; int color; char name[32]; };
```

Solo recompilas `circle.c`. El header no cambio, los usuarios no
recompilan.

### 6.3 Reduccion de dependencias de compilacion

```
  Sin opaque:                      Con opaque:

  circle.h                         circle.h
  ┌──────────────────┐             ┌──────────────────┐
  │ #include "color.h"│            │ typedef struct    │
  │ #include "shape.h"│            │   Circle Circle;  │
  │ struct Circle {   │             └──────────────────┘
  │   Shape base;     │             ↓
  │   Color fill;     │             Cambiar Circle solo
  │   double radius;  │             recompila circle.c
  │ };                │
  └──────────────────┘
  ↓
  Cambiar Color o Shape
  recompila TODO lo que
  incluya circle.h
```

En proyectos grandes, esta diferencia convierte compilaciones de
minutos en segundos.

### 6.4 Tabla resumen

| Aspecto | Struct publico | Opaque pointer |
|---------|---------------|----------------|
| Acceso a campos | Directo | Solo via API |
| sizeof visible | Si | No |
| Validacion en setters | Opcional (bypass posible) | Forzada |
| Agregar campos | Recompila todo | Recompila solo el .c |
| Asignar en stack | `Circle c;` | No — solo heap (malloc) |
| Inline por compilador | Posible (ve los campos) | Imposible (no ve los campos) |
| Header dependencies | Transitivas | Minimas |

---

## 7. Limitaciones del opaque pointer

### 7.1 Solo heap — no stack

Con tipo incompleto, no puedes declarar variables en stack:

```c
Circle c;                   // ERROR: tamano desconocido
Circle arr[10];             // ERROR: tamano desconocido
Circle *c = &(Circle){};   // ERROR: no puedes usar compound literal
```

Todo debe ser `malloc` + puntero. Esto tiene un costo: cada instancia
es una allocation del heap, con overhead de `malloc` y potenciales cache
misses.

### 7.2 Workaround: alloc-in-buffer

Algunas APIs exponen el tamano sin exponer los campos:

```c
// circle.h
size_t circle_sizeof(void);  // retorna sizeof(struct Circle)
void   circle_init(Circle *c, double radius);  // init in-place

// Uso: el usuario asigna memoria, la API inicializa
char buf[circle_sizeof()];          // VLA
Circle *c = (Circle *)buf;
circle_init(c, 5.0);
// c vive en el stack — sin malloc
```

Esto sacrifica algo de opacidad (el tamano es visible) pero permite
evitar malloc. Es comun en APIs de alto rendimiento (ej. cryptography
libraries).

### 7.3 No se puede copiar por valor

```c
// Esto no compila:
Circle copy = *original;    // ERROR: sizeof desconocido

// Necesitas una funcion de clone:
Circle *copy = circle_clone(original);  // la API hace malloc+memcpy
```

### 7.4 Inlining imposible

El compilador no puede ver los campos desde otro archivo, asi que no
puede optimizar accesos. Para codigo en hot paths, esto puede importar:

```c
// Con struct publico, el compilador puede inlinear:
double a = c->radius * c->radius * M_PI;  // una instruccion

// Con opaque, debe hacer una llamada a funcion:
double a = circle_area(c);  // call + return, no inlineable
```

En la practica, LTO (Link-Time Optimization) puede mitigar esto en
builds de release.

---

## 8. Opaque pointer en Rust

Rust no necesita opaque pointers para encapsulacion — el sistema de
modulos y visibilidad (`pub` / no `pub`) lo resuelve:

```rust
// circle.rs
pub struct Circle {
    radius: f64,    // privado por defecto — nadie fuera del modulo accede
}

impl Circle {
    pub fn new(radius: f64) -> Self {
        Circle { radius }
    }
    pub fn area(&self) -> f64 {
        std::f64::consts::PI * self.radius.powi(2)
    }
    pub fn radius(&self) -> f64 {
        self.radius
    }
}
```

```rust
// main.rs
let c = Circle::new(5.0);
println!("{}", c.area());     // OK: metodo publico
// c.radius = -1.0;           // ERROR: campo privado
println!("{}", c.radius());   // OK: getter publico
```

| Aspecto | C opaque pointer | Rust `pub` / privado |
|---------|-----------------|---------------------|
| Encapsulacion | Forward declaration | `pub` fields / methods |
| Stack allocation | No (sizeof desconocido) | Si (sizeof conocido por el compilador) |
| ABI estable al agregar campos | Si | No (sizeof cambia) |
| Copia | Solo con clone explicito | Copy/Clone traits |
| Inlining | No (funcion en otro .c) | Si (monomorphization, inlining) |

Rust obtiene encapsulacion **sin** sacrificar stack allocation ni
inlining. El opaque pointer en C es un trade-off: ganas ocultacion
pero pierdes rendimiento potencial.

---

## 9. Patrones de API con opaque pointer

### 9.1 Constructor/destructor simetrico

```c
Circle *circle_create(double radius);  // alloc + init
void    circle_destroy(Circle *c);     // cleanup + free

// Regla: si create alloca, destroy libera
// Documentar claramente: "el llamador es responsable de destroy"
```

### 9.2 Init/deinit para memoria pre-asignada

```c
// Cuando el usuario controla la memoria:
void circle_init(Circle *c, double radius);  // solo init, no alloca
void circle_deinit(Circle *c);               // solo cleanup, no free
```

### 9.3 Getters y setters con validacion

```c
// Getter: siempre const
double circle_get_radius(const Circle *c);

// Setter: retorna error code
int circle_set_radius(Circle *c, double r);
// Retorna 0 en exito, -1 en error
```

### 9.4 Convencion de nombres: prefijo del modulo

```c
// Modulo "circle" — todas las funciones usan el prefijo "circle_"
circle_create()
circle_destroy()
circle_area()
circle_get_radius()

// Modulo "list" — prefijo "list_"
list_create()
list_push()
list_destroy()
```

Esto simula namespaces en C (que no tiene namespaces nativos). Rust
usa `mod` y `use`; C usa prefijos por convencion.

### 9.5 Resumen de una API completa

```c
// widget.h — API opaque completa

typedef struct Widget Widget;

// Lifecycle
Widget *widget_create(const char *label, int width, int height);
Widget *widget_clone(const Widget *w);
void    widget_destroy(Widget *w);

// Getters
const char *widget_get_label(const Widget *w);
int         widget_get_width(const Widget *w);
int         widget_get_height(const Widget *w);

// Setters (con validacion)
int  widget_set_label(Widget *w, const char *label);
int  widget_set_size(Widget *w, int width, int height);

// Operaciones
void widget_draw(const Widget *w);
int  widget_resize(Widget *w, double factor);

// Serialization
char *widget_to_string(const Widget *w);  // caller frees
```

Todas las funciones reciben `Widget *` como primer argumento — el
equivalente manual del `self` de Rust o el `this` de C++.

---

## Errores comunes

### Error 1 — Definir el struct en el header

```c
// circle.h — MAL: el struct esta definido aqui
struct Circle {
    double radius;
};
```

Si defines el struct en el header, deja de ser opaque. Cualquier
archivo que incluya `circle.h` puede acceder a los campos. El forward
declaration (`typedef struct Circle Circle;`) es lo que crea la
opacidad.

### Error 2 — sizeof de tipo incompleto

```c
// main.c
#include "circle.h"

Circle *c = malloc(sizeof(Circle));  // ERROR: tipo incompleto
```

Solo el .c que define el struct puede usar `sizeof`. La solucion:
toda allocation pasa por `circle_create` (que esta en `circle.c`).

### Error 3 — Olvidar destroy (memory leak)

```c
void process(void) {
    Circle *c = circle_create(5.0);
    printf("%.2f\n", circle_area(c));
    // LEAK: olvido circle_destroy(c)
}
```

Sin destructor de C++ ni Drop de Rust, es responsabilidad del
programador llamar a `destroy`. Herramientas como Valgrind y
AddressSanitizer detectan esto.

### Error 4 — Use after destroy

```c
Circle *c = circle_create(5.0);
circle_destroy(c);
double a = circle_area(c);  // UB: use-after-free
```

Patron defensivo: setear a NULL despues de destroy:

```c
circle_destroy(c);
c = NULL;
// circle_area(c) → segfault inmediato (mejor que corrupcion silenciosa)
```

### Error 5 — Exponer detalles internos en el return

```c
// circle.h
double *circle_get_radius_ptr(Circle *c);  // MAL: expone puntero interno
```

Retornar un puntero a un campo interno rompe la encapsulacion — el
usuario puede modificar `radius` sin pasar por el setter. Retorna
el valor, no el puntero:

```c
double circle_get_radius(const Circle *c);  // BIEN: retorna copia
```

---

## Ejercicios

### Ejercicio 1 — Que compila y que no

Sin compilar, indica si cada linea compila correctamente dado este
header:

```c
// stack.h
typedef struct Stack Stack;
Stack *stack_create(int capacity);
void   stack_push(Stack *s, int value);
int    stack_pop(Stack *s);
void   stack_destroy(Stack *s);
```

```c
#include "stack.h"

// A
Stack *s = stack_create(10);

// B
Stack local;

// C
sizeof(Stack);

// D
Stack *copy = malloc(sizeof(*s));

// E
stack_push(s, 42);

// F
s->count;
```

**Prediccion**: Para las que no compilan, cual es el mensaje de error
exacto que daria gcc?

<details><summary>Respuesta</summary>

| Linea | Compila | Razon |
|-------|---------|-------|
| A | Si | Declara un puntero a tipo incompleto — valido |
| B | No | `Stack local` requiere sizeof — tipo incompleto |
| C | No | sizeof de tipo incompleto no es valido |
| D | No | `sizeof(*s)` requiere completar el tipo de `*s` |
| E | Si | Pasa un puntero — no necesita sizeof |
| F | No | Acceso a campo de tipo incompleto |

Mensajes de gcc:

- B: `error: storage size of 'local' isn't known`
- C: `error: invalid application of 'sizeof' to incomplete type 'struct Stack'`
- D: `error: invalid application of 'sizeof' to incomplete type 'struct Stack'`
- F: `error: dereferencing pointer to incomplete type 'struct Stack'`

Nota sobre D: `sizeof(*s)` es equivalente a `sizeof(Stack)` — ambos
necesitan la definicion completa. Dentro de `stack.c`, donde el struct
esta definido, `sizeof(*s)` funciona perfectamente — es la forma
idiomatica de malloc en la implementacion.

</details>

---

### Ejercicio 2 — Implementar un buffer opaco

Implementa un buffer circular (ring buffer) con opaque pointer:

```c
// ringbuf.h
typedef struct RingBuf RingBuf;

RingBuf *ringbuf_create(size_t capacity);
void     ringbuf_destroy(RingBuf *rb);
int      ringbuf_push(RingBuf *rb, int value);   // 0=ok, -1=full
int      ringbuf_pop(RingBuf *rb, int *out);      // 0=ok, -1=empty
size_t   ringbuf_count(const RingBuf *rb);
bool     ringbuf_is_full(const RingBuf *rb);
bool     ringbuf_is_empty(const RingBuf *rb);
```

**Prediccion**: Que campos necesita el struct interno? Piensa antes de
ver la respuesta: head, tail, count, capacity, data.

<details><summary>Respuesta</summary>

```c
// ringbuf.h
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>
#include <stdbool.h>

typedef struct RingBuf RingBuf;

RingBuf *ringbuf_create(size_t capacity);
void     ringbuf_destroy(RingBuf *rb);
int      ringbuf_push(RingBuf *rb, int value);
int      ringbuf_pop(RingBuf *rb, int *out);
size_t   ringbuf_count(const RingBuf *rb);
bool     ringbuf_is_full(const RingBuf *rb);
bool     ringbuf_is_empty(const RingBuf *rb);

#endif
```

```c
// ringbuf.c
#include "ringbuf.h"
#include <stdlib.h>

struct RingBuf {
    int    *data;
    size_t  head;      // indice de lectura
    size_t  tail;      // indice de escritura
    size_t  count;     // elementos actuales
    size_t  capacity;
};

RingBuf *ringbuf_create(size_t capacity) {
    RingBuf *rb = malloc(sizeof(*rb));
    if (!rb) return NULL;
    rb->data = malloc(sizeof(int) * capacity);
    if (!rb->data) { free(rb); return NULL; }
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->capacity = capacity;
    return rb;
}

void ringbuf_destroy(RingBuf *rb) {
    if (!rb) return;
    free(rb->data);
    free(rb);
}

int ringbuf_push(RingBuf *rb, int value) {
    if (rb->count == rb->capacity) return -1;
    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return 0;
}

int ringbuf_pop(RingBuf *rb, int *out) {
    if (rb->count == 0) return -1;
    *out = rb->data[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return 0;
}

size_t ringbuf_count(const RingBuf *rb) {
    return rb->count;
}

bool ringbuf_is_full(const RingBuf *rb) {
    return rb->count == rb->capacity;
}

bool ringbuf_is_empty(const RingBuf *rb) {
    return rb->count == 0;
}
```

Uso:

```c
RingBuf *rb = ringbuf_create(4);

ringbuf_push(rb, 10);
ringbuf_push(rb, 20);
ringbuf_push(rb, 30);

int val;
ringbuf_pop(rb, &val);  // val == 10 (FIFO)

ringbuf_push(rb, 40);
ringbuf_push(rb, 50);
// buffer: [20, 30, 40, 50] — lleno

ringbuf_push(rb, 60);  // retorna -1 (full)

ringbuf_destroy(rb);
```

El usuario no sabe que hay head, tail, ni count. Solo interactua via
push/pop. Puedes cambiar la implementacion (ej. usar potencias de 2 y
mascaras en vez de modulo) sin cambiar el header.

</details>

---

### Ejercicio 3 — Convertir struct publico a opaque

Dado este codigo con struct publico, refactorizalo a opaque pointer:

```c
// ANTES — todo publico
typedef struct {
    char  *items[100];
    int    count;
    int    max_items;
} TodoList;

void todo_add(TodoList *t, const char *item) {
    if (t->count >= t->max_items) return;
    t->items[t->count] = strdup(item);
    t->count++;
}

void todo_print(const TodoList *t) {
    for (int i = 0; i < t->count; i++)
        printf("%d. %s\n", i + 1, t->items[i]);
}
```

**Prediccion**: Que funciones nuevas necesitas agregar al header que
antes no existian?

<details><summary>Respuesta</summary>

```c
// todo.h — DESPUES (opaque)
#ifndef TODO_H
#define TODO_H

typedef struct TodoList TodoList;

TodoList   *todo_create(int max_items);
void        todo_destroy(TodoList *t);
void        todo_add(TodoList *t, const char *item);
void        todo_print(const TodoList *t);
int         todo_count(const TodoList *t);

#endif
```

```c
// todo.c
#include "todo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TodoList {
    char **items;
    int    count;
    int    max_items;
};

TodoList *todo_create(int max_items) {
    TodoList *t = malloc(sizeof(*t));
    if (!t) return NULL;
    t->items = calloc(max_items, sizeof(char *));
    if (!t->items) { free(t); return NULL; }
    t->count = 0;
    t->max_items = max_items;
    return t;
}

void todo_destroy(TodoList *t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++)
        free(t->items[i]);
    free(t->items);
    free(t);
}

void todo_add(TodoList *t, const char *item) {
    if (t->count >= t->max_items) return;
    t->items[t->count] = strdup(item);
    t->count++;
}

void todo_print(const TodoList *t) {
    for (int i = 0; i < t->count; i++)
        printf("%d. %s\n", i + 1, t->items[i]);
}

int todo_count(const TodoList *t) {
    return t->count;
}
```

Funciones nuevas necesarias:
- `todo_create` — antes el usuario hacia `TodoList t = {0}` en stack
- `todo_destroy` — antes el usuario liberaba manualmente o no liberaba
- `todo_count` — antes leia `t->count` directamente

Ademas, cambie `char *items[100]` (array fijo en el struct) a
`char **items` (array dinamico). Esto es comun al opaquificar: puedes
mejorar la implementacion sin afectar la API.

</details>

---

### Ejercicio 4 — Opaque + vtable combinados

Implementa un sistema `Logger` con opaque pointer y vtable. Dos
implementaciones: `ConsoleLogger` y `FileLogger`.

El header publico solo debe exponer:

```c
// logger.h
typedef struct Logger Logger;

Logger *console_logger_create(void);
Logger *file_logger_create(const char *path);
void    logger_log(Logger *log, const char *msg);
void    logger_destroy(Logger *log);
```

**Prediccion**: El usuario no ve la vtable, no ve los campos, no ve
la diferencia entre Console y File. Donde vive la vtable?

<details><summary>Respuesta</summary>

```c
// logger.h
#ifndef LOGGER_H
#define LOGGER_H

typedef struct Logger Logger;

Logger *console_logger_create(void);
Logger *file_logger_create(const char *path);
void    logger_log(Logger *log, const char *msg);
void    logger_destroy(Logger *log);

#endif
```

```c
// logger.c
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

// Vtable — privada, el usuario no la ve
typedef struct {
    void (*log)(void *self, const char *msg);
    void (*destroy)(void *self);
} LoggerVtable;

// Base — privada
struct Logger {
    const LoggerVtable *vt;
};

// --- Console Logger ---
typedef struct {
    Logger base;
} ConsoleLogger;

static void console_log(void *self, const char *msg) {
    (void)self;
    fprintf(stderr, "[LOG] %s\n", msg);
}

static void console_destroy(void *self) {
    free(self);
}

static const LoggerVtable console_vt = {
    .log     = console_log,
    .destroy = console_destroy,
};

Logger *console_logger_create(void) {
    ConsoleLogger *cl = malloc(sizeof(*cl));
    if (!cl) return NULL;
    cl->base.vt = &console_vt;
    return (Logger *)cl;
}

// --- File Logger ---
typedef struct {
    Logger base;
    FILE  *file;
} FileLogger;

static void file_log(void *self, const char *msg) {
    FileLogger *fl = (FileLogger *)self;
    fprintf(fl->file, "[LOG] %s\n", msg);
    fflush(fl->file);
}

static void file_destroy(void *self) {
    FileLogger *fl = (FileLogger *)self;
    if (fl->file) fclose(fl->file);
    free(fl);
}

static const LoggerVtable file_vt = {
    .log     = file_log,
    .destroy = file_destroy,
};

Logger *file_logger_create(const char *path) {
    FileLogger *fl = malloc(sizeof(*fl));
    if (!fl) return NULL;
    fl->file = fopen(path, "a");
    if (!fl->file) { free(fl); return NULL; }
    fl->base.vt = &file_vt;
    return (Logger *)fl;
}

// --- API generica ---
void logger_log(Logger *log, const char *msg) {
    log->vt->log(log, msg);
}

void logger_destroy(Logger *log) {
    if (log) log->vt->destroy(log);
}
```

```c
// main.c
#include "logger.h"

int main(void) {
    Logger *log = console_logger_create();
    logger_log(log, "Aplicacion iniciada");
    logger_destroy(log);

    log = file_logger_create("app.log");
    logger_log(log, "Escribiendo a archivo");
    logger_destroy(log);
}
```

La vtable vive en `logger.c` — es `static const`, invisible fuera del
archivo. El usuario solo ve `Logger *` y las 4 funciones del header.
No sabe que hay vtables, no sabe que hay ConsoleLogger vs FileLogger
como structs separados. Solo sabe que `console_logger_create` y
`file_logger_create` retornan un `Logger *` que se usa igual.

Este es el nivel maximo de encapsulacion en C: opaque pointer + vtable
+ implementaciones privadas en un solo .c.

</details>

---

### Ejercicio 5 — API robusta: errores y lifecycle

Disena (solo el header, no la implementacion) una API opaque para un
`HashMap` generico. Debe soportar:

- Crear con capacidad inicial
- Insertar key-value (ambos `const char *` por simplicidad)
- Buscar por key
- Eliminar por key
- Obtener el numero de elementos
- Destruir

**Prediccion**: Para cada funcion, decide el tipo de retorno y como
reportas errores. No hay excepciones en C — como indica el usuario que
una busqueda no encontro el key?

<details><summary>Respuesta</summary>

```c
// hashmap.h
#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

typedef struct HashMap HashMap;

/**
 * Crea un HashMap vacio.
 * @param initial_capacity Capacidad inicial de buckets.
 * @return HashMap nuevo, o NULL si falla la allocation.
 */
HashMap *hashmap_create(size_t initial_capacity);

/**
 * Destruye el HashMap y libera todas las keys y values copiadas.
 * @param map El HashMap a destruir. Puede ser NULL (no-op).
 */
void hashmap_destroy(HashMap *map);

/**
 * Inserta o actualiza un par key-value.
 * Hace copia interna de key y value (strdup).
 * Si el key ya existe, actualiza el value.
 * @return 0 en exito, -1 en error de allocation.
 */
int hashmap_insert(HashMap *map, const char *key, const char *value);

/**
 * Busca un value por key.
 * @return Puntero al value interno (NO hacer free), o NULL si no existe.
 *         El puntero se invalida si se inserta o elimina del map.
 */
const char *hashmap_get(const HashMap *map, const char *key);

/**
 * Verifica si un key existe.
 * @return true si el key existe, false si no.
 */
bool hashmap_contains(const HashMap *map, const char *key);

/**
 * Elimina un par key-value.
 * @return true si se elimino, false si el key no existia.
 */
bool hashmap_remove(HashMap *map, const char *key);

/**
 * Retorna el numero de pares key-value almacenados.
 */
size_t hashmap_count(const HashMap *map);

#endif
```

Decisiones de diseno:

| Funcion | Retorno | Reporta error via |
|---------|---------|-------------------|
| `create` | `HashMap *` | NULL = fallo |
| `insert` | `int` | -1 = fallo, 0 = ok |
| `get` | `const char *` | NULL = no encontrado |
| `contains` | `bool` | false = no existe |
| `remove` | `bool` | false = no existia |
| `count` | `size_t` | No puede fallar |
| `destroy` | `void` | Acepta NULL silenciosamente |

Nota sobre `hashmap_get`: retorna `const char *` (puntero al valor
interno, no una copia). Esto es BORROW semantics — el usuario no debe
hacer `free` y el puntero se invalida con mutaciones al map.

Alternativa mas segura (COPY semantics):

```c
// El llamador pasa un buffer y recibe la copia:
int hashmap_get(const HashMap *map, const char *key,
                char *buf, size_t buf_size);
// Retorna 0 si encontro y copio, -1 si no encontro
```

Ambos disenos son validos. El primero es mas ergonomico, el segundo
mas seguro. En Rust, `HashMap::get` retorna `Option<&V>` — borrow
con lifetime atado al map, verificado en compile time.

</details>

---

### Ejercicio 6 — Analizar API real: FILE*

`FILE *` de `<stdio.h>` es el opaque handle mas usado en C. Analiza
su API respondiendo:

1. Cual es la funcion create (`fopen`) y la funcion destroy (`fclose`)?
2. Como reporta errores `fopen`? Y `fread`?
3. `fprintf` necesita acceder a campos internos de FILE. Como lo hace
   si FILE es opaco?
4. Puedes copiar un FILE? (`FILE a = *f;`)

**Prediccion**: En tu sistema, `FILE` es realmente opaco? Busca en
`/usr/include/stdio.h` si la definicion del struct es visible.

<details><summary>Respuesta</summary>

1. **Create/destroy**: `fopen` crea, `fclose` destruye. Simetricos:
   todo `fopen` debe tener su `fclose`.

2. **Errores**: `fopen` retorna `NULL` en error (errno se setea).
   `fread` retorna menos elementos que los pedidos — el llamador debe
   verificar con `feof`/`ferror`.

3. **fprintf y campos internos**: `fprintf` esta implementada en la
   libc (el mismo .c que define `struct FILE`). Dentro de la libc,
   el struct no es opaco — las funciones acceden a los campos
   directamente. Solo es opaco para el **usuario** de la API.

4. **Copiar FILE**: depende de la implementacion.

   ```c
   FILE a = *f;  // Puede compilar si FILE es un struct visible
   ```

   En muchas implementaciones de libc (glibc, musl), `struct FILE` SI
   esta definida en los headers publicos (para compatibilidad historica
   y macros como `getc`). Esto significa que tecnicamente puedes copiar
   un FILE, pero es **undefined behavior** usarlo — los campos internos
   (buffers, posiciones de archivo, locks) no estan disenados para ser
   copiados.

   En una implementacion ideal, FILE seria completamente opaco y
   `FILE a = *f` no compilaria. La realidad historica de C lo impide.

**Sobre tu sistema**: en glibc (Linux), `struct _IO_FILE` esta definida
en `<bits/types/struct_FILE.h>`. Los campos son visibles pero
documentados como internos. En musl, la definicion esta en un header
interno. El grado de opacidad varia.

</details>

---

### Ejercicio 7 — Opaque pointer generico

Implementa un `Pair` opaque que almacena dos valores de cualquier tipo:

```c
// pair.h
typedef struct Pair Pair;

Pair       *pair_create(const void *first, size_t first_size,
                        const void *second, size_t second_size);
void        pair_destroy(Pair *p);
const void *pair_first(const Pair *p);
const void *pair_second(const Pair *p);
```

**Prediccion**: Cuantas allocations necesita `pair_create`? Podrias
hacerlo con una sola?

<details><summary>Respuesta</summary>

```c
// pair.c
#include "pair.h"
#include <stdlib.h>
#include <string.h>

struct Pair {
    void  *first;
    void  *second;
    size_t first_size;
    size_t second_size;
};

// Version con 3 allocations (simple):
Pair *pair_create(const void *first, size_t first_size,
                  const void *second, size_t second_size) {
    Pair *p = malloc(sizeof(*p));
    if (!p) return NULL;

    p->first = malloc(first_size);
    p->second = malloc(second_size);
    if (!p->first || !p->second) {
        free(p->first);
        free(p->second);
        free(p);
        return NULL;
    }

    memcpy(p->first, first, first_size);
    memcpy(p->second, second, second_size);
    p->first_size = first_size;
    p->second_size = second_size;
    return p;
}

void pair_destroy(Pair *p) {
    if (!p) return;
    free(p->first);
    free(p->second);
    free(p);
}

const void *pair_first(const Pair *p)  { return p->first; }
const void *pair_second(const Pair *p) { return p->second; }
```

**Version con 1 allocation** (optimizada):

```c
Pair *pair_create(const void *first, size_t first_size,
                  const void *second, size_t second_size) {
    // Una sola allocation: struct + datos de first + datos de second
    size_t total = sizeof(Pair) + first_size + second_size;
    Pair *p = malloc(total);
    if (!p) return NULL;

    // first va justo despues del struct
    p->first = (char *)p + sizeof(Pair);
    memcpy(p->first, first, first_size);

    // second va despues de first
    p->second = (char *)p->first + first_size;
    memcpy(p->second, second, second_size);

    p->first_size = first_size;
    p->second_size = second_size;
    return p;
}

void pair_destroy(Pair *p) {
    free(p);  // una sola allocation → un solo free
}
```

```
  Memoria (1 allocation):

  ┌──────────────┬───────────────┬────────────────┐
  │ Pair struct  │ first data    │ second data    │
  │ first ─────────→             │                │
  │ second ──────────────────────────→            │
  └──────────────┴───────────────┴────────────────┘
  ← sizeof(Pair) →← first_size →← second_size  →
```

La version de 1 allocation es mas eficiente (menos overhead de malloc,
mejor cache locality) y mas segura (imposible olvidar un free parcial).
Este patron se llama "flexible member" o "trailing data".

Uso:

```c
int x = 42;
double y = 3.14;
Pair *p = pair_create(&x, sizeof(x), &y, sizeof(y));

int a = *(const int *)pair_first(p);       // 42
double b = *(const double *)pair_second(p); // 3.14

pair_destroy(p);
```

</details>

---

### Ejercicio 8 — Encontrar bugs

Cada fragmento tiene un bug relacionado con opaque pointers:

```c
// Bug A — en el .c de implementacion
struct Circle {
    double radius;
};

Circle *circle_create(double r) {
    Circle c = { .radius = r };
    return &c;
}

// Bug B — en main.c
#include "circle.h"
Circle *c = circle_create(5.0);
Circle *copy = malloc(sizeof(*c));
memcpy(copy, c, sizeof(*c));

// Bug C — en circle.h
typedef struct Circle Circle;
struct Circle {
    double radius;
};
Circle *circle_create(double r);
```

**Prediccion**: Cual causa crash, cual anula la opacidad, y cual es
error de compilacion?

<details><summary>Respuesta</summary>

**Bug A: Crash (retorno de puntero a local).**

`Circle c` es una variable local en stack. Retornar `&c` da un dangling
pointer — la memoria se destruye al salir de la funcion.

**Fix**: `Circle *c = malloc(sizeof(*c));`

**Bug B: Error de compilacion.**

En `main.c`, `Circle` es tipo incompleto (solo incluye `circle.h` con
forward declaration). `sizeof(*c)` requiere la definicion completa del
struct. El compilador dara:
`error: invalid application of 'sizeof' to incomplete type`

Incluso si compilara, copiar un struct opaco byte a byte romperia
cualquier invariante interna (punteros compartidos, handles, etc.).

**Bug C: Anula la opacidad.**

El struct `Circle` esta definido en el header — cualquier archivo que
incluya `circle.h` puede acceder a `c->radius`. La forward declaration
`typedef struct Circle Circle` es inutil si la definicion completa
le sigue en el mismo header.

**Fix**: mover `struct Circle { ... }` a `circle.c`. El header solo
debe tener el typedef y los prototipos de funciones.

</details>

---

### Ejercicio 9 — Diseno de API: trade-offs

Para cada escenario, decide si usarias struct publico u opaque pointer,
y justifica:

1. Una struct `Vec2 { double x, y; }` para operaciones matematicas en
   un game engine (hot path, millones por frame).

2. Una struct `DatabaseConnection` que almacena un socket, un buffer de
   queries, y credenciales.

3. Una struct `Config { int port; bool debug; char *logfile; }` que se
   lee una vez al inicio y se consulta frecuentemente.

4. Una libreria de criptografia que almacena keys en un `CryptoContext`.

**Prediccion**: La respuesta no siempre es "opaque es mejor". Para cada
caso, piensa que se gana y que se pierde.

<details><summary>Respuesta</summary>

**1. Vec2 — Struct publico.**

- Razones: solo 2 campos, no tiene invariantes complejas (cualquier
  combinacion de x,y es valida), hot path necesita inlining y acceso
  directo, stack allocation critica para millones de instancias.
- Un opaque pointer aqui significaria: malloc por cada vector,
  call overhead por cada `vec2_x()`, imposibilidad de SIMD operations
  directas. Inaceptable en un game engine.

**2. DatabaseConnection — Opaque pointer.**

- Razones: muchos campos internos, invariantes complejos (socket debe
  estar abierto, credenciales deben ser validas, buffer consistente),
  el usuario no debe manipular el socket directamente, la implementacion
  puede cambiar (ej. agregar connection pooling) sin afectar usuarios.
- Stack allocation no importa (se crea una vez).

**3. Config — Depende, probablemente struct publico.**

- Se lee una vez y se consulta miles de veces. Si es opaque, cada
  consulta es una llamada a funcion (`config_get_port(cfg)`). Si es
  publica, es acceso directo (`cfg->port`).
- Los campos son simples y estables (no cambian entre versiones).
- Pero si la config viene de un archivo externo y necesita validacion,
  un opaque con setters validados puede aportar.
- **Compromiso**: struct publico con campos `const` para read-only:

  ```c
  typedef struct {
      const int port;
      const bool debug;
      const char *logfile;
  } Config;
  ```

**4. CryptoContext — Opaque pointer, obligatorio.**

- Razones: las keys criptograficas deben estar protegidas, no expuestas
  en headers. La implementacion puede usar hardware (HSM), memoria
  protegida (mlock), o zeroing automatico. El usuario no debe poder
  leer ni copiar keys directamente.
- Ademas, la API debe forzar `crypto_destroy` que haga `memset(0)`
  (zeroize) antes de `free`.
- Aqui la opacidad es una **necesidad de seguridad**, no solo de diseno.

</details>

---

### Ejercicio 10 — Reflexion: encapsulacion en C vs Rust

Responde sin ver la respuesta:

1. En C, el opaque pointer previene acceso a campos en compile time.
   Pero un programador determinado podria leer `circle.c` y reconstruir
   el struct. Ofrece proteccion real? Contra que protege?

2. Rust tiene `pub` y `pub(crate)` para controlar visibilidad. Un
   campo privado es inaccesible incluso si el programador lee el codigo
   fuente. Cual es la diferencia fundamental con C?

3. Un opaque pointer forza heap allocation. Rust puede tener campos
   privados en structs en stack. Como logra Rust encapsulacion sin
   heap?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. Contra que protege el opaque pointer:**

No protege contra un programador determinado que quiera hacer trampa
(puede copiar la definicion del struct o leer bytes con offsets
manuales). Protege contra:

- **Acceso accidental**: `c->radius = -1` no compila. El programador
  debe pasar por la API deliberadamente.
- **Dependencia accidental**: si el struct cambia, los usuarios no
  compilan codigo que dependa de campos internos — porque nunca lo
  pudieron escribir.
- **ABI breakage**: agregar campos no rompe binarios enlazados.

La proteccion es a nivel de **disciplina del equipo** y **contrato de
API**, no de seguridad criptografica. Es suficiente para la mayoria de
software.

**2. Diferencia fundamental con Rust:**

En C, la encapsulacion es **structural** (depende de la visibilidad de
la definicion del struct). Si alguien incluye el .c o copia el struct,
la barrera desaparece.

En Rust, la encapsulacion es **enforced por el compilador**: un campo
sin `pub` es inaccesible desde fuera del modulo, **sin excepciones**
(sin contar `unsafe`). No importa que el programador lea el codigo
fuente — el compilador rechaza el acceso.

```
  C:     Ocultacion por ausencia de informacion (tipo incompleto)
  Rust:  Ocultacion por reglas del compilador (visibilidad)
```

C te dice "no puedo darte la informacion". Rust te dice "tengo la
informacion pero no te la doy". El segundo es estrictamente mas fuerte.

**3. Como Rust logra encapsulacion sin heap:**

El compilador de Rust conoce la definicion completa del struct en
compile time (necesita sizeof para stack allocation). Pero las reglas
de visibilidad impiden que codigo externo **acceda** a los campos, aun
conociendo el layout.

```rust
pub struct Circle {
    radius: f64,   // privado: sizeof conocido, acceso prohibido
}

let c = Circle::new(5.0);  // vive en stack
// c.radius;                // ERROR de compilacion
```

El compilador sabe que `Circle` ocupa 8 bytes (para ponerlo en stack),
pero el sistema de tipos impide leer `radius`. En C, si sabes sizeof,
puedes hacer `memcpy` y reinterpretar — en Rust, el tipo system lo
impide.

Esto es por que Rust no necesita opaque pointers: tiene encapsulacion a
nivel de tipo sin sacrificar stack allocation, inlining, ni sizeof.

</details>
