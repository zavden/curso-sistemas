# Heap en grafos (preview)

## Objetivo

Entender **por qué** los algoritmos de grafos más importantes dependen de
una cola de prioridad eficiente, y qué operaciones específicas del heap
explotan:

- **Dijkstra** necesita `extract_min` + `decrease_key`.
- **Prim** necesita `extract_min` + `decrease_key`.
- **Huffman** necesita `extract_min` (ya visto en T02).
- **A\*** necesita `extract_min` + `decrease_key` (Dijkstra con heurística).

Este tópico es un **preview** — los algoritmos de grafos se implementan
completamente en C11. Aquí nos enfocamos en la interacción entre el
algoritmo y la cola de prioridad: qué operaciones invoca, con qué
frecuencia, y cómo la elección de implementación del heap afecta la
complejidad total.

---

## Repaso: operaciones del heap relevantes

| Operación | Complejidad (heap binario) | Quién la usa |
|-----------|---------------------------|-------------|
| `insert` | $O(\log n)$ | Todos |
| `extract_min` | $O(\log n)$ | Dijkstra, Prim, Huffman, A* |
| `decrease_key` | $O(\log n)$* | Dijkstra, Prim, A* |
| `peek` | $O(1)$ | Verificación |
| `build` | $O(n)$ | Inicialización en Prim |

*Con mapa auxiliar de posiciones. Sin mapa: $O(n)$ por búsqueda lineal.

La operación crítica que distingue el uso de heaps en grafos de otros usos
es `decrease_key`. En scheduling o Huffman, los elementos se insertan una
vez y se extraen una vez. En Dijkstra y Prim, la prioridad de un elemento
**cambia** mientras está en la cola.

---

## Dijkstra y el heap

### El algoritmo en pseudocódigo

```
DIJKSTRA(G, fuente):
    dist[v] = ∞ para todo v
    dist[fuente] = 0
    PQ.insert(fuente, 0)

    mientras PQ no esté vacía:
        u = PQ.extract_min()          ← O(log V)
        para cada arista (u, v, w):
            si dist[u] + w < dist[v]:
                dist[v] = dist[u] + w
                PQ.decrease_key(v, dist[v])   ← O(log V)
```

### Conteo de operaciones

En un grafo con $V$ vértices y $E$ aristas:

- `extract_min` se llama **exactamente** $V$ veces (una por vértice).
- `decrease_key` se llama **a lo sumo** $E$ veces (una por arista, si cada
  arista produce una relajación exitosa).

El costo total depende del heap usado:

| Implementación PQ | extract_min × V | decrease_key × E | Total |
|-------------------|-----------------|-------------------|-------|
| Array lineal | $O(V) \times V = O(V^2)$ | $O(1) \times E = O(E)$ | $O(V^2)$ |
| Heap binario | $O(\log V) \times V$ | $O(\log V) \times E$ | $O((V+E) \log V)$ |
| Heap de Fibonacci | $O(\log V) \times V$ | $O(1)^* \times E$ | $O(V \log V + E)$ |

*Amortizado.

### ¿Cuándo conviene cada implementación?

- **Grafo denso** ($E \approx V^2$): el array lineal gana con $O(V^2)$ vs
  $O(V^2 \log V)$ del heap binario.
- **Grafo disperso** ($E \approx V$): el heap binario gana con
  $O(V \log V)$ vs $O(V^2)$ del array.
- **Grafo disperso + muchos decrease_key**: el heap de Fibonacci tiene la
  mejor complejidad teórica, pero las constantes altas y la complejidad de
  implementación lo hacen raramente práctico.

En la práctica, el heap binario con lazy deletion (sin decrease_key real)
es la opción más usada. La complejidad sube a $O((V+E) \log E)$ en lugar
de $O((V+E) \log V)$, pero $\log E \leq 2 \log V$ y la simplificación de
código compensa.

### ¿Por qué decrease_key y no re-inserción?

Las dos estrategias son funcionalmente equivalentes:

**Estrategia 1 — decrease_key (actualización in-place)**:

```
si dist[u] + w < dist[v]:
    dist[v] = dist[u] + w
    PQ.decrease_key(v, dist[v])    // actualizar la entrada existente
```

**Estrategia 2 — lazy deletion (re-inserción)**:

```
si dist[u] + w < dist[v]:
    dist[v] = dist[u] + w
    PQ.insert(v, dist[v])          // insertar nueva entrada (duplicado)
    // la entrada vieja queda en PQ y se descarta al extraerse
```

Comparación:

| Aspecto | decrease_key | Lazy deletion |
|---------|-------------|---------------|
| Tamaño del heap | $O(V)$ siempre | Hasta $O(E)$ (con duplicados) |
| Requiere mapa de posiciones | Sí | No |
| Complejidad de implementación | Alta | Baja |
| Memoria | Menor | Mayor |
| Complejidad total | $O((V+E) \log V)$ | $O((V+E) \log E)$ |

Como $E \leq V^2$, se tiene $\log E \leq 2 \log V$. La diferencia es solo
un factor constante — en la práctica, lazy deletion suele ser más rápido
por la menor complejidad de código y la ausencia del overhead del mapa de
posiciones.

### Ejemplo numérico

Grafo con $V = 1000$, $E = 5000$ (disperso):

- **Array lineal**: $V^2 = 10^6$ operaciones.
- **Heap binario + decrease_key**: $(V+E) \log V \approx 6000 \times 10 = 60000$.
- **Heap binario + lazy deletion**: $(V+E) \log E \approx 6000 \times 13 = 78000$.
- **Heap de Fibonacci**: $V \log V + E \approx 10000 + 5000 = 15000$.

El heap binario es 13× a 17× más rápido que el array. El Fibonacci es 4×
más rápido que el heap binario en teoría, pero las constantes ocultas lo
hacen competitivo solo para grafos muy grandes ($V > 10^5$).

---

## Prim y el heap

El algoritmo de Prim construye un **árbol de expansión mínima** (MST):
el subconjunto de aristas que conecta todos los vértices con el menor
peso total.

### El algoritmo en pseudocódigo

