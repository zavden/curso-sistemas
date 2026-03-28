# T01 — Punteros a función

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 4 archivos `.c` de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

Sin erratas detectadas en el material fuente.

---

## 1. Qué es un puntero a función

Las funciones en C tienen una dirección en memoria. Un puntero a función almacena
esa dirección y permite llamar a la función **indirectamente**:

```c
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    int (*op)(int, int);         // declarar puntero a función

    op = add;                    // apunta a add
    printf("%d\n", op(3, 4));    // 7 — llama a add(3, 4)

    op = sub;                    // reasignar a sub
    printf("%d\n", op(3, 4));    // -1 — llama a sub(3, 4)
    return 0;
}
```

### Sintaxis: los paréntesis son obligatorios

```c
int (*fp)(int, int);     // puntero a función: (int, int) → int
int *fp(int, int);       // función que retorna int* — ¡diferente!
```

Los paréntesis alrededor de `(*fp)` son necesarios por precedencia. Sin ellos,
`fp` se interpreta como una función, no como un puntero.

Lectura de adentro hacia afuera:
```
(*fp)            → fp es un puntero
(*fp)(int, int)  → a una función que toma dos ints
int (*fp)(...)   → que retorna int
```

### Asignación y llamada: equivalencias

```c
int (*fp)(int, int) = add;     // sin & — el nombre ES la dirección
int (*fp)(int, int) = &add;    // con & — equivalente

fp(3, 4);         // llamada directa (forma normal)
(*fp)(3, 4);      // desreferencia explícita — equivalente
```

Ambas formas de asignar y ambas formas de llamar son equivalentes. La forma
sin `&` y sin `*` es la más común.

---

## 2. `typedef` para simplificar

La sintaxis de punteros a función es verbosa. `typedef` crea un alias de tipo:

```c
typedef int (*BinaryOp)(int, int);
// BinaryOp es ahora un tipo: "puntero a función (int, int) → int"

BinaryOp op = add;               // más legible
printf("%d\n", op(3, 4));

// Sin typedef:
int (*op)(int, int) = add;       // verboso

// typedef es especialmente útil en parámetros:
void compute(BinaryOp operation, int x, int y);
// vs:
void compute(int (*operation)(int, int), int x, int y);
```

---

## 3. Callbacks — funciones como argumento

El uso más importante de punteros a función: pasar **comportamiento** como
argumento. La función receptora no sabe qué operación va a aplicar — eso lo
decide el caller:

```c
// apply: transforma cada elemento del array in-place
void apply(int *arr, int n, int (*transform)(int)) {
    for (int i = 0; i < n; i++) {
        arr[i] = transform(arr[i]);
    }
}

int double_it(int x) { return x * 2; }
int negate(int x)    { return -x; }
int square(int x)    { return x * x; }

int data[] = {1, 2, 3, 4, 5};
apply(data, 5, double_it);   // data = {2, 4, 6, 8, 10}
apply(data, 5, negate);      // data = {-2, -4, -6, -8, -10}
```

`apply` es genérica: no conoce la transformación. El tercer argumento es un
callback — una función que `apply` "llama de vuelta" para cada elemento.

### Predicados como callbacks

```c
// filter_count: cuenta cuántos elementos cumplen un predicado
int filter_count(const int *arr, int n, int (*predicate)(int)) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (predicate(arr[i])) count++;
    }
    return count;
}

int is_even(int x)     { return x % 2 == 0; }
int is_positive(int x) { return x > 0; }

int data[] = {-3, 0, 7, -1, 4, 12, -8, 2};
filter_count(data, 8, is_even);      // 5 (0, 4, 12, -8, 2)
filter_count(data, 8, is_positive);  // 4 (7, 4, 12, 2)
```

---

## 4. `qsort` — callback en la biblioteca estándar

`qsort` ordena cualquier tipo de array. El caller provee la función de comparación:

```c
void qsort(void *base,          // array a ordenar
            size_t nmemb,        // número de elementos
            size_t size,         // tamaño de cada elemento
            int (*compar)(const void *, const void *));
```

La función comparadora debe retornar:
- **Negativo** si `a < b`
- **Cero** si `a == b`
- **Positivo** si `a > b`

### Comparar enteros

```c
int compare_ascending(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);    // produce -1, 0, o 1
}

int nums[] = {42, 7, 13, 99, 1, 55, 23};
qsort(nums, 7, sizeof(int), compare_ascending);
// nums = {1, 7, 13, 23, 42, 55, 99}
```

