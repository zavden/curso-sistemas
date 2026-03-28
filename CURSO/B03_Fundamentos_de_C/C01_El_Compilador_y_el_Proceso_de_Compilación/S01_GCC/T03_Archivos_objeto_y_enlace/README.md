# T03 — Archivos objeto y enlace

## Archivos objeto (.o)

Un archivo objeto es el resultado de compilar un archivo `.c` sin
enlazar. Contiene código máquina, pero aún no es un programa
ejecutable:

```bash
gcc -c math.c -o math.o
gcc -c main.c -o main.o

# math.o y main.o son archivos objeto
# Ninguno se puede ejecutar solo — les falta el enlace
```

### Qué contiene un .o

```bash
# Un .o es un archivo ELF relocatable:
file math.o
# math.o: ELF 64-bit LSB relocatable, x86-64, ...

# Tiene secciones:
objdump -h math.o
# .text    — código ejecutable (las funciones)
# .data    — variables globales inicializadas
# .bss     — variables globales sin inicializar (ceros)
# .rodata  — constantes de solo lectura (strings, const)
# .symtab  — tabla de símbolos
# .rela.*  — información de reubicación
```

### Símbolos

Cada `.o` tiene una tabla de símbolos — los nombres de funciones
y variables globales que define o que necesita:

```bash
nm math.o
# 0000000000000000 T add       ← definido aquí (T = text/code)
# 0000000000000000 D counter   ← definido aquí (D = data)
#                  U printf    ← undefined (viene de otro lado)
```

```
# Tipos de símbolos en nm:
# T/t  — definido en .text (código)
# D/d  — definido en .data (datos inicializados)
# B/b  — definido en .bss (datos sin inicializar)
# U    — undefined (necesita resolverse al enlazar)
# Mayúscula = linkage externo (visible), minúscula = local
```

```c
// math.c
int counter = 0;              // símbolo D (data, externo)
static int internal = 5;      // símbolo d (data, local)

int add(int a, int b) {       // símbolo T (text, externo)
    return a + b;
}

static int helper(int x) {    // símbolo t (text, local)
    return x * 2;
}
```

```bash
nm math.o
# 0000000000000000 D counter
# 0000000000000004 d internal
# 0000000000000000 T add
# 0000000000000014 t helper
# static hace los símbolos locales — no visibles fuera del .o
```

### Relocatable — direcciones pendientes

```bash
# Un .o no tiene direcciones finales. Las llamadas a funciones
# externas están marcadas como "por resolver":

objdump -d math.o
# ...
# callq  0x0 <add+0x1a>     ← dirección temporal (0x0)
# El enlazador pondrá la dirección real

# Ver las entradas de reubicación:
objdump -r math.o
# RELOCATION RECORDS FOR [.text]:
# OFFSET    TYPE              VALUE
# 0000001a  R_X86_64_PLT32    printf-0x4
# El enlazador sabe que en offset 0x1a debe poner la dirección de printf
```

## El proceso de enlace

El enlazador (`ld`, invocado automáticamente por `gcc`) toma uno
o más `.o` y las bibliotecas necesarias para producir un ejecutable:

```bash
# Compilar archivos por separado y enlazar:
gcc -c math.c -o math.o
gcc -c main.c -o main.o
gcc math.o main.o -o program

# O todo de una vez (GCC hace las 4 fases internamente):
gcc math.c main.c -o program
```

### Qué hace el enlazador

```
1. Resolución de símbolos
   main.o dice: "necesito add" (símbolo U)
   math.o dice: "yo tengo add" (símbolo T)
   → El enlazador los conecta

2. Combinación de secciones
   Junta todas las .text de todos los .o en una sola .text
   Junta todas las .data, .bss, .rodata...

3. Reubicación
   Asigna direcciones finales a cada sección
   Reemplaza las direcciones temporales por las definitivas

4. Generación del ejecutable
   Escribe el header ELF, tabla de programa, etc.
   El resultado es un archivo que el kernel puede ejecutar
```

### Errores de enlace

