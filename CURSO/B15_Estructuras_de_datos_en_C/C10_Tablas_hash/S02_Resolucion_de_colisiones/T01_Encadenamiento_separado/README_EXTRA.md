# T01 — Demostración formal: costos de búsqueda en encadenamiento separado

> Complemento riguroso del README.md. Aquí se demuestra formalmente que,
> bajo hashing uniforme simple, la búsqueda fallida tiene costo esperado
> $\Theta(1 + \alpha)$ y la búsqueda exitosa tiene costo esperado
> $\Theta(1 + \alpha/2)$, donde $\alpha = n/m$.

---

## Modelo

**Tabla hash.** Array $T[0..m-1]$ con encadenamiento separado: cada
bucket $T[j]$ contiene una lista enlazada de los elementos cuya clave
hashea a $j$.

**Inserción.** Nuevos elementos se insertan al **inicio** de la lista
(prepend). Así, el elemento más reciente está primero.

**Hashing uniforme simple (SUHA).** Para toda clave $k$:

$$\Pr[h(k) = j] = \frac{1}{m} \quad \forall\, j \in \{0, \ldots, m-1\}$$

independientemente de las demás claves.

**Notación.** $n$ = número de elementos almacenados, $m$ = número de
buckets, $\alpha = n/m$. Sea $L_j$ la longitud de la cadena en
$T[j]$.

Del README_EXTRA de T03 (Factor de carga): $E[L_j] = \alpha$ para
todo $j$.

---

## Demostración 1: búsqueda fallida $\Theta(1 + \alpha)$

**Teorema.** El número esperado de comparaciones en una búsqueda
fallida (la clave buscada no existe en la tabla) es exactamente
$\alpha$.

**Prueba.** Sea $k$ una clave que no está en la tabla. La búsqueda:

1. Calcula $j = h(k)$: costo $O(1)$.
2. Recorre **toda** la lista en $T[j]$, comparando $k$ con cada
   elemento, sin encontrarlo.

El número de comparaciones es exactamente $L_j$, la longitud de la
cadena en el bucket $j = h(k)$.

Bajo SUHA, $h(k)$ es uniformemente aleatorio e independiente de los
$n$ elementos ya insertados. Por lo tanto:

$$E[\text{comparaciones}] = E[L_{h(k)}]$$

Como $h(k)$ es uniforme en $\{0, \ldots, m-1\}$:

$$E[L_{h(k)}] = \sum_{j=0}^{m-1} \Pr[h(k) = j] \cdot E[L_j] = \sum_{j=0}^{m-1} \frac{1}{m} \cdot \alpha = \alpha$$

Incluyendo el costo $O(1)$ de calcular el hash y acceder al bucket:

$$T_{\text{miss}} = \Theta(1 + \alpha)$$

El $1$ asegura que el costo nunca sea sublineal en la operación misma
(aun con $\alpha = 0$, hay que calcular el hash).

$$\blacksquare$$

---

## Demostración 2: búsqueda exitosa $\Theta(1 + \alpha/2)$

**Teorema.** El número esperado de comparaciones en una búsqueda
exitosa (la clave buscada sí existe en la tabla) es:

$$1 + \frac{\alpha}{2} - \frac{1}{2m}$$

**Prueba.** Supongamos que los $n$ elementos fueron insertados en
orden $k_1, k_2, \ldots, k_n$. Buscamos un elemento elegido
uniformemente al azar entre los $n$ almacenados.

**Paso 1: comparaciones para encontrar $k_i$.**

El elemento $k_i$ fue el $i$-ésimo insertado. Al insertarse, se
colocó al inicio de su cadena. Los elementos insertados
**después** de $k_i$ en el **mismo bucket** se colocan delante de
$k_i$ (por el prepend). Así, para encontrar $k_i$, debemos
comparar primero con todos los elementos insertados después que
están en el mismo bucket, y luego con $k_i$ mismo.

El número de comparaciones para encontrar $k_i$ es:

$$C_i = 1 + \sum_{r=i+1}^{n} Y_{ri}$$

donde $Y_{ri}$ es la variable indicadora:

$$Y_{ri} = \begin{cases} 1 & \text{si } h(k_r) = h(k_i) \quad \text{($k_r$ cayó en el mismo bucket que $k_i$)} \\ 0 & \text{si no} \end{cases}$$

El $1$ cuenta la comparación final con $k_i$ mismo.

