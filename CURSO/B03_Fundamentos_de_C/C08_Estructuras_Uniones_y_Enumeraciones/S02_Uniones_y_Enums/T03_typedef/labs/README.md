# Lab — typedef

## Objetivo

Practicar los tres usos principales de `typedef`: crear alias legibles para
tipos existentes, simplificar declaraciones de punteros a funcion, y construir
opaque types para encapsulacion. Al finalizar, sabras cuando y como usar
`typedef` para mejorar la legibilidad y modularidad de tu codigo C.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `basic_typedef.c` | Alias para tipos primitivos, structs y enums |
| `define_vs_typedef.c` | Diferencia entre `typedef` y `#define` para tipos |
| `func_pointers.c` | typedef con punteros a funcion, dispatch y callbacks |
| `stack.h` | Interfaz publica: opaque type `Stack` |
| `stack.c` | Implementacion privada: definicion completa de `struct Stack` |
| `stack_main.c` | Programa que usa `Stack` solo a traves de la API publica |

---

## Parte 1 — typedef basico

**Objetivo**: Crear alias para tipos primitivos, structs y enums. Entender la
diferencia entre `typedef` y `#define`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat basic_typedef.c
```

Observa la estructura del archivo:

- `typedef unsigned long long u64` — alias para un tipo primitivo largo
- `typedef enum { ... } Direction` — alias para un enum
- `typedef struct Point { ... } Point` — struct con tag Y alias (ambas formas)
- `typedef struct { ... } Color` — struct anonimo (solo se accede por alias)

Antes de compilar, predice:

- `sizeof(u64)` y `sizeof(unsigned long long)` daran el mismo valor?
- Se puede declarar una variable como `struct Point p2`? Y como `struct Color c`?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic basic_typedef.c -o basic_typedef
./basic_typedef
```

Salida esperada:

```
=== Primitive type aliases ===
counter (u64):         1000000
temp (Temperature):    36.6
greeting (String):     Hello from typedef
sizeof(u64):           8
sizeof(unsigned long long): 8

=== Enum typedef ===
dir = NORTH (0)

=== Struct typedef (with tag) ===
p  (Point):        (10, 20)
p2 (struct Point): (30, 40)

=== Struct typedef (anonymous) ===
red (Color): (1.0, 0.0, 0.0)
```

Verifica tus predicciones:

- `sizeof(u64)` es identico a `sizeof(unsigned long long)` — typedef no crea un
  tipo nuevo, es solo un alias.
- `struct Point p2` funciona porque `Point` tiene tag. Pero `struct Color c` no
  compilaria porque `Color` es un struct anonimo (sin tag).

### Paso 1.3 — typedef vs #define

```bash
cat define_vs_typedef.c
```

Observa las dos definiciones al inicio:

```c
typedef char *String_t;    /* typedef: el compilador entiende el tipo */
#define String_d char *    /* #define: sustitucion de texto */
```

Antes de compilar, predice:

- `String_t t_a, t_b;` — que tipo tiene `t_b`?
- `String_d d_a, d_b;` — que tipo tiene `d_b`?

Pista: `#define` hace sustitucion de texto, no analisis de tipos.

### Paso 1.4 — Verificar la prediccion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic define_vs_typedef.c -o define_vs_typedef
./define_vs_typedef
```

Salida esperada:

```
=== typedef char *String_t ===
t_a = hello
t_b = world
sizeof(t_a) = 8 (pointer)
sizeof(t_b) = 8 (pointer)

=== #define String_d char * ===
d_a = hello
d_b = 'X'
sizeof(d_a) = 8 (pointer)
sizeof(d_b) = 1 (char!)
```

Con `typedef`, ambas variables son `char *` (puntero, 8 bytes). Con `#define`,
la linea `String_d d_a, d_b;` se expande a `char *d_a, d_b;` — el `*` solo
aplica a `d_a`. La variable `d_b` queda como `char` (1 byte).

Esta es la razon principal por la que se prefiere `typedef` sobre `#define` para
alias de tipos.

### Limpieza de la parte 1

```bash
rm -f basic_typedef define_vs_typedef
```

---

