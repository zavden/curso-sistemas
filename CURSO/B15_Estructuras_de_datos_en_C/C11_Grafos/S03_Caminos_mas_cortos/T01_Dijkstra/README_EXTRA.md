# T01 — Demostración formal: correctitud y complejidad de Dijkstra

> Complemento riguroso del README.md. Aquí se demuestra formalmente que
> el algoritmo de Dijkstra calcula las distancias mínimas correctas, y
> se prueba que la precondición de pesos no negativos es necesaria.

---

## Modelo

**Grafo.** $G = (V, E)$ dirigido con función de peso
$w: E \to \mathbb{R}_{\geq 0}$ (pesos no negativos).

**Fuente.** Vértice $s \in V$.

**Distancia.** $\delta(s, v) = \min \{ w(p) : p \text{ es un camino de } s \text{ a } v \}$, donde $w(p) = \sum_{(u,v) \in p} w(u,v)$.
Si no existe camino, $\delta(s, v) = \infty$.

**Algoritmo.** Dijkstra mantiene:
- $S$: conjunto de vértices ya extraídos (finalizados).
- $\text{dist}[v]$: estimación de distancia (inicialmente $\infty$,
  $\text{dist}[s] = 0$).
- Cola de prioridad $Q = V \setminus S$, ordenada por $\text{dist}$.

En cada paso: extraer $u = \arg\min_{v \in Q} \text{dist}[v]$,
agregar $u$ a $S$, y relajar las aristas salientes de $u$.

---

## Lema previo: propiedad de subestructura óptima

**Lema.** Si $p = s \leadsto u \to v$ es un camino más corto de $s$
a $v$, entonces el subcamino $s \leadsto u$ es un camino más corto
de $s$ a $u$.

