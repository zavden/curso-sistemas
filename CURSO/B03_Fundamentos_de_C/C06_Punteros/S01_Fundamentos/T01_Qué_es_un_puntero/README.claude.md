# T01 — Qué es un puntero

> Sin erratas detectadas en el material base.

---

## 1 — Dirección de memoria y modelo mental

Cada byte en memoria tiene una **dirección** numérica. Un puntero es una variable que almacena una dirección:

```c
int x = 42;
int *p = &x;    // p almacena la dirección de x

printf("Valor de x:     %d\n", x);         // 42
printf("Dirección de x: %p\n", (void *)&x); // 0x7ffd...
printf("Valor de p:     %p\n", (void *)p);  // misma dirección
printf("Valor de *p:    %d\n", *p);         // 42
```

Modelo mental:

```
Variable x:                 Puntero p:
┌─────────┐                ┌──────────────┐
│   42    │                │  0x7ffd...   │ ← almacena la dirección de x
└─────────┘                └──────────────┘
 0x7ffd...  ← dirección     0x7ffd...+8   ← p tiene su propia dirección
```

Un puntero es una variable como cualquier otra — ocupa memoria, tiene su propia dirección, y su valor es una dirección de otra ubicación.

---

## 2 — Declaración de punteros

```c
// tipo *nombre — declara un puntero a 'tipo':
int *p;        // puntero a int
double *q;     // puntero a double
char *s;       // puntero a char
```

El `*` pertenece a la **variable**, no al tipo:

```c
int *a, b;     // a es int*, b es int (NO int*)
int *a, *b;    // a y b son int*
```

Estilos de posición del `*`:

```c
int *p;        // estilo K&R / Linux kernel — más común en C
int* p;        // estilo C++ — "el tipo es int-pointer"
int * p;       // poco usado
```

En C se prefiere `int *p` porque deja claro que el `*` va con la variable, especialmente en declaraciones múltiples.

---

## 3 — Operadores & y *

### & — operador de dirección (address-of)

Obtiene la dirección de memoria de una variable:

```c
int x = 42;
int *p = &x;   // p = dirección de x
```

### * — operador de desreferencia (indirección)

Accede al dato almacenado en la dirección que contiene el puntero:

```c
int val = *p;   // val = 42 — lee el dato en la dirección p
*p = 100;       // escribe 100 en la dirección p → x ahora vale 100
```

### Son operaciones inversas

```c
int x = 42;
int *p = &x;

*(&x);     // 42 — toma dirección de x, luego desreferencia → valor de x
&(*p);     // &x — desreferencia p (obtiene x como lvalue), luego toma dirección
```

`*p` es un **lvalue** — se puede usar en ambos lados del `=`:

```c
*p = 100;       // escritura
int y = *p;     // lectura
(*p)++;         // incrementa x a través del puntero
```

---

## 4 — Tamaño de un puntero

El tamaño de un puntero depende de la **arquitectura**, no del tipo apuntado:

```c
// En 64-bit: todos los punteros son 8 bytes
sizeof(int *)      // 8
sizeof(double *)   // 8
sizeof(char *)     // 8
sizeof(void *)     // 8

// En 32-bit: todos serían 4 bytes
```

Un puntero almacena una dirección, y en 64 bits todas las direcciones son de 8 bytes. El tipo del puntero determina:
- **Cuántos bytes** lee/escribe al desreferenciar (`*p` en un `int *` lee 4 bytes; en un `char *`, 1 byte)
- **Cuánto avanza** con aritmética (`p + 1` avanza 4 bytes para `int *`, 8 para `double *`)

---

## 5 — NULL: el puntero nulo

```c
#include <stddef.h>  // NULL también en stdlib.h, stdio.h

int *p = NULL;       // p no apunta a nada
```

`NULL` se define típicamente como `((void *)0)` en C. Desreferenciar `NULL` es **comportamiento indefinido** (en la mayoría de sistemas causa segfault).

```c
// SIEMPRE verificar antes de desreferenciar:
if (p != NULL) {
    printf("%d\n", *p);
}

// Forma abreviada (NULL es 0, que es falsy):
if (p) {
    printf("%d\n", *p);
}
```

### Reglas de inicialización

