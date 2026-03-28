# T04 — Subdirectorios y estructura de proyecto

## add_subdirectory()

`add_subdirectory()` incluye un subdirectorio que contiene su propio
`CMakeLists.txt`. CMake procesa ese archivo como parte del mismo
proyecto: los targets definidos en el subdirectorio son visibles
desde cualquier otro punto del arbol de compilacion.

```cmake
# Sintaxis:
# add_subdirectory(<directorio> [<directorio_binario>] [EXCLUDE_FROM_ALL])

# CMakeLists.txt raiz:
cmake_minimum_required(VERSION 3.20)
project(myapp C)

add_subdirectory(src)
add_subdirectory(lib)
add_subdirectory(tests)

# CMake entra en cada subdirectorio, procesa su CMakeLists.txt
# y registra todos los targets definidos ahi. Al terminar,
# el grafo global de dependencias contiene los targets de
# todos los subdirectorios.
```

```cmake
# lib/CMakeLists.txt:
add_library(mathlib math.c)
target_include_directories(mathlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# src/CMakeLists.txt:
add_executable(app main.c)
target_link_libraries(app PRIVATE mathlib)
# mathlib se definio en lib/, pero es visible aqui.
# CMake resuelve la referencia porque los targets son globales.
```

```cmake
# El orden de add_subdirectory() NO importa para la
# resolucion de targets. CMake primero procesa todos los
# subdirectorios y luego resuelve las dependencias.
# Esto es valido:

add_subdirectory(src)    # src/ usa mathlib
add_subdirectory(lib)    # lib/ define mathlib

# CMake no falla aunque src/ se procese antes que lib/.
# Las dependencias se resuelven al generar el build system,
# no al procesar los CMakeLists.txt.
```

## Estructura idiomatica de un proyecto C

La comunidad de CMake ha convergido en una estructura de
directorios estandar que separa headers publicos, implementacion,
tests y configuracion:

```
proyecto/
  CMakeLists.txt              (raiz: project(), opciones, add_subdirectory)
  include/proyecto/            (headers publicos — la API publica)
    proyecto.h
    types.h
  src/                         (implementacion — .c y headers internos)
    core.c
    parser.c
    internal.h
  tests/                       (tests unitarios y de integracion)
    CMakeLists.txt
    test_core.c
    test_parser.c
  examples/                    (ejemplos opcionales de uso)
    CMakeLists.txt
    demo.c
  cmake/                       (modulos CMake propios: Find*.cmake, etc.)
    FindMyDep.cmake
```

```cmake
# Por que include/proyecto/ y no simplemente include/?
#
# El subdirectorio con el nombre del proyecto evita colisiones
# de headers entre bibliotecas. Si dos proyectos exponen un
# "types.h", sin namespace de directorio habria conflicto.
#
# Con la convencion include/proyecto/:
#   #include <proyecto/types.h>     ← sin ambiguedad
#
# Sin la convencion:
#   #include <types.h>              ← cual types.h?

# En CMake, el include directory se apunta a include/, no a
# include/proyecto/:
target_include_directories(proyecto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# El consumidor hace: #include <proyecto/proyecto.h>
```

## CMakeLists.txt raiz vs subdirectorios

El CMakeLists.txt raiz tiene responsabilidades distintas a los
de subdirectorios. Mezclarlas produce proyectos dificiles de
mantener:

```cmake
# ── CMakeLists.txt raiz ──
# Responsabilidades:
#   - cmake_minimum_required()
#   - project()
#   - Opciones globales (option())
#   - Configuracion del compilador (estandar C, warnings)
#   - add_subdirectory() para cada componente
#   - NO define targets de compilacion directamente

cmake_minimum_required(VERSION 3.20)
project(netutils VERSION 1.0 LANGUAGES C)

# Opciones globales:
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

# Estandar C para todo el proyecto:
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Subdirectorios con targets:
add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

```cmake
# ── src/CMakeLists.txt ──
# Responsabilidades:
#   - Definir los targets de este directorio
#   - Configurar propiedades del target (includes, flags, deps)
#   - NO llamar a project() (salvo subproyectos independientes)