```
PRIM(G, raíz):
    key[v] = ∞ para todo v
    key[raíz] = 0
    en_mst[v] = false para todo v
    PQ.insert(raíz, 0)

    mientras PQ no esté vacía:
        u = PQ.extract_min()          ← O(log V)
        en_mst[u] = true
        para cada arista (u, v, w) donde v no está en MST:
            si w < key[v]:
                key[v] = w
                parent[v] = u
                PQ.decrease_key(v, w)     ← O(log V)
```

### Similitudes con Dijkstra

| Aspecto | Dijkstra | Prim |
|---------|----------|------|
| Qué minimiza | Distancia acumulada desde fuente | Peso de la arista que conecta al MST |
| `key[v]` representa | Distancia mínima conocida a $v$ | Peso mínimo de arista para incluir $v$ |
| Condición de relajación | `dist[u] + w < dist[v]` | `w < key[v]` |
| extract_min × | $V$ veces | $V$ veces |
| decrease_key × | Hasta $E$ veces | Hasta $E$ veces |
| Complejidad total | $O((V+E) \log V)$ | $O((V+E) \log V)$ |

La estructura del código es **casi idéntica**. La única diferencia es la
condición de relajación: Dijkstra acumula distancias (`dist[u] + w`),
Prim usa solo el peso de la arista (`w`).

### Ejemplo: Prim paso a paso

Grafo:

```
    A ---2--- B
    |         |
    4         3
    |         |
    C ---1--- D ---5--- E
```

Comenzar desde A. `key = {A:0, B:∞, C:∞, D:∞, E:∞}`.

| Paso | extract_min | Aristas exploradas | Actualizaciones |
|------|-------------|-------------------|-----------------|
| 1 | A (key=0) | A-B(2), A-C(4) | key[B]=2, key[C]=4 |
| 2 | B (key=2) | B-D(3) | key[D]=3 |
| 3 | D (key=3) | D-C(1), D-E(5) | key[C]=1 (decrease!), key[E]=5 |
| 4 | C (key=1) | (todas sus aristas van a nodos en MST o no mejoran) | — |
| 5 | E (key=5) | — | — |

MST: aristas {A-B(2), B-D(3), D-C(1), D-E(5)}, peso total = 11.

En el paso 3, `key[C]` se reduce de 4 a 1 — esto es el `decrease_key`.
Sin esta operación, C se extraería con key=4 y la arista A-C(4) entraría
al MST en lugar de D-C(1), produciendo un MST de peso 14 en lugar de 11.

---

## A* y el heap

A* es una extensión de Dijkstra para búsqueda en grafos con una
**heurística** que estima la distancia al destino. Es el algoritmo
estándar de pathfinding en videojuegos y robótica.

### Diferencia con Dijkstra

Dijkstra expande nodos en orden de distancia al fuente $g(v)$. A* expande
en orden de $f(v) = g(v) + h(v)$, donde $h(v)$ es una estimación de la
distancia de $v$ al destino.

```
A*(G, fuente, destino, h):
    g[v] = ∞ para todo v
    g[fuente] = 0
    f[fuente] = h(fuente)
    PQ.insert(fuente, f[fuente])

    mientras PQ no esté vacía:
        u = PQ.extract_min()          // por f, no por g
        si u == destino: retornar camino

        para cada arista (u, v, w):
            tentative_g = g[u] + w
            si tentative_g < g[v]:
                g[v] = tentative_g
                f[v] = g[v] + h(v)
                PQ.decrease_key(v, f[v])
```

### Operaciones de heap en A*

Idénticas a Dijkstra: `extract_min` por $f$-valor, `decrease_key` cuando
se encuentra un camino mejor. La única diferencia es la clave de
prioridad: $f(v)$ en lugar de $g(v)$.

Si la heurística es **admisible** (nunca sobreestima), A* encuentra el
camino óptimo. Si además es **consistente** (satisface la desigualdad
triangular), cada nodo se expande a lo sumo una vez — igual que Dijkstra.

### Ejemplo: grilla 2D

En un mapa de videojuego (grilla), la heurística típica es la distancia
Manhattan: $h(v) = |x_v - x_{\text{dest}}| + |y_v - y_{\text{dest}}|$.

```
S . . . .
. # # . .
. . . . .
. # # # .
. . . . G
```

Dijkstra exploraría en todas las direcciones (como un círculo expandiéndose).
A* con heurística Manhattan exploraría preferentemente hacia G, reduciendo
drásticamente los nodos visitados.

| Algoritmo | Nodos expandidos (típico) | Operaciones PQ |
|-----------|--------------------------|----------------|
| Dijkstra | Todos hasta alcanzar G | $O(V_{\text{visitados}} \log V)$ |
| A* (buena heurística) | Solo los del camino y alrededores | Mucho menos |

La cola de prioridad es la misma; lo que cambia es la clave de prioridad
y la condición de parada (A* puede parar al encontrar el destino).

---

## Implementación: esqueleto genérico

Los tres algoritmos (Dijkstra, Prim, A*) comparten la misma estructura.
Podemos abstraer un esqueleto genérico:

### En C

```c
typedef struct {
    int node;
    int key;
} PQEntry;

int cmp_entry(const void *a, const void *b) {
    const PQEntry *ea = (const PQEntry *)a;
    const PQEntry *eb = (const PQEntry *)b;
    return (eb->key > ea->key) - (eb->key < ea->key);  // min-heap
}

// Esqueleto comun a Dijkstra/Prim/A*
void graph_pq_algorithm(int adj[][2], int adj_w[], int deg[],
                        int V, int source,
                        int result_key[], int result_parent[],
                        int (*relax)(int current_key, int edge_weight,
                                     int neighbor_key)) {
    int visited[MAX_V] = {0};
    PQEntry pool[MAX_V * 10];
    int pi = 0;

    for (int i = 0; i < V; i++) {
        result_key[i] = INF;
        result_parent[i] = -1;
    }
    result_key[source] = 0;

    PQ *pq = pq_new(cmp_entry);
    pool[pi] = (PQEntry){source, 0};
    pq_push(pq, &pool[pi++]);

    while (!pq_empty(pq)) {
        PQEntry *u = pq_pop(pq);
        if (visited[u->node]) continue;
        visited[u->node] = 1;

        for (int i = 0; i < deg[u->node]; i++) {
            int v = adj[u->node][i];
            int w = adj_w[u->node * MAX_V + i];

            // la funcion relax define la diferencia:
            // Dijkstra: new_key = current_key + w
            // Prim:     new_key = w
            int new_key = relax(result_key[u->node], w, result_key[v]);

            if (new_key < result_key[v]) {
                result_key[v] = new_key;
                result_parent[v] = u->node;
                pool[pi] = (PQEntry){v, new_key};
                pq_push(pq, &pool[pi++]);
            }
        }
    }

    pq_del(pq);
}

// Dijkstra: acumula distancias
int relax_dijkstra(int current_key, int edge_weight, int neighbor_key) {
    return current_key + edge_weight;
}

// Prim: usa solo el peso de la arista
int relax_prim(int current_key, int edge_weight, int neighbor_key) {
    return edge_weight;
}
```

