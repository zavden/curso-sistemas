# T02 — Análisis de mejor, peor y caso promedio

---

## 1. Un mismo algoritmo, tres comportamientos

La complejidad de un algoritmo no es un número fijo — depende de la **entrada
concreta**. El mismo algoritmo puede ser rápido o lento según los datos:

```c
// Búsqueda lineal: buscar target en arr[0..n-1]
int linear_search(int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}
```

```
arr = [5, 2, 8, 1, 9, 3, 7, 4, 6, 0]    n = 10

Buscar 5 (primer elemento):   1 comparación     ← mejor caso
Buscar 6 (penúltimo):         9 comparaciones
Buscar 0 (último):            10 comparaciones   ← peor caso
Buscar 42 (no existe):        10 comparaciones   ← peor caso
Buscar un elemento aleatorio: ~5 comparaciones   ← caso promedio
```

Por eso se analizan tres escenarios:

| Caso | Pregunta que responde | Notación habitual |
|------|----------------------|-------------------|
| **Mejor caso** | ¿Cuánto tarda como mínimo? | Ω o "best case O" |
| **Peor caso** | ¿Cuánto tarda como máximo? | O (Big-O) |
| **Caso promedio** | ¿Cuánto tarda "normalmente"? | O promedio o Θ promedio |

---

## 2. Mejor caso (Best Case)

El mejor caso es la entrada que hace que el algoritmo termine lo más rápido
posible.

### Búsqueda lineal

```
Mejor caso: el target está en arr[0]
  → 1 comparación → Ω(1)
```

### Insertion sort

```c
void insertion_sort(int *arr, int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}
```

```
Mejor caso: array ya ordenado [1, 2, 3, 4, 5]
  → el while nunca se ejecuta (arr[j] ≤ key siempre)
  → solo el for externo: n-1 iteraciones → Ω(n)
```

### ¿Es útil el mejor caso?

Rara vez. Saber que un algoritmo **puede** ser rápido no dice mucho — uno
querría saber si lo será en la práctica. El mejor caso es útil en dos
situaciones:

1. **Algoritmos adaptativos**: si los datos están "casi" en el mejor caso,
   el algoritmo se beneficia. Insertion sort es O(n) para datos casi-ordenados
   — eso es una ventaja real.

2. **Cotas inferiores del problema**: si el mejor caso de **cualquier**
   algoritmo para un problema es Ω(n), entonces ningún algoritmo puede
   resolver el problema en menos de n pasos (ej: encontrar el mínimo
   requiere mirar todos los elementos → Ω(n)).

---

## 3. Peor caso (Worst Case)

El peor caso es la entrada que hace que el algoritmo tarde lo más posible.

### Búsqueda lineal

```
Peor caso: target no está en el array, o está en arr[n-1]
  → n comparaciones → O(n)
```

### Insertion sort

```
Peor caso: array ordenado en orden inverso [5, 4, 3, 2, 1]
  → cada elemento se compara con todos los anteriores
  → 1 + 2 + 3 + ... + (n-1) = n(n-1)/2 comparaciones → O(n²)
```

### Quicksort

```
Peor caso: pivote siempre es el menor o mayor elemento
  → particiones de tamaño 0 y n-1
  → n + (n-1) + (n-2) + ... + 1 = n(n-1)/2 → O(n²)

  Entrada que lo causa: array ya ordenado con pivote = primer elemento
    [1, 2, 3, 4, 5] con pivote arr[0]:
      Partición: [] [1] [2, 3, 4, 5]  → subproblema de n-1
```

### ¿Por qué importa el peor caso?

Es la **garantía**. Si un sistema médico, financiero o de tiempo real usa un
algoritmo, necesita saber que **nunca** tardará más de X — no importa los
datos que reciba:

```
Escenario: servidor web procesando queries con búsqueda
  Peor caso O(n):      puedo garantizar respuesta en <100ms para n=10⁶
  Peor caso O(n²):     para n=10⁶, podría tardar 10⁶ × 10⁶ = 10¹² ops
                        → potencialmente minutos → inaceptable

Si el caso promedio es O(1) pero el peor caso es O(n²):
  → un atacante podría enviar la entrada que dispara el peor caso
  → denial of service (ej: hash flooding attacks contra tablas hash)
```

Por eso Big-O (peor caso) es la notación más usada — da la garantía más
fuerte.

---

## 4. Caso promedio (Average Case)

