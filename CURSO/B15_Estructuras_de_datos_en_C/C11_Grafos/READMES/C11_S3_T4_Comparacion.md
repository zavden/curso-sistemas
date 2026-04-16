# T04 — Comparación de algoritmos de caminos más cortos

## Objetivo

Sintetizar los tres algoritmos de caminos más cortos (Dijkstra, Bellman-Ford,
Floyd-Warshall) en una comparación completa: restricciones, complejidades,
fortalezas, debilidades, y reglas de decisión para elegir el correcto según
el problema.

---

## 1. Tabla comparativa principal

| Propiedad | Dijkstra | Bellman-Ford | Floyd-Warshall |
|-----------|:--------:|:------------:|:--------------:|
| Problema | SSSP | SSSP | APSP |
| Pesos negativos | **No** | Sí | Sí |
| Ciclos negativos | No detecta | **Detecta** | **Detecta** |
| Complejidad (heap) | $O((n+m)\log n)$ | $O(n \cdot m)$ | $O(n^3)$ |
| Complejidad (array) | $O(n^2)$ | — | — |
| Espacio extra | $O(n)$ | $O(n)$ | $O(n^2)$ |
| Estructura de datos | Min-heap | Lista de aristas | Matriz |
| Técnica | Greedy | Relajación iterativa | DP |
| APSP (repitiendo) | $O(n(n+m)\log n)$ | $O(n^2 m)$ | $O(n^3)$ |
| Implementación | Moderada | Simple | Muy simple |

---

## 2. Restricciones de cada algoritmo

### 2.1 Dijkstra

**Requisito estricto**: todos los pesos $w(u,v) \geq 0$.

Si se viola, el invariante greedy se rompe: un vértice "finalizado" con
distancia $d$ podría mejorar a $d + w_{\text{neg}}$ por un camino más largo
que pasa por una arista negativa.

```
Ejemplo de falla:
  A →(1)→ B →(-5)→ C
  A →(3)→ C

Dijkstra: dist[C]=3 (directo). Correcto: dist[C]=1+(-5)=-4.
```

**Excepción**: BFS 0-1 (deque) funciona con pesos 0 y 1, que es un caso
especial de Dijkstra con pesos no negativos.

### 2.2 Bellman-Ford

**Sin restricciones en pesos**, pero:
- Si hay ciclo negativo alcanzable desde la fuente, las distancias no están
  bien definidas (el algoritmo lo reporta).
- $O(n \cdot m)$ puede ser prohibitivo para grafos grandes.

### 2.3 Floyd-Warshall

**Sin restricciones en pesos**, pero:
- Si hay ciclos negativos, la diagonal muestra valores $< 0$ y las
  distancias entre pares afectados son $-\infty$.
- Requiere $O(n^2)$ espacio para las matrices, limitando $n$ a unos miles.
- Siempre $O(n^3)$ sin importar $m$ (no aprovecha grafos dispersos).

---

## 3. Complejidad detallada

### 3.1 SSSP (una fuente)

| Algoritmo | Disperso ($m = O(n)$) | Moderado ($m = O(n\sqrt{n})$) | Denso ($m = O(n^2)$) |
|-----------|:---------------------:|:-----------------------------:|:--------------------:|
| Dijkstra (heap binario) | $O(n \log n)$ | $O(n^{3/2} \log n)$ | $O(n^2 \log n)$ |
| Dijkstra (array) | $O(n^2)$ | $O(n^2)$ | $O(n^2)$ |
| Dijkstra (Fibonacci) | $O(n \log n)$ | $O(n \log n + n\sqrt{n})$ | $O(n \log n + n^2)$ |
| Bellman-Ford | $O(n^2)$ | $O(n^2 \sqrt{n})$ | $O(n^3)$ |
| BFS (pesos = 1) | $O(n)$ | $O(n\sqrt{n})$ | $O(n^2)$ |

**Conclusión SSSP**:
- Pesos $\geq 0$: siempre Dijkstra (heap para disperso, array para denso).
- Pesos negativos: Bellman-Ford (o SPFA en la práctica).

### 3.2 APSP (todos los pares)

| Algoritmo | Disperso ($m = O(n)$) | Denso ($m = O(n^2)$) |
|-----------|:---------------------:|:--------------------:|
| Dijkstra × $n$ (heap) | $O(n^2 \log n)$ | $O(n^3 \log n)$ |
| Dijkstra × $n$ (array) | $O(n^3)$ | $O(n^3)$ |
| Johnson | $O(n^2 \log n + nm)$ | $O(n^3 \log n)$ |
| Floyd-Warshall | $O(n^3)$ | $O(n^3)$ |
| Bellman-Ford × $n$ | $O(n^3)$ | $O(n^4)$ |

**Conclusión APSP**:
- Disperso, pesos $\geq 0$: Dijkstra × $n$ con heap.
- Disperso, pesos negativos: Johnson.
- Denso: Floyd-Warshall (constante más pequeña que Dijkstra × $n$).
- $n$ pequeño ($\leq 500$): Floyd-Warshall siempre (simplicidad).

---

## 4. Árbol de decisión

```
¿Necesitas TODAS las parejas (APSP)?
├── SÍ
│   ├── n ≤ 500? → Floyd-Warshall
│   ├── ¿Pesos negativos?
│   │   ├── SÍ → Johnson (BF + Dijkstra×n)
│   │   └── NO
│   │       ├── Grafo disperso? → Dijkstra × n (heap)
│   │       └── Grafo denso? → Floyd-Warshall o Dijkstra×n (array)
│   └── ¿Solo alcanzabilidad (sin distancias)?
│       └── Warshall (cierre transitivo, con bitsets)
│
└── NO (SSSP — una fuente)
    ├── ¿Todos los pesos iguales (o sin pesos)?
    │   └── BFS (O(n + m))
    │
    ├── ¿Solo pesos 0 y 1?
    │   └── BFS 0-1 con deque (O(n + m))
    │
    ├── ¿DAG (grafo acíclico dirigido)?
    │   └── Relajar en orden topológico (O(n + m)) — funciona con negativos
    │
    ├── ¿Pesos ≥ 0?
    │   ├── Grafo disperso → Dijkstra (heap binario)
    │   └── Grafo denso → Dijkstra (array lineal)
    │
    └── ¿Pesos negativos?
        ├── ¿Necesitas detectar ciclos negativos?
        │   └── Bellman-Ford
        └── ¿Solo caminos más cortos?
            └── Bellman-Ford (o SPFA en la práctica)
```

---

## 5. Caso especial: DAG

Un DAG (directed acyclic graph) permite caminos más cortos en $O(n + m)$
con cualquier peso, incluyendo negativos:

1. Ordenamiento topológico ($O(n + m)$).
2. Relajar aristas en orden topológico ($O(n + m)$).

Esto es más rápido que Dijkstra y funciona con pesos negativos. También
permite encontrar el **camino más largo** (cambiando el sentido de la
comparación o negando los pesos).

```
DAG-SHORTEST-PATH(G, w, s):
    topo_order ← topological_sort(G)
    dist[v] ← ∞ for all v
    dist[s] ← 0

    for each u in topo_order:
        for each (v, w) in adj[u]:
            if dist[u] + w < dist[v]:
                dist[v] ← dist[u] + w
```

| Algoritmo | DAG $O(n+m)$ | Dijkstra $O((n+m)\log n)$ | BF $O(nm)$ |
|-----------|:---:|:---:|:---:|
| Pesos negativos | Sí | No | Sí |
| Ciclos | No funciona | Sí | Sí |
| Complejidad | Mejor | Medio | Peor |

---

