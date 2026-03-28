# T03 — Lógicos y relacionales

## Operadores relacionales

Comparan dos valores y retornan un `int`: 1 (verdadero) o
0 (falso):

```c
int a = 10, b = 20;

a == b     // 0 (false) — igualdad
a != b     // 1 (true)  — desigualdad
a <  b     // 1 (true)  — menor que
a >  b     // 0 (false) — mayor que
a <= b     // 1 (true)  — menor o igual
a >= b     // 0 (false) — mayor o igual
```

```c
// El resultado es int, no bool:
int result = (10 > 5);    // result = 1

// En C, cualquier valor distinto de cero es "verdadero":
if (42)  { }    // se ejecuta (42 != 0)
if (-1)  { }    // se ejecuta (-1 != 0)
if (0)   { }    // NO se ejecuta (0 == false)
if (0.0) { }    // NO se ejecuta

// Punteros:
if (ptr) { }    // equivalente a: if (ptr != NULL)
if (!ptr) { }   // equivalente a: if (ptr == NULL)
```

### Trampas de comparación

```c
// TRAMPA 1: = vs ==
if (x = 5) { }     // ASIGNACIÓN, no comparación
                    // Asigna 5 a x, luego evalúa 5 → true
                    // -Wall genera warning

// TRAMPA 2: encadenar comparaciones
if (1 < x < 10) { }
// Se parsea como: (1 < x) < 10
// (1 < x) es 0 o 1, y ambos son < 10 → siempre true
// Correcto: if (1 < x && x < 10)

// TRAMPA 3: comparar floats con ==
if (0.1 + 0.2 == 0.3) { }    // false (errores de redondeo)
// Correcto: if (fabs(0.1 + 0.2 - 0.3) < 1e-9)

// TRAMPA 4: comparar signed con unsigned
int s = -1;
unsigned int u = 0;
if (s < u) { }     // false (-1 se convierte a UINT_MAX)
```

## Operadores lógicos

```c
&&     // AND lógico
||     // OR lógico
!      // NOT lógico
```

```c
int a = 1, b = 0;

a && b     // 0 (false) — ambos deben ser true
a || b     // 1 (true)  — al menos uno true
!a         // 0 — inversión
!b         // 1

// Trabajan con "verdadero" (≠0) y "falso" (==0):
5 && 3     // 1 (ambos son no-cero)
5 || 0     // 1 (al menos uno no-cero)
!5         // 0
!0         // 1
```

## Short-circuit evaluation

Los operadores `&&` y `||` **no evalúan** el segundo operando
si el resultado ya está determinado por el primero:

```c
// && — si el primero es false, NO evalúa el segundo:
if (ptr != NULL && *ptr > 10) {
    // Si ptr es NULL, *ptr NO se evalúa
    // Sin short-circuit, *ptr con ptr=NULL sería UB
}

// || — si el primero es true, NO evalúa el segundo:
if (is_cached || load_from_disk()) {
    // Si is_cached es true, load_from_disk() NO se llama
}
```

```c
// Short-circuit es una GARANTÍA del estándar, no una optimización.
// Puedes depender de este comportamiento:

// Patrón seguro — verificar antes de usar:
if (index >= 0 && index < size && arr[index] > 0) {
    // Cada condición se evalúa solo si la anterior fue true
    // Si index < 0, arr[index] no se evalúa (evita out-of-bounds)
}

// Patrón seguro — fallback:
FILE *f = fopen("config.txt", "r");
if (f == NULL && (f = fopen("default.txt", "r")) == NULL) {
    // Si el primer fopen funciona, no intenta el segundo
}
```

### Efectos secundarios y short-circuit

```c
// CUIDADO con side effects en condiciones:
int x = 0, y = 0;

if (x++ || y++) {
    // x se incrementa siempre (se evalúa primero)
    // y se incrementa SOLO si x++ fue 0 (false)
}
// Si x era 0: x++ = 0 (false), evalúa y++: x=1, y=1
// Si x era 5: x++ = 5 (true), NO evalúa y++: x=6, y=0

// Esto puede ser confuso. Es mejor no depender de side effects
// en condiciones con short-circuit.
```

## & vs && — Diferencia crítica

