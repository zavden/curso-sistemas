# T03 — Aplicaciones de DFS

## Objetivo

Dominar las tres aplicaciones fundamentales de DFS: **detección de ciclos**
(en grafos dirigidos y no dirigidos), **componentes conexos** (etiquetado,
conteo, y análisis), y **ordenamiento topológico** (DFS post-order inverso
vs Kahn). Cada aplicación se implementa completamente en C y Rust con manejo
de todos los casos borde.

---

## 1. Detección de ciclos

### 1.1 Grafos dirigidos: back edges

En T02 vimos que un grafo dirigido tiene ciclo $\iff$ DFS encuentra una
**back edge** (arista hacia un vértice GRAY). Ahora profundizamos en la
implementación y en **extraer el ciclo concreto**.

**¿Por qué funciona?** Si al explorar la arista $(u, v)$ encontramos que $v$
está GRAY, significa que $v$ es ancestro de $u$ en el árbol DFS. Luego existe
el camino $v \leadsto u$ (por el árbol) y la arista $u \to v$, formando un
ciclo.

#### Pseudocódigo para encontrar el ciclo

```
HAS-CYCLE-DIRECTED(G):
    color[] ← all WHITE
    parent[] ← all NIL

    for each u in V:
        if color[u] == WHITE:
            if DFS-CYCLE(G, u, color, parent):
                return true
    return false

DFS-CYCLE(G, u, color, parent):
    color[u] ← GRAY
    for each v in adj[u]:
        if color[v] == GRAY:
            // Back edge u→v found — extract cycle
            PRINT-CYCLE(u, v, parent)
            return true
        if color[v] == WHITE:
            parent[v] ← u
            if DFS-CYCLE(G, v, color, parent):
                return true
    color[u] ← BLACK
    return false

PRINT-CYCLE(u, v, parent):
    // Cycle: v → ... → u → v
    path ← [v]
    x ← u
    while x ≠ v:
        path.append(x)
        x ← parent[x]
    path.append(v)
    print path reversed
```

#### Traza paso a paso

```
Grafo dirigido:
  0 → 1 → 2 → 3
            ↑   ↓
            └── 4

Aristas: 0→1, 1→2, 2→3, 3→4, 4→2

DFS desde 0:
  Visit 0 (GRAY)
    Visit 1 (GRAY)
      Visit 2 (GRAY)
        Visit 3 (GRAY)
          Visit 4 (GRAY)
            Vecino 2 → ¡está GRAY! → Back edge 4→2
            Ciclo: 2 → 3 → 4 → 2
```

### 1.2 Grafos no dirigidos: arista a no-padre

En un grafo no dirigido, la representación lista ambos sentidos de cada
arista. Un vecino ya visitado que **no es el padre** indica un ciclo.

**Sutileza con multi-aristas**: si hay múltiples aristas entre $u$ y $v$,
el chequeo `v != parent[u]` no basta. Se debe rastrear la arista concreta
usada para llegar a $u$. Esto se resuelve pasando el **índice de la arista**
en lugar del vértice padre.

```
HAS-CYCLE-UNDIRECTED(G):
    visited[] ← all false
    parent[] ← all -1

    for each u in V:
        if not visited[u]:
            if DFS-CYCLE-UNDIR(G, u, visited, parent):
                return true
    return false

DFS-CYCLE-UNDIR(G, u, visited, parent):
    visited[u] ← true
    for each v in adj[u]:
        if not visited[v]:
            parent[v] ← u
            if DFS-CYCLE-UNDIR(G, v, visited, parent):
                return true
        else if v ≠ parent[u]:
            return true     // back edge → cycle
    return false
```

#### Reconstruir el ciclo en grafo no dirigido

```
Cuando se detecta v ≠ parent[u] y visited[v]:
  Ciclo: v ← ... ← u ← v (subir por parent[] desde u hasta v)

Ejemplo:
  0 — 1 — 2
  |       |
  3 ——————┘

  parent chain: 0←nil, 1←0, 2←1, 3←0
  Al visitar 3, vecino 2: visited[2]=true, 2 ≠ parent[3]=0
  Pero 2 no es ancestro directo. Recorrer: u=3, parent=0... no llega a 2.

  Corrección: esto funciona solo si v es ancestro de u.
  En DFS no dirigido, si visited[v] y v ≠ parent[u], entonces v
  necesariamente ES ancestro de u (está en la pila recursiva).
```

### 1.3 Complejidad de detección de ciclos

La detección de ciclos con DFS es $O(n + m)$: se visita cada vértice y
arista como máximo una vez. La extracción del ciclo concreto cuesta $O(n)$
adicional (recorrer el array `parent`).

**Comparación con otros métodos:**

| Método | Dirigido | No dirigido | Complejidad |
|--------|:--------:|:-----------:|:-----------:|
| DFS (back edge) | Sí | Sí | $O(n + m)$ |
| Kahn (in-degree) | Sí | No | $O(n + m)$ |
| Union-Find | No* | Sí | $O(m \cdot \alpha(n))$ |

*Union-Find no detecta ciclos en grafos dirigidos porque no captura la
dirección de las aristas.

---

## 2. Componentes conexos

### 2.1 Grafos no dirigidos

Un **componente conexo** es un subconjunto maximal de vértices tal que
existe camino entre cada par. Encontrar todos los componentes es trivial con
DFS: cada lanzamiento desde un vértice no visitado descubre exactamente un
componente.

```
CONNECTED-COMPONENTS(G):
    comp_id[] ← all -1
    num_components ← 0

    for each u in V:
        if comp_id[u] == -1:
            DFS-LABEL(G, u, comp_id, num_components)
            num_components ← num_components + 1

    return num_components, comp_id[]

DFS-LABEL(G, u, comp_id, label):
    comp_id[u] ← label
    for each v in adj[u]:
        if comp_id[v] == -1:
            DFS-LABEL(G, v, comp_id, label)
```

### 2.2 Propiedades de los componentes

Para un grafo con $n$ vértices, $m$ aristas y $k$ componentes:

- $n - m \leq k$ (igualdad cuando cada componente es un árbol, es decir, el
  grafo es un bosque).
- Si $k = 1$, el grafo es **conexo**.
- Si el grafo es conexo, $m \geq n - 1$ (mínimo: un árbol spanning).
- Cada componente con $n_i$ vértices y $m_i$ aristas cumple
  $m_i \geq n_i - 1$ si es conexo. Si $m_i = n_i - 1$, el componente es un
  **árbol**.

### 2.3 Análisis avanzado de componentes

Además de contar y etiquetar, frecuentemente necesitamos:

- **Tamaño de cada componente**: array `comp_size[c]`.
- **Componente más grande**: $\max_c(\text{comp\_size}[c])$.
- **¿Es el grafo un bosque?**: sí $\iff$ $m = n - k$.
- **Densidad por componente**: $2m_i / (n_i(n_i - 1))$.

### 2.4 Componentes con BFS vs DFS

Ambos algoritmos producen los mismos componentes (difieren solo en el orden
de visita dentro de cada componente). La elección es de preferencia:

| Criterio | DFS | BFS |
|----------|:---:|:---:|
| Implementación | Recursión (más simple) | Cola explícita |
| Stack overflow | Riesgo en grafos profundos | No |
| Orden de visita | Profundidad primero | Niveles |
| Distancias | No las calcula | Sí |

Para solo contar/etiquetar componentes, DFS recursivo es la opción más
concisa.

---

## 3. Ordenamiento topológico

### 3.1 Definición

Un **ordenamiento topológico** de un DAG $G = (V, E)$ es una permutación
$v_1, v_2, \ldots, v_n$ de los vértices tal que para cada arista
$(v_i, v_j) \in E$, se cumple $i < j$. Es decir, cada vértice aparece antes
que todos sus sucesores.

Un ordenamiento topológico existe $\iff$ el grafo es un **DAG** (directed
acyclic graph). Si hay ciclo, no es posible: algún vértice debería aparecer
antes de sí mismo.

### 3.2 Método DFS (post-order inverso)

**Propiedad clave**: si $(u, v) \in E$ y el grafo es un DAG, entonces
$\text{fin}[u] > \text{fin}[v]$ tras DFS. Esto se demuestra por casos:

- Si $v$ es WHITE cuando se explora $(u, v)$: $v$ se convierte en
  descendiente de $u$, así que $v$ finaliza antes que $u$.
- Si $v$ es BLACK: $v$ ya finalizó, así que $\text{fin}[v] < \text{fin}[u]$.
- $v$ no puede ser GRAY (eso sería una back edge, y no hay ciclos en un
  DAG).

Por tanto, ordenar los vértices por $\text{fin}[]$ decreciente produce un
ordenamiento topológico válido. Esto equivale a **invertir el post-order**.

