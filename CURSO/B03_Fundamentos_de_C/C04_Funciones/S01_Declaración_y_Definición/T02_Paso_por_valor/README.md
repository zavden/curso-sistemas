# T02 — Paso por valor

## Todo en C se pasa por valor

C solo tiene un mecanismo de paso de argumentos: **por valor**.
La función recibe una **copia** del argumento. Modificar la
copia no afecta al original:

```c
#include <stdio.h>

void try_to_modify(int x) {
    x = 100;                     // modifica la COPIA
    printf("Inside: x = %d\n", x);
}

int main(void) {
    int a = 42;
    try_to_modify(a);
    printf("Outside: a = %d\n", a);
    return 0;
}

// Inside: x = 100
// Outside: a = 42     ← a no cambió
```

```c
// Esto aplica a TODOS los tipos:

// Enteros: se copia el valor
void foo(int x) { x = 100; }       // no afecta al original

// Floats: se copia el valor
void bar(double d) { d = 3.14; }   // no afecta al original

// Structs: se copia TODO el struct
void baz(struct Point p) { p.x = 100; }  // no afecta al original

// Punteros: se copia LA DIRECCIÓN (no el dato)
void qux(int *p) { p = NULL; }     // no afecta al puntero original
                                     // (pero *p sí afecta al dato)
```

## Simular paso por referencia con punteros

Para que una función modifique una variable del caller,
se pasa un **puntero** a esa variable:

```c
#include <stdio.h>

void modify(int *p) {
    *p = 100;                    // modifica el dato apuntado
}

int main(void) {
    int a = 42;
    modify(&a);                  // pasa la DIRECCIÓN de a
    printf("a = %d\n", a);      // a = 100 — SÍ cambió
    return 0;
}
```

```c
// ¿Qué pasó?
// 1. main tiene a = 42, en dirección 0x7fff1234
// 2. modify(&a) pasa el VALOR 0x7fff1234 al parámetro p
// 3. p es una copia de la dirección (paso por valor del puntero)
// 4. *p = 100 escribe 100 en la dirección 0x7fff1234
// 5. a está en 0x7fff1234, así que a ahora vale 100

// El puntero se pasó por valor.
// Pero a través del puntero, accedemos al dato original.
```

### Swap — El ejemplo clásico

```c
// INCORRECTO — paso por valor:
void swap_wrong(int a, int b) {
    int tmp = a;
    a = b;
    b = tmp;
    // intercambia las COPIAS — los originales no cambian
}

// CORRECTO — punteros:
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(void) {
    int x = 5, y = 10;

    swap_wrong(x, y);
    printf("x=%d y=%d\n", x, y);   // x=5 y=10 (no cambió)

    swap(&x, &y);
    printf("x=%d y=%d\n", x, y);   // x=10 y=5 (intercambiados)
    return 0;
}
```

## Parámetros de salida (output parameters)

Los punteros permiten que una función **retorne** múltiples
valores:

```c
// Una función solo puede retornar UN valor.
// Para "retornar" más, usar punteros como parámetros de salida:

void divide(int dividend, int divisor, int *quotient, int *remainder) {
    *quotient = dividend / divisor;
    *remainder = dividend % divisor;
}

int main(void) {
    int q, r;
    divide(17, 5, &q, &r);
    printf("17 / 5 = %d remainder %d\n", q, r);  // 3 remainder 2
    return 0;
}
```

```c
// Patrón común: retornar error code, valor por puntero:
int parse_int(const char *str, int *result) {
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') {
        return -1;    // error: no todo el string era un número
    }
    *result = (int)val;
    return 0;          // éxito
}

int main(void) {
    int value;
    if (parse_int("42", &value) == 0) {
        printf("Parsed: %d\n", value);
    }
    return 0;
}
```

## Arrays como argumentos

Los arrays en C **decaen** a punteros cuando se pasan a
funciones. No se copia el array:

```c
void print_array(int arr[], int n) {
    // arr es un PUNTERO, no un array
    printf("sizeof(arr) = %zu\n", sizeof(arr));  // 8 (tamaño del puntero)
    // No se puede saber el tamaño del array dentro de la función
}

// Estas tres declaraciones son EQUIVALENTES:
void foo(int arr[]);        // parece array, pero es puntero
void foo(int arr[10]);      // el 10 se IGNORA — sigue siendo puntero
void foo(int *arr);         // lo que realmente es

// Por eso siempre se pasa el tamaño por separado:
void process(int *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        arr[i] *= 2;    // modifica el array ORIGINAL
    }
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    process(data, 5);
    // data = {2, 4, 6, 8, 10} — fue modificado
    return 0;
}
```

