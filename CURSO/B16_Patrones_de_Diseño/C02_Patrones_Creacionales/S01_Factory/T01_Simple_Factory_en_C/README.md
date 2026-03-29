# T01 — Simple Factory en C

---

## 1. El problema que resuelve Factory

Cuando creas objetos polimorficos en C, el codigo cliente necesita
conocer cada tipo concreto: incluir el header, llamar al constructor
especifico, y saber que vtable asignar. Esto acopla al cliente con
los detalles internos de cada tipo.

```c
// Sin factory: el cliente conoce TODO
#include "circle.h"
#include "rect.h"
#include "triangle.h"

Shape *create_from_config(const char *type) {
    if (strcmp(type, "circle") == 0) {
        Circle *c = circle_new(5.0);       // conoce Circle
        return (Shape *)c;                  // conoce el cast
    } else if (strcmp(type, "rect") == 0) {
        Rect *r = rect_new(3.0, 4.0);     // conoce Rect
        return (Shape *)r;
    } else if (strcmp(type, "triangle") == 0) {
        Triangle *t = triangle_new(3.0, 4.0, 5.0);
        return (Shape *)t;
    }
    return NULL;
}
```

Problemas:
- El cliente incluye N headers (uno por tipo concreto)
- Si agregas un tipo, modificas el codigo del cliente
- La logica de creacion se repite en multiples lugares
- El cliente necesita saber como inicializar cada tipo

**Factory**: centralizar la creacion en una funcion que recibe un
parametro y retorna el objeto correcto, ocultando los tipos concretos.

---

## 2. Anatomia del Simple Factory

```
  Sin Factory:                      Con Factory:

  Cliente                           Cliente
  ├─ #include "circle.h"            ├─ #include "shape_factory.h"
  ├─ #include "rect.h"              │
  ├─ circle_new(5.0)                └─ shape_create("circle", 5.0)
  ├─ rect_new(3.0, 4.0)                    │
  └─ conoce cada tipo                      ▼
                                    shape_factory.c
                                    ├─ #include "circle.h"   (oculto)
                                    ├─ #include "rect.h"     (oculto)
                                    └─ retorna Shape*

  El cliente solo conoce Shape* y shape_create().
  Los tipos concretos quedan encapsulados en la factory.
```

### 2.1 Componentes

```
  1. Interfaz comun:     Shape (struct con vtable)
  2. Tipos concretos:    Circle, Rect, Triangle (implementan Shape)
  3. Factory function:   shape_create(type, params) → Shape*
  4. Parametro selector: string, enum, o ID que indica que tipo crear
```

### 2.2 Diagrama de relaciones

```
  ┌─────────────────────────────────────────────┐
  │                 Cliente                      │
  │                                              │
  │  Shape *s = shape_create("circle", 5.0);    │
  │  double a = s->vt->area(s);                 │
  │  s->vt->destroy(s);                         │
  │                                              │
  │  Solo conoce: Shape, ShapeVtable, factory   │
  └──────────────────────┬──────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────┐
  │              shape_factory.c                 │
  │                                              │
  │  Conoce Circle, Rect, Triangle              │
  │  Decide cual crear segun parametro          │
  │  Retorna Shape* (tipo concreto oculto)      │
  └──────────┬───────────┬───────────┬──────────┘
             │           │           │
             ▼           ▼           ▼
        ┌────────┐  ┌────────┐  ┌──────────┐
        │ Circle │  │  Rect  │  │ Triangle │
        └────────┘  └────────┘  └──────────┘
```

---

## 3. Implementacion paso a paso

### 3.1 La interfaz comun (shape.h)

```c
/* shape.h — interfaz publica, lo unico que ve el cliente */
#ifndef SHAPE_H
#define SHAPE_H

#include <stddef.h>

typedef struct Shape Shape;
typedef struct ShapeVtable ShapeVtable;

struct ShapeVtable {
    const char *(*type_name)(const Shape *self);
    double      (*area)(const Shape *self);
    double      (*perimeter)(const Shape *self);
    void        (*destroy)(Shape *self);
};

struct Shape {
    const ShapeVtable *vt;
};

/* Funciones convenience que delegan al vtable */
static inline const char *shape_type_name(const Shape *s) {
    return s->vt->type_name(s);
}

static inline double shape_area(const Shape *s) {
    return s->vt->area(s);
}

static inline double shape_perimeter(const Shape *s) {
    return s->vt->perimeter(s);
}

static inline void shape_destroy(Shape *s) {
    if (s) s->vt->destroy(s);
}

#endif /* SHAPE_H */
```

### 3.2 Tipos concretos (ocultos al cliente)

```c
/* circle.h — header INTERNO (no distribuido al cliente) */
#ifndef CIRCLE_H
#define CIRCLE_H

#include "shape.h"

Shape *circle_new(double radius);

#endif /* CIRCLE_H */
```

```c
/* circle.c */
#include "circle.h"
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct {
    Shape base;       /* DEBE ser el primer campo */
    double radius;
} Circle;

static const char *circle_type_name(const Shape *self) {
    (void)self;
    return "Circle";
}

static double circle_area(const Shape *self) {
    const Circle *c = (const Circle *)self;
    return PI * c->radius * c->radius;
}

static double circle_perimeter(const Shape *self) {
    const Circle *c = (const Circle *)self;
    return 2.0 * PI * c->radius;
}

static void circle_destroy(Shape *self) {
    free(self);
}

static const ShapeVtable circle_vt = {
    .type_name  = circle_type_name,
    .area       = circle_area,
    .perimeter  = circle_perimeter,
    .destroy    = circle_destroy,
};

Shape *circle_new(double radius) {
    Circle *c = malloc(sizeof *c);
    if (!c) return NULL;
    c->base.vt = &circle_vt;
    c->radius = radius;
    return &c->base;
}
```

```c
/* rect.h */
#ifndef RECT_H
#define RECT_H

#include "shape.h"

Shape *rect_new(double width, double height);

#endif /* RECT_H */
```

```c
/* rect.c */
#include "rect.h"
#include <stdlib.h>

typedef struct {
    Shape base;
    double width;
    double height;
} Rect;

static const char *rect_type_name(const Shape *self) {
    (void)self;
    return "Rect";
}

static double rect_area(const Shape *self) {
    const Rect *r = (const Rect *)self;
    return r->width * r->height;
}

static double rect_perimeter(const Shape *self) {
    const Rect *r = (const Rect *)self;
    return 2.0 * (r->width + r->height);
}

static void rect_destroy(Shape *self) {
    free(self);
}

static const ShapeVtable rect_vt = {
    .type_name  = rect_type_name,
    .area       = rect_area,
    .perimeter  = rect_perimeter,
    .destroy    = rect_destroy,
};

Shape *rect_new(double width, double height) {
    Rect *r = malloc(sizeof *r);
    if (!r) return NULL;
    r->base.vt = &rect_vt;
    r->width = width;
    r->height = height;
    return &r->base;
}
```

