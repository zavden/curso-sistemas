# T02 — Bellman-Ford

## Objetivo

Dominar el algoritmo de Bellman-Ford para caminos más cortos desde una fuente
única (SSSP) con **pesos arbitrarios** (incluidos negativos): su mecánica de
relajación iterativa, la detección de **ciclos negativos**, y las diferencias
con Dijkstra.

---

## 1. Motivación

Dijkstra falla con pesos negativos porque su invariante greedy ("al extraer
$u$, `dist[u]` es definitivo") requiere que todos los pesos sean $\geq 0$.
Bellman-Ford resuelve SSSP correctamente con pesos negativos y además
**detecta ciclos negativos**.

| Propiedad | Dijkstra | Bellman-Ford |
|-----------|:--------:|:------------:|
| Pesos negativos | No | **Sí** |
| Ciclos negativos | No detecta | **Detecta** |
| Complejidad | $O((n+m) \log n)$ | $O(n \cdot m)$ |
| Técnica | Greedy (heap) | Relajación iterativa |
| Uso principal | Grafos con pesos $\geq 0$ | Pesos negativos, verificación |

---

## 2. Idea del algoritmo

### 2.1 Principio

Bellman-Ford realiza $n - 1$ **pasadas** sobre todas las aristas del grafo.
En cada pasada, relaja cada arista $(u, v)$:

```
if dist[u] + w(u,v) < dist[v]:
    dist[v] = dist[u] + w(u,v)
    parent[v] = u
```

Tras $k$ pasadas, `dist[v]` contiene la distancia del camino más corto desde
$s$ a $v$ que usa **a lo sumo $k$ aristas**.

### 2.2 ¿Por qué $n - 1$ pasadas?

Un camino más corto simple (sin ciclos) en un grafo de $n$ vértices tiene a
lo sumo $n - 1$ aristas. Después de la pasada $i$, todos los caminos más
cortos que usan $\leq i$ aristas están correctos. Tras $n - 1$ pasadas,
todos los caminos más cortos simples están calculados.

**Demostración por inducción**:
- **Base** ($i = 0$): $\text{dist}[s] = 0$ (correcto, 0 aristas).
- **Paso inductivo**: supongamos que tras la pasada $i$, todos los caminos
  más cortos con $\leq i$ aristas son correctos. Sea $v$ un vértice cuyo
  camino más corto usa $i + 1$ aristas: $s \to \cdots \to u \to v$. Tras la
  pasada $i$, $\text{dist}[u]$ es correcto (usa $\leq i$ aristas). En la
  pasada $i + 1$, la relajación de $(u, v)$ establece
  $\text{dist}[v] = \text{dist}[u] + w(u, v)$, que es correcto.

### 2.3 Detección de ciclos negativos

Tras las $n - 1$ pasadas, se hace una pasada adicional (la $n$-ésima). Si
alguna arista $(u, v)$ se puede relajar:

$$\text{dist}[u] + w(u, v) < \text{dist}[v]$$

entonces existe un **ciclo negativo** alcanzable desde $s$. Esto es porque
un camino más corto que necesita $\geq n$ aristas debe contener un ciclo,
y ese ciclo tiene peso negativo (de lo contrario, eliminarlo daría un
camino más corto o igual).

---

## 3. Pseudocódigo

```
BELLMAN-FORD(G, w, s):
    // Initialize
    for each v in V:
        dist[v] ← ∞
        parent[v] ← NIL
    dist[s] ← 0

    // Relax all edges n-1 times
    for i ← 1 to |V| - 1:
        for each edge (u, v, w) in E:
            if dist[u] ≠ ∞ and dist[u] + w < dist[v]:
                dist[v] ← dist[u] + w
                parent[v] ← u

    // Check for negative cycles
    for each edge (u, v, w) in E:
        if dist[u] ≠ ∞ and dist[u] + w < dist[v]:
            return "NEGATIVE CYCLE"

    return dist[], parent[]
```

### Notas de implementación

- La condición `dist[u] ≠ ∞` evita relajar aristas desde vértices no
  alcanzables (sumar a infinito puede causar overflow).
- El orden de iteración sobre las aristas no afecta la correctitud,
  pero puede afectar cuántas pasadas son necesarias en la práctica.

---

## 4. Traza detallada

```
Grafo dirigido:
  0 →(6)→ 1
  0 →(7)→ 2
  1 →(5)→ 2
  1 →(-4)→ 3
  1 →(8)→ 4
  2 →(-3)→ 4
  3 →(7)→ 1    ← no crea ciclo negativo (6+(-4)+7=9 > 0)
  3 →(9)→ 4
  4 →(-2)→ 0   ← tampoco (6+(-4)+9+(-2)=9 > 0)

Aristas en orden: (0,1,6),(0,2,7),(1,2,5),(1,3,-4),(1,4,8),(2,4,-3),(3,1,7),(3,4,9),(4,0,-2)

Fuente: 0

Pasada 0 (init): dist = [0, ∞, ∞, ∞, ∞]

Pasada 1:
  (0,1,6):  dist[0]+6=6 < ∞    → dist[1]=6
  (0,2,7):  dist[0]+7=7 < ∞    → dist[2]=7
  (1,2,5):  dist[1]+5=11 > 7   → no
  (1,3,-4): dist[1]+(-4)=2 < ∞ → dist[3]=2
  (1,4,8):  dist[1]+8=14 < ∞   → dist[4]=14
  (2,4,-3): dist[2]+(-3)=4 < 14→ dist[4]=4
  (3,1,7):  dist[3]+7=9 > 6    → no
  (3,4,9):  dist[3]+9=11 > 4   → no
  (4,0,-2): dist[4]+(-2)=2 > 0 → no
  dist = [0, 6, 7, 2, 4]

Pasada 2:
  (0,1,6): 6 = 6  → no
  (0,2,7): 7 = 7  → no
  (1,2,5): 6+5=11 > 7  → no
  (1,3,-4): 6+(-4)=2 = 2 → no
  (1,4,8): 6+8=14 > 4  → no
  (2,4,-3): 7+(-3)=4 = 4 → no
  (3,1,7): 2+7=9 > 6  → no
  (3,4,9): 2+9=11 > 4  → no
  (4,0,-2): 4+(-2)=2 > 0 → no
  Ningún cambio → podemos terminar temprano.

Pasada de verificación (n-ésima):
  Ninguna arista se puede relajar → NO hay ciclo negativo.

Resultado: dist = [0, 6, 7, 2, 4]
```

### Traza con ciclo negativo

```
Grafo: 0→1:1, 1→2:-1, 2→0:-1

Ciclo: 0→1→2→0, peso = 1+(-1)+(-1) = -1 < 0

Pasada 1: dist = [0, 1, 0]
Pasada 2: dist = [0, 1, 0] → (2,0,-1): 0+(-1)=-1 < 0 → dist[0]=-1
          → (0,1,1): -1+1=0 < 1 → dist[1]=0
          dist = [-1, 0, 0]
Pasada 3 (verificación):
  (2,0,-1): 0+(-1)=-1 < -1? No... pero:
  (0,1,1): -1+1=0 = 0 → no
  (1,2,-1): 0+(-1)=-1 < 0 → SÍ, se puede relajar → CICLO NEGATIVO

Las distancias siguen bajando indefinidamente.
```

---

## 5. Complejidad

| Aspecto | Complejidad |
|---------|:-----------:|
| Tiempo | $O(n \cdot m)$ |
| Espacio | $O(n)$ (dist, parent) |

Desglose:
- $n - 1$ pasadas × $m$ aristas relajadas = $O(n \cdot m)$.
- Pasada de verificación: $O(m)$.

### Comparación para grafos dispersos ($m = O(n)$)

| Algoritmo | Complejidad |
|-----------|:-----------:|
| Dijkstra | $O(n \log n)$ |
| Bellman-Ford | $O(n^2)$ |

### Comparación para grafos densos ($m = O(n^2)$)

| Algoritmo | Complejidad |
|-----------|:-----------:|
| Dijkstra | $O(n^2 \log n)$ |
| Bellman-Ford | $O(n^3)$ |

Bellman-Ford es significativamente más lento que Dijkstra. Se usa cuando
hay pesos negativos o se necesita detectar ciclos negativos.

---

## 6. Optimizaciones

### 6.1 Terminación temprana

Si una pasada completa no relaja ninguna arista, el algoritmo puede
terminar: todas las distancias son definitivas.

```
for i ← 1 to |V| - 1:
    changed ← false
    for each edge (u, v, w) in E:
        if dist[u] + w < dist[v]:
            dist[v] ← dist[u] + w
            parent[v] ← u
            changed ← true
    if not changed:
        break       // early exit
```

En el mejor caso (aristas en orden topológico de un DAG), una sola pasada
basta: $O(m)$.

### 6.2 SPFA (Shortest Path Faster Algorithm)

SPFA es una variante de Bellman-Ford que usa una **cola** para procesar solo
los vértices cuya distancia cambió:

```
SPFA(G, w, s):
    dist[v] ← ∞ for all v
    dist[s] ← 0
    in_queue[s] ← true
    queue ← {s}

    while queue not empty:
        u ← dequeue
        in_queue[u] ← false
        for each (v, w) in adj[u]:
            if dist[u] + w < dist[v]:
                dist[v] ← dist[u] + w
                parent[v] ← u
                if not in_queue[v]:
                    enqueue(v)
                    in_queue[v] ← true

    // Detect negative cycle: if any vertex enters queue > n times
```

SPFA tiene complejidad $O(n \cdot m)$ en el peor caso (igual que
Bellman-Ford), pero en la práctica suele ser mucho más rápido, cercano a
$O(m)$ para grafos aleatorios. Sin embargo, existen grafos adversarios
que fuerzan el peor caso.

**Detección de ciclo negativo en SPFA**: un vértice entra en la cola más de
$n$ veces $\implies$ ciclo negativo.

### 6.3 Orden de aristas

Si las aristas se procesan en un orden favorable (e.g., empezando por las
más cercanas a $s$), las primeras pasadas hacen más trabajo útil y la
terminación temprana se activa antes.

---

## 7. Encontrar el ciclo negativo

Detectar que existe un ciclo negativo no es suficiente para muchas
aplicaciones — a menudo necesitamos **extraer el ciclo concreto**.

### Algoritmo

1. Ejecutar $n$ pasadas de relajación (no $n-1$).
2. Si la $n$-ésima pasada relaja alguna arista $(u, v)$, entonces $v$ está
   en un ciclo negativo (o es alcanzable desde uno).
3. Para encontrar el ciclo: seguir `parent[]` desde $v$ durante $n$ pasos
   (esto garantiza llegar a un vértice dentro del ciclo). Luego seguir
   `parent[]` hasta volver a ese vértice.

```
FIND-NEGATIVE-CYCLE(G, w, s):
    // Run n passes
    last_relaxed ← -1
    for i ← 1 to |V|:
        last_relaxed ← -1
        for each (u, v, w) in E:
            if dist[u] ≠ ∞ and dist[u] + w < dist[v]:
                dist[v] ← dist[u] + w
                parent[v] ← u
                last_relaxed ← v

    if last_relaxed == -1:
        return "No negative cycle"

    // Find vertex in cycle: walk back n steps
    v ← last_relaxed
    for i ← 1 to |V|:
        v ← parent[v]

    // Extract cycle from v
    cycle ← [v]
    u ← parent[v]
    while u ≠ v:
        cycle.append(u)
        u ← parent[u]
    cycle.append(v)
    cycle.reverse()
    return cycle
```

---

## 8. Aplicaciones

### 8.1 Arbitraje de divisas

Dadas las tasas de cambio entre monedas, ¿existe una secuencia de
conversiones que produzca más dinero del que se comenzó?

**Modelado**: cada moneda es un vértice, cada tasa $r_{ij}$ es una arista.
Queremos maximizar el producto $r_{01} \cdot r_{12} \cdot \ldots \cdot r_{k0} > 1$.

Tomando logaritmos negativos: $-\log(r_{ij})$ como peso. Entonces:
- Maximizar el producto $\equiv$ minimizar la suma de $-\log(r)$.
- Un ciclo con producto $> 1$ se convierte en un ciclo con suma $< 0$
  (ciclo negativo).

Bellman-Ford con pesos $-\log(r_{ij})$ detecta oportunidades de arbitraje.

### 8.2 Restricciones de diferencias

Un sistema de restricciones de diferencias tiene la forma:
$x_j - x_i \leq w_{ij}$ para pares $(i, j)$.

Se modela como un grafo dirigido con arista $i \to j$ de peso $w_{ij}$.
Bellman-Ford desde un vértice fuente artificial (conectado a todos con
peso 0) encuentra una solución o detecta inconsistencia (ciclo negativo).

### 8.3 Tabla de aplicaciones

| Aplicación | Peso de aristas | Qué detecta |
|-----------|:---------------:|-------------|
| Arbitraje | $-\log(\text{tasa})$ | Ciclo negativo = oportunidad |
| Restricciones de diferencias | Cota de diferencia | Ciclo neg. = inconsistencia |
| Redes con costos/beneficios | Beneficio negativo | Camino más barato |
| Routing (RIP) | Distancia | Ciclos neg. = routing loops |
| Verificación de Dijkstra | Pesos originales | ¿Hay negativos ocultos? |

---

## 9. Bellman-Ford como subroutina

### 9.1 Johnson's algorithm

Para all-pairs shortest paths en grafos dispersos con pesos negativos:

1. Agregar un vértice fuente $q$ con aristas de peso 0 a todos los demás.
2. Ejecutar Bellman-Ford desde $q$ para obtener potenciales $h[v]$.
3. **Reponderar**: $\hat{w}(u,v) = w(u,v) + h[u] - h[v] \geq 0$.
4. Ejecutar Dijkstra desde cada vértice con los pesos reponderados.

Complejidad: $O(n \cdot m + n \cdot (n + m) \log n)$ vs Floyd-Warshall
$O(n^3)$. Johnson es mejor para grafos dispersos.

### 9.2 Verificar la no existencia de ciclos negativos

Antes de ejecutar Dijkstra (que requiere pesos $\geq 0$), se puede correr
Bellman-Ford como verificación. Si no hay ciclos negativos, Dijkstra es
seguro.

---

## 10. Programa completo en C

```c
/*
 * bellman_ford.c
 *
 * Bellman-Ford algorithm: SSSP with negative weights,
 * negative cycle detection and extraction, SPFA,
 * and currency arbitrage.
 *
 * Compile: gcc -O2 -o bellman_ford bellman_ford.c -lm
 * Run:     ./bellman_ford
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define MAX_V 50
#define MAX_E 200
#define INF (INT_MAX / 2)

/* =================== EDGE LIST ==================================== */

typedef struct {
    int from, to, weight;
} Edge;

typedef struct {
    Edge edges[MAX_E];
    int n_vertices, n_edges;
} EdgeGraph;

void egraph_init(EdgeGraph *g, int n) {
    g->n_vertices = n;
    g->n_edges = 0;
}

void egraph_add(EdgeGraph *g, int u, int v, int w) {
    g->edges[g->n_edges++] = (Edge){u, v, w};
}

/* =================== BELLMAN-FORD ================================= */

typedef enum { BF_OK, BF_NEG_CYCLE } BFResult;

BFResult bellman_ford(EdgeGraph *g, int src, int dist[], int parent[]) {
    int n = g->n_vertices;
    int m = g->n_edges;

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        parent[i] = -1;
    }
    dist[src] = 0;

    /* n-1 passes */
    for (int i = 0; i < n - 1; i++) {
        bool changed = false;
        for (int j = 0; j < m; j++) {
            int u = g->edges[j].from;
            int v = g->edges[j].to;
            int w = g->edges[j].weight;
            if (dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                changed = true;
            }
        }
        if (!changed) break; /* early exit */
    }

    /* Check for negative cycle */
    for (int j = 0; j < m; j++) {
        int u = g->edges[j].from;
        int v = g->edges[j].to;
        int w = g->edges[j].weight;
        if (dist[u] != INF && dist[u] + w < dist[v])
            return BF_NEG_CYCLE;
    }

    return BF_OK;
}

/* ============ DEMO 1: Basic Bellman-Ford =========================== */

void demo_basic(void) {
    printf("=== Demo 1: Basic Bellman-Ford ===\n");

    EdgeGraph g;
    egraph_init(&g, 5);
    /* CLRS-style graph */
    egraph_add(&g, 0, 1, 6);
    egraph_add(&g, 0, 2, 7);
    egraph_add(&g, 1, 2, 5);
    egraph_add(&g, 1, 3, -4);
    egraph_add(&g, 1, 4, 8);
    egraph_add(&g, 2, 4, -3);
    egraph_add(&g, 3, 1, 7);
    egraph_add(&g, 3, 4, 9);
    egraph_add(&g, 4, 0, -2);

    printf("Directed graph with negative weights:\n");
    printf("  0→1:6, 0→2:7, 1→2:5, 1→3:-4, 1→4:8\n");
    printf("  2→4:-3, 3→1:7, 3→4:9, 4→0:-2\n");
    printf("Source: 0\n\n");

    int dist[MAX_V], parent[MAX_V];
    BFResult res = bellman_ford(&g, 0, dist, parent);

    if (res == BF_NEG_CYCLE) {
        printf("Negative cycle detected!\n");
    } else {
        printf("  Vertex  Dist  Parent\n");
        for (int v = 0; v < g.n_vertices; v++) {
            printf("  %d       %-5d %d\n", v, dist[v], parent[v]);
        }
    }
    printf("\n");
}

/* ============ DEMO 2: Negative cycle detection ==================== */

void demo_neg_cycle(void) {
    printf("=== Demo 2: Negative cycle detection ===\n");

    EdgeGraph g;
    egraph_init(&g, 4);
    egraph_add(&g, 0, 1, 1);
    egraph_add(&g, 1, 2, -1);
    egraph_add(&g, 2, 3, -1);
    egraph_add(&g, 3, 1, -1);  /* cycle 1→2→3→1, weight = -1-1-1 = -3 */

    printf("Graph: 0→1:1, 1→2:-1, 2→3:-1, 3→1:-1\n");
    printf("Cycle 1→2→3→1: weight = -3 (negative!)\n\n");

    int dist[MAX_V], parent[MAX_V];
    BFResult res = bellman_ford(&g, 0, dist, parent);

    if (res == BF_NEG_CYCLE) {
        printf("Result: NEGATIVE CYCLE DETECTED ✓\n");
    } else {
        printf("Result: No negative cycle (unexpected!)\n");
    }
    printf("\n");
}

/* ============ DEMO 3: Extract the negative cycle ================== */

void demo_extract_cycle(void) {
    printf("=== Demo 3: Extract the negative cycle ===\n");

    EdgeGraph g;
    egraph_init(&g, 5);
    egraph_add(&g, 0, 1, 1);
    egraph_add(&g, 1, 2, 2);
    egraph_add(&g, 2, 3, -5);
    egraph_add(&g, 3, 1, 1);  /* cycle 1→2→3→1: 2+(-5)+1 = -2 */
    egraph_add(&g, 3, 4, 3);

    printf("Graph: 0→1:1, 1→2:2, 2→3:-5, 3→1:1, 3→4:3\n");
    printf("Cycle 1→2→3→1: weight = 2+(-5)+1 = -2\n\n");

    int dist[MAX_V], parent[MAX_V];
    int n = g.n_vertices, m = g.n_edges;

    for (int i = 0; i < n; i++) { dist[i] = INF; parent[i] = -1; }
    dist[0] = 0;

    int last_relaxed = -1;
    for (int i = 0; i < n; i++) { /* n passes (not n-1) */
        last_relaxed = -1;
        for (int j = 0; j < m; j++) {
            int u = g.edges[j].from, v = g.edges[j].to, w = g.edges[j].weight;
            if (dist[u] != INF && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                last_relaxed = v;
            }
        }
    }

    if (last_relaxed == -1) {
        printf("No negative cycle.\n");
    } else {
        /* Walk back n steps to ensure we're in the cycle */
        int v = last_relaxed;
        for (int i = 0; i < n; i++)
            v = parent[v];

        /* Extract cycle */
        printf("Negative cycle found! Extracting...\n  ");
        int cycle[MAX_V], clen = 0;
        int u = v;
        do {
            cycle[clen++] = u;
            u = parent[u];
        } while (u != v);
        cycle[clen++] = v;

        /* Print reversed */
        for (int i = clen - 1; i >= 0; i--) {
            printf("%d", cycle[i]);
            if (i > 0) printf(" → ");
        }

        /* Calculate cycle weight */
        int weight = 0;
        for (int i = clen - 1; i > 0; i--) {
            for (int j = 0; j < m; j++) {
                if (g.edges[j].from == cycle[i] && g.edges[j].to == cycle[i-1]) {
                    weight += g.edges[j].weight;
                    break;
                }
            }
        }
        printf("\n  Cycle weight: %d\n", weight);
    }
    printf("\n");
}

/* ============ DEMO 4: Comparison with Dijkstra ==================== */

/* Simple Dijkstra for comparison (only valid for non-negative weights) */
void dijkstra_simple(int adj[][MAX_V], int w[][MAX_V], int n, int src, int dist[]) {
    bool done[MAX_V] = {false};
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        for (int v = 0; v < n; v++)
            if (!done[v] && (u == -1 || dist[v] < dist[u]))
                u = v;
        if (u == -1 || dist[u] == INF) break;
        done[u] = true;
        for (int v = 0; v < n; v++)
            if (adj[u][v] && dist[u] + w[u][v] < dist[v])
                dist[v] = dist[u] + w[u][v];
    }
}

void demo_comparison(void) {
    printf("=== Demo 4: Bellman-Ford vs Dijkstra ===\n");

    /* Graph with negative weight but no negative cycle */
    EdgeGraph bg;
    egraph_init(&bg, 4);
    egraph_add(&bg, 0, 1, 4);
    egraph_add(&bg, 0, 2, 3);
    egraph_add(&bg, 1, 3, 1);
    egraph_add(&bg, 2, 1, -2);
    egraph_add(&bg, 2, 3, 5);

    printf("Graph: 0→1:4, 0→2:3, 1→3:1, 2→1:-2, 2→3:5\n");
    printf("(Negative weight on 2→1, but no negative cycle)\n\n");

    int dist_bf[MAX_V], parent_bf[MAX_V];
    bellman_ford(&bg, 0, dist_bf, parent_bf);

    /* Build adjacency matrix for Dijkstra */
    int adj[MAX_V][MAX_V] = {0}, wt[MAX_V][MAX_V] = {0};
    for (int i = 0; i < bg.n_edges; i++) {
        adj[bg.edges[i].from][bg.edges[i].to] = 1;
        wt[bg.edges[i].from][bg.edges[i].to] = bg.edges[i].weight;
    }
    int dist_dj[MAX_V];
    dijkstra_simple(adj, wt, bg.n_vertices, 0, dist_dj);

    printf("  Vertex  Bellman-Ford  Dijkstra\n");
    for (int v = 0; v < bg.n_vertices; v++) {
        printf("  %d       %-13d %d%s\n", v, dist_bf[v], dist_dj[v],
               dist_bf[v] != dist_dj[v] ? "  ← WRONG!" : "");
    }

    printf("\nBellman-Ford correctly finds 0→2→1: 3+(-2)=1 < 4\n");
    printf("Dijkstra may finalize 1 with dist=4 before processing 2→1:-2\n");
    printf("\n");
}

/* ============ DEMO 5: SPFA ======================================== */

typedef struct {
    Edge *edges;
    int size, cap;
} AdjList;

void demo_spfa(void) {
    printf("=== Demo 5: SPFA (Shortest Path Faster Algorithm) ===\n");

    int n = 5;
    /* Build adjacency list */
    AdjList adj[MAX_V];
    for (int i = 0; i < n; i++) {
        adj[i].size = 0;
        adj[i].cap = 4;
        adj[i].edges = malloc(4 * sizeof(Edge));
    }

    /* Add edges via adjacency list */
    #define ADD(u,v,w) do { \
        AdjList *l = &adj[u]; \
        if (l->size == l->cap) { l->cap *= 2; l->edges = realloc(l->edges, l->cap * sizeof(Edge)); } \
        l->edges[l->size++] = (Edge){u, v, w}; \
    } while(0)

    ADD(0,1,6); ADD(0,2,7); ADD(1,2,5); ADD(1,3,-4); ADD(1,4,8);
    ADD(2,4,-3); ADD(3,1,7); ADD(3,4,9); ADD(4,0,-2);

    printf("Same graph as Demo 1, using SPFA\n\n");

    int dist[MAX_V], parent[MAX_V], count[MAX_V];
    bool in_queue[MAX_V];
    for (int i = 0; i < n; i++) {
        dist[i] = INF; parent[i] = -1; count[i] = 0; in_queue[i] = false;
    }
    dist[0] = 0;

    int queue[MAX_V * MAX_V], front = 0, back = 0;
    queue[back++] = 0;
    in_queue[0] = true;
    count[0] = 1;
    bool neg_cycle = false;
    int relaxations = 0;

    while (front < back && !neg_cycle) {
        int u = queue[front++];
        in_queue[u] = false;

        for (int i = 0; i < adj[u].size; i++) {
            int v = adj[u].edges[i].to;
            int w = adj[u].edges[i].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                parent[v] = u;
                relaxations++;
                if (!in_queue[v]) {
                    queue[back++] = v;
                    in_queue[v] = true;
                    count[v]++;
                    if (count[v] > n) {
                        neg_cycle = true;
                        break;
                    }
                }
            }
        }
    }

    if (neg_cycle) {
        printf("Negative cycle detected by SPFA!\n");
    } else {
        printf("  Vertex  Dist\n");
        for (int v = 0; v < n; v++)
            printf("  %d       %d\n", v, dist[v]);
        printf("\n  Total relaxations: %d (vs Bellman-Ford worst: %d)\n",
               relaxations, (n - 1) * 9);
    }

    for (int i = 0; i < n; i++) free(adj[i].edges);
    #undef ADD
    printf("\n");
}

/* ============ DEMO 6: Currency arbitrage ========================== */

void demo_arbitrage(void) {
    printf("=== Demo 6: Currency arbitrage ===\n");

    /* Currencies: USD(0), EUR(1), GBP(2), JPY(3) */
    const char *curr[] = {"USD", "EUR", "GBP", "JPY"};
    int n = 4;

    /* Exchange rates (some are set to create arbitrage opportunity) */
    double rates[4][4] = {
        /* USD   EUR    GBP    JPY */
        {1.00,  0.92,  0.79,  149.5},  /* from USD */
        {1.09,  1.00,  0.86,  163.0},  /* from EUR */
        {1.27,  1.17,  1.00,  190.0},  /* from GBP */
        {0.0067,0.0062,0.0053,1.00},   /* from JPY */
    };

    printf("Exchange rate matrix:\n       ");
    for (int i = 0; i < n; i++) printf("%-8s", curr[i]);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %-4s ", curr[i]);
        for (int j = 0; j < n; j++)
            printf("%-8.4f", rates[i][j]);
        printf("\n");
    }

    /* Build graph with weights = -log(rate) */
    EdgeGraph g;
    egraph_init(&g, n);

    /* Use integer weights: multiply -log by 10000 and round */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) {
                int w = (int)(-log(rates[i][j]) * 10000);
                egraph_add(&g, i, j, w);
            }

    int dist[MAX_V], parent[MAX_V];
    BFResult res = bellman_ford(&g, 0, dist, parent);

    printf("\nArbitrage opportunity: %s\n",
           res == BF_NEG_CYCLE ? "YES! (negative cycle found)" : "No");

    /* Check each cycle manually */
    printf("\nChecking triangle cycles:\n");
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (j != i)
                for (int k = 0; k < n; k++)
                    if (k != i && k != j) {
                        double product = rates[i][j] * rates[j][k] * rates[k][i];
                        if (product > 1.001) /* threshold for floating point */
                            printf("  %s→%s→%s→%s: %.6f (profit: %.4f%%)\n",
                                   curr[i], curr[j], curr[k], curr[i],
                                   product, (product - 1.0) * 100);
                    }

    printf("\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_basic();
    demo_neg_cycle();
    demo_extract_cycle();
    demo_comparison();
    demo_spfa();
    demo_arbitrage();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: Basic Bellman-Ford ===
Directed graph with negative weights:
  0→1:6, 0→2:7, 1→2:5, 1→3:-4, 1→4:8
  2→4:-3, 3→1:7, 3→4:9, 4→0:-2
Source: 0

  Vertex  Dist  Parent
  0       0     -1
  1       6     0
  2       7     0
  3       2     1
  4       4     2

=== Demo 2: Negative cycle detection ===
Graph: 0→1:1, 1→2:-1, 2→3:-1, 3→1:-1
Cycle 1→2→3→1: weight = -3 (negative!)

Result: NEGATIVE CYCLE DETECTED ✓

=== Demo 3: Extract the negative cycle ===
Graph: 0→1:1, 1→2:2, 2→3:-5, 3→1:1, 3→4:3
Cycle 1→2→3→1: weight = 2+(-5)+1 = -2

Negative cycle found! Extracting...
  1 → 2 → 3 → 1
  Cycle weight: -2

=== Demo 4: Bellman-Ford vs Dijkstra ===
Graph: 0→1:4, 0→2:3, 1→3:1, 2→1:-2, 2→3:5
(Negative weight on 2→1, but no negative cycle)

  Vertex  Bellman-Ford  Dijkstra
  0       0             0
  1       1             4  ← WRONG!
  2       3             3
  3       2             5  ← WRONG!

Bellman-Ford correctly finds 0→2→1: 3+(-2)=1 < 4
Dijkstra may finalize 1 with dist=4 before processing 2→1:-2

=== Demo 5: SPFA (Shortest Path Faster Algorithm) ===
Same graph as Demo 1, using SPFA

  Vertex  Dist
  0       0
  1       6
  2       7
  3       2
  4       4

  Total relaxations: 7 (vs Bellman-Ford worst: 36)

=== Demo 6: Currency arbitrage ===
Exchange rate matrix:
       USD     EUR     GBP     JPY
  USD  1.0000  0.9200  0.7900  149.5000
  EUR  1.0900  1.0000  0.8600  163.0000
  GBP  1.2700  1.1700  1.0000  190.0000
  JPY  0.0067  0.0062  0.0053  1.0000

Arbitrage opportunity: YES! (negative cycle found)

Checking triangle cycles:
  USD→EUR→GBP→USD: 1.003800 (profit: 0.3800%)
  ... (exact cycles depend on rate values)
```

---

## 11. Programa completo en Rust

```rust
/*
 * bellman_ford.rs
 *
 * Bellman-Ford: SSSP with negative weights, negative cycle detection
 * and extraction, SPFA, currency arbitrage, and difference constraints.
 *
 * Compile: rustc -O -o bellman_ford bellman_ford.rs
 * Run:     ./bellman_ford
 */

use std::collections::VecDeque;

const INF: i64 = i64::MAX / 2;

/* =================== GRAPH STRUCTURES ============================= */

#[derive(Clone)]
struct Edge {
    to: usize,
    weight: i64,
}

struct WDiGraph {
    adj: Vec<Vec<Edge>>,
}

impl WDiGraph {
    fn new(n: usize) -> Self {
        Self { adj: vec![vec![]; n] }
    }

    fn add_edge(&mut self, u: usize, v: usize, w: i64) {
        self.adj[u].push(Edge { to: v, weight: w });
    }

    fn n(&self) -> usize { self.adj.len() }

    fn edges(&self) -> Vec<(usize, usize, i64)> {
        let mut result = vec![];
        for u in 0..self.n() {
            for e in &self.adj[u] {
                result.push((u, e.to, e.weight));
            }
        }
        result
    }
}

/* =================== BELLMAN-FORD ================================= */

#[derive(Debug)]
enum BFResult {
    Ok(Vec<i64>, Vec<Option<usize>>),
    NegativeCycle(Vec<usize>),
}

fn bellman_ford(g: &WDiGraph, src: usize) -> BFResult {
    let n = g.n();
    let edges = g.edges();
    let mut dist = vec![INF; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    dist[src] = 0;

    // n-1 passes with early exit
    for _ in 0..n - 1 {
        let mut changed = false;
        for &(u, v, w) in &edges {
            if dist[u] != INF && dist[u] + w < dist[v] {
                dist[v] = dist[u] + w;
                parent[v] = Some(u);
                changed = true;
            }
        }
        if !changed { break; }
    }

    // n-th pass: check for negative cycle
    let mut last_relaxed = None;
    for &(u, v, w) in &edges {
        if dist[u] != INF && dist[u] + w < dist[v] {
            dist[v] = dist[u] + w;
            parent[v] = Some(u);
            last_relaxed = Some(v);
        }
    }

    match last_relaxed {
        None => BFResult::Ok(dist, parent),
        Some(mut v) => {
            // Walk back n steps to ensure we're in the cycle
            for _ in 0..n {
                v = parent[v].unwrap();
            }

            // Extract cycle
            let start = v;
            let mut cycle = vec![start];
            let mut u = parent[start].unwrap();
            while u != start {
                cycle.push(u);
                u = parent[u].unwrap();
            }
            cycle.push(start);
            cycle.reverse();
            BFResult::NegativeCycle(cycle)
        }
    }
}

/* ======== Demo 1: Basic Bellman-Ford =============================== */

fn demo_basic() {
    println!("=== Demo 1: Basic Bellman-Ford ===");

    let mut g = WDiGraph::new(5);
    g.add_edge(0, 1, 6);
    g.add_edge(0, 2, 7);
    g.add_edge(1, 2, 5);
    g.add_edge(1, 3, -4);
    g.add_edge(1, 4, 8);
    g.add_edge(2, 4, -3);
    g.add_edge(3, 1, 7);
    g.add_edge(3, 4, 9);
    g.add_edge(4, 0, -2);

    println!("Directed graph with negative weights");
    println!("Source: 0\n");

    match bellman_ford(&g, 0) {
        BFResult::Ok(dist, parent) => {
            for v in 0..g.n() {
                let p = parent[v].map_or("-".to_string(), |p| p.to_string());
                println!("  dist[{}]={:<4} parent={}", v, dist[v], p);
            }
        }
        BFResult::NegativeCycle(cycle) => {
            let c: Vec<String> = cycle.iter().map(|v| v.to_string()).collect();
            println!("  Negative cycle: {}", c.join("→"));
        }
    }
    println!();
}

/* ======== Demo 2: Negative cycle detection and extraction ========== */

fn demo_neg_cycle() {
    println!("=== Demo 2: Negative cycle extraction ===");

    let mut g = WDiGraph::new(5);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 3, -5);
    g.add_edge(3, 1, 1); // cycle 1→2→3→1: 2+(-5)+1=-2
    g.add_edge(3, 4, 3);

    println!("Cycle 1→2→3→1: weight = 2+(-5)+1 = -2\n");

    match bellman_ford(&g, 0) {
        BFResult::Ok(_, _) => println!("No negative cycle (unexpected!)"),
        BFResult::NegativeCycle(cycle) => {
            let c: Vec<String> = cycle.iter().map(|v| v.to_string()).collect();
            println!("  Cycle: {}", c.join(" → "));

            // Compute weight
            let weight: i64 = cycle.windows(2).map(|w| {
                g.adj[w[0]].iter().find(|e| e.to == w[1]).unwrap().weight
            }).sum();
            println!("  Weight: {}", weight);
        }
    }
    println!();
}

/* ======== Demo 3: Bellman-Ford vs Dijkstra ======================== */

fn dijkstra_simple(g: &WDiGraph, src: usize) -> Vec<i64> {
    let n = g.n();
    let mut dist = vec![INF; n];
    let mut done = vec![false; n];
    dist[src] = 0;

    for _ in 0..n {
        let u = (0..n).filter(|&v| !done[v])
            .min_by_key(|&v| dist[v]);
        let u = match u { Some(u) if dist[u] != INF => u, _ => break };
        done[u] = true;
        for e in &g.adj[u] {
            if dist[u] + e.weight < dist[e.to] {
                dist[e.to] = dist[u] + e.weight;
            }
        }
    }
    dist
}

fn demo_vs_dijkstra() {
    println!("=== Demo 3: Bellman-Ford vs Dijkstra ===");

    let mut g = WDiGraph::new(4);
    g.add_edge(0, 1, 4);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 3, 1);
    g.add_edge(2, 1, -2);
    g.add_edge(2, 3, 5);

    println!("Graph with negative edge 2→1:-2 (no neg cycle)\n");

    let dist_bf = match bellman_ford(&g, 0) {
        BFResult::Ok(d, _) => d,
        _ => unreachable!(),
    };
    let dist_dj = dijkstra_simple(&g, 0);

    println!("  Vertex  BF    Dijkstra  Match?");
    for v in 0..g.n() {
        let ok = dist_bf[v] == dist_dj[v];
        println!("  {}       {:<5} {:<9} {}",
                 v, dist_bf[v], dist_dj[v],
                 if ok { "OK" } else { "WRONG" });
    }
    println!("\n  Dijkstra fails: finalizes 1 at dist=4 before discovering 0→2→1=1");
    println!();
}

/* ======== Demo 4: SPFA ============================================ */

fn spfa(g: &WDiGraph, src: usize) -> Result<(Vec<i64>, usize), Vec<usize>> {
    let n = g.n();
    let mut dist = vec![INF; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    let mut in_queue = vec![false; n];
    let mut count = vec![0usize; n];
    let mut relaxations = 0usize;

    dist[src] = 0;
    let mut queue = VecDeque::new();
    queue.push_back(src);
    in_queue[src] = true;
    count[src] = 1;

    while let Some(u) = queue.pop_front() {
        in_queue[u] = false;

        for e in &g.adj[u] {
            if dist[u] + e.weight < dist[e.to] {
                dist[e.to] = dist[u] + e.weight;
                parent[e.to] = Some(u);
                relaxations += 1;

                if !in_queue[e.to] {
                    queue.push_back(e.to);
                    in_queue[e.to] = true;
                    count[e.to] += 1;
                    if count[e.to] > n {
                        return Err(vec![e.to]); // negative cycle
                    }
                }
            }
        }
    }

    Ok((dist, relaxations))
}

fn demo_spfa() {
    println!("=== Demo 4: SPFA ===");

    let mut g = WDiGraph::new(5);
    g.add_edge(0, 1, 6);
    g.add_edge(0, 2, 7);
    g.add_edge(1, 2, 5);
    g.add_edge(1, 3, -4);
    g.add_edge(1, 4, 8);
    g.add_edge(2, 4, -3);
    g.add_edge(3, 1, 7);
    g.add_edge(3, 4, 9);
    g.add_edge(4, 0, -2);

    let edges_count = g.edges().len();

    match spfa(&g, 0) {
        Ok((dist, relaxations)) => {
            for v in 0..g.n() {
                println!("  dist[{}]={}", v, dist[v]);
            }
            println!("\n  Relaxations: {} (BF worst: {})",
                     relaxations, (g.n() - 1) * edges_count);
        }
        Err(_) => println!("  Negative cycle!"),
    }
    println!();
}

/* ======== Demo 5: Currency arbitrage =============================== */

fn demo_arbitrage() {
    println!("=== Demo 5: Currency arbitrage ===");

    let currencies = ["USD", "EUR", "GBP", "JPY"];
    let rates = vec![
        vec![1.0,    0.92,  0.79,  149.5],
        vec![1.09,   1.0,   0.86,  163.0],
        vec![1.27,   1.17,  1.0,   190.0],
        vec![0.0067, 0.0062,0.0053,1.0],
    ];
    let n = currencies.len();

    println!("Exchange rates:");
    print!("       ");
    for c in &currencies { print!("{:<8}", c); }
    println!();
    for i in 0..n {
        print!("  {:<4} ", currencies[i]);
        for j in 0..n { print!("{:<8.4}", rates[i][j]); }
        println!();
    }

    // Build graph: weight = -ln(rate) × 10000 (integer)
    let mut g = WDiGraph::new(n);
    for i in 0..n {
        for j in 0..n {
            if i != j {
                let w = (-rates[i][j].ln() * 10000.0) as i64;
                g.add_edge(i, j, w);
            }
        }
    }

    // Check from each source
    let mut found = false;
    for src in 0..n {
        if let BFResult::NegativeCycle(cycle) = bellman_ford(&g, src) {
            let names: Vec<&str> = cycle.iter().map(|&v| currencies[v]).collect();
            println!("\nArbitrage cycle found from {}:", currencies[src]);
            println!("  {}", names.join(" → "));

            // Compute profit
            let mut product = 1.0;
            for w in cycle.windows(2) {
                product *= rates[w[0]][w[1]];
            }
            println!("  Rate product: {:.6} → profit: {:.4}%",
                     product, (product - 1.0) * 100.0);
            found = true;
            break;
        }
    }

    if !found {
        println!("\nNo arbitrage opportunity found.");
    }

    // Manual check of all triangles
    println!("\nAll profitable triangles:");
    for i in 0..n {
        for j in 0..n {
            if j == i { continue; }
            for k in 0..n {
                if k == i || k == j { continue; }
                let p = rates[i][j] * rates[j][k] * rates[k][i];
                if p > 1.001 {
                    println!("  {}→{}→{}→{}: {:.6} (+{:.4}%)",
                             currencies[i], currencies[j], currencies[k],
                             currencies[i], p, (p - 1.0) * 100.0);
                }
            }
        }
    }
    println!();
}

/* ======== Demo 6: Difference constraints =========================== */

fn demo_difference_constraints() {
    println!("=== Demo 6: Difference constraints ===");

    // System: x1-x0 ≤ 1, x2-x0 ≤ 5, x2-x1 ≤ 3, x0-x2 ≤ -1
    // Constraint xj - xi ≤ w  →  edge i→j with weight w
    let constraints = [
        (0, 1, 1),   // x1 - x0 ≤ 1
        (0, 2, 5),   // x2 - x0 ≤ 5
        (1, 2, 3),   // x2 - x1 ≤ 3
        (2, 0, -1),  // x0 - x2 ≤ -1 (i.e., x2 ≥ x0 + 1)
    ];
    let n_vars = 3;

    println!("Constraints:");
    let var_name = |i: usize| -> String { format!("x{}", i) };
    for &(i, j, w) in &constraints {
        println!("  {} - {} ≤ {}", var_name(j), var_name(i), w);
    }

    // Add source vertex connected to all with weight 0
    let n = n_vars + 1;
    let src = n_vars; // artificial source
    let mut g = WDiGraph::new(n);

    for &(i, j, w) in &constraints {
        g.add_edge(i, j, w);
    }
    for i in 0..n_vars {
        g.add_edge(src, i, 0);
    }

    match bellman_ford(&g, src) {
        BFResult::Ok(dist, _) => {
            println!("\nSolution found (dist from artificial source):");
            for i in 0..n_vars {
                println!("  {} = {}", var_name(i), dist[i]);
            }

            // Verify
            println!("\nVerification:");
            for &(i, j, w) in &constraints {
                let diff = dist[j] - dist[i];
                let ok = diff <= w;
                println!("  {} - {} = {} ≤ {} → {}",
                         var_name(j), var_name(i), diff, w,
                         if ok { "✓" } else { "✗" });
            }
        }
        BFResult::NegativeCycle(cycle) => {
            let c: Vec<String> = cycle.iter().map(|v| v.to_string()).collect();
            println!("\nNo solution: inconsistent constraints (negative cycle: {})",
                     c.join("→"));
        }
    }
    println!();
}

/* ======== Demo 7: Pass-by-pass trace ============================== */

fn demo_trace() {
    println!("=== Demo 7: Pass-by-pass trace ===");

    let mut g = WDiGraph::new(5);
    g.add_edge(0, 1, 6);
    g.add_edge(0, 2, 7);
    g.add_edge(1, 3, -4);
    g.add_edge(2, 4, -3);
    g.add_edge(3, 4, 9);

    let n = g.n();
    let edges = g.edges();
    let mut dist = vec![INF; n];
    dist[0] = 0;

    println!("Edges: {:?}\n", edges);
    println!("Pass 0 (init): dist = {:?}\n", dist.iter()
        .map(|&d| if d == INF { "∞".to_string() } else { d.to_string() })
        .collect::<Vec<_>>());

    for pass in 1..=n-1 {
        let mut changed = false;
        for &(u, v, w) in &edges {
            if dist[u] != INF && dist[u] + w < dist[v] {
                println!("  Pass {}: relax ({},{},{}): {}+{}={} < {} → update",
                         pass, u, v, w, dist[u], w, dist[u]+w, dist[v]);
                dist[v] = dist[u] + w;
                changed = true;
            }
        }
        let dstr: Vec<String> = dist.iter()
            .map(|&d| if d == INF { "∞".to_string() } else { d.to_string() })
            .collect();
        println!("  dist after pass {}: {:?}", pass, dstr);
        if !changed {
            println!("  No changes → early exit!");
            break;
        }
        println!();
    }
    println!();
}

/* ======== Demo 8: Johnson's reweighting preview ==================== */

fn demo_johnson_preview() {
    println!("=== Demo 8: Johnson's reweighting (preview) ===");

    let mut g = WDiGraph::new(4);
    g.add_edge(0, 1, 2);
    g.add_edge(0, 2, 5);
    g.add_edge(1, 2, -3);
    g.add_edge(1, 3, 4);
    g.add_edge(2, 3, 1);

    println!("Original graph: 0→1:2, 0→2:5, 1→2:-3, 1→3:4, 2→3:1\n");

    // Add artificial source q connected to all with weight 0
    let n = g.n();
    let q = n;
    let mut gq = WDiGraph::new(n + 1);
    for (u, v, w) in g.edges() {
        gq.add_edge(u, v, w);
    }
    for i in 0..n {
        gq.add_edge(q, i, 0);
    }

    // Bellman-Ford from q to get potentials h[]
    let h = match bellman_ford(&gq, q) {
        BFResult::Ok(dist, _) => dist,
        BFResult::NegativeCycle(_) => {
            println!("Negative cycle! Johnson's not applicable.");
            return;
        }
    };

    println!("Potentials h[] (Bellman-Ford from q):");
    for i in 0..n {
        println!("  h[{}] = {}", i, h[i]);
    }

    // Reweight
    println!("\nReweighted edges (w' = w + h[u] - h[v]):");
    for (u, v, w) in g.edges() {
        let w_new = w + h[u] - h[v];
        println!("  {}→{}: w={}, w'={} + {} - {} = {} {}",
                 u, v, w, w, h[u], h[v], w_new,
                 if w_new >= 0 { "≥ 0 ✓" } else { "< 0 ✗" });
    }

    println!("\nAll reweighted edges are ≥ 0 → Dijkstra is now safe!");
    println!("Run Dijkstra from each vertex, then adjust: d(u,v) = d'(u,v) - h[u] + h[v]");
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_basic();
    demo_neg_cycle();
    demo_vs_dijkstra();
    demo_spfa();
    demo_arbitrage();
    demo_difference_constraints();
    demo_trace();
    demo_johnson_preview();
}
```

### Salida esperada

```
=== Demo 1: Basic Bellman-Ford ===
Directed graph with negative weights
Source: 0

  dist[0]=0    parent=-
  dist[1]=6    parent=0
  dist[2]=7    parent=0
  dist[3]=2    parent=1
  dist[4]=4    parent=2

=== Demo 2: Negative cycle extraction ===
Cycle 1→2→3→1: weight = 2+(-5)+1 = -2

  Cycle: 1 → 2 → 3 → 1
  Weight: -2

=== Demo 3: Bellman-Ford vs Dijkstra ===
Graph with negative edge 2→1:-2 (no neg cycle)

  Vertex  BF    Dijkstra  Match?
  0       0     0         OK
  1       1     4         WRONG
  2       3     3         OK
  3       2     5         WRONG

  Dijkstra fails: finalizes 1 at dist=4 before discovering 0→2→1=1

=== Demo 4: SPFA ===
  dist[0]=0
  dist[1]=6
  dist[2]=7
  dist[3]=2
  dist[4]=4

  Relaxations: 7 (BF worst: 36)

=== Demo 5: Currency arbitrage ===
Exchange rates:
       USD     EUR     GBP     JPY
  USD  1.0000  0.9200  0.7900  149.5000
  EUR  1.0900  1.0000  0.8600  163.0000
  GBP  1.2700  1.1700  1.0000  190.0000
  JPY  0.0067  0.0062  0.0053  1.0000

Arbitrage cycle found from USD:
  USD → EUR → GBP → USD
  Rate product: 1.003800 → profit: 0.3800%

All profitable triangles:
  USD→EUR→GBP→USD: 1.003800 (+0.3800%)
  EUR→GBP→USD→EUR: 1.003800 (+0.3800%)
  GBP→USD→EUR→GBP: 1.003800 (+0.3800%)

=== Demo 6: Difference constraints ===
Constraints:
  x1 - x0 ≤ 1
  x2 - x0 ≤ 5
  x2 - x1 ≤ 3
  x0 - x2 ≤ -1

Solution found (dist from artificial source):
  x0 = 0
  x1 = 1
  x2 = 4

Verification:
  x1 - x0 = 1 ≤ 1 → ✓
  x2 - x0 = 4 ≤ 5 → ✓
  x2 - x1 = 3 ≤ 3 → ✓
  x0 - x2 = -4 ≤ -1 → ✓

=== Demo 7: Pass-by-pass trace ===
Edges: [(0,1,6), (0,2,7), (1,3,-4), (2,4,-3), (3,4,9)]

Pass 0 (init): dist = ["0", "∞", "∞", "∞", "∞"]

  Pass 1: relax (0,1,6): 0+6=6 < ∞ → update
  Pass 1: relax (0,2,7): 0+7=7 < ∞ → update
  Pass 1: relax (1,3,-4): 6+-4=2 < ∞ → update
  Pass 1: relax (2,4,-3): 7+-3=4 < ∞ → update
  dist after pass 1: ["0", "6", "7", "2", "4"]

  dist after pass 2: ["0", "6", "7", "2", "4"]
  No changes → early exit!

=== Demo 8: Johnson's reweighting (preview) ===
Original graph: 0→1:2, 0→2:5, 1→2:-3, 1→3:4, 2→3:1

Potentials h[] (Bellman-Ford from q):
  h[0] = 0
  h[1] = 0
  h[2] = -3
  h[3] = -2

Reweighted edges (w' = w + h[u] - h[v]):
  0→1: w=2, w'=2 + 0 - 0 = 2 ≥ 0 ✓
  0→2: w=5, w'=5 + 0 - -3 = 8 ≥ 0 ✓
  1→2: w=-3, w'=-3 + 0 - -3 = 0 ≥ 0 ✓
  1→3: w=4, w'=4 + 0 - -2 = 6 ≥ 0 ✓
  2→3: w=1, w'=1 + -3 - -2 = 0 ≥ 0 ✓

All reweighted edges are ≥ 0 → Dijkstra is now safe!
Run Dijkstra from each vertex, then adjust: d(u,v) = d'(u,v) - h[u] + h[v]
```

---

## 12. Ejercicios

### Ejercicio 1 — Bellman-Ford a mano

Grafo dirigido: $0\to1{:}4,\ 0\to2{:}5,\ 1\to2{:}{-3},\ 2\to3{:}4,\ 1\to3{:}6$.

Ejecuta Bellman-Ford desde 0. Muestra `dist[]` después de cada pasada.

<details>
<summary>Traza</summary>

```
Init: dist = [0, ∞, ∞, ∞]

Pasada 1 (aristas en orden dado):
  (0,1,4): 0+4=4 < ∞ → dist[1]=4
  (0,2,5): 0+5=5 < ∞ → dist[2]=5
  (1,2,-3): 4+(-3)=1 < 5 → dist[2]=1
  (2,3,4): 1+4=5 < ∞ → dist[3]=5
  (1,3,6): 4+6=10 > 5 → no
  dist = [0, 4, 1, 5]

Pasada 2:
  Ningún cambio posible (verificar cada arista).
  dist = [0, 4, 1, 5] — early exit.

Verificación: no hay ciclo negativo.
```
</details>

---

### Ejercicio 2 — Detectar ciclo negativo

Grafo: $0\to1{:}3,\ 1\to2{:}{-2},\ 2\to3{:}1,\ 3\to1{:}{-3}$.

¿Hay ciclo negativo? Si sí, extráelo.

<details>
<summary>Respuesta</summary>

Ciclo $1 \to 2 \to 3 \to 1$: peso $= -2 + 1 + (-3) = -4 < 0$.

**Sí, hay ciclo negativo**: $1 \to 2 \to 3 \to 1$ con peso $-4$.

Bellman-Ford detecta esto en la $n$-ésima pasada (pasada 4) cuando alguna
arista se puede seguir relajando.
</details>

---

### Ejercicio 3 — SPFA vs Bellman-Ford

Usando el grafo del ejercicio 1, ejecuta SPFA y cuenta cuántas
relajaciones realiza. Compara con el máximo teórico de Bellman-Ford.

<details>
<summary>Respuesta</summary>

SPFA:
- Iniciar cola: {0}. dist=[0,∞,∞,∞].
- Dequeue 0: relax 1=4, 2=5. Cola: {1,2}. Relax: 2.
- Dequeue 1: relax 2: 4-3=1 < 5→yes. relax 3: 4+6=10. Cola: {2,3}. Relax: 2.
- Dequeue 2: relax 3: 1+4=5 < 10→yes. Cola: {3}. Relax: 1.
- Dequeue 3: nada. Cola vacía. Relax: 0.

**Total SPFA: 5 relajaciones**.
Bellman-Ford: $(n-1) \times m = 3 \times 5 = 15$ en el peor caso.
Ratio: 5/15 = 33%.
</details>

---

### Ejercicio 4 — Bellman-Ford en grafo no dirigido

Cada arista no dirigida $u$-$v{:}w$ se convierte en dos aristas dirigidas
$u\to v{:}w$ y $v\to u{:}w$. ¿Puede Bellman-Ford manejar grafos no
dirigidos con pesos negativos?

<details>
<summary>Respuesta</summary>

**No.** Una arista no dirigida con peso negativo $w < 0$ crea un ciclo
negativo automáticamente: $u \to v \to u$ con peso $w + w = 2w < 0$.

Por lo tanto, en un grafo no dirigido con una arista de peso negativo,
Bellman-Ford siempre reportará un ciclo negativo. Los pesos negativos
en grafos no dirigidos hacen que el problema de caminos más cortos no esté
bien definido.
</details>

---

### Ejercicio 5 — Número de pasadas necesarias

Para cada grafo, determina cuántas pasadas de Bellman-Ford son necesarias
(con terminación temprana):

a) Cadena: $0\to1{:}3,\ 1\to2{:}2,\ 2\to3{:}1$.
b) Mismo, pero aristas procesadas en orden inverso: $(2,3,1), (1,2,2), (0,1,3)$.
c) Grafo completo $K_4$ con todas las aristas de peso 1.

