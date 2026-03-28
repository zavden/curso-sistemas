# T02 — Punto flotante

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| labs/README.md:98–99 | "long double muestra 0.1 exacto con 20 decimales porque tiene más bits de mantisa" | 0.1 **nunca es exacto** en ningún formato binario de punto flotante (es 0.0001100110011... periódico en binario). Con `long double` (80-bit extended, 64 bits de mantisa), el error es ~1.4×10⁻²⁰, que no aparece con 20 decimales pero sí con 25: `0.1000000000000000000013553`. No es exacto — solo *aparenta* serlo con 20 dígitos. |

---

## 1. Los tipos de punto flotante

C tiene tres tipos para números con parte decimal:

```c
float       // precisión simple — 32 bits (el más pequeño)
double      // precisión doble — 64 bits (el default del lenguaje)
long double // precisión extendida — al menos tan grande como double
```

| Tipo | Bytes | Bits mantisa | Dígitos significativos | Rango |
|------|-------|-------------|----------------------|-------|
| `float` | 4 | 23 | ~6 (`FLT_DIG`) | ±3.4×10³⁸ |
| `double` | 8 | 52 | ~15 (`DBL_DIG`) | ±1.8×10³⁰⁸ |
| `long double` | 16* | 64 | ~18 (`LDBL_DIG`) | ±1.2×10⁴⁹³² |

\* `sizeof(long double)` = 16 bytes en x86-64, pero internamente usa formato de 80 bits (10 bytes reales + 6 de padding).

### Cuándo usar cada uno

```c
double x = 3.14;        // double — el default. Literales decimales son double
float  y = 3.14f;       // float — requiere sufijo f. Para gráficos, audio, arrays grandes
long double z = 3.14L;  // long double — requiere sufijo L. Máxima precisión

// Regla: usar double a menos que tengas razón para no hacerlo.
```

---

## 2. IEEE 754 — Cómo se almacenan

Formato: **signo × mantisa × 2^exponente**

```
float (32 bits):
┌──┬──────────┬─────────────────────────┐
│S │ Exponente│      Mantisa            │
│1 │  8 bits  │     23 bits             │
└──┴──────────┴─────────────────────────┘

double (64 bits):
┌──┬───────────────┬────────────────────────────────────────────────────┐
│S │  Exponente    │                  Mantisa                          │
│1 │  11 bits      │                 52 bits                           │
└──┴───────────────┴────────────────────────────────────────────────────┘
```

**Consecuencia**: los floats tienen precisión **limitada**. No todos los números decimales se representan exactamente:

```c
float  f = 0.1f;    // NO es exactamente 0.1
double d = 0.1;     // NO es exactamente 0.1 (pero más cerca)

// 0.1 en binario es periódico: 0.0001100110011...
// Se trunca al número de bits de la mantisa.

printf("%.20f\n", 0.1);   // 0.10000000000000000555
printf("%.20f\n", 0.1f);  // 0.10000000149011611938
printf("%.25Lf\n", 0.1L); // 0.1000000000000000000013553
// Ninguno es exactamente 0.1
```

### Valores especiales de IEEE 754

```c
#include <math.h>

// Infinito (resultado de dividir por cero con floats):
double inf = 1.0 / 0.0;       // +inf
double neg_inf = -1.0 / 0.0;  // -inf

// NaN (Not a Number — operaciones indefinidas):
double nan_val = 0.0 / 0.0;   // NaN
double nan_sqrt = sqrt(-1.0);  // NaN

// Verificar:
isinf(inf);       // true
isnan(nan_val);   // true

// Propiedad clave de NaN: no es igual a NADA, ni a sí mismo:
nan_val == nan_val    // FALSE (siempre)
nan_val != nan_val    // TRUE  (forma de detectar NaN)
isnan(nan_val)        // TRUE  (forma clara de detectar NaN)
```

Aritmética con valores especiales:

