# Lab -- Macros peligrosas

## Objetivo

Demostrar los problemas mas comunes de las macros del preprocesador C y sus
soluciones. Al finalizar, sabras identificar bugs de precedencia, evaluacion
multiple de argumentos, macros multi-statement sin proteccion, y falta de
verificacion de tipos. Tambien conoceras las alternativas: do-while(0),
statement expressions de GCC, y funciones inline.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `precedence.c` | Macros sin/con parentesis, precedencia de operadores |
| `double_eval.c` | Evaluacion multiple con ++, funciones con side effects |
| `do_while.c` | Macros multi-statement: naked, braces, do-while(0) |
| `type_safety.c` | Macros vs funciones inline en verificacion de tipos |
| `stmt_expr.c` | Statement expressions de GCC, comparacion con inline |

---

## Parte 1 -- Precedencia sin parentesis

**Objetivo**: Ver como la falta de parentesis en macros produce resultados
incorrectos por precedencia de operadores. Usar `gcc -E` para ver la expansion
real.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat precedence.c
```

Observa las dos versiones de cada macro:

- `DOUBLE_BAD(x)` se define como `x + x` (sin parentesis)
- `DOUBLE_GOOD(x)` se define como `((x) + (x))` (con parentesis)
- Lo mismo para `SQUARE` e `IS_ODD`

### Paso 1.2 -- Predecir el resultado

Antes de compilar, responde mentalmente:

- `SQUARE_BAD(2 + 3)` se expande a `2 + 3 * 2 + 3`. Que valor da eso?
  (recuerda que `*` tiene mayor precedencia que `+`)
- `10 - DOUBLE_BAD(3)` se expande a `10 - 3 + 3`. Cual es el resultado?
- `IS_ODD_BAD(4 | 1)` se expande a `4 | 1 % 2 == 1`. Que se evalua primero?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic precedence.c -o precedence
```

Observa que GCC produce un warning para `IS_ODD_BAD`:

```
precedence.c: warning: suggest parentheses around comparison in operand of '|'
```

GCC detecta el problema de precedencia con `|` y sugiere parentesis. Pero no
todos los bugs de precedencia generan warnings.

```bash
./precedence
```

Salida esperada:

```
=== Precedence bugs without parentheses ===

--- SQUARE(2 + 3) ---
BAD:  SQUARE_BAD(2 + 3)  = 11  (expected 25)
GOOD: SQUARE_GOOD(2 + 3) = 25  (expected 25)

--- 10 - DOUBLE(3) ---
BAD:  10 - DOUBLE_BAD(3)  = 10  (expected 4)
GOOD: 10 - DOUBLE_GOOD(3) = 4  (expected 4)

--- 2 * DOUBLE(5) ---
BAD:  2 * DOUBLE_BAD(5)  = 15  (expected 20)
GOOD: 2 * DOUBLE_GOOD(5) = 20  (expected 20)

--- IS_ODD(4 | 1) ---
BAD:  IS_ODD_BAD(4 | 1)  = 5  (expected 1)
GOOD: IS_ODD_GOOD(4 | 1) = 1  (expected 1)

--- SQUARE in expression: 100 / SQUARE(2 + 3) ---
BAD:  100 / SQUARE_BAD(2 + 3)  = 59  (expected 4)
GOOD: 100 / SQUARE_GOOD(2 + 3) = 4  (expected 4)
```

Cada caso BAD da un resultado incorrecto. La version GOOD da el resultado
esperado.

### Paso 1.4 -- Ver la expansion con gcc -E

`gcc -E` muestra el codigo despues del preprocesador, antes de compilar. Asi
puedes ver exactamente a que se expande cada macro:

```bash
gcc -std=c11 -E precedence.c 2>/dev/null | tail -30
```

Salida esperada (extracto relevante):

```
           2 + 3 * 2 + 3);

           ((2 + 3) * (2 + 3)));

           10 - 3 + 3);

           10 - ((3) + (3)));

           2 * 5 + 5);

           2 * ((5) + (5)));

           4 | 1 % 2 == 1);

           (((4 | 1) % 2) == 1));

           100 / 2 + 3 * 2 + 3);

           100 / ((2 + 3) * (2 + 3)));
```