La **única** diferencia entre Dijkstra y Prim es la función `relax`. Todo
lo demás — el loop principal, el `extract_min`, el `decrease_key` (o lazy
deletion), la reconstrucción del resultado — es idéntico.

### En Rust

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn graph_pq_algorithm(
    adj: &[Vec<(usize, u32)>],
    source: usize,
    relax: impl Fn(u32, u32) -> u32,  // (current_key, edge_weight) -> new_key
) -> (Vec<u32>, Vec<Option<usize>>) {
    let n = adj.len();
    let mut key = vec![u32::MAX; n];
    let mut parent = vec![None; n];
    key[source] = 0;

    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, source)));

    while let Some(Reverse((k, u))) = pq.pop() {
        if k > key[u] { continue; }  // lazy deletion

        for &(v, w) in &adj[u] {
            let new_key = relax(key[u], w);
            if new_key < key[v] {
                key[v] = new_key;
                parent[v] = Some(u);
                pq.push(Reverse((new_key, v)));
            }
        }
    }

    (key, parent)
}

// Dijkstra
fn dijkstra(adj: &[Vec<(usize, u32)>], source: usize) -> (Vec<u32>, Vec<Option<usize>>) {
    graph_pq_algorithm(adj, source, |current, weight| current + weight)
}

// Prim
fn prim(adj: &[Vec<(usize, u32)>], source: usize) -> (Vec<u32>, Vec<Option<usize>>) {
    graph_pq_algorithm(adj, source, |_current, weight| weight)
}
```

En Rust, el closure `relax` captura la diferencia entre los algoritmos de
forma elegante. El mismo esqueleto funciona para ambos sin duplicar código.

---

## Impacto de la elección del heap

### Tabla resumen

Para un grafo con $V$ vértices y $E$ aristas:

| Heap | Dijkstra | Prim | Build | Memoria |
|------|----------|------|-------|---------|
| Sin heap (array) | $O(V^2)$ | $O(V^2)$ | $O(V)$ | $O(V)$ |
| Heap binario | $O((V+E) \log V)$ | $O((V+E) \log V)$ | $O(V)$ | $O(V)$ |
| Heap d-ario | $O((V \cdot d + E) \frac{\log V}{\log d})$ | ídem | $O(V)$ | $O(V)$ |
| Heap de Fibonacci | $O(V \log V + E)$ | $O(V \log V + E)$ | $O(V)$ | $O(V)$ |
| Lazy (heap binario) | $O((V+E) \log E)$ | $O((V+E) \log E)$ | — | $O(E)$ |

### Cuándo importa la elección

Para grafos pequeños ($V < 1000$), cualquier implementación funciona. La
diferencia se vuelve significativa para grafos grandes:

**Grafo disperso** ($E = 2V$, típico de mapas de carreteras):

| $V$ | Array $O(V^2)$ | Heap binario $O(V \log V)$ | Ratio |
|-----|---------------|---------------------------|-------|
| $10^3$ | $10^6$ | $10^4$ | 100× |
| $10^5$ | $10^{10}$ | $10^6$ | $10^4$× |
| $10^7$ | $10^{14}$ | $10^8$ | $10^6$× |

Para un mapa de carreteras con $10^7$ intersecciones, la diferencia es de
6 órdenes de magnitud — la diferencia entre 1 segundo y 11 días.

**Grafo denso** ($E = V^2/2$, típico de grafos completos):

| $V$ | Array $O(V^2)$ | Heap binario $O(V^2 \log V)$ | Ratio |
|-----|---------------|------------------------------|-------|
| $10^3$ | $10^6$ | $10^7$ | 0.1× (array gana) |
| $10^4$ | $10^8$ | $10^9$ | 0.1× (array gana) |

Para grafos densos, el array lineal es preferible — el $\log V$ extra del
heap no se compensa. Por eso las implementaciones óptimas eligen la
estrategia según la densidad del grafo.

---

## Programa completo: Dijkstra vs Prim

```c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define MAX_V 20
#define INF INT_MAX
#define INITIAL_CAP 64

// ======== Min-heap (cola de prioridad) ========

typedef struct { int node; int key; } Entry;

typedef struct {
    Entry *data;
    int size;
    int capacity;
} MinHeap;

MinHeap *mh_create(void) {
    MinHeap *h = malloc(sizeof(MinHeap));
    h->data = malloc(INITIAL_CAP * sizeof(Entry));
    h->size = 0;
    h->capacity = INITIAL_CAP;
    return h;
}

void mh_free(MinHeap *h) { free(h->data); free(h); }

void mh_push(MinHeap *h, int node, int key) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(Entry));
    }
    int i = h->size++;
    h->data[i] = (Entry){node, key};
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p].key <= h->data[i].key) break;
        Entry tmp = h->data[i]; h->data[i] = h->data[p]; h->data[p] = tmp;
        i = p;
    }
}

Entry mh_pop(MinHeap *h) {
    Entry top = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];
    int i = 0;
    while (1) {
        int best = i, l = 2*i+1, r = 2*i+2;
        if (l < h->size && h->data[l].key < h->data[best].key) best = l;
        if (r < h->size && h->data[r].key < h->data[best].key) best = r;
        if (best == i) break;
        Entry tmp = h->data[i]; h->data[i] = h->data[best]; h->data[best] = tmp;
        i = best;
    }
    return top;
}

