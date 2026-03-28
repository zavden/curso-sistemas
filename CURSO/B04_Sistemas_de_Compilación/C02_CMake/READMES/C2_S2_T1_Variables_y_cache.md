# T01 — Variables y cache

## Tipos de variables

CMake tiene tres tipos de variables fundamentales. Entender la
diferencia es critico porque cada tipo tiene un ciclo de vida,
un scope y un mecanismo de persistencia distinto:

```cmake
# 1. Variables normales — existen solo durante la ejecucion de cmake.
#    Se crean con set(), viven en el scope del CMakeLists.txt actual,
#    y desaparecen cuando cmake termina.
set(MY_VAR "hello")
message(STATUS "${MY_VAR}")   # -- hello

# 2. Variables de cache — persisten entre ejecuciones de cmake.
#    Se almacenan en build/CMakeCache.txt. Se crean con set(... CACHE ...)
#    o con -D en la linea de comandos.
set(MY_OPTION "default" CACHE STRING "A configurable option")

# 3. Variables de entorno — se leen del entorno del proceso.
#    Se acceden con $ENV{} y solo existen durante la ejecucion.
message(STATUS "Home: $ENV{HOME}")
```

```cmake
# Las tres pueden coexistir con el mismo nombre.
# Cuando CMake evalua ${VAR}, la prioridad es:
#
#   1. Variable normal (scope actual)
#   2. Variable de cache
#
# Si existe una variable normal llamada MY_VAR, ${MY_VAR} devuelve
# la normal, incluso si tambien hay una variable de cache MY_VAR.
# Esto es un gotcha comun:

set(BUILD_TESTS ON CACHE BOOL "Build tests")   # cache: ON
set(BUILD_TESTS OFF)                             # normal: OFF
message(STATUS "${BUILD_TESTS}")                 # -- OFF  (la normal oculta la cache)

# Las variables de entorno NUNCA se acceden con ${}, solo con $ENV{}:
#   ${HOME}      → variable CMake llamada HOME (probablemente vacia)
#   $ENV{HOME}   → /home/usuario (del entorno)
```

## set() normal

El comando `set()` crea variables en el scope actual. El valor
es siempre un string; multiples argumentos se unen con punto y
coma para formar una lista:

```cmake
# Asignacion basica:
set(PROJECT_AUTHOR "Jane Doe")
set(MAX_RETRIES 5)

# Sin comillas funciona si no hay espacios:
set(VERSION 1)

# Con espacios, las comillas son obligatorias:
set(GREETING "Hello World")

# Variable vacia:
set(EMPTY_VAR "")
set(ALSO_EMPTY)     # sin valor = vacia
```

```cmake
# Multiples argumentos crean una lista (separada por ;):
set(SOURCES main.c utils.c parser.c)
# Internamente: "main.c;utils.c;parser.c"

# Esto es equivalente:
set(SOURCES "main.c;utils.c;parser.c")

# Borrar una variable:
unset(MY_VAR)
# Despues de unset(), ${MY_VAR} se expande a string vacio.
```

```cmake
# Scope de directorio:
# Una variable definida en un CMakeLists.txt es visible en ese archivo
# y en todos los subdirectorios incluidos con add_subdirectory().
# Pero NO es visible hacia arriba — el padre no ve las variables del hijo.

# proyecto/CMakeLists.txt:
set(ROOT_VAR "visible en todo el arbol")
add_subdirectory(lib)
message(STATUS "${LIB_VAR}")   # -- (vacio, lib/ no puede propagar hacia arriba)

# proyecto/lib/CMakeLists.txt:
message(STATUS "${ROOT_VAR}")  # -- visible en todo el arbol  (heredada del padre)
set(LIB_VAR "solo visible aqui")
```

```cmake
# PARENT_SCOPE — propagar una variable al scope padre:
# proyecto/lib/CMakeLists.txt:
set(LIB_VERSION "2.0" PARENT_SCOPE)

# Ahora el CMakeLists.txt padre puede acceder a ${LIB_VERSION}.
# IMPORTANTE: PARENT_SCOPE NO modifica la variable en el scope actual.
# Solo la define en el scope padre:

set(MY_VAR "value" PARENT_SCOPE)
message(STATUS "${MY_VAR}")   # -- (vacio en el scope actual)
# El padre ve "value", pero aqui MY_VAR no se definio localmente.

# Si necesitas el valor en ambos scopes:
set(MY_VAR "value")
set(MY_VAR "value" PARENT_SCOPE)
```

## set() con CACHE

Las variables de cache se almacenan en `CMakeCache.txt` y persisten
entre ejecuciones de `cmake`. Son el mecanismo principal para
configuracion del proyecto:

```cmake
# Sintaxis:
# set(VAR value CACHE TYPE "docstring")

set(MAX_THREADS 4 CACHE STRING "Maximum number of threads")
set(ENABLE_DEBUG OFF CACHE BOOL "Enable debug mode")
set(INSTALL_PREFIX "/usr/local" CACHE PATH "Installation directory")
set(CONFIG_FILE "config.ini" CACHE FILEPATH "Path to config file")
```

```cmake
# Tipos de cache disponibles:
#
# BOOL     — ON/OFF, TRUE/FALSE, 1/0. cmake-gui muestra un checkbox.
# STRING   — texto libre. cmake-gui muestra un campo de texto.
# PATH     — ruta a un directorio. cmake-gui muestra un selector de directorio.
# FILEPATH — ruta a un archivo. cmake-gui muestra un selector de archivo.
# INTERNAL — no se muestra en cmake-gui ni ccmake. Para uso interno.
#            No necesita docstring (aunque se puede poner).

set(BUILD_TIMESTAMP "" CACHE INTERNAL "Timestamp of last configuration")
# INTERNAL es util para guardar valores calculados que el usuario
# no deberia editar.
```

```cmake
# Comportamiento clave: set(... CACHE ...) NO sobreescribe.
# Si la variable ya existe en la cache, set() la ignora:

# Primera ejecucion de cmake:
set(MY_OPTION "default" CACHE STRING "An option")
# CMakeCache.txt: MY_OPTION = "default"

# El usuario ejecuta: cmake -DMY_OPTION="custom" -B build
# CMakeCache.txt: MY_OPTION = "custom"

# Segunda ejecucion (sin -D):
# CMakeLists.txt dice: set(MY_OPTION "default" CACHE STRING "An option")
# Pero "custom" ya esta en la cache → CMake NO lo cambia a "default".
# CMakeCache.txt: MY_OPTION = "custom"  (se mantiene)

# Este comportamiento es intencional: permite que las elecciones del
# usuario persistan sin que el CMakeLists.txt las pise.
```

## FORCE

El modificador `FORCE` obliga a `set()` a sobreescribir el valor
en la cache, sin importar si ya existia:

```cmake
# Sin FORCE — respeta el valor existente:
set(MY_VAR "new_value" CACHE STRING "doc")
# Si MY_VAR ya esta en la cache, no cambia.

# Con FORCE — siempre sobreescribe:
set(MY_VAR "new_value" CACHE STRING "doc" FORCE)
# Aunque MY_VAR exista en la cache, se reemplaza por "new_value".
```

```cmake
# Caso de uso legitimo: valores calculados que DEBEN actualizarse.
# Ejemplo: detectar la version de una dependencia en cada configuracion.

find_program(PYTHON_EXE python3)
if(PYTHON_EXE)
    execute_process(
        COMMAND ${PYTHON_EXE} --version
        OUTPUT_VARIABLE _py_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(DETECTED_PYTHON_VERSION "${_py_version}" CACHE INTERNAL "" FORCE)
endif()
```

```cmake
# ADVERTENCIA: no usar FORCE con opciones configurables por el usuario.
# Esto anula las decisiones del usuario en cada reconfiguracion:

# MAL — el usuario ejecuta cmake -DMAX_THREADS=8 y en la siguiente
# reconfiguracion vuelve a 4:
set(MAX_THREADS 4 CACHE STRING "Max threads" FORCE)

# BIEN — el usuario puede cambiar el valor y este persiste:
set(MAX_THREADS 4 CACHE STRING "Max threads")
```

## option()

El comando `option()` es un atajo para crear variables de cache
de tipo `BOOL`. Esta disenado para features activables/desactivables:

```cmake
# Sintaxis:
# option(VAR "description" [initial_value])

option(MYAPP_ENABLE_TESTS "Build the test suite" ON)
option(MYAPP_BUILD_DOCS "Build documentation" OFF)
option(MYAPP_USE_OPENSSL "Use OpenSSL for encryption" ON)

# Equivalente exacto a:
# set(MYAPP_ENABLE_TESTS ON CACHE BOOL "Build the test suite")
# set(MYAPP_BUILD_DOCS OFF CACHE BOOL "Build documentation")
# set(MYAPP_USE_OPENSSL ON CACHE BOOL "Use OpenSSL for encryption")
```

```cmake
# El tercer argumento (ON/OFF) es el valor por defecto.
# Si se omite, el default es OFF:
option(MYAPP_ENABLE_PROFILING "Enable profiling support")
# Equivale a: option(MYAPP_ENABLE_PROFILING "Enable profiling support" OFF)
```

