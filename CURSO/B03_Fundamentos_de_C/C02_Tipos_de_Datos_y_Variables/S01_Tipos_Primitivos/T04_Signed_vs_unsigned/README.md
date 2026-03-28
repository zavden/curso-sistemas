# T04 — Signed vs unsigned

## La diferencia fundamental

Signed y unsigned usan los mismos bits pero los interpretan
de forma diferente:

```c
// Unsigned: solo positivos, rango 0 a 2^N - 1
unsigned char uc = 200;    // 200 (válido: 0–255)

// Signed: positivos y negativos, rango -2^(N-1) a 2^(N-1) - 1
signed char sc = 200;      // implementation-defined: probablemente -56

// Mismos bits, diferente interpretación:
// 11001000 como unsigned char = 200
// 11001000 como signed char   = -56 (complemento a dos)
```

```
Representación en complemento a dos (8 bits):

Bits        unsigned    signed
00000000    0           0
00000001    1           1
01111111    127         127
10000000    128         -128    ← aquí divergen
10000001    129         -127
11111111    255         -1
```

## Overflow: UB vs wrapping

Esta es la diferencia más importante entre signed y unsigned
desde el punto de vista del programador:

### Signed overflow → Undefined Behavior

```c
#include <limits.h>

int x = INT_MAX;          // 2147483647
int y = x + 1;            // UB: signed integer overflow

// El compilador puede:
// 1. Producir -2147483648 (wrapping — lo que "esperas")
// 2. Producir 0
// 3. Eliminar código que dependa de este resultado
// 4. Cualquier cosa — es UB
```

```c
// Consecuencia práctica — el compilador optimiza con UB:
int check(int x) {
    return x + 1 > x;     // ¿es x + 1 > x?
}
// El compilador puede optimizar a: return 1;
// Razonamiento: "x + 1 > x siempre es true para signed,
// porque el overflow no puede ocurrir (es UB)"

// Con unsigned NO puede hacer esa optimización,
// porque UINT_MAX + 1 == 0 es un comportamiento definido.
```

### Unsigned overflow → Wrapping (definido)

```c
unsigned int u = UINT_MAX;   // 4294967295
unsigned int v = u + 1;      // 0 (wrapping definido)

// El estándar dice:
// "A computation involving unsigned operands can never overflow,
//  because a result that cannot be represented by the resulting
//  unsigned integer type is reduced modulo 2^N"

// Es decir: resultado = (resultado real) % 2^N
// UINT_MAX + 1 = 4294967296 % 4294967296 = 0
```

```c
// Wrapping funciona en ambas direcciones:
unsigned int u = 0;
u = u - 1;               // 4294967295 (UINT_MAX)
// 0 - 1 = -1, que mod 2^32 = 4294967295

// Esto es útil y está definido, pero puede ser un bug
// si no era intencional.
```

## Conversiones implícitas peligrosas

### Signed a unsigned

```c
// Cuando asignas un signed negativo a unsigned:
int s = -1;
unsigned int u = s;       // u = 4294967295 (UINT_MAX)
// -1 se reinterpreta como unsigned: todos los bits en 1

printf("%u\n", u);        // 4294967295

// El compilador hace la conversión sin warning (a menos
// que uses -Wconversion)
```

### Comparación signed vs unsigned

```c
// TRAMPA clásica:
int s = -1;
unsigned int u = 0;

if (s < u) {
    printf("negativo es menor\n");    // NO se ejecuta
} else {
    printf("negativo es mayor?\n");   // se ejecuta
}

// ¿Por qué? Cuando comparas signed con unsigned,
// el signed se CONVIERTE a unsigned primero:
// -1 → 4294967295 (UINT_MAX)
// 4294967295 < 0 → false
```

```c
// -Wall detecta esto:
// warning: comparison of integer expressions of different signedness:
//          'int' and 'unsigned int'

// Soluciones:
// 1. No mezclar signed y unsigned en comparaciones
// 2. Convertir explícitamente (con verificación):
if (s >= 0 && (unsigned int)s < u) {
    // ahora es correcto
}
```

### size_t y la trampa del loop descendente

```c
// size_t es unsigned (siempre):
size_t len = strlen(s);    // unsigned

// TRAMPA: loop descendente con unsigned:
for (size_t i = len - 1; i >= 0; i--) {
    // LOOP INFINITO: cuando i llega a 0 y se decrementa,
    // se convierte en SIZE_MAX (un número enorme).
    // i >= 0 SIEMPRE es true para unsigned.
}

// Soluciones:
// 1. Usar un patrón diferente:
for (size_t i = len; i > 0; i--) {
    use(arr[i - 1]);           // usar i-1 como índice
}

// 2. Usar signed si la variable puede ser negativa:
for (ptrdiff_t i = (ptrdiff_t)len - 1; i >= 0; i--) {
    use(arr[i]);
}
```

### Comparar strlen con valor negativo

```c
// strlen retorna size_t (unsigned):
if (strlen(s) >= -1) {
    // SIEMPRE true
    // -1 se convierte a SIZE_MAX
    // cualquier strlen >= SIZE_MAX... no, SIZE_MAX >= SIZE_MAX es true
}

// Más sutil:
int n = -5;
if (strlen(s) > n) {
    // SIEMPRE true si n es negativo:
    // n se convierte a unsigned → número enorme
    // strlen nunca es mayor que SIZE_MAX... pero SIZE_MAX
    // es el -1 unsigned, y strlen(s) < SIZE_MAX generalmente
    // En realidad: depende del valor exacto. El punto es que
    // la comparación no hace lo que esperas.
}
```

## Reglas de conversión aritmética

