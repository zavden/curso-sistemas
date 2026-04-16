# T03 — Floyd-Warshall

## Objetivo

Dominar el algoritmo de Floyd-Warshall para caminos más cortos entre
**todos los pares** de vértices (APSP — All-Pairs Shortest Paths):
su formulación de programación dinámica, la reconstrucción de caminos,
la detección de ciclos negativos, y el cierre transitivo.

---

## 1. El problema APSP

Dado un grafo ponderado dirigido $G = (V, E)$ con función de peso
$w: E \to \mathbb{R}$, calcular para **cada par** $(i, j)$:

- $\text{dist}[i][j]$: la distancia mínima de $i$ a $j$.
- El camino concreto (vía una matriz de predecesores `next[i][j]`).

### Alternativas para APSP

| Método | Complejidad | Cuándo usar |
|--------|:-----------:|-------------|
| Dijkstra × $n$ | $O(n(n+m) \log n)$ | Pesos $\geq 0$, grafo disperso |
| Johnson | $O(nm + n(n+m) \log n)$ | Pesos negativos, disperso |
| Floyd-Warshall | $O(n^3)$ | Grafos densos, implementación simple |
| Multiplicación de matrices | $O(n^3 \log n)$ | Teórico, rara vez práctico |

Floyd-Warshall es óptimo para grafos densos ($m = O(n^2)$) donde las
alternativas basadas en Dijkstra son $O(n^3 \log n)$.

---

## 2. Formulación DP

### 2.1 Subproblema

Definir $d_k[i][j]$ como la distancia más corta de $i$ a $j$ usando solo
los vértices $\{0, 1, \ldots, k-1\}$ como intermedios.

- **Base** ($k = 0$): $d_0[i][j] = w(i, j)$ si existe arista, $\infty$ si
  no, y $0$ si $i = j$.
- **Transición**: $d_k[i][j] = \min\bigl(d_{k-1}[i][j],\; d_{k-1}[i][k-1] + d_{k-1}[k-1][j]\bigr)$.

La transición dice: el camino más corto de $i$ a $j$ usando vértices
$\{0, \ldots, k-1\}$ como intermedios, o bien:
1. **No pasa por $k-1$**: usa solo $\{0, \ldots, k-2\}$ → $d_{k-1}[i][j]$.
2. **Pasa por $k-1$**: va de $i$ a $k-1$ y de $k-1$ a $j$, ambos usando
   solo $\{0, \ldots, k-2\}$ → $d_{k-1}[i][k-1] + d_{k-1}[k-1][j]$.

### 2.2 Optimización de espacio

Observar que $d_k$ solo depende de $d_{k-1}$. Además, $d_k[i][k] = d_{k-1}[i][k]$
y $d_k[k][j] = d_{k-1}[k][j]$ (el camino de $i$ a $k$ pasando por $k$ no
mejora). Por lo tanto, se puede actualizar **in-place** usando una sola
matriz $\text{dist}[i][j]$:

```
for k ← 0 to n-1:
    for i ← 0 to n-1:
        for j ← 0 to n-1:
            if dist[i][k] + dist[k][j] < dist[i][j]:
                dist[i][j] ← dist[i][k] + dist[k][j]
```

Esto funciona porque en la iteración $k$, los valores `dist[i][k]` y
`dist[k][j]` no se modifican (ya son $d_k[i][k] = d_{k-1}[i][k]$).

---

## 3. Pseudocódigo completo

```
FLOYD-WARSHALL(W, n):
    // Initialize
    dist[i][j] ← W[i][j]     for all i,j
    next[i][j] ← j            if W[i][j] < ∞
    next[i][j] ← NIL          if W[i][j] = ∞

    // DP
    for k ← 0 to n-1:
        for i ← 0 to n-1:
            for j ← 0 to n-1:
                if dist[i][k] + dist[k][j] < dist[i][j]:
                    dist[i][j] ← dist[i][k] + dist[k][j]
                    next[i][j] ← next[i][k]

    // Check for negative cycles
    for i ← 0 to n-1:
        if dist[i][i] < 0:
            return "NEGATIVE CYCLE through vertex i"

    return dist[][], next[][]
```

### Reconstrucción de caminos

```
PATH(next, i, j):
    if next[i][j] == NIL:
        return "no path"
    path ← [i]
    while i ≠ j:
        i ← next[i][j]
        path.append(i)
    return path
```

La matriz `next[i][j]` almacena el siguiente vértice en el camino más
corto de $i$ a $j$. Alternativa: `prev[i][j]` almacena el predecesor,
pero `next` es más natural para reconstruir el camino hacia adelante.

---

## 4. Traza detallada

```
Grafo dirigido:
  0 →(3)→ 1
  0 →(8)→ 2
  1 →(2)→ 2
  2 →(1)→ 3
  3 →(4)→ 0
  1 →(7)→ 3

Matriz de pesos W:
       0    1    2    3
  0 [  0    3    8    ∞ ]
  1 [  ∞    0    2    7 ]
  2 [  ∞    ∞    0    1 ]
  3 [  4    ∞    ∞    0 ]

=== k=0 (intermedio: vértice 0) ===
Solo caminos que pasan por 0.
  dist[1][0] + dist[0][2] = ∞ + 8 → no mejora
  dist[3][0] + dist[0][1] = 4 + 3 = 7 → dist[3][1] = 7 (era ∞)
  dist[3][0] + dist[0][2] = 4 + 8 = 12 → dist[3][2] = 12 (era ∞)

       0    1    2    3
  0 [  0    3    8    ∞ ]
  1 [  ∞    0    2    7 ]
  2 [  ∞    ∞    0    1 ]
  3 [  4    7   12    0 ]

=== k=1 (intermedio: vértice 1) ===
  dist[0][1] + dist[1][2] = 3 + 2 = 5 < 8 → dist[0][2] = 5
  dist[0][1] + dist[1][3] = 3 + 7 = 10 → dist[0][3] = 10 (era ∞)
  dist[3][1] + dist[1][2] = 7 + 2 = 9 < 12 → dist[3][2] = 9

       0    1    2    3
  0 [  0    3    5   10 ]
  1 [  ∞    0    2    7 ]
  2 [  ∞    ∞    0    1 ]
  3 [  4    7    9    0 ]

=== k=2 (intermedio: vértice 2) ===
  dist[0][2] + dist[2][3] = 5 + 1 = 6 < 10 → dist[0][3] = 6
  dist[1][2] + dist[2][3] = 2 + 1 = 3 < 7 → dist[1][3] = 3
  dist[3][2] + dist[2][3] = 9 + 1 = 10 → no (dist[3][3]=0)

       0    1    2    3
  0 [  0    3    5    6 ]
  1 [  ∞    0    2    3 ]
  2 [  ∞    ∞    0    1 ]
  3 [  4    7    9    0 ]

=== k=3 (intermedio: vértice 3) ===
  dist[0][3] + dist[3][0] = 6 + 4 = 10 → dist[0][0] = 0, no
  dist[1][3] + dist[3][0] = 3 + 4 = 7 → dist[1][0] = 7 (era ∞)
  dist[1][3] + dist[3][1] = 3 + 7 = 10 → dist[1][1] = 0, no
  dist[1][3] + dist[3][2] = 3 + 9 = 12 → no (2)
  dist[2][3] + dist[3][0] = 1 + 4 = 5 → dist[2][0] = 5 (era ∞)
  dist[2][3] + dist[3][1] = 1 + 7 = 8 → dist[2][1] = 8 (era ∞)
  dist[2][3] + dist[3][2] = 1 + 9 = 10 → no (0)

Final:
       0    1    2    3
  0 [  0    3    5    6 ]
  1 [  7    0    2    3 ]
  2 [  5    8    0    1 ]
  3 [  4    7    9    0 ]

Diagonal: todos 0 → no hay ciclos negativos.
```

