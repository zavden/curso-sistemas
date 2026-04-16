# qsort en C

## Objetivo

Dominar la función `qsort` de la biblioteca estándar de C (`<stdlib.h>`),
la interfaz genérica de ordenamiento disponible en cualquier compilador
conforme al estándar:

- Entender la **firma** completa y el rol de cada parámetro.
- Escribir **comparadores** correctos para distintos tipos.
- Dominar el **casting de `void *`** que hace posible la genericidad.
- Comprender que el estándar **no garantiza quicksort** — las
  implementaciones reales varían enormemente.

---

## La firma de qsort

```c
#include <stdlib.h>

void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
```

Cuatro parámetros, cada uno con un propósito preciso:

| Parámetro | Tipo | Significado |
|-----------|------|-------------|
| `base` | `void *` | Puntero al inicio del array |
| `nmemb` | `size_t` | Número de elementos |
| `size` | `size_t` | Tamaño en bytes de cada elemento |
| `compar` | función | Comparador que define el orden |

`void *` es el mecanismo de genericidad de C: un puntero que puede
apuntar a cualquier tipo. El programador es responsable de hacer el
cast correcto dentro del comparador. No hay comprobación en tiempo de
compilación — un cast incorrecto compila sin errores y produce
comportamiento indefinido en tiempo de ejecución.

El parámetro `size` le dice a `qsort` cuántos bytes ocupa cada
elemento. Internamente `qsort` hace aritmética de punteros con
`(char *)base + i * size` para acceder al i-ésimo elemento, porque
la aritmética con `void *` no está definida en el estándar (aunque
GCC la permite como extensión con semántica de `char *`).

---

## El comparador

El comparador es una función con firma fija:

```c
int compar(const void *a, const void *b);
```

Debe retornar:

| Retorno | Significado |
|---------|-------------|
| `< 0` | `*a` va antes que `*b` |
| `0` | `*a` y `*b` son equivalentes |
| `> 0` | `*a` va después de `*b` |

El contrato es idéntico al de `strcmp`. Los punteros `a` y `b` apuntan
a **elementos del array**, no a índices. El comparador debe:

1. Castear de `const void *` al tipo real con `const`.
2. Comparar los valores desreferenciados.
3. Retornar un entero con el signo correcto.

### Comparador para enteros

```c
int cmp_int(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;

    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}
```

La versión con `if` es la forma **segura**. Muchos programadores
escriben `return *(int *)a - *(int *)b;` como atajo, pero esto tiene
un bug grave que veremos a continuación.

### El bug de la resta

La expresión `a - b` desborda si `a` y `b` tienen signos opuestos y
la diferencia excede el rango de `int`. Para enteros de 32 bits:

```
a =  2000000000
b = -2000000000
a - b = 4000000000  →  desborda int  →  resultado negativo (UB)
```

El comparador retorna un valor negativo cuando debería retornar positivo,
invirtiendo el orden. Esto produce resultados silenciosamente incorrectos
— el programa no falla, simplemente ordena mal.

La resta es segura **solo** cuando los valores están acotados de forma
que `a - b` no puede desbordar. Por ejemplo, para valores en
$[0, 2^{30}]$ la diferencia cabe en un `int` de 32 bits. En la
práctica, la versión con `if` es preferible porque no requiere
razonar sobre rangos.

Para tipos donde la resta nunca desborda (como `unsigned char`), el
atajo es seguro:

```c
int cmp_uchar(const void *a, const void *b)
{
    return *(const unsigned char *)a - *(const unsigned char *)b;
}
```

### Comparador para doubles

```c
int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}
```

La resta `da - db` **no funciona** aquí por dos razones:

1. El resultado es `double`, no `int`. Truncar a `int` pierde
   diferencias menores que 1.0.
2. Si algún operando es `NaN`, todas las comparaciones retornan
   falso, y el comparador retorna 0 para cualquier par — lo que
   viola la transitividad y produce comportamiento indefinido en
   `qsort`.

### Comparador para strings

```c
int cmp_str(const void *a, const void *b)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;

    return strcmp(sa, sb);
}
```

Atención al **doble puntero**: el array es de tipo `char *[]`, así que
cada elemento es un `char *`. El parámetro `void *a` apunta a un
elemento del array, es decir, apunta a un `char *`. Por eso el cast
es a `const char **` y se desreferencia una vez para obtener el
`char *` que `strcmp` necesita.

