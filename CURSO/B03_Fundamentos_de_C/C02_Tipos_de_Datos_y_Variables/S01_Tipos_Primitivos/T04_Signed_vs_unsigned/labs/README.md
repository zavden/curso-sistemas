# Lab — Signed vs unsigned

## Objetivo

Observar como los mismos bits se interpretan de forma diferente dependiendo de
si el tipo es signed o unsigned. Provocar overflow en ambos contextos, ver las
conversiones implicitas que C hace al mezclar tipos, y aprender a usar los
warnings de GCC para detectar estos problemas. Al finalizar, sabras por que
comparar un `int` negativo con un `unsigned int` produce resultados inesperados
y como protegerte de estas trampas.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `bit_representation.c` | Muestra la misma secuencia de bits como signed y unsigned |
| `overflow_behavior.c` | Demuestra wrapping en unsigned y UB en signed |
| `implicit_conversions.c` | Trampas de comparacion signed vs unsigned y size_t |
| `warnings_demo.c` | Codigo que dispara warnings con diferentes flags de GCC |

---

## Parte 1 — Representacion en bits: complemento a dos

**Objetivo**: Ver como los mismos 8 bits producen valores diferentes segun sean
interpretados como signed o unsigned.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat bit_representation.c
```

Observa:

- `print_bits()` imprime los 8 bits de un byte
- El `main()` recorre varios valores y los muestra como unsigned y como signed
- Se usan las constantes de `<limits.h>` para mostrar los rangos

### Paso 1.2 — Predecir antes de compilar

Antes de compilar, piensa:

- El valor `200` almacenado en un `unsigned char`: que valor tendra como
  `signed char`?
- El valor `128`: es positivo o negativo como `signed char`?
- A partir de que valor de bits las interpretaciones signed y unsigned divergen?

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic bit_representation.c -o bit_representation
./bit_representation
```

Salida esperada:

```
=== Signed vs Unsigned: same bits, different interpretation ===

Bits        unsigned    signed      Hex
--------    --------    ------      ---
00000000  0           0           0x00
00000001  1           1           0x01
01111111  127         127         0x7F
10000000  128         -128        0x80
10000001  129         -127        0x81
11001000  200         -56         0xC8
11111111  255         -1          0xFF

=== Ranges (8 bits) ===
unsigned char: 0 to 255
signed char:   -128 to 127

=== Ranges (32 bits) ===
unsigned int: 0 to 4294967295
signed int:   -2147483648 to 2147483647
```

Analisis:

- Mientras el bit mas significativo (bit 7) es 0, ambas interpretaciones
  coinciden (0 a 127).
- Cuando el bit 7 es 1, unsigned lo interpreta como +128, pero signed lo
  interpreta como el bit de signo: el valor se vuelve negativo.
- `11111111` es 255 como unsigned pero -1 como signed. Esto es complemento
  a dos: para obtener el negativo, se invierten los bits y se suma 1.
- `11001000` (200 unsigned): invierte -> `00110111` (55) + 1 = 56, por lo
  tanto signed = -56.

### Limpieza de la parte 1

```bash
rm -f bit_representation
```

---

## Parte 2 — Overflow: UB signed vs wrapping unsigned

**Objetivo**: Observar que el overflow en unsigned tiene un comportamiento
definido (wrapping modular), mientras que en signed es Undefined Behavior.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat overflow_behavior.c
```

Observa:

- La primera seccion suma 1 a `UINT_MAX` y resta 1 a 0 (ambos unsigned)
- Un loop muestra un `unsigned char` pasando de 255 a 0
- La segunda seccion suma 1 a `INT_MAX` (signed) -- esto es UB

### Paso 2.2 — Predecir

Antes de ejecutar:

- `UINT_MAX + 1` sera 0 o un error?
- `0 - 1` en unsigned sera -1 o un valor enorme?
- `INT_MAX + 1` tendra un resultado predecible?

### Paso 2.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic overflow_behavior.c -o overflow_behavior
./overflow_behavior
```

Salida esperada:

