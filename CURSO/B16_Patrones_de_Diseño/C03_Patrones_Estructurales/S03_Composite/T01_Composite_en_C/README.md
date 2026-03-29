# Composite en C

## 1. El problema: estructuras jerárquicas

Muchas estructuras del mundo real son **árboles**: un directorio contiene archivos y
otros directorios, un menú contiene ítems y submenús, una expresión aritmética contiene
operandos y subexpresiones. El patrón Composite permite tratar hojas y contenedores
con la **misma interfaz**.

```
Sin Composite:                 Con Composite:

if (is_file(node))             node->ops->size(node);
    return file_size(node);    // Funciona igual para
else {                         // archivos y directorios
    sum = 0;
    for each child in node
        sum += dir_size(child);
    return sum;
}
```

El código cliente no necesita saber si opera sobre una hoja o un nodo compuesto.

---

## 2. Anatomía del patrón

```
                    ┌──────────────┐
                    │  Component   │  ← interfaz común
                    │──────────────│
                    │ ops: *Ops    │
                    │ name: char*  │
                    └──────┬───────┘
                           │
              ┌────────────┴────────────┐
              │                         │
      ┌───────┴──────┐        ┌────────┴────────┐
      │    Leaf       │        │   Composite     │
      │──────────────│        │─────────────────│
      │ (datos prop.) │        │ children: **Comp │
      │              │        │ count, capacity  │
      └──────────────┘        └─────────────────┘
```

Tres participantes:
1. **Component**: interfaz común (vtable en C)
2. **Leaf**: elemento terminal sin hijos
3. **Composite**: contenedor que almacena hijos y delega operaciones recursivamente

---

## 3. Implementación base: sistema de archivos

### 3.1 Interfaz común con vtable

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration
typedef struct FsNode FsNode;

// Vtable: operaciones que todo nodo debe soportar
typedef struct {
    size_t (*size)(const FsNode *self);
    void   (*print)(const FsNode *self, int depth);
    void   (*destroy)(FsNode *self);
} FsOps;

// Componente base (campo común a hojas y composites)
struct FsNode {
    const FsOps *ops;
    char        *name;
};
```

Cada `FsNode` tiene un puntero a su vtable y un nombre. No importa si es archivo
o directorio: el código cliente solo usa `node->ops->size(node)`.

### 3.2 Hoja: File

```c
typedef struct {
    FsNode  base;       // DEBE ser primer campo (upcasting seguro)
    size_t  file_size;
} File;

static size_t file_size(const FsNode *self) {
    const File *f = (const File *)self;  // downcast seguro
    return f->file_size;
}

static void file_print(const FsNode *self, int depth) {
    const File *f = (const File *)self;
    printf("%*s📄 %s (%zu bytes)\n", depth * 2, "", self->name, f->file_size);
}

static void file_destroy(FsNode *self) {
    free(self->name);
    free(self);
}

static const FsOps FILE_OPS = {
    .size    = file_size,
    .print   = file_print,
    .destroy = file_destroy,
};

File *file_create(const char *name, size_t size) {
    File *f = malloc(sizeof(File));
    f->base.ops  = &FILE_OPS;
    f->base.name = strdup(name);
    f->file_size = size;
    return f;
}
```

**Punto clave**: `FsNode base` es el **primer campo**. Esto permite castear
`File *` ↔ `FsNode *` sin aritmética de punteros (§6.7.2.1 del estándar C).

### 3.3 Composite: Directory

```c
typedef struct {
    FsNode   base;
    FsNode **children;   // Array dinámico de hijos (void** conceptual)
    size_t   count;
    size_t   capacity;
} Directory;

static size_t dir_size(const FsNode *self) {
    const Directory *d = (const Directory *)self;
    size_t total = 0;
    for (size_t i = 0; i < d->count; i++) {
        total += d->children[i]->ops->size(d->children[i]);  // recursión
    }
    return total;
}

static void dir_print(const FsNode *self, int depth) {
    const Directory *d = (const Directory *)self;
    printf("%*s📁 %s/\n", depth * 2, "", self->name);
    for (size_t i = 0; i < d->count; i++) {
        d->children[i]->ops->print(d->children[i], depth + 1);
    }
}

