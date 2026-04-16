# T02 — Kruskal

## 1. El algoritmo

El algoritmo de Kruskal construye el MST procesando **aristas en orden ascendente de
peso** y agregando cada una si no crea un ciclo:

```
KRUSKAL(G):
    sort edges by weight ascending
    init Union-Find with n singletons
    mst = []
    for each edge (u, v, w) in sorted order:
        if find(u) != find(v):        // different components
            union(u, v)
            mst.append((u, v, w))
            if |mst| == n - 1: break  // early termination
    return mst
```

La decisión "no crea ciclo" equivale a verificar que los extremos están en **componentes
distintas**, operación que Union-Find resuelve eficientemente.


## 2. Traza paso a paso

Grafo de T01 (6 vértices, 9 aristas):

```
        4           2
    0-------1-------2
    |      /|      /|
    |3   8/ |6   5/ |7
    |   /   |   /   |
    3-------4-------5
        9       1
```

Aristas ordenadas: (4,5):1, (1,2):2, (0,3):3, (0,1):4, (2,4):5, (1,4):6,
(2,5):7, (1,3):8, (3,4):9.

| Paso | Arista | find(u) | find(v) | Decisión | Componentes |
|------|--------|---------|---------|----------|-------------|
| 1 | (4,5):1 | 4 | 5 | ACCEPT | {4,5} {0} {1} {2} {3} |
| 2 | (1,2):2 | 1 | 2 | ACCEPT | {4,5} {1,2} {0} {3} |
| 3 | (0,3):3 | 0 | 3 | ACCEPT | {4,5} {1,2} {0,3} |
| 4 | (0,1):4 | 0 | 1 | ACCEPT | {4,5} {0,1,2,3} |
| 5 | (2,4):5 | 0 | 4 | ACCEPT | {0,1,2,3,4,5} |
| 6 | (1,4):6 | 0 | 0 | REJECT | — ciclo — |

En el paso 5, `find(2)` retorna 0 (la raíz de 2 es 0 tras las uniones anteriores),
y `find(4)` retorna 4. Como 0 $\neq$ 4, la arista se acepta y los 5 edges del MST
están completos. Las aristas 6-9 no se procesan (terminación temprana).

MST: peso total $= 1 + 2 + 3 + 4 + 5 = 15$.


## 3. Corrección y complejidad

### Corrección

Cada arista aceptada es la más ligera que cruza el corte entre las dos componentes que
une. Por la **propiedad de corte** (T01), esta arista pertenece a algún MST. La
invariante se mantiene en cada paso, y al final las $n - 1$ aristas forman un MST.

Equivalentemente, cada arista rechazada es la más pesada de algún ciclo (la componente
ya está conectada internamente con aristas más livianas). Por la **propiedad de ciclo**,
esta arista no pertenece a ningún MST.

### Complejidad

| Fase | Costo |
|------|-------|
| Ordenar $m$ aristas | $O(m \log m) = O(m \log n)$ |
| $m$ operaciones find + $n-1$ operaciones union | $O(m \cdot \alpha(n))$ |
| **Total** | $O(m \log n)$ |

El costo está dominado por el ordenamiento. Si las aristas llegan **pre-ordenadas**,
el algoritmo corre en $O(m \cdot \alpha(n)) \approx O(m)$.


## 4. Union-Find: representación básica

Union-Find (también llamado *Disjoint Set Union*, DSU) mantiene una partición de
$\{0, \dots, n-1\}$ en conjuntos disjuntos, soportando:

- `find(x)`: retorna el **representante** (raíz) del conjunto de $x$.
- `union(x, y)`: fusiona los conjuntos que contienen $x$ e $y$.

### Representación con arreglo de padres

Cada conjunto se representa como un **árbol enraizado**. El arreglo `parent[i]` almacena
el padre de $i$; la raíz satisface `parent[r] = r`.

```
parent: [0, 0, 1, 0, 4, 4]    // Ejemplo: dos árboles

Árbol 1:        Árbol 2:
    0               4
   / \              |
  1   3             5
  |
  2
```

**find naive**: seguir punteros hasta la raíz.

```
find(x):
    while parent[x] != x:
        x = parent[x]
    return x
```

**union naive**: enlazar la raíz de uno bajo la del otro.

```
union(a, b):
    ra = find(a), rb = find(b)
    if ra != rb: parent[ra] = rb
```

### El problema: cadenas

Con uniones arbitrarias, el árbol puede degenerar en una **cadena** de profundidad
$O(n)$: `union(0,1), union(1,2), union(2,3), ...` produce $0 \to 1 \to 2 \to \dots \to n{-}1$.
Cada `find(0)` cuesta $O(n)$, dando $O(mn)$ total para Kruskal.


