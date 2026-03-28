# T04 — Comportamiento indefinido (UB)

> **Errata del material base**
>
> | Ubicación | Error | Corrección |
> |-----------|-------|------------|
> | `labs/README.md:103‑104` | Dice *"UBSan detectó 3 de los 4 UB (el acceso out-of-bounds puede requerir `-fsanitize=address` para detección completa)"*, implicando que el OOB fue el UB no detectado. Sin embargo, la propia salida mostrada dos líneas arriba incluye `runtime error: index 7 out of bounds for type 'int [5]'` — UBSan **sí** detectó el OOB. | El UB no detectado por UBSan es la **variable sin inicializar** (`uninitialized_use`). UBSan no instrumenta lecturas de memoria indeterminada. Para detectar uso de variables sin inicializar se necesita **MemorySanitizer** (`-fsanitize=memory`, solo clang) o **Valgrind** (`valgrind ./programa`). La nota sobre ASan y OOB es cierta en general (ASan detecta OOB en heap, UBSan solo en arrays con tamaño conocido en compilación), pero en este contexto confunde sobre cuál fue el UB no detectado. |

---

## 1. Qué es el comportamiento indefinido

El estándar de C define tres categorías para código que no es estrictamente correcto:

| Categoría | Definición | Ejemplo |
|-----------|-----------|---------|
| **Implementation-defined** | El resultado depende del compilador, pero **debe** ser consistente y documentado | `sizeof(int)` puede ser 2 o 4, siempre el mismo en la misma plataforma |
| **Unspecified** | El compilador elige entre opciones válidas, no tiene que documentar ni ser consistente | Orden de evaluación de argumentos de función |
| **Undefined behavior (UB)** | El estándar no impone **ningún** requisito. Cualquier cosa puede pasar | `INT_MAX + 1`, desreferencia de NULL |

```c
// Undefined behavior = el programa no tiene significado.
// No es "el resultado es impredecible" — es peor:
// el compilador puede hacer CUALQUIER COSA, incluyendo:
// - Producir el resultado "esperado" (la trampa más peligrosa)
// - Producir un resultado diferente cada ejecución
// - Crashear
// - No ejecutar código que parece que debería ejecutarse
// - Eliminar código que el programador escribió
```

---

## 2. Por qué es peligroso

El peligro real del UB no es que el programa crashee — es que **parece funcionar**:

```c
#include <stdio.h>

int main(void) {
    int arr[3] = {10, 20, 30};
    printf("%d\n", arr[5]);    // UB: acceso fuera de límites
    return 0;
}
// Con -O0 puede imprimir basura (el valor que esté en esa memoria).
// Con -O2 puede imprimir 0, crashear, o no imprimir nada.
// "Funciona en mi máquina" no significa que sea correcto.
```

### El compilador asume que no hay UB

Este es el concepto clave. Los compiladores modernos usan UB como **información para optimizar**:

```c
// Ejemplo — null check eliminado:
int foo(int *p) {
    int x = *p;         // desreferencia p
    if (p == NULL) {     // ¿p es NULL?
        return -1;       // manejar NULL
    }
    return x + 1;
}

// El compilador razona:
// "En la línea 2, desreferencia p. Si p fuera NULL, eso es UB.
//  Como UB no puede ocurrir, p NO es NULL.
//  Por lo tanto, el if (p == NULL) siempre es false."
//
// Con -O2, genera:
// int foo(int *p) { return *p + 1; }
```

Ver `labs/ub_optimizer.c:24-31` para la implementación concreta de este patrón (`null_check_after_deref`).

```c
// Ejemplo — loop infinito eliminado:
int fermat(void) {
    int a = 1, b = 1, c = 1;
    while (1) {
        if (a*a*a + b*b*b == c*c*c)
            return 1;
        a++; b++; c++;               // overflow eventual = UB
    }
    return 0;
}

// El compilador razona:
// "Los enteros eventualmente hacen overflow (UB).
//  Como UB no puede ocurrir, el loop debe terminar.
//  Si termina, solo puede retornar 1."
// Puede optimizar toda la función a: return 1;
```