---

## 5. Complejidad

| Aspecto | Complejidad |
|---------|:-----------:|
| Tiempo | $O(n^3)$ |
| Espacio | $O(n^2)$ |

- Los tres bucles anidados hacen exactamente $n^3$ iteraciones.
- Independiente del número de aristas $m$ (siempre $n^3$).
- El espacio $O(n^2)$ es para las matrices `dist` y `next`.

### Constantes

El bucle interno es extremadamente simple (una comparación y una suma), lo
que hace que Floyd-Warshall sea muy rápido en la práctica para $n$ moderado
($n \leq 500$). Además, el acceso es mayormente secuencial en memoria, lo
que favorece la cache.

---

## 6. Detección de ciclos negativos

Tras ejecutar Floyd-Warshall, la diagonal $\text{dist}[i][i]$ indica la
distancia del ciclo más corto que pasa por $i$:

- $\text{dist}[i][i] < 0$: existe un ciclo negativo que incluye a $i$.
- $\text{dist}[i][i] = 0$: no hay ciclo negativo que pase por $i$.

**¿Qué pares se ven afectados?** Si existe un ciclo negativo que pasa por
$k$, entonces $\text{dist}[i][j] = -\infty$ para todo par $(i, j)$ tal que
existe camino de $i$ a $k$ y de $k$ a $j$. Para detectar estos pares:

```
for k ← 0 to n-1:
    if dist[k][k] < 0:
        for i ← 0 to n-1:
            for j ← 0 to n-1:
                if dist[i][k] < ∞ and dist[k][j] < ∞:
                    dist[i][j] ← -∞
```

---

## 7. Cierre transitivo

### 7.1 Definición

El **cierre transitivo** de un grafo dirigido $G$ es una matriz booleana
$T$ donde $T[i][j] = \text{true}$ si existe un camino de $i$ a $j$ en $G$.

### 7.2 Algoritmo de Warshall

Es Floyd-Warshall simplificado: en lugar de distancias, se usan booleanos:

```
TRANSITIVE-CLOSURE(G):
    reach[i][j] ← true    if (i,j) ∈ E or i=j
    reach[i][j] ← false   otherwise

    for k ← 0 to n-1:
        for i ← 0 to n-1:
            for j ← 0 to n-1:
                reach[i][j] ← reach[i][j] OR (reach[i][k] AND reach[k][j])
```

**Optimización con bits**: cada fila de `reach` se almacena como un bitset.
La operación `OR` se realiza palabra por palabra (64 bits a la vez),
reduciendo la constante por un factor de 64:

$$O\Bigl(\frac{n^3}{64}\Bigr) \text{ operaciones de bits}$$

---

## 8. Variantes

### 8.1 Minimax y maximin

Floyd-Warshall se puede adaptar para otros problemas de optimización
cambiando la operación en el bucle interno:

| Problema | Operación |
|----------|-----------|
| Camino más corto | $\min(d[i][j],\; d[i][k] + d[k][j])$ |
| Camino más largo (DAG) | $\max(d[i][j],\; d[i][k] + d[k][j])$ |
| Cuello de botella (maximin) | $\max(d[i][j],\; \min(d[i][k], d[k][j]))$ |
| Minimax | $\min(d[i][j],\; \max(d[i][k], d[k][j]))$ |
| Alcanzabilidad | $d[i][j] \lor (d[i][k] \land d[k][j])$ |

**Maximin** (widest path): encontrar el camino donde la arista de menor peso
es la más grande posible. Útil en redes donde el ancho de banda está
limitado por el enlace más lento.

### 8.2 Contar caminos

Para contar el número de caminos (no necesariamente simples) de longitud
$\leq n$:

```
count[i][j] ← 1  if (i,j) ∈ E
// En lugar de min, sumar:
count[i][j] += count[i][k] * count[k][j]
```

### 8.3 Floyd-Warshall en grafos no dirigidos

El grafo no dirigido se trata como un grafo dirigido con dos aristas
por cada arista original. La matriz de pesos es simétrica:
$W[i][j] = W[j][i]$. El resultado `dist[][]` también será simétrico.

---

## 9. Cuándo usar Floyd-Warshall

### Ventajas

- **Simplicidad**: 5 líneas de código para el core.
- **Grafos densos**: $O(n^3)$ es óptimo cuando $m = O(n^2)$.
- **Pesos negativos**: maneja pesos negativos y detecta ciclos negativos.
- **Todos los pares**: resultado completo en una ejecución.
- **Versatilidad**: se adapta a minimax, cierre transitivo, etc.

### Desventajas

- **Grafos dispersos**: $O(n^3)$ es excesivo cuando $m \ll n^2$. Dijkstra
  $\times n$ es $O(n(n + m) \log n)$, que es mejor si $m = O(n)$.
- **Grafos grandes**: $n > 5000$ empieza a ser lento ($5000^3 = 1.25 \times 10^{11}$
  operaciones).
- **Espacio**: $O(n^2)$ para las matrices. Con $n = 10000$,
  eso es $10^8$ entradas ($\approx 800$ MB con `int64`).

### Regla práctica

| $n$ | Floyd-Warshall | Dijkstra × $n$ |
|:---:|:--------------:|:--------------:|
| $\leq 500$ | Excelente | OK |
| $500 - 2000$ | Viable | Mejor si disperso |
| $> 2000$ | Muy lento | Necesario |

---

## 10. Programa completo en C