## 5. Unión por rango

Para evitar cadenas, mantenemos `rank[x]` como cota superior de la altura del subárbol
enraizado en $x$. Al unir, **enlazamos el árbol de menor rango bajo el de mayor rango**:

```
union_rank(a, b):
    ra = find(a), rb = find(b)
    if ra == rb: return
    if rank[ra] < rank[rb]: swap(ra, rb)
    parent[rb] = ra
    if rank[ra] == rank[rb]: rank[ra]++
```

### Garantía de profundidad

**Lema**: un árbol con raíz de rango $r$ contiene al menos $2^r$ nodos.

*Demostración por inducción*: rango 0 tiene $\geq 1 = 2^0$ nodos. Al unir dos raíces
de rango $r$, el resultado tiene rango $r + 1$ y al menos $2^r + 2^r = 2^{r+1}$ nodos.
Si los rangos difieren, el rango no aumenta y los nodos solo crecen. $\blacksquare$

**Corolario**: $\text{rank} \leq \lfloor \log_2 n \rfloor$, por lo que `find` es
$O(\log n)$. Para Kruskal esto da $O(m \log n)$ en la fase de Union-Find — ya igualando
el costo del ordenamiento.


## 6. Compresión de caminos

Después de `find(x)`, **todos los nodos del camino** de $x$ a la raíz se redirigen
directamente a la raíz. Así, futuras consultas sobre esos nodos son $O(1)$.

```
find_pc(x):
    if parent[x] != x:
        parent[x] = find_pc(parent[x])   // recursión + compresión
    return parent[x]
```

### Ejemplo

Antes de `find(0)` — cadena $0 \to 1 \to 2 \to 3 \to 4$:

```
parent: [1, 2, 3, 4, 4]
```

Después de `find(0)` con compresión:

```
parent: [4, 4, 4, 4, 4]     // estrella: todos apuntan a raíz 4
```

El primer `find(0)` recorre 4 saltos; todos los siguientes, 1 salto.

### Variantes

| Variante | Acción | Costo amortizado |
|----------|--------|-----------------|
| **Compresión completa** | Todos a la raíz (recursivo) | $O(\alpha(n))$ con rango |
| **Path halving** | `parent[x] = parent[parent[x]]` | $O(\alpha(n))$ con rango |
| **Path splitting** | Cada nodo apunta al abuelo | $O(\alpha(n))$ con rango |

Path halving es la más práctica: sin recursión, un solo while.


## 7. Ambas optimizaciones: la función inversa de Ackermann

Con **unión por rango** y **compresión de caminos** combinadas, una secuencia de $m$
operaciones sobre $n$ elementos cuesta $O(m \cdot \alpha(n))$ amortizado, donde
$\alpha(n)$ es la **función inversa de Ackermann**.

$\alpha(n)$ crece tan lentamente que:

| $n$ | $\alpha(n)$ |
|-----|-------------|
| $1$ | $0$ |
| $2$ | $1$ |
| $\leq 2047$ | $\leq 3$ |
| $\leq 2^{2^{2^{2^{16}}}}$ | $\leq 4$ |

Para cualquier $n$ concebible en la práctica, $\alpha(n) \leq 4$. Esto hace que cada
operación Union-Find sea **efectivamente $O(1)$**.

Aplicado a Kruskal: la fase de Union-Find cuesta $O(m \cdot \alpha(n)) \approx O(m)$,
y el **cuello de botella es el ordenamiento** $O(m \log n)$.


## 8. Detalles de implementación y variantes

### Detalles prácticos

- **Self-loops**: una arista $(u, u)$ siempre tiene `find(u) == find(u)`, se rechaza
  automáticamente.
- **Aristas paralelas**: se manejan naturalmente — la de menor peso se procesa primero.
- **Terminación temprana**: tras aceptar $n - 1$ aristas, el MST está completo.
- **Detección de desconexión**: si al final `count < n - 1`, el grafo no es conexo;
  se obtuvo un bosque generador mínimo de $n - \text{count}$ componentes.

### Variantes

| Variante | Cambio | Aplicación |
|----------|--------|------------|
| **Máximo spanning tree** | Ordenar descendente | Red con máximo ancho de banda |
| **Aristas pre-ordenadas** | Omitir sort: $O(m\alpha(n))$ | Aristas de entrada ya ordenadas |
| **Kruskal parcial** | Empezar con aristas forzadas pre-unidas | MST con restricciones |
| **Kruskal online** | Agregar aristas incrementalmente | Grafos dinámicos |


## 9. Programa en C

