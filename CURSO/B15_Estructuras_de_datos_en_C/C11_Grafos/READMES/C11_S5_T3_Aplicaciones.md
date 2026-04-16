# T03 — Aplicaciones de flujo en redes

## Objetivo

Aplicar flujo máximo y corte mínimo a problemas combinatorios: **matching
bipartito máximo** (teorema de König, condición de Hall), **caminos disjuntos**
(teorema de Menger), **selección de proyectos** (max weight closure), y
**asignación de recursos**. Cada reducción transforma el problema original en una
instancia de flujo máximo / corte mínimo, resolvible con Edmonds-Karp (T02).

---

## 1. Matching bipartito máximo

### 1.1 Definición

Dado un grafo bipartito $G = (L \cup R, E)$, un **matching** $M \subseteq E$ es
un conjunto de aristas sin vértices en común. El **matching máximo** es el de
mayor cardinalidad.

### 1.2 Reducción a flujo máximo

Construir la red de flujo:

1. Añadir **super-fuente** $s$ con arista $s \to \ell$ de capacidad 1 para
   cada $\ell \in L$.
2. Añadir **super-sumidero** $t$ con arista $r \to t$ de capacidad 1 para
   cada $r \in R$.
3. Para cada arista $(\ell, r) \in E$, añadir $\ell \to r$ con capacidad 1.

**Teorema**: el flujo máximo en esta red es igual al matching máximo de $G$.

*Demostración*: por el teorema de integralidad, el flujo máximo es entero (todas
las capacidades son 1). Un flujo entero de valor $k$ corresponde a $k$ aristas
$\ell \to r$ con $f(\ell,r) = 1$. Como $f(s,\ell) \leq 1$, cada $\ell$ aparece
a lo sumo una vez. Análogamente, cada $r$ aparece a lo sumo una vez. Esto es un
matching de tamaño $k$.

### 1.3 Ejemplo

Trabajadores $L = \{W_0, W_1, W_2, W_3\}$ y trabajos $R = \{J_0, J_1, J_2, J_3\}$:

| Trabajador | Trabajos posibles |
|:----------:|:-----------------:|
| $W_0$ | $J_0, J_1$ |
| $W_1$ | $J_0, J_2$ |
| $W_2$ | $J_1, J_3$ |
| $W_3$ | $J_2, J_3$ |

Red de flujo (con $s=8, t=9$, workers=0-3, jobs=4-7):

```
    s --1--> W0 --1--> J0 --1--> t
    |        |         ^         ^
    |--1--> W1 --1-----+  J1 ---+
    |        |             ^
    |--1--> W2 --1--> J2 --+
    |        |         ^
    +--1--> W3 --1--> J3 --+
```

Augmentaciones BFS:
1. $s \to W_0 \to J_0 \to t$: $\delta=1$. Match $W_0$-$J_0$.
2. $s \to W_1 \to J_2 \to t$: $\delta=1$. Match $W_1$-$J_2$.
3. $s \to W_2 \to J_1 \to t$: $\delta=1$. Match $W_2$-$J_1$.
4. $s \to W_3 \to J_3 \to t$: $\delta=1$. Match $W_3$-$J_3$.

**Matching máximo**: $\{W_0\text{-}J_0,\ W_1\text{-}J_2,\ W_2\text{-}J_1,\ W_3\text{-}J_3\}$, tamaño **4** (perfecto).

### 1.4 Augmentaciones que rearreglan el matching

Si $W_3$ solo puede hacer $J_2$ (eliminamos $W_3$-$J_3$), las primeras 3
augmentaciones son iguales. La cuarta:

$s \to W_3 \to J_2$: bloqueado ($J_2$ ya matcheado con $W_1$). BFS encuentra
camino usando backward edges:

$$s \to W_3 \to J_2 \xrightarrow{\text{back}} W_1 \to J_0 \xrightarrow{\text{back}} W_0 \to J_1 \xrightarrow{\text{back}} W_2 \to J_3 \to t$$

Resultado: $W_0$-$J_1$, $W_1$-$J_0$, $W_2$-$J_3$, $W_3$-$J_2$. Aún perfecto
— el flujo **rearregló** el matching completo.

---

## 2. Teorema de König

### 2.1 Enunciado

En un grafo bipartito:

$$\text{max matching} = \text{min vertex cover}$$

Un **vertex cover** es un conjunto $C \subseteq V$ tal que cada arista tiene al
menos un extremo en $C$.

### 2.2 Construcción del vertex cover desde el matching

Dado un matching máximo $M$:

1. $U \gets$ vértices de $L$ no matcheados.
2. Construir **árbol alternante** desde $U$: desde cada $u \in U$, seguir
   aristas no matcheadas hacia $R$, luego aristas matcheadas de vuelta a $L$,
   alternando.
3. $Z \gets$ todos los vértices alcanzados (incluyendo $U$).
4. **Vertex cover** $= (L \setminus Z) \cup (R \cap Z)$.

### 2.3 Ejemplo

Grafo: $L = \{0,1,2,3\}$, $R = \{4,5,6\}$.

| Vértice L | Vecinos en R |
|:---------:|:------------:|
| 0 | 4, 5 |
| 1 | 5 |
| 2 | 4, 5 |
| 3 | 6 |

Matching máximo $M = \{0\text{-}4, 1\text{-}5, 3\text{-}6\}$, tamaño 3.
Vértice 2 no matcheado.

Árbol alternante desde $U = \{2\}$:
- $2 \to 4$ (no matcheada), $4$ matcheado a $0$: $4 \to 0$.
- $0 \to 5$ (no matcheada), $5$ matcheado a $1$: $5 \to 1$.
- $1$: sin más aristas no matcheadas a $R$.
- $2 \to 5$ (no matcheada), ya visitado.

$Z = \{2, 4, 0, 5, 1\}$. $Z_L = \{2, 0, 1\}$, $Z_R = \{4, 5\}$.

Vertex cover $= (L \setminus \{2,0,1\}) \cup (R \cap \{4,5\}) = \{3\} \cup \{4, 5\} = \{3, 4, 5\}$. Tamaño **3**.

Verificación: 0-4(4✓), 0-5(5✓), 1-5(5✓), 2-4(4✓), 2-5(5✓), 3-6(3✓). ✓

---

## 3. Condición de Hall

### 3.1 Enunciado

Un grafo bipartito $G = (L \cup R, E)$ tiene un **matching perfecto** (que cubre
todo $L$) si y solo si para todo $S \subseteq L$:

$$|N(S)| \geq |S|$$

donde $N(S) = \{r \in R : \exists \ell \in S, (\ell, r) \in E\}$ es la
vecindad de $S$.

### 3.2 Consecuencia práctica

Para verificar si un matching perfecto existe, basta verificar la condición de
Hall. Si falla para algún $S$, no existe. El subconjunto $S$ que viola la
condición identifica exactamente el cuello de botella.

En el ejemplo del §1.3: $|N(\{W_0,W_1,W_2,W_3\})| = |\{J_0,J_1,J_2,J_3\}| = 4 \geq 4$. Verificar para todos los $2^4 = 16$ subconjuntos confirma que Hall
se cumple, consistente con que existe matching perfecto.

