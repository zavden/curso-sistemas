# T01 — Notación O, Ω, Θ

---

## 1. ¿Por qué medir algoritmos?

Dos algoritmos que resuelven el mismo problema pueden tardar tiempos
radicalmente distintos:

```
Buscar un elemento en n datos:

  Búsqueda lineal:   revisa uno por uno       → ~n operaciones
  Búsqueda binaria:  divide a la mitad cada vez → ~log₂(n) operaciones

  Para n = 1,000,000:
    Lineal:   hasta 1,000,000 comparaciones
    Binaria:  hasta 20 comparaciones

    Diferencia: 50,000x
```

No medimos en segundos porque eso depende del hardware. Medimos en **cómo
crece el trabajo** cuando crece la entrada — eso es independiente de la
máquina.

---

## 2. Notación O grande (Big-O) — cota superior

### Definición formal

f(n) ∈ O(g(n)) si existen constantes c > 0 y n₀ ≥ 0 tales que:

```
f(n) ≤ c · g(n)    para todo n ≥ n₀
```

En palabras: a partir de algún punto (n₀), f(n) nunca crece más rápido
que g(n) (multiplicada por una constante).

### Interpretación intuitiva

O(g(n)) describe el **peor caso** de crecimiento — un **límite superior**.

```
"Este algoritmo es O(n²)"
  = "en el peor caso, el tiempo crece a lo sumo como n²"
  = "si duplico la entrada, el tiempo se multiplica como máximo por 4"
```

### Ejemplo gráfico

```
Tiempo
  │
  │                              c·n²  ─── cota superior
  │                           ╱
  │                        ╱
  │                     ╱
  │              f(n) ╱   ← la función real
  │            ╱   ╱
  │         ╱  ╱
  │      ╱╱
  │   ╱╱
  │ ╱╱
  ├──────────────────────────── n
  n₀

  A partir de n₀, f(n) siempre está debajo de c·g(n).
  → f(n) ∈ O(g(n))
```

### Ejemplo formal

Demostrar que f(n) = 3n² + 5n + 2 es O(n²):

```
Necesitamos: 3n² + 5n + 2 ≤ c · n²   para todo n ≥ n₀

Para n ≥ 1:
  3n² + 5n + 2  ≤  3n² + 5n² + 2n²   (porque n ≤ n² y 2 ≤ 2n² para n ≥ 1)
                =  10n²

Entonces: c = 10, n₀ = 1.
3n² + 5n + 2 ≤ 10n² para todo n ≥ 1.   ∴ f(n) ∈ O(n²)  ✓
```

---

## 3. Notación Ω (Omega) — cota inferior

### Definición formal

f(n) ∈ Ω(g(n)) si existen constantes c > 0 y n₀ ≥ 0 tales que:

```
f(n) ≥ c · g(n)    para todo n ≥ n₀
```

En palabras: f(n) crece **al menos** tan rápido como g(n).

### Interpretación intuitiva

Ω describe el **mejor caso** de crecimiento — un **límite inferior**.

```
"Este algoritmo es Ω(n)"
  = "incluso en el mejor caso, necesita al menos tiempo proporcional a n"
  = "no importa la suerte que tenga, tiene que mirar al menos n elementos"
```

### Ejemplo

Búsqueda lineal:
- **O(n)**: en el peor caso, revisa los n elementos
- **Ω(1)**: en el mejor caso, el elemento está en la primera posición

La búsqueda lineal es Ω(1) y O(n) — su rendimiento varía entre ambos extremos.

### Ejemplo formal

Demostrar que f(n) = 3n² + 5n + 2 es Ω(n²):

```
Necesitamos: 3n² + 5n + 2 ≥ c · n²   para todo n ≥ n₀

Para n ≥ 0:
  3n² + 5n + 2  ≥  3n²   (los términos extra son positivos)

Entonces: c = 3, n₀ = 0.
3n² + 5n + 2 ≥ 3n² para todo n ≥ 0.   ∴ f(n) ∈ Ω(n²)  ✓
```

---

## 4. Notación Θ (Theta) — cota ajustada

### Definición formal

f(n) ∈ Θ(g(n)) si existen constantes c₁, c₂ > 0 y n₀ ≥ 0 tales que:

```
c₁ · g(n) ≤ f(n) ≤ c₂ · g(n)    para todo n ≥ n₀
```

Equivalentemente: f(n) ∈ Θ(g(n)) si y solo si f(n) ∈ O(g(n)) **y**
f(n) ∈ Ω(g(n)).

### Interpretación intuitiva

Θ dice que f(n) crece **exactamente** como g(n) (salvo constantes).