add_library(netutils
    socket.c
    http.c
    dns.c
)

target_include_directories(netutils
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(netutils PRIVATE -Wall -Wextra)
```

```cmake
# ── tests/CMakeLists.txt ──
add_executable(test_socket test_socket.c)
target_link_libraries(test_socket PRIVATE netutils)

add_executable(test_http test_http.c)
target_link_libraries(test_http PRIVATE netutils)
```

## Scope de variables

`add_subdirectory()` crea un **scope hijo** para las variables
de CMake. Las variables del padre son visibles en el hijo, pero
las del hijo no propagan al padre. Los targets, en cambio, son
siempre globales:

```cmake
# CMakeLists.txt raiz:
set(ROOT_VAR "visible en hijos")
add_subdirectory(lib)
# Aqui CHILD_VAR no existe — no se propago al padre.
message(STATUS "CHILD_VAR = '${CHILD_VAR}'")
# Imprime: CHILD_VAR = ''
```

```cmake
# lib/CMakeLists.txt (scope hijo):
message(STATUS "ROOT_VAR = '${ROOT_VAR}'")
# Imprime: ROOT_VAR = 'visible en hijos'

set(CHILD_VAR "solo existe aqui")
# CHILD_VAR solo existe en este scope y en los sub-scopes
# que se creen desde aqui (sub-subdirectorios).
```

```cmake
# Para propagar una variable del hijo al padre, usar PARENT_SCOPE:
# lib/CMakeLists.txt:
set(LIB_VERSION "2.1" PARENT_SCOPE)
# Ahora LIB_VERSION existe en el scope del padre.
# ATENCION: con PARENT_SCOPE, la variable NO se establece
# en el scope actual, solo en el del padre.

# Si se necesita en ambos scopes:
set(LIB_VERSION "2.1")                  # scope local
set(LIB_VERSION "2.1" PARENT_SCOPE)     # scope padre
```

```cmake
# Resumen de visibilidad:
#
#   Entidad          Scope en add_subdirectory()
#   ───────────────  ──────────────────────────────────────────
#   Variables        Hijo ve las del padre. Padre NO ve las del
#                    hijo (salvo PARENT_SCOPE).
#   Targets          Globales. Un target definido en cualquier
#                    subdirectorio es visible en todo el proyecto.
#   Propiedades de   Asociadas al target — globales como el target.
#   target
```

```cmake
# Ejemplo practico: set() en un subdirectorio NO contamina al padre.
#
# CMakeLists.txt raiz:
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
add_subdirectory(vendor)
# vendor/ puede hacer set(CMAKE_C_FLAGS ...) sin afectar
# al resto del proyecto. El scope lo aisla.
```

## CMAKE_CURRENT_SOURCE_DIR vs CMAKE_SOURCE_DIR

CMake define varias variables de directorio. Es critico
entender cual usar en cada contexto, especialmente cuando
un proyecto puede ser consumido como subdirectorio de otro:

```cmake
# CMAKE_SOURCE_DIR
#   Siempre apunta al directorio del CMakeLists.txt de nivel
#   mas alto (la raiz absoluta). NUNCA cambia.
#
# CMAKE_CURRENT_SOURCE_DIR
#   Apunta al directorio del CMakeLists.txt que se esta
#   procesando actualmente. Cambia con cada add_subdirectory().
#
# PROJECT_SOURCE_DIR
#   Apunta al directorio del project() mas cercano en la
#   cadena de padres. Cambia si un subdirectorio llama a project().

# Ejemplo con esta estructura:
#   /home/user/myapp/                     ← cmake raiz
#     CMakeLists.txt                      ← project(myapp)
#     lib/
#       CMakeLists.txt                    ← add_library(...)
#     external/
#       otherlib/
#         CMakeLists.txt                  ← project(otherlib)
```

```cmake
# Cuando CMake procesa lib/CMakeLists.txt:
#   CMAKE_SOURCE_DIR         = /home/user/myapp
#   CMAKE_CURRENT_SOURCE_DIR = /home/user/myapp/lib
#   PROJECT_SOURCE_DIR       = /home/user/myapp     (project myapp)

# Cuando CMake procesa external/otherlib/CMakeLists.txt:
#   CMAKE_SOURCE_DIR         = /home/user/myapp
#   CMAKE_CURRENT_SOURCE_DIR = /home/user/myapp/external/otherlib
#   PROJECT_SOURCE_DIR       = /home/user/myapp/external/otherlib
#                               (otherlib tiene su propio project())
```

```cmake
# Regla practica:
#
#   - Usar CMAKE_CURRENT_SOURCE_DIR para referirse a archivos
#     del directorio actual. Es la opcion correcta el 90% del tiempo.
#
#   - Usar PROJECT_SOURCE_DIR si se necesita la raiz del proyecto
#     actual (respeta subproyectos).
#
#   - EVITAR CMAKE_SOURCE_DIR en bibliotecas. Si alguien incluye
#     tu biblioteca como add_subdirectory(), CMAKE_SOURCE_DIR
#     apuntara a SU raiz, no a la tuya.

# INCORRECTO en una biblioteca:
target_include_directories(mylib PUBLIC ${CMAKE_SOURCE_DIR}/include)
# Si alguien usa add_subdirectory(path/to/mylib), CMAKE_SOURCE_DIR
# apunta a su proyecto, no al tuyo. Los headers no se encuentran.

# CORRECTO:
target_include_directories(mylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# CMAKE_CURRENT_SOURCE_DIR siempre apunta al directorio de este
# CMakeLists.txt, independientemente de quien lo incluya.
```

## Visibilidad de targets

En CMake, todos los targets son globales. No existe el concepto
de "target privado" o "target local a un subdirectorio". Un target
definido en cualquier subdirectorio puede ser referenciado desde
cualquier otro:

```cmake
# lib/core/CMakeLists.txt:
add_library(core core.c)

# lib/net/CMakeLists.txt:
add_library(net net.c)
target_link_libraries(net PUBLIC core)

# app/CMakeLists.txt:
add_executable(app main.c)
target_link_libraries(app PRIVATE net)
# app usa net, que esta en otro subdirectorio.
# net usa core, que esta en un tercer subdirectorio.
# No importa la estructura de directorios: los targets son globales.
```

```cmake
# Consecuencia: los nombres de targets deben ser unicos en
# todo el proyecto. Dos subdirectorios NO pueden definir un
# target con el mismo nombre.

# lib/CMakeLists.txt:
add_library(utils utils.c)

# vendor/CMakeLists.txt:
add_library(utils vendor_utils.c)     # ERROR: target "utils" ya existe.

# Solucion: usar nombres con prefijo o namespace:
add_library(myproject_utils utils.c)
add_library(vendor_utils vendor_utils.c)
```

## ALIAS targets

Un ALIAS target es un nombre alternativo de solo lectura para
un target existente. La convencion es usar el formato
`namespace::nombre`, que replica el nombre que `find_package()`
generaria para una dependencia externa:

```cmake
# Crear una biblioteca y su alias:
add_library(mycore src/core.c)
add_library(myproject::core ALIAS mycore)

# Ahora se puede usar el alias en target_link_libraries:
add_executable(app main.c)
target_link_libraries(app PRIVATE myproject::core)
```

```cmake
# Por que usar aliases?
#
# 1. Permiten que el codigo de CMake sea identico independientemente
#    de si la dependencia viene de add_subdirectory() o find_package().
#
# Si el proyecto se consume como subdirectorio:
#   add_subdirectory(myproject)
#   target_link_libraries(app PRIVATE myproject::core)
#
# Si el proyecto se consume como paquete instalado:
#   find_package(myproject REQUIRED)
#   target_link_libraries(app PRIVATE myproject::core)
#
# El nombre myproject::core funciona en ambos casos.
# Sin alias, habria que cambiar el nombre segun el metodo de consumo.
```

```cmake
# 2. Los aliases producen errores claros si el target no existe.
#
# Sin alias:
target_link_libraries(app PRIVATE mycore_typo)
# CMake asume que "mycore_typo" es un flag de linker (-lmycore_typo).
# El error aparece en tiempo de linkeo, confuso.
#
# Con alias:
target_link_libraries(app PRIVATE myproject::core_typo)
# CMake detecta inmediatamente que el target no existe:
#   "Target 'myproject::core_typo' not found"
# El "::" le indica a CMake que debe ser un target, no un flag.
```

```cmake
# 3. Convenciones de nombres:
#
# Patron:  add_library(<namespace>::<componente> ALIAS <target_real>)
#
# Ejemplos reales:
add_library(json_parser src/parser.c)
add_library(myproject::json_parser ALIAS json_parser)

add_library(http_client src/http.c)
add_library(myproject::http_client ALIAS http_client)

# Limitaciones de aliases:
#   - No se pueden instalar (install no acepta aliases).
#   - No se les puede asignar propiedades.
#   - No se puede crear un alias de un alias.
#   - Solo sirven como referencia de lectura.
```

## Proyecto con multiples bibliotecas

Un proyecto real suele tener multiples bibliotecas internas que
dependen unas de otras. La estructura de subdirectorios y
CMakeLists.txt refleja esta organizacion:

```
calculator/
  CMakeLists.txt               (raiz)
  lib/
    core/
      CMakeLists.txt
      include/core/
        core.h
      src/
        core.c
    io/
      CMakeLists.txt
      include/io/
        io.h
      src/
        io.c
  app/
    CMakeLists.txt
    main.c
  tests/
    CMakeLists.txt
    test_core.c
    test_io.c
```

```cmake
# CMakeLists.txt (raiz):
cmake_minimum_required(VERSION 3.20)
project(calculator VERSION 1.0 LANGUAGES C)

option(BUILD_TESTS "Build test suite" ON)

add_subdirectory(lib/core)
add_subdirectory(lib/io)
add_subdirectory(app)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

```cmake
# lib/core/CMakeLists.txt:
add_library(core src/core.c)
add_library(calculator::core ALIAS core)

target_include_directories(core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_options(core PRIVATE -Wall -Wextra)
```

```cmake
# lib/io/CMakeLists.txt:
add_library(io src/io.c)
add_library(calculator::io ALIAS io)

target_include_directories(io
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(io PUBLIC core)
# io depende de core. Quien use io tambien hereda core
# (y sus include directories) porque la dependencia es PUBLIC.

target_compile_options(io PRIVATE -Wall -Wextra)
```

```cmake
# app/CMakeLists.txt:
add_executable(calc_app main.c)
target_link_libraries(calc_app
    PRIVATE calculator::io    # trae io + core transitivamente
)
```

```cmake
# tests/CMakeLists.txt:
add_executable(test_core test_core.c)
target_link_libraries(test_core PRIVATE calculator::core)

add_executable(test_io test_io.c)
target_link_libraries(test_io PRIVATE calculator::io)
# test_io hereda core transitivamente via io (PUBLIC).
```

## option() para componentes opcionales

`option()` define variables booleanas que el usuario puede
activar o desactivar al configurar el proyecto. Se usan
para controlar que subdirectorios se incluyen:

```cmake
# Sintaxis:
# option(<nombre> "<descripcion>" <valor_por_defecto>)

option(BUILD_TESTS "Build the test suite" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(BUILD_DOCS "Build documentation" OFF)

# El usuario controla estas opciones al configurar:
#   cmake -B build -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=ON
```

```cmake
# Uso tipico: condicionar add_subdirectory con if():

cmake_minimum_required(VERSION 3.20)
project(mylib C)

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_DOCS)
    add_subdirectory(docs)
endif()
```

```bash
# Construir solo la biblioteca, sin tests ni ejemplos:
cmake -B build -DBUILD_TESTS=OFF
cmake --build build

# Construir todo:
cmake -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build build

# Los valores se cachean en CMakeCache.txt. En configuraciones
# posteriores no hace falta repetirlos:
cmake -B build                # usa los valores cacheados
cmake -B build -DBUILD_DOCS=ON  # cambia solo BUILD_DOCS
```

```cmake
# Patron comun: habilitar tests solo si es el proyecto raiz.
# Si alguien incluye tu proyecto con add_subdirectory(),
# probablemente no quiere compilar tus tests.

cmake_minimum_required(VERSION 3.20)
project(mylib C)

# CMAKE_SOURCE_DIR == PROJECT_SOURCE_DIR solo si este es
# el proyecto raiz (nadie nos incluyo con add_subdirectory).
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(BUILD_TESTS "Build tests" ON)
else()
    option(BUILD_TESTS "Build tests" OFF)
endif()

add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

## add_subdirectory con EXCLUDE_FROM_ALL

El flag `EXCLUDE_FROM_ALL` indica que los targets del
subdirectorio no se compilan por defecto. Solo se compilan
si se piden explicitamente o si otro target depende de ellos:

```cmake
# Sin EXCLUDE_FROM_ALL:
add_subdirectory(examples)
# cmake --build build compila TODO, incluyendo examples.

# Con EXCLUDE_FROM_ALL:
add_subdirectory(examples EXCLUDE_FROM_ALL)
# cmake --build build NO compila examples.
# Para compilar un target especifico de examples:
#   cmake --build build --target demo_basic
```

```cmake
# Ejemplo: proyecto con ejemplos opcionales.

cmake_minimum_required(VERSION 3.20)
project(netlib C)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples EXCLUDE_FROM_ALL)
```

```cmake
# examples/CMakeLists.txt:
add_executable(demo_basic basic.c)
target_link_libraries(demo_basic PRIVATE netlib)

add_executable(demo_server server.c)
target_link_libraries(demo_server PRIVATE netlib)
```

```bash
# Compilar el proyecto (sin examples):
cmake -B build
cmake --build build
# Solo compila netlib y los tests. Los examples no se tocan.

# Compilar un ejemplo especifico:
cmake --build build --target demo_basic
# Compila demo_basic (y sus dependencias: netlib si no esta compilada).

# Compilar todos los examples no es directo con EXCLUDE_FROM_ALL.
# Solucion: agregar un custom target en examples/CMakeLists.txt:
add_custom_target(all_examples DEPENDS demo_basic demo_server)

# Ahora:
cmake --build build --target all_examples
# Compila todos los ejemplos de una vez.
```

```cmake
# Diferencia entre EXCLUDE_FROM_ALL y option():
#
#   EXCLUDE_FROM_ALL:
#     - Los targets existen pero no se compilan por defecto.
#     - Se pueden compilar individualmente.
#     - El CMakeLists.txt del subdirectorio siempre se procesa.
#
#   option() + if():
#     - El subdirectorio no se procesa en absoluto.
#     - Los targets no existen (no se pueden compilar ni referenciar).
#     - Mas limpio si el subdirectorio tiene muchos targets.
#
# Usar EXCLUDE_FROM_ALL para cosas que el desarrollador podria
# querer compilar ocasionalmente (examples, benchmarks).
# Usar option() para componentes que se habilitan/deshabilitan
# segun la configuracion (tests, soporte de una feature).
```

## Comparacion con Make recursivo

CMake con `add_subdirectory()` parece superficialmente similar
a Make recursivo (`$(MAKE) -C subdir`), pero el mecanismo
interno es fundamentalmente distinto:

```cmake
# Make recursivo:
#   - Cada sub-make es un proceso independiente.
#   - Cada sub-make ve SOLO sus archivos.
#   - No hay grafo global de dependencias.
#   - Cambios en lib/header.h no se detectan desde app/.
#   - Paralelismo con -j sobrecarga la maquina (N jobs x M sub-makes).
#
# CMake con add_subdirectory():
#   - Un solo proceso de CMake recorre todos los subdirectorios.
#   - Construye un grafo GLOBAL de dependencias.
#   - El build system generado (Makefile o Ninja) es NON-RECURSIVE.
#   - Cambios en cualquier header se detectan correctamente.
#   - Paralelismo gestionado por un unico proceso (jobs correctos).
```

```cmake
# CMake toma la estructura organizativa del recursive make
# (cada subdirectorio tiene su CMakeLists.txt) pero genera
# un build system non-recursive internamente.
#
# Es lo mejor de ambos mundos:
#
#   Propiedad                    Make recursivo   CMake
#   ────────────────────────────  ──────────────  ───────────────
#   Organizacion modular          SI              SI
#   Grafo global de dependencias  NO              SI
#   Deteccion correcta de cambios NO              SI
#   Paralelismo correcto          NO              SI
#   Cada modulo se gestiona solo  SI              SI
#
# Esta es una de las razones principales por las que CMake
# desplazo a Make en proyectos grandes de C y C++.
```

## Ejemplo completo

Un proyecto con tres subdirectorios: una biblioteca de utilidades
de strings, una biblioteca de formateo que depende de ella, y una
aplicacion que usa ambas:

```
strfmt/
  CMakeLists.txt
  lib/
    strutil/
      CMakeLists.txt
      include/strutil/
        strutil.h
      src/
        strutil.c
    format/
      CMakeLists.txt
      include/format/
        format.h
      src/
        format.c
  app/
    CMakeLists.txt
    main.c
  tests/
    CMakeLists.txt
    test_strutil.c
```

```c
// lib/strutil/include/strutil/strutil.h
#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>

// Reverse a string in place. Returns the same pointer.
char *str_reverse(char *s);

// Convert a string to uppercase in place. Returns the same pointer.
char *str_upper(char *s);

// Return the length without counting trailing whitespace.
size_t str_trimmed_len(const char *s);

#endif // STRUTIL_H
```

```c
// lib/strutil/src/strutil.c
#include "strutil/strutil.h"
#include <ctype.h>
#include <string.h>

char *str_reverse(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len / 2; i++) {
        char tmp = s[i];
        s[i] = s[len - 1 - i];
        s[len - 1 - i] = tmp;
    }
    return s;
}

