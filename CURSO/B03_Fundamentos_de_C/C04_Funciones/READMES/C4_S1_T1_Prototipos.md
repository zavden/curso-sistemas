# T01 — Prototipos

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 12 archivos de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

Sin erratas detectadas en el material fuente.

---

## 1. Declaración vs definición

Una función en C tiene dos partes separables:

```c
// DECLARACIÓN (prototipo) — dice que la función existe:
int add(int a, int b);     // tipo de retorno, nombre, tipos de parámetros
                           // NO tiene cuerpo — termina en ;

// DEFINICIÓN — la implementación real:
int add(int a, int b) {    // misma firma que la declaración
    return a + b;           // tiene cuerpo { }
}
```

La declaración le dice al compilador tres cosas:
1. Qué tipo **retorna** la función.
2. **Cuántos** parámetros recibe.
3. Qué **tipo** tiene cada parámetro.

Con esta información, el compilador puede verificar que las llamadas sean correctas
**antes** de ver la definición.

Una definición también sirve como declaración: si defines una función antes de usarla,
no necesitas un prototipo separado.

---

## 2. Por qué se necesitan prototipos

El compilador procesa el código de **arriba a abajo**. Si llamas a una función
antes de declararla o definirla, no la conoce:

```c
#include <stdio.h>

int main(void) {
    int result = add(3, 4);    // ERROR: add no está declarada
    printf("%d\n", result);
    return 0;
}

int add(int a, int b) {       // definición después del uso
    return a + b;
}
```

En C99+ con `-Wall`, GCC produce un error de **implicit declaration**:
```
error: implicit declaration of function 'add' [-Wimplicit-function-declaration]
```

### Solución 1: prototipo forward

```c
#include <stdio.h>

int add(int a, int b);         // prototipo: declara antes de usar

int main(void) {
    int result = add(3, 4);    // OK: el compilador conoce add
    printf("%d\n", result);
    return 0;
}

int add(int a, int b) {       // definición puede ir después
    return a + b;
}
```

### Solución 2: definir antes de usar

```c
int add(int a, int b) {       // definición antes del uso
    return a + b;
}

int main(void) {
    int result = add(3, 4);   // OK: add ya fue definida
    return 0;
}
```

Esto funciona para programas de un solo archivo, pero en proyectos multi-archivo
se necesitan prototipos en headers.

### Historia: por qué C89 era peligroso

En C89, llamar a una función sin prototipo era válido. El compilador asumía que
retornaba `int` y aceptaba **cualquier** argumento sin verificación. Esto causaba
bugs silenciosos que solo aparecían en runtime. C99 prohibió las declaraciones
implícitas precisamente por esto.

---

## 3. `f()` vs `f(void)` — diferencia crítica

En C (hasta C17), paréntesis vacíos y `(void)` significan cosas **diferentes**:

```c
int foo();       // "foo acepta un número INDETERMINADO de argumentos"
                 // el compilador NO verifica los argumentos

int bar(void);   // "bar acepta CERO argumentos"
                 // el compilador SÍ verifica que no le pases nada
```

Demostración:

```c
int foo();
foo(1, 2, 3);       // compila sin error ni warning
foo();               // también OK
foo("hello", 3.14);  // también OK — cualquier cosa se acepta

int bar(void);
bar(1);              // ERROR: too many arguments to function 'bar'
bar();               // OK
```

`int foo()` **no es un prototipo real** — es una declaración sin especificación de
parámetros. El compilador no puede verificar nada. Los argumentos extra simplemente
se apilan en el stack y nadie los usa.

**Regla: siempre usar `(void)` para funciones sin parámetros.**

```c
int main(void) {     // correcto — (void) explícito
    return 0;
}
```

GCC tiene el flag `-Wstrict-prototypes` para detectar `()` sin `void`:
```
warning: function declaration isn't a prototype [-Wstrict-prototypes]
```

> En **C23**, `()` y `(void)` serán equivalentes (como en C++). Pero en C11/C17,
> la diferencia existe y es importante.

---

## 4. Prototipos en headers

El patrón estándar para proyectos multi-archivo:

### El header (`.h`) — solo declaraciones

```c
// math_ops.h
#ifndef MATH_OPS_H
#define MATH_OPS_H

int add(int a, int b);
int subtract(int a, int b);
int multiply(int a, int b);

#endif
```

