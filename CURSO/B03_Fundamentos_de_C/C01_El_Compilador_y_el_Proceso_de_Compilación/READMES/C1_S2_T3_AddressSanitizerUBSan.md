# T03 — AddressSanitizer y UBSan

> **Errata del material base**
>
> | Ubicación | Error | Corrección |
> |-----------|-------|------------|
> | `README.md:407` | Lista `unsigned-integer-overflow` como sub-sanitizer activado por `-fsanitize=undefined`, con la nota "(como warning, no UB técnicamente)". | **`unsigned-integer-overflow` NO es parte de `-fsanitize=undefined`** ni en GCC ni en clang. En GCC no existe como check separado. En clang existe como `-fsanitize=unsigned-integer-overflow` pero pertenece al grupo `-fsanitize=integer`, no a `-fsanitize=undefined`. El unsigned overflow es comportamiento **definido** en C (wrapping modular aritmético, §6.2.5/9), por lo que no corresponde a un sanitizer de UB. Eliminar de la lista. |

---

## 1. Qué son los sanitizers

Los sanitizers son herramientas de detección de errores **integradas en el compilador**. A diferencia de Valgrind (CPU virtual), los sanitizers instrumentan el código durante la compilación — agregan verificaciones directamente al binario:

```bash
# AddressSanitizer (ASan):
gcc -fsanitize=address -g main.c -o main

# UndefinedBehaviorSanitizer (UBSan):
gcc -fsanitize=undefined -g main.c -o main

# Ambos:
gcc -fsanitize=address,undefined -g main.c -o main
```

| Aspecto | Sanitizers | Valgrind |
|---------|-----------|----------|
| Instrumentación | En compilación | En runtime (CPU virtual) |
| Overhead | ~2x (ASan), ~1.2x (UBSan) | ~20-30x |
| Errores de stack | **Sí** (ASan) | No |
| Errores de heap | Sí | Sí |
| Leaks | Básico (LSan) | Detallado (categorías) |
| Requiere recompilar | Sí | No |
| Memoria sin inicializar | No (necesita MSan/clang) | Sí |

---

## 2. AddressSanitizer (ASan)

ASan detecta errores de acceso a memoria. Intercepta malloc/free y agrega **"red zones"** alrededor de cada bloque:

```
Memoria real:          [  datos  ]
Con ASan:     [red zone][  datos  ][red zone]

Si el programa toca una red zone → ASan lo detecta inmediatamente.
```

### Heap buffer overflow

```c
int *arr = malloc(5 * sizeof(int));
arr[5] = 42;               // 1 posición fuera de límites
free(arr);
```

```
==PID==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
WRITE of size 4 at 0x... thread T0
    #0 0x... in main prog.c:5
0x... is located 0 bytes after 20-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in main prog.c:4
```

Cómo leer: tipo de error → operación (READ/WRITE) → línea → contexto de la memoria → dónde se asignó.

Ver `labs/heap_errors.c:19-29` — `heap_buffer_overflow()` con `i <= 5` en array de 5.

### Stack buffer overflow — lo que Valgrind NO detecta

```c
int arr[5] = {1, 2, 3, 4, 5};
printf("%d\n", arr[10]);   // acceso fuera de límites en stack
```

```
==PID==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
READ of size 4 at 0x... thread T0
    #0 0x... in main prog.c:5

  This frame has 1 object(s):
    [32, 52) 'arr' (line 4)     <== Memory access at offset 72 overflows this variable
```

ASan identifica la variable exacta (`arr`), su tamaño ([32, 52) = 20 bytes = 5 ints), y cuánto se pasó. **Valgrind no detecta esto** porque solo instrumenta malloc/free, no variables locales.

Ver `labs/stack_errors.c` — stack overflow que Valgrind reporta como "0 errors".

### Global buffer overflow

```c
int arr[5] = {1, 2, 3, 4, 5};  // variable global
int main(void) {
    return arr[5];              // 1 posición fuera del global
}
```

