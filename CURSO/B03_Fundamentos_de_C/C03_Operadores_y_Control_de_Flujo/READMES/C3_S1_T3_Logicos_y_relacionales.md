# T03 — Operadores lógicos y relacionales

> **Sin erratas detectadas** en el material fuente.

---

## 1. Operadores relacionales

Comparan dos valores y retornan un **`int`**: 1 (verdadero) o 0 (falso):

```c
int a = 10, b = 20;

a == b     // 0 (false) — igualdad
a != b     // 1 (true)  — desigualdad
a <  b     // 1 (true)  — menor que
a >  b     // 0 (false) — mayor que
a <= b     // 1 (true)  — menor o igual
a >= b     // 0 (false) — mayor o igual
```

El resultado es `int`, no `bool`. `sizeof(a == b)` es 4 bytes.

---

## 2. Valores truthy y falsy

En C no existe un tipo booleano nativo (hasta C99/`stdbool.h`). Las condiciones trabajan con enteros:

| Falso (0) | Verdadero (≠0) |
|---|---|
| `0` | `42`, `-1`, `1` |
| `0.0` | `0.001`, `3.14` |
| `'\0'` (ASCII 0) | `'0'` (ASCII 48), `'a'` (ASCII 97) |
| `NULL` | Cualquier puntero no-NULL |

```c
if (42)  { }    // se ejecuta
if (-1)  { }    // se ejecuta (-1 ≠ 0)
if (0)   { }    // NO se ejecuta

// Punteros:
if (ptr) { }    // equivale a: if (ptr != NULL)
if (!ptr) { }   // equivale a: if (ptr == NULL)
```

### No comparar con `true` o `1`

```c
int result = some_function();    // puede retornar cualquier no-cero

if (result == 1) { }       // PELIGROSO: result podría ser 2 o -1
if (result == true) { }    // IGUAL de peligroso (true es 1)
if (result) { }            // CORRECTO: cualquier no-cero es true

// Comparar con false/0 SÍ es seguro:
if (result == 0) { }       // OK
if (!result) { }           // OK — equivalente
```

---

## 3. Operadores lógicos

```c
&&     // AND lógico — ambos deben ser true
||     // OR lógico  — al menos uno true
!      // NOT lógico — inversión
```

```c
int a = 1, b = 0;

a && b     // 0 (false)
a || b     // 1 (true)
!a         // 0
!b         // 1

// Trabajan con "verdadero" (≠0) y "falso" (==0):
5 && 3     // 1 (ambos no-cero)
5 || 0     // 1 (al menos uno no-cero)
!5         // 0
!0         // 1
```

---

## 4. Short-circuit evaluation

Los operadores `&&` y `||` **no evalúan** el segundo operando si el resultado ya está determinado por el primero. Esto es una **garantía del estándar**, no una optimización.

```c
// && — si el primero es false, NO evalúa el segundo:
if (ptr != NULL && *ptr > 10) {
    // Si ptr es NULL, *ptr NO se evalúa → no hay crash
}

// || — si el primero es true, NO evalúa el segundo:
if (is_cached || load_from_disk()) {
    // Si is_cached es true, load_from_disk() NO se llama
}
```

### Patrón seguro: verificar antes de usar

```c
if (index >= 0 && index < size && arr[index] > 0) {
    // Cada condición se evalúa solo si la anterior fue true
    // Si index < 0, arr[index] no se evalúa (evita out-of-bounds)
}
```

### Side effects y short-circuit

```c
int x = 0, y = 0;

if (x++ || y++) {
    // x++ evalúa a 0 (false), LUEGO x=1
    // Como fue false, || evalúa y++ → y++ evalúa a 0, LUEGO y=1
    // 0 || 0 = false → NO entra al if
    // Resultado: x=1, y=1
}

// Si ahora x=1:
if (x++ || y++) {
    // x++ evalúa a 1 (true), LUEGO x=2
    // Como fue true, || NO evalúa y++ → y queda en 0
    // Resultado: x=2, y=0 — ¡y no se incrementó!
}
```

Es mejor no depender de side effects en condiciones con short-circuit.

---

## 5. `&` vs `&&` — diferencia crítica

| Aspecto | `&` (bitwise) | `&&` (lógico) |
|---|---|---|
| Opera sobre | Bits individuales | Verdadero/falso |
| Resultado | Valor bit a bit | 0 o 1 |
| Evalúa ambos lados | **Siempre** | Solo si necesario (short-circuit) |

```c
int a = 6;     // 0b110
int b = 3;     // 0b011

a & b          // 2 (0b010) — AND bit a bit
a && b         // 1 (true) — ambos no-cero
```

