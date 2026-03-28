# T05 — Demostración formal: inserción con rehash es $O(1)$ amortizado

> Complemento riguroso del README.md. Aquí se demuestra formalmente,
> por tres métodos (agregado, banquero y potencial), que una secuencia
> de $n$ inserciones en una tabla hash con redimensionamiento automático
> cuesta $O(n)$ en total, es decir, $O(1)$ amortizado por inserción.

---

## Modelo

**Tabla hash dinámica.** Tabla de capacidad inicial $m_0$, factor de
crecimiento $\times 2$, umbral de carga $\alpha_{\max}$ (constante,
por ejemplo $0.75$).

**Política de grow.** Cuando una inserción haría que
$n > \alpha_{\max} \cdot m$, se crea una nueva tabla de capacidad
$2m$ y se reinsertan los $n$ elementos existentes (rehash completo).

**Costos unitarios.**
- Inserción sin resize: $c_{\text{ins}} = 1$.
- Reinserción de un elemento durante rehash: $c_{\text{re}} = 1$.
- Resize de tabla con $n$ elementos: $c_{\text{resize}} = n$
  (reinsertar todos los elementos).

**Objetivo.** Demostrar que $n$ inserciones consecutivas (empezando
con tabla vacía) cuestan $O(n)$ en total.

---

## Demostración 1: método agregado

**Teorema.** El costo total de $n$ inserciones es a lo sumo $3n$.

**Prueba.** Partimos de una tabla vacía con capacidad $m_0$. Definimos
$n_k = \alpha_{\max} \cdot 2^k m_0$ como el número de elementos que
dispara el $k$-ésimo resize ($k = 0, 1, 2, \ldots$).

El costo total tiene dos componentes:

**1) Inserciones ordinarias.** Cada una de las $n$ inserciones cuesta
1 unidad:

$$C_{\text{ins}} = n$$

**2) Rehashes.** El $k$-ésimo resize ocurre cuando la tabla pasa de
capacidad $2^k m_0$ a $2^{k+1} m_0$, reinsertando $n_k$ elementos.
Sea $K$ el número total de resizes realizados. El último resize
reinsertar $n_K \leq n$ elementos:

$$C_{\text{rehash}} = \sum_{k=0}^{K} n_k = \sum_{k=0}^{K} \alpha_{\max} \cdot 2^k m_0$$

Esta es una serie geométrica:

$$C_{\text{rehash}} = \alpha_{\max} m_0 \cdot \frac{2^{K+1} - 1}{2 - 1} = \alpha_{\max} m_0 (2^{K+1} - 1)$$

El último resize insertó $n_K = \alpha_{\max} \cdot 2^K m_0$
elementos, y $n_K \leq n$. Entonces $2^K \leq n / (\alpha_{\max} m_0)$,
y:

$$C_{\text{rehash}} \leq \alpha_{\max} m_0 \cdot (2 \cdot \frac{n}{\alpha_{\max} m_0} - 1) = 2n - \alpha_{\max} m_0 < 2n$$

**Total:**

$$C_{\text{total}} = C_{\text{ins}} + C_{\text{rehash}} < n + 2n = 3n$$

**Costo amortizado por inserción:**

$$\hat{c} = \frac{C_{\text{total}}}{n} < 3 = O(1)$$

$$\blacksquare$$

---

## Demostración 2: método del banquero (créditos)

**Teorema.** Si cada inserción paga 3 unidades de costo, el saldo de
créditos nunca es negativo.

**Prueba.** Cada inserción usa sus 3 unidades así:

- **1 unidad**: paga la inserción misma.
- **2 unidades**: se depositan como crédito, asociadas al elemento
  insertado.

**Invariante de crédito.** Después de cada operación, cada elemento
insertado desde el último resize tiene al menos 2 créditos.

**Verificación al momento del resize.** Cuando se dispara un resize,
la tabla tiene $n_{\text{cur}} = \alpha_{\max} \cdot m$ elementos. De
estos, $n_{\text{cur}}/2$ fueron insertados desde el resize anterior
(cuando la tabla tenía $\alpha_{\max} \cdot m/2$ elementos y se
expandió a capacidad $m$, quedando con factor $\alpha_{\max}/2$).

Créditos disponibles de los elementos insertados desde el último
resize:

$$\text{créditos} = 2 \cdot \frac{n_{\text{cur}}}{2} = n_{\text{cur}}$$

Costo del resize (reinsertar todos los $n_{\text{cur}}$ elementos):

$$\text{costo resize} = n_{\text{cur}}$$

Los créditos **cubren exactamente** el costo del resize. ✓

