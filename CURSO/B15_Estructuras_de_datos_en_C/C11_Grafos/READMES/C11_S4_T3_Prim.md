# T03 — Prim

## 1. El algoritmo

Prim construye el MST **haciendo crecer un árbol** desde un vértice fuente. En cada
paso, agrega la arista más ligera que cruza el corte entre los vértices ya incluidos
($S$) y los restantes ($V \setminus S$):

```
PRIM(G, src):
    key[v] = INF for all v;  key[src] = 0
    parent[v] = -1 for all v
    in_mst[v] = false for all v
    Q = priority queue with all vertices, keyed by key[]

    while Q not empty:
        u = extract_min(Q)
        in_mst[u] = true
        for each (v, w) in adj[u]:
            if !in_mst[v] and w < key[v]:
                key[v] = w
                parent[v] = u
                decrease_key(Q, v, w)

    return parent[]   // MST edges: (parent[v], v) for v != src
```

`key[v]` almacena el peso de la arista más ligera que conecta $v$ al conjunto $S$.
`parent[v]` almacena el otro extremo de esa arista.


## 2. Traza paso a paso

Grafo de T01/T02 (6 vértices), Prim desde vértice 0:

```
        4           2
    0-------1-------2
    |      /|      /|
    |3   8/ |6   5/ |7
    |   /   |   /   |
    3-------4-------5
        9       1
```

| Paso | $S$ | Extraer | Arista MST | Actualizaciones de key |
|------|-----|---------|------------|------------------------|
| 1 | {0} | 0 (key=0) | — (fuente) | key[1]=4, key[3]=3 |
| 2 | {0,3} | 3 (key=3) | (0,3):3 | key[4]=9 (8>4, 1 no mejora) |
| 3 | {0,1,3} | 1 (key=4) | (0,1):4 | key[2]=2, key[4]=6 |
| 4 | {0,1,2,3} | 2 (key=2) | (1,2):2 | key[4]=5, key[5]=7 |
| 5 | {0,1,2,3,4} | 4 (key=5) | (2,4):5 | key[5]=1 |
| 6 | {0,...,5} | 5 (key=1) | (4,5):1 | — |

MST: $\{(0,3):3,\; (0,1):4,\; (1,2):2,\; (2,4):5,\; (4,5):1\}$, peso total = 15.

Resultado idéntico al de Kruskal (T02). El orden de descubrimiento difiere: Prim
procesa vértices por cercanía al árbol parcial; Kruskal procesa aristas globalmente
por peso.


## 3. Corrección

En cada paso, $S$ define un corte $(S, V \setminus S)$. Prim extrae el vértice $v$
con `key[v]` mínimo, que corresponde a la **arista ligera del corte**. Por la
**propiedad de corte** (T01), esta arista pertenece a algún MST. La invariante
se mantiene en cada paso, y al final las $n - 1$ aristas forman un MST.


## 4. Implementación con arreglo — $O(n^2)$

La versión más simple: en cada iteración, recorrer linealmente todos los vértices para
encontrar el de menor key:

```
for n iterations:
    u = argmin key[v] for v not in MST    // O(n) scan
    in_mst[u] = true
    for each (v, w) in adj[u]:            // O(degree(u))
        if !in_mst[v] and w < key[v]:
            key[v] = w
            parent[v] = u
```

- **Encontrar mínimo**: $O(n)$ por iteración, $n$ iteraciones → $O(n^2)$.
- **Relajaciones**: cada arista se examina una vez por extremo → $O(m)$ total.
- **Total**: $O(n^2)$ (domina el escaneo).

Esta versión es **óptima para grafos densos** ($m \approx n^2$) porque no tiene
overhead de estructura de datos.


## 5. Implementación con heap — $O(m \log n)$

Usar una cola de prioridad (min-heap) para encontrar el mínimo en $O(\log n)$.
La estrategia más práctica es **lazy deletion**: push duplicados al heap y descartar
los obsoletos al extraer.

```
heap = min-heap of (weight, vertex)
push (0, src)

while heap not empty:
    (w, u) = pop()
    if in_mst[u]: continue       // skip stale entry
    in_mst[u] = true
    for each (v, weight) in adj[u]:
        if !in_mst[v] and weight < key[v]:
            key[v] = weight
            parent[v] = u
            push(weight, v)      // may create duplicate
```