| Operación | Resultado |
|-----------|-----------|
| `inf + 1` | `inf` (infinito absorbe finitos) |
| `inf + inf` | `inf` |
| `inf - inf` | `NaN` (indeterminado) |
| `inf * 0` | `NaN` (indeterminado) |
| `NaN + 1` | `NaN` (NaN es contagioso) |

### Subnormales

Números entre 0 y `DBL_MIN` (~2.2×10⁻³⁰⁸) que llenan gradualmente el hueco. Pierden precisión progresivamente.

```c
double subnormal = DBL_MIN / 2.0;  // ~1.1e-308
isnormal(subnormal);   // false (es subnormal)
isfinite(subnormal);   // true  (es finito)
```

---

## 3. Errores de redondeo

### El error clásico: comparar con `==`

```c
double a = 0.1 + 0.2;
if (a == 0.3) {
    printf("equal\n");     // NO se ejecuta
}
// 0.1 + 0.2 = 0.30000000000000004 (no exactamente 0.3)
```

### Comparar con epsilon

```c
#include <math.h>

// Epsilon absoluto — funciona con números pequeños:
int approx_equal_abs(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

// Epsilon relativo — funciona con cualquier magnitud:
int approx_equal_rel(double a, double b, double rel_epsilon) {
    double max_val = fmax(fabs(a), fabs(b));
    if (max_val == 0.0) return 1;
    return fabs(a - b) / max_val < rel_epsilon;
}
```

| Método | Funciona para | Falla para |
|--------|--------------|------------|
| Epsilon absoluto | Números pequeños (~0 a ~1000) | Números muy grandes (la diferencia absoluta crece) |
| Epsilon relativo | Números de cualquier magnitud | Valores cercanos a cero (divide por ~0) |

Valores de epsilon comunes:
- `double`: 1e-9 a 1e-12
- `float`: 1e-5 a 1e-6

### Acumulación de errores

```c
// Los errores de redondeo se ACUMULAN:
float sum = 0.0f;
for (int i = 0; i < 1000000; i++) {
    sum += 0.1f;
}
// Esperado: 100000.0
// Real: ~100958 (error de ~958 — ¡casi 1%!)

// Con double: error de ~1e-6 (mucho mejor, pero no cero)
```

Solución para sumas masivas: **Kahan summation** (suma compensada):

```c
float sum = 0.0f;
float comp = 0.0f;
for (int i = 0; i < 1000000; i++) {
    float y = 0.1f - comp;
    float t = sum + y;
    comp = (t - sum) - y;  // captura el error de redondeo
    sum = t;
}
// sum es mucho más preciso
```

### Pérdida por magnitud y cancelación catastrófica

```c
// Pérdida por magnitud — sumar pequeño a grande:
float big = 1e10f;
float result = big + 1.0f;
// result == big → el 1.0 se perdió (float tiene ~7 dígitos)

// Cancelación catastrófica — restar casi iguales:
double a = 1.0000001;
double b = 1.0000000;
double diff = a - b;  // 1e-7, pero solo ~1 dígito significativo de 15
```

---

## 4. Conversiones de tipo

### Implícitas

```c
// C convierte al tipo "mayor" en expresiones mixtas:
int i = 42;
double d = i;           // int → double (sin pérdida)

double pi = 3.14;
int truncated = pi;     // double → int: trunca a 3 (NO redondea)

// Jerarquía (simplificada):
// int → long → long long → float → double → long double
```

### Peligros

```c
// Truncamiento silencioso (double → int):
double d = 3.99;
int i = d;              // i = 3 (trunca hacia cero)

// Para redondear: usar math.h:
int rounded = (int)round(3.99);    // 4
int floored = (int)floor(3.99);    // 3
int ceiled  = (int)ceil(3.01);     // 4

// Overflow al convertir:
double huge = 1e30;
int bad = (int)huge;    // UB: el valor no cabe en int

// Pérdida de precisión int64_t → double:
int64_t big = 9007199254740993LL;  // 2^53 + 1
double d2 = (double)big;           // pierde el último bit
int64_t back = (int64_t)d2;
// back != big — double solo tiene 52 bits de mantisa
```

