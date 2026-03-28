# T02 — Demostración formal: correctitud de Bellman-Ford por inducción

> Complemento riguroso del README.md. Aquí se demuestra formalmente por
> inducción que Bellman-Ford calcula las distancias mínimas, se prueba
> la correctitud de la detección de ciclos negativos, y se establece
> la cota de complejidad.

---

## Modelo

**Grafo.** $G = (V, E)$ dirigido con función de peso
$w: E \to \mathbb{R}$ (pesos arbitrarios, incluidos negativos).

**Fuente.** Vértice $s \in V$, $|V| = n$, $|E| = m$.

**Distancia.** $\delta(s, v)$ = peso del camino más corto de $s$ a
$v$, o $-\infty$ si $v$ es alcanzable desde un ciclo negativo, o
$\infty$ si $v$ no es alcanzable.

**Distancia con cota de aristas.** $\delta_k(s, v)$ = peso del camino
más corto de $s$ a $v$ que usa **a lo sumo $k$ aristas**.

**Algoritmo.** Bellman-Ford realiza $n - 1$ pasadas. En la pasada $i$
($i = 1, \ldots, n-1$), relaja todas las aristas $(u, v) \in E$.

---

## Lema previo: caminos más cortos simples

**Lema.** Si no existen ciclos negativos alcanzables desde $s$,
entonces para todo vértice $v$ alcanzable desde $s$, existe un camino
más corto de $s$ a $v$ que es **simple** (sin vértices repetidos) y
tiene a lo sumo $n - 1$ aristas.

**Prueba.** Sea $p$ un camino más corto de $s$ a $v$. Si $p$ contiene
un ciclo $c$, el peso de $c$ es $w(c) \geq 0$ (por hipótesis, no hay
ciclos negativos). Eliminando $c$ de $p$ obtenemos un camino $p'$ con:

$$w(p') = w(p) - w(c) \leq w(p) = \delta(s, v)$$

Como $p$ era un camino más corto, $w(p') = w(p)$, así que $p'$
también es un camino más corto. Repitiendo hasta eliminar todos los
ciclos, obtenemos un camino simple.

Un camino simple en un grafo de $n$ vértices visita a lo sumo $n$
vértices, y por tanto usa a lo sumo $n - 1$ aristas.

$$\blacksquare$$

---

## Demostración 1: correctitud por inducción

**Teorema.** Tras la pasada $i$ de Bellman-Ford ($i = 1, \ldots, n-1$):

$$\text{dist}[v] \leq \delta_i(s, v) \quad \forall\, v \in V$$

En particular, tras $n - 1$ pasadas,
$\text{dist}[v] = \delta(s, v)$ para todo $v$ (asumiendo que no hay
ciclos negativos alcanzables).

**Prueba por inducción fuerte sobre $i$.**

**Base ($i = 0$, antes de cualquier pasada).**

$\text{dist}[s] = 0 = \delta_0(s, s)$. Para $v \neq s$:
$\text{dist}[v] = \infty$, y $\delta_0(s, v) = \infty$ (no hay
camino de 0 aristas a $v \neq s$). Así $\text{dist}[v] \leq \delta_0(s, v)$. ✓

**Paso inductivo.** Supongamos que tras la pasada $i - 1$:

$$\text{dist}[v] \leq \delta_{i-1}(s, v) \quad \forall\, v \in V \tag{H.I.}$$

Queremos probar que tras la pasada $i$:
$\text{dist}[v] \leq \delta_i(s, v)$.

Sea $v$ un vértice con $\delta_i(s, v) < \infty$. Sea
$p = s \to v_1 \to v_2 \to \cdots \to u \to v$ un camino más corto
de $s$ a $v$ con a lo sumo $i$ aristas. El subcamino
$s \to v_1 \to \cdots \to u$ tiene a lo sumo $i - 1$ aristas, y por
subestructura óptima:

$$w(s \leadsto u) = \delta_{i-1}(s, u)$$

Por la hipótesis inductiva, tras la pasada $i - 1$:

$$\text{dist}[u] \leq \delta_{i-1}(s, u) \tag{1}$$

En la pasada $i$, se relaja la arista $(u, v)$. Tras la relajación:

$$\text{dist}[v] \leq \text{dist}[u] + w(u, v) \tag{2}$$

(Si $\text{dist}[v]$ ya era menor, la relajación no lo cambia, y la
desigualdad sigue valiendo.)

Combinando (1) y (2):

$$\text{dist}[v] \leq \delta_{i-1}(s, u) + w(u, v) = \delta_i(s, v) \tag{3}$$

Además, $\text{dist}[v]$ nunca aumenta (solo se actualiza cuando
disminuye), y siempre satisface $\text{dist}[v] \geq \delta(s, v)$
por el invariante de cota superior (misma prueba que en Dijkstra:
inducción sobre el número de relajaciones).

**Conclusión tras $n - 1$ pasadas.** Por el lema de caminos simples,
$\delta(s, v) = \delta_{n-1}(s, v)$ si no hay ciclos negativos. Así:

$$\delta(s, v) \leq \text{dist}[v] \leq \delta_{n-1}(s, v) = \delta(s, v)$$

$$\therefore \text{dist}[v] = \delta(s, v) \qquad \blacksquare$$

---

## Demostración 2: correctitud de la detección de ciclos negativos

**Teorema.** Tras las $n - 1$ pasadas, si existe una arista
$(u, v) \in E$ tal que $\text{dist}[u] + w(u,v) < \text{dist}[v]$,
entonces el grafo contiene un ciclo negativo alcanzable desde $s$.

**Prueba por contraposición.** Demostramos la contrapositiva: si no
hay ciclos negativos alcanzables desde $s$, entonces ninguna arista se
puede relajar en la pasada $n$-ésima.

Si no hay ciclos negativos, por la Demostración 1:

$$\text{dist}[v] = \delta(s, v) \quad \forall\, v \in V$$

Para cualquier arista $(u, v) \in E$, la **desigualdad triangular**
establece:

$$\delta(s, v) \leq \delta(s, u) + w(u, v)$$

(El camino más corto a $v$ no puede ser peor que ir a $u$ y luego
tomar la arista $(u, v)$.)

Sustituyendo $\text{dist}[u] = \delta(s, u)$ y
$\text{dist}[v] = \delta(s, v)$:

$$\text{dist}[v] \leq \text{dist}[u] + w(u, v)$$

Así que la condición de relajación
$\text{dist}[u] + w(u, v) < \text{dist}[v]$ es **falsa** para toda
arista. Ninguna arista se puede relajar. ✓

**Contrapositiva:** si alguna arista se puede relajar en la pasada
$n$, entonces existe un ciclo negativo.

$$\blacksquare$$

**Teorema (recíproco).** Si existe un ciclo negativo alcanzable desde
$s$, entonces la pasada $n$-ésima detecta al menos una arista
relajable.

**Prueba.** Sea $c = v_0 \to v_1 \to \cdots \to v_k \to v_0$ un
ciclo negativo alcanzable desde $s$ con peso $\sum_{j=0}^{k} w(v_j, v_{j+1}) < 0$ (donde $v_{k+1} = v_0$).

Supongamos por contradicción que tras $n - 1$ pasadas, ninguna arista
del ciclo se puede relajar:

$$\text{dist}[v_{j+1}] \leq \text{dist}[v_j] + w(v_j, v_{j+1}) \quad \forall\, j = 0, \ldots, k$$

Sumando sobre las $k + 1$ aristas del ciclo:

$$\sum_{j=0}^{k} \text{dist}[v_{j+1}] \leq \sum_{j=0}^{k} \text{dist}[v_j] + \sum_{j=0}^{k} w(v_j, v_{j+1})$$

El lado izquierdo y el primer sumando del lado derecho son iguales
(ambos suman $\text{dist}$ sobre los mismos vértices del ciclo):

$$0 \leq \sum_{j=0}^{k} w(v_j, v_{j+1})$$

Pero $\sum w(v_j, v_{j+1}) < 0$ (ciclo negativo). Contradicción.

$\therefore$ Al menos una arista del ciclo es relajable en la pasada $n$.

$$\blacksquare$$

---

## Demostración 3: complejidad $\Theta(nm)$

**Teorema.** Bellman-Ford ejecuta en $\Theta(nm)$ en el peor caso.

**Prueba.**

*Cota superior.* El algoritmo consiste en:
- Inicialización: $O(n)$.
- $n - 1$ pasadas, cada una relajando $m$ aristas: $O((n-1) \cdot m)$.
- Pasada de verificación: $O(m)$.

$$T = O(n + (n-1)m + m) = O(nm)$$

*Cota inferior.* Considérese un camino simple
$s \to v_1 \to v_2 \to \cdots \to v_{n-1}$ con aristas en orden
inverso en la lista de aristas:
$(v_{n-2}, v_{n-1}), \ldots, (v_1, v_2), (s, v_1)$.

En la pasada 1, solo se relaja exitosamente $(s, v_1)$ (la última
arista procesada). En la pasada 2, solo $(v_1, v_2)$. Se necesitan
exactamente $n - 1$ pasadas, cada una procesando las $m$ aristas.

$$T = \Omega(nm)$$

Combinando: $T = \Theta(nm)$.

$$\blacksquare$$

---

## Demostración 4: terminación temprana es correcta

**Teorema.** Si una pasada completa no produce ninguna relajación
exitosa, el algoritmo puede terminar: las distancias son correctas.

**Prueba.** Si ninguna arista $(u, v)$ satisface
$\text{dist}[u] + w(u, v) < \text{dist}[v]$, entonces:

$$\text{dist}[v] \leq \text{dist}[u] + w(u, v) \quad \forall\, (u,v) \in E$$

Esto es exactamente la condición que verifica la pasada $n$-ésima.
Por la Demostración 2, no hay ciclos negativos, y por la
Demostración 1, $\text{dist}[v] = \delta(s, v)$ para todo $v$.

Pasadas adicionales no cambiarían ningún $\text{dist}[v]$ (no hay
relajaciones exitosas posibles), así que el resultado ya es correcto.

**Ganancia en el mejor caso.** Si el grafo no tiene pesos negativos
y las aristas están en "buen" orden (ej: BFS order), puede bastar
una sola pasada. Mejor caso: $O(m)$.

$$\blacksquare$$

---

## Demostración 5: relación entre pasadas y longitud de caminos

**Teorema.** Tras exactamente $k$ pasadas, $\text{dist}[v]$ es el
peso del camino más corto de $s$ a $v$ con a lo sumo $k$ aristas:

$$\text{dist}^{(k)}[v] = \delta_k(s, v)$$

(Asumiendo que $\text{dist}$ se actualiza usando una copia del array
de la pasada anterior, sin "contaminación" intra-pasada.)

**Prueba.** Idéntica a la Demostración 1, pero con la observación
adicional de que si se usa el array actualizado dentro de la misma
pasada (como en la implementación estándar), la propiedad es:

$$\text{dist}^{(k)}[v] \leq \delta_k(s, v)$$

(Puede converger **más rápido** que $k$ pasadas, porque las
actualizaciones intra-pasada propagan información adicional.)

**Corolario.** La versión con "contaminación" (usar dist actualizado
inmediatamente) es **al menos tan rápida** como la versión pura:
nunca necesita más pasadas, y frecuentemente menos.

**Ejemplo.** Camino $s \to a \to b \to c$ con aristas en ese orden en
la lista. En la pasada 1:
- Relajar $(s, a)$: $\text{dist}[a] = w(s,a)$. ✓
- Relajar $(a, b)$: usa $\text{dist}[a]$ recién actualizado:
  $\text{dist}[b] = w(s,a) + w(a,b)$. ✓
- Relajar $(b, c)$: $\text{dist}[c] = w(s,a) + w(a,b) + w(b,c)$. ✓

Una sola pasada calculó un camino de 3 aristas. Con la versión pura,
se necesitarían 3 pasadas.

$$\blacksquare$$

---

## Verificación numérica

Grafo del README.md: $V = \{0,1,2,3,4\}$, aristas con pesos
(incluido $-4$ y $-3$).

| Pasada $i$ | $\text{dist}[0..4]$ | Aristas relajadas exitosamente |
|:---:|:---:|:---:|
| 0 (init) | $[0, \infty, \infty, \infty, \infty]$ | — |
| 1 | $[0, 6, 7, 2, 4]$ | $(0,1), (0,2), (1,3), (1,4), (2,4)$ |
| 2 | $[0, 6, 7, 2, 4]$ | ninguna |

Terminación temprana tras pasada 2 (0 relajaciones). ✓

Verificación de $\delta_k$:
- $\delta_1(0, 3) = w(0 \to 1 \to 3) = 6 + (-4) = 2$. Pero esto usa
  2 aristas... Con contaminación intra-pasada, $\text{dist}[1] = 6$
  se calculó antes de relajar $(1, 3)$, permitiendo
  $\text{dist}[3] = 2$ en la pasada 1. ✓ (Convergencia acelerada.)

Verificación de la pasada $n$: ninguna arista relajable $\Rightarrow$
no hay ciclo negativo. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $\text{dist}[v] = \delta(s,v)$ tras $n{-}1$ pasadas | Inducción sobre pasadas | Pasada $i$ corrige caminos de $\leq i$ aristas |
| Detección de ciclos negativos | Contrapositiva + contradicción | Sumar desigualdades en el ciclo da $0 \leq w(c) < 0$ |
| $\Theta(nm)$ | Cota sup. + cota inf. (aristas en orden inverso) | Peor caso: 1 arista corregida por pasada |
| Terminación temprana | Sin relajación $\Rightarrow$ desigualdad triangular | Equivale a la verificación de pasada $n$ |
| Contaminación intra-pasada no rompe correctitud | $\text{dist}^{(k)}[v] \leq \delta_k(s,v)$ | Solo converge más rápido, nunca peor |
