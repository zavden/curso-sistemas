# T02 — find_package y modulos

## El problema

Tu proyecto en C necesita usar bibliotecas externas: zlib para
compresion, OpenSSL para criptografia, curl para HTTP. Estas
bibliotecas ya estan instaladas en el sistema, pero sus archivos
no estan en la misma ruta en todas las distribuciones:

```bash
# En Fedora, los headers de zlib estan en:
/usr/include/zlib.h
# Y la biblioteca en:
/usr/lib64/libz.so

# En Ubuntu:
/usr/include/zlib.h
/usr/lib/x86_64-linux-gnu/libz.so

# En una instalacion manual:
/opt/zlib/include/zlib.h
/opt/zlib/lib/libz.so

# En macOS con Homebrew:
/opt/homebrew/include/zlib.h
/opt/homebrew/lib/libz.dylib
```

```cmake
# Solucion ingenua: hardcodear las rutas.
# Esto funciona en TU maquina y en ninguna otra.
target_include_directories(app PRIVATE /usr/include)
target_link_libraries(app PRIVATE /usr/lib64/libz.so)

# CMake resuelve esto con find_package(): una funcion que busca
# la biblioteca en rutas estandar del sistema y crea targets
# o variables con las rutas correctas.
```

## find_package()

`find_package()` busca una biblioteca instalada en el sistema.
Si la encuentra, crea targets importados o define variables con
las rutas de headers y bibliotecas:

```cmake
cmake_minimum_required(VERSION 3.20)
project(myapp C)

# Buscar zlib. REQUIRED hace que CMake aborte si no la encuentra.
find_package(ZLIB REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE ZLIB::ZLIB)
# ZLIB::ZLIB es un target importado que find_package creo.
# Propaga automaticamente:
#   - El directorio de headers (-I/usr/include)
#   - La biblioteca para el linker (-lz)
#   - Cualquier flag necesario
```

```bash
# Configurar y compilar:
cmake -B build
# -- Found ZLIB: /usr/lib64/libz.so (found version "1.2.13")

cmake --build build
# gcc -I/usr/include -o app main.c -lz
```

```cmake
# Si la biblioteca no esta instalada y se uso REQUIRED:
find_package(ZLIB REQUIRED)
# CMake error:
#   Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
#
# El error detalla que variables no se pudieron resolver.
# El build se detiene inmediatamente.
```

## Module mode vs Config mode

`find_package()` tiene dos modos de busqueda. Entender la diferencia
es clave para diagnosticar problemas cuando una biblioteca no se
encuentra.

```cmake
# Modo 1: Module mode
# CMake busca un archivo llamado FindPackage.cmake.
# Primero en CMAKE_MODULE_PATH, luego en sus modulos built-in.
# El archivo Find contiene la logica de busqueda: llama a
# find_path(), find_library(), etc.
#
# Ejemplo: CMake trae FindZLIB.cmake, FindOpenSSL.cmake,
# FindCURL.cmake, FindThreads.cmake, etc.
# Estos modulos saben donde buscar en cada plataforma.

find_package(ZLIB REQUIRED)
# CMake busca:
#   1. ${CMAKE_MODULE_PATH}/FindZLIB.cmake
#   2. <cmake-install>/share/cmake-3.x/Modules/FindZLIB.cmake
# Encuentra su modulo built-in FindZLIB.cmake y lo ejecuta.
```

```cmake
# Modo 2: Config mode
# CMake busca un archivo llamado PackageConfig.cmake o
# package-config.cmake, provisto por el propio paquete.
# El paquete se instalo con soporte CMake y dejo su archivo
# de configuracion en una ruta estandar.
#
# Ejemplo: si instalas fmt con CMake, su install deja
# /usr/lib64/cmake/fmt/fmtConfig.cmake
# Ese archivo define los targets importados directamente.

find_package(fmt REQUIRED)
# CMake busca:
#   1. /usr/lib64/cmake/fmt/fmtConfig.cmake
#   2. /usr/lib64/cmake/fmt/fmt-config.cmake
#   3. Otras rutas estandar del sistema
# No necesita un modulo Find — el paquete se describe a si mismo.
```