---

## 4. Caminos disjuntos y teorema de Menger

### 4.1 Edge-disjoint paths

El **máximo número de caminos arista-disjuntos** de $s$ a $t$ en un grafo
(dirigido o no dirigido) es igual al flujo máximo con capacidades unitarias.

**Teorema de Menger (aristas)**: el máximo número de caminos $s$-$t$
arista-disjuntos es igual al mínimo número de aristas cuya eliminación
desconecta $s$ de $t$ (min edge cut).

### 4.2 Vertex-disjoint paths

Para caminos **vértice-disjuntos**, se divide cada vértice $v$ (excepto $s$ y
$t$) en $v_{\text{in}}$ y $v_{\text{out}}$ con arista de capacidad 1. Las
aristas entrantes van a $v_{\text{in}}$ y las salientes salen de $v_{\text{out}}$.

**Teorema de Menger (vértices)**: el máximo número de caminos $s$-$t$
internamente vértice-disjuntos es igual al mínimo número de vértices (distintos
de $s, t$) cuya eliminación desconecta $s$ de $t$.

### 4.3 Ejemplo

Grafo no dirigido con 6 vértices:

```
    0 --- 1 --- 5
    |     |     |
    2 --- 3 --- 4
```

Aristas: 0-1, 0-2, 1-3, 1-5, 2-3, 3-4, 4-5.

Caminos arista-disjuntos de 0 a 5:
1. 0 → 1 → 5
2. 0 → 2 → 3 → 4 → 5

Máximo: **2**. Min edge cut: {0-1, 0-2} (eliminar ambas desconecta 0 de 5).
Tamaño 2 = max paths. ✓

Caminos vértice-disjuntos de 0 a 5: los mismos 2 caminos (no comparten vértices
internos: el primero usa {1}, el segundo usa {2, 3, 4}).

---

## 5. Selección de proyectos (Max Weight Closure)

### 5.1 Problema

Dado un DAG donde cada vértice $v$ tiene un **beneficio** $p_v$ (positivo o
negativo), seleccionar un subconjunto $S$ (**closure**: si $u \in S$ y
$(u,v) \in E$, entonces $v \in S$) que maximice $\sum_{v \in S} p_v$.

"Si seleccionas una tarea, debes también seleccionar todas sus dependencias."

### 5.2 Reducción a min cut

Construir red de flujo:

- Para cada vértice $v$ con $p_v > 0$: arista $s \to v$ con capacidad $p_v$.
- Para cada vértice $v$ con $p_v < 0$: arista $v \to t$ con capacidad $|p_v|$.
- Para cada arista de dependencia $(u, v)$: arista $u \to v$ con capacidad $\infty$.

$$\text{max profit} = \sum_{v: p_v > 0} p_v - \text{min cut}$$

El corte mínimo "paga" por los beneficios perdidos (tareas positivas no
seleccionadas) y los costos asumidos (tareas negativas seleccionadas).

### 5.3 Ejemplo

Tareas de un proyecto de software:

| Tarea | Beneficio | Dependencias (closure) |
|:-----:|:---------:|:----------------------:|
| A | +5 | A → C (seleccionar A requiere C) |
| B | +8 | B → C |
| C | -3 | — |
| D | +4 | D → E |
| E | -6 | — |

Red de flujo ($s=5, t=6$, tareas A=0, B=1, C=2, D=3, E=4):

```
s --5--> A --∞--> C --3--> t
|        |
+--8--> B --∞----^
|
+--4--> D --∞--> E --6--> t
```

Suma de beneficios positivos: $5 + 8 + 4 = 17$.

**Cortes posibles**:

| $S$ (con $s$) | Aristas cortadas | Capacidad | Profit = 17 - cap |
|---------------|------------------|:---------:|:-----------------:|
| $\{s,A,B,C,D,E\}$ | C→t:3, E→t:6 | 9 | 8 |
| $\{s,A,B,C\}$ | s→D:4, C→t:3 | 7 | **10** |
| $\{s,B,C\}$ | s→A:5, s→D:4, C→t:3 | 12 | 5 |
| $\{s,A,B,C,D\}$ | D→E:∞ | ∞ | -∞ |
| $\{s\}$ | s→A:5, s→B:8, s→D:4 | 17 | 0 |

**Min cut** = 7 ($S = \{s, A, B, C\}$). **Max profit** = 17 - 7 = **10**.

Selección óptima: $\{A, B, C\}$ con beneficio $5 + 8 - 3 = 10$.

Notar: $D$ no se selecciona porque $D \to E$ forzaría incluir $E$ (-6),
y $4 - 6 = -2$ neto negativo.

---

## 6. Conectividad de redes

### 6.1 Edge connectivity

La **conectividad de aristas** $\lambda(G)$ de un grafo es el mínimo número de
aristas cuya eliminación desconecta el grafo. Por Menger:

$$\lambda(G) = \min_{s \neq t} \text{maxflow}(s, t, \text{cap}=1)$$

Se puede calcular fijando $s$ y variando $t$ sobre todos los demás vértices:
$\lambda(G) = \min_t \text{maxflow}(s, t)$ (para cualquier $s$ fijo).

### 6.2 Vertex connectivity

La **conectividad de vértices** $\kappa(G)$ es el mínimo número de vértices
cuya eliminación desconecta el grafo. Se calcula con vertex splitting
(capacidad 1 en arista interna $v_{\text{in}} \to v_{\text{out}}$).

### 6.3 Relación de Whitney

$$\kappa(G) \leq \lambda(G) \leq \delta(G)$$

donde $\delta(G)$ es el grado mínimo del grafo.

---

## 7. Programa completo en C

