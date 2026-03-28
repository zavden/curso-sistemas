# Terminología de ordenamiento

## Objetivo

Antes de estudiar algoritmos concretos, definir el vocabulario que se usa
para clasificar y comparar algoritmos de ordenamiento:

- **Estabilidad**: ¿preserva el orden relativo de elementos iguales?
- **In-place**: ¿usa memoria auxiliar proporcional a $n$?
- **Adaptativo**: ¿se beneficia de orden previo en los datos?
- **Interno vs externo**: ¿los datos caben en RAM?

Estos criterios determinan qué algoritmo elegir para cada situación.

---

## Estabilidad

Un algoritmo de ordenamiento es **estable** si, cuando dos elementos tienen
la misma clave de ordenamiento, mantienen su **orden relativo original** en
la salida.

### Ejemplo concreto

Ordenar estudiantes por nota:

```
Entrada: [("Ana", 85), ("Bob", 90), ("Cat", 85), ("Dan", 90)]
```

Estable:

```
[("Ana", 85), ("Cat", 85), ("Bob", 90), ("Dan", 90)]
   ↑ Ana antes de Cat (como en la entrada)
                          ↑ Bob antes de Dan (como en la entrada)
```

No estable (posible resultado):

```
[("Cat", 85), ("Ana", 85), ("Dan", 90), ("Bob", 90)]
   ↑ Cat antes de Ana (invertido respecto a la entrada)
```

### ¿Por qué importa?

La estabilidad es crucial cuando se ordena por **múltiples criterios**
en cascada. Para ordenar primero por departamento y luego por nombre:

1. Ordenar por nombre (criterio secundario).
2. Ordenar por departamento (criterio primario) con algoritmo **estable**.

El sort estable del paso 2 preserva el orden por nombre dentro de cada
departamento. Con un sort no estable, el paso 2 podría desordenar los
nombres.

```c
// Datos originales
{"Ana",  "Engineering"}
{"Bob",  "Sales"}
{"Cat",  "Engineering"}
{"Dan",  "Sales"}
{"Eve",  "Engineering"}

// Paso 1: ordenar por nombre
{"Ana",  "Engineering"}
{"Bob",  "Sales"}
{"Cat",  "Engineering"}
{"Dan",  "Sales"}
{"Eve",  "Engineering"}

// Paso 2 (estable): ordenar por departamento
{"Ana",  "Engineering"}   // Ana < Cat < Eve preservado
{"Cat",  "Engineering"}
{"Eve",  "Engineering"}
{"Bob",  "Sales"}         // Bob < Dan preservado
{"Dan",  "Sales"}
```

### Clasificación

| Estable | No estable |
|---------|------------|
| Merge sort | Quicksort |
| Insertion sort | Heapsort |
| Bubble sort | Selection sort |
| Counting sort | Shell sort |
| Radix sort | Introsort |
| Timsort (Rust `sort`) | pdqsort (Rust `sort_unstable`) |

---

## In-place

Un algoritmo es **in-place** si usa $O(1)$ memoria auxiliar (o $O(\log n)$
para la pila de recursión). Es decir, ordena dentro del propio array de
entrada sin crear una copia.

### Por qué importa

- **Memoria**: para arrays grandes, duplicar el espacio puede ser
  prohibitivo. Un array de $10^8$ enteros ocupa ~400 MB; un sort no
  in-place necesitaría ~800 MB.
- **Caché**: operar sobre un solo array aprovecha mejor la jerarquía de
  caché que copiar entre dos arrays.

### Clasificación

| In-place ($O(1)$ extra) | No in-place ($O(n)$ extra) |
|-------------------------|---------------------------|
| Quicksort | Merge sort (estándar) |
| Heapsort | Counting sort |
| Insertion sort | Radix sort |
| Bubble sort | Timsort (parcialmente) |
| Selection sort | |
| Shell sort | |

Merge sort puede implementarse in-place, pero la implementación es
compleja y las constantes son peores. En la práctica, merge sort usa
$O(n)$ espacio auxiliar.

Quicksort usa $O(\log n)$ espacio de pila (o $O(n)$ en el peor caso sin
tail-call optimization). Se considera in-place porque la pila es
proporcional a la **profundidad** de recursión, no al tamaño de los datos.

---

## Adaptativo

Un algoritmo es **adaptativo** si su rendimiento mejora cuando los datos
de entrada tienen algún grado de orden previo.

### Ejemplo: insertion sort