static void dir_destroy(FsNode *self) {
    Directory *d = (Directory *)self;
    for (size_t i = 0; i < d->count; i++) {
        d->children[i]->ops->destroy(d->children[i]);  // destrucción recursiva
    }
    free(d->children);
    free(d->name);
    free(d);
}

static const FsOps DIR_OPS = {
    .size    = dir_size,
    .print   = dir_print,
    .destroy = dir_destroy,
};

Directory *dir_create(const char *name) {
    Directory *d = malloc(sizeof(Directory));
    d->base.ops   = &DIR_OPS;
    d->base.name  = strdup(name);
    d->children   = NULL;
    d->count      = 0;
    d->capacity   = 0;
    return d;
}

void dir_add(Directory *parent, FsNode *child) {
    if (parent->count == parent->capacity) {
        parent->capacity = parent->capacity == 0 ? 4 : parent->capacity * 2;
        parent->children = realloc(parent->children,
                                   parent->capacity * sizeof(FsNode *));
    }
    parent->children[parent->count++] = child;
}
```

### 3.4 Uso

```c
int main(void) {
    Directory *root = dir_create("project");
    dir_add(root, (FsNode *)file_create("main.c", 2048));
    dir_add(root, (FsNode *)file_create("Makefile", 512));

    Directory *src = dir_create("src");
    dir_add(src, (FsNode *)file_create("parser.c", 4096));
    dir_add(src, (FsNode *)file_create("lexer.c", 3072));

    Directory *include = dir_create("include");
    dir_add(include, (FsNode *)file_create("parser.h", 256));

    dir_add(root, (FsNode *)src);
    dir_add(root, (FsNode *)include);

    // Interfaz uniforme: no importa si es archivo o directorio
    FsNode *node = (FsNode *)root;
    printf("Total size: %zu bytes\n", node->ops->size(node));
    node->ops->print(node, 0);

    node->ops->destroy(node);  // limpia todo recursivamente
    return 0;
}
```

Salida:
```
Total size: 9984 bytes
📁 project/
  📄 main.c (2048 bytes)
  📄 Makefile (512 bytes)
  📁 src/
    📄 parser.c (4096 bytes)
    📄 lexer.c (3072 bytes)
  📁 include/
    📄 parser.h (256 bytes)
```

Un solo `destroy` en la raíz libera todo el árbol. Un solo `size` en la raíz
suma recursivamente todo el subárbol.

---

## 4. Operaciones recursivas: más allá de size

### 4.1 Búsqueda por nombre

```c
typedef FsNode *(*FindFn)(const FsNode *self, const char *name);

// En la vtable extendida:
typedef struct {
    size_t  (*size)(const FsNode *self);
    void    (*print)(const FsNode *self, int depth);
    void    (*destroy)(FsNode *self);
    FsNode *(*find)(const FsNode *self, const char *name);
} FsOps;

// Hoja: solo compara su propio nombre
static FsNode *file_find(const FsNode *self, const char *name) {
    return strcmp(self->name, name) == 0 ? (FsNode *)self : NULL;
}

// Composite: busca en sí mismo y luego recursivamente en hijos
static FsNode *dir_find(const FsNode *self, const char *name) {
    if (strcmp(self->name, name) == 0) return (FsNode *)self;

    const Directory *d = (const Directory *)self;
    for (size_t i = 0; i < d->count; i++) {
        FsNode *found = d->children[i]->ops->find(d->children[i], name);
        if (found) return found;  // primera coincidencia (DFS)
    }
    return NULL;
}
```

### 4.2 Contar nodos

```c
// Añadir a la vtable:
size_t (*node_count)(const FsNode *self);

static size_t file_node_count(const FsNode *self) {
    (void)self;
    return 1;
}

static size_t dir_node_count(const FsNode *self) {
    const Directory *d = (const Directory *)self;
    size_t count = 1;  // el propio directorio
    for (size_t i = 0; i < d->count; i++) {
        count += d->children[i]->ops->node_count(d->children[i]);
    }
    return count;
}
```

### 4.3 Profundidad máxima

```c
size_t (*max_depth)(const FsNode *self);

static size_t file_max_depth(const FsNode *self) {
    (void)self;
    return 0;  // una hoja tiene profundidad 0
}

