# T02 — Paso por valor

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 4 archivos `.c` de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

Sin erratas detectadas en el material fuente.

---

## 1. Todo en C se pasa por valor

C tiene **un solo** mecanismo de paso de argumentos: **por valor**. La función
siempre recibe una **copia** del argumento. Modificar la copia no afecta al original:

```c
void try_to_modify(int x) {
    x = 999;                      // modifica la COPIA local
}

int main(void) {
    int a = 42;
    try_to_modify(a);
    printf("a = %d\n", a);        // a = 42 — no cambió
    return 0;
}
```

`a` y `x` viven en direcciones de memoria distintas. Al llamar a la función,
el valor de `a` se copia a `x`. Son variables independientes en el stack.

Esto aplica a **todos** los tipos:

| Tipo | Qué se copia | La función modifica el original? |
|------|-------------|----------------------------------|
| `int`, `double`, etc. | El valor | No |
| `struct` | Todo el struct (todos los bytes) | No |
| `int *` (puntero) | La dirección (no el dato) | No el puntero, **sí el dato apuntado** |
| `int[]` (array) | Decae a puntero → copia la dirección | **Sí** (vía el puntero) |

---

## 2. Simular paso por referencia con punteros

Para que una función modifique una variable del caller, se pasa un **puntero**
a esa variable:

```c
void modify(int *p) {
    *p = 100;                     // escribe en la dirección apuntada
}

int main(void) {
    int a = 42;
    modify(&a);                   // pasa la DIRECCIÓN de a
    printf("a = %d\n", a);       // a = 100 — SÍ cambió
    return 0;
}
```

¿Qué pasó internamente?
1. `a` vive en dirección `0x7fff1234`, con valor 42.
2. `modify(&a)` pasa el **valor** `0x7fff1234` al parámetro `p` (paso por valor del puntero).
3. `p` es una copia de la dirección — pero apunta al mismo lugar.
4. `*p = 100` escribe 100 en `0x7fff1234`.
5. `a` está en `0x7fff1234`, así que ahora vale 100.

El puntero se pasó por valor. Pero a través del puntero, accedemos al dato original.

### Swap — el ejemplo clásico

```c
// INCORRECTO — intercambia copias locales:
void swap_wrong(int a, int b) {
    int tmp = a;
    a = b;
    b = tmp;
    // a y b son copias — x e y no cambian
}

// CORRECTO — usa punteros para acceder a los originales:
void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(void) {
    int x = 5, y = 10;

    swap_wrong(x, y);
    printf("x=%d y=%d\n", x, y);    // x=5 y=10 (no cambió)

    swap(&x, &y);
    printf("x=%d y=%d\n", x, y);    // x=10 y=5 (intercambiados)
    return 0;
}
```

---

## 3. Parámetros de salida (output parameters)

Una función solo puede retornar **un** valor. Para "retornar" múltiples
valores, se usan punteros como parámetros de salida:

```c
void divide(int dividend, int divisor, int *quotient, int *remainder) {
    *quotient = dividend / divisor;
    *remainder = dividend % divisor;
}

int main(void) {
    int q, r;
    divide(17, 5, &q, &r);
    printf("17 / 5 = %d remainder %d\n", q, r);   // 3 remainder 2
    return 0;
}
```

### Patrón: retornar código de error, valor por puntero

```c
int parse_int(const char *str, int *result) {
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') return -1;    // error: string no era todo número
    *result = (int)val;
    return 0;                       // éxito
}

int main(void) {
    int value;
    if (parse_int("42", &value) == 0) {
        printf("Parsed: %d\n", value);   // Parsed: 42
    }
    return 0;
}
```

Este patrón es ubicuo en C: la función retorna un `int` como status (0 = éxito,
negativo = error), y los resultados salen por punteros.

---

## 4. Arrays como argumentos — decaimiento a puntero

Los arrays son la excepción a "todo se copia": **decaen** a un puntero al
primer elemento al pasarse a funciones. No se copia el array:

```c
void print_sizeof(int arr[], int n) {
    printf("sizeof(arr) = %zu\n", sizeof(arr));   // 8 (tamaño del puntero!)
    (void)n;
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    printf("sizeof(data) = %zu\n", sizeof(data));  // 20 (5 ints × 4 bytes)
    print_sizeof(data, 5);
    return 0;
}
```

GCC incluso emite un warning:
```
warning: 'sizeof' on array function parameter 'arr' will return size of 'int *'
```

### Tres declaraciones equivalentes

En parámetros de función, estas tres son **idénticas** — las tres declaran un puntero:

```c
void foo(int arr[]);       // parece array, pero es int *
void foo(int arr[10]);     // el 10 se IGNORA — sigue siendo int *
void foo(int *arr);        // lo que realmente es
```

Por eso siempre se pasa el tamaño como parámetro separado:

```c
void process(int *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        arr[i] *= 2;       // modifica el array ORIGINAL
    }
}
```

### `const` para proteger contra modificación

```c
// Sin const: la función puede modificar el array
void modify(int *arr, int n) { arr[0] = 999; }

// Con const: el compilador impide modificar
int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
        // arr[i] = 0;     // ERROR: arr apunta a const int
    }
    return total;
}
```

`const` en el prototipo comunica la **intención**: "esta función solo lee el array,
no lo modifica".

---

## 5. Structs como argumentos

A diferencia de los arrays, los structs **sí se copian completos** al pasar por valor:

```c
struct SmallPoint { int x; int y; };          // 8 bytes
struct LargeBuffer { char data[4096]; int length; };  // ~4100 bytes

// Por valor — copia TODO el struct:
void modify_value(struct SmallPoint p) {
    p.x = 999;     // modifica la copia — original no cambia
}

// Por puntero — solo copia la dirección (8 bytes):
void modify_ptr(struct SmallPoint *p) {
    p->x = 999;    // modifica el original a través del puntero
}
```

### sizeof dentro de la función

```c
void by_value(struct LargeBuffer buf) {
    printf("sizeof(buf) = %zu\n", sizeof(buf));     // ~4100 (copia completa)
}

void by_ptr(const struct LargeBuffer *buf) {
    printf("sizeof(buf) = %zu\n", sizeof(buf));     // 8 (puntero)
    printf("sizeof(*buf) = %zu\n", sizeof(*buf));   // ~4100 (struct completo)
}
```

### Regla práctica

| Tamaño del struct | Pasar por... | Razón |
|-------------------|-------------|-------|
| Pequeño (~8-32 bytes) | Valor | La copia es barata, código más simple |
| Grande (>32 bytes) | Puntero (`const` si solo lectura) | Evita copias costosas |

```c
// Pequeño — por valor está bien:
double distance(struct Point a, struct Point b) { ... }

// Grande — pasar por puntero:
void apply_config(const struct Config *cfg) { ... }
```

### Retornar structs

```c
// C permite retornar structs por valor:
struct Point make_point(int x, int y) {
    struct Point p = { .x = x, .y = y };
    return p;    // se copia al caller (el compilador suele optimizar con NRVO)
}

struct Point p = make_point(10, 20);

// Para structs grandes, usar puntero de salida:
void make_config(struct Config *out) {
    out->port = 8080;
    out->timeout = 5000;
}
```

---

## 6. Modificar un puntero del caller — puntero a puntero

Si necesitas que una función modifique el **puntero mismo** (no el dato apuntado),
necesitas un puntero **a** puntero:

```c
// INCORRECTO — modifica la copia del puntero:
void alloc_wrong(int *p) {
    p = malloc(sizeof(int));    // p local apunta a nueva memoria
    *p = 42;                     // escribe en la nueva memoria
}   // p se destruye → la nueva memoria se pierde (memory leak)

// CORRECTO — puntero a puntero:
void alloc_correct(int **p) {
    *p = malloc(sizeof(int));   // modifica el puntero original
    **p = 42;                    // escribe en la nueva memoria
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

### Uso real: funciones que allocan memoria

```c
int read_file(const char *path, char **content, size_t *size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    // ...
    *content = malloc(file_size + 1);
    *size = file_size;
    fclose(f);
    return 0;
}