```
=== Unsigned overflow: wrapping (defined behavior) ===

UINT_MAX     = 4294967295
UINT_MAX + 1 = 0

0 - 1 (unsigned) = 4294967295

--- Unsigned wrapping demo ---
uc = 250  ->  uc + 1 = 251
uc = 251  ->  uc + 1 = 252
uc = 252  ->  uc + 1 = 253
uc = 253  ->  uc + 1 = 254
uc = 254  ->  uc + 1 = 255
uc = 255  ->  uc + 1 =   0
uc =   0  ->  uc + 1 =   1
uc =   1  ->  uc + 1 =   2
uc =   2  ->  uc + 1 =   3
uc =   3  ->  uc + 1 =   4

=== Signed overflow: UNDEFINED BEHAVIOR ===

INT_MAX = 2147483647
WARNING: The next line triggers signed integer overflow (UB).
The compiler may produce any result.
INT_MAX + 1 = ~-2147483648  (UB: this result is NOT guaranteed)
```

Analisis:

- Unsigned wrapping esta definido por el estandar: el resultado es
  `valor_real % 2^N`. Por eso `UINT_MAX + 1 = 0` y `0 - 1 = UINT_MAX`.
- El demo con `unsigned char` muestra el wrapping visualmente: 255 + 1 = 0.
- El signed overflow es UB. En la practica, la mayoria de las plataformas
  producen wrapping (INT_MAX + 1 = INT_MIN), pero el compilador NO garantiza
  esto. Con optimizacion (`-O2`), el compilador puede asumir que el overflow
  nunca ocurre y eliminar o transformar codigo que dependa de el.

### Paso 2.4 — Ver el efecto del UB con optimizacion

El verdadero peligro del signed overflow UB es que el compilador puede
optimizar de formas inesperadas. Crea un archivo temporal para demostrarlo:

```bash
cat > ub_optimization.c << 'EOF'
#include <stdio.h>
#include <limits.h>

/* Does adding 1 always increase a number? */
int always_increases_signed(int x) {
    return x + 1 > x;
}

int always_increases_unsigned(unsigned int x) {
    return x + 1 > x;
}

int main(void) {
    printf("=== Does x + 1 > x? ===\n\n");

    printf("Signed   with INT_MAX:  x + 1 > x = %d\n",
           always_increases_signed(INT_MAX));
    printf("Unsigned with UINT_MAX: x + 1 > x = %d\n",
           always_increases_unsigned(UINT_MAX));

    return 0;
}
EOF
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ub_optimization.c -o ub_optimization
./ub_optimization
```

Salida esperada:

```
=== Does x + 1 > x? ===

Signed   with INT_MAX:  x + 1 > x = 1
Unsigned with UINT_MAX: x + 1 > x = 0
```

Para la funcion signed, el resultado es 1 (true). Pero `INT_MAX + 1` deberia
hacer wrapping a `INT_MIN`, y `INT_MIN > INT_MAX` es false. Que paso?

Veamos el assembly para entender:

```bash
gcc -std=c11 -S -O0 ub_optimization.c -o ub_optimization.s
```

Busca las dos funciones:

```bash
grep -A 12 "always_increases_signed:" ub_optimization.s
```

Salida esperada:

```
always_increases_signed:
        ...
        movl    $1, %eax
        ...
        ret
```

La funcion signed fue reemplazada por `return 1` (`movl $1, %eax`). El
compilador razona: "para signed, `x + 1 > x` siempre es true, porque el
overflow es UB y no puede ocurrir en un programa correcto". No hay comparacion,
no hay suma: solo retorna 1.

```bash
grep -A 15 "always_increases_unsigned:" ub_optimization.s
```

Salida esperada:

```
always_increases_unsigned:
        ...
        cmpl    $-1, ...
        setne   %al
        ...
        ret
```

La funcion unsigned **si hace la comparacion** (instruccion `cmpl`). El
compilador no puede optimizarla porque el wrapping de `UINT_MAX + 1 = 0` es
comportamiento definido, y `0 > UINT_MAX` es legitimamente false.

Analisis:

- Incluso sin flags de optimizacion (`-O0`), el compilador aprovecha el UB de
  signed overflow para simplificar la funcion a `return 1`.
- Para unsigned, el compilador debe generar el codigo real porque el wrapping
  puede ocurrir y es comportamiento valido.
- Este es el peligro real del UB: el compilador transforma tu logica de formas
  que no esperas.

### Limpieza de la parte 2

```bash
rm -f overflow_behavior ub_optimization.c ub_optimization ub_optimization.s
```

---

## Parte 3 — Conversiones implicitas peligrosas

