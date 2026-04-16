# T04 — Componentes fuertemente conexos

## Objetivo

Dominar el concepto de **componente fuertemente conexo** (SCC — Strongly
Connected Component) en grafos dirigidos, e implementar los dos algoritmos
clásicos: **Kosaraju** (dos pasadas de DFS) y **Tarjan** (una pasada con
stack y `low[]`). Comprender el **grafo de condensación** (DAG de SCCs) y
sus aplicaciones.

---

## 1. Definición

### 1.1 Componente fuertemente conexo

En un grafo dirigido $G = (V, E)$, un **componente fuertemente conexo** es
un subconjunto maximal $C \subseteq V$ tal que para todo par $u, v \in C$
existen caminos $u \leadsto v$ **y** $v \leadsto u$.

"Maximal" significa que no se puede agregar ningún vértice más sin romper la
propiedad. Cada vértice pertenece a exactamente un SCC (los SCCs forman una
**partición** de $V$).

```
Grafo dirigido:
  0 → 1 → 2 → 0    (ciclo: SCC {0, 1, 2})
            ↓
            3 → 4   (3 solo, 4 solo)
            ↑   ↓
            └── 5   (ciclo: SCC {3, 4, 5})
                ↓
                6    (solo: SCC {6})

SCCs: {0, 1, 2}, {3, 4, 5}, {6}
```

### 1.2 Propiedades fundamentales

- Un vértice aislado (sin aristas) es un SCC de tamaño 1.
- Si hay un ciclo que pasa por los vértices $v_1, v_2, \ldots, v_k$,
  todos pertenecen al mismo SCC.
- En un DAG, cada vértice es su propio SCC (no hay ciclos).
- El número de SCCs satisface $1 \leq |\text{SCCs}| \leq n$.
  - $|\text{SCCs}| = 1$: el grafo completo es fuertemente conexo.
  - $|\text{SCCs}| = n$: no hay ciclo alguno (DAG).

### 1.3 Relación con conectividad

| Concepto | Tipo de grafo | Condición |
|----------|:-------------:|-----------|
| Conexo | No dirigido | Camino entre todo par |
| Débilmente conexo | Dirigido | Conexo si se ignoran las direcciones |
| Fuertemente conexo | Dirigido | Camino dirigido en **ambas** direcciones entre todo par |

Un grafo dirigido fuertemente conexo es necesariamente débilmente conexo,
pero no al revés.

---

## 2. Grafo de condensación

### 2.1 Definición

El **grafo de condensación** (o **DAG de SCCs**) $G^{\text{SCC}}$ se
construye contrayendo cada SCC a un solo vértice:

- Vértices: uno por cada SCC.
- Arista $(C_i, C_j)$: si existe alguna arista $(u, v)$ en $G$ con
  $u \in C_i$ y $v \in C_j$ (donde $C_i \neq C_j$).

**Propiedad clave**: $G^{\text{SCC}}$ es siempre un **DAG**. Si hubiera un
ciclo entre SCCs, esos SCCs deberían ser un solo SCC más grande
(contradicción con maximalidad).

### 2.2 Ejemplo

```
Grafo original:          Condensación:
  0 → 1 → 2 → 0           SCC₀{0,1,2} → SCC₁{3,4,5} → SCC₂{6}
            ↓
            3 → 4
            ↑   ↓
            └── 5
                ↓
                6
```

El DAG de condensación permite aplicar algoritmos de DAG (topological sort,
camino más largo, DP) sobre la estructura macro del grafo.

### 2.3 Aplicaciones del grafo de condensación

| Aplicación | Uso del DAG de SCCs |
|-----------|---------------------|
| 2-SAT | Cada variable y su negación en SCCs; satisfacible $\iff$ ninguna variable y su negación están en el mismo SCC |
| Alcanzabilidad | Si $u$ y $v$ están en el mismo SCC, se alcanzan mutuamente. Si no, verificar alcanzabilidad en el DAG |
| Mínimas aristas para fuertemente conexo | Agregar aristas entre fuentes y sumideros del DAG |
| Propagación de información | Un mensaje desde cualquier nodo de un SCC llega a todos los demás |

---

## 3. Algoritmo de Kosaraju

### 3.1 Idea

Kosaraju usa **dos pasadas de DFS**:

1. **Primera pasada**: DFS sobre $G$ original, registrar el **orden de
   finalización** (post-order).
2. **Segunda pasada**: DFS sobre $G^T$ (grafo transpuesto — todas las
   aristas invertidas), procesando los vértices en **orden inverso de
   finalización**.

Cada árbol DFS de la segunda pasada es exactamente un SCC.

### 3.2 ¿Por qué funciona?

**Intuición**: la primera pasada ordena los vértices de modo que los SCCs
"fuente" (sin aristas entrantes desde otros SCCs) tengan los mayores
tiempos de finalización. Al procesar en orden inverso sobre $G^T$, se
garantiza que el DFS no "escape" del SCC actual hacia otros SCCs.

**Argumento formal**: sean $C$ y $C'$ dos SCCs con una arista de $C$ a $C'$
en $G$. Entonces el máximo $\text{fin}[]$ en $C$ es mayor que el máximo
$\text{fin}[]$ en $C'$. Esto se demuestra por casos:

- Si DFS visita $C$ primero: explora $C$ y luego "cae" en $C'$, así que
  todos los vértices de $C'$ finalizan antes que el vértice de $C$ que
  inició la exploración.
- Si DFS visita $C'$ primero: termina $C'$ antes de tocar $C$ (no hay
  arista de $C'$ a $C$ que permita volver, salvo dentro de $C'$), así que
  el máximo fin de $C$ es mayor.