int main(void) {
    char *data = NULL;
    size_t len = 0;
    if (read_file("test.txt", &data, &len) == 0) {
        printf("Read %zu bytes\n", len);
        free(data);    // caller es responsable del free
    }
    return 0;
}
```

Resumen de niveles de indirección:

| Quiero modificar... | Pasar... | Ejemplo |
|---------------------|----------|---------|
| Un `int` | `int *` | `void set(int *x) { *x = 42; }` |
| Un `int *` (puntero) | `int **` | `void alloc(int **p) { *p = malloc(...); }` |
| Un `int **` | `int ***` | Raro, pero el mismo principio |

---

## Ejercicios

### Ejercicio 1 — Paso por valor: predecir la salida

```c
#include <stdio.h>

void increment(int x) {
    x++;
    printf("  Inside: x = %d\n", x);
}

int main(void) {
    int a = 10;
    increment(a);
    increment(a);
    increment(a);
    printf("a = %d\n", a);
    return 0;
}
```

<details><summary>Predicción</summary>

```
  Inside: x = 11
  Inside: x = 11
  Inside: x = 11
a = 10
```

Cada llamada recibe una copia de `a` (que siempre vale 10), la incrementa a 11,
y la descarta al retornar. `a` nunca cambia. Las tres llamadas son independientes
porque cada una trabaja con su propia copia.

</details>

---

### Ejercicio 2 — Swap correcto

```c
#include <stdio.h>

void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int main(void) {
    int x = 100, y = 200;
    printf("Before: x=%d, y=%d\n", x, y);
    swap(&x, &y);
    printf("After:  x=%d, y=%d\n", x, y);

    // ¿Qué pasa si hacemos swap dos veces?
    swap(&x, &y);
    printf("Again:  x=%d, y=%d\n", x, y);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Before: x=100, y=200
After:  x=200, y=100
Again:  x=100, y=200
```

Primer swap: intercambia 100↔200. Segundo swap: intercambia 200↔100 (vuelve al
estado original). Swap es su propia inversa.

</details>

---

### Ejercicio 3 — Output parameters: min y max

```c
#include <stdio.h>

void min_max(const int *arr, int n, int *out_min, int *out_max) {
    *out_min = arr[0];
    *out_max = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] < *out_min) *out_min = arr[i];
        if (arr[i] > *out_max) *out_max = arr[i];
    }
}

int main(void) {
    int data[] = {3, 7, 1, 9, 4, 2, 8};
    int n = sizeof(data) / sizeof(data[0]);
    int lo, hi;

    min_max(data, n, &lo, &hi);
    printf("min=%d, max=%d\n", lo, hi);
    return 0;
}
```

<details><summary>Predicción</summary>

```
min=1, max=9
```

La función recorre el array una sola vez. `out_min` y `out_max` son punteros
a `lo` y `hi` en `main`. Al escribir `*out_min` y `*out_max`, modifica
directamente las variables del caller.

- Inicio: min=3, max=3.
- 7: max→7. 1: min→1. 9: max→9. 4: sin cambio. 2: sin cambio. 8: sin cambio.
- Resultado: min=1, max=9.

La función retorna `void` y "devuelve" dos valores a través de punteros de salida.

</details>

---

### Ejercicio 4 — Array decay: sizeof

```c
#include <stdio.h>

void show(int arr[], int n) {
    printf("  sizeof(arr) in function: %zu\n", sizeof(arr));
    printf("  n = %d\n", n);
}

int main(void) {
    int a[5] = {10, 20, 30, 40, 50};
    int b[100] = {0};

    printf("sizeof(a) in main: %zu\n", sizeof(a));
    show(a, 5);

    printf("sizeof(b) in main: %zu\n", sizeof(b));
    show(b, 100);

    return 0;
}
```

<details><summary>Predicción</summary>

```
sizeof(a) in main: 20
  sizeof(arr) in function: 8
  n = 5
