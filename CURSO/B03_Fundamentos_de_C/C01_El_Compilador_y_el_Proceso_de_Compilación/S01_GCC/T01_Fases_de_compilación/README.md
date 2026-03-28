# T01 — Fases de compilación

## El pipeline de compilación

Cuando ejecutas `gcc main.c -o main`, parece un solo paso, pero GCC
ejecuta **cuatro fases** internamente. Entender cada fase es fundamental
para diagnosticar errores y controlar el proceso:

```
main.c ──→ Preprocesador ──→ Compilador ──→ Ensamblador ──→ Enlazador ──→ main
            (-E)               (-S)           (-c)           (ld)
           main.i             main.s         main.o         ejecutable
```

```bash
# Compilación completa (las 4 fases de una vez):
gcc main.c -o main

# Parar después de cada fase:
gcc -E main.c -o main.i     # solo preprocesar
gcc -S main.c -o main.s     # preprocesar + compilar (a assembly)
gcc -c main.c -o main.o     # preprocesar + compilar + ensamblar (a objeto)
gcc main.o -o main           # enlazar (objeto a ejecutable)
```

## Fase 1 — Preprocesamiento (-E)

El preprocesador procesa todas las directivas que empiezan con `#`.
Su salida es C puro, sin ninguna directiva:

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
# Generar la salida preprocesada:
gcc -E main.c -o main.i

# Ver el resultado:
wc -l main.i
# ~700 líneas — el contenido de stdio.h se insertó

tail -10 main.i
# int main(void) {
#     printf("Max: %d\n", 100);     ← MAX fue reemplazado por 100
#     return 0;
# }
```

### Qué hace el preprocesador

```c
// 1. Procesa #include — inserta el contenido del archivo:
#include <stdio.h>     // busca en paths del sistema (/usr/include)
#include "myheader.h"  // busca primero en el directorio actual

// 2. Expande #define — reemplaza macros:
#define PI 3.14159
// PI se reemplaza por 3.14159 en todo el archivo

// 3. Evalúa #if, #ifdef, #ifndef — compilación condicional:
#ifdef DEBUG
    printf("debug info\n");
#endif
// Si DEBUG no está definido, el printf desaparece

// 4. Procesa #pragma — directivas al compilador:
#pragma once    // guarda de inclusión (extensión de compilador)

// 5. Elimina comentarios
// Este texto desaparece en main.i
```

```bash
# El preprocesador solo manipula TEXTO — no entiende C.
# No verifica tipos, no busca variables, solo hace
# sustitución de texto y manejo de directivas.

# Usar -E es útil para:
# - Verificar que una macro se expande correctamente
# - Depurar problemas de #include
# - Entender qué código realmente compila el compilador
```

## Fase 2 — Compilación (-S)

El compilador traduce el C preprocesado a **assembly** (código
ensamblador para la arquitectura del host):

```bash
# Generar assembly:
gcc -S main.c -o main.s

# Ver el resultado:
cat main.s
```

```asm
# main.s (x86-64, simplificado):
    .file   "main.c"
    .section .rodata
.LC0:
    .string "Max: %d\n"
    .text
    .globl  main
main:
    pushq   %rbp
    movq    %rsp, %rbp
    movl    $100, %esi          # el 100 de MAX
    leaq    .LC0(%rip), %rdi    # la cadena "Max: %d\n"
    movl    $0, %eax
    call    printf@PLT
    movl    $0, %eax            # return 0
    popq    %rbp
    ret
```

```bash
# El assembly depende de la arquitectura:
# x86-64 → registros como %rax, %rdi, instrucciones como movl, pushq
# ARM    → registros como r0, r1, instrucciones como mov, bl
# RISC-V → registros como a0, a1, instrucciones como li, jal

# El compilador hace MUCHAS cosas en esta fase:
# - Análisis léxico (tokenización)
# - Análisis sintáctico (AST)
# - Análisis semántico (tipos, scopes)
# - Optimizaciones (-O2, -O3)
# - Generación de código
# - Asignación de registros
```

```bash
# Comparar assembly con y sin optimización:
gcc -S -O0 main.c -o main_O0.s    # sin optimización
gcc -S -O2 main.c -o main_O2.s    # con optimización

wc -l main_O0.s main_O2.s
# main_O0.s puede tener más instrucciones que main_O2.s
# El optimizador elimina código innecesario, reordena, inline...
```

## Fase 3 — Ensamblado (-c)

El ensamblador convierte el assembly en **código máquina** (binario),
produciendo un archivo objeto `.o`:

```bash
# Generar archivo objeto:
gcc -c main.c -o main.o

# Es un archivo binario:
file main.o
# main.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped

# Ver las secciones:
objdump -h main.o
# .text    — código ejecutable
# .data    — variables globales inicializadas
# .bss     — variables globales sin inicializar (ceros)
# .rodata  — constantes de solo lectura (strings, const)

# Ver los símbolos:
nm main.o
#                  U printf     ← undefined (viene de libc)
# 0000000000000000 T main       ← definido aquí (T = text/code)
```

### Relocatable — aún no tiene direcciones finales

```bash
# Un archivo .o es "relocatable" — las direcciones son relativas.
# No sabe dónde estará printf en memoria. Solo dice:
# "aquí necesito la dirección de printf"

# El enlazador resolverá estas referencias en la siguiente fase.

