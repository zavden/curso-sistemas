# T02 — Punteros a arrays vs arrays de punteros

## La confusión

La posición de `*` y `[]` cambia completamente el significado.
Los paréntesis son críticos:

```c
int *p[5];       // array de 5 punteros a int
int (*p)[5];     // puntero a un array de 5 ints
```

```c
// Regla de precedencia:
// [] tiene mayor precedencia que *
//
// int *p[5]:
//   1. p[5]  → p es un array de 5 elementos
//   2. int * → cada elemento es un int*
//   → array de 5 punteros a int
//
// int (*p)[5]:
//   1. (*p)  → p es un puntero (los paréntesis fuerzan esto)
//   2. [5]   → a un array de 5 elementos
//   3. int   → de tipo int
//   → puntero a un array de 5 ints
```

## Array de punteros: int *p[5]

```c
#include <stdio.h>

int main(void) {
    int a = 10, b = 20, c = 30;

    // Array de 3 punteros a int:
    int *ptrs[3] = { &a, &b, &c };

    // ptrs[0] es un int* que apunta a a
    // ptrs[1] es un int* que apunta a b
    // ptrs[2] es un int* que apunta a c

    for (int i = 0; i < 3; i++) {
        printf("*ptrs[%d] = %d\n", i, *ptrs[i]);
    }

    // sizeof(ptrs) = 3 * sizeof(int*) = 24

    return 0;
}
```

```c
// Uso principal: array de strings
const char *names[] = {
    "Alice",
    "Bob",
    "Charlie",
};
// names[0] → "Alice"
// names[1] → "Bob"
// names[2] → "Charlie"

// Cada elemento es un char* que apunta a un string literal.
// Los strings pueden tener longitudes diferentes (ragged array).

// sizeof(names) = 3 * sizeof(char*) = 24
// strlen(names[0]) = 5
```

```c
// argv es un array de punteros a char:
int main(int argc, char *argv[]) {
    // char *argv[] es equivalente a char **argv
    // argv[0] → nombre del programa
    // argv[1] → primer argumento
    // ...
    // argv[argc] → NULL
}
```

## Puntero a array: int (*p)[5]

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    // p es un puntero al array COMPLETO:
    int (*p)[5] = &arr;

    // Acceso:
    (*p)[0];     // 10 — desreferenciar p da el array, luego indexar
    (*p)[4];     // 50

    // sizeof(*p) = sizeof(int[5]) = 20
    // sizeof(p)  = 8 (tamaño del puntero)

    // p + 1 avanza sizeof(int[5]) = 20 bytes:
    printf("p     = %p\n", (void *)p);
    printf("p + 1 = %p\n", (void *)(p + 1));
    // Diferencia: 20 bytes (no 4)

    return 0;
}
```

```c
// ¿Cuándo se usa int (*p)[5]?
// Principalmente para pasar arrays 2D a funciones:

void print_row(int (*row)[4]) {
    for (int j = 0; j < 4; j++) {
        printf("%4d", (*row)[j]);
    }
    printf("\n");
}

void print_matrix(int (*mat)[4], int rows) {
    // mat es un puntero al primer sub-array de 4 ints
    // mat[i] accede a la fila i (aritmética avanza por arrays de 4)
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
    // m decae a int (*)[4] — puntero al primer array de 4 ints
    return 0;
}
```

## Comparación visual

```c
// int *p[5] — array de punteros:
//
// p: [ptr0][ptr1][ptr2][ptr3][ptr4]
//      ↓     ↓     ↓     ↓     ↓
//     int   int   int   int   int    (en ubicaciones independientes)
//
// sizeof(p) = 5 * 8 = 40 bytes (en 64-bit)
// p[i] es un int*