```cmake
# Uso tipico con condicionales:
option(MYAPP_ENABLE_TESTS "Build the test suite" ON)

if(MYAPP_ENABLE_TESTS)
    add_subdirectory(tests)
    message(STATUS "Tests: enabled")
else()
    message(STATUS "Tests: disabled")
endif()
```

```cmake
# Convenciones de nombre para options:
# Prefijar SIEMPRE con el nombre del proyecto para evitar colisiones
# cuando el proyecto se usa como subdirectorio de otro.

# BIEN — con prefijo del proyecto:
option(MYLIB_ENABLE_TESTS "Build tests for mylib" ON)
option(MYLIB_BUILD_SHARED "Build as shared library" OFF)
option(MYLIB_USE_SIMD "Use SIMD instructions" ON)

# MAL — nombres genericos que colisionan:
option(ENABLE_TESTS "Build tests" ON)         # colisiona con otros proyectos
option(BUILD_SHARED "Build shared" OFF)       # colisiona con BUILD_SHARED_LIBS
option(USE_OPENSSL "Use OpenSSL" ON)          # colisiona si otro proyecto tiene lo mismo

# Patrones comunes:
#   PROJECT_ENABLE_X     — activar feature X
#   PROJECT_BUILD_X      — construir componente X (docs, examples, benchmarks)
#   PROJECT_USE_X        — usar dependencia externa X
#   PROJECT_WITH_X       — alternativa a USE (algunos proyectos la prefieren)
```

## -D en la linea de comandos

El flag `-D` de cmake crea o modifica variables de cache
directamente desde la terminal. Es la forma principal de
configurar un proyecto al compilarlo:

```bash
# Sintaxis basica:
cmake -DVAR=value -S . -B build

# Ejemplos:
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake -DCMAKE_INSTALL_PREFIX=/opt/myapp -S . -B build
cmake -DMYAPP_ENABLE_TESTS=OFF -S . -B build

# Multiples variables:
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DMYAPP_ENABLE_TESTS=ON \
      -DMYAPP_USE_OPENSSL=OFF \
      -S . -B build
```

```bash
# Especificar el tipo explicitamente con :TYPE
cmake -DMAX_THREADS:STRING=8 -S . -B build
cmake -DENABLE_DEBUG:BOOL=ON -S . -B build
cmake -DINSTALL_DIR:PATH=/opt/myapp -S . -B build
cmake -DCONFIG_FILE:FILEPATH=/etc/myapp.conf -S . -B build

# Si no se especifica tipo, CMake infiere:
#   - ON/OFF/TRUE/FALSE/1/0 → BOOL
#   - Todo lo demas → STRING (que no se muestra correctamente en cmake-gui)
# Recomendacion: especificar el tipo cuando se usan cmake-gui o ccmake.
```

```bash
# Prioridad: -D tiene prioridad sobre set() en CMakeLists.txt.
#
# Si CMakeLists.txt dice:
#   set(MY_VAR "default" CACHE STRING "doc")
#
# Y ejecutas:
#   cmake -DMY_VAR="custom" -S . -B build
#
# MY_VAR sera "custom". La razon: -D escribe en la cache ANTES de que
# CMake procese el CMakeLists.txt. Cuando set(... CACHE ...) se ejecuta,
# la variable ya existe en la cache, y sin FORCE, no la sobreescribe.
```

```bash
# Variables de CMake que se configuran comunmente con -D:
cmake -DCMAKE_BUILD_TYPE=Release         # tipo de build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local  # destino de install
cmake -DCMAKE_C_COMPILER=clang          # compilador de C
cmake -DCMAKE_CXX_COMPILER=clang++      # compilador de C++
cmake -DBUILD_SHARED_LIBS=ON            # bibliotecas compartidas
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON # compile_commands.json para LSP
```

## CMakeCache.txt

El archivo `CMakeCache.txt` se genera en el directorio de build
y contiene todas las variables de cache. Es el mecanismo de
persistencia de la configuracion:

```bash
# Ubicacion: siempre en la raiz del directorio de build.
cmake -S . -B build
# Se genera: build/CMakeCache.txt

ls build/CMakeCache.txt
# build/CMakeCache.txt
```

```bash
# Formato del archivo — cada entrada tiene esta estructura:
#
#   // Help string (docstring)
#   VAR:TYPE=VALUE
#
# Ejemplo del contenido real:

# build/CMakeCache.txt (fragmento):
#
# // Build type: Debug, Release, RelWithDebInfo, MinSizeRel
# CMAKE_BUILD_TYPE:STRING=Release
#
# // C compiler
# CMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc
#
# // Flags used by the C compiler
# CMAKE_C_FLAGS:STRING=
#
# // Install path prefix
# CMAKE_INSTALL_PREFIX:PATH=/usr/local
#
# // Build the test suite
# MYAPP_ENABLE_TESTS:BOOL=ON
#
# Las lineas que empiezan con // son comentarios (los help strings).
# Las lineas que empiezan con # son comentarios del usuario.
# Las variables INTERNAL no aparecen en la salida de cmake -L.
```