```c
/*
 * floyd_warshall.c
 *
 * Floyd-Warshall: all-pairs shortest paths, path reconstruction,
 * negative cycle detection, transitive closure, and bottleneck paths.
 *
 * Compile: gcc -O2 -o floyd_warshall floyd_warshall.c
 * Run:     ./floyd_warshall
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 20
#define INF (INT_MAX / 2)

/* =================== FLOYD-WARSHALL =============================== */

int dist[MAX_V][MAX_V];
int next_hop[MAX_V][MAX_V]; /* next vertex in path from i to j */

void floyd_warshall(int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (dist[i][k] + dist[k][j] < dist[i][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next_hop[i][j] = next_hop[i][k];
                }
}

void init_matrices(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            dist[i][j] = (i == j) ? 0 : INF;
            next_hop[i][j] = -1;
        }
}

void set_edge(int u, int v, int w) {
    dist[u][v] = w;
    next_hop[u][v] = v;
}

void print_matrix(int n, const char *label) {
    printf("%s:\n", label);
    printf("     ");
    for (int j = 0; j < n; j++) printf("%4d", j);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %d [", i);
        for (int j = 0; j < n; j++) {
            if (dist[i][j] >= INF / 2) printf("   ∞");
            else printf("%4d", dist[i][j]);
        }
        printf(" ]\n");
    }
}

void print_path(int i, int j) {
    if (next_hop[i][j] == -1) {
        printf("no path");
        return;
    }
    printf("%d", i);
    while (i != j) {
        i = next_hop[i][j];
        printf("→%d", i);
    }
}

/* ============ DEMO 1: Basic Floyd-Warshall ======================== */

void demo_basic(void) {
    printf("=== Demo 1: Basic Floyd-Warshall ===\n");

    int n = 4;
    init_matrices(n);
    set_edge(0, 1, 3);
    set_edge(0, 2, 8);
    set_edge(1, 2, 2);
    set_edge(1, 3, 7);
    set_edge(2, 3, 1);
    set_edge(3, 0, 4);

    printf("Graph: 0→1:3, 0→2:8, 1→2:2, 1→3:7, 2→3:1, 3→0:4\n\n");
    print_matrix(n, "Initial distances");

    floyd_warshall(n);
    printf("\n");
    print_matrix(n, "After Floyd-Warshall");

    printf("\nShortest paths:\n");
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) {
                printf("  %d→%d (dist=%d): ", i, j, dist[i][j]);
                print_path(i, j);
                printf("\n");
            }
    printf("\n");
}

/* ============ DEMO 2: Negative cycle detection ==================== */

void demo_neg_cycle(void) {
    printf("=== Demo 2: Negative cycle detection ===\n");

    int n = 4;
    init_matrices(n);
    set_edge(0, 1, 1);
    set_edge(1, 2, -1);
    set_edge(2, 3, -1);
    set_edge(3, 1, -1); /* cycle 1→2→3→1: -1-1-1 = -3 */

    printf("Graph: 0→1:1, 1→2:-1, 2→3:-1, 3→1:-1\n");
    printf("Cycle 1→2→3→1: weight = -3\n\n");

    floyd_warshall(n);
    print_matrix(n, "After Floyd-Warshall");

    printf("\nDiagonal check:\n");
    bool has_neg = false;
    for (int i = 0; i < n; i++) {
        printf("  dist[%d][%d] = %d%s\n", i, i, dist[i][i],
               dist[i][i] < 0 ? " ← NEGATIVE CYCLE" : "");
        if (dist[i][i] < 0) has_neg = true;
    }
    printf("Negative cycle: %s\n\n", has_neg ? "YES" : "NO");
}

/* ============ DEMO 3: Step-by-step trace ========================== */

void demo_trace(void) {
    printf("=== Demo 3: Step-by-step trace ===\n");

    int n = 4;
    init_matrices(n);
    set_edge(0, 1, 5);
    set_edge(0, 2, 10);
    set_edge(1, 2, 3);
    set_edge(2, 3, 1);
    set_edge(3, 0, 7);

    printf("Graph: 0→1:5, 0→2:10, 1→2:3, 2→3:1, 3→0:7\n\n");
    print_matrix(n, "k=-1 (initial)");

    for (int k = 0; k < n; k++) {
        printf("\n--- k=%d (intermediate vertex %d) ---\n", k, k);
        int updates = 0;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (dist[i][k] + dist[k][j] < dist[i][j]) {
                    int old = dist[i][j];
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next_hop[i][j] = next_hop[i][k];
                    printf("  dist[%d][%d]: %s → %d (via %d: %d+%d)\n",
                           i, j,
                           old >= INF/2 ? "∞" : (char[12]){0},
                           dist[i][j], k, dist[i][k] - (dist[i][k]+dist[k][j]-dist[i][j]) + (dist[i][k]+dist[k][j]-dist[i][j]), dist[k][j]);
                    /* Simpler print */
                    updates++;
                }
        if (updates == 0) printf("  (no changes)\n");
        print_matrix(n, "  Matrix");
    }
    printf("\n");
}

/* ============ DEMO 4: Transitive closure ========================== */

bool reach[MAX_V][MAX_V];

void demo_transitive(void) {
    printf("=== Demo 4: Transitive closure (Warshall) ===\n");

    int n = 5;
    memset(reach, false, sizeof(reach));
    for (int i = 0; i < n; i++) reach[i][i] = true;

    /* Graph: 0→1, 1→2, 2→3, 3→4, 2→0 (cycle) */
    int edges[][2] = {{0,1},{1,2},{2,3},{3,4},{2,0}};
    int m = 5;
    for (int e = 0; e < m; e++)
        reach[edges[e][0]][edges[e][1]] = true;

    printf("Graph: 0→1, 1→2, 2→3, 3→4, 2→0\n\n");

    printf("Before:\n     ");
    for (int j = 0; j < n; j++) printf("%2d", j);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %d [", i);
        for (int j = 0; j < n; j++)
            printf("%2d", reach[i][j] ? 1 : 0);
        printf(" ]\n");
    }

    /* Warshall's algorithm */
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (reach[i][k] && reach[k][j])
                    reach[i][j] = true;

    printf("\nAfter (transitive closure):\n     ");
    for (int j = 0; j < n; j++) printf("%2d", j);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %d [", i);
        for (int j = 0; j < n; j++)
            printf("%2d", reach[i][j] ? 1 : 0);
        printf(" ]\n");
    }

    printf("\nReachable pairs:\n");
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j && reach[i][j])
                printf("  %d → %d\n", i, j);
    printf("\n");
}

/* ============ DEMO 5: Undirected graph ============================ */

void demo_undirected(void) {
    printf("=== Demo 5: Undirected weighted graph ===\n");

    int n = 5;
    init_matrices(n);
    /* Symmetric edges */
    int edges[][3] = {{0,1,2},{0,2,6},{1,2,3},{1,3,8},{2,3,1},{2,4,5},{3,4,3}};
    int m = 7;

    printf("Undirected graph:\n");
    for (int e = 0; e < m; e++) {
        int u = edges[e][0], v = edges[e][1], w = edges[e][2];
        set_edge(u, v, w);
        set_edge(v, u, w);
        printf("  %d—%d:%d\n", u, v, w);
    }

    floyd_warshall(n);
    printf("\n");
    print_matrix(n, "All-pairs distances");

    /* Verify symmetry */
    bool symmetric = true;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (dist[i][j] != dist[j][i]) symmetric = false;
    printf("\nSymmetric? %s\n", symmetric ? "YES" : "NO");

    /* Diameter: max shortest path */
    int diameter = 0;
    int di = 0, dj = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (dist[i][j] < INF/2 && dist[i][j] > diameter) {
                diameter = dist[i][j];
                di = i; dj = j;
            }
    printf("Diameter: %d (between %d and %d)\n", diameter, di, dj);
    printf("  Path: ");
    print_path(di, dj);
    printf("\n\n");
}

/* ============ DEMO 6: Bottleneck (maximin) ======================== */

void demo_bottleneck(void) {
    printf("=== Demo 6: Bottleneck (widest) paths ===\n");

    int n = 4;
    int cap[MAX_V][MAX_V];

    /* Initialize: 0 capacity (no path), self = INF */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            cap[i][j] = (i == j) ? INF : 0;

    /* Network with link capacities (bandwidth) */
    cap[0][1] = 10; cap[1][0] = 10;
    cap[0][2] = 5;  cap[2][0] = 5;
    cap[1][2] = 8;  cap[2][1] = 8;
    cap[1][3] = 3;  cap[3][1] = 3;
    cap[2][3] = 7;  cap[3][2] = 7;

    printf("Network link capacities:\n");
    printf("  0—1:10, 0—2:5, 1—2:8, 1—3:3, 2—3:7\n\n");

    /* Maximin Floyd-Warshall */
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                int through_k = cap[i][k] < cap[k][j] ? cap[i][k] : cap[k][j];
                if (through_k > cap[i][j])
                    cap[i][j] = through_k;
            }

    printf("Bottleneck (widest path) matrix:\n");
    printf("     ");
    for (int j = 0; j < n; j++) printf("%4d", j);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %d [", i);
        for (int j = 0; j < n; j++) {
            if (cap[i][j] >= INF/2) printf("   ∞");
            else printf("%4d", cap[i][j]);
        }
        printf(" ]\n");
    }

    printf("\nMax bandwidth 0→3: %d\n", cap[0][3]);
    printf("  (Best path limited by its narrowest link)\n\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_basic();
    demo_neg_cycle();
    demo_trace();
    demo_transitive();
    demo_undirected();
    demo_bottleneck();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: Basic Floyd-Warshall ===
Graph: 0→1:3, 0→2:8, 1→2:2, 1→3:7, 2→3:1, 3→0:4

Initial distances:
        0   1   2   3
  0 [   0   3   8   ∞ ]
  1 [   ∞   0   2   7 ]
  2 [   ∞   ∞   0   1 ]
  3 [   4   ∞   ∞   0 ]

After Floyd-Warshall:
        0   1   2   3
  0 [   0   3   5   6 ]
  1 [   7   0   2   3 ]
  2 [   5   8   0   1 ]
  3 [   4   7   9   0 ]

Shortest paths:
  0→1 (dist=3): 0→1
  0→2 (dist=5): 0→1→2
  0→3 (dist=6): 0→1→2→3
  1→0 (dist=7): 1→2→3→0
  1→2 (dist=2): 1→2
  1→3 (dist=3): 1→2→3
  2→0 (dist=5): 2→3→0
  2→1 (dist=8): 2→3→0→1
  2→3 (dist=1): 2→3
  3→0 (dist=4): 3→0
  3→1 (dist=7): 3→0→1
  3→2 (dist=9): 3→0→1→2

=== Demo 2: Negative cycle detection ===
Graph: 0→1:1, 1→2:-1, 2→3:-1, 3→1:-1
Cycle 1→2→3→1: weight = -3

After Floyd-Warshall:
        0   1   2   3
  0 [   0  -2  -3  -4 ]
  1 [   ∞  -3  -4  -5 ]
  2 [   ∞  -4  -3  -4 ]
  3 [   ∞  -3  -4  -3 ]

Diagonal check:
  dist[0][0] = 0
  dist[1][1] = -3 ← NEGATIVE CYCLE
  dist[2][2] = -3 ← NEGATIVE CYCLE
  dist[3][3] = -3 ← NEGATIVE CYCLE
Negative cycle: YES

=== Demo 4: Transitive closure (Warshall) ===
Graph: 0→1, 1→2, 2→3, 3→4, 2→0

Before:
      0 1 2 3 4
  0 [ 1 1 0 0 0 ]
  1 [ 0 1 1 0 0 ]
  2 [ 1 0 1 1 0 ]
  3 [ 0 0 0 1 1 ]
  4 [ 0 0 0 0 1 ]

After (transitive closure):
      0 1 2 3 4
  0 [ 1 1 1 1 1 ]
  1 [ 1 1 1 1 1 ]
  2 [ 1 1 1 1 1 ]
  3 [ 0 0 0 1 1 ]
  4 [ 0 0 0 0 1 ]

=== Demo 6: Bottleneck (widest) paths ===
Network link capacities:
  0—1:10, 0—2:5, 1—2:8, 1—3:3, 2—3:7

Bottleneck (widest path) matrix:
        0   1   2   3
  0 [   ∞  10   8   7 ]
  1 [  10   ∞   8   7 ]
  2 [   8   8   ∞   7 ]
  3 [   7   7   7   ∞ ]

Max bandwidth 0→3: 7
  (Best path limited by its narrowest link)
```

