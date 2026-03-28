# T02 — Aritmética de punteros

## Incremento y decremento

Sumar o restar un entero a un puntero avanza o retrocede
por **elementos**, no por bytes:

```c
#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;    // apunta a arr[0]

    printf("*p     = %d\n", *p);       // 10
    printf("*(p+1) = %d\n", *(p+1));   // 20
    printf("*(p+4) = %d\n", *(p+4));   // 50

    // p + 1 avanza sizeof(int) = 4 bytes, no 1 byte:
    printf("p     = %p\n", (void *)p);
    printf("p + 1 = %p\n", (void *)(p + 1));
    // Si p = 0x1000, entonces p+1 = 0x1004 (no 0x1001)

    return 0;
}
```

```c
// La fórmula real:
// p + n → dirección p + n * sizeof(*p)
//
// Para int* (sizeof(int) = 4):
//   p + 1 → p + 4 bytes
//   p + 3 → p + 12 bytes
//
// Para double* (sizeof(double) = 8):
//   p + 1 → p + 8 bytes
//
// Para char* (sizeof(char) = 1):
//   p + 1 → p + 1 byte
//
// El tipo del puntero determina el "paso" de la aritmética.
```

### Operadores de incremento

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;

// Post-incremento: usa el valor actual, luego avanza:
printf("%d\n", *p++);     // 10, luego p apunta a arr[1]
printf("%d\n", *p++);     // 20, luego p apunta a arr[2]

// Pre-incremento: avanza, luego usa el nuevo valor:
printf("%d\n", *++p);     // 40 — avanzó a arr[3], luego lee

// Decremento:
p--;                       // retrocede a arr[2]
printf("%d\n", *p);       // 30

// Precedencia: * y ++ tienen la misma precedencia,
// pero el post-incremento se aplica DESPUÉS:
// *p++ = *(p++) — desreferencia p, luego incrementa p
// *++p = *(++p) — incrementa p, luego desreferencia
// ++*p = ++(*p) — desreferencia p, luego incrementa el VALOR
// (*p)++ — desreferencia p, luego incrementa el VALOR (post)
```

## Recorrer arrays con punteros

```c
#include <stdio.h>

int main(void) {
    int arr[] = {10, 20, 30, 40, 50};
    int n = sizeof(arr) / sizeof(arr[0]);

    // Estilo 1: puntero que avanza
    for (int *p = arr; p < arr + n; p++) {
        printf("%d ", *p);
    }
    printf("\n");

    // Estilo 2: puntero begin/end
    int *begin = arr;
    int *end = arr + n;    // one-past-the-end
    for (int *p = begin; p != end; p++) {
        printf("%d ", *p);
    }
    printf("\n");

    // Estilo 3: copiar cadena con *dst++ = *src++
    char src[] = "hello";
    char dst[10];
    char *d = dst, *s = src;
    while ((*d++ = *s++) != '\0')
        ;
    // Copia incluyendo '\0'. Idioma clásico de C.

    return 0;
}
```

## Diferencia de punteros

Restar dos punteros al mismo array da el número de **elementos**
entre ellos:

```c
#include <stdio.h>
#include <stddef.h>    // ptrdiff_t

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = &arr[1];
    int *q = &arr[4];

    ptrdiff_t diff = q - p;    // 3 (elementos, no bytes)
    printf("q - p = %td\n", diff);

    // ptrdiff_t es un tipo con signo para diferencias de punteros.
    // Formato: %td

    ptrdiff_t diff2 = p - q;   // -3

    // Internamente: (q - p) = (dirección_q - dirección_p) / sizeof(int)

    return 0;
}
```

```c
// Solo es válido restar punteros al MISMO array
// (o uno-past-the-end). Restar punteros a objetos
// diferentes es UB:

int a[5], b[5];
int *p = &a[0];
int *q = &b[0];
ptrdiff_t bad = q - p;    // UB — diferentes arrays
```

## Comparación de punteros

```c
int arr[5] = {10, 20, 30, 40, 50};
int *p = &arr[1];
int *q = &arr[3];

// Comparar punteros al mismo array:
if (p < q)  printf("p is before q\n");     // sí
if (p == q) printf("same element\n");
if (p > q)  printf("p is after q\n");

// Comparar con NULL:
int *r = NULL;
if (r == NULL) printf("r is null\n");
if (!r) printf("r is null (short form)\n");

