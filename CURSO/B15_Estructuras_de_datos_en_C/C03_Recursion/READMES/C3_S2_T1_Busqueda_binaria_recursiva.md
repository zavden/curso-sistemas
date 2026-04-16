# T01 — Búsqueda binaria recursiva

## Índice

1. [El problema de la búsqueda](#1-el-problema-de-la-búsqueda)
2. [Idea de la búsqueda binaria](#2-idea-de-la-búsqueda-binaria)
3. [Implementación recursiva en C](#3-implementación-recursiva-en-c)
4. [Implementación recursiva en Rust](#4-implementación-recursiva-en-rust)
5. [Traza de ejecución](#5-traza-de-ejecución)
6. [Implementación iterativa](#6-implementación-iterativa)
7. [Comparación recursiva vs iterativa](#7-comparación-recursiva-vs-iterativa)
8. [Análisis de complejidad](#8-análisis-de-complejidad)
9. [Variantes y sutilezas](#9-variantes-y-sutilezas)
10. [Ejercicios](#10-ejercicios)

---

## 1. El problema de la búsqueda

Dado un array ordenado de $n$ elementos y un valor objetivo (target),
determinar si el valor existe en el array y, si existe, retornar su
posición.

### Búsqueda lineal

La solución ingenua recorre todo el array:

```c
int linear_search(const int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}
```

Complejidad: $O(n)$. Para un array de 1 millón de elementos, examina
hasta 1 millón de posiciones. No aprovecha que el array está ordenado.

### ¿Se puede hacer mejor?

Si el array está ordenado, al examinar un elemento cualquiera podemos
descartar **la mitad** del array en una sola comparación. Si el elemento
examinado es menor que el target, el target solo puede estar en la mitad
derecha. Si es mayor, solo en la mitad izquierda. Esto es la búsqueda
binaria.

---

## 2. Idea de la búsqueda binaria

### Analogía: el diccionario

Para buscar una palabra en un diccionario de papel, nadie empieza por la
primera página. Se abre por la mitad: si la palabra buscada va después
alfabéticamente, se descarta la primera mitad; si va antes, se descarta
la segunda. Se repite con la mitad restante.

Cada paso reduce el espacio de búsqueda a la mitad:

```
n → n/2 → n/4 → n/8 → ... → 1
```

El número de pasos es $\log_2(n)$. Para $n = 1,000,000$: solo
$\lceil\log_2(1,000,000)\rceil = 20$ comparaciones.

### Definición recursiva

$$\text{bsearch}(arr, lo, hi, t) = \begin{cases} -1 & \text{si } lo > hi \\ mid & \text{si } arr[mid] = t \\ \text{bsearch}(arr, lo, mid-1, t) & \text{si } arr[mid] > t \\ \text{bsearch}(arr, mid+1, hi, t) & \text{si } arr[mid] < t \end{cases}$$

donde $mid = \lfloor(lo + hi) / 2\rfloor$.

Esta es una definición naturalmente recursiva: el problema se reduce a
sí mismo con un rango más pequeño.

---

## 3. Implementación recursiva en C

```c
/* binary_search.c */
#include <stdio.h>

int binary_search_rec(const int *arr, int lo, int hi, int target) {
    if (lo > hi) return -1;             /* base: not found */

    int mid = lo + (hi - lo) / 2;       /* safe midpoint */

    if (arr[mid] == target) return mid;  /* base: found */
    if (arr[mid] > target) {
        return binary_search_rec(arr, lo, mid - 1, target);
    }
    return binary_search_rec(arr, mid + 1, hi, target);
}

/* Public wrapper */
int binary_search(const int *arr, int n, int target) {
    return binary_search_rec(arr, 0, n - 1, target);
}

int main(void) {
    int arr[] = {2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91};
    int n = sizeof(arr) / sizeof(arr[0]);

    int tests[] = {23, 2, 91, 50, 0, 100};
    int ntests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < ntests; i++) {
        int idx = binary_search(arr, n, tests[i]);
        if (idx >= 0) {
            printf("Found %d at index %d\n", tests[i], idx);
        } else {
            printf("%d not found\n", tests[i]);
        }
    }

    return 0;
}
```

Salida:

```
Found 23 at index 5
Found 2 at index 0
Found 91 at index 10
50 not found
0 not found
100 not found
```

### El cálculo seguro del punto medio

```c
int mid = lo + (hi - lo) / 2;
```

Esto es equivalente a `(lo + hi) / 2` pero **evita overflow de enteros**.
Si `lo` y `hi` son ambos cercanos a `INT_MAX`, `lo + hi` se desborda. La
forma `lo + (hi - lo) / 2` nunca desborda porque `hi - lo >= 0` y
`lo + algo_positivo_pequeño` no excede `hi`.

Este bug estuvo presente en la implementación de `bsearch` de la JDK de
Java durante casi una década antes de ser corregido. Es un ejemplo clásico
de error sutil en código aparentemente trivial.

---

## 4. Implementación recursiva en Rust

```rust
// binary_search.rs

fn binary_search_rec(arr: &[i32], target: i32) -> Option<usize> {
    if arr.is_empty() {
        return None;
    }

    let mid = arr.len() / 2;

    match arr[mid].cmp(&target) {
        std::cmp::Ordering::Equal   => Some(mid),
        std::cmp::Ordering::Greater => binary_search_rec(&arr[..mid], target),
        std::cmp::Ordering::Less    => {
            binary_search_rec(&arr[mid + 1..], target)
                .map(|i| i + mid + 1)
        }
    }
}

fn main() {
    let arr = [2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91];

    let tests = [23, 2, 91, 50, 0, 100];
    for &t in &tests {
        match binary_search_rec(&arr, t) {
            Some(idx) => println!("Found {} at index {}", t, idx),
            None => println!("{} not found", t),
        }
    }
}
```

### Diferencias con la versión C

**Slices en lugar de índices**: En Rust, `&arr[..mid]` y `&arr[mid+1..]`
crean sub-slices sin copiar datos. Esto elimina los parámetros `lo`/`hi`:
el slice **es** el rango de búsqueda.

**`Option<usize>` en lugar de `-1`**: En C, `-1` es un valor mágico que
puede confundirse con un índice válido en código descuidado. En Rust,
`None` es un tipo distinto — el compilador obliga a manejarlo.

**`.map(|i| i + mid + 1)`**: Cuando se busca en la mitad derecha
(`&arr[mid+1..]`), el índice retornado es relativo al sub-slice. Hay que
sumar `mid + 1` para obtener el índice en el array original. `.map()`
aplica la transformación solo si hay un `Some`.

**`cmp` y `Ordering`**: El patrón `match` sobre la comparación es
exhaustivo — no se puede olvidar un caso (igual, mayor, menor). En C,
la cadena `if/else if/else` puede tener ramas faltantes.

### Versión con índices (más similar a C)

```rust
fn binary_search_idx(arr: &[i32], lo: usize, hi: usize, target: i32) -> Option<usize> {
    if lo > hi { return None; }

    let mid = lo + (hi - lo) / 2;

    match arr[mid].cmp(&target) {
        std::cmp::Ordering::Equal   => Some(mid),
        std::cmp::Ordering::Greater => {
            if mid == 0 { return None; }  // prevent underflow
            binary_search_idx(arr, lo, mid - 1, target)
        }
        std::cmp::Ordering::Less => {
            binary_search_idx(arr, mid + 1, hi, target)
        }
    }
}
```

Nota el `if mid == 0` antes de `mid - 1`: en Rust, `usize` es unsigned,
así que `0 - 1` produce **panic** en debug y wrap-around en release. En C
con `int`, `-1` es un valor válido (aunque incorrecto como índice). Este
es un caso donde los unsigned de Rust exponen un error que C ocultaría.

---

## 5. Traza de ejecución

Array: `[2, 5, 8, 12, 16, 23, 38, 45, 56, 72, 91]` (11 elementos)

### Traza 1: buscar 23

```
Llamada                lo  hi  mid  arr[mid]  Acción
───────────────────────────────────────────────────────
bsearch(0, 10, 23)     0   10   5     23      arr[5]==23 → return 5 ✓
```

Encontrado en **1 comparación** (mejor caso: el target está en el medio).

### Traza 2: buscar 72

```
Llamada                lo  hi  mid  arr[mid]  Acción
───────────────────────────────────────────────────────
bsearch(0, 10, 72)     0   10   5     23      23 < 72 → buscar derecha
bsearch(6, 10, 72)     6   10   8     56      56 < 72 → buscar derecha
bsearch(9, 10, 72)     9   10   9     72      arr[9]==72 → return 9 ✓
```

Encontrado en **3 comparaciones**.

### Traza 3: buscar 50 (no existe)

```
Llamada                lo  hi  mid  arr[mid]  Acción
───────────────────────────────────────────────────────
bsearch(0, 10, 50)     0   10   5     23      23 < 50 → buscar derecha
bsearch(6, 10, 50)     6   10   8     56      56 > 50 → buscar izquierda
bsearch(6, 7, 50)      6    7   6     38      38 < 50 → buscar derecha
bsearch(7, 7, 50)      7    7   7     45      45 < 50 → buscar derecha
bsearch(8, 7, 50)      8    7   —     —       lo > hi → return -1 ✗
```

No encontrado en **4 comparaciones** + 1 verificación de rango vacío.

### Diagrama de decisión

```
                        [5]=23
                       /      \
               [2]=8            [8]=56
              /    \           /      \
         [0]=2    [3]=12   [6]=38    [9]=72
           \        \        \       /    \
          [1]=5   [4]=16   [7]=45 [10]=91
```

Este árbol binario de decisión muestra qué elemento se compara en cada
nivel. La profundidad máxima es $\lfloor\log_2(11)\rfloor = 3$ (4 niveles
contando la raíz). Cualquier elemento se encuentra en a lo sumo 4
comparaciones.

---

## 6. Implementación iterativa

### En C

```c
int binary_search_iter(const int *arr, int n, int target) {
    int lo = 0, hi = n - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;

        if (arr[mid] == target) return mid;
        if (arr[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return -1;
}
```

### En Rust

```rust
fn binary_search_iter(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo: usize = 0;
    let mut hi: usize = arr.len();  // exclusive upper bound

    while lo < hi {
        let mid = lo + (hi - lo) / 2;

        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal   => return Some(mid),
            std::cmp::Ordering::Less    => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }

    None
}
```

Nota: la versión Rust usa rango **semi-abierto** `[lo, hi)` donde `hi` es
exclusivo. Esto evita el problema de `mid - 1` con `usize` y es el
convenio estándar de Rust (los rangos `..` son semi-abiertos). La versión
C usa rango **cerrado** `[lo, hi]` donde `hi` es inclusivo.

| Convención | Rango | Condición de fin | `mid` excluido |
|:----------:|:-----:|:----------------:|:--------------:|
| Cerrado `[lo, hi]` | `lo <= hi` | `lo > hi` | `hi = mid - 1` |
| Semi-abierto `[lo, hi)` | `lo < hi` | `lo >= hi` | `hi = mid` |

Ambas son correctas. La semi-abierta evita underflow con unsigned y es
más consistente con la convención de slices de Rust.

---

## 7. Comparación recursiva vs iterativa

### Recursión de cola

La búsqueda binaria recursiva **es recursión de cola**: cada llamada
recursiva es la última operación (no hay trabajo pendiente). Esto
significa:

1. La conversión a iteración es trivial (lo hicimos en la sección 6).
2. Con TCO (`-O2` en GCC), el compilador puede eliminar la recursión.
3. Sin TCO, la profundidad máxima es $O(\log n)$ — segura para cualquier
   tamaño práctico.

### Tabla comparativa

| Aspecto | Recursiva | Iterativa |
|---------|:---------:|:---------:|
| Claridad | Alta (refleja la definición) | Alta (bucle simple) |
| Espacio de stack | $O(\log n)$ sin TCO, $O(1)$ con TCO | $O(1)$ siempre |
| Riesgo de overflow | Ninguno ($\log_2(10^{18}) = 60$) | Ninguno |
| Overhead por llamada | ~5 ns por frame | 0 |
| Rendimiento | Prácticamente igual | Prácticamente igual |

Para búsqueda binaria, la diferencia de rendimiento entre recursiva e
iterativa es **despreciable**: incluso sin TCO, solo hay ~30-40 llamadas
recursivas para arrays de miles de millones de elementos. El overhead de
40 frames es insignificante comparado con los ~40 accesos a memoria.

### ¿Cuál usar?

Para búsqueda binaria, es cuestión de preferencia. En la práctica:

- **Bibliotecas estándar** usan iterativa (C `bsearch`, Rust
  `slice::binary_search`).
- **Enseñanza y teoría** usan recursiva (refleja la definición y la
  estructura divide-y-vencerás).
- **Código de producción** en Rust: usar `arr.binary_search(&target)`
  directamente.

---

## 8. Análisis de complejidad

### Tiempo

En cada paso, el espacio de búsqueda se reduce a la mitad. Partiendo de
$n$ elementos:

$$n \to \frac{n}{2} \to \frac{n}{4} \to \cdots \to 1$$

El número de pasos $k$ satisface $\frac{n}{2^k} = 1$, es decir,
$k = \log_2(n)$.

$$T(n) = O(\log n)$$

Más precisamente, el número de comparaciones es a lo sumo
$\lfloor\log_2(n)\rfloor + 1$.

### Recurrencia

La complejidad se puede expresar como recurrencia:

$$T(n) = T(n/2) + O(1)$$

- $T(n/2)$: se busca en una mitad.
- $O(1)$: una comparación con el punto medio.

Resolviendo: $T(n) = O(\log n)$.

### Espacio

| Versión | Espacio |
|---------|:-------:|
| Iterativa | $O(1)$ |
| Recursiva sin TCO | $O(\log n)$ (frames en el stack) |
| Recursiva con TCO | $O(1)$ |

### Comparación con búsqueda lineal

| $n$ | Lineal ($n$) | Binaria ($\log_2 n$) | Speedup |
|:---:|:------------:|:--------------------:|:-------:|
| 100 | 100 | 7 | 14× |
| 10,000 | 10,000 | 14 | 714× |
| 1,000,000 | 1,000,000 | 20 | 50,000× |
| $10^9$ | $10^9$ | 30 | 33,000,000× |

La diferencia es dramática para arrays grandes. La búsqueda binaria es
uno de los algoritmos más eficientes que existen, pero requiere que el
array esté **ordenado** (costo de ordenar: $O(n \log n)$, amortizable si
se hacen muchas búsquedas).

---

## 9. Variantes y sutilezas

### 9.1 Primera/última ocurrencia

Si el array tiene duplicados, `binary_search` retorna **alguna** posición
del target, no necesariamente la primera ni la última.

Para encontrar la **primera** ocurrencia:

```c
int first_occurrence(const int *arr, int n, int target) {
    int lo = 0, hi = n - 1;
    int result = -1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (arr[mid] == target) {
            result = mid;       /* remember this position */
            hi = mid - 1;       /* keep searching left */
        } else if (arr[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return result;
}
```

La diferencia clave: al encontrar el target, **no se retorna
inmediatamente**. Se guarda la posición y se sigue buscando en la mitad
izquierda para ver si hay una ocurrencia anterior.

### 9.2 Punto de inserción

`binary_search` modificada para retornar dónde **debería insertarse** el
target para mantener el orden:

```rust
fn insertion_point(arr: &[i32], target: i32) -> usize {
    let mut lo = 0;
    let mut hi = arr.len();

    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        if arr[mid] < target {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    lo  // position where target should be inserted
}
```

Esto es equivalente a `lower_bound` en C++ o `partition_point` en Rust.

### 9.3 Rust estándar

Rust proporciona búsqueda binaria en la librería estándar:

```rust
let arr = [2, 5, 8, 12, 16, 23, 38, 45];

// Returns Result<usize, usize>
match arr.binary_search(&23) {
    Ok(idx)  => println!("Found at {}", idx),        // found
    Err(idx) => println!("Not found, insert at {}", idx), // insertion point
}
```

El `Result` codifica ambas variantes: `Ok(índice)` si se encuentra,
`Err(punto_de_inserción)` si no. Esto es más útil que un simple
`Option<usize>` porque el punto de inserción permite mantener el array
ordenado con `vec.insert(idx, value)`.

### 9.4 Precondición: array ordenado

La búsqueda binaria **requiere** que el array esté ordenado. Si no lo
está, el resultado es indefinido (puede retornar un índice incorrecto o
no encontrar un elemento que sí existe). Verificar que el array esté
ordenado es $O(n)$, lo que anularía la ventaja de la búsqueda binaria.
En la práctica, se asume la precondición como contrato del caller.

---

## 10. Ejercicios

---

### Ejercicio 1 — Traza manual

Realiza la traza de `binary_search_rec` para buscar el valor 16 en:

```
arr = [3, 7, 11, 16, 23, 31, 42, 55, 68]
```

Anota `lo`, `hi`, `mid`, `arr[mid]` y la decisión en cada paso.

<details>
<summary>¿Cuántas comparaciones se necesitan?</summary>

```
Llamada              lo  hi  mid  arr[mid]  Decisión
────────────────────────────────────────────────────────
bsearch(0, 8, 16)    0   8    4     23      23 > 16 → izquierda
bsearch(0, 3, 16)    0   3    1      7       7 < 16 → derecha
bsearch(2, 3, 16)    2   3    2     11      11 < 16 → derecha
bsearch(3, 3, 16)    3   3    3     16      16 == 16 → return 3 ✓
```

**4 comparaciones**. El array tiene 9 elementos, $\lfloor\log_2(9)\rfloor + 1 = 4$.
Este caso alcanza el máximo teórico (peor caso para 9 elementos).

</details>

---

### Ejercicio 2 — Elemento no existente

Busca el valor 20 en el mismo array del ejercicio 1. ¿Cuántas
comparaciones se hacen hasta determinar que no existe?

<details>
<summary>¿Cuál es el último rango examinado antes de retornar -1?</summary>

```
Llamada              lo  hi  mid  arr[mid]  Decisión
────────────────────────────────────────────────────────
bsearch(0, 8, 20)    0   8    4     23      23 > 20 → izquierda
bsearch(0, 3, 20)    0   3    1      7       7 < 20 → derecha
bsearch(2, 3, 20)    2   3    2     11      11 < 20 → derecha
bsearch(3, 3, 20)    3   3    3     16      16 < 20 → derecha
bsearch(4, 3, 20)    4   3    —     —       lo > hi → return -1 ✗
```

**4 comparaciones** + 1 verificación de rango vacío. El último rango
examinado fue `[3, 3]` con `arr[3] = 16 < 20`, que generó `lo = 4 > hi = 3`.

El 20 debería estar entre 16 (índice 3) y 23 (índice 4). La función
`insertion_point` retornaría 4.

</details>

---

### Ejercicio 3 — Peor caso

Para un array de 1024 elementos, ¿cuál es el número máximo de
comparaciones? ¿Y para 1023?

<details>
<summary>¿Cuántas comparaciones en el peor caso?</summary>

$\lfloor\log_2(n)\rfloor + 1$:

- $n = 1024 = 2^{10}$: $10 + 1 = $ **11 comparaciones**.
- $n = 1023 = 2^{10} - 1$: $\lfloor\log_2(1023)\rfloor + 1 = 9 + 1 = $ **10 comparaciones**.

Paradoja aparente: 1024 elementos requieren más comparaciones que 1023.
Esto se debe a que $1024 = 2^{10}$ es una potencia exacta de 2 y produce
un árbol de decisión con un nivel adicional para una sola hoja.

En la práctica, la diferencia de una comparación es irrelevante. Pero
demuestra que la complejidad es $\Theta(\log n)$ con un factor constante
que varía entre $\lfloor\log_2 n\rfloor$ y $\lfloor\log_2 n\rfloor + 1$.

</details>

---

### Ejercicio 4 — Overflow del punto medio

Demuestra el bug de overflow con un ejemplo concreto. Si `lo = 2,000,000,000`
y `hi = 2,100,000,000`, calcula `(lo + hi) / 2` vs `lo + (hi - lo) / 2`
usando aritmética de `int` (32 bits, máximo $2^{31}-1 = 2,147,483,647$).

<details>
<summary>¿Cuál produce overflow y cuál no?</summary>

```
lo = 2,000,000,000
hi = 2,100,000,000

Forma insegura: (lo + hi) / 2
  lo + hi = 4,100,000,000
  INT_MAX = 2,147,483,647
  4,100,000,000 > INT_MAX → OVERFLOW
  En complemento a 2: 4,100,000,000 - 2^32 = -194,967,296
  (-194,967,296) / 2 = -97,483,648  ← resultado NEGATIVO (basura)

Forma segura: lo + (hi - lo) / 2
  hi - lo = 100,000,000  (no overflow, resultado positivo)
  100,000,000 / 2 = 50,000,000
  lo + 50,000,000 = 2,050,000,000  ← resultado CORRECTO
```

El resultado correcto es 2,050,000,000. La forma insegura produce
$-97,483,648$, que como índice de array causaría acceso fuera de límites
(segfault o corrupción de memoria).

En Rust con `usize` (64 bits), el overflow no ocurre para arrays
prácticos (máximo $2^{64}$ bytes de memoria). Pero con `i32`, el mismo
bug existe. La forma segura es preferible siempre.

</details>

---

### Ejercicio 5 — Recursiva de cola explícita

La búsqueda binaria recursiva ya es de cola. Conviértela a `loop` en
Rust siguiendo el patrón mecánico de T04.

<details>
<summary>¿Es esta conversión idéntica a la versión iterativa de la sección 6?</summary>

```rust
fn binary_search_loop(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo: usize = 0;
    let mut hi: usize = arr.len();

    loop {
        if lo >= hi { return None; }

        let mid = lo + (hi - lo) / 2;

        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal   => return Some(mid),
            std::cmp::Ordering::Less    => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }
}
```

**Sí, es esencialmente idéntica** a la versión iterativa de la sección 6.
La conversión mecánica de recursión de cola a `loop` produce exactamente
el mismo bucle `while` (o `loop` con `return`). Esto confirma que la
búsqueda binaria recursiva es un caso donde recursión e iteración son
intercambiables sin complejidad adicional.

La conversión: `lo` y `hi` (parámetros de la recursión) se convierten en
variables mutables. La condición de caso base se convierte en condición
de salida del loop. Las reasignaciones `lo = mid + 1` / `hi = mid`
reemplazan las llamadas recursivas.

</details>

---

### Ejercicio 6 — Primera y última ocurrencia

Dado el array `[1, 3, 3, 3, 5, 7, 7, 9]`, implementa en Rust funciones
que retornen:

a) El índice de la **primera** ocurrencia de 3.
b) El índice de la **última** ocurrencia de 3.
c) El **conteo** de ocurrencias de 3.

<details>
<summary>¿Cómo calculas el conteo sin recorrer todas las ocurrencias?</summary>

```rust
fn first_occurrence(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo = 0;
    let mut hi = arr.len();
    let mut result = None;

    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal => {
                result = Some(mid);
                hi = mid;           // keep searching left
            }
            std::cmp::Ordering::Less => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }
    result
}

fn last_occurrence(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo = 0;
    let mut hi = arr.len();
    let mut result = None;

    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(&target) {
            std::cmp::Ordering::Equal => {
                result = Some(mid);
                lo = mid + 1;       // keep searching right
            }
            std::cmp::Ordering::Less => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }
    result
}

fn count_occurrences(arr: &[i32], target: i32) -> usize {
    match (first_occurrence(arr, target), last_occurrence(arr, target)) {
        (Some(first), Some(last)) => last - first + 1,
        _ => 0,
    }
}
```

Para `[1, 3, 3, 3, 5, 7, 7, 9]` buscando 3:
- Primera: **1** (índice 1)
- Última: **3** (índice 3)
- Conteo: $3 - 1 + 1 = $ **3**

El conteo se obtiene en $O(\log n)$ — dos búsquedas binarias, sin
recorrer los duplicados. Para un array con 1 millón de 3s, la búsqueda
lineal haría 1 millón de comparaciones; esta solución hace ~40.

</details>

---

### Ejercicio 7 — Búsqueda en array rotado

Un array ordenado se "rota": `[4, 5, 6, 7, 0, 1, 2]` (era
`[0, 1, 2, 4, 5, 6, 7]`). Implementa búsqueda binaria modificada que
funcione en arrays rotados. Pista: una de las dos mitades siempre está
ordenada.

<details>
<summary>¿Cómo decides en qué mitad buscar?</summary>

```rust
fn search_rotated(arr: &[i32], target: i32) -> Option<usize> {
    let mut lo = 0;
    let mut hi = arr.len();

    while lo < hi {
        let mid = lo + (hi - lo) / 2;

        if arr[mid] == target {
            return Some(mid);
        }

        if arr[lo] <= arr[mid] {
            // left half [lo..mid] is sorted
            if arr[lo] <= target && target < arr[mid] {
                hi = mid;       // target is in sorted left half
            } else {
                lo = mid + 1;   // target is in right half
            }
        } else {
            // right half [mid..hi) is sorted
            if arr[mid] < target && target <= arr[hi - 1] {
                lo = mid + 1;   // target is in sorted right half
            } else {
                hi = mid;       // target is in left half
            }
        }
    }

    None
}
```

La clave: si `arr[lo] <= arr[mid]`, la mitad izquierda está ordenada.
Si el target cae dentro de ese rango ordenado, buscar ahí; si no, buscar
en la otra mitad. Siempre una de las dos mitades está completamente
ordenada en un array rotado.

Para `[4, 5, 6, 7, 0, 1, 2]`, buscando 0:
- mid=3, arr[3]=7. arr[0]=4 ≤ 7 → izquierda ordenada [4,5,6,7].
  0 no está en [4,7] → buscar derecha.
- lo=4, mid=5, arr[5]=1. arr[4]=0 ≤ 1 → izquierda ordenada [0,1].
  0 está en [0,1] → buscar izquierda.
- lo=4, mid=4, arr[4]=0 == 0 → found at index 4 ✓

Complejidad: $O(\log n)$ — misma que búsqueda binaria estándar.

</details>

---

### Ejercicio 8 — Búsqueda genérica en Rust

Implementa una búsqueda binaria genérica que funcione con cualquier tipo
que implemente `Ord`:

```rust
fn binary_search<T: Ord>(arr: &[T], target: &T) -> Option<usize>;
```

Prueba con `&[i32]`, `&[&str]`, y `&[char]`.

<details>
<summary>¿Qué trait bound necesitas y por qué?</summary>

```rust
fn binary_search<T: Ord>(arr: &[T], target: &T) -> Option<usize> {
    let mut lo = 0;
    let mut hi = arr.len();

    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        match arr[mid].cmp(target) {
            std::cmp::Ordering::Equal   => return Some(mid),
            std::cmp::Ordering::Less    => lo = mid + 1,
            std::cmp::Ordering::Greater => hi = mid,
        }
    }

    None
}

fn main() {
    let ints = [1, 3, 5, 7, 9];
    assert_eq!(binary_search(&ints, &5), Some(2));

    let strs = ["apple", "banana", "cherry", "date"];
    assert_eq!(binary_search(&strs, &"cherry"), Some(2));
    assert_eq!(binary_search(&strs, &"fig"), None);

    let chars = ['a', 'e', 'i', 'o', 'u'];
    assert_eq!(binary_search(&chars, &'o'), Some(3));
}
```

El trait bound `Ord` proporciona el método `.cmp()` que retorna
`Ordering`. `Ord` implica orden total (todo par de elementos es
comparable). `PartialOrd` no bastaría porque tipos como `f64` tienen
`NaN` que no es comparable (`NaN != NaN`), lo que rompería la búsqueda
binaria.

Para `f64`, se usaría `PartialOrd` con manejo especial de `NaN`, o el
wrapper `OrderedFloat` de la crate `ordered-float`.

</details>

---

### Ejercicio 9 — Benchmark lineal vs binaria

Mide el tiempo de búsqueda en un array de 10 millones de enteros
ordenados, buscando un elemento que no existe. Compara búsqueda lineal
vs binaria.

<details>
<summary>¿Cuál es la diferencia de tiempo y de número de comparaciones?</summary>

```rust
use std::time::Instant;

fn main() {
    let n = 10_000_000;
    let arr: Vec<i32> = (0..n).map(|i| i * 2).collect(); // even numbers
    let target = 7; // odd, not in array

    let start = Instant::now();
    let _ = arr.iter().position(|&x| x == target);
    let linear_time = start.elapsed();

    let start = Instant::now();
    let _ = arr.binary_search(&target);
    let binary_time = start.elapsed();

    println!("Linear: {:?}", linear_time);
    println!("Binary: {:?}", binary_time);
}
```

Resultados típicos:

| Método | Comparaciones | Tiempo |
|--------|:------------:|:------:|
| Lineal | 10,000,000 | ~15 ms |
| Binaria | 24 | ~50 ns |

Speedup: **~300,000×**. La binaria es 24 comparaciones independientemente
de $n$ (el target no existe, así que recorre toda la profundidad del
árbol de decisión). La lineal debe recorrer los 10 millones de elementos.

Con $n = 10^9$: lineal ~1.5 s, binaria ~100 ns (30 comparaciones).
La brecha crece linealmente con $n$.

</details>

---

### Ejercicio 10 — Recursiva vs iterativa en GDB

Compila la versión recursiva en C con `-g -O0`. Busca el valor 72 en el
array de la sección 5. Pon un breakpoint en `binary_search_rec` con
condición `lo == hi`. Observa el backtrace.

Luego recompila con `-O2` y repite. ¿Cambia el backtrace?

<details>
<summary>¿El compilador aplica TCO a la búsqueda binaria?</summary>

Con `-O0`:

```
(gdb) bt
#0  binary_search_rec (arr=..., lo=9, hi=9, target=72)
#1  binary_search_rec (arr=..., lo=9, hi=10, target=72)
#2  binary_search_rec (arr=..., lo=6, hi=10, target=72)
#3  binary_search_rec (arr=..., lo=0, hi=10, target=72)
#4  binary_search (arr=..., n=11, target=72)
#5  main ()
```

6 frames: main + wrapper + 4 llamadas recursivas.

Con `-O2`:

```
(gdb) bt
#0  binary_search (arr=..., n=11, target=72)
#1  main ()
```

Solo 2 frames. GCC aplicó TCO y posiblemente inlining — toda la
recursión se convirtió en un bucle dentro de `binary_search`. El
backtrace lo confirma: no hay frames intermedios de `binary_search_rec`.

Esto demuestra que para la búsqueda binaria recursiva, la distinción
recursiva/iterativa es puramente estilística cuando se compila con
optimizaciones.

</details>
