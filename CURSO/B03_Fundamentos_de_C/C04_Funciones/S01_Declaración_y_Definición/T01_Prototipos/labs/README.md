# Lab -- Prototipos

## Objetivo

Experimentar con prototipos de funciones en C: ver que pasa cuando faltan,
como organizarlos en headers con include guards, y que errores detecta el
compilador gracias a ellos. Al finalizar, sabras por que los prototipos existen
y como usarlos correctamente en proyectos multi-archivo.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `with_prototype.c` | Programa con prototipos antes de main |
| `no_prototype.c` | Programa sin prototipos (provoca error) |
| `def_before_use.c` | Definicion antes del uso (sin prototipo necesario) |
| `math_ops.h` | Header con prototipos e include guard |
| `math_ops.c` | Implementacion de funciones matematicas |
| `main_multi.c` | Programa principal que usa math_ops.h |
| `no_header.c` | main sin incluir el header (provoca error) |
| `empty_vs_void.c` | Diferencia entre `()` y `(void)` en prototipos |
| `bar_wrong_args.c` | Llamar con argumentos a funcion `(void)` |
| `wrong_signature.c` | Numero incorrecto de argumentos |
| `wrong_type.c` | Tipo incorrecto de argumento |

---

## Parte 1 -- Declaracion vs definicion

**Objetivo**: Entender por que se necesitan prototipos, que pasa cuando faltan,
y la alternativa de definir antes de usar.

### Paso 1.1 -- Compilar con prototipos

```bash
cat with_prototype.c
```

Observa la estructura: los prototipos de `add` y `multiply` estan declarados
antes de `main`. Las definiciones estan despues.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic with_prototype.c -o with_prototype
./with_prototype
```

Salida esperada:

```
add(3, 4) = 7
multiply(5, 6) = 30
```

Compila sin warnings. El compilador conoce la firma de ambas funciones antes de
que main las llame, gracias a los prototipos.

### Paso 1.2 -- Predecir: sin prototipos

Ahora examina el archivo sin prototipos:

```bash
cat no_prototype.c
```

Antes de compilar, predice:

- `main` llama a `add` y `multiply` antes de que esten definidas.
- El compilador procesa de arriba a abajo. Cuando llega a `add(3, 4)` en la
  linea 4, no ha visto ninguna declaracion de `add`.
- Con `-std=c11 -Wall`, que pasara? Un warning o un error?

### Paso 1.3 -- Compilar sin prototipos

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic no_prototype.c -o no_prototype 2>&1
```

Salida esperada:

```
no_prototype.c: In function 'main':
no_prototype.c:4:32: error: implicit declaration of function 'add' [-Wimplicit-function-declaration]
no_prototype.c:5:37: error: implicit declaration of function 'multiply' [-Wimplicit-function-declaration]
```

En C99+ con `-Wall`, GCC trata la declaracion implicita como error. El
compilador no puede verificar los tipos de los argumentos ni el tipo de retorno
si no conoce la funcion.

En C89, esto era solo un warning: el compilador asumia que la funcion retornaba
`int` y aceptaba cualquier argumento. Esto causaba bugs silenciosos, y por eso
C99 lo prohibio.

### Paso 1.4 -- Alternativa: definir antes de usar

```bash
cat def_before_use.c
```

Observa: `add` y `multiply` estan definidas **antes** de `main`. No hay
prototipos.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic def_before_use.c -o def_before_use
./def_before_use
```

Salida esperada:

```
add(3, 4) = 7
multiply(5, 6) = 30
```

Compila sin warnings. Cuando el compilador llega a `add(3, 4)` en main, ya
proceso la definicion de `add` y conoce su firma.

Esto funciona para programas de un solo archivo, pero en proyectos con
multiples archivos se necesitan prototipos en headers (parte 2).

### Limpieza de la parte 1

```bash
rm -f with_prototype no_prototype def_before_use
```

---

## Parte 2 -- Headers y compilacion multi-archivo

**Objetivo**: Organizar prototipos en un header (.h), entender los include
guards, y compilar un proyecto con multiples archivos fuente.

### Paso 2.1 -- Examinar el header

```bash
cat math_ops.h
```

Observa tres cosas:

1. `#ifndef MATH_OPS_H` / `#define MATH_OPS_H` / `#endif` -- el include guard
2. Solo declaraciones (prototipos), no definiciones
3. Los nombres de parametros estan incluidos -- sirven como documentacion

### Paso 2.2 -- Examinar la implementacion

```bash
cat math_ops.c
```

Observa: `math_ops.c` incluye su propio header (`#include "math_ops.h"`). Esto
permite que el compilador verifique que los prototipos del header coincidan con
las definiciones. Si hay una discrepancia (por ejemplo, cambias un tipo en el
header pero no en el .c), el compilador lo detectara.

### Paso 2.3 -- Examinar el programa principal

```bash
cat main_multi.c
```

