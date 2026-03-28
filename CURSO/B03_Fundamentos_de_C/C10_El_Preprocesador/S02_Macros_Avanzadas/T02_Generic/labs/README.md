# Lab -- _Generic

## Objetivo

Usar `_Generic` de C11 para crear macros que seleccionan expresiones en tiempo
de compilacion segun el tipo del argumento. Al finalizar, sabras escribir macros
genericas type-safe para imprimir valores, obtener nombres de tipo, calcular
valor absoluto y maximo, manejar la clausula `default`, diagnosticar errores de
tipo no soportado, y combinar `_Generic` con variadic macros.

## Prerequisitos

- GCC con soporte C11 instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `print_val.c` | Macro `print_val` basica con `_Generic` para 4 tipos |
| `type_name.c` | Macros `type_name` y `PRINT_VAR` con stringify |
| `abs_max.c` | `abs_val` y `max` genericos con despacho a funciones |
| `default_error.c` | Clausula `default` y tipos no soportados |
| `no_default_error.c` | Ejemplo que falla sin `default` (no compila) |
| `variadic_generic.c` | Combinacion de `_Generic` con variadic macros |

---

## Parte 1 -- Sintaxis basica de _Generic

**Objetivo**: Entender la sintaxis de `_Generic`, como selecciona una expresion
segun el tipo de la expresion de control, y como el compilador determina los
tipos de literales.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat print_val.c
```

Observa:

- La macro `print_val(x)` usa `_Generic` con cuatro asociaciones: `int`,
  `double`, `char`, `char*`
- Cada asociacion selecciona un `printf` con el format specifier correcto
- La expresion de control `(x)` no se evalua -- solo se usa su tipo
- Se prueban variables y tambien literales directos

### Paso 1.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic print_val.c -o print_val
```

No debe producir warnings ni errores.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Un literal como `42` sin sufijo, que tipo tiene en C?
- Un literal como `2.71` sin sufijo, es `float` o `double`?
- `_Generic` selecciona en tiempo de compilacion o de ejecucion?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar y verificar

```bash
./print_val
```

Salida esperada:

```
int:    30
double: 3.141590
char:   'A'
char*:  Alice
literal 42: 42
literal 2.71: 2.710000
```

Observa:

- `42` se trata como `int` (los literales enteros sin sufijo son `int`)
- `2.71` se trata como `double` (los literales flotantes sin sufijo son `double`,
  no `float` -- necesitarias `2.71f` para `float`)
- La seleccion ocurre en tiempo de compilacion: no hay overhead en runtime
- Cada branch de `_Generic` produce el `printf` con el formato adecuado

### Limpieza de la parte 1

```bash
rm -f print_val
```

---

## Parte 2 -- type_name y PRINT_VAR

**Objetivo**: Usar `_Generic` para obtener el nombre del tipo como string, y
combinar el operador de stringify (`#`) con `_Generic` para crear una macro
`PRINT_VAR` que imprima "nombre = valor".

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat type_name.c
```

Observa:

- `type_name(x)` tiene asociaciones para 18 tipos diferentes y un `default`
- `PRINT_VAR(var)` usa `#var` para convertir el nombre de la variable a string
  y `_Generic` para seleccionar el format specifier
- El operador `#` (stringify) es del preprocesador, no de `_Generic` -- se
  evalua antes de que `_Generic` haga su seleccion

### Paso 2.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic type_name.c -o type_name
```

### Paso 2.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Un literal de caracter como `'A'`, que tipo tiene en C? (pista: en C es
  diferente a C++)
- `"hello"` es un `char[6]` en la declaracion, pero que tipo ve `_Generic`?
  (pista: arrays decaen a punteros)
- `PRINT_VAR(count)` con `count = 42`, que string producira `#count`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Ejecutar y verificar

```bash
./type_name
```

Salida esperada:

```
--- type_name ---
42        -> int
3.14f     -> float
3.14      -> double
'A'       -> int
"hello"   -> char*
(bool)1   -> bool
42UL      -> unsigned long
(void*)0  -> void*

--- PRINT_VAR ---
count = 42
pi = 3.140000
greeting = "hello"
letter = 'Z'
flags = 255
```

Observa:

- `'A'` es de tipo `int` en C (no `char` -- en C++ si seria `char`)
- `"hello"` decae de `char[6]` a `char*` (array decay)
- `#var` stringify funciona correctamente: `PRINT_VAR(count)` produce la cadena
  `"count"` que aparece antes del `=` en la salida
- `flags = 255` porque `0xFF` es 255 en decimal, y el format specifier `%u`
  muestra el valor decimal

### Limpieza de la parte 2

```bash
rm -f type_name
```

---

## Parte 3 -- abs y max genericos

