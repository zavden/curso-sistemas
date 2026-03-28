# T03 — Comportamiento por compilador

## Erratas detectadas en el material original

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| README.md:6,239; labs/README.md:67,256,382 | Múltiples referencias afirman que el default de GCC es `-std=gnu17` y que "GCC 8+" usa gnu17 | La progresión real de defaults es: **GCC 5→gnu11**, **GCC 14→gnu17**, **GCC 15→gnu23**. En GCC 15 (el de este sistema), el default es `gnu23` (`__STDC_VERSION__ = 202311L`), no gnu17. La tabla del lab dice "(default) = gnu17/C23" lo cual es contradictorio. |
| README.md:276–280 | "Orden de evaluación de argumentos" listado bajo "Implementation-defined behavior" | Es **unspecified behavior**, no implementation-defined. La diferencia: implementation-defined requiere que el compilador documente su elección; unspecified no. El orden de evaluación de argumentos de función no está especificado por el estándar y el compilador no tiene obligación de documentar qué orden elige. |

---

## 1. GCC no compila C estándar por defecto

Cuando ejecutas `gcc main.c` sin flags, GCC **no** usa C estándar estricto. Usa un modo `gnu*` que es C + extensiones GNU:

```bash
# Lo que PARECE que haces:
gcc main.c -o main            # "compilar C"

# Lo que REALMENTE hace GCC (depende de la versión):
# GCC 5–13:  -std=gnu11  (C11 + extensiones GNU)
# GCC 14:    -std=gnu17  (C17 + extensiones GNU)
# GCC 15:    -std=gnu23  (C23 + extensiones GNU)

# Para compilar C11 estricto:
gcc -std=c11 main.c -o main

# Para que avise si usas extensiones:
gcc -std=c11 -Wpedantic main.c -o main
```

Verificar el default de tu GCC:

```bash
gcc --version
gcc -dM -E - < /dev/null | grep STDC_VERSION
# GCC 15: __STDC_VERSION__ = 202311L (gnu23)
```

---

## 2. `-std=c11` vs `-std=gnu11`

| Aspecto | `-std=c11` | `-std=gnu11` |
|---------|-----------|-------------|
| Estándar base | C11 | C11 |
| Extensiones GNU | No (warning con `-Wpedantic`) | Sí |
| `__STRICT_ANSI__` definido | Sí | No |
| Headers POSIX (`popen`, `fileno`) | Necesita feature test macro | Disponibles |
| Portabilidad | Alta | Limitada a GCC/Clang |

**Punto clave**: `-std=c11` **sin** `-Wpedantic` sigue aceptando extensiones silenciosamente. El flag `-std=c11` cambia qué macros se definen y qué headers exponen, pero no avisa sobre extensiones por sí solo. Necesitas **ambos** flags juntos.

---

## 3. Extensiones GNU que `-std=gnu*` permite

```c
// 1. typeof (estandarizado en C23):
typeof(42) x = 100;

// 2. Statement expressions:
int max = ({ int a = 5, b = 3; a > b ? a : b; });
// Un bloque que retorna un valor — no existe en C estándar

// 3. Nested functions (solo GCC, NO Clang):
void outer(void) {
    void inner(void) { printf("inner\n"); }
    inner();
}

// 4. Zero-length arrays:
struct Flex {
    int count;
    int data[0];               // extensión GNU
};
// C99/C11 tiene flexible array members:
struct Flex_std {
    int count;
    int data[];                // estándar (sin tamaño)
};

// 5. Case ranges:
switch (c) {
    case 'a' ... 'z':          // extensión GNU
        break;
}

// 6. Binary literals (estandarizados en C23):
int mask = 0b11110000;

// 7. __attribute__:
void die(void) __attribute__((noreturn));
int printf_like(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// 8. Designated initializer con rangos:
int arr[10] = { [2 ... 5] = 42 };

// 9. Labels as values (computed goto):
void *labels[] = { &&label1, &&label2 };
goto *labels[i];
// Usado en intérpretes para dispatch rápido
```

---

## 4. Qué cambia en los headers del sistema

`-std=c11` también afecta qué funciones exponen los headers del sistema:

```c
// Con -std=gnu11 (u otros gnu*):
#include <stdio.h>
// Expone: popen(), fileno(), fdopen()... (extensiones POSIX)

// Con -std=c11:
#include <stdio.h>
// Solo expone funciones del estándar C.
// popen(), fileno() NO están disponibles.

// Para usar funciones POSIX con -std=c11:
#define _POSIX_C_SOURCE 200809L   // ANTES de cualquier #include
#include <stdio.h>
// Ahora popen() está disponible
```

Feature test macros comunes:

| Macro | Qué expone |
|-------|------------|
| `_POSIX_C_SOURCE 200809L` | POSIX.1-2008 |
| `_XOPEN_SOURCE 700` | X/Open + POSIX |
| `_GNU_SOURCE` | Todo lo anterior + extensiones GNU |
| `_DEFAULT_SOURCE` | Equivalente al default de glibc |

Estas macros **deben ir antes de cualquier `#include`**. Controlan qué funciones son visibles en los headers.

---

## 5. GCC vs Clang

Ambos soportan C11, pero difieren en extensiones, warnings y optimización:

### Diferencias en warnings

```c
int x;
printf("%s\n", x);

// GCC:
// warning: format '%s' expects argument of type 'char *',
// but argument 2 has type 'int'

// Clang (más visual):
// warning: format specifies type 'char *' but the argument has type 'int'
//     printf("%s\n", x);
//             ~~     ^
//             %d
// Marca el error con ^ y sugiere el formato correcto
```

### Diferencias en extensiones

| Feature | GCC | Clang |
|---------|-----|-------|
| Nested functions | Sí | No |
| Blocks (`^{ ... }`) | No | Sí (con `-fblocks`) |
| `__attribute__` | Sí (la mayoría) | Sí (la mayoría) |
| VLAs en structs | Sí (extensión) | No |
| Statement expressions | Sí | Sí |

### Cuándo importa

- **Para este curso** (`-std=c11 -Wpedantic`): no importa. El código compila igual en ambos.
- **Para proyectos reales**: compilar con ambos es buena práctica — cada uno detecta bugs que el otro no ve. CI/CD debería tener builds con GCC y Clang.

---

## 6. Implementation-defined vs unspecified behavior

El material original mezcla estos conceptos. La distinción importa:

| Categoría | Requisito del compilador | Ejemplo |
|-----------|--------------------------|---------|
| **Implementation-defined** | Debe elegir un comportamiento consistente y **documentarlo** | `sizeof(int)`, signo de `char`, shift right de negativos |
| **Unspecified** | Puede elegir cualquier comportamiento válido, **sin obligación de documentar** | Orden de evaluación de argumentos de función |
| **Undefined** | Puede pasar cualquier cosa | Signed overflow, null deref |

```c
// Implementation-defined — el compilador DEBE documentar qué hace:
char c = 128;
// x86 GCC/Clang: char es signed → c = -128 (documentado)
// ARM GCC: char puede ser unsigned → c = 128 (documentado)
// Solución: usar signed char o unsigned char explícitamente

int x = -1;
int y = x >> 1;
// Implementation-defined: shift aritmético o lógico
// GCC/Clang en x86: aritmético (preserva signo, documentado)

// Unspecified — el compilador NO necesita documentar:
printf("%d %d\n", f(), g());
// ¿Se llama f() o g() primero? No especificado.
// Puede cambiar entre compilaciones. No depender de un orden.
```

---

## 7. Buenas prácticas para portabilidad

```bash
# 1. Compilar con -std=c11 -Wpedantic:
gcc -std=c11 -Wpedantic main.c -o main
# Detecta cualquier extensión no estándar

# 2. Compilar con ambos compiladores:
gcc   -std=c11 -Wall -Wextra main.c -o main_gcc
clang -std=c11 -Wall -Wextra main.c -o main_clang

# 3. Usar feature test macros para POSIX:
#    #define _POSIX_C_SOURCE 200809L  // antes de cualquier #include

# 4. Usar stdint.h en lugar de confiar en sizeof(int):
#    int32_t x;    // siempre 32 bits
#    uint64_t y;   // siempre 64 bits

# 5. No depender de extensiones GNU:
#    typeof → esperar a C23 o usar macros alternativas
#    nested functions → refactorizar con punteros a función
```

