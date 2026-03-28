# Lab — Punteros a arrays vs arrays de punteros

## Objetivo

Distinguir las dos declaraciones que mas confusion generan en C: `int *p[5]`
(array de punteros) vs `int (*p)[5]` (puntero a array). Al finalizar, sabras
leer cualquier declaracion compleja usando la regla de precedencia, entenderas
las diferencias de sizeof y aritmetica, y podras usar typedef para
simplificar.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sizeof_compare.c` | Comparacion de sizeof y aritmetica de punteros |
| `array_of_ptrs.c` | Array de punteros a int y a char (ragged array) |
| `ptr_to_array_2d.c` | Puntero a array para matrices 2D |
| `complex_decl.c` | Declaraciones complejas con typedef |

---

## Parte 1 — `int *p[5]` vs `int (*p)[5]`: sizeof y precedencia

**Objetivo**: Entender que la posicion de los parentesis cambia completamente
el significado de la declaracion, y verificarlo con sizeof.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat sizeof_compare.c
```

Observa las dos declaraciones:

- `int *arr_of_ptrs[5]` — sin parentesis
- `int (*ptr_to_arr)[5]` — con parentesis

Antes de compilar y ejecutar, predice:

- `sizeof(arr_of_ptrs)` sera 40 o 8? Por que?
- `sizeof(ptr_to_arr)` sera 40 o 8? Por que?
- `sizeof(*ptr_to_arr)` sera 20 o 8? Por que?
- Si sumas 1 a `ptr_to_arr`, cuantos bytes avanza?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizeof_compare.c -o sizeof_compare
./sizeof_compare
```

Salida esperada:

```
=== sizeof comparison ===

int *arr_of_ptrs[5]:
  sizeof(arr_of_ptrs)    = 40 bytes
  sizeof(arr_of_ptrs[0]) = 8 bytes (each element is int*)
  number of elements     = 5

int (*ptr_to_arr)[5]:
  sizeof(ptr_to_arr)     = 8 bytes (just a pointer)
  sizeof(*ptr_to_arr)    = 20 bytes (the whole array)

=== pointer arithmetic ===

ptr_to_arr       = <addr>
ptr_to_arr + 1   = <addr>
difference       = 20 bytes (sizeof(int[5]) = 20)
```

### Paso 1.3 — Analisis de resultados

Verifica tus predicciones:

- `sizeof(arr_of_ptrs) = 40`: es un array de 5 elementos, cada uno es un
  `int*` de 8 bytes. Total: 5 x 8 = 40.
- `sizeof(ptr_to_arr) = 8`: es solo un puntero. Un puntero siempre ocupa 8
  bytes en sistemas de 64 bits.
- `sizeof(*ptr_to_arr) = 20`: al desreferenciar el puntero, obtienes el array
  completo de 5 ints. Total: 5 x 4 = 20.
- `ptr_to_arr + 1` avanza 20 bytes: la aritmetica de punteros avanza por el
  tamano del tipo apuntado, que es `int[5]` = 20 bytes.

La regla de precedencia explica todo:

```
int *p[5]:   [] tiene mayor precedencia que *
             p es un array de 5... punteros a int
             sizeof = 5 x sizeof(int*) = 40

int (*p)[5]: los parentesis fuerzan * primero
             p es un puntero a... un array de 5 ints
             sizeof = sizeof(pointer) = 8
```

### Limpieza de la parte 1

```bash
rm -f sizeof_compare
```

---

## Parte 2 — Array de punteros: strings y ragged arrays

**Objetivo**: Usar arrays de punteros para manejar strings de diferente
longitud (ragged arrays), el caso de uso mas comun de `char *names[]`.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat array_of_ptrs.c
```

Observa las dos secciones:

1. `int *int_ptrs[]` — array de punteros a int, cada uno apuntando a una
   variable independiente
2. `const char *names[]` — array de punteros a char, cada uno apuntando a un
   string literal de longitud diferente

Antes de ejecutar, predice:

- `sizeof(int_ptrs)` sera 40 o 20? (Pista: hay 5 punteros de 8 bytes cada
  uno.)
- `sizeof(names)` sera 32 o 19? (Pista: hay 4 punteros. Los sizeof de los
  strings no cuentan.)
- Las direcciones en `int_ptrs` seran consecutivas o estaran dispersas?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic array_of_ptrs.c -o array_of_ptrs
./array_of_ptrs
```

Salida esperada:

```
=== array of pointers to int ===

int_ptrs[0] = <addr>  ->  *int_ptrs[0] = 10
int_ptrs[1] = <addr>  ->  *int_ptrs[1] = 20
int_ptrs[2] = <addr>  ->  *int_ptrs[2] = 30
int_ptrs[3] = <addr>  ->  *int_ptrs[3] = 40
int_ptrs[4] = <addr>  ->  *int_ptrs[4] = 50

sizeof(int_ptrs) = 40 bytes (5 elements x 8 bytes each)

=== array of pointers to char (ragged array) ===

sizeof(names) = 32 bytes (4 pointers x 8 bytes each)