Si el array fuera de tipo `char[][MAX_LEN]` (strings almacenados
directamente, no como punteros), el comparador sería diferente:

```c
int cmp_str_fixed(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}
```

Aquí `a` ya apunta al primer carácter del string — no hay indirección
adicional.

---

## Casting de void *

El patrón general para el cast dentro del comparador es:

```
void *a  →  (const T *)a  →  *(const T *)a
  ↑              ↑                  ↑
puntero     cast al tipo      desreferencia
genérico       real           para obtener
                              el valor
```

Reglas del cast:

1. **Siempre usar `const`**: los parámetros son `const void *`, y el
   comparador no debe modificar los elementos. Quitar `const` en el
   cast es técnicamente legal pero viola el contrato implícito.

2. **El tipo del cast debe coincidir con el tipo real del array**. Si
   el array es `int[]` y se castea a `double *`, el comportamiento es
   indefinido por violación de strict aliasing.

3. **Para arrays de punteros**, se necesita un nivel extra de
   indirección. El esquema es:
   - Array de `T`: cast a `const T *`, desreferenciar una vez.
   - Array de `T *`: cast a `const T **`, desreferenciar una vez para
     obtener el `T *`, luego acceder al objeto apuntado.

### sizeof y el parámetro size

El tercer parámetro de `qsort` debe ser `sizeof(elemento)`. Un error
frecuente es pasar el tamaño del puntero en lugar del tamaño del
elemento:

```c
char *names[] = {"Ana", "Zoe", "Max"};

/* CORRECTO: cada elemento es un char * */
qsort(names, 3, sizeof(char *), cmp_str);

/* INCORRECTO: sizeof(char) = 1, no es el tamaño del elemento */
qsort(names, 3, sizeof(char), cmp_str);  /* UB */
```

Para evitar este error, el idiom seguro es `sizeof(arr[0])`:

```c
qsort(arr, n, sizeof(arr[0]), comparador);
```

Esto funciona independientemente del tipo de `arr`.

---

## qsort no garantiza quicksort

A pesar del nombre, el estándar C (C99 §7.20.5.2, C11 §7.22.5.2) dice:

> "The `qsort` function sorts an array [...] The implementation shall
> define the algorithm used."

No hay ningún requisito sobre el algoritmo interno. Las implementaciones
reales varían considerablemente:

| Implementación | Algoritmo real |
|----------------|----------------|
| **glibc** (Linux) | Merge sort (con malloc) o quicksort (si malloc falla) |
| **musl** (Alpine) | Smoothsort (Dijkstra, in-place, heapsort variant) |
| **dietlibc** | Shellsort |
| **uClibc** | Shellsort |
| **BSD/macOS** | Introsort (quicksort + heapsort fallback) |
| **MSVC** (Windows) | Quicksort con median-of-three |
| **Newlib** (embedded) | Quicksort recursivo |

Consecuencias prácticas:

1. **No se puede asumir estabilidad**. El estándar no la requiere.
   glibc es estable (merge sort), pero musl y BSD no lo son.
   Código que depende de estabilidad es portablemente incorrecto.

2. **No se puede asumir complejidad en peor caso**. Si la implementación
   usa quicksort puro (MSVC, Newlib), el peor caso es $O(n^2)$. glibc
   es $O(n \log n)$ siempre (merge sort), BSD también (introsort).

3. **No se puede asumir espacio constante**. glibc asigna $O(n)$ con
   `malloc` para merge sort. Si necesitas in-place garantizado, `qsort`
   no es la herramienta adecuada en todas las plataformas.

### La indirección del comparador

El costo principal de `qsort` comparado con una implementación
directa es la **llamada indirecta** al comparador. Cada comparación
implica:

1. Llamada a través de un puntero a función (no puede ser inlined).
2. Dos casts de `void *` al tipo real.
3. Dos desreferencias.

En un quicksort directo para `int`, la comparación es simplemente
`if (arr[i] < pivot)` — una instrucción. Con `qsort`, es una
llamada a función con frame setup, argumentos en registros/stack,
return, etc.

Este overhead es medible: en benchmarks típicos, `qsort` es entre
2x y 5x más lento que un quicksort con tipo fijo para enteros. Para
$n = 10^6$ enteros, la diferencia puede ser de 50-100 ms vs 20-30 ms.

El compilador **no puede** optimizar esto porque:

- El puntero a función se resuelve en tiempo de ejecución.
- `qsort` está compilada por separado en la libc — no hay oportunidad
  de inlining cross-library.