```
TOPOLOGICAL-SORT-DFS(G):
    // First check: is it a DAG?
    color[] ← all WHITE
    stack ← empty

    for each u in V:
        if color[u] == WHITE:
            if not DFS-TOPO(G, u, color, stack):
                return "ERROR: cycle detected"

    return stack (reversed)

DFS-TOPO(G, u, color, stack):
    color[u] ← GRAY
    for each v in adj[u]:
        if color[v] == GRAY:
            return false        // cycle!
        if color[v] == WHITE:
            if not DFS-TOPO(G, v, color, stack):
                return false
    color[u] ← BLACK
    stack.push(u)               // post-order
    return true
```

### 3.3 Método Kahn (BFS con in-degree)

Alternativa basada en BFS que remueve repetidamente vértices sin predecesores:

```
KAHN(G):
    in_deg[] ← compute in-degrees
    queue ← all u where in_deg[u] == 0
    order ← empty

    while queue not empty:
        u ← dequeue
        order.append(u)
        for each v in adj[u]:
            in_deg[v] ← in_deg[v] - 1
            if in_deg[v] == 0:
                queue.enqueue(v)

    if |order| < |V|:
        return "ERROR: cycle detected"
    return order
```

### 3.4 Comparación DFS vs Kahn

| Aspecto | DFS (post-order) | Kahn (BFS in-degree) |
|---------|:-----------------:|:--------------------:|
| Complejidad | $O(n + m)$ | $O(n + m)$ |
| Detecta ciclos | Sí (back edge) | Sí ($\lvert\text{order}\rvert < n$) |
| Resultado | Reverse post-order | Orden directo |
| Implementación | Recursión natural | Cola + in-degree |
| Todos los topo-orders | DFS + backtracking | Más difícil |
| Paralelizable | No naturalmente | Sí (niveles independientes) |
| Lexicográficamente menor | Difícil | Sí (con min-heap) |

**Kahn para el menor lexicográfico**: reemplazar la cola FIFO con una
**min-priority queue** (min-heap). En cada paso, entre los vértices con
in-degree 0 se elige el de menor índice/nombre.

### 3.5 Múltiples ordenamientos válidos

Un DAG puede tener muchos ordenamientos topológicos. El número depende de la
estructura del grafo:

- **Cadena** $0 \to 1 \to \cdots \to n-1$: exactamente **1** orden.
- **Sin aristas** ($n$ vértices aislados): $n!$ órdenes.
- **Diamante** ($A \to B, A \to C, B \to D, C \to D$): **2** órdenes
  ($A,B,C,D$ y $A,C,B,D$).

Contar el número exacto de ordenamientos topológicos es un problema
$\#\text{P}$-completo (más difícil que NP).

### 3.6 Aplicaciones del ordenamiento topológico

| Aplicación | Descripción |
|-----------|-------------|
| Compilación | Ordenar dependencias entre módulos |
| Planificación | Tareas con precedencias (PERT/CPM) |
| Resolución de dependencias | Paquetes (apt, cargo, npm) |
| Evaluación de fórmulas | Spreadsheet: evaluar celdas en orden |
| Serialización | Orden de inicialización de servicios |
| Camino crítico | Ruta más larga en DAG ponderado |

---

## 4. Puentes y puntos de articulación

Aunque no están en la descripción del tópico, son aplicaciones clásicas de
DFS que completan el panorama.

### 4.1 Definiciones

- **Puente** (bridge): arista cuya eliminación desconecta el grafo (aumenta
  el número de componentes).
- **Punto de articulación** (cut vertex): vértice cuya eliminación (junto con
  sus aristas) desconecta el grafo.

### 4.2 Algoritmo de Tarjan para puentes

Se define `low[u]` como el menor `disc[]` alcanzable desde el subárbol de
$u$ usando aristas del árbol DFS y **a lo sumo una back edge**:

$$\text{low}[u] = \min\bigl(\text{disc}[u],\ \min_{(u,v) \text{ back}} \text{disc}[v],\ \min_{(u,v) \text{ tree}} \text{low}[v]\bigr)$$

**Puente**: la arista tree $(u, v)$ (donde $u$ es padre de $v$) es un puente
$\iff$ $\text{low}[v] > \text{disc}[u]$. Esto significa que ningún vértice
del subárbol de $v$ puede "escapar" hacia arriba sin pasar por $(u, v)$.

**Punto de articulación**: el vértice $u$ es articulación si:
- $u$ es raíz del árbol DFS **y** tiene $\geq 2$ hijos, o
- $u$ no es raíz **y** tiene algún hijo $v$ con $\text{low}[v] \geq \text{disc}[u]$.

### 4.3 Traza

```
Grafo:  0 — 1 — 2 — 3
             |   |
             4   5 — 6

Aristas: {0-1, 1-2, 1-4, 2-3, 2-5, 5-6}

DFS desde 0:
  0(d=1) → 1(d=2) → 2(d=3) → 3(d=4) fin 3(low=4)
                              → 5(d=5) → 6(d=6) fin 6(low=6)
                              fin 5(low=5)
                     fin 2(low=3)
           → 4(d=7) fin 4(low=7)
  fin 1(low=2)
  fin 0(low=1)

disc = [1, 2, 3, 4, 5, 6, 7]
low  = [1, 2, 3, 4, 5, 6, 7]

Puentes (low[v] > disc[u] para tree edge u→v):
  0-1: low[1]=2 > disc[0]=1 → SÍ, puente
  1-2: low[2]=3 > disc[1]=2 → SÍ, puente
  1-4: low[4]=7 > disc[1]=2 → SÍ, puente
  2-3: low[3]=4 > disc[2]=3 → SÍ, puente
  2-5: low[5]=5 > disc[2]=3 → SÍ, puente
  5-6: low[6]=6 > disc[5]=5 → SÍ, puente

(Todas son puentes porque no hay back edges — es un árbol.)
```

```
Grafo con ciclo:
  0 — 1 — 2
  |       |
  └———————┘   3 — 4

DFS desde 0:
  0(d=1) → 1(d=2) → 2(d=3) → back to 0 (disc=1)
  fin 2(low=1), fin 1(low=1), fin 0(low=1)
  3(d=4) → 4(d=5), fin 4(low=5), fin 3(low=4)

Puentes: 3-4 (low[4]=5 > disc[3]=4)
No puentes: 0-1, 1-2 (ciclo protege las aristas)
```

---

## 5. Resumen de aplicaciones

| Aplicación | Tipo de grafo | Clave de DFS usada | Complejidad |
|-----------|:-------------|:-------------------:|:-----------:|
| Detectar ciclo | Dirigido | Back edge (GRAY) | $O(n + m)$ |
| Detectar ciclo | No dirigido | Vecino visitado $\neq$ padre | $O(n + m)$ |
| Componentes conexos | No dirigido | Lanzar DFS por componente | $O(n + m)$ |
| Topological sort | DAG | Reverse post-order | $O(n + m)$ |
| Puentes | No dirigido | `low[v] > disc[u]` | $O(n + m)$ |
| Puntos de articulación | No dirigido | `low[v] >= disc[u]` | $O(n + m)$ |
| SCC (Tarjan) | Dirigido | `low[]` + stack | $O(n + m)$ |
| Biconexo | No dirigido | Sin art. points | $O(n + m)$ |

Todas las aplicaciones son $O(n + m)$ — DFS solo visita cada vértice y arista
una vez, y el procesamiento adicional es $O(1)$ por vértice/arista.

---

## 6. Programa completo en C

