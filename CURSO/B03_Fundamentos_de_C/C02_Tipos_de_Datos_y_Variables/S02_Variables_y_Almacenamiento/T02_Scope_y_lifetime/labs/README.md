# Lab -- Scope y lifetime

## Objetivo

Observar como el scope determina donde es visible una variable, como el
lifetime determina cuanto tiempo vive su memoria, y como el linkage controla
si un nombre es compartido entre archivos. Al finalizar, sabras identificar
shadowing, provocar y diagnosticar el bug clasico de dangling pointer, y
usar `nm` para verificar la visibilidad de simbolos en archivos objeto.

## Prerequisitos

- GCC instalado
- `nm` disponible (incluido con binutils)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `block_scope.c` | Shadowing de variables en bloques anidados |
| `scope_error.c` | Acceso a variable fuera de su scope (no compila) |
| `dangling_pointer.c` | Retorno de puntero a variable local y alternativas seguras |
| `linkage_visible.c` | Funciones y variables con internal/external linkage |
| `linkage_visible.h` | Header publico para linkage_visible |
| `linkage_visible_main.c` | Main que accede a simbolos publicos |
| `linkage_counter.c` | Modulo contador con `static int count` |
| `linkage_counter.h` | Header del contador |
| `linkage_logger.c` | Modulo logger con `static int count` independiente |
| `linkage_logger.h` | Header del logger |
| `linkage_main.c` | Main que usa ambos modulos |

---

## Parte 1 -- Block scope y shadowing

**Objetivo**: Ver como las variables en bloques internos ocultan (shadow) a las
de bloques externos, y como el compilador puede advertirlo.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat block_scope.c
```

Observa la estructura:

- Una variable global `x = 100`
- Dentro de `show_shadowing()`, otra `x = 200`
- Dentro de un bloque anonimo, otra `x = 300`
- En `main()`, un `for` con `i` y `temp` declaradas en el loop

Antes de compilar y ejecutar, predice:

- Cuando se imprime "Inside inner block", que valor de `x` se mostrara?
- Cuando se imprime "Back in function", que valor de `x` se mostrara?
- Cuando se imprime "After call", que valor de `x` se mostrara?

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic block_scope.c -o block_scope
./block_scope
```

Salida esperada:

```
Before call: x = 100
Inside function: x = 200
Inside inner block: x = 300
Back in function: x = 200
After call: x = 100
Loop i=0, temp=0
Loop i=1, temp=10
Loop i=2, temp=20
```

Cada `x` es una variable diferente. La mas interna oculta a las externas, pero
al salir de su bloque, la anterior vuelve a ser visible. La global nunca cambio
de valor.

### Paso 1.3 -- Detectar shadowing con -Wshadow

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wshadow block_scope.c -o block_scope 2>&1
```

Salida esperada:

```
block_scope.c: In function 'show_shadowing':
block_scope.c:6:9: warning: declaration of 'x' shadows a global declaration [-Wshadow]
    6 |     int x = 200;
      |         ^
block_scope.c:3:5: note: shadowed declaration is here
    3 | int x = 100;
      |     ^
block_scope.c:10:13: warning: declaration of 'x' shadows a previous local [-Wshadow]
   10 |         int x = 300;
      |             ^
block_scope.c:6:9: note: shadowed declaration is here
    6 |     int x = 200;
      |         ^
```

`-Wshadow` no esta incluido en `-Wall` ni `-Wextra`. Es un flag adicional que
conviene usar cuando se sospecha de bugs por shadowing. El compilador indica
exactamente que variable oculta a cual.

### Paso 1.4 -- Provocar un error de scope

```bash
cat scope_error.c
```

Observa: se declara `inner` dentro de un bloque `{ }` y se intenta acceder
fuera de el.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic scope_error.c -o scope_error 2>&1
```

Salida esperada (error de compilacion):

```
scope_error.c: In function 'main':
scope_error.c:10:28: error: 'inner' undeclared (first use in this function)
   10 |     printf("inner = %d\n", inner);
      |                            ^~~~~
```

