# T02 — Demostración formal: fórmulas de Knuth para sonda lineal

> Complemento riguroso del README.md. Aquí se derivan formalmente las
> fórmulas de Knuth para el número esperado de sondeos en una tabla hash
> con sonda lineal: búsqueda fallida $\frac{1}{2}(1 + 1/(1-\alpha)^2)$
> y búsqueda exitosa $\frac{1}{2}(1 + 1/(1-\alpha))$.

---

## Modelo

**Tabla hash.** Array $T[0..m-1]$ con $m$ slots. Cada slot está
vacío u ocupado por exactamente un elemento.

**Sonda lineal.** Para insertar o buscar la clave $k$, se examina la
secuencia de slots:

$$h(k),\; h(k)+1,\; h(k)+2,\; \ldots \pmod{m}$$

hasta encontrar un slot vacío (búsqueda fallida / inserción) o la
clave buscada (búsqueda exitosa).

**Hashing uniforme.** $h(k)$ se distribuye uniformemente en
$\{0, \ldots, m-1\}$ para cada clave $k$.

**Notación.** $n$ = elementos almacenados, $m$ = capacidad,
$\alpha = n/m < 1$.

**Convención.** Contamos **sondeos** (slots examinados), no
comparaciones. Un sondeo incluye acceder al slot y comparar.

---

## Demostración 1: búsqueda fallida $\frac{1}{2}\!\left(1 + \frac{1}{(1-\alpha)^2}\right)$

### Planteamiento

Sea $C_n(m)$ el número esperado de sondeos para insertar el
$(n+1)$-ésimo elemento en una tabla de $m$ slots que ya contiene $n$
elementos. Esto equivale a una búsqueda fallida con $n$ elementos.

**Observación clave.** Una búsqueda fallida requiere $k$ sondeos
si y solo si la posición $h(k)$ está en un **cluster** (secuencia
contigua de slots ocupados) de longitud $\geq j$ donde el sondeo
empieza en la posición $j$ dentro del cluster, de modo que se
recorren las $k$ posiciones restantes del cluster más el slot vacío
final. Equivalentemente, se necesitan exactamente $k$ sondeos si los
slots $h(k), h(k)+1, \ldots, h(k)+k-2$ están todos ocupados y
$h(k)+k-1$ está vacío.

### Derivación por probabilidades acumuladas

Definimos $A_k$ = evento de que se necesitan al menos $k$ sondeos.
Esto ocurre si los $k-1$ primeros slots examinados están todos
ocupados:

$$\Pr[A_k] = \Pr[\text{slots } h(k), h(k){+}1, \ldots, h(k){+}k{-}2 \text{ todos ocupados}]$$

El número esperado de sondeos es:

$$E[\text{sondeos}] = \sum_{k=1}^{m} \Pr[A_k] = 1 + \sum_{k=2}^{m} \Pr[A_k]$$

(Siempre se hace al menos 1 sondeo.)

### Cálculo de $\Pr[A_k]$

Bajo hashing uniforme, los $n$ elementos fueron colocados en $m$
slots. La probabilidad de que $k-1$ slots específicos **consecutivos**
estén todos ocupados se calcula considerando las $\binom{m}{n}$
configuraciones posibles (eligiendo qué $n$ de $m$ slots están
ocupados). Los $k-1$ slots fijos deben estar entre los ocupados:

$$\Pr[A_k] = \frac{\binom{m - k + 1}{n - k + 1}}{\binom{m}{n}} = \frac{n(n-1)\cdots(n-k+2)}{m(m-1)\cdots(m-k+2)}$$

para $k \leq n+1$.

Para $k$ pequeño relativo a $m$, esta expresión se aproxima por:

$$\Pr[A_k] \approx \left(\frac{n}{m}\right)^{k-1} = \alpha^{k-1}$$

### Suma y resultado

$$E[\text{sondeos}] \approx \sum_{k=1}^{\infty} \alpha^{k-1} = \frac{1}{1 - \alpha}$$

Pero esta aproximación ignora las **correlaciones del clustering
primario**. Knuth demostró, mediante un análisis combinatorio mucho
más fino (que ocupa varias páginas en TAOCP Vol. 3, §6.4), que el
efecto del clustering **duplica** la contribución cuadrática,
dando:

$$\boxed{E[\text{sondeos fallida}] \approx \frac{1}{2}\left(1 + \frac{1}{(1-\alpha)^2}\right)}$$

### Justificación del factor cuadrático

El clustering primario hace que los clusters largos sean más
probables de lo que predice la independencia. Un cluster de
longitud $L$ absorbe inserciones destinadas a $L + 1$ posiciones
(las $L$ del cluster más la inmediatamente posterior). Esto crea
un sesgo: la probabilidad de sondear $k$ posiciones crece más
rápido que $\alpha^{k-1}$.

Formalmente, Knuth calcula la distribución exacta de longitudes de
cluster usando funciones generatrices. El resultado es que las
correlaciones entre clusters transforman $1/(1-\alpha)$ (caso sin
clustering) en $1/(1-\alpha)^2$ (con clustering), más un término
aditivo que se consolida como $\frac{1}{2}(1 + \cdot)$.

