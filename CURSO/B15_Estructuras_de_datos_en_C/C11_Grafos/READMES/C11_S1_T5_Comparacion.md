# T05 — Comparación de representaciones de grafos

## Objetivo

Consolidar las representaciones estudiadas en S01 (matriz de adyacencia,
lista enlazada, array dinámico, HashMap, petgraph) mediante una comparación
cuantitativa y cualitativa, y establecer criterios claros para elegir la
representación correcta según el problema.

---

## 1. Tabla comparativa principal

| Operación | Matriz | Lista enlazada | Array dinámico (`Vec`) | HashMap | Notas |
|-----------|:------:|:--------------:|:---------------------:|:-------:|-------|
| **Espacio** | $O(n^2)$ | $O(n + m)$ | $O(n + m)$ | $O(n + m)$ | Matriz desperdicia en grafos dispersos |
| **¿Existe arista $(u,v)$?** | $O(1)$ | $O(\deg(u))$ | $O(\deg(u))$ | $O(\deg(u))$* | *$O(1)$ si se usa `HashSet` auxiliar |
| **Agregar arista** | $O(1)$ | $O(1)$ | $O(1)$ amort | $O(1)$ amort | Todas son rápidas |
| **Eliminar arista** | $O(1)$ | $O(\deg)$ | $O(1)$ swap / $O(\deg)$ | $O(\deg)$ | Matriz gana aquí |
| **Listar vecinos** | $O(n)$ | $O(\deg)$ | $O(\deg)$ | $O(\deg)$ | Crítico para BFS/DFS |
| **Grado** | $O(n)$ | $O(\deg)$ o $O(1)$** | $O(1)$ | $O(1)$ | **$O(1)$ si se almacena count |
| **Agregar vértice** | $O(n^2)$† | $O(1)$ | $O(1)$ amort | $O(1)$ | †Requiere reasignar la matriz |
| **Eliminar vértice** | $O(n^2)$ | $O(n + m)$ | $O(n + m)$ | $O(n + m)$ | Costoso en todas |
| **Recorrer todo** | $O(n^2)$ | $O(n + m)$ | $O(n + m)$ | $O(n + m)$ | BFS/DFS total |

---

## 2. Espacio en detalle

### Bytes por arista

Para un grafo no dirigido (cada arista almacenada dos veces en listas):

| Representación | Bytes/arista | Bytes/vértice (overhead fijo) | Total para $n$, $m$ |
|----------------|:------------:|:----------------------------:|:-------------------:|
| Matriz (`int`) | $\frac{4n^2}{m}$‡ | 0 | $4n^2$ |
| Matriz (bits) | $\frac{n^2/8}{m}$‡ | 0 | $\frac{n^2}{8}$ |
| Lista enlazada | $\sim 32$ (16 × 2) | $\sim 8$ (puntero) | $8n + 32m$ |
| Array dinámico | $\sim 16$ (8 × 2) | $\sim 24$ (Vec header) | $24n + 16m$ |
| HashMap | $\sim 24$ (8 × 2 + overhead) | $\sim 80$ (HashMap entry) | $80n + 24m$ |

‡ La "bytes por arista" de la matriz depende de $m$ porque el espacio es
fijo $O(n^2)$; para grafos dispersos esto es un desperdicio enorme.

### Punto de cruce

La matriz consume menos espacio que la lista cuando el costo de la matriz
($4n^2$) es menor que el de la lista ($24n + 16m$). Simplificando:

$$4n^2 < 24n + 16m \implies n < 6 + 4m/n$$

Para un grafo completo ($m = n(n-1)/2$), la matriz siempre gana. Para
$m = 2n$ (muy disperso), la matriz pierde cuando $n > 14$ aproximadamente.

Regla práctica: **si la densidad $d > 1/8$ (12.5%), la matriz es competitiva
en espacio**.

### Tabla numérica

| $n$ | $m$ | Densidad | Matriz | Array `Vec` | Ratio |
|-----|-----|:--------:|:------:|:-----------:|:-----:|
| 100 | 200 | 0.04 | 40 KB | 6 KB | 6.7x |
| 100 | 4,950 | 1.00 | 40 KB | 82 KB | 0.5x |
| 1,000 | 5,000 | 0.01 | 4 MB | 104 KB | 39x |
| 1,000 | 499,500 | 1.00 | 4 MB | 8 MB | 0.5x |
| 10,000 | 50,000 | 0.001 | 400 MB | 1 MB | 400x |
| 100,000 | 500,000 | 0.0001 | 40 GB | 10 MB | 4,000x |

Para la última fila, la matriz ni siquiera cabe en RAM.

---

## 3. Rendimiento de caché

La complejidad asintótica no cuenta toda la historia. La **localidad de
caché** afecta drásticamente el rendimiento real:

| Representación | Patrón de acceso | Cache behavior |
|----------------|:----------------:|:--------------:|
| Matriz (flat) | Secuencial por fila | Excelente |
| Matriz (`int **`) | Indirección por fila | Medio |
| Lista enlazada | Saltos aleatorios en heap | Malo |
| Array dinámico (`Vec`) | Secuencial dentro de cada Vec | Bueno |
| HashMap | Hashing + posible indirección | Medio |

### Impacto práctico

Para un BFS en un grafo de $n = 10^5$, $m = 5 \times 10^5$:

- **Array dinámico** recorre vecinos secuencialmente → el prefetcher del
  CPU carga la línea de caché siguiente automáticamente.
- **Lista enlazada** salta a una dirección arbitraria por cada vecino →
  cache miss por cada nodo si los nodos no están contiguos.
- Diferencia medida típica: **2-5x** más rápido con array dinámico.

Esta es la razón principal por la que `Vec<Vec<>>` es la representación
estándar en la práctica, no la lista enlazada "clásica" de los libros de
texto.

---

## 4. Rendimiento por algoritmo

Cada algoritmo tiene un patrón de acceso dominante que favorece a una
representación:

| Algoritmo | Operación dominante | Mejor representación |
|-----------|:-------------------:|:--------------------:|
| BFS | Listar vecinos | Lista / Vec |
| DFS | Listar vecinos | Lista / Vec |
| Dijkstra | Listar vecinos + grado | Vec + priority queue |
| Bellman-Ford | Recorrer todas las aristas | Lista de aristas o Vec |
| Floyd-Warshall | Acceso $A[i][j]$ directo | **Matriz** |
| Verificar arista | Consulta puntual | **Matriz** o HashSet |
| Kruskal | Ordenar aristas | **Lista de aristas** |
| Prim | Listar vecinos + min | Vec + priority queue |
| Topológico (Kahn) | In-degree + listar vecinos | Vec + array in-deg |
| Tarjan SCC | DFS con stack | Vec |
| PageRank | Multiplicación matriz-vector | Matriz o CSR |
| Coloreo de grafos | Verificar arista vecinos | Matriz |

### Regla general

- Si el algoritmo **recorre vecinos** → lista/Vec gana.
- Si el algoritmo **consulta aristas específicas** → matriz gana.
- Si el algoritmo **opera sobre todas las aristas en bloque** → lista de
  aristas o CSR.

---

## 5. Comparación de implementación

### Complejidad de código (líneas aproximadas)

| Tarea | Matriz (C) | Vec (C) | Lista enlazada (C) | Rust `Vec<Vec<>>` |
|-------|:----------:|:-------:|:-------------------:|:-----------------:|
| Crear grafo | 5 | 15 | 10 | 1 |
| Agregar arista | 2 | 8 | 10 | 2 |
| BFS completo | 15 | 15 | 15 | 12 |
| Eliminar arista | 2 | 10 | 15 | 3 |
| Liberar memoria | 2 | 10 | 15 | 0 (automático) |
| **Total mínimo** | **~26** | **~58** | **~65** | **~18** |

La matriz es la más simple de implementar en C. La lista enlazada es la
más verbosa. Rust con `Vec<Vec<>>` es la más concisa porque no requiere
gestión manual de memoria.

### Facilidad de depuración

| Representación | Print/debug | Errores comunes |
|----------------|:-----------:|:---------------:|
| Matriz | Fácil (print 2D) | Olvidar simetría, off-by-one |
| Lista enlazada | Medio (recorrer listas) | Memory leaks, dangling ptrs |
| Array dinámico | Fácil (`{:?}` en Rust) | Capacidad no reservada |
| HashMap | Fácil (`.iter()`) | Vértices no insertados |

---

## 6. Árbol de decisión

```
¿Cuántos vértices?
├─ n > 10⁴ → Imposible usar matriz
│   └─ ¿Vértices son enteros 0..n?
│       ├─ Sí → Vec<Vec<(usize, W)>>
│       └─ No → HashMap o NamedGraph
├─ n ≤ 10⁴
│   └─ ¿Grafo denso (d > 0.1)?
│       ├─ Sí → ¿Algoritmo necesita A[i][j]?
│       │       ├─ Sí → Matriz de adyacencia
│       │       └─ No → Cualquiera funciona, Vec es más versátil
│       └─ No (disperso)
│           └─ Vec<Vec<(usize, W)>>
│
¿Es un proyecto de producción?
├─ Sí → petgraph (algoritmos incluidos, API segura)
└─ No → ¿Competencia?
        ├─ Sí → Vec<Vec<>> (mínimo boilerplate)
        └─ No (aprendizaje) → Implementar ambas (matriz y lista)
```

---

## 7. Casos de estudio

### Caso 1: Red social (Facebook)

- $n \approx 3 \times 10^9$ usuarios
- $m \approx 2 \times 10^{11}$ amistades
- Densidad: $d \approx 4 \times 10^{-8}$

**Representación**: lista de adyacencia distribuida (cada servidor almacena
un subconjunto de vértices). La matriz necesitaría $\sim 10^{18}$ bytes —
imposible.

### Caso 2: Mapa de carreteras (DIMACS)

- $n \approx 24 \times 10^6$ intersecciones (US road network)
- $m \approx 58 \times 10^6$ segmentos de carretera
- Densidad: $d \approx 2 \times 10^{-7}$

**Representación**: CSR (Compressed Sparse Row) — inmutable, compacto,
cache-friendly para Dijkstra y A*. Similar a `Vec<Vec<>>` pero con arrays
planos.

### Caso 3: Circuito integrado (VLSI)

- $n \approx 10^3$ a $10^4$ compuertas
- $m \approx n$ a $5n$ (disperso pero no extremo)
- Algoritmos: verificación de aristas, coloreo

**Representación**: matriz de adyacencia (cabe en memoria, verificación de
aristas en $O(1)$ es crítica para coloreo).

### Caso 4: Dependencias de paquetes (Cargo/npm)

- $n \approx 10^5$ paquetes
- $m \approx 5 \times 10^5$ dependencias
- Operaciones: topological sort, detección de ciclos, BFS

**Representación**: `HashMap<String, Vec<String>>` o `NamedGraph` — los
paquetes se identifican por nombre, no por entero. `petgraph::GraphMap` es
ideal aquí.

---

## 8. Programa completo en C

