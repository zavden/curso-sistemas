# T03 — Install y export

## Que es instalar

Instalar un proyecto significa copiar los artefactos compilados
(ejecutables, bibliotecas, headers) a un destino estandar del
sistema donde otros programas y usuarios los puedan encontrar.
En Linux, el destino por defecto es `/usr/local`:

```bash
# Despues de instalar, los archivos quedan en rutas estandar:
#
#   /usr/local/
#   ├── bin/           ← ejecutables (my_app)
#   ├── lib/           ← bibliotecas (.so, .a)
#   ├── include/       ← headers (.h)
#   └── share/         ← datos, documentacion, etc.
#
# Esto permite que:
# 1. El usuario ejecute el programa sin dar la ruta completa
#    (si /usr/local/bin esta en el PATH)
# 2. Otros proyectos encuentren la biblioteca con find_package()
# 3. El linker encuentre las .so en rutas estandar
# 4. Los headers se incluyan con #include <mylib/header.h>
#
# Sin instalar, los artefactos viven dentro de build/ y solo
# son accesibles dando la ruta completa: ./build/my_app
```

## install(TARGETS)

El comando `install(TARGETS)` instala ejecutables y bibliotecas.
CMake determina automaticamente que tipo de artefacto es cada
target y lo coloca en el subdirectorio correcto:

```cmake
# Instalar un ejecutable:
add_executable(my_app main.c)
install(TARGETS my_app)

# CMake sabe que my_app es un ejecutable y lo copia a bin/.
# Por defecto: /usr/local/bin/my_app
```

```cmake
# Especificar destinos por tipo de artefacto:
add_library(mylib SHARED src/mylib.c)
add_executable(my_app src/main.c)

install(TARGETS my_app mylib
    RUNTIME DESTINATION bin          # ejecutables → bin/
    LIBRARY DESTINATION lib          # bibliotecas .so → lib/
    ARCHIVE DESTINATION lib          # bibliotecas .a → lib/
)

# RUNTIME — ejecutables y DLLs (Windows).
# LIBRARY — bibliotecas compartidas (.so en Linux, .dylib en macOS).
# ARCHIVE — bibliotecas estaticas (.a) y archivos de importacion (.lib).
#
# Si no se especifica DESTINATION, CMake usa valores por defecto
# razonables (bin, lib, etc.), pero es buena practica ser explicito.
```

```cmake
# Incluir las rutas de headers como parte de la interfaz del target:
target_include_directories(mylib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# BUILD_INTERFACE: ruta usada al compilar dentro de este proyecto.
# INSTALL_INTERFACE: ruta usada por proyectos que consuman la
#   biblioteca despues de instalarla. Es relativa a CMAKE_INSTALL_PREFIX.
#
# Estas generator expressions son necesarias porque la ruta de
# los headers es distinta durante el build y despues de instalar.
```

## install(FILES) e install(DIRECTORY)

Para instalar headers y otros archivos que no son targets,
se usan `install(FILES)` e `install(DIRECTORY)`:

```cmake
# install(FILES) — instalar archivos individuales:
install(FILES
    include/mylib/core.h
    include/mylib/utils.h
    DESTINATION include/mylib
)

# Esto copia cada archivo listado al destino.
# Resultado: /usr/local/include/mylib/core.h
#            /usr/local/include/mylib/utils.h
```

```cmake
# install(DIRECTORY) — instalar un directorio completo:
install(DIRECTORY include/
    DESTINATION include
)

# Nota el "/" al final de include/:
#   include/  → copia el CONTENIDO de include/ al destino
#   include   → copia el directorio include/ completo (include/include/...)
#
# Si la estructura es:
#   include/
#   └── mylib/
#       ├── core.h
#       └── utils.h
#
# El resultado con include/ (con barra):
#   /usr/local/include/mylib/core.h    ← correcto
#
# El resultado con include (sin barra):
#   /usr/local/include/include/mylib/core.h   ← probablemente no deseado
```

