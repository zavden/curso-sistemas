# T03 — Demostración formal: Prim produce un MST por la propiedad de corte

> Complemento riguroso del README.md. Aquí se demuestra formalmente que
> el algoritmo de Prim produce un árbol generador de peso mínimo (MST),
> usando un invariante de bucle basado en la propiedad de corte.

---

## Modelo

**Grafo.** $G = (V, E)$ no dirigido, conexo, con función de peso
$w: E \to \mathbb{R}$. $|V| = n$, $|E| = m$.

**Algoritmo.** Prim mantiene un conjunto $S$ de vértices ya incluidos
en el MST parcial (inicialmente $S = \{s\}$ para un vértice fuente
arbitrario $s$). En cada paso, agrega la arista de peso mínimo que
cruza el corte $(S, V \setminus S)$, incorporando su extremo
$v \in V \setminus S$ a $S$.

**Convención.** Pesos distintos (la prueba se adapta con desempate
consistente para pesos iguales).

---

## Demostración 1: correctitud por invariante de bucle

**Teorema.** Al terminar, las aristas seleccionadas por Prim forman
un MST.

**Invariante.** Sea $A_k$ el conjunto de aristas seleccionadas tras
$k$ iteraciones. Definimos:

$$\text{INV}(k): A_k \text{ es subconjunto de algún MST de } G$$

**Inicialización ($k = 0$).** $A_0 = \emptyset$, que es subconjunto
de cualquier MST. ✓

**Mantenimiento.** Supongamos $\text{INV}(k)$: existe un MST $T^*$
con $A_k \subseteq T^*$. En la iteración $k + 1$, Prim selecciona
la arista $e = (u, v)$ de peso mínimo que cruza el corte
$(S_k, V \setminus S_k)$, donde $S_k$ es el conjunto de vértices
incluidos tras $k$ iteraciones.

**Caso 1: $e \in T^*$.** Entonces
$A_{k+1} = A_k \cup \{e\} \subseteq T^*$. ✓

**Caso 2: $e \notin T^*$.** Agregar $e$ a $T^*$ crea un ciclo $C$.
El ciclo $C$ contiene $e = (u, v)$ con $u \in S_k$ y
$v \in V \setminus S_k$, así que $C$ cruza el corte al menos dos
veces. Sea $e' \neq e$ otra arista de $C$ que cruza el corte
$(S_k, V \setminus S_k)$, con $e' \in T^*$.

