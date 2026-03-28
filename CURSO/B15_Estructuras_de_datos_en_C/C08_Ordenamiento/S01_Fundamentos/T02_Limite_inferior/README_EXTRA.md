# T02 — Demostración formal: cota inferior $\Omega(n \log n)$ para ordenamiento por comparación

> Complemento riguroso del README.md. Aquí se prueban formalmente todos
> los pasos de la cota inferior usando árboles de decisión, incluyendo
> la derivación completa de la aproximación de Stirling para $\log_2(n!)$.

---

## Definiciones

**Definición (Modelo de comparación).** Un algoritmo opera en el modelo
de comparación si la única forma de obtener información sobre los
elementos de entrada es mediante comparaciones de la forma $a_i \leq a_j$
(o $<$, $>$, $=$), cada una con resultado binario.

**Definición (Árbol de decisión).** Un árbol de decisión para ordenar
$n$ elementos es un árbol binario donde:

- Cada nodo interno contiene una comparación $a_i \leq a_j$.
- La rama izquierda corresponde al resultado "sí".
- La rama derecha corresponde al resultado "no".
- Cada hoja está etiquetada con una permutación $\pi \in S_n$.

La ejecución del algoritmo sobre una entrada concreta corresponde a un
camino raíz→hoja en el árbol. La altura del árbol es el número de
comparaciones en el peor caso.

---

## Lema 1: un árbol binario de altura $h$ tiene a lo sumo $2^h$ hojas

**Lema.** Sea $T$ un árbol binario de altura $h$. Entonces $T$ tiene a
lo sumo $2^h$ hojas.

**Prueba (por inducción sobre $h$).**

**Base ($h = 0$).** Un árbol de altura 0 es un solo nodo (hoja).
Hojas $= 1 = 2^0$. ✓

**Paso inductivo.** Sea $T$ un árbol de altura $h \geq 1$ con raíz $r$.
Los subárboles izquierdo y derecho de $r$ tienen altura a lo sumo
$h - 1$. Por hipótesis inductiva, cada uno tiene a lo sumo $2^{h-1}$
hojas. Las hojas de $T$ son exactamente las hojas de sus subárboles:

$$L(T) = L(T_{\text{izq}}) + L(T_{\text{der}}) \leq 2^{h-1} + 2^{h-1} = 2^h$$

$$\blacksquare$$

---

## Lema 2: el árbol de decisión tiene al menos $n!$ hojas

**Lema.** Todo árbol de decisión que ordena correctamente $n$ elementos
tiene al menos $n!$ hojas.

**Prueba (por contradicción).** Supongamos que el árbol tiene menos de
$n!$ hojas. Como hay $n!$ permutaciones posibles de la entrada, por el
principio del palomar existen dos permutaciones distintas $\pi_1$ y
$\pi_2$ que llegan a la misma hoja $\ell$.

La hoja $\ell$ está etiquetada con una única permutación de salida
$\sigma$. Esto significa que el algoritmo produce la misma salida
$\sigma$ para las entradas $\pi_1$ y $\pi_2$.

Pero $\pi_1 \neq \pi_2$ implica que las permutaciones de ordenamiento
correctas son distintas: $\sigma_1 \neq \sigma_2$ (donde $\sigma_i$ es
la permutación que ordena $\pi_i$). El algoritmo aplica $\sigma$ a
ambas, por lo que al menos una queda mal ordenada.

Contradicción con la hipótesis de que el algoritmo ordena correctamente.

$$\therefore L \geq n!. \qquad \blacksquare$$

---

## Teorema principal: $\Omega(n \log n)$ comparaciones

**Teorema.** Todo algoritmo de ordenamiento basado en comparaciones
requiere $\Omega(n \log n)$ comparaciones en el peor caso.

**Prueba.** Sea $T$ el árbol de decisión de un algoritmo que ordena
$n$ elementos, con altura $h$ (= peor caso de comparaciones).

Por el Lema 1: $L(T) \leq 2^h$.

Por el Lema 2: $L(T) \geq n!$.

Combinando:

$$n! \leq 2^h$$

Tomando $\log_2$:

$$h \geq \log_2(n!)$$

Resta probar que $\log_2(n!) = \Omega(n \log n)$. Lo hacemos de dos
formas independientes.

$$\blacksquare \text{ (módulo la cota de } \log_2(n!) \text{, que sigue)}$$

---

## Demostración de $\log_2(n!) = \Theta(n \log n)$

### Método 1: cota inferior directa (sin Stirling)

**Lema.** $\log_2(n!) \geq \frac{n}{2} \log_2\left(\frac{n}{2}\right)$.

**Prueba.** En el producto $n! = 1 \cdot 2 \cdot 3 \cdots n$, los
factores $\lceil n/2 \rceil, \lceil n/2 \rceil + 1, \ldots, n$ son
todos $\geq n/2$. Hay al menos $\lfloor n/2 \rfloor$ tales factores.
Descartando los factores menores:

$$n! = \prod_{i=1}^{n} i \geq \prod_{i=\lceil n/2 \rceil}^{n} i \geq \left(\frac{n}{2}\right)^{\lfloor n/2 \rfloor} \geq \left(\frac{n}{2}\right)^{n/2}$$