Cuando se mezclan tipos en una expresión, C aplica las
"usual arithmetic conversions":

```c
// 1. Si algún operando es long double → el otro se convierte a long double
// 2. Si alguno es double → el otro se convierte a double
// 3. Si alguno es float → el otro se convierte a float
// 4. Si no hay flotantes, se aplica integer promotion primero
//    (tipos menores que int se promueven a int)
// 5. Luego, entre los dos tipos resultantes:
//    - Si ambos son signed o ambos unsigned → al de mayor rango
//    - Si el unsigned tiene rango >= que el signed → al unsigned
//    - Si el signed puede representar todos los valores del unsigned
//      → al signed
//    - Si no → al unsigned correspondiente al signed
```

```c
// Ejemplos prácticos:
int s = -1;
unsigned int u = 1;
// s + u → s se convierte a unsigned → -1 se convierte a UINT_MAX
// resultado es unsigned: UINT_MAX + 1 = 0

long ls = -1;
unsigned int ui = 1;
// En LP64 (Linux 64-bit): long es 8 bytes, unsigned int es 4 bytes
// long puede representar todos los unsigned int → resultado es long
// ls + ui = -1 + 1 = 0 (como long, correcto)

// En ILP32 o LLP64: long es 4 bytes = unsigned int
// long NO puede representar todos los unsigned int
// → resultado es unsigned long
// Resultado diferente en diferentes plataformas
```

## Cuándo usar unsigned

```c
// USAR unsigned para:

// 1. Contadores y tamaños que nunca son negativos:
size_t count = 0;
size_t length = strlen(s);

// 2. Operaciones de bits (máscaras, flags):
unsigned int flags = 0;
flags |= FLAG_READ;          // set bit
flags &= ~FLAG_WRITE;        // clear bit

// 3. Cuando necesitas wrapping definido:
unsigned int hash = 0;
hash = hash * 31 + c;        // overflow = wrapping (deseado)

// 4. Cuando el protocolo/formato lo requiere:
uint16_t port = 8080;
uint32_t ipv4 = 0xC0A80001;  // 192.168.0.1

// NO usar unsigned para:

// 1. Variables que podrían ser negativas (errores comunes):
unsigned int balance = get_balance();
balance -= 100;              // si balance < 100 → wrapping, no negativo

// 2. Loops descendentes (trampa del wrapping):
for (unsigned int i = 10; i >= 0; i--) { } // loop infinito

// 3. Diferencias que podrían ser negativas:
unsigned int diff = a - b;   // si a < b → número enorme, no negativo
```

## -Wconversion y -Wsign-conversion

```bash
# GCC tiene warnings específicos para estas trampas:

# -Wsign-conversion: warn al convertir entre signed y unsigned
gcc -Wall -Wextra -Wsign-conversion main.c -o main

# -Wconversion: warn por cualquier conversión implícita que
# podría cambiar el valor (incluye sign conversion)
gcc -Wall -Wextra -Wconversion main.c -o main

# -Wsign-compare: warn al comparar signed con unsigned
# (incluido en -Wall)
```

```c
// Con -Wconversion:
int x = 42;
unsigned int u = x;        // warning: conversion from 'int' to 'unsigned int'
                           // may change the sign of the result

// Con -Wsign-compare (en -Wall):
int s = -1;
unsigned int u2 = 0;
if (s < u2) { }           // warning: comparison of integer expressions
                           // of different signedness
```

## Patrones seguros

```c
// 1. Verificar antes de convertir:
int to_unsigned(int s) {
    if (s < 0) {
        // manejar error
        return 0;
    }
    return (unsigned int)s;    // conversión segura
}

// 2. Verificar antes de restar unsigned:
unsigned int safe_sub(unsigned int a, unsigned int b) {
    if (b > a) {
        // manejar underflow
        return 0;
    }
    return a - b;
}

// 3. Verificar overflow antes de sumar signed:
#include <limits.h>
int safe_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) {
        // overflow
        return INT_MAX;  // o manejar error
    }
    if (b < 0 && a < INT_MIN - b) {
        // underflow
        return INT_MIN;
    }
    return a + b;
}
```

## Tabla resumen

| Aspecto | signed | unsigned |
|---|---|---|
| Rango | -2^(N-1) a 2^(N-1)-1 | 0 a 2^N - 1 |
| Overflow | UB | Wrapping (definido) |
| Default de int | signed | necesita keyword |
| Comparación con otro tipo | Se convierte a unsigned | Se queda unsigned |
| Uso en loops descendentes | Seguro (puede ser < 0) | Trampa (nunca < 0) |
| Operaciones de bits | Comportamiento definido para ambos, pero unsigned es más claro |

---

## Ejercicios

### Ejercicio 1 — Conversión signed-unsigned

```c
// ¿Qué imprime este programa? Predecir antes de ejecutar.
#include <stdio.h>

int main(void) {
    int s = -1;
    unsigned int u = 1;

    if (s < u)
        printf("s < u\n");
    else
        printf("s >= u\n");

    printf("s + u = %u\n", s + u);

    return 0;
}
```

### Ejercicio 2 — Loop infinito

```c
// ¿Por qué este loop es infinito?
// Corregirlo de dos formas diferentes.

#include <stdio.h>

int main(void) {
    unsigned int i;
    for (i = 10; i >= 0; i--) {
        printf("%u\n", i);
    }
    return 0;
}
```

### Ejercicio 3 — safe_subtract

```c
// Implementar una función que reste dos unsigned int
// y retorne 0 si el resultado sería negativo.
// Verificar con: safe_subtract(5, 10) → 0
//                safe_subtract(10, 5) → 5
```