**Objetivo**: Usar `_Generic` para despachar a funciones diferentes segun el
tipo del argumento, creando versiones genericas de `abs` y `max` que son
type-safe.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat abs_max.c
```

Observa:

- `abs_val(x)` despacha a `abs`, `labs`, `llabs`, `fabsf` o `fabs` segun el
  tipo. La seleccion `_Generic` retorna la *funcion*, y luego `(x)` la invoca
- `max` requiere funciones auxiliares `max_int`, `max_dbl`, `max_lng` porque C
  no tiene overloading
- Las funciones auxiliares son `static inline` para evitar overhead de llamada
- El macro `max(a, b)` despacha segun el tipo del *primer* argumento solamente

### Paso 3.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic abs_max.c -o abs_max -lm
```

Nota: se necesita `-lm` para enlazar la biblioteca matematica (`fabsf`, `fabs`).

### Paso 3.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `abs_val(-3.14)` llamara a `abs()` (que trunca a int) o a `fabs()` (que
  retorna double)?
- `max(10, 25)` llamara a `max_int`. Que pasaria si pasaras `max(10, 25.0)`?
  (el segundo argumento es double pero el despacho se hace por el primero)

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar y verificar

```bash
./abs_max
```

Salida esperada:

```
--- abs_val ---
abs_val(-42)    = 42
abs_val(-100000L)  = 100000
abs_val(-3.140000) = 3.140000
abs_val(-2.710000f)  = 2.710000

--- max ---
max(10, 25) = 25
max(1.500000, 2.700000) = 2.700000
max(100, -50) = 100
```

Observa:

- `abs_val(-3.14)` llamo a `fabs` (no a `abs`), preservando el valor decimal.
  Si hubiera llamado a `abs()`, retornaria `3` (truncado a int)
- `abs_val(-2.71f)` llamo a `fabsf` (la version para `float`)
- El patron `_Generic((x), tipo: funcion)(x)` es el idioma estandar:
  `_Generic` selecciona la funcion, y luego `(x)` la invoca
- `max` solo despacha por el primer argumento. Si los tipos de `a` y `b` no
  coinciden, el compilador aplica conversion implicita en la funcion (o da
  warning)

### Limpieza de la parte 3

```bash
rm -f abs_max
```

---

## Parte 4 -- default y errores de tipo

**Objetivo**: Entender la clausula `default` como catch-all para tipos no
listados, y observar el error de compilacion cuando no hay `default` y se
pasa un tipo no soportado.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat default_error.c
```

Observa:

- `describe_type(x)` tiene tres tipos explicitos (`int`, `double`, `char*`) y
  un `default`
- `format_val(x)` tambien tiene `default` con un mensaje generico
- Se prueban tipos listados y tipos que caen en `default` (`float`, `long`,
  `struct Point`)

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic default_error.c -o default_error
./default_error
```

Salida esperada:

```
--- describe_type ---
42      -> integer
3.14    -> floating-point
"hi"    -> string
3.14f   -> unsupported type
42L     -> unsupported type
'A'     -> integer
pt      -> unsupported type

--- format_val ---
int: 99
double: 2.718000
string: world
(type not supported)
(type not supported)
```

Observa:

- `float`, `long` y `struct Point` caen en `default` porque no estan listados
- `'A'` es `int` en C, asi que coincide con la asociacion `int` (no con
  `default`)
- `default` actua como un catch-all: el programa compila y ejecuta, pero el
  comportamiento para tipos no listados es generico

### Paso 4.3 -- Predecir el error sin default

Ahora examina `no_default_error.c`:

```bash
cat no_default_error.c
```

Este programa tiene un `_Generic` con solo `int` y `double`, sin `default`.
Se intenta usar con un `float`.

Antes de compilar, responde mentalmente:

- Que pasara al compilar? Sera un error o un warning?
- El error sera en tiempo de compilacion o de ejecucion?
- El mensaje mencionara `_Generic` o `float`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Verificar el error de compilacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic no_default_error.c -o no_default_error 2>&1
```

Salida esperada (el error, no compila):

```
no_default_error.c: In function 'main':
no_default_error.c:4:31: error: '_Generic' selector of type 'float' is not compatible with any association
    4 | #define print_num(x) _Generic((x), \
      |                               ^
no_default_error.c:17:5: note: in expansion of macro 'print_num'
   17 |     print_num(f);
      |     ^~~~~~~~~