```cmake
# Orden de busqueda por defecto (CMake 3.15+):
#
#   1. Config mode — busca PackageConfig.cmake
#   2. Module mode — busca FindPackage.cmake
#
# Si el paquete tiene archivo Config, se usa Config mode.
# Si no, CMake cae al Module mode y busca un Find module.
#
# Se puede forzar el modo:
find_package(ZLIB MODULE REQUIRED)   # solo Module mode
find_package(fmt CONFIG REQUIRED)    # solo Config mode
```

```cmake
# Diferencias practicas:
#
#   Aspecto         Module mode              Config mode
#   ──────────────  ───────────────────────  ─────────────────────────
#   Quien lo crea   CMake o el usuario       El propio paquete
#   Archivo         FindPackage.cmake        PackageConfig.cmake
#   Ubicacion       CMAKE_MODULE_PATH o      Junto a la instalacion
#                   modulos built-in          del paquete
#   Calidad         Variable (algunos son    Consistente (el autor
#                   viejos y usan variables   del paquete define los
#                   en vez de targets)        targets correctos)
#   Tendencia       Legacy                   Moderno (preferido)
```

## Targets importados

Un target importado es un target que no se construye en tu
proyecto sino que representa una biblioteca externa ya compilada.
Se usa exactamente igual que un target local:

```cmake
find_package(OpenSSL REQUIRED)

# find_package(OpenSSL) crea estos targets importados:
#   OpenSSL::SSL     — biblioteca libssl
#   OpenSSL::Crypto  — biblioteca libcrypto

add_executable(app main.c)
target_link_libraries(app PRIVATE OpenSSL::SSL)

# OpenSSL::SSL propaga automaticamente:
#   - Los include directories de OpenSSL
#   - La biblioteca libssl para el linker
#   - La dependencia transitiva OpenSSL::Crypto
#
# No hay que buscar manualmente los headers ni las rutas.
```

```cmake
# Algunos paquetes crean multiples targets:
find_package(Boost REQUIRED COMPONENTS filesystem system)
# Boost::filesystem
# Boost::system
# Boost::headers  (solo headers)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
# Qt6::Widgets
# Qt6::Network

find_package(CURL REQUIRED)
# CURL::libcurl
```

```cmake
# Los targets importados se comportan como targets normales.
# Tienen propiedades que se propagan via target_link_libraries:

# Ver que propiedades tiene un target importado:
# Despues de cmake -B build, inspeccionar con:
#   cmake --build build --target help
#
# O en CMakeLists.txt, imprimir propiedades:
find_package(ZLIB REQUIRED)
get_target_property(ZLIB_INC ZLIB::ZLIB INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(ZLIB_LIB ZLIB::ZLIB IMPORTED_LOCATION)
message(STATUS "ZLIB includes: ${ZLIB_INC}")
message(STATUS "ZLIB library:  ${ZLIB_LIB}")
# -- ZLIB includes: /usr/include
# -- ZLIB library:  /usr/lib64/libz.so
```

## Variables legacy

Algunos Find modules antiguos (o la forma vieja de usarlos) definen
variables en lugar de targets. Es importante reconocer este patron
porque aparece en documentacion vieja y proyectos existentes:

```cmake
# Forma legacy con variables:
find_package(ZLIB REQUIRED)

# El modulo FindZLIB define:
#   ZLIB_FOUND          — TRUE si se encontro
#   ZLIB_INCLUDE_DIRS   — directorio con zlib.h
#   ZLIB_LIBRARIES      — ruta a libz.so/libz.a
#   ZLIB_VERSION_STRING — version encontrada (e.g. "1.2.13")

# Uso legacy:
add_executable(app main.c)
target_include_directories(app PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${ZLIB_LIBRARIES})

# Esto funciona pero es inferior a targets importados:
# - No propaga dependencias transitivas
# - No propaga flags de compilacion
# - Hay que repetir include_directories y link_libraries
```

```cmake
# Forma moderna con targets importados:
find_package(ZLIB REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE ZLIB::ZLIB)

# Una sola linea. Las propiedades se propagan automaticamente.
# Siempre preferir el target importado (Package::Component)
# sobre las variables legacy.
```