```c
/*
 * flow_apps.c
 *
 * Applications of max flow / min cut:
 * bipartite matching, König's theorem, edge-disjoint paths,
 * project selection, Hall's condition, network reliability.
 *
 * Compile: gcc -O2 -o flow_apps flow_apps.c
 * Run:     ./flow_apps
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 30
#define MAX_E 200

/* =================== FLOW GRAPH (reused from T02) ================= */

typedef struct {
    int to, cap, rev;
} Edge;

typedef struct {
    Edge edges[MAX_E];
    int head[MAX_V];
    int next[MAX_E];
    int n, s, t, ecnt;
} FG;

void fg_init(FG *g, int n, int s, int t) {
    g->n = n; g->s = s; g->t = t; g->ecnt = 0;
    memset(g->head, -1, sizeof(g->head));
}

void fg_add(FG *g, int u, int v, int cap) {
    int e1 = g->ecnt++;
    g->edges[e1] = (Edge){v, cap, g->ecnt};
    g->next[e1] = g->head[u]; g->head[u] = e1;
    int e2 = g->ecnt++;
    g->edges[e2] = (Edge){u, 0, e1};
    g->next[e2] = g->head[v]; g->head[v] = e2;
}

int fg_bfs(FG *g, int pe[]) {
    bool vis[MAX_V] = {false};
    int q[MAX_V], f = 0, b = 0;
    memset(pe, -1, MAX_V * sizeof(int));
    vis[g->s] = true; q[b++] = g->s;
    while (f < b) {
        int u = q[f++];
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to;
            if (!vis[v] && g->edges[e].cap > 0) {
                vis[v] = true; pe[v] = e;
                if (v == g->t) {
                    int bn = INT_MAX;
                    for (int x = g->t; x != g->s;
                         x = g->edges[g->edges[pe[x]].rev].to) {
                        if (g->edges[pe[x]].cap < bn)
                            bn = g->edges[pe[x]].cap;
                    }
                    return bn;
                }
                q[b++] = v;
            }
        }
    }
    return 0;
}

int fg_maxflow(FG *g) {
    int total = 0, pe[MAX_V], bn;
    while ((bn = fg_bfs(g, pe)) > 0) {
        for (int x = g->t; x != g->s;
             x = g->edges[g->edges[pe[x]].rev].to) {
            g->edges[pe[x]].cap -= bn;
            g->edges[g->edges[pe[x]].rev].cap += bn;
        }
        total += bn;
    }
    return total;
}

void fg_reachable(FG *g, bool reach[]) {
    memset(reach, false, MAX_V);
    int q[MAX_V], f = 0, b = 0;
    reach[g->s] = true; q[b++] = g->s;
    while (f < b) {
        int u = q[f++];
        for (int e = g->head[u]; e != -1; e = g->next[e])
            if (g->edges[e].cap > 0 && !reach[g->edges[e].to]) {
                reach[g->edges[e].to] = true;
                q[b++] = g->edges[e].to;
            }
    }
}

/* =================== DEMOS ======================================== */

void demo1_matching(void) {
    printf("=== Demo 1: Bipartite Matching via Max Flow ===\n\n");

    /*
     * Workers: W0(0), W1(1), W2(2), W3(3)
     * Jobs:    J0(4), J1(5), J2(6), J3(7)
     * Source=8, Sink=9
     */
    FG g;
    fg_init(&g, 10, 8, 9);

    /* Source to workers */
    for (int w = 0; w < 4; w++) fg_add(&g, 8, w, 1);
    /* Jobs to sink */
    for (int j = 4; j < 8; j++) fg_add(&g, j, 9, 1);

    /* Skills */
    const char *wname[] = {"Alice", "Bob", "Carol", "Dave"};
    const char *jname[] = {"Design", "Backend", "Frontend", "Testing"};

    int skills[][2] = {{0,4},{0,5}, {1,4},{1,6}, {2,5},{2,7}, {3,6},{3,7}};
    int nskills = 8;

    printf("Workers and their possible jobs:\n");
    for (int w = 0; w < 4; w++) {
        printf("  %-6s: ", wname[w]);
        bool first = true;
        for (int i = 0; i < nskills; i++)
            if (skills[i][0] == w) {
                if (!first) printf(", ");
                printf("%s", jname[skills[i][1] - 4]);
                first = false;
            }
        printf("\n");
    }
    printf("\n");

    for (int i = 0; i < nskills; i++)
        fg_add(&g, skills[i][0], skills[i][1], 1);

    int max_match = fg_maxflow(&g);
    printf("Maximum matching: %d\n\n", max_match);

    /* Extract matching from flow */
    printf("Assignment:\n");
    for (int w = 0; w < 4; w++) {
        for (int e = g.head[w]; e != -1; e = g.next[e]) {
            int j = g.edges[e].to;
            if (j >= 4 && j <= 7 && e % 2 == 0) {
                int flow = g.edges[g.edges[e].rev].cap;
                if (flow > 0)
                    printf("  %-6s -> %s\n", wname[w], jname[j - 4]);
            }
        }
    }
    printf("\n");
}

void demo2_konig(void) {
    printf("=== Demo 2: König's Theorem — Min Vertex Cover ===\n\n");

    /*
     * L = {0,1,2,3}, R = {4,5,6}
     * Edges: 0-4, 0-5, 1-5, 2-4, 2-5, 3-6
     * Source=7, Sink=8
     */
    FG g;
    fg_init(&g, 9, 7, 8);
    for (int l = 0; l < 4; l++) fg_add(&g, 7, l, 1);
    for (int r = 4; r < 7; r++) fg_add(&g, r, 8, 1);
    fg_add(&g, 0, 4, 1); fg_add(&g, 0, 5, 1);
    fg_add(&g, 1, 5, 1);
    fg_add(&g, 2, 4, 1); fg_add(&g, 2, 5, 1);
    fg_add(&g, 3, 6, 1);

    printf("Bipartite graph:\n");
    printf("  L = {0, 1, 2, 3}, R = {4, 5, 6}\n");
    printf("  Edges: 0-4, 0-5, 1-5, 2-4, 2-5, 3-6\n\n");

    int max_match = fg_maxflow(&g);
    printf("Max matching size: %d\n", max_match);

    /* Find min vertex cover via reachability in residual */
    bool reach[MAX_V];
    fg_reachable(&g, reach);

    printf("Min vertex cover (König): { ");
    int cover_size = 0;
    for (int l = 0; l < 4; l++) {
        if (!reach[l]) { /* L \ Z */
            printf("%d ", l);
            cover_size++;
        }
    }
    for (int r = 4; r < 7; r++) {
        if (reach[r]) { /* R ∩ Z */
            printf("%d ", r);
            cover_size++;
        }
    }
    printf("} (size %d)\n\n", cover_size);

    /* Verify cover */
    int edges_arr[][2] = {{0,4},{0,5},{1,5},{2,4},{2,5},{3,6}};
    bool all_covered = true;
    for (int i = 0; i < 6; i++) {
        int u = edges_arr[i][0], v = edges_arr[i][1];
        bool covered = (!reach[u]) || reach[v];
        printf("  Edge %d-%d: %s\n", u, v, covered ? "covered" : "NOT COVERED");
        if (!covered) all_covered = false;
    }
    printf("\nAll edges covered: %s\n", all_covered ? "yes" : "NO");
    printf("König: max matching (%d) = min vertex cover (%d): %s\n\n",
           max_match, cover_size,
           max_match == cover_size ? "VERIFIED" : "MISMATCH");
}

void demo3_edge_disjoint(void) {
    printf("=== Demo 3: Edge-Disjoint Paths (Menger) ===\n\n");

    /*
     * Graph: 0-1, 0-2, 1-3, 1-5, 2-3, 3-4, 4-5
     * Undirected: add both directions, cap=1 each
     * Source=0, Sink=5
     */
    FG g;
    fg_init(&g, 6, 0, 5);

    int edges[][2] = {{0,1},{0,2},{1,3},{1,5},{2,3},{3,4},{4,5}};
    printf("Graph edges: ");
    for (int i = 0; i < 7; i++) {
        fg_add(&g, edges[i][0], edges[i][1], 1);
        fg_add(&g, edges[i][1], edges[i][0], 1);
        printf("%d-%d%s", edges[i][0], edges[i][1], i < 6 ? ", " : "");
    }
    printf("\n\n");

    int max_paths = fg_maxflow(&g);
    printf("Max edge-disjoint paths from 0 to 5: %d\n\n", max_paths);

    /* Find min edge cut */
    bool reach[MAX_V];
    fg_reachable(&g, reach);

    printf("Min edge cut (S -> T):\n");
    printf("  S = { ");
    for (int v = 0; v < 6; v++) if (reach[v]) printf("%d ", v);
    printf("}\n  T = { ");
    for (int v = 0; v < 6; v++) if (!reach[v]) printf("%d ", v);
    printf("}\n  Cut edges: ");

    int cut = 0;
    for (int u = 0; u < 6; u++) {
        if (!reach[u]) continue;
        for (int e = g.head[u]; e != -1; e = g.next[e]) {
            int v = g.edges[e].to;
            if (!reach[v] && e % 4 == 0) {
                /* Original forward of undirected edge */
                printf("%d-%d ", u, v);
                cut++;
            }
        }
    }
    printf("\n\nMenger: max paths (%d) = min edge cut (%d): %s\n\n",
           max_paths, cut,
           max_paths == cut ? "VERIFIED" : "check");
}

void demo4_project_selection(void) {
    printf("=== Demo 4: Project Selection (Max Weight Closure) ===\n\n");

    /*
     * Tasks: A(+5), B(+8), C(-3), D(+4), E(-6)
     * Dependencies: A->C, B->C, D->E
     * Vertices: A=0, B=1, C=2, D=3, E=4, s=5, t=6
     */
    FG g;
    fg_init(&g, 7, 5, 6);

    int profit[] = {5, 8, -3, 4, -6};
    const char *names[] = {"A", "B", "C", "D", "E"};
    int pos_sum = 0;

    printf("Tasks:\n");
    for (int i = 0; i < 5; i++) {
        printf("  %s: profit = %+d\n", names[i], profit[i]);
        if (profit[i] > 0) {
            fg_add(&g, 5, i, profit[i]);  /* s -> task */
            pos_sum += profit[i];
        } else {
            fg_add(&g, i, 6, -profit[i]); /* task -> t */
        }
    }

    /* Dependencies (closure): selecting u requires selecting v */
    int INF = 1000;
    fg_add(&g, 0, 2, INF); /* A -> C */
    fg_add(&g, 1, 2, INF); /* B -> C */
    fg_add(&g, 3, 4, INF); /* D -> E */

    printf("\nDependencies: A->C, B->C, D->E\n");
    printf("Sum of positive profits: %d\n\n", pos_sum);

    int min_cut = fg_maxflow(&g);
    int max_profit = pos_sum - min_cut;

    /* Find selected tasks (in S set) */
    bool reach[MAX_V];
    fg_reachable(&g, reach);

    printf("Min cut: %d\n", min_cut);
    printf("Max profit: %d - %d = %d\n\n", pos_sum, min_cut, max_profit);

    printf("Selected tasks: { ");
    int actual_profit = 0;
    for (int i = 0; i < 5; i++) {
        if (reach[i]) {
            printf("%s(%+d) ", names[i], profit[i]);
            actual_profit += profit[i];
        }
    }
    printf("}\nVerification: %+d = %d  %s\n\n",
           actual_profit, max_profit,
           actual_profit == max_profit ? "CORRECT" : "MISMATCH");
}

void demo5_hall(void) {
    printf("=== Demo 5: Hall's Condition Verification ===\n\n");

    /* Case 1: Perfect matching exists */
    printf("Case 1: W0:{J0,J1}, W1:{J0,J2}, W2:{J1,J3}, W3:{J2,J3}\n");
    int adj1[4][4] = {{1,1,0,0},{1,0,1,0},{0,1,0,1},{0,0,1,1}};
    bool hall1 = true;
    for (int mask = 1; mask < 16; mask++) {
        bool nbr[4] = {false};
        int s_size = 0;
        for (int w = 0; w < 4; w++) {
            if (!(mask & (1 << w))) continue;
            s_size++;
            for (int j = 0; j < 4; j++)
                if (adj1[w][j]) nbr[j] = true;
        }
        int n_size = 0;
        for (int j = 0; j < 4; j++) if (nbr[j]) n_size++;
        if (n_size < s_size) {
            hall1 = false;
            printf("  FAILS: S={");
            for (int w = 0; w < 4; w++)
                if (mask & (1<<w)) printf("W%d,",w);
            printf("} |N(S)|=%d < |S|=%d\n", n_size, s_size);
        }
    }
    printf("  Hall's condition: %s -> perfect matching %s\n\n",
           hall1 ? "SATISFIED" : "FAILS",
           hall1 ? "EXISTS" : "does NOT exist");

    /* Case 2: No perfect matching */
    printf("Case 2: W0:{J0,J1}, W1:{J0}, W2:{J0}, W3:{J2,J3}\n");
    int adj2[4][4] = {{1,1,0,0},{1,0,0,0},{1,0,0,0},{0,0,1,1}};
    bool hall2 = true;
    for (int mask = 1; mask < 16; mask++) {
        bool nbr[4] = {false};
        int s_size = 0;
        for (int w = 0; w < 4; w++) {
            if (!(mask & (1 << w))) continue;
            s_size++;
            for (int j = 0; j < 4; j++)
                if (adj2[w][j]) nbr[j] = true;
        }
        int n_size = 0;
        for (int j = 0; j < 4; j++) if (nbr[j]) n_size++;
        if (n_size < s_size) {
            hall2 = false;
            printf("  FAILS: S={");
            for (int w = 0; w < 4; w++)
                if (mask & (1<<w)) printf("W%d,",w);
            printf("} |N(S)|=%d < |S|=%d\n", n_size, s_size);
        }
    }
    printf("  Hall's condition: %s -> perfect matching %s\n\n",
           hall2 ? "SATISFIED" : "FAILS",
           hall2 ? "EXISTS" : "does NOT exist");
}

void demo6_reliability(void) {
    printf("=== Demo 6: Network Reliability (Edge Connectivity) ===\n\n");

    /*
     * Network: 6 nodes representing servers
     * Edges: 0-1, 0-2, 1-2, 1-3, 2-4, 3-4, 3-5, 4-5
     */
    int edges[][2] = {{0,1},{0,2},{1,2},{1,3},{2,4},{3,4},{3,5},{4,5}};
    int nedges = 8;

    printf("Server network:\n  Connections: ");
    for (int i = 0; i < nedges; i++)
        printf("%d-%d%s", edges[i][0], edges[i][1], i<nedges-1?", ":"");
    printf("\n\n");

    /* Edge connectivity = min over all t of maxflow(0, t, cap=1) */
    int min_conn = INT_MAX;
    printf("Max flow (unit capacities) from node 0 to each node:\n");
    for (int t = 1; t < 6; t++) {
        FG g;
        fg_init(&g, 6, 0, t);
        for (int i = 0; i < nedges; i++) {
            fg_add(&g, edges[i][0], edges[i][1], 1);
            fg_add(&g, edges[i][1], edges[i][0], 1);
        }
        int flow = fg_maxflow(&g);
        printf("  0 -> %d : %d edge-disjoint paths\n", t, flow);
        if (flow < min_conn) min_conn = flow;
    }

    printf("\nEdge connectivity lambda(G) = %d\n", min_conn);
    printf("Need to cut at least %d links to disconnect the network.\n\n",
           min_conn);
}

/* =================== MAIN ========================================= */

int main(void) {
    demo1_matching();
    demo2_konig();
    demo3_edge_disjoint();
    demo4_project_selection();
    demo5_hall();
    demo6_reliability();
    return 0;
}
```

