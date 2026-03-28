# T04 — Demostración formal: build-heap bottom-up es $O(n)$

> Complemento riguroso del README.md. Aquí se prueba formalmente que
> la construcción bottom-up de un heap (algoritmo de Floyd) tiene
> complejidad $O(n)$, incluyendo la derivación completa de la serie
> $\sum k/2^k$.

---

## Modelo

Consideramos un heap binario almacenado en un array de $n$ elementos.
La altura del heap es $h = \lfloor \log_2 n \rfloor$.

**Convención de niveles.** Numeramos los niveles desde abajo: el nivel 0
son las hojas, el nivel $k$ tiene nodos a distancia $k$ del nivel de
hojas, y el nivel $h$ es la raíz.

**Costo de sift-down.** Un sift-down en un nodo de nivel $k$ (medido
desde abajo) puede descender a lo sumo $k$ niveles, con $O(1)$ de
trabajo por nivel. Así, su costo es $O(k)$.

**Algoritmo de Floyd.** Ejecuta sift-down para cada nodo interno,
de derecha a izquierda y de abajo hacia arriba (desde el índice
$\lfloor n/2 \rfloor - 1$ hasta 0).

---

## Demostración 1: conteo de nodos por nivel

**Lema.** En un heap completo de $n$ nodos, el número de nodos en el
nivel $k$ (desde abajo) es a lo sumo $\lceil n / 2^{k+1} \rceil$.

**Prueba.** En un árbol binario completo con $n$ nodos, el nivel $d$
(desde arriba) tiene a lo sumo $2^d$ nodos. El nivel $d$ desde arriba
corresponde al nivel $k = h - d$ desde abajo, donde $h = \lfloor \log_2 n \rfloor$.

Los nodos en el nivel $k$ desde abajo tienen profundidad $h - k$ desde
arriba, así que hay a lo sumo $2^{h-k}$ nodos. Como $2^h \leq n < 2^{h+1}$:

$$2^{h-k} = \frac{2^h}{2^k} \leq \frac{n}{2^k}$$

Además, cada nodo de nivel $k$ es raíz de un subárbol de altura $k$.
Cada subárbol tiene al menos $2^k$ nodos (si es completo), así que
los subárboles son disjuntos y abarcan como máximo $n$ nodos:

$$\text{nodos en nivel } k \leq \left\lfloor \frac{n}{2^k} \right\rfloor$$

Para el algoritmo de Floyd, solo nos interesan los nodos internos
(nivel $k \geq 1$). El número de nodos internos en nivel $k$ es a lo
sumo $\lfloor n / 2^{k+1} \rfloor$ (ya que las hojas, nivel 0, ocupan
$\lceil n/2 \rceil$ posiciones).

$$\blacksquare$$

---

## Demostración 2: la serie $\sum_{k=0}^{\infty} k x^k = \frac{x}{(1-x)^2}$

Antes de probar la complejidad de build-heap, derivamos la identidad
que necesitamos.

**Teorema.** Para $|x| < 1$:

$$\sum_{k=0}^{\infty} k \cdot x^k = \frac{x}{(1 - x)^2}$$

**Prueba.** Partimos de la serie geométrica:

$$\sum_{k=0}^{\infty} x^k = \frac{1}{1-x} \qquad \text{para } |x| < 1$$

Derivamos ambos lados respecto a $x$. El lado izquierdo:

$$\frac{d}{dx} \sum_{k=0}^{\infty} x^k = \sum_{k=1}^{\infty} k \cdot x^{k-1}$$

(la derivación término a término es válida dentro del radio de
convergencia).

El lado derecho:

$$\frac{d}{dx} \frac{1}{1-x} = \frac{1}{(1-x)^2}$$

Así:

$$\sum_{k=1}^{\infty} k \cdot x^{k-1} = \frac{1}{(1-x)^2}$$

Multiplicamos ambos lados por $x$:

$$\sum_{k=1}^{\infty} k \cdot x^{k} = \frac{x}{(1-x)^2}$$

Como el término $k=0$ aporta $0 \cdot x^0 = 0$:

$$\sum_{k=0}^{\infty} k \cdot x^{k} = \frac{x}{(1-x)^2}$$

$$\blacksquare$$

**Corolario.** Evaluando en $x = 1/2$:

$$\sum_{k=0}^{\infty} \frac{k}{2^k} = \frac{1/2}{(1 - 1/2)^2} = \frac{1/2}{1/4} = 2$$

**Verificación parcial:**

| $k$ | $k/2^k$ | Suma parcial |
|:---:|:-------:|:------------:|
| 0 | 0 | 0 |
| 1 | 0.5 | 0.5 |
| 2 | 0.5 | 1.0 |
| 3 | 0.375 | 1.375 |
| 4 | 0.25 | 1.625 |
| 5 | 0.15625 | 1.78125 |
| 6 | 0.09375 | 1.875 |
| 10 | 0.00977 | 1.98047 |
| $\infty$ | — | **2** |

La convergencia es rápida: con $k = 6$ ya estamos al 94% del total.

---

## Demostración 3: build-heap es $O(n)$

**Teorema.** El algoritmo build-heap bottom-up (Floyd) ejecuta a lo
sumo $n$ comparaciones de sift-down. Su complejidad es $O(n)$.