```cmake
# Diferencia entre FILES y DIRECTORY:
#
# FILES:
#   - Lista archivos uno por uno
#   - Control total sobre que se instala
#   - Si agregas un .h nuevo, debes actualizar CMakeLists.txt
#
# DIRECTORY:
#   - Copia todo el contenido del directorio
#   - Menos mantenimiento si agregas archivos frecuentemente
#   - Puede copiar archivos no deseados (.swp, backups)
#   - Se puede filtrar con PATTERN o FILES_MATCHING:

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"    # solo archivos .h
)

# Esto ignora cualquier archivo que no termine en .h.
```

## CMAKE_INSTALL_PREFIX

`CMAKE_INSTALL_PREFIX` es la ruta base donde `install()` coloca
los archivos. Todos los `DESTINATION` son relativos a esta ruta:

```bash
# Valor por defecto en Linux: /usr/local
# Valor por defecto en Windows: C:/Program Files/${PROJECT_NAME}

# Los DESTINATION en install() se concatenan con CMAKE_INSTALL_PREFIX:
#   DESTINATION bin     → /usr/local/bin
#   DESTINATION lib     → /usr/local/lib
#   DESTINATION include → /usr/local/include
```

```bash
# Override en la configuracion con -D:
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/myapp

# Ahora los destinos seran:
#   DESTINATION bin     → /opt/myapp/bin
#   DESTINATION lib     → /opt/myapp/lib
#   DESTINATION include → /opt/myapp/include
#
# Esto es util para:
# - Instalar en el home del usuario (sin sudo)
# - Mantener versiones separadas de un programa
# - Instalar en un directorio de pruebas
```

```bash
# DESTDIR — staging para empaquetado:
DESTDIR=/tmp/staging cmake --install build

# DESTDIR se antepone a CMAKE_INSTALL_PREFIX sin modificarlo.
# Si CMAKE_INSTALL_PREFIX=/usr/local:
#   Resultado: /tmp/staging/usr/local/bin/my_app
#
# DESTDIR es fundamental para crear paquetes (RPM, DEB):
# 1. El empaquetador instala con DESTDIR en un directorio temporal
# 2. El contenido de ese directorio se empaqueta
# 3. El paquete instala los archivos en las rutas reales
#
# La diferencia con CMAKE_INSTALL_PREFIX:
#   CMAKE_INSTALL_PREFIX cambia la ruta logica (la que el programa
#   usa para buscar sus archivos en runtime).
#   DESTDIR cambia solo donde se copian los archivos, sin afectar
#   las rutas internas del programa.
```

## cmake --install

El comando `cmake --install` ejecuta la fase de instalacion.
Es la forma moderna (CMake 3.15+) de instalar un proyecto:

```bash
# Instalar despues de compilar:
cmake -S . -B build
cmake --build build
cmake --install build

# Salida tipica:
# -- Install configuration: ""
# -- Installing: /usr/local/bin/my_app
# -- Installing: /usr/local/lib/libmylib.so
# -- Installing: /usr/local/include/mylib/core.h
```

```bash
# Override del prefix en el momento de instalar:
cmake --install build --prefix /custom/path

# Esto sobreescribe CMAKE_INSTALL_PREFIX para esta instalacion.
# Util para probar la instalacion sin modificar la configuracion.

# Instalar un solo componente (si se definieron componentes):
cmake --install build --component Runtime
cmake --install build --component Development

# Instalar en modo verbose (ver cada archivo):
cmake --install build --verbose
```

```bash
# Forma vieja (pre-3.15) — sigue funcionando:
cd build
make install

# O equivalentemente:
cmake --build build --target install

# La forma cmake --install es preferible porque es independiente
# del generador (funciona igual con Make, Ninja, etc.).
```

## GNUInstallDirs

El modulo `GNUInstallDirs` define variables estandar para los
directorios de instalacion segun las convenciones GNU. Usar
estas variables hace que el proyecto sea portable entre
distribuciones Linux:

```cmake
include(GNUInstallDirs)

# Esto define las siguientes variables (entre otras):
#   CMAKE_INSTALL_BINDIR      = bin
#   CMAKE_INSTALL_LIBDIR      = lib   (o lib64 en algunas distros)
#   CMAKE_INSTALL_INCLUDEDIR  = include
#   CMAKE_INSTALL_DATADIR     = share
#   CMAKE_INSTALL_MANDIR      = share/man
#   CMAKE_INSTALL_DOCDIR      = share/doc/${PROJECT_NAME}
#
# Tambien define versiones con ruta completa:
#   CMAKE_INSTALL_FULL_BINDIR     = /usr/local/bin
#   CMAKE_INSTALL_FULL_LIBDIR     = /usr/local/lib
#   CMAKE_INSTALL_FULL_INCLUDEDIR = /usr/local/include
```

```cmake
# Usar GNUInstallDirs en install():
include(GNUInstallDirs)

install(TARGETS my_app mylib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# En Fedora/RHEL con arquitectura 64-bit:
#   CMAKE_INSTALL_LIBDIR = lib64
#   Las bibliotecas se instalan en /usr/local/lib64/
#
# En Debian/Ubuntu:
#   CMAKE_INSTALL_LIBDIR = lib/x86_64-linux-gnu  (multiarch)
#
# Sin GNUInstallDirs, si hardcodeas "lib", tu biblioteca
# se instala en /usr/local/lib/ en Fedora, donde el sistema
# busca en /usr/local/lib64/ — y no la encuentra.
```

## install(EXPORT)

`install(EXPORT)` genera archivos CMake que describen los targets
instalados. Esto permite que otros proyectos los encuentren con
`find_package()`:

```cmake
# Paso 1 — instalar los targets CON un export set:
install(TARGETS mylib
    EXPORT MyLibTargets               # nombre del export set
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Paso 2 — instalar el export set como archivo CMake:
install(EXPORT MyLibTargets
    FILE MyLibTargets.cmake
    NAMESPACE MyLib::                   # prefijo para los targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)

# Esto genera e instala un archivo MyLibTargets.cmake que contiene
# la definicion del target MyLib::mylib con todas sus propiedades
# (include dirs, flags, dependencias).
#
# El namespace MyLib:: es una convencion:
#   - Diferencia targets importados de targets locales
#   - Si el target no se encuentra, CMake da error claro
#     (sin namespace, podria fallar silenciosamente)
```

```cmake
# Despues de instalar, otro proyecto puede hacer:
find_package(MyLib REQUIRED)
target_link_libraries(consumer PRIVATE MyLib::mylib)

# find_package busca el archivo MyLibConfig.cmake (o MyLibTargets.cmake)
# en las rutas estandar de CMake, incluyendo:
#   /usr/local/lib/cmake/MyLib/
#   /usr/lib/cmake/MyLib/
#   etc.
```

## Paquete CMake config

Para que `find_package(MyLib)` funcione completamente, se necesitan
dos archivos ademas de `MyLibTargets.cmake`:

```cmake
# MyLibConfig.cmake — punto de entrada que find_package busca.
# MyLibConfigVersion.cmake — informacion de version para find_package.
#
# El flujo completo de find_package(MyLib 1.0 REQUIRED):
#   1. CMake busca MyLibConfig.cmake en rutas estandar
#   2. Lee MyLibConfigVersion.cmake para verificar la version
#   3. Si la version es compatible, ejecuta MyLibConfig.cmake
#   4. MyLibConfig.cmake incluye MyLibTargets.cmake
#   5. Los targets MyLib::mylib quedan disponibles
```

```cmake
# Estructura de archivos instalados para un paquete CMake:
#
#   /usr/local/lib/cmake/MyLib/
#   ├── MyLibConfig.cmake              ← punto de entrada
#   ├── MyLibConfigVersion.cmake       ← verificacion de version
#   ├── MyLibTargets.cmake             ← definicion de targets
#   └── MyLibTargets-release.cmake     ← rutas de artefactos (por config)
```

## configure_package_config_file y write_basic_package_version_file

CMake proporciona helpers en el modulo `CMakePackageConfigHelpers`
para generar los archivos de configuracion del paquete:

```cmake
include(CMakePackageConfigHelpers)

# --- MyLibConfig.cmake ---
# Se genera a partir de un template MyLibConfig.cmake.in:

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyLibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)

# --- MyLibConfigVersion.cmake ---
# Se genera automaticamente con la version del proyecto:

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# COMPATIBILITY define como se comparan versiones:
#   SameMajorVersion — 1.2 es compatible con 1.0, no con 2.0
#   AnyNewerVersion  — cualquier version >= solicitada
#   ExactVersion     — solo la version exacta
#   SameMinorVersion — 1.2.x es compatible con 1.2.0, no con 1.3.0
```

```cmake
# El template MyLibConfig.cmake.in:
# (archivo cmake/MyLibConfig.cmake.in en el proyecto)

@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/MyLibTargets.cmake")

check_required_components(MyLib)

# @PACKAGE_INIT@ se reemplaza por codigo CMake que establece
# las rutas del paquete correctamente.
#
# check_required_components() verifica que todos los componentes
# solicitados en find_package(MyLib COMPONENTS ...) existen.
```

```cmake
# Instalar los archivos de configuracion generados:
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)
```

## Ejemplo completo

Un proyecto que crea una biblioteca, la instala, y otro
proyecto la consume con `find_package()`:

```bash
# Estructura del proyecto de la biblioteca:
#
#   mylib/
#   ├── CMakeLists.txt
#   ├── cmake/
#   │   └── MyLibConfig.cmake.in
#   ├── include/
#   │   └── mylib/
#   │       └── mylib.h
#   └── src/
#       └── mylib.c
```

```c
// include/mylib/mylib.h
#ifndef MYLIB_H
#define MYLIB_H

int mylib_add(int a, int b);
int mylib_mul(int a, int b);

#endif // MYLIB_H
```

```c
// src/mylib.c
#include "mylib/mylib.h"

int mylib_add(int a, int b) {
    return a + b;
}

int mylib_mul(int a, int b) {
    return a * b;
}
```

```cmake
# CMakeLists.txt de la biblioteca
cmake_minimum_required(VERSION 3.20)

project(MyLib
    VERSION 1.0.0
    DESCRIPTION "Example installable library"
    LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# --- Crear la biblioteca ---
add_library(mylib
    src/mylib.c
)

target_include_directories(mylib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(mylib PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# --- Instalar la biblioteca y headers ---
install(TARGETS mylib
    EXPORT MyLibTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# --- Exportar targets ---
install(EXPORT MyLibTargets
    FILE MyLibTargets.cmake
    NAMESPACE MyLib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)

# --- Generar e instalar config files ---
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyLibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MyLibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MyLib
)
```

```cmake
# cmake/MyLibConfig.cmake.in
@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/MyLibTargets.cmake")

check_required_components(MyLib)
```

```bash
# Flujo completo: compilar e instalar la biblioteca

# 1. Configurar (instalacion en un directorio local para pruebas):
cmake -S mylib -B mylib/build \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local

# 2. Compilar:
cmake --build mylib/build

# 3. Instalar:
cmake --install mylib/build

# Resultado en ~/.local/:
#   ~/.local/
#   ├── include/
#   │   └── mylib/
#   │       └── mylib.h
#   ├── lib/
#   │   ├── libmylib.so → libmylib.so.1
#   │   ├── libmylib.so.1 → libmylib.so.1.0.0
#   │   ├── libmylib.so.1.0.0
#   │   └── cmake/
#   │       └── MyLib/
#   │           ├── MyLibConfig.cmake
#   │           ├── MyLibConfigVersion.cmake
#   │           ├── MyLibTargets.cmake
#   │           └── MyLibTargets-noconfig.cmake
#   └── ...
```

```bash
# Ahora crear un proyecto consumidor que use la biblioteca:
#
#   consumer/
#   ├── CMakeLists.txt
#   └── main.c
```

```c
// consumer/main.c
#include <stdio.h>
#include <mylib/mylib.h>

int main(void) {
    printf("3 + 4 = %d\n", mylib_add(3, 4));
    printf("3 * 4 = %d\n", mylib_mul(3, 4));
    return 0;
}
```