- **Pushes**: a lo sumo $m$ (uno por relajación).
- **Pops**: a lo sumo $m + n$ (incluyendo duplicados).
- **Total**: $O(m \log n)$.

### Alternativa: decrease-key

En lugar de duplicados, actualizar la prioridad del vértice existente. Con un heap
binario indexado esto cuesta $O(\log n)$ por operación, dando el mismo $O(m \log n)$
asintótico pero con menor uso de memoria.


## 6. Comparación de implementaciones

| Implementación | Complejidad | Mejor para |
|---------------|-------------|------------|
| **Arreglo** | $O(n^2)$ | Grafos densos ($m \approx n^2$) |
| **Binary heap** | $O(m \log n)$ | Grafos dispersos ($m \ll n^2$) |
| **Fibonacci heap** | $O(m + n \log n)$ | Teórico (constantes grandes) |

**Punto de cruce** arreglo vs heap: cuando $n^2 \approx m \log n$, es decir
$m \approx n^2 / \log n$. Para grafos más densos, el arreglo gana; para más
dispersos, el heap.

Para un grafo completo $K_n$ ($m = n(n{-}1)/2$):
arreglo $O(n^2)$ vs heap $O(n^2 \log n)$ → arreglo gana por factor $\log n$.


## 7. Prim vs Kruskal

| Aspecto | Prim | Kruskal |
|---------|------|---------|
| Enfoque | Crece desde un vértice | Procesa aristas globalmente |
| Estructura | Lista de adyacencia + heap | Lista de aristas + Union-Find |
| Complejidad | $O(m \log n)$ o $O(n^2)$ | $O(m \log n)$ |
| Grafos densos | $O(n^2)$ con arreglo (mejor) | $O(m \log m) = O(n^2 \log n)$ |
| Grafos dispersos | $O(m \log n)$ (similar) | $O(m \log n)$ (similar) |
| Pesos negativos | Funciona correctamente | Funciona correctamente |
| Desconexión | Solo encuentra MST de una componente | Encuentra bosque completo |
| Paralelismo | Difícil de paralelizar | Borůvka es más paralelo |


## 8. Similitud con Dijkstra

Prim y Dijkstra comparten la misma estructura de priority-first search, pero
difieren en la **regla de relajación**:

| | Prim (MST) | Dijkstra (SPT) |
|---|------------|----------------|
| key[v] representa | Peso de arista mínima a $S$ | Distancia acumulada desde src |
| Relajación | `key[v] = min(key[v], w(u,v))` | `dist[v] = min(dist[v], dist[u]+w(u,v))` |
| Resultado | Árbol generador mínimo | Árbol de caminos más cortos |
| Pesos negativos | Funciona | **No** funciona |

Producen árboles **distintos** en general. El MST minimiza la suma total de pesos;
el SPT minimiza distancias individuales desde la fuente.


## 9. Detalles prácticos

- **Vértice inicial**: no afecta las aristas del MST (si los pesos son distintos, el
  MST es único). Solo cambia el parent[] y el orden de descubrimiento.
- **Pesos negativos**: Prim funciona correctamente — solo compara pesos individuales,
  no acumula distancias.
- **Grafos desconexos**: Prim solo construye el MST de la componente del vértice fuente.
  Para un bosque generador mínimo, ejecutar Prim desde cada vértice no visitado.
- **Self-loops**: ignorados automáticamente (no mejoran key de ningún vecino útil).


## 10. Programa en C

