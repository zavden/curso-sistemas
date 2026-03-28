# T03 — AddressSanitizer y UBSan

## Qué son los sanitizers

Los sanitizers son herramientas de detección de errores **integradas
en el compilador**. A diferencia de Valgrind (que ejecuta el programa
en una CPU virtual), los sanitizers instrumentan el código durante
la compilación — agregan verificaciones directamente al binario:

```bash
# Compilar con AddressSanitizer (ASan):
gcc -fsanitize=address -g main.c -o main

# Compilar con UndefinedBehaviorSanitizer (UBSan):
gcc -fsanitize=undefined -g main.c -o main

# Ambos:
gcc -fsanitize=address,undefined -g main.c -o main
```

```
Sanitizers vs Valgrind:
- Sanitizers: instrumentan en compilación, ~2x más lento
- Valgrind: instrumenta en runtime, ~20-30x más lento
- Sanitizers detectan errores en stack Y heap
- Valgrind detecta errores solo en heap (pero mejor para leaks)
- Sanitizers requieren recompilar
- Valgrind funciona con cualquier binario
```

## AddressSanitizer (ASan)

ASan detecta errores de acceso a memoria. Intercepta malloc/free
y agrega "zonas rojas" alrededor de cada bloque para detectar
accesos fuera de límites:

```
Memoria real:          [  datos  ]
Con ASan:     [red zone][  datos  ][red zone]

Si el programa toca una red zone → ASan lo detecta inmediatamente.
```

### Heap buffer overflow

```c
// heap_overflow.c
#include <stdlib.h>

int main(void) {
    int *arr = malloc(5 * sizeof(int));
    arr[5] = 42;               // 1 posición fuera de límites
    free(arr);
    return 0;
}
```

```bash
gcc -fsanitize=address -g heap_overflow.c -o heap_overflow
./heap_overflow
```

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x603000000054
WRITE of size 4 at 0x603000000054 thread T0
    #0 0x401180 in main heap_overflow.c:5
    #1 0x7f... in __libc_start_main

0x603000000054 is located 0 bytes after 20-byte region [0x603000000040,0x603000000054)
allocated by thread T0 here:
    #0 0x7f... in malloc
    #1 0x401160 in main heap_overflow.c:4
```

```
Cómo leer el reporte de ASan:
1. Tipo de error: "heap-buffer-overflow"
2. Operación: "WRITE of size 4" (escribió 4 bytes)
3. Dónde: "heap_overflow.c:5"
4. Contexto: "0 bytes after 20-byte region" (justo después del bloque)
5. Dónde se allocó: "heap_overflow.c:4"
```

### Stack buffer overflow

**Esto es lo que Valgrind NO detecta pero ASan sí:**

```c
// stack_overflow.c
#include <stdio.h>

int main(void) {
    int arr[5] = {1, 2, 3, 4, 5};
    printf("%d\n", arr[10]);   // acceso fuera de límites en stack
    return 0;
}
```

```bash
gcc -fsanitize=address -g stack_overflow.c -o stack_overflow
./stack_overflow
```

```
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ffd12345698
READ of size 4 at 0x7ffd12345698 thread T0
    #0 0x401190 in main stack_overflow.c:5

Address 0x7ffd12345698 is located in stack of thread T0 at offset 56 in frame
    #0 0x401150 in main stack_overflow.c:3

  This frame has 1 object(s):
    [32, 52) 'arr' (line 4)     <== Memory access at offset 56 overflows this variable
```

```
ASan muestra exactamente:
- Qué variable se desbordó ('arr')
- El tamaño real del array ([32, 52) = 20 bytes = 5 ints)
- Cuánto se pasó (offset 56, que está fuera de [32, 52))
```

### Global buffer overflow

```c
// global_overflow.c
#include <stdio.h>

int arr[5] = {1, 2, 3, 4, 5};

int main(void) {
    return arr[5];             // acceso fuera de global
}
```

```
==12345==ERROR: AddressSanitizer: global-buffer-overflow on address 0x404034
READ of size 4 at 0x404034 thread T0
    #0 0x401130 in main global_overflow.c:6

0x404034 is located 0 bytes after global variable 'arr'
    defined at 'global_overflow.c:3:5' (0x404020) of size 20
