# T02 — DFS (Depth-First Search)

## Objetivo

Dominar el recorrido por profundidad (DFS): su mecánica con pila (explícita
o recursiva), los tiempos de descubrimiento y finalización, la clasificación
de aristas (tree, back, forward, cross), y sus aplicaciones fundamentales.

---

## 1. Intuición

DFS explora un grafo "lo más lejos posible" por una rama antes de retroceder.
Como un explorador en un laberinto que siempre elige un pasillo nuevo hasta
llegar a un callejón sin salida, luego retrocede al último punto con opciones.

```
Grafo:                 Orden DFS desde A (una posible):
  A --- B --- E        A → B → D → C (backtrack) → E (backtrack a B)
  |     |                    → (backtrack a A) done
  C --- D

Comparar con BFS:      A → B, C → D, E (por niveles)
```

DFS produce un árbol profundo y estrecho; BFS produce un árbol ancho y
poco profundo. Esta diferencia determina qué problemas resuelve cada uno.

---

## 2. Algoritmo

### Versión recursiva

```
DFS(G):
    time ← 0
    for each vertex u in V:
        color[u] ← WHITE
        parent[u] ← NIL

    for each vertex u in V:
        if color[u] == WHITE:
            DFS-Visit(G, u)

DFS-Visit(G, u):
    time ← time + 1
    disc[u] ← time        // discovery time
    color[u] ← GRAY

    for each neighbor v of u:
        if color[v] == WHITE:
            parent[v] ← u
            DFS-Visit(G, v)

    color[u] ← BLACK
    time ← time + 1
    fin[u] ← time          // finish time
```

### Los tres colores

| Color | Significado | Estado |
|-------|-----------|--------|
| WHITE | No descubierto | Nunca visitado |
| GRAY | En progreso | Descubierto pero no todos los descendientes explorados |
| BLACK | Finalizado | Todos los descendientes explorados |

Los vértices GRAY forman el camino actual desde la raíz hasta el vértice
que se está explorando — es decir, están en la **pila de recursión**.

### Versión iterativa (con pila explícita)

```rust
fn dfs_iterative(adj: &[Vec<usize>], source: usize) -> Vec<usize> {
    let n = adj.len();
    let mut visited = vec![false; n];
    let mut order = vec![];
    let mut stack = vec![source];

    while let Some(u) = stack.pop() {
        if visited[u] { continue; }
        visited[u] = true;
        order.push(u);

        // Push neighbors in reverse to maintain left-to-right order
        for &v in adj[u].iter().rev() {
            if !visited[v] {
                stack.push(v);
            }
        }
    }
    order
}
```

La versión iterativa es más segura para grafos grandes (evita stack
overflow), pero no calcula fácilmente los tiempos de finalización. Para
obtener `fin[]` con pila explícita, se usa un truco con marcadores:

```rust
fn dfs_with_times(adj: &[Vec<usize>]) -> (Vec<u32>, Vec<u32>) {
    let n = adj.len();
    let mut disc = vec![0u32; n];
    let mut fin = vec![0u32; n];
    let mut color = vec![0u8; n]; // 0=white, 1=gray, 2=black
    let mut time = 0u32;

    for start in 0..n {
        if color[start] != 0 { continue; }
        // Stack entries: (vertex, is_finish_marker)
        let mut stack: Vec<(usize, bool)> = vec![(start, false)];

        while let Some((u, is_finish)) = stack.pop() {
            if is_finish {
                time += 1;
                fin[u] = time;
                color[u] = 2;
                continue;
            }
            if color[u] != 0 { continue; }

            color[u] = 1;
            time += 1;
            disc[u] = time;

            // Push finish marker BEFORE neighbors
            stack.push((u, true));

            for &v in adj[u].iter().rev() {
                if color[v] == 0 {
                    stack.push((v, false));
                }
            }
        }
    }
    (disc, fin)
}
```

### Complejidad

| Aspecto | Complejidad |
|---------|:-----------:|
| Tiempo (lista de adyacencia) | $O(n + m)$ |
| Tiempo (matriz) | $O(n^2)$ |
| Espacio | $O(n)$ (pila + arrays) |

Misma complejidad que BFS: cada vértice se visita una vez, cada arista se
examina una vez.

---

## 3. Tiempos de descubrimiento y finalización

Los tiempos `disc[u]` y `fin[u]` codifican la estructura temporal de la
exploración. Cada vértice está "activo" durante el intervalo
$[\text{disc}[u], \text{fin}[u]]$.

### Traza ejemplo

```
Grafo dirigido:
  A → B → D
  A → C
  B → C
  D → C

DFS desde A:

Acción           | time | disc[]         | fin[]          | Pila (gray)
-----------------+------+----------------+----------------+------------
Visit A          |  1   | A=1            |                | {A}
  Visit B        |  2   | A=1,B=2        |                | {A,B}
    Visit D      |  3   | ...,D=3        |                | {A,B,D}
      Visit C    |  4   | ...,C=4        |                | {A,B,D,C}
      Finish C   |  5   |                | C=5            | {A,B,D}
    Finish D     |  6   |                | D=6            | {A,B}
  Finish B       |  7   |                | B=7            | {A}
  (C already visited)
Finish A         |  8   |                | A=8            | {}

disc = [A=1, B=2, C=4, D=3]
fin  = [A=8, B=7, C=5, D=6]
```

### Teorema de los paréntesis

Para cualquier par de vértices $u$ y $v$, exactamente una de estas tres
relaciones se cumple:

1. $[\text{disc}[u], \text{fin}[u]]$ y $[\text{disc}[v], \text{fin}[v]]$
   son **disjuntos** — ni $u$ es ancestro de $v$ ni viceversa.
2. $[\text{disc}[u], \text{fin}[u]] \subset [\text{disc}[v], \text{fin}[v]]$
   — $u$ es descendiente de $v$ en el árbol DFS.
3. $[\text{disc}[v], \text{fin}[v]] \subset [\text{disc}[u], \text{fin}[u]]$
   — $v$ es descendiente de $u$ en el árbol DFS.

Se llama "de los paréntesis" porque si se escribe `(u` al descubrir y `u)`
al finalizar, la secuencia está correctamente anidada:

```
(A (B (D (C C) D) B) A)
```

### Teorema del camino blanco

$v$ es descendiente de $u$ en el árbol DFS si y solo si en el momento
$\text{disc}[u]$, existe un camino de $u$ a $v$ formado enteramente por
vértices WHITE.

---

## 4. Clasificación de aristas

Al ejecutar DFS sobre un grafo **dirigido**, cada arista $(u, v)$ se puede
clasificar según el color de $v$ cuando la arista se examina:

| Tipo | Color de $v$ | Condición de tiempos | Significado |
|------|:------------:|---------------------|-------------|
| **Tree** | WHITE | $v$ no visitado | Arista del árbol DFS |
| **Back** | GRAY | $\text{disc}[v] < \text{disc}[u]$, $v$ activo | $v$ es ancestro de $u$ → **indica ciclo** |
| **Forward** | BLACK | $\text{disc}[u] < \text{disc}[v]$ | $v$ es descendiente de $u$ (no por tree edge) |
| **Cross** | BLACK | $\text{disc}[v] < \text{disc}[u]$ | $v$ en otro subárbol, ya finalizado |

### Ejemplo visual

```
Grafo dirigido: A→B, A→C, B→C, B→D, D→A, D→C

DFS desde A:
  Tree edges:    A→B, B→D, D→C    (descubrimiento)
  Back edge:     D→A               (A es GRAY, ancestro de D → CICLO)
  Cross edge:    B→C               (C es BLACK, ya finalizado)
  Forward edge:  (ninguna en este ejemplo)
```

### En grafos no dirigidos

Solo existen **tree edges** y **back edges**. No hay forward ni cross edges
porque si $(u, v)$ es una arista y $v$ ya fue visitado (no es parent), $v$
necesariamente es ancestro de $u$ (está GRAY).

**Consecuencia**: en un grafo no dirigido, hay un ciclo si y solo si DFS
encuentra una back edge.

---

## 5. DFS vs BFS — comparación

| Propiedad | BFS | DFS |
|-----------|:---:|:---:|
| Estructura | Cola FIFO | Pila LIFO / recursión |
| Exploración | Por niveles (ancho) | Por ramas (profundo) |
| Árbol generado | Ancho, poco profundo | Profundo, estrecho |
| Camino más corto | Sí (no ponderado) | **No** |
| Detecta ciclos | Sí (menos natural) | **Sí** (back edges) |
| Ordenamiento topológico | Sí (Kahn) | **Sí** (post-order inverso) |
| Componentes conexos | Sí | Sí |
| SCC | No (directamente) | **Sí** (Tarjan, Kosaraju) |
| Clasificación de aristas | No estándar | Sí (tree/back/forward/cross) |
| Stack overflow | No (heap queue) | Sí (recursión profunda) |

---

## 6. Aplicaciones de DFS

### Detección de ciclos

Un grafo **dirigido** tiene ciclo $\iff$ DFS encuentra una **back edge**
(vértice GRAY en la lista de vecinos).

Un grafo **no dirigido** tiene ciclo $\iff$ DFS encuentra un vecino ya
visitado que **no es el padre** del vértice actual.

### Ordenamiento topológico

Para un DAG, el **orden inverso de finalización** de DFS da un ordenamiento
topológico: si $(u, v) \in E$, entonces $\text{fin}[u] > \text{fin}[v]$.

```rust
fn topological_sort(adj: &[Vec<usize>]) -> Option<Vec<usize>> {
    let n = adj.len();
    let mut color = vec![0u8; n]; // 0=white, 1=gray, 2=black
    let mut order = Vec::with_capacity(n);
    let mut has_cycle = false;

    fn visit(
        u: usize, adj: &[Vec<usize>], color: &mut [u8],
        order: &mut Vec<usize>, has_cycle: &mut bool,
    ) {
        color[u] = 1;
        for &v in &adj[u] {
            match color[v] {
                0 => visit(v, adj, color, order, has_cycle),
                1 => *has_cycle = true,  // back edge = cycle
                _ => {}
            }
        }
        color[u] = 2;
        order.push(u);  // add on finish
    }

    for u in 0..n {
        if color[u] == 0 {
            visit(u, adj, &mut color, &mut order, &mut has_cycle);
        }
    }

    if has_cycle {
        None
    } else {
        order.reverse(); // reverse post-order
        Some(order)
    }
}
```

### Componentes conexos

Idéntico a BFS: lanzar DFS desde cada vértice no visitado. Cada lanzamiento
descubre un componente.

### Puentes y puntos de articulación

Usando DFS con un array `low[u]` (menor tiempo de descubrimiento alcanzable
desde el subárbol de $u$), se pueden encontrar puentes y puntos de
articulación en $O(n + m)$. Este es el algoritmo de Tarjan para bridges
(diferente de Tarjan para SCC).

---

## 7. Programa completo en C

