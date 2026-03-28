# Lab -- Operadores aritmeticos y de asignacion

## Objetivo

Observar el comportamiento real de los operadores aritmeticos y de asignacion
en C: como trunca la division entera, que signo tiene el modulo con negativos,
la diferencia entre pre y post incremento, como se promueven los tipos en
expresiones mixtas, y como la precedencia determina el resultado de una
expresion. Al finalizar, sabras predecir el resultado de expresiones aritmeticas
sin necesidad de ejecutarlas.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `int_division.c` | Division entera, modulo, invariante, conversion a horas |
| `compound_assign.c` | Operadores compuestos, pre/post incremento/decremento |
| `promotion.c` | Promocion de enteros, conversiones mixtas, signed/unsigned |
| `precedence.c` | Precedencia, asociatividad, expresiones complejas |

---

## Parte 1 -- Division entera y modulo

**Objetivo**: Verificar que la division entera trunca hacia cero (no redondea) y
que el signo del modulo sigue al dividendo.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat int_division.c
```

Observa las cuatro combinaciones de signos en la division y el modulo, la
verificacion del invariante `(a/b)*b + a%b == a`, y la conversion de segundos
a horas/minutos/segundos.

### Paso 1.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- `-17 / 5` da -3 o -4? (recuerda: trunca hacia cero, no redondea)
- `-17 % 5` da -2 o 2? (recuerda: el signo sigue al dividendo)
- `17 / 5` como entero da 3. Y `17.0 / 5`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic int_division.c -o int_division
./int_division
```

Salida esperada:

```
=== Integer division (truncation toward zero) ===

 17 / 5  = 3
-17 / 5  = -3
 17 / -5 = -3
-17 / -5 = 3

=== Modulo (sign follows dividend, C99+) ===

 17 %  5 = 2
-17 %  5 = -2
 17 % -5 = 2
-17 % -5 = -2

=== Invariant: (a/b)*b + a%b == a ===

a = -17, b = 5
a/b = -3, a%b = -2
(a/b)*b + a%b = -3 * 5 + -2 = -17
Equals a? YES

=== Practical: seconds to h/m/s ===

3661 seconds = 1h 1m 1s

=== Float vs integer division ===

17 / 5     = 3   (integer)
17.0 / 5   = 3.400000   (float)
(double)17 / 5 = 3.400000   (cast)
```

### Paso 1.4 -- Analisis

Puntos clave de la salida:

- **Truncamiento hacia cero**: `-17 / 5` da `-3`, no `-4`. C trunca hacia cero,
  no hacia menos infinito. Esto es diferente de Python, donde `-17 // 5` da `-4`.

- **Signo del modulo**: `-17 % 5` da `-2`. Desde C99, el signo del resultado
  del modulo siempre sigue al dividendo (el operando izquierdo). El divisor
  negativo no afecta el signo del resultado.

- **Invariante**: `(a/b)*b + a%b == a` siempre se cumple. Esta es la definicion
  formal que conecta division y modulo.

- **Division real**: Basta con que uno de los operandos sea `double` (o `float`)
  para obtener division real. El cast `(double)17` o el literal `17.0` logran
  lo mismo.

### Limpieza de la parte 1

```bash
rm -f int_division
```

---

## Parte 2 -- Asignacion compuesta e incremento

**Objetivo**: Verificar el efecto de los operadores compuestos (`+=`, `-=`, etc.)
y la diferencia critica entre pre-incremento (`++x`) y post-incremento (`x++`).

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat compound_assign.c
```

Observa como `x` se modifica en cadena con cada operador compuesto, y las
cuatro combinaciones de pre/post con incremento/decremento.

### Paso 2.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- Si `x = 10` y luego `x += 5; x -= 3; x *= 2; x /= 4; x %= 3`, cual es el
  valor final de `x`?
- Si `a = 5` y `b = a++`, cual es el valor de `a` y de `b`?
- Si `a = 5` y `b = ++a`, cual es el valor de `a` y de `b`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic compound_assign.c -o compound_assign
./compound_assign
```

Salida esperada:

```
=== Compound assignment operators ===

Initial x = 10

x += 5  -> x = 15
x -= 3  -> x = 12
x *= 2  -> x = 24
x /= 4  -> x = 6
x %= 3  -> x = 0

=== Pre-increment vs post-increment ===

a = 5; b = ++a; -> a = 6, b = 6  (prefix: increment THEN use)
a = 5; b = a++; -> a = 6, b = 5  (postfix: use THEN increment)

=== Pre-decrement vs post-decrement ===

a = 5; b = --a; -> a = 4, b = 4  (prefix)
a = 5; b = a--; -> a = 4, b = 5  (postfix)

=== In standalone statements, no difference ===

a = 10; a++; -> a = 11
a = 10; ++a; -> a = 11

=== Chained assignment ===

p = q = r = 42; -> p = 42, q = 42, r = 42
```

