# T02 — Aritmética de punteros

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 162 | Dice que `==` y `!=` entre punteros a objetos diferentes son "implementation-defined" | Son **well-defined** (C11 §6.5.9/6): retornan si las direcciones son iguales o no. Solo `<`, `>`, `<=`, `>=` entre punteros a objetos diferentes son UB (C11 §6.5.8/5) |

---

## 1 — Incremento y decremento: p+n avanza n elementos

Sumar o restar un entero a un puntero avanza o retrocede por **elementos**, no por bytes:

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;    // apunta a arr[0]

*p;        // 10
*(p + 1);  // 20
*(p + 4);  // 50
```

Si `p = 0x1000` y `sizeof(int) = 4`:
- `p + 1` = `0x1004` (no `0x1001`)
- `p + 3` = `0x100C`

El compilador multiplica automáticamente — el programador piensa en elementos.

---

## 2 — La fórmula: p + n → p + n * sizeof(*p)

```c
// La dirección real que calcula el compilador:
// p + n  →  dirección_de_p + n * sizeof(*p)

// Para int* (sizeof(int) = 4):
//   p + 1 → avanza 4 bytes
//   p + 3 → avanza 12 bytes

// Para double* (sizeof(double) = 8):
//   p + 1 → avanza 8 bytes

// Para char* (sizeof(char) = 1):
//   p + 1 → avanza 1 byte
```

El tipo del puntero determina el "paso" (stride) de la aritmética. Por eso `char *` se usa para recorrer byte por byte:

```c
int x = 0x04030201;
char *bytes = (char *)&x;
// bytes[0] = 0x01 (little-endian: byte menos significativo primero)
// bytes[1] = 0x02
// bytes[2] = 0x03
// bytes[3] = 0x04
```

---

## 3 — Operadores de incremento y precedencia

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;

// Post-incremento: usa el valor actual, luego avanza p:
*p++     // → *(p++) → lee 10, luego p apunta a arr[1]

// Pre-incremento: avanza p, luego usa el nuevo valor:
*++p     // → *(++p) → avanza a arr[2], luego lee 30

// Incrementar el VALOR apuntado (no el puntero):
++*p     // → ++(*p) → incrementa el valor: arr[2] pasa de 30 a 31
(*p)++   // → incrementa el valor después de usarlo (post)
```

Tabla de precedencia:

| Expresión | Equivalente | Qué hace |
|-----------|-------------|----------|
| `*p++` | `*(p++)` | Lee `*p`, luego avanza `p` |
| `*++p` | `*(++p)` | Avanza `p`, luego lee `*p` |
| `++*p` | `++(*p)` | Incrementa el **valor** apuntado (pre) |
| `(*p)++` | — | Incrementa el **valor** apuntado (post) |

El patrón `*p++` es fundamental en C — aparece en idiomas como la copia de cadena:

```c
// Copia clásica de string:
while ((*dst++ = *src++) != '\0')
    ;
// Lee *src, escribe en *dst, avanza ambos, repite hasta '\0'
```

---

## 4 — Recorrer arrays con punteros

### Estilo 1 — Puntero que avanza con <

```c
int arr[] = {10, 20, 30, 40, 50};
int n = sizeof(arr) / sizeof(arr[0]);

for (int *p = arr; p < arr + n; p++) {
    printf("%d ", *p);
}
```

### Estilo 2 — begin/end con !=

```c
int *begin = arr;
int *end = arr + n;    // one-past-the-end

for (int *p = begin; p != end; p++) {
    printf("%d ", *p);
}
```

### Estilo 3 — Recorrido inverso

```c
// Forma correcta (evita UB de decrementar antes del inicio):
for (int *p = end; p != begin; ) {
    --p;
    printf("%d ", *p);
}
// Imprime: 50 40 30 20 10
```

Nota: el patrón `for (int *p = end - 1; p >= begin; p--)` funciona en la práctica, pero técnicamente cuando `p` se decrementa más allá de `begin` se crea un puntero antes del inicio del array, lo cual es UB (a diferencia de one-past-the-end, que sí está permitido). El patrón con `!=` y pre-decremento lo evita.

---

## 5 — Diferencia de punteros: ptrdiff_t

Restar dos punteros al **mismo array** da el número de **elementos** entre ellos:

```c
#include <stddef.h>  // ptrdiff_t

int arr[] = {10, 20, 30, 40, 50, 60, 70, 80};
int *p = &arr[1];
int *q = &arr[6];

ptrdiff_t diff = q - p;    // 5 (elementos, no bytes)
ptrdiff_t back = p - q;    // -5 (ptrdiff_t tiene signo)
```

La fórmula interna es la inversa de la suma:
```
q - p = (dirección_q - dirección_p) / sizeof(*p)
```

Si la diferencia en bytes es 20 y `sizeof(int) = 4`: `20 / 4 = 5` elementos.

`ptrdiff_t` se imprime con `%td`.

### Restricción: solo dentro del mismo array

```c
int a[5], b[5];
int *p = &a[0];
int *q = &b[0];
ptrdiff_t bad = q - p;    // UB — punteros a arrays diferentes
```

---

## 6 — Comparación de punteros

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = &arr[1];
int *q = &arr[3];

p < q;     // true — arr[1] tiene dirección menor que arr[3]
p > q;     // false
p == q;    // false
p != q;    // true
```

Un puntero a un índice menor es "menor" que uno a un índice mayor, porque las direcciones de memoria crecen con el índice dentro de un array.

### Reglas del estándar

- `<`, `>`, `<=`, `>=` entre punteros al **mismo array** (o one-past-the-end): **bien definido**
- `<`, `>`, `<=`, `>=` entre punteros a **objetos diferentes**: **UB** (C11 §6.5.8/5)
- `==`, `!=` entre **cualquier** par de punteros: **bien definido** (C11 §6.5.9/6) — retorna si las direcciones son iguales
- Comparación con `NULL`: siempre válida con `==` y `!=`

---

## 7 — arr[i] = *(arr + i): relación con arrays

El operador `[]` es azúcar sintáctico para aritmética de punteros:

```c
int arr[5] = {10, 20, 30, 40, 50};

arr[2];        // → *(arr + 2) → 30
*(arr + 2);    // → 30 — equivalente

// Como la suma es conmutativa:
2[arr];        // → *(2 + arr) → 30 — legal pero ilegible
```

Funciona con cualquier puntero, incluso desplazado:

```c
int *mid = arr + 2;
mid[0];     // → *(arr + 2) → 30
mid[-1];    // → *(arr + 1) → 20  — índices negativos son válidos
mid[-2];    // → *(arr + 0) → 10
```

Los índices negativos son válidos siempre que el resultado apunte dentro del array original.

---

## 8 — One-past-the-end

```c
int arr[5] = {1, 2, 3, 4, 5};
int *end = arr + 5;    // un elemento DESPUÉS del último
```

Reglas:
- **Calcular** esta dirección: **válido**
- **Comparar** con otros punteros al array: **válido**
- **Desreferenciar** (`*end`): **UB** — no hay elemento ahí
- Ir **más allá** (`arr + 6`): **UB** — ni siquiera calcular la dirección

Este diseño permite el patrón begin/end:

```c
for (int *p = arr; p != end; p++) {
    printf("%d ", *p);
}
```

Sin one-past-the-end, necesitaríamos pasar el tamaño del array como parámetro separado. Con este puntero, bastan dos punteros (`begin` y `end`).

---

## 9 — Operaciones válidas e inválidas

| Operación | Resultado | ¿Válida? |
|-----------|-----------|:---:|
| `puntero + entero` | puntero | Sí |
| `puntero - entero` | puntero | Sí |
| `puntero - puntero` | `ptrdiff_t` | Sí (mismo array) |
| `puntero < puntero` | `int` | Sí (mismo array) |
| `puntero == puntero` | `int` | Sí (cualquiera) |
| `puntero + puntero` | — | No (error) |
| `puntero * entero` | — | No (error) |
| `puntero / entero` | — | No (error) |
| `puntero % entero` | — | No (error) |
| `puntero & entero` | — | No (error) |

Sumar dos punteros no tiene sentido: si `p = 0x1000` y `q = 0x2000`, `0x3000` no tiene relación con ningún dato.

---

## 10 — Comparación con Rust

| Aspecto | C | Rust |
|---------|---|------|
| Aritmética de punteros | Core del lenguaje — implícita | Solo en `unsafe` con punteros crudos |
| Acceso a arrays | `arr[i]` = `*(arr+i)`, sin bounds checking | `arr[i]` con bounds checking (panic en OOB) |
| Iteración | Puntero manual `p++` | Iteradores: `arr.iter()` |
| Begin/end | Punteros crudos al programador | `iter()` encapsula begin/end internamente |
| One-past-the-end | Válido calcular, UB desreferenciar | No existe — iteradores manejan el rango |
| Índices negativos | `mid[-1]` = `*(mid - 1)`, peligroso | No existen — slices usan `usize` (unsigned) |

```rust
// Rust: iteración segura sin aritmética de punteros
let arr = [10, 20, 30, 40, 50];