```cmake
# Como saber si un paquete provee targets importados:
# 1. Leer la documentacion del modulo Find:
#    cmake --help-module FindZLIB
#    cmake --help-module FindOpenSSL
#
# 2. Buscar "Imported Targets" en la salida.
#    Si la seccion existe, hay targets disponibles.
#
# 3. En la practica, casi todos los modulos Find de CMake 3.x
#    modernos proveen targets importados.
```

## REQUIRED y QUIET

```cmake
# REQUIRED — aborta la configuracion si no se encuentra:
find_package(ZLIB REQUIRED)
# Si falla:
#   CMake Error at CMakeLists.txt:5 (find_package):
#     Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)

# Sin REQUIRED — es opcional. Hay que verificar manualmente:
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(app PRIVATE ZLIB::ZLIB)
    target_compile_definitions(app PRIVATE HAS_ZLIB)
else()
    message(STATUS "ZLIB not found, compression disabled")
endif()
# Util para dependencias opcionales: si existe, la usas;
# si no, compilas sin esa funcionalidad.
```

```cmake
# QUIET — suprime los mensajes informativos:
find_package(ZLIB QUIET)
# No imprime "-- Found ZLIB: /usr/lib64/libz.so ..."
# Util cuando pruebas si algo existe sin ensuciar la salida.
# Si se combina con REQUIRED y falla, el error SI se muestra.

# Patron tipico para dependencias opcionales:
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    message(STATUS "ZLIB found: compression enabled")
    target_link_libraries(app PRIVATE ZLIB::ZLIB)
    target_compile_definitions(app PRIVATE HAS_COMPRESSION)
endif()
```

## Versiones

Se puede requerir una version minima o un rango de versiones:

```cmake
# Version minima:
find_package(ZLIB 1.2.11 REQUIRED)
# Solo acepta zlib >= 1.2.11. Si la instalada es 1.2.8, falla:
#   Could NOT find ZLIB (found version "1.2.8", required is "1.2.11")

# Version exacta:
find_package(ZLIB 1.2.13 EXACT REQUIRED)
# Solo acepta exactamente 1.2.13. Cualquier otra version falla.
```

```cmake
# Rango de versiones (CMake 3.19+):
find_package(ZLIB 1.2.11...1.3.0 REQUIRED)
# Acepta >= 1.2.11 y < 1.3.0.
# No todos los Find modules soportan rangos.
# Los archivos Config modernos generalmente si los soportan.
```

```cmake
# Despues de find_package, la version encontrada esta en
# PackageName_VERSION:
find_package(ZLIB REQUIRED)
message(STATUS "Found ZLIB version: ${ZLIB_VERSION_STRING}")

# Comparar versiones en logica condicional:
if(ZLIB_VERSION_STRING VERSION_GREATER_EQUAL "1.2.11")
    message(STATUS "ZLIB version is sufficient")
endif()

# Operadores de version disponibles:
#   VERSION_LESS
#   VERSION_GREATER
#   VERSION_EQUAL
#   VERSION_LESS_EQUAL
#   VERSION_GREATER_EQUAL
```

## COMPONENTS

Algunos paquetes se dividen en componentes. Se puede pedir solo
los que se necesitan:

```cmake
# Boost tiene decenas de componentes:
find_package(Boost REQUIRED COMPONENTS filesystem system regex)
# Solo busca y configura filesystem, system y regex.
# Si alguno no existe, falla (porque se uso REQUIRED).

add_executable(app main.c)
target_link_libraries(app PRIVATE
    Boost::filesystem
    Boost::system
    Boost::regex
)
```

```cmake
# Componentes opcionales:
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(Boost OPTIONAL_COMPONENTS regex)
# filesystem es obligatorio, regex es opcional.

if(Boost_regex_FOUND)
    target_link_libraries(app PRIVATE Boost::regex)
    target_compile_definitions(app PRIVATE HAS_REGEX)
endif()
```

```cmake
# No todos los paquetes soportan componentes.
# Si se piden componentes a un paquete que no los soporta,
# generalmente se ignoran sin error.
# Los paquetes que tipicamente usan componentes:
#   - Boost (filesystem, system, thread, regex, ...)
#   - Qt (Widgets, Network, Sql, ...)
#   - OpenCV (core, imgproc, highgui, ...)
```

## CMAKE_PREFIX_PATH