---

## 8. Tabla resumen: `-std=c11` vs `-std=gnu11`

| Feature | `-std=c11` | `-std=gnu11` |
|---------|-----------|-------------|
| `typeof` | No (warning con `-Wpedantic`) | Sí |
| Statement expressions | No | Sí |
| Nested functions | No | Sí (solo GCC) |
| Binary literals | No | Sí |
| Zero-length arrays | No | Sí |
| Case ranges | No | Sí |
| `__attribute__` | Sí (reservado por `__` prefix) | Sí |
| POSIX en headers | Necesita feature test macro | Disponible |
| Portabilidad | Alta | Limitada a GCC/Clang |

---

## Ejercicios

### Ejercicio 1 — Extensiones GNU: silencio vs `-Wpedantic`

Examina `labs/extensions.c` y compila con tres combinaciones de flags:

```bash
cd labs/

# Sin flags (default del sistema):
gcc extensions.c -o ext_default 2>&1
echo "--- salida ---"
./ext_default

# Con -std=c11 (sin -Wpedantic):
gcc -std=c11 extensions.c -o ext_c11 2>&1

# Con -std=c11 -Wpedantic:
gcc -std=c11 -Wpedantic extensions.c -o ext_pedantic 2>&1

rm -f ext_default ext_c11 ext_pedantic
```

**Predicción**: ¿cuántos warnings produce cada compilación? ¿El paso 2 (sin `-Wpedantic`) produce algún warning?

<details><summary>Respuesta</summary>

- **Sin flags**: 0 warnings, 0 errores. GCC acepta todo silenciosamente con su default (`gnu23` en GCC 15).
- **`-std=c11` sin `-Wpedantic`**: **0 warnings**. Sorprendente pero cierto — `-std=c11` cambia qué macros se definen y qué headers exponen, pero sin `-Wpedantic` no avisa sobre extensiones.
- **`-std=c11 -Wpedantic`**: **5 warnings**, uno por cada extensión:
  1. Statement expression (braced-groups within expressions)
  2. Nested function
  3. Binary literal (C23 feature or GCC extension)
  4. Zero-size array
  5. Case range

En los tres casos el programa compila y funciona. Los warnings no impiden la compilación.

</details>

### Ejercicio 2 — C99 features rechazadas en C89

Compila `labs/c99_features.c` con `-std=c89` y observa qué rechaza:

```bash
cd labs/

# Intentar compilar como C89:
gcc -std=c89 -Wpedantic c99_features.c -o c99_as_c89 2>&1

# Ahora compilar como C99:
gcc -std=c99 c99_features.c -o c99_ok && ./c99_ok

rm -f c99_as_c89 c99_ok
```

**Predicción**: ¿la compilación con `-std=c89` falla con errores o solo da warnings? ¿Cuáles de las 6 features de C99 son rechazadas como errores (no warnings)?

<details><summary>Respuesta</summary>

La compilación **falla**. De las 6 features:

- **Errores** (impiden compilación):
  - `//` comments: `error: C++ style comments are not allowed in ISO C90`
  - Declaraciones en `for`: `error: 'for' loop initial declarations are only allowed in C99 or C11 mode`

- **Warnings** (no impiden compilación):
  - Mixed declarations: `warning: ISO C90 forbids mixed declarations and code`
  - VLA: `warning: ISO C90 forbids variable length array`
  - Designated initializers: `warning: ISO C90 forbids specifying subobject to initialize`

Con `-std=c99` compila limpio. `__STDC_VERSION__ = 199901L`.

</details>

### Ejercicio 3 — C11 features como extensiones en C99

Compila `labs/c11_features.c` con `-std=c99` y luego con `-std=c11`:

```bash
cd labs/

# Como C99 SIN -Wpedantic:
gcc -std=c99 c11_features.c -o c11_as_c99_quiet 2>&1
echo "--- sin Wpedantic: silencio ---"

# Como C99 CON -Wpedantic:
gcc -std=c99 -Wpedantic c11_features.c -o c11_as_c99 2>&1

# Como C11:
gcc -std=c11 c11_features.c -o c11_ok && ./c11_ok

rm -f c11_as_c99_quiet c11_as_c99 c11_ok
```