El compilador rechaza el codigo porque `inner` no existe fuera de su bloque.
Este es el mecanismo fundamental del block scope: la variable nace al entrar
al bloque y deja de existir al salir.

### Limpieza de la parte 1

```bash
rm -f block_scope scope_error
```

---

## Parte 2 -- File scope y linkage

**Objetivo**: Distinguir entre external linkage (visible en otros archivos) e
internal linkage (solo visible en el archivo que lo define), usando `nm` para
verificar.

### Paso 2.1 -- Examinar los archivos fuente

```bash
cat linkage_visible.c
```

Observa:

- `public_var` y `public_func` no tienen `static` -- external linkage
- `private_var` y `private_func` tienen `static` -- internal linkage
- `call_private` es un wrapper publico que invoca a `private_func`

```bash
cat linkage_visible.h
cat linkage_visible_main.c
```

El header solo expone lo publico. El main accede a `public_var` y llama
funciones publicas.

### Paso 2.2 -- Compilar los archivos objeto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c linkage_visible.c -o linkage_visible.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c linkage_visible_main.c -o linkage_visible_main.o
```

### Paso 2.3 -- Inspeccionar simbolos con nm

Antes de ejecutar `nm`, predice:

- `public_var` aparecera con letra mayuscula o minuscula en `nm`?
- `private_var` aparecera con mayuscula o minuscula?
- `private_func` aparecera con `T` (mayuscula) o `t` (minuscula)?

```bash
nm linkage_visible.o
```

Salida esperada:

```
0000000000000035 T call_private
                 U printf
0000000000000024 t private_func
0000000000000004 d private_var
0000000000000000 T public_func
0000000000000000 D public_var
                 U puts
```

Interpretacion de las letras de `nm`:

| Letra | Significado |
|-------|-------------|
| `T` (mayuscula) | Simbolo definido en `.text`, **external** linkage (visible desde otros archivos) |
| `t` (minuscula) | Simbolo definido en `.text`, **internal** linkage (solo este archivo) |
| `D` (mayuscula) | Dato inicializado, external linkage |
| `d` (minuscula) | Dato inicializado, internal linkage |
| `U` | Undefined -- se resolvera al enlazar |

La regla clave: **mayuscula = external linkage, minuscula = internal linkage**.
Las funciones y variables `static` aparecen con minuscula.

### Paso 2.4 -- Enlazar y ejecutar

```bash
gcc linkage_visible.o linkage_visible_main.o -o linkage_visible_prog
./linkage_visible_prog
```

Salida esperada:

```
From main: public_var = 10
public_func: public_var=10, private_var=20
private_func: only callable from this file
```

`main` accede a `public_var` directamente (external linkage). `private_func` solo
se ejecuta a traves del wrapper `call_private`.

### Paso 2.5 -- Verificar que lo privado no se enlaza directamente

Crea un archivo temporal que intenta acceder a `private_var`:

```bash
cat > access_private.c << 'EOF'
#include <stdio.h>

extern int private_var;