```c
/*
 * dfs_apps.c
 *
 * DFS Applications: cycle detection (directed & undirected),
 * connected components, topological sort (DFS & Kahn),
 * bridges, and articulation points.
 *
 * Compile: gcc -O2 -o dfs_apps dfs_apps.c
 * Run:     ./dfs_apps
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_V 50
#define MAX_EDGES 200

/* =================== ADJACENCY LIST =============================== */

typedef struct {
    int *dests;
    int size, cap;
} NbrList;

typedef struct {
    NbrList adj[MAX_V];
    int n;
    bool directed;
} Graph;

void graph_init(Graph *g, int n, bool directed) {
    g->n = n;
    g->directed = directed;
    for (int i = 0; i < n; i++) {
        g->adj[i].size = 0;
        g->adj[i].cap = 4;
        g->adj[i].dests = malloc(4 * sizeof(int));
    }
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++)
        free(g->adj[i].dests);
}

void add_edge(Graph *g, int u, int v) {
    NbrList *list = &g->adj[u];
    if (list->size == list->cap) {
        list->cap *= 2;
        list->dests = realloc(list->dests, list->cap * sizeof(int));
    }
    list->dests[list->size++] = v;

    if (!g->directed) {
        list = &g->adj[v];
        if (list->size == list->cap) {
            list->cap *= 2;
            list->dests = realloc(list->dests, list->cap * sizeof(int));
        }
        list->dests[list->size++] = u;
    }
}

/* ============ DEMO 1: Cycle detection — directed graph ============ */

static int color_d[MAX_V];
static int parent_d[MAX_V];
static int cycle_start, cycle_end;

bool dfs_cycle_directed(Graph *g, int u) {
    color_d[u] = 1; /* GRAY */
    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];
        if (color_d[v] == 1) {
            /* Back edge: u → v (v is GRAY ancestor) */
            cycle_end = u;
            cycle_start = v;
            return true;
        }
        if (color_d[v] == 0) {
            parent_d[v] = u;
            if (dfs_cycle_directed(g, v))
                return true;
        }
    }
    color_d[u] = 2; /* BLACK */
    return false;
}

void demo_cycle_directed(void) {
    printf("=== Demo 1: Cycle detection (directed) ===\n");

    /* Graph: 0→1→2→3→4→2 (cycle: 2→3→4→2) */
    Graph g;
    graph_init(&g, 5, true);
    add_edge(&g, 0, 1);
    add_edge(&g, 1, 2);
    add_edge(&g, 2, 3);
    add_edge(&g, 3, 4);
    add_edge(&g, 4, 2); /* creates cycle */

    printf("Directed graph edges: 0->1, 1->2, 2->3, 3->4, 4->2\n");

    memset(color_d, 0, sizeof(color_d));
    memset(parent_d, -1, sizeof(parent_d));
    cycle_start = cycle_end = -1;

    bool has_cycle = false;
    for (int u = 0; u < g.n && !has_cycle; u++) {
        if (color_d[u] == 0)
            has_cycle = dfs_cycle_directed(&g, u);
    }

    if (has_cycle) {
        printf("Cycle found! Extracting...\n  ");
        int path[MAX_V], len = 0;
        path[len++] = cycle_start;
        for (int x = cycle_end; x != cycle_start; x = parent_d[x])
            path[len++] = x;
        path[len++] = cycle_start;

        /* Print reversed */
        for (int i = len - 1; i >= 0; i--) {
            printf("%d", path[i]);
            if (i > 0) printf(" -> ");
        }
        printf("\n");
    } else {
        printf("No cycle.\n");
    }

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 2: Cycle detection — undirected graph ========== */

static bool visited_u[MAX_V];
static int parent_u[MAX_V];
static bool found_cycle_u;
static int ucycle_u, ucycle_v;

void dfs_cycle_undirected(Graph *g, int u) {
    visited_u[u] = true;
    for (int i = 0; i < g->adj[u].size && !found_cycle_u; i++) {
        int v = g->adj[u].dests[i];
        if (!visited_u[v]) {
            parent_u[v] = u;
            dfs_cycle_undirected(g, v);
        } else if (v != parent_u[u]) {
            found_cycle_u = true;
            ucycle_u = u;
            ucycle_v = v;
        }
    }
}

void demo_cycle_undirected(void) {
    printf("=== Demo 2: Cycle detection (undirected) ===\n");

    /* Graph:  0-1-2-3-0 (square cycle), 4-5 (separate edge) */
    Graph g;
    graph_init(&g, 6, false);
    add_edge(&g, 0, 1);
    add_edge(&g, 1, 2);
    add_edge(&g, 2, 3);
    add_edge(&g, 3, 0); /* creates cycle */
    add_edge(&g, 4, 5); /* no cycle here */

    printf("Undirected graph: 0-1-2-3-0 (square), 4-5\n");

    memset(visited_u, false, sizeof(visited_u));
    memset(parent_u, -1, sizeof(parent_u));
    found_cycle_u = false;

    for (int u = 0; u < g.n && !found_cycle_u; u++) {
        if (!visited_u[u])
            dfs_cycle_undirected(&g, u);
    }

    if (found_cycle_u) {
        printf("Cycle found! Back edge: %d — %d\n", ucycle_u, ucycle_v);
        printf("  Cycle: ");
        int path[MAX_V], len = 0;
        for (int x = ucycle_u; x != ucycle_v; x = parent_u[x])
            path[len++] = x;
        path[len++] = ucycle_v;
        for (int i = len - 1; i >= 0; i--) {
            printf("%d", path[i]);
            if (i > 0) printf(" - ");
        }
        printf(" - %d\n", ucycle_v);
    } else {
        printf("No cycle.\n");
    }

    /* Test acyclic */
    Graph tree;
    graph_init(&tree, 4, false);
    add_edge(&tree, 0, 1);
    add_edge(&tree, 1, 2);
    add_edge(&tree, 2, 3);

    memset(visited_u, false, sizeof(visited_u));
    memset(parent_u, -1, sizeof(parent_u));
    found_cycle_u = false;
    for (int u = 0; u < tree.n && !found_cycle_u; u++) {
        if (!visited_u[u])
            dfs_cycle_undirected(&tree, u);
    }
    printf("Tree 0-1-2-3: %s\n", found_cycle_u ? "has cycle" : "no cycle");

    graph_free(&g);
    graph_free(&tree);
    printf("\n");
}

/* ============ DEMO 3: Connected components ======================== */

static int comp_id[MAX_V];

void dfs_label(Graph *g, int u, int label) {
    comp_id[u] = label;
    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];
        if (comp_id[v] == -1)
            dfs_label(g, v, label);
    }
}

void demo_components(void) {
    printf("=== Demo 3: Connected components ===\n");

    /* 3 components: {0,1,2}, {3,4}, {5} */
    Graph g;
    graph_init(&g, 6, false);
    add_edge(&g, 0, 1);
    add_edge(&g, 1, 2);
    add_edge(&g, 0, 2);
    add_edge(&g, 3, 4);
    /* vertex 5 is isolated */

    printf("Undirected graph: {0-1, 1-2, 0-2}, {3-4}, {5 isolated}\n");

    memset(comp_id, -1, sizeof(comp_id));
    int num_comp = 0;

    for (int u = 0; u < g.n; u++) {
        if (comp_id[u] == -1) {
            dfs_label(&g, u, num_comp);
            num_comp++;
        }
    }

    printf("Number of components: %d\n", num_comp);
    for (int c = 0; c < num_comp; c++) {
        printf("  Component %d: {", c);
        bool first = true;
        for (int u = 0; u < g.n; u++) {
            if (comp_id[u] == c) {
                if (!first) printf(", ");
                printf("%d", u);
                first = false;
            }
        }
        printf("}\n");
    }

    /* Analysis */
    int comp_size[MAX_V] = {0};
    int comp_edges[MAX_V] = {0};
    for (int u = 0; u < g.n; u++) {
        comp_size[comp_id[u]]++;
        comp_edges[comp_id[u]] += g.adj[u].size;
    }

    printf("\n  Analysis:\n");
    for (int c = 0; c < num_comp; c++) {
        int edges = comp_edges[c] / 2; /* undirected: each edge counted twice */
        bool is_tree = (edges == comp_size[c] - 1);
        printf("  Comp %d: %d vertices, %d edges%s\n",
               c, comp_size[c], edges,
               is_tree ? " (tree)" : "");
    }

    int total_edges = 0;
    for (int u = 0; u < g.n; u++)
        total_edges += g.adj[u].size;
    total_edges /= 2;
    bool is_forest = (total_edges == g.n - num_comp);
    printf("  Is forest (m == n - k)? %d == %d - %d? %s\n",
           total_edges, g.n, num_comp,
           is_forest ? "YES" : "NO");

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 4: Topological sort (DFS) ====================== */

static int topo_color[MAX_V];
static int topo_order[MAX_V];
static int topo_idx;
static bool topo_has_cycle;

void dfs_topo(Graph *g, int u) {
    topo_color[u] = 1;
    for (int i = 0; i < g->adj[u].size && !topo_has_cycle; i++) {
        int v = g->adj[u].dests[i];
        if (topo_color[v] == 1) {
            topo_has_cycle = true;
            return;
        }
        if (topo_color[v] == 0)
            dfs_topo(g, v);
    }
    topo_color[u] = 2;
    topo_order[topo_idx++] = u; /* post-order */
}

void demo_topo_dfs(void) {
    printf("=== Demo 4: Topological sort (DFS) ===\n");

    /* DAG: course prerequisites
     * 0(Math) → 1(Physics) → 3(Quantum)
     * 0(Math) → 2(CS)      → 3(Quantum)
     * 2(CS)   → 4(AI)
     * 5(English) — no dependencies
     */
    Graph g;
    graph_init(&g, 6, true);
    add_edge(&g, 0, 1);
    add_edge(&g, 0, 2);
    add_edge(&g, 1, 3);
    add_edge(&g, 2, 3);
    add_edge(&g, 2, 4);

    const char *names[] = {"Math", "Physics", "CS", "Quantum", "AI", "English"};
    printf("DAG (course prereqs):\n");
    printf("  Math→Physics, Math→CS, Physics→Quantum, CS→Quantum, CS→AI\n");
    printf("  English (independent)\n\n");

    memset(topo_color, 0, sizeof(topo_color));
    topo_idx = 0;
    topo_has_cycle = false;

    for (int u = 0; u < g.n && !topo_has_cycle; u++) {
        if (topo_color[u] == 0)
            dfs_topo(&g, u);
    }

    if (topo_has_cycle) {
        printf("ERROR: cycle detected!\n");
    } else {
        printf("Topological order (DFS reverse post-order):\n  ");
        for (int i = topo_idx - 1; i >= 0; i--) {
            printf("%s(%d)", names[topo_order[i]], topo_order[i]);
            if (i > 0) printf(" → ");
        }
        printf("\n");
    }

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 5: Topological sort (Kahn / BFS) =============== */

void demo_topo_kahn(void) {
    printf("=== Demo 5: Topological sort (Kahn / BFS) ===\n");

    /* Same DAG as Demo 4 */
    Graph g;
    graph_init(&g, 6, true);
    add_edge(&g, 0, 1);
    add_edge(&g, 0, 2);
    add_edge(&g, 1, 3);
    add_edge(&g, 2, 3);
    add_edge(&g, 2, 4);

    const char *names[] = {"Math", "Physics", "CS", "Quantum", "AI", "English"};

    /* Compute in-degrees */
    int in_deg[MAX_V] = {0};
    for (int u = 0; u < g.n; u++)
        for (int i = 0; i < g.adj[u].size; i++)
            in_deg[g.adj[u].dests[i]]++;

    printf("In-degrees: ");
    for (int u = 0; u < g.n; u++)
        printf("%s=%d ", names[u], in_deg[u]);
    printf("\n");

    /* Kahn's algorithm with simple queue */
    int queue[MAX_V], front = 0, back = 0;
    for (int u = 0; u < g.n; u++) {
        if (in_deg[u] == 0)
            queue[back++] = u;
    }

    int order[MAX_V], order_len = 0;
    printf("\nKahn's algorithm trace:\n");

    while (front < back) {
        int u = queue[front++];
        order[order_len++] = u;
        printf("  Dequeue %s(%d), remove its edges:\n", names[u], u);

        for (int i = 0; i < g.adj[u].size; i++) {
            int v = g.adj[u].dests[i];
            in_deg[v]--;
            printf("    in_deg[%s] = %d", names[v], in_deg[v]);
            if (in_deg[v] == 0) {
                queue[back++] = v;
                printf(" → enqueue");
            }
            printf("\n");
        }
    }

    if (order_len < g.n) {
        printf("ERROR: cycle detected (%d processed < %d vertices)\n",
               order_len, g.n);
    } else {
        printf("\nKahn order: ");
        for (int i = 0; i < order_len; i++) {
            printf("%s(%d)", names[order[i]], order[i]);
            if (i < order_len - 1) printf(" → ");
        }
        printf("\n");
    }

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 6: Bridges and articulation points ============= */

static int disc_b[MAX_V], low_b[MAX_V], parent_b[MAX_V];
static int timer_b;
static bool is_artic[MAX_V];

typedef struct { int u, v; } Bridge;
static Bridge bridges[MAX_EDGES];
static int num_bridges;

void dfs_bridges(Graph *g, int u) {
    disc_b[u] = low_b[u] = ++timer_b;
    int children = 0;

    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];

        if (disc_b[v] == 0) {
            children++;
            parent_b[v] = u;
            dfs_bridges(g, v);

            if (low_b[v] < low_b[u])
                low_b[u] = low_b[v];

            /* Bridge: no back edge from subtree of v reaches above u */
            if (low_b[v] > disc_b[u]) {
                bridges[num_bridges].u = u;
                bridges[num_bridges].v = v;
                num_bridges++;
            }

            /* Articulation point */
            if (parent_b[u] == -1 && children > 1)
                is_artic[u] = true;
            if (parent_b[u] != -1 && low_b[v] >= disc_b[u])
                is_artic[u] = true;

        } else if (v != parent_b[u]) {
            if (disc_b[v] < low_b[u])
                low_b[u] = disc_b[v];
        }
    }
}

void demo_bridges(void) {
    printf("=== Demo 6: Bridges and articulation points ===\n");

    /*
     *  0 — 1 — 2
     *  |   |
     *  3 — 4   5 — 6
     *          |   |
     *          7 ——┘
     */
    Graph g;
    graph_init(&g, 8, false);
    add_edge(&g, 0, 1);
    add_edge(&g, 0, 3);
    add_edge(&g, 1, 4);
    add_edge(&g, 3, 4); /* cycle: 0-1-4-3-0 */
    add_edge(&g, 1, 2); /* bridge */
    add_edge(&g, 2, 5); /* bridge */
    add_edge(&g, 5, 6);
    add_edge(&g, 5, 7);
    add_edge(&g, 6, 7); /* cycle: 5-6-7-5 */

    printf("Graph:\n");
    printf("  0-1, 0-3, 1-4, 3-4 (cycle)\n");
    printf("  1-2 (bridge?), 2-5 (bridge?)\n");
    printf("  5-6, 5-7, 6-7 (cycle)\n\n");

    memset(disc_b, 0, sizeof(disc_b));
    memset(low_b, 0, sizeof(low_b));
    memset(parent_b, -1, sizeof(parent_b));
    memset(is_artic, false, sizeof(is_artic));
    timer_b = 0;
    num_bridges = 0;

    for (int u = 0; u < g.n; u++) {
        if (disc_b[u] == 0)
            dfs_bridges(&g, u);
    }

    printf("disc[]: ");
    for (int i = 0; i < g.n; i++) printf("%d=%d ", i, disc_b[i]);
    printf("\nlow[]:  ");
    for (int i = 0; i < g.n; i++) printf("%d=%d ", i, low_b[i]);
    printf("\n\n");

    printf("Bridges (%d):\n", num_bridges);
    for (int i = 0; i < num_bridges; i++)
        printf("  %d — %d\n", bridges[i].u, bridges[i].v);

    printf("\nArticulation points: ");
    bool any = false;
    for (int i = 0; i < g.n; i++) {
        if (is_artic[i]) {
            if (any) printf(", ");
            printf("%d", i);
            any = true;
        }
    }
    printf("%s\n", any ? "" : "(none)");

    graph_free(&g);
    printf("\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_cycle_directed();
    demo_cycle_undirected();
    demo_components();
    demo_topo_dfs();
    demo_topo_kahn();
    demo_bridges();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: Cycle detection (directed) ===
Directed graph edges: 0->1, 1->2, 2->3, 3->4, 4->2
Cycle found! Extracting...
  2 -> 3 -> 4 -> 2

=== Demo 2: Cycle detection (undirected) ===
Undirected graph: 0-1-2-3-0 (square), 4-5
Cycle found! Back edge: 3 — 0
  Cycle: 0 - 1 - 2 - 3 - 0
Tree 0-1-2-3: no cycle

=== Demo 3: Connected components ===
Undirected graph: {0-1, 1-2, 0-2}, {3-4}, {5 isolated}
Number of components: 3
  Component 0: {0, 1, 2}
  Component 1: {3, 4}
  Component 2: {5}

  Analysis:
  Comp 0: 3 vertices, 3 edges
  Comp 1: 2 vertices, 1 edges (tree)
  Comp 2: 1 vertices, 0 edges (tree)
  Is forest (m == n - k)? 4 == 6 - 3? NO

=== Demo 4: Topological sort (DFS) ===
DAG (course prereqs):
  Math→Physics, Math→CS, Physics→Quantum, CS→Quantum, CS→AI
  English (independent)

Topological order (DFS reverse post-order):
  English(5) → Math(0) → CS(2) → AI(4) → Physics(1) → Quantum(3)

=== Demo 5: Topological sort (Kahn / BFS) ===
In-degrees: Math=0 Physics=1 CS=1 Quantum=2 AI=1 English=0

Kahn's algorithm trace:
  Dequeue Math(0), remove its edges:
    in_deg[Physics] = 0 → enqueue
    in_deg[CS] = 0 → enqueue
  Dequeue English(5), remove its edges:
  Dequeue Physics(1), remove its edges:
    in_deg[Quantum] = 1
  Dequeue CS(2), remove its edges:
    in_deg[Quantum] = 0 → enqueue
    in_deg[AI] = 0 → enqueue
  Dequeue Quantum(3), remove its edges:
  Dequeue AI(4), remove its edges:

Kahn order: Math(0) → English(5) → Physics(1) → CS(2) → Quantum(3) → AI(4)

=== Demo 6: Bridges and articulation points ===
Graph:
  0-1, 0-3, 1-4, 3-4 (cycle)
  1-2 (bridge?), 2-5 (bridge?)
  5-6, 5-7, 6-7 (cycle)

disc[]: 0=1 1=2 2=5 3=3 4=4 5=6 6=7 7=8
low[]:  0=1 1=1 2=2 3=1 4=1 5=6 6=6 7=6

Bridges (2):
  1 — 2
  2 — 5

Articulation points: 1, 2
```