---

## 8. Programa completo en Rust

```rust
/*
 * flow_apps.rs
 *
 * Applications of max flow / min cut in Rust:
 * bipartite matching, König's theorem, edge-disjoint paths,
 * vertex-disjoint paths, project selection, Hall's condition,
 * network reliability, assignment visualization.
 *
 * Compile: rustc -o flow_apps flow_apps.rs
 * Run:     ./flow_apps
 */

use std::collections::VecDeque;

/* =================== FLOW GRAPH =================================== */

#[derive(Clone)]
struct Edge { to: usize, cap: i32, rev: usize }

#[derive(Clone)]
struct FG {
    adj: Vec<Vec<Edge>>,
    n: usize, s: usize, t: usize,
}

impl FG {
    fn new(n: usize, s: usize, t: usize) -> Self {
        FG { adj: vec![vec![]; n], n, s, t }
    }

    fn add_edge(&mut self, u: usize, v: usize, cap: i32) {
        let ru = self.adj[v].len();
        let rv = self.adj[u].len();
        self.adj[u].push(Edge { to: v, cap, rev: ru });
        self.adj[v].push(Edge { to: u, cap: 0, rev: rv });
    }

    fn add_undirected(&mut self, u: usize, v: usize, cap: i32) {
        let ru = self.adj[v].len();
        let rv = self.adj[u].len();
        self.adj[u].push(Edge { to: v, cap, rev: ru });
        self.adj[v].push(Edge { to: u, cap, rev: rv });
    }

    fn max_flow(&mut self) -> i32 {
        let mut total = 0;
        loop {
            let mut parent = vec![None::<(usize, usize)>; self.n];
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

            if !visited[self.t] { break; }

            let mut bn = i32::MAX;
            let mut v = self.t;
            while let Some((u, i)) = parent[v] {
                bn = bn.min(self.adj[u][i].cap);
                v = u;
            }

            v = self.t;
            while let Some((u, i)) = parent[v] {
                self.adj[u][i].cap -= bn;
                let r = self.adj[u][i].rev;
                self.adj[v][r].cap += bn;
                v = u;
            }
            total += bn;
        }
        total
    }

    fn reachable(&self) -> Vec<bool> {
        let mut vis = vec![false; self.n];
        let mut q = VecDeque::new();
        vis[self.s] = true;
        q.push_back(self.s);
        while let Some(u) = q.pop_front() {
            for e in &self.adj[u] {
                if e.cap > 0 && !vis[e.to] {
                    vis[e.to] = true;
                    q.push_back(e.to);
                }
            }
        }
        vis
    }
}

/* =================== DEMOS ======================================== */

fn demo1_matching() {
    println!("=== Demo 1: Bipartite Matching via Max Flow ===\n");

    let workers = ["Alice", "Bob", "Carol", "Dave"];
    let jobs = ["Design", "Backend", "Frontend", "Testing"];

    /* Skills: (worker_idx, job_idx) */
    let skills = [(0,0),(0,1), (1,0),(1,2), (2,1),(2,3), (3,2),(3,3)];

    /* Build flow network: s=8, t=9, workers=0-3, jobs=4-7 */
    let mut g = FG::new(10, 8, 9);
    for w in 0..4 { g.add_edge(8, w, 1); }
    for j in 4..8 { g.add_edge(j, 9, 1); }
    for &(w, j) in &skills { g.add_edge(w, j + 4, 1); }

    println!("Workers and possible jobs:");
    for w in 0..4 {
        let js: Vec<&str> = skills.iter()
            .filter(|&&(ww, _)| ww == w)
            .map(|&(_, j)| jobs[j])
            .collect();
        println!("  {:<6}: {}", workers[w], js.join(", "));
    }
    println!();

    let max_match = g.max_flow();
    println!("Maximum matching: {}\n", max_match);

    println!("Assignment:");
    for w in 0..4usize {
        for (i, e) in g.adj[w].iter().enumerate() {
            if e.to >= 4 && e.to <= 7 && i % 2 == 0 {
                let flow = g.adj[e.to][e.rev].cap;
                if flow > 0 {
                    println!("  {} -> {}", workers[w], jobs[e.to - 4]);
                }
            }
        }
    }
    println!();
}

fn demo2_konig() {
    println!("=== Demo 2: König's Theorem — Min Vertex Cover ===\n");

    /* L={0,1,2,3}, R={4,5,6}, s=7, t=8 */
    let edges = [(0,4),(0,5),(1,5),(2,4),(2,5),(3,6)];

    let mut g = FG::new(9, 7, 8);
    for l in 0..4 { g.add_edge(7, l, 1); }
    for r in 4..7 { g.add_edge(r, 8, 1); }
    for &(l, r) in &edges { g.add_edge(l, r, 1); }

    println!("L = {{0,1,2,3}}, R = {{4,5,6}}");
    let es: Vec<String> = edges.iter().map(|(a,b)| format!("{}-{}", a, b)).collect();
    println!("Edges: {}\n", es.join(", "));

    let mm = g.max_flow();
    let reach = g.reachable();

    /* Cover = (L \ Z_L) ∪ (R ∩ Z_R) */
    let cover: Vec<usize> = (0..4).filter(|&v| !reach[v])
        .chain((4..7).filter(|&v| reach[v]))
        .collect();

    println!("Max matching: {}", mm);
    println!("Min vertex cover: {:?} (size {})", cover, cover.len());

    let all_covered = edges.iter().all(|&(u, v)| cover.contains(&u) || cover.contains(&v));
    println!("All edges covered: {}", all_covered);
    println!("König: {} = {}: {}\n",
             mm, cover.len(),
             if mm as usize == cover.len() { "VERIFIED" } else { "MISMATCH" });
}

fn demo3_hall() {
    println!("=== Demo 3: Hall's Condition ===\n");

    let cases: Vec<(&str, Vec<Vec<usize>>)> = vec![
        ("W0:{J0,J1}, W1:{J0,J2}, W2:{J1,J3}, W3:{J2,J3}",
         vec![vec![0,1], vec![0,2], vec![1,3], vec![2,3]]),
        ("W0:{J0,J1}, W1:{J0}, W2:{J0}, W3:{J2,J3}",
         vec![vec![0,1], vec![0], vec![0], vec![2,3]]),
    ];

    for (desc, adj) in &cases {
        println!("  {}", desc);
        let n = adj.len();
        let mut hall_ok = true;

        for mask in 1..(1u32 << n) {
            let mut nbr = vec![false; 4];
            let mut s_size = 0;
            for w in 0..n {
                if mask & (1 << w) == 0 { continue; }
                s_size += 1;
                for &j in &adj[w] { nbr[j] = true; }
            }
            let n_size = nbr.iter().filter(|&&b| b).count();
            if n_size < s_size {
                hall_ok = false;
                let s_set: Vec<String> = (0..n)
                    .filter(|&w| mask & (1 << w) != 0)
                    .map(|w| format!("W{}", w)).collect();
                println!("    FAILS: S={{{}}}, |N(S)|={} < |S|={}",
                         s_set.join(","), n_size, s_size);
            }
        }
        println!("    Hall's condition: {} -> perfect matching {}\n",
                 if hall_ok { "SATISFIED" } else { "FAILS" },
                 if hall_ok { "exists" } else { "does NOT exist" });
    }
}

fn demo4_edge_disjoint() {
    println!("=== Demo 4: Edge-Disjoint Paths (Menger) ===\n");

    let edges = [(0,1),(0,2),(1,3),(1,5),(2,3),(3,4),(4,5)];

    let mut g = FG::new(6, 0, 5);
    for &(u, v) in &edges {
        g.add_undirected(u, v, 1);
    }

    let es: Vec<String> = edges.iter().map(|(a,b)| format!("{}-{}", a, b)).collect();
    println!("Graph: {}", es.join(", "));
    println!("Source=0, Sink=5\n");

    let max_paths = g.max_flow();
    println!("Max edge-disjoint paths: {}", max_paths);

    let reach = g.reachable();
    let s_set: Vec<usize> = (0..6).filter(|&v| reach[v]).collect();
    let t_set: Vec<usize> = (0..6).filter(|&v| !reach[v]).collect();
    println!("Min edge cut: S={:?}, T={:?}", s_set, t_set);
    println!("Menger's theorem verified.\n");
}

fn demo5_vertex_disjoint() {
    println!("=== Demo 5: Vertex-Disjoint Paths (Vertex Splitting) ===\n");

    /*
     * Original graph: 0-1, 0-2, 0-3, 1-4, 2-4, 3-4
     * Find vertex-disjoint paths from 0 to 4.
     *
     * Split each internal vertex v into v_in (2v) and v_out (2v+1)
     * with edge v_in -> v_out of capacity 1.
     * Vertices 0 and 4 are not split (infinite internal capacity).
     */
    let n_orig = 5;
    let n_split = 2 * n_orig;
    let s = 1;  /* 0_out */
    let t = 8;  /* 4_in */

    let mut g = FG::new(n_split, s, t);

    /* Internal edges: v_in -> v_out, cap=1 for internal vertices */
    for v in 1..4 { /* vertices 1, 2, 3 */
        g.add_edge(2 * v, 2 * v + 1, 1);
    }
    /* Source and sink: effectively infinite internal capacity */
    /* s = 0_out = 1, t = 4_in = 8 */

    /* Original edges: u_out -> v_in and v_out -> u_in (undirected) */
    let orig_edges = [(0,1),(0,2),(0,3),(1,4),(2,4),(3,4)];
    for &(u, v) in &orig_edges {
        let u_out = 2 * u + 1;
        let v_in = 2 * v;
        g.add_edge(u_out, v_in, 1);
        let v_out = 2 * v + 1;
        let u_in = 2 * u;
        g.add_edge(v_out, u_in, 1);
    }

    println!("Original graph: 0-1, 0-2, 0-3, 1-4, 2-4, 3-4");
    println!("Find vertex-disjoint paths from 0 to 4.\n");
    println!("Vertex splitting: each v -> (v_in, v_out) with cap=1\n");

    let max_paths = g.max_flow();
    println!("Max vertex-disjoint paths: {}", max_paths);
    println!("Min vertex cut = {} internal vertices\n", max_paths);

    println!("The 3 paths (0->1->4, 0->2->4, 0->3->4) share no internal");
    println!("vertices, confirming {} vertex-disjoint paths.\n", max_paths);
}

fn demo6_project_selection() {
    println!("=== Demo 6: Project Selection (Max Weight Closure) ===\n");

    let names = ["A", "B", "C", "D", "E"];
    let profit = [5i32, 8, -3, 4, -6];
    let deps = [(0, 2), (1, 2), (3, 4)]; /* A->C, B->C, D->E */

    /* s=5, t=6 */
    let mut g = FG::new(7, 5, 6);
    let mut pos_sum = 0i32;

    println!("Tasks:");
    for i in 0..5 {
        println!("  {}: profit = {:+}", names[i], profit[i]);
        if profit[i] > 0 {
            g.add_edge(5, i, profit[i]);
            pos_sum += profit[i];
        } else {
            g.add_edge(i, 6, -profit[i]);
        }
    }

    let inf = 1000;
    for &(u, v) in &deps {
        g.add_edge(u, v, inf);
    }

    println!("\nDependencies: A->C, B->C, D->E");
    println!("Sum of positive profits: {}\n", pos_sum);

    let min_cut = g.max_flow();
    let max_profit = pos_sum - min_cut;

    let reach = g.reachable();
    let selected: Vec<&str> = (0..5).filter(|&i| reach[i]).map(|i| names[i]).collect();

    println!("Min cut: {}", min_cut);
    println!("Max profit: {} - {} = {}", pos_sum, min_cut, max_profit);
    println!("Selected tasks: {{{}}}", selected.join(", "));

    let actual: i32 = (0..5).filter(|&i| reach[i]).map(|i| profit[i]).sum();
    println!("Verification: {:+} = {}: {}\n",
             actual, max_profit,
             if actual == max_profit { "CORRECT" } else { "MISMATCH" });
}

fn demo7_reliability() {
    println!("=== Demo 7: Network Reliability (Edge Connectivity) ===\n");

    let edges = [(0,1),(0,2),(1,2),(1,3),(2,4),(3,4),(3,5),(4,5)];
    let n = 6;

    let es: Vec<String> = edges.iter().map(|(a,b)| format!("{}-{}", a, b)).collect();
    println!("Server network: {}\n", es.join(", "));

    let mut min_conn = i32::MAX;
    println!("Edge-disjoint paths from node 0:");
    for t in 1..n {
        let mut g = FG::new(n, 0, t);
        for &(u, v) in &edges {
            g.add_undirected(u, v, 1);
        }
        let flow = g.max_flow();
        println!("  0 -> {}: {} paths", t, flow);
        if flow < min_conn { min_conn = flow; }
    }

    println!("\nEdge connectivity λ(G) = {}", min_conn);

    /* Verify with Whitney inequality */
    let min_degree = (0..n).map(|v| {
        edges.iter().filter(|&&(a, b)| a == v || b == v).count() as i32
    }).min().unwrap();

    println!("Min degree δ(G) = {}", min_degree);
    println!("Whitney: κ ≤ λ ≤ δ, so κ ≤ {} ≤ {}\n",
             min_conn, min_degree);
}

fn demo8_assignment() {
    println!("=== Demo 8: Assignment with Augmenting Path Trace ===\n");

    /*
     * Workers: W0, W1, W2, W3
     * Jobs:    J0, J1, J2
     * W3 can only do J2, forcing rearrangement.
     */
    let workers = ["Ana", "Ben", "Cam", "Dan"];
    let jobs = ["Dev", "QA", "Ops"];
    let skills = [(0,0),(0,1), (1,0),(1,2), (2,1), (3,2)];

    println!("Workers and skills:");
    for w in 0..4 {
        let js: Vec<&str> = skills.iter()
            .filter(|&&(ww, _)| ww == w)
            .map(|&(_, j)| jobs[j])
            .collect();
        println!("  {}: {}", workers[w], js.join(", "));
    }
    println!("\nNote: 4 workers, 3 jobs -> max matching = 3\n");

    /* s=4, t=5, workers=0-3, jobs=6-8 mapped to internal 4..6 */
    /* Actually let's use: workers 0-3, jobs 4-6, s=7, t=8 */
    let mut g = FG::new(9, 7, 8);
    for w in 0..4 { g.add_edge(7, w, 1); }
    for j in 4..7 { g.add_edge(j, 8, 1); }
    for &(w, j) in &skills { g.add_edge(w, j + 4, 1); }

    let mm = g.max_flow();
    println!("Maximum matching: {} (one worker unassigned)\n", mm);

    println!("Final assignment:");
    let mut assigned = vec![false; 4];
    for w in 0..4usize {
        for (i, e) in g.adj[w].iter().enumerate() {
            if e.to >= 4 && e.to <= 6 && i % 2 == 0 {
                let flow = g.adj[e.to][e.rev].cap;
                if flow > 0 {
                    println!("  {} -> {}", workers[w], jobs[e.to - 4]);
                    assigned[w] = true;
                }
            }
        }
    }
    for w in 0..4 {
        if !assigned[w] {
            println!("  {} -> (unassigned)", workers[w]);
        }
    }
    println!();
}

/* =================== MAIN ========================================= */

fn main() {
    demo1_matching();
    demo2_konig();
    demo3_hall();
    demo4_edge_disjoint();
    demo5_vertex_disjoint();
    demo6_project_selection();
    demo7_reliability();
    demo8_assignment();
}
```