### Paso 2.4 -- Analisis

Puntos clave de la salida:

- **Asignacion compuesta**: `x += 5` es equivalente a `x = x + 5`. Cada
  operador compuesto aplica la operacion y asigna el resultado. La cadena
  completa: 10 -> 15 -> 12 -> 24 -> 6 -> 0.

- **Pre vs post incremento**: Esta es la distincion mas importante.
  `++a` incrementa primero y luego retorna el valor nuevo (`b = 6`).
  `a++` retorna el valor actual y luego incrementa (`b = 5`, pero `a` ya es 6).

- **Statements independientes**: Cuando no se usa el valor de retorno (`a++;`
  como sentencia sola), no hay diferencia entre pre y post. Ambos solo
  incrementan `a`.

- **Asignacion encadenada**: `p = q = r = 42` se evalua de derecha a izquierda
  porque la asignacion es asociativa a la derecha. Primero `r = 42`, que retorna
  42, luego `q = 42`, que retorna 42, y finalmente `p = 42`.

### Limpieza de la parte 2

```bash
rm -f compound_assign
```

---

## Parte 3 -- Promocion de enteros y conversiones en expresiones mixtas

**Objetivo**: Observar como C promueve automaticamente `char` y `short` a `int`
antes de operar, y como se comportan las expresiones que mezclan `int` con
`double` o `signed` con `unsigned`.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat promotion.c
```

Observa como se usa `sizeof` para verificar el tipo resultante de cada
expresion, y la comparacion peligrosa entre `signed` y `unsigned`.

### Paso 3.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- `sizeof(a)` es 1 (un `unsigned char`). Cuanto vale `sizeof(a + b)`?
- `unsigned char a = 200, b = 100`. Cuanto vale `a + b`? Se produce overflow?
- Si guardamos `a + b` (300) en un `unsigned char c`, que valor tendra `c`?
- `sizeof(int + double)` es 4 u 8?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic promotion.c -o promotion
./promotion
```

Salida esperada:

```
=== Integer promotion: char + char -> int ===

a = 200 (unsigned char)
b = 100 (unsigned char)
sizeof(a)     = 1 bytes
sizeof(a + b) = 4 bytes  (promoted to int!)
a + b = 300  (no overflow, computed as int)

=== Truncation when storing back in char ===

unsigned char c = a + b;
c = 44  (300 % 256 = 44, wraps around)
signed char sc = a + b;
sc = 44  (implementation-defined truncation)

=== Usual arithmetic conversions (mixed types) ===

int i = 5, double d = 2.500000
i + d = 7.500000  (i promoted to double)
i * d = 12.500000  (i promoted to double)
sizeof(i + d) = 8 bytes  (result is double)

=== Dangerous: signed + unsigned ===

int negative = -1
unsigned int positive = 1
With cast: negative < positive  (correct)
(unsigned)(-1) + 1 = 0  (-1 becomes 4294967295, then +1 wraps to 0)
```

### Paso 3.4 -- Analisis

Puntos clave de la salida:

- **Promocion a int**: Antes de cualquier operacion aritmetica, `char` y `short`
  se promueven a `int`. Por eso `sizeof(a + b)` es 4 (un `int`), no 1. La suma
  200 + 100 = 300 no desborda porque se calcula como `int`.

- **Truncamiento al almacenar**: Cuando se guarda 300 en un `unsigned char`
  (rango 0-255), el valor se trunca: 300 % 256 = 44. Los bits superiores
  simplemente se descartan.

- **Conversiones mixtas**: Cuando se opera `int + double`, el `int` se promueve
  a `double`. El resultado es `double` (8 bytes). La regla es: el tipo "menor"
  se convierte al tipo "mayor".

- **signed vs unsigned**: `-1` como `unsigned int` es 4294967295 (UINT_MAX).
  Sumarle 1 produce 0 por wrap around. Este es un error comun cuando se comparan
  o se operan valores `signed` con `unsigned` sin precaucion.

### Limpieza de la parte 3

```bash
rm -f promotion
```

---

## Parte 4 -- Precedencia y asociatividad

**Objetivo**: Verificar como la precedencia y la asociatividad determinan el
resultado de expresiones sin parentesis, y por que los parentesis explicitos
mejoran la legibilidad.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat precedence.c
```

Observa las expresiones con diferentes operadores y como se agrupan segun la
precedencia.

### Paso 4.2 -- Predecir los resultados

Antes de compilar, responde mentalmente:

- `2 + 3 * 4` da 14 o 20?
- `10 - 3 - 2` da 5 o 9? (piensa en la asociatividad)
- `2 * 3 % 4` da 2 o 6? (piensa en la precedencia de * y %)
- `15 / 4 * 4` da 15 o 12? (piensa en la division entera)
- `2 + 3 * 4 - 6 / 2` cuanto da?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic precedence.c -o precedence
./precedence
```