---

## 7. Programa completo en Rust

```rust
/*
 * dfs_apps.rs
 *
 * DFS Applications: cycle detection, connected components,
 * topological sort (DFS & Kahn), bridges and articulation points.
 *
 * Compile: rustc -O -o dfs_apps dfs_apps.rs
 * Run:     ./dfs_apps
 */

use std::collections::VecDeque;

/* =================== GRAPH STRUCTURE ============================== */

struct Graph {
    adj: Vec<Vec<usize>>,
    directed: bool,
}

impl Graph {
    fn new(n: usize, directed: bool) -> Self {
        Self {
            adj: vec![vec![]; n],
            directed,
        }
    }

    fn add_edge(&mut self, u: usize, v: usize) {
        self.adj[u].push(v);
        if !self.directed {
            self.adj[v].push(u);
        }
    }

    fn n(&self) -> usize {
        self.adj.len()
    }
}

/* ======== Demo 1: Cycle detection — directed graph ================ */

fn find_cycle_directed(g: &Graph) -> Option<Vec<usize>> {
    let n = g.n();
    let mut color = vec![0u8; n]; // 0=white, 1=gray, 2=black
    let mut parent = vec![usize::MAX; n];
    let mut cycle_start = usize::MAX;
    let mut cycle_end = usize::MAX;

    fn dfs(
        u: usize, g: &Graph, color: &mut [u8], parent: &mut [usize],
        cs: &mut usize, ce: &mut usize,
    ) -> bool {
        color[u] = 1;
        for &v in &g.adj[u] {
            if color[v] == 1 {
                *ce = u;
                *cs = v;
                return true;
            }
            if color[v] == 0 {
                parent[v] = u;
                if dfs(v, g, color, parent, cs, ce) {
                    return true;
                }
            }
        }
        color[u] = 2;
        false
    }

    for u in 0..n {
        if color[u] == 0 {
            if dfs(u, g, &mut color, &mut parent,
                   &mut cycle_start, &mut cycle_end) {
                // Extract cycle: cycle_start → ... → cycle_end → cycle_start
                let mut path = vec![cycle_start];
                let mut x = cycle_end;
                while x != cycle_start {
                    path.push(x);
                    x = parent[x];
                }
                path.push(cycle_start);
                path.reverse();
                return Some(path);
            }
        }
    }
    None
}

fn demo_cycle_directed() {
    println!("=== Demo 1: Cycle detection (directed) ===");

    let mut g = Graph::new(5, true);
    // 0→1→2→3→4→2 (cycle: 2→3→4→2)
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    g.add_edge(3, 4);
    g.add_edge(4, 2);

    println!("Edges: 0→1, 1→2, 2→3, 3→4, 4→2");

    match find_cycle_directed(&g) {
        Some(cycle) => {
            let parts: Vec<String> = cycle.iter().map(|v| v.to_string()).collect();
            println!("Cycle: {}", parts.join(" → "));
        }
        None => println!("No cycle."),
    }

    // Test DAG (no cycle)
    let mut dag = Graph::new(4, true);
    dag.add_edge(0, 1);
    dag.add_edge(0, 2);
    dag.add_edge(1, 3);
    dag.add_edge(2, 3);
    println!("DAG 0→1,2; 1,2→3: {}",
             if find_cycle_directed(&dag).is_some() { "has cycle" }
             else { "no cycle" });
    println!();
}

/* ======== Demo 2: Cycle detection — undirected graph ============== */

fn find_cycle_undirected(g: &Graph) -> Option<Vec<usize>> {
    let n = g.n();
    let mut visited = vec![false; n];
    let mut parent = vec![usize::MAX; n];
    let mut result: Option<Vec<usize>> = None;

    fn dfs(
        u: usize, g: &Graph, visited: &mut [bool], parent: &mut [usize],
        result: &mut Option<Vec<usize>>,
    ) {
        visited[u] = true;
        for &v in &g.adj[u] {
            if result.is_some() { return; }
            if !visited[v] {
                parent[v] = u;
                dfs(v, g, visited, parent, result);
            } else if v != parent[u] {
                // Back edge u—v, extract cycle
                let mut path = vec![v];
                let mut x = u;
                while x != v {
                    path.push(x);
                    x = parent[x];
                }
                path.push(v);
                *result = Some(path);
            }
        }
    }

    for u in 0..n {
        if !visited[u] {
            dfs(u, g, &mut visited, &mut parent, &mut result);
            if result.is_some() { break; }
        }
    }
    result
}

fn demo_cycle_undirected() {
    println!("=== Demo 2: Cycle detection (undirected) ===");

    let mut g = Graph::new(5, false);
    // Triangle: 0-1-2-0, plus 3-4
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 0);
    g.add_edge(3, 4);

    println!("Graph: 0-1-2-0 (triangle), 3-4");

    match find_cycle_undirected(&g) {
        Some(cycle) => {
            let parts: Vec<String> = cycle.iter().map(|v| v.to_string()).collect();
            println!("Cycle: {}", parts.join(" — "));
        }
        None => println!("No cycle."),
    }

    // Tree (no cycle)
    let mut tree = Graph::new(4, false);
    tree.add_edge(0, 1);
    tree.add_edge(1, 2);
    tree.add_edge(1, 3);
    println!("Tree 0-1-2, 1-3: {}",
             if find_cycle_undirected(&tree).is_some() { "has cycle" }
             else { "no cycle" });
    println!();
}

/* ======== Demo 3: Connected components ============================ */

fn connected_components(g: &Graph) -> (usize, Vec<usize>) {
    let n = g.n();
    let mut comp = vec![usize::MAX; n];
    let mut num = 0;

    fn dfs(u: usize, g: &Graph, comp: &mut [usize], label: usize) {
        comp[u] = label;
        for &v in &g.adj[u] {
            if comp[v] == usize::MAX {
                dfs(v, g, comp, label);
            }
        }
    }

    for u in 0..n {
        if comp[u] == usize::MAX {
            dfs(u, g, &mut comp, num);
            num += 1;
        }
    }
    (num, comp)
}

fn demo_components() {
    println!("=== Demo 3: Connected components ===");

    // 4 components: {0,1,2,3}, {4,5}, {6}, {7,8,9}
    let mut g = Graph::new(10, false);
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    g.add_edge(0, 3);
    g.add_edge(4, 5);
    g.add_edge(7, 8);
    g.add_edge(8, 9);

    let (num, comp) = connected_components(&g);
    println!("{} components:", num);

    for c in 0..num {
        let members: Vec<usize> = (0..g.n()).filter(|&u| comp[u] == c).collect();
        let edges: usize = members.iter()
            .map(|&u| g.adj[u].iter().filter(|&&v| comp[v] == c).count())
            .sum::<usize>() / 2;
        let is_tree = edges == members.len() - 1;

        let names: Vec<String> = members.iter().map(|v| v.to_string()).collect();
        println!("  Component {}: {{{}}} — {} vertices, {} edges{}",
                 c, names.join(", "), members.len(), edges,
                 if is_tree { " (tree)" } else { "" });
    }

    // Forest check
    let total_edges: usize = g.adj.iter().map(|a| a.len()).sum::<usize>() / 2;
    let is_forest = total_edges == g.n() - num;
    println!("Is forest? m={} == n-k={}? {}",
             total_edges, g.n() - num,
             if is_forest { "YES" } else { "NO" });
    println!();
}

/* ======== Demo 4: Topological sort (DFS) ========================== */

fn topo_sort_dfs(g: &Graph) -> Option<Vec<usize>> {
    let n = g.n();
    let mut color = vec![0u8; n];
    let mut order = Vec::with_capacity(n);
    let mut has_cycle = false;

    fn dfs(
        u: usize, g: &Graph, color: &mut [u8],
        order: &mut Vec<usize>, has_cycle: &mut bool,
    ) {
        color[u] = 1;
        for &v in &g.adj[u] {
            if *has_cycle { return; }
            match color[v] {
                0 => dfs(v, g, color, order, has_cycle),
                1 => *has_cycle = true,
                _ => {}
            }
        }
        color[u] = 2;
        order.push(u);
    }

    for u in 0..n {
        if color[u] == 0 {
            dfs(u, g, &mut color, &mut order, &mut has_cycle);
        }
    }

    if has_cycle {
        None
    } else {
        order.reverse();
        Some(order)
    }
}

fn demo_topo_dfs() {
    println!("=== Demo 4: Topological sort (DFS) ===");

    let names = ["Math", "Physics", "CS", "Quantum", "AI", "English"];
    let mut g = Graph::new(6, true);
    g.add_edge(0, 1); // Math → Physics
    g.add_edge(0, 2); // Math → CS
    g.add_edge(1, 3); // Physics → Quantum
    g.add_edge(2, 3); // CS → Quantum
    g.add_edge(2, 4); // CS → AI

    println!("DAG: Math→Physics, Math→CS, Physics→Quantum, CS→Quantum, CS→AI");
    println!("     English (independent)\n");

    match topo_sort_dfs(&g) {
        Some(order) => {
            let parts: Vec<String> = order.iter()
                .map(|&v| format!("{}({})", names[v], v))
                .collect();
            println!("DFS topo order: {}", parts.join(" → "));
        }
        None => println!("ERROR: cycle detected!"),
    }

    // Test with cycle
    let mut cyclic = Graph::new(3, true);
    cyclic.add_edge(0, 1);
    cyclic.add_edge(1, 2);
    cyclic.add_edge(2, 0);
    println!("Cyclic 0→1→2→0: {}",
             if topo_sort_dfs(&cyclic).is_some() { "sorted" }
             else { "cycle detected" });
    println!();
}

/* ======== Demo 5: Topological sort (Kahn / BFS) ================== */

fn topo_sort_kahn(g: &Graph) -> Option<Vec<usize>> {
    let n = g.n();
    let mut in_deg = vec![0usize; n];

    for u in 0..n {
        for &v in &g.adj[u] {
            in_deg[v] += 1;
        }
    }

    let mut queue: VecDeque<usize> = (0..n).filter(|&u| in_deg[u] == 0).collect();
    let mut order = Vec::with_capacity(n);

    while let Some(u) = queue.pop_front() {
        order.push(u);
        for &v in &g.adj[u] {
            in_deg[v] -= 1;
            if in_deg[v] == 0 {
                queue.push_back(v);
            }
        }
    }

    if order.len() < n {
        None
    } else {
        Some(order)
    }
}

fn demo_topo_kahn() {
    println!("=== Demo 5: Topological sort (Kahn) ===");

    let names = ["Math", "Physics", "CS", "Quantum", "AI", "English"];
    let mut g = Graph::new(6, true);
    g.add_edge(0, 1);
    g.add_edge(0, 2);
    g.add_edge(1, 3);
    g.add_edge(2, 3);
    g.add_edge(2, 4);

    match topo_sort_kahn(&g) {
        Some(order) => {
            let parts: Vec<String> = order.iter()
                .map(|&v| format!("{}({})", names[v], v))
                .collect();
            println!("Kahn order: {}", parts.join(" → "));
        }
        None => println!("ERROR: cycle detected!"),
    }

    // Compare both methods
    let order_dfs = topo_sort_dfs(&g).unwrap();
    let order_kahn = topo_sort_kahn(&g).unwrap();
    println!("Same result? {} (both valid, may differ)",
             if order_dfs == order_kahn { "identical" }
             else { "different but both valid" });

    // Verify: for each edge u→v, u appears before v
    let mut pos_dfs = vec![0; g.n()];
    for (i, &v) in order_dfs.iter().enumerate() { pos_dfs[v] = i; }
    let valid = (0..g.n()).all(|u| {
        g.adj[u].iter().all(|&v| pos_dfs[u] < pos_dfs[v])
    });
    println!("DFS order valid? {}", valid);
    println!();
}

/* ======== Demo 6: Bridges and articulation points ================= */

fn find_bridges_and_artics(g: &Graph) -> (Vec<(usize, usize)>, Vec<usize>) {
    let n = g.n();
    let mut disc = vec![0u32; n];
    let mut low = vec![0u32; n];
    let mut parent = vec![usize::MAX; n];
    let mut timer = 0u32;
    let mut bridges = vec![];
    let mut is_artic = vec![false; n];

    fn dfs(
        u: usize, g: &Graph,
        disc: &mut [u32], low: &mut [u32], parent: &mut [usize],
        timer: &mut u32,
        bridges: &mut Vec<(usize, usize)>,
        is_artic: &mut [bool],
    ) {
        *timer += 1;
        disc[u] = *timer;
        low[u] = *timer;
        let mut children = 0u32;

        for &v in &g.adj[u] {
            if disc[v] == 0 {
                children += 1;
                parent[v] = u;
                dfs(v, g, disc, low, parent, timer, bridges, is_artic);

                low[u] = low[u].min(low[v]);

                // Bridge
                if low[v] > disc[u] {
                    bridges.push((u, v));
                }

                // Articulation point
                if parent[u] == usize::MAX && children > 1 {
                    is_artic[u] = true;
                }
                if parent[u] != usize::MAX && low[v] >= disc[u] {
                    is_artic[u] = true;
                }
            } else if v != parent[u] {
                low[u] = low[u].min(disc[v]);
            }
        }
    }

    for u in 0..n {
        if disc[u] == 0 {
            dfs(u, g, &mut disc, &mut low, &mut parent,
                &mut timer, &mut bridges, &mut is_artic);
        }
    }

    let artics: Vec<usize> = (0..n).filter(|&u| is_artic[u]).collect();
    (bridges, artics)
}

fn demo_bridges() {
    println!("=== Demo 6: Bridges and articulation points ===");

    // 0-1-4-3-0 (cycle), 1-2 (bridge), 2-5-6-7-5 (5-6-7 cycle)
    let mut g = Graph::new(8, false);
    g.add_edge(0, 1);
    g.add_edge(0, 3);
    g.add_edge(1, 4);
    g.add_edge(3, 4);
    g.add_edge(1, 2);
    g.add_edge(2, 5);
    g.add_edge(5, 6);
    g.add_edge(5, 7);
    g.add_edge(6, 7);

    println!("Graph: 0-1-4-3-0 (cycle), 1-2 (bridge?), 2-5 (bridge?)");
    println!("       5-6-7-5 (cycle)\n");

    let (bridges, artics) = find_bridges_and_artics(&g);

    println!("Bridges ({}):", bridges.len());
    for (u, v) in &bridges {
        println!("  {} — {}", u, v);
    }

    let artic_str: Vec<String> = artics.iter().map(|v| v.to_string()).collect();
    println!("\nArticulation points: {{{}}}", artic_str.join(", "));

    // Verify: removing a bridge should increase component count
    println!("\nVerification — removing bridge {}—{}:", bridges[0].0, bridges[0].1);
    let mut g2 = Graph::new(8, false);
    for u in 0..g.n() {
        for &v in &g.adj[u] {
            if u < v && !((u == bridges[0].0 && v == bridges[0].1)
                       || (u == bridges[0].1 && v == bridges[0].0)) {
                g2.add_edge(u, v);
            }
        }
    }
    let (orig_comp, _) = connected_components(&g);
    let (new_comp, _) = connected_components(&g2);
    println!("  Components: {} → {} (increased by {})",
             orig_comp, new_comp, new_comp - orig_comp);
    println!();
}

/* ======== Demo 7: Topological sort — all valid orderings ========== */

fn all_topo_sorts(g: &Graph) -> Vec<Vec<usize>> {
    let n = g.n();
    let mut in_deg = vec![0usize; n];
    for u in 0..n {
        for &v in &g.adj[u] { in_deg[v] += 1; }
    }

    let mut result = vec![];
    let mut current = vec![];
    let mut used = vec![false; n];

    fn backtrack(
        g: &Graph, in_deg: &mut [usize], used: &mut [bool],
        current: &mut Vec<usize>, result: &mut Vec<Vec<usize>>,
    ) {
        let n = g.n();
        if current.len() == n {
            result.push(current.clone());
            return;
        }
        for u in 0..n {
            if !used[u] && in_deg[u] == 0 {
                used[u] = true;
                current.push(u);
                for &v in &g.adj[u] { in_deg[v] -= 1; }

                backtrack(g, in_deg, used, current, result);

                current.pop();
                used[u] = false;
                for &v in &g.adj[u] { in_deg[v] += 1; }
            }
        }
    }

    backtrack(g, &mut in_deg, &mut used, &mut current, &mut result);
    result
}

fn demo_all_topo() {
    println!("=== Demo 7: All topological orderings ===");

    // Diamond: A→B, A→C, B→D, C→D
    let mut g = Graph::new(4, true);
    g.add_edge(0, 1); // A→B
    g.add_edge(0, 2); // A→C
    g.add_edge(1, 3); // B→D
    g.add_edge(2, 3); // C→D

    let names = ["A", "B", "C", "D"];
    println!("Diamond DAG: A→B, A→C, B→D, C→D\n");

    let all = all_topo_sorts(&g);
    println!("{} valid topological orderings:", all.len());
    for (i, order) in all.iter().enumerate() {
        let parts: Vec<&str> = order.iter().map(|&v| names[v]).collect();
        println!("  {}: {}", i + 1, parts.join(" → "));
    }

    // Chain: only 1 order
    let mut chain = Graph::new(4, true);
    chain.add_edge(0, 1);
    chain.add_edge(1, 2);
    chain.add_edge(2, 3);
    let chain_all = all_topo_sorts(&chain);
    println!("\nChain A→B→C→D: {} ordering(s)", chain_all.len());

    // No edges: n! orderings
    let isolated = Graph::new(3, true);
    let iso_all = all_topo_sorts(&isolated);
    println!("3 isolated vertices: {} orderings (= 3!)", iso_all.len());
    println!();
}

/* ======== Demo 8: Practical — build system dependency order ======= */

fn demo_build_system() {
    println!("=== Demo 8: Build system dependency order ===");

    let packages = [
        "libc", "libm", "zlib", "openssl", "curl",
        "sqlite", "python", "flask",
    ];
    let n = packages.len();
    let mut g = Graph::new(n, true);

    // Dependencies (package → dependency it requires)
    // We model: if A depends on B, edge B→A (B must build before A)
    g.add_edge(0, 1); // libc → libm
    g.add_edge(0, 2); // libc → zlib
    g.add_edge(1, 3); // libm → openssl
    g.add_edge(2, 3); // zlib → openssl
    g.add_edge(3, 4); // openssl → curl
    g.add_edge(0, 5); // libc → sqlite
    g.add_edge(0, 6); // libc → python
    g.add_edge(5, 6); // sqlite → python
    g.add_edge(3, 6); // openssl → python
    g.add_edge(6, 7); // python → flask
    g.add_edge(4, 7); // curl → flask

    println!("Build dependencies:");
    for u in 0..n {
        if !g.adj[u].is_empty() {
            let deps: Vec<&str> = g.adj[u].iter().map(|&v| packages[v]).collect();
            println!("  {} must build before: {}", packages[u], deps.join(", "));
        }
    }

    // DFS topological sort
    match topo_sort_dfs(&g) {
        Some(order) => {
            println!("\nBuild order (DFS):");
            for (i, &v) in order.iter().enumerate() {
                println!("  {}. {}", i + 1, packages[v]);
            }
        }
        None => println!("\nERROR: circular dependency!"),
    }

    // Kahn with level info (parallel build opportunities)
    let mut in_deg = vec![0usize; n];
    for u in 0..n {
        for &v in &g.adj[u] { in_deg[v] += 1; }
    }

    let mut queue: VecDeque<usize> = (0..n)
        .filter(|&u| in_deg[u] == 0)
        .collect();
    let mut level = 0;

    println!("\nParallel build levels (Kahn):");
    while !queue.is_empty() {
        let batch: Vec<usize> = queue.drain(..).collect();
        let names_batch: Vec<&str> = batch.iter().map(|&v| packages[v]).collect();
        println!("  Level {}: [{}]", level, names_batch.join(", "));

        for &u in &batch {
            for &v in &g.adj[u] {
                in_deg[v] -= 1;
                if in_deg[v] == 0 {
                    queue.push_back(v);
                }
            }
        }
        level += 1;
    }
    println!("  → {} levels (critical path length)", level);
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_cycle_directed();
    demo_cycle_undirected();
    demo_components();
    demo_topo_dfs();
    demo_topo_kahn();
    demo_bridges();
    demo_all_topo();
    demo_build_system();
}
```

