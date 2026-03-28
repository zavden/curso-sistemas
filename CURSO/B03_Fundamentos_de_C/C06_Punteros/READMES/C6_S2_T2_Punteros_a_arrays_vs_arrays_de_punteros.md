# T02 — Punteros a arrays vs arrays de punteros

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `sizeof_compare.c`, `array_of_ptrs.c`, `ptr_to_array_2d.c`, `complex_decl.c`
>
> Sin erratas detectadas en el material fuente.

---

## 1. La confusión: `int *p[5]` vs `int (*p)[5]`

La posición de los paréntesis cambia **completamente** el significado:

```c
int *p[5];       // array de 5 punteros a int
int (*p)[5];     // puntero a un array de 5 ints
```

Son tipos completamente distintos con sizeof, aritmética y uso diferentes. La confusión es una de las más comunes en C.

---

## 2. Regla de precedencia: `[]` > `*`

`[]` tiene **mayor precedencia** que `*`. Esto explica la diferencia:

```
int *p[5]:
  1. p[5]  → p es un array de 5 elementos
  2. int * → cada elemento es un int*
  → "array de 5 punteros a int"

int (*p)[5]:
  1. (*p)  → p es un puntero (paréntesis fuerzan esto)
  2. [5]   → a un array de 5 elementos
  3. int   → de tipo int
  → "puntero a un array de 5 ints"
```

Los paréntesis en `(*p)` son **necesarios** para que `*` se aplique antes que `[]`. Sin ellos, la precedencia de `[]` gana y `p` se interpreta como array.

---

## 3. Array de punteros: `int *p[N]`

Un array cuyos elementos son punteros. Cada elemento almacena una dirección:

```c
int a = 10, b = 20, c = 30;
int *ptrs[3] = { &a, &b, &c };

// ptrs[0] es un int* que apunta a a
// ptrs[1] es un int* que apunta a b
// ptrs[2] es un int* que apunta a c

// sizeof(ptrs) = 3 * sizeof(int*) = 24 bytes
// Los datos apuntados pueden estar en ubicaciones independientes
```

Representación en memoria:

```
ptrs: [ptr0][ptr1][ptr2]
        ↓     ↓     ↓
       int   int   int    (ubicaciones independientes)

sizeof(ptrs) = N × 8 bytes (en 64-bit)
ptrs[i] es un int*
```

### Uso principal: array de strings (ragged array)

```c
const char *names[] = {
    "Alice",       // strlen = 5
    "Bob",         // strlen = 3
    "Charlie",     // strlen = 7
};
// sizeof(names) = 3 * sizeof(char*) = 24 (cuenta punteros, no chars)
// Cada string tiene longitud diferente — "ragged array"
```

Con un array 2D fijo (`char names[3][8]`) cada fila ocupa 8 bytes aunque "Bob" solo necesita 4. Un array de punteros elimina este desperdicio.

### `argv` es un array de punteros

```c
int main(int argc, char *argv[]) {
    // char *argv[] es equivalente a char **argv
    // argv[0] → nombre del programa
    // argv[1] → primer argumento
    // argv[argc] → NULL (centinela)
    // Cada argumento tiene longitud diferente — ragged array
}
```

---

## 4. Puntero a array: `int (*p)[N]`

Un único puntero que apunta a un **array completo** como unidad:

```c
int arr[5] = {10, 20, 30, 40, 50};
int (*p)[5] = &arr;     // p apunta al array completo

// Acceso: desreferenciar p da el array, luego indexar
(*p)[0];     // 10
(*p)[4];     // 50

// sizeof(p)  = 8 bytes (solo el puntero)
// sizeof(*p) = 20 bytes (el array completo: 5 × 4)
```

Representación en memoria:

```
p: [ptr]
     ↓
    [int][int][int][int][int]    (bloque contiguo)

sizeof(p) = 8 bytes
*p es un int[5]
(*p)[i] es un int
```

La clave: `p + 1` avanza **`sizeof(int[5])` = 20 bytes**, no 4. La aritmética de punteros avanza por el tamaño del tipo apuntado, que aquí es un array completo.

---

## 5. `sizeof` y aritmética — las diferencias clave