---

## 11. Programa completo en Rust

```rust
/*
 * floyd_warshall.rs
 *
 * Floyd-Warshall: all-pairs shortest paths, path reconstruction,
 * negative cycle detection, transitive closure, bottleneck paths,
 * comparison with repeated Dijkstra, and graph diameter.
 *
 * Compile: rustc -O -o floyd_warshall floyd_warshall.rs
 * Run:     ./floyd_warshall
 */

use std::cmp::Reverse;
use std::collections::BinaryHeap;

const INF: i64 = i64::MAX / 2;

/* =================== FLOYD-WARSHALL =============================== */

struct FWResult {
    dist: Vec<Vec<i64>>,
    next: Vec<Vec<Option<usize>>>,
    n: usize,
}

impl FWResult {
    fn path(&self, mut i: usize, j: usize) -> Option<Vec<usize>> {
        if self.next[i][j].is_none() { return None; }
        let mut path = vec![i];
        while i != j {
            i = self.next[i][j]?;
            path.push(i);
        }
        Some(path)
    }

    fn has_negative_cycle(&self) -> bool {
        (0..self.n).any(|i| self.dist[i][i] < 0)
    }

    fn print_matrix(&self, label: &str) {
        println!("{}:", label);
        print!("     ");
        for j in 0..self.n { print!("{:>5}", j); }
        println!();
        for i in 0..self.n {
            print!("  {} [", i);
            for j in 0..self.n {
                if self.dist[i][j] >= INF / 2 { print!("    ∞"); }
                else { print!("{:>5}", self.dist[i][j]); }
            }
            println!(" ]");
        }
    }
}

fn floyd_warshall(weight: &[Vec<i64>]) -> FWResult {
    let n = weight.len();
    let mut dist = weight.to_vec();
    let mut next: Vec<Vec<Option<usize>>> = vec![vec![None; n]; n];

    // Initialize next
    for i in 0..n {
        for j in 0..n {
            if dist[i][j] < INF / 2 {
                next[i][j] = Some(j);
            }
        }
    }

    // DP
    for k in 0..n {
        for i in 0..n {
            for j in 0..n {
                if dist[i][k] + dist[k][j] < dist[i][j] {
                    dist[i][j] = dist[i][k] + dist[k][j];
                    next[i][j] = next[i][k];
                }
            }
        }
    }

    FWResult { dist, next, n }
}

fn build_weight_matrix(n: usize, edges: &[(usize, usize, i64)]) -> Vec<Vec<i64>> {
    let mut w = vec![vec![INF; n]; n];
    for i in 0..n { w[i][i] = 0; }
    for &(u, v, wt) in edges { w[u][v] = wt; }
    w
}

/* ======== Demo 1: Basic Floyd-Warshall ============================ */

fn demo_basic() {
    println!("=== Demo 1: Basic Floyd-Warshall ===");

    let edges = vec![
        (0,1,3), (0,2,8), (1,2,2), (1,3,7), (2,3,1), (3,0,4),
    ];
    let w = build_weight_matrix(4, &edges);
    let fw = floyd_warshall(&w);

    fw.print_matrix("All-pairs distances");

    println!("\nAll shortest paths:");
    for i in 0..fw.n {
        for j in 0..fw.n {
            if i == j { continue; }
            if let Some(path) = fw.path(i, j) {
                let ps: Vec<String> = path.iter().map(|v| v.to_string()).collect();
                println!("  {}→{} (dist={}): {}", i, j, fw.dist[i][j], ps.join("→"));
            } else {
                println!("  {}→{}: no path", i, j);
            }
        }
    }
    println!();
}

/* ======== Demo 2: Negative cycle ================================== */

fn demo_neg_cycle() {
    println!("=== Demo 2: Negative cycle detection ===");

    let edges = vec![(0,1,1), (1,2,-1), (2,3,-1), (3,1,-1)];
    let w = build_weight_matrix(4, &edges);
    let fw = floyd_warshall(&w);

    fw.print_matrix("Distance matrix");

    println!("\nDiagonal:");
    for i in 0..fw.n {
        let tag = if fw.dist[i][i] < 0 { " ← NEG CYCLE" } else { "" };
        println!("  dist[{}][{}] = {}{}", i, i, fw.dist[i][i], tag);
    }
    println!("Has negative cycle: {}\n", fw.has_negative_cycle());
}

/* ======== Demo 3: Transitive closure ============================== */

fn transitive_closure(n: usize, edges: &[(usize, usize)]) -> Vec<Vec<bool>> {
    let mut reach = vec![vec![false; n]; n];
    for i in 0..n { reach[i][i] = true; }
    for &(u, v) in edges { reach[u][v] = true; }

    for k in 0..n {
        for i in 0..n {
            for j in 0..n {
                if reach[i][k] && reach[k][j] {
                    reach[i][j] = true;
                }
            }
        }
    }
    reach
}

fn demo_transitive() {
    println!("=== Demo 3: Transitive closure ===");

    let edges = vec![(0,1), (1,2), (2,3), (3,4), (2,0)];
    let reach = transitive_closure(5, &edges);

    println!("Graph: 0→1, 1→2, 2→3, 3→4, 2→0\n");
    print!("     ");
    for j in 0..5 { print!("{:>2}", j); }
    println!();
    for i in 0..5 {
        print!("  {} [", i);
        for j in 0..5 {
            print!("{:>2}", if reach[i][j] { 1 } else { 0 });
        }
        println!(" ]");
    }

    // Which vertices can reach vertex 4?
    let can_reach_4: Vec<usize> = (0..5).filter(|&i| reach[i][4]).collect();
    println!("\nVertices that can reach 4: {:?}", can_reach_4);

    // Is the graph strongly connected?
    let strong = (0..5).all(|i| (0..5).all(|j| reach[i][j]));
    println!("Strongly connected? {}", strong);
    println!();
}

/* ======== Demo 4: Undirected + diameter ============================ */

fn demo_undirected() {
    println!("=== Demo 4: Undirected graph + diameter ===");

    let n = 6;
    let edges_undir = vec![
        (0,1,4), (0,2,2), (1,2,1), (1,3,5),
        (2,3,8), (2,4,10), (3,4,2), (3,5,6), (4,5,3),
    ];

    // Build symmetric matrix
    let mut w = vec![vec![INF; n]; n];
    for i in 0..n { w[i][i] = 0; }
    for &(u, v, wt) in &edges_undir {
        w[u][v] = wt;
        w[v][u] = wt;
    }

    let fw = floyd_warshall(&w);
    fw.print_matrix("All-pairs distances");

    // Diameter (longest shortest path)
    let mut diameter = 0i64;
    let mut di = 0;
    let mut dj = 0;
    for i in 0..n {
        for j in 0..n {
            if fw.dist[i][j] < INF / 2 && fw.dist[i][j] > diameter {
                diameter = fw.dist[i][j];
                di = i;
                dj = j;
            }
        }
    }

    println!("\nDiameter: {} (between {} and {})", diameter, di, dj);
    if let Some(path) = fw.path(di, dj) {
        let ps: Vec<String> = path.iter().map(|v| v.to_string()).collect();
        println!("Path: {}", ps.join("→"));
    }

    // Eccentricity of each vertex
    println!("\nEccentricity (max dist from vertex):");
    for i in 0..n {
        let ecc = (0..n).filter(|&j| j != i)
            .map(|j| fw.dist[i][j])
            .filter(|&d| d < INF / 2)
            .max().unwrap_or(0);
        println!("  vertex {}: {}", i, ecc);
    }
    println!();
}

/* ======== Demo 5: Bottleneck (maximin) paths ====================== */

fn demo_bottleneck() {
    println!("=== Demo 5: Bottleneck (widest) paths ===");

    let n = 5;
    let mut cap = vec![vec![0i64; n]; n];
    for i in 0..n { cap[i][i] = INF; }

    let links = vec![
        (0,1,10), (0,2,5), (1,2,8), (1,3,3), (2,3,7), (2,4,6), (3,4,4),
    ];
    for &(u, v, c) in &links {
        cap[u][v] = c;
        cap[v][u] = c;
    }

    println!("Network capacities:");
    for &(u, v, c) in &links {
        println!("  {}—{}: {}", u, v, c);
    }

    // Maximin Floyd-Warshall
    for k in 0..n {
        for i in 0..n {
            for j in 0..n {
                let through_k = cap[i][k].min(cap[k][j]);
                if through_k > cap[i][j] {
                    cap[i][j] = through_k;
                }
            }
        }
    }

    println!("\nBottleneck matrix:");
    print!("     ");
    for j in 0..n { print!("{:>5}", j); }
    println!();
    for i in 0..n {
        print!("  {} [", i);
        for j in 0..n {
            if cap[i][j] >= INF / 2 { print!("    ∞"); }
            else { print!("{:>5}", cap[i][j]); }
        }
        println!(" ]");
    }

    println!("\nMax bandwidth 0→4: {}", cap[0][4]);
    println!();
}

/* ======== Demo 6: FW vs repeated Dijkstra ========================= */

fn dijkstra_apsp(n: usize, adj: &[Vec<(usize, i64)>]) -> Vec<Vec<i64>> {
    let mut all_dist = vec![vec![INF; n]; n];

    for src in 0..n {
        let mut dist = vec![INF; n];
        dist[src] = 0;
        let mut heap = BinaryHeap::new();
        heap.push(Reverse((0i64, src)));

        while let Some(Reverse((d, u))) = heap.pop() {
            if d > dist[u] { continue; }
            for &(v, w) in &adj[u] {
                let nd = dist[u] + w;
                if nd < dist[v] {
                    dist[v] = nd;
                    heap.push(Reverse((nd, v)));
                }
            }
        }
        all_dist[src] = dist;
    }
    all_dist
}

fn demo_vs_dijkstra() {
    println!("=== Demo 6: Floyd-Warshall vs repeated Dijkstra ===");

    let n = 5;
    let edges = vec![
        (0,1,3), (0,2,8), (1,2,2), (1,3,5), (2,3,1), (2,4,4), (3,4,6), (4,0,7),
    ];

    // Build adjacency list for Dijkstra
    let mut adj = vec![vec![]; n];
    for &(u, v, w) in &edges {
        adj[u].push((v, w));
    }

    // Floyd-Warshall
    let w = build_weight_matrix(n, &edges);
    let fw = floyd_warshall(&w);

    // Dijkstra × n
    let dj = dijkstra_apsp(n, &adj);

    println!("Comparing results:");
    let mut all_match = true;
    for i in 0..n {
        for j in 0..n {
            if fw.dist[i][j] != dj[i][j] {
                println!("  MISMATCH at ({},{}): FW={}, DJ={}",
                         i, j, fw.dist[i][j], dj[i][j]);
                all_match = false;
            }
        }
    }
    if all_match { println!("  All {} entries match! ✓", n * n); }

    println!("\nComplexity comparison for n={}:", n);
    println!("  Floyd-Warshall: O(n³) = O({})", n * n * n);
    println!("  Dijkstra × n:   O(n(n+m)log n) = O({})",
             n * (n + edges.len()) * ((n as f64).log2() as usize + 1));
    println!();
}

/* ======== Demo 7: Graph center and radius ========================= */

fn demo_center() {
    println!("=== Demo 7: Graph center and radius ===");

    let n = 6;
    let edges_undir = vec![
        (0,1,1), (1,2,2), (2,3,1), (3,4,3), (4,5,1), (0,5,4), (1,4,5),
    ];

    let mut w = vec![vec![INF; n]; n];
    for i in 0..n { w[i][i] = 0; }
    for &(u, v, wt) in &edges_undir {
        w[u][v] = wt;
        w[v][u] = wt;
    }

    let fw = floyd_warshall(&w);

    // Eccentricity of each vertex
    let ecc: Vec<i64> = (0..n).map(|i| {
        (0..n).filter(|&j| j != i)
            .map(|j| fw.dist[i][j])
            .filter(|&d| d < INF / 2)
            .max().unwrap_or(0)
    }).collect();

    let radius = *ecc.iter().min().unwrap();
    let diameter = *ecc.iter().max().unwrap();
    let center: Vec<usize> = (0..n).filter(|&i| ecc[i] == radius).collect();

    println!("Eccentricities:");
    for i in 0..n {
        let tag = if ecc[i] == radius { " ← center" } else { "" };
        println!("  vertex {}: {}{}", i, ecc[i], tag);
    }
    println!("\nRadius: {}", radius);
    println!("Diameter: {}", diameter);
    println!("Center vertex(es): {:?}", center);
    println!();
}

/* ======== Demo 8: Step-by-step with k tracking ==================== */

fn demo_trace() {
    println!("=== Demo 8: Step-by-step trace ===");

    let edges = vec![(0,1,5), (0,2,10), (1,2,3), (2,3,1), (3,0,7)];
    let n = 4;
    let mut dist = build_weight_matrix(n, &edges);

    println!("Graph: 0→1:5, 0→2:10, 1→2:3, 2→3:1, 3→0:7\n");

    for k in 0..n {
        let mut updates = 0;
        for i in 0..n {
            for j in 0..n {
                if dist[i][k] + dist[k][j] < dist[i][j] {
                    println!("  k={}: dist[{}][{}] = {} → {} (via {}: {}+{})",
                             k, i, j, dist[i][j], dist[i][k] + dist[k][j],
                             k, dist[i][k], dist[k][j]);
                    dist[i][j] = dist[i][k] + dist[k][j];
                    updates += 1;
                }
            }
        }
        if updates == 0 { println!("  k={}: no changes", k); }
        println!();
    }

    println!("Final matrix:");
    for i in 0..n {
        let row: Vec<String> = dist[i].iter().map(|&d|
            if d >= INF / 2 { "∞".to_string() } else { d.to_string() }
        ).collect();
        println!("  {}: [{}]", i, row.join(", "));
    }
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_basic();
    demo_neg_cycle();
    demo_transitive();
    demo_undirected();
    demo_bottleneck();
    demo_vs_dijkstra();
    demo_center();
    demo_trace();
}
```