El caso promedio asume una **distribución de probabilidad** sobre las entradas
posibles y calcula el costo esperado.

### Búsqueda lineal — caso promedio

Suposiciones:
- El target está en el array (existe)
- Cada posición es igualmente probable (distribución uniforme)

```
Costo de encontrar el elemento en posición i: i+1 comparaciones

Costo promedio = (1/n) · Σ(i+1) para i = 0..n-1
               = (1/n) · (1 + 2 + 3 + ... + n)
               = (1/n) · n(n+1)/2
               = (n+1)/2
               ≈ n/2

→ O(n) en promedio   (la mitad de n, pero sigue siendo lineal)
```

Si incluimos la posibilidad de que el target **no** esté en el array:
- Si la probabilidad de que exista es p:
  - Costo promedio = p · (n+1)/2 + (1-p) · n
  - Para p = 0.5: costo ≈ 3n/4 → sigue siendo O(n)

### Quicksort — caso promedio

Con pivote aleatorio y distribución uniforme de inputs:

```
Caso promedio: O(n log n)

Intuitivamente: con pivote aleatorio, la partición divide
el array "razonablemente bien" la mayoría de las veces.
Las particiones muy desbalanceadas son raras.

Formalmente: T(n) = (1/n) · Σ [T(k) + T(n-1-k)] + O(n)  para k=0..n-1
           → T(n) = O(n log n)
```

Quicksort es interesante porque el peor caso (O(n²)) y el caso promedio
(O(n log n)) son diferentes:

| | Mejor | Promedio | Peor |
|---|-------|---------|------|
| Quicksort | O(n log n) | O(n log n) | O(n²) |
| Merge sort | O(n log n) | O(n log n) | O(n log n) |
| Insertion sort | O(n) | O(n²) | O(n²) |

Merge sort tiene los tres casos iguales — es **predecible**. Quicksort
es más rápido en la práctica (mejores constantes, cache-friendly) pero
tiene un peor caso vulnerable.

### ¿Cuándo importa el caso promedio?

Cuando el peor caso es raro y quieres saber el rendimiento **esperado** en
uso real:

```
Tablas hash:
  Peor caso:   O(n)     todas las claves colisionan
  Promedio:    O(1)     con buena función hash
  → Se usan extensamente porque el promedio es excelente
    y el peor caso se puede mitigar (hash aleatorizado, rehash)

BST no balanceado:
  Peor caso:   O(n)     insertar datos ya ordenados → lista
  Promedio:    O(log n)  inserciones aleatorias → árbol balanceado
  → Para datos aleatorios funciona bien; para datos ordenados, no
  → Solución: usar AVL o RB tree (peor caso O(log n) garantizado)
```

---

## 5. Caso promedio: las suposiciones importan

El caso promedio **depende de qué suposiciones haces** sobre la entrada.
Diferentes suposiciones dan diferentes resultados:

### Ejemplo: quicksort

```
Suposición 1: "Todas las permutaciones son igualmente probables"
  → Caso promedio: O(n log n)

Suposición 2: "Los datos vienen mayormente ordenados" (ej: logs con timestamp)
  → Caso promedio: más cercano a O(n²) si el pivote es el primero

Suposición 3: "Un adversario elige la entrada"
  → Caso promedio: O(n²) (el adversario siempre puede forzar el peor caso)
```

Por eso el caso promedio siempre debe declarar sus suposiciones. Sin ellas,
el análisis no tiene sentido.

### Aleatorización como defensa

Para que el caso promedio no dependa de la distribución de la entrada, se
puede **aleatorizar el algoritmo**:

```c
// Quicksort aleatorizado: pivote random
int random_pivot(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}
```

Con pivote aleatorio, el **caso esperado** (sobre la moneda del algoritmo,
no sobre los datos) es O(n log n) **para cualquier entrada**. El adversario
ya no puede forzar el peor caso eligiendo la entrada — tendría que
predecir las elecciones aleatorias.

---

## 6. Tabla comparativa de algoritmos comunes