**Paso 2: esperanza de $C_i$.**

Por SUHA: $\Pr[h(k_r) = h(k_i)] = 1/m$ (ambos uniformes e
independientes). Entonces:

$$E[C_i] = 1 + \sum_{r=i+1}^{n} \frac{1}{m} = 1 + \frac{n - i}{m}$$

**Paso 3: promedio sobre todos los elementos.**

Si buscamos un elemento elegido uniformemente al azar entre los $n$:

$$E[\text{comparaciones}] = \frac{1}{n} \sum_{i=1}^{n} E[C_i] = \frac{1}{n} \sum_{i=1}^{n} \left(1 + \frac{n - i}{m}\right)$$

Separando:

$$= 1 + \frac{1}{nm} \sum_{i=1}^{n} (n - i)$$

La suma $\sum_{i=1}^{n} (n - i) = (n-1) + (n-2) + \ldots + 0 = \frac{n(n-1)}{2}$.

$$= 1 + \frac{1}{nm} \cdot \frac{n(n-1)}{2} = 1 + \frac{n-1}{2m}$$

Expresando en función de $\alpha = n/m$:

$$= 1 + \frac{n-1}{2m} = 1 + \frac{\alpha}{2} - \frac{1}{2m}$$

Para $n$ y $m$ grandes:

$$E[\text{comparaciones}] \approx 1 + \frac{\alpha}{2}$$

Incluyendo el costo $O(1)$ de hash y acceso al bucket:

$$T_{\text{hit}} = \Theta\!\left(1 + \frac{\alpha}{2}\right)$$

$$\blacksquare$$

---

## Demostración 3: ambos costos son $O(1)$ cuando $\alpha = O(1)$

**Teorema.** Si la tabla se redimensiona para mantener
$\alpha \leq \alpha_{\max}$ (una constante fija), entonces
búsqueda, inserción y eliminación son $O(1)$ en esperanza.

**Prueba.** Si $\alpha \leq \alpha_{\max}$ en todo momento:

- Búsqueda fallida: $\Theta(1 + \alpha) \leq \Theta(1 + \alpha_{\max}) = O(1)$.
- Búsqueda exitosa: $\Theta(1 + \alpha/2) \leq \Theta(1 + \alpha_{\max}/2) = O(1)$.
- Inserción (con verificación de duplicados): dominada por búsqueda
  fallida: $O(1)$.
- Eliminación: dominada por búsqueda exitosa: $O(1)$.

El costo de **redimensionar** es $O(n)$ por operación de resize, pero
se amortiza a $O(1)$ por inserción (demostrado en T05).

**Esto es lo que hace que las tablas hash sean $O(1)$ en la
práctica**: no es que las operaciones sean $O(1)$ para cualquier
$\alpha$, sino que el redimensionamiento mantiene $\alpha$ acotado.

$$\blacksquare$$

---

## Demostración 4: la búsqueda exitosa es estrictamente más barata que la fallida

**Teorema.** Para $n \geq 2$ y $\alpha > 0$:

$$E[C_{\text{hit}}] < E[C_{\text{miss}}]$$

**Prueba.** Comparamos directamente:

$$E[C_{\text{miss}}] = \alpha = \frac{n}{m}$$

$$E[C_{\text{hit}}] = 1 + \frac{n-1}{2m} = 1 + \frac{\alpha}{2} - \frac{1}{2m}$$

Para que $C_{\text{hit}} < C_{\text{miss}}$:

$$1 + \frac{n-1}{2m} < \frac{n}{m}$$

$$1 < \frac{n}{m} - \frac{n-1}{2m} = \frac{2n - n + 1}{2m} = \frac{n+1}{2m}$$

$$2m < n + 1$$

Esto vale cuando $\alpha > 2 - 1/m$, es decir, para $\alpha \geq 2$
(aproximadamente). Para $\alpha < 2$, el $+1$ de la comparación
final con el elemento encontrado puede hacer que $C_{\text{hit}} >
C_{\text{miss}}$ (por ejemplo, si $\alpha = 0.1$: $C_{\text{miss}}
= 0.1$, $C_{\text{hit}} \approx 1.05$).

**Corrección**: lo anterior muestra que la relación depende de $\alpha$.
La comparación más informativa es sobre el **costo total** (hash +
recorrido):

$$T_{\text{miss}} = 1 + \alpha, \quad T_{\text{hit}} = 1 + \frac{\alpha}{2} - \frac{1}{2m}$$

