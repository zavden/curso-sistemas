# Lab — Operador coma y ternario

## Objetivo

Practicar el operador ternario (`?:`) y el operador coma (`,`) en contextos
reales: asignaciones condicionales, inicializacion de `const`, bucles `for`
con multiples variables, y entender como la precedencia afecta la evaluacion.
Al finalizar, sabras cuando usar cada operador y cuando evitarlo.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `ternary_basic.c` | Ternario: seleccion, printf, const, plural |
| `ternary_types.c` | Ternario: promocion de tipos, asociatividad derecha |
| `comma_basics.c` | Coma: valor de retorno, precedencia vs asignacion, for |
| `comma_vs_separator.c` | Coma como operador vs coma como separador |
| `precedence_demo.c` | Precedencia entre ternario, coma y asignacion |

---

## Parte 1 — Operador ternario

**Objetivo**: Usar el ternario para asignaciones condicionales, argumentos de
funciones, y inicializacion de `const`. Observar la promocion de tipos y la
asociatividad.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat ternary_basic.c
```

Observa los distintos usos del ternario:

- Seleccionar el mayor de dos valores
- Pasar una cadena condicional a `printf`
- Calcular un valor absoluto
- Formatear plurales (`item` vs `items`)
- Inicializar una variable `const`

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ternary_basic.c -o ternary_basic
```

Antes de ejecutar, predice:

- Cuando `x = -5`, que imprimira `(x > 0) ? "positive" : "non-positive"`?
- Para `n = 1`, que cadena seleccionara `(n == 1) ? "" : "s"`?
- Si `debug = 1`, cual sera el valor de `mode`?

### Paso 1.3 — Verificar las predicciones

```bash
./ternary_basic
```

Salida esperada:

```
a=7, b=3, max=7
x is non-positive
x=-5, abs(x)=5
Found 0 items
Found 1 item
Found 2 items
Found 3 items
mode = DEBUG
```

Observa:

- El ternario es una **expresion**: retorna un valor que se puede asignar,
  pasar como argumento, o usar en cualquier contexto que acepte un valor.
- Usar `(n == 1) ? "" : "s"` evita un `if/else` entero para manejar plurales.
- `const char *mode` solo se puede inicializar una vez. Con `if/else` no se
  podria declarar como `const` y asignar condicionalmente en el mismo paso.

### Paso 1.4 — Promocion de tipos y asociatividad

```bash
cat ternary_types.c
```

Observa dos conceptos nuevos:

- Cuando las ramas del ternario tienen tipos distintos (`int` vs `double`),
  se aplican las conversiones aritmeticas usuales.
- El ternario asocia de **derecha a izquierda**: `a ? b : c ? d : e` se
  parsea como `a ? b : (c ? d : e)`.

### Paso 1.5 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ternary_types.c -o ternary_types
```

Antes de ejecutar, predice:

- Si `condition = 1` y las ramas son `int i = 5` y `double d = 3.14`, cual es
  el tipo del resultado? Sera `int` o `double`?
- `sizeof(condition ? i : d)` sera `sizeof(int)` o `sizeof(double)`?
- Con `a=0, c=1`: en `a ? b : (c ? d : e)`, que rama se selecciona?

### Paso 1.6 — Verificar las predicciones

```bash
./ternary_types
```

Salida esperada:

```
condition=1: result = 5.000000 (from int 5, promoted to double)
condition=0: result = 3.140000 (double branch selected)

sizeof(int)    = 4
sizeof(double) = 8
sizeof( condition ? i : d ) = 8

n=2 -> "two"

a ? b : c ? d : e
  a=0, b=10, c=1, d=20, e=30
  result (implicit grouping) = 20
  result (explicit grouping) = 20
```

Observa:

- Aunque `condition = 1` selecciona la rama `int`, el **tipo de toda la
  expresion** es `double` (el tipo mas amplio de ambas ramas). El `int` se
  promueve a `double`. `sizeof` confirma esto: el resultado ocupa 8 bytes.
- La asociatividad derecha permite encadenar ternarios, pero un `switch` o
  `if/else` es mas legible.

### Limpieza de la parte 1

```bash
rm -f ternary_basic ternary_types
```

---

## Parte 2 — Operador coma

**Objetivo**: Entender la evaluacion del operador coma, su valor de retorno,
y distinguirlo del separador de declaraciones y argumentos.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat comma_basics.c
```

