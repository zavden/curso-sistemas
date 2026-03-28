# T04 — Signed vs unsigned

## Erratas detectadas en el material original

| Archivo | Línea(s) | Error | Corrección |
|---------|----------|-------|------------|
| `README.md` | 167–171 | `strlen(s) >= -1` descrito como "SIEMPRE true". `-1` se convierte a `SIZE_MAX`, y `strlen(s) >= SIZE_MAX` es **casi siempre false** (lo opuesto) | El resultado correcto es "casi siempre false", no "siempre true" |
| `README.md` | 174–178 | `strlen(s) > n` con `n = -5` descrito como "SIEMPRE true si n es negativo". `-5` como `size_t` = `SIZE_MAX - 4`; `strlen(s) > SIZE_MAX - 4` es **false** para cualquier string práctico | El resultado correcto es "casi siempre false", lo opuesto a lo afirmado |
| `README.md` | 291–297 | Función `to_unsigned` declara retorno `int` pero pretende devolver `unsigned int` — el cast en línea 296 se deshace implícitamente al retornar como `int` | El tipo de retorno debe ser `unsigned int` |

---

## 1. La diferencia fundamental

Signed y unsigned usan los mismos bits pero los interpretan de forma diferente. La representación dominante en hardware moderno es **complemento a dos** (C11 no lo mandaba; C23 lo formalizó como la única representación permitida):

```c
// Unsigned: solo positivos, rango 0 a 2^N - 1
unsigned char uc = 200;    // 200 (valid: 0–255)

// Signed: positivos y negativos, rango -2^(N-1) a 2^(N-1) - 1
signed char sc = 200;      // implementation-defined: -56 en complemento a dos
```

```
Complemento a dos (8 bits):

Bits        unsigned    signed
00000000    0           0
00000001    1           1
01111111    127         127       ← máximo positivo signed
10000000    128         -128      ← aquí divergen
10000001    129         -127
11001000    200         -56
11111111    255         -1
```

El bit más significativo (MSB) determina la divergencia:
- MSB = 0 → ambas interpretaciones coinciden (0 a 127 para 8 bits)
- MSB = 1 → unsigned ve valores positivos altos, signed ve negativos

Para convertir mentalmente de unsigned a signed (cuando MSB = 1): invertir todos los bits y sumar 1 para obtener la magnitud. `11001000` → `00110111` (55) + 1 = 56, por tanto signed = -56.

---

## 2. Overflow: UB signed vs wrapping unsigned

Esta es la diferencia más importante desde el punto de vista del programador.

### Unsigned overflow → wrapping definido

```c
unsigned int u = UINT_MAX;   // 4294967295
unsigned int v = u + 1;      // 0 — wrapping definido por el estándar

// El estándar C11 §6.2.5p9:
// "A computation involving unsigned operands can never overflow,
//  because a result that cannot be represented by the resulting
//  unsigned integer type is reduced modulo 2^N"

// UINT_MAX + 1 = 4294967296 % 4294967296 = 0
// 0 - 1 = -1, que mod 2^32 = 4294967295 (UINT_MAX)
```

### Signed overflow → Undefined Behavior

```c
int x = INT_MAX;          // 2147483647
int y = x + 1;            // UB: signed integer overflow

// El compilador puede:
// 1. Producir -2147483648 (wrapping — lo que "esperas")
// 2. Producir 0
// 3. Eliminar código que dependa de este resultado
// 4. Cualquier cosa — es UB
```

### El peligro real del UB: optimización del compilador

```c
// ¿Es x + 1 > x siempre true?
int always_increases_signed(int x) {
    return x + 1 > x;
}

unsigned int always_increases_unsigned(unsigned int x) {
    return x + 1 > x;
}
```

El compilador transforma `always_increases_signed` a `return 1` — razona que para signed, `x + 1 > x` siempre es true porque el overflow "no puede ocurrir" (es UB). No genera suma ni comparación.

Para `always_increases_unsigned`, el compilador **sí** genera la comparación real, porque `UINT_MAX + 1 = 0` es comportamiento definido y `0 > UINT_MAX` es legítimamente false.

Esto se puede verificar con:

```bash
gcc -std=c11 -S -O0 source.c -o source.s
# Buscar la función signed: verás movl $1, %eax (return 1)
# Buscar la función unsigned: verás cmpl/setne (comparación real)
```

Incluso sin flags de optimización (`-O0`), GCC puede aprovechar el UB para simplificar. Con `-O2`, las transformaciones son aún más agresivas.

