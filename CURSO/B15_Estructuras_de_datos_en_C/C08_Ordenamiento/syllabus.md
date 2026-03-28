# C08 — Ordenamiento

**Objetivo**: Comprender y comparar los principales algoritmos de ordenamiento,
desde los cuadráticos O(n²) hasta los eficientes O(n log n) y lineales O(n),
incluyendo las funciones de biblioteca de C (`qsort`) y Rust (`sort`/`sort_unstable`).

**Prerequisitos**: C03 (recursión — merge sort y quicksort son recursivos),
C07 (heaps — heapsort), C01 S02 (complejidad algorítmica).

---

## S01 — Fundamentos de ordenamiento

| Tópico | Contenido |
|--------|-----------|
| **T01 — Terminología** | Estabilidad, in-place, adaptativo, interno vs externo. |
| **T02 — Límite inferior** | Ω(n log n) para comparaciones. ▸ **Demostración formal**: prueba completa por árbol de decisión (un árbol binario con n! hojas tiene altura ≥ log₂(n!) = Ω(n log n), usando la aproximación de Stirling). |
| **T03 — Medir rendimiento** | Comparaciones, swaps, espacio, cache-friendliness. `clock()` en C, `Instant` en Rust. |

---

## S02 — Ordenamientos cuadráticos O(n²)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Bubble sort** | Algoritmo, optimización con bandera, análisis. |
| **T02 — Selection sort** | Encontrar mínimo, intercambiar. Siempre O(n²) comparaciones, O(n) swaps. |
| **T03 — Insertion sort** | Insertar en posición correcta. Adaptativo: O(n) si casi-ordenado. |
| **T04 — Cuándo usar cuadráticos** | Arrays pequeños (n < ~50), insertion sort como base para Timsort/introsort. |

**Ejercicios**: Los tres en C con contadores de comparaciones y swaps. Insertion sort
O(n) para casi-ordenados. Comparar tiempos para n = 10², 10³, 10⁴.

---

## S03 — Ordenamientos eficientes O(n log n)

| Tópico | Contenido |
|--------|-----------|
| **T01 — Merge sort** | Divide-and-conquer, merge, O(n log n) siempre, estable, O(n) espacio. En C y Rust. ▸ **Demostración formal**: derivar T(n) = 2T(n/2) + Θ(n) = Θ(n log n) por el Teorema Maestro (verificar caso 2: a=2, b=2, f(n)=Θ(n^{log_b a})) y por árbol de recursión (n trabajo por nivel × log n niveles). |
| **T02 — Quicksort** | Partición con pivote, O(n log n) promedio, O(n²) peor. Pivote: primero, último, mediana de tres, aleatorio. ▸ **Demostración formal**: análisis de caso promedio — derivar E[comparaciones] = 2n·Hₙ ≈ 2n·ln n ≈ 1.39·n·log₂ n usando probabilidad de que el i-ésimo y j-ésimo elementos se comparen: 2/(j−i+1). |
| **T03 — Quicksort optimizaciones** | Cutoff a insertion sort, partición de tres vías (Dutch National Flag), quicksort aleatorizado. ▸ **Demostración formal**: probar que quicksort aleatorizado tiene complejidad esperada O(n log n) independientemente de la entrada (la aleatorización elimina la dependencia del peor caso en el input). |
| **T04 — Heapsort** | O(n log n) garantizado, in-place, no estable. Referencia a C07. |
| **T05 — Shell sort** | Generalización de insertion sort con gaps. Secuencias de Knuth, Sedgewick. |
| **T06 — Comparación** | Tabla: estabilidad, espacio, peor caso, rendimiento práctico, cache. |

**Ejercicios**: Merge sort en C y Rust. Quicksort con 3 pivotes, comparar. Merge vs
quick vs heap para n = 10⁵ (aleatorio, ordenado, casi-ordenado).

---

## S04 — Ordenamientos lineales y funciones de biblioteca

| Tópico | Contenido |
|--------|-----------|
| **T01 — Counting sort** | Rango conocido [0, k]. O(n + k). Estable. |
| **T02 — Radix sort** | Ordenar por dígitos (LSD/MSD), counting sort como subrutina. O(d·(n + k)). |
| **T03 — qsort en C** | Firma, comparador, `void *` casting. No garantiza quicksort. |
| **T04 — Ordenamiento en Rust** | `sort` (Timsort, estable), `sort_unstable` (pdqsort, más rápido), closures como comparadores. |
| **T05 — C vs Rust** | `qsort` (indirección, no inline) vs `.sort()` (monomorphization, inline). Benchmark para 10⁶ elementos. |

**Ejercicios**: Radix sort para enteros en C. Comparar `qsort` vs `.sort_unstable()`.
Ordenar structs por múltiples campos.

---

## Demostraciones formales pendientes (README_EXTRA.md)

| Ubicación | Contenido |
|-----------|-----------|
| `S01_Fundamentos_de_ordenamiento/T02_Limite_inferior/README_EXTRA.md` | Prueba completa por árbol de decisión (un árbol binario con n! hojas tiene altura ≥ log₂(n!) = Ω(n log n), usando la aproximación de Stirling). |
| `S03_Ordenamientos_eficientes/T01_Merge_sort/README_EXTRA.md` | Derivar T(n) = 2T(n/2) + Θ(n) = Θ(n log n) por el Teorema Maestro (caso 2) y por árbol de recursión (n trabajo por nivel × log n niveles). |
| `S03_Ordenamientos_eficientes/T02_Quicksort/README_EXTRA.md` | Análisis de caso promedio — derivar E[comparaciones] = 2n·Hₙ ≈ 1.39·n·log₂ n usando probabilidad de comparación entre i-ésimo y j-ésimo: 2/(j−i+1). |
| `S03_Ordenamientos_eficientes/T03_Quicksort_optimizaciones/README_EXTRA.md` | Probar que quicksort aleatorizado tiene complejidad esperada O(n log n) independientemente de la entrada. |