**Caso crítico:**

```c
int c = 2, d = 4;
c & d          // 0 (0b010 & 0b100 = 0b000) — no comparten bits
c && d         // 1 (ambos no-cero → true)
```

Confundir `&` con `&&` cambia completamente el resultado. Lo mismo aplica para `|` vs `||`:

```c
int e = 0, f = 5;
e | f          // 5 (OR bitwise)
e || f         // 1 (true)
```

---

## 6. Trampas de comparación

### Trampa 1: `=` vs `==`

```c
if (x = 5) { }     // ASIGNACIÓN: asigna 5 a x, evalúa 5 → true
if (x == 5) { }    // COMPARACIÓN: verdadero solo si x es 5
// -Wall genera warning para la primera
```

### Trampa 2: comparaciones encadenadas

```c
if (1 < x < 10) { }
// Se parsea como: (1 < x) < 10
// (1 < x) es 0 o 1, ambos son < 10 → SIEMPRE true
// Correcto: if (1 < x && x < 10)
```

### Trampa 3: comparar floats con `==`

```c
if (0.1 + 0.2 == 0.3) { }    // false (errores de redondeo)
// 0.1 + 0.2 = 0.30000000000000004...
// 0.3       = 0.29999999999999998...
// Correcto: if (fabs(0.1 + 0.2 - 0.3) < 1e-9)
```

### Trampa 4: signed vs unsigned

```c
int s = -1;
unsigned int u = 0;
if (s < u) { }     // false: -1 se convierte a UINT_MAX (4294967295)
```

---

## 7. Precedencia y leyes de De Morgan

### Precedencia (de mayor a menor)

```
1. !          (NOT, unario)
2. < <= > >=  (relacionales)
3. == !=      (igualdad)
4. &&         (AND lógico)
5. ||         (OR lógico)
```

Nota: `&&` tiene **mayor precedencia** que `||`:

```c
a > 5 && b < 10 || c == 0
// se parsea como: ((a > 5) && (b < 10)) || (c == 0)
```

Nota: `&` (bitwise) tiene **menor precedencia** que `==`:

```c
if (flags & FLAG_READ == FLAG_READ) { }
// se parsea como: flags & (FLAG_READ == FLAG_READ) = flags & 1
// Correcto: if ((flags & FLAG_READ) == FLAG_READ)
```

### Leyes de De Morgan

```c
!(A && B)  ≡  !A || !B
!(A || B)  ≡  !A && !B
```

```c
// Ejemplo:
if (!(x > 0 && y > 0)) { }
// equivale a:
if (x <= 0 || y <= 0) { }
```

---

## Ejercicios

### Ejercicio 1 — Short-circuit con función de tracing

```c
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

**¿Cuántas veces se llama a `check()` en cada línea? ¿Qué se imprime?**

<details>
<summary>Predicción</summary>

```
check(0) A: 0
check(1) check(0) B: 0
check(0) check(1) C: 1
check(1) D: 1
```

- **A:** `check(0)` retorna 0 (false). Con `&&`, resultado ya determinado → NO llama `check(1)`. 1 llamada.
- **B:** `check(1)` retorna 1 (true). Con `&&`, necesita evaluar segundo → llama `check(0)`. 2 llamadas. `1 && 0 = 0`.
- **C:** `check(0)` retorna 0 (false). Con `||`, necesita evaluar segundo → llama `check(1)`. 2 llamadas. `0 || 1 = 1`.
- **D:** `check(1)` retorna 1 (true). Con `||`, resultado ya determinado → NO llama `check(0)`. 1 llamada.

</details>

---

### Ejercicio 2 — Bitwise vs lógico

```c
#include <stdio.h>

int main(void) {
    int a = 6, b = 3;
    int c = 2, d = 4;
    int e = 0, f = 5;

    printf("6 & 3  = %d\n", a & b);
    printf("6 && 3 = %d\n", a && b);
    printf("2 & 4  = %d\n", c & d);
    printf("2 && 4 = %d\n", c && d);
    printf("0 & 5  = %d\n", e & f);
    printf("0 && 5 = %d\n", e && f);

    return 0;
}
```

**Calcular cada resultado. ¿En cuál caso `&` y `&&` dan resultados diferentes siendo ambos "distintos de cero" o ambos "cero"?**

<details>
<summary>Predicción</summary>

```
6 & 3  = 2     // 0b110 & 0b011 = 0b010
6 && 3 = 1     // ambos no-cero → true
2 & 4  = 0     // 0b010 & 0b100 = 0b000 (no comparten bits)
2 && 4 = 1     // ambos no-cero → true
0 & 5  = 0     // 0b000 & 0b101 = 0b000
0 && 5 = 0     // 0 es falso → short-circuit → false
```

El caso crítico es `2 & 4 = 0` vs `2 && 4 = 1`. Bitwise dice "no comparten bits" (0), pero lógico dice "ambos son no-cero" (1). Si confundes `&` con `&&` en un `if`, obtienes el resultado opuesto.

</details>

---

### Ejercicio 3 — Truthiness de caracteres

```c
#include <stdio.h>