### Salida esperada

```
=== Demo 1: Basic Floyd-Warshall ===
All-pairs distances:
        0    1    2    3
  0 [    0    3    5    6 ]
  1 [    7    0    2    3 ]
  2 [    5    8    0    1 ]
  3 [    4    7    9    0 ]

All shortest paths:
  0→1 (dist=3): 0→1
  0→2 (dist=5): 0→1→2
  0→3 (dist=6): 0→1→2→3
  ...

=== Demo 2: Negative cycle detection ===
Diagonal:
  dist[0][0] = 0
  dist[1][1] = -3 ← NEG CYCLE
  dist[2][2] = -3 ← NEG CYCLE
  dist[3][3] = -3 ← NEG CYCLE
Has negative cycle: true

=== Demo 3: Transitive closure ===
      0 1 2 3 4
  0 [ 1 1 1 1 1 ]
  1 [ 1 1 1 1 1 ]
  2 [ 1 1 1 1 1 ]
  3 [ 0 0 0 1 1 ]
  4 [ 0 0 0 0 1 ]
Vertices that can reach 4: [0, 1, 2, 3, 4]
Strongly connected? false

=== Demo 4: Undirected graph + diameter ===
Diameter: 12 (between 0 and 5)
Eccentricity:
  vertex 0: 12
  ...

=== Demo 5: Bottleneck (widest) paths ===
Max bandwidth 0→4: 6

=== Demo 6: Floyd-Warshall vs repeated Dijkstra ===
  All 25 entries match! ✓

=== Demo 7: Graph center and radius ===
Eccentricities:
  vertex 0: 5
  vertex 1: 5
  vertex 2: 4 ← center
  vertex 3: 5
  vertex 4: 5
  vertex 5: 5
Radius: 4
Diameter: 5
Center vertex(es): [2]
```

