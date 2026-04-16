# T01 — Redes de flujo

## Objetivo

Dominar el modelo formal de **redes de flujo**: definición de red, flujo válido
(restricciones de capacidad y conservación), valor del flujo, **grafo residual**,
**caminos aumentantes**, **cortes s-t**, y el **teorema de flujo máximo / corte
mínimo** (MFMC). Este tópico establece toda la teoría necesaria para que T02
implemente los algoritmos y T03 explore las aplicaciones.

---

## 1. Red de flujo: definición formal

### 1.1 Componentes

Una **red de flujo** es una tupla $G = (V, E, c, s, t)$ donde:

- $V$ es el conjunto de vértices.
- $E \subseteq V \times V$ es el conjunto de aristas dirigidas.
- $c : V \times V \to \mathbb{R}_{\geq 0}$ es la **función de capacidad**.
  Si $(u, v) \notin E$, entonces $c(u, v) = 0$.
- $s \in V$ es la **fuente** (source).
- $t \in V$ es el **sumidero** (sink), con $s \neq t$.

### 1.2 Restricciones estructurales

- Para cada arista $(u, v) \in E$, se requiere $c(u, v) > 0$.
- Si $(u, v) \in E$, se asume $(v, u) \notin E$ (sin aristas antiparalelas).
  Si el problema original las tiene, se resuelve añadiendo un vértice auxiliar:
  reemplazar $(v, u)$ por $(v, x)$ y $(x, u)$ con la misma capacidad.
- No hay self-loops: $(v, v) \notin E$.
- Cada vértice es alcanzable desde $s$ y puede alcanzar $t$ (vértices inútiles
  se eliminan sin afectar el flujo).

### 1.3 Red de ejemplo

Usaremos esta red de 6 vértices a lo largo de todo el tópico:

```
          10          6
    0 ==========> 1 ==========> 5
    ‖             ↑ \            ↑
   8‖           2↑   \8        10‖
    ‖             ↑    ↓         ‖
    2 ==========> 3 ==========> 4
          5              10
```

**Vértices**: 0 (fuente $s$), 1, 2, 3, 4, 5 (sumidero $t$)

| Arista | Capacidad |
|--------|:---------:|
| 0 → 1  |    10     |
| 0 → 2  |     8     |
| 1 → 5  |     6     |
| 1 → 4  |     8     |
| 2 → 3  |     5     |
| 3 → 1  |     2     |
| 3 → 4  |    10     |
| 4 → 5  |    10     |

---

## 2. Flujo: definición y propiedades

### 2.1 Definición

Un **flujo** en $G$ es una función $f : V \times V \to \mathbb{R}$ que satisface:

**Restricción de capacidad**: para todo $u, v \in V$:

$$0 \leq f(u, v) \leq c(u, v)$$

**Conservación de flujo**: para todo $u \in V \setminus \{s, t\}$:

$$\sum_{v \in V} f(v, u) = \sum_{v \in V} f(u, v)$$

Es decir, el flujo que entra a cada vértice intermedio es igual al que sale.

### 2.2 Valor del flujo

El **valor** del flujo $|f|$ es el flujo neto que sale de la fuente:

$$|f| = \sum_{v \in V} f(s, v) - \sum_{v \in V} f(v, s)$$

Equivalentemente, es el flujo neto que llega al sumidero:

$$|f| = \sum_{v \in V} f(v, t) - \sum_{v \in V} f(t, v)$$

El **problema de flujo máximo** busca un flujo $f^*$ tal que $|f^*|$ sea máximo.

### 2.3 Ejemplo de flujo válido

Un flujo con valor 10 en nuestra red:

| Arista | Capacidad | Flujo |
|--------|:---------:|:-----:|
| 0 → 1  |    10     |   7   |
| 0 → 2  |     8     |   3   |
| 1 → 5  |     6     |   3   |
| 1 → 4  |     8     |   4   |
| 2 → 3  |     5     |   3   |
| 3 → 1  |     2     |   0   |
| 3 → 4  |    10     |   3   |
| 4 → 5  |    10     |   7   |

**Verificación de conservación**:
- Nodo 1: entra $f(0,1) + f(3,1) = 7 + 0 = 7$. Sale $f(1,5) + f(1,4) = 3 + 4 = 7$. ✓
- Nodo 2: entra $f(0,2) = 3$. Sale $f(2,3) = 3$. ✓
- Nodo 3: entra $f(2,3) = 5$... espera, $f(2,3) = 3$. Sale $f(3,1) + f(3,4) = 0 + 3 = 3$. ✓
- Nodo 4: entra $f(1,4) + f(3,4) = 4 + 3 = 7$. Sale $f(4,5) = 7$. ✓

Valor: $|f| = f(0,1) + f(0,2) = 7 + 3 = 10$.

¿Es óptimo? No — veremos que el flujo máximo es 15.

### 2.4 Flujo nulo y flujo saturado

- **Flujo nulo**: $f(u,v) = 0$ para todo $(u,v)$. Siempre válido, valor 0.
- Una arista está **saturada** si $f(u,v) = c(u,v)$.
- Una arista está **vacía** si $f(u,v) = 0$.
- Si todas las aristas desde $s$ están saturadas, el flujo podría o no ser
  máximo (depende de si hay caminos alternativos).

---

## 3. Grafo residual

### 3.1 Definición

Dado un flujo $f$ en $G$, el **grafo residual** $G_f = (V, E_f)$ tiene aristas
con **capacidad residual**:

$$c_f(u, v) = \begin{cases}
c(u,v) - f(u,v) & \text{si } (u,v) \in E \quad \text{(arista forward)}\\
f(v,u) & \text{si } (v,u) \in E \quad \text{(arista backward)}\\
0 & \text{en otro caso}
\end{cases}$$

La arista $(u,v)$ existe en $G_f$ si y solo si $c_f(u,v) > 0$.

**Intuición**:
- Una **arista forward** $(u,v)$ con capacidad residual $c(u,v) - f(u,v)$ dice
  "puedo enviar más flujo por aquí".
- Una **arista backward** $(u,v)$ con capacidad residual $f(v,u)$ dice "puedo
  cancelar flujo que actualmente va de $v$ a $u$".

### 3.2 Grafo residual de nuestro ejemplo

Con el flujo de valor 10 del apartado 2.3:

| Arista original | $f$ | $c$ | Forward $c_f$ | Backward $c_f$ |
|----------------|:---:|:---:|:--------------:|:---------------:|
| 0 → 1 | 7 | 10 | $c_f(0,1)=3$ | $c_f(1,0)=7$ |
| 0 → 2 | 3 | 8 | $c_f(0,2)=5$ | $c_f(2,0)=3$ |
| 1 → 5 | 3 | 6 | $c_f(1,5)=3$ | $c_f(5,1)=3$ |
| 1 → 4 | 4 | 8 | $c_f(1,4)=4$ | $c_f(4,1)=4$ |
| 2 → 3 | 3 | 5 | $c_f(2,3)=2$ | $c_f(3,2)=3$ |
| 3 → 1 | 0 | 2 | $c_f(3,1)=2$ | $c_f(1,3)=0$ |
| 3 → 4 | 3 | 10 | $c_f(3,4)=7$ | $c_f(4,3)=3$ |
| 4 → 5 | 7 | 10 | $c_f(4,5)=3$ | $c_f(5,4)=7$ |

Aristas en $G_f$ (las con $c_f > 0$): 0→1:3, 1→0:7, 0→2:5, 2→0:3, 1→5:3,
5→1:3, 1→4:4, 4→1:4, 2→3:2, 3→2:3, 3→1:2, 3→4:7, 4→3:3, 4→5:3, 5→4:7.

### 3.3 Propiedades del grafo residual

- $|E_f| \leq 2|E|$: cada arista original puede generar a lo sumo una forward
  y una backward.
- Si $f(u,v) = 0$: solo existe la forward $(u,v)$ con $c_f = c(u,v)$.
- Si $f(u,v) = c(u,v)$ (saturada): solo existe la backward $(v,u)$ con
  $c_f = c(u,v)$.
- Si $0 < f(u,v) < c(u,v)$: existen ambas.

---

## 4. Caminos aumentantes

### 4.1 Definición

Un **camino aumentante** (augmenting path) es un camino simple de $s$ a $t$ en
el grafo residual $G_f$. El **cuello de botella** (bottleneck) del camino $P$ es:

$$\text{bottleneck}(P) = \min_{(u,v) \in P} c_f(u,v)$$

### 4.2 Aumentar el flujo

