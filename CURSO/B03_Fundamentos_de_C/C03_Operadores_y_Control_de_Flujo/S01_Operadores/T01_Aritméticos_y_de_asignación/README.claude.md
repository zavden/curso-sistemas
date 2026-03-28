# T01 — Operadores aritméticos y de asignación

> **Sin erratas detectadas** en el material fuente.

---

## 1. Operadores aritméticos

```c
int a = 17, b = 5;

a + b     // 22   suma
a - b     // 12   resta
a * b     // 85   multiplicación
a / b     // 3    división entera (trunca hacia cero)
a % b     // 2    módulo (resto de la división)

-a        // -17  negación unaria
+a        // 17   plus unario (casi nunca se usa)
```

### División entera

La división entre enteros **trunca hacia cero** (no redondea). Esto es distinto de Python, donde `//` redondea hacia menos infinito:

```c
 17 / 5   //  3   (no 3.4, no 4)
-17 / 5   // -3   (no -4 — trunca hacia cero)
 17 / -5  // -3
-17 / -5  //  3
```

Para obtener división real, al menos un operando debe ser `double` (o `float`):

```c
17.0 / 5        // 3.4
(double)17 / 5  // 3.4
17 / 5.0        // 3.4
```

División por cero:

```c
17 / 0      // UB para enteros (señal SIGFPE en la mayoría de sistemas)
17.0 / 0.0  // inf (IEEE 754, no es UB)
```

### Módulo (%)

El signo del resultado sigue al **dividendo** (operando izquierdo), garantizado desde C99:

```c
 17 %  5   //  2
-17 %  5   // -2   (signo de -17)
 17 % -5   //  2   (signo de 17)
-17 % -5   // -2   (signo de -17)
```

**Invariante fundamental:** `(a/b)*b + a%b == a` — siempre se cumple. Es la definición formal que conecta división y módulo.

Usos comunes:

```c
n % 2 == 0          // ¿n es par?
n % 10              // último dígito
index % ARRAY_SIZE  // índice circular (wrap around)
```

---

## 2. Operadores de asignación

### Asignación simple y compuesta

```c
int x = 10;

x = 20;       // asignación simple

// Asignación compuesta — operación + asignación en uno:
x += 5;       // x = x + 5   → 25
x -= 3;       // x = x - 3   → 22
x *= 2;       // x = x * 2   → 44
x /= 4;       // x = x / 4   → 11
x %= 3;       // x = x % 3   → 2

// También existen para bitwise:
x &= 0xFF;   // x = x & 0xFF
x |= 0x01;   // x = x | 0x01
x ^= mask;   // x = x ^ mask
x <<= 2;     // x = x << 2
x >>= 1;     // x = x >> 1
```

### La asignación es una expresión

La asignación retorna el valor asignado, lo que permite encadenar:

```c
int a, b, c;
a = b = c = 0;     // c=0 retorna 0, b=0 retorna 0, a=0

// Patrón útil — asignar y comparar en una expresión:
int n;
while ((n = read_input()) > 0) {
    process(n);
}
```

---

## 3. Incremento y decremento

```c
int x = 5;

// Prefijo — incrementa ANTES de usar el valor:
int a = ++x;    // x se incrementa a 6, a recibe 6

// Postfijo — incrementa DESPUÉS de usar el valor:
x = 5;
int b = x++;    // b recibe 5, luego x se incrementa a 6

// Decremento — misma lógica:
x = 5;
int c = --x;    // x = 4, c = 4
x = 5;
int d = x--;    // d = 5, x = 4
```

En sentencias independientes, **no hay diferencia**:

```c
x++;    // incrementa x
++x;    // incrementa x (mismo efecto)
```

**Regla crítica — nunca modificar la misma variable dos veces sin sequence point:**

```c
int i = 0;
int bad = i++ + i++;    // UB: modifica i dos veces
int bad2 = ++i + ++i;   // UB
```