char *str_upper(char *s) {
    for (char *p = s; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }
    return s;
}

size_t str_trimmed_len(const char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        len--;
    }
    return len;
}
```

```c
// lib/format/include/format/format.h
#ifndef FORMAT_H
#define FORMAT_H

// Format a greeting: writes "HELLO, <NAME>!" into buf.
// name is uppercased. buf must be large enough.
// Returns the number of characters written (excluding '\0').
int fmt_greeting(char *buf, size_t bufsize, const char *name);

// Format a reversed banner: writes "=== <REVERSED> ===" into buf.
// Returns the number of characters written (excluding '\0').
int fmt_banner(char *buf, size_t bufsize, const char *text);

#endif // FORMAT_H
```

```c
// lib/format/src/format.c
#include "format/format.h"
#include "strutil/strutil.h"
#include <stdio.h>
#include <string.h>

int fmt_greeting(char *buf, size_t bufsize, const char *name) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s", name);
    str_upper(tmp);
    return snprintf(buf, bufsize, "HELLO, %s!", tmp);
}

int fmt_banner(char *buf, size_t bufsize, const char *text) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s", text);
    str_reverse(tmp);
    return snprintf(buf, bufsize, "=== %s ===", tmp);
}
```

```c
// app/main.c
#include <stdio.h>
#include "strutil/strutil.h"
#include "format/format.h"