int main(void) {
    char a = 'A';
    char b = '0';
    char c = '\0';
    char d = ' ';
    char e = 0;

    printf("'A'  -> %s\n", a ? "true" : "false");
    printf("'0'  -> %s\n", b ? "true" : "false");
    printf("'\\0' -> %s\n", c ? "true" : "false");
    printf("' '  -> %s\n", d ? "true" : "false");
    printf("0    -> %s\n", e ? "true" : "false");

    return 0;
}
```

**¿Cuáles son true y cuáles false? ¿`'0'` y `0` dan el mismo resultado?**

<details>
<summary>Predicción</summary>

```
'A'  -> true     // ASCII 65 ≠ 0
'0'  -> true     // ASCII 48 ≠ 0 (¡es el carácter cero, no el número cero!)
'\0' -> false    // ASCII 0 = 0
' '  -> true     // ASCII 32 ≠ 0
0    -> false    // 0 = 0
```

`'0'` (el carácter dígito cero, ASCII 48) es **true**. Solo `'\0'` (el carácter nulo, valor numérico 0) y el literal `0` son false. Esta es una trampa frecuente.

</details>

---

### Ejercicio 4 — Side effects en short-circuit

```c
#include <stdio.h>

int main(void) {
    int a = 0, b = 0, c = 0;

    if (a++ && b++) {
        printf("Entro\n");
    }
    printf("Paso 1: a=%d b=%d c=%d\n", a, b, c);

    if (a++ || c++) {
        printf("Entro\n");
    }
    printf("Paso 2: a=%d b=%d c=%d\n", a, b, c);

    return 0;
}
```

**¿Qué valores tienen `a`, `b` y `c` después de cada paso? ¿Se entra al `if`?**

<details>
<summary>Predicción</summary>

**Paso 1:** `a++` evalúa a 0 (a era 0), luego a=1. Como 0 es false y es `&&`, short-circuit → `b++` NO se evalúa. b sigue en 0. No entra al if.

```
Paso 1: a=1 b=0 c=0
```

**Paso 2:** `a++` evalúa a 1 (a era 1), luego a=2. Como 1 es true y es `||`, short-circuit → `c++` NO se evalúa. c sigue en 0. Entra al if.

```
Entro
Paso 2: a=2 b=0 c=0
```

Resultado: `b` y `c` nunca se incrementan porque short-circuit los salta en ambos casos.

</details>

---

### Ejercicio 5 — Comparaciones encadenadas

```c
#include <stdio.h>

int main(void) {
    int x = 50;

    int r1 = (1 < x < 10);
    int r2 = (1 < x && x < 10);
    int r3 = (100 < x < 200);
    int r4 = (100 < x && x < 200);

    printf("x = %d\n", x);
    printf("(1 < x < 10)        = %d\n", r1);
    printf("(1 < x && x < 10)   = %d\n", r2);
    printf("(100 < x < 200)     = %d\n", r3);
    printf("(100 < x && x < 200)= %d\n", r4);

    return 0;
}
```

**¿Cuáles son verdaderas? ¿Coinciden las versiones encadenada y correcta?**

<details>
<summary>Predicción</summary>

```
x = 50
(1 < x < 10)        = 1    // (1 < 50) = 1, luego 1 < 10 = 1 → true (¡INCORRECTO!)
(1 < x && x < 10)   = 0    // 1 < 50 = true, 50 < 10 = false → false (correcto)
(100 < x < 200)     = 1    // (100 < 50) = 0, luego 0 < 200 = 1 → true (¡INCORRECTO!)
(100 < x && x < 200)= 0    // 100 < 50 = false → false (correcto)
```

Las versiones encadenadas dan `true` en ambos casos, lo cual es incorrecto: 50 NO está entre 1 y 10, ni entre 100 y 200. La trampa es que `(a < b < c)` se parsea como `(a < b) < c`, donde `(a < b)` es 0 o 1, y casi siempre `0 o 1 < c` es true.

</details>

---

### Ejercicio 6 — Comparación segura de floats

```c
#include <stdio.h>
#include <math.h>

