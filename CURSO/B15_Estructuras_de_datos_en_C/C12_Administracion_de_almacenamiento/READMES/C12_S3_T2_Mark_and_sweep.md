# T02 — Mark-and-sweep

## Objetivo

Comprender el algoritmo **mark-and-sweep** como la estrategia fundamental
de *tracing garbage collection*: como el colector identifica objetos
vivos recorriendo el grafo de referencias desde las **raices**, y
como libera todo lo que queda sin marcar. Implementar una version
simplificada en C y explorar los conceptos equivalentes en Rust.

---

## 1. De reference counting a tracing

### 1.1 El problema que resuelve mark-and-sweep

En T01 vimos que el conteo de referencias falla con **ciclos**: objetos
que se referencian mutuamente nunca llegan a rc=0, aunque sean
inaccesibles desde el programa. Mark-and-sweep resuelve esto con un
enfoque radicalmente diferente:

> En lugar de preguntar *"cuantas referencias apuntan a este objeto?"*,
> pregunta *"es este objeto alcanzable desde las raices?"*

Si un objeto no es alcanzable — sin importar cuantas referencias internas
tenga — se considera **basura** y se libera.

### 1.2 Taxonomia de garbage collectors

```
Garbage Collection
├── Reference counting (T01)
│   └── Libera al llegar a rc=0
└── Tracing GC
    ├── Mark-and-sweep  <-- este topico
    ├── Mark-and-compact
    ├── Copying (semi-space)
    └── Generacional (T03)
```

Todos los *tracing GC* comparten la idea de recorrer el grafo de objetos
desde las raices. La diferencia esta en **que hacen con la basura**:
sweep la libera in-place, compact la compacta, copying copia los vivos
a otro espacio.

---

## 2. El algoritmo mark-and-sweep

### 2.1 Componentes

El colector necesita:

1. **Root set** (conjunto raiz): punteros desde los cuales comienza el
   recorrido. Tipicamente incluyen:
   - Variables locales en el stack.
   - Variables globales/estaticas.
   - Registros del CPU.

2. **Bit de marca** (*mark bit*): un bit por objeto que indica si fue
   visitado durante la fase de marcado. Puede almacenarse:
   - En el header del objeto (inline).
   - En un bitmap separado (mejor localidad de cache).

3. **Lista de todos los objetos**: el colector debe poder iterar sobre
   *todos* los objetos asignados para barrer los no marcados.

### 2.2 Fase 1 — Mark (marcado)

Recorrer el grafo de objetos desde las raices usando DFS o BFS:

```
mark(root):
    if root.marked:
        return
    root.marked = true
    for each reference ref in root:
        mark(ref)
```

Al finalizar, todo objeto con `marked = true` es **alcanzable** (vivo).
Todo objeto con `marked = false` es **basura**.

```
Antes del marcado:

  [stack]
    |
    v
    A -----> B -----> C
             |
             v
             D

    E -----> F (ciclo: F->E)

Despues del marcado:

    A [✓] --> B [✓] --> C [✓]
              |
              v
              D [✓]

    E [✗] --> F [✗]   <-- basura (ciclo inaccesible)
```

El ciclo E↔F no importa: ninguno es alcanzable desde las raices,
asi que ambos quedan sin marcar.

### 2.3 Fase 2 — Sweep (barrido)

Recorrer la lista de **todos** los objetos asignados:

```
sweep():
    for each object in all_objects:
        if object.marked:
            object.marked = false   // resetear para el proximo ciclo
        else:
            free(object)            // basura: liberar
```

### 2.4 Pseudocodigo completo

```
gc_collect():
    // Fase 1: Mark
    for each root in root_set:
        mark(root)

    // Fase 2: Sweep
    for each object in all_objects:
        if object.marked:
            object.marked = false
        else:
            remove from all_objects
            free(object)
```

---

## 3. La abstraccion tricolor

### 3.1 Concepto

La abstraccion tricolor (Dijkstra, 1978) describe el estado del
marcado usando tres colores:

| Color | Significado |
|-------|-------------|
| **Blanco** | No visitado. Si sigue blanco al final, es basura |
| **Gris** | Visitado, pero sus hijos aun no han sido procesados |
| **Negro** | Visitado y todos sus hijos procesados |

### 3.2 Invariante tricolor

> Ningun objeto **negro** apunta directamente a un objeto **blanco**.

Esta invariante es fundamental para la correccion del algoritmo. Si se
mantiene, ningun objeto vivo sera liberado accidentalmente.

### 3.3 Progresion del marcado

```
Inicio:          raices = gris, todo lo demas = blanco
Durante mark:    gris -> procesar hijos -> negro
                 hijos blancos -> gris
Fin:             solo negro (vivo) y blanco (basura)
                 no quedan grises
```

Ejemplo paso a paso:

```
Grafo: root -> A -> B -> C
               |
               +-> D

Paso 0: root[gris]  A[blanco] B[blanco] C[blanco] D[blanco]
Paso 1: root[negro] A[gris]   B[blanco] C[blanco] D[blanco]
Paso 2: root[negro] A[negro]  B[gris]   C[blanco] D[gris]
Paso 3: root[negro] A[negro]  B[negro]  C[gris]   D[negro]
Paso 4: root[negro] A[negro]  B[negro]  C[negro]  D[negro]

Worklist vacia -> todo negro es vivo, todo blanco es basura.
```

### 3.4 DFS vs BFS para el marcado

| Aspecto | DFS (stack) | BFS (queue) |
|---------|:-----------:|:-----------:|
| Estructura | Stack (recursivo o explicito) | Cola |
| Uso de memoria | Profundidad maxima del grafo | Anchura maxima del nivel |
| Localidad | Peor (salta en profundidad) | Mejor para grafos anchos |
| Desbordamiento de stack | Riesgo con grafos profundos | No |
| Implementacion comun | Recursiva (simple) | Iterativa con cola |