```bash
# Listar variables de cache desde la terminal:

# Variables basicas (las que el usuario normalmente configura):
cmake -L -B build
# -- Cache values
# CMAKE_BUILD_TYPE:STRING=Release
# CMAKE_INSTALL_PREFIX:PATH=/usr/local
# MYAPP_ENABLE_TESTS:BOOL=ON

# Incluir help strings:
cmake -LH -B build
# -- Cache values
# // Build type
# CMAKE_BUILD_TYPE:STRING=Release
# // Install path prefix
# CMAKE_INSTALL_PREFIX:PATH=/usr/local

# Incluir variables avanzadas (marcadas con mark_as_advanced()):
cmake -LA -B build

# Combinar ambas — avanzadas con help strings:
cmake -LAH -B build
```

```bash
# Editar CMakeCache.txt a mano:
# Es posible pero NO recomendado. Los riesgos:
# 1. Error de sintaxis invalida la cache completa.
# 2. Cambiar variables internas puede romper la configuracion.
# 3. Hay que re-ejecutar cmake despues de editarlo.

# Si necesitas cambiar un valor, es mejor usar -D:
cmake -DMYAPP_ENABLE_TESTS=OFF -B build

# Para resetear toda la configuracion — borrar el directorio de build:
rm -rf build
cmake -S . -B build
# Esto garantiza un estado limpio.

# Alternativa: borrar solo la cache sin borrar los objetos compilados:
rm build/CMakeCache.txt
cmake -S . -B build
# CMake reconfigura desde cero pero reutiliza los .o existentes
# si los flags no cambiaron.
```

## ccmake y cmake-gui

CMake incluye dos interfaces interactivas para editar la cache
sin tener que recordar los nombres de las variables:

```bash
# ccmake — interfaz de terminal (ncurses):
ccmake -B build

# Controles:
#   Flechas     — navegar entre variables
#   Enter       — editar el valor de la variable seleccionada
#   t           — alternar entre vista normal y avanzada
#   c           — configurar (equivale a ejecutar cmake)
#   g           — generar y salir (si la configuracion fue exitosa)
#   q           — salir sin generar
#
# ccmake muestra las variables BOOL como ON/OFF toggleables
# y las PATH/FILEPATH con el path completo editable.
```

```bash
# cmake-gui — interfaz grafica (Qt):
cmake-gui

# Funcionalidad:
# - Selector de directorio fuente y build
# - Lista de variables con tipos: checkbox para BOOL, campo de texto
#   para STRING, boton de browse para PATH/FILEPATH
# - Boton "Configure" y "Generate"
# - Busqueda de variables por nombre
# - Agrupacion por prefijo (CMAKE_, PROJECT_, etc.)
```

```bash
# Instalar ccmake y cmake-gui:

# Fedora:
sudo dnf install cmake-curses-gui    # ccmake
sudo dnf install cmake-gui           # cmake-gui

# Ubuntu/Debian:
sudo apt install cmake-curses-gui    # ccmake
sudo apt install cmake-qt-gui       # cmake-gui

# Arch Linux:
sudo pacman -S cmake                 # ccmake viene incluido
# cmake-gui requiere qt5: instalar cmake con soporte Qt
```

```bash
# Flujo tipico con ccmake:
cmake -S . -B build               # configuracion inicial
ccmake -B build                    # ajustar opciones interactivamente
cmake --build build                # compilar con las opciones elegidas

# ccmake es especialmente util cuando no conoces todas las opciones
# de un proyecto ajeno. Ejecutarlo muestra todas las variables
# configurables con sus help strings.
```

## Scope de variables

Las variables normales en CMake tienen reglas de scope estrictas.
Cada directorio y funcion crea un scope aislado:

```cmake
# Scope de directorio:
# Cada CMakeLists.txt tiene su propio scope.
# add_subdirectory() crea un scope HIJO que HEREDA las variables
# del padre, pero los cambios en el hijo NO se propagan al padre.

# proyecto/CMakeLists.txt (padre):
set(SHARED_VAR "from root")
set(ROOT_ONLY "only in root")
add_subdirectory(lib)
message(STATUS "SHARED_VAR: ${SHARED_VAR}")  # -- from root (no cambio)
message(STATUS "LIB_VAR: ${LIB_VAR}")        # -- (vacio)

# proyecto/lib/CMakeLists.txt (hijo):
message(STATUS "SHARED_VAR: ${SHARED_VAR}")  # -- from root (heredada)
set(SHARED_VAR "modified in lib")            # solo cambia en ESTE scope
set(LIB_VAR "from lib")                     # solo existe en ESTE scope
```

