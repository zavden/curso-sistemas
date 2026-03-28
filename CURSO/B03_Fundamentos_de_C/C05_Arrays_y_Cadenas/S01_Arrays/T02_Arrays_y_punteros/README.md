# T02 — Arrays y punteros

## La relación array → puntero

En la mayoría de contextos, el nombre de un array **decae** (decay)
a un puntero al primer elemento:

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    // arr decae a &arr[0]:
    int *p = arr;           // equivale a: int *p = &arr[0];

    printf("%d\n", *p);     // 10 — primer elemento
    printf("%d\n", p[2]);   // 30 — notación de array con puntero
    printf("%d\n", arr[2]); // 30 — misma operación

    // arr[i] se transforma en *(arr + i) por el compilador:
    printf("%d\n", *(arr + 2));   // 30 — equivalente a arr[2]
    printf("%d\n", *(p + 2));     // 30 — equivalente a p[2]

    return 0;
}
```

```c
// El decay ocurre automáticamente:
int arr[5] = {1, 2, 3, 4, 5};

int *p = arr;           // decay a puntero — OK sin &
int *q = &arr[0];       // explícitamente tomar la dirección del primer elem
// p == q → true

// Pero el array NO es un puntero. Son cosas distintas
// que se comportan igual en ciertos contextos.
```

## Array ≠ Puntero

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {1, 2, 3, 4, 5};
    int *ptr = arr;

    // DIFERENCIA 1: sizeof
    printf("sizeof(arr) = %zu\n", sizeof(arr));    // 20 (5 × 4)
    printf("sizeof(ptr) = %zu\n", sizeof(ptr));    // 8 (tamaño del puntero)

    // DIFERENCIA 2: & (dirección de)
    printf("arr  = %p\n", (void *)arr);       // dirección del primer elem
    printf("&arr = %p\n", (void *)&arr);      // misma dirección numérica...
    // ...pero &arr tiene tipo int (*)[5] (puntero a array de 5 ints)
    // y arr (decayed) tiene tipo int* (puntero a int)

    printf("arr + 1  = %p\n", (void *)(arr + 1));   // avanza 4 bytes (1 int)
    printf("&arr + 1 = %p\n", (void *)(&arr + 1));  // avanza 20 bytes (1 array)

    // DIFERENCIA 3: asignación
    // arr = ptr;    // ERROR: array no es un lvalue modificable
    ptr = arr;       // OK: puntero se puede reasignar

    return 0;
}
```

```c
// Resumen de diferencias:
//
// | Operación      | Array (int arr[5])  | Puntero (int *p)    |
// |----------------|---------------------|---------------------|
// | sizeof         | 20 (tamaño total)   | 8 (tamaño puntero)  |
// | &x             | int (*)[5]          | int **              |
// | &x + 1         | avanza sizeof(arr)  | avanza sizeof(int*) |
// | x = something  | ERROR               | OK                  |
// | Declaración    | reserva memoria     | solo almacena addr  |
```

## Cuándo NO hay decay

El nombre del array NO decae a puntero en tres contextos:

```c
int arr[5] = {1, 2, 3, 4, 5};

// 1. sizeof(arr) — retorna el tamaño del array completo:
size_t total = sizeof(arr);        // 20, no 8

// 2. &arr — retorna puntero al array completo:
int (*p)[5] = &arr;                // tipo: int (*)[5]

// 3. Inicialización de string literal en char[]:
char str[] = "hello";              // copia el string, no decay
// str es un array de 6 chars, no un puntero
```

## Aritmética de punteros con arrays

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;

// p + n avanza n ELEMENTOS (no bytes):
printf("%d\n", *(p + 0));   // 10 — arr[0]
printf("%d\n", *(p + 1));   // 20 — arr[1]
printf("%d\n", *(p + 4));   // 50 — arr[4]

// Internamente: p + n → dirección p + n * sizeof(int)
// Si p = 0x1000:
//   p + 0 = 0x1000
//   p + 1 = 0x1004 (avanzó 4 bytes)
//   p + 2 = 0x1008
```

```c
// arr[i] es EXACTAMENTE *(arr + i):
int arr[5] = {10, 20, 30, 40, 50};

arr[3];          // *(arr + 3) → 40
3[arr];          // *(3 + arr) → 40 — legal pero nunca usar esto
// La suma es conmutativa: arr + 3 == 3 + arr
// Por eso 3[arr] compila — es un accidente de la definición
```

```c
// Recorrer con puntero vs con índice:

int arr[5] = {10, 20, 30, 40, 50};