```c
/* triangle.h */
#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "shape.h"

Shape *triangle_new(double a, double b, double c);

#endif /* TRIANGLE_H */
```

```c
/* triangle.c */
#include "triangle.h"
#include <stdlib.h>
#include <math.h>

typedef struct {
    Shape base;
    double a, b, c;  /* lados */
} Triangle;

static const char *triangle_type_name(const Shape *self) {
    (void)self;
    return "Triangle";
}

static double triangle_area(const Shape *self) {
    const Triangle *t = (const Triangle *)self;
    double s = (t->a + t->b + t->c) / 2.0;  /* semi-perimetro */
    return sqrt(s * (s - t->a) * (s - t->b) * (s - t->c));
}

static double triangle_perimeter(const Shape *self) {
    const Triangle *t = (const Triangle *)self;
    return t->a + t->b + t->c;
}

static void triangle_destroy(Shape *self) {
    free(self);
}

static const ShapeVtable triangle_vt = {
    .type_name  = triangle_type_name,
    .area       = triangle_area,
    .perimeter  = triangle_perimeter,
    .destroy    = triangle_destroy,
};

Shape *triangle_new(double a, double b, double c) {
    Triangle *t = malloc(sizeof *t);
    if (!t) return NULL;
    t->base.vt = &triangle_vt;
    t->a = a;
    t->b = b;
    t->c = c;
    return &t->base;
}
```

### 3.3 La factory

```c
/* shape_factory.h — lo que distribuyes al cliente */
#ifndef SHAPE_FACTORY_H
#define SHAPE_FACTORY_H

#include "shape.h"

/*
 * Crea un shape segun el tipo.
 * type: "circle", "rect", "triangle"
 * params: array de doubles (significado depende del tipo)
 * nparams: cantidad de parametros
 *
 * Retorna NULL si el tipo no existe o params insuficientes.
 * El caller es responsable de llamar shape_destroy().
 */
Shape *shape_create(const char *type, const double *params, int nparams);

#endif /* SHAPE_FACTORY_H */
```

```c
/* shape_factory.c */
#include "shape_factory.h"
#include "circle.h"
#include "rect.h"
#include "triangle.h"
#include <string.h>

Shape *shape_create(const char *type, const double *params, int nparams) {
    if (strcmp(type, "circle") == 0) {
        if (nparams < 1) return NULL;
        return circle_new(params[0]);
    }
    if (strcmp(type, "rect") == 0) {
        if (nparams < 2) return NULL;
        return rect_new(params[0], params[1]);
    }
    if (strcmp(type, "triangle") == 0) {
        if (nparams < 3) return NULL;
        return triangle_new(params[0], params[1], params[2]);
    }
    return NULL;
}
```

### 3.4 El cliente

```c
/* main.c — el cliente solo incluye shape.h y shape_factory.h */
#include <stdio.h>
#include "shape.h"
#include "shape_factory.h"

int main(void) {
    /* Crear shapes via factory */
    double circle_p[]   = { 5.0 };
    double rect_p[]     = { 3.0, 4.0 };
    double triangle_p[] = { 3.0, 4.0, 5.0 };

    Shape *shapes[] = {
        shape_create("circle",   circle_p,   1),
        shape_create("rect",     rect_p,     2),
        shape_create("triangle", triangle_p, 3),
    };
    int n = sizeof shapes / sizeof shapes[0];

    /* Usar polimorficamente — el cliente no sabe los tipos */
    for (int i = 0; i < n; i++) {
        if (!shapes[i]) continue;
        printf("%-10s  area=%.2f  perim=%.2f\n",
               shape_type_name(shapes[i]),
               shape_area(shapes[i]),
               shape_perimeter(shapes[i]));
    }

    /* Cleanup */
    for (int i = 0; i < n; i++) {
        shape_destroy(shapes[i]);
    }
    return 0;
}
```

```
  Output:
  Circle      area=78.54  perim=31.42
  Rect        area=12.00  perim=14.00
  Triangle    area=6.00   perim=12.00
```

---

## 4. Factory con enum selector

Usar strings como selector es flexible (configs, JSON), pero tiene
desventajas: no hay verificacion en compile time, typos son bugs
silenciosos. Alternativa: usar un enum.

```c
/* shape_factory.h — version con enum */
#ifndef SHAPE_FACTORY_H
#define SHAPE_FACTORY_H

#include "shape.h"

typedef enum {
    SHAPE_CIRCLE,
    SHAPE_RECT,
    SHAPE_TRIANGLE,
    SHAPE_COUNT  /* utilidad: cantidad de tipos */
} ShapeType;

Shape *shape_create(ShapeType type, const double *params, int nparams);

/* Bonus: convertir string a enum (para configs) */
ShapeType shape_type_from_string(const char *name);

#endif
```

```c
/* shape_factory.c */
#include "shape_factory.h"
#include "circle.h"
#include "rect.h"
#include "triangle.h"
#include <string.h>

Shape *shape_create(ShapeType type, const double *params, int nparams) {
    switch (type) {
    case SHAPE_CIRCLE:
        if (nparams < 1) return NULL;
        return circle_new(params[0]);
    case SHAPE_RECT:
        if (nparams < 2) return NULL;
        return rect_new(params[0], params[1]);
    case SHAPE_TRIANGLE:
        if (nparams < 3) return NULL;
        return triangle_new(params[0], params[1], params[2]);
    case SHAPE_COUNT:
        return NULL;
    }
    return NULL;  /* tipo desconocido */
}

ShapeType shape_type_from_string(const char *name) {
    static const struct { const char *name; ShapeType type; } map[] = {
        { "circle",   SHAPE_CIRCLE   },
        { "rect",     SHAPE_RECT     },
        { "triangle", SHAPE_TRIANGLE },
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (strcmp(name, map[i].name) == 0) return map[i].type;
    }
    return SHAPE_COUNT;  /* no encontrado */
}
```

Ventaja del enum: si agregas `SHAPE_ELLIPSE` y olvidas el case en
el switch, `gcc -Wswitch` te avisa (siempre que no haya `default`).

---

## 5. Factory driven by config

Un caso de uso clasico: leer de un archivo de configuracion que
shapes crear.

```
  shapes.conf:
  circle 5.0
  rect 3.0 4.0
  triangle 3.0 4.0 5.0
  circle 10.0
  rect 1.0 1.0
```

