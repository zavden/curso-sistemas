# T01 — Creación de bibliotecas estáticas

## Qué es una biblioteca estática

Una biblioteca estática es un **archivo** que contiene uno o más
archivos objeto (`.o`) empaquetados juntos. En el enlace, el linker
**copia** el código necesario directamente dentro del ejecutable:

```
Compilación:
  math.c  → gcc -c → math.o  ─┐
  string.c → gcc -c → string.o ┼─→ ar rcs libutils.a math.o string.o
  io.c    → gcc -c → io.o    ─┘

Enlace:
  main.o + libutils.a → gcc → programa
  (el linker copia solo los .o que main.o necesita)
```

```c
// Convención de nombres:
// lib<nombre>.a
//
// lib    — prefijo obligatorio
// nombre — nombre de la biblioteca
// .a     — extensión (archive)
//
// Ejemplos:
// libmath.a
// libparser.a
// libmyproject.a
```

## Paso a paso: crear una biblioteca estática

### 1. Escribir el código fuente

```c
// --- vec3.h ---
#ifndef VEC3_H
#define VEC3_H

typedef struct {
    double x, y, z;
} Vec3;

Vec3   vec3_create(double x, double y, double z);
Vec3   vec3_add(Vec3 a, Vec3 b);
Vec3   vec3_scale(Vec3 v, double s);
double vec3_dot(Vec3 a, Vec3 b);
double vec3_length(Vec3 v);
void   vec3_print(Vec3 v);

#endif
```

```c
// --- vec3.c ---
#include "vec3.h"
#include <stdio.h>
#include <math.h>

Vec3 vec3_create(double x, double y, double z) {
    return (Vec3){x, y, z};
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_scale(Vec3 v, double s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double vec3_length(Vec3 v) {
    return sqrt(vec3_dot(v, v));
}

void vec3_print(Vec3 v) {
    printf("(%.2f, %.2f, %.2f)\n", v.x, v.y, v.z);
}
```

### 2. Compilar a archivos objeto

```bash
# -c: compilar sin enlazar (produce .o)
gcc -c -Wall -Wextra -O2 vec3.c -o vec3.o

# Para múltiples archivos:
gcc -c -Wall -Wextra -O2 vec3.c matrix.c utils.c
# Produce: vec3.o matrix.o utils.o
```

```bash
# Verificar el contenido del .o:
nm vec3.o
# T vec3_create    ← T = text (función definida)
# T vec3_add
# T vec3_dot
# U sqrt           ← U = undefined (necesita libm)
# U printf         ← U = undefined (necesita libc)

file vec3.o
# vec3.o: ELF 64-bit LSB relocatable, x86-64, ...
# "relocatable" = no tiene direcciones finales → listo para enlazar
```

### 3. Crear el archivo .a con ar

```bash
# ar — archiver, empaqueta .o en un .a:
ar rcs libvec3.a vec3.o

# Flags:
# r — replace: insertar archivos (reemplazar si ya existen)
# c — create: crear el archivo si no existe (suprime warning)
# s — index: crear índice de símbolos (equivale a ranlib)

# Con múltiples archivos:
ar rcs libmath3d.a vec3.o matrix.o transform.o
```

```bash
# ranlib — crear/actualizar el índice de símbolos:
ranlib libvec3.a

# El índice permite al linker encontrar símbolos rápidamente.
# ar rcs ya incluye ranlib, pero en algunos sistemas antiguos
# era necesario ejecutarlo por separado.

# En GNU ar, 's' y ranlib hacen lo mismo.
# En la práctica moderna: ar rcs es suficiente.
```

### 4. Verificar el contenido

```bash
# Listar los archivos dentro del .a:
ar t libvec3.a
# vec3.o

ar t libmath3d.a
# vec3.o
# matrix.o
# transform.o

# Listar símbolos:
nm libvec3.a
# vec3.o:
# 0000000000000000 T vec3_create
# 0000000000000030 T vec3_add
# ...

# Extraer un .o del .a (rara vez necesario):
ar x libmath3d.a vec3.o
```

## Usar la biblioteca

```bash
# Compilar el programa que usa la biblioteca:
gcc main.c -L. -lvec3 -lm -o program

# -L.     — buscar bibliotecas en el directorio actual
# -lvec3  — enlazar con libvec3.a (el prefijo lib y .a son implícitos)
# -lm     — enlazar con libm (math, porque vec3 usa sqrt)

# Equivalente sin -l (ruta completa):
gcc main.c ./libvec3.a -lm -o program

# El binario resultante contiene el código de vec3 copiado dentro.
# No necesita libvec3.a en runtime.
```

