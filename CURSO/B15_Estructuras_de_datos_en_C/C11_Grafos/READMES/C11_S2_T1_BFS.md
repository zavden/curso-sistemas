# T01 — BFS (Breadth-First Search)

## Objetivo

Dominar el recorrido por amplitud (BFS): su mecánica con cola FIFO, la
construcción del árbol BFS, el cálculo de distancias (camino más corto en
grafos no ponderados), y su implementación eficiente en C y Rust.

---

## 1. Intuición

BFS explora un grafo **por niveles**: primero visita todos los vecinos
directos del origen (distancia 1), luego todos los vecinos de esos vecinos
(distancia 2), y así sucesivamente, como una onda expandiéndose desde una
piedra lanzada al agua.

```
Grafo:                 Orden BFS desde A:
  A --- B --- E        Nivel 0: A
  |     |              Nivel 1: B, C      (vecinos de A)
  C --- D --- F        Nivel 2: D, E      (vecinos de B, C no visitados)
                       Nivel 3: F         (vecino de D no visitado)
```

Esta exploración por niveles garantiza que la primera vez que BFS alcanza un
vértice, lo hace por el **camino más corto** (en número de aristas). Esta
propiedad es la razón fundamental por la que BFS es útil.

---

## 2. Algoritmo

### Pseudocódigo

```
BFS(G, source):
    for each vertex u in V:
        dist[u] ← ∞
        parent[u] ← NIL

    dist[source] ← 0
    queue ← empty FIFO queue
    enqueue(queue, source)

    while queue is not empty:
        u ← dequeue(queue)
        for each neighbor v of u:
            if dist[v] == ∞:          // not visited
                dist[v] ← dist[u] + 1
                parent[v] ← u
                enqueue(queue, v)
```

### Invariantes

En todo momento durante la ejecución:

1. **Los vértices en la cola tienen distancia $d$ o $d+1$** para algún $d$.
   La cola nunca contiene vértices con diferencia de distancia mayor que 1.
   Esto se conoce como la **propiedad de dos niveles** de la cola BFS.

2. **Un vértice se encola exactamente una vez**. Una vez que `dist[v]` se
   asigna, nunca cambia. No hay "relajación" como en Dijkstra.

3. **`dist[v]` es la distancia mínima** desde `source` a `v` en número de
   aristas.

### Complejidad

| Aspecto | Con lista de adyacencia | Con matriz |
|---------|:-----------------------:|:----------:|
| Tiempo | $O(n + m)$ | $O(n^2)$ |
| Espacio | $O(n)$ (cola + arrays) | $O(n)$ |

Con lista de adyacencia, cada vértice se desencola una vez ($O(n)$) y cada
arista se examina una vez por cada extremo ($O(m)$). Total: $O(n + m)$.

Con matriz, "listar vecinos" de cada vértice cuesta $O(n)$, así que el
total es $O(n \cdot n) = O(n^2)$.

---

## 3. Traza paso a paso

```
Grafo (no dirigido):
  0 --- 1 --- 4
  |     |     |
  2 --- 3 --- 5

BFS desde 0:

Paso  | Dequeue | Cola (después)     | dist[]              | parent[]
------+---------+--------------------+---------------------+------------------
init  |    -    | [0]                | [0,∞,∞,∞,∞,∞]       | [-,-,-,-,-,-]
  1   |    0    | [1, 2]             | [0,1,1,∞,∞,∞]       | [-,0,0,-,-,-]
  2   |    1    | [2, 3, 4]          | [0,1,1,2,2,∞]       | [-,0,0,1,1,-]
  3   |    2    | [3, 4]             | [0,1,1,2,2,∞]       | [-,0,0,1,1,-]
      |         | (3 already visited)|                      |
  4   |    3    | [4, 5]             | [0,1,1,2,2,3]       | [-,0,0,1,1,3]
  5   |    4    | [5]                | [0,1,1,2,2,3]       | [-,0,0,1,1,3]
      |         | (5 already visited)|                      |
  6   |    5    | []                 | [0,1,1,2,2,3]       | [-,0,0,1,1,3]
```

Resultado: `dist = [0, 1, 1, 2, 2, 3]`.

---

## 4. Árbol BFS

El array `parent[]` define un **árbol BFS** enraizado en `source`. Cada
vértice (excepto `source`) tiene exactamente un padre, y el camino desde
`source` a cualquier vértice $v$ siguiendo los punteros `parent` es el
camino más corto.

```
Árbol BFS desde 0 (del ejemplo anterior):

        0
       / \
      1   2
     / \
    3   4
    |
    5

parent = [-1, 0, 0, 1, 1, 3]
```

### Reconstruir el camino

Para obtener el camino más corto de `source` a `target`, se sigue
`parent[]` desde `target` hasta `source` y se invierte:

```rust
fn reconstruct_path(parent: &[Option<usize>], target: usize) -> Vec<usize> {
    let mut path = vec![];
    let mut current = Some(target);
    while let Some(v) = current {
        path.push(v);
        current = parent[v];
    }
    path.reverse();
    path
}
```

En C:

```c
int reconstruct_path(const int *parent, int target, int *path) {
    int len = 0;
    for (int v = target; v != -1; v = parent[v])
        path[len++] = v;
    // Reverse
    for (int i = 0; i < len / 2; i++) {
        int tmp = path[i];
        path[i] = path[len - 1 - i];
        path[len - 1 - i] = tmp;
    }
    return len;
}
```

Complejidad: $O(\text{longitud del camino}) \le O(n)$.

---

## 5. Propiedades del camino más corto

### Teorema

Para todo vértice $v$ alcanzable desde $s$ en un grafo no ponderado:

$$\text{dist}[v] = \delta(s, v)$$

donde $\delta(s, v)$ es la distancia mínima (número de aristas) de $s$ a $v$.

### Demostración (sketch)

Por inducción sobre la distancia $d$:

- **Base**: $d = 0$. Solo `source` tiene distancia 0. Correcto.
- **Paso inductivo**: supongamos que todos los vértices a distancia $\le d$
  tienen su `dist` correcto. Sea $v$ un vértice a distancia $d + 1$. Entonces
  existe un vecino $u$ de $v$ con $\delta(s, u) = d$. Por hipótesis inductiva,
  $u$ ya fue encolado con `dist[u] = d`. Cuando $u$ se desencola, examina a
  $v$. Si $v$ no fue visitado, se le asigna `dist[v] = d + 1 = \delta(s, v)$.
  Si $v$ ya fue visitado, fue con distancia $\le d + 1$ (no puede ser menor
  por la propiedad del camino más corto). En ambos casos, `dist[v]` es
  correcto.

### Vértices no alcanzables

Si BFS termina y `dist[v]` sigue siendo $\infty$ (o $-1$ como centinela),
$v$ no es alcanzable desde `source`. Esto ocurre cuando $v$ está en un
componente conexo diferente.

---

## 6. Variantes y aplicaciones

### BFS en grafos dirigidos

El algoritmo es idéntico — solo se siguen las aristas en su dirección.
La distancia calculada es la distancia dirigida $\delta(s, v)$. Un vértice
puede ser no alcanzable aunque esté en el mismo "componente" si no hay
camino dirigido.

### BFS multi-source

Para encontrar la distancia desde **cualquiera** de varios orígenes, se
encolan todos los orígenes inicialmente con distancia 0:

```rust
// Distances from the nearest source in `sources`
fn multi_source_bfs(adj: &[Vec<usize>], sources: &[usize]) -> Vec<i32> {
    let n = adj.len();
    let mut dist = vec![-1i32; n];
    let mut queue = VecDeque::new();

    for &s in sources {
        dist[s] = 0;
        queue.push_back(s);
    }

    while let Some(u) = queue.pop_front() {
        for &v in &adj[u] {
            if dist[v] == -1 {
                dist[v] = dist[u] + 1;
                queue.push_back(v);
            }
        }
    }
    dist
}
```

Aplicación: en un tablero de ajedrez, encontrar la distancia mínima de cada
casilla a la pieza más cercana.

### BFS por niveles

A veces se necesita procesar todos los vértices de un nivel antes de pasar al
siguiente (ej. contar vértices por nivel, imprimir por niveles):

```rust
fn bfs_by_level(adj: &[Vec<usize>], source: usize) -> Vec<Vec<usize>> {
    let n = adj.len();
    let mut visited = vec![false; n];
    let mut levels: Vec<Vec<usize>> = vec![];
    let mut current_level = vec![source];
    visited[source] = true;

    while !current_level.is_empty() {
        let mut next_level = vec![];
        for &u in &current_level {
            for &v in &adj[u] {
                if !visited[v] {
                    visited[v] = true;
                    next_level.push(v);
                }
            }
        }
        levels.push(current_level);
        current_level = next_level;
    }
    levels
}
```

### BFS 0-1

Para grafos con aristas de peso 0 o 1, se puede usar un **deque** en lugar
de una cola de prioridad: aristas de peso 0 se encolan al frente, aristas de
peso 1 al fondo. Esto da caminos más cortos en $O(n + m)$ sin necesidad de
Dijkstra:

```rust
fn bfs_01(adj: &[Vec<(usize, u32)>], source: usize) -> Vec<u32> {
    let n = adj.len();
    let mut dist = vec![u32::MAX; n];
    let mut deque = VecDeque::new();

    dist[source] = 0;
    deque.push_back(source);

    while let Some(u) = deque.pop_front() {
        for &(v, w) in &adj[u] {
            let new_dist = dist[u] + w;
            if new_dist < dist[v] {
                dist[v] = new_dist;
                if w == 0 {
                    deque.push_front(v);  // weight 0: high priority
                } else {
                    deque.push_back(v);   // weight 1: normal
                }
            }
        }
    }
    dist
}
```

### Aplicaciones clásicas

| Aplicación | Cómo usa BFS |
|-----------|-------------|
| Camino más corto (no ponderado) | BFS directamente |
| Componentes conexos | BFS/DFS desde cada vértice no visitado |
| Verificar bipartito | BFS con 2-coloreo (T01/S01) |
| Distancia mínima a un conjunto | BFS multi-source |
| Número de Kevin Bacon | BFS en grafo de actores/películas |
| Crawling web | BFS desde URL semilla |
| Puzzle solving | BFS en grafo de estados |
| Flood fill | BFS en grid 2D |

---

## 7. BFS en grids 2D

Una aplicación muy frecuente es BFS sobre una cuadrícula (matrix), donde
cada celda es un "vértice" y las celdas adyacentes (arriba, abajo, izquierda,
derecha) son los "vecinos":

```rust
fn bfs_grid(grid: &[Vec<char>], start: (usize, usize)) -> Vec<Vec<i32>> {
    let rows = grid.len();
    let cols = grid[0].len();
    let mut dist = vec![vec![-1i32; cols]; rows];
    let mut queue = VecDeque::new();

    dist[start.0][start.1] = 0;
    queue.push_back(start);

    let dirs: [(i32, i32); 4] = [(-1, 0), (1, 0), (0, -1), (0, 1)];

    while let Some((r, c)) = queue.pop_front() {
        for &(dr, dc) in &dirs {
            let nr = r as i32 + dr;
            let nc = c as i32 + dc;
            if nr >= 0 && nr < rows as i32 && nc >= 0 && nc < cols as i32 {
                let (nr, nc) = (nr as usize, nc as usize);
                if grid[nr][nc] != '#' && dist[nr][nc] == -1 {
                    dist[nr][nc] = dist[r][c] + 1;
                    queue.push_back((nr, nc));
                }
            }
        }
    }
    dist
}
```

No se necesita construir un grafo explícito: las celdas adyacentes son los
vecinos implícitos. La complejidad es $O(\text{rows} \times \text{cols})$.

---

## 8. Programa completo en C

```c
/*
 * bfs.c
 *
 * Breadth-First Search: shortest paths, BFS tree, level-order
 * traversal, multi-source BFS, and grid BFS.
 *
 * Compile: gcc -O2 -o bfs bfs.c
 * Run:     ./bfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_V 100

/* =================== ADJACENCY LIST =============================== */