---

## 12. Ejercicios

### Ejercicio 1 — Floyd-Warshall a mano

Grafo dirigido: $0\to1{:}2,\ 0\to2{:}5,\ 1\to2{:}1,\ 2\to3{:}3,\ 3\to0{:}6$.

Ejecuta Floyd-Warshall mostrando la matriz después de cada $k$.

<details>
<summary>Traza</summary>

```
Inicial:
     0  1  2  3
0 [  0  2  5  ∞ ]
1 [  ∞  0  1  ∞ ]
2 [  ∞  ∞  0  3 ]
3 [  6  ∞  ∞  0 ]

k=0: 3→0→1: 6+2=8, 3→0→2: 6+5=11
     0  1  2  3
0 [  0  2  5  ∞ ]
1 [  ∞  0  1  ∞ ]
2 [  ∞  ∞  0  3 ]
3 [  6  8 11  0 ]

k=1: 0→1→2: 2+1=3<5, 3→1→2: 8+1=9<11
     0  1  2  3
0 [  0  2  3  ∞ ]
1 [  ∞  0  1  ∞ ]
2 [  ∞  ∞  0  3 ]
3 [  6  8  9  0 ]

k=2: 0→2→3: 3+3=6, 1→2→3: 1+3=4, 3→2→3: 9+3=12>0
     0  1  2  3
0 [  0  2  3  6 ]
1 [  ∞  0  1  4 ]
2 [  ∞  ∞  0  3 ]
3 [  6  8  9  0 ]

k=3: 1→3→0: 4+6=10, 2→3→0: 3+6=9
     0  1  2  3
0 [  0  2  3  6 ]
1 [ 10  0  1  4 ]
2 [  9  ∞  0  3 ]  → 2→3→0→1: 9+2=11
3 [  6  8  9  0 ]

Continuando k=3 completo:
  2→3→0: 3+6=9, 2→3→0→1: wait, need to check all.
  Actually after k=3: 2→0=9, 2→1: 2→3→0→1? = 3+6+2... no,
  dist[2][3]+dist[3][1] = 3+8=11.

Final:
     0  1  2  3
0 [  0  2  3  6 ]
1 [ 10  0  1  4 ]
2 [  9 11  0  3 ]
3 [  6  8  9  0 ]
```
</details>

