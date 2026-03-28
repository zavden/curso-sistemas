# T03 — Demostración formal: longitud esperada de cadena = α

> Complemento riguroso del README.md. Aquí se demuestra formalmente que,
> bajo hashing uniforme simple, la longitud esperada de cada cadena en
> una tabla hash con encadenamiento separado es exactamente
> $\alpha = n/m$, y se derivan propiedades adicionales de la
> distribución.

---

## Modelo

**Tabla hash.** Un array $T[0..m-1]$ de $m$ buckets. Cada bucket
$T[j]$ contiene una lista enlazada (cadena).

**Elementos.** Se insertan $n$ claves $k_1, k_2, \ldots, k_n$ en la
tabla.

**Hipótesis de hashing uniforme simple (SUHA).** Para cada clave $k$,
$h(k)$ se distribuye uniformemente en $\{0, 1, \ldots, m-1\}$, e
independientemente de las demás claves:

$$\Pr[h(k) = j] = \frac{1}{m} \quad \forall\, j \in \{0, \ldots, m-1\}$$

**Factor de carga.** $\alpha = n/m$.

**Variable de interés.** Sea $L_j$ la longitud de la cadena en el
bucket $j$, es decir, el número de claves mapeadas a $T[j]$.

---

## Demostración 1: $E[L_j] = \alpha$ por linealidad de la esperanza

**Teorema.** Bajo SUHA, para todo bucket $j \in \{0, \ldots, m-1\}$:

$$E[L_j] = \frac{n}{m} = \alpha$$

**Prueba.** Definimos variables indicadoras. Para cada clave $k_i$
($1 \leq i \leq n$) y cada bucket $j$:

$$X_{ij} = \begin{cases} 1 & \text{si } h(k_i) = j \\ 0 & \text{si no} \end{cases}$$

La longitud de la cadena en el bucket $j$ es:

$$L_j = \sum_{i=1}^{n} X_{ij}$$

Por linealidad de la esperanza (que **no** requiere independencia):

$$E[L_j] = \sum_{i=1}^{n} E[X_{ij}] = \sum_{i=1}^{n} \Pr[h(k_i) = j]$$

Por SUHA, $\Pr[h(k_i) = j] = 1/m$ para todo $i$:

$$E[L_j] = \sum_{i=1}^{n} \frac{1}{m} = \frac{n}{m} = \alpha$$

**Observación.** El resultado vale para **todo** bucket $j$, no solo
"en promedio sobre los buckets". Cada bucket individual tiene longitud
esperada exactamente $\alpha$.

$$\blacksquare$$

---

## Demostración 2: $L_j \sim \text{Binomial}(n, 1/m)$

**Teorema.** Bajo SUHA, $L_j$ sigue una distribución binomial con
parámetros $n$ y $p = 1/m$.

**Prueba.** Las variables $X_{1j}, X_{2j}, \ldots, X_{nj}$ son:

1. **Independientes**: por SUHA, $h(k_i)$ es independiente de
   $h(k_\ell)$ para $i \neq \ell$.

2. **Bernoulli con el mismo parámetro**: $X_{ij} \sim \text{Bernoulli}(1/m)$
   para todo $i$.

Por definición, la suma de $n$ variables Bernoulli independientes con
el mismo parámetro $p$ es $\text{Binomial}(n, p)$:

$$L_j \sim \text{Binomial}(n, 1/m)$$

$$\blacksquare$$

---

## Demostración 3: varianza de $L_j$

**Teorema.** $\text{Var}[L_j] = \alpha(1 - 1/m)$.

**Prueba.** Como $L_j \sim \text{Binomial}(n, 1/m)$, y la varianza de
una binomial $\text{Binomial}(n, p)$ es $np(1-p)$:

$$\text{Var}[L_j] = n \cdot \frac{1}{m} \cdot \left(1 - \frac{1}{m}\right) = \frac{n}{m} \cdot \frac{m-1}{m} = \alpha \cdot \frac{m-1}{m}$$

**Corolarios.**

1. **Desviación estándar**: $\sigma_{L_j} = \sqrt{\alpha(1 - 1/m)} \approx \sqrt{\alpha}$ para $m$ grande.

2. **Para $m$ grande** ($m \gg 1$): $\text{Var}[L_j] \approx \alpha$,
   y $L_j$ se aproxima a una distribución $\text{Poisson}(\alpha)$.