```
"Este algoritmo es Θ(n²)"
  = "el tiempo siempre crece como n², ni más ni menos"
  = "en el mejor Y peor caso, el crecimiento es cuadrático"
```

### Ejemplo

```
f(n) = 3n² + 5n + 2

Es O(n²):  3n² + 5n + 2 ≤ 10n²  para n ≥ 1    (cota superior)
Es Ω(n²):  3n² + 5n + 2 ≥ 3n²   para n ≥ 0    (cota inferior)

∴ f(n) ∈ Θ(n²)   (cota ajustada)
```

### Relación entre las tres

```
        Ω(g(n))                O(g(n))
     cota inferior          cota superior
          │                      │
          │    ┌─────────────┐   │
          └───▶│   Θ(g(n))   │◀──┘
               │ cota ajustada│
               └─────────────┘

  Θ = O ∩ Ω
```

```
  O(n²): conjunto de funciones que crecen como máximo como n²
    → incluye: n, n log n, n², 7n² + 3n
    → NO incluye: n³, 2ⁿ

  Ω(n²): conjunto de funciones que crecen al menos como n²
    → incluye: n², n³, 2ⁿ, n² + 100
    → NO incluye: n, n log n

  Θ(n²): funciones que crecen exactamente como n²
    → incluye: n², 3n² + 5n, n²/2 + 1000
    → NO incluye: n, n³, n² + n³
```

---

## 5. ¿Cuál usar?

| Notación | Dice | Uso práctico |
|----------|------|-------------|
| O(g) | "como máximo g" | El más usado — "¿cuánto puede tardar en el peor caso?" |
| Ω(g) | "al menos g" | Poco usado — "¿cuánto tarda como mínimo?" |
| Θ(g) | "exactamente g" | Preciso — "el tiempo siempre crece así" |

En la práctica, la industria y la mayoría de los textos usan **O** para todo,
incluso cuando técnicamente quieren decir Θ:

```
"Merge sort es O(n log n)"
  → Técnicamente preciso (no crece más que n log n)
  → Pero también es Θ(n log n) — siempre crece así
  → La comunidad dice "O" por costumbre

"Búsqueda lineal es O(n)"
  → Técnicamente: O(n) y Ω(1)
  → No es Θ(n) porque el mejor caso es O(1)
  → Aquí O es correcto y Θ sería incorrecto
```

Para este curso: usaremos **O** para el peor caso (lo estándar), y
mencionaremos Θ cuando el mejor y peor caso coincidan.

---

## 6. Reglas de simplificación

### Regla 1: Eliminar constantes multiplicativas

```
O(3n)    = O(n)
O(100n²) = O(n²)
O(n/2)   = O(n)
```

Las constantes no afectan el crecimiento asintótico. Un algoritmo que hace
3n operaciones y otro que hace 100n crecen igual — ambos son lineales.

¿Por qué? Porque siempre puedes elegir una c mayor:
```
f(n) = 100n  ≤  100 · n    → O(n) con c = 100
```

### Regla 2: Eliminar términos de menor orden

```
O(n² + n)        = O(n²)
O(n³ + n² + n)   = O(n³)
O(n + log n)     = O(n)
O(2ⁿ + n³)       = O(2ⁿ)
```

El término dominante es el que crece más rápido. Los demás son insignificantes
para n grande:

```
n = 1000:
  n²    = 1,000,000
  n     = 1,000
  n² + n = 1,001,000  ≈  n²  (n es el 0.1% del total)
```

### Regla 3: Sumar secuencias, multiplicar anidamientos

```c
// Secuencia: O(f) + O(g) = O(max(f, g))
for (int i = 0; i < n; i++) { ... }    // O(n)
for (int j = 0; j < n; j++) { ... }    // O(n)
// Total: O(n) + O(n) = O(n)

// Anidamiento: O(f) · O(g) = O(f · g)
for (int i = 0; i < n; i++) {          // n veces
    for (int j = 0; j < n; j++) {      //   n veces cada una
        // ...                          //   → n × n = n²
    }
}
// Total: O(n) × O(n) = O(n²)
```

### Regla 4: Logaritmos — la base no importa

```
O(log₂ n) = O(log₁₀ n) = O(ln n) = O(log n)
```

Porque log_a(n) = log_b(n) / log_b(a), y 1/log_b(a) es una constante.

Por convención, `log n` sin base en complejidad significa cualquier
logaritmo (usualmente base 2 en CS).

### Tabla de simplificación

| Expresión | Simplifica a | Motivo |
|-----------|-------------|--------|
| 5n + 3 | O(n) | Constantes + término menor |
| n² + 1000n | O(n²) | n² domina |
| n · log n + n | O(n log n) | n log n domina |
| 3ⁿ + n¹⁰⁰ | O(3ⁿ) | Exponencial domina polinomial |
| log(n²) | O(log n) | log(n²) = 2 log n → constante × log n |
| log(n!) | O(n log n) | Aproximación de Stirling |

