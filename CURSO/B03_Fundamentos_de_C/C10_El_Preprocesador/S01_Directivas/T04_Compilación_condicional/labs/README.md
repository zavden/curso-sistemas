# Lab -- Compilacion condicional

## Objetivo

Usar las directivas de compilacion condicional del preprocesador (`#ifdef`,
`#ifndef`, `#if`, `#elif`, `#else`, `#endif`, `defined()`) para incluir o
excluir codigo en tiempo de compilacion. Al finalizar, sabras controlar builds
con `gcc -D`, distinguir entre existencia y valor de macros, usar feature macros
como `_GNU_SOURCE`, desactivar `assert()` con `NDEBUG`, detener la compilacion
con `#error`, y usar `gcc -E` para inspeccionar que codigo pasa el
preprocesador.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `ifdef_basics.c` | #ifdef, #ifndef, defined(), existencia vs valor, gcc -D |
| `conditional_chain.c` | #if/#elif/#else cadenas, defined() combinado, macros no definidos |
| `feature_macros.c` | _POSIX_C_SOURCE, _GNU_SOURCE, deteccion de plataforma |
| `ndebug_assert.c` | NDEBUG, assert(), side effects en asserts |
| `error_warning.c` | #error para requerir macros, #warning para deprecaciones |
| `preprocess_view.c` | Archivo pequeno para examinar salida de gcc -E |

---

## Parte 1 -- #ifdef, #ifndef, defined() y gcc -D

**Objetivo**: Entender la diferencia entre verificar la existencia de un macro
(`#ifdef`) y verificar su valor (`#if`), y aprender a definir macros desde la
linea de compilacion con `gcc -D`.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat ifdef_basics.c
```

Observa:

- `#ifdef DEBUG` verifica si `DEBUG` existe (sin importar su valor)
- `#ifndef RELEASE` verifica que `RELEASE` NO exista
- `#if defined(VERBOSE)` es equivalente a `#ifdef VERBOSE`
- Se definen tres macros internas: `FEATURE_A` (vacio), `FEATURE_B` (valor 0),
  `FEATURE_C` (valor 1) para demostrar existencia vs valor
- `LEVEL` se espera desde la linea de compilacion con `-DLEVEL=N`

### Paso 1.2 -- Compilar sin flags

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ifdef_basics.c -o ifdef_basics
```

No debe producir warnings ni errores.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `DEBUG`, `RELEASE` y `VERBOSE` no fueron definidos. Que ramas se ejecutaran?
- `FEATURE_B` tiene valor 0. `#ifdef FEATURE_B` sera TRUE o FALSE?
- Y `#if FEATURE_B`?
- `LEVEL` no fue definido. Que se imprimira en la seccion de LEVEL?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar sin flags

```bash
./ifdef_basics
```

Salida esperada:

```
=== #ifdef / #ifndef basics ===

[DEBUG not defined] Debug mode is OFF
[RELEASE not defined] Not a release build
[VERBOSE not defined] Verbose output disabled

=== Existence (#ifdef) vs Value (#if) ===

FEATURE_A: #ifdef -> TRUE  (exists, empty value)
FEATURE_B: #ifdef -> TRUE  (exists, value 0)
FEATURE_C: #ifdef -> TRUE  (exists, value 1)
FEATURE_B: #if    -> FALSE (value is 0)
FEATURE_C: #if    -> TRUE  (value is 1)

=== Macro value from -D ===

LEVEL is not defined (not passed with -DLEVEL=N)
```

Observa el punto clave: `FEATURE_B` tiene valor 0. Para `#ifdef` es TRUE
(existe), pero para `#if` es FALSE (vale 0). Esta es la diferencia fundamental
entre existencia y valor.

### Paso 1.5 -- Compilar con flags desde la linea de comandos

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
    -DDEBUG -DRELEASE -DVERBOSE -DLEVEL=5 \
    ifdef_basics.c -o ifdef_basics
```

`-DDEBUG` equivale a poner `#define DEBUG 1` al inicio del archivo.
`-DLEVEL=5` equivale a `#define LEVEL 5`.

### Paso 1.6 -- Predecir el resultado con flags

Ahora `DEBUG`, `RELEASE`, `VERBOSE` y `LEVEL` estan definidos.

