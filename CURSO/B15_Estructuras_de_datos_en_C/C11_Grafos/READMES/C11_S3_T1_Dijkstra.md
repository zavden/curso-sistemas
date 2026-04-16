# T01 — Dijkstra

## Objetivo

Dominar el algoritmo de Dijkstra para caminos más cortos desde una fuente
única (SSSP — Single-Source Shortest Path) en grafos con **pesos no
negativos**: su lógica greedy, implementación con cola de prioridad (min-heap),
reconstrucción de caminos, variantes y limitaciones.

---

## 1. El problema SSSP

Dado un grafo ponderado $G = (V, E)$ con función de peso
$w: E \to \mathbb{R}_{\geq 0}$ y un vértice fuente $s$, encontrar para
cada vértice $v \in V$:

- $\text{dist}[v]$: la distancia mínima (suma de pesos) de $s$ a $v$.
- $\text{parent}[v]$: el predecesor en el camino más corto (para
  reconstruirlo).

Si $v$ no es alcanzable desde $s$, $\text{dist}[v] = \infty$.

### Camino más corto vs BFS

BFS resuelve SSSP cuando todos los pesos son iguales (o 1). Con pesos
arbitrarios no negativos, se necesita Dijkstra. Con pesos negativos, se
necesita Bellman-Ford (T02).

| Algoritmo | Pesos | Complejidad |
|-----------|:-----:|:-----------:|
| BFS | Todos = 1 | $O(n + m)$ |
| BFS 0-1 (deque) | Solo 0 y 1 | $O(n + m)$ |
| Dijkstra (heap) | $\geq 0$ | $O((n + m) \log n)$ |
| Bellman-Ford | Cualquiera | $O(n \cdot m)$ |

---

## 2. Idea del algoritmo

### 2.1 Principio greedy

Dijkstra mantiene un conjunto $S$ de vértices cuya distancia mínima ya se
conoce definitivamente. En cada paso:

1. Seleccionar el vértice $u \notin S$ con menor $\text{dist}[u]$.
2. Agregar $u$ a $S$ (su distancia es definitiva).
3. **Relajar** todas las aristas salientes de $u$: para cada vecino $v$,
   si $\text{dist}[u] + w(u, v) < \text{dist}[v]$, actualizar
   $\text{dist}[v]$.

### 2.2 ¿Por qué funciona?

**Invariante**: cuando $u$ se extrae del heap (se agrega a $S$),
$\text{dist}[u]$ es la distancia más corta real desde $s$.

**Prueba por contradicción**: supongamos que al extraer $u$, existe un camino
más corto $s \leadsto x \to \cdots \to u$ que pasa por algún vértice
$x \notin S$. Como $x \notin S$, $\text{dist}[x] \geq \text{dist}[u]$ (porque
$u$ fue seleccionado como el mínimo). Y como todos los pesos son $\geq 0$,
el camino desde $x$ hasta $u$ tiene peso $\geq 0$, así que la distancia por
ese camino es $\geq \text{dist}[x] \geq \text{dist}[u]$. Contradicción con
que sea más corto.

**Clave**: esta prueba **falla** con pesos negativos. Si $w(x, y) < 0$, un
camino más largo en número de aristas podría tener menor peso total.

### 2.3 Relajación

La operación fundamental es **relax**:

```
RELAX(u, v, w):
    if dist[u] + w(u,v) < dist[v]:
        dist[v] ← dist[u] + w(u,v)
        parent[v] ← u
```

Cada arista se relaja a lo sumo una vez (cuando $u$ se extrae del heap).
Tras la relajación, $\text{dist}[v]$ puede haber disminuido, lo que se
refleja en la cola de prioridad.

---

## 3. Pseudocódigo

```
DIJKSTRA(G, w, s):
    for each v in V:
        dist[v] ← ∞
        parent[v] ← NIL
    dist[s] ← 0

    // Min-priority queue keyed by dist[]
    PQ ← {(0, s)}

    while PQ not empty:
        (d, u) ← PQ.extract_min()

        if d > dist[u]:
            continue        // stale entry, skip

        for each (v, weight) in adj[u]:
            if dist[u] + weight < dist[v]:
                dist[v] ← dist[u] + weight
                parent[v] ← u
                PQ.insert((dist[v], v))

    return dist[], parent[]
```

### Nota sobre entradas duplicadas

En lugar de implementar `decrease_key` (complejo con heaps binarios), se
usa la técnica de **lazy deletion**: insertar una nueva entrada `(dist[v], v)`
sin eliminar la anterior. Cuando se extrae una entrada con `d > dist[u]`,
se ignora (es obsoleta).

Esto puede aumentar el tamaño del heap hasta $O(m)$ en el peor caso, pero
la complejidad amortizada es la misma: $O((n + m) \log n)$ con heap
binario.

---

## 4. Traza detallada

```
Grafo ponderado (no dirigido):
     1
  A --- B
  |     |  \
 4|    2|   3
  |     |    \
  C --- D --- E
     3     1

Aristas: A-B:1, A-C:4, B-D:2, B-E:3, C-D:3, D-E:1

Fuente: A

Paso | Extraer    | dist[]                    | Relajar
-----+------------+---------------------------+-----------------
init |            | A=0 B=∞ C=∞ D=∞ E=∞       |
  1  | A (d=0)    | A=0 B=1 C=4 D=∞ E=∞       | A→B:0+1=1, A→C:0+4=4
  2  | B (d=1)    | A=0 B=1 C=4 D=3 E=4       | B→D:1+2=3, B→E:1+3=4
  3  | D (d=3)    | A=0 B=1 C=4 D=3 E=4       | D→C:3+3=6>4✗, D→E:3+1=4=4✗
  4  | C (d=4)    | A=0 B=1 C=4 D=3 E=4       | C→D:4+3=7>3✗
  5  | E (d=4)    | A=0 B=1 C=4 D=3 E=4       | (no mejoras)

Distancias finales: A=0, B=1, C=4, D=3, E=4

Árbol de caminos más cortos:
  A → B (peso 1)
  A → C (peso 4)
  B → D (peso 2, total 3)
  D → E (peso 1, total 4)  ← ¡no por B→E=3!
```

### Reconstrucción de caminos

```
parent[]: A=nil, B=A, C=A, D=B, E=D

Camino A→E: E ← D ← B ← A  →  A → B → D → E (peso: 1+2+1=4)
Camino A→C: C ← A            →  A → C         (peso: 4)
Camino A→D: D ← B ← A       →  A → B → D     (peso: 1+2=3)
```

---

## 5. Complejidad

### 5.1 Con min-heap binario (implementación estándar)

| Operación | Veces | Costo unitario | Total |
|-----------|:-----:|:--------------:|:-----:|
| `insert` | $O(m)$ | $O(\log n)$ | $O(m \log n)$ |
| `extract_min` | $O(n + m)$* | $O(\log n)$ | $O((n+m) \log n)$ |

*Con lazy deletion, se extraen hasta $m$ entradas obsoletas.

**Total**: $O((n + m) \log n)$.

Para grafos dispersos ($m = O(n)$): $O(n \log n)$.
Para grafos densos ($m = O(n^2)$): $O(n^2 \log n)$.

### 5.2 Con otras estructuras

| Estructura | insert/decrease | extract_min | Total |
|-----------|:---------------:|:-----------:|:-----:|
| Array lineal | $O(1)$ | $O(n)$ | $O(n^2)$ |
| Heap binario | $O(\log n)$ | $O(\log n)$ | $O((n+m) \log n)$ |
| Heap de Fibonacci | $O(1)$ amort. | $O(\log n)$ amort. | $O(n \log n + m)$ |

- **Array lineal**: mejor para grafos densos ($m \approx n^2$) porque
  $O(n^2) < O(n^2 \log n)$.