**Objetivo**: Ver como C convierte silenciosamente entre signed y unsigned al
mezclarlos en comparaciones y asignaciones, produciendo resultados incorrectos.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat implicit_conversions.c
```

Observa tres escenarios:

1. Asignar `-1` a un `unsigned int`
2. Comparar un `int` negativo con un `unsigned int`
3. Comparar `strlen()` (que retorna `size_t`, un tipo unsigned) con un valor
   negativo

### Paso 3.2 — Predecir

Antes de ejecutar:

- Si `s = -1` y haces `unsigned int u = s`, cuanto vale `u`?
- Si comparas `int neg = -1` con `unsigned int zero = 0`, es `neg < zero`
  verdadero o falso?
- Si `strlen("Hello") = 5` y `n = -1`, es `strlen >= n` verdadero o falso?

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic implicit_conversions.c -o implicit_conversions
```

Observa que GCC ya produce un warning con `-Wall`:

```
implicit_conversions.c: In function 'main':
implicit_conversions.c:21:13: warning: comparison of integer expressions of
    different signedness: 'int' and 'unsigned int' [-Wsign-compare]
```

GCC detecta la comparacion peligrosa entre `neg` (signed) y `zero` (unsigned).

```bash
./implicit_conversions
```

Salida esperada:

```
=== Implicit conversion: signed to unsigned ===

int s = -1
unsigned int u = s  ->  u = 4294967295

=== Comparison trap: signed vs unsigned ===

Comparing: int neg = -1  vs  unsigned int zero = 0
Is neg < zero?  NO (neg is NOT less!)

Explanation:
  neg (-1) is converted to unsigned before comparison.
  -1 as unsigned = 4294967295 (UINT_MAX)
  4294967295 < 0 ?  -> false

=== size_t comparison trap ===

strlen("Hello") = 5
n = -1
Is len >= n?  (expecting YES, since 5 >= -1)
  Result: NO!  (unexpected)

  Why? n (-1) as size_t = 18446744073709551615
  That is SIZE_MAX. 5 >= SIZE_MAX? -> false!
```

Analisis:

- Cuando C compara un signed con un unsigned, convierte el signed a unsigned
  **antes** de comparar. El valor `-1` se convierte a `UINT_MAX` (todos los
  bits en 1).
- El resultado: `-1 < 0` deberia ser true, pero como `-1` se convierte a
  `4294967295`, la comparacion es `4294967295 < 0`, que es false.
- Con `size_t` el problema es identico pero con 64 bits: `-1` se convierte a
  `18446744073709551615` (SIZE_MAX).
- Este es uno de los bugs mas comunes en C y ha causado vulnerabilidades de
  seguridad en software real.

### Paso 3.4 — El patron seguro para comparaciones mixtas

La solucion es verificar que el valor signed no sea negativo antes de comparar:

```bash
cat > safe_comparison.c << 'EOF'
#include <stdio.h>

int main(void) {
    int s = -1;
    unsigned int u = 10;

    /* UNSAFE: direct comparison */
    printf("Unsafe: s < u = %s\n", (s < u) ? "true" : "false");

    /* SAFE: check sign first */
    if (s < 0) {
        printf("Safe: s is negative, so s < u is true\n");
    } else if ((unsigned int)s < u) {
        printf("Safe: s < u (both positive)\n");
    } else {
        printf("Safe: s >= u (both positive)\n");
    }

    return 0;
}
EOF
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic safe_comparison.c -o safe_comparison
./safe_comparison
```

Salida esperada:

```
Unsafe: s < u = false
Safe: s is negative, so s < u is true
```

### Limpieza de la parte 3

```bash
rm -f implicit_conversions safe_comparison safe_comparison.c
```

---

## Parte 4 — Warnings de GCC: detectar los problemas

**Objetivo**: Compilar el mismo codigo con diferentes niveles de warnings para
ver como GCC puede detectar problemas de signedness.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat warnings_demo.c
```

Observa cuatro escenarios:

1. Comparacion signed vs unsigned (`s < u`)
2. Asignacion signed a unsigned (`unsigned int y = x`)
3. Asignacion unsigned a signed (`int truncated = big`)
4. Patron seguro con verificacion previa

### Paso 4.2 — Compilar solo con -Wall -Wextra

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic warnings_demo.c -o warnings_demo 2>&1
```

Salida esperada:

```
warnings_demo.c: In function 'main':
warnings_demo.c:9:11: warning: comparison of integer expressions of different
    signedness: 'int' and 'unsigned int' [-Wsign-compare]
```

