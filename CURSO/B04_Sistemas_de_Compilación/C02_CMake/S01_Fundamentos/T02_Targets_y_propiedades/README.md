# T02 — Targets y propiedades

## Que es un target

En CMake, un target es una entidad con nombre que el sistema sabe
construir. A diferencia de Make, donde un target es simplemente un
nombre de archivo con una receta asociada, un target de CMake es un
**objeto con propiedades**: directorios de headers, flags de compilacion,
dependencias de linkeo, definiciones de macros. Estas propiedades se
configuran con comandos dedicados y se propagan automaticamente entre
targets que dependen unos de otros.

```cmake
# Los tres tipos principales de targets:

# 1. Ejecutable:
add_executable(myapp main.c)

# 2. Biblioteca:
add_library(mylib utils.c)

# 3. Custom target (comandos arbitrarios, no produce binario compilado):
add_custom_target(docs COMMAND doxygen Doxyfile)
```

```cmake
# Cada target tiene propiedades que se pueden consultar e inspeccionar:

add_executable(myapp main.c)
target_compile_options(myapp PRIVATE -Wall)
target_include_directories(myapp PRIVATE include/)

# Internamente CMake almacena:
#   myapp.COMPILE_OPTIONS = [-Wall]
#   myapp.INCLUDE_DIRECTORIES = [include/]
#
# Estas propiedades determinan como se compila el target.
# No hay variables globales contaminando otros targets.
```

## add_library()

`add_library()` crea un target de tipo biblioteca. El primer argumento
es el nombre del target, el segundo (opcional) es el tipo, y el
resto son los archivos fuente:

```cmake
# STATIC — archivo estatico (.a en Linux, .lib en Windows).
# El linker copia el codigo al ejecutable final.
add_library(mathlib STATIC math.c vector.c)
# Genera: libmathlib.a

# SHARED — biblioteca compartida (.so en Linux, .dll en Windows).
# Se carga en tiempo de ejecucion, compartida entre procesos.
add_library(mathlib SHARED math.c vector.c)
# Genera: libmathlib.so

# MODULE — similar a SHARED pero pensada para dlopen/LoadLibrary.
# No se enlaza en tiempo de compilacion, solo se carga en runtime.
add_library(plugin MODULE plugin.c)
# No se usa con target_link_libraries.

# OBJECT — compila los .c a .o pero no empaqueta en .a ni .so.
# Los objetos se reusan en otros targets.
add_library(common OBJECT log.c config.c)
add_executable(app main.c $<TARGET_OBJECTS:common>)
add_executable(tests test_main.c $<TARGET_OBJECTS:common>)
# Ambos ejecutables incluyen los .o de common sin compilar dos veces.
```

```cmake
# Si no se especifica el tipo, CMake decide segun BUILD_SHARED_LIBS:
add_library(mylib utils.c)

# Equivale a STATIC por defecto.
# Si el usuario configura -DBUILD_SHARED_LIBS=ON, se construye como SHARED.
# Esto permite al consumidor elegir sin modificar el CMakeLists.txt.
```

```bash
# Construir como estatica (defecto):
cmake -B build
cmake --build build
# Genera: libmylib.a

# Construir como compartida:
cmake -B build -DBUILD_SHARED_LIBS=ON
cmake --build build
# Genera: libmylib.so
```

## target_include_directories()

Agrega directorios de busqueda de headers a un target. Es la forma
moderna de indicar donde buscar los `#include`:

```cmake
# Sintaxis:
# target_include_directories(<target> <PUBLIC|PRIVATE|INTERFACE> <dirs>...)

add_library(mylib src/mylib.c)
target_include_directories(mylib
    PUBLIC include/          # headers que expone mylib a quien la use
    PRIVATE src/             # headers internos de implementacion
)
```

```cmake
# Forma legacy (NO recomendada):
include_directories(include/)
# Afecta a TODOS los targets del directorio y subdirectorios.
# Es una variable global — contamina targets que no la necesitan.

# Forma moderna (recomendada):
target_include_directories(mylib PUBLIC include/)
# Solo afecta a mylib (y a quien dependa de mylib, si es PUBLIC).
# Cada target controla sus propios includes.
```