En $G^T$, la arista va de $C'$ a $C$. Al procesar por fin descendente, $C$
se procesa primero. El DFS en $G^T$ desde $C$ no puede alcanzar $C'$
(la arista original $C \to C'$ no existe en $G^T$), así que se queda
dentro de $C$.

### 3.3 Pseudocódigo

```
KOSARAJU(G):
    // Pass 1: DFS on G, record finish order
    visited[] ← all false
    finish_stack ← empty

    for each u in V:
        if not visited[u]:
            DFS1(G, u, visited, finish_stack)

    // Build transposed graph
    G_T ← transpose(G)

    // Pass 2: DFS on G_T in reverse finish order
    comp_id[] ← all -1
    num_scc ← 0

    while finish_stack not empty:
        u ← finish_stack.pop()
        if comp_id[u] == -1:
            DFS2(G_T, u, comp_id, num_scc)
            num_scc ← num_scc + 1

    return num_scc, comp_id[]

DFS1(G, u, visited, finish_stack):
    visited[u] ← true
    for each v in adj[u]:
        if not visited[v]:
            DFS1(G, v, visited, finish_stack)
    finish_stack.push(u)        // post-order

DFS2(G_T, u, comp_id, label):
    comp_id[u] ← label
    for each v in adj_T[u]:
        if comp_id[v] == -1:
            DFS2(G_T, v, comp_id, label)
```

### 3.4 Traza detallada

```
Grafo:
  0 → 1
  1 → 2
  2 → 0     (SCC: {0, 1, 2})
  2 → 3
  3 → 4
  4 → 5
  5 → 3     (SCC: {3, 4, 5})
  5 → 6     (SCC: {6})

=== Pass 1: DFS on G ===

DFS(0):
  0 → 1 → 2 → 0(visited) → 3 → 4 → 5 → 3(visited) → 6
  Finish: 6, 5, 4, 3, 2, 1, 0

finish_stack (top→bottom): [0, 1, 2, 3, 4, 5, 6]

=== Build G^T ===
  1 → 0, 2 → 1, 0 → 2, 3 → 2, 4 → 3, 5 → 4, 3 → 5, 6 → 5

=== Pass 2: DFS on G^T, reverse finish order ===

Pop 0: DFS_T(0) → 2 → 1 → (all visited in {0,1,2})
  SCC 0 = {0, 2, 1}

Pop 3: DFS_T(3) → 5 → 4 → (all visited in {3,4,5})
  SCC 1 = {3, 5, 4}

Pop 6: DFS_T(6) → (no unvisited neighbors)
  SCC 2 = {6}

Result: 3 SCCs → {0,1,2}, {3,4,5}, {6}
```

### 3.5 Complejidad

| Operación | Complejidad |
|-----------|:-----------:|
| DFS Pass 1 | $O(n + m)$ |
| Construir $G^T$ | $O(n + m)$ |
| DFS Pass 2 | $O(n + m)$ |
| **Total** | $O(n + m)$ |
| Espacio | $O(n + m)$ (el grafo transpuesto) |

---

## 4. Algoritmo de Tarjan

### 4.1 Idea

Tarjan encuentra los SCCs con **una sola pasada de DFS**, usando un stack
auxiliar y el array `low[]`:

- `disc[u]`: tiempo de descubrimiento.
- `low[u]`: el menor `disc[]` alcanzable desde el subárbol de $u$ usando
  aristas del árbol DFS y **a lo sumo una arista adicional** (back o cross
  dentro del mismo SCC).
- **Stack**: mantiene los vértices que podrían pertenecer al SCC actual.
- **Raíz del SCC**: un vértice $u$ es raíz de su SCC si $\text{low}[u] = \text{disc}[u]$.
  Cuando se detecta una raíz, se extraen vértices del stack hasta $u$
  (inclusive) — esos forman el SCC.

### 4.2 ¿Por qué funciona?

El stack mantiene los vértices en el orden en que fueron descubiertos y que
aún no han sido asignados a un SCC. Un vértice permanece en el stack
mientras exista la posibilidad de que pertenezca al SCC de algún ancestro.

Cuando $\text{low}[u] = \text{disc}[u]$, ningún vértice en el subárbol de
$u$ puede alcanzar un ancestro de $u$ (fuera de los que están en el stack
debajo de $u$). Por lo tanto, $u$ y todos los vértices sobre él en el stack
forman un SCC completo.

**Condición para actualizar `low[u]`**: al examinar la arista $(u, v)$:
- Si $v$ no fue visitado: recursión, luego `low[u] = min(low[u], low[v])`.
- Si $v$ está **en el stack** (aún no asignado): `low[u] = min(low[u], disc[v])`.
- Si $v$ ya fue asignado a un SCC: ignorar (está en otro SCC ya cerrado).

La condición "en el stack" es crucial y distingue a Tarjan del algoritmo de
puentes (que usa back edges sin verificar pertenencia al stack).

### 4.3 Pseudocódigo

```
TARJAN(G):
    disc[] ← all 0 (unvisited)
    low[] ← all 0
    on_stack[] ← all false
    stack ← empty
    timer ← 0
    comp_id[] ← all -1
    num_scc ← 0

    for each u in V:
        if disc[u] == 0:
            TARJAN-DFS(G, u)

    return num_scc, comp_id[]

TARJAN-DFS(G, u):
    timer ← timer + 1
    disc[u] ← low[u] ← timer
    stack.push(u)
    on_stack[u] ← true

    for each v in adj[u]:
        if disc[v] == 0:               // not visited
            TARJAN-DFS(G, v)
            low[u] ← min(low[u], low[v])
        else if on_stack[v]:            // in current SCC path
            low[u] ← min(low[u], disc[v])

    // If u is root of SCC
    if low[u] == disc[u]:
        repeat:
            w ← stack.pop()
            on_stack[w] ← false
            comp_id[w] ← num_scc
        until w == u
        num_scc ← num_scc + 1
```

### 4.4 Traza detallada

```
Grafo:
  0 → 1, 1 → 2, 2 → 0       (SCC)
  2 → 3, 3 → 4, 4 → 5, 5 → 3  (SCC)
  5 → 6                        (SCC solo)

TARJAN DFS:

Visit 0: disc=1, low=1, stack=[0]
  Visit 1: disc=2, low=2, stack=[0,1]
    Visit 2: disc=3, low=3, stack=[0,1,2]
      Edge 2→0: on_stack[0]=true → low[2]=min(3,disc[0]=1)=1
      Visit 3: disc=4, low=4, stack=[0,1,2,3]
        Visit 4: disc=5, low=5, stack=[0,1,2,3,4]
          Visit 5: disc=6, low=6, stack=[0,1,2,3,4,5]
            Edge 5→3: on_stack[3]=true → low[5]=min(6,disc[3]=4)=4
            Visit 6: disc=7, low=7, stack=[0,1,2,3,4,5,6]
              No neighbors.
              low[6]==disc[6]==7 → ROOT! Pop until 6: SCC₀={6}
              stack=[0,1,2,3,4,5]
            Return to 5.
          low[5]=min(4, low[6]→no update)=4
          Return to 4.
        low[4]=min(5, low[5]=4)=4
        Return to 3.
      low[3]=min(4, low[4]=4)=4
      low[3]==disc[3]==4 → ROOT! Pop until 3: SCC₁={5,4,3}
      stack=[0,1,2]
      Return to 2.
    low[2]=1 (no update from SCC₁, already 1)
    Return to 1.
  low[1]=min(2, low[2]=1)=1
  Return to 0.
low[0]=min(1, low[1]=1)=1
low[0]==disc[0]==1 → ROOT! Pop until 0: SCC₂={2,1,0}
stack=[]

Result: SCC₀={6}, SCC₁={3,4,5}, SCC₂={0,1,2}
(Note: Tarjan discovers SCCs in reverse topological order of the DAG)
```

### 4.5 Complejidad

| Operación | Complejidad |
|-----------|:-----------:|
| DFS (una pasada) | $O(n + m)$ |
| Stack operations | $O(n)$ amortizado |
| **Total** | $O(n + m)$ |
| Espacio | $O(n)$ (stack + arrays, no necesita $G^T$) |

---

## 5. Kosaraju vs Tarjan

| Aspecto | Kosaraju | Tarjan |
|---------|:--------:|:------:|
| Pasadas de DFS | 2 | 1 |
| Necesita $G^T$ | Sí ($O(n + m)$ extra) | No |
| Espacio extra | $O(n + m)$ ($G^T$) | $O(n)$ (stack + `on_stack[]`) |
| Complejidad total | $O(n + m)$ | $O(n + m)$ |
| Simplicidad conceptual | Mayor (dos DFS simples) | Menor (invariantes de `low[]`) |
| Orden de SCCs | Topológico del DAG | **Inverso** topológico |
| Implementación iterativa | Directa | Más compleja |
| Cache performance | Peor (dos grafos) | Mejor (un solo grafo) |

**¿Cuál elegir?**
- **Kosaraju**: cuando la claridad es prioritaria, o cuando ya necesitas $G^T$
  para otro propósito.
- **Tarjan**: cuando el espacio importa, o para grafos muy grandes donde una
  sola pasada es significativamente más rápida en la práctica.

### Orden topológico de SCCs

- **Kosaraju** produce los SCCs en **orden topológico** del DAG de
  condensación (el primer SCC encontrado en la segunda pasada es una fuente).
- **Tarjan** produce los SCCs en **orden inverso topológico** (el primer SCC
  descubierto es un sumidero). Para obtener el orden topológico, invertir
  la secuencia.

---

## 6. Variantes y extensiones

### 6.1 2-SAT

El problema 2-SAT (satisfacibilidad con cláusulas de 2 literales) se reduce
a SCCs en un grafo de implicaciones:

- Cada variable $x$ tiene dos nodos: $x$ y $\lnot x$.
- Cada cláusula $(a \lor b)$ se convierte en dos aristas de implicación:
  $\lnot a \to b$ y $\lnot b \to a$.
- La fórmula es satisfacible $\iff$ para ninguna variable $x$, los nodos
  $x$ y $\lnot x$ están en el mismo SCC.
- La asignación se obtiene del orden topológico de los SCCs.

### 6.2 Hacer un grafo fuertemente conexo

Dado un grafo dirigido con $k$ SCCs, ¿cuántas aristas mínimas hay que
agregar para hacerlo fuertemente conexo?

Sea el DAG de condensación con $s$ fuentes (in-degree 0) y $t$ sumideros
(out-degree 0):

$$\text{aristas necesarias} = \max(s, t) \quad \text{(si } k > 1\text{)}$$

Si $k = 1$, ya es fuertemente conexo (0 aristas necesarias).

### 6.3 SCC en grafos no dirigidos

En grafos no dirigidos el concepto equivalente es el **componente conexo**
ordinario (T03). La "dirección" no existe, así que "alcanzabilidad mutua"
es automática si hay un camino.

Para conceptos más finos en grafos no dirigidos, se usan:
- **Componentes biconexos** (2-edge-connected): sin puentes.
- **Componentes 2-vértice-conexos**: sin puntos de articulación.

---

## 7. Programa completo en C

```c
/*
 * scc.c
 *
 * Strongly Connected Components: Kosaraju and Tarjan algorithms,
 * condensation DAG, and 2-SAT application.
 *
 * Compile: gcc -O2 -o scc scc.c
 * Run:     ./scc
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_V 50
#define MAX_STACK 50

/* =================== ADJACENCY LIST =============================== */

typedef struct {
    int *dests;
    int size, cap;
} NbrList;

typedef struct {
    NbrList adj[MAX_V];
    int n;
} DiGraph;

void digraph_init(DiGraph *g, int n) {
    g->n = n;
    for (int i = 0; i < n; i++) {
        g->adj[i].size = 0;
        g->adj[i].cap = 4;
        g->adj[i].dests = malloc(4 * sizeof(int));
    }
}

void digraph_free(DiGraph *g) {
    for (int i = 0; i < g->n; i++)
        free(g->adj[i].dests);
}

void digraph_add(DiGraph *g, int u, int v) {
    NbrList *list = &g->adj[u];
    if (list->size == list->cap) {
        list->cap *= 2;
        list->dests = realloc(list->dests, list->cap * sizeof(int));
    }
    list->dests[list->size++] = v;
}

void digraph_transpose(const DiGraph *g, DiGraph *gt) {
    digraph_init(gt, g->n);
    for (int u = 0; u < g->n; u++)
        for (int i = 0; i < g->adj[u].size; i++)
            digraph_add(gt, g->adj[u].dests[i], u);
}

/* ============ DEMO 1: Kosaraju's algorithm ======================== */

static bool kos_visited[MAX_V];
static int kos_finish[MAX_V]; /* finish stack */
static int kos_top;
static int kos_comp[MAX_V];

void kosaraju_dfs1(DiGraph *g, int u) {
    kos_visited[u] = true;
    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];
        if (!kos_visited[v])
            kosaraju_dfs1(g, v);
    }
    kos_finish[kos_top++] = u;
}

void kosaraju_dfs2(DiGraph *gt, int u, int label) {
    kos_comp[u] = label;
    for (int i = 0; i < gt->adj[u].size; i++) {
        int v = gt->adj[u].dests[i];
        if (kos_comp[v] == -1)
            kosaraju_dfs2(gt, v, label);
    }
}

int kosaraju(DiGraph *g, int comp_out[]) {
    int n = g->n;

    /* Pass 1 */
    memset(kos_visited, false, sizeof(kos_visited));
    kos_top = 0;
    for (int u = 0; u < n; u++)
        if (!kos_visited[u])
            kosaraju_dfs1(g, u);

    /* Transpose */
    DiGraph gt;
    digraph_transpose(g, &gt);

    /* Pass 2 */
    memset(kos_comp, -1, sizeof(kos_comp));
    int num_scc = 0;
    for (int i = kos_top - 1; i >= 0; i--) {
        int u = kos_finish[i];
        if (kos_comp[u] == -1) {
            kosaraju_dfs2(&gt, u, num_scc);
            num_scc++;
        }
    }

    for (int i = 0; i < n; i++)
        comp_out[i] = kos_comp[i];

    digraph_free(&gt);
    return num_scc;
}

void demo_kosaraju(void) {
    printf("=== Demo 1: Kosaraju's algorithm ===\n");

    DiGraph g;
    digraph_init(&g, 8);
    /* SCC {0,1,2}: 0→1→2→0 */
    digraph_add(&g, 0, 1);
    digraph_add(&g, 1, 2);
    digraph_add(&g, 2, 0);
    /* Bridge: 2→3 */
    digraph_add(&g, 2, 3);
    /* SCC {3,4,5}: 3→4→5→3 */
    digraph_add(&g, 3, 4);
    digraph_add(&g, 4, 5);
    digraph_add(&g, 5, 3);
    /* Bridge: 5→6 */
    digraph_add(&g, 5, 6);
    /* SCC {6,7}: 6→7→6 */
    digraph_add(&g, 6, 7);
    digraph_add(&g, 7, 6);

    printf("Graph: {0→1→2→0}→{3→4→5→3}→{6→7→6}\n\n");

    int comp[MAX_V];
    int num = kosaraju(&g, comp);

    printf("Kosaraju found %d SCCs:\n", num);
    for (int c = 0; c < num; c++) {
        printf("  SCC %d: {", c);
        bool first = true;
        for (int u = 0; u < g.n; u++) {
            if (comp[u] == c) {
                if (!first) printf(", ");
                printf("%d", u);
                first = false;
            }
        }
        printf("}\n");
    }

    digraph_free(&g);
    printf("\n");
}

/* ============ DEMO 2: Tarjan's algorithm ========================== */

static int tar_disc[MAX_V], tar_low[MAX_V];
static bool tar_on_stack[MAX_V];
static int tar_stack[MAX_STACK], tar_stop;
static int tar_timer;
static int tar_comp[MAX_V];
static int tar_num_scc;

void tarjan_dfs(DiGraph *g, int u) {
    tar_timer++;
    tar_disc[u] = tar_low[u] = tar_timer;
    tar_stack[tar_stop++] = u;
    tar_on_stack[u] = true;

    for (int i = 0; i < g->adj[u].size; i++) {
        int v = g->adj[u].dests[i];
        if (tar_disc[v] == 0) {
            tarjan_dfs(g, v);
            if (tar_low[v] < tar_low[u])
                tar_low[u] = tar_low[v];
        } else if (tar_on_stack[v]) {
            if (tar_disc[v] < tar_low[u])
                tar_low[u] = tar_disc[v];
        }
    }

    if (tar_low[u] == tar_disc[u]) {
        int w;
        do {
            w = tar_stack[--tar_stop];
            tar_on_stack[w] = false;
            tar_comp[w] = tar_num_scc;
        } while (w != u);
        tar_num_scc++;
    }
}

int tarjan(DiGraph *g, int comp_out[]) {
    memset(tar_disc, 0, sizeof(tar_disc));
    memset(tar_low, 0, sizeof(tar_low));
    memset(tar_on_stack, false, sizeof(tar_on_stack));
    tar_stop = 0;
    tar_timer = 0;
    tar_num_scc = 0;

    for (int u = 0; u < g->n; u++)
        if (tar_disc[u] == 0)
            tarjan_dfs(g, u);

    for (int i = 0; i < g->n; i++)
        comp_out[i] = tar_comp[i];

    return tar_num_scc;
}

void demo_tarjan(void) {
    printf("=== Demo 2: Tarjan's algorithm ===\n");

    /* Same graph as Demo 1 */
    DiGraph g;
    digraph_init(&g, 8);
    digraph_add(&g, 0, 1);
    digraph_add(&g, 1, 2);
    digraph_add(&g, 2, 0);
    digraph_add(&g, 2, 3);
    digraph_add(&g, 3, 4);
    digraph_add(&g, 4, 5);
    digraph_add(&g, 5, 3);
    digraph_add(&g, 5, 6);
    digraph_add(&g, 6, 7);
    digraph_add(&g, 7, 6);

    int comp[MAX_V];
    int num = tarjan(&g, comp);

    printf("Tarjan found %d SCCs:\n", num);
    for (int c = 0; c < num; c++) {
        printf("  SCC %d: {", c);
        bool first = true;
        for (int u = 0; u < g.n; u++) {
            if (comp[u] == c) {
                if (!first) printf(", ");
                printf("%d", u);
                first = false;
            }
        }
        printf("}\n");
    }

    /* Show disc[] and low[] */
    printf("\ndisc[]: ");
    for (int i = 0; i < g.n; i++) printf("%d=%d ", i, tar_disc[i]);
    printf("\nlow[]:  ");
    for (int i = 0; i < g.n; i++) printf("%d=%d ", i, tar_low[i]);
    printf("\n");

    digraph_free(&g);
    printf("\n");
}

/* ============ DEMO 3: Compare Kosaraju vs Tarjan ================== */

void demo_compare(void) {
    printf("=== Demo 3: Kosaraju vs Tarjan comparison ===\n");

    /* More complex graph with 10 vertices */
    DiGraph g;
    digraph_init(&g, 10);
    /* SCC {0,1,2,3} */
    digraph_add(&g, 0, 1);
    digraph_add(&g, 1, 2);
    digraph_add(&g, 2, 3);
    digraph_add(&g, 3, 0);
    /* SCC {4,5} */
    digraph_add(&g, 4, 5);
    digraph_add(&g, 5, 4);
    /* SCC {6} (singleton) */
    /* SCC {7,8,9} */
    digraph_add(&g, 7, 8);
    digraph_add(&g, 8, 9);
    digraph_add(&g, 9, 7);
    /* Inter-SCC edges */
    digraph_add(&g, 3, 4);
    digraph_add(&g, 3, 6);
    digraph_add(&g, 5, 7);
    digraph_add(&g, 6, 7);

    printf("Graph: SCC{0,1,2,3} → SCC{4,5} → SCC{7,8,9}\n");
    printf("       SCC{0,1,2,3} → SCC{6}   → SCC{7,8,9}\n\n");

    int comp_k[MAX_V], comp_t[MAX_V];
    int nk = kosaraju(&g, comp_k);
    int nt = tarjan(&g, comp_t);

    printf("Kosaraju: %d SCCs, Tarjan: %d SCCs\n\n", nk, nt);

    /* Both should find same SCCs (possibly with different labels) */
    printf("Kosaraju SCC labels: ");
    for (int i = 0; i < g.n; i++) printf("[%d]=%d ", i, comp_k[i]);
    printf("\nTarjan   SCC labels: ");
    for (int i = 0; i < g.n; i++) printf("[%d]=%d ", i, comp_t[i]);
    printf("\n\n");

    /* Verify: same SCC iff same label */
    bool match = true;
    for (int u = 0; u < g.n && match; u++)
        for (int v = u + 1; v < g.n && match; v++)
            if ((comp_k[u] == comp_k[v]) != (comp_t[u] == comp_t[v]))
                match = false;
    printf("Same partition? %s\n", match ? "YES" : "NO");

    digraph_free(&g);
    printf("\n");
}

/* ============ DEMO 4: Condensation DAG ============================ */

void demo_condensation(void) {
    printf("=== Demo 4: Condensation DAG ===\n");

    DiGraph g;
    digraph_init(&g, 8);
    /* Same graph as Demo 1 */
    digraph_add(&g, 0, 1);
    digraph_add(&g, 1, 2);
    digraph_add(&g, 2, 0);
    digraph_add(&g, 2, 3);
    digraph_add(&g, 3, 4);
    digraph_add(&g, 4, 5);
    digraph_add(&g, 5, 3);
    digraph_add(&g, 5, 6);
    digraph_add(&g, 6, 7);
    digraph_add(&g, 7, 6);

    int comp[MAX_V];
    int num = kosaraju(&g, comp);

    printf("Original graph (8 vertices) → %d SCCs\n\n", num);

    /* Print SCC members */
    for (int c = 0; c < num; c++) {
        printf("  SCC %d: {", c);
        bool first = true;
        for (int u = 0; u < g.n; u++) {
            if (comp[u] == c) {
                if (!first) printf(", ");
                printf("%d", u);
                first = false;
            }
        }
        printf("}\n");
    }

    /* Build condensation DAG */
    printf("\nCondensation DAG edges:\n");
    bool dag_edge[MAX_V][MAX_V];
    memset(dag_edge, false, sizeof(dag_edge));

    for (int u = 0; u < g.n; u++) {
        for (int i = 0; i < g.adj[u].size; i++) {
            int v = g.adj[u].dests[i];
            int cu = comp[u], cv = comp[v];
            if (cu != cv && !dag_edge[cu][cv]) {
                dag_edge[cu][cv] = true;
                printf("  SCC %d → SCC %d\n", cu, cv);
            }
        }
    }

    /* In/out degree of condensation */
    int in_deg[MAX_V] = {0}, out_deg[MAX_V] = {0};
    for (int i = 0; i < num; i++)
        for (int j = 0; j < num; j++)
            if (dag_edge[i][j]) {
                out_deg[i]++;
                in_deg[j]++;
            }

    printf("\nSources (in_deg=0): ");
    for (int i = 0; i < num; i++)
        if (in_deg[i] == 0) printf("SCC %d ", i);

    printf("\nSinks (out_deg=0):  ");
    for (int i = 0; i < num; i++)
        if (out_deg[i] == 0) printf("SCC %d ", i);
    printf("\n");

    int sources = 0, sinks = 0;
    for (int i = 0; i < num; i++) {
        if (in_deg[i] == 0) sources++;
        if (out_deg[i] == 0) sinks++;
    }
    int needed = (num == 1) ? 0 : (sources > sinks ? sources : sinks);
    printf("\nEdges needed for strong connectivity: %d\n", needed);

    digraph_free(&g);
    printf("\n");
}

/* ============ DEMO 5: DAG detection =============================== */

void demo_dag_check(void) {
    printf("=== Demo 5: DAG detection via SCCs ===\n");

    /* Test 1: DAG */
    DiGraph dag;
    digraph_init(&dag, 5);
    digraph_add(&dag, 0, 1);
    digraph_add(&dag, 0, 2);
    digraph_add(&dag, 1, 3);
    digraph_add(&dag, 2, 3);
    digraph_add(&dag, 3, 4);

    int comp_d[MAX_V];
    int nd = tarjan(&dag, comp_d);
    bool is_dag = (nd == dag.n);
    printf("Graph 1 (0→1,2; 1,2→3; 3→4): %d SCCs for %d vertices → %s\n",
           nd, dag.n, is_dag ? "DAG" : "has cycles");

    /* Test 2: has cycle */
    DiGraph cyclic;
    digraph_init(&cyclic, 4);
    digraph_add(&cyclic, 0, 1);
    digraph_add(&cyclic, 1, 2);
    digraph_add(&cyclic, 2, 0);
    digraph_add(&cyclic, 2, 3);

    int comp_c[MAX_V];
    int nc = tarjan(&cyclic, comp_c);
    bool is_dag2 = (nc == cyclic.n);
    printf("Graph 2 (0→1→2→0, 2→3):       %d SCCs for %d vertices → %s\n",
           nc, cyclic.n, is_dag2 ? "DAG" : "has cycles");

    /* Test 3: single strongly connected */
    DiGraph strong;
    digraph_init(&strong, 3);
    digraph_add(&strong, 0, 1);
    digraph_add(&strong, 1, 2);
    digraph_add(&strong, 2, 0);

    int comp_s[MAX_V];
    int ns = tarjan(&strong, comp_s);
    printf("Graph 3 (0→1→2→0):             %d SCC  for %d vertices → %s\n",
           ns, strong.n, ns == 1 ? "strongly connected" : "not strongly connected");

    digraph_free(&dag);
    digraph_free(&cyclic);
    digraph_free(&strong);
    printf("\n");
}

/* ============ DEMO 6: Reachability via SCCs ======================= */

void demo_reachability(void) {
    printf("=== Demo 6: Reachability queries via SCCs ===\n");

    DiGraph g;
    digraph_init(&g, 8);
    digraph_add(&g, 0, 1);
    digraph_add(&g, 1, 2);
    digraph_add(&g, 2, 0);
    digraph_add(&g, 2, 3);
    digraph_add(&g, 3, 4);
    digraph_add(&g, 4, 5);
    digraph_add(&g, 5, 3);
    digraph_add(&g, 5, 6);
    digraph_add(&g, 6, 7);
    digraph_add(&g, 7, 6);

    int comp[MAX_V];
    int num = kosaraju(&g, comp);

    /* Build condensation reachability with BFS/DFS */
    bool dag_adj[MAX_V][MAX_V];
    memset(dag_adj, false, sizeof(dag_adj));
    for (int u = 0; u < g.n; u++)
        for (int i = 0; i < g.adj[u].size; i++) {
            int v = g.adj[u].dests[i];
            if (comp[u] != comp[v])
                dag_adj[comp[u]][comp[v]] = true;
        }

    /* Transitive closure on DAG (Floyd-Warshall) */
    bool reach[MAX_V][MAX_V];
    memset(reach, false, sizeof(reach));
    for (int i = 0; i < num; i++) {
        reach[i][i] = true;
        for (int j = 0; j < num; j++)
            if (dag_adj[i][j]) reach[i][j] = true;
    }
    for (int k = 0; k < num; k++)
        for (int i = 0; i < num; i++)
            for (int j = 0; j < num; j++)
                if (reach[i][k] && reach[k][j])
                    reach[i][j] = true;

    printf("%d SCCs. Reachability queries:\n\n", num);

    int queries[][2] = {{0, 7}, {0, 4}, {7, 0}, {6, 3}, {3, 6}};
    int nq = 5;

    for (int q = 0; q < nq; q++) {
        int u = queries[q][0], v = queries[q][1];
        bool same_scc = (comp[u] == comp[v]);
        bool reachable = same_scc || reach[comp[u]][comp[v]];
        printf("  %d → %d: ", u, v);
        if (same_scc)
            printf("SAME SCC (%d) → mutually reachable\n", comp[u]);
        else if (reachable)
            printf("SCC %d → SCC %d → reachable (one-way)\n",
                   comp[u], comp[v]);
        else
            printf("SCC %d ↛ SCC %d → NOT reachable\n",
                   comp[u], comp[v]);
    }

    digraph_free(&g);
    printf("\n");
}

/* ======================== MAIN ==================================== */

int main(void) {
    demo_kosaraju();
    demo_tarjan();
    demo_compare();
    demo_condensation();
    demo_dag_check();
    demo_reachability();
    return 0;
}
```

### Salida esperada

```
=== Demo 1: Kosaraju's algorithm ===
Graph: {0→1→2→0}→{3→4→5→3}→{6→7→6}

Kosaraju found 3 SCCs:
  SCC 0: {0, 1, 2}
  SCC 1: {3, 4, 5}
  SCC 2: {6, 7}

=== Demo 2: Tarjan's algorithm ===
Tarjan found 3 SCCs:
  SCC 0: {6, 7}
  SCC 1: {3, 4, 5}
  SCC 2: {0, 1, 2}

disc[]: 0=1 1=2 2=3 3=4 4=5 5=6 6=7 7=8
low[]:  0=1 1=1 2=1 3=4 4=4 5=4 6=7 7=7

=== Demo 3: Kosaraju vs Tarjan comparison ===
Graph: SCC{0,1,2,3} → SCC{4,5} → SCC{7,8,9}
       SCC{0,1,2,3} → SCC{6}   → SCC{7,8,9}

Kosaraju: 4 SCCs, Tarjan: 4 SCCs

Kosaraju SCC labels: [0]=0 [1]=0 [2]=0 [3]=0 [4]=1 [5]=1 [6]=2 [7]=3 [8]=3 [9]=3
Tarjan   SCC labels: [0]=3 [1]=3 [2]=3 [3]=3 [4]=2 [5]=2 [6]=1 [7]=0 [8]=0 [9]=0

Same partition? YES

=== Demo 4: Condensation DAG ===
Original graph (8 vertices) → 3 SCCs

  SCC 0: {0, 1, 2}
  SCC 1: {3, 4, 5}
  SCC 2: {6, 7}

Condensation DAG edges:
  SCC 0 → SCC 1
  SCC 1 → SCC 2

Sources (in_deg=0): SCC 0
Sinks (out_deg=0):  SCC 2

Edges needed for strong connectivity: 1

=== Demo 5: DAG detection via SCCs ===
Graph 1 (0→1,2; 1,2→3; 3→4): 5 SCCs for 5 vertices → DAG
Graph 2 (0→1→2→0, 2→3):       2 SCCs for 4 vertices → has cycles
Graph 3 (0→1→2→0):             1 SCC  for 3 vertices → strongly connected

=== Demo 6: Reachability queries via SCCs ===
3 SCCs. Reachability queries:

  0 → 7: SCC 0 → SCC 2 → reachable (one-way)
  0 → 4: SCC 0 → SCC 1 → reachable (one-way)
  7 → 0: SCC 2 ↛ SCC 0 → NOT reachable
  6 → 3: SCC 2 ↛ SCC 1 → NOT reachable
  3 → 6: SCC 1 → SCC 2 → reachable (one-way)
```

---

## 8. Programa completo en Rust

```rust
/*
 * scc.rs
 *
 * Strongly Connected Components: Kosaraju, Tarjan, condensation DAG,
 * DAG detection, reachability, 2-SAT, and minimum edges for strong
 * connectivity.
 *
 * Compile: rustc -O -o scc scc.rs
 * Run:     ./scc
 */

use std::collections::VecDeque;

/* =================== DIRECTED GRAPH =============================== */

struct DiGraph {
    adj: Vec<Vec<usize>>,
}

impl DiGraph {
    fn new(n: usize) -> Self {
        Self { adj: vec![vec![]; n] }
    }

    fn add_edge(&mut self, u: usize, v: usize) {
        self.adj[u].push(v);
    }

    fn n(&self) -> usize {
        self.adj.len()
    }

    fn transpose(&self) -> DiGraph {
        let mut gt = DiGraph::new(self.n());
        for u in 0..self.n() {
            for &v in &self.adj[u] {
                gt.add_edge(v, u);
            }
        }
        gt
    }
}

/* =================== KOSARAJU ==================================== */

fn kosaraju(g: &DiGraph) -> (usize, Vec<usize>) {
    let n = g.n();

    // Pass 1: DFS on G, record finish order
    let mut visited = vec![false; n];
    let mut finish_stack = Vec::with_capacity(n);

    fn dfs1(u: usize, g: &DiGraph, visited: &mut [bool], stack: &mut Vec<usize>) {
        visited[u] = true;
        for &v in &g.adj[u] {
            if !visited[v] {
                dfs1(v, g, visited, stack);
            }
        }
        stack.push(u);
    }

    for u in 0..n {
        if !visited[u] {
            dfs1(u, g, &mut visited, &mut finish_stack);
        }
    }

    // Pass 2: DFS on G^T in reverse finish order
    let gt = g.transpose();
    let mut comp = vec![usize::MAX; n];
    let mut num_scc = 0;

    fn dfs2(u: usize, gt: &DiGraph, comp: &mut [usize], label: usize) {
        comp[u] = label;
        for &v in &gt.adj[u] {
            if comp[v] == usize::MAX {
                dfs2(v, gt, comp, label);
            }
        }
    }

    for &u in finish_stack.iter().rev() {
        if comp[u] == usize::MAX {
            dfs2(u, &gt, &mut comp, num_scc);
            num_scc += 1;
        }
    }

    (num_scc, comp)
}

/* =================== TARJAN ====================================== */

fn tarjan(g: &DiGraph) -> (usize, Vec<usize>) {
    let n = g.n();
    let mut disc = vec![0u32; n];
    let mut low = vec![0u32; n];
    let mut on_stack = vec![false; n];
    let mut stack = Vec::with_capacity(n);
    let mut timer = 0u32;
    let mut comp = vec![usize::MAX; n];
    let mut num_scc = 0usize;

    fn dfs(
        u: usize, g: &DiGraph,
        disc: &mut [u32], low: &mut [u32],
        on_stack: &mut [bool], stack: &mut Vec<usize>,
        timer: &mut u32, comp: &mut [usize], num_scc: &mut usize,
    ) {
        *timer += 1;
        disc[u] = *timer;
        low[u] = *timer;
        stack.push(u);
        on_stack[u] = true;

        for &v in &g.adj[u] {
            if disc[v] == 0 {
                dfs(v, g, disc, low, on_stack, stack, timer, comp, num_scc);
                low[u] = low[u].min(low[v]);
            } else if on_stack[v] {
                low[u] = low[u].min(disc[v]);
            }
        }

        if low[u] == disc[u] {
            loop {
                let w = stack.pop().unwrap();
                on_stack[w] = false;
                comp[w] = *num_scc;
                if w == u { break; }
            }
            *num_scc += 1;
        }
    }

    for u in 0..n {
        if disc[u] == 0 {
            dfs(u, g, &mut disc, &mut low, &mut on_stack, &mut stack,
                &mut timer, &mut comp, &mut num_scc);
        }
    }

    (num_scc, comp)
}

/* ======== Demo 1: Kosaraju ======================================== */

fn print_sccs(num: usize, comp: &[usize], n: usize) {
    for c in 0..num {
        let members: Vec<usize> = (0..n).filter(|&u| comp[u] == c).collect();
        let names: Vec<String> = members.iter().map(|v| v.to_string()).collect();
        println!("  SCC {}: {{{}}}", c, names.join(", "));
    }
}

fn demo_kosaraju() {
    println!("=== Demo 1: Kosaraju's algorithm ===");

    let mut g = DiGraph::new(8);
    // SCC {0,1,2}
    g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0);
    // SCC {3,4,5}
    g.add_edge(2, 3);
    g.add_edge(3, 4); g.add_edge(4, 5); g.add_edge(5, 3);
    // SCC {6,7}
    g.add_edge(5, 6);
    g.add_edge(6, 7); g.add_edge(7, 6);

    println!("Graph: {{0→1→2→0}}→{{3→4→5→3}}→{{6→7→6}}\n");

    let (num, comp) = kosaraju(&g);
    println!("Kosaraju: {} SCCs", num);
    print_sccs(num, &comp, g.n());

    // Verify topological order: SCC 0 (source) before SCC 1 before SCC 2 (sink)
    println!("\nSCC discovery order (Kosaraju = topological):");
    for c in 0..num {
        let members: Vec<usize> = (0..g.n()).filter(|&u| comp[u] == c).collect();
        println!("  SCC {} = {:?}", c, members);
    }
    println!();
}

/* ======== Demo 2: Tarjan ========================================== */

fn demo_tarjan() {
    println!("=== Demo 2: Tarjan's algorithm ===");

    let mut g = DiGraph::new(8);
    g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0);
    g.add_edge(2, 3);
    g.add_edge(3, 4); g.add_edge(4, 5); g.add_edge(5, 3);
    g.add_edge(5, 6);
    g.add_edge(6, 7); g.add_edge(7, 6);

    let (num, comp) = tarjan(&g);
    println!("Tarjan: {} SCCs", num);
    print_sccs(num, &comp, g.n());
    println!("\nNote: Tarjan discovers SCCs in REVERSE topological order.");
    println!();
}

/* ======== Demo 3: Condensation DAG ================================ */

fn build_condensation(g: &DiGraph, comp: &[usize], num_scc: usize) -> DiGraph {
    let mut dag = DiGraph::new(num_scc);
    let mut seen = vec![vec![false; num_scc]; num_scc];

    for u in 0..g.n() {
        for &v in &g.adj[u] {
            let cu = comp[u];
            let cv = comp[v];
            if cu != cv && !seen[cu][cv] {
                seen[cu][cv] = true;
                dag.add_edge(cu, cv);
            }
        }
    }
    dag
}

fn demo_condensation() {
    println!("=== Demo 3: Condensation DAG ===");

    let mut g = DiGraph::new(8);
    g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0);
    g.add_edge(2, 3);
    g.add_edge(3, 4); g.add_edge(4, 5); g.add_edge(5, 3);
    g.add_edge(5, 6);
    g.add_edge(6, 7); g.add_edge(7, 6);

    let (num, comp) = kosaraju(&g);
    let dag = build_condensation(&g, &comp, num);

    println!("{} vertices → {} SCCs\n", g.n(), num);
    print_sccs(num, &comp, g.n());

    println!("\nCondensation DAG edges:");
    for u in 0..dag.n() {
        for &v in &dag.adj[u] {
            println!("  SCC {} → SCC {}", u, v);
        }
    }

    // Sources and sinks
    let mut in_deg = vec![0usize; num];
    for u in 0..num {
        for &v in &dag.adj[u] { in_deg[v] += 1; }
    }
    let sources: Vec<usize> = (0..num).filter(|&u| in_deg[u] == 0).collect();
    let sinks: Vec<usize> = (0..num).filter(|&u| dag.adj[u].is_empty()).collect();

    println!("\nSources: {:?}", sources);
    println!("Sinks:   {:?}", sinks);

    let needed = if num == 1 { 0 } else { sources.len().max(sinks.len()) };
    println!("Edges to make strongly connected: {}", needed);
    println!();
}

/* ======== Demo 4: Verify strongly connected ======================= */

fn is_strongly_connected(g: &DiGraph) -> bool {
    let (num, _) = tarjan(g);
    num == 1
}

fn demo_strong_check() {
    println!("=== Demo 4: Strongly connected check ===");

    let cases: Vec<(&str, Vec<(usize, usize)>, usize)> = vec![
        ("triangle 0→1→2→0", vec![(0,1),(1,2),(2,0)], 3),
        ("chain 0→1→2", vec![(0,1),(1,2)], 3),
        ("complete K3", vec![(0,1),(1,0),(1,2),(2,1),(0,2),(2,0)], 3),
        ("two cycles linked", vec![(0,1),(1,0),(2,3),(3,2),(1,2),(3,0)], 4),
    ];

    for (name, edges, n) in &cases {
        let mut g = DiGraph::new(*n);
        for &(u, v) in edges { g.add_edge(u, v); }
        let (num, _) = tarjan(&g);
        println!("  {}: {} SCC(s) → {}",
                 name, num,
                 if num == 1 { "strongly connected" }
                 else { "NOT strongly connected" });
    }
    println!();
}

/* ======== Demo 5: Reachability via SCCs =========================== */

fn demo_reachability() {
    println!("=== Demo 5: Reachability via condensation ===");

    let mut g = DiGraph::new(8);
    g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0);
    g.add_edge(2, 3);
    g.add_edge(3, 4); g.add_edge(4, 5); g.add_edge(5, 3);
    g.add_edge(5, 6);
    g.add_edge(6, 7); g.add_edge(7, 6);

    let (num, comp) = kosaraju(&g);
    let dag = build_condensation(&g, &comp, num);

    // Transitive closure on DAG via BFS from each node
    let mut reach = vec![vec![false; num]; num];
    for start in 0..num {
        reach[start][start] = true;
        let mut queue = VecDeque::new();
        queue.push_back(start);
        while let Some(u) = queue.pop_front() {
            for &v in &dag.adj[u] {
                if !reach[start][v] {
                    reach[start][v] = true;
                    queue.push_back(v);
                }
            }
        }
    }

    let queries = [(0,7), (1,5), (7,0), (6,3), (4,7)];
    println!("{} SCCs. Queries:\n", num);

    for (u, v) in queries {
        let cu = comp[u];
        let cv = comp[v];
        let mutual = cu == cv;
        let fwd = reach[cu][cv];
        let bwd = reach[cv][cu];

        print!("  {} → {}: ", u, v);
        if mutual {
            println!("same SCC {} — mutually reachable", cu);
        } else if fwd && bwd {
            println!("SCC {} ↔ SCC {} — mutually reachable (both directions)", cu, cv);
        } else if fwd {
            println!("SCC {} → SCC {} — one-way reachable", cu, cv);
        } else {
            println!("SCC {} ↛ SCC {} — NOT reachable", cu, cv);
        }
    }
    println!();
}

/* ======== Demo 6: 2-SAT via SCCs ================================== */

/// Solve 2-SAT with n variables and clauses (a OR b).
/// Variables: 0..n-1. Literal x = 2*x (positive), 2*x+1 (negative).
fn solve_2sat(n_vars: usize, clauses: &[(usize, usize)]) -> Option<Vec<bool>> {
    let n = 2 * n_vars; // 2 nodes per variable

    let neg = |x: usize| -> usize { x ^ 1 };

    let mut g = DiGraph::new(n);
    for &(a, b) in clauses {
        // (a OR b) ≡ (¬a → b) AND (¬b → a)
        g.add_edge(neg(a), b);
        g.add_edge(neg(b), a);
    }

    let (_, comp) = tarjan(&g);

    // Check: x and ¬x must NOT be in same SCC
    for x in 0..n_vars {
        if comp[2 * x] == comp[2 * x + 1] {
            return None; // unsatisfiable
        }
    }

    // Assignment: x = true if comp[x] > comp[¬x]
    // (Tarjan finds SCCs in reverse topo order, so higher comp_id = earlier in topo)
    let mut assignment = vec![false; n_vars];
    for x in 0..n_vars {
        assignment[x] = comp[2 * x] > comp[2 * x + 1];
    }
    Some(assignment)
}

fn demo_2sat() {
    println!("=== Demo 6: 2-SAT via SCCs ===");

    // Variables: x0, x1, x2
    // Literals: x0=0, ¬x0=1, x1=2, ¬x1=3, x2=4, ¬x2=5

    // Satisfiable: (x0 OR x1) AND (¬x0 OR x2) AND (¬x1 OR ¬x2)
    let clauses1 = vec![(0, 2), (1, 4), (3, 5)];
    println!("Clauses: (x0∨x1) ∧ (¬x0∨x2) ∧ (¬x1∨¬x2)");
    match solve_2sat(3, &clauses1) {
        Some(assign) => {
            let vals: Vec<String> = assign.iter().enumerate()
                .map(|(i, &v)| format!("x{}={}", i, v))
                .collect();
            println!("  Satisfiable: {}", vals.join(", "));

            // Verify
            let check = |lit: usize| -> bool {
                let var = lit / 2;
                if lit % 2 == 0 { assign[var] } else { !assign[var] }
            };
            let ok = clauses1.iter().all(|&(a, b)| check(a) || check(b));
            println!("  Verification: {}", if ok { "PASS" } else { "FAIL" });
        }
        None => println!("  Unsatisfiable"),
    }

    // Unsatisfiable: (x0) AND (¬x0)  →  (x0 OR x0) AND (¬x0 OR ¬x0)
    let clauses2 = vec![(0, 0), (1, 1)];
    println!("\nClauses: (x0∨x0) ∧ (¬x0∨¬x0)  [= x0 AND ¬x0]");
    match solve_2sat(1, &clauses2) {
        Some(_) => println!("  Satisfiable (unexpected)"),
        None => println!("  Unsatisfiable (correct — x0 and ¬x0 in same SCC)"),
    }
    println!();
}

/* ======== Demo 7: Large random-ish graph ========================== */

fn demo_larger_graph() {
    println!("=== Demo 7: Larger graph analysis ===");

    // 15 vertices, several SCCs of different sizes
    let mut g = DiGraph::new(15);

    // SCC {0,1,2,3}: 4-cycle
    g.add_edge(0,1); g.add_edge(1,2); g.add_edge(2,3); g.add_edge(3,0);
    // SCC {4,5,6}: triangle
    g.add_edge(4,5); g.add_edge(5,6); g.add_edge(6,4);
    // SCC {7}: singleton
    // SCC {8,9}: pair
    g.add_edge(8,9); g.add_edge(9,8);
    // SCC {10,11,12,13,14}: 5-cycle
    g.add_edge(10,11); g.add_edge(11,12); g.add_edge(12,13);
    g.add_edge(13,14); g.add_edge(14,10);

    // Inter-SCC edges
    g.add_edge(3, 4);  // SCC0 → SCC1
    g.add_edge(2, 7);  // SCC0 → SCC2
    g.add_edge(6, 8);  // SCC1 → SCC3
    g.add_edge(7, 8);  // SCC2 → SCC3
    g.add_edge(9, 10); // SCC3 → SCC4

    let (num_k, comp_k) = kosaraju(&g);
    let (num_t, comp_t) = tarjan(&g);

    println!("15 vertices, {} SCCs (Kosaraju={}, Tarjan={})\n", num_k, num_k, num_t);

    println!("Kosaraju SCCs:");
    print_sccs(num_k, &comp_k, g.n());

    // Component sizes
    let mut sizes = vec![0usize; num_k];
    for &c in &comp_k { sizes[c] += 1; }
    println!("\nSCC sizes: {:?}", sizes);
    println!("Largest SCC: {} vertices", sizes.iter().max().unwrap());
    println!("Singleton SCCs: {}", sizes.iter().filter(|&&s| s == 1).count());

    // Condensation
    let dag = build_condensation(&g, &comp_k, num_k);
    println!("\nCondensation DAG ({} nodes):", num_k);
    for u in 0..num_k {
        if !dag.adj[u].is_empty() {
            let targets: Vec<String> = dag.adj[u].iter().map(|v| v.to_string()).collect();
            println!("  SCC {} → [{}]", u, targets.join(", "));
        }
    }
    println!();
}

/* ======== Demo 8: Iterative Tarjan ================================ */

fn tarjan_iterative(g: &DiGraph) -> (usize, Vec<usize>) {
    let n = g.n();
    let mut disc = vec![0u32; n];
    let mut low = vec![0u32; n];
    let mut on_stack = vec![false; n];
    let mut scc_stack = Vec::new();
    let mut timer = 0u32;
    let mut comp = vec![usize::MAX; n];
    let mut num_scc = 0;

    // Call stack: (vertex, neighbor_index)
    let mut call_stack: Vec<(usize, usize)> = Vec::new();

    for start in 0..n {
        if disc[start] != 0 { continue; }

        call_stack.push((start, 0));
        timer += 1;
        disc[start] = timer;
        low[start] = timer;
        scc_stack.push(start);
        on_stack[start] = true;

        while let Some(&mut (u, ref mut idx)) = call_stack.last_mut() {
            if *idx < g.adj[u].len() {
                let v = g.adj[u][*idx];
                *idx += 1;

                if disc[v] == 0 {
                    // "Recurse" into v
                    timer += 1;
                    disc[v] = timer;
                    low[v] = timer;
                    scc_stack.push(v);
                    on_stack[v] = true;
                    call_stack.push((v, 0));
                } else if on_stack[v] {
                    low[u] = low[u].min(disc[v]);
                }
            } else {
                // Done with u — check if SCC root
                if low[u] == disc[u] {
                    loop {
                        let w = scc_stack.pop().unwrap();
                        on_stack[w] = false;
                        comp[w] = num_scc;
                        if w == u { break; }
                    }
                    num_scc += 1;
                }

                // "Return" from u — update parent's low
                call_stack.pop();
                if let Some(&(parent, _)) = call_stack.last() {
                    low[parent] = low[parent].min(low[u]);
                }
            }
        }
    }

    (num_scc, comp)
}

fn demo_iterative_tarjan() {
    println!("=== Demo 8: Iterative Tarjan (stack-safe) ===");

    let mut g = DiGraph::new(8);
    g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0);
    g.add_edge(2, 3);
    g.add_edge(3, 4); g.add_edge(4, 5); g.add_edge(5, 3);
    g.add_edge(5, 6);
    g.add_edge(6, 7); g.add_edge(7, 6);

    let (num_r, comp_r) = tarjan(&g);
    let (num_i, comp_i) = tarjan_iterative(&g);

    println!("Recursive Tarjan: {} SCCs", num_r);
    print_sccs(num_r, &comp_r, g.n());

    println!("\nIterative Tarjan: {} SCCs", num_i);
    print_sccs(num_i, &comp_i, g.n());

    // Verify same partition
    let same = (0..g.n()).all(|u| {
        (u+1..g.n()).all(|v| {
            (comp_r[u] == comp_r[v]) == (comp_i[u] == comp_i[v])
        })
    });
    println!("\nSame partition? {}", if same { "YES" } else { "NO" });
    println!("Iterative version is safe for deep graphs (no stack overflow).");
    println!();
}

/* ======================== MAIN ==================================== */

fn main() {
    demo_kosaraju();
    demo_tarjan();
    demo_condensation();
    demo_strong_check();
    demo_reachability();
    demo_2sat();
    demo_larger_graph();
    demo_iterative_tarjan();
}
```

### Salida esperada

```
=== Demo 1: Kosaraju's algorithm ===
Graph: {0→1→2→0}→{3→4→5→3}→{6→7→6}

Kosaraju: 3 SCCs
  SCC 0: {0, 1, 2}
  SCC 1: {3, 4, 5}
  SCC 2: {6, 7}

SCC discovery order (Kosaraju = topological):
  SCC 0 = [0, 1, 2]
  SCC 1 = [3, 4, 5]
  SCC 2 = [6, 7]

=== Demo 2: Tarjan's algorithm ===
Tarjan: 3 SCCs
  SCC 0: {6, 7}
  SCC 1: {3, 4, 5}
  SCC 2: {0, 1, 2}

Note: Tarjan discovers SCCs in REVERSE topological order.

=== Demo 3: Condensation DAG ===
8 vertices → 3 SCCs

  SCC 0: {0, 1, 2}
  SCC 1: {3, 4, 5}
  SCC 2: {6, 7}

Condensation DAG edges:
  SCC 0 → SCC 1
  SCC 1 → SCC 2

Sources: [0]
Sinks:   [2]
Edges to make strongly connected: 1

=== Demo 4: Strongly connected check ===
  triangle 0→1→2→0: 1 SCC(s) → strongly connected
  chain 0→1→2: 3 SCC(s) → NOT strongly connected
  complete K3: 1 SCC(s) → strongly connected
  two cycles linked: 1 SCC(s) → strongly connected

=== Demo 5: Reachability via condensation ===
3 SCCs. Queries:

  0 → 7: SCC 0 → SCC 2 — one-way reachable
  1 → 5: SCC 0 → SCC 1 — one-way reachable
  7 → 0: SCC 2 ↛ SCC 0 — NOT reachable
  6 → 3: SCC 2 ↛ SCC 1 — NOT reachable
  4 → 7: SCC 1 → SCC 2 — one-way reachable

=== Demo 6: 2-SAT via SCCs ===
Clauses: (x0∨x1) ∧ (¬x0∨x2) ∧ (¬x1∨¬x2)
  Satisfiable: x0=true, x1=false, x2=true
  Verification: PASS

Clauses: (x0∨x0) ∧ (¬x0∨¬x0)  [= x0 AND ¬x0]
  Unsatisfiable (correct — x0 and ¬x0 in same SCC)

=== Demo 7: Larger graph analysis ===
15 vertices, 5 SCCs (Kosaraju=5, Tarjan=5)

Kosaraju SCCs:
  SCC 0: {0, 1, 2, 3}
  SCC 1: {4, 5, 6}
  SCC 2: {7}
  SCC 3: {8, 9}
  SCC 4: {10, 11, 12, 13, 14}

SCC sizes: [4, 3, 1, 2, 5]
Largest SCC: 5 vertices
Singleton SCCs: 1

Condensation DAG (5 nodes):
  SCC 0 → [1, 2]
  SCC 1 → [3]
  SCC 2 → [3]
  SCC 3 → [4]

=== Demo 8: Iterative Tarjan (stack-safe) ===
Recursive Tarjan: 3 SCCs
  SCC 0: {6, 7}
  SCC 1: {3, 4, 5}
  SCC 2: {0, 1, 2}

Iterative Tarjan: 3 SCCs
  SCC 0: {6, 7}
  SCC 1: {3, 4, 5}
  SCC 2: {0, 1, 2}

Same partition? YES
Iterative version is safe for deep graphs (no stack overflow).
```

---

## 9. Ejercicios

### Ejercicio 1 — Identificar SCCs a mano

Grafo dirigido con aristas:
$0 \to 1,\ 1 \to 2,\ 2 \to 0,\ 2 \to 3,\ 3 \to 4,\ 4 \to 3,\ 4 \to 5$.

Identifica los SCCs sin ejecutar ningún algoritmo (solo razonando sobre
caminos).

<details>
<summary>¿Cuáles son los SCCs?</summary>

- $\{0, 1, 2\}$: ciclo $0 \to 1 \to 2 \to 0$, todos mutuamente alcanzables.
- $\{3, 4\}$: ciclo $3 \to 4 \to 3$.
- $\{5\}$: singleton, no hay arista de vuelta a $\{3,4\}$ ni a $\{0,1,2\}$.

Total: **3 SCCs**. El vértice 5 no puede regresar a nadie porque no tiene
aristas salientes en el enunciado.
</details>

---

### Ejercicio 2 — Kosaraju paso a paso

Usando el grafo del ejercicio 1, ejecuta Kosaraju:

a) Primera pasada DFS (desde vértice 0), registra el orden de finalización.
b) Construye $G^T$.
c) Segunda pasada en orden inverso de finalización.