```
==PID==ERROR: AddressSanitizer: global-buffer-overflow on address 0x...
0x... is located 0 bytes after global variable 'arr'
    defined at 'prog.c:3:5' of size 20
```

### Use-after-free

```
==PID==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 4 at 0x... thread T0
    #0 ... in main prog.c:7          ← dónde se usó
freed by thread T0 here:
    #0 ... in free
    #1 ... in main prog.c:6          ← dónde se liberó
previously allocated by thread T0 here:
    #0 ... in malloc
    #1 ... in main prog.c:4          ← dónde se asignó
```

Tres stack traces, igual que Valgrind. Ver `labs/heap_errors.c:32-43`.

### Double free

```
==PID==ERROR: AddressSanitizer: attempting double-free on 0x...
```

Ver `labs/heap_errors.c:46-55`.

### Use-after-return

```c
int *bad(void) {
    int x = 42;
    return &x;                 // puntero a variable local
}
```

```bash
# ASan necesita un flag extra para detectar use-after-return:
ASAN_OPTIONS=detect_stack_use_after_return=1 ./programa
```

### Detección de leaks (LSan)

ASan incluye **LeakSanitizer** por defecto en Linux:

```bash
gcc -fsanitize=address -g main.c -o main
./main
# Al terminar, si hay leaks:
# ==PID==ERROR: LeakSanitizer: detected memory leaks
# Direct leak of 40 bytes in 1 object(s) allocated from: ...

# Controlar:
ASAN_OPTIONS="detect_leaks=0" ./main     # desactivar
ASAN_OPTIONS="detect_leaks=1" ./main     # activar (default)
```

---

## 3. UndefinedBehaviorSanitizer (UBSan)

UBSan detecta comportamiento indefinido en runtime. Instrumenta operaciones que pueden ser UB y verifica en cada ejecución:

```bash
gcc -fsanitize=undefined -g main.c -o main
```

### Signed integer overflow

```c
int x = INT_MAX;
int y = x + 1;             // UB
```

```
prog.c:6:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
```

Ver `labs/ub_errors.c:22-27` — `signed_overflow()`.

### División por cero

```c
printf("%d\n", 42 / 0);    // UB
```

```
prog.c:6:22: runtime error: division by zero
```

Puede ir seguido de SIGFPE (señal de hardware). Ver `labs/ub_errors.c:30-34`.

### Shift inválido

```c
int y = 1 << 32;            // shift >= ancho del tipo
int z = 1 << -1;            // shift negativo
```

```
prog.c:5:17: runtime error: shift exponent 32 is too large for 32-bit type 'int'
prog.c:6:17: runtime error: shift exponent -1 is negative
```

Ver `labs/ub_errors.c:37-45`.

### Null pointer dereference

```c
int *p = NULL;
printf("%d\n", *p);        // UB
```

```
prog.c:5:20: runtime error: load of null pointer of type 'int'
```

Ver `labs/ub_errors.c:48-51`.

### Acceso fuera de límites (con tipo conocido)

```
prog.c:5:12: runtime error: index 10 out of bounds for type 'int [5]'
```

### Alignment inválido

```c
char buf[8] = {0};
int *p = (int *)(buf + 1);  // dirección no alineada para int
printf("%d\n", *p);
```

```
prog.c:6:20: runtime error: load of misaligned address 0x... for type 'int',
    which requires 4 byte alignment
```

### Bool inválido

```c
bool b;
memset(&b, 42, sizeof(b));  // valor no válido para bool
if (b) { ... }
```

```
prog.c:8:9: runtime error: load of value 42, which is not a valid value for type 'bool'
```

---

## 4. Sub-sanitizers de -fsanitize=undefined

`-fsanitize=undefined` activa estos checks (GCC):