Dado un camino aumentante $P$ con cuello de botella $\delta$, se puede construir
un nuevo flujo $f'$ con $|f'| = |f| + \delta$:

$$f'(u,v) = \begin{cases}
f(u,v) + \delta & \text{si } (u,v) \in P \text{ es forward}\\
f(v,u) - \delta & \text{si } (u,v) \in P \text{ es backward (equivale a } (v,u) \text{ forward)}\\
f(u,v) & \text{en otro caso}
\end{cases}$$

**Lema**: $f'$ es un flujo válido (satisface capacidad y conservación).

*Demostración (sketch)*: Para capacidad, $f'(u,v) \leq f(u,v) + \delta \leq
f(u,v) + c_f(u,v) = c(u,v)$. Para conservación, cada vértice intermedio en $P$
tiene una arista entrante y una saliente, así que el flujo adicional $\delta$
se cancela.

### 4.3 Ejemplo: aumentar nuestro flujo

En $G_f$ del flujo de valor 10, un camino aumentante es:

$$0 \xrightarrow{5} 2 \xrightarrow{2} 3 \xrightarrow{7} 4 \xrightarrow{3} 5$$

Cuello de botella: $\min(5, 2, 7, 3) = 2$.

Nuevo flujo $f'$:

| Arista | $f$ | $f'$ | Cambio |
|--------|:---:|:----:|:------:|
| 0 → 2  |  3  |  5   | +2 (forward) |
| 2 → 3  |  3  |  5   | +2 (forward) |
| 3 → 4  |  3  |  5   | +2 (forward) |
| 4 → 5  |  7  |  9   | +2 (forward) |

Valor: $|f'| = 10 + 2 = 12$.

Podemos seguir encontrando caminos aumentantes hasta que no queden (esto es
Ford-Fulkerson, que se formaliza en T02).

---

## 5. Cortes s-t

### 5.1 Definición

Un **corte s-t** $(S, T)$ es una partición de $V$ en dos conjuntos $S$ y
$T = V \setminus S$ tal que $s \in S$ y $t \in T$.

La **capacidad del corte** es la suma de capacidades de las aristas que van de
$S$ a $T$:

$$c(S, T) = \sum_{u \in S, v \in T} c(u, v)$$

Notar que las aristas de $T$ a $S$ **no** se cuentan en la capacidad.

### 5.2 Flujo neto a través de un corte

El **flujo neto** a través de un corte $(S, T)$ es:

$$f(S, T) = \sum_{u \in S, v \in T} f(u,v) - \sum_{u \in T, v \in S} f(u,v)$$

**Lema fundamental**: para cualquier corte $(S, T)$, el flujo neto a través del
corte es igual al valor del flujo: $f(S, T) = |f|$.

*Demostración*: por inducción sobre $|S|$. Caso base: $S = \{s\}$, el flujo
neto es exactamente $|f|$ por definición. Paso inductivo: al añadir un vértice
$v \neq t$ a $S$, la conservación en $v$ garantiza que el flujo neto no cambia.

### 5.3 Cota superior del flujo

**Corolario**: para cualquier flujo $f$ y cualquier corte $(S, T)$:

$$|f| = f(S, T) \leq c(S, T)$$

Esto es porque $f(u,v) \leq c(u,v)$ y restamos los flujos de $T$ a $S$
(que son $\geq 0$).

El valor de **cualquier** flujo es $\leq$ la capacidad de **cualquier** corte.
En particular, el flujo máximo $\leq$ el corte mínimo.

### 5.4 Todos los cortes de nuestra red

Para 6 vértices (con $s=0$ fijo en $S$ y $t=5$ fijo en $T$), hay $2^4 = 16$
posibles cortes. Los más interesantes:

| $S$ | $T$ | Aristas $S \to T$ | Capacidad |
|-----|-----|-------------------|:---------:|
| {0} | {1,2,3,4,5} | 0→1, 0→2 | 10+8 = **18** |
| {0,2} | {1,3,4,5} | 0→1, 2→3 | 10+5 = **15** |
| {0,1} | {2,3,4,5} | 0→2, 1→5, 1→4 | 8+6+8 = 22 |
| {0,1,2,3} | {4,5} | 1→5, 1→4, 3→4 | 6+8+10 = 24 |
| {0,1,2,3,4} | {5} | 1→5, 4→5 | 6+10 = **16** |
| {0,2,3} | {1,4,5} | 0→1, 3→1, 3→4 | 10+2+10 = 22 |

**Corte mínimo**: $(\{0, 2\}, \{1, 3, 4, 5\})$ con capacidad **15**.

---

## 6. Teorema de flujo máximo / corte mínimo (MFMC)

### 6.1 Enunciado

Las siguientes tres afirmaciones son equivalentes:

1. $f$ es un flujo máximo en $G$.
2. El grafo residual $G_f$ no contiene caminos aumentantes ($s$ no alcanza $t$ en $G_f$).
3. $|f| = c(S, T)$ para algún corte $(S, T)$.

**Consecuencia**: $\max_f |f| = \min_{(S,T)} c(S, T)$.

### 6.2 Demostración (sketch)

**$(1) \Rightarrow (2)$**: Si existiera un camino aumentante, podríamos
incrementar $|f|$, contradiciendo que $f$ es máximo.

**$(2) \Rightarrow (3)$**: Si no hay camino de $s$ a $t$ en $G_f$, definimos
$S = \{v : v \text{ es alcanzable desde } s \text{ en } G_f\}$ y $T = V \setminus S$.
Entonces $s \in S$ y $t \in T$. Para cada arista $(u,v)$ con $u \in S, v \in T$:
$c_f(u,v) = 0$, así que $f(u,v) = c(u,v)$ (saturada). Para cada arista $(v,u)$
con $v \in T, u \in S$: $c_f(u,v) = f(v,u) = 0$ (vacía, pues si $f(v,u)>0$,
habría arista backward $u \to v$ en $G_f$ y $v$ sería alcanzable).
Luego $f(S,T) = c(S,T)$ y $|f| = c(S,T)$.

**$(3) \Rightarrow (1)$**: Ya vimos que $|f| \leq c(S,T)$ para todo corte.
Si hay igualdad, $|f|$ no puede ser mayor.

### 6.3 Significado práctico

El MFMC conecta dos problemas aparentemente distintos:
- **Flujo máximo**: ¿cuánto se puede enviar de $s$ a $t$?
- **Corte mínimo**: ¿cuál es el cuello de botella mínimo que separa $s$ de $t$?

Aplicaciones del corte mínimo:
- Fiabilidad de redes: el corte mínimo indica cuántas conexiones hay que cortar
  para desconectar la fuente del sumidero.
- Segmentación de imágenes: separar foreground/background minimizando un costo.

### 6.4 Verificación con nuestra red

Flujo óptimo ($|f^*| = 15$):

| Arista | Cap | Flujo |
|--------|:---:|:-----:|
| 0 → 1  | 10  |  10   |
| 0 → 2  |  8  |   5   |
| 1 → 5  |  6  |   6   |
| 1 → 4  |  8  |   4   |
| 2 → 3  |  5  |   5   |
| 3 → 1  |  2  |   0   |
| 3 → 4  | 10  |   5   |
| 4 → 5  | 10  |   9   |

Verificación: $|f^*| = 10 + 5 = 15$. Corte mínimo $(\{0,2\}, \{1,3,4,5\})$
tiene capacidad $10 + 5 = 15$. MFMC: $15 = 15$. ✓

Residual: $c_f(0,1) = 0$, $c_f(0,2) = 3$, $c_f(2,3) = 0$. Desde 0 en $G_f$:
solo alcanzamos 2 (y de ahí no se puede avanzar pues $c_f(2,3) = 0$). No hay
camino de 0 a 5 en $G_f$. ✓

---

## 7. Descomposición de flujo

### 7.1 Teorema de descomposición

Todo flujo $f$ con valor $|f|$ se puede descomponer en a lo sumo $|E|$ caminos
$s$-$t$ y/o ciclos, donde cada camino/ciclo tiene un flujo positivo asociado.

Para nuestro flujo óptimo:

| Camino | Flujo |
|--------|:-----:|
| 0 → 1 → 5 | 6 |
| 0 → 1 → 4 → 5 | 4 |
| 0 → 2 → 3 → 4 → 5 | 5 |

Total: $6 + 4 + 5 = 15 = |f^*|$. ✓

### 7.2 No unicidad

La descomposición no es única. Otra descomposición válida para el mismo flujo:

| Camino | Flujo |
|--------|:-----:|
| 0 → 1 → 5 | 5 |
| 0 → 1 → 4 → 5 | 5 |
| 0 → 2 → 3 → 4 → 5 | 5 |

Total: $5 + 5 + 5 = 15$. ✓ (con $f(1,5)=5$, $f(1,4)=5$, $f(4,5)=10$).

Ambas son descomposiciones del *mismo* flujo porque el flujo por arista es
idéntico... en realidad, esta segunda descomposición corresponde a un **flujo
diferente** (con $f(1,5) = 5 \neq 6$) pero del mismo valor. La moraleja: el
flujo máximo puede no ser único, aunque su valor sí lo es.

---

## 8. Extensiones del modelo

### 8.1 Múltiples fuentes y sumideros

Si hay fuentes $s_1, \ldots, s_k$ y sumideros $t_1, \ldots, t_j$, se reducen a
una sola fuente/sumidero:

- Añadir **super-fuente** $S$ con aristas $S \to s_i$ de capacidad $\infty$.
- Añadir **super-sumidero** $T$ con aristas $t_j \to T$ de capacidad $\infty$.

El flujo máximo en la red extendida es el flujo máximo en la red original
con múltiples fuentes/sumideros.

### 8.2 Capacidades en vértices

Si un vértice $v$ tiene capacidad $c_v$ (máximo flujo que puede pasar por él),
se modela dividiendo $v$ en $v_{\text{in}}$ y $v_{\text{out}}$ con una arista
$(v_{\text{in}}, v_{\text{out}})$ de capacidad $c_v$. Todas las aristas que
entraban a $v$ ahora entran a $v_{\text{in}}$, y las que salían salen de
$v_{\text{out}}$.

### 8.3 Aristas antiparalelas

Si existen aristas $(u, v)$ y $(v, u)$ simultáneamente, se introduce un vértice
auxiliar $x$: reemplazar $(v, u)$ por $(v, x)$ y $(x, u)$, ambas con capacidad
$c(v, u)$.

### 8.4 Teorema de integralidad

Si todas las capacidades son enteras, existe un flujo máximo entero (todas las
$f(u,v)$ son enteras). Esto se demuestra observando que Ford-Fulkerson (T02)
siempre produce cuellos de botella enteros cuando las capacidades son enteras.

**Consecuencia práctica**: para problemas combinatorios (matching, asignación)
donde las capacidades son 0/1, el flujo máximo es un entero, lo que permite
modelar problemas discretos con flujo en redes.

---

## 9. Programa completo en C

