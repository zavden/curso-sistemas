# T01 — Creación de bibliotecas dinámicas

## Qué es una biblioteca dinámica

Una biblioteca dinámica (shared library) es un archivo `.so`
que se carga en memoria en **runtime**, no se copia dentro del
ejecutable:

```
Compilación:
  vec3.c → gcc -c -fPIC → vec3.o ─┐
  mat3.c → gcc -c -fPIC → mat3.o ─┼→ gcc -shared → libmath3d.so
  transform.c → gcc -c -fPIC → transform.o ─┘

Enlace:
  main.o + libmath3d.so → gcc → programa
  (el programa solo guarda una REFERENCIA a libmath3d.so)

Runtime:
  programa se ejecuta → ld-linux.so carga libmath3d.so → resuelve símbolos
```

```bash
# Convención de nombres:
# lib<nombre>.so              — linker name (symlink)
# lib<nombre>.so.<major>      — soname (symlink)
# lib<nombre>.so.<major>.<minor>.<patch> — real name (archivo)
#
# Ejemplo:
# libmath3d.so      → libmath3d.so.1       (symlink)
# libmath3d.so.1    → libmath3d.so.1.2.3   (symlink)
# libmath3d.so.1.2.3                        (archivo real)
#
# Por ahora, creamos sin versión. El versionado se cubre en T03.
```

## Position-Independent Code (PIC)

Para que una biblioteca se pueda cargar en **cualquier dirección**
de memoria, el código debe ser PIC:

```bash
# -fPIC: genera código independiente de posición
gcc -c -fPIC vec3.c -o vec3.o

# ¿Por qué es necesario?
# - Cada proceso puede cargar libmath3d.so en una dirección diferente
#   (especialmente con ASLR activado).
# - Sin PIC, las instrucciones tendrían direcciones absolutas
#   "hardcoded" que solo funcionan en una dirección específica.
# - Con PIC, las referencias usan offsets relativos al PC (Program Counter)
#   o tablas de indirección (GOT — Global Offset Table).
```

```bash
# -fPIC vs -fpic:
# -fPIC: genera código PIC sin restricciones de tamaño.
# -fpic: genera código PIC más eficiente pero con límite de
#        tamaño de GOT (dependiente de la arquitectura).
# En la práctica: siempre usar -fPIC (mayúsculas).

# Verificar si un .o es PIC:
readelf -d vec3.o | grep TEXTREL
# Si NO aparece TEXTREL → es PIC (correcto).
# Si aparece TEXTREL → tiene text relocations → no es PIC.
```

```c
// Qué cambia PIC en el código generado:

// Sin PIC — dirección absoluta:
// mov eax, [0x402000]    ← dirección fija

// Con PIC — relativo al PC:
// lea rax, [rip + offset]  ← relativo a la instrucción actual
// → funciona sin importar dónde se cargó en memoria.

// Para funciones externas, PIC usa la GOT (Global Offset Table):
// 1. El código accede a la GOT (posición conocida relativa al código)
// 2. La GOT contiene la dirección real de la función
// 3. El linker dinámico llena la GOT al cargar la biblioteca
```

## Crear la biblioteca compartida

### Paso a paso

```bash
# 1. Compilar con -fPIC:
gcc -c -fPIC -Wall -Wextra -O2 vec3.c -o vec3.o
gcc -c -fPIC -Wall -Wextra -O2 mat3.c -o mat3.o

# 2. Crear el .so con -shared:
gcc -shared -o libmath3d.so vec3.o mat3.o

# Con soname (recomendado — ver T03):
gcc -shared -Wl,-soname,libmath3d.so.1 -o libmath3d.so.1.0.0 vec3.o mat3.o

# En un solo paso:
gcc -shared -fPIC -o libmath3d.so vec3.c mat3.c
```

```bash
# Verificar:
file libmath3d.so
# libmath3d.so: ELF 64-bit LSB shared object, x86-64, ...

# Ver símbolos exportados:
nm -D libmath3d.so
# 0000000000001100 T vec3_create
# 0000000000001130 T vec3_add
# ...

# nm -D muestra solo símbolos dinámicos (los visibles para otros).
# nm sin -D muestra todos, incluyendo internos.

# Ver dependencias:
ldd libmath3d.so
# libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
```

### Ejemplo completo

