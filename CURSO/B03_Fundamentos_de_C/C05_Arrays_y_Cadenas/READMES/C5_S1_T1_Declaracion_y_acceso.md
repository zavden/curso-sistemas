# T01 — Declaración y acceso a arrays

> Sin erratas detectadas en el material base.

---

## 1. Declaración de arrays

Un array es una secuencia contigua en memoria de elementos del mismo tipo:

```c
int nums[5];         // 5 ints, valores indeterminados (basura)
double temps[3];     // 3 doubles
char name[20];       // 20 chars
```

`sizeof` del array = tamaño del elemento × cantidad de elementos:

```c
sizeof(nums)   == 20   // 5 × sizeof(int) = 5 × 4
sizeof(temps)  == 24   // 3 × sizeof(double) = 3 × 8
sizeof(name)   == 20   // 20 × sizeof(char) = 20 × 1
```

El tamaño debe ser una constante en C89. En C99+, puede ser una variable (VLA):

```c
int arr[10];          // OK — literal
#define N 100
int arr2[N];          // OK — macro se expande a literal

int n = 10;
int arr3[n];          // OK en C99+ — VLA (Variable-Length Array)

int bad[0];           // UB en el estándar; GCC lo permite como extensión
```

---

## 2. Inicialización

### Completa

```c
int nums[5] = {10, 20, 30, 40, 50};
```

### Parcial — el resto se pone a 0

```c
int nums[5] = {10, 20};    // → {10, 20, 0, 0, 0}
```

Los elementos no especificados se inicializan a **0**. Esto es una garantía del estándar C, no un accidente del compilador.

### Poner todo a cero

```c
int zeros[100] = {0};     // Inicializa el primero a 0, el resto también a 0
int zeros2[100] = {};      // C23: equivalente a {0}
```

`{0}` es el idioma estándar en C para "todo a cero". Funciona porque la regla de inicialización parcial rellena los huecos con 0.

### Tamaño automático

```c
int auto_size[] = {1, 2, 3, 4};
// El compilador cuenta: tamaño = 4
// sizeof(auto_size) / sizeof(auto_size[0]) == 4
```

### Designated initializers (C99)

```c
int arr[10] = {
    [0] = 100,
    [5] = 500,
    [9] = 900,
};
// → {100, 0, 0, 0, 0, 500, 0, 0, 0, 900}
```

Permiten inicializar posiciones específicas. Los huecos se rellenan con 0. El índice puede estar desordenado y se puede combinar con inicialización posicional:

```c
int arr[5] = { 1, 2, [4] = 5 };   // → {1, 2, 0, 0, 5}
```

### Arrays no son asignables

```c
int a[3] = {1, 2, 3};
int b[3];
b = a;                  // ERROR: array no es un lvalue asignable

#include <string.h>
memcpy(b, a, sizeof(a));  // Correcto: copia byte a byte
```

---

## 3. Acceso a elementos y out-of-bounds

```c
int arr[5] = {10, 20, 30, 40, 50};

arr[0];     // 10 — primer elemento (índice 0)
arr[4];     // 50 — último elemento (tamaño - 1)
arr[2] = 99;   // Modificar un elemento
```

### C no verifica límites

```c
arr[5]  = 42;     // UB — escribe después del array
int x = arr[-1];  // UB — lee antes del array
```

Esto **compila sin errores ni warnings**. El resultado es impredecible: puede funcionar "por casualidad", corromper datos, o crashear. Es **Undefined Behavior**.

### Detectar OOB con sanitizers

```bash
gcc -fsanitize=address programa.c -o programa
./programa
# ERROR: AddressSanitizer: stack-buffer-overflow on address ...
```

`-fsanitize=address` (ASan) agrega verificaciones en runtime que detectan accesos fuera de límites y abortan con un reporte detallado. Invaluable para depurar bugs de memoria.

### Patrón de iteración

```c
int data[5] = {10, 20, 30, 40, 50};
int n = sizeof(data) / sizeof(data[0]);  // 5

for (int i = 0; i < n; i++) {
    printf("data[%d] = %d\n", i, data[i]);
}
```