// Forward
for val in &arr {
    print!("{} ", val);
}

// Reverse
for val in arr.iter().rev() {
    print!("{} ", val);
}

// Slices: el equivalente seguro de begin/end
let mid = &arr[1..4];  // [20, 30, 40] — bounds checked
```

En Rust, la aritmética de punteros solo es posible dentro de bloques `unsafe` con `*const T` / `*mut T` y métodos como `.add()`, `.offset()`. En código `safe`, los iteradores y slices hacen lo mismo con garantías de seguridad.

---

## Ejercicios

### Ejercicio 1 — Stride de diferentes tipos

```c
// ¿Qué imprime? Calcula las direcciones manualmente.
#include <stdio.h>

int main(void) {
    int    arr_i[3] = {100, 200, 300};
    char   arr_c[3] = {'X', 'Y', 'Z'};
    double arr_d[3] = {1.5, 2.5, 3.5};

    int    *pi = arr_i;
    char   *pc = arr_c;
    double *pd = arr_d;

    printf("int:    %td bytes between elements\n",
           (char *)(pi + 1) - (char *)pi);
    printf("char:   %td bytes between elements\n",
           (char *)(pc + 1) - (char *)pc);
    printf("double: %td bytes between elements\n",
           (char *)(pd + 1) - (char *)pd);

    printf("\n*(pi + 2) = %d\n", *(pi + 2));
    printf("*(pc + 2) = %c\n", *(pc + 2));
    printf("*(pd + 2) = %.1f\n", *(pd + 2));
    return 0;
}
```

<details><summary>Predicción</summary>

```
int:    4 bytes between elements
char:   1 bytes between elements
double: 8 bytes between elements

*(pi + 2) = 300
*(pc + 2) = Z
*(pd + 2) = 3.5
```

- `int *`: avanza 4 bytes por elemento (`sizeof(int) = 4`)
- `char *`: avanza 1 byte por elemento (`sizeof(char) = 1`)
- `double *`: avanza 8 bytes por elemento (`sizeof(double) = 8`)

El `(char *)` cast convierte a puntero a byte para medir la diferencia real en bytes, ya que `char` siempre es 1 byte. `*(pi + 2)` accede al tercer elemento de cada array.

</details>

### Ejercicio 2 — Precedencia de * y ++

```c
// ¿Qué imprime? Rastrea p y arr en cada paso.
#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;

    printf("A: %d\n", *p++);
    printf("B: %d\n", *p++);
    printf("C: %d\n", *++p);
    printf("D: %d\n", ++*p);
    printf("E: %d\n", (*p)++);
    printf("F: %d\n", *p);

    printf("\narr: ");
    for (int i = 0; i < 5; i++) printf("%d ", arr[i]);
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
A: 10
B: 20
C: 40
D: 41
E: 41
F: 42

arr: 10 20 30 42 50
```

Paso a paso:
- **A**: `*p++` = `*(p++)`: lee `*p` = arr[0] = 10, luego p→arr[1]
- **B**: `*p++` = `*(p++)`: lee `*p` = arr[1] = 20, luego p→arr[2]
- **C**: `*++p` = `*(++p)`: avanza p→arr[3], luego lee `*p` = 40
- **D**: `++*p` = `++(*p)`: incrementa arr[3] de 40 a 41, retorna 41. p no cambia.
- **E**: `(*p)++`: lee arr[3] = 41 (retorna 41), luego incrementa arr[3] a 42. p no cambia.
- **F**: `*p`: lee arr[3] = 42

El array queda `{10, 20, 30, 42, 50}` — solo arr[3] fue modificado por `++*p` y `(*p)++`.

</details>

### Ejercicio 3 — ptrdiff_t

```c
// ¿Qué imprime?
#include <stdio.h>
#include <stddef.h>

