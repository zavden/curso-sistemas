# T03 — Garbage collection generacional

## Objetivo

Comprender la **hipotesis generacional** y como los colectores modernos
la explotan para minimizar pausas: dividir el heap en **generaciones**,
colectar la generacion joven frecuentemente (barata) y la vieja rara
vez. Comparar stop-the-world vs concurrente, y ver como Java (G1/ZGC),
Go y Python implementan estas ideas.

---

## 1. La hipotesis generacional

### 1.1 Observacion empirica

En 1984, David Ungar observo un patron consistente en programas reales:

> **La mayoria de los objetos mueren jovenes.**

Esto se conoce como la **hipotesis generacional debil** (*weak
generational hypothesis*). Estudios posteriores confirmaron que
tipicamente el 80-95% de los objetos se vuelven basura poco despues
de ser creados.

```
Objetos
creados
  |
  |████████████
  |████████████
  |████████████░░░░
  |████████████░░░░░░
  |████████████░░░░░░░░░░░░░░░░░░░░░░
  +------+------+------+------+-------->  Edad
       joven              viejo

  ████ = objetos que mueren en esta edad
  ░░░░ = objetos que sobreviven
```

### 1.2 Ejemplos de objetos efimeros

| Patron | Ejemplo | Vida tipica |
|--------|---------|:-----------:|
| Variables temporales | `let temp = parse(input)` | Microsegundos |
| Iteradores | `.iter().map().filter()` | Microsegundos |
| Strings intermedios | `format!("{} {}", a, b)` | Milisegundos |
| Buffers de I/O | Leer linea, procesar, descartar | Milisegundos |
| Request/response | HTTP request object | Segundos |

### 1.3 Por que importa para el GC

Recordemos de T02 que mark-and-sweep tiene costo:

- Mark: O(L) donde L = objetos **vivos**.
- Sweep: O(N) donde N = objetos **totales**.

Si colectamos solo la generacion joven:
- L es muy pequeno (pocos jovenes sobreviven).
- N es pequeno (solo objetos jovenes, no todo el heap).

Resultado: colecciones jovenes son **ordenes de magnitud mas rapidas**
que colecciones completas del heap.

---

## 2. Estructura de un GC generacional

### 2.1 Generaciones

El heap se divide en regiones por edad:

```
+--------------------+------------------------------------+
|    Gen 0 (Young)   |           Gen 1 (Old)              |
|                    |                                    |
|  nursery / eden    |   tenured / old generation         |
|  + survivor spaces |                                    |
+--------------------+------------------------------------+
       pequeño                    grande
    colectado seguido          colectado rara vez
```

Algunos colectores usan mas generaciones (tipicamente 2-3):

```
Gen 0 (Young)  -->  Gen 1 (Middle)  -->  Gen 2 (Old)
   colectar          colectar              colectar
  cada ~ms          cada ~100ms           cada ~segundos
```

### 2.2 Promocion (tenuring)

Cuando un objeto sobrevive una coleccion de su generacion, se
**promueve** a la siguiente generacion:

```
Ciclo 1: objeto X creado en Gen 0
         GC minor -> X sobrevive -> promover X a Gen 1

Ciclo 2: X ahora vive en Gen 1
         GC minor -> no toca Gen 1
         GC major -> X examinado como parte de Gen 1
```

Criterios de promocion comunes:
- **Edad**: sobrevivir N colecciones (Java: `-XX:MaxTenuringThreshold`).
- **Tamaño**: objetos grandes se asignan directamente en Gen vieja.

### 2.3 Minor vs major collection

| Tipo | Que colecta | Frecuencia | Costo |
|------|-------------|:----------:|:-----:|
| **Minor GC** | Solo Gen 0 (young) | Frecuente | Bajo |
| **Major GC** | Todo el heap (young + old) | Infrecuente | Alto |
| **Full GC** | Heap + metaspace (Java) | Raro | Muy alto |

---

## 3. El problema de las referencias inter-generacionales

### 3.1 El desafio

Al colectar solo Gen 0, necesitamos saber si algun objeto en Gen 1
apunta a un objeto en Gen 0. Sin esa informacion, podriamos liberar
un objeto joven que esta referenciado desde la generacion vieja.

```
Gen 0 (young):        Gen 1 (old):
  +---+                 +---+
  | B | <-------------- | A |   A (viejo) apunta a B (joven)
  +---+                 +---+

Sin considerar A como raiz, minor GC liberaria B -> dangling pointer!
```

### 3.2 Remembered sets y write barriers

**Remembered set**: estructura que registra las referencias de Gen
vieja a Gen joven. Se usa como raices adicionales durante minor GC.

**Write barrier**: codigo que se ejecuta en cada escritura de puntero
para actualizar el remembered set:

```
// Pseudocodigo: write barrier
write_ref(obj, field, new_value):
    obj.field = new_value
    if generation(obj) > generation(new_value):
        remembered_set.add(obj)   // referencia inter-generacional
```

### 3.3 Card table

Optimizacion del remembered set. Divide el heap en **cards** (tipicamente
512 bytes). Un bit por card indica si contiene punteros a la generacion
joven:

```
Card table:  [0][0][1][0][0][1][0][0]...
              |     |           |
              v     v           v
             card  card        card
             limpia sucia      sucia
                   (tiene ref  (tiene ref
                    a joven)    a joven)
```

Durante minor GC, solo se escanean las cards marcadas como sucias.
Esto reduce drasticamente el costo de buscar referencias
inter-generacionales.

---

## 4. Stop-the-world vs concurrente

### 4.1 Stop-the-world (STW)

El enfoque mas simple: detener todos los threads del programa durante
la coleccion.

```
Thread 1: [  trabajo  ][ STOP ][    GC    ][ RESUME ][  trabajo  ]
Thread 2: [  trabajo  ][ STOP ][          ][ RESUME ][  trabajo  ]
Thread 3: [  trabajo  ][ STOP ][          ][ RESUME ][  trabajo  ]
                                |<-pausa->|
```

**Ventajas**: simple, correcto (no hay mutaciones durante GC).
**Desventajas**: pausa visible, inaceptable para aplicaciones
interactivas o de baja latencia.

### 4.2 Concurrent GC

El colector corre en paralelo con el programa:

```
Mutator:  [  trabajo  ][  trabajo  ][  trabajo  ][  trabajo  ]
GC:                [  mark  ][  sweep  ]
                   ^                   ^
                   |                   |
              write barriers mantienen consistencia
```

Requiere write barriers (ver seccion 3.2) y la abstraccion tricolor
(T02) para mantener la invariante: ningun negro apunta directamente
a un blanco.

### 4.3 Incremental GC

Compromiso entre STW y concurrente: hacer GC en pequenos pasos
intercalados con el programa.