---

## 5. Funciones de `math.h`

```c
#include <math.h>

// Básicas:
sqrt(2.0);          pow(2.0, 10.0);       fabs(-3.14);

// Redondeo:
floor(3.7);    // 3.0 (hacia abajo)    ceil(3.2);     // 4.0 (hacia arriba)
round(3.5);    // 4.0 (al más cercano) trunc(3.9);    // 3.0 (hacia cero)

// Trigonometría (radianes):
sin(M_PI / 2);      cos(0.0);      tan(M_PI / 4);

// Logaritmos:
log(M_E);       // 1.0 (ln)       log10(100.0);   // 2.0
log2(8.0);      // 3.0

// Clasificación:
isnan(x);       isinf(x);       isfinite(x);       isnormal(x);
```

```bash
# IMPORTANTE: math.h necesita -lm para enlazar:
gcc -std=c11 main.c -lm -o main
# Sin -lm: "undefined reference to `sqrt'"
```

---

## 6. Constantes de `float.h`

```c
#include <float.h>

// Dígitos significativos:
FLT_DIG     // 6       DBL_DIG     // 15      LDBL_DIG    // 18

// Rangos (menor positivo normalizado / mayor):
FLT_MIN     // ~1.2e-38       FLT_MAX     // ~3.4e+38
DBL_MIN     // ~2.2e-308      DBL_MAX     // ~1.8e+308

// Epsilon (menor diferencia distinguible cerca de 1.0):
FLT_EPSILON // ~1.2e-7        DBL_EPSILON // ~2.2e-16

// 1.0 + FLT_EPSILON != 1.0  (son diferentes)
// 1.0 + FLT_EPSILON/2 == 1.0 (indistinguibles en float)
```

---

## 7. Formatos de `printf`

```c
double d = 3.14159265358979;
float f = 3.14159f;
long double ld = 3.14159265358979L;

printf("%f\n", d);          // 3.141593 (6 decimales por defecto)
printf("%.2f\n", d);        // 3.14
printf("%.15f\n", d);       // 3.141592653589790
printf("%e\n", d);          // 3.141593e+00 (notación científica)
printf("%g\n", d);          // 3.14159 (el más compacto)
printf("%f\n", f);          // float se promueve a double en printf
printf("%Lf\n", ld);        // long double necesita L
```

---

## Ejercicios

### Ejercicio 1 — Tamaños y precisión

Compila y ejecuta `labs/sizes_precision.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic sizes_precision.c -o sizes_precision && ./sizes_precision
rm -f sizes_precision
```

**Predicción**: ¿cuántos dígitos significativos tiene `float`? ¿`0.1` como `float` difiere de `0.1` como `double`?

<details><summary>Respuesta</summary>

```
=== Sizes ===
float:       4 bytes
double:      8 bytes
long double: 16 bytes

=== Significant decimal digits ===
float:       6 (FLT_DIG)
double:      15 (DBL_DIG)
long double: 18 (LDBL_DIG)

=== Precision demo ===
0.1 as float:       0.10000000149011611938
0.1 as double:      0.10000000000000000555
0.1 as long double: 0.10000000000000000000
```

`float` tiene 6 dígitos significativos. `0.1f` empieza a desviarse en el 8vo decimal, `0.1` (double) en el 17vo. `long double` *aparenta* ser exacto con 20 decimales, pero el error (~1.4×10⁻²⁰) está en el 21er decimal — ningún formato binario puede representar 0.1 exactamente.

</details>

### Ejercicio 2 — 0.1 + 0.2 y acumulación

Compila y ejecuta `labs/rounding_errors.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic rounding_errors.c -o rounding_errors && ./rounding_errors
rm -f rounding_errors
```

**Predicción**: ¿`0.1 + 0.2 == 0.3`? Si sumas `0.1f` un millón de veces, ¿el error es mayor o menor que 1.0?

<details><summary>Respuesta</summary>

```
=== 0.1 + 0.2 vs 0.3 ===
0.1 + 0.2 = 0.30000000000000004441
0.3       = 0.29999999999999998890
Are they equal (==)? NO

