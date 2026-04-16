# T01 — Terminología de grafos

## Objetivo

Dominar el vocabulario fundamental de la teoría de grafos: vértices, aristas,
dirección, pesos, grados, caminos, ciclos, conectividad y bipartición.
Este vocabulario es prerequisito para todas las secciones siguientes de C11.

---

## 1. ¿Qué es un grafo?

Un **grafo** $G$ es un par $G = (V, E)$ donde:

- $V$ es un conjunto finito de **vértices** (también llamados *nodos*).
- $E$ es un conjunto de **aristas** (también llamadas *edges* o *arcos*) que
  conectan pares de vértices.

Los grafos modelan **relaciones** entre objetos. Cualquier problema que
involucre conexiones, dependencias, rutas o interacciones se puede representar
como un grafo:

| Dominio | Vértices | Aristas |
|---------|----------|---------|
| Red social | Personas | Amistades |
| Mapa de carreteras | Ciudades | Carreteras |
| Internet | Routers/hosts | Enlaces |
| Dependencias de paquetes | Paquetes | "A depende de B" |
| Circuito eléctrico | Nodos del circuito | Conexiones |
| Compilación | Archivos fuente | `#include` / `import` |
| Scheduling | Tareas | "A debe terminar antes de B" |

La notación estándar usa $|V| = n$ (número de vértices) y $|E| = m$ (número
de aristas). A lo largo de C11 usaremos $n$ y $m$ extensivamente en el
análisis de complejidad.

---

## 2. Grafos dirigidos vs no dirigidos

### Grafo no dirigido

Cada arista es un **par no ordenado** $\{u, v\}$: la conexión es simétrica.
Si $u$ está conectado a $v$, entonces $v$ está conectado a $u$.

```
    A --- B
    |     |
    C --- D
```

Aquí $E = \{\{A,B\}, \{A,C\}, \{B,D\}, \{C,D\}\}$.

Ejemplo real: una red de amistades en Facebook — si A es amigo de B, B es
amigo de A.

### Grafo dirigido (digrafo)

Cada arista es un **par ordenado** $(u, v)$: la conexión tiene dirección. Se
lee "arista de $u$ a $v$" o "$u \to v$".

```
    A → B
    ↑   ↓
    C ← D
```

Aquí $E = \{(A,B), (B,D), (D,C), (C,A)\}$. Notar que $(A,B)$ existe pero
$(B,A)$ no.

Ejemplo real: seguidores en Twitter/X — que A siga a B no implica que B siga
a A.

### Notación

| Concepto | No dirigido | Dirigido |
|----------|:-----------:|:--------:|
| Arista entre $u$ y $v$ | $\{u,v\}$ | $(u,v)$ |
| Máximo de aristas | $\binom{n}{2} = \frac{n(n-1)}{2}$ | $n(n-1)$ |
| Arista $\{u,v\}$ implica $\{v,u\}$ | Sí (misma arista) | No |

Un grafo dirigido puede tener **hasta el doble** de aristas que uno no
dirigido con los mismos vértices, porque $(u,v)$ y $(v,u)$ son aristas
distintas.

---

## 3. Grafos ponderados

Un **grafo ponderado** asigna un valor numérico $w(e)$ a cada arista $e$.
Formalmente, se añade una función de peso $w: E \to \mathbb{R}$.

```
    A --5-- B
    |       |
    2       3
    |       |
    C --7-- D
```

Los pesos representan costos, distancias, capacidades, probabilidades o
cualquier magnitud asociada a la conexión.

| Dominio | Significado del peso |
|---------|---------------------|
| Mapa de carreteras | Distancia en km |
| Red de comunicación | Latencia en ms |
| Red de flujo | Capacidad máxima |
| Grafo de probabilidades | Probabilidad de transición |
| Grafo de costos | Costo monetario |

Un grafo **no ponderado** es equivalente a un grafo donde todas las aristas
tienen peso 1. Muchos algoritmos (BFS, DFS) trabajan sobre grafos no
ponderados; otros (Dijkstra, Bellman-Ford) requieren pesos.

Los pesos pueden ser **negativos** en ciertos contextos (ej. ganancia en vez
de costo). Esto tiene implicaciones algorítmicas importantes: Dijkstra no
funciona con pesos negativos, pero Bellman-Ford sí.

---

## 4. Grado de un vértice

### En grafos no dirigidos

El **grado** de un vértice $v$, denotado $\deg(v)$, es el número de aristas
incidentes a $v$ — es decir, cuántos vecinos tiene.

```
    A --- B --- E
    |   / |
    C --- D

    deg(A) = 2  (conectado a B, C)
    deg(B) = 4  (conectado a A, C, D, E)
    deg(C) = 3  (conectado a A, B, D)
    deg(D) = 2  (conectado a B, C)
    deg(E) = 1  (conectado a B)
```

**Lema del apretón de manos** (*Handshaking lemma*): la suma de todos los
grados es exactamente el doble del número de aristas:

$$\sum_{v \in V} \deg(v) = 2|E|$$

Esto es porque cada arista contribuye 1 al grado de cada uno de sus dos
extremos. Consecuencia inmediata: el número de vértices con grado impar es
siempre **par**.

Un vértice con grado 0 se llama **aislado**. Un vértice con grado 1 se
llama **hoja** (especialmente en árboles).

### En grafos dirigidos

Se distinguen dos tipos de grado:

- **Grado de entrada** $\deg^{-}(v)$ (*in-degree*): número de aristas que
  **llegan** a $v$.
- **Grado de salida** $\deg^{+}(v)$ (*out-degree*): número de aristas que
  **salen** de $v$.

```
    A → B → E
    ↑ ↗ ↓
    C ← D

    deg⁺(A) = 1, deg⁻(A) = 1
    deg⁺(B) = 2, deg⁻(B) = 2
    deg⁺(C) = 2, deg⁻(C) = 1
    deg⁺(D) = 2, deg⁻(D) = 1
    deg⁺(E) = 0, deg⁻(E) = 1
```

La versión dirigida del lema del apretón de manos:

$$\sum_{v \in V} \deg^{+}(v) = \sum_{v \in V} \deg^{-}(v) = |E|$$

Un vértice con $\deg^{-}(v) = 0$ se llama **fuente** (*source*). Un vértice
con $\deg^{+}(v) = 0$ se llama **sumidero** (*sink*). En el ejemplo, $E$ es
un sumidero.

---

## 5. Caminos y ciclos

### Camino (*path*)

Un **camino** de $u$ a $v$ es una secuencia de vértices
$u = v_0, v_1, v_2, \ldots, v_k = v$ donde cada par consecutivo
$(v_{i}, v_{i+1})$ es una arista. La **longitud** del camino es $k$ (número
de aristas).

```
    A --- B --- C --- D
              |
              E

    Camino de A a D: A → B → C → D  (longitud 3)
    Camino de A a E: A → B → C → E  (longitud 3)
```

Un **camino simple** es uno donde ningún vértice se repite.

En un grafo ponderado, el **peso de un camino** es la suma de los pesos de
sus aristas: $w(P) = \sum_{i=0}^{k-1} w(v_i, v_{i+1})$.

