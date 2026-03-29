# T03 — Vtable manual

---

## 1. El problema: multiples operaciones sobre multiples tipos

En T01 viste como un **function pointer** permite seleccionar una
operacion en runtime. En T02 viste como `void *` permite operar sobre
datos de cualquier tipo. Ahora el problema se combina: tienes multiples
tipos, cada uno con **multiples operaciones**, y quieres tratar todos
los tipos de forma uniforme.

```
  Un function pointer  =  UNA operacion intercambiable
  void *               =  UN dato generico

  Vtable               =  TODAS las operaciones de un tipo,
                           agrupadas en un struct de function pointers,
                           asociadas a datos de instancia via void*
```

Ejemplo concreto: un sistema de formas geometricas.

```
  Operaciones que toda forma debe tener:
  - area()
  - perimeter()
  - draw()
  - destroy()

  Tipos concretos:
  - Circulo   → implementa area, perimeter, draw, destroy a su manera
  - Rectangulo → implementa area, perimeter, draw, destroy a su manera
  - Triangulo  → implementa area, perimeter, draw, destroy a su manera
```

Con function pointers individuales, necesitarias 4 punteros sueltos por
cada instancia. Con una vtable, los agrupas en un solo struct y cada
tipo concreto tiene **una unica vtable** compartida por todas sus
instancias.

---

## 2. Anatomia de una vtable

Una vtable (virtual method table) tiene dos componentes:

1. **La tabla**: un struct de function pointers que define las
   operaciones. Todas las funciones reciben `void *self` como primer
   argumento.
2. **La instancia**: un struct que contiene los datos + un puntero a
   su vtable.

```c
// 1. La tabla de operaciones (una por "interfaz")
typedef struct {
    double (*area)(const void *self);
    double (*perimeter)(const void *self);
    void   (*draw)(const void *self);
    void   (*destroy)(void *self);
} ShapeVtable;

// 2. La "base" que toda forma incluye
typedef struct {
    const ShapeVtable *vt;   // puntero a la vtable del tipo concreto
} Shape;
```

### 2.1 Diagrama en memoria

```
  Instancia (Circle)              Vtable (compartida por TODOS los Circle)
  ┌───────────────────┐           ┌──────────────────────────┐
  │ vt ─────────────────────────→ │ area:      circle_area   │
  │ radius: 5.0       │           │ perimeter: circle_perim  │
  └───────────────────┘           │ draw:      circle_draw   │
                                  │ destroy:   circle_destroy│
  Instancia (Circle)              └──────────────────────────┘
  ┌───────────────────┐                  ↑
  │ vt ──────────────────────────────────┘
  │ radius: 3.0       │
  └───────────────────┘

  Instancia (Rect)                Vtable (compartida por TODOS los Rect)
  ┌───────────────────┐           ┌──────────────────────────┐
  │ vt ─────────────────────────→ │ area:      rect_area     │
  │ width: 4.0        │           │ perimeter: rect_perim    │
  │ height: 6.0       │           │ draw:      rect_draw     │
  └───────────────────┘           │ destroy:   rect_destroy  │
                                  └──────────────────────────┘
```

Observa:
- Las dos instancias de Circle comparten **la misma vtable**.
- La vtable se define como `static const` — una sola copia en memoria.
- La instancia de Rect apunta a una vtable distinta con sus propias
  implementaciones.

### 2.2 Por que un puntero a vtable (no una copia)

Cada instancia tiene un **puntero** a la vtable, no una copia de la
vtable completa. Esto es critico:

| Enfoque | Tamano por instancia | Si cambias la vtable |
|---------|---------------------|---------------------|
| Copia de vtable en cada instancia | `N * sizeof(fn_ptr)` | Hay que actualizar todas las instancias |
| Puntero a vtable | `sizeof(void *)` = 8 bytes | Un solo lugar |

Con 4 operaciones, copiar la vtable costaria 32 bytes por instancia.
Con puntero, solo 8 bytes. C++ y Rust usan exactamente esta
optimizacion: un puntero a vtable (vptr) por instancia.

---

## 3. Implementacion completa: Shape

### 3.1 La interfaz (header publico)

```c
// shape.h — la "interfaz" que todos los tipos implementan

#ifndef SHAPE_H
#define SHAPE_H

typedef struct ShapeVtable ShapeVtable;
typedef struct Shape Shape;

struct ShapeVtable {
    double (*area)(const void *self);
    double (*perimeter)(const void *self);
    void   (*draw)(const void *self);
    void   (*destroy)(void *self);
};

struct Shape {
    const ShapeVtable *vt;
};

// API generica — opera sobre cualquier Shape
static inline double shape_area(const Shape *s) {
    return s->vt->area(s);
}

static inline double shape_perimeter(const Shape *s) {
    return s->vt->perimeter(s);
}

static inline void shape_draw(const Shape *s) {
    s->vt->draw(s);
}

static inline void shape_destroy(Shape *s) {
    s->vt->destroy(s);
}

#endif
```

Las funciones `shape_area`, `shape_draw`, etc. son **wrappers** que
simplemente despachan a traves de la vtable. El llamador no necesita
saber como funciona la vtable — solo llama `shape_area(s)`.

### 3.2 Tipo concreto: Circle

```c
// circle.h
#ifndef CIRCLE_H
#define CIRCLE_H

#include "shape.h"

typedef struct {
    Shape base;      // DEBE ser el primer campo
    double radius;
} Circle;

Circle *circle_create(double radius);

#endif
```