---

## 7. Jerarquía de crecimiento

Las clases de complejidad están ordenadas:

```
O(1) < O(log n) < O(√n) < O(n) < O(n log n) < O(n²) < O(n³) < O(2ⁿ) < O(n!)
```

### Valores concretos

| n | 1 | log n | √n | n | n log n | n² | n³ | 2ⁿ |
|---|---|-------|-----|---|---------|-----|-----|------|
| 1 | 1 | 0 | 1 | 1 | 0 | 1 | 1 | 2 |
| 10 | 1 | 3.3 | 3.2 | 10 | 33 | 100 | 1,000 | 1,024 |
| 100 | 1 | 6.6 | 10 | 100 | 664 | 10⁴ | 10⁶ | 10³⁰ |
| 1,000 | 1 | 10 | 31.6 | 1,000 | 10,000 | 10⁶ | 10⁹ | 10³⁰¹ |
| 10⁶ | 1 | 20 | 1,000 | 10⁶ | 2×10⁷ | 10¹² | 10¹⁸ | 10³⁰¹⁰²⁹ |

```
Para n = 10⁶ y 10⁹ operaciones/segundo:

  O(log n):     ~20 ops        → instantáneo
  O(n):         10⁶ ops        → 0.001 segundos
  O(n log n):   2×10⁷ ops      → 0.02 segundos
  O(n²):        10¹² ops       → ~17 minutos
  O(n³):        10¹⁸ ops       → ~31 años
  O(2ⁿ):        imposible     → más que la edad del universo
```

### Regla práctica para competencias y entrevistas

| Complejidad | n máximo viable (~10⁸ ops/s, 1 segundo) |
|-------------|----------------------------------------|
| O(n!) | n ≤ 12 |
| O(2ⁿ) | n ≤ 25 |
| O(n³) | n ≤ 500 |
| O(n²) | n ≤ 10⁴ |
| O(n log n) | n ≤ 10⁶ |
| O(n) | n ≤ 10⁸ |
| O(log n) | n ≤ 10¹⁸ |
| O(1) | cualquier n |

---

## 8. Análisis de código: patrones comunes

### Patrón 1: Loop simple → O(n)

```c
int sum = 0;
for (int i = 0; i < n; i++) {
    sum += arr[i];
}
// n iteraciones → O(n)
```

### Patrón 2: Loops anidados → O(n²)

```c
for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
        matrix[i][j] = i + j;
    }
}
// n × n iteraciones → O(n²)
```

### Patrón 3: Loop interno dependiente → O(n²)

```c
for (int i = 0; i < n; i++) {
    for (int j = 0; j < i; j++) {   // j < i, no j < n
        // ...
    }
}
// Iteraciones: 0 + 1 + 2 + ... + (n-1) = n(n-1)/2 → O(n²)
```

### Patrón 4: Loop que divide → O(log n)

```c
int count = 0;
int x = n;
while (x > 0) {
    x = x / 2;
    count++;
}
// x se divide por 2 cada vez → log₂(n) iteraciones → O(log n)
```

### Patrón 5: Loop lineal dentro de loop logarítmico → O(n log n)

```c
for (int i = 1; i < n; i *= 2) {     // log n veces
    for (int j = 0; j < n; j++) {     // n veces
        // ...
    }
}
// log(n) × n → O(n log n)
```

### Patrón 6: Loop externo lineal, interno logarítmico → O(n log n)

```c
for (int i = 0; i < n; i++) {        // n veces
    int x = i;
    while (x > 0) {                   // log(i) veces
        x /= 2;
    }
}
// Σ log(i) para i=1..n = log(n!) ≈ n log n → O(n log n)
```

### Patrón 7: Dos punteros → O(n)

```c
int left = 0, right = n - 1;
while (left < right) {
    // ... avanzar left o retroceder right ...
    if (condition)
        left++;
    else
        right--;
}
// left y right se encuentran en el medio → O(n)
```

### Patrón 8: Recursión divide-and-conquer → depende

```c
// División por 2, trabajo O(n) en cada nivel
void merge_sort(int *arr, int n) {
    if (n <= 1) return;
    merge_sort(arr, n/2);            // mitad izquierda
    merge_sort(arr + n/2, n - n/2);  // mitad derecha
    merge(arr, n);                   // O(n) merge
}
// T(n) = 2T(n/2) + O(n) → O(n log n) [Master Theorem caso 2]
```