names[0] = Alice       strlen = 5  (different lengths!)
names[1] = Bob         strlen = 3  (different lengths!)
names[2] = Charlie     strlen = 7  (different lengths!)
names[3] = Dave        strlen = 4  (different lengths!)
```

### Paso 2.3 — Analisis de resultados

Puntos clave:

- `sizeof(int_ptrs) = 40`: el array contiene 5 punteros (5 x 8 = 40). No
  importa el tamano de lo que apunten — sizeof cuenta los punteros, no los
  datos.
- `sizeof(names) = 32`: 4 punteros x 8 bytes = 32. No 5 + 3 + 7 + 4 = 19
  (la suma de los strlen). El array almacena punteros, no los strings.
- Las direcciones de `int_ptrs` probablemente estan cercanas pero no
  necesariamente consecutivas, porque `a`, `b`, `c`, `d`, `e` son variables
  locales separadas en el stack.
- Cada `names[i]` tiene un `strlen` diferente. Esto es un **ragged array** —
  un array donde cada "fila" tiene longitud diferente. Es imposible con un
  `char names[4][8]` sin desperdiciar espacio.

### Paso 2.4 — Conexion con argv

La misma estructura se usa en `main(int argc, char *argv[])`:

- `argv` es un array de punteros a char
- `argv[0]` apunta al nombre del programa
- `argv[1]` apunta al primer argumento
- Cada argumento tiene longitud diferente (ragged array)
- `char *argv[]` es equivalente a `char **argv`

### Limpieza de la parte 2

```bash
rm -f array_of_ptrs
```

---

## Parte 3 — Puntero a array: matrices 2D

**Objetivo**: Usar `int (*p)[COLS]` para pasar matrices 2D a funciones y
entender la aritmetica de punteros por filas completas.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat ptr_to_array_2d.c
```

Observa las funciones:

- `print_row(int (*row)[COLS])` — recibe un puntero a UNA fila
- `print_matrix(int (*mat)[COLS], int rows)` — recibe la matriz completa
- `sum_matrix(int (*mat)[COLS], int rows)` — suma todos los elementos

Todas usan `int (*)[COLS]` como parametro. No `int **`, no `int *`, no
`int mat[][COLS]` (aunque esta ultima seria equivalente).

Antes de ejecutar, predice:

- Cuanto vale `sizeof(int[COLS])` con COLS = 4?
- Si `p` apunta a la fila 0, cuantos bytes hay entre `p + 0` y `p + 1`?
- Cual es la suma de todos los elementos de la matriz {{1,2,3,4},
  {5,6,7,8}, {9,10,11,12}}?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ptr_to_array_2d.c -o ptr_to_array_2d
./ptr_to_array_2d
```

Salida esperada:

```
=== pointer to array for 2D matrices ===

Full matrix (via print_matrix):
   1   2   3   4
   5   6   7   8
   9  10  11  12

Row by row (via print_row):
  row 0:    1   2   3   4
  row 1:    5   6   7   8
  row 2:    9  10  11  12

=== pointer arithmetic on int (*p)[4] ===

p + 0 = <addr>  (row 0)
p + 1 = <addr>  (row 1)
p + 2 = <addr>  (row 2)

Each step = 16 bytes (sizeof(int[4]))

Sum of all elements = 78
```

### Paso 3.3 — Analisis de resultados

Verifica tus predicciones:

- `sizeof(int[4]) = 16`: cada fila es un bloque contiguo de 4 ints (4 x 4 =
  16 bytes).
- Entre `p + 0` y `p + 1` hay exactamente 16 bytes (0x10 en hexadecimal).
  La aritmetica de punteros avanza por el tamano del tipo apuntado, que es
  `int[4]`.
- La suma es 78: (1+2+3+4) + (5+6+7+8) + (9+10+11+12) = 10 + 26 + 42 = 78.

El punto clave: cuando una funcion recibe `int (*mat)[COLS]`, el compilador
sabe que cada "paso" de `mat` avanza `COLS * sizeof(int)` bytes. Por eso
`mat[i][j]` funciona correctamente — internamente es `*(*(mat + i) + j)`.

### Paso 3.4 — Por que no usar int **

Antes de continuar, piensa: se podria declarar `print_matrix(int **mat, ...)`
en lugar de `int (*mat)[COLS]`?

No. Un `int **` es un puntero a puntero. Espera que `mat[0]` sea un `int*`
(una direccion). Pero en un `int m[3][4]`, `m[0]` no es un puntero almacenado
en memoria — es la direccion calculada del inicio de la fila 0. No hay
punteros intermedios. Un `int **` seguiria dos niveles de indirection, lo
cual causaria un segfault con una matriz declarada como `int m[3][4]`.

### Limpieza de la parte 3

```bash
rm -f ptr_to_array_2d
```

---

## Parte 4 — Declaraciones complejas y typedef

**Objetivo**: Leer declaraciones complejas de C usando la regla de
precedencia y simplificarlas con typedef.

### Paso 4.1 — Ejercicio de decodificacion manual

Antes de ver el codigo, intenta decodificar estas declaraciones usando la
regla espiral (derecha-izquierda):

```
1. int *p[5]
2. int (*p)[5]
3. int *(*p)[5]
4. int (*fp)(int, int)
5. int (*ops[3])(int)
```

Para cada una, empieza por el nombre de la variable, ve a la derecha (si hay
`[]` o `()`), luego a la izquierda (si hay `*`), y repite.

Intenta resolverlas antes de continuar al siguiente paso.

### Paso 4.2 — Verificar las decodificaciones

Las respuestas:

1. `int *p[5]` — p es un array de 5 punteros a int
2. `int (*p)[5]` — p es un puntero a un array de 5 ints
3. `int *(*p)[5]` — p es un puntero a un array de 5 punteros a int
4. `int (*fp)(int, int)` — fp es un puntero a funcion que toma (int, int) y
   retorna int
5. `int (*ops[3])(int)` — ops es un array de 3 punteros a funcion que toman
   int y retornan int

### Paso 4.3 — Examinar el codigo con typedef

```bash
cat complex_decl.c
```

Observa como typedef simplifica las declaraciones:

| Sin typedef | Con typedef |
|-------------|-------------|
| `int (*matrix)[4]` | `Row *matrix` |
| `int (*fp)(int, int)` | `BinaryOp fp` |
| `int (*ops[3])(int, int)` | `BinaryOp ops[3]` |

### Paso 4.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic complex_decl.c -o complex_decl
./complex_decl
```