typedef struct {
    int *dests;
    int size, cap;
} NbrList;

typedef struct {
    NbrList *adj;
    int n;
} Graph;

static void nl_push(NbrList *nl, int v) {
    if (nl->size == nl->cap) {
        nl->cap = nl->cap ? nl->cap * 2 : 4;
        nl->dests = realloc(nl->dests, nl->cap * sizeof(int));
    }
    nl->dests[nl->size++] = v;
}

Graph graph_create(int n) {
    Graph g;
    g.n = n;
    g.adj = calloc(n, sizeof(NbrList));
    return g;
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++) free(g->adj[i].dests);
    free(g->adj);
}

void graph_add_edge(Graph *g, int u, int v) {
    nl_push(&g->adj[u], v);
    nl_push(&g->adj[v], u);
}

void graph_add_directed(Graph *g, int u, int v) {
    nl_push(&g->adj[u], v);
}

/* ========================= BFS ==================================== */

typedef struct {
    int dist[MAX_V];
    int parent[MAX_V];
    int order[MAX_V];   /* visit order */
    int order_len;
} BFSResult;

BFSResult bfs(const Graph *g, int source) {
    BFSResult r;
    r.order_len = 0;
    for (int i = 0; i < g->n; i++) {
        r.dist[i] = -1;
        r.parent[i] = -1;
    }

    int queue[MAX_V], front = 0, back = 0;
    r.dist[source] = 0;
    queue[back++] = source;

    while (front < back) {
        int u = queue[front++];
        r.order[r.order_len++] = u;

        for (int i = 0; i < g->adj[u].size; i++) {
            int v = g->adj[u].dests[i];
            if (r.dist[v] == -1) {
                r.dist[v] = r.dist[u] + 1;
                r.parent[v] = u;
                queue[back++] = v;
            }
        }
    }
    return r;
}

int reconstruct_path(const int *parent, int target, int *path) {
    int len = 0;
    for (int v = target; v != -1; v = parent[v])
        path[len++] = v;
    for (int i = 0; i < len / 2; i++) {
        int tmp = path[i];
        path[i] = path[len - 1 - i];
        path[len - 1 - i] = tmp;
    }
    return len;
}

/* ============================ DEMOS =============================== */

/*
 * Demo 1: Basic BFS — distances and visit order
 */
void demo_basic_bfs(void) {
    printf("=== Demo 1: Basic BFS ===\n\n");

    /*  0 --- 1 --- 4
     *  |     |     |
     *  2 --- 3 --- 5
     */
    Graph g = graph_create(6);
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 0, 2);
    graph_add_edge(&g, 1, 3);
    graph_add_edge(&g, 1, 4);
    graph_add_edge(&g, 2, 3);
    graph_add_edge(&g, 3, 5);
    graph_add_edge(&g, 4, 5);

    BFSResult r = bfs(&g, 0);

    printf("BFS from vertex 0:\n");
    printf("  Visit order: ");
    for (int i = 0; i < r.order_len; i++)
        printf("%d ", r.order[i]);
    printf("\n\n");

    printf("  %-8s %-8s %-8s\n", "Vertex", "Dist", "Parent");
    for (int i = 0; i < g.n; i++)
        printf("  %-8d %-8d %-8d\n", i, r.dist[i], r.parent[i]);

    printf("\n  Shortest path 0 → 5: ");
    int path[MAX_V];
    int plen = reconstruct_path(r.parent, 5, path);
    for (int i = 0; i < plen; i++)
        printf("%d%s", path[i], i < plen - 1 ? " → " : "");
    printf(" (length %d)\n\n", r.dist[5]);

    graph_free(&g);
}

/*
 * Demo 2: BFS tree visualization
 */
void demo_bfs_tree(void) {
    printf("=== Demo 2: BFS Tree ===\n\n");

    /*  A(0)---B(1)---E(4)---G(6)
     *  |      |      |
     *  C(2)   D(3)   F(5)
     *  |
     *  H(7)
     */
    Graph g = graph_create(8);
    graph_add_edge(&g, 0, 1);  /* A-B */
    graph_add_edge(&g, 0, 2);  /* A-C */
    graph_add_edge(&g, 1, 3);  /* B-D */
    graph_add_edge(&g, 1, 4);  /* B-E */
    graph_add_edge(&g, 4, 5);  /* E-F */
    graph_add_edge(&g, 4, 6);  /* E-G */
    graph_add_edge(&g, 2, 7);  /* C-H */

    const char *labels = "ABCDEFGH";
    BFSResult r = bfs(&g, 0);

    printf("BFS tree from A:\n");

    /* Print by levels */
    int max_dist = 0;
    for (int i = 0; i < g.n; i++)
        if (r.dist[i] > max_dist) max_dist = r.dist[i];

    for (int d = 0; d <= max_dist; d++) {
        printf("  Level %d: ", d);
        for (int i = 0; i < g.n; i++) {
            if (r.dist[i] == d) {
                if (r.parent[i] == -1)
                    printf("%c(root) ", labels[i]);
                else
                    printf("%c(←%c) ", labels[i], labels[r.parent[i]]);
            }
        }
        printf("\n");
    }

    /* Tree edges */
    printf("\n  BFS tree edges:\n");
    for (int i = 0; i < g.n; i++) {
        if (r.parent[i] != -1)
            printf("    %c → %c\n", labels[r.parent[i]], labels[i]);
    }
    printf("\n");

    graph_free(&g);
}

/*
 * Demo 3: Shortest paths between all pairs (via repeated BFS)
 */