---

## 3. Los 12 UB clásicos

### 3.1 Overflow de enteros con signo

```c
int x = INT_MAX;
x = x + 1;          // UB: signed integer overflow

// Unsigned overflow NO es UB — está definido como wrapping:
unsigned int u = UINT_MAX;
u = u + 1;           // definido: u == 0
```

Consecuencia práctica — el compilador puede eliminar checks:

```c
int check_overflow(int x) {
    if (x + 1 > x)      // el compilador puede reducir a: return 1;
        return 1;        // porque x + 1 > x "siempre es true"
    return 0;            // (overflow no puede ocurrir = UB)
}
```

Ver `labs/ub_optimizer.c:43-48` — esta función es `overflow_check`, que produce resultados distintos con `-O0` vs `-O2`.

### 3.2 Desreferencia de NULL

```c
int *p = NULL;
int x = *p;          // UB: null pointer dereference
// En la práctica: segfault en la mayoría de sistemas.
// Pero el compilador no está obligado a generar un segfault.
```

### 3.3 Acceso fuera de límites

```c
int arr[10];
arr[10] = 42;        // UB: índice fuera de rango
arr[-1] = 42;        // UB: índice negativo
// No hay bounds checking en C.
// Puede corromper memoria adyacente (stack smashing),
// sobreescribir la dirección de retorno (exploit), o "funcionar".
```

Ver `labs/ub_showcase.c:22-26` — `out_of_bounds()` accede a `arr[7]` de un array de 5 elementos.

### 3.4 Uso de variable sin inicializar

```c
int x;               // sin inicializar (valor indeterminado)
printf("%d\n", x);   // UB

// No es simplemente "basura" — el compilador puede asumir
// que nunca se lee, y optimizar de formas inesperadas:
int x;
if (x == 0) printf("zero\n");
if (x != 0) printf("nonzero\n");
// Puede imprimir ambos, ninguno, o uno solo.
```

Este UB es el más difícil de detectar: `-Wall` lo atrapa parcialmente (`-Wuninitialized`), pero UBSan no lo detecta en runtime. Se necesita **MemorySanitizer** (solo clang: `-fsanitize=memory`) o **Valgrind**.

### 3.5 Doble free

```c
int *p = malloc(sizeof(int));
free(p);
free(p);             // UB: double free
// Puede corromper el heap, crashear, o parecer funcionar.
// Un atacante puede explotar un double free para ejecutar código arbitrario.
```

### 3.6 Uso después de free (use-after-free)

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p);  // UB: use-after-free
// El valor puede estar intacto, ser basura, o crashear.
```

### 3.7 Violación de aliasing estricto

```c
float f = 3.14f;
int i = *(int *)&f;   // UB: acceder a un float como int

// Strict aliasing: no se puede acceder a un objeto a través
// de un puntero de tipo incompatible.
// Excepciones: char*, unsigned char* (pueden alias a cualquier tipo).

// Forma correcta:
int i;
memcpy(&i, &f, sizeof(i));   // definido: copia bytes
```

### 3.8 Shift inválido

```c
int x = 1;
int y = x << 32;     // UB si int es 32 bits (shift >= ancho del tipo)
int z = x << -1;     // UB: shift negativo
```

Ver `labs/ub_showcase.c:38-42` — `bad_shift()` demuestra `1 << 32`.

### 3.9 División por cero

```c
int x = 42;
int y = x / 0;       // UB: división entera por cero
// En la mayoría de arquitecturas: señal SIGFPE.
```

### 3.10 Modificar un string literal

```c
char *s = "hello";
s[0] = 'H';          // UB: modificar un string literal
// Los string literals están en .rodata (solo lectura). Segfault en la práctica.
// Correcto: char s[] = "hello"; (copia en stack, modificable)
```

### 3.11 Retornar puntero a variable local

```c
int *bad(void) {
    int x = 42;
    return &x;       // UB: x se destruye al salir de bad()
}
int *p = bad();
printf("%d\n", *p);  // UB: dangling pointer
```

### 3.12 Orden de evaluación

```c
int i = 0;
int x = i++ + i++;   // UB: modificar i dos veces sin sequence point