- Que ramas se ejecutaran ahora para DEBUG, RELEASE, VERBOSE?
- LEVEL vale 5. Que rama del `#if LEVEL >= 3` / `#elif LEVEL >= 1` se tomara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.7 -- Ejecutar con flags

```bash
./ifdef_basics
```

Salida esperada:

```
=== #ifdef / #ifndef basics ===

[DEBUG is defined] Debug mode is ON
[RELEASE is defined] This is a release build
[VERBOSE is defined] Verbose output enabled

=== Existence (#ifdef) vs Value (#if) ===

FEATURE_A: #ifdef -> TRUE  (exists, empty value)
FEATURE_B: #ifdef -> TRUE  (exists, value 0)
FEATURE_C: #ifdef -> TRUE  (exists, value 1)
FEATURE_B: #if    -> FALSE (value is 0)
FEATURE_C: #if    -> TRUE  (value is 1)

=== Macro value from -D ===

LEVEL is defined, value = 5
  -> High level (>= 3)
```

La seccion de `FEATURE_A/B/C` no cambio porque esos macros estan definidos
dentro del codigo fuente, no desde la linea de compilacion.

### Paso 1.8 -- Probar con LEVEL=1

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DLEVEL=1 \
    ifdef_basics.c -o ifdef_basics
./ifdef_basics
```

Salida esperada (solo la seccion relevante):

```
=== Macro value from -D ===

LEVEL is defined, value = 1
  -> Low level (1-2)
```

`LEVEL=1` no cumple `>= 3`, pero si cumple `>= 1`, asi que entra en el
`#elif`.

### Limpieza de la parte 1

```bash
rm -f ifdef_basics
```

---

## Parte 2 -- #if, #elif, #else y cadenas condicionales

**Objetivo**: Usar cadenas `#if`/`#elif`/`#else` para seleccion multiple,
combinar `defined()` con operadores logicos, y entender que macros no definidos
valen 0 silenciosamente en `#if`.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat conditional_chain.c
```

Observa:

- `LOG_LEVEL` tiene un default de 0 si no se define desde la linea de comandos
- Se usa `defined(OPT_A) && defined(OPT_B)` para combinar condiciones
  (esto NO se puede hacer con `#ifdef`, que solo acepta un macro)
- `BUFSIZE` tiene default de 256 y se evalua con operadores de comparacion
- `MYSTERY_MACRO` nunca se define -- demuestra el peligro de macros no definidos
  en `#if`

### Paso 2.2 -- Compilar y ejecutar sin flags

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic conditional_chain.c -o conditional_chain
./conditional_chain
```

Salida esperada:

```
=== #if / #elif / #else chains ===

LOG_LEVEL = 0
  -> Logging: OFF

=== Combining defined() ===

Neither OPT_A nor OPT_B is defined

=== Arithmetic in #if ===

BUFSIZE = 256
  -> Buffer size is reasonable

=== Undefined macros in #if ===

MYSTERY_MACRO is false (0 or undefined)
  -> Was it intentionally 0, or just not defined? No way to tell.
MYSTERY_MACRO is not defined at all
```

Observa el problema con `MYSTERY_MACRO`: en `#if MYSTERY_MACRO`, un macro no
definido silenciosamente vale 0. No hay error ni warning. Es imposible
distinguir "definido con valor 0" de "no definido" usando solo `#if`. La
solucion es combinar: `#if defined(X) && X`.

### Paso 2.3 -- Detectar el peligro con -Wundef

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wundef \
    conditional_chain.c -o conditional_chain 2>&1
```

Salida esperada:

```
conditional_chain.c:56:5: warning: "MYSTERY_MACRO" is not defined, evaluates to 0 [-Wundef]
   56 | #if MYSTERY_MACRO
      |     ^~~~~~~~~~~~~
```

`-Wundef` avisa cuando un macro no definido se usa en `#if`. Es una buena
practica habilitarlo en proyectos reales para evitar bugs silenciosos.

### Paso 2.4 -- Compilar con multiples flags

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
    -DLOG_LEVEL=2 -DOPT_A -DOPT_B -DBUFSIZE=8192 \
    conditional_chain.c -o conditional_chain
./conditional_chain
```

Salida esperada:

```
=== #if / #elif / #else chains ===