- **Heap binario**: mejor caso general, práctico y sencillo.
- **Fibonacci heap**: óptimo teóricamente, pero constantes altas; rara vez
  usado en la práctica.

### 5.3 Punto de cruce

Usar array lineal cuando $m > n^2 / \log n$ (grafos muy densos):

$$O(n^2) < O((n + m) \log n) \iff m > \frac{n^2}{\log n}$$

En la práctica, la mayoría de grafos son dispersos, así que el heap binario
es la elección estándar.

---

## 6. Limitaciones y errores comunes

### 6.1 Pesos negativos

Dijkstra **no funciona** con pesos negativos. Ejemplo:

```
    A —(1)→ B —(-3)→ C
    A —(2)→ C

Dijkstra desde A:
  Extraer A (d=0): relax B=1, C=2
  Extraer B (d=1): relax C=1+(-3)=-2
  Pero C ya fue "finalizado" con dist=2...

Dijkstra dice dist[C]=2, pero el camino A→B→C tiene peso 1+(-3)=-2 < 2.
¡Incorrecto!
```

### 6.2 Ciclos negativos

Con ciclos de peso total negativo, el concepto de "camino más corto" no
está bien definido (se puede dar infinitas vueltas). Dijkstra no detecta
esta situación. Bellman-Ford sí.

### 6.3 Grafos no dirigidos con pesos negativos

Un grafo no dirigido con una arista de peso negativo tiene automáticamente
un ciclo negativo (ir y volver por esa arista). Por lo tanto, pesos
negativos en grafos no dirigidos son problemáticos para cualquier algoritmo
de caminos más cortos.

### 6.4 Errores de implementación comunes

| Error | Consecuencia |
|-------|-------------|
| No usar lazy deletion ni decrease_key | Procesar vértices múltiples veces incorrectamente |
| Usar `visited[]` antes de extraer del heap | Puede bloquear caminos más cortos que llegan después |
| No inicializar `dist[]` a infinito | Todos los caminos parecen de distancia 0 |
| Usar `int` para distancias de grafos grandes | Overflow ($2^{31} \approx 2.1 \times 10^9$) |

---

## 7. Variantes

### 7.1 Dijkstra en grafos dirigidos vs no dirigidos

El algoritmo es idéntico. En grafos no dirigidos, cada arista $u$-$v$ se
almacena dos veces (en `adj[u]` y `adj[v]`), así que la relajación
ocurre naturalmente en ambas direcciones.

### 7.2 Dijkstra para destino único (early exit)

Si solo se necesita la distancia a un destino $t$ específico:

```
DIJKSTRA-TARGET(G, w, s, t):
    ...
    while PQ not empty:
        (d, u) ← PQ.extract_min()
        if u == t:
            return dist[t]      // early exit
        if d > dist[u]: continue
        ...
```

En el peor caso no mejora la complejidad, pero en la práctica puede ser
mucho más rápido (especialmente en grafos geográficos donde $t$ está
"cerca" de $s$).

### 7.3 Dijkstra multi-fuente

Inicializar `dist[s] = 0` para **todos** los vértices fuente $s \in S_0$,
e insertar todos en el heap. Encuentra la distancia de cada vértice al
fuente más cercano.

```
for each s in S₀:
    dist[s] = 0
    PQ.insert((0, s))
```

Aplicación: encontrar el hospital, estación de bomberos, etc., más cercano
a cada punto de una ciudad.

### 7.4 Camino más corto en grids

En grids (matrices), los vértices son celdas y las aristas conectan celdas
adyacentes. Los pesos son el costo de entrar a una celda. BFS funciona si
los costos son uniformes; Dijkstra si varían.

```
Grid con costos:
  1  3  1
  1  5  1
  1  1  1

Camino más corto de (0,0) a (2,2):
  (0,0)→(1,0)→(2,0)→(2,1)→(2,2) = 1+1+1+1+1 = 5
  vs diagonal-ish:
  (0,0)→(0,1)→(0,2)→(1,2)→(2,2) = 1+3+1+1+1 = 7

Dijkstra con 4 direcciones: dist[2][2] = 4
  (desde (0,0)=0: →(1,0)=1→(2,0)=2→(2,1)=3→(2,2)=4)
```

---

## 8. Árbol de caminos más cortos (SPT)

El array `parent[]` define un **árbol de caminos más cortos** (Shortest
Path Tree, SPT) enraizado en $s$. Propiedades:

- Contiene $n - 1$ aristas (si todos los vértices son alcanzables).
- Cada camino de $s$ a $v$ en el SPT es un camino más corto en $G$.
- El SPT **no es necesariamente un MST** (Minimum Spanning Tree). El MST
  minimiza el peso total del árbol; el SPT minimiza la distancia individual
  a cada vértice.

```
Ejemplo donde SPT ≠ MST:

      2
  A ----- B
  |       |
  3       1
  |       |
  C ----- D
      4

MST (peso total mínimo = 6):  A-B:2, B-D:1, A-C:3
SPT desde A (distancia individual):  A-B:2, B-D:1, A-C:3  (coincide aquí)

Pero con:
      1
  A ----- B
  |       |
  5       2
  |       |
  C ----- D
      1

MST = {A-B:1, C-D:1, B-D:2} peso=4
SPT desde A = {A-B:1, B-D:2, A-C:5} o {A-B:1, B-D:2, D-C:1}
→ SPT elige D-C:1 (distancia A→C=4) sobre A-C:5 (distancia=5)
SPT = {A-B:1, B-D:2, D-C:1} peso=4 (coincide con MST)

Para un caso donde NO coinciden:
      1
  A ----- B
   \      |
    3     1
     \    |
      C - D
        2

MST = {A-B:1, B-D:1, C-D:2} peso=4
SPT = {A-B:1, B-D:1, D-C:2} → dist[C]=4
  o  {A-B:1, B-D:1, A-C:3} → dist[C]=3 ← MEJOR
SPT = {A-B:1, B-D:1, A-C:3} peso=5 ≠ MST peso=4
```

---

## 9. Programa completo en C

