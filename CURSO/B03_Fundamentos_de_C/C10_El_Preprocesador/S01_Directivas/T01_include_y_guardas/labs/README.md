# Lab — #include y guardas

## Objetivo

Observar como funciona `#include` internamente (copia de texto), distinguir
`< >` de `" "`, usar `-I` para agregar directorios de busqueda, proteger
headers con include guards y `#pragma once`, y usar `gcc -E` para inspeccionar
la expansion del preprocesador.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `point.h` | Header con include guard |
| `point_noguard.h` | Header sin include guard (demuestra el problema) |
| `point.c` | Implementacion de point.h |
| `shape.h` | Header que incluye point.h (inclusion transitiva) |
| `shape.c` | Implementacion de shape.h |
| `guards_ok.c` | Incluye point.h directa e indirectamente |
| `noguard_fail.c` | Incluye point_noguard.h dos veces |
| `pragma_once_demo.h` | Header con `#pragma once` |
| `pragma_once_demo.c` | Incluye pragma_once_demo.h dos veces |
| `search_path.c` | Usa `#include <greeting.h>` con `-I` |
| `preprocess_inspect.c` | Para inspeccionar con `gcc -E` |
| `myincludes/greeting.h` | Header en directorio separado |

---

## Parte 1 — Inclusion doble sin guardas

**Objetivo**: Ver el error que produce incluir un header sin guardas dos veces.

### Paso 1.1 — Examinar el header sin guardas

```bash
cat point_noguard.h
```

Observa: `point_noguard.h` define `struct PointNG` pero no tiene
`#ifndef`/`#define`/`#endif` ni `#pragma once`. No hay nada que impida que su
contenido se copie multiples veces.

### Paso 1.2 — Examinar el programa que lo incluye dos veces

```bash
cat noguard_fail.c
```

Observa: el archivo incluye `point_noguard.h` dos veces consecutivas. Cuando
el preprocesador expanda ambos `#include`, el `struct PointNG` quedara definido
dos veces en la misma unidad de traduccion.

### Paso 1.3 — Predecir el resultado

Antes de compilar, responde mentalmente:

- El preprocesador copia el contenido del header en cada `#include`. Si el
  header define un `struct`, que pasa cuando la definicion aparece dos veces?