```cmake
# Ejemplo practico — estructura tipica:
#
#   mylib/
#   ├── CMakeLists.txt
#   ├── include/
#   │   └── mylib/
#   │       └── mylib.h       ← header publico
#   └── src/
#       ├── internal.h        ← header interno
#       └── mylib.c
#
add_library(mylib src/mylib.c)
target_include_directories(mylib
    PUBLIC include/           # consumidores hacen: #include <mylib/mylib.h>
    PRIVATE src/              # mylib.c puede hacer: #include "internal.h"
)

# Un ejecutable que depende de mylib:
add_executable(app main.c)
target_link_libraries(app PRIVATE mylib)
# app hereda include/ (PUBLIC) pero NO src/ (PRIVATE).
# En main.c se puede hacer: #include <mylib/mylib.h>
```

## PUBLIC, PRIVATE, INTERFACE

Este es el concepto central de CMake moderno. Cada propiedad de un
target se clasifica segun su **visibilidad**:

```cmake
# PRIVATE — solo para compilar el propio target.
# No se propaga a quien dependa de este target.

# PUBLIC — para el target Y para quien lo use.
# Se propaga transitivamente.

# INTERFACE — solo para quien use el target.
# No se aplica al target mismo.
```

```cmake
# Ejemplo concreto: una biblioteca de red que usa pthread internamente
# y expone un directorio de headers publico.

add_library(netlib src/socket.c src/http.c)

target_include_directories(netlib
    PUBLIC include/                # quienes usen netlib necesitan estos headers
    PRIVATE src/                   # headers internos, solo para compilar netlib
)

target_link_libraries(netlib
    PRIVATE pthread                # netlib usa pthread, pero quien use netlib no
)

target_compile_definitions(netlib
    PUBLIC NETLIB_VERSION=2        # quienes usen netlib ven esta macro tambien
    PRIVATE _GNU_SOURCE            # solo para compilar netlib
)

# Resultado:
# - Para compilar netlib: incluye include/ y src/, define NETLIB_VERSION=2 y
#   _GNU_SOURCE, enlaza contra pthread.
# - Para compilar app que depende de netlib: incluye include/ (no src/),
#   define NETLIB_VERSION=2 (no _GNU_SOURCE), NO enlaza pthread directamente.
```

```cmake
# INTERFACE — se usa cuando el target no consume la propiedad pero
# quienes lo usen si la necesitan.

add_library(api src/api.c)
target_compile_definitions(api
    INTERFACE API_STATIC           # quien use api debe definir API_STATIC
)
# api.c no se compila con -DAPI_STATIC, pero main.c (que usa api) si.

# Caso tipico de INTERFACE: una biblioteca que requiere un estandar
# especifico de C para su API publica:
target_compile_features(api INTERFACE c_std_11)
# Quien use api se compilara con -std=c11 (o superior).
```

```cmake
# Resumen visual:
#
#   Visibilidad        Compila el target    Propaga a dependientes
#   ─────────────────  ─────────────────    ──────────────────────
#   PRIVATE            SI                   NO
#   PUBLIC             SI                   SI
#   INTERFACE          NO                   SI
```

## target_link_libraries()

Enlaza un target contra bibliotecas. Ademas de agregar el flag `-l`
al linker, **propaga todas las propiedades PUBLIC e INTERFACE** del
target enlazado:

```cmake
# Sintaxis:
# target_link_libraries(<target> <PUBLIC|PRIVATE|INTERFACE> <libs>...)

add_library(mathlib STATIC math.c)
target_include_directories(mathlib PUBLIC include/)

add_executable(app main.c)
target_link_libraries(app PRIVATE mathlib)

# Esto hace DOS cosas:
# 1. Enlaza app contra libmathlib.a (el linker recibe -lmathlib).
# 2. Propaga las propiedades PUBLIC de mathlib a app:
#    - app hereda include/ en sus directorios de busqueda de headers.
#    - Si mathlib tuviera PUBLIC compile_definitions, app las heredaria.
#    - Si mathlib tuviera PUBLIC compile_options, app las heredaria.
```