printf("%d %d\n", i++, i++);  // UB también
// El orden de evaluación de argumentos es unspecified,
// pero modificar i dos veces es directamente UB.
```

---

## 4. Cómo detectar UB

### En compilación — Warnings

```bash
gcc -Wall -Wextra -Wpedantic main.c -o main

# Warnings relevantes para UB:
# -Wuninitialized         variable sin inicializar
# -Warray-bounds          acceso fuera de límites (si es detectable estáticamente)
# -Wreturn-type           función no-void sin return
# -Wshift-count-overflow  shift >= ancho del tipo
# -Wformat                formato de printf incorrecto
# -Wsequence-point        doble modificación sin sequence point
# -Wstrict-aliasing       violaciones de strict aliasing (impreciso)
# -Wreturn-local-addr     retornar dirección de variable local
```

Limitaciones: los warnings son análisis estático — solo detectan lo que el compilador puede ver en compilación. Muchos UB dependen de valores en runtime.

### En runtime — Sanitizers

```bash
# UndefinedBehaviorSanitizer (UBSan):
gcc -fsanitize=undefined -g main.c -o main
./main
# Detecta: signed overflow, shift inválido, división por 0,
# OOB en arrays con tamaño conocido, null deref, y más.
# NO detecta: variables sin inicializar.

# AddressSanitizer (ASan):
gcc -fsanitize=address -g main.c -o main
./main
# Detecta: OOB (stack, heap, global), use-after-free,
# double free, stack-use-after-return.

# Ambos juntos:
gcc -fsanitize=address,undefined -g main.c -o main

# MemorySanitizer (MSan) — solo clang:
clang -fsanitize=memory -g main.c -o main
# Detecta: uso de variables/memoria sin inicializar.
# No se puede combinar con ASan (son mutuamente exclusivos).
```

### En runtime — Valgrind

```bash
# Detecta errores de memoria sin recompilar (pero compilar con -g ayuda):
gcc -g main.c -o main
valgrind ./main
# Detecta: acceso inválido, uso de memoria sin inicializar,
# memory leaks, double free, use-after-free.
# Más lento que sanitizers (~20x), pero no requiere recompilación.
```

### Qué herramienta detecta qué

| UB | `-Wall` | UBSan | ASan | MSan/Valgrind |
|----|---------|-------|------|---------------|
| Signed overflow | — | **Sí** | — | — |
| Null deref | — | **Sí** | **Sí** | — |
| OOB (stack, tipo conocido) | parcial | **Sí** | **Sí** | — |
| OOB (heap) | — | — | **Sí** | **Sí** |
| Sin inicializar | parcial | — | — | **Sí** |
| Double free | — | — | **Sí** | **Sí** |
| Use-after-free | — | — | **Sí** | **Sí** |
| Shift inválido | **Sí** | **Sí** | — | — |
| División por 0 | — | **Sí** | — | — |
| Dangling pointer (return &local) | **Sí** | — | **Sí** | **Sí** |

---

## 5. Cómo evitar UB

```c
// 1. Inicializar TODAS las variables:
int x = 0;
int *p = NULL;

// 2. Verificar punteros antes de desreferenciar:
if (p != NULL) {
    *p = 42;
}

// 3. Verificar límites de arrays:
if (i >= 0 && i < ARRAY_SIZE) {
    arr[i] = value;
}

// 4. Usar unsigned para aritmética que puede wrappear:
unsigned int u = UINT_MAX;
u = u + 1;               // definido: wrapping a 0

// 5. Verificar antes de dividir:
if (divisor != 0) {
    result = dividend / divisor;
}

// 6. free una sola vez, y anular el puntero:
free(p);
p = NULL;                // evita double free y use-after-free

// 7. Validar shift amount:
if (shift >= 0 && shift < (int)(sizeof(int) * 8)) {
    result = x << shift;
}

