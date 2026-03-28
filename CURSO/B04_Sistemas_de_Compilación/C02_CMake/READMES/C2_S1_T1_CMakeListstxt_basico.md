# T01 — CMakeLists.txt basico

## Que es CMake

CMake es un **meta-build system**: no compila codigo directamente,
sino que genera archivos de build para otras herramientas (Makefiles
para GNU Make, build.ninja para Ninja, proyectos de Visual Studio,
etc.). Es multiplataforma y se ha convertido en el estandar de facto
para proyectos en C y C++.

```bash
# CMake genera Makefiles (o Ninja, etc.), y luego esas herramientas compilan:
#
#   CMakeLists.txt
#        |
#        v
#   cmake (configuracion)
#        |
#        v
#   Makefile / build.ninja / .sln
#        |
#        v
#   make / ninja / msbuild (compilacion)
#        |
#        v
#   ejecutable / biblioteca
#
# Ventajas sobre Make puro:
# - Detecta el compilador automaticamente (gcc, clang, msvc)
# - Multiplataforma: el mismo CMakeLists.txt funciona en Linux, macOS, Windows
# - Builds fuera del directorio fuente (out-of-source) por defecto
# - Dependencias de headers automaticas (no hay que listarlas a mano)
# - Gestion de dependencias externas (find_package, FetchContent)
```

## Instalar CMake

```bash
# Fedora:
sudo dnf install cmake

# Ubuntu/Debian:
sudo apt install cmake

# Arch Linux:
sudo pacman -S cmake

# Verificar la instalacion:
cmake --version
# cmake version 3.28.1
#
# La version minima recomendada para proyectos modernos es 3.20+.
# Muchas funcionalidades importantes se agregaron en versiones recientes:
#   3.12 — target_link_libraries con OBJECT libraries
#   3.14 — install(FILES) con TYPE
#   3.20 — C17 support nativo, cmake_path()
#   3.21 — PROJECT_IS_TOP_LEVEL
#   3.24 — CMAKE_COLOR_DIAGNOSTICS
#   3.28 — C++20 modules (experimental)
```

```bash
# Verificar que generadores estan disponibles:
cmake --help
# Generators
#   * Unix Makefiles               = Generates standard UNIX makefiles.
#     Ninja                        = Generates build.ninja files.
#     ...
# El asterisco (*) marca el generador por defecto de tu sistema.
# En Linux suele ser "Unix Makefiles". Ninja es mas rapido si lo instalas:
sudo dnf install ninja-build
```

## El archivo CMakeLists.txt

El archivo se llama exactamente `CMakeLists.txt` — case-sensitive.
No `cmakelists.txt`, no `CMakelists.txt`. Este es el nombre que CMake
busca automaticamente al configurar un proyecto:

```cmake
# CMakeLists.txt — el nombre es fijo y obligatorio.
#
# Estructura de un proyecto tipico:
#
#   mi_proyecto/
#   ├── CMakeLists.txt        ← archivo principal (raiz)
#   ├── src/
#   │   ├── main.c
#   │   └── utils.c
#   └── include/
#       └── utils.h
#
# En proyectos grandes hay un CMakeLists.txt por directorio:
#
#   mi_proyecto/
#   ├── CMakeLists.txt        ← raiz: define el proyecto, agrega subdirectorios
#   ├── src/
#   │   ├── CMakeLists.txt    ← define targets de la aplicacion
#   │   └── main.c
#   └── lib/
#       ├── CMakeLists.txt    ← define targets de la biblioteca
#       └── utils.c
#
# El CMakeLists.txt de la raiz es el punto de entrada.
# Los subdirectorios se incluyen con add_subdirectory().
```

## cmake_minimum_required()

La primera linea de todo CMakeLists.txt debe ser
`cmake_minimum_required()`. Establece la version minima de CMake
necesaria para procesar el archivo y activa las politicas de
compatibilidad correspondientes:

```cmake
# Forma basica — version exacta minima:
cmake_minimum_required(VERSION 3.20)

# Si el usuario tiene CMake 3.18, obtiene un error inmediato:
#   CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
#     CMake 3.20 or higher is required.  You are running version 3.18.4
```

