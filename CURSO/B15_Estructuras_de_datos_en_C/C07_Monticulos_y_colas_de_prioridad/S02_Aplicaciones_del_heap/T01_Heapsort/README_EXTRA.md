# T01 — Demostración formal: correctitud del heapsort

> Complemento riguroso del README.md. Aquí se prueba formalmente, usando
> invariantes de bucle, que el heapsort produce un array ordenado.

---

## El algoritmo

Referencia del código cuya correctitud probamos:

```c
void heapsort(int arr[], int n) {
    // Fase 1: build max-heap
    for (int i = n / 2 - 1; i >= 0; i--)        // bucle B
        sift_down(arr, n, i);

    // Fase 2: extraer máximo
    for (int i = n - 1; i > 0; i--) {            // bucle E
        swap(&arr[0], &arr[i]);
        sift_down(arr, i, 0);
    }
}
```

La prueba se divide en tres partes: correctitud de la Fase 1 (build),
correctitud de la Fase 2 (extract), y complejidad.

---

## Definiciones

**Definición (Max-heap en subrango).** Decimos que `arr[0..m)` es un
max-heap si para todo $0 \leq j < m$:

```
Si 2j+1 < m:  arr[j] ≥ arr[2j+1]   (padre ≥ hijo izquierdo)
Si 2j+2 < m:  arr[j] ≥ arr[2j+2]   (padre ≥ hijo derecho)
```

**Definición (Partición heap-sorted).** Decimos que el array tiene una
partición $(i)$ si:

```
(P1)  arr[0..i+1) es un max-heap de tamaño i+1.
(P2)  arr[i+1..n) contiene los n−(i+1) mayores elementos del array
      original, en orden ascendente.
(P3)  Todo elemento en arr[0..i+1) es ≤ todo elemento en arr[i+1..n).
```

---

## Demostración 1: correctitud de la Fase 1 (build-heap)

**Teorema.** Al terminar el bucle B, `arr[0..n)` es un max-heap válido.

### Invariante del bucle B

```
INV_B(i): Para todo j con i < j < n, el nodo j es raíz de un
          max-heap válido dentro de arr[0..n).
```

Es decir, cada subárbol enraizado en un nodo con índice $> i$ ya
satisface la propiedad de max-heap.

### Inicialización ($i = \lfloor n/2 \rfloor - 1$, antes de la primera iteración)

Los nodos con índice $j \geq \lfloor n/2 \rfloor$ son hojas. Una hoja
es trivialmente un max-heap de tamaño 1 (no tiene hijos que violen la
propiedad).

Para todo $j > \lfloor n/2 \rfloor - 1$: el nodo $j$ es hoja → raíz
de un max-heap válido. ✓

### Mantenimiento (iteración $i$)

Al inicio de la iteración, $\text{INV\_B}(i)$ dice que todos los nodos
con índice $> i$ son raíces de max-heaps válidos. En particular, los
hijos de $i$ (índices $2i+1$ y $2i+2$, si existen) son raíces de
max-heaps válidos.

Se ejecuta `sift_down(arr, n, i)`. La operación sift-down tiene la
precondición de que ambos subárboles del nodo son max-heaps, que
se cumple por el invariante. Después de sift-down, el subárbol
enraizado en $i$ es un max-heap válido.

Al decrementar $i$ a $i-1$, el invariante $\text{INV\_B}(i-1)$ dice
que todos los nodos con índice $> i - 1$ (es decir, $\geq i$) son
raíces de max-heaps. Como acabamos de reparar $i$, esto se cumple. ✓

### Terminación ($i = -1$)

El bucle termina cuando $i < 0$. El invariante $\text{INV\_B}(-1)$
dice que todos los nodos con índice $> -1$ (es decir, todos los nodos
$0, 1, \ldots, n-1$) son raíces de max-heaps válidos. En particular,
el nodo 0 (la raíz) es raíz de un max-heap válido sobre `arr[0..n)`.

$$\therefore \text{arr[0..n) es un max-heap al terminar la Fase 1.} \qquad \blacksquare$$

---

## Demostración 2: correctitud de la Fase 2 (extract)

**Teorema.** Al terminar el bucle E, `arr[0..n)` está ordenado en
orden ascendente.

### Invariante del bucle E

```
INV_E(i): Al inicio de cada iteración con valor i:
  (E1)  arr[0..i+1) es un max-heap de tamaño i+1.
  (E2)  arr[i+1..n) contiene los n−(i+1) mayores elementos del array
        original, en orden ascendente.
  (E3)  Todo elemento en arr[0..i+1) es ≤ todo elemento en arr[i+1..n).
```

### Inicialización ($i = n - 1$, antes de la primera iteración)

- **(E1)**: `arr[0..n)` es un max-heap de tamaño $n$ (por la Fase 1). ✓
- **(E2)**: `arr[n..n)` es vacío. La condición sobre el subrango vacío
  se satisface trivialmente. ✓
- **(E3)**: No hay elementos en `arr[n..n)`, así que la condición se
  satisface trivialmente. ✓

### Mantenimiento (iteración $i$)

Asumimos $\text{INV\_E}(i)$. La iteración ejecuta:

**Paso 1: `swap(&arr[0], &arr[i])`.**

Por (E1), `arr[0]` es el máximo de `arr[0..i+1)` (propiedad del
max-heap: la raíz es el mayor). Por (E3), este máximo es $\leq$ todos
los elementos en `arr[i+1..n)`.

Después del swap:
- `arr[i]` contiene el antiguo máximo de `arr[0..i+1)`.
- `arr[0]` contiene el antiguo `arr[i]`.

**Paso 2: `sift_down(arr, i, 0)`.**