```c
// --- main.c ---
#include "vec3.h"

int main(void) {
    Vec3 a = vec3_create(1, 2, 3);
    Vec3 b = vec3_create(4, 5, 6);

    Vec3 c = vec3_add(a, b);
    vec3_print(c);                   // (5.00, 7.00, 9.00)

    printf("dot: %.2f\n", vec3_dot(a, b));     // 32.00
    printf("len: %.2f\n", vec3_length(a));      // 3.74

    return 0;
}
```

## Organización de proyecto típica

```
proyecto/
├── include/
│   └── vec3.h              ← header público
├── src/
│   └── vec3.c              ← implementación
├── lib/
│   └── libvec3.a           ← biblioteca compilada
├── main.c
└── Makefile
```

```makefile
# Makefile para crear y usar la biblioteca:

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
AR = ar rcs

# Crear la biblioteca:
lib/libvec3.a: src/vec3.o
	$(AR) $@ $^

src/vec3.o: src/vec3.c include/vec3.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compilar el programa:
program: main.c lib/libvec3.a
	$(CC) $(CFLAGS) main.c -Llib -lvec3 -lm -o $@

clean:
	rm -f src/*.o lib/*.a program
```

## El comando ar en detalle

```bash
# Operaciones principales:
ar r  lib.a file.o     # insertar/reemplazar
ar d  lib.a file.o     # eliminar un miembro
ar t  lib.a            # listar miembros
ar x  lib.a            # extraer todos los miembros
ar x  lib.a file.o     # extraer un miembro específico
ar p  lib.a file.o     # imprimir contenido a stdout

# Modificadores:
# c — crear si no existe (sin warning)
# s — crear índice de símbolos
# v — verbose
# u — solo reemplazar si el .o es más nuevo

# Actualizar un miembro:
gcc -c -O2 vec3.c -o vec3.o
ar rcs libmath3d.a vec3.o    # reemplaza solo vec3.o, mantiene los demás

# Crear desde cero con todos los .o de un directorio:
ar rcs libproject.a src/*.o
```

## Bibliotecas estáticas del sistema

```bash
# Las bibliotecas estáticas del sistema están en /usr/lib/:
ls /usr/lib/x86_64-linux-gnu/lib*.a
# libc.a, libm.a, libpthread.a, etc.

# Para enlazar estáticamente con libc:
gcc -static main.c -o program
# El binario incluye TODO, incluyendo libc.
# Resultado: binario mucho más grande pero independiente.

# Mezclar: forzar estático solo para una biblioteca:
gcc main.c -Wl,-Bstatic -lvec3 -Wl,-Bdynamic -lm -o program
# libvec3 se enlaza estáticamente, libm dinámicamente.
```

## Diferencia entre .o y .a

```bash
# Un .o es un archivo objeto individual.
# Un .a es un contenedor de múltiples .o.

# Enlazar con .o directamente:
gcc main.c vec3.o matrix.o -o program
# El linker incluye TODO el código de cada .o.

# Enlazar con .a:
gcc main.c -L. -lmath3d -o program
# El linker incluye SOLO los .o que contienen símbolos necesarios.
# Si main.c solo usa vec3_add, solo vec3.o se incluye,
# matrix.o y transform.o se ignoran → binario más pequeño.
```

---

## Ejercicios

### Ejercicio 1 — Crear una biblioteca estática

```bash
# 1. Crear dos archivos: str_utils.c (to_upper, to_lower, reverse)
#    y math_utils.c (clamp, lerp, map_range).
# 2. Crear los headers correspondientes.
# 3. Compilar a .o y empaquetar en libutils.a.
# 4. Verificar contenido con ar t y nm.
# 5. Crear main.c que use funciones de ambos módulos.
# 6. Compilar y ejecutar.
```

### Ejercicio 2 — Makefile para biblioteca

```makefile
# Crear un Makefile que:
# - Compile cada .c a .o con flags apropiados
# - Cree lib/libmylib.a con todos los .o
# - Compile main.c enlazando con la biblioteca
# - Tenga targets: all, clean, test
# - Use variables para CC, CFLAGS, AR
# Verificar que make rebuild solo recompila lo necesario.
```

### Ejercicio 3 — Verificar enlace selectivo

```bash
# 1. Crear una biblioteca con 3 módulos: a.c, b.c, c.c
#    Cada uno con una función (func_a, func_b, func_c)
#    y un printf en cada función que identifique cuál es.
# 2. Crear main.c que solo use func_a.
# 3. Compilar enlazando con la biblioteca.
# 4. Usar nm sobre el ejecutable para verificar que
#    solo func_a está presente, no func_b ni func_c.
# 5. ¿Qué pasa si func_a llama a func_b? ¿Se incluye func_c?
```