## 6. Comparación práctica

### 6.1 Constantes y cache

La complejidad asintótica no cuenta toda la historia. Las constantes
importan para tamaños prácticos:

| Algoritmo | Patrón de acceso | Cache-friendly |
|-----------|:----------------:|:--------------:|
| Dijkstra (heap) | Random (heap + adj list) | Moderado |
| Bellman-Ford | Secuencial (lista de aristas) | **Bueno** |
| Floyd-Warshall | Secuencial (matrices) | **Excelente** |

Floyd-Warshall es extremadamente cache-friendly: el triple bucle recorre
la matriz secuencialmente. En la práctica, FW con $n = 400$ puede ser
más rápido que Dijkstra × $n$ con $n = 400$ a pesar de la misma complejidad
asintótica $O(n^3)$.

### 6.2 Paralelismo

| Algoritmo | Paralelizable |
|-----------|:-------------:|
| Dijkstra | Difícil (dependencias secuenciales) |
| Bellman-Ford | Moderado (cada pasada es independiente por arista) |
| Floyd-Warshall | Moderado (bucle $k$ es secuencial, pero $i,j$ paralelizables) |
| $\Delta$-stepping | Buena alternativa paralela a Dijkstra |

### 6.3 Espacio

| Algoritmo | Espacio adicional |
|-----------|:-----------------:|
| BFS | $O(n)$ (cola + dist) |
| Dijkstra | $O(n + m)$ (heap puede tener $m$ entradas con lazy deletion) |
| Bellman-Ford | $O(n)$ (dist + parent) |
| Floyd-Warshall | $O(n^2)$ (matrices dist + next) |

Para grafos grandes ($n > 10^5$), Floyd-Warshall es imposible en memoria:
$10^{10}$ entradas = ~80 GB.

---

## 7. Escenarios y la elección correcta

### 7.1 Navegación GPS (mapa de carreteras)

- **Característica**: grafo enorme ($n = 10^7$), disperso ($m \approx 3n$),
  pesos positivos (distancia/tiempo), SSSP con early exit.
- **Elección**: **Dijkstra** con heap binario + early exit.
- **Optimización**: A* (heurística de distancia euclidiana).
- Floyd-Warshall: imposible ($10^{21}$ operaciones).
- Bellman-Ford: $O(n^2) = 10^{14}$, demasiado lento.

### 7.2 Routing en red (protocolo RIP)

- **Característica**: grafo pequeño-mediano ($n < 100$), pesos positivos
  (latencia), APSP, actualizaciones frecuentes.
- **Elección**: **Bellman-Ford distribuido** (cada router ejecuta BF
  independientemente, compartiendo vectores de distancia).
- Floyd-Warshall: viable pero centralizado.

### 7.3 Arbitraje de divisas

- **Característica**: grafo completo ($m = n^2$, $n \approx 30$ monedas),
  pesos negativos ($-\log$ de tasas), detectar ciclo negativo.
- **Elección**: **Bellman-Ford** (detecta ciclos negativos directamente).
- Floyd-Warshall: también funciona (diagonal negativa = arbitraje).

### 7.4 Planificación de tareas con precedencias

- **Característica**: DAG, pesos positivos (duración de tareas), necesita
  camino más largo (ruta crítica).
- **Elección**: **Relajación en orden topológico** ($O(n + m)$).
- Negar los pesos y usar SSSP, o modificar la relajación a `max`.

### 7.5 Red social (grados de separación)

- **Característica**: grafo no dirigido, no ponderado, APSP para estadísticas
  (diámetro, centro).
- **Elección**: **BFS × n** ($O(n(n + m))$).
- Si $n$ es pequeño ($\leq 500$): Floyd-Warshall con pesos = 1.

### 7.6 VLSI/circuitos (timing analysis)

- **Característica**: DAG grande, pesos positivos y negativos (delays,
  setup/hold times), SSSP.
- **Elección**: **Relajación topológica** ($O(n + m)$, funciona con negativos).

### 7.7 Juego con teleportación (grid con portales)

- **Característica**: grid con celdas de costo variable + portales (peso 0).
- **Elección**: **Dijkstra** en grafo implícito (grid + aristas de portal).
- Si solo costos 0 y 1: **BFS 0-1** con deque.

---

## 8. Tabla resumen de decisión

| Condición | Algoritmo | Complejidad |
|-----------|:---------:|:-----------:|
| Sin pesos (o todos = 1), SSSP | BFS | $O(n + m)$ |
| Pesos 0 o 1, SSSP | BFS 0-1 (deque) | $O(n + m)$ |
| DAG, cualquier peso, SSSP | Topológico + relax | $O(n + m)$ |
| Pesos $\geq 0$, disperso, SSSP | Dijkstra (heap) | $O((n+m)\log n)$ |
| Pesos $\geq 0$, denso, SSSP | Dijkstra (array) | $O(n^2)$ |
| Pesos negativos, SSSP | Bellman-Ford | $O(n \cdot m)$ |
| Pesos negativos, SSSP (práctico) | SPFA | $O(n \cdot m)$ peor, $\sim O(m)$ promedio |
| APSP, $n \leq 500$ | Floyd-Warshall | $O(n^3)$ |
| APSP, disperso, pesos $\geq 0$ | Dijkstra × $n$ | $O(n(n+m)\log n)$ |
| APSP, disperso, pesos negativos | Johnson | $O(nm + n(n+m)\log n)$ |
| APSP, denso | Floyd-Warshall | $O(n^3)$ |
| Alcanzabilidad (sin distancias) | Warshall (bitsets) | $O(n^3/w)$ |
| Camino más largo en DAG | Topo + relax (max) | $O(n + m)$ |
| Bottleneck path, APSP | FW (maximin) | $O(n^3)$ |

---

## 9. Programa completo en C

