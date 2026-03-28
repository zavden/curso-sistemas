# Fases de compilacion

> Sin erratas detectadas en el material base.

---

## Indice

1. [El pipeline de compilacion](#1-el-pipeline-de-compilacion)
2. [Fase 1 — Preprocesamiento (-E)](#2-fase-1--preprocesamiento--e)
3. [Fase 2 — Compilacion (-S)](#3-fase-2--compilacion--s)
4. [Fase 3 — Ensamblado (-c)](#4-fase-3--ensamblado--c)
5. [Fase 4 — Enlace (Linking)](#5-fase-4--enlace-linking)
6. [El ejecutable final](#6-el-ejecutable-final)
7. [Compilacion multi-archivo](#7-compilacion-multi-archivo)
8. [Errores de enlace](#8-errores-de-enlace)
9. [Resumen de archivos](#9-resumen-de-archivos)
10. [Ejercicios](#10-ejercicios)

---

## 1. El pipeline de compilacion

Cuando ejecutas `gcc main.c -o main`, parece un solo paso, pero GCC ejecuta **cuatro fases** internamente. Entender cada fase es fundamental para diagnosticar errores y controlar el proceso:

```
main.c --> Preprocesador --> Compilador --> Ensamblador --> Enlazador --> main
            (-E)              (-S)           (-c)           (ld)
           main.i            main.s         main.o         ejecutable
```

```bash
# Full compilation (all 4 phases at once):
gcc main.c -o main

# Stop after each phase:
gcc -E main.c -o main.i     # preprocess only
gcc -S main.c -o main.s     # preprocess + compile (to assembly)
gcc -c main.c -o main.o     # preprocess + compile + assemble (to object)
gcc main.o -o main           # link (object to executable)
```

### Resumen visual

```
      Fase 1            Fase 2           Fase 3          Fase 4
   Preprocesador      Compilador      Ensamblador      Enlazador
  +--------------+  +--------------+  +-------------+  +-----------+
  | #include     |  | C -> ASM     |  | ASM -> bin  |  | .o + lib  |
  | #define      |  | Tipos        |  | Codigo      |  | Resolver  |
  | #ifdef       |  | Optimizar    |  | maquina     |  | simbolos  |
  | Comentarios  |  | Registros    |  | relocatable |  | Ejecutable|
  +------+-------+  +------+-------+  +------+------+  +-----+-----+
   .c -> .i          .i -> .s          .s -> .o         .o -> ELF

   gcc -E            gcc -S           gcc -c          gcc (ld)
```

---

## 2. Fase 1 — Preprocesamiento (-E)

El preprocesador procesa todas las directivas que empiezan con `#`. Su salida es C puro, sin ninguna directiva:

```c
// main.c
#include <stdio.h>
#define MAX 100

int main(void) {
    printf("Max: %d\n", MAX);
    return 0;
}
```

```bash
# Generate preprocessed output:
gcc -E main.c -o main.i

# Check the result:
wc -l main.i
# ~700 lines - the content of stdio.h was inserted

tail -10 main.i
# int main(void) {
#     printf("Max: %d\n", 100);     <- MAX was replaced by 100
#     return 0;
# }
```

### Que hace el preprocesador

```c
// 1. Processes #include - inserts file contents:
#include <stdio.h>     // searches system paths (/usr/include)
#include "myheader.h"  // searches current directory first

// 2. Expands #define - replaces macros:
#define PI 3.14159
// PI is replaced by 3.14159 throughout the file

// 3. Evaluates #if, #ifdef, #ifndef - conditional compilation:
#ifdef DEBUG
    printf("debug info\n");
#endif
// If DEBUG is not defined, the printf disappears

// 4. Processes #pragma - compiler directives:
#pragma once    // include guard (compiler extension)

// 5. Removes comments
// This text disappears in main.i
```

El preprocesador solo manipula **texto** — no entiende C. No verifica tipos, no busca variables, solo hace sustitucion de texto y manejo de directivas.

Usar `-E` es util para:
- Verificar que una macro se expande correctamente
- Depurar problemas de `#include`
- Entender que codigo realmente compila el compilador

---

## 3. Fase 2 — Compilacion (-S)

El compilador traduce el C preprocesado a **assembly** (codigo ensamblador para la arquitectura del host):

```bash
# Generate assembly:
gcc -S main.c -o main.s
```

```asm
# main.s (x86-64, simplified):
    .file   "main.c"
    .section .rodata
.LC0:
    .string "Max: %d\n"
    .text
    .globl  main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $100, %esi          # the 100 from MAX
    leaq    .LC0(%rip), %rdi    # the string "Max: %d\n"
    movl    $0, %eax
    call    printf@PLT
    movl    $0, %eax            # return 0
    popq    %rbp
    ret
```

El assembly depende de la arquitectura:
- x86-64: registros como `%rax`, `%rdi`, instrucciones como `movl`, `pushq`
- ARM: registros como `r0`, `r1`, instrucciones como `mov`, `bl`
- RISC-V: registros como `a0`, `a1`, instrucciones como `li`, `jal`

El compilador hace muchas cosas en esta fase:
- Analisis lexico (tokenizacion)
- Analisis sintactico (AST)
- Analisis semantico (tipos, scopes)
- Optimizaciones (`-O2`, `-O3`)
- Generacion de codigo
- Asignacion de registros

```bash
# Compare assembly with and without optimization:
gcc -S -O0 main.c -o main_O0.s    # no optimization
gcc -S -O2 main.c -o main_O2.s    # with optimization

wc -l main_O0.s main_O2.s
# main_O0.s may have more instructions than main_O2.s
# The optimizer removes unnecessary code, reorders, inlines...
```

---

## 4. Fase 3 — Ensamblado (-c)

El ensamblador convierte el assembly en **codigo maquina** (binario), produciendo un archivo objeto `.o`:

```bash
# Generate object file:
gcc -c main.c -o main.o

# It's a binary file:
file main.o
# main.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped

# View sections:
objdump -h main.o
# .text    - executable code
# .data    - initialized global variables
# .bss     - uninitialized global variables (zeros)
# .rodata  - read-only constants (strings, const)

# View symbols:
nm main.o
#                  U printf     <- undefined (comes from libc)
# 0000000000000000 T main       <- defined here (T = text/code)
```

### Relocatable — aun no tiene direcciones finales

Un archivo `.o` es "relocatable" — las direcciones son relativas. No sabe donde estara `printf` en memoria. Solo dice: "aqui necesito la direccion de printf". El enlazador resolvera estas referencias en la siguiente fase.

```bash
# objdump -d shows disassembled code:
objdump -d main.o
# You'll see things like:
# callq  0 <main+0x1a>    <- printf's address is 0 (to be resolved)
```

---

## 5. Fase 4 — Enlace (Linking)

El enlazador (`ld`, invocado por GCC) combina uno o mas archivos `.o` con las bibliotecas necesarias para producir el ejecutable final:

```bash
# Link:
gcc main.o -o main

# Or compile and link everything at once:
gcc main.c -o main
```

### Que hace el enlazador

1. **Resuelve simbolos**: `main.o` dice "necesito printf" (simbolo undefined), `libc.so` dice "yo tengo printf" (simbolo definido). El enlazador conecta la referencia con la definicion.

2. **Combina secciones**: junta todas las `.text` de todos los `.o` en una sola `.text`. Lo mismo con `.data`, `.bss`, `.rodata`.

3. **Asigna direcciones finales**: decide donde va cada seccion en el espacio de direcciones. Reemplaza las direcciones relativas por absolutas.

4. **Genera el ejecutable ELF**: agrega el header ELF, tabla de secciones, etc.

---

## 6. El ejecutable final

```bash
# File type:
file main
# main: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
# dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, ...

# Dynamic libraries it needs:
ldd main
# linux-vdso.so.1
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# /lib64/ld-linux-x86-64.so.2

# Symbols:
nm main | head -10

# Section sizes:
size main
# text    data     bss     dec     hex filename
# 1234     567       8    1809     711 main
```

---

## 7. Compilacion multi-archivo

```c
// math_utils.h
#ifndef MATH_UTILS_H
#define MATH_UTILS_H
int add(int a, int b);
int multiply(int a, int b);
#endif

// math_utils.c
#include "math_utils.h"
int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }

// main.c
#include <stdio.h>
#include "math_utils.h"
int main(void) {
    int a = 7, b = 5;
    printf("%d + %d = %d\n", a, b, add(a, b));
    printf("%d * %d = %d\n", a, b, multiply(a, b));
    return 0;
}
```

```bash
# Compile each file separately:
gcc -c math_utils.c -o math_utils.o
gcc -c main.c -o main.o

# Link both:
gcc math_utils.o main.o -o program

# Or all at once:
gcc math_utils.c main.c -o program
```

---

## 8. Errores de enlace

### undefined reference

```bash
# If you forget to link a .o:
gcc main.o -o program
# undefined reference to `add'
# The linker can't find the definition of add
```

El enlazador encontro `add` como undefined en `main.o` pero no encontro ninguna definicion. Sin `math_utils.o`, no hay quien defina ese simbolo.

### multiple definition

```bash
# If something is defined twice (add in math_utils.o AND in extra.o):
gcc math_utils.o extra.o main.o -o program
# multiple definition of `add'
```

Dos archivos objeto definen `add` — el enlazador no sabe cual usar.

---

## 9. Resumen de archivos

| Extension | Nombre | Contenido | Fase que lo produce |
|---|---|---|---|
| `.c` | Codigo fuente | C con directivas | — (tu lo escribes) |
| `.h` | Header | Declaraciones, macros | — (tu lo escribes) |
| `.i` | Preprocesado | C puro expandido | Preprocesador (`-E`) |
| `.s` | Assembly | Ensamblador | Compilador (`-S`) |
| `.o` | Objeto | Codigo maquina relocatable | Ensamblador (`-c`) |
| (sin ext) | Ejecutable | ELF, codigo maquina final | Enlazador |
| `.a` | Archivo estatico | Coleccion de `.o` | `ar` |
| `.so` | Biblioteca dinamica | Codigo compartido | `gcc -shared` |

---

## 10. Ejercicios

Los archivos fuente del laboratorio estan en `labs/`. Usa ese directorio como base.

### Ejercicio 1: Preprocesamiento y expansion de macros

Usando `labs/phases.c`:

```bash
cd labs/
gcc -E phases.c -o phases.i
```

1. Ejecuta `wc -l phases.c phases.i`.

**Prediccion**: `phases.c` tiene 11 lineas. `phases.i` tendra aproximadamente ____ lineas.

<details><summary>Respuesta</summary>

**~700-900 lineas** (depende de la version de glibc). Todo el contenido de `stdio.h` y los headers que este incluye se insertan literalmente. Un archivo de 11 lineas se convierte en cientos porque `stdio.h` arrastra definiciones de tipos, prototipos de funciones, macros internas del sistema, etc.

</details>

2. Ejecuta `tail -15 phases.i`.

**Prediccion**: la macro `GREETING` aparecera como ____ y `REPEAT` como ____

<details><summary>Respuesta</summary>

- `GREETING` fue reemplazado por **`"Hello"`** (el texto literal de la macro)
- `REPEAT` fue reemplazado por **`3`** (el valor numerico)

No quedan directivas `#define` ni `#include` — el preprocesador las proceso todas. Los comentarios tambien desaparecieron. Lo que queda es C puro listo para la fase de compilacion.

</details>

3. Limpieza: `rm -f phases.i`

---

### Ejercicio 2: De C a assembly

```bash
gcc -S phases.c -o phases.s
cat phases.s
```

1. Busca la cadena `"Hello"` en el archivo.

**Prediccion**: en que seccion del assembly aparecera la cadena?

<details><summary>Respuesta</summary>

En la seccion **`.rodata`** (read-only data). Los strings literales son constantes que no cambian durante la ejecucion, asi que el compilador los coloca en una seccion de solo lectura. Veras algo como:

```asm
    .section .rodata
.LC0:
    .string "Hello, world! (%d)\n"
```

</details>

2. Busca la instruccion `call`.

**Prediccion**: a que funcion llama?

<details><summary>Respuesta</summary>

A **`printf@PLT`**. `@PLT` indica que la llamada pasa por la Procedure Linkage Table, el mecanismo de enlace dinamico. En el `.o`, la direccion exacta aun no esta resuelta; el enlazador dinamico la completara en runtime.

</details>

3. Limpieza: `rm -f phases.s`

---

### Ejercicio 3: Archivo objeto y tabla de simbolos

```bash
gcc -c phases.c -o phases.o
```

1. Ejecuta `file phases.o`.

**Prediccion**: la palabra clave que distingue un `.o` de un ejecutable es ____

<details><summary>Respuesta</summary>

**`relocatable`**. La salida dice `ELF 64-bit LSB relocatable`. Un archivo objeto es relocatable porque sus direcciones son relativas — el enlazador las convertira en direcciones absolutas. Un ejecutable dice `pie executable` o `executable`.

</details>

2. Ejecuta `nm phases.o`.

**Prediccion**: `main` aparecera con tipo ____ y `printf` con tipo ____

<details><summary>Respuesta</summary>

- `main`: **`T`** (Text/code — definido aqui, con direccion 0x0000...)
- `printf`: **`U`** (Undefined — se necesita pero no esta definido en este archivo)

`T` = el simbolo esta definido en la seccion `.text` de este archivo.
`U` = el simbolo esta referenciado pero no definido — el enlazador debe encontrarlo en otro `.o` o en una biblioteca.

</details>

3. Limpieza: `rm -f phases.o`

---

### Ejercicio 4: Del objeto al ejecutable

```bash
gcc -c phases.c -o phases.o
gcc phases.o -o phases
```

1. Ejecuta `file phases.o` y `file phases`. Compara las salidas.

**Prediccion**: la diferencia clave entre ambos es ____

<details><summary>Respuesta</summary>

- `phases.o`: `ELF 64-bit LSB **relocatable**` — direcciones sin resolver
- `phases`: `ELF 64-bit LSB **pie executable**, dynamically linked` — direcciones resueltas, enlazado con libc

El ejecutable ademas muestra `interpreter /lib64/ld-linux-x86-64.so.2` — el enlazador dinamico que cargara libc en runtime.

</details>

2. Ejecuta `nm phases | grep -E "main|printf"`.

**Prediccion**: `main` tiene direccion real o sigue en 0? `printf` sigue como `U`?

<details><summary>Respuesta</summary>

- `main`: tiene una **direccion real** (ya no es 0x0000...). El enlazador le asigno una posicion en el ejecutable.
- `printf`: sigue como **`U`** en `nm` regular, porque se enlaza **dinamicamente**. Se resolvera en runtime, no en link time. Con `nm -D phases | grep printf` puedes verlo como simbolo dinamico.

Esto es normal con bibliotecas compartidas (`libc.so`). Si se enlazara estaticamente (`gcc -static`), `printf` tendria una direccion fija.

</details>

3. Ejecuta `ldd phases`.

**Prediccion**: de que biblioteca viene `printf`?

<details><summary>Respuesta</summary>

De **`libc.so.6`** (`/lib/x86_64-linux-gnu/libc.so.6` o similar). `libc` es la biblioteca estandar de C que contiene `printf`, `malloc`, `strcmp` y cientos de funciones mas. Practicamente todo programa en C la necesita.

Tambien veras `linux-vdso.so.1` (optimizaciones del kernel) y `ld-linux-x86-64.so.2` (el propio enlazador dinamico).

</details>

4. Limpieza: `rm -f phases.o phases`

---

### Ejercicio 5: Compilacion multi-archivo — simbolos

Usando los archivos en `labs/`:

```bash
gcc -c math_utils.c -o math_utils.o
gcc -c main_multi.c -o main_multi.o
```

1. Ejecuta `nm math_utils.o`.

**Prediccion**: `add` y `multiply` apareceran con tipo ____

<details><summary>Respuesta</summary>

Ambas con tipo **`T`** (Text — definidas aqui). `math_utils.c` es donde vive la implementacion de estas funciones. Las direcciones seran `0x0000...` (relativas, por ser relocatable).

```
0000000000000000 T add
0000000000000014 T multiply
```

Nota: `multiply` tiene un offset diferente de 0 porque va despues de `add` en la seccion `.text`.

</details>

2. Ejecuta `nm main_multi.o`.

**Prediccion**: `add`, `multiply` y `printf` apareceran con tipo ____, y `main` con tipo ____

<details><summary>Respuesta</summary>

- `add`: **`U`** (undefined — se usa pero no se define aqui)
- `multiply`: **`U`** (misma razon)
- `printf`: **`U`** (viene de libc)
- `main`: **`T`** (definida aqui)

`main_multi.c` incluye `math_utils.h` que solo tiene **declaraciones** (prototipos). Las declaraciones le dicen al compilador "esta funcion existe y tiene esta firma", pero no proveen la implementacion — por eso quedan como `U`.

</details>

3. Limpieza: `rm -f math_utils.o main_multi.o`

---

### Ejercicio 6: Enlace exitoso y errores

```bash
gcc -c math_utils.c -o math_utils.o
gcc -c main_multi.c -o main_multi.o
```

1. Enlaza ambos: `gcc math_utils.o main_multi.o -o calculator && ./calculator`

**Prediccion**: la salida sera ____

<details><summary>Respuesta</summary>

```
7 + 5 = 12
7 * 5 = 35
```

El enlazador conecto los `U` de `main_multi.o` (`add`, `multiply`) con los `T` de `math_utils.o`. Todos los simbolos resueltos.

</details>

2. Intenta enlazar solo `main_multi.o`: `gcc main_multi.o -o calculator_broken`

**Prediccion**: el error sera ____

<details><summary>Respuesta</summary>

```
undefined reference to `add'
undefined reference to `multiply'
```

El enlazador encontro `add` y `multiply` como `U` en `main_multi.o` pero no tiene ninguna definicion (`T`) para ellos. Sin `math_utils.o`, nadie provee esas funciones. `printf` no da error porque se resuelve contra `libc.so`.

</details>

3. Provoca "multiple definition":
   ```bash
   cat > /tmp/extra_math.c << 'EOF'
   int add(int a, int b) { return a + b + 1; }
   EOF
   gcc -c /tmp/extra_math.c -o /tmp/extra_math.o
   gcc math_utils.o /tmp/extra_math.o main_multi.o -o calculator_dup
   ```

**Prediccion**: el error sera ____

<details><summary>Respuesta</summary>

```
multiple definition of `add'
```

Dos archivos objeto definen `add` (ambos con `T`). El enlazador no sabe cual usar — esto viola la regla de una sola definicion (ODR). Cada simbolo debe estar definido **exactamente una vez** en el conjunto de archivos enlazados.

</details>

4. Limpieza: `rm -f math_utils.o main_multi.o calculator calculator_broken /tmp/extra_math.c /tmp/extra_math.o`

---

### Ejercicio 7: Todo de una vez vs por separado

```bash
# Method A: separate compilation + linking
gcc -c math_utils.c -o math_utils.o
gcc -c main_multi.c -o main_multi.o
gcc math_utils.o main_multi.o -o calc_separate

# Method B: all at once
gcc math_utils.c main_multi.c -o calc_onestep
```

1. Ejecuta ambos y compara las salidas.

**Prediccion**: las salidas seran ____ (iguales/diferentes)?

<details><summary>Respuesta</summary>

**Iguales**. Ambos metodos producen el mismo resultado. El metodo B simplemente ejecuta las 4 fases para cada `.c` y luego enlaza todos los `.o` resultantes — es equivalente al metodo A pero en un solo comando.

La ventaja del metodo A (compilacion separada) es que si modificas solo un archivo, solo necesitas recompilar ese `.o` y re-enlazar. Con el metodo B, GCC recompila todo cada vez. Esto importa en proyectos grandes (miles de archivos) — `make` automatiza esta compilacion incremental.

</details>

2. Limpieza: `rm -f math_utils.o main_multi.o calc_separate calc_onestep`

---

### Ejercicio 8: Optimizacion — dead code elimination

Usando `labs/optimize.c`:

```bash
gcc -S -O0 optimize.c -o optimize_O0.s
gcc -S -O2 optimize.c -o optimize_O2.s
```

1. Busca la cadena "This never runs" en ambos:
   ```bash
   grep -c "This never runs" optimize_O0.s
   grep -c "This never runs" optimize_O2.s
   ```

**Prediccion**: con -O0 aparece ____ veces, con -O2 aparece ____ veces.

<details><summary>Respuesta</summary>

- Con `-O0`: **1** vez — el compilador genera codigo fielmente para todo, incluyendo el bloque `if (0)`. La cadena esta en `.rodata` aunque nunca se ejecute.
- Con `-O2`: **0** veces — el optimizador detecto que `if (0)` nunca es verdadero y elimino todo el bloque, incluyendo la cadena. Esto se llama **dead code elimination**.

El optimizador es lo suficientemente inteligente para saber que `0` es siempre falso y que el bloque interior es codigo muerto que puede eliminarse sin cambiar el comportamiento del programa.

</details>

---

### Ejercicio 9: Optimizacion — constant folding e inlining

Con los mismos archivos del ejercicio anterior:

1. Busca los valores `2`, `3` y `5` en ambos assemblies:
   ```bash
   grep -n '\$2\|\$3\|\$5' optimize_O0.s | head -5
   grep -n '\$2\|\$3\|\$5' optimize_O2.s | head -5
   ```

**Prediccion**: en -O0, `val = 2 + 3` se calculara con instrucciones de ____ (suma en runtime). En -O2, el valor ____ aparecera directamente.

<details><summary>Respuesta</summary>

- Con `-O0`: veras los valores `$2` y `$3` por separado, con una instruccion de suma en runtime (`addl` o similar). El compilador traduce el codigo literalmente.
- Con `-O2`: veras directamente `$5`. El compilador calculo `2 + 3 = 5` durante la compilacion y solo emite el resultado. Esto se llama **constant folding** — evaluar expresiones constantes en tiempo de compilacion.

</details>

2. Busca llamadas a `square` en ambos:
   ```bash
   grep "call.*square" optimize_O0.s
   grep "call.*square" optimize_O2.s
   ```

**Prediccion**: con -O0 habra ____ llamadas a `square`, con -O2 habra ____

<details><summary>Respuesta</summary>

- Con `-O0`: **2** llamadas (`call square`) — una desde `main` y otra desde `dead_code_example`. El compilador respeta la estructura del codigo fuente.
- Con `-O2`: probablemente **0** llamadas — el optimizador **inlineo** la funcion `square()`. En vez de emitir una instruccion `call`, copio el cuerpo de la funcion (`x * x` → una sola instruccion `imull`) directamente en el punto de llamada. Esto elimina el overhead de llamada a funcion (push/pop del stack frame, call/ret).

</details>

3. Compara el tamano total:
   ```bash
   wc -l optimize_O0.s optimize_O2.s
   ```

**Prediccion**: cual es mas grande?

<details><summary>Respuesta</summary>

**`-O0` es mas grande** (mas lineas de assembly). Sin optimizacion, el compilador genera codigo verboso: guarda cada variable en el stack, hace llamadas a funciones, incluye dead code. Con `-O2`, el optimizador elimina codigo muerto, inlinea funciones, precalcula constantes, y usa registros en lugar del stack. El resultado puede ser la mitad del tamano o menos.

</details>

4. Limpieza: `rm -f optimize_O0.s optimize_O2.s`

---

### Ejercicio 10: Secciones y tamanos con objdump y size

```bash
gcc -c phases.c -o phases.o
gcc phases.o -o phases
```

1. Ejecuta `objdump -h phases.o` y busca las secciones `.text`, `.rodata`, `.data`, `.bss`.

**Prediccion**: `.data` y `.bss` tendran tamano ____ porque ____

<details><summary>Respuesta</summary>

**Tamano 0** (o no apareceran) — porque `phases.c` no tiene variables globales. Cada seccion contiene un tipo especifico de datos:

- `.text`: codigo ejecutable — tendra los bytes de las instrucciones de `main`
- `.rodata`: la cadena `"Hello, world! (%d)\n"` — unos ~20 bytes
- `.data`: variables globales inicializadas — **vacia** (no hay `int global = 5;`)
- `.bss`: variables globales sin inicializar — **vacia** (no hay `int counter;` a nivel global)

</details>

2. Ejecuta `size phases.o` y `size phases`. Compara.

**Prediccion**: el ejecutable sera ____ (mas grande/igual/mas pequeno) que el `.o`, y la razon es ____

<details><summary>Respuesta</summary>

El ejecutable es **significativamente mas grande**. El `.o` solo tiene el codigo de `main` (~100 bytes de text). El ejecutable incluye ademas:

- Codigo de inicio (`_start`, `__libc_start_main` desde `crt0.o`, `crti.o`, `crtn.o`) que prepara el entorno antes de llamar a `main`
- La tabla PLT (Procedure Linkage Table) para enlace dinamico con `printf`
- La GOT (Global Offset Table) para direcciones de simbolos dinamicos
- Secciones adicionales de metadata ELF

Esto es el "overhead" minimo de cualquier ejecutable enlazado dinamicamente.

</details>

3. Limpieza: `rm -f phases.o phases`