```c
// División por 2, trabajo O(1) en cada nivel
int binary_search(int *arr, int n, int target) {
    if (n == 0) return -1;
    int mid = n / 2;
    if (arr[mid] == target) return mid;
    if (target < arr[mid])
        return binary_search(arr, mid, target);
    else
        return binary_search(arr + mid + 1, n - mid - 1, target);
}
// T(n) = T(n/2) + O(1) → O(log n) [Master Theorem caso 1... bueno, caso 2 con f=1]
```

---

## 9. Errores comunes

### "O(n) es siempre más rápido que O(n²)"

No necesariamente para entradas pequeñas. Si algoritmo A hace `100n`
operaciones y algoritmo B hace `n²/2`, B es más rápido para n < 200:

```
n = 50:  A = 5000,  B = 1250  → B gana
n = 200: A = 20000, B = 20000 → empate
n = 1000: A = 100000, B = 500000 → A gana
```

La notación asintótica solo importa para n suficientemente grande. Por eso
los algoritmos de ordenamiento usan insertion sort (O(n²)) para subarrays
pequeños dentro de merge sort (O(n log n)).

### "Mi algoritmo es O(n²) y O(n)"

Técnicamente correcto (si es O(n), también es O(n²), porque O(n) ⊂ O(n²)),
pero inútil. Siempre se reporta la **cota más ajustada**:

```
O(n) ⊂ O(n log n) ⊂ O(n²) ⊂ O(n³) ⊂ ...

Si f(n) ∈ O(n), también está en O(n²), O(n³), O(2ⁿ), ...
Pero la información útil es O(n) — la más ajustada.
```

### "Big-O mide el tiempo en segundos"

No. Big-O mide la **tasa de crecimiento**, no el tiempo absoluto. Un
algoritmo O(n) puede ser más lento que uno O(n²) si la constante oculta
es grande. Big-O dice que **a partir de algún n**, el O(n) será más
rápido.

### Confundir O con Θ

```
"Búsqueda lineal es O(n)" — CORRECTO
"Búsqueda lineal es Θ(n)" — INCORRECTO para el caso general
  → El mejor caso es Θ(1), el peor caso es Θ(n)
  → Solo el peor caso es Θ(n)
```

```
"Merge sort es O(n log n)" — CORRECTO
"Merge sort es Θ(n log n)" — CORRECTO
  → Mejor caso, peor caso y caso promedio son todos Θ(n log n)
```

---

## 10. O, Ω, Θ en contexto de estructuras de datos

Para cada operación de un TAD, la complejidad depende de la implementación:

```
TAD Conjunto — operación "contains":

  Implementación        Peor caso    Caso promedio
  ─────────────────────────────────────────────────
  Array desordenado     O(n)         O(n)
  Array ordenado        O(log n)     O(log n)        ← búsqueda binaria
  BST balanceado        O(log n)     O(log n)
  BST no balanceado     O(n)         O(log n)        ← O ≠ Θ
  Tabla hash            O(n)         O(1)             ← O ≠ Θ
```

Las tablas hash son un caso interesante:
- **O(n)** en peor caso (todas las claves colisionan)
- **Θ(1)** en caso promedio (distribución uniforme)
- Se dice informalmente "O(1)" pero se refiere al caso promedio

---

## Ejercicios

### Ejercicio 1 — Simplificar expresiones

Simplifica a la notación Big-O más ajustada:

```
a) 5n³ + 100n² + 50
b) n + n/2 + n/4 + ... + 1
c) 3 · 2ⁿ + n¹⁰
d) n² · log(n) + n²
e) log(n³)
f) n · (n + 1) / 2
g) 1 + 2 + 4 + 8 + ... + n    (potencias de 2 hasta n)
h) √n + log(n)
```

**Predicción**: Antes de simplificar, identifica cuál término domina en cada
expresión.

<details><summary>Respuesta</summary>

| Expresión | Término dominante | Simplificado |
|-----------|------------------|-------------|
| a) 5n³ + 100n² + 50 | n³ | **O(n³)** |
| b) n + n/2 + n/4 + ... | n (serie geométrica → 2n) | **O(n)** |
| c) 3·2ⁿ + n¹⁰ | 2ⁿ (exponencial domina) | **O(2ⁿ)** |
| d) n²·log(n) + n² | n²·log(n) | **O(n² log n)** |
| e) log(n³) | = 3·log(n) | **O(log n)** |
| f) n·(n+1)/2 | = n²/2 + n/2 | **O(n²)** |
| g) 1+2+4+...+n | = 2n - 1 (serie geom.) | **O(n)** |
| h) √n + log(n) | √n (crece más rápido) | **O(√n)** |

Detalle de (b): n + n/2 + n/4 + ... = n · (1 + 1/2 + 1/4 + ...) = n · 2 = 2n → O(n).
Serie geométrica con razón 1/2 converge a 2.