Observa:

- `(1, 2, 3)` usa la coma como **operador**: evalua de izquierda a derecha y
  retorna el ultimo valor.
- La coma en `int a = 0, b = 0` es un **separador** de declaraciones.
- El uso mas comun del operador coma es en bucles `for`.

### Paso 2.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic comma_basics.c -o comma_basics
```

Observa los warnings:

```
comma_basics.c:5:15: warning: left-hand operand of comma expression has no effect [-Wunused-value]
    5 |     int x = (1, 2, 3);
```

El compilador advierte que `1` y `2` se evaluan pero su valor se descarta.
Esto es intencional en este ejemplo didactico, pero en codigo real indica un
error o mal estilo.

### Paso 2.3 — Ejecutar

Antes de ejecutar, predice:

- Que valor tendra `x` despues de `int x = (1, 2, 3)`?
- En `y = (1, z = 99)`, que valores tendran `y` y `z`?
- En el `for` con `i = 0, j = 5` e `i++, j--`, cuantas iteraciones habra?

```bash
./comma_basics
```

Salida esperada:

```
x = (1, 2, 3) -> x = 3
(a = 10, b = 20, a + b) -> result = 30
  a = 10, b = 20

y = 1, z = 2 -> y=1, z=2
y = (1, z = 99) -> y=99, z=99

for loop with comma operator:
  i=0, j=5
  i=1, j=4
  i=2, j=3
```

Observa:

- `(1, 2, 3)` evalua las tres expresiones y retorna `3`.
- `y = 1, z = 2` se parsea como `(y = 1), (z = 2)` porque la asignacion tiene
  **mayor precedencia** que la coma.
- `y = (1, z = 99)` se parsea como `y = ((1), (z = 99))`. La coma descarta
  `1` y retorna `z = 99` (que vale `99`), asi que `y = 99`.
- El `for` itera mientras `i < j`: empieza con `i=0, j=5` y converge en 3
  iteraciones.

### Paso 2.4 — Coma operador vs coma separador

```bash
cat comma_vs_separator.c
```

Este programa muestra los dos roles de la coma en C. Observa especialmente:

- En declaraciones, argumentos de funcion, e inicializadores de array, la coma
  es un **separador** (no un operador).
- Para forzar la coma como operador dentro de un argumento de funcion, se
  necesitan **parentesis**: `add((a++, b), 100)`.

### Paso 2.5 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic comma_vs_separator.c -o comma_vs_separator
```

Antes de ejecutar, predice:

- En `add((a++, b), 100)` con `a=1, b=2`: cuantos argumentos recibe `add`?
  Que valor tiene el primer argumento?
- El `while (val = data[idx++], val >= 0)`: que evalua la coma? Cual es la
  condicion real del bucle?

```bash
./comma_vs_separator
```

Salida esperada:

```
Declaration separator: a=1, b=2, c=3
Function arg separator: add(1, 2) = 3
Initializer separator: arr = {10, 20, 30}

--- Comma as operator ---
add((a++, b), 100): a is now 2, r=102

while with comma operator:
  val = 10
  val = 20
  val = 30
Processed 3 values
```

Observa:

- `add((a++, b), 100)` recibe **dos** argumentos: la coma dentro del
  parentesis evalua `a++` (side effect: `a` pasa de 1 a 2), luego retorna `b`
  (que es 2). Asi, `add(2, 100) = 102`.
- En el `while`, la coma primero ejecuta `val = data[idx++]` (lee el dato),
  luego evalua `val >= 0` (la condicion real). Cuando `val` es `-1`, el bucle
  termina.

### Limpieza de la parte 2

```bash
rm -f comma_basics comma_vs_separator
```

---

## Parte 3 — Precedencia completa

**Objetivo**: Ver como interactuan la coma, el ternario, y la asignacion en
expresiones combinadas. Consultar la tabla de precedencia para predecir el
parseo.

### Paso 3.1 — Tabla de referencia

Antes de los ejercicios, consulta la tabla de precedencia (extracto de los
niveles relevantes):

```
Nivel  Operador                     Asociatividad
──────────────────────────────────────────────────
 3     * / %                        izquierda
 4     + -                          izquierda
 6     < <= > >=                    izquierda
 7     == !=                        izquierda
 8     &  (bitwise AND)             izquierda
 9     ^  (bitwise XOR)             izquierda
10     |  (bitwise OR)              izquierda
11     &&                           izquierda
12     ||                           izquierda
13     ?:                           derecha
14     = += -= *= /= etc.           derecha
15     ,                            izquierda
```