---

## 4. Precedencia y asociatividad

La **precedencia** determina qué operador se evalúa primero. La **asociatividad** determina el orden cuando hay operadores de la misma precedencia.

```c
2 + 3 * 4       // 14 (no 20) — * antes que +
2 * 3 + 4 * 5   // 26 — (2*3) + (4*5)
10 - 3 - 2      // 5 — (10-3) - 2 (izquierda a derecha)
a = b = 5       // a = (b = 5) — derecha a izquierda
```

### Tabla de precedencia completa

```
Precedencia  Operador                         Asociatividad
──────────────────────────────────────────────────────────────
1 (máxima)   () [] -> .  postfix ++ --       izquierda → derecha
2            prefix ++ --  + - ! ~           derecha → izquierda
             (type) * & sizeof
3            * / %                           izquierda → derecha
4            + -                             izquierda → derecha
5            << >>                           izquierda → derecha
6            < <= > >=                       izquierda → derecha
7            == !=                           izquierda → derecha
8            &                               izquierda → derecha
9            ^                               izquierda → derecha
10           |                               izquierda → derecha
11           &&                              izquierda → derecha
12           ||                              izquierda → derecha
13           ?:                              derecha → izquierda
14           = += -= *= /= %= etc.           derecha → izquierda
15 (mínima)  ,                               izquierda → derecha
```

### Trampas de precedencia

```c
// TRAMPA 1: bitwise vs comparación
if (x & MASK == 0) { }
// Se parsea como: x & (MASK == 0)  ← == tiene mayor precedencia que &
// Correcto: if ((x & MASK) == 0)

// TRAMPA 2: shift vs suma
x << 2 + 1
// Se parsea como: x << (2 + 1) = x << 3  ← + tiene mayor precedencia que <<
// Si querías (x << 2) + 1, usar paréntesis

// TRAMPA 3: asignación en condición
if (x = 5) { }
// Asigna 5 a x, luego evalúa 5 como true
// Probablemente querías: if (x == 5)
// -Wall genera warning para esto
```

**Regla práctica:** ante la duda, usar paréntesis. La legibilidad importa más que memorizar la tabla.

---

## 5. Promoción de enteros

Antes de cualquier operación aritmética, `char` y `short` se promueven automáticamente a `int` (o `unsigned int`):

```c
char a = 100;
char b = 100;
// a + b → ambos se promueven a int → 200 (int)
// sizeof(a + b) == 4, no 1

char c = a + b;    // 200 no cabe en signed char (rango -128..127)
                   // → truncamiento, implementation-defined
                   // probablemente -56 (200 - 256)
```

La promoción evita overflows en operaciones intermedias:

```c
unsigned char x = 255;
unsigned char y = 1;
int result = x + y;     // 256 (int, no unsigned char)
// No hay wrapping porque la suma se hizo en int
```

Pero al almacenar el resultado en un tipo pequeño, se trunca:

```c
unsigned char a = 200;
unsigned char b = 100;
// a + b = 300 como int
unsigned char c = a + b;   // 300 % 256 = 44 (wrapping)
signed char sc = a + b;    // 300 truncado a 8 bits = 44
                           // (44 cabe en signed char, así que es 44)
```

---

## 6. Conversiones aritméticas usuales

Cuando los operandos tienen tipos diferentes después de la promoción, el tipo "menor" se convierte al "mayor":

```c
// float + int    → float
// double + float → double
// long + int     → long
// unsigned int + int → unsigned int (si mismo tamaño)

int i = 5;
double d = 2.5;
i + d       // 7.5 — i se promueve a double
i * d       // 12.5
sizeof(i + d)  // 8 bytes (double)
```

**Peligro: signed + unsigned:**