Después del swap, `arr[0..i)` puede no ser un max-heap (porque `arr[0]`
fue reemplazado). Sin embargo, los subárboles de la raíz (enraizados en
índices 1 y 2) siguen siendo max-heaps válidos: el swap solo afectó a
`arr[0]` e `arr[i]`, y el nodo `arr[i]` ya no es parte de la zona de
heap `[0..i)`.

La precondición de sift-down (ambos subárboles de la raíz son
max-heaps) se cumple. Después de `sift_down(arr, i, 0)`, `arr[0..i)`
es un max-heap de tamaño $i$.

**Verificación del invariante para $i - 1$:**

- **(E1)**: `arr[0..i)` = `arr[0..(i-1)+1)` es un max-heap de
  tamaño $i = (i-1) + 1$. ✓
- **(E2)**: `arr[i..n)` contiene el antiguo máximo de `arr[0..i+1)`
  (ahora en `arr[i]`) seguido de `arr[i+1..n)` (que por hipótesis
  contenía los $n - i - 1$ mayores en orden). El antiguo máximo de
  `arr[0..i+1)` es, por (E3), $\leq$ todos los elementos en
  `arr[i+1..n)`, y es $\geq$ todos los de `arr[0..i+1)`. Así,
  `arr[i]` $\leq$ `arr[i+1]`, y `arr[i..n)` contiene los $n - i$
  mayores en orden ascendente. ✓
- **(E3)**: El máximo de `arr[0..i)` (que es `arr[0]` tras sift-down)
  era un elemento de `arr[0..i+1)` antes del swap. Por (E3) del paso
  anterior, era $\leq$ todos los de `arr[i+1..n)`. Como `arr[i]` es
  el antiguo máximo de `arr[0..i+1)` y es $\geq$ todos los demás
  en `arr[0..i+1)$, tenemos `arr[0]` $\leq$ `arr[i]` $\leq$
  `arr[i+1]`. ✓

### Terminación ($i = 0$)

El bucle termina cuando $i = 0$ (la condición $i > 0$ falla). El
invariante $\text{INV\_E}(0)$ dice:

- (E1): `arr[0..1)` es un max-heap de tamaño 1 (trivialmente cierto).
- (E2): `arr[1..n)` contiene los $n - 1$ mayores elementos en orden
  ascendente.
- (E3): `arr[0]` $\leq$ todos los elementos en `arr[1..n)`.

Combinando: `arr[0]` es el menor elemento, y `arr[1..n)` son los
$n - 1$ mayores en orden ascendente. Por lo tanto, `arr[0..n)` está
completamente ordenado en orden ascendente.

$$\therefore \text{heapsort produce un array ordenado.} \qquad \blacksquare$$

---

## Demostración 3: complejidad $O(n \log n)$

**Teorema.** Heapsort tiene complejidad $O(n \log n)$ en el peor caso
y $\Omega(n \log n)$ en el peor caso.

**Prueba.**

*Cota superior.* La Fase 1 (build-heap) cuesta $O(n)$ (demostrado en
T04/README_EXTRA). La Fase 2 ejecuta $n - 1$ extracciones, cada una
con un swap $O(1)$ y un sift-down $O(\log i)$ (la zona de heap tiene
tamaño $i$):

$$T_2 = \sum_{i=1}^{n-1} O(\log i) \leq \sum_{i=1}^{n-1} O(\log n) = O(n \log n)$$

Total: $O(n) + O(n \log n) = O(n \log n)$.

*Cota inferior.* Heapsort es un algoritmo de ordenamiento basado en
comparaciones. Por la cota inferior de $\Omega(n \log n)$ para
ordenamiento por comparación (demostrada en C08/README_EXTRA), todo
algoritmo de este tipo requiere $\Omega(n \log n)$ comparaciones en
el peor caso.

Combinando: heapsort es $\Theta(n \log n)$ en el peor caso.

$$\blacksquare$$

**Corolario (complejidad en todos los casos).**

| Caso | Complejidad | Razón |
|------|-------------|-------|
| Mejor | $O(n \log n)$ | Incluso si el array ya está ordenado, la Fase 2 hace $n-1$ sift-downs |
| Promedio | $O(n \log n)$ | Los sift-downs en la Fase 2 descienden $\Theta(\log n)$ niveles en promedio |
| Peor | $O(n \log n)$ | Cada sift-down desciende a lo sumo $\lfloor \log_2 n \rfloor$ niveles |

Heapsort es uno de los pocos algoritmos con la **misma** complejidad
asintótica en los tres casos.

---

## Demostración 4: heapsort es in-place

**Teorema.** Heapsort usa $O(1)$ memoria auxiliar.

**Prueba.** El algoritmo opera sobre el array de entrada sin alocar
estructuras adicionales. Las únicas variables auxiliares son:

- Índices del bucle ($i$, `index`, `left`, `right`, `largest`): $O(1)$.
- Variable temporal para swap: $O(1)$.

No se usan arrays auxiliares ni recursión (sift-down es iterativo).
La versión recursiva de sift-down usaría $O(\log n)$ stack, pero la
versión iterativa (con `while`) es $O(1)$.

$$\therefore \text{Heapsort usa } O(1) \text{ memoria auxiliar.} \qquad \blacksquare$$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|---------|-------|
| Fase 1 produce max-heap | Invariante de bucle B | Hojas son heaps triviales, sift-down repara subárboles |
| Fase 2 produce array ordenado | Invariante de bucle E (3 cláusulas) | Máximo del heap va a la zona ordenada |
| Complejidad $\Theta(n \log n)$ | Suma + cota inferior | $O(n) + O(n \log n)$; $\Omega(n \log n)$ por comparaciones |
| In-place $O(1)$ | Análisis de variables | Solo índices y temporal de swap |