```c
#include <stdio.h>
#include <string.h>
#include "shape.h"
#include "shape_factory.h"

#define MAX_SHAPES 1024

int load_shapes(const char *filename, Shape **out, int max) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    int count = 0;
    char type[64];
    double params[8];

    while (count < max && fscanf(f, "%63s", type) == 1) {
        int nparams = 0;
        /* Leer doubles hasta fin de linea */
        int ch;
        while ((ch = fgetc(f)) != '\n' && ch != EOF) {
            if (ch == ' ' || ch == '\t') continue;
            ungetc(ch, f);
            if (fscanf(f, "%lf", &params[nparams]) == 1) {
                nparams++;
                if (nparams >= 8) break;
            } else {
                break;
            }
        }

        ShapeType st = shape_type_from_string(type);
        if (st == SHAPE_COUNT) {
            fprintf(stderr, "Unknown shape: %s\n", type);
            continue;
        }

        Shape *s = shape_create(st, params, nparams);
        if (s) {
            out[count++] = s;
        }
    }

    fclose(f);
    return count;
}

int main(void) {
    Shape *shapes[MAX_SHAPES];
    int n = load_shapes("shapes.conf", shapes, MAX_SHAPES);
    if (n < 0) { perror("load_shapes"); return 1; }

    double total_area = 0;
    for (int i = 0; i < n; i++) {
        printf("%-10s area=%.2f\n",
               shape_type_name(shapes[i]),
               shape_area(shapes[i]));
        total_area += shape_area(shapes[i]);
    }
    printf("Total area: %.2f\n", total_area);

    for (int i = 0; i < n; i++) shape_destroy(shapes[i]);
    return 0;
}
```

Este patron es comun en:
- Servidores que cargan plugins de un config
- Game engines que cargan niveles de archivos
- Parsers que construyen ASTs desde tokens
- GUIs que crean widgets desde XML/JSON

---

## 6. Parametros de creacion: variantes

La factory necesita recibir parametros que varian por tipo. Hay
multiples formas de resolver esto:

### 6.1 Array de doubles (simple pero limitado)

```c
/* Ya visto arriba */
Shape *shape_create(ShapeType type, const double *params, int nparams);

/* Pro: simple. Con: solo doubles, no hay nombres. */
```

### 6.2 Varargs (flexible pero inseguro)

```c
#include <stdarg.h>

Shape *shape_createv(ShapeType type, ...) {
    va_list ap;
    va_start(ap, type);

    Shape *result = NULL;
    switch (type) {
    case SHAPE_CIRCLE: {
        double r = va_arg(ap, double);
        result = circle_new(r);
        break;
    }
    case SHAPE_RECT: {
        double w = va_arg(ap, double);
        double h = va_arg(ap, double);
        result = rect_new(w, h);
        break;
    }
    case SHAPE_TRIANGLE: {
        double a = va_arg(ap, double);
        double b = va_arg(ap, double);
        double c = va_arg(ap, double);
        result = triangle_new(a, b, c);
        break;
    }
    default:
        break;
    }

    va_end(ap);
    return result;
}

/* Uso: mas natural */
Shape *c = shape_createv(SHAPE_CIRCLE, 5.0);
Shape *r = shape_createv(SHAPE_RECT, 3.0, 4.0);
```

```
  Pro: interfaz natural, cada tipo toma sus propios parametros.
  Con: CERO type safety. Si pasas un int donde espera double → UB.
       No hay forma de verificar que los argumentos sean correctos.
```

### 6.3 Config struct con union (type-safe)

```c
typedef struct {
    ShapeType type;
    union {
        struct { double radius; }           circle;
        struct { double width, height; }    rect;
        struct { double a, b, c; }          triangle;
    };
} ShapeConfig;

Shape *shape_create_config(const ShapeConfig *cfg) {
    switch (cfg->type) {
    case SHAPE_CIRCLE:
        return circle_new(cfg->circle.radius);
    case SHAPE_RECT:
        return rect_new(cfg->rect.width, cfg->rect.height);
    case SHAPE_TRIANGLE:
        return triangle_new(cfg->triangle.a, cfg->triangle.b,
                            cfg->triangle.c);
    default:
        return NULL;
    }
}

/* Uso con designated initializers (C99) */
Shape *c = shape_create_config(&(ShapeConfig){
    .type = SHAPE_CIRCLE,
    .circle.radius = 5.0,
});

Shape *r = shape_create_config(&(ShapeConfig){
    .type = SHAPE_RECT,
    .rect = { .width = 3.0, .height = 4.0 },
});
```

```
  Pro: type-safe, auto-documentado, serializable.
  Con: el config struct crece con cada tipo nuevo.
       Tamaño = el de la union mas grande (desperdicio si tipos
       son muy diferentes en tamaño de parametros).
```

### 6.4 Comparacion

```
  Metodo           Type-safe   Extensible   Ergonomia
  ──────           ─────────   ──────────   ─────────
  double[]         No          Si           Baja
  varargs          No          Si           Alta
  Config struct    Si          Medio        Alta
  Funciones indiv. Si          Si           Media

  Recomendacion:
  - API interna → Config struct (type-safe, designators)
  - API publica → Funciones individuales (shape_new_circle, etc.)
  - Config files → double[] o string parsing
```

---

## 7. Factory con registro dinamico

Hasta ahora la factory conoce todos los tipos en compile time.
Para un sistema de plugins, necesitas poder **registrar** tipos
en runtime:

```c
/* shape_registry.h */
#ifndef SHAPE_REGISTRY_H
#define SHAPE_REGISTRY_H

#include "shape.h"

/* Tipo de funcion constructora */
typedef Shape *(*ShapeConstructor)(const double *params, int nparams);

/* Registrar un nuevo tipo */
int shape_register(const char *name, ShapeConstructor ctor);

/* Crear via registro */
Shape *shape_create_registered(const char *name,
                               const double *params, int nparams);

/* Listar tipos registrados */
void shape_list_types(void);

#endif
```

```c
/* shape_registry.c */
#include "shape_registry.h"
#include <string.h>
#include <stdio.h>

#define MAX_TYPES 64

typedef struct {
    char name[64];
    ShapeConstructor ctor;
} TypeEntry;

static TypeEntry registry[MAX_TYPES];
static int registry_count = 0;

int shape_register(const char *name, ShapeConstructor ctor) {
    if (registry_count >= MAX_TYPES) return -1;

    /* Verificar duplicado */
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].name, name) == 0) return -2;
    }

    strncpy(registry[registry_count].name, name, 63);
    registry[registry_count].name[63] = '\0';
    registry[registry_count].ctor = ctor;
    registry_count++;
    return 0;
}

Shape *shape_create_registered(const char *name,
                               const double *params, int nparams) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry[i].name, name) == 0) {
            return registry[i].ctor(params, nparams);
        }
    }
    return NULL;  /* tipo no registrado */
}

void shape_list_types(void) {
    printf("Registered types (%d):\n", registry_count);
    for (int i = 0; i < registry_count; i++) {
        printf("  - %s\n", registry[i].name);
    }
}
```