El **camino más corto** (*shortest path*) entre $u$ y $v$ es el camino con
menor peso (o menor número de aristas si no hay pesos). Este es el problema
central de S03.

### Ciclo (*cycle*)

Un **ciclo** es un camino donde el vértice inicial y final son el mismo:
$v_0 = v_k$, con $k \ge 1$.

```
    A --- B
    |     |
    C --- D

    Ciclo: A → B → D → C → A  (longitud 4)
```

Un **ciclo simple** no repite vértices (excepto el primero = último).

Un grafo sin ciclos se llama **acíclico**:
- **DAG** (*Directed Acyclic Graph*): grafo dirigido sin ciclos. Fundamental
  para ordenamiento topológico, dependencias, scheduling.
- **Bosque**: grafo no dirigido acíclico (puede tener múltiples componentes).
- **Árbol**: grafo no dirigido acíclico **conexo** (exactamente $n - 1$
  aristas para $n$ vértices).

### Self-loops y multigrafos

Un **self-loop** (bucle) es una arista de un vértice a sí mismo: $(v, v)$.

Un **multigrafo** permite múltiples aristas entre el mismo par de vértices
(aristas paralelas). La mayoría de algoritmos de este curso asumen grafos
**simples** (sin self-loops ni aristas paralelas), salvo que se indique lo
contrario.

---

## 6. Conectividad

### En grafos no dirigidos

Un grafo no dirigido es **conexo** si existe un camino entre **todo** par de
vértices. Si no es conexo, se divide en **componentes conexos** — subgrafos
maximales que sí son conexos.

```
    Componente 1:       Componente 2:
    A --- B             E --- F
    |                   |
    C --- D             G

    3 componentes no (2 mostrados + cada aislado sería otro).
    Aquí hay 2 componentes conexos.
```

El número de componentes conexos se puede calcular con BFS o DFS en
$O(n + m)$. Un grafo conexo tiene exactamente 1 componente.

Un **puente** (*bridge*) es una arista cuya eliminación aumenta el número de
componentes conexos. Un **punto de articulación** (*cut vertex*) es un vértice
cuya eliminación (junto con sus aristas) aumenta el número de componentes.

### En grafos dirigidos

La conectividad es más matizada:

- **Fuertemente conexo** (*strongly connected*): para todo par $u, v$, existe
  camino de $u$ a $v$ **y** de $v$ a $u$.
- **Débilmente conexo** (*weakly connected*): sería conexo si se ignoraran las
  direcciones de las aristas.

```
    A → B → C
    ↑       ↓
    E ← D ←─┘

    Fuertemente conexo: desde cualquier vértice se llega a cualquier otro.
```

Los **componentes fuertemente conexos** (SCC) de un digrafo son los subgrafos
maximales fuertemente conexos. Encontrar SCCs es el tema de S02/T04 (algoritmos
de Kosaraju y Tarjan).

---

## 7. Grafos bipartitos

Un grafo es **bipartito** si sus vértices se pueden dividir en dos conjuntos
disjuntos $U$ y $W$ tales que **toda** arista conecta un vértice de $U$ con
uno de $W$ — nunca dos vértices del mismo conjunto.

```
    Bipartito:              NO bipartito:
    U = {A, C}              A --- B
    W = {B, D}              |     |
                            C --- D
    A --- B                  \   /
    |     |                   \ /
    C --- D                    E
                            (A-B-D-C-A tiene longitud par,
                             pero el triángulo rompe bipartición)
```

**Teorema**: un grafo es bipartito si y solo si **no contiene ciclos de
longitud impar**.

Para verificar si un grafo es bipartito, se hace un BFS/DFS coloreando
vértices con dos colores alternados. Si al explorar una arista se encuentran
dos vértices del mismo color, el grafo no es bipartito.

**Aplicaciones**:
- **Matching**: asignar trabajadores a tareas, donde cada trabajador puede
  hacer ciertas tareas (matching bipartito, S05/T03).
- **Scheduling**: partición de tareas en dos turnos sin conflictos.
- **Detección de conflictos**: verificar si un conjunto de restricciones
  "binarias" es satisfacible (2-coloreo).

---

## 8. Tipos especiales de grafos

| Tipo | Definición | Propiedades |
|------|-----------|-------------|
| **Grafo completo** $K_n$ | Toda pareja de vértices está conectada | $m = \binom{n}{2}$, $\deg(v) = n - 1$ |
| **Árbol** | Conexo y acíclico | $m = n - 1$, camino único entre todo par |
| **Bosque** | Acíclico (no necesariamente conexo) | $m \le n - 1$ |
| **DAG** | Dirigido, acíclico | Admite ordenamiento topológico |
| **Grafo bipartito completo** $K_{p,q}$ | Bipartito, toda arista posible | $m = p \cdot q$ |
| **Grafo planar** | Se puede dibujar sin cruces de aristas | Euler: $n - m + f = 2$ |
| **Grafo disperso** (*sparse*) | $m = O(n)$ o $m \ll n^2$ | Lista de adyacencia eficiente |
| **Grafo denso** | $m = \Theta(n^2)$ | Matriz de adyacencia eficiente |

### Densidad

La **densidad** de un grafo mide qué tan cerca está de ser completo:

$$d = \frac{2|E|}{|V|(|V|-1)}$$

Para un grafo no dirigido, $d \in [0, 1]$. Un grafo con $d > 0.5$ se considera
denso; con $d < 0.1$ se considera disperso. Esta distinción es crucial para
elegir la representación (S01/T02 vs T03).

En la práctica, la mayoría de grafos reales son **dispersos**: redes sociales
($d \approx 10^{-4}$), internet ($d \approx 10^{-6}$), dependencias de
paquetes ($d \approx 10^{-3}$). Esto motiva la preferencia por listas de
adyacencia.

---

## 9. Subgrafos y grafos inducidos

Un **subgrafo** de $G = (V, E)$ es un grafo $G' = (V', E')$ donde
$V' \subseteq V$ y $E' \subseteq E$, con cada arista de $E'$ teniendo ambos
extremos en $V'$.

Un **subgrafo inducido** por $V' \subseteq V$ incluye **todas** las aristas
de $E$ que tienen ambos extremos en $V'$:

$$G[V'] = (V', \{(u,v) \in E : u \in V' \text{ y } v \in V'\})$$

```
    Grafo original:        Subgrafo inducido por {A, B, C}:
    A --- B --- D          A --- B
    |   / |                |   /
    C --- E                C

    (La arista B-D y el vértice D/E se excluyen)
```

Un **clique** es un subgrafo completo: un subconjunto de vértices donde todos
están conectados entre sí. Encontrar el clique máximo es un problema
NP-completo.

---

## 10. Resumen de notación

| Símbolo | Significado |
|---------|------------|
| $G = (V, E)$ | Grafo con vértices $V$ y aristas $E$ |
| $n = \|V\|$ | Número de vértices |
| $m = \|E\|$ | Número de aristas |
| $\deg(v)$ | Grado de $v$ (no dirigido) |
| $\deg^{+}(v)$, $\deg^{-}(v)$ | Grado de salida/entrada (dirigido) |
| $w(u, v)$ | Peso de la arista $(u,v)$ |
| $K_n$ | Grafo completo de $n$ vértices |
| $K_{p,q}$ | Grafo bipartito completo |
| DAG | Directed Acyclic Graph |
| SCC | Strongly Connected Component |