**Importante**: la expresión `(a > b) - (a < b)` es segura. El patrón `return a - b`
puede causar **overflow** con enteros grandes (ej. `INT_MAX - (-1)` desborda).

Para orden descendente, invertir la comparación:
```c
int compare_descending(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ib > ia) - (ib < ia);    // invertido
}
```

### Comparar strings

```c
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

const char *words[] = {"banana", "apple", "cherry"};
qsort(words, 3, sizeof(char *), compare_strings);
// words = {"apple", "banana", "cherry"}
```

¿Por qué `*(const char **)a`? El array es de `const char *` (punteros a char).
`qsort` pasa la **dirección** de cada elemento, así que `a` es un `char **`
(puntero a puntero). Se desreferencia una vez para obtener el `char *` que
`strcmp` necesita.

---

## 5. Dispatch tables — array de punteros a función

Un array de punteros a función permite seleccionar comportamiento por **índice**,
reemplazando un `switch`:

```c
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divi(int a, int b) { return b != 0 ? a / b : 0; }

typedef int (*Operation)(int, int);

Operation ops[] = { add, sub, mul, divi };
const char *symbols[] = { "+", "-", "*", "/" };

// Llamar por índice:
int result = ops[2](20, 6);    // mul(20, 6) = 120
```

### Dispatch table vs switch

```c
// Con switch:
switch (choice) {
    case 0: result = add(a, b); break;
    case 1: result = sub(a, b); break;
    case 2: result = mul(a, b); break;
    case 3: result = divi(a, b); break;
}

// Con dispatch table:
result = ops[choice](a, b);    // una sola línea
```

Ventajas de la dispatch table:
- **O(1)**: acceso directo por índice.
- **Extensible**: agregar una operación = agregar un elemento al array.
- **Menos código**: una línea en lugar de múltiples cases.

### Dispatch table con enum

```c
enum Command { CMD_START, CMD_STOP, CMD_RESET, CMD_COUNT };

typedef void (*Handler)(void);

void handle_start(void) { printf("Starting\n"); }
void handle_stop(void)  { printf("Stopping\n"); }
void handle_reset(void) { printf("Resetting\n"); }

Handler handlers[CMD_COUNT] = {
    [CMD_START] = handle_start,
    [CMD_STOP]  = handle_stop,
    [CMD_RESET] = handle_reset,
};

void dispatch(enum Command cmd) {
    if (cmd >= 0 && cmd < CMD_COUNT && handlers[cmd]) {
        handlers[cmd]();
    }
}
```

`CMD_COUNT` al final del enum vale 3 (la cantidad de comandos) y sirve como
tamaño del array. Los designated initializers (`[CMD_START] =`) aseguran la
correspondencia correcta.

---

## 6. Puntero a función como campo de struct

Permite simular "métodos" en C — polimorfismo manual:

```c
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

struct Shape rect = {
    .area = rect_area,
    .perimeter = rect_perimeter,
    .width = 5.0,
    .height = 3.0,
};

printf("Area: %.1f\n", rect.area(&rect));           // 15.0
printf("Perimeter: %.1f\n", rect.perimeter(&rect));  // 16.0
```

El parámetro `self` es el equivalente manual del `this` de C++ o del `self` de
Rust/Python. El caller debe pasarlo explícitamente.

---

## 7. Retornar punteros a función

Una función puede retornar un puntero a función. Con `typedef` es legible:

```c
typedef int (*Operation)(int, int);

Operation get_operation(char op) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        default:  return NULL;
    }
}

Operation op = get_operation('+');
if (op) printf("%d\n", op(10, 3));    // 13
```

Sin `typedef`, la declaración sería:
```c
int (*get_operation(char op))(int, int) { ... }
// Ilegible. Siempre usar typedef.
```

### NULL como puntero a función

Un puntero a función puede ser NULL. Siempre verificar antes de llamar:

```c
typedef void (*Callback)(int);

void process(int x, Callback on_complete) {
    // ... hacer algo ...
    if (on_complete) {         // verificar != NULL
        on_complete(x);
    }
}

process(42, my_handler);    // llama a my_handler(42)
process(42, NULL);           // no llama a nada (callback opcional)
```

---

## Ejercicios

### Ejercicio 1 — Puntero básico: reasignar

