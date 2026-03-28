# T04 — Comportamiento indefinido (UB)

## Qué es el comportamiento indefinido

El estándar de C define tres categorías de comportamiento para
código que no es estrictamente correcto:

```
1. Implementation-defined behavior
   El resultado depende del compilador, pero DEBE ser consistente
   y documentado.
   Ejemplo: sizeof(int) puede ser 2 o 4, pero siempre el mismo
   en la misma plataforma.

2. Unspecified behavior
   El compilador puede elegir entre varias opciones válidas,
   y no tiene que documentar cuál elige ni ser consistente.
   Ejemplo: orden de evaluación de argumentos de función.

3. Undefined behavior (UB)
   El estándar no impone NINGÚN requisito. Cualquier cosa
   puede pasar. El compilador puede asumir que el UB
   nunca ocurre y optimizar en consecuencia.
```

```c
// Undefined behavior = el programa no tiene significado.
// No es "el resultado es impredecible" — es peor:
// el compilador puede hacer CUALQUIER COSA, incluyendo:
// - Producir el resultado "esperado" (la trampa)
// - Producir un resultado diferente cada ejecución
// - Crashear
// - No ejecutar código que parece que debería ejecutarse
// - Borrar código que el programador escribió
// - Formatear tu disco (en teoría — los "nasal demons")
```

## Por qué es peligroso

El peligro real del UB no es que el programa crashee — es que
**parece funcionar**:

```c
// Este programa "funciona" en muchos compiladores con -O0:
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

Este es el concepto clave. Los compiladores modernos usan UB
como **información para optimizar**:

```c
// El compilador razona: "si el programador escribió esto,
// y el estándar dice que X es UB, entonces X nunca ocurre,
// porque el programador escribe código correcto."

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
//  Por lo tanto, el if (p == NULL) siempre es false.
//  Puedo eliminar todo el bloque if."

// Con -O2, el compilador genera:
int foo(int *p) {
    return *p + 1;       // el null check desapareció
}
```

```c
// Ejemplo — loop infinito eliminado:
int fermat(void) {
    int a = 1, b = 1, c = 1;
    while (1) {
        if (a*a*a + b*b*b == c*c*c)  // imposible por Fermat
            return 1;
        // incrementar a, b, c...
        a++; b++; c++;               // overflow eventual = UB
    }
    return 0;
}

// El compilador razona:
// "Los enteros eventualmente hacen overflow (UB).
//  Como UB no puede ocurrir, el loop debe terminar.
//  Si termina, solo puede retornar 1 (la única ruta)."
// Puede optimizar toda la función a: return 1;
```

## Ejemplos clásicos de UB

### 1. Overflow de enteros con signo

```c
int x = INT_MAX;
x = x + 1;          // UB: signed integer overflow

// Unsigned overflow NO es UB — está definido como wrapping:
unsigned int u = UINT_MAX;
u = u + 1;           // definido: u == 0 (wrapping)
```

```c
// Consecuencia práctica:
int check_overflow(int x) {
    if (x + 1 > x) {        // ¿puede fallar?
        return 1;            // "x + 1 es mayor que x"
    }
    return 0;
}

// El compilador puede optimizar esto a: return 1;
// Razonamiento: x + 1 > x siempre es true para enteros con signo,
// porque el overflow no puede ocurrir (es UB).
// Con unsigned, el compilador no puede hacer esa asunción.
```

### 2. Desreferencia de NULL

```c
int *p = NULL;
int x = *p;          // UB: null pointer dereference

// En la práctica: segfault en la mayoría de sistemas.
// Pero el compilador no está obligado a generar un segfault.
```

### 3. Acceso fuera de límites

```c
int arr[10];
arr[10] = 42;        // UB: índice fuera de rango
arr[-1] = 42;        // UB: índice negativo

// No hay bounds checking en C.
// El programa puede:
// - Corromper memoria adyacente (stack smashing)
// - Sobreescribir variables locales
// - Sobreescribir la dirección de retorno (exploit)
// - Parecer funcionar durante años
```

### 4. Uso de variable sin inicializar

```c
int x;               // sin inicializar (valor indeterminado)
printf("%d\n", x);   // UB