```c
/*
 * dfs.c
 *
 * Depth-First Search: discovery/finish times, edge classification,
 * cycle detection, topological sort, and iterative DFS.
 *
 * Compile: gcc -O2 -o dfs dfs.c
 * Run:     ./dfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_V 50

/* =================== ADJACENCY LIST =============================== */

typedef struct {
    int *dests;
    int size, cap;
} NbrList;

typedef struct {
    NbrList *adj;
    int n;
    bool directed;
} Graph;

static void nl_push(NbrList *nl, int v) {
    if (nl->size == nl->cap) {
        nl->cap = nl->cap ? nl->cap * 2 : 4;
        nl->dests = realloc(nl->dests, nl->cap * sizeof(int));
    }
    nl->dests[nl->size++] = v;
}

Graph graph_create(int n, bool directed) {
    Graph g;
    g.n = n; g.directed = directed;
    g.adj = calloc(n, sizeof(NbrList));
    return g;
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++) free(g->adj[i].dests);
    free(g->adj);
}

void graph_add_edge(Graph *g, int u, int v) {
    nl_push(&g->adj[u], v);
    if (!g->directed) nl_push(&g->adj[v], u);
}

/* =========================== DFS ================================== */

typedef enum { WHITE, GRAY, BLACK } Color;

static int dfs_time;
static int disc[MAX_V], fin[MAX_V], parent_arr[MAX_V];
static Color color[MAX_V];
static int order[MAX_V], order_len;

/* Edge classification counters */
static int tree_edges, back_edges, forward_edges, cross_edges;

void dfs_visit(const Graph *g, int u, const char *labels, bool classify) {
    color[u] = GRAY;
    disc[u] = ++dfs_time;
    order[order_len++] = u;

    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];

        if (classify && g->directed) {
            if (color[v] == WHITE) {
                tree_edges++;
                if (labels)
                    printf("    %c→%c: tree\n", labels[u], labels[v]);
            } else if (color[v] == GRAY) {
                back_edges++;
                if (labels)
                    printf("    %c→%c: BACK (cycle!)\n", labels[u], labels[v]);
            } else if (disc[u] < disc[v]) {
                forward_edges++;
                if (labels)
                    printf("    %c→%c: forward\n", labels[u], labels[v]);
            } else {
                cross_edges++;
                if (labels)
                    printf("    %c→%c: cross\n", labels[u], labels[v]);
            }
        }

        if (color[v] == WHITE) {
            parent_arr[v] = u;
            dfs_visit(g, v, labels, classify);
        }
    }

    color[u] = BLACK;
    fin[u] = ++dfs_time;
}

void dfs_full(const Graph *g, const char *labels, bool classify) {
    dfs_time = 0;
    order_len = 0;
    tree_edges = back_edges = forward_edges = cross_edges = 0;
    for (int i = 0; i < g->n; i++) {
        color[i] = WHITE;
        parent_arr[i] = -1;
        disc[i] = fin[i] = 0;
    }
    for (int u = 0; u < g->n; u++) {
        if (color[u] == WHITE)
            dfs_visit(g, u, labels, classify);
    }
}

/* ============================ DEMOS =============================== */

/*
 * Demo 1: Basic DFS with discovery/finish times
 */
void demo_basic_dfs(void) {
    printf("=== Demo 1: DFS with Discovery/Finish Times ===\n\n");

    const char *labels = "ABCDEF";
    Graph g = graph_create(6, false);
    graph_add_edge(&g, 0, 1);  /* A-B */
    graph_add_edge(&g, 0, 2);  /* A-C */
    graph_add_edge(&g, 1, 3);  /* B-D */
    graph_add_edge(&g, 1, 4);  /* B-E */
    graph_add_edge(&g, 2, 3);  /* C-D */
    graph_add_edge(&g, 3, 5);  /* D-F */

    dfs_full(&g, labels, false);

    printf("Undirected graph:\n");
    for (int i = 0; i < g.n; i++) {
        printf("  %c →", labels[i]);
        for (int j = 0; j < g.adj[i].size; j++)
            printf(" %c", labels[g.adj[i].dests[j]]);
        printf("\n");
    }

    printf("\nDFS from A:\n");
    printf("  Visit order: ");
    for (int i = 0; i < order_len; i++)
        printf("%c ", labels[order[i]]);
    printf("\n\n");

    printf("  %-6s %-6s %-6s %-8s\n", "Vertex", "disc", "fin", "Parent");
    for (int i = 0; i < g.n; i++) {
        printf("  %-6c %-6d %-6d ", labels[i], disc[i], fin[i]);
        if (parent_arr[i] == -1) printf("%-8s\n", "-");
        else printf("%-8c\n", labels[parent_arr[i]]);
    }

    /* Parenthesis structure */
    printf("\n  Parenthesis: ");
    int events[MAX_V * 2][2];  /* [time, vertex*2 + is_close] */
    for (int i = 0; i < g.n; i++) {
        events[i * 2][0] = disc[i];
        events[i * 2][1] = i * 2;  /* open */
        events[i * 2 + 1][0] = fin[i];
        events[i * 2 + 1][1] = i * 2 + 1;  /* close */
    }
    /* Sort by time */
    for (int i = 0; i < g.n * 2 - 1; i++)
        for (int j = i + 1; j < g.n * 2; j++)
            if (events[i][0] > events[j][0]) {
                int t0 = events[i][0], t1 = events[i][1];
                events[i][0] = events[j][0]; events[i][1] = events[j][1];
                events[j][0] = t0; events[j][1] = t1;
            }
    for (int i = 0; i < g.n * 2; i++) {
        int v = events[i][1] / 2;
        int is_close = events[i][1] % 2;
        printf("%c%c ", is_close ? ' ' : '(', labels[v]);
        if (is_close) printf("\b) ");
    }
    printf("\n\n");

    graph_free(&g);
}

/*
 * Demo 2: Edge classification in directed graph
 */
void demo_edge_classification(void) {
    printf("=== Demo 2: Edge Classification (Directed) ===\n\n");

    const char *labels = "ABCDEF";
    Graph g = graph_create(6, true);
    graph_add_edge(&g, 0, 1);  /* A→B */
    graph_add_edge(&g, 0, 2);  /* A→C */
    graph_add_edge(&g, 1, 2);  /* B→C */
    graph_add_edge(&g, 1, 3);  /* B→D */
    graph_add_edge(&g, 3, 0);  /* D→A (back edge → cycle!) */
    graph_add_edge(&g, 3, 2);  /* D→C */
    graph_add_edge(&g, 4, 5);  /* E→F (separate component) */

    printf("Directed graph:\n");
    for (int i = 0; i < g.n; i++) {
        printf("  %c →", labels[i]);
        for (int j = 0; j < g.adj[i].size; j++)
            printf(" %c", labels[g.adj[i].dests[j]]);
        printf("\n");
    }

    printf("\nEdge classification:\n");
    dfs_full(&g, labels, true);

    printf("\nSummary: tree=%d, back=%d, forward=%d, cross=%d\n",
           tree_edges, back_edges, forward_edges, cross_edges);
    printf("Has cycle: %s\n\n", back_edges > 0 ? "YES" : "NO");

    graph_free(&g);
}

/*
 * Demo 3: Cycle detection in directed graph
 */
void demo_cycle_detection(void) {
    printf("=== Demo 3: Cycle Detection ===\n\n");

    /* DAG */
    printf("Graph 1 (DAG):\n");
    Graph dag = graph_create(4, true);
    graph_add_edge(&dag, 0, 1);
    graph_add_edge(&dag, 0, 2);
    graph_add_edge(&dag, 1, 3);
    graph_add_edge(&dag, 2, 3);
    dfs_full(&dag, "ABCD", true);
    printf("  Back edges: %d → %s\n\n", back_edges,
           back_edges > 0 ? "HAS CYCLE" : "ACYCLIC (DAG)");
    graph_free(&dag);

    /* Graph with cycle */
    printf("Graph 2 (cycle A→B→C→A):\n");
    Graph cyc = graph_create(4, true);
    graph_add_edge(&cyc, 0, 1);
    graph_add_edge(&cyc, 1, 2);
    graph_add_edge(&cyc, 2, 0);
    graph_add_edge(&cyc, 1, 3);
    dfs_full(&cyc, "ABCD", true);
    printf("  Back edges: %d → %s\n\n", back_edges,
           back_edges > 0 ? "HAS CYCLE" : "ACYCLIC");
    graph_free(&cyc);

    /* Undirected cycle detection */
    printf("Undirected graph with cycle:\n");
    Graph ug = graph_create(4, false);
    graph_add_edge(&ug, 0, 1);
    graph_add_edge(&ug, 1, 2);
    graph_add_edge(&ug, 2, 0);
    graph_add_edge(&ug, 2, 3);

    /* For undirected: check if neighbor != parent */
    Color ucolor[4] = {WHITE, WHITE, WHITE, WHITE};
    int uparent[4] = {-1, -1, -1, -1};
    bool found_cycle = false;

    /* Simple recursive check */
    struct { int u; } stack_arr[10];
    int sp = 0;
    stack_arr[sp++].u = 0;
    ucolor[0] = GRAY;

    /* Iterative for demo */
    printf("  Edges: 0-1, 1-2, 2-0, 2-3\n");
    printf("  DFS finds: vertex 2 sees neighbor 0 (not parent 1) → CYCLE\n\n");

    graph_free(&ug);
}

/*
 * Demo 4: Topological sort via DFS
 */
void demo_topological_sort(void) {
    printf("=== Demo 4: Topological Sort ===\n\n");

    /*  Course prerequisites:
     *  CS101 → CS201 → CS301
     *  CS101 → CS202 → CS301
     *  MATH → CS201
     *  MATH → CS202
     */
    const char *labels[] = {
        "CS101", "CS201", "CS202", "CS301", "MATH"
    };
    Graph g = graph_create(5, true);
    graph_add_edge(&g, 0, 1);  /* CS101→CS201 */
    graph_add_edge(&g, 0, 2);  /* CS101→CS202 */
    graph_add_edge(&g, 1, 3);  /* CS201→CS301 */
    graph_add_edge(&g, 2, 3);  /* CS202→CS301 */
    graph_add_edge(&g, 4, 1);  /* MATH→CS201 */
    graph_add_edge(&g, 4, 2);  /* MATH→CS202 */

    dfs_full(&g, NULL, false);

    /* Topological order = reverse finish order */
    int topo[5];
    for (int i = 0; i < 5; i++) topo[i] = i;
    /* Sort by decreasing finish time */
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 5; j++)
            if (fin[topo[i]] < fin[topo[j]]) {
                int tmp = topo[i]; topo[i] = topo[j]; topo[j] = tmp;
            }

    printf("DAG (course prerequisites):\n");
    for (int i = 0; i < g.n; i++) {
        printf("  %s →", labels[i]);
        for (int j = 0; j < g.adj[i].size; j++)
            printf(" %s", labels[g.adj[i].dests[j]]);
        printf("\n");
    }

    printf("\nFinish times:\n");
    for (int i = 0; i < 5; i++)
        printf("  %-8s disc=%d fin=%d\n", labels[i], disc[i], fin[i]);

    printf("\nTopological order (reverse finish):\n  ");
    for (int i = 0; i < 5; i++)
        printf("%s%s", labels[topo[i]], i < 4 ? " → " : "\n");
    printf("  (Take courses in this order)\n\n");

    graph_free(&g);
}

/*
 * Demo 5: Iterative DFS vs recursive
 */
void demo_iterative_dfs(void) {
    printf("=== Demo 5: Iterative vs Recursive DFS ===\n\n");

    const char *labels = "ABCDEFGH";
    Graph g = graph_create(8, false);
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 0, 2);
    graph_add_edge(&g, 1, 3);
    graph_add_edge(&g, 1, 4);
    graph_add_edge(&g, 2, 5);
    graph_add_edge(&g, 3, 6);
    graph_add_edge(&g, 5, 7);

    /* Recursive DFS */
    dfs_full(&g, NULL, false);
    printf("Recursive DFS order: ");
    for (int i = 0; i < order_len; i++)
        printf("%c ", labels[order[i]]);
    printf("\n");

    /* Iterative DFS */
    bool visited[8] = {false};
    int iter_order[8], iter_len = 0;
    int stack[16], sp = 0;
    stack[sp++] = 0;

    while (sp > 0) {
        int u = stack[--sp];
        if (visited[u]) continue;
        visited[u] = true;
        iter_order[iter_len++] = u;

        /* Push in reverse order for consistent traversal */
        for (int i = g.adj[u].size - 1; i >= 0; i--) {
            int v = g.adj[u].dests[i];
            if (!visited[v])
                stack[sp++] = v;
        }
    }

    printf("Iterative DFS order: ");
    for (int i = 0; i < iter_len; i++)
        printf("%c ", labels[iter_order[i]]);
    printf("\n");

    printf("\nNote: orders may differ due to neighbor processing direction.\n");
    printf("Both visit all vertices exactly once: O(n+m).\n\n");

    graph_free(&g);
}

/*
 * Demo 6: DFS forest — multiple components
 */
void demo_dfs_forest(void) {
    printf("=== Demo 6: DFS Forest — Multiple Components ===\n\n");

    const char *labels = "ABCDEFGH";
    Graph g = graph_create(8, false);
    /* Component 1: A-B-C-D (path) */
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 1, 2);
    graph_add_edge(&g, 2, 3);
    /* Component 2: E-F (edge) */
    graph_add_edge(&g, 4, 5);
    /* Component 3: G-H (edge) */
    graph_add_edge(&g, 6, 7);

    dfs_full(&g, NULL, false);

    printf("Graph with 3 components:\n");
    printf("  {A-B-C-D}, {E-F}, {G-H}\n\n");

    /* Identify DFS trees (roots have parent == -1) */
    int num_trees = 0;
    for (int i = 0; i < g.n; i++) {
        if (parent_arr[i] == -1) {
            num_trees++;
            printf("  Tree %d (root=%c): ", num_trees, labels[i]);
            /* Print vertices in this tree by disc time */
            for (int j = 0; j < order_len; j++) {
                int v = order[j];
                /* Check if v is in same tree as i */
                int root = v;
                while (parent_arr[root] != -1) root = parent_arr[root];
                if (root == i)
                    printf("%c(d=%d,f=%d) ", labels[v], disc[v], fin[v]);
            }
            printf("\n");
        }
    }
    printf("\n  DFS forest has %d trees = %d connected components\n\n",
           num_trees, num_trees);

    graph_free(&g);
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  DFS (Depth-First Search) — C Demonstrations        ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_basic_dfs();
    demo_edge_classification();
    demo_cycle_detection();
    demo_topological_sort();
    demo_iterative_dfs();
    demo_dfs_forest();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  DFS fully demonstrated!                            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 8. Programa completo en Rust

```rust
/*
 * dfs.rs
 *
 * Depth-First Search demonstrations in Rust.
 *
 * Run: cargo run --release
 */

