# T03 — Comportamiento por compilador

## GCC no compila C estándar por defecto

Cuando ejecutas `gcc main.c`, GCC **no** usa C estándar estricto.
Usa `-std=gnu17`, que es C17 más extensiones GNU:

```bash
# Lo que PARECE que haces:
gcc main.c -o main            # "compilar C"

# Lo que REALMENTE hace GCC:
gcc -std=gnu17 main.c -o main  # C17 + extensiones GNU

# Para compilar C11 estricto:
gcc -std=c11 main.c -o main

# Para que te avise si usas extensiones:
gcc -std=c11 -Wpedantic main.c -o main
```

## -std=c11 vs -std=gnu11

La diferencia es si GCC permite sus extensiones propias o no:

```
-std=c11:
  Solo acepta C11 estándar.
  Con -Wpedantic, advierte si usas extensiones.
  El código compila en cualquier compilador que soporte C11.

-std=gnu11:
  C11 + extensiones de GCC.
  Permite features no estándar.
  El código puede no compilar en otros compiladores.
```

### Extensiones GNU que -std=gnu11 permite

```c
// 1. typeof (estandarizado en C23):
typeof(42) x = 100;           // extensión GNU en C11
// Con -std=c11 -Wpedantic:
// warning: ISO C does not support 'typeof'

// 2. Statement expressions:
int max = ({ int a = 5, b = 3; a > b ? a : b; });
// Un bloque que retorna un valor — no existe en C estándar

// 3. Nested functions:
void outer(void) {
    void inner(void) {         // función dentro de función
        printf("inner\n");
    }
    inner();
}
// No existe en C estándar. No existe en Clang.

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
// En estándar hay que listar cada case

// 6. Binary literals (estandarizados en C23):
int mask = 0b11110000;         // extensión GNU en C11
// Con -std=c11 -Wpedantic:
// warning: binary constants are a C23 feature or GCC extension

// 7. __attribute__:
void die(void) __attribute__((noreturn));
void *alloc(size_t n) __attribute__((malloc));
int printf_like(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// En C estándar: _Noreturn (C11), [[noreturn]] (C23)

// 8. Designated initializer con rangos:
int arr[10] = { [2 ... 5] = 42 };   // extensión GNU
// En estándar: [2] = 42, [3] = 42, [4] = 42, [5] = 42

// 9. Labels as values (computed goto):
void *labels[] = { &&label1, &&label2 };
goto *labels[i];
label1: /* ... */
label2: /* ... */
// Usado en intérpretes para dispatch rápido. No estándar.
```

### Qué cambia en los headers del sistema

```c
// -std=c11 también afecta qué funciones exponen los headers del sistema.

// Con -std=gnu11 (default):
#include <stdio.h>
// Expone: popen(), fileno(), fdopen()... (extensiones POSIX)

// Con -std=c11:
#include <stdio.h>
// Solo expone funciones del estándar C.
// popen(), fileno(), etc. NO están disponibles.

// Para usar funciones POSIX con -std=c11:
#define _POSIX_C_SOURCE 200809L   // ANTES de cualquier #include
#include <stdio.h>
// Ahora popen() está disponible

// O para todo GNU:
#define _GNU_SOURCE               // expone todo
#include <stdio.h>
```

```c
// Feature test macros comunes:
#define _POSIX_C_SOURCE 200809L   // POSIX.1-2008
#define _XOPEN_SOURCE 700         // X/Open + POSIX
#define _GNU_SOURCE               // todo lo anterior + extensiones GNU
#define _DEFAULT_SOURCE           // equivalente al default de glibc

// Estas macros DEBEN ir antes de cualquier #include.
// Controlan qué funciones son visibles en los headers.
```

## GCC vs Clang

GCC y Clang son los dos compiladores principales en Linux.
Ambos soportan C11, pero tienen diferencias:

```bash
# Compilar con GCC:
gcc -std=c11 -Wall -Wextra main.c -o main

# Compilar con Clang:
clang -std=c11 -Wall -Wextra main.c -o main

# En la mayoría de los casos, el código compila igual.
```

### Diferencias en warnings

```c
// Los dos compiladores tienen warnings diferentes.
// Clang tiende a dar mensajes más claros:

int x;
printf("%s\n", x);

// GCC:
// warning: format '%s' expects argument of type 'char *',
// but argument 2 has type 'int'

// Clang:
// warning: format specifies type 'char *' but the argument
// has type 'int' [-Wformat]
//     printf("%s\n", x);
//             ~~     ^
//             %d
// Clang muestra la línea, marca el error con ^ y sugiere %d
```

### Diferencias en extensiones

```c
// Nested functions — solo GCC:
void foo(void) {
    void bar(void) { }        // GCC: OK. Clang: ERROR.
}

// Blocks — solo Clang (con -fblocks):
void (^block)(void) = ^{ printf("hello\n"); };
// Clang: OK. GCC: ERROR.

// __attribute__ — ambos, pero con diferencias:
// GCC y Clang soportan la mayoría de __attribute__
// pero cada uno tiene algunos exclusivos.

// VLAs en structs — solo GCC:
struct S {
    int n;
    int data[n];              // GCC extensión. Clang: ERROR.
};
```

### Diferencias en optimización