En la practica, se prefiere DFS con **stack explicito** (no recursivo)
para evitar desbordamiento del stack del programa.

---

## 4. Stop-the-world y sus implicaciones

### 4.1 El problema

Mark-and-sweep basico requiere **detener** la ejecucion del programa
(el *mutator*) durante la coleccion:

```
Tiempo -->
[  mutator  ][ STOP ][ mark ][ sweep ][ RESUME ][ mutator ]
                      |<--- pausa GC --->|
```

Si el mutator modificara punteros durante el marcado, podria:
- Crear una referencia de un objeto negro a un objeto blanco (violando
  la invariante tricolor).
- Causar que un objeto vivo sea tratado como basura y liberado.

### 4.2 Duracion de la pausa

La pausa depende de:

- **Mark**: proporcional al numero de objetos **vivos** (debe recorrerlos).
- **Sweep**: proporcional al numero **total** de objetos (debe examinar todos).

Para un heap de N objetos totales y L objetos vivos:

Costo mark = O(L), costo sweep = O(N)

### 4.3 Alternativas al stop-the-world

| Tecnica | Idea | Complejidad |
|---------|------|:-----------:|
| Incremental | Hacer GC en pequenos pasos entre operaciones del mutator | Media |
| Concurrent | GC corre en thread paralelo al mutator | Alta |
| Write barrier | Interceptar escrituras de punteros para mantener invariante tricolor | Media |

Estos conceptos se expandiran en T03 (Generacional), donde veremos
como los colectores modernos minimizan las pausas.

---

## 5. Comparacion con otras estrategias

| Aspecto | Ref counting | Mark-sweep | Mark-compact | Copying GC |
|---------|:-----------:|:----------:|:------------:|:----------:|
| Ciclos | Leak | Detectados | Detectados | Detectados |
| Fragmentacion | Si | Si | No | No |
| Costo mark | N/A | O(vivos) | O(vivos) | O(vivos) |
| Costo sweep/compact | N/A | O(total) | O(total) | O(vivos) |
| Mueve objetos | No | No | Si | Si |
| Pausa | Ninguna* | Si | Si (mayor) | Si |
| Throughput | Menor (overhead constante) | Mayor (batch) | Mayor | Mayor |

Mark-compact agrega una fase que mueve objetos para eliminar
fragmentacion (requiere actualizar todos los punteros). Copying GC
copia los vivos a un nuevo espacio — su costo es O(vivos), no O(total),
pero requiere el doble de memoria.

---

## 6. Mark-and-sweep en lenguajes reales

### 6.1 Donde se usa

| Lenguaje | Estrategia GC | Mark-sweep? |
|----------|---------------|:-----------:|
| Java | Generacional + G1/ZGC | Si (como base) |
| Go | Concurrent tricolor mark-sweep | Si |
| Python | RC + generational mark-sweep | Si (backup) |
| JavaScript (V8) | Generational + mark-compact | Si (old gen) |
| Lua | Incremental mark-sweep | Si |
| Ruby | Generational mark-sweep | Si |

### 6.2 Go: concurrent tricolor mark-sweep

Go usa mark-and-sweep **concurrente** con la abstraccion tricolor.
El GC corre en paralelo al programa con **write barriers** para
mantener la invariante tricolor. Resultado: pausas tipicamente
menores a 1 ms, incluso con heaps de varios GB.

### 6.3 Python: RC + mark-sweep

Python usa reference counting como mecanismo primario, pero agrega
un colector de ciclos basado en mark-sweep generacional para detectar
ciclos. El modulo `gc` permite controlar el colector:

```python
import gc
gc.collect()           # forzar coleccion
gc.get_threshold()     # (700, 10, 10) por defecto
gc.disable()           # desactivar colector de ciclos
```

---

## 7. Programa en C — Mini garbage collector

Este programa implementa un **mini garbage collector** con mark-and-sweep.
Cada "objeto" vive en un heap simulado y puede tener referencias a otros
objetos. El colector recorre desde las raices, marca los alcanzables, y
libera el resto.

### Estructura del GC

```c
#define MAX_OBJECTS  64
#define MAX_REFS      4

typedef struct GCObject {
    int          id;
    int          marked;
    int          alive;          // 1 = allocated, 0 = freed
    int          refs[MAX_REFS]; // indices to other objects (-1 = none)
    int          ref_count;      // number of refs (not RC, just count of edges)
    const char  *label;
} GCObject;

typedef struct VM {
    GCObject objects[MAX_OBJECTS];
    int      count;              // total objects allocated
    int      roots[MAX_REFS];    // root set (indices, -1 = none)
    int      root_count;
} VM;
```

### Funciones del GC

```c
// Allocate a new object
int vm_alloc(VM *vm, const char *label);

// Add a reference from src to dst
void vm_add_ref(VM *vm, int src, int dst);

// Add a root
void vm_add_root(VM *vm, int obj);

// Remove a root
void vm_remove_root(VM *vm, int obj);

// Mark phase (DFS)
void gc_mark(VM *vm, int index);

// Sweep phase
int gc_sweep(VM *vm);

// Full collection
int gc_collect(VM *vm);

// Print state
void vm_print(VM *vm);
```