static size_t dir_max_depth(const FsNode *self) {
    const Directory *d = (const Directory *)self;
    size_t max = 0;
    for (size_t i = 0; i < d->count; i++) {
        size_t child_depth = d->children[i]->ops->max_depth(d->children[i]);
        if (child_depth > max) max = child_depth;
    }
    return max + 1;
}
```

**Patrón común**: la hoja retorna un valor base, el composite combina los
resultados de sus hijos con alguna operación (suma, máximo, concatenación...).

---

## 5. Array dinámico de hijos: void** children

El syllabus menciona `void** children`. Veamos la variante genérica:

```c
typedef struct {
    void  **items;      // array de punteros genéricos
    size_t  count;
    size_t  capacity;
} PtrArray;

void ptr_array_init(PtrArray *a) {
    a->items    = NULL;
    a->count    = 0;
    a->capacity = 0;
}

void ptr_array_push(PtrArray *a, void *item) {
    if (a->count == a->capacity) {
        a->capacity = a->capacity == 0 ? 4 : a->capacity * 2;
        a->items = realloc(a->items, a->capacity * sizeof(void *));
    }
    a->items[a->count++] = item;
}

void *ptr_array_get(const PtrArray *a, size_t index) {
    return index < a->count ? a->items[index] : NULL;
}

void ptr_array_free(PtrArray *a) {
    free(a->items);
    a->items    = NULL;
    a->count    = 0;
    a->capacity = 0;
}
```

Con `PtrArray`, el Directory queda más limpio:

```c
typedef struct {
    FsNode   base;
    PtrArray children;  // en lugar de FsNode** + count + capacity
} Directory;

void dir_add(Directory *d, FsNode *child) {
    ptr_array_push(&d->children, child);
}

static size_t dir_size(const FsNode *self) {
    const Directory *d = (const Directory *)self;
    size_t total = 0;
    for (size_t i = 0; i < d->children.count; i++) {
        FsNode *child = ptr_array_get(&d->children, i);
        total += child->ops->size(child);
    }
    return total;
}
```

**Trade-off**:
- `FsNode **children` → tipado, más seguro, específico
- `void **children` → genérico, reutilizable, requiere casts

---

## 6. Eliminar hijos

Un composite real necesita también remover hijos:

```c
bool dir_remove(Directory *parent, const char *name) {
    for (size_t i = 0; i < parent->count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            FsNode *removed = parent->children[i];

            // Shift: mover los siguientes una posición atrás
            memmove(&parent->children[i],
                    &parent->children[i + 1],
                    (parent->count - i - 1) * sizeof(FsNode *));
            parent->count--;

            removed->ops->destroy(removed);
            return true;
        }
    }
    return false;
}
```

```
Antes de remove("b"):    Después:
┌───┬───┬───┬───┐       ┌───┬───┬───┐
│ a │ b │ c │ d │       │ a │ c │ d │
└───┴───┴───┴───┘       └───┴───┴───┘
count = 4                count = 3

memmove mueve c y d una posición a la izquierda
```

**Cuidado con la propiedad**: `dir_remove` destruye el hijo. Si quieres extraerlo
sin destruirlo (transferir propiedad), necesitas una variante `dir_detach`:

```c
FsNode *dir_detach(Directory *parent, const char *name) {
    for (size_t i = 0; i < parent->count; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            FsNode *detached = parent->children[i];
            memmove(&parent->children[i],
                    &parent->children[i + 1],
                    (parent->count - i - 1) * sizeof(FsNode *));
            parent->count--;
            return detached;  // caller es dueño ahora
        }
    }
    return NULL;
}
```

---

## 7. Composite con tipo discriminado (union + enum)

Alternativa sin vtable, usando un tag para distinguir tipos:

```c
typedef enum { NODE_FILE, NODE_DIR } NodeType;

typedef struct Node Node;

struct Node {
    char     *name;
    NodeType  type;
    union {
        struct {                // NODE_FILE
            size_t file_size;
        } file;
        struct {                // NODE_DIR
            Node  **children;
            size_t  count;
            size_t  capacity;
        } dir;
    };
};

