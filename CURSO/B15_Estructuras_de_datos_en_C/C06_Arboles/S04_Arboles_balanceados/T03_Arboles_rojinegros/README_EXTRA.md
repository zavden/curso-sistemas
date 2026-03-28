# T03 — Demostración formal: cota de altura de árboles rojinegros

> Complemento riguroso del README.md. Aquí se prueba formalmente que
> la altura de un árbol rojinegro con $n$ nodos internos satisface
> $h \leq 2 \cdot \log_2(n + 1)$ usando el argumento de black-height
> por inducción.

---

## Recordatorio: propiedades rojinegras

Un árbol rojinegro es un BST donde cada nodo tiene color rojo o negro
y se cumplen:

```
P1. Cada nodo es rojo o negro.
P2. La raíz es negra.
P3. Cada hoja (NULL) es negra.
P4. Si un nodo es rojo, ambos hijos son negros.
P5. Para cada nodo, todos los caminos desde él hasta las hojas NULL
    contienen el mismo número de nodos negros.
```

**Definición (Black-height).** El black-height de un nodo $x$, denotado
$\text{bh}(x)$, es el número de nodos negros en cualquier camino desde
$x$ (sin incluir a $x$) hasta una hoja NULL. La propiedad P5 garantiza
que este valor está bien definido (es el mismo para todos los caminos).

---

## Demostración 1: un subárbol tiene al menos $2^{\text{bh}(x)} - 1$ nodos internos

**Teorema.** Sea $x$ un nodo en un árbol rojinegro. El subárbol
enraizado en $x$ contiene al menos $2^{\text{bh}(x)} - 1$ nodos
internos.

**Prueba (por inducción sobre la altura de $x$).**

**Base (altura 0).** Si $x$ es una hoja NULL, entonces
$\text{bh}(x) = 0$ y el subárbol tiene 0 nodos internos:

$$2^{\text{bh}(x)} - 1 = 2^0 - 1 = 0 \quad \checkmark$$

**Paso inductivo.** Sea $x$ un nodo interno con hijos $l$ (izquierdo)
y $r$ (derecho), ambos con altura menor que la de $x$.

Por hipótesis inductiva:
- El subárbol de $l$ tiene al menos $2^{\text{bh}(l)} - 1$ nodos
  internos.
- El subárbol de $r$ tiene al menos $2^{\text{bh}(r)} - 1$ nodos
  internos.

Ahora determinamos $\text{bh}(l)$ y $\text{bh}(r)$ en función de
$\text{bh}(x)$:

- Si un hijo es **negro**, su black-height es $\text{bh}(x) - 1$
  (porque al descender, pasamos por un nodo negro, que se resta del
  conteo).
- Si un hijo es **rojo**, su black-height es $\text{bh}(x)$
  (un nodo rojo no cuenta para el black-height).

En ambos casos: $\text{bh}(\text{hijo}) \geq \text{bh}(x) - 1$.

Usando la hipótesis inductiva con la cota inferior:

$$\text{nodos internos en subárbol de } x \geq (2^{\text{bh}(l)} - 1) + (2^{\text{bh}(r)} - 1) + 1$$

(el $+1$ es por el propio nodo $x$).

Como $\text{bh}(l) \geq \text{bh}(x) - 1$ y
$\text{bh}(r) \geq \text{bh}(x) - 1$:

$$\geq (2^{\text{bh}(x) - 1} - 1) + (2^{\text{bh}(x) - 1} - 1) + 1$$

$$= 2 \cdot 2^{\text{bh}(x) - 1} - 2 + 1$$

$$= 2^{\text{bh}(x)} - 1$$

$$\therefore \text{El subárbol de } x \text{ tiene } \geq 2^{\text{bh}(x)} - 1 \text{ nodos internos.} \qquad \blacksquare$$

---

## Demostración 2: $\text{bh}(\text{raíz}) \geq h/2$

**Teorema.** Sea $T$ un árbol rojinegro con altura $h$ y raíz $r$.
Entonces $\text{bh}(r) \geq h/2$.

**Prueba.** Consideremos un camino más largo desde la raíz hasta una
hoja NULL. Este camino tiene $h$ aristas (y $h$ nodos internos antes
del NULL).

Por la propiedad P4 (no hay dos rojos consecutivos), en cualquier
camino de longitud $h$, a lo sumo la mitad de los nodos pueden ser
rojos. Por lo tanto, al menos $\lceil h/2 \rceil$ nodos son negros.

Más formalmente: en un camino de $h$ nodos, si dos nodos consecutivos
no pueden ser ambos rojos, entonces entre cada par de rojos debe haber
al menos un negro. La configuración que maximiza los rojos es la
alternancia rojo-negro-rojo-negro..., que da exactamente $\lfloor h/2 \rfloor$
rojos y $\lceil h/2 \rceil$ negros.

El black-height de la raíz cuenta los nodos negros en un camino
raíz→NULL **sin contar la raíz**. La raíz es negra (propiedad P2),
así que:

$$\text{bh}(r) = \text{negros en el camino} - \underbrace{1}_{\text{si contamos la raíz, la restamos}}$$

Pero por convención, $\text{bh}(r)$ no incluye a $r$ mismo. De los
$h$ nodos en el camino (incluyendo $r$), al menos $\lceil h/2 \rceil$
son negros. Restando la raíz (que es negra):

$$\text{bh}(r) \geq \lceil h/2 \rceil - 1$$

Sin embargo, la desigualdad $\text{bh}(r) \geq h/2$ se obtiene más
directamente. Observemos que en un camino de longitud $h$:

- La raíz $r$ es negra (P2), no se cuenta en $\text{bh}(r)$.
- Los $h$ nodos restantes del camino (de $r$ a NULL, sin incluir $r$
  pero incluyendo los hijos) alternan como máximo rojo-negro.
- De los $h$ nodos debajo de $r$, al menos la mitad son negros.

Así: $\text{bh}(r) \geq h/2$.

**Justificación alternativa (más directa).** En un camino
raíz→NULL de longitud $h$, sean $b$ los nodos negros bajo la raíz
y $r_{\text{count}}$ los rojos. Entonces $b + r_{\text{count}} = h$ y
$\text{bh}(\text{raíz}) = b$. Por P4, entre cada par de nodos
consecutivos, al menos uno es negro, lo que implica
$r_{\text{count}} \leq b$ (cada rojo necesita un negro adyacente
debajo). De $r_{\text{count}} \leq b$ y $b + r_{\text{count}} = h$:

$$h = b + r_{\text{count}} \leq b + b = 2b$$

$$b \geq h/2$$

$$\therefore \text{bh}(r) \geq h/2. \qquad \blacksquare$$

---

## Demostración 3: $h \leq 2 \cdot \log_2(n + 1)$

**Teorema.** Todo árbol rojinegro con $n$ nodos internos tiene altura:

$$h \leq 2 \cdot \log_2(n + 1)$$

**Prueba.** Sea $T$ un árbol rojinegro con $n$ nodos internos, altura
$h$ y raíz $r$.

**Paso 1: acotar $n$ por abajo.**

Por la Demostración 1, aplicada a la raíz:

$$n \geq 2^{\text{bh}(r)} - 1$$

**Paso 2: relacionar $\text{bh}(r)$ con $h$.**

Por la Demostración 2:

$$\text{bh}(r) \geq h/2$$

**Paso 3: combinar.**

Sustituyendo:

$$n \geq 2^{\text{bh}(r)} - 1 \geq 2^{h/2} - 1$$

Despejando $h$:

$$n + 1 \geq 2^{h/2}$$

Tomando $\log_2$ en ambos lados (ambos positivos):

$$\log_2(n + 1) \geq h/2$$

$$h \leq 2 \cdot \log_2(n + 1)$$

$$\therefore h \leq 2 \cdot \log_2(n + 1). \qquad \blacksquare$$

---

## Comparación formal con AVL

**Corolario.** Para $n$ nodos, las cotas de altura son:

| Estructura | Cota de altura | Constante multiplicativa |
|:---:|:---:|:---:|
| Árbol perfecto | $\log_2(n+1)$ | 1.0 |
| AVL | $1.44 \cdot \log_2(n+2)$ | 1.44 |
| Rojinegro | $2 \cdot \log_2(n+1)$ | 2.0 |
| BST sin balance | $n - 1$ | — |

**Prueba de que la cota rojinegra es alcanzable.** Considérese un árbol
rojinegro donde el camino más largo alterna rojo-negro y el más corto
es todo negro. Si $\text{bh} = k$:

- Camino más corto: $k$ nodos (todos negros) → longitud $k$.
- Camino más largo: $k$ negros intercalados con $k$ rojos → longitud $2k$.

Ejemplo con $\text{bh} = 2$:

```
Camino largo:  raíz(B) → rojo → negro → rojo → negro → NULL
               longitud = 4 = 2·bh

Camino corto:  raíz(B) → negro → negro → NULL
               longitud = 2 = bh
```

La razón entre caminos es exactamente 2:1, confirmando que el factor 2
en la cota es ajustado (tight). $\square$

---

## Demostración 4: operaciones rojinegras son $O(\log n)$

**Corolario.** La búsqueda, inserción y eliminación en un árbol
rojinegro con $n$ nodos tienen complejidad $O(\log n)$ en el peor caso.

**Prueba.**

*Búsqueda*: recorre un camino raíz→hoja, con $O(1)$ por nivel.
Costo $= O(h) = O(\log n)$. ✓

*Inserción*: inserción BST estándar $O(h)$, seguida de correcciones.
Las correcciones (recoloreo + rotaciones) suben por el árbol como máximo
$O(h)$ niveles, con $O(1)$ por nivel. Además, se realizan **a lo sumo
2 rotaciones** en total (después de las cuales el recoloreo se propaga
sin más rotaciones). Costo total $= O(h) = O(\log n)$. ✓

*Eliminación*: eliminación BST estándar $O(h)$, seguida de correcciones.
Se realizan **a lo sumo 3 rotaciones** en total, más $O(h)$ recoloreos.
Costo total $= O(h) = O(\log n)$. ✓

$$\blacksquare$$

**Ventaja sobre AVL**: aunque la cota de altura rojinegra ($2 \log_2 n$)
es peor que la AVL ($1.44 \log_2 n$), la inserción y eliminación en un
rojinegro requieren menos rotaciones en el peor caso (2-3 rotaciones vs
$O(\log n)$ en AVL). En la práctica, los rojinegros suelen ser más
rápidos en escenarios con muchas inserciones/eliminaciones.

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|---------|-------|
| Subárbol de $x$ tiene $\geq 2^{\text{bh}(x)} - 1$ nodos | Inducción sobre altura | $\text{bh}(\text{hijo}) \geq \text{bh}(x) - 1$ |
| $\text{bh}(r) \geq h/2$ | P4 (no dos rojos seguidos) | $r_{\text{count}} \leq b \Rightarrow h \leq 2b$ |
| $h \leq 2 \cdot \log_2(n+1)$ | Combinar las dos anteriores | $n \geq 2^{h/2} - 1$ |
| Cota es ajustada (tight) | Ejemplo con alternancia rojo-negro | Razón camino largo/corto = 2:1 |
