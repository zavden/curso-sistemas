# Lab — Logicos y relacionales

## Objetivo

Experimentar con operadores relacionales y logicos en C, observar que el
resultado de las comparaciones es `int` (no `bool`), entender truthiness,
dominar short-circuit evaluation con side effects, y reconocer las trampas
clasicas de comparacion y precedencia.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `relational.c` | Operadores relacionales y sizeof del resultado |
| `truthiness.c` | Valores truthy y falsy: enteros, chars, punteros |
| `short_circuit.c` | Short-circuit evaluation y side effects |
| `logical_vs_bitwise.c` | Comparacion entre & y && (y \| vs \|\|) |
| `traps.c` | Trampas: = vs ==, floats, signed/unsigned |
| `precedence.c` | Precedencia de operadores y leyes de De Morgan |

---

## Parte 1 — Operadores relacionales y truthiness

**Objetivo**: Verificar que los operadores relacionales retornan `int` (0 o 1),
y entender que valores son "verdaderos" y "falsos" en C.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat relational.c
```

Observa que el programa compara dos variables con los 6 operadores relacionales
y luego usa `sizeof` sobre el resultado.

Antes de compilar, predice:

- Que tipo retorna `a == b`? Un `bool` o un `int`?
- Cual sera el `sizeof` del resultado de una comparacion?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic relational.c -o relational
./relational
```

Salida esperada:

```
a = 10, b = 20

a == b  -> 0
a != b  -> 1
a <  b  -> 1
a >  b  -> 0
a <= b  -> 1
a >= b  -> 0

--- sizeof del resultado ---
sizeof(a == b) = 4
sizeof(a < b)  = 4
```

El `sizeof` es 4 bytes (un `int`). En C, las comparaciones retornan `int`, no
`bool`. El valor es siempre 0 (falso) o 1 (verdadero).

### Paso 1.3 — Truthiness en C

```bash
cat truthiness.c
```

Antes de compilar, predice para cada caso si el resultado sera "true" o "false":

- `if (42)` ?
- `if (-1)` ?
- `if (0)` ?
- `if ('0')` ? (Atencion: el caracter '0', no el numero 0)
- `if ('\0')` ?
- Un puntero no-NULL?
- Un puntero NULL?

### Paso 1.4 — Verificar las predicciones

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic truthiness.c -o truthiness
./truthiness
```

Salida esperada:

```
--- Valores truthy y falsy en C ---

if (42)    -> true
if (-1)    -> true
if (1)     -> true
if (0)     -> false
if (0.0)   -> false

if ('a')   -> true  (ASCII 97)
if ('0')   -> true  (ASCII 48)
if ('\0')  -> false  (ASCII 0)

if (ptr)      -> true  (points to 5)
if (null_ptr) -> false
```

Regla: en C, "falso" es exactamente 0, 0.0, NULL y '\0'. Todo lo demas es
"verdadero". El caracter `'0'` tiene valor ASCII 48, que es no-cero, por lo
tanto es verdadero.

### Limpieza de la parte 1

```bash
rm -f relational truthiness
```

---

## Parte 2 — Operadores logicos y short-circuit

**Objetivo**: Observar short-circuit evaluation en accion y entender como
interactua con side effects.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat short_circuit.c
```

El programa usa una funcion `check()` que imprime su argumento antes de
retornarlo. Esto permite ver cuales operandos se evaluan y cuales no.

Antes de compilar, predice para cada linea cuantas veces se llamara `check()`:

- `check(0) && check(1)` -- se llama a check(1)?
- `check(1) && check(0)` -- se llama a check(0)?
- `check(0) || check(1)` -- se llama a check(1)?
- `check(1) || check(0)` -- se llama a check(0)?

Recuerda las reglas de short-circuit:

- `&&` — si el primero es falso, NO evalua el segundo
- `||` — si el primero es verdadero, NO evalua el segundo

### Paso 2.2 — Verificar las predicciones

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic short_circuit.c -o short_circuit
./short_circuit
```

Salida esperada:

```
--- Short-circuit evaluation ---

check(0) A: 0
check(1) check(0) B: 0
check(0) check(1) C: 1
check(1) D: 1

--- Side effects con short-circuit ---

Antes: x=0, y=0
No entro al if
Despues: x=1, y=1

--- Segunda ronda (x ya no es 0) ---