size_t node_size(const Node *n) {
    switch (n->type) {
        case NODE_FILE:
            return n->file.file_size;
        case NODE_DIR: {
            size_t total = 0;
            for (size_t i = 0; i < n->dir.count; i++) {
                total += node_size(n->dir.children[i]);
            }
            return total;
        }
    }
    return 0;  // unreachable
}
```

### Comparación: vtable vs tagged union

| Aspecto | Vtable | Tagged union |
|---------|--------|-------------|
| Añadir nuevo tipo | Crear nueva vtable (OCP) | Modificar todos los switch |
| Añadir nueva operación | Modificar vtable (rompe ABI) | Añadir nuevo switch |
| Rendimiento | Indirección por puntero | Branch prediction |
| Extensibilidad externa | Sí (plugins) | No (enum cerrado) |
| Seguridad de tipos | Requiere casts | Union directa |
| Memoria | Puntero a vtable por nodo | Enum tag por nodo |

**Regla práctica**:
- Pocos tipos, muchas operaciones → tagged union (switch)
- Muchos tipos, pocas operaciones → vtable (polimorfismo)

En un filesystem real: vtable (porque puedes querer symlinks, pipes, sockets...).
En un AST pequeño: tagged union (porque añades operaciones de análisis constantemente).

---

## 8. Expresiones aritméticas como composite

Un caso clásico del patrón Composite:

```c
typedef struct Expr Expr;

typedef struct {
    double (*eval)(const Expr *self);
    void   (*print)(const Expr *self);
    void   (*destroy)(Expr *self);
} ExprOps;

struct Expr {
    const ExprOps *ops;
};

// --- Hoja: literal numérico ---
typedef struct {
    Expr   base;
    double value;
} NumExpr;

static double num_eval(const Expr *self) {
    return ((const NumExpr *)self)->value;
}

static void num_print(const Expr *self) {
    printf("%.2f", ((const NumExpr *)self)->value);
}

static void num_destroy(Expr *self) { free(self); }

static const ExprOps NUM_OPS = { num_eval, num_print, num_destroy };

Expr *num_create(double value) {
    NumExpr *n = malloc(sizeof(NumExpr));
    n->base.ops = &NUM_OPS;
    n->value = value;
    return (Expr *)n;
}

// --- Composite: operación binaria ---
typedef struct {
    Expr  base;
    char  op;       // '+', '-', '*', '/'
    Expr *left;
    Expr *right;
} BinExpr;

static double bin_eval(const Expr *self) {
    const BinExpr *b = (const BinExpr *)self;
    double l = b->left->ops->eval(b->left);
    double r = b->right->ops->eval(b->right);
    switch (b->op) {
        case '+': return l + r;
        case '-': return l - r;
        case '*': return l * r;
        case '/': return r != 0 ? l / r : 0;  // simplificado
    }
    return 0;
}

static void bin_print(const Expr *self) {
    const BinExpr *b = (const BinExpr *)self;
    printf("(");
    b->left->ops->print(b->left);
    printf(" %c ", b->op);
    b->right->ops->print(b->right);
    printf(")");
}

static void bin_destroy(Expr *self) {
    BinExpr *b = (BinExpr *)self;
    b->left->ops->destroy(b->left);
    b->right->ops->destroy(b->right);
    free(b);
}

static const ExprOps BIN_OPS = { bin_eval, bin_print, bin_destroy };

Expr *bin_create(char op, Expr *left, Expr *right) {
    BinExpr *b = malloc(sizeof(BinExpr));
    b->base.ops = &BIN_OPS;
    b->op    = op;
    b->left  = left;
    b->right = right;
    return (Expr *)b;
}
```

Uso:

```c
// Representa: (3 + 4) * (10 - 2)
Expr *expr = bin_create('*',
    bin_create('+', num_create(3), num_create(4)),
    bin_create('-', num_create(10), num_create(2))
);

expr->ops->print(expr);   // ((3.00 + 4.00) * (10.00 - 2.00))
printf(" = %.2f\n", expr->ops->eval(expr));  // = 56.00
expr->ops->destroy(expr);
```

```
Árbol de la expresión:

        [*]
       /   \
     [+]   [-]
    / \    / \
   3   4  10  2

eval recorre post-order:
  eval(+) = eval(3) + eval(4) = 7
  eval(-) = eval(10) - eval(2) = 8
  eval(*) = 7 * 8 = 56
```

---

## 9. Referencia al padre

A veces necesitas que un nodo conozca su padre (ej: obtener ruta completa):

```c
struct FsNode {
    const FsOps *ops;
    char        *name;
    FsNode      *parent;  // NULL para la raíz
};