```bash
# Error más común — símbolo undefined:
gcc main.o -o program
# undefined reference to `add'
# main.o necesita add pero no enlazamos math.o

# Otro error — definición duplicada:
# Si add está definido en math.o Y en extra.o:
gcc math.o extra.o main.o -o program
# multiple definition of `add'
```

```c
// Para evitar definiciones duplicadas:

// En headers (.h) — solo DECLARACIONES:
int add(int a, int b);        // declaración (prototipo)
extern int counter;            // declaración de variable

// En fuentes (.c) — las DEFINICIONES:
int add(int a, int b) {       // definición
    return a + b;
}
int counter = 0;               // definición
```

## Enlace estático

En el enlace estático, el código de la biblioteca se **copia
dentro** del ejecutable:

```bash
# Una biblioteca estática es un archivo .a — un paquete de .o:
# libmath.a contiene math.o, trig.o, etc.

# Crear una biblioteca estática:
gcc -c add.c -o add.o
gcc -c mul.c -o mul.o
ar rcs libmath.a add.o mul.o

# ar r = insertar/reemplazar
# ar c = crear si no existe
# ar s = escribir índice de símbolos (como ranlib)
```

```bash
# Ver el contenido de un .a:
ar t libmath.a
# add.o
# mul.o

# Ver los símbolos:
nm libmath.a
# add.o:
# 0000000000000000 T add
#
# mul.o:
# 0000000000000000 T mul
```

### Enlazar con una biblioteca estática

```bash
# Con -l (busca libNOMBRE.a o libNOMBRE.so):
gcc main.c -L. -lmath -o program
# -L.     → buscar bibliotecas en el directorio actual
# -lmath  → buscar libmath.a (o libmath.so)

# O directamente:
gcc main.c libmath.a -o program
```

### El orden importa

```bash
# El enlazador procesa los archivos de IZQUIERDA A DERECHA.
# Cuando encuentra un .o, registra sus símbolos undefined.
# Cuando encuentra un .a, busca en él solo los símbolos que necesita.

# INCORRECTO — la biblioteca antes de quien la usa:
gcc -lmath main.c -o program
# El enlazador procesa libmath.a primero, no necesita nada → la ignora
# Luego procesa main.o, necesita add → undefined reference

# CORRECTO — la biblioteca después:
gcc main.c -lmath -o program
# main.o necesita add → busca en libmath.a → lo encuentra

# Regla: las dependencias van DESPUÉS de quien las necesita
# Si A depende de B y B depende de C:
gcc A.o B.o C.o -o program
```

### Características del enlace estático

```
Ventajas:
- Ejecutable autónomo (no necesita .a en runtime)
- No hay problemas de versiones de biblioteca
- Ligeramente más rápido en arranque (no hay resolución dinámica)

Desventajas:
- Ejecutable más grande (copia el código de la biblioteca)
- Si la biblioteca se actualiza, hay que recompilar
- Cada programa que usa la biblioteca tiene su propia copia en memoria
```

## Enlace dinámico

En el enlace dinámico, el ejecutable solo contiene una
**referencia** a la biblioteca. El código se carga en runtime:

```bash
# Una biblioteca dinámica es un .so (shared object):
# libmath.so contiene el código compartido

# Crear una biblioteca dinámica:
gcc -c -fPIC add.c -o add.o
gcc -c -fPIC mul.c -o mul.o
gcc -shared -o libmath.so add.o mul.o

# -fPIC = Position Independent Code (necesario para .so)
# -shared = generar una biblioteca compartida
```

### Enlazar con una biblioteca dinámica

```bash
gcc main.c -L. -lmath -o program

# -lmath busca primero libmath.so, luego libmath.a
# Para forzar estático: -static (todo) o -l:libmath.a (solo esa)
```

```bash
# El ejecutable necesita encontrar el .so en runtime:
./program
# error while loading shared libraries: libmath.so:
# cannot open shared object file: No such file or directory