| Sub-sanitizer | Qué detecta |
|---------------|-------------|
| `signed-integer-overflow` | Overflow de enteros con signo |
| `float-divide-by-zero` | División flotante por cero |
| `float-cast-overflow` | Cast de float fuera de rango del tipo destino |
| `shift` | Shift >= ancho del tipo o negativo |
| `unreachable` | `__builtin_unreachable()` alcanzado |
| `return` | Función non-void que no retorna valor |
| `null` | Desreferencia de null |
| `alignment` | Acceso desalineado |
| `bounds` | Índice fuera de límites (arrays con tamaño conocido) |
| `vla-bound` | VLA con tamaño negativo o cero |
| `bool` | Valor inválido para `bool` |
| `enum` | Valor fuera de rango para enum |
| `integer-divide-by-zero` | División entera por cero |
| `nonnull-attribute` | NULL pasado a parámetro marcado `__attribute__((nonnull))` |
| `object-size` | Acceso a objeto cuyo tamaño el compilador conoce |
| `pointer-overflow` | Aritmética de punteros con overflow |

```bash
# Activar uno específico:
gcc -fsanitize=signed-integer-overflow -g main.c -o main

# Combinar varios:
gcc -fsanitize=signed-integer-overflow,null,bounds -g main.c -o main
```

---

## 5. Comportamiento de UBSan: warn vs abort

Por defecto, UBSan **imprime un warning y continúa ejecutando**. Esto puede ser útil (muestra múltiples errores) o peligroso (el programa sigue con estado corrupto).

```bash
# Hacer que UBSan aborte al primer error:
gcc -fsanitize=undefined -fno-sanitize-recover=all -g main.c -o main

# O vía variable de entorno:
UBSAN_OPTIONS="halt_on_error=1" ./main

# Incluir stack trace en warnings de UBSan:
UBSAN_OPTIONS="print_stacktrace=1" ./main
```

Contraste con ASan: ASan **siempre aborta** al primer error de memoria.

---

## 6. Combinando ASan + UBSan

```bash
gcc -fsanitize=address,undefined -g -O0 main.c -o main
```

Detecta errores de memoria (ASan) Y comportamiento indefinido (UBSan) en una sola ejecución. UBSan reporta primero (warnings), ASan aborta al primer error de memoria.

Ver `labs/combined.c` — signed overflow (UBSan) + heap overflow (ASan) en un solo programa.

### Sanitizers incompatibles

```bash
# ASan + TSan → ERROR (no se pueden combinar)
# ASan + MSan → ERROR (no se pueden combinar)
# TSan + MSan → ERROR

# Combinaciones válidas:
# ASan + UBSan      ← RECOMENDADO para desarrollo
# TSan solo         ← para data races en threads
# MSan solo         ← para memoria sin inicializar (solo clang)
```

---

## 7. Opciones de runtime

```bash
# ASan:
ASAN_OPTIONS="halt_on_error=0" ./main              # no parar al primer error
ASAN_OPTIONS="detect_leaks=1" ./main               # detectar leaks (default)
ASAN_OPTIONS="detect_stack_use_after_return=1" ./main  # use-after-return

# UBSan:
UBSAN_OPTIONS="halt_on_error=1" ./main             # abortar al primer error
UBSAN_OPTIONS="print_stacktrace=1" ./main          # stack trace completo

# Combinar:
ASAN_OPTIONS="halt_on_error=0:detect_leaks=1" \
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1" \
./main
```

---

## 8. Sets de compilación recomendados

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

---

## 9. Tabla comparativa de detección

| Error | ASan | UBSan | Valgrind |
|-------|------|-------|----------|
| Heap buffer overflow | **Sí** | — | **Sí** |
| Stack buffer overflow | **Sí** | — | No |
| Global buffer overflow | **Sí** | — | No |
| Use-after-free | **Sí** | — | **Sí** |
| Double free | **Sí** | — | **Sí** |
| Memory leaks | Sí (LSan, básico) | — | **Sí** (detallado, categorías) |
| Use-after-return | Sí (con opción) | — | No |
| Signed overflow | — | **Sí** | No |
| Shift inválido | — | **Sí** | No |
| División por cero | — | **Sí** | No |
| Null dereference | — | **Sí** | No |
| Alignment inválido | — | **Sí** | No |
| Valor sin inicializar | No (usar MSan) | — | **Sí** |