Ahora los bugs son evidentes:

- `SQUARE_BAD(2 + 3)` = `2 + 3 * 2 + 3` = `2 + 6 + 3` = 11
- `10 - DOUBLE_BAD(3)` = `10 - 3 + 3` = `10` (la resta y la suma son
  izquierda a derecha)
- `100 / SQUARE_BAD(2 + 3)` = `100 / 2 + 3 * 2 + 3` = `50 + 6 + 3` = 59

La regla es: envolver cada parametro en parentesis Y la expresion completa.

### Limpieza de la parte 1

```bash
rm -f precedence
```

---

## Parte 2 -- Evaluacion multiple de argumentos

**Objetivo**: Demostrar que los argumentos de una macro se evaluan cada vez que
aparecen en la expansion. Esto produce bugs cuando los argumentos tienen side
effects.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat double_eval.c
```

Observa:

- `MAX(a, b)` usa `a` y `b` dos veces en el operador ternario
- `SQUARE(x)` usa `x` dos veces en la multiplicacion
- Se compara cada macro con su equivalente inline function

### Paso 2.2 -- Ver la expansion con gcc -E

Antes de ejecutar, veamos que hace el preprocesador:

```bash
gcc -std=c11 -E double_eval.c 2>/dev/null | grep 'result ='
```

Salida esperada:

```
    result = ((x++) > (y++) ? (x++) : (y++));
    result = max_inline(x++, y++);
    result = ((x++) * (x++));
    result = square_inline(x++);
    result = ((counted_value(10)) > (counted_value(20)) ? (counted_value(10)) : (counted_value(20)));
    result = max_inline(counted_value(10), counted_value(20));
```

Observa:

- `MAX(x++, y++)` expande `x++` DOS veces: una en la comparacion y otra en
  el resultado. El incremento ocurre dos veces.
- `max_inline(x++, y++)` evalua `x++` UNA sola vez antes de llamar a la
  funcion.
- `MAX(counted_value(10), counted_value(20))` llama a la funcion TRES veces
  (dos en la comparacion, una en el resultado del ternario).

### Paso 2.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `MAX(x++, y++)` con x=5, y=3: la comparacion `(5) > (3)` es true, x pasa
  a 6, y pasa a 4. Luego se evalua `(x++)` del lado true: retorna 6 y x pasa
  a 7. Que valores tendra result, x, y?
- `SQUARE(x++)` con x=4: se expande a `(x++) * (x++)`. Que valor tiene
  result? Cuantas veces se incrementa x?
- `MAX(counted_value(10), counted_value(20))`: cuantas veces se llama
  `counted_value`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic double_eval.c -o double_eval
```

Observa el warning:

```
double_eval.c: warning: operation on 'x' may be undefined [-Wsequence-point]
```

GCC detecta que `SQUARE(x++)` expande a `(x++) * (x++)`, lo cual es
undefined behavior (dos modificaciones de `x` sin sequence point).

```bash
./double_eval
```

Salida esperada:

```
=== Multiple evaluation: MAX(x++, y++) ===

Before macro:  x=5, y=3
After MAX(x++, y++): result=6, x=7, y=4
Expected if correct: result=5, x=6, y=4

Before inline: x=5, y=3
After max_inline(x++, y++): result=5, x=6, y=4

=== Multiple evaluation: SQUARE(x++) ===

Before macro:  x=4
After SQUARE(x++): result=20, x=6
Expected if correct: result=16, x=5

Before inline: x=4
After square_inline(x++): result=16, x=5

=== Side effect: function called multiple times ===

MAX(counted_value(10), counted_value(20))
result=20, function called 3 times (expected 2)

max_inline(counted_value(10), counted_value(20))
result=20, function called 2 times (expected 2)
```

Observa:

- Con la macro `MAX`, x se incremento DOS veces (de 5 a 7) en vez de una
- Con la funcion inline, x se incremento UNA vez (de 5 a 6) como se esperaba
- `SQUARE(x++)` produce resultado incorrecto (20 en vez de 16) y x se
  incrementa dos veces