// Comparar punteros a objetos diferentes es UB
// (excepto == y != que son implementation-defined).
```

## Relación con arrays: arr[i] = *(arr + i)

```c
// El operador [] es azúcar sintáctico para aritmética de punteros:

int arr[5] = {10, 20, 30, 40, 50};

arr[2];         // → *(arr + 2) → 30
*(arr + 2);     // → 30

// Como la suma es conmutativa:
2[arr];         // → *(2 + arr) → 30  — legal pero horrendo

// Esto funciona con cualquier puntero:
int *p = arr;
p[3];           // → *(p + 3) → 40

// Y con punteros desplazados:
int *mid = arr + 2;
mid[0];         // → *(arr + 2) → 30
mid[-1];        // → *(arr + 1) → 20  — índices negativos son válidos
mid[-2];        // → *(arr + 0) → 10
```

## Aritmética con diferentes tipos

```c
#include <stdio.h>

int main(void) {
    // El "paso" de la aritmética depende del tipo:
    char   *pc = (char *)0x1000;
    int    *pi = (int *)0x1000;
    double *pd = (double *)0x1000;

    printf("pc + 1 = %p\n", (void *)(pc + 1));   // 0x1001 (+1 byte)
    printf("pi + 1 = %p\n", (void *)(pi + 1));   // 0x1004 (+4 bytes)
    printf("pd + 1 = %p\n", (void *)(pd + 1));   // 0x1008 (+8 bytes)

    // Por eso char* se usa para acceder byte por byte:
    int x = 0x04030201;
    char *bytes = (char *)&x;
    for (int i = 0; i < (int)sizeof(int); i++) {
        printf("byte[%d] = 0x%02x\n", i, (unsigned char)bytes[i]);
    }
    // En little-endian:
    // byte[0] = 0x01
    // byte[1] = 0x02
    // byte[2] = 0x03
    // byte[3] = 0x04

    return 0;
}
```

## Operaciones válidas e inválidas

```c
int arr[5];
int *p = arr;
int *q = arr + 3;

// VÁLIDO:
p + 3;          // puntero + entero → puntero
p - 2;          // puntero - entero → puntero
q - p;          // puntero - puntero → ptrdiff_t
p < q;          // comparación (mismo array)
p == NULL;      // comparación con NULL
p++;            // incremento
p--;            // decremento

// INVÁLIDO:
// p + q;       // ERROR: no se pueden sumar dos punteros
// p * 2;       // ERROR: no se puede multiplicar un puntero
// p / 2;       // ERROR: no se puede dividir un puntero
// p % 2;       // ERROR: no se puede aplicar módulo
// p & 0xFF;    // ERROR: no se pueden aplicar operaciones bitwise
```

## One-past-the-end

```c
int arr[5] = {1, 2, 3, 4, 5};

// arr + 5 apunta UNO DESPUÉS del último elemento:
int *end = arr + 5;

// Es VÁLIDO calcular esta dirección y compararla:
for (int *p = arr; p != end; p++) {
    printf("%d ", *p);
}

// Es INVÁLIDO desreferenciarla:
// *end = 42;    // UB — no hay elemento ahí

// Esto es por diseño: permite el patrón begin/end
// sin necesitar el tamaño del array.

// Solo se puede ir ONE past the end, no más:
// arr + 6  → UB (ni siquiera calcular la dirección)
```

---

## Ejercicios

### Ejercicio 1 — Recorrer con punteros

```c
// Dado int arr[] = {5, 10, 15, 20, 25, 30, 35, 40}:
// 1. Recorrer con un puntero que avanza (p++)
// 2. Recorrer en reversa con un puntero que retrocede (p--)
// 3. Recorrer solo los elementos pares (p += 2)
// Imprimir los valores en cada caso.
```

### Ejercicio 2 — Diferencia de punteros

```c
// Implementar size_t my_strlen(const char *s) usando
// aritmética de punteros:
// 1. Avanzar un puntero hasta encontrar '\0'
// 2. Restar el puntero original del puntero final
// 3. La diferencia es la longitud
// NO usar un contador entero — solo punteros.
```

### Ejercicio 3 — Buscar con punteros

```c
// Implementar int *find(int *begin, int *end, int value)
// que busque value en el rango [begin, end).
// Retorna puntero al elemento si lo encuentra, end si no.
// Usar aritmética de punteros para recorrer.
// Probar con arr[] = {3, 7, 1, 9, 4}.
```