```cmake
# El modificador en target_link_libraries determina si la dependencia
# se propaga mas alla:

add_library(low_level STATIC low.c)
target_include_directories(low_level PUBLIC include/low/)

add_library(mid_level STATIC mid.c)
target_link_libraries(mid_level PUBLIC low_level)
# mid_level depende de low_level, y esta dependencia es PUBLIC.

add_executable(app main.c)
target_link_libraries(app PRIVATE mid_level)

# Cadena de propagacion:
# app → mid_level (PRIVATE) → low_level (PUBLIC)
#
# app hereda las propiedades PUBLIC de mid_level.
# Como mid_level declaro low_level como PUBLIC, las propiedades
# PUBLIC de low_level tambien llegan a app.
# app puede hacer: #include <low/low.h>
```

```cmake
# Si mid_level usara PRIVATE en vez de PUBLIC:
target_link_libraries(mid_level PRIVATE low_level)

# Ahora app NO hereda nada de low_level.
# app no puede hacer #include <low/low.h>.
# Solo mid_level usa low_level internamente.
```

## target_compile_options()

Agrega flags de compilacion a un target:

```cmake
# Flags que solo afectan la compilacion de este target:
add_library(mylib src/mylib.c)
target_compile_options(mylib PRIVATE
    -Wall -Wextra -Werror -pedantic
)
# Solo mylib.c se compila con estos warnings.
# Quien use mylib no recibe estos flags.
```

```cmake
# Flags que se propagan a dependientes:
add_library(strict_base src/base.c)
target_compile_options(strict_base PUBLIC
    -fno-exceptions
)

add_executable(app main.c)
target_link_libraries(app PRIVATE strict_base)
# app se compila con -fno-exceptions (heredado de strict_base).
```

```cmake
# Forma legacy (NO recomendada):
add_compile_options(-Wall -Wextra)
# Afecta TODOS los targets del directorio y subdirectorios.

# Forma moderna (recomendada):
target_compile_options(mylib PRIVATE -Wall -Wextra)
# Solo afecta a mylib.
```

```cmake
# Flags condicionales por generador:
target_compile_options(mylib PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wformat=2>
    $<$<C_COMPILER_ID:Clang>:-Weverything>
)
# -Wformat=2 solo con GCC, -Weverything solo con Clang.
# Esto usa generator expressions (se cubren en otro topico).
```

## target_compile_definitions()

Define macros de preprocesador por target. Equivale a `-D` en la
linea de comandos del compilador:

```cmake
# Definir macros para un target:
add_library(mylib src/mylib.c)
target_compile_definitions(mylib
    PRIVATE
        _GNU_SOURCE              # solo para compilar mylib
        MAX_BUFFER=4096          # equivale a -DMAX_BUFFER=4096
    PUBLIC
        MYLIB_VERSION=3          # mylib y sus dependientes ven esta macro
)
```

```cmake
# Uso tipico: modo debug vs release:
add_executable(app main.c)
target_compile_definitions(app PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG>
)
# En modo Debug: -DDEBUG=1
# En modo Release: -DNDEBUG
```

```cmake
# Forma legacy (NO recomendada):
add_definitions(-DDEBUG=1)
# Afecta a todos los targets del directorio.

# Forma moderna (recomendada):
target_compile_definitions(app PRIVATE DEBUG=1)
# Solo afecta a app.
```

## Propagacion de propiedades

CMake propaga automaticamente las propiedades PUBLIC e INTERFACE
a traves de la cadena de dependencias. Esto es lo que hace al
sistema poderoso: no hay que gestionar flags manualmente cuando
las dependencias crecen.

```cmake
# Ejemplo con 3 targets: app → network → crypto
#
# Diagrama de dependencias:
#
#   crypto              network              app
#   ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
#   │ PUBLIC:      │    │ PUBLIC:      │    │              │
#   │  -Icrypto/   │───▶│  -Inet/      │───▶│ main.c       │
#   │ PRIVATE:     │    │ PRIVATE:     │    │              │
#   │  -DHAVE_SSL  │    │  -D_POSIX    │    │              │
#   │  -lssl       │    │  -lpthread   │    │              │
#   └──────────────┘    └──────────────┘    └──────────────┘
#
#   Flechas = target_link_libraries con PUBLIC

add_library(crypto src/crypto.c)
target_include_directories(crypto PUBLIC include/crypto/)
target_compile_definitions(crypto PRIVATE HAVE_SSL)
target_link_libraries(crypto PRIVATE ssl)

add_library(network src/net.c)
target_include_directories(network PUBLIC include/net/)
target_compile_definitions(network PRIVATE _POSIX)
target_link_libraries(network
    PUBLIC crypto             # la dependencia se propaga
    PRIVATE pthread           # la dependencia NO se propaga
)

add_executable(app main.c)
target_link_libraries(app PRIVATE network)
```