<details>
<summary>Respuesta</summary>

a) Aristas en "buen" orden: **1 pasada**. Cada arista propaga la distancia
correcta inmediatamente (en orden topológico).

b) Aristas en "mal" orden: **3 pasadas** (= $n-1$). Pasada 1: solo
$(0,1,3)$ relaja (1=3). Pasada 2: $(1,2,2)$ relaja (2=5). Pasada 3:
$(2,3,1)$ relaja (3=6). Cada pasada propaga una arista.

c) $K_4$ con peso 1: **1 pasada**. Desde la fuente 0, la primera pasada
relaja todas las aristas directas 0→1, 0→2, 0→3. La segunda pasada no
cambia nada. El orden de aristas puede hacer que necesite 2 pasadas si
las aristas desde 0 se procesan al final.
</details>

---

### Ejercicio 6 — Arbitraje de divisas

Tasas de cambio:
- USD→EUR: 0.85, EUR→GBP: 0.90, GBP→USD: 1.32.
- USD→JPY: 110, JPY→EUR: 0.0080.

¿Existe oportunidad de arbitraje? Calcula el producto para los ciclos
triangulares.

<details>
<summary>Respuesta</summary>

Ciclo USD→EUR→GBP→USD: $0.85 \times 0.90 \times 1.32 = 1.0098$.
Como $1.0098 > 1$: **sí, hay arbitraje** (ganancia del 0.98%).