```cmake
# PARENT_SCOPE en add_subdirectory():
# Para que un subdirectorio comunique un valor al padre,
# debe usar PARENT_SCOPE explicitamente.

# proyecto/lib/CMakeLists.txt:
set(LIB_VERSION "3.1.4" PARENT_SCOPE)
set(LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/foo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/bar.c"
    PARENT_SCOPE
)

# proyecto/CMakeLists.txt:
add_subdirectory(lib)
message(STATUS "Lib version: ${LIB_VERSION}")   # -- 3.1.4
message(STATUS "Lib sources: ${LIB_SOURCES}")   # -- /ruta/lib/foo.c;/ruta/lib/bar.c
```

```cmake
# Scope de funcion:
# function() crea su propio scope, similar a add_subdirectory().
# Las variables del scope llamante se heredan, pero las modificaciones
# no se propagan hacia arriba (a menos que se use PARENT_SCOPE).

function(print_info prefix)
    set(MSG "${prefix}: ${PROJECT_NAME} v${PROJECT_VERSION}")
    message(STATUS "${MSG}")
    # MSG solo existe dentro de esta funcion.
endfunction()

print_info("Info")   # -- Info: my_app v1.0.0
message(STATUS "${MSG}")  # -- (vacio — MSG no existe fuera de la funcion)
```

```cmake
# function() vs macro():
# macro() NO crea un scope nuevo. Las variables de una macro
# se definen en el scope del llamante:

macro(set_version major minor)
    set(VERSION_MAJOR ${major})
    set(VERSION_MINOR ${minor})
endmacro()

set_version(2 5)
message(STATUS "${VERSION_MAJOR}.${VERSION_MINOR}")  # -- 2.5
# VERSION_MAJOR y VERSION_MINOR existen en el scope actual
# porque macro() no crea scope propio.

# Consecuencia: si necesitas variables locales, usa function().
# Si quieres modificar variables del llamante sin PARENT_SCOPE, usa macro().
```

## Listas en CMake

En CMake no existe un tipo "lista" separado. Una lista es
simplemente un string donde los elementos estan separados por
punto y coma (`;`):

```cmake
# Estas dos formas son identicas:
set(FRUITS apple orange banana)
set(FRUITS "apple;orange;banana")

# Verificar:
message(STATUS "${FRUITS}")   # -- apple;orange;banana
```

```cmake
# El comando list() opera sobre listas:

set(COLORS red green blue)

# Longitud:
list(LENGTH COLORS count)
message(STATUS "Count: ${count}")   # -- Count: 3

# Agregar elementos:
list(APPEND COLORS yellow purple)
message(STATUS "${COLORS}")   # -- red;green;blue;yellow;purple

# Eliminar elementos:
list(REMOVE_ITEM COLORS green)
message(STATUS "${COLORS}")   # -- red;blue;yellow;purple

# Acceder por indice (0-based):
list(GET COLORS 0 first)
message(STATUS "First: ${first}")   # -- First: red

# Buscar:
list(FIND COLORS blue idx)
message(STATUS "blue at index: ${idx}")   # -- blue at index: 1
# Si no se encuentra, idx = -1.

# Insertar en posicion:
list(INSERT COLORS 1 white)
message(STATUS "${COLORS}")   # -- red;white;blue;yellow;purple

# Ordenar:
list(SORT COLORS)
message(STATUS "${COLORS}")   # -- blue;purple;red;white;yellow

# Eliminar duplicados:
set(NUMS 1 2 3 2 1)
list(REMOVE_DUPLICATES NUMS)
message(STATUS "${NUMS}")   # -- 1;2;3
```

```cmake
# foreach() itera sobre listas:
set(SOURCES main.c utils.c parser.c)

foreach(src IN LISTS SOURCES)
    message(STATUS "Source: ${src}")
endforeach()
# -- Source: main.c
# -- Source: utils.c
# -- Source: parser.c

# Tambien funciona con valores directos:
foreach(lang C CXX Fortran)
    message(STATUS "Language: ${lang}")
endforeach()
```

```cmake
# Gotcha: las comillas afectan como se interpretan las listas.

set(MY_LIST a b c)

# Sin comillas — se expande como tres argumentos separados:
foreach(item ${MY_LIST})
    message(STATUS "${item}")
endforeach()
# -- a
# -- b
# -- c

# Con comillas — es UN SOLO argumento (el string completo):
foreach(item "${MY_LIST}")
    message(STATUS "${item}")
endforeach()
# -- a;b;c   (un solo elemento)
```