// 8. Compilar SIEMPRE con warnings y sanitizers en desarrollo:
// gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g
```

Ver `labs/ub_fixed.c` para implementaciones concretas de los patrones 1, 3, 4 y 7 aplicados a los 4 UB de `ub_showcase.c`.

---

## 6. UB en la práctica — decisión de diseño

```
C le da UB al compilador como "espacio libre para optimizar":
- Si signed overflow es UB → el compilador asume (x + 1 > x) siempre true
- Si null deref es UB → puede eliminar null checks tras una desreferencia

La alternativa (definir todo) haría C más lento.
Rust eligió esa alternativa: checked arithmetic, bounds checking,
ownership — más seguro, pero con mayor complejidad del lenguaje.

En C, la responsabilidad es del programador.
Los sanitizers y warnings son las herramientas de defensa.
```

---

## 7. Tabla resumen de UB comunes

| UB | Ejemplo | Cómo detectar | Cómo evitar |
|----|---------|---------------|-------------|
| Signed overflow | `INT_MAX + 1` | `-fsanitize=undefined` | Usar `unsigned` o verificar antes de operar |
| Null deref | `*NULL` | `-fsanitize=address` | Verificar `p != NULL` antes de `*p` |
| Out of bounds | `arr[10]` en `int arr[10]` | `-fsanitize=address` | Bounds check: `i >= 0 && i < size` |
| Sin inicializar | `int x;` uso de `x` | `-Wuninitialized`, Valgrind | Inicializar siempre: `int x = 0;` |
| Double free | `free(p); free(p);` | `-fsanitize=address`, Valgrind | `free(p); p = NULL;` |
| Use-after-free | `free(p); *p` | `-fsanitize=address`, Valgrind | No usar puntero tras `free` |
| Dangling pointer | `return &local;` | `-Wreturn-local-addr` | Retornar por valor o usar `malloc` |
| Shift inválido | `x << 32` (int 32-bit) | `-fsanitize=undefined` | Validar `shift < sizeof(type)*8` |
| División por 0 | `x / 0` | `-fsanitize=undefined` | Verificar `divisor != 0` |
| String literal mod | `"hello"[0] = 'H'` | Segfault (runtime) | Usar `char s[] = "hello"` |
| Strict aliasing | `*(int*)&float_var` | `-Wstrict-aliasing` | Usar `memcpy` |
| Doble modificación | `i++ + i++` | `-Wsequence-point` | Una modificación por sentencia |

---

## 8. Archivos del laboratorio

| Archivo | Descripción |
|---------|-------------|
| `labs/ub_showcase.c` | 4 funciones con UB clásicos: signed overflow, OOB, uninit, shift |
| `labs/ub_optimizer.c` | 2 funciones donde el optimizador explota UB (null check, overflow check) |
| `labs/ub_fixed.c` | Versión corregida de `ub_showcase.c` con patrones seguros |

---

## Ejercicios

### Ejercicio 1 — UB silencioso: compilar sin protecciones

Compila `ub_showcase.c` sin flags de warning ni sanitizers y ejecútalo:

```bash
cd labs/
gcc ub_showcase.c -o ub_showcase
./ub_showcase
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿El programa crasheará?</summary>

No. Los 4 UB (`signed_overflow`, `out_of_bounds`, `uninitialized_use`, `bad_shift`) producirán valores sin sentido pero el programa no crasheará. `signed_overflow` probablemente imprimirá `-2147483648` (complemento a dos wrap). `out_of_bounds` imprimirá basura (lo que haya en `arr[7]`). `uninitialized_use` imprimirá 0 o 1 según el valor residual del stack. `bad_shift` será impredecible.

El mensaje final *"All functions returned without crashing"* aparecerá. Esto es exactamente lo que hace peligroso al UB: el silencio.

</details>

---

### Ejercicio 2 — Warnings: la primera línea de defensa

Compila con `-Wall -Wextra`:

```bash
gcc -Wall -Wextra ub_showcase.c -o ub_showcase_warn
```