- `counted_value` se llamo 3 veces con la macro vs 2 con la funcion inline.
  En la macro: se llama 2 veces en la comparacion, y 1 vez mas en la rama del
  ternario que se toma

### Limpieza de la parte 2

```bash
rm -f double_eval
```

---

## Parte 3 -- do-while(0) para macros multi-statement

**Objetivo**: Demostrar por que las macros con multiples sentencias necesitan
`do { ... } while (0)`, y por que ni sentencias sueltas ni llaves solas
funcionan dentro de if/else.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat do_while.c
```

Observa las tres versiones del macro LOG:

- `LOG_NAKED(msg)` -- dos printf separados por `;`
- `LOG_BRACES(msg)` -- dos printf dentro de `{ }`
- `LOG_GOOD(msg)` -- dos printf dentro de `do { ... } while (0)`

### Paso 3.2 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Si escribes `if (error) LOG_NAKED("fail"); else printf("ok\n");`, que pasa
  con el segundo printf de la macro? Queda dentro del if?
- Si escribes `if (error) LOG_BRACES("fail"); else printf("ok\n");`, el `;`
  despues de la llamada cierra el bloque. Que pasa con el else?
- Por que `do { ... } while (0)` resuelve ambos problemas?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic do_while.c -o do_while
```

No debe producir errores. El codigo evita los casos que no compilarian
(LOG_NAKED y LOG_BRACES dentro de if/else sin llaves) y los explica en
comentarios.

```bash
./do_while
```

Salida esperada:

```
=== Test 1: LOG_NAKED (standalone) ===
[LOG] naked macro works alone
  (cannot use LOG_NAKED inside if/else -- would not compile)

=== Test 2: LOG_BRACES (standalone) ===
[LOG] braces macro works alone
  (cannot use LOG_BRACES inside if/else either)

=== Test 3: LOG_GOOD inside if/else ===
[LOG] error path taken
ok path

=== Test 4: All three with explicit braces around if ===
[LOG] naked (inside braces)
[LOG] braces (inside braces)
[LOG] good (inside braces)

Only LOG_GOOD works correctly without requiring the caller
to wrap the call in braces.
```

Observa:

- Las tres versiones funcionan cuando se usan solas (standalone)
- Solo `LOG_GOOD` funciona dentro de un if/else sin llaves explicas
- Si el caller agrega llaves (Test 4), todas funcionan, pero no puedes
  exigir que el caller siempre use llaves -- es responsabilidad de la macro
  ser segura en cualquier contexto

### Paso 3.4 -- Entender la expansion

Para ver por que `do-while(0)` funciona, piensa en la expansion:

```c
/* LOG_GOOD("error") se expande a: */
if (error)
    do { printf("[LOG] "); printf("%s\n", "error"); } while (0);
else
    printf("ok\n");
```

El `do { ... } while (0)` es una sola sentencia que:
1. Requiere `;` al final (como cualquier sentencia)
2. Se ejecuta exactamente una vez
3. No interfiere con if/else porque es una unidad sintactica completa

### Limpieza de la parte 3

```bash
rm -f do_while
```

---

## Parte 4 -- Type safety

**Objetivo**: Demostrar que las macros no verifican tipos de sus argumentos,
lo que permite mezclar tipos incompatibles sin error de compilacion. Las
funciones inline si verifican tipos.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat type_safety.c
```

Observa:

- `MAX(a, b)` es un macro que acepta cualquier tipo
- `max_int` y `max_double` son funciones inline con tipos especificos
- Se prueba la comparacion entre `int` y `unsigned int` (caso peligroso)

### Paso 4.2 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `MAX(-1, 1u)` compara un `int` con un `unsigned int`. Segun las reglas de
  conversion de C, el int se promueve a unsigned. Que valor tiene `-1` como
  unsigned?
- `max_int(-1, 1)` recibe ambos como `int`. Que retorna?
- Si escribes `MAX(42, "hello")`, compilaria? Y `max_int(42, "hello")`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic type_safety.c -o type_safety
```

Observa los warnings:

```
type_safety.c: warning: comparison of integer expressions of different signedness:
    'int' and 'unsigned int' [-Wsign-compare]
type_safety.c: warning: operand of '?:' changes signedness from 'int' to
    'unsigned int' due to unsignedness of other operand [-Wsign-compare]
```