```cmake
# Construir una lista incrementalmente:
set(ALL_SOURCES "")
list(APPEND ALL_SOURCES main.c)
list(APPEND ALL_SOURCES utils.c parser.c)
message(STATUS "${ALL_SOURCES}")   # -- main.c;utils.c;parser.c

# Patron comun: agregar archivos condicionalmente
set(SOURCES main.c core.c)
if(MYAPP_ENABLE_LOGGING)
    list(APPEND SOURCES logger.c)
endif()
if(WIN32)
    list(APPEND SOURCES platform_win.c)
else()
    list(APPEND SOURCES platform_unix.c)
endif()
add_executable(myapp ${SOURCES})
```

## Variables de entorno

Las variables de entorno se acceden con `$ENV{}` y se leen
en tiempo de configuracion, no en tiempo de compilacion:

```cmake
# Leer una variable de entorno:
message(STATUS "Home: $ENV{HOME}")
message(STATUS "Path: $ENV{PATH}")
message(STATUS "User: $ENV{USER}")

# Si la variable de entorno no existe, se expande a string vacio:
message(STATUS "Missing: '$ENV{NONEXISTENT}'")   # -- Missing: ''

# Verificar si existe:
if(DEFINED ENV{HOME})
    message(STATUS "HOME esta definida")
endif()
```

```cmake
# Establecer una variable de entorno (solo para el proceso de cmake):
set(ENV{MY_BUILD_VAR} "some_value")

# IMPORTANTE: esto solo afecta al proceso actual de cmake.
# No modifica el shell del usuario. No persiste despues de que cmake termina.
# Los subprocesos lanzados por cmake SI la heredan (execute_process, etc.)
```

```cmake
# Caso de uso comun: usar variables de entorno como defaults.

# El usuario puede exportar MYAPP_PREFIX en su shell:
#   export MYAPP_PREFIX=/opt/myapp
#
# El CMakeLists.txt lo usa como default para una variable de cache:

if(DEFINED ENV{MYAPP_PREFIX})
    set(_default_prefix "$ENV{MYAPP_PREFIX}")
else()
    set(_default_prefix "/usr/local")
endif()
set(MYAPP_INSTALL_PREFIX "${_default_prefix}" CACHE PATH "Install prefix")
```

```cmake
# Las variables de entorno NO se almacenan en la cache.
# Esto significa que si cambias una variable de entorno entre
# ejecuciones de cmake, el cambio NO se detecta automaticamente:

# Primera ejecucion: export CC=gcc → cmake detecta gcc
# Cambio: export CC=clang
# Segunda ejecucion (sin borrar build/): cmake sigue usando gcc
# porque CMAKE_C_COMPILER ya esta en la cache.

# Solucion: borrar build/ o usar -D explicitamente:
#   cmake -DCMAKE_C_COMPILER=clang -B build
```

## Condicionales con variables

CMake tiene un sistema de condicionales rico que funciona
con variables. Entender la evaluacion booleana es fundamental:

```cmake
# Valores falsos en CMake:
#   - String vacio ""
#   - "0"
#   - "OFF" (case insensitive)
#   - "NO" (case insensitive)
#   - "FALSE" (case insensitive)
#   - "NOTFOUND"
#   - Cualquier string que termina en "-NOTFOUND"
#   - Variable no definida
#
# Todo lo demas es verdadero: "1", "ON", "YES", "TRUE", "hello", etc.
```

```cmake
# if(VAR) — evalua el valor de la variable (sin ${}):
set(ENABLE_FEATURE ON)
if(ENABLE_FEATURE)
    message(STATUS "Feature enabled")
endif()

# IMPORTANTE: if() desreferencia automaticamente nombres de variables.
# if(ENABLE_FEATURE) evalua el VALOR de ENABLE_FEATURE.
# if(${ENABLE_FEATURE}) evalua el valor de la variable llamada "ON"
# — probablemente no lo que quieres.
```

```cmake
# if(DEFINED VAR) — verifica si la variable existe (sin importar su valor):
if(DEFINED CMAKE_BUILD_TYPE)
    message(STATUS "Build type is set to: ${CMAKE_BUILD_TYPE}")
else()
    message(STATUS "Build type not specified")
endif()

# Tambien funciona con variables de entorno:
if(DEFINED ENV{CI})
    message(STATUS "Running in CI environment")
endif()
```

```cmake
# Comparaciones de strings:
set(COMPILER_ID "GNU")

if(COMPILER_ID STREQUAL "GNU")
    message(STATUS "Using GCC")
elseif(COMPILER_ID STREQUAL "Clang")
    message(STATUS "Using Clang")
endif()

# Comparacion case-insensitive — no existe directamente.
# Solucion: convertir a mayusculas:
string(TOUPPER "${COMPILER_ID}" _upper)
if(_upper STREQUAL "GNU")
    message(STATUS "GCC detected")
endif()
```