Antes: x=1, y=0
Entro al if
Despues: x=2, y=0
```

Analisis de la primera seccion:

- **A**: `check(0)` retorna 0 (falso). Con `&&`, el resultado ya es falso. No
  llama a `check(1)`. Solo se imprime `check(0)`.
- **B**: `check(1)` retorna 1 (verdadero). Con `&&`, necesita evaluar el
  segundo. Llama a `check(0)`. Se imprimen ambos.
- **C**: `check(0)` retorna 0 (falso). Con `||`, necesita evaluar el segundo.
  Llama a `check(1)`. Se imprimen ambos.
- **D**: `check(1)` retorna 1 (verdadero). Con `||`, el resultado ya es
  verdadero. No llama a `check(0)`. Solo se imprime `check(1)`.

Analisis de los side effects:

- **Primera ronda** (x=0): `x++` evalua a 0 (falso, pero x pasa a 1). Como es
  falso, `||` evalua `y++` (y pasa a 1). Ambos incrementos ocurren. El
  resultado de `0 || 0` es 0, no entra al if.
- **Segunda ronda** (x=1): `x++` evalua a 1 (verdadero, x pasa a 2). Como es
  verdadero, `||` NO evalua `y++`. Solo x se incrementa. y se queda en 0.

### Limpieza de la parte 2

```bash
rm -f short_circuit
```

---

## Parte 3 — Errores comunes y precedencia

**Objetivo**: Provocar los errores clasicos de comparacion, observar los
warnings de GCC, y verificar las leyes de De Morgan.

### Paso 3.1 — Las 4 trampas clasicas

```bash
cat traps.c
```

Antes de compilar, predice:

- `if (x = 5)` -- que valor tendra x despues? Entrara al if?
- `(1 < 100 < 10)` -- sera verdadero o falso?
- `(0.1 + 0.2 == 0.3)` -- sera verdadero o falso?
- `int s = -1; unsigned int u = 0; (s < u)` -- sera verdadero o falso?

### Paso 3.2 — Compilar y observar los warnings

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic traps.c -o traps -lm
```

Salida esperada (warnings del compilador):

```
traps.c: ... warning: suggest parentheses around assignment used as truth value [-Wparentheses]
traps.c: ... warning: comparison of constant '10' with boolean expression is always true [-Wbool-compare]
traps.c: ... warning: comparisons like 'X<=Y<=Z' do not have their mathematical meaning [-Wparentheses]
traps.c: ... warning: comparison of integer expressions of different signedness [-Wsign-compare]
```

GCC con `-Wall -Wextra` detecta las 4 trampas. Cada warning corresponde a un
error clasico. Los flags `-Wall -Wextra -Wpedantic` son tu red de seguridad.

### Paso 3.3 — Ejecutar y verificar

```bash
./traps
```

Salida esperada:

```
--- Trampa 1: = vs == ---

if (x = 5) -> entro (x ahora vale 5)
Esto fue ASIGNACION, no comparacion.
x = 5 asigna 5, luego evalua 5 -> true.

--- Trampa 2: comparaciones encadenadas ---

val = 100
(1 < val < 10) -> 1
Parece correcto, pero 100 NO esta entre 1 y 10.
Se parsea como: (1 < 100) < 10  ->  1 < 10  ->  1 (true!)

Forma correcta: (1 < val && val < 10) -> 0

--- Trampa 3: comparar floats con == ---

0.1 + 0.2 = 0.30000000000000004441
0.3       = 0.29999999999999998890
(0.1 + 0.2 == 0.3) -> 0

Con epsilon: fabs(0.1 + 0.2 - 0.3) < 1e-9 -> 1

--- Trampa 4: signed vs unsigned ---

int s = -1, unsigned int u = 0
(s < u) -> 0
-1 se convierte a unsigned: 4294967295
Ese valor es UINT_MAX, que es mayor que 0.
```

Observa la trampa 3: los 20 decimales muestran que `0.1 + 0.2` y `0.3` tienen
representaciones binarias diferentes. Nunca comparar floats con `==`.

Observa la trampa 4: cuando se compara `signed` con `unsigned`, el valor
signed se convierte a unsigned. -1 se convierte a 4294967295 (UINT_MAX), que
es mayor que 0. El resultado de `(s < u)` es 0 (falso).

### Paso 3.4 — Logico vs bitwise

```bash
cat logical_vs_bitwise.c
```

Antes de compilar, predice los resultados de cada operacion para los tres pares
de valores:

- `a=6, b=3`: que da `a & b`? Y `a && b`?
- `c=2, d=4`: que da `c & d`? Y `c && d`?
- `e=0, f=5`: que da `e & f`? Y `e && f`?

Piensa en la diferencia: `&` opera bit a bit, `&&` solo evalua verdadero/falso.

### Paso 3.5 — Verificar logico vs bitwise

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic logical_vs_bitwise.c -o logical_vs_bitwise
./logical_vs_bitwise
```

Salida esperada:

```
--- & vs && y | vs || ---

a = 6 (0b110), b = 3 (0b011)
a & b   = 2   (AND bitwise)
a && b  = 1   (AND logico)
a | b   = 7   (OR bitwise)
a || b  = 1   (OR logico)

