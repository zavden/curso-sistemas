# T02 — Punto flotante

## Los tipos de punto flotante

C tiene tres tipos para representar números con decimales:

```c
float       // precisión simple — al menos 32 bits
double      // precisión doble — al menos 64 bits (el default)
long double // precisión extendida — al menos tan grande como double
```

```c
#include <stdio.h>
#include <float.h>

int main(void) {
    printf("float:       %zu bytes, %d dígitos significativos\n",
           sizeof(float), FLT_DIG);
    printf("double:      %zu bytes, %d dígitos significativos\n",
           sizeof(double), DBL_DIG);
    printf("long double: %zu bytes, %d dígitos significativos\n",
           sizeof(long double), LDBL_DIG);
    return 0;
}

// Resultado típico en x86-64:
// float:       4 bytes, 6 dígitos significativos
// double:      8 bytes, 15 dígitos significativos
// long double: 16 bytes, 18 dígitos significativos
```

### Cuándo usar cada uno

```c
// double es el tipo por defecto para punto flotante en C.
// Los literales decimales son double:
double x = 3.14;        // double (default)
float  y = 3.14f;       // float (requiere sufijo f)
long double z = 3.14L;  // long double (requiere sufijo L)

// Usar double a menos que tengas una razón para no hacerlo:
// - float: gráficos, audio, arrays grandes donde el ahorro de memoria importa
// - long double: cálculos que necesitan la máxima precisión disponible
// - double: todo lo demás (el default del lenguaje)
```

## IEEE 754 — Cómo se almacenan

Los números de punto flotante se representan en formato
IEEE 754 como: signo × mantisa × 2^exponente:

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

```c
// Consecuencia: los floats tienen precisión LIMITADA.
// No todos los números decimales se pueden representar exactamente.

float  f = 0.1f;    // NO es exactamente 0.1
double d = 0.1;     // NO es exactamente 0.1 (pero más cerca)

// 0.1 en binario es periódico: 0.0001100110011...
// Se trunca al número de bits de la mantisa.

printf("%.20f\n", 0.1);   // 0.10000000000000000555
printf("%.20f\n", 0.1f);  // 0.10000000149011611938
```

### Valores especiales

```c
#include <math.h>

// IEEE 754 define valores especiales:

// Infinito:
double inf = 1.0 / 0.0;       // +inf
double neg_inf = -1.0 / 0.0;  // -inf
printf("%f\n", inf);           // inf

// NaN (Not a Number):
double nan = 0.0 / 0.0;       // NaN
printf("%f\n", nan);           // nan o -nan

// Verificar:
if (isinf(inf))  printf("es infinito\n");
if (isnan(nan))  printf("es NaN\n");

// Propiedad de NaN: NaN no es igual a NADA, ni a sí mismo:
if (nan == nan) { }    // FALSE — nunca se ejecuta
if (nan != nan) { }    // TRUE — esta es la forma de detectar NaN

// isnan() es más claro:
if (isnan(nan)) { }    // TRUE
```

## Errores de redondeo

### El error clásico: comparar con ==

```c
// NUNCA comparar floats/doubles con ==:
double a = 0.1 + 0.2;
if (a == 0.3) {
    printf("equal\n");     // NO se ejecuta
} else {
    printf("not equal\n"); // se ejecuta
}
// 0.1 + 0.2 = 0.30000000000000004 (no exactamente 0.3)
```

```c
// Comparar con un epsilon (tolerancia):
#include <math.h>

int double_equal(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

if (double_equal(0.1 + 0.2, 0.3, 1e-9)) {
    printf("approximately equal\n");   // se ejecuta
}

// ¿Qué epsilon usar?
// Para doubles: DBL_EPSILON (~2.2e-16) es el mínimo teórico
// En la práctica: 1e-9 a 1e-12 según la aplicación
// Para floats: FLT_EPSILON (~1.2e-7), práctica: 1e-5 a 1e-6
```

### Acumulación de errores

```c
// Los errores de redondeo se ACUMULAN:
float sum = 0.0f;
for (int i = 0; i < 1000000; i++) {
    sum += 0.1f;
}
printf("sum = %f\n", sum);
// Esperado: 100000.0
// Real: algo como 100958.34... (el error se acumuló)

// Solución: sumar de menor a mayor, o usar algoritmos
// de suma compensada (Kahan summation):
float sum_kahan = 0.0f;
float comp = 0.0f;
for (int i = 0; i < 1000000; i++) {
    float y = 0.1f - comp;
    float t = sum_kahan + y;
    comp = (t - sum_kahan) - y;
    sum_kahan = t;
}
// sum_kahan es mucho más preciso
```

### Pérdida de precisión por magnitud

```c
// Sumar un número muy pequeño a uno muy grande puede no tener efecto:
float big = 1e10f;
float small = 1.0f;
float result = big + small;
printf("%.0f\n", result);    // 10000000000 (el 1.0 se perdió)
// float tiene ~7 dígitos significativos
// 10000000000 + 1 = 10000000001 (11 dígitos — no cabe)

// Con double (15 dígitos) este ejemplo funciona,
// pero el problema existe con números más grandes.
```

### Cancelación catastrófica