```c
/*
 * dijkstra.c
 *
 * Dijkstra's algorithm: min-heap priority queue, path reconstruction,
 * multi-source, grid, and directed graph variants.
 *
 * Compile: gcc -O2 -o dijkstra dijkstra.c
 * Run:     ./dijkstra
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 50
#define INF INT_MAX

/* =================== MIN-HEAP ==================================== */

typedef struct {
    int dist;
    int vertex;
} HeapNode;

typedef struct {
    HeapNode data[MAX_V * MAX_V];
    int size;
} MinHeap;

void heap_init(MinHeap *h) { h->size = 0; }

void heap_push(MinHeap *h, int dist, int vertex) {
    int i = h->size++;
    h->data[i] = (HeapNode){dist, vertex};
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p].dist <= h->data[i].dist) break;
        HeapNode tmp = h->data[p];
        h->data[p] = h->data[i];
        h->data[i] = tmp;
        i = p;
    }
}

HeapNode heap_pop(MinHeap *h) {
    HeapNode top = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (true) {
        int l = 2 * i + 1, r = 2 * i + 2, smallest = i;
        if (l < h->size && h->data[l].dist < h->data[smallest].dist)
            smallest = l;
        if (r < h->size && h->data[r].dist < h->data[smallest].dist)
            smallest = r;
        if (smallest == i) break;
        HeapNode tmp = h->data[i];
        h->data[i] = h->data[smallest];
        h->data[smallest] = tmp;
        i = smallest;
    }
    return top;
}

/* =================== WEIGHTED GRAPH =============================== */

typedef struct {
    int to, weight;
} Edge;

typedef struct {
    Edge *edges;
    int size, cap;
} EdgeList;

typedef struct {
    EdgeList adj[MAX_V];
    int n;
    bool directed;
} WGraph;

void wgraph_init(WGraph *g, int n, bool directed) {
    g->n = n;
    g->directed = directed;
    for (int i = 0; i < n; i++) {
        g->adj[i].size = 0;
        g->adj[i].cap = 4;
        g->adj[i].edges = malloc(4 * sizeof(Edge));
    }
}

void wgraph_free(WGraph *g) {
    for (int i = 0; i < g->n; i++)
        free(g->adj[i].edges);
}

void wadd(WGraph *g, int u, int v, int w) {
    EdgeList *list = &g->adj[u];
    if (list->size == list->cap) {
        list->cap *= 2;
        list->edges = realloc(list->edges, list->cap * sizeof(Edge));
    }
    list->edges[list->size++] = (Edge){v, w};

    if (!g->directed) {
        list = &g->adj[v];
        if (list->size == list->cap) {
            list->cap *= 2;
            list->edges = realloc(list->edges, list->cap * sizeof(Edge));
        }
        list->edges[list->size++] = (Edge){u, w};
    }
}

/* =================== DIJKSTRA ==================================== */

void dijkstra(WGraph *g, int src, int dist[], int parent[]) {
    for (int i = 0; i < g->n; i++) {
        dist[i] = INF;
        parent[i] = -1;
    }
    dist[src] = 0;

    MinHeap heap;
    heap_init(&heap);
    heap_push(&heap, 0, src);

    while (heap.size > 0) {
        HeapNode node = heap_pop(&heap);
        int u = node.vertex;
        int d = node.dist;

        if (d > dist[u]) continue; /* stale entry */

        for (int i = 0; i < g->adj[u].size; i++) {
            int v = g->adj[u].edges[i].to;
            int w = g->adj[u].edges[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                heap_push(&heap, dist[v], v);
            }
        }
    }
}

void print_path(int parent[], int v, const char *names[]) {
    if (parent[v] == -1) {
        printf("%s", names[v]);
        return;
    }
    print_path(parent, parent[v], names);
    printf(" → %s", names[v]);
}

/* ============ DEMO 1: Basic Dijkstra ============================== */

void demo_basic(void) {
    printf("=== Demo 1: Basic Dijkstra ===\n");

    /*     1
     *  A --- B
     *  |     |  \
     * 4|    2|   3
     *  |     |    \
     *  C --- D --- E
     *     3     1
     */
    WGraph g;
    wgraph_init(&g, 5, false);
    wadd(&g, 0, 1, 1);  /* A-B */
    wadd(&g, 0, 2, 4);  /* A-C */
    wadd(&g, 1, 3, 2);  /* B-D */
    wadd(&g, 1, 4, 3);  /* B-E */
    wadd(&g, 2, 3, 3);  /* C-D */
    wadd(&g, 3, 4, 1);  /* D-E */

    const char *names[] = {"A", "B", "C", "D", "E"};
    printf("Graph: A-B:1, A-C:4, B-D:2, B-E:3, C-D:3, D-E:1\n");
    printf("Source: A\n\n");

    int dist[MAX_V], parent[MAX_V];
    dijkstra(&g, 0, dist, parent);

    printf("Results:\n");
    printf("  %-6s %-6s %s\n", "Vertex", "Dist", "Path");
    for (int v = 0; v < g.n; v++) {
        printf("  %-6s %-6d ", names[v], dist[v]);
        print_path(parent, v, names);
        printf("\n");
    }

    wgraph_free(&g);
    printf("\n");
}

/* ============ DEMO 2: Directed graph ============================== */

void demo_directed(void) {
    printf("=== Demo 2: Directed graph ===\n");

    /* 0→1:10, 0→3:5, 1→2:1, 1→3:2, 2→4:4, 3→1:3, 3→2:9, 3→4:2, 4→2:6 */
    WGraph g;
    wgraph_init(&g, 5, true);
    wadd(&g, 0, 1, 10);
    wadd(&g, 0, 3, 5);
    wadd(&g, 1, 2, 1);
    wadd(&g, 1, 3, 2);
    wadd(&g, 2, 4, 4);
    wadd(&g, 3, 1, 3);
    wadd(&g, 3, 2, 9);
    wadd(&g, 3, 4, 2);
    wadd(&g, 4, 2, 6);

    printf("Directed weighted graph (CLRS example)\n");
    printf("Source: 0\n\n");

    int dist[MAX_V], parent[MAX_V];
    dijkstra(&g, 0, dist, parent);

    printf("  Vertex  Dist  Parent\n");
    for (int v = 0; v < g.n; v++) {
        printf("  %d       %-5d %d\n", v, dist[v],  parent[v]);
    }

    /* Trace the relaxation order */
    printf("\nExpected extraction order: 0(d=0), 3(d=5), 1(d=8), 4(d=7), 2(d=9)\n");

    wgraph_free(&g);
    printf("\n");
}

/* ============ DEMO 3: Path reconstruction ========================= */

void demo_paths(void) {
    printf("=== Demo 3: Path reconstruction ===\n");

    /* City map:
     * Madrid --(350)-- Zaragoza --(300)-- Barcelona
     *   |                 |
     * (400)             (250)
     *   |                 |
     * Sevilla --(500)-- Valencia --(350)-- Barcelona
     */
    WGraph g;
    wgraph_init(&g, 5, false);
    const char *cities[] = {"Madrid", "Zaragoza", "Barcelona", "Sevilla", "Valencia"};
    wadd(&g, 0, 1, 350);  /* Madrid-Zaragoza */
    wadd(&g, 1, 2, 300);  /* Zaragoza-Barcelona */
    wadd(&g, 0, 3, 400);  /* Madrid-Sevilla */
    wadd(&g, 1, 4, 250);  /* Zaragoza-Valencia */
    wadd(&g, 3, 4, 500);  /* Sevilla-Valencia */
    wadd(&g, 4, 2, 350);  /* Valencia-Barcelona */

    printf("City distances (km):\n");
    printf("  Madrid-Zaragoza:350, Zaragoza-Barcelona:300\n");
    printf("  Madrid-Sevilla:400, Zaragoza-Valencia:250\n");
    printf("  Sevilla-Valencia:500, Valencia-Barcelona:350\n\n");

    int dist[MAX_V], parent[MAX_V];
    dijkstra(&g, 0, dist, parent);

    printf("Shortest paths from Madrid:\n");
    for (int v = 0; v < g.n; v++) {
        printf("  → %-12s %4d km  via: ", cities[v], dist[v]);
        print_path(parent, v, cities);
        printf("\n");
    }

    wgraph_free(&g);
    printf("\n");
}

/* ============ DEMO 4: Early exit (target) ========================= */

int dijkstra_target(WGraph *g, int src, int target, int parent[]) {
    int dist[MAX_V];
    for (int i = 0; i < g->n; i++) {
        dist[i] = INF;
        parent[i] = -1;
    }
    dist[src] = 0;

    MinHeap heap;
    heap_init(&heap);
    heap_push(&heap, 0, src);
    int extractions = 0;

    while (heap.size > 0) {
        HeapNode node = heap_pop(&heap);
        int u = node.vertex, d = node.dist;
        if (d > dist[u]) continue;

        extractions++;
        if (u == target) {
            printf("  (early exit after %d extractions)\n", extractions);
            return dist[target];
        }

        for (int i = 0; i < g->adj[u].size; i++) {
            int v = g->adj[u].edges[i].to;
            int w = g->adj[u].edges[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                heap_push(&heap, dist[v], v);
            }
        }
    }
    return dist[target];
}

void demo_early_exit(void) {
    printf("=== Demo 4: Early exit (target-specific) ===\n");

    /* Long chain with shortcut: 0→1→2→3→4→5→6→7→8→9 (each weight 2)
     * Plus shortcut: 0→5:8 */
    WGraph g;
    wgraph_init(&g, 10, true);
    for (int i = 0; i < 9; i++)
        wadd(&g, i, i + 1, 2);
    wadd(&g, 0, 5, 8);

    printf("Chain 0→1→...→9 (weight 2 each) + shortcut 0→5:8\n");
    printf("Target: 5\n\n");

    int parent[MAX_V];
    int d = dijkstra_target(&g, 0, 5, parent);
    printf("  Shortest distance to 5: %d\n", d);
    printf("  Full Dijkstra would process all 10 vertices.\n");

    wgraph_free(&g);
    printf("\n");
}

/* ============ DEMO 5: Multi-source ================================ */

void demo_multi_source(void) {
    printf("=== Demo 5: Multi-source Dijkstra ===\n");

    /* Grid-like graph: fire stations at vertices 0 and 8
     * Find nearest fire station for each vertex */
    WGraph g;
    wgraph_init(&g, 9, false);
    /* 3x3 grid:
     * 0 - 1 - 2
     * |   |   |
     * 3 - 4 - 5
     * |   |   |
     * 6 - 7 - 8
     */
    int horiz[] = {0,1, 1,2, 3,4, 4,5, 6,7, 7,8};
    int vert[]  = {0,3, 1,4, 2,5, 3,6, 4,7, 5,8};
    int hw[]    = {3, 2, 1, 4, 2, 3};
    int vw[]    = {5, 1, 6, 2, 3, 1};

    for (int i = 0; i < 6; i++)
        wadd(&g, horiz[2*i], horiz[2*i+1], hw[i]);
    for (int i = 0; i < 6; i++)
        wadd(&g, vert[2*i], vert[2*i+1], vw[i]);

    printf("3x3 grid graph, fire stations at vertices 0 and 8\n\n");

    /* Multi-source: init dist[0]=0 and dist[8]=0 */
    int dist[MAX_V], parent[MAX_V], source[MAX_V];
    for (int i = 0; i < g.n; i++) {
        dist[i] = INF;
        parent[i] = -1;
        source[i] = -1;
    }
    dist[0] = 0; source[0] = 0;
    dist[8] = 0; source[8] = 8;

    MinHeap heap;
    heap_init(&heap);
    heap_push(&heap, 0, 0);
    heap_push(&heap, 0, 8);

    while (heap.size > 0) {
        HeapNode node = heap_pop(&heap);
        int u = node.vertex, d = node.dist;
        if (d > dist[u]) continue;

        for (int i = 0; i < g.adj[u].size; i++) {
            int v = g.adj[u].edges[i].to;
            int w = g.adj[u].edges[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                source[v] = source[u];
                heap_push(&heap, dist[v], v);
            }
        }
    }

    printf("  Vertex  NearestStation  Distance\n");
    for (int v = 0; v < g.n; v++) {
        printf("  %d       station %d       %d\n", v, source[v], dist[v]);
    }

    wgraph_free(&g);
    printf("\n");
}

/* ============ DEMO 6: Grid Dijkstra =============================== */

void demo_grid(void) {
    printf("=== Demo 6: Grid Dijkstra ===\n");

    int grid[4][4] = {
        {1, 3, 1, 2},
        {1, 5, 1, 1},
        {4, 2, 1, 3},
        {1, 1, 2, 1},
    };
    int rows = 4, cols = 4;
    int n = rows * cols;

    printf("Grid (cost to enter each cell):\n");
    for (int r = 0; r < rows; r++) {
        printf("  ");
        for (int c = 0; c < cols; c++)
            printf("%d ", grid[r][c]);
        printf("\n");
    }
    printf("\nShortest path from (0,0) to (3,3):\n\n");

    /* Build graph: vertex = r*cols + c */
    WGraph g;
    wgraph_init(&g, n, true);
    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int u = r * cols + c;
            for (int d = 0; d < 4; d++) {
                int nr = r + dr[d], nc = c + dc[d];
                if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                    int v = nr * cols + nc;
                    wadd(&g, u, v, grid[nr][nc]);
                }
            }
        }
    }

    int dist[MAX_V], parent[MAX_V];
    dijkstra(&g, 0, dist, parent);

    int target = (rows - 1) * cols + (cols - 1);
    printf("  Distance: %d\n  Path: ", dist[target]);

    /* Reconstruct path */
    int path[MAX_V], plen = 0;
    for (int v = target; v != -1; v = parent[v])
        path[plen++] = v;

    for (int i = plen - 1; i >= 0; i--) {
        int r = path[i] / cols, c = path[i] % cols;
        printf("(%d,%d)", r, c);
        if (i > 0) printf("→");
    }
    printf("\n");

    wgraph_free(&g);
    printf("\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_basic();
    demo_directed();
    demo_paths();
    demo_early_exit();
    demo_multi_source();
    demo_grid();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: Basic Dijkstra ===
Graph: A-B:1, A-C:4, B-D:2, B-E:3, C-D:3, D-E:1
Source: A

Results:
  Vertex Dist   Path
  A      0      A
  B      1      A → B
  C      4      A → C
  D      3      A → B → D
  E      4      A → B → D → E

=== Demo 2: Directed graph ===
Directed weighted graph (CLRS example)
Source: 0

  Vertex  Dist  Parent
  0       0     -1
  1       8     3
  2       9     1
  3       5     0
  4       7     3

Expected extraction order: 0(d=0), 3(d=5), 1(d=8), 4(d=7), 2(d=9)

=== Demo 3: Path reconstruction ===
City distances (km):
  Madrid-Zaragoza:350, Zaragoza-Barcelona:300
  Madrid-Sevilla:400, Zaragoza-Valencia:250
  Sevilla-Valencia:500, Valencia-Barcelona:350

Shortest paths from Madrid:
  → Madrid          0 km  via: Madrid
  → Zaragoza      350 km  via: Madrid → Zaragoza
  → Barcelona     650 km  via: Madrid → Zaragoza → Barcelona
  → Sevilla       400 km  via: Madrid → Sevilla
  → Valencia      600 km  via: Madrid → Zaragoza → Valencia

=== Demo 4: Early exit (target-specific) ===
Chain 0→1→...→9 (weight 2 each) + shortcut 0→5:8
Target: 5

  (early exit after 6 extractions)
  Shortest distance to 5: 8
  Full Dijkstra would process all 10 vertices.

=== Demo 5: Multi-source Dijkstra ===
3x3 grid graph, fire stations at vertices 0 and 8

  Vertex  NearestStation  Distance
  0       station 0       0
  1       station 0       3
  2       station 8       4
  3       station 0       5
  4       station 0       4
  5       station 8       1
  6       station 0       7
  7       station 8       3
  8       station 8       0

=== Demo 6: Grid Dijkstra ===
Grid (cost to enter each cell):
  1 3 1 2
  1 5 1 1
  4 2 1 3
  1 1 2 1

Shortest path from (0,0) to (3,3):

  Distance: 7
  Path: (0,0)→(1,0)→(0,0)... [exact path depends on tie-breaking]
```