```c
// circle.c
#include "circle.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return M_PI * c->radius * c->radius;
}

static double circle_perimeter(const void *self) {
    const Circle *c = (const Circle *)self;
    return 2.0 * M_PI * c->radius;
}

static void circle_draw(const void *self) {
    const Circle *c = (const Circle *)self;
    printf("Circle(r=%.1f)\n", c->radius);
}

static void circle_destroy(void *self) {
    free(self);
}

// LA vtable — una sola instancia, compartida por todos los Circle
static const ShapeVtable circle_vtable = {
    .area      = circle_area,
    .perimeter = circle_perimeter,
    .draw      = circle_draw,
    .destroy   = circle_destroy,
};

Circle *circle_create(double radius) {
    Circle *c = malloc(sizeof(Circle));
    c->base.vt = &circle_vtable;   // cablear la vtable
    c->radius = radius;
    return c;
}
```

### 3.3 Tipo concreto: Rect

```c
// rect.h
#ifndef RECT_H
#define RECT_H

#include "shape.h"

typedef struct {
    Shape base;
    double width;
    double height;
} Rect;

Rect *rect_create(double width, double height);

#endif
```

```c
// rect.c
#include "rect.h"
#include <stdio.h>
#include <stdlib.h>

static double rect_area(const void *self) {
    const Rect *r = (const Rect *)self;
    return r->width * r->height;
}

static double rect_perimeter(const void *self) {
    const Rect *r = (const Rect *)self;
    return 2.0 * (r->width + r->height);
}

static void rect_draw(const void *self) {
    const Rect *r = (const Rect *)self;
    printf("Rect(%.1f x %.1f)\n", r->width, r->height);
}

static void rect_destroy(void *self) {
    free(self);
}

static const ShapeVtable rect_vtable = {
    .area      = rect_area,
    .perimeter = rect_perimeter,
    .draw      = rect_draw,
    .destroy   = rect_destroy,
};

Rect *rect_create(double width, double height) {
    Rect *r = malloc(sizeof(Rect));
    r->base.vt = &rect_vtable;
    r->width = width;
    r->height = height;
    return r;
}
```

### 3.4 Uso polimorfico

```c
#include "shape.h"
#include "circle.h"
#include "rect.h"

int main(void) {
    // Crear formas de distintos tipos
    Shape *shapes[3];
    shapes[0] = (Shape *)circle_create(5.0);
    shapes[1] = (Shape *)rect_create(4.0, 6.0);
    shapes[2] = (Shape *)circle_create(3.0);

    // Tratarlas uniformemente — polimorfismo
    for (int i = 0; i < 3; i++) {
        shape_draw(shapes[i]);
        printf("  area = %.2f\n", shape_area(shapes[i]));
        printf("  perimeter = %.2f\n", shape_perimeter(shapes[i]));
    }

    // Limpiar — cada tipo sabe como destruirse
    for (int i = 0; i < 3; i++) {
        shape_destroy(shapes[i]);
    }
}
```

Salida:

```
Circle(r=5.0)
  area = 78.54
  perimeter = 31.42
Rect(4.0 x 6.0)
  area = 24.00
  perimeter = 20.00
Circle(r=3.0)
  area = 28.27
  perimeter = 18.85
```

El array `shapes` contiene punteros a `Shape *`. Cada llamada a
`shape_area` despacha a la funcion correcta (`circle_area` o
`rect_area`) sin que `main` sepa que tipo concreto hay detras.

---

## 4. Por que Shape debe ser el primer campo

El cast `(Shape *)circle_ptr` funciona **solo si** `Shape base` es el
primer campo del struct. Esto es porque C garantiza que la direccion de
un struct coincide con la direccion de su primer campo:

```
  Circle en memoria:

  Direccion 0x1000:
  ┌───────────────────────────────────────────┐
  │ base.vt (8 bytes)  │ radius (8 bytes)     │
  └───────────────────────────────────────────┘
  ↑                     ↑
  &circle               &circle->radius
  == &circle->base      (offset 8)
  (offset 0)

  (Shape *)circle == &circle->base  ← misma direccion
```

Si `base` no fuera el primer campo, el cast no apuntaria a la vtable:

```c
// MAL: base no es el primer campo
typedef struct {
    double radius;    // offset 0
    Shape base;       // offset 8 ← (Shape *)circle apuntaria a radius!
} BadCircle;
```

En C++, el compilador genera los offsets automaticamente (incluso con
herencia multiple). En C, usamos la convencion de "base como primer
campo" porque es la unica forma segura y portable de simular herencia.

---

## 5. El flujo de una llamada virtual

Cuando llamas `shape_area(shapes[1])`, la ejecucion sigue este camino:

```
  1. shapes[1]                → puntero a Rect (como Shape*)
  2. shapes[1]->vt            → puntero a rect_vtable
  3. shapes[1]->vt->area      → puntero a rect_area
  4. shapes[1]->vt->area(shapes[1])  → invoca rect_area con self

  En codigo:
  shape_area(s)
    → s->vt->area(s)
    → rect_vtable.area(s)
    → rect_area(s)
    → cast a Rect*, calcular width * height
```

Esto es **dispatch dinamico**: la funcion que se ejecuta se decide
en runtime, no en compile time. El "costo" son dos indirecciones de
puntero: una para llegar a la vtable, otra para llegar a la funcion.

```
  Dispatch directo (compile time):

  call rect_area          ← el compilador sabe que funcion llamar

  Dispatch dinamico (runtime):

  load  vt = s->vt        ← indirection 1: leer puntero a vtable
  load  fn = vt->area     ← indirection 2: leer puntero a funcion
  call  fn(s)             ← invocar
```

---

## 6. Vtable interna vs vtable externa

Hay dos formas de asociar la vtable con la instancia:

### 6.1 Vtable interna (la que usamos)

El puntero a vtable vive **dentro** del struct de la instancia.

```c
typedef struct {
    const ShapeVtable *vt;   // dentro de la instancia
    double radius;
} Circle;
```

Ventaja: cada instancia carga su propia vtable. Puedes tener dos
Circles con vtables distintas (ej. una version "debug" que loguea).

Desventaja: cada instancia ocupa 8 bytes extra (el puntero a vtable).

### 6.2 Vtable externa (fat pointer)

La vtable viaja **junto** con el puntero, no dentro de la instancia.