```
Mutator:  [trabajo][GC][trabajo][GC][trabajo][GC][trabajo]
                   ^^^         ^^^         ^^^
              micro-pausas (~100us cada una)
```

### 4.4 Comparacion

| Aspecto | STW | Incremental | Concurrent |
|---------|:---:|:-----------:|:----------:|
| Pausa maxima | Alta | Media | Muy baja |
| Throughput | Mejor | Medio | Menor (overhead de barriers) |
| Complejidad | Baja | Media | Alta |
| Latencia | Impredecible | Acotada | Acotada |
| Uso de CPU | Solo durante GC | Intercalado | Thread(s) dedicado(s) |

---

## 5. GC generacional en lenguajes reales

### 5.1 Java — G1 y ZGC

**G1 (Garbage-First)** — colector por defecto desde Java 9:
- Divide el heap en **regiones** (1-32 MB).
- Cada region puede ser Eden, Survivor, Old, o Humongous.
- Minor GC: colecta regiones Eden + Survivor (STW, ~5-10ms).
- Mixed GC: colecta regiones joven + algunas old (STW).
- Objetivo: mantener pausas bajo un target configurable
  (`-XX:MaxGCPauseMillis=200`).

```
Heap G1:
+--+--+--+--+--+--+--+--+--+--+--+--+
|E |S |O |E |  |O |E |H  H |O |S |  |
+--+--+--+--+--+--+--+--+--+--+--+--+
 E=Eden  S=Survivor  O=Old  H=Humongous  (vacio)
```

**ZGC** — colector de baja latencia (Java 15+):
- Pausas tipicas: < 1 ms (incluso con heaps de TB).
- Casi completamente **concurrente**.
- Usa *colored pointers*: bits en el puntero para tracking.
- Compacta concurrentemente (sin STW para mover objetos).

### 5.2 Go — concurrent tricolor

Go no usa generaciones (por diseno). Su GC es:
- **Concurrent** tricolor mark-and-sweep.
- Pausas tipicas: < 0.5 ms.
- Sin compactacion (Go usa un allocator por tamano, similar a TCMalloc).
- Write barrier: **hybrid** (Dijkstra insertion + Yuasa deletion).

Por que Go no usa generaciones?

- Los objetos de Go se asignan en **stack** cuando el escape analysis
  determina que no escapan — los objetos efimeros nunca llegan al heap.
- El allocator por tamano (size classes) reduce fragmentacion.
- El GC concurrente ya es suficientemente rapido para los heaps tipicos.

### 5.3 Python — RC + generacional

Python combina **reference counting** con un GC **generacional** para
ciclos:

```python
import gc

# Tres generaciones: 0, 1, 2
# Thresholds: (700, 10, 10)
#   Gen 0 colectado cada 700 allocaciones
#   Gen 1 colectado cada 10 colecciones de Gen 0
#   Gen 2 colectado cada 10 colecciones de Gen 1

gc.get_threshold()  # (700, 10, 10)
gc.get_count()      # (87, 3, 1) — allocaciones pendientes por gen
```

| Mecanismo | Que libera | Cuando |
|-----------|-----------|--------|
| Reference counting | Objetos sin ciclos (rc=0) | Inmediato |
| Gen 0 collector | Ciclos entre objetos jovenes | Cada ~700 allocs |
| Gen 1 collector | Ciclos que sobreviven Gen 0 | Cada ~7000 allocs |
| Gen 2 collector | Ciclos longevos | Cada ~70000 allocs |

### 5.4 JavaScript (V8) — Orinoco

V8 (Chrome, Node.js) usa un GC generacional con dos generaciones:

- **Young generation** (Scavenger): semi-space copying collector.
  Dos espacios iguales (from-space, to-space). Copia vivos al
  to-space. Muy rapido para objetos efimeros.
- **Old generation** (Mark-Compact): mark-sweep con compactacion.
  Incremental + concurrent marking.

```
Young gen (semi-space):
+------------+------------+
| from-space | to-space   |
| (allocar)  | (copiar    |
|            |  vivos)    |
+------------+------------+

Despues de scavenge: swap from <-> to
```

### 5.5 Tabla comparativa

| Lenguaje | Generaciones | Minor GC | Major GC | Pausa tipica |
|----------|:------------:|----------|----------|:------------:|
| Java G1 | 2+ (regions) | STW copy | Mixed STW | 5-200 ms |
| Java ZGC | 1 (logica) | Concurrent | Concurrent | < 1 ms |
| Go | 0 | N/A | Concurrent M&S | < 0.5 ms |
| Python | 3 | STW tracing | STW tracing | < 10 ms |
| V8 | 2 | Scavenge (copy) | Mark-compact | 1-10 ms |
| .NET | 3 | STW copy | STW/concurrent | 1-50 ms |

---

## 6. Metricas de GC

### 6.1 Metricas clave

| Metrica | Definicion | Ideal |
|---------|-----------|:-----:|
| **Throughput** | % de tiempo ejecutando programa (no GC) | > 95% |
| **Latencia (pausa)** | Duracion de cada pausa de GC | < 10 ms |
| **Footprint** | Memoria total usada (heap + overhead GC) | Minimo |
| **Promptness** | Tiempo entre que un objeto muere y se libera | Bajo |

### 6.2 Tradeoffs

No se pueden optimizar todas las metricas simultaneamente:

```
        Throughput
           /\
          /  \
         /    \
        /  GC  \
       / design \
      / choices  \
     /____________\
 Latencia      Footprint
```

- **Mas throughput** = colectar menos seguido = pausas mas largas.
- **Menos latencia** = colectar incrementalmente = mas overhead = menos throughput.
- **Menos footprint** = colectar mas agresivamente = mas pausas.

---

## 7. Programa en C — Simulacion de GC generacional

Este programa simula un GC generacional con dos generaciones (young y
old), promocion por edad, y colecciones minor/major separadas.