- Incluso con LTO (Link-Time Optimization), la libc normalmente no
  participa en la optimización del programa del usuario.

En Rust, `.sort()` usa monomorphization: el compilador genera una
versión especializada para cada tipo, permitiendo inline del comparador.
Veremos esta diferencia en detalle en T05.

---

## Ordenar structs

El uso más interesante de `qsort` es ordenar arrays de structs por
uno o más campos. El patrón es siempre el mismo: castear, acceder
al campo, comparar.

### Por un campo

```c
typedef struct {
    char name[64];
    int  age;
    double gpa;
} Student;

int cmp_by_age(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    if (sa->age < sb->age) return -1;
    if (sa->age > sb->age) return  1;
    return 0;
}

int cmp_by_name(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    return strcmp(sa->name, sb->name);
}
```

### Por múltiples campos (orden lexicográfico)

Para ordenar primero por edad y, ante empate, por nombre:

```c
int cmp_by_age_name(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    if (sa->age != sb->age) {
        return (sa->age < sb->age) ? -1 : 1;
    }
    return strcmp(sa->name, sb->name);
}
```

El patrón es: comparar el campo primario; si son iguales, pasar al
secundario; si también son iguales, pasar al terciario, etc.

### Orden descendente

Para invertir el orden, hay dos opciones:

1. **Invertir la lógica**: cambiar `<` por `>` en el comparador.
2. **Invertir los argumentos**: `cmp(b, a)` en lugar de `cmp(a, b)`.

```c
int cmp_int_desc(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;

    if (va > vb) return -1;  /* invertido */
    if (va < vb) return  1;
    return 0;
}
```

---

## Errores frecuentes

### 1. Cast incorrecto del tipo

```c
double arr[] = {3.14, 1.41, 2.72};
qsort(arr, 3, sizeof(double), cmp_int);  /* BUG: interpreta bits de double como int */
```

Compila sin warnings. Produce resultados basura porque los bits de un
`double` IEEE 754 no representan el mismo valor cuando se interpretan
como `int`.

### 2. sizeof incorrecto

```c
int *arr = malloc(n * sizeof(int));
qsort(arr, n, sizeof(arr), cmp_int);  /* BUG: sizeof(arr) = sizeof(int *) = 8 */
```

`sizeof(arr)` cuando `arr` es puntero da el tamaño del puntero (4 u 8
bytes), no del elemento. Usar `sizeof(*arr)` o `sizeof(int)`.

### 3. Comparador no transitivo

El estándar exige que el comparador defina un **orden total estricto**:

- Si `cmp(a, b) < 0` y `cmp(b, c) < 0`, entonces `cmp(a, c) < 0`.
- Si `cmp(a, b) == 0` y `cmp(b, c) == 0`, entonces `cmp(a, c) == 0`.

Un comparador que viola la transitividad (por ejemplo, usando `NaN` en
doubles sin manejarlos) produce comportamiento indefinido — `qsort`
puede entrar en un bucle infinito o acceder fuera del array.

### 4. Olvidar const en el cast

```c
int cmp_bad(const void *a, const void *b)
{
    int *pa = (int *)a;  /* quita const — legal pero peligroso */
    *pa = 0;             /* modifica el array durante el sort — UB */
    ...
}
```

Aunque quitar `const` compila, modificar los elementos durante el
ordenamiento viola el contrato de `qsort` y produce comportamiento
indefinido.

### 5. Confundir array de strings vs strings empotrados

```c
/* Array de punteros: char *arr[] = {"b", "a", "c"} */
/* sizeof(arr[0]) = sizeof(char *) = 8 */
/* comparador: cast a (const char **), desreferenciar */

/* Array fijo: char arr[][10] = {"b", "a", "c"} */
/* sizeof(arr[0]) = 10 */
/* comparador: cast a (const char *), NO desreferenciar */
```

Mezclar estos dos patrones es un error frecuente que produce
segfaults o resultados incorrectos.

---

## Implementación interna: cómo funciona qsort por dentro

Para entender la indirección, veamos un esqueleto simplificado de cómo
`qsort` manipula los elementos internamente:

```c
/* Pseudocódigo de qsort internamente */
void qsort(void *base, size_t nmemb, size_t size,
            int (*cmp)(const void *, const void *))
{
    char *arr = (char *)base;    /* aritmética byte a byte */

    /* Para acceder al elemento i: */
    void *elem_i = arr + i * size;

    /* Para comparar elementos i y j: */
    int result = cmp(arr + i * size, arr + j * size);

    /* Para intercambiar elementos i y j: */
    /* copiar size bytes con memcpy o byte-swap manual */
    char tmp[size];  /* o malloc si size es grande */
    memcpy(tmp,             arr + i * size, size);
    memcpy(arr + i * size,  arr + j * size, size);
    memcpy(arr + j * size,  tmp,            size);
}
```

Cada swap copia `size` bytes — para structs grandes, esto es costoso.
Una alternativa cuando los elementos son grandes es ordenar un array
de punteros y luego reordenar los datos, reduciendo el costo de swap
a un intercambio de punteros (8 bytes en 64-bit).

---

## Programa completo en C

```c
/* qsort_demo.c — Demostración completa de qsort */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Comparadores ---------- */

/* Enteros ascendente (seguro, sin overflow) */
int cmp_int_asc(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

/* Enteros descendente */
int cmp_int_desc(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va > vb) return -1;
    if (va < vb) return  1;
    return 0;
}

/* Doubles ascendente */
int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return  1;
    return 0;
}

/* Strings (array de char *) */
int cmp_str(const void *a, const void *b)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);
}

/* ---------- Struct y comparadores multi-campo ---------- */

typedef struct {
    char name[32];
    int  age;
    double gpa;
} Student;

/* Por GPA descendente, luego por nombre ascendente */
int cmp_student(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    /* GPA descendente: mayor primero */
    if (sa->gpa > sb->gpa) return -1;
    if (sa->gpa < sb->gpa) return  1;

    /* Empate en GPA: ordenar por nombre ascendente */
    return strcmp(sa->name, sb->name);
}

/* ---------- Helpers de impresión ---------- */

void print_ints(const char *label, const int *arr, size_t n)
{
    printf("%s:", label);
    for (size_t i = 0; i < n; i++)
        printf(" %d", arr[i]);
    printf("\n");
}

void print_doubles(const char *label, const double *arr, size_t n)
{
    printf("%s:", label);
    for (size_t i = 0; i < n; i++)
        printf(" %.2f", arr[i]);
    printf("\n");
}

void print_students(const char *label, const Student *arr, size_t n)
{
    printf("%s:\n", label);
    for (size_t i = 0; i < n; i++)
        printf("  %-10s  age=%d  gpa=%.2f\n",
               arr[i].name, arr[i].age, arr[i].gpa);
}

/* ---------- Demo del bug de overflow ---------- */

int cmp_int_bad(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;  /* BUG: overflow */
}

/* ---------- main ---------- */

int main(void)
{
    /* Demo 1: Enteros ascendente */
    printf("=== Demo 1: Enteros ascendente ===\n");
    int nums[] = {42, 7, -3, 99, 0, 15, -8, 23};
    size_t n = sizeof(nums) / sizeof(nums[0]);

    print_ints("Antes", nums, n);
    qsort(nums, n, sizeof(nums[0]), cmp_int_asc);
    print_ints("Después", nums, n);

    /* Demo 2: Enteros descendente */
    printf("\n=== Demo 2: Enteros descendente ===\n");
    int nums2[] = {42, 7, -3, 99, 0, 15, -8, 23};
    n = sizeof(nums2) / sizeof(nums2[0]);

    print_ints("Antes", nums2, n);
    qsort(nums2, n, sizeof(nums2[0]), cmp_int_desc);
    print_ints("Después", nums2, n);

    /* Demo 3: Doubles */
    printf("\n=== Demo 3: Doubles ===\n");
    double vals[] = {3.14, 1.41, 2.72, 0.58, 1.73};
    n = sizeof(vals) / sizeof(vals[0]);

    print_doubles("Antes", vals, n);
    qsort(vals, n, sizeof(vals[0]), cmp_double);
    print_doubles("Después", vals, n);

    /* Demo 4: Strings */
    printf("\n=== Demo 4: Strings ===\n");
    const char *words[] = {"mango", "apple", "cherry", "banana", "date"};
    n = sizeof(words) / sizeof(words[0]);

    printf("Antes:");
    for (size_t i = 0; i < n; i++) printf(" %s", words[i]);
    printf("\n");

    qsort(words, n, sizeof(words[0]), cmp_str);

    printf("Después:");
    for (size_t i = 0; i < n; i++) printf(" %s", words[i]);
    printf("\n");

    /* Demo 5: Structs multi-campo */
    printf("\n=== Demo 5: Structs (GPA desc, nombre asc) ===\n");
    Student students[] = {
        {"Carlos",  20, 3.50},
        {"Ana",     22, 3.90},
        {"Beatriz", 21, 3.50},
        {"David",   23, 3.90},
        {"Elena",   20, 3.20},
    };
    n = sizeof(students) / sizeof(students[0]);

    print_students("Antes", students, n);
    qsort(students, n, sizeof(students[0]), cmp_student);
    print_students("Después", students, n);

    /* Demo 6: Bug de overflow */
    printf("\n=== Demo 6: Bug de overflow en comparador ===\n");
    int extreme[] = {2000000000, -2000000000, 0, 1, -1};
    n = sizeof(extreme) / sizeof(extreme[0]);

    int safe_copy[5];
    memcpy(safe_copy, extreme, sizeof(extreme));

    print_ints("Input", extreme, n);

    qsort(extreme, n, sizeof(extreme[0]), cmp_int_bad);
    print_ints("Con resta (buggy)", extreme, n);

    qsort(safe_copy, n, sizeof(safe_copy[0]), cmp_int_asc);
    print_ints("Con if (correcto)", safe_copy, n);

    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -o qsort_demo qsort_demo.c
./qsort_demo
```

