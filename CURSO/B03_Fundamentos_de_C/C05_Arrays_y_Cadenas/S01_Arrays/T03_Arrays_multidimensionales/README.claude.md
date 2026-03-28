# T03 — Arrays multidimensionales

> Sin erratas detectadas en el material base.

---

## 1. Arrays 2D: declaración e inicialización

Un array 2D es un **array de arrays**. `int matrix[3][4]` es un array de 3 elementos, donde cada elemento es un array de 4 ints:

```c
int matrix[3][4] = {
    {1,  2,  3,  4},    // fila 0
    {5,  6,  7,  8},    // fila 1
    {9, 10, 11, 12},    // fila 2
};

matrix[1][2];   // 7 — fila 1, columna 2
```

### Formas de inicialización

```c
// Completa con llaves internas:
int m[2][3] = { {1,2,3}, {4,5,6} };

// Sin llaves internas — C llena fila por fila:
int m2[2][3] = {1, 2, 3, 4, 5, 6};   // equivalente

// Parcial — el resto es 0:
int m3[2][3] = { {1}, {4, 5} };       // → {{1,0,0}, {4,5,0}}

// Todo a cero:
int m4[3][3] = {0};

// Deducir primera dimensión:
int m5[][3] = { {1,2,3}, {4,5,6} };   // → 2 filas
// Solo la PRIMERA dimensión se puede omitir.
// int m[][] = {...};   // ERROR
```

---

## 2. Layout en memoria: row-major order

En memoria, un array 2D es **contiguo** — no hay huecos entre filas. C usa **row-major order**: las filas se almacenan una tras otra:

```
matrix[3][4] en memoria:

Offset:  0x00  0x04  0x08  0x0C  0x10  0x14  0x18  0x1C  0x20  0x24  0x28  0x2C
Valor:   [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]   [10]  [11]  [12]
         |------- fila 0 ------|------- fila 1 ------|------- fila 2 ------|
```

La fórmula de acceso:

```
matrix[i][j] está en: base + (i * COLS + j) * sizeof(int)
matrix[1][2] → base + (1*4 + 2) * 4 = base + 24 bytes → valor 7
```

Consecuencias:
- `sizeof(matrix)` = 3 × 4 × 4 = 48 bytes (todo el bloque).
- `sizeof(matrix[0])` = 4 × 4 = 16 bytes (una fila).
- Se puede recorrer con un puntero plano `int *p = &matrix[0][0]`.

**Row-major** lo usan: C, C++, Python (NumPy default), Rust.
**Column-major** lo usan: Fortran, MATLAB, Julia.

---

## 3. Cache performance: por qué importa el orden de recorrido

```c
#define N 1000
int matrix[N][N];

// Row-major (EFICIENTE) — acceso secuencial:
for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
        matrix[i][j] = 0;
// → cache line se usa completamente

// Column-major (INEFICIENTE) — saltos de N*sizeof(int):
for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++)
        matrix[i][j] = 0;
// → cada acceso puede ser un cache miss
```

Para arrays pequeños la diferencia es imperceptible. En matrices de 1000×1000+, el recorrido column-major puede ser **5-10x más lento** por cache misses.

Regla: el índice que cambia más rápido (loop interno) debe ser el de la **última dimensión** (columnas en C).

---

## 4. Pasar arrays 2D a funciones: cuatro formas

El compilador necesita conocer el número de columnas para calcular `mat[i][j] = *(base + i*COLS + j)`. Sin `COLS`, no puede calcular el offset.

### Forma 1: notación de array (COLS fijo)

```c
void print_matrix(int mat[][4], int rows) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < 4; j++)
            printf("%4d", mat[i][j]);
}
```

### Forma 2: puntero a array (equivalente a forma 1)

```c
void print_matrix(int (*mat)[4], int rows) {
    // int (*mat)[4] ≡ int mat[][4] como parámetro
    // mat es un puntero a arrays de 4 ints
}
```

### Forma 3: VLA en parámetro (C99+ — dimensiones flexibles)

```c
void print_matrix(int rows, int cols, int mat[rows][cols]) {
    // rows y cols deben aparecer ANTES de mat
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            printf("%4d", mat[i][j]);
}
```