int main(void) {
    int arr[8] = {0, 10, 20, 30, 40, 50, 60, 70};
    int *a = &arr[2];
    int *b = &arr[7];

    ptrdiff_t d1 = b - a;
    ptrdiff_t d2 = a - b;

    printf("b - a = %td\n", d1);
    printf("a - b = %td\n", d2);
    printf("bytes = %td\n", (char *)b - (char *)a);

    printf("\nElements from a to b: ");
    for (int *p = a; p < b; p++) {
        printf("%d ", *p);
    }
    printf("\n");

    printf("a[-1] = %d\n", a[-1]);
    printf("a[-2] = %d\n", a[-2]);
    return 0;
}
```

<details><summary>Predicción</summary>

```
b - a = 5
a - b = -5
bytes = 20

Elements from a to b: 20 30 40 50 60
a[-1] = 10
a[-2] = 0
```

- `b` apunta a arr[7], `a` apunta a arr[2] → `b - a = 5` elementos
- `a - b = -5` (ptrdiff_t tiene signo)
- Bytes: `5 * sizeof(int) = 5 * 4 = 20`
- Recorrido de a hasta b (sin incluir b): arr[2] a arr[6] = {20, 30, 40, 50, 60}
- `a[-1]` = `*(a - 1)` = arr[1] = 10
- `a[-2]` = `*(a - 2)` = arr[0] = 0

Los índices negativos son válidos porque `a[-n]` equivale a `*(a - n)`, que apunta a posiciones anteriores dentro del array.

</details>

### Ejercicio 4 — Copia con *dst++ = *src++

```c
// ¿Qué imprime? ¿Cómo funciona el idiom de copia?
#include <stdio.h>

void my_strcpy(char *dst, const char *src) {
    while ((*dst++ = *src++) != '\0')
        ;
}