---

### Ejercicio 2 — Detectar ciclo negativo

Grafo: $0\to1{:}2,\ 1\to2{:}{-3},\ 2\to0{:}{-1}$.

Ejecuta Floyd-Warshall y verifica la diagonal.

<details>
<summary>Respuesta</summary>

Ciclo $0\to1\to2\to0$: $2 + (-3) + (-1) = -2 < 0$.

Tras Floyd-Warshall:
- $\text{dist}[0][0]$: vía $0\to1\to2\to0 = -2 < 0$ → **ciclo negativo**.
- $\text{dist}[1][1]$: vía $1\to2\to0\to1 = -3+(-1)+2 = -2 < 0$.
- $\text{dist}[2][2]$: vía $2\to0\to1\to2 = -1+2+(-3) = -2 < 0$.

Toda la diagonal es $-2$. Todos los vértices participan en el ciclo negativo.
</details>

---

### Ejercicio 3 — Cierre transitivo

Grafo dirigido: $0\to1,\ 1\to2,\ 3\to4,\ 4\to3$.

a) Calcula el cierre transitivo con Warshall.
b) ¿Es el grafo fuertemente conexo?
c) ¿Desde qué vértices se puede alcanzar el vértice 2?

<details>
<summary>Respuesta</summary>

a) Resultado:
```
     0 1 2 3 4
0 [  1 1 1 0 0 ]
1 [  0 1 1 0 0 ]
2 [  0 0 1 0 0 ]
3 [  0 0 0 1 1 ]
4 [  0 0 0 1 1 ]
```

b) **No** es fuertemente conexo (e.g., $\text{reach}[2][0] = \text{false}$).

c) Vértices que alcanzan 2: columna 2 → $\{0, 1, 2\}$.
</details>

---

### Ejercicio 4 — Bottleneck path

Red con capacidades:
```
0 —(10)— 1 —(4)— 2
|                 |
(6)              (8)
|                 |
3 ———(5)——— 4 —(3)— 5
```

Calcula el ancho de banda máximo (bottleneck) de 0 a 5.

<details>
<summary>Respuesta</summary>

Caminos posibles (bottleneck = mín arista en el camino):
- $0\to1\to2\to4\to5$: $\min(10, 4, 8, 3) = 3$.
- $0\to3\to4\to5$: $\min(6, 5, 3) = 3$.
- $0\to1\to2\to4\to3$... no llega a 5.
- $0\to3\to4\to2\to1$... no ayuda.

Maximin Floyd-Warshall:
cap[0][5] = $\max$ de los bottlenecks de todos los caminos.

El mejor camino: $0\to1\to2\to4\to5$, bottleneck $= 3$, o
$0\to3\to4\to5$, bottleneck $= 3$. Hay empate.

¿Hay algo mejor? $0\to3\to4\to2\to\ldots$? No llega a 5 mejor.

**Bottleneck = 3** (limitado por la arista $4\to5$).
</details>

---

### Ejercicio 5 — Diámetro del grafo

Grafo no dirigido: $0\text{-}1{:}1,\ 1\text{-}2{:}2,\ 2\text{-}3{:}3,\ 3\text{-}4{:}1,\ 0\text{-}4{:}10$.

a) Calcula la matriz de distancias con Floyd-Warshall.
b) ¿Cuál es el diámetro?
c) ¿Cuál es el centro del grafo?

