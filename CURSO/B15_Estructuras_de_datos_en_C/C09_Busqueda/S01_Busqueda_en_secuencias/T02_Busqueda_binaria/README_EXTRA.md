# T02 — Demostración formal: correctitud y terminación de búsqueda binaria

> Complemento riguroso del README.md. Aquí se prueba formalmente, usando
> invariante de bucle, que la búsqueda binaria es correcta, y se
> demuestra su terminación mediante una función de decrecimiento.

---

## El algoritmo

```c
int binary_search(int arr[], int n, int target) {
    int lo = 0;                              // S1
    int hi = n - 1;                          // S2
    while (lo <= hi) {                       // L1
        int mid = lo + (hi - lo) / 2;        // L2
        if (arr[mid] == target)               // L3
            return mid;                       // L4
        if (arr[mid] < target)                // L5
            lo = mid + 1;                     // L6
        else                                  // L7
            hi = mid - 1;                     // L8
    }
    return -1;                               // S3
}
```

**Precondición.** `arr[0..n)` está ordenado en orden no decreciente:
$\text{arr}[0] \leq \text{arr}[1] \leq \ldots \leq \text{arr}[n-1]$.

**Postcondición.** El valor retornado es:
- Un índice $m$ tal que $\text{arr}[m] = \text{target}$, si tal índice
  existe.
- $-1$ si $\text{target}$ no aparece en `arr[0..n)`.

---

## Demostración 1: correctitud (invariante de bucle)

### Invariante

```
INV(lo, hi): Si target existe en arr[0..n), entonces
             target ∈ arr[lo..hi]  (es decir, ∃ k con lo ≤ k ≤ hi
             tal que arr[k] = target).
```

Equivalentemente: si $\text{target} \notin \text{arr}[\text{lo}..\text{hi}]$,
entonces $\text{target} \notin \text{arr}[0..n)$.

### Inicialización (antes de la primera iteración)

Después de S1-S2: $\text{lo} = 0$, $\text{hi} = n - 1$.

El rango $[\text{lo}, \text{hi}] = [0, n-1]$ es todo el array. Si
$\text{target}$ existe en el array, ciertamente está en
$\text{arr}[0..n-1]$. ✓

### Mantenimiento (si el bucle continúa)

Asumimos $\text{INV}(\text{lo}, \text{hi})$ al inicio de una iteración
con $\text{lo} \leq \text{hi}$.

Se calcula $\text{mid} = \text{lo} + \lfloor(\text{hi} - \text{lo})/2\rfloor$.

**Observación**: $\text{lo} \leq \text{mid} \leq \text{hi}$ (porque
$0 \leq \lfloor(\text{hi} - \text{lo})/2\rfloor \leq \text{hi} - \text{lo}$).

Hay tres casos:

**Caso A: $\text{arr}[\text{mid}] = \text{target}$ (L3-L4).**

Se retorna $\text{mid}$. La postcondición se satisface directamente:
$\text{arr}[\text{mid}] = \text{target}$ y $0 \leq \text{mid} < n$. ✓

**Caso B: $\text{arr}[\text{mid}] < \text{target}$ (L5-L6).**

Se actualiza $\text{lo} = \text{mid} + 1$. Debemos probar
$\text{INV}(\text{mid} + 1, \text{hi})$.

Como el array está ordenado y $\text{arr}[\text{mid}] < \text{target}$:

$$\text{arr}[j] \leq \text{arr}[\text{mid}] < \text{target} \quad \forall\, j \leq \text{mid}$$

Ningún elemento en $\text{arr}[0..\text{mid}]$ puede ser igual a
$\text{target}$. Si $\text{target}$ existe en el array, debe estar en
$\text{arr}[\text{mid}+1..\text{hi}]$.

$\text{INV}(\text{mid}+1, \text{hi})$ vale. ✓

**Caso C: $\text{arr}[\text{mid}] > \text{target}$ (L7-L8).**

Se actualiza $\text{hi} = \text{mid} - 1$. Debemos probar
$\text{INV}(\text{lo}, \text{mid} - 1)$.

Como el array está ordenado y $\text{arr}[\text{mid}] > \text{target}$:

$$\text{arr}[j] \geq \text{arr}[\text{mid}] > \text{target} \quad \forall\, j \geq \text{mid}$$

Ningún elemento en $\text{arr}[\text{mid}..\text{hi}]$ puede ser igual
a $\text{target}$. Si $\text{target}$ existe, debe estar en
$\text{arr}[\text{lo}..\text{mid}-1]$.

$\text{INV}(\text{lo}, \text{mid}-1)$ vale. ✓

### Terminación (cuando el bucle termina: $\text{lo} > \text{hi}$)

El bucle termina con $\text{lo} > \text{hi}$. El rango
$[\text{lo}, \text{hi}]$ es vacío.

Por el invariante, si $\text{target}$ existiera en el array, estaría
en $\text{arr}[\text{lo}..\text{hi}]$. Pero ese rango es vacío, así
que $\text{target}$ no existe en el array. Retornar $-1$ es correcto. ✓

$$\therefore \text{binary\_search es correcta.} \qquad \blacksquare$$

---

## Demostración 2: terminación (función de decrecimiento)

**Teorema.** El bucle de búsqueda binaria termina en a lo sumo
$\lfloor \log_2 n \rfloor + 1$ iteraciones.

**Prueba.** Definimos la función de decrecimiento (variant):

$$V = \text{hi} - \text{lo} + 1$$