Salida esperada:

```
=== complex declarations with typedef ===

Row *matrix (typedef for int (*matrix)[4]):
  row 0:  1  2  3  4
  row 1:  5  6  7  8
  row 2:  9 10 11 12

BinaryOp fp = add:
  fp(10, 3) = 13

BinaryOp ops[3] (array of 3 function pointers):
  ops[0](10, 3) = 13  (add)
  ops[1](10, 3) =  7  (sub)
  ops[2](10, 3) = 30  (mul)

int *(*p)[5] — pointer to array of 5 pointers to int:
  (*p)[0] = <addr>  ->  *(*p)[0] = 100
  (*p)[1] = <addr>  ->  *(*p)[1] = 200
  (*p)[2] = <addr>  ->  *(*p)[2] = 300
  (*p)[3] = <addr>  ->  *(*p)[3] = 400
  (*p)[4] = <addr>  ->  *(*p)[4] = 500
```

### Paso 4.5 — Analisis de resultados

Puntos clave:

- `Row *matrix` es exactamente igual a `int (*matrix)[4]`. El typedef `Row`
  encapsula `int[4]`, haciendo la declaracion legible.
- `BinaryOp fp` reemplaza `int (*fp)(int, int)`. Sin typedef, un array de
  punteros a funcion seria `int (*ops[3])(int, int)` — con typedef, es
  simplemente `BinaryOp ops[3]`.
- `int *(*p)[5]` tiene 3 niveles: p es un puntero, a un array de 5 elementos,
  donde cada elemento es un puntero a int. Se accede con `*(*p)[i]`.

La recomendacion practica: cuando una declaracion tiene mas de un nivel de
`*` o mezcla `*` con `[]` o `()`, usa typedef. El codigo queda mas legible
y menos propenso a errores.

### Limpieza de la parte 4

```bash
rm -f complex_decl
```

---

## Limpieza final

```bash
rm -f sizeof_compare array_of_ptrs ptr_to_array_2d complex_decl
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  array_of_ptrs.c  complex_decl.c  ptr_to_array_2d.c  sizeof_compare.c
```

---

## Conceptos reforzados

1. `int *p[5]` (sin parentesis) declara un **array de 5 punteros a int** —
   sizeof es 40 bytes (5 punteros x 8 bytes). `int (*p)[5]` (con parentesis)
   declara un **puntero a un array de 5 ints** — sizeof es 8 bytes (un
   puntero).

2. La precedencia de `[]` es mayor que `*`. Los parentesis en `(*p)` son
   necesarios para forzar que `p` sea un puntero antes de aplicar `[5]`.

3. `char *names[]` es un **ragged array** — cada elemento es un puntero a un
   string de longitud diferente. sizeof del array cuenta los punteros (8 bytes
   cada uno), no la longitud de los strings.

4. Para pasar una matriz 2D `int m[R][C]` a una funcion, el parametro
   correcto es `int (*mat)[C]` (puntero a array de C ints). La aritmetica
   de punteros avanza `C * sizeof(int)` bytes por cada incremento.

5. `int **` no es equivalente a una matriz 2D. Un `int **` espera punteros
   almacenados en memoria, mientras que un `int m[R][C]` es un bloque
   contiguo sin punteros intermedios.

6. La regla espiral (derecha-izquierda) decodifica cualquier declaracion de
   C: empezar por el nombre, ir a la derecha (`[]`, `()`), luego a la
   izquierda (`*`), y repetir.

7. typedef simplifica declaraciones complejas. `typedef int Row[4]` convierte
   `int (*matrix)[4]` en `Row *matrix`. `typedef int (*BinaryOp)(int, int)`
   convierte un array de punteros a funcion en `BinaryOp ops[3]`.