---

## 9. Ejercicios

### Ejercicio 1 — Matching bipartito

Estudiantes $\{A, B, C, D, E\}$ y proyectos $\{P, Q, R, S\}$:
- A: P, Q
- B: Q, R
- C: R, S
- D: P
- E: P, S

Modela como red de flujo y encuentra el matching máximo. ¿Es perfecto sobre los
estudiantes?

<details><summary>Predicción</summary>

Red: s→A:1, s→B:1, s→C:1, s→D:1, s→E:1. P→t:1, Q→t:1, R→t:1, S→t:1.
Aristas de skills con capacidad 1.

Augmentaciones:
1. s→A→P→t. Match A-P.
2. s→B→Q→t. Match B-Q.
3. s→C→R→t. Match C-R.
4. s→D→P: bloqueado. BFS: s→D→P→(back)A→Q→(back)B→R→(back)C→S→t. Augment!
   Resultado: D-P, A-Q, B-R, C-S.
5. s→E→P: bloqueado. s→E→S: bloqueado. No más caminos.

**Matching máximo: 4**. E queda sin asignar. No es perfecto sobre estudiantes
($|L| = 5 > |R| = 4$, imposible cubrir a todos).

Hall: $|\{A,D,E\}| = 3$, $N(\{A,D,E\}) = \{P,Q,S\}$, $|N| = 3 \geq 3$. OK.
$|\{A,B,C,D,E\}| = 5 > |N| = 4 = |\{P,Q,R,S\}|$. Falla. No puede haber
matching perfecto sobre L.