Cuando una biblioteca esta instalada en una ruta no estandar,
`find_package()` no la encuentra. `CMAKE_PREFIX_PATH` le dice
a CMake donde buscar:

```bash
# La biblioteca esta en /opt/mylibs:
#   /opt/mylibs/include/mylib.h
#   /opt/mylibs/lib/libmylib.so
#   /opt/mylibs/lib/cmake/mylib/mylibConfig.cmake

# Sin CMAKE_PREFIX_PATH:
cmake -B build
# -- Could NOT find mylib

# Con CMAKE_PREFIX_PATH:
cmake -B build -DCMAKE_PREFIX_PATH=/opt/mylibs
# -- Found mylib: /opt/mylibs/lib/libmylib.so
```

```bash
# Multiples rutas separadas por punto y coma:
cmake -B build -DCMAKE_PREFIX_PATH="/opt/mylibs;/opt/another"

# Tambien se puede definir como variable de entorno:
export CMAKE_PREFIX_PATH="/opt/mylibs:/opt/another"
cmake -B build
# Nota: como variable de entorno usa : (separador del sistema).
# Como variable CMake usa ; (separador de listas CMake).
```

```cmake
# Tambien se puede definir dentro del CMakeLists.txt,
# aunque es menos flexible (mejor pasarlo desde la linea de comandos):
list(APPEND CMAKE_PREFIX_PATH "/opt/mylibs")
find_package(mylib REQUIRED)
```

```cmake
# Otras variables que afectan la busqueda:
#
# CMAKE_PREFIX_PATH       — raiz de instalacion (busca lib/, include/, etc.)
# PackageName_DIR         — ruta directa al directorio con el Config.cmake
# CMAKE_MODULE_PATH       — directorios adicionales para Find modules
#
# Ejemplo con PackageName_DIR:
cmake -B build -Dmylib_DIR=/opt/mylibs/lib/cmake/mylib
# Apunta directamente al directorio con mylibConfig.cmake.
```

## PkgConfig integration

Muchas bibliotecas en Linux no traen soporte CMake nativo pero
si proveen archivos `.pc` para `pkg-config`. CMake puede usarlos
a traves del modulo `PkgConfig`:

```bash
# pkg-config es una herramienta del sistema que gestiona flags
# de compilacion de bibliotecas instaladas:
pkg-config --cflags libpng
# -I/usr/include/libpng16

pkg-config --libs libpng
# -lpng16 -lz

# Los archivos .pc estan tipicamente en:
# /usr/lib64/pkgconfig/
# /usr/lib/x86_64-linux-gnu/pkgconfig/
# /usr/share/pkgconfig/
```

```cmake
# Paso 1: buscar el modulo PkgConfig de CMake:
find_package(PkgConfig REQUIRED)
# Esto pone disponibles los comandos pkg_check_modules y
# pkg_search_module.
```

```cmake
# Paso 2 (forma legacy con variables):
pkg_check_modules(PNG REQUIRED libpng)

# Crea variables:
#   PNG_FOUND          — TRUE si se encontro
#   PNG_INCLUDE_DIRS   — directorios de headers
#   PNG_LIBRARIES      — nombres de bibliotecas para el linker
#   PNG_LIBRARY_DIRS   — directorios donde estan las bibliotecas
#   PNG_CFLAGS         — flags de compilacion completos
#   PNG_VERSION        — version encontrada

add_executable(app main.c)
target_include_directories(app PRIVATE ${PNG_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${PNG_LIBRARIES})
target_link_directories(app PRIVATE ${PNG_LIBRARY_DIRS})
```

```cmake
# Paso 2 (forma moderna con target importado):
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

# Crea un target importado PkgConfig::PNG:
add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::PNG)
# Una sola linea — headers, flags y bibliotecas se propagan.
# Siempre preferir IMPORTED_TARGET sobre las variables.
```

```cmake
# Buscar multiples paquetes con un solo comando:
pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET
    libpng
    libjpeg
    libxml-2.0
)
# Crea PkgConfig::DEPS que agrupa las tres bibliotecas.

# O buscar cada uno por separado para targets independientes:
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)

add_executable(app main.c)
target_link_libraries(app PRIVATE
    PkgConfig::PNG
    PkgConfig::JPEG
)
```