```

### Use-after-free

```c
// uaf.c
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    *p = 42;
    free(p);
    return *p;                 // uso después de free
}
```

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010
READ of size 4 at 0x602000000010 thread T0
    #0 0x401170 in main uaf.c:7

0x602000000010 is located 0 bytes inside of 4-byte region
freed by thread T0 here:
    #0 0x7f... in free
    #1 0x401160 in main uaf.c:6

previously allocated by thread T0 here:
    #0 0x7f... in malloc
    #1 0x401140 in main uaf.c:4
```

### Double free

```c
// double_free.c
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    free(p);
    free(p);                   // double free
    return 0;
}
```

```
==12345==ERROR: AddressSanitizer: attempting double-free on 0x602000000010
    #0 0x7f... in free
    #1 0x401160 in main double_free.c:6

0x602000000010 is located 0 bytes inside of 4-byte region
freed by thread T0 here:
    #0 0x7f... in free
    #1 0x401150 in main double_free.c:5
```

### Use-after-return

```c
// uar.c
#include <stdio.h>

int *bad(void) {
    int x = 42;
    return &x;                 // puntero a variable local
}

int main(void) {
    int *p = bad();
    printf("%d\n", *p);        // la variable ya no existe
    return 0;
}
```

```bash
# ASan necesita un flag extra para detectar use-after-return:
ASAN_OPTIONS=detect_stack_use_after_return=1 ./uar
```

### Detección de leaks con ASan

```bash
# ASan incluye LeakSanitizer (LSan) por defecto en Linux:
gcc -fsanitize=address -g main.c -o main
./main

# Al terminar, si hay leaks:
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 40 bytes in 1 object(s) allocated from:
    #0 0x7f... in malloc
    #1 0x401150 in main leak.c:4

SUMMARY: AddressSanitizer: 40 byte(s) leaked in 1 allocation(s).
```

```bash
# Controlar el comportamiento de leak detection:
ASAN_OPTIONS=detect_leaks=0 ./main     # desactivar
ASAN_OPTIONS=detect_leaks=1 ./main     # activar (default)
```

## UndefinedBehaviorSanitizer (UBSan)

UBSan detecta comportamiento indefinido en runtime. Instrumenta
operaciones que pueden ser UB y verifica en cada ejecución:

```bash
gcc -fsanitize=undefined -g main.c -o main
```

### Signed integer overflow

```c
// overflow.c
#include <limits.h>
#include <stdio.h>

int main(void) {
    int x = INT_MAX;
    int y = x + 1;             // signed overflow = UB
    printf("%d\n", y);
    return 0;
}
```

```bash
gcc -fsanitize=undefined -g overflow.c -o overflow
./overflow
```

```
overflow.c:6:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
```

### División por cero

```c
// divzero.c
#include <stdio.h>

int main(void) {
    int x = 42;
    int y = 0;
    printf("%d\n", x / y);    // división por cero
    return 0;
}
```

```
divzero.c:6:22: runtime error: division by zero
```

### Shift inválido

```c
// shift.c
#include <stdio.h>

int main(void) {
    int x = 1;
    int y = x << 32;          // shift >= ancho del tipo
    int z = x << -1;          // shift negativo
    printf("%d %d\n", y, z);
    return 0;
}
```

```
shift.c:5:17: runtime error: shift exponent 32 is too large
    for 32-bit type 'int'
shift.c:6:17: runtime error: shift exponent -1 is negative
```

### Null pointer dereference

```c
// null.c
#include <stdio.h>

int main(void) {
    int *p = (void *)0;
    printf("%d\n", *p);       // null deref
    return 0;
}
```

```
null.c:5:20: runtime error: load of null pointer of type 'int'
```

### Acceso a array fuera de límites (con información de tipo)

```c
// bounds.c
#include <stdio.h>

int main(void) {
    int arr[5];
    return arr[10];            // fuera de límites
}
```

```bash
# -fsanitize=bounds necesita -fsanitize=undefined
# o se puede especificar individualmente:
gcc -fsanitize=bounds -g bounds.c -o bounds
./bounds
```

```
bounds.c:5:12: runtime error: index 10 out of bounds
    for type 'int [5]'
```