`sizeof(data) / sizeof(data[0])` funciona solo con arrays reales (no punteros). Ver sección 5.

---

## 4. Macro `ARRAY_SIZE`

```c
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int nums[] = {1, 2, 3, 4, 5};
for (int i = 0; i < (int)ARRAY_SIZE(nums); i++) {
    printf("%d\n", nums[i]);
}
```

Este patrón estándar calcula el número de elementos de un array. Funciona **solo con arrays reales**, no con punteros. Si pasas un puntero, da un resultado incorrecto sin error de compilación.

El kernel de Linux usa una versión más segura que detecta si recibe un puntero:

```c
// Versión del kernel (simplificada):
#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) + \
     sizeof(typeof(int[1 - 2 * __builtin_types_compatible_p(typeof(arr), typeof(&(arr)[0]))])))
// Error de compilación si arr es un puntero
```

---

## 5. Array decay a puntero en funciones

Cuando un array se pasa a una función, **decae a puntero al primer elemento**. Se pierde la información de tamaño:

```c
void print_info(int arr[], int len) {
    // arr es un PUNTERO, no un array
    sizeof(arr);  // → 8 (tamaño de int* en x86-64), NO tamaño del array
    // ARRAY_SIZE(arr) → 8/4 = 2 → ¡INCORRECTO!
}

int main(void) {
    int data[5] = {10, 20, 30, 40, 50};
    sizeof(data);   // → 20 (correcto: 5 × 4)

    print_info(data, 5);  // data decae a &data[0]
}
```

GCC emite un warning cuando usas `sizeof` en un parámetro array:

```
warning: 'sizeof' on array function parameter 'arr'
         will return size of 'int *' [-Wsizeof-array-argument]
```

Estas tres declaraciones de parámetro son **idénticas** — todas son `int *`:

```c
void f(int arr[]);      // Parece array, pero es puntero
void f(int arr[10]);    // El 10 se ignora — sigue siendo puntero
void f(int *arr);       // Lo que realmente es
```

**Solución**: siempre pasar el tamaño como parámetro separado:

```c
void process(const int *arr, int len) {
    for (int i = 0; i < len; i++) {
        printf("%d\n", arr[i]);
    }
}
```

---

## 6. Variable-Length Arrays (VLAs)

C99 introdujo arrays cuyo tamaño se determina en runtime:

```c
void process(int n) {
    int data[n];  // VLA — se aloca en el stack
    // sizeof(data) se evalúa en RUNTIME (no en compilación)
    // No se puede inicializar con {}
}
```

### VLAs como parámetros

```c
void print_matrix(int rows, int cols, int mat[rows][cols]) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            printf("%4d", mat[i][j]);
}
// rows y cols deben aparecer ANTES de mat en la lista de parámetros
```

### Problemas de los VLAs

1. **Stack overflow**: sin control de tamaño. `int arr[1000000]` puede crashear silenciosamente.
2. **No se pueden inicializar**: `int arr[n] = {0}` es un error.
3. **Rendimiento**: ajuste del stack pointer en runtime vs frame fijo en compilación.
4. **Soporte inconsistente**: opcionales en C11 (`__STDC_NO_VLA__`), no soportados por MSVC, prohibidos en el kernel de Linux (`-Wvla`).

### Alternativas

```c
// Para tamaños variables → malloc:
int *data = malloc(n * sizeof(int));
if (!data) { perror("malloc"); return; }
// ... usar data ...
free(data);

// Para tamaños fijos pequeños → constante:
#define MAX_ITEMS 256
int data[MAX_ITEMS];
```

---

## 7. Arrays `const`

```c
const int primes[] = {2, 3, 5, 7, 11, 13};
primes[0] = 100;  // ERROR: assignment to const

// Tabla de lookup inmutable:
static const char *const months[] = {
    "January", "February", "March", /*...*/ "December"
};
// const char *const:
//   - const char *  → el contenido del string es const
//   - *const        → el puntero mismo es const
// static: solo visible en este translation unit
```

---

## 8. Comparación con Rust

### Arrays de tamaño fijo