### Salida esperada

```
=== Demo 1: Enteros ascendente ===
Antes: 42 7 -3 99 0 15 -8 23
Después: -8 -3 0 7 15 23 42 99

=== Demo 2: Enteros descendente ===
Antes: 42 7 -3 99 0 15 -8 23
Después: 99 42 23 15 7 0 -3 -8

=== Demo 3: Doubles ===
Antes: 3.14 1.41 2.72 0.58 1.73
Después: 0.58 1.41 1.73 2.72 3.14

=== Demo 4: Strings ===
Antes: mango apple cherry banana date
Después: apple banana cherry date mango

=== Demo 5: Structs (GPA desc, nombre asc) ===
Antes:
  Carlos      age=20  gpa=3.50
  Ana         age=22  gpa=3.90
  Beatriz     age=21  gpa=3.50
  David       age=23  gpa=3.90
  Elena       age=20  gpa=3.20
Después:
  Ana         age=22  gpa=3.90
  David       age=23  gpa=3.90
  Beatriz     age=21  gpa=3.50
  Carlos      age=20  gpa=3.50
  Elena       age=20  gpa=3.20

=== Demo 6: Bug de overflow en comparador ===
Input: 2000000000 -2000000000 0 1 -1
Con resta (buggy): -2000000000 -1 0 1 2000000000
Con if (correcto): -2000000000 -1 0 1 2000000000
```

**Nota sobre Demo 6**: El resultado del comparador con resta puede
variar entre ejecuciones y plataformas. En algunos casos, la
implementación de `qsort` puede producir un resultado que coincide con
el correcto por casualidad (los valores extremos pueden no compararse
directamente entre sí en todas las particiones). El bug se manifiesta
de forma intermitente, lo que lo hace especialmente peligroso.

Para provocar el bug de forma confiable, se necesitan inputs donde la
comparación defectuosa altere una decisión de partición. El punto
clave es: **la resta puede desbordar, y código que funciona "por
casualidad" sigue siendo incorrecto**.

---

## bsearch: búsqueda binaria genérica

La biblioteca estándar también ofrece `bsearch`, que usa el mismo
patrón de comparador:

```c
void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
```

`bsearch` requiere que el array esté **previamente ordenado** con el
mismo comparador. Retorna un puntero al elemento encontrado, o `NULL`
si no existe. El primer parámetro `key` apunta a un valor del mismo
tipo que los elementos del array.

```c
int target = 23;
int *found = bsearch(&target, nums, n, sizeof(nums[0]), cmp_int_asc);

if (found)
    printf("Encontrado: %d (posición %td)\n", *found, found - nums);
else
    printf("No encontrado\n");
```

`qsort` + `bsearch` es el par estándar de C para "ordenar y buscar" —
equivalente a la combinación `.sort()` + `.binary_search()` en Rust.

---

## Ejercicios

### Ejercicio 1 — Traza del comparador

Dado el siguiente array y comparador:

```c
int arr[] = {30, 10, 20};
qsort(arr, 3, sizeof(int), cmp_int_asc);
```

Suponiendo que `qsort` usa insertion sort para arrays pequeños,
lista las llamadas al comparador y el estado del array después de
cada swap.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

Paso a paso con insertion sort:

```
Estado inicial: [30, 10, 20]

i=1: comparar arr[1]=10 con arr[0]=30
     cmp_int_asc(&10, &30) → -1  (10 < 30)
     swap: [10, 30, 20]

i=2: comparar arr[2]=20 con arr[1]=30
     cmp_int_asc(&20, &30) → -1  (20 < 30)
     swap: [10, 20, 30]
     comparar arr[1]=20 con arr[0]=10
     cmp_int_asc(&20, &10) → 1  (20 > 10)
     parar

Resultado: [10, 20, 30]
Total: 3 llamadas al comparador
```

Nota: el número exacto de llamadas depende del algoritmo interno de
`qsort`, que varía entre implementaciones.

</details>

### Ejercicio 2 — Comparador para orden descendente de strings

Escribe un comparador para ordenar un array `const char *words[]`
en orden alfabético **descendente** (Z antes que A).

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int cmp_str_desc(const void *a, const void *b)
{
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sb, sa);  /* argumentos invertidos */
}
```

Se invierte el orden de los argumentos de `strcmp`: `strcmp(sb, sa)`
en lugar de `strcmp(sa, sb)`. Esto hace que strings "mayores"
lexicográficamente queden antes en el resultado.

Para `{"apple", "cherry", "banana"}`:

```
Resultado: cherry banana apple
```

</details>

### Ejercicio 3 — El bug de la resta

Dado este comparador y array:

```c
int cmp_bad(const void *a, const void *b) {
    return *(int *)a - *(int *)b;
}
int arr[] = {INT_MAX, INT_MIN, 0};
```

Explica qué valor retorna `cmp_bad` cuando compara `INT_MAX` con
`INT_MIN`, y por qué el resultado del sort puede ser incorrecto.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```
INT_MAX - INT_MIN = 2147483647 - (-2147483648)
                  = 2147483647 + 2147483648
                  = 4294967295
```

Este valor excede `INT_MAX` (2147483647). En aritmética de complemento
a dos, el resultado se envuelve (wrap around) a un valor negativo
(exactamente $-1$ en la mayoría de plataformas).

El comparador retorna **negativo** cuando debería retornar **positivo**
(ya que `INT_MAX > INT_MIN`). Esto le dice a `qsort` que `INT_MAX`
va antes que `INT_MIN` — el orden inverso al correcto.

Técnicamente es comportamiento indefinido (signed integer overflow),
así que el compilador podría generar cualquier resultado.

</details>

### Ejercicio 4 — sizeof incorrecto

¿Qué sucede con este código? ¿Compila? ¿Da resultado correcto?

```c
int *arr = malloc(5 * sizeof(int));
arr[0]=50; arr[1]=10; arr[2]=40; arr[3]=20; arr[4]=30;
qsort(arr, 5, sizeof(arr), cmp_int_asc);
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

**Compila sin warnings.** Pero el resultado es incorrecto.

`sizeof(arr)` cuando `arr` es un `int *` da `sizeof(int *)`, que es
8 en plataformas de 64 bits. Pero cada elemento es un `int` de 4
bytes.

`qsort` cree que cada elemento ocupa 8 bytes y hace aritmética de
punteros con pasos de 8 bytes. Lee y escribe pares de enteros como si
fueran un solo elemento, produciendo resultados basura.

Corrección: usar `sizeof(*arr)` o `sizeof(int)`:

```c
qsort(arr, 5, sizeof(*arr), cmp_int_asc);
```

En 32 bits (`sizeof(int) == sizeof(int *) == 4`) el bug no se
manifiesta — otro motivo por el que este tipo de errores son
peligrosos: pueden pasar tests en una plataforma y fallar en otra.

</details>

### Ejercicio 5 — Ordenar structs por dos campos

Dado este array de structs:

```c
typedef struct { int priority; char task[32]; } Todo;
Todo items[] = {
    {2, "Review PR"},
    {1, "Fix bug"},
    {2, "Write docs"},
    {1, "Deploy"},
    {3, "Refactor"},
};
```