```c
int i = -1;
unsigned int u = 1;
// i + u → i se convierte a unsigned → UINT_MAX (4294967295) + 1 = 0
// El resultado es unsigned y vale 0

// En LP64 (Linux 64-bit): long + unsigned int
long l = -1;
unsigned int u2 = 1;
// l + u2 → u2 se convierte a long → -1 + 1 = 0
// long es más grande que unsigned int → resultado es long
```

---

## 7. Operadores que no son lo que parecen

```c
// sizeof NO es una función, es un operador:
sizeof(int)      // con tipo: paréntesis obligatorios
sizeof x         // con expresión: paréntesis opcionales
// sizeof se evalúa en compilación (excepto VLAs)

// El cast (type) es un operador unario:
double d = (double)42;    // cast de int a double
int i = (int)3.14;        // cast de double a int (trunca a 3)

// & tiene dos significados:
// Unario: dirección de (&x)
// Binario: AND bitwise (a & b)

// * tiene dos significados:
// Unario: desreferenciar (*p)
// Binario: multiplicación (a * b)

// - tiene dos significados:
// Unario: negación (-x)
// Binario: resta (a - b)
```

El compilador distingue el significado por el contexto: si el operador tiene un operando a la izquierda, es binario; si no, es unario.

---

## Ejercicios

### Ejercicio 1 — División entera y módulo con negativos

```c
#include <stdio.h>

int main(void) {
    printf("%d\n", 23 / 7);
    printf("%d\n", -23 / 7);
    printf("%d\n", 23 % 7);
    printf("%d\n", -23 % 7);
    printf("%d\n", 23 % -7);

    // Verificar invariante:
    int a = -23, b = 7;
    printf("(a/b)*b + a%%b = %d\n", (a / b) * b + a % b);

    return 0;
}
```

**¿Qué imprime cada línea?**

<details>
<summary>Predicción</summary>

```
3           // 23/7 = 3.28... trunca a 3
-3          // -23/7 trunca hacia cero → -3 (no -4)
2           // 23%7 = 2
-2          // -23%7: signo sigue al dividendo (-23) → -2
2           // 23%-7: signo sigue al dividendo (23) → 2
(a/b)*b + a%b = -23   // invariante: (-3)*7 + (-2) = -21 + (-2) = -23 ✓
```

</details>

---

### Ejercicio 2 — Conversión de segundos

```c
#include <stdio.h>

int main(void) {
    int total = 7384;
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;

    printf("%d seconds = %dh %dm %ds\n", total, h, m, s);

    return 0;
}
```

**¿Cuáles son los valores de `h`, `m` y `s`?**

<details>
<summary>Predicción</summary>

```
7384 seconds = 2h 3m 4s
```

- `h = 7384 / 3600 = 2` (trunca)
- `7384 % 3600 = 184` (segundos restantes)
- `m = 184 / 60 = 3`
- `s = 7384 % 60 = 4` (o equivalentemente `184 % 60 = 4`)

Verificación: 2×3600 + 3×60 + 4 = 7200 + 180 + 4 = 7384 ✓

</details>

---

### Ejercicio 3 — Cadena de asignación compuesta

```c
#include <stdio.h>

int main(void) {
    int x = 20;

    x += 10;    // A
    x /= 6;     // B
    x *= 5;     // C
    x -= 3;     // D
    x %= 7;     // E

    printf("x = %d\n", x);

    return 0;
}
```

**¿Cuál es el valor de `x` después de cada paso (A-E)?**

<details>
<summary>Predicción</summary>

- A: `x = 20 + 10 = 30`
- B: `x = 30 / 6 = 5`
- C: `x = 5 * 5 = 25`
- D: `x = 25 - 3 = 22`
- E: `x = 22 % 7 = 1` (22 = 3×7 + 1)

```
x = 1
```

</details>

---

### Ejercicio 4 — Pre vs post incremento

```c
#include <stdio.h>

int main(void) {
    int a = 3;
    int b = a++ + ++a;  // ¿Qué pasa aquí?

    int x = 10;
    int y = x++;
    int z = ++x;

    printf("y = %d, z = %d, x = %d\n", y, z, x);

    return 0;
}
```