```cmake
# consumer/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(consumer LANGUAGES C)

find_package(MyLib 1.0 REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE MyLib::mylib)
```

```bash
# Compilar el consumidor indicando donde buscar el paquete:
cmake -S consumer -B consumer/build \
    -DCMAKE_PREFIX_PATH=$HOME/.local

cmake --build consumer/build

./consumer/build/consumer
# 3 + 4 = 7
# 3 * 4 = 12

# CMAKE_PREFIX_PATH le dice a find_package donde buscar
# paquetes ademas de las rutas del sistema.
# Si la biblioteca se instalo en /usr/local (el default),
# CMAKE_PREFIX_PATH no es necesario porque CMake ya busca ahi.
```

---

## Ejercicios

### Ejercicio 1 — Instalar un ejecutable

```cmake
# Crear un proyecto con un ejecutable "greeter" (greeter.c)
# que imprima un saludo.
#
# El CMakeLists.txt debe:
#   - Usar include(GNUInstallDirs)
#   - Instalar el ejecutable con install(TARGETS) usando
#     CMAKE_INSTALL_BINDIR como destino
#   - Incluir un archivo de datos greeting.txt con install(FILES)
#     en ${CMAKE_INSTALL_DATADIR}/greeter/
#
# Verificar:
#   1. cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/tmp/test-install
#   2. cmake --build build
#   3. cmake --install build
#   4. ls /tmp/test-install/bin/greeter          ← debe existir
#   5. ls /tmp/test-install/share/greeter/greeting.txt  ← debe existir
#   6. /tmp/test-install/bin/greeter             ← debe ejecutarse
#   7. rm -rf /tmp/test-install                  ← limpiar
```

### Ejercicio 2 — Biblioteca instalable con headers

```cmake
# Crear una biblioteca estatica "mathutil" con:
#   include/mathutil/mathutil.h  — declaraciones: int factorial(int n);
#   src/mathutil.c               — implementacion
#
# El CMakeLists.txt debe:
#   - Crear la biblioteca con add_library(mathutil STATIC ...)
#   - Configurar target_include_directories con BUILD_INTERFACE
#     e INSTALL_INTERFACE
#   - Instalar la biblioteca con install(TARGETS) usando GNUInstallDirs
#   - Instalar los headers con install(DIRECTORY include/ ...)
#   - Usar CMAKE_INSTALL_PREFIX=/tmp/test-mathutil para probar
#
# Verificar:
#   1. cmake --install build
#   2. ls /tmp/test-mathutil/lib/libmathutil.a         ← biblioteca
#   3. ls /tmp/test-mathutil/include/mathutil/mathutil.h  ← header
#   4. Crear un programa de prueba que haga:
#        #include <mathutil/mathutil.h>
#        gcc -I/tmp/test-mathutil/include -L/tmp/test-mathutil/lib \
#            -o test test.c -lmathutil
#      Verificar que compila y ejecuta correctamente
```

### Ejercicio 3 — Paquete CMake exportable

```cmake
# Extender el Ejercicio 2 para que find_package(mathutil) funcione.
#
# Agregar al CMakeLists.txt:
#   - install(EXPORT) con namespace MathUtil::
#   - Un template cmake/MathUtilConfig.cmake.in con @PACKAGE_INIT@
#   - configure_package_config_file() y write_basic_package_version_file()
#   - Instalar los config files generados
#
# Crear un proyecto consumidor con:
#   find_package(MathUtil 1.0 REQUIRED)
#   target_link_libraries(app PRIVATE MathUtil::mathutil)
#
# Verificar el flujo completo:
#   1. Compilar e instalar la biblioteca en /tmp/test-mathutil
#   2. Compilar el consumidor con -DCMAKE_PREFIX_PATH=/tmp/test-mathutil
#   3. El consumidor debe compilar y ejecutar sin errores
#   4. Inspeccionar los archivos generados en
#      /tmp/test-mathutil/lib/cmake/MathUtil/ para entender
#      que contiene cada archivo .cmake
```