```c
#include <stdio.h>
#include <string.h>

/* ============================================================
 *  Mini Garbage Collector — Mark-and-Sweep
 * ============================================================ */

#define MAX_OBJECTS  64
#define MAX_REFS      4

typedef struct GCObject {
    int          id;
    int          marked;
    int          alive;
    int          refs[MAX_REFS];
    int          ref_count;
    const char  *label;
} GCObject;

typedef struct VM {
    GCObject objects[MAX_OBJECTS];
    int      count;
    int      roots[MAX_REFS];
    int      root_count;
} VM;

/* ---- VM operations ---- */

void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));
    for (int i = 0; i < MAX_OBJECTS; i++) {
        for (int j = 0; j < MAX_REFS; j++)
            vm->objects[i].refs[j] = -1;
    }
    for (int i = 0; i < MAX_REFS; i++)
        vm->roots[i] = -1;
}

int vm_alloc(VM *vm, const char *label) {
    if (vm->count >= MAX_OBJECTS) {
        printf("  [vm] heap full!\n");
        return -1;
    }
    int idx = vm->count++;
    GCObject *obj = &vm->objects[idx];
    obj->id        = idx;
    obj->marked    = 0;
    obj->alive     = 1;
    obj->ref_count = 0;
    obj->label     = label;
    for (int j = 0; j < MAX_REFS; j++)
        obj->refs[j] = -1;
    return idx;
}

void vm_add_ref(VM *vm, int src, int dst) {
    GCObject *obj = &vm->objects[src];
    if (obj->ref_count < MAX_REFS) {
        obj->refs[obj->ref_count++] = dst;
    }
}

void vm_add_root(VM *vm, int obj) {
    if (vm->root_count < MAX_REFS) {
        vm->roots[vm->root_count++] = obj;
    }
}

void vm_remove_root(VM *vm, int obj) {
    for (int i = 0; i < vm->root_count; i++) {
        if (vm->roots[i] == obj) {
            vm->roots[i] = vm->roots[--vm->root_count];
            vm->roots[vm->root_count] = -1;
            return;
        }
    }
}

/* ---- Mark phase (DFS recursive) ---- */

void gc_mark(VM *vm, int index) {
    if (index < 0 || index >= vm->count) return;
    GCObject *obj = &vm->objects[index];
    if (!obj->alive || obj->marked) return;

    obj->marked = 1;
    for (int i = 0; i < obj->ref_count; i++) {
        gc_mark(vm, obj->refs[i]);
    }
}

/* ---- Sweep phase ---- */

int gc_sweep(VM *vm) {
    int freed = 0;
    for (int i = 0; i < vm->count; i++) {
        GCObject *obj = &vm->objects[i];
        if (!obj->alive) continue;
        if (obj->marked) {
            obj->marked = 0;   /* reset for next cycle */
        } else {
            printf("  [gc] sweep: freeing object %d (%s)\n",
                   obj->id, obj->label);
            obj->alive = 0;
            freed++;
        }
    }
    return freed;
}

/* ---- Full GC cycle ---- */

int gc_collect(VM *vm) {
    printf("  [gc] === collection start ===\n");

    /* Mark from roots */
    for (int i = 0; i < vm->root_count; i++) {
        if (vm->roots[i] >= 0) {
            gc_mark(vm, vm->roots[i]);
        }
    }

    /* Sweep */
    int freed = gc_sweep(vm);
    printf("  [gc] === collection end: freed %d object(s) ===\n", freed);
    return freed;
}

/* ---- Print state ---- */

void vm_print(VM *vm) {
    printf("  Objects:\n");
    for (int i = 0; i < vm->count; i++) {
        GCObject *obj = &vm->objects[i];
        if (!obj->alive) {
            printf("    [%d] %-10s  (freed)\n", obj->id, obj->label);
            continue;
        }
        printf("    [%d] %-10s  alive  refs=[", obj->id, obj->label);
        for (int j = 0; j < obj->ref_count; j++) {
            if (j > 0) printf(",");
            printf("%d", obj->refs[j]);
        }
        printf("]\n");
    }
    printf("  Roots: [");
    for (int i = 0; i < vm->root_count; i++) {
        if (i > 0) printf(",");
        printf("%d", vm->roots[i]);
    }
    printf("]\n");
}

/* ============================================================
 *  Demo 1: Basic mark-and-sweep
 *  Create a chain root->A->B->C and an unreachable D.
 *  GC should free D only.
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: Basic mark-and-sweep ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "A");
    int b = vm_alloc(&vm, "B");
    int c = vm_alloc(&vm, "C");
    int d = vm_alloc(&vm, "D");  /* unreachable */

    vm_add_ref(&vm, a, b);
    vm_add_ref(&vm, b, c);
    vm_add_root(&vm, a);

    printf("Before GC:\n");
    vm_print(&vm);

    gc_collect(&vm);

    printf("After GC:\n");
    vm_print(&vm);
    /* D should be freed, A/B/C alive */
}

/* ============================================================
 *  Demo 2: Cycles collected correctly
 *  Create cycle E->F->G->E with no root. All should be freed.
 * ============================================================ */
void demo2(void) {
    printf("\n=== Demo 2: Cycles collected correctly ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "root_obj");
    int e = vm_alloc(&vm, "E");
    int f = vm_alloc(&vm, "F");
    int g = vm_alloc(&vm, "G");

    /* root_obj is rooted */
    vm_add_root(&vm, a);

    /* cycle: E -> F -> G -> E (no root points to them) */
    vm_add_ref(&vm, e, f);
    vm_add_ref(&vm, f, g);
    vm_add_ref(&vm, g, e);

    printf("Before GC (cycle E->F->G->E is unreachable):\n");
    vm_print(&vm);

    gc_collect(&vm);

    printf("After GC:\n");
    vm_print(&vm);
    /* E, F, G freed despite cycle — this is what RC can't do */
}

/* ============================================================
 *  Demo 3: Reachable cycle survives
 *  Create cycle A->B->C->A with root pointing to A.
 *  All survive because reachable from root.
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Reachable cycle survives ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "A");
    int b = vm_alloc(&vm, "B");
    int c = vm_alloc(&vm, "C");

    vm_add_ref(&vm, a, b);
    vm_add_ref(&vm, b, c);
    vm_add_ref(&vm, c, a);  /* cycle */
    vm_add_root(&vm, a);

    printf("Before GC (cycle A->B->C->A, rooted at A):\n");
    vm_print(&vm);

    gc_collect(&vm);

    printf("After GC (all survive — reachable from root):\n");
    vm_print(&vm);
}

/* ============================================================
 *  Demo 4: Tricolor marking trace
 *  Show step-by-step tricolor states during mark phase.
 * ============================================================ */
void demo4(void) {
    printf("\n=== Demo 4: Tricolor marking trace ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "A");
    int b = vm_alloc(&vm, "B");
    int c = vm_alloc(&vm, "C");
    int d = vm_alloc(&vm, "D");
    int e = vm_alloc(&vm, "E");  /* unreachable */

    /* root -> A -> B -> C, A -> D */
    vm_add_ref(&vm, a, b);
    vm_add_ref(&vm, a, d);
    vm_add_ref(&vm, b, c);
    vm_add_root(&vm, a);

    printf("Tricolor trace (DFS from root A):\n\n");

    /* Manual tricolor simulation */
    /* State: white=0, gray=1, black=2 */
    int color[5] = {0, 0, 0, 0, 0}; /* all white */
    const char *names[] = {"A", "B", "C", "D", "E"};
    const char *color_names[] = {"white", "gray ", "black"};

    /* Step 0: initial */
    printf("  Step 0 (initial):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    /* Step 1: root A becomes gray */
    color[a] = 1;
    printf("\n  Step 1 (A grayed from root set):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    /* Step 2: process A -> gray children B,D; A becomes black */
    color[a] = 2;
    color[b] = 1;
    color[d] = 1;
    printf("\n  Step 2 (process A: children B,D grayed; A black):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    /* Step 3: process B -> gray child C; B becomes black */
    color[b] = 2;
    color[c] = 1;
    printf("\n  Step 3 (process B: child C grayed; B black):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    /* Step 4: process C -> no children; C becomes black */
    color[c] = 2;
    printf("\n  Step 4 (process C: no children; C black):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    /* Step 5: process D -> no children; D becomes black */
    color[d] = 2;
    printf("\n  Step 5 (process D: no children; D black):\n");
    for (int i = 0; i < 5; i++)
        printf("    %s: %s\n", names[i], color_names[color[i]]);

    printf("\n  Final: white objects (E) are garbage.\n");

    /* Run actual GC to confirm */
    gc_collect(&vm);
    printf("After GC:\n");
    vm_print(&vm);
}

/* ============================================================
 *  Demo 5: Multiple GC cycles
 *  Objects become unreachable in successive cycles.
 * ============================================================ */
void demo5(void) {
    printf("\n=== Demo 5: Multiple GC cycles ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "A");
    int b = vm_alloc(&vm, "B");
    int c = vm_alloc(&vm, "C");
    int d = vm_alloc(&vm, "D");

    vm_add_ref(&vm, a, b);
    vm_add_ref(&vm, a, c);
    vm_add_ref(&vm, c, d);
    vm_add_root(&vm, a);

    printf("--- Cycle 1: all reachable from A ---\n");
    vm_print(&vm);
    gc_collect(&vm);
    vm_print(&vm);

    /* Remove root -> everything becomes garbage */
    printf("--- Cycle 2: remove root (A becomes unreachable) ---\n");
    vm_remove_root(&vm, a);
    gc_collect(&vm);
    vm_print(&vm);
}

/* ============================================================
 *  Demo 6: Iterative (BFS) mark with explicit queue
 *  Same graph, but uses a queue instead of recursion.
 * ============================================================ */

void gc_mark_bfs(VM *vm) {
    int queue[MAX_OBJECTS];
    int head = 0, tail = 0;

    /* Enqueue roots */
    for (int i = 0; i < vm->root_count; i++) {
        int r = vm->roots[i];
        if (r >= 0 && vm->objects[r].alive && !vm->objects[r].marked) {
            vm->objects[r].marked = 1;
            queue[tail++] = r;
        }
    }

    /* BFS */
    while (head < tail) {
        int idx = queue[head++];
        GCObject *obj = &vm->objects[idx];
        printf("  [bfs] visiting %d (%s)\n", obj->id, obj->label);
        for (int i = 0; i < obj->ref_count; i++) {
            int child = obj->refs[i];
            if (child >= 0 && child < vm->count &&
                vm->objects[child].alive && !vm->objects[child].marked) {
                vm->objects[child].marked = 1;
                queue[tail++] = child;
            }
        }
    }
}

void demo6(void) {
    printf("\n=== Demo 6: BFS mark (iterative with queue) ===\n");
    VM vm;
    vm_init(&vm);

    int a = vm_alloc(&vm, "A");
    int b = vm_alloc(&vm, "B");
    int c = vm_alloc(&vm, "C");
    int d = vm_alloc(&vm, "D");
    int e = vm_alloc(&vm, "E");
    int f = vm_alloc(&vm, "F");  /* unreachable */

    vm_add_ref(&vm, a, b);
    vm_add_ref(&vm, a, c);
    vm_add_ref(&vm, b, d);
    vm_add_ref(&vm, c, e);
    vm_add_root(&vm, a);

    printf("Graph: A->{B,C}, B->D, C->E, F unreachable\n");
    printf("BFS traversal order:\n");

    gc_mark_bfs(&vm);
    int freed = gc_sweep(&vm);
    printf("  [gc] freed %d object(s)\n", freed);

    printf("After BFS GC:\n");
    vm_print(&vm);
}

/* ---- main ---- */
int main(void) {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -o mark_sweep mark_sweep.c
./mark_sweep
```