```

Observa:

- Es un **error de compilacion**, no un warning -- el programa no se puede
  compilar
- El mensaje dice exactamente que tipo causo el problema (`float`) y que no es
  compatible con ninguna asociacion
- Esto es una ventaja de `_Generic` sin `default`: detecta tipos inesperados
  en tiempo de compilacion, no en runtime
- La decision entre usar `default` o no depende del caso: `default` da
  flexibilidad, omitirlo da seguridad de tipo mas estricta

### Limpieza de la parte 4

```bash
rm -f default_error
```

---

## Parte 5 -- Combinacion con variadic macros

**Objetivo**: Combinar `_Generic` con variadic macros (`__VA_ARGS__`) para crear
un macro `PRINT_VALS` que imprime multiples valores de tipos diferentes en una
sola llamada.

### Paso 5.1 -- Examinar el codigo fuente

```bash
cat variadic_generic.c
```

Observa:

- `print_one(x)` usa `_Generic` para imprimir un solo valor con el formato
  correcto
- `NARGS(...)` cuenta cuantos argumentos recibe (2 o 3) usando el truco de
  desplazamiento de argumentos
- `DISPATCH(name, n)` concatena el nombre del macro con el numero de argumentos
  para seleccionar `PRINT_VALS_2` o `PRINT_VALS_3`
- Cada version `PRINT_VALS_N` llama a `print_one` para cada argumento,
  separandolos con comas

### Paso 5.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic variadic_generic.c -o variadic_generic
```

No debe producir warnings ni errores.

### Paso 5.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `PRINT_VALS(42, 3.14, "world")` tiene 3 argumentos. Que macro se expandira:
  `PRINT_VALS_2` o `PRINT_VALS_3`?
- `100L` se despachara al branch `int` o al branch `long` de `print_one`?
- El formato de `2.5f` (float) y de `3.14` (double) se vera diferente en la
  salida?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.4 -- Ejecutar y verificar

```bash
./variadic_generic
```

Salida esperada:

```
--- PRINT_VALS con 2 argumentos ---
42, "hello"
3.140000, 100
"name", 100

--- PRINT_VALS con 3 argumentos ---
42, 3.140000, "world"
100, 2.500000, 99

--- Con variables ---
25, "Madrid"
25, "Madrid", 21.500000
```

Observa:

- El conteo de argumentos funciona: 2 argumentos usa `PRINT_VALS_2`, 3 usa
  `PRINT_VALS_3`
- `100L` se imprime como `100` (branch `long`, formato `%ld`)
- `2.5f` (float) y `3.14` (double) ambos producen formato `%f`, pero pasan
  por branches diferentes de `_Generic`
- Cada argumento se formatea independientemente segun su tipo -- un solo
  `PRINT_VALS` puede mezclar `int`, `char*` y `double` en la misma llamada
- La combinacion de variadic macros + `_Generic` permite crear interfaces
  genericas que aceptan multiples argumentos de tipos diferentes

### Limpieza de la parte 5

```bash
rm -f variadic_generic
```

---

## Limpieza final

```bash
rm -f print_val type_name abs_max default_error variadic_generic
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  abs_max.c  default_error.c  no_default_error.c  print_val.c  type_name.c  variadic_generic.c
```

---

## Conceptos reforzados

1. `_Generic` selecciona una expresion en **tiempo de compilacion** segun el
   tipo de la expresion de control. No hay overhead en runtime -- el compilador
   descarta todos los branches excepto el seleccionado.

2. La expresion de control de `_Generic` **no se evalua** -- solo se usa su
   tipo. Arrays decaen a punteros y literales de caracter (`'A'`) son `int` en
   C, no `char`.

3. Los literales flotantes sin sufijo (`3.14`) son `double`, no `float`. Para
   obtener `float` se necesita el sufijo `f` (`3.14f`). Este detalle importa
   al escribir las asociaciones de `_Generic`.

4. El patron `_Generic((x), tipo: funcion)(x)` es el idioma estandar para
   despacho de funciones por tipo: `_Generic` selecciona la funcion y los
   parentesis finales la invocan con el argumento.

5. La clausula `default` actua como catch-all para tipos no listados. **Sin**
   `default`, pasar un tipo no soportado produce un error de compilacion -- esto
   puede ser una ventaja cuando se quiere seguridad de tipo estricta.

6. El operador `#` (stringify) del preprocesador se evalua **antes** que
   `_Generic`. Combinar `#var` con `_Generic` permite crear macros como
   `PRINT_VAR` que imprimen el nombre y valor de cualquier variable con el
   formato correcto.

7. `_Generic` se puede combinar con variadic macros (`__VA_ARGS__`) y el truco
   de conteo de argumentos para crear interfaces genericas que aceptan multiples
   argumentos de tipos diferentes en una sola llamada.

8. `_Generic` solo despacha por **un** argumento. Para despachar por dos
   argumentos (ej. `max(a, b)` donde `a` y `b` pueden tener tipos diferentes)
   se necesitan `_Generic` anidados, lo cual se vuelve verboso rapidamente.