int main(void) {
    char src[] = "hello";
    char dst[10];

    my_strcpy(dst, src);
    printf("dst = \"%s\"\n", dst);
    printf("dst bytes: ");
    for (int i = 0; i < 8; i++) {
        if (dst[i] == '\0')
            printf("\\0 ");
        else
            printf("'%c' ", dst[i]);
    }
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
dst = "hello"
dst bytes: 'h' 'e' 'l' 'l' 'o' \0 ?? ??
```

(Los bytes 6 y 7 son indeterminados — no fueron escritos.)

El idiom `*dst++ = *src++` hace tres cosas en una expresión:
1. Lee `*src` (carácter actual)
2. Escribe ese carácter en `*dst`
3. Avanza ambos punteros (`src++`, `dst++`)

La condición `!= '\0'` detiene el loop **después** de copiar el `'\0'`, porque la asignación ocurre antes de la comparación. Así `dst` termina con un terminador válido.

</details>

### Ejercicio 5 — One-past-the-end

```c
// ¿Qué imprime? ¿Cuáles desreferencias son válidas?
#include <stdio.h>

int main(void) {
    int arr[4] = {10, 20, 30, 40};
    int *begin = arr;
    int *end = arr + 4;

    printf("begin = %p\n", (void *)begin);
    printf("end   = %p\n", (void *)end);
    printf("end - begin = %td\n", end - begin);

    // ¿Cuáles son válidos?
    printf("*begin     = %d\n", *begin);       // [A]
    printf("*(end - 1) = %d\n", *(end - 1));   // [B]
    // printf("*end       = %d\n", *end);       // [C]
    // printf("*(end + 1) = %d\n", *(end + 1)); // [D]

    printf("begin < end ? %s\n", begin < end ? "true" : "false");
    return 0;
}
```

<details><summary>Predicción</summary>

```
begin = 0x7ffd...
end   = 0x7ffd... (+16 bytes)
end - begin = 4
*begin     = 10
*(end - 1) = 40
begin < end ? true
```

Validez de cada desreferencia:
- **[A]** `*begin`: válido — apunta a arr[0]
- **[B]** `*(end - 1)`: válido — apunta a arr[3] (el último elemento)
- **[C]** `*end`: **UB** — one-past-the-end se puede calcular y comparar, pero NO desreferenciar
- **[D]** `*(end + 1)`: **UB** — `end + 1 = arr + 5`, que está más allá de one-past-the-end. Ni siquiera calcular `arr + 5` es válido per el estándar (C11 §6.5.6/8)

La resta `end - begin = 4` es válida y devuelve el número de elementos. Esto es lo que hace útil al puntero one-past-the-end: delimitar el rango sin necesitar el tamaño.

</details>

### Ejercicio 6 — begin/end como parámetros

```c
// ¿Qué imprime?
#include <stdio.h>

int sum_range(const int *begin, const int *end) {
    int total = 0;
    for (const int *p = begin; p != end; p++) {
        total += *p;
    }
    return total;
}

int *find(int *begin, int *end, int value) {
    for (int *p = begin; p != end; p++) {
        if (*p == value) return p;
    }
    return end;  // no encontrado
}

int main(void) {
    int arr[] = {3, 7, 1, 9, 4, 6, 2};
    int n = sizeof(arr) / sizeof(arr[0]);

    printf("Sum all: %d\n", sum_range(arr, arr + n));
    printf("Sum [2..5): %d\n", sum_range(arr + 2, arr + 5));

    int *pos = find(arr, arr + n, 9);
    if (pos != arr + n) {
        printf("Found 9 at index %td\n", pos - arr);
    }

    pos = find(arr, arr + n, 42);
    if (pos == arr + n) {
        printf("42 not found\n");
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
Sum all: 32
Sum [2..5): 14
Found 9 at index 3
42 not found
```

- `sum_range(arr, arr + 7)`: suma todos → 3+7+1+9+4+6+2 = 32
- `sum_range(arr + 2, arr + 5)`: suma arr[2], arr[3], arr[4] → 1+9+4 = 14
- `find(arr, arr + 7, 9)`: encuentra 9 en arr[3] → retorna `arr + 3`, índice = `(arr+3) - arr` = 3
- `find(arr, arr + 7, 42)`: no lo encuentra → retorna `end` (arr + 7), que es igual a `arr + n`

El patrón begin/end permite reutilizar la misma función para subarrays sin copiar datos: `sum_range(arr + 2, arr + 5)` opera sobre un "slice" del array original.

</details>

### Ejercicio 7 — Operaciones inválidas

```c
// ¿Cuáles líneas compilan? ¿Cuáles dan error?
#include <stdio.h>
#include <stddef.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;
    int *q = arr + 3;

    int *r1 = p + 2;           // [A]
    int *r2 = q - 1;           // [B]
    ptrdiff_t d = q - p;       // [C]
    // int *r3 = p + q;        // [D]
    // int *r4 = p * 2;        // [E]
    // int *r5 = p / q;        // [F]
    int x = p < q;             // [G]
    // int y = p & 0xFF;       // [H]
    p++;                       // [I]
    p += 2;                    // [J]

    printf("r1=%p r2=%p d=%td x=%d\n",
           (void *)r1, (void *)r2, d, x);
    return 0;
}
```

<details><summary>Predicción</summary>

| Línea | ¿Compila? | Razón |
|:---:|:---:|---|
| [A] | Sí | `puntero + entero → puntero` |
| [B] | Sí | `puntero - entero → puntero` |
| [C] | Sí | `puntero - puntero → ptrdiff_t` |
| [D] | No | Sumar dos punteros no tiene sentido |
| [E] | No | Multiplicar un puntero no tiene sentido |
| [F] | No | Dividir un puntero entre otro no tiene sentido |
| [G] | Sí | Comparación entre punteros al mismo array → `int` |
| [H] | No | Operaciones bitwise sobre punteros no son válidas |
| [I] | Sí | `p++` equivale a `p = p + 1` (avanza un elemento) |
| [J] | Sí | `p += 2` equivale a `p = p + 2` (avanza dos elementos) |

Las únicas operaciones aritméticas válidas con punteros son: sumar/restar un entero, restar otro puntero, y comparar. Cualquier otra operación (`+`, `*`, `/`, `%`, `&`, `|`, `^`, `~` entre punteros o con tipos inválidos) es un error de compilación.

</details>

### Ejercicio 8 — Comparación de punteros

```c
// ¿Qué imprime? ¿Todas las comparaciones son bien definidas?
#include <stdio.h>

int main(void) {
    int a[3] = {1, 2, 3};
    int b[3] = {4, 5, 6};

    int *pa = &a[1];
    int *pb = &b[1];

    // Mismo array:
    printf("&a[0] < &a[2] ? %s\n", &a[0] < &a[2] ? "true" : "false");

    // Diferentes arrays:
    printf("pa == pb ? %s\n", pa == pb ? "true" : "false");
    printf("pa != pb ? %s\n", pa != pb ? "true" : "false");
    // printf("pa < pb ? %s\n", pa < pb ? "true" : "false");  // UB

    // NULL:
    int *null = NULL;
    printf("null == NULL ? %s\n", null == NULL ? "true" : "false");
    printf("pa == NULL ? %s\n", pa == NULL ? "true" : "false");
    return 0;
}
```

<details><summary>Predicción</summary>

```
&a[0] < &a[2] ? true
pa == pb ? false
pa != pb ? true
null == NULL ? true
pa == NULL ? false
```

- `&a[0] < &a[2]`: bien definido (mismo array) → true, porque arr[0] tiene dirección menor
- `pa == pb`: **bien definido** (C11 §6.5.9/6) — `==` y `!=` entre punteros a objetos diferentes son válidos. Retorna false porque apuntan a objetos distintos
- `pa < pb`: sería **UB** — `<`, `>`, `<=`, `>=` entre punteros a arrays diferentes no están definidos
- Comparaciones con `NULL`: siempre válidas con `==` y `!=`

La distinción es importante: `==`/`!=` simplemente comparan direcciones numéricas y siempre son válidos. Los operadores relacionales (`<`, `>`) asumen un orden lineal que solo tiene sentido dentro del mismo array.

</details>

### Ejercicio 9 — Recorrido inverso con punteros

```c
// ¿Qué imprime? Compara los dos estilos de recorrido inverso.
#include <stdio.h>

void print_reverse_v1(const int *arr, int n) {
    const int *end = arr + n;
    for (const int *p = end; p != arr; ) {
        --p;
        printf("%d ", *p);
    }
    printf("\n");
}

void print_reverse_v2(const int *arr, int n) {
    for (int i = n - 1; i >= 0; i--) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int main(void) {
    int data[] = {100, 200, 300, 400, 500};

    printf("v1: ");
    print_reverse_v1(data, 5);

    printf("v2: ");
    print_reverse_v2(data, 5);
    return 0;
}
```

<details><summary>Predicción</summary>

```
v1: 500 400 300 200 100
v2: 500 400 300 200 100
```

Ambas imprimen lo mismo, pero v1 usa punteros y v2 usa índices.

**v1** (punteros): empieza en `end` (one-past-the-end), decrementa **antes** de desreferenciar. Cuando `p == arr`, el loop termina sin decrementar más allá del inicio. Este patrón es correcto y evita el UB de crear un puntero antes del inicio del array.

**v2** (índices): empieza en `n - 1`, decrementa hasta 0 inclusive. Usa `int i` (con signo) para que `i >= 0` funcione. Si `i` fuera `size_t` (unsigned), `i >= 0` sería siempre true → loop infinito.

La versión v1 es más idiomática en código que trabaja con punteros begin/end; la v2 es más clara para lectores no familiarizados con aritmética de punteros.

</details>

### Ejercicio 10 — my_strlen con aritmética de punteros

```c
// ¿Qué imprime? ¿Cómo funciona strlen sin contador entero?
#include <stdio.h>
#include <stddef.h>

size_t my_strlen(const char *s) {
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

int main(void) {
    printf("strlen(\"hello\")   = %zu\n", my_strlen("hello"));
    printf("strlen(\"\")        = %zu\n", my_strlen(""));
    printf("strlen(\"abc\\0def\") = %zu\n", my_strlen("abc\0def"));

    // Bonus: usar my_strlen para iterar
    const char *msg = "world";
    const char *end = msg + my_strlen(msg);
    printf("Reverse: ");
    for (const char *p = end; p != msg; ) {
        --p;
        printf("%c", *p);
    }
    printf("\n");
    return 0;
}
```

<details><summary>Predicción</summary>

```
strlen("hello")   = 5
strlen("")        = 0
strlen("abc\0def") = 3
Reverse: dlrow
```

`my_strlen` avanza un puntero `p` desde el inicio `s` hasta encontrar `'\0'`. La longitud es la diferencia de punteros `p - s`, que da el número de caracteres recorridos.

- `"hello"`: p avanza 5 posiciones → `p - s = 5`
- `""`: `*p` es `'\0'` inmediatamente → `p - s = 0`
- `"abc\0def"`: p para en el primer `'\0'` (posición 3) → `p - s = 3`

No se necesita ningún contador entero — la resta de punteros calcula directamente la distancia. Este es exactamente cómo se implementa `strlen` en la biblioteca estándar (versión simple; las optimizadas procesan múltiples bytes a la vez).

El bonus itera en reversa: `end = msg + 5` apunta al `'\0'`, el loop decrementa antes de leer → imprime `d`, `l`, `r`, `o`, `w` = "dlrow".

</details>