=== Accumulation with float (1 million times 0.1f) ===
Expected: 100000.0
Got:      100958.34
Error:    958.34

=== Loss of precision by magnitude ===
1e10f + 1.0f = 10000000000
The 1.0 was LOST (absorbed)
```

- `0.1 + 0.2` NO es igual a `0.3` — cada uno tiene un error de representación diferente.
- El error acumulado con `float` es **~958** — casi 1% de error después de 1 millón de sumas.
- Con `double` el error es ~1.3×10⁻⁶ — mucho mejor pero no cero.
- `1e10f + 1.0f` pierde el `1.0` porque `float` solo tiene ~7 dígitos significativos.

</details>

### Ejercicio 3 — Comparación correcta con epsilon

Compila y ejecuta `labs/compare_floats.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic compare_floats.c -lm -o compare_floats && ./compare_floats
rm -f compare_floats
```

**Predicción**: ¿epsilon absoluto 1e-9 dice que `1e15 + 1` y `1e15` son iguales? ¿Y epsilon relativo?

<details><summary>Respuesta</summary>

```
=== Comparison with absolute epsilon ===
epsilon = 1e-9:  EQUAL
epsilon = 1e-16: EQUAL

=== Problem with absolute epsilon on large numbers ===
1e15 + 1.0 == 1e15? (with ==):       NO
With abs epsilon 1e-9:                NOT EQUAL
Actual difference: 1.000000

=== Comparison with relative epsilon ===
0.1+0.2 vs 0.3 (rel 1e-9):           EQUAL
1e15+1 vs 1e15 (rel 1e-9):            EQUAL

=== FLT_EPSILON demo ===
1.0f + FLT_EPSILON     == 1.0f? NO (different)
1.0f + FLT_EPSILON/2   == 1.0f? YES (indistinguishable)
```

- Epsilon absoluto detecta correctamente que `1e15 + 1` y `1e15` difieren (diferencia = 1.0, mucho mayor que 1e-9).
- Epsilon relativo dice EQUAL porque la diferencia relativa es `1/1e15 = 1e-15 < 1e-9`.
- Cuál es "correcto" depende de la aplicación: ¿importa una diferencia de 1.0 en valores de 10¹⁵?
- `FLT_EPSILON` es el umbral exacto: sumarlo a 1.0f cambia el valor, pero `FLT_EPSILON/2` se absorbe.

</details>

### Ejercicio 4 — Valores especiales de IEEE 754

Compila y ejecuta `labs/special_values.c`:

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic special_values.c -lm -o special_values && ./special_values
rm -f special_values
```

**Predicción**: ¿`NaN == NaN`? ¿`inf - inf` da 0, inf, o NaN?

<details><summary>Respuesta</summary>

```
=== NaN comparisons ===
NaN == NaN?  NO
NaN != NaN?  YES
NaN > 0?     NO
NaN < 0?     NO
NaN == 0?    NO

=== Arithmetic with special values ===
inf - inf = -nan
inf * 0   = -nan
NaN + 1   = -nan

=== Classification ===
isnormal(0.0)?       NO
isnormal(DBL_MIN/2)? NO (subnormal)
```

- `NaN == NaN` es **FALSE** — NaN no es igual a nada, ni a sí mismo. Por diseño de IEEE 754.
- `inf - inf` = **NaN** (indeterminado, no 0).
- `inf * 0` = **NaN** (indeterminado, no 0).
- NaN es contagioso: cualquier operación con NaN produce NaN.
- `0.0` no es "normal" — es un valor especial. Los subnormales tampoco son normales.