### Forma 4: puntero plano (siempre funciona)

```c
void print_matrix(int *mat, int rows, int cols) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            printf("%4d", mat[i * cols + j]);
}
// Llamar: print_matrix(&m[0][0], 3, 4);
```

| Firma | Columnas fijas | Dimensiones flexibles |
|-------|:-:|:-:|
| `int mat[][N]` | Sí | No |
| `int (*mat)[N]` | Sí | No |
| VLA `int mat[rows][cols]` | No | Sí |
| `int *mat` (flat) | No | Sí |

---

## 5. Array 2D real vs array de punteros (ragged array)

### Array 2D real — contiguo

```c
int matrix[3][4];
// Memoria: [fila0: 4 ints][fila1: 4 ints][fila2: 4 ints]
//          ←──────── 48 bytes contiguos ──────────→
// sizeof(matrix) = 48
// 1 indirección: *(base + i*cols + j)
```

### Array de punteros — fragmentado

```c
int *rows[3];
rows[0] = malloc(4 * sizeof(int));
rows[1] = malloc(4 * sizeof(int));
rows[2] = malloc(4 * sizeof(int));
// Memoria: rows = [ptr0][ptr1][ptr2]  (24 bytes)
//                   ↓     ↓     ↓
//                [4 ints][4 ints][4 ints]  (en ubicaciones independientes)
// sizeof(rows) = 24 (solo los punteros)
// 2 indirecciones: *(*(rows + i) + j)
```

La sintaxis de acceso (`x[i][j]`) es idéntica, pero el mecanismo es diferente.

| Aspecto | `int m[3][4]` | `int *m[3]` |
|---------|---------------|-------------|
| Memoria | Contigua | Fragmentada |
| sizeof | 48 (datos) | 24 (punteros) |
| Filas de distinto tamaño | No | Sí (ragged) |
| Cache | Excelente | Puede ser malo |
| Pasar a función | Necesita cols fijo | Solo `int **` |
| Indirecciones | 1 | 2 |

### Cuándo usar cada uno

- **Array 2D real**: tamaño conocido en compilación, filas uniformes, rendimiento importa. Ej: `int board[8][8]`, `int pixels[1080][1920]`.
- **Array de punteros**: filas de distinto tamaño, tamaño en runtime, necesidad de `int **`. Ej: `argv`, líneas de texto de longitud variable.

---

## 6. Malloc para matrices: tres estrategias

### Estrategia 1: `int **` — array de punteros (simple pero fragmentado)

```c
int **create_matrix(int rows, int cols) {
    int **mat = malloc(rows * sizeof(int *));
    if (!mat) return NULL;
    for (int i = 0; i < rows; i++) {
        mat[i] = malloc(cols * sizeof(int));
        if (!mat[i]) {
            for (int j = 0; j < i; j++) free(mat[j]);
            free(mat);
            return NULL;
        }
    }
    return mat;
}

void free_matrix(int **mat, int rows) {
    for (int i = 0; i < rows; i++) free(mat[i]);
    free(mat);
}
```

Ventaja: sintaxis natural `mat[i][j]`. Desventaja: `rows + 1` mallocs, memoria fragmentada.

### Estrategia 2: `int *` — bloque contiguo (eficiente pero sintaxis fea)

```c
int *create_flat(int rows, int cols) {
    return calloc(rows * cols, sizeof(int));
}
// Acceso: mat[i * cols + j]
// Un solo malloc, un solo free
```

Ventaja: contiguo, cache-friendly, un solo `free`. Desventaja: `mat[i * cols + j]` es menos legible.

### Estrategia 3: contiguo + array de punteros (lo mejor de ambos mundos)

```c
int **create_matrix_contig(int rows, int cols) {
    int **mat = malloc(rows * sizeof(int *));
    int *data = calloc(rows * cols, sizeof(int));
    if (!mat || !data) { free(mat); free(data); return NULL; }
    for (int i = 0; i < rows; i++)
        mat[i] = data + i * cols;  // cada fila apunta dentro de data
    return mat;
}

void free_matrix_contig(int **mat) {
    free(mat[0]);   // libera el bloque de datos
    free(mat);      // libera el array de punteros
}
// Sintaxis: mat[i][j] — natural
// Solo 2 mallocs, memoria contigua
```

