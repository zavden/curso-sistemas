# Lab -- Punto flotante

## Objetivo

Compilar y ejecutar programas que revelan como se comportan los tipos de
punto flotante en C: sus tamanos, precision limitada, errores de
representacion, la forma correcta de compararlos, y los valores especiales
de IEEE 754. Al finalizar, sabras por que `0.1 + 0.2 != 0.3` y como
escribir comparaciones robustas.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `sizes_precision.c` | Tamanos, rangos y precision de float, double, long double |
| `rounding_errors.c` | Errores de representacion y acumulacion |
| `compare_floats.c` | Comparacion correcta con epsilon absoluto y relativo |
| `special_values.c` | Infinity, NaN, subnormales y funciones de clasificacion |

---

## Parte 1 -- Tamanos y precision

**Objetivo**: Ver cuanto ocupa cada tipo, cuantos digitos significativos
tiene, y como la precision afecta la representacion de numeros como 0.1.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat sizes_precision.c
```

Observa:

- Usa `sizeof` para obtener el tamano en bytes de cada tipo
- Usa constantes de `<float.h>` como `FLT_DIG`, `DBL_DIG`, `LDBL_DIG`
- Imprime 0.1 con 20 decimales en los tres tipos

### Paso 1.2 -- Predecir antes de compilar

Antes de compilar, responde mentalmente:

- Cuantos bytes ocupa un `float`? Y un `double`?
- Si `float` tiene 23 bits de mantisa y `double` tiene 52, cual representara
  0.1 con mas decimales correctos?
- `long double` sera exactamente 0.1 con 20 decimales?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic sizes_precision.c -o sizes_precision
./sizes_precision
```

Salida esperada:

```
=== Sizes ===
float:       4 bytes
double:      8 bytes
long double: 16 bytes

=== Significant decimal digits ===
float:       6 (FLT_DIG)
double:      15 (DBL_DIG)
long double: 18 (LDBL_DIG)

=== Ranges ===
float:       [~1.18e-38, ~3.40e+38]
double:      [~2.23e-308, ~1.80e+308]
long double: [~3.36e-4932, ~1.19e+4932]

=== Epsilon (smallest difference near 1.0) ===
FLT_EPSILON:  ~1.19e-07
DBL_EPSILON:  ~2.22e-16
LDBL_EPSILON: ~1.08e-19

=== Precision demo ===
0.1 as float:       0.10000000149011611938
0.1 as double:      0.10000000000000000555
0.1 as long double: 0.10000000000000000000
```

Analisis:

- `float` ocupa 4 bytes (32 bits), `double` 8 (64 bits), `long double`
  16 (128 bits en x86-64, aunque solo usa 80 bits internamente en muchas
  implementaciones).
- `float` garantiza 6 digitos significativos, `double` 15, `long double` 18.
- 0.1 como `float` empieza a desviarse en el 8vo decimal. Como `double`
  la desviacion aparece en el 17vo. `long double` muestra 0.1 exacto con
  20 decimales porque tiene mas bits de mantisa.
- `FLT_EPSILON` es la menor diferencia que `float` puede distinguir cerca
  de 1.0. Si sumas algo menor que `FLT_EPSILON` a 1.0f, el resultado
  sigue siendo 1.0f.

### Limpieza de la parte 1

```bash
rm -f sizes_precision
```

---

## Parte 2 -- Errores de representacion y acumulacion

**Objetivo**: Ver el clasico `0.1 + 0.2 != 0.3`, medir como los errores
se acumulan en un millon de sumas, y observar la perdida de precision
cuando se suman numeros de magnitudes muy diferentes.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat rounding_errors.c
```

Observa tres experimentos:

- `0.1 + 0.2` comparado con `0.3` usando `==`
- Sumar `0.1f` un millon de veces con `float` y con `double`
- Sumar `1.0f` a `1e10f` (magnitudes muy diferentes)

### Paso 2.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- `0.1 + 0.2 == 0.3` sera verdadero o falso?
- Si sumas `0.1f` un millon de veces, el resultado sera exactamente
  100000.0? Cuanto error esperas?
- Si sumas 1.0 a 10000000000.0 como `float` (que tiene ~7 digitos
  significativos), el 1.0 se preservara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic rounding_errors.c -o rounding_errors
./rounding_errors
```

Salida esperada:

```
=== 0.1 + 0.2 vs 0.3 ===
0.1 + 0.2 = 0.30000000000000004441
0.3       = 0.29999999999999998890
Are they equal (==)? NO

=== Accumulation with float (1 million times 0.1f) ===
Expected: 100000.0
Got:      ~100958.34
Error:    ~958.34

=== Accumulation with double (1 million times 0.1) ===
Expected: 100000.0
Got:      ~100000.0000013329
Error:    ~0.0000013329

=== Loss of precision by magnitude ===
1e10f + 1.0f = 10000000000
Expected:      10000000001
The 1.0 was LOST (absorbed)
```

Analisis:

- `0.1 + 0.2` no da exactamente `0.3` porque 0.1 es periodico en binario
  (0.0001100110011...). Al truncar la mantisa, cada valor tiene un error
  minusculo que no se cancela al sumar.
- Con `float`, un millon de sumas de 0.1f acumulan ~958 de error. Cada suma
  individual tiene un error de ~1e-8, pero al acumular un millon de veces,
  el error crece significativamente.
- Con `double`, el error es mucho menor (~1e-6) pero no cero.
- Al sumar 1.0 a 1e10 como `float`, el 1.0 se pierde completamente. `float`
  tiene ~7 digitos significativos. El numero 10000000001 tiene 11 digitos,
  asi que los ultimos se truncan.

### Limpieza de la parte 2

```bash
rm -f rounding_errors
```

---

## Parte 3 -- Comparacion correcta de flotantes

**Objetivo**: Entender por que `==` falla con flotantes, implementar
comparacion con epsilon absoluto y relativo, y ver donde falla cada
estrategia.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat compare_floats.c
```

Observa dos funciones de comparacion:

- `approx_equal_abs()` — usa epsilon absoluto: `fabs(a - b) < epsilon`
- `approx_equal_rel()` — usa epsilon relativo: `fabs(a - b) / max(|a|, |b|) < epsilon`

El programa prueba ambas con numeros pequenos (0.1 + 0.2 vs 0.3) y con
numeros grandes (1e15 + 1 vs 1e15).

### Paso 3.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- La comparacion con epsilon absoluto 1e-9 dira que `0.1 + 0.2` y `0.3`
  son iguales?
- Si usas epsilon absoluto 1e-9 para comparar `1e15 + 1` con `1e15`,
  dira que son iguales? (Pista: la diferencia real es 1.0)
- `1.0f + FLT_EPSILON/2` sera distinguible de `1.0f`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic compare_floats.c -lm -o compare_floats
./compare_floats
```

Nota: este programa usa `fabs()` y `fmax()` de `<math.h>`, por eso
necesita `-lm` para enlazar la biblioteca matematica.

Salida esperada:

```
=== Comparison with == (WRONG) ===
0.1 + 0.2 == 0.3? NO

=== Comparison with absolute epsilon ===
epsilon = 1e-9:  EQUAL
epsilon = 1e-16: EQUAL
DBL_EPSILON = ~2.22e-16

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

Analisis:

- `==` falla porque la diferencia entre `0.1 + 0.2` y `0.3` es ~5.5e-17,
  un numero minusculo pero no cero.
- Epsilon absoluto funciona bien con numeros pequenos, pero con numeros
  grandes la diferencia real puede ser 1.0 (mucho mayor que 1e-9), asi
  que la comparacion correctamente dice NOT EQUAL. El problema es cuando
  dos numeros grandes son "casi iguales" en terminos relativos.
- Epsilon relativo normaliza la diferencia por la magnitud de los numeros.
  `1e15 + 1` vs `1e15` tiene diferencia relativa de 1e-15, que es menor
  que 1e-9, asi que los considera iguales.
- `FLT_EPSILON` (~1.19e-7) es el umbral exacto: sumar `FLT_EPSILON` a
  1.0f produce un resultado diferente, pero sumar `FLT_EPSILON/2` no
  cambia el valor (se redondea de vuelta a 1.0f).

### Limpieza de la parte 3

```bash
rm -f compare_floats
```

---

## Parte 4 -- Valores especiales

**Objetivo**: Explorar los valores especiales de IEEE 754 — infinito, NaN,
subnormales — y las funciones de clasificacion de `<math.h>`.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat special_values.c
```

Observa:

- Se generan infinito y NaN mediante operaciones aritmeticas
- Se prueban todas las comparaciones de NaN (==, !=, >, <)
- Se realizan operaciones entre valores especiales (inf - inf, inf * 0)
- Se usa `isfinite()`, `isnormal()` y subnormales