<details>
<summary>Traza completa</summary>

**Pass 1** (DFS en $G$, desde 0):
$0 \to 1 \to 2 \to 0$ (visitado) $\to 3 \to 4 \to 3$ (visitado)
$\to 5$. Finish: 5, 4, 3, 2, 1, 0.

Stack (top→bottom): [0, 1, 2, 3, 4, 5].

**$G^T$**: $1\to0, 2\to1, 0\to2, 3\to2, 4\to3, 3\to4, 5\to4$.

**Pass 2** (procesar top del stack):
- Pop 0: DFS en $G^T$ desde 0 → 2 → 1 (todos en $\{0,1,2\}$ visitados).
  SCC₀ = $\{0, 2, 1\}$.
- Pop 3: DFS en $G^T$ desde 3 → 4 (ya visitados en $\{3,4\}$).
  SCC₁ = $\{3, 4\}$.
- Pop 5: DFS en $G^T$ desde 5 → solo.
  SCC₂ = $\{5\}$.
</details>

---

### Ejercicio 3 — Tarjan paso a paso

Mismo grafo del ejercicio 1. Ejecuta Tarjan desde vértice 0, registrando
`disc[]`, `low[]`, y el estado del stack en cada paso.

<details>
<summary>Traza de disc/low y stack</summary>