```c
/* Registrar tipos al inicio del programa */

/* Adapter: convertir circle_new a ShapeConstructor */
static Shape *circle_ctor(const double *p, int n) {
    return (n >= 1) ? circle_new(p[0]) : NULL;
}
static Shape *rect_ctor(const double *p, int n) {
    return (n >= 2) ? rect_new(p[0], p[1]) : NULL;
}

int main(void) {
    /* Registro */
    shape_register("circle", circle_ctor);
    shape_register("rect",   rect_ctor);

    /* Crear via registro */
    double p1[] = { 5.0 };
    Shape *s = shape_create_registered("circle", p1, 1);
    printf("%s area=%.2f\n", shape_type_name(s), shape_area(s));
    shape_destroy(s);

    shape_list_types();
    return 0;
}
```

```
  Sin registro (compile-time):    Con registro (runtime):
  ──────────────────────────      ────────────────────────
  Factory conoce los tipos        Factory descubre los tipos
  Agregar tipo = recompilar       Agregar tipo = registrar
  Rapido (switch)                 Busqueda lineal (O(N))
  Type-safe (enum)                String-based (sin compile check)

  Usar registro cuando: plugins, shared libraries, config-driven
  Usar switch cuando: tipos fijos conocidos en compile time
```

---

## 8. Patron completo: factory + opaque types

La combinacion mas robusta: el cliente no puede ver ni crear los
tipos concretos directamente.

```
  Distribucion de headers:

  Al cliente:                    Interno:
  ┌───────────────────┐         ┌──────────────────┐
  │ shape.h           │         │ circle.c          │
  │ (Shape, vtable)   │         │ rect.c            │
  │                   │         │ triangle.c        │
  │ shape_factory.h   │         │ shape_factory.c   │
  │ (shape_create)    │         └──────────────────┘
  └───────────────────┘

  El cliente NO tiene circle.h, rect.h, triangle.h.
  No puede hacer circle_new() directamente.
  La unica forma de crear shapes es via la factory.
```

Esto da control total sobre:
- Que tipos existen (el cliente no puede inventar)
- Como se inicializan (validacion centralizada)
- Que recursos se asignan (pooling, arena, etc.)
- Conteo de instancias (metricas, limites)

### 8.1 Factory con validacion

```c
Shape *shape_create(ShapeType type, const double *params, int nparams) {
    switch (type) {
    case SHAPE_CIRCLE:
        if (nparams < 1) return NULL;
        if (params[0] <= 0) {
            fprintf(stderr, "circle: radius must be > 0\n");
            return NULL;
        }
        return circle_new(params[0]);

    case SHAPE_RECT:
        if (nparams < 2) return NULL;
        if (params[0] <= 0 || params[1] <= 0) {
            fprintf(stderr, "rect: dimensions must be > 0\n");
            return NULL;
        }
        return rect_new(params[0], params[1]);

    case SHAPE_TRIANGLE:
        if (nparams < 3) return NULL;
        /* Desigualdad triangular */
        if (params[0] + params[1] <= params[2] ||
            params[0] + params[2] <= params[1] ||
            params[1] + params[2] <= params[0]) {
            fprintf(stderr, "triangle: invalid side lengths\n");
            return NULL;
        }
        return triangle_new(params[0], params[1], params[2]);

    default:
        return NULL;
    }
}
```

La factory centraliza la validacion: no importa desde donde se
cree un shape, siempre pasa por las mismas reglas.

---

## 9. Memoria y ownership

### 9.1 Convencion: la factory alloca, el caller libera

```c
/* Contrato de ownership:
 *
 * shape_create() llama malloc internamente.
 * El caller recibe ownership del Shape*.
 * El caller DEBE llamar shape_destroy() cuando termine.
 *
 * shape_destroy() libera toda la memoria del shape,
 * incluida la que shape_create() asigno.
 */

Shape *s = shape_create(SHAPE_CIRCLE, (double[]){5.0}, 1);
/* ... usar s ... */
shape_destroy(s);  /* OBLIGATORIO */
```

### 9.2 Factory con pool (evitar malloc por shape)

```c
/* Para hot paths: pre-asignar un pool de shapes */

#define POOL_SIZE 1024

typedef struct {
    char buf[POOL_SIZE][128];  /* 128 bytes max por shape */
    int used[POOL_SIZE];
    int count;
} ShapePool;

static ShapePool pool = { .count = 0 };

void *pool_alloc(size_t size) {
    if (size > 128) return NULL;  /* no cabe */
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!pool.used[i]) {
            pool.used[i] = 1;
            pool.count++;
            return pool.buf[i];
        }
    }
    return NULL;  /* pool lleno */
}

void pool_free(void *ptr) {
    for (int i = 0; i < POOL_SIZE; i++) {
        if (pool.buf[i] == ptr) {
            pool.used[i] = 0;
            pool.count--;
            return;
        }
    }
}
```

La factory usa `pool_alloc` en lugar de `malloc`. El caller sigue
llamando `shape_destroy`, que internamente llama `pool_free`.
La interfaz no cambia — el cambio es interno a la factory.

### 9.3 Diagrama de lifetime

```
  shape_create("circle", ...)
      │
      ├─ malloc(sizeof(Circle))        ← factory asigna
      ├─ circle->base.vt = &circle_vt  ← factory inicializa
      ├─ circle->radius = params[0]
      └─ return &circle->base          ← ownership transferido
            │
            │  (el caller usa s->vt->area(s), etc.)
            │
            ▼
  shape_destroy(s)
      │
      └─ s->vt->destroy(s)
            │
            └─ free(self)               ← factory libera
```

---

## 10. Comparacion con Rust (preview)

En Rust, el Simple Factory es una funcion que retorna `Box<dyn Trait>`
o una variante de enum. T02 lo implementa en detalle, pero aqui esta
el contraste:

```c
/* C: factory retorna Shape* (puntero opaco) */
Shape *shape_create(ShapeType type, const double *params, int nparams);
/* Riesgo: NULL, leak, double-free, tipo incorrecto */
```

```rust
// Rust: factory retorna Box<dyn Shape> o enum
fn shape_create(kind: &str, params: &[f64]) -> Option<Box<dyn Shape>> {
    match kind {
        "circle" => Some(Box::new(Circle::new(params[0]?))),
        "rect"   => Some(Box::new(Rect::new(params[0]?, params[1]?))),
        _ => None,
    }
}

// O con enum (sin allocation):
fn shape_create(kind: &str, params: &[f64]) -> Option<ShapeEnum> {
    match kind {
        "circle" => Some(ShapeEnum::Circle(params.first()?.clone())),
        "rect"   => {
            let (w, h) = (params.get(0)?, params.get(1)?);
            Some(ShapeEnum::Rect(*w, *h))
        }
        _ => None,
    }
}
```