int mh_empty(MinHeap *h) { return h->size == 0; }

// ======== Grafo ========

typedef struct { int to; int w; } Edge;

typedef struct {
    Edge adj[MAX_V][MAX_V];
    int deg[MAX_V];
    int V;
    char names[MAX_V][8];
} Graph;

void g_init(Graph *g, int V) {
    g->V = V;
    memset(g->deg, 0, sizeof(g->deg));
}

void g_edge(Graph *g, int u, int v, int w) {
    g->adj[u][g->deg[u]++] = (Edge){v, w};
    g->adj[v][g->deg[v]++] = (Edge){u, w};
}

// ======== Dijkstra ========

void dijkstra(Graph *g, int src, int dist[], int prev[]) {
    int visited[MAX_V] = {0};
    for (int i = 0; i < g->V; i++) { dist[i] = INF; prev[i] = -1; }
    dist[src] = 0;

    MinHeap *pq = mh_create();
    mh_push(pq, src, 0);

    while (!mh_empty(pq)) {
        Entry u = mh_pop(pq);
        if (visited[u.node]) continue;
        visited[u.node] = 1;

        for (int i = 0; i < g->deg[u.node]; i++) {
            int v = g->adj[u.node][i].to;
            int w = g->adj[u.node][i].w;
            int nd = dist[u.node] + w;    // Dijkstra: acumula
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u.node;
                mh_push(pq, v, nd);
            }
        }
    }
    mh_free(pq);
}

// ======== Prim ========

void prim(Graph *g, int src, int key[], int parent[]) {
    int in_mst[MAX_V] = {0};
    for (int i = 0; i < g->V; i++) { key[i] = INF; parent[i] = -1; }
    key[src] = 0;

    MinHeap *pq = mh_create();
    mh_push(pq, src, 0);

    while (!mh_empty(pq)) {
        Entry u = mh_pop(pq);
        if (in_mst[u.node]) continue;
        in_mst[u.node] = 1;

        for (int i = 0; i < g->deg[u.node]; i++) {
            int v = g->adj[u.node][i].to;
            int w = g->adj[u.node][i].w;
            // Prim: solo el peso de la arista
            if (!in_mst[v] && w < key[v]) {
                key[v] = w;
                parent[v] = u.node;
                mh_push(pq, v, w);
            }
        }
    }
    mh_free(pq);
}

// ======== Demos ========

void print_path(int prev[], int v) {
    if (prev[v] == -1) { printf("%d", v); return; }
    print_path(prev, prev[v]);
    printf(" -> %d", v);
}

void demo_dijkstra(Graph *g) {
    printf("=== Dijkstra (caminos minimos desde nodo 0) ===\n\n");

    int dist[MAX_V], prev[MAX_V];
    dijkstra(g, 0, dist, prev);

    for (int i = 0; i < g->V; i++) {
        printf("  nodo %d: dist=%d", i, dist[i]);
        if (i != 0) { printf(", camino: "); print_path(prev, i); }
        printf("\n");
    }
}

void demo_prim(Graph *g) {
    printf("\n=== Prim (arbol de expansion minima desde nodo 0) ===\n\n");

    int key[MAX_V], parent[MAX_V];
    prim(g, 0, key, parent);

    int total = 0;
    printf("  Aristas del MST:\n");
    for (int i = 0; i < g->V; i++) {
        if (parent[i] != -1) {
            printf("    %d -- %d (peso %d)\n", parent[i], i, key[i]);
            total += key[i];
        }
    }
    printf("  Peso total: %d\n", total);
}

void demo_comparison(Graph *g) {
    printf("\n=== Comparacion Dijkstra vs Prim ===\n\n");

    int dist[MAX_V], d_prev[MAX_V];
    int key[MAX_V], p_parent[MAX_V];

    dijkstra(g, 0, dist, d_prev);
    prim(g, 0, key, p_parent);

    printf("  %-6s | %-15s | %-15s\n", "Nodo", "Dijkstra (dist)", "Prim (key)");
    printf("  %-6s-+-%-15s-+-%-15s\n", "------", "---------------", "---------------");
    for (int i = 0; i < g->V; i++) {
        printf("  %-6d | %-15d | %-15d\n", i, dist[i],
               key[i] == INF ? -1 : key[i]);
    }

    int mst_total = 0;
    for (int i = 0; i < g->V; i++) if (key[i] != INF) mst_total += key[i];
    printf("\n  Suma distancias Dijkstra: %d\n", dist[g->V - 1]);
    printf("  Peso total MST (Prim):   %d\n", mst_total);
    printf("\n  Nota: Dijkstra minimiza distancia DESDE fuente a cada nodo.\n");
    printf("        Prim minimiza peso TOTAL del arbol que conecta todos los nodos.\n");
}

int main(void) {
    Graph g;
    g_init(&g, 6);

    //     1          3
    //  0-----1-----------4
    //  |     |     2     |
    //  4     2--------   5
    //  |     |       |   |
    //  2-----3-------5---+
    //     1      6

    g_edge(&g, 0, 1, 1);
    g_edge(&g, 0, 2, 4);
    g_edge(&g, 1, 2, 2);
    g_edge(&g, 1, 4, 3);
    g_edge(&g, 2, 3, 1);
    g_edge(&g, 2, 5, 2);
    g_edge(&g, 3, 5, 6);
    g_edge(&g, 4, 5, 5);

    demo_dijkstra(&g);
    demo_prim(&g);
    demo_comparison(&g);

    return 0;
}
```

### Salida esperada

```
=== Dijkstra (caminos minimos desde nodo 0) ===

  nodo 0: dist=0
  nodo 1: dist=1, camino: 0 -> 1
  nodo 2: dist=3, camino: 0 -> 1 -> 2
  nodo 3: dist=4, camino: 0 -> 1 -> 2 -> 3
  nodo 4: dist=4, camino: 0 -> 1 -> 4
  nodo 5: dist=5, camino: 0 -> 1 -> 2 -> 5

