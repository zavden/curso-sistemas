# T02 — Arrays y punteros

> Sin erratas detectadas en el material base.

---

## 1. Decay: de array a puntero

En la mayoría de contextos, el nombre de un array **decae** (decay) a un puntero al primer elemento:

```c
int arr[5] = {10, 20, 30, 40, 50};

int *p = arr;         // equivale a: int *p = &arr[0];
int *q = &arr[0];     // forma explícita

// p == q → true
// arr, &arr[0], p y q contienen la misma dirección
```

El decay es automático e implícito. Ocurre cada vez que el nombre del array se usa en una expresión donde se espera un puntero (asignación, paso a función, aritmética).

Pero el array **no es** un puntero. Son cosas distintas que se comportan igual en ciertos contextos.

---

## 2. Array ≠ Puntero: las tres diferencias

### Diferencia 1: `sizeof`

```c
int arr[5] = {1, 2, 3, 4, 5};
int *ptr = arr;

sizeof(arr);   // 20 — tamaño total del array (5 × 4)
sizeof(ptr);   // 8  — tamaño del puntero (en x86-64)
```

`sizeof` es el único operador que distingue un array de un puntero en una expresión.

### Diferencia 2: `&` (dirección de)

```c
int arr[5] = {1, 2, 3, 4, 5};

// arr (decayed) tiene tipo int*
// &arr tiene tipo int (*)[5] — puntero a array de 5 ints

// Mismo valor numérico, pero tipos diferentes:
arr + 1;     // avanza 4 bytes  (sizeof(int))
&arr + 1;    // avanza 20 bytes (sizeof(int[5]))
```

El tipo del puntero determina el tamaño del paso en la aritmética.

### Diferencia 3: asignación

```c
int arr[5], other[5];
int *ptr;

arr = other;   // ERROR: array no es un lvalue modificable
ptr = arr;     // OK: puntero se puede reasignar
```

Un array no se puede reasignar — su dirección es fija durante toda su vida.

### Resumen

| Operación | Array (`int arr[5]`) | Puntero (`int *p`) |
|-----------|----------------------|--------------------|
| `sizeof(x)` | 20 (tamaño total) | 8 (tamaño puntero) |
| `&x` | `int (*)[5]` | `int **` |
| `&x + 1` | avanza `sizeof(arr)` bytes | avanza `sizeof(int*)` bytes |
| `x = algo` | ERROR | OK |
| Declaración | reserva memoria para elementos | almacena solo una dirección |

---

## 3. Cuándo NO hay decay

El array NO decae a puntero en exactamente tres contextos:

```c
int arr[5] = {1, 2, 3, 4, 5};

// 1. sizeof(arr) — retorna tamaño del array completo:
size_t total = sizeof(arr);       // 20, no 8

// 2. &arr — retorna puntero al array completo:
int (*p)[5] = &arr;               // tipo: int (*)[5]

// 3. Inicialización de string literal en char[]:
char str[] = "hello";             // copia el string, no decay
// str es un array de 6 chars {'h','e','l','l','o','\0'}, no un puntero
```

En cualquier otro contexto, `arr` se convierte implícitamente en `&arr[0]`.

---

## 4. `arr[i]` es `*(arr + i)`

El compilador transforma la notación de corchetes en aritmética de punteros:

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;

arr[3];          // *(arr + 3) → 40
*(arr + 3);      // lo mismo
*(p + 3);        // lo mismo
p[3];            // lo mismo — [] funciona con punteros
```

Como la suma es conmutativa (`arr + 3 == 3 + arr`), esto también compila:

```c
3[arr];    // *(3 + arr) → 40
```

Es un accidente de la definición de C. **Nunca** escribir código así.

---

## 5. Aritmética de punteros

### Avance por elementos, no bytes

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;

// p + n avanza n ELEMENTOS (no bytes):
*(p + 0);   // 10 — en dirección p + 0*4
*(p + 1);   // 20 — en dirección p + 1*4
*(p + 4);   // 50 — en dirección p + 4*4

// Internamente: p + n → dirección_p + n * sizeof(int)
// Si p = 0x1000:
//   p + 0 = 0x1000
//   p + 1 = 0x1004 (avanzó 4 bytes)
//   p + 2 = 0x1008
```