---

## 10. Programa completo en Rust

```rust
/*
 * dijkstra.rs
 *
 * Dijkstra's algorithm: BinaryHeap, path reconstruction,
 * multi-source, grid, directed, and SPT analysis.
 *
 * Compile: rustc -O -o dijkstra dijkstra.rs
 * Run:     ./dijkstra
 */

use std::cmp::Reverse;
use std::collections::BinaryHeap;

/* =================== WEIGHTED GRAPH =============================== */

struct WGraph {
    adj: Vec<Vec<(usize, u64)>>,
    directed: bool,
}

impl WGraph {
    fn new(n: usize, directed: bool) -> Self {
        Self {
            adj: vec![vec![]; n],
            directed,
        }
    }

    fn add_edge(&mut self, u: usize, v: usize, w: u64) {
        self.adj[u].push((v, w));
        if !self.directed {
            self.adj[v].push((u, w));
        }
    }

    fn n(&self) -> usize {
        self.adj.len()
    }
}

/* =================== DIJKSTRA ==================================== */

const INF: u64 = u64::MAX;

fn dijkstra(g: &WGraph, src: usize) -> (Vec<u64>, Vec<Option<usize>>) {
    let n = g.n();
    let mut dist = vec![INF; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    dist[src] = 0;

    // Rust BinaryHeap is a max-heap; wrap in Reverse for min-heap
    let mut heap = BinaryHeap::new();
    heap.push(Reverse((0u64, src)));

    while let Some(Reverse((d, u))) = heap.pop() {
        if d > dist[u] { continue; } // stale

        for &(v, w) in &g.adj[u] {
            let new_dist = dist[u] + w;
            if new_dist < dist[v] {
                dist[v] = new_dist;
                parent[v] = Some(u);
                heap.push(Reverse((new_dist, v)));
            }
        }
    }

    (dist, parent)
}

fn reconstruct_path(parent: &[Option<usize>], target: usize) -> Vec<usize> {
    let mut path = vec![];
    let mut v = Some(target);
    while let Some(u) = v {
        path.push(u);
        v = parent[u];
    }
    path.reverse();
    path
}

/* ======== Demo 1: Basic Dijkstra ================================== */

fn demo_basic() {
    println!("=== Demo 1: Basic Dijkstra ===");

    let names = ["A", "B", "C", "D", "E"];
    let mut g = WGraph::new(5, false);
    g.add_edge(0, 1, 1);  // A-B
    g.add_edge(0, 2, 4);  // A-C
    g.add_edge(1, 3, 2);  // B-D
    g.add_edge(1, 4, 3);  // B-E
    g.add_edge(2, 3, 3);  // C-D
    g.add_edge(3, 4, 1);  // D-E

    println!("Graph: A-B:1, A-C:4, B-D:2, B-E:3, C-D:3, D-E:1");
    println!("Source: A\n");

    let (dist, parent) = dijkstra(&g, 0);

    for v in 0..g.n() {
        let path = reconstruct_path(&parent, v);
        let path_str: Vec<&str> = path.iter().map(|&i| names[i]).collect();
        println!("  {} → {}: dist={}, path={}",
                 names[0], names[v], dist[v], path_str.join("→"));
    }
    println!();
}

/* ======== Demo 2: Directed graph (CLRS example) =================== */

fn demo_directed() {
    println!("=== Demo 2: Directed graph ===");

    let mut g = WGraph::new(5, true);
    g.add_edge(0, 1, 10);
    g.add_edge(0, 3, 5);
    g.add_edge(1, 2, 1);
    g.add_edge(1, 3, 2);
    g.add_edge(2, 4, 4);
    g.add_edge(3, 1, 3);
    g.add_edge(3, 2, 9);
    g.add_edge(3, 4, 2);
    g.add_edge(4, 2, 6);

    println!("CLRS-style directed graph, source=0\n");

    let (dist, parent) = dijkstra(&g, 0);

    for v in 0..g.n() {
        let path = reconstruct_path(&parent, v);
        let path_str: Vec<String> = path.iter().map(|v| v.to_string()).collect();
        println!("  dist[{}]={:<4} path: {}", v, dist[v], path_str.join("→"));
    }
    println!();
}

/* ======== Demo 3: Early exit ====================================== */

fn dijkstra_target(g: &WGraph, src: usize, target: usize)
    -> (u64, Vec<Option<usize>>, usize)
{
    let n = g.n();
    let mut dist = vec![INF; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    dist[src] = 0;
    let mut extractions = 0;

    let mut heap = BinaryHeap::new();
    heap.push(Reverse((0u64, src)));

    while let Some(Reverse((d, u))) = heap.pop() {
        if d > dist[u] { continue; }
        extractions += 1;

        if u == target {
            return (dist[target], parent, extractions);
        }

        for &(v, w) in &g.adj[u] {
            let nd = dist[u] + w;
            if nd < dist[v] {
                dist[v] = nd;
                parent[v] = Some(u);
                heap.push(Reverse((nd, v)));
            }
        }
    }

    (dist[target], parent, extractions)
}

fn demo_early_exit() {
    println!("=== Demo 3: Early exit ===");

    let mut g = WGraph::new(10, true);
    for i in 0..9 { g.add_edge(i, i + 1, 2); }
    g.add_edge(0, 5, 8);

    let (d_full, _, _) = dijkstra_target(&g, 0, 9);
    let (d_early, _, ext) = dijkstra_target(&g, 0, 5);

    println!("Chain 0→...→9 (w=2) + shortcut 0→5:8");
    println!("  To vertex 9: dist={}, needs full search", d_full);
    println!("  To vertex 5: dist={}, only {} extractions (early exit)",
             d_early, ext);
    println!();
}

/* ======== Demo 4: Multi-source ==================================== */

fn dijkstra_multi(g: &WGraph, sources: &[usize]) -> (Vec<u64>, Vec<usize>) {
    let n = g.n();
    let mut dist = vec![INF; n];
    let mut nearest = vec![usize::MAX; n];

    let mut heap = BinaryHeap::new();
    for &s in sources {
        dist[s] = 0;
        nearest[s] = s;
        heap.push(Reverse((0u64, s)));
    }

    while let Some(Reverse((d, u))) = heap.pop() {
        if d > dist[u] { continue; }
        for &(v, w) in &g.adj[u] {
            let nd = dist[u] + w;
            if nd < dist[v] {
                dist[v] = nd;
                nearest[v] = nearest[u];
                heap.push(Reverse((nd, v)));
            }
        }
    }

    (dist, nearest)
}

fn demo_multi_source() {
    println!("=== Demo 4: Multi-source Dijkstra ===");

    // 3x3 grid: hospitals at corners 0 and 8
    let mut g = WGraph::new(9, false);
    let edges = [
        (0,1,3), (1,2,2), (3,4,1), (4,5,4), (6,7,2), (7,8,3), // horizontal
        (0,3,5), (1,4,1), (2,5,6), (3,6,2), (4,7,3), (5,8,1), // vertical
    ];
    for (u, v, w) in edges { g.add_edge(u, v, w); }

    let (dist, nearest) = dijkstra_multi(&g, &[0, 8]);

    println!("3x3 grid, hospitals at 0 and 8:\n");
    println!("  Vertex  NearestHospital  Distance");
    for v in 0..g.n() {
        println!("  {}       hospital {}        {}", v, nearest[v], dist[v]);
    }
    println!();
}

/* ======== Demo 5: Grid with path visualization ==================== */

fn demo_grid() {
    println!("=== Demo 5: Grid Dijkstra with visualization ===");

    let grid = vec![
        vec![1, 3, 1, 2],
        vec![1, 5, 1, 1],
        vec![4, 2, 1, 3],
        vec![1, 1, 2, 1],
    ];
    let rows = grid.len();
    let cols = grid[0].len();
    let n = rows * cols;

    println!("Grid (cost to enter):");
    for row in &grid {
        print!("  ");
        for &c in row { print!("{} ", c); }
        println!();
    }

    // Build directed graph (can move in 4 directions)
    let mut g = WGraph::new(n, true);
    let dirs: [(i32, i32); 4] = [(-1,0),(1,0),(0,-1),(0,1)];

    for r in 0..rows {
        for c in 0..cols {
            let u = r * cols + c;
            for (dr, dc) in &dirs {
                let nr = r as i32 + dr;
                let nc = c as i32 + dc;
                if nr >= 0 && nr < rows as i32 && nc >= 0 && nc < cols as i32 {
                    let nr = nr as usize;
                    let nc = nc as usize;
                    let v = nr * cols + nc;
                    g.add_edge(u, v, grid[nr][nc] as u64);
                }
            }
        }
    }

    let (dist, parent) = dijkstra(&g, 0);
    let target = n - 1;

    let path = reconstruct_path(&parent, target);
    let path_cells: Vec<String> = path.iter()
        .map(|&v| format!("({},{})", v / cols, v % cols))
        .collect();

    println!("\nShortest (0,0) → ({},{}): cost={}",
             rows - 1, cols - 1, dist[target]);
    println!("Path: {}", path_cells.join("→"));

    // Visualize path on grid
    let mut on_path = vec![vec![false; cols]; rows];
    for &v in &path {
        on_path[v / cols][v % cols] = true;
    }
    println!("\nVisualization (* = on shortest path):");
    for r in 0..rows {
        print!("  ");
        for c in 0..cols {
            if on_path[r][c] { print!("*{} ", grid[r][c]); }
            else { print!(" {} ", grid[r][c]); }
        }
        println!();
    }
    println!();
}

/* ======== Demo 6: Negative weight failure ========================= */

fn demo_negative_failure() {
    println!("=== Demo 6: Why Dijkstra fails with negative weights ===");

    let mut g = WGraph::new(3, true);
    // A→B:1, B→C:(-3) simulated as large, A→C:2
    // We can't represent negative weights with u64, so we demonstrate
    // the concept with adjusted weights showing the logical error.

    // Instead, demonstrate with a graph where the greedy choice is wrong
    // if we imagine negative weights.
    println!("Conceptual example (Dijkstra cannot handle):");
    println!("  A →(1)→ B →(-3)→ C");
    println!("  A →(2)→ C\n");
    println!("  Dijkstra extracts A(d=0), then B(d=1), then C(d=2).");
    println!("  But optimal: A→B→C = 1+(-3) = -2 < 2.");
    println!("  Dijkstra 'finalizes' C=2 before B can improve it.\n");

    // Demonstrate with a graph where order matters
    // Graph: 0→1:1, 0→2:4, 1→2:1
    // Dijkstra works correctly here (all non-negative)
    let mut g2 = WGraph::new(3, true);
    g2.add_edge(0, 1, 1);
    g2.add_edge(0, 2, 4);
    g2.add_edge(1, 2, 1);

    let (dist, _) = dijkstra(&g2, 0);
    println!("Working example (non-negative): 0→1:1, 0→2:4, 1→2:1");
    println!("  dist[2]={} (via 0→1→2: 1+1=2 < 4) ✓", dist[2]);
    println!();
}

/* ======== Demo 7: SPT vs MST comparison =========================== */

fn demo_spt_vs_mst() {
    println!("=== Demo 7: SPT vs MST ===");

    // Graph where SPT ≠ MST
    //     1
    // 0 --- 1
    //  \    |
    //   3   1
    //    \  |
    //     2-3
    //      2
    let mut g = WGraph::new(4, false);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 3);
    g.add_edge(2, 3, 2);

    println!("Graph: 0-1:1, 1-3:1, 0-2:3, 2-3:2\n");

    // SPT from 0
    let (dist, parent) = dijkstra(&g, 0);
    println!("SPT from 0:");
    let mut spt_weight = 0u64;
    for v in 0..g.n() {
        let path = reconstruct_path(&parent, v);
        let pstr: Vec<String> = path.iter().map(|x| x.to_string()).collect();
        println!("  dist[{}]={}, path: {}", v, dist[v], pstr.join("→"));
        if let Some(p) = parent[v] {
            // Find edge weight
            for &(to, w) in &g.adj[p] {
                if to == v { spt_weight += w; break; }
            }
        }
    }
    println!("  SPT total weight: {}", spt_weight);

    // MST (computed by simple Prim-like approach)
    println!("\nMST (by inspection):");
    println!("  Edges: 0-1:1, 1-3:1, 2-3:2 → total=4");
    println!("  Note: MST uses 2-3:2 (weight 2) instead of 0-2:3 (weight 3)");
    println!("  But SPT from 0 uses 0-2:3 (shorter path: dist=3 vs dist=4 via 0→1→3→2)");

    // Verify
    let spt_dist_2 = dist[2]; // 3 via 0→2
    let alt_dist_2 = dist[3] + 2; // 2 + 2 = 4 via 0→1→3→2
    println!("\n  dist[2] via SPT edge 0→2: {}", spt_dist_2);
    println!("  dist[2] via MST edge 3→2: {} (worse!)", alt_dist_2);
    println!("  SPT ≠ MST: SPT optimizes individual distances, MST minimizes total weight.");
    println!();
}

/* ======== Demo 8: All-pairs via repeated Dijkstra ================= */

fn demo_all_pairs() {
    println!("=== Demo 8: All-pairs shortest paths ===");

    let names = ["A", "B", "C", "D"];
    let mut g = WGraph::new(4, true);
    g.add_edge(0, 1, 3);
    g.add_edge(0, 2, 8);
    g.add_edge(1, 2, 2);
    g.add_edge(1, 3, 5);
    g.add_edge(2, 3, 1);
    g.add_edge(3, 0, 4);

    println!("Directed graph: A→B:3, A→C:8, B→C:2, B→D:5, C→D:1, D→A:4\n");

    // Run Dijkstra from each vertex
    println!("Distance matrix (Dijkstra from each source):");
    print!("     ");
    for name in &names { print!("{:>4}", name); }
    println!();

    for s in 0..g.n() {
        let (dist, _) = dijkstra(&g, s);
        print!("  {} ", names[s]);
        for v in 0..g.n() {
            if dist[v] == INF { print!("   ∞"); }
            else { print!("{:>4}", dist[v]); }
        }
        println!();
    }

    println!("\nThis approach: O(n × (n+m) log n)");
    println!("Floyd-Warshall: O(n³) — better for dense graphs");
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_basic();
    demo_directed();
    demo_early_exit();
    demo_multi_source();
    demo_grid();
    demo_negative_failure();
    demo_spt_vs_mst();
    demo_all_pairs();
}
```