```c
/* kruskal.c — Kruskal with Union-Find: trace, comparison, variants */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAXV 30
#define MAXE 100

typedef struct { int u, v, w; } Edge;

/* --- Optimized Union-Find (rank + path compression) --- */
static int par[MAXV], rnk[MAXV];
static long opt_ops;

void uf_init(int n) {
    for (int i = 0; i < n; i++) { par[i] = i; rnk[i] = 0; }
    opt_ops = 0;
}

int uf_find(int x) {
    if (par[x] != x) { opt_ops++; par[x] = uf_find(par[x]); }
    return par[x];
}

void uf_link(int a, int b) {   /* a, b must be roots */
    if (rnk[a] < rnk[b]) { int t = a; a = b; b = t; }
    par[b] = a;
    if (rnk[a] == rnk[b]) rnk[a]++;
}

bool uf_union(int a, int b) {
    a = uf_find(a); b = uf_find(b);
    if (a == b) return false;
    uf_link(a, b);
    return true;
}

/* --- Naive Union-Find (for comparison) --- */
static int npar[MAXV];
static long naive_ops;

void naive_init(int n) {
    for (int i = 0; i < n; i++) npar[i] = i;
    naive_ops = 0;
}

int naive_find(int x) {
    while (npar[x] != x) { naive_ops++; x = npar[x]; }
    return x;
}

bool naive_union(int a, int b) {
    a = naive_find(a); b = naive_find(b);
    if (a == b) return false;
    npar[a] = b;
    return true;
}

int cmp_asc(const void *a, const void *b) {
    return ((const Edge *)a)->w - ((const Edge *)b)->w;
}

int cmp_desc(const void *a, const void *b) {
    return ((const Edge *)b)->w - ((const Edge *)a)->w;
}

void print_parent(const int p[], int n) {
    printf("[");
    for (int i = 0; i < n; i++) printf("%d%s", p[i], i < n - 1 ? ", " : "");
    printf("]");
}

void print_children(const int p[], int n) {
    for (int i = 0; i < n; i++) {
        bool first = true;
        for (int j = 0; j < n; j++) {
            if (j != i && p[j] == i) {
                if (first) { printf("    %d -> {", i); first = false; }
                else printf(", ");
                printf("%d", j);
            }
        }
        if (!first) printf("}\n");
    }
}

/* Path compression on arbitrary array */
int find_pc(int t[], int x) {
    if (t[x] != x) t[x] = find_pc(t, t[x]);
    return t[x];
}

/* ====================================================== */
int main(void) {
    int n = 6;
    Edge edges[] = {
        {0,1,4}, {0,3,3}, {1,2,2}, {1,3,8}, {1,4,6},
        {2,4,5}, {2,5,7}, {3,4,9}, {4,5,1}
    };
    int m = 9;

    /* == Demo 1: Kruskal step-by-step trace == */
    printf("=== Demo 1: Kruskal trace ===\n");
    Edge sorted[MAXE];
    memcpy(sorted, edges, m * sizeof(Edge));
    qsort(sorted, m, sizeof(Edge), cmp_asc);

    uf_init(n);
    Edge mst[MAXE];
    int mst_cnt = 0, total = 0;

    for (int i = 0; i < m; i++) {
        int ru = uf_find(sorted[i].u);
        int rv = uf_find(sorted[i].v);
        if (ru != rv) {
            uf_link(ru, rv);
            mst[mst_cnt++] = sorted[i];
            total += sorted[i].w;
            printf("  %d. (%d,%d):%d  root(%d)=%d root(%d)=%d  ACCEPT [%d/%d]\n",
                   i + 1, sorted[i].u, sorted[i].v, sorted[i].w,
                   sorted[i].u, ru, sorted[i].v, rv, mst_cnt, n - 1);
            printf("     parent: "); print_parent(par, n); printf("\n");
            if (mst_cnt == n - 1) break;
        } else {
            printf("  %d. (%d,%d):%d  root(%d)=%d root(%d)=%d  REJECT\n",
                   i + 1, sorted[i].u, sorted[i].v, sorted[i].w,
                   sorted[i].u, ru, sorted[i].v, rv);
        }
    }
    printf("MST weight: %d, edges: %d\n", total, mst_cnt);

    /* == Demo 2: UF tree growth (balanced merges) == */
    printf("\n=== Demo 2: Union-Find tree growth ===\n");
    int n2 = 8;
    uf_init(n2);
    int unions[][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{4,6},{0,4}};
    for (int i = 0; i < 7; i++) {
        int a = unions[i][0], b = unions[i][1];
        int ra = uf_find(a), rb = uf_find(b);
        uf_link(ra, rb);
        printf("union(%d,%d): ", a, b);
        printf("parent="); print_parent(par, n2); printf("\n");
    }
    printf("Final tree:\n");
    print_children(par, n2);
    printf("Height=3, rank[0]=%d (log2(%d)=%d)\n", rnk[0], n2, 3);

    /* == Demo 3: Path compression before/after == */
    printf("\n=== Demo 3: Path compression ===\n");
    int tree[] = {1, 2, 3, 4, 4};
    int nn3 = 5;
    printf("Before find(0) — chain 0->1->2->3->4:\n  parent: ");
    print_parent(tree, nn3); printf("\n");

    find_pc(tree, 0);

    printf("After find(0) with path compression:\n  parent: ");
    print_parent(tree, nn3); printf("\n");
    printf("  All nodes now point directly to root 4\n");
    printf("  Next find(0): 1 hop instead of 4\n");

    /* == Demo 4: Naive vs optimized operation count == */
    printf("\n=== Demo 4: Naive vs optimized ===\n");
    int nn4 = 16;
    /* Naive: sequential unions create chain */
    naive_init(nn4);
    for (int i = 0; i < nn4 - 1; i++) naive_union(i, i + 1);
    /* Optimized: same sequence, rank creates star */
    uf_init(nn4);
    for (int i = 0; i < nn4 - 1; i++) uf_union(i, i + 1);

    naive_ops = 0;
    opt_ops = 0;
    int nf = 20;
    for (int i = 0; i < nf; i++) { naive_find(0); uf_find(0); }

    printf("After union(0,1), union(1,2), ..., union(14,15):\n");
    printf("  Naive tree:     chain 0->1->2->...->15 (depth 15)\n");
    printf("  Optimized tree: star around root 0 (depth 1)\n\n");
    printf("%d x find(0):\n", nf);
    printf("  Naive:     %ld pointer traversals (%.1f/find)\n",
           naive_ops, (double)naive_ops / nf);
    printf("  Optimized: %ld pointer traversals (%.1f/find)\n",
           opt_ops, (double)opt_ops / nf);

    /* == Demo 5: Maximum spanning tree == */
    printf("\n=== Demo 5: Maximum spanning tree ===\n");
    Edge desc[MAXE];
    memcpy(desc, edges, m * sizeof(Edge));
    qsort(desc, m, sizeof(Edge), cmp_desc);
    uf_init(n);
    Edge max_st[MAXE];
    int max_cnt = 0, max_total = 0;
    for (int i = 0; i < m && max_cnt < n - 1; i++) {
        if (uf_union(desc[i].u, desc[i].v)) {
            max_st[max_cnt++] = desc[i];
            max_total += desc[i].w;
        }
    }
    printf("Max spanning tree edges:\n");
    for (int i = 0; i < max_cnt; i++)
        printf("  (%d,%d):%d\n", max_st[i].u, max_st[i].v, max_st[i].w);
    printf("Total: %d (MST was %d)\n", max_total, total);

    /* == Demo 6: Spanning forest (disconnected graph) == */
    printf("\n=== Demo 6: Spanning forest ===\n");
    Edge disc_edges[] = {
        {0,1,3}, {0,2,5}, {1,2,4}, {3,4,2}, {3,5,7}, {4,5,6}
    };
    int nd = 6, md = 6;
    Edge disc_sorted[MAXE];
    memcpy(disc_sorted, disc_edges, md * sizeof(Edge));
    qsort(disc_sorted, md, sizeof(Edge), cmp_asc);
    uf_init(nd);
    Edge forest[MAXE];
    int fc = 0, ft = 0;
    for (int i = 0; i < md; i++) {
        if (uf_union(disc_sorted[i].u, disc_sorted[i].v)) {
            forest[fc++] = disc_sorted[i];
            ft += disc_sorted[i].w;
        }
    }
    printf("6 vertices, 2 disconnected components:\n");
    printf("  {0,1,2} edges: (0,1):3 (0,2):5 (1,2):4\n");
    printf("  {3,4,5} edges: (3,4):2 (3,5):7 (4,5):6\n\n");
    printf("Forest edges (%d, expected n-1=%d for connected):\n", fc, nd - 1);
    for (int i = 0; i < fc; i++)
        printf("  (%d,%d):%d\n", forest[i].u, forest[i].v, forest[i].w);
    printf("Total: %d\n", ft);
    printf("Components: %d (detected: edges %d < n-1=%d)\n", nd - fc, fc, nd - 1);

    return 0;
}
```