## Parte 2 — typedef con punteros a funcion

**Objetivo**: Usar `typedef` para simplificar declaraciones de punteros a
funcion, crear tablas de dispatch y pasar callbacks.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat func_pointers.c
```

Observa los dos typedef de funcion:

```c
typedef int (*BinaryOp)(int, int);
typedef void (*Formatter)(const char *, int);
```

Sin typedef, una funcion que retorna un puntero a funcion seria:

```c
int (*get_operation(char op))(int, int);    /* ilegible */
```

Con typedef:

```c
BinaryOp get_operation(char op);            /* claro */
```

Antes de compilar, predice:

- `BinaryOp ops[] = {add, subtract, multiply};` — es un array de que?
- Cuando se ejecuta `ops[2](15, 4)`, que funcion se invoca y que retorna?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic func_pointers.c -o func_pointers
./func_pointers
```

Salida esperada:

```
=== Function pointer typedef: BinaryOp ===
add(15, 4) = 19
add(15, 4) = 19
subtract(15, 4) = 11
multiply(15, 4) = 60

=== Array of BinaryOp ===
ops[0](15, 4) = 19
ops[1](15, 4) = 11
ops[2](15, 4) = 60

=== Formatter callback ===
  decimal -> 60
  hex -> 0x3C
```

Observa:

- `get_operation('+')` retorna un `BinaryOp` — un puntero a funcion que se
  puede invocar directamente con `fn(a, b)`.
- `ops[]` es un array de punteros a funcion. Se puede indexar y llamar:
  `ops[i](a, b)`.
- `Formatter` se usa como callback: la funcion `show()` recibe un `Formatter`
  y lo invoca internamente. El caller decide el formato.

### Paso 2.3 — Sin typedef (comparacion mental)

Imagina declarar el array sin typedef:

```c
int (*ops[])(int, int) = {add, subtract, multiply};
```

Y la funcion de dispatch:

```c
int (*get_operation(char op))(int, int);
```

Compara con:

```c
BinaryOp ops[] = {add, subtract, multiply};
BinaryOp get_operation(char op);
```

El typedef convierte declaraciones crpticas en codigo legible. Este es su uso
mas valioso con punteros a funcion.

### Limpieza de la parte 2

```bash
rm -f func_pointers
```

---

## Parte 3 — Opaque types (encapsulacion)

**Objetivo**: Implementar un tipo opaco con `typedef struct Stack Stack` en el
header y la definicion completa en el `.c`. Verificar que el compilador impide
el acceso directo a los campos internos.

### Paso 3.1 — Examinar la interfaz publica

```bash
cat stack.h
```

Observa:

- `typedef struct Stack Stack;` — forward declaration con typedef. El usuario
  sabe que existe `Stack` pero no conoce su contenido.
- Todas las funciones reciben `Stack *` — solo punteros, nunca el struct por
  valor (el compilador no sabe el tamano).

### Paso 3.2 — Examinar la implementacion privada

```bash
cat stack.c
```

Observa:

- `struct Stack { int *data; int top; int capacity; };` — la definicion
  completa solo existe aqui.
- Este archivo incluye `stack.h` y puede trabajar con los campos internos.
- Ningun otro archivo que incluya `stack.h` puede acceder a `data`, `top` o
  `capacity`.

### Paso 3.3 — Examinar el programa principal

```bash
cat stack_main.c
```

Observa que `stack_main.c`:

- Incluye `stack.h` (no `stack.c`)
- Solo usa la API publica: `stack_create`, `stack_push`, `stack_pop`, etc.
- Tiene un comentario `s->data = 0;` que NO compilaria

Antes de compilar, predice:

- Cuantos archivos `.o` se generan al compilar?
- Si en `stack_main.c` escribieras `s->data = NULL;`, que error daria el
  compilador?

### Paso 3.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c stack.c -o stack.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c stack_main.c -o stack_main.o
gcc stack.o stack_main.o -o stack_main
./stack_main
```

Salida esperada:

```
=== Opaque type: Stack ===

Empty? yes
Size:  0

Pushing: 10 20 30
Size:  3
Empty? no

Peek:  30 (top, not removed)
Size:  3