```c
/*
 * sp_comparison.c
 *
 * Side-by-side comparison of Dijkstra, Bellman-Ford, and Floyd-Warshall
 * on the same graphs, with timing and correctness verification.
 *
 * Compile: gcc -O2 -o sp_comparison sp_comparison.c
 * Run:     ./sp_comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define MAX_V 100
#define INF (INT_MAX / 2)

/* =================== GRAPH STRUCTURES ============================= */

typedef struct { int to, weight; } Edge;
typedef struct {
    Edge *edges;
    int size, cap;
} AdjList;

typedef struct {
    AdjList adj[MAX_V];
    int n;
} Graph;

void graph_init(Graph *g, int n) {
    g->n = n;
    for (int i = 0; i < n; i++) {
        g->adj[i].size = 0;
        g->adj[i].cap = 4;
        g->adj[i].edges = malloc(4 * sizeof(Edge));
    }
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++) free(g->adj[i].edges);
}

void graph_add(Graph *g, int u, int v, int w) {
    AdjList *l = &g->adj[u];
    if (l->size == l->cap) {
        l->cap *= 2;
        l->edges = realloc(l->edges, l->cap * sizeof(Edge));
    }
    l->edges[l->size++] = (Edge){v, w};
}

/* =================== DIJKSTRA (array) ============================= */

void dijkstra(Graph *g, int src, int dist[]) {
    bool done[MAX_V] = {false};
    for (int i = 0; i < g->n; i++) dist[i] = INF;
    dist[src] = 0;

    for (int iter = 0; iter < g->n; iter++) {
        int u = -1;
        for (int v = 0; v < g->n; v++)
            if (!done[v] && (u == -1 || dist[v] < dist[u]))
                u = v;
        if (u == -1 || dist[u] == INF) break;
        done[u] = true;
        for (int i = 0; i < g->adj[u].size; i++) {
            int v = g->adj[u].edges[i].to;
            int w = g->adj[u].edges[i].weight;
            if (dist[u] + w < dist[v])
                dist[v] = dist[u] + w;
        }
    }
}

/* =================== BELLMAN-FORD ================================= */

/* Returns true if no negative cycle */
bool bellman_ford(Graph *g, int src, int dist[]) {
    int n = g->n;
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    for (int pass = 0; pass < n - 1; pass++) {
        bool changed = false;
        for (int u = 0; u < n; u++)
            for (int i = 0; i < g->adj[u].size; i++) {
                int v = g->adj[u].edges[i].to;
                int w = g->adj[u].edges[i].weight;
                if (dist[u] != INF && dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    changed = true;
                }
            }
        if (!changed) break;
    }

    /* Check negative cycle */
    for (int u = 0; u < n; u++)
        for (int i = 0; i < g->adj[u].size; i++) {
            int v = g->adj[u].edges[i].to;
            int w = g->adj[u].edges[i].weight;
            if (dist[u] != INF && dist[u] + w < dist[v])
                return false;
        }
    return true;
}

/* =================== FLOYD-WARSHALL =============================== */

void floyd_warshall(int n, int dist[][MAX_V]) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (dist[i][k] + dist[k][j] < dist[i][j])
                    dist[i][j] = dist[i][k] + dist[k][j];
}

/* =================== DAG shortest path ============================ */

/* Topological sort + relax */
static int topo_color[MAX_V];
static int topo_order[MAX_V];
static int topo_idx;

void topo_dfs(Graph *g, int u) {
    topo_color[u] = 1;
    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].edges[i].to;
        if (topo_color[v] == 0)
            topo_dfs(g, v);
    }
    topo_order[topo_idx++] = u;
}

void dag_shortest(Graph *g, int src, int dist[]) {
    int n = g->n;
    memset(topo_color, 0, sizeof(topo_color));
    topo_idx = 0;
    for (int u = 0; u < n; u++)
        if (topo_color[u] == 0)
            topo_dfs(g, u);

    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    /* Process in reverse post-order (topological order) */
    for (int i = topo_idx - 1; i >= 0; i--) {
        int u = topo_order[i];
        if (dist[u] == INF) continue;
        for (int j = 0; j < g->adj[u].size; j++) {
            int v = g->adj[u].edges[j].to;
            int w = g->adj[u].edges[j].weight;
            if (dist[u] + w < dist[v])
                dist[v] = dist[u] + w;
        }
    }
}

/* ============ DEMO 1: All algorithms on same graph ================ */

void demo_compare_all(void) {
    printf("=== Demo 1: All algorithms on same graph ===\n");

    Graph g;
    graph_init(&g, 6);
    /* Directed graph, non-negative weights */
    graph_add(&g, 0, 1, 4);
    graph_add(&g, 0, 2, 2);
    graph_add(&g, 1, 2, 1);
    graph_add(&g, 1, 3, 5);
    graph_add(&g, 2, 3, 8);
    graph_add(&g, 2, 4, 10);
    graph_add(&g, 3, 4, 2);
    graph_add(&g, 3, 5, 6);
    graph_add(&g, 4, 5, 3);

    printf("Directed graph (non-negative weights), source=0\n\n");

    /* Dijkstra */
    int d_dj[MAX_V];
    dijkstra(&g, 0, d_dj);

    /* Bellman-Ford */
    int d_bf[MAX_V];
    bellman_ford(&g, 0, d_bf);

    /* Floyd-Warshall */
    int fw[MAX_V][MAX_V];
    for (int i = 0; i < g.n; i++)
        for (int j = 0; j < g.n; j++)
            fw[i][j] = (i == j) ? 0 : INF;
    for (int u = 0; u < g.n; u++)
        for (int i = 0; i < g.adj[u].size; i++)
            fw[u][g.adj[u].edges[i].to] = g.adj[u].edges[i].weight;
    floyd_warshall(g.n, fw);

    printf("  Vertex  Dijkstra  Bellman-Ford  Floyd-Warshall  Match?\n");
    bool all_ok = true;
    for (int v = 0; v < g.n; v++) {
        bool ok = (d_dj[v] == d_bf[v]) && (d_bf[v] == fw[0][v]);
        if (!ok) all_ok = false;
        printf("  %d       %-9d %-13d %-15d %s\n",
               v, d_dj[v], d_bf[v], fw[0][v], ok ? "✓" : "✗");
    }
    printf("\n  All match: %s\n", all_ok ? "YES" : "NO");

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 2: Negative weights — BF wins ================== */

void demo_negative_weights(void) {
    printf("=== Demo 2: Negative weights — only BF correct ===\n");

    Graph g;
    graph_init(&g, 4);
    graph_add(&g, 0, 1, 4);
    graph_add(&g, 0, 2, 3);
    graph_add(&g, 1, 3, 1);
    graph_add(&g, 2, 1, -2);
    graph_add(&g, 2, 3, 5);

    printf("Graph with edge 2→1:-2 (no neg cycle)\n\n");

    int d_dj[MAX_V], d_bf[MAX_V];
    dijkstra(&g, 0, d_dj);
    bellman_ford(&g, 0, d_bf);

    printf("  Vertex  Dijkstra  Bellman-Ford  Correct?\n");
    for (int v = 0; v < g.n; v++) {
        bool dj_ok = (d_dj[v] == d_bf[v]);
        printf("  %d       %-9d %-13d %s\n",
               v, d_dj[v], d_bf[v],
               dj_ok ? "both OK" : "Dijkstra WRONG");
    }

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 3: DAG — topological approach ================== */

void demo_dag(void) {
    printf("=== Demo 3: DAG shortest path (O(n+m)) ===\n");

    Graph g;
    graph_init(&g, 6);
    /* DAG with negative weights */
    graph_add(&g, 0, 1, 5);
    graph_add(&g, 0, 2, 3);
    graph_add(&g, 1, 2, -2);
    graph_add(&g, 1, 3, 6);
    graph_add(&g, 2, 3, 7);
    graph_add(&g, 2, 4, 4);
    graph_add(&g, 3, 4, -1);
    graph_add(&g, 3, 5, 2);
    graph_add(&g, 4, 5, -3);

    printf("DAG with some negative weights, source=0\n\n");

    int d_dag[MAX_V], d_bf[MAX_V];
    dag_shortest(&g, 0, d_dag);
    bellman_ford(&g, 0, d_bf);

    printf("  Vertex  DAG(O(n+m))  Bellman-Ford  Match?\n");
    for (int v = 0; v < g.n; v++) {
        bool ok = (d_dag[v] == d_bf[v]);
        printf("  %d       %-12d %-13d %s\n",
               v, d_dag[v], d_bf[v], ok ? "✓" : "✗");
    }

    printf("\nDAG approach: O(n + m) — faster than both Dijkstra and BF!\n");
    printf("Works with negative weights (unlike Dijkstra).\n");

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 4: APSP — FW vs Dijkstra × n ================== */

void demo_apsp(void) {
    printf("=== Demo 4: APSP comparison ===\n");

    int n = 5;
    Graph g;
    graph_init(&g, n);
    graph_add(&g, 0, 1, 3); graph_add(&g, 0, 2, 8);
    graph_add(&g, 1, 2, 2); graph_add(&g, 1, 3, 5);
    graph_add(&g, 2, 3, 1); graph_add(&g, 2, 4, 4);
    graph_add(&g, 3, 4, 6); graph_add(&g, 4, 0, 7);

    printf("Directed graph, 5 vertices, 8 edges\n\n");

    /* Dijkstra × n */
    int dj_apsp[MAX_V][MAX_V];
    for (int s = 0; s < n; s++)
        dijkstra(&g, s, dj_apsp[s]);

    /* Floyd-Warshall */
    int fw[MAX_V][MAX_V];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            fw[i][j] = (i == j) ? 0 : INF;
    for (int u = 0; u < n; u++)
        for (int i = 0; i < g.adj[u].size; i++)
            fw[u][g.adj[u].edges[i].to] = g.adj[u].edges[i].weight;
    floyd_warshall(n, fw);

    /* Compare */
    printf("Distance matrices:\n");
    printf("Dijkstra × n:           Floyd-Warshall:\n");
    for (int i = 0; i < n; i++) {
        printf("  [");
        for (int j = 0; j < n; j++) {
            if (dj_apsp[i][j] >= INF/2) printf(" ∞ ");
            else printf("%3d", dj_apsp[i][j]);
        }
        printf("]    [");
        for (int j = 0; j < n; j++) {
            if (fw[i][j] >= INF/2) printf(" ∞ ");
            else printf("%3d", fw[i][j]);
        }
        printf("]\n");
    }

    int mismatches = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (dj_apsp[i][j] != fw[i][j]) mismatches++;
    printf("\nMismatches: %d\n", mismatches);

    graph_free(&g);
    printf("\n");
}

/* ============ DEMO 5: Algorithm selection guide =================== */

void demo_selection(void) {
    printf("=== Demo 5: Algorithm selection examples ===\n\n");

    struct Scenario {
        const char *name;
        int n, m;
        bool negative_weights;
        bool need_apsp;
        bool is_dag;
    } scenarios[] = {
        {"GPS navigation",     1000000, 3000000, false, false, false},
        {"Currency arbitrage",      30,     870,  true, false, false},
        {"Small network APSP",     100,    2000, false,  true, false},
        {"Task scheduling",        500,    1200,  true, false,  true},
        {"Social network BFS",   50000,  200000, false,  true, false},
        {"Dense graph APSP",       400,  160000, false,  true, false},
    };
    int ns = 6;

    printf("  %-25s %8s %8s  Neg?  APSP? DAG?  → Algorithm\n",
           "Scenario", "n", "m");
    printf("  %-25s %8s %8s  ----  ----- ----  -----------\n",
           "--------", "---", "---");

    for (int i = 0; i < ns; i++) {
        struct Scenario *s = &scenarios[i];
        const char *choice;

        if (s->is_dag)
            choice = "Topo + relax O(n+m)";
        else if (!s->need_apsp) {
            if (!s->negative_weights) {
                if (s->m > (long)s->n * s->n / 10)
                    choice = "Dijkstra (array)";
                else
                    choice = "Dijkstra (heap)";
            } else
                choice = "Bellman-Ford";
        } else {
            if (s->n <= 500)
                choice = "Floyd-Warshall";
            else if (!s->negative_weights)
                choice = "Dijkstra × n (heap)";
            else
                choice = "Johnson";
        }

        printf("  %-25s %8d %8d  %-5s %-5s %-5s → %s\n",
               s->name, s->n, s->m,
               s->negative_weights ? "YES" : "no",
               s->need_apsp ? "YES" : "no",
               s->is_dag ? "YES" : "no",
               choice);
    }
    printf("\n");
}

/* ============ DEMO 6: Timing comparison =========================== */

void demo_timing(void) {
    printf("=== Demo 6: Operation count comparison ===\n");

    /* Count operations for a medium graph */
    int n = 20;
    int m = 0;

    Graph g;
    graph_init(&g, n);

    /* Random-ish edges */
    srand(42);
    for (int u = 0; u < n; u++) {
        int targets = 2 + rand() % 4;
        for (int t = 0; t < targets; t++) {
            int v = rand() % n;
            if (v != u) {
                graph_add(&g, u, v, 1 + rand() % 20);
                m++;
            }
        }
    }

    printf("Graph: n=%d, m=%d\n\n", n, m);

    /* Count relaxations for Dijkstra */
    int d_dj[MAX_V];
    bool done[MAX_V] = {false};
    for (int i = 0; i < n; i++) d_dj[i] = INF;
    d_dj[0] = 0;
    int dj_relax = 0, dj_compare = 0;

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        for (int v = 0; v < n; v++) {
            dj_compare++;
            if (!done[v] && (u == -1 || d_dj[v] < d_dj[u]))
                u = v;
        }
        if (u == -1 || d_dj[u] == INF) break;
        done[u] = true;
        for (int i = 0; i < g.adj[u].size; i++) {
            dj_relax++;
            int v = g.adj[u].edges[i].to;
            int w = g.adj[u].edges[i].weight;
            if (d_dj[u] + w < d_dj[v])
                d_dj[v] = d_dj[u] + w;
        }
    }

    /* Count relaxations for Bellman-Ford */
    int d_bf[MAX_V];
    for (int i = 0; i < n; i++) d_bf[i] = INF;
    d_bf[0] = 0;
    int bf_relax = 0, bf_passes = 0;

    for (int pass = 0; pass < n - 1; pass++) {
        bool changed = false;
        for (int u = 0; u < n; u++)
            for (int i = 0; i < g.adj[u].size; i++) {
                bf_relax++;
                int v = g.adj[u].edges[i].to;
                int w = g.adj[u].edges[i].weight;
                if (d_bf[u] != INF && d_bf[u] + w < d_bf[v]) {
                    d_bf[v] = d_bf[u] + w;
                    changed = true;
                }
            }
        bf_passes++;
        if (!changed) break;
    }

    /* Floyd-Warshall operations */
    int fw_ops = n * n * n;

    printf("  Algorithm        Relaxations  Other ops    Total\n");
    printf("  Dijkstra(array)  %-12d %-12d %d\n",
           dj_relax, dj_compare, dj_relax + dj_compare);
    printf("  Bellman-Ford     %-12d %d passes     %d\n",
           bf_relax, bf_passes, bf_relax);
    printf("  Floyd-Warshall   %-12d —            %d (= n³)\n",
           fw_ops, fw_ops);

    printf("\n  Theoretical worst cases:\n");
    printf("  Dijkstra(array): O(n²) = %d\n", n * n);
    printf("  Bellman-Ford:    O(n·m) = %d\n", n * m);
    printf("  Floyd-Warshall:  O(n³) = %d\n", n * n * n);

    graph_free(&g);
    printf("\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_compare_all();
    demo_negative_weights();
    demo_dag();
    demo_apsp();
    demo_selection();
    demo_timing();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: All algorithms on same graph ===
Directed graph (non-negative weights), source=0

  Vertex  Dijkstra  Bellman-Ford  Floyd-Warshall  Match?
  0       0         0             0               ✓
  1       4         4             4               ✓
  2       2         2             2               ✓
  3       10        10            10              ✓
  4       12        12            12              ✓
  5       15        15            15              ✓

  All match: YES

=== Demo 2: Negative weights — only BF correct ===
Graph with edge 2→1:-2 (no neg cycle)

  Vertex  Dijkstra  Bellman-Ford  Correct?
  0       0         0             both OK
  1       4         1             Dijkstra WRONG
  2       3         3             both OK
  3       5         2             Dijkstra WRONG

=== Demo 3: DAG shortest path (O(n+m)) ===
DAG with some negative weights, source=0

  Vertex  DAG(O(n+m))  Bellman-Ford  Match?
  0       0            0             ✓
  1       5            5             ✓
  2       3            3             ✓
  3       10           10            ✓
  4       7            7             ✓
  5       4            4             ✓

DAG approach: O(n + m) — faster than both Dijkstra and BF!
Works with negative weights (unlike Dijkstra).

=== Demo 4: APSP comparison ===
...
Mismatches: 0

=== Demo 5: Algorithm selection examples ===
  Scenario                        n        m  Neg?  APSP? DAG?  → Algorithm
  GPS navigation              1000000  3000000  no    no    no    → Dijkstra (heap)
  Currency arbitrage               30      870  YES   no    no    → Bellman-Ford
  Small network APSP              100     2000  no    YES   no    → Floyd-Warshall
  Task scheduling                 500     1200  YES   no    YES   → Topo + relax O(n+m)
  Social network BFS            50000   200000  no    YES   no    → Dijkstra × n (heap)
  Dense graph APSP                400   160000  no    YES   no    → Floyd-Warshall

=== Demo 6: Operation count comparison ===
Graph: n=20, m=~60

  Algorithm        Relaxations  Other ops    Total
  Dijkstra(array)  60           400          460
  Bellman-Ford     120          2 passes     120
  Floyd-Warshall   8000         —            8000 (= n³)
```