```rust
// Rust: tamaño es parte del tipo
let nums: [i32; 5] = [10, 20, 30, 40, 50];
let zeros = [0i32; 100];  // 100 ceros

// El tamaño es conocido en compilación — .len() es constante:
println!("{}", nums.len());  // 5
```

En Rust, `[i32; 5]` y `[i32; 3]` son tipos **diferentes**. No puedes pasar uno donde se espera el otro.

### Acceso con verificación

```rust
let arr = [10, 20, 30];
arr[5];  // panic! en runtime — Rust verifica límites
```

Rust siempre verifica límites por defecto. Para evitar el costo, se usa `.get()` que retorna `Option`, o `unsafe` con `get_unchecked()`.

### Slices en vez de decay

```rust
fn process(data: &[i32]) {
    // data es un "fat pointer": puntero + longitud
    println!("len = {}", data.len());  // siempre correcto
}

let arr = [10, 20, 30, 40, 50];
process(&arr);  // No pierde la información de tamaño
```

En Rust, pasar un array a una función crea un slice (`&[T]`) que contiene puntero **y** longitud — resuelve el problema de decay de C.

### No hay VLAs

Rust no tiene VLAs. Para tamaños dinámicos se usa `Vec<T>` (heap):

```rust
let data: Vec<i32> = (0..n).map(|i| i * 10).collect();
```

---

## Ejercicios

### Ejercicio 1 — Designated initializers y valores por defecto