```c
// Restar dos números casi iguales pierde dígitos significativos:
double a = 1.0000001;
double b = 1.0000000;
double diff = a - b;
// diff = 0.0000001 (1e-7)
// Pero solo tiene ~1 dígito significativo de los 15 de double

// Si luego usas diff en más cálculos, la imprecisión se amplifica.
// Esto se llama "cancelación catastrófica".
```

## Conversiones de tipo

### Implícitas (automáticas)

```c
// Cuando mezclas tipos, C convierte al tipo "mayor":
int i = 42;
double d = i;              // int → double (sin pérdida)

double pi = 3.14;
int truncated = pi;        // double → int: trunca a 3
                           // (no redondea, trunca hacia cero)

float f = 3.14;            // double literal → float (posible pérdida de precisión)

// Jerarquía de conversión:
// int → long → long long → float → double → long double
// (tipos de menor rango se convierten al de mayor rango)
```

### Peligros de conversión

```c
// double a int — truncamiento silencioso:
double d = 3.99;
int i = d;              // i = 3 (NO 4, trunca)

// Para redondear: usar round(), floor(), ceil() de <math.h>
int rounded = (int)round(3.99);    // 4
int floored = (int)floor(3.99);    // 3
int ceiled  = (int)ceil(3.01);     // 4

// Overflow al convertir:
double huge = 1e30;
int bad = (int)huge;    // UB: el valor no cabe en int

// float a int64_t — pérdida de precisión:
int64_t big = 9007199254740993LL;  // 2^53 + 1
double d2 = (double)big;
int64_t back = (int64_t)d2;
// back != big — double solo tiene 52 bits de mantisa
// No puede representar todos los int64_t exactamente
```

## Funciones de math.h

```c
#include <math.h>

// Funciones básicas:
double r = sqrt(2.0);         // raíz cuadrada
double p = pow(2.0, 10.0);    // 2^10 = 1024
double a = fabs(-3.14);       // valor absoluto (3.14)

// Redondeo:
double f = floor(3.7);        // 3.0 (hacia abajo)
double c = ceil(3.2);         // 4.0 (hacia arriba)
double r2 = round(3.5);       // 4.0 (al más cercano)
double t = trunc(3.9);        // 3.0 (hacia cero)

// Trigonometría (radianes):
double s = sin(M_PI / 2);     // 1.0
double co = cos(0.0);         // 1.0
double ta = tan(M_PI / 4);    // 1.0

// Logaritmos:
double ln = log(M_E);         // 1.0 (logaritmo natural)
double l10 = log10(100.0);    // 2.0
double l2 = log2(8.0);        // 3.0

// Clasificación:
isnan(x);       // ¿es NaN?
isinf(x);       // ¿es infinito?
isfinite(x);    // ¿es finito (no NaN ni inf)?
isnormal(x);    // ¿es normal (no cero, no subnormal, no inf, no NaN)?
```

```bash
# math.h necesita -lm para enlazar:
gcc -std=c11 main.c -lm -o main
# Sin -lm: undefined reference to `sqrt'
```

## Constantes de float.h

```c
#include <float.h>

// Precisión:
FLT_DIG     // dígitos decimales significativos de float (6)
DBL_DIG     // dígitos decimales significativos de double (15)

// Rangos:
FLT_MIN     // menor float positivo normalizado (~1.2e-38)
FLT_MAX     // mayor float (~3.4e+38)
DBL_MIN     // menor double positivo normalizado (~2.2e-308)
DBL_MAX     // mayor double (~1.8e+308)

// Epsilon — la menor diferencia representable cerca de 1.0:
FLT_EPSILON // ~1.2e-7
DBL_EPSILON // ~2.2e-16

// Ejemplo:
// 1.0 + FLT_EPSILON != 1.0 (son diferentes)
// 1.0 + FLT_EPSILON/2 == 1.0 (indistinguibles en float)
```

## Formatos de printf

```c
double d = 3.14159265358979;
float f = 3.14159f;
long double ld = 3.14159265358979L;

printf("%f\n", d);          // 3.141593 (6 decimales por defecto)
printf("%.2f\n", d);        // 3.14 (2 decimales)
printf("%.15f\n", d);       // 3.141592653589790 (15 decimales)
printf("%e\n", d);          // 3.141593e+00 (notación científica)
printf("%g\n", d);          // 3.14159 (el más compacto)

printf("%f\n", f);          // float se promueve a double en printf
printf("%Lf\n", ld);        // long double necesita L
```

---

## Ejercicios

### Ejercicio 1 — Precisión

```c
// Imprimir 0.1 + 0.2 con 20 decimales.
// ¿Es igual a 0.3?
// Escribir una función approx_equal(a, b, epsilon)
// y verificar que 0.1 + 0.2 es "aproximadamente" 0.3.
```

### Ejercicio 2 — Acumulación de errores

```c
// Sumar 0.1f un millón de veces con float.
// ¿Cuánto se aleja del resultado esperado (100000.0)?
// Repetir con double. ¿Mejora?
```

### Ejercicio 3 — Valores especiales

```c
// Calcular y verificar:
// 1.0 / 0.0 → ¿qué da? ¿isinf?
// 0.0 / 0.0 → ¿qué da? ¿isnan?
// sqrt(-1.0) → ¿qué da?
// ¿NaN == NaN es true o false?
```