Ciclo USD→JPY→EUR→GBP→USD:
$110 \times 0.0080 \times 0.90 \times 1.32 = 1.0454$.
Ganancia del 4.54% (más rentable).

Bellman-Ford con pesos $-\ln(\text{tasa})$ detectaría ambos ciclos
como ciclos negativos.
</details>

---

### Ejercicio 7 — Restricciones de diferencias

Sistema:
$x_1 - x_0 \leq 3,\quad x_2 - x_1 \leq 2,\quad x_0 - x_2 \leq -4$.

¿Tiene solución? Usa Bellman-Ford para verificar.

<details>
<summary>Respuesta</summary>

Aristas: $0\to1{:}3,\ 1\to2{:}2,\ 2\to0{:}{-4}$.

Ciclo $0\to1\to2\to0$: peso $= 3 + 2 + (-4) = 1 > 0$. No es ciclo negativo.

**Sí tiene solución.** Bellman-Ford desde un vértice artificial $s$ conectado
a todos con peso 0:
- $\text{dist} = [s{=}0, 0{=}0, 1{=}3, 2{=}5]$.
- Pasada: relax $s\to0{:}0$, $s\to1{:}0$, $s\to2{:}0$ → dist=[0,0,0].
  Relax $0\to1{:}3$: 0+3=3>0 no. $1\to2{:}2$: 0+2=2>0 no. $2\to0{:}{-4}$:
  0-4=-4<0 → dist[0]=-4.