### Salida esperada

```
=== Demo 1: Cycle detection (directed) ===
Edges: 0→1, 1→2, 2→3, 3→4, 4→2
Cycle: 2 → 3 → 4 → 2
DAG 0→1,2; 1,2→3: no cycle

=== Demo 2: Cycle detection (undirected) ===
Graph: 0-1-2-0 (triangle), 3-4
Cycle: 0 — 2 — 1 — 0
Tree 0-1-2, 1-3: no cycle

=== Demo 3: Connected components ===
4 components:
  Component 0: {0, 1, 2, 3} — 4 vertices, 4 edges
  Component 1: {4, 5} — 2 vertices, 1 edges (tree)
  Component 2: {6} — 1 vertices, 0 edges (tree)
  Component 3: {7, 8, 9} — 3 vertices, 2 edges (tree)
Is forest? m=7 == n-k=6? NO

=== Demo 4: Topological sort (DFS) ===
DAG: Math→Physics, Math→CS, Physics→Quantum, CS→Quantum, CS→AI
     English (independent)

DFS topo order: English(5) → Math(0) → CS(2) → AI(4) → Physics(1) → Quantum(3)
Cyclic 0→1→2→0: cycle detected

=== Demo 5: Topological sort (Kahn) ===
Kahn order: Math(0) → English(5) → Physics(1) → CS(2) → Quantum(3) → AI(4)
Same result? different but both valid
DFS order valid? true

=== Demo 6: Bridges and articulation points ===
Graph: 0-1-4-3-0 (cycle), 1-2 (bridge?), 2-5 (bridge?)
       5-6-7-5 (cycle)

Bridges (2):
  1 — 2
  2 — 5

Articulation points: {1, 2}

Verification — removing bridge 1—2:
  Components: 1 → 2 (increased by 1)

=== Demo 7: All topological orderings ===
Diamond DAG: A→B, A→C, B→D, C→D

2 valid topological orderings:
  1: A → B → C → D
  2: A → C → B → D

Chain A→B→C→D: 1 ordering(s)
3 isolated vertices: 6 orderings (= 3!)

=== Demo 8: Build system dependency order ===
Build dependencies:
  libc must build before: libm, zlib, sqlite, python
  libm must build before: openssl
  zlib must build before: openssl
  openssl must build before: curl, python
  curl must build before: flask
  sqlite must build before: python
  python must build before: flask

Build order (DFS):
  1. libc
  2. sqlite
  3. zlib
  4. libm
  5. openssl
  6. python
  7. curl
  8. flask

Parallel build levels (Kahn):
  Level 0: [libc]
  Level 1: [libm, zlib, sqlite]
  Level 2: [openssl]
  Level 3: [curl, python]
  Level 4: [flask]
  → 5 levels (critical path length)
```