Detalle de (g): 1 + 2 + 4 + ... + n = 2n - 1 (si n es potencia de 2).
Cada término duplica el anterior, y la suma siempre es menor que 2 veces
el último término.

</details>

---

### Ejercicio 2 — Determinar complejidad de fragmentos

¿Cuál es la complejidad de cada fragmento?

```c
// Fragmento A
for (int i = 0; i < n; i++) {
    for (int j = i; j < n; j++) {
        sum++;
    }
}

// Fragmento B
for (int i = 1; i <= n; i *= 2) {
    for (int j = 0; j < i; j++) {
        sum++;
    }
}

// Fragmento C
for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
        for (int k = 0; k < n; k++) {
            if (i == j && j == k) {
                sum++;
            }
        }
    }
}

// Fragmento D
int i = n;
while (i > 1) {
    i = i / 3;
}
```

**Predicción**: Antes de calcular, estima: ¿alguno de los fragmentos tiene
complejidad diferente a lo que parece a primera vista?

<details><summary>Respuesta</summary>

**Fragmento A: O(n²)**

```
j va desde i hasta n-1:
  i=0: n iteraciones
  i=1: n-1 iteraciones
  ...
  i=n-1: 1 iteración

Total: n + (n-1) + ... + 1 = n(n-1)/2 → O(n²)
```

Parece O(n²) y es O(n²). No engaña.

**Fragmento B: O(n)**

```
i toma valores: 1, 2, 4, 8, ..., n (potencias de 2)
j hace i iteraciones para cada valor de i:

Total: 1 + 2 + 4 + 8 + ... + n = 2n - 1 → O(n)
```

Engañoso: parece O(n log n) por el loop externo logarítmico, pero el loop
interno crece exponencialmente y la suma geométrica da O(n).

**Fragmento C: O(n³)**

Tiene tres loops anidados de n → parece O(n³). El `if` filtra, pero el
compilador sigue evaluando los tres loops. La condición `i == j && j == k`
se cumple solo n veces, pero la verificación se hace n³ veces.

El `sum++` se ejecuta O(n) veces, pero el **trabajo total** (incluyendo
comparaciones) es O(n³). La complejidad mide todo el trabajo, no solo
las operaciones "útiles".

**Fragmento D: O(log₃ n) = O(log n)**

```
i se divide por 3 cada vez: n, n/3, n/9, n/27, ...
Iteraciones: log₃(n) → O(log n)
```

La base del logaritmo no importa (Regla 4): O(log₃ n) = O(log n).

</details>

---

### Ejercicio 3 — ¿O, Ω o Θ?

Para cada afirmación, indica si es verdadera o falsa:

```
a) n² + n ∈ O(n²)
b) n² + n ∈ Ω(n²)
c) n² + n ∈ Θ(n²)
d) n² + n ∈ O(n)
e) n² + n ∈ Ω(n³)
f) n ∈ O(n²)
g) n² ∈ O(n)
h) 2ⁿ ∈ O(3ⁿ)
i) 3ⁿ ∈ O(2ⁿ)
```

**Predicción**: Identifica las que son "técnicamente correctas pero inútiles"
(cotas no ajustadas).

<details><summary>Respuesta</summary>

| Afirmación | V/F | Razón |
|-----------|-----|-------|
| a) n²+n ∈ O(n²) | **V** | n²+n ≤ 2n² para n≥1 |
| b) n²+n ∈ Ω(n²) | **V** | n²+n ≥ n² para todo n |
| c) n²+n ∈ Θ(n²) | **V** | O(n²) ∧ Ω(n²) → Θ(n²) |
| d) n²+n ∈ O(n) | **F** | n² crece más rápido que cn para cualquier c |
| e) n²+n ∈ Ω(n³) | **F** | n² crece más lento que n³ |
| f) n ∈ O(n²) | **V** | Correcto pero no ajustado — n ∈ O(n) es mejor |
| g) n² ∈ O(n) | **F** | n² crece más rápido que n |
| h) 2ⁿ ∈ O(3ⁿ) | **V** | 2ⁿ ≤ 3ⁿ para todo n (2 < 3) |
| i) 3ⁿ ∈ O(2ⁿ) | **F** | 3ⁿ/2ⁿ = (3/2)ⁿ → ∞, no acotable |

"Técnicamente correctas pero inútiles": **(f)** y **(h)**.

(f) n ∈ O(n²) es verdadera pero la cota ajustada es O(n).
(h) 2ⁿ ∈ O(3ⁿ) es verdadera pero la cota ajustada es O(2ⁿ).