```c
// Fat pointer: datos + vtable viajan juntos
typedef struct {
    void              *data;  // puntero a los datos
    const ShapeVtable *vt;    // puntero a la vtable
} ShapeFat;

// Crear un fat pointer
ShapeFat shape_from_circle(Circle *c) {
    return (ShapeFat){ .data = c, .vt = &circle_vtable };
}

// Uso:
double a = shape_fat.vt->area(shape_fat.data);
```

```
  Vtable interna:                Vtable externa (fat pointer):

  ┌────────┐                     ShapeFat
  │ vt ──────→ vtable            ┌────────────┐
  │ radius │                     │ data ──────────→ Circle { radius }
  └────────┘                     │ vt ────────────→ vtable
                                 └────────────┘

  Tamano instancia: 16 bytes     Tamano instancia: 8 bytes (sin vt)
  Tamano puntero: 8 bytes        Tamano puntero: 16 bytes (fat)
```

| Aspecto | Vtable interna | Vtable externa (fat) |
|---------|---------------|---------------------|
| Tamano del puntero | 8 bytes (normal) | 16 bytes (fat) |
| Tamano de la instancia | +8 bytes (vt) | sin overhead |
| Quien sabe el tipo | La instancia misma | El puntero |
| Cambiar vtable en runtime | Facil (cambiar vt) | Requiere nuevo fat ptr |
| Equivalente | C++ vptr | Rust `dyn Trait` |

Rust usa fat pointers para `dyn Trait`: un `&dyn Shape` son 16 bytes
(puntero a datos + puntero a vtable). C++ usa vtable interna (vptr
dentro del objeto). Ambos son validos — la seccion S02 de este capitulo
cubre como Rust lo hace.

---

## 7. Agregar un tipo nuevo

La potencia del patron: agregar un tipo nuevo no requiere modificar
ningun codigo existente. Solo creas un nuevo archivo.

```c
// triangle.c — tipo completamente nuevo
#include "shape.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    Shape base;
    double a, b, c;  // lados
} Triangle;

static double triangle_area(const void *self) {
    const Triangle *t = (const Triangle *)self;
    double s = (t->a + t->b + t->c) / 2.0;
    return sqrt(s * (s - t->a) * (s - t->b) * (s - t->c));
}

static double triangle_perimeter(const void *self) {
    const Triangle *t = (const Triangle *)self;
    return t->a + t->b + t->c;
}

static void triangle_draw(const void *self) {
    const Triangle *t = (const Triangle *)self;
    printf("Triangle(%.1f, %.1f, %.1f)\n", t->a, t->b, t->c);
}

static void triangle_destroy(void *self) { free(self); }

static const ShapeVtable triangle_vtable = {
    .area      = triangle_area,
    .perimeter = triangle_perimeter,
    .draw      = triangle_draw,
    .destroy   = triangle_destroy,
};

Triangle *triangle_create(double a, double b, double c) {
    Triangle *t = malloc(sizeof(Triangle));
    t->base.vt = &triangle_vtable;
    t->a = a;  t->b = b;  t->c = c;
    return t;
}
```

Ningun cambio en `shape.h`, `circle.c`, `rect.c`, ni en `main`. El
nuevo tipo se "conecta" al sistema solo con una vtable que implementa
las mismas operaciones.

Esto es el **Open/Closed Principle** (SOLID): abierto para extension,
cerrado para modificacion. En C++ se logra con herencia y metodos
virtuales. En Rust, con traits. En C, con vtables manuales.

---

## 8. Patrones de vtable en la practica

### 8.1 Vtable con nombre de tipo

Es util agregar un campo `name` o `type_name` para debugging:

```c
typedef struct {
    const char *type_name;
    double (*area)(const void *self);
    double (*perimeter)(const void *self);
    void   (*draw)(const void *self);
    void   (*destroy)(void *self);
} ShapeVtable;

static const ShapeVtable circle_vtable = {
    .type_name = "Circle",
    .area      = circle_area,
    // ...
};

// Ahora puedes hacer:
printf("Tipo: %s\n", s->vt->type_name);
```

### 8.2 Vtable con tamano de instancia

Util para clonar o serializar sin conocer el tipo:

```c
typedef struct {
    size_t instance_size;          // sizeof del tipo concreto
    void *(*clone)(const void *self);  // deep copy
    // ... demas operaciones
} ShapeVtable;

static const ShapeVtable circle_vtable = {
    .instance_size = sizeof(Circle),
    .clone         = circle_clone,
    // ...
};

// Clone generico (shallow):
void *generic_clone(const Shape *s) {
    void *copy = malloc(s->vt->instance_size);
    memcpy(copy, s, s->vt->instance_size);
    return copy;
}
```

### 8.3 Operaciones opcionales (NULL check)

No todos los tipos necesitan implementar todas las operaciones. Marca
las opcionales como NULL y verifica antes de invocar:

```c
static const ShapeVtable point_vtable = {
    .area      = NULL,           // un punto no tiene area
    .perimeter = NULL,           // ni perimetro
    .draw      = point_draw,
    .destroy   = point_destroy,
};

static inline double shape_area(const Shape *s) {
    if (s->vt->area)
        return s->vt->area(s);
    return 0.0;  // valor por defecto si no implementa
}
```

Esto es analogo a los metodos con implementacion por defecto en Rust
traits:

```rust
trait Shape {
    fn area(&self) -> f64 { 0.0 }  // default, puede ser override
    fn draw(&self);                 // requerido
}
```

---

## 9. Vtable vs otras formas de dispatch

| Mecanismo | Que resuelve | Costo |
|-----------|-------------|-------|
| Switch/if-else | Dispatch por valor de enum | O(n) ramas, no extensible |
| Dispatch table (T01) | UNA operacion por indice | O(1) pero solo una funcion |
| Function pointer en struct (T01) | UNA operacion por instancia | Un puntero por operacion |
| **Vtable** | N operaciones por tipo | Un puntero por instancia + tabla compartida |