| Propiedad | `int *p[5]` | `int (*p)[5]` |
|-----------|-------------|---------------|
| Qué es | Array de 5 punteros | Un puntero |
| `sizeof(p)` | 40 (5 × 8) | 8 |
| `p[0]` es | `int *` | `int[5]` (un array) |
| `sizeof(p[0])` | 8 | 20 |
| `p + 1` avanza | 8 bytes | 20 bytes |
| Se inicializa con | Direcciones: `{&a, &b, &c, ...}` | `&arr` (dirección de un array) |

Regla mnemotécnica: si `sizeof(p)` da un número grande (múltiplo de 8 × N), es un array de punteros. Si da 8, es un puntero.

---

## 6. Matrices 2D: por qué `int (*mat)[COLS]` y no `int **`

Para pasar una matriz 2D `int m[ROWS][COLS]` a una función, el parámetro correcto es `int (*mat)[COLS]`:

```c
void print_matrix(int (*mat)[4], int rows) {
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
    // m decae a int (*)[4] — puntero al primer sub-array de 4 ints
}
```

`mat[i][j]` funciona porque internamente es `*(*(mat + i) + j)`:
- `mat + i` avanza `i * sizeof(int[4])` = `i * 16` bytes → apunta a la fila `i`
- `*(mat + i)` es `int[4]` que decae a `int *` → apunta al primer elemento de la fila
- `*(mat + i) + j` avanza `j * sizeof(int)` = `j * 4` bytes → apunta al elemento `[i][j]`

### Por qué `int **` NO funciona

```c
void bad_print(int **mat, int rows, int cols);  // INCORRECTO

int m[3][4] = {...};
bad_print(m, 3, 4);    // ¡ERROR de tipos!
```

Un `int **` es un puntero a puntero. Espera que `mat[0]` sea un `int *` almacenado en memoria (una dirección de 8 bytes). Pero en `int m[3][4]`, no hay punteros intermedios — es un bloque contiguo de 48 bytes (12 ints). `mat[0]` no es un puntero guardado en memoria, sino la dirección calculada del inicio de la fila 0. Usar `int **` con una matriz 2D causaría segfault al intentar leer 8 bytes de datos como una dirección.

`int **` solo es válido para arrays dinámicos donde cada fila se asignó por separado con `malloc` (ahí sí existen punteros intermedios en memoria).

### Sintaxis alternativa equivalente

```c
void f(int (*mat)[4], int rows);    // puntero a array (explícito)
void f(int mat[][4], int rows);     // array de tamaño desconocido de arrays (equivalente)
// Ambas son idénticas — mat[][] decae a int (*)[4]
```

---

## 7. Ragged arrays vs matrices 2D fijas

| Propiedad | `char names[4][8]` | `const char *names[4]` |
|-----------|--------------------|-----------------------|
| Tipo | Matriz 2D fija | Array de punteros (ragged) |
| Memoria | Bloque contiguo (32 bytes) | 4 punteros + strings separados |
| Cada "fila" | Siempre 8 bytes | Longitud variable |
| Desperdicio | Sí (padding si nombre < 8) | No |
| `sizeof` | 32 | 32 (solo los punteros) |
| Strings literales | Se copian al array | Los punteros apuntan a `.rodata` |
| Se puede modificar | Sí (`names[0][0] = 'x'`) | No (string literal → UB) |

Ragged arrays son ideales cuando los elementos tienen longitudes muy diferentes (nombres, rutas de archivo, líneas de texto).

---

## 8. Regla espiral (clockwise/spiral rule)

Para decodificar cualquier declaración compleja de C:

1. **Empezar** por el nombre de la variable
2. Ir a la **derecha** (si hay `[]` o `()`)
3. Ir a la **izquierda** (si hay `*`)
4. **Repetir** hasta terminar

```
Ejemplo 1: int *p[5]
  p → [5]    "p es un array de 5"
    → *      "punteros a"
    → int    "int"
  Resultado: "p es un array de 5 punteros a int"

Ejemplo 2: int (*p)[5]
  p → *      "p es un puntero a"  (paréntesis fuerzan)
    → [5]    "un array de 5"
    → int    "int"
  Resultado: "p es un puntero a un array de 5 ints"

Ejemplo 3: int *(*p)[5]
  p → *      "p es un puntero a"
    → [5]    "un array de 5"
    → *      "punteros a"
    → int    "int"
  Resultado: "p es un puntero a un array de 5 punteros a int"

Ejemplo 4: int (*fp)(int, int)
  fp → *          "fp es un puntero a"
     → (int,int)  "una función que toma (int, int)"
     → int        "que retorna int"
  Resultado: "fp es un puntero a función (int,int)→int"

Ejemplo 5: int (*ops[3])(int)
  ops → [3]    "ops es un array de 3"
      → *      "punteros a"
      → (int)  "funciones que toman int"
      → int    "que retornan int"
  Resultado: "ops es un array de 3 punteros a función (int)→int"
```