## 10. Programa en Rust

```rust
// kruskal.rs — Kruskal with Union-Find: trace, comparison, variants

use std::collections::HashMap;

/* --- Optimized Union-Find --- */
struct UnionFind {
    parent: Vec<usize>,
    rank: Vec<u32>,
    ops: u64,
}

impl UnionFind {
    fn new(n: usize) -> Self {
        Self { parent: (0..n).collect(), rank: vec![0; n], ops: 0 }
    }
    fn find(&mut self, x: usize) -> usize {
        if self.parent[x] != x {
            self.ops += 1;
            self.parent[x] = self.find(self.parent[x]);
        }
        self.parent[x]
    }
    fn link(&mut self, mut a: usize, mut b: usize) {
        if self.rank[a] < self.rank[b] { std::mem::swap(&mut a, &mut b); }
        self.parent[b] = a;
        if self.rank[a] == self.rank[b] { self.rank[a] += 1; }
    }
    fn union(&mut self, a: usize, b: usize) -> bool {
        let (ra, rb) = (self.find(a), self.find(b));
        if ra == rb { return false; }
        self.link(ra, rb);
        true
    }
    fn components(&self) -> usize {
        (0..self.parent.len()).filter(|&i| self.parent[i] == i).count()
    }
}

/* --- Naive Union-Find --- */
struct NaiveUF {
    parent: Vec<usize>,
    ops: u64,
}

impl NaiveUF {
    fn new(n: usize) -> Self {
        Self { parent: (0..n).collect(), ops: 0 }
    }
    fn find(&mut self, mut x: usize) -> usize {
        while self.parent[x] != x { self.ops += 1; x = self.parent[x]; }
        x
    }
    fn union(&mut self, a: usize, b: usize) -> bool {
        let (ra, rb) = (self.find(a), self.find(b));
        if ra == rb { return false; }
        self.parent[ra] = rb;
        true
    }
}

#[derive(Clone, Copy)]
struct Edge { u: usize, v: usize, w: i32 }

fn print_parent(p: &[usize]) {
    print!("[");
    for (i, v) in p.iter().enumerate() {
        if i > 0 { print!(", "); }
        print!("{}", v);
    }
    print!("]");
}

fn print_children(p: &[usize]) {
    let n = p.len();
    for i in 0..n {
        let children: Vec<usize> = (0..n).filter(|&j| j != i && p[j] == i).collect();
        if !children.is_empty() {
            println!("    {} -> {{{}}}", i,
                children.iter().map(|c| c.to_string()).collect::<Vec<_>>().join(", "));
        }
    }
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

    // == Demo 1: Kruskal trace ==
    println!("=== Demo 1: Kruskal trace ===");
    let mut sorted = edges.clone();
    sorted.sort_by_key(|e| e.w);
    let mut uf = UnionFind::new(n);
    let mut mst = Vec::new();
    let mut total = 0;

    for (i, e) in sorted.iter().enumerate() {
        let ru = uf.find(e.u);
        let rv = uf.find(e.v);
        if ru != rv {
            uf.link(ru, rv);
            mst.push(*e);
            total += e.w;
            print!("  {}. ({},{}):{}  root({})={} root({})={}  ACCEPT [{}/{}]  parent:",
                i + 1, e.u, e.v, e.w, e.u, ru, e.v, rv, mst.len(), n - 1);
            print_parent(&uf.parent[..n]);
            println!();
            if mst.len() == n - 1 { break; }
        } else {
            println!("  {}. ({},{}):{}  root({})={} root({})={}  REJECT",
                i + 1, e.u, e.v, e.w, e.u, ru, e.v, rv);
        }
    }
    println!("MST weight: {}\n", total);

    // == Demo 2: UF tree growth ==
    println!("=== Demo 2: Union-Find tree growth ===");
    let mut uf2 = UnionFind::new(8);
    let unions = [(0,1),(2,3),(4,5),(6,7),(0,2),(4,6),(0,4)];
    for (a, b) in &unions {
        let ra = uf2.find(*a);
        let rb = uf2.find(*b);
        uf2.link(ra, rb);
        print!("  union({},{}): parent=", a, b);
        print_parent(&uf2.parent);
        println!();
    }
    println!("  Final tree (height=3, rank[0]={}):", uf2.rank[0]);
    print_children(&uf2.parent);

    // == Demo 3: Path compression ==
    println!("\n=== Demo 3: Path compression ===");
    let mut chain = vec![1usize, 2, 3, 4, 4];
    print!("  Before find(0): chain 0->1->2->3->4  parent=");
    print_parent(&chain); println!();

    fn find_pc(t: &mut [usize], x: usize) -> usize {
        if t[x] != x { t[x] = find_pc(t, t[x]); }
        t[x]
    }
    find_pc(&mut chain, 0);

    print!("  After  find(0): star at root 4       parent=");
    print_parent(&chain); println!();
    println!("  Next find(0): 1 hop instead of 4");

    // == Demo 4: Naive vs optimized ==
    println!("\n=== Demo 4: Naive vs optimized ===");
    let nn = 16;
    let mut naive = NaiveUF::new(nn);
    let mut opt = UnionFind::new(nn);
    for i in 0..nn - 1 {
        naive.union(i, i + 1);
        opt.union(i, i + 1);
    }
    naive.ops = 0;
    opt.ops = 0;
    let nf = 20;
    for _ in 0..nf { naive.find(0); opt.find(0); }
    println!("  After sequential unions on {} elements:", nn);
    println!("  {} x find(0):", nf);
    println!("    Naive:     {} traversals ({:.1}/find)",
        naive.ops, naive.ops as f64 / nf as f64);
    println!("    Optimized: {} traversals ({:.1}/find)",
        opt.ops, opt.ops as f64 / nf as f64);

    // == Demo 5: Maximum spanning tree ==
    println!("\n=== Demo 5: Maximum spanning tree ===");
    let mut desc = edges.clone();
    desc.sort_by(|a, b| b.w.cmp(&a.w));
    let mut uf5 = UnionFind::new(n);
    let mut max_mst = Vec::new();
    let mut max_total = 0;
    for e in &desc {
        if uf5.union(e.u, e.v) {
            max_mst.push(*e);
            max_total += e.w;
            if max_mst.len() == n - 1 { break; }
        }
    }
    for e in &max_mst {
        println!("  ({},{}): {}", e.u, e.v, e.w);
    }
    println!("  Max ST total: {} (MST was {})", max_total, total);

    // == Demo 6: Spanning forest ==
    println!("\n=== Demo 6: Spanning forest ===");
    let disc = vec![
        Edge { u: 0, v: 1, w: 3 }, Edge { u: 0, v: 2, w: 5 },
        Edge { u: 1, v: 2, w: 4 }, Edge { u: 3, v: 4, w: 2 },
        Edge { u: 3, v: 5, w: 7 }, Edge { u: 4, v: 5, w: 6 },
    ];
    let nd = 6;
    let mut ds = disc.clone();
    ds.sort_by_key(|e| e.w);
    let mut uf6 = UnionFind::new(nd);
    let mut forest = Vec::new();
    for e in &ds {
        if uf6.union(e.u, e.v) { forest.push(*e); }
    }
    let ftot: i32 = forest.iter().map(|e| e.w).sum();
    println!("  Components: {0,1,2} and {3,4,5}");
    for e in &forest { println!("  ({},{}): {}", e.u, e.v, e.w); }
    println!("  Forest: {} edges (n-1={} for connected)", forest.len(), nd - 1);
    println!("  Detected {} components", nd - forest.len());
    println!("  Forest weight: {}", ftot);

    // == Demo 7: Verify matches T01 ==
    println!("\n=== Demo 7: Verify against T01 ===");
    let t01_mst = vec![(4,5,1), (1,2,2), (0,3,3), (0,1,4), (2,4,5)];
    let our_mst: Vec<(usize,usize,i32)> = mst.iter()
        .map(|e| (e.u.min(e.v), e.u.max(e.v), e.w)).collect();
    let t01_set: std::collections::HashSet<_> = t01_mst.iter()
        .map(|&(u,v,w)| (u.min(v), u.max(v), w)).collect();
    let our_set: std::collections::HashSet<_> = our_mst.iter().copied().collect();
    println!("  T01 MST: {:?}", t01_mst);
    println!("  Our MST: {:?}", our_mst);
    println!("  Match: {}", if t01_set == our_set { "YES" } else { "NO" });

    // == Demo 8: Connection threshold ==
    println!("\n=== Demo 8: Connection threshold ===");
    let mut uf8 = UnionFind::new(n);
    let mut components = n;
    println!("  Processing edges by weight to find when graph connects:");
    for e in &sorted {
        if uf8.union(e.u, e.v) {
            components -= 1;
            println!("    ({},{}):{}  components: {}",
                e.u, e.v, e.w, components);
            if components == 1 {
                println!("  Graph connected! Threshold weight: {}", e.w);
                println!("  (= heaviest MST edge)");
                break;
            }
        }
    }
}
```


