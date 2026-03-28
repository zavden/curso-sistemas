# T03 — Demostración formal: quicksort aleatorizado es $O(n \log n)$ en esperanza

> Complemento riguroso del README.md. Aquí se prueba formalmente que
> quicksort con pivote aleatorio uniforme tiene complejidad esperada
> $O(n \log n)$ para **cualquier** entrada fija, eliminando la
> dependencia en la distribución de los datos.

---

## Modelo

**Entrada.** Un array fijo $A[0..n-1]$ de $n$ elementos distintos
(la entrada **no** es aleatoria).

**Algoritmo.** Quicksort con pivote aleatorio: en cada llamada
recursiva sobre un subarreglo de tamaño $m$, se elige un pivote
uniformemente al azar entre los $m$ elementos del subarreglo.

**La aleatoriedad es del algoritmo, no de la entrada.** A diferencia
del análisis de caso promedio (T02), aquí la entrada es fija y
arbitraria. La esperanza se toma sobre las elecciones aleatorias del
pivote.

---

## Distinción clave: caso promedio vs aleatorizado

| | Caso promedio (T02) | Aleatorizado (este archivo) |
|---|---|---|
| Entrada | Permutación uniforme aleatoria | **Fija, arbitraria** |
| Pivote | Determinístico (ej: último) | **Aleatorio uniforme** |
| Fuente de aleatoriedad | Los datos | El algoritmo |
| Garantía | "En promedio sobre entradas" | "Para toda entrada, en esperanza" |
| ¿Existe entrada adversarial? | Sí ($O(n^2)$ determinístico) | **No** |

La conclusión numérica es la misma ($\approx 1.39 \cdot n\log_2 n$
comparaciones esperadas), pero la garantía es fundamentalmente más
fuerte en el caso aleatorizado.

---

## Demostración 1: la esperanza es independiente de la entrada

**Teorema.** Sea $A$ cualquier array fijo de $n$ elementos distintos.
El número esperado de comparaciones de quicksort aleatorizado sobre
$A$ es:

$$E[C] = 2(n+1)H_n - 4n \approx 1.386 \cdot n \log_2 n$$

**Prueba.** Sean $z_1 < z_2 < \ldots < z_n$ los elementos de $A$ en
orden creciente (los valores, no las posiciones). Definimos:

$$X_{ij} = \begin{cases} 1 & \text{si } z_i \text{ y } z_j \text{ se comparan} \\ 0 & \text{si no} \end{cases}$$

Como en T02:

$$E[C] = \sum_{i=1}^{n-1}\sum_{j=i+1}^{n} \Pr[X_{ij} = 1]$$

**Afirmación.** $\Pr[X_{ij} = 1] = 2/(j - i + 1)$, y esta
probabilidad **no depende de la disposición de los elementos en $A$**,
solo de sus valores relativos.

**Prueba de la afirmación.** Consideremos $Z_{ij} = \{z_i, z_{i+1}, \ldots, z_j\}$.
Independientemente de dónde estén estos elementos en el array:

- Mientras ningún elemento de $Z_{ij}$ sea elegido como pivote, todos
  permanecen en el mismo subarreglo (cualquier pivote externo a $Z_{ij}$
  los mantiene juntos, pues son todos mayores o todos menores que él).

- El primer elemento de $Z_{ij}$ elegido como pivote determina si
  $z_i$ y $z_j$ se comparan. Como el pivote se elige uniformemente
  dentro del subarreglo, y los elementos de $Z_{ij}$ están en el
  mismo subarreglo al momento de la elección, cada uno tiene
  probabilidad $1/(j-i+1)$ de ser el primero elegido.

- Solo si se elige $z_i$ o $z_j$ (2 opciones de $j-i+1$) ocurre la
  comparación.

Esta probabilidad depende solo de la estructura de rangos, no de las
posiciones iniciales en $A$. Por lo tanto:

$$\Pr[X_{ij} = 1] = \frac{2}{j-i+1}$$

para **cualquier** disposición de los elementos.

El resto de la derivación es idéntico a T02:

$$E[C] = 2(n+1)H_n - 4n$$

$$\blacksquare$$

---

## Demostración 2: concentración — el costo se desvía poco de la esperanza

**Teorema.** Para quicksort aleatorizado sobre cualquier entrada de
tamaño $n$:

$$\Pr[C > \alpha \cdot n \ln n] \leq n^{2 - \alpha} \quad \text{para } \alpha > 2$$

