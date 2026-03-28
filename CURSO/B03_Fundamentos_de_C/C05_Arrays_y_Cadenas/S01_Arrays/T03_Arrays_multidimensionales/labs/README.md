# Lab — Arrays multidimensionales

## Objetivo

Trabajar con arrays 2D en C: declarar e inicializar matrices, verificar que el
layout en memoria es contiguo y sigue row-major order, pasar arrays 2D a
funciones con distintas firmas, y entender la diferencia entre `const char *arr[]`
y `char arr[][N]` para representar colecciones de strings. Al finalizar, sabras
elegir la representacion correcta segun el caso y entenderas por que el
compilador necesita conocer el numero de columnas.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `matrix_basics.c` | Declaracion, inicializacion y acceso a arrays 2D |
| `memory_layout.c` | Direcciones en memoria, recorrido plano con puntero |
| `pass_to_func.c` | Cuatro formas de pasar array 2D a funciones |
| `string_arrays.c` | `const char *arr[]` vs `char arr[][N]` |

---

## Parte 1 — Arrays 2D: declaracion, inicializacion y acceso

**Objetivo**: Declarar un array 2D, inicializarlo de distintas formas, y acceder
a elementos individuales con `matrix[i][j]`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat matrix_basics.c
```

Observa:

- La declaracion `int matrix[ROWS][COLS]` con inicializacion completa
- La funcion `print_matrix` que recibe `int mat[][COLS]`
- La declaracion `int partial[2][3]` con inicializacion parcial

Antes de compilar, predice:

- Que valor tendra `matrix[1][2]`? Cuenta: fila 1, columna 2.
- En `partial`, que valores tendran las posiciones no inicializadas?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic matrix_basics.c -o matrix_basics
./matrix_basics
```

Salida esperada:

```
=== Matrix 3x4 ===
   1   2   3   4
   5   6   7   8
   9  10  11  12

Access examples:
matrix[0][0] = 1
matrix[1][2] = 7
matrix[2][3] = 12

=== Partial initialization ===
   1   0   0
   4   5   0
```

Verifica tus predicciones:

- `matrix[1][2]` es 7 (segunda fila `{5, 6, 7, 8}`, tercera columna).
- En `partial`, las posiciones no inicializadas son 0. C garantiza que la
  inicializacion parcial rellena el resto con ceros.

### Paso 1.3 — Experimentar con la inicializacion

Abre `matrix_basics.c` y busca la inicializacion parcial. Modifica
temporalmente para probar otra forma:

```bash
cat > init_test.c << 'EOF'
#include <stdio.h>

int main(void) {
    /* Without inner braces — C fills row by row */
    int m1[2][3] = {1, 2, 3, 4, 5, 6};

    /* Deduced first dimension */
    int m2[][3] = {
        {10, 20, 30},
        {40, 50, 60},
        {70, 80, 90},
    };

    printf("m1 (flat init):\n");
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++)
            printf("%4d", m1[i][j]);
        printf("\n");
    }

    int rows = (int)(sizeof(m2) / sizeof(m2[0]));
    printf("\nm2 (deduced rows = %d):\n", rows);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < 3; j++)
            printf("%4d", m2[i][j]);
        printf("\n");
    }

    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic init_test.c -o init_test
./init_test
```

Salida esperada:

```
m1 (flat init):
   1   2   3
   4   5   6

m2 (deduced rows = 3):
  10  20  30
  40  50  60
  70  80  90
```

Observa:

- Sin llaves internas, C llena fila por fila (row-major).
- Con `int m2[][3]`, el compilador deduce que hay 3 filas. Solo la primera
  dimension se puede omitir.

### Limpieza de la parte 1

```bash
rm -f matrix_basics init_test.c init_test
```

---

## Parte 2 — Layout en memoria: row-major order

