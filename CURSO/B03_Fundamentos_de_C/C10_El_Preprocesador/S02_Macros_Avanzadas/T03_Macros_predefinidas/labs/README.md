# Lab -- Macros predefinidas

## Objetivo

Explorar las macros predefinidas del compilador C: las del estandar (`__FILE__`,
`__LINE__`, `__func__`, `__DATE__`, `__TIME__`, `__STDC__`, `__STDC_VERSION__`)
y las extensiones GCC/Clang (`__GNUC__`, `__SIZEOF_*__`, `__COUNTER__`).
Al finalizar, sabras usar estas macros para construir sistemas de logging,
asserts personalizados y build info embebido en binarios. Tambien sabras
listar todas las macros predefinidas del compilador con `gcc -dM -E`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `predefined_basics.c` | Macros estandar: __FILE__, __LINE__, __func__, __DATE__, __TIME__, __STDC_VERSION__ |
| `debug_log.c` | Macros de logging, assert personalizado, RETURN_ERROR |
| `build_info.c` | Informacion de build embebida: compilador, plataforma, arquitectura |
| `counter_demo.c` | __COUNTER__ y generacion de nombres unicos (extension GCC/Clang) |

---

## Parte 1 -- Macros estandar basicas

**Objetivo**: Verificar el comportamiento de `__FILE__`, `__LINE__`, `__func__`,
`__DATE__`, `__TIME__`, `__STDC__` y `__STDC_VERSION__`.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat predefined_basics.c
```

Observa:

- `__FILE__` se expande al nombre del archivo fuente
- `__LINE__` se expande al numero de linea donde aparece
- `__func__` cambia segun la funcion donde se use
- `__DATE__` y `__TIME__` se fijan en el momento de la compilacion
- `__STDC_VERSION__` indica que version del estandar se usa

### Paso 1.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic predefined_basics.c -o predefined_basics
```

No debe producir warnings ni errores.

### Paso 1.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `__FILE__` mostrara la ruta completa o solo el nombre del archivo?
  (Depende de como se invoco gcc -- con ruta relativa o absoluta.)
- `__LINE__` en el primer `printf` de `main`, que numero de linea tendra?
  No sera 1 -- sera la linea donde esta esa sentencia en el archivo fuente.
- `__STDC_VERSION__` con `-std=c11`, que valor tendra?
  (Pista: C99 es 199901L, C11 es 201112L.)

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Ejecutar y verificar

```bash
./predefined_basics
```

Salida esperada:

```
=== Standard Predefined Macros ===

__FILE__         : predefined_basics.c
__LINE__         : 19
__DATE__         : <fecha de hoy>
__TIME__         : <hora actual>
__STDC__         : 1
__STDC_VERSION__ : 201112

=== __func__ in different functions ===

  Inside function: main
  Inside function: show_func_name
  At line: 12

=== __LINE__ expands where it appears ===

  This is line 30
  This is line 31
  This is line 32
```

Observa:

- `__FILE__` muestra `predefined_basics.c` (nombre relativo, porque
  compilaste con `gcc ... predefined_basics.c`)
- `__LINE__` vale 19 en la primera llamada a `printf` dentro de `main`,
  no 1 -- refleja la linea real del archivo fuente
- `__func__` cambia segun la funcion: `main` en `main()`, `show_func_name`
  en la otra funcion
- `__STDC_VERSION__` vale `201112` con `-std=c11` (el sufijo `L` no se
  imprime, es parte del tipo `long`)
- Las tres lineas consecutivas de `__LINE__` muestran 30, 31, 32 --
  cada `__LINE__` se expande donde aparece en el fuente

### Paso 1.5 -- __FILE__ cambia segun como se invoca gcc

Ahora compila con la ruta completa al archivo:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ./predefined_basics.c -o predefined_basics
./predefined_basics | head -4
```

Salida esperada:

```
=== Standard Predefined Macros ===

