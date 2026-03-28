# Lab -- #define

## Objetivo

Practicar las directivas `#define` y `#undef` del preprocesador de C: macros sin
parametros (object-like), macros con parametros (function-like), los operadores
de stringizing (`#`) y token pasting (`##`), macros multilinea, y herramientas
de linea de comandos (`gcc -E`, `-D`). Al finalizar, sabras cuando usar macros
y cuando preferir `const`, `enum` o `static inline`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `object_macros.c` | Macros sin parametros, trampa de precedencia de operadores |
| `function_macros.c` | Macros con parametros, regla de parentesis, doble evaluacion |
| `undef_demo.c` | #undef y redefinicion de macros por secciones |
| `stringify_paste.c` | Operadores # y ##, XSTRINGIFY, X-macros |
| `multiline_cmdline.c` | Macros multilinea con do-while(0), define desde -D |

---

## Parte 1 -- Object-like macros y gcc -E

**Objetivo**: Entender como el preprocesador sustituye macros sin parametros por
texto literal, ver la trampa clasica de precedencia de operadores, y usar
`gcc -E` para inspeccionar la expansion.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat object_macros.c
```

Observa:

- Se definen `PI`, `MAX_SIZE`, `VERSION` como macros sin parametros
- `BAD_EXPR` se define como `1 + 2` (sin parentesis)
- `GOOD_EXPR` se define como `(1 + 2)` (con parentesis)
- Ambas se multiplican por 3 para mostrar la diferencia

### Paso 1.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic object_macros.c -o object_macros
```

No debe producir warnings ni errores.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `BAD_EXPR * 3` se expande a `1 + 2 * 3`. Cual es el resultado? (recuerda la
  precedencia: `*` antes que `+`)
- `GOOD_EXPR * 3` se expande a `(1 + 2) * 3`. Y ahora?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar y verificar

```bash
./object_macros
```

Salida esperada:

```
PI         = 3.141593
Area (r=5) = 78.539816
MAX_SIZE   = 1024
VERSION    = 1.0.0

--- Parentheses trap ---
BAD_EXPR  * 3 = 7  (1 + 2 * 3 = 7)
GOOD_EXPR * 3 = 9  ((1 + 2) * 3 = 9)
```

Observa:

- `BAD_EXPR * 3` produjo 7 porque se expandio a `1 + 2 * 3` = `1 + 6` = `7`
- `GOOD_EXPR * 3` produjo 9 porque los parentesis forzaron `(1 + 2) * 3` = `9`
- La macro es sustitucion de texto puro -- no entiende tipos ni precedencia

### Paso 1.5 -- Ver la expansion con gcc -E

`gcc -E` ejecuta solo el preprocesador y muestra el resultado sin compilar:

```bash
gcc -std=c11 -E object_macros.c | tail -20
```

Salida esperada:

```
int main(void)
{
    double radius = 5.0;
    double area = 3.14159265358979 * radius * radius;
    printf("PI         = %f\n", 3.14159265358979);
    printf("Area (r=5) = %f\n", area);
    printf("MAX_SIZE   = %d\n", 1024);
    printf("VERSION    = %s\n", "1.0.0");

    printf("\n--- Parentheses trap ---\n");
    int bad = 1 + 2 * 3;
    int good = (1 + 2) * 3;
    printf("BAD_EXPR  * 3 = %d  (1 + 2 * 3 = 7)\n", bad);
    printf("GOOD_EXPR * 3 = %d  ((1 + 2) * 3 = 9)\n", good);

    return 0;
}
```

Observa:

- `PI` fue reemplazado por `3.14159265358979` en todas partes
- `BAD_EXPR * 3` se convirtio en `1 + 2 * 3` -- sin parentesis
- `GOOD_EXPR * 3` se convirtio en `(1 + 2) * 3` -- con parentesis
- Los nombres de las macros desaparecieron por completo; el compilador nunca los
  ve

### Paso 1.6 -- Guardar la salida preprocesada en un archivo

```bash
gcc -std=c11 -E object_macros.c -o object_macros.i
wc -l object_macros.i
```

Salida esperada:

```
~700 object_macros.i
```

El archivo `.i` contiene cientos de lineas porque incluye toda la expansion de
`<stdio.h>`. Puedes examinarlo con `less object_macros.i` y buscar `/int main`
para ver tu codigo.

### Limpieza de la parte 1

```bash
rm -f object_macros object_macros.i
```

---

## Parte 2 -- Function-like macros

**Objetivo**: Usar macros con parametros, entender la regla de parentesis para
evitar errores de precedencia, y ver por que los macros con efectos secundarios
son peligrosos.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat function_macros.c
```

Observa:

- `SQUARE(x)` esta definido como `((x) * (x))` -- parentesis en cada parametro
  y en la expresion completa
- `BAD_SQUARE(x)` esta definido como `x * x` -- sin parentesis
- Al final hay una demo de doble evaluacion con `next_val()` que incrementa un
  contador cada vez que se llama

### Paso 2.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic function_macros.c -o function_macros
```