---

## 7. Arrays de strings: `const char *arr[]` vs `char arr[][N]`

```c
// Array de punteros a string literals:
const char *names[] = {"Alice", "Bob", "Charlie"};
// sizeof(names) = 24 (3 punteros × 8)
// Cada string ocupa solo lo necesario
// Read-only — modificar es UB

// Array 2D de chars con buffer fijo:
char cities[][12] = {"Madrid", "Barcelona", "Valencia"};
// sizeof(cities) = 36 (3 filas × 12 bytes)
// Cada fila es un buffer de 12 bytes (puede desperdiciar espacio)
// Modificable — son copias en el stack
```

| Aspecto | `const char *arr[]` | `char arr[][N]` |
|---------|---------------------|-----------------|
| sizeof | Punteros (8 bytes c/u) | Filas × N bytes |
| Tamaño variable por string | Sin desperdicio | Buffer fijo |
| Modificable | No (string literals) | Sí (buffer propio) |
| Layout | Fragmentado | Contiguo |
| Uso típico | Tablas de constantes, argv | Buffers editables |

---

## 8. Arrays de más dimensiones

```c
int cube[2][3][4];   // 2 "capas", cada una de 3×4
// sizeof(cube) = 2 * 3 * 4 * sizeof(int) = 96 bytes

cube[0][1][2] = 42;
// Offset: (0*3*4 + 1*4 + 2) * sizeof(int) = 24 bytes

// Para 3D+, abstraer con función de acceso:
static inline int idx3d(int i, int j, int k, int d2, int d3) {
    return i * d2 * d3 + j * d3 + k;
}

int *data = calloc(2 * 3 * 4, sizeof(int));
data[idx3d(0, 1, 2, 3, 4)] = 42;
```

En la práctica, arrays de más de 2 dimensiones son raros en C. Para datos multidimensionales complejos, se usa un bloque plano con funciones de acceso.

---

## 9. Comparación con Rust

### Arrays multidimensionales

```rust
// Array 2D: array de arrays (tamaño en el tipo)
let matrix: [[i32; 4]; 3] = [
    [1, 2, 3, 4],
    [5, 6, 7, 8],
    [9, 10, 11, 12],
];
matrix[1][2];  // 7 — con bounds checking
```

El layout es row-major como en C, pero Rust verifica límites en cada acceso.

### Slices 2D

```rust
fn print_matrix(mat: &[[i32; 4]]) {
    for row in mat {
        for &val in row {
            print!("{:4}", val);
        }
        println!();
    }
}
// El número de columnas (4) está en el tipo
// El número de filas viene del slice (&[...])
```

### Vec para matrices dinámicas

```rust
// Equivalente a la estrategia flat con malloc:
let rows = 3;
let cols = 4;
let mut mat = vec![0i32; rows * cols];
mat[1 * cols + 2] = 42;

// O con Vec<Vec<T>> (equivalente a int **):
let mut mat: Vec<Vec<i32>> = vec![vec![0; cols]; rows];
mat[1][2] = 42;
```

Rust no tiene un equivalente directo a `int m[rows][cols]` con VLA. Para tamaños dinámicos, se usa `Vec`.

---

## Ejercicios

### Ejercicio 1 — Declarar, inicializar y recorrer

Declara una matriz 3×3, inicialízala con valores y recórrela imprimiendo en formato de tabla:

```c
#include <stdio.h>

#define ROWS 3
#define COLS 3

int main(void) {
    int m[ROWS][COLS] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
    };

    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%4d", m[i][j]);
        }
        printf("\n");
    }

    printf("\nsizeof(m)    = %zu bytes\n", sizeof(m));
    printf("sizeof(m[0]) = %zu bytes (one row)\n", sizeof(m[0]));
    printf("Total ints   = %zu\n", sizeof(m) / sizeof(int));

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
./ex1
```

<details><summary>Predicción</summary>

```
   1   2   3
   4   5   6
   7   8   9

sizeof(m)    = 36 bytes
sizeof(m[0]) = 12 bytes (one row)
Total ints   = 9
```