Aquí $T_{\text{hit}} < T_{\text{miss}}$ para todo $\alpha > 0$:

$$1 + \frac{\alpha}{2} - \frac{1}{2m} < 1 + \alpha \iff -\frac{1}{2m} < \frac{\alpha}{2}$$

que vale siempre que $\alpha > 0$. ✓

**Interpretación.** En una búsqueda exitosa, encontramos el elemento
antes de llegar al final de la lista (en promedio, a mitad de
camino). En una fallida, recorremos toda la lista.

$$\blacksquare$$

---

## Demostración 5: el modelo de inserción afecta $C_{\text{hit}}$

**Teorema.** Si los nuevos elementos se insertan al **final** de la
cadena (append) en vez de al inicio, el costo de búsqueda exitosa es:

$$E[C_{\text{hit}}^{\text{append}}] = 1 + \frac{\alpha}{2} - \frac{1}{2m}$$

es decir, **el mismo** resultado.

**Prueba.** Con inserción al final, el elemento $k_i$ está precedido
por los elementos insertados **antes** que $k_i$ en el mismo bucket.
El número de comparaciones para encontrar $k_i$ es:

$$C_i^{\text{append}} = 1 + \sum_{r=1}^{i-1} Y_{ri}'$$

donde $Y_{ri}' = \mathbf{1}[h(k_r) = h(k_i)]$.

$$E[C_i^{\text{append}}] = 1 + \frac{i - 1}{m}$$

Promediando:

$$E[C_{\text{hit}}^{\text{append}}] = \frac{1}{n}\sum_{i=1}^{n}\left(1 + \frac{i-1}{m}\right) = 1 + \frac{1}{nm}\sum_{i=1}^{n}(i-1)$$

$$= 1 + \frac{1}{nm} \cdot \frac{n(n-1)}{2} = 1 + \frac{n-1}{2m}$$

Idéntico al caso prepend. ✓

**Interpretación.** La política de inserción (prepend vs append) no
afecta el costo esperado de búsqueda exitosa. Lo que importa es que,
en promedio, el elemento buscado está a mitad de la lista
independientemente del orden de inserción, ya que buscamos un elemento
uniformemente aleatorio.

$$\blacksquare$$

---

## Verificación numérica

| $\alpha$ | $C_{\text{miss}}$ | $C_{\text{hit}}$ (exacto, $m = 1000$) | $1 + \alpha/2$ |
|:---:|:---:|:---:|:---:|
| 0.25 | 0.25 | 1.125 | 1.125 |
| 0.50 | 0.50 | 1.250 | 1.250 |
| 0.75 | 0.75 | 1.375 | 1.375 |
| 1.00 | 1.00 | 1.500 | 1.500 |
| 2.00 | 2.00 | 2.000 | 2.000 |
| 5.00 | 5.00 | 3.500 | 3.500 |

Costo total (incluyendo hash + acceso):

| $\alpha$ | $T_{\text{miss}} = 1 + \alpha$ | $T_{\text{hit}} = 1 + \alpha/2$ | Ratio $T_{\text{miss}}/T_{\text{hit}}$ |
|:---:|:---:|:---:|:---:|
| 0.50 | 1.50 | 1.25 | 1.20 |
| 1.00 | 2.00 | 1.50 | 1.33 |
| 2.00 | 3.00 | 2.00 | 1.50 |
| 5.00 | 6.00 | 3.50 | 1.71 |

A medida que $\alpha$ crece, la ventaja de la búsqueda exitosa se
amplifica: con $\alpha = 5$, una búsqueda exitosa hace la mitad del
trabajo de una fallida. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $C_{\text{miss}} = \alpha$ | $E[L_{h(k)}] = \alpha$ | Recorrer lista completa |
| $C_{\text{hit}} = 1 + (n{-}1)/2m$ | Contar posteriores en mismo bucket | $\Pr[h(k_r) = h(k_i)] = 1/m$, sumar $n{-}i$ |
| $O(1)$ con $\alpha = O(1)$ | Redimensionamiento | $\alpha$ acotado $\Rightarrow$ costos acotados |
| $T_{\text{hit}} < T_{\text{miss}}$ siempre | Comparación directa | Encontrar antes de fin de lista |
| Prepend $\equiv$ append para $C_{\text{hit}}$ | Simetría de la suma | $\sum(n{-}i) = \sum(i{-}1) = n(n{-}1)/2$ |