---

## 10. Archivos del laboratorio

| Archivo | Descripción |
|---------|-------------|
| `labs/heap_errors.c` | 3 bugs de heap seleccionables por argumento: overflow, use-after-free, double-free |
| `labs/stack_errors.c` | Stack buffer overflow — lo que Valgrind no detecta |
| `labs/ub_errors.c` | 4 tipos de UB seleccionables: signed overflow, div/0, shift, null deref |
| `labs/combined.c` | Signed overflow (UBSan) + heap overflow (ASan) combinados |

---

## Ejercicios

### Ejercicio 1 — Heap buffer overflow con ASan

Compila `heap_errors.c` con ASan y ejecuta el caso 1:

```bash
cd labs/
gcc -fsanitize=address -g heap_errors.c -o heap_errors
./heap_errors 1
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué tipo de error reportará ASan? ¿En qué línea? ¿Qué tamaño tiene el bloque?</summary>

ASan reporta **heap-buffer-overflow**:

- **Operación**: `WRITE of size 4` — el loop escribe `arr[5]` (un int de 4 bytes) fuera del array
- **Línea del error**: `heap_errors.c:24` — `arr[i] = i * 10;` cuando `i == 5`
- **Contexto**: "0 bytes after 20-byte region" — el bloque tiene 20 bytes (5 ints × 4), el acceso es justo después
- **Línea del malloc**: `heap_errors.c:20`

El bug es `i <= 5` en el loop — debería ser `i < 5`.

</details>

---

### Ejercicio 2 — Use-after-free y double free

Ejecuta los casos 2 y 3:

```bash
./heap_errors 2
echo "---"
./heap_errors 3
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Cuántas stack traces mostrará ASan para cada error?</summary>

**Use-after-free** (`./heap_errors 2`): **3 stack traces**
1. Dónde se leyó la memoria liberada (línea 42: `printf("after free: %d\n", *p)`)
2. Dónde se liberó (`free(p)` en línea 39)
3. Dónde se asignó (`malloc` en línea 33)

**Double free** (`./heap_errors 3`): **2 stack traces**
1. Dónde ocurrió el segundo free (línea 54)
2. Dónde se hizo el primer free (línea 53)

Para use-after-free, ASan reporta `heap-use-after-free`. Para double free, `attempting double-free`. Ambos abortan inmediatamente — ASan siempre aborta al primer error de memoria.

</details>

---

### Ejercicio 3 — Stack overflow: ASan vs Valgrind

Compila `stack_errors.c` con ASan y sin sanitizer. Ejecuta con ASan, luego el binario sin sanitizer con Valgrind:

```bash
gcc -fsanitize=address -g stack_errors.c -o stack_errors
gcc -g stack_errors.c -o stack_errors_nosanit

./stack_errors
echo "==="
valgrind ./stack_errors_nosanit
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿ASan detectará el error? ¿Y Valgrind? ¿Por qué la diferencia?</summary>

**ASan**: **Sí**, detecta `stack-buffer-overflow`. Reporta la variable exacta (`arr`, línea 17), su tamaño ([32, 52) = 20 bytes), y que el acceso a `arr[8]` cae fuera del rango.

**Valgrind**: **No**, reporta `ERROR SUMMARY: 0 errors from 0 contexts`. No detecta nada.

**La razón**: Valgrind instrumenta `malloc`/`free` — solo tiene visibilidad sobre el heap. Los arrays locales viven en el stack, fuera del alcance de Valgrind. ASan instrumenta el código en **compilación**, poniendo red zones alrededor de variables locales en el stack, por eso sí lo detecta.

**Este es el caso de uso principal de ASan sobre Valgrind**: errores de stack que pasan completamente desapercibidos para Valgrind y para el programador.

</details>

---

### Ejercicio 4 — Sin sanitizer: el silencio peligroso

Ejecuta el mismo `stack_errors_nosanit` (sin ASan, sin Valgrind):

```bash
./stack_errors_nosanit
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿El programa crasheará? ¿Qué valor imprimirá arr[8]?</summary>