```c
// & es AND BITWISE — opera bit a bit, evalúa AMBOS operandos
// && es AND LÓGICO — opera con verdadero/falso, short-circuit

int a = 6;     // 0b110
int b = 3;     // 0b011

a & b          // 2 (0b010) — AND bit a bit
a && b         // 1 (true) — ambos son no-cero

// Diferencia práctica:
int x = 2, y = 4;
x & y          // 0 (0b010 & 0b100 = 0b000)
x && y         // 1 (ambos son no-cero → true)

// Short-circuit:
int *p = NULL;
p & 1          // lee el valor de p como entero, hace AND con 1
               // (warning por mezcla de puntero e int)
p && *p        // si p es NULL, *p NO se evalúa (seguro)
p & *p         // si p es NULL, *p SÍ se evalúa → crash
```

```c
// Lo mismo para | vs ||:

int a = 0, b = 5;
a | b          // 5 (OR bitwise)
a || b         // 1 (true — al menos uno no-cero)

// | evalúa AMBOS operandos siempre
// || tiene short-circuit
```

## Expresiones condicionales comunes

```c
// Verificar rango:
if (x >= MIN && x <= MAX) { }

// Verificar puntero antes de usar:
if (ptr && ptr->value > 0) { }

// Verificar múltiples condiciones:
if (is_valid && !is_expired && has_permission) { }

// Negar una condición compuesta (De Morgan):
// !(A && B)  equivale a  !A || !B
// !(A || B)  equivale a  !A && !B

if (!(x > 0 && y > 0)) { }
// equivale a:
if (x <= 0 || y <= 0) { }
```

## Valores "truthy" y "falsy"

```c
// En C, "falso" es exactamente: 0, 0.0, NULL, '\0'
// Todo lo demás es "verdadero"

if (42)      { }    // true
if (-1)      { }    // true
if (0.001)   { }    // true
if ('a')     { }    // true (valor ASCII 97)
if (ptr)     { }    // true si ptr != NULL

if (0)       { }    // false
if (0.0)     { }    // false
if ('\0')    { }    // false (valor 0)
if (NULL)    { }    // false

// stdbool.h define:
// true  = 1
// false = 0
// Pero cualquier no-cero se trata como true en condiciones.
```

### Comparar con true/false explícitamente

```c
// NO comparar con true o 1:
int result = some_function();    // puede retornar cualquier no-cero

if (result == 1) { }       // PELIGROSO: result podría ser 2 o -1 (true, pero != 1)
if (result == true) { }    // IGUAL de peligroso (true es 1)
if (result) { }            // CORRECTO: cualquier no-cero es true

// SÍ se puede comparar con false/0:
if (result == 0) { }       // OK
if (!result) { }           // OK — equivalente

// Para punteros:
if (ptr != NULL) { }       // OK — explícito
if (ptr) { }               // OK — idiomático en C
// if (ptr == true) { }    // INCORRECTO
```

## Precedencia de lógicos y relacionales

```c
// De mayor a menor precedencia:
// !          (NOT, unario)
// < <= > >=  (relacionales)
// == !=      (igualdad)
// &&         (AND lógico)
// ||         (OR lógico)

// Esto significa:
a > 5 && b < 10 || c == 0
// se parsea como:
((a > 5) && (b < 10)) || (c == 0)
// && tiene mayor precedencia que ||

// Para claridad, usar paréntesis siempre en condiciones complejas:
if ((a > 5 && b < 10) || c == 0) { }
```

```c
// TRAMPA: & tiene MENOR precedencia que ==
if (flags & FLAG_READ == FLAG_READ) { }
// se parsea como: flags & (FLAG_READ == FLAG_READ) = flags & 1
// Correcto: if ((flags & FLAG_READ) == FLAG_READ)
```

---

## Ejercicios

### Ejercicio 1 — Short-circuit

```c
// ¿Qué imprime este programa? Predecir antes de ejecutar.

#include <stdio.h>

int check(int x) {
    printf("check(%d) ", x);
    return x;
}

int main(void) {
    printf("A: %d\n", check(0) && check(1));
    printf("B: %d\n", check(1) && check(0));
    printf("C: %d\n", check(0) || check(1));
    printf("D: %d\n", check(1) || check(0));
    return 0;
}
```

### Ejercicio 2 — & vs &&

```c
// Predecir el resultado de cada expresión:
int a = 6, b = 3;

a & b       // ¿?
a && b      // ¿?

int c = 2, d = 4;
c & d       // ¿?
c && d      // ¿?

int e = 0, f = 5;
e & f       // ¿?
e && f      // ¿?
```

### Ejercicio 3 — Validación segura

```c
// Escribir una función safe_divide(int a, int b, int *result)
// que retorne 1 si la división fue exitosa, 0 si no.
// Usar short-circuit para verificar b != 0 y result != NULL.

// Uso: if (safe_divide(10, 3, &r)) { printf("%d\n", r); }
```
