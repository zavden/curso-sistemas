# T01 — Declaración y acceso

## Declaración de arrays

Un array es una secuencia contigua de elementos del mismo tipo:

```c
#include <stdio.h>

int main(void) {
    int nums[5];             // 5 ints, valores indeterminados
    double temps[3];         // 3 doubles
    char name[20];           // 20 chars

    // sizeof del array = tamaño_elemento × cantidad:
    printf("sizeof(nums) = %zu\n", sizeof(nums));     // 20 (5 × 4)
    printf("sizeof(temps) = %zu\n", sizeof(temps));    // 24 (3 × 8)

    return 0;
}
```

```c
// El tamaño debe ser una constante en C89:
int arr[10];           // OK — literal
#define N 100
int arr2[N];           // OK — macro se expande a literal

// En C99+, puede ser una variable (VLA, ver más abajo):
int n = 10;
int arr3[n];           // OK en C99/C11 — VLA

// El tamaño debe ser > 0:
int bad[0];            // Comportamiento indefinido en el estándar
                       // GCC lo permite como extensión (zero-length array)
```

## Inicialización

```c
// Inicialización completa:
int nums[5] = {10, 20, 30, 40, 50};

// Inicialización parcial — el resto se pone a 0:
int nums2[5] = {10, 20};        // {10, 20, 0, 0, 0}

// Poner todo a 0:
int zeros[100] = {0};           // todos los elementos son 0
int zeros2[100] = {};           // C23: equivalente a {0}

// Dejar que el compilador determine el tamaño:
int auto_size[] = {1, 2, 3, 4}; // tamaño = 4
// sizeof(auto_size) / sizeof(auto_size[0]) == 4
```

```c
// Designated initializers (C99):
int arr[10] = {
    [0] = 100,
    [5] = 500,
    [9] = 900,
};
// Los no mencionados son 0: {100, 0, 0, 0, 0, 500, 0, 0, 0, 900}

// El índice puede estar desordenado:
int arr2[5] = { [3] = 30, [1] = 10, [0] = 0 };
// {0, 10, 0, 30, 0}

// Combinar posicional y designado:
int arr3[5] = { 1, 2, [4] = 5 };
// {1, 2, 0, 0, 5}
```

```c
// NO se pueden asignar arrays completos después de la declaración:
int a[3] = {1, 2, 3};
int b[3];
b = a;                  // ERROR: array no es un lvalue asignable
// Usar memcpy:
memcpy(b, a, sizeof(a));
```

## Acceso a elementos

```c
int arr[5] = {10, 20, 30, 40, 50};

arr[0];     // 10 — primer elemento (índice 0)
arr[4];     // 50 — último elemento
arr[5];     // UB — fuera de límites (no hay protección)

// Modificar:
arr[2] = 99;    // arr = {10, 20, 99, 40, 50}
```

```c
// C no verifica límites. Esto compila sin warnings:
int arr[5] = {0};
arr[100] = 42;      // UB — escribe en memoria ajena
int x = arr[-1];    // UB — lee antes del array

// Con -fsanitize=address o -fsanitize=bounds:
// runtime error: index 100 out of bounds for type 'int [5]'
```

```c
// Recorrer un array:
int data[5] = {10, 20, 30, 40, 50};
int n = sizeof(data) / sizeof(data[0]);    // 5

for (int i = 0; i < n; i++) {
    printf("data[%d] = %d\n", i, data[i]);
}

// CUIDADO: sizeof solo funciona con el array real,
// no con un puntero recibido en una función (ver T02).
```

## ARRAY_SIZE macro

```c
// Patrón estándar para obtener el número de elementos:
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int nums[] = {1, 2, 3, 4, 5};
for (int i = 0; i < ARRAY_SIZE(nums); i++) {
    printf("%d\n", nums[i]);
}

// Funciona solo con arrays reales, no con punteros.
// En el kernel de Linux se usa una versión que detecta si
// recibe un puntero por error (con __builtin_types_compatible_p).
```

## Variable-Length Arrays (VLAs)

C99 introdujo arrays cuyo tamaño se determina en runtime:

```c
#include <stdio.h>

void process(int n) {
    int data[n];          // VLA — tamaño determinado en runtime
    // Se aloca en el STACK, no en el heap
    // No se puede inicializar con {}
    // sizeof(data) se evalúa en runtime (no en compilación)

    for (int i = 0; i < n; i++) {
        data[i] = i * 10;
    }

    printf("sizeof(data) = %zu (n=%d)\n", sizeof(data), n);
    // sizeof(data) = n * sizeof(int), evaluado en runtime
}

int main(void) {
    process(5);    // sizeof(data) = 20
    process(10);   // sizeof(data) = 40
    return 0;
}
```

### VLAs como parámetros

```c
// VLA permite declarar el tamaño de un array parámetro:
void print_matrix(int rows, int cols, int mat[rows][cols]) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d", mat[i][j]);
        }
        printf("\n");
    }
}

// rows y cols deben aparecer ANTES de mat en la lista de parámetros.
// Internamente, mat sigue siendo un puntero, pero el compilador
// puede calcular el offset correcto: mat[i][j] = *(mat + i*cols + j)
```

### Problemas con VLAs

```c
// 1. Stack overflow — sin control de tamaño:
void dangerous(int n) {
    int arr[n];        // si n = 1000000 → stack overflow
    // El stack típico es 1-8 MB
    // 1,000,000 ints = 4 MB → puede crashear
}

// 2. No se pueden inicializar:
int n = 5;
int arr[n] = {0};     // ERROR: VLA cannot be initialized

// 3. Rendimiento:
// Cada VLA requiere ajustar el stack pointer en runtime.
// Con tamaño fijo, el compilador calcula el frame en compilación.

// 4. Soporte inconsistente:
// C11 hizo VLAs opcionales (__STDC_NO_VLA__)
// MSVC no los soporta
// El kernel de Linux los prohibió (-Wvla)
```

### Alternativas a VLAs

```c
// Para arrays grandes o tamaño variable, usar malloc:
void safe_process(int n) {
    int *data = malloc(n * sizeof(int));
    if (!data) {
        perror("malloc");
        return;
    }

    for (int i = 0; i < n; i++) {
        data[i] = i * 10;
    }

    // usar data...
    free(data);
}

// Para arrays pequeños de tamaño fijo, usar tamaño constante:
#define MAX_ITEMS 256
void fixed_process(void) {
    int data[MAX_ITEMS];
    // ...
}
```

## Arrays const

```c
// const impide modificación:
const int primes[] = {2, 3, 5, 7, 11, 13};
primes[0] = 100;    // ERROR: assignment to const

// Útil para tablas de lookup:
static const char *const months[] = {
    "January", "February", "March", "April",
    "May", "June", "July", "August",
    "September", "October", "November", "December"
};
// const char *const: el puntero Y el string son const
// static: solo visible en este archivo
```

## Tabla resumen

| Característica | Array estático | VLA |
|---|---|---|
| Tamaño | Constante en compilación | Variable en runtime |
| Ubicación | Stack | Stack |
| Inicialización con {} | Sí | No |
| sizeof | Compilación | Runtime |
| Estándar | C89+ | C99 (opcional en C11) |
| Riesgo | Ninguno | Stack overflow |
| Recomendación | Usar siempre que se pueda | Evitar — usar malloc |

---

## Ejercicios

### Ejercicio 1 — Inicialización

```c
// Declarar un array de 10 ints con designated initializers:
// posición 0 = 100, posición 4 = 400, posición 9 = 900.
// Imprimir todos los valores. ¿Qué contienen las posiciones
// no inicializadas explícitamente?
```

### Ejercicio 2 — ARRAY_SIZE

```c
// Crear un array de doubles con valores arbitrarios.
// Usar ARRAY_SIZE para recorrerlo y calcular el promedio.
// Verificar que ARRAY_SIZE da el resultado correcto.
```

### Ejercicio 3 — VLA vs malloc

```c
// Escribir dos versiones de una función que crea un array
// de n ints y los llena con valores del 1 al n:
// 1. Con VLA
// 2. Con malloc
// Medir el tiempo de ambas con n = 1000 y n = 100000.
// ¿Cuál es más rápida? ¿Cuál es más segura?
```