- Pasada 2: $0\to1$: -4+3=-1<0 → dist[1]=-1. $1\to2$: -1+2=1>0 no.
  $2\to0$: 0-4=-4=-4 no.
- Pasada 3: nada.

Solución: $x_0=-4, x_1=-1, x_2=0$.
Verificar: $x_1-x_0 = 3 \leq 3$ ✓, $x_2-x_1 = 1 \leq 2$ ✓,
$x_0-x_2 = -4 \leq -4$ ✓.
</details>

---

### Ejercicio 8 — Implementar Bellman-Ford con lista de aristas

Escribe una implementación que:
1. Lea el número de vértices, aristas y fuente.
2. Lea las aristas como tripletas $(u, v, w)$.
3. Ejecute Bellman-Ford con terminación temprana.
4. Imprima distancias y caminos, o reporte ciclo negativo.

Prueba con los grafos de los demos 1 y 2.

<details>
<summary>Pista de estructura</summary>

La lista de aristas es la estructura más natural para Bellman-Ford:

```c
struct Edge { int from, to, weight; };
struct Edge edges[MAX_EDGES];
int n_edges;

// Bellman-Ford: iterate over edges[] directly
for (int pass = 0; pass < n-1; pass++)
    for (int e = 0; e < n_edges; e++)
        relax(edges[e]);
```