### La implementación (`.c`) — definiciones

```c
// math_ops.c
#include "math_ops.h"    // incluye su propio header

int add(int a, int b) { return a + b; }
int subtract(int a, int b) { return a - b; }
int multiply(int a, int b) { return a * b; }
```

¿Por qué el `.c` incluye su propio header? Para que el compilador verifique que
los prototipos del header **coincidan** con las definiciones. Si cambias un tipo
en el header pero no en el `.c`, el compilador lo detecta.

### El programa principal — incluye el header

```c
// main.c
#include <stdio.h>
#include "math_ops.h"    // conoce los prototipos

int main(void) {
    printf("%d + %d = %d\n", 10, 3, add(10, 3));
    printf("%d * %d = %d\n", 10, 3, multiply(10, 3));
    return 0;
}
```

### Compilar

```bash
gcc -std=c11 -Wall -Wextra math_ops.c main.c -o calculator
```

GCC compila cada `.c` por separado y luego enlaza los `.o` resultantes. Cada `.c`
necesita ver las declaraciones de las funciones que usa — eso lo provee el header.

### Include guards

Los include guards evitan problemas cuando un header se incluye más de una vez
(directa o indirectamente):

```c
#ifndef MATH_OPS_H     // si NO está definido este macro
#define MATH_OPS_H     // definirlo

// ... contenido del header ...

#endif                 // fin del guard
```

Sin guards, incluir el mismo header dos veces duplicaría las declaraciones.
La segunda inclusión ve que `MATH_OPS_H` ya está definido → `#ifndef` es falso →
se salta todo el contenido.

Alternativa (extensión de compilador, no estándar pero universal):
```c
#pragma once           // mismo efecto, más simple
                       // soportado por GCC, Clang, MSVC
```

---

## 5. Nombres de parámetros en prototipos

Los nombres son **opcionales** en prototipos — solo los tipos son obligatorios:

```c
// Con nombres (recomendado — documenta qué es cada parámetro):
int power(int base, int exponent);
double average(const int *arr, int n);

// Sin nombres (válido pero menos claro):
int power(int, int);
double average(const int *, int);
```

Los nombres en el prototipo no necesitan coincidir con los de la definición.
Son puramente informativos.

---

## 6. Verificación que proveen los prototipos

Con un prototipo, el compilador detecta estos errores en tiempo de compilación:

```c
int add(int a, int b);

// 1. Número incorrecto de argumentos:
add(5);           // ERROR: too few arguments (expected 2, have 1)
add(1, 2, 3);     // ERROR: too many arguments (expected 2, have 3)

// 2. Tipo incorrecto:
add("hello", 2);  // ERROR: passing 'char *' where 'int' expected

// 3. Función no declarada:
unknown(1, 2);    // ERROR: implicit declaration of function 'unknown'
```

Sin prototipos (o con `int f()` sin void), **ninguno** de estos errores se detecta.
Los argumentos incorrectos se apilan en el stack y la función lee basura.

---

## 7. `static` y funciones

`static` en una función limita su visibilidad al **archivo** donde se define:

```c
// helpers.c
static int internal_helper(int x) {    // solo visible en helpers.c
    return x * 2;
}

int public_function(int x) {          // visible desde otros archivos
    return internal_helper(x) + 1;
}
```

Reglas:
- Las funciones `static` **no** se ponen en el header — son "privadas" del archivo.
- Pueden tener prototipos `static` al inicio del mismo `.c` para usarlas en
  cualquier orden:

```c
// archivo.c
static int helper_a(int x);     // prototipo static
static int helper_b(int x);

int public_function(int x) {
    return helper_a(x) + helper_b(x);   // usa ambas antes de definirlas
}

static int helper_a(int x) { return x * 2; }
static int helper_b(int x) { return x + 1; }
```

Esto es análogo a funciones privadas en lenguajes con clases.

---

## 8. Recursión

Una función puede llamarse a sí misma. No necesita prototipo forward para
recursión simple (su propia definición sirve como declaración):

```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);    // llamada recursiva
}
```

### Recursión mutua

Cuando dos funciones se llaman entre sí, al menos una necesita un prototipo forward:

```c
int is_even(int n);    // prototipo forward necesario

int is_odd(int n) {
    if (n == 0) return 0;      // 0 no es impar
    return is_even(n - 1);     // ¿(n-1) es par? → entonces n es impar
}

int is_even(int n) {
    if (n == 0) return 1;      // 0 sí es par
    return is_odd(n - 1);      // ¿(n-1) es impar? → entonces n es par
}
```

