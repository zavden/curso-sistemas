# T03 — Arrays multidimensionales

## Arrays 2D

Un array 2D es un array de arrays. En memoria es **contiguo**
y está organizado en **row-major order** (fila por fila):

```c
#include <stdio.h>

int main(void) {
    int matrix[3][4] = {
        {1,  2,  3,  4},      // fila 0
        {5,  6,  7,  8},      // fila 1
        {9, 10, 11, 12},      // fila 2
    };

    // Acceso: matrix[fila][columna]
    printf("%d\n", matrix[0][0]);    // 1
    printf("%d\n", matrix[1][2]);    // 7
    printf("%d\n", matrix[2][3]);    // 12

    return 0;
}
```

### Layout en memoria (row-major)

```c
// matrix[3][4] en memoria es contiguo:
//
// Dirección:  0x00  0x04  0x08  0x0C  0x10  0x14  0x18  0x1C  0x20  0x24  0x28  0x2C
// Valor:      [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8]   [9]   [10]  [11]  [12]
//             |------- fila 0 ------|------- fila 1 ------|------- fila 2 ------|
//
// matrix[i][j] está en: base + (i * 4 + j) * sizeof(int)
//
// matrix[1][2] → base + (1*4 + 2) * 4 = base + 24 = 0x18 → valor 7

// Row-major: las filas son contiguas.
// C, C++, Python (NumPy default), Rust usan row-major.
// Fortran, MATLAB, Julia usan column-major.
```

### Inicialización

```c
// Inicialización completa:
int m[2][3] = {
    {1, 2, 3},
    {4, 5, 6},
};

// Sin llaves internas (funciona, pero es confuso):
int m2[2][3] = {1, 2, 3, 4, 5, 6};
// Equivalente — C llena fila por fila

// Inicialización parcial — el resto es 0:
int m3[2][3] = {
    {1},           // {1, 0, 0}
    {4, 5},        // {4, 5, 0}
};

// Todo a 0:
int m4[3][3] = {0};

// Deducir primera dimensión:
int m5[][3] = {      // el compilador determina que son 2 filas
    {1, 2, 3},
    {4, 5, 6},
};
// Solo la PRIMERA dimensión se puede omitir.
// int m5[][] = {...};    // ERROR
```

### Recorrer un array 2D

```c
#define ROWS 3
#define COLS 4

int matrix[ROWS][COLS] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12},
};

// Recorrido natural (row-major — eficiente en cache):
for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
        printf("%4d", matrix[i][j]);
    }
    printf("\n");
}

// Recorrido column-major (INEFICIENTE — salta en memoria):
for (int j = 0; j < COLS; j++) {
    for (int i = 0; i < ROWS; i++) {
        printf("%4d", matrix[i][j]);
        // Acceso no secuencial: cache misses
    }
}
```

```c
// Cache performance importa en arrays grandes:
// Array 1000×1000 de ints = 4 MB
//
// Row-major traversal: acceso secuencial
//   matrix[0][0], matrix[0][1], matrix[0][2], ...
//   → cache line se usa completamente → rápido
//
// Column-major traversal: saltos de 4000 bytes
//   matrix[0][0], matrix[1][0], matrix[2][0], ...
//   → cada acceso puede ser un cache miss → lento
//
// Diferencia real: puede ser 5-10x más lento con column-major
```

## Pasar arrays 2D a funciones

```c
// Al pasar un array 2D, se DEBEN especificar todas las
// dimensiones excepto la primera:

void print_matrix(int mat[][4], int rows) {
    // mat[][4]: la segunda dimensión (4) es obligatoria
    // El compilador la necesita para calcular mat[i][j]:
    // offset = i * 4 + j
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

// Equivalente con puntero explícito:
void print_matrix2(int (*mat)[4], int rows) {
    // mat es un puntero a array de 4 ints
    // int (*mat)[4] ≡ int mat[][4] como parámetro
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

int main(void) {
    int m[3][4] = {{1,2,3,4}, {5,6,7,8}, {9,10,11,12}};
    print_matrix(m, 3);
    return 0;
}
```

```c
// La limitación: el número de columnas está fijo en la firma.
// Para matrices de tamaño variable, hay dos opciones:

// Opción 1: VLA en parámetro (C99):
void print_any(int rows, int cols, int mat[rows][cols]) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}
// rows y cols deben aparecer ANTES de mat

// Opción 2: puntero plano (siempre funciona):
void print_flat(int *mat, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d", mat[i * cols + j]);
        }
        printf("\n");
    }
}
// Llamar: print_flat(&m[0][0], 3, 4);
// o:      print_flat((int *)m, 3, 4);
```

## Array de punteros vs array 2D real

Hay dos formas de representar una "matriz" — son
fundamentalmente diferentes:

### Array 2D real (contiguo)

```c
// Todo en un bloque contiguo:
int matrix[3][4];

// Memoria:
// [fila0: 4 ints][fila1: 4 ints][fila2: 4 ints]
// ←──────────── 48 bytes contiguos ──────────→
//
// sizeof(matrix) = 48
// Todas las filas tienen el mismo número de columnas.
// Cache-friendly.
```

### Array de punteros (ragged array)

