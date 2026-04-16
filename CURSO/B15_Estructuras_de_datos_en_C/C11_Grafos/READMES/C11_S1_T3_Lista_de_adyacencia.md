# T03 — Lista de adyacencia

## Objetivo

Comprender la representación de grafos mediante **listas de adyacencia**:
por qué es la representación preferida para grafos dispersos, cómo
implementarla en C (con listas enlazadas y con arrays dinámicos), analizar
sus costos y compararla con la matriz de adyacencia.

---

## 1. Concepto

Dado un grafo $G = (V, E)$ con $n$ vértices, la **lista de adyacencia**
almacena, para cada vértice $v$, una lista (o array) con todos los vecinos
de $v$.

```
Grafo:                  Lista de adyacencia:
  0 --- 1               0: [1, 2]
  |   / |               1: [0, 2, 3]
  2 --- 3               2: [0, 1, 3]
                         3: [1, 2]
```

Para un grafo **dirigido**, la lista de $v$ contiene solo los vértices
alcanzables desde $v$ (destinos de las aristas que salen de $v$):

```
Grafo:                  Lista de adyacencia:
  0 → 1                 0: [1, 2]
  ↓ ↗ ↓                 1: [3]
  2   3                 2: [1]
                         3: []
```

Para un grafo **ponderado**, cada entrada almacena tanto el destino como el
peso:

```
  0 --5-- 1              0: [(1, 5), (2, 2)]
  |       |              1: [(0, 5), (3, 3)]
  2       3              2: [(0, 2), (3, 7)]
  |       |              3: [(1, 3), (2, 7)]
  2 --7-- 3
```

---

## 2. Espacio

El espacio total es $O(n + m)$ para un grafo dirigido y $O(n + 2m)$ para uno
no dirigido (cada arista aparece en dos listas). Comparado con la matriz de
adyacencia ($O(n^2)$), la diferencia es dramática para grafos dispersos:

| $n$ | $m$ | Densidad | Matriz ($n^2$ ints) | Lista ($n + 2m$ entries) |
|-----|-----|:--------:|:-------------------:|:------------------------:|
| 1,000 | 3,000 | 0.006 | 4 MB | ~100 KB |
| 10,000 | 30,000 | 0.0006 | 400 MB | ~1 MB |
| 100,000 | 300,000 | 0.00006 | 40 GB | ~10 MB |
| 1,000,000 | 5,000,000 | 0.00001 | 4 TB | ~160 MB |

En la última fila, la matriz ni siquiera cabe en un servidor típico. La
lista cabe en la RAM de un laptop.

---

## 3. Implementación en C — listas enlazadas

La forma clásica usa un **array de punteros** a listas enlazadas:

```c
typedef struct AdjNode {
    int dest;
    int weight;
    struct AdjNode *next;
} AdjNode;

typedef struct {
    AdjNode **lists;   // array of n head pointers
    int n;
    bool directed;
} Graph;
```

Cada vértice $i$ tiene un puntero `lists[i]` que apunta al primer nodo de
su lista de vecinos. Agregar un vecino es insertar al principio de la lista
($O(1)$).

### Creación y destrucción

```c
Graph graph_create(int n, bool directed) {
    Graph g;
    g.n = n;
    g.directed = directed;
    g.lists = calloc(n, sizeof(AdjNode *)); // all NULL
    return g;
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++) {
        AdjNode *curr = g->lists[i];
        while (curr) {
            AdjNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    free(g->lists);
    g->lists = NULL;
}
```

### Agregar arista

```c
void graph_add_edge(Graph *g, int u, int v, int weight) {
    // Insert at head of u's list
    AdjNode *node = malloc(sizeof(AdjNode));
    node->dest = v;
    node->weight = weight;
    node->next = g->lists[u];
    g->lists[u] = node;

    if (!g->directed) {
        // Also add v → u
        node = malloc(sizeof(AdjNode));
        node->dest = u;
        node->weight = weight;
        node->next = g->lists[v];
        g->lists[v] = node;
    }
}
```

Insertar al principio es $O(1)$, pero el orden de los vecinos queda en
orden inverso de inserción. Si el orden importa, se puede insertar al final
(manteniendo un puntero `tail` por lista) o insertar ordenado ($O(\deg(v))$
por inserción).

### Buscar arista

```c
bool graph_has_edge(const Graph *g, int u, int v) {
    for (AdjNode *curr = g->lists[u]; curr; curr = curr->next) {
        if (curr->dest == v)
            return true;
    }
    return false;
}
```

Complejidad: $O(\deg(u))$ — debe recorrer la lista de $u$ hasta encontrar
$v$ o llegar al final. Para grafos dispersos, $\deg(u)$ es típicamente
pequeño (constante o $O(\sqrt{m})$), pero en el peor caso puede ser $O(n)$.

### Eliminar arista

```c
void graph_remove_edge(Graph *g, int u, int v) {
    // Remove v from u's list
    AdjNode **ptr = &g->lists[u];
    while (*ptr) {
        if ((*ptr)->dest == v) {
            AdjNode *tmp = *ptr;
            *ptr = (*ptr)->next;
            free(tmp);
            break;
        }
        ptr = &(*ptr)->next;
    }

    if (!g->directed) {
        // Also remove u from v's list
        ptr = &g->lists[v];
        while (*ptr) {
            if ((*ptr)->dest == u) {
                AdjNode *tmp = *ptr;
                *ptr = (*ptr)->next;
                free(tmp);
                break;
            }
            ptr = &(*ptr)->next;
        }
    }
}
```

La técnica del **doble puntero** (`AdjNode **ptr`) simplifica la eliminación
al evitar el caso especial del primer nodo. Esta misma técnica se vio en
C05 (listas enlazadas).

---

## 4. Implementación en C — arrays dinámicos

Una alternativa más cache-friendly es usar **arrays dinámicos** (similar a
`Vec` en Rust) en lugar de listas enlazadas:

```c
typedef struct {
    int *dests;     // dynamic array of destinations
    int *weights;   // parallel array of weights
    int size;
    int capacity;
} NeighborList;

typedef struct {
    NeighborList *adj;   // array of n NeighborLists
    int n;
    bool directed;
} GraphVec;
```

### Ventajas sobre listas enlazadas

| Aspecto | Lista enlazada | Array dinámico |
|---------|:--------------:|:--------------:|
| Localidad de caché | Mala (nodos dispersos en heap) | Buena (contiguo) |
| Overhead por entrada | ~16-24 bytes (dato + puntero + padding) | ~4-8 bytes (solo dato) |
| Inserción | $O(1)$ al principio | $O(1)$ amortizado al final |
| Recorrer vecinos | Cache misses por saltos | Secuencial, prefetch-friendly |
| Eliminación | $O(\deg)$ con punteros | $O(\deg)$ con shift (o swap-remove) |