```cmake
# Rango de versiones (disponible desde CMake 3.12):
cmake_minimum_required(VERSION 3.20...3.28)

# Esto dice:
#   - Version minima: 3.20 (versiones anteriores fallan)
#   - Version maxima de politicas: 3.28
#
# Las "politicas" (policies) son cambios de comportamiento entre versiones.
# Cada version de CMake puede cambiar como funcionan ciertos comandos.
# Con el rango, CMake activa las politicas hasta 3.28, incluso si
# el usuario tiene CMake 3.30. Esto evita warnings de deprecacion
# futuros y asegura comportamiento consistente.
#
# Sin el rango, cmake_minimum_required(VERSION 3.20) activa las
# politicas solo hasta 3.20 — versiones nuevas pueden emitir warnings
# porque algunas politicas viejas estan deprecadas.
```

```cmake
# Por que es obligatoria:
#
# 1. Sin cmake_minimum_required(), CMake emite un warning de deprecacion
#    y asume politicas muy antiguas (compatibilidad con CMake 2.x).
#
# 2. Las politicas determinan el comportamiento de casi todo.
#    Ejemplo: la politica CMP0048 (CMake 3.0) permite project(VERSION).
#    Sin cmake_minimum_required(VERSION 3.0), project(VERSION) falla.
#
# 3. Debe ser ANTES de project(). CMake lo exige:
#    "CMake Error: ... cmake_minimum_required is not called ..."

# Recomendacion: usar la version de CMake que tengas instalada como minima,
# a menos que necesites soportar sistemas mas antiguos.
```

## project()

La segunda linea define el nombre del proyecto y sus propiedades
globales. Debe ir inmediatamente despues de `cmake_minimum_required()`:

```cmake
# Forma minima:
project(my_app)

# Esto define automaticamente varias variables:
#   PROJECT_NAME       = "my_app"
#   PROJECT_SOURCE_DIR = directorio donde esta este CMakeLists.txt
#   PROJECT_BINARY_DIR = directorio de build correspondiente
#   my_app_SOURCE_DIR  = igual que PROJECT_SOURCE_DIR
#   my_app_BINARY_DIR  = igual que PROJECT_BINARY_DIR
```

```cmake
# Forma completa con todas las opciones:
project(my_app
    VERSION 1.2.3
    DESCRIPTION "A simple example application"
    HOMEPAGE_URL "https://example.com/my_app"
    LANGUAGES C
)

# VERSION descompone en variables:
#   PROJECT_VERSION       = "1.2.3"
#   PROJECT_VERSION_MAJOR = 1
#   PROJECT_VERSION_MINOR = 2
#   PROJECT_VERSION_PATCH = 3
#   PROJECT_VERSION_TWEAK = (no definida, seria el cuarto componente)
#
# LANGUAGES especifica que compiladores necesita CMake:
#   C        — solo C
#   CXX      — solo C++
#   C CXX    — ambos (default si no se especifica LANGUAGES)
#   NONE     — ningun compilador (util para proyectos solo de scripts)
#
# Si se especifica LANGUAGES C, CMake no busca un compilador de C++.
# Esto acelera la configuracion y evita errores si no hay g++ instalado.
```

```cmake
# project() tambien busca y valida el compilador.
# Si LANGUAGES incluye C, CMake busca un compilador de C:
#   1. Variable de entorno CC
#   2. cc en el PATH
#   3. gcc, clang, etc.
#
# Si no encuentra compilador, error fatal:
#   CMake Error: ... No CMAKE_C_COMPILER could be found.
#
# Esto es lo primero que debes verificar si cmake falla en la configuracion.
# Solucion en Fedora: sudo dnf install gcc
```

## add_executable()

Este comando crea un target de tipo ejecutable. Lista el nombre
del target y los archivos fuente que lo componen:

```cmake
# Un solo archivo fuente:
add_executable(my_app main.c)

# Multiples archivos fuente:
add_executable(my_app
    main.c
    utils.c
    parser.c
)
```

```cmake
# NO usar file(GLOB) para listar archivos fuente.
# Esto es un anti-patron conocido:

# MAL — no hagas esto:
file(GLOB SOURCES "src/*.c")
add_executable(my_app ${SOURCES})

# Problemas:
# 1. Si agregas un nuevo .c, CMake NO se reconfigura automaticamente.
#    Hay que ejecutar cmake de nuevo manualmente.
#    (file(GLOB) se evalua solo durante la configuracion.)
#
# 2. Archivos temporales o de prueba en src/ se incluyen sin querer.
#    Un editor que crea main.c~ o #main.c# contamina la compilacion.
#
# 3. Archivos sin commitear en control de versiones se incluyen.
#    Un desarrollador puede tener un .c de prueba local que otro no tiene.
#    El proyecto compila para uno y no para el otro.
#
# file(GLOB) con CONFIGURE_DEPENDS mitiga el punto 1 pero NO los demas:
file(GLOB SOURCES CONFIGURE_DEPENDS "src/*.c")
# CMake reconfigura si cambian los archivos, pero la documentacion
# oficial advierte que no todos los generadores lo soportan bien.

# BIEN — listar archivos explicitamente:
add_executable(my_app
    src/main.c
    src/utils.c
    src/parser.c
)
# Es mas trabajo pero es determinista y predecible.
# Cada archivo nuevo requiere editar CMakeLists.txt — esto es una VENTAJA:
# queda registrado en el control de versiones.
```