```cmake
# Comparaciones de versiones:
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20")
    message(STATUS "CMake 3.20+ detected")
endif()

# Operadores de version:
#   VERSION_EQUAL
#   VERSION_LESS
#   VERSION_GREATER
#   VERSION_LESS_EQUAL       (CMake 3.7+)
#   VERSION_GREATER_EQUAL    (CMake 3.7+)

set(MY_VERSION "2.1.3")
if(MY_VERSION VERSION_GREATER "1.0")
    message(STATUS "Version ${MY_VERSION} is greater than 1.0")
endif()
```

```cmake
# Operadores logicos:
if(MYAPP_ENABLE_TESTS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug tests enabled")
endif()

if(NOT MYAPP_USE_OPENSSL)
    message(STATUS "Building without OpenSSL")
endif()

if(WIN32 OR APPLE)
    message(STATUS "Not a Linux system")
endif()

# Operadores numericos:
set(THREAD_COUNT 8)
if(THREAD_COUNT GREATER 4)
    message(STATUS "Using more than 4 threads")
endif()
# Tambien: LESS, EQUAL, GREATER_EQUAL, LESS_EQUAL
```

```cmake
# Patron comun: establecer un default si no se especifico:
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
endif()
```

## Buenas practicas

```cmake
# 1. Prefijar variables con el nombre del proyecto.
#    Evita colisiones cuando el proyecto se usa como subdirectorio.

# BIEN:
set(MYLIB_MAX_BUFFER_SIZE 4096)
option(MYLIB_ENABLE_TESTS "Build mylib tests" ON)
option(MYLIB_BUILD_DOCS "Build mylib documentation" OFF)

# MAL:
set(MAX_BUFFER_SIZE 4096)        # nombre generico, colisiona facilmente
option(ENABLE_TESTS "Build tests" ON)   # otro proyecto puede tener lo mismo
```

```cmake
# 2. Usar option() para features on/off en lugar de set(... CACHE BOOL ...).
#    option() es mas legible y su intencion es clara.

# BIEN:
option(MYAPP_ENABLE_LOGGING "Enable runtime logging" ON)

# FUNCIONA PERO ES MENOS CLARO:
set(MYAPP_ENABLE_LOGGING ON CACHE BOOL "Enable runtime logging")
```

```cmake
# 3. No abusar de CACHE. La mayoria de variables internas deben ser normales.
#    Solo usar CACHE para opciones que el usuario necesita configurar.

# BIEN — variable interna, no necesita persistir:
set(_internal_flags "-fPIC")

# MAL — variable interna expuesta innecesariamente en la cache:
set(_internal_flags "-fPIC" CACHE STRING "Internal flags" FORCE)
```

```cmake
# 4. Documentar con help strings descriptivos.
#    Los help strings se muestran en ccmake, cmake-gui y cmake -LH.

# BIEN — explica que hace y que valores acepta:
set(MYAPP_LOG_LEVEL "INFO" CACHE STRING
    "Logging level: TRACE, DEBUG, INFO, WARN, ERROR, FATAL")

# MAL — no ayuda al usuario:
set(MYAPP_LOG_LEVEL "INFO" CACHE STRING "Log level")
```

```cmake
# 5. Usar CACHE con STRINGS para crear menus desplegables en cmake-gui:
set(MYAPP_LOG_LEVEL "INFO" CACHE STRING
    "Logging level: TRACE, DEBUG, INFO, WARN, ERROR, FATAL")
set_property(CACHE MYAPP_LOG_LEVEL PROPERTY STRINGS
    TRACE DEBUG INFO WARN ERROR FATAL)
# cmake-gui muestra un dropdown con las opciones en vez de un campo de texto.
```

```cmake
# 6. Patron completo para un proyecto bien configurado:
cmake_minimum_required(VERSION 3.20)
project(myapp VERSION 1.0.0 LANGUAGES C)

# Opciones del proyecto:
option(MYAPP_ENABLE_TESTS "Build the test suite" ON)
option(MYAPP_BUILD_DOCS "Build API documentation" OFF)
option(MYAPP_USE_OPENSSL "Use OpenSSL for TLS support" OFF)

set(MYAPP_LOG_LEVEL "INFO" CACHE STRING
    "Logging level: TRACE, DEBUG, INFO, WARN, ERROR, FATAL")
set_property(CACHE MYAPP_LOG_LEVEL PROPERTY STRINGS
    TRACE DEBUG INFO WARN ERROR FATAL)

# Default para CMAKE_BUILD_TYPE:
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

# Resumen de configuracion:
message(STATUS "=== ${PROJECT_NAME} v${PROJECT_VERSION} ===")
message(STATUS "Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "Tests:       ${MYAPP_ENABLE_TESTS}")
message(STATUS "Docs:        ${MYAPP_BUILD_DOCS}")
message(STATUS "OpenSSL:     ${MYAPP_USE_OPENSSL}")
message(STATUS "Log level:   ${MYAPP_LOG_LEVEL}")
message(STATUS "Install to:  ${CMAKE_INSTALL_PREFIX}")
```