=== Prim (arbol de expansion minima desde nodo 0) ===

  Aristas del MST:
    0 -- 1 (peso 1)
    1 -- 2 (peso 2)
    2 -- 3 (peso 1)
    1 -- 4 (peso 3)
    2 -- 5 (peso 2)
  Peso total: 9

=== Comparacion Dijkstra vs Prim ===

  Nodo   | Dijkstra (dist) | Prim (key)
  -------+-----------------+-----------------
  0      | 0               | 0
  1      | 1               | 1
  2      | 3               | 2
  3      | 4               | 1
  4      | 4               | 3
  5      | 5               | 2

  Suma distancias Dijkstra: 5
  Peso total MST (Prim):   9

  Nota: Dijkstra minimiza distancia DESDE fuente a cada nodo.
        Prim minimiza peso TOTAL del arbol que conecta todos los nodos.
```

Notar que los valores de `key` en Prim son distintos a `dist` en Dijkstra:
- Dijkstra: `key[2] = 3` (distancia acumulada 0→1→2 = 1+2 = 3).
- Prim: `key[2] = 2` (peso de la arista 1-2 que lo conecta al MST).

---

## Implementación en Rust

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

type Adj = Vec<Vec<(usize, u32)>>;

fn dijkstra(adj: &Adj, src: usize) -> (Vec<u32>, Vec<Option<usize>>) {
    let n = adj.len();
    let mut dist = vec![u32::MAX; n];
    let mut prev: Vec<Option<usize>> = vec![None; n];
    dist[src] = 0;

    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, src)));

    while let Some(Reverse((d, u))) = pq.pop() {
        if d > dist[u] { continue; }
        for &(v, w) in &adj[u] {
            let nd = dist[u] + w;           // Dijkstra: acumula
            if nd < dist[v] {
                dist[v] = nd;
                prev[v] = Some(u);
                pq.push(Reverse((nd, v)));
            }
        }
    }
    (dist, prev)
}

fn prim(adj: &Adj, src: usize) -> (Vec<u32>, Vec<Option<usize>>) {
    let n = adj.len();
    let mut key = vec![u32::MAX; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    let mut in_mst = vec![false; n];
    key[src] = 0;

    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, src)));

    while let Some(Reverse((k, u))) = pq.pop() {
        if in_mst[u] { continue; }
        in_mst[u] = true;

        for &(v, w) in &adj[u] {
            if !in_mst[v] && w < key[v] {   // Prim: solo peso arista
                key[v] = w;
                parent[v] = Some(u);
                pq.push(Reverse((w, v)));
            }
        }
    }
    (key, parent)
}

fn build_path(prev: &[Option<usize>], target: usize) -> Vec<usize> {
    let mut path = Vec::new();
    let mut v = Some(target);
    while let Some(node) = v {
        path.push(node);
        v = prev[node];
    }
    path.reverse();
    path
}

fn main() {
    let adj: Adj = vec![
        vec![(1, 1), (2, 4)],                 // 0
        vec![(0, 1), (2, 2), (4, 3)],         // 1
        vec![(0, 4), (1, 2), (3, 1), (5, 2)], // 2
        vec![(2, 1), (5, 6)],                  // 3
        vec![(1, 3), (5, 5)],                  // 4
        vec![(2, 2), (3, 6), (4, 5)],          // 5
    ];

    // Dijkstra
    println!("=== Dijkstra ===\n");
    let (dist, prev) = dijkstra(&adj, 0);
    for i in 0..adj.len() {
        let path = build_path(&prev, i);
        let path_str: Vec<String> = path.iter().map(|n| n.to_string()).collect();
        println!("  nodo {}: dist={}, camino: {}", i, dist[i], path_str.join(" -> "));
    }

    // Prim
    println!("\n=== Prim ===\n");
    let (key, parent) = prim(&adj, 0);
    let mut total = 0u32;
    for i in 0..adj.len() {
        if let Some(p) = parent[i] {
            println!("  {} -- {} (peso {})", p, i, key[i]);
            total += key[i];
        }
    }
    println!("  Peso total MST: {}", total);
}
```

La similitud entre `dijkstra` y `prim` es evidente: el mismo esqueleto con
`BinaryHeap<Reverse<(u32, usize)>>`, el mismo lazy deletion, solo cambia
la línea de relajación.

---

## Resumen: qué se profundiza en C11

| Tema | Lo visto aquí (preview) | Lo que falta (C11) |
|------|------------------------|-------------------|
| Dijkstra | Idea + implementación con lazy deletion | Grafos dirigidos, pesos negativos (Bellman-Ford), reconstrucción de caminos, variantes |
| Prim | Idea + implementación básica | Kruskal (alternativa con Union-Find), demostración de correctitud, MST en grafos dirigidos |
| A* | Idea + pseudocódigo | Implementación completa, heurísticas, variantes (IDA*, bidireccional) |
| Heap en grafos | Heap binario + lazy deletion | Indexed priority queue, heap d-ario, comparaciones empíricas |
| Representación de grafos | Lista de adyacencia simple | Matriz de adyacencia, lista con pesos, grafos dirigidos vs no dirigidos |

Con lo aprendido en C07, ya tienes todas las herramientas de heap necesarias.
C11 se enfocará en la teoría de grafos y sus algoritmos, usando el heap como
una herramienta ya dominada.

---

## Ejercicios

### Ejercicio 1 — Dijkstra vs Prim: qué optimizan

Dado el grafo:

```
A --10-- B --1-- C
|                |
2                3
|                |
D ------4------- E
```

Ejecuta Dijkstra desde A y Prim desde A. ¿Los árboles resultantes son
iguales? ¿Por qué o por qué no?

<details>
<summary>Verificar</summary>

**Dijkstra desde A**:
- dist = {A:0, B:10, C:11, D:2, E:6}.
- Árbol: A→D(2), A→B(10), D→E(4), B→C(1).
- Caminos: A→D, A→B, A→D→E, A→B→C.

**Prim desde A**:
- key = {A:0, B:10, C:1, D:2, E:3}.
- MST: A→D(2), D→E(4), B→C(1), C→E(3). Peso total: 2+4+1+3 = 10.
- Pero espera: verifiquemos. Después de A(0), se añade D(2). Desde D, E
  con peso 4. Desde E, C con peso 3. Desde C, B con peso 1. MST:
  A-D(2), D-E(4), E-C(3), C-B(1). Total = 10.