```c
/*
 * graph_comparison.c
 *
 * Benchmarks and compares matrix vs linked list vs dynamic array
 * adjacency list for key graph operations.
 *
 * Compile: gcc -O2 -o graph_comparison graph_comparison.c -lm
 * Run:     ./graph_comparison
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/* ========================= MATRIX ================================= */

typedef struct {
    int *data;
    int n;
} MatGraph;

MatGraph mat_create(int n) {
    MatGraph g;
    g.n = n;
    g.data = calloc((size_t)n * n, sizeof(int));
    return g;
}

void mat_free(MatGraph *g) { free(g->data); }

void mat_add_edge(MatGraph *g, int u, int v) {
    g->data[u * g->n + v] = 1;
    g->data[v * g->n + u] = 1;
}

int mat_has_edge(const MatGraph *g, int u, int v) {
    return g->data[u * g->n + v];
}

/* ====================== LINKED LIST =============================== */

typedef struct LLNode {
    int dest;
    struct LLNode *next;
} LLNode;

typedef struct {
    LLNode **heads;
    int n;
} LLGraph;

LLGraph ll_create(int n) {
    LLGraph g;
    g.n = n;
    g.heads = calloc(n, sizeof(LLNode *));
    return g;
}

void ll_free(LLGraph *g) {
    for (int i = 0; i < g->n; i++) {
        LLNode *curr = g->heads[i];
        while (curr) { LLNode *tmp = curr; curr = curr->next; free(tmp); }
    }
    free(g->heads);
}

void ll_add_edge(LLGraph *g, int u, int v) {
    LLNode *node = malloc(sizeof(LLNode));
    node->dest = v; node->next = g->heads[u]; g->heads[u] = node;
    node = malloc(sizeof(LLNode));
    node->dest = u; node->next = g->heads[v]; g->heads[v] = node;
}

int ll_has_edge(const LLGraph *g, int u, int v) {
    for (LLNode *curr = g->heads[u]; curr; curr = curr->next)
        if (curr->dest == v) return 1;
    return 0;
}

/* ===================== DYNAMIC ARRAY ============================== */

typedef struct {
    int *dests;
    int size, cap;
} DynArr;

typedef struct {
    DynArr *adj;
    int n;
} VecGraph;

VecGraph vec_create(int n) {
    VecGraph g;
    g.n = n;
    g.adj = calloc(n, sizeof(DynArr));
    return g;
}

void vec_free(VecGraph *g) {
    for (int i = 0; i < g->n; i++) free(g->adj[i].dests);
    free(g->adj);
}

static void dyn_push(DynArr *d, int val) {
    if (d->size == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 4;
        d->dests = realloc(d->dests, d->cap * sizeof(int));
    }
    d->dests[d->size++] = val;
}

void vec_add_edge(VecGraph *g, int u, int v) {
    dyn_push(&g->adj[u], v);
    dyn_push(&g->adj[v], u);
}

int vec_has_edge(const VecGraph *g, int u, int v) {
    for (int i = 0; i < g->adj[u].size; i++)
        if (g->adj[u].dests[i] == v) return 1;
    return 0;
}

/* ======================== TIMING ================================== */

static double elapsed_ms(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
}

/* ========================= DEMOS ================================== */

/*
 * Demo 1: Construction time comparison
 */
void demo_construction(void) {
    printf("=== Demo 1: Construction Time ===\n\n");

    int sizes[][2] = {{500, 2000}, {1000, 5000}, {2000, 20000}};
    int num = 3;

    printf("%-10s %-10s  %-12s %-12s %-12s\n",
           "n", "m", "Matrix (ms)", "LinkedL (ms)", "Vec (ms)");
    printf("%s\n", "--------------------------------------------------------------");

    for (int s = 0; s < num; s++) {
        int n = sizes[s][0], m = sizes[s][1];

        /* Generate random edges */
        int (*edges)[2] = malloc(m * sizeof(*edges));
        srand(42);
        for (int i = 0; i < m; i++) {
            edges[i][0] = rand() % n;
            edges[i][1] = rand() % n;
        }

        struct timespec t0, t1;

        /* Matrix */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        MatGraph mg = mat_create(n);
        for (int i = 0; i < m; i++) mat_add_edge(&mg, edges[i][0], edges[i][1]);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double mat_ms = elapsed_ms(t0, t1);

        /* Linked list */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        LLGraph lg = ll_create(n);
        for (int i = 0; i < m; i++) ll_add_edge(&lg, edges[i][0], edges[i][1]);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ll_ms = elapsed_ms(t0, t1);

        /* Vec */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        VecGraph vg = vec_create(n);
        for (int i = 0; i < m; i++) vec_add_edge(&vg, edges[i][0], edges[i][1]);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double vec_ms = elapsed_ms(t0, t1);

        printf("%-10d %-10d  %-12.3f %-12.3f %-12.3f\n",
               n, m, mat_ms, ll_ms, vec_ms);

        mat_free(&mg); ll_free(&lg); vec_free(&vg);
        free(edges);
    }
    printf("\n");
}

/*
 * Demo 2: BFS comparison
 */
void demo_bfs(void) {
    printf("=== Demo 2: BFS Time ===\n\n");

    int n = 5000, m = 20000;
    srand(123);

    /* Build all three representations with same edges */
    MatGraph mg = mat_create(n);
    LLGraph lg = ll_create(n);
    VecGraph vg = vec_create(n);

    for (int i = 0; i < m; i++) {
        int u = rand() % n, v = rand() % n;
        mat_add_edge(&mg, u, v);
        ll_add_edge(&lg, u, v);
        vec_add_edge(&vg, u, v);
    }

    int *visited = malloc(n * sizeof(int));
    int *queue = malloc(n * sizeof(int));
    struct timespec t0, t1;

    /* BFS on matrix */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int rep = 0; rep < 10; rep++) {
        memset(visited, 0, n * sizeof(int));
        int front = 0, back = 0;
        visited[0] = 1; queue[back++] = 0;
        while (front < back) {
            int u = queue[front++];
            for (int v = 0; v < n; v++) {
                if (mat_has_edge(&mg, u, v) && !visited[v]) {
                    visited[v] = 1; queue[back++] = v;
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double mat_ms = elapsed_ms(t0, t1);

    /* BFS on linked list */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int rep = 0; rep < 10; rep++) {
        memset(visited, 0, n * sizeof(int));
        int front = 0, back = 0;
        visited[0] = 1; queue[back++] = 0;
        while (front < back) {
            int u = queue[front++];
            for (LLNode *curr = lg.heads[u]; curr; curr = curr->next) {
                if (!visited[curr->dest]) {
                    visited[curr->dest] = 1; queue[back++] = curr->dest;
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ll_ms = elapsed_ms(t0, t1);

    /* BFS on Vec */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int rep = 0; rep < 10; rep++) {
        memset(visited, 0, n * sizeof(int));
        int front = 0, back = 0;
        visited[0] = 1; queue[back++] = 0;
        while (front < back) {
            int u = queue[front++];
            for (int i = 0; i < vg.adj[u].size; i++) {
                int v = vg.adj[u].dests[i];
                if (!visited[v]) {
                    visited[v] = 1; queue[back++] = v;
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double vec_ms = elapsed_ms(t0, t1);

    printf("BFS on n=%d, m=%d (10 runs):\n", n, m);
    printf("  Matrix:      %.3f ms  (O(n²) per BFS)\n", mat_ms);
    printf("  Linked list: %.3f ms  (O(n+m) but cache misses)\n", ll_ms);
    printf("  Vec array:   %.3f ms  (O(n+m), cache-friendly)\n\n", vec_ms);

    free(visited); free(queue);
    mat_free(&mg); ll_free(&lg); vec_free(&vg);
}

/*
 * Demo 3: Edge query comparison
 */
void demo_edge_query(void) {
    printf("=== Demo 3: Edge Query Time ===\n\n");

    int n = 2000, m = 10000;
    srand(456);

    MatGraph mg = mat_create(n);
    LLGraph lg = ll_create(n);
    VecGraph vg = vec_create(n);

    for (int i = 0; i < m; i++) {
        int u = rand() % n, v = rand() % n;
        mat_add_edge(&mg, u, v);
        ll_add_edge(&lg, u, v);
        vec_add_edge(&vg, u, v);
    }

    /* Generate random queries */
    int Q = 100000;
    int (*queries)[2] = malloc(Q * sizeof(*queries));
    for (int i = 0; i < Q; i++) {
        queries[i][0] = rand() % n;
        queries[i][1] = rand() % n;
    }

    struct timespec t0, t1;
    volatile int found;

    /* Matrix queries */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < Q; i++)
        found = mat_has_edge(&mg, queries[i][0], queries[i][1]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double mat_ms = elapsed_ms(t0, t1);

    /* Linked list queries */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < Q; i++)
        found = ll_has_edge(&lg, queries[i][0], queries[i][1]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ll_ms = elapsed_ms(t0, t1);

    /* Vec queries */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < Q; i++)
        found = vec_has_edge(&vg, queries[i][0], queries[i][1]);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double vec_ms = elapsed_ms(t0, t1);

    printf("%d has_edge queries on n=%d, m=%d:\n", Q, n, m);
    printf("  Matrix:      %.3f ms  (O(1) per query)\n", mat_ms);
    printf("  Linked list: %.3f ms  (O(deg) per query)\n", ll_ms);
    printf("  Vec array:   %.3f ms  (O(deg) per query)\n\n", vec_ms);

    free(queries);
    mat_free(&mg); ll_free(&lg); vec_free(&vg);
}

/*
 * Demo 4: Memory usage comparison
 */
void demo_memory(void) {
    printf("=== Demo 4: Memory Usage ===\n\n");

    printf("%-8s %-8s %-8s  %-14s %-14s %-14s\n",
           "n", "m", "density", "Matrix", "LinkedList", "Vec");
    printf("%s\n", "-----------------------------------------------------------------------");

    struct { int n; int m; } cases[] = {
        {100, 200}, {100, 4950}, {1000, 5000},
        {1000, 50000}, {5000, 25000}, {5000, 500000}
    };

    for (int c = 0; c < 6; c++) {
        int n = cases[c].n, m = cases[c].m;
        double d = 2.0 * m / ((double)n * (n - 1));

        long mat_bytes = (long)n * n * sizeof(int);
        long ll_bytes = (long)n * sizeof(LLNode *) + 2L * m * sizeof(LLNode);
        long vec_bytes = (long)n * sizeof(DynArr) + 2L * m * sizeof(int);

        char mat_str[32], ll_str[32], vec_str[32];
        snprintf(mat_str, 32, "%.1f KB", mat_bytes / 1024.0);
        snprintf(ll_str, 32, "%.1f KB", ll_bytes / 1024.0);
        snprintf(vec_str, 32, "%.1f KB", vec_bytes / 1024.0);

        printf("%-8d %-8d %-8.4f  %-14s %-14s %-14s\n",
               n, m, d, mat_str, ll_str, vec_str);
    }
    printf("\n");
}

/*
 * Demo 5: Neighbor iteration time
 */
void demo_neighbor_iteration(void) {
    printf("=== Demo 5: Neighbor Iteration ===\n\n");

    int n = 3000, m = 15000;
    srand(789);

    MatGraph mg = mat_create(n);
    LLGraph lg = ll_create(n);
    VecGraph vg = vec_create(n);

    for (int i = 0; i < m; i++) {
        int u = rand() % n, v = rand() % n;
        mat_add_edge(&mg, u, v);
        ll_add_edge(&lg, u, v);
        vec_add_edge(&vg, u, v);
    }

    struct timespec t0, t1;
    volatile long sum;

    /* Sum all neighbor indices (forces iteration) */

    /* Matrix */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = 0;
    for (int rep = 0; rep < 50; rep++)
        for (int u = 0; u < n; u++)
            for (int v = 0; v < n; v++)
                if (mg.data[u * n + v]) sum += v;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double mat_ms = elapsed_ms(t0, t1);

    /* Linked list */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = 0;
    for (int rep = 0; rep < 50; rep++)
        for (int u = 0; u < n; u++)
            for (LLNode *curr = lg.heads[u]; curr; curr = curr->next)
                sum += curr->dest;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ll_ms = elapsed_ms(t0, t1);

    /* Vec */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    sum = 0;
    for (int rep = 0; rep < 50; rep++)
        for (int u = 0; u < n; u++)
            for (int i = 0; i < vg.adj[u].size; i++)
                sum += vg.adj[u].dests[i];
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double vec_ms = elapsed_ms(t0, t1);

    printf("Iterate all neighbors, all vertices (50 runs), n=%d, m=%d:\n", n, m);
    printf("  Matrix:      %.3f ms  (scans n²=%d cells)\n", mat_ms, n * n);
    printf("  Linked list: %.3f ms  (follows 2m=%d pointers)\n", ll_ms, 2 * m);
    printf("  Vec array:   %.3f ms  (reads 2m=%d contiguous ints)\n\n", vec_ms, 2 * m);

    mat_free(&mg); ll_free(&lg); vec_free(&vg);
}

/*
 * Demo 6: Summary recommendation
 */
void demo_summary(void) {
    printf("=== Demo 6: Recommendation Summary ===\n\n");

    printf("  ┌─────────────────────────┬──────────────────────────────┐\n");
    printf("  │ Scenario                │ Best representation          │\n");
    printf("  ├─────────────────────────┼──────────────────────────────┤\n");
    printf("  │ Dense graph, small n    │ Adjacency matrix             │\n");
    printf("  │ Floyd-Warshall          │ Adjacency matrix             │\n");
    printf("  │ Sparse, BFS/DFS         │ Vec (dynamic array)          │\n");
    printf("  │ Competitive programming │ Vec<Vec<>> or static array   │\n");
    printf("  │ Named vertices          │ HashMap / NamedGraph         │\n");
    printf("  │ Production Rust         │ petgraph                     │\n");
    printf("  │ Kruskal's MST           │ Edge list                    │\n");
    printf("  │ Frequent has_edge       │ Matrix or Vec + HashSet      │\n");
    printf("  │ Very large (n > 10^5)   │ Vec or CSR (never matrix)    │\n");
    printf("  └─────────────────────────┴──────────────────────────────┘\n\n");
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Graph Representation Comparison — C Benchmarks     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_construction();
    demo_bfs();
    demo_edge_query();
    demo_memory();
    demo_neighbor_iteration();
    demo_summary();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Comparison complete — choose wisely!               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 9. Programa completo en Rust

```rust
/*
 * graph_comparison.rs
 *
 * Benchmarks and compares graph representations in Rust.
 *
 * Run: cargo run --release
 */