sizeof(b) in main: 400
  sizeof(arr) in function: 8
  n = 100
```

- En `main`, `sizeof(a)` = 5 × 4 = 20 bytes. `sizeof(b)` = 100 × 4 = 400 bytes.
- Dentro de `show`, `sizeof(arr)` = 8 en ambos casos (tamaño del puntero en x86-64).
  No importa si el array original tiene 5 o 100 elementos — `arr` es un `int *`.

Por eso el tamaño se pasa siempre como parámetro separado. GCC emitirá un warning:
```
warning: 'sizeof' on array function parameter 'arr' will return size of 'int *'
```

</details>

---

### Ejercicio 5 — La función modifica el array original

```c
#include <stdio.h>

void zero_negatives(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        if (arr[i] < 0) arr[i] = 0;
    }
}

int main(void) {
    int data[] = {5, -3, 8, -1, 0, -7, 2};
    int n = sizeof(data) / sizeof(data[0]);

    printf("Before: ");
    for (int i = 0; i < n; i++) printf("%d ", data[i]);
    printf("\n");

    zero_negatives(data, n);

    printf("After:  ");
    for (int i = 0; i < n; i++) printf("%d ", data[i]);
    printf("\n");

    return 0;
}
```

<details><summary>Predicción</summary>

```
Before: 5 -3 8 -1 0 -7 2
After:  5 0 8 0 0 0 2
```

`data` decae a un puntero al pasarse a `zero_negatives`. La función accede al
array original a través del puntero y reemplaza los valores negativos (-3, -1, -7)
con 0. El 0 original (posición 4) no cambia — no es negativo.

No se hizo copia del array. Los cambios son visibles en `main` inmediatamente.

</details>

---

### Ejercicio 6 — Struct por valor no modifica

```c
#include <stdio.h>

struct Point { int x; int y; };

struct Point move_wrong(struct Point p, int dx, int dy) {
    p.x += dx;
    p.y += dy;
    return p;
}

void move_right(struct Point *p, int dx, int dy) {
    p->x += dx;
    p->y += dy;
}

int main(void) {
    struct Point a = {10, 20};

    // Versión 1: retornar struct (por valor)
    struct Point b = move_wrong(a, 5, 3);
    printf("a = (%d, %d)\n", a.x, a.y);
    printf("b = (%d, %d)\n", b.x, b.y);

    // Versión 2: modificar vía puntero
    move_right(&a, 5, 3);
    printf("a = (%d, %d)\n", a.x, a.y);

    return 0;
}
```

<details><summary>Predicción</summary>

```
a = (10, 20)
b = (15, 23)
a = (15, 23)
```

- `move_wrong` recibe una copia de `a`, la modifica, y retorna la copia modificada.
  `a` no cambia (sigue en 10,20). `b` recibe el resultado (15,23).
- `move_right` recibe un puntero a `a` y la modifica directamente. Ahora `a` = (15,23).

Nota: `move_wrong` no es realmente "incorrecto" — retornar un struct modificado
es un patrón válido (estilo funcional). Lo "incorrecto" sería esperar que `a` cambiara
sin capturar el retorno.

</details>

---

### Ejercicio 7 — Puntero a puntero

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int string_dup(const char *src, char **dst) {
    size_t len = strlen(src);
    *dst = malloc(len + 1);
    if (*dst == NULL) return -1;
    memcpy(*dst, src, len + 1);
    return 0;
}

int main(void) {
    char *copy = NULL;

    if (string_dup("Hello, World!", &copy) == 0) {
        printf("copy = \"%s\"\n", copy);
        printf("len  = %zu\n", strlen(copy));
        free(copy);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
copy = "Hello, World!"
len  = 13
```

- `copy` empieza como `NULL`.
- `string_dup` recibe `&copy` (un `char **`).
- `*dst = malloc(14)` modifica el puntero original `copy` en `main`.
- `memcpy` copia los 13 caracteres + el `'\0'` terminador.
- De vuelta en `main`, `copy` apunta a la memoria allocada con la copia del string.
- `free(copy)` libera la memoria (el caller es responsable).