</details>

### Ejercicio 5 — `FLT_EPSILON` en acción

```c
// epsilon_explore.c
#include <stdio.h>
#include <float.h>

int main(void) {
    float one = 1.0f;

    printf("FLT_EPSILON = %e\n", FLT_EPSILON);
    printf("1.0f + FLT_EPSILON     = %.10f\n", one + FLT_EPSILON);
    printf("1.0f + FLT_EPSILON/2   = %.10f\n", one + FLT_EPSILON / 2.0f);
    printf("1.0f + FLT_EPSILON/2 == 1.0f? %s\n",
           (one + FLT_EPSILON / 2.0f == one) ? "YES" : "NO");

    // ¿Y cerca de 2.0?
    float two = 2.0f;
    printf("\n2.0f + FLT_EPSILON     == 2.0f? %s\n",
           (two + FLT_EPSILON == two) ? "YES (absorbed)" : "NO (different)");
    printf("2.0f + 2*FLT_EPSILON   == 2.0f? %s\n",
           (two + 2.0f * FLT_EPSILON == two) ? "YES (absorbed)" : "NO (different)");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic epsilon_explore.c -o epsilon_explore && ./epsilon_explore
rm -f epsilon_explore
```

**Predicción**: ¿`2.0f + FLT_EPSILON == 2.0f`? ¿Por qué?

<details><summary>Respuesta</summary>

```
FLT_EPSILON = 1.192093e-07
1.0f + FLT_EPSILON     = 1.0000001192
1.0f + FLT_EPSILON/2   = 1.0000000000
1.0f + FLT_EPSILON/2 == 1.0f? YES

2.0f + FLT_EPSILON     == 2.0f? YES (absorbed)
2.0f + 2*FLT_EPSILON   == 2.0f? NO (different)
```

`FLT_EPSILON` se absorbe al sumarlo a `2.0f`. Esto es porque `FLT_EPSILON` se define como la menor diferencia relativa a **1.0f**. Cerca de 2.0f, el "epsilon" real es `2 * FLT_EPSILON` (la precisión relativa se mantiene, pero la absoluta cambia con la magnitud). En general, el epsilon de un valor `x` es `x * FLT_EPSILON`.

</details>

### Ejercicio 6 — Truncamiento vs redondeo

```c
// truncation.c
#include <stdio.h>
#include <math.h>

int main(void) {
    double values[] = {3.1, 3.5, 3.9, -3.1, -3.5, -3.9};
    int n = 6;

    printf("%-8s %-8s %-8s %-8s %-8s\n",
           "value", "(int)", "round", "floor", "ceil");
    printf("%-8s %-8s %-8s %-8s %-8s\n",
           "-----", "-----", "-----", "-----", "----");

    for (int i = 0; i < n; i++) {
        double v = values[i];
        printf("%-8.1f %-8d %-8.0f %-8.0f %-8.0f\n",
               v, (int)v, round(v), floor(v), ceil(v));
    }

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic truncation.c -lm -o truncation && ./truncation
rm -f truncation
```

**Predicción**: ¿`(int)(-3.9)` es -3 o -4? ¿`floor(-3.1)` es -3 o -4?

<details><summary>Respuesta</summary>

```
value    (int)    round    floor    ceil
-----    -----    -----    -----    ----
3.1      3        3        3        4
3.5      3        4        3        4
3.9      3        4        3        4
-3.1     -3       -3       -4       -3
-3.5     -3       -4       -4       -3
-3.9     -3       -4       -4       -3
```

- `(int)` **trunca hacia cero**: `-3.9` → `-3` (no -4)
- `floor` **redondea hacia -∞**: `-3.1` → `-4`
- `ceil` **redondea hacia +∞**: `3.1` → `4`
- `round` **redondea al más cercano**: `3.5` → `4`, `-3.5` → `-4`