Nota sobre (h) e (i): a diferencia de logaritmos (donde la base no importa),
en exponenciales la base **sí importa**: O(2ⁿ) ≠ O(3ⁿ). 2ⁿ crece más
lento que 3ⁿ, así que 2ⁿ ∈ O(3ⁿ) pero 3ⁿ ∉ O(2ⁿ).

</details>

---

### Ejercicio 4 — Analizar funciones recursivas

¿Cuál es la complejidad de cada función?

```c
// Función A: factorial
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// Función B: fibonacci ingenuo
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

// Función C: potencia rápida
int power(int base, int exp) {
    if (exp == 0) return 1;
    if (exp % 2 == 0) {
        int half = power(base, exp / 2);
        return half * half;
    }
    return base * power(base, exp - 1);
}
```

**Predicción**: Ordena las tres de menor a mayor complejidad. ¿Cuál crece
exponencialmente?

<details><summary>Respuesta</summary>

**Función A — O(n)**:

```
T(n) = T(n-1) + O(1)
     = T(n-2) + O(1) + O(1)
     = ... = n · O(1) = O(n)
```

Una llamada recursiva que reduce n en 1, con O(1) trabajo en cada nivel.

**Función B — O(2ⁿ)** (más precisamente O(φⁿ) donde φ ≈ 1.618):

```
T(n) = T(n-1) + T(n-2) + O(1)

Árbol de llamadas para fib(5):
                    fib(5)
                   /      \
              fib(4)      fib(3)
             /     \      /    \
         fib(3)  fib(2) fib(2) fib(1)
        /    \
    fib(2) fib(1)

Cada nivel duplica (aprox.) las llamadas → ~2ⁿ nodos
```

Crece exponencialmente. `fib(40)` ≈ 10⁹ llamadas. `fib(50)` ≈ 10¹⁰.

**Función C — O(log n)**:

```
T(n) = T(n/2) + O(1)  (caso par — la mayoría)
     = O(log n)
```

Cada llamada divide `exp` por 2 (caso par) o reduce en 1 (caso impar,
pero el siguiente será par). En el peor caso, alterna: n, n-1, (n-1)/2, ...
que sigue siendo O(log n).

Orden: **C** O(log n) < **A** O(n) < **B** O(2ⁿ).

Para n = 30:
- C: ~30 llamadas
- A: 30 llamadas
- B: ~10⁹ llamadas

</details>

---

### Ejercicio 5 — Complejidad de operaciones de TAD

Completa la tabla con la complejidad en el peor caso:

```
                    Array      Array       Lista      Tabla hash
Operación           desordenado ordenado   enlazada   (promedio)
────────────────────────────────────────────────────────────────
Acceder por índice  ?          ?          ?          N/A
Buscar              ?          ?          ?          ?
Insertar al final   ?          ?          ?          ?
Insertar al inicio  ?          ?          ?          ?
Eliminar            ?          ?          ?          ?
```

**Predicción**: Completa la tabla antes de ver la respuesta. ¿Cuál estructura
es mejor para cada operación?

<details><summary>Respuesta</summary>

| Operación | Array desord. | Array ord. | Lista enlaz. | Hash table |
|-----------|--------------|-----------|-------------|-----------|
| Acceder por índice | **O(1)** | **O(1)** | O(n) | N/A |
| Buscar | O(n) | **O(log n)** | O(n) | **O(1)** prom. |
| Insertar al final | **O(1)** amort. | O(n)* | O(n)** | **O(1)** prom. |
| Insertar al inicio | O(n)*** | O(n) | **O(1)** | **O(1)** prom. |
| Eliminar | O(n) | O(n) | O(1)**** | **O(1)** prom. |

Notas:
- \* Array ordenado insertar: O(n) por shift para mantener orden
- \** Lista insertar al final: O(n) si no hay puntero tail; O(1) con tail
- \*** Array insertar al inicio: O(n) por shift de todos los elementos
- \**** Lista eliminar: O(1) si ya tienes el puntero al nodo; O(n) si primero debes buscar

No hay estructura perfecta — cada una tiene tradeoffs. Elegir la
implementación correcta para un TAD depende de qué operaciones son
más frecuentes en tu caso de uso.

</details>

---

### Ejercicio 6 — Verificación experimental

Escribe un programa en C que mida el tiempo de un loop simple (O(n)),
un loop anidado (O(n²)), y un loop logarítmico (O(log n)) para
n = 10³, 10⁴, 10⁵, 10⁶:

```c
#include <stdio.h>
#include <time.h>

volatile int sink;  // evitar que el compilador optimice

void linear(int n) {
    for (int i = 0; i < n; i++) sink = i;
}

void quadratic(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            sink = i + j;
}

void logarithmic(int n) {
    int x = n;
    while (x > 0) { sink = x; x /= 2; }
}
```