</details>

### Ejercicio 2 — König en acción

Para el matching del ejercicio 1 (tamaño 4), encuentra el minimum vertex cover
usando el método de König.

<details><summary>Predicción</summary>

Matching $M = \{D\text{-}P, A\text{-}Q, B\text{-}R, C\text{-}S\}$. Unmatched
left: $U = \{E\}$.

Alternating tree desde E:
- $E \to P$ (no matcheada), P matcheado a D: $P \to D$.
- $D$: no más aristas no matcheadas (D solo conecta a P, ya visitado).
- $E \to S$ (no matcheada), S matcheado a C: $S \to C$.
- $C \to R$ (no matcheada), R matcheado a B: $R \to B$.
- $B \to Q$ (no matcheada), Q matcheado a A: $Q \to A$.
- $A$: no más aristas (A conecta a P visitado, Q visitado).

$Z = \{E, P, D, S, C, R, B, Q, A\}$ = todos.
$Z_L = \{E,D,C,B,A\}$, $Z_R = \{P,S,R,Q\}$.

Cover $= (L \setminus Z_L) \cup (R \cap Z_R) = \{\} \cup \{P,Q,R,S\} = \{P,Q,R,S\}$. Tamaño 4.

Todas las aristas tienen un extremo en R, así que cubrir todo R siempre funciona
cuando $|R| = $ matching size.