```cmake
# Los archivos fuente se buscan relativos al directorio donde
# esta el CMakeLists.txt que contiene el add_executable():
#
#   proyecto/
#   ├── CMakeLists.txt      ← add_executable(my_app src/main.c src/utils.c)
#   └── src/
#       ├── main.c
#       └── utils.c
#
# NO es necesario listar headers (.h) en add_executable().
# CMake detecta automaticamente las dependencias de headers
# al compilar (a diferencia de Make, donde hay que listarlas a mano).
```

## El flujo de CMake — configuracion y compilacion

CMake separa el proceso en dos fases: **configuracion** (generar
los archivos de build) y **compilacion** (ejecutar el build system
generado). Ademas, impone builds fuera del directorio fuente
(out-of-source):

```bash
# Fase 1: configuracion (genera Makefiles u otro build system)
cmake -S . -B build

# -S .       → directorio fuente (source): donde esta CMakeLists.txt
# -B build   → directorio de build: donde se generan los archivos

# Salida tipica:
# -- The C compiler identification is GNU 13.2.1
# -- Detecting C compiler ABI info
# -- Detecting C compiler ABI info - done
# -- Check for working C compiler: /usr/bin/gcc - skipped
# -- Detecting C compile features
# -- Detecting C compile features - done
# -- Configuring done (0.3s)
# -- Generating done (0.0s)
# -- Build files have been written to: /home/user/proyecto/build
```

```bash
# Fase 2: compilacion (ejecuta make/ninja dentro de build/)
cmake --build build

# Esto invoca el build system subyacente:
#   - Si se generaron Makefiles: ejecuta make
#   - Si se genero Ninja: ejecuta ninja
#
# Salida tipica:
# [ 33%] Building C object CMakeFiles/my_app.dir/main.c.o
# [ 66%] Building C object CMakeFiles/my_app.dir/utils.c.o
# [100%] Linking C executable my_app
# [100%] Built target my_app
```

```bash
# Ejecutar el programa:
./build/my_app

# Nota: el ejecutable se genera DENTRO de build/, no en la raiz.
# Esto mantiene el directorio fuente limpio.
```

```bash
# Out-of-source build: el directorio fuente NO se contamina.
# Todo lo generado (Makefiles, .o, ejecutable) esta en build/.
#
# Ventajas:
# 1. git status siempre esta limpio (build/ va en .gitignore)
# 2. Puedes tener multiples builds en paralelo:
#      cmake -S . -B build-debug    -DCMAKE_BUILD_TYPE=Debug
#      cmake -S . -B build-release  -DCMAKE_BUILD_TYPE=Release
# 3. Para "limpiar todo", basta borrar el directorio:
rm -rf build
# Esto es equivalente a make clean pero mas seguro — no puede
# dejar archivos huerfanos.
```

## cmake -S y -B

Los flags `-S` y `-B` son la forma moderna (CMake 3.13+) de
especificar los directorios fuente y build. Reemplazan la
forma vieja que requeria crear el directorio manualmente:

```bash
# Forma moderna (recomendada):
cmake -S . -B build

# Forma vieja (equivalente, pero mas pasos):
mkdir -p build
cd build
cmake ..
# En la forma vieja, cmake recibe el directorio fuente como argumento
# posicional y usa el directorio actual como directorio de build.
```

```bash
# -S puede apuntar a cualquier directorio con CMakeLists.txt:
cmake -S /home/user/projects/mylib -B /tmp/mylib-build

# Si omites -S, CMake usa el directorio actual:
cmake -B build
# Equivale a: cmake -S . -B build
```