```c
#include <stdio.h>
#include <string.h>

/* ============================================================
 *  Generational GC Simulator
 * ============================================================ */

#define YOUNG_CAPACITY   16
#define OLD_CAPACITY     32
#define MAX_REFS          4
#define TENURE_THRESHOLD  2  /* survive 2 minor GCs -> promote */

typedef struct GCObject {
    int          id;
    int          alive;
    int          marked;
    int          generation;     /* 0=young, 1=old */
    int          age;            /* minor GC survival count */
    int          refs[MAX_REFS];
    int          ref_count;
    const char  *label;
} GCObject;

typedef struct GCHeap {
    GCObject young[YOUNG_CAPACITY];
    int      young_count;

    GCObject old[OLD_CAPACITY];
    int      old_count;

    int      roots[MAX_REFS];
    int      root_count;

    int      next_id;

    /* Statistics */
    int      total_minor;
    int      total_major;
    int      total_promoted;
    int      total_freed_young;
    int      total_freed_old;
} GCHeap;

/* ---- Init ---- */

void heap_init(GCHeap *h) {
    memset(h, 0, sizeof(GCHeap));
    for (int i = 0; i < YOUNG_CAPACITY; i++)
        for (int j = 0; j < MAX_REFS; j++)
            h->young[i].refs[j] = -1;
    for (int i = 0; i < OLD_CAPACITY; i++)
        for (int j = 0; j < MAX_REFS; j++)
            h->old[i].refs[j] = -1;
    for (int i = 0; i < MAX_REFS; i++)
        h->roots[i] = -1;
}

/* ---- Allocate in young generation ---- */

int heap_alloc(GCHeap *h, const char *label) {
    if (h->young_count >= YOUNG_CAPACITY) {
        printf("    [heap] young gen full! trigger minor GC\n");
        return -1;
    }
    int idx = h->young_count++;
    GCObject *obj = &h->young[idx];
    obj->id         = h->next_id++;
    obj->alive      = 1;
    obj->marked     = 0;
    obj->generation = 0;
    obj->age        = 0;
    obj->ref_count  = 0;
    obj->label      = label;
    for (int j = 0; j < MAX_REFS; j++)
        obj->refs[j] = -1;
    return obj->id;
}

/* ---- Find object by id ---- */

GCObject *find_obj(GCHeap *h, int id) {
    for (int i = 0; i < h->young_count; i++)
        if (h->young[i].id == id && h->young[i].alive)
            return &h->young[i];
    for (int i = 0; i < h->old_count; i++)
        if (h->old[i].id == id && h->old[i].alive)
            return &h->old[i];
    return NULL;
}

/* ---- Add reference ---- */

void heap_add_ref(GCHeap *h, int src_id, int dst_id) {
    GCObject *src = find_obj(h, src_id);
    if (src && src->ref_count < MAX_REFS) {
        src->refs[src->ref_count++] = dst_id;
    }
}

/* ---- Add/remove root ---- */

void heap_add_root(GCHeap *h, int id) {
    if (h->root_count < MAX_REFS)
        h->roots[h->root_count++] = id;
}

void heap_remove_root(GCHeap *h, int id) {
    for (int i = 0; i < h->root_count; i++) {
        if (h->roots[i] == id) {
            h->roots[i] = h->roots[--h->root_count];
            h->roots[h->root_count] = -1;
            return;
        }
    }
}

/* ---- Mark (DFS from roots, optionally young-only) ---- */

void gc_mark_obj(GCHeap *h, int id, int young_only) {
    GCObject *obj = find_obj(h, id);
    if (!obj || obj->marked) return;
    if (young_only && obj->generation != 0) {
        /* old object: mark it but don't recurse further into old gen */
        obj->marked = 1;
        return;
    }
    obj->marked = 1;
    for (int i = 0; i < obj->ref_count; i++) {
        gc_mark_obj(h, obj->refs[i], young_only);
    }
}

/* ---- Promote object from young to old ---- */

int promote(GCHeap *h, GCObject *young_obj) {
    if (h->old_count >= OLD_CAPACITY) {
        printf("    [gc] old gen full! cannot promote\n");
        return 0;
    }
    GCObject *old_obj = &h->old[h->old_count++];
    *old_obj = *young_obj;
    old_obj->generation = 1;
    young_obj->alive = 0;  /* remove from young */
    h->total_promoted++;
    return 1;
}

/* ---- Minor GC: collect young generation ---- */

int gc_minor(GCHeap *h) {
    printf("  [gc] --- minor GC start ---\n");
    h->total_minor++;

    /* Mark from roots (young only) */
    for (int i = 0; i < h->root_count; i++) {
        if (h->roots[i] >= 0)
            gc_mark_obj(h, h->roots[i], 1);
    }

    /* Also mark from old->young references (remembered set simulation) */
    for (int i = 0; i < h->old_count; i++) {
        GCObject *old = &h->old[i];
        if (!old->alive) continue;
        for (int j = 0; j < old->ref_count; j++) {
            GCObject *target = find_obj(h, old->refs[j]);
            if (target && target->generation == 0)
                gc_mark_obj(h, old->refs[j], 1);
        }
    }

    /* Sweep/promote young gen */
    int freed = 0;
    int promoted = 0;
    for (int i = 0; i < h->young_count; i++) {
        GCObject *obj = &h->young[i];
        if (!obj->alive) continue;
        if (obj->marked) {
            obj->marked = 0;
            obj->age++;
            if (obj->age >= TENURE_THRESHOLD) {
                printf("    [gc] promote id=%d (%s) age=%d -> old gen\n",
                       obj->id, obj->label, obj->age);
                promote(h, obj);
                promoted++;
            }
        } else {
            printf("    [gc] free young id=%d (%s)\n",
                   obj->id, obj->label);
            obj->alive = 0;
            freed++;
            h->total_freed_young++;
        }
    }

    /* Reset marks on old gen */
    for (int i = 0; i < h->old_count; i++)
        h->old[i].marked = 0;

    printf("  [gc] --- minor GC end: freed=%d promoted=%d ---\n",
           freed, promoted);
    return freed;
}

/* ---- Major GC: collect entire heap ---- */

int gc_major(GCHeap *h) {
    printf("  [gc] === MAJOR GC start ===\n");
    h->total_major++;

    /* Mark from roots (all generations) */
    for (int i = 0; i < h->root_count; i++) {
        if (h->roots[i] >= 0)
            gc_mark_obj(h, h->roots[i], 0);
    }

    /* Sweep young */
    int freed = 0;
    for (int i = 0; i < h->young_count; i++) {
        GCObject *obj = &h->young[i];
        if (!obj->alive) continue;
        if (obj->marked) {
            obj->marked = 0;
        } else {
            printf("    [gc] free young id=%d (%s)\n",
                   obj->id, obj->label);
            obj->alive = 0;
            freed++;
            h->total_freed_young++;
        }
    }

    /* Sweep old */
    for (int i = 0; i < h->old_count; i++) {
        GCObject *obj = &h->old[i];
        if (!obj->alive) continue;
        if (obj->marked) {
            obj->marked = 0;
        } else {
            printf("    [gc] free old id=%d (%s)\n",
                   obj->id, obj->label);
            obj->alive = 0;
            freed++;
            h->total_freed_old++;
        }
    }

    printf("  [gc] === MAJOR GC end: freed=%d ===\n", freed);
    return freed;
}

/* ---- Print state ---- */

void heap_print(GCHeap *h) {
    printf("  Young gen (%d slots used):\n", h->young_count);
    for (int i = 0; i < h->young_count; i++) {
        GCObject *o = &h->young[i];
        if (!o->alive) {
            printf("    [id=%d] %-10s (freed)\n", o->id, o->label);
            continue;
        }
        printf("    [id=%d] %-10s age=%d refs=[", o->id, o->label, o->age);
        for (int j = 0; j < o->ref_count; j++) {
            if (j) printf(",");
            printf("%d", o->refs[j]);
        }
        printf("]\n");
    }
    printf("  Old gen (%d slots used):\n", h->old_count);
    for (int i = 0; i < h->old_count; i++) {
        GCObject *o = &h->old[i];
        if (!o->alive) {
            printf("    [id=%d] %-10s (freed)\n", o->id, o->label);
            continue;
        }
        printf("    [id=%d] %-10s age=%d refs=[", o->id, o->label, o->age);
        for (int j = 0; j < o->ref_count; j++) {
            if (j) printf(",");
            printf("%d", o->refs[j]);
        }
        printf("]\n");
    }
}

void heap_print_stats(GCHeap *h) {
    printf("  Stats: minor=%d major=%d promoted=%d "
           "freed_young=%d freed_old=%d\n",
           h->total_minor, h->total_major, h->total_promoted,
           h->total_freed_young, h->total_freed_old);
}

/* ============================================================
 *  Demo 1: Minor GC — ephemeral objects freed
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: Minor GC — ephemeral objects freed ===\n");
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    int a    = heap_alloc(&h, "A_long");
    int b    = heap_alloc(&h, "B_temp");
    int c    = heap_alloc(&h, "C_temp");

    heap_add_ref(&h, root, a);
    /* B and C are unreachable — ephemeral garbage */
    heap_add_root(&h, root);

    printf("Before minor GC (B_temp, C_temp unreachable):\n");
    heap_print(&h);

    gc_minor(&h);

    printf("After minor GC:\n");
    heap_print(&h);
    /* B_temp and C_temp freed — typical ephemeral pattern */

    (void)b; (void)c;
}

/* ============================================================
 *  Demo 2: Promotion — objects that survive become tenured
 * ============================================================ */
void demo2(void) {
    printf("\n=== Demo 2: Promotion after surviving %d minor GCs ===\n",
           TENURE_THRESHOLD);
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    int a    = heap_alloc(&h, "A_surv");
    heap_alloc(&h, "temp1");

    heap_add_ref(&h, root, a);
    heap_add_root(&h, root);

    printf("Minor GC #1:\n");
    gc_minor(&h);
    printf("  A_surv age after GC #1: %d\n",
           find_obj(&h, a)->age);

    heap_alloc(&h, "temp2");

    printf("\nMinor GC #2:\n");
    gc_minor(&h);
    /* A_surv should be promoted to old gen now (age >= TENURE_THRESHOLD) */

    printf("\nAfter promotion:\n");
    heap_print(&h);
}

/* ============================================================
 *  Demo 3: Old-to-young reference (remembered set)
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Old-to-young reference ===\n");
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    int a    = heap_alloc(&h, "A");
    heap_add_ref(&h, root, a);
    heap_add_root(&h, root);

    /* Promote root and A by running minor GCs */
    gc_minor(&h);  /* age 0->1 */
    gc_minor(&h);  /* age 1->2 -> promote */

    printf("\nRoot and A now in old gen.\n");

    /* Create young object B, referenced from old A */
    int b = heap_alloc(&h, "B_young");
    GCObject *a_obj = find_obj(&h, a);
    if (a_obj && a_obj->ref_count < MAX_REFS) {
        a_obj->refs[a_obj->ref_count++] = b;
    }

    printf("Old A -> young B (inter-generational ref)\n");
    heap_print(&h);

    printf("\nMinor GC (must scan old->young refs):\n");
    gc_minor(&h);

    printf("B_young should survive (reachable via old->young ref):\n");
    heap_print(&h);
}

/* ============================================================
 *  Demo 4: Major GC — collecting old generation
 * ============================================================ */
void demo4(void) {
    printf("\n=== Demo 4: Major GC — collecting old generation ===\n");
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    int a    = heap_alloc(&h, "A_prom");
    int b    = heap_alloc(&h, "B_prom");
    heap_add_ref(&h, root, a);
    heap_add_ref(&h, root, b);
    heap_add_root(&h, root);

    /* Promote all to old */
    gc_minor(&h);
    gc_minor(&h);

    printf("\nAll in old gen. Now remove ref to B_prom:\n");
    /* Simulate removing ref: find root, clear ref to b */
    GCObject *root_obj = find_obj(&h, root);
    for (int i = 0; i < root_obj->ref_count; i++) {
        if (root_obj->refs[i] == b) {
            root_obj->refs[i] = root_obj->refs[--root_obj->ref_count];
            break;
        }
    }

    printf("Minor GC won't free B_prom (it's in old gen):\n");
    gc_minor(&h);
    heap_print(&h);

    printf("\nMajor GC will free B_prom:\n");
    gc_major(&h);
    heap_print(&h);
}

/* ============================================================
 *  Demo 5: Generational hypothesis simulation
 *  Create many objects, most die young.
 * ============================================================ */
void demo5(void) {
    printf("\n=== Demo 5: Generational hypothesis simulation ===\n");
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    heap_add_root(&h, root);

    /* Long-lived object */
    int cache = heap_alloc(&h, "cache");
    heap_add_ref(&h, root, cache);

    /* Simulate workload: create ephemeral objects, collect, repeat */
    const char *temps[] = {"req1", "req2", "req3", "req4",
                           "req5", "req6", "req7", "req8"};

    for (int round = 0; round < 3; round++) {
        printf("\n  Round %d: allocate temporaries\n", round + 1);
        for (int i = 0; i < 4; i++) {
            heap_alloc(&h, temps[round * 3 + i < 8 ?
                                 round * 3 + i : 7]);
        }
        gc_minor(&h);
    }

    printf("\nFinal stats:\n");
    heap_print_stats(&h);
    printf("  cache object survived all rounds (promoted to old gen).\n");
}

/* ============================================================
 *  Demo 6: Minor vs Major cost comparison
 * ============================================================ */
void demo6(void) {
    printf("\n=== Demo 6: Minor vs Major cost comparison ===\n");
    GCHeap h;
    heap_init(&h);

    int root = heap_alloc(&h, "root");
    heap_add_root(&h, root);

    /* Create objects, promote some to old */
    int ids[6];
    const char *labels[] = {"L1", "L2", "L3", "L4", "L5", "L6"};
    for (int i = 0; i < 6; i++) {
        ids[i] = heap_alloc(&h, labels[i]);
        heap_add_ref(&h, root, ids[i]);
    }

    /* Promote via minor GCs */
    gc_minor(&h);
    gc_minor(&h);

    /* Now remove refs to L4, L5, L6 (in old gen) */
    GCObject *r = find_obj(&h, root);
    r->ref_count = 3; /* keep only L1, L2, L3 refs */

    /* Add new young objects */
    heap_alloc(&h, "Y1");
    heap_alloc(&h, "Y2");

    printf("State: L1-L3 rooted (old), L4-L6 garbage (old), "
           "Y1-Y2 garbage (young)\n");

    printf("\nMinor GC (only frees young garbage):\n");
    gc_minor(&h);
    heap_print_stats(&h);

    printf("\nMajor GC (also frees old garbage):\n");
    gc_major(&h);
    heap_print_stats(&h);
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
gcc -std=c11 -Wall -Wextra -o gen_gc gen_gc.c
./gen_gc
```