Regla practica: **aritmetica > comparacion > ternario > asignacion > coma**.

### Paso 3.2 — Examinar el codigo fuente

```bash
cat precedence_demo.c
```

Observa las expresiones que combinan multiples operadores. Intenta parsear
cada una mentalmente usando la tabla de precedencia.

### Paso 3.3 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic precedence_demo.c -o precedence_demo
```

El warning en `p = (1, 2)` es esperado (la coma descarta el `1`).

### Paso 3.4 — Predecir y ejecutar

Antes de ejecutar, predice el resultado de estas expresiones:

1. `p = 1, q = 2` -- se parsea como `(p = 1), (q = 2)` o como
   `p = (1, q = 2)`?
2. `r = val > 5 ? val * 2 : val + 1, printf(...)` con `val = 7` -- cual es
   el valor de `r`? El `printf` se ejecuta?
3. `(flags & 0x01) ? "set" : "clear"` con `flags = 0x0F` -- por que son
   necesarios los parentesis alrededor de `flags & 0x01`?

```bash
./precedence_demo
```

Salida esperada:

```
Ternary + assignment: max = 5
Without parens on condition: max = 5

condition=1: x=10, y=0
condition=0: x=0, y=20

p = 1, q = 2: p=1, q=2
p = (1, 2): p=2
Side effect in comma
r = 14

flags = 0x0F
(flags & 0x01) ? set : clear -> set
```

Analisis:

- `p = 1, q = 2`: la asignacion tiene mayor precedencia que la coma, asi que
  se parsea como `(p = 1), (q = 2)`. Son dos expresiones separadas.
- `r = val > 5 ? val * 2 : val + 1, printf(...)`: se parsea como
  `(r = (val > 5 ? val * 2 : val + 1)), (printf(...))`. Primero se evalua
  `val > 5` (verdadero), luego `val * 2 = 14`, se asigna a `r`. Despues la
  coma ejecuta `printf` como side effect. `r = 14`.
- `flags & 0x01 ? "set" : "clear"` sin parentesis se parsearia como
  `flags & (0x01 ? "set" : "clear")` porque `?:` (nivel 13) tiene mayor
  precedencia que `&` (nivel 8). Con `(flags & 0x01)` se fuerza el AND
  primero.

### Limpieza de la parte 3

```bash
rm -f precedence_demo
```

---

## Limpieza final

```bash
rm -f ternary_basic ternary_types comma_basics comma_vs_separator precedence_demo
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  comma_basics.c  comma_vs_separator.c  precedence_demo.c  ternary_basic.c  ternary_types.c
```

---

## Conceptos reforzados

1. El operador ternario (`?:`) es una **expresion**, no un statement. Puede
   usarse en asignaciones, argumentos, retornos, y para inicializar `const`
   -- algo imposible con `if/else`.

2. Cuando las ramas del ternario tienen tipos distintos, se aplican las
   **conversiones aritmeticas usuales**: el tipo del resultado es el mas
   amplio de ambas ramas, independientemente de cual se seleccione.

3. El ternario asocia de **derecha a izquierda**: `a ? b : c ? d : e` se
   parsea como `a ? b : (c ? d : e)`. Esto permite encadenar, pero un
   `switch` o `if/else` es mas legible.

4. El operador coma evalua de izquierda a derecha y retorna el valor de la
   **expresion derecha**. Los valores intermedios se descartan.

5. La coma tiene **dos roles** en C: como separador (declaraciones, argumentos,
   inicializadores) y como operador. Para usar el operador coma donde se espera
   un separador, se necesitan parentesis.

6. El uso mas comun y legitimo del operador coma es en bucles `for` con
   multiples variables: `for (int i = 0, j = n; i < j; i++, j--)`.

7. La coma es el operador de **menor precedencia** (nivel 15). La asignacion
   (nivel 14) tiene mayor precedencia, por eso `x = 1, y = 2` se parsea como
   `(x = 1), (y = 2)`.

8. El operador `&` (bitwise AND) tiene **menor precedencia** que `==` y `?:`.
   Escribir `flags & 0x01 ? "yes" : "no"` sin parentesis produce un parseo
   incorrecto. Siempre usar `(flags & mask)` en condiciones.