```
  Diferencia                C                    Rust
  ──────────                ─                    ────
  Retorno falla             NULL (olvidable)     Option (forzado)
  Ownership                 Documentacion        Tipo sistema (Box)
  Destruccion               Manual (forget=leak) Automatica (Drop)
  Vtable                    Manual               Automatica (dyn Trait)
  Alternativa sin heap      No (siempre malloc)  Si (enum)
  Type safety de params     Varargs = UB         Tipado
```

---

## Errores comunes

### Error 1 — Olvidar shape_destroy

```c
void process_config(const char *filename) {
    Shape *shapes[100];
    int n = load_shapes(filename, shapes, 100);
    for (int i = 0; i < n; i++) {
        printf("area=%.2f\n", shape_area(shapes[i]));
    }
    /* BUG: nunca llama shape_destroy → leak de N shapes */
}
```

Siempre liberar en la misma funcion que posee el recurso, o
documentar claramente la transferencia de ownership.

### Error 2 — Switch sin case para nuevo tipo

```c
Shape *shape_create(ShapeType type, ...) {
    switch (type) {
    case SHAPE_CIRCLE:   return circle_new(r);
    case SHAPE_RECT:     return rect_new(w, h);
    /* Olvidaste SHAPE_TRIANGLE → retorna NULL silenciosamente */
    default:             return NULL;
    }
}
```

Compilar con `-Wswitch -Werror` y **no** usar `default` en switches
sobre enums. Asi el compilador avisa si falta un case.

### Error 3 — Factory que no valida parametros

```c
Shape *shape_create(ShapeType type, const double *params, int nparams) {
    switch (type) {
    case SHAPE_CIRCLE:
        return circle_new(params[0]);  /* BUG: nparams puede ser 0 */
    /* ... */
    }
}

/* El caller pasa: */
shape_create(SHAPE_CIRCLE, NULL, 0);
/* → acceso a params[0] = NULL dereference → crash o UB */
```

Validar siempre `nparams` antes de acceder a `params[]`.

### Error 4 — Mezclar factories con creacion directa

```c
/* La factory asigna via pool_alloc */
Shape *s1 = shape_create(SHAPE_CIRCLE, p, 1);

/* Alguien crea directamente con malloc */
Circle *s2 = malloc(sizeof(Circle));
s2->base.vt = &circle_vt;

/* Despues: */
shape_destroy(s1);  /* OK: pool_free */
shape_destroy(s2);  /* BUG: pool_free recibe un puntero de malloc */
```

Si la factory usa un allocator custom, el `destroy` debe usar el
mismo. No mezclar creacion directa con factory.

---

## Ejercicios

### Ejercicio 1 — Implementar un tipo nuevo

Agrega `Ellipse` (semi-ejes `a` y `b`) al sistema de shapes:

1. Crear `ellipse.h` y `ellipse.c`
2. Agregar `SHAPE_ELLIPSE` al enum
3. Agregar el case en la factory
4. Verificar que el cliente funciona sin cambios

**Prediccion**: Cuantos archivos necesitas modificar?

<details><summary>Respuesta</summary>

Archivos a crear: 2 (ellipse.h, ellipse.c)
Archivos a modificar: 2 (shape_factory.h para el enum, shape_factory.c
para el case)

Total: 4 archivos tocados.

```c
/* ellipse.h */
#ifndef ELLIPSE_H
#define ELLIPSE_H
#include "shape.h"
Shape *ellipse_new(double semi_a, double semi_b);
#endif

/* ellipse.c */
#include "ellipse.h"
#include <stdlib.h>
#include <math.h>
#define PI 3.14159265358979323846

typedef struct {
    Shape base;
    double semi_a, semi_b;
} Ellipse;

static const char *ellipse_type_name(const Shape *s) {
    (void)s; return "Ellipse";
}
static double ellipse_area(const Shape *self) {
    const Ellipse *e = (const Ellipse *)self;
    return PI * e->semi_a * e->semi_b;
}
static double ellipse_perimeter(const Shape *self) {
    const Ellipse *e = (const Ellipse *)self;
    /* Aproximacion de Ramanujan */
    double a = e->semi_a, b = e->semi_b;
    return PI * (3*(a+b) - sqrt((3*a+b)*(a+3*b)));
}
static void ellipse_destroy(Shape *self) { free(self); }

static const ShapeVtable ellipse_vt = {
    ellipse_type_name, ellipse_area, ellipse_perimeter, ellipse_destroy
};

Shape *ellipse_new(double semi_a, double semi_b) {
    Ellipse *e = malloc(sizeof *e);
    if (!e) return NULL;
    e->base.vt = &ellipse_vt;
    e->semi_a = semi_a;
    e->semi_b = semi_b;
    return &e->base;
}
```

En shape_factory.h: agregar `SHAPE_ELLIPSE` antes de `SHAPE_COUNT`.
En shape_factory.c: agregar el case y `#include "ellipse.h"`.

El main.c **no cambia** — ese es el beneficio del patron factory.

</details>

---

### Ejercicio 2 — Factory con nombre conveniente

Escribe funciones wrapper que oculten los detalles de `params[]`:

```c
Shape *shape_new_circle(double radius);
Shape *shape_new_rect(double width, double height);
Shape *shape_new_triangle(double a, double b, double c);
```

Que internamente llamen a `shape_create`. Que ventaja tiene esto
sobre que el cliente llame a `shape_create` directamente?

**Prediccion**: Estas funciones deben estar en el header publico
o en el privado?

<details><summary>Respuesta</summary>

```c
/* En shape_factory.h (header publico) */
static inline Shape *shape_new_circle(double radius) {
    return shape_create(SHAPE_CIRCLE, (double[]){ radius }, 1);
}

static inline Shape *shape_new_rect(double width, double height) {
    return shape_create(SHAPE_RECT, (double[]){ width, height }, 2);
}

static inline Shape *shape_new_triangle(double a, double b, double c) {
    return shape_create(SHAPE_TRIANGLE, (double[]){ a, b, c }, 3);
}
```

Ventajas:
1. **Type-safe**: el compilador verifica que `radius` es double, no
   es posible pasar nparams incorrecto.
2. **Auto-documentadas**: los nombres de parametros explican su
   significado.
3. **Errores en compile time**: si pasas 2 argumentos a
   `shape_new_circle`, el compilador avisa.
4. **Mantiene la centralizacion**: toda la creacion sigue pasando
   por `shape_create` internamente.

Deben estar en el header **publico** — son la interfaz ergonomica
que el cliente usa. `shape_create` con `double[]` queda como API
de bajo nivel para el caso config-driven.

</details>

---

### Ejercicio 3 — Validar en la factory

Agrega validacion a la factory para estos invariantes:

- Circle: radius > 0
- Rect: width > 0 y height > 0
- Triangle: lados positivos y cumplen desigualdad triangular