### Salida esperada (extracto)

```
=== Demo 1: Minor GC — ephemeral objects freed ===
Before minor GC (B_temp, C_temp unreachable):
  Young gen (4 slots used):
    [id=0] root       age=0 refs=[1]
    [id=1] A_long     age=0 refs=[]
    [id=2] B_temp     age=0 refs=[]
    [id=3] C_temp     age=0 refs=[]
  Old gen (0 slots used):
  [gc] --- minor GC start ---
    [gc] free young id=2 (B_temp)
    [gc] free young id=3 (C_temp)
  [gc] --- minor GC end: freed=2 promoted=0 ---

=== Demo 2: Promotion after surviving 2 minor GCs ===
  ...
    [gc] promote id=1 (A_surv) age=2 -> old gen
  ...

=== Demo 3: Old-to-young reference ===
  ...
  B_young should survive (reachable via old->young ref)
  ...
```

---

## 8. Programa en Rust — GC generacional conceptual

```rust
use std::collections::VecDeque;

/* ============================================================
 *  Generational GC Simulator in Rust
 * ============================================================ */

const MAX_REFS: usize = 4;
const TENURE_THRESHOLD: u32 = 2;

#[derive(Clone)]
struct GCObject {
    id:         usize,
    label:      &'static str,
    alive:      bool,
    marked:     bool,
    generation: u8,      // 0=young, 1=old
    age:        u32,     // minor GC survival count
    refs:       Vec<usize>, // ids of referenced objects
}

struct GCHeap {
    young: Vec<GCObject>,
    old:   Vec<GCObject>,
    roots: Vec<usize>,   // root ids
    next_id: usize,

    // Statistics
    minor_count:  u32,
    major_count:  u32,
    promoted:     u32,
    freed_young:  u32,
    freed_old:    u32,
}

impl GCHeap {
    fn new() -> Self {
        GCHeap {
            young: Vec::new(),
            old:   Vec::new(),
            roots: Vec::new(),
            next_id: 0,
            minor_count: 0, major_count: 0,
            promoted: 0, freed_young: 0, freed_old: 0,
        }
    }

    fn alloc(&mut self, label: &'static str) -> usize {
        let id = self.next_id;
        self.next_id += 1;
        self.young.push(GCObject {
            id, label, alive: true, marked: false,
            generation: 0, age: 0, refs: Vec::new(),
        });
        id
    }

    fn find_mut(&mut self, id: usize) -> Option<&mut GCObject> {
        self.young.iter_mut()
            .chain(self.old.iter_mut())
            .find(|o| o.id == id && o.alive)
    }

    fn find(&self, id: usize) -> Option<&GCObject> {
        self.young.iter()
            .chain(self.old.iter())
            .find(|o| o.id == id && o.alive)
    }

    fn add_ref(&mut self, src: usize, dst: usize) {
        if let Some(obj) = self.find_mut(src) {
            if obj.refs.len() < MAX_REFS {
                obj.refs.push(dst);
            }
        }
    }

    fn add_root(&mut self, id: usize) {
        self.roots.push(id);
    }

    fn remove_root(&mut self, id: usize) {
        self.roots.retain(|&r| r != id);
    }

    /* ---- Mark using BFS ---- */
    fn mark_from(&mut self, start_ids: &[usize], young_only: bool) {
        let mut queue: VecDeque<usize> = start_ids.iter().copied().collect();

        while let Some(id) = queue.pop_front() {
            // Find in young
            let mut found = false;
            for obj in &mut self.young {
                if obj.id == id && obj.alive && !obj.marked {
                    obj.marked = true;
                    found = true;
                    break;
                }
            }
            if !found {
                for obj in &mut self.old {
                    if obj.id == id && obj.alive && !obj.marked {
                        obj.marked = true;
                        if young_only { continue; }
                        found = true;
                        break;
                    }
                }
            }

            // Collect children
            let children: Vec<usize> = self.young.iter()
                .chain(self.old.iter())
                .find(|o| o.id == id && o.alive)
                .map(|o| o.refs.clone())
                .unwrap_or_default();

            for child in children {
                let already_marked = self.young.iter()
                    .chain(self.old.iter())
                    .any(|o| o.id == child && o.alive && o.marked);
                if !already_marked {
                    queue.push_back(child);
                }
            }
        }
    }

    /* ---- Minor GC ---- */
    fn minor_gc(&mut self) -> (u32, u32) {
        println!("  [gc] --- minor GC #{} ---", self.minor_count + 1);
        self.minor_count += 1;

        // Mark from roots
        let roots = self.roots.clone();
        self.mark_from(&roots, true);

        // Also mark from old->young references
        let old_refs: Vec<usize> = self.old.iter()
            .filter(|o| o.alive)
            .flat_map(|o| o.refs.clone())
            .collect();
        self.mark_from(&old_refs, true);

        // Sweep/promote young
        let mut freed = 0u32;
        let mut promoted_count = 0u32;
        let mut to_promote = Vec::new();

        for obj in &mut self.young {
            if !obj.alive { continue; }
            if obj.marked {
                obj.marked = false;
                obj.age += 1;
                if obj.age >= TENURE_THRESHOLD {
                    println!("    promote id={} ({}) age={}",
                             obj.id, obj.label, obj.age);
                    to_promote.push(obj.clone());
                    obj.alive = false;
                    promoted_count += 1;
                }
            } else {
                println!("    free young id={} ({})", obj.id, obj.label);
                obj.alive = false;
                freed += 1;
            }
        }

        // Move promoted to old
        for mut obj in to_promote {
            obj.generation = 1;
            self.old.push(obj);
        }

        // Reset old marks
        for obj in &mut self.old {
            obj.marked = false;
        }

        self.freed_young += freed;
        self.promoted += promoted_count;
        println!("  [gc] freed={} promoted={}", freed, promoted_count);
        (freed, promoted_count)
    }

    /* ---- Major GC ---- */
    fn major_gc(&mut self) -> u32 {
        println!("  [gc] === MAJOR GC #{} ===", self.major_count + 1);
        self.major_count += 1;

        let roots = self.roots.clone();
        self.mark_from(&roots, false);

        let mut freed = 0u32;
        for obj in &mut self.young {
            if !obj.alive { continue; }
            if obj.marked { obj.marked = false; }
            else {
                println!("    free young id={} ({})", obj.id, obj.label);
                obj.alive = false;
                freed += 1;
                self.freed_young += 1;
            }
        }
        for obj in &mut self.old {
            if !obj.alive { continue; }
            if obj.marked { obj.marked = false; }
            else {
                println!("    free old id={} ({})", obj.id, obj.label);
                obj.alive = false;
                freed += 1;
                self.freed_old += 1;
            }
        }
        println!("  [gc] major freed={}", freed);
        freed
    }

    fn print_state(&self) {
        println!("  Young:");
        for obj in &self.young {
            if !obj.alive {
                println!("    [{}] {:10} (freed)", obj.id, obj.label);
            } else {
                let refs: Vec<String> = obj.refs.iter()
                    .map(|r| r.to_string()).collect();
                println!("    [{}] {:10} age={} refs=[{}]",
                         obj.id, obj.label, obj.age, refs.join(","));
            }
        }
        println!("  Old:");
        for obj in &self.old {
            if !obj.alive {
                println!("    [{}] {:10} (freed)", obj.id, obj.label);
            } else {
                let refs: Vec<String> = obj.refs.iter()
                    .map(|r| r.to_string()).collect();
                println!("    [{}] {:10} age={} refs=[{}]",
                         obj.id, obj.label, obj.age, refs.join(","));
            }
        }
    }

    fn print_stats(&self) {
        println!("  Stats: minor={} major={} promoted={} \
                  freed_young={} freed_old={}",
                 self.minor_count, self.major_count, self.promoted,
                 self.freed_young, self.freed_old);
    }

    fn alive_count(&self) -> usize {
        self.young.iter().filter(|o| o.alive).count()
            + self.old.iter().filter(|o| o.alive).count()
    }

    fn remove_ref(&mut self, src: usize, dst: usize) {
        if let Some(obj) = self.find_mut(src) {
            obj.refs.retain(|&r| r != dst);
        }
    }
}

/* ============================================================
 *  Demo 1: Ephemeral objects freed by minor GC
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Ephemeral objects freed ===");
    let mut h = GCHeap::new();

    let root = h.alloc("root");
    let a    = h.alloc("A_long");
    let _b   = h.alloc("B_temp");
    let _c   = h.alloc("C_temp");

    h.add_ref(root, a);
    h.add_root(root);

    println!("B_temp, C_temp unreachable:");
    h.minor_gc();
    h.print_state();
}

/* ============================================================
 *  Demo 2: Promotion to old generation
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Promotion (threshold={}) ===",
             TENURE_THRESHOLD);
    let mut h = GCHeap::new();

    let root = h.alloc("root");
    let a    = h.alloc("A_surv");
    h.alloc("temp1");

    h.add_ref(root, a);
    h.add_root(root);

    h.minor_gc(); // age 0->1
    if let Some(obj) = h.find(a) {
        println!("  A_surv age: {}", obj.age);
    }

    h.alloc("temp2");
    h.minor_gc(); // age 1->2 -> promote
    h.print_state();
}

/* ============================================================
 *  Demo 3: Old-to-young reference survives minor GC
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Old-to-young reference ===");
    let mut h = GCHeap::new();

    let root = h.alloc("root");
    let a    = h.alloc("A");
    h.add_ref(root, a);
    h.add_root(root);

    h.minor_gc(); // age 1
    h.minor_gc(); // promote root+A to old

    // New young object referenced from old A
    let b = h.alloc("B_young");
    h.add_ref(a, b);

    println!("  Old A -> young B:");
    h.minor_gc();
    println!("  B_young alive: {}",
             h.find(b).map(|o| o.alive).unwrap_or(false));
}

/* ============================================================
 *  Demo 4: Major GC collects old garbage
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Major GC collects old garbage ===");
    let mut h = GCHeap::new();

    let root = h.alloc("root");
    let a    = h.alloc("A_prom");
    let b    = h.alloc("B_prom");
    h.add_ref(root, a);
    h.add_ref(root, b);
    h.add_root(root);

    h.minor_gc();
    h.minor_gc(); // promote all

    // Remove ref to B
    h.remove_ref(root, b);

    println!("  Minor GC won't free B (in old gen):");
    h.minor_gc();
    println!("  B alive after minor: {}",
             h.find(b).map(|o| o.alive).unwrap_or(false));

    println!("  Major GC frees B:");
    h.major_gc();
    println!("  B alive after major: {}",
             h.find(b).map(|o| o.alive).unwrap_or(false));
}

/* ============================================================
 *  Demo 5: Generational hypothesis — workload simulation
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: Generational hypothesis simulation ===");
    let mut h = GCHeap::new();

    let root  = h.alloc("root");
    let cache = h.alloc("cache");
    h.add_ref(root, cache);
    h.add_root(root);

    let temp_labels: Vec<&str> = vec![
        "req_a", "req_b", "req_c", "req_d",
        "req_e", "req_f", "req_g", "req_h",
        "req_i", "req_j", "req_k", "req_l",
    ];

    for round in 0..4 {
        println!("\n  Round {} — allocate 3 temporaries:", round + 1);
        for i in 0..3 {
            let idx = round * 3 + i;
            if idx < temp_labels.len() {
                h.alloc(temp_labels[idx]);
            }
        }
        h.minor_gc();
    }

    println!("\n  cache promoted and survives all rounds.");
    h.print_stats();
    println!("  Alive: {}", h.alive_count());
}

/* ============================================================
 *  Demo 6: Tricolor with generations
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Tricolor states across generations ===");

    #[derive(Clone, Copy)]
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

    // Simulate: root(old)->A(old)->B(young), C(young) unreachable
    let names = ["root", "A", "B", "C"];
    let gens  = ["old", "old", "young", "young"];
    // root->A, A->B
    let edges: Vec<Vec<usize>> = vec![
        vec![1],    // root -> A
        vec![2],    // A -> B
        vec![],     // B
        vec![],     // C (unreachable)
    ];

    println!("  Objects: root(old)->A(old)->B(young), C(young)");
    println!("  Minor GC: start from roots + old->young refs\n");

    let mut colors = vec![Color::White; 4];
    let mut worklist: Vec<usize> = Vec::new();

    // Root is in old gen — treated as root for minor GC
    colors[0] = Color::Gray;
    worklist.push(0);

    let mut step = 0;
    println!("  Step {}: root grayed", step);
    for (i, name) in names.iter().enumerate() {
        println!("    {} ({}): {}", name, gens[i], colors[i]);
    }

    while let Some(idx) = worklist.pop() {
        step += 1;
        colors[idx] = Color::Black;
        for &child in &edges[idx] {
            if let Color::White = colors[child] {
                colors[child] = Color::Gray;
                worklist.push(child);
            }
        }
        println!("\n  Step {}: process {}", step, names[idx]);
        for (i, name) in names.iter().enumerate() {
            println!("    {} ({}): {}", name, gens[i], colors[i]);
        }
    }

    println!("\n  Result: C (young, white) is garbage.");
    println!("  Old objects (root, A) were marked but not swept in minor GC.");
}

/* ============================================================
 *  Demo 7: GC metrics — throughput calculation
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: GC metrics simulation ===");

    struct GCRun {
        name: &'static str,
        work_us: u64,
        gc_us:   u64,
    }

    let runs = vec![
        GCRun { name: "STW (full heap)",
                work_us: 10000, gc_us: 500 },
        GCRun { name: "Generational (minor only)",
                work_us: 10000, gc_us: 50 },
        GCRun { name: "Concurrent (Go-like)",
                work_us: 10000, gc_us: 10 },
    ];

    println!("  {:<30} {:>8} {:>8} {:>10}",
             "Strategy", "Work(us)", "GC(us)", "Throughput");
    println!("  {}", "-".repeat(60));

    for run in &runs {
        let total = run.work_us + run.gc_us;
        let throughput = run.work_us as f64 / total as f64 * 100.0;
        println!("  {:<30} {:>8} {:>8} {:>9.1}%",
                 run.name, run.work_us, run.gc_us, throughput);
    }

    println!();
    println!("  Key insight: generational minor GC has ~10x less");
    println!("  overhead than full-heap STW collection.");
}

/* ============================================================
 *  Demo 8: Language comparison — GC strategies
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: Language GC comparison ===");

    println!("  +------------+------------------+-----------+----------+");
    println!("  | Language   | Strategy         | Gens      | Pause    |");
    println!("  +------------+------------------+-----------+----------+");
    println!("  | Java G1    | Gen mark-compact | 2+ (rgns) | 5-200ms  |");
    println!("  | Java ZGC   | Concurrent       | logical 1 | < 1ms    |");
    println!("  | Go         | Conc tri-color   | 0 (none)  | < 0.5ms  |");
    println!("  | Python     | RC + gen tracing | 3         | < 10ms   |");
    println!("  | V8 (JS)    | Gen copy+compact | 2         | 1-10ms   |");
    println!("  | .NET       | Gen mark-compact | 3         | 1-50ms   |");
    println!("  | Rust       | (none — compile) | N/A       | 0ms      |");
    println!("  +------------+------------------+-----------+----------+");
    println!();
    println!("  Rust achieves zero GC pause because ownership moves all");
    println!("  reachability analysis to compile time. Objects are freed");
    println!("  deterministically via Drop — no runtime tracing needed.");
    println!();

    // Show Rust's compile-time "GC" in action
    {
        let data = vec![1, 2, 3, 4, 5];
        println!("  Vec allocated: {:?}", data);
        // data dropped here — deterministic, zero overhead
    }
    println!("  Vec freed at scope end — Drop = deterministic sweep");

    {
        use std::rc::Rc;
        let a = Rc::new(42);
        let b = Rc::clone(&a);
        println!("  Rc strong_count={} (runtime RC for shared ownership)",
                 Rc::strong_count(&a));
        drop(b);
        println!("  After drop(b): strong_count={}", Rc::strong_count(&a));
    }
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
rustc -o gen_gc_rs gen_gc.rs
./gen_gc_rs
```