### Paso 4.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- `1.0 / 0.0` da error o devuelve algo? Y `0.0 / 0.0`?
- `NaN == NaN` es verdadero o falso?
- `inf - inf` es 0, inf, o NaN?
- `inf * 0` es 0, inf, o NaN?
- Un numero subnormal (menor que `DBL_MIN` pero mayor que 0) es `isnormal`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic special_values.c -lm -o special_values
./special_values
```

Salida esperada:

```
=== Infinity ===
1.0 / 0.0  = inf
-1.0 / 0.0 = -inf
isinf(1.0/0.0)? YES
inf + 1 = inf
inf + inf = inf

=== NaN (Not a Number) ===
0.0 / 0.0   = -nan
sqrt(-1.0)  = -nan
isnan(0.0/0.0)? YES

=== NaN comparisons ===
NaN == NaN?  NO
NaN != NaN?  YES
NaN > 0?     NO
NaN < 0?     NO
NaN == 0?    NO

=== Arithmetic with special values ===
inf - inf = -nan
inf * 0   = -nan
0 / 0     = -nan
NaN + 1   = -nan

=== Classification: isfinite, isnormal ===
isfinite(42.0)?      YES
isfinite(inf)?       NO
isfinite(NaN)?       NO
isnormal(42.0)?      YES
isnormal(0.0)?       NO
isnormal(DBL_MIN/2)? NO (subnormal)
DBL_MIN   = ~2.23e-308
DBL_MIN/2 = ~1.11e-308 (subnormal: below DBL_MIN but > 0)
```

Analisis:

- **Infinito**: dividir por cero con flotantes no es error en C -- produce
  `inf` o `-inf`. El infinito absorbe cualquier operacion finita
  (`inf + 1 = inf`).
- **NaN**: `0.0 / 0.0` y `sqrt(-1.0)` producen NaN. La propiedad mas
  importante: NaN no es igual a nada, ni a si mismo. `NaN == NaN` es
  falso. Esto es por diseno de IEEE 754 -- cualquier comparacion con NaN
  devuelve falso excepto `!=`.
- **Operaciones indefinidas**: `inf - inf`, `inf * 0` y `0 / 0` son
  indeterminadas y producen NaN.
- **NaN es contagioso**: cualquier operacion con NaN produce NaN
  (`NaN + 1 = NaN`).
- **Subnormales**: numeros entre 0 y `DBL_MIN` (~2.2e-308). Existen para
  llenar el hueco entre 0 y el menor numero normalizado. `isnormal()` los
  rechaza, pero `isfinite()` los acepta.

### Limpieza de la parte 4

```bash
rm -f special_values
```

---

## Limpieza final

```bash
rm -f sizes_precision rounding_errors compare_floats special_values
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  compare_floats.c  rounding_errors.c  sizes_precision.c  special_values.c
```

---

## Conceptos reforzados

1. `float` ocupa 4 bytes con ~6 digitos significativos, `double` 8 bytes
   con ~15, y `long double` 16 bytes con ~18. `double` es el tipo por
   defecto en C.

2. Numeros como 0.1 son periodicos en binario (0.0001100110011...) y se
   truncan al almacenarse. Por eso `0.1 + 0.2 != 0.3` -- cada operando
   tiene un error de representacion que se propaga al resultado.

3. Los errores de redondeo se acumulan: sumar `0.1f` un millon de veces
   con `float` produce ~958 de error. Con `double` el error baja a ~1e-6,
   pero no desaparece.

4. Nunca comparar flotantes con `==`. Usar epsilon absoluto
   (`fabs(a - b) < epsilon`) para valores pequenos, o epsilon relativo
   (`fabs(a - b) / max(|a|, |b|) < epsilon`) cuando los valores pueden
   tener magnitudes variadas.

5. `FLT_EPSILON` (~1.19e-7) es la menor diferencia que `float` distingue
   cerca de 1.0. Sumar `FLT_EPSILON/2` a 1.0f no tiene efecto -- se
   redondea de vuelta a 1.0f.

6. IEEE 754 define valores especiales: `inf` (division por cero),
   `NaN` (operaciones indefinidas como `0/0`). NaN no es igual a nada, ni
   a si mismo -- `NaN == NaN` es falso.

7. Los subnormales son numeros entre 0 y `DBL_MIN` que llenan el hueco
   gradualmente. `isnormal()` los rechaza pero `isfinite()` los acepta.

8. Los programas que usan `<math.h>` necesitan enlazarse con `-lm`
   (`gcc ... -lm`), de lo contrario el enlazador da "undefined reference".