---

## 11. Programa completo en C

```c
/*
 * graph_terminology.c
 *
 * Demonstrates fundamental graph concepts: construction, degree
 * calculation, path finding, cycle detection, connectivity, and
 * bipartiteness checking.
 *
 * Compile: gcc -O2 -o graph_terminology graph_terminology.c
 * Run:     ./graph_terminology
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_V 20

/* =================== ADJACENCY LIST GRAPH ========================= */

typedef struct AdjNode {
    int dest;
    int weight;
    struct AdjNode *next;
} AdjNode;

typedef struct {
    AdjNode *head[MAX_V];
    int n;          /* number of vertices */
    bool directed;
} Graph;

Graph graph_create(int n, bool directed) {
    Graph g;
    g.n = n;
    g.directed = directed;
    for (int i = 0; i < n; i++)
        g.head[i] = NULL;
    return g;
}

void graph_add_edge(Graph *g, int u, int v, int weight) {
    AdjNode *node = malloc(sizeof(AdjNode));
    node->dest = v;
    node->weight = weight;
    node->next = g->head[u];
    g->head[u] = node;

    if (!g->directed) {
        node = malloc(sizeof(AdjNode));
        node->dest = u;
        node->weight = weight;
        node->next = g->head[v];
        g->head[v] = node;
    }
}

void graph_free(Graph *g) {
    for (int i = 0; i < g->n; i++) {
        AdjNode *curr = g->head[i];
        while (curr) {
            AdjNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
        g->head[i] = NULL;
    }
}

void graph_print(const Graph *g, const char *labels) {
    for (int i = 0; i < g->n; i++) {
        printf("  %c:", labels[i]);
        for (AdjNode *curr = g->head[i]; curr; curr = curr->next) {
            if (curr->weight != 1)
                printf(" %c(%d)", labels[curr->dest], curr->weight);
            else
                printf(" %c", labels[curr->dest]);
        }
        printf("\n");
    }
}

/* ========================== DEGREE ================================ */

/*
 * Demo 1: Degree calculation and handshaking lemma verification
 */
void demo_degree(void) {
    printf("=== Demo 1: Degree and Handshaking Lemma ===\n\n");

    /* Undirected graph:
     *   A(0)---B(1)---E(4)
     *   |    / |
     *   C(2)--D(3)
     */
    const char labels[] = "ABCDE";
    Graph g = graph_create(5, false);
    graph_add_edge(&g, 0, 1, 1);  /* A-B */
    graph_add_edge(&g, 0, 2, 1);  /* A-C */
    graph_add_edge(&g, 1, 2, 1);  /* B-C */
    graph_add_edge(&g, 1, 3, 1);  /* B-D */
    graph_add_edge(&g, 1, 4, 1);  /* B-E */
    graph_add_edge(&g, 2, 3, 1);  /* C-D */

    printf("Undirected graph (adjacency list):\n");
    graph_print(&g, labels);

    int sum_deg = 0;
    int num_edges = 6;
    printf("\nDegrees:\n");
    for (int i = 0; i < g.n; i++) {
        int deg = 0;
        for (AdjNode *curr = g.head[i]; curr; curr = curr->next)
            deg++;
        printf("  deg(%c) = %d\n", labels[i], deg);
        sum_deg += deg;
    }
    printf("\nSum of degrees = %d\n", sum_deg);
    printf("2 * |E| = 2 * %d = %d\n", num_edges, 2 * num_edges);
    printf("Handshaking lemma verified: %s\n\n",
           sum_deg == 2 * num_edges ? "YES" : "NO");

    graph_free(&g);
}

/* ========================= DIRECTED =============================== */

/*
 * Demo 2: In-degree and out-degree in a directed graph
 */
void demo_directed_degree(void) {
    printf("=== Demo 2: Directed Graph — In/Out Degree ===\n\n");

    /* Directed graph:
     *   A(0) → B(1) → C(2)
     *   ↑              ↓
     *   E(4) ← D(3) ←─┘
     */
    const char labels[] = "ABCDE";
    Graph g = graph_create(5, true);
    graph_add_edge(&g, 0, 1, 1);  /* A→B */
    graph_add_edge(&g, 1, 2, 1);  /* B→C */
    graph_add_edge(&g, 2, 3, 1);  /* C→D */
    graph_add_edge(&g, 3, 4, 1);  /* D→E */
    graph_add_edge(&g, 4, 0, 1);  /* E→A */

    printf("Directed graph (adjacency list):\n");
    graph_print(&g, labels);

    /* Calculate in-degree and out-degree */
    int in_deg[MAX_V] = {0};
    int out_deg[MAX_V] = {0};

    for (int i = 0; i < g.n; i++) {
        for (AdjNode *curr = g.head[i]; curr; curr = curr->next) {
            out_deg[i]++;
            in_deg[curr->dest]++;
        }
    }

    int sum_in = 0, sum_out = 0;
    printf("\n%-6s  %-10s  %-10s\n", "Vertex", "out-deg", "in-deg");
    for (int i = 0; i < g.n; i++) {
        printf("%-6c  %-10d  %-10d", labels[i], out_deg[i], in_deg[i]);
        if (in_deg[i] == 0) printf("  (source)");
        if (out_deg[i] == 0) printf("  (sink)");
        printf("\n");
        sum_in += in_deg[i];
        sum_out += out_deg[i];
    }
    printf("\nSum of out-degrees = %d = Sum of in-degrees = %d = |E| = 5\n\n",
           sum_out, sum_in);

    graph_free(&g);
}

/* ====================== CONNECTIVITY ============================== */

static void dfs_visit(const Graph *g, int v, bool *visited) {
    visited[v] = true;
    for (AdjNode *curr = g->head[v]; curr; curr = curr->next) {
        if (!visited[curr->dest])
            dfs_visit(g, curr->dest, visited);
    }
}

/*
 * Demo 3: Connected components in an undirected graph
 */
void demo_connectivity(void) {
    printf("=== Demo 3: Connected Components ===\n\n");

    /* Graph with 3 components:
     *   Component 1: A(0)-B(1)-C(2)
     *   Component 2: D(3)-E(4)
     *   Component 3: F(5) (isolated)
     */
    const char labels[] = "ABCDEF";
    Graph g = graph_create(6, false);
    graph_add_edge(&g, 0, 1, 1);  /* A-B */
    graph_add_edge(&g, 1, 2, 1);  /* B-C */
    graph_add_edge(&g, 3, 4, 1);  /* D-E */
    /* F is isolated */

    printf("Undirected graph:\n");
    graph_print(&g, labels);

    bool visited[MAX_V] = {false};
    int num_components = 0;

    printf("\nConnected components:\n");
    for (int i = 0; i < g.n; i++) {
        if (!visited[i]) {
            num_components++;
            printf("  Component %d: {", num_components);
            /* Collect component vertices */
            bool comp_visited[MAX_V] = {false};
            dfs_visit(&g, i, comp_visited);
            bool first = true;
            for (int j = 0; j < g.n; j++) {
                if (comp_visited[j]) {
                    visited[j] = true;
                    if (!first) printf(", ");
                    printf("%c", labels[j]);
                    first = false;
                }
            }
            printf("}\n");
        }
    }
    printf("\nTotal components: %d\n", num_components);
    printf("Graph is %sconnected\n\n",
           num_components == 1 ? "" : "NOT ");

    graph_free(&g);
}

/* ====================== CYCLE DETECTION =========================== */

typedef enum { WHITE, GRAY, BLACK } Color;

static bool dfs_has_cycle(const Graph *g, int v, Color *color,
                          const char *labels) {
    color[v] = GRAY;
    for (AdjNode *curr = g->head[v]; curr; curr = curr->next) {
        if (color[curr->dest] == GRAY) {
            printf("  Back edge: %c → %c (cycle!)\n",
                   labels[v], labels[curr->dest]);
            return true;
        }
        if (color[curr->dest] == WHITE) {
            if (dfs_has_cycle(g, curr->dest, color, labels))
                return true;
        }
    }
    color[v] = BLACK;
    return false;
}

/*
 * Demo 4: Cycle detection in directed graph
 */
void demo_cycle_detection(void) {
    printf("=== Demo 4: Cycle Detection (Directed) ===\n\n");

    const char labels[] = "ABCDE";

    /* DAG (no cycle): A→B→C, A→C, B→D */
    printf("Graph 1 (DAG): A→B→C, A→C, B→D\n");
    Graph dag = graph_create(5, true);
    graph_add_edge(&dag, 0, 1, 1);  /* A→B */
    graph_add_edge(&dag, 1, 2, 1);  /* B→C */
    graph_add_edge(&dag, 0, 2, 1);  /* A→C */
    graph_add_edge(&dag, 1, 3, 1);  /* B→D */

    Color color1[MAX_V];
    for (int i = 0; i < dag.n; i++) color1[i] = WHITE;

    bool has_cycle = false;
    for (int i = 0; i < dag.n; i++) {
        if (color1[i] == WHITE) {
            if (dfs_has_cycle(&dag, i, color1, labels)) {
                has_cycle = true;
                break;
            }
        }
    }
    printf("  Has cycle: %s\n\n", has_cycle ? "YES" : "NO");

    /* Graph with cycle: A→B→C→A */
    printf("Graph 2 (cycle): A→B→C→A, B→D\n");
    Graph cyc = graph_create(5, true);
    graph_add_edge(&cyc, 0, 1, 1);  /* A→B */
    graph_add_edge(&cyc, 1, 2, 1);  /* B→C */
    graph_add_edge(&cyc, 2, 0, 1);  /* C→A (creates cycle!) */
    graph_add_edge(&cyc, 1, 3, 1);  /* B→D */

    Color color2[MAX_V];
    for (int i = 0; i < cyc.n; i++) color2[i] = WHITE;

    has_cycle = false;
    for (int i = 0; i < cyc.n; i++) {
        if (color2[i] == WHITE) {
            if (dfs_has_cycle(&cyc, i, color2, labels)) {
                has_cycle = true;
                break;
            }
        }
    }
    printf("  Has cycle: %s\n\n", has_cycle ? "YES" : "NO");

    graph_free(&dag);
    graph_free(&cyc);
}

/* ====================== BIPARTITENESS ============================= */

/*
 * Demo 5: Bipartiteness check using BFS 2-coloring
 */
void demo_bipartite(void) {
    printf("=== Demo 5: Bipartite Check (2-coloring) ===\n\n");

    const char labels[] = "ABCDEF";

    /* Bipartite: A-B, A-D, C-B, C-D (complete bipartite K2,2 + extras) */
    printf("Graph 1: A-B, A-D, C-B, C-D, E-B, E-D\n");
    Graph bp = graph_create(6, false);
    graph_add_edge(&bp, 0, 1, 1);  /* A-B */
    graph_add_edge(&bp, 0, 3, 1);  /* A-D */
    graph_add_edge(&bp, 2, 1, 1);  /* C-B */
    graph_add_edge(&bp, 2, 3, 1);  /* C-D */
    graph_add_edge(&bp, 4, 1, 1);  /* E-B */
    graph_add_edge(&bp, 4, 3, 1);  /* E-D */

    /* BFS 2-coloring */
    int color_bp[MAX_V];
    for (int i = 0; i < bp.n; i++) color_bp[i] = -1;

    bool is_bipartite = true;
    int queue[MAX_V], front = 0, back = 0;

    for (int start = 0; start < bp.n && is_bipartite; start++) {
        if (color_bp[start] != -1) continue;
        color_bp[start] = 0;
        queue[back++] = start;

        while (front < back && is_bipartite) {
            int u = queue[front++];
            for (AdjNode *curr = bp.head[u]; curr; curr = curr->next) {
                int v = curr->dest;
                if (color_bp[v] == -1) {
                    color_bp[v] = 1 - color_bp[u];
                    queue[back++] = v;
                } else if (color_bp[v] == color_bp[u]) {
                    printf("  Conflict: %c and %c both color %d\n",
                           labels[u], labels[v], color_bp[u]);
                    is_bipartite = false;
                }
            }
        }
    }

    printf("  Bipartite: %s\n", is_bipartite ? "YES" : "NO");
    if (is_bipartite) {
        printf("  Set U = {");
        bool first = true;
        for (int i = 0; i < bp.n; i++) {
            if (color_bp[i] == 0) {
                if (!first) printf(", ");
                printf("%c", labels[i]);
                first = false;
            }
        }
        printf("}\n  Set W = {");
        first = true;
        for (int i = 0; i < bp.n; i++) {
            if (color_bp[i] == 1) {
                if (!first) printf(", ");
                printf("%c", labels[i]);
                first = false;
            }
        }
        printf("}\n");
    }

    /* Non-bipartite: triangle A-B-C-A */
    printf("\nGraph 2: A-B, B-C, C-A (triangle)\n");
    Graph tri = graph_create(3, false);
    graph_add_edge(&tri, 0, 1, 1);  /* A-B */
    graph_add_edge(&tri, 1, 2, 1);  /* B-C */
    graph_add_edge(&tri, 2, 0, 1);  /* C-A */

    int color_tri[MAX_V];
    for (int i = 0; i < tri.n; i++) color_tri[i] = -1;

    is_bipartite = true;
    front = back = 0;
    color_tri[0] = 0;
    queue[back++] = 0;

    while (front < back && is_bipartite) {
        int u = queue[front++];
        for (AdjNode *curr = tri.head[u]; curr; curr = curr->next) {
            int v = curr->dest;
            if (color_tri[v] == -1) {
                color_tri[v] = 1 - color_tri[u];
                queue[back++] = v;
            } else if (color_tri[v] == color_tri[u]) {
                printf("  Conflict: %c and %c both color %d\n",
                       labels[u], labels[v], color_tri[u]);
                is_bipartite = false;
            }
        }
    }
    printf("  Bipartite: %s (odd cycle!)\n\n",
           is_bipartite ? "YES" : "NO");

    graph_free(&bp);
    graph_free(&tri);
}

/* ================== WEIGHTED GRAPH / PATHS ======================== */

/*
 * Demo 6: Weighted graph and path weight calculation
 */
void demo_weighted_paths(void) {
    printf("=== Demo 6: Weighted Graph and Path Costs ===\n\n");

    /*   A --5-- B --3-- C
     *   |              |
     *   2              4
     *   |              |
     *   D -----7----- E
     */
    const char labels[] = "ABCDE";
    Graph g = graph_create(5, false);
    graph_add_edge(&g, 0, 1, 5);   /* A-B: 5 */
    graph_add_edge(&g, 1, 2, 3);   /* B-C: 3 */
    graph_add_edge(&g, 0, 3, 2);   /* A-D: 2 */
    graph_add_edge(&g, 2, 4, 4);   /* C-E: 4 */
    graph_add_edge(&g, 3, 4, 7);   /* D-E: 7 */

    printf("Weighted undirected graph:\n");
    graph_print(&g, labels);

    /* Calculate path weights manually */
    printf("\nPath A → B → C → E:\n");
    printf("  Weight = w(A,B) + w(B,C) + w(C,E) = 5 + 3 + 4 = 12\n");

    printf("Path A → D → E:\n");
    printf("  Weight = w(A,D) + w(D,E) = 2 + 7 = 9\n");

    printf("\nShortest path A → E: via D (cost 9) < via B,C (cost 12)\n");

    /* Graph properties */
    int total_edges = 5;
    int max_edges = g.n * (g.n - 1) / 2;  /* n choose 2 */
    double density = (2.0 * total_edges) / (g.n * (g.n - 1));
    printf("\nGraph statistics:\n");
    printf("  |V| = %d, |E| = %d\n", g.n, total_edges);
    printf("  Max edges (complete) = %d\n", max_edges);
    printf("  Density = %.3f (%s)\n\n",
           density, density > 0.5 ? "dense" : "sparse");

    graph_free(&g);
}

/* ================================================================== */

int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Graph Terminology — C Demonstrations               ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    demo_degree();
    demo_directed_degree();
    demo_connectivity();
    demo_cycle_detection();
    demo_bipartite();
    demo_weighted_paths();

    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  All graph terminology concepts demonstrated!       ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    return 0;
}
```