```
Visit 0: disc=1,low=1, stack=[0]
  Visit 1: disc=2,low=2, stack=[0,1]
    Visit 2: disc=3,low=3, stack=[0,1,2]
      Edge 2→0: on_stack → low[2]=min(3,1)=1
      Visit 3: disc=4,low=4, stack=[0,1,2,3]
        Visit 4: disc=5,low=5, stack=[0,1,2,3,4]
          Edge 4→3: on_stack → low[4]=min(5,4)=4
          Visit 5: disc=6,low=6, stack=[0,1,2,3,4,5]
            (no neighbors)
            low[5]==disc[5] → ROOT! Pop: SCC₀={5}
            stack=[0,1,2,3,4]
          low[4]=min(4,...) stays 4
        low[4]==disc[4]? 4≠5 → NO. Pero low[4]=4=disc[3+1]...
        Wait: disc[4]=5, low[4]=4 → 4≠5 → NOT root.
        Return to 3.
      low[3]=min(4, low[4]=4)=4
      low[3]==disc[3]==4 → ROOT! Pop until 3: SCC₁={4,3}
      stack=[0,1,2]
    Return to 2: low[2] stays 1
  Return to 1: low[1]=min(2,low[2]=1)=1
Return to 0: low[0]=min(1,low[1]=1)=1
low[0]==disc[0]==1 → ROOT! Pop until 0: SCC₂={2,1,0}
```