### Diferencia de punteros

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = &arr[1];   // apunta a 20
int *q = &arr[4];   // apunta a 50

ptrdiff_t diff = q - p;   // 3 (elementos, no bytes)
// ptrdiff_t está en <stddef.h>, formato %td
```

La diferencia de punteros solo es válida si ambos apuntan al mismo array (o a one-past-the-end). Si apuntan a arrays distintos: UB.

### Puntero one-past-the-end

```c
int arr[5] = {10, 20, 30, 40, 50};
int *end = arr + 5;   // apunta DESPUÉS del último elemento

// end es válido como dirección para comparaciones:
for (int *p = arr; p != end; p++) {
    printf("%d ", *p);
}

// Pero desreferenciar end (*end) es UB.
// Es el mismo patrón que los iteradores end() en C++.
```

---

## 6. Cuatro formas de recorrer un array

```c
int arr[] = {5, 10, 15, 20, 25, 30};
int n = sizeof(arr) / sizeof(arr[0]);
```

### 1. Índice clásico

```c
for (int i = 0; i < n; i++) {
    printf("%d ", arr[i]);
}
```

### 2. Puntero que se incrementa

```c
for (int *p = arr; p < arr + n; p++) {
    printf("%d ", *p);
}
```

### 3. Aritmética de punteros

```c
for (int i = 0; i < n; i++) {
    printf("%d ", *(arr + i));
}
```

### 4. Puntero con sentinel (end)

```c
int *end = arr + n;
for (int *p = arr; p != end; p++) {
    printf("%d ", *p);
}
```

Las cuatro producen el mismo resultado. El índice clásico es el más legible. La forma con sentinel es el patrón de C++ y se usa en código de bajo nivel.

---

## 7. String literals y arrays

```c
// Caso 1: puntero a string literal (read-only)
const char *s = "hello";
// s apunta a un string literal en memoria de solo lectura
// s[0] = 'H';    // UB — modificar string literal

// Caso 2: array inicializado con string literal (copia modificable)
char arr[] = "hello";
// arr es un array de 6 chars: {'h','e','l','l','o','\0'}
// arr se puede modificar — es una copia
arr[0] = 'H';    // OK — arr = "Hello"

// sizeof:
sizeof(s);     // 8 — tamaño del puntero
sizeof(arr);   // 6 — tamaño del array (5 chars + '\0')
```

Esto es la excepción 3 del decay: cuando un string literal inicializa un `char[]`, se **copia** en el array en vez de decaer a puntero.

---

## 8. Puntero a array completo: `int (*p)[N]`

```c
int arr[5] = {1, 2, 3, 4, 5};
int (*p)[5] = &arr;    // puntero a array de 5 ints

// Acceso:
(*p)[0];   // 1 — desreferenciar p da el array, luego indexar
(*p)[4];   // 5

// Desreferencia:
*p;        // el array mismo (decae a int*) — tipo: int[5]
**p;       // primer elemento — valor: 1

// Aritmética:
p + 1;     // avanza 20 bytes (sizeof(int[5]))
```

No confundir:

```c
int (*p)[5];   // puntero a array de 5 ints
int *p[5];     // array de 5 punteros a int
```

Los paréntesis cambian completamente el significado. La regla: `*` se asocia al nombre antes que `[]`, así que sin paréntesis `p` es un array.

`int (*p)[N]` es útil para recorrer matrices (arrays de arrays) — cada `p + 1` avanza una fila completa.

---

## 9. Comparación con Rust

### No hay decay en Rust

```rust
let arr = [10, 20, 30, 40, 50];

// No hay decay implícito — debes crear un slice explícitamente:
let slice: &[i32] = &arr;     // slice = puntero + longitud

// El slice conserva la longitud:
println!("{}", slice.len());  // 5 — siempre correcto
```

En C, la información de tamaño se pierde al pasar a una función. En Rust, un slice (`&[T]`) es un "fat pointer" que contiene puntero **y** longitud — nunca se pierde.

### Aritmética de punteros: solo en `unsafe`

```rust
let arr = [10, 20, 30, 40, 50];