---

## 12. Programa completo en Rust

```rust
/*
 * graph_terminology.rs
 *
 * Demonstrates fundamental graph terminology with concrete examples.
 *
 * Run: cargo run --release
 */

use std::collections::VecDeque;

/// Simple adjacency list graph
struct Graph {
    adj: Vec<Vec<(usize, i32)>>,  // (destination, weight)
    directed: bool,
    labels: Vec<char>,
}

impl Graph {
    fn new(labels: &[char], directed: bool) -> Self {
        let n = labels.len();
        Graph {
            adj: vec![vec![]; n],
            directed,
            labels: labels.to_vec(),
        }
    }

    fn add_edge(&mut self, u: usize, v: usize, weight: i32) {
        self.adj[u].push((v, weight));
        if !self.directed {
            self.adj[v].push((u, weight));
        }
    }

    fn n(&self) -> usize {
        self.adj.len()
    }

    fn print(&self) {
        for (i, neighbors) in self.adj.iter().enumerate() {
            print!("  {}:", self.labels[i]);
            for &(dest, w) in neighbors {
                if w != 1 {
                    print!(" {}({})", self.labels[dest], w);
                } else {
                    print!(" {}", self.labels[dest]);
                }
            }
            println!();
        }
    }
}

// ── Demo 1: Degree and handshaking lemma ─────────────────────────────

fn demo_degree() {
    println!("=== Demo 1: Degree and Handshaking Lemma ===\n");

    //   A---B---E
    //   | / |
    //   C---D
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 2, 1); // A-C
    g.add_edge(1, 2, 1); // B-C
    g.add_edge(1, 3, 1); // B-D
    g.add_edge(1, 4, 1); // B-E
    g.add_edge(2, 3, 1); // C-D

    println!("Undirected graph:");
    g.print();

    println!("\nDegrees:");
    let mut sum_deg = 0;
    for i in 0..g.n() {
        let deg = g.adj[i].len();
        println!("  deg({}) = {}", g.labels[i], deg);
        sum_deg += deg;
    }

    let num_edges = 6;
    println!("\nSum of degrees = {}", sum_deg);
    println!("2 * |E| = 2 * {} = {}", num_edges, 2 * num_edges);
    println!(
        "Handshaking lemma verified: {}\n",
        if sum_deg == 2 * num_edges { "YES" } else { "NO" }
    );
}

// ── Demo 2: Directed graph — in/out degree ───────────────────────────

fn demo_directed_degree() {
    println!("=== Demo 2: Directed Graph — In/Out Degree ===\n");

    // A→B→C
    // ↑     ↓
    // E←D←──┘
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], true);
    g.add_edge(0, 1, 1); // A→B
    g.add_edge(1, 2, 1); // B→C
    g.add_edge(2, 3, 1); // C→D
    g.add_edge(3, 4, 1); // D→E
    g.add_edge(4, 0, 1); // E→A

    println!("Directed graph:");
    g.print();

    let mut in_deg = vec![0usize; g.n()];
    let mut out_deg = vec![0usize; g.n()];

    for (u, neighbors) in g.adj.iter().enumerate() {
        out_deg[u] = neighbors.len();
        for &(v, _) in neighbors {
            in_deg[v] += 1;
        }
    }

    println!("\n{:<8} {:<10} {:<10} Role", "Vertex", "out-deg", "in-deg");
    let mut sum_in = 0;
    let mut sum_out = 0;
    for i in 0..g.n() {
        let role = match (in_deg[i], out_deg[i]) {
            (0, _) => "source",
            (_, 0) => "sink",
            _ => "",
        };
        println!(
            "{:<8} {:<10} {:<10} {}",
            g.labels[i], out_deg[i], in_deg[i], role
        );
        sum_in += in_deg[i];
        sum_out += out_deg[i];
    }
    println!(
        "\nΣ out-deg = {} = Σ in-deg = {} = |E|\n",
        sum_out, sum_in
    );
}

// ── Demo 3: Connected components ─────────────────────────────────────

fn demo_connectivity() {
    println!("=== Demo 3: Connected Components ===\n");

    // Component 1: A-B-C, Component 2: D-E, Component 3: F (isolated)
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E', 'F'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(1, 2, 1); // B-C
    g.add_edge(3, 4, 1); // D-E

    println!("Undirected graph:");
    g.print();

    let mut visited = vec![false; g.n()];
    let mut num_components = 0;

    println!("\nConnected components:");
    for start in 0..g.n() {
        if visited[start] {
            continue;
        }
        num_components += 1;

        // DFS to find component
        let mut stack = vec![start];
        let mut component = Vec::new();
        while let Some(v) = stack.pop() {
            if visited[v] {
                continue;
            }
            visited[v] = true;
            component.push(g.labels[v]);
            for &(neighbor, _) in &g.adj[v] {
                if !visited[neighbor] {
                    stack.push(neighbor);
                }
            }
        }
        component.sort();
        let comp_str: Vec<String> = component.iter().map(|c| c.to_string()).collect();
        println!("  Component {}: {{{}}}", num_components, comp_str.join(", "));
    }

    println!("\nTotal components: {}", num_components);
    println!(
        "Graph is {}connected\n",
        if num_components == 1 { "" } else { "NOT " }
    );
}

// ── Demo 4: Cycle detection in directed graph ────────────────────────

#[derive(Clone, Copy, PartialEq)]
enum Color {
    White,
    Gray,
    Black,
}

fn dfs_has_cycle(
    g: &Graph,
    v: usize,
    color: &mut [Color],
    back_edge: &mut Option<(usize, usize)>,
) -> bool {
    color[v] = Color::Gray;
    for &(u, _) in &g.adj[v] {
        if color[u] == Color::Gray {
            *back_edge = Some((v, u));
            return true;
        }
        if color[u] == Color::White && dfs_has_cycle(g, u, color, back_edge) {
            return true;
        }
    }
    color[v] = Color::Black;
    false
}

fn demo_cycle_detection() {
    println!("=== Demo 4: Cycle Detection (Directed) ===\n");

    // DAG: A→B→C, A→C, B→D
    println!("Graph 1 (DAG): A→B→C, A→C, B→D");
    let mut dag = Graph::new(&['A', 'B', 'C', 'D'], true);
    dag.add_edge(0, 1, 1);
    dag.add_edge(1, 2, 1);
    dag.add_edge(0, 2, 1);
    dag.add_edge(1, 3, 1);

    let mut color = vec![Color::White; dag.n()];
    let mut back_edge = None;
    let mut has_cycle = false;
    for i in 0..dag.n() {
        if color[i] == Color::White && dfs_has_cycle(&dag, i, &mut color, &mut back_edge) {
            has_cycle = true;
            break;
        }
    }
    println!("  Has cycle: {}\n", if has_cycle { "YES" } else { "NO" });

    // Graph with cycle: A→B→C→A
    println!("Graph 2 (cycle): A→B→C→A, B→D");
    let mut cyc = Graph::new(&['A', 'B', 'C', 'D'], true);
    cyc.add_edge(0, 1, 1);
    cyc.add_edge(1, 2, 1);
    cyc.add_edge(2, 0, 1); // cycle!
    cyc.add_edge(1, 3, 1);

    let mut color = vec![Color::White; cyc.n()];
    let mut back_edge = None;
    let mut has_cycle = false;
    for i in 0..cyc.n() {
        if color[i] == Color::White && dfs_has_cycle(&cyc, i, &mut color, &mut back_edge) {
            has_cycle = true;
            break;
        }
    }
    if let Some((u, v)) = back_edge {
        println!("  Back edge: {} → {}", cyc.labels[u], cyc.labels[v]);
    }
    println!("  Has cycle: {}\n", if has_cycle { "YES" } else { "NO" });
}

// ── Demo 5: Bipartite check ─────────────────────────────────────────

fn demo_bipartite() {
    println!("=== Demo 5: Bipartite Check (2-coloring) ===\n");

    // Bipartite: K_{3,2} → {A,C,E} vs {B,D}
    println!("Graph 1: A-B, A-D, C-B, C-D, E-B, E-D");
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], false);
    g.add_edge(0, 1, 1); // A-B
    g.add_edge(0, 3, 1); // A-D
    g.add_edge(2, 1, 1); // C-B
    g.add_edge(2, 3, 1); // C-D
    g.add_edge(4, 1, 1); // E-B
    g.add_edge(4, 3, 1); // E-D

    check_bipartite(&g);

    // Non-bipartite: triangle
    println!("Graph 2: A-B, B-C, C-A (triangle)");
    let mut tri = Graph::new(&['A', 'B', 'C'], false);
    tri.add_edge(0, 1, 1);
    tri.add_edge(1, 2, 1);
    tri.add_edge(2, 0, 1);

    check_bipartite(&tri);
}

fn check_bipartite(g: &Graph) {
    let mut color: Vec<Option<u8>> = vec![None; g.n()];
    let mut is_bipartite = true;

    for start in 0..g.n() {
        if color[start].is_some() {
            continue;
        }
        color[start] = Some(0);
        let mut queue = VecDeque::new();
        queue.push_back(start);

        while let Some(u) = queue.pop_front() {
            for &(v, _) in &g.adj[u] {
                match color[v] {
                    None => {
                        color[v] = Some(1 - color[u].unwrap());
                        queue.push_back(v);
                    }
                    Some(c) if c == color[u].unwrap() => {
                        println!(
                            "  Conflict: {} and {} both color {}",
                            g.labels[u], g.labels[v], c
                        );
                        is_bipartite = false;
                    }
                    _ => {}
                }
            }
        }
    }

    println!(
        "  Bipartite: {}",
        if is_bipartite { "YES" } else { "NO" }
    );
    if is_bipartite {
        let set_u: Vec<char> = (0..g.n())
            .filter(|&i| color[i] == Some(0))
            .map(|i| g.labels[i])
            .collect();
        let set_w: Vec<char> = (0..g.n())
            .filter(|&i| color[i] == Some(1))
            .map(|i| g.labels[i])
            .collect();
        println!("  Set U = {:?}", set_u);
        println!("  Set W = {:?}", set_w);
    }
    println!();
}

// ── Demo 6: Weighted graph and paths ─────────────────────────────────

fn demo_weighted_paths() {
    println!("=== Demo 6: Weighted Graph and Path Costs ===\n");

    //   A --5-- B --3-- C
    //   |               |
    //   2               4
    //   |               |
    //   D ------7------ E
    let mut g = Graph::new(&['A', 'B', 'C', 'D', 'E'], false);
    g.add_edge(0, 1, 5); // A-B: 5
    g.add_edge(1, 2, 3); // B-C: 3
    g.add_edge(0, 3, 2); // A-D: 2
    g.add_edge(2, 4, 4); // C-E: 4
    g.add_edge(3, 4, 7); // D-E: 7

    println!("Weighted undirected graph:");
    g.print();

    println!("\nPath A → B → C → E:");
    println!("  Weight = 5 + 3 + 4 = 12");
    println!("Path A → D → E:");
    println!("  Weight = 2 + 7 = 9");
    println!("\nShortest path A → E: via D (9) < via B,C (12)");

    let num_edges = 5;
    let max_edges = g.n() * (g.n() - 1) / 2;
    let density = (2.0 * num_edges as f64) / (g.n() * (g.n() - 1)) as f64;
    println!("\nGraph statistics:");
    println!("  |V| = {}, |E| = {}", g.n(), num_edges);
    println!("  Max edges (complete) = {}", max_edges);
    println!(
        "  Density = {:.3} ({})\n",
        density,
        if density > 0.5 { "dense" } else { "sparse" }
    );
}

// ── Demo 7: Graph types showcase ─────────────────────────────────────

fn demo_graph_types() {
    println!("=== Demo 7: Special Graph Types ===\n");

    // Complete graph K5
    let labels5: Vec<char> = (0..5).map(|i| (b'A' + i) as char).collect();
    let mut k5 = Graph::new(&labels5, false);
    for i in 0..5 {
        for j in (i + 1)..5 {
            k5.add_edge(i, j, 1);
        }
    }
    let k5_edges: usize = k5.adj.iter().map(|a| a.len()).sum::<usize>() / 2;
    println!("Complete graph K5:");
    println!("  |V| = 5, |E| = {} (expected {})", k5_edges, 5 * 4 / 2);

    // Tree (n-1 edges, connected, acyclic)
    let mut tree = Graph::new(&['A', 'B', 'C', 'D', 'E', 'F', 'G'], false);
    tree.add_edge(0, 1, 1); // A-B
    tree.add_edge(0, 2, 1); // A-C
    tree.add_edge(1, 3, 1); // B-D
    tree.add_edge(1, 4, 1); // B-E
    tree.add_edge(2, 5, 1); // C-F
    tree.add_edge(2, 6, 1); // C-G
    let tree_edges: usize = tree.adj.iter().map(|a| a.len()).sum::<usize>() / 2;
    println!("\nTree:");
    println!("  |V| = {}, |E| = {} (= n-1 = {})", tree.n(), tree_edges, tree.n() - 1);
    tree.print();

    // Leaves (degree 1)
    let leaves: Vec<char> = (0..tree.n())
        .filter(|&i| tree.adj[i].len() == 1)
        .map(|i| tree.labels[i])
        .collect();
    println!("  Leaves (deg=1): {:?}", leaves);

    // DAG
    println!("\nDAG (build system dependencies):");
    let mut dag = Graph::new(&['M', 'L', 'U', 'T', 'B'], true);
    // M=main, L=lib, U=utils, T=tests, B=build
    dag.add_edge(4, 0, 1); // build→main
    dag.add_edge(4, 3, 1); // build→tests
    dag.add_edge(0, 1, 1); // main→lib
    dag.add_edge(0, 2, 1); // main→utils
    dag.add_edge(1, 2, 1); // lib→utils
    dag.add_edge(3, 1, 1); // tests→lib
    dag.print();
    println!("  (Topological order would give build order)\n");
}

// ── Demo 8: Density analysis ─────────────────────────────────────────

fn demo_density() {
    println!("=== Demo 8: Density of Real-World Graph Sizes ===\n");

    let examples = [
        ("Small social (20 people)", 20, 50),
        ("Class schedule (30 slots)", 30, 45),
        ("City map (1000 intersections)", 1000, 3000),
        ("Web graph (1M pages)", 1_000_000, 10_000_000),
        ("Facebook (~3B users)", 3_000_000_000u64, 200_000_000_000u64),
    ];

    println!("{:<35} {:>12} {:>15} {:>12}", "Graph", "|V|", "|E|", "Density");
    println!("{}", "-".repeat(78));
    for (name, n, m) in &examples {
        let max_edges = (*n as f64) * (*n as f64 - 1.0) / 2.0;
        let density = 2.0 * *m as f64 / ((*n as f64) * (*n as f64 - 1.0));
        let density_str = if density < 0.001 {
            format!("{:.2e}", density)
        } else {
            format!("{:.4}", density)
        };
        println!(
            "{:<35} {:>12} {:>15} {:>12}",
            name,
            format_num(*n),
            format_num(*m),
            density_str
        );
    }
    println!("\nConclusion: most real-world graphs are VERY sparse.\n");
}

fn format_num(n: u64) -> String {
    if n >= 1_000_000_000 {
        format!("{:.1}B", n as f64 / 1e9)
    } else if n >= 1_000_000 {
        format!("{:.1}M", n as f64 / 1e6)
    } else if n >= 1_000 {
        format!("{:.1}K", n as f64 / 1e3)
    } else {
        format!("{}", n)
    }
}

// ═════════════════════════════════════════════════════════════════════

fn main() {
    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  Graph Terminology — Rust Demonstrations            ║");
    println!("╚══════════════════════════════════════════════════════╝\n");

    demo_degree();
    demo_directed_degree();
    demo_connectivity();
    demo_cycle_detection();
    demo_bipartite();
    demo_weighted_paths();
    demo_graph_types();
    demo_density();

    println!("╔══════════════════════════════════════════════════════╗");
    println!("║  All graph terminology concepts demonstrated!       ║");
    println!("╚══════════════════════════════════════════════════════╝");
}
```