SCCs en orden de descubrimiento (Tarjan): $\{5\}, \{3,4\}, \{0,1,2\}$
(inverso topológico).
</details>

---

### Ejercicio 4 — Grafo de condensación

Dado el grafo del ejercicio 1 con SCCs $\{0,1,2\}, \{3,4\}, \{5\}$:

a) Dibuja el grafo de condensación.
b) ¿Cuáles son las fuentes y sumideros?
c) ¿Cuántas aristas hay que agregar para hacer el grafo original fuertemente
   conexo?

<details>
<summary>Respuesta</summary>

a) Aristas inter-SCC en $G$: $2\to3$ (SCC₀→SCC₁), $4\to5$ (SCC₁→SCC₂).
   Condensación: SCC₀ → SCC₁ → SCC₂ (cadena lineal).

b) Fuente: SCC₀ ($\text{in-degree} = 0$). Sumidero: SCC₂ ($\text{out-degree} = 0$).

c) $s = 1$ fuente, $t = 1$ sumidero.
   Aristas necesarias = $\max(s, t) = \max(1, 1) = 1$.
   Una posible arista: $5 \to 0$ (del sumidero a la fuente), cerrando
   un gran ciclo que hace todo fuertemente conexo.
</details>

---

### Ejercicio 5 — ¿DAG o no?