3. **Concentración**: por la desigualdad de Chebyshev,

   $$\Pr[|L_j - \alpha| \geq t\sqrt{\alpha}] \leq \frac{1}{t^2}$$

   Con $\alpha = 1$ y $t = 3$: $\Pr[L_j \geq 4] \leq 1/9 \approx 11\%$.

$$\blacksquare$$

---

## Demostración 4: la longitud total es exactamente $n$

**Teorema.** $\sum_{j=0}^{m-1} L_j = n$ con probabilidad 1.

**Prueba.** Cada clave $k_i$ es mapeada a exactamente un bucket
(la función hash es total y determinista para cada clave fija). Por
lo tanto, cada $k_i$ contribuye exactamente 1 a exactamente un $L_j$:

$$\sum_{j=0}^{m-1} L_j = \sum_{j=0}^{m-1} \sum_{i=1}^{n} X_{ij} = \sum_{i=1}^{n} \underbrace{\sum_{j=0}^{m-1} X_{ij}}_{= 1} = n$$

**Verificación de consistencia con Demostración 1:**

$$\sum_{j=0}^{m-1} E[L_j] = \sum_{j=0}^{m-1} \alpha = m \cdot \frac{n}{m} = n \quad \checkmark$$

$$\blacksquare$$

---

## Demostración 5: longitud máxima esperada

**Teorema.** Bajo SUHA, con $n = m$ ($\alpha = 1$):

$$E[\max_j L_j] = \Theta\!\left(\frac{\log n}{\log \log n}\right)$$

**Prueba (sketch).** Usamos la cota de la distribución binomial.
Para un bucket fijo $j$:

$$\Pr[L_j \geq k] = \binom{n}{k} \left(\frac{1}{m}\right)^k \left(1 - \frac{1}{m}\right)^{n-k} \leq \binom{n}{k} \frac{1}{m^k}$$

Con $n = m$ y usando $\binom{n}{k} \leq (ne/k)^k$:

$$\Pr[L_j \geq k] \leq \left(\frac{ne}{km}\right)^k = \left(\frac{e}{k}\right)^k$$

*Cota superior.* Sea $k^* = c\log n / \log\log n$ para $c$
suficientemente grande. Entonces $(e/k^*)^{k^*}$ decrece más rápido
que $1/n^2$. Por union bound sobre los $m = n$ buckets:

$$\Pr[\max_j L_j \geq k^*] \leq n \cdot \frac{1}{n^2} = \frac{1}{n} \to 0$$

*Cota inferior.* Se muestra (por segundo momento o inclusión-exclusión)
que $\Pr[\max_j L_j \geq c'\log n / \log\log n] \to 1$ para $c'$
suficientemente pequeño.

**Implicación práctica.** Aunque $E[L_j] = 1$ (con $\alpha = 1$), el
bucket **más largo** tiene $\Theta(\log n / \log\log n)$ elementos. El
peor caso de una búsqueda individual no es $O(1)$, sino
$O(\log n / \log\log n)$.

$$\blacksquare$$

---

## Verificación numérica

Para $\alpha = 1$ ($n = m$), la distribución $\text{Poisson}(1)$ predice:

| Longitud $k$ | $\Pr[L_j = k]$ | $n = 1000$: buckets esperados con longitud $k$ |
|:---:|:---:|:---:|
| 0 | 0.368 | 368 |
| 1 | 0.368 | 368 |
| 2 | 0.184 | 184 |
| 3 | 0.061 | 61 |
| 4 | 0.015 | 15 |
| 5 | 0.003 | 3 |
| $\geq 6$ | 0.001 | ~1 |

Verificación: $\sum k \cdot \Pr[L_j = k] = 1 = \alpha$. ✓

Media de $L_j$: 1. Varianza de $L_j$: 1. Ambas coinciden con
$\alpha = 1$ y $\alpha(1 - 1/m) \approx 1$ para $m$ grande. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $E[L_j] = \alpha$ | Linealidad de la esperanza | $X_{ij} \sim \text{Bernoulli}(1/m)$, sumar $n$ indicadoras |
| $L_j \sim \text{Binomial}(n, 1/m)$ | Independencia por SUHA | Suma de Bernoulli i.i.d. |
| $\text{Var}[L_j] = \alpha(1 - 1/m)$ | Varianza de la binomial | $\approx \alpha$ para $m$ grande |
| $\sum L_j = n$ | Doble conteo | Cada clave en exactamente un bucket |
| $\max L_j = \Theta(\log n / \log\log n)$ | Union bound + segundo momento | El peor bucket es superconstante |