---

## 3. Conversiones implícitas peligrosas

### Signed a unsigned

```c
int s = -1;
unsigned int u = s;       // u = 4294967295 (UINT_MAX)
// -1 en complemento a dos: todos los bits en 1
// Reinterpretado como unsigned: 2^32 - 1

// GCC no genera warning con -Wall
// Necesitas -Wsign-conversion o -Wconversion para detectarlo
```

### Comparación signed vs unsigned

```c
// TRAMPA clásica:
int neg = -1;
unsigned int zero = 0;

if (neg < zero) {
    printf("negativo es menor\n");    // NO se ejecuta
} else {
    printf("negativo es mayor?\n");   // se ejecuta
}

// ¿Por qué? Las "usual arithmetic conversions" convierten
// el signed a unsigned ANTES de comparar:
// -1 → (unsigned int)-1 → 4294967295
// 4294967295 < 0 → false
```

`-Wall` incluye `-Wsign-compare`, que detecta esta trampa en tiempo de compilación.

### Solución: verificar signo antes de comparar

```c
int s = -1;
unsigned int u = 10;

// INSEGURO:
if (s < u) { ... }                    // conversión implícita, resultado incorrecto

// SEGURO:
if (s < 0 || (unsigned int)s < u) {
    // s es negativo (obviamente menor) o s es positivo y menor que u
}
```

---

## 4. `size_t` y la trampa del loop descendente

### `strlen()` y valores negativos

`size_t` es unsigned (siempre). Comparar `strlen()` con un valor negativo produce resultados invertidos:

```c
size_t len = strlen("Hello");  // 5
int n = -1;

// TRAMPA:
if (len >= n) { ... }
// n se convierte a (size_t)-1 = SIZE_MAX (18446744073709551615 en 64-bit)
// 5 >= SIZE_MAX → false
// El resultado es lo OPUESTO a lo esperado
```

De forma similar, `strlen(s) >= -1` es **casi siempre false** (no "siempre true"), porque `-1` como `size_t` es `SIZE_MAX`, y la longitud de cualquier string práctico es mucho menor que `SIZE_MAX`.

### Loop descendente con unsigned

```c
// LOOP INFINITO:
for (size_t i = len - 1; i >= 0; i--) {
    // Cuando i llega a 0 y se decrementa:
    // 0 - 1 = SIZE_MAX (wrapping)
    // i >= 0 SIEMPRE es true para unsigned
}

// Solución 1: cambiar la condición
for (size_t i = len; i > 0; i--) {
    use(arr[i - 1]);           // usar i-1 como índice
}

// Solución 2: usar un tipo signed
for (ptrdiff_t i = (ptrdiff_t)len - 1; i >= 0; i--) {
    use(arr[i]);
}
```

---

## 5. Reglas de conversión aritmética (usual arithmetic conversions)

Cuando se mezclan tipos en una expresión, C aplica un conjunto de reglas para determinar el tipo del resultado:

```
1. Si algún operando es long double → el otro se convierte a long double
2. Si alguno es double → el otro se convierte a double
3. Si alguno es float → el otro se convierte a float
4. Si no hay flotantes, se aplica integer promotion primero
   (tipos menores que int se promueven a int)
5. Luego, entre los dos tipos resultantes:
   a. Si ambos signed o ambos unsigned → al de mayor rango
   b. Si el unsigned tiene rango >= que el signed → al unsigned
   c. Si el signed puede representar TODOS los valores del unsigned → al signed
   d. Si no → al unsigned correspondiente al signed
```

```c
// Ejemplo 1: int + unsigned int → unsigned int (regla 5b)
int s = -1;
unsigned int u = 1;
// s se convierte a unsigned → UINT_MAX
// UINT_MAX + 1 = 0 (wrapping)

// Ejemplo 2 (LP64, Linux 64-bit): long + unsigned int → long (regla 5c)
long ls = -1;
unsigned int ui = 1;
// long (8 bytes) puede representar todos los unsigned int (4 bytes)
// ls + ui = -1 + 1 = 0 (como long, correcto)

// Ejemplo 3 (LLP64, Windows 64-bit): long + unsigned int → unsigned long (regla 5d)
// long (4 bytes) NO puede representar todos los unsigned int (4 bytes)
// Resultado tipo y comportamiento DIFERENTE a Linux
```

La regla 5c/5d es la que causa portabilidad rota entre plataformas: el mismo código puede dar resultados diferentes en Linux vs Windows.

---

## 6. Cuándo usar unsigned vs signed