**Objetivo**: Verificar que un array 2D ocupa memoria contigua en row-major
order y entender como `sizeof` refleja la estructura.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat memory_layout.c
```

Observa:

- Se imprimen las direcciones de cada elemento con `%p`
- Se calcula el total de bytes entre el primer y ultimo elemento
- Se recorre la matriz como un array plano usando un puntero `int *`
- Se usa `sizeof` para mostrar el tamano total y el de una fila

Antes de ejecutar, predice:

- Cuantos bytes separan `matrix[0][0]` de `matrix[0][1]`? (pista: `sizeof(int)`)
- Cuantos bytes separan `matrix[0][3]` de `matrix[1][0]`?
- Que valor reportara `sizeof(matrix)`? (`ROWS * COLS * sizeof(int)`)

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic memory_layout.c -o memory_layout
./memory_layout
```

Salida esperada (las direcciones varian, pero las diferencias son fijas):

```
=== Memory addresses (row-major order) ===

matrix[0][0] =  1  addr: 0x7fff...b0
matrix[0][1] =  2  addr: 0x7fff...b4
matrix[0][2] =  3  addr: 0x7fff...b8
matrix[0][3] =  4  addr: 0x7fff...bc
matrix[1][0] =  5  addr: 0x7fff...c0
matrix[1][1] =  6  addr: 0x7fff...c4
...

=== Verifying contiguous layout ===
First element addr: 0x7fff...b0
Last  element addr: 0x7fff...dc
Total bytes: 48  (expected: 48)

=== Flat traversal via pointer ===
ptr[ 0] =  1
ptr[ 1] =  2
...
ptr[11] = 12

=== sizeof info ===
sizeof(matrix)    = 48 bytes (12 ints)
sizeof(matrix[0]) = 16 bytes (4 ints = 1 row)
```

Verifica tus predicciones:

- Cada elemento esta a 4 bytes del anterior (sizeof(int) = 4).
- `matrix[0][3]` y `matrix[1][0]` estan separados por exactamente 4 bytes:
  no hay padding ni huecos entre filas. La fila 1 empieza inmediatamente
  despues de la fila 0.
- `sizeof(matrix)` = 3 * 4 * 4 = 48 bytes.
- `sizeof(matrix[0])` = 4 * 4 = 16 bytes (una fila completa).

Esto confirma **row-major order**: las filas se almacenan una tras otra,
formando un bloque contiguo. Por eso un `int *` puede recorrer todos los
elementos secuencialmente.

### Paso 2.3 — Por que importa el orden de recorrido

El layout row-major tiene implicaciones para el rendimiento. Recorrer por
filas (el indice de columna en el loop interno) accede a posiciones contiguas
en memoria, aprovechando la cache. Recorrer por columnas salta entre filas,
causando cache misses.

Para arrays pequenos la diferencia es imperceptible. En matrices grandes
(1000x1000+), recorrer en column-major puede ser 5-10x mas lento.

### Limpieza de la parte 2

```bash
rm -f memory_layout
```

---

## Parte 3 — Pasar array 2D a funciones

**Objetivo**: Entender las cuatro formas de pasar un array 2D a una funcion y
por que el compilador necesita conocer el numero de columnas.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat pass_to_func.c
```

Observa las cuatro funciones:

1. `print_bracket(int mat[][COLS], int rows)` — notacion de array
2. `print_ptr_array(int (*mat)[COLS], int rows)` — puntero a array
3. `print_vla(int rows, int cols, int mat[rows][cols])` — VLA (C99+)
4. `print_flat(int *mat, int rows, int cols)` — puntero plano

Antes de compilar, predice:

- Las cuatro produciran la misma salida?
- Cual es la diferencia entre `int *mat[4]` y `int (*mat)[4]`?
  (Pista: uno es un array de 4 punteros, el otro es un puntero a un array de
  4 ints. Los parentesis importan.)

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic pass_to_func.c -o pass_to_func
./pass_to_func
```

Salida esperada:

```
--- print_bracket (int mat[][4]) ---
   1   2   3   4
   5   6   7   8
   9  10  11  12

--- print_ptr_array (int (*mat)[4]) ---
   1   2   3   4
   5   6   7   8
   9  10  11  12

--- print_vla (VLA 3x4) ---
   1   2   3   4
   5   6   7   8
   9  10  11  12

--- print_flat (int *mat) ---
   1   2   3   4
   5   6   7   8
   9  10  11  12
```

