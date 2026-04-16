# T04 — Ordenamiento topológico

## Objetivo

Dominar el ordenamiento topológico como **herramienta constructiva** sobre DAGs:
programación dinámica (camino más largo, más corto, conteo de caminos), método del
camino crítico (CPM), niveles topológicos para ejecución paralela, y enumeración
de todos los órdenes válidos. Los algoritmos base (DFS post-order y Kahn) se
cubrieron en S02/T03; aquí los usamos como *building block* para resolver
problemas más ricos.

---

## 1. DAG como estructura computacional

### 1.1 Propiedades fundamentales

Un **DAG** (Directed Acyclic Graph) $G = (V, E)$ cumple:

- Existe al menos un vértice con in-degree 0 (**fuente**) y al menos uno con
  out-degree 0 (**sumidero**). Si no existiera fuente, podríamos retroceder
  indefinidamente por predecesores, contradiciendo la finitud y aciclicidad.
- Todo DAG admite al menos un ordenamiento topológico. El número de órdenes
  válidos depende de la estructura y contarlos es $\#\text{P}$-completo.
- $|E| \leq \binom{|V|}{2}$: máximo $n(n-1)/2$ aristas (DAG completo
  transitivo).
- Todo subgrafo inducido de un DAG es un DAG.

### 1.2 DAG y relaciones de orden parcial

Un DAG define un **orden parcial** sobre sus vértices: $u \preceq v$ si existe
un camino de $u$ a $v$. El ordenamiento topológico es una **extensión lineal** de
este orden parcial — extiende $\preceq$ a un orden total compatible.

El **teorema de Dilworth** establece que en un orden parcial, el tamaño de la
**anticadena** máxima (conjunto de elementos mutuamente incomparables) es igual al
número mínimo de cadenas que cubren todos los elementos. En términos de DAGs: la
longitud del camino más largo + 1 es una cota inferior para el número de niveles
topológicos.

### 1.3 Grafo de ejemplo

Usaremos este DAG de 7 vértices a lo largo de todo el tópico:

```
         4         5
    0 -------> 1 -------> 3
    |          |          |
   3|         2|         4|
    v          v          v
    2 -------> 4 -------> 5
         6     |     3    |
               |         2|
              7|          v
               +--------> 6
```

**Vértices**: 0, 1, 2, 3, 4, 5, 6

**Aristas** (con pesos para demos de DP):

| Arista | Peso |
|--------|:----:|
| 0 → 1  |  4   |
| 0 → 2  |  3   |
| 1 → 3  |  5   |
| 1 → 4  |  2   |
| 2 → 4  |  6   |
| 3 → 5  |  4   |
| 4 → 5  |  3   |
| 4 → 6  |  7   |
| 5 → 6  |  2   |

**In-degrees**: 0:0, 1:1, 2:1, 3:1, 4:2, 5:2, 6:2

**Orden topológico** (uno de varios): 0, 1, 2, 3, 4, 5, 6

---

## 2. Recap: algoritmos base

En S02/T03 se cubrieron los dos algoritmos canónicos. Aquí resumimos sus
interfaces para usarlos como subrutinas.

### 2.1 DFS post-order inverso

Ejecutar DFS completo; al finalizar cada vértice (color BLACK), apilarlo. El
reverso de la pila es un orden topológico. Detecta ciclos si encuentra una back
edge (vértice GRAY). Complejidad $O(n + m)$.

### 2.2 Kahn (BFS con in-degree)

Iniciar cola con vértices de in-degree 0. Extraer, agregar al resultado, decrementar
in-degree de vecinos. Si alguno llega a 0, encolarlo. Si al final
$|\text{result}| < n$, hay ciclo. Complejidad $O(n + m)$.

### 2.3 Cuándo usar cada uno

| Necesidad | Algoritmo preferido |
|-----------|:-------------------:|
| Detección de ciclos simultánea | Ambos |
| Lexicográficamente menor | Kahn con min-heap |
| Niveles topológicos | Kahn (procesar por rondas) |
| Integración con DFS existente | DFS post-order |
| Programación dinámica sobre DAG | Cualquiera (solo necesitamos el orden) |

---

## 3. Programación dinámica en DAGs

El poder real del ordenamiento topológico es habilitar **DP eficiente**. Si
procesamos los vértices en orden topológico, al llegar a $v$ todos sus
predecesores ya fueron procesados. Esto permite resolver en $O(n + m)$ problemas
que serían NP-hard en grafos generales.

### 3.1 Camino más largo (Longest Path)

En grafos generales, el camino más largo es NP-hard. En DAGs, se resuelve en
$O(n + m)$ con DP:

$$\text{longest}[v] = \max_{(u,v) \in E}\bigl(\text{longest}[u] + w(u,v)\bigr)$$

Base: $\text{longest}[\text{fuente}] = 0$.

**Traza con nuestro DAG** (camino más largo desde vértice 0):

| Vértice (topo-order) | Cálculo | longest[] | Predecesor |
|:---------------------:|---------|:---------:|:----------:|
| 0 | fuente | 0 | — |
| 1 | longest[0]+4 = 4 | 4 | 0 |
| 2 | longest[0]+3 = 3 | 3 | 0 |
| 3 | longest[1]+5 = 9 | 9 | 1 |
| 4 | max(longest[1]+2, longest[2]+6) = max(6, 9) | 9 | 2 |
| 5 | max(longest[3]+4, longest[4]+3) = max(13, 12) | 13 | 3 |
| 6 | max(longest[4]+7, longest[5]+2) = max(16, 15) | 16 | 4 |

**Camino más largo**: 0 → 2 → 4 → 6, peso = 3 + 6 + 7 = **16**.

### 3.2 Camino más corto (Shortest Path)

Análogo, con $\min$ en lugar de $\max$:

$$\text{shortest}[v] = \min_{(u,v) \in E}\bigl(\text{shortest}[u] + w(u,v)\bigr)$$

Base: $\text{shortest}[\text{fuente}] = 0$, resto $+\infty$.

**Traza**:

| Vértice | Cálculo | shortest[] | Predecesor |
|:-------:|---------|:----------:|:----------:|
| 0 | fuente | 0 | — |
| 1 | 0+4 = 4 | 4 | 0 |
| 2 | 0+3 = 3 | 3 | 0 |
| 3 | 4+5 = 9 | 9 | 1 |
| 4 | min(4+2, 3+6) = min(6, 9) | 6 | 1 |
| 5 | min(9+4, 6+3) = min(13, 9) | 9 | 4 |
| 6 | min(6+7, 9+2) = min(13, 11) | 11 | 5 |

**Camino más corto** a 6: 0 → 1 → 4 → 5 → 6, peso = 4 + 2 + 3 + 2 = **11**.

Notar que funciona con **pesos negativos** (a diferencia de Dijkstra), porque el
orden topológico garantiza que no revisitamos vértices.

### 3.3 Conteo de caminos

Contar caminos distintos entre $s$ y $t$:

$$\text{count}[v] = \sum_{(u,v) \in E} \text{count}[u]$$

Base: $\text{count}[s] = 1$.

**Traza** (caminos de 0 a 6):

| Vértice | Cálculo | count[] |
|:-------:|---------|:-------:|
| 0 | base | 1 |
| 1 | count[0] = 1 | 1 |
| 2 | count[0] = 1 | 1 |
| 3 | count[1] = 1 | 1 |
| 4 | count[1] + count[2] = 1+1 | 2 |
| 5 | count[3] + count[4] = 1+2 | 3 |
| 6 | count[4] + count[5] = 2+3 | 5 |

**5 caminos** de 0 a 6: {0→1→3→5→6, 0→1→4→5→6, 0→1→4→6, 0→2→4→5→6, 0→2→4→6}.

---

## 4. Método del camino crítico (CPM)

### 4.1 Modelo

Cada vértice es una **tarea** con duración $d[v]$. Las aristas representan
**dependencias**: $(u, v)$ significa que $u$ debe completarse antes de que $v$
pueda iniciar. Se busca la **duración mínima del proyecto** (makespan) y las
tareas **críticas** (cuyo retraso extiende el proyecto).