```cmake
# Requerir una version minima:
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng>=1.6.0)

# pkg_search_module — busca el primero disponible de una lista:
pkg_search_module(JSON REQUIRED IMPORTED_TARGET json-c jansson)
# Intenta json-c primero. Si no esta, intenta jansson.
# Crea PkgConfig::JSON con el que encuentre.
```

## Escribir un Find module basico

Cuando una biblioteca no tiene soporte CMake (Config mode) ni
archivo `.pc` para pkg-config, se puede escribir un Find module
propio. Esto es necesario para bibliotecas legacy o internas:

```cmake
# cmake/FindMyLib.cmake
#
# Estructura tipica de un Find module:
# 1. Buscar los headers con find_path()
# 2. Buscar la biblioteca con find_library()
# 3. Validar con find_package_handle_standard_args()
# 4. Crear un target importado

# --- 1. Buscar el header ---
find_path(MyLib_INCLUDE_DIR
    NAMES mylib.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/mylib/include
    PATH_SUFFIXES mylib
)
# Busca mylib.h en las rutas listadas.
# PATH_SUFFIXES permite buscar en subdirectorios (e.g., /usr/include/mylib/).
# Si lo encuentra, MyLib_INCLUDE_DIR = /usr/include/mylib (por ejemplo).

# --- 2. Buscar la biblioteca ---
find_library(MyLib_LIBRARY
    NAMES mylib libmylib
    PATHS
        /usr/lib
        /usr/lib64
        /usr/local/lib
        /opt/mylib/lib
)
# Busca libmylib.so, libmylib.a, mylib.lib, etc.
# Si lo encuentra, MyLib_LIBRARY = /usr/lib64/libmylib.so (por ejemplo).

# --- 3. Validar ---
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MyLib
    REQUIRED_VARS MyLib_LIBRARY MyLib_INCLUDE_DIR
)
# Si ambas variables estan definidas, marca MyLib_FOUND = TRUE.
# Si alguna falta y se uso REQUIRED en find_package(), aborta con error.
# Imprime un mensaje estandar: "-- Found MyLib: /usr/lib64/libmylib.so"

# --- 4. Crear target importado ---
if(MyLib_FOUND AND NOT TARGET MyLib::MyLib)
    add_library(MyLib::MyLib UNKNOWN IMPORTED)
    set_target_properties(MyLib::MyLib PROPERTIES
        IMPORTED_LOCATION "${MyLib_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MyLib_INCLUDE_DIR}"
    )
endif()
# UNKNOWN IMPORTED — CMake detecta automaticamente si es .so o .a.
# INTERFACE_INCLUDE_DIRECTORIES se propaga a quien enlace contra MyLib::MyLib.
```

```cmake
# Para usar el Find module, hay que decirle a CMake donde buscarlo:

# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(myapp C)

# Agregar el directorio cmake/ al module path:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(MyLib REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE MyLib::MyLib)
```

```
# Estructura del proyecto:
#
#   project/
#   ├── CMakeLists.txt
#   ├── cmake/
#   │   └── FindMyLib.cmake
#   └── src/
#       └── main.c
```

```cmake
# Cuando escribir un Find module:
#
# 1. La biblioteca no tiene soporte CMake (no provee Config.cmake)
# 2. La biblioteca no tiene archivo .pc para pkg-config
# 3. Necesitas logica de busqueda especial (multiples variantes,
#    headers en rutas inusuales, etc.)
#
# En la practica, la mayoria de bibliotecas modernas proveen Config
# mode o al menos un .pc. Solo bibliotecas muy viejas o internas
# requieren un Find module manual.
```

## Ejemplo completo

Un proyecto que usa zlib (via `find_package`) y libpng (via
`pkg-config`) para comprimir y manipular imagenes:

```
# Estructura del proyecto:
#
#   compress_tool/
#   ├── CMakeLists.txt
#   └── src/
#       ├── compress.c
#       ├── compress.h
#       └── main.c
```

```c
// src/compress.h
#ifndef COMPRESS_H
#define COMPRESS_H

#include <stddef.h>

// Compress data using zlib.
// Returns 0 on success, -1 on failure.
int compress_data(const void *src, size_t src_len,
                  void *dst, size_t *dst_len);

// Decompress data using zlib.
int decompress_data(const void *src, size_t src_len,
                    void *dst, size_t *dst_len);

#endif // COMPRESS_H
```