---

## 8. Ejercicios

### Ejercicio 1 — Detección de ciclo dirigido a mano

Dado el grafo dirigido con aristas:
$0 \to 1,\ 1 \to 2,\ 2 \to 3,\ 3 \to 1,\ 0 \to 4,\ 4 \to 5$.

Ejecuta DFS desde el vértice 0 y muestra en cada paso el color de cada
vértice. Indica exactamente cuándo se detecta la back edge y qué ciclo
forma.

<details>
<summary>¿En qué arista se detecta el ciclo?</summary>

Al explorar la arista $3 \to 1$: el vértice 1 está GRAY (aún en la pila
de recursión), por lo que es una back edge. El ciclo es $1 \to 2 \to 3 \to 1$.
El vértice 4 y 5 nunca llegan a explorarse si se termina al primer ciclo.
</details>

---

### Ejercicio 2 — Ciclo en grafo no dirigido: caso límite

Grafo no dirigido: vértices $\{0, 1, 2, 3, 4\}$,
aristas $\{0\text{-}1,\ 1\text{-}2,\ 2\text{-}3,\ 3\text{-}4\}$.

a) ¿Tiene ciclo? Ejecuta DFS desde 0 y verifica.
b) Agrega la arista $4\text{-}1$. Repite DFS. ¿En qué paso se detecta el
   ciclo y cuál es?