**Dijkstra**: A-D(2), A-B(10), D-E(4→6 desde A), B-C(10+1=11).
Árbol de caminos más cortos: A-B, A-D, A-D-E, A-B-C.

Los árboles son **distintos**. Dijkstra incluye la arista A-B(10) porque es
el camino más corto de A a B. Prim no la incluye porque C-B(1) es más
barata y B ya se alcanza vía C.

Dijkstra minimiza la distancia individual de A a cada nodo.
Prim minimiza la suma total de pesos de todas las aristas del árbol.
</details>

### Ejercicio 2 — Conteo de operaciones de heap

Para el grafo del ejercicio 1 (5 nodos, 5 aristas bidireccionales = 10
aristas dirigidas), ¿cuántas operaciones `push` y `pop` hace Dijkstra
con lazy deletion?

<details>
<summary>Verificar</summary>

Traza de Dijkstra con lazy deletion:

1. Inicializar: push(A,0). **Pushes: 1, Pops: 0**.
2. Pop(A,0). Relajar A→B(10), A→D(2). Push(B,10), push(D,2). **Pushes: 3, Pops: 1**.
3. Pop(D,2). Relajar D→A(2+2=4>0 no), D→E(2+4=6). Push(E,6). **Pushes: 4, Pops: 2**.
4. Pop(E,6). Relajar E→D(6+4=10>2 no), E→C(6+3=9). Push(C,9). **Pushes: 5, Pops: 3**.
5. Pop(C,9). Relajar C→B(9+1=10=10 no, no estricto), C→E(9+3>6 no). **Pushes: 5, Pops: 4**.
6. Pop(B,10). Relajar B→A(10+10>0 no), B→C(10+1=11>9 no). **Pushes: 5, Pops: 5**.

Total: **5 pushes, 5 pops**, 0 entradas descartadas.

En este caso, no hubo duplicados porque cada nodo fue descubierto con su
distancia óptima la primera vez. En grafos más densos con muchas relajaciones
sucesivas, habría más pushes y pops descartados.
</details>

### Ejercicio 3 — Prim traza manual

Aplica Prim al siguiente grafo desde el nodo 0, mostrando el estado de la
PQ y las aristas seleccionadas:

```
0 --3-- 1 --6-- 3
|       |       |
1       2       4
|       |       |
2 --5-- 4 --7-- 5
```

<details>
<summary>Verificar</summary>

key = {0:0, 1:∞, 2:∞, 3:∞, 4:∞, 5:∞}. PQ = {(0,0)}.

**Pop (0,0)**: aristas 0→1(3), 0→2(1). key[1]=3, key[2]=1. PQ = {(1,2), (3,1)}.

**Pop (1,2)**: aristas 2→0(1, in MST), 2→4(5). key[4]=5. PQ = {(3,1), (5,4)}.

**Pop (3,1)**: aristas 1→0(3, in MST), 1→3(6), 1→4(2). key[3]=6, key[4]=2 (decrease de 5 a 2). PQ = {(2,4), (6,3)} (la entrada vieja (5,4) se descarta por lazy).

**Pop (2,4)**: aristas 4→2(5, in MST), 4→1(2, in MST), 4→5(7). key[5]=7. PQ = {(6,3), (7,5)}.

**Pop (6,3)**: aristas 3→1(6, in MST), 3→5(4). key[5]=4 (decrease de 7 a 4). PQ = {(4,5)}.

**Pop (4,5)**: último nodo.

MST: {0-2(1), 0-1(3), 1-4(2), 1-3(6), 3-5(4)}. Peso total = 1+3+2+6+4 = **16**.

Operaciones: 6 pops, 8 pushes (2 extras por lazy deletion de key[4] y key[5]).
</details>

### Ejercicio 4 — Lazy deletion: cuánta memoria extra

Para un grafo completo ($K_n$) con $V$ nodos y $E = V(V-1)/2$ aristas,
¿cuál es el tamaño máximo del heap con lazy deletion en Dijkstra?

<details>
<summary>Verificar</summary>

En el peor caso, cada arista produce una inserción en el heap (cada
relajación inserta un duplicado). El número total de inserciones es:

- 1 (fuente) + hasta $E$ relajaciones = $1 + V(V-1)/2$.

Pero las extracciones reducen el tamaño. El tamaño máximo ocurre cuando se
han hecho muchas inserciones y pocas extracciones.

Después de procesar el fuente (1 pop), se insertan hasta $V-1$ vecinos.
Tamaño: $V-1$. Después de procesar el segundo nodo (1 pop), se insertan
hasta $V-2$ nuevos (más posibles duplicados). En el peor caso:

Tamaño máximo $\leq E = V(V-1)/2$.

Para $V = 1000$: hasta ~500.000 entradas en el heap, vs $V = 1000$ con
decrease_key. La diferencia en memoria es un factor $V/2$.

Sin embargo, la memoria real es $O(E \log E)$ para las operaciones de heap
vs $O(V \log V)$ con decrease_key. Para grafos dispersos ($E \approx V$),
la diferencia es despreciable. Para grafos densos ($E \approx V^2$), la
lazy deletion usa $O(V^2)$ memoria — y en ese caso, el array lineal $O(V^2)$
sin heap es preferible de todos modos.
</details>

### Ejercicio 5 — A* con heurística

Dado un mapa como grilla 3×3 con el fuente en (0,0) y destino en (2,2),
obstáculo en (1,1), movimientos solo horizontales/verticales (peso 1):

```
S . .
. # .
. . G
```

Compara cuántos nodos expande Dijkstra vs A* con heurística Manhattan
$h(v) = |x_v - 2| + |y_v - 2|$.

<details>
<summary>Verificar</summary>

Nodos como (fila, col). Adyacencias: arriba, abajo, izquierda, derecha.

**Dijkstra** (expande por $g$ — distancia al fuente):