---

## Ejercicios

### Ejercicio 1 — Variables normales y scope

```cmake
# Crear un proyecto con esta estructura:
#
#   scope_demo/
#   ├── CMakeLists.txt
#   └── lib/
#       └── CMakeLists.txt
#
# En el CMakeLists.txt raiz:
#   1. Definir project(scope_demo LANGUAGES C)
#   2. set(ROOT_VAR "defined in root")
#   3. set(SHARED_VAR "original value")
#   4. add_subdirectory(lib)
#   5. Despues de add_subdirectory(), imprimir ROOT_VAR, SHARED_VAR y LIB_RESULT
#
# En lib/CMakeLists.txt:
#   1. Imprimir ROOT_VAR y SHARED_VAR (verificar que se heredan)
#   2. set(SHARED_VAR "modified in lib") — sin PARENT_SCOPE
#   3. Imprimir SHARED_VAR otra vez (deberia mostrar "modified in lib")
#   4. set(LIB_RESULT "42" PARENT_SCOPE)
#
# Predecir antes de ejecutar:
#   - El ROOT_VAR en el padre despues de add_subdirectory(), cambia?
#   - El SHARED_VAR en el padre, se modifico?
#   - LIB_RESULT es visible en el padre?
#
# Ejecutar con cmake -S . -B build y verificar las predicciones.
# Luego modificar lib/ para que SHARED_VAR SI se propague al padre.
```

### Ejercicio 2 — Cache, option() y -D

```cmake
# Crear un proyecto con un CMakeLists.txt que tenga:
#
#   option(DEMO_ENABLE_FEATURE_A "Enable feature A" ON)
#   option(DEMO_ENABLE_FEATURE_B "Enable feature B" OFF)
#   set(DEMO_MAX_CONNECTIONS 100 CACHE STRING "Max simultaneous connections")
#   set(DEMO_LOG_LEVEL "INFO" CACHE STRING "Log level: DEBUG, INFO, WARN, ERROR")
#
# Imprimir todas las variables con message(STATUS ...).
#
# Verificar:
#   1. cmake -S . -B build
#      → Comprobar los valores por defecto
#   2. cmake -DDEMO_ENABLE_FEATURE_B=ON -S . -B build
#      → Feature B cambio? Y Feature A se mantuvo?
#   3. cmake -S . -B build (sin -D, segunda ejecucion)
#      → Feature B sigue en ON? (deberia, porque esta en la cache)
#   4. cmake -LH -B build
#      → Verificar que las variables aparecen con sus help strings
#   5. rm -rf build && cmake -S . -B build
#      → Todo vuelve a los defaults?
#   6. Abrir build/CMakeCache.txt con un editor y encontrar las variables.
#      Identificar el formato: // help string, VAR:TYPE=VALUE
```

### Ejercicio 3 — Listas y condicionales

```cmake
# Crear un CMakeLists.txt que:
#
# 1. Defina una lista de modulos disponibles:
#    set(ALL_MODULES core network database ui)
#
# 2. Cree una option para cada modulo:
#    option(MYAPP_MODULE_CORE "Enable core module" ON)
#    option(MYAPP_MODULE_NETWORK "Enable network module" ON)
#    option(MYAPP_MODULE_DATABASE "Enable database module" OFF)
#    option(MYAPP_MODULE_UI "Enable ui module" OFF)
#
# 3. Itere sobre ALL_MODULES con foreach y construya una lista
#    ENABLED_MODULES agregando solo los modulos cuya option este ON.
#    Hint: usar string(TOUPPER ...) para construir el nombre de la option
#    y usar if(${option_name}) para evaluarla.
#
# 4. Imprima la lista de modulos habilitados y su cantidad.
#
# 5. Si ENABLED_MODULES esta vacia, emitir FATAL_ERROR
#    "At least one module must be enabled"
#
# Probar con:
#   cmake -S . -B build
#   cmake -DMYAPP_MODULE_DATABASE=ON -S . -B build
#   cmake -DMYAPP_MODULE_CORE=OFF -DMYAPP_MODULE_NETWORK=OFF -S . -B build
#   (el ultimo deberia dar error si database y ui tambien estan OFF)
```