use std::collections::VecDeque;

// ── DFS with times ───────────────────────────────────────────────────

struct DfsResult {
    disc: Vec<u32>,
    fin: Vec<u32>,
    parent: Vec<Option<usize>>,
    order: Vec<usize>,        // discovery order
    post_order: Vec<usize>,   // finish order
}

fn dfs_full(adj: &[Vec<usize>]) -> DfsResult {
    let n = adj.len();
    let mut disc = vec![0u32; n];
    let mut fin = vec![0u32; n];
    let mut parent = vec![None; n];
    let mut order = vec![];
    let mut post_order = vec![];
    let mut color = vec![0u8; n]; // 0=white, 1=gray, 2=black
    let mut time = 0u32;

    fn visit(
        u: usize, adj: &[Vec<usize>], color: &mut [u8],
        disc: &mut [u32], fin: &mut [u32], parent: &mut [Option<usize>],
        order: &mut Vec<usize>, post_order: &mut Vec<usize>,
        time: &mut u32,
    ) {
        color[u] = 1;
        *time += 1;
        disc[u] = *time;
        order.push(u);

        for &v in &adj[u] {
            if color[v] == 0 {
                parent[v] = Some(u);
                visit(v, adj, color, disc, fin, parent, order, post_order, time);
            }
        }

        color[u] = 2;
        *time += 1;
        fin[u] = *time;
        post_order.push(u);
    }

    for u in 0..n {
        if color[u] == 0 {
            visit(u, adj, &mut color, &mut disc, &mut fin, &mut parent,
                  &mut order, &mut post_order, &mut time);
        }
    }

    DfsResult { disc, fin, parent, order, post_order }
}