**Predicción**: ¿compila `c11_features.c` con `-std=c99` (sin `-Wpedantic`)? ¿Cuántos warnings produce con `-Wpedantic`?

<details><summary>Respuesta</summary>

- **`-std=c99` sin `-Wpedantic`**: compila **sin warnings**. GCC acepta features de C11 como extensiones incluso en modo C99.
- **`-std=c99 -Wpedantic`**: compila con **múltiples warnings** sobre features no C99:
  - `_Noreturn` no es C99
  - Anonymous structs/unions no son C99
  - `_Generic` no es C99
  - `_Alignof` no es C99
- **`-std=c11`**: compila limpio, 0 warnings. Todas las features son C11 estándar.

Esto demuestra el patrón: GCC acepta features de estándares posteriores como extensiones, pero solo `-Wpedantic` lo señala.

</details>

### Ejercicio 4 — Los tres archivos con flags del curso

Compila los tres archivos del lab con los flags recomendados del curso y verifica cuáles pasan:

```bash
cd labs/

echo "=== extensions.c ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g extensions.c -o ext 2>&1
echo ""

echo "=== c99_features.c ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g c99_features.c -o c99 2>&1
echo ""

echo "=== c11_features.c ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g c11_features.c -o c11 2>&1

rm -f ext c99 c11
```

**Predicción**: ¿cuáles de los tres archivos compilan sin warnings con los flags del curso?

<details><summary>Respuesta</summary>

- **`extensions.c`**: 5 warnings (extensiones GNU no son C11 estándar)
- **`c99_features.c`**: **0 warnings** — C99 es un subconjunto de C11, todo lo que es C99 válido es C11 válido
- **`c11_features.c`**: **0 warnings** — features de C11 son exactamente lo que el estándar espera

El criterio del curso: si compila limpio con `-std=c11 -Wpedantic`, el código es portable a cualquier compilador que soporte C11.

</details>

### Ejercicio 5 — Feature test macros: `_POSIX_C_SOURCE`

Este programa usa `popen()`, que es POSIX, no C estándar:

```c
// posix_test.c
#include <stdio.h>

int main(void) {
    FILE *f = popen("echo 'hello from popen'", "r");
    if (!f) return 1;

    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
    }
    pclose(f);
    return 0;
}
```

```bash
# Paso 1: compilar con -std=c11 (C estricto):
gcc -std=c11 -Wall -Wextra -Wpedantic posix_test.c -o posix_test 2>&1

# Paso 2: agregar _POSIX_C_SOURCE al inicio del archivo y recompilar
# (añadir: #define _POSIX_C_SOURCE 200809L ANTES del #include)

rm -f posix_test
```

**Predicción**: ¿el paso 1 compila? ¿Qué tipo de error/warning da?

<details><summary>Respuesta</summary>

El paso 1 produce warnings (con `-std=c11`):

```
warning: implicit declaration of function 'popen' [-Wimplicit-function-declaration]
warning: implicit declaration of function 'pclose' [-Wimplicit-function-declaration]
```

Con `-std=c11`, los headers solo exponen funciones del estándar C. `popen()` y `pclose()` son POSIX, no C. El compilador no las ve declaradas.

Solución — agregar antes de cualquier `#include`:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
```

Esto le dice a los headers que expongan funciones POSIX. Ahora compila sin warnings.

</details>

### Ejercicio 6 — `_GNU_SOURCE` vs `_POSIX_C_SOURCE`

```c
// gnu_source_test.c
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "/home/user/documents/file.txt";

    // basename() está en <string.h> con _GNU_SOURCE
    // pero NO con _POSIX_C_SOURCE
    // (la versión POSIX está en <libgen.h>)

    char *base = basename(path);
    printf("basename: %s\n", base);

    return 0;
}
```

```bash
# Intentar con -std=c11:
gcc -std=c11 -Wall -Wextra gnu_source_test.c -o gnu_test 2>&1
echo "---"