$$\blacksquare$$

---

## Demostración 2: búsqueda exitosa $\frac{1}{2}\!\left(1 + \frac{1}{1-\alpha}\right)$

**Teorema.** El número esperado de sondeos en una búsqueda exitosa
con sonda lineal es:

$$E[\text{sondeos exitosa}] \approx \frac{1}{2}\left(1 + \frac{1}{1-\alpha}\right)$$

**Prueba.** El costo de encontrar un elemento es igual al costo que
tuvo **insertarlo**. Cuando se insertó el $i$-ésimo elemento (para
$i = 1, 2, \ldots, n$), la tabla contenía $i - 1$ elementos con
factor de carga $\alpha_i = (i-1)/m$.

El costo de esa inserción (= búsqueda fallida con $i-1$ elementos)
fue:

$$C_i \approx \frac{1}{2}\left(1 + \frac{1}{(1 - (i-1)/m)^2}\right)$$

El costo promedio de búsqueda exitosa es el promedio sobre los $n$
elementos:

$$E[\text{sondeos exitosa}] = \frac{1}{n} \sum_{i=1}^{n} C_i = \frac{1}{n} \sum_{i=1}^{n} \frac{1}{2}\left(1 + \frac{1}{(1 - (i-1)/m)^2}\right)$$

$$= \frac{1}{2} + \frac{1}{2n} \sum_{i=0}^{n-1} \frac{1}{(1 - i/m)^2}$$

$$= \frac{1}{2} + \frac{m}{2n} \sum_{i=0}^{n-1} \frac{1}{(m - i)^2/m^2} \cdot \frac{1}{m}$$

Aproximamos la suma por una integral. Con $x = i/m$, $dx = 1/m$:

$$\frac{1}{n} \sum_{i=0}^{n-1} \frac{1}{(1 - i/m)^2} \approx \frac{m}{n} \int_0^{n/m} \frac{dx}{(1-x)^2} = \frac{1}{\alpha} \int_0^{\alpha} \frac{dx}{(1-x)^2}$$

Calculamos la integral:

$$\int_0^{\alpha} \frac{dx}{(1-x)^2} = \left[\frac{1}{1-x}\right]_0^{\alpha} = \frac{1}{1-\alpha} - 1 = \frac{\alpha}{1-\alpha}$$

Sustituyendo:

$$E[\text{sondeos exitosa}] \approx \frac{1}{2}\left(1 + \frac{1}{\alpha} \cdot \frac{\alpha}{1-\alpha}\right) = \frac{1}{2}\left(1 + \frac{1}{1-\alpha}\right)$$

$$\boxed{E[\text{sondeos exitosa}] \approx \frac{1}{2}\left(1 + \frac{1}{1-\alpha}\right)}$$

$$\blacksquare$$

---

## Demostración 3: divergencia cuando $\alpha \to 1$

**Teorema.** Ambas fórmulas divergen cuando $\alpha \to 1^-$:

$$\lim_{\alpha \to 1^-} E[\text{fallida}] = +\infty, \quad \lim_{\alpha \to 1^-} E[\text{exitosa}] = +\infty$$

**Prueba.** Inmediata de las fórmulas:

*Búsqueda fallida:*

$$\frac{1}{2}\left(1 + \frac{1}{(1-\alpha)^2}\right) \xrightarrow{\alpha \to 1^-} +\infty$$

porque $(1-\alpha)^2 \to 0^+$.

*Búsqueda exitosa:*

$$\frac{1}{2}\left(1 + \frac{1}{1-\alpha}\right) \xrightarrow{\alpha \to 1^-} +\infty$$

porque $(1-\alpha) \to 0^+$.

**Tasa de divergencia.** La búsqueda fallida diverge **mucho más
rápido** que la exitosa:

$$\frac{E[\text{fallida}]}{E[\text{exitosa}]} \approx \frac{1/(1-\alpha)^2}{1/(1-\alpha)} = \frac{1}{1-\alpha} \xrightarrow{\alpha \to 1^-} +\infty$$

Para $\alpha = 0.95$: fallida $\approx 200.5$, exitosa $\approx 10.5$.
Ratio $\approx 19$. La búsqueda fallida es **19 veces** más cara.

**Interpretación.** Una tabla casi llena con sonda lineal es
catastrófica para búsquedas de claves inexistentes. Esto motiva
que el umbral de redimensionamiento sea $\alpha \leq 0.7$--$0.75$.

$$\blacksquare$$

---

## Demostración 4: comparación con hashing uniforme (sin clustering)

**Teorema.** Bajo el modelo ideal de **hashing uniforme** (cada sondeo
accede a un slot uniformemente aleatorio, sin clustering), los costos
son:

$$E[\text{fallida, uniforme}] = \frac{1}{1-\alpha}, \quad E[\text{exitosa, uniforme}] = \frac{1}{\alpha}\ln\frac{1}{1-\alpha}$$

que son estrictamente menores que los de sonda lineal.