**¿La línea de `b` es segura? ¿Qué valores tienen `y`, `z` y `x`?**

<details>
<summary>Predicción</summary>

La línea `int b = a++ + ++a;` es **UB** (comportamiento indefinido). Modifica `a` dos veces sin sequence point. No se puede predecir el resultado — cualquier valor es posible. **Nunca escribir código así.**

Para `y`, `z`, `x` — cada línea es segura porque hay un sequence point (el `;`) entre ellas:

```
y = 10, z = 12, x = 12
```

- `y = x++`: `y` recibe 10 (valor actual), luego `x` sube a 11
- `z = ++x`: `x` sube a 12, luego `z` recibe 12

</details>

---

### Ejercicio 5 — Precedencia

```c
#include <stdio.h>

int main(void) {
    int a = 2 + 3 * 4 - 1;
    int b = 15 / 4 * 4;
    int c = 2 * 3 % 5 + 1;
    int d = 10 - 2 - 3 - 1;

    printf("a = %d\n", a);
    printf("b = %d\n", b);
    printf("c = %d\n", c);
    printf("d = %d\n", d);

    return 0;
}
```

**¿Qué valor tiene cada variable?**

<details>
<summary>Predicción</summary>

```
a = 13      // 2 + (3*4) - 1 = 2 + 12 - 1 = 13
b = 12      // (15/4) * 4 = 3 * 4 = 12  (no 15 — la división entera pierde info)
c = 2       // ((2*3) % 5) + 1 = (6 % 5) + 1 = 1 + 1 = 2
d = 4       // ((10-2) - 3) - 1 = (8-3) - 1 = 5 - 1 = 4  (asociatividad izquierda)
```

Nota sobre `b`: `15 / 4 * 4 ≠ 15`. La división entera trunca `15/4` a 3, y `3 * 4 = 12`. La información perdida por el truncamiento no se recupera al multiplicar.

</details>

---

### Ejercicio 6 — Promoción de enteros

```c
#include <stdio.h>

int main(void) {
    unsigned char a = 250;
    unsigned char b = 10;

    printf("sizeof(a)     = %zu\n", sizeof(a));
    printf("sizeof(a + b) = %zu\n", sizeof(a + b));
    printf("a + b = %d\n", a + b);

    unsigned char c = a + b;
    printf("c = %u\n", c);

    signed char sc = (signed char)(a + b);
    printf("sc = %d\n", sc);

    return 0;
}
```

**¿Cuánto valen `a + b` como int, `c` como unsigned char, y `sc` como signed char?**

<details>
<summary>Predicción</summary>

```
sizeof(a)     = 1
sizeof(a + b) = 4         // promovido a int
a + b = 260               // como int, sin overflow
c = 4                     // 260 % 256 = 4 (wrapping en unsigned char)
sc = 4                    // 260 truncado a 8 bits = 0x04 = 4
                          // 4 está en rango de signed char, así que es 4
```

La suma se hace en `int` (260), pero al almacenar en `unsigned char` se trunca a 8 bits: 260 mod 256 = 4.

</details>

---

### Ejercicio 7 — Conversiones mixtas

```c
#include <stdio.h>

int main(void) {
    int a = 7;
    double b = 2.0;

    double r1 = a / 3;
    double r2 = a / 3.0;
    int r3 = a / b;

    printf("r1 = %f\n", r1);
    printf("r2 = %f\n", r2);
    printf("r3 = %d\n", r3);

    return 0;
}
```

**¿Qué valores tienen `r1`, `r2` y `r3`? ¿Dónde se pierde precisión?**

<details>
<summary>Predicción</summary>

```
r1 = 2.000000    // 7/3 = 2 (división entera), luego 2 se convierte a 2.0
r2 = 2.333333    // 7/3.0 → 7 se promueve a double → 7.0/3.0 = 2.333...
r3 = 3           // 7/2.0 = 3.5 (double), truncado a int → 3
```