La diferencia clave: `(int)` y `trunc()` van hacia cero, `floor()` siempre baja. Para negativos dan resultados diferentes.

</details>

### Ejercicio 7 — Pérdida de precisión int64 → double

```c
// int64_precision.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int main(void) {
    // double tiene 52 bits de mantisa → puede representar
    // exactamente enteros hasta 2^53 = 9007199254740992

    int64_t exact = 9007199254740992LL;     // 2^53 — exacto en double
    int64_t lost  = 9007199254740993LL;     // 2^53 + 1 — NO exacto

    double d_exact = (double)exact;
    double d_lost  = (double)lost;

    printf("exact:      %" PRId64 "\n", exact);
    printf("as double:  %.0f\n", d_exact);
    printf("back:       %" PRId64 "\n", (int64_t)d_exact);
    printf("match: %s\n\n", (exact == (int64_t)d_exact) ? "YES" : "NO");

    printf("lost:       %" PRId64 "\n", lost);
    printf("as double:  %.0f\n", d_lost);
    printf("back:       %" PRId64 "\n", (int64_t)d_lost);
    printf("match: %s\n", (lost == (int64_t)d_lost) ? "YES" : "NO");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic int64_precision.c -o int64_prec && ./int64_prec
rm -f int64_prec
```

**Predicción**: ¿2⁵³ + 1 sobrevive la conversión int64 → double → int64?

<details><summary>Respuesta</summary>

```
exact:      9007199254740992
as double:  9007199254740992
back:       9007199254740992
match: YES

lost:       9007199254740993
as double:  9007199254740992
back:       9007199254740992
match: NO
```

2⁵³ se representa exactamente. 2⁵³ + 1 **no** — double lo redondea a 2⁵³. La conversión ida-y-vuelta pierde el `+1`. Esto es relevante para IDs, timestamps, y cualquier entero grande que pase por `double` (como en JSON, que usa double para todos los números).

</details>

### Ejercicio 8 — Kahan summation vs suma ingenua

```c
// kahan.c
#include <stdio.h>

int main(void) {
    int n = 1000000;

    // Suma ingenua:
    float naive = 0.0f;
    for (int i = 0; i < n; i++) {
        naive += 0.1f;
    }

    // Kahan summation:
    float kahan = 0.0f;
    float comp = 0.0f;
    for (int i = 0; i < n; i++) {
        float y = 0.1f - comp;
        float t = kahan + y;
        comp = (t - kahan) - y;
        kahan = t;
    }

    // Referencia con double:
    double precise = 0.0;
    for (int i = 0; i < n; i++) {
        precise += 0.1;
    }

    double expected = 100000.0;
    printf("Expected:     %.2f\n", expected);
    printf("Naive float:  %.2f (error: %.2f)\n",
           (double)naive, (double)(naive - (float)expected));
    printf("Kahan float:  %.2f (error: %.2f)\n",
           (double)kahan, (double)(kahan - (float)expected));
    printf("Double:       %.6f (error: %.10f)\n",
           precise, precise - expected);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic kahan.c -o kahan && ./kahan
rm -f kahan
```

**Predicción**: ¿Kahan reduce el error de ~958 a cuánto?

<details><summary>Respuesta</summary>

```
Expected:     100000.00
Naive float:  100958.34 (error: 958.34)
Kahan float:  100000.00 (error: 0.00)
Double:       100000.000001 (error: 0.0000013329)
```

Kahan summation reduce el error de **~958 a ~0** con el mismo tipo `float`. El algoritmo compensa el error de cada suma, acumulándolo en una variable separada (`comp`) y corrigiéndolo en la siguiente iteración. Es tan preciso como usar `double` (o mejor) con el costo de solo una variable extra y unas pocas operaciones más.

</details>

### Ejercicio 9 — Formatos de printf para flotantes