```c
/* prim.c — Prim's algorithm: heap and array implementations */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define MAXV 20
#define INF  1000000

typedef struct { int to, w; } Adj;
static Adj adj[MAXV][MAXV];
static int deg[MAXV];

void clear_graph(int n) {
    for (int i = 0; i < n; i++) deg[i] = 0;
}

void add_edge(int u, int v, int w) {
    adj[u][deg[u]++] = (Adj){v, w};
    adj[v][deg[v]++] = (Adj){u, w};
}

/* ── Min-heap (weight, vertex) ── */
typedef struct { int w, v; } HE;
static HE heap[500];
static int hsz;

void h_init(void) { hsz = 0; }

void h_push(int w, int v) {
    int i = hsz++;
    heap[i] = (HE){w, v};
    while (i > 0 && heap[(i-1)/2].w > heap[i].w) {
        HE t = heap[(i-1)/2]; heap[(i-1)/2] = heap[i]; heap[i] = t;
        i = (i-1)/2;
    }
}

HE h_pop(void) {
    HE top = heap[0];
    heap[0] = heap[--hsz];
    int i = 0;
    for (;;) {
        int l = 2*i+1, r = 2*i+2, b = i;
        if (l < hsz && heap[l].w < heap[b].w) b = l;
        if (r < hsz && heap[r].w < heap[b].w) b = r;
        if (b == i) break;
        HE t = heap[i]; heap[i] = heap[b]; heap[b] = t;
        i = b;
    }
    return top;
}

/* ── Prim with heap (lazy deletion) ── */
int prim_heap(int n, int src, int par[], int key[], bool trace) {
    bool in_mst[MAXV];
    memset(in_mst, false, n * sizeof(bool));
    for (int i = 0; i < n; i++) { key[i] = INF; par[i] = -1; }
    key[src] = 0;
    h_init();
    h_push(0, src);
    int total = 0;

    while (hsz > 0) {
        HE e = h_pop();
        if (in_mst[e.v]) continue;
        in_mst[e.v] = true;
        total += e.w;
        if (trace && e.v != src)
            printf("  Add %d via (%d,%d):%d\n", e.v, par[e.v], e.v, e.w);
        for (int i = 0; i < deg[e.v]; i++) {
            int v = adj[e.v][i].to, w = adj[e.v][i].w;
            if (!in_mst[v] && w < key[v]) {
                key[v] = w;
                par[v] = e.v;
                h_push(w, v);
            }
        }
    }
    return total;
}

/* ── Prim with array O(n²) ── */
int prim_array(int n, int src, int par[], int key[], bool trace) {
    bool in_mst[MAXV];
    memset(in_mst, false, n * sizeof(bool));
    for (int i = 0; i < n; i++) { key[i] = INF; par[i] = -1; }
    key[src] = 0;
    int total = 0;

    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        for (int i = 0; i < n; i++)
            if (!in_mst[i] && (u == -1 || key[i] < key[u])) u = i;
        if (key[u] >= INF) break;
        in_mst[u] = true;
        total += key[u];
        if (trace && u != src)
            printf("  Add %d via (%d,%d):%d\n", u, par[u], u, key[u]);
        for (int i = 0; i < deg[u]; i++) {
            int v = adj[u][i].to, w = adj[u][i].w;
            if (!in_mst[v] && w < key[v]) {
                key[v] = w;
                par[v] = u;
            }
        }
    }
    return total;
}

void print_mst_edges(int par[], int key[], int n) {
    for (int i = 0; i < n; i++)
        if (par[i] != -1)
            printf("  (%d,%d):%d", par[i], i, key[i]);
    printf("\n");
}

/* ========================================================= */
int main(void) {
    int n = 6;
    clear_graph(n);
    add_edge(0,1,4); add_edge(0,3,3); add_edge(1,2,2);
    add_edge(1,3,8); add_edge(1,4,6); add_edge(2,4,5);
    add_edge(2,5,7); add_edge(3,4,9); add_edge(4,5,1);

    int par[MAXV], key[MAXV];

    /* == Demo 1: Prim heap from vertex 0 == */
    printf("=== Demo 1: Prim (heap) from vertex 0 ===\n");
    int t1 = prim_heap(n, 0, par, key, true);
    printf("Total: %d\nEdges:", t1);
    print_mst_edges(par, key, n);

    /* == Demo 2: Prim array from vertex 0 == */
    printf("\n=== Demo 2: Prim (array) from vertex 0 ===\n");
    int t2 = prim_array(n, 0, par, key, true);
    printf("Total: %d\nEdges:", t2);
    print_mst_edges(par, key, n);
    printf("Heap vs array: %s\n", t1 == t2 ? "MATCH" : "DIFFER");

    /* == Demo 3: Different starting vertices == */
    printf("\n=== Demo 3: Different starting vertices ===\n");
    int par0[MAXV], key0[MAXV], par3[MAXV], key3[MAXV], par5[MAXV], key5[MAXV];
    int s0 = prim_heap(n, 0, par0, key0, false);
    int s3 = prim_heap(n, 3, par3, key3, false);
    int s5 = prim_heap(n, 5, par5, key5, false);
    printf("From 0: total=%d  edges:", s0); print_mst_edges(par0, key0, n);
    printf("From 3: total=%d  edges:", s3); print_mst_edges(par3, key3, n);
    printf("From 5: total=%d  edges:", s5); print_mst_edges(par5, key5, n);
    printf("All produce same MST edge set with same total weight\n");

    /* == Demo 4: Prim vs Dijkstra (SPT vs MST) == */
    printf("\n=== Demo 4: Prim vs Dijkstra ===\n");
    prim_heap(n, 0, par, key, false);
    printf("MST (Prim):  ");
    for (int i = 0; i < n; i++)
        if (par[i] != -1) printf("(%d,%d):%d ", par[i], i, key[i]);
    int mst_w = 0;
    for (int i = 0; i < n; i++) if (par[i] != -1) mst_w += key[i];
    printf(" total=%d\n", mst_w);

    /* Dijkstra from 0 (array, O(n²)) */
    int dist[MAXV], dpar[MAXV];
    bool vis[MAXV];
    memset(vis, false, n * sizeof(bool));
    for (int i = 0; i < n; i++) { dist[i] = INF; dpar[i] = -1; }
    dist[0] = 0;
    for (int iter = 0; iter < n; iter++) {
        int u = -1;
        for (int i = 0; i < n; i++)
            if (!vis[i] && (u == -1 || dist[i] < dist[u])) u = i;
        vis[u] = true;
        for (int i = 0; i < deg[u]; i++) {
            int v = adj[u][i].to, w = adj[u][i].w;
            if (!vis[v] && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                dpar[v] = u;
            }
        }
    }
    printf("SPT (Dijkstra):");
    int spt_w = 0;
    for (int i = 0; i < n; i++)
        if (dpar[i] != -1) {
            int ew = dist[i] - dist[dpar[i]];
            printf(" (%d,%d):%d", dpar[i], i, ew);
            spt_w += ew;
        }
    printf("  total=%d\n", spt_w);
    printf("MST minimizes total weight (%d < %d)\n", mst_w, spt_w);
    printf("SPT minimizes distances: dist=[");
    for (int i = 0; i < n; i++) printf("%d%s", dist[i], i < n-1 ? "," : "");
    printf("]\n");

    /* == Demo 5: K6 (dense — array wins) == */
    printf("\n=== Demo 5: Prim on K6 (dense) ===\n");
    clear_graph(6);
    add_edge(0,1,7); add_edge(0,2,4); add_edge(0,3,9); add_edge(0,4,3); add_edge(0,5,8);
    add_edge(1,2,5); add_edge(1,3,2); add_edge(1,4,11); add_edge(1,5,6);
    add_edge(2,3,10); add_edge(2,4,1); add_edge(2,5,12);
    add_edge(3,4,13); add_edge(3,5,14);
    add_edge(4,5,15);
    int tk = prim_array(n, 0, par, key, true);
    printf("Total: %d\n", tk);
    printf("K6: m=15, n=6. Array O(n²)=O(36) vs heap O(m log n)=O(39)\n");
    printf("Array is simpler and faster for dense graphs\n");

    /* == Demo 6: MST as rooted tree == */
    printf("\n=== Demo 6: MST as rooted tree ===\n");
    printf("parent[]: ");
    for (int i = 0; i < n; i++) printf("%d ", par[i]);
    printf("\nRooted at 0:\n");
    /* Print hierarchy */
    for (int depth = 0; depth < n; depth++) {
        for (int i = 0; i < n; i++) {
            /* Check if i is at this depth */
            int d = 0, x = i;
            while (par[x] != -1) { x = par[x]; d++; }
            if (d == depth) {
                for (int s = 0; s < d; s++) printf("  ");
                if (par[i] != -1) printf("+-- %d (weight %d)\n", i, key[i]);
                else printf("%d (root)\n", i);
            }
        }
    }

    return 0;
}
```