## 11. Ejercicios

**Ejercicio 1**: aplique Kruskal paso a paso al siguiente grafo. Para cada arista
indique si se acepta o rechaza y el estado de las componentes.

```
Vértices: {0, 1, 2, 3, 4, 5}
Aristas: (0,1):4, (0,4):1, (1,2):7, (1,4):3, (2,3):6,
         (2,4):8, (2,5):2, (3,5):5, (4,5):9
```

<details><summary>Predicción</summary>

Ordenadas: (0,4):1, (2,5):2, (1,4):3, (0,1):4, (3,5):5, (2,3):6, (1,2):7, (2,4):8,
(4,5):9.

1. (0,4):1 — ACCEPT, une {0} y {4}
2. (2,5):2 — ACCEPT, une {2} y {5}
3. (1,4):3 — ACCEPT, une {1} con {0,4}
4. (0,1):4 — REJECT, 0 y 1 ya en {0,1,4} (ciclo)
5. (3,5):5 — ACCEPT, une {3} con {2,5}
6. (2,3):6 — REJECT, 2 y 3 ya en {2,3,5}
7. (1,2):7 — ACCEPT, une {0,1,4} con {2,3,5} → MST completo

MST: {(0,4):1, (2,5):2, (1,4):3, (3,5):5, (1,2):7}, peso total = 18.
Procesadas 7 de 9 aristas (2 rechazadas, 5 aceptadas).
</details>