```c
// Cada "fila" es un puntero a memoria independiente:
int *rows[3];
rows[0] = malloc(4 * sizeof(int));
rows[1] = malloc(4 * sizeof(int));
rows[2] = malloc(4 * sizeof(int));

// Memoria:
// rows: [ptr0][ptr1][ptr2]
//          ↓     ↓     ↓
//       [4 ints][4 ints][4 ints]   ← en ubicaciones diferentes
//
// sizeof(rows) = 24 (3 punteros de 8 bytes)
// Las filas pueden tener tamaños diferentes (ragged).
// No necesariamente cache-friendly.

// Acceso: rows[i][j] — misma sintaxis, pero mecanismo diferente:
// Array 2D:       *(base + i * cols + j)         — 1 indirección
// Array de ptrs:  *(*(rows + i) + j)             — 2 indirecciones
```

```c
// Comparación:
//
// | Aspecto           | int m[3][4]         | int *m[3]            |
// |--------------------|---------------------|----------------------|
// | Memoria            | Contigua            | Fragmentada          |
// | sizeof             | 48 (datos)          | 24 (punteros)        |
// | Filas diferentes   | No                  | Sí (ragged)          |
// | Cache              | Excelente           | Puede ser malo       |
// | malloc necesario   | No                  | Sí (para cada fila)  |
// | Pasar a función    | Necesita cols fijo  | Solo int **          |
// | Indirecciones      | 1                   | 2                    |
```

### Cuándo usar cada uno

```c
// Array 2D real — cuando:
// - El tamaño es conocido en compilación
// - Todas las filas tienen el mismo tamaño
// - Rendimiento importa (acceso secuencial, cache)
int board[8][8];           // tablero de ajedrez
int pixels[1080][1920];    // imagen

// Array de punteros — cuando:
// - Las filas tienen tamaños diferentes
// - El tamaño se conoce en runtime
// - Se necesita pasar como int ** a funciones
char *argv[];              // argumentos de línea de comando
char *lines[MAX_LINES];   // líneas de texto de longitud variable
```

## Malloc para matrices

```c
// Opción 1: Array de punteros (int **)
int **create_matrix(int rows, int cols) {
    int **mat = malloc(rows * sizeof(int *));
    if (!mat) return NULL;

    for (int i = 0; i < rows; i++) {
        mat[i] = malloc(cols * sizeof(int));
        if (!mat[i]) {
            // Liberar lo ya alocado si falla:
            for (int j = 0; j < i; j++) free(mat[j]);
            free(mat);
            return NULL;
        }
    }
    return mat;
}

void free_matrix(int **mat, int rows) {
    for (int i = 0; i < rows; i++) {
        free(mat[i]);
    }
    free(mat);
}

// Uso:
int **m = create_matrix(3, 4);
m[1][2] = 42;
free_matrix(m, 3);
```

```c
// Opción 2: Bloque contiguo (más eficiente)
int *create_flat(int rows, int cols) {
    return calloc(rows * cols, sizeof(int));
}

// Acceso: mat[i * cols + j]
int *m = create_flat(3, 4);
m[1 * 4 + 2] = 42;    // m[1][2]
free(m);               // un solo free

// Ventajas: una sola alocación, contiguo, cache-friendly
// Desventaja: la sintaxis m[i*cols+j] es menos legible
```

```c
// Opción 3: Contiguo con array de punteros a filas
// (lo mejor de ambos mundos)
int **create_matrix_contiguous(int rows, int cols) {
    int **mat = malloc(rows * sizeof(int *));
    int *data = calloc(rows * cols, sizeof(int));
    if (!mat || !data) {
        free(mat);
        free(data);
        return NULL;
    }

    for (int i = 0; i < rows; i++) {
        mat[i] = data + i * cols;    // cada fila apunta dentro de data
    }
    return mat;
}

void free_matrix_contiguous(int **mat) {
    free(mat[0]);    // libera el bloque de datos (data)
    free(mat);       // libera el array de punteros
}

// Uso: mat[i][j] — sintaxis natural
// Memoria contigua — cache-friendly
// Solo 2 mallocs en vez de rows + 1
```

## Arrays de más dimensiones

```c
// 3D:
int cube[2][3][4];    // 2 "capas", cada una de 3×4

cube[0][1][2] = 42;

// sizeof(cube) = 2 * 3 * 4 * 4 = 96 bytes

// Layout: las dimensiones se aplanan de derecha a izquierda
// cube[i][j][k] → *(base + i*3*4 + j*4 + k)

// En la práctica, arrays de más de 2 dimensiones son raros en C.
// Para 3D+, considerar abstraer con funciones de acceso:
static inline int idx3d(int i, int j, int k, int d2, int d3) {
    return i * d2 * d3 + j * d3 + k;
}

int *data = calloc(2 * 3 * 4, sizeof(int));
data[idx3d(0, 1, 2, 3, 4)] = 42;
```

---

## Ejercicios

### Ejercicio 1 — Transpuesta

```c
// Dada una matriz int m[3][4], crear su transpuesta int t[4][3].
// t[j][i] = m[i][j] para todo i, j.
// Imprimir ambas matrices.
```

### Ejercicio 2 — Multiplicación de matrices

```c
// Implementar multiplicación de matrices A[M][N] × B[N][P] = C[M][P].
// Usar #define para M, N, P.
// Probar con matrices pequeñas y verificar a mano.
```

### Ejercicio 3 — Matriz dinámica contigua

```c
// Implementar create_matrix(rows, cols) que aloque un bloque
// contiguo con array de punteros a filas (opción 3 de arriba).
// Implementar free_matrix().
// Llenar con valores, imprimir, y verificar con valgrind
// que no hay leaks.
```