# ¿Por qué? El ejecutable sabe que necesita libmath.so
# pero el loader no sabe dónde está
```

### Cómo encuentra el loader las bibliotecas

```bash
# El loader dinámico (ld-linux.so) busca en este orden:
# 1. RPATH embebido en el ejecutable (deprecated)
# 2. LD_LIBRARY_PATH (variable de entorno)
# 3. RUNPATH embebido en el ejecutable
# 4. /etc/ld.so.cache (caché de ldconfig)
# 5. /lib, /usr/lib (paths por defecto)

# Soluciones:

# 1. Instalar en un path del sistema:
sudo cp libmath.so /usr/local/lib/
sudo ldconfig    # actualizar el caché

# 2. LD_LIBRARY_PATH (temporal, no recomendado para producción):
LD_LIBRARY_PATH=. ./program

# 3. RPATH (embeber el path en el ejecutable):
gcc main.c -L. -lmath -Wl,-rpath,/opt/mylibs -o program
```

```bash
# Ver qué bibliotecas dinámicas necesita un ejecutable:
ldd program
# linux-vdso.so.1
# libmath.so => /usr/local/lib/libmath.so
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# /lib64/ld-linux-x86-64.so.2

# ldd muestra cada .so y dónde lo encontró
# "not found" indica un problema
```

### Características del enlace dinámico

```
Ventajas:
- Ejecutable más pequeño (no copia el código)
- Múltiples programas comparten una copia en memoria
- Se puede actualizar la biblioteca sin recompilar
- Permite carga en runtime (dlopen/dlsym — plugins)

Desventajas:
- Necesita el .so en runtime (dependencia externa)
- Ligeramente más lento (resolución de símbolos en runtime)
- Posibles conflictos de versiones ("dependency hell")
```

## Comparación directa

```bash
# Compilar el mismo programa de forma estática y dinámica:

# Dinámico (por defecto):
gcc main.c -L. -lmath -o program_dynamic
ls -la program_dynamic
# ~16 KB

# Estático:
gcc -static main.c -L. -lmath -o program_static
ls -la program_static
# ~800 KB (incluye libc completa)

ldd program_dynamic
# libmath.so => ...
# libc.so.6 => ...

ldd program_static
# not a dynamic executable
```

```
| Aspecto | Estático (.a) | Dinámico (.so) |
|---------|---------------|----------------|
| Código en ejecutable | Sí (copia) | No (referencia) |
| Necesita .so en runtime | No | Sí |
| Tamaño del ejecutable | Grande | Pequeño |
| Memoria compartida | No | Sí |
| Actualización de lib | Recompilar | Solo reemplazar .so |
| Velocidad de arranque | Más rápido | Más lento |
| Distribución | Más simple | Necesita dependencias |
```

## Flags de paths: -I, -L, -l

### -I — Path de headers

```bash
# -I agrega un directorio a la búsqueda de #include:
gcc -I./include main.c -o main

# Sin -I:
#include "mylib.h"   # busca en el directorio del .c, luego paths del sistema

# Con -I./include:
#include "mylib.h"   # busca en ./include/ también
```

```bash
# Múltiples -I:
gcc -I./include -I../common/headers main.c -o main
# Busca en cada directorio en orden

# Estructura típica de proyecto:
# proyecto/
# ├── include/       ← headers públicos
# │   └── mylib.h
# ├── src/           ← fuentes
# │   ├── main.c
# │   └── mylib.c
# └── Makefile

gcc -I./include -c src/main.c -o build/main.o
gcc -I./include -c src/mylib.c -o build/mylib.o
gcc build/main.o build/mylib.o -o build/program
```

```c
// Convención de #include:

#include <stdio.h>      // busca en paths del sistema (/usr/include)
#include "myheader.h"   // busca primero en el directorio del .c,
                        // luego en los -I, luego en paths del sistema

// Usar <> para bibliotecas del sistema/externas
// Usar "" para headers del proyecto
```

### -L — Path de bibliotecas

```bash
# -L agrega un directorio a la búsqueda de bibliotecas para el enlazador:
gcc main.c -L./lib -lmath -o program
# -L./lib  → el enlazador busca en ./lib/ además de los paths estándar
```

```bash
# Paths estándar donde busca el enlazador:
# /lib, /usr/lib, /usr/local/lib
# (y lo que esté en /etc/ld.so.conf)