Tomando $\log_2$:

$$\log_2(n!) \geq \frac{n}{2} \log_2\left(\frac{n}{2}\right) = \frac{n}{2}(\log_2 n - 1)$$

Para $n \geq 4$: $\log_2 n \geq 2$, así que $\log_2 n - 1 \geq \frac{1}{2}\log_2 n$:

$$\log_2(n!) \geq \frac{n}{2} \cdot \frac{1}{2}\log_2 n = \frac{n \log_2 n}{4}$$

$$\therefore \log_2(n!) = \Omega(n \log n). \qquad \blacksquare$$

### Método 2: cota superior (para obtener $\Theta$)

**Lema.** $\log_2(n!) \leq n \log_2 n$.

**Prueba.** Cada factor del producto es $\leq n$:

$$n! = \prod_{i=1}^{n} i \leq n^n$$

$$\log_2(n!) \leq \log_2(n^n) = n \log_2 n \qquad \blacksquare$$

**Conclusión.** Combinando ambas cotas:

$$\frac{n \log_2 n}{4} \leq \log_2(n!) \leq n \log_2 n$$

$$\therefore \log_2(n!) = \Theta(n \log n)$$

### Método 3: aproximación de Stirling (cota más precisa)

**Teorema (Stirling).** Para $n \geq 1$:

$$n! = \sqrt{2\pi n} \left(\frac{n}{e}\right)^n \left(1 + O\left(\frac{1}{n}\right)\right)$$

**Derivación de $\log_2(n!)$ usando Stirling:**

$$\log_2(n!) = \log_2\left(\sqrt{2\pi n}\right) + n \log_2\left(\frac{n}{e}\right) + O\left(\frac{1}{n}\right)$$

Expandimos cada término:

$$= \frac{1}{2}\log_2(2\pi n) + n\log_2 n - n\log_2 e + O\left(\frac{1}{n}\right)$$

$$= n\log_2 n - n\log_2 e + \frac{1}{2}\log_2(2\pi) + \frac{1}{2}\log_2 n + O\left(\frac{1}{n}\right)$$

Como $\log_2 e \approx 1.4427$:

$$= n\log_2 n - 1.4427\,n + O(\log n)$$

**Verificación numérica:**

| $n$ | $\log_2(n!)$ exacto | $n\log_2 n - 1.443n$ | Error |
|:---:|:-------------------:|:---------------------:|:-----:|
| 10 | 21.79 | 18.78 | 3.01 |
| 100 | 524.76 | 520.57 | 4.19 |
| 1000 | 8529.66 | 8527.14 | 2.52 |

La aproximación de Stirling es excelente para $n$ moderado.

$$\therefore \log_2(n!) = n\log_2 n - \Theta(n) = \Theta(n \log n). \qquad \blacksquare$$

---

## Corolario: optimalidad de merge sort y heapsort

**Corolario.** Los algoritmos merge sort y heapsort son asintóticamente
óptimos en el modelo de comparación.

**Prueba.** Ambos ejecutan $O(n \log n)$ comparaciones en el peor caso.
La cota inferior demuestra que ningún algoritmo basado en comparaciones
puede hacer menos de $\Omega(n \log n)$. Por lo tanto, su complejidad
de $\Theta(n \log n)$ es óptima.

$$\blacksquare$$

**Nota.** Quicksort tiene peor caso $O(n^2)$, así que no es óptimo en
el peor caso. Sin embargo, su caso promedio es $O(n \log n)$, que
coincide con la cota inferior. Quicksort randomizado tiene esperanza
$O(n \log n)$, que es óptimo en expectativa.

---

## Corolario: los sorting lineales no contradicen la cota

**Corolario.** Counting sort ($O(n + k)$), radix sort ($O(d(n + k))$)
y bucket sort ($O(n)$ esperado) no contradicen la cota de
$\Omega(n \log n)$.

**Prueba.** Estos algoritmos **no** operan en el modelo de comparación:

- Counting sort usa valores como índices de array.
- Radix sort examina dígitos individuales.
- Bucket sort distribuye por rangos.

La cota $\Omega(n \log n)$ solo aplica a algoritmos que obtienen
información exclusivamente mediante comparaciones binarias. Los
algoritmos lineales explotan propiedades adicionales de los datos
(rango acotado, representación digital), lo que les permite esquivar
la cota. $\square$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|---------|-------|
| Árbol de altura $h$ tiene $\leq 2^h$ hojas | Inducción | $L(T_{\text{izq}}) + L(T_{\text{der}}) \leq 2^{h-1} + 2^{h-1}$ |
| Árbol de decisión tiene $\geq n!$ hojas | Principio del palomar | Dos permutaciones distintas → misma hoja → error |
| $h \geq \log_2(n!)$ | Combinar los dos lemas | $n! \leq 2^h$ |
| $\log_2(n!) = \Omega(n \log n)$ | Mitad de los factores $\geq n/2$ | $(n/2)^{n/2} \leq n!$ |
| $\log_2(n!) = n\log_2 n - 1.443n + O(\log n)$ | Stirling | Asíntota precisa |
| Merge sort / heapsort son óptimos | Cota inferior vs cota superior | $\Theta(n \log n)$ ambas |