Para BFS y DFS, donde se recorren **todos** los vecinos secuencialmente,
el array dinámico es significativamente más rápido en la práctica.

### Swap-remove

Para eliminar un elemento de un array sin importar el orden, se puede usar
**swap-remove**: intercambiar el elemento con el último y reducir el tamaño.
Esto es $O(1)$ en lugar de $O(\deg)$:

```c
void neighbor_swap_remove(NeighborList *nl, int idx) {
    nl->size--;
    nl->dests[idx] = nl->dests[nl->size];
    nl->weights[idx] = nl->weights[nl->size];
}
```

Rust tiene `Vec::swap_remove()` exactamente para esto.

---

## 5. Operaciones y complejidad

| Operación | Lista de adyacencia | Matriz de adyacencia |
|-----------|:-------------------:|:--------------------:|
| Espacio | $O(n + m)$ | $O(n^2)$ |
| ¿Existe arista $(u,v)$? | $O(\deg(u))$ | $O(1)$ |
| Agregar arista | $O(1)$ | $O(1)$ |
| Eliminar arista | $O(\deg(u))$ | $O(1)$ |
| Listar vecinos de $u$ | $O(\deg(u))$ | $O(n)$ |
| Grado de $u$ | $O(1)$ si se almacena size | $O(n)$ |
| Recorrer todas las aristas | $O(n + m)$ | $O(n^2)$ |

La ventaja decisiva de la lista es **listar vecinos**: $O(\deg(u))$ vs $O(n)$.
Como BFS y DFS listan vecinos de cada vértice visitado, su complejidad total
es $O(n + m)$ con lista vs $O(n^2)$ con matriz.

Para un grafo disperso con $m = O(n)$, esto es $O(n)$ vs $O(n^2)$ — una
diferencia de un factor de $n$ que hace que la matriz sea inutilizable para
grafos grandes.

---

## 6. Variante: lista de aristas

Una tercera representación, más simple pero menos eficiente, es la **lista
de aristas**: simplemente almacenar todas las aristas en un array:

```c
typedef struct {
    int u, v;
    int weight;
} Edge;

typedef struct {
    Edge *edges;
    int m;        // number of edges
    int n;        // number of vertices
    int capacity;
} EdgeList;
```

| Operación | Lista de aristas |
|-----------|:----------------:|
| Espacio | $O(m)$ |
| ¿Existe arista? | $O(m)$ |
| Listar vecinos de $u$ | $O(m)$ |
| Agregar arista | $O(1)$ |
| Ordenar aristas por peso | $O(m \log m)$ |

La lista de aristas es ineficiente para la mayoría de operaciones, pero es
ideal para el algoritmo de **Kruskal** (S04/T02), que necesita ordenar todas
las aristas por peso y luego procesarlas secuencialmente. También es útil
para leer grafos desde archivos (formato de entrada típico: una arista por
línea).

---

## 7. In-degree en grafos dirigidos

Con lista de adyacencia, el out-degree de $v$ es simplemente el tamaño de
su lista. Pero el **in-degree** requiere recorrer todas las listas de
todos los vértices — $O(n + m)$ — porque no hay una lista de "quién apunta
a $v$".

Soluciones:

1. **Array auxiliar de in-degrees**: calcular una sola vez en $O(n + m)$ y
   almacenar en un array `in_deg[n]`.
2. **Grafo transpuesto**: construir $G^T$ (invertir todas las aristas).
   Ahora `G_T.lists[v]` contiene quién apunta a $v$ en $G$.
3. **Doble lista**: cada vértice mantiene tanto `out_neighbors` como
   `in_neighbors`, a costa de duplicar el espacio de aristas.

El algoritmo de Kahn para ordenamiento topológico (S04/T04) necesita
in-degrees, y típicamente usa la solución 1.

---

## 8. Lectura de grafos desde archivo

Un formato de entrada común es:

```
5 6         ← n m (vértices, aristas)
0 1 5       ← u v weight (una arista por línea)
0 2 2
1 2 8
1 3 3
2 3 7
3 4 1
```

```c
Graph graph_from_file(const char *filename, bool directed) {
    FILE *f = fopen(filename, "r");
    int n, m;
    fscanf(f, "%d %d", &n, &m);

    Graph g = graph_create(n, directed);
    for (int i = 0; i < m; i++) {
        int u, v, w;
        fscanf(f, "%d %d %d", &u, &v, &w);
        graph_add_edge(&g, u, v, w);
    }
    fclose(f);
    return g;
}
```

En Rust, el mismo patrón:

```rust
fn graph_from_str(input: &str, directed: bool) -> Vec<Vec<(usize, i32)>> {
    let mut lines = input.lines();
    let first: Vec<usize> = lines.next().unwrap()
        .split_whitespace()
        .map(|s| s.parse().unwrap())
        .collect();
    let (n, _m) = (first[0], first[1]);

    let mut adj = vec![vec![]; n];
    for line in lines {
        let parts: Vec<i32> = line.split_whitespace()
            .map(|s| s.parse().unwrap())
            .collect();
        let (u, v, w) = (parts[0] as usize, parts[1] as usize, parts[2]);
        adj[u].push((v, w));
        if !directed {
            adj[v].push((u, w));
        }
    }
    adj
}
```

---

## 9. Lista de adyacencia en competencias

En programación competitiva, se usa frecuentemente un `Vec<Vec<(usize, W)>>`
sin struct envolvente, porque minimiza el boilerplate:

```rust
let n = 5;
let mut adj: Vec<Vec<(usize, i32)>> = vec![vec![]; n];

// Add edge 0-1 weight 5
adj[0].push((1, 5));
adj[1].push((0, 5));

// BFS/DFS just iterates adj[u]
for &(v, w) in &adj[u] {
    // process neighbor v with weight w
}
```

En C, el equivalente mínimo es un array de `struct` con tamaño fijo:

```c
#define MAXN 100001
#define MAXM 200001

struct { int to, w, next; } edge[MAXM * 2];
int head[MAXN], edge_cnt;

void add_edge(int u, int v, int w) {
    edge[edge_cnt] = (typeof(edge[0])){v, w, head[u]};
    head[u] = edge_cnt++;
}

// Iterate neighbors of u:
// for (int e = head[u]; e != -1; e = edge[e].next)
//     process(edge[e].to, edge[e].w);
```

