# Lab -- Archivos objeto y enlace

## Objetivo

Entender como GCC produce archivos objeto, como el enlazador resuelve simbolos
entre multiples archivos, y como funcionan las bibliotecas estaticas (.a) y
dinamicas (.so). Al finalizar, sabras interpretar la salida de `nm`, crear ambos
tipos de biblioteca, y diagnosticar errores de enlace.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `calc.h` | Header con prototipos de add, sub, mul |
| `add.c` | Implementacion de add |
| `sub.c` | Implementacion de sub |
| `mul.c` | Implementacion de mul |
| `main_calc.c` | Programa principal que usa las tres funciones |
| `visibility.c` | Programa con simbolos extern y static para demostrar visibilidad |

---

## Parte 1 -- Simbolos en archivos objeto

**Objetivo**: Entender la tabla de simbolos de un archivo objeto y la diferencia
entre simbolos externos (visibles) y simbolos locales (static).

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat visibility.c
```

Observa la estructura del archivo:

```c
int global_counter = 42;
static int internal_counter = 10;

void public_function(void) { ... }
static void helper_function(void) { ... }
```

Hay cuatro simbolos declarados: dos variables y dos funciones. Dos son `static`
y dos no.

### Paso 1.2 -- Compilar a archivo objeto

```bash
gcc -c visibility.c -o visibility.o
```

Esto produce un `.o` sin enlazar. Ahora vamos a inspeccionar su tabla de
simbolos.

### Paso 1.3 -- Predecir la visibilidad

Antes de ejecutar `nm`, predice para cada simbolo:

| Simbolo | Es static? | Prediccion: letra nm? |
|---------|------------|----------------------|
| `global_counter` | No | ? |
| `internal_counter` | Si | ? |
| `public_function` | No | ? |
| `helper_function` | Si | ? |
| `main` | No | ? |
| `printf` | -- | ? |

Pista: `nm` usa **mayuscula** para simbolos externos (visibles fuera del .o) y
**minuscula** para simbolos locales (solo visibles dentro del .o).

Las letras clave son:

- `T`/`t` = definido en la seccion de texto (codigo)
- `D`/`d` = definido en la seccion de datos inicializados
- `U` = undefined (se necesita de otro lado)

### Paso 1.4 -- Verificar con nm

```bash
nm visibility.o
```

Salida esperada:

```
0000000000000000 D global_counter
000000000000001e t helper_function
0000000000000004 d internal_counter
000000000000003c T main
                 U printf
0000000000000000 T public_function
```

Analisis:

- `D global_counter` -- mayuscula, dato global visible externamente
- `d internal_counter` -- minuscula, dato local (static), invisible fuera de este .o
- `T public_function` -- mayuscula, funcion visible externamente
- `t helper_function` -- minuscula, funcion local (static), invisible fuera de este .o
- `T main` -- mayuscula, funcion visible externamente
- `U printf` -- undefined, no esta definido aqui; el enlazador lo resolvera

La regla es directa: `static` en C produce un simbolo con letra minuscula en
`nm`. Sin `static`, el simbolo es externo (mayuscula) y puede ser visto por
otros archivos objeto al enlazar.

### Paso 1.5 -- Verificar que static impide el acceso externo

Compila y ejecuta para confirmar que todo funciona dentro del mismo archivo:

```bash
gcc visibility.c -o visibility
./visibility
```

Salida esperada:

```
public_function: global_counter = 42
helper_function: internal_counter = 10
```

Los simbolos `static` funcionan dentro de su unidad de traduccion (su .c), pero
no son visibles para el enlazador desde otros archivos.

### Limpieza de la parte 1

```bash
rm -f visibility.o visibility
```

---

## Parte 2 -- Compilacion multi-archivo y enlace

**Objetivo**: Compilar archivos separados a .o, inspeccionar sus simbolos,
enlazarlos, y provocar los errores clasicos de enlace.

### Paso 2.1 -- Examinar los archivos fuente

```bash
cat calc.h
cat add.c
cat main_calc.c
```

Observa la estructura: `main_calc.c` incluye `calc.h` (solo prototipos). Las
implementaciones estan en `add.c`, `sub.c` y `mul.c`.

### Paso 2.2 -- Compilar cada archivo por separado

```bash
gcc -c add.c -o add.o
gcc -c sub.c -o sub.o
gcc -c mul.c -o mul.o
gcc -c main_calc.c -o main_calc.o
```

### Paso 2.3 -- Predecir los simbolos

Antes de ejecutar `nm`, predice:

- En `add.o`, `add` estara como `T` (definido) o `U` (undefined)?
- En `main_calc.o`, `add` estara como `T` o `U`?
- En `main_calc.o`, `printf` estara como `T` o `U`?
- Alguno de los .o contendra los tres simbolos `add`, `sub`, `mul`?

### Paso 2.4 -- Verificar con nm

```bash
echo "=== add.o ==="
nm add.o