```cmake
# Que hereda cada target al compilar:
#
# crypto:
#   includes:     include/crypto/
#   definitions:  HAVE_SSL
#   link:         ssl
#
# network:
#   includes:     include/net/ + include/crypto/ (heredado de crypto PUBLIC)
#   definitions:  _POSIX
#   link:         crypto + pthread
#
# app:
#   includes:     include/net/ + include/crypto/ (propagados via PUBLIC)
#   definitions:  (ninguna — las de network y crypto son PRIVATE)
#   link:         network + crypto (propagado via PUBLIC)
#                 NO pthread (es PRIVATE en network)
#                 NO ssl (es PRIVATE en crypto)
```

```cmake
# Verificar que propiedades recibe un target:
# Despues de cmake -B build, inspeccionar con:
#
# cmake --build build --target app -- VERBOSE=1
#
# O con Ninja:
# cmake -B build -G Ninja
# cmake --build build -- -v
#
# Esto muestra los comandos exactos de compilacion con todos los
# flags, includes y definiciones que CMake calculo para cada target.
```

## Bibliotecas INTERFACE

Una biblioteca INTERFACE no tiene archivos fuente propios. Solo
existe para transportar propiedades a quien la use. El caso
principal son las bibliotecas **header-only**:

```c
// include/vec3/vec3.h — biblioteca header-only
#ifndef VEC3_H
#define VEC3_H

typedef struct {
    float x, y, z;
} Vec3;

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

#endif // VEC3_H
```

```cmake
# No hay .c que compilar. Solo un header.
# Se crea como INTERFACE:
add_library(vec3 INTERFACE)
target_include_directories(vec3 INTERFACE include/vec3/)

# Una biblioteca INTERFACE solo puede tener propiedades INTERFACE.
# No tiene PRIVATE ni PUBLIC — no hay nada propio que compilar.

add_executable(game main.c)
target_link_libraries(game PRIVATE vec3)
# game recibe -Iinclude/vec3/ en su compilacion.
# No se enlaza ningun .a ni .so — vec3 no genera binario.
```

```cmake
# Otro uso de INTERFACE: agrupar configuracion comun.
# Un target que solo transporta flags:

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Werror
)

add_library(project_defaults INTERFACE)
target_compile_features(project_defaults INTERFACE c_std_11)
target_compile_definitions(project_defaults INTERFACE
    $<$<CONFIG:Debug>:DEBUG=1>
)

# Ahora cada target del proyecto puede heredar estas configuraciones:
add_library(mylib src/mylib.c)
target_link_libraries(mylib PRIVATE project_warnings project_defaults)

add_executable(app main.c)
target_link_libraries(app PRIVATE project_warnings project_defaults)

# Todos los targets comparten los mismos warnings y estandar
# sin repetir las lineas en cada uno.
```

## Propiedades vs variables globales

CMake soporta dos estilos: el moderno (basado en targets) y el
legacy (basado en variables globales). El estilo moderno es
superior en todos los aspectos:

```cmake
# ── Estilo legacy (pre-CMake 2.8) ──
# Variables globales que afectan a todo:

include_directories(include/)          # todos los targets ven include/
link_libraries(pthread m)              # todos los targets enlazan pthread y m
add_compile_options(-Wall -Wextra)     # todos los targets reciben estos flags
add_definitions(-DUSE_FEATURE_X)       # todos los targets definen esta macro

add_library(mylib src/mylib.c)
add_executable(app main.c)
add_executable(tests test_main.c)

# Problema: tests no necesita -DUSE_FEATURE_X, pero lo recibe.
# Problema: app no necesita pthread, pero enlaza contra ella.
# Problema: si agregas un subdirectorio con add_subdirectory(),
#           hereda TODAS las variables globales del padre.
```