```c
/*
 * flow_network.c
 *
 * Flow network foundations: build network, validate flow,
 * compute residual graph, find augmenting path, enumerate
 * s-t cuts, verify max-flow = min-cut.
 *
 * Compile: gcc -O2 -o flow_network flow_network.c
 * Run:     ./flow_network
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define MAX_V 20

/* =================== FLOW NETWORK ================================= */

typedef struct {
    int cap[MAX_V][MAX_V];   /* capacity matrix */
    int flow[MAX_V][MAX_V];  /* flow matrix */
    int n;                   /* number of vertices */
    int s, t;                /* source, sink */
} FlowNet;

void net_init(FlowNet *net, int n, int s, int t) {
    net->n = n;
    net->s = s;
    net->t = t;
    memset(net->cap, 0, sizeof(net->cap));
    memset(net->flow, 0, sizeof(net->flow));
}

void net_add_edge(FlowNet *net, int u, int v, int c) {
    net->cap[u][v] = c;
}

/* Residual capacity */
int residual(const FlowNet *net, int u, int v) {
    return net->cap[u][v] - net->flow[u][v] + net->flow[v][u];
}

/* =================== FLOW VALIDATION ============================== */

typedef struct {
    bool valid;
    bool capacity_ok;
    bool conservation_ok;
    int violated_cap_u, violated_cap_v;
    int violated_cons_v;
    int flow_value;
} FlowCheck;

FlowCheck validate_flow(const FlowNet *net) {
    FlowCheck chk;
    chk.valid = true;
    chk.capacity_ok = true;
    chk.conservation_ok = true;
    chk.violated_cap_u = -1;
    chk.violated_cap_v = -1;
    chk.violated_cons_v = -1;

    /* Check capacity constraints */
    for (int u = 0; u < net->n; u++)
        for (int v = 0; v < net->n; v++) {
            if (net->flow[u][v] < 0 ||
                net->flow[u][v] > net->cap[u][v]) {
                chk.capacity_ok = false;
                chk.valid = false;
                chk.violated_cap_u = u;
                chk.violated_cap_v = v;
            }
        }

    /* Check flow conservation */
    for (int v = 0; v < net->n; v++) {
        if (v == net->s || v == net->t) continue;
        int in = 0, out = 0;
        for (int u = 0; u < net->n; u++) {
            in += net->flow[u][v];
            out += net->flow[v][u];
        }
        if (in != out) {
            chk.conservation_ok = false;
            chk.valid = false;
            chk.violated_cons_v = v;
        }
    }

    /* Compute flow value */
    chk.flow_value = 0;
    for (int v = 0; v < net->n; v++)
        chk.flow_value += net->flow[net->s][v] - net->flow[v][net->s];

    return chk;
}

/* =================== RESIDUAL GRAPH =============================== */

void print_residual(const FlowNet *net) {
    printf("Residual graph edges:\n");
    for (int u = 0; u < net->n; u++)
        for (int v = 0; v < net->n; v++) {
            int r = residual(net, u, v);
            if (r > 0) {
                const char *type = "";
                if (net->cap[u][v] > 0 && net->flow[v][u] > 0)
                    type = " (forward+backward)";
                else if (net->cap[u][v] > 0)
                    type = " (forward)";
                else if (net->flow[v][u] > 0)
                    type = " (backward)";
                printf("  %d -> %d : %d%s\n", u, v, r, type);
            }
        }
}

/* =================== BFS AUGMENTING PATH ========================== */

bool bfs_augmenting(const FlowNet *net, int parent[], int *bottleneck) {
    bool visited[MAX_V] = {false};
    int queue[MAX_V], front = 0, back = 0;

    visited[net->s] = true;
    parent[net->s] = -1;
    queue[back++] = net->s;

    while (front < back) {
        int u = queue[front++];
        for (int v = 0; v < net->n; v++) {
            if (!visited[v] && residual(net, u, v) > 0) {
                visited[v] = true;
                parent[v] = u;
                queue[back++] = v;
                if (v == net->t) goto found;
            }
        }
    }
    return false;

found:
    *bottleneck = INT_MAX;
    for (int v = net->t; v != net->s; v = parent[v]) {
        int u = parent[v];
        int r = residual(net, u, v);
        if (r < *bottleneck) *bottleneck = r;
    }
    return true;
}

/* =================== AUGMENT FLOW ================================= */

void augment(FlowNet *net, int parent[], int delta) {
    for (int v = net->t; v != net->s; v = parent[v]) {
        int u = parent[v];
        if (net->cap[u][v] > 0) {
            /* Forward edge */
            int push = delta;
            if (push > net->cap[u][v] - net->flow[u][v])
                push = net->cap[u][v] - net->flow[u][v];
            net->flow[u][v] += push;
            delta -= push;
            /* Remaining delta goes to backward */
            if (delta > 0) {
                net->flow[v][u] -= delta;
                delta = 0;
            }
        } else {
            /* Backward edge only */
            net->flow[v][u] -= delta;
        }
    }
}

/* Cleaner augment: handle forward/backward correctly */
void augment_clean(FlowNet *net, int parent[], int delta) {
    for (int v = net->t; v != net->s; v = parent[v]) {
        int u = parent[v];
        if (net->cap[u][v] > 0) {
            /* Forward edge: increase flow u->v */
            net->flow[u][v] += delta;
        }
        if (net->flow[v][u] > 0 && net->cap[u][v] == 0) {
            /* Pure backward edge: decrease flow v->u */
            net->flow[v][u] -= delta;
        }
        /* Mixed case: forward has priority */
        /* This simplified version works for our demo */
    }
}

/* =================== ENUMERATE S-T CUTS =========================== */

typedef struct {
    int S[MAX_V], S_size;
    int T[MAX_V], T_size;
    int capacity;
} Cut;

int enumerate_cuts(const FlowNet *net, Cut cuts[], int max_cuts) {
    int cnt = 0;
    int n = net->n;
    /* Iterate over all subsets of intermediate vertices */
    for (int mask = 0; mask < (1 << n); mask++) {
        /* s must be in S, t must be in T */
        if (!(mask & (1 << net->s))) continue;
        if (mask & (1 << net->t)) continue;

        if (cnt >= max_cuts) break;

        Cut *c = &cuts[cnt];
        c->S_size = 0;
        c->T_size = 0;
        for (int v = 0; v < n; v++) {
            if (mask & (1 << v))
                c->S[c->S_size++] = v;
            else
                c->T[c->T_size++] = v;
        }

        c->capacity = 0;
        for (int i = 0; i < c->S_size; i++)
            for (int j = 0; j < c->T_size; j++)
                c->capacity += net->cap[c->S[i]][c->T[j]];

        cnt++;
    }
    return cnt;
}

/* =================== BUILD EXAMPLE NETWORK ======================== */

void build_example(FlowNet *net) {
    net_init(net, 6, 0, 5);
    net_add_edge(net, 0, 1, 10);
    net_add_edge(net, 0, 2, 8);
    net_add_edge(net, 1, 5, 6);
    net_add_edge(net, 1, 4, 8);
    net_add_edge(net, 2, 3, 5);
    net_add_edge(net, 3, 1, 2);
    net_add_edge(net, 3, 4, 10);
    net_add_edge(net, 4, 5, 10);
}

/* =================== DEMOS ======================================== */

void demo1_network(void) {
    printf("=== Demo 1: Flow Network — Capacity Matrix ===\n\n");

    FlowNet net;
    build_example(&net);

    printf("Network: 6 vertices, source=0, sink=5\n\n");

    printf("Edges:\n");
    for (int u = 0; u < net.n; u++)
        for (int v = 0; v < net.n; v++)
            if (net.cap[u][v] > 0)
                printf("  %d -> %d : capacity %d\n", u, v, net.cap[u][v]);

    printf("\nCapacity matrix:\n     ");
    for (int v = 0; v < net.n; v++) printf("%3d", v);
    printf("\n");
    for (int u = 0; u < net.n; u++) {
        printf("  %d: ", u);
        for (int v = 0; v < net.n; v++) {
            if (net.cap[u][v] > 0)
                printf("%3d", net.cap[u][v]);
            else
                printf("  .");
        }
        printf("\n");
    }
    printf("\n");
}

void demo2_validate_flow(void) {
    printf("=== Demo 2: Valid vs Invalid Flow ===\n\n");

    /* Valid flow */
    FlowNet net;
    build_example(&net);
    net.flow[0][1] = 7;  net.flow[0][2] = 3;
    net.flow[1][5] = 3;  net.flow[1][4] = 4;
    net.flow[2][3] = 3;  net.flow[3][4] = 3;
    net.flow[4][5] = 7;

    printf("Flow 1 (valid, value=10):\n");
    for (int u = 0; u < net.n; u++)
        for (int v = 0; v < net.n; v++)
            if (net.flow[u][v] > 0)
                printf("  f(%d,%d) = %d / %d\n", u, v,
                       net.flow[u][v], net.cap[u][v]);

    FlowCheck chk = validate_flow(&net);
    printf("  Capacity OK: %s\n", chk.capacity_ok ? "yes" : "NO");
    printf("  Conservation OK: %s\n", chk.conservation_ok ? "yes" : "NO");
    printf("  Flow value: %d\n\n", chk.flow_value);

    /* Invalid flow: exceeds capacity */
    FlowNet net2;
    build_example(&net2);
    net2.flow[0][1] = 12;  /* exceeds cap=10 */
    net2.flow[0][2] = 3;
    net2.flow[1][5] = 6;  net2.flow[1][4] = 6;
    net2.flow[2][3] = 3;  net2.flow[3][4] = 3;
    net2.flow[4][5] = 9;

    printf("Flow 2 (INVALID — capacity violation):\n");
    printf("  f(0,1) = 12, but c(0,1) = 10\n");
    FlowCheck chk2 = validate_flow(&net2);
    printf("  Capacity OK: %s", chk2.capacity_ok ? "yes" : "NO");
    if (!chk2.capacity_ok)
        printf(" (violated at edge %d->%d)", chk2.violated_cap_u,
               chk2.violated_cap_v);
    printf("\n\n");

    /* Invalid flow: conservation violation */
    FlowNet net3;
    build_example(&net3);
    net3.flow[0][1] = 7;  net3.flow[0][2] = 3;
    net3.flow[1][5] = 5;  net3.flow[1][4] = 4;  /* in=7, out=9 */
    net3.flow[2][3] = 3;  net3.flow[3][4] = 3;
    net3.flow[4][5] = 7;

    printf("Flow 3 (INVALID — conservation violation):\n");
    printf("  Node 1: in=7, out=5+4=9\n");
    FlowCheck chk3 = validate_flow(&net3);
    printf("  Conservation OK: %s", chk3.conservation_ok ? "yes" : "NO");
    if (!chk3.conservation_ok)
        printf(" (violated at node %d)", chk3.violated_cons_v);
    printf("\n\n");
}

void demo3_residual(void) {
    printf("=== Demo 3: Residual Graph ===\n\n");

    FlowNet net;
    build_example(&net);
    net.flow[0][1] = 7;  net.flow[0][2] = 3;
    net.flow[1][5] = 3;  net.flow[1][4] = 4;
    net.flow[2][3] = 3;  net.flow[3][4] = 3;
    net.flow[4][5] = 7;

    printf("Current flow (value=10):\n");
    for (int u = 0; u < net.n; u++)
        for (int v = 0; v < net.n; v++)
            if (net.flow[u][v] > 0)
                printf("  f(%d,%d) = %d/%d\n", u, v,
                       net.flow[u][v], net.cap[u][v]);
    printf("\n");

    print_residual(&net);

    printf("\nResidual capacity matrix:\n     ");
    for (int v = 0; v < net.n; v++) printf("%3d", v);
    printf("\n");
    for (int u = 0; u < net.n; u++) {
        printf("  %d: ", u);
        for (int v = 0; v < net.n; v++) {
            int r = residual(&net, u, v);
            if (r > 0)
                printf("%3d", r);
            else
                printf("  .");
        }
        printf("\n");
    }
    printf("\n");
}

void demo4_augmenting_path(void) {
    printf("=== Demo 4: Find Augmenting Path (BFS) ===\n\n");

    FlowNet net;
    build_example(&net);
    /* Start with flow = 0 */

    printf("Starting with zero flow.\n\n");

    for (int iter = 1; iter <= 4; iter++) {
        int parent[MAX_V], bottleneck;
        bool found = bfs_augmenting(&net, parent, &bottleneck);

        if (!found) {
            printf("Iteration %d: no augmenting path found. Done!\n\n", iter);
            break;
        }

        printf("Iteration %d: augmenting path (BFS): ", iter);
        /* Reconstruct path */
        int path[MAX_V], plen = 0;
        for (int v = net.t; v != -1; v = parent[v])
            path[plen++] = v;
        for (int i = plen - 1; i >= 0; i--)
            printf("%d%s", path[i], i > 0 ? " -> " : "");
        printf("\n  Bottleneck: %d\n", bottleneck);

        /* Augment */
        for (int v = net.t; v != net.s; v = parent[v]) {
            int u = parent[v];
            if (net.cap[u][v] > 0)
                net.flow[u][v] += bottleneck;
            else
                net.flow[v][u] -= bottleneck;
        }

        int fval = 0;
        for (int v = 0; v < net.n; v++)
            fval += net.flow[net.s][v];
        printf("  Flow value after augmentation: %d\n\n", fval);
    }

    printf("Final flow:\n");
    for (int u = 0; u < net.n; u++)
        for (int v = 0; v < net.n; v++)
            if (net.flow[u][v] > 0)
                printf("  f(%d,%d) = %d/%d\n", u, v,
                       net.flow[u][v], net.cap[u][v]);
    int fval = 0;
    for (int v = 0; v < net.n; v++)
        fval += net.flow[net.s][v];
    printf("Max flow: %d\n\n", fval);
}

void demo5_all_cuts(void) {
    printf("=== Demo 5: All s-t Cuts and Min Cut ===\n\n");

    FlowNet net;
    build_example(&net);

    Cut cuts[64];
    int num = enumerate_cuts(&net, cuts, 64);

    /* Find min cut */
    int min_cap = INT_MAX, min_idx = 0;
    for (int i = 0; i < num; i++) {
        if (cuts[i].capacity < min_cap) {
            min_cap = cuts[i].capacity;
            min_idx = i;
        }
    }

    printf("Total s-t cuts: %d\n\n", num);

    /* Print a selection of cuts */
    printf("Selected cuts (sorted by capacity):\n");
    printf("%-20s  %-25s  Capacity\n", "S", "T");
    printf("%-20s  %-25s  --------\n", "---", "---");

    /* Simple selection sort to show top cuts */
    bool shown[64] = {false};
    for (int rank = 0; rank < num && rank < 8; rank++) {
        int best = -1;
        for (int i = 0; i < num; i++) {
            if (!shown[i] && (best == -1 ||
                cuts[i].capacity < cuts[best].capacity))
                best = i;
        }
        shown[best] = true;
        Cut *c = &cuts[best];
        char s_str[50], t_str[50];
        sprintf(s_str, "{");
        for (int j = 0; j < c->S_size; j++)
            sprintf(s_str + strlen(s_str), "%s%d",
                    j > 0 ? "," : "", c->S[j]);
        strcat(s_str, "}");
        sprintf(t_str, "{");
        for (int j = 0; j < c->T_size; j++)
            sprintf(t_str + strlen(t_str), "%s%d",
                    j > 0 ? "," : "", c->T[j]);
        strcat(t_str, "}");
        printf("%-20s  %-25s  %d%s\n", s_str, t_str, c->capacity,
               c->capacity == min_cap ? "  <-- MIN" : "");
    }
    printf("\nMin cut capacity: %d\n\n", min_cap);
}

void demo6_mfmc_verify(void) {
    printf("=== Demo 6: Max-Flow = Min-Cut Verification ===\n\n");

    FlowNet net;
    build_example(&net);

    /* Find max flow via repeated BFS augmentation */
    int total_flow = 0;
    int parent[MAX_V], bottleneck;
    while (bfs_augmenting(&net, parent, &bottleneck)) {
        for (int v = net.t; v != net.s; v = parent[v]) {
            int u = parent[v];
            if (net.cap[u][v] > 0)
                net.flow[u][v] += bottleneck;
            else
                net.flow[v][u] -= bottleneck;
        }
        total_flow += bottleneck;
    }

    printf("Max flow: %d\n\n", total_flow);

    /* Find min cut from residual: S = reachable from s */
    bool reachable[MAX_V] = {false};
    int queue[MAX_V], front = 0, back = 0;
    reachable[net.s] = true;
    queue[back++] = net.s;
    while (front < back) {
        int u = queue[front++];
        for (int v = 0; v < net.n; v++) {
            if (!reachable[v] && residual(&net, u, v) > 0) {
                reachable[v] = true;
                queue[back++] = v;
            }
        }
    }

    printf("Min cut (from residual graph):\n");
    printf("  S = { ");
    for (int v = 0; v < net.n; v++)
        if (reachable[v]) printf("%d ", v);
    printf("}\n  T = { ");
    for (int v = 0; v < net.n; v++)
        if (!reachable[v]) printf("%d ", v);
    printf("}\n\n");

    int cut_cap = 0;
    printf("Edges crossing cut (S -> T):\n");
    for (int u = 0; u < net.n; u++)
        for (int v = 0; v < net.n; v++)
            if (reachable[u] && !reachable[v] && net.cap[u][v] > 0) {
                printf("  %d -> %d : cap=%d, flow=%d (saturated: %s)\n",
                       u, v, net.cap[u][v], net.flow[u][v],
                       net.flow[u][v] == net.cap[u][v] ? "yes" : "no");
                cut_cap += net.cap[u][v];
            }

    printf("\nMin cut capacity: %d\n", cut_cap);
    printf("Max flow = Min cut: %d = %d  %s\n\n",
           total_flow, cut_cap,
           total_flow == cut_cap ? "VERIFIED" : "MISMATCH!");
}

/* =================== MAIN ========================================= */

int main(void) {
    demo1_network();
    demo2_validate_flow();
    demo3_residual();
    demo4_augmenting_path();
    demo5_all_cuts();
    demo6_mfmc_verify();
    return 0;
}
```