Salida esperada:

```
=== Precedence: * / % before + - ===

2 + 3 * 4   = 14   (multiplication first)
(2 + 3) * 4 = 20   (parentheses override)

=== Associativity: left to right for - ===

10 - 3 - 2 = 5   ((10 - 3) - 2, left to right)

=== Mixed * and % (same precedence, left to right) ===

2 * 3 % 4 = 2   ((2 * 3) % 4 = 6 % 4)
15 / 4 * 4 = 12   ((15 / 4) * 4 = 3 * 4)

=== Assignment: right to left ===

p = q = r = 7  -> p=7, q=7, r=7   (r=7 first, then q=7, then p=7)

=== Complex expression ===

2 + 3 * 4 - 6 / 2 = 11
Breakdown: 2 + (3*4) - (6/2) = 2 + 12 - 3 = 11

=== Why parentheses matter ===

x = 10
x + 4 / 2   = 12   (division first: x + 2)
(x + 4) / 2 = 7   (addition first: 14 / 2)
```

### Paso 4.4 -- Analisis

Puntos clave de la salida:

- **Precedencia**: `*`, `/` y `%` se evaluan antes que `+` y `-`. Por eso
  `2 + 3 * 4` es 14 (no 20). Los parentesis anulan la precedencia.

- **Asociatividad izquierda**: `10 - 3 - 2` se evalua como `(10 - 3) - 2 = 5`.
  Si fuera derecha seria `10 - (3 - 2) = 9`. Los operadores `*`, `/`, `%`, `+`
  y `-` son todos asociativos a la izquierda.

- **Misma precedencia**: `*` y `%` tienen la misma precedencia, asi que se
  aplica la asociatividad (izquierda). `2 * 3 % 4` se evalua como
  `(2 * 3) % 4 = 6 % 4 = 2`.

- **Division entera en cadena**: `15 / 4 * 4` da 12, no 15. Primero
  `15 / 4 = 3` (truncamiento), luego `3 * 4 = 12`. La informacion se pierde
  en la division entera y no se recupera al multiplicar.

- **Asignacion asociativa a la derecha**: `p = q = r = 7` se evalua como
  `p = (q = (r = 7))`. La asignacion es uno de los pocos operadores con
  asociatividad derecha.

- **Regla practica**: Ante la duda, usar parentesis. `(x + 4) / 2` es mas claro
  que memorizar la tabla de precedencia.

### Limpieza de la parte 4

```bash
rm -f precedence
```

---

## Limpieza final

```bash
rm -f int_division compound_assign promotion precedence
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  compound_assign.c  int_division.c  precedence.c  promotion.c
```

---

## Conceptos reforzados

1. La division entera en C **trunca hacia cero**, no redondea. `-17 / 5` da
   `-3`, no `-4`. Esto es diferente del comportamiento de Python (`//`).

2. El signo del resultado de `%` **sigue al dividendo** (operando izquierdo)
   desde C99. El invariante `(a/b)*b + a%b == a` siempre se cumple.

3. Los operadores compuestos (`+=`, `-=`, etc.) combinan operacion y asignacion.
   `x += 5` es equivalente a `x = x + 5`.

4. `++x` (prefijo) incrementa **antes** de usar el valor. `x++` (postfijo)
   usa el valor **antes** de incrementar. En sentencias independientes no hay
   diferencia.

5. Antes de cualquier operacion aritmetica, `char` y `short` se **promueven a
   int**. `sizeof(char + char)` es 4, no 1. Esto evita overflows en operaciones
   intermedias.

6. En expresiones mixtas (`int + double`), el tipo menor se convierte al mayor.
   El resultado de `int + double` es `double`.

7. Mezclar `signed` y `unsigned` es peligroso: `-1` como `unsigned int` vale
   4294967295 (UINT_MAX). Siempre usar cast explicito o evitar la mezcla.

8. `*`, `/` y `%` tienen mayor precedencia que `+` y `-`. `15 / 4 * 4` da 12,
   no 15, porque la division entera pierde informacion que la multiplicacion
   no recupera.

9. La asignacion (`=`) es asociativa a la derecha: `a = b = c = 0` se evalua
   como `a = (b = (c = 0))`.

10. Ante la duda sobre precedencia, **usar parentesis**. La legibilidad importa
    mas que memorizar la tabla.