```cmake
# ── Estilo moderno (CMake 3.x) ──
# Propiedades por target, encapsuladas:

add_library(mylib src/mylib.c)
target_include_directories(mylib PUBLIC include/)
target_link_libraries(mylib PRIVATE pthread)
target_compile_options(mylib PRIVATE -Wall -Wextra)
target_compile_definitions(mylib PRIVATE USE_FEATURE_X)

add_executable(app main.c)
target_link_libraries(app PRIVATE mylib)
# app hereda include/ (PUBLIC) pero nada mas.

add_executable(tests test_main.c)
target_link_libraries(tests PRIVATE mylib)
# tests hereda include/ (PUBLIC) pero nada mas.
# No recibe -DUSE_FEATURE_X ni -lpthread.
```

```cmake
# Tabla comparativa:
#
#   Aspecto            Legacy                  Moderno
#   ────────────────── ─────────────────────── ──────────────────────────────
#   Scope              Global (directorio)     Por target
#   Propagacion        Ninguna                 Automatica (PUBLIC/INTERFACE)
#   Encapsulacion      No hay                  PRIVATE oculta detalles
#   Composabilidad     Dificil                 target_link_libraries encadena
#   Conflictos         Frecuentes              Aislados por target
#   Recomendacion      Evitar                  Usar siempre
#
# Regla simple: si el comando empieza con target_, es moderno.
# Si no tiene target_ en el nombre, probablemente es legacy.
```

## Ejemplo completo

Un proyecto con una biblioteca estatica, un ejecutable y un test,
mostrando la propagacion de propiedades:

```
# Estructura del proyecto:
#
#   project/
#   ├── CMakeLists.txt
#   ├── include/
#   │   └── calc/
#   │       └── calc.h
#   ├── src/
#   │   ├── calc.c
#   │   └── main.c
#   └── tests/
#       └── test_calc.c
```

```c
// include/calc/calc.h
#ifndef CALC_H
#define CALC_H

int calc_add(int a, int b);
int calc_multiply(int a, int b);
int calc_power(int base, int exp);

#endif // CALC_H
```

```c
// src/calc.c
#include "calc/calc.h"

int calc_add(int a, int b) {
    return a + b;
}

int calc_multiply(int a, int b) {
    return a * b;
}

int calc_power(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}
```

```c
// src/main.c
#include <stdio.h>
#include "calc/calc.h"

int main(void) {
    printf("3 + 4 = %d\n", calc_add(3, 4));
    printf("3 * 4 = %d\n", calc_multiply(3, 4));
    printf("2 ^ 10 = %d\n", calc_power(2, 10));
    return 0;
}
```

```c
// tests/test_calc.c
#include <stdio.h>
#include <stdlib.h>
#include "calc/calc.h"

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "FAIL: %s (got %d, expected %d)\n", msg, a, b); \
            exit(1); \
        } \
        printf("PASS: %s\n", msg); \
    } while (0)

int main(void) {
    ASSERT_EQ(calc_add(2, 3), 5, "add basic");
    ASSERT_EQ(calc_add(-1, 1), 0, "add negative");
    ASSERT_EQ(calc_multiply(3, 4), 12, "multiply basic");
    ASSERT_EQ(calc_multiply(0, 100), 0, "multiply by zero");
    ASSERT_EQ(calc_power(2, 0), 1, "power zero");
    ASSERT_EQ(calc_power(2, 10), 1024, "power of two");

    printf("All tests passed.\n");
    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(calc_project C)

# ── Biblioteca estatica ──
add_library(calc STATIC src/calc.c)
target_include_directories(calc
    PUBLIC include/           # quien use calc hereda include/
)
target_compile_options(calc
    PRIVATE -Wall -Wextra     # solo para compilar calc
)

# ── Ejecutable principal ──
add_executable(calculator src/main.c)
target_link_libraries(calculator PRIVATE calc)
# calculator hereda:
#   - include/ (PUBLIC de calc)
# calculator NO hereda:
#   - -Wall -Wextra (PRIVATE de calc)

# ── Tests ──
add_executable(test_calc tests/test_calc.c)
target_link_libraries(test_calc PRIVATE calc)
target_compile_definitions(test_calc PRIVATE TESTING=1)
# test_calc hereda:
#   - include/ (PUBLIC de calc)
# test_calc tiene ademas:
#   - -DTESTING=1 (su propio PRIVATE)
```

