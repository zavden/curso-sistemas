# T02 — Matriz de adyacencia

## Objetivo

Comprender la representación de grafos mediante una **matriz de adyacencia**:
cuándo es la representación adecuada, cómo implementarla en C y Rust, y
analizar sus costos en espacio y tiempo para cada operación fundamental.

---

## 1. Concepto

Dado un grafo $G = (V, E)$ con $n = |V|$ vértices numerados $0, 1, \ldots, n-1$,
la **matriz de adyacencia** es una matriz $A$ de $n \times n$ donde:

$$A[i][j] = \begin{cases} 1 & \text{si } (i,j) \in E \\ 0 & \text{si } (i,j) \notin E \end{cases}$$

Para un grafo **ponderado**, se almacena el peso en lugar de 1:

$$A[i][j] = \begin{cases} w(i,j) & \text{si } (i,j) \in E \\ \infty \text{ o } 0 & \text{si } (i,j) \notin E \end{cases}$$

El valor centinela para "sin arista" depende del contexto: $\infty$ (o
`INT_MAX`) para algoritmos de caminos más cortos, 0 para conteo o
multiplicación de matrices.

---

## 2. Ejemplos visuales

### Grafo no dirigido

```
    0 --- 1
    |   / |
    2 --- 3

    A = [ 0  1  1  0 ]
        [ 1  0  1  1 ]
        [ 1  1  0  1 ]
        [ 0  1  1  0 ]
```

Propiedad clave: en un grafo no dirigido, la matriz es **simétrica**:
$A[i][j] = A[j][i]$. Esto es porque la arista $\{i,j\}$ implica conexión
en ambas direcciones.

### Grafo dirigido

```
    0 → 1
    ↑   ↓
    2 ← 3

    A = [ 0  1  0  0 ]
        [ 0  0  0  1 ]
        [ 1  0  0  0 ]
        [ 0  0  1  0 ]
```

La fila $i$ contiene las aristas que **salen** de $i$. La columna $j$ contiene
las aristas que **llegan** a $j$. La matriz generalmente **no** es simétrica.

### Grafo ponderado

```
    0 --5-- 1
    |       |
    2       3
    |       |
    2 --7-- 3

    A = [  0   5   2   ∞ ]
        [  5   0   ∞   3 ]
        [  2   ∞   0   7 ]
        [  ∞   3   7   0 ]
```

La diagonal es 0 (distancia de un vértice a sí mismo) en grafos de caminos
más cortos, o puede ser un peso de self-loop si el grafo los permite.

---

## 3. Implementación en C

La forma más directa es un array 2D estático o un array dinámico
asignado en el heap.

### Array 2D estático

```c
#define MAX_V 100

typedef struct {
    int mat[MAX_V][MAX_V];
    int n;
    bool directed;
} AdjMatrix;
```

Ventaja: simple, sin `malloc`. Desventaja: desperdicia memoria si $n \ll$
`MAX_V` (reserva siempre $100 \times 100 = 10000$ ints).

### Array dinámico (flat)

Para evitar el desperdicio, se puede usar un array 1D de tamaño $n^2$:

```c
typedef struct {
    int *data;    // flat array of n*n elements
    int n;
    bool directed;
} AdjMatrix;

AdjMatrix am_create(int n, bool directed) {
    AdjMatrix m;
    m.n = n;
    m.directed = directed;
    m.data = calloc(n * n, sizeof(int));  // all zeros
    return m;
}

// Access: mat[i][j] = data[i * n + j]
static inline int am_get(const AdjMatrix *m, int i, int j) {
    return m->data[i * m->n + j];
}

static inline void am_set(AdjMatrix *m, int i, int j, int val) {
    m->data[i * m->n + j] = val;
}
```

El layout flat (`data[i * n + j]`) es preferible al array de punteros
(`int **data`) porque:

1. **Una sola asignación** de memoria (un `calloc` vs $n + 1$ `malloc`s).
2. **Contigüidad**: todas las filas están contiguas en memoria, lo que mejora
   la localidad de caché cuando se recorre por filas.
3. **Liberación simple**: un solo `free(m->data)`.

### Array de punteros (`int **`)

Más flexible pero menos eficiente:

```c
int **mat = malloc(n * sizeof(int *));
for (int i = 0; i < n; i++)
    mat[i] = calloc(n, sizeof(int));
```

Cada fila puede estar en una posición diferente del heap → peor localidad
de caché. Además requiere $n + 1$ asignaciones y $n + 1$ liberaciones.
Usar solo cuando se necesiten filas de tamaño variable (raro para matrices
de adyacencia).

---

## 4. Operaciones y complejidad

| Operación | Complejidad | Cómo |
|-----------|:-----------:|------|
| ¿Existe arista $(i,j)$? | $O(1)$ | `A[i][j] != 0` |
| Agregar arista $(i,j)$ | $O(1)$ | `A[i][j] = 1` (+ `A[j][i]` si no dirigido) |
| Eliminar arista $(i,j)$ | $O(1)$ | `A[i][j] = 0` (+ `A[j][i]` si no dirigido) |
| Listar vecinos de $i$ | $O(n)$ | Recorrer fila $i$, filtrar no-ceros |
| Grado de $i$ | $O(n)$ | Sumar fila $i$ (no dirigido) |
| In-degree de $i$ | $O(n)$ | Sumar columna $i$ (dirigido) |
| Out-degree de $i$ | $O(n)$ | Sumar fila $i$ (dirigido) |
| Contar aristas | $O(n^2)$ | Recorrer toda la matriz |
| Espacio total | $O(n^2)$ | Siempre, independiente de $m$ |

La gran ventaja es **verificar aristas en $O(1)$**. La desventaja principal
es **listar vecinos en $O(n)$** incluso si el vértice tiene pocos vecinos, y
el **espacio $O(n^2)$** que no depende de cuántas aristas existan.

---

## 5. ¿Cuándo usar matriz de adyacencia?

### Ideal para grafos densos

Un grafo es **denso** cuando $m = \Theta(n^2)$ — es decir, tiene un número de
aristas proporcional al máximo posible. En ese caso, la matriz de adyacencia
no desperdicia espacio significativo porque la mayoría de las celdas son
no-cero.

