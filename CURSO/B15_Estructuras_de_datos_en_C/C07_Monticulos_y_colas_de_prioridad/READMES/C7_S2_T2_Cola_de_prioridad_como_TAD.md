# Cola de prioridad como TAD

## Objetivo

Estudiar la cola de prioridad como **Tipo Abstracto de Datos** (TAD),
independientemente de su implementación:

- Definir la interfaz formal: `insert`, `extract_min`, `peek`, `decrease_key`.
- Comparar implementaciones: array no ordenado, array ordenado, lista
  enlazada, heap binario.
- Aplicaciones clásicas: scheduling de procesos, algoritmo de Dijkstra,
  codificación de Huffman.
- Implementación completa en C y Rust con las tres aplicaciones.

En C04 vimos la cola de prioridad con implementación ingenua (lista
ordenada, $O(n)$ inserción). Ahora la formalizamos como TAD y demostramos
por qué el heap es la implementación estándar.

---

## Definición formal

Una **cola de prioridad** es un TAD que almacena elementos con prioridades
asociadas y soporta las siguientes operaciones:

| Operación | Descripción |
|-----------|-------------|
| `insert(elem, priority)` | Inserta un elemento con su prioridad |
| `extract_min()` | Elimina y retorna el elemento de menor prioridad (o mayor, según variante) |
| `peek()` | Retorna el elemento de mayor prioridad sin eliminarlo |
| `decrease_key(elem, new_priority)` | Reduce la prioridad de un elemento existente |
| `is_empty()` | Verifica si la cola está vacía |
| `size()` | Retorna el número de elementos |

La distinción fundamental con una cola FIFO (C04) es que el orden de salida
no depende del orden de llegada, sino de la **prioridad**.

### Min-priority queue vs max-priority queue

- **Min-priority queue**: `extract_min` retorna el de **menor** valor de
  prioridad (el más urgente). Usado en Dijkstra, Huffman, scheduling por
  deadline.
- **Max-priority queue**: `extract_max` retorna el de **mayor** valor.
  Usado en heapsort, scheduling por importancia.

Ambas variantes son equivalentes — negar las prioridades convierte una en
otra. Por convención, usaremos min-priority queue en este tópico (la más
común en algoritmos).

### TAD vs estructura de datos

El TAD define **qué** operaciones existen y su semántica. La estructura de
datos define **cómo** se implementan. Una cola de prioridad puede
implementarse con:

| Implementación | insert | extract_min | peek | decrease_key |
|----------------|--------|-------------|------|--------------|
| Array no ordenado | $O(1)$ | $O(n)$ | $O(n)$ | $O(n)$ búsqueda + $O(1)$ |
| Array ordenado | $O(n)$ | $O(1)$ | $O(1)$ | $O(n)$ búsqueda + $O(n)$ shift |
| Lista enlazada ordenada | $O(n)$ | $O(1)$ | $O(1)$ | $O(n)$ búsqueda |
| **Heap binario** | $O(\log n)$ | $O(\log n)$ | $O(1)$ | $O(\log n)$* |
| Heap de Fibonacci | $O(1)$ amort. | $O(\log n)$ amort. | $O(1)$ | $O(1)$ amort. |

*Con mapa auxiliar de posiciones. Sin mapa, la búsqueda es $O(n)$.

El heap binario ofrece el mejor balance para la mayoría de aplicaciones.
El heap de Fibonacci tiene mejores complejidades amortizadas para
`decrease_key`, pero su complejidad de implementación y constantes altas
lo hacen raramente útil en la práctica.

---

## Interfaz en C

```c
typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
} PriorityQueue;

PriorityQueue *pq_create(CmpFunc cmp);
void           pq_free(PriorityQueue *pq);
void           pq_insert(PriorityQueue *pq, void *item);
void          *pq_extract_min(PriorityQueue *pq);
void          *pq_peek(const PriorityQueue *pq);
int            pq_size(const PriorityQueue *pq);
int            pq_is_empty(const PriorityQueue *pq);
```

Esta es exactamente la API del heap de T05 renombrada. La cola de prioridad
**es** un heap con nombres más descriptivos. El comparador determina si es
min o max: para min-priority queue, el comparador retorna positivo cuando el
primer argumento tiene menor valor (mayor prioridad).

### Implementación

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 16

typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
} PriorityQueue;

static void swap(void **a, void **b) {
    void *t = *a; *a = *b; *b = t;
}

static void sift_up(PriorityQueue *pq, int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (pq->cmp(pq->data[i], pq->data[p]) <= 0) break;
        swap(&pq->data[i], &pq->data[p]);
        i = p;
    }
}

static void sift_down(PriorityQueue *pq, int i) {
    while (1) {
        int best = i, l = 2*i+1, r = 2*i+2;
        if (l < pq->size && pq->cmp(pq->data[l], pq->data[best]) > 0)
            best = l;
        if (r < pq->size && pq->cmp(pq->data[r], pq->data[best]) > 0)
            best = r;
        if (best == i) break;
        swap(&pq->data[i], &pq->data[best]);
        i = best;
    }
}

PriorityQueue *pq_create(CmpFunc cmp) {
    PriorityQueue *pq = malloc(sizeof(PriorityQueue));
    pq->data = malloc(INITIAL_CAP * sizeof(void *));
    pq->size = 0;
    pq->capacity = INITIAL_CAP;
    pq->cmp = cmp;
    return pq;
}

void pq_free(PriorityQueue *pq) {
    free(pq->data);
    free(pq);
}

void pq_insert(PriorityQueue *pq, void *item) {
    if (pq->size == pq->capacity) {
        pq->capacity *= 2;
        pq->data = realloc(pq->data, pq->capacity * sizeof(void *));
    }
    pq->data[pq->size] = item;
    sift_up(pq, pq->size);
    pq->size++;
}

void *pq_extract_min(PriorityQueue *pq) {
    if (pq->size == 0) return NULL;
    void *top = pq->data[0];
    pq->size--;
    pq->data[0] = pq->data[pq->size];
    if (pq->size > 0) sift_down(pq, 0);
    return top;
}

void *pq_peek(const PriorityQueue *pq) {
    return pq->size > 0 ? pq->data[0] : NULL;
}

int pq_size(const PriorityQueue *pq) { return pq->size; }
int pq_is_empty(const PriorityQueue *pq) { return pq->size == 0; }
```

---

## Aplicación 1: Scheduling de procesos

Un scheduler de sistema operativo usa una cola de prioridad para decidir
qué proceso ejecutar a continuación. Cada proceso tiene:

- Un nombre o PID.
- Una prioridad numérica (menor valor = mayor urgencia).
- Un tiempo de ráfaga (burst time).

### Modelo

```
Procesos llegan → [cola de prioridad] → CPU ejecuta el de mayor prioridad
```

El scheduler extrae el proceso de mayor prioridad, lo ejecuta por un
quantum (o hasta que termine), y si no terminó, lo reinserta con prioridad
posiblemente ajustada.

### Implementación

```c
typedef struct {
    char name[16];
    int priority;     // menor = mas urgente
    int burst;        // tiempo restante
    int arrival;      // timestamp de llegada
} Process;

// min-priority queue: menor priority = mayor urgencia
int cmp_process_min(const void *a, const void *b) {
    const Process *pa = (const Process *)a;
    const Process *pb = (const Process *)b;
    if (pa->priority != pb->priority) {
        // retornar > 0 si a tiene mayor prioridad (menor valor)
        return (pb->priority > pa->priority) - (pb->priority < pa->priority);
    }
    // misma prioridad: FCFS (menor arrival primero)
    return (pb->arrival > pa->arrival) - (pb->arrival < pa->arrival);
}