void dir_add(Directory *parent, FsNode *child) {
    // ... (push al array) ...
    child->parent = (FsNode *)parent;  // establecer referencia al padre
}

// Construir ruta completa subiendo al padre recursivamente
void node_path(const FsNode *node, char *buf, size_t buf_size) {
    if (node->parent) {
        node_path(node->parent, buf, buf_size);
        strncat(buf, "/", buf_size - strlen(buf) - 1);
    }
    strncat(buf, node->name, buf_size - strlen(buf) - 1);
}
```

```
         root (parent = NULL)
        /    \
      src     include
     (↑root)  (↑root)
    /    \
  a.c   b.c
 (↑src) (↑src)

node_path(b.c) → "root/src/b.c"
```

**Riesgos del puntero al padre**:
1. **Ciclos**: si actualizas mal, un nodo puede apuntar a sí mismo
2. **Dangling**: si mueves un hijo sin actualizar `parent`
3. **Propiedad confusa**: ¿quién libera qué? El padre destruye hijos, no al revés

**Regla**: actualizar `parent` en `dir_add` y en `dir_remove`/`dir_detach`.

---

## 10. Composite con operaciones acumuladoras

Patrón útil: pasar un acumulador a la recursión en lugar de retornar valores.

```c
// Recolectar todos los archivos mayores a un umbral
typedef struct {
    FsNode **results;
    size_t   count;
    size_t   capacity;
} FileList;

void file_list_push(FileList *list, FsNode *node) {
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->results = realloc(list->results,
                                list->capacity * sizeof(FsNode *));
    }
    list->results[list->count++] = node;
}

// Añadir a la vtable:
void (*find_large)(const FsNode *self, size_t threshold, FileList *out);

static void file_find_large(const FsNode *self, size_t threshold,
                            FileList *out) {
    const File *f = (const File *)self;
    if (f->file_size >= threshold) {
        file_list_push(out, (FsNode *)self);
    }
}

static void dir_find_large(const FsNode *self, size_t threshold,
                           FileList *out) {
    const Directory *d = (const Directory *)self;
    for (size_t i = 0; i < d->count; i++) {
        d->children[i]->ops->find_large(d->children[i], threshold, out);
    }
}
```

Uso:

```c
FileList large = {0};
root->ops->find_large(root, 2048, &large);

printf("Files >= 2048 bytes:\n");
for (size_t i = 0; i < large.count; i++) {
    printf("  %s\n", large.results[i]->name);
}
free(large.results);
// NO destruir los nodos: pertenecen al árbol
```

**Ventaja sobre retornar arrays**: un solo array acumulado, sin merges en cada nivel.

---

## 11. Ejemplos reales de Composite en C

### 11.1 Widgets de GUI

```c
typedef struct Widget Widget;

typedef struct {
    void (*draw)(const Widget *self, int x, int y);
    void (*handle_event)(Widget *self, int event);
    void (*destroy)(Widget *self);
} WidgetOps;

struct Widget {
    const WidgetOps *ops;
    int x, y, width, height;
};

// Panel es un composite de widgets
typedef struct {
    Widget    base;
    Widget  **children;
    size_t    count;
    size_t    capacity;
} Panel;

static void panel_draw(const Widget *self, int x, int y) {
    const Panel *p = (const Panel *)self;
    // Dibujar fondo del panel...
    for (size_t i = 0; i < p->count; i++) {
        Widget *child = p->children[i];
        // Coordenadas relativas al panel
        child->ops->draw(child, x + child->x, y + child->y);
    }
}
```

### 11.2 Organigrama / estructura organizacional

```c
typedef struct Employee Employee;

struct Employee {
    char       *name;
    char       *role;
    double      salary;
    Employee  **reports;    // subordinados directos
    size_t      report_count;
    size_t      report_cap;
};

double total_salary(const Employee *e) {
    double total = e->salary;
    for (size_t i = 0; i < e->report_count; i++) {
        total += total_salary(e->reports[i]);
    }
    return total;
}