int main(void) {
    double a = 0.1 + 0.2;
    double b = 0.3;
    double c = 1.0 / 3.0;
    double d = c * 3.0;

    printf("0.1 + 0.2 == 0.3    : %d\n", a == b);
    printf("1/3 * 3   == 1.0    : %d\n", d == 1.0);
    printf("fabs diff (a,b)     : %.20e\n", fabs(a - b));
    printf("fabs diff (d,1.0)   : %.20e\n", fabs(d - 1.0));

    double eps = 1e-9;
    printf("Con epsilon (a≈b)   : %d\n", fabs(a - b) < eps);
    printf("Con epsilon (d≈1.0) : %d\n", fabs(d - 1.0) < eps);

    return 0;
}
```

**¿Las comparaciones con `==` son true o false? ¿Y con epsilon?**

<details>
<summary>Predicción</summary>

```
0.1 + 0.2 == 0.3    : 0    // false — diferente representación binaria
1/3 * 3   == 1.0    : 0    // false — 1/3 no es exacto en binario
fabs diff (a,b)     : ~5.5e-17   // error del orden de 10⁻¹⁷
fabs diff (d,1.0)   : ~1.1e-16   // error del orden de 10⁻¹⁶
Con epsilon (a≈b)   : 1    // true — 5.5e-17 < 1e-9
Con epsilon (d≈1.0) : 1    // true — 1.1e-16 < 1e-9
```

Los errores de punto flotante son muy pequeños (~10⁻¹⁶), pero suficientes para que `==` falle. Un epsilon de `1e-9` los absorbe sin problemas. La regla: nunca `==` con floats, siempre `fabs(a - b) < epsilon`.

</details>

---

### Ejercicio 7 — Precedencia de `&&` vs `||`

```c
#include <stdio.h>

int main(void) {
    int a = 0, b = 1, c = 1;

    int r1 = a || b && c;
    int r2 = (a || b) && c;
    int r3 = a || (b && c);

    printf("a=%d, b=%d, c=%d\n", a, b, c);
    printf("a || b && c     = %d\n", r1);
    printf("(a || b) && c   = %d\n", r2);
    printf("a || (b && c)   = %d\n", r3);

    return 0;
}
```

**¿`r1` se agrupa como `r2` o como `r3`?**

<details>
<summary>Predicción</summary>

```
a=0, b=1, c=1
a || b && c     = 1
(a || b) && c   = 1
a || (b && c)   = 1
```

`r1` se agrupa como `r3` — `&&` tiene mayor precedencia que `||`. Evaluación: `b && c = 1 && 1 = 1`, luego `a || 1 = 0 || 1 = 1`.

En este caso los tres dan 1, lo que oculta la diferencia. Con `a=0, b=1, c=0`:
- `a || (b && c)` = `0 || (1 && 0)` = `0 || 0` = **0** ← así se parsea
- `(a || b) && c` = `(0 || 1) && 0` = `1 && 0` = **0** ← misma casualidad

Con `a=1, b=0, c=0`:
- `a || (b && c)` = `1 || (0 && 0)` = **1** (short-circuit)
- `(a || b) && c` = `(1 || 0) && 0` = `1 && 0` = **0**

Aquí sí difieren. Usar paréntesis siempre para evitar ambigüedad.

</details>

---

### Ejercicio 8 — Leyes de De Morgan

```c
#include <stdio.h>

int main(void) {
    for (int p = 0; p <= 1; p++) {
        for (int q = 0; q <= 1; q++) {
            int dm1_left  = !(p && q);
            int dm1_right = (!p || !q);
            int dm2_left  = !(p || q);
            int dm2_right = (!p && !q);

            printf("p=%d q=%d | !(p&&q)=%d  !p||!q=%d | !(p||q)=%d  !p&&!q=%d\n",
                   p, q, dm1_left, dm1_right, dm2_left, dm2_right);
        }
    }

    return 0;
}
```

**Completar la tabla. ¿Las columnas emparejadas siempre coinciden?**

<details>
<summary>Predicción</summary>

```
p=0 q=0 | !(p&&q)=1  !p||!q=1 | !(p||q)=1  !p&&!q=1
p=0 q=1 | !(p&&q)=1  !p||!q=1 | !(p||q)=0  !p&&!q=0
p=1 q=0 | !(p&&q)=1  !p||!q=1 | !(p||q)=0  !p&&!q=0
p=1 q=1 | !(p&&q)=0  !p||!q=0 | !(p||q)=0  !p&&!q=0
```

Sí, las columnas emparejadas **siempre coinciden**, confirmando las leyes de De Morgan:
- `!(p && q) ≡ !p || !q` — las 4 filas coinciden
- `!(p || q) ≡ !p && !q` — las 4 filas coinciden

Esto es una verdad lógica, no depende de los valores. Útil para simplificar o invertir condiciones.

</details>

---

### Ejercicio 9 — Validación segura con short-circuit

```c
#include <stdio.h>