Esta técnica de **linked list con array estático** (a veces llamada
"forward star" o "chain forward star") evita `malloc` completamente, lo que
es crítico en competencias por rendimiento. El campo `next` actúa como
puntero al siguiente elemento de la lista, pero usando índices en un array
en lugar de punteros reales.

---

## 10. Programa completo en C

```c
/*
 * adj_list.c
 *
 * Adjacency list implementations (linked list and dynamic array)
 * with demonstrations of key operations.
 *
 * Compile: gcc -O2 -o adj_list adj_list.c
 * Run:     ./adj_list
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/* ============== LINKED LIST ADJACENCY LIST ======================== */

typedef struct AdjNode {
    int dest;
    int weight;
    struct AdjNode *next;
} AdjNode;

typedef struct {
    AdjNode **lists;
    int n;
    int m;          /* edge count */
    bool directed;
    char *labels;
} GraphLL;

GraphLL gll_create(int n, bool directed, const char *labels) {
    GraphLL g;
    g.n = n;
    g.m = 0;
    g.directed = directed;
    g.lists = calloc(n, sizeof(AdjNode *));
    g.labels = malloc(n);
    for (int i = 0; i < n; i++)
        g.labels[i] = labels ? labels[i] : (char)('0' + i);
    return g;
}

void gll_free(GraphLL *g) {
    for (int i = 0; i < g->n; i++) {
        AdjNode *curr = g->lists[i];
        while (curr) {
            AdjNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    free(g->lists);
    free(g->labels);
}

void gll_add_edge(GraphLL *g, int u, int v, int w) {
    AdjNode *node = malloc(sizeof(AdjNode));
    node->dest = v;
    node->weight = w;
    node->next = g->lists[u];
    g->lists[u] = node;

    if (!g->directed) {
        node = malloc(sizeof(AdjNode));
        node->dest = u;
        node->weight = w;
        node->next = g->lists[v];
        g->lists[v] = node;
    }
    g->m++;
}

bool gll_has_edge(const GraphLL *g, int u, int v) {
    for (AdjNode *curr = g->lists[u]; curr; curr = curr->next)
        if (curr->dest == v) return true;
    return false;
}

void gll_remove_edge(GraphLL *g, int u, int v) {
    AdjNode **ptr = &g->lists[u];
    while (*ptr) {
        if ((*ptr)->dest == v) {
            AdjNode *tmp = *ptr;
            *ptr = (*ptr)->next;
            free(tmp);
            break;
        }
        ptr = &(*ptr)->next;
    }
    if (!g->directed) {
        ptr = &g->lists[v];
        while (*ptr) {
            if ((*ptr)->dest == u) {
                AdjNode *tmp = *ptr;
                *ptr = (*ptr)->next;
                free(tmp);
                break;
            }
            ptr = &(*ptr)->next;
        }
    }
    g->m--;
}

int gll_degree(const GraphLL *g, int v) {
    int deg = 0;
    for (AdjNode *curr = g->lists[v]; curr; curr = curr->next)
        deg++;
    return deg;
}

void gll_print(const GraphLL *g) {
    for (int i = 0; i < g->n; i++) {
        printf("  %c →", g->labels[i]);
        for (AdjNode *curr = g->lists[i]; curr; curr = curr->next) {
            if (curr->weight != 1)
                printf(" %c(%d)", g->labels[curr->dest], curr->weight);
            else
                printf(" %c", g->labels[curr->dest]);
        }
        printf("\n");
    }
}

/* ============== DYNAMIC ARRAY ADJACENCY LIST ====================== */

typedef struct {
    int *dests;
    int *weights;
    int size;
    int capacity;
} NbrArray;

typedef struct {
    NbrArray *adj;
    int n;
    int m;
    bool directed;
    char *labels;
} GraphVec;

static void nbr_init(NbrArray *na) {
    na->dests = NULL;
    na->weights = NULL;
    na->size = 0;
    na->capacity = 0;
}

static void nbr_push(NbrArray *na, int dest, int weight) {
    if (na->size == na->capacity) {
        na->capacity = na->capacity ? na->capacity * 2 : 4;
        na->dests = realloc(na->dests, na->capacity * sizeof(int));
        na->weights = realloc(na->weights, na->capacity * sizeof(int));
    }
    na->dests[na->size] = dest;
    na->weights[na->size] = weight;
    na->size++;
}

static void nbr_swap_remove(NbrArray *na, int idx) {
    na->size--;
    na->dests[idx] = na->dests[na->size];
    na->weights[idx] = na->weights[na->size];
}

static void nbr_free(NbrArray *na) {
    free(na->dests);
    free(na->weights);
    na->dests = na->weights = NULL;
    na->size = na->capacity = 0;
}

GraphVec gv_create(int n, bool directed, const char *labels) {
    GraphVec g;
    g.n = n;
    g.m = 0;
    g.directed = directed;
    g.adj = malloc(n * sizeof(NbrArray));
    g.labels = malloc(n);
    for (int i = 0; i < n; i++) {
        nbr_init(&g.adj[i]);
        g.labels[i] = labels ? labels[i] : (char)('0' + i);
    }
    return g;
}

void gv_free(GraphVec *g) {
    for (int i = 0; i < g->n; i++)
        nbr_free(&g->adj[i]);
    free(g->adj);
    free(g->labels);
}

void gv_add_edge(GraphVec *g, int u, int v, int w) {
    nbr_push(&g->adj[u], v, w);
    if (!g->directed)
        nbr_push(&g->adj[v], u, w);
    g->m++;
}

void gv_print(const GraphVec *g) {
    for (int i = 0; i < g->n; i++) {
        printf("  %c →", g->labels[i]);
        for (int j = 0; j < g->adj[i].size; j++) {
            int d = g->adj[i].dests[j];
            int w = g->adj[i].weights[j];
            if (w != 1)
                printf(" %c(%d)", g->labels[d], w);
            else
                printf(" %c", g->labels[d]);
        }
        printf("\n");
    }
}

/* =========================== DEMOS ================================ */

/*
 * Demo 1: Build and display — linked list version
 */
void demo_linked_list(void) {
    printf("=== Demo 1: Linked List Adjacency List ===\n\n");

    GraphLL g = gll_create(6, false, "ABCDEF");
    gll_add_edge(&g, 0, 1, 1);  /* A-B */
    gll_add_edge(&g, 0, 2, 1);  /* A-C */
    gll_add_edge(&g, 1, 3, 1);  /* B-D */
    gll_add_edge(&g, 1, 4, 1);  /* B-E */
    gll_add_edge(&g, 2, 4, 1);  /* C-E */
    gll_add_edge(&g, 3, 5, 1);  /* D-F */
    gll_add_edge(&g, 4, 5, 1);  /* E-F */

    printf("Undirected graph (linked list):\n");
    gll_print(&g);

    printf("\nDegrees:\n");
    for (int i = 0; i < g.n; i++)
        printf("  deg(%c) = %d\n", g.labels[i], gll_degree(&g, i));

    printf("\nEdge queries:\n");
    printf("  A-B? %s\n", gll_has_edge(&g, 0, 1) ? "YES" : "NO");
    printf("  A-F? %s\n", gll_has_edge(&g, 0, 5) ? "YES" : "NO");

    printf("\nRemove edge B-D:\n");
    gll_remove_edge(&g, 1, 3);
    gll_print(&g);
    printf("  Edges: %d\n\n", g.m);

    gll_free(&g);
}

/*
 * Demo 2: Dynamic array version
 */
void demo_dynamic_array(void) {
    printf("=== Demo 2: Dynamic Array Adjacency List ===\n\n");

    GraphVec g = gv_create(5, false, "ABCDE");
    gv_add_edge(&g, 0, 1, 3);  /* A-B: 3 */
    gv_add_edge(&g, 0, 2, 7);  /* A-C: 7 */
    gv_add_edge(&g, 1, 2, 1);  /* B-C: 1 */
    gv_add_edge(&g, 1, 3, 5);  /* B-D: 5 */
    gv_add_edge(&g, 2, 4, 2);  /* C-E: 2 */
    gv_add_edge(&g, 3, 4, 4);  /* D-E: 4 */

    printf("Weighted undirected graph (dynamic array):\n");
    gv_print(&g);

    printf("\nCapacities (showing growth strategy):\n");
    for (int i = 0; i < g.n; i++)
        printf("  %c: size=%d, capacity=%d\n",
               g.labels[i], g.adj[i].size, g.adj[i].capacity);

    /* Swap-remove demo */
    printf("\nSwap-remove: removing first neighbor of B (index 0):\n");
    printf("  Before: B →");
    for (int j = 0; j < g.adj[1].size; j++)
        printf(" %c(%d)", g.labels[g.adj[1].dests[j]], g.adj[1].weights[j]);
    printf("\n");

    nbr_swap_remove(&g.adj[1], 0);
    printf("  After:  B →");
    for (int j = 0; j < g.adj[1].size; j++)
        printf(" %c(%d)", g.labels[g.adj[1].dests[j]], g.adj[1].weights[j]);
    printf("\n  (last element moved to index 0 — order changed)\n\n");

    gv_free(&g);
}

/*
 * Demo 3: Directed graph — in/out degree
 */
void demo_directed(void) {
    printf("=== Demo 3: Directed Graph ===\n\n");

    GraphLL g = gll_create(5, true, "ABCDE");
    gll_add_edge(&g, 0, 1, 1);  /* A→B */
    gll_add_edge(&g, 0, 2, 1);  /* A→C */
    gll_add_edge(&g, 1, 3, 1);  /* B→D */
    gll_add_edge(&g, 2, 1, 1);  /* C→B */
    gll_add_edge(&g, 2, 3, 1);  /* C→D */
    gll_add_edge(&g, 3, 4, 1);  /* D→E */

    printf("Directed graph:\n");
    gll_print(&g);

    /* Compute in-degrees (requires scanning all lists) */
    int in_deg[5] = {0};
    for (int i = 0; i < g.n; i++)
        for (AdjNode *curr = g.lists[i]; curr; curr = curr->next)
            in_deg[curr->dest]++;

    printf("\n%-6s  %-10s  %-10s  %s\n", "Vertex", "out-deg", "in-deg", "Role");
    for (int i = 0; i < g.n; i++) {
        const char *role = "";
        int out = gll_degree(&g, i);
        if (in_deg[i] == 0) role = "source";
        if (out == 0) role = "sink";
        printf("%-6c  %-10d  %-10d  %s\n",
               g.labels[i], out, in_deg[i], role);
    }
    printf("\nNote: in-degree required O(n+m) scan of all lists.\n\n");

    gll_free(&g);
}

/*
 * Demo 4: BFS using adjacency list — O(n+m)
 */
void demo_bfs(void) {
    printf("=== Demo 4: BFS on Adjacency List ===\n\n");

    GraphLL g = gll_create(8, false, "ABCDEFGH");
    gll_add_edge(&g, 0, 1, 1);  /* A-B */
    gll_add_edge(&g, 0, 2, 1);  /* A-C */
    gll_add_edge(&g, 1, 3, 1);  /* B-D */
    gll_add_edge(&g, 1, 4, 1);  /* B-E */
    gll_add_edge(&g, 2, 5, 1);  /* C-F */
    gll_add_edge(&g, 3, 6, 1);  /* D-G */
    gll_add_edge(&g, 4, 6, 1);  /* E-G */
    gll_add_edge(&g, 5, 7, 1);  /* F-H */
    gll_add_edge(&g, 6, 7, 1);  /* G-H */

    printf("Graph:\n");
    gll_print(&g);

    /* BFS from A */
    bool visited[8] = {false};
    int dist[8], parent[8];
    for (int i = 0; i < 8; i++) { dist[i] = -1; parent[i] = -1; }

    int queue[8], front = 0, back = 0;
    visited[0] = true;
    dist[0] = 0;
    queue[back++] = 0;

    int ops = 0;  /* count operations */

    printf("\nBFS from A:\n");
    while (front < back) {
        int u = queue[front++];
        printf("  Visit %c (dist=%d): neighbors →",
               g.labels[u], dist[u]);

        /* O(deg(u)) to iterate neighbors — the key advantage */
        for (AdjNode *curr = g.lists[u]; curr; curr = curr->next) {
            ops++;
            printf(" %c", g.labels[curr->dest]);
            if (!visited[curr->dest]) {
                visited[curr->dest] = true;
                dist[curr->dest] = dist[u] + 1;
                parent[curr->dest] = u;
                queue[back++] = curr->dest;
            }
        }
        printf("\n");
    }

    printf("\nDistances from A:\n");
    for (int i = 0; i < g.n; i++)
        printf("  %c: dist=%d\n", g.labels[i], dist[i]);

    printf("\nTotal neighbor-checks: %d (= 2*|E| = %d)\n", ops, 2 * g.m);
    printf("With matrix, would be: n*n = %d checks\n\n", g.n * g.n);

    gll_free(&g);
}

/*
 * Demo 5: Build transpose of directed graph
 */
void demo_transpose(void) {
    printf("=== Demo 5: Graph Transpose ===\n\n");

    GraphLL g = gll_create(4, true, "ABCD");
    gll_add_edge(&g, 0, 1, 1);  /* A→B */
    gll_add_edge(&g, 0, 2, 1);  /* A→C */
    gll_add_edge(&g, 1, 2, 1);  /* B→C */
    gll_add_edge(&g, 2, 3, 1);  /* C→D */

    printf("Original graph G:\n");
    gll_print(&g);

    /* Build transpose */
    GraphLL gt = gll_create(4, true, "ABCD");
    for (int u = 0; u < g.n; u++) {
        for (AdjNode *curr = g.lists[u]; curr; curr = curr->next) {
            gll_add_edge(&gt, curr->dest, u, curr->weight);
        }
    }

    printf("\nTranspose G^T (all edges reversed):\n");
    gll_print(&gt);

    printf("\nVerification:\n");
    printf("  G has A→B? %s, G^T has B→A? %s\n",
           gll_has_edge(&g, 0, 1) ? "YES" : "NO",
           gll_has_edge(&gt, 1, 0) ? "YES" : "NO");
    printf("  G has C→D? %s, G^T has D→C? %s\n\n",
           gll_has_edge(&g, 2, 3) ? "YES" : "NO",
           gll_has_edge(&gt, 3, 2) ? "YES" : "NO");

    gll_free(&g);
    gll_free(&gt);
}

/*
 * Demo 6: Memory overhead comparison
 */
void demo_memory_overhead(void) {
    printf("=== Demo 6: Memory Overhead — Linked vs Array ===\n\n");

    printf("Per-neighbor overhead:\n");
    printf("  Linked list node: %zu bytes (dest + weight + next ptr + padding)\n",
           sizeof(AdjNode));
    printf("  Array entry:      %zu bytes (dest + weight, no pointer)\n",
           2 * sizeof(int));

    int n = 1000, m = 5000;
    long ll_bytes = (long)n * sizeof(AdjNode *) +
                    2L * m * sizeof(AdjNode);  /* undirected: 2m nodes */
    long arr_bytes = (long)n * (sizeof(NbrArray)) +
                     2L * m * 2 * sizeof(int);  /* 2 arrays per neighbor */

    printf("\nFor graph with n=%d, m=%d (undirected):\n", n, m);
    printf("  Linked list: ~%ld bytes (%.1f KB)\n", ll_bytes, ll_bytes / 1024.0);
    printf("  Dyn. array:  ~%ld bytes (%.1f KB)\n", arr_bytes, arr_bytes / 1024.0);
    printf("  Matrix:      ~%ld bytes (%.1f KB)\n",
           (long)n * n * sizeof(int),
           (long)n * n * sizeof(int) / 1024.0);
    printf("  Savings vs matrix: %.0fx (linked), %.0fx (array)\n\n",
           (double)(n * n * sizeof(int)) / ll_bytes,
           (double)(n * n * sizeof(int)) / arr_bytes);
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Adjacency List — C Demonstrations                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_linked_list();
    demo_dynamic_array();
    demo_directed();
    demo_bfs();
    demo_transpose();
    demo_memory_overhead();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Adjacency list representation fully demonstrated!  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 11. Programa completo en Rust

```rust
/*
 * adj_list.rs
 *
 * Adjacency list implementations in Rust with demonstrations.
 *
 * Run: cargo run --release
 */