## 11. Programa en Rust

```rust
// prim.rs — Prim's algorithm: heap and array, comparison with Dijkstra

use std::collections::BinaryHeap;
use std::cmp::Reverse;

type AdjList = Vec<Vec<(usize, i32)>>;

fn build_graph(n: usize, edges: &[(usize, usize, i32)]) -> AdjList {
    let mut adj = vec![vec![]; n];
    for &(u, v, w) in edges {
        adj[u].push((v, w));
        adj[v].push((u, w));
    }
    adj
}

fn prim_heap(adj: &AdjList, src: usize, trace: bool)
    -> (i32, Vec<i32>, Vec<Option<usize>>)
{
    let n = adj.len();
    let mut key = vec![i32::MAX; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    let mut in_mst = vec![false; n];
    let mut heap = BinaryHeap::new();

    key[src] = 0;
    heap.push(Reverse((0i32, src)));
    let mut total = 0;

    while let Some(Reverse((w, u))) = heap.pop() {
        if in_mst[u] { continue; }
        in_mst[u] = true;
        total += w;
        if trace && u != src {
            println!("  Add {} via ({},{}):{}", u, parent[u].unwrap(), u, w);
        }
        for &(v, weight) in &adj[u] {
            if !in_mst[v] && weight < key[v] {
                key[v] = weight;
                parent[v] = Some(u);
                heap.push(Reverse((weight, v)));
            }
        }
    }
    (total, key, parent)
}

fn prim_array(adj: &AdjList, src: usize, trace: bool)
    -> (i32, Vec<i32>, Vec<Option<usize>>)
{
    let n = adj.len();
    let mut key = vec![i32::MAX; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    let mut in_mst = vec![false; n];

    key[src] = 0;
    let mut total = 0;

    for _ in 0..n {
        let u = (0..n).filter(|&i| !in_mst[i])
            .min_by_key(|&i| key[i]).unwrap();
        if key[u] == i32::MAX { break; }
        in_mst[u] = true;
        total += key[u];
        if trace && u != src {
            println!("  Add {} via ({},{}):{}", u, parent[u].unwrap(), u, key[u]);
        }
        for &(v, w) in &adj[u] {
            if !in_mst[v] && w < key[v] {
                key[v] = w;
                parent[v] = Some(u);
            }
        }
    }
    (total, key, parent)
}

fn dijkstra(adj: &AdjList, src: usize) -> (Vec<i32>, Vec<Option<usize>>) {
    let n = adj.len();
    let mut dist = vec![i32::MAX; n];
    let mut parent: Vec<Option<usize>> = vec![None; n];
    let mut visited = vec![false; n];
    dist[src] = 0;

    for _ in 0..n {
        let u = (0..n).filter(|&i| !visited[i])
            .min_by_key(|&i| dist[i]).unwrap();
        if dist[u] == i32::MAX { break; }
        visited[u] = true;
        for &(v, w) in &adj[u] {
            if !visited[v] && dist[u] + w < dist[v] {
                dist[v] = dist[u] + w;
                parent[v] = Some(u);
            }
        }
    }
    (dist, parent)
}

fn print_mst(parent: &[Option<usize>], key: &[i32]) {
    for (i, p) in parent.iter().enumerate() {
        if let Some(pi) = p {
            print!(" ({},{}){}", pi, i, key[i]);
        }
    }
}

fn main() {
    let n = 6;
    let edges = vec![
        (0,1,4), (0,3,3), (1,2,2), (1,3,8), (1,4,6),
        (2,4,5), (2,5,7), (3,4,9), (4,5,1),
    ];
    let adj = build_graph(n, &edges);

    // == Demo 1: Prim heap trace ==
    println!("=== Demo 1: Prim (heap) from vertex 0 ===");
    let (t1, key1, par1) = prim_heap(&adj, 0, true);
    print!("Total: {}  edges:", t1); print_mst(&par1, &key1); println!();

    // == Demo 2: Prim array trace ==
    println!("\n=== Demo 2: Prim (array) from vertex 0 ===");
    let (t2, key2, par2) = prim_array(&adj, 0, true);
    print!("Total: {}  edges:", t2); print_mst(&par2, &key2); println!();
    println!("Heap vs array: {}", if t1 == t2 { "MATCH" } else { "DIFFER" });

    // == Demo 3: Different starting vertices ==
    println!("\n=== Demo 3: Different starting vertices ===");
    for src in [0, 2, 5] {
        let (t, key, par) = prim_heap(&adj, src, false);
        print!("  From {}: total={}  edges:", src, t);
        print_mst(&par, &key);
        println!();
    }
    println!("  Same edge set regardless of start");

    // == Demo 4: Prim vs Dijkstra ==
    println!("\n=== Demo 4: Prim (MST) vs Dijkstra (SPT) ===");
    let (mt, mk, mp) = prim_heap(&adj, 0, false);
    let (dist, dp) = dijkstra(&adj, 0);

    print!("  MST edges:");
    print_mst(&mp, &mk);
    println!("  total={}", mt);

    print!("  SPT edges:");
    let mut spt_w = 0;
    for (i, p) in dp.iter().enumerate() {
        if let Some(pi) = p {
            let ew = dist[i] - dist[*pi];
            print!(" ({},{}):{}", pi, i, ew);
            spt_w += ew;
        }
    }
    println!("  total={}", spt_w);

    println!("  MST minimizes total weight ({} < {})", mt, spt_w);
    println!("  SPT minimizes distances: {:?}", dist);

    // Highlight the difference
    println!("  Difference at vertex 4:");
    println!("    MST uses ({},4):{}  (lighter edge)",
        mp[4].unwrap(), mk[4]);
    println!("    SPT uses ({},4):{}  (shorter path: dist={})",
        dp[4].unwrap(), dist[4] - dist[dp[4].unwrap()], dist[4]);

    // == Demo 5: Negative weights ==
    println!("\n=== Demo 5: Prim with negative weights ===");
    let neg_edges = vec![(0,1,-3), (0,2,5), (1,2,2), (1,3,4), (2,3,-1)];
    let neg_adj = build_graph(4, &neg_edges);
    let (nt, nk, np) = prim_heap(&neg_adj, 0, true);
    print!("Total: {}  edges:", nt); print_mst(&np, &nk); println!();
    println!("Prim works with negative weights (unlike Dijkstra)");

    // == Demo 6: K6 (dense) ==
    println!("\n=== Demo 6: Prim on K6 ===");
    let k6_edges = vec![
        (0,1,7),(0,2,4),(0,3,9),(0,4,3),(0,5,8),
        (1,2,5),(1,3,2),(1,4,11),(1,5,6),
        (2,3,10),(2,4,1),(2,5,12),
        (3,4,13),(3,5,14),
        (4,5,15),
    ];
    let k6 = build_graph(6, &k6_edges);
    let (kt, kk, kp) = prim_array(&k6, 0, true);
    print!("Total: {}  edges:", kt); print_mst(&kp, &kk); println!();
    println!("Dense graph: array O(n²) beats heap O(m log n)");

    // == Demo 7: MST as rooted tree ==
    println!("\n=== Demo 7: MST as rooted tree (from vertex 0) ===");
    let (_, rk, rp) = prim_heap(&adj, 0, false);
    fn print_subtree(par: &[Option<usize>], key: &[i32], root: usize, indent: usize) {
        for s in 0..indent { if s % 2 == 0 { print!("|") } else { print!(" ") } }
        if indent > 0 { print!("-- "); }
        println!("{} (key={})", root, key[root]);
        for (i, p) in par.iter().enumerate() {
            if *p == Some(root) {
                print_subtree(par, key, i, indent + 2);
            }
        }
    }
    print_subtree(&rp, &rk, 0, 0);

    // == Demo 8: Spanning forest ==
    println!("\n=== Demo 8: Spanning forest (disconnected) ===");
    let disc = build_graph(6, &[(0,1,3),(0,2,5),(1,2,4),(3,4,2),(3,5,7),(4,5,6)]);
    let mut visited = vec![false; 6];
    let mut forest_total = 0;
    let mut comp = 0;
    for start in 0..6 {
        if visited[start] { continue; }
        comp += 1;
        let (t, _, par) = prim_heap(&disc, start, false);
        forest_total += t;
        let members: Vec<usize> = par.iter().enumerate()
            .filter(|(i, _)| {
                let mut x = *i;
                loop {
                    match par[x] { Some(p) => x = p, None => break }
                }
                x == start
            })
            .map(|(i, _)| i)
            .collect();
        // Simpler: just check which vertices are reachable
        print!("  Component {}: {{", comp);
        let (_, _, p2) = prim_array(&disc, start, false);
        for i in 0..6 {
            if i == start || p2[i].is_some() {
                // Check if connected to start
                let mut x = i;
                let mut conn = false;
                loop {
                    if x == start { conn = true; break; }
                    match p2[x] { Some(p) => x = p, None => break }
                }
                if conn { print!("{} ", i); visited[i] = true; }
            }
        }
        println!("}}  MST weight: {}", t);
    }
    println!("  Total forest weight: {}, {} components", forest_total, comp);
}
```