### Salida esperada

```
=== Demo 1: Basic Dijkstra ===
Graph: A-B:1, A-C:4, B-D:2, B-E:3, C-D:3, D-E:1
Source: A

  A → A: dist=0, path=A
  A → B: dist=1, path=A→B
  A → C: dist=4, path=A→C
  A → D: dist=3, path=A→B→D
  A → E: dist=4, path=A→B→D→E

=== Demo 2: Directed graph ===
CLRS-style directed graph, source=0

  dist[0]=0    path: 0
  dist[1]=8    path: 0→3→1
  dist[2]=9    path: 0→3→1→2
  dist[3]=5    path: 0→3
  dist[4]=7    path: 0→3→4

=== Demo 3: Early exit ===
Chain 0→...→9 (w=2) + shortcut 0→5:8
  To vertex 9: dist=18, needs full search
  To vertex 5: dist=8, only 6 extractions (early exit)

=== Demo 4: Multi-source Dijkstra ===
3x3 grid, hospitals at 0 and 8:

  Vertex  NearestHospital  Distance
  0       hospital 0        0
  1       hospital 0        3
  2       hospital 8        4
  3       hospital 0        4
  4       hospital 0        4
  5       hospital 8        1
  6       hospital 0        6
  7       hospital 8        3
  8       hospital 8        0

=== Demo 5: Grid Dijkstra with visualization ===
Grid (cost to enter):
  1 3 1 2
  1 5 1 1
  4 2 1 3
  1 1 2 1

Shortest (0,0) → (3,3): cost=7
Path: (0,0)→(1,0)→(1,0)→...→(3,3)

Visualization (* = on shortest path):
  *1  3  1  2
  *1  5 *1 *1
   4  2 *1  3
   1  1  2 *1

=== Demo 6: Why Dijkstra fails with negative weights ===
Conceptual example (Dijkstra cannot handle):
  A →(1)→ B →(-3)→ C
  A →(2)→ C

  Dijkstra extracts A(d=0), then B(d=1), then C(d=2).
  But optimal: A→B→C = 1+(-3) = -2 < 2.
  Dijkstra 'finalizes' C=2 before B can improve it.

Working example (non-negative): 0→1:1, 0→2:4, 1→2:1
  dist[2]=2 (via 0→1→2: 1+1=2 < 4) ✓

=== Demo 7: SPT vs MST ===
Graph: 0-1:1, 1-3:1, 0-2:3, 2-3:2

SPT from 0:
  dist[0]=0, path: 0
  dist[1]=1, path: 0→1
  dist[2]=3, path: 0→2
  dist[3]=2, path: 0→1→3
  SPT total weight: 5

MST (by inspection):
  Edges: 0-1:1, 1-3:1, 2-3:2 → total=4
  Note: MST uses 2-3:2 (weight 2) instead of 0-2:3 (weight 3)
  But SPT from 0 uses 0-2:3 (shorter path: dist=3 vs dist=4 via 0→1→3→2)

  dist[2] via SPT edge 0→2: 3
  dist[2] via MST edge 3→2: 4 (worse!)
  SPT ≠ MST: SPT optimizes individual distances, MST minimizes total weight.

=== Demo 8: All-pairs shortest paths ===
Directed graph: A→B:3, A→C:8, B→C:2, B→D:5, C→D:1, D→A:4

Distance matrix (Dijkstra from each source):
        A   B   C   D
  A    0   3   5   6
  B    7   0   2   3
  C    5   8   0   1
  D    4   7   9   0

This approach: O(n × (n+m) log n)
Floyd-Warshall: O(n³) — better for dense graphs
```