// int (*p)[5] — puntero a array:
//
// p: [ptr]
//      ↓
//     [int][int][int][int][int]    (bloque contiguo)
//
// sizeof(p) = 8 bytes (solo el puntero)
// *p es un int[5]
// (*p)[i] es un int
```

## Regla de decodificación (clockwise/spiral rule)

Para leer declaraciones complejas de C, usar la regla
de espiral o de derecha a izquierda:

```c
// 1. Empezar por el nombre de la variable
// 2. Ir a la derecha (si hay [] o ())
// 3. Ir a la izquierda (si hay *)
// 4. Repetir hasta terminar

// Ejemplo 1: int *p[5]
// p → [5]    "p es un array de 5"
//   → *      "punteros a"
//   → int    "int"
// Resultado: "p es un array de 5 punteros a int"

// Ejemplo 2: int (*p)[5]
// p → *      "p es un puntero a"  (paréntesis fuerzan)
//   → [5]    "un array de 5"
//   → int    "int"
// Resultado: "p es un puntero a un array de 5 ints"

// Ejemplo 3: int *(*p)[5]
// p → *      "p es un puntero a"
//   → [5]    "un array de 5"
//   → *      "punteros a"
//   → int    "int"
// Resultado: "p es un puntero a un array de 5 punteros a int"
```

```c
// Ejemplo 4: int (*fp)(int, int)
// fp → *           "fp es un puntero a"
//    → (int, int)  "una función que toma (int, int)"
//    → int         "que retorna int"
// Resultado: "fp es un puntero a función (int,int)→int"

// Ejemplo 5: int (*arr[3])(int)
// arr → [3]     "arr es un array de 3"
//     → *       "punteros a"
//     → (int)   "funciones que toman int"
//     → int     "que retornan int"
// Resultado: "arr es un array de 3 punteros a función (int)→int"
```

## typedef para simplificar

```c
// Las declaraciones complejas se vuelven legibles con typedef:

// Sin typedef:
int (*matrix)[4];                          // puntero a array de 4 ints
int (*fp)(int, int);                       // puntero a función
int (*arr_of_fp[3])(int);                  // array de 3 punteros a función
int *(*fp_returning_ptr)(const char *);    // función que retorna int*

// Con typedef:
typedef int Row[4];
Row *matrix;                    // puntero a Row (array de 4 ints)

typedef int (*BinaryOp)(int, int);
BinaryOp fp;                   // puntero a función

BinaryOp ops[3];               // array de 3 punteros a función

typedef int *(*StrToIntPtr)(const char *);
StrToIntPtr fp2;               // función que retorna int*
```

## cdecl — Herramienta para descifrar

```bash
# El comando cdecl traduce declaraciones C a inglés:
$ cdecl
cdecl> explain int *p[5]
declare p as array 5 of pointer to int

cdecl> explain int (*p)[5]
declare p as pointer to array 5 of int

cdecl> explain int (*fp)(int, int)
declare fp as pointer to function (int, int) returning int

# También traduce de inglés a C:
cdecl> declare p as pointer to array 5 of pointer to int
int *(*p)[5]

# Instalar: apt install cdecl
# O usar online: cdecl.org
```

---

## Ejercicios

### Ejercicio 1 — Decodificar declaraciones

```c
// Traducir cada declaración a español:
// 1. char **argv
// 2. int (*fp)(void)
// 3. const char *const *p
// 4. int *(*p)[10]
// 5. void (*signal(int, void (*)(int)))(int)
// (la última es la firma real de signal() en C)
```

### Ejercicio 2 — Escribir declaraciones

```c
// Escribir las declaraciones para:
// 1. Array de 10 punteros a double
// 2. Puntero a un array de 10 doubles
// 3. Función que retorna un puntero a un array de 5 ints
// 4. Array de 3 punteros a funciones que toman void y retornan int
// Verificar con cdecl o compilando.
```

### Ejercicio 3 — Pasar matriz a función

```c
// Crear una función que reciba una matriz int m[N][M]
// usando la sintaxis int (*mat)[M].
// La función debe calcular la suma de todos los elementos.
// Probar con una matriz 3×4.
```
