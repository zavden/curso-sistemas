# T01 — Demostración formal: merge sort es $\Theta(n \log n)$

> Complemento riguroso del README.md. Aquí se resuelve formalmente la
> recurrencia $T(n) = 2T(n/2) + \Theta(n)$ por dos métodos: árbol de
> recursión y Teorema Maestro.

---

## La recurrencia

El costo de merge sort se describe por:

$$T(n) = \begin{cases} \Theta(1) & \text{si } n \leq 1 \\ 2\,T(n/2) + \Theta(n) & \text{si } n > 1 \end{cases}$$

- $2T(n/2)$: ordenar las dos mitades recursivamente.
- $\Theta(n)$: fusionar (merge) las dos mitades ordenadas.

Para simplificar, asumimos $n = 2^k$ (potencia de 2). El resultado
se extiende a todo $n$ con pisos y techos, sin cambiar la clase
asintótica.

Usamos $T(n) = 2T(n/2) + cn$ con $c > 0$ constante, y $T(1) = d$
con $d > 0$ constante.

---

## Demostración 1: árbol de recursión

**Teorema.** $T(n) = \Theta(n \log n)$.

**Prueba.** Expandimos la recurrencia en forma de árbol. Cada nodo
representa una llamada, con su costo de merge (sin contar las llamadas
recursivas):

```
Nivel 0:                    cn                          → cn
                          /      \
Nivel 1:             cn/2         cn/2                  → cn
                    /    \       /    \
Nivel 2:         cn/4   cn/4  cn/4   cn/4               → cn
                  ...    ...   ...    ...
Nivel j:       2ʲ nodos, cada uno con costo cn/2ʲ       → cn
                  ...    ...   ...    ...
Nivel k:       n nodos, cada uno con costo c = cn/n     → cn
```

**Número de niveles.** La recursión divide $n$ por 2 en cada nivel.
Llega al caso base ($n = 1$) en el nivel $k = \log_2 n$. Hay
$\log_2 n + 1$ niveles en total (nivel 0 a nivel $\log_2 n$).

**Trabajo por nivel.** En el nivel $j$ hay $2^j$ subproblemas, cada
uno de tamaño $n/2^j$, con costo de merge $c \cdot n/2^j$. El trabajo
total en el nivel $j$ es:

$$2^j \cdot \frac{cn}{2^j} = cn$$

Cada nivel contribuye exactamente $cn$ de trabajo, independientemente
del nivel.

**Trabajo total.** Sumando sobre todos los niveles:

$$T(n) = \sum_{j=0}^{\log_2 n} cn = cn \cdot (\log_2 n + 1) = cn\log_2 n + cn$$

Por lo tanto:

$$T(n) = cn\log_2 n + cn = \Theta(n \log n)$$

$$\blacksquare$$

---

## Demostración 2: Teorema Maestro

### Enunciado del Teorema Maestro

**Teorema Maestro.** Sea $T(n) = aT(n/b) + f(n)$ con $a \geq 1$,
$b > 1$, y $f(n)$ asintóticamente positiva. Sea
$c_{\text{crit}} = \log_b a$. Entonces:

**Caso 1.** Si $f(n) = O(n^{c_{\text{crit}} - \varepsilon})$ para algún
$\varepsilon > 0$, entonces $T(n) = \Theta(n^{c_{\text{crit}}})$.

**Caso 2.** Si $f(n) = \Theta(n^{c_{\text{crit}}} \log^k n)$ para
algún $k \geq 0$, entonces $T(n) = \Theta(n^{c_{\text{crit}}} \log^{k+1} n)$.

**Caso 3.** Si $f(n) = \Omega(n^{c_{\text{crit}} + \varepsilon})$ para
algún $\varepsilon > 0$ y $af(n/b) \leq cf(n)$ para algún $c < 1$,
entonces $T(n) = \Theta(f(n))$.

### Aplicación a merge sort

Para $T(n) = 2T(n/2) + cn$:

- $a = 2$ (dos subproblemas).
- $b = 2$ (cada uno de tamaño $n/2$).
- $f(n) = cn = \Theta(n)$.

Calculamos $c_{\text{crit}}$:

$$c_{\text{crit}} = \log_b a = \log_2 2 = 1$$

Comparamos $f(n)$ con $n^{c_{\text{crit}}} = n^1 = n$:

$$f(n) = cn = \Theta(n) = \Theta(n^{c_{\text{crit}}} \cdot \log^0 n)$$

Esto es el **Caso 2** con $k = 0$. Por el Teorema Maestro:

$$T(n) = \Theta(n^{c_{\text{crit}}} \log^{k+1} n) = \Theta(n^1 \cdot \log^1 n) = \Theta(n \log n)$$