---

## 10. Programa completo en Rust

```rust
/*
 * flow_network.rs
 *
 * Flow network foundations in Rust: build network, validate
 * flow, residual graph, augmenting path, cuts, MFMC
 * verification, multiple sources/sinks, flow decomposition.
 *
 * Compile: rustc -o flow_network flow_network.rs
 * Run:     ./flow_network
 */

use std::collections::VecDeque;

/* =================== FLOW NETWORK ================================= */

#[derive(Clone)]
struct FlowNet {
    cap: Vec<Vec<i32>>,
    flow: Vec<Vec<i32>>,
    n: usize,
    s: usize,
    t: usize,
}

impl FlowNet {
    fn new(n: usize, s: usize, t: usize) -> Self {
        FlowNet {
            cap: vec![vec![0; n]; n],
            flow: vec![vec![0; n]; n],
            n, s, t,
        }
    }

    fn add_edge(&mut self, u: usize, v: usize, c: i32) {
        self.cap[u][v] = c;
    }

    fn residual(&self, u: usize, v: usize) -> i32 {
        self.cap[u][v] - self.flow[u][v] + self.flow[v][u]
    }

    fn flow_value(&self) -> i32 {
        (0..self.n).map(|v| self.flow[self.s][v] - self.flow[v][self.s]).sum()
    }

    fn bfs_augmenting(&self) -> Option<(Vec<Option<usize>>, i32)> {
        let mut visited = vec![false; self.n];
        let mut parent = vec![None; self.n];
        let mut queue = VecDeque::new();

        visited[self.s] = true;
        queue.push_back(self.s);

        while let Some(u) = queue.pop_front() {
            for v in 0..self.n {
                if !visited[v] && self.residual(u, v) > 0 {
                    visited[v] = true;
                    parent[v] = Some(u);
                    if v == self.t {
                        /* Find bottleneck */
                        let mut bn = i32::MAX;
                        let mut cur = self.t;
                        while let Some(p) = parent[cur] {
                            bn = bn.min(self.residual(p, cur));
                            cur = p;
                        }
                        return Some((parent, bn));
                    }
                    queue.push_back(v);
                }
            }
        }
        None
    }

    fn augment(&mut self, parent: &[Option<usize>], delta: i32) {
        let mut v = self.t;
        while let Some(u) = parent[v] {
            if self.cap[u][v] > 0 {
                self.flow[u][v] += delta;
            } else {
                self.flow[v][u] -= delta;
            }
            v = u;
        }
    }

    fn find_max_flow(&mut self) -> i32 {
        let mut total = 0;
        while let Some((parent, bn)) = self.bfs_augmenting() {
            self.augment(&parent, bn);
            total += bn;
        }
        total
    }

    fn reachable_from_s(&self) -> Vec<bool> {
        let mut visited = vec![false; self.n];
        let mut queue = VecDeque::new();
        visited[self.s] = true;
        queue.push_back(self.s);
        while let Some(u) = queue.pop_front() {
            for v in 0..self.n {
                if !visited[v] && self.residual(u, v) > 0 {
                    visited[v] = true;
                    queue.push_back(v);
                }
            }
        }
        visited
    }
}

fn build_example() -> FlowNet {
    let mut net = FlowNet::new(6, 0, 5);
    net.add_edge(0, 1, 10);
    net.add_edge(0, 2, 8);
    net.add_edge(1, 5, 6);
    net.add_edge(1, 4, 8);
    net.add_edge(2, 3, 5);
    net.add_edge(3, 1, 2);
    net.add_edge(3, 4, 10);
    net.add_edge(4, 5, 10);
    net
}

/* =================== FLOW VALIDATION ============================== */

struct FlowCheck {
    capacity_ok: bool,
    conservation_ok: bool,
    cap_violation: Option<(usize, usize)>,
    cons_violation: Option<usize>,
    flow_value: i32,
}

fn validate_flow(net: &FlowNet) -> FlowCheck {
    let mut chk = FlowCheck {
        capacity_ok: true,
        conservation_ok: true,
        cap_violation: None,
        cons_violation: None,
        flow_value: 0,
    };

    for u in 0..net.n {
        for v in 0..net.n {
            if net.flow[u][v] < 0 || net.flow[u][v] > net.cap[u][v] {
                chk.capacity_ok = false;
                chk.cap_violation = Some((u, v));
            }
        }
    }

    for v in 0..net.n {
        if v == net.s || v == net.t { continue; }
        let flow_in: i32 = (0..net.n).map(|u| net.flow[u][v]).sum();
        let flow_out: i32 = (0..net.n).map(|u| net.flow[v][u]).sum();
        if flow_in != flow_out {
            chk.conservation_ok = false;
            chk.cons_violation = Some(v);
        }
    }

    chk.flow_value = net.flow_value();
    chk
}

/* =================== DEMOS ======================================== */

fn demo1_network() {
    println!("=== Demo 1: Flow Network — Edges and Capacity ===\n");

    let net = build_example();

    println!("Network: {} vertices, source={}, sink={}\n", net.n, net.s, net.t);
    println!("Edges:");
    for u in 0..net.n {
        for v in 0..net.n {
            if net.cap[u][v] > 0 {
                println!("  {} -> {} : capacity {}", u, v, net.cap[u][v]);
            }
        }
    }

    println!("\nCapacity matrix:");
    print!("     ");
    for v in 0..net.n { print!("{:>3}", v); }
    println!();
    for u in 0..net.n {
        print!("  {}: ", u);
        for v in 0..net.n {
            if net.cap[u][v] > 0 {
                print!("{:>3}", net.cap[u][v]);
            } else {
                print!("  .");
            }
        }
        println!();
    }
    println!();
}

fn demo2_validate() {
    println!("=== Demo 2: Valid vs Invalid Flow ===\n");

    /* Valid flow */
    let mut net = build_example();
    net.flow[0][1] = 7;  net.flow[0][2] = 3;
    net.flow[1][5] = 3;  net.flow[1][4] = 4;
    net.flow[2][3] = 3;  net.flow[3][4] = 3;
    net.flow[4][5] = 7;

    println!("Flow A (valid, value=10):");
    let chk = validate_flow(&net);
    println!("  Capacity OK: {}", chk.capacity_ok);
    println!("  Conservation OK: {}", chk.conservation_ok);
    println!("  Flow value: {}\n", chk.flow_value);

    /* Invalid: capacity violation */
    let mut net2 = build_example();
    net2.flow[0][1] = 12;  net2.flow[0][2] = 3;
    net2.flow[1][5] = 6;  net2.flow[1][4] = 6;
    net2.flow[2][3] = 3;  net2.flow[3][4] = 3;
    net2.flow[4][5] = 9;

    println!("Flow B (INVALID — capacity):");
    let chk2 = validate_flow(&net2);
    print!("  Capacity OK: {}", chk2.capacity_ok);
    if let Some((u, v)) = chk2.cap_violation {
        print!(" (violated at {}->{}: flow {} > cap {})",
               u, v, net2.flow[u][v], net2.cap[u][v]);
    }
    println!("\n");

    /* Invalid: conservation violation */
    let mut net3 = build_example();
    net3.flow[0][1] = 7;  net3.flow[0][2] = 3;
    net3.flow[1][5] = 5;  net3.flow[1][4] = 4;
    net3.flow[2][3] = 3;  net3.flow[3][4] = 3;
    net3.flow[4][5] = 7;

    println!("Flow C (INVALID — conservation):");
    let chk3 = validate_flow(&net3);
    print!("  Conservation OK: {}", chk3.conservation_ok);
    if let Some(v) = chk3.cons_violation {
        let flow_in: i32 = (0..net3.n).map(|u| net3.flow[u][v]).sum();
        let flow_out: i32 = (0..net3.n).map(|u| net3.flow[v][u]).sum();
        print!(" (node {}: in={}, out={})", v, flow_in, flow_out);
    }
    println!("\n");
}

fn demo3_residual() {
    println!("=== Demo 3: Residual Graph ===\n");

    let mut net = build_example();
    net.flow[0][1] = 7;  net.flow[0][2] = 3;
    net.flow[1][5] = 3;  net.flow[1][4] = 4;
    net.flow[2][3] = 3;  net.flow[3][4] = 3;
    net.flow[4][5] = 7;

    println!("Current flow (value=10):\n");
    println!("Residual graph edges:");
    for u in 0..net.n {
        for v in 0..net.n {
            let r = net.residual(u, v);
            if r > 0 {
                let kind = if net.cap[u][v] > 0 && net.flow[v][u] > 0 {
                    "forward+backward"
                } else if net.cap[u][v] > 0 {
                    "forward"
                } else if net.flow[v][u] > 0 {
                    "backward"
                } else {
                    "?"
                };
                println!("  {} -> {} : {} ({})", u, v, r, kind);
            }
        }
    }
    println!();
}

fn demo4_augmenting() {
    println!("=== Demo 4: BFS Augmenting Path ===\n");

    let mut net = build_example();
    net.flow[0][1] = 7;  net.flow[0][2] = 3;
    net.flow[1][5] = 3;  net.flow[1][4] = 4;
    net.flow[2][3] = 3;  net.flow[3][4] = 3;
    net.flow[4][5] = 7;

    println!("Starting with flow value = {}\n", net.flow_value());

    if let Some((parent, bn)) = net.bfs_augmenting() {
        let mut path = Vec::new();
        let mut v = net.t;
        path.push(v);
        while let Some(p) = parent[v] {
            path.push(p);
            v = p;
        }
        path.reverse();

        let path_str: Vec<String> = path.iter().map(|v| v.to_string()).collect();
        println!("Augmenting path: {}", path_str.join(" -> "));
        println!("Bottleneck: {}", bn);

        net.augment(&parent, bn);
        println!("Flow value after augmentation: {}\n", net.flow_value());
    } else {
        println!("No augmenting path found (flow is maximum).\n");
    }
}

fn demo5_min_cut() {
    println!("=== Demo 5: All s-t Cuts and Min Cut ===\n");

    let net = build_example();

    let mut min_cap = i32::MAX;
    let mut min_s_set = Vec::new();
    let mut cut_count = 0;
    let mut cuts: Vec<(Vec<usize>, Vec<usize>, i32)> = Vec::new();

    for mask in 0..(1u32 << net.n) {
        if mask & (1 << net.s) == 0 { continue; }
        if mask & (1 << net.t) != 0 { continue; }

        let s_set: Vec<usize> = (0..net.n).filter(|&v| mask & (1 << v) != 0).collect();
        let t_set: Vec<usize> = (0..net.n).filter(|&v| mask & (1 << v) == 0).collect();

        let cap: i32 = s_set.iter()
            .flat_map(|&u| t_set.iter().map(move |&v| net.cap[u][v]))
            .sum();

        if cap < min_cap {
            min_cap = cap;
            min_s_set = s_set.clone();
        }
        cuts.push((s_set, t_set, cap));
        cut_count += 1;
    }

    println!("Total s-t cuts: {}\n", cut_count);

    /* Show top 6 by capacity */
    cuts.sort_by_key(|c| c.2);
    println!("Top 6 cuts by capacity:");
    println!("{:<20} {:<25} Capacity", "S", "T");
    for (s_set, t_set, cap) in cuts.iter().take(6) {
        let s_str = format!("{{{}}}", s_set.iter()
            .map(|v| v.to_string()).collect::<Vec<_>>().join(","));
        let t_str = format!("{{{}}}", t_set.iter()
            .map(|v| v.to_string()).collect::<Vec<_>>().join(","));
        let marker = if *cap == min_cap { " <-- MIN" } else { "" };
        println!("{:<20} {:<25} {}{}", s_str, t_str, cap, marker);
    }
    println!("\nMin cut capacity: {}\n", min_cap);
}

fn demo6_mfmc() {
    println!("=== Demo 6: Max-Flow = Min-Cut Verification ===\n");

    let mut net = build_example();
    let max_flow = net.find_max_flow();

    println!("Max flow: {}\n", max_flow);

    let reachable = net.reachable_from_s();

    let s_set: Vec<usize> = (0..net.n).filter(|&v| reachable[v]).collect();
    let t_set: Vec<usize> = (0..net.n).filter(|&v| !reachable[v]).collect();

    println!("Min cut from residual:");
    println!("  S = {{{}}}", s_set.iter()
        .map(|v| v.to_string()).collect::<Vec<_>>().join(", "));
    println!("  T = {{{}}}", t_set.iter()
        .map(|v| v.to_string()).collect::<Vec<_>>().join(", "));

    println!("\nCut edges (S -> T):");
    let mut cut_cap = 0;
    for &u in &s_set {
        for &v in &t_set {
            if net.cap[u][v] > 0 {
                println!("  {} -> {} : cap={}, flow={} (saturated: {})",
                         u, v, net.cap[u][v], net.flow[u][v],
                         if net.flow[u][v] == net.cap[u][v] { "yes" } else { "no" });
                cut_cap += net.cap[u][v];
            }
        }
    }

    println!("\nMin cut capacity: {}", cut_cap);
    println!("Max flow = Min cut: {} = {}  {}",
             max_flow, cut_cap,
             if max_flow == cut_cap { "VERIFIED" } else { "MISMATCH!" });
    println!();
}

fn demo7_multi_source() {
    println!("=== Demo 7: Multiple Sources/Sinks Reduction ===\n");

    /*
     * Original: sources {0, 1}, sinks {4, 5}
     * Edges: 0->2:5, 0->3:4, 1->2:3, 1->3:6, 2->4:7, 2->5:3, 3->4:4, 3->5:8
     *
     * Reduction: add super-source 6, super-sink 7
     * 6->0:INF, 6->1:INF, 4->7:INF, 5->7:INF
     */
    let inf = 1000;
    let mut net = FlowNet::new(8, 6, 7);
    /* Original edges */
    net.add_edge(0, 2, 5); net.add_edge(0, 3, 4);
    net.add_edge(1, 2, 3); net.add_edge(1, 3, 6);
    net.add_edge(2, 4, 7); net.add_edge(2, 5, 3);
    net.add_edge(3, 4, 4); net.add_edge(3, 5, 8);
    /* Super-source/sink */
    net.add_edge(6, 0, inf); net.add_edge(6, 1, inf);
    net.add_edge(4, 7, inf); net.add_edge(5, 7, inf);

    println!("Original network: sources {{0, 1}}, sinks {{4, 5}}");
    println!("Edges: 0->2:5, 0->3:4, 1->2:3, 1->3:6, 2->4:7, 2->5:3, 3->4:4, 3->5:8\n");
    println!("Reduced network: super-source=6, super-sink=7");
    println!("Added: 6->0:INF, 6->1:INF, 4->7:INF, 5->7:INF\n");

    let max_flow = net.find_max_flow();
    println!("Max flow (super-source to super-sink): {}\n", max_flow);

    println!("Flow from each original source:");
    for &src in &[0usize, 1] {
        let out: i32 = (0..net.n).map(|v| net.flow[src][v]).sum();
        println!("  Source {}: sends {} units", src, out);
    }
    println!("Flow to each original sink:");
    for &snk in &[4usize, 5] {
        let in_flow: i32 = (0..net.n).map(|u| net.flow[u][snk]).sum();
        println!("  Sink {}: receives {} units", snk, in_flow);
    }
    println!();
}

fn demo8_decomposition() {
    println!("=== Demo 8: Flow Decomposition into Paths ===\n");

    let mut net = build_example();
    let max_flow = net.find_max_flow();
    println!("Max flow: {}\n", max_flow);

    /* Decompose into s-t paths using greedy DFS */
    let mut residual_flow = net.flow.clone();
    let mut paths: Vec<(Vec<usize>, i32)> = Vec::new();

    loop {
        /* DFS from s to t following positive flow */
        let mut visited = vec![false; net.n];
        let mut stack: Vec<(usize, Vec<usize>)> = vec![(net.s, vec![net.s])];
        let mut found_path: Option<Vec<usize>> = None;

        while let Some((u, path)) = stack.pop() {
            if u == net.t {
                found_path = Some(path);
                break;
            }
            if visited[u] { continue; }
            visited[u] = true;

            for v in 0..net.n {
                if !visited[v] && residual_flow[u][v] > 0 {
                    let mut new_path = path.clone();
                    new_path.push(v);
                    stack.push((v, new_path));
                }
            }
        }

        match found_path {
            None => break,
            Some(path) => {
                /* Find min flow along path */
                let bn = path.windows(2)
                    .map(|w| residual_flow[w[0]][w[1]])
                    .min()
                    .unwrap();

                /* Subtract flow */
                for w in path.windows(2) {
                    residual_flow[w[0]][w[1]] -= bn;
                }

                paths.push((path, bn));
            }
        }
    }

    println!("Flow decomposition into s-t paths:");
    let mut total = 0;
    for (i, (path, flow)) in paths.iter().enumerate() {
        let path_str: Vec<String> = path.iter().map(|v| v.to_string()).collect();
        println!("  Path {}: {} (flow = {})", i + 1, path_str.join(" -> "), flow);
        total += flow;
    }
    println!("\nTotal: {} (should equal max flow {})\n", total, max_flow);
}

/* =================== MAIN ========================================= */

fn main() {
    demo1_network();
    demo2_validate();
    demo3_residual();
    demo4_augmenting();
    demo5_min_cut();
    demo6_mfmc();
    demo7_multi_source();
    demo8_decomposition();
}
```