// No es simplemente "basura" — el estándar dice que leer
// una variable sin inicializar es UB completo.
// El compilador puede asumir que nunca se lee, y optimizar
// de formas inesperadas.
```

```c
// Ejemplo peligroso:
int x;
if (x == 0) {
    printf("zero\n");
}
if (x != 0) {
    printf("nonzero\n");
}
// Puede imprimir ambos, ninguno, o uno solo.
// El compilador no está obligado a que x tenga un valor consistente.
```

### 5. Doble free

```c
int *p = malloc(sizeof(int));
free(p);
free(p);             // UB: double free

// Puede corromper el heap, crashear, o parecer funcionar.
// Un atacante puede explotar un double free para ejecutar
// código arbitrario.
```

### 6. Uso después de free (use-after-free)

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);
printf("%d\n", *p);  // UB: use-after-free

// El valor puede estar intacto (el allocator no borró la memoria),
// o puede ser basura (el allocator reutilizó el espacio),
// o puede crashear.
```

### 7. Violación de aliasing estricto

```c
float f = 3.14f;
int i = *(int *)&f;   // UB: acceder a un float como int

// Violación de strict aliasing:
// no se puede acceder a un objeto a través de un puntero
// de tipo incompatible (con excepciones: char*, unsigned char*).

// Forma correcta:
int i;
memcpy(&i, &f, sizeof(i));   // definido: copia bytes
```

### 8. Shift inválido

```c
int x = 1;
int y = x << 32;     // UB si int es 32 bits (shift >= ancho del tipo)
int z = x << -1;     // UB: shift negativo

// El resultado de un shift mayor o igual al ancho del tipo
// no está definido. En x86 puede dar un resultado,
// en ARM otro completamente distinto.
```

### 9. División por cero

```c
int x = 42;
int y = x / 0;       // UB: división entera por cero

// En la mayoría de arquitecturas: señal SIGFPE.
// Pero el estándar no lo garantiza.
```

### 10. Modificar un string literal

```c
char *s = "hello";
s[0] = 'H';          // UB: modificar un string literal

// Los string literals están en .rodata (memoria de solo lectura).
// En la práctica: segfault.
// Correcto: char s[] = "hello"; (copia en stack, modificable)
```

### 11. Retornar puntero a variable local

```c
int *bad(void) {
    int x = 42;
    return &x;       // UB: x se destruye al salir de bad()
}

int *p = bad();
printf("%d\n", *p);  // UB: dangling pointer
// p apunta a memoria que ya no es válida.
```

### 12. Orden de evaluación

```c
int i = 0;
int x = i++ + i++;   // UB: modificar i dos veces sin sequence point

// No es "no sé qué resultado da" — es UB.
// El compilador puede generar cualquier código.

// También:
printf("%d %d\n", i++, i++);  // UB

// El orden de evaluación de argumentos es unspecified,
// pero modificar i dos veces es directamente UB.
```

## Cómo detectar UB

### En compilación — Warnings

```bash
# -Wall -Wextra detectan muchos casos:
gcc -Wall -Wextra -Wpedantic main.c -o main

# Algunos warnings relevantes:
# -Wuninitialized         variable sin inicializar
# -Warray-bounds          acceso fuera de límites (si es detectable)
# -Wreturn-type           función no-void sin return
# -Wshift-count-overflow  shift mayor que el ancho del tipo
# -Wformat                formato de printf incorrecto
```

### En runtime — Sanitizers

```bash
# UndefinedBehaviorSanitizer (UBSan):
gcc -fsanitize=undefined -g main.c -o main
./main
# runtime error: signed integer overflow:
# 2147483647 + 1 cannot be represented in type 'int'

# AddressSanitizer (ASan) — errores de memoria:
gcc -fsanitize=address -g main.c -o main
./main
# ERROR: AddressSanitizer: heap-use-after-free

# Ambos juntos:
gcc -fsanitize=address,undefined -g main.c -o main
```

### En runtime — Valgrind