```c
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

int main(void) {
    int (*op)(int, int);

    op = add;
    printf("op(10, 3) = %d\n", op(10, 3));

    op = sub;
    printf("op(10, 3) = %d\n", op(10, 3));

    op = mul;
    printf("op(10, 3) = %d\n", op(10, 3));

    return 0;
}
```

<details><summary>Predicción</summary>

```
op(10, 3) = 13
op(10, 3) = 7
op(10, 3) = 30
```

`op` apunta a tres funciones distintas en secuencia. Cada llamada `op(10, 3)`
ejecuta la función a la que `op` apunta en ese momento:
- `add(10, 3)` = 13.
- `sub(10, 3)` = 7.
- `mul(10, 3)` = 30.

El puntero se reasigna sin problema porque las tres funciones tienen la misma
firma: `int (int, int)`.

</details>

---

### Ejercicio 2 — Callback: apply

```c
#include <stdio.h>

void apply(int *arr, int n, int (*transform)(int)) {
    for (int i = 0; i < n; i++) {
        arr[i] = transform(arr[i]);
    }
}

int double_it(int x) { return x * 2; }
int square(int x)    { return x * x; }
int negate(int x)    { return -x; }

void print_array(const int *arr, int n) {
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
}

int main(void) {
    int data[] = {1, 2, 3, 4, 5};

    apply(data, 5, double_it);
    printf("After double: ");
    print_array(data, 5);

    apply(data, 5, square);
    printf("After square: ");
    print_array(data, 5);

    apply(data, 5, negate);
    printf("After negate: ");
    print_array(data, 5);

    return 0;
}
```

<details><summary>Predicción</summary>

```
After double: 2 4 6 8 10
After square: 4 16 36 64 100
After negate: -4 -16 -36 -64 -100
```

Las transformaciones se **acumulan** porque `apply` modifica el array in-place:
- `double_it`: {1,2,3,4,5} → {2,4,6,8,10}.
- `square` sobre el array ya duplicado: {2,4,6,8,10} → {4,16,36,64,100}.
- `negate` sobre el cuadrado: {4,16,36,64,100} → {-4,-16,-36,-64,-100}.

Si se quisieran transformaciones independientes, habría que copiar el array
antes de cada `apply`.

</details>

---

### Ejercicio 3 — filter_count con predicados

```c
#include <stdio.h>

int filter_count(const int *arr, int n, int (*pred)(int)) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (pred(arr[i])) count++;
    }
    return count;
}

int is_even(int x)     { return x % 2 == 0; }
int is_negative(int x) { return x < 0; }
int is_zero(int x)     { return x == 0; }

int main(void) {
    int data[] = {-3, 0, 7, -1, 4, 12, -8, 2, 0, 5};
    int n = 10;

    printf("Even:     %d\n", filter_count(data, n, is_even));
    printf("Negative: %d\n", filter_count(data, n, is_negative));
    printf("Zero:     %d\n", filter_count(data, n, is_zero));
    return 0;
}
```

<details><summary>Predicción</summary>

```
Even:     6
Negative: 3
Zero:     2
```

Array: {-3, 0, 7, -1, 4, 12, -8, 2, 0, 5}

- Pares (x % 2 == 0): 0, 4, 12, -8, 2, 0 → 6.
- Negativos (x < 0): -3, -1, -8 → 3.
- Ceros (x == 0): 0, 0 → 2.

Nota: 0 es par (0 % 2 == 0), pero no es negativo ni positivo.
-8 es par (-8 % 2 == 0, el signo no afecta la paridad).

</details>

---

### Ejercicio 4 — qsort con enteros

```c
#include <stdio.h>
#include <stdlib.h>

int cmp_asc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int cmp_desc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ib > ia) - (ib < ia);
}

void print(const int *arr, int n) {
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
}

int main(void) {
    int nums[] = {42, 7, 13, 99, 1, 55, 23};
    int n = 7;

    printf("Original:   "); print(nums, n);

    qsort(nums, n, sizeof(int), cmp_asc);
    printf("Ascending:  "); print(nums, n);

    qsort(nums, n, sizeof(int), cmp_desc);
    printf("Descending: "); print(nums, n);

    return 0;
}
```

<details><summary>Predicción</summary>

```
Original:   42 7 13 99 1 55 23
Ascending:  1 7 13 23 42 55 99
Descending: 99 55 42 23 13 7 1
```

- `cmp_asc` retorna positivo cuando `a > b` → los menores van primero.
- `cmp_desc` invierte la lógica → los mayores van primero.

