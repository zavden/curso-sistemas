# T01 — Prototipos

## Declaración vs definición

En C, una función tiene dos partes separables: la **declaración**
(prototipo) y la **definición** (implementación):

```c
// DECLARACIÓN (prototipo) — dice que la función existe:
int add(int a, int b);     // tipo de retorno, nombre, tipos de parámetros
                           // NO tiene cuerpo — termina en ;

// DEFINICIÓN — la implementación real:
int add(int a, int b) {    // mismo tipo de retorno y parámetros
    return a + b;           // tiene cuerpo { }
}
```

```c
// La declaración le dice al compilador:
// 1. Qué tipo retorna la función
// 2. Cuántos parámetros recibe
// 3. Qué tipo tiene cada parámetro
//
// Con esta información, el compilador puede VERIFICAR
// que las llamadas son correctas ANTES de ver la definición.
```

## Por qué se necesitan prototipos

```c
// Sin prototipo, el compilador procesa de arriba a abajo.
// Si llamas a una función antes de definirla, no la conoce:

#include <stdio.h>

int main(void) {
    int result = add(3, 4);    // ERROR o WARNING: add no está declarada
    printf("%d\n", result);
    return 0;
}

int add(int a, int b) {
    return a + b;
}
```

```c
// Con prototipo — declara la función ANTES de usarla:

#include <stdio.h>

int add(int a, int b);         // prototipo (declaración forward)

int main(void) {
    int result = add(3, 4);    // OK: el compilador conoce add
    printf("%d\n", result);
    return 0;
}

int add(int a, int b) {        // definición (puede ir después)
    return a + b;
}
```

```c
// Alternativa: definir la función ANTES de usarla
// (no necesita prototipo):

int add(int a, int b) {        // definición antes del uso
    return a + b;
}

int main(void) {
    int result = add(3, 4);    // OK: add ya fue definida
    return 0;
}

// Esto funciona para programas simples, pero en proyectos
// con múltiples archivos se necesitan prototipos en headers.
```

### Qué pasa sin prototipo

```c
// En C89, llamar a una función sin prototipo era válido.
// El compilador asumía que retornaba int y aceptaba
// cualquier argumento (sin verificación de tipos).
// Esto causaba bugs silenciosos.

// En C99+, llamar a una función sin declaración es un error
// (o al menos un warning con -Wall).

// GCC con -Wall -Wimplicit-function-declaration:
// warning: implicit declaration of function 'add'
```

## f() vs f(void) — Diferencia crítica

```c
// En C, paréntesis vacíos y (void) significan cosas DIFERENTES:

int foo();          // "foo acepta un número INDETERMINADO de argumentos"
                    // El compilador NO verifica los argumentos

int bar(void);      // "bar acepta CERO argumentos"
                    // El compilador verifica que no le pases nada

// Demostración:
int foo();
foo(1, 2, 3);       // sin error ni warning — el compilador no verifica
foo();               // también OK
foo("hello", 3.14);  // también OK — cualquier cosa se acepta

int bar(void);
bar(1);              // ERROR: too many arguments to function 'bar'
bar();               // OK

// SIEMPRE usar (void) para funciones sin parámetros:
int main(void) {     // correcto
    return 0;
}

// En C23, () y (void) serán equivalentes (como en C++).
// Pero en C11, la diferencia existe y es importante.
```

## Prototipos en headers

El patrón estándar para proyectos con múltiples archivos:

```c
// math_utils.h — declaraciones (prototipos):
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

int add(int a, int b);
int multiply(int a, int b);
double average(const int *arr, int n);

#endif
```

```c
// math_utils.c — definiciones:
#include "math_utils.h"

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

double average(const int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return (double)sum / n;
}
```

```c
// main.c — uso:
#include <stdio.h>
#include "math_utils.h"    // incluye los prototipos

int main(void) {
    printf("%d\n", add(3, 4));        // OK: prototipo conocido
    printf("%d\n", multiply(5, 6));   // OK
    return 0;
}
```