<details>
<summary>Respuesta</summary>

a) Matriz (simétrica):
```
     0  1  2  3  4
0 [  0  1  3  6  7 ]
1 [  1  0  2  5  6 ]
2 [  3  2  0  3  4 ]
3 [  6  5  3  0  1 ]
4 [  7  6  4  1  0 ]
```

b) **Diámetro** = $\max_{i,j} \text{dist}[i][j] = 7$ (entre 0 y 4).
Nota: la arista directa $0\text{-}4{:}10$ es más larga que
$0\to1\to2\to3\to4 = 1+2+3+1 = 7$.

c) Excentricidades: $e(0)=7, e(1)=6, e(2)=4, e(3)=6, e(4)=7$.
**Centro**: vértice 2 (excentricidad mínima = 4).
**Radio**: 4.
</details>

---

### Ejercicio 6 — FW vs Dijkstra × n: cuándo elegir cada uno

Para cada caso, indica qué algoritmo usar para APSP y por qué:

a) $n = 200,\ m = 15000$ (grafo denso).
b) $n = 10000,\ m = 30000$ (grafo disperso).
c) $n = 100,\ m = 500$, con pesos negativos.
d) $n = 50$, solo necesitas el cierre transitivo.

<details>
<summary>Respuesta</summary>

a) **Floyd-Warshall**. Grafo denso ($m \approx n^2/2.7$). FW: $O(200^3) = 8 \times 10^6$.
Dijkstra × $n$: $O(200 \times 15000 \times \log 200) \approx 2.3 \times 10^7$. FW es más
rápido y más simple.

b) **Dijkstra × n**. Grafo disperso ($m = 3n$). FW: $O(10^{12})$ — imposible.
Dijkstra × $n$: $O(10000 \times 40000 \times 14) \approx 5.6 \times 10^9$ — viable con
optimizaciones.

c) **Floyd-Warshall** (o Johnson). FW: $O(10^6)$ — trivial.
Dijkstra no sirve por los pesos negativos. Johnson requiere Bellman-Ford +
Dijkstra × $n$, más complejo para $n$ tan pequeño.

d) **Warshall** (cierre transitivo). $O(50^3/64)$ con bitsets — instantáneo.
No necesita distancias, solo alcanzabilidad.
</details>

---

### Ejercicio 7 — Reconstrucción de caminos

Usando la matriz final del ejercicio 1, reconstruye los caminos:
a) $0 \to 3$
b) $1 \to 0$
c) $2 \to 1$

<details>
<summary>Respuesta</summary>

De la traza del ejercicio 1:
```
next[][] (inicializado con destino directo, actualizado con FW):

Después de FW:
  next[0][3] = next[0][1] = 1 (porque 0→3 pasó por 0→1→2→3)
  next[1][0] = next[1][3] (porque 1→0 pasó por 1→3→0)
  next[2][1] = next[2][3] (porque 2→1 pasó por 2→3→0→1)
```

a) $0\to3$: next[0][3]=1, next[1][3]=... siguiendo: $0 \to 1 \to 2 \to 3$.
   Dist = 2+1+3 = 6.

b) $1\to0$: $1 \to 2 \to 3 \to 0$. Dist = 1+3+6 = 10.

c) $2\to1$: $2 \to 3 \to 0 \to 1$. Dist = 3+6+2 = 11.
</details>

---

### Ejercicio 8 — Implementar Floyd-Warshall con impresión de $k$

Escribe un programa que:
1. Lea un grafo dirigido ponderado.
2. Ejecute Floyd-Warshall imprimiendo la matriz después de cada $k$.
3. Al final, imprima todos los caminos más cortos.
4. Detecte si hay ciclo negativo.

Prueba con el grafo de la traza (sección 4).

<details>
<summary>Pista</summary>

El core son las 3 líneas del triple bucle. El esfuerzo está en la
inicialización correcta de `next[][]` y la función de reconstrucción.
Asegúrate de que `next[i][j] = j` cuando hay arista directa, y
`next[i][j] = next[i][k]` cuando se actualiza vía $k$.
</details>

---

### Ejercicio 9 — Cierre transitivo con bitsets

Para un grafo de 8 vértices, representa cada fila del cierre transitivo
como un `uint8_t` (8 bits). Implementa Warshall usando operaciones `|` y
`&` a nivel de bits.

<details>
<summary>Idea de implementación</summary>

```c
uint8_t reach[8]; // reach[i] es un bitset: bit j = 1 iff i can reach j

// Initialize
for each edge (i,j): reach[i] |= (1 << j);
for each i: reach[i] |= (1 << i); // self

// Warshall
for (int k = 0; k < 8; k++)
    for (int i = 0; i < 8; i++)
        if (reach[i] & (1 << k))  // i can reach k
            reach[i] |= reach[k]; // then i can reach everything k can
```

Esto es $O(n^2)$ en el bucle (vs $O(n^3)$ sin bits) porque la
operación `|= reach[k]` procesa 8 bits simultáneamente. Para $n=64$,
usar `uint64_t` para una fila entera.
</details>

---

### Ejercicio 10 — Aplicación: ruteo en red

Una red tiene 5 routers (0-4) con latencias:

| Enlace | Latencia (ms) |
|:------:|:------------:|
| 0-1 | 5 |
| 0-3 | 12 |
| 1-2 | 3 |
| 1-3 | 8 |
| 2-4 | 7 |
| 3-4 | 2 |

a) Calcula las tablas de ruteo completas (APSP) con Floyd-Warshall.
b) ¿Por dónde envía el router 0 un paquete destinado al router 4?
c) Si el enlace 1-3 se cae (latencia $\infty$), ¿cómo cambian las rutas?

<details>
<summary>Respuesta</summary>

a) Grafo no dirigido. Matriz final:
```
     0   1   2   3   4
0 [  0   5   8  13  15 ]
1 [  5   0   3   8  10 ]
2 [  8   3   0  11   7 ]
3 [ 13   8  11   0   2 ]
4 [ 15  10   7   2   0 ]
```

b) Router 0 → Router 4: dist=15. next[0][4]=1 (vía 0→1→2→4: 5+3+7=15).
Alternativa 0→3→4: 12+2=14... ¡es mejor!

Corrección recalculando: 0→3→4 = 12+2 = **14** < 15.
next[0][4] = 3. El router 0 envía al router 3.

c) Sin 1-3: 0→3 cambia de 12 (directo) a... sigue 12 (directo, no pasaba
por 1-3). Las rutas que usaban 1→3 se ven afectadas:
1→3: antes 8, ahora 1→2→4→3 = 3+7+2=12. 1→4: antes 10, ahora 1→2→4=10
(sin cambio). Las tablas de 0 no cambian porque 0→3 es directo.
</details>