```c
// GCC y Clang optimizan diferente.
// Un programa con UB puede comportarse diferente según el compilador:

#include <limits.h>

int test(int x) {
    if (x + 1 > x)            // UB si x == INT_MAX
        return 1;
    return 0;
}

// GCC -O2: puede retornar siempre 1 (elimina el check)
// Clang -O2: puede hacer lo mismo, o no
// El resultado es impredecible — es UB
```

### Cuándo importa la diferencia

```
Para este curso (C11 estándar):
  No importa. El código con -std=c11 -Wpedantic
  compila igual en GCC y Clang.

Para proyectos reales:
  Compilar con AMBOS compiladores es buena práctica.
  Cada uno detecta bugs que el otro no ve.
  CI/CD debería tener builds con GCC y Clang.

Para extensiones GNU:
  Si usas extensiones, estás atado a GCC (o Clang para
  las que soporta). Usar -std=c11 evita esta dependencia.
```

## Versiones y defaults de GCC

```bash
# Ver la versión de GCC:
gcc --version

# Defaults por versión de GCC:
# GCC 4.x: -std=gnu89 (default antiguo)
# GCC 5+:  -std=gnu11
# GCC 8+:  -std=gnu17

# Verificar el default actual:
gcc -dM -E - < /dev/null | grep STDC_VERSION
# #define __STDC_VERSION__ 201710L  → gnu17/c17

# Ver qué flags están activados:
gcc -Q --help=warning       # lista de warnings
gcc -v main.c -o main       # compilación verbosa
```

## Implementation-defined behavior

Algunos comportamientos dependen del compilador pero deben ser
consistentes y documentados:

```c
// sizeof de tipos — varía por plataforma, no por compilador:
printf("sizeof(int) = %zu\n", sizeof(int));
// LP64 (Linux 64-bit): 4
// ILP32 (Linux 32-bit): 4
// Ambos GCC y Clang dan el mismo resultado en la misma plataforma

// Signo de char — depende de la plataforma:
char c = 128;
// En x86 (GCC/Clang): char es signed → c = -128
// En ARM (GCC): char puede ser unsigned → c = 128
// Solución: usar signed char o unsigned char explícitamente

// Shift right de negativos:
int x = -1;
int y = x >> 1;
// Implementation-defined: puede ser aritmético o lógico
// GCC/Clang en x86: aritmético (preserva signo)
// Pero no está garantizado por el estándar

// Orden de evaluación de argumentos:
printf("%d %d\n", f(), g());
// ¿Se llama f() o g() primero? No especificado.
// GCC y Clang pueden elegir diferente.
// No depender de un orden específico.
```

## Buenas prácticas para portabilidad

```bash
# 1. Compilar con -std=c11 -Wpedantic:
gcc -std=c11 -Wpedantic main.c -o main
# Detecta cualquier extensión no estándar

# 2. Compilar con ambos compiladores:
gcc -std=c11 -Wall -Wextra main.c -o main_gcc
clang -std=c11 -Wall -Wextra main.c -o main_clang
# Cada uno detecta warnings diferentes

# 3. Usar feature test macros para POSIX:
# #define _POSIX_C_SOURCE 200809L
# Antes de cualquier #include

# 4. Usar stdint.h en lugar de confiar en sizeof(int):
int32_t x;     // siempre 32 bits
uint64_t y;    // siempre 64 bits

# 5. No depender de extensiones GNU:
# Si necesitas typeof → esperar a C23 o usar macros alternativas
# Si necesitas nested functions → refactorizar con punteros a función
```

## Tabla comparativa

| Aspecto | -std=c11 | -std=gnu11 |
|---|---|---|
| typeof | No (warning con -Wpedantic) | Sí |
| Statement expressions | No | Sí |
| Nested functions | No | Sí (solo GCC) |
| Binary literals | No | Sí |
| Zero-length arrays | No | Sí |
| __attribute__ | Sí (no estándar pero tolerado) | Sí |
| POSIX en headers | Necesita feature test macro | Disponible |
| Portabilidad | Alta | Limitada a GCC/Clang |

---

## Ejercicios

### Ejercicio 1 — Extensiones

```c
// Compilar con -std=c11 -Wpedantic y con -std=gnu11.
// ¿Qué warnings aparecen en cada caso?

#include <stdio.h>

int main(void) {
    typeof(42) x = 100;
    int mask = 0b11110000;

    int max = ({ int a = 5, b = 3; a > b ? a : b; });

    printf("%d %d %d\n", x, mask, max);
    return 0;
}
```

### Ejercicio 2 — Feature test macros

```c
// Este código no compila con -std=c11. ¿Por qué?
// ¿Qué se necesita agregar para que compile?

#include <stdio.h>

int main(void) {
    FILE *f = popen("ls", "r");
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        printf("%s", buf);
    }
    pclose(f);
    return 0;
}
```

### Ejercicio 3 — GCC vs Clang

```bash
# Compilar el mismo programa con GCC y Clang.
# Comparar los warnings.
# ¿Alguno detecta algo que el otro no?

gcc -std=c11 -Wall -Wextra main.c -o main_gcc 2> warnings_gcc.txt
clang -std=c11 -Wall -Wextra main.c -o main_clang 2> warnings_clang.txt
diff warnings_gcc.txt warnings_clang.txt
```