### Salida esperada (extracto)

```
=== Demo 1: Basic mark-and-sweep ===
Before GC:
  Objects:
    [0] A          alive  refs=[1]
    [1] B          alive  refs=[2]
    [2] C          alive  refs=[]
    [3] D          alive  refs=[]
  Roots: [0]
  [gc] === collection start ===
  [gc] sweep: freeing object 3 (D)
  [gc] === collection end: freed 1 object(s) ===
After GC:
  Objects:
    [0] A          alive  refs=[1]
    [1] B          alive  refs=[2]
    [2] C          alive  refs=[]
    [3] D          (freed)
  Roots: [0]

=== Demo 2: Cycles collected correctly ===
  ...
  [gc] sweep: freeing object 1 (E)
  [gc] sweep: freeing object 2 (F)
  [gc] sweep: freeing object 3 (G)
  ...
  (cycle freed — RC would leak this!)
```

---

## 8. Programa en Rust — GC conceptual

Rust no necesita garbage collector gracias al ownership system, pero
podemos implementar uno para entender los conceptos. Ademas, exploramos
como los patrones de Rust se relacionan con las ideas de tracing GC.

```rust
use std::collections::VecDeque;

/* ============================================================
 *  Mini Garbage Collector — Mark-and-Sweep in Rust
 * ============================================================ */

const MAX_REFS: usize = 4;

#[derive(Clone)]
struct GCObject {
    id:      usize,
    label:   &'static str,
    marked:  bool,
    alive:   bool,
    refs:    Vec<usize>,
}

struct VM {
    objects: Vec<GCObject>,
    roots:   Vec<usize>,
}

impl VM {
    fn new() -> Self {
        VM {
            objects: Vec::new(),
            roots:   Vec::new(),
        }
    }

    fn alloc(&mut self, label: &'static str) -> usize {
        let id = self.objects.len();
        self.objects.push(GCObject {
            id,
            label,
            marked: false,
            alive:  true,
            refs:   Vec::new(),
        });
        id
    }

    fn add_ref(&mut self, src: usize, dst: usize) {
        if self.objects[src].refs.len() < MAX_REFS {
            self.objects[src].refs.push(dst);
        }
    }

    fn add_root(&mut self, obj: usize) {
        self.roots.push(obj);
    }

    fn remove_root(&mut self, obj: usize) {
        self.roots.retain(|&r| r != obj);
    }

    /* ---- Mark: DFS with explicit stack ---- */
    fn mark_dfs(&mut self) {
        let mut stack: Vec<usize> = self.roots.clone();

        while let Some(idx) = stack.pop() {
            if idx >= self.objects.len() { continue; }
            if !self.objects[idx].alive || self.objects[idx].marked {
                continue;
            }
            self.objects[idx].marked = true;
            for &child in &self.objects[idx].refs.clone() {
                stack.push(child);
            }
        }
    }

    /* ---- Mark: BFS with queue ---- */
    fn mark_bfs(&mut self) -> Vec<String> {
        let mut order = Vec::new();
        let mut queue: VecDeque<usize> = VecDeque::new();

        for &r in &self.roots.clone() {
            if r < self.objects.len() && self.objects[r].alive
                && !self.objects[r].marked {
                self.objects[r].marked = true;
                queue.push_back(r);
            }
        }

        while let Some(idx) = queue.pop_front() {
            order.push(format!("{} ({})", self.objects[idx].label,
                               self.objects[idx].id));
            for &child in &self.objects[idx].refs.clone() {
                if child < self.objects.len() && self.objects[child].alive
                    && !self.objects[child].marked {
                    self.objects[child].marked = true;
                    queue.push_back(child);
                }
            }
        }
        order
    }

    /* ---- Sweep ---- */
    fn sweep(&mut self) -> Vec<String> {
        let mut freed = Vec::new();
        for obj in &mut self.objects {
            if !obj.alive { continue; }
            if obj.marked {
                obj.marked = false;
            } else {
                freed.push(format!("{} ({})", obj.label, obj.id));
                obj.alive = false;
            }
        }
        freed
    }

    /* ---- Full GC ---- */
    fn collect_dfs(&mut self) -> Vec<String> {
        self.mark_dfs();
        self.sweep()
    }

    /* ---- Print ---- */
    fn print_state(&self) {
        println!("  Objects:");
        for obj in &self.objects {
            if !obj.alive {
                println!("    [{}] {:10}  (freed)", obj.id, obj.label);
                continue;
            }
            let refs: Vec<String> = obj.refs.iter()
                .map(|r| r.to_string()).collect();
            println!("    [{}] {:10}  alive  refs=[{}]",
                     obj.id, obj.label, refs.join(","));
        }
        let roots: Vec<String> = self.roots.iter()
            .map(|r| r.to_string()).collect();
        println!("  Roots: [{}]", roots.join(","));
    }

    fn alive_count(&self) -> usize {
        self.objects.iter().filter(|o| o.alive).count()
    }

    fn total_count(&self) -> usize {
        self.objects.len()
    }
}

/* ============================================================
 *  Demo 1: Basic mark-and-sweep
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Basic mark-and-sweep ===");
    let mut vm = VM::new();

    let a = vm.alloc("A");
    let b = vm.alloc("B");
    let c = vm.alloc("C");
    let _d = vm.alloc("D"); // unreachable

    vm.add_ref(a, b);
    vm.add_ref(b, c);
    vm.add_root(a);

    println!("Before GC:");
    vm.print_state();

    let freed = vm.collect_dfs();
    println!("  Freed: {:?}", freed);

    println!("After GC:");
    vm.print_state();
}

/* ============================================================
 *  Demo 2: Unreachable cycle collected
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Unreachable cycle collected ===");
    let mut vm = VM::new();

    let root = vm.alloc("root");
    let e = vm.alloc("E");
    let f = vm.alloc("F");
    let g = vm.alloc("G");

    vm.add_root(root);

    // Cycle: E -> F -> G -> E (not reachable from root)
    vm.add_ref(e, f);
    vm.add_ref(f, g);
    vm.add_ref(g, e);

    println!("Cycle E->F->G->E exists but is unreachable.");
    let freed = vm.collect_dfs();
    println!("  Freed: {:?} (cycle collected!)", freed);
    vm.print_state();
}

/* ============================================================
 *  Demo 3: Reachable cycle survives
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Reachable cycle survives ===");
    let mut vm = VM::new();

    let a = vm.alloc("A");
    let b = vm.alloc("B");
    let c = vm.alloc("C");

    vm.add_ref(a, b);
    vm.add_ref(b, c);
    vm.add_ref(c, a); // cycle
    vm.add_root(a);

    let freed = vm.collect_dfs();
    println!("  Freed: {:?}", freed);
    println!("  Alive: {} (all 3 survive — cycle is reachable)",
             vm.alive_count());
}

/* ============================================================
 *  Demo 4: Tricolor simulation
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Tricolor simulation ===");

    #[derive(Clone, Copy, PartialEq)]
    enum Color { White, Gray, Black }

    impl std::fmt::Display for Color {
        fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
            match self {
                Color::White => write!(f, "white"),
                Color::Gray  => write!(f, "gray "),
                Color::Black => write!(f, "black"),
            }
        }
    }

    let names = ["A", "B", "C", "D", "E"];
    // Graph: A->{B,D}, B->C, E unreachable
    let edges: Vec<Vec<usize>> = vec![
        vec![1, 3],  // A -> B, D
        vec![2],     // B -> C
        vec![],      // C
        vec![],      // D
        vec![],      // E (unreachable)
    ];

    let mut colors = vec![Color::White; 5];
    let mut worklist: Vec<usize> = Vec::new();

    // Step 0
    println!("  Step 0 (initial):");
    for (i, name) in names.iter().enumerate() {
        println!("    {}: {}", name, colors[i]);
    }

    // Step 1: root A -> gray
    colors[0] = Color::Gray;
    worklist.push(0);
    println!("\n  Step 1 (A grayed from root):");
    for (i, name) in names.iter().enumerate() {
        println!("    {}: {}", name, colors[i]);
    }

    let mut step = 2;
    while let Some(idx) = worklist.pop() {
        colors[idx] = Color::Black;
        for &child in &edges[idx] {
            if colors[child] == Color::White {
                colors[child] = Color::Gray;
                worklist.push(child);
            }
        }
        println!("\n  Step {} (process {}):", step, names[idx]);
        for (i, name) in names.iter().enumerate() {
            println!("    {}: {}", name, colors[i]);
        }
        step += 1;
    }

    let garbage: Vec<&str> = names.iter().enumerate()
        .filter(|(i, _)| colors[*i] == Color::White)
        .map(|(_, n)| *n)
        .collect();
    println!("\n  Garbage (still white): {:?}", garbage);
}

/* ============================================================
 *  Demo 5: BFS traversal order
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: BFS mark traversal order ===");
    let mut vm = VM::new();

    let a = vm.alloc("A");
    let b = vm.alloc("B");
    let c = vm.alloc("C");
    let d = vm.alloc("D");
    let e = vm.alloc("E");
    let _f = vm.alloc("F"); // unreachable

    // A -> {B, C}, B -> D, C -> E
    vm.add_ref(a, b);
    vm.add_ref(a, c);
    vm.add_ref(b, d);
    vm.add_ref(c, e);
    vm.add_root(a);

    println!("  Graph: A->{{B,C}}, B->D, C->E, F unreachable");
    let order = vm.mark_bfs();
    println!("  BFS visit order: {:?}", order);

    let freed = vm.sweep();
    println!("  Freed: {:?}", freed);
}

/* ============================================================
 *  Demo 6: Multiple GC cycles — progressive collection
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Multiple GC cycles ===");
    let mut vm = VM::new();

    let a = vm.alloc("A");
    let b = vm.alloc("B");
    let c = vm.alloc("C");
    let d = vm.alloc("D");

    vm.add_ref(a, b);
    vm.add_ref(a, c);
    vm.add_ref(c, d);
    vm.add_root(a);

    println!("  Cycle 1: root=A, all reachable");
    let freed = vm.collect_dfs();
    println!("  Freed: {:?}  Alive: {}", freed, vm.alive_count());

    // Remove root
    vm.remove_root(a);
    println!("\n  Cycle 2: root removed, everything unreachable");
    let freed = vm.collect_dfs();
    println!("  Freed: {:?}  Alive: {}", freed, vm.alive_count());
}

/* ============================================================
 *  Demo 7: GC statistics — live ratio
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: GC statistics — live ratio ===");
    let mut vm = VM::new();

    // Create 10 objects, root only points to first 3
    let objs: Vec<usize> = (0..10)
        .map(|i| vm.alloc(match i {
            0 => "obj_0", 1 => "obj_1", 2 => "obj_2",
            3 => "obj_3", 4 => "obj_4", 5 => "obj_5",
            6 => "obj_6", 7 => "obj_7", 8 => "obj_8",
            _ => "obj_9",
        }))
        .collect();

    let root = vm.alloc("root");
    vm.add_root(root);
    vm.add_ref(root, objs[0]);
    vm.add_ref(root, objs[1]);
    vm.add_ref(root, objs[2]);

    let total_before = vm.total_count();
    let freed = vm.collect_dfs();
    let alive = vm.alive_count();

    println!("  Total objects:  {}", total_before);
    println!("  Freed:          {} ({:?})", freed.len(), freed);
    println!("  Alive:          {}", alive);
    println!("  Live ratio:     {:.0}%",
             alive as f64 / total_before as f64 * 100.0);
    println!("  Garbage ratio:  {:.0}%",
             freed.len() as f64 / total_before as f64 * 100.0);
}

/* ============================================================
 *  Demo 8: Ownership as compile-time GC — comparison
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: Ownership as compile-time GC ===");

    println!("  Rust's ownership system acts like a compile-time GC:");
    println!();

    // Box: single owner, freed on drop
    {
        let data = Box::new(vec![1, 2, 3, 4, 5]);
        println!("  Box allocated: {:?}", data);
        // data is automatically freed here — no GC needed
    }
    println!("  Box freed at end of scope (Drop = deterministic sweep)");

    // Rc: reference counting (runtime GC for shared ownership)
    {
        use std::rc::Rc;
        let shared = Rc::new("shared data");
        let _ref2 = Rc::clone(&shared);
        println!("  Rc strong_count: {} (RC = runtime tracing for sharing)",
                 Rc::strong_count(&shared));
    }

    // Comparison table
    println!();
    println!("  +-----------------------+-----------------------+");
    println!("  |  GC concept           |  Rust equivalent      |");
    println!("  +-----------------------+-----------------------+");
    println!("  |  Root set             |  Stack variables      |");
    println!("  |  Mark (reachability)  |  Ownership + borrows  |");
    println!("  |  Sweep (free dead)    |  Drop at scope end    |");
    println!("  |  Cycle detection      |  Borrow checker       |");
    println!("  |  Stop-the-world       |  (none — compile time)|");
    println!("  |  Runtime overhead     |  Zero                 |");
    println!("  +-----------------------+-----------------------+");
    println!();
    println!("  Key insight: Rust moves GC decisions to compile time.");
    println!("  The borrow checker is a 'static mark phase' that");
    println!("  proves reachability at compile time, so no runtime");
    println!("  tracing is needed.");
}

fn main() {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    demo7();
    demo8();
}
```