use std::collections::{HashMap, HashSet, VecDeque};
use std::time::Instant;

// ── Representations ──────────────────────────────────────────────────

/// Adjacency matrix (flat Vec<i32>)
struct MatGraph {
    data: Vec<i32>,
    n: usize,
}

impl MatGraph {
    fn new(n: usize) -> Self {
        MatGraph { data: vec![0; n * n], n }
    }
    fn add_edge(&mut self, u: usize, v: usize) {
        self.data[u * self.n + v] = 1;
        self.data[v * self.n + u] = 1;
    }
    fn has_edge(&self, u: usize, v: usize) -> bool {
        self.data[u * self.n + v] != 0
    }
}

/// Vec<Vec<usize>> adjacency list
struct VecGraph {
    adj: Vec<Vec<usize>>,
}

impl VecGraph {
    fn new(n: usize) -> Self {
        VecGraph { adj: vec![vec![]; n] }
    }
    fn add_edge(&mut self, u: usize, v: usize) {
        self.adj[u].push(v);
        self.adj[v].push(u);
    }
    fn has_edge(&self, u: usize, v: usize) -> bool {
        self.adj[u].contains(&v)
    }
    fn n(&self) -> usize {
        self.adj.len()
    }
}

/// HashMap<usize, HashSet<usize>> — for O(1) has_edge
struct HashGraph {
    adj: HashMap<usize, HashSet<usize>>,
    n: usize,
}