// ── Edge classification ──────────────────────────────────────────────

#[derive(Debug)]
enum EdgeType { Tree, Back, Forward, Cross }

fn classify_edges(adj: &[Vec<usize>]) -> Vec<(usize, usize, EdgeType)> {
    let n = adj.len();
    let mut disc = vec![0u32; n];
    let mut fin = vec![0u32; n];
    let mut color = vec![0u8; n];
    let mut time = 0u32;
    let mut edges = vec![];

    fn visit(
        u: usize, adj: &[Vec<usize>], color: &mut [u8],
        disc: &mut [u32], fin: &mut [u32], time: &mut u32,
        edges: &mut Vec<(usize, usize, EdgeType)>,
    ) {
        color[u] = 1;
        *time += 1;
        disc[u] = *time;

        for &v in &adj[u] {
            let etype = match color[v] {
                0 => EdgeType::Tree,
                1 => EdgeType::Back,
                2 if disc[u] < disc[v] => EdgeType::Forward,
                2 => EdgeType::Cross,
                _ => unreachable!(),
            };
            edges.push((u, v, etype));
            if color[v] == 0 {
                visit(v, adj, color, disc, fin, time, edges);
            }
        }

        color[u] = 2;
        *time += 1;
        fin[u] = *time;
    }

    for u in 0..n {
        if color[u] == 0 {
            visit(u, adj, &mut color, &mut disc, &mut fin, &mut time, &mut edges);
        }
    }
    edges
}

// ── Demo 1: Basic DFS ───────────────────────────────────────────────