```bash
# Reconfiguracion: si ya existe build/, cmake actualiza la configuracion.
# No es necesario borrar build/ para reconfigurar:
cmake -S . -B build                        # primera vez
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release  # reconfigurar

# CMake recuerda opciones previas en build/CMakeCache.txt.
# Si cambias una opcion, solo esa se actualiza; las demas persisten.
# Para empezar desde cero:
rm -rf build
cmake -S . -B build
```

```bash
# Seleccionar el generador explicitamente:
cmake -S . -B build -G "Unix Makefiles"
cmake -S . -B build -G "Ninja"

# Si no se especifica -G, CMake usa el generador por defecto del sistema.
# En Linux suele ser "Unix Makefiles".
# Ninja es mas rapido para compilaciones incrementales en proyectos grandes.
```

```bash
# Argumentos utiles de cmake --build:
cmake --build build                  # compilar
cmake --build build --parallel 4     # compilar con 4 procesos (-j4)
cmake --build build -j$(nproc)       # tantos procesos como nucleos
cmake --build build --target clean   # limpiar (equivale a make clean)
cmake --build build --verbose        # mostrar comandos completos de gcc
```

## Variables automaticas de CMake

CMake define variables que contienen rutas importantes del proyecto.
Es fundamental entender la diferencia entre ellas:

```cmake
# Variables de directorio — se definen automaticamente:

# CMAKE_SOURCE_DIR
#   Directorio raiz del PROYECTO PRINCIPAL (el CMakeLists.txt de mas arriba).
#   Nunca cambia, sin importar en que subdirectorio estes.

# CMAKE_BINARY_DIR
#   Directorio de build raiz (el que pasaste con -B).
#   Equivalente a CMAKE_SOURCE_DIR pero para el build tree.

# CMAKE_CURRENT_SOURCE_DIR
#   Directorio del CMakeLists.txt que se esta procesando AHORA.
#   Cambia al entrar en subdirectorios con add_subdirectory().

# CMAKE_CURRENT_BINARY_DIR
#   Directorio de build correspondiente al CMakeLists.txt actual.

# PROJECT_SOURCE_DIR
#   Directorio del CMakeLists.txt donde se llamo project() mas recientemente.

# PROJECT_BINARY_DIR
#   Directorio de build correspondiente al ultimo project().
```

```cmake
# Diferencia entre CMAKE_SOURCE_DIR y PROJECT_SOURCE_DIR:
#
# En un proyecto simple (un solo CMakeLists.txt), son iguales.
# La diferencia aparece cuando tu proyecto se usa como subdirectorio
# de otro proyecto:
#
#   super_proyecto/
#   ├── CMakeLists.txt         ← project(super)
#   └── libs/
#       └── mi_lib/
#           ├── CMakeLists.txt ← project(mi_lib)
#           └── src/
#
# Dentro de mi_lib/CMakeLists.txt:
#   CMAKE_SOURCE_DIR   = /ruta/super_proyecto    (la raiz absoluta)
#   PROJECT_SOURCE_DIR = /ruta/super_proyecto/libs/mi_lib  (mi project)
#
# Regla: usar PROJECT_SOURCE_DIR en bibliotecas reutilizables.
# CMAKE_SOURCE_DIR asume que tu proyecto es la raiz — se rompe
# si alguien incluye tu proyecto como subdirectorio.
```

```cmake
# Ejemplo practico:
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES C)

message(STATUS "CMAKE_SOURCE_DIR:         ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR:         ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "PROJECT_SOURCE_DIR:       ${PROJECT_SOURCE_DIR}")
message(STATUS "PROJECT_BINARY_DIR:       ${PROJECT_BINARY_DIR}")

# Salida (con cmake -S /home/user/myapp -B /home/user/myapp/build):
# -- CMAKE_SOURCE_DIR:         /home/user/myapp
# -- CMAKE_BINARY_DIR:         /home/user/myapp/build
# -- CMAKE_CURRENT_SOURCE_DIR: /home/user/myapp
# -- PROJECT_SOURCE_DIR:       /home/user/myapp
# -- PROJECT_BINARY_DIR:       /home/user/myapp/build
```

## message()

El comando `message()` imprime mensajes durante la fase de
configuracion (cuando ejecutas `cmake -S . -B build`). Es la
herramienta principal de depuracion en CMake:

```cmake
# Sin modo — mensaje simple:
message("Configuring my_app...")
# Salida: Configuring my_app...

# STATUS — informativo, con prefijo "--":
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
# Salida: -- Compiler: /usr/bin/gcc

# WARNING — advertencia, no detiene la configuracion:
message(WARNING "Feature X is experimental")
# Salida:
# CMake Warning at CMakeLists.txt:5 (message):
#   Feature X is experimental

# FATAL_ERROR — detiene la configuracion inmediatamente:
message(FATAL_ERROR "GCC version too old")
# Salida:
# CMake Error at CMakeLists.txt:7 (message):
#   GCC version too old
#
# -- Configuring incomplete, errors occurred!

# SEND_ERROR — marca error, continua procesando, pero falla al final:
message(SEND_ERROR "Missing dependency X")
message(SEND_ERROR "Missing dependency Y")
# Reporta ambos errores antes de detenerse.

# DEBUG — solo visible con --log-level=debug:
message(DEBUG "Internal value: ${INTERNAL_VAR}")
# No se muestra por defecto. Para verlo:
#   cmake -S . -B build --log-level=debug
```

```cmake
# Uso tipico para depuracion — inspeccionar variables:
message(STATUS "CMAKE_C_COMPILER:  ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_FLAGS:     ${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_BUILD_TYPE:  ${CMAKE_BUILD_TYPE}")
message(STATUS "Sources:           ${SOURCES}")

# Patron para validacion:
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
    set(CMAKE_BUILD_TYPE Release)
endif()
```

## Comentarios

```cmake
# Comentario de una linea — todo despues de # se ignora:
cmake_minimum_required(VERSION 3.20)  # version minima

# Comentario de bloque — desde #[[ hasta ]]:
#[[ Este es un comentario
    que abarca multiples lineas.
    Util para documentar secciones largas
    o deshabilitar codigo temporalmente. ]]

# Los comentarios de bloque se pueden anidar con diferentes marcadores:
#[==[
    Este bloque usa == como delimitador.
    Dentro puedes tener #[[ sin problemas ]].
]==]

# Uso practico — deshabilitar temporalmente un bloque de codigo:
#[[
add_executable(old_test
    test_old.c
    test_utils.c
)
]]
```

## set() y variables

Las variables en CMake se definen con `set()` y se referencian
con `${}`. Todas las variables son strings internamente:

```cmake
# Definir una variable:
set(MY_VAR "hello")

# Referenciar una variable:
message(STATUS "MY_VAR = ${MY_VAR}")
# -- MY_VAR = hello

# Las comillas son opcionales si el valor no tiene espacios:
set(VERSION 1)
set(NAME my_app)

# Pero son NECESARIAS si el valor tiene espacios o esta vacio:
set(DESCRIPTION "A simple application")
set(EMPTY_VAR "")
```

```cmake
# Variables no definidas se expanden a string vacio:
message(STATUS "Undefined: '${NO_EXISTE}'")
# -- Undefined: ''

# Esto NO es un error — es un gotcha comun.
# Para verificar si una variable esta definida:
if(DEFINED MY_VAR)
    message(STATUS "MY_VAR esta definida")
endif()

if(NOT DEFINED MY_VAR)
    message(STATUS "MY_VAR no esta definida")
endif()
```

```cmake
# Listas en CMake son strings separados por punto y coma (;):
set(SOURCES main.c utils.c parser.c)
# Internamente: "main.c;utils.c;parser.c"

# Estas dos formas son equivalentes:
set(SOURCES main.c utils.c parser.c)
set(SOURCES "main.c;utils.c;parser.c")

# Cuando una variable de lista se usa sin comillas, CMake la expande
# como argumentos separados:
add_executable(my_app ${SOURCES})
# Equivale a: add_executable(my_app main.c utils.c parser.c)

# Con comillas, se pasa como UN SOLO argumento:
message(STATUS "${SOURCES}")
# -- main.c;utils.c;parser.c

# Agregar a una lista:
list(APPEND SOURCES extra.c)
# Ahora: "main.c;utils.c;parser.c;extra.c"
```

```cmake
# Variables CACHE — persistentes entre ejecuciones de cmake.
# Se almacenan en build/CMakeCache.txt:
set(MY_OPTION "default_value" CACHE STRING "Description of this option")

# El tipo puede ser: STRING, BOOL, PATH, FILEPATH, INTERNAL
# Las variables CACHE se establecen la primera vez y NO se sobreescriben
# en ejecuciones posteriores (a menos que se use FORCE):
set(MY_OPTION "new_value" CACHE STRING "..." FORCE)

# Las variables -D en la linea de comandos son variables CACHE:
#   cmake -S . -B build -DMY_OPTION=custom_value
# Esto escribe MY_OPTION en CMakeCache.txt.

# option() es un atajo para CACHE BOOL:
option(ENABLE_TESTS "Build unit tests" ON)
# Equivale a: set(ENABLE_TESTS ON CACHE BOOL "Build unit tests")
```