### Salida esperada (extracto)

```
=== Demo 1: Ephemeral objects freed ===
  [gc] --- minor GC #1 ---
    free young id=2 (B_temp)
    free young id=3 (C_temp)
  [gc] freed=2 promoted=0

=== Demo 2: Promotion (threshold=2) ===
  [gc] --- minor GC #1 ---
    free young id=2 (temp1)
  [gc] freed=1 promoted=0
  A_surv age: 1
  [gc] --- minor GC #2 ---
    promote id=0 (root) age=2
    promote id=1 (A_surv) age=2
    free young id=3 (temp2)
  [gc] freed=1 promoted=2

=== Demo 5: Generational hypothesis simulation ===
  ...
  Stats: minor=4 major=0 promoted=2 freed_young=12 freed_old=0
  (12 ephemeral objects freed, 2 long-lived promoted — hypothesis confirmed)
```

---

## 9. Ejercicios

### Ejercicio 1: Tasa de supervivencia

Si en cada minor GC se crean 100 objetos y solo 5 sobreviven, cual
es la tasa de mortalidad? Despues de 10 minor GCs, cuantos objetos
se han promovido (threshold=2)?

<details><summary>Prediccion</summary>

Tasa de mortalidad: 95/100 = **95%** (consistente con la hipotesis
generacional).