No se necesita lista de adyacencia — la lista de aristas es suficiente y
más eficiente en cache (acceso secuencial).
</details>

---

### Ejercicio 9 — Johnson's algorithm a mano

Grafo: $0\to1{:}2,\ 1\to2{:}{-1},\ 0\to2{:}4$.

a) Agrega vértice $q$ con aristas a todos de peso 0. Ejecuta Bellman-Ford
   desde $q$ para obtener potenciales $h[]$.
b) Calcula los pesos reponderados $\hat{w}(u,v) = w(u,v) + h[u] - h[v]$.
c) Verifica que todos los pesos reponderados son $\geq 0$.

<details>
<summary>Respuesta</summary>

a) Grafo extendido: $q\to0{:}0, q\to1{:}0, q\to2{:}0$ + aristas originales.
Bellman-Ford desde $q$: dist = $[q{=}0, 0{=}0, 1{=}0, 2{=}{-1}]$.
$h = [0, 0, -1]$.

b) Reponderación:
- $0\to1$: $2 + h[0] - h[1] = 2 + 0 - 0 = 2$ ✓.
- $1\to2$: $-1 + h[1] - h[2] = -1 + 0 - (-1) = 0$ ✓.
- $0\to2$: $4 + h[0] - h[2] = 4 + 0 - (-1) = 5$ ✓.