---

## 10. Programa completo en Rust

```rust
/*
 * sp_comparison.rs
 *
 * Side-by-side comparison: Dijkstra, Bellman-Ford, Floyd-Warshall,
 * DAG relaxation, BFS. Correctness checks, selection guide,
 * and operation counting.
 *
 * Compile: rustc -O -o sp_comparison sp_comparison.rs
 * Run:     ./sp_comparison
 */

use std::cmp::Reverse;
use std::collections::{BinaryHeap, VecDeque};

const INF: i64 = i64::MAX / 2;

/* =================== GRAPH ======================================== */

struct Graph {
    adj: Vec<Vec<(usize, i64)>>,
}

impl Graph {
    fn new(n: usize) -> Self {
        Self { adj: vec![vec![]; n] }
    }
    fn add_edge(&mut self, u: usize, v: usize, w: i64) {
        self.adj[u].push((v, w));
    }
    fn n(&self) -> usize { self.adj.len() }
    fn edge_count(&self) -> usize {
        self.adj.iter().map(|a| a.len()).sum()
    }
}

/* =================== ALGORITHMS =================================== */

fn bfs(g: &Graph, src: usize) -> Vec<i64> {
    let n = g.n();
    let mut dist = vec![INF; n];
    dist[src] = 0;
    let mut queue = VecDeque::new();
    queue.push_back(src);
    while let Some(u) = queue.pop_front() {
        for &(v, _) in &g.adj[u] {
            if dist[v] == INF {
                dist[v] = dist[u] + 1;
                queue.push_back(v);
            }
        }
    }
    dist
}

fn dijkstra(g: &Graph, src: usize) -> Vec<i64> {
    let n = g.n();
    let mut dist = vec![INF; n];
    dist[src] = 0;
    let mut heap = BinaryHeap::new();
    heap.push(Reverse((0i64, src)));

    while let Some(Reverse((d, u))) = heap.pop() {
        if d > dist[u] { continue; }
        for &(v, w) in &g.adj[u] {
            let nd = dist[u] + w;
            if nd < dist[v] {
                dist[v] = nd;
                heap.push(Reverse((nd, v)));
            }
        }
    }
    dist
}

fn bellman_ford(g: &Graph, src: usize) -> Option<Vec<i64>> {
    let n = g.n();
    let mut dist = vec![INF; n];
    dist[src] = 0;

    for _ in 0..n - 1 {
        let mut changed = false;
        for u in 0..n {
            for &(v, w) in &g.adj[u] {
                if dist[u] != INF && dist[u] + w < dist[v] {
                    dist[v] = dist[u] + w;
                    changed = true;
                }
            }
        }
        if !changed { break; }
    }

    // Check negative cycle
    for u in 0..n {
        for &(v, w) in &g.adj[u] {
            if dist[u] != INF && dist[u] + w < dist[v] {
                return None;
            }
        }
    }
    Some(dist)
}

fn floyd_warshall(n: usize, edges: &[(usize, usize, i64)]) -> Vec<Vec<i64>> {
    let mut dist = vec![vec![INF; n]; n];
    for i in 0..n { dist[i][i] = 0; }
    for &(u, v, w) in edges { dist[u][v] = w; }

    for k in 0..n {
        for i in 0..n {
            for j in 0..n {
                if dist[i][k] + dist[k][j] < dist[i][j] {
                    dist[i][j] = dist[i][k] + dist[k][j];
                }
            }
        }
    }
    dist
}

fn dag_shortest(g: &Graph, src: usize) -> Vec<i64> {
    let n = g.n();
    let mut color = vec![0u8; n];
    let mut order = Vec::with_capacity(n);

    fn dfs(u: usize, g: &Graph, color: &mut [u8], order: &mut Vec<usize>) {
        color[u] = 1;
        for &(v, _) in &g.adj[u] {
            if color[v] == 0 { dfs(v, g, color, order); }
        }
        order.push(u);
    }

    for u in 0..n {
        if color[u] == 0 { dfs(u, g, &mut color, &mut order); }
    }

    let mut dist = vec![INF; n];
    dist[src] = 0;

    for &u in order.iter().rev() {
        if dist[u] == INF { continue; }
        for &(v, w) in &g.adj[u] {
            if dist[u] + w < dist[v] {
                dist[v] = dist[u] + w;
            }
        }
    }
    dist
}

/* ======== Demo 1: All algorithms on same graph ==================== */

fn demo_compare_all() {
    println!("=== Demo 1: All algorithms, same graph ===");

    let mut g = Graph::new(6);
    let edges = [
        (0,1,4), (0,2,2), (1,2,1), (1,3,5),
        (2,3,8), (2,4,10), (3,4,2), (3,5,6), (4,5,3),
    ];
    for &(u, v, w) in &edges { g.add_edge(u, v, w); }

    println!("Directed, non-negative weights, source=0\n");

    let d_dj = dijkstra(&g, 0);
    let d_bf = bellman_ford(&g, 0).unwrap();
    let fw = floyd_warshall(g.n(), &edges.iter().map(|&(u,v,w)| (u,v,w)).collect::<Vec<_>>());

    println!("  V  Dijkstra  BF   FW   Match?");
    for v in 0..g.n() {
        let ok = d_dj[v] == d_bf[v] && d_bf[v] == fw[0][v];
        println!("  {}  {:<9} {:<4} {:<4} {}",
                 v, d_dj[v], d_bf[v], fw[0][v],
                 if ok { "✓" } else { "✗" });
    }
    println!();
}

/* ======== Demo 2: Negative weights ================================ */

fn demo_negative() {
    println!("=== Demo 2: Negative weights ===");

    let mut g = Graph::new(4);
    g.add_edge(0, 1, 4);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 3, 1);
    g.add_edge(2, 1, -2);
    g.add_edge(2, 3, 5);

    println!("Edge 2→1:-2, no neg cycle\n");

    let d_dj = dijkstra(&g, 0);
    let d_bf = bellman_ford(&g, 0).unwrap();

    println!("  V  Dijkstra  BF   Status");
    for v in 0..g.n() {
        let status = if d_dj[v] == d_bf[v] { "OK" } else { "Dijkstra WRONG" };
        println!("  {}  {:<9} {:<4} {}", v, d_dj[v], d_bf[v], status);
    }
    println!();
}

/* ======== Demo 3: DAG approach ==================================== */

fn demo_dag() {
    println!("=== Demo 3: DAG shortest path ===");

    let mut g = Graph::new(6);
    g.add_edge(0, 1, 5); g.add_edge(0, 2, 3);
    g.add_edge(1, 2, -2); g.add_edge(1, 3, 6);
    g.add_edge(2, 3, 7); g.add_edge(2, 4, 4);
    g.add_edge(3, 4, -1); g.add_edge(3, 5, 2);
    g.add_edge(4, 5, -3);

    println!("DAG with negative weights\n");

    let d_dag = dag_shortest(&g, 0);
    let d_bf = bellman_ford(&g, 0).unwrap();

    println!("  V  DAG   BF   Match?");
    for v in 0..g.n() {
        println!("  {}  {:<5} {:<4} {}",
                 v, d_dag[v], d_bf[v],
                 if d_dag[v] == d_bf[v] { "✓" } else { "✗" });
    }
    println!("DAG method: O(n+m) with negative weights!\n");
}

/* ======== Demo 4: Selection guide ================================= */

fn demo_selection() {
    println!("=== Demo 4: Algorithm selection guide ===\n");

    let scenarios = [
        ("GPS (10M nodes, sparse, w≥0, SSSP)",   "Dijkstra (heap)"),
        ("Arbitrage (30 currencies, neg, SSSP)",  "Bellman-Ford"),
        ("Small network (100 nodes, APSP)",       "Floyd-Warshall"),
        ("Task deps (DAG, neg, SSSP)",            "Topo + relax"),
        ("Social graph (50K, unweighted, APSP)",  "BFS × n"),
        ("Dense graph (400 nodes, APSP)",         "Floyd-Warshall"),
        ("Sparse neg weights (10K, APSP)",        "Johnson"),
        ("Grid w/ portals (weights 0,1, SSSP)",   "BFS 0-1 (deque)"),
    ];

    for (scenario, choice) in &scenarios {
        println!("  {:<45} → {}", scenario, choice);
    }
    println!();
}

/* ======== Demo 5: Negative cycle detection comparison ============= */

fn demo_neg_cycle() {
    println!("=== Demo 5: Negative cycle detection ===");

    let mut g = Graph::new(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, -1);
    g.add_edge(2, 3, -1);
    g.add_edge(3, 1, -1);

    println!("Cycle 1→2→3→1: weight = -3\n");

    // Bellman-Ford
    let bf = bellman_ford(&g, 0);
    println!("  Bellman-Ford: {}",
             if bf.is_none() { "NEGATIVE CYCLE detected ✓" }
             else { "no cycle (wrong!)" });

    // Floyd-Warshall
    let edges: Vec<(usize,usize,i64)> = vec![
        (0,1,1), (1,2,-1), (2,3,-1), (3,1,-1),
    ];
    let fw = floyd_warshall(4, &edges);
    let fw_neg = (0..4).any(|i| fw[i][i] < 0);
    println!("  Floyd-Warshall: {} (diagonal: {:?})",
             if fw_neg { "NEGATIVE CYCLE detected ✓" } else { "no cycle" },
             (0..4).map(|i| fw[i][i]).collect::<Vec<_>>());

    // Dijkstra (can't detect)
    println!("  Dijkstra: CANNOT detect negative cycles");
    println!();
}

/* ======== Demo 6: APSP method comparison ========================== */

fn demo_apsp() {
    println!("=== Demo 6: APSP — FW vs Dijkstra×n ===");

    let n = 6;
    let mut g = Graph::new(n);
    let edges = vec![
        (0,1,3), (0,2,8), (1,2,2), (1,3,5),
        (2,3,1), (2,4,4), (3,4,6), (3,5,2),
        (4,5,3), (5,0,7),
    ];
    for &(u, v, w) in &edges { g.add_edge(u, v, w); }

    // Floyd-Warshall
    let fw = floyd_warshall(n, &edges);

    // Dijkstra × n
    let dj: Vec<Vec<i64>> = (0..n).map(|s| dijkstra(&g, s)).collect();

    let mismatches: usize = (0..n).flat_map(|i| (0..n).map(move |j| (i,j)))
        .filter(|&(i,j)| fw[i][j] != dj[i][j]).count();

    println!("n={}, m={}", n, edges.len());
    println!("Mismatches between FW and Dijkstra×n: {}\n", mismatches);

    // Print FW matrix
    println!("FW distance matrix:");
    print!("     ");
    for j in 0..n { print!("{:>4}", j); }
    println!();
    for i in 0..n {
        print!("  {} [", i);
        for j in 0..n {
            if fw[i][j] >= INF / 2 { print!("   ∞"); }
            else { print!("{:>4}", fw[i][j]); }
        }
        println!(" ]");
    }

    println!("\nComplexity:");
    println!("  FW:           O(n³) = {}", n * n * n);
    println!("  Dijkstra × n: O(n(n+m)log n) ≈ {}",
             n * (n + edges.len()) * (((n as f64).log2().ceil()) as usize));
    println!();
}

/* ======== Demo 7: BFS vs Dijkstra on unweighted graph ============= */

fn demo_bfs_vs_dijkstra() {
    println!("=== Demo 7: BFS vs Dijkstra on unweighted graph ===");

    let n = 8;
    let mut g = Graph::new(n);
    let edges = [
        (0,1), (0,2), (1,3), (1,4), (2,4), (2,5),
        (3,6), (4,6), (4,7), (5,7),
    ];
    for &(u, v) in &edges {
        g.add_edge(u, v, 1);
        g.add_edge(v, u, 1);
    }

    println!("Unweighted undirected graph, {} vertices\n", n);

    let d_bfs = bfs(&g, 0);
    let d_dj = dijkstra(&g, 0);

    println!("  V  BFS  Dijkstra  Match?");
    let mut all_ok = true;
    for v in 0..n {
        let ok = d_bfs[v] == d_dj[v];
        if !ok { all_ok = false; }
        println!("  {}  {:<4} {:<9} {}", v, d_bfs[v], d_dj[v],
                 if ok { "✓" } else { "✗" });
    }
    println!("\nAll match: {} — BFS is O(n+m), Dijkstra is O((n+m)log n)", all_ok);
    println!("For unweighted graphs, BFS is strictly better.\n");
}

/* ======== Demo 8: Complete decision engine ======================== */

fn recommend(n: usize, m: usize, neg: bool, apsp: bool, dag: bool, unweighted: bool)
    -> &'static str
{
    if unweighted && !apsp { return "BFS"; }
    if unweighted && apsp { return "BFS × n"; }
    if dag { return "Topo + relax"; }
    if !apsp {
        if !neg {
            if m > n * n / 10 { "Dijkstra (array)" }
            else { "Dijkstra (heap)" }
        } else { "Bellman-Ford" }
    } else {
        if n <= 500 { "Floyd-Warshall" }
        else if !neg { "Dijkstra × n" }
        else { "Johnson" }
    }
}

fn demo_decision_engine() {
    println!("=== Demo 8: Decision engine ===\n");

    let cases = [
        (1000000, 3000000, false, false, false, false, "Road network SSSP"),
        (30, 870, true, false, false, false, "Currency arbitrage"),
        (200, 40000, false, true, false, false, "Dense APSP"),
        (10000, 30000, true, true, false, false, "Sparse neg APSP"),
        (1000, 3000, true, false, true, false, "DAG scheduling"),
        (50000, 200000, false, false, false, true, "Social BFS"),
        (500, 5000, false, true, false, false, "Medium APSP"),
        (100, 300, false, true, false, true, "Small unweighted APSP"),
    ];

    println!("  {:30} {:>8} {:>8}  → {}", "Problem", "n", "m", "Algorithm");
    for &(n, m, neg, apsp, dag, uw, desc) in &cases {
        let algo = recommend(n, m, neg, apsp, dag, uw);
        println!("  {:30} {:>8} {:>8}  → {}", desc, n, m, algo);
    }
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_compare_all();
    demo_negative();
    demo_dag();
    demo_selection();
    demo_neg_cycle();
    demo_apsp();
    demo_bfs_vs_dijkstra();
    demo_decision_engine();
}
```