`sizeof(m)` = 3 × 3 × 4 = 36 bytes. `sizeof(m[0])` = 3 × 4 = 12 bytes (una fila de 3 ints). Total de ints = 36 / 4 = 9. La matriz ocupa un bloque contiguo de 36 bytes sin huecos.

</details>

---

### Ejercicio 2 — Verificar contigüidad con direcciones

Imprime las direcciones de cada elemento para confirmar row-major order:

```c
#include <stdio.h>

int main(void) {
    int m[2][3] = { {10, 20, 30}, {40, 50, 60} };

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            printf("m[%d][%d] = %2d  addr: %p  offset: %td\n",
                   i, j, m[i][j], (void *)&m[i][j],
                   (char *)&m[i][j] - (char *)&m[0][0]);
        }
    }

    printf("\nFlat traversal: ");
    int *p = &m[0][0];
    for (int k = 0; k < 6; k++) {
        printf("%d ", p[k]);
    }
    printf("\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
./ex2
```

<details><summary>Predicción</summary>

```
m[0][0] = 10  addr: 0x7fff...  offset: 0
m[0][1] = 20  addr: 0x7fff...  offset: 4
m[0][2] = 30  addr: 0x7fff...  offset: 8
m[1][0] = 40  addr: 0x7fff...  offset: 12
m[1][1] = 50  addr: 0x7fff...  offset: 16
m[1][2] = 60  addr: 0x7fff...  offset: 20

Flat traversal: 10 20 30 40 50 60
```

Cada elemento está a 4 bytes del anterior. No hay gap entre `m[0][2]` (offset 8) y `m[1][0]` (offset 12) — las filas son perfectamente contiguas. El recorrido plano con `int *p` lee los 6 elementos secuencialmente porque el layout es un bloque contiguo de 24 bytes.

</details>

---

### Ejercicio 3 — Inicialización parcial y primera dimensión deducida

Experimenta con inicialización parcial y deducción de la primera dimensión:

```c
#include <stdio.h>

void print_mat(const char *label, int rows, int cols, int mat[rows][cols]) {
    printf("%s:\n", label);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%4d", mat[i][j]);
        printf("\n");
    }
    printf("\n");
}

int main(void) {
    int partial[3][4] = {
        {1, 2},
        {5},
    };

    int deduced[][3] = {
        {10, 20, 30},
        {40, 50, 60},
        {70, 80, 90},
        {100, 110, 120},
    };
    int rows = (int)(sizeof(deduced) / sizeof(deduced[0]));

    print_mat("partial[3][4]", 3, 4, partial);
    printf("deduced rows = %d\n", rows);
    print_mat("deduced[][3]", rows, 3, deduced);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
./ex3
```

<details><summary>Predicción</summary>

```
partial[3][4]:
   1   2   0   0
   5   0   0   0
   0   0   0   0

deduced rows = 4
deduced[][3]:
  10  20  30
  40  50  60
  70  80  90
 100 110 120
```

En `partial`: la fila 0 tiene {1, 2, 0, 0} (parcial), la fila 1 tiene {5, 0, 0, 0} (parcial), la fila 2 tiene {0, 0, 0, 0} (entera fila sin inicializador → todo ceros). El estándar C garantiza que los elementos no especificados se ponen a 0.

En `deduced`: el compilador cuenta 4 inicializadores de fila → 4 filas. `sizeof(deduced) / sizeof(deduced[0])` = (4×3×4) / (3×4) = 48/12 = 4.

</details>

---

### Ejercicio 4 — Cuatro formas de pasar a función

Pasa la misma matriz usando las cuatro firmas de función y verifica que todas producen la misma salida:

```c
#include <stdio.h>

#define COLS 3

void with_bracket(int mat[][COLS], int rows) {
    printf("bracket:  ");
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < COLS; j++)
            printf("%d ", mat[i][j]);
    printf("\n");
}

void with_ptr(int (*mat)[COLS], int rows) {
    printf("ptr:      ");
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < COLS; j++)
            printf("%d ", mat[i][j]);
    printf("\n");
}

void with_vla(int rows, int cols, int mat[rows][cols]) {
    printf("vla:      ");
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            printf("%d ", mat[i][j]);
    printf("\n");
}

void with_flat(int *mat, int rows, int cols) {
    printf("flat:     ");
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            printf("%d ", mat[i * cols + j]);
    printf("\n");
}

int main(void) {
    int m[2][COLS] = { {1,2,3}, {4,5,6} };

    with_bracket(m, 2);
    with_ptr(m, 2);
    with_vla(2, COLS, m);
    with_flat(&m[0][0], 2, COLS);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
./ex4
```