Umbral práctico: si la densidad $d = \frac{2m}{n(n-1)} > 0.1$ (más del 10%
de las aristas posibles), la matriz es competitiva. Si $d > 0.5$, es
claramente preferible.

### Ideal para ciertos algoritmos

Varios algoritmos clásicos operan directamente sobre la matriz:

- **Floyd-Warshall** (S03/T03): caminos más cortos entre todos los pares,
  usa la matriz como estructura principal. Complejidad $O(n^3)$, que ya es
  proporcional al tamaño de la matriz.
- **Detección de aristas**: verificar si una arista específica existe en
  $O(1)$, útil en problemas como coloreo de grafos o verificación de cliques.
- **Multiplicación de matrices**: $A^k[i][j]$ cuenta el número de caminos de
  longitud $k$ de $i$ a $j$. Esto permite encontrar caminos de longitud
  exacta usando exponenciación de matrices.
- **Grafo complemento**: el complemento $\bar{G}$ tiene arista $(i,j)$ cuando
  $G$ no la tiene: $\bar{A}[i][j] = 1 - A[i][j]$. Trivial con matriz,
  costoso con lista.

### No usar cuando

- El grafo es disperso ($m \ll n^2$): desperdicio masivo de memoria. Un grafo
  de $10^5$ vértices con $2 \times 10^5$ aristas necesitaría
  $10^{10}$ celdas (≈ 40 GB para `int`) vs $\sim 10^5$ nodos de lista.
- Se necesitan recorridos frecuentes de vecinos (BFS, DFS): cada "listar
  vecinos" es $O(n)$ vs $O(\deg(v))$ con lista.
- $n$ es muy grande: la matriz ni siquiera cabe en memoria.

---

## 6. Propiedades algebraicas

La matriz de adyacencia tiene propiedades matemáticas útiles:

### Simetría

En un grafo no dirigido, $A = A^T$. Esto permite almacenar solo la mitad
triangular superior (o inferior), reduciendo el espacio a $\frac{n(n+1)}{2}$:

```c
// Triangular storage: only store i <= j
// Index in flat array: i * n - i*(i-1)/2 + (j - i)
// But simpler: just use full matrix unless memory is critical
```

En la práctica, la optimización triangular rara vez vale la complejidad
adicional del código, excepto para $n$ muy grande.

### Potencias de la matriz

$A^k[i][j]$ = número de caminos de longitud exactamente $k$ de $i$ a $j$.

Para $k = 2$: $(A^2)[i][j] = \sum_{r=0}^{n-1} A[i][r] \cdot A[r][j]$, que
cuenta cuántos vértices $r$ existen tales que hay arista $i \to r$ y $r \to j$.

Esto permite responder preguntas como "¿están $i$ y $j$ a distancia 2?" en
$O(n)$ (una fila de $A^2$) o "¿cuántos triángulos contiene el grafo?" en
$O(n^3)$ (la traza de $A^3$ dividida entre 6).

### Valores propios

El **espectro** del grafo (eigenvalores de $A$) codifica propiedades
estructurales:

- El eigenvalor máximo $\lambda_1$ está relacionado con el grado máximo.
- El segundo eigenvalor $\lambda_2$ mide la conectividad (expander graphs).
- La traza de $A^k$ cuenta caminos cerrados de longitud $k$.

Esto pertenece al campo de la **teoría espectral de grafos**, que va más
allá del alcance de este curso pero es fundamental en machine learning sobre
grafos (Graph Neural Networks).

---

## 7. Variantes de almacenamiento

### Matriz de bits

Si el grafo no es ponderado, cada celda es 0 o 1. Se puede usar un **bit**
por celda en lugar de un `int` (32 bits), reduciendo el espacio por un
factor de 32:

```c
#include <stdint.h>

typedef struct {
    uint64_t *bits;  // packed bits
    int n;
} BitMatrix;

BitMatrix bm_create(int n) {
    BitMatrix bm;
    bm.n = n;
    int words_per_row = (n + 63) / 64;
    bm.bits = calloc(n * words_per_row, sizeof(uint64_t));
    return bm;
}

static inline void bm_set(BitMatrix *bm, int i, int j) {
    int words_per_row = (bm->n + 63) / 64;
    bm->bits[i * words_per_row + j / 64] |= (1ULL << (j % 64));
}

static inline bool bm_get(const BitMatrix *bm, int i, int j) {
    int words_per_row = (bm->n + 63) / 64;
    return (bm->bits[i * words_per_row + j / 64] >> (j % 64)) & 1;
}
```

Para $n = 10000$: matriz de `int` = 400 MB, matriz de bits ≈ 12.5 MB.

Ventaja adicional: operaciones de conjuntos sobre vecinos se pueden hacer con
operaciones bitwise (`AND` para intersección de vecindarios, `OR` para unión,
`POPCOUNT` para grado).

### Matriz triangular empaquetada

Para grafos no dirigidos sin self-loops, almacenar solo $\frac{n(n-1)}{2}$
elementos:

```c
// Map (i, j) with i < j to flat index:
// index = i * n - i*(i+1)/2 + (j - i - 1)
int tri_index(int i, int j, int n) {
    if (i > j) { int tmp = i; i = j; j = tmp; }
    return i * n - i * (i + 1) / 2 + (j - i - 1);
}
```

### Matriz dispersa (CSR)

Para grafos que son "medio densos" (ni muy densos ni muy dispersos), el
formato **Compressed Sparse Row** (CSR) almacena solo los no-ceros:

- `values[]`: pesos de las aristas (o 1s).
- `col_idx[]`: columna de cada no-cero.
- `row_ptr[]`: inicio de cada fila en `values[]`.

Espacio: $O(n + m)$ — como una lista de adyacencia pero en arrays contiguos.
CSR se usa extensivamente en álgebra lineal dispersa (BLAS) y en frameworks
de grafos (GraphBLAS). Es la representación interna de muchas bibliotecas de
análisis de redes.

---

## 8. Programa completo en C

