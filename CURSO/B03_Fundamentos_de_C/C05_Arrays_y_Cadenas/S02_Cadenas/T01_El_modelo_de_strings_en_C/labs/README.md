# Lab — El modelo de strings en C

## Objetivo

Experimentar con el modelo de null-terminated strings de C: visualizar como se
almacenan en memoria, entender la diferencia entre `char[]` y `const char *`,
implementar operaciones de string manualmente, e identificar los errores mas
comunes. Al finalizar, sabras interpretar la representacion interna de un string
y evitar los patrones que producen undefined behavior.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `null_term.c` | Demuestra `sizeof` vs `strlen` y visualiza bytes en memoria |
| `arr_vs_ptr.c` | Diferencias entre `char[]` (stack) y `const char *` (.rodata) |
| `manual_ops.c` | Implementacion manual de `strlen`, `strcmp`, `strcpy` |
| `common_errors.c` | Errores clasicos: `\0` faltante, `sizeof` de puntero, `==` en strings |

---

## Parte 1 — Null-terminated strings

**Objetivo**: Entender que un string en C es un array de `char` terminado en
`\0`, y que `sizeof` y `strlen` reportan valores distintos.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat null_term.c
```

Observa:

- Tres formas equivalentes de declarar `"hello"`: con string literal, con
  inicializador char por char, y con tamano explicito
- El programa imprime `sizeof` y `strlen` para cada una
- Luego recorre los bytes del string uno por uno, incluyendo el `\0` final

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic null_term.c -o null_term
```

Antes de ejecutar, predice:

- `sizeof(s1)` para `"hello"` sera 5 o 6? Por que?
- `strlen(s1)` sera 5 o 6?
- Que valor decimal tendra el ultimo byte del string?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 — Verificar la prediccion

```bash
./null_term
```

Salida esperada:

```
=== sizeof vs strlen ===
s1: sizeof = 6, strlen = 5
s2: sizeof = 6, strlen = 5
s3: sizeof = 6, strlen = 5

=== Byte-level view of s1 ===
s1[0] = 'h' (decimal: 104, hex: 0x68)
s1[1] = 'e' (decimal: 101, hex: 0x65)
s1[2] = 'l' (decimal: 108, hex: 0x6c)
s1[3] = 'l' (decimal: 108, hex: 0x6c)
s1[4] = 'o' (decimal: 111, hex: 0x6f)
s1[5] = '?' (decimal: 0, hex: 0x00)

=== Null terminator identity ===
'\0' == 0  : true
'0'  == 48 : true
```

Analisis:

- `sizeof` retorna **6**: los 5 caracteres mas el terminador `\0`
- `strlen` retorna **5**: cuenta caracteres hasta `\0`, sin incluirlo
- El byte `s1[5]` tiene valor decimal 0 (es `\0`, no el caracter `'0'` que es 48)
- Las tres declaraciones producen exactamente el mismo array en memoria

### Paso 1.4 — Observar el string literal en .rodata

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -S null_term.c -o null_term.s
grep -A1 '\.string\|\.ascii' null_term.s | head -30
```

Busca las cadenas que el compilador coloca en la seccion de solo lectura. Veras
los string literals del programa (los de `printf` y las cadenas `"hello"`) como
directivas `.string` o `.ascii` en el assembly.

### Limpieza de la parte 1

```bash
rm -f null_term null_term.s
```

---

## Parte 2 — char[] vs const char *

**Objetivo**: Entender la diferencia entre un array de char (copia modificable
en el stack) y un puntero a string literal (apunta a `.rodata`, solo lectura).

### Paso 2.1 — Examinar el codigo fuente

```bash
cat arr_vs_ptr.c
```

Observa:

- `char arr[]` se inicializa con un string literal -- el compilador copia los
  bytes al stack
- `const char *ptr` apunta directamente al literal en `.rodata`
- El programa modifica `arr[0]` y reasigna `ptr`

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arr_vs_ptr.c -o arr_vs_ptr
```

Antes de ejecutar, predice:

- `sizeof(arr)` sera 6 u 8?
- `sizeof(ptr)` sera 6 u 8?
- Despues de `arr[0] = 'H'`, que imprimira `arr`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 — Verificar la prediccion

```bash
./arr_vs_ptr
```

Salida esperada:

```
=== sizeof difference ===
sizeof(arr) = 6  (array of 6 chars)
sizeof(ptr) = 8  (size of a pointer)

=== strlen is the same ===
strlen(arr) = 5
strlen(ptr) = 5

=== Modifying the array ===
arr after arr[0] = 'H': "Hello"

=== Reassigning the pointer ===
ptr before: "hello"
ptr after ptr = "world": "world"
```

Analisis:

- `sizeof(arr)` es **6** porque `arr` es un array de 6 bytes (5 chars + `\0`)
- `sizeof(ptr)` es **8** porque `ptr` es un puntero (8 bytes en 64-bit)
- `strlen` retorna 5 en ambos casos: la longitud del contenido es la misma
- `arr[0] = 'H'` funciona porque `arr` es una copia en el stack
- `ptr = "world"` funciona porque se reasigna a donde apunta el puntero
- `arr = "world"` no compilaria: un array no es reasignable

### Paso 2.4 — Verificar que el literal vive en .rodata

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c arr_vs_ptr.c -o arr_vs_ptr.o
objdump -s -j .rodata arr_vs_ptr.o
```

Salida esperada (los offsets pueden variar):

```
Contents of section .rodata:
 0000 68656c6c 6f00...
```

Los bytes `68 65 6c 6c 6f 00` son `h e l l o \0`. Los string literals que se
usan con `const char *` viven aqui, en memoria de solo lectura. Intentar
modificarlos provoca un segfault en la mayoria de sistemas.

### Paso 2.5 — Resumen visual

```
char arr[] = "hello";          const char *ptr = "hello";

Stack:                         Stack:            .rodata:
+---+---+---+---+---+---+     +--------+        +---+---+---+---+---+---+
| h | e | l | l | o |\0 |     | 0x...  |------->| h | e | l | l | o |\0 |
+---+---+---+---+---+---+     +--------+        +---+---+---+---+---+---+
  arr (6 bytes, modificable)     ptr (8 bytes)     (solo lectura)
```

### Limpieza de la parte 2

```bash
rm -f arr_vs_ptr arr_vs_ptr.o
```

---

## Parte 3 — Operaciones manuales sin string.h

**Objetivo**: Implementar `strlen`, `strcmp` y `strcpy` manualmente para
entender como funcionan internamente: recorren el array byte por byte hasta
encontrar `\0`.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat manual_ops.c
```

Observa las tres funciones:

- `my_strlen`: un contador que avanza hasta encontrar `\0`
- `my_strcmp`: compara char por char y retorna la diferencia del primer par
  distinto
- `my_strcpy`: copia char por char y agrega `\0` al final

Nota que ninguna usa `#include <string.h>`. Todo se basa en recorrer hasta `\0`.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic manual_ops.c -o manual_ops
```

Antes de ejecutar, predice:

- `my_strlen("hello\0world")` retornara 5, 11, o 12?
- `my_strcmp("abc", "abc")` retornara 0, 1, o -1?
- `my_strcmp("ab", "abc")` sera positivo o negativo?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 — Verificar la prediccion

```bash
./manual_ops
```

Salida esperada:

```
=== my_strlen ===
my_strlen("")            = 0
my_strlen("a")           = 1
my_strlen("hello")       = 5
my_strlen("hello\0world") = 5

=== my_strcmp ===
my_strcmp("abc", "abc") = 0
my_strcmp("abc", "abd") = -1
my_strcmp("abd", "abc") = 1
my_strcmp("ab",  "abc") = -99

=== my_strcpy ===
buf after my_strcpy: "copied!"
```

Analisis:

- `my_strlen("hello\0world")` retorna **5**: el `\0` embebido termina el conteo.
  Los bytes `world` existen en memoria pero son invisibles para cualquier
  funcion de string
- `my_strcmp("abc", "abc")` retorna **0**: todos los caracteres son iguales
- `my_strcmp("abc", "abd")` retorna **-1**: `'c' - 'd'` = 99 - 100 = -1
- `my_strcmp("ab", "abc")` retorna **-99**: `'\0' - 'c'` = 0 - 99 = -99. El
  string mas corto termina primero, y `\0` (valor 0) se resta con el caracter
  correspondiente del otro string. El valor exacto puede variar segun la
  plataforma, pero siempre sera negativo
- `my_strcpy` copia correctamente incluyendo el `\0` final

### Limpieza de la parte 3

```bash
rm -f manual_ops
```

---

## Parte 4 — Errores comunes

**Objetivo**: Identificar y entender los errores mas frecuentes al trabajar con
strings en C.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat common_errors.c
```

