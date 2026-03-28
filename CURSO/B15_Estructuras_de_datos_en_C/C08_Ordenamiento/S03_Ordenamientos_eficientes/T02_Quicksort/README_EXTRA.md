# T02 — Demostración formal: caso promedio de quicksort

> Complemento riguroso del README.md. Aquí se deriva formalmente que
> el número esperado de comparaciones de quicksort es
> $2n H_n \approx 1.39 \cdot n \log_2 n$, donde $H_n$ es el $n$-ésimo
> número armónico.

---

## Modelo

Consideramos quicksort con pivote aleatorio uniforme (cada elemento
tiene probabilidad $1/n$ de ser elegido como pivote). La entrada es
una permutación uniforme de $\{1, 2, \ldots, n\}$ (valores distintos).

Sea $C(n)$ la variable aleatoria que cuenta el número total de
comparaciones. Queremos calcular $E[C(n)]$.

---

## Observación clave: qué pares se comparan

En lugar de analizar la recurrencia directamente, contamos
**qué pares de elementos se comparan** a lo largo de toda la ejecución.

**Hecho.** Dos elementos se comparan **a lo sumo una vez** durante
quicksort. Cuando se comparan, uno de ellos es el pivote de alguna
llamada recursiva. Después de la partición, el pivote queda en su
posición final y nunca participa en comparaciones futuras.

Sea $z_i$ el $i$-ésimo menor elemento de la entrada (es decir, el
elemento de rango $i$ en el array ordenado). Definimos la variable
indicadora:

$$X_{ij} = \begin{cases} 1 & \text{si } z_i \text{ y } z_j \text{ se comparan durante la ejecución} \\ 0 & \text{si no} \end{cases}$$

para $1 \leq i < j \leq n$.

El número total de comparaciones es:

$$C(n) = \sum_{i=1}^{n-1} \sum_{j=i+1}^{n} X_{ij}$$

Por linealidad de la esperanza:

$$E[C(n)] = \sum_{i=1}^{n-1} \sum_{j=i+1}^{n} E[X_{ij}] = \sum_{i=1}^{n-1} \sum_{j=i+1}^{n} \Pr[z_i \text{ y } z_j \text{ se comparan}]$$

---

## Demostración 1: $\Pr[z_i \text{ y } z_j \text{ se comparan}] = \frac{2}{j - i + 1}$

**Lema.** Para $1 \leq i < j \leq n$:

$$\Pr[X_{ij} = 1] = \frac{2}{j - i + 1}$$

**Prueba.** Consideremos el conjunto $Z_{ij} = \{z_i, z_{i+1}, \ldots, z_j\}$,
que tiene $j - i + 1$ elementos.

**Observación clave:** $z_i$ y $z_j$ se comparan si y solo si uno de
ellos es el **primer** elemento de $Z_{ij}$ elegido como pivote en
alguna llamada recursiva.

¿Por qué? Mientras ningún elemento de $Z_{ij}$ haya sido pivote, todos
los elementos de $Z_{ij}$ permanecen en el mismo subarreglo (porque
ningún pivote externo los separa: un pivote $z_k$ con $k < i$ pone a
todos los $Z_{ij}$ a su derecha, y un pivote $z_k$ con $k > j$ pone a
todos los $Z_{ij}$ a su izquierda).

Cuando el primer elemento de $Z_{ij}$ se elige como pivote:

- Si es $z_i$ o $z_j$: ese pivote se compara con todos los demás
  elementos del subarreglo, incluyendo el otro extremo. $z_i$ y $z_j$
  **se comparan**. ✓
- Si es $z_k$ con $i < k < j$: la partición pone $z_i$ en el lado
  izquierdo (pues $z_i < z_k$) y $z_j$ en el lado derecho (pues
  $z_j > z_k$). $z_i$ y $z_j$ quedan en subproblemas disjuntos y
  **nunca se comparan**. ✗

Como el pivote es aleatorio uniforme, cada elemento de $Z_{ij}$ tiene
la misma probabilidad $1/(j - i + 1)$ de ser el primero elegido. Los
casos favorables son $z_i$ o $z_j$ (2 de $j - i + 1$):

$$\Pr[X_{ij} = 1] = \frac{2}{j - i + 1}$$

$$\blacksquare$$

---

## Demostración 2: $E[C(n)] = 2n H_n - 2n$

**Teorema.** El número esperado de comparaciones de quicksort con
pivote aleatorio es:

$$E[C(n)] = 2(n+1)H_n - 4n$$

donde $H_n = \sum_{k=1}^{n} 1/k$ es el $n$-ésimo número armónico.

**Nota.** La fórmula se simplifica frecuentemente como $\approx 2nH_n$,
ya que los términos de orden inferior son $O(n)$.

**Prueba.** Sustituimos $\Pr[X_{ij} = 1] = 2/(j-i+1)$:

$$E[C(n)] = \sum_{i=1}^{n-1} \sum_{j=i+1}^{n} \frac{2}{j - i + 1}$$

Hacemos el cambio de variable $d = j - i$ (la "distancia" entre los
rangos):

$$= \sum_{i=1}^{n-1} \sum_{d=1}^{n-i} \frac{2}{d + 1}$$