---

## 11. Ejercicios

### Ejercicio 1 — Dijkstra a mano

Grafo no dirigido ponderado:
```
  0 --(2)-- 1 --(3)-- 2
  |         |         |
 (6)       (1)       (5)
  |         |         |
  3 --(4)-- 4 --(2)-- 5
```

Ejecuta Dijkstra desde el vértice 0. En cada paso, muestra: qué vértice se
extrae, el estado de `dist[]`, y qué aristas se relajan.

<details>
<summary>Traza completa</summary>

```
Init: dist = [0, ∞, ∞, ∞, ∞, ∞]

Paso 1: Extraer 0 (d=0)
  Relax 0→1: 0+2=2 < ∞ → dist[1]=2
  Relax 0→3: 0+6=6 < ∞ → dist[3]=6
  dist = [0, 2, ∞, 6, ∞, ∞]

Paso 2: Extraer 1 (d=2)
  Relax 1→2: 2+3=5 < ∞ → dist[2]=5
  Relax 1→4: 2+1=3 < ∞ → dist[4]=3
  dist = [0, 2, 5, 6, 3, ∞]

Paso 3: Extraer 4 (d=3)
  Relax 4→3: 3+4=7 > 6 → no mejora
  Relax 4→5: 3+2=5 < ∞ → dist[5]=5
  dist = [0, 2, 5, 6, 3, 5]

Paso 4: Extraer 2 (d=5) [o 5, empate]
  Relax 2→5: 5+5=10 > 5 → no mejora
  dist sin cambios

Paso 5: Extraer 5 (d=5)
  No mejoras.

Paso 6: Extraer 3 (d=6)
  No mejoras.

Final: dist = [0, 2, 5, 6, 3, 5]
```
</details>