El programa probablemente **no crashea**. `arr[8]` lee un valor del stack que pertenece a otra variable o al frame del llamador. Imprimirá un número cualquiera (basura del stack) sin ningún warning.

No hay crash porque `arr[8]` sigue siendo memoria del proceso (el stack es mapeado en un rango grande). El programa "funciona" pero tiene comportamiento indefinido — el valor leído es impredecible y puede cambiar con otra versión del compilador, otro nivel de optimización, o al agregar/quitar variables locales.

En una base de código real, un stack overflow puede corromper el **return address** o variables adyacentes, causando bugs intermitentes imposibles de reproducir sin sanitizers.

</details>

---

### Ejercicio 5 — UBSan: signed overflow

Compila `ub_errors.c` con UBSan y ejecuta el caso 1:

```bash
gcc -fsanitize=undefined -g ub_errors.c -o ub_errors
./ub_errors 1
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿UBSan abortará el programa o solo imprimirá un warning? ¿Qué valor mostrará INT_MAX + 1?</summary>

Por defecto, UBSan **solo imprime un warning y continúa ejecutando**:

```
INT_MAX     = 2147483647
ub_errors.c:25:17: runtime error: signed integer overflow:
2147483647 + 1 cannot be represented in type 'int'
INT_MAX + 1 = -2147483648
```

El programa NO aborta — sigue e imprime `-2147483648` (wrap de complemento a dos). Este comportamiento es diferente de ASan, que **siempre aborta** al primer error.

Para hacer que UBSan aborte, recompilar con:
```bash
gcc -fsanitize=undefined -fno-sanitize-recover=all -g ub_errors.c -o ub_errors_strict
./ub_errors_strict 1
# Ahora aborta tras el runtime error, sin imprimir INT_MAX + 1
```

</details>

---

### Ejercicio 6 — UBSan: todos los UB secuencialmente

Ejecuta todos los UB con la opción 0:

```bash
./ub_errors 0
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Cuántos de los 4 UB se reportarán antes de que el programa termine?</summary>

Sin `-fno-sanitize-recover=all`:

1. **Signed overflow**: UBSan imprime warning, **continúa**
2. **División por cero**: UBSan imprime warning, pero `42 / 0` genera **SIGFPE** (excepción de hardware) → el programa **termina**

Solo se reportan **2 de 4**. La división por cero y el null dereference generan señales del sistema (SIGFPE, SIGSEGV) que terminan el programa independientemente de UBSan. Los errores 3 (shift) y 4 (null deref) nunca se alcanzan.

Para ver todos los errores, ejecutarlos individualmente con `./ub_errors 1`, `./ub_errors 2`, etc.

</details>

---

### Ejercicio 7 — ASan + UBSan combinados

Compila `combined.c` con ambos sanitizers:

```bash
gcc -fsanitize=address,undefined -g -O0 combined.c -o combined
./combined
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué error se reporta primero: el signed overflow o el heap overflow? ¿Cuántos errores se muestran en total?</summary>

Se reportan **2 errores**, en este orden:

1. **UBSan** reporta primero: `runtime error: signed integer overflow` (línea 19: `x + 1`). Es un warning — UBSan continúa.

2. **ASan** reporta segundo: `heap-buffer-overflow` (línea 30: `arr[4] = 500` en un array de 4 elementos). ASan **aborta** — el programa termina.

El orden refleja el orden de ejecución en el código. UBSan imprime y continúa (modo default); ASan detecta y aborta inmediatamente en el error de memoria.

</details>

---

### Ejercicio 8 — -fno-sanitize-recover=all

Recompila `combined.c` con abort en UBSan:

```bash
gcc -fsanitize=address,undefined -fno-sanitize-recover=all -g -O0 combined.c -o combined_strict
./combined_strict
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Cuántos errores se reportarán ahora? ¿Se alcanzará el heap overflow?</summary>