## CMAKE_C_STANDARD y CMAKE_C_STANDARD_REQUIRED

Para especificar el estandar de C que debe usar el compilador,
CMake ofrece variables dedicadas en lugar de flags manuales:

```cmake
# Establecer el estandar de C:
set(CMAKE_C_STANDARD 11)

# Valores posibles: 90, 99, 11, 17, 23
#   90 → C90/ANSI C (-std=c90)
#   99 → C99       (-std=c99)
#   11 → C11       (-std=c11)
#   17 → C17       (-std=c17)
#   23 → C23       (-std=c23, requiere CMake 3.21+ y GCC 14+/Clang 18+)
```

```cmake
# CMAKE_C_STANDARD_REQUIRED — hacer el estandar obligatorio:
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Con REQUIRED ON:
#   Si el compilador no soporta C17, CMake da error fatal.
#
# Con REQUIRED OFF (default):
#   CMake intenta C17, pero si no esta disponible, baja a C11, C99...
#   Esto puede compilar tu codigo con un estandar inferior SIN avisarte.
#   El codigo puede compilar pero comportarse de forma inesperada.
#
# Recomendacion: siempre usar REQUIRED ON.
```

```cmake
# CMAKE_C_EXTENSIONS — controla extensiones del compilador:
set(CMAKE_C_EXTENSIONS OFF)

# Con EXTENSIONS OFF: usa -std=c17 (estandar estricto)
# Con EXTENSIONS ON:  usa -std=gnu17 (estandar + extensiones GNU)
#
# Las extensiones GNU incluyen cosas como typeof, __attribute__,
# expresiones en sentencias ({...}), etc.
# Para codigo portable, usar EXTENSIONS OFF.
```

```cmake
# Estas variables se aplican a TODOS los targets del proyecto.
# Para aplicar a un target especifico, usar propiedades:
add_executable(my_app main.c)
set_target_properties(my_app PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# O con la forma moderna target_compile_features():
target_compile_features(my_app PRIVATE c_std_17)
# Esto es equivalente y ademas se propaga con PUBLIC/INTERFACE.
```

## Flags de compilacion

CMake tiene dos formas de agregar flags de compilacion: la forma
moderna (por target) y la forma legacy (global). La forma moderna
es la recomendada:

```cmake
# FORMA MODERNA — target_compile_options() (recomendada):
add_executable(my_app main.c utils.c)
target_compile_options(my_app PRIVATE -Wall -Wextra -Wpedantic)

# PRIVATE: los flags se aplican solo a este target.
# PUBLIC:  se aplican a este target Y a los que lo enlazan.
# INTERFACE: solo a los que lo enlazan, no a este target.
#
# Para un ejecutable, PRIVATE es casi siempre lo correcto.
# PUBLIC/INTERFACE son utiles para bibliotecas.
```

```cmake
# FORMA LEGACY — CMAKE_C_FLAGS (global):
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

# Problemas:
# 1. Afecta a TODOS los targets del proyecto.
# 2. Contamina targets que no necesitan esos flags.
# 3. Los flags se concatenan como strings — facil cometer errores.
# 4. No se propaga correctamente a subdirectorios en todos los casos.
#
# Unica justificacion: configurar flags globales al inicio de un
# proyecto simple donde todos los targets necesitan los mismos flags.
```

```cmake
# Flags condicionales con generator expressions:
target_compile_options(my_app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
)

# $<$<CONFIG:Debug>:-g -O0> agrega -g -O0 solo en modo Debug.
# Esto es mas avanzado; por ahora basta con la forma simple.
```

```cmake
# Flags tipicos para desarrollo en C:
target_compile_options(my_app PRIVATE
    -Wall              # warnings basicos
    -Wextra            # warnings adicionales
    -Wpedantic         # cumplimiento estricto del estandar
    -Wshadow           # variable oculta otra del scope superior
    -Wconversion       # conversiones implicitas que pueden perder datos
    -Wstrict-prototypes  # prototipos sin parametros explicitos
)
```

## Ejemplo completo

Un proyecto C minimo con CMake:

```c
// utils.h
#ifndef UTILS_H
#define UTILS_H

void greet(const char *name);
int add(int a, int b);

#endif // UTILS_H
```

```c
// utils.c
#include <stdio.h>
#include "utils.h"

void greet(const char *name) {
    printf("Hello, %s!\n", name);
}

int add(int a, int b) {
    return a + b;
}
```

```c
// main.c
#include <stdio.h>
#include "utils.h"

int main(void) {
    greet("CMake");
    printf("3 + 4 = %d\n", add(3, 4));
    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(my_app
    VERSION 1.0.0
    DESCRIPTION "Example CMake project"
    LANGUAGES C
)

# Estandar de C
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Crear el ejecutable
add_executable(my_app
    main.c
    utils.c
)

# Warnings de compilacion
target_compile_options(my_app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

message(STATUS "Project:  ${PROJECT_NAME}")
message(STATUS "Version:  ${PROJECT_VERSION}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Standard: C${CMAKE_C_STANDARD}")
```

```bash
# Sesion completa de trabajo:

# Estructura del proyecto:
# mi_proyecto/
# ├── CMakeLists.txt
# ├── main.c
# ├── utils.c
# └── utils.h

# 1. Configurar (genera Makefiles en build/):
cmake -S . -B build
# -- The C compiler identification is GNU 13.2.1
# -- Detecting C compiler ABI info - done
# -- Check for working C compiler: /usr/bin/gcc - skipped
# -- Detecting C compile features - done
# -- Project:  my_app
# -- Version:  1.0.0
# -- Compiler: /usr/bin/gcc
# -- Standard: C17
# -- Configuring done (0.3s)
# -- Generating done (0.0s)
# -- Build files have been written to: /home/user/mi_proyecto/build

# 2. Compilar:
cmake --build build
# [ 33%] Building C object CMakeFiles/my_app.dir/main.c.o
# [ 66%] Building C object CMakeFiles/my_app.dir/utils.c.o
# [100%] Linking C executable my_app
# [100%] Built target my_app

# 3. Ejecutar:
./build/my_app
# Hello, CMake!
# 3 + 4 = 7

# 4. Modificar utils.c y recompilar:
cmake --build build
# [ 33%] Building C object CMakeFiles/my_app.dir/utils.c.o
# [100%] Linking C executable my_app
# [100%] Built target my_app
# Solo recompila lo que cambio (como Make, pero automatico).

# 5. Compilacion con verbose (ver comandos exactos de gcc):
cmake --build build --verbose
# /usr/bin/gcc -std=c17 -Wall -Wextra -Wpedantic -o ...

# 6. Limpiar todo:
rm -rf build
# Directorio fuente intacto — el .gitignore deberia tener "build/".

# 7. Build con Ninja (si esta instalado):
cmake -S . -B build -G Ninja
cmake --build build
# Mismo resultado, pero Ninja es mas rapido en compilaciones incrementales.
```

## Comparacion con Make

```
+-----------------------------------+---------------------+---------------------+
| Caracteristica                    | Make                | CMake               |
+-----------------------------------+---------------------+---------------------+
| Tipo                              | Build system        | Meta-build system   |
|                                   | (compila directo)   | (genera Makefiles)  |
+-----------------------------------+---------------------+---------------------+
| Multiplataforma                   | Solo Unix           | Linux, macOS,       |
|                                   | (GNU Make)          | Windows, etc.       |
+-----------------------------------+---------------------+---------------------+
| Deteccion de compilador           | Manual              | Automatica          |
|                                   | (CC = gcc)          | (project(LANGUAGES))|
+-----------------------------------+---------------------+---------------------+
| Dependencias de headers           | Manuales            | Automaticas         |
|                                   | (listar .h en       | (CMake genera -MMD  |
|                                   |  prerequisites)     |  internamente)      |
+-----------------------------------+---------------------+---------------------+
| Out-of-source builds              | Requiere esfuerzo   | Nativo              |
|                                   | manual              | (cmake -B build)    |
+-----------------------------------+---------------------+---------------------+
| Multiples configuraciones         | Dificil             | Natural             |
|                                   |                     | (build-debug,       |
|                                   |                     |  build-release)     |
+-----------------------------------+---------------------+---------------------+
| Estandar de lenguaje              | Flag manual         | set(CMAKE_C_STANDARD|
|                                   | (-std=c17)          |  17)                |
+-----------------------------------+---------------------+---------------------+
| Dependencias externas             | Manual              | find_package(),     |
|                                   | (pkg-config, etc.)  | FetchContent        |
+-----------------------------------+---------------------+---------------------+
| Curva de aprendizaje              | Menor               | Mayor               |
+-----------------------------------+---------------------+---------------------+
| Flexibilidad / control bajo nivel | Total               | Limitada al API     |
|                                   |                     | de CMake            |
+-----------------------------------+---------------------+---------------------+
```