GCC advierte sobre la mezcla signed/unsigned en la macro, pero no es un error
-- el codigo compila y ejecuta. Con `-Wsign-compare` (incluido en `-Wextra`)
se ve el warning, pero sin esos flags pasaria inadvertido.

```bash
./type_safety
```

Salida esperada:

```
=== Type safety: macro vs inline function ===

--- Correct: same types ---
MAX(10, 20)       = 20
max_int(10, 20)   = 20
MAX(3.14, 2.71)   = 3.14
max_double(3.14, 2.71) = 3.14

--- Dangerous: int vs unsigned ---
MAX(-1, 1u)       = -1  (macro, -1 promoted to unsigned!)
max_int(-1, 1)    = 1  (function, types match)

--- Dangerous: int vs pointer ---
The macro MAX(42, "hello") would compile with only a warning.
A function max_int(42, "hello") would NOT compile (type error).

--- Practical: signed/unsigned mismatch ---
threshold = -5, strlen("abc") = 3
MAX(threshold, (int)len) = 3  (cast to avoid mismatch)

--- Summary ---
Macros: no type checking, silent promotion, hard-to-find bugs
Inline functions: compiler enforces types, clear error messages
```

Observa:

- `MAX(-1, 1u)` retorna -1 porque la comparacion se hace en unsigned.
  `-1` como unsigned es `UINT_MAX` (4294967295), que es mayor que 1.
  El resultado se imprime como int (-1), pero la logica de comparacion
  fue incorrecta.
- `max_int(-1, 1)` retorna 1 correctamente porque ambos son `int`
- La macro acepta tipos incompatibles; la funcion inline los rechaza

### Limpieza de la parte 4

```bash
rm -f type_safety
```

---

## Parte 5 -- Statement expressions de GCC y funciones inline

**Objetivo**: Conocer las statement expressions de GCC (`({ ... })`) como
solucion al problema de evaluacion multiple, y compararlas con funciones
inline.

### Paso 5.1 -- Examinar el codigo fuente

```bash
cat stmt_expr.c
```

Observa:

- `MAX_UNSAFE` es la macro clasica con evaluacion multiple
- `MAX_SAFE` usa una statement expression: guarda los argumentos en variables
  temporales con `__typeof__`, evaluando cada argumento una sola vez
- `CLAMP` es un ejemplo practico con tres argumentos
- Cada macro tiene su equivalente funcion inline para comparar

### Paso 5.2 -- Intentar compilar con -std=c11

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic stmt_expr.c -o stmt_expr 2>&1 | head -10
```

Salida esperada (extracto):

```
stmt_expr.c: error: implicit declaration of function 'typeof'
stmt_expr.c: warning: ISO C forbids braced-groups within expressions [-Wpedantic]
```

Las statement expressions (`({ ... })`) y `__typeof__` son extensiones de GCC,
no parte del estandar C11. Con `-std=c11 -Wpedantic`, GCC las rechaza.

### Paso 5.3 -- Compilar con -std=gnu11

Para usar extensiones de GCC, se necesita `-std=gnu11` en vez de `-std=c11`,
y se omite `-Wpedantic` (que prohibe extensiones):

```bash
gcc -std=gnu11 -Wall -Wextra stmt_expr.c -o stmt_expr
```

Compila sin errores ni warnings.

### Paso 5.4 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `MAX_UNSAFE(x++, y++)` con x=5, y=3: ya sabes que x se incrementa dos
  veces. Que valores dara?
- `MAX_SAFE(x++, y++)`: la statement expression evalua `x++` una sola vez
  y guarda el resultado en `_a`. Que valores dara?
- `CLAMP(15, 0, 10)`: si val > hi, retorna hi. Que retorna?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.5 -- Ejecutar y verificar

```bash
./stmt_expr
```

Salida esperada:

```
=== Statement expressions (GCC extension) ===

--- MAX with x++, y++ ---
MAX_UNSAFE(x++, y++): result=6, x=7, y=4
MAX_SAFE(x++, y++):   result=5, x=6, y=4
max_inline(x++, y++): result=5, x=6, y=4