---

## 13. Ejercicios

### Ejercicio 1 — Handshaking lemma

Dado un grafo no dirigido con 7 vértices y grados $[3, 3, 2, 2, 4, 1, 1]$,
¿cuántas aristas tiene? ¿Es posible un grafo con grados $[3, 3, 3, 2, 2]$?

<details><summary>¿Cuántas aristas tiene el primer grafo?</summary>

Suma de grados $= 3+3+2+2+4+1+1 = 16$. Por el lema del apretón de manos,
$|E| = 16/2 = 8$ aristas.

Para el segundo: suma $= 3+3+3+2+2 = 13$. Como 13 es impar, no puede
existir tal grafo — la suma de grados siempre es par.

</details>

### Ejercicio 2 — Dirigido vs no dirigido

Construye un grafo dirigido de 5 vértices donde el vértice $A$ es fuente
(in-degree 0) y el vértice $E$ es sumidero (out-degree 0). Verifica que
$\sum \deg^{+} = \sum \deg^{-} = |E|$.

<details><summary>¿Cuántas aristas mínimas necesita el grafo para que exista camino de A a E?</summary>

Al menos 4 aristas para un camino simple $A \to B \to C \to D \to E$
(necesitas pasar por los 3 vértices intermedios si el camino es hamiltoniano).
Pero basta 1 arista $A \to E$ si no hay restricción sobre los intermedios.
Para que $A$ sea fuente y $E$ sumidero con **todos** los vértices alcanzables,
se necesitan al menos 4 aristas en un camino lineal.