Usa `clock()` para medir.

**Predicción**: Si el tiempo de `linear(10³)` es T, ¿cuánto tardará
`linear(10⁶)` aproximadamente? ¿Y `quadratic(10³)` vs `quadratic(10⁴)`?

<details><summary>Respuesta</summary>

```c
int main(void) {
    int sizes[] = {1000, 10000, 100000, 1000000};

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];
        clock_t start, end;

        start = clock();
        linear(n);
        end = clock();
        double t_lin = (double)(end - start) / CLOCKS_PER_SEC;

        start = clock();
        logarithmic(n);
        end = clock();
        double t_log = (double)(end - start) / CLOCKS_PER_SEC;

        printf("n=%7d  linear=%.6f  log=%.6f", n, t_lin, t_log);

        if (n <= 100000) {   // quadratic es muy lento para n=10⁶
            start = clock();
            quadratic(n);
            end = clock();
            double t_quad = (double)(end - start) / CLOCKS_PER_SEC;
            printf("  quadratic=%.6f", t_quad);
        }
        printf("\n");
    }
    return 0;
}
```

Predicciones:

**Linear**: si `linear(10³)` tarda T, `linear(10⁶)` tarda ~1000T.
Razón: n se multiplicó por 1000, O(n) → tiempo × 1000.

**Quadratic**: si `quadratic(10³)` tarda T, `quadratic(10⁴)` tarda ~100T.
Razón: n se multiplicó por 10, O(n²) → tiempo × 10² = × 100.

**Logarithmic**: si `logarithmic(10³)` tarda T, `logarithmic(10⁶)` tarda ~2T.
Razón: log(10⁶)/log(10³) = 20/10 = 2. Casi no cambia.

En la práctica los tiempos no escalan perfectamente (cache effects,
branch prediction, overhead de medición), pero la tendencia se observa.

Compilar con `gcc -O0` para que no optimice los loops:
```bash
gcc -O0 -std=c11 -o bench bench.c && ./bench
```

</details>

Limpieza:

```bash
rm -f bench
```

---

### Ejercicio 7 — Trampa del if dentro de loops

¿Cuál es la complejidad?

```c
void process(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == 0) {
            for (int j = 0; j < n; j++) {
                arr[j] += 1;
            }
        }
    }
}
```

**Predicción**: ¿Es O(n) porque el if rara vez se cumple? ¿O es O(n²)?
¿Importa cuántos ceros hay?

<details><summary>Respuesta</summary>

**Peor caso: O(n²)**.

Si todos los elementos son 0 (antes de que se modifiquen), el loop interno
se ejecuta n veces → n × n = n².

Pero hay una sutileza: el loop interno modifica el array, sumando 1 a cada
elemento. Después de la primera vez que `arr[i] == 0` dispara el loop
interno, todos los elementos pasan a ser ≥ 1. Entonces la condición no se
cumple más... ¿o sí?

Depende de los valores iniciales. Si `arr = {0, -1, 0, -1, ...}`, después
del primer disparo los -1 pasan a 0, disparando otra vez al llegar a ellos.

**La complejidad Big-O es siempre sobre el peor caso de los datos de
entrada**, no sobre el caso "típico". El peor caso (todos ceros, con
modificaciones que crean más ceros) es O(n²).

Si se pregunta "¿cuál es la complejidad si el if se cumple k veces?":
O(n · k). Si k es constante → O(n). Si k = O(n) → O(n²).

Big-O sin aclaración = **peor caso sobre todos los inputs posibles**.

</details>

---

### Ejercicio 8 — Demostración formal

Demuestra formalmente (encontrando c y n₀) que:

```
a) 2n + 10 ∈ O(n)
b) n² ∉ O(n)
c) n log n ∈ Ω(n)
```

**Predicción**: Para (b), ¿qué técnica usarás para demostrar que algo
**no** está en O?

<details><summary>Respuesta</summary>

**a) 2n + 10 ∈ O(n)**:

Necesitamos: 2n + 10 ≤ c·n para todo n ≥ n₀.

```
Para n ≥ 10:  10 ≤ n
  2n + 10 ≤ 2n + n = 3n

c = 3, n₀ = 10.   ∴ 2n + 10 ∈ O(n)  ✓
```

**b) n² ∉ O(n)**:

Por contradicción. Supongamos que n² ∈ O(n). Entonces existen c, n₀ tales que:

```
n² ≤ c·n   para todo n ≥ n₀
n  ≤ c     para todo n ≥ n₀   (dividir ambos lados por n)
```

Pero n crece sin límite y c es una constante fija. Para n > c, la
desigualdad se viola. Contradicción.