## 12. Ejercicios

**Ejercicio 1**: trace Prim (con arreglo) desde el vértice 2 en el grafo del programa.
Muestre la tabla con $S$, vértice extraído, arista MST y actualizaciones de key.

<details><summary>Predicción</summary>

| Paso | $S$ | Extraer | Arista | key actualizado |
|------|-----|---------|--------|-----------------|
| 1 | {2} | 2 (key=0) | — | key[1]=2, key[4]=5, key[5]=7 |
| 2 | {1,2} | 1 (key=2) | (2,1):2 | key[0]=4, key[3]=8 |
| 3 | {0,1,2} | 0 (key=4) | (1,0):4 | key[3]=3 (3<8) |
| 4 | {0,1,2,3} | 3 (key=3) | (0,3):3 | (4,9): 9>5 no |
| 5 | {0,1,2,3,4} | 4 (key=5) | (2,4):5 | key[5]=1 (1<7) |
| 6 | {0,...,5} | 5 (key=1) | (4,5):1 | — |

MST: {(2,1):2, (1,0):4, (0,3):3, (2,4):5, (4,5):1}. Total = 15. Mismo MST.
</details>

---

**Ejercicio 2**: para el grafo completo $K_n$, compare las operaciones de Prim con
arreglo vs Prim con heap para $n = 100$ y $n = 1000$.