- El error sera en tiempo de preprocesamiento, compilacion, o enlazado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 — Compilar y observar el error

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic noguard_fail.c -o noguard_fail
```

Salida esperada:

```
In file included from noguard_fail.c:4:
point_noguard.h:3:8: error: redefinition of 'struct PointNG'
    3 | struct PointNG {
      |        ^~~~~~~
In file included from noguard_fail.c:3:
point_noguard.h:3:8: note: originally defined here
    3 | struct PointNG {
      |        ^~~~~~~
```

El compilador detecta que `struct PointNG` se define dos veces. Esto es un
error en C -- un tipo solo puede definirse una vez por unidad de traduccion
(regla del "one definition rule"). En proyectos reales, este error suele
ocurrir de forma indirecta: `a.h` incluye `common.h`, `b.h` tambien incluye
`common.h`, y `main.c` incluye ambos.

---

## Parte 2 — Include guards

**Objetivo**: Proteger un header con include guards para que la inclusion doble
(directa o transitiva) no cause errores.

### Paso 2.1 — Examinar el header con guardas

```bash
cat point.h
```

Observa el patron:

- `#ifndef POINT_H` -- si la macro `POINT_H` **no** esta definida, continuar
- `#define POINT_H` -- definir la macro inmediatamente
- (contenido del header)
- `#endif` -- fin del bloque condicional

La primera vez que se incluye, `POINT_H` no existe, asi que se procesa todo y
se define la macro. La segunda vez, `POINT_H` ya existe y `#ifndef` es falso:
se salta todo el contenido hasta `#endif`.

### Paso 2.2 — Examinar la inclusion transitiva

```bash
cat shape.h
```

`shape.h` incluye `point.h` porque necesita `struct Point` para definir
`struct Rectangle`. Si `main.c` incluye tanto `point.h` como `shape.h`,
`point.h` se procesaria dos veces sin guardas.

### Paso 2.3 — Examinar el programa que incluye ambos

```bash
cat guards_ok.c
```

Observa: `guards_ok.c` incluye `point.h` directamente Y `shape.h` (que
tambien incluye `point.h`). Esto significa que `point.h` se "incluye" dos
veces.

### Paso 2.4 — Predecir el resultado

Antes de compilar, responde mentalmente:

- Cuando el preprocesador procesa el primer `#include "point.h"`, la macro
  `POINT_H` existe o no?
- Cuando procesa el segundo (dentro de `shape.h`), que pasa con `#ifndef`?
- Compilara sin errores?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.5 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic guards_ok.c point.c shape.c -o guards_ok
```

No debe producir warnings ni errores. Los include guards evitaron la
redefinicion.

```bash
./guards_ok
```

Salida esperada:

```
Punto creado: Point(3, 7)
Rectangulo: Rectangle at (1, 2), 10x5
```

La inclusion doble de `point.h` no causo problemas. La primera inclusion
proceso todo el contenido y definio `POINT_H`. La segunda fue ignorada
completamente por `#ifndef POINT_H`.

### Limpieza de la parte 2

```bash
rm -f guards_ok
```

---

## Parte 3 — #pragma once

**Objetivo**: Usar `#pragma once` como alternativa a los include guards
tradicionales.

### Paso 3.1 — Examinar el header con #pragma once

```bash
cat pragma_once_demo.h
```

Observa: en lugar de `#ifndef`/`#define`/`#endif`, el header usa una sola
linea: `#pragma once`. El compilador recuerda que ya proceso este archivo y no
lo vuelve a incluir.

### Paso 3.2 — Examinar el programa

```bash
cat pragma_once_demo.c
```

Observa: incluye `pragma_once_demo.h` dos veces explicitamente.

### Paso 3.3 — Predecir el resultado

Antes de compilar, responde mentalmente:

- `#pragma once` es parte del estandar C11?
- Compilara sin errores a pesar de la doble inclusion?
- Habra algun warning por usar `#pragma once` con `-Wpedantic`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic pragma_once_demo.c -o pragma_once_demo
```

Salida esperada: sin errores ni warnings. Aunque `#pragma once` no es estandar
C, GCC no genera warnings por pragmas desconocidos (el estandar dice que los
pragmas no reconocidos se ignoran, y GCC reconoce `once`).

```bash
./pragma_once_demo
```

Salida esperada:

```
Incluido dos veces con #pragma once, sin error.
Color(r=255, g=0, b=0)
```

### Paso 3.5 — Comparacion: include guards vs #pragma once

| Aspecto | Include guards | `#pragma once` |
|---------|---------------|----------------|
| Estandar C | Si | No (extension) |
| Soporte | Todos los compiladores | GCC, Clang, MSVC |
| Lineas | 3 (`#ifndef`, `#define`, `#endif`) | 1 |
| Riesgo de colision de nombres | Si (macros duplicadas) | No |
| Problemas con symlinks | No | Si (mismo archivo, dos paths) |

En la practica, ambos son aceptables. `#pragma once` es mas simple.
Los include guards son la opcion segura para codigo que debe ser estrictamente
compatible con el estandar.

### Limpieza de la parte 3

```bash
rm -f pragma_once_demo
```

---

## Parte 4 — #include < > vs " " y rutas de busqueda con -I

**Objetivo**: Entender la diferencia entre `#include <header.h>` y
`#include "header.h"`, y usar `-I` para agregar directorios de busqueda.

### Paso 4.1 — Examinar la estructura de directorios

```bash
ls myincludes/
```

Salida esperada:

```
greeting.h
```

```bash
cat myincludes/greeting.h
```

El header `greeting.h` esta en el subdirectorio `myincludes/`, no en el
directorio actual.

### Paso 4.2 — Examinar el programa

```bash
cat search_path.c
```

Observa: usa `#include <greeting.h>` (con angulares, no comillas). Esto
significa que el preprocesador **no** buscara en el directorio actual -- solo
en los directorios del sistema y los agregados con `-I`.

### Paso 4.3 — Predecir el resultado

Antes de compilar, responde mentalmente:

- Sin `-I`, el compilador encontrara `greeting.h`?
- Con `-Imyincludes`, que directorio se agrega a la ruta de busqueda?
- Si cambiaras `<greeting.h>` por `"greeting.h"`, cambiaria algo sin `-I`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 — Compilar sin -I (debe fallar)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic search_path.c -o search_path
```

Salida esperada:

```
search_path.c:4:10: fatal error: greeting.h: No such file or directory
    4 | #include <greeting.h>
      |          ^~~~~~~~~~~~
compilation terminated.
```

El preprocesador busco `greeting.h` en los directorios del sistema y no lo
encontro. No busca en el directorio actual porque se uso `< >`.

### Paso 4.5 — Compilar con -I (debe funcionar)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Imyincludes search_path.c -o search_path
```

Compila sin errores. `-Imyincludes` agrego el directorio `myincludes/` a las
rutas de busqueda, y el preprocesador encontro `greeting.h` ahi.

```bash
./search_path
```

Salida esperada:

```
Este programa usa un header de un directorio personalizado.
Hello, Preprocesador!
```

### Paso 4.6 — Ver las rutas de busqueda del compilador

```bash
gcc -E -v search_path.c -Imyincludes 2>&1 | grep -A 15 "search starts here"
```

Salida esperada (los paths exactos varian):

```
#include "..." search starts here:
#include <...> search starts here:
 myincludes
 /usr/lib/gcc/x86_64-linux-gnu/<version>/include
 /usr/local/include
 /usr/include/x86_64-linux-gnu
 /usr/include
End of search list.
```

Observa:

- La seccion `#include "..."` muestra los directorios donde se buscan los
  includes con comillas (primero el directorio del archivo fuente)
- La seccion `#include <...>` muestra los directorios para angulares
- `myincludes` aparece al principio de la lista de `< >` porque se agrego
  con `-I`

### Limpieza de la parte 4

```bash
rm -f search_path
```

---

## Parte 5 — gcc -E: inspeccion del preprocesador

**Objetivo**: Usar `gcc -E` para ver exactamente que produce el preprocesador
y entender que `#include` es una copia literal de texto.

### Paso 5.1 — Examinar el programa

```bash
cat preprocess_inspect.c
```

Es un programa minimo que incluye `<stdio.h>` y `"point.h"`.

### Paso 5.2 — Predecir el resultado

Antes de ejecutar `gcc -E`, responde mentalmente:

- `preprocess_inspect.c` tiene ~8 lineas de codigo. Despues de expandir los
  `#include`, cuantas lineas tendra la salida? (pista: `<stdio.h>` es grande)
- El contenido de `point.h` aparecera literalmente en la salida?
- Las lineas `#ifndef`/`#define`/`#endif` de `point.h` aparecen en la salida?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 — Ejecutar gcc -E y contar lineas

```bash
gcc -E preprocess_inspect.c 2>/dev/null | wc -l
```

Salida esperada:

```
~860
```

De 8 lineas de codigo se expandieron a cientos. La mayor parte viene de
`<stdio.h>` y sus includes transitivos.

### Paso 5.4 — Ejecutar gcc -E -P (sin marcadores de linea)

```bash
gcc -E -P preprocess_inspect.c 2>/dev/null | wc -l
```

Salida esperada:

```
~318
```

La opcion `-P` elimina los marcadores de linea (`# 1 "stdio.h"`, etc.) que
el compilador usa para reportar errores con el archivo y linea originales.
Sin esos marcadores, la salida es mas corta pero aun asi enorme comparada
con el codigo original.

### Paso 5.5 — Ver el final de la expansion

```bash
gcc -E -P preprocess_inspect.c 2>/dev/null | tail -20
```

Salida esperada (los atributos exactos varian segun la version de GCC):

```
...
struct Point {
    int x;
    int y;
};
struct Point point_create(int x, int y);
void point_print(const struct Point *p);
int main(void) {
    struct Point p = { 1, 2 };
    printf("Point(%d, %d)\n", p.x, p.y);
    return 0;
}
```

Observa:

- El contenido de `point.h` aparece **literalmente** copiado (sin los
  `#ifndef`/`#define`/`#endif` -- esas directivas fueron procesadas y
  eliminadas)
- `#include <stdio.h>` se expandio a cientos de lineas de declaraciones
  de funciones, tipos, y macros del sistema
- Tu codigo original queda al final, despues de toda la expansion

### Paso 5.6 — Comparar: con y sin un include

Para ver cuantas lineas agrega cada `#include`, compara:

```bash
echo 'int main(void) { return 0; }' > /tmp/empty_test.c
gcc -E -P /tmp/empty_test.c 2>/dev/null | wc -l
```

Salida esperada:

```
1
```

Sin ningun `#include`, la salida es solo tu codigo. Cada `#include` agrega
cientos de lineas. Esto es por lo que los tiempos de compilacion en C crecen
con los headers incluidos: cada unidad de traduccion reprocesa todo.

```bash
rm -f /tmp/empty_test.c
```

---

## Limpieza final

```bash
rm -f guards_ok pragma_once_demo search_path preprocess_inspect
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md            noguard_fail.c       point_noguard.h      search_path.c
guards_ok.c          point.c              pragma_once_demo.c   shape.c
myincludes           point.h              pragma_once_demo.h   shape.h
                                          preprocess_inspect.c
```

---

## Conceptos reforzados

1. `#include` es una sustitucion de texto: el preprocesador copia el contenido
   completo del archivo en el punto de inclusion. No hay "importar" ni
   "modulos" -- es copia literal.

2. Sin include guards, incluir un header dos veces (directa o transitivamente)
   produce errores de redefinicion. Este es el caso comun en proyectos reales
   donde multiples headers incluyen un header compartido.

3. Include guards (`#ifndef`/`#define`/`#endif`) resuelven la inclusion doble:
   la primera inclusion define una macro, y la segunda es ignorada porque la
   macro ya existe. El nombre de la macro debe ser unico en todo el proyecto.

4. `#pragma once` es una alternativa no estandar que logra lo mismo con una
   sola linea. Es soportada por GCC, Clang, y MSVC. Es mas simple pero puede
   fallar con symlinks.

5. `#include <header.h>` busca solo en directorios del sistema y los agregados
   con `-I`. `#include "header.h"` busca primero en el directorio del archivo
   fuente y luego en los mismos directorios que `< >`.

6. `-I` agrega directorios a la ruta de busqueda del preprocesador. Es la
   forma estandar de organizar proyectos con headers en subdirectorios
   (`-Iinclude`, `-Isrc/lib`, etc.).

7. `gcc -E` muestra la salida del preprocesador antes de la compilacion. Un
   archivo de 8 lineas puede expandirse a cientos o miles despues de procesar
   los `#include`. Esto explica por que los tiempos de compilacion en C crecen
   con cada header incluido.

8. Las directivas del preprocesador (`#ifndef`, `#define`, `#endif`,
   `#pragma`) no aparecen en la salida de `gcc -E`: son procesadas y
   eliminadas. Solo queda el texto resultante.