```c
// Los arrays se pasan "por referencia" automáticamente
// porque decaen a puntero. La función puede modificar
// el array original.

// Para evitar modificación, usar const:
int sum(const int *arr, size_t n) {
    int total = 0;
    for (size_t i = 0; i < n; i++) {
        total += arr[i];
        // arr[i] = 0;     ← ERROR: arr apunta a const int
    }
    return total;
}
```

## Structs como argumentos

A diferencia de arrays, los structs se pasan **por valor**
(se copia todo el struct):

```c
struct Matrix {
    double data[100][100];    // 80,000 bytes
};

// INEFICIENTE — copia 80,000 bytes en cada llamada:
double trace_bad(struct Matrix m) {
    double sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += m.data[i][i];
    }
    return sum;
}

// EFICIENTE — pasa solo un puntero (8 bytes):
double trace_good(const struct Matrix *m) {
    double sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += m->data[i][i];    // -> para acceder via puntero
    }
    return sum;
}
```

```c
// Regla de pulgar:
// - Structs pequeños (hasta ~16-32 bytes): por valor está bien
// - Structs grandes: pasar por puntero (const si no se modifica)

// Struct pequeño — por valor es OK:
struct Point { int x; int y; };
double distance(struct Point a, struct Point b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return sqrt(dx*dx + dy*dy);
}

// Struct grande — pasar por puntero:
struct Config { /* muchos campos */ };
void apply_config(const struct Config *cfg) {
    // usar cfg-> para acceder
}
```

### Retornar structs

```c
// En C, las funciones PUEDEN retornar structs (por valor):
struct Point make_point(int x, int y) {
    struct Point p = { .x = x, .y = y };
    return p;    // se copia el struct al caller
}

struct Point p = make_point(10, 20);

// Para structs pequeños, esto es eficiente.
// El compilador suele optimizar la copia (NRVO).

// Para structs grandes, es mejor pasar un puntero de salida:
void make_config(struct Config *out) {
    out->port = 8080;
    out->timeout = 5000;
    // ...
}
```

## Modificar un puntero del caller

```c
// Para modificar el PUNTERO (no el dato), necesitas
// un puntero A puntero:

// INCORRECTO — modifica la copia del puntero:
void alloc_wrong(int *p) {
    p = malloc(sizeof(int));    // modifica la copia de p
    *p = 42;
}

// CORRECTO — puntero a puntero:
void alloc_correct(int **p) {
    *p = malloc(sizeof(int));   // modifica el puntero original
    **p = 42;
}

int main(void) {
    int *ptr = NULL;

    alloc_wrong(ptr);
    // ptr sigue siendo NULL — la función modificó una copia

    alloc_correct(&ptr);
    // ptr apunta a memoria válida con valor 42

    free(ptr);
    return 0;
}
```

```c
// Uso real: funciones que allocan y retornan memoria:
int read_file(const char *path, char **content, size_t *size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    // determinar tamaño...
    *content = malloc(file_size + 1);
    // leer contenido...
    *size = file_size;

    fclose(f);
    return 0;
}

int main(void) {
    char *data = NULL;
    size_t len = 0;
    if (read_file("test.txt", &data, &len) == 0) {
        printf("Read %zu bytes\n", len);
        free(data);
    }
    return 0;
}
```

---

## Ejercicios

### Ejercicio 1 — Swap genérico

```c
// Implementar swap para int, double y struct Point.
// Verificar que los valores se intercambian.
```

### Ejercicio 2 — Output parameters

```c
// Implementar min_max(const int *arr, int n, int *min, int *max)
// que encuentre el mínimo y máximo de un array.
// Retorna 0 si éxito, -1 si arr es NULL o n <= 0.
```

### Ejercicio 3 — Puntero a puntero

```c
// Implementar string_duplicate(const char *src, char **dst)
// que alloce memoria y copie el string.
// El caller es responsable de hacer free(*dst).
// Retorna 0 si éxito, -1 si error.
```