```bash
# Compilar:
gcc -std=c11 -Wall math_utils.c main.c -o program
```

### Include guards

```c
// Los include guards evitan inclusión múltiple:

// Sin guards, si dos headers incluyen el mismo header:
// #include "utils.h"   ← primera vez: define las funciones
// #include "utils.h"   ← segunda vez: redefinición → error

// Con guards:
#ifndef MATH_UTILS_H     // si no está definido
#define MATH_UTILS_H     // definirlo

// ... contenido del header ...

#endif                    // fin del guard

// La segunda inclusión: MATH_UTILS_H ya está definido
// → el #ifndef es false → se salta todo el contenido

// Alternativa (extensión de compilador, no estándar):
#pragma once              // mismo efecto, más simple
                          // soportado por GCC, Clang, MSVC
```

## Nombres de parámetros en prototipos

```c
// En prototipos, los nombres de parámetros son OPCIONALES:

// Con nombres (documenta qué es cada parámetro):
int power(int base, int exponent);

// Sin nombres (válido pero menos claro):
int power(int, int);

// RECOMENDACIÓN: incluir los nombres.
// Son documentación gratuita.
```

## Verificación que proveen los prototipos

```c
// El compilador verifica con prototipos:

int add(int a, int b);

// 1. Número de argumentos:
add(1);           // ERROR: too few arguments
add(1, 2, 3);     // ERROR: too many arguments

// 2. Tipos de argumentos:
add("hello", 2);  // WARNING: passing 'char*' where 'int' expected

// 3. Tipo de retorno:
double result = add(1, 2);  // WARNING: implicit conversion int → double
                            // (dependiendo del contexto)

// 4. Funciones no declaradas:
unknown(1, 2);    // ERROR o WARNING: implicit declaration
```

## static y funciones

```c
// static en una función limita su visibilidad al archivo:

// helpers.c
static int internal_helper(int x) {    // solo visible en helpers.c
    return x * 2;
}

int public_function(int x) {           // visible en otros archivos
    return internal_helper(x) + 1;
}

// No se pone el prototipo de static functions en el header.
// Son "privadas" del archivo.
```

```c
// Prototipos static en el mismo archivo:
// A veces se declaran al inicio del .c para poder usarlas
// en cualquier orden dentro del archivo:

static int helper_a(int x);
static int helper_b(int x);

int public_function(int x) {
    return helper_a(x) + helper_b(x);
}

static int helper_a(int x) { return x * 2; }
static int helper_b(int x) { return x + 1; }
```

## Recursión

```c
// Una función puede llamarse a sí misma.
// El prototipo está implícito dentro de su propia definición:

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);    // llamada recursiva
}

// Para recursión mutua (A llama a B, B llama a A),
// se necesita al menos un prototipo forward:

int is_even(int n);    // prototipo forward necesario

int is_odd(int n) {
    if (n == 0) return 0;
    return is_even(n - 1);     // llama a is_even
}

int is_even(int n) {
    if (n == 0) return 1;
    return is_odd(n - 1);      // llama a is_odd
}
```

---

## Ejercicios

### Ejercicio 1 — f() vs f(void)

```c
// Compilar con -Wall -Wextra -std=c11:
// ¿Qué warnings aparecen?

int foo();
int bar(void);

int main(void) {
    foo(1, 2, 3);
    bar(1, 2, 3);
    return 0;
}

int foo() { return 0; }
int bar(void) { return 0; }
```

### Ejercicio 2 — Header y source

```c
// Crear:
// - string_utils.h con prototipos de:
//   int my_strlen(const char *s);
//   int my_strcmp(const char *a, const char *b);
// - string_utils.c con las definiciones
// - main.c que las use
// Compilar los tres archivos juntos.
```

### Ejercicio 3 — Recursión mutua

```c
// Implementar is_even e is_odd usando recursión mutua.
// Verificar con valores 0 a 10.
// ¿Qué pasa con valores grandes (ej. 1000000)?
```