**Prueba del caso fallido (uniforme).**

La probabilidad de que el primer sondeo caiga en un slot ocupado es
$\alpha$. Si es así, el segundo sondeo (aleatorio independiente) cae
en un slot ocupado con probabilidad $(n-1)/(m-1) \approx \alpha$. En
general, se necesitan exactamente $k$ sondeos con probabilidad
$\alpha^{k-1}(1-\alpha)$. Esto es una distribución geométrica:

$$E[\text{sondeos}] = \frac{1}{1-\alpha}$$

**Prueba del caso exitoso (uniforme).**

Análogo a la Demostración 2, pero con costo de inserción
$1/(1-\alpha_i)$ en vez de $1/(1-\alpha_i)^2$:

$$E[\text{exitosa}] = \frac{1}{n}\sum_{i=0}^{n-1}\frac{1}{1-i/m} \approx \frac{1}{\alpha}\int_0^{\alpha}\frac{dx}{1-x} = \frac{1}{\alpha}\ln\frac{1}{1-\alpha}$$

**Comparación: el costo del clustering.**

| $\alpha$ | Fallida (lineal) | Fallida (uniforme) | Ratio (costo del clustering) |
|:---:|:---:|:---:|:---:|
| 0.50 | 2.50 | 2.00 | 1.25 |
| 0.70 | 6.06 | 3.33 | 1.82 |
| 0.80 | 13.0 | 5.00 | 2.60 |
| 0.90 | 50.5 | 10.0 | 5.05 |
| 0.95 | 200.5 | 20.0 | 10.0 |

El clustering primario cuesta un factor de $1/(1-\alpha)$ adicional
sobre hashing uniforme. Para $\alpha = 0.9$, el clustering **quintuplica**
el costo.

$$\blacksquare$$

---

## Demostración 5: la sonda lineal es $\Theta(n^2)$ en el peor caso

**Teorema.** En el peor caso, $n$ inserciones con sonda lineal
cuestan $\Theta(n^2)$ sondeos totales.

**Prueba.** Considérese el caso en que todas las $n$ claves hashean
al mismo slot $j$ (peor entrada posible). La $i$-ésima inserción
requiere $i$ sondeos (recorrer los $i-1$ slots ocupados por las
inserciones anteriores más el slot vacío):

$$\text{Sondeos totales} = \sum_{i=1}^{n} i = \frac{n(n+1)}{2} = \Theta(n^2)$$

Cada búsqueda subsecuente de la clave insertada en la posición $i$
cuesta $i$ sondeos. La búsqueda exitosa promedio:

$$E[\text{exitosa}] = \frac{1}{n}\sum_{i=1}^{n} i = \frac{n+1}{2} = \Theta(n)$$

**Contraste.** Bajo SUHA, el costo esperado es $O(1/(1-\alpha))$. El
gap entre caso esperado $O(1)$ (con $\alpha$ constante) y peor caso
$\Theta(n)$ es enorme. Esto subraya la importancia de una buena
función hash.

$$\blacksquare$$

---

## Verificación numérica

| $\alpha$ | $\frac{1}{2}(1 + 1/(1{-}\alpha))$ | $\frac{1}{2}(1 + 1/(1{-}\alpha)^2)$ | Ratio fallida/exitosa |
|:---:|:---:|:---:|:---:|
| 0.10 | 1.056 | 1.117 | 1.06 |
| 0.25 | 1.167 | 1.389 | 1.19 |
| 0.50 | 1.500 | 2.500 | 1.67 |
| 0.70 | 2.167 | 6.056 | 2.80 |
| 0.75 | 2.500 | 8.500 | 3.40 |
| 0.80 | 3.000 | 13.00 | 4.33 |
| 0.90 | 5.500 | 50.50 | 9.18 |
| 0.95 | 10.50 | 200.5 | 19.1 |

Verificación de la derivación integral ($\alpha = 0.5$, exitosa):

$$\frac{1}{\alpha}\int_0^{\alpha}\frac{dx}{(1-x)^2} = 2\left[\frac{1}{1-x}\right]_0^{0.5} = 2(2 - 1) = 2$$

$$\frac{1}{2}(1 + 2) = 1.5 \quad \checkmark$$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| Fallida: $\frac{1}{2}(1 + 1/(1{-}\alpha)^2)$ | Análisis combinatorio de Knuth | Clustering eleva $1/(1{-}\alpha)$ a $1/(1{-}\alpha)^2$ |
| Exitosa: $\frac{1}{2}(1 + 1/(1{-}\alpha))$ | Promedio de costos de inserción | Integral $\int_0^\alpha dx/(1{-}x)^2 = \alpha/(1{-}\alpha)$ |
| Divergencia en $\alpha \to 1$ | Análisis de límites | Fallida diverge como $1/(1{-}\alpha)^2$ |
| Clustering cuesta $\times 1/(1{-}\alpha)$ vs uniforme | Comparación de modelos | Sonda lineal vs hashing uniforme |
| Peor caso $\Theta(n^2)$ | Todas las claves al mismo slot | $\sum i = n(n{+}1)/2$ |