∴ n² ∉ O(n)  ✓

La técnica: **contradicción**. Asumir que sí está en O, derivar que una
variable que crece sin límite está acotada por una constante → imposible.

**c) n log n ∈ Ω(n)**:

Necesitamos: n log n ≥ c·n para todo n ≥ n₀.

```
Para n ≥ 2:  log₂(n) ≥ 1
  n · log₂(n) ≥ n · 1 = n

c = 1, n₀ = 2.   ∴ n log n ∈ Ω(n)  ✓
```

</details>

---

### Ejercicio 9 — Complejidad en Rust

¿Cuál es la complejidad de cada operación del `Vec<T>` de Rust?

```rust
let mut v = Vec::new();

v.push(42);           // a)
v.pop();              // b)
v.insert(0, 99);      // c)
v.remove(0);          // d)
v[500];               // e)
v.contains(&42);      // f)
v.sort();             // g)
v.binary_search(&42); // h)
v.iter().sum::<i32>(); // i)
v.retain(|&x| x > 0); // j)
```

**Predicción**: Clasifica cada operación antes de ver la respuesta.

<details><summary>Respuesta</summary>

| Operación | Complejidad | Razón |
|-----------|------------|-------|
| a) `push` | **O(1)** amortizado | Append al final, realloc duplica capacidad |
| b) `pop` | **O(1)** | Quita el último, decrementa len |
| c) `insert(0, ..)` | **O(n)** | Shift de todos los elementos a la derecha |
| d) `remove(0)` | **O(n)** | Shift de todos los elementos a la izquierda |
| e) `v[500]` | **O(1)** | Acceso directo por índice |
| f) `contains` | **O(n)** | Búsqueda lineal |
| g) `sort` | **O(n log n)** | Timsort (merge sort adaptativo, estable) |
| h) `binary_search` | **O(log n)** | Requiere vec ordenado |
| i) `iter().sum()` | **O(n)** | Recorre todos los elementos |
| j) `retain` | **O(n)** | Recorre y filtra in-place |

`push` es O(1) **amortizado**: la mayoría de las veces es O(1), pero
cuando el Vec necesita crecer, hace realloc que es O(n). Promediando
sobre n pushes, el costo por push es O(1). Esto se analizará en detalle
en T03 (análisis amortizado).

`insert(0, ..)` y `remove(0)` son O(n) — para insertar/eliminar del
inicio eficientemente, se necesita un `VecDeque` (O(1) amortizado).

</details>

---

### Ejercicio 10 — Identificar la complejidad por el patrón

Sin calcular, identifica la complejidad de cada fragmento por el patrón
que reconoces (de la sección 8):

```c
// A
int x = n;
while (x > 1) x /= 2;

// B
for (int i = 0; i < n; i++)
    for (int j = 0; j < m; j++)
        sum++;

// C
for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++)
        for (int k = 0; k < n; k++)
            sum++;

// D
for (int i = 0; i < n; i++) {
    int j = 1;
    while (j < n) j *= 2;
}

// E
void f(int n) {
    if (n <= 0) return;
    f(n - 1);
    f(n - 1);
}
```

**Predicción**: Identifica cada uno. ¿Cuál es el más rápido y cuál el más
lento para n grande?

<details><summary>Respuesta</summary>

| Fragmento | Patrón | Complejidad |
|-----------|--------|-------------|
| A | Dividir por 2 | **O(log n)** |
| B | Dos loops independientes con distinto límite | **O(n · m)** |
| C | Tres loops anidados | **O(n³)** |
| D | Loop lineal × loop logarítmico | **O(n log n)** |
| E | Recursión que se bifurca en 2, profundidad n | **O(2ⁿ)** |

Orden (más rápido a más lento):

```
A: O(log n) < D: O(n log n) < B: O(nm)* < C: O(n³) < E: O(2ⁿ)
```

\* B depende de la relación entre n y m. Si m = n, es O(n²) (entre D y C).

E es exponencial — para n = 30, hace ~10⁹ llamadas. Para n = 50,
~10¹⁵ — impracticable. Es el mismo patrón que fibonacci ingenuo
(ejercicio 4).

El árbol de E:
```
          f(3)
         /    \
      f(2)    f(2)
     /   \    /   \
  f(1) f(1) f(1) f(1)
  / \  / \  / \  / \
f(0)...  ...  ...  ...

Nivel 0: 1 llamada
Nivel 1: 2 llamadas
Nivel 2: 4 llamadas
...
Nivel n: 2ⁿ llamadas
Total: 2⁰ + 2¹ + ... + 2ⁿ = 2ⁿ⁺¹ - 1 → O(2ⁿ)
```

</details>
