# T01 — Árbol generador mínimo (MST)

## 1. Definición

Un **árbol generador** (*spanning tree*) de un grafo conexo no dirigido $G = (V, E)$ es un
subgrafo $T = (V, E_T)$ que satisface tres condiciones simultáneamente:

1. Contiene **todos** los vértices de $G$.
2. Es **conexo**: existe camino entre cualquier par de vértices.
3. Es **acíclico**: no contiene ciclos.

Cualquier dos de estas propiedades implican la tercera junto con $|E_T| = n - 1$, donde
$n = |V|$:

| Propiedad | Equivalencia |
|-----------|-------------|
| Conexo + acíclico | Árbol con $n - 1$ aristas |
| Conexo + $n - 1$ aristas | Acíclico |
| Acíclico + $n - 1$ aristas | Conexo |

Un **árbol generador mínimo** (MST, *minimum spanning tree*) es el árbol generador que
minimiza el peso total:

$$w(T) = \sum_{(u,v) \in E_T} w(u,v)$$

### Requisitos

- **No dirigido**: para grafos dirigidos el problema análogo es la *minimum spanning
  arborescence* (algoritmo de Edmonds/Chu-Liu), significativamente más complejo.
- **Conexo**: si $G$ no es conexo, no existe spanning tree. Se trabaja con el **bosque
  generador mínimo** (*minimum spanning forest*): la unión de MSTs de cada componente.
- **Ponderado**: sin pesos, todo spanning tree tiene el mismo "costo" ($n - 1$ aristas).

### Ejemplo

Grafo con 6 vértices y 9 aristas:

```
        4           2
    0-------1-------2
    |      /|      /|
    |3   8/ |6   5/ |7
    |   /   |   /   |
    3-------4-------5
        9       1
```

Aristas ordenadas por peso: (4,5):1, (1,2):2, (0,3):3, (0,1):4, (2,4):5,
(1,4):6, (2,5):7, (1,3):8, (3,4):9.

MST: $\{(4,5):1,\; (1,2):2,\; (0,3):3,\; (0,1):4,\; (2,4):5\}$ con peso total
$1 + 2 + 3 + 4 + 5 = 15$.


## 2. Existencia, unicidad y conteo

### Existencia

Todo grafo conexo posee al menos un spanning tree. La demostración es constructiva:
partiendo de $G$, mientras exista un ciclo, eliminamos una arista del ciclo (el grafo sigue
conexo porque la arista eliminada era redundante). Al terminar tenemos un subgrafo conexo
y acíclico: un spanning tree.

### Unicidad

**Teorema**: si todos los pesos de las aristas son **distintos**, el MST es **único**.

*Demostración*: supongamos dos MSTs distintos $T_1$ y $T_2$. Sea $e$ la arista de menor
peso en la diferencia simétrica $T_1 \triangle T_2$. Sin pérdida de generalidad,
$e \in T_1 \setminus T_2$. Agregar $e$ a $T_2$ crea un ciclo $C$. Alguna arista
$f \in C$ pertenece a $T_2 \setminus T_1$ (de lo contrario $T_1$ tendría un ciclo).
Por elección de $e$, $w(f) > w(e)$ (pesos distintos y $e$ es la de menor peso en la
diferencia). Entonces $T_2' = T_2 - f + e$ tiene $w(T_2') < w(T_2)$, contradiciendo
que $T_2$ es MST. $\blacksquare$

Si existen aristas con pesos iguales, pueden existir **múltiples MSTs**, pero todos
comparten una propiedad notable:

**Teorema del multiconjunto**: todos los MSTs de un grafo tienen exactamente el mismo
**multiconjunto** de pesos de aristas — no solo la misma suma, sino las mismas cantidades
de cada peso.

### Conteo de spanning trees

Para un grafo general, el número de spanning trees se calcula mediante el **teorema de
Kirchhoff** (*matrix tree theorem*): es igual a cualquier cofactor de la **matriz
laplaciana** $L = D - A$, donde $D$ es la matriz diagonal de grados y $A$ la de adyacencia.

Para el grafo completo $K_n$, la **fórmula de Cayley** da $\tau(K_n) = n^{n-2}$:
$K_3$ tiene 3, $K_4$ tiene 16, $K_5$ tiene 125, $K_{10}$ tiene $10^8$. Este crecimiento
exponencial hace inviable la búsqueda exhaustiva de MST.


## 3. Propiedad de corte

Un **corte** de $G$ es una partición $(S, V \setminus S)$ con $S \neq \emptyset$ y
$S \neq V$. Las aristas que **cruzan** el corte son aquellas con un extremo en $S$ y otro
en $V \setminus S$. La **arista ligera** del corte es la de menor peso entre las que
lo cruzan.

**Teorema (propiedad de corte)**: sea $(S, V \setminus S)$ un corte y $e$ una arista
ligera de ese corte. Entonces existe un MST que contiene $e$. Si $e$ es la **única**
arista ligera (peso estrictamente menor), entonces $e$ pertenece a **todo** MST.

*Demostración*: sea $T$ un MST que no contiene $e = (u,v)$ con $u \in S, v \in V \setminus S$.
Agregar $e$ a $T$ crea un ciclo $C$. Como $C$ cruza el corte en $e$, debe cruzarlo al
menos una vez más en otra arista $f$. Por ser $e$ ligera, $w(e) \leq w(f)$. Definimos
$T' = T - f + e$:

- $T'$ tiene $n - 1$ aristas (removimos una, agregamos una).
- $T'$ es conexo (el camino que usaba $f$ ahora usa $e$ más parte del ciclo).
- $w(T') = w(T) - w(f) + w(e) \leq w(T)$.

Como $T$ es MST, $w(T') \geq w(T)$, luego $w(T') = w(T)$ y $T'$ también es MST.
Si $e$ es estrictamente la más ligera, $w(e) < w(f)$ implica $w(T') < w(T)$:
contradicción. $\blacksquare$

### Verificación en el ejemplo

Removiendo la arista (0,1):4 del MST se obtienen componentes $S = \{0, 3\}$ y
$V \setminus S = \{1, 2, 4, 5\}$. Aristas que cruzan:

| Arista | Peso |
|--------|------|
| (0,1) | 4 |
| (1,3) | 8 |
| (3,4) | 9 |

La arista (0,1):4 es efectivamente la más ligera del corte.


## 4. Propiedad de ciclo

**Teorema (propiedad de ciclo)**: sea $C$ un ciclo en $G$ y $e$ la arista de mayor peso
en $C$. Entonces existe un MST que **no** contiene $e$. Si $e$ es la **única** arista
más pesada, entonces $e$ no pertenece a **ningún** MST.

*Demostración*: supongamos $e = (u,v)$ en algún MST $T$. Eliminar $e$ de $T$ crea un
corte $(S, V \setminus S)$. Como $C$ pasa por $e$ y es un ciclo, al menos otra arista
$f \in C$ cruza este corte con $w(f) < w(e)$. Entonces $T' = T - e + f$ tiene
$w(T') < w(T)$, contradicción. $\blacksquare$

La propiedad de ciclo es la **dual** de la propiedad de corte. Juntas caracterizan
completamente las aristas del MST:

- Una arista $e$ está en **algún** MST $\iff$ no es la arista estrictamente más pesada
  de ningún ciclo.
- Una arista $e$ está en **todo** MST $\iff$ es la arista estrictamente más ligera de
  algún corte.

### Verificación en el ejemplo

Agregando la arista no-MST (1,3):8 al MST se forma el ciclo $1 \to 0 \to 3 \to 1$
con pesos (0,1):4, (0,3):3, (1,3):8. La más pesada es (1,3):8, que efectivamente
no pertenece al MST.


## 5. Algoritmo genérico (regla azul-roja)

Tarjan formuló un marco genérico donde **cualquier** algoritmo correcto de MST puede
entenderse como aplicación de dos reglas de coloreo:

- **Regla azul** (inclusión): elegir un corte sin aristas azules que lo crucen.
  Colorear de azul la arista más ligera del corte.
- **Regla roja** (exclusión): elegir un ciclo sin aristas rojas.
  Colorear de rojo la arista más pesada del ciclo.

Aplicar estas reglas en cualquier orden hasta que no queden aplicables. Al terminar,
las aristas azules forman un MST.

### Instancias del algoritmo genérico

| Algoritmo | Regla | Elección del corte/ciclo |
|-----------|-------|--------------------------|
| **Kruskal** | Azul | Arista mínima global; el corte separa las dos componentes que une |
| **Prim** | Azul | Corte $(S, V \setminus S)$ donde $S$ es el árbol parcial |
| **Borůvka** | Azul (paralelo) | Cada componente elige su arista saliente mínima |
| **Inverso (arista máx.)** | Roja | Procesar aristas de mayor a menor, eliminar si hay ciclo |

La corrección de todos se reduce a la validez de las propiedades de corte y ciclo.


## 6. Relación con matroides

La familia de subconjuntos de aristas que forman **bosques** (subgrafos acíclicos)
constituye un **matroide gráfico**. En cualquier matroide, el algoritmo greedy — que
procesa elementos en orden de peso ascendente y agrega cada uno si mantiene la
independencia — produce un conjunto independiente de peso mínimo.

Kruskal es exactamente este greedy: ordena aristas por peso, agrega cada una si no crea
ciclo (mantiene independencia en el matroide). Esta perspectiva no solo justifica la
corrección sino que revela que MST es un caso particular de un fenómeno combinatorio
más amplio: la optimización greedy en matroides.


## 7. Aplicaciones

### Diseño de redes

Conectar $n$ ubicaciones (ciudades, servidores, edificios) con el mínimo costo total de
cableado, tubería o carretera. El MST da exactamente la infraestructura óptima cuando
cada conexión es punto a punto y el costo depende solo de los dos extremos.

### Clustering

El MST proporciona un método natural de **agrupamiento jerárquico** (*single-linkage
clustering*): al eliminar las $k - 1$ aristas más pesadas del MST, el bosque resultante
tiene exactamente $k$ componentes conexas que forman $k$ clusters.

Este método maximiza la **separación mínima** entre clusters (la arista más liviana que
conectaría dos clusters distintos).

### Aproximación para TSP métrico

Para el **problema del viajante** (TSP) con desigualdad triangular, el MST da una
2-aproximación: un recorrido Euler del MST duplicado visita cada arista dos veces,
produciendo un tour de costo $\leq 2 \cdot w(\text{MST}) \leq 2 \cdot \text{OPT}$.
El algoritmo de Christofides mejora esto a $\frac{3}{2}$-aproximación.

### Árbol bottleneck

El MST es también el **minimum bottleneck spanning tree**: minimiza el peso de la arista
más pesada del árbol. Como consecuencia, el camino entre cualesquiera $u$ y $v$ en el MST
tiene el menor *bottleneck* posible (menor máximo de arista) entre todos los caminos
$u \to v$ en $G$.


## 8. Variantes y problemas relacionados

| Problema | Descripción | Complejidad |
|----------|-------------|-------------|
| MST | Peso total mínimo | $O(m \log n)$ o mejor |
| Minimum spanning forest | MST por componente conexa | Igual que MST |
| Segundo mejor MST | Segundo menor peso total | $O(m \log n)$ con LCA |
| Bottleneck spanning tree | Minimizar arista máxima | Coincide con MST |
| Degree-constrained MST | Grado máximo acotado | NP-hard |
| Steiner tree | Conectar subconjunto de vértices | NP-hard |
| Arborescencia mínima | MST dirigido (raíz fija) | $O(m \log n)$ (Edmonds) |


## 9. Programa en C

```c
/* mst_properties.c — MST: definicion, propiedades de corte y ciclo */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAXV 20
#define MAXE 100
#define INF  1000000

typedef struct { int u, v, w; } Edge;

/* --- Basic Union-Find (optimizations in T02) --- */
static int par[MAXV], uf_sz[MAXV];

void uf_init(int n) {
    for (int i = 0; i < n; i++) { par[i] = i; uf_sz[i] = 1; }
}

int uf_find(int x) {
    while (par[x] != x) x = par[x];
    return x;
}

bool uf_union(int a, int b) {
    a = uf_find(a); b = uf_find(b);
    if (a == b) return false;
    if (uf_sz[a] < uf_sz[b]) { int t = a; a = b; b = t; }
    par[b] = a; uf_sz[a] += uf_sz[b];
    return true;
}

int cmp_edge(const void *a, const void *b) {
    return ((const Edge *)a)->w - ((const Edge *)b)->w;
}

/* --- Kruskal: returns MST edge count, fills mst[] --- */
int kruskal(Edge edges[], int m, int n, Edge mst[]) {
    Edge sorted[MAXE];
    memcpy(sorted, edges, m * sizeof(Edge));
    qsort(sorted, m, sizeof(Edge), cmp_edge);
    uf_init(n);
    int cnt = 0;
    for (int i = 0; i < m && cnt < n - 1; i++)
        if (uf_union(sorted[i].u, sorted[i].v))
            mst[cnt++] = sorted[i];
    return cnt;
}

/* --- MST adjacency list for path/component queries --- */
typedef struct { int to, w; } Adj;
static Adj adj[MAXV][MAXV];
static int adj_cnt[MAXV];

void build_mst_adj(Edge mst[], int cnt) {
    memset(adj_cnt, 0, sizeof(adj_cnt));
    for (int i = 0; i < cnt; i++) {
        adj[mst[i].u][adj_cnt[mst[i].u]++] = (Adj){mst[i].v, mst[i].w};
        adj[mst[i].v][adj_cnt[mst[i].v]++] = (Adj){mst[i].u, mst[i].w};
    }
}

/* --- DFS: find path from src to dst in MST --- */
static int path_v[MAXV], path_w[MAXV];
static int path_len;

bool dfs_path(int cur, int dst, int from) {
    path_v[path_len] = cur;
    if (cur == dst) { path_len++; return true; }
    for (int i = 0; i < adj_cnt[cur]; i++) {
        int nx = adj[cur][i].to;
        if (nx == from) continue;
        path_w[path_len + 1] = adj[cur][i].w;
        path_len++;
        if (dfs_path(nx, dst, cur)) return true;
        path_len--;
    }
    return false;
}

/* --- BFS: component of start excluding edge (eu,ev) --- */
static bool comp[MAXV];

void find_component(int start, int eu, int ev, int n) {
    memset(comp, false, n * sizeof(bool));
    int queue[MAXV], front = 0, back = 0;
    comp[start] = true;
    queue[back++] = start;
    while (front < back) {
        int cur = queue[front++];
        for (int i = 0; i < adj_cnt[cur]; i++) {
            int nx = adj[cur][i].to;
            if ((cur == eu && nx == ev) || (cur == ev && nx == eu)) continue;
            if (!comp[nx]) { comp[nx] = true; queue[back++] = nx; }
        }
    }
}

/* ======================================================= */
int main(void) {
    /*     0 ---4--- 1 ---2--- 2
     *     |       / |       / |
     *     3     8   6     5   7
     *     |   /     |   /     |
     *     3 ---9--- 4 ---1--- 5           */
    int n = 6;
    Edge edges[] = {
        {0,1,4}, {0,3,3}, {1,2,2}, {1,3,8}, {1,4,6},
        {2,4,5}, {2,5,7}, {3,4,9}, {4,5,1}
    };
    int m = 9;
    Edge mst[MAXE];

    /* == Demo 1: Find MST == */
    printf("=== Demo 1: MST basico ===\n");
    int mst_cnt = kruskal(edges, m, n, mst);
    int total = 0;
    printf("MST edges:\n");
    for (int i = 0; i < mst_cnt; i++) {
        printf("  (%d, %d) weight %d\n", mst[i].u, mst[i].v, mst[i].w);
        total += mst[i].w;
    }
    printf("Total weight: %d\n", total);
    printf("Edges: %d (expected n-1 = %d)\n\n", mst_cnt, n - 1);

    build_mst_adj(mst, mst_cnt);

    /* == Demo 2: Verify MST properties == */
    printf("=== Demo 2: Verify spanning tree properties ===\n");
    bool visited[MAXV] = {false};
    int queue[MAXV], front = 0, back = 0;
    visited[0] = true; queue[back++] = 0;
    int reach = 1;
    while (front < back) {
        int cur = queue[front++];
        for (int i = 0; i < adj_cnt[cur]; i++) {
            int nx = adj[cur][i].to;
            if (!visited[nx]) { visited[nx] = true; queue[back++] = nx; reach++; }
        }
    }
    printf("Connected: %s (reached %d/%d)\n", reach == n ? "yes" : "no", reach, n);
    printf("Acyclic: %s (n-1=%d edges, connected)\n",
           mst_cnt == n - 1 ? "yes" : "no", n - 1);

    bool in_mst[MAXE];
    for (int i = 0; i < m; i++) {
        in_mst[i] = false;
        for (int j = 0; j < mst_cnt; j++)
            if (edges[i].u == mst[j].u && edges[i].v == mst[j].v &&
                edges[i].w == mst[j].w) { in_mst[i] = true; break; }
    }

    printf("Optimality — non-MST edge vs max on MST path:\n");
    bool optimal = true;
    for (int i = 0; i < m; i++) {
        if (in_mst[i]) continue;
        path_len = 0;
        dfs_path(edges[i].u, edges[i].v, -1);
        int max_w = 0;
        for (int j = 1; j < path_len; j++)
            if (path_w[j] > max_w) max_w = path_w[j];
        bool ok = edges[i].w >= max_w;
        printf("  (%d,%d):%d  path max: %d  %s\n",
               edges[i].u, edges[i].v, edges[i].w, max_w, ok ? "OK" : "FAIL");
        if (!ok) optimal = false;
    }
    printf("MST is optimal: %s\n\n", optimal ? "yes" : "no");

    /* == Demo 3: Cut property == */
    printf("=== Demo 3: Cut property ===\n");
    for (int k = 0; k < mst_cnt; k++) {
        int eu = mst[k].u, ev = mst[k].v, ew = mst[k].w;
        find_component(eu, eu, ev, n);
        printf("Remove (%d,%d):%d  S={", eu, ev, ew);
        for (int i = 0; i < n; i++) if (comp[i]) printf("%d ", i);
        printf("}  crossing:");
        int lightest = INF;
        for (int i = 0; i < m; i++) {
            bool cu = comp[edges[i].u], cv = comp[edges[i].v];
            if (cu != cv) {
                printf(" (%d,%d):%d", edges[i].u, edges[i].v, edges[i].w);
                if (edges[i].w < lightest) lightest = edges[i].w;
            }
        }
        printf("\n  Lightest: %d == MST edge: %d => %s\n",
               lightest, ew, lightest == ew ? "CUT OK" : "FAIL");
    }

    /* == Demo 4: Cycle property == */
    printf("\n=== Demo 4: Cycle property ===\n");
    for (int i = 0; i < m; i++) {
        if (in_mst[i]) continue;
        path_len = 0;
        dfs_path(edges[i].u, edges[i].v, -1);
        printf("Add (%d,%d):%d  cycle:", edges[i].u, edges[i].v, edges[i].w);
        for (int j = 0; j < path_len; j++) printf(" %d", path_v[j]);
        int heaviest = edges[i].w;
        printf("  weights:");
        for (int j = 1; j < path_len; j++) {
            printf(" %d", path_w[j]);
            if (path_w[j] > heaviest) heaviest = path_w[j];
        }
        printf(" [+%d]", edges[i].w);
        printf("\n  Heaviest: %d == non-MST: %d => %s\n",
               heaviest, edges[i].w, heaviest == edges[i].w ? "CYCLE OK" : "FAIL");
    }

    /* == Demo 5: Uniqueness == */
    printf("\n=== Demo 5: Uniqueness ===\n");
    bool all_distinct = true;
    for (int i = 0; i < m && all_distinct; i++)
        for (int j = i + 1; j < m; j++)
            if (edges[i].w == edges[j].w) { all_distinct = false; break; }
    printf("All weights distinct: %s => MST %s\n\n",
           all_distinct ? "yes" : "no",
           all_distinct ? "UNIQUE (guaranteed)" : "possibly non-unique");

    printf("--- Modified graph: edge (2,4) changed from 5 to 6 ---\n");
    Edge edges2[] = {
        {0,1,4}, {0,3,3}, {1,2,2}, {1,3,8}, {1,4,6},
        {2,4,6}, {2,5,7}, {3,4,9}, {4,5,1}
    };
    Edge mst2[MAXE];
    int cnt2 = kruskal(edges2, 9, n, mst2);
    int t2 = 0;
    printf("MST A: ");
    for (int i = 0; i < cnt2; i++) {
        printf("(%d,%d):%d ", mst2[i].u, mst2[i].v, mst2[i].w);
        t2 += mst2[i].w;
    }
    printf(" total=%d\n", t2);
    printf("MST B: (4,5):1 (1,2):2 (0,3):3 (0,1):4 (2,4):6  total=%d\n", 1+2+3+4+6);
    printf("Same weight, different edges => MST NOT unique\n");

    /* == Demo 6: Network design == */
    printf("\n=== Demo 6: Network design ===\n");
    const char *cities[] = {"Lima","Cusco","Arequipa","Trujillo","Iquitos","Piura"};
    for (int i = 0; i < n; i++) printf("  %d: %s\n", i, cities[i]);
    int all_cost = 0;
    for (int i = 0; i < m; i++) all_cost += edges[i].w;
    printf("All %d links cost: %d\n", m, all_cost);
    printf("MST: %d links, cost: %d  (savings %.0f%%)\n",
           mst_cnt, total, 100.0 * (all_cost - total) / all_cost);
    printf("MST network:\n");
    for (int i = 0; i < mst_cnt; i++)
        printf("  %s -- %s  (cost %d)\n",
               cities[mst[i].u], cities[mst[i].v], mst[i].w);

    return 0;
}
```


## 10. Programa en Rust

```rust
// mst_properties.rs — MST: definition, cut/cycle properties, applications

use std::collections::{HashMap, HashSet, VecDeque};

// --- Union-Find ---
struct UnionFind {
    parent: Vec<usize>,
    size: Vec<usize>,
}

impl UnionFind {
    fn new(n: usize) -> Self {
        Self { parent: (0..n).collect(), size: vec![1; n] }
    }
    fn find(&mut self, mut x: usize) -> usize {
        while self.parent[x] != x {
            self.parent[x] = self.parent[self.parent[x]];
            x = self.parent[x];
        }
        x
    }
    fn union(&mut self, a: usize, b: usize) -> bool {
        let (mut a, mut b) = (self.find(a), self.find(b));
        if a == b { return false; }
        if self.size[a] < self.size[b] { std::mem::swap(&mut a, &mut b); }
        self.parent[b] = a;
        self.size[a] += self.size[b];
        true
    }
}

#[derive(Clone, Copy)]
struct Edge { u: usize, v: usize, w: i32 }

fn kruskal(edges: &[Edge], n: usize) -> Vec<Edge> {
    let mut sorted: Vec<Edge> = edges.to_vec();
    sorted.sort_by_key(|e| e.w);
    let mut uf = UnionFind::new(n);
    let mut mst = Vec::new();
    for e in &sorted {
        if uf.union(e.u, e.v) {
            mst.push(*e);
            if mst.len() == n - 1 { break; }
        }
    }
    mst
}

fn build_adj(mst: &[Edge], n: usize) -> Vec<Vec<(usize, i32)>> {
    let mut adj = vec![vec![]; n];
    for e in mst {
        adj[e.u].push((e.v, e.w));
        adj[e.v].push((e.u, e.w));
    }
    adj
}

fn find_path(adj: &[Vec<(usize, i32)>], src: usize, dst: usize)
    -> Option<(Vec<usize>, Vec<i32>)>
{
    let n = adj.len();
    let mut visited = vec![false; n];
    let mut path = Vec::new();
    let mut weights = Vec::new();

    fn dfs(adj: &[Vec<(usize, i32)>], cur: usize, dst: usize,
           vis: &mut [bool], path: &mut Vec<usize>, wts: &mut Vec<i32>) -> bool {
        vis[cur] = true;
        path.push(cur);
        if cur == dst { return true; }
        for &(nx, w) in &adj[cur] {
            if !vis[nx] {
                wts.push(w);
                if dfs(adj, nx, dst, vis, path, wts) { return true; }
                wts.pop();
            }
        }
        path.pop();
        false
    }

    if dfs(adj, src, dst, &mut visited, &mut path, &mut weights) {
        Some((path, weights))
    } else { None }
}

fn find_component(adj: &[Vec<(usize, i32)>], start: usize,
                  eu: usize, ev: usize) -> Vec<bool> {
    let n = adj.len();
    let mut comp = vec![false; n];
    let mut queue = VecDeque::new();
    comp[start] = true;
    queue.push_back(start);
    while let Some(cur) = queue.pop_front() {
        for &(nx, _) in &adj[cur] {
            if (cur == eu && nx == ev) || (cur == ev && nx == eu) { continue; }
            if !comp[nx] { comp[nx] = true; queue.push_back(nx); }
        }
    }
    comp
}

fn edge_key(e: &Edge) -> (usize, usize, i32) {
    (e.u.min(e.v), e.u.max(e.v), e.w)
}

fn main() {
    let n = 6;
    let edges = vec![
        Edge { u: 0, v: 1, w: 4 }, Edge { u: 0, v: 3, w: 3 },
        Edge { u: 1, v: 2, w: 2 }, Edge { u: 1, v: 3, w: 8 },
        Edge { u: 1, v: 4, w: 6 }, Edge { u: 2, v: 4, w: 5 },
        Edge { u: 2, v: 5, w: 7 }, Edge { u: 3, v: 4, w: 9 },
        Edge { u: 4, v: 5, w: 1 },
    ];

    // == Demo 1: Basic MST ==
    println!("=== Demo 1: Basic MST ===");
    let mst = kruskal(&edges, n);
    let total: i32 = mst.iter().map(|e| e.w).sum();
    for e in &mst {
        println!("  ({}, {}) weight {}", e.u, e.v, e.w);
    }
    println!("Total: {}, edges: {} (n-1={})\n", total, mst.len(), n - 1);

    let adj = build_adj(&mst, n);
    let mst_set: HashSet<(usize, usize, i32)> = mst.iter().map(|e| edge_key(e)).collect();

    // == Demo 2: Cut property ==
    println!("=== Demo 2: Cut property ===");
    for e in &mst {
        let comp = find_component(&adj, e.u, e.u, e.v);
        let crossing: Vec<&Edge> = edges.iter()
            .filter(|g| comp[g.u] != comp[g.v]).collect();
        let lightest = crossing.iter().map(|g| g.w).min().unwrap();
        print!("  Remove ({},{}):{}  S={{", e.u, e.v, e.w);
        for i in 0..n { if comp[i] { print!("{} ", i); } }
        print!("}}  crossing:");
        for g in &crossing { print!(" ({},{}):{}", g.u, g.v, g.w); }
        println!("  lightest={} {}", lightest,
            if lightest == e.w { "OK" } else { "FAIL" });
    }

    // == Demo 3: Cycle property ==
    println!("\n=== Demo 3: Cycle property ===");
    for e in &edges {
        if mst_set.contains(&edge_key(e)) { continue; }
        if let Some((path, weights)) = find_path(&adj, e.u, e.v) {
            let max_w = *weights.iter().max().unwrap();
            let heaviest = e.w.max(max_w);
            print!("  Add ({},{}):{}  cycle:", e.u, e.v, e.w);
            for v in &path { print!(" {}", v); }
            print!("  weights:");
            for w in &weights { print!(" {}", w); }
            println!(" [+{}]  heaviest={} {}",
                e.w, heaviest, if heaviest == e.w { "OK" } else { "FAIL" });
        }
    }

    // == Demo 4: Uniqueness ==
    println!("\n=== Demo 4: Uniqueness ===");
    let mut wset = HashSet::new();
    let all_distinct = edges.iter().all(|e| wset.insert(e.w));
    println!("All weights distinct: {} => MST {}",
        all_distinct, if all_distinct { "UNIQUE" } else { "possibly non-unique" });

    let edges2 = vec![
        Edge { u: 0, v: 1, w: 4 }, Edge { u: 0, v: 3, w: 3 },
        Edge { u: 1, v: 2, w: 2 }, Edge { u: 1, v: 3, w: 8 },
        Edge { u: 1, v: 4, w: 6 }, Edge { u: 2, v: 4, w: 6 },
        Edge { u: 2, v: 5, w: 7 }, Edge { u: 3, v: 4, w: 9 },
        Edge { u: 4, v: 5, w: 1 },
    ];
    let mst2 = kruskal(&edges2, n);
    let t2: i32 = mst2.iter().map(|e| e.w).sum();
    print!("Modified (2,4)=6: MST=");
    for e in &mst2 { print!("({},{}):{} ", e.u, e.v, e.w); }
    println!("total={}", t2);
    println!("Alternative: (4,5):1 (1,2):2 (0,3):3 (0,1):4 (2,4):6 total=16");
    println!("=> Two distinct MSTs with equal weight\n");

    // == Demo 5: Minimum spanning forest ==
    println!("=== Demo 5: Spanning forest (disconnected) ===");
    let edges3 = vec![
        Edge { u: 0, v: 1, w: 3 }, Edge { u: 1, v: 2, w: 5 },
        Edge { u: 3, v: 4, w: 2 }, Edge { u: 4, v: 5, w: 4 },
        Edge { u: 6, v: 7, w: 1 },
    ];
    let n3 = 8;
    let forest = kruskal(&edges3, n3);
    let ftotal: i32 = forest.iter().map(|e| e.w).sum();
    println!("{} vertices, {} edges, {} components",
        n3, edges3.len(), n3 - forest.len());
    for e in &forest { println!("  ({}, {}) weight {}", e.u, e.v, e.w); }
    println!("Forest weight: {}\n", ftotal);

    // == Demo 6: Clustering via MST ==
    println!("=== Demo 6: Clustering (k=3) ===");
    let mut sorted_mst = mst.clone();
    sorted_mst.sort_by(|a, b| b.w.cmp(&a.w));
    let k = 3;
    println!("Remove {} heaviest MST edges:", k - 1);
    for i in 0..k - 1 {
        println!("  ({}, {}): {}", sorted_mst[i].u, sorted_mst[i].v, sorted_mst[i].w);
    }
    let mut uf = UnionFind::new(n);
    for e in &sorted_mst[k - 1..] {
        uf.union(e.u, e.v);
    }
    let mut clusters: HashMap<usize, Vec<usize>> = HashMap::new();
    for i in 0..n {
        clusters.entry(uf.find(i)).or_default().push(i);
    }
    println!("{} clusters:", clusters.len());
    for (_, members) in &clusters {
        print!("  {{");
        for (i, v) in members.iter().enumerate() {
            if i > 0 { print!(", "); }
            print!("{}", v);
        }
        println!("}}");
    }

    // == Demo 7: Bottleneck distance matrix ==
    println!("\n=== Demo 7: Bottleneck distances ===");
    println!("(min possible max-edge on any path between u and v)");
    print!("     ");
    for j in 0..n { print!("{:>3}", j); }
    println!();
    for i in 0..n {
        print!("  {} [", i);
        for j in 0..n {
            if i == j {
                print!("{:>3}", 0);
            } else if let Some((_, weights)) = find_path(&adj, i, j) {
                print!("{:>3}", weights.iter().max().unwrap());
            } else {
                print!("  -");
            }
        }
        println!(" ]");
    }

    // == Demo 8: Second-best MST ==
    println!("\n=== Demo 8: Second-best MST ===");
    let mut best_alt = i32::MAX;
    let mut best_remove = mst[0];
    let mut best_add = mst[0];
    for me in &mst {
        let comp = find_component(&adj, me.u, me.u, me.v);
        let mut min_cross = i32::MAX;
        let mut min_edge = *me;
        for e in &edges {
            if edge_key(e) == edge_key(me) { continue; }
            if comp[e.u] != comp[e.v] && e.w < min_cross {
                min_cross = e.w;
                min_edge = *e;
            }
        }
        if min_cross < i32::MAX {
            let alt = total - me.w + min_cross;
            println!("  Remove ({},{}):{}  add ({},{}):{}  alt={}",
                me.u, me.v, me.w, min_edge.u, min_edge.v, min_edge.w, alt);
            if alt < best_alt {
                best_alt = alt;
                best_remove = *me;
                best_add = min_edge;
            }
        }
    }
    println!("Second-best MST: {} (swap ({},{}):{} -> ({},{}):{}, +{})",
        best_alt, best_remove.u, best_remove.v, best_remove.w,
        best_add.u, best_add.v, best_add.w, best_alt - total);
}
```


## 11. Ejercicios

**Ejercicio 1**: encuentre el MST del siguiente grafo paso a paso (procesando aristas de
menor a mayor peso). Indique para cada arista si se acepta o rechaza y por qué.

```
Vertices: {0, 1, 2, 3, 4, 5, 6}
Aristas: (0,1):10, (0,2):6, (0,3):5, (1,3):15, (2,3):4,
         (2,4):8, (3,4):9, (3,5):12, (4,5):7, (4,6):3, (5,6):11
```

<details><summary>Predicción</summary>

Ordenadas: (4,6):3, (2,3):4, (0,3):5, (0,2):6, (4,5):7, (2,4):8, (3,4):9,
(0,1):10, (5,6):11, (3,5):12, (1,3):15.

1. (4,6):3 — aceptar, une {4} y {6}.
2. (2,3):4 — aceptar, une {2} y {3}.
3. (0,3):5 — aceptar, une {0} con {2,3}.
4. (0,2):6 — **rechazar**, 0 y 2 ya están conectados (ciclo 0-3-2).
5. (4,5):7 — aceptar, une {5} con {4,6}.
6. (2,4):8 — aceptar, une {0,2,3} con {4,5,6}. Ya tenemos 5 aristas → 6 edges = n-1.
7. (0,1):10 — aceptar, une {1} con el resto. 6 aristas = n-1 → MST completo.

MST: {(4,6):3, (2,3):4, (0,3):5, (4,5):7, (2,4):8, (0,1):10}, peso total = 37.
</details>

---

**Ejercicio 2**: para el MST del ejercicio 1, verifique la propiedad de corte en la
arista (2,4):8. Identifique las componentes al removerla y todas las aristas que cruzan
el corte.

<details><summary>Predicción</summary>

Remover (2,4):8 del MST. Las aristas restantes son: (4,6):3, (2,3):4, (0,3):5,
(4,5):7, (0,1):10.

Componente de 2: $2 \to 3 \to 0 \to 1$ → $S = \{0, 1, 2, 3\}$.
Componente de 4: $4 \to 6, 4 \to 5$ → $V \setminus S = \{4, 5, 6\}$.

Aristas que cruzan $(S, V \setminus S)$: (2,4):8, (3,4):9, (3,5):12.
La más ligera es (2,4):8, confirmando la propiedad de corte.
</details>

---

**Ejercicio 3**: verifique la propiedad de ciclo para la arista (0,2):6 en el grafo
del ejercicio 1.

<details><summary>Predicción</summary>

Agregar (0,2):6 al MST crea un ciclo. El camino de 0 a 2 en el MST es:
$0 \to 3 \to 2$ con aristas (0,3):5 y (2,3):4.

Ciclo: $0 \to 3 \to 2 \to 0$, pesos: 5, 4, 6.
La arista más pesada es (0,2):6, que efectivamente no está en el MST.
Propiedad de ciclo verificada.
</details>

---

**Ejercicio 4**: ¿cuántos spanning trees tiene el grafo completo $K_4$? Enumere al
menos 5 de ellos describiendo sus aristas.

<details><summary>Predicción</summary>

Por la fórmula de Cayley: $\tau(K_4) = 4^{4-2} = 16$ spanning trees.

Con vértices {0,1,2,3}, algunos ejemplos (3 aristas cada uno):
1. {(0,1), (1,2), (2,3)} — camino 0-1-2-3
2. {(0,1), (0,2), (0,3)} — estrella centrada en 0
3. {(0,1), (1,2), (1,3)} — estrella centrada en 1
4. {(0,2), (1,2), (2,3)} — estrella centrada en 2
5. {(0,3), (1,3), (2,3)} — estrella centrada en 3

Hay 4 estrellas y 12 caminos (3 aristas de 6 posibles, formando camino hamiltoniano),
sumando 16 en total.
</details>

---

**Ejercicio 5**: en el grafo modificado del demo 5 del programa (arista (2,4) con
peso 6 en lugar de 5), identifique los dos MSTs distintos y verifique que tienen el
mismo multiconjunto de pesos.

<details><summary>Predicción</summary>

MST A: (4,5):1, (1,2):2, (0,3):3, (0,1):4, (1,4):6. Total = 16.
MST B: (4,5):1, (1,2):2, (0,3):3, (0,1):4, (2,4):6. Total = 16.

Multiconjunto de A: {1, 2, 3, 4, 6}.
Multiconjunto de B: {1, 2, 3, 4, 6}.

Son idénticos, confirmando el teorema del multiconjunto. La diferencia está en
*cuál* arista de peso 6 se usa: (1,4) o (2,4), ambas conectan la misma pareja de
componentes en el mismo paso de Kruskal.
</details>

---

**Ejercicio 6**: en el grafo del programa (6 vértices), si el peso de la arista (1,4)
cambia de 6 a 2, ¿cuál es el nuevo MST? Use la propiedad de ciclo para justificar
el cambio sin recalcular desde cero.

<details><summary>Predicción</summary>

Agregar (1,4):2 al MST actual crea el ciclo $1 \to 2 \to 4 \to 1$ con pesos
(1,2):2, (2,4):5, (1,4):2. La arista más pesada del ciclo es (2,4):5.

Como $2 < 5$, la arista (2,4):5 viola la propiedad de ciclo y debe ser reemplazada.

Nuevo MST: {(4,5):1, (1,2):2, **(1,4):2**, (0,3):3, (0,1):4}. Total = 12.

La actualización se hizo en $O(n)$: agregar la nueva arista, encontrar el ciclo en
el MST, remover la más pesada del ciclo.
</details>

---

**Ejercicio 7**: un grafo tiene 10 vértices y 3 componentes conexas de tamaños 4, 3
y 3. ¿Cuántas aristas tiene su bosque generador mínimo?

<details><summary>Predicción</summary>

El bosque generador mínimo tiene un spanning tree por componente. Cada spanning tree
de $k$ vértices tiene $k - 1$ aristas:

$(4 - 1) + (3 - 1) + (3 - 1) = 3 + 2 + 2 = 7$ aristas.

En general, para $n$ vértices y $c$ componentes: $n - c$ aristas en el bosque.
Aquí: $10 - 3 = 7$.
</details>

---

**Ejercicio 8**: dado el MST del programa ({(4,5):1, (1,2):2, (0,3):3, (0,1):4,
(2,4):5}), elimine las 2 aristas más pesadas para obtener 3 clusters. ¿Cuáles son
los clusters resultantes y cuál es la separación mínima entre ellos?

<details><summary>Predicción</summary>

Aristas MST por peso descendente: (2,4):5, (0,1):4, (0,3):3, (1,2):2, (4,5):1.
Eliminar las 2 más pesadas: (2,4):5 y (0,1):4.

Aristas restantes: (0,3):3, (1,2):2, (4,5):1.
Clusters: {0, 3}, {1, 2}, {4, 5}.

La separación mínima entre clusters es el peso de la arista eliminada más liviana
entre las removidas: min(5, 4) = 4 (la arista (0,1):4 que separaba {0,3} de {1,2}).

Esto significa que la distancia mínima entre cualquier par de clusters distintos
es al menos 4 en el sentido de *single-linkage*.
</details>

---

**Ejercicio 9**: ¿cuál es el peso del segundo mejor spanning tree del grafo del
programa? Identifique qué intercambio de aristas lo produce.

<details><summary>Predicción</summary>

Para cada arista del MST, calcular el costo de reemplazarla por la mejor alternativa:

| Remover | Mejor reemplazo | Peso alternativo |
|---------|----------------|------------------|
| (4,5):1 | (2,5):7 | 15 - 1 + 7 = 21 |
| (1,2):2 | (1,4):6 | 15 - 2 + 6 = 19 |
| (0,3):3 | (1,3):8 | 15 - 3 + 8 = 20 |
| (0,1):4 | (1,3):8 | 15 - 4 + 8 = 19 |
| (2,4):5 | (1,4):6 | 15 - 5 + 6 = 16 |

El segundo mejor MST tiene peso **16**, obtenido al intercambiar (2,4):5 por (1,4):6.
Segundo mejor MST: {(4,5):1, (1,2):2, (0,3):3, (0,1):4, (1,4):6}.
</details>

---

**Ejercicio 10**: se agrega la arista (0,5) con peso 2 al grafo del programa.
Describa cómo actualizar el MST en $O(n)$ sin recalcular desde cero. ¿Cuál es
el nuevo MST?

<details><summary>Predicción</summary>

**Procedimiento**: agregar (0,5):2 al MST crea exactamente un ciclo. Encontrar el
camino de 0 a 5 en el MST y la arista más pesada de ese camino.

Camino en MST: $0 \to 1 \to 2 \to 4 \to 5$, con pesos 4, 2, 5, 1.
Arista más pesada: (2,4):5.

Como $w(0,5) = 2 < 5 = w(2,4)$, reemplazar (2,4):5 por (0,5):2 reduce el peso.

Nuevo MST: {(4,5):1, (1,2):2, **(0,5):2**, (0,3):3, (0,1):4}. Total = 12.

El procedimiento es $O(n)$: recorrer el camino en el árbol (máximo $n - 1$ aristas)
para encontrar la más pesada.
</details>