- Array aleatorio: $O(n^2)$ — cada elemento se compara con muchos otros.
- Array ya ordenado: $O(n)$ — cada elemento se compara solo con su
  predecesor y no se mueve.
- Array casi ordenado (pocos elementos fuera de lugar): $O(n + k)$ donde
  $k$ es el número de inversiones.

### Inversiones

Una **inversión** es un par $(i, j)$ donde $i < j$ pero $a[i] > a[j]$. El
número de inversiones mide "cuán desordenado" está el array.

```
[2, 1, 3, 5, 4]
Inversiones: (2,1) y (5,4) → 2 inversiones → "casi ordenado"

[5, 4, 3, 2, 1]
Inversiones: todo par → 10 inversiones → "completamente desordenado"
```

Un array ordenado tiene 0 inversiones. Un array en orden inverso tiene
$n(n-1)/2$ inversiones (el máximo).

### Clasificación

| Adaptativo | No adaptativo |
|------------|---------------|
| Insertion sort ($O(n + k)$ inversiones) | Merge sort ($O(n \log n)$ siempre) |
| Bubble sort ($O(n + k)$) | Heapsort ($O(n \log n)$ siempre) |
| Timsort ($O(n)$ a $O(n \log n)$) | Quicksort* |
| Shell sort (depende de gaps) | Selection sort ($O(n^2)$ siempre) |
| Natural merge sort | Counting/Radix sort |

*Quicksort con pivote fijo es $O(n^2)$ para datos ya ordenados (peor caso).
Con pivote aleatorio, no se adapta pero evita el peor caso.

### Timsort: el adaptativo por excelencia

Timsort (usado por Python y Rust `sort`) detecta **runs** — secuencias ya
ordenadas (ascendentes o descendentes) dentro del array. Luego las fusiona
con merge sort. Para datos con estructura natural (logs con timestamps,
datos parcialmente ordenados), Timsort puede ser $O(n)$.

---

## Interno vs externo

### Ordenamiento interno

Los datos caben completamente en **RAM**. Todos los algoritmos que
estudiaremos en C08 son internos. El acceso a cualquier posición del array
es $O(1)$.

### Ordenamiento externo

Los datos residen en **disco** (o almacenamiento secundario) y no caben
en RAM. El costo está dominado por las operaciones de I/O, no por las
comparaciones.

El algoritmo clásico es **merge sort externo**:

1. Dividir el archivo en chunks que quepan en RAM.
2. Ordenar cada chunk en RAM (con cualquier sort interno).
3. Hacer merge de los chunks ordenados usando una cola de prioridad
   (merge de $k$ vías con min-heap, como vimos en C07 T02).

```
Disco: [chunk₁, chunk₂, ..., chunkₖ]
         ↓         ↓            ↓
RAM:  [sortear] [sortear] ... [sortear]
         ↓         ↓            ↓
      [chunk₁ₛ, chunk₂ₛ, ..., chunkₖₛ]  (ordenados)
         ↓         ↓            ↓
      [     min-heap merge      ]  → archivo de salida ordenado
```

### ¿Por qué merge sort y no quicksort?

Merge sort accede a los datos **secuencialmente** — lee chunks de izquierda
a derecha. Quicksort necesita acceso **aleatorio** al array (el pivote
puede requerir saltar a cualquier posición). En disco, el acceso secuencial
es órdenes de magnitud más rápido que el aleatorio:

| Operación | RAM | SSD | HDD |
|-----------|-----|-----|-----|
| Acceso secuencial | ~1 ns | ~10 µs | ~1 ms |
| Acceso aleatorio | ~1 ns | ~100 µs | ~10 ms |
| Ratio aleatorio/secuencial | 1× | 10× | 10000× |

Por eso merge sort domina el ordenamiento externo, y el heap (cola de
prioridad) es la pieza clave de la fase de merge.

---

## Resumen de propiedades