# Múltiples -L:
gcc main.c -L./lib -L/opt/custom/lib -lmath -lcustom -o program
```

### -l — Enlazar con biblioteca

```bash
# -lNOMBRE busca libNOMBRE.so o libNOMBRE.a:
gcc main.c -lm -o program       # busca libm.so (matemáticas)
gcc main.c -lpthread -o program  # busca libpthread.so (threads)
gcc main.c -lssl -lcrypto -o p   # OpenSSL

# Bibliotecas comunes:
# -lm        libm        funciones matemáticas (sin, cos, sqrt)
# -lpthread  libpthread  threads POSIX
# -lrt       librt       funciones realtime (timers, shared memory)
# -ldl       libdl       carga dinámica (dlopen, dlsym)
```

```c
// ¿Por qué -lm es necesario?
#include <math.h>

int main(void) {
    double x = sqrt(2.0);   // sqrt está en libm, no en libc
    return 0;
}

// gcc main.c -o main
// undefined reference to `sqrt'

// gcc main.c -lm -o main
// Funciona — enlaza con libm

// Nota: algunas funciones de math.h (como abs) SÍ están en libc.
// Las que operan con doubles/floats generalmente están en libm.
```

## Herramientas de inspección

```bash
# file — tipo de archivo:
file main.o      # ELF 64-bit LSB relocatable
file program     # ELF 64-bit LSB pie executable
file libmath.a   # current ar archive
file libmath.so  # ELF 64-bit LSB shared object

# nm — tabla de símbolos:
nm main.o        # símbolos de un .o
nm -D libmath.so # símbolos dinámicos de un .so

# ldd — dependencias dinámicas:
ldd program      # qué .so necesita en runtime

# objdump — secciones y desensamblado:
objdump -h main.o    # headers de secciones
objdump -d main.o    # desensamblado
objdump -t main.o    # tabla de símbolos (más detallada que nm)

# readelf — información ELF detallada:
readelf -h program   # header ELF
readelf -S program   # secciones
readelf -d program   # dependencias dinámicas

# size — tamaño de secciones:
size program
# text    data     bss     dec     hex filename
# 1234     567       8    1809     711 program

# strings — cadenas legibles en un binario:
strings program | grep "Hello"
```

## Tabla de flags

| Flag | Fase | Qué hace |
|---|---|---|
| -c | Compilación | Compilar sin enlazar (produce .o) |
| -I | Preprocesador | Agregar directorio de búsqueda de headers |
| -L | Enlace | Agregar directorio de búsqueda de bibliotecas |
| -l | Enlace | Enlazar con una biblioteca |
| -shared | Enlace | Producir una biblioteca dinámica (.so) |
| -fPIC | Compilación | Generar código independiente de posición |
| -static | Enlace | Forzar enlace estático |
| -Wl,opt | Enlace | Pasar opción directamente al enlazador |

---

## Ejercicios

### Ejercicio 1 — Crear y enlazar archivos objeto

```bash
# 1. Crear greet.c con una función greet(const char *name)
# 2. Crear main.c que llame a greet()
# 3. Compilar cada uno a .o por separado
# 4. Ver los símbolos de cada .o con nm
# 5. Enlazar y ejecutar
# 6. ¿Qué pasa si solo enlazas main.o?
```

### Ejercicio 2 — Biblioteca estática

```bash
# 1. Crear add.c (función add) y mul.c (función mul)
# 2. Compilar ambos a .o
# 3. Crear libcalc.a con ar
# 4. Compilar main.c enlazando con -lcalc
# 5. Verificar con nm que los símbolos están en el .a
```

### Ejercicio 3 — Biblioteca dinámica

```bash
# 1. Con los mismos add.c y mul.c, crear libcalc.so
# 2. Compilar main.c enlazando dinámicamente
# 3. Verificar con ldd las dependencias
# 4. ¿Qué error da si borras el .so y ejecutas?
# 5. Comparar el tamaño del ejecutable con enlace estático vs dinámico
```