No debe producir warnings ni errores.

### Paso 2.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `BAD_SQUARE(2 + 3)` se expande a `2 + 3 * 2 + 3`. Cuanto da?
- `SQUARE(2 + 3)` se expande a `((2 + 3) * (2 + 3))`. Cuanto da?
- Si `next_val()` retorna siempre 3 pero incrementa un contador, cuantas veces
  se llama en `SQUARE(next_val())`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Ejecutar y verificar

```bash
./function_macros
```

Salida esperada:

```
--- SQUARE ---
SQUARE(5)     = 25
SQUARE(2 + 3) = 25  (should be 25)

--- BAD_SQUARE (no parentheses) ---
BAD_SQUARE(5)     = 25
BAD_SQUARE(2 + 3) = 11  (should be 25, but is 11)

--- 100 / SQUARE(5) ---
100 / SQUARE(5)     = 4  (correct: 4)

--- MIN / MAX / ABS ---
MIN(10, 3)  = 3
MAX(10, 3)  = 10
ABS(-7)     = 7

--- ARRAY_SIZE ---
Array elements: 5

--- Side-effect danger ---
square_func(next_val()) = 9, next_val called 1 time
SQUARE(next_val())      = 9, next_val called 2 times!
Macro expanded to: ((next_val()) * (next_val()))
This is why inline functions are preferred over macros.
```

Observa:

- `BAD_SQUARE(2 + 3)` produjo 11 en vez de 25 -- la falta de parentesis cambio
  la precedencia
- `100 / SQUARE(5)` = 4 porque `SQUARE` envuelve todo en `(( ))`, lo que fuerza
  `100 / 25 = 4`
- `square_func(next_val())` llamo a `next_val` una vez (como toda funcion)
- `SQUARE(next_val())` llamo a `next_val` dos veces porque se expandio a
  `((next_val()) * (next_val()))` -- el argumento se evalua en cada aparicion

### Paso 2.5 -- Ver la expansion con gcc -E

```bash
gcc -std=c11 -E function_macros.c | grep -A2 "Side-effect"
```

Esto muestra como el compilador ve las llamadas a `SQUARE(next_val())` despues
de la expansion. La doble evaluacion queda explicita en el codigo preprocesado.

### Limpieza de la parte 2

```bash
rm -f function_macros
```

---

## Parte 3 -- #undef

**Objetivo**: Usar `#undef` para eliminar la definicion de un macro, redefinir
macros sin producir warnings, y verificar con `#ifdef` si un macro esta definido.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat undef_demo.c
```

Observa:

- `MODE` y `LIMIT` se definen al inicio
- En la seccion 2, se hace `#undef` seguido de `#define` para redefinir
- En la seccion 3, se hace `#undef LIMIT` sin redefinir -- `LIMIT` deja de
  existir
- Se usa `#ifdef` para verificar si `LIMIT` esta definido

### Paso 3.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic undef_demo.c -o undef_demo
```

### Paso 3.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- En la seccion 2, que valor tendra `MODE`? Y `LIMIT`?
- En la seccion 3, `LIMIT` fue eliminado con `#undef`. El `#ifdef LIMIT` sera
  verdadero o falso?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar y verificar

```bash
./undef_demo
```

Salida esperada:

```
=== Section 1 ===
MODE  = fast
LIMIT = 100

=== Section 2 (after #undef + redefine) ===
MODE  = safe
LIMIT = 200

=== Section 3 (after #undef LIMIT) ===
MODE  = safe
LIMIT is not defined in this section
#ifdef LIMIT -> false (LIMIT was #undef'd)
```

Observa:

- Despues de `#undef MODE` + `#define MODE "safe"`, el macro tiene el nuevo
  valor sin warnings del compilador
- Despues de `#undef LIMIT` (sin redefinir), `LIMIT` ya no existe. Intentar
  usarlo como si fuera una variable causaria un error de compilacion
- `#ifdef LIMIT` es falso porque el macro fue eliminado

### Paso 3.5 -- Verificar: redefinir sin #undef produce warning

Abre el archivo y observa que siempre se usa `#undef` antes de `#define` para
redefinir. Si redefinieramos sin `#undef`, el compilador produciria un warning
como:

```
warning: "LIMIT" redefined
```

Esta es la razon por la que el patron estandar es `#undef` + `#define`.

### Limpieza de la parte 3

```bash
rm -f undef_demo
```

---

## Parte 4 -- Stringizing (#) y token pasting (##)