Sin el prototipo de `is_even`, el compilador daría error de implicit declaration
cuando `is_odd` intenta llamarla.

**Peligro**: con valores grandes (ej. 1000000), cada llamada recursiva consume
stack. Sin tail-call optimization (que C no garantiza), esto causa stack overflow.

---

## Ejercicios

### Ejercicio 1 — Prototipo vs sin prototipo

```c
// ¿Este programa compila con gcc -std=c11 -Wall?
// Si no, ¿cuál es el error? Si sí, ¿cuál es la salida?

#include <stdio.h>

int main(void) {
    printf("Result: %d\n", square(5));
    return 0;
}

int square(int x) {
    return x * x;
}
```

<details><summary>Predicción</summary>

**No compila** (o da error con `-Wall` en C99+):
```
error: implicit declaration of function 'square' [-Wimplicit-function-declaration]
```

`square` se llama en `main` antes de ser declarada o definida. El compilador
procesa de arriba a abajo: cuando llega a `square(5)`, no conoce la función.

Solución: agregar `int square(int x);` antes de `main`, o mover la definición
de `square` antes de `main`.

</details>

---

### Ejercicio 2 — Definir antes de usar

```c
// Reescribe el programa anterior sin prototipo, definiendo
// la función antes de main. ¿Compila sin warnings?

#include <stdio.h>

int square(int x) {
    return x * x;
}

int cube(int x) {
    return x * x * x;
}

int main(void) {
    printf("square(5) = %d\n", square(5));
    printf("cube(3) = %d\n", cube(3));
    return 0;
}
```

<details><summary>Predicción</summary>

```
square(5) = 25
cube(3) = 27
```

Compila sin warnings. Cuando `main` llama a `square` y `cube`, ambas ya fueron
definidas (y una definición también es una declaración). El compilador conoce
sus firmas.

Esta técnica funciona para archivos sencillos, pero si `square` llamara a `cube`
y `cube` llamara a `square` (recursión mutua), necesitaríamos al menos un prototipo.

</details>

---

### Ejercicio 3 — `()` vs `(void)`: predecir warnings

```c
// Compilar con: gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes
// ¿Cuántos warnings/errores habrá? ¿Cuáles?

#include <stdio.h>

int foo();
int bar(void);

int main(void) {
    printf("foo(1,2,3) = %d\n", foo(1, 2, 3));
    printf("bar() = %d\n", bar());
    return 0;
}

int foo() { return 42; }
int bar(void) { return 99; }
```

<details><summary>Predicción</summary>

Con `-Wstrict-prototypes`:
```
warning: function declaration isn't a prototype [-Wstrict-prototypes]
    int foo();
warning: function declaration isn't a prototype [-Wstrict-prototypes]
    int foo() {
```

**Dos warnings**, ambos sobre `foo`:
1. La declaración `int foo();` no es un prototipo real.
2. La definición `int foo()` tampoco especifica parámetros.

`bar(void)` no genera warning — es un prototipo correcto.

La llamada `foo(1, 2, 3)` **no** genera error porque `int foo()` acepta
cualquier número de argumentos. La salida:
```
foo(1,2,3) = 42
bar() = 99
```

`foo` ignora los 3 argumentos y retorna 42. No hay crash — los argumentos
se ponen en el stack pero nadie los lee.

</details>

---

### Ejercicio 4 — Error de firma

```c
// ¿Qué errores produce este programa?

#include <stdio.h>

int add(int a, int b);

int main(void) {
    printf("%d\n", add(5));
    printf("%d\n", add(1, 2, 3));
    printf("%d\n", add("hello", 2));
    return 0;
}

int add(int a, int b) {
    return a + b;
}
```

<details><summary>Predicción</summary>

Tres errores (el programa no compila):
```
error: too few arguments to function 'add'      (línea del add(5))
error: too many arguments to function 'add'      (línea del add(1,2,3))
error: passing 'char *' where 'int' expected      (línea del add("hello",2))
```

El prototipo `int add(int a, int b)` le da al compilador la información exacta:
- 2 parámetros (no 1, no 3).
- Ambos `int` (no `char *`).

Sin el prototipo (o con `int add()`), **ninguno** de estos errores se detectaría.