La declaración más famosa:

```
void (*signal(int, void (*)(int)))(int)
  signal → (int, void(*)(int))  "signal es una función que toma (int, puntero a función)"
         → *                     "que retorna un puntero a"
         → (int)                 "una función que toma int"
         → void                  "que retorna void"
```

---

## 9. `typedef` para simplificar

Cuando una declaración tiene más de un nivel de `*` o mezcla `*` con `[]` o `()`, `typedef` la vuelve legible:

```c
// Sin typedef:
int (*matrix)[4];                        // puntero a array de 4 ints
int (*fp)(int, int);                     // puntero a función
int (*ops[3])(int, int);                 // array de 3 punteros a función
int *(*fp_returning_ptr)(const char *);  // función que retorna int*

// Con typedef:
typedef int Row[4];
Row *matrix;                    // puntero a Row (= int (*)[4])

typedef int (*BinaryOp)(int, int);
BinaryOp fp;                   // puntero a función
BinaryOp ops[3];               // array de 3 punteros a función

typedef int *(*StrToIntPtr)(const char *);
StrToIntPtr fp2;               // función que retorna int*
```

Beneficio práctico:

```c
// Sin typedef — difícil de leer:
int (*get_operation(char op))(int, int) {
    if (op == '+') return add;
    return sub;
}

// Con typedef — claro:
BinaryOp get_operation(char op) {
    if (op == '+') return add;
    return sub;
}
```

### `cdecl` — herramienta para descifrar

```bash
$ cdecl
cdecl> explain int *p[5]
declare p as array 5 of pointer to int

cdecl> explain int (*p)[5]
declare p as pointer to array 5 of int

# También traduce de inglés a C:
cdecl> declare p as pointer to array 5 of pointer to int
int *(*p)[5]

# Instalar: apt install cdecl
# O usar online: cdecl.org
```

---

## 10. Comparación con Rust

Rust elimina esta confusión de sintaxis con tipos claros:

```rust
// Array de punteros — equivalente a int *p[5]:
let ptrs: [&i32; 5];           // array de 5 referencias a i32
let ptrs: [Box<i32>; 5];       // array de 5 punteros owned a i32

// Referencia a array — equivalente a int (*p)[5]:
let p: &[i32; 5];              // referencia a array de 5 i32

// Slice — alternativa más flexible:
let s: &[i32];                 // referencia a slice (array + longitud)
fn sum(arr: &[i32]) -> i32;    // acepta arrays de cualquier tamaño
```

| C | Rust | Notas |
|---|------|-------|
| `int *p[5]` | `[&i32; 5]` | Sintaxis clara — array de referencias |
| `int (*p)[5]` | `&[i32; 5]` | Referencia a array |
| `int (*mat)[COLS]` | `&[[i32; COLS]]` | Slice de arrays |
| `int **` | `Vec<Vec<i32>>` o `&[&[i32]]` | Sin confusión con matrices 2D |
| `int (*fp)(int,int)` | `fn(i32,i32) -> i32` | Tipo función directamente |

En Rust no existe la confusión `*p[5]` vs `(*p)[5]` porque la sintaxis es inequívoca. Además, los slices `&[T]` llevan el tamaño incorporado, así que no se necesita pasar `size_t n` por separado.

---

## Ejercicios

### Ejercicio 1 — sizeof revela el tipo

```c
// Declara:
//   int *arr_ptrs[5];
//   int (*ptr_arr)[5];
//   int data[5];
// Imprime sizeof de cada uno y de *ptr_arr, *arr_ptrs[0], data[0].
// ¿Cuáles dan 8, cuáles 20, cuáles 40, cuáles 4?
```

<details><summary>Predicción</summary>

`sizeof(arr_ptrs)` = 40 (5 punteros × 8). `sizeof(ptr_arr)` = 8 (un puntero). `sizeof(data)` = 20 (5 ints × 4). `sizeof(*ptr_arr)` = 20 (el array de 5 ints al que apunta). `sizeof(*arr_ptrs[0])` = 4 (un int, lo apuntado por el primer puntero). `sizeof(data[0])` = 4 (un int).