--- SQUARE with x++ ---
SQUARE_SAFE(x++):   result=16, x=5  (correct)
square_inline(x++): result=16, x=5  (correct)

--- CLAMP(value, low, high) ---
CLAMP(15, 0, 10)    = 10  (clamped to 10)
CLAMP(-5, 0, 10)    = 0  (clamped to 0)
CLAMP(7, 0, 10)     = 7  (within range)
clamp_inline(15, 0, 10) = 10
clamp_inline(-5, 0, 10) = 0
clamp_inline(7, 0, 10)  = 7

--- Type genericity ---
MAX_SAFE(3.14, 2.71) = 3.14  (works with double)
max_inline(3, 2)     = 3    (only works with int)

--- Comparison ---
Standard macro:       multiple evaluation, type-generic, portable
Statement expression: single evaluation, type-generic, GCC/Clang only
Inline function:      single evaluation, type-safe, portable, debuggable
```

Observa:

- `MAX_SAFE` y `max_inline` dan los mismos resultados correctos (x=6, y=4)
- `MAX_UNSAFE` tiene el bug de evaluacion multiple (x=7)
- `CLAMP` funciona correctamente con la statement expression
- La statement expression mantiene la genericidad de tipos (funciona con
  `double`), a diferencia de la funcion inline que solo acepta `int`
- La desventaja: las statement expressions no son estandar C, solo funcionan
  con GCC y Clang

### Paso 5.6 -- Cuando usar cada alternativa

Resumen de las tres opciones:

| Caracteristica | Macro clasica | Statement expression | Inline function |
|----------------|---------------|----------------------|-----------------|
| Evaluacion unica | No | Si | Si |
| Type-safe | No | No | Si |
| Type-generic | Si | Si | No (una por tipo) |
| Portable | Si | Solo GCC/Clang | Si |
| Debuggable | No (se expande) | No (se expande) | Si (aparece en gdb) |
| Puede usar #, ## | Si | Si | No |
| Puede usar __FILE__, __LINE__ | Si | Si | No (del caller) |

Regla practica: usar funciones inline siempre que sea posible. Usar macros
solo cuando se necesita stringify (#), token pasting (##), `__FILE__`/`__LINE__`
del caller, o genericidad de tipos.

### Limpieza de la parte 5

```bash
rm -f stmt_expr
```

---

## Limpieza final

```bash
rm -f precedence double_eval do_while type_safety stmt_expr
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  do_while.c  double_eval.c  precedence.c  stmt_expr.c  type_safety.c
```

---

## Conceptos reforzados

1. Los macros sin parentesis alrededor de cada parametro y de la expresion
   completa producen bugs de precedencia. `gcc -E` permite ver la expansion
   real y entender exactamente donde falla la precedencia.

2. Los argumentos de una macro se evaluan cada vez que aparecen en la
   expansion. Pasar expresiones con side effects (`x++`, llamadas a funciones)
   produce evaluaciones multiples con resultados incorrectos.

3. Las macros multi-statement requieren `do { ... } while (0)` para
   funcionar como una sola sentencia dentro de if/else. Ni sentencias sueltas
   ni llaves solas resuelven el problema porque el `;` del caller rompe la
   estructura del if.

4. Las macros no verifican tipos de sus argumentos. Mezclar `int` con
   `unsigned int` produce resultados silenciosamente incorrectos por las
   reglas de promotion de C. Las funciones inline rechazan tipos incompatibles
   en tiempo de compilacion.

5. Las statement expressions de GCC (`({ ... })`) resuelven la evaluacion
   multiple guardando cada argumento en una variable temporal con
   `__typeof__`. Mantienen la genericidad de tipos pero no son estandar C.

6. Las funciones inline son la alternativa preferida: evaluacion unica,
   type-safe, portables, y visibles en el debugger. Usar macros solo cuando
   se necesita stringify, token pasting, `__FILE__`/`__LINE__` del caller,
   o genericidad de tipos que no se puede lograr con `_Generic`.

7. `gcc -E` es la herramienta fundamental para diagnosticar bugs en macros:
   muestra el codigo despues de la expansion del preprocesador, revelando
   exactamente a que se expande cada invocacion.