```c
int *p = &x;      // bien — dirección válida
int *p = NULL;     // bien — se puede verificar con if (p)
int *q;            // MAL — valor indeterminado (wild pointer)

// Después de free, poner a NULL:
free(data);
data = NULL;       // previene use-after-free accidental
```

Un puntero no inicializado contiene basura — puede apuntar a cualquier dirección. Desreferenciarlo es UB.

---

## 6 — Punteros y funciones

### Paso por referencia simulado

C pasa todo **por valor**. Para que una función modifique una variable del caller, se pasa la dirección:

```c
void increment(int *p) {
    (*p)++;    // modifica el dato apuntado
}

int x = 10;
increment(&x);
printf("x = %d\n", x);    // 11
```

Sin punteros, `increment(int x)` recibiría una **copia** — modificarla no afectaría al original.

### Swap — el ejemplo clásico

```c
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int x = 5, y = 10;
swap(&x, &y);
// x = 10, y = 5
```

### Output parameters — retornar múltiples valores

C solo permite retornar un valor con `return`. Para retornar múltiples valores, se usan punteros:

```c
void find_extremes(const int *arr, int n, int *min, int *max) {
    *min = arr[0];
    *max = arr[0];
    for (int i = 1; i < n; i++) {
        if (arr[i] < *min) *min = arr[i];
        if (arr[i] > *max) *max = arr[i];
    }
}

int data[] = {5, 2, 8, 1, 9, 3};
int min, max;
find_extremes(data, 6, &min, &max);
// min = 1, max = 9
```

Este patrón aparece en toda la biblioteca estándar: `scanf("%d", &x)`, `strtol(str, &end, 10)`, etc.

### Patrón éxito/error + valor por puntero

```c
#include <stdbool.h>

bool parse_int(const char *str, int *out) {
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') return false;  // no se pudo parsear
    *out = (int)val;
    return true;
}

int value;
if (parse_int("42", &value)) {
    printf("Parsed: %d\n", value);   // 42
}
```

La función retorna `bool` (éxito/error) y escribe el resultado a través del puntero `out`.

---

## 7 — Punteros y arrays (introducción)

El nombre de un array decae a puntero al primer elemento:

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;        // p = &arr[0]

arr[2];              // 30
*(p + 2);            // 30 — equivalente
p[2];                // 30 — un puntero se puede indexar como array
```

Pero **array ≠ puntero**:

```c
sizeof(arr);   // 20 — tamaño total del array (5 × 4)
sizeof(p);     // 8  — tamaño del puntero
// arr = p;    // ERROR — un array no se puede reasignar
```

(Esto se cubre en detalle en C05/S01/T02_Arrays_y_punteros.)

---

## 8 — Errores comunes con punteros

```c
// 1. Desreferenciar sin inicializar (wild pointer):
int *p;
*p = 42;         // UB — escribe en dirección indeterminada

// 2. Desreferenciar NULL:
int *p = NULL;
*p = 42;         // UB — segfault en la mayoría de sistemas

// 3. Use-after-free:
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p);   // UB — memoria ya liberada

// 4. Retornar puntero a variable local (dangling pointer):
int *bad_function(void) {
    int x = 42;
    return &x;    // WARNING — x se destruye al retornar
}
int *p = bad_function();
*p;               // UB — dirección ya inválida

// 5. Olvidar & al pasar a funciones:
int x = 42;
scanf("%d", x);   // INCORRECTO — pasa el valor 42 como dirección
scanf("%d", &x);  // CORRECTO — pasa la dirección de x
```

---

## 9 — Comparación con Rust

| Aspecto | C | Rust |
|---------|---|------|
| Referencia inmutable | `const int *p` | `&i32` (referencia) |
| Referencia mutable | `int *p` | `&mut i32` |
| Puntero nulo | `NULL` (requiere verificar manualmente) | No existe en referencias — `Option<&T>` |
| Puntero sin inicializar | Compila (UB al desreferenciar) | No compila — el borrow checker lo impide |
| Dangling pointer | Compila (UB en runtime) | No compila — lifetimes impiden devolver ref a local |
| Puntero crudo | Todo puntero es crudo | `*const T` / `*mut T` (solo en `unsafe`) |

```rust
// Referencias en Rust — seguras por diseño:
fn increment(x: &mut i32) {
    *x += 1;
}