Escribe un comparador que ordene por `priority` ascendente y, ante
empate, por `task` ascendente. ¿Cuál es el orden resultante?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int cmp_todo(const void *a, const void *b)
{
    const Todo *ta = (const Todo *)a;
    const Todo *tb = (const Todo *)b;

    if (ta->priority != tb->priority)
        return (ta->priority < tb->priority) ? -1 : 1;

    return strcmp(ta->task, tb->task);
}
```

Resultado:

```
priority=1  task=Deploy
priority=1  task=Fix bug
priority=2  task=Review PR
priority=2  task=Write docs
priority=3  task=Refactor
```

Primero se agrupa por prioridad (1, 2, 3). Dentro de cada grupo,
se ordena alfabéticamente: "Deploy" < "Fix bug" (D < F) y
"Review PR" < "Write docs" (R < W).

</details>

### Ejercicio 6 — Comparador para array de char fijo

Dado un array de strings almacenados como `char[][16]` (no como
punteros):

```c
char names[][16] = {"Zelda", "Alice", "Marge", "Bob"};
```

Escribe la llamada a `qsort` correcta con el comparador apropiado.
Explica por qué el cast es diferente al caso de `char *[]`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int cmp_fixed_str(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

qsort(names, 4, sizeof(names[0]), cmp_fixed_str);
/* sizeof(names[0]) = 16 */
```

La diferencia clave: en `char *[]`, cada elemento es un **puntero**
(8 bytes) que apunta al string. El comparador recibe un puntero al
puntero → cast a `char **`, desreferenciar.

En `char[][16]`, cada elemento es un **bloque de 16 bytes** que
contiene el string directamente. El comparador recibe un puntero al
bloque → cast a `char *`, sin desreferencia extra. El puntero ya
apunta al primer carácter del string.

Resultado: `Alice Bob Marge Zelda`

Si usaras el comparador de `char *[]` (con `char **` cast), leería
los primeros 8 bytes del string como si fueran una dirección de
memoria y luego intentaría acceder a esa dirección → **segfault**.

</details>

### Ejercicio 7 — bsearch después de qsort

Después de ordenar un array de enteros con `qsort`, usa `bsearch`
para buscar un valor. ¿Qué pasa si el array no está ordenado?

```c
int arr[] = {42, 7, 15, 99, 3, 28};
size_t n = 6;
int target = 15;
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
/* Paso 1: Ordenar */
qsort(arr, n, sizeof(arr[0]), cmp_int_asc);
/* arr ahora es: {3, 7, 15, 28, 42, 99} */

/* Paso 2: Buscar */
int *found = bsearch(&target, arr, n, sizeof(arr[0]), cmp_int_asc);

if (found)
    printf("Encontrado: %d en posición %td\n", *found, found - arr);
else
    printf("No encontrado\n");
```

Salida: `Encontrado: 15 en posición 2`

Si el array **no** está ordenado, `bsearch` hace búsqueda binaria
sobre datos desordenados — puede retornar `NULL` para un valor que
sí existe, o encontrar un valor por casualidad. El estándar dice que
el comportamiento no está definido si el array no está ordenado con
el mismo comparador.

</details>

### Ejercicio 8 — Ordenar punteros a struct (indirección doble)

Dado un array de **punteros** a structs (no un array de structs):

```c
Student *ptrs[3];
ptrs[0] = &(Student){"Carlos", 20, 3.5};
ptrs[1] = &(Student){"Ana",    22, 3.9};
ptrs[2] = &(Student){"Bob",    21, 3.7};
```

Escribe el comparador para ordenar por nombre. ¿Cuántos niveles
de desreferencia necesitas?

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
int cmp_student_ptr_by_name(const void *a, const void *b)
{
    /* a apunta a un Student * → cast a (Student **), desreferenciar */
    const Student *sa = *(const Student **)a;
    const Student *sb = *(const Student **)b;

    return strcmp(sa->name, sb->name);
}

qsort(ptrs, 3, sizeof(ptrs[0]), cmp_student_ptr_by_name);
```

Niveles de desreferencia:

```
void *a  →  (const Student **)a  →  *(const Student **)a  →  sa->name
  ↑               ↑                        ↑                    ↑
puntero      cast a puntero         desref: obtener      acceder campo
genérico     a puntero a            el Student *
             Student
```

Son dos niveles: uno para obtener el `Student *` del array, y la
flecha `->` para acceder al campo `name`. El mismo patrón que con
`char *[]` (array de punteros a char).

Resultado: `Ana Bob Carlos`

Ventaja de ordenar punteros: el swap mueve solo 8 bytes (un puntero)
en lugar de `sizeof(Student)` bytes por cada intercambio.

</details>

### Ejercicio 9 — Comparador genérico con macro

Escribe una macro que genere comparadores para tipos numéricos,
evitando el bug de overflow:

```c
#define MAKE_CMP(name, type)  /* tu código aquí */