Para cada grafo, determina si es un DAG usando la relación SCCs = $n$:

a) $n=5$, aristas: $0\to1, 1\to2, 2\to3, 3\to4$.
b) $n=4$, aristas: $0\to1, 1\to2, 2\to3, 3\to1$.
c) $n=6$, aristas: $0\to1, 0\to2, 1\to3, 2\to3, 3\to4, 3\to5$.

<details>
<summary>Respuesta</summary>

a) Cadena lineal: cada vértice es su propio SCC (5 SCCs = 5 vértices).
   **Sí, es DAG**.

b) Ciclo $1\to2\to3\to1$: SCC $\{1,2,3\}$ + singleton $\{0\}$ = 2 SCCs < 4.
   **No es DAG**.

c) Sin ciclos (verificar: no hay camino de vuelta desde 4 o 5 a 0,1,2,3):
   6 SCCs = 6 vértices. **Sí, es DAG**.
</details>

---

### Ejercicio 6 — Kosaraju sin grafo transpuesto

El paso de construir $G^T$ usa $O(n + m)$ espacio extra. Propón cómo
implementar la segunda pasada de Kosaraju **sin construir explícitamente
$G^T$**, si tienes acceso a la lista de adyacencia original.

<details>
<summary>Pista y solución</summary>

**Idea**: almacenar las aristas como pares $(u, v)$ en una **lista de
aristas** además de la lista de adyacencia. Para la segunda pasada,
iterar las aristas y agrupar por destino:

1. Crear un array `head[n]` inicializado a -1 y un array `next[m]`.
2. Para cada arista $(u, v)$ con índice $i$: `next[i] = head[v]`,
   `head[v] = i`. Esto construye la lista de adyacencia transpuesta
   de forma implícita (chain forward star para $G^T$).

Alternativamente, si la memoria es la preocupación, considerar Tarjan
en su lugar (no necesita $G^T$ en absoluto).
</details>

---

### Ejercicio 7 — 2-SAT a mano

Fórmula 2-SAT con variables $x_0, x_1, x_2$:
$(x_0 \lor x_1) \land (\lnot x_0 \lor x_2) \land (\lnot x_1 \lor \lnot x_2) \land (x_0 \lor \lnot x_2)$.

a) Construye el grafo de implicaciones (literales: $x_i = 2i$,
$\lnot x_i = 2i+1$).
b) Encuentra los SCCs.
c) ¿Es satisfacible? Si sí, da una asignación.

<details>
<summary>Respuesta</summary>

a) Cláusulas e implicaciones:
- $(x_0 \lor x_1)$: $\lnot x_0 \to x_1$ y $\lnot x_1 \to x_0$
  → aristas: $1\to2, 3\to0$.
- $(\lnot x_0 \lor x_2)$: $x_0 \to x_2$ y $\lnot x_2 \to \lnot x_0$
  → aristas: $0\to4, 5\to1$.