# Con -std=gnu11 (extensiones habilitadas):
gcc -std=gnu11 -Wall -Wextra gnu_source_test.c -o gnu_test 2>&1
echo "---"

rm -f gnu_test
```

**Predicción**: ¿por qué falla con `-std=c11`? ¿Con `-std=gnu11` funciona?

<details><summary>Respuesta</summary>

- Con `-std=c11`: `basename()` en `<string.h>` es una extensión GNU. Sin `_GNU_SOURCE` ni modo gnu*, el header no la expone. Se obtiene un warning de declaración implícita.
- Con `-std=gnu11`: funciona porque el modo `gnu*` define `_GNU_SOURCE` implícitamente (o equivalente), exponiendo todas las extensiones en los headers.

La versión portable de `basename()` está en `<libgen.h>` (POSIX), no en `<string.h>`. La versión de `<string.h>` es específica de GNU y modifica el string original de forma diferente.

</details>

### Ejercicio 7 — Implementation-defined: signo de `char`

```c
// char_sign.c
#include <stdio.h>
#include <limits.h>

int main(void) {
    char c = CHAR_MAX;
    printf("CHAR_MIN = %d\n", CHAR_MIN);
    printf("CHAR_MAX = %d\n", CHAR_MAX);
    printf("CHAR_BIT = %d\n", CHAR_BIT);

    if (CHAR_MIN < 0) {
        printf("char es SIGNED en esta plataforma\n");
        printf("  Rango: %d a %d\n", CHAR_MIN, CHAR_MAX);
    } else {
        printf("char es UNSIGNED en esta plataforma\n");
        printf("  Rango: %d a %d\n", CHAR_MIN, CHAR_MAX);
    }

    // Forzar explícitamente:
    signed char   sc = -1;
    unsigned char uc = 255;
    printf("\nsigned char:   %d\n", sc);
    printf("unsigned char: %u\n", uc);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic char_sign.c -o char_sign && ./char_sign
rm -f char_sign
```

**Predicción**: ¿`char` es signed o unsigned en x86-64 Linux?

<details><summary>Respuesta</summary>

En x86-64 Linux (GCC y Clang), `char` es **signed**:

```
CHAR_MIN = -128
CHAR_MAX = 127
CHAR_BIT = 8
char es SIGNED en esta plataforma
  Rango: -128 a 127

signed char:   -1
unsigned char: 255
```

Esto es **implementation-defined**: el compilador elige y lo documenta. En ARM Linux, `char` suele ser **unsigned** (rango 0–255). Por eso, código portable usa `signed char` o `unsigned char` explícitamente cuando el signo importa.

</details>

### Ejercicio 8 — Unspecified: orden de evaluación de argumentos

```c
// eval_order.c
#include <stdio.h>

int counter = 0;

int next(void) {
    return ++counter;
}

int main(void) {
    // ¿En qué orden se evalúan los argumentos?
    printf("a=%d b=%d c=%d\n", next(), next(), next());

    // Forma segura: evaluar primero, luego imprimir
    counter = 0;
    int a = next();
    int b = next();
    int c = next();
    printf("a=%d b=%d c=%d\n", a, b, c);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic eval_order.c -o eval_order && ./eval_order
rm -f eval_order
```

**Predicción**: ¿el primer `printf` imprime `a=1 b=2 c=3`? ¿Y el segundo?

<details><summary>Respuesta</summary>

El **segundo** `printf` siempre imprime `a=1 b=2 c=3` (el orden es explícito).

El **primer** `printf` puede imprimir `a=1 b=2 c=3` o `a=3 b=2 c=1` u otra permutación. El orden de evaluación de argumentos de función es **unspecified** — el compilador puede evalarlos en cualquier orden. En la práctica, GCC en x86-64 suele evaluarlos de derecha a izquierda (imprime `a=3 b=2 c=1`), pero esto no está garantizado y puede cambiar con flags de optimización.

Regla: **nunca dependas del orden de evaluación de argumentos**. Si el orden importa, evalúa en variables separadas primero.

</details>

### Ejercicio 9 — `__STDC_VERSION__` entre estándares

```c
// stdc_version.c
#include <stdio.h>

int main(void) {
    printf("Compilado con: ");

#if !defined(__STDC_VERSION__)
    printf("C89/C90 (pre-C99)\n");
#elif __STDC_VERSION__ == 199901L
    printf("C99\n");
#elif __STDC_VERSION__ == 201112L
    printf("C11\n");
#elif __STDC_VERSION__ == 201710L
    printf("C17\n");
#elif __STDC_VERSION__ == 202311L
    printf("C23\n");
#else
    printf("Desconocido (%ldL)\n", __STDC_VERSION__);
#endif

    printf("__STDC_VERSION__ = ");
#ifdef __STDC_VERSION__
    printf("%ldL\n", __STDC_VERSION__);
#else
    printf("(no definido)\n");
#endif

    return 0;
}
```

```bash
echo "=== -std=c89 ==="
gcc -std=c89 stdc_version.c -o sv && ./sv

echo "=== -std=c99 ==="
gcc -std=c99 stdc_version.c -o sv && ./sv

echo "=== -std=c11 ==="
gcc -std=c11 stdc_version.c -o sv && ./sv

echo "=== -std=c17 ==="
gcc -std=c17 stdc_version.c -o sv && ./sv

echo "=== default (sin -std) ==="
gcc stdc_version.c -o sv && ./sv

rm -f sv
```

**Predicción**: ¿qué imprime en cada caso? ¿El default (sin `-std`) es C17 o C23?

<details><summary>Respuesta</summary>

```
=== -std=c89 ===
Compilado con: C89/C90 (pre-C99)
__STDC_VERSION__ = (no definido)

=== -std=c99 ===
Compilado con: C99
__STDC_VERSION__ = 199901L

=== -std=c11 ===
Compilado con: C11
__STDC_VERSION__ = 201112L

=== -std=c17 ===
Compilado con: C17
__STDC_VERSION__ = 201710L

=== default (sin -std) ===
Compilado con: C23
__STDC_VERSION__ = 202311L
```

En GCC 15, el default es `gnu23` → `__STDC_VERSION__ = 202311L`. En C89, `__STDC_VERSION__` no está definido (se introdujo en C95). Cada valor identifica unívocamente el estándar.

</details>

### Ejercicio 10 — Confirmar portabilidad con los flags del curso

Escribe un programa que use solo features de C11 (no extensiones GNU) y verifica que compila limpio:

```c
// portable_c11.c
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdalign.h>

// _Static_assert (C11)
static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");

// _Generic (C11)
#define describe(x) _Generic((x), \
    int:     "integer",            \
    double:  "floating-point",     \
    char*:   "string",             \
    default: "other"               \
)

int main(void) {
    // Anonymous struct/union (C11)
    struct Point {
        union {
            struct { float x, y; };
            float coords[2];
        };
    };

    struct Point p = { .x = 3.0f, .y = 4.0f };

    // alignof (C11)
    printf("alignof(struct Point) = %zu\n", alignof(struct Point));

    // _Generic
    int n = 42;
    double d = 3.14;
    printf("n is %s\n", describe(n));
    printf("d is %s\n", describe(d));

    // Anonymous struct access
    printf("p.x=%.1f, p.coords[0]=%.1f\n", p.x, p.coords[0]);

    printf("__STDC_VERSION__ = %ldL\n", __STDC_VERSION__);

    return 0;
}
```

```bash
# Compilar con los flags exactos del curso:
gcc -Wall -Wextra -Wpedantic -std=c11 -g portable_c11.c -o portable_c11 && ./portable_c11

rm -f portable_c11
```

**Predicción**: ¿compila sin warnings? ¿Qué `__STDC_VERSION__` imprime?

<details><summary>Respuesta</summary>

Compila con **0 warnings**. Todo es C11 estándar puro — sin extensiones GNU.

```
alignof(struct Point) = 4
n is integer
d is floating-point
p.x=3.0, p.coords[0]=3.0
__STDC_VERSION__ = 201112L
```

`__STDC_VERSION__ = 201112L` porque se compiló con `-std=c11`. Si compilas con `-std=c11 -Wpedantic` y obtienes 0 warnings, tienes la garantía de que el código compila en cualquier compilador conforme a C11. Este es el objetivo del curso.

</details>