void demo_all_pairs(void) {
    printf("=== Demo 3: All-Pairs Shortest Paths (via BFS) ===\n\n");

    /*  0---1---2
     *  |       |
     *  3---4---5
     */
    Graph g = graph_create(6);
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 1, 2);
    graph_add_edge(&g, 0, 3);
    graph_add_edge(&g, 3, 4);
    graph_add_edge(&g, 4, 5);
    graph_add_edge(&g, 2, 5);

    printf("Distance matrix (BFS from each vertex):\n");
    printf("     ");
    for (int j = 0; j < g.n; j++) printf("%3d", j);
    printf("\n");

    for (int s = 0; s < g.n; s++) {
        BFSResult r = bfs(&g, s);
        printf("  %d: ", s);
        for (int j = 0; j < g.n; j++)
            printf("%3d", r.dist[j]);
        printf("\n");
    }
    printf("\n  Complexity: O(n(n+m)) — one BFS per vertex\n");
    printf("  For weighted graphs, use Floyd-Warshall instead.\n\n");

    graph_free(&g);
}

/*
 * Demo 4: Multi-source BFS
 */
void demo_multi_source(void) {
    printf("=== Demo 4: Multi-Source BFS ===\n\n");

    /*  Grid 5x5 with two sources at corners
     *  S . . . .
     *  . . . . .
     *  . . . . .
     *  . . . . .
     *  . . . . S
     */
    int rows = 5, cols = 5;
    int n = rows * cols;
    Graph g = graph_create(n);

    /* Connect grid */
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int id = r * cols + c;
            if (c + 1 < cols) graph_add_edge(&g, id, id + 1);
            if (r + 1 < rows) graph_add_edge(&g, id, id + cols);
        }
    }

    /* Multi-source BFS from corners (0,0) and (4,4) */
    int dist[MAX_V], parent[MAX_V];
    for (int i = 0; i < n; i++) { dist[i] = -1; parent[i] = -1; }

    int sources[] = {0, 24};  /* top-left, bottom-right */
    int queue[MAX_V], front = 0, back = 0;
    for (int i = 0; i < 2; i++) {
        dist[sources[i]] = 0;
        queue[back++] = sources[i];
    }

    while (front < back) {
        int u = queue[front++];
        for (int i = 0; i < g.adj[u].size; i++) {
            int v = g.adj[u].dests[i];
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                queue[back++] = v;
            }
        }
    }

    printf("Distance to nearest source (S at corners):\n");
    for (int r = 0; r < rows; r++) {
        printf("  ");
        for (int c = 0; c < cols; c++)
            printf("%3d", dist[r * cols + c]);
        printf("\n");
    }
    printf("\n");

    graph_free(&g);
}

/*
 * Demo 5: BFS on directed graph
 */
void demo_directed_bfs(void) {
    printf("=== Demo 5: BFS on Directed Graph ===\n\n");

    /*  A(0) → B(1) → D(3)
     *  ↓      ↓
     *  C(2)   E(4) → F(5)
     */
    const char *labels = "ABCDEF";
    Graph g = graph_create(6);
    graph_add_directed(&g, 0, 1);  /* A→B */
    graph_add_directed(&g, 0, 2);  /* A→C */
    graph_add_directed(&g, 1, 3);  /* B→D */
    graph_add_directed(&g, 1, 4);  /* B→E */
    graph_add_directed(&g, 4, 5);  /* E→F */

    printf("Directed graph:\n");
    for (int i = 0; i < g.n; i++) {
        printf("  %c →", labels[i]);
        for (int j = 0; j < g.adj[i].size; j++)
            printf(" %c", labels[g.adj[i].dests[j]]);
        printf("\n");
    }

    BFSResult r = bfs(&g, 0);

    printf("\nBFS from A:\n");
    printf("  %-8s %-8s %-12s\n", "Vertex", "Dist", "Reachable?");
    for (int i = 0; i < g.n; i++) {
        printf("  %-8c %-8d %s\n",
               labels[i], r.dist[i],
               r.dist[i] >= 0 ? "YES" : "NO");
    }

    printf("\n  Note: all vertices reachable from A in this graph.\n");

    /* BFS from F — nothing reachable (F is a sink) */
    BFSResult r2 = bfs(&g, 5);
    printf("\nBFS from F (sink):\n");
    int reachable = 0;
    for (int i = 0; i < g.n; i++)
        if (r2.dist[i] >= 0) reachable++;
    printf("  Reachable vertices: %d (only F itself)\n\n", reachable);

    graph_free(&g);
}

/*
 * Demo 6: BFS on grid — maze shortest path
 */
void demo_grid_bfs(void) {
    printf("=== Demo 6: Grid BFS — Maze Shortest Path ===\n\n");

    /* Maze: S = start, E = end, # = wall, . = open */
    const char *maze[] = {
        "S...#.",
        ".#.#..",
        ".#...#",
        "...#..",
        ".#.#.E",
    };
    int rows = 5, cols = 6;

    printf("Maze:\n");
    for (int r = 0; r < rows; r++)
        printf("  %s\n", maze[r]);

    /* BFS on grid */
    int dist[5][6];
    int par_r[5][6], par_c[5][6];
    memset(dist, -1, sizeof(dist));
    memset(par_r, -1, sizeof(par_r));
    memset(par_c, -1, sizeof(par_c));

    int start_r = 0, start_c = 0;
    int end_r = 4, end_c = 5;

    int qr[30], qc[30], front = 0, back = 0;
    dist[start_r][start_c] = 0;
    qr[back] = start_r; qc[back] = start_c; back++;

    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};

    while (front < back) {
        int r = qr[front], c = qc[front]; front++;
        for (int d = 0; d < 4; d++) {
            int nr = r + dr[d], nc = c + dc[d];
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols &&
                maze[nr][nc] != '#' && dist[nr][nc] == -1) {
                dist[nr][nc] = dist[r][c] + 1;
                par_r[nr][nc] = r;
                par_c[nr][nc] = c;
                qr[back] = nr; qc[back] = nc; back++;
            }
        }
    }

    printf("\nDistances from S:\n");
    for (int r = 0; r < rows; r++) {
        printf("  ");
        for (int c = 0; c < cols; c++) {
            if (dist[r][c] == -1)
                printf("  #");
            else
                printf("%3d", dist[r][c]);
        }
        printf("\n");
    }

    /* Reconstruct and display path */
    if (dist[end_r][end_c] == -1) {
        printf("\n  No path from S to E!\n\n");
    } else {
        char result[5][7];
        for (int r = 0; r < rows; r++) {
            strncpy(result[r], maze[r], 6);
            result[r][6] = '\0';
        }

        int r = end_r, c = end_c;
        while (r != start_r || c != start_c) {
            result[r][c] = '*';
            int pr = par_r[r][c], pc = par_c[r][c];
            r = pr; c = pc;
        }
        result[start_r][start_c] = 'S';
        result[end_r][end_c] = 'E';

        printf("\nShortest path (length %d):\n", dist[end_r][end_c]);
        for (int r = 0; r < rows; r++)
            printf("  %s\n", result[r]);
        printf("\n");
    }
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  BFS (Breadth-First Search) — C Demonstrations      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_basic_bfs();
    demo_bfs_tree();
    demo_all_pairs();
    demo_multi_source();
    demo_directed_bfs();
    demo_grid_bfs();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  BFS fully demonstrated!                            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * bfs.rs
 *
 * Breadth-First Search demonstrations in Rust.
 *
 * Run: cargo run --release
 */