El programa demuestra cuatro errores clasicos. Lee el codigo y, para cada
seccion, predice que imprimira.

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic common_errors.c -o common_errors
./common_errors
```

Salida esperada:

```
=== Error 1: Missing null terminator ===
With '\0': "hello" (strlen = 5)

=== Error 2: sizeof pointer vs array ===
sizeof(arr) = 6  <-- size of the array (6 bytes)
sizeof(ptr) = 8  <-- size of the pointer (8 bytes on 64-bit)
strlen(arr) = 5
strlen(ptr) = 5

=== Error 3: == compares pointers, not content ===
&a[0] == &b[0]   : false
strcmp(a, b) == 0: true

=== Error 4: Embedded null ===
printf: "hello"
strlen: 5
sizeof: 12  <-- compiler knows full size (12)
```

### Paso 4.3 — Analisis de cada error

**Error 1 -- Olvidar el terminador `\0`**

Si se construye un char array manualmente sin poner `\0` al final, `printf("%s")`
y `strlen()` seguiran leyendo mas alla del buffer hasta encontrar un byte 0 en
algun lugar arbitrario de la memoria. Esto es undefined behavior. El programa
solo muestra el caso correcto (con `\0`); el caso sin terminador no se ejecuta
porque su comportamiento es impredecible.

**Error 2 -- sizeof de puntero vs array**

`sizeof(arr)` reporta el tamano del array completo (6 bytes). Pero
`sizeof(ptr)` reporta el tamano del puntero (8 bytes en 64-bit), **no** la
longitud del string. Si se pasa un array a una funcion como parametro, el
parametro recibe un puntero, y `sizeof` dentro de la funcion dara 8, no el
tamano original del array. Para obtener la longitud del contenido, usar siempre
`strlen`.

**Error 3 -- Comparar strings con ==**

`==` compara las **direcciones** de memoria, no el contenido. Dos arrays `a` y
`b` con el mismo contenido `"hello"` viven en posiciones distintas del stack, asi
que `&a[0] == &b[0]` es `false`. Para comparar contenido, usar `strcmp()`. Un
retorno de 0 significa que son iguales.

**Error 4 -- Null embebido trunca el string**

`"hello\0world"` tiene 12 bytes en total (incluyendo el `\0` final que agrega
el compilador). Pero `printf` y `strlen` solo ven `"hello"` porque paran en el
primer `\0`. `sizeof` reporta 12 porque el compilador conoce el tamano real del
array en tiempo de compilacion. Para datos binarios que contienen bytes 0, se
debe usar `memcpy`/`memcmp` con longitud explicita.

### Limpieza de la parte 4

```bash
rm -f common_errors
```

---

## Limpieza final

```bash
rm -f null_term arr_vs_ptr manual_ops common_errors
rm -f *.o *.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  arr_vs_ptr.c  common_errors.c  manual_ops.c  null_term.c
```

---

## Conceptos reforzados

1. Un string en C es un array de `char` terminado en `\0` (byte 0). No existe
   un tipo `string` nativo -- es una convencion que todas las funciones de la
   biblioteca estandar siguen.

2. `sizeof` retorna el tamano total del array incluyendo `\0`. `strlen` retorna
   la cantidad de caracteres antes de `\0`. Confundir ambos es una fuente comun
   de off-by-one errors.

3. `char arr[] = "hello"` crea una **copia** del literal en el stack, que se
   puede modificar. `const char *ptr = "hello"` apunta al literal en `.rodata`,
   que es de solo lectura -- modificarlo es undefined behavior.

4. `sizeof` aplicado a un puntero retorna el tamano del puntero (8 bytes en
   64-bit), **no** la longitud del string. Cuando un array se pasa como
   parametro a una funcion, decae a puntero y `sizeof` pierde la informacion
   del tamano original.

5. Todas las funciones de string (`strlen`, `strcpy`, `strcmp`, `printf %s`)
   recorren el array byte por byte hasta encontrar `\0`. Si el terminador no
   existe, leen mas alla del buffer -- undefined behavior.

6. El operador `==` entre strings compara **direcciones**, no contenido. Para
   comparar contenido se usa `strcmp()`, que retorna 0 si son iguales.

7. Un `\0` embebido dentro de un string trunca efectivamente el string para
   todas las funciones de string, aunque `sizeof` reporte el tamano real del
   array. Para datos binarios con bytes 0, usar `memcpy`/`memcmp` con longitud
   explicita.