<details><summary>Predicción</summary>

```
bracket:  1 2 3 4 5 6
ptr:      1 2 3 4 5 6
vla:      1 2 3 4 5 6
flat:     1 2 3 4 5 6
```

Las cuatro formas producen el mismo resultado. `int mat[][3]` e `int (*mat)[3]` son equivalentes como parámetros — ambos son un puntero a arrays de 3 ints. El VLA permite dimensiones variables. El puntero plano requiere calcular el índice manualmente con `i * cols + j`.

La diferencia práctica es la flexibilidad: las formas 1 y 2 fijan COLS en la firma; las formas 3 y 4 aceptan cualquier dimensión.

</details>

---

### Ejercicio 5 — Transpuesta de una matriz

Dada una matriz 3×4, genera su transpuesta 4×3:

```c
#include <stdio.h>

#define ROWS 3
#define COLS 4

void print_mat(const char *label, int r, int c, int mat[r][c]) {
    printf("%s (%dx%d):\n", label, r, c);
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++)
            printf("%4d", mat[i][j]);
        printf("\n");
    }
}

int main(void) {
    int m[ROWS][COLS] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
    };

    int t[COLS][ROWS];
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            t[j][i] = m[i][j];

    print_mat("Original", ROWS, COLS, m);
    printf("\n");
    print_mat("Transpose", COLS, ROWS, t);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex5.c -o ex5
./ex5
```

<details><summary>Predicción</summary>

```
Original (3x4):
   1   2   3   4
   5   6   7   8
   9  10  11  12

Transpose (4x3):
   1   5   9
   2   6  10
   3   7  11
   4   8  12
```

`t[j][i] = m[i][j]` intercambia filas y columnas. La fila 0 de `m` (1, 2, 3, 4) se convierte en la columna 0 de `t`. La dimensión cambia de 3×4 a 4×3. Nota: la lectura de `m` es por filas (cache-friendly) pero la escritura de `t` salta entre filas (menos cache-friendly). Para matrices grandes, existen algoritmos de transposición por bloques que optimizan el uso de cache.

</details>

---

### Ejercicio 6 — Array de strings: `char *[]` vs `char [][N]`

Compara las dos formas de representar colecciones de strings:

```c
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *ptrs[] = {"cat", "elephant", "dog"};
    char bufs[][12] = {"cat", "elephant", "dog"};

    int np = (int)(sizeof(ptrs) / sizeof(ptrs[0]));
    int nb = (int)(sizeof(bufs) / sizeof(bufs[0]));

    printf("=== const char *ptrs[] ===\n");
    printf("sizeof = %zu (%d pointers)\n\n", sizeof(ptrs), np);
    for (int i = 0; i < np; i++)
        printf("  [%d] \"%s\" strlen=%zu ptr=%p\n",
               i, ptrs[i], strlen(ptrs[i]), (void *)ptrs[i]);

    printf("\n=== char bufs[][12] ===\n");
    printf("sizeof = %zu (%d rows x 12)\n\n", sizeof(bufs), nb);
    for (int i = 0; i < nb; i++)
        printf("  [%d] \"%s\" strlen=%zu addr=%p\n",
               i, bufs[i], strlen(bufs[i]), (void *)bufs[i]);

    /* Modify bufs (OK) */
    strcpy(bufs[0], "lion");
    printf("\nAfter strcpy: bufs[0] = \"%s\"\n", bufs[0]);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
./ex6
```

<details><summary>Predicción</summary>