```bash
# Detecta errores de memoria sin recompilar:
gcc -g main.c -o main
valgrind ./main
# Invalid read of size 4
# Address 0x... is 0 bytes after a block of size 40 alloc'd
```

## Cómo evitar UB

```c
// 1. Inicializar TODAS las variables:
int x = 0;              // siempre inicializar
int *p = NULL;           // NULL es mejor que basura

// 2. Verificar punteros antes de desreferenciar:
if (p != NULL) {
    *p = 42;
}

// 3. Verificar límites de arrays:
if (i >= 0 && i < ARRAY_SIZE) {
    arr[i] = value;
}

// 4. Usar unsigned para operaciones que pueden overflow:
unsigned int u = UINT_MAX;
u = u + 1;               // definido: wrapping a 0

// 5. Verificar antes de dividir:
if (divisor != 0) {
    result = dividend / divisor;
}

// 6. free una sola vez, y anular el puntero:
free(p);
p = NULL;                // evita double free y use-after-free

// 7. Compilar SIEMPRE con warnings y sanitizers en desarrollo:
// gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g
```

## UB en la práctica

```c
// El UB no es un defecto del lenguaje — es una decisión de diseño.
//
// C le da UB al compilador como "espacio libre para optimizar":
// - Si signed overflow es UB, el compilador puede asumir que
//   (x + 1 > x) siempre es true → optimización válida.
// - Si null deref es UB, el compilador puede eliminar null checks
//   después de una desreferencia → código más rápido.
//
// La alternativa (definir todo) haría C más lento.
// Rust eligió la alternativa: checked arithmetic, bounds checking,
// ownership — más seguro, pero con costo en complejidad.
//
// En C, la responsabilidad es del programador.
// Los sanitizers y warnings son tus herramientas.
```

## Tabla de UB comunes

| UB | Ejemplo | Cómo detectar |
|---|---|---|
| Signed overflow | INT_MAX + 1 | -fsanitize=undefined |
| Null deref | *NULL | -fsanitize=address |
| Out of bounds | arr[10] en int arr[10] | -fsanitize=address |
| Sin inicializar | int x; uso de x | -Wuninitialized, valgrind |
| Double free | free(p); free(p); | -fsanitize=address, valgrind |
| Use-after-free | free(p); *p | -fsanitize=address, valgrind |
| Dangling pointer | return &local; | -Wreturn-local-addr |
| Shift inválido | x << 32 (int 32-bit) | -fsanitize=undefined |
| División por 0 | x / 0 | -fsanitize=undefined |
| String literal mod | "hello"[0] = 'H' | segfault (runtime) |
| Strict aliasing | *(int*)&float_var | -fstrict-aliasing -Wstrict-aliasing |
| Doble modificación | i++ + i++ | -Wsequence-point |

---

## Ejercicios

### Ejercicio 1 — Detectar UB

```c
// Compilar este código con -Wall -Wextra y con -fsanitize=undefined.
// ¿Cuántos UB puedes encontrar?

#include <stdio.h>
#include <limits.h>

int main(void) {
    int a = INT_MAX;
    int b = a + 1;
    printf("%d\n", b);

    int arr[5] = {1, 2, 3, 4, 5};
    printf("%d\n", arr[5]);

    int x;
    if (x > 0) {
        printf("positive\n");
    }

    int y = 1 << 33;
    printf("%d\n", y);

    return 0;
}
```

### Ejercicio 2 — Optimización por UB

```c
// Compilar con -O0 y con -O2. ¿Cambia el comportamiento?
// ¿Por qué?

#include <stdio.h>
#include <limits.h>

int check(int x) {
    if (x + 1 > x) {
        return 1;
    }
    return 0;
}

int main(void) {
    printf("%d\n", check(INT_MAX));
    return 0;
}
```

### Ejercicio 3 — Sanitizers

```bash
# Compilar este programa con -fsanitize=address,undefined -g
# Ejecutar y leer los mensajes del sanitizer
# ¿Qué errores reporta? ¿En qué líneas?

# Crear un programa que haga:
# 1. malloc de 10 ints
# 2. Escribir en el índice 10 (fuera de límites)
# 3. free
# 4. Leer del puntero liberado
```
