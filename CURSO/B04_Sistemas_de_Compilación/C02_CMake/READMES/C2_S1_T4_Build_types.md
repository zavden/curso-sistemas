# T04 --- Build types

## Que es un build type

Un build type es una configuracion predefinida que determina los flags
de compilacion: nivel de optimizacion, inclusion de informacion de
depuracion y estado de macros como `NDEBUG`. CMake controla esto
mediante la variable `CMAKE_BUILD_TYPE`:

```cmake
# CMAKE_BUILD_TYPE acepta un string.
# Los 4 valores estandar son: Debug, Release, RelWithDebInfo, MinSizeRel.
# CMake traduce cada valor a un conjunto de flags de compilador.

set(CMAKE_BUILD_TYPE Debug)
```

```bash
# Lo mas comun es establecerlo desde la linea de comandos:
cmake -DCMAKE_BUILD_TYPE=Release -B build
```

El build type afecta tres cosas fundamentales:

1. **Flags de optimizacion** --- `-O0`, `-O2`, `-O3`, `-Os`
2. **Informacion de depuracion** --- presencia o ausencia de `-g`
3. **Macro NDEBUG** --- si se define `-DNDEBUG`, `assert()` se desactiva

## Los 4 build types estandar

### Debug

Compilacion sin optimizacion, con informacion de depuracion completa.
Ideal para desarrollo y depuracion con GDB/LLDB:

```bash
# Flags que CMake agrega en Debug (GCC/Clang):
#   -g       informacion de depuracion completa
#   -O0      sin optimizacion (el codigo generado sigue el fuente linea a linea)
#
# NDEBUG NO se define → assert() esta ACTIVO.
# Los executables son mas grandes y lentos, pero faciles de depurar.

cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug
```

### Release

Compilacion con maxima optimizacion, sin informacion de depuracion.
Para distribucion y produccion:

```bash
# Flags que CMake agrega en Release (GCC/Clang):
#   -O3       optimizacion agresiva (inlining, vectorizacion, loop unrolling)
#   -DNDEBUG  desactiva assert()
#
# No incluye -g → sin informacion de depuracion.
# Executables pequenos y rapidos, pero imposibles de depurar con GDB.

cmake -DCMAKE_BUILD_TYPE=Release -B build-release
```

### RelWithDebInfo

Release con informacion de depuracion. Para profiling y diagnostico
de problemas en produccion:

```bash
# Flags que CMake agrega en RelWithDebInfo (GCC/Clang):
#   -O2       optimizacion alta (menos agresiva que -O3)
#   -g        informacion de depuracion
#   -DNDEBUG  desactiva assert()
#
# Permite usar GDB, perf, valgrind sobre codigo optimizado.
# El debugger puede saltar lineas o mostrar variables como <optimized out>
# porque el compilador reordena el codigo, pero la informacion basica
# de funciones, callstacks y lineas esta presente.

cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build-reldbg
```

### MinSizeRel

Optimizacion por tamano minimo. Para firmware, sistemas embebidos o
aplicaciones donde el tamano del binario importa:

```bash
# Flags que CMake agrega en MinSizeRel (GCC/Clang):
#   -Os       optimizar por tamano (habilita las optimizaciones de -O2
#             que no aumentan tamano, y agrega optimizaciones de reduccion)
#   -DNDEBUG  desactiva assert()
#
# No incluye -g → sin informacion de depuracion.

cmake -DCMAKE_BUILD_TYPE=MinSizeRel -B build-minsize
```

## Resumen de flags por tipo

```
Build type       Optimizacion   Debug info   NDEBUG   Uso tipico
-----------------------------------------------------------------------
Debug            -O0            -g           no       Desarrollo, GDB
Release          -O3            (no)         si       Produccion
RelWithDebInfo   -O2            -g           si       Profiling, diagnostico
MinSizeRel       -Os            (no)         si       Embebidos, tamano critico
```

## Variables de flags por tipo

CMake almacena los flags de cada build type en variables separadas.
Estas variables se pueden inspeccionar y modificar:

```cmake
# Para C:
#   CMAKE_C_FLAGS              — flags comunes a todos los build types
#   CMAKE_C_FLAGS_DEBUG        — flags adicionales para Debug
#   CMAKE_C_FLAGS_RELEASE      — flags adicionales para Release
#   CMAKE_C_FLAGS_RELWITHDEBINFO — flags adicionales para RelWithDebInfo
#   CMAKE_C_FLAGS_MINSIZEREL   — flags adicionales para MinSizeRel
#
# Para C++:
#   CMAKE_CXX_FLAGS
#   CMAKE_CXX_FLAGS_DEBUG
#   CMAKE_CXX_FLAGS_RELEASE
#   ... (misma estructura)
#
# El comando final de compilacion es:
#   ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_<BUILD_TYPE>}
# Por ejemplo, en Debug:
#   ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}
```

```cmake
# Ver los valores por defecto:
cmake_minimum_required(VERSION 3.20)
project(flags_demo C)

message(STATUS "C flags (comunes):        ${CMAKE_C_FLAGS}")
message(STATUS "C flags (Debug):          ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "C flags (Release):        ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "C flags (RelWithDebInfo): ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "C flags (MinSizeRel):     ${CMAKE_C_FLAGS_MINSIZEREL}")
message(STATUS "Build type actual:        ${CMAKE_BUILD_TYPE}")
```

```bash
# Ejecutar para ver los flags:
cmake -DCMAKE_BUILD_TYPE=Debug -B build
# -- C flags (comunes):
# -- C flags (Debug):          -g
# -- C flags (Release):        -O3 -DNDEBUG
# -- C flags (RelWithDebInfo): -O2 -g -DNDEBUG
# -- C flags (MinSizeRel):     -Os -DNDEBUG
# -- Build type actual:        Debug
```

```bash
# Para ver los flags REALES que el compilador recibe, compilar con verbose:
cmake --build build --verbose
# [1/2] /usr/bin/gcc -g -o CMakeFiles/app.dir/main.c.o -c /path/main.c
#                     ^^
#                     flags de Debug
```

## Establecer el build type

### Desde la linea de comandos

```bash
# La forma estandar: -D define una variable de cache.
cmake -DCMAKE_BUILD_TYPE=Release -B build

# Es case-sensitive en el valor:
cmake -DCMAKE_BUILD_TYPE=release -B build    # NO funciona correctamente
cmake -DCMAKE_BUILD_TYPE=Release -B build    # correcto
cmake -DCMAKE_BUILD_TYPE=RELEASE -B build    # NO funciona correctamente
```

### Como default en CMakeLists.txt

```cmake
# Si el usuario no especifica un build type, CMAKE_BUILD_TYPE esta vacio.
# Esto es un error comun: el compilador no recibe flags de optimizacion
# NI de depuracion. Es peor que cualquiera de los 4 tipos estandar.
#
# Solucion: definir un default al inicio del CMakeLists.txt.

cmake_minimum_required(VERSION 3.20)
project(my_app C)

# Establecer un default si el usuario no especifico nada:
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(app main.c)
```

```cmake
# Version mas robusta con lista de valores validos:
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    # Mostrar las opciones validas en cmake-gui y ccmake:
    set_property(CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# CMAKE_CONFIGURATION_TYPES se comprueba porque en generadores multi-config
# (Visual Studio, Ninja Multi-Config) no se usa CMAKE_BUILD_TYPE.
```

## El problema del build type vacio

Si no se define `CMAKE_BUILD_TYPE` y el CMakeLists.txt no establece
un default, la variable queda vacia. Esto significa que CMake no
agrega ninguna flag de optimizacion ni de depuracion:

```bash
# Sin build type:
cmake -B build
cmake --build build --verbose
# gcc -o CMakeFiles/app.dir/main.c.o -c main.c
# Sin -O, sin -g, sin -DNDEBUG.
# El compilador usa su default: generalmente -O0 sin -g.
# Resultado: codigo lento como Debug pero SIN informacion de depuracion.
# Lo peor de ambos mundos.
```

```bash
# Con build type Release:
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build --verbose
# gcc -O3 -DNDEBUG -o CMakeFiles/app.dir/main.c.o -c main.c
# Ahora si hay optimizacion.
```

## Impacto en el codigo: NDEBUG y assert()

La macro `NDEBUG` controla el comportamiento de `assert()` de la
biblioteca estandar. Los build types Release, RelWithDebInfo y
MinSizeRel definen `-DNDEBUG`, lo que desactiva todos los asserts:

```c
// main.c
#include <stdio.h>
#include <assert.h>

int divide(int a, int b) {
    // assert verifica la condicion en tiempo de ejecucion.
    // Si b == 0, el programa aborta con un mensaje de error.
    // En Release (-DNDEBUG), esta linea se elimina por completo.
    assert(b != 0 && "Division by zero");
    return a / b;
}

int main(void) {
    printf("10 / 2 = %d\n", divide(10, 2));

#ifdef NDEBUG
    printf("NDEBUG esta definido: asserts desactivados.\n");
#else
    printf("NDEBUG NO esta definido: asserts activos.\n");
#endif

    // En Debug, esto aborta el programa con un mensaje claro.
    // En Release, esto causa undefined behavior (division por cero).
    printf("10 / 0 = %d\n", divide(10, 0));

    return 0;
}
```

```bash
# Compilar en Debug:
cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug
cmake --build build-debug
./build-debug/app
# 10 / 2 = 5
# NDEBUG NO esta definido: asserts activos.
# app: main.c:7: divide: Assertion `b != 0 && "Division by zero"' failed.
# Aborted (core dumped)

# Compilar en Release:
cmake -DCMAKE_BUILD_TYPE=Release -B build-release
cmake --build build-release
./build-release/app
# 10 / 2 = 5
# NDEBUG esta definido: asserts desactivados.
# (undefined behavior — puede imprimir basura, crashear, o lo que sea)
```

```c
// Patron comun: logging condicional basado en el build type.
// Usar #ifndef NDEBUG para codigo que solo existe en Debug:

#include <stdio.h>