```c
// --- vec3.h ---
#ifndef VEC3_H
#define VEC3_H

typedef struct { double x, y, z; } Vec3;

Vec3   vec3_create(double x, double y, double z);
Vec3   vec3_add(Vec3 a, Vec3 b);
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

```bash
# Compilar:
gcc -c -fPIC -Wall -Wextra -O2 vec3.c -o vec3.o
gcc -shared -o libvec3.so vec3.o

# Compilar el programa:
gcc main.c -L. -lvec3 -lm -o program

# Ejecutar:
LD_LIBRARY_PATH=. ./program
# (necesita LD_LIBRARY_PATH porque libvec3.so no está en un path estándar)
```

## Controlar la visibilidad de símbolos

```c
// Por defecto, TODAS las funciones de un .so son visibles.
// Esto puede ser un problema:
// - Funciones internas quedan expuestas → no se pueden cambiar
// - Más símbolos = carga más lenta
// - Posibles colisiones de nombres con otros .so

// Solución 1: __attribute__((visibility))
__attribute__((visibility("default")))    // visible (exportada)
void public_func(void) { /* ... */ }

__attribute__((visibility("hidden")))     // oculta
void internal_helper(void) { /* ... */ }
```

```bash
# Solución 2: compilar con -fvisibility=hidden y marcar explícitamente:
gcc -c -fPIC -fvisibility=hidden vec3.c -o vec3.o

# Ahora NADA es visible por defecto.
# Solo las funciones marcadas con visibility("default") se exportan.
```

```c
// Patrón con macro de exportación:
#ifdef BUILDING_MYLIB
    #define MYLIB_API __attribute__((visibility("default")))
#else
    #define MYLIB_API
#endif

// En el header:
MYLIB_API Vec3 vec3_create(double x, double y, double z);
MYLIB_API Vec3 vec3_add(Vec3 a, Vec3 b);
// Las funciones sin MYLIB_API quedan ocultas.

// Compilar con:
gcc -c -fPIC -fvisibility=hidden -DBUILDING_MYLIB vec3.c -o vec3.o
```

```bash
# Solución 3: version script para controlar exports:
# --- libvec3.map ---
# {
#     global:
#         vec3_create;
#         vec3_add;
#         vec3_dot;
#         vec3_length;
#         vec3_print;
#     local:
#         *;
# };

gcc -shared -Wl,--version-script=libvec3.map -o libvec3.so vec3.o
# Solo los símbolos en "global" son visibles.
# Todo lo demás es "local" (oculto).
```

## Makefile para biblioteca dinámica

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -Iinclude
LDFLAGS = -shared

SRC = src/vec3.c src/mat3.c
OBJ = $(SRC:.c=.o)
LIB = lib/libmath3d.so

all: $(LIB)

$(LIB): $(OBJ) | lib
	$(CC) $(LDFLAGS) -o $@ $^

lib:
	mkdir -p lib

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Programa de prueba:
test: main.c $(LIB)
	$(CC) -Wall -Iinclude main.c -Llib -lmath3d -lm -o $@
	LD_LIBRARY_PATH=lib ./test

clean:
	rm -f $(OBJ) $(LIB) test

.PHONY: all test clean
```

---

## Ejercicios

### Ejercicio 1 — Crear y usar un .so

```bash
# 1. Crear string_utils.c con funciones: str_reverse, str_to_upper,
#    str_count_char (contar ocurrencias de un carácter).
# 2. Compilar como libstrutils.so.
# 3. Verificar con nm -D que las funciones son visibles.
# 4. Crear main.c que use las funciones.
# 5. Compilar, enlazar y ejecutar.
# 6. ¿Qué pasa si ejecutas sin LD_LIBRARY_PATH?
```

### Ejercicio 2 — Visibilidad de símbolos

```bash
# 1. Crear una biblioteca con 4 funciones:
#    public_a(), public_b() (API pública)
#    helper_x(), helper_y() (internas)
# 2. Compilar SIN control de visibilidad.
#    Verificar con nm -D que las 4 son visibles.
# 3. Recompilar con -fvisibility=hidden y marcar solo
#    public_a y public_b con visibility("default").
# 4. Verificar que helper_x y helper_y ya no aparecen en nm -D.
```

### Ejercicio 3 — Comparar .a vs .so

```bash
# 1. Crear la misma biblioteca como .a y como .so.
# 2. Compilar main.c enlazando con cada una.
# 3. Comparar:
#    - Tamaño del binario (ls -lh)
#    - Dependencias (ldd)
#    - Símbolos incluidos (nm)
#    - Velocidad de ejecución (time, repetir 1000 veces)
# 4. ¿Cuál ejecutable es más grande? ¿Por qué?
```