use std::collections::VecDeque;

// ── Graph with Vec<Vec<(usize, i32)>> — the idiomatic Rust way ──────

struct Graph {
    adj: Vec<Vec<(usize, i32)>>,  // (destination, weight)
    directed: bool,
    labels: Vec<char>,
}

impl Graph {
    fn new(labels: &[char], directed: bool) -> Self {
        let n = labels.len();
        Graph {
            adj: vec![vec![]; n],
            directed,
            labels: labels.to_vec(),
        }
    }

    fn n(&self) -> usize {
        self.adj.len()
    }

    fn add_edge(&mut self, u: usize, v: usize, w: i32) {
        self.adj[u].push((v, w));
        if !self.directed {
            self.adj[v].push((u, w));
        }
    }

    fn has_edge(&self, u: usize, v: usize) -> bool {
        self.adj[u].iter().any(|&(dest, _)| dest == v)
    }

    fn remove_edge(&mut self, u: usize, v: usize) {
        if let Some(pos) = self.adj[u].iter().position(|&(d, _)| d == v) {
            self.adj[u].swap_remove(pos);
        }
        if !self.directed {
            if let Some(pos) = self.adj[v].iter().position(|&(d, _)| d == u) {
                self.adj[v].swap_remove(pos);
            }
        }
    }