Declara un array de 8 ints con designated initializers: posición 0 = 10, posición 3 = 30, posición 7 = 70. Imprime todos los elementos.

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void) {
    int arr[8] = {
        [0] = 10,
        [3] = 30,
        [7] = 70,
    };

    for (int i = 0; i < (int)ARRAY_SIZE(arr); i++) {
        printf("arr[%d] = %d\n", i, arr[i]);
    }
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
./ex1
```

<details><summary>Predicción</summary>

```
arr[0] = 10
arr[1] = 0
arr[2] = 0
arr[3] = 30
arr[4] = 0
arr[5] = 0
arr[6] = 0
arr[7] = 70
```

Los designated initializers establecen las posiciones 0, 3 y 7. Todas las demás se inicializan a 0 por la regla del estándar C: en una inicialización parcial, los elementos no especificados se ponen a cero.

</details>

---

### Ejercicio 2 — `ARRAY_SIZE` y promedio

Crea un array de doubles con tamaño automático y calcula el promedio usando `ARRAY_SIZE`:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void) {
    double scores[] = {8.5, 9.0, 7.5, 6.0, 9.5, 8.0};
    int n = (int)ARRAY_SIZE(scores);

    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += scores[i];
    }
    double avg = sum / n;

    printf("Scores (%d elements):\n", n);
    for (int i = 0; i < n; i++) {
        printf("  [%d] = %.1f\n", i, scores[i]);
    }
    printf("Average: %.2f\n", avg);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
./ex2
```

<details><summary>Predicción</summary>

```
Scores (6 elements):
  [0] = 8.5
  [1] = 9.0
  [2] = 7.5
  [3] = 6.0
  [4] = 9.5
  [5] = 8.0
Average: 8.08
```

`ARRAY_SIZE(scores)` = `sizeof(scores) / sizeof(scores[0])` = `48 / 8` = `6`. El compilador determinó el tamaño del array por el número de inicializadores (6 doubles). La suma es 48.5, dividida entre 6 = 8.0833... → imprime `8.08`.

</details>

---

### Ejercicio 3 — sizeof en array vs en función

Demuestra la diferencia entre `sizeof` aplicado a un array real y a un parámetro de función:

```c
#include <stdio.h>

void show_sizeof(int arr[], int len) {
    printf("  Inside function:\n");
    printf("    sizeof(arr) = %zu (pointer size)\n", sizeof(arr));
    printf("    len param   = %d\n", len);
}

int main(void) {
    int data[6] = {1, 2, 3, 4, 5, 6};

    printf("In main:\n");
    printf("  sizeof(data) = %zu\n", sizeof(data));
    printf("  elements     = %zu\n", sizeof(data) / sizeof(data[0]));

    show_sizeof(data, 6);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
./ex3
```

<details><summary>Predicción</summary>

El compilador emite un warning:
```
ex3.c: In function 'show_sizeof':
warning: 'sizeof' on array function parameter 'arr' will return size of 'int *'
```

Salida:
```
In main:
  sizeof(data) = 24
  elements     = 6

  Inside function:
    sizeof(arr) = 8 (pointer size)
    len param   = 6
```

En `main`, `data` es un array real: `sizeof(data)` = 6 × 4 = 24. Dentro de `show_sizeof`, `arr` es un puntero (decay): `sizeof(arr)` = 8 (tamaño de `int*` en x86-64). La información de tamaño se perdió. Por eso siempre se pasa `len` como parámetro separado.

</details>

---

### Ejercicio 4 — Out-of-bounds con y sin ASan

Accede fuera de los límites de un array y observa la diferencia con y sin AddressSanitizer:

```c
#include <stdio.h>

int main(void) {
    int arr[4] = {10, 20, 30, 40};

    printf("arr[0] = %d\n", arr[0]);
    printf("arr[3] = %d\n", arr[3]);

    /* OOB: undefined behavior */
    printf("arr[4] = %d  (UB)\n", arr[4]);

    return 0;
}
```

```bash
echo "=== Sin sanitizer ==="
gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
./ex4

echo ""
echo "=== Con AddressSanitizer ==="
gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address ex4.c -o ex4_asan
./ex4_asan
```

<details><summary>Predicción</summary>

Sin sanitizer:
```
arr[0] = 10
arr[3] = 40
arr[4] = <valor impredecible>  (UB)
```

El programa compila sin warnings (GCC no siempre detecta OOB estático). `arr[4]` lee lo que sea que esté en esa posición del stack — un valor de basura. El programa no crashea porque la memoria es accesible (está en el stack), pero el resultado es indefinido.

Con ASan:
```
arr[0] = 10
arr[3] = 40
=================================================================
==<pid>==ERROR: AddressSanitizer: stack-buffer-overflow on address ...
READ of size 4 at ...
```

ASan detecta el acceso fuera de límites y aborta con un reporte detallado que incluye la dirección, el tamaño del acceso, y el stack trace. Es una herramienta esencial para desarrollo en C.

</details>

---

### Ejercicio 5 — Inicialización parcial vs sin inicializar

Compara un array con inicialización parcial contra uno sin inicializar:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void) {
    int partial[5] = {42};
    int uninit[5];

    printf("partial (initialized with {42}):\n");
    for (int i = 0; i < (int)ARRAY_SIZE(partial); i++) {
        printf("  [%d] = %d\n", i, partial[i]);
    }

    printf("\nuninit (no initializer):\n");
    for (int i = 0; i < (int)ARRAY_SIZE(uninit); i++) {
        printf("  [%d] = %d\n", i, uninit[i]);
    }

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex5.c -o ex5
./ex5
```

<details><summary>Predicción</summary>

```
partial (initialized with {42}):
  [0] = 42
  [1] = 0
  [2] = 0
  [3] = 0
  [4] = 0

uninit (no initializer):
  [0] = <basura>
  [1] = <basura>
  [2] = <basura>
  [3] = <basura>
  [4] = <basura>
```

`partial` tiene inicializador, así que el estándar garantiza: `[0] = 42`, el resto = 0.

`uninit` NO tiene inicializador. Sus valores son **indeterminados** — lo que haya en esa zona del stack. Leer estos valores es técnicamente UB (comportamiento indefinido en ciertos contextos). En la práctica, GCC puede emitir un warning con `-Wuninitialized` si detecta el uso.

Moraleja: siempre inicializar arrays, al menos con `= {0}`.

</details>

---

### Ejercicio 6 — VLA: sizeof en runtime

Crea un VLA y observa cómo `sizeof` se evalúa en runtime:

```c
#include <stdio.h>

void create_vla(int n) {
    int data[n];  /* VLA */

    for (int i = 0; i < n; i++) {
        data[i] = i * i;
    }

    printf("n = %2d, sizeof(data) = %3zu, elements: ", n, sizeof(data));
    for (int i = 0; i < n; i++) {
        printf("%d ", data[i]);
    }
    printf("\n");
}

int main(void) {
    create_vla(3);
    create_vla(5);
    create_vla(10);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
./ex6
```

<details><summary>Predicción</summary>

```
n =  3, sizeof(data) =  12, elements: 0 1 4
n =  5, sizeof(data) =  20, elements: 0 1 4 9 16
n = 10, sizeof(data) =  40, elements: 0 1 4 9 16 25 36 49 64 81
```

`sizeof(data)` cambia en cada llamada: 12, 20, 40 (= n × 4). Con un array de tamaño fijo, `sizeof` se evalúa en compilación y es constante. Con un VLA, se evalúa en **runtime** — el compilador genera código que multiplica `n × sizeof(int)`.

Los VLAs se alocan en el stack. Si `n` fuera enorme (p.ej. 10 millones), causaría stack overflow sin mensaje de error — por eso son peligrosos y el kernel de Linux los prohíbe.

</details>

---

### Ejercicio 7 — Array `const` como tabla de lookup

Crea una tabla de lookup inmutable y úsala para traducir índices a strings:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char *const day_names[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday"
};

const char *get_day_name(int day) {
    if (day < 0 || day >= (int)ARRAY_SIZE(day_names)) {
        return "INVALID";
    }
    return day_names[day];
}

int main(void) {
    for (int i = -1; i <= 7; i++) {
        printf("day(%2d) = %s\n", i, get_day_name(i));
    }
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
./ex7
```

<details><summary>Predicción</summary>

```
day(-1) = INVALID
day( 0) = Monday
day( 1) = Tuesday
day( 2) = Wednesday
day( 3) = Thursday
day( 4) = Friday
day( 5) = Saturday
day( 6) = Sunday
day( 7) = INVALID
```

`day_names` es `static const char *const`: `static` limita la visibilidad al archivo, el primer `const` hace que los strings sean read-only, el segundo `const` hace que los punteros del array sean read-only. `ARRAY_SIZE` funciona correctamente porque `day_names` es un array real (no un puntero). La función valida el rango antes de acceder — patrón defensivo contra OOB.

</details>

---

### Ejercicio 8 — Combinar designated initializers con posicional

Experimenta con la interacción entre inicialización posicional y designated initializers:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void) {
    /* Positional then designated */
    int a[6] = { 1, 2, 3, [5] = 50 };

    /* Designated then positional (continues from designated index) */
    int b[6] = { [2] = 20, 21, 22 };

    /* Overlapping: designated overwrites positional */
    int c[5] = { 1, 2, 3, 4, 5, [1] = 99 };

    printf("a = {");
    for (int i = 0; i < (int)ARRAY_SIZE(a); i++)
        printf("%d%s", a[i], i < 5 ? ", " : "");
    printf("}\n");

    printf("b = {");
    for (int i = 0; i < (int)ARRAY_SIZE(b); i++)
        printf("%d%s", b[i], i < 5 ? ", " : "");
    printf("}\n");

    printf("c = {");
    for (int i = 0; i < (int)ARRAY_SIZE(c); i++)
        printf("%d%s", c[i], i < 3 ? ", " : (i < 4 ? ", " : ""));
    printf("}\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
a = {1, 2, 3, 0, 0, 50}
b = {0, 0, 20, 21, 22, 0}
c = {1, 99, 3, 4, 5}
```

**Array `a`**: posiciones 0, 1, 2 por inicialización posicional (1, 2, 3); posiciones 3 y 4 son 0 (no especificadas); posición 5 = 50 por designador.

**Array `b`**: `[2] = 20` inicia en la posición 2; luego `21, 22` continúan posicionalmente desde la 3 y 4. Posiciones 0, 1, 5 son 0.

**Array `c`**: primero se ponen 1–5 posicionalmente; luego `[1] = 99` **sobrescribe** la posición 1 (que era 2). El compilador puede emitir un warning: `initialized field overwritten`.

Nota: `c` tiene 6 inicializadores para un array de 5 — esto NO es un error cuando un designator "retrocede" y sobrescribe. Pero GCC puede advertir con `-Woverride-init`.

</details>

---

### Ejercicio 9 — Búsqueda lineal con verificación de bounds

Implementa una búsqueda lineal que retorna el índice o -1, con verificación de parámetros:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int find(const int *arr, int len, int target) {
    if (arr == NULL || len <= 0) {
        return -1;
    }
    for (int i = 0; i < len; i++) {
        if (arr[i] == target) {
            return i;
        }
    }
    return -1;
}

int main(void) {
    int data[] = {15, 42, 8, 23, 4, 16, 42};
    int n = (int)ARRAY_SIZE(data);

    int targets[] = {23, 42, 99, 4};

    for (int i = 0; i < (int)ARRAY_SIZE(targets); i++) {
        int idx = find(data, n, targets[i]);
        if (idx >= 0) {
            printf("find(%d) = index %d\n", targets[i], idx);
        } else {
            printf("find(%d) = not found\n", targets[i]);
        }
    }

    /* Test with NULL */
    printf("find in NULL = %d\n", find(NULL, 0, 42));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
./ex9
```

<details><summary>Predicción</summary>

```
find(23) = index 3
find(42) = index 1
find(99) = not found
find(4) = index 4
find in NULL = -1
```

`find` recorre el array linealmente y retorna el índice de la **primera** ocurrencia. 42 aparece en posiciones 1 y 6, pero retorna 1 (la primera). 99 no está en el array → retorna -1. La función recibe `const int *` (no `int arr[]`) y `len` separado — el patrón correcto en C ya que no puede confiar en `sizeof` dentro de la función. La verificación de `NULL` al inicio es un guard defensivo.

</details>

---

### Ejercicio 10 — Invertir array in-place

Invierte un array sin usar un array auxiliar, usando swap con índices:

```c
#include <stdio.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void reverse(int *arr, int len) {
    for (int i = 0; i < len / 2; i++) {
        int tmp = arr[i];
        arr[i] = arr[len - 1 - i];
        arr[len - 1 - i] = tmp;
    }
}

void print_array(const int *arr, int len) {
    printf("{");
    for (int i = 0; i < len; i++) {
        printf("%d%s", arr[i], i < len - 1 ? ", " : "");
    }
    printf("}\n");
}

int main(void) {
    int even[] = {1, 2, 3, 4, 5, 6};
    int odd[]  = {10, 20, 30, 40, 50};
    int one[]  = {42};

    printf("Before: "); print_array(even, (int)ARRAY_SIZE(even));
    reverse(even, (int)ARRAY_SIZE(even));
    printf("After:  "); print_array(even, (int)ARRAY_SIZE(even));

    printf("\nBefore: "); print_array(odd, (int)ARRAY_SIZE(odd));
    reverse(odd, (int)ARRAY_SIZE(odd));
    printf("After:  "); print_array(odd, (int)ARRAY_SIZE(odd));

    printf("\nBefore: "); print_array(one, (int)ARRAY_SIZE(one));
    reverse(one, (int)ARRAY_SIZE(one));
    printf("After:  "); print_array(one, (int)ARRAY_SIZE(one));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
Before: {1, 2, 3, 4, 5, 6}
After:  {6, 5, 4, 3, 2, 1}

Before: {10, 20, 30, 40, 50}
After:  {50, 40, 30, 20, 10}

Before: {42}
After:  {42}
```

`reverse` recorre solo la primera mitad (`len / 2`) e intercambia el elemento `i` con el `len - 1 - i`. Para 6 elementos: hace 3 swaps (posiciones 0↔5, 1↔4, 2↔3). Para 5 elementos: hace 2 swaps (0↔4, 1↔3), el central (posición 2) queda en su lugar (`5/2 = 2`, así que `i` va de 0 a 1). Para 1 elemento: `1/2 = 0`, el loop no ejecuta ninguna iteración — el array queda igual.

La función recibe `int *arr` (no `const`) porque modifica el array. `print_array` recibe `const int *` porque solo lee.

</details>