La vtable es la evolucion natural:
```
  T01: function pointer     → UNA funcion intercambiable
  T01: fn ptr en struct     → UNA funcion por instancia
  T03: vtable               → TODAS las funciones, compartidas por tipo
```

---

## 10. Como C++ y Rust implementan esto

### 10.1 C++ (automatico, vtable interna)

```cpp
class Shape {
public:
    virtual double area() const = 0;
    virtual void draw() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double radius;
public:
    Circle(double r) : radius(r) {}
    double area() const override { return M_PI * radius * radius; }
    void draw() const override { /* ... */ }
};
```

El compilador genera automaticamente:
- Un vptr (puntero a vtable) como primer campo oculto del objeto
- Una vtable global con punteros a `circle_area`, `circle_draw`, etc.
- El dispatch `shape->area()` se traduce a `shape->vptr->area(shape)`

Es exactamente lo que hicimos a mano en la seccion 3.

### 10.2 Rust (automatico, vtable externa)

```rust
trait Shape {
    fn area(&self) -> f64;
    fn draw(&self);
}

struct Circle { radius: f64 }

impl Shape for Circle {
    fn area(&self) -> f64 { std::f64::consts::PI * self.radius.powi(2) }
    fn draw(&self) { println!("Circle(r={})", self.radius); }
}

// Dispatch dinamico via fat pointer:
fn print_shape(s: &dyn Shape) {
    s.draw();
    println!("  area = {:.2}", s.area());
}
```

`&dyn Shape` es un fat pointer de 16 bytes: `(data_ptr, vtable_ptr)`.
La vtable contiene punteros a `Circle::area`, `Circle::draw`, mas
`drop` y metadata de tamano/alignment.

### 10.3 Tabla comparativa del mecanismo

```
  C (manual):                          C++ (automatico):
  ─────────                            ──────────────
  ShapeVtable { area, draw, ... }      vtable generada por compilador
  Shape { const ShapeVtable *vt }      vptr oculto en el objeto
  shape->vt->area(shape)               shape->area()  ← sugar
  circle_vtable = { ... }              vtable_Circle[] = { ... }

  Rust (automatico, fat pointer):
  ─────────────────────────────
  trait Shape { fn area, fn draw }     trait → vtable layout
  &dyn Shape = (data_ptr, vt_ptr)      fat pointer
  s.area()                             → vt_ptr->area(data_ptr)
```

| Aspecto | C manual | C++ | Rust dyn |
|---------|---------|-----|----------|
| Vtable | Definida por el programador | Generada por compilador | Generada por compilador |
| Ubicacion del vptr | Primer campo (convencion) | Primer campo (oculto) | En el fat pointer |
| Type safety | Ninguna (void* casts) | Parcial (herencia permite errores) | Completa |
| Herencia multiple | Posible pero compleja | Soportada (con offsets) | No (traits, no herencia) |
| Tamano puntero | 8 bytes | 8 bytes | 16 bytes (fat) |
| Tamano instancia | +8 bytes (vptr) | +8 bytes (vptr) | Sin overhead |

---

## Errores comunes

### Error 1 — Olvidar cablear la vtable en el constructor

```c
Circle *circle_create(double radius) {
    Circle *c = malloc(sizeof(Circle));
    // OLVIDO: c->base.vt = &circle_vtable;
    c->radius = radius;
    return c;
}

// Mas tarde:
shape_area((Shape *)c);
// c->vt es NULL o basura → SEGFAULT al acceder a vt->area
```

Toda funcion constructora **debe** asignar el puntero a vtable. Es el
equivalente de olvidar llamar al constructor base en C++.

### Error 2 — Base no es el primer campo

```c
typedef struct {
    double radius;     // campo de datos primero
    Shape base;        // vtable despues
} BadCircle;

BadCircle *bc = bad_circle_create(5.0);
Shape *s = (Shape *)bc;
// s->vt ahora apunta a los bits de radius, no a la vtable
// → SEGFAULT o corrupcion
```

**Regla**: `Shape base` siempre debe ser el primer campo del struct.

### Error 3 — Cast incorrecto en la implementacion

```c
static double circle_area(const void *self) {
    const Rect *r = (const Rect *)self;  // BUG: castea a Rect, no Circle
    return r->width * r->height;         // lee datos incorrectos
}
```

El compilador no detecta este error — `void *` acepta cualquier cast.
La disciplina depende del programador: cada implementacion solo castea
a su propio tipo.

### Error 4 — Vtable no es static const

```c
// MAL: vtable como variable local
Circle *circle_create(double radius) {
    ShapeVtable vt = { .area = circle_area, /* ... */ };
    Circle *c = malloc(sizeof(Circle));
    c->base.vt = &vt;   // apunta a variable local del stack
    return c;            // vt se destruye → dangling pointer
}
```

La vtable debe ser `static const`: vive durante toda la ejecucion
del programa, y es compartida por todas las instancias del tipo.

```c
// BIEN:
static const ShapeVtable circle_vtable = { ... };
```

### Error 5 — No implementar destroy

```c
static const ShapeVtable circle_vtable = {
    .area      = circle_area,
    .perimeter = circle_perimeter,
    .draw      = circle_draw,
    .destroy   = NULL,            // "ya lo libero yo despues"
};

// En la funcion generica de limpieza:
shape_destroy(s);
// → s->vt->destroy(s)
// → NULL(s) → SEGFAULT
```

Si hay `destroy` en la vtable, debe estar implementado. La alternativa
es verificar NULL (seccion 8.3), pero es error-prone.

---

## Ejercicios

### Ejercicio 1 — Diagrama de vtable

Dado este codigo, dibuja el diagrama de memoria (similar al de la
seccion 2.1) mostrando las instancias y sus vtables:

```c
Shape *a = (Shape *)circle_create(2.0);
Shape *b = (Shape *)rect_create(3.0, 4.0);
Shape *c = (Shape *)circle_create(7.0);
```

**Prediccion**: Cuantas vtables existen en memoria? Cuantos punteros
a vtable? Son las vtables de `a` y `c` la misma direccion?

<details><summary>Respuesta</summary>

Existen **2 vtables** en memoria: `circle_vtable` y `rect_vtable`.
Hay **3 punteros** a vtable (uno por instancia).
`a->vt` y `c->vt` apuntan a la **misma direccion** (`&circle_vtable`).

```
  a (Circle)                    circle_vtable (static const)
  ┌──────────────┐              ┌─────────────────────────┐
  │ vt ────────────────────────→│ area:  circle_area      │
  │ radius: 2.0  │      ┌─────→│ perim: circle_perimeter  │
  └──────────────┘      │      │ draw:  circle_draw       │
                        │      │ dest:  circle_destroy    │
  c (Circle)            │      └─────────────────────────┘
  ┌──────────────┐      │
  │ vt ──────────────────┘
  │ radius: 7.0  │              rect_vtable (static const)
  └──────────────┘              ┌─────────────────────────┐
                         ┌─────→│ area:  rect_area        │
  b (Rect)               │      │ perim: rect_perimeter    │
  ┌──────────────┐      │      │ draw:  rect_draw         │
  │ vt ──────────────────┘      │ dest:  rect_destroy     │
  │ width: 3.0   │              └─────────────────────────┘
  │ height: 4.0  │
  └──────────────┘

  Total memoria vtables: 2 * sizeof(ShapeVtable) = 2 * 32 = 64 bytes
  Total vptrs: 3 * 8 = 24 bytes
```

Si tuvieras 1000 Circles, seguirian siendo 2 vtables. Solo agregarias
1000 punteros de 8 bytes. La vtable escala O(tipos), no O(instancias).

</details>

---

### Ejercicio 2 — Implementar un tipo nuevo

Agrega un tipo `Ellipse` al sistema de formas. Campos: `double a, b`
(semi-ejes). Formulas:

- `area = pi * a * b`
- `perimeter ≈ pi * (3(a+b) - sqrt((3a+b)(a+3b)))` (Ramanujan)

Solo necesitas crear el .h y .c — no modifiques `shape.h`.

**Prediccion**: Cuantas lineas de codigo existente necesitas cambiar?

<details><summary>Respuesta</summary>

**Cero lineas de codigo existente cambian.** Solo se agregan archivos
nuevos:

```c
// ellipse.h
#ifndef ELLIPSE_H
#define ELLIPSE_H
#include "shape.h"

typedef struct {
    Shape base;
    double a, b;
} Ellipse;

Ellipse *ellipse_create(double a, double b);
#endif
```

```c
// ellipse.c
#include "ellipse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static double ellipse_area(const void *self) {
    const Ellipse *e = (const Ellipse *)self;
    return M_PI * e->a * e->b;
}

static double ellipse_perimeter(const void *self) {
    const Ellipse *e = (const Ellipse *)self;
    double a = e->a, b = e->b;
    return M_PI * (3.0*(a+b) - sqrt((3.0*a+b)*(a+3.0*b)));
}

static void ellipse_draw(const void *self) {
    const Ellipse *e = (const Ellipse *)self;
    printf("Ellipse(a=%.1f, b=%.1f)\n", e->a, e->b);
}

static void ellipse_destroy(void *self) { free(self); }

static const ShapeVtable ellipse_vtable = {
    .area      = ellipse_area,
    .perimeter = ellipse_perimeter,
    .draw      = ellipse_draw,
    .destroy   = ellipse_destroy,
};

Ellipse *ellipse_create(double a, double b) {
    Ellipse *e = malloc(sizeof(Ellipse));
    e->base.vt = &ellipse_vtable;
    e->a = a;
    e->b = b;
    return e;
}
```

Uso:

```c
Shape *s = (Shape *)ellipse_create(5.0, 3.0);
shape_draw(s);            // Ellipse(a=5.0, b=3.0)
printf("%.2f\n", shape_area(s));  // 47.12
shape_destroy(s);
```

El sistema es **abierto para extension**: agregar un tipo nuevo es
agregar archivos, no modificar existentes.

</details>

---

### Ejercicio 3 — Contenedor polimorfico

Escribe una funcion `total_area` que recibe un array de `Shape *` y
retorna la suma de todas las areas:

```c
double total_area(Shape **shapes, int n);
```

Luego escribe `largest_shape` que retorna el puntero al Shape con mayor
area.

**Prediccion**: Podrias implementar estas funciones sin saber que tipos
concretos existen? Eso es polimorfismo.

<details><summary>Respuesta</summary>

```c
double total_area(Shape **shapes, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += shape_area(shapes[i]);
    }
    return sum;
}

Shape *largest_shape(Shape **shapes, int n) {
    if (n == 0) return NULL;
    Shape *max = shapes[0];
    double max_area = shape_area(max);
    for (int i = 1; i < n; i++) {
        double a = shape_area(shapes[i]);
        if (a > max_area) {
            max_area = a;
            max = shapes[i];
        }
    }
    return max;
}

// Uso:
Shape *shapes[] = {
    (Shape *)circle_create(5.0),
    (Shape *)rect_create(4.0, 6.0),
    (Shape *)circle_create(3.0),
};

printf("Total: %.2f\n", total_area(shapes, 3));
// 78.54 + 24.00 + 28.27 = 130.81

Shape *big = largest_shape(shapes, 3);
shape_draw(big);  // Circle(r=5.0) — el de mayor area
```

Si, estas funciones no mencionan ni `Circle` ni `Rect`. Solo usan la
interfaz `Shape`. Si manana agregas `Ellipse`, estas funciones siguen
funcionando sin cambios.

En Rust:

```rust
fn total_area(shapes: &[&dyn Shape]) -> f64 {
    shapes.iter().map(|s| s.area()).sum()
}
```