int safe_divide(int a, int b, int *result) {
    if (b != 0 && result != NULL) {
        *result = a / b;
        return 1;
    }
    return 0;
}

int main(void) {
    int r;

    if (safe_divide(10, 3, &r)) {
        printf("10 / 3 = %d\n", r);
    }
    if (safe_divide(10, 0, &r)) {
        printf("10 / 0 = %d\n", r);
    } else {
        printf("Division por cero evitada\n");
    }
    if (safe_divide(10, 5, NULL)) {
        printf("No deberia llegar aqui\n");
    } else {
        printf("Puntero NULL evitado\n");
    }

    return 0;
}
```

**¿Qué imprime? ¿Por qué el short-circuit hace esto seguro?**

<details>
<summary>Predicción</summary>

```
10 / 3 = 3
Division por cero evitada
Puntero NULL evitado
```

- Primera llamada: `b=3 ≠ 0` (true), `result ≠ NULL` (true) → divide: 10/3 = 3 ✓
- Segunda llamada: `b=0 ≠ 0` es false → `&&` short-circuit → no evalúa `result != NULL`, no divide. Retorna 0.
- Tercera llamada: `b=5 ≠ 0` (true), `result != NULL` es false (result es NULL) → no intenta `*result = ...`. Retorna 0.

El short-circuit de `&&` garantiza que nunca se llega a `*result = a / b` si `b` es 0 o `result` es NULL. Cada condición protege de un tipo de error diferente.

</details>

---

### Ejercicio 10 — Trampa de precedencia con `&` y `==`

```c
#include <stdio.h>

#define PERM_READ  (1U << 0)
#define PERM_WRITE (1U << 1)
#define PERM_EXEC  (1U << 2)

int main(void) {
    unsigned int perms = PERM_READ | PERM_EXEC;   // 0b101 = 5

    // Versión incorrecta:
    int r1 = perms & PERM_READ == PERM_READ;
    // Versión correcta:
    int r2 = (perms & PERM_READ) == PERM_READ;

    printf("perms = 0x%02X\n", perms);
    printf("Sin parens: perms & PERM_READ == PERM_READ → %d\n", r1);
    printf("Con parens: (perms & PERM_READ) == PERM_READ → %d\n", r2);

    // Otro caso:
    unsigned int p2 = PERM_WRITE;   // 0b010 = 2
    int r3 = p2 & PERM_READ == PERM_READ;
    int r4 = (p2 & PERM_READ) == PERM_READ;
    printf("\np2 = 0x%02X\n", p2);
    printf("Sin parens: %d\n", r3);
    printf("Con parens: %d\n", r4);

    return 0;
}
```

**¿Por qué `r1` y `r2` pueden dar resultados diferentes? ¿Y `r3` vs `r4`?**

<details>
<summary>Predicción</summary>

```
perms = 0x05
Sin parens: perms & PERM_READ == PERM_READ → 5
Con parens: (perms & PERM_READ) == PERM_READ → 1
```

- `r1`: `==` tiene mayor precedencia que `&`. Se parsea como `perms & (PERM_READ == PERM_READ)` = `5 & (1 == 1)` = `5 & 1` = `5` (no es 0 ni 1, es el valor bitwise).
- `r2`: `(perms & PERM_READ)` = `5 & 1` = `1`, luego `1 == 1` = `1` (correcto).

```
p2 = 0x02
Sin parens: 2
Con parens: 0
```

- `r3`: `p2 & (1 == 1)` = `2 & 1` = `0`. Pero espera, aquí `p2 = 2 = 0b010`, y `2 & 1 = 0`. Resultado: 0.

Hmm, let me recalculate. `PERM_READ == PERM_READ` = `1 == 1` = 1. `p2 & 1` = `2 & 1` = `0b010 & 0b001` = 0. So r3 = 0.

- `r4`: `(p2 & PERM_READ)` = `2 & 1` = 0, luego `0 == 1` = 0. Correcto: p2 no tiene PERM_READ.

Aquí r3 y r4 coinciden por casualidad (ambos 0), pero por razones diferentes. La versión sin paréntesis da resultados impredecibles — a veces coincide, a veces no.

</details>