Los 5 supervivientes de cada ronda incrementan su edad. Despues de
2 minor GCs, los que sobrevivieron ambas se promueven. Si los mismos
5 sobreviven consistentemente: tras GC #2, se promueven 5. Tras GC #4,
otros 5 (los de ronda 3-4 que sobrevivieron 2 veces). En total tras
10 minor GCs: hasta **25 objetos promovidos** (5 por cada par de GCs),
asumiendo los mismos 5 sobreviven cada ronda.

En la practica, la mayoria de los supervivientes tambien mueren
eventualmente, asi que el numero real de promovidos seria menor.

</details>

### Ejercicio 2: Card table con 4 GB de old gen

Si la old generation ocupa 4 GB y las cards son de 512 bytes, cuanto
memoria ocupa la card table?

<details><summary>Prediccion</summary>

Numero de cards: 4 GB / 512 bytes = 4 * 1024^3 / 512 = 8,388,608 cards.

Con 1 bit por card: 8,388,608 / 8 = **1,048,576 bytes = 1 MB**.

Con 1 byte por card (mas comun, permite estados adicionales):
8,388,608 bytes = **8 MB**.

En ambos casos, el overhead es minimo comparado con 4 GB de heap
(0.025% con bits, 0.2% con bytes). Este es un tradeoff excelente:
8 MB de card table evita escanear 4 GB de old gen en cada minor GC.