```c
/*
 * adj_matrix.c
 *
 * Adjacency matrix implementation with demonstrations of key
 * operations: edge queries, degree, matrix power, complement,
 * and algorithm suitability.
 *
 * Compile: gcc -O2 -o adj_matrix adj_matrix.c
 * Run:     ./adj_matrix
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

/* ===================== ADJACENCY MATRIX =========================== */

typedef struct {
    int *data;
    int n;
    bool directed;
    char *labels;  /* optional vertex labels */
} AdjMatrix;

AdjMatrix am_create(int n, bool directed, const char *labels) {
    AdjMatrix m;
    m.n = n;
    m.directed = directed;
    m.data = calloc(n * n, sizeof(int));
    m.labels = malloc(n);
    if (labels) {
        memcpy(m.labels, labels, n);
    } else {
        for (int i = 0; i < n; i++)
            m.labels[i] = '0' + i;
    }
    return m;
}

void am_free(AdjMatrix *m) {
    free(m->data);
    free(m->labels);
    m->data = NULL;
    m->labels = NULL;
}

static inline int am_get(const AdjMatrix *m, int i, int j) {
    return m->data[i * m->n + j];
}

static inline void am_set(AdjMatrix *m, int i, int j, int val) {
    m->data[i * m->n + j] = val;
}

void am_add_edge(AdjMatrix *m, int u, int v, int weight) {
    am_set(m, u, v, weight);
    if (!m->directed)
        am_set(m, v, u, weight);
}

void am_remove_edge(AdjMatrix *m, int u, int v) {
    am_set(m, u, v, 0);
    if (!m->directed)
        am_set(m, v, u, 0);
}

bool am_has_edge(const AdjMatrix *m, int u, int v) {
    return am_get(m, u, v) != 0;
}

void am_print(const AdjMatrix *m) {
    printf("     ");
    for (int j = 0; j < m->n; j++)
        printf(" %c  ", m->labels[j]);
    printf("\n");

    for (int i = 0; i < m->n; i++) {
        printf("  %c [", m->labels[i]);
        for (int j = 0; j < m->n; j++) {
            int val = am_get(m, i, j);
            if (j > 0) printf(" ");
            if (val == INT_MAX)
                printf(" ∞ ");
            else
                printf("%2d ", val);
        }
        printf("]\n");
    }
}

/* ========================== DEMOS ================================= */

/*
 * Demo 1: Basic operations — create, add/remove edges, query
 */
void demo_basic_operations(void) {
    printf("=== Demo 1: Basic Operations ===\n\n");

    AdjMatrix g = am_create(5, false, "ABCDE");

    printf("Empty graph (5 vertices):\n");
    am_print(&g);

    /* Add edges */
    am_add_edge(&g, 0, 1, 1);  /* A-B */
    am_add_edge(&g, 0, 2, 1);  /* A-C */
    am_add_edge(&g, 1, 2, 1);  /* B-C */
    am_add_edge(&g, 1, 3, 1);  /* B-D */
    am_add_edge(&g, 3, 4, 1);  /* D-E */

    printf("\nAfter adding edges A-B, A-C, B-C, B-D, D-E:\n");
    am_print(&g);

    /* Query edges */
    printf("\nEdge queries:\n");
    printf("  A-B? %s\n", am_has_edge(&g, 0, 1) ? "YES" : "NO");
    printf("  A-D? %s\n", am_has_edge(&g, 0, 3) ? "YES" : "NO");
    printf("  D-E? %s\n", am_has_edge(&g, 3, 4) ? "YES" : "NO");
    printf("  C-E? %s\n", am_has_edge(&g, 2, 4) ? "YES" : "NO");

    /* Remove edge */
    am_remove_edge(&g, 1, 2);  /* Remove B-C */
    printf("\nAfter removing B-C:\n");
    am_print(&g);

    /* Verify symmetry */
    printf("\nSymmetry check (undirected):\n");
    bool symmetric = true;
    for (int i = 0; i < g.n && symmetric; i++)
        for (int j = 0; j < g.n && symmetric; j++)
            if (am_get(&g, i, j) != am_get(&g, j, i))
                symmetric = false;
    printf("  Matrix is symmetric: %s\n\n", symmetric ? "YES" : "NO");

    am_free(&g);
}

/*
 * Demo 2: Directed graph — in/out degree from matrix
 */
void demo_directed(void) {
    printf("=== Demo 2: Directed Graph Degrees ===\n\n");

    AdjMatrix g = am_create(5, true, "ABCDE");
    am_add_edge(&g, 0, 1, 1);  /* A→B */
    am_add_edge(&g, 0, 2, 1);  /* A→C */
    am_add_edge(&g, 1, 3, 1);  /* B→D */
    am_add_edge(&g, 2, 1, 1);  /* C→B */
    am_add_edge(&g, 2, 3, 1);  /* C→D */
    am_add_edge(&g, 3, 4, 1);  /* D→E */

    printf("Directed graph:\n");
    am_print(&g);

    printf("\n%-8s %-10s %-10s %-10s\n", "Vertex", "out-deg", "in-deg", "Role");
    for (int i = 0; i < g.n; i++) {
        int out = 0, in = 0;
        for (int j = 0; j < g.n; j++) {
            if (am_get(&g, i, j)) out++;   /* row sum = out-degree */
            if (am_get(&g, j, i)) in++;    /* column sum = in-degree */
        }
        const char *role = "";
        if (in == 0) role = "source";
        if (out == 0) role = "sink";
        printf("%-8c %-10d %-10d %s\n", g.labels[i], out, in, role);
    }
    printf("\n");

    am_free(&g);
}

/*
 * Demo 3: Weighted graph — Floyd-Warshall preview
 */
void demo_weighted(void) {
    printf("=== Demo 3: Weighted Graph ===\n\n");

    AdjMatrix g = am_create(4, false, "ABCD");

    /* Initialize with "infinity" for non-edges */
    for (int i = 0; i < g.n; i++)
        for (int j = 0; j < g.n; j++)
            am_set(&g, i, j, i == j ? 0 : INT_MAX);

    am_add_edge(&g, 0, 1, 5);   /* A-B: 5 */
    am_add_edge(&g, 0, 2, 2);   /* A-C: 2 */
    am_add_edge(&g, 1, 2, 8);   /* B-C: 8 */
    am_add_edge(&g, 1, 3, 3);   /* B-D: 3 */
    am_add_edge(&g, 2, 3, 7);   /* C-D: 7 */

    printf("Weighted adjacency matrix:\n");
    am_print(&g);

    /* List neighbors with weights */
    printf("\nNeighbors of each vertex:\n");
    for (int i = 0; i < g.n; i++) {
        printf("  %c:", g.labels[i]);
        for (int j = 0; j < g.n; j++) {
            int w = am_get(&g, i, j);
            if (w != 0 && w != INT_MAX)
                printf(" %c(w=%d)", g.labels[j], w);
        }
        printf("\n");
    }

    printf("\nThis matrix is ready for Floyd-Warshall (S03/T03).\n\n");

    am_free(&g);
}

/*
 * Demo 4: Matrix power — counting paths of length k
 */
void demo_matrix_power(void) {
    printf("=== Demo 4: Matrix Power — Paths of Length k ===\n\n");

    /*  0 → 1 → 3
     *  ↓ ↗ ↓
     *  2   4
     *  ↓
     *  3
     */
    AdjMatrix g = am_create(5, true, "01234");
    am_add_edge(&g, 0, 1, 1);
    am_add_edge(&g, 0, 2, 1);
    am_add_edge(&g, 1, 3, 1);
    am_add_edge(&g, 1, 4, 1);
    am_add_edge(&g, 2, 1, 1);
    am_add_edge(&g, 2, 3, 1);

    printf("Directed graph A:\n");
    am_print(&g);

    /* Compute A^2 */
    int n = g.n;
    int *a2 = calloc(n * n, sizeof(int));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            for (int r = 0; r < n; r++)
                a2[i * n + j] += am_get(&g, i, r) * am_get(&g, r, j);

    printf("\nA² (paths of length 2):\n");
    printf("     ");
    for (int j = 0; j < n; j++) printf(" %c  ", g.labels[j]);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  %c [", g.labels[i]);
        for (int j = 0; j < n; j++) {
            if (j > 0) printf(" ");
            printf("%2d ", a2[i * n + j]);
        }
        printf("]\n");
    }

    printf("\nInterpretation:\n");
    printf("  A²[0][3] = %d → %d path(s) of length 2 from 0 to 3\n",
           a2[0 * n + 3], a2[0 * n + 3]);
    printf("    Paths: 0→1→3, 0→2→3\n");
    printf("  A²[0][4] = %d → %d path(s) of length 2 from 0 to 4\n",
           a2[0 * n + 4], a2[0 * n + 4]);
    printf("    Paths: 0→1→4\n");
    printf("  A²[0][1] = %d → %d path(s) of length 2 from 0 to 1\n",
           a2[0 * n + 1], a2[0 * n + 1]);
    printf("    Paths: 0→2→1\n\n");

    free(a2);
    am_free(&g);
}

/*
 * Demo 5: Graph complement
 */
void demo_complement(void) {
    printf("=== Demo 5: Graph Complement ===\n\n");

    AdjMatrix g = am_create(4, false, "ABCD");
    am_add_edge(&g, 0, 1, 1);  /* A-B */
    am_add_edge(&g, 1, 2, 1);  /* B-C */
    am_add_edge(&g, 2, 3, 1);  /* C-D */

    printf("Original graph G (path A-B-C-D):\n");
    am_print(&g);

    /* Build complement */
    AdjMatrix comp = am_create(4, false, "ABCD");
    for (int i = 0; i < g.n; i++)
        for (int j = 0; j < g.n; j++)
            if (i != j)
                am_set(&comp, i, j, am_get(&g, i, j) ? 0 : 1);

    printf("\nComplement G̅ (edges that G doesn't have):\n");
    am_print(&comp);

    int edges_g = 0, edges_comp = 0;
    for (int i = 0; i < g.n; i++)
        for (int j = i + 1; j < g.n; j++) {
            if (am_get(&g, i, j)) edges_g++;
            if (am_get(&comp, i, j)) edges_comp++;
        }

    printf("\n|E(G)| = %d, |E(G̅)| = %d, sum = %d = C(%d,2) = %d\n\n",
           edges_g, edges_comp, edges_g + edges_comp,
           g.n, g.n * (g.n - 1) / 2);

    am_free(&g);
    am_free(&comp);
}

/*
 * Demo 6: Space comparison — matrix vs list for different densities
 */
void demo_space_comparison(void) {
    printf("=== Demo 6: Space Comparison ===\n\n");

    printf("%-8s  %-12s  %-15s  %-15s  %-10s\n",
           "n", "m (edges)", "Matrix (bytes)", "List (bytes)", "Winner");
    printf("%s\n", "--------------------------------------------------------------------");

    struct { int n; int m; } cases[] = {
        {10, 45},       /* K10 complete */
        {10, 15},       /* medium density */
        {100, 200},     /* sparse */
        {100, 4950},    /* K100 complete */
        {1000, 3000},   /* sparse */
        {1000, 100000}, /* medium */
    };

    for (int c = 0; c < 6; c++) {
        int n = cases[c].n;
        int m = cases[c].m;

        /* Matrix: n*n ints */
        long mat_bytes = (long)n * n * sizeof(int);

        /* Adjacency list: n pointers + 2m nodes (each node ~12 bytes) */
        long list_bytes = (long)n * sizeof(void *) + 2L * m * 12;
        /* For directed: m nodes instead of 2m */

        const char *winner = mat_bytes <= list_bytes ? "Matrix" : "List";

        printf("%-8d  %-12d  %-15ld  %-15ld  %s\n",
               n, m, mat_bytes, list_bytes, winner);
    }

    printf("\nRule of thumb: matrix wins when m > n²/8 (density > ~25%%)\n\n");
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Adjacency Matrix — C Demonstrations                ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_basic_operations();
    demo_directed();
    demo_weighted();
    demo_matrix_power();
    demo_complement();
    demo_space_comparison();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Matrix representation fully demonstrated!          ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * adj_matrix.rs
 *
 * Adjacency matrix implementation in Rust with demonstrations.
 *
 * Run: cargo run --release
 */

use std::fmt;

/// Adjacency matrix stored as flat Vec<i32>
struct AdjMatrix {
    data: Vec<i32>,
    n: usize,
    directed: bool,
    labels: Vec<char>,
}

impl AdjMatrix {
    fn new(labels: &[char], directed: bool) -> Self {
        let n = labels.len();
        AdjMatrix {
            data: vec![0; n * n],
            n,
            directed,
            labels: labels.to_vec(),
        }
    }

    fn get(&self, i: usize, j: usize) -> i32 {
        self.data[i * self.n + j]
    }

    fn set(&mut self, i: usize, j: usize, val: i32) {
        self.data[i * self.n + j] = val;
    }

    fn add_edge(&mut self, u: usize, v: usize, weight: i32) {
        self.set(u, v, weight);
        if !self.directed {
            self.set(v, u, weight);
        }
    }

    fn remove_edge(&mut self, u: usize, v: usize) {
        self.set(u, v, 0);
        if !self.directed {
            self.set(v, u, 0);
        }
    }

    fn has_edge(&self, u: usize, v: usize) -> bool {
        self.get(u, v) != 0
    }

    fn neighbors(&self, u: usize) -> Vec<(usize, i32)> {
        (0..self.n)
            .filter(|&j| {
                let w = self.get(u, j);
                w != 0 && w != i32::MAX
            })
            .map(|j| (j, self.get(u, j)))
            .collect()
    }

    fn out_degree(&self, u: usize) -> usize {
        (0..self.n).filter(|&j| self.has_edge(u, j)).count()
    }

    fn in_degree(&self, u: usize) -> usize {
        (0..self.n).filter(|&i| self.has_edge(i, u)).count()
    }

    fn edge_count(&self) -> usize {
        let total: usize = (0..self.n)
            .flat_map(|i| (0..self.n).map(move |j| (i, j)))
            .filter(|&(i, j)| self.has_edge(i, j))
            .count();
        if self.directed { total } else { total / 2 }
    }

    /// Matrix multiply: self * other → new matrix
    fn mat_mul(&self, other: &AdjMatrix) -> AdjMatrix {
        assert_eq!(self.n, other.n);
        let mut result = AdjMatrix::new(&self.labels, self.directed);
        for i in 0..self.n {
            for j in 0..self.n {
                let mut sum = 0i32;
                for r in 0..self.n {
                    sum += self.get(i, r) * other.get(r, j);
                }
                result.set(i, j, sum);
            }
        }
        result
    }

    /// Graph complement (for unweighted graphs)
    fn complement(&self) -> AdjMatrix {
        let mut comp = AdjMatrix::new(&self.labels, self.directed);
        for i in 0..self.n {
            for j in 0..self.n {
                if i != j {
                    comp.set(i, j, if self.has_edge(i, j) { 0 } else { 1 });
                }
            }
        }
        comp
    }

    fn is_symmetric(&self) -> bool {
        for i in 0..self.n {
            for j in (i + 1)..self.n {
                if self.get(i, j) != self.get(j, i) {
                    return false;
                }
            }
        }
        true
    }
}

impl fmt::Display for AdjMatrix {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "     ")?;
        for j in 0..self.n {
            write!(f, " {}  ", self.labels[j])?;
        }
        writeln!(f)?;
        for i in 0..self.n {
            write!(f, "  {} [", self.labels[i])?;
            for j in 0..self.n {
                let val = self.get(i, j);
                if j > 0 {
                    write!(f, " ")?;
                }
                if val == i32::MAX {
                    write!(f, " ∞ ")?;
                } else {
                    write!(f, "{:2} ", val)?;
                }
            }
            writeln!(f, "]")?;
        }
        Ok(())
    }
}

// ── Demo 1: Basic operations ─────────────────────────────────────────

fn demo_basic_operations() {
    println!("=== Demo 1: Basic Operations ===\n");

    let mut g = AdjMatrix::new(&['A', 'B', 'C', 'D', 'E'], false);

    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(1, 2, 1); // B-C
    g.add_edge(1, 3, 1); // B-D
    g.add_edge(3, 4, 1); // D-E

    println!("Undirected graph:");
    println!("{g}");

    println!("Edge queries:");
    for &(u, v) in &[(0, 1), (0, 3), (3, 4), (2, 4)] {
        println!(
            "  {}-{}? {}",
            g.labels[u], g.labels[v],
            if g.has_edge(u, v) { "YES" } else { "NO" }
        );
    }

    println!("\nNeighbors:");
    for i in 0..g.n {
        let nbrs: Vec<char> = g.neighbors(i).iter().map(|&(j, _)| g.labels[j]).collect();
        println!("  {} → {:?} (degree {})", g.labels[i], nbrs, g.out_degree(i));
    }

    g.remove_edge(1, 2); // Remove B-C
    println!("\nAfter removing B-C:");
    println!("{g}");
    println!("Symmetric: {}", g.is_symmetric());
    println!("Total edges: {}\n", g.edge_count());
}

// ── Demo 2: Directed graph ───────────────────────────────────────────

fn demo_directed() {
    println!("=== Demo 2: Directed Graph Degrees ===\n");

    let mut g = AdjMatrix::new(&['A', 'B', 'C', 'D', 'E'], true);
    g.add_edge(0, 1, 1); // A→B
    g.add_edge(0, 2, 1); // A→C
    g.add_edge(1, 3, 1); // B→D
    g.add_edge(2, 1, 1); // C→B
    g.add_edge(2, 3, 1); // C→D
    g.add_edge(3, 4, 1); // D→E

    println!("Directed graph:");
    println!("{g}");

    println!("{:<8} {:<10} {:<10} Role", "Vertex", "out-deg", "in-deg");
    for i in 0..g.n {
        let out = g.out_degree(i);
        let ind = g.in_degree(i);
        let role = match (ind, out) {
            (0, _) => "source",
            (_, 0) => "sink",
            _ => "",
        };
        println!("{:<8} {:<10} {:<10} {}", g.labels[i], out, ind, role);
    }

    println!("\nSymmetric: {} (expected false for digraph)\n", g.is_symmetric());
}

// ── Demo 3: Weighted graph ───────────────────────────────────────────

fn demo_weighted() {
    println!("=== Demo 3: Weighted Graph ===\n");

    let mut g = AdjMatrix::new(&['A', 'B', 'C', 'D'], false);

    // Initialize with infinity
    for i in 0..g.n {
        for j in 0..g.n {
            g.set(i, j, if i == j { 0 } else { i32::MAX });
        }
    }

    g.add_edge(0, 1, 5); // A-B: 5
    g.add_edge(0, 2, 2); // A-C: 2
    g.add_edge(1, 2, 8); // B-C: 8
    g.add_edge(1, 3, 3); // B-D: 3
    g.add_edge(2, 3, 7); // C-D: 7

    println!("Weighted adjacency matrix:");
    println!("{g}");

    println!("Neighbors with weights:");
    for i in 0..g.n {
        let nbrs: Vec<String> = g.neighbors(i)
            .iter()
            .map(|&(j, w)| format!("{}(w={})", g.labels[j], w))
            .collect();
        println!("  {}: {}", g.labels[i], nbrs.join(", "));
    }
    println!();
}

// ── Demo 4: Matrix power — paths of length k ────────────────────────

fn demo_matrix_power() {
    println!("=== Demo 4: Matrix Power — Paths of Length k ===\n");

    let mut g = AdjMatrix::new(&['0', '1', '2', '3', '4'], true);
    g.add_edge(0, 1, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(1, 4, 1);
    g.add_edge(2, 1, 1);
    g.add_edge(2, 3, 1);

    println!("Directed graph A:");
    println!("{g}");

    let a2 = g.mat_mul(&g);
    println!("A² (paths of length 2):");
    println!("{a2}");

    println!("Interpretation:");
    println!(
        "  A²[0][3] = {} → path(s) of length 2 from 0 to 3",
        a2.get(0, 3)
    );
    println!("    Paths: 0→1→3, 0→2→3");
    println!(
        "  A²[0][4] = {} → path(s) of length 2 from 0 to 4",
        a2.get(0, 4)
    );
    println!("    Paths: 0→1→4");

    let a3 = a2.mat_mul(&g);
    println!("\nA³ (paths of length 3):");
    println!("{a3}");

    println!(
        "  A³[0][3] = {} → path(s) of length 3 from 0 to 3",
        a3.get(0, 3)
    );
    println!("    e.g., 0→2→1→3\n");
}

// ── Demo 5: Complement graph ─────────────────────────────────────────

fn demo_complement() {
    println!("=== Demo 5: Graph Complement ===\n");

    let mut g = AdjMatrix::new(&['A', 'B', 'C', 'D'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(1, 2, 1); // B-C
    g.add_edge(2, 3, 1); // C-D

    println!("Original graph G (path A-B-C-D):");
    println!("{g}");

    let comp = g.complement();
    println!("Complement G̅:");
    println!("{comp}");

    let edges_g = g.edge_count();
    let edges_comp = comp.edge_count();
    let n = g.n;
    println!(
        "|E(G)| = {}, |E(G̅)| = {}, sum = {} = C({},2) = {}\n",
        edges_g,
        edges_comp,
        edges_g + edges_comp,
        n,
        n * (n - 1) / 2
    );
}

// ── Demo 6: BFS on adjacency matrix ─────────────────────────────────

fn demo_bfs_on_matrix() {
    println!("=== Demo 6: BFS on Adjacency Matrix ===\n");

    let mut g = AdjMatrix::new(&['A', 'B', 'C', 'D', 'E', 'F'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(1, 3, 1); // B-D
    g.add_edge(1, 4, 1); // B-E
    g.add_edge(2, 4, 1); // C-E
    g.add_edge(3, 5, 1); // D-F
    g.add_edge(4, 5, 1); // E-F

    println!("Graph:");
    println!("{g}");

    // BFS from vertex 0 (A)
    let mut visited = vec![false; g.n];
    let mut dist = vec![-1i32; g.n];
    let mut queue = std::collections::VecDeque::new();

    visited[0] = true;
    dist[0] = 0;
    queue.push_back(0);

    println!("BFS from A:");
    while let Some(u) = queue.pop_front() {
        print!("  Visit {} (dist={}), neighbors: [", g.labels[u], dist[u]);
        let mut first = true;
        // Note: O(n) to find neighbors — this is the matrix disadvantage
        for v in 0..g.n {
            if g.has_edge(u, v) {
                if !first { print!(", "); }
                print!("{}", g.labels[v]);
                first = false;
                if !visited[v] {
                    visited[v] = true;
                    dist[v] = dist[u] + 1;
                    queue.push_back(v);
                }
            }
        }
        println!("]");
    }

    println!("\nDistances from A:");
    for i in 0..g.n {
        println!("  {} → {} = {}", g.labels[0], g.labels[i], dist[i]);
    }

    println!("\nNote: each 'find neighbors' step is O(n) with matrix,");
    println!("vs O(deg(v)) with adjacency list.\n");
}

// ── Demo 7: Space analysis ──────────────────────────────────────────

fn demo_space_analysis() {
    println!("=== Demo 7: Space Analysis ===\n");

    let cases: Vec<(usize, usize, &str)> = vec![
        (10, 45, "K10 (complete)"),
        (10, 15, "medium density"),
        (100, 200, "sparse"),
        (100, 4950, "K100 (complete)"),
        (1000, 3000, "sparse"),
        (1000, 100_000, "medium"),
    ];

    println!(
        "{:<22} {:>6} {:>8} {:>14} {:>14} {:>8}",
        "Description", "n", "m", "Matrix (B)", "List (B)", "Winner"
    );
    println!("{}", "-".repeat(76));

    for (n, m, desc) in &cases {
        let mat_bytes = n * n * 4; // i32
        // List: n Vec headers (~24B each) + 2*m edges (~16B each for (usize, i32))
        let list_bytes = n * 24 + 2 * m * 16;

        let winner = if mat_bytes <= list_bytes { "Matrix" } else { "List" };
        let density = 2.0 * *m as f64 / (*n as f64 * (*n as f64 - 1.0));

        println!(
            "{:<22} {:>6} {:>8} {:>14} {:>14} {:>8}  (d={:.3})",
            desc, n, m, mat_bytes, list_bytes, winner, density
        );
    }
    println!();
}

// ── Demo 8: Triangular storage ──────────────────────────────────────

fn demo_triangular() {
    println!("=== Demo 8: Triangular Storage (Undirected) ===\n");

    let n = 5;
    let labels = ['A', 'B', 'C', 'D', 'E'];
    let tri_size = n * (n - 1) / 2; // 10 elements for n=5
    let mut tri = vec![0i32; tri_size];

    // Map (i, j) with i < j to flat index
    let tri_index = |i: usize, j: usize| -> usize {
        let (i, j) = if i < j { (i, j) } else { (j, i) };
        i * n - i * (i + 1) / 2 + (j - i - 1)
    };

    // Add edges
    let edges = [(0, 1), (0, 2), (1, 3), (2, 3), (3, 4)];
    for &(u, v) in &edges {
        tri[tri_index(u, v)] = 1;
    }

    println!("Edges: {:?}", edges);
    println!("Triangular storage ({} elements vs {} in full matrix):", tri_size, n * n);
    println!("  {:?}\n", tri);

    // Reconstruct full matrix for display
    println!("Reconstructed matrix:");
    print!("     ");
    for j in 0..n {
        print!(" {}  ", labels[j]);
    }
    println!();
    for i in 0..n {
        print!("  {} [", labels[i]);
        for j in 0..n {
            if j > 0 { print!(" "); }
            if i == j {
                print!(" 0 ");
            } else {
                print!("{:2} ", tri[tri_index(i, j)]);
            }
        }
        println!("]");
    }

    println!("\nSpace savings: {:.0}% less memory\n",
             (1.0 - tri_size as f64 / (n * n) as f64) * 100.0);
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Adjacency Matrix — Rust Demonstrations             ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_basic_operations();
    demo_directed();
    demo_weighted();
    demo_matrix_power();
    demo_complement();
    demo_bfs_on_matrix();
    demo_space_analysis();
    demo_triangular();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Matrix representation fully demonstrated!          ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Construcción de matriz

Dado el siguiente grafo no dirigido, construye la matriz de adyacencia
a mano:

```
    A --- B --- C
          |     |
          D --- E --- F