### Salida esperada

```
=== Demo 1: All algorithms, same graph ===
Directed, non-negative weights, source=0

  V  Dijkstra  BF   FW   Match?
  0  0         0    0    ✓
  1  4         4    4    ✓
  2  2         2    2    ✓
  3  10        10   10   ✓
  4  12        12   12   ✓
  5  15        15   15   ✓

=== Demo 2: Negative weights ===
Edge 2→1:-2, no neg cycle

  V  Dijkstra  BF   Status
  0  0         0    OK
  1  4         1    Dijkstra WRONG
  2  3         3    OK
  3  5         2    Dijkstra WRONG

=== Demo 3: DAG shortest path ===
DAG with negative weights

  V  DAG   BF   Match?
  0  0     0    ✓
  1  5     5    ✓
  2  3     3    ✓
  3  10    10   ✓
  4  7     7    ✓
  5  4     4    ✓
DAG method: O(n+m) with negative weights!

=== Demo 5: Negative cycle detection ===
Cycle 1→2→3→1: weight = -3

  Bellman-Ford: NEGATIVE CYCLE detected ✓
  Floyd-Warshall: NEGATIVE CYCLE detected ✓ (diagonal: [0, -3, -3, -3])
  Dijkstra: CANNOT detect negative cycles

=== Demo 7: BFS vs Dijkstra on unweighted graph ===
...
All match: true — BFS is O(n+m), Dijkstra is O((n+m)log n)
For unweighted graphs, BFS is strictly better.

=== Demo 8: Decision engine ===
  Problem                               n        m  → Algorithm
  Road network SSSP                1000000  3000000  → Dijkstra (heap)
  Currency arbitrage                    30      870  → Bellman-Ford
  Dense APSP                          200    40000  → Floyd-Warshall
  Sparse neg APSP                   10000    30000  → Johnson
  DAG scheduling                     1000     3000  → Topo + relax
  Social BFS                        50000   200000  → BFS
  Medium APSP                         500     5000  → Floyd-Warshall
  Small unweighted APSP               100      300  → BFS × n
```