MAKE_CMP(cmp_int,    int)
MAKE_CMP(cmp_long,   long)
MAKE_CMP(cmp_float,  float)
MAKE_CMP(cmp_double, double)
```

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
#define MAKE_CMP(name, type)                          \
    int name(const void *a, const void *b)            \
    {                                                 \
        type va = *(const type *)a;                   \
        type vb = *(const type *)b;                   \
        if (va < vb) return -1;                       \
        if (va > vb) return  1;                       \
        return 0;                                     \
    }
```

Cada invocación de `MAKE_CMP` genera una función completa con el
cast correcto para el tipo especificado. La versión con `if` es
segura para todos los tipos numéricos: enteros (sin overflow),
floating-point (sin truncamiento), signed y unsigned.

Uso:

```c
MAKE_CMP(cmp_int, int)
MAKE_CMP(cmp_uint, unsigned int)

int arr[] = {5, 2, 8};
qsort(arr, 3, sizeof(int), cmp_int);
```

Esta técnica es la aproximación más cercana a la genericidad real en
C. En Rust, el compilador genera código equivalente automáticamente
con monomorphization — sin macros, sin posibilidad de error de tipo.

</details>

### Ejercicio 10 — qsort vs sort manual: rendimiento

Implementa un quicksort manual para `int` (sin `void *`, sin
comparador) y compara su rendimiento con `qsort` para $10^6$ enteros
aleatorios. Mide el tiempo con `clock()`.

<details><summary>Predice el resultado antes de ver la respuesta</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int cmp_int(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

void swap_int(int *a, int *b)
{
    int t = *a; *a = *b; *b = t;
}

int partition(int *arr, int lo, int hi)
{
    int pivot = arr[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (arr[j] <= pivot) {
            swap_int(&arr[i], &arr[j]);
            i++;
        }
    }
    swap_int(&arr[i], &arr[hi]);
    return i;
}

void quicksort(int *arr, int lo, int hi)
{
    if (lo >= hi) return;
    int p = partition(arr, lo, hi);
    quicksort(arr, lo, p - 1);
    quicksort(arr, p + 1, hi);
}

int main(void)
{
    const int N = 1000000;
    int *a = malloc(N * sizeof(int));
    int *b = malloc(N * sizeof(int));

    srand(42);
    for (int i = 0; i < N; i++)
        a[i] = rand();
    memcpy(b, a, N * sizeof(int));

    clock_t t0 = clock();
    qsort(a, N, sizeof(int), cmp_int);
    clock_t t1 = clock();

    clock_t t2 = clock();
    quicksort(b, 0, N - 1);
    clock_t t3 = clock();

    printf("qsort:     %.3f ms\n",
           (double)(t1 - t0) / CLOCKS_PER_SEC * 1000);
    printf("manual QS: %.3f ms\n",
           (double)(t3 - t2) / CLOCKS_PER_SEC * 1000);

    free(a);
    free(b);
    return 0;
}
```

Resultado típico (x86-64, gcc -O2):

```
qsort:     ~70-90 ms
manual QS: ~25-35 ms
```

El quicksort manual es **2-3x más rápido** que `qsort` porque:

1. **Sin llamada indirecta**: la comparación `arr[j] <= pivot` es una
   instrucción, no una llamada a función.
2. **Sin cast de void ***: se trabaja directamente con `int *`.
3. **Inline de swap**: `swap_int` se inlinea completamente con `-O2`.
4. **Mejor optimización del compilador**: el compilador puede
   vectorizar, reordenar instrucciones y usar registros óptimamente
   cuando ve el tipo concreto.

Este es el costo de la genericidad en C. En Rust, `.sort_unstable()`
obtiene la velocidad del sort manual con la ergonomía de `qsort` —
tema de T05.

</details>

---

## Resumen

| Aspecto | Detalle |
|---------|---------|
| Firma | `void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *))` |
| Comparador | Retorna `<0`, `0`, `>0` — como `strcmp` |
| Genericidad | Via `void *` casting — responsabilidad del programador |
| Algoritmo | **No especificado** por el estándar — varía entre implementaciones |
| Estabilidad | **No garantizada** — depende de la implementación |
| Overhead | 2-5x más lento que sort con tipo fijo por indirección del comparador |
| Bug clásico | Resta en comparador: desborda para valores extremos |
| Complemento | `bsearch` — búsqueda binaria con el mismo patrón de comparador |