**Objetivo**: Usar el operador `#` para convertir argumentos de macros a strings,
el operador `##` para concatenar tokens, y entender el patron XSTRINGIFY para
expandir macros antes de stringizar.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat stringify_paste.c
```

Observa:

- `STRINGIFY(x)` usa `#x` para convertir el argumento a string literal
- `XSTRINGIFY(x)` llama a `STRINGIFY(x)` -- el doble nivel permite que `x` se
  expanda primero
- `CONCAT(a, b)` usa `a##b` para pegar dos tokens en uno
- `PRINT_INT(var)` usa `#var` para imprimir el nombre de la variable junto con
  su valor
- La seccion de X-macros usa `#` y `##` juntos para generar un enum y un array
  de strings desde una unica lista

### Paso 4.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic stringify_paste.c -o stringify_paste
```

### Paso 4.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `STRINGIFY(APP_VERSION)` -- se expande `APP_VERSION` a 42, o se convierte el
  nombre `APP_VERSION` a string?
- `XSTRINGIFY(APP_VERSION)` -- pasa por dos niveles. Primero se expande
  `APP_VERSION` a 42, luego se aplica `#`. Que resultado da?
- `CONCAT(my, Var)` -- que nombre de variable produce?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Ejecutar y verificar

```bash
./stringify_paste
```

Salida esperada:

```
=== Stringizing (#) ===
STRINGIFY(hello)       = hello
STRINGIFY(1 + 2)       = 1 + 2
STRINGIFY(foo bar)     = foo bar

=== STRINGIFY vs XSTRINGIFY ===
STRINGIFY(APP_VERSION)  = APP_VERSION  (not expanded)
XSTRINGIFY(APP_VERSION) = 42  (expanded first)

=== Token pasting (##) ===
myVar = 100
var_1 = 10
var_2 = 20

=== PRINT_INT (# in practice) ===
count = 7
total = 42
count + total = 49

=== X-macros: enum + string table ===
  color_names[0] = RED
  color_names[1] = GREEN
  color_names[2] = BLUE
  color_names[3] = YELLOW
```

Observa:

- `STRINGIFY(APP_VERSION)` produjo la string `"APP_VERSION"` -- el operador `#`
  actua ANTES de que el preprocesador expanda el argumento
- `XSTRINGIFY(APP_VERSION)` produjo `"42"` -- el argumento se expande a 42 en
  el primer nivel, y luego `#` convierte `42` a string en el segundo nivel
- `CONCAT(my, Var)` pego los tokens `my` y `Var` formando el identificador
  `myVar`
- `PRINT_INT(count)` imprimio `count = 7` -- `#var` produjo la string `"count"`
- Las X-macros generaron automaticamente el enum y el array de strings desde una
  unica definicion de `COLORS`

### Paso 4.5 -- Ver la expansion de X-macros con gcc -E

```bash
gcc -std=c11 -E stringify_paste.c | grep -A3 "enum Color"
```

Salida esperada:

```
enum Color { RED, GREEN, BLUE, YELLOW, COLOR_COUNT };
static const char *color_names[] = { "RED", "GREEN", "BLUE", "YELLOW", };
```

Observa como una unica lista `COLORS(X)` genero tanto el enum como el array de
strings. Si agregas un color nuevo a la lista, ambos se actualizan
automaticamente -- no hay riesgo de que queden desincronizados.

### Limpieza de la parte 4

```bash
rm -f stringify_paste
```

---

## Parte 5 -- Macros multilinea y -D desde linea de comandos

**Objetivo**: Escribir macros que ocupan varias lineas con `\`, usar el patron
`do { ... } while (0)` para macros con multiples sentencias, y definir macros
desde la linea de comandos con `gcc -D`.

### Paso 5.1 -- Examinar el codigo fuente

```bash
cat multiline_cmdline.c
```

Observa:

- `SWAP(a, b)` usa `do { ... } while (0)` para agrupar multiples sentencias.
  Cada linea termina con `\` (el ultimo caracter antes del salto de linea)
- `LOG(msg)` imprime el archivo y numero de linea usando `__FILE__` y `__LINE__`
- `BUILD_MODE` y `MAX_ITEMS` usan `#ifndef` para que se puedan definir desde la
  linea de comandos con `-D`, o tomar valores por defecto

### Paso 5.2 -- Compilar sin flags -D (valores por defecto)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic multiline_cmdline.c -o multiline_cmdline
```

### Paso 5.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- El macro `SWAP` intercambia dos valores usando una variable temporal. Funciona
  con `int` y con `double`?
- `LOG("program started")` imprime `__FILE__` y `__LINE__`. Que archivo y que
  numero de linea apareceran?
- Sin flags `-D`, que valores tendran `BUILD_MODE` y `MAX_ITEMS`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.4 -- Ejecutar con valores por defecto

```bash
./multiline_cmdline
```

Salida esperada:

```
=== Multi-line macro: SWAP ===
Before: x=10, y=20
After:  x=20, y=10