```
=== const char *ptrs[] ===
sizeof = 24 (3 pointers)

  [0] "cat" strlen=3 ptr=0x...
  [1] "elephant" strlen=8 ptr=0x...
  [2] "dog" strlen=3 ptr=0x...

=== char bufs[][12] ===
sizeof = 36 (3 rows x 12)

  [0] "cat" strlen=3 addr=0x7fff...
  [1] "elephant" strlen=8 addr=0x7fff...
  [2] "dog" strlen=3 addr=0x7fff...

After strcpy: bufs[0] = "lion"
```

`ptrs`: 3 punteros × 8 bytes = 24. Los punteros apuntan a `.rodata` (read-only). Los strings no son contiguos necesariamente. `bufs`: 3 filas × 12 bytes = 36. Cada fila es un buffer de 12 bytes en el stack, contiguo. "cat" solo usa 4 bytes (3 + `\0`), los 8 restantes son padding con ceros. `bufs` se puede modificar (`strcpy`); `ptrs` no.

Las direcciones de `bufs` estarán separadas por exactamente 12 bytes.

</details>

---

### Ejercicio 7 — Suma de filas y columnas

Calcula la suma de cada fila y cada columna de una matriz:

```c
#include <stdio.h>

#define ROWS 3
#define COLS 4

int main(void) {
    int m[ROWS][COLS] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
    };

    printf("Matrix:\n");
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++)
            printf("%4d", m[i][j]);
        printf("\n");
    }

    printf("\nRow sums:\n");
    for (int i = 0; i < ROWS; i++) {
        int sum = 0;
        for (int j = 0; j < COLS; j++)
            sum += m[i][j];
        printf("  row %d: %d\n", i, sum);
    }

    printf("\nColumn sums:\n");
    for (int j = 0; j < COLS; j++) {
        int sum = 0;
        for (int i = 0; i < ROWS; i++)
            sum += m[i][j];
        printf("  col %d: %d\n", j, sum);
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
Matrix:
   1   2   3   4
   5   6   7   8
   9  10  11  12

Row sums:
  row 0: 10
  row 1: 26
  row 2: 42

Column sums:
  col 0: 15
  col 1: 18
  col 2: 21
  col 3: 24
```

Fila 0: 1+2+3+4 = 10. Fila 1: 5+6+7+8 = 26. Fila 2: 9+10+11+12 = 42.
Col 0: 1+5+9 = 15. Col 1: 2+6+10 = 18. Col 2: 3+7+11 = 21. Col 3: 4+8+12 = 24.

Nota sobre cache: la suma por filas (loop externo en `i`, interno en `j`) es cache-friendly. La suma por columnas (loop externo en `j`, interno en `i`) no lo es — para esta matriz pequeña no importa, pero en matrices grandes sería más eficiente transponer primero o usar un solo recorrido acumulando en un array de sumas.

</details>

---

### Ejercicio 8 — Multiplicación de matrices

Implementa C = A × B para matrices de tamaño fijo:

```c
#include <stdio.h>

#define M 2
#define N 3
#define P 2

void print_mat(const char *label, int r, int c, int mat[r][c]) {
    printf("%s (%dx%d):\n", label, r, c);
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++)
            printf("%4d", mat[i][j]);
        printf("\n");
    }
}

int main(void) {
    int a[M][N] = { {1, 2, 3}, {4, 5, 6} };
    int b[N][P] = { {7, 8}, {9, 10}, {11, 12} };
    int c[M][P] = {0};

    for (int i = 0; i < M; i++)
        for (int j = 0; j < P; j++)
            for (int k = 0; k < N; k++)
                c[i][j] += a[i][k] * b[k][j];

    print_mat("A", M, N, a);
    printf("\n");
    print_mat("B", N, P, b);
    printf("\n");
    print_mat("C = A x B", M, P, c);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
A (2x3):
   1   2   3
   4   5   6

B (3x2):
   7   8
   9  10
  11  12

C = A x B (2x2):
  58  64
 139 154
```

C[0][0] = 1×7 + 2×9 + 3×11 = 7 + 18 + 33 = 58.
C[0][1] = 1×8 + 2×10 + 3×12 = 8 + 20 + 36 = 64.
C[1][0] = 4×7 + 5×9 + 6×11 = 28 + 45 + 66 = 139.
C[1][1] = 4×8 + 5×10 + 6×12 = 32 + 50 + 72 = 154.