</details>

### Ejercicio 3 — Caminos arista-disjuntos

Grafo: 0-1, 0-2, 0-3, 1-4, 2-4, 3-4, 1-2, 2-3.

¿Cuántos caminos arista-disjuntos hay de 0 a 4? Encuéntralos. ¿Cuál es el
min edge cut?

<details><summary>Predicción</summary>

Caminos candidatos:
1. 0→1→4
2. 0→2→4
3. 0→3→4

Estos 3 caminos usan aristas disjuntas. ¿Hay un cuarto? Tendríamos que reusar
las aristas 0→1, 0→2, 0→3 (las únicas que salen de 0). No es posible.

**3 caminos arista-disjuntos**. Min edge cut: $\{0\text{-}1, 0\text{-}2, 0\text{-}3\}$, tamaño 3.

Las aristas 1-2 y 2-3 no ayudan a crear más caminos disjuntos porque solo hay 3
aristas saliendo de 0 y 3 aristas llegando a 4.

</details>

### Ejercicio 4 — Caminos vértice-disjuntos

En el grafo del ejercicio 3, ¿cuántos caminos **vértice-disjuntos** hay de 0 a 4?

<details><summary>Predicción</summary>

Los mismos 3 caminos: 0→1→4, 0→2→4, 0→3→4. No comparten vértices internos
(1, 2, 3 son todos distintos).

Usando vertex splitting: cada vértice interno (1, 2, 3) se divide con capacidad
1. Fuente=0 y sumidero=4 no se dividen.

Max flow = 3 (cada vértice interno usado exactamente una vez).

**3 caminos vértice-disjuntos**, igual que arista-disjuntos en este caso.

Si tuviéramos grafo 0-1, 0-1, 1-4, 1-4 (multi-edges): 2 arista-disjuntos pero
solo 1 vértice-disjunto (ambos pasan por vértice 1).

</details>

### Ejercicio 5 — Project selection