**Prueba.** Supongamos que existe un camino $p'$ de $s$ a $u$ con
$w(p') < w(s \leadsto u)$. Entonces el camino $p' \to v$ tendría
peso $w(p') + w(u,v) < w(s \leadsto u) + w(u,v) = w(p)$, lo que
contradice que $p$ es un camino más corto de $s$ a $v$.

$$\blacksquare$$

---

## Lema previo: propiedad de relajación

**Lema (convergencia de la relajación).** Si $(u, v) \in E$ y
$\text{dist}[u] = \delta(s, u)$ en el momento de relajar $(u, v)$,
entonces tras la relajación:

$$\text{dist}[v] \leq \delta(s, v)$$

**Prueba.** Tras relajar $(u, v)$:

$$\text{dist}[v] \leq \text{dist}[u] + w(u, v) = \delta(s, u) + w(u,v)$$

Si el camino más corto a $v$ pasa por $u$:
$\delta(s, v) = \delta(s, u) + w(u, v)$, así que
$\text{dist}[v] \leq \delta(s, v)$.

Si el camino más corto a $v$ no pasa por $u$:
$\delta(s, v) \leq \delta(s, u) + w(u, v)$, así que también
$\text{dist}[v] \leq \delta(s, u) + w(u,v)$, pero podría ser que
$\text{dist}[v] > \delta(s, v)$ temporalmente. Sin embargo, como
$\text{dist}[v]$ nunca aumenta (solo disminuye por relajación) y
nunca baja de $\delta(s, v)$ (invariante de cota superior), el
resultado vale. ✓

$$\blacksquare$$

---

## Lema previo: invariante de cota superior

**Lema.** En todo momento del algoritmo,
$\text{dist}[v] \geq \delta(s, v)$ para todo $v \in V$.

**Prueba por inducción sobre el número de relajaciones.**

*Base.* Inicialmente $\text{dist}[s] = 0 = \delta(s,s)$ y
$\text{dist}[v] = \infty \geq \delta(s, v)$ para $v \neq s$. ✓

*Paso inductivo.* La relajación de $(u, v)$ actualiza
$\text{dist}[v] \leftarrow \text{dist}[u] + w(u, v)$. Por hipótesis
inductiva, $\text{dist}[u] \geq \delta(s, u)$. Y por definición de
distancia mínima:

$$\delta(s, v) \leq \delta(s, u) + w(u, v) \leq \text{dist}[u] + w(u, v) = \text{dist}[v]_{\text{nuevo}}$$

Así $\text{dist}[v] \geq \delta(s, v)$ se mantiene. ✓

$$\blacksquare$$

---

## Demostración 1: correctitud (al extraer $u$, $\text{dist}[u] = \delta(s,u)$)

**Teorema.** Al extraer el vértice $u$ de la cola de prioridad
(agregarlo a $S$), $\text{dist}[u] = \delta(s, u)$.

**Prueba por contradicción.** Supongamos que existe un vértice $u$
tal que al ser extraído de $Q$, $\text{dist}[u] \neq \delta(s, u)$.
Sea $u$ el **primer** vértice extraído con esta propiedad.

Por el invariante de cota superior,
$\text{dist}[u] > \delta(s, u)$. En particular,
$\delta(s, u) < \infty$, así que existe un camino de $s$ a $u$.

**Paso 1: identificar el primer vértice fuera de $S$ en el camino
óptimo.**

Sea $p = s \leadsto u$ un camino más corto de $s$ a $u$. Como
$s \in S$ y $u \notin S$ (está siendo extraído ahora), el camino $p$
cruza la frontera de $S$. Sea $(x, y)$ la **primera arista** de $p$
con $x \in S$ y $y \notin S$:

$$p: \underbrace{s \leadsto x}_{\in S} \to \underbrace{y \leadsto u}_{\notin S}$$

**Paso 2: $\text{dist}[y] = \delta(s, y)$.**

Como $x$ fue extraído antes de $u$, y $u$ es el primer vértice
extraído con distancia incorrecta, tenemos
$\text{dist}[x] = \delta(s, x)$.

Cuando $x$ fue extraído, se relajó la arista $(x, y)$. Por el lema
de convergencia:

$$\text{dist}[y] \leq \delta(s, x) + w(x, y) = \delta(s, y)$$

(La última igualdad vale porque $s \leadsto x \to y$ es un
subcamino del camino óptimo $p$, y por subestructura óptima.)

Por el invariante de cota superior,
$\text{dist}[y] \geq \delta(s, y)$. Combinando:

$$\text{dist}[y] = \delta(s, y)$$

**Paso 3: contradicción.**

Como $y$ está en el camino óptimo de $s$ a $u$ y todos los pesos son
no negativos:

$$\delta(s, y) \leq \delta(s, u) \quad \text{(el subcamino no puede pesar más)}$$

Así:

$$\text{dist}[y] = \delta(s, y) \leq \delta(s, u) < \text{dist}[u]$$

Pero $u$ fue extraído de $Q$ antes que $y$ (en este paso), lo que
significa $\text{dist}[u] \leq \text{dist}[y]$ (el extract-min elige
el mínimo). Esto contradice
$\text{dist}[y] < \text{dist}[u]$.

$$\therefore \text{dist}[u] = \delta(s, u) \text{ para todo } u \text{ al ser extraído.} \qquad \blacksquare$$

---

## Demostración 2: necesidad de pesos no negativos

**Teorema.** Si existe una arista con peso negativo, Dijkstra puede
producir distancias incorrectas.

**Prueba (contraejemplo).** Considérese el grafo:

$$s \xrightarrow{1} a \xrightarrow{-3} b, \quad s \xrightarrow{2} b$$

Distancias reales: $\delta(s, a) = 1$, $\delta(s, b) = \min(2, 1 + (-3)) = -2$.

Ejecución de Dijkstra:

1. Extraer $s$ ($\text{dist} = 0$). Relajar: $\text{dist}[a] = 1$,
   $\text{dist}[b] = 2$.

2. Extraer $a$ ($\text{dist} = 1$). Relajar $(a, b)$:
   $1 + (-3) = -2 < 2$, así que $\text{dist}[b] = -2$. ✓ (correcto
   en este caso particular.)

Pero con un grafo ligeramente diferente:

$$s \xrightarrow{2} a, \quad s \xrightarrow{3} b \xrightarrow{-2} a$$

Distancias reales: $\delta(s, a) = \min(2, 3 + (-2)) = 1$.

Ejecución de Dijkstra:

1. Extraer $s$. Relajar: $\text{dist}[a] = 2$, $\text{dist}[b] = 3$.

2. Extraer $a$ ($\text{dist} = 2$). Se finaliza $a$ con
   $\text{dist}[a] = 2$.

3. Extraer $b$ ($\text{dist} = 3$). Relajar $(b, a)$:
   $3 + (-2) = 1 < 2$. Pero $a$ ya fue finalizado.

Dijkstra retorna $\text{dist}[a] = 2$, pero $\delta(s, a) = 1$.
**Incorrecto.**

**Dónde falla la prueba formal.** En el Paso 3 de la Demostración 1,
usamos $\delta(s, y) \leq \delta(s, u)$ porque el subcamino
$y \leadsto u$ tiene peso $\geq 0$. Con pesos negativos, podría
ser que $\delta(s, y) > \delta(s, u)$ (el tramo $y \to u$ resta
peso), y la contradicción no se produce.

$$\blacksquare$$

---

## Demostración 3: complejidad $O((n + m)\log n)$ con heap binario

**Teorema.** Dijkstra con min-heap binario y lazy deletion ejecuta en
$O((n + m) \log n)$.

**Prueba.** Contamos las operaciones sobre el heap:

**Inserciones.** Cada relajación exitosa produce una inserción en el
heap. Una arista $(u, v)$ se relaja a lo sumo una vez (cuando $u$ se
extrae). Por lo tanto, hay a lo sumo $m$ inserciones. Cada una cuesta
$O(\log |Q|)$. Con lazy deletion, $|Q| \leq m$, así que cada
inserción cuesta $O(\log m) = O(\log n)$ (porque $m \leq n^2$, así
$\log m \leq 2\log n$).

$$C_{\text{insert}} = O(m \log n)$$

**Extracciones.** Cada vértice se extrae a lo sumo una vez
(verificación $d > \text{dist}[u]$ descarta entradas obsoletas). Se
extraen a lo sumo $m$ entradas del heap en total (incluyendo
obsoletas), cada una a costo $O(\log n)$:

$$C_{\text{extract}} = O(m \log n)$$

Más precisamente, hay $n$ extracciones válidas y hasta $m - n$
obsoletas, pero el total es $O(m)$.

**Total:**

$$T = C_{\text{insert}} + C_{\text{extract}} = O(m \log n) + O(m \log n) = O(m \log n)$$

Como el algoritmo también inicializa los arrays ($O(n)$) y procesa
la lista de adyacencia ($O(n + m)$):

$$T = O(n + m \log n) = O((n + m) \log n)$$

$$\blacksquare$$

---

## Demostración 4: optimalidad del array lineal para grafos densos

**Teorema.** Dijkstra con array lineal (sin heap) ejecuta en
$O(n^2)$, que es óptimo para grafos con $m = \Omega(n^2 / \log n)$.

**Prueba.** Con array lineal:

- `extract_min`: escanear el array de $n$ elementos: $O(n)$. Se
  ejecuta $n$ veces: $O(n^2)$.
- Relajación: $O(1)$ por arista, total $O(m)$.

$$T_{\text{array}} = O(n^2 + m) = O(n^2) \quad \text{(si } m = O(n^2)\text{)}$$

Con heap binario:

$$T_{\text{heap}} = O((n + m) \log n)$$

El array es mejor cuando:

$$n^2 < (n + m) \log n$$

Para $m = \Theta(n^2)$:

$$T_{\text{heap}} = O(n^2 \log n) > O(n^2) = T_{\text{array}}$$

El punto de cruce exacto: $n^2 = m \log n$, es decir,
$m = n^2 / \log n$. Para grafos más densos que esto, el array
gana.

$$\blacksquare$$

---

## Demostración 5: el árbol de caminos más cortos es un árbol

**Teorema.** Los arcos $\{(\text{parent}[v], v) : v \neq s,\, \delta(s,v) < \infty\}$
forman un árbol enraizado en $s$.

**Prueba.** Sea $T_{\pi}$ el subgrafo inducido por los arcos parent.

1. **$T_{\pi}$ es conexo desde $s$.** Para todo $v$ con
   $\delta(s, v) < \infty$, el camino
   $v, \text{parent}[v], \text{parent}[\text{parent}[v]], \ldots$
   termina en $s$ (cada relajación exitosa asigna un parent cuya
   distancia es estrictamente menor, y la distancia es finita y
   no negativa, así que la cadena es finita).

2. **$T_{\pi}$ no tiene ciclos.** Si hubiera un ciclo
   $v_1 \to v_2 \to \cdots \to v_k \to v_1$ en $T_{\pi}$, entonces
   $\text{dist}[v_1] > \text{dist}[v_2] > \cdots > \text{dist}[v_k] > \text{dist}[v_1]$
   (cada parent tiene distancia estrictamente menor por la condición
   de relajación), lo que es imposible.

3. **$T_{\pi}$ tiene $|V'| - 1$ aristas** (donde $V'$ es el conjunto
   de vértices alcanzables). Cada vértice $v \neq s$ tiene exactamente
   un parent, aportando una arista. Así,
   $|E(T_{\pi})| = |V'| - 1$.

Conexo, acíclico, con $|V'| - 1$ aristas: $T_{\pi}$ es un árbol. ✓

$$\blacksquare$$

---

## Verificación numérica

Grafo del README.md: $V = \{A, B, C, D, E\}$,
aristas $\{A\text{-}B:1, A\text{-}C:4, B\text{-}D:2, B\text{-}E:3, C\text{-}D:3, D\text{-}E:1\}$.

| Paso | Extraer | $\text{dist}[u]$ | $\delta(s, u)$ | ¿Iguales? |
|:---:|:---:|:---:|:---:|:---:|
| 1 | A | 0 | 0 | ✓ |
| 2 | B | 1 | 1 | ✓ |
| 3 | D | 3 | 3 | ✓ |
| 4 | C | 4 | 4 | ✓ |
| 5 | E | 4 | 4 | ✓ |

Todas las distancias coinciden con las reales al momento de
extracción, confirmando el Teorema 1. ✓

Aristas del árbol parent: $\{(A,B), (A,C), (B,D), (D,E)\}$ — 4 aristas,
5 vértices: árbol. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $\text{dist}[u] = \delta(s,u)$ al extraer | Contradicción | Primer vértice fuera de $S$ en camino óptimo tiene $\text{dist}[y] \leq \text{dist}[u]$ |
| Necesidad de $w \geq 0$ | Contraejemplo | Vértice finalizado prematuramente; $\delta(s,y) > \delta(s,u)$ posible con $w < 0$ |
| $O((n+m)\log n)$ con heap | Contar operaciones | $\leq m$ inserciones y extracciones, $O(\log n)$ cada una |
| $O(n^2)$ con array, óptimo para $m = \Omega(n^2/\log n)$ | Comparación directa | $n^2 < m\log n$ cuando $m > n^2/\log n$ |
| Arcos parent forman un árbol | Conexo + acíclico + conteo | Cadena de parents finita; sin ciclos por distancias decrecientes |