| Algoritmo | Mejor | Promedio | Peor | Comentario |
|-----------|-------|---------|------|------------|
| Búsqueda lineal | O(1) | O(n) | O(n) | Target al inicio vs no existe |
| Búsqueda binaria | O(1) | O(log n) | O(log n) | Target en medio vs extremo |
| Insertion sort | O(n) | O(n²) | O(n²) | Ordenado vs invertido |
| Selection sort | O(n²) | O(n²) | O(n²) | Siempre n²/2 comparaciones |
| Merge sort | O(n log n) | O(n log n) | O(n log n) | Siempre divide + merge |
| Quicksort | O(n log n) | O(n log n) | O(n²) | Buena partición vs degenerada |
| Heapsort | O(n log n) | O(n log n) | O(n log n) | Siempre O(n log n) |
| Hash lookup | O(1) | O(1) | O(n) | Sin colisiones vs todas colisionan |
| BST lookup | O(1) | O(log n) | O(n) | En raíz vs lista degenerada |
| AVL lookup | O(1) | O(log n) | O(log n) | Siempre balanceado |

Patrones notables:
- **Selection sort**: los tres casos son iguales — siempre hace n²/2 comparaciones, sin importar la entrada
- **Quicksort**: el peor caso difiere del promedio — la implementación importa (pivote aleatorio mitiga)
- **Hash table**: enorme diferencia entre promedio (O(1)) y peor caso (O(n))

---

## 7. Análisis de caso para estructuras de datos

Las operaciones de un TAD también tienen diferentes casos:

### Array dinámico (Vec/vector): push

```
Mejor caso:  O(1)     hay capacidad disponible
Peor caso:   O(n)     necesita realloc → copia n elementos
Promedio:    O(1)     amortizado (la mayoría no necesitan realloc)
```

### BST no balanceado: insert

```
Mejor caso:  O(1)     árbol vacío
Peor caso:   O(n)     insertar en secuencia → árbol es lista

  Insertar 1, 2, 3, 4, 5 en BST:
          1
           \
            2
             \
              3
               \
                4
                 \
                  5
  → Altura = n → búsqueda/inserción O(n)

Promedio:    O(log n)  inserciones aleatorias → árbol "razonablemente" balanceado
```

### Tabla hash: buscar

```
Mejor caso:  O(1)     sin colisión en el bucket
Peor caso:   O(n)     todos los elementos en el mismo bucket

  n claves con hash(k) = 0 para todas:
  bucket[0] → [k1] → [k2] → [k3] → ... → [kn]
  → lista enlazada de longitud n → búsqueda O(n)

Promedio:    O(1)     con buena función hash, load factor < 0.75
  → cada bucket tiene ~1 elemento → acceso directo
```

### ¿Cuál caso reportar?

| Contexto | Caso a reportar | Razón |
|----------|----------------|-------|
| Documentación de API | Peor caso | El usuario necesita la garantía |
| Comparación de algoritmos | Peor caso + promedio | Para elegir el correcto |
| Benchmarking | Promedio (medido) | Para ver rendimiento real |
| Sistemas críticos | Peor caso exclusivamente | No se permiten picos |
| Entrevistas técnicas | Peor caso, luego promedio si difiere | Mostrar conocimiento |

---

## 8. Cuando el caso promedio cambia la elección

### Ejemplo: diccionario con n = 10⁶ entradas

Opciones:
- **AVL tree**: O(log n) garantizado para todo
- **Hash table**: O(1) promedio, O(n) peor

```
                    AVL           Hash table
Peor caso:          O(log n) = 20  O(n) = 10⁶
Promedio:           O(log n) = 20  O(1) = 1-3
Mejor caso:         O(1)           O(1)
Espacio:            O(n)           O(n)
Ordenado:           Sí             No
```

¿Cuál elegir?

- Si necesitas **orden** (iterar de menor a mayor): AVL
- Si necesitas **garantía** de peor caso (sistema crítico): AVL
- Si quieres **máxima velocidad promedio** y no hay riesgo adversarial: hash table
- Si hay riesgo de input adversarial (servidor público): hash table con hash aleatorizado (randomized → peor caso esperado O(1))

En la práctica, la mayoría de los programas usan hash tables (`HashMap` en
Rust, `unordered_map` en C++) porque el caso promedio O(1) domina el
rendimiento real.

---

## 9. Cómo medir experimentalmente