<details><summary>Predicción</summary>

$K_n$: $m = n(n{-}1)/2$.

| $n$ | $m$ | Arreglo $O(n^2)$ | Heap $O(m \log n)$ |
|-----|-----|------------------|-------------------|
| 100 | 4950 | 10 000 | $4950 \times 7 \approx 34\,650$ |
| 1000 | 499 500 | 1 000 000 | $499\,500 \times 10 \approx 4\,995\,000$ |

El arreglo es **3-5× más rápido** para grafos densos. La diferencia crece con $n$.
Para grafos completos, siempre usar la versión con arreglo.
</details>

---

**Ejercicio 3**: aplique Prim al grafo completo $K_5$ con pesos:
(0,1):8, (0,2):5, (0,3):10, (0,4):2, (1,2):3, (1,3):6, (1,4):9,
(2,3):7, (2,4):4, (3,4):1. Inicio en vértice 0.

<details><summary>Predicción</summary>

key = [0, ∞, ∞, ∞, ∞]

1. Extraer 0: key[1]=8, key[2]=5, key[3]=10, key[4]=2.
2. Extraer 4 (key=2): edge (0,4):2. key[1]=9→no, key[2]=4→sí, key[3]=1→sí.
   key = [0, 8, 4, 1, 2, ∞]