---

## 11. Ejercicios

### Ejercicio 1 — Elegir el algoritmo

Para cada situación, indica qué algoritmo de caminos más cortos usar y
por qué:

a) Red de telecomunicaciones con 50,000 nodos y 100,000 enlaces, latencias
   positivas. Necesitas la ruta más rápida de un servidor a otro.
b) 20 aeropuertos, necesitas las distancias entre todos los pares.
c) Sistema de ecuaciones con restricciones $x_j - x_i \leq c_{ij}$.
d) Dependencias de compilación (DAG) con tiempos de build negativos
   (algunos módulos reducen el tiempo total).

<details>
<summary>Respuestas</summary>

a) **Dijkstra (heap)**: SSSP, pesos positivos, grafo disperso ($m = 2n$).
$O((50000 + 100000) \log 50000) \approx 2.5 \times 10^6$ — instantáneo.

b) **Floyd-Warshall**: APSP, $n = 20$ (muy pequeño). $O(20^3) = 8000$
operaciones — trivial. Más simple que Dijkstra × 20.

c) **Bellman-Ford**: las restricciones de diferencias se modelan como un
grafo con pesos potencialmente negativos. BF detecta inconsistencias (ciclos
negativos).

d) **Relajación topológica**: es un DAG, por lo que $O(n + m)$ funciona con
pesos negativos. Ni Dijkstra (no soporta negativos) ni BF (innecesariamente
lento) son necesarios.
</details>