### 4.2 Pase hacia adelante (Forward Pass)

Procesar en orden topológico:

$$\text{ES}[v] = \max_{(u,v) \in E}\bigl(\text{ES}[u] + d[u]\bigr)$$

$$\text{EF}[v] = \text{ES}[v] + d[v]$$

Base: $\text{ES}[\text{fuente}] = 0$.

El **makespan** es $\max_v(\text{EF}[v])$.

### 4.3 Pase hacia atrás (Backward Pass)

Procesar en orden topológico **inverso**:

$$\text{LF}[v] = \min_{(v,w) \in E}\bigl(\text{LS}[w]\bigr)$$

$$\text{LS}[v] = \text{LF}[v] - d[v]$$

Base: $\text{LF}[\text{sumidero}] = \text{EF}[\text{sumidero}] = \text{makespan}$.

### 4.4 Holgura y camino crítico

$$\text{Slack}[v] = \text{LS}[v] - \text{ES}[v] = \text{LF}[v] - \text{EF}[v]$$

Las tareas con $\text{Slack}[v] = 0$ son **críticas**. El camino formado por
tareas críticas conectadas es el **camino crítico** — la secuencia más larga de
dependencias que determina el makespan.

### 4.5 Traza CPM con nuestro DAG

Duraciones: $d = [3, 4, 2, 5, 6, 3, 2]$

**Forward pass** (orden 0, 1, 2, 3, 4, 5, 6):

| Tarea | $d[v]$ | ES | EF | Cálculo ES |
|:-----:|:------:|:--:|:--:|------------|
| 0 | 3 | 0 | 3 | fuente |
| 1 | 4 | 3 | 7 | EF[0] = 3 |
| 2 | 2 | 3 | 5 | EF[0] = 3 |
| 3 | 5 | 7 | 12 | EF[1] = 7 |
| 4 | 6 | 7 | 13 | max(EF[1], EF[2]) = max(7, 5) = 7 |
| 5 | 3 | 13 | 16 | max(EF[3], EF[4]) = max(12, 13) = 13 |
| 6 | 2 | 16 | 18 | max(EF[4], EF[5]) = max(13, 16) = 16 |

**Makespan** = 18.

**Backward pass** (orden 6, 5, 4, 3, 2, 1, 0):

| Tarea | LF | LS | Cálculo LF |
|:-----:|:--:|:--:|------------|
| 6 | 18 | 16 | sumidero |
| 5 | 16 | 13 | LS[6] = 16 |
| 4 | 13 | 7 | min(LS[5], LS[6]) = min(13, 16) = 13 |
| 3 | 13 | 8 | LS[5] = 13 |
| 2 | 7 | 5 | LS[4] = 7 |
| 1 | 7 | 3 | min(LS[3], LS[4]) = min(8, 7) = 7 |
| 0 | 3 | 0 | min(LS[1], LS[2]) = min(3, 5) = 3 |

**Holgura**:

| Tarea | ES | LS | Slack | Crítica |
|:-----:|:--:|:--:|:-----:|:-------:|
| 0 | 0 | 0 | 0 | Sí |
| 1 | 3 | 3 | 0 | Sí |
| 2 | 3 | 5 | 2 | No |
| 3 | 7 | 8 | 1 | No |
| 4 | 7 | 7 | 0 | Sí |
| 5 | 13 | 13 | 0 | Sí |
| 6 | 16 | 16 | 0 | Sí |

**Camino crítico**: 0 → 1 → 4 → 5 → 6, duración = 3 + 4 + 6 + 3 + 2 = **18**.

La tarea 2 puede retrasarse hasta 2 unidades sin afectar el proyecto. La tarea 3
puede retrasarse hasta 1 unidad.

---

## 5. Niveles topológicos

### 5.1 Definición

Los **niveles topológicos** particionan los vértices del DAG en capas
$L_0, L_1, \ldots$ donde:

- $L_0$ contiene todos los vértices con in-degree 0.
- $L_k$ contiene los vértices cuyo in-degree es 0 tras eliminar $L_0 \cup \cdots \cup L_{k-1}$.

Las tareas dentro del mismo nivel son **mutuamente independientes** y pueden
ejecutarse en paralelo.

### 5.2 Algoritmo (Kahn por rondas)

Es una variante de Kahn donde en lugar de procesar un vértice a la vez, se
procesan **todos** los vértices de in-degree 0 como una **ronda**:

```
TOPOLOGICAL-LEVELS(G):
    in_deg[] <- compute in-degrees
    current <- all v where in_deg[v] == 0
    levels <- empty list of lists

    while current not empty:
        levels.append(current)
        next <- empty
        for each u in current:
            for each v in adj[u]:
                in_deg[v] <- in_deg[v] - 1
                if in_deg[v] == 0:
                    next.append(v)
        current <- next

    return levels
```

### 5.3 Traza con nuestro DAG

| Nivel | Vértices | In-degrees restantes |
|:-----:|----------|---------------------|
| 0 | {0} | Tras eliminar 0: 1:0, 2:0 |
| 1 | {1, 2} | Tras eliminar 1,2: 3:0, 4:0 |
| 2 | {3, 4} | Tras eliminar 3,4: 5:0, 6:1 |
| 3 | {5} | Tras eliminar 5: 6:0 |
| 4 | {6} | — |

El DAG tiene **5 niveles**. El **ancho máximo** es 2 (niveles 1 y 2), indicando
que a lo sumo 2 tareas pueden ejecutarse simultáneamente.

### 5.4 Relación con camino más largo

El número de niveles es exactamente $\ell + 1$, donde $\ell$ es la longitud del
**camino más largo** (en número de aristas, sin pesos). En nuestro DAG, el camino
más largo tiene 4 aristas (0→1→4→5→6), así que hay 5 niveles. Esto refleja el
**teorema de Mirsky**: la longitud de la cadena más larga en un orden parcial
es igual al número mínimo de anticadenas que lo cubren.

---

## 6. Enumeración de todos los ordenamientos

### 6.1 Algoritmo backtracking

Para encontrar **todos** los ordenamientos topológicos válidos, se usa
backtracking sobre el algoritmo de Kahn: en cada paso, cualquier vértice con
in-degree 0 puede ser el siguiente.

```
ALL-TOPO-ORDERS(G, in_deg[], order[], pos, result):
    if pos == |V|:
        result.append(copy of order[])
        return

    for each v in V:
        if in_deg[v] == 0 and v not in order[0..pos]:
            order[pos] <- v
            // "Remove" v: decrement in-degree of neighbors
            for each w in adj[v]:
                in_deg[w] <- in_deg[w] - 1

            ALL-TOPO-ORDERS(G, in_deg, order, pos+1, result)

            // Backtrack: restore in-degrees
            for each w in adj[v]:
                in_deg[w] <- in_deg[w] + 1
```

### 6.2 Los 5 ordenamientos de nuestro DAG

1. **0, 1, 2, 3, 4, 5, 6** (lexicográficamente menor)
2. 0, 1, 2, 4, 3, 5, 6
3. 0, 1, 3, 2, 4, 5, 6
4. 0, 2, 1, 3, 4, 5, 6
5. 0, 2, 1, 4, 3, 5, 6

### 6.3 Complejidad

El número de ordenamientos topológicos puede ser exponencial: un DAG sin aristas
con $n$ vértices tiene $n!$ órdenes. El algoritmo de backtracking tiene
complejidad $O(n! \cdot n)$ en el peor caso, lo cual es inevitable si se quieren
enumerar todos.

---

## 7. Orden lexicográficamente menor

Reemplazar la cola FIFO de Kahn por un **min-heap** garantiza que en cada paso se
elige el vértice de menor índice entre los disponibles. Esto produce el
**lexicográficamente menor** ordenamiento topológico en $O((n + m) \log n)$.

Para nuestro DAG: el resultado es **0, 1, 2, 3, 4, 5, 6** (coincide con el
primer orden enumerado porque los índices ya están bien ordenados).