**Predicción**: Antes de compilar, responde:

<details><summary>¿Cuántos de los 4 UB generarán warning? ¿Cuáles?</summary>

**2 de 4** generan warning:

- `uninitialized_use` → `-Wuninitialized`: *'x' is used uninitialized*
- `bad_shift` → `-Wshift-count-overflow`: *left shift count >= width of type*

Los otros 2 **no** generan warning:
- `signed_overflow` (`INT_MAX + 1`) — GCC no advierte sobre overflow con valores en runtime
- `out_of_bounds` (`arr[7]`) — el análisis estático de `-Wall` no siempre detecta OOB (depende de la versión de GCC y de si el índice es constante)

Lección: los warnings son útiles pero **no son suficientes**. Necesitas sanitizers.

</details>

---

### Ejercicio 3 — UBSan: detección en runtime

Compila con UBSan y ejecuta:

```bash
gcc -fsanitize=undefined -g ub_showcase.c -o ub_showcase_ubsan
./ub_showcase_ubsan
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Cuántos de los 4 UB detectará UBSan? ¿Cuál quedará sin detectar y por qué?</summary>

UBSan detecta **3 de 4**:

1. `signed_overflow` → *runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'*
2. `out_of_bounds` → *runtime error: index 7 out of bounds for type 'int [5]'* (UBSan detecta OOB en arrays cuyo tamaño es conocido en compilación)
3. `bad_shift` → *runtime error: shift exponent 32 is too large for 32-bit type 'int'*

El **no detectado** es `uninitialized_use`. UBSan no instrumenta lecturas de memoria indeterminada. Para detectar variables sin inicializar se necesita:
- **MemorySanitizer** (solo clang): `clang -fsanitize=memory -g`
- **Valgrind**: `valgrind ./programa`

</details>

---

### Ejercicio 4 — El optimizador explota UB: -O0 vs -O2

Compila `ub_optimizer.c` con dos niveles de optimización:

```bash
gcc -O0 ub_optimizer.c -o ub_opt_O0
gcc -O2 ub_optimizer.c -o ub_opt_O2
```

Ejecuta ambos:

```bash
echo "=== -O0 ===" && ./ub_opt_O0
echo ""
echo "=== -O2 ===" && ./ub_opt_O2
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué retornará overflow_check(INT_MAX) con -O0 y con -O2?</summary>

- **-O0**: retorna **0**. `INT_MAX + 1` wrappea a `-2147483648` en complemento a dos. La comparación `-2147483648 > INT_MAX` es false, así que retorna 0.
- **-O2**: retorna **1**. El compilador razona: *"signed overflow es UB, por lo tanto no puede ocurrir. Si no hay overflow, `x + 1 > x` es siempre true."* Reduce la función entera a `return 1;`.

El **mismo código fuente** produce resultados distintos. Esto no es un bug del compilador — es una optimización válida según el estándar.

</details>

---

### Ejercicio 5 — Verificar en el assembly

Genera el assembly de `ub_optimizer.c` con ambos niveles:

```bash
gcc -S -O0 ub_optimizer.c -o ub_opt_O0.s
gcc -S -O2 ub_optimizer.c -o ub_opt_O2.s
```

Busca la función `overflow_check` en ambos archivos `.s`.

**Predicción**: Antes de inspeccionar, responde:

<details><summary>¿Qué instrucciones esperas ver con -O2 para overflow_check?</summary>

Con **-O0**, verás instrucciones de comparación (`addl`, `cmpl`, `jle` o similar) — el compilador generó el código "literal" con la suma y la comparación.

Con **-O2**, la función se reduce a:

```asm
overflow_check:
    movl    $1, %eax
    ret
```

Dos instrucciones. El compilador eliminó toda la lógica de comparación porque dedujo que el resultado siempre es 1 (asumiendo que no hay overflow, porque eso sería UB).

</details>

Limpieza:

```bash
rm -f ub_opt_O0 ub_opt_O2 ub_opt_O0.s ub_opt_O2.s
```