### Usar unsigned para:

```c
// 1. Contadores y tamaños que nunca son negativos:
size_t count = 0;
size_t length = strlen(s);

// 2. Operaciones de bits (máscaras, flags):
unsigned int flags = 0;
flags |= FLAG_READ;
flags &= ~FLAG_WRITE;

// 3. Cuando necesitas wrapping definido (hashing, criptografía):
unsigned int hash = 0;
hash = hash * 31 + c;        // overflow = wrapping (deseado)

// 4. Cuando el protocolo/formato lo requiere:
uint16_t port = 8080;
uint32_t ipv4 = 0xC0A80001;  // 192.168.0.1
```

### NO usar unsigned para:

```c
// 1. Variables que podrían ser negativas:
unsigned int balance = get_balance();
balance -= 100;              // si balance < 100 → wrapping, no negativo

// 2. Loops descendentes (trampa del wrapping):
for (unsigned int i = 10; i >= 0; i--) { } // loop infinito

// 3. Diferencias que podrían ser negativas:
unsigned int diff = a - b;   // si a < b → número enorme, no negativo
```

### Tabla resumen

| Aspecto | signed | unsigned |
|---------|--------|----------|
| Rango | -2^(N-1) a 2^(N-1)-1 | 0 a 2^N - 1 |
| Overflow | UB | Wrapping definido (mod 2^N) |
| Default de `int` | signed | necesita keyword `unsigned` |
| En comparación mixta | Se convierte a unsigned | Se queda unsigned |
| Loops descendentes | Seguro (puede ser < 0) | Trampa (nunca < 0) |
| Operaciones de bits | Funciona, pero unsigned es más claro | Preferido |
| Uso principal | Aritmética general | Tamaños, bits, protocolos |

---

## 7. Warnings de GCC para signedness

```bash
# -Wsign-compare (incluido en -Wall):
# Detecta comparaciones entre signed y unsigned
gcc -Wall file.c

# -Wsign-conversion (NO incluido en -Wall ni -Wextra):
# Detecta asignaciones peligrosas entre signed y unsigned
gcc -Wall -Wextra -Wsign-conversion file.c

# -Wconversion (incluye -Wsign-conversion):
# Detecta cualquier conversión implícita que podría cambiar el valor
gcc -Wall -Wextra -Wconversion file.c
```

Ejemplo de lo que detecta cada nivel:

```c
int s = -1;
unsigned int u = 0;

if (s < u) { }         // -Wsign-compare (incluido en -Wall)

int x = 42;
unsigned int y = x;    // -Wsign-conversion (necesita flag explícito)

unsigned int big = UINT_MAX;
int trunc = big;       // -Wsign-conversion (necesita flag explícito)
```

Recomendación para el curso: compilar siempre con al menos `-Wall -Wextra -Wpedantic -Wsign-conversion`.

---

## 8. Patrones seguros

```c
// 1. Verificar antes de convertir signed → unsigned:
unsigned int to_unsigned(int s) {
    if (s < 0) {
        // manejar error (retornar 0, abort, etc.)
        return 0;
    }
    return (unsigned int)s;
}

// 2. Verificar antes de restar unsigned:
unsigned int safe_sub(unsigned int a, unsigned int b) {
    if (b > a) {
        return 0;  // o manejar underflow
    }
    return a - b;
}

// 3. Verificar overflow antes de sumar signed:
#include <limits.h>
int safe_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) {
        return INT_MAX;  // overflow
    }
    if (b < 0 && a < INT_MIN - b) {
        return INT_MIN;  // underflow
    }
    return a + b;
}

// 4. Comparación mixta segura:
_Bool safe_less(int s, unsigned int u) {
    if (s < 0) return 1;             // negativo siempre es menor
    return (unsigned int)s < u;      // ambos unsigned, comparación válida
}
```

---

## Ejercicios

### Ejercicio 1 — Reinterpretación de bits

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex1.c -o ex1
#include <stdio.h>