</details>

### Ejercicio 3 — Componentes conexos

Dado un grafo con $n = 10$ vértices y $m = 6$ aristas, ¿cuál es el número
mínimo y máximo posible de componentes conexos?

<details><summary>¿Cuáles son los extremos?</summary>

**Mínimo**: concentrar todas las aristas en un solo componente. Un componente
conexo de $k$ vértices necesita al menos $k - 1$ aristas (árbol). Con 6
aristas se puede conectar hasta 7 vértices (árbol de 7 nodos). Los 3
restantes son aislados → mínimo = $1 + 3 = 4$ componentes.

**Máximo**: distribuir aristas para no conectar nada nuevo... pero cada
arista conecta 2 vértices ya (posiblemente) conectados. Si usamos self-loops
(en un multigrafo) o aristas paralelas, podríamos mantener 10. Pero en un
grafo simple, cada arista reduce componentes en al menos 0 (si ambos extremos
ya estaban en el mismo componente). Máximo con grafo simple: poner las 6
aristas en pares → 3 pares (3 componentes de 2) + 4 aislados = 7 componentes.
O 2 triángulos (6 aristas, 6 vértices) + 4 aislados = 6. El máximo es
crear 3 componentes de tamaño 2 (3 aristas) y luego 3 aristas más dentro de
esos componentes → sigue siendo 3+4=7. Realmente: cada arista nueva en un
grafo simple sobre vértices aislados reduce el número de componentes en
exactamente 1. Empezando con 10 componentes y 6 aristas, si cada arista
conecta dos componentes distintos: $10 - 6 = 4$. Si algunas aristas conectan
vértices ya en el mismo componente, el resultado es mayor. Máximo = colocar
las 6 aristas todas dentro de un solo componente (ej. $K_4$ tiene 6
aristas, 4 vértices) → $1 + 6 = 7$ componentes.