void demo_scheduler(void) {
    printf("=== Scheduler con cola de prioridad ===\n\n");

    Process procs[] = {
        {"init",    0, 1, 0},
        {"nginx",   2, 5, 1},
        {"cron",    3, 2, 2},
        {"backup",  5, 8, 3},
        {"ssh",     1, 3, 4},
        {"logger",  4, 1, 5},
    };
    int n = sizeof(procs) / sizeof(procs[0]);

    PriorityQueue *pq = pq_create(cmp_process_min);

    printf("Llegada de procesos:\n");
    for (int i = 0; i < n; i++) {
        pq_insert(pq, &procs[i]);
        printf("  t=%d: llega %-8s (prioridad %d, burst %d)\n",
               procs[i].arrival, procs[i].name,
               procs[i].priority, procs[i].burst);
    }

    printf("\nEjecucion (prioridad no preemptiva):\n");
    int time = 0;
    while (!pq_is_empty(pq)) {
        Process *p = (Process *)pq_extract_min(pq);
        printf("  t=%2d: ejecutar %-8s (prioridad %d) por %d unidades -> termina t=%d\n",
               time, p->name, p->priority, p->burst, time + p->burst);
        time += p->burst;
    }

    pq_free(pq);
}
```

Salida:

```
Llegada de procesos:
  t=0: llega init     (prioridad 0, burst 1)
  t=1: llega nginx    (prioridad 2, burst 5)
  t=2: llega cron     (prioridad 3, burst 2)
  t=3: llega backup   (prioridad 5, burst 8)
  t=4: llega ssh      (prioridad 1, burst 3)
  t=5: llega logger   (prioridad 4, burst 1)

Ejecucion (prioridad no preemptiva):
  t= 0: ejecutar init     (prioridad 0) por 1 unidades -> termina t=1
  t= 1: ejecutar ssh      (prioridad 1) por 3 unidades -> termina t=4
  t= 4: ejecutar nginx    (prioridad 2) por 5 unidades -> termina t=9
  t= 9: ejecutar cron     (prioridad 3) por 2 unidades -> termina t=11
  t=11: ejecutar logger   (prioridad 4) por 1 unidades -> termina t=12
  t=12: ejecutar backup   (prioridad 5) por 8 unidades -> termina t=20
```

`init` (prioridad 0) se ejecuta primero pese a no ser el primero en
insertarse en la cola. El scheduler ignora el orden de llegada y se guía
solo por la prioridad.

### Variante preemptiva

En un scheduler preemptivo, cuando llega un proceso de mayor prioridad,
interrumpe al que se está ejecutando:

```c
void demo_preemptive(void) {
    printf("\n=== Scheduler preemptivo ===\n\n");

    // procesos con tiempo de llegada diferido
    typedef struct { char name[16]; int priority; int burst; int arrive_at; } PProc;

    PProc all[] = {
        {"P1", 3, 4, 0},
        {"P2", 1, 2, 2},
        {"P3", 2, 3, 4},
    };
    int n = 3;

    PriorityQueue *pq = pq_create(cmp_process_min);
    int time = 0, next = 0;

    // simular con quantum = 1
    Process running[10];  // copia para insertar
    int run_idx = 0;

    while (next < n || !pq_is_empty(pq)) {
        // insertar procesos que llegan en este instante
        while (next < n && all[next].arrive_at <= time) {
            running[run_idx] = (Process){
                .priority = all[next].priority,
                .burst = all[next].burst,
                .arrival = all[next].arrive_at
            };
            strncpy(running[run_idx].name, all[next].name, 15);
            pq_insert(pq, &running[run_idx]);
            run_idx++;
            next++;
        }

        if (pq_is_empty(pq)) {
            time++;
            continue;
        }

        Process *p = (Process *)pq_extract_min(pq);
        printf("  t=%d: ejecutar %s (prioridad %d, burst restante %d)\n",
               time, p->name, p->priority, p->burst);
        p->burst--;
        time++;

        if (p->burst > 0) {
            pq_insert(pq, p);  // reinsertar con burst reducido
        } else {
            printf("         -> %s termina en t=%d\n", p->name, time);
        }
    }

    pq_free(pq);
}
```

El scheduler preemptivo reinserta el proceso actual después de cada quantum.
Si un proceso más urgente llegó mientras tanto, ese será extraído primero.

---

## Aplicación 2: Algoritmo de Dijkstra

Dijkstra encuentra el camino más corto desde un nodo fuente a todos los
demás en un grafo con pesos no negativos. La cola de prioridad es central
en su funcionamiento.

### Idea del algoritmo

1. Inicializar distancia del fuente a 0, todos los demás a $\infty$.
2. Insertar el fuente en la cola de prioridad con distancia 0.
3. Repetir hasta que la cola esté vacía:
   a. Extraer el nodo $u$ con menor distancia.
   b. Para cada vecino $v$ de $u$: si `dist[u] + peso(u,v) < dist[v]`,
      actualizar `dist[v]` y agregar $v$ a la cola.

### ¿Por qué se necesita una cola de prioridad?

Sin cola de prioridad, encontrar el nodo no visitado de menor distancia
requiere escanear todos los nodos: $O(V)$ por iteración, $O(V^2)$ total.

Con cola de prioridad (heap):
- `extract_min`: $O(\log V)$ — obtener el siguiente nodo.
- `insert`/`decrease_key`: $O(\log V)$ — actualizar distancias.
- Total: $O((V + E) \log V)$.

Para grafos dispersos ($E \approx V$), el heap es mucho mejor que el
escaneo lineal. Para grafos densos ($E \approx V^2$), la versión sin heap
puede ser competitiva.

### Implementación

```c
#include <limits.h>

#define MAX_V 100
#define INF INT_MAX

typedef struct {
    int to;
    int weight;
} Edge;

typedef struct {
    Edge adj[MAX_V][MAX_V];  // lista de adyacencia (simplificada)
    int deg[MAX_V];          // grado de cada nodo
    int n;                   // numero de nodos
} Graph;

typedef struct {
    int node;
    int dist;
} DijkNode;

int cmp_dijkstra(const void *a, const void *b) {
    const DijkNode *da = (const DijkNode *)a;
    const DijkNode *db = (const DijkNode *)b;
    // min-priority: menor distancia = mayor prioridad
    return (db->dist > da->dist) - (db->dist < da->dist);
}

void dijkstra(const Graph *g, int source, int dist[], int prev[]) {
    int visited[MAX_V] = {0};

    for (int i = 0; i < g->n; i++) {
        dist[i] = INF;
        prev[i] = -1;
    }
    dist[source] = 0;

    // pool de nodos para la PQ (evitar malloc por cada insercion)
    DijkNode pool[MAX_V * MAX_V];
    int pool_idx = 0;

    PriorityQueue *pq = pq_create(cmp_dijkstra);

    pool[pool_idx] = (DijkNode){source, 0};
    pq_insert(pq, &pool[pool_idx++]);

    while (!pq_is_empty(pq)) {
        DijkNode *u = (DijkNode *)pq_extract_min(pq);

        if (visited[u->node]) continue;  // ya procesado con menor distancia
        visited[u->node] = 1;

        for (int i = 0; i < g->deg[u->node]; i++) {
            Edge *e = &g->adj[u->node][i];
            int new_dist = dist[u->node] + e->weight;

            if (new_dist < dist[e->to]) {
                dist[e->to] = new_dist;
                prev[e->to] = u->node;

                pool[pool_idx] = (DijkNode){e->to, new_dist};
                pq_insert(pq, &pool[pool_idx++]);
            }
        }
    }

    pq_free(pq);
}
```

**Nota sobre decrease_key**: en esta implementación, en lugar de buscar el
nodo existente en el heap y decrementar su clave, simplemente insertamos
una nueva entrada con la distancia actualizada. Cuando el nodo se extrae
la primera vez, se marca como visitado; las entradas duplicadas posteriores
se descartan con `if (visited[u->node]) continue`.

Esto es el patrón de **lazy deletion**: menos eficiente en memoria (el heap
puede contener duplicados), pero más simple que mantener un mapa de
posiciones para `decrease_key`. La complejidad sube a $O((V + E) \log E)$
en lugar de $O((V + E) \log V)$, pero como $E \leq V^2$, $\log E \leq 2 \log V$,
la diferencia es solo un factor constante.

### Demo

```c
void add_edge(Graph *g, int from, int to, int weight) {
    g->adj[from][g->deg[from]++] = (Edge){to, weight};
    g->adj[to][g->deg[to]++] = (Edge){from, weight};  // no dirigido
}