### Alignment inválido

```c
// align.c
#include <stdio.h>

int main(void) {
    char buf[8] = {0};
    int *p = (int *)(buf + 1);  // dirección no alineada para int
    printf("%d\n", *p);         // acceso desalineado
    return 0;
}
```

```
align.c:6:20: runtime error: load of misaligned address
    0x7ffd12345679 for type 'int', which requires 4 byte alignment
```

### Bool inválido

```c
// bool.c
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    bool b;
    memset(&b, 42, sizeof(b));  // valor no válido para bool
    if (b) {                     // uso de bool con valor inválido
        printf("true\n");
    }
    return 0;
}
```

```
bool.c:8:9: runtime error: load of value 42, which is not
    a valid value for type 'bool'
```

## Sub-sanitizers de UBSan

```bash
# -fsanitize=undefined activa todos estos:
#   signed-integer-overflow
#   unsigned-integer-overflow (como warning, no UB técnicamente)
#   float-divide-by-zero
#   float-cast-overflow
#   shift
#   unreachable
#   return (función non-void sin return)
#   null
#   alignment
#   bounds
#   vla-bound (VLA con tamaño negativo)
#   bool
#   enum

# Activar uno específico:
gcc -fsanitize=signed-integer-overflow -g main.c -o main

# Combinar varios:
gcc -fsanitize=signed-integer-overflow,null,bounds -g main.c -o main
```

## Combinando ASan + UBSan

```bash
# La combinación recomendada para desarrollo:
gcc -fsanitize=address,undefined -g -O0 main.c -o main

# Esto detecta:
# - Errores de memoria (ASan): heap/stack/global overflow,
#   use-after-free, double free, leaks
# - Comportamiento indefinido (UBSan): overflow, shift,
#   null deref, bounds, alignment
```

```c
// all_bugs.c — programa con errores de ambos tipos
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    // UBSan detecta esto:
    int x = INT_MAX;
    x = x + 1;                // signed overflow

    // ASan detecta esto:
    int *arr = malloc(5 * sizeof(int));
    arr[5] = 42;               // heap buffer overflow

    free(arr);
    return 0;
}
```

```bash
gcc -fsanitize=address,undefined -g all_bugs.c -o all_bugs
./all_bugs

# UBSan reporta el overflow primero (línea 8)
# Luego ASan reporta el heap overflow (línea 12)
# El programa aborta al primer error de ASan
```

## Opciones de runtime

```bash
# Las opciones se controlan con variables de entorno:

# ASan:
ASAN_OPTIONS="option1=value:option2=value" ./main

# Opciones útiles de ASan:
ASAN_OPTIONS="halt_on_error=0" ./main          # no parar al primer error
ASAN_OPTIONS="detect_leaks=1" ./main           # detectar leaks (default)
ASAN_OPTIONS="detect_stack_use_after_return=1" ./main  # use-after-return
ASAN_OPTIONS="check_initialization_order=1" ./main     # orden de init

# UBSan:
UBSAN_OPTIONS="option1=value:option2=value" ./main

# Opciones útiles de UBSan:
UBSAN_OPTIONS="halt_on_error=1" ./main         # abortar al primer error
UBSAN_OPTIONS="print_stacktrace=1" ./main      # incluir stack trace
# (por defecto UBSan solo imprime el warning y continúa)
```

```bash
# Hacer que UBSan aborte en errores (como ASan):
gcc -fsanitize=undefined -fno-sanitize-recover=all -g main.c -o main
# -fno-sanitize-recover=all → abortar en cualquier UBSan error
# Sin esto, UBSan solo imprime un warning y sigue ejecutando
```

## Sanitizers que NO se pueden combinar

```bash
# ASan + TSan (ThreadSanitizer) NO se pueden usar juntos:
# gcc -fsanitize=address,thread ...    ← ERROR

# ASan + MSan (MemorySanitizer) NO se pueden usar juntos:
# gcc -fsanitize=address,memory ...    ← ERROR

# Combinaciones válidas:
# ASan + UBSan        ← RECOMENDADO para desarrollo
# TSan solo           ← para detectar data races
# MSan solo           ← para detectar uso de memoria sin inicializar
#                       (similar a Valgrind pero más rápido)
```