    fn degree(&self, v: usize) -> usize {
        self.adj[v].len()
    }

    fn edge_count(&self) -> usize {
        let total: usize = self.adj.iter().map(|v| v.len()).sum();
        if self.directed { total } else { total / 2 }
    }

    fn transpose(&self) -> Graph {
        assert!(self.directed);
        let mut gt = Graph::new(&self.labels, true);
        for (u, neighbors) in self.adj.iter().enumerate() {
            for &(v, w) in neighbors {
                gt.adj[v].push((u, w));
            }
        }
        gt
    }

    fn print(&self) {
        for (i, neighbors) in self.adj.iter().enumerate() {
            print!("  {} →", self.labels[i]);
            for &(dest, w) in neighbors {
                if w != 1 {
                    print!(" {}({})", self.labels[dest], w);
                } else {
                    print!(" {}", self.labels[dest]);
                }
            }
            println!();
        }
    }
}

// ── Demo 1: Basic operations ─────────────────────────────────────────

fn demo_basic() {
    println!("=== Demo 1: Basic Adjacency List Operations ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E', 'F'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(1, 3, 1); // B-D
    g.add_edge(1, 4, 1); // B-E
    g.add_edge(2, 4, 1); // C-E
    g.add_edge(3, 5, 1); // D-F
    g.add_edge(4, 5, 1); // E-F

    println!("Undirected graph:");
    g.print();

    println!("\nDegrees:");
    for i in 0..g.n() {
        println!("  deg({}) = {}", g.labels[i], g.degree(i));
    }

    println!("\nEdge queries:");
    println!("  A-B? {}", g.has_edge(0, 1));
    println!("  A-F? {}", g.has_edge(0, 5));

    println!("\nRemove A-C:");
    g.remove_edge(0, 2);
    g.print();
    println!("  Edges: {}\n", g.edge_count());
}

// ── Demo 2: Weighted graph ───────────────────────────────────────────

fn demo_weighted() {
    println!("=== Demo 2: Weighted Graph ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], false);
    g.add_edge(0, 1, 5); // A-B: 5
    g.add_edge(0, 2, 2); // A-C: 2
    g.add_edge(1, 3, 3); // B-D: 3
    g.add_edge(2, 3, 7); // C-D: 7
    g.add_edge(3, 4, 1); // D-E: 1

    println!("Weighted undirected graph:");
    g.print();

    // Show neighbors with weights
    println!("\nNeighbors (functional style):");
    for i in 0..g.n() {
        let nbrs: Vec<String> = g.adj[i]
            .iter()
            .map(|&(d, w)| format!("{}(w={})", g.labels[d], w))
            .collect();
        println!("  {}: [{}]", g.labels[i], nbrs.join(", "));
    }

    // Memory usage
    println!("\nMemory layout:");
    for i in 0..g.n() {
        println!(
            "  {}: len={}, capacity={}, bytes={}",
            g.labels[i],
            g.adj[i].len(),
            g.adj[i].capacity(),
            g.adj[i].capacity() * std::mem::size_of::<(usize, i32)>()
        );
    }
    println!();
}

// ── Demo 3: Directed graph with in-degree calculation ────────────────

fn demo_directed() {
    println!("=== Demo 3: Directed Graph — In/Out Degree ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], true);
    g.add_edge(0, 1, 1); // A→B
    g.add_edge(0, 2, 1); // A→C
    g.add_edge(1, 3, 1); // B→D
    g.add_edge(2, 1, 1); // C→B
    g.add_edge(2, 3, 1); // C→D
    g.add_edge(3, 4, 1); // D→E

    println!("Directed graph:");
    g.print();

    // Compute in-degrees: O(n + m)
    let mut in_deg = vec![0usize; g.n()];
    for neighbors in &g.adj {
        for &(dest, _) in neighbors {
            in_deg[dest] += 1;
        }
    }

    println!("\n{:<8} {:<10} {:<10} Role", "Vertex", "out-deg", "in-deg");
    for i in 0..g.n() {
        let out = g.degree(i);
        let role = match (in_deg[i], out) {
            (0, _) => "source",
            (_, 0) => "sink",
            _ => "",
        };
        println!("{:<8} {:<10} {:<10} {}", g.labels[i], out, in_deg[i], role);
    }
    println!();
}

// ── Demo 4: BFS with operation counting ──────────────────────────────

fn demo_bfs() {
    println!("=== Demo 4: BFS on Adjacency List — O(n+m) ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(1, 3, 1); // B-D
    g.add_edge(1, 4, 1); // B-E
    g.add_edge(2, 5, 1); // C-F
    g.add_edge(3, 6, 1); // D-G
    g.add_edge(4, 6, 1); // E-G
    g.add_edge(5, 7, 1); // F-H
    g.add_edge(6, 7, 1); // G-H

    println!("Graph:");
    g.print();

    let mut visited = vec![false; g.n()];
    let mut dist = vec![-1i32; g.n()];
    let mut queue = VecDeque::new();
    let mut ops = 0usize;

    visited[0] = true;
    dist[0] = 0;
    queue.push_back(0);

    println!("\nBFS from A:");
    while let Some(u) = queue.pop_front() {
        let nbrs: Vec<char> = g.adj[u].iter().map(|&(d, _)| g.labels[d]).collect();
        println!(
            "  Visit {} (dist={}): neighbors → {:?}",
            g.labels[u], dist[u], nbrs
        );

        for &(v, _) in &g.adj[u] {
            ops += 1;
            if !visited[v] {
                visited[v] = true;
                dist[v] = dist[u] + 1;
                queue.push_back(v);
            }
        }
    }

    println!("\nDistances from A:");
    for i in 0..g.n() {
        println!("  {} → {} = {}", g.labels[0], g.labels[i], dist[i]);
    }

    let m = g.edge_count();
    println!("\nNeighbor checks: {} (= 2*|E| = {})", ops, 2 * m);
    println!("With matrix: would be n² = {} checks\n", g.n() * g.n());
}

// ── Demo 5: Graph transpose ─────────────────────────────────────────

fn demo_transpose() {
    println!("=== Demo 5: Graph Transpose ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D'], true);
    g.add_edge(0, 1, 1); // A→B
    g.add_edge(0, 2, 1); // A→C
    g.add_edge(1, 2, 1); // B→C
    g.add_edge(2, 3, 1); // C→D

    println!("Original G:");
    g.print();

    let gt = g.transpose();
    println!("\nTranspose G^T:");
    gt.print();

    println!("\nVerification:");
    println!(
        "  G has A→B: {}, G^T has B→A: {}",
        g.has_edge(0, 1),
        gt.has_edge(1, 0)
    );
    println!(
        "  G has C→D: {}, G^T has D→C: {}",
        g.has_edge(2, 3),
        gt.has_edge(3, 2)
    );
    println!();
}

// ── Demo 6: Connected components ─────────────────────────────────────

fn demo_components() {
    println!("=== Demo 6: Connected Components ===\n");

    // Graph with 3 components: {A,B,C}, {D,E}, {F}
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E', 'F'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(1, 2, 1); // B-C
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(3, 4, 1); // D-E
    // F is isolated

    println!("Graph:");
    g.print();

    let mut visited = vec![false; g.n()];
    let mut components: Vec<Vec<char>> = Vec::new();

    for start in 0..g.n() {
        if visited[start] {
            continue;
        }
        let mut component = Vec::new();
        let mut stack = vec![start];
        while let Some(v) = stack.pop() {
            if visited[v] {
                continue;
            }
            visited[v] = true;
            component.push(g.labels[v]);
            for &(u, _) in &g.adj[v] {
                if !visited[u] {
                    stack.push(u);
                }
            }
        }
        component.sort();
        components.push(component);
    }

    println!("\nConnected components:");
    for (i, comp) in components.iter().enumerate() {
        println!("  Component {}: {:?}", i + 1, comp);
    }
    println!("  Total: {}\n", components.len());
}

// ── Demo 7: swap_remove demonstration ────────────────────────────────

fn demo_swap_remove() {
    println!("=== Demo 7: swap_remove for O(1) Edge Deletion ===\n");

    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(0, 3, 1); // A-D
    g.add_edge(0, 4, 1); // A-E

    println!("A's neighbors: {:?}",
             g.adj[0].iter().map(|&(d, _)| g.labels[d]).collect::<Vec<_>>());

    // Remove A-B using swap_remove (position-based, O(1))
    println!("\nRemoving A-B via swap_remove:");
    if let Some(pos) = g.adj[0].iter().position(|&(d, _)| d == 1) {
        println!("  Found B at index {}", pos);
        g.adj[0].swap_remove(pos);
    }
    println!("  A's neighbors after: {:?}",
             g.adj[0].iter().map(|&(d, _)| g.labels[d]).collect::<Vec<_>>());
    println!("  (last element E moved to index 0 — order changed)\n");

    // Compare with retain (removes all matching, preserves order, O(n))
    println!("Removing A-C via retain (preserves order):");
    g.adj[0].retain(|&(d, _)| d != 2);
    println!("  A's neighbors after: {:?}",
             g.adj[0].iter().map(|&(d, _)| g.labels[d]).collect::<Vec<_>>());
    println!("  (order preserved, but O(deg) instead of O(1))\n");
}

// ── Demo 8: Edge list representation ─────────────────────────────────

fn demo_edge_list() {
    println!("=== Demo 8: Edge List Representation ===\n");

    #[derive(Debug, Clone)]
    struct Edge {
        u: usize,
        v: usize,
        weight: i32,
    }

    let labels = ['A', 'B', 'C', 'D', 'E'];
    let mut edges = vec![
        Edge { u: 0, v: 1, weight: 5 },
        Edge { u: 0, v: 2, weight: 2 },
        Edge { u: 1, v: 3, weight: 3 },
        Edge { u: 2, v: 3, weight: 7 },
        Edge { u: 3, v: 4, weight: 1 },
    ];

    println!("Edge list (unsorted):");
    for e in &edges {
        println!("  {}-{}: weight {}", labels[e.u], labels[e.v], e.weight);
    }

    // Sort by weight — ideal for Kruskal's algorithm
    edges.sort_by_key(|e| e.weight);
    println!("\nSorted by weight (for Kruskal's MST):");
    for e in &edges {
        println!("  {}-{}: weight {}", labels[e.u], labels[e.v], e.weight);
    }

    // Convert edge list to adjacency list
    let n = labels.len();
    let mut adj: Vec<Vec<(usize, i32)>> = vec![vec![]; n];
    for e in &edges {
        adj[e.u].push((e.v, e.weight));
        adj[e.v].push((e.u, e.weight));
    }
    println!("\nConverted to adjacency list:");
    for (i, neighbors) in adj.iter().enumerate() {
        let nbrs: Vec<String> = neighbors
            .iter()
            .map(|&(d, w)| format!("{}({})", labels[d], w))
            .collect();
        println!("  {}: [{}]", labels[i], nbrs.join(", "));
    }
    println!();
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Adjacency List — Rust Demonstrations               ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_basic();
    demo_weighted();
    demo_directed();
    demo_bfs();
    demo_transpose();
    demo_components();
    demo_swap_remove();
    demo_edge_list();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Adjacency list representation fully demonstrated!  ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 12. Ejercicios

### Ejercicio 1 — Construcción manual

Dado el siguiente grafo no dirigido, escribe la lista de adyacencia:

```
    A --- B --- C
    |         / |
    D --- E --- F
```

<details><summary>¿Cuántas entradas tiene la lista en total?</summary>

Aristas: A-B, B-C, A-D, D-E, E-C, E-F, C-F. Total: 7 aristas.

Lista de adyacencia:
- A: [B, D]
- B: [A, C]
- C: [B, E, F]
- D: [A, E]
- E: [D, C, F]
- F: [E, C]

Entradas totales: $2 + 2 + 3 + 2 + 3 + 2 = 14 = 2 \times 7 = 2|E|$.
Cada arista aparece exactamente dos veces (una por cada extremo).

</details>

### Ejercicio 2 — Complejidad de BFS

¿Cuál es la complejidad de BFS usando lista de adyacencia? Desglósala en
términos de las operaciones sobre la cola y el recorrido de vecinos.

<details><summary>¿Por qué es O(n + m) y no O(nm)?</summary>

Cada vértice se encola y desencola **una sola vez**: $O(n)$ operaciones de
cola. Para cada vértice $u$ que se desencola, se recorren sus $\deg(u)$
vecinos. La suma total de vecinos recorridos es:

$$\sum_{u \in V} \deg(u) = 2|E| = 2m$$

Total: $O(n + 2m) = O(n + m)$.

**No** es $O(nm)$ porque cada arista se examina un número constante de
veces (exactamente 2 en grafo no dirigido), no una vez por cada vértice.
Con matriz, sería $O(n \cdot n) = O(n^2)$ porque "listar vecinos" cuesta
$O(n)$ por vértice.

</details>

### Ejercicio 3 — Lista enlazada vs array dinámico

Implementa ambas versiones (linked list y `Vec`) para un grafo de 1000
vértices y 5000 aristas. Mide el tiempo de un BFS completo con cada
implementación.

<details><summary>¿Cuál esperas que sea más rápida y por qué?</summary>

El **array dinámico** (`Vec`) será más rápido, típicamente 2-5x, por dos
razones:

1. **Localidad de caché**: los vecinos de un vértice están contiguos en
   memoria, lo que permite prefetching por hardware. En una lista enlazada,
   cada nodo puede estar en cualquier posición del heap.
2. **Menos overhead por elemento**: un par `(dest, weight)` ocupa 8 bytes
   en un array vs ~24 bytes en un nodo de lista (dest + weight + puntero
   + padding por alineamiento).

Para BFS, donde se recorren secuencialmente todos los vecinos, la localidad
es el factor dominante.

</details>

### Ejercicio 4 — Swap-remove

Explica por qué `swap_remove` es $O(1)$ pero cambia el orden de los vecinos.
¿En qué situaciones esto es aceptable y en cuáles no?

<details><summary>¿Cuándo importa el orden?</summary>

`swap_remove` copia el último elemento a la posición del eliminado y reduce
el tamaño — una operación $O(1)$. El `remove` convencional desplaza todos
los elementos posteriores — $O(\deg)$.

**Aceptable** (la mayoría de casos): BFS, DFS, Dijkstra, Kruskal — el orden
de exploración de vecinos no afecta la corrección, solo el orden de empates.

**No aceptable**:
- Si el orden de vecinos codifica prioridad o precedencia.
- Si se necesita salida determinista (testing con snapshots).
- Si el grafo representa un polígono o path donde el orden importa.

En la práctica, >95% de los algoritmos de grafos son indiferentes al orden
de vecinos.

</details>

### Ejercicio 5 — In-degree eficiente

Modifica la estructura `Graph` para mantener un array `in_deg[n]` que se
actualice en $O(1)$ con cada `add_edge` y `remove_edge`. ¿Cuánto espacio
extra cuesta?

<details><summary>¿Cómo se actualiza con cada operación?</summary>

```c
void add_edge(Graph *g, int u, int v, int w) {
    // ... normal add ...
    g->in_deg[v]++;
    if (!g->directed) g->in_deg[u]++;
}

void remove_edge(Graph *g, int u, int v) {
    // ... normal remove ...
    g->in_deg[v]--;
    if (!g->directed) g->in_deg[u]--;
}
```

Espacio extra: $O(n)$ (un array de $n$ enteros). Es insignificante comparado
con el espacio $O(n + m)$ de la lista. Consultar in-degree pasa de $O(n + m)$
a $O(1)$.

</details>

### Ejercicio 6 — Grafo transpuesto

Implementa `graph_transpose` que construya $G^T$ en $O(n + m)$. Verifica
que $(G^T)^T = G$ (el transpuesto del transpuesto es el original).

<details><summary>¿Por qué es O(n + m)?</summary>

Se recorre cada lista de adyacencia exactamente una vez: para cada vértice
$u$, para cada vecino $v$ en la lista de $u$, se agrega $u$ a la lista de
$v$ en $G^T$. Total de operaciones = suma de tamaños de todas las listas
= $m$ (dirigido). Crear el array de listas vacías = $O(n)$. Total: $O(n + m)$.

$(G^T)^T = G$ porque invertir dos veces cada arista la devuelve a su
dirección original.

</details>

### Ejercicio 7 — Lectura desde archivo

Escribe una función que lea un grafo desde un archivo con formato:
```
n m directed
u1 v1 w1
u2 v2 w2
...
```
y construya la lista de adyacencia. Pruébala con un archivo de al menos
20 aristas.

<details><summary>¿Qué validaciones debería hacer?</summary>

1. $0 \le u, v < n$ (vértices válidos).
2. $w \ne 0$ si se usa 0 como "sin arista" (depende de la semántica).
3. No duplicar aristas si el grafo no es multigrafo.
4. Contar las aristas leídas y verificar que coincide con $m$.
5. Si es no dirigido, no leer la misma arista dos veces (el formato da
   cada arista una sola vez, la función agrega ambas direcciones).

</details>

### Ejercicio 8 — Chain forward star

Implementa la representación "chain forward star" (array estático sin
`malloc`) que se usa en competencias. Compárala con la versión `malloc`
para un grafo de $10^5$ vértices.

<details><summary>¿Por qué evitar malloc en competencias?</summary>

1. **Velocidad**: `malloc` tiene overhead por bookkeeping interno (headers,
   freelists, posible syscall `sbrk`/`mmap`). Miles de mallocs pequeños
   son lentos comparados con un array preasignado.
2. **Fragmentación**: miles de nodos pequeños dispersos en el heap causan
   cache misses. Un array contiguo es mucho más cache-friendly.
3. **Simplicidad**: no hay que liberar memoria (el proceso termina y el OS
   recupera todo). Menos código = menos bugs.
4. **Predictibilidad**: en competencias, el tiempo de ejecución debe ser
   predecible. `malloc` puede variar según el estado del allocator.

</details>

### Ejercicio 9 — Detección de arista con hash

Para mejorar `has_edge` de $O(\deg)$ a $O(1)$, se puede mantener un
`HashSet` de aristas además de la lista de adyacencia. Implementa esta
versión híbrida. ¿Cuánta memoria extra cuesta?

<details><summary>¿Cuál es el trade-off?</summary>

Memoria extra: $O(m)$ para el HashSet (en la práctica ~20-30 bytes por arista
con overhead de hashing). Para un grafo con $m = 10^6$:
- Lista sola: ~16 MB.
- Lista + HashSet: ~16 + ~24 MB = ~40 MB.

El trade-off es **2.5x más memoria** a cambio de **$O(1)$ edge queries**.
Vale la pena cuando `has_edge` se llama frecuentemente (ej. verificar si
una arista ya existe antes de insertar, o algoritmos de clique/coloring).

En Rust, `HashSet<(usize, usize)>` es la forma natural.

</details>

### Ejercicio 10 — Conversión entre representaciones

Implementa funciones de conversión:
1. Matriz de adyacencia → Lista de adyacencia.
2. Lista de adyacencia → Matriz de adyacencia.
3. Lista de aristas → Lista de adyacencia.

¿Cuál es la complejidad de cada conversión?

<details><summary>¿Cuáles son las complejidades?</summary>

1. **Matriz → Lista**: $O(n^2)$. Recorrer toda la matriz; para cada
   $A[i][j] \ne 0$, agregar $j$ a la lista de $i$.
2. **Lista → Matriz**: $O(n + m)$ para la construcción + $O(n^2)$ para
   inicializar la matriz a 0. Total: $O(n^2)$. Incluso si el grafo es
   disperso, hay que llenar $n^2$ celdas con 0.
3. **Aristas → Lista**: $O(n + m)$. Crear $n$ listas vacías ($O(n)$) y
   agregar cada arista ($O(m)$).

Notar que toda conversión **hacia** la matriz es al menos $O(n^2)$ por la
inicialización. Toda conversión **desde** la lista es $O(n + m)$.

</details>

---

## 13. Resumen

| Aspecto | Lista enlazada | Array dinámico (`Vec`) | Lista de aristas |
|---------|:--------------:|:---------------------:|:----------------:|
| Espacio | $O(n + m)$, overhead alto | $O(n + m)$, compacto | $O(m)$ |
| Listar vecinos | $O(\deg)$, cache misses | $O(\deg)$, cache-friendly | $O(m)$ |
| ¿Existe arista? | $O(\deg)$ | $O(\deg)$ | $O(m)$ |
| Agregar arista | $O(1)$ | $O(1)$ amortizado | $O(1)$ |
| Eliminar arista | $O(\deg)$ | $O(1)$ swap / $O(\deg)$ shift | $O(m)$ |
| Ideal para | C clásico, libros de texto | Rust, C++, producción | Kruskal, E/S |

La lista de adyacencia (en su variante de array dinámico) es la
representación **por defecto** para la gran mayoría de problemas de grafos.
En T04 veremos cómo Rust y el crate `petgraph` ofrecen abstracciones de
más alto nivel, y en T05 haremos la comparación definitiva entre todas las
representaciones.