echo "=== sub.o ==="
nm sub.o

echo "=== mul.o ==="
nm mul.o

echo "=== main_calc.o ==="
nm main_calc.o
```

Salida esperada:

```
=== add.o ===
0000000000000000 T add

=== sub.o ===
0000000000000000 T sub

=== mul.o ===
0000000000000000 T mul

=== main_calc.o ===
                 U add
0000000000000000 T main
                 U mul
                 U printf
                 U sub
```

Cada `.o` de implementacion tiene exactamente un simbolo `T` (la funcion que
define). `main_calc.o` tiene `T main` y tres `U` (las funciones que necesita
de otros archivos) mas `U printf` (que viene de libc).

### Paso 2.5 -- Enlazar todos los .o

```bash
gcc add.o sub.o mul.o main_calc.o -o calculator
./calculator
```

Salida esperada:

```
10 + 3 = 13
10 - 3 = 7
10 * 3 = 30
```

El enlazador conecto cada `U` de `main_calc.o` con el `T` correspondiente
de los otros .o. `printf` se resolvio con libc (enlace dinamico automatico).

### Paso 2.6 -- Error: undefined reference

Que pasa si olvidamos enlazar `mul.o`?

```bash
gcc add.o sub.o main_calc.o -o calculator_broken 2>&1
```

Salida esperada (error):

```
undefined reference to `mul'
```

El enlazador encontro `mul` como `U` en `main_calc.o` pero ningun otro .o
proporciona un `T mul`. Sin `mul.o`, ese simbolo queda sin resolver.

### Paso 2.7 -- Error: multiple definition

Crea un archivo que defina `add` por segunda vez:

```bash
cat > extra_add.c << 'EOF'
int add(int a, int b) {
    return a + b + 100;
}
EOF

gcc -c extra_add.c -o extra_add.o
```

Ahora enlaza con los dos archivos que definen `add`:

```bash
gcc add.o extra_add.o sub.o mul.o main_calc.o -o calculator_dup 2>&1
```

Salida esperada (error):

```
multiple definition of `add'
```

Dos archivos objeto definen el mismo simbolo `T add` -- el enlazador no puede
elegir cual usar.

### Limpieza de la parte 2

```bash
rm -f add.o sub.o mul.o main_calc.o calculator calculator_broken
rm -f extra_add.c extra_add.o calculator_dup
```

---

## Parte 3 -- Biblioteca estatica

**Objetivo**: Crear una biblioteca estatica (.a), inspeccionarla, y demostrar
que el orden de los argumentos en la linea de enlace importa.

### Paso 3.1 -- Compilar los archivos objeto

```bash
gcc -c add.c -o add.o
gcc -c sub.c -o sub.o
gcc -c mul.c -o mul.o
```

### Paso 3.2 -- Crear la biblioteca estatica

```bash
ar rcs libcalc.a add.o sub.o mul.o
```

Significado de los flags:

- `r` -- insertar los .o en el archivo (reemplazar si ya existen)
- `c` -- crear el archivo si no existe
- `s` -- escribir un indice de simbolos (para que el enlazador busque rapido)

### Paso 3.3 -- Inspeccionar la biblioteca

Lista los archivos dentro:

```bash
ar t libcalc.a
```

Salida esperada:

```
add.o
sub.o
mul.o
```

Inspecciona los simbolos de toda la biblioteca:

```bash
nm libcalc.a
```

Salida esperada:

```
add.o:
0000000000000000 T add

sub.o:
0000000000000000 T sub