La teoría da la complejidad asintótica. La práctica da el rendimiento real
con constantes, cache effects y hardware:

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Medir la función para diferentes n
void benchmark(const char *name, void (*fn)(int *, int), int *sizes, int count) {
    printf("%-20s", name);
    for (int s = 0; s < count; s++) {
        int n = sizes[s];
        int *arr = malloc(n * sizeof(int));
        for (int i = 0; i < n; i++) arr[i] = rand();

        clock_t start = clock();
        fn(arr, n);
        clock_t end = clock();

        double ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
        printf("  n=%7d: %8.2f ms", n, ms);

        free(arr);
    }
    printf("\n");
}
```

### Calcular la tasa de crecimiento

Si mides T(n) y T(2n), la **razón T(2n)/T(n)** revela la complejidad:

```
T(2n)/T(n)  ≈ 1    → O(1) o O(log n)
T(2n)/T(n)  ≈ 2    → O(n)
T(2n)/T(n)  ≈ 2-3  → O(n log n)
T(2n)/T(n)  ≈ 4    → O(n²)
T(2n)/T(n)  ≈ 8    → O(n³)
```

¿Por qué? Si T(n) = c · n², entonces T(2n) = c · (2n)² = 4 · c · n² = 4 · T(n).

### Medir los tres casos

```c
// Peor caso de insertion sort: array invertido
for (int i = 0; i < n; i++) arr[i] = n - i;

// Mejor caso de insertion sort: array ordenado
for (int i = 0; i < n; i++) arr[i] = i;

// Caso promedio de insertion sort: array aleatorio
for (int i = 0; i < n; i++) arr[i] = rand();
```

Medir los tres con el mismo n revela la diferencia entre los casos.

---

## 10. Resumen visual

```
                            ┌─────────────────────────┐
                            │      ALGORITMO           │
                            │  (ej: búsqueda lineal)   │
                            └───────────┬─────────────┘
                                        │
                    ┌───────────────────┬┴───────────────────┐
                    │                   │                    │
              ┌─────▼─────┐     ┌──────▼──────┐     ┌──────▼──────┐
              │ MEJOR CASO │     │ CASO PROMEDIO│     │  PEOR CASO  │
              │            │     │              │     │             │
              │ Target en  │     │ Target en    │     │ Target no   │
              │ posición 0 │     │ posición n/2 │     │ existe      │
              │            │     │ (esperado)   │     │             │
              │   Ω(1)     │     │    Θ(n)      │     │    O(n)     │
              └────────────┘     └──────────────┘     └─────────────┘
                    │                   │                    │
                    │                   │                    │
              Poco útil           Rendimiento          Garantía
              (excepto para       real esperado        máxima
              adaptativos)                             (contratos,
                                                       SLAs)
```

---

## Ejercicios

### Ejercicio 1 — Identificar los tres casos

Para cada algoritmo, describe qué entrada produce el mejor caso, el
peor caso, y estima el caso promedio:

```
a) Búsqueda binaria en array ordenado
b) Bubble sort con optimización (flag de "no hubo swaps")
c) Eliminar un elemento de un array desordenado (buscar + swap con último)
d) Buscar el máximo en un array
```

**Predicción**: ¿Cuál de los cuatro tiene los tres casos iguales?

<details><summary>Respuesta</summary>

**a) Búsqueda binaria**:
- Mejor: target está en el medio → O(1)
- Peor: target no existe, o está en un extremo → O(log n)
- Promedio: ~log₂(n)/2 comparaciones → O(log n)

**b) Bubble sort optimizado**:
- Mejor: array ya ordenado → una pasada sin swaps, flag termina → O(n)
- Peor: array en orden inverso → n pasadas de n comparaciones → O(n²)
- Promedio: ~n²/4 swaps → O(n²)

**c) Eliminar de array desordenado**:
- Mejor: elemento en posición 0 → 1 comparación + 1 swap → O(1)
- Peor: elemento en la última posición o no existe → n comparaciones → O(n)
- Promedio: ~n/2 comparaciones → O(n)

**d) Buscar el máximo**: los tres casos son **iguales** — O(n).
Siempre hay que recorrer todos los elementos. No se puede saber que un
elemento es el máximo sin haber visto los demás. Sin importar la entrada,
siempre se hacen n-1 comparaciones.

</details>

---

### Ejercicio 2 — Quicksort: construir el peor caso

Dado un quicksort que siempre elige `arr[0]` como pivote, construye
un array de 7 elementos que cause el peor caso. Dibuja el árbol de
recursión.

**Predicción**: ¿El array ordenado es el peor caso? ¿Y el inverso?

<details><summary>Respuesta</summary>

Sí, ambos son peor caso:

**Array ordenado**: `[1, 2, 3, 4, 5, 6, 7]`, pivote = `arr[0]`

```
Pivote 1: partición [] | [2, 3, 4, 5, 6, 7]    → trabajo: 6 comparaciones
Pivote 2: partición [] | [3, 4, 5, 6, 7]        → trabajo: 5 comparaciones
Pivote 3: partición [] | [4, 5, 6, 7]            → trabajo: 4 comparaciones
Pivote 4: partición [] | [5, 6, 7]                → trabajo: 3
Pivote 5: partición [] | [6, 7]                    → trabajo: 2
Pivote 6: partición [] | [7]                        → trabajo: 1
Total: 6 + 5 + 4 + 3 + 2 + 1 = 21 = n(n-1)/2 → O(n²)
```

**Array inverso**: `[7, 6, 5, 4, 3, 2, 1]`, pivote = `arr[0]`

```
Pivote 7: partición [6, 5, 4, 3, 2, 1] | []    → trabajo: 6
Pivote 6: partición [5, 4, 3, 2, 1] | []        → trabajo: 5
...
Total: 21 = n(n-1)/2 → O(n²)
```

Ambos producen particiones maximalmente desbalanceadas (0 | n-1).
El caso ideal sería `[4, 2, 6, 1, 3, 5, 7]` donde cada pivote divide
en mitades iguales → O(n log n).

Solución al peor caso: pivote aleatorio, o mediana de tres (primero, medio,
último).

</details>

---

### Ejercicio 3 — Caso promedio de búsqueda lineal

Calcula el número promedio de comparaciones de búsqueda lineal bajo
dos suposiciones diferentes:

```
Suposición A: El target siempre está en el array, cualquier posición
              es igualmente probable.