</details>

---

### Ejercicio 5 — Header con include guard

```c
// Crea un header math_utils.h con include guard y prototipos:
//   int add(int a, int b);
//   int multiply(int a, int b);
//   int max(int a, int b);
// Luego crea math_utils.c con las definiciones.
// ¿Qué pasa si incluyes el header dos veces en main?

// --- math_utils.h ---
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

int add(int a, int b);
int multiply(int a, int b);
int max(int a, int b);

#endif

// --- math_utils.c ---
#include "math_utils.h"

int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }
int max(int a, int b) { return (a > b) ? a : b; }

// --- main.c ---
#include <stdio.h>
#include "math_utils.h"
#include "math_utils.h"    // inclusión duplicada

int main(void) {
    printf("add(3,4) = %d\n", add(3, 4));
    printf("max(10,7) = %d\n", max(10, 7));
    return 0;
}
```

<details><summary>Predicción</summary>

```
add(3,4) = 7
max(10,7) = 10
```

Compila sin errores ni warnings a pesar del doble `#include "math_utils.h"`.

La primera inclusión define `MATH_UTILS_H` y procesa los prototipos.
La segunda inclusión encuentra que `MATH_UTILS_H` ya está definido →
`#ifndef MATH_UTILS_H` es falso → salta todo hasta `#endif`.

Sin el include guard, habría errores de redeclaración (o al menos warnings con
tipos más complejos como structs).

Verificable con:
```bash
gcc -E main.c | grep -c "int add"   # resultado: 1 (una sola declaración)
```

</details>

---

### Ejercicio 6 — Sin header en multi-archivo

```c
// Archivo: no_header.c (NO incluye math_ops.h)
// Se compila junto con math_ops.c.
// ¿Es un error de compilación o de enlace?

#include <stdio.h>

int main(void) {
    printf("%d\n", add(10, 3));
    printf("%d\n", subtract(10, 3));
    return 0;
}
```

```bash
gcc -std=c11 -Wall math_ops.c no_header.c -o no_header
```

<details><summary>Predicción</summary>

**Error de compilación**, no de enlace:
```
error: implicit declaration of function 'add' [-Wimplicit-function-declaration]
error: implicit declaration of function 'subtract' [-Wimplicit-function-declaration]
```

Aunque `add` y `subtract` existen en `math_ops.c` y el enlazador las encontraría,
el compilador procesa cada `.c` de forma **independiente**. Al compilar `no_header.c`,
no ve ninguna declaración de `add` ni `subtract`.

El error ocurre en la fase de compilación, antes de que el enlazador intervenga.
La solución: `#include "math_ops.h"`.

</details>

---

### Ejercicio 7 — `static` funciones

```c
// ¿Compila este programa? ¿Qué imprime?

#include <stdio.h>

static int double_it(int x);    // prototipo static

int compute(int x) {
    return double_it(x) + 1;
}

static int double_it(int x) {   // definición static
    return x * 2;
}

int main(void) {
    printf("compute(5) = %d\n", compute(5));
    printf("double_it(5) = %d\n", double_it(5));
    return 0;
}
```

<details><summary>Predicción</summary>

```
compute(5) = 11
double_it(5) = 10
```

Compila sin warnings. `double_it` es `static` → su visibilidad se limita a este
archivo. Pero como todo está en un solo archivo, `main` también puede llamarla.

- `compute(5)` → `double_it(5) + 1` → `10 + 1` = 11.
- `double_it(5)` → `5 * 2` = 10.

El prototipo `static int double_it(int x);` al inicio permite que `compute`
la llame antes de su definición. Sin él, `compute` daría error de implicit
declaration.

Si `double_it` estuviera en otro `.c`, no se podría llamar desde este archivo:
`static` la hace "privada" del archivo donde se define.

</details>

---

### Ejercicio 8 — Recursión simple

```c
// Implementa fibonacci recursivo.
// ¿Necesita prototipo? ¿Cuántas llamadas hace fib(6)?

#include <stdio.h>

int fib(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    for (int i = 0; i <= 10; i++) {
        printf("fib(%d) = %d\n", i, fib(i));
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
fib(0) = 0
fib(1) = 1
fib(2) = 1
fib(3) = 2
fib(4) = 3
fib(5) = 5
fib(6) = 8
fib(7) = 13
fib(8) = 21
fib(9) = 34
fib(10) = 55
```