La factory debe retornar NULL y escribir un mensaje a stderr si
los parametros son invalidos.

**Prediccion**: Donde es mejor validar — en la factory o en
`circle_new`?

<details><summary>Respuesta</summary>

```c
Shape *shape_create(ShapeType type, const double *params, int nparams) {
    switch (type) {
    case SHAPE_CIRCLE:
        if (nparams < 1 || params[0] <= 0) {
            fprintf(stderr, "circle: need radius > 0 (got %.2f)\n",
                    nparams >= 1 ? params[0] : 0.0);
            return NULL;
        }
        return circle_new(params[0]);

    case SHAPE_RECT:
        if (nparams < 2 || params[0] <= 0 || params[1] <= 0) {
            fprintf(stderr, "rect: need width,height > 0\n");
            return NULL;
        }
        return rect_new(params[0], params[1]);

    case SHAPE_TRIANGLE:
        if (nparams < 3 || params[0] <= 0 || params[1] <= 0 ||
            params[2] <= 0) {
            fprintf(stderr, "triangle: need sides > 0\n");
            return NULL;
        }
        if (params[0]+params[1] <= params[2] ||
            params[0]+params[2] <= params[1] ||
            params[1]+params[2] <= params[0]) {
            fprintf(stderr, "triangle: violates triangle inequality\n");
            return NULL;
        }
        return triangle_new(params[0], params[1], params[2]);

    default:
        return NULL;
    }
}
```

Donde validar: **en ambos sitios, pero con roles distintos**:

- **Factory**: validacion de negocio (desigualdad triangular,
  rangos aceptables). Es el punto de entrada unico.
- **Constructor** (`circle_new`): validacion minima de contrato
  (assert para cosas que "no deberian pasar" si el caller es
  correcto). Esto es defensa en profundidad.

```c
Shape *circle_new(double radius) {
    assert(radius > 0 && "circle_new: radius must be > 0");
    Circle *c = malloc(sizeof *c);
    /* ... */
}
```

Si solo la factory puede llamar a `circle_new` (header interno),
la validacion de la factory es suficiente y el assert es redundante
pero inofensivo. Si `circle_new` es publico, necesita su propia
validacion.

</details>

---

### Ejercicio 4 — Registry: registrar un tipo externo

Usando el sistema de registro dinamico (seccion 7), escribe un
"plugin" que registra un tipo `Pentagon` sin modificar la factory:

```c
/* pentagon_plugin.c — compilado separadamente */
void pentagon_register(void);
```

El main debe poder hacer:
```c
pentagon_register();
Shape *p = shape_create_registered("pentagon", (double[]){5.0}, 1);
```

**Prediccion**: Puede un plugin agregar un tipo que la factory
original no conoce?

<details><summary>Respuesta</summary>

```c
/* pentagon_plugin.c */
#include "shape.h"
#include "shape_registry.h"
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct {
    Shape base;
    double side;
} Pentagon;

static const char *pentagon_type_name(const Shape *s) {
    (void)s; return "Pentagon";
}

static double pentagon_area(const Shape *self) {
    const Pentagon *p = (const Pentagon *)self;
    /* Area de pentagono regular: (sqrt(5(5+2*sqrt(5)))/4) * s^2 */
    return 0.25 * sqrt(5.0 * (5.0 + 2.0 * sqrt(5.0)))
         * p->side * p->side;
}

static double pentagon_perimeter(const Shape *self) {
    const Pentagon *p = (const Pentagon *)self;
    return 5.0 * p->side;
}

static void pentagon_destroy(Shape *self) { free(self); }

static const ShapeVtable pentagon_vt = {
    pentagon_type_name, pentagon_area, pentagon_perimeter, pentagon_destroy
};

static Shape *pentagon_ctor(const double *params, int nparams) {
    if (nparams < 1 || params[0] <= 0) return NULL;
    Pentagon *p = malloc(sizeof *p);
    if (!p) return NULL;
    p->base.vt = &pentagon_vt;
    p->side = params[0];
    return &p->base;
}

void pentagon_register(void) {
    shape_register("pentagon", pentagon_ctor);
}
```

Si, el plugin puede agregar un tipo que la factory original no
conoce. Eso es exactamente el proposito del registry: extensibilidad
en runtime sin recompilar la factory.

Con shared libraries (`dlopen`), esto permite cargar plugins
de disco:

```c
void *lib = dlopen("./pentagon_plugin.so", RTLD_LAZY);
void (*reg)(void) = dlsym(lib, "pentagon_register");
reg();  /* ahora "pentagon" esta disponible */
```

</details>

---

### Ejercicio 5 — Config struct con designated initializers

Reescribe la factory usando la variante Config struct (seccion 6.3).
Crea tres shapes usando designated initializers de C99.

**Prediccion**: Es posible asignar el campo incorrecto del union
(ej. `.circle.radius` cuando `type = SHAPE_RECT`)?

<details><summary>Respuesta</summary>

```c
typedef struct {
    ShapeType type;
    union {
        struct { double radius; }        circle;
        struct { double width, height; } rect;
        struct { double a, b, c; }       triangle;
    };
} ShapeConfig;

Shape *shape_create_config(const ShapeConfig *cfg) {
    switch (cfg->type) {
    case SHAPE_CIRCLE:
        return circle_new(cfg->circle.radius);
    case SHAPE_RECT:
        return rect_new(cfg->rect.width, cfg->rect.height);
    case SHAPE_TRIANGLE:
        return triangle_new(cfg->triangle.a, cfg->triangle.b,
                            cfg->triangle.c);
    default:
        return NULL;
    }
}

/* Uso: */
Shape *c = shape_create_config(&(ShapeConfig){
    .type = SHAPE_CIRCLE,
    .circle.radius = 5.0,
});

Shape *r = shape_create_config(&(ShapeConfig){
    .type = SHAPE_RECT,
    .rect = { .width = 3.0, .height = 4.0 },
});

Shape *t = shape_create_config(&(ShapeConfig){
    .type = SHAPE_TRIANGLE,
    .triangle = { .a = 3.0, .b = 4.0, .c = 5.0 },
});
```

Si, es posible escribir:
```c
ShapeConfig bad = {
    .type = SHAPE_RECT,
    .circle.radius = 5.0,  /* BUG: tipo dice rect, datos dicen circle */
};
```

C no verifica que el campo de la union corresponda al tag del enum.
Esto compilara sin advertencia. La factory leera `cfg->rect.width`,
que reinterpreta los bytes de `circle.radius` como width (misma
posicion en la union), y `cfg->rect.height` sera 0.0 (no
inicializado).

Esto es un **tagged union sin enforcement** — C confía en el
programador. Rust resuelve esto con enums que empaquetan tag + datos:
```rust
enum ShapeConfig {
    Circle { radius: f64 },       // imposible mezclar tag y datos
    Rect { width: f64, height: f64 },
}
```