Es importante inicializar `c` con `{0}` antes de acumular las sumas. El loop triple tiene complejidad O(M×N×P). El orden de los loops (i, j, k) vs (i, k, j) afecta el rendimiento en matrices grandes por cache locality.

</details>

---

### Ejercicio 9 — Matriz dinámica contigua (estrategia 3)

Implementa la creación y destrucción de una matriz dinámica con bloque contiguo y array de punteros:

```c
#include <stdio.h>
#include <stdlib.h>

int **create_matrix(int rows, int cols) {
    int **mat = malloc(rows * sizeof(int *));
    int *data = calloc(rows * cols, sizeof(int));
    if (!mat || !data) {
        free(mat);
        free(data);
        return NULL;
    }
    for (int i = 0; i < rows; i++)
        mat[i] = data + i * cols;
    return mat;
}

void free_matrix(int **mat) {
    if (mat) {
        free(mat[0]);
        free(mat);
    }
}

int main(void) {
    int rows = 3, cols = 4;
    int **m = create_matrix(rows, cols);
    if (!m) { fprintf(stderr, "alloc failed\n"); return 1; }

    /* Fill with values */
    int val = 1;
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            m[i][j] = val++;

    /* Print */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%4d", m[i][j]);
        printf("\n");
    }

    /* Verify contiguity */
    int *first = &m[0][0];
    int *last  = &m[rows-1][cols-1];
    printf("\nContiguous: %s (span = %td bytes, expected = %d)\n",
           ((char *)last - (char *)first + sizeof(int)) == (size_t)(rows * cols * (int)sizeof(int))
               ? "yes" : "no",
           (char *)last - (char *)first + (long)sizeof(int),
           rows * cols * (int)sizeof(int));

    free_matrix(m);
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex9.c -o ex9
./ex9
```

<details><summary>Predicción</summary>

```
   1   2   3   4
   5   6   7   8
   9  10  11  12

Contiguous: yes (span = 48 bytes, expected = 48)
```

La estrategia 3 combina las ventajas: `m[i][j]` usa sintaxis natural (como un array 2D real), pero la memoria es un bloque contiguo asignado con `calloc`. Solo se necesitan 2 `malloc`s (uno para los punteros de fila, otro para los datos) y 2 `free`s. La verificación de contigüidad confirma que los datos ocupan exactamente 48 bytes sin huecos (3 × 4 × 4).

`calloc` inicializa a cero automáticamente (a diferencia de `malloc`).

</details>

---

### Ejercicio 10 — Búsqueda del máximo en una matriz

Encuentra el valor máximo y su posición (fila, columna):

```c
#include <stdio.h>
#include <limits.h>

#define ROWS 3
#define COLS 4

void find_max(int rows, int cols, int mat[rows][cols],
              int *max_val, int *max_row, int *max_col) {
    *max_val = INT_MIN;
    *max_row = 0;
    *max_col = 0;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (mat[i][j] > *max_val) {
                *max_val = mat[i][j];
                *max_row = i;
                *max_col = j;
            }
        }
    }
}

int main(void) {
    int m[ROWS][COLS] = {
        { 3, 14,  1,  5},
        { 9,  2,  6, 53},
        {58,  9,  7, 32},
    };

    printf("Matrix:\n");
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++)
            printf("%4d", m[i][j]);
        printf("\n");
    }

    int val, row, col;
    find_max(ROWS, COLS, m, &val, &row, &col);
    printf("\nMax value: %d at m[%d][%d]\n", val, row, col);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
Matrix:
   3  14   1   5
   9   2   6  53
  58   9   7  32

Max value: 58 at m[2][0]
```

El valor máximo es 58, en la fila 2, columna 0. La función usa **output parameters** (`int *max_val`, `int *max_row`, `int *max_col`) para retornar tres valores — patrón visto en T02_Paso_por_valor. Se inicializa `max_val` con `INT_MIN` (de `<limits.h>`) para garantizar que cualquier valor en la matriz sea mayor. El recorrido es row-major (loop externo en filas) — cache-friendly.

El uso de VLA en el parámetro (`int mat[rows][cols]`) permite que la función acepte matrices de cualquier dimensión.

</details>