---

### Ejercicio 6 — Null check eliminado

Examina la función `null_check_after_deref` en `labs/ub_optimizer.c:24-31`. El patrón es:

```c
int x = *p;            // desreferencia primero
if (p == NULL) {       // check después — demasiado tarde
    return -1;
}
return x + 1;
```

Genera assembly con `-O0` y `-O2`, y busca la función:

```bash
gcc -S -O0 ub_optimizer.c -o null_O0.s
gcc -S -O2 ub_optimizer.c -o null_O2.s
```

**Predicción**: Antes de inspeccionar, responde:

<details><summary>¿Qué pasa con el if (p == NULL) en -O2?</summary>

Con **-O0**, verás la comparación con cero (`testq %rdi, %rdi` o `cmpq $0, %rdi` seguido de `je`) — el null check está presente en el assembly.

Con **-O2**, el compilador razona: *"p fue desreferenciado en `int x = *p`. Si p fuera NULL, eso es UB. Como UB no puede ocurrir, p NO es NULL. El `if (p == NULL)` es código muerto."* La comparación **desaparece** del assembly — solo queda la desreferencia y el `return x + 1`.

Esto demuestra que el orden de operaciones importa: siempre verificar NULL **antes** de desreferenciar.

</details>

Limpieza:

```bash
rm -f null_O0.s null_O2.s
```

---

### Ejercicio 7 — Código corregido: cero errores de sanitizer

Compila `ub_fixed.c` con sanitizers completos:

```bash
gcc -Wall -Wextra -fsanitize=address,undefined -g ub_fixed.c -o ub_fixed
./ub_fixed
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué valores imprimirá cada función corregida?</summary>

```
1. Safe wrapping   (UINT_MAX + 1): 0       ← unsigned wrapping definido
2. Safe access     (index 7):      -1      ← bounds check rechazó el acceso
3. Safe init       (x = 0; x>0):   0       ← x inicializado a 0, condición false
4. Safe shift      (shift 32):     0       ← validación rechazó shift >= width
```

Además imprimirá mensajes explicativos para los casos 2 y 4 (OOB y shift inválido).

Los sanitizers reportan **cero errores** — cada UB fue reemplazado por comportamiento definido con un valor de retorno seguro.

| UB original | Corrección en `ub_fixed.c` |
|-------------|---------------------------|
| `INT_MAX + 1` (signed overflow) | `unsigned int` (wrapping definido) |
| `arr[7]` (OOB) | Bounds check + fallback `-1` |
| `int x;` (sin inicializar) | `int x = 0;` |
| `1 << 32` (shift inválido) | Validar `shift < sizeof(int)*8` |

</details>

---

### Ejercicio 8 — Escribir y detectar use-after-free

Crea un archivo `ub_heap.c` con el siguiente programa:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *p = malloc(10 * sizeof(int));
    if (p == NULL) return 1;

    p[0] = 42;
    printf("Before free: p[0] = %d\n", p[0]);

    free(p);

    // UB 1: use-after-free
    printf("After free: p[0] = %d\n", p[0]);

    // UB 2: double free
    free(p);

    return 0;
}
```

Compila y ejecuta sin sanitizers, luego con `-fsanitize=address -g`:

```bash
gcc ub_heap.c -o ub_heap
./ub_heap
echo "---"
gcc -fsanitize=address -g ub_heap.c -o ub_heap_asan
./ub_heap_asan
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué pasa sin sanitizers? ¿Y con ASan?</summary>

**Sin sanitizers**: el programa probablemente **no crashea** (o crashea en el double free, depende del allocator). El `printf` tras el `free` puede imprimir `42` porque el allocator no borró la memoria — parece "funcionar".

**Con ASan**: el programa se detiene en el **primer** UB detectado — el use-after-free en `printf("After free: p[0] = %d\n", p[0])`. ASan reporta:

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 4 at 0x... thread T0
```

Con la línea exacta del acceso y la línea donde se hizo el `free`. El double free no llega a ejecutarse porque ASan abortó en el use-after-free.

