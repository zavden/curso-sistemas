# T01 — Qué es un puntero

## Dirección de memoria

Cada byte en memoria tiene una **dirección** numérica.
Un puntero es una variable que almacena una dirección:

```c
#include <stdio.h>

int main(void) {
    int x = 42;

    // &x es la DIRECCIÓN de x en memoria:
    printf("Valor de x:      %d\n", x);
    printf("Dirección de x:  %p\n", (void *)&x);
    // Ejemplo: 0x7ffd5a3b4c2c

    // Un puntero almacena esa dirección:
    int *p = &x;
    printf("Valor de p:      %p\n", (void *)p);    // misma dirección
    printf("Valor de *p:     %d\n", *p);            // 42 — el dato en esa dirección

    return 0;
}
```

```c
// Modelo mental:
//
// Variable x:
// ┌─────────┐
// │   42     │  ← valor
// └─────────┘
//  0x7ffd...  ← dirección (dada por el SO)
//
// Puntero p:
// ┌──────────────┐
// │ 0x7ffd...    │  ← p almacena la dirección de x
// └──────────────┘
//  0x7ffd...+8    ← p también tiene su propia dirección
```

## Declaración de punteros

```c
// tipo *nombre — declara un puntero a 'tipo':
int *p;          // puntero a int
double *q;       // puntero a double
char *s;         // puntero a char

// El * pertenece a la VARIABLE, no al tipo:
int *a, b;       // a es int*, b es int (no int*)
int *a, *b;      // a y b son int*

// Estilo: el * puede ir en diferentes posiciones:
int *p;          // estilo K&R / Linux kernel (más común en C)
int* p;          // estilo C++ / "el tipo es int-pointer"
int * p;         // poco usado
// En C, el estilo int *p es preferido porque deja claro
// que * va con la variable, no con el tipo.
```

## Operadores & y *

```c
// & — operador de dirección (address-of):
int x = 42;
int *p = &x;     // p = dirección de x

// * — operador de desreferencia (indirección):
int val = *p;     // val = 42 — lee el dato en la dirección p
*p = 100;         // escribe 100 en la dirección p → x ahora vale 100

printf("x = %d\n", x);    // 100 — x fue modificado a través de p
```

```c
// & y * son inversos:
int x = 42;
int *p = &x;

*(&x);      // 42 — dirección de x, luego desreferenciar → valor de x
&(*p);      // dirección de x — desreferenciar p, luego tomar dirección

// *p es un lvalue — se puede usar en ambos lados del =:
*p = 100;       // escribe
int y = *p;     // lee
(*p)++;         // incrementa x a través del puntero
```

## Tamaño de un puntero

```c
#include <stdio.h>

int main(void) {
    // Un puntero almacena una dirección.
    // El tamaño depende de la arquitectura, NO del tipo apuntado:
    printf("sizeof(int *)    = %zu\n", sizeof(int *));       // 8
    printf("sizeof(double *) = %zu\n", sizeof(double *));    // 8
    printf("sizeof(char *)   = %zu\n", sizeof(char *));      // 8
    printf("sizeof(void *)   = %zu\n", sizeof(void *));      // 8

    // En 64-bit: todos los punteros son 8 bytes
    // En 32-bit: todos los punteros son 4 bytes

    // El tipo del puntero NO afecta su tamaño.
    // Afecta cuántos bytes lee/escribe al desreferenciar
    // y cuánto avanza con aritmética.
    return 0;
}
```

## NULL — El puntero nulo

```c
#include <stdio.h>
#include <stddef.h>    // NULL está en stddef.h, stdlib.h, stdio.h...

int main(void) {
    int *p = NULL;     // p no apunta a nada

    // NULL se define como:
    // #define NULL ((void *)0)    — en C
    // #define NULL 0              — alternativa

    // Desreferenciar NULL es UB (típicamente segfault):
    // *p = 42;    // segfault en la mayoría de sistemas

    // SIEMPRE verificar antes de desreferenciar:
    if (p != NULL) {
        printf("%d\n", *p);
    }

    // Forma abreviada (0 es false, no-0 es true):
    if (p) {
        printf("%d\n", *p);
    }

    return 0;
}
```