Solo **1 error**: UBSan reporta el signed overflow y **aborta inmediatamente** (por `-fno-sanitize-recover=all`). El programa nunca llega al heap overflow.

Sin `-fno-sanitize-recover=all`, UBSan solo advierte y continúa, permitiendo que ASan detecte el segundo error. Con esta flag, UBSan aborta igual que ASan.

Implicación práctica: `-fno-sanitize-recover=all` es más estricto (aborta al primer UB), lo que es bueno para CI/CD. Sin la flag, es mejor para exploración manual (ves más errores por ejecución).

</details>

---

### Ejercicio 9 — Opciones de runtime: ASAN_OPTIONS

Usando el binario `combined` (sin `-fno-sanitize-recover`), ejecuta con `halt_on_error=0`:

```bash
ASAN_OPTIONS="halt_on_error=0" ./combined
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿ASan seguirá ejecutando después del heap overflow? ¿Esto es confiable?</summary>

Con `halt_on_error=0`, ASan intenta **continuar después del error** de memoria. Puede mostrar el reporte del heap overflow y seguir ejecutando.

Sin embargo, **esto no es confiable** — la memoria ya está corrupta. ASan escribió en la red zone, y el estado del programa es indefinido. Puede funcionar, crashear, o producir resultados incorrectos.

`halt_on_error=0` es útil para descubrir **múltiples errores** en una sola ejecución (por ejemplo, en un test suite), pero los resultados después del primer error de memoria no son de confianza.

</details>

---

### Ejercicio 10 — Línea de compilación completa para desarrollo

Compila `combined.c` con el set completo de desarrollo y con el set de producción. Compara tamaños:

```bash
gcc -Wall -Wextra -Wpedantic -std=c11 \
    -g -O0 \
    -fsanitize=address,undefined \
    -fno-sanitize-recover=all \
    combined.c -o combined_dev

gcc -Wall -Wextra -std=c11 \
    -O2 -DNDEBUG \
    combined.c -o combined_prod

ls -l combined_dev combined_prod
```

**Predicción**: Antes de comparar, responde:

<details><summary>¿Cuál binario será más grande? ¿Cuál "funcionará" sin errores? ¿Cuál es más seguro?</summary>

**Tamaño**: `combined_dev` será significativamente más grande (~2x o más) porque incluye la instrumentación de ASan y UBSan, red zones, y la info de debug (`-g`).

**Ejecución**:
- `combined_dev` → aborta al primer UB (signed overflow). Nunca llega al heap overflow.
- `combined_prod` → "funciona" sin errores visibles. El overflow produce `-2147483648` silenciosamente. El heap overflow escribe fuera de límites sin crash. **Parece correcto pero tiene 2 bugs.**

**Seguridad**: `combined_dev` es más seguro — detecta los errores inmediatamente. `combined_prod` es más rápido pero los bugs pasan desapercibidos hasta que causan problemas en producción.

| Set | Flags clave | Para qué |
|-----|-------------|----------|
| Desarrollo | `-g -O0 -fsanitize=address,undefined -fno-sanitize-recover=all` | Máxima detección, aborta al primer error |
| CI/CD | `-g -O1 -fsanitize=address,undefined -Werror` | Detección + velocidad razonable + warnings como errores |
| Producción | `-O2 -DNDEBUG` | Máximo rendimiento, sin sanitizers |

</details>

Limpieza final:

```bash
rm -f heap_errors heap_errors_nosanit stack_errors stack_errors_nosanit
rm -f ub_errors ub_errors_strict ub_errors_nosanit
rm -f combined combined_dev combined_prod combined_strict
```