### Compilar y ejecutar

```bash
rustc -o mark_sweep_rs mark_sweep.rs
./mark_sweep_rs
```

### Salida esperada (extracto)

```
=== Demo 1: Basic mark-and-sweep ===
Before GC:
  Objects:
    [0] A          alive  refs=[1]
    [1] B          alive  refs=[2]
    [2] C          alive  refs=[]
    [3] D          alive  refs=[]
  Roots: [0]
  Freed: ["D (3)"]
After GC:
  Objects:
    [0] A          alive  refs=[1]
    [1] B          alive  refs=[2]
    [2] C          alive  refs=[]
    [3] D          (freed)
  Roots: [0]

=== Demo 2: Unreachable cycle collected ===
  Freed: ["E (1)", "F (2)", "G (3)"] (cycle collected!)

=== Demo 5: BFS mark traversal order ===
  BFS visit order: ["A (0)", "B (1)", "C (2)", "D (3)", "E (4)"]
  Freed: ["F (5)"]
```

---

## 9. Ejercicios

Estos ejercicios refuerzan los conceptos de mark-and-sweep. Para cada
uno, **predice** el resultado antes de ejecutar.

### Ejercicio 1: Diamante de referencias

Crea el grafo: root -> A -> C, root -> B -> C. C es alcanzable por
dos caminos. Ejecuta GC.