Para un contraejemplo donde Kahn con FIFO podría dar un orden diferente, basta
insertar vértices en la cola en orden inverso.

---

## 8. Programa completo en C

```c
/*
 * topological.c
 *
 * Topological sorting and DAG algorithms:
 * Kahn, DFS, DP (longest/shortest path, path count),
 * Critical Path Method (CPM), topological levels,
 * all orderings enumeration.
 *
 * Compile: gcc -O2 -o topological topological.c
 * Run:     ./topological
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 30
#define MAX_E 100

/* =================== WEIGHTED DAG ================================= */

typedef struct {
    int to, weight;
} Edge;

typedef struct {
    Edge edges[MAX_E];
    int head[MAX_V];   /* head[u]: first edge index for u, -1 if none */
    int next[MAX_E];   /* next edge in u's list */
    int n, m;
} DAG;

void dag_init(DAG *g, int n) {
    g->n = n;
    g->m = 0;
    memset(g->head, -1, sizeof(g->head));
}

void dag_add(DAG *g, int u, int v, int w) {
    int idx = g->m++;
    g->edges[idx].to = v;
    g->edges[idx].weight = w;
    g->next[idx] = g->head[u];
    g->head[u] = idx;
}

/* =================== KAHN'S ALGORITHM ============================= */

int kahn(const DAG *g, int order[]) {
    int in_deg[MAX_V] = {0};
    for (int u = 0; u < g->n; u++)
        for (int e = g->head[u]; e != -1; e = g->next[e])
            in_deg[g->edges[e].to]++;

    int queue[MAX_V], front = 0, back = 0;
    for (int v = 0; v < g->n; v++)
        if (in_deg[v] == 0)
            queue[back++] = v;

    int count = 0;
    while (front < back) {
        int u = queue[front++];
        order[count++] = u;
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to;
            if (--in_deg[v] == 0)
                queue[back++] = v;
        }
    }
    return count; /* < n means cycle */
}

/* =================== DFS TOPOLOGICAL SORT ========================= */

typedef enum { WHITE, GRAY, BLACK } Color;

static bool dfs_topo(const DAG *g, int u, Color color[], int stack[],
                     int *top) {
    color[u] = GRAY;
    for (int e = g->head[u]; e != -1; e = g->next[e]) {
        int v = g->edges[e].to;
        if (color[v] == GRAY) return false;
        if (color[v] == WHITE && !dfs_topo(g, v, color, stack, top))
            return false;
    }
    color[u] = BLACK;
    stack[(*top)++] = u;
    return true;
}

bool topo_dfs(const DAG *g, int order[]) {
    Color color[MAX_V];
    int stack[MAX_V], top = 0;
    for (int i = 0; i < g->n; i++) color[i] = WHITE;

    for (int u = 0; u < g->n; u++)
        if (color[u] == WHITE)
            if (!dfs_topo(g, u, color, stack, &top))
                return false;

    for (int i = 0; i < g->n; i++)
        order[i] = stack[g->n - 1 - i];
    return true;
}

/* =================== DP ON DAG ==================================== */

/* Longest path from source (single-source, edge weights) */
void longest_path(const DAG *g, int src, int dist[], int pred[]) {
    int order[MAX_V];
    kahn(g, order);

    for (int i = 0; i < g->n; i++) { dist[i] = INT_MIN; pred[i] = -1; }
    dist[src] = 0;

    for (int i = 0; i < g->n; i++) {
        int u = order[i];
        if (dist[u] == INT_MIN) continue;
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to, w = g->edges[e].weight;
            if (dist[u] + w > dist[v]) {
                dist[v] = dist[u] + w;
                pred[v] = u;
            }
        }
    }
}

/* Shortest path from source (single-source, edge weights) */
void shortest_path(const DAG *g, int src, int dist[], int pred[]) {
    int order[MAX_V];
    kahn(g, order);

    for (int i = 0; i < g->n; i++) { dist[i] = INT_MAX; pred[i] = -1; }
    dist[src] = 0;

    for (int i = 0; i < g->n; i++) {
        int u = order[i];
        if (dist[u] == INT_MAX) continue;
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to, w = g->edges[e].weight;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pred[v] = u;
            }
        }
    }
}

/* Count paths from src to every vertex */
void count_paths(const DAG *g, int src, long long cnt[]) {
    int order[MAX_V];
    kahn(g, order);

    for (int i = 0; i < g->n; i++) cnt[i] = 0;
    cnt[src] = 1;

    for (int i = 0; i < g->n; i++) {
        int u = order[i];
        if (cnt[u] == 0) continue;
        for (int e = g->head[u]; e != -1; e = g->next[e])
            cnt[g->edges[e].to] += cnt[u];
    }
}

/* =================== CPM ========================================== */

void cpm(const DAG *g, const int dur[], int es[], int ef[], int ls[],
         int lf[], int slack[]) {
    int order[MAX_V];
    int cnt = kahn(g, order);

    /* Forward pass */
    for (int i = 0; i < g->n; i++) { es[i] = 0; ef[i] = dur[i]; }
    for (int i = 0; i < cnt; i++) {
        int u = order[i];
        ef[u] = es[u] + dur[u];
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to;
            if (ef[u] > es[v])
                es[v] = ef[u];
        }
    }

    /* Find makespan */
    int makespan = 0;
    for (int i = 0; i < g->n; i++)
        if (ef[i] > makespan) makespan = ef[i];

    /* Backward pass */
    for (int i = 0; i < g->n; i++) { lf[i] = makespan; }
    for (int i = cnt - 1; i >= 0; i--) {
        int u = order[i];
        ls[u] = lf[u] - dur[u];
        for (int e = g->head[u]; e != -1; e = g->next[e]) {
            int v = g->edges[e].to;
            if (ls[v] < lf[u])
                lf[u] = ls[v];
        }
        ls[u] = lf[u] - dur[u];
    }

    for (int i = 0; i < g->n; i++)
        slack[i] = ls[i] - es[i];
}

/* =================== TOPOLOGICAL LEVELS =========================== */

int topo_levels(const DAG *g, int level[]) {
    int in_deg[MAX_V] = {0};
    for (int u = 0; u < g->n; u++)
        for (int e = g->head[u]; e != -1; e = g->next[e])
            in_deg[g->edges[e].to]++;

    int current[MAX_V], cur_sz = 0;
    for (int v = 0; v < g->n; v++)
        if (in_deg[v] == 0)
            current[cur_sz++] = v;

    int num_levels = 0;
    while (cur_sz > 0) {
        int next[MAX_V], nxt_sz = 0;
        for (int i = 0; i < cur_sz; i++) {
            level[current[i]] = num_levels;
            for (int e = g->head[current[i]]; e != -1; e = g->next[e]) {
                int v = g->edges[e].to;
                if (--in_deg[v] == 0)
                    next[nxt_sz++] = v;
            }
        }
        memcpy(current, next, nxt_sz * sizeof(int));
        cur_sz = nxt_sz;
        num_levels++;
    }
    return num_levels;
}

/* =================== ALL TOPOLOGICAL ORDERINGS ==================== */

static int all_orders[720][MAX_V]; /* up to 6! = 720 */
static int all_count;

void all_topo(const DAG *g, int in_deg[], int order[], int pos,
              bool used[]) {
    if (pos == g->n) {
        memcpy(all_orders[all_count], order, g->n * sizeof(int));
        all_count++;
        return;
    }
    for (int v = 0; v < g->n; v++) {
        if (!used[v] && in_deg[v] == 0) {
            used[v] = true;
            order[pos] = v;
            for (int e = g->head[v]; e != -1; e = g->next[e])
                in_deg[g->edges[e].to]--;

            all_topo(g, in_deg, order, pos + 1, used);

            used[v] = false;
            for (int e = g->head[v]; e != -1; e = g->next[e])
                in_deg[g->edges[e].to]++;
        }
    }
}

/* =================== PATH RECONSTRUCTION ========================== */

void print_path(int pred[], int v) {
    if (pred[v] == -1) { printf("%d", v); return; }
    print_path(pred, pred[v]);
    printf(" -> %d", v);
}

/* =================== BUILD EXAMPLE DAG ============================ */

void build_example(DAG *g) {
    dag_init(g, 7);
    dag_add(g, 0, 1, 4);
    dag_add(g, 0, 2, 3);
    dag_add(g, 1, 3, 5);
    dag_add(g, 1, 4, 2);
    dag_add(g, 2, 4, 6);
    dag_add(g, 3, 5, 4);
    dag_add(g, 4, 5, 3);
    dag_add(g, 4, 6, 7);
    dag_add(g, 5, 6, 2);
}

/* =================== DEMOS ======================================== */

void demo1_kahn_trace(void) {
    printf("=== Demo 1: Kahn's Algorithm — Step-by-Step Trace ===\n\n");

    DAG g;
    build_example(&g);

    /* Manual trace with verbose output */
    int in_deg[MAX_V] = {0};
    for (int u = 0; u < g.n; u++)
        for (int e = g.head[u]; e != -1; e = g.next[e])
            in_deg[g.edges[e].to]++;

    printf("Initial in-degrees: ");
    for (int i = 0; i < g.n; i++) printf("%d:%d ", i, in_deg[i]);
    printf("\n\n");

    int queue[MAX_V], front = 0, back = 0;
    for (int v = 0; v < g.n; v++)
        if (in_deg[v] == 0) {
            queue[back++] = v;
            printf("  Source: vertex %d (in-degree 0)\n", v);
        }

    int order[MAX_V], count = 0, step = 1;
    printf("\n");

    while (front < back) {
        int u = queue[front++];
        order[count++] = u;
        printf("Step %d: dequeue %d | ", step++, u);

        bool first = true;
        for (int e = g.head[u]; e != -1; e = g.next[e]) {
            int v = g.edges[e].to;
            in_deg[v]--;
            if (first) { printf("update: "); first = false; }
            else printf(", ");
            printf("in[%d]=%d", v, in_deg[v]);
            if (in_deg[v] == 0) {
                queue[back++] = v;
                printf("(enqueue!)");
            }
        }
        if (first) printf("(no outgoing edges)");
        printf("\n");
    }

    printf("\nTopological order: ");
    for (int i = 0; i < count; i++) printf("%d%s", order[i],
                                            i < count-1 ? ", " : "");
    printf("\nAll %d vertices processed: %s\n\n",
           count, count == g.n ? "valid DAG" : "CYCLE DETECTED");
}

void demo2_dfs_topo(void) {
    printf("=== Demo 2: DFS Topological Sort with Timestamps ===\n\n");

    DAG g;
    build_example(&g);

    /* DFS with discovery/finish times */
    Color color[MAX_V];
    int disc[MAX_V], fin[MAX_V];
    int stack[MAX_V], top = 0;
    int timer = 0;
    for (int i = 0; i < g.n; i++) color[i] = WHITE;

    /* Iterative-style trace using recursion */
    typedef struct { int v; int depth; } Frame;

    /* Simple recursive DFS with printing */
    /* We'll use the existing topo_dfs and then show timestamps */
    int order[MAX_V];
    topo_dfs(&g, order);

    /* Redo with timestamps for display */
    for (int i = 0; i < g.n; i++) color[i] = WHITE;
    timer = 0;

    /* Manual DFS trace */
    struct { int v; int edge; } stk[MAX_V];
    int sp = 0;

    for (int start = 0; start < g.n; start++) {
        if (color[start] != WHITE) continue;
        stk[sp].v = start;
        stk[sp].edge = g.head[start];
        color[start] = GRAY;
        disc[start] = ++timer;
        printf("  discover %d (time=%d)\n", start, disc[start]);
        sp++;

        while (sp > 0) {
            int u = stk[sp-1].v;
            int e = stk[sp-1].edge;
            if (e != -1) {
                stk[sp-1].edge = g.next[e];
                int v = g.edges[e].to;
                if (color[v] == WHITE) {
                    color[v] = GRAY;
                    disc[v] = ++timer;
                    printf("  discover %d (time=%d)\n", v, disc[v]);
                    stk[sp].v = v;
                    stk[sp].edge = g.head[v];
                    sp++;
                }
            } else {
                color[u] = BLACK;
                fin[u] = ++timer;
                printf("  finish   %d (time=%d)\n", u, fin[u]);
                sp--;
            }
        }
    }

    printf("\nVertex   disc   fin\n");
    for (int i = 0; i < g.n; i++)
        printf("  %d       %2d     %2d\n", i, disc[i], fin[i]);

    printf("\nTopological order (decreasing finish time): ");
    for (int i = 0; i < g.n; i++)
        printf("%d%s", order[i], i < g.n-1 ? ", " : "");
    printf("\n\n");
}

void demo3_longest_path(void) {
    printf("=== Demo 3: Longest Path in Weighted DAG ===\n\n");

    DAG g;
    build_example(&g);

    int dist[MAX_V], pred[MAX_V];
    longest_path(&g, 0, dist, pred);

    printf("Longest paths from vertex 0:\n");
    printf("Vertex   Distance   Path\n");
    for (int v = 0; v < g.n; v++) {
        if (dist[v] == INT_MIN) {
            printf("  %d       unreachable\n", v);
        } else {
            printf("  %d       %2d         ", v, dist[v]);
            print_path(pred, v);
            printf("\n");
        }
    }

    printf("\nLongest path overall: vertex 0 to vertex 6, distance = %d\n",
           dist[6]);
    printf("Path: ");
    print_path(pred, 6);
    printf("\n\n");
}

void demo4_cpm(void) {
    printf("=== Demo 4: Critical Path Method (CPM) ===\n\n");

    DAG g;
    /* CPM uses unweighted edges (dependencies only) */
    dag_init(&g, 7);
    dag_add(&g, 0, 1, 0);
    dag_add(&g, 0, 2, 0);
    dag_add(&g, 1, 3, 0);
    dag_add(&g, 1, 4, 0);
    dag_add(&g, 2, 4, 0);
    dag_add(&g, 3, 5, 0);
    dag_add(&g, 4, 5, 0);
    dag_add(&g, 4, 6, 0);
    dag_add(&g, 5, 6, 0);

    int dur[] = {3, 4, 2, 5, 6, 3, 2};
    int es[MAX_V], ef[MAX_V], ls[MAX_V], lf[MAX_V], slack[MAX_V];

    const char *names[] = {"Design", "Backend", "Frontend", "API",
                           "Database", "Testing", "Deploy"};

    printf("Project tasks:\n");
    for (int i = 0; i < g.n; i++)
        printf("  Task %d (%-8s): duration = %d\n", i, names[i], dur[i]);
    printf("\nDependencies: 0->1, 0->2, 1->3, 1->4, 2->4, 3->5, 4->5, "
           "4->6, 5->6\n\n");

    cpm(&g, dur, es, ef, ls, lf, slack);

    printf("Task      Dur   ES   EF   LS   LF   Slack  Critical\n");
    printf("--------- ---  ---  ---  ---  ---  -----  --------\n");
    for (int i = 0; i < g.n; i++)
        printf("%-9s  %d    %2d   %2d   %2d   %2d    %d      %s\n",
               names[i], dur[i], es[i], ef[i], ls[i], lf[i], slack[i],
               slack[i] == 0 ? "YES" : "no");

    int makespan = 0;
    for (int i = 0; i < g.n; i++)
        if (ef[i] > makespan) makespan = ef[i];
    printf("\nProject makespan: %d time units\n", makespan);
    printf("Critical path: ");
    bool first = true;
    for (int i = 0; i < g.n; i++) {
        /* Print critical tasks in topological order */
        if (slack[i] == 0) {
            if (!first) printf(" -> ");
            printf("%s(%d)", names[i], i);
            first = false;
        }
    }
    printf("\n\n");
}

void demo5_levels(void) {
    printf("=== Demo 5: Topological Levels (Parallel Execution) ===\n\n");

    DAG g;
    build_example(&g);

    int level[MAX_V];
    int num_levels = topo_levels(&g, level);

    printf("Number of levels: %d\n\n", num_levels);
    for (int l = 0; l < num_levels; l++) {
        printf("Level %d: { ", l);
        bool first = true;
        for (int v = 0; v < g.n; v++) {
            if (level[v] == l) {
                if (!first) printf(", ");
                printf("%d", v);
                first = false;
            }
        }
        printf(" }\n");
    }

    printf("\nParallel schedule (each level is a time step):\n");
    for (int l = 0; l < num_levels; l++) {
        printf("  t=%d: process ", l);
        bool first = true;
        for (int v = 0; v < g.n; v++) {
            if (level[v] == l) {
                if (!first) printf(" and ");
                printf("%d", v);
                first = false;
            }
        }
        printf("\n");
    }

    int max_width = 0;
    for (int l = 0; l < num_levels; l++) {
        int w = 0;
        for (int v = 0; v < g.n; v++)
            if (level[v] == l) w++;
        if (w > max_width) max_width = w;
    }
    printf("\nMaximum parallelism: %d tasks simultaneously\n", max_width);
    printf("Minimum time steps (ignoring task duration): %d\n\n",
           num_levels);
}

void demo6_all_orderings(void) {
    printf("=== Demo 6: All Topological Orderings (Backtracking) ===\n\n");

    DAG g;
    build_example(&g);

    int in_deg[MAX_V] = {0};
    for (int u = 0; u < g.n; u++)
        for (int e = g.head[u]; e != -1; e = g.next[e])
            in_deg[g.edges[e].to]++;

    int order[MAX_V];
    bool used[MAX_V] = {false};
    all_count = 0;

    all_topo(&g, in_deg, order, 0, used);

    printf("Found %d valid topological orderings:\n", all_count);
    for (int i = 0; i < all_count; i++) {
        printf("  %d: [", i + 1);
        for (int j = 0; j < g.n; j++)
            printf("%d%s", all_orders[i][j], j < g.n-1 ? ", " : "");
        printf("]\n");
    }

    printf("\nLexicographically smallest: [");
    for (int j = 0; j < g.n; j++)
        printf("%d%s", all_orders[0][j], j < g.n-1 ? ", " : "");
    printf("]\n\n");
}

/* =================== MAIN ========================================= */

int main(void) {
    demo1_kahn_trace();
    demo2_dfs_topo();
    demo3_longest_path();
    demo4_cpm();
    demo5_levels();
    demo6_all_orderings();
    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * topological.rs
 *
 * Topological sorting and DAG algorithms in Rust:
 * Kahn, DFS, DP (longest/shortest path, path count),
 * CPM, topological levels, lexicographically smallest order,
 * all orderings enumeration.
 *
 * Compile: rustc -o topological topological.rs
 * Run:     ./topological
 */

use std::collections::BinaryHeap;
use std::cmp::Reverse;

/* =================== WEIGHTED DAG ================================= */

struct DAG {
    adj: Vec<Vec<(usize, i64)>>, // (destination, weight)
    n: usize,
}

impl DAG {
    fn new(n: usize) -> Self {
        DAG { adj: vec![vec![]; n], n }
    }

    fn add_edge(&mut self, u: usize, v: usize, w: i64) {
        self.adj[u].push((v, w));
    }

    fn in_degrees(&self) -> Vec<usize> {
        let mut deg = vec![0usize; self.n];
        for u in 0..self.n {
            for &(v, _) in &self.adj[u] {
                deg[v] += 1;
            }
        }
        deg
    }

    /* Kahn's algorithm */
    fn kahn(&self) -> Option<Vec<usize>> {
        let mut in_deg = self.in_degrees();
        let mut queue: Vec<usize> = (0..self.n)
            .filter(|&v| in_deg[v] == 0)
            .collect();
        let mut order = Vec::with_capacity(self.n);
        let mut front = 0;

        while front < queue.len() {
            let u = queue[front];
            front += 1;
            order.push(u);
            for &(v, _) in &self.adj[u] {
                in_deg[v] -= 1;
                if in_deg[v] == 0 {
                    queue.push(v);
                }
            }
        }

        if order.len() == self.n { Some(order) } else { None }
    }

    /* DFS topological sort */
    fn topo_dfs(&self) -> Option<Vec<usize>> {
        #[derive(Clone, Copy, PartialEq)]
        enum Color { White, Gray, Black }

        let mut color = vec![Color::White; self.n];
        let mut stack = Vec::with_capacity(self.n);

        fn dfs(g: &DAG, u: usize, color: &mut [Color],
               stack: &mut Vec<usize>) -> bool {
            color[u] = Color::Gray;
            for &(v, _) in &g.adj[u] {
                match color[v] {
                    Color::Gray => return false,
                    Color::White => {
                        if !dfs(g, v, color, stack) { return false; }
                    }
                    Color::Black => {}
                }
            }
            color[u] = Color::Black;
            stack.push(u);
            true
        }

        for u in 0..self.n {
            if color[u] == Color::White {
                if !dfs(self, u, &mut color, &mut stack) {
                    return None;
                }
            }
        }
        stack.reverse();
        Some(stack)
    }

    /* Kahn with min-heap for lexicographically smallest order */
    fn kahn_lex_smallest(&self) -> Option<Vec<usize>> {
        let mut in_deg = self.in_degrees();
        let mut heap = BinaryHeap::new();
        for v in 0..self.n {
            if in_deg[v] == 0 {
                heap.push(Reverse(v));
            }
        }

        let mut order = Vec::with_capacity(self.n);
        while let Some(Reverse(u)) = heap.pop() {
            order.push(u);
            for &(v, _) in &self.adj[u] {
                in_deg[v] -= 1;
                if in_deg[v] == 0 {
                    heap.push(Reverse(v));
                }
            }
        }

        if order.len() == self.n { Some(order) } else { None }
    }
}

/* =================== DP ON DAG ==================================== */

fn longest_path(g: &DAG, src: usize) -> (Vec<i64>, Vec<Option<usize>>) {
    let order = g.kahn().expect("DAG required");
    let mut dist = vec![i64::MIN; g.n];
    let mut pred = vec![None; g.n];
    dist[src] = 0;

    for &u in &order {
        if dist[u] == i64::MIN { continue; }
        for &(v, w) in &g.adj[u] {
            if dist[u] + w > dist[v] {
                dist[v] = dist[u] + w;
                pred[v] = Some(u);
            }
        }
    }
    (dist, pred)
}

fn shortest_path(g: &DAG, src: usize) -> (Vec<i64>, Vec<Option<usize>>) {
    let order = g.kahn().expect("DAG required");
    let mut dist = vec![i64::MAX; g.n];
    let mut pred = vec![None; g.n];
    dist[src] = 0;

    for &u in &order {
        if dist[u] == i64::MAX { continue; }
        for &(v, w) in &g.adj[u] {
            if dist[u] + w < dist[v] {
                dist[v] = dist[u] + w;
                pred[v] = Some(u);
            }
        }
    }
    (dist, pred)
}

fn count_paths(g: &DAG, src: usize) -> Vec<u64> {
    let order = g.kahn().expect("DAG required");
    let mut cnt = vec![0u64; g.n];
    cnt[src] = 1;

    for &u in &order {
        if cnt[u] == 0 { continue; }
        for &(v, _) in &g.adj[u] {
            cnt[v] += cnt[u];
        }
    }
    cnt
}

/* =================== CPM ========================================== */

struct CpmResult {
    es: Vec<i64>,
    ef: Vec<i64>,
    ls: Vec<i64>,
    lf: Vec<i64>,
    slack: Vec<i64>,
    makespan: i64,
}

fn cpm(g: &DAG, dur: &[i64]) -> CpmResult {
    let order = g.kahn().expect("DAG required");
    let n = g.n;
    let mut es = vec![0i64; n];
    let mut ef = vec![0i64; n];

    /* Forward pass */
    for &u in &order {
        ef[u] = es[u] + dur[u];
        for &(v, _) in &g.adj[u] {
            if ef[u] > es[v] {
                es[v] = ef[u];
            }
        }
    }

    let makespan = *ef.iter().max().unwrap();

    /* Backward pass */
    let mut lf = vec![makespan; n];
    let mut ls = vec![0i64; n];
    for &u in order.iter().rev() {
        ls[u] = lf[u] - dur[u];
        /* Update predecessors: for each edge u->v, lf[u] = min(lf[u], ls[v]) */
        /* But we iterate in reverse, so we need to look at successors */
    }

    /* Correct backward pass: iterate reverse, for each u update lf[u]
       based on ls of successors, then compute ls[u] */
    lf = vec![makespan; n];
    for &u in order.iter().rev() {
        for &(v, _) in &g.adj[u] {
            if ls[v] < lf[u] {
                lf[u] = ls[v];
            }
        }
        ls[u] = lf[u] - dur[u];
    }

    /* Redo correctly: first compute lf for sinks, then backwards */
    lf = vec![makespan; n];
    ls = vec![0i64; n];
    for &u in order.iter().rev() {
        /* lf[u] = min over successors of ls[successor] */
        let mut min_ls = makespan;
        for &(v, _) in &g.adj[u] {
            if ls[v] < min_ls {
                min_ls = ls[v];
            }
        }
        lf[u] = min_ls;
        ls[u] = lf[u] - dur[u];
    }

    let slack: Vec<i64> = (0..n).map(|i| ls[i] - es[i]).collect();

    CpmResult { es, ef, ls, lf, slack, makespan }
}

/* =================== TOPOLOGICAL LEVELS =========================== */

fn topo_levels(g: &DAG) -> Vec<Vec<usize>> {
    let mut in_deg = g.in_degrees();
    let mut current: Vec<usize> = (0..g.n)
        .filter(|&v| in_deg[v] == 0)
        .collect();
    let mut levels = Vec::new();

    while !current.is_empty() {
        let mut next = Vec::new();
        for &u in &current {
            for &(v, _) in &g.adj[u] {
                in_deg[v] -= 1;
                if in_deg[v] == 0 {
                    next.push(v);
                }
            }
        }
        levels.push(current);
        current = next;
    }
    levels
}

/* =================== ALL TOPOLOGICAL ORDERINGS ==================== */

fn all_topo_orders(g: &DAG) -> Vec<Vec<usize>> {
    let mut in_deg = g.in_degrees();
    let mut order = Vec::with_capacity(g.n);
    let mut used = vec![false; g.n];
    let mut result = Vec::new();

    fn backtrack(g: &DAG, in_deg: &mut Vec<usize>, order: &mut Vec<usize>,
                 used: &mut Vec<bool>, result: &mut Vec<Vec<usize>>) {
        if order.len() == g.n {
            result.push(order.clone());
            return;
        }
        for v in 0..g.n {
            if !used[v] && in_deg[v] == 0 {
                used[v] = true;
                order.push(v);
                for &(w, _) in &g.adj[v] {
                    in_deg[w] -= 1;
                }

                backtrack(g, in_deg, order, used, result);

                order.pop();
                used[v] = false;
                for &(w, _) in &g.adj[v] {
                    in_deg[w] += 1;
                }
            }
        }
    }

    backtrack(g, &mut in_deg, &mut order, &mut used, &mut result);
    result
}

/* =================== HELPER ======================================= */

fn reconstruct_path(pred: &[Option<usize>], v: usize) -> Vec<usize> {
    let mut path = vec![v];
    let mut cur = v;
    while let Some(p) = pred[cur] {
        path.push(p);
        cur = p;
    }
    path.reverse();
    path
}

fn build_example() -> DAG {
    let mut g = DAG::new(7);
    g.add_edge(0, 1, 4);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 3, 5);
    g.add_edge(1, 4, 2);
    g.add_edge(2, 4, 6);
    g.add_edge(3, 5, 4);
    g.add_edge(4, 5, 3);
    g.add_edge(4, 6, 7);
    g.add_edge(5, 6, 2);
    g
}

/* =================== DEMOS ======================================== */

fn demo1_kahn_trace() {
    println!("=== Demo 1: Kahn's Algorithm — Step-by-Step Trace ===\n");

    let g = build_example();
    let mut in_deg = g.in_degrees();

    print!("Initial in-degrees: ");
    for (i, &d) in in_deg.iter().enumerate() {
        print!("{}:{} ", i, d);
    }
    println!("\n");

    let mut queue = std::collections::VecDeque::new();
    for v in 0..g.n {
        if in_deg[v] == 0 {
            queue.push_back(v);
            println!("  Source: vertex {} (in-degree 0)", v);
        }
    }
    println!();

    let mut order = Vec::new();
    let mut step = 1;

    while let Some(u) = queue.pop_front() {
        order.push(u);
        print!("Step {}: dequeue {} | ", step, u);
        step += 1;

        if g.adj[u].is_empty() {
            print!("(no outgoing edges)");
        } else {
            print!("update: ");
            for (i, &(v, _)) in g.adj[u].iter().enumerate() {
                in_deg[v] -= 1;
                if i > 0 { print!(", "); }
                print!("in[{}]={}", v, in_deg[v]);
                if in_deg[v] == 0 {
                    queue.push_back(v);
                    print!("(enqueue!)");
                }
            }
        }
        println!();
    }

    print!("\nTopological order: ");
    let strs: Vec<String> = order.iter().map(|v| v.to_string()).collect();
    println!("{}", strs.join(", "));
    println!("All {} vertices processed: {}\n",
             order.len(),
             if order.len() == g.n { "valid DAG" } else { "CYCLE DETECTED" });
}

fn demo2_dfs_topo() {
    println!("=== Demo 2: DFS Topological Sort ===\n");

    let g = build_example();
    let order = g.topo_dfs().expect("DAG");

    let strs: Vec<String> = order.iter().map(|v| v.to_string()).collect();
    println!("DFS topological order: {}", strs.join(", "));

    /* Verify: for each edge u->v, u appears before v */
    let mut pos = vec![0usize; g.n];
    for (i, &v) in order.iter().enumerate() {
        pos[v] = i;
    }
    let mut valid = true;
    for u in 0..g.n {
        for &(v, _) in &g.adj[u] {
            if pos[u] >= pos[v] {
                println!("  INVALID: edge {}->{} but {} appears after {}", u, v, u, v);
                valid = false;
            }
        }
    }
    println!("Verification: {}\n", if valid { "all edges respect order" }
             else { "FAILED" });
}

fn demo3_longest_shortest() {
    println!("=== Demo 3: Longest & Shortest Paths in Weighted DAG ===\n");

    let g = build_example();

    let (ldist, lpred) = longest_path(&g, 0);
    println!("Longest paths from vertex 0:");
    println!("Vertex  Distance  Path");
    for v in 0..g.n {
        if ldist[v] == i64::MIN {
            println!("  {}     unreachable", v);
        } else {
            let path = reconstruct_path(&lpred, v);
            let ps: Vec<String> = path.iter().map(|x| x.to_string()).collect();
            println!("  {}     {:>3}       {}", v, ldist[v], ps.join(" -> "));
        }
    }

    let (sdist, spred) = shortest_path(&g, 0);
    println!("\nShortest paths from vertex 0:");
    println!("Vertex  Distance  Path");
    for v in 0..g.n {
        if sdist[v] == i64::MAX {
            println!("  {}     unreachable", v);
        } else {
            let path = reconstruct_path(&spred, v);
            let ps: Vec<String> = path.iter().map(|x| x.to_string()).collect();
            println!("  {}     {:>3}       {}", v, sdist[v], ps.join(" -> "));
        }
    }
    println!();
}

fn demo4_count_paths() {
    println!("=== Demo 4: Count All Paths ===\n");

    let g = build_example();
    let cnt = count_paths(&g, 0);

    println!("Number of distinct paths from vertex 0:");
    for v in 0..g.n {
        println!("  to {}: {} path(s)", v, cnt[v]);
    }

    /* Verify by listing paths to vertex 6 */
    println!("\nAll {} paths from 0 to 6:", cnt[6]);
    fn find_paths(g: &DAG, u: usize, target: usize, path: &mut Vec<usize>,
                  all: &mut Vec<Vec<usize>>) {
        path.push(u);
        if u == target {
            all.push(path.clone());
        } else {
            for &(v, _) in &g.adj[u] {
                find_paths(g, v, target, path, all);
            }
        }
        path.pop();
    }
    let mut all_paths = Vec::new();
    let mut path = Vec::new();
    find_paths(&g, 0, 6, &mut path, &mut all_paths);
    for (i, p) in all_paths.iter().enumerate() {
        let ps: Vec<String> = p.iter().map(|x| x.to_string()).collect();
        println!("  {}: {}", i + 1, ps.join(" -> "));
    }
    println!();
}

fn demo5_cpm() {
    println!("=== Demo 5: Critical Path Method (CPM) ===\n");

    let mut g = DAG::new(7);
    g.add_edge(0, 1, 0);
    g.add_edge(0, 2, 0);
    g.add_edge(1, 3, 0);
    g.add_edge(1, 4, 0);
    g.add_edge(2, 4, 0);
    g.add_edge(3, 5, 0);
    g.add_edge(4, 5, 0);
    g.add_edge(4, 6, 0);
    g.add_edge(5, 6, 0);

    let dur = vec![3, 4, 2, 5, 6, 3, 2];
    let names = ["Design", "Backend", "Frontend", "API",
                 "Database", "Testing", "Deploy"];

    println!("Project tasks:");
    for (i, &d) in dur.iter().enumerate() {
        println!("  Task {} ({:>8}): duration = {}", i, names[i], d);
    }
    println!();

    let r = cpm(&g, &dur);

    println!("Task       Dur   ES   EF   LS   LF  Slack  Critical");
    println!("--------- ---  ---  ---  ---  ---  -----  --------");
    for i in 0..g.n {
        println!("{:<9}  {}    {:>2}   {:>2}   {:>2}   {:>2}    {}      {}",
                 names[i], dur[i], r.es[i], r.ef[i], r.ls[i], r.lf[i],
                 r.slack[i], if r.slack[i] == 0 { "YES" } else { "no" });
    }

    println!("\nProject makespan: {} time units", r.makespan);
    print!("Critical path: ");
    let critical: Vec<String> = (0..g.n)
        .filter(|&i| r.slack[i] == 0)
        .map(|i| format!("{}({})", names[i], i))
        .collect();
    println!("{}\n", critical.join(" -> "));
}

fn demo6_levels() {
    println!("=== Demo 6: Topological Levels (Parallel Execution) ===\n");

    let g = build_example();
    let levels = topo_levels(&g);

    println!("Number of levels: {}\n", levels.len());
    for (i, level) in levels.iter().enumerate() {
        let vs: Vec<String> = level.iter().map(|v| v.to_string()).collect();
        println!("Level {}: {{ {} }}", i, vs.join(", "));
    }

    let max_width = levels.iter().map(|l| l.len()).max().unwrap_or(0);
    println!("\nMaximum parallelism: {} tasks simultaneously", max_width);
    println!("Minimum time steps: {}\n", levels.len());
}

fn demo7_lex_smallest() {
    println!("=== Demo 7: Lexicographically Smallest Topological Order ===\n");

    /* DAG where FIFO Kahn gives non-lexicographic order */
    let mut g = DAG::new(6);
    g.add_edge(5, 0, 0);
    g.add_edge(5, 2, 0);
    g.add_edge(4, 0, 0);
    g.add_edge(4, 1, 0);
    g.add_edge(2, 3, 0);
    g.add_edge(3, 1, 0);

    println!("DAG: 5->0, 5->2, 4->0, 4->1, 2->3, 3->1");
    println!("Sources (in-degree 0): 4, 5\n");

    let fifo = g.kahn().expect("DAG");
    let lex = g.kahn_lex_smallest().expect("DAG");

    let fifo_s: Vec<String> = fifo.iter().map(|v| v.to_string()).collect();
    let lex_s: Vec<String> = lex.iter().map(|v| v.to_string()).collect();

    println!("FIFO Kahn order:     [{}]", fifo_s.join(", "));
    println!("Min-heap Kahn order: [{}]", lex_s.join(", "));
    println!("The min-heap version always picks the smallest available vertex.\n");
}

fn demo8_all_orderings() {
    println!("=== Demo 8: All Topological Orderings (Backtracking) ===\n");

    let g = build_example();
    let orders = all_topo_orders(&g);

    println!("Found {} valid topological orderings:", orders.len());
    for (i, ord) in orders.iter().enumerate() {
        let s: Vec<String> = ord.iter().map(|v| v.to_string()).collect();
        println!("  {}: [{}]", i + 1, s.join(", "));
    }

    /* Verify all are valid */
    let mut all_valid = true;
    let mut pos = vec![0usize; g.n];
    for ord in &orders {
        for (i, &v) in ord.iter().enumerate() {
            pos[v] = i;
        }
        for u in 0..g.n {
            for &(v, _) in &g.adj[u] {
                if pos[u] >= pos[v] {
                    all_valid = false;
                }
            }
        }
    }
    println!("\nAll orderings valid: {}\n", all_valid);
}

/* =================== MAIN ========================================= */

fn main() {
    demo1_kahn_trace();
    demo2_dfs_topo();
    demo3_longest_shortest();
    demo4_count_paths();
    demo5_cpm();
    demo6_levels();
    demo7_lex_smallest();
    demo8_all_orderings();
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Verificación manual de Kahn

Dado el DAG:
```
0 → 2, 1 → 2, 1 → 3, 2 → 4, 3 → 4, 3 → 5, 4 → 5
```

Ejecuta Kahn paso a paso. ¿Cuál es el ordenamiento resultante si se usa una
cola FIFO y los vértices se encolan en orden numérico?

<details><summary>Predicción</summary>

In-degrees iniciales: 0:0, 1:0, 2:2, 3:1, 4:2, 5:2. Cola: {0, 1}.

- Dequeue 0 → in[2]=1. Cola: {1}.
- Dequeue 1 → in[2]=0, in[3]=0. Cola: {2, 3}.
- Dequeue 2 → in[4]=1. Cola: {3}.
- Dequeue 3 → in[4]=0, in[5]=1. Cola: {4}.
- Dequeue 4 → in[5]=0. Cola: {5}.
- Dequeue 5.

Orden: **0, 1, 2, 3, 4, 5**.

</details>

### Ejercicio 2 — Camino más largo con pesos negativos

DAG con aristas: A→B:3, A→C:-2, B→D:1, C→D:5, C→E:-1, D→E:2.

Calcula el camino más largo y más corto desde A hasta E.

<details><summary>Predicción</summary>

Topo-order: A, B, C, D, E (o A, C, B, D, E).

**Más largo** desde A:
- longest[A]=0, longest[B]=3, longest[C]=-2
- longest[D]=max(3+1, -2+5)=max(4,3)=4
- longest[E]=max(-2+(-1), 4+2)=max(-3, 6)=**6** (camino A→B→D→E)

**Más corto** desde A:
- shortest[A]=0, shortest[B]=3, shortest[C]=-2
- shortest[D]=min(4, 3)=3
- shortest[E]=min(-3, 5)=**-3** (camino A→C→E)

Nota: Dijkstra no podría resolver esto por los pesos negativos, pero DP en DAG sí.

</details>

### Ejercicio 3 — Conteo de caminos

En el DAG del ejercicio 1 (6 vértices, 7 aristas), ¿cuántos caminos hay de 0 a 5?
Enuméralos todos.

<details><summary>Predicción</summary>

count[0]=1, count[1]=1, count[2]=count[0]+count[1]=2, count[3]=count[1]=1,
count[4]=count[2]+count[3]=3, count[5]=count[3]+count[4]=4.

Los 4 caminos: 0→2→4→5, 1→2→4→5, 1→3→4→5, 1→3→5.

Pero la pregunta es desde 0, y 0 no alcanza a 1 directamente. Revisando: 0→2→4→5
es el único camino desde 0. Recalculando con fuente = 0 solamente:

count[0]=1, count[1]=0 (0 no llega a 1), count[2]=1, count[3]=0, count[4]=1,
count[5]=1.

**1 camino** desde 0 a 5: 0→2→4→5.

(Si se pregunta desde el conjunto de fuentes {0,1}, serían 4 caminos.)

</details>

### Ejercicio 4 — CPM manual

Un proyecto tiene 5 tareas:

| Tarea | Duración | Depende de |
|:-----:|:--------:|:----------:|
| A | 4 | — |
| B | 3 | A |
| C | 5 | A |
| D | 2 | B, C |
| E | 3 | D |

Calcula ES, EF, LS, LF, Slack para cada tarea. ¿Cuál es el camino crítico y
el makespan?

<details><summary>Predicción</summary>

**Forward**: ES[A]=0, EF[A]=4. ES[B]=4, EF[B]=7. ES[C]=4, EF[C]=9.
ES[D]=max(7,9)=9, EF[D]=11. ES[E]=11, EF[E]=14. Makespan=**14**.

**Backward**: LF[E]=14, LS[E]=11. LF[D]=11, LS[D]=9. LF[C]=9, LS[C]=4.
LF[B]=9, LS[B]=6. LF[A]=min(6,4)=4, LS[A]=0.

Slack: A=0, B=2, C=0, D=0, E=0.

**Camino crítico**: A → C → D → E (duración 4+5+2+3=14).

Tarea B tiene holgura 2: puede empezar hasta el tiempo 6 sin afectar el proyecto.

</details>

### Ejercicio 5 — Niveles topológicos

Para el DAG: 0→3, 0→4, 1→3, 1→4, 2→5, 3→5, 4→6, 5→6.

Determina los niveles topológicos y el máximo paralelismo posible.

<details><summary>Predicción</summary>

In-degrees: 0:0, 1:0, 2:0, 3:2, 4:2, 5:2, 6:2.

- Nivel 0: {0, 1, 2} (in-degree 0)
- Tras remover: in[3]=0, in[4]=0, in[5]=1
- Nivel 1: {3, 4}
- Tras remover: in[5]=0, in[6]=1
- Nivel 2: {5}
- Tras remover: in[6]=0
- Nivel 3: {6}

**4 niveles**. Paralelismo máximo: **3** (nivel 0).

</details>

### Ejercicio 6 — Todos los ordenamientos

Para el diamante: A→B, A→C, B→D, C→D.

Enumera todos los ordenamientos topológicos válidos.

<details><summary>Predicción</summary>

Solo A tiene in-degree 0. Después de A: B y C (ambos in-degree 0).

1. A, B, C, D
2. A, C, B, D

**2 ordenamientos** (ya mencionado en la teoría). El número es pequeño porque la
estructura es muy restrictiva: A debe ser primero, D debe ser último, solo B y C
son intercambiables.

</details>

### Ejercicio 7 — DAG con pesos negativos vs Dijkstra

DAG: 0→1:5, 0→2:3, 1→3:2, 2→1:-4, 2→3:6.

Calcula caminos más cortos desde 0 con DP en DAG. Explica por qué Dijkstra
fallaría.

<details><summary>Predicción</summary>

Topo-order: 0, 2, 1, 3.

DP: dist[0]=0. dist[2]=3 (0→2). dist[1]=min(5, 3+(-4))=min(5,-1)=**-1** (0→2→1).
dist[3]=min(-1+2, 3+6)=min(1,9)=**1** (0→2→1→3).

Dijkstra con fuente 0:
- Extrae 0 (dist=0). Relaja: dist[1]=5, dist[2]=3.
- Extrae 2 (dist=3). Relaja 1: 3+(-4)=-1 < 5, dist[1]=-1. Relaja 3: 3+6=9.
- Extrae 1... pero Dijkstra con cola de prioridad estándar no procesa pesos
  negativos correctamente. Si ya marcó 1 como finalizado con dist=5, no volvería
  a actualizarlo.

El problema: Dijkstra asume que una vez extraído un vértice, su distancia es
definitiva. Con pesos negativos, un camino más largo (más aristas) puede ser más
corto (menor peso), violando esta invariante.

</details>

### Ejercicio 8 — Lexicográficamente menor

DAG: 3→0, 3→1, 2→0, 2→1, 1→4, 0→4.

¿Cuál es el orden topológico lexicográficamente menor? ¿Y el mayor?

<details><summary>Predicción</summary>

In-degrees: 0:2, 1:2, 2:0, 3:0, 4:2. Fuentes: {2, 3}.

**Menor** (min-heap): tomar 2 primero. Tras 2: in[0]=1, in[1]=1. Fuente: {3}.
Tras 3: in[0]=0, in[1]=0. Min: 0. Tras 0: in[4]=1. Fuente: {1}. Tras 1:
in[4]=0. → **2, 3, 0, 1, 4**.

**Mayor** (max-heap): tomar 3 primero. Tras 3: in[0]=1, in[1]=1. Fuente: {2}.
Tras 2: in[0]=0, in[1]=0. Max: 1. Tras 1: in[4]=1. Fuente: {0}. Tras 0:
in[4]=0. → **3, 2, 1, 0, 4**.

</details>

### Ejercicio 9 — Detección de ciclos con Kahn

Dado el grafo dirigido: 0→1, 1→2, 2→3, 3→1.

Ejecuta Kahn y explica cómo detecta el ciclo. ¿Qué vértices quedan sin procesar?

<details><summary>Predicción</summary>

In-degrees: 0:0, 1:2, 2:1, 3:1. Cola: {0}.

- Dequeue 0 → in[1]=1. Cola: vacía.

No hay más vértices con in-degree 0. Solo procesamos 1 de 4 vértices.
$|\text{order}| = 1 < 4 = n$ → **ciclo detectado**.

Los vértices **no procesados** {1, 2, 3} forman el ciclo 1→2→3→1. Sus
in-degrees nunca llegan a 0 porque se "sostienen" mutuamente: cada uno tiene
un predecesor dentro del ciclo que nunca se remueve.

Esta es la ventaja de Kahn para identificar **qué vértices** participan en
ciclos: los no incluidos en el resultado son exactamente los que están en
algún ciclo o son alcanzables desde un ciclo (pero no necesariamente
*dentro* del ciclo).

</details>

### Ejercicio 10 — DP en DAG: número de caminos de longitud exacta k

Diseña un algoritmo para contar, dado un DAG ponderado (pesos = 1 en cada arista)
y dos vértices $s, t$, el número de caminos de exactamente $k$ aristas de $s$ a
$t$. ¿Cuál es la complejidad?

<details><summary>Predicción</summary>

Se usa DP con dos dimensiones: `dp[v][j]` = número de caminos de exactamente $j$
aristas desde $s$ hasta $v$.

Base: `dp[s][0] = 1`, `dp[v][0] = 0` para $v \neq s$.

Transición (procesar por $j = 0, \ldots, k-1$; dentro de cada $j$, en orden
topológico):

$$\text{dp}[v][j+1] = \sum_{(u,v) \in E} \text{dp}[u][j]$$

Respuesta: `dp[t][k]`.

**Complejidad**: $O(k \cdot (n + m))$ — para cada valor de $j$ hacemos un pase
sobre todos los vértices y aristas.

**Espacio**: se puede optimizar a $O(n)$ manteniendo solo dos capas (`dp_prev[]`
y `dp_curr[]`), ya que la capa $j+1$ solo depende de la capa $j$.

Alternativa: potencias de la matriz de adyacencia. El elemento $(s, t)$ de $A^k$
da el número de caminos de longitud $k$. Pero esto es $O(n^3 \log k)$ con
exponenciación rápida, peor para grafos dispersos.

</details>