# objdump -d muestra el código desensamblado:
objdump -d main.o
# Verás cosas como:
# callq  0 <main+0x1a>    ← la dirección de printf es 0 (por resolver)
```

## Fase 4 — Enlace (Linking)

El enlazador (`ld`, invocado por GCC) combina uno o más archivos `.o`
con las bibliotecas necesarias para producir el ejecutable final:

```bash
# Enlazar:
gcc main.o -o main

# O compilar y enlazar todo de una vez:
gcc main.c -o main
```

### Qué hace el enlazador

```bash
# 1. Resuelve símbolos:
#    main.o dice "necesito printf" (símbolo undefined)
#    libc.so dice "yo tengo printf" (símbolo definido)
#    El enlazador conecta la referencia con la definición

# 2. Combina secciones:
#    Junta todas las .text de todos los .o en una sola .text
#    Junta todas las .data, .bss, .rodata...

# 3. Asigna direcciones finales:
#    Decide dónde va cada sección en el espacio de direcciones
#    Reemplaza las direcciones relativas por absolutas

# 4. Genera el ejecutable ELF:
#    Agrega el header ELF, tabla de secciones, etc.
```

### Múltiples archivos

```c
// math_utils.c
int add(int a, int b) {
    return a + b;
}

// main.c
#include <stdio.h>
int add(int a, int b);  // declaración (prototipo)

int main(void) {
    printf("%d\n", add(3, 4));
    return 0;
}
```

```bash
# Compilar cada archivo por separado:
gcc -c math_utils.c -o math_utils.o
gcc -c main.c -o main.o

# Enlazar ambos:
gcc math_utils.o main.o -o program

# O todo de una vez:
gcc math_utils.c main.c -o program
```

### Errores de enlace

```bash
# Si olvidas enlazar un .o:
gcc main.o -o program
# undefined reference to `add'
# El enlazador no encuentra la definición de add

# Si defines algo dos veces:
# (add definido en math_utils.o Y en extra.o)
gcc math_utils.o extra.o main.o -o program
# multiple definition of `add'
```

## El ejecutable final

```bash
# Ver qué tipo de archivo es:
file main
# main: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV),
# dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, ...

# Ver las bibliotecas dinámicas que necesita:
ldd main
# linux-vdso.so.1
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# /lib64/ld-linux-x86-64.so.2

# Ver los símbolos del ejecutable:
nm main | head -10

# Ver las secciones:
size main
# text    data     bss     dec     hex filename
# 1234     567       8    1809     711 main
```

## Resumen visual

```
      Fase 1            Fase 2           Fase 3          Fase 4
   Preprocesador      Compilador      Ensamblador      Enlazador
  ┌─────────────┐  ┌─────────────┐  ┌────────────┐  ┌──────────┐
  │ #include    │  │ C → ASM     │  │ ASM → bin  │  │ .o + lib │
  │ #define     │  │ Tipos       │  │ Código     │  │ Resolver │
  │ #ifdef      │  │ Optimizar   │  │ máquina    │  │ símbolos │
  │ Comentarios │  │ Registros   │  │ relocatable│  │ Ejecutable│
  └──────┬──────┘  └──────┬──────┘  └─────┬──────┘  └────┬─────┘
   .c → .i          .i → .s          .s → .o         .o → ELF

   gcc -E            gcc -S           gcc -c          gcc (ld)
```

## Resumen de archivos

| Extensión | Nombre | Contenido | Fase que lo produce |
|---|---|---|---|
| .c | Código fuente | C con directivas | — (tú lo escribes) |
| .h | Header | Declaraciones, macros | — (tú lo escribes) |
| .i | Preprocesado | C puro expandido | Preprocesador (-E) |
| .s | Assembly | Ensamblador | Compilador (-S) |
| .o | Objeto | Código máquina relocatable | Ensamblador (-c) |
| (sin ext) | Ejecutable | ELF, código máquina final | Enlazador |
| .a | Archivo estático | Colección de .o | ar |
| .so | Biblioteca dinámica | Código compartido | gcc -shared |

---

## Ejercicios

### Ejercicio 1 — Paso a paso

```bash
# Crear un archivo simple y ejecutar cada fase:
cat > hello.c << 'EOF'
#include <stdio.h>
#define GREETING "Hello"

int main(void) {
    printf("%s, world!\n", GREETING);
    return 0;
}
EOF

# 1. Preprocesar y ver cuántas líneas genera stdio.h:
gcc -E hello.c | wc -l

# 2. Generar assembly y buscar la cadena "Hello":
gcc -S hello.c && grep -i hello hello.s

# 3. Generar objeto y ver los símbolos:
gcc -c hello.c && nm hello.o
```

### Ejercicio 2 — Múltiples archivos

```bash
# 1. Crear dos archivos (greet.c con una función, main.c que la usa)
# 2. Compilar cada uno a .o por separado
# 3. Enlazar ambos
# 4. ¿Qué error da si solo enlazas main.o?
```

### Ejercicio 3 — Comparar optimización

```bash
# Generar assembly con -O0 y -O2 para el mismo código
# ¿Cuántas líneas tiene cada uno?
# ¿Qué diferencias notas?
gcc -S -O0 hello.c -o hello_O0.s
gcc -S -O2 hello.c -o hello_O2.s
wc -l hello_O0.s hello_O2.s
```