// Acceso por índice con bounds checking:
arr[2];    // OK: 30
arr[5];    // panic! — Rust verifica en runtime

// Aritmética de punteros solo en unsafe:
unsafe {
    let p = arr.as_ptr();
    let val = *p.add(2);   // 30
}
```

Rust elimina las clases de bugs más comunes de C (OOB, dangling pointers) moviendo la aritmética de punteros a bloques `unsafe`.

### No hay distinción array/puntero

En Rust no existe el problema array-vs-puntero. Los arrays (`[T; N]`) tienen tamaño conocido en compilación y siempre preservan su longitud. Los slices (`&[T]`) son la forma segura de pasar "vistas" a arrays.

---

## Ejercicios

### Ejercicio 1 — Decay en acción

Verifica que `arr`, `&arr[0]` y un puntero asignado con `arr` contienen la misma dirección:

```c
#include <stdio.h>

int main(void) {
    int arr[4] = {100, 200, 300, 400};
    int *p = arr;

    printf("arr      = %p\n", (void *)arr);
    printf("&arr[0]  = %p\n", (void *)&arr[0]);
    printf("p        = %p\n", (void *)p);
    printf("Equal: %s\n", (arr == &arr[0] && p == arr) ? "yes" : "no");

    printf("\narr[2] = %d\n", arr[2]);
    printf("*(arr+2) = %d\n", *(arr + 2));
    printf("*(p+2)   = %d\n", *(p + 2));
    printf("p[2]     = %d\n", p[2]);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
./ex1
```

<details><summary>Predicción</summary>

```
arr      = 0x7fff...
&arr[0]  = 0x7fff...
p        = 0x7fff...
Equal: yes

arr[2] = 300
*(arr+2) = 300
*(p+2)   = 300
p[2]     = 300
```

Las tres direcciones son idénticas: `arr` decae a `&arr[0]`, y `p` se inicializó con el valor decayed. Las cuatro formas de acceder al elemento 2 son equivalentes — el compilador transforma `arr[2]` en `*(arr + 2)` internamente.

</details>

---

### Ejercicio 2 — `sizeof(arr)` vs `sizeof(ptr)`

Demuestra las tres diferencias entre un array y un puntero:

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {1, 2, 3, 4, 5};
    int *ptr = arr;

    printf("=== sizeof ===\n");
    printf("sizeof(arr) = %zu\n", sizeof(arr));
    printf("sizeof(ptr) = %zu\n", sizeof(ptr));

    printf("\n=== & type (step size) ===\n");
    printf("arr          = %p\n", (void *)arr);
    printf("arr + 1      = %p  (+%td bytes)\n",
           (void *)(arr + 1), (char *)(arr + 1) - (char *)arr);
    printf("&arr + 1     = %p  (+%td bytes)\n",
           (void *)(&arr + 1), (char *)(&arr + 1) - (char *)&arr);

    printf("\n=== assignment ===\n");
    printf("ptr = arr OK\n");
    // arr = ptr;  // ERROR: array is not assignable
    printf("arr = ptr would be ERROR\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
./ex2
```

<details><summary>Predicción</summary>

```
=== sizeof ===
sizeof(arr) = 20
sizeof(ptr) = 8

=== & type (step size) ===
arr          = 0x7fff...
arr + 1      = 0x7fff...  (+4 bytes)
&arr + 1     = 0x7fff...  (+20 bytes)

=== assignment ===
ptr = arr OK
arr = ptr would be ERROR
```

`sizeof(arr)` = 20 (5 × 4 bytes). `sizeof(ptr)` = 8 (tamaño de un puntero en x86-64). `arr + 1` avanza 4 bytes (un `int`) porque `arr` decae a `int*`. `&arr + 1` avanza 20 bytes (un array entero) porque `&arr` tiene tipo `int (*)[5]`. El puntero se puede reasignar; el array no.

</details>

---

### Ejercicio 3 — Cuatro formas de recorrer

Recorre el mismo array de cuatro formas y verifica que todas producen el mismo resultado:

```c
#include <stdio.h>

int main(void) {
    int arr[] = {2, 4, 6, 8, 10};
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("1. Index:     ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    printf("2. Pointer++: ");
    for (int *p = arr; p < arr + n; p++) printf("%d ", *p);
    printf("\n");

    printf("3. *(arr+i):  ");
    for (int i = 0; i < n; i++) printf("%d ", *(arr + i));
    printf("\n");

    printf("4. Sentinel:  ");
    int *end = arr + n;
    for (int *p = arr; p != end; p++) printf("%d ", *p);
    printf("\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
./ex3
```

<details><summary>Predicción</summary>

```
1. Index:     2 4 6 8 10
2. Pointer++: 2 4 6 8 10
3. *(arr+i):  2 4 6 8 10
4. Sentinel:  2 4 6 8 10
```

Las cuatro formas producen exactamente la misma salida. Internamente, el compilador las traduce a la misma operación: calcular `dirección_base + i * sizeof(int)` y leer el valor. La forma 1 (índice) es la más legible. La forma 4 (sentinel) usa `arr + n` como puntero one-past-the-end — válido para comparar, pero no para desreferenciar.

</details>

---

### Ejercicio 4 — Aritmética de punteros: direcciones

Observa las direcciones de cada elemento para confirmar que avanzan en `sizeof(int)`:

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    for (int i = 0; i < 5; i++) {
        printf("arr + %d = %p  value = %d  (offset: %td bytes)\n",
               i, (void *)(arr + i), arr[i],
               (char *)(arr + i) - (char *)arr);
    }

    printf("\nStep size = %td bytes = sizeof(int) = %zu\n",
           (char *)(arr + 1) - (char *)arr, sizeof(int));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
./ex4
```

<details><summary>Predicción</summary>

```
arr + 0 = 0x7fff...  value = 10  (offset: 0 bytes)
arr + 1 = 0x7fff...  value = 20  (offset: 4 bytes)
arr + 2 = 0x7fff...  value = 30  (offset: 8 bytes)
arr + 3 = 0x7fff...  value = 40  (offset: 12 bytes)
arr + 4 = 0x7fff...  value = 50  (offset: 16 bytes)

Step size = 4 bytes = sizeof(int) = 4
```

Cada elemento está separado por exactamente 4 bytes (`sizeof(int)`). La aritmética de punteros opera en unidades del tipo apuntado: `arr + i` no avanza `i` bytes, sino `i * sizeof(int)` bytes. Los arrays ocupan memoria contigua — no hay gaps entre elementos.

</details>

---

### Ejercicio 5 — Diferencia de punteros (`ptrdiff_t`)

Calcula la distancia (en elementos) entre dos punteros que apuntan al mismo array:

```c
#include <stdio.h>
#include <stddef.h>

int main(void) {
    int arr[8] = {10, 20, 30, 40, 50, 60, 70, 80};

    int *first = &arr[1];
    int *last  = &arr[6];

    ptrdiff_t dist = last - first;

    printf("first -> arr[1] = %d\n", *first);
    printf("last  -> arr[6] = %d\n", *last);
    printf("distance = %td elements\n", dist);
    printf("distance in bytes = %td\n",
           (char *)last - (char *)first);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex5.c -o ex5
./ex5
```

<details><summary>Predicción</summary>

```
first -> arr[1] = 20
last  -> arr[6] = 70
distance = 5 elements
distance in bytes = 20
```

`last - first` = 6 - 1 = 5 (en elementos, no bytes). La diferencia en bytes es 5 × 4 = 20. Para obtener bytes hay que castear a `char *` y restar. `ptrdiff_t` es un tipo con signo lo suficientemente grande para representar la diferencia entre dos punteros — se imprime con `%td`.

La diferencia de punteros solo es válida si ambos apuntan al mismo array. Si apuntan a arrays distintos, el resultado es UB.

</details>

---

### Ejercicio 6 — Reverse con dos punteros

Invierte un array usando dos punteros (sin índices):

```c
#include <stdio.h>

void reverse(int *arr, int n) {
    int *left  = arr;
    int *right = arr + n - 1;

    while (left < right) {
        int tmp = *left;
        *left  = *right;
        *right = tmp;
        left++;
        right--;
    }
}

void print_arr(const int *arr, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int main(void) {
    int a[] = {1, 2, 3, 4, 5};
    int b[] = {10, 20, 30, 40};

    printf("Before: "); print_arr(a, 5);
    reverse(a, 5);
    printf("After:  "); print_arr(a, 5);

    printf("\nBefore: "); print_arr(b, 4);
    reverse(b, 4);
    printf("After:  "); print_arr(b, 4);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
./ex6
```

<details><summary>Predicción</summary>

```
Before: 1 2 3 4 5
After:  5 4 3 2 1

Before: 10 20 30 40
After:  40 30 20 10
```

`left` parte de `arr` (inicio) y `right` de `arr + n - 1` (último elemento). Se intercambian valores y los punteros avanzan hacia el centro. El loop termina cuando se cruzan (`left >= right`).

Para 5 elementos: swap(0↔4), swap(1↔3), luego `left == right` (ambos en posición 2) → termina. El elemento central queda en su lugar.

Para 4 elementos: swap(0↔3), swap(1↔2), luego `left > right` → termina.

La ventaja de esta versión sobre índices es puramente estilística en C. En código de bajo nivel, mover punteros puede ser marginalmente más eficiente (un incremento vs una suma base + offset), aunque con `-O2` el compilador genera el mismo código.

</details>

---

### Ejercicio 7 — String literal vs array: mutabilidad

Demuestra la diferencia entre un puntero a string literal y un array char:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *ptr = "hello";
    char arr[] = "hello";

    printf("sizeof(ptr) = %zu (pointer)\n", sizeof(ptr));
    printf("sizeof(arr) = %zu (array: 5 chars + '\\0')\n", sizeof(arr));

    printf("\nptr = \"%s\"\n", ptr);
    printf("arr = \"%s\"\n", arr);

    /* Modify the array copy: */
    arr[0] = 'H';
    printf("\nAfter arr[0] = 'H':\n");
    printf("arr = \"%s\"\n", arr);

    /* ptr[0] = 'H';  would be UB — read-only memory */
    printf("ptr[0] = 'H' would be UB (read-only literal)\n");

    /* They are independent copies: */
    printf("\nptr still = \"%s\"\n", ptr);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
./ex7
```

<details><summary>Predicción</summary>

```
sizeof(ptr) = 8 (pointer)
sizeof(arr) = 6 (array: 5 chars + '\0')

ptr = "hello"
arr = "hello"

After arr[0] = 'H':
arr = "Hello"
ptr[0] = 'H' would be UB (read-only literal)

ptr still = "hello"
```

`ptr` es un puntero de 8 bytes que apunta a un string literal en memoria de solo lectura. `arr` es un array de 6 bytes (`"hello"` = 5 chars + `'\0'`) que contiene una **copia** del literal en el stack. Modificar `arr` es seguro. Modificar `*ptr` es UB porque los string literals residen en una sección de solo lectura del binario (`.rodata`).

`sizeof(ptr)` da 8 (tamaño del puntero); `sizeof(arr)` da 6 (tamaño del array). Esta es una de las tres excepciones al decay: inicialización de `char[]` con un string literal no causa decay.

</details>

---

### Ejercicio 8 — `arr` vs `&arr`: tipo y paso

Demuestra que `arr` y `&arr` tienen el mismo valor numérico pero tipos diferentes, resultando en pasos distintos:

```c
#include <stdio.h>

int main(void) {
    int arr[4] = {10, 20, 30, 40};

    int *p_int        = arr;      // int *
    int (*p_arr)[4]   = &arr;     // int (*)[4]

    printf("p_int   = %p\n", (void *)p_int);
    printf("p_arr   = %p\n", (void *)p_arr);
    printf("Same: %s\n\n", (void *)p_int == (void *)p_arr ? "yes" : "no");

    printf("p_int + 1 = %p  (+%td bytes)\n",
           (void *)(p_int + 1), (char *)(p_int + 1) - (char *)p_int);
    printf("p_arr + 1 = %p  (+%td bytes)\n",
           (void *)(p_arr + 1), (char *)(p_arr + 1) - (char *)p_arr);

    printf("\n*p_int     = %d (first element)\n", *p_int);
    printf("(*p_arr)[0] = %d (first element via deref + index)\n", (*p_arr)[0]);
    printf("**p_arr     = %d (first element via double deref)\n", **p_arr);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
p_int   = 0x7fff...
p_arr   = 0x7fff...
Same: yes

p_int + 1 = 0x7fff...  (+4 bytes)
p_arr + 1 = 0x7fff...  (+16 bytes)

*p_int     = 10 (first element)
(*p_arr)[0] = 10 (first element via deref + index)
**p_arr     = 10 (first element via double deref)
```

Ambos punteros contienen la misma dirección, pero `p_int + 1` avanza 4 bytes (un `int`) y `p_arr + 1` avanza 16 bytes (un array de 4 ints = 4 × 4). `*p_int` desreferencia a un `int` (valor 10). `*p_arr` desreferencia a un `int[4]` (el array, que decae a `int*`). Se necesita `**p_arr` o `(*p_arr)[i]` para acceder a los elementos.

</details>

---

### Ejercicio 9 — Modificar array desde función

Demuestra que una función puede modificar el array original (decay pasa la dirección, no una copia):

```c
#include <stdio.h>

void double_elements(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] *= 2;
    }
}

void fill_sequence(int *arr, int n, int start) {
    for (int i = 0; i < n; i++) {
        arr[i] = start + i;
    }
}

void print_arr(const int *arr, int n) {
    printf("{");
    for (int i = 0; i < n; i++) {
        printf("%d%s", arr[i], i < n - 1 ? ", " : "");
    }
    printf("}\n");
}

int main(void) {
    int data[5] = {1, 2, 3, 4, 5};

    printf("Original:  "); print_arr(data, 5);

    double_elements(data, 5);
    printf("Doubled:   "); print_arr(data, 5);

    fill_sequence(data, 5, 10);
    printf("Refilled:  "); print_arr(data, 5);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
./ex9
```

<details><summary>Predicción</summary>

```
Original:  {1, 2, 3, 4, 5}
Doubled:   {2, 4, 6, 8, 10}
Refilled:  {10, 11, 12, 13, 14}
```

Cuando `data` se pasa a `double_elements`, decae a un puntero al primer elemento. La función modifica los valores **en el array original** (no en una copia). Por eso el segundo `print_arr` muestra los valores duplicados, y el tercero muestra la nueva secuencia.

`print_arr` recibe `const int *` — promete no modificar. `double_elements` y `fill_sequence` reciben `int *` — pueden modificar. La distinción `const`/no-const en el parámetro comunica la intención al caller.

</details>

---

### Ejercicio 10 — Búsqueda con punteros: retornar posición

Implementa una búsqueda lineal que retorna un puntero al elemento encontrado (o `NULL`):

```c
#include <stdio.h>
#include <stddef.h>

int *find(int *arr, int n, int target) {
    int *end = arr + n;
    for (int *p = arr; p != end; p++) {
        if (*p == target) {
            return p;
        }
    }
    return NULL;
}

int main(void) {
    int data[] = {15, 42, 8, 23, 4, 16};
    int n = sizeof(data) / sizeof(data[0]);

    int targets[] = {23, 99, 42};
    for (int i = 0; i < 3; i++) {
        int *result = find(data, n, targets[i]);
        if (result) {
            ptrdiff_t idx = result - data;
            printf("find(%d) -> index %td, value %d\n",
                   targets[i], idx, *result);
        } else {
            printf("find(%d) -> NULL\n", targets[i]);
        }
    }

    /* Modify through returned pointer: */
    int *p = find(data, n, 42);
    if (p) {
        *p = 100;
        printf("\nAfter *find(42) = 100: data[1] = %d\n", data[1]);
    }

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
find(23) -> index 3, value 23
find(99) -> NULL
find(42) -> index 1, value 42

After *find(42) = 100: data[1] = 100
```

`find` retorna un puntero al elemento encontrado. El índice se calcula como `result - data` (diferencia de punteros = distancia en elementos). 23 está en la posición 3, 99 no existe (retorna NULL), 42 está en la posición 1.

Retornar un puntero (en vez de un índice) tiene una ventaja: permite modificar el elemento directamente a través del puntero retornado (`*p = 100`). También permite verificar "no encontrado" con `NULL` en vez de un valor mágico como -1. Este es el patrón que usan funciones de la stdlib como `memchr` y `strchr`.

</details>