```c
// float_formats.c
#include <stdio.h>

int main(void) {
    double pi = 3.14159265358979;
    double big = 6.022e23;
    double tiny = 1.602e-19;

    printf("=== %%f (fixed) ===\n");
    printf("pi:   %f\n", pi);
    printf("big:  %f\n", big);
    printf("tiny: %f\n", tiny);

    printf("\n=== %%e (scientific) ===\n");
    printf("pi:   %e\n", pi);
    printf("big:  %e\n", big);
    printf("tiny: %e\n", tiny);

    printf("\n=== %%g (compact) ===\n");
    printf("pi:   %g\n", pi);
    printf("big:  %g\n", big);
    printf("tiny: %g\n", tiny);

    printf("\n=== precision ===\n");
    printf("%%.2f:  %.2f\n", pi);
    printf("%%.10f: %.10f\n", pi);
    printf("%%.2e:  %.2e\n", big);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic float_formats.c -o float_formats && ./float_formats
rm -f float_formats
```

**Predicción**: ¿`%f` con un número como 6.022×10²³ es legible? ¿Qué formato es mejor?

<details><summary>Respuesta</summary>

```
=== %f (fixed) ===
pi:   3.141593
big:  602200000000000000000000.000000
tiny: 0.000000

=== %e (scientific) ===
pi:   3.141593e+00
big:  6.022000e+23
tiny: 1.602000e-19

=== %g (compact) ===
pi:   3.14159
big:  6.022e+23
tiny: 1.602e-19
```

- `%f` con 6.022×10²³ produce una cadena enorme con 24 dígitos antes del punto. Ilegible.
- `%f` con 1.602×10⁻¹⁹ muestra `0.000000` — el valor se pierde con solo 6 decimales.
- `%e` funciona bien para cualquier magnitud.
- `%g` elige automáticamente entre `%f` y `%e` — el más compacto y usualmente el mejor.

</details>

### Ejercicio 10 — Cancelación catastrófica

```c
// catastrophic.c
#include <stdio.h>
#include <math.h>

int main(void) {
    // Calcular f(x) = (1 - cos(x)) / x^2 para x pequeño
    // Valor teórico cuando x → 0: 0.5

    printf("%-15s %-25s %-25s\n", "x", "naive", "stable");
    printf("%-15s %-25s %-25s\n", "---", "-----", "------");

    double x_vals[] = {1e-1, 1e-3, 1e-5, 1e-7, 1e-8};
    int n = 5;

    for (int i = 0; i < n; i++) {
        double x = x_vals[i];

        // Forma ingenua (cancelación catastrófica):
        double naive = (1.0 - cos(x)) / (x * x);

        // Forma estable: usar identidad 1-cos(x) = 2*sin^2(x/2)
        double stable = 2.0 * pow(sin(x / 2.0), 2) / (x * x);

        printf("%-15e %-25.17f %-25.17f\n", x, naive, stable);
    }

    printf("\nExpected (limit): 0.5\n");

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic catastrophic.c -lm -o catastrophic && ./catastrophic
rm -f catastrophic
```

**Predicción**: ¿la forma ingenua se acerca a 0.5 conforme `x` se achica? ¿O empeora?

<details><summary>Respuesta</summary>

```
x               naive                     stable
---             -----                     ------
1.000000e-01    0.49958347219742783       0.49958347219742783
1.000000e-03    0.49999995833333413       0.49999999583333347
1.000000e-05    0.49999999999924734       0.49999999999958340
1.000000e-07    0.49999999918144948       0.49999999999999994
1.000000e-08    0.49999999272770602       0.50000000000000000
```

Para `x` moderado ambas dan resultados similares. Pero conforme `x` se achica, la forma ingenua **empeora** — `1.0 - cos(x)` cuando `x` es muy pequeño produce cancelación catastrófica: `cos(x)` es casi 1.0, y restar dos números casi iguales pierde dígitos significativos. La forma estable usa `sin²(x/2)` que no sufre este problema y converge limpiamente a 0.5.

</details>