que es el tamaño del rango de búsqueda.

**Cota inferior.** El bucle solo ejecuta si $\text{lo} \leq \text{hi}$,
es decir, $V \geq 1$. Cuando el bucle termina, $V \leq 0$.

**Decrecimiento estricto.** En cada iteración (que no retorna en L4),
$V$ decrece estrictamente:

*Caso B* ($\text{lo} = \text{mid} + 1$):

$$V' = \text{hi} - (\text{mid} + 1) + 1 = \text{hi} - \text{mid}$$

Como $\text{mid} \geq \text{lo}$:

$$V' = \text{hi} - \text{mid} \leq \text{hi} - \text{lo} = V - 1 < V$$

Más precisamente: $\text{mid} = \text{lo} + \lfloor(\text{hi}-\text{lo})/2\rfloor$,
así que:

$$V' = \text{hi} - \text{lo} - \lfloor(\text{hi}-\text{lo})/2\rfloor = \lceil(\text{hi}-\text{lo})/2\rceil \leq \lfloor V/2 \rfloor$$

*Caso C* ($\text{hi} = \text{mid} - 1$):

$$V' = (\text{mid} - 1) - \text{lo} + 1 = \text{mid} - \text{lo} = \lfloor(\text{hi}-\text{lo})/2\rfloor \leq \lfloor V/2 \rfloor$$

**En ambos casos: $V' \leq \lfloor V/2 \rfloor$.**

El rango se reduce a la mitad (o menos) en cada iteración.

**Número de iteraciones.** Partiendo de $V_0 = n$ y reduciéndose al
menos a la mitad:

$$V_k \leq \left\lfloor \frac{n}{2^k} \right\rfloor$$

El bucle termina cuando $V_k \leq 0$, lo que ocurre cuando
$\lfloor n/2^k \rfloor = 0$, es decir, $2^k > n$:

$$k > \log_2 n \implies k \geq \lfloor \log_2 n \rfloor + 1$$

El bucle ejecuta a lo sumo $\lfloor \log_2 n \rfloor + 1$ iteraciones.

$$\therefore \text{La búsqueda binaria termina en } O(\log n) \text{ pasos.} \qquad \blacksquare$$

---

## Demostración 3: complejidad $\Theta(\log n)$ en el peor caso

**Teorema.** La búsqueda binaria ejecuta $\Theta(\log n)$ comparaciones
en el peor caso.

**Prueba.**

*Cota superior.* Demostrada arriba: a lo sumo $\lfloor\log_2 n\rfloor + 1$
iteraciones, cada una con $O(1)$ comparaciones.

*Cota inferior.* Cualquier algoritmo de búsqueda basado en comparaciones
en un array ordenado de $n$ elementos necesita $\Omega(\log n)$
comparaciones en el peor caso.

Argumento por árbol de decisión: cada comparación tiene 3 resultados
posibles ($<$, $=$, $>$), produciendo un árbol ternario. Hay $n$
resultados posibles (el elemento está en la posición 0, 1, ..., $n-1$)
más 1 resultado "no encontrado", dando $n + 1$ hojas. Un árbol ternario
de altura $h$ tiene a lo sumo $3^h$ hojas:

$$n + 1 \leq 3^h \implies h \geq \log_3(n+1) = \Omega(\log n)$$

Combinando: la búsqueda binaria es $\Theta(\log n)$ en el peor caso,
y por lo tanto es asintóticamente óptima.

$$\blacksquare$$

---

## Demostración 4: correctitud del cálculo de `mid`

**Lema.** Si $0 \leq \text{lo} \leq \text{hi} < 2^{31}$, entonces
$\text{mid} = \text{lo} + (\text{hi} - \text{lo})/2$ no produce
overflow en aritmética de enteros de 32 bits con signo.

**Prueba.**

1. $\text{hi} - \text{lo} \geq 0$ (porque $\text{lo} \leq \text{hi}$).
   No hay underflow.

2. $\text{hi} - \text{lo} \leq \text{hi} < 2^{31}$. La diferencia cabe
   en un `int`.

3. $(\text{hi} - \text{lo})/2 \leq \text{hi}/2 < 2^{30}$. La división
   reduce el valor.

4. $\text{lo} + (\text{hi} - \text{lo})/2 \leq \text{lo} + (\text{hi} - \text{lo}) = \text{hi} < 2^{31}$.
   La suma no excede `hi`, que cabe en un `int`.

**Contraste con la versión con bug:**

$\text{mid} = (\text{lo} + \text{hi})/2$: si $\text{lo} = \text{hi} = 2^{30}$,
entonces $\text{lo} + \text{hi} = 2^{31}$, que excede `INT_MAX` =
$2^{31} - 1$. Overflow con comportamiento indefinido en C.

$$\blacksquare$$

---

## Resumen de resultados

| Resultado | Técnica | Clave |
|-----------|--------|-------|
| Correctitud | Invariante de bucle | Si target existe, está en arr[lo..hi] |
| Terminación | Función de decrecimiento $V = \text{hi} - \text{lo} + 1$ | $V' \leq \lfloor V/2 \rfloor$ en cada iteración |
| Peor caso $\Theta(\log n)$ | Cota superior + árbol de decisión | $\lfloor\log_2 n\rfloor + 1$ iteraciones; $\Omega(\log n)$ hojas |
| `mid` sin overflow | Análisis aritmético | $\text{lo} + (\text{hi}-\text{lo})/2 \leq \text{hi} < 2^{31}$ |