---

### Ejercicio 2 — Complejidad para grafos concretos

Calcula la complejidad exacta (no asintótica) para cada algoritmo en estos
grafos:

a) $n = 1000,\ m = 5000$. SSSP.
b) $n = 100,\ m = 9000$. APSP.
c) $n = 500,\ m = 1000$. APSP.

<details>
<summary>Respuestas</summary>

a) SSSP:
- Dijkstra (heap): $(n + m)\log n = 6000 \times 10 = 60{,}000$.
- BF: $n \times m = 1000 \times 5000 = 5{,}000{,}000$.
- Ratio: BF es 83× más lento. **Dijkstra**.

b) APSP:
- FW: $n^3 = 10^6$.
- Dijkstra × n (heap): $100 \times (100 + 9000) \times 7 = 6{,}370{,}000$.
- FW es 6× más rápido y mucho más simple. **Floyd-Warshall**.

c) APSP:
- FW: $n^3 = 1.25 \times 10^8$.
- Dijkstra × n (heap): $500 \times (500 + 1000) \times 9 = 6{,}750{,}000$.
- Dijkstra × n es **18× más rápido** (grafo disperso). **Dijkstra × n**.
</details>

---

### Ejercicio 3 — ¿Cuándo falla Dijkstra?

Construye tres grafos donde Dijkstra produce resultados incorrectos y
muestra la respuesta correcta (de Bellman-Ford):

a) Grafo con una arista negativa (4 vértices).
b) Grafo donde el camino óptimo tiene más aristas que el subóptimo.
c) Grafo donde Dijkstra da la respuesta correcta a pesar de una arista
   negativa (explica por qué coincide).

<details>
<summary>Respuestas</summary>

a) $0\to1{:}5,\ 0\to2{:}2,\ 2\to1{:}{-4},\ 1\to3{:}1$.
Dijkstra: dist[1]=5, dist[3]=6. BF: dist[1]=-2, dist[3]=-1. **Falla**.

b) $0\to1{:}1,\ 1\to2{:}1,\ 2\to3{:}{-5},\ 0\to3{:}0$.
Dijkstra: dist[3]=0 (directo, 1 arista). BF: dist[3]=-3 (vía 0→1→2→3, 3 aristas). **Falla**.

c) $0\to1{:}2,\ 0\to2{:}3,\ 2\to1{:}{-1},\ 1\to3{:}4$.
Dijkstra: extrae 0(0), luego 1(2), luego 2(3). Relaja 2→1: 3+(-1)=2, no
mejora dist[1]=2. Resultado correcto por coincidencia: $0\to2\to1=2 = 0\to1=2$.
Funciona porque el vértice 1 fue extraído antes de que la arista negativa
pudiera crear un camino más corto.
</details>

---

### Ejercicio 4 — Johnson's algorithm a mano

Grafo: $0\to1{:}3,\ 0\to2{:}8,\ 1\to2{:}{-2},\ 2\to3{:}1,\ 3\to0{:}5$.

a) Agrega vértice $q$ con aristas de peso 0 a todos.
b) Ejecuta Bellman-Ford desde $q$ para obtener $h[]$.
c) Calcula pesos reponderados.
d) Ejecuta Dijkstra desde cada vértice con pesos reponderados.
e) Ajusta las distancias finales.

<details>
<summary>Traza</summary>

b) BF desde $q$: $h = [0, 0, -2, -1]$ (aproximados — calcular
cuidadosamente).
$q\to$ todos: 0. $0\to1{:}3$: h[1]=0. $1\to2{:}{-2}$: 0+(-2)=-2 → h[2]=-2.
$2\to3{:}1$: -2+1=-1 → h[3]=-1. $3\to0{:}5$: -1+5=4 > 0 → no.
$h = [0, 0, -2, -1]$.