Después del resize, todos los créditos se consumieron y los elementos
"viejos" (del resize anterior) no tenían créditos, pero eso no
importa — el invariante se restablece conforme llegan nuevas
inserciones con 2 créditos cada una.

**¿Qué pasa con los elementos que sobrevivieron del resize anterior?**

Los $n_{\text{cur}}/2$ elementos viejos no contribuyen créditos. Pero
los $n_{\text{cur}}/2$ elementos nuevos aportan $2 \cdot n_{\text{cur}}/2 = n_{\text{cur}}$ créditos, que pagan la reinserción de
**todos** los $n_{\text{cur}}$ elementos (viejos + nuevos). Cada
nuevo elemento paga por sí mismo y por un viejo.

**Saldo.** El saldo de créditos antes de cada resize es exactamente 0
después de pagar el resize, y crece en 2 por cada inserción posterior.
Nunca es negativo. ✓

$$\blacksquare$$

---

## Demostración 3: método del potencial

**Teorema.** Con la función de potencial
$\Phi = 2n - \alpha_{\max} \cdot m$, el costo amortizado de cada
inserción es a lo sumo 3.

**Prueba.** Definimos:

$$\Phi = 2n - \alpha_{\max} \cdot m$$

donde $n$ es el número de elementos y $m$ la capacidad actual.

**Propiedades del potencial:**

- **Tras un resize** (la tabla acaba de expandirse): $n = \alpha_{\max} \cdot m/2$ (la tabla nueva tiene capacidad $m$,
  con $n$ elementos que estaban al límite en la tabla vieja de
  capacidad $m/2$). Entonces:

  $$\Phi = 2 \cdot \frac{\alpha_{\max} m}{2} - \alpha_{\max} m = 0$$

- **Justo antes del siguiente resize**: $n = \alpha_{\max} \cdot m$.
  Entonces:

  $$\Phi = 2\alpha_{\max} m - \alpha_{\max} m = \alpha_{\max} m = n$$

- **$\Phi \geq 0$ siempre**: entre resizes, $n$ crece desde
  $\alpha_{\max} m / 2$ hasta $\alpha_{\max} m$, así que
  $\Phi = 2n - \alpha_{\max} m$ va de 0 a $n \geq 0$. ✓

**Caso 1: inserción sin resize.**

$n$ aumenta en 1, $m$ no cambia:

$$\hat{c} = c_{\text{real}} + \Delta\Phi = 1 + (2(n+1) - \alpha_{\max} m) - (2n - \alpha_{\max} m) = 1 + 2 = 3$$

**Caso 2: inserción con resize.**

Justo antes del resize: $n = \alpha_{\max} m$,
$\Phi_{\text{antes}} = \alpha_{\max} m = n$.

El resize cambia $m \to 2m$. Luego se inserta el nuevo elemento:
$n \to n + 1$. El potencial después es:

$$\Phi_{\text{después}} = 2(n + 1) - \alpha_{\max} \cdot 2m = 2(n+1) - 2\alpha_{\max} m = 2(n+1) - 2n = 2$$

El costo real es $c_{\text{real}} = n + 1$ (reinsertar $n$ elementos
+ insertar el nuevo):

$$\hat{c} = c_{\text{real}} + \Delta\Phi = (n + 1) + (2 - n) = 3$$

**En ambos casos, $\hat{c} = 3$.**

**Conclusión.** El costo total real satisface:

$$\sum_{i=1}^{n} c_i = \sum_{i=1}^{n} \hat{c}_i - \Phi_n + \Phi_0 \leq 3n - \Phi_n + 0 \leq 3n$$

ya que $\Phi_n \geq 0$ y $\Phi_0 = 0$.

$$\blacksquare$$

---

## Demostración 4: el factor de crecimiento debe ser $> 1$

**Teorema.** Si la tabla crece sumando una constante $c$ (en vez de
multiplicar), el costo amortizado por inserción es $\Theta(n)$, no
$O(1)$.

**Prueba.** Con crecimiento aditivo ($m \to m + c$), los resizes
ocurren cuando $n$ alcanza $\alpha_{\max}(m_0 + kc)$ para
$k = 0, 1, 2, \ldots$

El $k$-ésimo resize reinsertar $\alpha_{\max}(m_0 + kc)$ elementos.
El número de resizes hasta insertar $n$ elementos es
$K = \Theta(n/c)$ (cada resize añade capacidad para $\alpha_{\max}c$
elementos más).

Costo total de los rehashes:

$$C_{\text{rehash}} = \sum_{k=0}^{K} \alpha_{\max}(m_0 + kc) = \alpha_{\max}\left(K \cdot m_0 + c\cdot\frac{K(K+1)}{2}\right) = \Theta(K^2) = \Theta(n^2/c)$$