**No necesita prototipo** porque `fib` se llama a sí misma dentro de su propia
definición — la definición ya sirve como declaración. Y `main` la llama después.

Llamadas para `fib(6)`:
- `fib(6)` = `fib(5)` + `fib(4)`
- `fib(5)` = `fib(4)` + `fib(3)` ... y así recursivamente.
- Total: 25 llamadas (sin contar la llamada original). La complejidad es O(2^n) —
  exponencial. Muchos valores se recalculan múltiples veces (ej. `fib(3)` se
  calcula 3 veces). Con memoización o un loop iterativo sería O(n).

</details>

---

### Ejercicio 9 — Recursión mutua

```c
// Implementa is_even e is_odd con recursión mutua.
// ¿Qué pasa con n = 1000000?

#include <stdio.h>

int is_even(int n);    // prototipo forward necesario

int is_odd(int n) {
    if (n == 0) return 0;
    return is_even(n - 1);
}

int is_even(int n) {
    if (n == 0) return 1;
    return is_odd(n - 1);
}

int main(void) {
    for (int i = 0; i <= 6; i++) {
        printf("%d: %s\n", i, is_even(i) ? "even" : "odd");
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
0: even
1: odd
2: even
3: odd
4: even
5: odd
6: even
```

Traza de `is_even(4)`:
- `is_even(4)` → `is_odd(3)` → `is_even(2)` → `is_odd(1)` → `is_even(0)` → 1 (true).

Sin el prototipo `int is_even(int n)`, el compilador daría error cuando `is_odd`
intenta llamar a `is_even` (que aún no fue definida).

**Con n = 1000000**: stack overflow (segmentation fault). Cada llamada recursiva
consume ~32-64 bytes de stack. Con 1 millón de llamadas anidadas, eso es ~32-64 MB.
El stack típico es de 8 MB. C no garantiza tail-call optimization, así que
cada llamada acumula un frame.

La solución práctica: `return n % 2 == 0;` — O(1), sin recursión.

</details>

---

### Ejercicio 10 — Prototipos como documentación

```c
// Lee estos prototipos y deduce qué hace cada función
// sin ver la implementación. Los nombres de parámetros son
// la clave.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Prototipos (con nombres descriptivos):
int clamp(int value, int min_val, int max_val);
int count_char(const char *str, char target);
void to_uppercase(char *str);

int main(void) {
    printf("clamp(15, 0, 10) = %d\n", clamp(15, 0, 10));
    printf("clamp(-5, 0, 10) = %d\n", clamp(-5, 0, 10));
    printf("clamp(7, 0, 10) = %d\n", clamp(7, 0, 10));

    printf("count_char(\"banana\", 'a') = %d\n",
           count_char("banana", 'a'));

    char msg[] = "Hello, World!";
    to_uppercase(msg);
    printf("to_uppercase: \"%s\"\n", msg);

    return 0;
}

int clamp(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

int count_char(const char *str, char target) {
    int count = 0;
    for (const char *p = str; *p != '\0'; p++) {
        if (*p == target) count++;
    }
    return count;
}

void to_uppercase(char *str) {
    for (; *str != '\0'; str++) {
        *str = (char)toupper((unsigned char)*str);
    }
}
```

<details><summary>Predicción</summary>

```
clamp(15, 0, 10) = 10
clamp(-5, 0, 10) = 0
clamp(7, 0, 10) = 7
count_char("banana", 'a') = 3
to_uppercase: "HELLO, WORLD!"
```

- `clamp` restringe `value` al rango `[min_val, max_val]`. 15 > 10 → 10. -5 < 0 → 0. 7 está en rango → 7.
- `count_char` cuenta cuántas veces aparece `target` en `str`. "banana" tiene 3 'a'.
- `to_uppercase` modifica `str` in-place, convirtiendo cada carácter a mayúscula.

Los nombres de los parámetros son documentación gratuita:
- `int clamp(int, int, int)` → ¿qué es cada int? Imposible saberlo.
- `int clamp(int value, int min_val, int max_val)` → inmediatamente claro.

Nota: `to_uppercase` recibe `char *` (no `const char *`) porque modifica el string.
`count_char` recibe `const char *` porque solo lee. Estos calificadores en el
prototipo comunican la **intención**: ¿la función modifica el argumento o solo lo lee?

</details>