</details>

Limpieza:

```bash
rm -f ub_heap ub_heap_asan ub_heap.c
```

---

### Ejercicio 9 — Dangling pointer: retornar dirección de variable local

Crea un archivo `ub_dangling.c`:

```c
#include <stdio.h>

int *bad_pointer(void) {
    int x = 42;
    return &x;    // warning: retornando dirección de variable local
}

int clobber(void) {
    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int sum = 0;
    for (int i = 0; i < 10; i++) sum += arr[i];
    return sum;
}

int main(void) {
    int *p = bad_pointer();
    printf("Before clobber: *p = %d\n", *p);

    clobber();   // usa el stack, sobrescribiendo el frame de bad_pointer
    printf("After clobber:  *p = %d\n", *p);

    return 0;
}
```

Compila con `-Wall` y ejecuta:

```bash
gcc -Wall -g ub_dangling.c -o ub_dangling
./ub_dangling
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué imprimirá cada printf? ¿Cambiará el valor de *p?</summary>

El compilador emitirá el warning: *"function returns address of local variable"* (`-Wreturn-local-addr`).

El primer `printf` probablemente imprimirá `42` — el stack frame de `bad_pointer` aún no ha sido reutilizado, así que el valor sobrevive por coincidencia.

El segundo `printf` probablemente imprimirá un **valor diferente** (posiblemente 0, 45, o basura). La llamada a `clobber()` reutiliza el espacio del stack donde estaba `x`, sobrescribiendo el valor. `p` ahora apunta a memoria que contiene datos de `clobber`.

Esto demuestra que un dangling pointer puede "funcionar" temporalmente y fallar solo cuando el stack se reutiliza — un bug intermitente extremadamente difícil de diagnosticar sin herramientas.

</details>

Limpieza:

```bash
rm -f ub_dangling ub_dangling.c
```

---

### Ejercicio 10 — El set de compilación para desarrollo

Compila `ub_showcase.c` con el set completo de desarrollo y compara con `ub_fixed.c`:

```bash
gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g ub_showcase.c -o ub_check 2>&1
echo "---"
gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g ub_fixed.c -o ub_fixed_check 2>&1
```

Ejecuta ambos y cuenta errores:

```bash
echo "=== ub_showcase (con UB) ==="
./ub_check 2>&1 | grep -c "runtime error"
echo "errores de runtime"

echo ""
echo "=== ub_fixed (corregido) ==="
./ub_fixed_check 2>&1 | grep -c "runtime error"
echo "errores de runtime"
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Cuántos warnings emite cada compilación? ¿Cuántos runtime errors?</summary>

**Compilación**:
- `ub_showcase.c`: al menos **2 warnings** (uninitialized, shift-count-overflow). Con `-Wpedantic` puede haber alguno adicional.
- `ub_fixed.c`: **0 warnings** — el código corregido compila limpio.

**Runtime**:
- `ub_showcase`: **3 runtime errors** de UBSan (signed overflow, OOB, bad shift). El uninitialized no es detectado por UBSan.
- `ub_fixed`: **0 runtime errors** — todos los patrones son comportamiento definido.

El set de desarrollo `gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g` combina análisis estático (warnings en compilación) y dinámico (sanitizers en runtime). Usarlo sistemáticamente atrapa la mayoría de errores antes de que lleguen a producción.

| Flag | Qué hace |
|------|----------|
| `-Wall -Wextra -Wpedantic` | Máximos warnings en compilación |
| `-fsanitize=address` | Detecta errores de memoria (OOB, use-after-free, double free) |
| `-fsanitize=undefined` | Detecta UB (overflow, shift, null deref, división por 0) |
| `-g` | Info de debug para que los sanitizers muestren líneas exactas |

Para **producción**: `gcc -O2 -DNDEBUG -Wall -Wextra` (sin sanitizers — penalizan rendimiento).

</details>

Limpieza final:

```bash
rm -f ub_check ub_fixed_check ub_showcase ub_showcase_warn ub_showcase_ubsan
```