Mismo concepto, misma genericidad — pero con type safety completa.

</details>

---

### Ejercicio 4 — Vtable con clone

Agrega una operacion `clone` a la `ShapeVtable`:

```c
void *(*clone)(const void *self);
```

Implementa `clone` para Circle y Rect. Luego escribe una funcion
generica `shape_clone` que retorna una copia deep del shape.

**Prediccion**: Para Circle (un solo campo numerico), clone es trivial.
Para un tipo que contuviera un `char *name` con malloc, que necesitaria
el clone?

<details><summary>Respuesta</summary>

```c
// Actualizar ShapeVtable:
typedef struct {
    double (*area)(const void *self);
    double (*perimeter)(const void *self);
    void   (*draw)(const void *self);
    void  *(*clone)(const void *self);   // nuevo
    void   (*destroy)(void *self);
} ShapeVtable;

// circle.c
static void *circle_clone(const void *self) {
    const Circle *src = (const Circle *)self;
    return circle_create(src->radius);  // reutiliza el constructor
}

// rect.c
static void *rect_clone(const void *self) {
    const Rect *src = (const Rect *)self;
    return rect_create(src->width, src->height);
}

// API generica:
static inline Shape *shape_clone(const Shape *s) {
    return (Shape *)s->vt->clone(s);
}

// Uso:
Shape *original = (Shape *)circle_create(5.0);
Shape *copia = shape_clone(original);

shape_draw(original);  // Circle(r=5.0)
shape_draw(copia);     // Circle(r=5.0) — copia independiente

shape_destroy(original);
shape_draw(copia);     // sigue funcionando — es independiente
shape_destroy(copia);
```

Si un tipo tuviera `char *name` con malloc:

```c
typedef struct {
    Shape base;
    char *name;    // heap-allocated
    double value;
} NamedShape;

static void *named_clone(const void *self) {
    const NamedShape *src = (const NamedShape *)self;
    NamedShape *copy = malloc(sizeof(NamedShape));
    copy->base.vt = src->base.vt;
    copy->name = strdup(src->name);  // deep copy del string
    copy->value = src->value;
    return copy;
}

static void named_destroy(void *self) {
    NamedShape *ns = (NamedShape *)self;
    free(ns->name);    // liberar el string
    free(ns);          // liberar el struct
}
```

Cada tipo implementa su propio `clone` sabiendo que datos necesitan
copia profunda. Es el equivalente manual de `impl Clone for T` en Rust.

</details>

---

### Ejercicio 5 — Vtable con to_string

Agrega una operacion `to_string` que retorna una representacion textual
(el llamador debe liberar el string):

```c
char *(*to_string)(const void *self);
```

Implementa para Circle (ej. `"Circle(r=5.0, area=78.54)"`) y Rect.

**Prediccion**: Que funcion de C usarias para crear un string formateado
de longitud variable? Quien es responsable de liberar el string retornado?

<details><summary>Respuesta</summary>

```c
// circle.c
static char *circle_to_string(const void *self) {
    const Circle *c = (const Circle *)self;
    char *buf;
    int len = asprintf(&buf, "Circle(r=%.1f, area=%.2f)",
                       c->radius, M_PI * c->radius * c->radius);
    if (len < 0) return NULL;
    return buf;  // llamador debe hacer free(buf)
}

// rect.c
static char *rect_to_string(const void *self) {
    const Rect *r = (const Rect *)self;
    char *buf;
    asprintf(&buf, "Rect(%.1f x %.1f, area=%.2f)",
             r->width, r->height, r->width * r->height);
    return buf;
}

// API generica:
static inline char *shape_to_string(const Shape *s) {
    return s->vt->to_string(s);
}

// Uso:
Shape *s = (Shape *)circle_create(5.0);
char *desc = shape_to_string(s);
printf("%s\n", desc);   // Circle(r=5.0, area=78.54)
free(desc);             // el llamador libera
shape_destroy(s);
```

`asprintf` (POSIX, no C estandar) hace malloc interno y retorna el
string formateado. La alternativa portable es `snprintf` con dos
pasadas:

```c
int len = snprintf(NULL, 0, "Circle(r=%.1f)", c->radius);
char *buf = malloc(len + 1);
snprintf(buf, len + 1, "Circle(r=%.1f)", c->radius);
```

El string retornado es ownership del llamador — **TAKE** semantics.
En Rust, esto seria retornar `String` (owned).

</details>

---

### Ejercicio 6 — Multiple dispatch (dos interfaces)

Un tipo puede implementar mas de una "interfaz". Crea una segunda
vtable `Printable` con una sola operacion `print`:

```c
typedef struct {
    void (*print)(const void *self, FILE *out);
} PrintableVtable;
```

Haz que Circle implemente tanto `ShapeVtable` como `PrintableVtable`.

**Prediccion**: Puede un struct tener dos punteros a vtable? Como se
organizan los campos?

<details><summary>Respuesta</summary>

```c
// Dos "interfaces"
typedef struct {
    double (*area)(const void *self);
    void   (*destroy)(void *self);
} ShapeVtable;

typedef struct {
    void (*print)(const void *self, FILE *out);
} PrintableVtable;

// Circle implementa ambas
typedef struct {
    const ShapeVtable     *shape_vt;
    const PrintableVtable *print_vt;
    double radius;
} Circle;

static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return M_PI * c->radius * c->radius;
}

static void circle_destroy(void *self) { free(self); }

static void circle_print(const void *self, FILE *out) {
    const Circle *c = (const Circle *)self;
    fprintf(out, "Circle(r=%.1f)\n", c->radius);
}

static const ShapeVtable circle_shape_vt = {
    .area = circle_area,
    .destroy = circle_destroy,
};

static const PrintableVtable circle_print_vt = {
    .print = circle_print,
};

Circle *circle_create(double radius) {
    Circle *c = malloc(sizeof(Circle));
    c->shape_vt = &circle_shape_vt;
    c->print_vt = &circle_print_vt;
    c->radius = radius;
    return c;
}

// Uso como Shape (cast al primer campo):
Shape *s = (Shape *)circle_create(5.0);
shape_area(s);

// Uso como Printable (necesitas acceso directo):
Circle *c = circle_create(3.0);
c->print_vt->print(c, stdout);
```