```cmake
# En resumen: CMake agrega una capa de abstraccion sobre Make.
# Para proyectos pequenos (1-5 archivos), Make directo es mas simple.
# Para proyectos medianos/grandes, o multiplataforma, CMake escala mejor.
# En la industria, la mayoria de proyectos C/C++ usan CMake.
```

---

## Ejercicios

### Ejercicio 1 — CMakeLists.txt minimo

```cmake
# Crear un proyecto con:
#   calculator.h  — declaraciones: int add(int, int); int sub(int, int);
#   calculator.c  — implementacion de ambas funciones
#   main.c        — pide dos numeros por stdin, imprime suma y resta
#
# Escribir un CMakeLists.txt que:
#   - Requiera CMake 3.20 minimo
#   - Defina el proyecto con nombre "calculator", version 1.0, LANGUAGES C
#   - Establezca C17 como estandar con REQUIRED ON y EXTENSIONS OFF
#   - Cree el ejecutable listando los archivos fuente explicitamente
#   - Agregue -Wall -Wextra -Wpedantic con target_compile_options()
#   - Imprima con message(STATUS ...) el compilador y el estandar
#
# Verificar:
#   1. cmake -S . -B build        → configuracion exitosa
#   2. cmake --build build        → compila sin warnings
#   3. ./build/calculator          → funciona correctamente
#   4. rm -rf build               → limpia todo sin tocar el fuente
#   5. cmake -S . -B build --verbose  → ver los flags de compilacion
```

### Ejercicio 2 — Variables y message()

```cmake
# Usando el proyecto del Ejercicio 1, agregar al CMakeLists.txt:
#
# 1. Variables con set():
#    - APP_AUTHOR con tu nombre
#    - APP_LICENSE con "MIT"
#    - Imprimir ambas con message(STATUS ...)
#
# 2. Explorar variables automaticas:
#    - Imprimir CMAKE_SOURCE_DIR, CMAKE_BINARY_DIR,
#      CMAKE_CURRENT_SOURCE_DIR, PROJECT_SOURCE_DIR
#    - Verificar que todas apuntan donde esperas
#
# 3. Probar message() con distintos modos:
#    - Un message(STATUS ...) informativo
#    - Un message(WARNING ...) con un texto de advertencia
#    - Un message(FATAL_ERROR ...) dentro de un if() que NUNCA se cumpla
#    - Luego cambiar la condicion para que se cumpla y ver el error
#
# 4. Probar una variable CACHE:
#    - set(GREETING "Hello" CACHE STRING "Greeting message")
#    - Ejecutar cmake -S . -B build
#    - Luego: cmake -S . -B build -DGREETING="Hola"
#    - Verificar que el valor cambio
```

### Ejercicio 3 — Comparacion practica con Make

```cmake
# Crear el mismo proyecto (calculator) con un Makefile manual
# y con un CMakeLists.txt. Comparar:
#
# 1. Con Make:
#    - Escribir un Makefile con compilacion separada (calculator.o, main.o)
#    - Listar los .h como prerequisites de cada .o
#    - Compilar con make
#
# 2. Con CMake:
#    - Usar el CMakeLists.txt del Ejercicio 1
#    - Compilar con cmake -S . -B build && cmake --build build
#
# Comparar:
#   a) Agregar un nuevo header (config.h) que main.c incluye.
#      - En Make: si olvidas agregarlo a los prerequisites, touch config.h
#        NO recompila main.o (bug silencioso). Verificar.
#      - En CMake: touch config.h SI recompila main.o automaticamente.
#        CMake detecto la dependencia al compilar. Verificar.
#
#   b) Cambiar el compilador a clang:
#      - En Make: make CC=clang
#      - En CMake: cmake -S . -B build-clang -DCMAKE_C_COMPILER=clang
#        (necesita un directorio de build separado)
#
#   c) Ver el tamano de los archivos generados:
#      - ls -la en ambos directorios de build
#      - CMake genera mas archivos auxiliares (CMakeCache.txt, etc.)
#        pero el ejecutable final es el mismo
```