</details>

### Ejercicio 4 — Ciclo o DAG

Determina si los siguientes grafos dirigidos tienen ciclos:
- $E = \{(A,B), (B,C), (C,D), (D,A)\}$
- $E = \{(A,B), (A,C), (B,D), (C,D)\}$
- $E = \{(A,B), (B,C), (A,C), (C,B)\}$

<details><summary>¿Cuáles tienen ciclo?</summary>

1. **Sí**: $A \to B \to C \to D \to A$ es un ciclo de longitud 4.
2. **No**: es un DAG (diamante). No hay camino de $D$ a ningún otro que
   eventualmente vuelva a $D$.
3. **Sí**: $B \to C \to B$ es un ciclo de longitud 2.

</details>

### Ejercicio 5 — Bipartito

¿Son bipartitos los siguientes grafos no dirigidos?
- Ciclo de 4 vértices: $A-B-C-D-A$
- Ciclo de 5 vértices: $A-B-C-D-E-A$
- Estrella de 5 puntas: $A-B$, $A-C$, $A-D$, $A-E$, $A-F$

<details><summary>¿Cuáles son bipartitos y por qué?</summary>

1. **Sí**: ciclo par (longitud 4). Partición: $U = \{A, C\}$, $W = \{B, D\}$.
2. **No**: ciclo impar (longitud 5). No se puede 2-colorear.
3. **Sí**: estrella. Partición: $U = \{A\}$, $W = \{B, C, D, E, F\}$. Toda
   arista conecta $A$ (en $U$) con un vértice de $W$.