// Con índice:
for (int i = 0; i < 5; i++) {
    printf("%d ", arr[i]);
}

// Con puntero:
for (int *p = arr; p < arr + 5; p++) {
    printf("%d ", *p);
}

// Con puntero y end:
int *end = arr + 5;    // puntero "past the end" (apunta después del último)
for (int *p = arr; p != end; p++) {
    printf("%d ", *p);
}
// arr + 5 es válido como dirección pero NO se puede desreferenciar.
// Es un patrón común para marcar el final (como iteradores en C++).
```

## Diferencia de punteros

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = &arr[1];    // apunta a 20
int *q = &arr[4];    // apunta a 50

ptrdiff_t diff = q - p;    // 3 (elementos, no bytes)
// ptrdiff_t está en <stddef.h>, formato %td

printf("q - p = %td\n", diff);    // 3

// Solo es válido si ambos punteros apuntan al mismo array
// (o a uno-past-the-end). Si no: UB.
```

## Pasar arrays a funciones

```c
// Cuando se pasa un array a una función, decae a puntero.
// La función NO sabe el tamaño:

void print_arr(int arr[], int n) {
    // arr es un int*, no un array
    // sizeof(arr) == sizeof(int*) == 8
    // Por eso necesitamos n
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

// La función puede modificar el array original:
void zero_fill(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = 0;
    }
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    print_arr(data, 5);      // 1 2 3 4 5
    zero_fill(data, 5);
    print_arr(data, 5);      // 0 0 0 0 0
    return 0;
}
```

```c
// Para impedir modificación, usar const:
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
        // arr[i] = 0;    // ERROR: const
    }
    return total;
}
```

## El trap clásico: sizeof en función

```c
#include <stdio.h>

void broken(int arr[]) {
    // arr es un puntero, no un array:
    int n = sizeof(arr) / sizeof(arr[0]);
    printf("n = %d\n", n);    // n = 2 en 64-bit (8/4)
    // WARNING: sizeof on array function parameter will return
    //          size of 'int *' instead of 'int []'
}

void correct(int *arr, int n) {
    // n se pasa explícitamente
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};

    // Aquí sizeof funciona — data es un array real:
    int n = sizeof(data) / sizeof(data[0]);    // 5
    printf("n = %d\n", n);

    broken(data);         // imprime n = 2 — INCORRECTO
    correct(data, n);     // funciona bien

    return 0;
}
```

## String literals y arrays

```c
// Un string literal es un array de char (con terminador \0):

// Caso 1: puntero a string literal
const char *s = "hello";
// s apunta a un string literal en memoria de solo lectura
// s[0] = 'H';    // UB — modificar string literal

// Caso 2: array inicializado con string literal
char arr[] = "hello";
// arr es un array de 6 chars: {'h','e','l','l','o','\0'}
// arr se puede modificar — es una copia
arr[0] = 'H';    // OK — arr = "Hello"

// sizeof:
printf("sizeof(s)   = %zu\n", sizeof(s));     // 8 (puntero)
printf("sizeof(arr) = %zu\n", sizeof(arr));   // 6 (array de 6 chars)
```

## Patrón: puntero a array completo

```c
// int (*p)[5] es un puntero a un array de 5 ints:
int arr[5] = {1, 2, 3, 4, 5};
int (*p)[5] = &arr;

// Acceso:
(*p)[0];         // 1 — desreferenciar p da el array, luego indexar
(*p)[4];         // 5

// p + 1 avanza 20 bytes (sizeof(int[5])):
// Útil para recorrer arrays de arrays (matrices)

// No confundir:
int (*p)[5];     // puntero a array de 5 ints
int *p[5];       // array de 5 punteros a int
```

---

## Ejercicios

### Ejercicio 1 — sizeof trap

```c
// Crear una función que reciba un array y un int n.
// Dentro de la función, imprimir sizeof del parámetro.
// En main, imprimir sizeof del array real.
// Verificar que son diferentes y explicar por qué.
```

### Ejercicio 2 — Recorrer con punteros

```c
// Dado int arr[] = {5, 10, 15, 20, 25, 30}:
// Recorrer e imprimir de 3 formas:
// 1. Con índice (arr[i])
// 2. Con puntero que avanza (p++)
// 3. Con aritmética (*(arr + i))
// Verificar que las 3 dan el mismo resultado.
```

### Ejercicio 3 — Reverse con punteros

```c
// Implementar void reverse(int *arr, int n) usando
// dos punteros: uno al inicio y otro al final.
// Intercambiar elementos moviendo los punteros hacia el centro.
// No usar índices.
```