<details><summary>Prediccion</summary>

C tiene dos padres (A y B), ambos alcanzables desde root. El mark
visitara C al encontrarlo por el primer camino. El segundo camino
encontrara C ya marcado y no hara nada (ya visitado). Resultado:
ningun objeto es liberado — todos son alcanzables.

El mark bit evita recorrer C dos veces (sin el bit, un grafo con
muchos caminos podria causar marcado exponencial).

</details>

### Ejercicio 2: Isla de basura

Crea 5 objetos que forman un grafo completo (todos apuntan a todos),
pero ningun root apunta a ninguno de ellos. Ejecuta GC.

<details><summary>Prediccion</summary>

Los 5 objetos forman una "isla" completamente aislada. A pesar de
que cada objeto tiene multiples referencias entrantes, ninguno es
alcanzable desde las raices. Mark-and-sweep los libera a todos.

Con reference counting, esta isla **nunca** se liberaria (cada objeto
tiene rc >= 4). Esta es la ventaja principal de tracing GC.

</details>

### Ejercicio 3: Cadena larga

Crea una cadena de 20 objetos: root -> N0 -> N1 -> ... -> N19. Ejecuta
GC. Luego elimina el root y ejecuta GC de nuevo.

<details><summary>Prediccion</summary>

Primera coleccion: la fase mark recorre los 20 nodos (profundidad 20
con DFS). Todos estan marcados y sobreviven. Freed = 0.