__FILE__         : ./predefined_basics.c
__LINE__         : 19
```

`__FILE__` ahora muestra `./predefined_basics.c` en lugar de
`predefined_basics.c`. El compilador usa exactamente la cadena que recibe como
argumento. En proyectos con Makefiles, esto suele ser una ruta relativa desde
el directorio de build.

### Paso 1.6 -- Cambiar el estandar y observar __STDC_VERSION__

```bash
gcc -std=c99 -Wall -Wextra -Wpedantic predefined_basics.c -o predefined_basics
./predefined_basics | grep STDC_VERSION
```

Salida esperada:

```
__STDC_VERSION__ : 199901
```

Con `-std=c99`, `__STDC_VERSION__` cambia a `199901`. Esta macro permite
escribir codigo que se adapta al estandar disponible.

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic predefined_basics.c -o predefined_basics
./predefined_basics | grep STDC_VERSION
```

Salida esperada:

```
__STDC_VERSION__ : 201710
```

### Limpieza de la parte 1

```bash
rm -f predefined_basics
```

---

## Parte 2 -- Logging, assert y error tracking

**Objetivo**: Usar `__FILE__`, `__LINE__` y `__func__` dentro de macros para
construir un sistema de logging y un assert personalizado que reportan la
ubicacion exacta del caller.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat debug_log.c
```

Observa tres patrones clave:

- `LOG(level, fmt, ...)` -- macro variadic que imprime nivel, archivo, linea,
  funcion y mensaje a stderr
- `ASSERT(cond)` -- si la condicion falla, imprime la expresion (con `#cond`),
  la ubicacion, y llama a `abort()`
- `RETURN_ERROR(code, fmt, ...)` -- loguea el error y retorna un codigo de error

Nota importante: `LOG_INFO`, `LOG_WARN` y `LOG_ERROR` siempre reciben al menos
un argumento variadic. Esto es necesario porque C11 estandar requiere al menos
un argumento para `...` en macros variadicas (la extension GNU `##__VA_ARGS__`
permite cero argumentos, pero no es portable).

### Paso 2.2 -- Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic debug_log.c -o debug_log
```

No debe producir warnings ni errores.

### Paso 2.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Cuando `LOG_ERROR` se usa dentro de `open_config()`, que archivo y linea
  reportara? La linea de la definicion de `LOG_ERROR`, o la linea donde se
  llama `RETURN_ERROR` dentro de `open_config`?
- Cuando `ASSERT(value >= 0)` falla, que texto mostrara despues de
  "Assertion failed:"? (Pista: `#cond` stringifica la expresion.)
- El programa terminara normalmente o con un codigo de salida no-cero?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Ejecutar y verificar

```bash
./debug_log 2>&1
```

Se redirige stderr a stdout con `2>&1` para ver toda la salida en orden.

Salida esperada:

```
=== Logging macros demo ===

[INFO] debug_log.c:71 main(): program started, pid=<pid>
[WARN] debug_log.c:72 main(): this is a warning from main

--- Calling open_config with invalid path ---

[ERROR] debug_log.c:56 open_config(): cannot open '/nonexistent/config.txt': No such file or directory
open_config returned: -1

--- Calling process_data with valid value ---

[INFO] debug_log.c:63 process_data(): processing value 42
[INFO] debug_log.c:65 process_data(): value 42 OK, continuing

--- Calling process_data with invalid value ---
(this will trigger ASSERT and abort)

[INFO] debug_log.c:63 process_data(): processing value -1
Assertion failed: value >= 0
  File:     debug_log.c
  Line:     64
  Function: process_data
```

Observa:

- Cada mensaje de log muestra el archivo, la linea y la funcion **donde se
  uso el macro**, no donde se definio. Este es el punto clave: `__FILE__`,
  `__LINE__` y `__func__` se expanden en el lugar de invocacion
- `RETURN_ERROR` en `open_config` (linea 56) muestra la linea de esa funcion,
  no la linea de `main` que la llamo
- `#cond` convirtio `value >= 0` en la cadena `"value >= 0"`
- El programa termina con `abort()` (codigo de salida 134 = 128 + SIGABRT)

### Paso 2.5 -- Verificar el codigo de salida

```bash
./debug_log 2>/dev/null; echo "Exit code: $?"
```

Salida esperada:

```
=== Logging macros demo ===

...
Exit code: 134
```

El codigo 134 = 128 + 6 (SIGABRT). `abort()` envia SIGABRT al proceso.