---

**Ejercicio 2**: dibuje el árbol de Union-Find (con unión por rango) después de las
siguientes operaciones: `init(8), union(0,1), union(2,3), union(4,5), union(6,7),
union(0,2), union(4,6), union(0,4)`. Muestre el arreglo `parent[]` y `rank[]` finales.

<details><summary>Predicción</summary>

1. union(0,1): par[1]=0, rank[0]=1
2. union(2,3): par[3]=2, rank[2]=1
3. union(4,5): par[5]=4, rank[4]=1
4. union(6,7): par[7]=6, rank[6]=1
5. union(0,2): rank iguales → par[2]=0, rank[0]=2
6. union(4,6): rank iguales → par[6]=4, rank[4]=2
7. union(0,4): rank iguales → par[4]=0, rank[0]=3

```
parent: [0, 0, 0, 2, 0, 4, 4, 6]
rank:   [3, 0, 1, 0, 2, 0, 1, 0]

            0 (rank 3)
          / | \
         1  2  4 (rank 2)
            |  / \
            3 5   6 (rank 1)
                  |
                  7
```

Altura del árbol = 3 = $\lfloor \log_2 8 \rfloor$. Coincide con la garantía teórica.
</details>

---

**Ejercicio 3**: dado `parent = [1, 2, 3, 4, 4]` (cadena $0 \to 1 \to 2 \to 3 \to 4$),
muestre el estado del arreglo después de `find(0)` con compresión de caminos. ¿Cuántos
saltos requiere el siguiente `find(0)`?