</details>

### Ejercicio 2 — Aritmética de punteros a array

```c
// Declara int arr[3][4] con valores del 1 al 12.
// Crea int (*p)[4] = arr;
// Imprime las direcciones de p+0, p+1, p+2 con %p.
// Calcula la diferencia en bytes entre cada par con (char*)(p+1) - (char*)p.
// ¿Cuántos bytes avanza p+1? ¿Por qué no avanza 4 bytes como int*?
```

<details><summary>Predicción</summary>

`p+1` avanza 16 bytes (= `sizeof(int[4])` = 4 × 4). La aritmética de punteros avanza por el tamaño del tipo apuntado. El tipo apuntado de `int (*)[4]` es `int[4]` (16 bytes), no `int` (4 bytes). Las direcciones de `p+0`, `p+1`, `p+2` difieren en 16 (0x10 hex) cada una.

</details>

### Ejercicio 3 — Ragged array de strings

```c
// Crea const char *months[] con los 12 meses del año.
// Imprime sizeof(months), sizeof(months[0]), y strlen de cada mes.
// ¿Cuánto espacio desperdicia un char months[12][10] para "May" vs "September"?
// ¿Cuánto "desperdicia" el array de punteros?
```

<details><summary>Predicción</summary>

`sizeof(months)` = 96 (12 punteros × 8). `sizeof(months[0])` = 8 (un `char *`). Los strlen van de 3 ("May") a 9 ("September"). Con `char months[12][10]`, cada fila ocupa 10 bytes — "May" desperdicia 6 bytes de padding. Total: 120 bytes. Con el array de punteros: 96 bytes de punteros + los strings en `.rodata` (sin padding por fila). El "desperdicio" del array de punteros es que cada puntero ocupa 8 bytes, así que para strings muy cortos el overhead del puntero supera al ahorro. El break-even es aproximadamente strings de 8+ caracteres.

</details>

### Ejercicio 4 — Pasar matriz a función

```c
// Escribe una función int sum_matrix(int (*mat)[4], int rows) que sume
// todos los elementos de una matriz de 'rows' filas y 4 columnas.
// Prueba con int m[3][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}}.
// Intenta también declarar el parámetro como int mat[][4] — ¿compila igual?
// Intenta declararlo como int **mat — ¿qué error obtienes?
```

<details><summary>Predicción</summary>

`int (*mat)[4]` y `int mat[][4]` son equivalentes — ambas compilan y producen la misma función. La suma es 78. Con `int **mat`, GCC da warning "passing argument from incompatible pointer type" porque `int[3][4]` decae a `int (*)[4]`, no a `int **`. Si se fuerza con cast, `mat[i][j]` causaría segfault al intentar leer los bytes del array como dirección de puntero.

</details>

### Ejercicio 5 — `int **` vs `int (*)[N]` visual

```c
// Crea dos formas de almacenar una "matriz" 3×3:
// a) int m[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
//    int (*p1)[3] = m;
// b) int r0[]={1,2,3}, r1[]={4,5,6}, r2[]={7,8,9};
//    int *rows[] = {r0, r1, r2};
//    int **p2 = rows;
// Imprime las direcciones de cada "fila" para ambos.
// En (a), ¿las filas son contiguas? ¿En (b)?
// Accede a ambas con p1[1][2] y p2[1][2] — ¿dan el mismo resultado?
```

<details><summary>Predicción</summary>

En (a), las filas son **contiguas** — `m` es un bloque de 36 bytes (9 ints). Las direcciones de `p1+0`, `p1+1`, `p1+2` difieren en 12 bytes exactos. En (b), las filas son arrays locales independientes — las direcciones de `r0`, `r1`, `r2` están cercanas en el stack pero no necesariamente a distancias de 12 bytes. Ambos `p1[1][2]` y `p2[1][2]` dan 6, pero el mecanismo es diferente: `p1[1][2]` calcula offset directo; `p2[1][2]` sigue un puntero intermedio real.

</details>

### Ejercicio 6 — Regla espiral

```c
// Sin compilar ni usar cdecl, decodifica con la regla espiral:
//   a) char **argv
//   b) int (*fp)(void)
//   c) const char *const *p
//   d) int *(*p)[10]
//   e) void (*signal(int, void (*)(int)))(int)
// Verifica con cdecl o con un programa que use cada declaración.
```

<details><summary>Predicción</summary>