---

## 11. Ejercicios

### Ejercicio 1 — Verificar flujo válido

Dada la red con aristas 0→1:5, 0→2:3, 1→3:4, 2→3:6, 1→2:2 (fuente=0,
sumidero=3), determina si el flujo $f(0,1)=4, f(0,2)=3, f(1,2)=2, f(1,3)=2,
f(2,3)=5$ es válido.

<details><summary>Predicción</summary>

Verificar capacidad:
- $f(0,1)=4 \leq c(0,1)=5$ ✓
- $f(0,2)=3 \leq c(0,2)=3$ ✓
- $f(1,2)=2 \leq c(1,2)=2$ ✓
- $f(1,3)=2 \leq c(1,3)=4$ ✓
- $f(2,3)=5 \leq c(2,3)=6$ ✓

Verificar conservación:
- Nodo 1: entra $f(0,1)=4$. Sale $f(1,2)+f(1,3)=2+2=4$. ✓
- Nodo 2: entra $f(0,2)+f(1,2)=3+2=5$. Sale $f(2,3)=5$. ✓

**Flujo válido** con valor $|f| = f(0,1)+f(0,2) = 4+3 = 7$.

</details>

### Ejercicio 2 — Grafo residual

Para la red y flujo del ejercicio 1, construye el grafo residual. Enumera
todas las aristas con su capacidad residual y tipo (forward/backward).