```c
// Inicialización: SIEMPRE inicializar punteros.
// Si no tienes un valor, usar NULL:
int *p = NULL;        // bien — se puede verificar
int *q;               // mal — valor indeterminado (wild pointer)

// Después de free, poner a NULL:
int *data = malloc(100 * sizeof(int));
// ...usar data...
free(data);
data = NULL;          // previene use-after-free accidental
```

## Punteros y funciones

```c
// Pasar punteros a funciones permite modificar variables del caller:

void increment(int *p) {
    (*p)++;    // modifica el dato apuntado
}

int main(void) {
    int x = 10;
    increment(&x);
    printf("x = %d\n", x);    // 11

    return 0;
}
```

```c
// Patrón: retornar éxito/error + valor por puntero:
#include <stdbool.h>

bool parse_int(const char *str, int *out) {
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') return false;
    *out = (int)val;
    return true;
}

int main(void) {
    int value;
    if (parse_int("42", &value)) {
        printf("Parsed: %d\n", value);    // 42
    }
    if (!parse_int("abc", &value)) {
        printf("Failed to parse\n");
    }
    return 0;
}
```

## Punteros y arrays

```c
// El nombre de un array decae a puntero al primer elemento:
int arr[5] = {10, 20, 30, 40, 50};
int *p = arr;          // p = &arr[0]

// arr[i] es equivalente a *(p + i):
printf("%d\n", arr[2]);       // 30
printf("%d\n", *(p + 2));     // 30
printf("%d\n", p[2]);         // 30 — un puntero se puede indexar

// Pero array ≠ puntero:
// sizeof(arr) = 20 (tamaño total del array)
// sizeof(p)   = 8  (tamaño del puntero)
// arr = p;    // ERROR — array no se puede reasignar
```

## Punteros con const

```c
// const protege contra modificación accidental:

void print_value(const int *p) {
    printf("%d\n", *p);
    // *p = 100;    // ERROR: p apunta a const int
}

int main(void) {
    int x = 42;
    print_value(&x);   // OK — int* se convierte a const int* implícitamente

    const int y = 10;
    int *bad = &y;     // WARNING: descarta const
    const int *good = &y;   // OK

    return 0;
}
// Las 4 combinaciones de const y punteros se ven en detalle en S02.
```

## Errores comunes

```c
// 1. Desreferenciar sin inicializar:
int *p;          // valor indeterminado
*p = 42;         // UB — escribe en dirección aleatoria

// 2. Desreferenciar NULL:
int *p = NULL;
*p = 42;         // UB — segfault

// 3. Usar después de free:
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p);   // UB — use-after-free

// 4. Retornar puntero a variable local:
int *bad_function(void) {
    int x = 42;
    return &x;    // WARNING: x se destruye al retornar
}
int *p = bad_function();
*p;    // UB — dangling pointer

// 5. Confundir & y *:
int x = 42;
int *p = &x;
int **pp = &p;      // puntero a puntero
printf("%d\n", **pp);  // 42 — doble desreferencia
```

---

## Ejercicios

### Ejercicio 1 — Básico

```c
// Declarar int x = 10, int y = 20.
// Crear un puntero int *p.
// 1. Apuntar p a x, imprimir *p.
// 2. Modificar x a través de *p, imprimir x.
// 3. Redirigir p a y, imprimir *p.
// 4. Imprimir las direcciones de x, y, p con %p.
```

### Ejercicio 2 — Swap con punteros

```c
// Implementar void swap(int *a, int *b).
// En main, declarar x = 5, y = 10.
// Llamar swap(&x, &y) y verificar que los valores se intercambiaron.
```

### Ejercicio 3 — min y max con output parameters

```c
// Implementar:
// void find_extremes(const int *arr, int n, int *min, int *max)
// que encuentre el mínimo y máximo de un array.
// Retornar los valores a través de punteros min y max.
// Probar con {5, 2, 8, 1, 9, 3}.
```