int main(void) {
    unsigned char uc = 0xA5;       // 10100101 en binario
    signed char sc = (signed char)uc;

    printf("unsigned char: %u\n", uc);
    printf("signed char:   %d\n", sc);
    printf("Same byte? %s\n",
           (unsigned char)sc == uc ? "YES" : "NO");
    return 0;
}
```

**Predicción:** ¿Qué valor tendrá `sc`? ¿Los bits son los mismos?

<details><summary>Respuesta</summary>

```
unsigned char: 165
signed char:   -91
Same byte? YES
```

`0xA5` = 165 unsigned. MSB = 1, así que signed es negativo. Complemento a dos: invertir `10100101` → `01011010` (90) + 1 = 91, por tanto -91. Los bits subyacentes son idénticos; solo cambia la interpretación.

</details>

---

### Ejercicio 2 — Comparación trampa

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
#include <stdio.h>

int main(void) {
    int a = -1;
    unsigned int b = 0;
    unsigned int c = 1;

    printf("a < b: %d\n", a < b);
    printf("a < c: %d\n", a < c);
    printf("a + c: %u\n", a + c);
    return 0;
}
```

**Predicción:** ¿Qué imprime cada línea? ¿Qué warnings produce GCC?

<details><summary>Respuesta</summary>

```
a < b: 0
a < c: 0
a + c: 0
```

GCC produce warnings `-Wsign-compare` en las dos comparaciones. `a` (-1) se convierte a unsigned (UINT_MAX = 4294967295) antes de cada comparación: `4294967295 < 0` → false (0), `4294967295 < 1` → false (0). Para la suma: `UINT_MAX + 1 = 0` (wrapping).

</details>

---

### Ejercicio 3 — Wrapping unsigned vs UB signed

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
#include <stdio.h>
#include <limits.h>

int main(void) {
    unsigned int u = 0;
    u = u - 1;
    printf("0u - 1 = %u\n", u);

    unsigned char uc = 255;
    uc = uc + 1;
    printf("255uc + 1 = %u\n", uc);

    printf("\nUINT_MAX = %u\n", UINT_MAX);
    printf("u == UINT_MAX? %s\n", u == UINT_MAX ? "YES" : "NO");

    return 0;
}
```

**Predicción:** ¿Qué valor tiene `u`? ¿Y `uc`? ¿`u == UINT_MAX`?

<details><summary>Respuesta</summary>

```
0u - 1 = 4294967295
255uc + 1 = 0
UINT_MAX = 4294967295
u == UINT_MAX? YES
```

`0 - 1` en unsigned: -1 mod 2^32 = 4294967295 = UINT_MAX. `255 + 1` en unsigned char: 256 mod 256 = 0. Ambos son wrapping definido. Nota: en la expresión `uc + 1`, `uc` se promueve a `int` (integer promotion), la suma es 256 como `int`, y al asignar de vuelta a `unsigned char` se trunca a 0.

</details>

---

### Ejercicio 4 — Loop infinito y corrección

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
// ADVERTENCIA: este programa tiene un loop infinito. Ctrl+C para detener.
#include <stdio.h>

int main(void) {
    // Este loop es infinito. ¿Por qué?
    for (unsigned int i = 5; i >= 0; i--) {
        printf("%u ", i);
    }
    return 0;
}
```

**Predicción:** ¿Por qué el loop es infinito? Escribe dos versiones corregidas (una con unsigned, otra con signed).

<details><summary>Respuesta</summary>

`i >= 0` siempre es true para `unsigned int` — un unsigned nunca es negativo. Cuando `i` llega a 0 y se decrementa, hace wrapping a UINT_MAX (4294967295), y el loop continúa.

Corrección 1 (mantener unsigned):
```c
for (unsigned int i = 5; i != (unsigned int)-1; i--) {
    printf("%u ", i);
}
// O equivalente y más legible:
for (unsigned int i = 6; i > 0; i--) {
    printf("%u ", i - 1);
}
```

Corrección 2 (usar signed):
```c
for (int i = 5; i >= 0; i--) {
    printf("%d ", i);
}
```

</details>

---

### Ejercicio 5 — `size_t` y comparaciones

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic -Wsign-conversion ex5.c -o ex5
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *str = "Hello";
    size_t len = strlen(str);
    int n = -1;

    printf("len = %zu, n = %d\n", len, n);
    printf("len > n?  %d\n", len > n);
    printf("(size_t)n = %zu\n", (size_t)n);

    return 0;
}
```

**Predicción:** ¿`len > n` es true o false? ¿Por qué? ¿Qué warnings produce?

<details><summary>Respuesta</summary>

```
len = 5, n = -1
len > n?  0
(size_t)n = 18446744073709551615
```

`len > n` es **false** (0). `n` (-1) se convierte a `size_t` antes de la comparación: `(size_t)-1 = SIZE_MAX = 18446744073709551615`. Luego `5 > 18446744073709551615` → false. GCC produce warning `-Wsign-compare` (incluido en `-Wall`).

</details>

---

### Ejercicio 6 — Reglas de conversión aritmética

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex6.c -o ex6
#include <stdio.h>
#include <limits.h>

int main(void) {
    int s = -1;
    unsigned int u = 1;
    long ls = -1;
    unsigned int ui = 1;

    // Expresión 1: int + unsigned int
    unsigned int r1 = s + u;
    printf("int(-1) + unsigned(1) = %u\n", r1);

    // Expresión 2: long + unsigned int (en LP64)
    long r2 = ls + ui;
    printf("long(-1) + unsigned int(1) = %ld\n", r2);

    // Expresión 3: multiplicación
    int a = -2;
    unsigned int b = 3;
    printf("int(-2) * unsigned(3) = %u\n", a * b);

    return 0;
}
```