Como $e$ es la arista de peso mínimo que cruza el corte:
$w(e) < w(e')$ (pesos distintos).

Definimos $T^{**} = T^* \setminus \{e'\} \cup \{e\}$.

- $T^{**}$ es un árbol generador (eliminar $e'$ del ciclo $C$ y
  mantener $e$ preserva la conexión; $n - 1$ aristas).
- $w(T^{**}) = w(T^*) - w(e') + w(e) < w(T^*)$.

Pero $T^*$ era un MST, así que $w(T^{**}) \geq w(T^*)$.
Contradicción. $\therefore$ el Caso 2 no ocurre (con pesos
distintos).

Con pesos iguales: $w(e) \leq w(e')$, y $T^{**}$ es también un MST
con $A_{k+1} \subseteq T^{**}$. ✓

**Verificación de que $e' \notin A_k$.** La arista $e'$ cruza el
corte $(S_k, V \setminus S_k)$ y está en $T^*$. Si $e' \in A_k$,
entonces $e'$ fue seleccionada en una iteración anterior $j < k$,
cuando cruzaba el corte $(S_j, V \setminus S_j)$. Pero entonces
ambos extremos de $e'$ estarían en $S_k$ (el extremo de
$V \setminus S_j$ fue incorporado a $S$ al seleccionar $e'$).
Esto contradice que $e'$ cruza $(S_k, V \setminus S_k)$.
$\therefore e' \notin A_k$, y el intercambio es válido. ✓

**Terminación ($k = n - 1$).** $A_{n-1}$ contiene $n - 1$ aristas,
$S = V$, y $A_{n-1}$ es subconjunto de algún MST. Como un MST tiene
exactamente $n - 1$ aristas, $A_{n-1}$ **es** un MST.

$$\blacksquare$$

---

## Demostración 2: key[v] es la arista ligera del corte que conecta a v

**Teorema.** Al momento de extraer $v$ de la cola de prioridad,
$\text{key}[v]$ es el peso de la arista de peso mínimo entre $v$ y
algún vértice de $S$.

**Prueba.** Se mantiene el invariante:

$$\text{key}[v] = \min\{w(u, v) : u \in S,\, (u, v) \in E\}$$

(o $\infty$ si no existe tal arista).

*Inicialización.* $S = \{s\}$. Para cada vecino $v$ de $s$:
$\text{key}[v] = w(s, v)$. Para no vecinos: $\text{key}[v] = \infty$.
Correcto. ✓

*Mantenimiento.* Al agregar $u$ a $S$, para cada vecino $v \notin S$
de $u$: si $w(u, v) < \text{key}[v]$, se actualiza
$\text{key}[v] = w(u, v)$. Esto incorpora la nueva información de
que $u$ ahora está en $S$.

Ninguna arista más ligera entre $v$ y $S$ puede quedar sin
considerar: la única información nueva es que $u \in S$, y se examina
$w(u, v)$. Las aristas desde vértices anteriores de $S$ ya fueron
consideradas cuando esos vértices se agregaron. ✓

**Corolario.** La arista seleccionada $(u, v)$ con
$\text{key}[v] = \min_{v \notin S} \text{key}[v]$ es la arista de
peso mínimo que cruza $(S, V \setminus S)$. Esto es exactamente lo
que requiere la propiedad de corte.

$$\blacksquare$$

---

## Demostración 3: Prim y Dijkstra son estructuralmente idénticos

**Teorema.** Prim y Dijkstra tienen la misma estructura algorítmica;
difieren únicamente en la clave de la cola de prioridad.

**Prueba (por comparación directa).**

| Aspecto | Dijkstra | Prim |
|---------|----------|------|
| Cola de prioridad | $\text{dist}[v] = \delta(s, v)$ estimada | $\text{key}[v] = \min w(u,v)$ para $u \in S$ |
| Actualización | $\text{dist}[v] = \min(\text{dist}[v],\, \text{dist}[u] + w(u,v))$ | $\text{key}[v] = \min(\text{key}[v],\, w(u,v))$ |
| Optimiza | Distancia acumulada desde $s$ | Peso de una sola arista |
| Resultado | Árbol de caminos más cortos | Árbol generador mínimo |

La diferencia es una sola línea:

```
// Dijkstra
if dist[u] + w(u,v) < dist[v]:  dist[v] = dist[u] + w(u,v)

// Prim
if w(u,v) < key[v]:             key[v] = w(u,v)
```

Dijkstra acumula ($\text{dist}[u] + w$); Prim no ($w$ solo). Ambos
extraen el mínimo de la cola y relajan vecinos. La estructura de
bucle, las operaciones sobre el heap, y el patrón de visita son
idénticos.

**Corolario.** La complejidad de Prim es idéntica a la de Dijkstra:
$O(n^2)$ con array, $O(m \log n)$ con heap binario, $O(m + n\log n)$
con Fibonacci heap.

$$\blacksquare$$

---

## Demostración 4: complejidad $O(m \log n)$ con heap binario

**Teorema.** Prim con min-heap binario (y lazy deletion) ejecuta en
$O(m \log n)$.

**Prueba.** Contamos operaciones sobre el heap:

**Extracciones.** Se extraen a lo sumo $n$ vértices válidos y hasta
$m - n$ entradas obsoletas (lazy deletion). Total: $O(m)$
extracciones, cada una $O(\log m) = O(\log n)$.

$$C_{\text{extract}} = O(m \log n)$$

**Inserciones.** Cada relajación exitosa ($w(u,v) < \text{key}[v]$)
produce un push al heap. Cada arista se examina a lo sumo dos veces
(una por extremo), así que hay a lo sumo $2m$ relajaciones. Pero cada
arista solo puede mejorar el key de un vértice que no está en $S$, y
se procesan $O(m)$ pares $(u, v)$ en total. Inserciones: $O(m)$,
cada una $O(\log n)$.

$$C_{\text{insert}} = O(m \log n)$$

**Total:**

$$T = O(m \log n)$$

**Con decrease-key (heap indexado):** A lo sumo $m$ decrease-keys,
cada una $O(\log n)$, y $n$ extracciones $O(\log n)$:

$$T = O((m + n) \log n) = O(m \log n)$$

(Mismo orden asintótico, menor constante por no tener entradas
duplicadas.)

$$\blacksquare$$

---

## Demostración 5: Prim con array es óptimo para grafos densos

**Teorema.** Para grafos con $m = \Theta(n^2)$, Prim con array lineal
($O(n^2)$) es asintóticamente óptimo.

**Prueba.**

*Cota superior.* Ya demostrada: $O(n^2)$ con array.

*Cota inferior.* Cualquier algoritmo MST debe examinar todas las
aristas (de lo contrario, una arista no examinada de peso $-\infty$
invalidaría el resultado). Para grafos densos con
$m = \Theta(n^2)$:

$$T \geq \Omega(m) = \Omega(n^2)$$

Combinando: $T = \Theta(n^2)$ para Prim con array en grafos densos.

**Comparación con heap para grafos densos ($m = n^2$):**

$$T_{\text{array}} = O(n^2), \quad T_{\text{heap}} = O(n^2 \log n)$$

El array gana por un factor de $\log n$. Para $n = 10^4$ con grafo
completo: $n^2 = 10^8$ vs $n^2 \log n \approx 1.3 \times 10^9$
— el heap es $\sim 13\times$ más lento.

$$\blacksquare$$

---

## Verificación numérica

Grafo del README.md: 6 vértices, fuente 0.

| Iter. | Extraer $v$ | $\text{key}[v]$ | Arista MST | Corte $(S, V \setminus S)$ | ¿Mín. que cruza? |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 1 | 0 | 0 | — | $(\{0\}, \{1,2,3,4,5\})$ | — |
| 2 | 3 | 3 | (0,3) | $(\{0\}, \{1,2,3,4,5\})$ | Sí: 3 < 4 ✓ |
| 3 | 1 | 4 | (0,1) | $(\{0,3\}, \{1,2,4,5\})$ | Sí: 4 < 8,9 ✓ |
| 4 | 2 | 2 | (1,2) | $(\{0,1,3\}, \{2,4,5\})$ | Sí: 2 < 5,6,7,9 ✓ |
| 5 | 4 | 5 | (2,4) | $(\{0,1,2,3\}, \{4,5\})$ | Sí: 5 < 6,7,9 ✓ |
| 6 | 5 | 1 | (4,5) | $(\{0,1,2,3,4\}, \{5\})$ | Sí: 1 < 7 ✓ |

Peso total: $3 + 4 + 2 + 5 + 1 = 15$. Idéntico a Kruskal. ✓

En cada paso, la arista seleccionada es efectivamente la de peso
mínimo que cruza el corte, confirmando la propiedad de corte. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| Prim produce MST | Invariante $A_k \subseteq T^*$ + propiedad de corte | Intercambio $T^{**} = T^* \setminus \{e'\} \cup \{e\}$ |
| key[v] = arista ligera del corte | Invariante de la cola de prioridad | Actualizar al agregar cada $u$ a $S$ |
| Prim $\equiv$ Dijkstra estructuralmente | Comparación directa | Diferencia: $w(u,v)$ vs $\text{dist}[u] + w(u,v)$ |
| $O(m \log n)$ con heap | Contar operaciones | $O(m)$ inserciones/extracciones × $O(\log n)$ |
| $\Theta(n^2)$ óptimo para grafos densos | Cota inferior $\Omega(m)$ | Toda arista debe examinarse |