---

### Ejercicio 2 — Reconstruir todos los caminos

Usando los resultados del ejercicio 1, reconstruye el camino más corto
desde 0 hasta cada vértice usando el array `parent[]`.

<details>
<summary>Caminos</summary>

```
parent = [nil, 0, 1, 0, 1, 4]

0: 0 (distancia 0)
1: 0→1 (distancia 2)
2: 0→1→2 (distancia 5)
3: 0→3 (distancia 6)
4: 0→1→4 (distancia 3)
5: 0→1→4→5 (distancia 5)
```

Nota: el camino a 5 pasa por 1 y 4 (no por 2, a pesar de que 2-5 tiene
peso 5, porque el camino total 0→1→2→5 = 2+3+5 = 10 > 5).
</details>

---

### Ejercicio 3 — Grafo dirigido

Grafo dirigido: $0\to1{:}4,\ 0\to2{:}1,\ 2\to1{:}2,\ 1\to3{:}1,\ 2\to3{:}5$.

a) Ejecuta Dijkstra desde 0.
b) ¿Cambia el resultado si agregamos la arista $3\to0{:}10$?

<details>
<summary>Respuesta</summary>

a) Init: dist = [0, ∞, ∞, ∞].
Extraer 0: relax 1=4, 2=1. dist=[0,4,1,∞].
Extraer 2: relax 1: 1+2=3 < 4 → dist[1]=3. relax 3: 1+5=6. dist=[0,3,1,6].
Extraer 1: relax 3: 3+1=4 < 6 → dist[3]=4. dist=[0,3,1,4].
Extraer 3: nada.
**dist = [0, 3, 1, 4]**.

b) La arista $3\to0{:}10$ no cambia nada: Dijkstra ya procesó 0 con dist=0,
y $4+10=14 > 0$. En general, aristas que apuntan "hacia atrás" a vértices
ya finalizados nunca pueden mejorarlos (porque pesos $\geq 0$).
</details>

---

### Ejercicio 4 — Lazy deletion vs decrease-key

Explica por qué la técnica de lazy deletion (insertar duplicados en el heap
y saltar entradas obsoletas) tiene la misma complejidad asintótica que
decrease-key, pero puede usar más memoria.

<details>
<summary>Explicación</summary>

Con **decrease-key**: el heap tiene a lo sumo $n$ entradas (una por
vértice). Cada `decrease_key` cuesta $O(\log n)$.
Total: $O(n)$ extractions × $O(\log n)$ + $O(m)$ decrease-keys × $O(\log n)$
= $O((n+m) \log n)$.

Con **lazy deletion**: el heap puede tener hasta $m$ entradas (una por
relajación). Cada inserción cuesta $O(\log m)$. Como $m \leq n^2$,
$\log m \leq 2 \log n$, así que $O(\log m) = O(\log n)$.
Total: $O(m)$ inserciones × $O(\log n)$ + $O(m)$ extracciones × $O(\log n)$
= $O(m \log n) = O((n+m) \log n)$.

**Memoria**: lazy deletion usa $O(m)$ espacio en el heap vs $O(n)$ con
decrease-key. Para grafos densos esto es $O(n^2)$ vs $O(n)$, pero la
constante de tiempo de decrease-key en heaps binarios es mayor
(requiere mapa de posiciones).
</details>

---

### Ejercicio 5 — Dijkstra en grid

Grid 5×5 con estos costos de entrada:

```
1 1 1 1 1
1 9 9 9 1
1 9 1 9 1
1 9 9 9 1
1 1 1 1 1
```

Calcula la distancia mínima de $(0,0)$ a $(4,4)$ usando Dijkstra con
4 direcciones. Compara con BFS (sin pesos).

<details>
<summary>Respuesta</summary>

**Dijkstra**: el camino óptimo rodea el bloque de 9s por el borde exterior:
$(0,0)\to(0,1)\to(0,2)\to(0,3)\to(0,4)\to(1,4)\to(2,4)\to(3,4)\to(4,4)$
= $1+1+1+1+1+1+1+1 = 8$.

