# Lab -- _Noreturn y _Static_assert

## Objetivo

Observar el comportamiento de `_Noreturn` (funciones que nunca retornan) y
`_Static_assert` (validaciones en tiempo de compilacion). Al finalizar, sabras
interpretar los warnings que genera el compilador con `noreturn`, verificar
supuestos de plataforma con `static_assert`, y combinar ambos en un patron
practico de manejo de errores.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `noreturn_basic.c` | Funcion `die()` con `noreturn` que elimina warning |
| `noreturn_without.c` | Misma logica sin `noreturn` (produce warning) |
| `noreturn_bad.c` | Funcion `noreturn` que intenta retornar (UB) |
| `static_assert_basic.c` | Validaciones de sizeof, alignment, enum-array |
| `static_assert_fail.c` | Assertion que falla intencionalmente |
| `error_framework.c` | Mini-framework combinando `noreturn` + `static_assert` |

---

## Parte 1 -- _Noreturn

**Objetivo**: Entender como `noreturn` informa al compilador que una funcion
nunca retorna, eliminando warnings y habilitando optimizaciones.

### Paso 1.1 -- Compilar sin noreturn

Compila `noreturn_without.c`. Este archivo tiene una funcion `die()` que llama
a `exit()` pero **no** esta marcada como `noreturn`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic noreturn_without.c -o noreturn_without
```

Antes de ejecutar, predice:

- La funcion `get_value()` retorna `int` en los 3 cases del switch pero despues
  llama a `die()`. El compilador sabe que `die()` no retorna?
- Habra algun warning?

### Paso 1.2 -- Analizar el warning

Salida esperada de la compilacion:

```
noreturn_without.c: In function 'get_value':
noreturn_without.c:17:1: warning: control reaches end of non-void function [-Wreturn-type]
```

El compilador no sabe que `die()` nunca retorna. Desde su perspectiva, despues
de `die("unknown type")` la funcion podria continuar ejecutando, y como
`get_value()` debe retornar `int`, emite el warning.

Ejecuta el programa para verificar que funciona a pesar del warning:

```bash
./noreturn_without
```

Salida esperada:

```
Value for type 2: 20
```

### Paso 1.3 -- Compilar con noreturn

Ahora compila `noreturn_basic.c`. Este archivo tiene la misma logica pero
`die()` esta marcada con `noreturn`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic noreturn_basic.c -o noreturn_basic
```

Observa: **sin warnings**. El compilador sabe que `die()` nunca retorna, asi
que no necesita un `return` despues de llamarla. `get_value()` esta completa.

```bash
./noreturn_basic
```

Salida esperada:

```
Value for type 1: 10
Value for type 2: 20
Value for type 3: 30
```

### Paso 1.4 -- Examinar el codigo fuente

Observa la diferencia clave entre ambos archivos:

```bash
head -6 noreturn_without.c
```

```c
/* No noreturn specifier -- just a regular function */
void die(const char *msg) {
```

```bash
head -6 noreturn_basic.c
```

```c
#include <stdnoreturn.h>

noreturn void die(const char *msg) {
```

El `#include <stdnoreturn.h>` provee el macro `noreturn` que se expande a
`_Noreturn`. Alternativamente se puede usar `_Noreturn` directamente sin el
header.

### Paso 1.5 -- Funcion noreturn que retorna (UB)

Compila `noreturn_bad.c`. Esta funcion esta marcada como `noreturn` pero tiene
un camino de ejecucion donde retorna:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic noreturn_bad.c -o noreturn_bad
```

Antes de mirar la salida, predice:

- Que tipo de mensaje emitira el compilador? Warning o error?
- El programa compilara?

### Paso 1.6 -- Verificar la prediccion

Salida esperada de la compilacion:

```
noreturn_bad.c: In function 'maybe_exit':
noreturn_bad.c:12:1: warning: 'noreturn' function does return
```

Es un **warning**, no un error. El programa compila pero el comportamiento es
**undefined** si `maybe_exit()` efectivamente retorna. El compilador confia en
tu promesa de que la funcion no retorna; si rompes esa promesa, el resultado es
impredecible.

Ejecuta para ver que pasa (el resultado depende del compilador y la
optimizacion):

```bash
./noreturn_bad
```

El programa puede funcionar, crashear, o comportarse de forma extraña. Es
comportamiento indefinido: cualquier resultado es "correcto".

### Paso 1.7 -- Activar die() en noreturn_basic

Ejecuta `noreturn_basic` con un caso no cubierto para ver `die()` en accion.
Para esto, modifica temporalmente la llamada en main:

```bash
cat > test_die.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