`main_multi.c` incluye `math_ops.h` para conocer los prototipos. No necesita
saber como estan implementadas las funciones -- solo necesita saber que existen,
que tipo retornan, y que parametros aceptan.

### Paso 2.4 -- Compilar multi-archivo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic math_ops.c main_multi.c -o calculator
./calculator
```

Salida esperada:

```
10 + 3 = 13
10 - 3 = 7
10 * 3 = 30
```

GCC compila cada .c por separado y luego enlaza los .o resultantes.

### Paso 2.5 -- Predecir: sin el header

Ahora examina `no_header.c`:

```bash
cat no_header.c
```

Es identico a `main_multi.c` pero sin `#include "math_ops.h"`. Antes de
compilar, predice:

- Las funciones `add`, `subtract` y `multiply` estan definidas en
  `math_ops.c`. El enlazador las encontrara.
- Pero el compilador, al procesar `no_header.c`, no ha visto ninguna
  declaracion de esas funciones.
- Que sucedera: error de compilacion o error de enlace?

### Paso 2.6 -- Compilar sin el header

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic math_ops.c no_header.c -o no_header 2>&1
```

Salida esperada:

```
no_header.c: In function 'main':
no_header.c:8:36: error: implicit declaration of function 'add' [-Wimplicit-function-declaration]
no_header.c:9:36: error: implicit declaration of function 'subtract' [-Wimplicit-function-declaration]
no_header.c:10:36: error: implicit declaration of function 'multiply' [-Wimplicit-function-declaration]
```

Error de compilacion, no de enlace. Aunque las funciones existen en otro
archivo, el compilador procesa cada .c de forma independiente. Sin el header,
`no_header.c` no tiene declaraciones de las funciones.

### Paso 2.7 -- Include guards en accion

Los include guards evitan problemas cuando un header se incluye mas de una vez.
Usa el preprocesador para ver la diferencia:

```bash
gcc -E main_multi.c | tail -20
```

Observa que los prototipos de `math_ops.h` aparecen una sola vez. Ahora
verifiquemos que pasa si incluimos el header dos veces. Crea un archivo
temporal:

```bash
cat > double_include_test.c << 'EOF'
#include <stdio.h>
#include "math_ops.h"
#include "math_ops.h"

int main(void) {
    printf("add(2, 3) = %d\n", add(2, 3));
    return 0;
}
EOF
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic math_ops.c double_include_test.c -o double_include_test
./double_include_test
```

Salida esperada:

```
add(2, 3) = 5
```

Compila sin errores. Aunque incluimos `math_ops.h` dos veces, el include guard
(`#ifndef MATH_OPS_H`) hace que la segunda inclusion sea ignorada.

Para comprobarlo, examina la salida del preprocesador:

```bash
gcc -E double_include_test.c | grep -c "int add"
```

Salida esperada:

```
1
```

Solo una declaracion de `add`, a pesar del doble `#include`. Sin el include
guard, la segunda inclusion duplicaria todas las declaraciones.

### Limpieza de la parte 2

```bash
rm -f calculator no_header double_include_test double_include_test.c
```

---

## Parte 3 -- Errores clasicos

**Objetivo**: Ver que errores detecta el compilador gracias a los prototipos, y
entender la diferencia critica entre `()` y `(void)`.

### Paso 3.1 -- Predecir: numero incorrecto de argumentos

Examina el archivo:

```bash
cat wrong_signature.c
```

El prototipo declara `int add(int a, int b)` -- dos parametros. Pero main
llama a `add(5)` (uno) y `add(1, 2, 3)` (tres).

Antes de compilar, predice: el compilador dara un warning o un error?

### Paso 3.2 -- Compilar con firma incorrecta

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic wrong_signature.c -o wrong_signature 2>&1
```

Salida esperada:

```
wrong_signature.c: In function 'main':
wrong_signature.c:8:29: error: too few arguments to function 'add'; expected 2, have 1
wrong_signature.c:11:35: error: too many arguments to function 'add'; expected 2, have 3
```

Error, no warning. El compilador conoce la firma exacta gracias al prototipo y
rechaza las llamadas incorrectas. Sin prototipo, estos errores pasarian
desapercibidos.

### Paso 3.3 -- Tipo incorrecto de argumento

```bash
cat wrong_type.c
```

El prototipo dice `int add(int a, int b)` pero main pasa `"hello"` (un
`char *`) como primer argumento.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic wrong_type.c -o wrong_type 2>&1
```

Salida esperada:

```
wrong_type.c:7:44: error: passing argument 1 of 'add' makes integer from pointer without a cast [-Wint-conversion]
wrong_type.c:3:13: note: expected 'int' but argument is of type 'char *'
```

El compilador detecta que `"hello"` es un `char *` y `add` espera un `int`.
Esta verificacion de tipos solo es posible porque el prototipo declara los
tipos de los parametros.

### Paso 3.4 -- Predecir: () vs (void)

Ahora la diferencia critica. Examina el archivo:

```bash
cat empty_vs_void.c
```

Observa las dos declaraciones:

- `int foo();` -- parentesis vacios
- `int bar(void);` -- explicitamente cero parametros

Antes de compilar, predice:

- `foo(1, 2, 3)` dara error? (foo tiene parentesis vacios)
- `bar()` -- que pasa al llamar a bar sin argumentos?
- Si compilamos con `-Wall -Wextra -Wpedantic`, habra warnings?

### Paso 3.5 -- Compilar () vs (void)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic empty_vs_void.c -o empty_vs_void 2>&1
echo "Exit code: $?"
```

Salida esperada:

```
Exit code: 0
```

Sin warnings ni errores. Esto es revelador: `foo(1, 2, 3)` compila
perfectamente a pesar de que `foo` no recibe parametros en su definicion.

En C11, `int foo()` no significa "cero parametros" -- significa "numero
indeterminado de parametros". El compilador no verifica los argumentos.

```bash
./empty_vs_void
```

Salida esperada:

```
foo(1, 2, 3) = 42
bar() = 99
```

`foo` ignoro los tres argumentos y retorno 42. No hubo error en runtime
tampoco. Los argumentos extra simplemente se pusieron en el stack y nadie los
uso.

### Paso 3.6 -- (void) si protege

Ahora veamos que pasa con `bar(void)` cuando le pasamos argumentos:

```bash
cat bar_wrong_args.c
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic bar_wrong_args.c -o bar_wrong_args 2>&1
```

Salida esperada:

```
bar_wrong_args.c:6:35: error: too many arguments to function 'bar'; expected 0, have 3
bar_wrong_args.c:3:5: note: declared here
```

Error. `int bar(void)` declara explicitamente que `bar` no acepta argumentos.
El compilador lo verifica y rechaza la llamada.

Esta es la diferencia critica:

| Declaracion | Significado | El compilador verifica argumentos? |
|-------------|-------------|-------------------------------------|
| `int f()` | Numero indeterminado de parametros | No |
| `int f(void)` | Cero parametros | Si |

Por eso la recomendacion es usar siempre `(void)` para funciones sin
parametros. En C23, `()` y `(void)` seran equivalentes (como en C++), pero
en C11 la diferencia existe.

### Paso 3.7 -- -Wstrict-prototypes

GCC tiene un flag especifico para detectar prototipos con `()`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes empty_vs_void.c -o empty_vs_void 2>&1
```

Salida esperada:

```
empty_vs_void.c:3:1: warning: function declaration isn't a prototype [-Wstrict-prototypes]
    3 | int foo();       /* empty parentheses: unspecified parameters */
      | ^~~
empty_vs_void.c:12:5: warning: function declaration isn't a prototype [-Wstrict-prototypes]
   12 | int foo() {
      |     ^~~
```

Con `-Wstrict-prototypes`, GCC avisa que `int foo()` no es un prototipo real
porque no especifica los parametros. Nota que solo marca `foo`, no `bar` --
porque `bar(void)` si es un prototipo correcto.

### Limpieza de la parte 3

```bash
rm -f wrong_signature wrong_type empty_vs_void bar_wrong_args
```

---

## Limpieza final

```bash
rm -f with_prototype no_prototype def_before_use calculator no_header
rm -f double_include_test double_include_test.c
rm -f wrong_signature wrong_type empty_vs_void bar_wrong_args
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  bar_wrong_args.c  def_before_use.c  empty_vs_void.c  main_multi.c  math_ops.c  math_ops.h  no_header.c  no_prototype.c  with_prototype.c  wrong_signature.c  wrong_type.c
```

---

## Conceptos reforzados

1. Un prototipo (declaracion forward) le dice al compilador el tipo de retorno,
   el nombre, y los tipos de los parametros de una funcion **antes de que se
   use**. Esto permite verificar que las llamadas son correctas.

2. En C99+ con `-Wall`, llamar a una funcion sin declaracion previa produce un
   error de **implicit declaration**. El compilador no puede verificar los tipos
   sin conocer la firma.

3. Definir una funcion antes de usarla elimina la necesidad de un prototipo
   separado, pero esta tecnica no escala a proyectos multi-archivo.

4. En proyectos multi-archivo, los prototipos se organizan en headers (.h). El
   archivo .c incluye su propio header para que el compilador verifique que las
   definiciones coincidan con las declaraciones.

5. Los include guards (`#ifndef` / `#define` / `#endif`) evitan que un header
   incluido multiples veces cause errores de redefinicion.

6. `int f()` y `int f(void)` son **diferentes** en C11: `()` significa
   "parametros indeterminados" (sin verificacion), mientras que `(void)`
   significa "cero parametros" (con verificacion). Siempre usar `(void)`.

7. Gracias a los prototipos, el compilador detecta errores de numero de
   argumentos (`too few`/`too many`) y de tipo (`makes integer from pointer`).
   Sin prototipos, estos errores pasan desapercibidos hasta runtime.