Si hubiéramos usado `char *dst` (sin el `**`), malloc habría asignado a una copia local
del puntero y `copy` en `main` seguiría siendo `NULL`.

</details>

---

### Ejercicio 8 — const protege el array

```c
#include <stdio.h>

int sum(const int *arr, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];
    }
    // arr[0] = 999;    // descomentar esta línea
    return total;
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};
    printf("sum = %d\n", sum(data, 5));
    return 0;
}
```

<details><summary>Predicción</summary>

Con la línea comentada:
```
sum = 15
```

Si se descomenta `arr[0] = 999;`, el compilador da error:
```
error: assignment of read-only location '*arr'
```

`const int *arr` significa "puntero a int constante" — se puede leer `arr[i]`
pero no escribir. Esto no afecta al array en `main` (que no es `const`), solo
restringe lo que la función puede hacer con el puntero.

`const` en parámetros es un **contrato**: la función promete no modificar los datos.
Si el parámetro no es `const`, la función **podría** modificar el array original.

</details>

---

### Ejercicio 9 — Contraste: array vs struct

```c
#include <stdio.h>

struct Pair { int a; int b; };

void modify_array(int arr[]) {
    arr[0] = 999;
}

void modify_struct(struct Pair p) {
    p.a = 999;
}

int main(void) {
    int arr[] = {1, 2};
    struct Pair pair = {1, 2};

    modify_array(arr);
    modify_struct(pair);

    printf("arr[0] = %d\n", arr[0]);
    printf("pair.a = %d\n", pair.a);
    return 0;
}
```

<details><summary>Predicción</summary>

```
arr[0] = 999
pair.a = 1
```

- `modify_array(arr)`: el array decae a puntero → la función modifica el original.
  `arr[0]` cambió a 999.
- `modify_struct(pair)`: el struct se copia completo → la función modifica la copia.
  `pair.a` sigue siendo 1.

Esta es la asimetría fundamental de C:
- **Arrays**: se pasan como puntero (automáticamente) → la función puede modificar el original.
- **Structs**: se copian completos → la función no puede modificar el original.

Para que `modify_struct` modifique el original, necesita `struct Pair *p` y usar `p->a = 999`.

</details>

---

### Ejercicio 10 — Divide con output parameters

```c
#include <stdio.h>

int safe_divide(int a, int b, int *quotient, int *remainder) {
    if (b == 0) return -1;      // error: división por cero
    *quotient = a / b;
    *remainder = a % b;
    return 0;                    // éxito
}

int main(void) {
    int q, r;

    if (safe_divide(17, 5, &q, &r) == 0) {
        printf("17 / 5 = %d remainder %d\n", q, r);
    }

    if (safe_divide(100, 7, &q, &r) == 0) {
        printf("100 / 7 = %d remainder %d\n", q, r);
    }

    if (safe_divide(10, 0, &q, &r) != 0) {
        printf("Division by zero: error\n");
    }

    if (safe_divide(-17, 5, &q, &r) == 0) {
        printf("-17 / 5 = %d remainder %d\n", q, r);
    }

    return 0;
}
```

<details><summary>Predicción</summary>

```
17 / 5 = 3 remainder 2
100 / 7 = 14 remainder 2
Division by zero: error
-17 / 5 = -3 remainder -2
```

- 17 / 5 = 3, 17 % 5 = 2. Verificación: 3×5 + 2 = 17 ✓
- 100 / 7 = 14, 100 % 7 = 2. Verificación: 14×7 + 2 = 100 ✓
- División por 0: la función retorna -1 sin escribir en `q` ni `r`.
- -17 / 5: C99+ trunca hacia cero → -3. Módulo: -17 - (-3×5) = -17 + 15 = -2.
  El signo del módulo sigue al dividendo (C99+).

El patrón: retornar `int` como código de error (0 = éxito, -1 = error), y los
resultados reales salen por punteros de salida. Este es el estilo idiomático de C.

</details>