Suposición B: El target tiene probabilidad 0.5 de estar en el array.
              Si está, cualquier posición es igualmente probable.
```

Calcula para n = 100.

**Predicción**: ¿Cuántas comparaciones en promedio para cada suposición?

<details><summary>Respuesta</summary>

**Suposición A**:

```
E[comparaciones] = (1/n) · Σ(i+1) para i=0..n-1
                 = (1/n) · n(n+1)/2
                 = (n+1)/2

Para n = 100: (100+1)/2 = 50.5 comparaciones
```

**Suposición B**:

```
Si está (prob 0.5): (n+1)/2 = 50.5 comparaciones
Si no está (prob 0.5): n = 100 comparaciones

E[comparaciones] = 0.5 · 50.5 + 0.5 · 100
                 = 25.25 + 50
                 = 75.25 comparaciones
```

La posibilidad de que el target no exista aumenta significativamente el
promedio (de 50.5 a 75.25) porque los "no encontrados" siempre cuestan n.

Ambas siguen siendo O(n), pero la constante importa en la práctica: la
suposición B da ~50% más trabajo que la A.

</details>

---

### Ejercicio 4 — Medir los tres casos experimentalmente

Escribe un programa que mida insertion sort para los tres casos con
n = 1000, 5000, 10000, 20000:

```c
// Genera array para cada caso
void fill_best(int *arr, int n)    { for (int i=0; i<n; i++) arr[i] = i; }
void fill_worst(int *arr, int n)   { for (int i=0; i<n; i++) arr[i] = n-i; }
void fill_random(int *arr, int n)  { for (int i=0; i<n; i++) arr[i] = rand(); }
```

**Predicción**: Si el peor caso para n=1000 tarda T ms, ¿cuánto tardará
para n=2000? ¿Y el mejor caso para n=2000 comparado con n=1000?

<details><summary>Respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

void insertion_sort(int *arr, int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

double measure(void (*fill)(int *, int), int n) {
    int *arr = malloc(n * sizeof(int));
    fill(arr, n);
    clock_t start = clock();
    insertion_sort(arr, n);
    clock_t end = clock();
    free(arr);
    return 1000.0 * (end - start) / CLOCKS_PER_SEC;
}

int main(void) {
    srand(42);
    int sizes[] = {1000, 5000, 10000, 20000};
    printf("%-8s %12s %12s %12s\n", "n", "Best(ms)", "Average(ms)", "Worst(ms)");

    for (int s = 0; s < 4; s++) {
        int n = sizes[s];
        // fill functions defined as above
        printf("%-8d %12.2f %12.2f %12.2f\n", n,
               measure(fill_best, n),
               measure(fill_random, n),
               measure(fill_worst, n));
    }
    return 0;
}
```

