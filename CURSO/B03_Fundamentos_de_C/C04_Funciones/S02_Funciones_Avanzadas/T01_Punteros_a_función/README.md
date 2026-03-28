# T01 — Punteros a función

## Qué es un puntero a función

En C, las funciones tienen una dirección en memoria. Un puntero
a función almacena esa dirección y permite llamar a la función
indirectamente:

```c
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    // Declarar un puntero a función:
    int (*op)(int, int);
    //  ↑     ↑     ↑
    //  |     |     tipo de los parámetros
    //  |     nombre del puntero
    //  tipo de retorno

    op = add;                    // op apunta a add
    printf("%d\n", op(3, 4));    // 7 — llama a add(3, 4)

    op = sub;                    // op ahora apunta a sub
    printf("%d\n", op(3, 4));    // -1 — llama a sub(3, 4)

    return 0;
}
```

### Sintaxis

```c
// La sintaxis es confusa. Los paréntesis alrededor de (*name) son
// necesarios por precedencia:

int (*fp)(int, int);     // puntero a función que toma 2 ints, retorna int
int *fp(int, int);       // función que toma 2 ints, retorna int* (¡diferente!)

// Leer de adentro hacia afuera:
// (*fp)        → fp es un puntero
// (*fp)(int,int) → a una función con dos ints
// int (*fp)... → que retorna int
```

```c
// El nombre de una función ES su dirección:
int add(int a, int b) { return a + b; }

int (*fp)(int, int) = add;     // sin &
int (*fp)(int, int) = &add;    // con & — equivalente

// Para llamar:
fp(3, 4);         // llamada directa (lo normal)
(*fp)(3, 4);      // desreferencia explícita — equivalente
```

## typedef para simplificar

```c
// Los punteros a función son verbosos. typedef ayuda:

typedef int (*BinaryOp)(int, int);
// BinaryOp es ahora un tipo: "puntero a función (int,int)→int"

BinaryOp op = add;
printf("%d\n", op(3, 4));

// Sin typedef:
int (*op)(int, int) = add;

// Con typedef es mucho más legible, especialmente en
// parámetros de función y arrays.
```

## Callbacks

El uso más importante de punteros a función: pasar
comportamiento como argumento:

```c
#include <stdio.h>

// Función que aplica una operación a cada elemento:
void apply(int *arr, int n, int (*transform)(int)) {
    for (int i = 0; i < n; i++) {
        arr[i] = transform(arr[i]);
    }
}

int double_it(int x) { return x * 2; }
int negate(int x)     { return -x; }
int square(int x)     { return x * x; }

int main(void) {
    int data[] = {1, 2, 3, 4, 5};

    apply(data, 5, double_it);   // data = {2, 4, 6, 8, 10}
    apply(data, 5, negate);      // data = {-2, -4, -6, -8, -10}

    return 0;
}
```

### qsort — Callback en la biblioteca estándar

```c
#include <stdlib.h>
#include <stdio.h>

// qsort ordena cualquier tipo de array.
// El caller provee la función de comparación:

int compare_ints(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);    // -1, 0, o 1
}

int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int main(void) {
    int nums[] = {5, 2, 8, 1, 9, 3};
    qsort(nums, 6, sizeof(int), compare_ints);
    // nums = {1, 2, 3, 5, 8, 9}

    const char *words[] = {"banana", "apple", "cherry"};
    qsort(words, 3, sizeof(char *), compare_strings);
    // words = {"apple", "banana", "cherry"}

    return 0;
}
```

```c
// Firma de qsort:
void qsort(void *base,         // array a ordenar
            size_t nmemb,       // número de elementos
            size_t size,        // tamaño de cada elemento
            int (*compar)(const void *, const void *));
// El último parámetro es un puntero a función de comparación.
// Retorna negativo si a<b, 0 si a==b, positivo si a>b.
```

## Tablas de despacho (dispatch tables)

Un array de punteros a función permite seleccionar comportamiento
por índice:

```c
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divi(int a, int b) { return b != 0 ? a / b : 0; }

int main(void) {
    // Array de punteros a función:
    typedef int (*Operation)(int, int);
    Operation ops[] = { add, sub, mul, divi };
    const char *names[] = { "add", "sub", "mul", "div" };

    int a = 10, b = 3;
    for (int i = 0; i < 4; i++) {
        printf("%s(%d, %d) = %d\n", names[i], a, b, ops[i](a, b));
    }

    return 0;
}
```

```c
// Dispatch table con enum:
enum Command { CMD_START, CMD_STOP, CMD_RESET, CMD_COUNT };

typedef void (*CommandHandler)(void);

void handle_start(void) { printf("Starting\n"); }
void handle_stop(void)  { printf("Stopping\n"); }
void handle_reset(void) { printf("Resetting\n"); }

CommandHandler handlers[CMD_COUNT] = {
    [CMD_START] = handle_start,
    [CMD_STOP]  = handle_stop,
    [CMD_RESET] = handle_reset,
};

void dispatch(enum Command cmd) {
    if (cmd >= 0 && cmd < CMD_COUNT && handlers[cmd]) {
        handlers[cmd]();    // llama al handler correspondiente
    }
}
```

```c
// Esto reemplaza un switch:
// En lugar de:
switch (cmd) {
    case CMD_START: handle_start(); break;
    case CMD_STOP:  handle_stop();  break;
    case CMD_RESET: handle_reset(); break;
}

// Dispatch table:
handlers[cmd]();

// La dispatch table es O(1) y extensible.
// Útil en intérpretes, máquinas de estado, protocolos.
```

## Puntero a función como campo de struct

```c
// Simular "métodos" en C:
struct Shape {
    double (*area)(const struct Shape *self);
    double (*perimeter)(const struct Shape *self);
    double width;
    double height;
};

double rect_area(const struct Shape *s) {
    return s->width * s->height;
}

double rect_perimeter(const struct Shape *s) {
    return 2 * (s->width + s->height);
}

int main(void) {
    struct Shape rect = {
        .area = rect_area,
        .perimeter = rect_perimeter,
        .width = 5.0,
        .height = 3.0,
    };

    printf("Area: %f\n", rect.area(&rect));           // 15.0
    printf("Perimeter: %f\n", rect.perimeter(&rect));  // 16.0

    return 0;
}
```

## Puntero a función como retorno

```c
// Una función puede retornar un puntero a función:

typedef int (*Operation)(int, int);

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

Operation get_operation(char op) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        default:  return NULL;
    }
}

int main(void) {
    Operation op = get_operation('+');
    if (op) {
        printf("%d\n", op(10, 3));   // 13
    }
    return 0;
}

// Sin typedef, la declaración sería:
// int (*get_operation(char op))(int, int) { ... }
// Ilegible. Siempre usar typedef.
```

## NULL como puntero a función

```c
// Un puntero a función puede ser NULL:
typedef void (*Callback)(int);

void process(int x, Callback on_complete) {
    // hacer algo...
    if (on_complete) {         // verificar antes de llamar
        on_complete(x);
    }
}

process(42, my_handler);    // llama a my_handler(42)
process(42, NULL);           // no llama a nada
```

---

## Ejercicios

### Ejercicio 1 — Callbacks

```c
// Implementar filter(int *arr, int n, int (*predicate)(int))
// que cuente cuántos elementos cumplen el predicado.
// Probar con is_even, is_positive, is_greater_than_10.
```

### Ejercicio 2 — Calculadora con dispatch table

```c
// Crear una calculadora que lea un operador (+,-,*,/)
// y use una dispatch table (array de punteros a función)
// para seleccionar la operación.
```

### Ejercicio 3 — qsort

```c
// Crear un array de structs { char name[50]; int age; }.
// Ordenar:
// 1. Por nombre (strcmp)
// 2. Por edad (numérico)
// Usar qsort con funciones de comparación diferentes.
```