Before: p=1.5, q=9.9
After:  p=9.9, q=1.5

=== Multi-line macro: LOG ===
[multiline_cmdline.c:38] program started
[multiline_cmdline.c:39] about to finish

=== -D command-line defines ===
BUILD_MODE = default
MAX_ITEMS  = 10
```

Observa:

- `SWAP` funciono tanto con `int` como con `double` gracias a `__typeof__`
- `LOG` mostro el nombre del archivo fuente y el numero de linea exacto de la
  invocacion
- Sin `-D`, `BUILD_MODE` tomo el valor `"default"` y `MAX_ITEMS` el valor `10`
  (los definidos con `#ifndef`)

### Paso 5.5 -- Compilar con -D para cambiar los valores

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
    -DBUILD_MODE='"release"' \
    -DMAX_ITEMS=50 \
    multiline_cmdline.c -o multiline_cmdline
```

Nota la sintaxis `-DBUILD_MODE='"release"'`: las comillas simples externas son
para el shell, las comillas dobles internas son parte del valor string en C.
Para un entero como `-DMAX_ITEMS=50`, no se necesitan comillas.

### Paso 5.6 -- Ejecutar con valores personalizados

```bash
./multiline_cmdline
```

Salida esperada:

```
=== Multi-line macro: SWAP ===
Before: x=10, y=20
After:  x=20, y=10

Before: p=1.5, q=9.9
After:  p=9.9, q=1.5

=== Multi-line macro: LOG ===
[multiline_cmdline.c:38] program started
[multiline_cmdline.c:39] about to finish

=== -D command-line defines ===
BUILD_MODE = release
MAX_ITEMS  = 50
```

Observa:

- `BUILD_MODE` ahora muestra `release` en vez de `default`
- `MAX_ITEMS` ahora es 50 en vez de 10
- El flag `-D` definio los macros ANTES de que el preprocesador viera los
  `#ifndef`, por lo que los valores por defecto no se aplicaron

### Paso 5.7 -- Ver la expansion del macro SWAP con gcc -E

```bash
gcc -std=c11 -E multiline_cmdline.c | grep -A5 "Before: x"
```

Salida esperada (aproximada):

```
    printf("Before: x=%d, y=%d\n", x, y);
    do { __typeof__(x) _tmp = (x); (x) = (y); (y) = _tmp; } while (0);
    printf("After:  x=%d, y=%d\n", x, y);
```

Observa como el macro multilinea se expandio a una sola linea con
`do { ... } while (0)`. El patron `do-while(0)` permite que el macro se use
como una sentencia normal terminada en `;` y funcione correctamente dentro de
bloques `if/else`.

### Limpieza de la parte 5

```bash
rm -f multiline_cmdline
```

---

## Limpieza final

```bash
rm -f object_macros function_macros undef_demo stringify_paste multiline_cmdline
rm -f object_macros.i
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  function_macros.c  multiline_cmdline.c  object_macros.c  stringify_paste.c  undef_demo.c
```

---

## Conceptos reforzados

1. Las macros object-like son sustitucion de texto puro. Si la expresion no esta
   envuelta en parentesis, la precedencia de operadores puede producir resultados
   incorrectos (`1 + 2 * 3` = 7, no 9).

2. En macros function-like, cada parametro debe estar entre parentesis Y la
   expresion completa entre parentesis. `((x) * (x))` es correcto; `x * x` no.

3. Los macros evaluan sus argumentos una vez por cada aparicion en la expansion.
   Si el argumento tiene efectos secundarios (como llamar a una funcion), se
   ejecuta multiples veces. Las funciones `static inline` son la alternativa
   segura.

4. `gcc -E` muestra el codigo despues de la expansion del preprocesador. Es la
   herramienta principal para depurar problemas con macros.

5. `#undef` elimina la definicion de un macro. Siempre usar `#undef` antes de
   `#define` para redefinir un macro sin producir warnings del compilador.

6. El operador `#` (stringizing) convierte un argumento de macro a string
   literal. Actua ANTES de la expansion del argumento. Para expandir primero,
   usar el patron de doble nivel: `XSTRINGIFY(x)` llama a `STRINGIFY(x)`.

7. El operador `##` (token pasting) concatena dos tokens en uno. Permite generar
   nombres de variables, funciones o tipos dinamicamente. Las X-macros combinan
   `#` y `##` para mantener enums y arrays de strings sincronizados
   automaticamente.

8. Los macros multilinea usan `\` al final de cada linea. El patron
   `do { ... } while (0)` permite que un macro con multiples sentencias funcione
   correctamente como una sola sentencia (especialmente dentro de `if/else`).

9. El flag `-D` de gcc define macros desde la linea de comandos. Es equivalente
   a poner `#define` al inicio del archivo. Combinado con `#ifndef`, permite
   valores configurables con defaults.