**Predicción:** ¿Qué resultado da cada expresión? ¿De qué tipo es cada resultado intermedio?

<details><summary>Respuesta</summary>

```
int(-1) + unsigned(1) = 0
long(-1) + unsigned int(1) = 0
int(-2) * unsigned(3) = 4294967290
```

- Expresión 1: `int + unsigned int` → resultado `unsigned int` (regla 5b). `-1` → UINT_MAX. `UINT_MAX + 1 = 0` (wrapping). Resultado = 0.
- Expresión 2: `long + unsigned int` → en LP64, `long` (8 bytes) puede representar todos los `unsigned int` (4 bytes), así que el resultado es `long` (regla 5c). `-1 + 1 = 0` como `long` (correcto).
- Expresión 3: `int * unsigned int` → resultado `unsigned int`. `-2` → UINT_MAX - 1 = 4294967294. `4294967294 * 3` mod 2^32 = 12884901882 mod 4294967296 = 4294967290. Verificación: esto corresponde a `-6` en complemento a dos de 32 bits, lo cual tiene sentido aritméticamente.

</details>

---

### Ejercicio 7 — `safe_subtract`

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
#include <stdio.h>

unsigned int safe_subtract(unsigned int a, unsigned int b) {
    // TODO: implementar. Retornar a - b si a >= b, sino 0.
    return a - b;  // ← PELIGROSO: ¿qué pasa si b > a?
}

int main(void) {
    printf("safe_subtract(10, 5) = %u\n", safe_subtract(10, 5));
    printf("safe_subtract(5, 10) = %u\n", safe_subtract(5, 10));
    printf("safe_subtract(0, 1) = %u\n", safe_subtract(0, 1));
    printf("safe_subtract(100, 100) = %u\n", safe_subtract(100, 100));
    return 0;
}
```

**Predicción:** ¿Qué imprime con la implementación insegura (sin el TODO)? Luego corrígela.

<details><summary>Respuesta</summary>

Sin corregir:
```
safe_subtract(10, 5) = 5
safe_subtract(5, 10) = 4294967291
safe_subtract(0, 1) = 4294967295
safe_subtract(100, 100) = 0
```

Cuando `b > a`, el resultado hace wrapping: `5 - 10 = -5` mod 2^32 = 4294967291. `0 - 1` = UINT_MAX.

Corrección:
```c
unsigned int safe_subtract(unsigned int a, unsigned int b) {
    if (b > a) return 0;
    return a - b;
}
```

Con la corrección: 5, 0, 0, 0.

</details>

---

### Ejercicio 8 — UB del compilador con signed overflow

```c
// Compila con DOS versiones:
//   gcc -std=c11 -Wall -Wextra -O0 ex8.c -o ex8_O0
//   gcc -std=c11 -Wall -Wextra -O2 ex8.c -o ex8_O2
#include <stdio.h>
#include <limits.h>

int check_overflow(int x) {
    if (x + 1 > x)
        return 1;
    return 0;
}

int main(void) {
    printf("check_overflow(42)      = %d\n", check_overflow(42));
    printf("check_overflow(INT_MAX) = %d\n", check_overflow(INT_MAX));
    return 0;
}
```

**Predicción:** ¿Cambia el resultado entre `-O0` y `-O2`? ¿Por qué?

<details><summary>Respuesta</summary>

Con `-O0`:
```
check_overflow(42)      = 1
check_overflow(INT_MAX) = 1
```

Con `-O2`:
```
check_overflow(42)      = 1
check_overflow(INT_MAX) = 1
```

En ambos casos, el resultado es 1 para ambas llamadas. El compilador puede (y suele) optimizar `x + 1 > x` a `return 1` incluso con `-O0`, porque el signed overflow es UB y puede asumir que nunca ocurre. Con `-O2`, la optimización es aún más agresiva y puede eliminar el branch completamente, inlineando un `1` constante. El punto clave: `INT_MAX + 1` en signed es UB, así que el compilador no está obligado a producir el resultado "esperado" de wrapping (-2147483648 > 2147483647 → false → 0).

</details>

---

### Ejercicio 9 — Warnings progresivos

```c
// Compila con 3 conjuntos de flags y compara los warnings:
//   gcc -std=c11 -Wall ex9.c -o ex9
//   gcc -std=c11 -Wall -Wsign-conversion ex9.c -o ex9
//   gcc -std=c11 -Wall -Wconversion ex9.c -o ex9
#include <stdio.h>
#include <limits.h>