- `r1`: la precisión se pierde **antes** de asignar — `a / 3` ya es 2 como `int`
- `r2`: un operando es `double` → la división es real
- `r3`: la división da 3.5 como `double`, pero al almacenar en `int` se trunca a 3

</details>

---

### Ejercicio 8 — Trampa de precedencia con bitwise

```c
#include <stdio.h>

int main(void) {
    int x = 6;        // 0b0110
    int mask = 2;     // 0b0010

    int result_bad  = x & mask == 2;
    int result_good = (x & mask) == 2;

    printf("Sin paréntesis: x & mask == 2 → %d\n", result_bad);
    printf("Con paréntesis: (x & mask) == 2 → %d\n", result_good);

    return 0;
}
```

**¿Por qué dan resultados diferentes? Compilar con `-Wall` para ver el warning.**

<details>
<summary>Predicción</summary>

```
Sin paréntesis: x & mask == 2 → 2
Con paréntesis: (x & mask) == 2 → 1
```

- `x & mask == 2` se parsea como `x & (mask == 2)` porque `==` (nivel 7) tiene mayor precedencia que `&` (nivel 8). `mask == 2` es `1` (true), y `6 & 1 = 0b0110 & 0b0001 = 0b0010 = 2`.
- `(x & mask) == 2`: primero `6 & 2 = 0b0110 & 0b0010 = 0b0010 = 2`, luego `2 == 2` = `1` (true).

GCC con `-Wall` advierte: `suggest parentheses around comparison in operand of '&'`.

</details>

---

### Ejercicio 9 — Signed vs unsigned en expresión

```c
#include <stdio.h>
#include <limits.h>

int main(void) {
    int a = -1;
    unsigned int b = 0;

    if (a < b) {
        printf("a < b  (esperado)\n");
    } else {
        printf("a >= b  (sorpresa!)\n");
    }

    unsigned int sum = (unsigned int)a + b;
    printf("sum = %u\n", sum);

    // Versión segura:
    if (a < 0 || (unsigned int)a < b) {
        printf("Comparación segura: a < b\n");
    }

    return 0;
}
```

**¿Qué imprime la comparación `a < b`? ¿Por qué? ¿Cuánto vale `sum`?**

<details>
<summary>Predicción</summary>

```
a >= b  (sorpresa!)
sum = 4294967295
Comparación segura: a < b
```

- `a < b`: `a` es `int` y `b` es `unsigned int`. Por las conversiones aritméticas usuales, `a` se convierte a `unsigned int`. `-1` como `unsigned int` = `UINT_MAX` (4294967295). `4294967295 < 0` es falso → entra al `else`.
- `sum = (unsigned)(-1) + 0 = UINT_MAX = 4294967295`.
- La comparación segura primero chequea `a < 0` (true), así que no necesita comparar los valores unsigned.

</details>

---

### Ejercicio 10 — Expresión compleja: desglosar paso a paso

```c
#include <stdio.h>

int main(void) {
    int x = 5;
    int y = 3;

    int r = x++ * 2 + --y;

    printf("r = %d, x = %d, y = %d\n", r, x, y);

    return 0;
}
```

**Desglosar la expresión paso a paso. ¿Qué valores tienen `r`, `x` e `y`?**

<details>
<summary>Predicción</summary>

```
r = 12, x = 6, y = 2
```

Desglose:
1. `x++` retorna el valor actual de `x` (5), luego `x` se incrementa a 6
2. `--y` decrementa `y` a 2 y retorna 2
3. Precedencia: `*` antes que `+` → `(x++) * 2 + (--y)`
4. `5 * 2 + 2 = 10 + 2 = 12`

Nota: esta expresión es segura porque modifica **variables distintas** (`x` e `y`). El problema de UB solo ocurre cuando se modifica la **misma** variable dos veces sin sequence point.

</details>