<details>
<summary>¿Qué cambia al agregar 4-1?</summary>

a) Sin $4\text{-}1$: es un path (árbol), no hay ciclo. Cada vecino visitado
es el padre, nunca se activa la condición `v ≠ parent[u]`.

b) Con $4\text{-}1$: al visitar vértice 4, su vecino 1 ya está visitado y
$1 \neq \text{parent}[4] = 3$. Se detecta el ciclo
$1 \to 2 \to 3 \to 4 \to 1$ (longitud 4).
</details>

---

### Ejercicio 3 — Componentes conexos con análisis

Grafo no dirigido con 12 vértices y aristas:
$\{0\text{-}1,\ 1\text{-}2,\ 2\text{-}0,\ 3\text{-}4,\ 4\text{-}5,\ 5\text{-}3,\ 6\text{-}7,\ 8\text{-}9,\ 9\text{-}10,\ 10\text{-}8\}$.
El vértice 11 es aislado.

a) ¿Cuántos componentes hay?
b) ¿Cuáles son árboles?
c) ¿Es el grafo un bosque?

<details>
<summary>Respuesta</summary>

a) **5 componentes**: $\{0,1,2\}$, $\{3,4,5\}$, $\{6,7\}$, $\{8,9,10\}$,
$\{11\}$.

b) Árboles son los componentes con $m_i = n_i - 1$:
- $\{0,1,2\}$: 3 aristas, 3 vértices → $m \neq n-1$ → **no** es árbol (tiene ciclo).
- $\{3,4,5\}$: 3 aristas, 3 vértices → **no** (ciclo).
- $\{6,7\}$: 1 arista, 2 vértices → **sí**, es árbol.
- $\{8,9,10\}$: 3 aristas, 3 vértices → **no** (ciclo).
- $\{11\}$: 0 aristas, 1 vértice → **sí**, es árbol (trivial).

c) Bosque requiere $m = n - k$: $m = 10$, $n = 12$, $k = 5$.
$10 \neq 12 - 5 = 7$ → **no** es bosque.
</details>

---

### Ejercicio 4 — Topological sort a mano (DFS)

DAG con 7 vértices y aristas:
$0 \to 1,\ 0 \to 2,\ 1 \to 3,\ 2 \to 3,\ 2 \to 4,\ 3 \to 5,\ 4 \to 5,\ 4 \to 6$.

Ejecuta DFS empezando por el vértice 0 (explora vecinos en orden numérico).
Registra `disc[]` y `fin[]`, luego ordena por `fin[]` decreciente.

<details>
<summary>¿Cuál es el orden topológico resultante?</summary>