<details><summary>Predicción</summary>

| Arista original | $f$ | $c$ | Forward $c_f$ | Backward $c_f$ |
|:-:|:-:|:-:|:-:|:-:|
| 0→1 | 4 | 5 | $c_f(0,1)=1$ | $c_f(1,0)=4$ |
| 0→2 | 3 | 3 | $c_f(0,2)=0$ (saturada) | $c_f(2,0)=3$ |
| 1→2 | 2 | 2 | $c_f(1,2)=0$ (saturada) | $c_f(2,1)=2$ |
| 1→3 | 2 | 4 | $c_f(1,3)=2$ | $c_f(3,1)=2$ |
| 2→3 | 5 | 6 | $c_f(2,3)=1$ | $c_f(3,2)=5$ |

Aristas en $G_f$: 0→1:1, 1→0:4, 2→0:3, 2→1:2, 1→3:2, 3→1:2, 2→3:1, 3→2:5.

</details>

### Ejercicio 3 — Camino aumentante

En el grafo residual del ejercicio 2, encuentra un camino aumentante de 0 a 3.
¿Cuál es el cuello de botella? ¿Cuál es el flujo resultante?

<details><summary>Predicción</summary>

Camino en $G_f$: $0 \to 1 \to 3$ con $c_f(0,1)=1, c_f(1,3)=2$.
Cuello de botella: $\min(1, 2) = 1$.

Nuevo flujo:
- $f'(0,1) = 4+1 = 5$
- $f'(1,3) = 2+1 = 3$
- Resto igual.

Valor: $|f'| = 5+3 = 8$.

¿Es máximo? Aristas desde $s=0$: $c(0,1)=5$ saturada, $c(0,2)=3$ saturada.
No puede salir más flujo de $s$. Entonces $|f'| = 8$ **es máximo**.

Corte mínimo: $(\{0\}, \{1,2,3\})$ con capacidad $5+3=8 = |f'|$. ✓

</details>

### Ejercicio 4 — Encontrar el corte mínimo

Red: 0→1:3, 0→2:2, 1→2:1, 1→3:2, 2→3:4 (s=0, t=3).

Encuentra el flujo máximo y el corte mínimo. Verifica MFMC.

<details><summary>Predicción</summary>

Augmenting paths:
1. 0→1→3: bottleneck min(3,2) = 2. Flow = 2.
2. 0→2→3: bottleneck min(2,4) = 2. Flow = 4.
3. 0→1→2→3: bottleneck min(1,1,2) = 1. Flow = 5.

No more paths. Max flow = **5**.

Corte: $(\{0\}, \{1,2,3\})$, capacidad $3+2=5$.

MFMC: $5 = 5$. ✓