LOG_LEVEL = 2
  -> Logging: ERROR + WARN

=== Combining defined() ===

Both OPT_A and OPT_B are defined

=== Arithmetic in #if ===

BUFSIZE = 8192
  -> Buffer is very large (> 4096)

=== Undefined macros in #if ===

MYSTERY_MACRO is false (0 or undefined)
  -> Was it intentionally 0, or just not defined? No way to tell.
MYSTERY_MACRO is not defined at all
```

### Paso 2.5 -- Probar con solo OPT_A

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
    -DLOG_LEVEL=3 -DOPT_A \
    conditional_chain.c -o conditional_chain
./conditional_chain
```

Salida esperada (secciones relevantes):

```
LOG_LEVEL = 3
  -> Logging: ERROR + WARN + INFO

=== Combining defined() ===

Only OPT_A is defined
```

`defined(OPT_A) && defined(OPT_B)` es FALSE porque `OPT_B` no fue definido.
El `#elif defined(OPT_A)` captura este caso.

### Limpieza de la parte 2

```bash
rm -f conditional_chain
```

---

## Parte 3 -- Feature macros

**Objetivo**: Entender como `_POSIX_C_SOURCE` y `_GNU_SOURCE` desbloquean
funciones adicionales, y como detectar plataforma, compilador y estandar de C
con macros predefinidos.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat feature_macros.c
```

Observa:

- `_GNU_SOURCE` y `_POSIX_C_SOURCE` se definen ANTES de cualquier `#include`
  (requisito obligatorio -- si se definen despues, no tienen efecto)
- El programa usa `USE_GNU` y `USE_POSIX` como interruptores desde la linea
  de compilacion
- Se detecta la plataforma con `__linux__`, el compilador con `__GNUC__`, y
  el estandar con `__STDC_VERSION__`
- Con `_GNU_SOURCE`, se usa `strcasestr()` (busqueda case-insensitive)
- Con `_POSIX_C_SOURCE >= 200809L`, se usa `strnlen()`

### Paso 3.2 -- Compilar y ejecutar sin feature macros

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic feature_macros.c -o feature_macros
./feature_macros
```

Salida esperada:

```
=== Feature test macros ===

Macros currently defined:
  _GNU_SOURCE       = (not defined)
  _POSIX_C_SOURCE   = (not defined)

=== Platform / compiler info ===

  Platform: Linux
  Compiler: GCC <major>.<minor>.<patch>
  C standard: C11 (__STDC_VERSION__ = 201112L)

=== Feature-gated functions ===

  strcasestr() NOT available
  -> Compile with -DUSE_GNU to enable _GNU_SOURCE
  strnlen() NOT available (needs _POSIX_C_SOURCE >= 200809L)
```

Sin feature macros, `strcasestr()` y `strnlen()` no estan disponibles. El
codigo que las usa esta excluido por compilacion condicional, asi que no hay
error.

### Paso 3.3 -- Predecir el efecto de -DUSE_POSIX

Antes de compilar, responde mentalmente:

- `_POSIX_C_SOURCE` se definira a `200809L`. Que funcion se desbloqueara?
- `strcasestr()` es una extension GNU, no POSIX. Estara disponible?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Compilar con -DUSE_POSIX

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DUSE_POSIX \
    feature_macros.c -o feature_macros
./feature_macros
```

Salida esperada:

```
=== Feature test macros ===

Macros currently defined:
  _GNU_SOURCE       = (not defined)
  _POSIX_C_SOURCE   = 200809L

=== Platform / compiler info ===

  Platform: Linux
  Compiler: GCC <major>.<minor>.<patch>
  C standard: C11 (__STDC_VERSION__ = 201112L)

=== Feature-gated functions ===

  strcasestr() NOT available
  -> Compile with -DUSE_GNU to enable _GNU_SOURCE
  strnlen("Hello, World!", 100) = 13
  -> strnlen() available (POSIX.1-2008)
```

`strnlen()` ahora esta disponible. `strcasestr()` sigue sin estarlo porque
es una extension GNU, no parte de POSIX.

### Paso 3.5 -- Compilar con -DUSE_GNU

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DUSE_GNU \
    feature_macros.c -o feature_macros
./feature_macros
```

Salida esperada:

```
=== Feature test macros ===