impl HashGraph {
    fn new(n: usize) -> Self {
        let mut adj = HashMap::with_capacity(n);
        for i in 0..n {
            adj.insert(i, HashSet::new());
        }
        HashGraph { adj, n }
    }
    fn add_edge(&mut self, u: usize, v: usize) {
        self.adj.get_mut(&u).unwrap().insert(v);
        self.adj.get_mut(&v).unwrap().insert(u);
    }
    fn has_edge(&self, u: usize, v: usize) -> bool {
        self.adj.get(&u).map_or(false, |s| s.contains(&v))
    }
}

// ── Helpers ──────────────────────────────────────────────────────────

fn generate_edges(n: usize, m: usize, seed: u64) -> Vec<(usize, usize)> {
    let mut edges = Vec::with_capacity(m);
    let mut rng = seed;
    for _ in 0..m {
        rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
        let u = ((rng >> 33) as usize) % n;
        rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
        let v = ((rng >> 33) as usize) % n;
        edges.push((u, v));
    }
    edges
}

// ── Demo 1: Construction time ────────────────────────────────────────

fn demo_construction() {
    println!("=== Demo 1: Construction Time ===\n");

    let cases = [(500, 2_000), (1_000, 5_000), (2_000, 20_000)];

    println!("{:<8} {:<8}  {:<12} {:<12} {:<12}",
             "n", "m", "Matrix (ms)", "Vec (ms)", "HashMap (ms)");
    println!("{}", "-".repeat(56));

    for &(n, m) in &cases {
        let edges = generate_edges(n, m, 42);

        let start = Instant::now();
        let mut mg = MatGraph::new(n);
        for &(u, v) in &edges { mg.add_edge(u, v); }
        let mat_ms = start.elapsed().as_secs_f64() * 1000.0;

        let start = Instant::now();
        let mut vg = VecGraph::new(n);
        for &(u, v) in &edges { vg.add_edge(u, v); }
        let vec_ms = start.elapsed().as_secs_f64() * 1000.0;

        let start = Instant::now();
        let mut hg = HashGraph::new(n);
        for &(u, v) in &edges { hg.add_edge(u, v); }
        let hash_ms = start.elapsed().as_secs_f64() * 1000.0;

        println!("{:<8} {:<8}  {:<12.3} {:<12.3} {:<12.3}",
                 n, m, mat_ms, vec_ms, hash_ms);
    }
    println!();
}

// ── Demo 2: BFS time ─────────────────────────────────────────────────

fn demo_bfs() {
    println!("=== Demo 2: BFS Time ===\n");

    let n = 5_000;
    let m = 20_000;
    let edges = generate_edges(n, m, 123);
    let runs = 10;

    let mut mg = MatGraph::new(n);
    let mut vg = VecGraph::new(n);
    for &(u, v) in &edges {
        mg.add_edge(u, v);
        vg.add_edge(u, v);
    }

    // BFS on matrix
    let start = Instant::now();
    for _ in 0..runs {
        let mut visited = vec![false; n];
        let mut queue = VecDeque::new();
        visited[0] = true;
        queue.push_back(0);
        while let Some(u) = queue.pop_front() {
            for v in 0..n {
                if mg.has_edge(u, v) && !visited[v] {
                    visited[v] = true;
                    queue.push_back(v);
                }
            }
        }
    }
    let mat_ms = start.elapsed().as_secs_f64() * 1000.0;

    // BFS on Vec
    let start = Instant::now();
    for _ in 0..runs {
        let mut visited = vec![false; n];
        let mut queue = VecDeque::new();
        visited[0] = true;
        queue.push_back(0);
        while let Some(u) = queue.pop_front() {
            for &v in &vg.adj[u] {
                if !visited[v] {
                    visited[v] = true;
                    queue.push_back(v);
                }
            }
        }
    }
    let vec_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("BFS on n={n}, m={m} ({runs} runs):");
    println!("  Matrix: {mat_ms:.3} ms  (O(n²) per BFS)");
    println!("  Vec:    {vec_ms:.3} ms  (O(n+m), cache-friendly)");
    println!("  Speedup: {:.1}x\n", mat_ms / vec_ms);
}