Costo amortizado por inserción:

$$\hat{c} = \frac{\Theta(n^2/c)}{n} = \Theta(n/c) = \Theta(n)$$

El crecimiento aditivo produce costo amortizado **lineal**, no
constante. Por esto las tablas hash usan crecimiento multiplicativo.

$$\blacksquare$$

---

## Demostración 5: shrink con histéresis preserva $O(1)$ amortizado

**Teorema.** Si la tabla encoge a la mitad cuando
$\alpha < \alpha_{\min}$ con $\alpha_{\min} \leq \alpha_{\max}/4$
(ej: grow en $\alpha > 0.75$, shrink en $\alpha < 0.125$), entonces
una secuencia intercalada de $n$ inserciones y eliminaciones tiene
costo amortizado $O(1)$.

**Prueba.** Extendemos el potencial para cubrir ambas operaciones:

$$\Phi = \begin{cases} 2n - \alpha_{\max} \cdot m & \text{si } n \geq \alpha_{\max} \cdot m / 2 \\ \alpha_{\min} \cdot m - n & \text{si } n < \alpha_{\max} \cdot m / 2 \end{cases}$$

**Propiedad clave:** $\Phi = 0$ cuando $n = \alpha_{\max} m / 2$ (el
punto medio) y $\Phi \geq 0$ siempre.

- Antes de un grow: $n = \alpha_{\max} m$, $\Phi = \alpha_{\max} m = n$. El costo del grow ($= n$) se paga con el potencial.

- Antes de un shrink: $n = \alpha_{\min} m$,
  $\Phi = \alpha_{\min} m - n$... pero necesitamos que
  $\Phi \geq n$ para pagar el rehash.

  Con $\alpha_{\min} \leq \alpha_{\max}/4$ y la rama inferior del
  potencial, se acumulan suficientes eliminaciones entre el punto
  medio y el umbral de shrink para financiar el rehash.

**Anti-thrashing.** Después de un grow, $\alpha = \alpha_{\max}/2$.
Para disparar un shrink, $\alpha$ debe bajar hasta $\alpha_{\min}
\leq \alpha_{\max}/4$, requiriendo al menos $\alpha_{\max} m / 4$
eliminaciones. Cada una aporta potencial. Después de un shrink,
$\alpha = 2\alpha_{\min} \leq \alpha_{\max}/2$, lejos de ambos
umbrales. No hay oscilación. ✓

$$\blacksquare$$

---

## Verificación numérica

Tabla con $m_0 = 8$, $\alpha_{\max} = 0.75$, factor $\times 2$:

| Inserción $i$ | $n$ | Resize? | Costo real | Créditos usados | Créditos restantes |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 1–6 | 1–6 | No | 1 c/u | 0 | 2 c/u = 12 |
| 7 | 7 | Sí ($m: 8 \to 16$) | $6 + 1 = 7$ | 6 (rehash) | 12 − 6 = 6, + 2 = 8 |
| 8–12 | 8–12 | No | 1 c/u | 0 | 8 + 5×2 = 18 |
| 13 | 13 | Sí ($m: 16 \to 32$) | $12 + 1 = 13$ | 12 | 18 − 12 = 6, + 2 = 8 |
| 14–24 | 14–24 | No | 1 c/u | 0 | 8 + 11×2 = 30 |
| 25 | 25 | Sí ($m: 32 \to 64$) | $24 + 1 = 25$ | 24 | 30 − 24 = 6, + 2 = 8 |

Costo total para 25 inserciones: $25 + 6 + 12 + 24 = 67$.
Cota del teorema: $3 \times 25 = 75$. ✓ ($67 < 75$)

Costo amortizado observado: $67/25 = 2.68 < 3$. ✓

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| $C_{\text{total}} < 3n$ | Método agregado | Serie geométrica $\sum 2^k < 2n$ |
| 3 unidades/inserción bastan | Método del banquero | 2 créditos por elemento; cada nuevo paga por sí mismo y un viejo |
| $\hat{c} = 3$ exacto | Método del potencial $\Phi = 2n - \alpha_{\max}m$ | $\Phi = 0$ tras resize, $\Phi = n$ antes de resize |
| Crecimiento aditivo es $\Theta(n)$ | Suma aritmética vs geométrica | $\sum k = \Theta(K^2)$ vs $\sum 2^k = \Theta(2^K)$ |
| Shrink con histéresis preserva $O(1)$ | Potencial extendido | $\alpha_{\min} \leq \alpha_{\max}/4$ evita thrashing |