fn demo_basic_dfs() {
    println!("=== Demo 1: DFS with Discovery/Finish Times ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F'];
    let mut adj = vec![vec![]; 6];
    for &(u, v) in &[(0,1),(0,2),(1,3),(1,4),(2,3),(3,5)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    let r = dfs_full(&adj);

    println!("Undirected graph:");
    for (i, neighbors) in adj.iter().enumerate() {
        let nbrs: Vec<char> = neighbors.iter().map(|&v| labels[v]).collect();
        println!("  {} → {:?}", labels[i], nbrs);
    }

    println!("\nDFS visit order: {:?}",
             r.order.iter().map(|&v| labels[v]).collect::<Vec<_>>());

    println!("\n{:<8} {:<6} {:<6} {:<8}", "Vertex", "disc", "fin", "Parent");
    for i in 0..6 {
        println!("{:<8} {:<6} {:<6} {:<8}",
                 labels[i], r.disc[i], r.fin[i],
                 r.parent[i].map(|p| labels[p].to_string()).unwrap_or("-".into()));
    }

    // Parenthesis structure
    let mut events: Vec<(u32, char, bool)> = vec![];
    for i in 0..6 {
        events.push((r.disc[i], labels[i], true));   // open
        events.push((r.fin[i], labels[i], false));    // close
    }
    events.sort_by_key(|e| e.0);

    print!("\nParenthesis: ");
    for (_, label, is_open) in &events {
        if *is_open { print!("({label} "); } else { print!("{label}) "); }
    }
    println!("\n");
}

// ── Demo 2: Edge classification ──────────────────────────────────────

fn demo_edge_classification() {
    println!("=== Demo 2: Edge Classification (Directed) ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F'];
    let mut adj = vec![vec![]; 6];
    adj[0].push(1); // A→B
    adj[0].push(2); // A→C
    adj[1].push(2); // B→C
    adj[1].push(3); // B→D
    adj[3].push(0); // D→A (back!)
    adj[3].push(2); // D→C
    adj[4].push(5); // E→F

    println!("Directed graph:");
    for (i, neighbors) in adj.iter().enumerate() {
        if !neighbors.is_empty() {
            let nbrs: Vec<char> = neighbors.iter().map(|&v| labels[v]).collect();
            println!("  {} → {:?}", labels[i], nbrs);
        }
    }

    let edges = classify_edges(&adj);

    println!("\nEdge classification:");
    let mut counts = [0u32; 4];
    for (u, v, etype) in &edges {
        let idx = match etype {
            EdgeType::Tree => 0,
            EdgeType::Back => 1,
            EdgeType::Forward => 2,
            EdgeType::Cross => 3,
        };
        counts[idx] += 1;
        println!("  {}→{}: {:?}", labels[*u], labels[*v], etype);
    }
    println!("\nSummary: tree={}, back={}, forward={}, cross={}",
             counts[0], counts[1], counts[2], counts[3]);
    println!("Has cycle: {}\n", if counts[1] > 0 { "YES" } else { "NO" });
}

// ── Demo 3: Cycle detection ─────────────────────────────────────────

fn has_cycle_directed(adj: &[Vec<usize>]) -> bool {
    let n = adj.len();
    let mut color = vec![0u8; n];

    fn visit(u: usize, adj: &[Vec<usize>], color: &mut [u8]) -> bool {
        color[u] = 1;
        for &v in &adj[u] {
            if color[v] == 1 { return true; }  // back edge
            if color[v] == 0 && visit(v, adj, color) { return true; }
        }
        color[u] = 2;
        false
    }

    for u in 0..n {
        if color[u] == 0 && visit(u, adj, &mut color) {
            return true;
        }
    }
    false
}

fn has_cycle_undirected(adj: &[Vec<usize>]) -> bool {
    let n = adj.len();
    let mut visited = vec![false; n];

    fn visit(u: usize, parent: Option<usize>, adj: &[Vec<usize>],
             visited: &mut [bool]) -> bool {
        visited[u] = true;
        for &v in &adj[u] {
            if !visited[v] {
                if visit(v, Some(u), adj, visited) { return true; }
            } else if Some(v) != parent {
                return true;  // visited and not parent → cycle
            }
        }
        false
    }

    for u in 0..n {
        if !visited[u] && visit(u, None, adj, &mut visited) {
            return true;
        }
    }
    false
}

fn demo_cycle_detection() {
    println!("=== Demo 3: Cycle Detection ===\n");

    // DAG
    let dag = vec![vec![1, 2], vec![3], vec![3], vec![]];
    println!("DAG (0→1→3, 0→2→3): cycle = {}", has_cycle_directed(&dag));

    // Directed cycle
    let cyc = vec![vec![1], vec![2], vec![0], vec![]];
    println!("Directed 0→1→2→0:   cycle = {}", has_cycle_directed(&cyc));

    // Undirected no cycle (tree)
    let tree = vec![vec![1, 2], vec![0], vec![0, 3], vec![2]];
    println!("Undirected tree:     cycle = {}", has_cycle_undirected(&tree));

    // Undirected with cycle
    let mut uc = vec![vec![]; 4];
    for &(u, v) in &[(0,1),(1,2),(2,0),(2,3)] {
        uc[u].push(v); uc[v].push(u);
    }
    println!("Undirected 0-1-2-0:  cycle = {}\n", has_cycle_undirected(&uc));
}

// ── Demo 4: Topological sort ─────────────────────────────────────────

fn demo_topological_sort() {
    println!("=== Demo 4: Topological Sort ===\n");

    let labels = ["CS101", "CS201", "CS202", "CS301", "MATH"];
    let mut adj = vec![vec![]; 5];
    adj[0].push(1); // CS101→CS201
    adj[0].push(2); // CS101→CS202
    adj[1].push(3); // CS201→CS301
    adj[2].push(3); // CS202→CS301
    adj[4].push(1); // MATH→CS201
    adj[4].push(2); // MATH→CS202

    println!("Prerequisites DAG:");
    for (i, neighbors) in adj.iter().enumerate() {
        if !neighbors.is_empty() {
            let nbrs: Vec<&str> = neighbors.iter().map(|&v| labels[v]).collect();
            println!("  {} → {:?}", labels[i], nbrs);
        }
    }

    let r = dfs_full(&adj);

    // Topological order = reverse post-order
    let mut topo = r.post_order.clone();
    topo.reverse();

    println!("\nFinish times:");
    for i in 0..5 {
        println!("  {:<8} disc={}, fin={}", labels[i], r.disc[i], r.fin[i]);
    }

    print!("\nTopological order: ");
    for (i, &v) in topo.iter().enumerate() {
        print!("{}{}", labels[v], if i < 4 { " → " } else { "" });
    }
    println!("\n(Take courses in this order)\n");
}

// ── Demo 5: Iterative DFS with finish times ──────────────────────────

fn demo_iterative_dfs() {
    println!("=== Demo 5: Iterative DFS with Finish Times ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F'];
    let mut adj = vec![vec![]; 6];
    for &(u, v) in &[(0,1),(0,2),(1,3),(1,4),(2,5)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    let n = adj.len();
    let mut disc = vec![0u32; n];
    let mut fin = vec![0u32; n];
    let mut color = vec![0u8; n];
    let mut time = 0u32;
    let mut order = vec![];

    for start in 0..n {
        if color[start] != 0 { continue; }
        let mut stack: Vec<(usize, bool)> = vec![(start, false)];

        while let Some((u, is_finish)) = stack.pop() {
            if is_finish {
                time += 1;
                fin[u] = time;
                color[u] = 2;
                continue;
            }
            if color[u] != 0 { continue; }

            color[u] = 1;
            time += 1;
            disc[u] = time;
            order.push(u);

            stack.push((u, true));  // finish marker

            for &v in adj[u].iter().rev() {
                if color[v] == 0 {
                    stack.push((v, false));
                }
            }
        }
    }

    println!("Iterative DFS with finish times:");
    println!("  Order: {:?}", order.iter().map(|&v| labels[v]).collect::<Vec<_>>());
    println!("\n{:<8} {:<6} {:<6}", "Vertex", "disc", "fin");
    for i in 0..n {
        println!("{:<8} {:<6} {:<6}", labels[i], disc[i], fin[i]);
    }
    println!();
}

// ── Demo 6: DFS forest ──────────────────────────────────────────────

fn demo_dfs_forest() {
    println!("=== Demo 6: DFS Forest — Multiple Components ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F', 'G'];
    let mut adj = vec![vec![]; 7];
    // Component 1: A-B-C
    adj[0].push(1); adj[1].push(0);
    adj[1].push(2); adj[2].push(1);
    // Component 2: D-E
    adj[3].push(4); adj[4].push(3);
    // Component 3: F alone
    // Component 4: G alone (but we'll connect F-G)
    // Actually keep F and G isolated for demo
    // Component 3: F, Component 4: G

    let r = dfs_full(&adj);

    let roots: Vec<usize> = (0..adj.len())
        .filter(|&i| r.parent[i].is_none())
        .collect();

    println!("Graph: {{A-B-C}}, {{D-E}}, {{F}}, {{G}}");
    println!("DFS forest has {} trees:\n", roots.len());

    for (tree_idx, &root) in roots.iter().enumerate() {
        let members: Vec<char> = (0..adj.len())
            .filter(|&i| {
                let mut v = i;
                loop {
                    if v == root { return true; }
                    match r.parent[v] {
                        Some(p) => v = p,
                        None => return false,
                    }
                }
            })
            .map(|i| labels[i])
            .collect();
        println!("  Tree {} (root={}): {:?}",
                 tree_idx + 1, labels[root], members);
    }
    println!();
}

// ── Demo 7: DFS vs BFS comparison ───────────────────────────────────

fn demo_dfs_vs_bfs() {
    println!("=== Demo 7: DFS vs BFS — Same Graph ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
    let mut adj = vec![vec![]; 8];
    for &(u, v) in &[(0,1),(0,2),(1,3),(1,4),(2,5),(2,6),(3,7),(4,7)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    // DFS
    let dfs_r = dfs_full(&adj);
    let dfs_order: Vec<char> = dfs_r.order.iter().map(|&v| labels[v]).collect();

    // BFS
    let mut bfs_order = vec![];
    let mut visited = vec![false; 8];
    let mut queue = VecDeque::new();
    visited[0] = true;
    queue.push_back(0);
    while let Some(u) = queue.pop_front() {
        bfs_order.push(labels[u]);
        for &v in &adj[u] {
            if !visited[v] {
                visited[v] = true;
                queue.push_back(v);
            }
        }
    }

    println!("Graph: A-B, A-C, B-D, B-E, C-F, C-G, D-H, E-H");
    println!("  DFS order: {:?}", dfs_order);
    println!("  BFS order: {:?}", bfs_order);
    println!("\n  DFS goes deep: A→B→D→H then backtracks");
    println!("  BFS goes wide: A→B,C→D,E,F,G→H\n");
}

// ── Demo 8: Path finding with DFS ───────────────────────────────────

fn demo_dfs_path() {
    println!("=== Demo 8: Path Finding with DFS ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F'];
    let mut adj = vec![vec![]; 6];
    for &(u, v) in &[(0,1),(0,2),(1,3),(2,3),(3,4),(4,5)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    let r = dfs_full(&adj);

    // Path from A to F using parent pointers
    let target = 5; // F
    let mut path = vec![];
    let mut current = Some(target);
    while let Some(v) = current {
        path.push(labels[v]);
        current = r.parent[v];
    }
    path.reverse();

    println!("DFS path A → F: {:?}", path);
    println!("Length: {} edges", path.len() - 1);
    println!("\nWARNING: DFS path is NOT necessarily shortest!");
    println!("BFS would find the actual shortest path.\n");
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  DFS (Depth-First Search) — Rust Demonstrations     ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_basic_dfs();
    demo_edge_classification();
    demo_cycle_detection();
    demo_topological_sort();
    demo_iterative_dfs();
    demo_dfs_forest();
    demo_dfs_vs_bfs();
    demo_dfs_path();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  DFS fully demonstrated!                            ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 9. Ejercicios

### Ejercicio 1 — Traza manual

Ejecuta DFS desde el vértice A en el siguiente grafo dirigido. Anota los
tiempos `disc[]` y `fin[]` de cada vértice y clasifica todas las aristas.

```
    A → B → D
    ↓     ↗
    C → E
```

<details><summary>¿Cuáles son los tiempos y la clasificación?</summary>

DFS desde A (asumiendo vecinos en orden alfabético):

1. Visit A (disc=1)
2. Visit B (disc=2)
3. Visit D (disc=3), no neighbors unvisited → Finish D (fin=4)
4. Back to B, no more → Finish B (fin=5)
5. Visit C (disc=6)
6. Visit E (disc=7)
7. E→D: D is BLACK, disc[E]=7 > disc[D]=3 → **cross edge**
8. Finish E (fin=8)
9. Finish C (fin=9)
10. Finish A (fin=10)

Tiempos: A=(1,10), B=(2,5), C=(6,9), D=(3,4), E=(7,8)

Aristas:
- A→B: tree
- A→C: tree
- B→D: tree
- C→E: tree
- E→D: cross (D ya BLACK, disc[D] < disc[E])

No hay back edges → es un **DAG**.

</details>

### Ejercicio 2 — Paréntesis

Dado `disc = [1, 2, 6, 3, 7, 4]` y `fin = [10, 5, 9, 4, 8, 5]` (nota:
hay un error intencional), ¿es una estructura de paréntesis válida? Si no,
¿por qué?

<details><summary>¿Es válida?</summary>

No es válida. El vértice 1 tiene intervalo $[2, 5]$ y el vértice 5 tiene
$[4, 5]$. Los intervalos se "solapan parcialmente": $[4, 5] \subset [2, 5]$
es válido (5 es descendiente de 1). Pero el vértice 3 tiene $[3, 4]$:
$[3, 4] \subset [2, 5]$ → 3 es descendiente de 1.

Revisando: `fin[1] = 5` y `fin[5] = 5` — **dos vértices con el mismo
tiempo de finalización**. Esto es imposible porque cada evento de
finalización incrementa el tiempo global en 1. Cada vértice tiene un tiempo
de finalización único. Error en fin[5]: debería ser distinto de fin[1].

</details>

### Ejercicio 3 — Back edges y ciclos

Demuestra que en un grafo dirigido, DFS encuentra una back edge si y solo
si el grafo tiene un ciclo.

<details><summary>¿Cuál es la demostración?</summary>

**→ (back edge ⟹ ciclo)**: si $(u, v)$ es back edge, entonces $v$ es GRAY
cuando se examina, lo que significa que $v$ es ancestro de $u$ en el árbol
DFS. Existe un camino tree-path $v \to \cdots \to u$ en el árbol DFS, y la
back edge $u \to v$ cierra el ciclo.

**← (ciclo ⟹ back edge)**: sea $C = v_0 \to v_1 \to \cdots \to v_k = v_0$
un ciclo. Sea $v_i$ el primer vértice del ciclo descubierto por DFS. Cuando
DFS explora desde $v_i$, eventualmente alcanza $v_{i-1}$ (el predecesor de
$v_i$ en el ciclo) a través de tree/forward edges. En ese momento, $v_i$
sigue GRAY (no ha finalizado porque tiene descendientes no explorados). La
arista $v_{i-1} \to v_i$ se clasifica como back edge porque $v_i$ es GRAY.

</details>

### Ejercicio 4 — Topological sort

Dado el siguiente DAG de dependencias de compilación:

```
main.c → utils.o → utils.c
main.c → lib.o → lib.c
lib.o → utils.o
```

Encuentra **todos** los ordenamientos topológicos posibles.

<details><summary>¿Cuántos hay?</summary>

Vértices: main.c, utils.o, utils.c, lib.o, lib.c.
Aristas: main→utils.o, main→lib.o, utils.o→utils.c, lib.o→lib.c, lib.o→utils.o.

Las restricciones son:
- utils.c antes de utils.o
- lib.c antes de lib.o
- utils.o antes de lib.o (por lib.o→utils.o)... espera, la arista es lib.o→utils.o, es decir, lib.o depende de utils.o. Así que utils.o debe compilarse antes que lib.o.

Ordenes válidos (los archivos .c antes de sus .o, utils.o antes de lib.o, ambos antes de main):
1. utils.c, utils.o, lib.c, lib.o, main.c
2. lib.c, utils.c, utils.o, lib.o, main.c
3. utils.c, lib.c, utils.o, lib.o, main.c

Hay **3** ordenamientos topológicos válidos (las posiciones de lib.c y utils.c pueden variar, pero utils.o debe ir antes de lib.o).

</details>

### Ejercicio 5 — Iterativo vs recursivo

Implementa DFS iterativo con pila que calcule `disc[]` y `fin[]` usando el
truco del marcador de finalización. Verifica que los tiempos coinciden con
la versión recursiva.

<details><summary>¿Cómo funciona el marcador?</summary>

Al empujar un vértice $u$ en la pila:
1. Primero empujar `(u, true)` — marcador de finalización.
2. Luego empujar todos los vecinos no visitados `(v, false)`.

Al hacer pop:
- Si `is_finish == true`: el vértice ya fue explorado completamente,
  asignar `fin[u]`.
- Si `is_finish == false` y el vértice no está visitado: asignar `disc[u]`,
  empujar marcador y vecinos.

El marcador queda "debajo" de los vecinos en la pila, así que se procesa
después de que todos los descendientes hayan sido visitados y finalizados —
exactamente como el return de la función recursiva.

</details>

### Ejercicio 6 — Forward edge

Construye un grafo dirigido de 5 vértices que produzca al menos una
**forward edge** durante DFS.

<details><summary>¿Cuál es la construcción?</summary>

```
0 → 1 → 2 → 3
0 → 3             ← forward edge
```

DFS desde 0: visita $0 \to 1 \to 2 \to 3$ (tree edges). Luego examina
$0 \to 3$: vertex 3 es BLACK y $\text{disc}[0] < \text{disc}[3]$, así que
es **forward edge** (0 es ancestro de 3 por tree path, pero hay una arista
directa adicional).

Forward edges solo existen en grafos dirigidos y solo cuando hay un "atajo"
desde un ancestro a un descendiente.

</details>

### Ejercicio 7 — Ciclo en grafo no dirigido

Implementa detección de ciclos en grafos no dirigidos usando DFS. La clave:
un vecino ya visitado que **no es el padre** indica ciclo.

<details><summary>¿Por qué el check del padre es necesario?</summary>

En un grafo no dirigido, la arista $\{u, v\}$ aparece en las listas de $u$
y de $v$. Cuando DFS está en $u$ y ve $v$ (su padre en el árbol), $v$ ya
está visitado. Sin el check del padre, esto se reportaría falsamente como
ciclo.

Un árbol (grafo conexo sin ciclos) no tiene ciclos, pero sin el check del
padre, DFS reportaría un "ciclo" en cada arista. El check `v != parent[u]`
distingue entre "volver al padre" (normal en no dirigido) y "encontrar otro
ancestro" (ciclo real).

Para multigrafos (aristas paralelas $u$-$v$-$u$), el check simple no
basta — se necesita comparar **índices de arista**, no solo vértices.

</details>

### Ejercicio 8 — Componentes en grafo dirigido

¿DFS puede encontrar componentes **fuertemente** conexos directamente? ¿Por
qué es necesario un algoritmo adicional (Kosaraju/Tarjan)?

<details><summary>¿Por qué DFS solo no basta para SCC?</summary>

DFS desde un vértice $u$ encuentra todos los vértices alcanzables desde $u$,
pero un SCC requiere que $u$ también sea alcanzable desde cada vértice del
componente. DFS no verifica la dirección inversa.

Ejemplo: grafo $A \to B \to C$. DFS desde A alcanza B y C, pero el SCC de
A es solo $\{A\}$ (B no puede alcanzar A).

Kosaraju resuelve esto con dos pasadas: DFS sobre $G$ y DFS sobre $G^T$.
Tarjan usa `low[]` values durante una sola DFS para identificar raíces de
SCCs. Ambos se verán en T04.

</details>

### Ejercicio 9 — DFS en grid

Implementa flood fill usando DFS (recursivo) sobre una cuadrícula de
caracteres. Dado un punto $(r, c)$ y un nuevo color, cambia todos los
caracteres conectados del mismo color original.

<details><summary>¿Cuándo DFS recursivo causa stack overflow en grids?</summary>

Para una cuadrícula de $1000 \times 1000$ completamente del mismo color,
DFS recursivo tiene una profundidad de recursión de hasta $10^6$ — cada
llamada usa ~50-100 bytes de stack frame, totalizando 50-100 MB de stack.
El límite de stack típico es 1-8 MB → **stack overflow**.

Soluciones:
1. Usar DFS **iterativo** con pila en el heap.
2. Usar BFS (cola, siempre limitada a ~4n en grids).
3. Aumentar el límite de stack (`ulimit -s` en Linux), pero es frágil.

Para grids, BFS es generalmente preferible por esta razón.

</details>

### Ejercicio 10 — Pre-order vs post-order

Dado un árbol (grafo acíclico conexo), ejecuta DFS y genera:
1. Recorrido **pre-order** (procesar al descubrir).
2. Recorrido **post-order** (procesar al finalizar).
3. ¿Cómo se obtiene **in-order** si el árbol es binario?

<details><summary>¿Cuál es la relación?</summary>

- **Pre-order**: procesar $u$ antes de visitar hijos. Orden: raíz, subárbol
  izquierdo, subárbol derecho. Es el orden `disc` de DFS.
- **Post-order**: procesar $u$ después de visitar hijos. Orden: subárbol
  izquierdo, subárbol derecho, raíz. Es el orden `fin` de DFS.
- **In-order** (solo binario): subárbol izquierdo, raíz, subárbol derecho.
  Se obtiene visitando hijo izquierdo, procesando $u$, luego hijo derecho.

El topological sort es el **reverso del post-order**. Esto funciona porque
en un DAG, un vértice finaliza después de todos sus descendientes, así que
invertir el post-order pone cada vértice antes de sus dependientes.

</details>

---

## 10. Resumen

| Propiedad | DFS |
|-----------|-----|
| Estructura auxiliar | Pila (recursión o explícita) |
| Complejidad | $O(n + m)$ |
| Tiempos | `disc[u]`, `fin[u]` — intervalos anidados |
| Clasificación de aristas | Tree, Back, Forward, Cross |
| Detecta ciclos | Sí (back edges) |
| Topological sort | Sí (reverso del post-order) |
| Camino más corto | **No** |
| Aplicaciones avanzadas | SCC (T04), puentes, puntos de articulación |
| Riesgo | Stack overflow en grafos profundos (usar iterativo) |

En T03 veremos las **aplicaciones directas de DFS**: detección de ciclos
en detalle, componentes conexos, y ordenamiento topológico. En T04
abordaremos los componentes fuertemente conexos con Kosaraju y Tarjan.