<details><summary>Predicción</summary>

`find(0)` recursivo:
- find(0) → par[0]=1 → find(1) → par[1]=2 → find(2) → par[2]=3 → find(3) → par[3]=4 → find(4) = 4 (raíz)
- Al retornar: par[3]=4, par[2]=4, par[1]=4, par[0]=4

Después: `parent = [4, 4, 4, 4, 4]`. Estrella con raíz 4.

El primer find(0) requirió 4 saltos. El siguiente find(0) requiere **1 salto**
(0→4, verificar que 4 es raíz).
</details>

---

**Ejercicio 4**: ¿cuál es el rango máximo posible en un Union-Find con 10 elementos
usando unión por rango? Justifique.

<details><summary>Predicción</summary>

El lema garantiza que un árbol de rango $r$ tiene $\geq 2^r$ nodos.
Para $n = 10$: $2^3 = 8 \leq 10 < 16 = 2^4$.

El rango máximo es **3**. Se alcanza, por ejemplo, con:
union(0,1), union(2,3), union(0,2) → rango 2 con 4 nodos.
union(4,5), union(6,7), union(4,6) → rango 2 con 4 nodos.
union(0,4) → rango 3 con 8 nodos.
Luego union(8,0) y union(9,0) agregan nodos sin aumentar el rango.
</details>

---

**Ejercicio 5**: ¿por qué Kruskal ordena ascendente para MST? ¿Qué se obtiene si
se ordena descendente? Justifique usando la propiedad de corte.

<details><summary>Predicción</summary>

**Ascendente** → MST: al procesar aristas de menor a mayor, cada arista aceptada es la
más ligera que cruza el corte entre las dos componentes que une. Por la propiedad de
corte, pertenece al MST.

**Descendente** → **Maximum spanning tree**: cada arista aceptada es la más *pesada*
que cruza el corte. Por la versión dual de la propiedad de corte (la arista más pesada
de un corte pertenece al maximum ST), se obtiene el árbol generador de peso máximo.

Operativamente es idéntico: solo cambia el criterio de ordenamiento.
</details>

---

**Ejercicio 6**: considere el grafo con aristas (0,1):3, (0,2):3, (1,3):5, (2,3):5,
(1,2):6. Identifique los dos MSTs posibles y explique por qué la duplicación de pesos
permite ambos.

<details><summary>Predicción</summary>

Ordenadas: (0,1):3, (0,2):3, (1,3):5, (2,3):5, (1,2):6.

Kruskal:
1. (0,1):3 — ACCEPT
2. (0,2):3 — ACCEPT → {0,1,2}
3. (1,3):5 — ACCEPT → MST completo