let mut val = 10;
increment(&mut val);
// val = 11

// Output parameters: en Rust se prefiere retornar tuplas:
fn find_extremes(arr: &[i32]) -> (i32, i32) {
    let min = *arr.iter().min().unwrap();
    let max = *arr.iter().max().unwrap();
    (min, max)
}

let (min, max) = find_extremes(&[5, 2, 8, 1, 9, 3]);
```

Rust no necesita `NULL` porque usa `Option<&T>`: el compilador fuerza a manejar el caso `None` antes de acceder al valor. Los dangling pointers son imposibles porque el borrow checker verifica que las referencias nunca sobrevivan a los datos que apuntan.

---

## Ejercicios

### Ejercicio 1 — Rastrear direcciones

```c
// ¿Qué imprime? ¿Cuál es la relación entre las direcciones?
#include <stdio.h>

int main(void) {
    int a = 10, b = 20, c = 30;
    int *p = &a;

    printf("a: addr=%p val=%d\n", (void *)&a, a);
    printf("b: addr=%p val=%d\n", (void *)&b, b);
    printf("c: addr=%p val=%d\n", (void *)&c, c);
    printf("p: addr=%p val=%p *p=%d\n", (void *)&p, (void *)p, *p);

    p = &c;
    printf("\nAfter p = &c:\n");
    printf("p: addr=%p val=%p *p=%d\n", (void *)&p, (void *)p, *p);

    *p = 99;
    printf("c = %d\n", c);
    return 0;
}
```

<details><summary>Predicción</summary>

Las direcciones exactas varían, pero la estructura es predecible:

```
a: addr=0x7ffd...XX val=10
b: addr=0x7ffd...YY val=20
c: addr=0x7ffd...ZZ val=30
p: addr=0x7ffd...WW val=0x7ffd...XX *p=10
```

- `a`, `b`, `c` tienen direcciones consecutivas (o cercanas) en el stack
- `p` almacena la dirección de `a` → `*p` es 10
- `p` tiene su propia dirección (diferente de `a`, `b`, `c`)

```
After p = &c:
p: addr=0x7ffd...WW val=0x7ffd...ZZ *p=30
c = 99
```

- La dirección de `p` **no cambia** (es la misma variable) — solo cambió su contenido (ahora apunta a `c`)
- `*p = 99` modifica `c` a través del puntero → `c` vale 99
- `a` no se afecta — `p` ya no apunta a ella

</details>

### Ejercicio 2 — sizeof punteros vs tipos

```c
// ¿Qué imprime? ¿Algún sizeof es igual a otro?
#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;
    int **pp = &p;

    printf("sizeof(x)   = %zu\n", sizeof(x));
    printf("sizeof(p)   = %zu\n", sizeof(p));
    printf("sizeof(pp)  = %zu\n", sizeof(pp));
    printf("sizeof(*p)  = %zu\n", sizeof(*p));
    printf("sizeof(*pp) = %zu\n", sizeof(*pp));
    printf("sizeof(**pp)= %zu\n", sizeof(**pp));
    return 0;
}
```

<details><summary>Predicción</summary>

```
sizeof(x)   = 4     ← int: 4 bytes
sizeof(p)   = 8     ← int *: puntero, 8 bytes en 64-bit
sizeof(pp)  = 8     ← int **: puntero a puntero, 8 bytes
sizeof(*p)  = 4     ← *p es int: 4 bytes
sizeof(*pp) = 8     ← *pp es int *: puntero, 8 bytes
sizeof(**pp)= 4     ← **pp es int: 4 bytes
```

Todos los punteros (independientemente del nivel de indirección) ocupan 8 bytes en 64-bit. `sizeof(*p)` evalúa el **tipo** del resultado de desreferenciar, no ejecuta la desreferencia.

`sizeof(p) == sizeof(pp)` = 8 (ambos son punteros).
`sizeof(x) == sizeof(*p) == sizeof(**pp)` = 4 (todos son `int`).

</details>

### Ejercicio 3 — Swap sin punteros (no funciona)

```c
// ¿Por qué este swap NO funciona? ¿Qué imprime?
#include <stdio.h>