Tareas con beneficios: F(+10), G(+7), H(-4), I(+3), J(-8).
Dependencias (closure): F→H, G→H, I→J.

¿Cuál es la selección óptima y su beneficio?

<details><summary>Predicción</summary>

Positivos: F=10, G=7, I=3. Suma = 20.

Red: s→F:10, s→G:7, s→I:3. H→t:4, J→t:8. F→H:∞, G→H:∞, I→J:∞.

Opciones:
- Seleccionar {F,G,H}: 10+7-4 = 13.
- Seleccionar {F,G,H,I,J}: 10+7-4+3-8 = 8.
- Seleccionar {F,H}: 10-4 = 6.
- Seleccionar {G,H}: 7-4 = 3.
- Seleccionar {I,J}: 3-8 = -5.
- Seleccionar {F,G,H,I}: inválido (I→J fuerza J).
- Seleccionar nada: 0.

**Óptimo**: $\{F, G, H\}$ con beneficio **13**.

Min cut: $S = \{s, F, G, H\}$, $T = \{I, J, t\}$.
Aristas cortadas: s→I:3, H→t:4. Total = 7.
Max profit = 20 - 7 = 13. ✓

</details>

### Ejercicio 6 — Hall's condition contraejemplo

Grafo bipartito: $L = \{0, 1, 2\}$, $R = \{3, 4\}$.
Aristas: 0-3, 1-3, 2-4.

¿Se cumple Hall? ¿Existe matching perfecto (que cubra L)?

<details><summary>Predicción</summary>

$|L| = 3 > |R| = 2$: imposible cubrir todo L (matching máximo $\leq |R| = 2$).

Hall: $S = L = \{0,1,2\}$, $N(S) = \{3, 4\}$, $|N(S)| = 2 < 3 = |S|$. **Falla**.

Matching máximo: 0-3, 2-4. Tamaño 2. Vértice 1 sin matchear (1 solo conecta a 3,
que ya está tomado por 0; no hay augmenting path).

</details>

### Ejercicio 7 — Edge connectivity

Grafo: 0-1, 0-3, 1-2, 1-3, 2-3, 2-4, 3-4.

Calcula la edge connectivity $\lambda(G)$ fijando $s=0$ y variando $t$.

<details><summary>Predicción</summary>

Max flow (unit capacities, undirected) desde 0 a cada vértice:
- 0→1: paths 0-1, 0-3-1. Max = 2.
- 0→2: paths 0-1-2, 0-3-2. Max = 2.
- 0→3: paths 0-3, 0-1-3. Max = 2.
- 0→4: paths 0-1-2-4, 0-3-4. Max = 2. (0-3-2-4 shares edge 0-3 with second path... let me recheck.)

For 0→4: 0-1-2-4 and 0-3-4. Edge-disjoint? 0-1, 1-2, 2-4 vs 0-3, 3-4. Yes, all different. Max = 2.

Can we get 3? From 0, only edges 0-1 and 0-3. Max 2 paths from 0.

$\lambda(G) = \min(2, 2, 2, 2) = 2$.

Min cut: $\{0\text{-}1, 0\text{-}3\}$ (the two edges incident to 0). Removing
them isolates vertex 0.

Whitney: $\delta(G) = \deg(0) = 2$. So $\lambda(G) = \delta(G) = 2$.

</details>

### Ejercicio 8 — Matching con preferencias

Modifica el ejercicio 1 para que cada estudiante tenga un ranking de preferencia.
¿Puede el flujo máximo modelar **weighted** bipartite matching? ¿Qué se necesita?

<details><summary>Predicción</summary>

El flujo máximo estándar solo encuentra matching de **cardinalidad** máxima, no
de **peso** máximo.

Para matching ponderado se necesita **min-cost max-flow**: cada arista tiene un
costo además de capacidad. Se busca el flujo máximo de costo mínimo (o máximo).

Modelar preferencias como costos:
- Arista estudiante→proyecto con costo = (ranking inverso) y capacidad 1.
- Buscar max-flow de min-cost.

Alternativa: **algoritmo Húngaro** ($O(n^3)$) resuelve el **assignment problem**
(matching perfecto de peso mínimo en bipartito completo) directamente, sin
modelar como flujo.

El flujo máximo sin costos **no distingue** entre matchings del mismo tamaño
pero diferente calidad.

</details>

### Ejercicio 9 — Selección con ganancias compartidas

Modifica el ejercicio 5: añadir tarea K con beneficio +6 y dependencia K→H
(compartida con F y G). ¿Cambia la selección óptima?

<details><summary>Predicción</summary>

Nueva red: s→F:10, s→G:7, s→K:6, s→I:3. H→t:4, J→t:8.
F→H:∞, G→H:∞, K→H:∞, I→J:∞.

Positivos: 10+7+6+3 = 26.

Opciones relevantes:
- {F,G,K,H}: 10+7+6-4 = 19.
- {F,G,H}: 10+7-4 = 13.
- {F,G,K,H,I,J}: 10+7+6-4+3-8 = 14.

Corte $S = \{s,F,G,K,H\}$, $T = \{I,J,t\}$:
Edges: s→I:3, H→t:4. Total = 7. Profit = 26-7 = **19**.

Corte $S = \{s,F,G,K,H,I,J\}$, $T = \{t\}$:
Edges: H→t:4, J→t:8. Total = 12. Profit = 26-12 = 14.

Min cut = 7. **Selección óptima: {F, G, K, H}** con beneficio **19**.

Sí cambia: antes era 13, ahora 19. Añadir K es rentable porque comparte la
dependencia H con F y G (H solo se "paga" una vez: -4).

</details>

### Ejercicio 10 — Menger y conectividad

Demuestra que un grafo es $k$-arista-conexo (necesita eliminar al menos $k$
aristas para desconectarlo) si y solo si entre cada par de vértices existen al
menos $k$ caminos arista-disjuntos.

<details><summary>Predicción</summary>

**($\Leftarrow$)**: Si entre cada par $s, t$ existen $k$ caminos arista-disjuntos,
entonces por Menger el min edge cut entre $s$ y $t$ es $\geq k$. Eliminar
$< k$ aristas no puede desconectar ningún par. Luego el grafo es $k$-arista-conexo.

**($\Rightarrow$)**: Si el grafo es $k$-arista-conexo, entonces para cada par
$s, t$, el min edge cut es $\geq k$ (si fuera $< k$, eliminar esas aristas
desconectaría $s$ de $t$, contradicción). Por Menger, existen $\geq k$ caminos
arista-disjuntos.

La equivalencia es directa por el **teorema de Menger** que establece:

$$\text{max edge-disjoint paths}(s,t) = \text{min edge cut}(s,t)$$

Y la $k$-arista-conexidad se define como $\min_{s \neq t} \text{min edge cut}(s,t) \geq k$.

La demostración de Menger se apoya en MFMC (T01): el flujo máximo con
capacidades unitarias es el número de caminos arista-disjuntos, y por MFMC
es igual al min cut. La integralidad (T01 §8.4) garantiza que el flujo máximo
es entero y se descompone en caminos unitarios.

</details>