**Predicciones**:

Peor caso (O(n²)): T(2000)/T(1000) ≈ 4. Si n=1000 tarda T, n=2000 tarda ~4T.

Mejor caso (O(n)): T(2000)/T(1000) ≈ 2. El tiempo se duplica (lineal).

```
Resultados típicos (gcc -O0):
n         Best(ms)    Average(ms)   Worst(ms)
1000         0.01          0.80         1.60
5000         0.03         19.00        39.00
10000        0.05         76.00       155.00
20000        0.10        305.00       620.00
```

Observaciones:
- Worst ≈ 2 × Average (predice la teoría: n²/2 vs n²/4)
- Best es ~1000x más rápido que Worst para n=10000
- La razón T(20000)/T(10000) ≈ 4 confirma O(n²) para worst y average
- La razón T(20000)/T(10000) ≈ 2 confirma O(n) para best

</details>

Limpieza:

```bash
rm -f insertion_bench
```

---

### Ejercicio 5 — Hash table: peor caso vs promedio

Un servidor web usa una hash table para cachear respuestas. Analiza los
escenarios:

```
Caso 1: URLs normales (distribuidas uniformemente por hash)
  → 10⁶ URLs, load factor 0.7, chaining

Caso 2: Un atacante envía URLs diseñadas para colisionar
  → 10⁶ URLs todas con el mismo hash
```

**Predicción**: ¿Cuántas comparaciones por lookup en cada caso? ¿Cómo
se defiende contra el caso 2?

<details><summary>Respuesta</summary>

**Caso 1 — distribución uniforme**:
- Load factor α = 0.7: cada bucket tiene ~0.7 elementos en promedio
- Lookup exitoso: ~1 + α/2 ≈ 1.35 comparaciones
- Lookup fallido: ~1 + α ≈ 1.7 comparaciones
- **Prácticamente O(1)**

**Caso 2 — colisión adversarial**:
- Todos los 10⁶ elementos en un solo bucket → lista de longitud 10⁶
- Lookup: recorrer la lista completa → **O(n) = 10⁶ comparaciones**
- Un lookup tarda ~10⁶ veces más que en el caso 1
- Con suficientes requests, el servidor colapsa → **denial of service**

**Defensas**:
1. **Hash aleatorizado (SipHash)**: usar una semilla secreta para el hash.
   El atacante no puede predecir los valores hash → no puede forzar
   colisiones. Rust usa SipHash por defecto en `HashMap`.

2. **Switching a árbol balanceado**: Java 8+ convierte buckets de listas
   a árboles rojo-negro cuando una cadena supera ~8 elementos → peor caso
   O(log n) en lugar de O(n).

3. **Límite de load factor**: redimensionar agresivamente para mantener
   cadenas cortas.

Este es un ejemplo real donde el peor caso importa: **HashDoS** (2011)
afectó servidores PHP, Python, Ruby, Java. La solución fue cambiar a hashes
aleatorizados.

</details>

---

### Ejercicio 6 — ¿Importa la diferencia?

Para cada par de algoritmos, indica cuándo preferirías cada uno
considerando los tres casos:

```
a) Quicksort (avg O(n log n), worst O(n²))
   vs Merge sort (siempre O(n log n))

b) BST no balanceado (avg O(log n), worst O(n))
   vs AVL (siempre O(log n))

c) Array desordenado (insert O(1), search O(n))
   vs Array ordenado (insert O(n), search O(log n))
```

**Predicción**: ¿Hay algún caso donde el algoritmo con peor "peor caso"
sea la mejor elección?

<details><summary>Respuesta</summary>

**a) Quicksort vs Merge sort**:

- **Quicksort** cuando: datos en memoria, rendimiento promedio importa más que
  garantía. En la práctica, quicksort es ~20-30% más rápido que merge sort por
  mejores constantes y cache locality. Las implementaciones modernas (introsort)
  cambian a heapsort si detectan degeneración → peor caso O(n log n).

- **Merge sort** cuando: necesitas estabilidad (mantener orden relativo de iguales),
  necesitas garantía O(n log n), o los datos no caben en memoria (merge sort
  externo). Rust usa Timsort (variante de merge sort) para `.sort()`.

**b) BST vs AVL**:

- **BST no balanceado** cuando: los datos llegan en orden aleatorio (promedio
  O(log n)), no hay riesgo adversarial, y la simplificación de implementación vale
  la pena.