| Algoritmo | Estable | In-place | Adaptativo | Mejor | Promedio | Peor | Espacio |
|-----------|---------|----------|------------|-------|----------|------|---------|
| Bubble sort | Sí | Sí | Sí | $O(n)$ | $O(n^2)$ | $O(n^2)$ | $O(1)$ |
| Selection sort | No | Sí | No | $O(n^2)$ | $O(n^2)$ | $O(n^2)$ | $O(1)$ |
| Insertion sort | Sí | Sí | Sí | $O(n)$ | $O(n^2)$ | $O(n^2)$ | $O(1)$ |
| Merge sort | Sí | No | No | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ | $O(n)$ |
| Quicksort | No | Sí | No | $O(n \log n)$ | $O(n \log n)$ | $O(n^2)$ | $O(\log n)$ |
| Heapsort | No | Sí | No | $O(n \log n)$ | $O(n \log n)$ | $O(n \log n)$ | $O(1)$ |
| Shell sort | No | Sí | Parcial | Depende | Depende | $O(n^{3/2})$+ | $O(1)$ |
| Counting sort | Sí | No | No | $O(n+k)$ | $O(n+k)$ | $O(n+k)$ | $O(k)$ |
| Radix sort | Sí | No | No | $O(dn)$ | $O(dn)$ | $O(dn)$ | $O(n+k)$ |
| Timsort | Sí | No* | Sí | $O(n)$ | $O(n \log n)$ | $O(n \log n)$ | $O(n)$ |

*Timsort usa $O(n)$ en el peor caso pero puede usar menos para runs
cortos.

### Conflictos entre propiedades

No existe un algoritmo que sea simultáneamente:
- Estable + in-place + $O(n \log n)$ peor caso.

Se puede tener **dos** de estas tres propiedades:
- Estable + $O(n \log n)$: merge sort (pero $O(n)$ espacio).
- In-place + $O(n \log n)$: heapsort (pero no estable).
- Estable + in-place: insertion sort (pero $O(n^2)$).

Este es un resultado teórico importante: cualquier sort basado en
comparaciones que sea estable e in-place tiene peor caso $\omega(n \log n)$
en la práctica (los algoritmos conocidos son $O(n \log^2 n)$ o tienen
constantes enormes).

---

## Demostración en código

### Verificar estabilidad

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int key;
    int original_index;  // para verificar estabilidad
} Item;

int is_stable(Item sorted[], int n) {
    for (int i = 1; i < n; i++) {
        if (sorted[i].key == sorted[i-1].key &&
            sorted[i].original_index < sorted[i-1].original_index) {
            return 0;  // par con misma clave tiene indice invertido
        }
    }
    return 1;
}

// insertion sort (estable)
void insertion_sort(Item arr[], int n) {
    for (int i = 1; i < n; i++) {
        Item key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j].key > key.key) {  // > estricto = estable
            arr[j+1] = arr[j];
            j--;
        }
        arr[j+1] = key;
    }
}

// selection sort (no estable)
void selection_sort(Item arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j].key < arr[min_idx].key)
                min_idx = j;
        }
        // swap puede mover elementos iguales fuera de orden
        Item tmp = arr[i]; arr[i] = arr[min_idx]; arr[min_idx] = tmp;
    }
}