</details>

### Ejercicio 3: Old-to-young sin remembered set

Que sucede si un objeto viejo apunta a uno joven y el minor GC no
tiene remembered set?

<details><summary>Prediccion</summary>

El minor GC solo recorre raices del stack y young gen. No sabe que
un objeto viejo apunta al joven. El objeto joven aparece como
inalcanzable y es **liberado erróneamente**. El puntero desde el
objeto viejo se convierte en un **dangling pointer**.

Es un error catastrofico equivalente a use-after-free. Por eso el
remembered set (o card table) es **obligatorio** para GC generacional
correcto. El write barrier lo mantiene actualizado.

</details>

### Ejercicio 4: Promotion prematura

Si el threshold de promocion es 1 (promover tras sobrevivir un solo
minor GC), que problema puede ocurrir?

<details><summary>Prediccion</summary>

Muchos objetos que vivirian solo un poco mas se promueven
prematuramente a la old generation. Esto causa:

1. **Old gen se llena rapido**: mas major GCs necesarios (costosos).
2. **"Premature tenuring"**: objetos que moriran pronto contaminan
   la old gen, forzando colecciones major mas frecuentes.
3. **Peor throughput**: el beneficio del GC generacional se reduce
   porque la old gen acumula basura que un minor GC habria limpiado.