Macros currently defined:
  _GNU_SOURCE       = (defined)
  _POSIX_C_SOURCE   = 200809L

=== Platform / compiler info ===

  Platform: Linux
  Compiler: GCC <major>.<minor>.<patch>
  C standard: C11 (__STDC_VERSION__ = 201112L)

=== Feature-gated functions ===

  strcasestr("Hello, World!", "world") found at offset 7
  -> strcasestr() available (GNU extension)
  strnlen("Hello, World!", 100) = 13
  -> strnlen() available (POSIX.1-2008)
```

Observa que `_POSIX_C_SOURCE` aparece como `200809L` aunque solo definimos
`_GNU_SOURCE`. Esto es porque `_GNU_SOURCE` es un superset: activa las
extensiones GNU mas todas las POSIX. Por eso `strnlen()` tambien esta
disponible.

### Limpieza de la parte 3

```bash
rm -f feature_macros
```

---

## Parte 4 -- NDEBUG y assert()

**Objetivo**: Entender como `NDEBUG` desactiva todas las llamadas a `assert()`,
y por que `assert()` no debe contener side effects.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat ndebug_assert.c
```

Observa:

- `assert(b != 0 && "divisor must not be zero")` verifica una condicion y
  aborta el programa si es falsa. El string despues de `&&` aparece en el
  mensaje de error (truco comun para agregar mensajes a assert)
- Con `NDEBUG` definido, `assert()` se expande a `((void)0)` -- desaparece
  completamente
- La seccion de side effects muestra por que separar `++counter` del `assert()`

### Paso 4.2 -- Compilar y ejecutar sin NDEBUG

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ndebug_assert.c -o ndebug_assert
./ndebug_assert
```

Salida esperada:

```
=== NDEBUG and assert() ===

NDEBUG is NOT defined -> assert() calls are ACTIVE

Calling divide(10, 2)...
  Result: 5

Calling divide(10, 0)...
  assert(b != 0) will FAIL -> program aborts
  (Line commented out to continue the demo)

=== Side effects in assert -- antipattern ===

counter = 1 (should always be 1)
  -> assert(counter > 0) passed.
```

### Paso 4.3 -- Compilar con -DNDEBUG

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DNDEBUG \
    ndebug_assert.c -o ndebug_assert
./ndebug_assert
```

Salida esperada:

```
=== NDEBUG and assert() ===

NDEBUG is DEFINED -> assert() calls are DISABLED

Calling divide(10, 2)...
  Result: 5

Calling divide(10, 0)...
  assert(b != 0) is disabled -- this will crash with SIGFPE!
  (Skipping to avoid crash in this demo)

=== Side effects in assert -- antipattern ===

counter = 1 (should always be 1)
  -> With NDEBUG, assert was a no-op but ++counter still ran
     because we separated the side effect from the assertion.
```

Con `NDEBUG`, `assert(b != 0)` desaparece -- si se ejecutara `divide(10, 0)`,
no habria ninguna verificacion y el programa crashearia con SIGFPE (division
por cero). Por eso `assert()` es para desarrollo y testing, no para validacion
en produccion.

### Paso 4.4 -- Ver un assert fallar

Descomenta la linea `/* divide(10, 0); */` en el codigo fuente (esta en la
seccion "Test 2"), compilar SIN `-DNDEBUG`, y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ndebug_assert.c -o ndebug_assert
./ndebug_assert
```

Salida esperada (el programa aborta):

```
=== NDEBUG and assert() ===

NDEBUG is NOT defined -> assert() calls are ACTIVE

Calling divide(10, 2)...
  Result: 5

Calling divide(10, 0)...
  assert(b != 0) will FAIL -> program aborts
ndebug_assert: ndebug_assert.c:19: divide: Assertion `b != 0 && "divisor must not be zero"' failed.
Aborted (core dumped)
```

El mensaje de `assert()` incluye el archivo, la linea, la funcion y la
expresion que fallo. Despues de observar el resultado, vuelve a comentar la
linea para restaurar el codigo original.

### Limpieza de la parte 4

```bash
rm -f ndebug_assert
```

---

## Parte 5 -- #error y #warning

**Objetivo**: Usar `#error` para detener la compilacion cuando faltan macros
requeridos o sus valores son invalidos, y `#warning` para emitir advertencias
de deprecacion.