size_t team_size(const Employee *e) {
    size_t count = 1;
    for (size_t i = 0; i < e->report_count; i++) {
        count += team_size(e->reports[i]);
    }
    return count;
}
```

Aquí no hay distinción hoja/composite explícita: un empleado sin subordinados
es implícitamente una hoja (report_count == 0).

---

## 12. Protección contra ciclos

En una estructura de árbol, los ciclos son un error grave (recursión infinita):

```c
// ¡PELIGRO! Esto crea un ciclo:
dir_add(root, (FsNode *)src);
dir_add(src, (FsNode *)root);  // root → src → root → src → ...
```

Detección simple al agregar:

```c
static bool is_ancestor(const FsNode *node, const FsNode *potential_ancestor) {
    const FsNode *current = node;
    while (current != NULL) {
        if (current == potential_ancestor) return true;
        current = current->parent;
    }
    return false;
}

bool dir_add_safe(Directory *parent, FsNode *child) {
    // Verificar que el hijo no sea ancestro del padre
    if (is_ancestor((FsNode *)parent, child)) {
        fprintf(stderr, "Error: adding '%s' would create a cycle\n",
                child->name);
        return false;
    }
    dir_add(parent, child);
    return true;
}
```

**Nota**: esta verificación requiere punteros al padre. Sin ellos, la detección
de ciclos requiere marcar nodos visitados durante el recorrido (más costoso).

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `base` no es primer campo | Cast `FsNode* → File*` produce basura | Siempre primer campo |
| No destruir hijos | Memory leak en todo el subárbol | `destroy` recursivo en composite |
| Destruir hijo y seguir usándolo | Use-after-free | `dir_remove` invalida el nodo |
| Ciclos en el árbol | Stack overflow por recursión infinita | Verificar antes de `dir_add` |
| `void**` sin cast correcto | Tipo incorrecto al desreferenciar | Usar `FsNode**` tipado |
| Olvidar actualizar `parent` | Puntero al padre inconsistente | Actualizar en add/remove/detach |
| `realloc` sin guardar resultado | Si falla, se pierde el puntero original | `tmp = realloc(...)` y verificar |
| Switch sin `default` | Tipo nuevo no manejado silenciosamente | `-Werror=switch` o default con abort |
| Mezclar propiedad | Un nodo en dos padres a la vez | Un nodo tiene exactamente un dueño |
| Recursión sin caso base | Hoja que intenta iterar hijos | Hoja retorna valor directo |

---

## 14. Ejercicios

### Ejercicio 1 — Estructura del composite

**Predicción**: dado este código, ¿qué imprime `node->ops->size(node)`?

```c
Directory *root = dir_create("root");
dir_add(root, (FsNode *)file_create("a.txt", 100));
dir_add(root, (FsNode *)file_create("b.txt", 200));

Directory *sub = dir_create("sub");
dir_add(sub, (FsNode *)file_create("c.txt", 50));
dir_add(root, (FsNode *)sub);

FsNode *node = (FsNode *)root;
printf("%zu\n", node->ops->size(node));
```

<details>
<summary>Respuesta</summary>

Imprime `350`.

```
root
├── a.txt (100)
├── b.txt (200)
└── sub/
    └── c.txt (50)

dir_size(root):
  file_size(a.txt) = 100
  file_size(b.txt) = 200
  dir_size(sub):
    file_size(c.txt) = 50
  total sub = 50
  total root = 100 + 200 + 50 = 350
```

La recursión desciende a todos los niveles automáticamente.
</details>

---

### Ejercicio 2 — Orden de destrucción

**Predicción**: si añadimos un `printf` al inicio de `file_destroy` y `dir_destroy`,
¿en qué orden se imprimen los nombres al hacer `root->ops->destroy(root)` con la
estructura del Ejercicio 1?

<details>
<summary>Respuesta</summary>

```
destroying: a.txt
destroying: b.txt
destroying: c.txt
destroying: sub
destroying: root
```

Es post-order: primero se destruyen los hijos (archivos), luego el directorio.
`dir_destroy` itera sus hijos llamando `destroy` en cada uno, y luego se destruye
a sí mismo. Es el mismo orden que `rm -rf`: primero el contenido, luego el
contenedor.
</details>

---

### Ejercicio 3 — Cast incorrecto

**Predicción**: ¿qué ocurre si `base` NO es el primer campo de `File`?

```c
typedef struct {
    size_t  file_size;  // ¡base ya no es primer campo!
    FsNode  base;
} File;