```

<details><summary>¿Cuál es la matriz y cuántos 1s tiene?</summary>

```
    A  B  C  D  E  F
A [ 0  1  0  0  0  0 ]
B [ 1  0  1  1  0  0 ]
C [ 0  1  0  0  1  0 ]
D [ 0  1  0  0  1  0 ]
E [ 0  0  1  1  0  1 ]
F [ 0  0  0  0  1  0 ]
```

Hay $2 \times 5 = 10$ unos (5 aristas × 2 por simetría). La suma de todos
los elementos de la matriz = 10 = $2|E|$, verificando el lema del apretón
de manos.

</details>

### Ejercicio 2 — Dirigido vs no dirigido

Construye las matrices de adyacencia para el mismo grafo interpretado como
(a) no dirigido y (b) dirigido con aristas $A \to B$, $B \to C$, $B \to D$,
$D \to C$. ¿Cuántas celdas difieren?

<details><summary>¿La matriz dirigida es simétrica?</summary>

No. En la versión dirigida:
- $A[0][1] = 1$ pero $A[1][0] = 0$ (hay $A \to B$ pero no $B \to A$).
- $A[3][2] = 1$ pero $A[2][3] = 0$ (hay $D \to C$ pero no $C \to D$).

Difieren exactamente las celdas $(j,i)$ para cada arista $(i,j)$ que no
tiene arista inversa. Si el grafo dirigido tiene $m$ aristas y ninguna es
bidireccional, difieren $m$ celdas respecto a la versión "si fueran 1 en
ambos lados".

</details>

### Ejercicio 3 — Espacio

¿Cuántos bytes ocupa la matriz de adyacencia (usando `int` de 4 bytes) para
un grafo de (a) 100 vértices, (b) 1000 vértices, (c) 10000 vértices?
¿Para cuál de estos es factible en un sistema con 1 GB de RAM?

<details><summary>¿Cuáles caben en 1 GB?</summary>

- $n = 100$: $100^2 \times 4 = 40{,}000$ bytes = 40 KB. Cabe fácilmente.
- $n = 1000$: $1000^2 \times 4 = 4{,}000{,}000$ bytes = 4 MB. Cabe.
- $n = 10{,}000$: $10{,}000^2 \times 4 = 400{,}000{,}000$ bytes = 400 MB. Cabe, pero ajustado.
- $n = 100{,}000$: $10^{10} \times 4 = 40$ GB. **No cabe**. Aquí se necesita lista de adyacencia obligatoriamente.

</details>

### Ejercicio 4 — Caminos de longitud 2

Dado $A = \begin{pmatrix} 0 & 1 & 1 \\ 0 & 0 & 1 \\ 1 & 0 & 0 \end{pmatrix}$ (grafo dirigido de 3 vértices), calcula $A^2$ a mano. ¿Cuántos caminos de longitud 2 hay de 0 a 0?

<details><summary>¿Cuál es $A^2$ y cuántos caminos de longitud 2 van de 0 a 0?</summary>

$A^2 = A \times A$:

$A^2[0][0] = 0 \cdot 0 + 1 \cdot 0 + 1 \cdot 1 = 1$
$A^2[0][1] = 0 \cdot 1 + 1 \cdot 0 + 1 \cdot 0 = 0$
$A^2[0][2] = 0 \cdot 1 + 1 \cdot 1 + 1 \cdot 0 = 1$
$A^2[1][0] = 0 \cdot 0 + 0 \cdot 0 + 1 \cdot 1 = 1$
$A^2[1][1] = 0 \cdot 1 + 0 \cdot 0 + 1 \cdot 0 = 0$
$A^2[1][2] = 0 \cdot 1 + 0 \cdot 1 + 1 \cdot 0 = 0$
$A^2[2][0] = 1 \cdot 0 + 0 \cdot 0 + 0 \cdot 1 = 0$
$A^2[2][1] = 1 \cdot 1 + 0 \cdot 0 + 0 \cdot 0 = 1$
$A^2[2][2] = 1 \cdot 1 + 0 \cdot 1 + 0 \cdot 0 = 1$

$$A^2 = \begin{pmatrix} 1 & 0 & 1 \\ 1 & 0 & 0 \\ 0 & 1 & 1 \end{pmatrix}$$

$A^2[0][0] = 1$: hay **1** camino de longitud 2 de 0 a 0, que es $0 \to 2 \to 0$.

</details>

### Ejercicio 5 — Complemento

Si $G$ es un ciclo de 5 vértices ($C_5$: $0-1-2-3-4-0$), dibuja el
complemento $\bar{G}$. ¿Qué grafo conocido es $\bar{C_5}$?

<details><summary>¿Qué grafo es el complemento de $C_5$?</summary>

$C_5$ tiene aristas: $\{0,1\}, \{1,2\}, \{2,3\}, \{3,4\}, \{4,0\}$.
El total posible es $\binom{5}{2} = 10$. El complemento tiene las 5
aristas restantes: $\{0,2\}, \{0,3\}, \{1,3\}, \{1,4\}, \{2,4\}$.

¡Esto es otro ciclo de 5! Específicamente $0-2-4-1-3-0$. El complemento
de $C_5$ es isomorfo a $C_5$. $C_5$ es un grafo **autocomplementario**.

</details>

### Ejercicio 6 — Grado desde la matriz

Demuestra que para un grafo no dirigido sin self-loops, el grado del
vértice $i$ es $\deg(i) = \sum_{j=0}^{n-1} A[i][j]$ (suma de la fila $i$).
¿Qué fórmula da el in-degree en un digrafo?

<details><summary>¿Cómo se calcula el in-degree?</summary>

El in-degree de $i$ en un digrafo es la **suma de la columna** $i$:
$\deg^{-}(i) = \sum_{j=0}^{n-1} A[j][i]$. Cada $A[j][i] = 1$ significa
que hay una arista $j \to i$, que incrementa el grado de entrada de $i$.

Esto muestra una asimetría de la matriz: el out-degree es eficiente (suma
de fila, acceso contiguo en memoria), pero el in-degree requiere acceso por
columna, que es menos cache-friendly en layout row-major.

</details>

### Ejercicio 7 — Matriz de bits

Implementa una `BitMatrix` en C usando `uint64_t` para almacenar la matriz
de un grafo no ponderado de 200 vértices. Compara el consumo de memoria con
la versión `int`.

<details><summary>¿Cuál es la ratio de compresión?</summary>

- Versión `int`: $200^2 \times 4 = 160{,}000$ bytes = 156 KB.
- Versión bits: $200 \times \lceil 200/64 \rceil \times 8 = 200 \times 4 \times 8 = 6{,}400$ bytes = 6.25 KB.
- Ratio: $160{,}000 / 6{,}400 = 25\times$ más compacto.

En general, la ratio es $\frac{32}{1} = 32\times$ para `int` y $\frac{64}{1} = 64\times$ si se comparara con `int64_t`. La ratio real es un poco
menor por el redondeo a múltiplos de 64.

</details>

### Ejercicio 8 — Transitividad

Dado un grafo dirigido con matriz $A$, ¿cómo puedes usar potencias de la
matriz para construir la **clausura transitiva** (matriz de alcanzabilidad:
$R[i][j] = 1$ si existe camino de cualquier longitud de $i$ a $j$)?

<details><summary>¿Cuál es el algoritmo?</summary>

La clausura transitiva es $R = \text{sign}(A + A^2 + A^3 + \cdots + A^{n-1})$,
donde $\text{sign}(x) = 1$ si $x > 0$, 0 si no.

Un camino de $i$ a $j$ tiene a lo sumo $n - 1$ aristas (si es simple). Así
que basta sumar hasta $A^{n-1}$. Complejidad: $O(n^4)$ si se calculan las
potencias una por una.

Pero hay un método mejor: el **algoritmo de Warshall** (variante booleana de
Floyd-Warshall): $O(n^3)$ y opera in-place sobre la matriz:

```
for k = 0 to n-1:
    for i = 0 to n-1:
        for j = 0 to n-1:
            R[i][j] = R[i][j] OR (R[i][k] AND R[k][j])