3. Extraer 3 (key=1): edge (4,3):1. key[1]=6→sí, key[2] 7>4 no.
   key = [0, 6, 4, 1, 2]
4. Extraer 2 (key=4): edge (4,2):4. key[1] 3<6→sí.
   key = [0, 3, 4, 1, 2]
5. Extraer 1 (key=3): edge (2,1):3.

MST: {(0,4):2, (4,3):1, (4,2):4, (2,1):3}. Total = 10.
</details>

---

**Ejercicio 4**: demuestre que el vértice inicial no afecta las aristas del MST
cuando todos los pesos son distintos.

<details><summary>Predicción</summary>

Si los pesos son distintos, el MST es **único** (demostrado en T01). Prim produce un
MST válido independientemente del vértice inicial (corrección vía propiedad de corte).
Como solo hay un MST posible, cualquier ejecución correcta produce el mismo conjunto
de aristas.

Lo que cambia es el arreglo `parent[]` (la raíz del árbol) y el **orden** en que se
descubren las aristas, pero el conjunto de aristas MST es idéntico.
</details>

---

**Ejercicio 5**: en el grafo del programa, identifique la diferencia entre el MST
(Prim) y el SPT (Dijkstra) desde el vértice 0. ¿En qué vértice difieren?

<details><summary>Predicción</summary>

MST (Prim): aristas (0,3):3, (0,1):4, (1,2):2, (2,4):5, (4,5):1.
SPT (Dijkstra): aristas (0,3):3, (0,1):4, (1,2):2, **(1,4):6**, (4,5):1.

Difieren en el **vértice 4**:
- MST usa (2,4):5 — arista más liviana (peso 5 < 6).
- SPT usa (1,4):6 — camino más corto desde 0 (dist=10 vs dist=11 por 0→1→2→4).

MST prioriza peso total mínimo (15 vs 16).
SPT prioriza distancias individuales mínimas.
</details>

---

**Ejercicio 6**: ¿funciona Prim con pesos negativos? Considere el grafo con aristas
(0,1):-3, (0,2):5, (1,2):2. Trace Prim desde 0.