File *f = file_create("test.txt", 1024);
FsNode *node = (FsNode *)f;  // cast directo
printf("%s\n", node->name);
```

<details>
<summary>Respuesta</summary>

**Comportamiento indefinido**. Al castear `File *` a `FsNode *`, el compilador
interpreta los primeros bytes como `FsNode`. Pero ahora los primeros bytes son
`file_size` (1024 = 0x400), no un puntero a vtable.

```
Memoria de File con base como primer campo:
[ops ptr][name ptr][file_size]
  ↑ FsNode empieza aquí → correcto

Memoria de File con file_size primero:
[file_size][ops ptr][name ptr]
  ↑ FsNode empieza aquí → ops = 1024 (¡basura!)
```

`node->ops` apunta a la dirección 0x400, que no es una vtable válida.
Acceder a `node->ops->size` provoca segfault o corrupción.

Regla: **siempre** poner la struct base como primer campo.
</details>

---

### Ejercicio 4 — Nodo count

**Predicción**: implementa `node_count` para la vtable. ¿Cuántos nodos retorna
para la estructura del Ejercicio 1?

<details>
<summary>Respuesta</summary>

```c
static size_t file_node_count(const FsNode *self) {
    (void)self;
    return 1;
}

static size_t dir_node_count(const FsNode *self) {
    const Directory *d = (const Directory *)self;
    size_t count = 1;  // el propio directorio cuenta como nodo
    for (size_t i = 0; i < d->count; i++) {
        count += d->children[i]->ops->node_count(d->children[i]);
    }
    return count;
}
```

Retorna **5**: root(1) + a.txt(1) + b.txt(1) + sub(1) + c.txt(1).

El directorio se cuenta a sí mismo (count = 1) y luego suma los resultados
de sus hijos.
</details>

---

### Ejercicio 5 — Tagged union vs vtable

**Predicción**: reescribe el filesystem del ejercicio 1 usando tagged union.
¿Qué cambia en la función `size`?

<details>
<summary>Respuesta</summary>

```c
typedef enum { NODE_FILE, NODE_DIR } NodeType;
typedef struct Node Node;

struct Node {
    char     *name;
    NodeType  type;
    union {
        struct { size_t file_size; } file;
        struct {
            Node  **children;
            size_t  count;
            size_t  capacity;
        } dir;
    };
};

size_t node_size(const Node *n) {
    switch (n->type) {
        case NODE_FILE:
            return n->file.file_size;
        case NODE_DIR: {
            size_t total = 0;
            for (size_t i = 0; i < n->dir.count; i++) {
                total += node_size(n->dir.children[i]);
            }
            return total;
        }
    }
    return 0;
}
```

Diferencias clave:
- No hay vtable ni indirección de puntero a función
- La función `node_size` contiene toda la lógica (switch)
- No hay casts entre tipos
- Para añadir un tipo nuevo (ej: symlink) hay que modificar **todos** los switch
- Para añadir una operación nueva, solo escribes un nuevo switch (no tocas structs)
</details>

---

### Ejercicio 6 — Detach y reattach

**Predicción**: ¿qué imprime este código?

```c
Directory *root = dir_create("root");
Directory *docs = dir_create("docs");
dir_add(docs, (FsNode *)file_create("readme.md", 500));
dir_add(root, (FsNode *)docs);

printf("Before: %zu\n", ((FsNode *)root)->ops->size((FsNode *)root));

FsNode *detached = dir_detach(root, "docs");

printf("After detach: %zu\n", ((FsNode *)root)->ops->size((FsNode *)root));

Directory *archive = dir_create("archive");
dir_add(archive, detached);
dir_add(root, (FsNode *)archive);

printf("After reattach: %zu\n", ((FsNode *)root)->ops->size((FsNode *)root));
```

<details>
<summary>Respuesta</summary>

```
Before: 500
After detach: 0
After reattach: 500
```

1. **Before**: root → docs → readme.md(500). Total = 500.
2. **After detach**: docs se extrae de root. root no tiene hijos. Total = 0.
   Pero `docs` (y su hijo readme.md) siguen vivos en memoria.
3. **After reattach**: docs se añade dentro de archive, archive se añade a root.
   root → archive → docs → readme.md(500). Total = 500.

`dir_detach` transfiere propiedad sin destruir. El nodo extraído puede
reinsertarse en otro lugar del árbol.
</details>

---

### Ejercicio 7 — Detección de ciclos

**Predicción**: con punteros al padre implementados, ¿qué retorna
`dir_add_safe(sub, (FsNode *)root)` si root → sub?

<details>
<summary>Respuesta</summary>

Retorna `false` e imprime el error.

```
is_ancestor((FsNode *)sub, (FsNode *)root):
  current = sub
  sub->parent == root → ¡es ancestro!
  return true