$$\blacksquare$$

---

## Demostración 3: verificación por sustitución (inducción)

Para confirmar el resultado, probamos por inducción fuerte que
$T(n) = cn\log_2 n + dn$ (forma exacta para $n = 2^k$).

**Teorema.** Si $T(1) = d$ y $T(n) = 2T(n/2) + cn$ para $n \geq 2$
(con $n$ potencia de 2), entonces $T(n) = cn\log_2 n + dn$.

**Prueba (por inducción sobre $k$ donde $n = 2^k$).**

**Base ($k = 0$, $n = 1$):**

$$T(1) = d = c \cdot 1 \cdot \log_2 1 + d \cdot 1 = 0 + d = d \quad \checkmark$$

**Paso inductivo.** Sea $n = 2^k$ con $k \geq 1$. Supongamos que
$T(n/2) = c(n/2)\log_2(n/2) + d(n/2)$ (hipótesis inductiva). Entonces:

$$T(n) = 2T(n/2) + cn$$

$$= 2\left[c\frac{n}{2}\log_2\frac{n}{2} + d\frac{n}{2}\right] + cn$$

$$= cn\log_2\frac{n}{2} + dn + cn$$

$$= cn(\log_2 n - 1) + dn + cn$$

$$= cn\log_2 n - cn + dn + cn$$

$$= cn\log_2 n + dn$$

que es la fórmula para $n$. ✓

$$\therefore T(n) = cn\log_2 n + dn = \Theta(n\log n). \qquad \blacksquare$$

---

## Extensión a $n$ general (no potencia de 2)

Cuando $n$ no es potencia de 2, la recurrencia exacta es:

$$T(n) = T\!\left(\left\lfloor \frac{n}{2} \right\rfloor\right) + T\!\left(\left\lceil \frac{n}{2} \right\rceil\right) + cn$$

**Teorema.** La clase asintótica no cambia: $T(n) = \Theta(n \log n)$.

**Prueba (sketch).** Sea $n' = 2^{\lceil \log_2 n \rceil}$ la potencia
de 2 inmediatamente $\geq n$. Entonces $n \leq n' < 2n$, y:

$$T(n) \leq T(n') = cn'\log_2 n' + dn' \leq c(2n)\log_2(2n) + d(2n) = O(n\log n)$$

La cota inferior se obtiene análogamente con $n'' = 2^{\lfloor \log_2 n \rfloor} \geq n/2$. Combinando: $T(n) = \Theta(n \log n)$. $\square$

---

## Demostración 4: merge sort es $\Theta(n \log n)$ en todos los casos

**Teorema.** Merge sort tiene complejidad $\Theta(n \log n)$ en el
mejor, promedio y peor caso.

**Prueba.** La estructura recursiva es **independiente de la entrada**:
merge sort siempre divide a la mitad y siempre fusiona. Lo único que
varía es el número de comparaciones dentro de cada merge.

**Peor caso.** El merge de dos subarrays de tamaños $p$ y $q$ ejecuta
a lo sumo $p + q - 1$ comparaciones (cuando los elementos se
intercalan perfectamente). En cada nivel, las comparaciones totales son
a lo sumo $n - (\text{número de subarrays}) \leq n$. Con $\log_2 n$
niveles: $T_{\text{worst}} \leq n \log_2 n = O(n \log n)$.

**Mejor caso.** Cada merge ejecuta al menos $\min(p, q)$
comparaciones (una mitad se agota antes de tocar la otra). Para mitades
iguales ($p = q = n/2^j$), esto es $n/2^j$. En cada nivel, las
comparaciones totales son al menos $n/2$. Con $\log_2 n$ niveles:
$T_{\text{best}} \geq (n/2)\log_2 n = \Omega(n \log n)$.

Combinando: merge sort es $\Theta(n \log n)$ en todos los casos.

$$\blacksquare$$

---

## Resumen de resultados

| Resultado | Método | Clave |
|-----------|--------|-------|
| $T(n) = cn\log_2 n + dn$ | Árbol de recursión | $cn$ trabajo por nivel $\times$ $\log_2 n + 1$ niveles |
| $T(n) = \Theta(n \log n)$ | Teorema Maestro (caso 2) | $a = b = 2$, $f(n) = \Theta(n^{\log_2 2})$ |
| Fórmula exacta verificada | Inducción (sustitución) | $cn(\log_2 n - 1) + dn + cn = cn\log_2 n + dn$ |
| $n$ general no cambia la clase | Redondeo a potencia de 2 | $n \leq 2^{\lceil \log_2 n \rceil} < 2n$ |
| $\Theta(n \log n)$ en todos los casos | Cotas de merge | Mejor: $\geq n/2$ por nivel; peor: $\leq n$ por nivel |