void bad_swap(int a, int b) {
    int temp = a;
    a = b;
    b = temp;
    printf("Inside bad_swap: a=%d b=%d\n", a, b);
}

void good_swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main(void) {
    int x = 5, y = 10;

    printf("Before bad_swap:  x=%d y=%d\n", x, y);
    bad_swap(x, y);
    printf("After bad_swap:   x=%d y=%d\n", x, y);

    printf("\nBefore good_swap: x=%d y=%d\n", x, y);
    good_swap(&x, &y);
    printf("After good_swap:  x=%d y=%d\n", x, y);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Before bad_swap:  x=5 y=10
Inside bad_swap: a=10 b=5
After bad_swap:   x=5 y=10

Before good_swap: x=5 y=10
After good_swap:  x=10 y=5
```

`bad_swap` recibe **copias** de `x` e `y`. El intercambio ocurre en las copias locales `a` y `b` — dentro de la función se ven intercambiados (`a=10, b=5`), pero `x` e `y` en `main` **no cambian**.

`good_swap` recibe **direcciones** de `x` e `y`. Al escribir `*a = *b`, modifica directamente la memoria de `x` e `y` en `main`. El intercambio persiste después del retorno.

Este es el motivo fundamental por el que existen los punteros en C: el lenguaje pasa todo por valor, y los punteros permiten simular paso por referencia.

</details>

### Ejercicio 4 — Puntero redirigido

```c
// ¿Qué imprime? Rastrea el valor de *p en cada paso.
#include <stdio.h>

int main(void) {
    int a = 1, b = 2, c = 3;
    int *p = &a;

    printf("*p = %d\n", *p);
    *p = 10;
    printf("a = %d\n", a);

    p = &b;
    printf("*p = %d\n", *p);
    *p += 20;
    printf("b = %d\n", b);

    p = &c;
    *p *= 10;
    printf("a=%d b=%d c=%d\n", a, b, c);
    return 0;
}
```

<details><summary>Predicción</summary>

```
*p = 1          ← p apunta a a (valor 1)
a = 10          ← *p = 10 modifica a
*p = 2          ← p ahora apunta a b (valor 2)
b = 22          ← *p += 20 → b = 2 + 20 = 22
a=10 b=22 c=30  ← p apunta a c, *p *= 10 → c = 3 * 10 = 30
```

El puntero `p` se redirige a diferentes variables. Cada `*p = ...` modifica la variable a la que `p` apunta en ese momento. Las variables anteriores mantienen el último valor que recibieron.

</details>

### Ejercicio 5 — Output parameters

```c
// ¿Qué imprime? ¿Cómo funciona el patrón de output parameters?
#include <stdio.h>
#include <stdbool.h>

bool divide(int a, int b, int *quotient, int *remainder) {
    if (b == 0) return false;
    *quotient = a / b;
    *remainder = a % b;
    return true;
}

int main(void) {
    int q, r;

    if (divide(17, 5, &q, &r)) {
        printf("17 / 5 = %d remainder %d\n", q, r);
    }

    if (divide(100, 3, &q, &r)) {
        printf("100 / 3 = %d remainder %d\n", q, r);
    }

    if (!divide(42, 0, &q, &r)) {
        printf("Division by zero!\n");
    }

    printf("q=%d r=%d\n", q, r);
    return 0;
}
```

<details><summary>Predicción</summary>

```
17 / 5 = 3 remainder 2
100 / 3 = 33 remainder 1
Division by zero!
q=33 r=1
```

- `divide(17, 5, &q, &r)`: `b != 0`, escribe `q = 3` y `r = 2`, retorna `true`
- `divide(100, 3, &q, &r)`: `b != 0`, escribe `q = 33` y `r = 1`, retorna `true`
- `divide(42, 0, &q, &r)`: `b == 0`, retorna `false` sin modificar `q` ni `r`
- Al final, `q` y `r` conservan los valores de la última llamada exitosa (33 y 1)

El patrón: la función retorna un `bool` indicando éxito/error, y escribe los resultados a través de punteros. El caller decide si usa los valores según el retorno.

</details>

### Ejercicio 6 — NULL safety

```c
// ¿Qué imprime? ¿Cuántas verificaciones de NULL hay?
#include <stdio.h>
#include <stdlib.h>

int safe_deref(const int *p, int default_val) {
    if (p == NULL) return default_val;
    return *p;
}

int main(void) {
    int x = 42;
    int *p1 = &x;
    int *p2 = NULL;
    int *p3 = malloc(sizeof(int));

    if (p3) *p3 = 99;

    printf("p1: %d\n", safe_deref(p1, -1));
    printf("p2: %d\n", safe_deref(p2, -1));
    printf("p3: %d\n", safe_deref(p3, -1));

    free(p3);
    p3 = NULL;

    printf("p3 after free: %d\n", safe_deref(p3, -1));
    return 0;
}
```

<details><summary>Predicción</summary>

```
p1: 42
p2: -1
p3: 99
p3 after free: -1
```

- `p1` apunta a `x` (42) → `safe_deref` retorna 42
- `p2` es `NULL` → `safe_deref` retorna el default (-1)
- `p3` apunta a heap con valor 99 → `safe_deref` retorna 99
- Después de `free(p3); p3 = NULL;` → `safe_deref` retorna -1

El patrón `free(p); p = NULL;` es importante: sin el `p = NULL`, `p` sería un **dangling pointer** — apunta a memoria liberada. Ponerlo a `NULL` permite que futuras verificaciones lo detecten como inválido.

`safe_deref` es un patrón defensivo: en lugar de crashear con un `NULL`, retorna un valor por defecto.

</details>

### Ejercicio 7 — & y * como inversos

```c
// ¿Qué imprime? ¿Son todas las expresiones equivalentes?
#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;
    int **pp = &p;

    printf("x        = %d\n", x);
    printf("*p       = %d\n", *p);
    printf("**pp     = %d\n", **pp);
    printf("*&x      = %d\n", *&x);
    printf("*&*p     = %d\n", *&*p);
    printf("**&p     = %d\n", **&p);

    printf("\n&x  = %p\n", (void *)&x);
    printf("p   = %p\n", (void *)p);
    printf("*pp = %p\n", (void *)*pp);
    printf("&*p = %p\n", (void *)&*p);
    printf("*&p = %p\n", (void *)*&p);
    return 0;
}
```

<details><summary>Predicción</summary>

Todos los valores en el primer grupo son **42**:

```
x        = 42       ← directo
*p       = 42       ← desreferencia p (apunta a x)
**pp     = 42       ← doble desreferencia: pp → p → x
*&x      = 42       ← & y * se cancelan: *(&x) = x
*&*p     = 42       ← *p = x, &x = &x, *(&x) = x
**&p     = 42       ← &p = &p, *(&p) = p, *p = x
```

Todas las direcciones en el segundo grupo son la **dirección de x**:

```
&x  = 0x7ffd...     ← dirección de x
p   = 0x7ffd...     ← p almacena &x
*pp = 0x7ffd...     ← *pp = p = &x
&*p = 0x7ffd...     ← *p = x (lvalue), &(x) = &x
*&p = 0x7ffd...     ← &p = &p, *(&p) = p = &x
```

`&` y `*` son inversos: aplicar uno después del otro se cancela. Esto es válido a cualquier nivel de indirección.

</details>

### Ejercicio 8 — Puntero a diferentes tipos

```c
// ¿Qué imprime? ¿Por qué el tipo del puntero importa?
#include <stdio.h>