El threshold optimo depende de la aplicacion. Java usa por defecto
`MaxTenuringThreshold=15`, pero el colector lo ajusta dinamicamente.

</details>

### Ejercicio 5: Semi-space copying

En un copying collector (young gen de V8), el from-space y to-space
son de 1 MB cada uno. Si hay 100 KB de objetos vivos, cual es el
costo de coleccion y cuanta memoria se "desperdicia"?

<details><summary>Prediccion</summary>

**Costo**: O(100 KB) — solo se copian los objetos vivos al to-space.
No hay fase de sweep (el from-space entero se descarta). Esto es
mucho mas rapido que mark-and-sweep para young gen con alta mortalidad.

**Memoria desperdiciada**: 1 MB (el to-space esta vacio durante la
asignacion, reservado para la copia). Total disponible para asignar:
solo 1 MB de los 2 MB totales. **50% de overhead de memoria** es el
precio del copying collector.

Tradeoff: velocidad (O(vivos) en vez de O(total)) a cambio de
duplicar la memoria necesaria. Para young gen pequena, vale la pena.

</details>

### Ejercicio 6: Write barrier overhead

Si un programa ejecuta 10 millones de escrituras de punteros por
segundo y cada write barrier toma 5 ns, cual es el overhead total?

<details><summary>Prediccion</summary>

Overhead = 10,000,000 * 5 ns = 50,000,000 ns = **50 ms por segundo**.

Eso es un overhead de **5%** (50 ms de cada 1000 ms). Para un GC
concurrent, este es un overhead aceptable a cambio de pausas de < 1 ms.

En contraste, un STW GC con pausas de 200 ms tendria peor latencia
maxima, aunque su throughput total podria ser ligeramente mejor
(sin overhead constante de barriers).

Optimizacion comun: **conditional write barrier** — solo ejecutar el
barrier cuando la referencia cruza generaciones, reduciendo el numero
efectivo de barriers.

</details>

### Ejercicio 7: Go sin generaciones

Go no usa GC generacional. Por que funciona bien a pesar de esto?

<details><summary>Prediccion</summary>

Tres razones principales:

1. **Escape analysis**: el compilador de Go determina que objetos no
   escapan de su funcion y los asigna en el **stack**, no en el heap.
   Los objetos efimeros nunca llegan al heap — el "minor GC" es
   simplemente retornar de la funcion.

2. **Allocator por tamano (size classes)**: similar a TCMalloc, reduce
   fragmentacion sin necesidad de compactacion. Cada tamano tiene su
   propia arena.

3. **GC concurrente eficiente**: el tricolor mark-and-sweep con write
   barriers logra pausas < 0.5 ms incluso sin generaciones, porque
   el marking corre en paralelo con el programa.

En resumen: Go evita la necesidad de generaciones eliminando la
mayoria de objetos efimeros antes de que lleguen al heap (via escape
analysis) y usando un GC concurrente que es rapido incluso para heaps
completos.

</details>

### Ejercicio 8: Python gc.disable()

En Python, si llamas `gc.disable()`, que tipo de objetos causaran
memory leaks?

<details><summary>Prediccion</summary>

`gc.disable()` desactiva el **colector de ciclos**, no el reference
counting. El RC sigue activo.

- Objetos **sin ciclos**: liberados normalmente via RC (rc llega a 0).
- Objetos **con ciclos**: **memory leak permanente**. Sin el colector
  de ciclos, los objetos en un ciclo nunca llegan a rc=0 y nunca son
  liberados.

Ejemplo:
```python
gc.disable()
a = {}
b = {}
a['ref'] = b
b['ref'] = a
del a, b  # rc=1 en ambos (ciclo) -> LEAK
```

`gc.disable()` se usa a veces en aplicaciones de alta frecuencia
(HFT, gaming) que evitan crear ciclos y quieren eliminar las pausas
del colector. Pero requiere disciplina estricta para evitar ciclos.

</details>

### Ejercicio 9: ZGC colored pointers

ZGC usa bits en el puntero (colored pointers) para tracking. Si un
puntero de 64 bits usa 4 bits para color, cuantos bits quedan para
la direccion y cual es el heap maximo?

<details><summary>Prediccion</summary>

En x86-64, solo se usan 48 bits para direcciones virtuales (los 16
superiores son signo-extendidos). ZGC usa 4 bits de esos para metadata:

Bits para direccion: 48 - 4 = **44 bits**.
Heap maximo: 2^44 = **16 TB**.

Esto es suficiente para la mayoria de aplicaciones. Los 4 bits de
color codifican:
- **Marked0, Marked1**: dos bits alternantes para marcado (evitan
  resetear bits entre ciclos).
- **Remapped**: indica si el puntero ya fue actualizado tras compactacion.
- **Finalizable**: objeto pendiente de finalizacion.

La ventaja: el GC puede determinar el estado de un objeto simplemente
inspeccionando el puntero, sin acceder al header del objeto — excelente
para localidad de cache.

</details>

### Ejercicio 10: Diseno de GC para un caso especifico

Tienes un servidor web que: (a) crea 50,000 objetos por request,
(b) cada request dura ~10 ms, (c) los objetos del request mueren al
terminar. Que estrategia de GC/memoria es optima?

<details><summary>Prediccion</summary>

Este patron (objetos con lifetime atado a un request) es ideal para
un **arena allocator** (T01 de S02), no un GC generacional:

1. **Arena por request**: asignar todos los objetos del request en una
   arena. Al terminar el request, liberar la arena completa en O(1).
   Sin GC, sin marcado, sin sweep.

2. Si se necesita GC: un **generacional con nursery grande** funcionaria
   bien. Los 50,000 objetos nacen en el nursery y mueren jovenes
   (tasa de mortalidad ~100%). Minor GC seria muy rapido (casi nada
   que copiar/promover).

3. **Go approach**: escape analysis asignaria muchos de estos objetos
   en el stack de la goroutine que maneja el request. Al retornar, se
   liberan automaticamente.

4. **Rust approach**: los objetos vivirian en el scope del handler.
   Al terminar la funcion, Drop los libera. Cero overhead runtime.

El arena allocator es la mejor opcion por su simplicidad y costo O(1)
de liberacion. Esto conecta S02 (arenas) con S03 (GC): a veces la
mejor solucion es **no usar GC**.

</details>