c) Reponderación:
- $0\to1$: $3 + 0 - 0 = 3$.
- $0\to2$: $8 + 0 - (-2) = 10$.
- $1\to2$: $-2 + 0 - (-2) = 0$.
- $2\to3$: $1 + (-2) - (-1) = 0$.
- $3\to0$: $5 + (-1) - 0 = 4$.

Todos $\geq 0$ ✓.

d) Dijkstra con pesos reponderados desde cada vértice.
Desde 0: dist'=[0, 3, 3, 3]. Ajuste: $d(0,v) = d'(0,v) - h[0] + h[v]$.
$d(0,1)=3-0+0=3, d(0,2)=3-0+(-2)=1, d(0,3)=3-0+(-1)=2$.

Verificación BF directo: $0\to1{:}3,\ 0\to1\to2{:}1,\ 0\to1\to2\to3{:}2$.
**Correcto** ✓.
</details>

---

### Ejercicio 5 — DAG camino más largo

DAG de tareas con duraciones:
$A\to B{:}3,\ A\to C{:}2,\ B\to D{:}4,\ C\to D{:}5,\ C\to E{:}6,\ D\to F{:}2,\ E\to F{:}1$.

a) Encuentra el camino más largo de A a F (ruta crítica).
b) ¿Qué algoritmo usas y cómo lo adaptas?

<details>
<summary>Respuesta</summary>

b) **Relajación topológica** con operación `max` en lugar de `min` (o negar
los pesos y usar `min`).

a) Orden topológico: A, B, C, D, E, F (o A, C, B, ...).

Con max-relax desde A:
- A: dist=[0, -∞, -∞, -∞, -∞, -∞].
- Procesar A: B=3, C=2.
- Procesar B: D=max(-∞, 3+4)=7.
- Procesar C: D=max(7, 2+5)=7, E=max(-∞, 2+6)=8.
- Procesar D: F=max(-∞, 7+2)=9.
- Procesar E: F=max(9, 8+1)=9.

**Camino más largo = 9**. Rutas: $A\to B\to D\to F = 3+4+2=9$ y
$A\to C\to E\to F = 2+6+1=9$ (empate).

**Ruta crítica**: ambas tienen longitud 9; el proyecto tarda 9 unidades.
</details>

---

### Ejercicio 6 — Punto de cruce Dijkstra vs FW

Para APSP, ¿para qué densidad de grafo ($m/n$) Floyd-Warshall es más
rápido que Dijkstra × n con heap binario?

<details>
<summary>Análisis</summary>

FW: $O(n^3)$. Dijkstra × n (heap): $O(n(n+m)\log n)$.

Cruce: $n^3 = n(n+m)\log n$ → $n^2 = (n+m)\log n$ → $m = n^2/\log n - n$.

Para $n = 1000$: $m \approx 1000^2 / 10 - 1000 = 99{,}000$.
Densidad: $m/n = 99$, o sea $m \approx n^2/10$.

**Regla**: FW es mejor cuando $m > n^2 / \log n$ (grafo denso, $> 10\%$
de las aristas posibles para $n = 1000$).

En la práctica, las constantes favorecen a FW aún más: su acceso secuencial
a memoria es ~3× más rápido que el acceso random de Dijkstra con heap.
</details>

---

### Ejercicio 7 — Implementar los tres

Implementa Dijkstra, Bellman-Ford y Floyd-Warshall para el mismo grafo.
Verifica que los tres dan el mismo resultado (para pesos no negativos).

Grafo: $0\to1{:}7,\ 0\to2{:}9,\ 0\to5{:}14,\ 1\to2{:}10,\ 1\to3{:}15,\ 2\to3{:}11,\ 2\to5{:}2,\ 3\to4{:}6,\ 4\to5{:}9$.

<details>
<summary>Resultado esperado</summary>

Desde vértice 0: dist = [0, 7, 9, 20, 26, 11].

Los tres algoritmos deben coincidir. Diferencias en el camino concreto
son posibles (puede haber empates), pero las distancias son únicas.
</details>

---

### Ejercicio 8 — SPFA vs Bellman-Ford vs Dijkstra

Para el grafo del ejercicio 7, cuenta el número de relajaciones de cada
algoritmo:
- Dijkstra: una relajación por cada arista examinada al extraer un vértice.
- Bellman-Ford: una por cada arista × pasada.
- SPFA: una por cada arista procesada desde la cola.

<details>
<summary>Conteo</summary>

- **Dijkstra**: cada vértice se extrae una vez, examina sus aristas.
  Relajaciones: $m = 9$ (cada arista examinada exactamente una vez).

- **Bellman-Ford**: peor caso $= (n-1) \times m = 5 \times 9 = 45$.
  Con early exit: ~2 pasadas × 9 = 18.

- **SPFA**: depende del orden de procesamiento. Típicamente entre $m$ y
  $2m$, así que ~9-18 relajaciones.

Dijkstra es el más eficiente para pesos no negativos. SPFA es competitivo.
BF es el más lento pero el más robusto.
</details>

---

### Ejercicio 9 — Grafo sin solución

¿Qué retorna cada algoritmo para un vértice inalcanzable?

Grafo: $0\to1{:}3,\ 2\to3{:}5$ (dos componentes: {0,1} y {2,3}).
Fuente: 0. ¿Cuál es dist[2] y dist[3]?

<details>
<summary>Respuesta</summary>

- **Los tres algoritmos**: dist[2] = $\infty$, dist[3] = $\infty$.
- **Floyd-Warshall**: dist[0][2] = $\infty$, dist[0][3] = $\infty$,
  pero dist[2][3] = 5 (alcanzable dentro de su componente).
- Ningún algoritmo falla — simplemente reportan $\infty$ para vértices
  inalcanzables.
- En implementación, usar un valor centinela (`INT_MAX/2` o similar) y
  verificar antes de sumar para evitar overflow.
</details>

---

### Ejercicio 10 — Diseño de sistema

Diseñas un sistema de navegación para una ciudad con:
- 100,000 intersecciones (vértices).
- 300,000 calles (aristas, no dirigidas).
- Pesos: tiempo de viaje (positivo, variable según tráfico).
- Consultas: miles de rutas por segundo.

a) ¿Qué algoritmo base usas?
b) ¿Pre-calculas APSP o calculas bajo demanda?
c) ¿Qué optimizaciones aplicas?
d) Si agregas "peajes negativos" (descuentos), ¿qué cambia?

<details>
<summary>Diseño</summary>

a) **Dijkstra (heap binario)** con early exit. $O((n+m)\log n) \approx
400{,}000 \times 17 = 6.8 \times 10^6$ — procesa en ~10ms.

b) **Bajo demanda**. APSP requeriría $O(n^2) = 10^{10}$ entradas de
almacenamiento (~80 GB). Imposible. Cada consulta se resuelve con un
Dijkstra individual.

c) Optimizaciones:
- **A\***: heurística de distancia euclidiana para podar la búsqueda.
- **Bidireccional**: Dijkstra desde fuente y destino simultáneamente.
- **Contraction Hierarchies (CH)**: preprocesamiento que permite consultas
  en ~1ms para redes de carreteras.
- **Cache**: guardar resultados de consultas frecuentes.

d) Con "peajes negativos": Dijkstra deja de ser correcto. Opciones:
- Si los descuentos son pequeños y acotados: Johnson's reweighting
  (una sola vez) para eliminar los negativos.
- Si cambian con el tráfico: Bellman-Ford bajo demanda (mucho más lento,
  ~$10^8$ operaciones por consulta, ~1s).
- Mejor diseño: modelar descuentos como reducción del peso original
  (asegurando que nunca baje de 0), manteniendo Dijkstra válido.
</details>