mul.o:
0000000000000000 T mul
```

La biblioteca es un archivo que contiene los tres .o empaquetados.

### Paso 3.4 -- Enlazar con la biblioteca estatica

```bash
gcc main_calc.c -L. -lcalc -o calc_static
./calc_static
```

Salida esperada:

```
10 + 3 = 13
10 - 3 = 7
10 * 3 = 30
```

Significado de los flags:

- `-L.` -- buscar bibliotecas en el directorio actual
- `-lcalc` -- enlazar con `libcalc.a` (el prefijo `lib` y sufijo `.a` son implicitos)

### Paso 3.5 -- Verificar con ldd

```bash
ldd calc_static
```

Salida esperada:

```
linux-vdso.so.1 (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

Observa: `libcalc` **no** aparece como dependencia. Las funciones de
`libcalc.a` se copiaron directamente al ejecutable en tiempo de enlace.
Solo `libc.so.6` aparece porque `printf` se enlaza dinamicamente.

### Paso 3.6 -- El orden de enlace importa

Intenta poner `-lcalc` antes de `main_calc.c`:

```bash
gcc -L. -lcalc main_calc.c -o calc_order_test 2>&1
```

Salida esperada (error):

```
undefined reference to `add'
undefined reference to `sub'
undefined reference to `mul'
```

El enlazador procesa los argumentos de izquierda a derecha. Cuando llega a
`libcalc.a`, todavia no ha visto ningun `U` que necesite resolver (no ha
procesado `main_calc.c` aun). Descarta la biblioteca. Luego procesa
`main_calc.c`, encuentra los `U`, pero la biblioteca ya paso. Sin nada que
los resuelva, falla.

La regla: los archivos que **usan** simbolos van antes de los que los
**definen**.

### Limpieza de la parte 3

```bash
rm -f add.o sub.o mul.o libcalc.a calc_static calc_order_test
```

---

## Parte 4 -- Biblioteca dinamica

**Objetivo**: Crear una biblioteca dinamica (.so), enlazar con ella, y entender
las diferencias con una biblioteca estatica.

### Paso 4.1 -- Compilar con -fPIC

```bash
gcc -fPIC -c add.c -o add.o
gcc -fPIC -c sub.c -o sub.o
gcc -fPIC -c mul.c -o mul.o
```

`-fPIC` genera codigo independiente de posicion (Position Independent Code).
Es necesario para bibliotecas dinamicas porque se cargan en direcciones de
memoria arbitrarias.

### Paso 4.2 -- Crear la biblioteca dinamica

```bash
gcc -shared -o libcalc.so add.o sub.o mul.o
```

Verifica el tipo de archivo:

```bash
file libcalc.so
```

Salida esperada:

```
libcalc.so: ELF 64-bit LSB shared object, x86-64, ...
```

### Paso 4.3 -- Enlazar con la biblioteca dinamica

```bash
gcc main_calc.c -L. -lcalc -o calc_dynamic
```

Los flags son identicos a los de la biblioteca estatica. GCC prefiere `.so`
sobre `.a` cuando ambas existen.

### Paso 4.4 -- Verificar con ldd

```bash
ldd calc_dynamic
```

Salida esperada:

```
linux-vdso.so.1 (0x...)
libcalc.so => not found
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

Observa: `libcalc.so => not found`. El ejecutable **sabe** que necesita
`libcalc.so`, pero el cargador dinamico no sabe donde buscarla. No esta en
`/lib`, `/usr/lib`, ni en ninguna ruta del sistema.

### Paso 4.5 -- Ejecutar con LD_LIBRARY_PATH

```bash
LD_LIBRARY_PATH=. ./calc_dynamic
```

Salida esperada:

```
10 + 3 = 13
10 - 3 = 7
10 * 3 = 30
```

`LD_LIBRARY_PATH=.` le dice al cargador dinamico que busque bibliotecas
tambien en el directorio actual. Ahora verifica que `ldd` tambien la encuentra:

```bash
LD_LIBRARY_PATH=. ldd calc_dynamic
```

Salida esperada:

```
linux-vdso.so.1 (0x...)
libcalc.so => ./libcalc.so (0x...)
libc.so.6 => /lib64/libc.so.6 (0x...)
/lib64/ld-linux-x86-64.so.2 (0x...)
```

Ahora `libcalc.so` se resuelve a `./libcalc.so`.

### Paso 4.6 -- Comparar con la biblioteca estatica

Recrea la biblioteca estatica para comparar:

```bash
ar rcs libcalc.a add.o sub.o mul.o
gcc main_calc.c -L. -lcalc -o calc_with_so
gcc main_calc.c libcalc.a -o calc_with_a
```

Nota: cuando `-L. -lcalc` encuentra tanto `libcalc.so` como `libcalc.a`, GCC
prefiere la `.so`. Para forzar la `.a`, se pasa directamente como argumento.

Compara las dependencias:

```bash
echo "=== con .so ==="
ldd calc_with_so

echo "=== con .a ==="
ldd calc_with_a
```

Observa:

- `calc_with_so` lista `libcalc.so` como dependencia (se carga en runtime)
- `calc_with_a` **no** lista `libcalc` (las funciones se copiaron al binario)

Compara los tamanos de las secciones:

```bash
echo "=== con .so ==="
size calc_with_so

echo "=== con .a ==="
size calc_with_a
```

En este caso las diferencias son minimas porque las funciones de calc son
pequenas. En proyectos reales con bibliotecas grandes, la diferencia es
significativa: el binario con `.a` es mas grande porque contiene una copia
del codigo; el binario con `.so` es mas pequeno porque solo contiene una
referencia.

### Paso 4.7 -- Que pasa si la .so no esta al ejecutar

```bash
mv libcalc.so libcalc.so.bak
./calc_with_so 2>&1
```

Salida esperada (error):

```
./calc_with_so: error while loading shared libraries: libcalc.so: cannot open shared object file: No such file or directory
```

El ejecutable no puede arrancar porque depende de `libcalc.so` en runtime.
Ahora compara con el binario enlazado estaticamente:

```bash
./calc_with_a
```

Salida esperada:

```
10 + 3 = 13
10 - 3 = 7
10 * 3 = 30
```

Funciona sin problemas. El codigo de las funciones esta dentro del binario;
no necesita ninguna biblioteca externa. Restaura la .so:

```bash
mv libcalc.so.bak libcalc.so
```

Esto resume la diferencia fundamental:

- **Estatica**: el codigo se copia al binario. Binario mas grande, sin dependencias externas.
- **Dinamica**: el binario solo tiene una referencia. Binario mas pequeno, pero requiere que la .so este disponible al ejecutar.

### Limpieza de la parte 4

```bash
rm -f add.o sub.o mul.o libcalc.a libcalc.so
rm -f calc_dynamic calc_with_so calc_with_a
```

---

## Limpieza final

```bash
rm -f *.o *.a *.so extra_add.c
rm -f visibility calculator calculator_broken calculator_dup
rm -f calc_static calc_dynamic calc_with_so calc_with_a calc_order_test
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  add.c  calc.h  main_calc.c  mul.c  sub.c  visibility.c
```

---

## Conceptos reforzados

1. `nm` muestra la tabla de simbolos de un archivo objeto. Letras mayusculas
   (`T`, `D`) indican simbolos externos (visibles al enlazador). Letras
   minusculas (`t`, `d`) indican simbolos locales (`static` en C).

2. `U` en `nm` significa undefined: el simbolo se usa pero no se define en
   ese .o. El enlazador debe encontrar su definicion `T` en otro .o o
   biblioteca.

3. "undefined reference" ocurre cuando el enlazador no encuentra ningun `T`
   que corresponda a un `U`. "multiple definition" ocurre cuando dos o mas
   archivos definen el mismo simbolo `T`.

4. Una biblioteca estatica (`.a`) es un archivo creado con `ar` que contiene
   varios `.o`. Al enlazar, el codigo se copia al binario. El ejecutable no
   depende de la `.a` en runtime.

5. Una biblioteca dinamica (`.so`) se crea con `-fPIC` y `-shared`. El
   ejecutable solo guarda una referencia; la `.so` debe estar disponible al
   ejecutar. `LD_LIBRARY_PATH` permite indicar directorios adicionales de
   busqueda.

6. El orden de enlace importa con bibliotecas estaticas: los archivos que
   usan simbolos deben ir antes de los que los definen. `-lcalc` despues de
   `main_calc.c`, no antes.

7. `ldd` muestra las dependencias de bibliotecas dinamicas de un ejecutable.
   Si la `.so` no se encuentra en runtime, el programa no puede arrancar.