// ── Demo 3: Edge query time ──────────────────────────────────────────

fn demo_edge_query() {
    println!("=== Demo 3: Edge Query Time ===\n");

    let n = 2_000;
    let m = 10_000;
    let edges = generate_edges(n, m, 456);
    let queries = generate_edges(n, 100_000, 789);

    let mut mg = MatGraph::new(n);
    let mut vg = VecGraph::new(n);
    let mut hg = HashGraph::new(n);
    for &(u, v) in &edges {
        mg.add_edge(u, v);
        vg.add_edge(u, v);
        hg.add_edge(u, v);
    }

    // Matrix queries
    let start = Instant::now();
    let mut found_m = 0u64;
    for &(u, v) in &queries {
        if mg.has_edge(u, v) { found_m += 1; }
    }
    let mat_ms = start.elapsed().as_secs_f64() * 1000.0;

    // Vec queries
    let start = Instant::now();
    let mut found_v = 0u64;
    for &(u, v) in &queries {
        if vg.has_edge(u, v) { found_v += 1; }
    }
    let vec_ms = start.elapsed().as_secs_f64() * 1000.0;

    // HashMap+HashSet queries
    let start = Instant::now();
    let mut found_h = 0u64;
    for &(u, v) in &queries {
        if hg.has_edge(u, v) { found_h += 1; }
    }
    let hash_ms = start.elapsed().as_secs_f64() * 1000.0;

    assert_eq!(found_m, found_v);
    assert_eq!(found_m, found_h);

    println!("{} has_edge queries on n={n}, m={m}:", queries.len());
    println!("  Matrix:     {mat_ms:.3} ms  (O(1) per query)");
    println!("  Vec:        {vec_ms:.3} ms  (O(deg) per query)");
    println!("  HashSet:    {hash_ms:.3} ms  (O(1) amortized)");
    println!("  Found: {found_m} edges\n");
}

// ── Demo 4: Memory estimation ────────────────────────────────────────

fn demo_memory() {
    println!("=== Demo 4: Memory Estimation ===\n");

    let cases: Vec<(usize, usize)> = vec![
        (100, 200), (100, 4950), (1000, 5000),
        (1000, 50000), (5000, 25000), (10000, 50000),
    ];

    println!("{:<8} {:<8} {:<8}  {:<14} {:<14} {:<14}",
             "n", "m", "density", "Matrix", "Vec<Vec>", "HashMap+HSet");
    println!("{}", "-".repeat(76));

    for &(n, m) in &cases {
        let d = 2.0 * m as f64 / (n as f64 * (n as f64 - 1.0));

        let mat_bytes = n * n * 4; // i32
        // Vec<Vec<usize>>: n * 24 (Vec header) + 2m * 8 (usize)
        let vec_bytes = n * 24 + 2 * m * 8;
        // HashMap<usize, HashSet<usize>>: ~80 per vertex + ~40 per edge entry
        let hash_bytes = n * 80 + 2 * m * 40;

        println!("{:<8} {:<8} {:<8.4}  {:<14} {:<14} {:<14}",
                 n, m, d,
                 format_bytes(mat_bytes),
                 format_bytes(vec_bytes),
                 format_bytes(hash_bytes));
    }
    println!();
}

fn format_bytes(b: usize) -> String {
    if b >= 1_048_576 {
        format!("{:.1} MB", b as f64 / 1_048_576.0)
    } else {
        format!("{:.1} KB", b as f64 / 1024.0)
    }
}

// ── Demo 5: Neighbor iteration ───────────────────────────────────────

fn demo_neighbor_iteration() {
    println!("=== Demo 5: Neighbor Iteration ===\n");

    let n = 3_000;
    let m = 15_000;
    let edges = generate_edges(n, m, 999);
    let runs = 50;

    let mut mg = MatGraph::new(n);
    let mut vg = VecGraph::new(n);
    for &(u, v) in &edges {
        mg.add_edge(u, v);
        vg.add_edge(u, v);
    }

    // Matrix: iterate all neighbors of all vertices
    let start = Instant::now();
    let mut sum_m = 0u64;
    for _ in 0..runs {
        for u in 0..n {
            for v in 0..n {
                if mg.data[u * n + v] != 0 { sum_m += v as u64; }
            }
        }
    }
    let mat_ms = start.elapsed().as_secs_f64() * 1000.0;

    // Vec: iterate all neighbors
    let start = Instant::now();
    let mut sum_v = 0u64;
    for _ in 0..runs {
        for u in 0..n {
            for &v in &vg.adj[u] {
                sum_v += v as u64;
            }
        }
    }
    let vec_ms = start.elapsed().as_secs_f64() * 1000.0;

    println!("Iterate all neighbors × {runs} runs (n={n}, m={m}):");
    println!("  Matrix: {mat_ms:.3} ms  (scans {} cells)", n * n);
    println!("  Vec:    {vec_ms:.3} ms  (reads {} entries)", 2 * m);
    println!("  Speedup: {:.1}x", mat_ms / vec_ms);
    println!("  (sums: {sum_m}, {sum_v} — may differ due to duplicates)\n");
}

// ── Demo 6: Complete comparison table ────────────────────────────────