```c
// src/compress.c
#include "compress.h"
#include <zlib.h>

int compress_data(const void *src, size_t src_len,
                  void *dst, size_t *dst_len) {
    uLongf out_len = (uLongf)*dst_len;
    int ret = compress2((Bytef *)dst, &out_len,
                        (const Bytef *)src, (uLong)src_len,
                        Z_BEST_COMPRESSION);
    *dst_len = (size_t)out_len;
    return (ret == Z_OK) ? 0 : -1;
}

int decompress_data(const void *src, size_t src_len,
                    void *dst, size_t *dst_len) {
    uLongf out_len = (uLongf)*dst_len;
    int ret = uncompress((Bytef *)dst, &out_len,
                         (const Bytef *)src, (uLong)src_len);
    *dst_len = (size_t)out_len;
    return (ret == Z_OK) ? 0 : -1;
}
```

```c
// src/main.c
#include <stdio.h>
#include <string.h>
#include "compress.h"

int main(void) {
    const char *original = "Hello, this is a test of zlib compression "
                           "with CMake find_package integration.";
    size_t orig_len = strlen(original) + 1;

    // Compress.
    unsigned char compressed[256];
    size_t comp_len = sizeof(compressed);
    if (compress_data(original, orig_len, compressed, &comp_len) != 0) {
        fprintf(stderr, "Compression failed\n");
        return 1;
    }
    printf("Original:   %zu bytes\n", orig_len);
    printf("Compressed: %zu bytes\n", comp_len);

    // Decompress.
    char restored[256];
    size_t rest_len = sizeof(restored);
    if (decompress_data(compressed, comp_len, restored, &rest_len) != 0) {
        fprintf(stderr, "Decompression failed\n");
        return 1;
    }
    printf("Restored:   %s\n", restored);

    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(compress_tool C)

# --- Dependencia 1: zlib (via find_package, Module mode) ---
find_package(ZLIB 1.2.11 REQUIRED)
# FindZLIB.cmake (modulo built-in de CMake) busca zlib.
# Crea el target importado ZLIB::ZLIB.

# --- Dependencia 2: libpng (via pkg-config) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng>=1.6.0)
# Crea el target importado PkgConfig::PNG.

# --- Biblioteca interna ---
add_library(compress STATIC src/compress.c)
target_include_directories(compress
    PUBLIC src/               # compress.h accesible para quien use compress
)
target_link_libraries(compress
    PUBLIC ZLIB::ZLIB         # zlib se propaga a quien use compress
)
target_compile_options(compress
    PRIVATE -Wall -Wextra
)

# --- Ejecutable ---
add_executable(compress_tool src/main.c)
target_link_libraries(compress_tool
    PRIVATE compress          # hereda ZLIB::ZLIB via PUBLIC
    PRIVATE PkgConfig::PNG    # enlaza libpng
)
```

```bash
# Asegurarse de que las dependencias estan instaladas:

# Fedora:
sudo dnf install zlib-devel libpng-devel

# Ubuntu/Debian:
sudo apt install zlib1g-dev libpng-dev

# Verificar con pkg-config:
pkg-config --modversion libpng
# 1.6.40

pkg-config --modversion zlib
# 1.2.13
```

```bash
# Compilar:
cmake -B build
# -- Found PkgConfig: /usr/bin/pkg-config (found version "0.29.2")
# -- Found ZLIB: /usr/lib64/libz.so (found version "1.2.13")
# -- Checking for module 'libpng>=1.6.0'
# --   Found libpng, version 1.6.40

cmake --build build
# [ 33%] Building C object CMakeFiles/compress.dir/src/compress.c.o
# [ 66%] Building C object CMakeFiles/compress_tool.dir/src/main.c.o
# [100%] Linking C executable compress_tool

./build/compress_tool
# Original:   82 bytes
# Compressed: 78 bytes
# Restored:   Hello, this is a test of zlib compression ...
```