Solo un warning. `-Wall` incluye `-Wsign-compare`, que detecta comparaciones
entre signed y unsigned. Pero las asignaciones (lineas 17 y 22) pasan
desapercibidas.

### Paso 4.3 — Predecir

Antes de agregar mas flags:

- `-Wsign-conversion` detectara las asignaciones de las lineas 17 y 22?
- Cuantos warnings totales esperas con `-Wconversion`?

### Paso 4.4 — Compilar con -Wsign-conversion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wsign-conversion warnings_demo.c -o warnings_demo 2>&1
```

Salida esperada:

```
warnings_demo.c:9:11: warning: comparison of integer expressions of different
    signedness: 'int' and 'unsigned int' [-Wsign-compare]
warnings_demo.c:17:22: warning: conversion to 'unsigned int' from 'int' may
    change the sign of the result [-Wsign-conversion]
warnings_demo.c:22:21: warning: conversion to 'int' from 'unsigned int' may
    change the sign of the result [-Wsign-conversion]
```

Ahora GCC detecta las asignaciones peligrosas: un `int` a `unsigned int`
(puede cambiar de signo) y un `unsigned int` a `int` (puede cambiar de signo).

### Paso 4.5 — Compilar con -Wconversion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion warnings_demo.c -o warnings_demo 2>&1
```

`-Wconversion` incluye a `-Wsign-conversion`, por lo que la salida es la misma.
En otros contextos, `-Wconversion` tambien detecta perdida de precision (por
ejemplo, asignar un `long` a un `int`).

### Paso 4.6 — Ejecutar el programa

```bash
./warnings_demo
```

Salida esperada:

```
s >= u  (unexpected!)
y = 42
truncated = -1
value is negative (-10), cannot convert safely
```

Analisis:

- `s >= u`: el `-Wsign-compare` ya aviso de esto. El resultado es incorrecto.
- `y = 42`: la conversion fue segura en este caso (42 cabe en unsigned), pero
  GCC no puede saberlo en tiempo de compilacion para todos los casos.
- `truncated = -1`: `UINT_MAX` convertido a signed produce `-1` (mismos bits).
- El ultimo caso muestra el patron seguro: verificar si el valor es negativo
  antes de convertir.

### Paso 4.7 — Recomendacion practica

En proyectos reales, se recomienda compilar con al menos:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wsign-conversion
```

O para maxima seguridad:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion
```

Esto detecta la mayoria de las trampas de signedness en tiempo de compilacion.

### Limpieza de la parte 4

```bash
rm -f warnings_demo
```

---

## Limpieza final

```bash
rm -f bit_representation overflow_behavior implicit_conversions warnings_demo
rm -f safe_comparison safe_comparison.c
rm -f ub_optimization.c ub_optimization ub_optimization.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  bit_representation.c  implicit_conversions.c  overflow_behavior.c  warnings_demo.c
```

---

## Conceptos reforzados

1. Signed y unsigned usan los mismos bits pero los interpretan de forma
   diferente. La divergencia ocurre cuando el bit mas significativo es 1: a
   partir de ahi, unsigned ve valores positivos altos, y signed ve valores
   negativos (complemento a dos).

2. El overflow en unsigned es **comportamiento definido**: el resultado se
   reduce modulo 2^N (wrapping). `UINT_MAX + 1 = 0` es correcto y predecible.

3. El overflow en signed es **Undefined Behavior**. Aunque en la practica
   suele producir wrapping, el compilador puede optimizar asumiendo que nunca
   ocurre, lo que transforma la logica del programa de formas inesperadas
   (como se demostro al comparar el assembly de signed vs unsigned).

4. Cuando C compara un signed con un unsigned, convierte el signed a unsigned
   **antes** de comparar. Esto hace que valores negativos se conviertan en
   numeros enormes, invirtiendo el resultado de la comparacion.

5. `size_t` siempre es unsigned. Comparar `strlen()` con valores negativos
   produce resultados incorrectos porque el negativo se convierte a un valor
   cercano a `SIZE_MAX`.

6. `-Wall` incluye `-Wsign-compare` (detecta comparaciones mixtas). Agregar
   `-Wsign-conversion` o `-Wconversion` detecta tambien las asignaciones
   peligrosas entre signed y unsigned.

7. El patron seguro para comparaciones mixtas es verificar que el valor signed
   no sea negativo antes de hacer la comparacion o conversion.