Si, un struct puede tener multiples vtable pointers. Cada uno le da
acceso a una "interfaz" distinta. Esto es herencia multiple de
interfaces.

El problema: solo el **primer** puntero a vtable funciona con cast
directo (`(Shape *)circle`). Para el segundo, necesitas cast manual
con offset o acceso via el tipo concreto. C++ resuelve esto con
thunks y ajustes de offset automaticos.

En Rust, un tipo implementa multiples traits sin problema:

```rust
impl Shape for Circle { /* ... */ }
impl Display for Circle { /* ... */ }

// Pero &dyn Shape solo tiene la vtable de Shape
// No puedes hacer &dyn (Shape + Display) excepto con supertraits
```

</details>

---

### Ejercicio 7 — Encontrar bugs en vtables

Cada fragmento tiene un bug. Identificalo:

```c
// Bug A
typedef struct {
    double x, y;
    Shape base;         // base NO es primer campo
} Point;
Shape *s = (Shape *)point_create(1.0, 2.0);
shape_draw(s);

// Bug B
Circle *circle_create(double r) {
    Circle c;
    c.base.vt = &circle_vtable;
    c.radius = r;
    return &c;
}

// Bug C
static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return c->radius * c->radius;  // olvido multiplicar por PI
}
// (este no causa crash — ¿por que es un bug mas sutil?)
```

**Prediccion**: Cual es crash inmediato, cual es datos corruptos, y
cual pasa los tests pero produce resultados incorrectos?

<details><summary>Respuesta</summary>

**Bug A: Crash inmediato.**

`Shape base` no es el primer campo. `(Shape *)point` apunta a `x` (un
double), no a la vtable. `s->vt` lee los bits de `x` como si fueran
una direccion de vtable → SEGFAULT al intentar acceder a `vt->draw`.

**Fix**: `Shape base` debe ser el primer campo.

**Bug B: Crash diferido (dangling pointer).**

`Circle c` es una variable local — vive en el stack de `circle_create`.
Retornar `&c` da un puntero a memoria que se destruye al salir de la
funcion. El puntero puede funcionar "por suerte" si el stack no se
reutiliza inmediatamente, pero cualquier llamada posterior lo
sobrescribe.

**Fix**: `Circle *c = malloc(sizeof(Circle));`

**Bug C: Logicamente incorrecto, nunca crashea.**

La funcion retorna `r * r` en vez de `pi * r * r`. El programa
funciona, no crashea, los tipos son correctos. Pero el resultado
esta mal. Este es el tipo de bug que los tests deben atrapar — la
vtable lo despacha correctamente, la implementacion es la que falla.

Es un bug mas sutil porque:
- No hay crash ni warning
- El mecanismo de vtable funciona perfecto
- Solo un test con valor esperado (`assert(area == M_PI * 25.0)`) lo
  detecta
- En un sistema grande, podria pasar desapercibido meses

</details>

---

### Ejercicio 8 — Vtable minima: Comparator

Crea una vtable minima para objetos comparables:

```c
typedef struct {
    int (*compare)(const void *self, const void *other);
    void (*print)(const void *self);
} ComparableVtable;
```

Implementa para `int` (wrapped en struct) y para `char *` (string).
Luego escribe un `generic_max` que recibe dos `Comparable *` y retorna
el mayor.

**Prediccion**: El comparador recibe `void *other`. Como verificas
que `other` es del mismo tipo que `self`?

<details><summary>Respuesta</summary>

```c
typedef struct {
    int (*compare)(const void *self, const void *other);
    void (*print)(const void *self);
} ComparableVtable;

typedef struct {
    const ComparableVtable *vt;
} Comparable;

// --- IntBox ---
typedef struct {
    Comparable base;
    int value;
} IntBox;

static int intbox_compare(const void *self, const void *other) {
    const IntBox *a = (const IntBox *)self;
    const IntBox *b = (const IntBox *)other;
    return (a->value > b->value) - (a->value < b->value);
}

static void intbox_print(const void *self) {
    const IntBox *a = (const IntBox *)self;
    printf("%d", a->value);
}

static const ComparableVtable intbox_vt = {
    .compare = intbox_compare,
    .print   = intbox_print,
};

IntBox intbox(int v) {
    return (IntBox){ .base.vt = &intbox_vt, .value = v };
}

// --- StrBox ---
typedef struct {
    Comparable base;
    const char *str;
} StrBox;

static int strbox_compare(const void *self, const void *other) {
    const StrBox *a = (const StrBox *)self;
    const StrBox *b = (const StrBox *)other;
    return strcmp(a->str, b->str);
}

static void strbox_print(const void *self) {
    const StrBox *a = (const StrBox *)self;
    printf("%s", a->str);
}

static const ComparableVtable strbox_vt = {
    .compare = strbox_compare,
    .print   = strbox_print,
};

StrBox strbox(const char *s) {
    return (StrBox){ .base.vt = &strbox_vt, .str = s };
}

// --- Generic max ---
Comparable *generic_max(Comparable *a, Comparable *b) {
    return a->vt->compare(a, b) >= 0 ? a : b;
}

// Uso:
IntBox a = intbox(42), b = intbox(17);
Comparable *max_int = generic_max((Comparable *)&a, (Comparable *)&b);
max_int->vt->print(max_int);  // 42

StrBox x = strbox("zebra"), y = strbox("apple");
Comparable *max_str = generic_max((Comparable *)&x, (Comparable *)&y);
max_str->vt->print(max_str);  // zebra
```

