# T02 — Demostración formal: la inserción preserva la propiedad BST

> Complemento riguroso del README.md. Aquí se prueba formalmente, usando
> invariantes de bucle e inducción estructural, que la inserción en un
> BST preserva la propiedad de orden.

---

## Definiciones formales

**Definición (Propiedad BST).** Un árbol binario $T$ satisface la
propiedad BST si para todo nodo $x$ en $T$:

```
∀ y ∈ subárbol_izquierdo(x):  key(y) < key(x)
∀ z ∈ subárbol_derecho(x):    key(z) > key(x)
```

Equivalentemente, cada nodo $x$ tiene un **rango válido** $(lo, hi)$
tal que $lo < \text{key}(x) < hi$, y los subárboles heredan rangos
restringidos:

```
nodo x con rango (lo, hi):
  hijo izquierdo tiene rango (lo, key(x))
  hijo derecho tiene rango (key(x), hi)
```

La raíz tiene rango $(-\infty, +\infty)$.

**Definición (BST válido).** Un árbol $T$ es un BST válido si:
- $T$ es vacío (NULL), o
- $T$ satisface la propiedad BST y el recorrido inorden produce una
  secuencia estrictamente creciente.

**Nota.** Asumimos claves únicas (no se insertan duplicados). La
inserción ignora valores ya presentes.

---

## Demostración 1: inserción iterativa (invariante de bucle)

Consideramos la inserción iterativa con puntero a puntero:

```c
void bst_insert_pp(TreeNode **root, int value) {
    while (*root) {                          // L1
        if (value < (*root)->data)           // L2
            root = &(*root)->left;           // L3
        else if (value > (*root)->data)      // L4
            root = &(*root)->right;          // L5
        else                                 // L6
            return;  // duplicado            // L7
    }
    *root = create_node(value);              // L8
}
```

**Teorema.** Si el árbol $T$ es un BST válido antes de llamar a
`bst_insert_pp(&T, v)`, entonces es un BST válido después.

### Invariante de bucle

Definimos el invariante con tres cláusulas:

```
INV(root, lo, hi):
  (I1)  *root es un subárbol del BST original T.
  (I2)  lo < v < hi, donde (lo, hi) es el rango válido de la
        posición a la que apunta root.
  (I3)  Los nodos del BST original no han sido modificados.
```

Intuitivamente: `root` siempre apunta al puntero correcto donde $v$
debería residir si existiera en el árbol, y $v$ cae dentro del rango
válido para esa posición.

### Inicialización (antes de la primera iteración)

Al entrar al bucle, `root = &T` (el puntero a la raíz del árbol).

- **(I1)**: `*root = T`, que es el árbol completo. ✓
- **(I2)**: El rango de la raíz es $(-\infty, +\infty)$. Como $v$ es un
  entero, $-\infty < v < +\infty$. ✓
- **(I3)**: No se ha ejecutado ninguna operación. ✓

### Mantenimiento (si el bucle continúa)

Supongamos que `INV(root, lo, hi)` vale al inicio de una iteración, y
que `*root` no es NULL (la condición del while). Sea $x = \text{*root}$
con clave $k = x\text{->data}$. Por (I2), $lo < v < hi$. Hay tres
subcasos:

**Caso A: $v < k$** (líneas L2-L3).

Se ejecuta `root = &(x->left)`. El nuevo rango es $(lo, k)$.

- **(I1)**: `*root` ahora es `x->left`, que es un subárbol del BST. ✓
- **(I2)**: Por hipótesis, $lo < v$. Como $v < k$, tenemos
  $lo < v < k$, que es el rango $(lo, k)$. ✓
- **(I3)**: Solo se reasigna el puntero local `root`; ningún nodo se
  modifica. ✓

**Caso B: $v > k$** (líneas L4-L5).

Se ejecuta `root = &(x->right)`. El nuevo rango es $(k, hi)$.

- **(I1)**: `*root` ahora es `x->right`, subárbol del BST. ✓
- **(I2)**: Por hipótesis, $v < hi$. Como $v > k$, tenemos
  $k < v < hi$, que es el rango $(k, hi)$. ✓