int main(void) {
    printf("private_var = %d\n", private_var);
    return 0;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -c access_private.c -o access_private.o
gcc linkage_visible.o access_private.o -o access_private 2>&1
```

Salida esperada (error de enlace):

```
undefined reference to `private_var'
```

Aunque `private_var` existe en `linkage_visible.o`, tiene internal linkage
(`static`). El enlazador no la comparte con otros archivos.

### Limpieza de la parte 2

```bash
rm -f linkage_visible.o linkage_visible_main.o linkage_visible_prog
rm -f access_private.c access_private.o access_private
```

---

## Parte 3 -- Lifetime: dangling pointer

**Objetivo**: Demostrar el bug clasico de retornar un puntero a una variable
local, entender por que es UB (undefined behavior), y ver las alternativas
seguras.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat dangling_pointer.c
```

Observa las tres funciones:

- `bad_return`: retorna la direccion de una variable local (UB)
- `good_return_malloc`: retorna un puntero a memoria del heap (seguro)
- `good_return_static`: retorna la direccion de una variable static (seguro)

Antes de compilar, predice:

- El compilador dara un warning para `bad_return`?
- El programa compilara a pesar del warning?

### Paso 3.2 -- Compilar y observar warnings

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dangling_pointer.c -o dangling_pointer 2>&1
```

El compilador no emite warnings para este caso particular porque la direccion
se toma de forma indirecta (a traves de `ptr`). Si escribieramos `return &local;`
directamente, GCC si emitiria:

```
warning: function returns address of local variable [-Wreturn-local-addr]
```

Esto demuestra que el compilador no siempre detecta todos los casos. Es
responsabilidad del programador entender el lifetime de cada variable.

### Paso 3.3 -- Ejecutar

```bash
./dangling_pointer
```

Salida esperada:

```
--- good_return_malloc ---
Value: 42 (safe, heap allocated)

--- good_return_static ---
Value: 42 (safe, static lifetime)

--- bad_return (dangling pointer) ---
Pointer bad = 0x<direccion>
Attempting to read *bad is undefined behavior.
The compiler warned us. Do NOT do this in real code.
```

Las dos funciones seguras retornan 42 correctamente. La funcion `bad_return`
retorna una direccion que apunta a memoria de stack que ya no pertenece a
`local`. Acceder a esa direccion es UB -- puede funcionar, dar basura, o
hacer crash.

### Paso 3.4 -- Entender la diferencia de lifetime

La clave esta en la diferencia entre scope y lifetime:

| Funcion | Scope del puntero | Lifetime de la memoria apuntada |
|---------|-------------------|--------------------------------|
| `bad_return` | Termina al retornar | **Termina al retornar** (automatic) |
| `good_return_malloc` | Termina al retornar | **Hasta que se haga free** (allocated) |
| `good_return_static` | Termina al retornar | **Toda la ejecucion** (static) |

En los tres casos, la variable puntero local deja de existir al retornar. Pero
lo que importa es el lifetime de la **memoria apuntada**, no del puntero.

### Limpieza de la parte 3

```bash
rm -f dangling_pointer
```

---

## Parte 4 -- Compilacion multi-archivo con linkage

**Objetivo**: Compilar dos modulos que usan `static int count` de forma
independiente, verificar con `nm` que los simbolos no se mezclan, y enlazarlos
en un programa.

### Paso 4.1 -- Examinar los modulos

```bash
cat linkage_counter.h
cat linkage_counter.c
```

Un modulo simple: variable `static int count`, funciones para incrementar y
leer.

```bash
cat linkage_logger.h
cat linkage_logger.c
```

Otro modulo con su propio `static int count`. Aunque se llama igual, es una
variable completamente diferente gracias a `static` (internal linkage).

```bash
cat linkage_main.c
```

El main usa ambos modulos a traves de sus funciones publicas.

### Paso 4.2 -- Compilar cada modulo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c linkage_counter.c -o linkage_counter.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c linkage_logger.c -o linkage_logger.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c linkage_main.c -o linkage_main.o
```

### Paso 4.3 -- Verificar simbolos con nm

Antes de ejecutar, predice:

- `count` aparecera en la salida de `nm` para cada modulo?
- Si aparece, sera con mayuscula (external) o minuscula (internal)?
- Las funciones `counter_increment` y `logger_log` seran visibles externamente?

```bash
echo "=== linkage_counter.o ==="
nm linkage_counter.o

echo ""
echo "=== linkage_logger.o ==="
nm linkage_logger.o

echo ""
echo "=== linkage_main.o ==="
nm linkage_main.o
```

Salida esperada:

```
=== linkage_counter.o ===
0000000000000000 b count
0000000000000016 T counter_get
0000000000000000 T counter_increment

=== linkage_logger.o ===
0000000000000000 b count
0000000000000039 T logger_get_count
0000000000000000 T logger_log
                 U printf

=== linkage_main.o ===
                 U counter_get
                 U counter_increment
                 U logger_get_count
                 U logger_log
0000000000000000 T main
                 U printf
```

Observa:

- `count` aparece en ambos modulos con `b` (minuscula) -- internal linkage, BSS
  (no inicializada explicitamente, pero `static` la pone a 0)
- Las funciones publicas aparecen con `T` (mayuscula) -- external linkage
- En `linkage_main.o`, todas las funciones externas aparecen como `U` (undefined)
  -- el enlazador las conectara

Los dos `count` son variables **completamente independientes** que el enlazador
no confunde porque no tienen external linkage.

### Paso 4.4 -- Enlazar y ejecutar

```bash
gcc linkage_counter.o linkage_logger.o linkage_main.o -o linkage_multi
./linkage_multi
```

Salida esperada:

```
[LOG #1] system started
[LOG #2] processing data
Counter value: 3
Logger count: 2
```

El contador llego a 3 (tres incrementos). El logger llego a 2 (dos mensajes).
Son dos variables `count` independientes, cada una en su modulo, sin interferir.

### Paso 4.5 -- Contraejemplo: que pasa sin static

Crea una version del contador sin `static`:

```bash
cat > counter_no_static.c << 'EOF'
#include <stdio.h>

int count = 0;

void counter_increment(void) {
    count++;
}

int counter_get(void) {
    return count;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -c counter_no_static.c -o counter_no_static.o
```

Ahora intenta enlazar con el logger (que tambien tiene un `count` pero `static`):

```bash
gcc counter_no_static.o linkage_logger.o linkage_main.o -o linkage_conflict 2>&1
```

No hay error porque el `count` del logger es `static` (internal). Pero si el
logger tambien tuviera `count` sin `static`, habria:

```
multiple definition of `count'
```

Verifica con `nm` que el `count` sin static ahora es external:

```bash
echo "=== counter_no_static.o ==="
nm counter_no_static.o | grep count

echo ""
echo "=== linkage_counter.o (con static) ==="
nm linkage_counter.o | grep count
```

Salida esperada:

```
=== counter_no_static.o ===
0000000000000000 B count

=== linkage_counter.o (con static) ===
0000000000000000 b count
```

`B` mayuscula (external) vs `b` minuscula (internal). Esa es la diferencia que
`static` produce a nivel de archivo.

### Limpieza de la parte 4

```bash
rm -f linkage_counter.o linkage_logger.o linkage_main.o linkage_multi
rm -f counter_no_static.c counter_no_static.o linkage_conflict
```

---

## Limpieza final

```bash
rm -f *.o block_scope scope_error dangling_pointer
rm -f linkage_visible_prog linkage_multi linkage_conflict
rm -f access_private.c access_private.o access_private
rm -f counter_no_static.c counter_no_static.o
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md            linkage_counter.h  linkage_main.c       linkage_visible_main.c
block_scope.c        linkage_logger.c   linkage_visible.c    scope_error.c
dangling_pointer.c   linkage_logger.h   linkage_visible.h
linkage_counter.c
```

---

## Conceptos reforzados

1. El **block scope** limita la visibilidad de una variable al bloque `{ }`
   donde fue declarada. Al salir del bloque, la variable deja de existir y no
   es accesible -- el compilador da error si se intenta usar.

2. El **shadowing** ocurre cuando una variable interna tiene el mismo nombre
   que una externa. La interna oculta a la externa dentro de su bloque. El flag
   `-Wshadow` (no incluido en `-Wall`) permite detectar estos casos.

3. El **lifetime** de una variable automatica (local) termina al salir de su
   funcion. Retornar un puntero a una variable local produce un **dangling
   pointer** -- UB que puede funcionar, dar basura, o hacer crash.

4. Las alternativas seguras para retornar datos desde una funcion son: usar
   `malloc` (el caller hace `free`), usar una variable `static` (vive toda la
   ejecucion), o pasar un buffer del caller.

5. El **linkage** controla si un nombre es compartido entre archivos. `static`
   a nivel de archivo produce **internal linkage** -- el simbolo solo es visible
   en ese archivo. Sin `static`, el linkage es **external** por defecto.

6. En la salida de `nm`, las letras **mayusculas** (`T`, `D`, `B`) indican
   external linkage (visible globalmente). Las **minusculas** (`t`, `d`, `b`)
   indican internal linkage (`static`).

7. Dos archivos pueden tener variables `static` con el mismo nombre sin
   conflicto. Son variables completamente independientes que el enlazador no
   confunde, porque internal linkage significa que el simbolo no se exporta.