int main(void) {
    char buf[256];

    fmt_greeting(buf, sizeof(buf), "world");
    printf("%s\n", buf);

    fmt_banner(buf, sizeof(buf), "cmake");
    printf("%s\n", buf);

    char word[] = "subdirectory";
    printf("reversed: %s\n", str_reverse(word));
    printf("trimmed length of 'hello   ': %zu\n",
           str_trimmed_len("hello   "));

    return 0;
}
```

```c
// tests/test_strutil.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strutil/strutil.h"

#define ASSERT_STREQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "FAIL: %s (got '%s', expected '%s')\n", \
                    msg, a, b); \
            exit(1); \
        } \
        printf("PASS: %s\n", msg); \
    } while (0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            fprintf(stderr, "FAIL: %s (got %zu, expected %zu)\n", \
                    msg, (size_t)(a), (size_t)(b)); \
            exit(1); \
        } \
        printf("PASS: %s\n", msg); \
    } while (0)

int main(void) {
    char s1[] = "abcd";
    ASSERT_STREQ(str_reverse(s1), "dcba", "reverse basic");

    char s2[] = "a";
    ASSERT_STREQ(str_reverse(s2), "a", "reverse single char");

    char s3[] = "hello";
    ASSERT_STREQ(str_upper(s3), "HELLO", "upper basic");

    ASSERT_EQ(str_trimmed_len("hello   "), 5, "trimmed len with spaces");
    ASSERT_EQ(str_trimmed_len("hello"), 5, "trimmed len no spaces");
    ASSERT_EQ(str_trimmed_len("   "), 0, "trimmed len all spaces");

    printf("All tests passed.\n");
    return 0;
}
```

```cmake
# CMakeLists.txt (raiz)
cmake_minimum_required(VERSION 3.20)
project(strfmt VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build test suite" ON)

add_subdirectory(lib/strutil)
add_subdirectory(lib/format)
add_subdirectory(app)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

```cmake
# lib/strutil/CMakeLists.txt
add_library(strutil src/strutil.c)
add_library(strfmt::strutil ALIAS strutil)

target_include_directories(strutil
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(strutil PRIVATE -Wall -Wextra)
```

```cmake
# lib/format/CMakeLists.txt
add_library(format src/format.c)
add_library(strfmt::format ALIAS format)

target_include_directories(format
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# format depende de strutil. PUBLIC porque los headers de format
# incluyen headers de strutil, asi que consumidores de format
# tambien necesitan los includes de strutil.
target_link_libraries(format PUBLIC strutil)

target_compile_options(format PRIVATE -Wall -Wextra)
```

```cmake
# app/CMakeLists.txt
add_executable(strfmt_app main.c)

# strfmt_app usa format (que trae strutil transitivamente via PUBLIC)
# y tambien usa strutil directamente. Listar ambos es correcto
# y explicito, pero bastaria con format si solo se usa via format.
target_link_libraries(strfmt_app PRIVATE strfmt::format strfmt::strutil)
```

```cmake
# tests/CMakeLists.txt
add_executable(test_strutil test_strutil.c)
target_link_libraries(test_strutil PRIVATE strfmt::strutil)
```

```bash
# Sesion de compilacion:
$ cmake -B build
-- The C compiler identification is GNU 13.2.0
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - works
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/strfmt/build

$ cmake --build build
[ 16%] Building C object lib/strutil/CMakeFiles/strutil.dir/src/strutil.c.o
[ 33%] Linking C static library libstrutil.a
[ 33%] Built target strutil
[ 50%] Building C object lib/format/CMakeFiles/format.dir/src/format.c.o
[ 66%] Linking C static library libformat.a
[ 66%] Built target format
[ 83%] Building C object app/CMakeFiles/strfmt_app.dir/main.c.o
[100%] Linking C executable strfmt_app
[100%] Built target strfmt_app
# tests tambien se compilan (BUILD_TESTS=ON por defecto)

$ ./build/app/strfmt_app
HELLO, WORLD!
=== ekamC ===
reversed: yrotceridbus
trimmed length of 'hello   ': 5

$ ./build/tests/test_strutil
PASS: reverse basic
PASS: reverse single char
PASS: upper basic
PASS: trimmed len with spaces
PASS: trimmed len no spaces
PASS: trimmed len all spaces
All tests passed.
```

```bash
# Compilar sin tests:
$ cmake -B build_notests -DBUILD_TESTS=OFF
$ cmake --build build_notests
# Solo compila strutil, format y strfmt_app. Sin test_strutil.
```

```bash
# Verificar la propagacion de propiedades con modo verbose:
$ cmake --build build -- VERBOSE=1

# strutil.c:
# gcc -Wall -Wextra -std=gnu17 -Ilib/strutil/include -c ...
#     ^^^^^^^^^^^^^ propagacion PRIVATE de strutil

# format.c:
# gcc -Wall -Wextra -std=gnu17 -Ilib/format/include -Ilib/strutil/include -c ...
#                               ^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^
#                               PUBLIC de format    heredado de strutil (PUBLIC)

# main.c:
# gcc -std=gnu17 -Ilib/format/include -Ilib/strutil/include -c ...
#                ^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^
#                heredado de format  heredado de strutil (transitivo)
#                NO tiene -Wall -Wextra (son PRIVATE de cada lib)
```

---

## Ejercicios

### Ejercicio 1 — Proyecto con dos bibliotecas y alias

```cmake
# Crear un proyecto con esta estructura:
#
#   mathproject/
#     CMakeLists.txt
#     lib/
#       vector/
#         CMakeLists.txt
#         include/vector/vector.h
#         src/vector.c
#       matrix/
#         CMakeLists.txt
#         include/matrix/matrix.h
#         src/matrix.c           (usa vector internamente)
#     app/
#       CMakeLists.txt
#       main.c
#
# Implementar:
#   vector: funciones vec_add(a,b) y vec_dot(a,b) para vectores de 3 floats.
#   matrix: funcion mat_identity(m) que inicializa una matriz 3x3.
#           mat_identity usa vec_dot internamente (dependencia de vector).
#   app: un main.c que usa ambas bibliotecas.
#
# Requisitos del CMake:
#   1. Cada biblioteca debe tener un ALIAS con namespace "mathproject::"
#   2. matrix debe depender de vector con visibilidad PUBLIC
#   3. app debe enlazar solo contra mathproject::matrix
#   4. Verificar que app hereda los include directories de vector
#      transitivamente (sin listarlos explicitamente)
#
# Verificar con cmake --build build -- VERBOSE=1 que los includes
# de vector/ aparecen en la compilacion de main.c.
```

### Ejercicio 2 — option() y EXCLUDE_FROM_ALL

```cmake
# Extender el proyecto del ejercicio 1 agregando:
#
#   mathproject/
#     ...estructura anterior...
#     tests/
#       CMakeLists.txt
#       test_vector.c
#       test_matrix.c
#     examples/
#       CMakeLists.txt
#       demo_dot_product.c
#       demo_matrix.c
#
# Requisitos:
#   1. Agregar option(BUILD_TESTS "Build tests" ON) al CMakeLists.txt raiz
#   2. Usar if(BUILD_TESTS) para condicionar add_subdirectory(tests)
#   3. Agregar examples con EXCLUDE_FROM_ALL
#   4. Crear un custom target "all_examples" que compile todos los demos
#
# Probar los siguientes escenarios:
#   a) cmake -B build                          → compila libs + app + tests
#   b) cmake -B build -DBUILD_TESTS=OFF        → compila libs + app (sin tests)
#   c) cmake --build build --target demo_matrix → compila solo el demo
#   d) cmake --build build --target all_examples → compila todos los demos
#
# Verificar que en el caso (b), el target test_vector no existe.
```

### Ejercicio 3 — Scope de variables y PARENT_SCOPE

```cmake
# Crear un proyecto que demuestre el scope de variables:
#
#   scopetest/
#     CMakeLists.txt
#     moduleA/
#       CMakeLists.txt
#     moduleB/
#       CMakeLists.txt
#
# En el CMakeLists.txt raiz:
#   set(SHARED_VAR "from_root")
#   set(ROOT_ONLY "only_in_root")
#
# En moduleA/CMakeLists.txt:
#   1. Imprimir SHARED_VAR (deberia ser "from_root")
#   2. set(SHARED_VAR "modified_by_A") — modificar localmente
#   3. Imprimir SHARED_VAR (deberia ser "modified_by_A")
#   4. set(A_VERSION "1.0" PARENT_SCOPE) — propagar al padre
#   5. Imprimir A_VERSION localmente (predecir: estara vacio?)
#
# En moduleB/CMakeLists.txt:
#   1. Imprimir SHARED_VAR (predecir: "from_root" o "modified_by_A"?)
#   2. Imprimir ROOT_ONLY (predecir: visible o no?)
#
# En el CMakeLists.txt raiz, DESPUES de los add_subdirectory():
#   1. Imprimir SHARED_VAR (predecir: "from_root" o "modified_by_A"?)
#   2. Imprimir A_VERSION (predecir: "1.0" o vacio?)
#
# Ejecutar cmake -B build y comparar las predicciones con la
# salida real. Para cada variable, explicar POR QUE tiene ese valor
# basandose en las reglas de scope.
```