```bash
# Construir y ejecutar:
cmake -B build
cmake --build build

./build/calculator
# 3 + 4 = 7
# 3 * 4 = 12
# 2 ^ 10 = 1024

./build/test_calc
# PASS: add basic
# PASS: add negative
# PASS: multiply basic
# PASS: multiply by zero
# PASS: power zero
# PASS: power of two
# All tests passed.
```

```bash
# Verificar los flags de compilacion con modo verbose:
cmake --build build -- VERBOSE=1

# Salida para calc.c (la biblioteca):
# gcc -Wall -Wextra -Iinclude/ -c src/calc.c -o build/libcalc.a ...
#     ^^^^^^^^^^^^^ ^^^^^^^^^^
#     PRIVATE opts   PUBLIC inc

# Salida para main.c (el ejecutable):
# gcc -Iinclude/ -c src/main.c -o build/calculator ...
#     ^^^^^^^^^^
#     heredado de calc (PUBLIC)
#     NO tiene -Wall -Wextra (PRIVATE de calc, no se propaga)

# Salida para test_calc.c:
# gcc -DTESTING=1 -Iinclude/ -c tests/test_calc.c ...
#     ^^^^^^^^^^^  ^^^^^^^^^^
#     PRIVATE def  heredado de calc (PUBLIC)
```

---

## Ejercicios

### Ejercicio 1 — Biblioteca con visibilidad controlada

```cmake
# Crear un proyecto con:
#   include/strutils/strutils.h  — declaraciones de str_reverse y str_upper
#   src/strutils.c               — implementacion (usa <ctype.h> y <string.h>)
#   src/main.c                   — main que usa str_reverse y str_upper
#
# Escribir un CMakeLists.txt que:
#   1. Cree una biblioteca STATIC llamada strutils
#   2. Asigne include/ como PUBLIC include directory
#   3. Asigne src/ como PRIVATE include directory (si strutils tiene headers internos)
#   4. Asigne -Wall -Wextra -Werror como PRIVATE compile options de strutils
#   5. Cree un ejecutable app que enlace contra strutils
#
# Verificar:
#   - cmake --build build -- VERBOSE=1 muestra que strutils.c se compila
#     con -Wall -Wextra -Werror, pero main.c NO recibe esos flags.
#   - main.c puede hacer #include <strutils/strutils.h> (heredado via PUBLIC).
```

### Ejercicio 2 — Cadena de propagacion con tres targets

```cmake
# Crear un proyecto con tres bibliotecas encadenadas:
#
#   logger (STATIC):
#     - src/logger.c, include/logger/logger.h
#     - PUBLIC: include/logger/
#     - PRIVATE: -D_GNU_SOURCE
#
#   database (STATIC):
#     - src/database.c, include/database/database.h
#     - PUBLIC: include/database/
#     - Depende de logger con PUBLIC (la dependencia se propaga)
#     - PRIVATE: -lpthread
#
#   app (ejecutable):
#     - src/main.c
#     - Depende de database con PRIVATE
#
# Predecir ANTES de compilar:
#   1. Que include directories recibe app al compilar main.c?
#   2. Se enlaza app contra pthread?
#   3. Se define _GNU_SOURCE al compilar main.c?
#
# Verificar con cmake --build build -- VERBOSE=1.
# Corregir las predicciones si difieren de la realidad.
```

### Ejercicio 3 — Biblioteca INTERFACE header-only

```cmake
# Crear una biblioteca header-only de macros de assertion para tests:
#
#   include/minitest/minitest.h:
#     - Macro ASSERT_TRUE(expr) que imprime PASS/FAIL
#     - Macro ASSERT_EQ(a, b) que compara dos valores
#     - Macro RUN_TEST(func) que ejecuta una funcion de test
#
# En CMakeLists.txt:
#   1. Crear un target INTERFACE llamado minitest
#   2. Asignarle include/minitest/ como INTERFACE include directory
#   3. Crear dos ejecutables de test (test_math.c, test_string.c) que
#      enlacen contra minitest
#   4. Verificar que NO se genera ningun .a ni .so para minitest
#   5. Verificar que ambos test reciben el include directory correcto
#
# Bonus: crear un target INTERFACE llamado strict_warnings que
# transporte -Wall -Wextra -Wpedantic. Hacer que ambos tests
# enlacen tambien contra strict_warnings.
```