**Sobre verificar tipos iguales**: en C puro, **no puedes**. Si alguien
pasa un `IntBox` y un `StrBox` a `generic_max`, `intbox_compare`
casteara el `StrBox` a `IntBox` y leera basura. No hay crash — solo
resultado incorrecto.

Mitigaciones:
- Agregar un campo `type_id` a la vtable y verificar en `compare`
- Documentar que ambos argumentos deben ser del mismo tipo
- Aceptar el riesgo (comun en C de produccion)

En Rust, el compilador lo impide:

```rust
fn max<T: Ord>(a: &T, b: &T) -> &T { ... }
// max(&42, &"hello")  → error: expected i32, found &str
```

</details>

---

### Ejercicio 9 — Reconstruir: que hace este codigo?

Lee el codigo sin ejecutarlo y explica que imprime:

```c
typedef struct { void (*speak)(const void *); } AnimalVt;
typedef struct { const AnimalVt *vt; } Animal;

void dog_speak(const void *s) { (void)s; printf("Woof!\n"); }
void cat_speak(const void *s) { (void)s; printf("Meow!\n"); }

static const AnimalVt dog_vt = { .speak = dog_speak };
static const AnimalVt cat_vt = { .speak = cat_speak };

typedef struct { Animal base; const char *name; } Dog;
typedef struct { Animal base; int lives; } Cat;

int main(void) {
    Dog rex = { .base.vt = &dog_vt, .name = "Rex" };
    Cat mia = { .base.vt = &cat_vt, .lives = 9 };
    Dog max = { .base.vt = &dog_vt, .name = "Max" };

    Animal *zoo[] = { (Animal *)&rex, (Animal *)&mia, (Animal *)&max };

    for (int i = 0; i < 3; i++)
        zoo[i]->vt->speak(zoo[i]);
}
```

**Prediccion**: Que imprime? Cuantas vtables hay? Que pasa si cambias
`max.base.vt = &cat_vt` — es un error o funcionaria?

<details><summary>Respuesta</summary>

**Salida**:

```
Woof!
Meow!
Woof!
```

**2 vtables**: `dog_vt` y `cat_vt`. `rex` y `max` comparten `dog_vt`.

**Sobre cambiar `max.base.vt = &cat_vt`**: **funcionaria**. `max`
seguiria siendo un `Dog` en memoria (con `name = "Max"`), pero hablaria
como gato:

```
Woof!
Meow!
Meow!    ← un Dog que dice Meow
```

Esto no crashea porque `speak` ignora `self` (`(void)s`). Si `speak`
accediera a campos especificos del tipo (como `self->lives`), leeria
datos del Dog — campos incorrectos, datos corruptos.

Esto demuestra algo importante: la vtable determina el **comportamiento**,
no el **tipo real** del dato. En C, nada impide que un Dog tenga la
vtable de Cat. El sistema confia en que el constructor asigna la vtable
correcta. C++ y Rust previenen esto: el compilador asigna la vtable y
el programador no puede cambiarla.

</details>

---

### Ejercicio 10 — Reflexion: vtables como mecanismo universal

Responde sin ver la respuesta:

1. La stdlib de C usa `void *` + function pointers sueltos (`qsort`
   recibe un comparador). Por que no usa vtables? Cuando una vtable
   aporta valor sobre function pointers individuales?

2. Una vtable agrupa operaciones. Un trait de Rust agrupa operaciones.
   Que ventaja tiene el trait sobre la vtable manual, ademas del type
   safety?

3. Si tienes 1000 instancias de 5 tipos distintos, cuantas vtables
   existen? Cuantos vptrs? Cual escala con los tipos y cual con las
   instancias?

**Prediccion**: Piensa las respuestas antes de abrir.

<details><summary>Respuesta</summary>

**1. Por que qsort no usa vtables:**

`qsort` necesita **una sola operacion** (comparar). Una vtable aporta
valor cuando:
- Un tipo tiene **multiples operaciones relacionadas** (area, perimeter,
  draw, destroy)
- Quieres garantizar que **todas** las operaciones estan implementadas
  como un conjunto coherente
- Quieres **almacenar** la capacidad de despacho dentro de la instancia
  para uso repetido

Para una funcion que recibe una sola operacion una vez, un function
pointer suelto es mas simple y suficiente.

**Regla practica**: 1 operacion → function pointer. 2+ operaciones
relacionadas que siempre van juntas → vtable.

**2. Ventajas del trait de Rust sobre vtable manual:**

Ademas del type safety:

- **Coherencia**: el compilador verifica que todas las funciones del
  trait estan implementadas. Con vtable manual, puedes olvidar un campo
  (queda NULL) y no hay error hasta runtime.
- **Metodos por defecto**: un trait puede tener implementaciones por
  defecto que los tipos heredan. Con vtable en C, cada tipo debe
  implementar todo o verificar NULL.
- **Dispatch estatico**: `impl Trait` genera codigo monomorphizado
  (zero-cost). La vtable siempre es dispatch dinamico.
- **Supertraits y bounds**: puedes expresar `T: Shape + Display +
  Clone` en compile time. Con vtables, combinar interfaces requiere
  multiples vptrs y casts manuales.

**3. Vtables vs vptrs:**

- **Vtables**: 5 (una por tipo) — escala con O(tipos)
- **Vptrs**: 1000 (uno por instancia) — escala con O(instancias)

La vtable contiene las funciones (grande pero compartida). El vptr es
solo un puntero de 8 bytes (pequeno pero por instancia). Con 1000
instancias y 5 tipos:

```
  Memoria vtables: 5 * sizeof(vtable)  = 5 * 32 = 160 bytes
  Memoria vptrs:   1000 * 8            = 8000 bytes
```

La vtable es O(tipos) y los vptrs son O(instancias). Incluso con
un millon de instancias, la vtable no crece — solo se agregan
vptrs de 8 bytes cada uno.

</details>