dir_add_safe detecta que root es ancestro de sub
→ añadir root como hijo de sub crearía: root → sub → root → ...
→ rechaza la operación
```

Sin esta verificación, `dir_size` entraría en recursión infinita:
`dir_size(root) → dir_size(sub) → dir_size(root) → ...` hasta stack overflow.
</details>

---

### Ejercicio 8 — Acumulador find_large

**Predicción**: dado este árbol, ¿cuántos archivos encuentra `find_large` con
threshold = 1000?

```c
Directory *root = dir_create("root");
dir_add(root, (FsNode *)file_create("small.txt", 100));
dir_add(root, (FsNode *)file_create("big.txt", 5000));

Directory *data = dir_create("data");
dir_add(data, (FsNode *)file_create("log.csv", 2000));
dir_add(data, (FsNode *)file_create("config.ini", 50));
dir_add(data, (FsNode *)file_create("dump.bin", 10000));
dir_add(root, (FsNode *)data);

FileList results = {0};
((FsNode *)root)->ops->find_large((FsNode *)root, 1000, &results);
printf("Found: %zu\n", results.count);
```

<details>
<summary>Respuesta</summary>

Imprime `Found: 3`.

Los archivos que cumplen `file_size >= 1000`:
1. `big.txt` (5000) ✓
2. `log.csv` (2000) ✓
3. `dump.bin` (10000) ✓

Los que no cumplen:
- `small.txt` (100) ✗
- `config.ini` (50) ✗

Los directorios (`root`, `data`) no son archivos, así que `dir_find_large`
solo delega a sus hijos sin añadirse a sí mismo.

El acumulador `FileList` se pasa por puntero a través de toda la recursión,
evitando crear y mergear arrays temporales en cada nivel.
</details>

---

### Ejercicio 9 — Expresiones aritméticas

**Predicción**: ¿qué imprime `eval` para este árbol de expresiones?

```c
Expr *expr = bin_create('+',
    bin_create('*', num_create(2), num_create(3)),
    bin_create('/', num_create(10), num_create(4))
);
```

<details>
<summary>Respuesta</summary>

Imprime `8.50`.

```
        [+]
       /   \
     [*]   [/]
    / \    / \
   2   3  10  4

Evaluación post-order:
  eval(*) = eval(2) * eval(3) = 6.00
  eval(/) = eval(10) / eval(4) = 2.50
  eval(+) = 6.00 + 2.50 = 8.50
```

Cada nodo hoja retorna su valor. Cada nodo binario evalúa recursivamente
sus dos hijos y aplica la operación.
</details>

---

### Ejercicio 10 — Memoria y propiedad

**Predicción**: ¿hay memory leak en este código? Si sí, ¿dónde?

```c
Directory *root = dir_create("root");
File *f = file_create("data.bin", 4096);
dir_add(root, (FsNode *)f);

// Alguien decide liberar f directamente:
f->base.ops->destroy((FsNode *)f);

// Más tarde:
((FsNode *)root)->ops->destroy((FsNode *)root);
```

<details>
<summary>Respuesta</summary>

No hay memory leak, pero hay algo **peor**: **double free** (use-after-free).

Secuencia:
1. `file_create` → malloc para File, strdup para name
2. `dir_add` → root->children[0] = (FsNode *)f
3. `destroy(f)` → free(name), free(f). Ahora f es memoria liberada
4. `destroy(root)` → itera children, llama `children[0]->ops->destroy(...)`.
   Pero children[0] apunta a f, que ya fue liberado → **double free**

```
Estado después del paso 3:

root->children[0] ─────→ [memoria liberada]
                          ↑ dangling pointer

dir_destroy intenta acceder → undefined behavior
```

**Regla de propiedad**: una vez que añades un nodo a un composite, el composite
es el dueño. No destruyas el nodo directamente; usa `dir_remove` o `dir_detach`.
</details>