Verificar residual tras flujo 5:
- $f(0,1)=3$ (saturada), $f(0,2)=2$ (saturada), $f(1,3)=2$, $f(1,2)=1$, $f(2,3)=3$.
- Desde 0 en $G_f$: no se puede ir a 1 (cap residual 0) ni a 2 (cap residual 0).
- $S=\{0\}$. Corte mínimo = 3+2 = 5. ✓

</details>

### Ejercicio 5 — Múltiples fuentes

Red con fuentes {A, B} y sumidero C. Aristas: A→D:5, B→D:3, B→E:4, D→C:6,
E→C:4.

Reduce a fuente/sumidero únicos y calcula el flujo máximo.

<details><summary>Predicción</summary>

Añadir super-fuente $S$ con $S→A:\infty$, $S→B:\infty$. El sumidero ya es único
($C$).

Red extendida: S→A:∞, S→B:∞, A→D:5, B→D:3, B→E:4, D→C:6, E→C:4.

Flujo máximo:
- S→A→D→C: min(∞,5,6) = 5.
- S→B→D→C: min(∞,3,1) = 1 (D→C tiene residual 6-5=1).
- S→B→E→C: min(∞,4,4) = 4. Wait, let me check: B→D already used 0, B→E used 0.

Let me redo:
1. S→A→D→C: bn = min(∞,5,6) = 5. f(A,D)=5, f(D,C)=5.
2. S→B→E→C: bn = min(∞,4,4) = 4. f(B,E)=4, f(E,C)=4.
3. S→B→D→C: bn = min(∞,3,1) = 1. f(B,D)=1, f(D,C)=6.

No more paths. Max flow = 5+4+1 = **10**.

Corte: $(\{S,A,B\}, \{D,E,C\})$, capacidad $5+3+4 = 12$. Hmm, eso no es 10.

Corte: $(\{S,A,B,D,E\}, \{C\})$, capacidad $6+4 = 10$. ✓

Max flow = min cut = **10**. ✓

</details>

### Ejercicio 6 — Aristas antiparalelas

Red con aristas 0→1:5 y 1→0:3 (además de 0→2:4, 1→2:6, s=0, t=2).

Explica el problema de las aristas antiparalelas y cómo resolverlo. Calcula
el flujo máximo en la red transformada.

<details><summary>Predicción</summary>

**Problema**: si existen $(0,1)$ y $(1,0)$, la definición estándar de capacidad
residual se complica porque $c_f(0,1) = c(0,1) - f(0,1) + f(1,0)$ mezcla
ambas aristas.

**Solución**: introducir vértice auxiliar $x$. Reemplazar $1→0:3$ por $1→x:3$
y $x→0:3$.

Red transformada: 0→1:5, 0→2:4, 1→2:6, 1→x:3, x→0:3 (s=0, t=2).

Max flow (la arista 1→0 no ayuda a enviar flujo de 0 a 2):
1. 0→1→2: bn = min(5,6) = 5. Flow = 5.
2. 0→2: bn = 4. Flow = 9.
No más paths útiles. Max flow = **9**.

Las aristas $1→x→0$ forman un ciclo potencial pero no contribuyen al flujo
de $s$ a $t$ porque redirigen flujo de vuelta a la fuente.

</details>

### Ejercicio 7 — Capacidad en vértices

Red: 0→1:10, 0→2:10, 1→3:10, 2→3:10 (s=0, t=3). Pero el vértice 1 tiene
capacidad 4 y el vértice 2 tiene capacidad 7.

Transforma la red y calcula el flujo máximo.

<details><summary>Predicción</summary>

Dividir vértice 1 en $1_{\text{in}}$ y $1_{\text{out}}$ con arista
$1_{\text{in}} → 1_{\text{out}}$: capacidad 4.
Dividir vértice 2 en $2_{\text{in}}$ y $2_{\text{out}}$ con arista
$2_{\text{in}} → 2_{\text{out}}$: capacidad 7.

Red transformada:
- $0 → 1_{\text{in}}$: 10
- $0 → 2_{\text{in}}$: 10
- $1_{\text{in}} → 1_{\text{out}}$: 4
- $2_{\text{in}} → 2_{\text{out}}$: 7
- $1_{\text{out}} → 3$: 10
- $2_{\text{out}} → 3$: 10

Max flow:
- Por vértice 1: limitado por min(10, 4, 10) = 4.
- Por vértice 2: limitado por min(10, 7, 10) = 7.
- Total: 4 + 7 = **11**.

Sin las capacidades en vértices, sería min(20, 20) = 20. Las capacidades en
vértices actúan como cuellos de botella internos.

</details>

### Ejercicio 8 — Descomposición de flujo

Dado el flujo: $f(0,1)=5, f(0,2)=3, f(1,2)=2, f(1,3)=3, f(2,3)=5$
en la red s=0, t=3, descompón el flujo en caminos s-t.

<details><summary>Predicción</summary>

Verificar valor: $|f| = 5+3 = 8$.

Descomposición greedy:
1. Camino 0→1→3: flujo = min(5,3) = 3.
   Restante: f(0,1)=2, f(1,3)=0.
2. Camino 0→1→2→3: flujo = min(2,2,5) = 2.
   Restante: f(0,1)=0, f(1,2)=0, f(2,3)=3.
3. Camino 0→2→3: flujo = min(3,3) = 3.
   Restante: todo 0.

Descomposición: {0→1→3: 3, 0→1→2→3: 2, 0→2→3: 3}. Total: 3+2+3 = 8 ✓.

Se usaron 3 caminos (≤ 5 aristas = $|E|$), consistente con el teorema.

</details>

### Ejercicio 9 — MFMC con corte no trivial

Red: 0→1:3, 0→2:3, 1→2:2, 1→3:3, 2→4:3, 3→4:2, 3→5:2, 4→5:4 (s=0, t=5).

Encuentra el flujo máximo, el corte mínimo, y verifica MFMC.

<details><summary>Predicción</summary>

Augmenting paths:
1. 0→1→3→5: bn = min(3,3,2) = 2. Flow = 2.
2. 0→2→4→5: bn = min(3,3,4) = 3. Flow = 5.
3. 0→1→3→4→5: bn = min(1,1,2,1) = 1. Flow = 6.

Wait, let me be more careful.

After path 1: f(0,1)=2, f(1,3)=2, f(3,5)=2. Residuals: r(0,1)=1, r(0,2)=3, r(1,3)=1, r(3,5)=0.
After path 2: f(0,2)=3, f(2,4)=3, f(4,5)=3. Residuals: r(0,2)=0, r(2,4)=0, r(4,5)=1.
After path 3: 0→1→3→4→5: r(0,1)=1, r(1,3)=1, r(3,4)=2, r(4,5)=1. bn = min(1,1,2,1) = 1.

After path 3: f(0,1)=3, f(1,3)=3, f(3,4)=1, f(4,5)=4. Flow = 6.

Try path 4: 0→?→...→5. r(0,1)=0, r(0,2)=0. No more paths from s. **Max flow = 6**.

Corte mínimo: $(\{0\}, \{1,2,3,4,5\})$, capacidad $3+3=6$.
MFMC: $6 = 6$. ✓

</details>

### Ejercicio 10 — Teorema de integralidad

Red con capacidades enteras: 0→1:3, 0→2:2, 1→3:2, 2→3:3, 1→2:1.
Demuestra que el flujo máximo es entero encontrándolo, y explica por qué
Ford-Fulkerson siempre produce flujos enteros con capacidades enteras.

<details><summary>Predicción</summary>

Augmenting paths:
1. 0→1→3: bn = min(3,2) = 2. Flow = 2.
2. 0→2→3: bn = min(2,3) = 2. Flow = 4.
3. 0→1→2→3: bn = min(1,1,1) = 1. Flow = 5.

No more paths. Max flow = **5**. Todas las $f$ son enteras. ✓

**¿Por qué siempre es entero?** Ford-Fulkerson parte de flujo 0 (entero).
En cada iteración:
1. El cuello de botella es $\min$ de capacidades residuales.
2. Las capacidades residuales son $c(u,v) - f(u,v)$ o $f(v,u)$, ambas enteras
   si $c$ y $f$ son enteras.
3. $\min$ de enteros es entero, así que $\delta$ es entero.
4. $f' = f \pm \delta$ mantiene la integralidad.

Por inducción, todo flujo intermedio y el flujo final son enteros.

Esto **no** garantiza terminación con capacidades irracionales (existe un
contraejemplo donde Ford-Fulkerson diverge). Edmonds-Karp (T02) resuelve
esto forzando BFS.

</details>