MST A: {(0,1):3, (0,2):3, (1,3):5}, total = 11.

Alternativa: en paso 3, si (2,3):5 se procesa antes que (1,3):5 (ambas peso 5):
MST B: {(0,1):3, (0,2):3, (2,3):5}, total = 11.

Las aristas (1,3) y (2,3) tienen el mismo peso y ambas conectan {0,1,2} con {3}.
Kruskal puede elegir cualquiera según el desempate del sort. Multiconjunto de
ambos MSTs: {3, 3, 5} — idéntico, como garantiza el teorema.
</details>

---

**Ejercicio 7**: para el grafo del ejercicio 1, ¿cuántas aristas procesa Kruskal
con terminación temprana? ¿Y sin ella?

<details><summary>Predicción</summary>

Con terminación temprana: se detiene tras la 5.a arista aceptada. De la traza del
ejercicio 1, la 5.a aceptada es (1,2):7, que es la arista 7 en orden. Se procesan
**7 de 9** aristas.

Sin terminación temprana: se procesan las **9 aristas**. Las 2 restantes ((2,4):8
y (4,5):9) se rechazan por crear ciclos.

El ahorro de terminación temprana es modesto aquí (2 aristas), pero en grafos densos
con $m \gg n$ puede ser significativo.
</details>

---

**Ejercicio 8**: construya una cadena de 8 elementos con Union-Find naive. ¿Cuántos
saltos de puntero se realizan en 20 llamadas a `find(0)`? Compare con Union-Find
optimizado.

<details><summary>Predicción</summary>

**Naive** — cadena $0 \to 1 \to 2 \to \dots \to 7$ (profundidad 7):
Cada find(0) requiere 7 saltos. 20 finds = $20 \times 7 = 140$ saltos.

**Optimizado** (rank + PC) con la misma secuencia de unions:
El algoritmo crea una estrella con raíz 0 (rank evita la cadena). find(0) = 0 saltos
(0 es la raíz). 20 finds = **0 saltos**.

Incluso si el optimizado tuviera un árbol de altura $\log_2 8 = 3$, el primer find
comprime la ruta y los siguientes cuestan 1 salto: $3 + 19 = 22$ saltos. En ambos
casos, la mejora sobre 140 es drástica.
</details>

---

**Ejercicio 9**: aplique Kruskal al grafo desconexo con vértices {0,...,5} y aristas
(0,1):3, (0,2):5, (1,2):4, (3,4):2, (3,5):7, (4,5):6. ¿Cuántas aristas retorna?
¿Cómo se detecta la desconexión?

<details><summary>Predicción</summary>

Ordenadas: (3,4):2, (0,1):3, (1,2):4, (0,2):5, (4,5):6, (3,5):7.

1. (3,4):2 — ACCEPT
2. (0,1):3 — ACCEPT
3. (1,2):4 — ACCEPT → {0,1,2} completo
4. (0,2):5 — REJECT (ciclo)
5. (4,5):6 — ACCEPT → {3,4,5} completo
6. (3,5):7 — REJECT (ciclo)

Retorna **4 aristas** (no 5 = n-1). La condición `edges_count < n - 1` tras procesar
todas las aristas indica desconexión. Componentes: $n - \text{edges} = 6 - 4 = 2$.

El bosque generador mínimo: {(3,4):2, (0,1):3, (1,2):4, (4,5):6}, peso = 15.
</details>

---

**Ejercicio 10**: después de ejecutar Kruskal sobre un grafo conexo, la arista más
pesada del MST tiene peso $w_{\max}$. Demuestre que $w_{\max}$ es el menor valor tal
que el subgrafo inducido por aristas con peso $\leq w_{\max}$ es conexo.

<details><summary>Predicción</summary>

**Demostración**:

($\Rightarrow$) Sea $w_{\max}$ el peso de la última arista aceptada por Kruskal. Hasta
ese momento, el grafo no era conexo (faltaba una componente por unir). Al incluir esta
arista, el grafo se conecta. Todas las aristas del MST tienen peso $\leq w_{\max}$,
y el MST conecta todos los vértices. Luego el subgrafo con aristas $\leq w_{\max}$
es conexo.

($\Leftarrow$) Sea $w' < w_{\max}$. La última arista aceptada por Kruskal tiene peso
$w_{\max}$ y une dos componentes que no tenían arista de cruce con peso $< w_{\max}$
(de lo contrario, Kruskal habría procesado esa arista antes). Luego el subgrafo con
aristas $\leq w'$ no contiene ninguna arista cruzando ese corte, y no es conexo.

Por lo tanto, $w_{\max}$ es exactamente el **umbral mínimo de conexión**.
</details>