</details>

---

### Ejercicio 6 — Factory que cuenta instancias

Modifica la factory para contar cuantas instancias de cada tipo
se han creado y cuantas estan activas (creadas - destruidas):

```c
void shape_stats(void);
/* Output:  circle: 5 created, 3 active
            rect:   2 created, 2 active */
```

**Prediccion**: Donde almacenas los contadores? Como incrementas
el de "destruidas"?

<details><summary>Respuesta</summary>

```c
/* En shape_factory.c */
static int created[SHAPE_COUNT]  = {0};
static int destroyed[SHAPE_COUNT] = {0};

Shape *shape_create(ShapeType type, const double *params, int nparams) {
    Shape *s = NULL;
    switch (type) {
    case SHAPE_CIRCLE:
        if (nparams < 1) return NULL;
        s = circle_new(params[0]);
        break;
    case SHAPE_RECT:
        if (nparams < 2) return NULL;
        s = rect_new(params[0], params[1]);
        break;
    case SHAPE_TRIANGLE:
        if (nparams < 3) return NULL;
        s = triangle_new(params[0], params[1], params[2]);
        break;
    default:
        return NULL;
    }
    if (s) created[type]++;
    return s;
}

/* Para contar destrucciones, necesitamos un wrapper de destroy */
/* Opcion 1: almacenar el ShapeType en el shape (un campo extra) */
/* Opcion 2: wrapper que se interpone en destroy */

/* Opcion 2: wrap destroy via vtable */
typedef struct {
    Shape base;
    ShapeType type_id;      /* tracking */
    Shape *inner;           /* el shape real */
} TrackedShape;

/* Opcion mas simple: usar una tabla global de punteros a tipo */
/* O la mas pragmatica: cada tipo notifica en su destroy */

/* Pragmatica: un callback global */
static void (*on_destroy)(ShapeType type) = NULL;

void shape_set_destroy_hook(void (*hook)(ShapeType)) {
    on_destroy = hook;
}

static void track_destroy(ShapeType type) {
    destroyed[type]++;
}

static const char *type_names[] = {
    "circle", "rect", "triangle"
};

void shape_stats(void) {
    for (int i = 0; i < SHAPE_COUNT; i++) {
        printf("%-10s %d created, %d active\n",
               type_names[i], created[i],
               created[i] - destroyed[i]);
    }
}
```

La solucion mas limpia es almacenar el `ShapeType` como un campo
extra en cada Shape (o en un campo de tracking que el factory
agrega). Asi el destroy generico puede consultar el tipo y
decrementar el contador:

```c
/* Extender Shape con tracking */
typedef struct {
    Shape base;
    ShapeType factory_type;
} TrackedShape;

/* shape_destroy wrapper */
void shape_destroy_tracked(Shape *s) {
    if (!s) return;
    TrackedShape *ts = (TrackedShape *)
        ((char *)s - offsetof(TrackedShape, base));
    /* Ojo: solo funciona si la factory creo el shape */
    destroyed[ts->factory_type]++;
    s->vt->destroy(s);
}
```

En la practica, la forma mas comun es que cada tipo reporte su
destruccion al factory via un hook global o simplemente agregar
un campo `type_id` al struct `Shape` base.

</details>

---

### Ejercicio 7 — Factory retornando error detallado

Modifica la factory para retornar un codigo de error en lugar de
NULL generico:

```c
typedef enum {
    FACTORY_OK = 0,
    FACTORY_UNKNOWN_TYPE,
    FACTORY_INVALID_PARAMS,
    FACTORY_ALLOC_FAILED,
} FactoryResult;

FactoryResult shape_create(ShapeType type, const double *params,
                           int nparams, Shape **out);
```

**Prediccion**: Por que el Shape* ahora es un parametro de salida
en lugar de retorno?

<details><summary>Respuesta</summary>

```c
FactoryResult shape_create(ShapeType type, const double *params,
                           int nparams, Shape **out) {
    *out = NULL;  /* default seguro */

    switch (type) {
    case SHAPE_CIRCLE:
        if (nparams < 1 || params[0] <= 0)
            return FACTORY_INVALID_PARAMS;
        *out = circle_new(params[0]);
        return *out ? FACTORY_OK : FACTORY_ALLOC_FAILED;

    case SHAPE_RECT:
        if (nparams < 2 || params[0] <= 0 || params[1] <= 0)
            return FACTORY_INVALID_PARAMS;
        *out = rect_new(params[0], params[1]);
        return *out ? FACTORY_OK : FACTORY_ALLOC_FAILED;

    case SHAPE_TRIANGLE:
        if (nparams < 3)
            return FACTORY_INVALID_PARAMS;
        if (params[0]+params[1] <= params[2] ||
            params[0]+params[2] <= params[1] ||
            params[1]+params[2] <= params[0])
            return FACTORY_INVALID_PARAMS;
        *out = triangle_new(params[0], params[1], params[2]);
        return *out ? FACTORY_OK : FACTORY_ALLOC_FAILED;

    default:
        return FACTORY_UNKNOWN_TYPE;
    }
}

/* Uso: */
Shape *s;
FactoryResult res = shape_create(SHAPE_CIRCLE, (double[]){-5.0}, 1, &s);
switch (res) {
case FACTORY_OK:
    printf("Created %s\n", shape_type_name(s));
    break;
case FACTORY_INVALID_PARAMS:
    fprintf(stderr, "Invalid parameters\n");
    break;
case FACTORY_ALLOC_FAILED:
    fprintf(stderr, "Out of memory\n");
    break;
case FACTORY_UNKNOWN_TYPE:
    fprintf(stderr, "Unknown type\n");
    break;
}
```

El Shape* es un parametro de salida (`Shape **out`) porque C solo
permite un valor de retorno. Necesitamos retornar DOS cosas: el
codigo de error Y el shape creado. Opciones:

1. **Retornar Shape*, error via errno** — tradicion Unix, pero
   errno es global y fragil en multithreaded.
2. **Retornar error, shape via out-param** — lo que hacemos aqui.
   Es el patron mas comun en C moderno.
3. **Retornar struct { error, shape }** — valido, pero las structs
   como retorno son incomodas en C.

En Rust, esto seria `Result<Box<dyn Shape>, FactoryError>` — un
solo valor de retorno que contiene O el shape O el error, nunca
ambos ni ninguno.

</details>

---

### Ejercicio 8 — Factory con clonacion

Agrega una operacion `clone` al vtable que permita duplicar shapes:

```c
Shape *shape_clone(const Shape *s);
```

Implementa `clone` para Circle y Rect. El clone crea una copia
independiente (deep copy).

**Prediccion**: Necesitas modificar el vtable? O puedes
implementarlo sin cambiar la interfaz existente?

<details><summary>Respuesta</summary>