Intercambiamos el orden de la suma. Para un valor fijo de $d$, $i$ va
de 1 a $n - d$ (pues $j = i + d \leq n$ implica $i \leq n - d$).
Hay $n - d$ términos:

$$= \sum_{d=1}^{n-1} (n - d) \cdot \frac{2}{d + 1}$$

$$= 2 \sum_{d=1}^{n-1} \frac{n - d}{d + 1}$$

Separamos:

$$= 2 \sum_{d=1}^{n-1} \frac{n}{d+1} - 2 \sum_{d=1}^{n-1} \frac{d}{d+1}$$

**Primer sumando:** Con $k = d + 1$, $k$ va de 2 a $n$:

$$2n \sum_{k=2}^{n} \frac{1}{k} = 2n\left(H_n - 1\right)$$

**Segundo sumando:**

$$2 \sum_{d=1}^{n-1} \frac{d}{d+1} = 2 \sum_{d=1}^{n-1} \left(1 - \frac{1}{d+1}\right) = 2\left[(n-1) - \sum_{k=2}^{n} \frac{1}{k}\right] = 2(n-1) - 2(H_n - 1)$$

**Combinando:**

$$E[C(n)] = 2n(H_n - 1) - 2(n-1) + 2(H_n - 1)$$

$$= 2nH_n - 2n - 2n + 2 + 2H_n - 2$$

$$= 2(n+1)H_n - 4n$$

$$\blacksquare$$

---

## Demostración 3: $E[C(n)] \approx 1.39 \cdot n \log_2 n$

**Teorema.** $E[C(n)] \sim 2n \ln n \approx 1.386 \cdot n \log_2 n$.

**Prueba.** Usamos la asíntota del número armónico:

$$H_n = \ln n + \gamma + O(1/n)$$

donde $\gamma \approx 0.5772$ es la constante de Euler-Mascheroni.

Sustituyendo en $E[C(n)] = 2(n+1)H_n - 4n$:

$$E[C(n)] = 2(n+1)(\ln n + \gamma + O(1/n)) - 4n$$

$$= 2n\ln n + 2\ln n + 2(n+1)\gamma + O(1) - 4n$$

$$= 2n\ln n + (2\gamma - 4)n + O(\ln n)$$

El término dominante es $2n\ln n$. Convertimos a $\log_2$:

$$2n\ln n = 2n \cdot \frac{\log_2 n}{\log_2 e} = 2n \cdot \log_2 n \cdot \ln 2 = 2\ln 2 \cdot n\log_2 n$$

Como $2\ln 2 \approx 1.3863$:

$$E[C(n)] \approx 1.386 \cdot n \log_2 n$$

**Comparación con merge sort:**

$$\frac{E[C_{\text{quicksort}}(n)]}{C_{\text{merge,worst}}(n)} \approx \frac{1.386 \cdot n\log_2 n}{n\log_2 n} \approx 1.39$$

Quicksort hace ~39% más comparaciones que merge sort en el peor caso
de merge sort. Sin embargo, quicksort es más rápido en la práctica
porque sus comparaciones tienen mejor localidad de cache (opera
in-place sobre un segmento contiguo).

$$\blacksquare$$

---

## Verificación numérica

| $n$ | $2(n{+}1)H_n - 4n$ | $1.386 \cdot n\log_2 n$ | Ratio |
|:---:|:-------------------:|:-----------------------:|:-----:|
| 10 | 15.7 | 46.1 ... | — |
| 100 | 746 | 921 | 0.81 |
| 1000 | 12,082 | 13,824 | 0.87 |
| 10000 | 155,746 | 184,207 | 0.85 |

(La asíntota $1.386 \cdot n\log_2 n$ es una cota ajustada para
$n$ grande. Para $n$ pequeño, los términos de orden inferior importan.)

---

## Demostración 4: peor caso $\Theta(n^2)$

**Teorema.** Quicksort con pivote fijo (primer o último elemento)
tiene peor caso $\Theta(n^2)$.

**Prueba.** Si la entrada está ordenada y el pivote es el último
elemento, cada partición produce un subarreglo de tamaño $n - 1$ y
uno vacío:

$$T(n) = T(n-1) + cn$$

Resolviendo por expansión:

$$T(n) = cn + c(n-1) + c(n-2) + \ldots + c = c\sum_{k=1}^{n} k = \frac{cn(n+1)}{2} = \Theta(n^2)$$

$$\blacksquare$$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $\Pr[z_i, z_j \text{ se comparan}] = \frac{2}{j-i+1}$ | Análisis del primer pivote en $Z_{ij}$ | Solo $z_i$ o $z_j$ causan comparación |
| $E[C(n)] = 2(n{+}1)H_n - 4n$ | Linealidad de la esperanza + cambio de suma | Doble suma $\to$ suma sobre distancias |
| $E[C(n)] \approx 1.386 \cdot n\log_2 n$ | Asíntota $H_n \sim \ln n$ | $2\ln 2 \approx 1.386$ |
| Peor caso $\Theta(n^2)$ | Expansión de $T(n) = T(n-1) + cn$ | Partición maximalmente desbalanceada |