int main(void) {
    int x = 0x41424344;    // 'D' 'C' 'B' 'A' en little-endian

    int *ip = &x;
    char *cp = (char *)&x;

    printf("*ip = 0x%X\n", *ip);
    printf("cp[0] = '%c' (0x%02X)\n", cp[0], (unsigned char)cp[0]);
    printf("cp[1] = '%c' (0x%02X)\n", cp[1], (unsigned char)cp[1]);
    printf("cp[2] = '%c' (0x%02X)\n", cp[2], (unsigned char)cp[2]);
    printf("cp[3] = '%c' (0x%02X)\n", cp[3], (unsigned char)cp[3]);
    return 0;
}
```

<details><summary>Predicción</summary>

```
*ip = 0x41424344
cp[0] = 'D' (0x44)
cp[1] = 'C' (0x43)
cp[2] = 'B' (0x42)
cp[3] = 'A' (0x41)
```

- `*ip` desreferencia como `int` → lee los 4 bytes como un entero: `0x41424344`
- `cp` es un `char *` al mismo dato → accede byte por byte
- En **little-endian** (x86), el byte menos significativo (0x44 = `'D'`) se almacena primero

El tipo del puntero determina **cuántos bytes** lee al desreferenciar y **cómo interpreta** esos bytes. Un `int *` lee 4 bytes como entero; un `char *` lee 1 byte como carácter. Los datos en memoria son los mismos — solo cambia la interpretación.

</details>

### Ejercicio 9 — Dangling pointer

```c
// ¿Qué problemas tiene este código? ¿Qué imprime?
#include <stdio.h>
#include <stdlib.h>