- **(I3)**: Solo se reasigna `root`. ✓

**Caso C: $v = k$** (líneas L6-L7).

Se ejecuta `return` — el bucle termina sin modificar el árbol.
El BST permanece inalterado y válido. ✓

En los casos A y B, el invariante se preserva con un rango estrictamente
más ajustado.

### Terminación (cuando el bucle termina normalmente)

El bucle termina cuando `*root == NULL`. En ese punto:

- Por (I1): `root` apunta a un campo `left` o `right` de algún nodo
  ancestro (o al puntero raíz si el árbol estaba vacío).
- Por (I2): $lo < v < hi$, es decir, $v$ está en el rango correcto
  para esta posición.
- Por (I3): ningún nodo existente ha sido modificado.

Se ejecuta `*root = create_node(v)` (línea L8), que crea una hoja con
clave $v$ y la enlaza en la posición apuntada por `root`.

### Prueba de que el resultado es un BST válido

Debemos verificar que después de insertar la hoja con clave $v$:

**1. La hoja satisface la propiedad BST.**

La nueva hoja tiene subárboles vacíos (NULL). Un nodo hoja sin hijos
satisface trivialmente la propiedad BST, ya que no hay nodos en sus
subárboles que violen la condición. ✓

**2. El padre de la hoja sigue satisfaciendo la propiedad BST.**

Sea $p$ el nodo padre (cuyo campo `left` o `right` apuntaba a NULL y
ahora apunta a la hoja $v$). Por el invariante (I2), $v$ está en el
rango $(lo, hi)$ donde fue insertado:

- Si la hoja se insertó como hijo izquierdo de $p$: se llegó ahí porque
  $v < \text{key}(p)$. Todos los nodos en el subárbol izquierdo de $p$
  (solo la hoja $v$) satisfacen $\text{key} < \text{key}(p)$. ✓
- Si se insertó como hijo derecho: $v > \text{key}(p)$. ✓

**3. Los ancestros siguen satisfaciendo la propiedad BST.**

Por (I3), ningún nodo existente fue modificado — solo se asignó un
puntero NULL. Las relaciones de orden entre nodos existentes no cambian.

Para cada ancestro $a$ de la hoja: la hoja $v$ pertenece al subárbol
izquierdo o derecho de $a$. El invariante (I2) garantiza que $v$ está
dentro del rango heredado de $a$:

- Si la ruta de la raíz a la hoja pasó por el hijo izquierdo de $a$
  (porque $v < \text{key}(a)$), entonces $v < \text{key}(a)$, y $v$
  es un nodo legítimo del subárbol izquierdo de $a$. ✓
- Análogamente para el hijo derecho. ✓

**4. Nodos no ancestrales no se ven afectados.**

Ningún nodo fuera del camino raíz→hoja fue tocado. Sus subárboles
no contienen la nueva hoja, por lo que sus propiedades BST son
idénticas a las del árbol original. ✓

$$\therefore \text{El árbol resultante es un BST válido.} \qquad \blacksquare$$

### Terminación del bucle

**Lema.** El bucle termina en a lo sumo $h + 1$ iteraciones, donde
$h$ es la altura del árbol.

**Prueba.** En cada iteración, `root` desciende un nivel: pasa de
apuntar a un nodo de profundidad $d$ a uno de profundidad $d + 1$ (o a
NULL). La profundidad máxima es $h$, y después de $h$ descensos se
alcanza una hoja, cuyo hijo es NULL. Por lo tanto, el bucle ejecuta
como máximo $h + 1$ iteraciones (el $+1$ por la última iteración que
detecta NULL). $\square$

---

## Demostración 2: inserción recursiva (inducción estructural)

Consideramos la inserción recursiva:

```c
TreeNode *bst_insert(TreeNode *root, int value) {
    if (!root) return create_node(value);
    if (value < root->data)
        root->left = bst_insert(root->left, value);
    else if (value > root->data)
        root->right = bst_insert(root->right, value);
    return root;
}
```

**Teorema.** Si $T$ es un BST válido con rango $(lo, hi)$ y
$lo < v < hi$, entonces `bst_insert(T, v)` retorna un BST válido
con rango $(lo, hi)$.