fn demo_comparison_table() {
    println!("=== Demo 6: When to Use What ===\n");

    println!("  ┌─────────────────────────────┬────────────────────────────────┐");
    println!("  │ Scenario                    │ Best representation            │");
    println!("  ├─────────────────────────────┼────────────────────────────────┤");
    println!("  │ Dense graph, n < 10⁴        │ Adjacency matrix               │");
    println!("  │ Floyd-Warshall              │ Adjacency matrix               │");
    println!("  │ Sparse, BFS/DFS/Dijkstra    │ Vec<Vec<(usize, W)>>           │");
    println!("  │ Competitive programming     │ Vec<Vec<>> (minimal setup)     │");
    println!("  │ Frequent has_edge queries   │ Matrix or Vec + HashSet        │");
    println!("  │ Named/string vertices       │ NamedGraph or HashMap          │");
    println!("  │ Production Rust project     │ petgraph::Graph                │");
    println!("  │ Kruskal's MST               │ Edge list (Vec<(u,v,w)>)       │");
    println!("  │ Very large graphs (n > 10⁵) │ Vec or CSR — never matrix      │");
    println!("  │ Dynamic (add/remove nodes)  │ HashMap or petgraph::Stable    │");
    println!("  └─────────────────────────────┴────────────────────────────────┘");
    println!();

    println!("  Default choice: Vec<Vec<(usize, W)>> — covers 90% of cases.");
    println!("  Add matrix only for Floyd-Warshall or dense + has_edge.");
    println!("  Add petgraph only when you need built-in algorithms.\n");
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Graph Representation Comparison — Rust Benchmarks  ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_construction();
    demo_bfs();
    demo_edge_query();
    demo_memory();
    demo_neighbor_iteration();
    demo_comparison_table();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Comparison complete — Vec<Vec<>> is the default!   ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 10. Ejercicios

### Ejercicio 1 — Benchmark propio

Implementa las tres representaciones (matriz, lista enlazada, Vec) en C y
mide construcción, BFS, y 10⁵ has_edge queries para $n = 3000$, $m = 15000$.
¿Los resultados coinciden con las predicciones teóricas?

<details><summary>¿Qué resultados esperas?</summary>

- **Construcción**: Vec ≈ Linked ≫ Matriz (la matriz requiere inicializar
  $n^2 = 9 \times 10^6$ celdas a 0, lo que domina para $m$ pequeño).
  Sin embargo, con `calloc` la inicialización puede ser lazy (OS pages),
  así que la matriz puede ser rápida en construcción.
- **BFS**: Vec > Linked ≫ Matriz. La matriz hace $n^2 = 9 \times 10^6$
  comparaciones vs $2m = 30000$ para las listas. Vec es 1.5-3x más rápido
  que Linked por localidad de caché.
- **has_edge**: Matriz ≫ Vec ≈ Linked. Matriz es $O(1)$ = una lectura de
  memoria. Vec y Linked son $O(\deg) \approx O(10)$ en promedio.

</details>

### Ejercicio 2 — Punto de cruce densidad

Encuentra experimentalmente la densidad a la cual BFS es igualmente rápido
en matriz y en Vec. Fija $n = 1000$ y varía $m$ de 1000 a 499500 (grafo
completo).

<details><summary>¿A qué densidad aproximada ocurre el cruce?</summary>

El cruce ocurre típicamente cuando $2m \approx n^2 / c$ donde $c$ refleja la
ventaja de caché del acceso secuencial de la matriz. En la práctica, para
$n = 1000$, el cruce está alrededor de $m \approx 100000$ a $200000$
(densidad $0.2$ a $0.4$). Por debajo de eso, Vec gana porque solo visita
$2m$ entradas; por encima, la matriz no "desperdicia" tanto porque la
mayoría de celdas son no-cero.

</details>

### Ejercicio 3 — CSR en Rust

Implementa una representación **Compressed Sparse Row** (CSR) para grafos
inmutables:

```rust
struct CsrGraph {
    targets: Vec<usize>,     // all neighbors, concatenated
    offsets: Vec<usize>,     // offsets[i]..offsets[i+1] = neighbors of i
}
```

Compara el espacio y tiempo de iteración con `Vec<Vec<usize>>`.

<details><summary>¿Cuánta memoria ahorra CSR?</summary>

`Vec<Vec<usize>>`: $n$ Vecs (24 bytes cada uno = `ptr + len + capacity`) +
datos de vecinos. Total overhead: $24n$ bytes de headers.

CSR: un solo `Vec<usize>` para targets + un `Vec<usize>` para offsets.
Total overhead: $48$ bytes (2 Vec headers) + $8(n+1)$ bytes para offsets.

Para $n = 100000$: Vec<Vec> overhead = 2.4 MB; CSR overhead = 0.8 MB.
Ahorro: ~3x en overhead de metadata. Los datos de vecinos son iguales.

Además, CSR tiene **mejor localidad de caché** porque todos los vecinos
están en un solo array contiguo — no $n$ allocations separadas.

</details>

### Ejercicio 4 — HashSet auxiliar

Añade un `HashSet<(usize, usize)>` a tu `VecGraph` para obtener `has_edge`
en $O(1)$. Mide el overhead de memoria y tiempo de construcción comparado
con `VecGraph` solo.

<details><summary>¿Cuánto overhead añade?</summary>

Un `HashSet<(usize, usize)>` con $m$ aristas (o $2m$ para no dirigido) usa
aproximadamente $2m \times 24$ bytes (dos `usize` + overhead de hashing).
Para $m = 50000$: ~2.4 MB extra. El tiempo de construcción aumenta
~1.5-2x por el hashing de cada arista.

El trade-off vale cuando `has_edge` se llama $\Omega(n)$ veces o más. Si
solo se llama ocasionalmente, el scan lineal $O(\deg)$ sobre el Vec es
suficiente.

</details>

### Ejercicio 5 — Grafos dinámicos

Implementa una secuencia de operaciones: 1000 inserciones de aristas, luego
alternancia de 500 eliminaciones y 500 inserciones, luego BFS. Compara las
tres representaciones.

<details><summary>¿Cuál representación maneja mejor la mezcla?</summary>

- **Matriz**: inserción $O(1)$, eliminación $O(1)$, BFS $O(n^2)$.
  Excelente para inserciones/eliminaciones, malo para BFS si $n$ es grande.
- **Vec con swap_remove**: inserción $O(1)$, eliminación $O(1)$ swap, BFS
  $O(n+m)$. Buen balance.
- **Linked list**: inserción $O(1)$, eliminación $O(\deg)$ (buscar nodo),
  BFS $O(n+m)$ pero con cache misses.

Para la mezcla de operaciones, **Vec con swap_remove** es el mejor
compromiso: todas las operaciones son $O(1)$ o $O(n+m)$, y tiene buena
localidad de caché.

</details>

### Ejercicio 6 — Tamaño máximo de matriz

¿Cuál es el $n$ máximo para el que puedes crear una matriz de adyacencia
(`int` = 4 bytes) en tu máquina? Prueba incrementando $n$ hasta que `calloc`
falle o el sistema empiece a paginar.

<details><summary>¿Qué n esperas para diferentes cantidades de RAM?</summary>

- 1 GB RAM: $n^2 \times 4 = 10^9 \implies n \approx 15{,}800$.
- 4 GB RAM: $n \approx 31{,}600$.
- 8 GB RAM: $n \approx 44{,}700$.
- 16 GB RAM: $n \approx 63{,}200$.

En la práctica, el límite es menor porque el OS y otros programas usan
memoria. También puede fallar antes por límites del proceso (`ulimit`).
Con una matriz de bits, se puede llegar a $n$ 5-6x mayor.

</details>

### Ejercicio 7 — Conversión matrix → Vec

Implementa `fn mat_to_vec(mat: &MatGraph) -> VecGraph` y
`fn vec_to_mat(vec: &VecGraph) -> MatGraph`. ¿Cuál conversión es más
costosa en tiempo?

<details><summary>¿Cuáles son las complejidades?</summary>

- **mat → vec**: $O(n^2)$. Recorrer toda la matriz, agregar no-ceros a las
  listas. No se puede hacer mejor porque hay que examinar cada celda.
- **vec → mat**: $O(n^2)$ para inicializar la matriz + $O(m)$ para copiar
  aristas. Total $O(n^2)$ dominado por la inicialización.

Ambas son $O(n^2)$, pero `vec → mat` puede ser ligeramente más rápida en
la práctica porque `calloc` de la matriz puede ser lazy (zero pages del OS),
mientras que `mat → vec` debe leer activamente $n^2$ valores.

</details>

### Ejercicio 8 — Representación para grafo bipartito

Un grafo bipartito $K_{p,q}$ tiene $p + q$ vértices y $p \times q$ aristas.
¿Para qué valores de $p, q$ es la matriz más eficiente que Vec? ¿Existe
una representación especializada mejor?

<details><summary>¿Cuál es la representación óptima?</summary>

La matriz gana cuando $pq > (p+q)^2 / 8$, es decir, cuando la densidad
supera $\sim 12.5\%$. Para $K_{p,q}$, la densidad es
$\frac{pq}{\binom{p+q}{2}} = \frac{2pq}{(p+q)(p+q-1)}$.

**Representación especializada**: almacenar solo la **bipartición** (dos
arrays de tamaño $p$ y $q$) y una matriz $p \times q$ en lugar de
$(p+q) \times (p+q)$. Esto ahorra espacio considerable cuando $p \ne q$:
una matriz $p \times q$ usa $pq$ celdas vs $(p+q)^2$ de la matriz completa.

</details>

### Ejercicio 9 — Grafo con 10⁶ vértices

Crea un grafo de un millón de vértices con 3 millones de aristas (tipo
red social dispersa). Mide: (a) tiempo de construcción, (b) BFS completo,
(c) memoria RSS del proceso. Solo Vec es viable aquí.

<details><summary>¿Qué tiempos y memoria esperas?</summary>

- **Memoria**: $n \times 24 + 2m \times 8 \approx 24M + 48M = 72$ MB para
  los datos. Con overhead de Vec capacities (que pueden ser 2x), ~100-150 MB.
- **Construcción**: ~0.5-2 segundos (dominado por allocations y push).
- **BFS**: ~50-200 ms para recorrer $n + 2m = 7 \times 10^6$ entradas.

La matriz necesitaría $10^{12} \times 4 = 4$ TB — completamente imposible.

</details>

### Ejercicio 10 — Diseña tu representación

Diseña una representación de grafo que combine las ventajas de Vec (BFS
rápido) y matriz (has_edge $O(1)$) para un grafo con $n \le 5000$. Pista:
usa una **bit matrix** auxiliar junto con `Vec<Vec<>>`.

<details><summary>¿Cuánta memoria extra cuesta la bit matrix?</summary>

Una bit matrix de $5000 \times 5000$: $\frac{5000^2}{8} = 3{,}125{,}000$
bytes ≈ **3 MB**. Esto es modesto comparado con una int matrix (100 MB).

La estructura híbrida:
- `adj: Vec<Vec<usize>>` — para BFS/DFS, $O(\deg)$ iteración.
- `edge_bits: Vec<u64>` — bit matrix para $O(1)$ has_edge.

Cada `add_edge` actualiza ambas ($O(1)$ amortizado). El espacio total es
$O(n^2/8 + n + m)$ — casi lo mejor de ambos mundos, factible hasta
$n \approx 50000$ ($\sim 300$ MB para bits).

</details>

---

## 11. Resumen final de S01

| Representación | Espacio | Vecinos | Has_edge | Ideal para |
|---------------|:-------:|:-------:|:--------:|-----------|
| **Matriz** | $O(n^2)$ | $O(n)$ | $O(1)$ | Denso, Floyd-Warshall |
| **Lista enlazada** | $O(n+m)$ | $O(\deg)$ | $O(\deg)$ | Libros de texto |
| **Vec/Array dinámico** | $O(n+m)$ | $O(\deg)$ | $O(\deg)$ | **Default**, BFS/DFS |
| **HashMap** | $O(n+m)$ | $O(\deg)$ | $O(\deg)$* | Vértices nombrados |
| **Lista de aristas** | $O(m)$ | $O(m)$ | $O(m)$ | Kruskal, E/S |
| **CSR** | $O(n+m)$ | $O(\deg)$ | $O(\log\deg)$† | Grandes, inmutables |
| **petgraph** | $O(n+m)$ | $O(\deg)$ | $O(\deg)$ | Producción Rust |

*Con `HashSet`: $O(1)$. †Con búsqueda binaria en el array ordenado.

**Recomendación por defecto**: `Vec<Vec<(usize, W)>>` en Rust,
array dinámico de vecinos en C. Cubre el 90% de los casos. Añadir matriz
solo cuando el algoritmo lo requiera.

Con S01 completa, en **S02 — Recorridos** aplicaremos estas representaciones
a BFS, DFS, detección de ciclos y componentes fuertemente conexos.