Necesitas agregar `clone` al vtable — sin ella, la funcion
`shape_clone` no sabe el tamano ni los campos del tipo concreto.

```c
/* Agregar al vtable: */
struct ShapeVtable {
    const char *(*type_name)(const Shape *self);
    double      (*area)(const Shape *self);
    double      (*perimeter)(const Shape *self);
    Shape      *(*clone)(const Shape *self);   /* NUEVO */
    void        (*destroy)(Shape *self);
};

/* Implementar para Circle: */
static Shape *circle_clone(const Shape *self) {
    const Circle *c = (const Circle *)self;
    return circle_new(c->radius);  /* crea copia independiente */
}

static const ShapeVtable circle_vt = {
    .type_name  = circle_type_name,
    .area       = circle_area,
    .perimeter  = circle_perimeter,
    .clone      = circle_clone,
    .destroy    = circle_destroy,
};

/* Implementar para Rect: */
static Shape *rect_clone(const Shape *self) {
    const Rect *r = (const Rect *)self;
    return rect_new(r->width, r->height);
}

/* Funcion convenience: */
Shape *shape_clone(const Shape *s) {
    if (!s || !s->vt->clone) return NULL;
    return s->vt->clone(s);
}

/* Uso: */
Shape *original = shape_create(SHAPE_CIRCLE, (double[]){5.0}, 1);
Shape *copy = shape_clone(original);
/* original y copy son independientes */
shape_destroy(original);
/* copy sigue valido */
shape_destroy(copy);
```

Sin `clone` en el vtable, la unica alternativa seria un switch
por tipo (que rompe el patron) o almacenar `sizeof` en el vtable
y hacer `memcpy` (que solo funciona para shallow copy de tipos
sin punteros internos).

La operacion `clone` es el equivalente en C de `Clone` trait en
Rust. En Rust: `#[derive(Clone)]` y `s.clone()`. En C: lo
implementas a mano para cada tipo.

</details>

---

### Ejercicio 9 — Comparar con funciones individuales

Algunos proyectos C no usan factory — exponen constructores
individuales directamente:

```c
/* Enfoque A: Factory */
Shape *s = shape_create(SHAPE_CIRCLE, params, 1);

/* Enfoque B: Constructores directos */
Shape *s = circle_new(5.0);
```

Lista al menos 3 ventajas de cada enfoque. Cuando preferirias
cada uno?

**Prediccion**: Los proyectos grandes usan mas factories o mas
constructores directos?

<details><summary>Respuesta</summary>

**Ventajas de Factory:**

1. **Desacoplamiento**: el cliente no necesita incluir headers de
   cada tipo concreto. Solo conoce `shape.h` y `shape_factory.h`.

2. **Creacion config-driven**: puede leer un string o config y crear
   el tipo correcto. Con constructores directos, necesitas un switch
   en el cliente.

3. **Centralizacion**: validacion, logging, metricas, pooling — todo
   en un solo lugar. Con constructores directos, cada call site
   necesita su propia validacion.

4. **Extensibilidad sin recompilacion del cliente**: agregar un tipo
   nuevo solo cambia la factory, no los clientes.

**Ventajas de constructores directos:**

1. **Type safety**: `circle_new(5.0)` es type-safe. No hay riesgo
   de pasar parametros incorrectos via `double[]`.

2. **Simplicidad**: menos indirecciones, menos codigo, mas facil de
   entender y debuggear.

3. **Rendimiento**: sin switch ni strcmp. Llamada directa.

4. **IDE support**: autocompletar `circle_` muestra los constructores
   disponibles. Con factory, necesitas saber los strings o enums.

**Cuando usar cada uno:**

| Escenario | Recomendacion |
|-----------|--------------|
| Tipos conocidos en compile time, performance critico | Directo |
| Config-driven (JSON, files, CLI args) | Factory |
| Libreria publica con muchos tipos | Factory (desacopla) |
| Codigo interno con pocos tipos | Directo (simple) |
| Sistema de plugins | Factory con registro |

Los proyectos grandes tipicamente usan **ambos**: factory para
creacion config-driven y constructores directos para uso interno
donde el tipo es conocido.

</details>

---

### Ejercicio 10 — Reflexion: factory en C vs la vida real

Responde sin ver la respuesta:

1. Nombra una factory que ya usaste en C sin saberlo (pista:
   funciones de `stdio.h` o `stdlib.h`).

2. La factory resuelve un problema de **acoplamiento**. Explica
   con tus palabras que estaba acoplado y que desacopla.

3. Si tu factory tiene 50 tipos en el switch, hay un problema
   de diseno? Cual seria la alternativa?

<details><summary>Respuesta</summary>

**1. Factory en la stdlib:**

`fopen()` es una factory. Recibe un path y un modo, y retorna un
`FILE*` opaco. El caller no sabe si es un archivo en disco, un
pipe, un dispositivo de red, o un buffer en memoria. Internamente,
`fopen` decide que implementacion crear segun el path y el sistema
operativo.

Otros ejemplos:
- `socket()` → retorna un fd que puede ser TCP, UDP, Unix, etc.
- `dlopen()` → retorna un handle opaco a una shared library.
- `opendir()` → retorna un `DIR*` opaco.

En todas: el caller recibe un handle opaco y usa funciones
genericas (`fread`, `recv`, `readdir`). La factory decide que
implementacion concreta se crea.

**2. Que desacopla:**

Sin factory, el **codigo cliente** esta acoplado a los **tipos
concretos**: conoce Circle, Rect, Triangle, sus headers, sus
constructores, sus parametros. Si cambias un tipo (ej. Circle
necesita un parametro mas), todos los clientes se rompen.

Con factory, el cliente esta acoplado solo a la **interfaz**
(Shape + factory). Los tipos concretos son un detalle de
implementacion de la factory. Puedes agregar, quitar, o
modificar tipos sin que el cliente lo note.

```
  Sin factory:  Cliente ──→ Circle, Rect, Triangle (N dependencias)
  Con factory:  Cliente ──→ Shape + Factory (1 dependencia)
                Factory ──→ Circle, Rect, Triangle (encapsulado)
```

**3. Factory con 50 tipos:**

Si, es un problema. Un switch de 50 cases es dificil de mantener
y viola el Open/Closed Principle (agregar un tipo = modificar la
factory).

Alternativas:
- **Registry dinamico** (seccion 7): cada tipo se registra a si
  mismo. La factory no necesita conocer los tipos en su codigo.
- **Tabla de constructores**: array indexado por enum o hash map
  de string → constructor.
- **Macros de auto-registro**: cada tipo define una macro que se
  expande en una entrada de registro (patron comun en kernel Linux).
- **Generacion de codigo**: un script genera el switch desde una
  definicion de tipos (IDL, protobuf, etc.).

En la practica, si tienes >10 tipos en un switch, considera el
patron registry.

</details>
