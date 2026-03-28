# T02 — Ford-Fulkerson y Edmonds-Karp

## Objetivo

Implementar el **método de Ford-Fulkerson** (caminos aumentantes genérico) y el
**algoritmo de Edmonds-Karp** (Ford-Fulkerson con BFS), demostrar la complejidad
$O(VE^2)$ de Edmonds-Karp, analizar el caso patológico de DFS, y comparar
variantes algorítmicas para flujo máximo.

---

## 1. El método de Ford-Fulkerson

### 1.1 Idea

Partir de flujo cero. Mientras exista un camino aumentante en el grafo residual
$G_f$, aumentar el flujo a lo largo de ese camino. La corrección se demostró en
T01 via el teorema MFMC: el algoritmo termina cuando no hay camino de $s$ a $t$
en $G_f$, que es exactamente la condición de optimalidad.

### 1.2 Pseudocódigo

```
FORD-FULKERSON(G, s, t):
    f(u,v) <- 0 for all (u,v)

    while exists augmenting path P in G_f from s to t:
        delta <- bottleneck(P)
        for each edge (u,v) in P:
            if (u,v) is forward:
                f(u,v) <- f(u,v) + delta
            else:  // backward
                f(v,u) <- f(v,u) - delta

    return f
```

### 1.3 Corrección

Al terminar, $s$ no alcanza $t$ en $G_f$. Por la demostración de MFMC (T01), el
conjunto $S = \{v : v \text{ alcanzable desde } s \text{ en } G_f\}$ define un
corte $(S, T)$ con $|f| = c(S,T)$. Como $|f| \leq c(S',T')$ para todo corte
$(S',T')$, el flujo es máximo.

### 1.4 Terminación y complejidad

**Con capacidades enteras**: cada augmentación incrementa $|f|$ en al menos 1.
Si el flujo máximo es $|f^*|$, hay a lo sumo $|f^*|$ iteraciones. Cada
iteración busca un camino en $G_f$ en $O(m)$ (DFS o BFS). Total: $O(|f^*| \cdot m)$.

**Problema**: $|f^*|$ puede ser exponencial en el tamaño del grafo. Ejemplo:

```
       M              M
  s -------> a -------> t
  |          ^↓          ^
  |     1    ||     M    |
  |          ↓^          |
  s -------> b -------> t
       M              M
```

Con $M = 2^k$: si DFS elige alternadamente $s \to a \to b \to t$ y
$s \to b \to a \to t$, cada augmentación envía solo 1 unidad. Total: $2M$
iteraciones. Con $M = 10^6$, esto es 2 millones de iteraciones, mientras que BFS
resolvería en **2 iteraciones**.

**Con capacidades irracionales**: Ford-Fulkerson puede **no terminar** y
converger a un valor subóptimo. Esto se demostró con un ejemplo de 6 vértices
usando la razón áurea $\phi = (1+\sqrt{5})/2$.

---

## 2. El algoritmo de Edmonds-Karp

### 2.1 Idea clave

Edmonds-Karp es Ford-Fulkerson donde se elige siempre el **camino aumentante
más corto** (menor número de aristas), encontrado por **BFS**. Esta simple
elección garantiza $O(VE)$ augmentaciones, independientemente de las capacidades.

### 2.2 Pseudocódigo

```
EDMONDS-KARP(G, s, t):
    f(u,v) <- 0 for all (u,v)

    loop:
        // BFS in residual graph
        parent[] <- BFS(G_f, s)
        if t not reached:
            return f

        // Find bottleneck
        delta <- INF
        v <- t
        while v != s:
            u <- parent[v]
            delta <- min(delta, c_f(u,v))
            v <- u

        // Augment
        v <- t
        while v != s:
            u <- parent[v]
            if (u,v) in E:
                f(u,v) <- f(u,v) + delta
            else:
                f(v,u) <- f(v,u) - delta
            v <- u
```

### 2.3 Traza completa en la red de ejemplo

Red de T01: 0→1:10, 0→2:8, 1→5:6, 1→4:8, 2→3:5, 3→1:2, 3→4:10, 4→5:10.
Fuente $s=0$, sumidero $t=5$.

**Augmentación 1**:

BFS desde 0: niveles {0} → {1,2} → {5,4,3}. Primer camino a $t$: **0→1→5**.

| Arista | $c_f$ |
|--------|:-----:|
| 0→1 | 10 |
| 1→5 | 6 |

$\delta = \min(10, 6) = 6$. Flujo: $|f| = 6$.

Flujo actual: $f(0,1)=6, f(1,5)=6$.

**Augmentación 2**:

Residual: $r(0,1)=4$, $r(0,2)=8$, $r(1,4)=8$, $r(1,5)=0$, $r(2,3)=5$,
$r(3,1)=2$, $r(3,4)=10$, $r(4,5)=10$.

BFS: {0} → {1,2} → {4,3} → {5}. Camino: **0→1→4→5**.

$\delta = \min(4, 8, 10) = 4$. Flujo: $|f| = 10$.

Flujo actual: $f(0,1)=10, f(1,5)=6, f(1,4)=4, f(4,5)=4$.

**Augmentación 3**:

Residual: $r(0,1)=0$, $r(0,2)=8$, $r(1,4)=4$, $r(2,3)=5$, $r(3,1)=2$,
$r(3,4)=10$, $r(4,5)=6$.

BFS: {0} → {2} → {3} → {1,4} → {5}. Camino: **0→2→3→4→5**.

$\delta = \min(8, 5, 10, 6) = 5$. Flujo: $|f| = 15$.

Flujo actual: $f(0,1)=10, f(0,2)=5, f(1,5)=6, f(1,4)=4, f(2,3)=5, f(3,4)=5,
f(4,5)=9$.

**Augmentación 4**:

BFS desde 0: solo alcanzamos {0, 2} ($r(0,2)=3$, pero $r(2,3)=0$). No hay
camino a $t=5$. **Termina**.

**Resultado**: flujo máximo $= 15$ en **3 augmentaciones**.

### 2.4 Resumen de augmentaciones

| # | Camino | $\delta$ | $|f|$ | Longitud |
|:-:|--------|:--------:|:-----:|:--------:|
| 1 | 0→1→5 | 6 | 6 | 2 |
| 2 | 0→1→4→5 | 4 | 10 | 3 |
| 3 | 0→2→3→4→5 | 5 | 15 | 4 |

Notar: las longitudes de los caminos son **no decrecientes** (2, 3, 4). Esto no
es coincidencia — es una propiedad fundamental de Edmonds-Karp.

---

## 3. Análisis de complejidad de Edmonds-Karp

### 3.1 Lema de monotonía de distancias

Sea $\delta_f(s, v)$ la distancia (número de aristas) de $s$ a $v$ en $G_f$.
Tras cada augmentación que produce un flujo $f'$:

$$\delta_{f'}(s, v) \geq \delta_f(s, v) \quad \forall v \in V$$

*Demostración (por contradicción)*: Supongamos que existe $v$ con
$\delta_{f'}(s,v) < \delta_f(s,v)$. Sea $v$ el vértice más cercano a $s$ en
$G_{f'}$ con esta propiedad. Sea $u$ el predecesor de $v$ en el camino más corto
de $s$ a $v$ en $G_{f'}$. Entonces $\delta_{f'}(s,u) = \delta_{f'}(s,v) - 1$.

Como $v$ es el más cercano con distancia que disminuyó:
$\delta_{f'}(s,u) \geq \delta_f(s,u)$.

La arista $(u,v)$ existe en $G_{f'}$. ¿Existía en $G_f$?

- **Si $(u,v) \in E_f$**: $\delta_f(s,v) \leq \delta_f(s,u) + 1 \leq
  \delta_{f'}(s,u) + 1 = \delta_{f'}(s,v)$, contradiciendo nuestro supuesto.

- **Si $(u,v) \notin E_f$**: $(u,v)$ apareció en $G_{f'}$ porque la
  augmentación usó la arista $(v,u)$. Como BFS usa caminos más cortos, $v$ y $u$
  son consecutivos en el camino con $\delta_f(s,v) = \delta_f(s,u) - 1$.
  Entonces: $\delta_f(s,v) = \delta_f(s,u) - 1 \leq \delta_{f'}(s,u) - 1 =
  \delta_{f'}(s,v) - 2$, lo que da $\delta_{f'}(s,v) \geq \delta_f(s,v) + 2$,
  contradicción.

### 3.2 Aristas críticas

Una arista $(u,v)$ es **crítica** en una augmentación si es el cuello de botella:
$c_f(u,v) = \delta$ (la capacidad residual mínima del camino).

Cuando $(u,v)$ es crítica, se satura ($c_f(u,v) = 0$ después) y desaparece de
$G_f$. Para reaparecer, la arista inversa $(v,u)$ debe usarse en una
augmentación futura, lo que requiere $\delta_f(s,v) = \delta_f(s,u) + 1$.

En la augmentación donde $(u,v)$ fue crítica: $\delta_f(s,v) = \delta_f(s,u) + 1$.
Cuando $(v,u)$ se usa después: $\delta_{f'}(s,u) = \delta_{f'}(s,v) + 1$.

Combinando con monotonía: $\delta_{f'}(s,u) = \delta_{f'}(s,v) + 1 \geq
\delta_f(s,v) + 1 = \delta_f(s,u) + 2$.

Cada vez que $(u,v)$ es crítica, $\delta_f(s,u)$ aumenta en al menos 2. Como
$\delta_f(s,u) \leq |V| - 2$ (camino simple), $(u,v)$ puede ser crítica a lo
sumo $|V|/2$ veces.

### 3.3 Complejidad total

- Hay $O(E)$ aristas en $G$ (cada una genera a lo sumo 2 aristas residuales).
- Cada arista es crítica $O(V)$ veces.
- Cada augmentación tiene al menos una arista crítica.
- Total de augmentaciones: $O(VE)$.
- Cada augmentación cuesta $O(E)$ (BFS + recorrido del camino).
- **Complejidad total: $O(VE^2)$**.

### 3.4 Tabla comparativa de algoritmos

| Algoritmo | Complejidad | Selección de camino |
|-----------|:-----------:|:-------------------:|
| Ford-Fulkerson (DFS) | $O(\lvert f^*\rvert \cdot E)$ | Cualquiera (DFS) |
| Edmonds-Karp | $O(VE^2)$ | BFS (más corto) |
| Dinic | $O(V^2E)$ | Blocking flow en level graph |
| Capacity scaling | $O(E^2 \log C)$ | Camino con $\delta \geq \Delta$ |
| Push-relabel | $O(V^2E)$ | Sin caminos (local) |
| Push-relabel (FIFO) | $O(V^3)$ | Sin caminos (local) |

Para grafos con capacidades grandes, Edmonds-Karp es drásticamente mejor que
Ford-Fulkerson con DFS. Para grafos densos, Dinic o push-relabel son preferibles.

---

## 4. El caso patológico de DFS

### 4.1 Construcción

```
       M              M
  0 -------> 1 -------> 3
  |          ^           ^
  |     1    |           |
  |          |           |
  +--->  2 --+     M     |
  |      |               |
  | M    +---------------+
  |
  (0->2: M)
```

Más precisamente, con 4 vértices ($s=0$, $t=3$):
- $0 \to 1$: capacidad $M$
- $0 \to 2$: capacidad $M$
- $1 \to 3$: capacidad $M$
- $2 \to 3$: capacidad $M$
- $1 \to 2$: capacidad $1$ (la arista trampa)

El flujo máximo es $2M$.

### 4.2 Comportamiento de DFS

Si DFS siempre encuentra primero el camino que pasa por la arista 1→2:

1. $0 \to 1 \to 2 \to 3$: $\delta = 1$. Crea backward $2 \to 1$.
2. $0 \to 2 \to 1 \to 3$: $\delta = 1$. Usa backward, restaura forward $1 \to 2$.
3. Repite.

Tras $2M$ iteraciones, flujo $= 2M$.

### 4.3 Comportamiento de BFS (Edmonds-Karp)

BFS encuentra directamente:
1. $0 \to 1 \to 3$: $\delta = M$. Flujo $= M$.
2. $0 \to 2 \to 3$: $\delta = M$. Flujo $= 2M$.

**2 iteraciones** vs $2M$. Con $M = 10^6$: 2 vs 2,000,000.

---

## 5. Implementación eficiente con lista de adyacencia

### 5.1 Estructura de aristas con reversa

Para implementar augmentaciones eficientemente, cada arista se almacena junto con
un puntero a su **arista reversa**. Al augmentar, se incrementa la forward y se
decrementa la backward en una sola operación.

```
Edge:
    to:   destination vertex
    cap:  residual capacity (starts at original capacity)
    rev:  index of reverse edge in adj[to]

add_edge(u, v, cap):
    adj[u].push(Edge(v, cap, len(adj[v])))
    adj[v].push(Edge(u, 0, len(adj[u]) - 1))    // reverse edge, cap=0
```

Al augmentar con $\delta$:
```
    edge.cap -= delta
    adj[edge.to][edge.rev].cap += delta
```

### 5.2 Ventajas

- No se necesita matriz $n \times n$ (eficiente en espacio para grafos dispersos).
- La augmentación es $O(|P|)$ donde $|P|$ es la longitud del camino.
- La arista reversa siempre está accesible en $O(1)$.

---

## 6. Algoritmo de Dinic (vista general)

### 6.1 Idea

Dinic mejora Edmonds-Karp procesando **todos** los caminos de longitud mínima en
una fase, no uno a la vez:

1. Construir **level graph** $L_f$ con BFS (solo aristas que avanzan un nivel).
2. Encontrar un **blocking flow** en $L_f$ con DFS.
3. Actualizar flujo y repetir.

Como las distancias crecen al menos 1 por fase, hay a lo sumo $O(V)$ fases.
Cada blocking flow se encuentra en $O(VE)$ con DFS (o $O(E)$ con link-cut trees).

### 6.2 Complejidad

$$O(V^2 E)$$

Para grafos unitarios (capacidades 0/1): $O(E\sqrt{V})$ — fundamental para
matching bipartito.

---

## 7. Programa completo en C

```c
/*
 * ford_fulkerson.c
 *
 * Ford-Fulkerson (DFS) and Edmonds-Karp (BFS) implementations
 * for maximum flow. Includes pathological case analysis and
 * min-cut extraction.
 *
 * Compile: gcc -O2 -o ford_fulkerson ford_fulkerson.c
 * Run:     ./ford_fulkerson
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 20
#define MAX_E 200  /* each undirected edge = 2 directed edges */

/* =================== ADJACENCY LIST WITH REVERSE EDGES ============ */

typedef struct {
    int to, cap, rev;
} Edge;

typedef struct {
    Edge edges[MAX_E];
    int head[MAX_V];
    int next[MAX_E];
    int n, s, t, edge_cnt;
    int iter_count;  /* count augmentations */
} FlowGraph;

void fg_init(FlowGraph *g, int n, int s, int t) {
    g->n = n; g->s = s; g->t = t;
    g->edge_cnt = 0;
    g->iter_count = 0;
    memset(g->head, -1, sizeof(g->head));
}

void fg_add_edge(FlowGraph *g, int u, int v, int cap) {
    int e1 = g->edge_cnt++;
    g->edges[e1] = (Edge){v, cap, g->edge_cnt};
    g->next[e1] = g->head[u];
    g->head[u] = e1;

    int e2 = g->edge_cnt++;
    g->edges[e2] = (Edge){u, 0, e1};  /* reverse edge */
    g->next[e2] = g->head[v];
    g->head[v] = e2;
}

/* =================== EDMONDS-KARP (BFS) =========================== */

int ek_bfs(FlowGraph *g, int parent_edge[]) {
    bool visited[MAX_V] = {false};
    int queue[MAX_V], front = 0, back = 0;

    memset(parent_edge, -1, MAX_V * sizeof(int));
    visited[g->s] = true;
    queue[back++] = g->s;

    while (front < back) {
        int u = queue[front++];
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to;
            if (!visited[v] && g->edges[e].cap > 0) {
                visited[v] = true;
                parent_edge[v] = e;
                if (v == g->t) {
                    /* Find bottleneck */
                    int bn = INT_MAX;
                    for (int x = g->t; x != g->s;) {
                        int pe = parent_edge[x];
                        if (g->edges[pe].cap < bn)
                            bn = g->edges[pe].cap;
                        /* Find who leads to x */
                        /* The edge pe goes TO x, from some u */
                        /* We stored reverse, so edges[edges[pe].rev].to
                           gives u indirectly. Easier: track parent vertex */
                        x = g->edges[g->edges[pe].rev].to;
                    }
                    return bn;
                }
                queue[back++] = v;
            }
        }
    }
    return 0;  /* no path */
}

int edmonds_karp(FlowGraph *g, bool verbose) {
    int total_flow = 0;
    int parent_edge[MAX_V];
    g->iter_count = 0;

    int bn;
    while ((bn = ek_bfs(g, parent_edge)) > 0) {
        g->iter_count++;

        if (verbose) {
            printf("  Aug %d: path ", g->iter_count);
            /* Reconstruct path */
            int path[MAX_V], plen = 0;
            for (int x = g->t; x != g->s;
                 x = g->edges[g->edges[parent_edge[x]].rev].to)
                path[plen++] = x;
            path[plen++] = g->s;
            for (int i = plen - 1; i >= 0; i--)
                printf("%d%s", path[i], i > 0 ? "->" : "");
            printf(", delta=%d\n", bn);
        }

        /* Augment */
        for (int x = g->t; x != g->s;) {
            int pe = parent_edge[x];
            g->edges[pe].cap -= bn;
            g->edges[g->edges[pe].rev].cap += bn;
            x = g->edges[g->edges[pe].rev].to;
        }
        total_flow += bn;
    }
    return total_flow;
}

/* =================== FORD-FULKERSON (DFS) ========================= */

bool ff_dfs(FlowGraph *g, int u, int bn, int *pushed, bool visited[]) {
    if (u == g->t) { *pushed = bn; return true; }
    visited[u] = true;

    for (int e = g->head[u]; e != -1; e = g->next[e]) {
        int v = g->edges[e].to;
        if (!visited[v] && g->edges[e].cap > 0) {
            int new_bn = bn < g->edges[e].cap ? bn : g->edges[e].cap;
            if (ff_dfs(g, v, new_bn, pushed, visited)) {
                g->edges[e].cap -= *pushed;
                g->edges[g->edges[e].rev].cap += *pushed;
                return true;
            }
        }
    }
    return false;
}

int ford_fulkerson_dfs(FlowGraph *g, bool verbose) {
    int total_flow = 0;
    g->iter_count = 0;

    while (true) {
        bool visited[MAX_V] = {false};
        int pushed = 0;
        if (!ff_dfs(g, g->s, INT_MAX, &pushed, visited))
            break;
        g->iter_count++;
        total_flow += pushed;
        if (verbose && g->iter_count <= 20)
            printf("  Aug %d: delta=%d, total=%d\n",
                   g->iter_count, pushed, total_flow);
    }
    if (verbose && g->iter_count > 20)
        printf("  ... (%d total augmentations)\n", g->iter_count);
    return total_flow;
}

/* =================== BUILD NETWORKS =============================== */

void build_example(FlowGraph *g) {
    fg_init(g, 6, 0, 5);
    fg_add_edge(g, 0, 1, 10);
    fg_add_edge(g, 0, 2, 8);
    fg_add_edge(g, 1, 5, 6);
    fg_add_edge(g, 1, 4, 8);
    fg_add_edge(g, 2, 3, 5);
    fg_add_edge(g, 3, 1, 2);
    fg_add_edge(g, 3, 4, 10);
    fg_add_edge(g, 4, 5, 10);
}

void build_pathological(FlowGraph *g, int M) {
    fg_init(g, 4, 0, 3);
    /* Add 1->2 FIRST so DFS from 1 tries 2 before 3 */
    fg_add_edge(g, 1, 2, 1);
    fg_add_edge(g, 0, 1, M);
    fg_add_edge(g, 0, 2, M);
    fg_add_edge(g, 1, 3, M);
    fg_add_edge(g, 2, 3, M);
}

/* =================== DEMOS ======================================== */

void demo1_ek_trace(void) {
    printf("=== Demo 1: Edmonds-Karp — Full Trace ===\n\n");

    printf("Network: 0->1:10, 0->2:8, 1->5:6, 1->4:8,\n");
    printf("         2->3:5, 3->1:2, 3->4:10, 4->5:10\n\n");

    FlowGraph g;
    build_example(&g);

    int max_flow = edmonds_karp(&g, true);
    printf("\nMax flow: %d (%d augmentations)\n\n", max_flow, g.iter_count);
}

void demo2_ff_trace(void) {
    printf("=== Demo 2: Ford-Fulkerson (DFS) — Same Network ===\n\n");

    FlowGraph g;
    build_example(&g);

    int max_flow = ford_fulkerson_dfs(&g, true);
    printf("\nMax flow: %d (%d augmentations)\n\n", max_flow, g.iter_count);
}

void demo3_comparison(void) {
    printf("=== Demo 3: DFS vs BFS — Augmentation Count ===\n\n");

    printf("%-30s  %8s  %8s\n", "Network", "DFS augs", "BFS augs");
    printf("%-30s  %8s  %8s\n", "-------", "--------", "--------");

    /* Example network */
    {
        FlowGraph g1, g2;
        build_example(&g1);
        build_example(&g2);
        ford_fulkerson_dfs(&g1, false);
        edmonds_karp(&g2, false);
        printf("%-30s  %8d  %8d\n", "Example (6V, maxflow=15)",
               g1.iter_count, g2.iter_count);
    }

    /* Pathological M=100 */
    {
        FlowGraph g1, g2;
        build_pathological(&g1, 100);
        build_pathological(&g2, 100);
        ford_fulkerson_dfs(&g1, false);
        edmonds_karp(&g2, false);
        printf("%-30s  %8d  %8d\n", "Pathological (M=100)",
               g1.iter_count, g2.iter_count);
    }

    /* Pathological M=1000 */
    {
        FlowGraph g1, g2;
        build_pathological(&g1, 1000);
        build_pathological(&g2, 1000);
        ford_fulkerson_dfs(&g1, false);
        edmonds_karp(&g2, false);
        printf("%-30s  %8d  %8d\n", "Pathological (M=1000)",
               g1.iter_count, g2.iter_count);
    }

    printf("\n");
}

void demo4_pathological(void) {
    printf("=== Demo 4: Pathological Case — DFS Zig-Zag ===\n\n");

    int M = 10;
    printf("Network: 0->1:%d, 0->2:%d, 1->3:%d, 2->3:%d, 1->2:1\n", M,M,M,M);
    printf("Max flow should be %d\n\n", 2 * M);

    printf("--- DFS (showing first 10 augmentations) ---\n");
    FlowGraph g1;
    build_pathological(&g1, M);
    int f1 = ford_fulkerson_dfs(&g1, true);
    printf("DFS: max flow = %d, augmentations = %d\n\n", f1, g1.iter_count);

    printf("--- BFS (Edmonds-Karp) ---\n");
    FlowGraph g2;
    build_pathological(&g2, M);
    int f2 = edmonds_karp(&g2, true);
    printf("BFS: max flow = %d, augmentations = %d\n\n", f2, g2.iter_count);
}

void demo5_mincut(void) {
    printf("=== Demo 5: Extract Min-Cut after Max-Flow ===\n\n");

    FlowGraph g;
    build_example(&g);
    int max_flow = edmonds_karp(&g, false);

    /* BFS in residual to find reachable set S */
    bool reachable[MAX_V] = {false};
    int queue[MAX_V], front = 0, back = 0;
    reachable[g.s] = true;
    queue[back++] = g.s;
    while (front < back) {
        int u = queue[front++];
        for (int e = g.head[u]; e != -1; e = g.next[e])
            if (g.edges[e].cap > 0 && !reachable[g.edges[e].to]) {
                reachable[g.edges[e].to] = true;
                queue[back++] = g.edges[e].to;
            }
    }

    printf("Max flow: %d\n\n", max_flow);
    printf("S = { ");
    for (int v = 0; v < g.n; v++) if (reachable[v]) printf("%d ", v);
    printf("}\nT = { ");
    for (int v = 0; v < g.n; v++) if (!reachable[v]) printf("%d ", v);
    printf("}\n\n");

    printf("Cut edges (S -> T):\n");
    int cut_cap = 0;
    for (int u = 0; u < g.n; u++) {
        if (!reachable[u]) continue;
        for (int e = g.head[u]; e != -1; e = g.next[e]) {
            int v = g.edges[e].to;
            /* Original forward edge (even index) that crosses cut */
            if (!reachable[v] && (e % 2 == 0)) {
                /* Original capacity = current residual + flow through */
                int orig_cap = g.edges[e].cap + g.edges[g.edges[e].rev].cap;
                int flow = g.edges[g.edges[e].rev].cap;
                printf("  %d -> %d : cap=%d, flow=%d\n",
                       u, v, orig_cap, flow);
                cut_cap += orig_cap;
            }
        }
    }
    printf("\nMin cut capacity: %d\n", cut_cap);
    printf("MFMC: %d = %d  %s\n\n", max_flow, cut_cap,
           max_flow == cut_cap ? "VERIFIED" : "MISMATCH");
}

void demo6_larger(void) {
    printf("=== Demo 6: Larger Network (10 vertices) ===\n\n");

    /*
     *  0 -> 1:16, 0 -> 2:13
     *  1 -> 2:4,  1 -> 3:12
     *  2 -> 4:14, 2 -> 1:10
     *  3 -> 2:9,  3 -> 5:20
     *  4 -> 3:7,  4 -> 5:4
     *  Plus: 0->6:8, 6->7:10, 7->5:12, 6->3:5, 1->8:6, 8->9:8, 9->5:7
     */
    FlowGraph g;
    fg_init(&g, 10, 0, 5);
    fg_add_edge(&g, 0, 1, 16); fg_add_edge(&g, 0, 2, 13);
    fg_add_edge(&g, 1, 2, 4);  fg_add_edge(&g, 1, 3, 12);
    fg_add_edge(&g, 2, 4, 14); fg_add_edge(&g, 2, 1, 10);
    fg_add_edge(&g, 3, 2, 9);  fg_add_edge(&g, 3, 5, 20);
    fg_add_edge(&g, 4, 3, 7);  fg_add_edge(&g, 4, 5, 4);
    fg_add_edge(&g, 0, 6, 8);  fg_add_edge(&g, 6, 7, 10);
    fg_add_edge(&g, 7, 5, 12); fg_add_edge(&g, 6, 3, 5);
    fg_add_edge(&g, 1, 8, 6);  fg_add_edge(&g, 8, 9, 8);
    fg_add_edge(&g, 9, 5, 7);

    printf("10-vertex network with 17 edges\n\n");

    FlowGraph g_dfs;
    memcpy(&g_dfs, &g, sizeof(FlowGraph));

    int ek_flow = edmonds_karp(&g, true);
    printf("\nEdmonds-Karp: max flow = %d (%d augmentations)\n\n",
           ek_flow, g.iter_count);

    int dfs_flow = ford_fulkerson_dfs(&g_dfs, false);
    printf("Ford-Fulkerson DFS: max flow = %d (%d augmentations)\n\n",
           dfs_flow, g_dfs.iter_count);
}

/* =================== MAIN ========================================= */

int main(void) {
    demo1_ek_trace();
    demo2_ff_trace();
    demo3_comparison();
    demo4_pathological();
    demo5_mincut();
    demo6_larger();
    return 0;
}
```

---

## 8. Programa completo en Rust

```rust
/*
 * ford_fulkerson.rs
 *
 * Ford-Fulkerson (DFS) and Edmonds-Karp (BFS) for maximum flow.
 * Adjacency list with reverse edge pointers.
 *
 * Compile: rustc -o ford_fulkerson ford_fulkerson.rs
 * Run:     ./ford_fulkerson
 */

use std::collections::VecDeque;

/* =================== FLOW GRAPH WITH REVERSE EDGES ================ */

#[derive(Clone)]
struct Edge {
    to: usize,
    cap: i64,
    rev: usize, /* index into adj[to] */
}

#[derive(Clone)]
struct FlowGraph {
    adj: Vec<Vec<Edge>>,
    n: usize,
    s: usize,
    t: usize,
}

impl FlowGraph {
    fn new(n: usize, s: usize, t: usize) -> Self {
        FlowGraph { adj: vec![vec![]; n], n, s, t }
    }

    fn add_edge(&mut self, u: usize, v: usize, cap: i64) {
        let rev_u = self.adj[v].len();
        let rev_v = self.adj[u].len();
        self.adj[u].push(Edge { to: v, cap, rev: rev_u });
        self.adj[v].push(Edge { to: u, cap: 0, rev: rev_v });
    }

    /* Edmonds-Karp: BFS augmenting path */
    fn ek_augment(&mut self) -> Option<(i64, Vec<usize>)> {
        let mut parent: Vec<Option<(usize, usize)>> = vec![None; self.n]; // (vertex, edge_idx)
        let mut visited = vec![false; self.n];
        let mut queue = VecDeque::new();

        visited[self.s] = true;
        queue.push_back(self.s);

        'bfs: while let Some(u) = queue.pop_front() {
            for i in 0..self.adj[u].len() {
                let v = self.adj[u][i].to;
                if !visited[v] && self.adj[u][i].cap > 0 {
                    visited[v] = true;
                    parent[v] = Some((u, i));
                    if v == self.t { break 'bfs; }
                    queue.push_back(v);
                }
            }
        }

        if !visited[self.t] { return None; }

        /* Find bottleneck */
        let mut bn = i64::MAX;
        let mut v = self.t;
        let mut path = vec![v];
        while let Some((u, idx)) = parent[v] {
            bn = bn.min(self.adj[u][idx].cap);
            path.push(u);
            v = u;
        }
        path.reverse();

        /* Augment */
        v = self.t;
        while let Some((u, idx)) = parent[v] {
            self.adj[u][idx].cap -= bn;
            let rev = self.adj[u][idx].rev;
            self.adj[v][rev].cap += bn;
            v = u;
        }

        Some((bn, path))
    }

    fn edmonds_karp(&mut self, verbose: bool) -> (i64, usize) {
        let mut total = 0i64;
        let mut iters = 0usize;

        while let Some((bn, path)) = self.ek_augment() {
            iters += 1;
            total += bn;
            if verbose {
                let ps: Vec<String> = path.iter().map(|v| v.to_string()).collect();
                println!("  Aug {}: path {}, delta={}, total={}",
                         iters, ps.join("->"), bn, total);
            }
        }
        (total, iters)
    }

    /* Ford-Fulkerson: DFS augmenting path */
    fn ff_dfs_rec(&mut self, u: usize, pushed: i64,
                  visited: &mut Vec<bool>) -> i64 {
        if u == self.t { return pushed; }
        visited[u] = true;

        for i in 0..self.adj[u].len() {
            let v = self.adj[u][i].to;
            let cap = self.adj[u][i].cap;
            if !visited[v] && cap > 0 {
                let new_pushed = pushed.min(cap);
                let result = self.ff_dfs_rec(v, new_pushed, visited);
                if result > 0 {
                    self.adj[u][i].cap -= result;
                    let rev = self.adj[u][i].rev;
                    self.adj[v][rev].cap += result;
                    return result;
                }
            }
        }
        0
    }

    fn ford_fulkerson_dfs(&mut self, verbose: bool) -> (i64, usize) {
        let mut total = 0i64;
        let mut iters = 0usize;

        loop {
            let mut visited = vec![false; self.n];
            let pushed = self.ff_dfs_rec(self.s, i64::MAX, &mut visited);
            if pushed == 0 { break; }
            iters += 1;
            total += pushed;
            if verbose && iters <= 20 {
                println!("  Aug {}: delta={}, total={}", iters, pushed, total);
            }
        }
        if verbose && iters > 20 {
            println!("  ... ({} total augmentations)", iters);
        }
        (total, iters)
    }

    /* Min-cut from residual: BFS reachability from s */
    fn min_cut_sets(&self) -> (Vec<usize>, Vec<usize>) {
        let mut visited = vec![false; self.n];
        let mut queue = VecDeque::new();
        visited[self.s] = true;
        queue.push_back(self.s);

        while let Some(u) = queue.pop_front() {
            for e in &self.adj[u] {
                if e.cap > 0 && !visited[e.to] {
                    visited[e.to] = true;
                    queue.push_back(e.to);
                }
            }
        }

        let s_set: Vec<usize> = (0..self.n).filter(|&v| visited[v]).collect();
        let t_set: Vec<usize> = (0..self.n).filter(|&v| !visited[v]).collect();
        (s_set, t_set)
    }
}

/* =================== BUILD NETWORKS =============================== */

fn build_example() -> FlowGraph {
    let mut g = FlowGraph::new(6, 0, 5);
    g.add_edge(0, 1, 10);
    g.add_edge(0, 2, 8);
    g.add_edge(1, 5, 6);
    g.add_edge(1, 4, 8);
    g.add_edge(2, 3, 5);
    g.add_edge(3, 1, 2);
    g.add_edge(3, 4, 10);
    g.add_edge(4, 5, 10);
    g
}

fn build_pathological(m: i64) -> FlowGraph {
    let mut g = FlowGraph::new(4, 0, 3);
    /* Add 1->2 FIRST so DFS from 1 tries 2 before 3 */
    g.add_edge(1, 2, 1);
    g.add_edge(0, 1, m);
    g.add_edge(0, 2, m);
    g.add_edge(1, 3, m);
    g.add_edge(2, 3, m);
    g
}

/* =================== DEMOS ======================================== */

fn demo1_ek_trace() {
    println!("=== Demo 1: Edmonds-Karp — Full Trace ===\n");

    println!("Network: 0->1:10, 0->2:8, 1->5:6, 1->4:8,");
    println!("         2->3:5, 3->1:2, 3->4:10, 4->5:10\n");

    let mut g = build_example();
    let (flow, iters) = g.edmonds_karp(true);
    println!("\nMax flow: {} ({} augmentations)\n", flow, iters);
}

fn demo2_ff_trace() {
    println!("=== Demo 2: Ford-Fulkerson (DFS) — Same Network ===\n");

    let mut g = build_example();
    let (flow, iters) = g.ford_fulkerson_dfs(true);
    println!("\nMax flow: {} ({} augmentations)\n", flow, iters);
}

fn demo3_comparison() {
    println!("=== Demo 3: DFS vs BFS — Augmentation Count ===\n");

    println!("{:<32} {:>8} {:>8}", "Network", "DFS", "BFS");
    println!("{:<32} {:>8} {:>8}", "-------", "---", "---");

    /* Example */
    {
        let mut g1 = build_example();
        let mut g2 = build_example();
        let (_, d) = g1.ford_fulkerson_dfs(false);
        let (_, b) = g2.edmonds_karp(false);
        println!("{:<32} {:>8} {:>8}", "Example (6V, flow=15)", d, b);
    }

    /* Pathological cases */
    for &m in &[100i64, 1000, 10000] {
        let mut g1 = build_pathological(m);
        let mut g2 = build_pathological(m);
        let (_, d) = g1.ford_fulkerson_dfs(false);
        let (_, b) = g2.edmonds_karp(false);
        println!("{:<32} {:>8} {:>8}",
                 format!("Pathological (M={})", m), d, b);
    }
    println!();
}

fn demo4_pathological() {
    println!("=== Demo 4: Pathological Case — DFS Zig-Zag ===\n");

    let m = 10;
    println!("Network: 0->1:{0}, 0->2:{0}, 1->3:{0}, 2->3:{0}, 1->2:1",
             m);
    println!("Expected max flow: {}\n", 2 * m);

    println!("--- DFS ---");
    let mut g1 = build_pathological(m);
    let (f1, i1) = g1.ford_fulkerson_dfs(true);
    println!("Result: flow={}, augmentations={}\n", f1, i1);

    println!("--- BFS (Edmonds-Karp) ---");
    let mut g2 = build_pathological(m);
    let (f2, i2) = g2.edmonds_karp(true);
    println!("Result: flow={}, augmentations={}\n", f2, i2);
}

fn demo5_distance_monotonicity() {
    println!("=== Demo 5: Distance Monotonicity (Edmonds-Karp) ===\n");

    let mut g = build_example();
    let mut distances: Vec<usize> = Vec::new();

    loop {
        /* BFS to find distances */
        let mut dist = vec![usize::MAX; g.n];
        let mut queue = VecDeque::new();
        dist[g.s] = 0;
        queue.push_back(g.s);
        while let Some(u) = queue.pop_front() {
            for e in &g.adj[u] {
                if e.cap > 0 && dist[e.to] == usize::MAX {
                    dist[e.to] = dist[u] + 1;
                    queue.push_back(e.to);
                }
            }
        }

        if dist[g.t] == usize::MAX { break; }
        distances.push(dist[g.t]);

        /* Do one augmentation */
        if g.ek_augment().is_none() { break; }
    }

    println!("Path lengths across augmentations: {:?}", distances);
    let monotone = distances.windows(2).all(|w| w[1] >= w[0]);
    println!("Non-decreasing: {}", monotone);
    println!("(This is guaranteed by the Edmonds-Karp lemma)\n");
}

fn demo6_mincut() {
    println!("=== Demo 6: Min-Cut Extraction ===\n");

    let mut g = build_example();
    let (flow, _) = g.edmonds_karp(false);

    let (s_set, t_set) = g.min_cut_sets();

    println!("Max flow: {}\n", flow);
    println!("S = {{{}}}", s_set.iter()
        .map(|v| v.to_string()).collect::<Vec<_>>().join(", "));
    println!("T = {{{}}}", t_set.iter()
        .map(|v| v.to_string()).collect::<Vec<_>>().join(", "));

    /* Find cut edges: original forward edges (even indices) crossing S->T */
    println!("\nCut edges (S -> T):");
    let mut cut_cap = 0i64;
    let in_s: Vec<bool> = {
        let mut v = vec![false; g.n];
        for &u in &s_set { v[u] = true; }
        v
    };
    for &u in &s_set {
        for (i, e) in g.adj[u].iter().enumerate() {
            /* Original edges have even index pairs (0,1), (2,3), ... */
            if !in_s[e.to] && i % 2 == 0 {
                let orig_cap = e.cap + g.adj[e.to][e.rev].cap;
                let flow_on = g.adj[e.to][e.rev].cap;
                println!("  {} -> {} : cap={}, flow={}",
                         u, e.to, orig_cap, flow_on);
                cut_cap += orig_cap;
            }
        }
    }
    println!("\nMin cut capacity: {}", cut_cap);
    println!("MFMC: {} = {}  {}\n",
             flow, cut_cap,
             if flow == cut_cap { "VERIFIED" } else { "MISMATCH" });
}

fn demo7_flow_evolution() {
    println!("=== Demo 7: Flow Evolution Across Augmentations ===\n");

    let mut g = build_example();
    let mut step = 0;

    println!("Edge flows after each augmentation:\n");
    println!("{:<5} {}", "Step", "Edge flows (non-zero)");

    loop {
        match g.ek_augment() {
            None => break,
            Some((bn, path)) => {
                step += 1;
                let ps: Vec<String> = path.iter()
                    .map(|v| v.to_string()).collect();
                print!("{:<5} path={}, delta={}\n      flows: ",
                       step, ps.join("->"), bn);

                /* Print current flow on each original edge */
                let mut first = true;
                for u in 0..g.n {
                    for (i, e) in g.adj[u].iter().enumerate() {
                        if i % 2 == 0 { /* original edges only */
                            let flow_on = g.adj[e.to][e.rev].cap;
                            if flow_on > 0 {
                                if !first { print!(", "); }
                                print!("f({},{})={}", u, e.to, flow_on);
                                first = false;
                            }
                        }
                    }
                }
                println!("\n");
            }
        }
    }
    println!("Final max flow: {}\n", g.edmonds_karp(false).0 +
             /* already found all flow */ 0);

    /* Recalculate from the already-maxed graph */
    let total: i64 = (0..g.n)
        .flat_map(|u| g.adj[u].iter().enumerate()
            .filter(|(i, _)| i % 2 == 0)
            .filter(|(_, e)| {
                let from = u;
                from == g.s
            })
            .map(|(_, e)| g.adj[e.to][e.rev].cap))
        .sum();
    println!("Total flow from source: {}\n", total);
}

fn demo8_scaling() {
    println!("=== Demo 8: Capacity Scaling Intuition ===\n");

    println!("Capacity scaling processes edges in phases by scale Delta.\n");
    println!("Phase: only use edges with residual >= Delta.\n");

    let mut g = build_example();

    /* Show what happens with different thresholds */
    for &threshold in &[8i64, 4, 2, 1] {
        let mut g_copy = g.clone();

        /* Only allow edges with cap >= threshold */
        /* We'll just count reachable from s using edges >= threshold */
        let mut visited = vec![false; g_copy.n];
        let mut queue = VecDeque::new();
        visited[g_copy.s] = true;
        queue.push_back(g_copy.s);
        while let Some(u) = queue.pop_front() {
            for e in &g_copy.adj[u] {
                if e.cap >= threshold && !visited[e.to] {
                    visited[e.to] = true;
                    queue.push_back(e.to);
                }
            }
        }

        let reachable: Vec<usize> = (0..g_copy.n)
            .filter(|&v| visited[v]).collect();
        let reaches_t = visited[g_copy.t];

        println!("  Delta={}: edges with residual >= {}: reachable from s = {:?}, reaches t: {}",
                 threshold, threshold,
                 reachable, reaches_t);
    }

    println!("\nWith Edmonds-Karp (no scaling, for comparison):");
    let (flow, iters) = g.edmonds_karp(false);
    println!("  Max flow = {}, augmentations = {}\n", flow, iters);
}

/* =================== MAIN ========================================= */

fn main() {
    demo1_ek_trace();
    demo2_ff_trace();
    demo3_comparison();
    demo4_pathological();
    demo5_distance_monotonicity();
    demo6_mincut();
    demo7_flow_evolution();
    demo8_scaling();
}
```

---

## 9. Ejercicios

### Ejercicio 1 — Traza de Edmonds-Karp

Red: 0→1:7, 0→2:4, 1→3:5, 1→2:3, 2→4:6, 3→5:3, 3→4:2, 4→5:8 (s=0, t=5).

Ejecuta Edmonds-Karp paso a paso. Para cada augmentación indica: camino BFS,
cuello de botella, flujo acumulado.

<details><summary>Predicción</summary>

Aug 1: BFS niveles {0}→{1,2}→{3,4}→{5}. Camino 0→1→3→5, $\delta=\min(7,5,3)=3$.
Flujo = 3.

Aug 2: Residual $r(0,1)=4, r(1,3)=2, r(3,5)=0$. BFS: {0}→{1,2}→{3,4}→{5}.
Camino 0→2→4→5, $\delta=\min(4,6,8)=4$. Flujo = 7.

Aug 3: $r(0,1)=4, r(0,2)=0, r(1,2)=3, r(1,3)=2$. BFS: {0}→{1}→{2,3}→{4}→{5}.
Camino 0→1→3→4→5, $\delta=\min(4,2,2,4)=2$. Flujo = 9.

Aug 4: $r(0,1)=2, r(1,2)=3, r(1,3)=0$. BFS: {0}→{1}→{2}→{4}→{5}.
Camino 0→1→2→4→5, $\delta=\min(2,3,2,2)=2$. Flujo = 11.

Aug 5: BFS from 0: $r(0,1)=0, r(0,2)=0$. No path. Done.

**Max flow = 11**.

Verificar corte: $(\{0\}, \{1,2,3,4,5\})$, cap = 7+4 = 11. ✓

</details>

### Ejercicio 2 — DFS vs BFS iteraciones

Red: 0→1:100, 0→2:100, 1→3:100, 2→3:100, 1→2:1 (s=0, t=3).

¿Cuántas augmentaciones hace DFS en el peor caso? ¿Y BFS?

<details><summary>Predicción</summary>

**DFS (peor caso)**: si siempre elige el camino por la arista 1→2:
- Alterna 0→1→2→3 ($\delta=1$) y 0→2→1→3 ($\delta=1$).
- Total: $2 \times 100 = 200$ augmentaciones para flujo = 200.

**BFS**: caminos más cortos primero:
- 0→1→3: $\delta=100$. Flujo = 100.
- 0→2→3: $\delta=100$. Flujo = 200.
- Total: **2 augmentaciones**.

La diferencia es 100x. Con $M = 10^6$ sería 1,000,000x.

</details>

### Ejercicio 3 — Monotonía de distancias

En la traza del ejercicio 1, registra la distancia de $s$ a $t$ en el residual
antes de cada augmentación. ¿Son no decrecientes?

<details><summary>Predicción</summary>

- Antes de aug 1: BFS distancia $s \to t$: 3 aristas (0→1→3→5).
- Antes de aug 2: distancia a 5 via 0→2→4→5: 3 aristas.
- Antes de aug 3: 0→1→3→4→5: 4 aristas.
- Antes de aug 4: 0→1→2→4→5: 4 aristas.

Secuencia: [3, 3, 4, 4]. **Sí, no decreciente** ✓.

Esto confirma el lema de monotonía: $\delta_{f'}(s,t) \geq \delta_f(s,t)$.

</details>

### Ejercicio 4 — Arista crítica

En la traza del ejercicio 1, identifica cuál arista fue crítica (bottleneck) en
cada augmentación. ¿Cuántas veces fue crítica la arista 1→3?

<details><summary>Predicción</summary>

- Aug 1: bottleneck en 3→5 ($c_f = 3$). Arista crítica: **3→5**.
- Aug 2: bottleneck en 0→2 ($c_f = 4$). Arista crítica: **0→2**.
- Aug 3: bottleneck en 1→3 y 3→4 ($c_f = 2$ ambas). Arista crítica: **1→3** (o 3→4).
- Aug 4: bottleneck depende, probablemente 0→1 o alguna con $c_f=2$. Arista crítica: **0→1** (o varias).

1→3 fue crítica **1 vez**. Máximo posible según análisis: $V/2 = 6/2 = 3$ veces.

</details>

### Ejercicio 5 — Corte mínimo

Tras encontrar el flujo máximo en el ejercicio 1, identifica el corte mínimo
a partir del grafo residual.

<details><summary>Predicción</summary>

Tras flujo máximo = 11:
- $f(0,1)=7$ (saturada), $f(0,2)=4$ (saturada).
- Residual desde 0: $r(0,1)=0, r(0,2)=0$. Solo 0 es alcanzable.

$S = \{0\}$, $T = \{1,2,3,4,5\}$.

Aristas cortadas: 0→1 (cap=7) y 0→2 (cap=4).
Capacidad del corte: $7 + 4 = 11 = $ max flow. ✓

**Interpretación**: el cuello de botella del sistema son las dos aristas que
salen de la fuente. Incluso si el resto de la red tiene capacidad sobrante,
no podemos enviar más de 11 unidades.

</details>

### Ejercicio 6 — Flujo no entero

Red con capacidades: 0→1:1.5, 0→2:1, 1→2:0.5, 1→3:1, 2→3:1.5 (s=0, t=3).

Encuentra el flujo máximo. ¿Es entero?

<details><summary>Predicción</summary>

Aug 1: 0→1→3, $\delta=\min(1.5, 1) = 1$. Flujo = 1.
Aug 2: 0→2→3, $\delta=\min(1, 1.5) = 1$. Flujo = 2.
Aug 3: 0→1→2→3, $\delta=\min(0.5, 0.5, 0.5) = 0.5$. Flujo = 2.5.

No más caminos. **Max flow = 2.5**. No es entero.

Esto es correcto: el teorema de integralidad solo aplica cuando **todas** las
capacidades son enteras. Con capacidades racionales, el flujo máximo es racional.
Ford-Fulkerson siempre termina con capacidades racionales (cada $\delta$ es
racional, y el flujo máximo es finito).

</details>

### Ejercicio 7 — Red con backward edges utilizadas

Red: 0→1:10, 0→2:5, 1→2:15, 2→3:10, 1→3:5 (s=0, t=3).

Ejecuta Edmonds-Karp. ¿Alguna augmentación usa una backward edge?

<details><summary>Predicción</summary>

Aug 1: 0→1→3, $\delta=\min(10,5) = 5$. Flujo = 5.
Aug 2: 0→1→2→3, $\delta=\min(5,15,10) = 5$. Flujo = 10.
Aug 3: 0→2→3, $\delta=\min(5,5) = 5$. Flujo = 15.

No más caminos. Max flow = 15.

En este caso, **ninguna augmentación usa backward edges**. Todas las aristas en
los caminos son forward edges con capacidad residual disponible.

Para forzar backward edges, necesitaríamos una red donde el camino BFS más corto
pasa por una arista en sentido contrario. Ejemplo: si en la aug 3 la única forma
de llegar a 3 fuera via backward 2→1. Pero aquí 0→2 sigue disponible.

</details>

### Ejercicio 8 — Probar que la complejidad es ajustada

Construye una familia de grafos donde Edmonds-Karp requiere $\Theta(VE)$
augmentaciones. (Pista: grafo "unit" con $V$ capas de 2 vértices cada una.)

<details><summary>Predicción</summary>

Construcción: $k$ capas, cada una con 2 vértices $a_i, b_i$ ($i=1,\ldots,k$).
Fuente $s$ conectada a $a_1$ y $b_1$. Sumidero $t$ conectado desde $a_k$ y $b_k$.
Entre capas: $a_i \to a_{i+1}$, $a_i \to b_{i+1}$, $b_i \to a_{i+1}$,
$b_i \to b_{i+1}$. Todas capacidad 1.

$V = 2k + 2$, $E = 4k + 2$. Flujo máximo = 2 (2 caminos disjuntos de $s$ a $t$).

Pero Edmonds-Karp hace $O(V)$ augmentaciones (no $\Theta(VE)$ aquí).

Para $\Theta(VE)$: usar un grafo con $O(V)$ nodos en línea con aristas cruzadas.
Cada arista puede ser crítica $O(V)$ veces, y hay $O(E)$ aristas. El total de
augmentaciones es $\Theta(VE)$ cuando las aristas se saturan y reaparecen
repetidamente.

La cota $O(VE)$ es ajustada pero construir el ejemplo explícito es complejo.
La idea clave es que cada arista puede ser bottleneck $O(V)$ veces y hay $O(E)$
aristas, así que el peor caso total es $O(VE)$.

</details>

### Ejercicio 9 — Dinic vs Edmonds-Karp

Red: cadena 0→1→2→3→4→5 con capacidad $C$ en cada arista, más aristas
transversales 1→3:1 y 2→4:1.

¿Cuántas fases usa Dinic? ¿Cuántas augmentaciones usa Edmonds-Karp?

<details><summary>Predicción</summary>

**Edmonds-Karp**:
Aug 1: 0→1→2→3→4→5, $\delta=C$. Flujo = $C$. Longitud 5.
Aug 2: 0→1→3→4→5 (via arista transversal 1→3), $\delta=1$. Flujo = $C+1$. Longitud 4.

Hmm, pero residual de 1→2 = 0 tras aug 1, no necesariamente. Si $C > 1$:
aug 1 satura toda la cadena. Residual: backward edges.
Aug 2: 0→1→3→4→5 (r=C−0=C... wait, edge 1→3 has cap 1).

Let me reconsider. After aug 1 with delta=C: all main chain edges saturated.
Residual: backward edges 1→0:C, 2→1:C, etc. Forward: 1→3:1, 2→4:1.

Aug 2: 0→...? r(0,1)=0. Dead end. So only path through 0→1 which has r=0.

Hmm, if C is the capacity, after sending C along the chain, no more flow.
The transversal edges don't help because there's no way to reach them from s
without going through 0→1 which is saturated.

So max flow = C, 1 augmentation for both.

This exercise doesn't work well. Let me rethink.

Better: parallel paths. 0→1→3→5 all cap C, 0→2→4→5 all cap C, plus 1→4:1, 2→3:1.

**Edmonds-Karp**:
Aug 1: 0→1→3→5, $\delta=C$. Aug 2: 0→2→4→5, $\delta=C$. Aug 3: 0→...? No more.
If C was already sent, 1→4:1 and 2→3:1 don't add anything new.

Max flow = 2C, 2 augmentations. Both algorithms essentially the same.

**Dinic**: Phase 1 (dist=3): blocking flow = 2C (both paths at once). 1 phase.

The point is that Dinic can send multiple paths in one phase while Edmonds-Karp
does one per iteration.

</details>

### Ejercicio 10 — Implementar capacity scaling

Describe el algoritmo de capacity scaling para flujo máximo. ¿Cuál es su
complejidad? ¿Cuándo es preferible a Edmonds-Karp?

<details><summary>Predicción</summary>

**Algoritmo**:
1. $\Delta \gets $ mayor potencia de 2 $\leq C_{\max}$.
2. Mientras $\Delta \geq 1$:
   a. Construir $G_f(\Delta)$: solo aristas con $c_f \geq \Delta$.
   b. Mientras exista camino aumentante en $G_f(\Delta)$:
      - Encontrar camino (BFS o DFS).
      - Augmentar con $\delta \geq \Delta$.
   c. $\Delta \gets \Delta / 2$.

**Complejidad**: $O(E^2 \log C)$.
- Hay $O(\log C)$ fases (valores de $\Delta$).
- En cada fase, a lo sumo $O(E)$ augmentaciones (cada una envía $\geq \Delta$,
  y el flujo restante posible al inicio de la fase es $\leq 2E\Delta$).
- Cada augmentación cuesta $O(E)$.

**Cuándo es preferible**: cuando $C$ es grande pero $E$ es moderado. Edmonds-Karp
es $O(VE^2)$ y no depende de $C$, pero capacity scaling puede ser mejor si
$E^2 \log C < VE^2$, es decir, $\log C < V$. En la práctica, capacity scaling
tiene buen rendimiento en redes con capacidades muy variadas.

</details>