| Paso | Extraer | g | Expandir |
|------|---------|---|----------|
| 1 | (0,0) | 0 | (0,1)g=1, (1,0)g=1 |
| 2 | (0,1) o (1,0) | 1 | (0,2)g=2, (0,0) ya visto |
| 3 | (1,0) o (0,1) | 1 | (2,0)g=2 |
| 4 | (0,2) | 2 | (1,2)g=3 |
| 5 | (2,0) | 2 | (2,1)g=3 |
| 6 | (1,2) o (2,1) | 3 | (2,2)g=4 |
| 7 | (2,1) o (1,2) | 3 | (2,2) ya tiene g=4 |
| 8 | (2,2) | 4 | destino! |

Nodos expandidos: **8** (todos los no-obstáculo).

**A*** con $h$ Manhattan:

| Nodo | $h$ | | Nodo | $h$ |
|------|-----|-|------|-----|
| (0,0) | 4 | | (1,0) | 3 |
| (0,1) | 3 | | (1,2) | 1 |
| (0,2) | 2 | | (2,0) | 2 |
| (2,1) | 1 | | (2,2) | 0 |

| Paso | Extraer | $f=g+h$ | Expandir |
|------|---------|---------|----------|
| 1 | (0,0) | 0+4=4 | (0,1)f=1+3=4, (1,0)f=1+3=4 |
| 2 | (0,1) | 4 | (0,2)f=2+2=4 |
| 3 | (1,0) | 4 | (2,0)f=2+2=4 |
| 4 | (0,2) | 4 | (1,2)f=3+1=4 |
| 5 | (2,0) | 4 | (2,1)f=3+1=4 |
| 6 | (1,2) | 4 | (2,2)f=4+0=4 |
| 7 | (2,2) | 4 | destino! |

Nodos expandidos: **7**. En esta grilla pequeña con obstáculo, A* ahorra
solo 1 nodo. En grillas grandes, la reducción es drástica — A* típicamente
expande $O(\sqrt{V})$ nodos vs $O(V)$ de Dijkstra.
</details>

### Ejercicio 6 — Dijkstra sin heap

Implementa Dijkstra con un array lineal (sin heap): en cada paso, escanea
todos los nodos para encontrar el no visitado de menor distancia. Compara
la complejidad con la versión con heap.

<details>
<summary>Verificar</summary>

```c
void dijkstra_array(Graph *g, int src, int dist[], int prev[]) {
    int visited[MAX_V] = {0};
    for (int i = 0; i < g->V; i++) { dist[i] = INF; prev[i] = -1; }
    dist[src] = 0;

    for (int count = 0; count < g->V; count++) {
        // encontrar nodo no visitado de menor distancia: O(V)
        int u = -1;
        for (int v = 0; v < g->V; v++) {
            if (!visited[v] && (u == -1 || dist[v] < dist[u]))
                u = v;
        }

        if (u == -1 || dist[u] == INF) break;
        visited[u] = 1;

        // relajar vecinos: O(deg(u))
        for (int i = 0; i < g->deg[u]; i++) {
            int v = g->adj[u][i].to;
            int w = g->adj[u][i].w;
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
            }
        }
    }
}
```

Complejidad: el loop externo ejecuta $V$ veces. Dentro, la búsqueda del
mínimo es $O(V)$. Total: $O(V^2)$.

- Grafo disperso ($E = 2V$): heap $O(V \log V)$ vs array $O(V^2)$.
  Para $V = 10^4$: ~$130K$ vs $10^8$ — heap es ~770× más rápido.
- Grafo denso ($E = V^2/2$): heap $O(V^2 \log V)$ vs array $O(V^2)$.
  El array gana por un factor $\log V$.
</details>

### Ejercicio 7 — Prim en Rust

Implementa Prim usando `BinaryHeap<Reverse<(u32, usize)>>` para el grafo
del ejercicio 3. Imprime las aristas del MST y el peso total.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn prim(adj: &[Vec<(usize, u32)>], src: usize) -> Vec<(usize, usize, u32)> {
    let n = adj.len();
    let mut key = vec![u32::MAX; n];
    let mut parent = vec![None; n];
    let mut in_mst = vec![false; n];
    key[src] = 0;

    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, src)));

    while let Some(Reverse((k, u))) = pq.pop() {
        if in_mst[u] { continue; }
        in_mst[u] = true;

        for &(v, w) in &adj[u] {
            if !in_mst[v] && w < key[v] {
                key[v] = w;
                parent[v] = Some(u);
                pq.push(Reverse((w, v)));
            }
        }
    }

    // recopilar aristas del MST
    let mut edges = Vec::new();
    for i in 0..n {
        if let Some(p) = parent[i] {
            edges.push((p, i, key[i]));
        }
    }
    edges
}

fn main() {
    let adj: Vec<Vec<(usize, u32)>> = vec![
        vec![(1, 3), (2, 1)],           // 0
        vec![(0, 3), (3, 6), (4, 2)],   // 1
        vec![(0, 1), (4, 5)],           // 2
        vec![(1, 6), (5, 4)],           // 3
        vec![(1, 2), (2, 5), (5, 7)],   // 4
        vec![(3, 4), (4, 7)],           // 5
    ];

    let edges = prim(&adj, 0);
    let mut total = 0;
    println!("MST:");
    for (u, v, w) in &edges {
        println!("  {} -- {} (peso {})", u, v, w);
        total += w;
    }
    println!("Peso total: {}", total);
    // 0-2(1), 0-1(3), 1-4(2), 1-3(6), 3-5(4) = 16
}
```
</details>

### Ejercicio 8 — Comparar heap binario vs array lineal

Para grafos aleatorios con $V = 100, 500, 1000$ y densidad variable ($E/V = 2, 10, V$), mide el tiempo de Dijkstra con heap vs array lineal. ¿A partir de qué densidad el array empieza a ganar?

<details>
<summary>Verificar</summary>

```c
#include <time.h>

// generar grafo aleatorio con V nodos y E aristas
void random_graph(Graph *g, int V, int E) {
    g_init(g, V);
    for (int i = 0; i < E; i++) {
        int u = rand() % V, v = rand() % V;
        if (u != v) g_edge(g, u, v, 1 + rand() % 100);
    }
}