Segunda coleccion (sin root): mark no visita ningun objeto. Sweep
libera los 20 objetos. La profundidad de la cadena no importa porque
ninguno es alcanzable.

Si se usa DFS recursivo, una cadena de 20 niveles no es problema.
Pero con cadenas de miles de objetos, la recursion podria desbordar
el stack — por eso colectores reales usan DFS con stack explicito.

</details>

### Ejercicio 4: Raices multiples

Crea: root1 -> A -> B, root2 -> C -> B. B es compartido. Elimina
root1 y ejecuta GC.

<details><summary>Prediccion</summary>

Antes de eliminar root1: A, B, C todos vivos. Despues de eliminar
root1: mark recorre desde root2 -> C -> B. A no es alcanzable
(solo root1 apuntaba a A). Resultado: A es liberado. B sobrevive
porque sigue alcanzable desde root2 a traves de C.

</details>

### Ejercicio 5: Costo del sweep

Si hay 1000 objetos totales pero solo 10 son basura, cuanto trabajo
hace la fase de sweep?

<details><summary>Prediccion</summary>

Sweep examina **todos** los 1000 objetos para verificar el bit de
marcado. Su costo es O(N) donde N es el numero **total** de objetos,
no solo la basura. Esto es una desventaja de mark-and-sweep basico
comparado con copying GC, donde el costo es O(L) donde L son los
objetos **vivos** (solo se copian los marcados).