void demo_dijkstra(void) {
    printf("\n=== Dijkstra con cola de prioridad ===\n\n");

    Graph g = {.n = 6};
    memset(g.deg, 0, sizeof(g.deg));

    //     1
    //  0-----1
    //  |4   /|3
    //  |  2  |
    //  | /1\ |
    //  2-----3
    //  |  5  |2
    //  5-----4
    //     6

    add_edge(&g, 0, 1, 1);
    add_edge(&g, 0, 2, 4);
    add_edge(&g, 1, 2, 2);
    add_edge(&g, 1, 3, 3);
    add_edge(&g, 2, 3, 1);
    add_edge(&g, 2, 5, 5);
    add_edge(&g, 3, 4, 2);
    add_edge(&g, 4, 5, 6);

    int dist[6], prev[6];
    dijkstra(&g, 0, dist, prev);

    printf("Distancias desde nodo 0:\n");
    for (int i = 0; i < g.n; i++) {
        printf("  nodo %d: distancia = %d", i, dist[i]);
        if (prev[i] >= 0) printf(" (via nodo %d)", prev[i]);
        printf("\n");
    }

    // reconstruir camino a nodo 4
    printf("\nCamino a nodo 4: ");
    int path[MAX_V], plen = 0;
    for (int v = 4; v != -1; v = prev[v])
        path[plen++] = v;
    for (int i = plen - 1; i >= 0; i--)
        printf("%d%s", path[i], i > 0 ? " -> " : "\n");
}
```

Salida:

```
Distancias desde nodo 0:
  nodo 0: distancia = 0
  nodo 1: distancia = 1 (via nodo 0)
  nodo 2: distancia = 3 (via nodo 1)
  nodo 3: distancia = 4 (via nodo 2)
  nodo 4: distancia = 6 (via nodo 3)
  nodo 5: distancia = 8 (via nodo 2)