Popping all:
  popped 30
  popped 20
  popped 10
Size:  0
Empty? yes

Stack destroyed.
```

Se generaron dos `.o` independientes. `stack_main.o` no necesita saber la
estructura interna de `Stack` — solo usa punteros y funciones publicas.

### Paso 3.5 — Verificar la encapsulacion

Intenta acceder a un campo interno desde fuera del modulo:

```bash
cat > opaque_test.c << 'EOF'
#include <stdio.h>
#include "stack.h"

int main(void) {
    Stack *s = stack_create(5);
    s->data = NULL;  /* Intentar acceder al campo interno */
    stack_destroy(s);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic opaque_test.c stack.c -o opaque_test
```

Salida esperada (error):

```
opaque_test.c: In function 'main':
opaque_test.c:6:6: error: invalid use of incomplete typedef 'Stack'
    6 |     s->data = NULL;
      |      ^~
```

El compilador rechaza el acceso porque `Stack` es un **incomplete type** fuera
de `stack.c`. Esta es la encapsulacion que proporcionan los opaque types:

- El usuario no puede leer ni modificar los campos internos
- Solo puede interactuar a traves de la API publica
- Si cambias los campos internos de `struct Stack`, solo recompilas `stack.c`

### Paso 3.6 — Verificar con nm

```bash
nm stack.o
```

Salida esperada:

```
                 U free
                 U malloc
0000000000000000 T stack_create
...              T stack_destroy
...              T stack_empty
...              T stack_peek
...              T stack_pop
...              T stack_push
...              T stack_size
```

Todos los simbolos publicos estan definidos (T). Los campos internos (`data`,
`top`, `capacity`) no aparecen como simbolos — son detalles de implementacion
que quedan dentro de las funciones.

```bash
nm stack_main.o
```

Salida esperada:

```
0000000000000000 T main
                 U printf
                 U stack_create
                 U stack_destroy
                 U stack_empty
                 U stack_peek
                 U stack_pop
                 U stack_push
                 U stack_size
```

`stack_main.o` solo conoce los nombres de las funciones (U = undefined). No
tiene referencia alguna a los campos internos del struct.

### Paso 3.7 — Compilacion en un solo comando

Tambien se puede compilar todo de una vez sin generar `.o` intermedios:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic stack.c stack_main.c -o stack_main
./stack_main
```

Mismo resultado. GCC genera los `.o` internamente y los enlaza.

### Limpieza de la parte 3

```bash
rm -f stack.o stack_main.o stack_main opaque_test.c
```

---

## Limpieza final

```bash
rm -f *.o basic_typedef define_vs_typedef func_pointers stack_main opaque_test.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  basic_typedef.c  define_vs_typedef.c  func_pointers.c  stack.c  stack.h  stack_main.c
```

---

## Conceptos reforzados

1. `typedef` crea un alias, no un tipo nuevo. `sizeof(alias)` y
   `sizeof(tipo_original)` son siempre iguales, y ambas formas son
   intercambiables en cualquier contexto.

2. Un struct con tag y typedef (`typedef struct Point { ... } Point`) permite
   usar ambas formas (`Point p` y `struct Point p`). Un struct anonimo
   (`typedef struct { ... } Color`) solo permite el alias.

3. `typedef` y `#define` no son equivalentes para alias de tipos.
   `typedef char *String` hace que `String a, b` declare dos punteros.
   `#define String char *` hace que `String a, b` declare un puntero y un char.

4. `typedef` simplifica radicalmente las declaraciones con punteros a funcion.
   `typedef int (*BinaryOp)(int, int)` permite declarar variables, parametros,
   arrays y retornos con el nombre `BinaryOp` en lugar de la sintaxis completa.

5. Un opaque type (`typedef struct Foo Foo` en el header, definicion en el `.c`)
   impide que el codigo externo acceda a los campos del struct. El compilador da
   "invalid use of incomplete typedef" si se intenta `s->campo`.

6. Con opaque types, cambiar los campos internos del struct solo requiere
   recompilar el `.c` de implementacion. El codigo que usa la API publica no se
   ve afectado — esto es estabilidad de ABI en C.