## Cuándo usar cada herramienta

```
| Herramienta | Qué detecta | Overhead | Requiere recompilar |
|-------------|-------------|----------|---------------------|
| ASan | Errores de memoria | ~2x | Sí |
| UBSan | Comportamiento indefinido | ~1.2x | Sí |
| Valgrind | Errores de memoria + leaks | ~20-30x | No |
| MSan | Uso de memoria sin inicializar | ~3x | Sí |
| TSan | Data races en threads | ~5-15x | Sí |
```

```
Cuándo usar Valgrind en lugar de ASan:
- No puedes recompilar (solo tienes el binario)
- Necesitas un análisis detallado de leaks (categorías)
- Quieres verificar un programa sin instrumentar
- Necesitas detectar mismatched alloc/dealloc (malloc/delete)

Cuándo usar ASan en lugar de Valgrind:
- Necesitas detectar errores en el stack (arrays locales)
- La velocidad importa (2x vs 20x)
- El programa usa threads intensivamente
- Quieres detección en CI/CD (menos overhead)
- Necesitas detectar global buffer overflows
```

## Sets recomendados

```bash
# DESARROLLO (máxima detección):
gcc -Wall -Wextra -Wpedantic -std=c11 \
    -g -O0 \
    -fsanitize=address,undefined \
    -fno-sanitize-recover=all \
    main.c -o main

# CI/CD (detección + velocidad razonable):
gcc -Wall -Wextra -Werror -std=c11 \
    -g -O1 \
    -fsanitize=address,undefined \
    main.c -o main

# PRODUCCIÓN (sin sanitizers):
gcc -Wall -Wextra -std=c11 \
    -O2 -DNDEBUG \
    main.c -o main

# DETECCIÓN DE RACES (threads):
gcc -Wall -Wextra -std=c11 \
    -g -O1 \
    -fsanitize=thread \
    main.c -o main -lpthread
```

## Tabla de errores detectados

| Error | ASan | UBSan | Valgrind |
|---|---|---|---|
| Heap buffer overflow | Sí | — | Sí |
| Stack buffer overflow | Sí | — | No |
| Global buffer overflow | Sí | — | No |
| Use-after-free | Sí | — | Sí |
| Double free | Sí | — | Sí |
| Memory leaks | Sí (LSan) | — | Sí (detallado) |
| Use-after-return | Sí (con opción) | — | No |
| Signed overflow | — | Sí | No |
| Shift inválido | — | Sí | No |
| División por cero | — | Sí | No |
| Null dereference | — | Sí | No |
| Alignment inválido | — | Sí | No |
| Valor sin inicializar | No (usar MSan) | — | Sí |

---

## Ejercicios

### Ejercicio 1 — ASan vs Valgrind

```c
// Compilar este programa:
// 1. Con -fsanitize=address y ejecutar
// 2. Sin sanitizer, ejecutar con valgrind
// ¿Ambos detectan el error? ¿Qué diferencias ves en el reporte?

#include <stdio.h>

int main(void) {
    int arr[10];
    for (int i = 0; i <= 10; i++) {
        arr[i] = i;
    }
    printf("%d\n", arr[0]);
    return 0;
}
```

### Ejercicio 2 — UBSan

```c
// Compilar con -fsanitize=undefined -fno-sanitize-recover=all
// ¿Cuántos errores de UB tiene este programa?

#include <limits.h>
#include <stdio.h>

int main(void) {
    int a = INT_MAX;
    int b = a + 1;

    int c = 1;
    int d = c << 32;

    int e = 10;
    int f = e / 0;

    int arr[5];
    int g = arr[10];

    printf("%d %d %d %d\n", b, d, f, g);
    return 0;
}
```

### Ejercicio 3 — Programa limpio

```bash
# Escribir un programa que:
# 1. Alloce memoria dinámica
# 2. Use aritmética con enteros
# 3. Tenga al menos una función con punteros
# 4. Libere toda la memoria
#
# Compilar con: gcc -fsanitize=address,undefined -g -O0
# El objetivo: 0 errores de ASan, 0 errores de UBSan, 0 leaks
```