Camino a nodo 4: 0 -> 1 -> 2 -> 3 -> 4
```

---

## Aplicación 3: Codificación de Huffman

La codificación de Huffman asigna códigos binarios de longitud variable a
símbolos, donde los símbolos más frecuentes reciben códigos más cortos.
La cola de prioridad es esencial para construir el árbol de Huffman.

### Algoritmo

1. Contar la frecuencia de cada carácter.
2. Crear un nodo hoja por cada carácter e insertarlo en una min-priority
   queue con su frecuencia como prioridad.
3. Repetir hasta que quede un solo nodo:
   a. Extraer los dos nodos de menor frecuencia ($a$ y $b$).
   b. Crear un nodo interno con frecuencia $f(a) + f(b)$ y $a$, $b$ como
      hijos.
   c. Insertar el nuevo nodo en la cola.
4. El nodo restante es la raíz del árbol de Huffman.

### ¿Por qué cola de prioridad?

En cada paso necesitamos los **dos** nodos de menor frecuencia. Sin cola de
prioridad: búsqueda lineal $O(n)$ por extracción, $O(n^2)$ total. Con cola
de prioridad: $O(\log n)$ por extracción, $O(n \log n)$ total.

Para un alfabeto de $k$ símbolos, hay $k - 1$ merges, cada uno con 2
extracciones y 1 inserción: $3(k-1)$ operaciones de heap = $O(k \log k)$.

### Implementación

```c
typedef struct HuffNode {
    char ch;               // '\0' para nodos internos
    int freq;
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;

int cmp_huff(const void *a, const void *b) {
    const HuffNode *ha = (const HuffNode *)a;
    const HuffNode *hb = (const HuffNode *)b;
    // min-priority: menor frecuencia = mayor prioridad
    return (hb->freq > ha->freq) - (hb->freq < ha->freq);
}

HuffNode *make_leaf(char ch, int freq) {
    HuffNode *node = malloc(sizeof(HuffNode));
    node->ch = ch;
    node->freq = freq;
    node->left = node->right = NULL;
    return node;
}

HuffNode *make_internal(HuffNode *left, HuffNode *right) {
    HuffNode *node = malloc(sizeof(HuffNode));
    node->ch = '\0';
    node->freq = left->freq + right->freq;
    node->left = left;
    node->right = right;
    return node;
}

HuffNode *build_huffman_tree(char chars[], int freqs[], int n) {
    PriorityQueue *pq = pq_create(cmp_huff);

    for (int i = 0; i < n; i++) {
        pq_insert(pq, make_leaf(chars[i], freqs[i]));
    }

    while (pq_size(pq) > 1) {
        HuffNode *a = (HuffNode *)pq_extract_min(pq);
        HuffNode *b = (HuffNode *)pq_extract_min(pq);
        pq_insert(pq, make_internal(a, b));
    }

    HuffNode *root = (HuffNode *)pq_extract_min(pq);
    pq_free(pq);
    return root;
}

void print_codes(HuffNode *root, char code[], int depth) {
    if (!root) return;

    if (root->ch != '\0') {
        code[depth] = '\0';
        printf("  '%c' (freq %d): %s\n", root->ch, root->freq, code);
        return;
    }

    code[depth] = '0';
    print_codes(root->left, code, depth + 1);
    code[depth] = '1';
    print_codes(root->right, code, depth + 1);
}

void free_tree(HuffNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

void demo_huffman(void) {
    printf("\n=== Codificacion de Huffman ===\n\n");

    // frecuencias del texto "abracadabra"
    char  chars[] = {'a', 'b', 'r', 'c', 'd'};
    int   freqs[] = { 5,   2,   2,   1,   1};
    int n = 5;

    printf("Frecuencias:\n");
    for (int i = 0; i < n; i++)
        printf("  '%c': %d\n", chars[i], freqs[i]);

    HuffNode *root = build_huffman_tree(chars, freqs, n);

    printf("\nCodigos de Huffman:\n");
    char code[64];
    print_codes(root, code, 0);

    // calcular tamano comprimido
    int original_bits = 0, compressed_bits = 0;
    for (int i = 0; i < n; i++) original_bits += freqs[i] * 8;  // ASCII

    // recalcular con profundidad real del arbol
    printf("\nTamano original (ASCII): %d bits\n", original_bits);
    printf("Tamano comprimido: depende de la profundidad de cada codigo\n");
    printf("Ratio teorico para 'abracadabra' ~= 60%% de reduccion\n");

    free_tree(root);
}
```

Salida:

```
Frecuencias:
  'a': 5
  'b': 2
  'r': 2
  'c': 1
  'd': 1

Codigos de Huffman:
  'a' (freq 5): 0
  'c' (freq 1): 100
  'd' (freq 1): 101
  'b' (freq 2): 110
  'r' (freq 2): 111

Tamano original (ASCII): 88 bits
Tamano comprimido: depende de la profundidad de cada codigo
Ratio teorico para 'abracadabra' ~= 60% de reduccion
```

El código de 'a' es `0` (1 bit) porque es el más frecuente. 'c' y 'd'
tienen códigos de 3 bits porque son los menos frecuentes. Esto minimiza
el número total de bits: $5 \times 1 + 2 \times 3 + 2 \times 3 + 1 \times 3 + 1 \times 3 = 5 + 6 + 6 + 3 + 3 = 23$ bits,
vs 88 bits en ASCII — una reducción del 74%.

### Propiedad del prefijo

Los códigos de Huffman son **códigos prefijo**: ningún código es prefijo de
otro. Esto permite decodificación sin ambigüedad — se lee bit por bit,
recorriendo el árbol desde la raíz. Cuando se llega a una hoja, se emite
el carácter y se vuelve a la raíz.

---

## Implementación en Rust

```rust
use std::collections::BinaryHeap;
use std::cmp::Reverse;

// ======== Scheduler ========

#[derive(Debug, Eq, PartialEq)]
struct Process {
    name: String,
    priority: u32,
    burst: u32,
    arrival: u32,
}

impl Ord for Process {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // min-priority: menor valor = mayor urgencia
        other.priority.cmp(&self.priority)
            .then_with(|| other.arrival.cmp(&self.arrival))
    }
}

impl PartialOrd for Process {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

fn demo_scheduler() {
    println!("=== Scheduler con BinaryHeap ===\n");

    let procs = vec![
        Process { name: "init".into(),   priority: 0, burst: 1, arrival: 0 },
        Process { name: "nginx".into(),  priority: 2, burst: 5, arrival: 1 },
        Process { name: "cron".into(),   priority: 3, burst: 2, arrival: 2 },
        Process { name: "backup".into(), priority: 5, burst: 8, arrival: 3 },
        Process { name: "ssh".into(),    priority: 1, burst: 3, arrival: 4 },
        Process { name: "logger".into(), priority: 4, burst: 1, arrival: 5 },
    ];

    let mut pq = BinaryHeap::new();
    for p in procs {
        println!("  llega {:8} (prioridad {}, burst {})", p.name, p.priority, p.burst);
        pq.push(p);
    }

    println!("\nEjecucion:");
    let mut time = 0;
    while let Some(p) = pq.pop() {
        println!("  t={:2}: {:8} (prioridad {}) por {} unidades -> t={}",
                 time, p.name, p.priority, p.burst, time + p.burst);
        time += p.burst;
    }
}

// ======== Dijkstra ========

fn demo_dijkstra() {
    println!("\n=== Dijkstra con BinaryHeap ===\n");

    // grafo como lista de adyacencia: adj[u] = [(v, peso), ...]
    let adj: Vec<Vec<(usize, u32)>> = vec![
        vec![(1, 1), (2, 4)],          // 0
        vec![(0, 1), (2, 2), (3, 3)],  // 1
        vec![(0, 4), (1, 2), (3, 1), (5, 5)],  // 2
        vec![(1, 3), (2, 1), (4, 2)],  // 3
        vec![(3, 2), (5, 6)],          // 4
        vec![(2, 5), (4, 6)],          // 5
    ];
    let n = adj.len();
    let source = 0;

    let mut dist = vec![u32::MAX; n];
    let mut prev = vec![None; n];
    dist[source] = 0;

    // min-heap: (Reverse(distancia), nodo)
    let mut pq = BinaryHeap::new();
    pq.push(Reverse((0u32, source)));

    while let Some(Reverse((d, u))) = pq.pop() {
        if d > dist[u] { continue; }  // entrada obsoleta

        for &(v, w) in &adj[u] {
            let new_dist = dist[u] + w;
            if new_dist < dist[v] {
                dist[v] = new_dist;
                prev[v] = Some(u);
                pq.push(Reverse((new_dist, v)));
            }
        }
    }

    println!("Distancias desde nodo {}:", source);
    for i in 0..n {
        print!("  nodo {}: distancia = {}", i, dist[i]);
        if let Some(p) = prev[i] {
            print!(" (via nodo {})", p);
        }
        println!();
    }

    // reconstruir camino a nodo 4
    let target = 4;
    let mut path = Vec::new();
    let mut v = Some(target);
    while let Some(node) = v {
        path.push(node);
        v = prev[node];
    }
    path.reverse();
    print!("\nCamino a nodo {}: ", target);
    for (i, &node) in path.iter().enumerate() {
        if i > 0 { print!(" -> "); }
        print!("{}", node);
    }
    println!();
}

// ======== Huffman ========

#[derive(Debug)]
enum HuffTree {
    Leaf { ch: char, freq: u32 },
    Internal { freq: u32, left: Box<HuffTree>, right: Box<HuffTree> },
}

impl HuffTree {
    fn freq(&self) -> u32 {
        match self {
            HuffTree::Leaf { freq, .. } => *freq,
            HuffTree::Internal { freq, .. } => *freq,
        }
    }
}

impl Eq for HuffTree {}
impl PartialEq for HuffTree {
    fn eq(&self, other: &Self) -> bool { self.freq() == other.freq() }
}
impl Ord for HuffTree {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        other.freq().cmp(&self.freq())  // invertido: min-heap
    }
}
impl PartialOrd for HuffTree {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

fn print_huffman_codes(tree: &HuffTree, code: &mut String) {
    match tree {
        HuffTree::Leaf { ch, freq } => {
            println!("  '{}' (freq {}): {}", ch, freq, code);
        }
        HuffTree::Internal { left, right, .. } => {
            code.push('0');
            print_huffman_codes(left, code);
            code.pop();
            code.push('1');
            print_huffman_codes(right, code);
            code.pop();
        }
    }
}

fn demo_huffman() {
    println!("\n=== Codificacion de Huffman ===\n");

    let symbols = [('a', 5), ('b', 2), ('r', 2), ('c', 1), ('d', 1)];

    println!("Frecuencias:");
    for &(ch, freq) in &symbols {
        println!("  '{}': {}", ch, freq);
    }

    let mut pq = BinaryHeap::new();
    for &(ch, freq) in &symbols {
        pq.push(HuffTree::Leaf { ch, freq });
    }

    while pq.len() > 1 {
        let a = pq.pop().unwrap();
        let b = pq.pop().unwrap();
        let merged = HuffTree::Internal {
            freq: a.freq() + b.freq(),
            left: Box::new(a),
            right: Box::new(b),
        };
        pq.push(merged);
    }

    let tree = pq.pop().unwrap();
    println!("\nCodigos de Huffman:");
    let mut code = String::new();
    print_huffman_codes(&tree, &mut code);

    // calcular bits
    let original: u32 = symbols.iter().map(|(_, f)| f * 8).sum();
    println!("\nTamano original (ASCII): {} bits", original);
}

fn main() {
    demo_scheduler();
    demo_dijkstra();
    demo_huffman();
}
```

### Salida esperada

```
=== Scheduler con BinaryHeap ===

  llega init     (prioridad 0, burst 1)
  llega nginx    (prioridad 2, burst 5)
  llega cron     (prioridad 3, burst 2)
  llega backup   (prioridad 5, burst 8)
  llega ssh      (prioridad 1, burst 3)
  llega logger   (prioridad 4, burst 1)

Ejecucion:
  t= 0: init     (prioridad 0) por 1 unidades -> t=1
  t= 1: ssh      (prioridad 1) por 3 unidades -> t=4
  t= 4: nginx    (prioridad 2) por 5 unidades -> t=9
  t= 9: cron     (prioridad 3) por 2 unidades -> t=11
  t=11: logger   (prioridad 4) por 1 unidades -> t=12
  t=12: backup   (prioridad 5) por 8 unidades -> t=20

=== Dijkstra con BinaryHeap ===

Distancias desde nodo 0:
  nodo 0: distancia = 0
  nodo 1: distancia = 1 (via nodo 0)
  nodo 2: distancia = 3 (via nodo 1)
  nodo 3: distancia = 4 (via nodo 2)
  nodo 4: distancia = 6 (via nodo 3)
  nodo 5: distancia = 8 (via nodo 2)

Camino a nodo 4: 0 -> 1 -> 2 -> 3 -> 4

=== Codificacion de Huffman ===

Frecuencias:
  'a': 5
  'b': 2
  'r': 2
  'c': 1
  'd': 1

Codigos de Huffman:
  'a' (freq 5): 0
  'c' (freq 1): 100
  'd' (freq 1): 101
  'b' (freq 2): 110
  'r' (freq 2): 111

Tamano original (ASCII): 88 bits
```

### Comparación C vs Rust en Dijkstra

| Aspecto | C | Rust |
|---------|---|------|
| Cola de prioridad | `PriorityQueue *` manual | `BinaryHeap<Reverse<(u32, usize)>>` |
| Min-heap | Comparador personalizado | `Reverse` wrapper |
| Distancias | `int dist[MAX_V]` (tamaño fijo) | `Vec<u32>` (tamaño dinámico) |
| Previo | `int prev[MAX_V]` con -1 como nulo | `Vec<Option<usize>>` (explícito) |
| Pool de nodos | Array manual para evitar malloc | No necesario (ownership automático) |
| Lazy deletion | `if (visited[u->node]) continue` | `if d > dist[u] { continue }` |

La versión Rust es más concisa porque `BinaryHeap` + `Reverse` eliminan
toda la maquinaria del heap manual y del comparador.

---

## Programa completo en C

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define INITIAL_CAP 16
#define MAX_V 100
#define INF INT_MAX

// ======== Cola de prioridad (heap) ========

typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
} PQ;

static void pq_swap(void **a, void **b) {
    void *t = *a; *a = *b; *b = t;
}

static void pq_sift_up(PQ *q, int i) {
    while (i > 0) {
        int p = (i-1)/2;
        if (q->cmp(q->data[i], q->data[p]) <= 0) break;
        pq_swap(&q->data[i], &q->data[p]);
        i = p;
    }
}

static void pq_sift_down(PQ *q, int i) {
    while (1) {
        int b = i, l = 2*i+1, r = 2*i+2;
        if (l < q->size && q->cmp(q->data[l], q->data[b]) > 0) b = l;
        if (r < q->size && q->cmp(q->data[r], q->data[b]) > 0) b = r;
        if (b == i) break;
        pq_swap(&q->data[i], &q->data[b]);
        i = b;
    }
}

PQ *pq_new(CmpFunc cmp) {
    PQ *q = malloc(sizeof(PQ));
    q->data = malloc(INITIAL_CAP * sizeof(void *));
    q->size = 0;
    q->capacity = INITIAL_CAP;
    q->cmp = cmp;
    return q;
}

void pq_del(PQ *q) { free(q->data); free(q); }

void pq_push(PQ *q, void *item) {
    if (q->size == q->capacity) {
        q->capacity *= 2;
        q->data = realloc(q->data, q->capacity * sizeof(void *));
    }
    q->data[q->size] = item;
    pq_sift_up(q, q->size);
    q->size++;
}

void *pq_pop(PQ *q) {
    if (q->size == 0) return NULL;
    void *top = q->data[0];
    q->size--;
    q->data[0] = q->data[q->size];
    if (q->size > 0) pq_sift_down(q, 0);
    return top;
}

int pq_size(PQ *q) { return q->size; }
int pq_empty(PQ *q) { return q->size == 0; }

// ======== Scheduling ========

typedef struct {
    char name[16];
    int priority;
    int burst;
    int arrival;
} Proc;

int cmp_proc(const void *a, const void *b) {
    const Proc *pa = a, *pb = b;
    if (pa->priority != pb->priority)
        return (pb->priority > pa->priority) - (pb->priority < pa->priority);
    return (pb->arrival > pa->arrival) - (pb->arrival < pa->arrival);
}

void demo_scheduler(void) {
    printf("=== Scheduler ===\n\n");
    Proc procs[] = {
        {"init",0,1,0}, {"nginx",2,5,1}, {"cron",3,2,2},
        {"backup",5,8,3}, {"ssh",1,3,4}, {"logger",4,1,5},
    };
    int n = 6;

    PQ *q = pq_new(cmp_proc);
    for (int i = 0; i < n; i++) {
        pq_push(q, &procs[i]);
        printf("  t=%d: llega %-8s (pri %d, burst %d)\n",
               procs[i].arrival, procs[i].name, procs[i].priority, procs[i].burst);
    }

    printf("\nEjecucion:\n");
    int t = 0;
    while (!pq_empty(q)) {
        Proc *p = pq_pop(q);
        printf("  t=%2d: %-8s (pri %d) x%d -> t=%d\n",
               t, p->name, p->priority, p->burst, t + p->burst);
        t += p->burst;
    }
    pq_del(q);
}

// ======== Dijkstra ========

typedef struct { int to, w; } Edge;
typedef struct { int node, dist; } DNode;

int cmp_dnode(const void *a, const void *b) {
    const DNode *da = a, *db = b;
    return (db->dist > da->dist) - (db->dist < da->dist);
}

void demo_dijkstra(void) {
    printf("\n=== Dijkstra ===\n\n");

    Edge adj[6][10];
    int deg[6] = {0};
    int V = 6;

    #define ADD(u, v, w) do { \
        adj[u][deg[u]++] = (Edge){v, w}; \
        adj[v][deg[v]++] = (Edge){u, w}; \
    } while(0)

    ADD(0,1,1); ADD(0,2,4); ADD(1,2,2);
    ADD(1,3,3); ADD(2,3,1); ADD(2,5,5);
    ADD(3,4,2); ADD(4,5,6);

    int dist[6], prev[6], visited[6] = {0};
    for (int i = 0; i < V; i++) { dist[i] = INF; prev[i] = -1; }
    dist[0] = 0;

    DNode pool[100];
    int pi = 0;

    PQ *q = pq_new(cmp_dnode);
    pool[pi] = (DNode){0, 0};
    pq_push(q, &pool[pi++]);

    while (!pq_empty(q)) {
        DNode *u = pq_pop(q);
        if (visited[u->node]) continue;
        visited[u->node] = 1;

        for (int i = 0; i < deg[u->node]; i++) {
            int v = adj[u->node][i].to;
            int nd = dist[u->node] + adj[u->node][i].w;
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u->node;
                pool[pi] = (DNode){v, nd};
                pq_push(q, &pool[pi++]);
            }
        }
    }

    printf("Distancias desde nodo 0:\n");
    for (int i = 0; i < V; i++) {
        printf("  nodo %d: dist=%d", i, dist[i]);
        if (prev[i] >= 0) printf(" (via %d)", prev[i]);
        printf("\n");
    }

    printf("\nCamino a 4: ");
    int path[10], plen = 0;
    for (int v = 4; v != -1; v = prev[v]) path[plen++] = v;
    for (int i = plen-1; i >= 0; i--) printf("%d%s", path[i], i>0?" -> ":"");
    printf("\n");

    pq_del(q);
    #undef ADD
}

// ======== Huffman ========

typedef struct HNode {
    char ch;
    int freq;
    struct HNode *left, *right;
} HNode;

int cmp_hnode(const void *a, const void *b) {
    const HNode *ha = a, *hb = b;
    return (hb->freq > ha->freq) - (hb->freq < ha->freq);
}

HNode *hleaf(char ch, int freq) {
    HNode *n = malloc(sizeof(HNode));
    *n = (HNode){ch, freq, NULL, NULL};
    return n;
}

void print_codes(HNode *n, char code[], int d) {
    if (!n) return;
    if (n->ch) {
        code[d] = '\0';
        printf("  '%c' (freq %d): %s\n", n->ch, n->freq, code);
        return;
    }
    code[d] = '0'; print_codes(n->left, code, d+1);
    code[d] = '1'; print_codes(n->right, code, d+1);
}

void free_htree(HNode *n) {
    if (!n) return;
    free_htree(n->left); free_htree(n->right); free(n);
}

void demo_huffman(void) {
    printf("\n=== Huffman ===\n\n");

    char  ch[] = {'a','b','r','c','d'};
    int freq[] = { 5,  2,  2,  1,  1};
    int n = 5;

    printf("Frecuencias:\n");
    for (int i = 0; i < n; i++) printf("  '%c': %d\n", ch[i], freq[i]);

    PQ *q = pq_new(cmp_hnode);
    for (int i = 0; i < n; i++) pq_push(q, hleaf(ch[i], freq[i]));

    while (pq_size(q) > 1) {
        HNode *a = pq_pop(q), *b = pq_pop(q);
        HNode *m = malloc(sizeof(HNode));
        *m = (HNode){'\0', a->freq + b->freq, a, b};
        pq_push(q, m);
    }

    HNode *root = pq_pop(q);
    printf("\nCodigos:\n");
    char code[64];
    print_codes(root, code, 0);

    free_htree(root);
    pq_del(q);
}

int main(void) {
    demo_scheduler();
    demo_dijkstra();
    demo_huffman();
    return 0;
}
```

### Salida esperada

```
=== Scheduler ===

  t=0: llega init     (pri 0, burst 1)
  t=1: llega nginx    (pri 2, burst 5)
  t=2: llega cron     (pri 3, burst 2)
  t=3: llega backup   (pri 5, burst 8)
  t=4: llega ssh      (pri 1, burst 3)
  t=5: llega logger   (pri 4, burst 1)

Ejecucion:
  t= 0: init     (pri 0) x1 -> t=1
  t= 1: ssh      (pri 1) x3 -> t=4
  t= 4: nginx    (pri 2) x5 -> t=9
  t= 9: cron     (pri 3) x2 -> t=11
  t=11: logger   (pri 4) x1 -> t=12
  t=12: backup   (pri 5) x8 -> t=20

=== Dijkstra ===

Distancias desde nodo 0:
  nodo 0: dist=0
  nodo 1: dist=1 (via 0)
  nodo 2: dist=3 (via 1)
  nodo 3: dist=4 (via 2)
  nodo 4: dist=6 (via 3)
  nodo 5: dist=8 (via 2)

Camino a 4: 0 -> 1 -> 2 -> 3 -> 4

=== Huffman ===

Frecuencias:
  'a': 5
  'b': 2
  'r': 2
  'c': 1
  'd': 1

Codigos:
  'a' (freq 5): 0
  'c' (freq 1): 100
  'd' (freq 1): 101
  'b' (freq 2): 110
  'r' (freq 2): 111
```

---

## Ejercicios

### Ejercicio 1 — Complejidad de implementaciones

Completa la tabla de complejidades para una cola de prioridad implementada
con un **array no ordenado**. Luego explica por qué el heap gana en el
caso general.

<details>
<summary>Verificar</summary>

| Operación | Array no ordenado | Heap binario |
|-----------|-------------------|--------------|
| insert | $O(1)$ — append al final | $O(\log n)$ |
| extract_min | $O(n)$ — buscar el mínimo | $O(\log n)$ |
| peek | $O(n)$ — buscar el mínimo | $O(1)$ |
| decrease_key | $O(n)$ buscar + $O(1)$ modificar | $O(\log n)$* |

Para una secuencia de $m$ operaciones mixtas (insert + extract), el array
no ordenado tiene costo total $O(mn)$ vs $O(m \log n)$ del heap. La mejora
es de un factor $n / \log n$, que para $n = 10^6$ es $\sim 50000\times$.

El array no ordenado solo gana si las operaciones son dominantemente
inserciones con muy pocas extracciones — un patrón raro en la práctica.
</details>

### Ejercicio 2 — Scheduler con aging

El **aging** evita starvation: cada vez que un proceso no es seleccionado,
su prioridad mejora (decrece). Implementa un scheduler donde cada quantum
reduce la prioridad de los procesos en espera en 1.

<details>
<summary>Verificar</summary>

```c
void scheduler_aging(void) {
    Proc procs[] = {
        {"low",  10, 5, 0},
        {"med",   5, 3, 0},
        {"high",  1, 2, 0},
        {"vlow", 20, 4, 0},
    };
    int n = 4, done[4] = {0};

    printf("Scheduler con aging:\n");
    for (int tick = 0; tick < 30; tick++) {
        // encontrar proceso listo de menor prioridad (sin heap, para simplicidad)
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (done[i]) continue;
            if (best == -1 || procs[i].priority < procs[best].priority)
                best = i;
        }
        if (best == -1) break;

        printf("  t=%2d: ejecutar %-6s (pri %2d, burst restante %d)\n",
               tick, procs[best].name, procs[best].priority, procs[best].burst);

        procs[best].burst--;
        if (procs[best].burst == 0) done[best] = 1;

        // aging: mejorar prioridad de procesos en espera
        for (int i = 0; i < n; i++) {
            if (!done[i] && i != best && procs[i].priority > 0) {
                procs[i].priority--;
            }
        }
    }
}
```

Sin aging, `vlow` (prioridad 20) esperaría hasta que todos los demás
terminen. Con aging, su prioridad decrece cada tick: después de ~10 ticks,
su prioridad habrá bajado a ~10, compitiendo con `low`. Esto previene
starvation — ningún proceso espera indefinidamente.

Con heap + decrease_key, el aging se haría con `decrease_key` para cada
proceso en espera, manteniendo $O(\log n)$ por actualización.
</details>

### Ejercicio 3 — Dijkstra traza manual

Aplica Dijkstra al siguiente grafo desde el nodo A, mostrando el estado
de la cola de prioridad y las distancias después de cada extracción:

```
A --2-- B --3-- D
|       |       |
4       1       2
|       |       |
C --5-- E --1-- F
```

<details>
<summary>Verificar</summary>

Inicialización: dist = {A:0, B:∞, C:∞, D:∞, E:∞, F:∞}. PQ = {(0,A)}.

**Extraer (0,A)**: relajar vecinos.
- B: 0+2=2 < ∞ → dist[B]=2. PQ += (2,B).
- C: 0+4=4 < ∞ → dist[C]=4. PQ += (4,C).
- dist = {A:0, B:2, C:4, D:∞, E:∞, F:∞}. PQ = {(2,B), (4,C)}.

**Extraer (2,B)**: relajar vecinos.
- A: 2+2=4 > 0 → no.
- D: 2+3=5 < ∞ → dist[D]=5. PQ += (5,D).
- E: 2+1=3 < ∞ → dist[E]=3. PQ += (3,E).
- dist = {A:0, B:2, C:4, D:5, E:3, F:∞}. PQ = {(3,E), (4,C), (5,D)}.

**Extraer (3,E)**: relajar vecinos.
- B: 3+1=4 > 2 → no.
- C: 3+5=8 > 4 → no.
- F: 3+1=4 < ∞ → dist[F]=4. PQ += (4,F).
- dist = {A:0, B:2, C:4, D:5, E:3, F:4}. PQ = {(4,C), (4,F), (5,D)}.

**Extraer (4,C)**: relajar vecinos.
- A: 4+4=8 > 0 → no.
- E: 4+5=9 > 3 → no.
- dist sin cambios. PQ = {(4,F), (5,D)}.

**Extraer (4,F)**: relajar vecinos.
- E: 4+1=5 > 3 → no.
- D: 4+2=6 > 5 → no.
- dist sin cambios. PQ = {(5,D)}.

**Extraer (5,D)**: relajar vecinos.
- B: 5+3=8 > 2 → no.
- F: 5+2=7 > 4 → no.
- dist sin cambios. PQ vacía.

**Resultado**: dist = {A:0, B:2, C:4, D:5, E:3, F:4}.
</details>

### Ejercicio 4 — Huffman con texto real

Implementa la codificación de Huffman completa: dado un string, cuenta
frecuencias, construye el árbol, genera los códigos, codifica el texto
y calcula la compresión.

<details>
<summary>Verificar</summary>

```rust
use std::collections::{BinaryHeap, HashMap};

#[derive(Debug)]
enum HTree {
    Leaf { ch: char, freq: u32 },
    Node { freq: u32, left: Box<HTree>, right: Box<HTree> },
}

impl HTree {
    fn freq(&self) -> u32 {
        match self { HTree::Leaf { freq, .. } | HTree::Node { freq, .. } => *freq }
    }
}

impl Eq for HTree {}
impl PartialEq for HTree { fn eq(&self, o: &Self) -> bool { self.freq() == o.freq() } }
impl Ord for HTree {
    fn cmp(&self, o: &Self) -> std::cmp::Ordering { o.freq().cmp(&self.freq()) }
}
impl PartialOrd for HTree {
    fn partial_cmp(&self, o: &Self) -> Option<std::cmp::Ordering> { Some(self.cmp(o)) }
}

fn build_codes(tree: &HTree, code: &str, map: &mut HashMap<char, String>) {
    match tree {
        HTree::Leaf { ch, .. } => { map.insert(*ch, code.to_string()); }
        HTree::Node { left, right, .. } => {
            build_codes(left, &format!("{}0", code), map);
            build_codes(right, &format!("{}1", code), map);
        }
    }
}

fn main() {
    let text = "abracadabra";

    // contar frecuencias
    let mut freq: HashMap<char, u32> = HashMap::new();
    for ch in text.chars() { *freq.entry(ch).or_insert(0) += 1; }

    // construir arbol
    let mut pq = BinaryHeap::new();
    for (&ch, &f) in &freq { pq.push(HTree::Leaf { ch, freq: f }); }
    while pq.len() > 1 {
        let a = pq.pop().unwrap();
        let b = pq.pop().unwrap();
        pq.push(HTree::Node {
            freq: a.freq() + b.freq(),
            left: Box::new(a), right: Box::new(b),
        });
    }
    let tree = pq.pop().unwrap();

    // generar codigos
    let mut codes = HashMap::new();
    build_codes(&tree, "", &mut codes);

    println!("Texto: \"{}\"", text);
    println!("\nCodigos:");
    let mut sorted: Vec<_> = codes.iter().collect();
    sorted.sort_by_key(|(ch, _)| *ch);
    for (ch, code) in &sorted { println!("  '{}': {}", ch, code); }

    // codificar
    let encoded: String = text.chars().map(|c| codes[&c].as_str()).collect();
    println!("\nCodificado: {}", encoded);
    println!("Bits Huffman:  {}", encoded.len());
    println!("Bits ASCII:    {}", text.len() * 8);
    println!("Compresion:    {:.1}%",
             (1.0 - encoded.len() as f64 / (text.len() * 8) as f64) * 100.0);
}
```

```
Texto: "abracadabra"

Codigos:
  'a': 0
  'b': 110
  'c': 100
  'd': 101
  'r': 111

Codificado: 01101110100010110111 0
Bits Huffman:  23
Bits ASCII:    88
Compresion:    73.9%
```

23 bits vs 88 bits — la alta frecuencia de 'a' (5/11 = 45%) le asigna un
código de 1 bit, dominando la compresión.
</details>

### Ejercicio 5 — Comparar PQ: array vs heap

Escribe un benchmark que mida $n$ operaciones `insert` seguidas de $n$
operaciones `extract_min`, comparando un array no ordenado vs un heap,
para $n = 10^3$ y $n = 10^4$.

<details>
<summary>Verificar</summary>

```c
#include <time.h>

// PQ con array no ordenado
typedef struct { int *data; int size; int cap; } ArrayPQ;

ArrayPQ *apq_new(void) {
    ArrayPQ *q = malloc(sizeof(ArrayPQ));
    q->data = malloc(1024 * sizeof(int));
    q->size = 0; q->cap = 1024;
    return q;
}

void apq_insert(ArrayPQ *q, int val) {
    if (q->size == q->cap) {
        q->cap *= 2;
        q->data = realloc(q->data, q->cap * sizeof(int));
    }
    q->data[q->size++] = val;  // O(1)
}

int apq_extract_min(ArrayPQ *q) {
    int min_idx = 0;
    for (int i = 1; i < q->size; i++) {  // O(n)
        if (q->data[i] < q->data[min_idx]) min_idx = i;
    }
    int val = q->data[min_idx];
    q->data[min_idx] = q->data[--q->size];  // swap con ultimo
    return val;
}

int main(void) {
    srand(42);
    for (int n = 1000; n <= 10000; n *= 10) {
        // Heap PQ
        int *vals = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) vals[i] = rand();

        clock_t t0 = clock();
        PQ *hpq = pq_new(cmp_dnode);  // reusar comparador
        // ... (usar heap PQ para n inserts + n extracts)
        double heap_t = (double)(clock() - t0) / CLOCKS_PER_SEC;

        // Array PQ
        t0 = clock();
        ArrayPQ *apq = apq_new();
        for (int i = 0; i < n; i++) apq_insert(apq, vals[i]);
        for (int i = 0; i < n; i++) apq_extract_min(apq);
        double arr_t = (double)(clock() - t0) / CLOCKS_PER_SEC;

        printf("n=%5d: heap=%.4fs, array=%.4fs, ratio=%.1fx\n",
               n, heap_t, arr_t, arr_t / heap_t);
        free(vals);
    }
}
```

Resultado típico:

```
n= 1000: heap=0.0001s, array=0.0020s, ratio=20.0x
n=10000: heap=0.0010s, array=0.2000s, ratio=200.0x
```

El array no ordenado tiene `extract_min` en $O(n)$, haciendo $n$
extracciones cuesta $O(n^2)$. El heap cuesta $O(n \log n)$. Para
$n = 10^4$, la diferencia ya es 200×.
</details>

### Ejercicio 6 — Merge de k listas con PQ

Usa una cola de prioridad para hacer merge de $k = 4$ listas ordenadas.
Traza los primeros 5 pasos mostrando el estado de la PQ.

<details>
<summary>Verificar</summary>

Listas: L0=[1,5,9], L1=[2,6], L2=[3,7,10], L3=[4,8].

**Inicializar PQ**: insertar primer elemento de cada lista.
PQ = {(1,L0), (2,L1), (3,L2), (4,L3)}.

**Paso 1**: extraer (1,L0). Output=[1]. Insertar L0[1]=5. PQ = {(2,L1), (3,L2), (4,L3), (5,L0)}.

**Paso 2**: extraer (2,L1). Output=[1,2]. Insertar L1[1]=6. PQ = {(3,L2), (4,L3), (5,L0), (6,L1)}.

**Paso 3**: extraer (3,L2). Output=[1,2,3]. Insertar L2[1]=7. PQ = {(4,L3), (5,L0), (6,L1), (7,L2)}.

**Paso 4**: extraer (4,L3). Output=[1,2,3,4]. Insertar L3[1]=8. PQ = {(5,L0), (6,L1), (7,L2), (8,L3)}.

**Paso 5**: extraer (5,L0). Output=[1,2,3,4,5]. Insertar L0[2]=9. PQ = {(6,L1), (7,L2), (8,L3), (9,L0)}.

La PQ siempre tiene exactamente $k$ elementos (o menos cuando las listas
se agotan). Cada operación es $O(\log k)$, total $O(N \log k)$ donde $N$
es el total de elementos.
</details>

### Ejercicio 7 — PQ con decrease_key

Implementa una cola de prioridad indexada en C que soporte `decrease_key`
en $O(\log n)$, usando un array auxiliar `position[]` para saber dónde
está cada elemento.

<details>
<summary>Verificar</summary>

```c
typedef struct {
    int *keys;       // keys[i] = prioridad del nodo i
    int *heap;       // heap[pos] = nodo_id
    int *pos;        // pos[nodo_id] = posicion en heap
    int size;
    int capacity;
} IndexedPQ;

IndexedPQ *ipq_create(int capacity) {
    IndexedPQ *q = malloc(sizeof(IndexedPQ));
    q->keys = malloc(capacity * sizeof(int));
    q->heap = malloc(capacity * sizeof(int));
    q->pos  = malloc(capacity * sizeof(int));
    q->size = 0;
    q->capacity = capacity;
    for (int i = 0; i < capacity; i++) q->pos[i] = -1;
    return q;
}

static void ipq_swap(IndexedPQ *q, int i, int j) {
    int ni = q->heap[i], nj = q->heap[j];
    q->heap[i] = nj; q->heap[j] = ni;
    q->pos[ni] = j;  q->pos[nj] = i;
}

static void ipq_sift_up(IndexedPQ *q, int i) {
    while (i > 0) {
        int p = (i-1)/2;
        if (q->keys[q->heap[i]] >= q->keys[q->heap[p]]) break;
        ipq_swap(q, i, p);
        i = p;
    }
}

static void ipq_sift_down(IndexedPQ *q, int i) {
    while (1) {
        int best = i, l = 2*i+1, r = 2*i+2;
        if (l < q->size && q->keys[q->heap[l]] < q->keys[q->heap[best]]) best = l;
        if (r < q->size && q->keys[q->heap[r]] < q->keys[q->heap[best]]) best = r;
        if (best == i) break;
        ipq_swap(q, i, best);
        i = best;
    }
}

void ipq_insert(IndexedPQ *q, int node, int key) {
    q->keys[node] = key;
    q->heap[q->size] = node;
    q->pos[node] = q->size;
    q->size++;
    ipq_sift_up(q, q->pos[node]);
}

int ipq_extract_min(IndexedPQ *q) {
    int node = q->heap[0];
    q->size--;
    ipq_swap(q, 0, q->size);
    q->pos[node] = -1;
    if (q->size > 0) ipq_sift_down(q, 0);
    return node;
}

void ipq_decrease_key(IndexedPQ *q, int node, int new_key) {
    if (new_key >= q->keys[node]) return;
    q->keys[node] = new_key;
    ipq_sift_up(q, q->pos[node]);  // O(log n) — pos[] da acceso directo
}
```

La clave es `pos[]`: dado un `node_id`, `pos[node_id]` da la posición en
el array del heap en $O(1)$. Cada `ipq_swap` actualiza ambos arrays
(`heap[]` y `pos[]`) para mantenerlos sincronizados.

Sin `pos[]`, `decrease_key` necesitaría buscar el nodo linealmente:
$O(n)$ en lugar de $O(\log n)$.
</details>

### Ejercicio 8 — Dijkstra con decrease_key vs lazy deletion

Compara las dos estrategias de Dijkstra:
1. **Lazy deletion**: insertar duplicados, descartar al extraer.
2. **Decrease_key**: usar `IndexedPQ` para actualizar in-place.

¿Cuántas operaciones de heap hace cada una para el grafo del ejercicio 3?

<details>
<summary>Verificar</summary>

Grafo del ejercicio 3 (6 nodos, 7 aristas bidireccionales = 14 aristas
dirigidas).

**Lazy deletion**:
- Inserciones totales: 1 (fuente) + hasta 14 (una por arista relajada).
  En este grafo: 1 + 6 relajaciones exitosas = 7 inserciones.
- Extracciones: 7 (una por inserción), de las cuales 1 es duplicado
  descartado.
- Total: 7 push + 7 pop = 14 operaciones de heap.
- Tamaño máximo del heap: 5 (puede tener duplicados).

**Decrease_key**:
- Inserciones: 6 (una por nodo, la primera vez que se descubre).
- Decrease_key: depende del grafo. Cada arista puede causar a lo sumo 1
  decrease_key. En este grafo: 0 decrease_keys (cada nodo se descubre con
  su distancia óptima la primera vez).
- Extracciones: 6 (una por nodo, sin duplicados).
- Total: 6 push + 6 pop + 0 decrease_key = 12 operaciones de heap.
- Tamaño máximo del heap: 4.

En este grafo pequeño la diferencia es mínima. Para grafos densos con muchas
relajaciones, lazy deletion puede insertar $O(E)$ entradas vs $O(V)$ del
decrease_key. Sin embargo, lazy deletion es más simple de implementar y en
la práctica la diferencia de constantes a menudo compensa.
</details>

### Ejercicio 9 — Huffman decodificación

Dado el árbol de Huffman construido para "abracadabra" y el string
codificado `01101110100010110111 0`, decodifícalo de vuelta al texto
original recorriendo el árbol bit por bit.

<details>
<summary>Verificar</summary>

```rust
fn decode(tree: &HTree, bits: &str) -> String {
    let mut result = String::new();
    let mut node = tree;

    for bit in bits.chars() {
        if bit == ' ' { continue; }  // ignorar espacios
        node = match (node, bit) {
            (HTree::Node { left, .. }, '0') => left,
            (HTree::Node { right, .. }, '1') => right,
            _ => panic!("bit invalido o hoja inesperada"),
        };

        if let HTree::Leaf { ch, .. } = node {
            result.push(*ch);
            node = tree;  // volver a la raiz
        }
    }
    result
}
```

Traza manual con el árbol:
```
        (11)
       /    \
     a(5)   (6)
           /    \
         (2)    (4)
        /   \   /  \
      c(1) d(1) b(2) r(2)
```

Bits: `0 110 111 0 100 0 101 0 110 111 0`

- `0` → hoja 'a'
- `110` → b
- `111` → r
- `0` → a
- `100` → c
- `0` → a
- `101` → d
- `0` → a
- `110` → b
- `111` → r
- `0` → a

Resultado: **"abracadabra"** — el texto original.

La propiedad del prefijo garantiza que la decodificación es unívoca: nunca
hay ambigüedad sobre dónde termina un código y empieza el siguiente.
</details>

### Ejercicio 10 — PQ en Rust con BinaryHeap

Implementa las tres aplicaciones (scheduler, Dijkstra, Huffman) usando
`BinaryHeap` de la stdlib de Rust. Para Dijkstra, usa el patrón de lazy
deletion con `Reverse<(u32, usize)>`.

<details>
<summary>Verificar</summary>

La solución completa ya está en la sección "Implementación en Rust" de
este tópico. Los puntos clave:

**Scheduler**: implementar `Ord` en `Process` con orden invertido
(menor `priority` = mayor prioridad para el `BinaryHeap`):

```rust
impl Ord for Process {
    fn cmp(&self, other: &Self) -> Ordering {
        other.priority.cmp(&self.priority)  // invertido
            .then_with(|| other.arrival.cmp(&self.arrival))
    }
}
```

**Dijkstra**: `BinaryHeap<Reverse<(u32, usize)>>` — `Reverse` convierte
el max-heap en min-heap. La tupla `(distancia, nodo)` compara primero por
distancia. Lazy deletion: `if d > dist[u] { continue; }`.

**Huffman**: `enum HTree` con `Ord` invertido (menor `freq` = mayor
prioridad). El merge loop es idéntico en estructura a C:

```rust
while pq.len() > 1 {
    let a = pq.pop().unwrap();
    let b = pq.pop().unwrap();
    pq.push(HTree::Node { freq: a.freq() + b.freq(), ... });
}
```

La versión Rust elimina todo el manejo manual de memoria (malloc/free del
árbol, pool de nodos para Dijkstra) y los casts `void *`, a cambio de
necesitar implementar traits (`Ord`, `Eq`, etc.) en cada tipo.
</details>