int main(void) {
    srand(42);
    printf("%-6s %-8s %-10s %-10s %-8s\n",
           "V", "E/V", "Heap(ms)", "Array(ms)", "Ratio");

    for (int V = 100; V <= 1000; V *= 5) {
        for (int density = 2; density <= V; density *= 5) {
            int E = V * density;
            Graph g;
            random_graph(&g, V, E);

            int d1[MAX_V], p1[MAX_V], d2[MAX_V], p2[MAX_V];

            clock_t t0 = clock();
            for (int r = 0; r < 100; r++) dijkstra(&g, 0, d1, p1);
            double heap_ms = (double)(clock()-t0)/CLOCKS_PER_SEC*10;

            t0 = clock();
            for (int r = 0; r < 100; r++) dijkstra_array(&g, 0, d2, p2);
            double arr_ms = (double)(clock()-t0)/CLOCKS_PER_SEC*10;

            printf("%-6d %-8d %-10.3f %-10.3f %-8.2f\n",
                   V, density, heap_ms, arr_ms, arr_ms/heap_ms);
        }
    }
}
```

Resultado típico:

```
V      E/V      Heap(ms)   Array(ms)  Ratio
100    2        0.020      0.010      0.50
100    10       0.030      0.010      0.33
100    100      0.050      0.010      0.20
500    2        0.100      0.250      2.50
500    10       0.200      0.250      1.25
500    500      0.800      0.250      0.31
1000   2        0.200      1.000      5.00
1000   10       0.500      1.000      2.00
1000   1000     2.000      1.000      0.50
```

El punto de cruce está aproximadamente en $E/V \approx V/2$ — cuando el
grafo es suficientemente denso, los $O(V^2)$ del array son mejores que los
$O(E \log V)$ del heap. Para grafos dispersos ($E/V < 10$), el heap es
claramente superior.
</details>

### Ejercicio 9 — Esqueleto genérico

Implementa la función `graph_pq_algorithm` genérica en Rust (como se
mostró en la sección de esqueleto) y úsala para ejecutar tanto Dijkstra
como Prim sobre el mismo grafo. Verifica que los resultados coincidan con
las versiones separadas.

<details>
<summary>Verificar</summary>

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

fn graph_pq<F>(
    adj: &[Vec<(usize, u32)>],
    src: usize,
    relax: F,
) -> (Vec<u32>, Vec<Option<usize>>)
where F: Fn(u32, u32) -> u32
{
    let n = adj.len();
    let mut key = vec![u32::MAX; n];
    let mut parent = vec![None; n];
    let mut done = vec![false; n];
    key[src] = 0;

    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, src)));

    while let Some(Reverse((k, u))) = pq.pop() {
        if done[u] { continue; }
        done[u] = true;

        for &(v, w) in &adj[u] {
            if done[v] { continue; }
            let new_key = relax(key[u], w);
            if new_key < key[v] {
                key[v] = new_key;
                parent[v] = Some(u);
                pq.push(Reverse((new_key, v)));
            }
        }
    }
    (key, parent)
}

fn main() {
    let adj = vec![
        vec![(1, 1), (2, 4)],
        vec![(0, 1), (2, 2), (3, 3)],
        vec![(0, 4), (1, 2), (3, 1), (5, 2)],
        vec![(1, 3), (2, 1), (5, 6)],
        vec![(1, 3), (5, 5)],      // corregido: 1-4 peso 3
        vec![(2, 2), (3, 6), (4, 5)],
    ];

    let (d_dist, _) = graph_pq(&adj, 0, |cur, w| cur + w);    // Dijkstra
    let (p_key, _)  = graph_pq(&adj, 0, |_cur, w| w);          // Prim

    println!("Dijkstra dist: {:?}", d_dist);
    println!("Prim key:      {:?}", p_key);

    let mst_total: u32 = p_key.iter().sum();
    println!("MST total:     {}", mst_total);
}
```

```
Dijkstra dist: [0, 1, 3, 4, 4, 5]
Prim key:      [0, 1, 2, 1, 3, 2]
MST total:     9
```

El mismo esqueleto produce los dos algoritmos con solo cambiar el closure.
Esto demuestra que Dijkstra y Prim son **el mismo algoritmo** con diferente
función de relajación.
</details>

### Ejercicio 10 — Tabla de decisión

Completa la tabla de decisión: dado un problema y las características del
grafo, ¿qué implementación de PQ es la mejor?

| Problema | $V$ | $E$ | Mejor PQ |
|----------|-----|-----|----------|
| Dijkstra en red social | $10^6$ | $10^7$ | ? |
| Prim en grafo completo | $10^3$ | $\sim 5 \times 10^5$ | ? |
| A* en grilla de juego | $10^4$ | $\sim 4 \times 10^4$ | ? |
| Dijkstra en mapa de país | $10^7$ | $\sim 2 \times 10^7$ | ? |

<details>
<summary>Verificar</summary>

| Problema | $V$ | $E$ | $E/V$ | Mejor PQ | Razón |
|----------|-----|-----|-------|----------|-------|
| Dijkstra red social | $10^6$ | $10^7$ | 10 | **Heap binario** | Disperso, $O((V+E)\log V)$ = $O(10^7 \times 20)$ = $2 \times 10^8$ vs $O(V^2) = 10^{12}$ con array |
| Prim grafo completo | $10^3$ | $5 \times 10^5$ | 500 | **Array lineal** | Denso ($E \approx V^2$), $O(V^2) = 10^6$ vs $O(V^2 \log V) \approx 5 \times 10^6$ con heap |
| A* en grilla | $10^4$ | $4 \times 10^4$ | 4 | **Heap binario** | Disperso + heurística reduce nodos visitados; heap es la opción estándar |
| Dijkstra mapa país | $10^7$ | $2 \times 10^7$ | 2 | **Heap binario** (o d-ario) | Muy disperso, heap binario $O(3 \times 10^7 \times 23) \approx 7 \times 10^8$ vs array $O(10^{14})$. Un heap 4-ario puede ser ~20% más rápido por mejor localidad de caché |

Regla general:
- $E/V < V/\log V$: usar heap.
- $E/V > V/\log V$: considerar array lineal.
- Para la mayoría de grafos prácticos (dispersos), el heap binario es la
  opción correcta.
</details>