int main(void) {
    int s = -1;
    unsigned int u = 0;

    // Caso A: comparación
    if (s < u) {
        printf("A: s < u\n");
    }

    // Caso B: asignación signed → unsigned
    unsigned int x = s;
    printf("B: x = %u\n", x);

    // Caso C: asignación unsigned → signed
    unsigned int big = UINT_MAX;
    int y = big;
    printf("C: y = %d\n", y);

    return 0;
}
```

**Predicción:** ¿Cuántos warnings produce cada conjunto de flags? ¿Cuáles?

<details><summary>Respuesta</summary>

Con `-Wall` solo: **1 warning**
- Caso A: `-Wsign-compare` (comparación signed vs unsigned)

Con `-Wall -Wsign-conversion`: **3 warnings**
- Caso A: `-Wsign-compare`
- Caso B: `-Wsign-conversion` (int → unsigned int, puede cambiar signo)
- Caso C: `-Wsign-conversion` (unsigned int → int, puede cambiar signo)

Con `-Wall -Wconversion`: **3 warnings** (mismos que `-Wsign-conversion`)
- `-Wconversion` incluye `-Wsign-conversion`, así que detecta lo mismo. En otros contextos, `-Wconversion` también detecta pérdida de precisión (e.g., `long` → `int`).

Salida del programa:
```
B: x = 4294967295
C: y = -1
```

Caso A no imprime nada (la condición es false). Caso B: `-1` como unsigned = UINT_MAX. Caso C: UINT_MAX como signed = -1 (mismos bits, complemento a dos).

</details>

---

### Ejercicio 10 — Comparación mixta segura

```c
// Compila con: gcc -std=c11 -Wall -Wextra -Wpedantic -Wsign-conversion ex10.c -o ex10
#include <stdio.h>
#include <stdbool.h>

bool unsafe_less(int s, unsigned int u) {
    return s < u;          // BUG: -1 se convierte a UINT_MAX
}

// TODO: implementar versión segura
bool safe_less(int s, unsigned int u) {
    return s < u;          // ← reemplazar con lógica correcta
}

int main(void) {
    printf("unsafe_less(-1, 0) = %d\n", unsafe_less(-1, 0));
    printf("unsafe_less(5, 10) = %d\n", unsafe_less(5, 10));
    printf("unsafe_less(-1, UINT_MAX) = %d\n", unsafe_less(-1, 4294967295u));

    printf("\nsafe_less(-1, 0) = %d\n", safe_less(-1, 0));
    printf("safe_less(5, 10) = %d\n", safe_less(5, 10));
    printf("safe_less(-1, UINT_MAX) = %d\n", safe_less(-1, 4294967295u));
    printf("safe_less(10, 5) = %d\n", safe_less(10, 5));

    return 0;
}
```

**Predicción:** ¿Qué da la versión insegura para cada caso? Luego implementa `safe_less`.

<details><summary>Respuesta</summary>

Versión insegura:
```
unsafe_less(-1, 0) = 0         ← incorrecto, -1 SÍ es menor que 0
unsafe_less(5, 10) = 1         ← correcto
unsafe_less(-1, UINT_MAX) = 0  ← incorrecto, -1 se convierte a UINT_MAX → iguales
```

Implementación segura:
```c
bool safe_less(int s, unsigned int u) {
    if (s < 0) return true;            // negativo siempre es menor que unsigned
    return (unsigned int)s < u;        // ambos positivos, comparación segura
}
```

Con la corrección:
```
safe_less(-1, 0) = 1         ← correcto
safe_less(5, 10) = 1         ← correcto
safe_less(-1, UINT_MAX) = 1  ← correcto, -1 es menor que cualquier unsigned
safe_less(10, 5) = 0         ← correcto
```

</details>