#ifndef NDEBUG
#define DEBUG_LOG(fmt, ...) \
    fprintf(stderr, "[DEBUG %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) ((void)0)
#endif

void process_data(const int *data, int n) {
    DEBUG_LOG("process_data called with n=%d", n);
    for (int i = 0; i < n; i++) {
        DEBUG_LOG("  data[%d] = %d", i, data[i]);
        // ... procesar ...
    }
}
```

## Impacto en tamano y rendimiento

Los diferentes build types producen binarios significativamente
distintos en tamano y velocidad:

```c
// bench.c --- programa para demostrar la diferencia
#include <stdio.h>
#include <assert.h>

int fibonacci(int n) {
    assert(n >= 0);
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(void) {
    int result = 0;
    for (int i = 0; i < 35; i++) {
        result += fibonacci(i);
    }
    printf("Result: %d\n", result);
    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(bench C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(bench bench.c)
```

```bash
# Compilar en los 4 modos y comparar:

cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug
cmake -DCMAKE_BUILD_TYPE=Release -B build-release
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build-reldbg
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -B build-minsize

cmake --build build-debug
cmake --build build-release
cmake --build build-reldbg
cmake --build build-minsize
```

```bash
# Comparar tamanos:
ls -lh build-debug/bench build-release/bench build-reldbg/bench build-minsize/bench
# -rwxr-xr-x 1 user user  22K  build-debug/bench
# -rwxr-xr-x 1 user user  17K  build-release/bench
# -rwxr-xr-x 1 user user  21K  build-reldbg/bench
# -rwxr-xr-x 1 user user  17K  build-minsize/bench
# Debug es mas grande por la informacion de depuracion.
# RelWithDebInfo tambien incluye -g, por eso es mas grande que Release.
```

```bash
# Comparar tiempos de ejecucion:
time ./build-debug/bench
# real    0m0.250s   (sin optimizacion, mucho mas lento)

time ./build-release/bench
# real    0m0.050s   (optimizado, mucho mas rapido)

time ./build-reldbg/bench
# real    0m0.060s   (-O2, ligeramente mas lento que -O3)

time ./build-minsize/bench
# real    0m0.065s   (-Os, optimiza tamano pero el rendimiento es similar a -O2)
```

```bash
# Verificar informacion de depuracion con file:
file build-debug/bench
# build-debug/bench: ELF 64-bit LSB pie executable, ..., with debug_info, not stripped

file build-release/bench
# build-release/bench: ELF 64-bit LSB pie executable, ..., not stripped
# (sin "with debug_info")

# strip elimina simbolos y debug info del binario:
cp build-reldbg/bench bench-stripped
strip bench-stripped
ls -lh build-reldbg/bench bench-stripped
# build-reldbg/bench:  21K  (con debug info)
# bench-stripped:       15K  (sin debug info ni simbolos)
```

## Agregar flags por build type con generator expressions

Las generator expressions permiten agregar flags condicionales
segun el build type. La sintaxis basica es
`$<$<CONFIG:tipo>:valor>`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(genexpr_demo C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_executable(app main.c)

# Agregar flags solo en Debug:
target_compile_options(app PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer>
)
target_link_options(app PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# Agregar flags solo en Release:
target_compile_options(app PRIVATE
    $<$<CONFIG:Release>:-march=native>
)
```

```cmake
# Sintaxis de generator expressions relevantes para build types:
#
# $<CONFIG:cfg>
#   Evalua a 1 si el build type es "cfg", 0 si no.
#
# $<$<CONFIG:Debug>:valor>
#   Si CONFIG es Debug, se expande a "valor". Si no, a nada.
#
# $<$<CONFIG:Release>:-march=native -flto>
#   Agrega -march=native -flto solo en Release.
#
# $<$<NOT:$<CONFIG:Debug>>:-DNDEBUG_EXTRA>
#   Agrega el flag en todos los modos EXCEPTO Debug.

target_compile_definitions(app PRIVATE
    $<$<CONFIG:Debug>:ENABLE_LOGGING>
    $<$<CONFIG:Debug>:ENABLE_ASSERTS>
    $<$<NOT:$<CONFIG:Debug>>:PRODUCTION_BUILD>
)
```

```cmake
# Tambien se pueden combinar condiciones:
# $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:GNU>>:-fanalyzer>
# Solo agrega -fanalyzer si es Debug Y el compilador es GCC.

target_compile_options(app PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:GNU>>:-fanalyzer>
    $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:Clang>>:-fsanitize=undefined>
)
```

## Multiples build directories

CMake permite tener multiples directorios de build simultaneamente,
cada uno con un build type diferente. Esto es una ventaja sobre
Make, donde tipicamente hay un solo directorio de build:

```bash
# Crear directorios de build separados:
cmake -DCMAKE_BUILD_TYPE=Debug        -B build-debug
cmake -DCMAKE_BUILD_TYPE=Release      -B build-release
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build-reldbg

# Compilar cada uno independientemente:
cmake --build build-debug
cmake --build build-release

# Ambos coexisten sin interferir.
# No hay que reconfigurar al cambiar de modo.
```

```bash
# Workflow tipico de desarrollo:
#
# 1. Desarrollar y depurar con Debug:
cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug
cmake --build build-debug
./build-debug/app        # ejecutar
gdb ./build-debug/app    # depurar con GDB

# 2. Probar rendimiento con Release:
cmake -DCMAKE_BUILD_TYPE=Release -B build-release
cmake --build build-release
time ./build-release/app  # medir rendimiento

# 3. Diagnosticar un problema en produccion con RelWithDebInfo:
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build-reldbg
cmake --build build-reldbg
perf record ./build-reldbg/app   # profiling
perf report                       # analizar resultados
```

```bash
# Estructura de directorios resultante:
# proyecto/
#   CMakeLists.txt
#   main.c
#   build-debug/
#     app              (con -g -O0, asserts activos)
#   build-release/
#     app              (con -O3 -DNDEBUG, optimizado)
#   build-reldbg/
#     app              (con -O2 -g -DNDEBUG, para profiling)
```

## Generadores multi-config

En generadores de build de una sola configuracion (Unix Makefiles,
Ninja), el build type se elige al configurar con
`-DCMAKE_BUILD_TYPE`. En generadores multi-config (Visual Studio,
Xcode, Ninja Multi-Config), el tipo se elige al compilar:

```bash
# Generador single-config (Unix Makefiles, Ninja):
# El tipo se fija al configurar. Un directorio de build = un tipo.
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
# No se puede cambiar el tipo sin reconfigurar.
```

```bash
# Generador multi-config (Ninja Multi-Config, Visual Studio):
# El directorio de build soporta TODOS los tipos simultaneamente.
cmake -G "Ninja Multi-Config" -B build
cmake --build build --config Debug      # compilar en Debug
cmake --build build --config Release    # compilar en Release
# Ambos tipos comparten el mismo directorio de build.
# Los binarios se colocan en subdirectorios separados.
```

```cmake
# En CMakeLists.txt, para ser compatible con ambos tipos de generador:
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# CMAKE_CONFIGURATION_TYPES existe en generadores multi-config.
# Si existe, NO debemos setear CMAKE_BUILD_TYPE (no tiene efecto).
# La condicion AND NOT CMAKE_CONFIGURATION_TYPES evita eso.
```

## Custom build types

Se pueden definir build types personalizados configurando las
variables de flags correspondientes. Es rara vez necesario, pero
posible:

```cmake
# Definir un custom build type "Profile":
set(CMAKE_C_FLAGS_PROFILE "-O2 -g -pg" CACHE STRING
    "Flags for profiling build type" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE "-pg" CACHE STRING
    "Linker flags for profiling build type" FORCE)

# Marcar como tipo valido (para cmake-gui):
set_property(CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel" "Profile")

# Usar:
# cmake -DCMAKE_BUILD_TYPE=Profile -B build-profile
```

```cmake
# En la practica, rara vez se necesitan custom build types.
# Las generator expressions con $<CONFIG:Debug> permiten agregar
# flags especificos sin crear un tipo nuevo.
# Solo crear custom build types cuando la combinacion de flags
# es fundamentalmente diferente a los 4 estandar.
```

## Ejemplo completo

Un proyecto que demuestra build types con default, generator
expressions para sanitizers en Debug, y comparacion de los 4 modos:

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void print_build_info(void) {
#ifdef NDEBUG
    printf("NDEBUG:  definido (asserts desactivados)\n");
#else
    printf("NDEBUG:  no definido (asserts activos)\n");
#endif

#ifdef ENABLE_EXTRA_CHECKS
    printf("Checks:  extra checks habilitados\n");
#else
    printf("Checks:  solo checks estandar\n");
#endif
}

static int sum_array(const int *arr, int n) {
    assert(arr != NULL && "Array pointer is NULL");
    assert(n > 0 && "Array size must be positive");

    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

static int *create_array(int n) {
    assert(n > 0 && "Size must be positive");
    int *arr = malloc((size_t)n * sizeof(int));
    if (!arr) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    for (int i = 0; i < n; i++) {
        arr[i] = i + 1;
    }
    return arr;
}

int main(void) {
    printf("=== Build Type Demo ===\n");
    print_build_info();

    int n = 10000;
    int *arr = create_array(n);

    // Calcular la suma varias veces para medir rendimiento:
    int result = 0;
    for (int rep = 0; rep < 100000; rep++) {
        result = sum_array(arr, n);
    }

    printf("Sum of 1..%d = %d\n", n, result);

    free(arr);
    return 0;
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(build_type_demo C)

# --- Default build type ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# --- Target principal ---
add_executable(app main.c)

target_compile_options(app PRIVATE
    -Wall -Wextra -Wpedantic -std=c17
)

# --- Flags condicionales por build type ---

# AddressSanitizer en Debug (detecta buffer overflows, use-after-free, leaks):
target_compile_options(app PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address -fno-omit-frame-pointer>
)
target_link_options(app PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

# Definicion extra solo en Debug:
target_compile_definitions(app PRIVATE
    $<$<CONFIG:Debug>:ENABLE_EXTRA_CHECKS>
)

# Optimizacion nativa en Release:
target_compile_options(app PRIVATE
    $<$<CONFIG:Release>:-march=native>
)
```

```bash
# Compilar y comparar los 4 modos:

# Debug:
cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug
cmake --build build-debug
echo "--- Debug ---"
./build-debug/app
# === Build Type Demo ===
# NDEBUG:  no definido (asserts activos)
# Checks:  extra checks habilitados
# Sum of 1..10000 = 50005000

# Release:
cmake -DCMAKE_BUILD_TYPE=Release -B build-release
cmake --build build-release
echo "--- Release ---"
./build-release/app
# === Build Type Demo ===
# NDEBUG:  definido (asserts desactivados)
# Checks:  solo checks estandar
# Sum of 1..10000 = 50005000

# RelWithDebInfo:
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build-reldbg
cmake --build build-reldbg
echo "--- RelWithDebInfo ---"
./build-reldbg/app
# === Build Type Demo ===
# NDEBUG:  definido (asserts desactivados)
# Checks:  solo checks estandar
# Sum of 1..10000 = 50005000

# MinSizeRel:
cmake -DCMAKE_BUILD_TYPE=MinSizeRel -B build-minsize
cmake --build build-minsize
echo "--- MinSizeRel ---"
./build-minsize/app
# === Build Type Demo ===
# NDEBUG:  definido (asserts desactivados)
# Checks:  solo checks estandar
# Sum of 1..10000 = 50005000
```

```bash
# Comparar tamanos y tiempos:
echo "--- Tamanos ---"
ls -lh build-debug/app build-release/app build-reldbg/app build-minsize/app

echo "--- Tiempos ---"
echo "Debug:"        && time ./build-debug/app > /dev/null
echo "Release:"      && time ./build-release/app > /dev/null
echo "RelWithDebInfo:" && time ./build-reldbg/app > /dev/null
echo "MinSizeRel:"   && time ./build-minsize/app > /dev/null

echo "--- Debug info ---"
file build-debug/app     # ... with debug_info, not stripped
file build-release/app   # ... not stripped (sin debug_info)
file build-reldbg/app    # ... with debug_info, not stripped
file build-minsize/app   # ... not stripped (sin debug_info)
```

```bash
# Verificar los flags reales en cada modo:
cmake --build build-debug --verbose 2>&1 | grep -o '\-[Og][^ ]*\|-DNDEBUG\|-fsanitize=[^ ]*'
# -g
# -O0 (implicito, no siempre visible)
# -fsanitize=address

cmake --build build-release --verbose 2>&1 | grep -o '\-[Og][^ ]*\|-DNDEBUG\|-march=[^ ]*'
# -O3
# -DNDEBUG
# -march=native
```

---

## Ejercicios

### Ejercicio 1 --- Comparar los 4 build types

```
Crear un proyecto con un programa que:
  - Imprima si NDEBUG esta definido o no
  - Use assert() para validar un argumento
  - Ejecute un loop computacionalmente costoso (ej: fibonacci recursivo hasta 40)

1. Escribir un CMakeLists.txt con default build type Debug.

2. Compilar en los 4 build types, cada uno en su directorio:
     build-debug, build-release, build-reldbg, build-minsize

3. Para cada binario, registrar:
   - Tamano con ls -lh
   - Tiempo de ejecucion con time
   - Presencia de debug info con file
   - Comportamiento de assert al pasar un argumento invalido

4. Responder:
   - Cual es el mas rapido? Por cuanto respecto a Debug?
   - Cual es el mas pequeno?
   - Que ocurre con assert en Release si le pasas un valor invalido?
   - Que diferencia hay entre Release y RelWithDebInfo en la salida de file?
```

### Ejercicio 2 --- Default build type y generator expressions

```
1. Crear un CMakeLists.txt que:
   - Establezca Release como build type por defecto (con la guarda
     para CMAKE_CONFIGURATION_TYPES)
   - Defina un target "app" a partir de main.c
   - En Debug: habilite AddressSanitizer (-fsanitize=address) y defina
     ENABLE_LOGGING
   - En Release: agregue -march=native y -flto
   - En todos los modos: agregue -Wall -Wextra

2. Escribir main.c con:
   - Un #ifdef ENABLE_LOGGING que imprima mensajes de depuracion
   - Un buffer overflow intencional SOLO cuando se pase un argumento
     especifico (ej: ./app --trigger-bug)

3. Verificar que:
   - cmake -B build (sin -D) usa Release (el default)
   - cmake -DCMAKE_BUILD_TYPE=Debug -B build-debug usa Debug
   - En Debug, AddressSanitizer detecta el buffer overflow
   - En Release, los mensajes de logging NO aparecen
   - cmake --build build --verbose muestra -flto en Release
     y -fsanitize=address en Debug
```

### Ejercicio 3 --- Multiples builds y profiling

```
1. Crear un proyecto con un programa que realice operaciones sobre
   un array grande (ej: ordenamiento burbuja de 50000 elementos).

2. Compilar en tres directorios simultaneos:
   - build-debug (para desarrollar)
   - build-release (para produccion)
   - build-reldbg (para profiling)

3. Ejecutar perf stat sobre el binario de RelWithDebInfo y el de Release.
   Comparar:
   - Instrucciones ejecutadas
   - Cache misses
   - Tiempo total

4. Usar gdb sobre el binario de RelWithDebInfo:
   - Poner un breakpoint en la funcion de ordenamiento
   - Observar que algunas variables aparecen como <optimized out>
   - Comparar con gdb sobre el binario de Debug (todas las variables visibles)

5. Responder:
   - Por que RelWithDebInfo usa -O2 en vez de -O3?
   - Que pasa si intentas usar gdb con el binario de Release?
   - En que situaciones usarias MinSizeRel en un proyecto real?
```