**Prueba (por inducción estructural sobre $T$).**

**Base ($T = $ NULL).** `bst_insert(NULL, v)` retorna un nodo hoja con
clave $v$. Un nodo sin hijos satisface la propiedad BST trivialmente.
Su rango es $(lo, hi)$ con $lo < v < hi$. ✓

**Paso inductivo ($T$ no vacío).** Sea $T$ un árbol con raíz $r$ de
clave $k$ y rango $(lo, hi)$.

**Hipótesis inductiva**: Para cualquier subárbol $S$ de $T$ con rango
$(lo', hi')$, si $lo' < v < hi'$, entonces `bst_insert(S, v)` retorna
un BST válido con rango $(lo', hi')$.

Hay tres casos:

**Caso $v < k$:**

Se ejecuta `root->left = bst_insert(root->left, v)`.

El subárbol izquierdo tiene rango $(lo, k)$. Como $v < k$ y
$lo < v$ (dado), tenemos $lo < v < k$. Por hipótesis inductiva,
`bst_insert(root->left, v)` retorna un BST válido con rango $(lo, k)$.

El subárbol derecho no se modifica. La raíz $r$ tiene:
- Subárbol izquierdo: BST válido con todas las claves en $(lo, k)$. ✓
- Subárbol derecho: sin cambios, BST válido con claves en $(k, hi)$. ✓
- Clave $k$ con $lo < k < hi$. ✓

**Caso $v > k$:** Simétrico al anterior, con el subárbol derecho de
rango $(k, hi)$ y $k < v < hi$. ✓

**Caso $v = k$:** Se retorna `root` sin modificación. El BST permanece
válido. ✓

Por inducción estructural, la propiedad vale para todo árbol $T$.

$$\therefore \text{bst\_insert preserva la propiedad BST.} \qquad \blacksquare$$

---

## Demostración 3: el inorden después de la inserción es correcto

**Teorema.** Sea $T$ un BST válido con recorrido inorden
$[a_1, a_2, \ldots, a_n]$ (secuencia estrictamente creciente). Después
de insertar $v \notin T$, el inorden del nuevo árbol es
$[a_1, \ldots, a_j, v, a_{j+1}, \ldots, a_n]$ donde $a_j < v < a_{j+1}$
(la posición ordenada de $v$).

**Prueba.** La inserción coloca $v$ como hoja. En el recorrido inorden,
una hoja aparece entre su antecesor inorden y su sucesor inorden.
Probamos que esos son exactamente $a_j$ y $a_{j+1}$.

Consideremos el camino de la raíz a la nueva hoja $v$. En cada nodo
del camino, si fuimos a la izquierda (porque $v < k$), entonces $k$ es
un candidato a sucesor inorden de $v$. Si fuimos a la derecha (porque
$v > k$), entonces $k$ es un candidato a antecesor inorden de $v$.

El antecesor inorden de $v$ es el **último** nodo en el camino donde
fuimos a la derecha (el nodo más profundo con $k < v$). El sucesor
inorden es el **último** nodo donde fuimos a la izquierda.

Por la propiedad BST del árbol original:
- Todos los nodos con clave $< v$ están "a la izquierda" de $v$ en
  inorden, y el más cercano es el antecesor.
- Todos los nodos con clave $> v$ están "a la derecha", y el más
  cercano es el sucesor.

Como $v$ se inserta como hoja sin hijos, no altera el orden relativo
de los nodos existentes. El inorden del nuevo árbol es el inorden
anterior con $v$ insertado en su posición ordenada.

$$\blacksquare$$

---

## Resumen de técnicas

| Qué se demuestra | Técnica | Componentes |
|-------------------|---------|-------------|
| Inserción iterativa preserva BST | Invariante de bucle | Inicialización, mantenimiento (3 casos), terminación |
| Inserción recursiva preserva BST | Inducción estructural | Base (NULL), paso (3 casos por comparación) |
| Inorden correcto tras inserción | Argumento sobre antecesor/sucesor inorden | Propiedad del camino raíz→hoja |
| Terminación del bucle | Función de decrecimiento | Profundidad estrictamente creciente, acotada por $h$ |