### Paso 5.1 -- Examinar el codigo fuente

```bash
cat error_warning.c
```

Observa:

- `#ifndef BUFFER_SIZE` seguido de `#error` obliga a definir el macro desde
  la linea de compilacion
- `#if BUFFER_SIZE < 64` seguido de `#error` valida el valor minimo
- `#warning` emite un aviso cuando `USE_OLD_API` esta definido, pero la
  compilacion continua

### Paso 5.2 -- Compilar sin BUFFER_SIZE

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic error_warning.c -o error_warning
```

Salida esperada:

```
error_warning.c:17:6: error: #error "BUFFER_SIZE must be defined. Use -DBUFFER_SIZE=N"
   17 |     #error "BUFFER_SIZE must be defined. Use -DBUFFER_SIZE=N"
      |      ^~~~~
```

La compilacion se detuvo. `#error` es util para validar la configuracion del
build en tiempo de compilacion, no en runtime.

### Paso 5.3 -- Compilar con BUFFER_SIZE demasiado pequeno

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DBUFFER_SIZE=32 \
    error_warning.c -o error_warning
```

Salida esperada:

```
error_warning.c:21:6: error: #error "BUFFER_SIZE must be at least 64"
   21 |     #error "BUFFER_SIZE must be at least 64"
      |      ^~~~~
```

El primer `#ifndef BUFFER_SIZE` pasa (porque esta definido), pero el segundo
`#if BUFFER_SIZE < 64` falla porque 32 < 64.

### Paso 5.4 -- Compilar con BUFFER_SIZE valido

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DBUFFER_SIZE=128 \
    error_warning.c -o error_warning
./error_warning
```

Salida esperada:

```
=== #error and #warning ===

BUFFER_SIZE = 128
sizeof(buffer) = 128

Using current API
```

### Paso 5.5 -- Activar #warning con USE_OLD_API

`#warning` es una extension de GCC/Clang (estandarizada en C23). Con
`-Wpedantic` en modo C11, el compilador emite un warning adicional sobre la
directiva misma. En este paso usamos `-std=c11 -Wall -Wextra` sin `-Wpedantic`
para ver solo el warning intencionado:

```bash
gcc -std=c11 -Wall -Wextra -DBUFFER_SIZE=256 -DUSE_OLD_API \
    error_warning.c -o error_warning
```

Salida esperada:

```
error_warning.c:26:6: warning: #warning "USE_OLD_API is deprecated -- migrate to the new API" [-Wcpp]
   26 |     #warning "USE_OLD_API is deprecated -- migrate to the new API"
      |      ^~~~~~~
```

La compilacion continua a pesar del warning. `#warning` no detiene el build,
solo avisa.

```bash
./error_warning
```

Salida esperada:

```
=== #error and #warning ===

BUFFER_SIZE = 256
sizeof(buffer) = 256

Using OLD API (deprecated)
```

### Limpieza de la parte 5

```bash
rm -f error_warning
```

---

## Parte 6 -- gcc -E para inspeccionar el preprocesador

**Objetivo**: Usar `gcc -E` para ver la salida del preprocesador y verificar
que codigo se incluye o excluye segun los macros definidos.

### Paso 6.1 -- Examinar el codigo fuente

```bash
cat preprocess_view.c
```

Observa que es un archivo pequeno con dos features controladas por
`FEATURE_GREET` y `FEATURE_FAREWELL`. Cada una define una funcion y la llama
desde `main()`.

### Paso 6.2 -- Compilar y ejecutar sin features

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic preprocess_view.c -o preprocess_view
./preprocess_view
```

Salida esperada:

```
Greeting disabled.
Farewell disabled.
```

### Paso 6.3 -- Ver la salida del preprocesador sin features

```bash
gcc -std=c11 -E preprocess_view.c | tail -20
```

Usamos `tail -20` porque `gcc -E` incluye cientos de lineas de la expansion
de `<stdio.h>`. Solo nos interesan las ultimas lineas (nuestro codigo).

Salida esperada:

```
int main(void) {



    printf("Greeting disabled.\n");





    printf("Farewell disabled.\n");


    return 0;
}
```

Las funciones `greet()` y `farewell()` no aparecen. Los bloques `#ifdef` que
no se cumplieron fueron eliminados completamente. Solo quedan las ramas `#else`.