```

</details>

### Ejercicio 9 — Triángulos

Un triángulo en un grafo no dirigido es un conjunto de 3 vértices $\{i, j, k\}$
mutuamente conectados. Demuestra que el número de triángulos es
$\frac{\text{tr}(A^3)}{6}$ donde $\text{tr}$ es la traza (suma de la
diagonal).

<details><summary>¿Por qué se divide entre 6?</summary>

$A^3[i][i]$ cuenta los caminos de longitud 3 que empiezan y terminan en $i$.
Cada triángulo $\{i, j, k\}$ contribuye 2 a $A^3[i][i]$ (los caminos
$i \to j \to k \to i$ y $i \to k \to j \to i$). La traza suma sobre todos
los vértices, así que cada triángulo se cuenta $3 \times 2 = 6$ veces
(3 vértices de inicio × 2 direcciones). Dividiendo entre 6 obtenemos el
conteo exacto.

</details>

### Ejercicio 10 — Implementación completa

Implementa una estructura `AdjMatrix` completa en C con las siguientes
funciones: `am_create`, `am_free`, `am_add_edge`, `am_remove_edge`,
`am_has_edge`, `am_degree`, `am_neighbors` (retorna array dinámico de
vecinos), `am_print`, y `am_transpose` (para grafos dirigidos). Pruébala con
un grafo de al menos 8 vértices.

<details><summary>¿Cuál es la complejidad de `am_transpose`?</summary>

$O(n^2)$: debe recorrer toda la matriz e intercambiar $A[i][j]$ con
$A[j][i]$ para todo $i < j$. In-place (sin memoria extra) se puede hacer
con swaps:

```c
for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++) {
        int tmp = A[i][j];
        A[i][j] = A[j][i];
        A[j][i] = tmp;
    }
```

Para un grafo no dirigido, `am_transpose` es un no-op (la matriz ya es
simétrica).

</details>

---

## 11. Resumen

| Aspecto | Matriz de adyacencia |
|---------|---------------------|
| Espacio | $O(n^2)$ — independiente de $m$ |
| Verificar arista | $O(1)$ — acceso directo |
| Listar vecinos | $O(n)$ — recorrer toda la fila |
| Agregar/eliminar arista | $O(1)$ |
| Ideal para | Grafos densos, Floyd-Warshall, potencias de matriz |
| Evitar cuando | Grafos dispersos, BFS/DFS frecuente, $n > 10^4$ |
| Variantes | Bits (32x compresión), triangular ($n(n-1)/2$), CSR |

En T03 veremos la alternativa principal — la **lista de adyacencia** — que
invierte estas ventajas: espacio $O(n + m)$ y vecinos en $O(\deg(v))$, pero
verificar arista en $O(\deg(v))$.