<details><summary>Predicción</summary>

key = [0, ∞, ∞].

1. Extraer 0: key[1]=-3, key[2]=5.
2. Extraer 1 (key=-3): edge (0,1):-3. key[2]: min(5, 2) = 2 → key[2]=2, parent[2]=1.
3. Extraer 2 (key=2): edge (1,2):2.

MST: {(0,1):-3, (1,2):2}. Total = -1. **Correcto.**

Prim funciona porque solo compara pesos individuales de aristas (`w < key[v]`), sin
acumular distancias. No importa que $w$ sea negativo — la comparación sigue siendo
válida. Dijkstra falla con negativos porque acumula ($\text{dist}[u] + w$).
</details>

---

**Ejercicio 7**: dibuje el MST como árbol enraizado en el vértice 0, usando el arreglo
parent[] = [-1, 0, 1, 0, 2, 4] del grafo del programa. Calcule la profundidad de
cada vértice.

<details><summary>Predicción</summary>

```
0 (depth 0)
├── 1 (depth 1, edge weight 4)
│   └── 2 (depth 2, edge weight 2)
│       └── 4 (depth 3, edge weight 5)
│           └── 5 (depth 4, edge weight 1)
└── 3 (depth 1, edge weight 3)
```

Profundidades: 0→0, 1→1, 2→2, 3→1, 4→3, 5→4.
Diámetro del árbol MST (enraizado): 4 (camino 3→0→1→2→4→5).

Nota: si enraizamos en otro vértice, la forma cambia pero las aristas no.
</details>

---

**Ejercicio 8**: ¿qué ocurre si ejecutamos Prim en un grafo desconexo con dos
componentes {0,1,2} y {3,4,5}, iniciando en el vértice 0?

<details><summary>Predicción</summary>

Prim solo explora vértices alcanzables desde 0. Construye el MST de la componente
{0,1,2} y termina. Los vértices 3, 4, 5 quedan con `key[v] = INF` y `in_mst[v] = false`.

Para obtener el **bosque generador mínimo** completo, se debe:
1. Ejecutar Prim desde 0 → MST de {0,1,2}.
2. Detectar que quedan vértices sin visitar.
3. Ejecutar Prim desde 3 → MST de {3,4,5}.

La versión con arreglo detecta desconexión cuando `key[u] == INF` para el mínimo
extraído. La versión con heap simplemente vacía el heap sin visitar todos los vértices.
</details>

---

**Ejercicio 9**: compare los trade-offs entre lazy deletion y decrease-key para la
implementación con heap.

<details><summary>Predicción</summary>

| Aspecto | Lazy deletion | Decrease-key |
|---------|--------------|--------------|
| Complejidad | $O(m \log n)$ | $O(m \log n)$ (binary heap) |
| Espacio heap | Hasta $O(m)$ entradas | Exactamente $O(n)$ entradas |
| Implementación | Simple: push y skip | Necesita heap indexado |
| Constante | Ligeramente mayor (más pops) | Menor (menos entradas) |
| Fibonacci heap | No aplica | $O(m + n \log n)$ |

**En la práctica**, lazy deletion es preferida por su simplicidad. Decrease-key solo
vale la pena con Fibonacci heap (mejora asintótica) o cuando la memoria es crítica.
</details>

---

**Ejercicio 10**: demuestre que en cada paso de Prim, la arista agregada es la arista
más ligera que cruza el corte $(S, V \setminus S)$.

<details><summary>Predicción</summary>

**Demostración**: Sea $S$ el conjunto de vértices ya en el MST y $v$ el vértice
extraído con `key[v]` mínimo.

1. `key[v]` es el peso de la arista más ligera entre $v$ y algún vértice de $S$
   (por la actualización `key[v] = min(key[v], w(u,v))` cada vez que un vecino $u$
   entra a $S$).

2. Para cualquier otro vértice $w \notin S$, `key[w] >= key[v]` (porque $v$ fue el
   mínimo extraído). La arista más ligera de $w$ a $S$ tiene peso `key[w]`.

3. Toda arista que cruza el corte $(S, V \setminus S)$ tiene un extremo $w \notin S$,
   y su peso es $\geq$ `key[w]` $\geq$ `key[v]`.

Por lo tanto, la arista $(parent[v], v)$ con peso `key[v]` es la **arista más ligera
del corte**, y por la propiedad de corte (T01) pertenece al MST. $\blacksquare$
</details>