Las cuatro producen el mismo resultado, pero difieren en flexibilidad:

| Firma | Columnas fijas | Filas flexibles | Dimensiones flexibles |
|-------|:-:|:-:|:-:|
| `int mat[][4]` | Si (4) | Si | No |
| `int (*mat)[4]` | Si (4) | Si | No |
| VLA `int mat[rows][cols]` | No | Si | Si |
| `int *mat` (flat) | No | Si | Si |

### Paso 3.3 — Por que el compilador necesita el numero de columnas

El compilador calcula la direccion de `mat[i][j]` como:

```
base + (i * COLS + j) * sizeof(int)
```

Sin conocer `COLS`, no puede calcular el offset. Por eso las opciones 1 y 2
requieren que `COLS` sea una constante en la firma.

La opcion VLA recibe `cols` como parametro (debe aparecer *antes* de `mat` en
la lista de parametros). La opcion flat calcula el indice manualmente con
`i * cols + j`.

### Paso 3.4 — Diferencia entre `int *mat[4]` e `int (*mat)[4]`

```bash
cat > ptr_vs_array.c << 'EOF'
#include <stdio.h>

int main(void) {
    int data[3][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
    };

    /* Pointer to array of 4 ints */
    int (*ptr_to_row)[4] = data;
    printf("int (*ptr)[4]  -> ptr_to_row[1][2] = %d\n", ptr_to_row[1][2]);
    printf("sizeof(*ptr_to_row) = %zu (one row = 4 ints)\n",
           sizeof(*ptr_to_row));

    /* Array of 4 int pointers (different thing!) */
    int *arr_of_ptrs[4];
    arr_of_ptrs[0] = data[0];
    arr_of_ptrs[1] = data[1];
    printf("\nint *arr[4]    -> arr_of_ptrs[1][2] = %d\n",
           arr_of_ptrs[1][2]);
    printf("sizeof(arr_of_ptrs) = %zu (4 pointers)\n",
           sizeof(arr_of_ptrs));

    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic ptr_vs_array.c -o ptr_vs_array
./ptr_vs_array
```

Salida esperada:

```
int (*ptr)[4]  -> ptr_to_row[1][2] = 7
sizeof(*ptr_to_row) = 16 (one row = 4 ints)

int *arr[4]    -> arr_of_ptrs[1][2] = 7
sizeof(arr_of_ptrs) = 32 (4 pointers)
```

Ambos acceden a `[1][2]` con la misma sintaxis, pero son tipos distintos:

- `int (*ptr)[4]` — un puntero a un array de 4 ints. `sizeof(*ptr)` = 16.
- `int *arr[4]` — un array de 4 punteros a int. `sizeof(arr)` = 32 (4 * 8).

### Limpieza de la parte 3

```bash
rm -f pass_to_func ptr_vs_array.c ptr_vs_array
```

---

## Parte 4 — Arrays de strings: `const char *arr[]` vs `char arr[][]`

**Objetivo**: Entender las dos formas de representar colecciones de strings en C
y las diferencias en layout, sizeof, y mutabilidad.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat string_arrays.c
```

Observa las dos declaraciones:

- `const char *names[]` — array de punteros a string literals
- `char cities[][12]` — array 2D de chars con buffer fijo de 12 por fila

Antes de ejecutar, predice:

- `sizeof(names)` contara los bytes de los strings o los bytes de los punteros?
- `sizeof(cities)` sera 4 * 12 = 48, o sera la suma de los strlen?
- Se puede modificar `cities[0]`? Y `names[0]`?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic string_arrays.c -o string_arrays
./string_arrays
```

Salida esperada:

```
=== const char *names[] (array of pointers) ===
sizeof(names)    = 32 bytes (4 pointers)
sizeof(names[0]) = 8 bytes (one pointer)

names[0] = Alice       ptr: 0x...  strlen: 5
names[1] = Bob         ptr: 0x...  strlen: 3
names[2] = Charlie     ptr: 0x...  strlen: 7
names[3] = Dave        ptr: 0x...  strlen: 4

=== char cities[][12] (2D char array) ===
sizeof(cities)    = 48 bytes (4 rows x 12 cols)
sizeof(cities[0]) = 12 bytes (one row = buffer)

cities[0] = Madrid        addr: 0x...  strlen: 6  buffer: 12
cities[1] = Barcelona     addr: 0x...  strlen: 9  buffer: 12
cities[2] = Valencia      addr: 0x...  strlen: 8  buffer: 12
cities[3] = Sevilla       addr: 0x...  strlen: 7  buffer: 12

=== Modifying cities[0] ===
Before: cities[0] = Madrid
After:  cities[0] = Bilbao

names[0] points to a string literal (read-only).
Modifying it would be undefined behavior.
```

Verifica tus predicciones:

- `sizeof(names)` = 32: son 4 punteros de 8 bytes. No mide los strings.
- `sizeof(cities)` = 48: son 4 filas de 12 bytes. Cada fila es un buffer fijo,
  independientemente del strlen.
- `cities[0]` se puede modificar (es un buffer local en el stack).
- `names[0]` apunta a un string literal en `.rodata` (read-only).
  Modificarlo es undefined behavior.

### Paso 4.3 — Analisis del layout

Observa las direcciones en la salida:

- En `names[]`, los punteros apuntan a la seccion `.rodata` del binario.
  Las direcciones de los strings no son contiguas entre si necesariamente
  (depende de como el compilador organice los literals). Cada string ocupa
  solo lo necesario (strlen + 1 para el `\0`).

- En `cities[][]`, las direcciones son contiguas y equidistantes (separadas
  por 12 bytes). Cada fila es un buffer fijo de 12 bytes, aunque el string
  sea mas corto. Hay bytes desperdiciados (padding con `\0`).

Comparacion:

| Aspecto | `const char *arr[]` | `char arr[][N]` |
|---------|---------------------|-----------------|
| sizeof(arr) | Punteros (8 bytes c/u) | Filas * N bytes |
| Strings diferentes tamanos | Sin desperdicio | Buffer fijo, puede desperdiciar |
| Modificable | No (apunta a literals) | Si (buffer propio) |
| Memoria | Fragmentada (punteros + literals) | Contigua |
| Uso tipico | Tablas de constantes, argv | Buffers editables de tamano fijo |

### Limpieza de la parte 4

```bash
rm -f string_arrays
```

---

## Limpieza final

```bash
rm -f matrix_basics memory_layout pass_to_func string_arrays
rm -f init_test init_test.c ptr_vs_array ptr_vs_array.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  matrix_basics.c  memory_layout.c  pass_to_func.c  string_arrays.c
```

---

## Conceptos reforzados

1. Un array 2D `int m[R][C]` ocupa `R * C * sizeof(int)` bytes contiguos en
   memoria. No hay huecos ni padding entre filas.

2. C usa **row-major order**: las filas se almacenan una tras otra. El elemento
   `m[i][j]` esta en `base + (i * C + j) * sizeof(int)`.

3. La inicializacion parcial rellena con ceros. Sin llaves internas, C llena
   fila por fila. Solo la primera dimension se puede omitir (`int m[][C]`).

4. Al pasar un array 2D a una funcion, el compilador necesita conocer el numero
   de columnas para calcular offsets. Las firmas `int mat[][N]` e `int (*mat)[N]`
   son equivalentes y requieren `N` fijo. Los VLA (`int mat[rows][cols]`) y el
   puntero plano (`int *mat`) permiten dimensiones flexibles.

5. `int (*mat)[N]` (puntero a array de N ints) e `int *mat[N]` (array de N
   punteros a int) son tipos fundamentalmente distintos. Los parentesis
   cambian la precedencia del `*`.

6. `const char *arr[]` es un array de punteros: cada elemento apunta a un
   string literal (read-only, tamano variable, sin desperdicio). `char arr[][N]`
   es un array 2D: cada fila es un buffer fijo de N bytes (modificable,
   contiguo, puede desperdiciar espacio).

7. `sizeof` sobre un array 2D retorna el tamano total del bloque de datos.
   `sizeof` sobre un array de punteros retorna solo el tamano de los punteros,
   no de los datos apuntados.
