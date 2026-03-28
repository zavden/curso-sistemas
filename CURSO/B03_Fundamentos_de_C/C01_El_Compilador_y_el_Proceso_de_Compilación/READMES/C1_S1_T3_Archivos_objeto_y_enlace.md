# Archivos objeto y enlace

## Erratas detectadas en el material base

| Ubicacion | Error | Correccion |
|-----------|-------|------------|
| README.md:456 | Dice "algunas funciones de math.h (como abs) SI estan en libc". `abs()` esta declarado en `<stdlib.h>`, no en `<math.h>`, y siempre ha sido parte de libc. No es una funcion matematica de `libm` que "resulta estar en libc". | Se corrige: `abs()` es de `<stdlib.h>` (libc). Las funciones que requieren `-lm` son las de `<math.h>` que operan con doubles/floats (`sin`, `cos`, `sqrt`, `pow`, etc.). |

---

## Indice

1. [Archivos objeto (.o)](#1-archivos-objeto-o)
2. [Simbolos y visibilidad](#2-simbolos-y-visibilidad)
3. [El proceso de enlace](#3-el-proceso-de-enlace)
4. [Errores de enlace](#4-errores-de-enlace)
5. [Enlace estatico (.a)](#5-enlace-estatico-a)
6. [Enlace dinamico (.so)](#6-enlace-dinamico-so)
7. [Comparacion estatico vs dinamico](#7-comparacion-estatico-vs-dinamico)
8. [Flags de paths: -I, -L, -l](#8-flags-de-paths--i--l--l)
9. [Herramientas de inspeccion](#9-herramientas-de-inspeccion)
10. [Ejercicios](#10-ejercicios)

---

## 1. Archivos objeto (.o)

Un archivo objeto es el resultado de compilar un `.c` sin enlazar. Contiene codigo maquina, pero aun no es un programa ejecutable:

```bash
gcc -c math.c -o math.o
gcc -c main.c -o main.o
# Neither can be executed alone - they need linking
```

### Que contiene un .o

```bash
# A .o is an ELF relocatable file:
file math.o
# math.o: ELF 64-bit LSB relocatable, x86-64, ...

# It has sections:
objdump -h math.o
# .text    - executable code (functions)
# .data    - initialized global variables
# .bss     - uninitialized global variables (zeros)
# .rodata  - read-only constants (strings, const)
# .symtab  - symbol table
# .rela.*  - relocation information
```

### Relocatable — direcciones pendientes

Un `.o` no tiene direcciones finales. Las llamadas a funciones externas estan marcadas como "por resolver":

```bash
objdump -d math.o
# callq  0x0 <add+0x1a>     <- temporary address (0x0)
# The linker will put the real address

# View relocation entries:
objdump -r math.o
# RELOCATION RECORDS FOR [.text]:
# OFFSET    TYPE              VALUE
# 0000001a  R_X86_64_PLT32    printf-0x4
```

---

## 2. Simbolos y visibilidad

Cada `.o` tiene una tabla de simbolos — los nombres de funciones y variables globales que define o necesita:

```bash
nm math.o
# 0000000000000000 T add       <- defined here (T = text/code)
# 0000000000000000 D counter   <- defined here (D = data)
#                  U printf    <- undefined (comes from elsewhere)
```

### Tipos de simbolos en nm

| Letra | Significado |
|-------|-------------|
| `T` / `t` | Definido en `.text` (codigo) |
| `D` / `d` | Definido en `.data` (datos inicializados) |
| `B` / `b` | Definido en `.bss` (datos sin inicializar) |
| `U` | Undefined (necesita resolverse al enlazar) |

**Mayuscula** = linkage externo (visible fuera del `.o`). **Minuscula** = local (solo visible dentro).

### static controla la visibilidad

```c
// math.c
int counter = 0;              // symbol D (data, external)
static int internal = 5;      // symbol d (data, local)

int add(int a, int b) {       // symbol T (text, external)
    return a + b;
}

static int helper(int x) {    // symbol t (text, local)
    return x * 2;
}
```

```bash
nm math.o
# 0000000000000000 D counter
# 0000000000000004 d internal    <- lowercase = local (static)
# 0000000000000000 T add
# 0000000000000014 t helper      <- lowercase = local (static)
```

`static` en C produce simbolos con letra minuscula en `nm` — no visibles fuera del `.o`. Sin `static`, el simbolo es externo y puede ser visto por otros archivos al enlazar.

---

## 3. El proceso de enlace

El enlazador (`ld`, invocado por `gcc`) toma uno o mas `.o` y las bibliotecas necesarias para producir un ejecutable:

```
1. Resolucion de simbolos
   main.o dice: "necesito add" (simbolo U)
   math.o dice: "yo tengo add" (simbolo T)
   -> El enlazador los conecta

2. Combinacion de secciones
   Junta todas las .text de todos los .o en una sola .text
   Junta todas las .data, .bss, .rodata...

3. Reubicacion
   Asigna direcciones finales a cada seccion
   Reemplaza las direcciones temporales por las definitivas

4. Generacion del ejecutable
   Escribe el header ELF, tabla de programa, etc.
   El resultado es un archivo que el kernel puede ejecutar
```

### Declaraciones vs definiciones

```c
// In headers (.h) - only DECLARATIONS:
int add(int a, int b);        // declaration (prototype)
extern int counter;            // variable declaration

// In sources (.c) - the DEFINITIONS:
int add(int a, int b) {       // definition
    return a + b;
}
int counter = 0;               // definition
```

---

## 4. Errores de enlace

### undefined reference

```bash
# If you forget to link a .o:
gcc main.o -o program
# undefined reference to `add'
# main.o needs add but we didn't link math.o
```

### multiple definition

```bash
# If add is defined in math.o AND in extra.o:
gcc math.o extra.o main.o -o program
# multiple definition of `add'
```

---

## 5. Enlace estatico (.a)

En el enlace estatico, el codigo de la biblioteca se **copia dentro** del ejecutable.

### Crear una biblioteca estatica

```bash
# Compile to .o:
gcc -c add.c -o add.o
gcc -c mul.c -o mul.o

# Package into .a with ar:
ar rcs libmath.a add.o mul.o
# r = insert/replace
# c = create if doesn't exist
# s = write symbol index (like ranlib)
```

### Inspeccionar

```bash
# List contents:
ar t libmath.a
# add.o
# mul.o

# View symbols:
nm libmath.a
# add.o:
# 0000000000000000 T add
#
# mul.o:
# 0000000000000000 T mul
```

### Enlazar con biblioteca estatica

```bash
# With -l (searches for libNAME.a or libNAME.so):
gcc main.c -L. -lmath -o program
# -L.     -> search for libraries in current directory
# -lmath  -> search for libmath.a (or libmath.so)

# Or directly:
gcc main.c libmath.a -o program
```

### El orden importa

```bash
# The linker processes arguments LEFT TO RIGHT.
# When it finds a .o, it records its undefined symbols.
# When it finds a .a, it searches it only for symbols it needs.

# WRONG - library before who uses it:
gcc -lmath main.c -o program
# Linker processes libmath.a first, needs nothing -> ignores it
# Then processes main.o, needs add -> undefined reference

# CORRECT - library after:
gcc main.c -lmath -o program
# Rule: dependencies go AFTER who needs them
```

### Caracteristicas

- Ejecutable autonomo (no necesita `.a` en runtime)
- Sin problemas de versiones de biblioteca
- Ejecutable mas grande (copia el codigo)
- Si la biblioteca se actualiza, hay que recompilar

---

## 6. Enlace dinamico (.so)

En el enlace dinamico, el ejecutable solo contiene una **referencia**. El codigo se carga en runtime.

### Crear una biblioteca dinamica

```bash
# Compile with -fPIC (Position Independent Code):
gcc -c -fPIC add.c -o add.o
gcc -c -fPIC mul.c -o mul.o

# Create shared object:
gcc -shared -o libmath.so add.o mul.o
```

### Enlazar y ejecutar

```bash
gcc main.c -L. -lmath -o program
# -lmath searches for libmath.so first, then libmath.a

./program
# error while loading shared libraries: libmath.so:
# cannot open shared object file: No such file or directory
```

### Como encuentra el loader las bibliotecas

El loader dinamico (`ld-linux.so`) busca en este orden:
1. RPATH embebido en el ejecutable (deprecated)
2. `LD_LIBRARY_PATH` (variable de entorno)
3. RUNPATH embebido en el ejecutable
4. `/etc/ld.so.cache` (cache de `ldconfig`)
5. `/lib`, `/usr/lib` (paths por defecto)

Soluciones:

```bash
# 1. Install in system path:
sudo cp libmath.so /usr/local/lib/
sudo ldconfig    # update cache

# 2. LD_LIBRARY_PATH (temporary, not for production):
LD_LIBRARY_PATH=. ./program

# 3. RPATH (embed path in executable):
gcc main.c -L. -lmath -Wl,-rpath,/opt/mylibs -o program
```

```bash
# Check dynamic dependencies:
ldd program
# libmath.so => /usr/local/lib/libmath.so
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
```

### Caracteristicas

- Ejecutable mas pequeno (no copia el codigo)
- Multiples programas comparten una copia en memoria
- Se puede actualizar la biblioteca sin recompilar
- Necesita el `.so` en runtime (dependencia externa)
- Posibles conflictos de versiones ("dependency hell")

---

## 7. Comparacion estatico vs dinamico

| Aspecto | Estatico (.a) | Dinamico (.so) |
|---------|---------------|----------------|
| Codigo en ejecutable | Si (copia) | No (referencia) |
| Necesita lib en runtime | No | Si |
| Tamano del ejecutable | Grande | Pequeno |
| Memoria compartida | No | Si |
| Actualizacion de lib | Recompilar | Solo reemplazar .so |
| Velocidad de arranque | Mas rapido | Mas lento |
| Distribucion | Mas simple | Necesita dependencias |

---

## 8. Flags de paths: -I, -L, -l

### -I — Path de headers

```bash
gcc -I./include main.c -o main
# Now #include "myheader.h" also searches in ./include/

# Convention:
#include <stdio.h>      // searches system paths (/usr/include)
#include "myheader.h"   // searches .c directory, then -I paths, then system
```

### -L — Path de bibliotecas

```bash
gcc main.c -L./lib -lmath -o program
# -L./lib -> linker also searches in ./lib/
```

### -l — Enlazar con biblioteca

```bash
gcc main.c -lm -o program       # libm (math: sin, cos, sqrt)
gcc main.c -lpthread -o program  # libpthread (POSIX threads)
```

**Por que `-lm` es necesario**:

```c
#include <math.h>

int main(void) {
    double x = sqrt(2.0);   // sqrt is in libm, not libc
    return 0;
}
// gcc main.c -o main        -> undefined reference to `sqrt'
// gcc main.c -lm -o main    -> works
```

Las funciones de `<math.h>` que operan con `double`/`float` (`sin`, `cos`, `sqrt`, `pow`, etc.) estan en `libm`. Nota: `abs()` esta en `<stdlib.h>` y es parte de `libc` — no requiere `-lm`.

---

## 9. Herramientas de inspeccion

```bash
# file - file type:
file main.o      # ELF 64-bit LSB relocatable
file program     # ELF 64-bit LSB pie executable
file libmath.a   # current ar archive
file libmath.so  # ELF 64-bit LSB shared object

# nm - symbol table:
nm main.o        # symbols of a .o
nm -D libmath.so # dynamic symbols of a .so

# ldd - dynamic dependencies:
ldd program      # what .so it needs at runtime

# objdump - sections and disassembly:
objdump -h main.o    # section headers
objdump -d main.o    # disassembly
objdump -t main.o    # symbol table (more detail than nm)
objdump -r main.o    # relocation entries

# readelf - detailed ELF info:
readelf -h program   # ELF header
readelf -S program   # sections
readelf -d program   # dynamic dependencies

# size - section sizes:
size program
# text    data     bss     dec     hex filename

# strings - readable strings in binary:
strings program | grep "Hello"
```

---

## 10. Ejercicios

Los archivos fuente del laboratorio estan en `labs/`. Usa ese directorio como base.

### Ejercicio 1: Visibilidad de simbolos — extern vs static

Usando `labs/visibility.c`:

```bash
gcc -c visibility.c -o visibility.o
nm visibility.o
```

**Prediccion**: para cada simbolo, indica la letra de `nm`:

| Simbolo | Es static? | Letra nm |
|---------|-----------|----------|
| `global_counter` | No | ? |
| `internal_counter` | Si | ? |
| `public_function` | No | ? |
| `helper_function` | Si | ? |
| `main` | No | ? |
| `printf` | — | ? |

<details><summary>Respuesta</summary>

| Simbolo | Letra | Explicacion |
|---------|-------|-------------|
| `global_counter` | **D** | Dato inicializado, externo (mayuscula = visible) |
| `internal_counter` | **d** | Dato inicializado, local (minuscula = static) |
| `public_function` | **T** | Codigo, externo |
| `helper_function` | **t** | Codigo, local (static) |
| `main` | **T** | Codigo, externo |
| `printf` | **U** | Undefined — no definido aqui, viene de libc |

Regla directa: `static` en C → minuscula en `nm` → invisible fuera del `.o`.

</details>

Limpieza: `rm -f visibility.o`

---

### Ejercicio 2: Simbolos en compilacion multi-archivo

Usando `labs/`:

```bash
gcc -c add.c -o add.o
gcc -c sub.c -o sub.o
gcc -c mul.c -o mul.o
gcc -c main_calc.c -o main_calc.o
```

**Prediccion**: en `main_calc.o`, los simbolos `add`, `sub`, `mul` aparecen como ____ y `main` como ____

<details><summary>Respuesta</summary>

- `add`, `sub`, `mul`: **`U`** (undefined) — `main_calc.c` incluye `calc.h` que tiene declaraciones (prototipos), pero no definiciones. El compilador sabe que existen pero no sabe donde estan.
- `main`: **`T`** — definido aqui
- `printf`: **`U`** — viene de libc

Cada `.o` de implementacion (`add.o`, `sub.o`, `mul.o`) tiene exactamente un simbolo `T`. El enlazador conectara los `U` de `main_calc.o` con los `T` correspondientes.

</details>

Limpieza: `rm -f add.o sub.o mul.o main_calc.o`

---

### Ejercicio 3: Errores de enlace clasicos

```bash
gcc -c add.c -o add.o
gcc -c sub.c -o sub.o
gcc -c mul.c -o mul.o
gcc -c main_calc.c -o main_calc.o
```

1. Enlaza sin `mul.o`: `gcc add.o sub.o main_calc.o -o calculator 2>&1`

**Prediccion**: el error sera ____

<details><summary>Respuesta</summary>

`undefined reference to 'mul'` — el enlazador encontro `mul` como `U` en `main_calc.o` pero ningun otro `.o` provee un `T mul`. `add` y `sub` se resuelven bien, pero falta `mul`.

</details>

2. Crea una definicion duplicada:
   ```bash
   echo 'int add(int a, int b) { return a + b + 100; }' > /tmp/extra_add.c
   gcc -c /tmp/extra_add.c -o /tmp/extra_add.o
   gcc add.o /tmp/extra_add.o sub.o mul.o main_calc.o -o calculator 2>&1
   ```

**Prediccion**: el error sera ____

<details><summary>Respuesta</summary>

`multiple definition of 'add'` — dos archivos objeto definen el mismo simbolo `T add`. El enlazador no puede elegir cual usar. Cada simbolo externo debe tener exactamente una definicion (ODR: One Definition Rule).

</details>

Limpieza: `rm -f add.o sub.o mul.o main_calc.o /tmp/extra_add.c /tmp/extra_add.o calculator`

---

### Ejercicio 4: Crear y usar biblioteca estatica

```bash
gcc -c add.c -o add.o
gcc -c sub.c -o sub.o
gcc -c mul.c -o mul.o
ar rcs libcalc.a add.o sub.o mul.o
```

1. Verifica el contenido: `ar t libcalc.a`

**Prediccion**: que archivos lista?

<details><summary>Respuesta</summary>

```
add.o
sub.o
mul.o
```

Un `.a` es simplemente un archivo que empaqueta multiples `.o`. `ar t` lista su contenido, como `tar -t` para tarballs.

</details>

2. Enlaza: `gcc main_calc.c -L. -lcalc -o calc_static && ./calc_static`

3. Verifica dependencias: `ldd calc_static`

**Prediccion**: `libcalc` aparece como dependencia?

<details><summary>Respuesta</summary>

**No**. Las funciones de `libcalc.a` se copiaron directamente al ejecutable en tiempo de enlace. Solo aparecen `libc.so.6` (para `printf`) y el enlazador dinamico. El ejecutable es autonomo respecto a `libcalc`.

</details>

Limpieza: `rm -f add.o sub.o mul.o libcalc.a calc_static`

---

### Ejercicio 5: Orden de enlace con bibliotecas estaticas

```bash
gcc -c add.c -o add.o && gcc -c sub.c -o sub.o && gcc -c mul.c -o mul.o
ar rcs libcalc.a add.o sub.o mul.o
```

1. Enlaza con `-lcalc` ANTES del fuente: `gcc -L. -lcalc main_calc.c -o calc_order 2>&1`

**Prediccion**: funciona o falla? Por que?

<details><summary>Respuesta</summary>

**Falla**: `undefined reference to 'add'`, `'sub'`, `'mul'`.

El enlazador procesa argumentos de izquierda a derecha. Cuando llega a `libcalc.a`, no ha visto ningun `U` que resolver (aun no proceso `main_calc.c`). Descarta la biblioteca. Despues procesa `main_calc.c`, encuentra los `U`, pero la biblioteca ya paso.

**Regla**: dependencias van **despues** de quien las necesita.

</details>

2. Enlaza en orden correcto: `gcc main_calc.c -L. -lcalc -o calc_order && ./calc_order`

Limpieza: `rm -f add.o sub.o mul.o libcalc.a calc_order`

---

### Ejercicio 6: Crear y usar biblioteca dinamica

```bash
gcc -fPIC -c add.c -o add.o
gcc -fPIC -c sub.c -o sub.o
gcc -fPIC -c mul.c -o mul.o
gcc -shared -o libcalc.so add.o sub.o mul.o
```

1. Verifica el tipo: `file libcalc.so`

**Prediccion**: que dice `file`?

<details><summary>Respuesta</summary>

`libcalc.so: ELF 64-bit LSB shared object, x86-64, ...`

A diferencia de un `.a` (que es un "current ar archive"), un `.so` es un archivo ELF compartido. Contiene codigo maquina que se carga en memoria en runtime.

</details>

2. Enlaza: `gcc main_calc.c -L. -lcalc -o calc_dynamic`

3. Ejecuta: `./calc_dynamic`

**Prediccion**: funciona directamente?

<details><summary>Respuesta</summary>

**No**: `error while loading shared libraries: libcalc.so: cannot open shared object file`. El ejecutable sabe que necesita `libcalc.so` pero el loader dinamico no sabe donde buscarla (no esta en `/lib`, `/usr/lib`, ni en la cache de `ldconfig`).

Solucion: `LD_LIBRARY_PATH=. ./calc_dynamic`

</details>

Limpieza: `rm -f add.o sub.o mul.o libcalc.so calc_dynamic`

---

### Ejercicio 7: ldd — comparar dependencias

```bash
gcc -c add.c -o add.o && gcc -c sub.c -o sub.o && gcc -c mul.c -o mul.o
ar rcs libcalc.a add.o sub.o mul.o
gcc -fPIC -c add.c -o add_pic.o && gcc -fPIC -c sub.c -o sub_pic.o && gcc -fPIC -c mul.c -o mul_pic.o
gcc -shared -o libcalc.so add_pic.o sub_pic.o mul_pic.o

gcc main_calc.c libcalc.a -o calc_static
gcc main_calc.c -L. -lcalc -o calc_dynamic
```

1. Compara: `ldd calc_static` vs `LD_LIBRARY_PATH=. ldd calc_dynamic`

**Prediccion**: cual lista `libcalc` como dependencia?

<details><summary>Respuesta</summary>

- `calc_static`: **no** lista `libcalc` — el codigo se copio al binario
- `calc_dynamic`: **si** lista `libcalc.so => ./libcalc.so` — es una dependencia en runtime

Ambos listan `libc.so.6` porque `printf` se enlaza dinamicamente en ambos casos (a menos que uses `-static`).

</details>

2. Que pasa si la `.so` desaparece:
   ```bash
   mv libcalc.so libcalc.so.bak
   ./calc_dynamic 2>&1
   ./calc_static
   mv libcalc.so.bak libcalc.so
   ```

**Prediccion**: cual falla y cual funciona?

<details><summary>Respuesta</summary>

- `calc_dynamic`: **falla** — `cannot open shared object file: No such file or directory`. Depende de la `.so` para ejecutarse.
- `calc_static`: **funciona** — el codigo ya esta dentro del binario, no necesita la biblioteca.

Esta es la diferencia fundamental: estatico = autonomo, dinamico = dependiente.

</details>

Limpieza: `rm -f *.o libcalc.a libcalc.so calc_static calc_dynamic`

---

### Ejercicio 8: -fPIC — por que es necesario

```bash
# Without -fPIC:
gcc -c add.c -o add_nopic.o
gcc -shared -o libcalc_nopic.so add_nopic.o 2>&1
```

**Prediccion**: compila la `.so` sin `-fPIC`?

<details><summary>Respuesta</summary>

En muchas arquitecturas (especialmente x86-64), **falla** o produce un warning:

`relocation R_X86_64_PC32 against symbol ... can not be used when making a shared object; recompile with -fPIC`

`-fPIC` (Position Independent Code) genera codigo que funciona en cualquier direccion de memoria. Es necesario para `.so` porque las bibliotecas dinamicas se cargan en direcciones arbitrarias. Sin `-fPIC`, el codigo asume direcciones fijas que no funcionan cuando el loader carga la `.so` en una posicion diferente.

Nota: en algunos sistemas/configuraciones puede funcionar porque GCC usa `-fPIE` por defecto, pero no es portable. Siempre usar `-fPIC` para `.so`.

</details>

Limpieza: `rm -f add_nopic.o libcalc_nopic.so`

---

### Ejercicio 9: -lm y la separacion libc/libm

```bash
cat > /tmp/mathtest.c << 'EOF'
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main(void) {
    printf("sqrt(2) = %f\n", sqrt(2.0));
    printf("abs(-5) = %d\n", abs(-5));
    return 0;
}
EOF
```

1. Compila sin `-lm`: `gcc -std=c11 -Wall /tmp/mathtest.c -o /tmp/mathtest 2>&1`

**Prediccion**: que error da y sobre que funcion?

<details><summary>Respuesta</summary>

`undefined reference to 'sqrt'`. Solo `sqrt` falla porque esta en `libm`. `abs` compila sin problemas porque esta en `libc` (declarado en `<stdlib.h>`, no en `<math.h>`). `printf` tambien esta en `libc`.

La separacion es historica: en Unix original, las funciones matematicas de punto flotante se pusieron en una biblioteca separada para evitar cargar codigo de FPU en programas que no lo necesitaban.

</details>

2. Compila con `-lm`: `gcc -std=c11 -Wall /tmp/mathtest.c -lm -o /tmp/mathtest && /tmp/mathtest`

Limpieza: `rm -f /tmp/mathtest.c /tmp/mathtest`

---

### Ejercicio 10: readelf y la estructura ELF

```bash
gcc -c labs/add.c -o /tmp/add.o
gcc labs/main_calc.c labs/add.c labs/sub.c labs/mul.c -o /tmp/calculator
```

1. Compara headers ELF: `readelf -h /tmp/add.o | grep Type` y `readelf -h /tmp/calculator | grep Type`

**Prediccion**: el Type del `.o` sera ____ y el del ejecutable ____

<details><summary>Respuesta</summary>

- `.o`: `Type: REL (Relocatable file)` — direcciones sin resolver, no ejecutable
- ejecutable: `Type: DYN (Position-Independent Executable file)` o `EXEC (Executable file)` — listo para ejecutar

GCC moderno genera PIE (Position-Independent Executable) por defecto, que es de tipo `DYN`. Los ejecutables clasicos (compilados con `-no-pie`) son de tipo `EXEC`.

</details>

2. Ve las secciones: `readelf -S /tmp/calculator | grep -E "\.text|\.data|\.bss|\.rodata"`

**Prediccion**: cuantas secciones `.text` tiene el ejecutable?

<details><summary>Respuesta</summary>

**Una sola** `.text`. El enlazador combino las secciones `.text` de todos los `.o` (add.o, sub.o, mul.o, main_calc.o, mas el codigo de inicio de crt*.o) en una unica seccion `.text` en el ejecutable. Lo mismo para `.data`, `.bss`, `.rodata`.

</details>

Limpieza: `rm -f /tmp/add.o /tmp/calculator`