(a) `argv` es un puntero a puntero a `char`. (b) `fp` es un puntero a función que toma `void` y retorna `int`. (c) `p` es un puntero a (`const` puntero a `const char`) — `p` se puede redirigir, `*p` no se puede redirigir, `**p` no se puede modificar. (d) `p` es un puntero a un array de 10 punteros a `int`. (e) `signal` es una función que toma `(int, puntero a función void(int))` y retorna un puntero a función `void(int)`.

</details>

### Ejercicio 7 — typedef para matrices

```c
// Usa typedef para simplificar:
//   a) typedef int Row3[3]; — usa Row3 * como parámetro de función
//   b) typedef int (*Comparator)(const void *, const void *);
// Escribe una función que reciba Row3 *mat e imprima una matriz 4×3.
// Escribe una función sort_and_print que reciba int*, size_t n, y Comparator.
// ¿La legibilidad mejora vs las declaraciones sin typedef?
```

<details><summary>Predicción</summary>

Con `Row3 *mat`, la firma es `void print_matrix(Row3 *mat, int rows)` en lugar de `void print_matrix(int (*mat)[3], int rows)`. Con `Comparator cmp`, la firma es `void sort_and_print(int *arr, size_t n, Comparator cmp)` en lugar de `void sort_and_print(int *arr, size_t n, int (*cmp)(const void *, const void *))`. La legibilidad mejora considerablemente, especialmente para el puntero a función.

</details>

### Ejercicio 8 — Puntero a array con VLA (C99+)

```c
// C99 permite VLA en parámetros de función:
// void print(int rows, int cols, int (*mat)[cols]);
// Escribe esta función e imprima una matriz de tamaño arbitrario.
// Prueba con matrices 2×3, 3×4 y 4×5.
// ¿Qué ventaja tiene sobre int (*mat)[FIXED_COLS]?
// Compila con -std=c11 y verifica que funciona.
```

<details><summary>Predicción</summary>

La función funciona para matrices de cualquier número de columnas, no solo un valor fijo. `cols` debe aparecer **antes** de `mat` en los parámetros para que esté disponible al declarar el tipo. Internamente, el compilador usa `cols` para calcular el stride de `mat+1`. Es la forma más flexible de recibir matrices 2D en C sin usar `int **`. Nota: VLA en parámetros de función son obligatorios en C99/C11 pero opcionales en C23.

</details>

### Ejercicio 9 — Array de punteros a función

```c
// Define funciones: int add(int,int), sub, mul, divide (div ya existe en stdlib).
// Crea un array de punteros a función BinaryOp ops[] = {add, sub, mul, divide};
// con typedef int (*BinaryOp)(int, int);
// Escribe un "calculador" que reciba un char op ('+','-','*','/'),
// busque en el array y ejecute la operación.
// ¿Qué declaración tendría ops[] sin typedef?
```

<details><summary>Predicción</summary>

Sin typedef: `int (*ops[])(int, int) = {add, sub, mul, divide};`. Con typedef: `BinaryOp ops[] = {...}`. El calculador puede usar un switch o mapear `"+-*/"[i]` → `ops[i]` con un loop. `divide` debe verificar que el divisor no sea 0. Las llamadas son `ops[i](a, b)` — el nombre del array con índice actúa como puntero a función que se puede invocar directamente.

</details>

### Ejercicio 10 — Decodificar y escribir declaraciones

```c
// Parte A — Decodifica (di qué tipo es):
//   1. int (*(*fp)(int))[3]
//   2. const char *(*names[4])(void)
//   3. void (*(*factory)(int))(int, int)
//
// Parte B — Escribe la declaración en C para:
//   1. "p es un puntero a un array de 10 doubles"
//   2. "fp es un puntero a función que toma (const char *) y retorna int *"
//   3. "arr es un array de 5 punteros a funciones que toman void y retornan double"
//
// Verifica con cdecl o compilando.
```

<details><summary>Predicción</summary>

**Parte A**: (1) `fp` es un puntero a función que toma `int` y retorna un puntero a un array de 3 ints. (2) `names` es un array de 4 punteros a funciones que toman `void` y retornan `const char *`. (3) `factory` es un puntero a función que toma `int` y retorna un puntero a función `void(int, int)`.

**Parte B**: (1) `double (*p)[10];` (2) `int *(*fp)(const char *);` (3) `double (*arr[5])(void);`

</details>