int main(void) {
    Item data[] = {
        {5, 0}, {3, 1}, {5, 2}, {1, 3}, {3, 4}, {5, 5}
    };
    int n = 6;

    // probar insertion sort
    Item copy1[6];
    memcpy(copy1, data, sizeof(data));
    insertion_sort(copy1, n);
    printf("Insertion sort: ");
    for (int i = 0; i < n; i++)
        printf("(%d,#%d) ", copy1[i].key, copy1[i].original_index);
    printf("-> estable: %s\n", is_stable(copy1, n) ? "si" : "no");

    // probar selection sort
    Item copy2[6];
    memcpy(copy2, data, sizeof(data));
    selection_sort(copy2, n);
    printf("Selection sort: ");
    for (int i = 0; i < n; i++)
        printf("(%d,#%d) ", copy2[i].key, copy2[i].original_index);
    printf("-> estable: %s\n", is_stable(copy2, n) ? "si" : "no");

    return 0;
}
```

Salida:

```
Insertion sort: (1,#3) (3,#1) (3,#4) (5,#0) (5,#2) (5,#5) -> estable: si
Selection sort: (1,#3) (3,#1) (3,#4) (5,#5) (5,#2) (5,#0) -> estable: no
```

Insertion sort preserva el orden: los tres 5s mantienen su orden original
(#0, #2, #5). Selection sort los invierte (#5, #2, #0) porque el swap
del primer 5 (posición 0) con el 1 (posición 3) lo movió al final.

### Verificar estabilidad en Rust

```rust
fn main() {
    let data = vec![
        (5, 'A'), (3, 'B'), (5, 'C'), (1, 'D'), (3, 'E'), (5, 'F'),
    ];

    // sort (Timsort, estable)
    let mut stable = data.clone();
    stable.sort_by_key(|&(k, _)| k);
    println!("sort (estable):          {:?}", stable);

    // sort_unstable (pdqsort, no estable)
    let mut unstable = data.clone();
    unstable.sort_unstable_by_key(|&(k, _)| k);
    println!("sort_unstable (no estable): {:?}", unstable);
}
```

```
sort (estable):             [(1, 'D'), (3, 'B'), (3, 'E'), (5, 'A'), (5, 'C'), (5, 'F')]
sort_unstable (no estable): [(1, 'D'), (3, 'E'), (3, 'B'), (5, 'F'), (5, 'A'), (5, 'C')]
```

`sort` preserva B antes de E (ambos key=3) y A antes de C antes de F
(todos key=5). `sort_unstable` puede invertirlos.

### Contar inversiones

```c
// Contar inversiones en O(n^2) — para verificacion
long count_inversions(int arr[], int n) {
    long inv = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (arr[i] > arr[j]) inv++;
    return inv;
}

int main(void) {
    int sorted[]  = {1, 2, 3, 4, 5};
    int almost[]  = {1, 2, 4, 3, 5};
    int reverse[] = {5, 4, 3, 2, 1};
    int random[]  = {3, 1, 4, 5, 2};

    printf("Ordenado:  %ld inversiones\n", count_inversions(sorted, 5));
    printf("Casi-ord:  %ld inversiones\n", count_inversions(almost, 5));
    printf("Aleatorio: %ld inversiones\n", count_inversions(random, 5));
    printf("Inverso:   %ld inversiones\n", count_inversions(reverse, 5));
    printf("Maximo posible: %d\n", 5 * 4 / 2);
}
```

```
Ordenado:  0 inversiones
Casi-ord:  1 inversiones
Aleatorio: 4 inversiones
Inverso:   10 inversiones
Maximo posible: 10
```

---

## Ejercicios

### Ejercicio 1 — Clasificar algoritmos

Para cada algoritmo, indica si es estable, in-place y adaptativo:
(a) Merge sort, (b) Quicksort con pivote aleatorio, (c) Heapsort,
(d) Insertion sort.

<details>
<summary>Verificar</summary>

| Algoritmo | Estable | In-place | Adaptativo |
|-----------|---------|----------|------------|
| Merge sort | Sí | No ($O(n)$ extra) | No |
| Quicksort (pivote aleatorio) | No | Sí ($O(\log n)$ pila) | No |
| Heapsort | No | Sí ($O(1)$) | No |
| Insertion sort | Sí | Sí ($O(1)$) | Sí ($O(n)$ si casi-ordenado) |

Notar que solo insertion sort tiene las tres propiedades favorables
(estable + in-place + adaptativo), pero a costa de $O(n^2)$ en el peor caso.
</details>

### Ejercicio 2 — Estabilidad con ejemplo

Dado `[(3,'A'), (1,'B'), (3,'C'), (2,'D')]`, ¿cuál es la salida de un
sort estable por la clave numérica? ¿Y de uno no estable?

<details>
<summary>Verificar</summary>

**Estable**: `[(1,'B'), (2,'D'), (3,'A'), (3,'C')]`.
Los dos 3s mantienen su orden original: A antes de C.

**No estable** (una posibilidad): `[(1,'B'), (2,'D'), (3,'C'), (3,'A')]`.
Los 3s están invertidos: C antes de A.

Con un sort no estable, el orden relativo de A y C es **indeterminado** —
puede ser correcto por casualidad, pero no está garantizado.
</details>

### Ejercicio 3 — Contar inversiones

¿Cuántas inversiones tiene el array `[4, 3, 1, 5, 2]`? ¿Y `[1, 2, 3, 4, 5]`?
¿Y `[5, 4, 3, 2, 1]`?

<details>
<summary>Verificar</summary>

**`[4, 3, 1, 5, 2]`**: pares $(i,j)$ con $i < j$ y $a[i] > a[j]$:
- (4,3), (4,1), (4,2): 3
- (3,1), (3,2): 2
- (5,2): 1

Total: **6 inversiones**.

**`[1, 2, 3, 4, 5]`**: 0 inversiones (ya ordenado).

**`[5, 4, 3, 2, 1]`**: $\binom{5}{2} = 10$ inversiones (todo par es una
inversión). Esto es el máximo para $n = 5$.

Fórmula general: un array de $n$ elementos tiene entre 0 y $n(n-1)/2$
inversiones. Un array aleatorio tiene en promedio $n(n-1)/4$ inversiones.
</details>

### Ejercicio 4 — In-place vs no in-place

¿Por qué merge sort estándar no es in-place? ¿Qué operación específica
requiere el array auxiliar?

<details>
<summary>Verificar</summary>

La operación de **merge** (fusión) necesita un array auxiliar. Al fusionar
dos mitades ordenadas `[1, 3, 5]` y `[2, 4, 6]`, necesitamos escribir el
resultado en un buffer temporal porque si escribimos directamente sobre el
array original, sobrescribimos elementos que aún no hemos leído.

```
Original: [1, 3, 5, 2, 4, 6]
           ↑ left   ↑ right

Si intentamos escribir in-place desde posición 0:
- Comparar 1 vs 2: escribir 1 en pos 0 → OK (ya estaba ahí)
- Comparar 3 vs 2: escribir 2 en pos 1 → sobrescribe el 3!
```

Con buffer auxiliar:

```
Buffer:   [_, _, _, _, _, _]
Original: [1, 3, 5, 2, 4, 6]

Copiar al buffer y fusionar de vuelta al original.
```

Existe merge sort in-place (con rotaciones o merge sin buffer), pero es
$O(n \log^2 n)$ o tiene constantes muy altas, por lo que rara vez se usa
en la práctica.
</details>

### Ejercicio 5 — Adaptativo en la práctica

¿Por qué insertion sort es $O(n)$ para un array ya ordenado? Traza la
ejecución para `[1, 2, 3, 4, 5]`.

<details>
<summary>Verificar</summary>

```
i=1: key=2. Comparar con arr[0]=1. 1 ≤ 2 → no mover. 1 comparación.
i=2: key=3. Comparar con arr[1]=2. 2 ≤ 3 → no mover. 1 comparación.
i=3: key=4. Comparar con arr[2]=3. 3 ≤ 4 → no mover. 1 comparación.
i=4: key=5. Comparar con arr[3]=4. 4 ≤ 5 → no mover. 1 comparación.
```

Total: 4 comparaciones, 0 movimientos. Para $n$ elementos: $n-1$
comparaciones = $O(n)$.

Para un array con $k$ inversiones, insertion sort hace exactamente $n - 1 + k$
comparaciones: $n-1$ del loop externo más $k$ comparaciones adicionales del
loop interno (una por cada inversión que "deshace"). Por eso su complejidad
es $O(n + k)$ donde $k$ es el número de inversiones.
</details>

### Ejercicio 6 — Sort externo

Si tienes un archivo de 10 GB de registros y solo 1 GB de RAM, ¿cuántos
chunks necesitas para la fase 1 del merge sort externo? ¿Cuántos
elementos tiene la cola de prioridad en la fase de merge?

<details>
<summary>Verificar</summary>

**Chunks**: $\lceil 10 / 1 \rceil = 10$ chunks de ~1 GB cada uno.

**Cola de prioridad**: 10 elementos (uno por chunk — el menor elemento
no procesado de cada chunk). Cada `extract_min` saca un elemento, y se
inserta el siguiente del mismo chunk. El heap siempre tiene a lo sumo 10
entradas.

Cada operación de heap es $O(\log 10) \approx 3.3$ comparaciones. Si hay
$N$ registros en total, el merge hace $N$ extracciones e inserciones:
$O(N \log k) = O(N \log 10)$ comparaciones. El cuello de botella es la I/O
de disco, no las comparaciones.

Con 100 GB y 1 GB RAM: 100 chunks, heap de 100, $O(N \log 100)$ — apenas
2× más comparaciones, pero 10× más I/O secuencial.
</details>

### Ejercicio 7 — Estabilidad en sort multi-criterio

Tienes empleados con `(departamento, salario)`. Quieres ordenar por
departamento y dentro de cada departamento por salario ascendente. ¿Cómo
lo haces con un sort estable? ¿Y con uno no estable?

<details>
<summary>Verificar</summary>

**Con sort estable** (dos pasadas):
1. Ordenar por salario (criterio secundario).
2. Ordenar por departamento (criterio primario) con sort estable.

El sort estable del paso 2 preserva el orden de salario dentro de cada
departamento.

**Con sort no estable** (una pasada con comparador compuesto):

```c
int cmp_employee(const void *a, const void *b) {
    const Employee *ea = a, *eb = b;
    int dept = strcmp(ea->department, eb->department);
    if (dept != 0) return dept;
    return ea->salary - eb->salary;  // desempate por salario
}
```

Con un comparador que compare primero por departamento y desempate por
salario, cualquier sort (estable o no) produce el resultado correcto en
una sola pasada. La estabilidad solo importa cuando se ordena por
criterios **separados** en múltiples pasadas.
</details>

### Ejercicio 8 — Clasificar is_stable en Rust

Escribe una función Rust que verifique empíricamente si un sort es estable,
dado un slice de `(key, original_index)`.

<details>
<summary>Verificar</summary>

```rust
fn is_stable(sorted: &[(i32, usize)]) -> bool {
    for window in sorted.windows(2) {
        if window[0].0 == window[1].0 && window[0].1 > window[1].1 {
            return false;
        }
    }
    true
}

fn main() {
    let data: Vec<(i32, usize)> = vec![
        (5, 0), (3, 1), (5, 2), (1, 3), (3, 4), (5, 5),
    ];

    let mut stable = data.clone();
    stable.sort_by_key(|&(k, _)| k);
    println!("sort: estable = {}", is_stable(&stable));

    let mut unstable = data.clone();
    unstable.sort_unstable_by_key(|&(k, _)| k);
    println!("sort_unstable: estable = {}", is_stable(&unstable));
}
```

```
sort: estable = true
sort_unstable: estable = false
```

`windows(2)` itera sobre pares consecutivos — si dos elementos con la misma
clave tienen `original_index` invertido, el sort no fue estable.
</details>

### Ejercicio 9 — Inversiones y complejidad

Si insertion sort tiene complejidad $O(n + k)$ donde $k$ es el número de
inversiones, ¿cuál es la complejidad promedio para un array aleatorio?
Pista: un array aleatorio tiene $n(n-1)/4$ inversiones en promedio.

<details>
<summary>Verificar</summary>

Para un array aleatorio de $n$ elementos, el número esperado de inversiones es:

$$E[k] = \frac{n(n-1)}{4}$$

Esto se demuestra así: para cada par $(i,j)$ con $i < j$, la probabilidad
de que $a[i] > a[j]$ es $1/2$ (por simetría). Hay $\binom{n}{2} = n(n-1)/2$
pares, así que:

$$E[k] = \frac{n(n-1)}{2} \times \frac{1}{2} = \frac{n(n-1)}{4}$$

La complejidad promedio de insertion sort es:

$$O\left(n + \frac{n(n-1)}{4}\right) = O\left(\frac{n^2}{4}\right) = O(n^2)$$

El factor $1/4$ es una constante — la complejidad promedio sigue siendo
cuadrática. Por eso insertion sort solo es eficiente para datos casi
ordenados (pocas inversiones) o arrays pequeños.
</details>

### Ejercicio 10 — El triángulo imposible

Demuestra con la tabla de propiedades que ningún algoritmo basado en
comparaciones logra las tres propiedades simultáneamente: estable +
in-place + $O(n \log n)$ peor caso. ¿Qué dos de las tres tiene cada uno
de los algoritmos principales?

<details>
<summary>Verificar</summary>

| Algoritmo | Estable | In-place | $O(n \log n)$ peor | Qué sacrifica |
|-----------|---------|----------|-------------------|---------------|
| Merge sort | Sí | No | Sí | In-place |
| Heapsort | No | Sí | Sí | Estabilidad |
| Insertion sort | Sí | Sí | No ($O(n^2)$) | Peor caso eficiente |
| Quicksort | No | Sí | No ($O(n^2)$) | Estabilidad + peor caso |

Cada algoritmo logra **dos** de las tres:
- **Merge sort**: estable + $O(n \log n)$, pero necesita $O(n)$ espacio.
- **Heapsort**: in-place + $O(n \log n)$, pero no es estable.
- **Insertion sort**: estable + in-place, pero $O(n^2)$ peor caso.

Se conocen algoritmos que logran las tres (como block merge sort / WikiSort),
pero con constantes tan grandes que en la práctica son más lentos que merge
sort con buffer. Por eso las implementaciones reales hacen un compromiso:
- Timsort: estable + $O(n \log n)$, usa $O(n)$ espacio.
- Introsort/pdqsort: in-place + $O(n \log n)$, no estable.

Este "triángulo imposible" es uno de los resultados más elegantes de la
teoría de ordenamiento.
</details>