use std::collections::VecDeque;

// ── BFS core ─────────────────────────────────────────────────────────

struct BfsResult {
    dist: Vec<i32>,
    parent: Vec<Option<usize>>,
    order: Vec<usize>,
}

fn bfs(adj: &[Vec<usize>], source: usize) -> BfsResult {
    let n = adj.len();
    let mut dist = vec![-1i32; n];
    let mut parent = vec![None; n];
    let mut order = Vec::with_capacity(n);
    let mut queue = VecDeque::new();

    dist[source] = 0;
    queue.push_back(source);

    while let Some(u) = queue.pop_front() {
        order.push(u);
        for &v in &adj[u] {
            if dist[v] == -1 {
                dist[v] = dist[u] + 1;
                parent[v] = Some(u);
                queue.push_back(v);
            }
        }
    }

    BfsResult { dist, parent, order }
}

fn reconstruct_path(parent: &[Option<usize>], target: usize) -> Vec<usize> {
    let mut path = vec![];
    let mut current = Some(target);
    while let Some(v) = current {
        path.push(v);
        current = parent[v];
    }
    path.reverse();
    path
}

// ── Demo 1: Basic BFS ───────────────────────────────────────────────

fn demo_basic_bfs() {
    println!("=== Demo 1: Basic BFS ===\n");

    //  0 --- 1 --- 4
    //  |     |     |
    //  2 --- 3 --- 5
    let mut adj = vec![vec![]; 6];
    for &(u, v) in &[(0,1),(0,2),(1,3),(1,4),(2,3),(3,5),(4,5)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    let r = bfs(&adj, 0);

    println!("BFS from vertex 0:");
    println!("  Visit order: {:?}\n", r.order);

    println!("  {:<8} {:<8} {:<8}", "Vertex", "Dist", "Parent");
    for i in 0..6 {
        println!("  {:<8} {:<8} {:<8}",
                 i, r.dist[i],
                 r.parent[i].map(|p| p.to_string()).unwrap_or("-".into()));
    }

    let path = reconstruct_path(&r.parent, 5);
    println!("\n  Shortest path 0 → 5: {:?} (length {})\n",
             path, r.dist[5]);
}

// ── Demo 2: BFS tree by levels ──────────────────────────────────────

fn demo_bfs_tree() {
    println!("=== Demo 2: BFS Tree by Levels ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
    let mut adj = vec![vec![]; 8];
    for &(u, v) in &[(0,1),(0,2),(1,3),(1,4),(4,5),(4,6),(2,7)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    let r = bfs(&adj, 0);

    let max_dist = *r.dist.iter().max().unwrap();
    println!("BFS tree from A:");
    for d in 0..=max_dist {
        let level: Vec<String> = (0..adj.len())
            .filter(|&i| r.dist[i] == d)
            .map(|i| {
                match r.parent[i] {
                    None => format!("{}(root)", labels[i]),
                    Some(p) => format!("{}(←{})", labels[i], labels[p]),
                }
            })
            .collect();
        println!("  Level {d}: {}", level.join(", "));
    }

    println!("\n  Tree edges:");
    for i in 0..adj.len() {
        if let Some(p) = r.parent[i] {
            println!("    {} → {}", labels[p], labels[i]);
        }
    }
    println!();
}

// ── Demo 3: All-pairs shortest paths via BFS ────────────────────────

fn demo_all_pairs() {
    println!("=== Demo 3: All-Pairs Shortest Paths ===\n");

    //  0---1---2
    //  |       |
    //  3---4---5
    let mut adj = vec![vec![]; 6];
    for &(u, v) in &[(0,1),(1,2),(0,3),(3,4),(4,5),(2,5)] {
        adj[u].push(v);
        adj[v].push(u);
    }

    println!("Distance matrix:");
    print!("     ");
    for j in 0..6 { print!("{j:3}"); }
    println!();

    for s in 0..6 {
        let r = bfs(&adj, s);
        print!("  {s}: ");
        for j in 0..6 {
            print!("{:3}", r.dist[j]);
        }
        println!();
    }
    println!();
}

// ── Demo 4: Multi-source BFS ────────────────────────────────────────

fn demo_multi_source() {
    println!("=== Demo 4: Multi-Source BFS ===\n");

    let rows = 5usize;
    let cols = 5;
    let n = rows * cols;
    let mut adj = vec![vec![]; n];

    for r in 0..rows {
        for c in 0..cols {
            let id = r * cols + c;
            if c + 1 < cols { adj[id].push(id + 1); adj[id + 1].push(id); }
            if r + 1 < rows { adj[id].push(id + cols); adj[id + cols].push(id); }
        }
    }

    // Multi-source from corners
    let sources = [0, 24]; // (0,0) and (4,4)
    let mut dist = vec![-1i32; n];
    let mut queue = VecDeque::new();
    for &s in &sources {
        dist[s] = 0;
        queue.push_back(s);
    }
    while let Some(u) = queue.pop_front() {
        for &v in &adj[u] {
            if dist[v] == -1 {
                dist[v] = dist[u] + 1;
                queue.push_back(v);
            }
        }
    }

    println!("Distance to nearest source (S at corners):");
    for r in 0..rows {
        print!("  ");
        for c in 0..cols {
            print!("{:3}", dist[r * cols + c]);
        }
        println!();
    }
    println!();
}

// ── Demo 5: Directed BFS and reachability ───────────────────────────

fn demo_directed() {
    println!("=== Demo 5: Directed BFS — Reachability ===\n");

    let labels = ['A', 'B', 'C', 'D', 'E', 'F'];
    // Directed: A→B, A→C, B→D, B→E, E→F
    let mut adj = vec![vec![]; 6];
    adj[0].push(1); // A→B
    adj[0].push(2); // A→C
    adj[1].push(3); // B→D
    adj[1].push(4); // B→E
    adj[4].push(5); // E→F

    println!("Directed graph:");
    for (i, neighbors) in adj.iter().enumerate() {
        if !neighbors.is_empty() {
            let nbrs: Vec<char> = neighbors.iter().map(|&v| labels[v]).collect();
            println!("  {} → {:?}", labels[i], nbrs);
        }
    }

    let r = bfs(&adj, 0);
    println!("\nBFS from A:");
    for i in 0..6 {
        println!("  {}: dist={}, reachable={}",
                 labels[i], r.dist[i],
                 if r.dist[i] >= 0 { "YES" } else { "NO" });
    }

    // BFS from F (sink)
    let r2 = bfs(&adj, 5);
    let reachable: usize = r2.dist.iter().filter(|&&d| d >= 0).count();
    println!("\nBFS from F: reachable = {} (only F itself)\n", reachable);
}

// ── Demo 6: Grid BFS — maze solving ─────────────────────────────────

fn demo_grid_bfs() {
    println!("=== Demo 6: Grid BFS — Maze Solving ===\n");

    let maze = vec![
        "S...#.".chars().collect::<Vec<_>>(),
        ".#.#..".chars().collect::<Vec<_>>(),
        ".#...#".chars().collect::<Vec<_>>(),
        "...#..".chars().collect::<Vec<_>>(),
        ".#.#.E".chars().collect::<Vec<_>>(),
    ];
    let rows = maze.len();
    let cols = maze[0].len();

    println!("Maze:");
    for row in &maze {
        println!("  {}", row.iter().collect::<String>());
    }

    let mut dist = vec![vec![-1i32; cols]; rows];
    let mut parent: Vec<Vec<Option<(usize, usize)>>> = vec![vec![None; cols]; rows];
    let mut queue = VecDeque::new();

    let (sr, sc) = (0, 0);
    let (er, ec) = (4, 5);
    dist[sr][sc] = 0;
    queue.push_back((sr, sc));

    let dirs: [(i32, i32); 4] = [(-1,0),(1,0),(0,-1),(0,1)];

    while let Some((r, c)) = queue.pop_front() {
        for &(dr, dc) in &dirs {
            let nr = r as i32 + dr;
            let nc = c as i32 + dc;
            if nr >= 0 && nr < rows as i32 && nc >= 0 && nc < cols as i32 {
                let (nr, nc) = (nr as usize, nc as usize);
                if maze[nr][nc] != '#' && dist[nr][nc] == -1 {
                    dist[nr][nc] = dist[r][c] + 1;
                    parent[nr][nc] = Some((r, c));
                    queue.push_back((nr, nc));
                }
            }
        }
    }

    println!("\nDistances from S:");
    for r in 0..rows {
        print!("  ");
        for c in 0..cols {
            if dist[r][c] == -1 { print!("  #"); }
            else { print!("{:3}", dist[r][c]); }
        }
        println!();
    }

    if dist[er][ec] >= 0 {
        let mut result = maze.clone();
        let (mut r, mut c) = (er, ec);
        while (r, c) != (sr, sc) {
            result[r][c] = '*';
            let (pr, pc) = parent[r][c].unwrap();
            r = pr; c = pc;
        }
        result[sr][sc] = 'S';
        result[er][ec] = 'E';

        println!("\nShortest path (length {}):", dist[er][ec]);
        for row in &result {
            println!("  {}", row.iter().collect::<String>());
        }
    }
    println!();
}

// ── Demo 7: BFS 0-1 ─────────────────────────────────────────────────

fn demo_bfs_01() {
    println!("=== Demo 7: BFS 0-1 (Deque-based) ===\n");

    // Graph with edge weights 0 or 1
    // Represents: walking on roads (weight 1) with free teleporters (weight 0)
    let n = 6;
    let mut adj: Vec<Vec<(usize, u32)>> = vec![vec![]; n];
    // Normal edges (weight 1)
    adj[0].push((1, 1)); adj[1].push((0, 1));
    adj[1].push((2, 1)); adj[2].push((1, 1));
    adj[2].push((3, 1)); adj[3].push((2, 1));
    adj[3].push((4, 1)); adj[4].push((3, 1));
    adj[4].push((5, 1)); adj[5].push((4, 1));
    // Teleporter: 0 → 4 (weight 0!)
    adj[0].push((4, 0)); adj[4].push((0, 0));

    println!("Graph with 0-1 weights:");
    for (u, neighbors) in adj.iter().enumerate() {
        let nbrs: Vec<String> = neighbors.iter()
            .map(|&(v, w)| format!("{v}(w={w})"))
            .collect();
        println!("  {u} → [{}]", nbrs.join(", "));
    }

    // BFS 0-1 with deque
    let mut dist = vec![u32::MAX; n];
    let mut deque = VecDeque::new();
    dist[0] = 0;
    deque.push_back(0);

    while let Some(u) = deque.pop_front() {
        for &(v, w) in &adj[u] {
            let new_dist = dist[u] + w;
            if new_dist < dist[v] {
                dist[v] = new_dist;
                if w == 0 {
                    deque.push_front(v);
                } else {
                    deque.push_back(v);
                }
            }
        }
    }

    println!("\nShortest distances from 0:");
    for i in 0..n {
        println!("  0 → {i}: {}", dist[i]);
    }
    println!("\n  Without teleporter: 0→5 = 5 steps");
    println!("  With teleporter 0↔4 (w=0): 0→5 = {} step(s)\n", dist[5]);
}

// ── Demo 8: Connected components via BFS ─────────────────────────────

fn demo_components() {
    println!("=== Demo 8: Connected Components via BFS ===\n");

    // 3 components: {0,1,2}, {3,4}, {5}
    let mut adj = vec![vec![]; 6];
    adj[0].push(1); adj[1].push(0);
    adj[1].push(2); adj[2].push(1);
    adj[0].push(2); adj[2].push(0);
    adj[3].push(4); adj[4].push(3);
    // 5 is isolated

    let mut visited = vec![false; adj.len()];
    let mut components: Vec<Vec<usize>> = vec![];

    for start in 0..adj.len() {
        if visited[start] { continue; }
        let mut component = vec![];
        let mut queue = VecDeque::new();
        visited[start] = true;
        queue.push_back(start);

        while let Some(u) = queue.pop_front() {
            component.push(u);
            for &v in &adj[u] {
                if !visited[v] {
                    visited[v] = true;
                    queue.push_back(v);
                }
            }
        }
        components.push(component);
    }

    println!("Graph: 0-1, 1-2, 0-2, 3-4, 5(isolated)");
    println!("Components found: {}", components.len());
    for (i, comp) in components.iter().enumerate() {
        println!("  Component {}: {:?}", i + 1, comp);
    }
    println!();
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  BFS (Breadth-First Search) — Rust Demonstrations   ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_basic_bfs();
    demo_bfs_tree();
    demo_all_pairs();
    demo_multi_source();
    demo_directed();
    demo_grid_bfs();
    demo_bfs_01();
    demo_components();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  BFS fully demonstrated!                            ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Traza manual

Dado el siguiente grafo no dirigido, ejecuta BFS desde el vértice A a mano.
Escribe la cola, `dist[]` y `parent[]` en cada paso.

```
    A --- B --- F
    |     |
    C --- D --- E
          |
          G
```

<details><summary>¿Cuál es la distancia de A a G?</summary>

BFS desde A:
- Nivel 0: A (dist=0)
- Nivel 1: B, C (vecinos de A, dist=1)
- Nivel 2: D, F (B→D, B→F; C→D ya visitado, dist=2)
- Nivel 3: E, G (D→E, D→G, dist=3)

$\text{dist}(A, G) = 3$. Camino: $A \to B \to D \to G$ o $A \to C \to D \to G$ (ambos de longitud 3).

</details>

### Ejercicio 2 — BFS vs DFS para camino más corto

Explica con un contraejemplo por qué DFS **no** encuentra el camino más
corto en un grafo no ponderado.

<details><summary>¿Cuál es el contraejemplo más simple?</summary>

```
    0 --- 1
    |     |
    2 --- 3
```

DFS desde 0 podría seguir el camino $0 \to 1 \to 3$ (longitud 2) y marcar
3 como "distancia 2". Pero el camino $0 \to 2 \to 3$ también tiene longitud
2. El problema real aparece con:

```
    0 --- 1 --- 2 --- 3
    |                 |
    └─────────────────┘
```

DFS desde 0 podría seguir $0 \to 1 \to 2 \to 3$ (longitud 3), pero la
arista directa $0 \to 3$ (longitud 1) es más corta. DFS no garantiza
encontrar primero el camino más corto porque explora en profundidad, no por
niveles.

</details>

### Ejercicio 3 — Diámetro del grafo

El **diámetro** de un grafo es la mayor distancia mínima entre cualquier par
de vértices. Implementa una función que calcule el diámetro ejecutando BFS
desde cada vértice.

<details><summary>¿Cuál es la complejidad?</summary>

Se ejecuta BFS $n$ veces, cada una con costo $O(n + m)$. Total: $O(n(n + m))$.

Para el grafo del ejercicio 1: las distancias máximas desde cada vértice
son $\max(\text{dist desde A}) = 3$ (A→G), y similar desde otros. El
diámetro es $\max$ sobre todos los BFS $= 4$ (distancia F→G: $F \to B \to D \to G$).

Para grafos muy grandes, calcular el diámetro exacto es costoso. Una
aproximación común es hacer BFS desde un vértice aleatorio, luego BFS desde
el vértice más lejano encontrado — el resultado es el diámetro para árboles
y una buena aproximación para grafos generales.

</details>

### Ejercicio 4 — Bipartito con BFS

Implementa la verificación de bipartiteness usando BFS con 2-coloreo: al
descubrir un vecino ya visitado, verificar que tiene color opuesto.

<details><summary>¿Cuándo falla el 2-coloreo?</summary>

Falla cuando se encuentra una arista $(u, v)$ donde $u$ y $v$ tienen el
**mismo** color. Esto implica que hay un ciclo de longitud impar (la arista
$(u,v)$ cierra un camino de longitud par en el árbol BFS, creando un ciclo
de longitud impar total).

La implementación usa `color[v] = 1 - color[u]` al descubrir $v$. Si $v$
ya fue descubierto y `color[v] == color[u]`, el grafo no es bipartito.

</details>

### Ejercicio 5 — Multi-source BFS

Dada una cuadrícula con múltiples "fuegos" (celdas marcadas con 'F'),
calcula la distancia mínima de cada celda vacía al fuego más cercano usando
multi-source BFS.

<details><summary>¿Por qué multi-source BFS es correcto aquí?</summary>

Al encolar todos los fuegos con distancia 0, BFS explora desde todos
simultáneamente, como si hubiera un "super-source" conectado a todos los
fuegos con aristas de peso 0. La primera vez que BFS alcanza una celda, lo
hace desde el fuego más cercano — por la misma propiedad de camino más corto
de BFS.

La complejidad es $O(\text{rows} \times \text{cols})$ — igual que un solo
BFS, no importa cuántos fuegos haya.

</details>

### Ejercicio 6 — BFS 0-1

En una cuadrícula, moverse a una celda del mismo color cuesta 0 y a una
celda de color diferente cuesta 1. Implementa BFS 0-1 con deque para
encontrar el costo mínimo de esquina a esquina.

<details><summary>¿Por qué no funciona BFS normal aquí?</summary>

BFS normal asume que todas las aristas tienen peso 1. Con pesos 0 y 1, BFS
normal podría encontrar un camino con más aristas pero menor peso total.

Ejemplo: un camino de 5 aristas de peso 0 (costo total 0) es mejor que un
camino de 1 arista de peso 1 (costo total 1). BFS normal reportaría el
segundo como "más corto" por tener menos aristas.

BFS 0-1 resuelve esto usando un deque: las aristas de peso 0 se encolan al
frente (alta prioridad), las de peso 1 al fondo. Esto mantiene la propiedad
de que los vértices se procesan en orden de distancia creciente.

</details>

### Ejercicio 7 — Camino más corto con restricción

Modifica BFS para encontrar el camino más corto de $s$ a $t$ que pase por
un vértice obligatorio $m$. Pista: BFS desde $s$, BFS desde $m$, sumar
distancias.

<details><summary>¿Cuál es la fórmula?</summary>

El camino más corto $s \to m \to t$ tiene longitud:

$$\delta(s, m) + \delta(m, t)$$

Se calculan dos BFS: uno desde $s$ (para $\delta(s, m)$) y otro desde $m$
(para $\delta(m, t)$). Si $m$ no es alcanzable desde $s$ o $t$ no es
alcanzable desde $m$, no hay camino.

Este truco funciona porque en grafos no ponderados, el camino más corto que
pasa por un punto intermedio se puede descomponer en dos subcaminos óptimos
(propiedad de subestructura óptima).

</details>

### Ejercicio 8 — Número de caminos más cortos

Modifica BFS para contar **cuántos** caminos más cortos existen desde
`source` a cada vértice. Pista: cuando un vecino tiene la misma distancia
que `dist[u] + 1`, incrementa el contador en lugar de ignorarlo.

<details><summary>¿Cómo se modifica el algoritmo?</summary>

Agregar un array `count[n]` inicializado a 0, con `count[source] = 1`.
Al examinar la arista $(u, v)$:

```
if dist[v] == -1:
    dist[v] = dist[u] + 1
    count[v] = count[u]      // v inherits u's count
    enqueue(v)
else if dist[v] == dist[u] + 1:
    count[v] += count[u]     // alternative path found
```

Al final, `count[v]` = número de caminos más cortos de `source` a `v`.
La complejidad sigue siendo $O(n + m)$.

</details>

### Ejercicio 9 — BFS en grafo implícito

Dado un número entero $n$, encuentra el mínimo número de operaciones
$\{+1, \times 2\}$ para llegar desde 1 hasta $n$. Modela como BFS en
un grafo implícito donde cada estado es un número y las aristas son las
operaciones.

<details><summary>¿Cuántas operaciones se necesitan para n = 10?</summary>

$1 \xrightarrow{\times 2} 2 \xrightarrow{+1} 3 \xrightarrow{+1} 4 \xrightarrow{+1} 5 \xrightarrow{\times 2} 10$

5 operaciones. Pero hay un camino más corto:
$1 \xrightarrow{+1} 2 \xrightarrow{+1} 3 \xrightarrow{+1} 4 \xrightarrow{+1} 5 \xrightarrow{\times 2} 10$ — también 5.

O: $1 \xrightarrow{\times 2} 2 \xrightarrow{\times 2} 4 \xrightarrow{+1} 5 \xrightarrow{\times 2} 10$ — **4 operaciones**.

BFS explora todos los estados por niveles y garantiza encontrar el mínimo.
El grafo es implícito: no se construye explícitamente, los vecinos de un
estado $x$ son $\{x+1, 2x\}$.

</details>

### Ejercicio 10 — Flood fill

Implementa flood fill (como la herramienta "balde de pintura") usando BFS.
Dada una cuadrícula de caracteres y una posición $(r, c)$, reemplaza todos
los caracteres conectados del mismo color.

<details><summary>¿Cuál es la complejidad?</summary>

$O(\text{rows} \times \text{cols})$ en el peor caso (si toda la cuadrícula
es del mismo color). Cada celda se visita a lo sumo una vez. Es equivalente
a BFS en un grafo de grid donde solo las celdas del color original son
vértices válidos.

Nota: flood fill también se puede implementar con DFS (recursivo o con
pila), pero BFS es preferible para cuadrículas grandes porque DFS recursivo
puede causar stack overflow.

</details>

---

## 11. Resumen

| Propiedad | BFS |
|-----------|-----|
| Estructura auxiliar | Cola FIFO (`VecDeque` / array circular) |
| Complejidad tiempo | $O(n + m)$ con lista de adyacencia |
| Complejidad espacio | $O(n)$ |
| Encuentra camino más corto | Sí, en grafos **no ponderados** |
| Árbol generado | Árbol BFS (por niveles, ancho) |
| Variantes | Multi-source, por niveles, 0-1, grid |
| No funciona para | Grafos con pesos arbitrarios (usar Dijkstra) |

BFS es el algoritmo fundamental de recorrido por amplitud. En T02 veremos
su contraparte — **DFS** (recorrido por profundidad) — que usa una pila en
lugar de una cola y produce un árbol con propiedades muy diferentes.