**Prueba.** El trabajo total $W$ es la suma de los costos de sift-down
sobre todos los nodos internos:

$$W = \sum_{k=1}^{h} (\text{nodos en nivel } k) \times (\text{costo de sift-down en nivel } k)$$

El costo de sift-down para un nodo en nivel $k$ es a lo sumo $k$
(desciende a lo sumo $k$ niveles). El número de nodos en nivel $k$
es a lo sumo $\lfloor n / 2^{k+1} \rfloor$ (por la Demostración 1):

$$W \leq \sum_{k=1}^{h} \left\lfloor \frac{n}{2^{k+1}} \right\rfloor \cdot k$$

Acotamos eliminando los pisos ($\lfloor x \rfloor \leq x$):

$$W \leq \sum_{k=1}^{h} \frac{n}{2^{k+1}} \cdot k = \frac{n}{2} \sum_{k=1}^{h} \frac{k}{2^k}$$

Acotamos la suma parcial por la serie infinita:

$$\sum_{k=1}^{h} \frac{k}{2^k} \leq \sum_{k=1}^{\infty} \frac{k}{2^k} = \sum_{k=0}^{\infty} \frac{k}{2^k} = 2$$

(por la Demostración 2 con $x = 1/2$).

Por lo tanto:

$$W \leq \frac{n}{2} \cdot 2 = n$$

El trabajo total de build-heap es a lo sumo $n$.

$$\therefore \text{build-heap es } O(n). \qquad \blacksquare$$

---

## Demostración 4: build-heap es $\Omega(n)$

**Teorema.** Cualquier algoritmo que construya un heap a partir de un
array arbitrario requiere $\Omega(n)$ operaciones.

**Prueba.** Cada uno de los $n$ elementos debe ser examinado al menos
una vez: un elemento no examinado podría ser el mínimo (o máximo) y
estar en una posición incorrecta. Cualquier algoritmo que lo ignore
podría producir un heap inválido.

Formalmente: para $n$ entradas arbitrarias, cualquier algoritmo basado
en comparaciones necesita leer al menos $n$ valores. Esto da una cota
inferior de $\Omega(n)$.

$$\therefore \text{build-heap es } \Theta(n). \qquad \blacksquare$$

---

## Demostración 5: build-heap top-down es $\Theta(n \log n)$

**Teorema.** La construcción top-down (insertar uno a uno con sift-up)
tiene complejidad $\Theta(n \log n)$.

**Prueba.**

*Cota superior.* Cada inserción $i$ ejecuta un sift-up de costo a lo
sumo $\lfloor \log_2 i \rfloor$:

$$W \leq \sum_{i=1}^{n} \lfloor \log_2 i \rfloor \leq \sum_{i=1}^{n} \log_2 n = n \log_2 n = O(n \log n)$$

*Cota inferior.* Los nodos en la segunda mitad del array ($i > n/2$)
están a profundidad $\geq \lfloor \log_2(n/2) \rfloor = \lfloor \log_2 n \rfloor - 1$.
Para una entrada adversarial (por ejemplo, secuencia decreciente en un
max-heap), cada uno de estos $n/2$ nodos ejecuta un sift-up de longitud
$\geq \lfloor \log_2 n \rfloor - 1$:

$$W \geq \frac{n}{2} \cdot (\lfloor \log_2 n \rfloor - 1) = \Omega(n \log n)$$

Combinando: $W = \Theta(n \log n)$.

$$\blacksquare$$

---

## Por qué la diferencia entre top-down y bottom-up

La asimetría se resume así:

```
Bottom-up (sift-down):
  Muchos nodos (n/2 hojas) hacen 0 trabajo.
  Pocos nodos (la raíz) hacen O(log n) trabajo.
  → Los pocos nodos caros no compensan los muchos baratos.

Top-down (sift-up):
  Muchos nodos (n/2 en el fondo) hacen O(log n) trabajo.
  Pocos nodos (la raíz) hacen 0 trabajo.
  → Los muchos nodos caros dominan.
```

Formalmente, la diferencia se reduce a qué serie se suma:

| Método | Serie | Valor |
|:------:|:-----:|:-----:|
| Bottom-up | $\sum_{k=0}^{h} k / 2^k$ | $2$ (converge) |
| Top-down | $\sum_{k=0}^{h} k \cdot 2^k / n$ | $\Theta(\log n)$ por nodo |

En bottom-up, los pesos ($1/2^k$) decrecen exponencialmente con $k$,
haciendo que la serie converja. En top-down, los pesos ($2^k$) crecen
exponencialmente, haciendo que los términos grandes dominen.

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|---------|-------|
| Nodos en nivel $k$: $\leq n/2^{k+1}$ | Conteo en árbol binario | Subárboles disjuntos de altura $k$ |
| $\sum k/2^k = 2$ | Derivar serie geométrica | $d/dx [1/(1-x)] = 1/(1-x)^2$ |
| Build-heap bottom-up: $O(n)$ | Sumar trabajo por nivel | $W \leq (n/2) \cdot 2 = n$ |
| Build-heap bottom-up: $\Omega(n)$ | Cota inferior trivial | Cada elemento debe examinarse |
| Build-heap top-down: $\Theta(n \log n)$ | Cotas superior e inferior | $n/2$ nodos hacen $\Omega(\log n)$ trabajo |