DFS desde 0: $0 \to 1 \to 3 \to 5$ (fin: 5=1, 3=2, vuelta) $\to 2 \to 4
\to 6$ (fin: 6=3, 4=4, vuelta a 2, visita 3 ya BLACK) fin 2=5, fin 1=6 (vuelta,
visita 2 ya BLACK) fin 0=7. Pero 1 no tiene 2 como vecino.

Corrección con exploración cuidadosa:
- Visit 0(d=1): vecinos 1, 2
  - Visit 1(d=2): vecino 3
    - Visit 3(d=3): vecino 5
      - Visit 5(d=4): sin vecinos → fin 5=5
    - Fin 3=6
  - Fin 1=7
  - Visit 2(d=8): vecinos 3(BLACK), 4
    - Visit 4(d=9): vecinos 5(BLACK), 6
      - Visit 6(d=10): sin vecinos → fin 6=11
    - Fin 4=12
  - Fin 2=13
- Fin 0=14

Post-order: [5, 3, 1, 6, 4, 2, 0]. Invertido: **0, 2, 4, 6, 1, 3, 5**.
</details>

---

### Ejercicio 5 — DFS vs Kahn: ¿mismos resultados?

Usando el mismo DAG del ejercicio 4, aplica el algoritmo de Kahn (con cola
FIFO, desempatando por menor índice).

<details>
<summary>¿Los órdenes coinciden?</summary>

Kahn: in-degree iniciales: 0=0, 1=1, 2=1, 3=2, 4=1, 5=2, 6=1.
Cola inicial: {0}. Dequeue 0 → reduce 1(→0), 2(→0). Cola: {1, 2}.
Dequeue 1 → reduce 3(→1). Cola: {2}. Dequeue 2 → reduce 3(→0), 4(→0).
Cola: {3, 4}. Dequeue 3 → reduce 5(→1). Cola: {4}. Dequeue 4 → reduce
5(→0), 6(→0). Cola: {5, 6}. Dequeue 5, dequeue 6.

Kahn: **0, 1, 2, 3, 4, 5, 6**.
DFS: **0, 2, 4, 6, 1, 3, 5**.

**No coinciden** — ambos son válidos pero diferentes. Kahn con FIFO tiende a
producir un resultado más "nivelado"; DFS produce un orden que refleja la
exploración en profundidad.
</details>

---

### Ejercicio 6 — Puentes a mano

Grafo no dirigido:
```
0 — 1 — 2 — 3
    |       |
    4 ———— 5
```
Aristas: $\{0\text{-}1,\ 1\text{-}2,\ 2\text{-}3,\ 3\text{-}5,\ 5\text{-}4,\ 4\text{-}1\}$.

Calcula `disc[]` y `low[]` con DFS desde 0. ¿Cuáles son los puentes?

<details>
<summary>Respuesta</summary>

DFS desde 0: $0(d=1) \to 1(d=2) \to 2(d=3) \to 3(d=4) \to 5(d=5) \to 4(d=6)$,
back edge $4\text{-}1$ ($\text{disc}[1]=2$), retroceso.

low[]: 4: min(6, disc[1]=2) = 2. 5: min(5, low[4]=2) = 2. 3: min(4, low[5]=2)
= 2. 2: min(3, low[3]=2) = 2. 1: min(2, low[2]=2, low[4]=2) = 2. 0: low=1.

Puentes: arista $(u,v)$ donde $\text{low}[v] > \text{disc}[u]$:
- $0\text{-}1$: low[1]=2 > disc[0]=1 → **SÍ, puente**.
- $1\text{-}2$: low[2]=2 > disc[1]=2 → NO (igual, no mayor).
- Todas las demás: low $\leq$ disc del padre → no son puentes.

**Único puente**: $0\text{-}1$. El ciclo $1\text{-}2\text{-}3\text{-}5\text{-}4\text{-}1$
protege todas las aristas del ciclo.
</details>

---

### Ejercicio 7 — Puntos de articulación

Usando el mismo grafo del ejercicio 6, identifica los puntos de articulación.

<details>
<summary>Respuesta</summary>

Reglas:
- Raíz (vértice 0): es articulación si tiene $\geq 2$ hijos en el árbol DFS.
  Vértice 0 tiene 1 hijo (1) → **no** es articulación.
- No-raíz: es articulación si algún hijo $v$ tiene $\text{low}[v] \geq \text{disc}[u]$.

Vértice 1: hijos en DFS = {2, 4} (si 4 se explora como segundo hijo) o {2}
(si 4 se alcanza por el ciclo). Con la exploración previa, 4 se alcanzó desde
5, no desde 1 directamente como tree edge.

Revisando: el tree edge path es $1\to2\to3\to5\to4$. Vértice 1 tiene un solo
hijo tree (2). low[2]=2 $\geq$ disc[1]=2 → **1 es articulación**.

Vértice 2: hijo 3, low[3]=2 < disc[2]=3 → no.
Vértices 3,4,5: todos tienen low que sube al menos hasta 2 → no.

**Único punto de articulación**: vértice 1 (su eliminación separa 0 del resto).
</details>

---

### Ejercicio 8 — Implementar detección de ciclo con extracción

Escribe en C o Rust una función que, dado un grafo **dirigido**, retorne:
- Si no hay ciclo: un mensaje indicándolo.
- Si hay ciclo: el ciclo concreto como lista de vértices.

Prueba con estos grafos:
1. Aristas: $0\to1, 1\to2, 2\to0$ → ciclo $0\to1\to2\to0$.
2. Aristas: $0\to1, 0\to2, 1\to3, 2\to3$ → sin ciclo.
3. Aristas: $0\to1, 1\to2, 2\to3, 3\to4, 4\to1$ → ciclo $1\to2\to3\to4\to1$.

<details>
<summary>Pista clave de implementación</summary>

El array `parent[]` permite reconstruir el ciclo. Cuando se detecta la back
edge $u \to v$ (con $v$ GRAY): recorre `parent[]` desde $u$ hacia atrás hasta
llegar a $v$. Eso da el camino $v \to \cdots \to u$, y con la arista $u \to v$
se cierra el ciclo. Ver Demo 1 en los programas.
</details>

---

### Ejercicio 9 — Kahn lexicográficamente menor

Implementa Kahn con un **min-heap** (en C: implementar o usar qsort; en Rust:
`BinaryHeap` con `Reverse`) para producir el ordenamiento topológico
**lexicográficamente menor**.

DAG: $0\to2, 0\to3, 1\to3, 1\to4, 2\to5, 3\to5, 4\to5$.

<details>
<summary>¿Cuál es el orden lexicográficamente menor?</summary>

In-degrees: 0=0, 1=0, 2=1, 3=2, 4=1, 5=3.
Min-heap: {0, 1}. Extraer 0 → reduce 2(→0), 3(→1). Heap: {1, 2}.
Extraer 1 → reduce 3(→0), 4(→0). Heap: {2, 3, 4}.
Extraer 2 → reduce 5(→2). Heap: {3, 4}.
Extraer 3 → reduce 5(→1). Heap: {4}.
Extraer 4 → reduce 5(→0). Heap: {5}.
Extraer 5.

Orden: **0, 1, 2, 3, 4, 5**. Con FIFO normal el resultado podría ser
diferente (depende del orden de inserción).
</details>

---

### Ejercicio 10 — Aplicación integrada: schedule de tareas

Un proyecto tiene 8 tareas (0-7) con estas dependencias (A→B significa "A
debe completarse antes de B"):

| Tarea | Depende de |
|:-----:|:----------:|
| 0 | — |
| 1 | 0 |
| 2 | 0 |
| 3 | 1, 2 |
| 4 | 1 |
| 5 | 3, 4 |
| 6 | 2 |
| 7 | 5, 6 |

a) Dibuja el DAG.
b) Encuentra un orden topológico con DFS (empezando por 0).
c) Encuentra el orden con Kahn.
d) ¿Cuántos niveles paralelos tiene el proyecto? (Usa Kahn por niveles.)
e) ¿Cuál es el camino crítico (ruta más larga)?

<details>
<summary>Respuesta</summary>

a) Aristas: $0\to1, 0\to2, 1\to3, 2\to3, 1\to4, 3\to5, 4\to5, 2\to6, 5\to7, 6\to7$.

b) DFS desde 0 (vecinos en orden): $0\to1\to3\to5\to7$ (fin 7,5,3), $\to4$
(fin 4), fin 1. $\to2\to6$ (fin 6), fin 2. fin 0.
Post-order inverso: **0, 2, 6, 1, 4, 3, 5, 7**.

c) Kahn (FIFO): in-deg: 0=0, 1=1, 2=1, 3=2, 4=1, 5=2, 6=1, 7=2.
Queue: {0}→{1,2}→{3,4,6}→{5}→{7}.
Orden: **0, 1, 2, 3, 4, 6, 5, 7**.

d) **5 niveles**: [0], [1,2], [3,4,6], [5], [7].

e) Camino crítico (ruta más larga): $0\to1\to3\to5\to7$ = longitud **4**
(5 tareas secuenciales). Reducir el tiempo total del proyecto requiere
optimizar estas tareas.
</details>