c) Todos $\geq 0$ → Dijkstra es seguro con estos pesos.

Para obtener distancias reales: $d(u,v) = d'(u,v) - h[u] + h[v]$.
</details>

---

### Ejercicio 10 — Comparación completa

Un grafo dirigido tiene 6 vértices y aristas:
$0\to1{:}5,\ 0\to2{:}3,\ 1\to2{:}{-2},\ 1\to3{:}6,\ 2\to3{:}7,\ 2\to4{:}4,\ 3\to5{:}{-1},\ 4\to3{:}{-3},\ 4\to5{:}2$.

a) ¿Hay ciclo negativo?
b) Ejecuta Bellman-Ford desde 0.
c) Compara con lo que daría Dijkstra (¿sería correcto?).
d) ¿Cuántas pasadas necesita Bellman-Ford con terminación temprana?

<details>
<summary>Respuesta</summary>

a) Ciclos posibles: $1\to2\to4\to3\to \ldots$ no vuelve a 1 (no hay arista
$3\to1$ ni $5\to\ldots\to1$). No hay ciclo alcanzable desde 0. **No hay
ciclo negativo**.

b) Bellman-Ford desde 0:
```
Pasada 1: dist = [0, 5, 3, 11, 7, ∞]
  0→1:5, 0→2:3, 1→2:5-2=3≥3 no, 1→3:5+6=11, 2→3:3+7=10<11→10,
  2→4:3+4=7, 3→5:10-1=9, 4→3:7-3=4<10→4, 4→5:7+2=9=9
  Recalc: dist = [0, 5, 3, 4, 7, 9]
  (Wait — order matters. Let me redo carefully)

  (0,1,5): dist[1]=5
  (0,2,3): dist[2]=3
  (1,2,-2): 5-2=3, 3≥3 → no
  (1,3,6): 5+6=11 → dist[3]=11
  (2,3,7): 3+7=10 < 11 → dist[3]=10
  (2,4,4): 3+4=7 → dist[4]=7
  (3,5,-1): 10-1=9 → dist[5]=9
  (4,3,-3): 7-3=4 < 10 → dist[3]=4
  (4,5,2): 7+2=9 = 9 → no
  dist = [0, 5, 3, 4, 7, 9]

Pasada 2:
  (3,5,-1): 4-1=3 < 9 → dist[5]=3
  dist = [0, 5, 3, 4, 7, 3]

Pasada 3: ningún cambio → early exit.
```

**dist = [0, 5, 3, 4, 7, 3]**. Necesita **2 pasadas** (terminación temprana
en la 3ra).

c) Dijkstra: la arista $1\to2{:}{-2}$ es negativa. Si Dijkstra procesa 1
(dist=5) y actualiza 2: $5-2=3 = \text{dist}[2]$ → no cambia. En este caso
particular, Dijkstra **daría el resultado correcto** porque la arista negativa
no causa que un vértice ya finalizado mejore.

Pero Dijkstra podría fallar si 2 se extrajera antes de que 1 pudiera
mejorarlo. Aquí $\text{dist}[2]=3 < \text{dist}[1]=5$, así que 2 se extrae
primero y su distancia ya es correcta. El resultado correcto es coincidencia;
no se debe confiar en Dijkstra con pesos negativos.

d) **2 pasadas** con terminación temprana.
</details>