### Paso 6.4 -- Predecir el efecto de FEATURE_GREET

Antes de preprocesar, responde mentalmente:

- Con `-DFEATURE_GREET`, la funcion `greet()` aparecera en la salida?
- En `main()`, que linea se incluira: `greet()` o `printf("Greeting
  disabled.")`?
- Y para `FEATURE_FAREWELL` (que NO esta definido)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.5 -- Preprocesar con FEATURE_GREET

```bash
gcc -std=c11 -E -DFEATURE_GREET preprocess_view.c | tail -25
```

Salida esperada:

```
static void greet(void) {
    printf("Hello! Greeting feature is active.\n");
}

int main(void) {

    greet();







    printf("Farewell disabled.\n");


    return 0;
}
```

Observa:

- `greet()` aparece ahora (su `#ifdef FEATURE_GREET` se cumplio)
- En `main()`, `greet()` reemplazo a `printf("Greeting disabled.")`
- `farewell()` sigue sin aparecer (su `FEATURE_FAREWELL` no esta definido)
- Las lineas en blanco corresponden a las directivas eliminadas (`#ifdef`,
  `#else`, `#endif`)

### Paso 6.6 -- Preprocesar con ambas features

```bash
gcc -std=c11 -E -DFEATURE_GREET -DFEATURE_FAREWELL \
    preprocess_view.c | tail -25
```

Salida esperada:

```
static void greet(void) {
    printf("Hello! Greeting feature is active.\n");
}



static void farewell(void) {
    printf("Goodbye! Farewell feature is active.\n");
}

int main(void) {

    greet();




    farewell();



    return 0;
}
```

Ambas funciones aparecen y se llaman desde `main()`. `gcc -E` es la
herramienta definitiva para verificar que codigo pasa el preprocesador cuando
tienes compilacion condicional compleja.

### Paso 6.7 -- Compilar y ejecutar con ambas features

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic \
    -DFEATURE_GREET -DFEATURE_FAREWELL \
    preprocess_view.c -o preprocess_view
./preprocess_view
```

Salida esperada:

```
Hello! Greeting feature is active.
Goodbye! Farewell feature is active.
```

### Limpieza de la parte 6

```bash
rm -f preprocess_view
```

---

## Limpieza final

```bash
rm -f ifdef_basics conditional_chain feature_macros
rm -f ndebug_assert error_warning preprocess_view
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md           error_warning.c   ndebug_assert.c
conditional_chain.c feature_macros.c  preprocess_view.c
ifdef_basics.c
```

---

## Conceptos reforzados

1. `#ifdef` verifica si un macro EXISTE, sin importar su valor. `#define FOO 0`
   hace que `#ifdef FOO` sea TRUE. Para verificar el VALOR, usar `#if FOO`.

2. `gcc -DNAME` define un macro con valor 1. `gcc -DNAME=valor` define un macro
   con un valor especifico. Equivale a poner `#define NAME valor` al inicio del
   archivo.

3. `#if`/`#elif`/`#else`/`#endif` permiten seleccion multiple. `defined()` se
   puede combinar con `&&` y `||` para verificar multiples macros, algo que
   `#ifdef` no puede hacer.

4. Un macro no definido en `#if` silenciosamente vale 0. Esto es un bug
   potencial. Usar `#if defined(X) && X` para ser explicito, o `-Wundef` para
   detectarlo.

5. `_POSIX_C_SOURCE` y `_GNU_SOURCE` son feature test macros que desbloquean
   funciones adicionales. Deben definirse ANTES de cualquier `#include`.
   `_GNU_SOURCE` es un superset que incluye POSIX.

6. `NDEBUG` desactiva todas las llamadas a `assert()`, que se expanden a
   `((void)0)`. Nunca poner side effects dentro de `assert()` porque
   desaparecen en release builds.

7. `#error` detiene la compilacion con un mensaje de error. Es util para
   validar que macros requeridos esten definidos con valores validos.
   `#warning` emite un aviso pero la compilacion continua (extension
   GCC/Clang, estandar en C23).

8. `gcc -E` muestra la salida del preprocesador. Los bloques condicionales
   que no se cumplieron desaparecen completamente. Es la herramienta para
   diagnosticar problemas de compilacion condicional.