### Paso 2.6 -- Comparar con el assert estandar

El `assert()` de `<assert.h>` usa el mismo mecanismo internamente. Compara:

```bash
cat > /tmp/std_assert.c << 'EOF'
#include <assert.h>

int main(void) {
    int x = -1;
    assert(x >= 0);
    return 0;
}
EOF
gcc -std=c11 /tmp/std_assert.c -o /tmp/std_assert
/tmp/std_assert 2>&1; echo "Exit code: $?"
```

Salida esperada:

```
std_assert: /tmp/std_assert.c:5: main: Assertion `x >= 0' failed.
Exit code: 134
```

El formato es diferente, pero el mecanismo es el mismo: `__FILE__`, `__LINE__`,
`__func__` y `#cond` (stringificacion de la condicion).

### Limpieza de la parte 2

```bash
rm -f debug_log /tmp/std_assert.c /tmp/std_assert
```

---

## Parte 3 -- Build info embebido

**Objetivo**: Incrustar informacion del compilador, plataforma y timestamp
en el binario usando macros predefinidas estandar y extensiones GCC.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat build_info.c
```

Observa:

- `BUILD_TIMESTAMP` concatena cadenas literales con `__DATE__` y `__TIME__`
  (esto funciona porque son string literals adyacentes)
- `#if defined(__clang__)` se verifica **antes** de `__GNUC__` porque Clang
  tambien define `__GNUC__`
- `__SIZEOF_POINTER__`, `__SIZEOF_INT__`, `__SIZEOF_LONG__` dan el tamano
  de tipos sin necesidad de `sizeof` en runtime
- `NDEBUG` permite distinguir builds debug de release

### Paso 3.2 -- Compilar y ejecutar (build debug)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic build_info.c -o build_info
./build_info
```

Salida esperada:

```
=== Build Information ===

Compiler    : GCC <version>
C Standard  : C11 (__STDC_VERSION__ = 201112)
__STDC__    : 1
Platform    : Linux
Architecture: x86-64
Pointer size: 8 bytes (64-bit)
int size    : 4 bytes
long size   : 8 bytes
Byte order  : little-endian
Build type  : debug (NDEBUG not defined)

Built on <fecha> at <hora>
```

### Paso 3.3 -- Predecir el efecto de NDEBUG

Antes de compilar con `-DNDEBUG`, responde mentalmente:

- Que linea de "Build type" cambiara?
- Cambiara alguna otra cosa en la salida?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Compilar con NDEBUG (build release)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -DNDEBUG build_info.c -o build_info
./build_info | grep "Build type"
```

Salida esperada:

```
Build type  : release (NDEBUG defined)
```

`-DNDEBUG` es equivalente a poner `#define NDEBUG` al inicio del archivo.
En produccion, `NDEBUG` desactiva `assert()` del estandar y tipicamente
se usa para distinguir builds debug de release.

### Paso 3.5 -- Compilar con otro estandar

```bash
gcc -std=c99 -Wall -Wextra -Wpedantic build_info.c -o build_info
./build_info | grep "C Standard"
```

Salida esperada:

```
C Standard  : C99 (__STDC_VERSION__ = 199901)
```

La funcion `get_std_name()` usa `#if __STDC_VERSION__ >= ...` para detectar
el estandar en tiempo de compilacion y devolver el nombre legible.

### Paso 3.6 -- Extraer el timestamp del binario

El timestamp queda embebido como cadena literal en el binario. Puedes
verificarlo con `strings`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic build_info.c -o build_info
strings build_info | grep "Built on"
```

Salida esperada:

```
Built on <fecha> at <hora>
```

Esto demuestra que `__DATE__` y `__TIME__` quedan literalmente dentro del
binario compilado. Por eso generan builds no-reproducibles: cada compilacion
produce un binario diferente aunque el codigo fuente no cambio.

### Paso 3.7 -- Warning de reproducibilidad

GCC puede advertir cuando se usan `__DATE__` o `__TIME__`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wdate-time build_info.c -o build_info 2>&1
```

Salida esperada:

```
build_info.c:10:42: warning: macro "__DATE__" might prevent reproducible builds [-Wdate-time]
build_info.c:10:56: warning: macro "__TIME__" might prevent reproducible builds [-Wdate-time]
```

El flag `-Wdate-time` esta pensado para pipelines de CI/CD donde la
reproducibilidad importa. En ese contexto, se pasan timestamps externos:
`gcc -DBUILD_DATE=\"$(date +%Y-%m-%d)\"`.

### Limpieza de la parte 3

```bash
rm -f build_info
```

---

## Parte 4 -- __COUNTER__ y gcc -dM -E

**Objetivo**: Explorar `__COUNTER__` (extension GCC/Clang) para generar
identificadores unicos, y usar `gcc -dM -E` para listar todas las macros
predefinidas del compilador.

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat counter_demo.c
```

Observa:

- `__COUNTER__` empieza en 0 y se incrementa con cada uso
- `TRACE()` usa `__COUNTER__` para numerar automaticamente los puntos de traza
- `UNIQUE_NAME(prefix)` usa `__COUNTER__` con token pasting (`##`) para
  generar nombres de variables que nunca colisionan
- Cada uso de `__COUNTER__` en cualquier macro consume el siguiente valor

### Paso 4.2 -- Compilar

`__COUNTER__` no es estandar C, asi que `-Wpedantic` daria warning.
Se compila sin ese flag para esta demo:

```bash
gcc -std=c11 -Wall -Wextra counter_demo.c -o counter_demo
```

### Paso 4.3 -- Predecir el resultado

Antes de ejecutar, responde mentalmente:

- La macro `TRACE()` se usa 3 veces. Cada una consume un valor de
  `__COUNTER__`. Que numeros mostrara? (Pista: empieza en 0.)
- Despues de las 3 llamadas a `TRACE()`, `UNIQUE_NAME(tmp_)` se usa 3 veces.
  Que nombres de variables generara? (Pista: `__COUNTER__` sigue desde 3.)

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Ejecutar y verificar

```bash
./counter_demo
```

Salida esperada:

```
=== __COUNTER__ demo ===

[trace #0] counter_demo.c:29: program started
[trace #1] counter_demo.c:30: doing work
[trace #2] counter_demo.c:31: more work

=== Unique variable names with __COUNTER__ ===

tmp_3 = 100
tmp_4 = 200
tmp_5 = 300

Three unique variables created without name collision.
(Verify with: gcc -E counter_demo.c | tail -20)
```

Observa:

- `TRACE()` uso los valores 0, 1, 2
- `UNIQUE_NAME(tmp_)` genero `tmp_3`, `tmp_4`, `tmp_5` (continuo desde 3)
- `__COUNTER__` es global al translation unit: cada uso consume el siguiente
  numero sin importar en que macro se use

### Paso 4.5 -- Verificar con el preprocesador

```bash
gcc -std=c11 -E counter_demo.c | tail -15
```

Salida esperada (simplificada):

```
    printf("[trace #%d] %s:%d: %s\n", 0, "counter_demo.c", 29, "program started");
    printf("[trace #%d] %s:%d: %s\n", 1, "counter_demo.c", 30, "doing work");
    printf("[trace #%d] %s:%d: %s\n", 2, "counter_demo.c", 31, "more work");
    ...
    int tmp_3 = 100;
    int tmp_4 = 200;
    int tmp_5 = 300;
```

El preprocesador reemplazo `__COUNTER__` por numeros concretos y
`UNIQUE_NAME(tmp_)` por nombres reales (`tmp_3`, `tmp_4`, `tmp_5`).

### Paso 4.6 -- Listar TODAS las macros predefinidas

`gcc -dM -E` es la herramienta definitiva para ver todas las macros que el
compilador define automaticamente:

```bash
gcc -std=c11 -dM -E - < /dev/null | wc -l
```

Salida esperada:

```
~250
```

Hay cientos de macros predefinidas. Veamos las mas relevantes:

```bash
gcc -std=c11 -dM -E - < /dev/null | grep -E "^#define __STDC"
```

Salida esperada:

```
#define __STDC__ 1
#define __STDC_HOSTED__ 1
#define __STDC_IEC_559__ 1
#define __STDC_IEC_559_COMPLEX__ 1
#define __STDC_IEC_60559_BFP__ 201404L
#define __STDC_IEC_60559_COMPLEX__ 201404L
#define __STDC_ISO_10646__ 201706L
#define __STDC_UTF_16__ 1
#define __STDC_UTF_32__ 1
#define __STDC_VERSION__ 201112L
```

### Paso 4.7 -- Macros de plataforma y arquitectura

```bash
gcc -std=c11 -dM -E - < /dev/null | grep -E "^#define __(linux|x86_64|SIZEOF|BYTE_ORDER|GNUC)" | sort
```

Salida esperada (puede variar):

```
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#define __GNUC_MINOR__ <minor>
#define __GNUC_PATCHLEVEL__ <patch>
#define __GNUC__ <major>
#define __SIZEOF_DOUBLE__ 8
#define __SIZEOF_FLOAT__ 4
#define __SIZEOF_INT__ 4
#define __SIZEOF_LONG_DOUBLE__ 16
#define __SIZEOF_LONG_LONG__ 8
#define __SIZEOF_LONG__ 8
#define __SIZEOF_POINTER__ 8
#define __SIZEOF_SHORT__ 2
#define __SIZEOF_SIZE_T__ 8
...
#define __linux 1
#define __linux__ 1
#define __x86_64 1
#define __x86_64__ 1
```

Estas macros permiten escribir codigo condicional para diferentes plataformas
y arquitecturas sin necesidad de herramientas externas como autoconf.

### Paso 4.8 -- Comparar macros entre estandares

```bash
diff <(gcc -std=c99 -dM -E - < /dev/null | sort) <(gcc -std=c11 -dM -E - < /dev/null | sort)
```

Observa las diferencias: `__STDC_VERSION__` cambia, y C11 puede definir
macros adicionales (como `__STDC_NO_THREADS__` si no soporta threads).

### Limpieza de la parte 4

```bash
rm -f counter_demo
```

---

## Limpieza final

```bash
rm -f predefined_basics build_info debug_log counter_demo
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  build_info.c  counter_demo.c  debug_log.c  predefined_basics.c
```

---

## Conceptos reforzados

1. `__FILE__` se expande a la cadena exacta que se paso a gcc como nombre del
   archivo fuente. Compilar con `gcc file.c` produce `"file.c"`, compilar con
   `gcc ./file.c` produce `"./file.c"`.

2. `__LINE__` se expande al numero de linea donde aparece en el archivo fuente,
   no al numero de linea del programa en ejecucion. Dentro de una macro, se
   expande en el punto de invocacion, no en la definicion del macro.

3. `__func__` no es un macro sino una variable local implicita equivalente a
   `static const char __func__[] = "nombre_funcion"`. No se puede usar con
   `#` (stringificacion) ni `##` (concatenacion) porque no es un macro.

4. `__DATE__` y `__TIME__` se fijan en el momento de compilacion e incrustan
   timestamps en el binario. Esto rompe la reproducibilidad de builds. El flag
   `-Wdate-time` permite detectar su uso.

5. `__STDC_VERSION__` permite escribir codigo que se adapta al estandar C
   disponible: 199901L para C99, 201112L para C11, 201710L para C17. Esto es
   esencial para macros de compatibilidad (fallbacks entre estandares).

6. Las macros `__GNUC__`, `__SIZEOF_*__`, `__BYTE_ORDER__` y `__COUNTER__`
   son extensiones GCC/Clang, no estandar. Usarlas dentro de `#ifdef __GNUC__`
   mantiene la portabilidad.

7. `__COUNTER__` genera un entero unico que se incrementa con cada uso en
   el translation unit. Es util para generar identificadores unicos con
   token pasting, evitando colisiones de nombres en macros.

8. `gcc -dM -E - < /dev/null` lista todas las macros predefinidas del
   compilador. Es la referencia definitiva para saber que macros estan
   disponibles en tu plataforma y configuracion.

9. El patron fundamental de logging/assert en C es: definir un **macro** (no
   una funcion) que captura `__FILE__`, `__LINE__` y `__func__` en el punto
   de invocacion, y luego pasa esos valores a una funcion que hace el trabajo
   real.