c = 2, d = 4
c & d   = 0   (0b010 & 0b100 = 0b000)
c && d  = 1   (ambos no-cero -> true)

e = 0, f = 5
e & f   = 0   (0b000 & 0b101 = 0b000)
e && f  = 0   (e es cero -> false, short-circuit)
e | f   = 5   (0b000 | 0b101 = 0b101)
e || f  = 1   (f es no-cero -> true)
```

Caso critico: `c=2, d=4`. Bitwise `c & d` da 0 (no comparten ningun bit en
comun), pero logico `c && d` da 1 (ambos son no-cero, ambos "verdaderos").
Confundir `&` con `&&` aqui cambia completamente el resultado.

### Paso 3.6 — Precedencia y De Morgan

```bash
cat precedence.c
```

Antes de compilar, predice:

- `a > 5 && b < 10 || c == 0` con a=10, b=3, c=0 -- como se agrupa?
- `!0 && 1` -- que se evalua primero, el `!` o el `&&`?
- `!(1 && 0)` da lo mismo que `(!1 || !0)`? (ley de De Morgan)

### Paso 3.7 — Verificar precedencia

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic precedence.c -o precedence
./precedence
```

Salida esperada:

```
--- Precedencia de operadores logicos y relacionales ---

a=10, b=3, c=0

a > 5 && b < 10 || c == 0      -> 1
(a > 5 && b < 10) || c == 0    -> 1  (asi se parsea)
a > 5 && (b < 10 || c == 0)    -> 1  (diferente agrupacion)

x = 0
!x         -> 1
!x && 1    -> 1  (! se evalua primero)
!(x && 1)  -> 1  (negacion del resultado)

p=1, q=0
!(p && q)      -> 1
(!p || !q)     -> 1  (De Morgan: equivalente)
!(p || q)      -> 0
(!p && !q)     -> 0  (De Morgan: equivalente)
```

GCC tambien emite un warning al compilar este programa:

```
warning: suggest parentheses around '&&' within '||' [-Wparentheses]
```

El compilador te advierte que mezclar `&&` y `||` sin parentesis puede ser
confuso, aunque el parseo es correcto por precedencia. La recomendacion: usar
parentesis siempre en condiciones compuestas para que la intencion sea explicita.

Orden de precedencia (de mayor a menor):

1. `!` (NOT, unario)
2. `< <= > >=` (relacionales)
3. `== !=` (igualdad)
4. `&&` (AND logico)
5. `||` (OR logico)

Las leyes de De Morgan se verifican con los dos pares de equivalencias:

- `!(p && q)` equivale a `(!p || !q)`
- `!(p || q)` equivale a `(!p && !q)`

### Limpieza de la parte 3

```bash
rm -f traps logical_vs_bitwise precedence
```

---

## Limpieza final

```bash
rm -f relational truthiness short_circuit traps logical_vs_bitwise precedence
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  logical_vs_bitwise.c  precedence.c  relational.c  short_circuit.c  traps.c  truthiness.c
```

---

## Conceptos reforzados

1. Los operadores relacionales (`==`, `!=`, `<`, `>`, `<=`, `>=`) retornan
   `int`, no `bool`. El resultado es siempre 0 (falso) o 1 (verdadero), y
   ocupa 4 bytes.

2. En C, "falso" es exactamente 0, 0.0, NULL y '\0'. Cualquier otro valor,
   incluyendo numeros negativos y el caracter '0' (ASCII 48), es "verdadero".

3. Short-circuit es una garantia del estandar: `&&` no evalua el segundo
   operando si el primero es falso; `||` no evalua el segundo si el primero
   es verdadero. Esto afecta los side effects (incrementos, llamadas a
   funciones).

4. `&` (bitwise AND) y `&&` (logico AND) son operadores completamente
   diferentes. `2 & 4` da 0 (no comparten bits), pero `2 && 4` da 1 (ambos
   son no-cero). Ademas, `&` evalua ambos operandos siempre, `&&` tiene
   short-circuit.

5. GCC con `-Wall -Wextra -Wpedantic` detecta las trampas clasicas: asignacion
   en condicion (`=` vs `==`), comparaciones encadenadas (`1 < x < 10`),
   mezcla de signed/unsigned, y `&&` dentro de `||` sin parentesis.

6. Nunca comparar floats con `==`. Usar un epsilon: `fabs(a - b) < 1e-9`.
   La representacion binaria de punto flotante introduce errores de redondeo
   que hacen que `0.1 + 0.2 != 0.3`.

7. Las leyes de De Morgan permiten transformar condiciones:
   `!(A && B)` equivale a `!A || !B`, y `!(A || B)` equivale a `!A && !B`.
   Usar parentesis en condiciones compuestas para que la intencion sea
   explicita.