Regla general: un grafo es bipartito $\iff$ no tiene ciclos de longitud impar.

</details>

### Ejercicio 6 — Densidad

Calcula la densidad de:
- $K_{10}$ (grafo completo de 10 vértices).
- Un árbol de 100 vértices.
- Un grafo con 1000 vértices y 5000 aristas.

<details><summary>¿Cuáles son las densidades?</summary>

- $K_{10}$: $d = \frac{2 \cdot 45}{10 \cdot 9} = 1.0$ (por definición, completo = densidad 1).
- Árbol de 100: $d = \frac{2 \cdot 99}{100 \cdot 99} = \frac{198}{9900} = 0.02$ (muy disperso).
- 1000 vértices, 5000 aristas: $d = \frac{10000}{999000} \approx 0.01$ (disperso).

</details>

### Ejercicio 7 — Grafo ponderado

Dado el siguiente grafo ponderado no dirigido, encuentra **todos** los
caminos simples de $A$ a $E$ y sus pesos:

```
    A --2-- B --5-- E
    |       |       |
    3       1       4
    |       |       |
    C --6-- D --8-- F
```

<details><summary>¿Cuántos caminos simples hay y cuál es el más corto?</summary>

Caminos simples de $A$ a $E$:
1. $A \to B \to E$: $2 + 5 = 7$
2. $A \to B \to D \to E$ (si hay arista D-E... no la hay directamente, hay D-F-E).
   Revisando: $A \to B \to D \to F \to E$: $2 + 1 + 8 + 4 = 15$
3. $A \to C \to D \to B \to E$: $3 + 6 + 1 + 5 = 15$
4. $A \to C \to D \to F \to E$: $3 + 6 + 8 + 4 = 21$

El camino más corto es $A \to B \to E$ con peso **7**.

</details>

### Ejercicio 8 — Implementación de Graph en C

Implementa una función `graph_remove_edge(Graph *g, int u, int v)` que
elimine una arista de la lista de adyacencia. Para grafos no dirigidos, debe
eliminar la arista en ambas direcciones.

<details><summary>¿Cuál es la complejidad?</summary>

$O(\deg(u) + \deg(v))$ para grafo no dirigido (recorrer la lista de $u$ para
encontrar $v$, y la lista de $v$ para encontrar $u$). Para dirigido,
$O(\deg(u))$ solamente. En el peor caso (grafo completo), $O(n)$.

</details>

### Ejercicio 9 — Grafo transpuesto

El **grafo transpuesto** $G^T$ de un grafo dirigido $G$ invierte todas las
aristas: si $(u,v) \in E$, entonces $(v,u) \in E^T$. Implementa
`graph_transpose(const Graph *g)` que retorne un nuevo grafo con las aristas
invertidas.

<details><summary>¿Para qué sirve el grafo transpuesto?</summary>

El algoritmo de **Kosaraju** para encontrar componentes fuertemente conexos
(S02/T04) requiere:
1. DFS sobre $G$ para obtener orden de finalización.
2. DFS sobre $G^T$ en orden inverso de finalización.

El transpuesto tiene la propiedad de que los SCCs son los mismos en $G$ y
$G^T$ (si hay camino de $u$ a $v$ y de $v$ a $u$ en $G$, lo mismo ocurre
en $G^T$ con las direcciones invertidas).

La complejidad de transponer es $O(n + m)$: recorrer todas las aristas y
agregarlas invertidas.

</details>

### Ejercicio 10 — Modelado

Modela los siguientes problemas como grafos. Para cada uno, indica: ¿qué
son los vértices? ¿Qué son las aristas? ¿Dirigido o no dirigido? ¿Ponderado?

1. Un mapa de vuelos entre ciudades con precio de boleto.
2. Las dependencias entre cursos universitarios (prerequisitos).
3. Una red de amistades en un colegio.
4. El flujo de agua en una red de tuberías.

<details><summary>¿Cómo se modela cada uno?</summary>

1. **Vuelos**: vértices = ciudades, aristas = vuelos, **dirigido** (puede
   haber vuelo A→B pero no B→A), **ponderado** (precio). Posiblemente
   multigrafo (varias aerolíneas).
2. **Prerequisitos**: vértices = cursos, arista $(A,B)$ = "A es prereq de B",
   **dirigido** (DAG — no puede haber ciclos de prereqs), **no ponderado**.
3. **Amistades**: vértices = estudiantes, arista = amistad, **no dirigido**
   (simétrica), **no ponderado** (o ponderado con "fuerza" de amistad).
4. **Tuberías**: vértices = nodos de la red, aristas = tuberías, **dirigido**
   (el agua fluye en una dirección), **ponderado** (capacidad de flujo).
   Este es exactamente un problema de flujo en redes (S05).

</details>

---

## 14. Resumen

La terminología de grafos es el lenguaje con el que se describen problemas de
conectividad, rutas, dependencias y flujo. Los conceptos clave son:

| Concepto | Importancia para C11 |
|----------|---------------------|
| Dirigido vs no dirigido | Determina algoritmos aplicables |
| Ponderado | Dijkstra/Bellman-Ford requieren pesos |
| Grado (in/out) | Análisis de complejidad, fuentes/sumideros |
| Camino / ciclo | Base de BFS, DFS, caminos más cortos |
| Conexo / SCC | Componentes, Kosaraju/Tarjan |
| Bipartito | Matching, 2-coloreo |
| Denso vs disperso | Determina representación (T02 vs T03) |
| DAG | Ordenamiento topológico |

Con este vocabulario establecido, en T02 y T03 veremos cómo **representar**
estos grafos en memoria (matriz y lista de adyacencia).