- **AVL** cuando: necesitas garantía O(log n), los datos pueden llegar ordenados
  (ej: insertando desde un archivo sorted), o es un sistema de producción.

**c) Array desordenado vs ordenado**:

- **Desordenado** cuando: muchas inserciones, pocas búsquedas. Log de eventos:
  append O(1) es perfecto, búsqueda O(n) es tolerable si es rara.

- **Ordenado** cuando: pocas inserciones, muchas búsquedas. Diccionario estático:
  se construye una vez, se busca muchas veces → O(log n) por búsqueda amortiza
  el costo O(n) de inserción.

**Sí**: quicksort (peor caso O(n²)) es frecuentemente mejor que merge sort
(peor caso O(n log n)) por las constantes más pequeñas en el caso promedio.

</details>

---

### Ejercicio 7 — Selection sort: ¿adaptativo?

Selection sort siempre hace n(n-1)/2 comparaciones, sin importar la entrada:

```c
void selection_sort(int *arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[min_idx])
                min_idx = j;
        }
        int tmp = arr[i];
        arr[i] = arr[min_idx];
        arr[min_idx] = tmp;
    }
}
```

**Predicción**: ¿Es verdad que los tres casos son idénticos en comparaciones?
¿Y en swaps? ¿Podría el mejor caso tener menos swaps que el peor?

<details><summary>Respuesta</summary>

**Comparaciones**: Sí, siempre n(n-1)/2. El loop interno recorre desde `i+1`
hasta `n-1` sin importar los valores → no hay early termination.

```
Mejor, peor, promedio: Θ(n²) comparaciones siempre.
```

**Swaps**: No, difieren.

- **Mejor caso** (array ya ordenado): en cada iteración, `min_idx == i` al
  final del loop interno → el swap intercambia `arr[i]` consigo mismo. Son
  n-1 swaps, pero todos son "no-ops" (misma posición). Si se agrega
  `if (min_idx != i)`, son **0 swaps** → O(1) swaps.

- **Peor caso** (para swaps): cada swap mueve un elemento fuera de posición.
  Siempre son como máximo n-1 swaps (uno por iteración del loop externo).

```
Comparaciones: siempre Θ(n²)
Swaps:         O(n) siempre (máximo n-1)
```

Selection sort es **no adaptativo** en comparaciones pero **siempre eficiente**
en swaps (O(n)). Esto lo hace útil cuando las comparaciones son baratas pero
los swaps son costosos (ej: mover structs grandes).

Contraste con insertion sort: adaptativo en comparaciones (O(n) para
casi-ordenados) pero puede hacer O(n²) shifts.

</details>

---

### Ejercicio 8 — Caso promedio de acceso a lista enlazada

Una lista enlazada simple de n nodos. Acceder al elemento en posición k
requiere recorrer k nodos desde la cabeza.

```
Suposición: cada posición es igualmente probable.
```

Calcula el número promedio de nodos visitados. Compara con un array.

**Predicción**: ¿Es el mismo resultado que la búsqueda lineal? ¿Por qué
o por qué no?

<details><summary>Respuesta</summary>

```
Costo de acceder a posición k: k+1 nodos recorridos (0-indexed)

E[nodos visitados] = (1/n) · Σ(k+1) para k=0..n-1
                   = (1/n) · (1 + 2 + ... + n)
                   = (1/n) · n(n+1)/2
                   = (n+1)/2
```

Para n = 100: promedio = 50.5 nodos.

Es exactamente el mismo resultado que la búsqueda lineal — y por la
misma razón: ambos recorren secuencialmente.

**Comparación con array**:

| Operación | Lista enlazada | Array |
|-----------|---------------|-------|
| Acceso por posición k | O(k) → promedio O(n) | **O(1)** siempre |
| Mejor caso | O(1) — posición 0 | O(1) — cualquier posición |
| Peor caso | O(n) — última posición | O(1) — cualquier posición |

El array gana en acceso aleatorio porque usa aritmética de punteros:
`arr[k] = base + k * sizeof(elem)` → un cálculo, sin recorrido.

La lista enlazada no tiene acceso directo — tiene que seguir punteros
uno por uno. Por eso las listas no son buenas para acceso aleatorio, pero
sí para inserciones/eliminaciones en posiciones conocidas (O(1) si tienes
el puntero al nodo).

</details>

---