```bash
# Verificar los flags con modo verbose:
cmake --build build -- VERBOSE=1

# compress.c se compila con:
#   gcc -Wall -Wextra -I/usr/include -c src/compress.c
#                      ^^^^^^^^^^^^^^
#                      heredado de ZLIB::ZLIB

# main.c se compila con:
#   gcc -I/usr/include -I/usr/include/libpng16 -c src/main.c
#       ^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^
#       ZLIB (via compress PUBLIC)  libpng (PkgConfig::PNG)

# El linker recibe:
#   gcc -o compress_tool main.c.o compress.c.o -lz -lpng16
```

---

## Ejercicios

### Ejercicio 1 --- Dependencia con find_package y version

```cmake
# Crear un proyecto que:
#   1. Use find_package para encontrar ZLIB con version minima 1.2.0
#   2. Use find_package para encontrar Threads (REQUIRED)
#   3. Cree una biblioteca STATIC llamada netutil (src/netutil.c)
#      que enlace contra ZLIB::ZLIB (PUBLIC) y Threads::Threads (PRIVATE)
#   4. Cree un ejecutable app (src/main.c) que enlace contra netutil
#
# Verificar:
#   a. cmake -B build muestra las versiones encontradas
#   b. cmake --build build -- VERBOSE=1 muestra que app hereda
#      los flags de ZLIB (PUBLIC) pero NO los de Threads (PRIVATE)
#   c. Agregar message(STATUS "ZLIB version: ${ZLIB_VERSION_STRING}")
#      para imprimir la version durante la configuracion
#   d. Probar con una version minima imposible (e.g., ZLIB 99.0 REQUIRED)
#      y verificar el mensaje de error
```

### Ejercicio 2 --- Dependencia opcional con pkg-config

```cmake
# Crear un proyecto que:
#   1. Busque ZLIB con find_package (REQUIRED)
#   2. Busque libxml-2.0 con pkg-config (OPTIONAL — sin REQUIRED)
#   3. Si libxml-2.0 esta disponible:
#      - Defina HAS_XML como compile definition del ejecutable
#      - Enlace contra PkgConfig::XML
#   4. Si no esta disponible:
#      - Imprima un mensaje: "libxml2 not found, XML support disabled"
#
# Esqueleto:
#   find_package(PkgConfig REQUIRED)
#   pkg_check_modules(XML IMPORTED_TARGET libxml-2.0)
#   if(XML_FOUND)
#       ...
#   else()
#       ...
#   endif()
#
# Verificar:
#   a. Con libxml2-devel instalado: debe compilar con -DHAS_XML
#   b. Sin libxml2-devel: debe compilar sin error, sin el flag
#   c. En el codigo C, usar #ifdef HAS_XML para condicionar la funcionalidad
```

### Ejercicio 3 --- Escribir un Find module

```cmake
# Escribir un FindSimpleMath.cmake para una biblioteca ficticia:
#
# Suponer que la biblioteca esta instalada en /usr/local:
#   /usr/local/include/simplemath.h
#   /usr/local/lib/libsimplemath.so
#
# El Find module debe:
#   1. Usar find_path() para buscar simplemath.h
#   2. Usar find_library() para buscar simplemath/libsimplemath
#   3. Usar find_package_handle_standard_args() para validar
#   4. Crear un target importado SimpleMath::SimpleMath
#
# Estructura del proyecto:
#   project/
#   ├── CMakeLists.txt
#   ├── cmake/
#   │   └── FindSimpleMath.cmake
#   └── src/
#       └── main.c
#
# En CMakeLists.txt:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
#   find_package(SimpleMath REQUIRED)
#   add_executable(app src/main.c)
#   target_link_libraries(app PRIVATE SimpleMath::SimpleMath)
#
# Para probar sin instalar la biblioteca real:
#   1. Crear los archivos ficticios:
#      mkdir -p /tmp/fakelib/include /tmp/fakelib/lib
#      echo "int sm_add(int a, int b);" > /tmp/fakelib/include/simplemath.h
#      touch /tmp/fakelib/lib/libsimplemath.so
#   2. Pasar CMAKE_PREFIX_PATH=/tmp/fakelib
#   3. Verificar que CMake encuentra la biblioteca
#      (la compilacion fallara porque el .so esta vacio, pero
#      el objetivo es verificar que find_package funciona)
```