En particular, $\Pr[C > 4n\ln n] < 1/n^2$.

**Prueba (sketch usando desigualdad de Markov sobre $2^C$).**

Definimos la variable aleatoria $Y = 2^{C/(cn)}$ para una constante
$c$ adecuada. Se puede demostrar que $E[Y] = O(n)$. Por la
desigualdad de Markov:

$$\Pr[C > \alpha \cdot n \ln n] = \Pr[Y > 2^{\alpha \ln n / c}] \leq \frac{E[Y]}{2^{\alpha \ln n / c}}$$

Con la elección adecuada de $c$, esto da una cota polinomialmente
pequeña en $n$.

**Interpretación práctica.** Para $n = 10^6$:
- $E[C] \approx 1.39 \cdot 10^6 \cdot 20 \approx 2.8 \times 10^7$
- $\Pr[C > 4 \cdot 10^6 \cdot \ln(10^6)] < 10^{-12}$

La probabilidad de que quicksort aleatorizado sea significativamente
más lento que su esperanza es infinitesimal. Nunca se observa en la
práctica.

$$\blacksquare$$

---

## Demostración 3: no existe entrada adversarial

**Teorema.** Para todo array $A$ de $n$ elementos y todo $T > 0$:

$$E_{\text{rand}}[C(A)] = 2(n+1)H_n - 4n$$

La esperanza es **la misma** para toda entrada $A$.

**Prueba.** Ya demostrada en la Demostración 1: la probabilidad
$\Pr[X_{ij} = 1] = 2/(j-i+1)$ no depende de $A$, solo de $n$.
Por lo tanto, la esperanza $E[C]$ tampoco depende de $A$.

**Corolario.** Un adversario que conoce el algoritmo pero no las
elecciones aleatorias no puede construir una entrada que fuerce
más de $2(n+1)H_n - 4n$ comparaciones en esperanza.

$$\blacksquare$$

**Contraste con quicksort determinístico:**

| Propiedad | Determinístico (pivote fijo) | Aleatorizado |
|---|---|---|
| $\exists$ entrada con $T(n) = \Theta(n^2)$? | **Sí** (ej: ordenada) | No (en esperanza) |
| $E[C]$ depende de la entrada? | Sí | **No** |
| Garantía para peor entrada | $\Theta(n^2)$ | $O(n\log n)$ esperado |
| $\Pr[C = \Omega(n^2)]$ | 1 para entrada adversarial | $< 1/n!$ |

---

## Demostración 4: profundidad de recursión esperada $O(\log n)$

**Teorema.** La profundidad de recursión esperada de quicksort
aleatorizado es $O(\log n)$.

**Prueba.** En cada llamada recursiva, el pivote cae en la posición
$k$ uniformemente entre 1 y $m$ (tamaño del subarreglo). La
probabilidad de que el pivote caiga en el "centro" (entre la posición
$m/4$ y $3m/4$) es $1/2$.

Cuando el pivote cae en el centro, ambas particiones tienen tamaño
$\leq 3m/4$. Así, el tamaño del subproblema se reduce por un factor
$\leq 3/4$ con probabilidad $1/2$ en cada nivel.

El número de reducciones exitosas necesarias para llegar a tamaño 1 es:

$$(3/4)^d \cdot n \leq 1 \implies d \geq \log_{4/3} n = \frac{\log_2 n}{\log_2(4/3)} \approx 2.41 \log_2 n$$

Cada reducción exitosa ocurre con probabilidad $1/2$. La profundidad
total (incluyendo intentos fallidos) tiene esperanza:

$$E[\text{profundidad}] \approx 2 \cdot 2.41 \log_2 n \approx 4.82 \log_2 n = O(\log n)$$

Más precisamente, $E[\text{profundidad}] = O(\log n)$ y la
profundidad excede $c\log n$ con probabilidad $\leq 1/n$ para $c$
suficientemente grande.

$$\blacksquare$$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $E[C] = 2(n{+}1)H_n - 4n$ para toda entrada | Variables indicadoras | $\Pr[X_{ij}=1]$ no depende de la disposición |
| Concentración: $\Pr[C > 4n\ln n] < 1/n^2$ | Desigualdad de Markov | Colas exponencialmente pequeñas |
| No existe entrada adversarial | $E[C]$ independiente de $A$ | La aleatoriedad es del algoritmo |
| Profundidad $O(\log n)$ | Pivote "centrado" con prob $1/2$ | Reducción geométrica $3/4$ por éxito |