### Ejercicio 9 — Construir el mejor y peor caso de BST

Dibuja el BST resultante de insertar estos elementos en dos órdenes distintos:
`{4, 2, 6, 1, 3, 5, 7}`

```
Orden A: 4, 2, 6, 1, 3, 5, 7
Orden B: 1, 2, 3, 4, 5, 6, 7
```

**Predicción**: ¿Cuál orden produce un árbol balanceado y cuál una lista?
¿Cuál es la altura de cada uno?

<details><summary>Respuesta</summary>

**Orden A: 4, 2, 6, 1, 3, 5, 7** → árbol balanceado

```
         4
       /   \
      2     6
     / \   / \
    1   3 5   7

Altura: 2 (log₂(7) ≈ 2.8)
Búsqueda de cualquier elemento: máximo 3 comparaciones
```

**Orden B: 1, 2, 3, 4, 5, 6, 7** → lista (degenerado)

```
    1
     \
      2
       \
        3
         \
          4
           \
            5
             \
              6
               \
                7

Altura: 6 (= n - 1)
Búsqueda de 7: 7 comparaciones (recorrer toda la "lista")
```

| | Orden A (balanceado) | Orden B (degenerado) |
|---|---------------------|---------------------|
| Altura | 2 | 6 |
| Búsqueda peor caso | O(log n) = 3 | O(n) = 7 |
| Espacio | O(n) | O(n) |

El mismo conjunto de datos, dependiendo del **orden de inserción**, produce
un árbol con rendimiento radicalmente distinto. Esto es exactamente la
diferencia entre caso promedio O(log n) (inserciones aleatorias) y peor caso
O(n) (inserciones ordenadas) del BST.

Solución: usar árboles auto-balanceados (AVL, Red-Black) que garantizan
altura O(log n) sin importar el orden de inserción.

</details>

---

### Ejercicio 10 — Elegir la estructura según el patrón de uso

Un programa necesita almacenar n = 10⁵ registros y realizar estas operaciones
con la frecuencia indicada:

```
Operación             Frecuencia
─────────────────────────────────
Insertar registro     100 veces/día
Buscar por clave      10,000 veces/segundo
Eliminar registro     10 veces/día
Iterar todos (orden)  1 vez/día
```

Opciones: array ordenado, hash table, AVL tree.

**Predicción**: ¿Cuál elegirías y por qué? Calcula el costo total
aproximado por segundo para cada opción.

<details><summary>Respuesta</summary>

Costo por operación (peor caso / promedio):

| Operación | Array ordenado | Hash table | AVL tree |
|-----------|---------------|-----------|----------|
| Insertar | O(n) = 10⁵ | O(1) | O(log n) = 17 |
| Buscar | O(log n) = 17 | **O(1)** = 1-3 | O(log n) = 17 |
| Eliminar | O(n) = 10⁵ | O(1) | O(log n) = 17 |
| Iterar orden | O(n) = 10⁵ | O(n log n)* | O(n) = 10⁵ |

\* Hash table necesita ordenar para iterar en orden.

**Costo dominante: búsqueda** (10,000/s vs 100 inserciones/día).

```
Array ordenado:  10,000 × 17 = 170,000 ops/s  (búsqueda)
                 100 × 10⁵ = 10⁷ ops/día       (inserciones, tolerable)

Hash table:      10,000 × 3 = 30,000 ops/s     (búsqueda, el más rápido)
                 1 × n·log(n) = 1.7×10⁶ ops/día (ordenar para iteración)

AVL tree:        10,000 × 17 = 170,000 ops/s   (búsqueda)
                 100 × 17 = 1,700 ops/día       (inserciones, trivial)
                 1 × 10⁵ ops/día                 (iteración in-order, trivial)
```

**Elección: Hash table**. La búsqueda (10,000/s) domina, y hash es 5-10x
más rápida que las otras. La iteración en orden (1/día) es costosa pero
infrecuente — ordenar 10⁵ elementos una vez al día tarda ~10ms.

**Segunda opción: AVL tree**. Si la iteración en orden fuera más frecuente
(ej: 1000/s) o si se necesitaran range queries ("todos entre 100 y 200"),
AVL ganaría porque soporta recorrido ordenado eficiente sin paso extra.

El array ordenado pierde por las inserciones O(n) — 100 inserciones de
10⁵ shifts cada una son 10⁷ operaciones diarias de puro movimiento de
datos.

</details>