int *create_value(int val) {
    int x = val;
    return &x;    // WARNING: address of local variable
}

int *create_value_safe(int val) {
    int *p = malloc(sizeof(int));
    if (p) *p = val;
    return p;
}

int main(void) {
    int *bad = create_value(42);
    // printf("*bad = %d\n", *bad);  // UB — dangling pointer

    int *good = create_value_safe(42);
    if (good) {
        printf("*good = %d\n", *good);
        free(good);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
*good = 42
```

`create_value` retorna `&x`, pero `x` es una variable local que se destruye al retornar. El puntero retornado apunta a memoria del stack que ya no es válida → **dangling pointer**. La línea `*bad` está comentada porque sería UB. El compilador emite un warning: `address of local variable 'x' returned`.

`create_value_safe` aloca en el **heap** con `malloc`. La memoria del heap persiste después del retorno — el caller es responsable de liberar con `free`. Este es el patrón correcto para retornar datos creados dentro de una función.

Tres formas válidas de retornar datos de una función:
1. `return value;` — retornar por valor (solo para tipos pequeños)
2. `malloc` + retornar puntero — caller hace `free`
3. Output parameters — caller provee el buffer

</details>

### Ejercicio 10 — Patrón completo con verificación

```c
// ¿Qué imprime? Rastrea cada puntero paso a paso.
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int x, y;
} Point;

Point *point_new(int x, int y) {
    Point *p = malloc(sizeof(Point));
    if (!p) return NULL;
    p->x = x;
    p->y = y;
    return p;
}

void point_print(const Point *p) {
    if (!p) {
        printf("(null)\n");
        return;
    }
    printf("(%d, %d)\n", p->x, p->y);
}

void point_move(Point *p, int dx, int dy) {
    if (!p) return;
    p->x += dx;
    p->y += dy;
}

int main(void) {
    Point *a = point_new(3, 4);
    Point *b = point_new(10, 20);
    Point *c = NULL;

    point_print(a);
    point_print(b);
    point_print(c);

    point_move(a, 1, -1);
    point_print(a);

    free(a);
    free(b);
    a = NULL;
    b = NULL;

    point_print(a);
    return 0;
}
```

<details><summary>Predicción</summary>

```
(3, 4)
(10, 20)
(null)
(4, 3)
(null)
```

Paso a paso:

1. `point_new(3, 4)` → aloca un `Point` en el heap, escribe `{x=3, y=4}`, retorna puntero
2. `point_new(10, 20)` → aloca otro `Point` con `{x=10, y=20}`
3. `c = NULL` → no apunta a nada
4. `point_print(a)` → `a != NULL`, imprime `(3, 4)`
5. `point_print(b)` → `b != NULL`, imprime `(10, 20)`
6. `point_print(c)` → `c == NULL`, imprime `(null)`
7. `point_move(a, 1, -1)` → `a->x += 1` (3→4), `a->y += -1` (4→3)
8. `point_print(a)` → imprime `(4, 3)`
9. `free(a); free(b);` → libera memoria del heap
10. `a = NULL; b = NULL;` → previene dangling pointers
11. `point_print(a)` → `a == NULL`, imprime `(null)`

Este código demuestra un patrón completo: constructor (`point_new`), operaciones (`point_move`), destructor (`free`), y NULL-safety en cada función. El operador `->` es azúcar sintáctico: `p->x` es equivalente a `(*p).x`.

</details>