- $(\lnot x_1 \lor \lnot x_2)$: $x_1 \to \lnot x_2$ y $x_2 \to \lnot x_1$
  → aristas: $2\to5, 4\to3$.
- $(x_0 \lor \lnot x_2)$: $\lnot x_0 \to \lnot x_2$ y $x_2 \to x_0$
  → aristas: $1\to5, 4\to0$.

b) Grafo (6 nodos): aristas $\{1\to2, 3\to0, 0\to4, 5\to1, 2\to5, 4\to3, 1\to5, 4\to0\}$.
Siguiendo las cadenas: $0\to4\to3\to0$ (ciclo) y $0\to4\to0$ (ciclo confirmado).
SCC: $\{0, 3, 4\}$ es decir $\{x_0, \lnot x_1, x_2\}$.
También: $1\to2\to5\to1$ (ciclo). SCC: $\{1, 2, 5\}$ = $\{\lnot x_0, x_1, \lnot x_2\}$.

c) Verificar: $x_0$ (nodo 0) y $\lnot x_0$ (nodo 1) están en **diferentes**
SCCs → OK. Igual para $x_1/\lnot x_1$ y $x_2/\lnot x_2$. **Satisfacible**.

Asignación: el SCC que contiene el literal positivo $x_0$ tiene ID distinto
del que contiene $\lnot x_0$. Por orden topológico: $x_0 = \text{true},
x_1 = \text{false}, x_2 = \text{true}$.

Verificación: $(T \lor F) \land (F \lor T) \land (T \lor F) \land (T \lor F) = T$.
</details>

---

### Ejercicio 8 — Implementar condensación

Escribe una función que, dado un grafo dirigido:
1. Calcule los SCCs (con Kosaraju o Tarjan).
2. Construya el grafo de condensación (DAG).
3. Ejecute un ordenamiento topológico sobre el DAG de condensación.
4. Imprima el resultado.

Prueba con: $0\to1, 1\to2, 2\to0, 1\to3, 3\to4, 4\to5, 5\to3, 5\to6$.

<details>
<summary>Resultado esperado</summary>

SCCs: $\{0,1,2\}$ (SCC A), $\{3,4,5\}$ (SCC B), $\{6\}$ (SCC C).

Condensación: A → B → C.

Orden topológico: A, B, C (o con los IDs numéricos correspondientes).

Verificar que no hay aristas "hacia atrás" en la condensación (es un DAG).
</details>

---

### Ejercicio 9 — Tarjan iterativo

La versión recursiva de Tarjan puede causar stack overflow en grafos con
cadenas largas (e.g., $0\to1\to2\to\cdots\to10000\to0$).

Implementa Tarjan **iterativo** usando una pila explícita que simule la
recursión. Los desafíos principales son:
- Guardar el estado `(u, índice_del_vecino)` en la pila.
- Actualizar `low[parent]` al "retornar" de un hijo.

Verifica que produce los mismos SCCs que la versión recursiva en al menos
3 grafos diferentes.

<details>
<summary>Pista de estructura</summary>

```
// Call stack entry: (vertex, neighbor_index)
// When idx == adj[u].len(), "return" from u:
//   1. Check if low[u] == disc[u] → pop SCC
//   2. Update parent's low: low[parent] = min(low[parent], low[u])
//   3. Pop call stack
// When idx < adj[u].len(), process next neighbor:
//   If unvisited: push (v, 0) onto call stack
//   If on_stack: update low[u]
//   Increment idx
```

Ver Demo 8 del programa Rust para la implementación completa.
</details>

---

### Ejercicio 10 — Aplicación: redes sociales

Una red social tiene 10 usuarios (0-9) donde "seguir" es dirigido:

```
0→1, 1→2, 2→0,           (grupo A se siguen mutuamente)
3→4, 4→5, 5→3,           (grupo B se siguen mutuamente)
2→3,                      (A sigue a alguien de B)
5→6, 6→7, 7→8, 8→9       (cadena de seguidores)
```

a) Identifica los SCCs (¿qué representan socialmente?).
b) Si la plataforma quiere recomendar "personas que podrías conocer" basándose
   en SCCs, ¿qué recomendaciones haría para el usuario 0?
c) ¿Cuántos "follows" mínimos hay que agregar para que todos se sigan
   mutuamente (fuertemente conexo)?

<details>
<summary>Respuesta</summary>

a) SCCs:
- $\{0,1,2\}$: grupo cerrado que se sigue mutuamente (comunidad A).
- $\{3,4,5\}$: grupo cerrado (comunidad B).
- $\{6\}, \{7\}, \{8\}, \{9\}$: singletons (usuarios pasivos que no forman ciclos).

Socialmente, un SCC representa un **grupo de usuarios mutuamente conectados**
— información compartida por uno llegará a todos los demás del grupo.

b) Para usuario 0 (en SCC $\{0,1,2\}$): ya conoce a 1 y 2.
A través de la condensación $A \to B \to \{6\} \to \{7\} \to \{8\} \to \{9\}$,
podría recomendar los miembros del SCC más cercano: **3, 4, 5** (comunidad B).

c) DAG de condensación: $\text{SCC}_A \to \text{SCC}_B \to 6 \to 7 \to 8 \to 9$.
Fuentes: 1 ($\text{SCC}_A$). Sumideros: 1 ($\{9\}$).
Aristas necesarias = $\max(1, 1) = 1$... pero hay 6 SCCs.

Corrección: con 6 SCCs en cadena, necesitamos $\max(s, t) = \max(1, 1) = 1$
arista (de 9 → 0), cerrando un gran ciclo. Pero eso solo hace que exista
**un camino** entre todos, no necesariamente que formen un solo SCC con las
aristas originales.

Revisando: con la arista $9 \to 0$, el gran ciclo sería
$0\to1\to2\to3\to4\to5\to6\to7\to8\to9\to0$ (usando las aristas internas
de cada SCC para conectar). Sí, **1 arista** ($9 \to 0$) basta para hacer
todo el grafo fuertemente conexo.
</details>