int get_value(int type) {
    switch (type) {
        case 1: return 10;
        case 2: return 20;
        case 3: return 30;
    }
    die("unknown type");
}

int main(void) {
    printf("Requesting type 99...\n");
    printf("Value: %d\n", get_value(99));
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic test_die.c -o test_die
./test_die
```

Salida esperada:

```
Requesting type 99...
FATAL: unknown type
```

El programa termina con `exit(1)`. La linea `printf("Value: %d\n", ...)` nunca
se ejecuta porque `die()` no retorna.

### Limpieza de la parte 1

```bash
rm -f noreturn_basic noreturn_without noreturn_bad test_die test_die.c
```

---

## Parte 2 -- _Static_assert

**Objetivo**: Usar `static_assert` para verificar supuestos sobre la plataforma
en tiempo de compilacion, sin costo en runtime.

### Paso 2.1 -- Compilar las validaciones

Compila `static_assert_basic.c`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic static_assert_basic.c -o static_assert_basic
```

Sin errores de compilacion significa que **todas** las aserciones pasaron.
Ejecuta para ver los valores de tu plataforma:

```bash
./static_assert_basic
```

Salida esperada (los valores pueden variar segun tu sistema):

```
All static assertions passed!
sizeof(char)   = 1
sizeof(int)    = ~4
sizeof(long)   = ~8
sizeof(Packet) = 8
alignof(double)= ~8
COLOR_COUNT    = 3
```

### Paso 2.2 -- Examinar las aserciones

```bash
head -27 static_assert_basic.c
```

Observa los diferentes tipos de validaciones:

| Asercion | Que verifica |
|----------|-------------|
| `sizeof(char) == 1` | Garantia del estandar C (siempre 1) |
| `CHAR_BIT == 8` | Que un byte tiene 8 bits |
| `sizeof(int) >= 4` | Enteros de al menos 32 bits |
| `sizeof(struct Packet) == 8` | Sin padding inesperado |
| `color_names == COLOR_COUNT` | Array sincronizado con enum |
| `alignof(double) >= 4` | Alignment minimo de doubles |
| `sizeof(int) <= sizeof(long)` | Relacion entre tipos |

Cada una falla en compilacion si la condicion no se cumple. No generan codigo
ni tienen costo en runtime.

### Paso 2.3 -- Provocar un fallo

Compila `static_assert_fail.c` que tiene una asercion intencionalmente falsa:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic static_assert_fail.c -o static_assert_fail
```

Antes de mirar, predice:

- El mensaje que definimos en el `static_assert` aparecera en la salida?
- El compilador creara un ejecutable?

### Paso 2.4 -- Verificar el fallo

Salida esperada (error, no warning):

```
static_assert_fail.c:5:1: error: static assertion failed: "int must be 2 bytes (this will fail on most platforms)"
```

La compilacion **falla**. No se genera ejecutable. El mensaje que proporcionaste
en el segundo argumento aparece como parte del error. Esto es lo que hace
`static_assert` util: **detiene la compilacion** antes de que un supuesto
incorrecto cause bugs en runtime.

Verifica que no se creo ejecutable:

```bash
ls static_assert_fail 2>&1
```

Salida esperada:

```
ls: cannot access 'static_assert_fail': No such file or directory
```

### Paso 2.5 -- Diferencia con assert() (runtime)

Crea un archivo temporal para comparar `static_assert` con `assert()`:

```bash
cat > assert_compare.c << 'EOF'
#include <stdio.h>
#include <assert.h>

static_assert(sizeof(int) >= 4, "need 32-bit ints");   /* compile time */

int main(void) {
    int x = 5;

    /* Runtime assert -- evaluated when the program runs */
    assert(x > 0);
    printf("x = %d (assert passed)\n", x);
    fflush(stdout);    /* ensure output appears before abort */

    /* This will fail at runtime, not at compile time */
    assert(x > 100);
    printf("This line is never reached.\n");

    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic assert_compare.c -o assert_compare
./assert_compare
```

Salida esperada:

```
x = 5 (assert passed)
assert_compare: assert_compare.c:15: main: Assertion `x > 100' failed.
```

El `static_assert` se verifico en compilacion (paso). El `assert(x > 100)`
fallo en **runtime** y llamo a `abort()`. Diferencia clave:

- `static_assert`: detiene la **compilacion**. Costo runtime: zero.
- `assert()`: detiene la **ejecucion**. Se evalua cada vez que se ejecuta.

### Paso 2.6 -- static_assert dentro de un struct

Crea un ejemplo que valida el layout de un struct justo donde se define:

```bash
cat > struct_check.c << 'EOF'
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

struct NetworkHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_number;
    uint32_t ack_number;
};
static_assert(sizeof(struct NetworkHeader) == 12,
              "NetworkHeader must be exactly 12 bytes for wire format");

int main(void) {
    printf("sizeof(NetworkHeader) = %zu bytes\n",
           sizeof(struct NetworkHeader));
    printf("Layout verified at compile time.\n");
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic struct_check.c -o struct_check
./struct_check
```

Salida esperada:

```
sizeof(NetworkHeader) = 12 bytes
Layout verified at compile time.
```

Si alguien agrega un campo al struct sin actualizar el `static_assert`, la
compilacion falla inmediatamente. Es una red de seguridad gratuita.

### Limpieza de la parte 2

```bash
rm -f static_assert_basic assert_compare assert_compare.c struct_check struct_check.c
```

---

## Parte 3 -- Caso practico: framework de error handling

**Objetivo**: Combinar `noreturn` y `static_assert` en un patron realista
de manejo de errores.

### Paso 3.1 -- Examinar el framework

```bash
cat error_framework.c
```

Observa la estructura del programa:

1. **static_assert** al inicio: verifica supuestos de plataforma y layout del
   struct `Record`
2. **noreturn void fatal()**: funcion que imprime error y llama a `abort()`
3. **Macro FATAL(msg)**: agrega `__FILE__` y `__LINE__` automaticamente
4. **Macro ENSURE(cond, msg)**: verifica una condicion en runtime; si falla,
   llama a `FATAL`

Antes de compilar, predice:

- Cuantos `static_assert` hay? Que verifican?
- La funcion `process_records(NULL, 3)` va a terminar el programa?
- Se imprimira la linea "This line is never printed"?

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic error_framework.c -o error_framework
./error_framework
```

Salida esperada:

```
=== Error Framework Demo ===

Processing 3 records:
  id=1 name=Alice      score=95
  id=2 name=Bob        score=87
  id=3 name=Charlie    score=92

All OK. Now triggering a fatal error...

error_framework.c:38: FATAL: assertion failed: records != NULL -- records pointer is NULL
```

Observa:

- Los records se procesaron correctamente la primera vez
- La segunda llamada con `NULL` activo `ENSURE`, que llamo a `FATAL`, que
  llamo a `fatal()` (noreturn), que llamo a `abort()`
- El mensaje incluye el archivo y la linea gracias a `__FILE__` y `__LINE__`
- "This line is never printed" nunca se imprimio porque `fatal()` no retorna

### Paso 3.3 -- Verificar el codigo de salida

```bash
./error_framework 2>/dev/null; echo "Exit code: $?"
```

Salida esperada (se repite la salida de stdout, y al final el codigo):

```
=== Error Framework Demo ===

Processing 3 records:
  id=1 name=Alice      score=95
  id=2 name=Bob        score=87
  id=3 name=Charlie    score=92

All OK. Now triggering a fatal error...

Exit code: 134
```

El mensaje de FATAL no aparece porque redirigimos stderr a `/dev/null`.
El codigo 134 = 128 + 6 (SIGABRT). `abort()` envia la senal SIGABRT al
proceso.

### Paso 3.4 -- Probar que static_assert protege el struct

Crea una version modificada donde el struct cambia pero el `static_assert` no
se actualiza:

```bash
cat > framework_broken.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdnoreturn.h>
#include <stdint.h>

struct Record {
    int32_t id;
    char    name[32];
    int32_t score;
    int32_t extra_field;    /* new field added */
};
static_assert(sizeof(struct Record) == 40,
              "Record layout changed unexpectedly");

int main(void) {
    printf("This should not compile.\n");
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic framework_broken.c -o framework_broken
```

Salida esperada (error):

```
framework_broken.c:13:1: error: static assertion failed: "Record layout changed unexpectedly"
```

El `static_assert` detecto que agregar `extra_field` cambio el tamano del
struct. Esto fuerza al desarrollador a revisar todo el codigo que depende del
layout antes de actualizar la asercion.

### Paso 3.5 -- Sin noreturn en fatal: que cambia?

Crea una version sin `noreturn` en `fatal()` para ver la diferencia:

```bash
cat > framework_no_noreturn.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

static_assert(sizeof(int) >= 4, "need 32-bit ints");

/* fatal WITHOUT noreturn */
void fatal(const char *file, int line, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

#define FATAL(msg) fatal(__FILE__, __LINE__, msg)

#define ENSURE(cond, msg) do { \
    if (!(cond)) FATAL("assertion failed: " #cond " -- " msg); \
} while (0)

int safe_divide(int a, int b) {
    ENSURE(b != 0, "division by zero");
    return a / b;
}

int main(void) {
    printf("10 / 3 = %d\n", safe_divide(10, 3));
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic framework_no_noreturn.c -o framework_no_noreturn
```

Antes de mirar la salida, predice:

- Sin `noreturn` en `fatal()`, la funcion `safe_divide()` produce algun
  warning?
- El programa funciona igual?

### Paso 3.6 -- Verificar la prediccion

Con `noreturn` en `fatal()`, el compilador sabe que `ENSURE` nunca retorna si
la condicion falla, asi que `safe_divide()` no genera warning despues del
`ENSURE`.

Sin `noreturn`, el compilador no puede hacer esa deduccion. Sin embargo, en
este caso particular no hay warning porque el `return a / b` esta despues del
`ENSURE` en la misma ruta de ejecucion.

Donde se nota la diferencia es en funciones tipo switch como `get_value()` de
la Parte 1. La version sin `noreturn` produce el warning
`control reaches end of non-void function`.

```bash
./framework_no_noreturn
```

Salida esperada:

```
10 / 3 = 3
```

### Limpieza de la parte 3

```bash
rm -f error_framework framework_broken.c framework_no_noreturn framework_no_noreturn.c
```

---

## Limpieza final

```bash
rm -f *.o test_die test_die.c assert_compare assert_compare.c struct_check struct_check.c
rm -f framework_broken framework_broken.c framework_no_noreturn framework_no_noreturn.c
rm -f noreturn_basic noreturn_without noreturn_bad static_assert_basic error_framework
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  error_framework.c  noreturn_bad.c  noreturn_basic.c  noreturn_without.c  static_assert_basic.c  static_assert_fail.c
```

---

## Conceptos reforzados

1. `noreturn` (de `<stdnoreturn.h>`) indica al compilador que una funcion nunca
   retorna. Elimina el warning `control reaches end of non-void function` en
   funciones que llaman a `die()` o similares como ultimo recurso.

2. Si una funcion marcada como `noreturn` tiene un camino de ejecucion donde
   retorna, el compilador emite warning `'noreturn' function does return`. El
   comportamiento es **undefined** si la funcion efectivamente retorna.

3. `static_assert` verifica una condicion en **tiempo de compilacion**. Si
   falla, la compilacion se detiene con error. No genera codigo ni tiene costo
   en runtime.

4. `assert()` verifica una condicion en **runtime**. Si falla, imprime un
   mensaje y llama a `abort()`. Se puede desactivar con `#define NDEBUG`.
   `static_assert` **no** se puede desactivar.

5. `static_assert` es ideal para validar supuestos de plataforma (`sizeof`,
   `alignof`), layout de structs (detectar padding inesperado), y
   sincronizacion entre enums y arrays de nombres.

6. Combinar `noreturn` y `static_assert` en un framework de errores permite
   verificaciones en compilacion (supuestos de plataforma) y en runtime
   (condiciones de error), con el compilador generando codigo optimo porque
   sabe que las rutas de error no retornan.

7. `abort()` envia la senal SIGABRT al proceso. El codigo de salida es 134
   (128 + 6). Las funciones `exit()`, `abort()`, `_Exit()` y `quick_exit()` de
   la biblioteca estandar son todas `noreturn`.