`(ia > ib) - (ia < ib)` produce:
- 1 - 0 = 1 cuando ia > ib.
- 0 - 1 = -1 cuando ia < ib.
- 0 - 0 = 0 cuando ia == ib.

</details>

---

### Ejercicio 5 — qsort con strings

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmp_alpha(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int cmp_length(const void *a, const void *b) {
    size_t la = strlen(*(const char **)a);
    size_t lb = strlen(*(const char **)b);
    return (la > lb) - (la < lb);
}

int main(void) {
    const char *words[] = {"banana", "fig", "cherry", "apple", "date"};
    int n = 5;

    qsort(words, n, sizeof(char *), cmp_alpha);
    printf("Alphabetical: ");
    for (int i = 0; i < n; i++) printf("%s ", words[i]);
    printf("\n");

    qsort(words, n, sizeof(char *), cmp_length);
    printf("By length:    ");
    for (int i = 0; i < n; i++) printf("%s ", words[i]);
    printf("\n");

    return 0;
}
```

<details><summary>Predicción</summary>

```
Alphabetical: apple banana cherry date fig
By length:    fig date apple banana cherry
```

- Alfabético: strcmp compara lexicográficamente.
- Por longitud: fig(3), date(4), apple(5), banana(6), cherry(6).

"banana" y "cherry" tienen la misma longitud (6). `qsort` **no es estable**:
su orden relativo es indeterminado (podrían aparecer en cualquier orden).

El doble cast `*(const char **)a` es necesario porque `qsort` pasa la dirección
de cada elemento del array. El array es de `char *`, así que cada elemento es un
`char *` y su dirección es un `char **`.

</details>

---

### Ejercicio 6 — Dispatch table: calculadora

```c
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divi(int a, int b) { return b != 0 ? a / b : 0; }

int main(void) {
    typedef int (*Op)(int, int);

    Op ops[] = {add, sub, mul, divi};
    const char *sym[] = {"+", "-", "*", "/"};

    int a = 15, b = 4;
    for (int i = 0; i < 4; i++) {
        printf("%d %s %d = %d\n", a, sym[i], b, ops[i](a, b));
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
15 + 4 = 19
15 - 4 = 11
15 * 4 = 60
15 / 4 = 3
```

- `ops[0]` = add → 15 + 4 = 19.
- `ops[1]` = sub → 15 - 4 = 11.
- `ops[2]` = mul → 15 × 4 = 60.
- `ops[3]` = divi → 15 / 4 = 3 (división entera, trunca hacia cero).

La dispatch table reemplaza un switch de 4 cases con un loop de una línea.
`ops[i](a, b)` selecciona la función por índice y la llama inmediatamente.

</details>

---

### Ejercicio 7 — Retornar puntero a función

```c
#include <stdio.h>

typedef int (*Operation)(int, int);

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

Operation get_op(char c) {
    switch (c) {
        case '+': return add;
        case '-': return sub;
        case '*': return mul;
        default:  return NULL;
    }
}

int main(void) {
    char ops[] = {'+', '-', '*', '?'};
    int a = 10, b = 3;

    for (int i = 0; i < 4; i++) {
        Operation op = get_op(ops[i]);
        if (op) {
            printf("'%c': %d\n", ops[i], op(a, b));
        } else {
            printf("'%c': unknown operator\n", ops[i]);
        }
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
'+': 13
'-': 7
'*': 30
'?': unknown operator
```

- `get_op('+')` retorna `add` → 10 + 3 = 13.
- `get_op('-')` retorna `sub` → 10 - 3 = 7.
- `get_op('*')` retorna `mul` → 10 × 3 = 30.
- `get_op('?')` retorna `NULL` → se imprime "unknown operator".

La verificación `if (op)` antes de llamar es esencial. Llamar a través de un
puntero a función NULL es UB (típicamente segfault).

</details>

---

### Ejercicio 8 — "Métodos" en struct

```c
#include <stdio.h>

struct Counter {
    int value;
    void (*increment)(struct Counter *self);
    void (*reset)(struct Counter *self);
    int (*get)(const struct Counter *self);
};

void counter_inc(struct Counter *self) { self->value++; }
void counter_reset(struct Counter *self) { self->value = 0; }
int counter_get(const struct Counter *self) { return self->value; }

struct Counter new_counter(void) {
    return (struct Counter){
        .value = 0,
        .increment = counter_inc,
        .reset = counter_reset,
        .get = counter_get,
    };
}

int main(void) {
    struct Counter c = new_counter();

    c.increment(&c);
    c.increment(&c);
    c.increment(&c);
    printf("After 3 increments: %d\n", c.get(&c));

    c.reset(&c);
    printf("After reset: %d\n", c.get(&c));
    return 0;
}
```

<details><summary>Predicción</summary>

```
After 3 increments: 3
After reset: 0
```

- `new_counter` retorna un struct con value=0 y los punteros a función asignados.
- Tres llamadas a `c.increment(&c)`: value pasa de 0 → 1 → 2 → 3.
- `c.reset(&c)`: value vuelve a 0.
- `c.get(&c)` retorna `c.value` sin modificarlo (parámetro `const`).

El `&c` se pasa explícitamente — C no tiene `this` implícito como C++.
Este patrón simula OOP en C y se usa en proyectos como el kernel de Linux
(vtables para file operations, device drivers, etc.).

</details>

---

### Ejercicio 9 — Dispatch table con enum

```c
#include <stdio.h>

enum Color { RED, GREEN, BLUE, COLOR_COUNT };

typedef void (*ColorPrinter)(void);

void print_red(void)   { printf("  #FF0000 (Red)\n"); }
void print_green(void) { printf("  #00FF00 (Green)\n"); }
void print_blue(void)  { printf("  #0000FF (Blue)\n"); }

ColorPrinter printers[COLOR_COUNT] = {
    [RED]   = print_red,
    [GREEN] = print_green,
    [BLUE]  = print_blue,
};

int main(void) {
    const char *names[] = {"RED", "GREEN", "BLUE"};

    for (int i = 0; i < COLOR_COUNT; i++) {
        printf("%s:\n", names[i]);
        printers[i]();
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
RED:
  #FF0000 (Red)
GREEN:
  #00FF00 (Green)
BLUE:
  #0000FF (Blue)
```

- `COLOR_COUNT` = 3 (tras RED=0, GREEN=1, BLUE=2).
- El array `printers` tiene 3 elementos, uno por color.
- Los designated initializers (`[RED] = print_red`) garantizan que cada índice
  corresponda al enum correcto.
- `printers[i]()` selecciona y llama al handler por índice en O(1).

Si se agrega un nuevo color (ej. `YELLOW`), basta insertar antes de `COLOR_COUNT`,
agregar `print_yellow`, y añadir `[YELLOW] = print_yellow`. El array se redimensiona
automáticamente porque su tamaño es `COLOR_COUNT`.

</details>

---

### Ejercicio 10 — qsort de structs

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Student {
    char name[50];
    int grade;
};

int cmp_by_name(const void *a, const void *b) {
    const struct Student *sa = (const struct Student *)a;
    const struct Student *sb = (const struct Student *)b;
    return strcmp(sa->name, sb->name);
}

int cmp_by_grade(const void *a, const void *b) {
    const struct Student *sa = (const struct Student *)a;
    const struct Student *sb = (const struct Student *)b;
    return (sa->grade > sb->grade) - (sa->grade < sb->grade);
}

int main(void) {
    struct Student students[] = {
        {"Charlie", 85},
        {"Alice", 92},
        {"Bob", 78},
        {"Diana", 95},
    };
    int n = 4;

    qsort(students, n, sizeof(struct Student), cmp_by_name);
    printf("By name:\n");
    for (int i = 0; i < n; i++)
        printf("  %s: %d\n", students[i].name, students[i].grade);

    qsort(students, n, sizeof(struct Student), cmp_by_grade);
    printf("By grade:\n");
    for (int i = 0; i < n; i++)
        printf("  %s: %d\n", students[i].name, students[i].grade);

    return 0;
}
```

<details><summary>Predicción</summary>

```
By name:
  Alice: 92
  Bob: 78
  Charlie: 85
  Diana: 95
By grade:
  Bob: 78
  Charlie: 85
  Alice: 92
  Diana: 95
```

- Por nombre: strcmp ordena alfabéticamente → Alice, Bob, Charlie, Diana.
- Por nota: orden ascendente → 78, 85, 92, 95.

A diferencia del ejemplo de strings, aquí el cast es simple: `(const struct Student *)a`
porque el array es de `struct Student` (no de punteros). `qsort` pasa la dirección de
cada struct directamente. No hay doble indirección como con `char **`.

El tercer argumento de qsort es `sizeof(struct Student)` — le dice a qsort cuántos
bytes ocupa cada elemento para poder moverlos durante el ordenamiento.

</details>