En este caso, sweep hace 1000 verificaciones para liberar solo 10
objetos — 99% del trabajo es verificar objetos que sobreviven.

</details>

### Ejercicio 6: Mark sin sweep

Ejecuta solo la fase de mark (sin sweep). Que pasa con los objetos
no marcados?

<details><summary>Prediccion</summary>

Los objetos no marcados siguen existiendo — ocupan memoria pero no
son accesibles. Es exactamente la situacion de un **memory leak**.
La fase de sweep es la que realmente libera la memoria. Mark solo
identifica que esta vivo; sweep actua sobre esa informacion.

Si mark se ejecuta pero sweep no, ademas los bits de marcado quedan
activos. En el proximo ciclo de GC, mark podria confundir objetos
previamente marcados con objetos recien alcanzados (por eso sweep
resetea los bits).

</details>

### Ejercicio 7: Invariante tricolor violada

Imagina que durante el marcado (concurrente), el mutator crea una
referencia de un objeto negro a un objeto blanco, y elimina la unica
referencia gris al blanco. Que ocurre?

<details><summary>Prediccion</summary>

El objeto blanco nunca sera visitado: el unico camino gris fue
eliminado, y el objeto negro que ahora apunta a el no sera
re-visitado (negro = "procesado completamente"). El objeto blanco
sera tratado como basura y liberado, a pesar de ser alcanzable.

Esto es un **dangling pointer** — un error catastrofico. Por eso los
GC concurrentes necesitan **write barriers**: interceptar las
escrituras de punteros para mantener la invariante tricolor (ningun
negro apunta directamente a un blanco sin un gris intermedio).

</details>

### Ejercicio 8: Comparar DFS vs BFS

Dado el grafo: root -> A, A -> {B, C}, B -> {D, E}, C -> {F, G}.
Predice el orden de visita con DFS y con BFS.

<details><summary>Prediccion</summary>

**BFS** (cola): visita nivel por nivel.
Orden: A, B, C, D, E, F, G

**DFS** (stack): visita en profundidad primero.
Orden depende de la implementacion. Con stack y pusheo en orden
[B, C], se procesa C primero (LIFO):
Orden: A, C, G, F, B, E, D

Con recursion en orden [B, C], se procesa B primero:
Orden: A, B, D, E, C, F, G

El resultado del GC es identico (los mismos objetos son marcados),
pero la eficiencia puede diferir:
- BFS: mejor localidad si los objetos de un nivel estan contiguos.
- DFS: menos memoria si el grafo es ancho (stack depth = profundidad).

</details>

### Ejercicio 9: Costo de mark vs sweep

En un heap con 10,000 objetos totales, 8,000 vivos y 2,000 basura,
calcula el costo relativo de mark y sweep.

<details><summary>Prediccion</summary>

- **Mark**: visita los 8,000 objetos vivos (recorre el grafo de
  alcanzabilidad). Costo = O(8,000). Ademas por cada objeto visitado,
  examina sus referencias (si cada objeto tiene en promedio R
  referencias, el costo real es O(8,000 * R)).

- **Sweep**: examina los 10,000 objetos totales. Costo = O(10,000).
  Libera 2,000.

En este escenario con 80% de objetos vivos, sweep domina ligeramente.
Pero en un escenario tipico donde la mayoria de objetos son
efimeros (mueren pronto — ver T03 hipotesis generacional), mark
seria mucho mas barato (pocos vivos) pero sweep seguiria recorriendo
todo el heap. Por eso los colectores generacionales dividen el heap
para reducir el costo de sweep en la generacion joven.

</details>

### Ejercicio 10: Mark-sweep vs RC — cuando usar cada uno

Para cada escenario, decide si es mejor reference counting o
mark-and-sweep:

1. Sistema embebido con requisitos de latencia estrictos.
2. Aplicacion web con grafos de objetos con ciclos.
3. Sistema con miles de objetos efimeros creados por segundo.

<details><summary>Prediccion</summary>

1. **Reference counting**: La liberacion determinista (sin pausas
   stop-the-world) es critica para sistemas de tiempo real. RC libera
   inmediatamente al llegar a rc=0, lo cual da latencia predecible.
   Mark-and-sweep introduciria pausas impredecibles.

2. **Mark-and-sweep**: Los ciclos son un problema fundamental de RC.
   Si la aplicacion crea grafos con ciclos frecuentemente, RC causaria
   memory leaks a menos que se complemente con deteccion de ciclos
   (como hace Python). Mark-and-sweep recoge ciclos naturalmente.

3. **Mark-and-sweep (generacional)**: Objetos efimeros son el caso
   ideal para GC generacional (basado en mark-sweep). La generacion
   joven se colecta rapidamente (pocos sobreviven, mark es barato).
   Con RC, cada creacion/destruccion requiere incremento/decremento
   del contador — el overhead constante por operacion es significativo
   con miles de objetos por segundo.

Nota: en la practica, muchos sistemas combinan ambos (como Python:
RC + GC generacional de ciclos).

</details>