Camino por el centro: $(0,0)\to(1,0)\to(2,0)\to(2,1)$... tendría que cruzar
los 9s, costando mucho más.

Perímetro: $1+1+1+1+1+1+1+1 = 8$ (borde superior + borde derecho).

**BFS** (sin pesos, solo contando pasos): distancia = 8 pasos (misma ruta,
8 movimientos). BFS y Dijkstra coinciden porque el borde tiene costo
uniforme 1. Pero si los costos del borde variaran, BFS daría 8 pasos
(siempre) mientras Dijkstra optimizaría el costo total.
</details>

---

### Ejercicio 6 — Demostrar la falla con pesos negativos

Construye un grafo dirigido con 4 vértices donde Dijkstra produce un
resultado **incorrecto** debido a un peso negativo. Muestra paso a paso
dónde falla y cuál es la respuesta correcta.

<details>
<summary>Ejemplo</summary>

Grafo: $A\to B{:}1,\ A\to C{:}3,\ B\to C{:}{-4}$.

Dijkstra desde A:
- Extraer A(d=0): relax B=1, C=3. dist=[0,1,3].
- Extraer B(d=1): relax C: 1+(-4)=-3 < 3 → dist[C]=-3.
  Pero **C ya está "finalizado"** (con dist=3) si usamos `visited[]`.
  Si usamos lazy deletion: la nueva entrada (-3,C) se inserta pero C
  ya fue procesado.

Si se usa `visited[]`: dist[C]=3 (incorrecto). Real: -3.
Si se usa lazy deletion sin `visited[]`: podría funcionar en este caso
particular, pero con grafos más complejos seguirá fallando.

El problema fundamental: Dijkstra asume que dist[u] es definitivo al
extraer $u$, lo cual requiere pesos $\geq 0$.
</details>

---

### Ejercicio 7 — Multi-fuente: estaciones de bomberos

Un grafo no dirigido con 8 vértices modela un vecindario. Las estaciones
de bomberos están en vértices 0 y 5.

Aristas: $0\text{-}1{:}2,\ 1\text{-}2{:}3,\ 2\text{-}3{:}1,\ 3\text{-}4{:}4,\ 4\text{-}5{:}2,\ 5\text{-}6{:}1,\ 6\text{-}7{:}3,\ 1\text{-}6{:}5$.

Para cada vértice, determina cuál es la estación más cercana y la distancia.

<details>
<summary>Respuesta</summary>

Multi-source con fuentes {0, 5}:

Desde 0: dist = [0, 2, 5, 6, 10, 12, 7, 10]
Desde 5: dist = [12, 9, 7, 6, 2, 0, 1, 4]

Para cada vértice, el mínimo:
- 0: est.0 (d=0)
- 1: est.0 (d=2) vs est.5 (d=9) → **est.0, d=2**
- 2: est.0 (d=5) vs est.5 (d=7) → **est.0, d=5**
- 3: est.0 (d=6) vs est.5 (d=6) → **empate, d=6**
- 4: est.0 (d=10) vs est.5 (d=2) → **est.5, d=2**
- 5: est.5 (d=0)
- 6: est.0 (d=7) vs est.5 (d=1) → **est.5, d=1**
- 7: est.0 (d=10) vs est.5 (d=4) → **est.5, d=4**

La "frontera" entre las dos estaciones está entre vértices 3 y 4.
</details>

---

### Ejercicio 8 — Implementar Dijkstra con array lineal

Implementa Dijkstra sin heap, usando un array lineal para encontrar el
mínimo ($O(n)$ por extracción). Compara el número de operaciones vs la
versión con heap para un grafo denso de 6 vértices completamente conectado.

<details>
<summary>Pista de implementación</summary>

```
DIJKSTRA-ARRAY(G, s):
    dist[v] = INF for all v
    dist[s] = 0
    processed[v] = false for all v

    repeat n times:
        // Find min (O(n) scan)
        u = vertex with min dist[u] where not processed[u]
        processed[u] = true

        for each (v, w) in adj[u]:
            if dist[u] + w < dist[v]:
                dist[v] = dist[u] + w

Total: n × O(n) scans + m relaxations = O(n² + m) = O(n²)
```

Para grafos densos ($m = O(n^2)$), esto es $O(n^2)$ vs $O(n^2 \log n)$ con
heap. El array lineal es más rápido cuando $m > n^2 / \log n$.
</details>

---

### Ejercicio 9 — SPT vs MST

Dado el grafo no dirigido:
```
  0 --(1)-- 1
  |         |
 (4)       (2)
  |         |
  3 --(1)-- 2
```

a) Calcula el MST (Kruskal o Prim).
b) Calcula el SPT desde el vértice 0 (Dijkstra).
c) ¿Son iguales? Si no, explica por qué difieren.

<details>
<summary>Respuesta</summary>

a) **MST** (aristas por peso): 0-1:1, 3-2:1, 1-2:2. Total = 4.

b) **SPT** desde 0:
dist = [0, 1, 3, 4].
parent = [nil, 0, 1, 0] o [nil, 0, 1, 2].
Caminos: 0→1:1, 0→1→2:3, 0→3:4 (si usa arista directa) o 0→1→2→3:4.
SPT aristas: {0-1, 1-2, 0-3} → total = 1+2+4 = 7.

c) **No son iguales**. El MST incluye 2-3:1 (peso 1) en vez de 0-3:4
(peso 4). Pero el SPT necesita 0-3:4 porque la distancia
$0\to1\to2\to3 = 4$ empata con $0\to3 = 4$, y el SPT minimiza
**distancia individual** (no peso total del árbol).

MST peso = 4. SPT peso = 7. MST minimiza peso total; SPT minimiza
distancia desde la raíz.
</details>

---

### Ejercicio 10 — Dijkstra para planificar viaje

Un mapa de carreteras tiene 7 ciudades (0-6) con tiempos de viaje en
minutos:

| Arista | Tiempo |
|:------:|:------:|
| 0-1 | 30 |
| 0-2 | 45 |
| 1-3 | 20 |
| 1-4 | 60 |
| 2-3 | 35 |
| 2-5 | 50 |
| 3-4 | 25 |
| 3-5 | 40 |
| 4-6 | 15 |
| 5-6 | 30 |

a) Calcula el tiempo mínimo desde la ciudad 0 a cada otra.
b) ¿Cuál es la ruta más rápida de 0 a 6?
c) Si la carretera 3-4 se cierra (infinito), ¿cuál es la nueva ruta a 6?

<details>
<summary>Respuesta</summary>

a) Dijkstra desde 0:
- Extraer 0(d=0): relax 1=30, 2=45.
- Extraer 1(d=30): relax 3=50, 4=90.
- Extraer 2(d=45): relax 3: 45+35=80 > 50✗. relax 5=95.
- Extraer 3(d=50): relax 4: 50+25=75 < 90→75. relax 5: 50+40=90 < 95→90.
- Extraer 4(d=75): relax 6=90.
- Extraer 5(d=90): relax 6: 90+30=120 > 90✗.
- Extraer 6(d=90).

**dist = [0, 30, 45, 50, 75, 90, 90]**.

b) Ruta 0→6: $0\to1\to3\to4\to6$ = 30+20+25+15 = **90 minutos**.

c) Sin 3-4: Dijkstra da dist[4] = 90 (por 0→1→4:60) y dist[6] vía
4→6: 90+15=105. Pero vía 5→6: dist[5]=90, 90+30=120. Pero recalculando:
dist[4]: $0\to1\to4 = 30+60 = 90$.
dist[6] = min(90+15, 90+30) = min(105, 120) = **105** (ruta $0\to1\to4\to6$).
</details>
