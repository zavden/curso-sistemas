# CTest con CMake — add_test(), ctest --output-on-failure, labels, fixtures, integracion con el build system

## 1. Introduccion

CTest es el **test runner integrado de CMake**. No es un framework de testing (no proporciona asserts, fixtures, ni macros de test). Es la capa que **descubre, ejecuta, filtra y reporta** los resultados de cualquier ejecutable de test, sin importar que framework usa internamente (Unity, CMocka, Check, un programa con assert.h, o incluso un script de shell).

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                           CTEST                                                  │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Que es:     Test runner integrado en CMake (parte del paquete cmake)            │
│  Que NO es:  Un framework de testing (no tiene asserts ni macros)               │
│  Proposito:  Descubrir, ejecutar, filtrar y reportar resultados de tests        │
│  Invocacion: ctest (desde el directorio de build)                               │
│  Relacion:   CMake registra tests → CTest los ejecuta                           │
│                                                                                  │
│  ┌──────────────────────────────────────────────────────────────────────┐        │
│  │                    FLUJO COMPLETO                                    │        │
│  │                                                                      │        │
│  │  CMakeLists.txt          Build                     Ejecucion         │        │
│  │  ┌──────────────┐   ┌──────────────┐   ┌────────────────────────┐   │        │
│  │  │ add_test()   │──▶│ cmake --build│──▶│ ctest                  │   │        │
│  │  │ set_tests_   │   │ .            │   │ --output-on-failure    │   │        │
│  │  │ properties() │   │              │   │ -j$(nproc)             │   │        │
│  │  └──────────────┘   └──────────────┘   └────────────────────────┘   │        │
│  │                                                                      │        │
│  │  Framework interno:  Unity, CMocka, Check, assert.h, cualquiera     │        │
│  │  CTest solo ve:      exit code 0 = PASS, != 0 = FAIL               │        │
│  └──────────────────────────────────────────────────────────────────────┘        │
│                                                                                  │
│  Ventajas clave:                                                                │
│  • Tests declarados en el mismo sistema de build (un solo CMakeLists.txt)       │
│  • Ejecucion paralela nativa (-j N)                                             │
│  • Labels para agrupar y filtrar                                                │
│  • Timeout y retry integrados                                                   │
│  • Salida en formatos XML (para CI: Jenkins, GitLab CI, GitHub Actions)         │
│  • Independiente del framework de testing subyacente                            │
│  • Works on all platforms (Windows, Linux, macOS)                               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 CTest vs Make-based test runners

En topicos anteriores construimos Makefiles con targets `test`. La diferencia fundamental:

| Aspecto | Make (target test) | CTest |
|---------|--------------------|-------|
| Declaracion de tests | Manual en Makefile | `add_test()` en CMakeLists.txt |
| Descubrimiento automatico | No (hay que listar cada test) | Si, con `add_test()` o `gtest_discover_tests()` |
| Ejecucion paralela | Limitada (`make -j` compila, no testea en paralelo) | Nativa (`ctest -j N`) |
| Labels/filtros | No tiene | Si (`-L`, `-R`, `-E`) |
| Timeout por test | Manual (timeout command) | Propiedad `TIMEOUT` |
| Retry on failure | Manual | `--repeat until-pass:N` |
| Salida XML/JUnit | No | Si (`--output-junit`) |
| Fixtures (setup/teardown entre tests) | No | Si (FIXTURES_SETUP, FIXTURES_CLEANUP) |
| Cross-platform | Solo Unix/Make | Windows, Linux, macOS |
| Reporte | Todo stdout mezclado | Por test, con pass/fail individual |

### 1.2 Relacion con CDash

CTest puede enviar resultados a **CDash**, un dashboard web (tambien parte del ecosistema Kitware). El flujo completo se llama **CTest-CDash pipeline**:

```
CMake → Build → CTest → XML results → CDash (dashboard web)
```

CDash es opcional y avanzado. Este topico se centra en CTest local y CI.

---

## 2. Requisitos e instalacion

CTest viene incluido con CMake. Si tienes cmake, tienes ctest.

```bash
# Verificar instalacion
cmake --version    # >= 3.14 recomendado (por mejoras en add_test)
ctest --version    # Mismo version que cmake

# Instalar en Fedora/RHEL
sudo dnf install cmake

# Instalar en Debian/Ubuntu
sudo apt install cmake

# Verificar que ctest esta disponible
which ctest
# /usr/bin/ctest
```

### 2.1 Version minima recomendada

| Feature | CMake minimo |
|---------|-------------|
| `add_test(NAME ... COMMAND ...)` | 2.8.12 |
| `set_tests_properties()` | 2.8 |
| Labels (`LABELS`) | 2.8 |
| `FIXTURES_SETUP` / `FIXTURES_CLEANUP` | 3.7 |
| `FIXTURES_REQUIRED` | 3.7 |
| `--output-junit` | 3.21 |
| `--repeat` | 3.17 |
| `cmake_test_timeout` global | 2.8 |
| `RESOURCE_LOCK` | 3.0 |
| `WILL_FAIL` | 2.8 |
| FetchContent (para bajar frameworks) | 3.11 |
| `DISCOVERY_MODE PRE_TEST` (GoogleTest) | 3.18 |

Recomendacion: usar `cmake_minimum_required(VERSION 3.14)` para tener todas las funcionalidades relevantes.

---

## 3. Primer ejemplo — add_test() y enable_testing()

### 3.1 Proyecto minimo

Estructura:

```
proyecto/
├── CMakeLists.txt
├── src/
│   └── math_utils.c
├── include/
│   └── math_utils.h
└── tests/
    └── test_math_utils.c
```

**include/math_utils.h**:

```c
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

int factorial(int n);
int fibonacci(int n);
int gcd(int a, int b);
int power(int base, int exp);

#endif
```

**src/math_utils.c**:

```c
#include "math_utils.h"

int factorial(int n) {
    if (n < 0) return -1;  // error
    if (n <= 1) return 1;
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

int fibonacci(int n) {
    if (n < 0) return -1;
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

int gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        int tmp = b;
        b = a % b;
        a = tmp;
    }
    return a;
}

int power(int base, int exp) {
    if (exp < 0) return 0;  // no soportamos exponentes negativos
    int result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}
```

**tests/test_math_utils.c** (usando assert.h simple):

```c
#include <stdio.h>
#include <assert.h>
#include "math_utils.h"

// --- Tests para factorial ---

static void test_factorial_zero(void) {
    assert(factorial(0) == 1);
    printf("  PASS: factorial(0) == 1\n");
}

static void test_factorial_one(void) {
    assert(factorial(1) == 1);
    printf("  PASS: factorial(1) == 1\n");
}

static void test_factorial_five(void) {
    assert(factorial(5) == 120);
    printf("  PASS: factorial(5) == 120\n");
}

static void test_factorial_negative(void) {
    assert(factorial(-1) == -1);
    printf("  PASS: factorial(-1) == -1 (error code)\n");
}

// --- Tests para fibonacci ---

static void test_fibonacci_zero(void) {
    assert(fibonacci(0) == 0);
    printf("  PASS: fibonacci(0) == 0\n");
}

static void test_fibonacci_one(void) {
    assert(fibonacci(1) == 1);
    printf("  PASS: fibonacci(1) == 1\n");
}

static void test_fibonacci_ten(void) {
    assert(fibonacci(10) == 55);
    printf("  PASS: fibonacci(10) == 55\n");
}

// --- Tests para gcd ---

static void test_gcd_basic(void) {
    assert(gcd(12, 8) == 4);
    printf("  PASS: gcd(12, 8) == 4\n");
}

static void test_gcd_coprime(void) {
    assert(gcd(17, 13) == 1);
    printf("  PASS: gcd(17, 13) == 1\n");
}

static void test_gcd_negative(void) {
    assert(gcd(-12, 8) == 4);
    printf("  PASS: gcd(-12, 8) == 4\n");
}

int main(void) {
    printf("=== math_utils tests ===\n");
    test_factorial_zero();
    test_factorial_one();
    test_factorial_five();
    test_factorial_negative();
    test_fibonacci_zero();
    test_fibonacci_one();
    test_fibonacci_ten();
    test_gcd_basic();
    test_gcd_coprime();
    test_gcd_negative();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 3.2 CMakeLists.txt con add_test()

```cmake
cmake_minimum_required(VERSION 3.14)
project(math_utils_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Libreria ---
add_library(math_utils src/math_utils.c)
target_include_directories(math_utils PUBLIC include)

# --- HABILITAR TESTING ---
# Esta linea es OBLIGATORIA para que add_test() funcione.
# Sin ella, add_test() es ignorado silenciosamente.
enable_testing()

# --- Ejecutable de test ---
add_executable(test_math_utils tests/test_math_utils.c)
target_link_libraries(test_math_utils PRIVATE math_utils)

# --- Registrar test ---
# Forma moderna (CMake >= 2.8.12):
add_test(
    NAME test_math_utils        # Nombre logico (el que ve ctest)
    COMMAND test_math_utils     # Ejecutable a correr
)
```

### 3.3 Las dos formas de add_test()

```cmake
# FORMA ANTIGUA (pre-2.8.12) — evitar
add_test(test_name executable_name arg1 arg2)
# Problemas:
# - El ejecutable debe estar en CMAKE_CURRENT_BINARY_DIR
# - No funciona bien con generator expressions
# - No soporta WORKING_DIRECTORY

# FORMA MODERNA (2.8.12+) — usar siempre
add_test(
    NAME test_name                     # Nombre logico
    COMMAND executable_name arg1 arg2  # Que ejecutar
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}  # Opcional
)
# Ventajas:
# - Resuelve la ruta del ejecutable automaticamente
# - Soporta generator expressions: $<TARGET_FILE:target>
# - WORKING_DIRECTORY configurable
```

### 3.4 Build y ejecucion

```bash
# Configurar
mkdir build && cd build
cmake ..

# Compilar
cmake --build .

# Ejecutar tests con CTest
ctest

# Salida tipica:
# Test project /home/user/proyecto/build
#     Start 1: test_math_utils
# 1/1 Test #1: test_math_utils ..................   Passed    0.00 sec
#
# 100% tests passed, 0 tests failed out of 1
#
# Total Test time (real) =   0.01 sec
```

### 3.5 enable_testing() — por que es obligatorio

```cmake
# enable_testing() hace DOS cosas:
#
# 1. Habilita el comando add_test() en el directorio actual y subdirectorios
# 2. Crea un archivo CTestTestfile.cmake en el directorio de build
#
# Sin enable_testing():
# - add_test() no genera error pero no registra nada
# - ctest dice "No tests were found!!!"
# - Es un error silencioso y frustrante

# DONDE ponerlo:
# - En el CMakeLists.txt raiz (lo mas comun)
# - Debe ir ANTES de cualquier add_test()
# - Si usas subdirectorios, basta con ponerlo en el raiz

# Equivalente alternativo (se ve en algunos proyectos):
include(CTest)
# Esto hace enable_testing() + configura CDash
# Usar si planeas enviar a CDash, sino enable_testing() basta
```

---

## 4. ctest — opciones de linea de comandos

### 4.1 Referencia completa de flags

```bash
# === EJECUCION BASICA ===
ctest                          # Ejecutar todos los tests
ctest --test-dir build         # Especificar directorio de build (CMake 3.20+)
ctest --build-and-test ...     # Configurar, compilar y testear en un paso

# === OUTPUT ===
ctest --output-on-failure      # Mostrar stdout/stderr SOLO si el test falla
ctest -V                       # Verbose: mostrar output de todos los tests
ctest -VV                      # Extra verbose: mas detalle de ctest internals
ctest --quiet                  # Solo mostrar resumen final
ctest --progress               # Mostrar barra de progreso

# === FILTRAR TESTS ===
ctest -R "regex"               # Solo tests cuyo NOMBRE matchea la regex
ctest -E "regex"               # EXCLUIR tests cuyo nombre matchea la regex
ctest -L "regex"               # Solo tests con LABEL que matchea la regex
ctest -LE "regex"              # Excluir tests con label que matchea
ctest -I start,end,stride      # Ejecutar tests por rango numerico
ctest -N                       # Dry run: listar tests sin ejecutar

# === PARALELISMO ===
ctest -j N                     # Ejecutar N tests en paralelo
ctest -j $(nproc)              # Un test por CPU core
ctest --parallel N             # Sinonimo de -j N

# === TIMEOUT ===
ctest --timeout 30             # Timeout global: 30 segundos por test
                                # (override de la propiedad TIMEOUT por test)

# === REPETICION / RETRY ===
ctest --repeat until-fail:5    # Repetir cada test 5 veces, fallar si alguna falla
ctest --repeat until-pass:3    # Repetir hasta 3 veces, pasar si alguna pasa
ctest --repeat after-timeout:2 # Repetir solo si hubo timeout

# === RERUN ===
ctest --rerun-failed           # Re-ejecutar solo los tests que fallaron antes

# === OUTPUT FORMAT ===
ctest --output-junit result.xml  # Generar XML en formato JUnit (CMake 3.21+)
ctest --output-log log.txt       # Log completo a archivo

# === SCHEDULING ===
ctest --schedule-random        # Ejecutar tests en orden aleatorio
                                # (detecta dependencias ocultas entre tests)

# === CONFIGURACION ===
ctest -C Debug                 # Seleccionar configuracion (multi-config generators)
ctest -C Release               # (Visual Studio, Xcode, Ninja Multi-Config)
```

### 4.2 La flag mas importante: --output-on-failure

Esta es la flag que usaras el 90% del tiempo. Sin ella, cuando un test falla ctest solo dice "Failed" sin mostrar por que:

```bash
# SIN --output-on-failure:
# 1/3 Test #1: test_factorial ...................   Passed    0.00 sec
# 2/3 Test #2: test_fibonacci ...................***Failed    0.00 sec
# 3/3 Test #3: test_gcd .........................   Passed    0.00 sec
#
# 67% tests passed, 1 tests failed out of 3
# The following tests FAILED:
#         2 - test_fibonacci (Failed)

# CON --output-on-failure:
# 1/3 Test #1: test_factorial ...................   Passed    0.00 sec
# 2/3 Test #2: test_fibonacci ...................***Failed    0.00 sec
# Output:
# === fibonacci tests ===
#   PASS: fibonacci(0) == 0
#   PASS: fibonacci(1) == 1
# test_fibonacci: tests/test_fibonacci.c:27: test_fibonacci_ten:
#     Assertion `fibonacci(10) == 56' failed.
# 3/3 Test #3: test_gcd .........................   Passed    0.00 sec
```

### 4.3 Hacer --output-on-failure permanente

```bash
# Opcion 1: Variable de entorno
export CTEST_OUTPUT_ON_FAILURE=1
ctest  # Ahora siempre muestra output en failure

# Opcion 2: En CMakeLists.txt (CMake 3.17+)
# No hay forma directa, pero puedes documentarlo en un preset

# Opcion 3: CMake presets (cmake 3.21+)
# En CMakePresets.json:
{
    "version": 3,
    "testPresets": [
        {
            "name": "default",
            "configurePreset": "default",
            "output": {
                "outputOnFailure": true
            }
        }
    ]
}
```

### 4.4 Combinaciones utiles

```bash
# Desarrollo normal: correr todo con output en failure
ctest --output-on-failure -j$(nproc)

# Debugging: correr un test especifico con output completo
ctest -R "test_factorial" -V

# CI pipeline: XML output + paralelo + timeout
ctest -j$(nproc) --output-on-failure --timeout 60 --output-junit results.xml

# Buscar tests flaky (intermitentes)
ctest --repeat until-fail:10 --schedule-random -j1

# Re-run solo los que fallaron
ctest --rerun-failed --output-on-failure

# Listar tests disponibles sin ejecutar
ctest -N

# Solo tests de una categoria (por label)
ctest -L "unit" --output-on-failure -j$(nproc)
```

---

## 5. Tests multiples — un add_test() por ejecutable

### 5.1 El patron basico

La forma mas directa de usar CTest es registrar cada ejecutable de test como un test independiente:

```cmake
cmake_minimum_required(VERSION 3.14)
project(multi_test_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libreria
add_library(mylib
    src/stack.c
    src/queue.c
    src/string_utils.c
)
target_include_directories(mylib PUBLIC include)

enable_testing()

# Test por modulo
add_executable(test_stack tests/test_stack.c)
target_link_libraries(test_stack PRIVATE mylib)
add_test(NAME test_stack COMMAND test_stack)

add_executable(test_queue tests/test_queue.c)
target_link_libraries(test_queue PRIVATE mylib)
add_test(NAME test_queue COMMAND test_queue)

add_executable(test_string_utils tests/test_string_utils.c)
target_link_libraries(test_string_utils PRIVATE mylib)
add_test(NAME test_string_utils COMMAND test_string_utils)
```

```bash
$ ctest -N
Test project /home/user/build
  Test #1: test_stack
  Test #2: test_queue
  Test #3: test_string_utils

Total Tests: 3
```

### 5.2 Reducir repeticion con una funcion CMake

Cuando tienes muchos test executables, el patron se repite. CMake permite definir funciones:

```cmake
# Funcion helper para registrar un test
function(add_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Uso limpio
add_unit_test(test_stack      tests/test_stack.c)
add_unit_test(test_queue      tests/test_queue.c)
add_unit_test(test_string_utils tests/test_string_utils.c)
```

### 5.3 Funcion con labels

```cmake
# Funcion que ademas asigna labels
function(add_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # ARGN contiene argumentos extra (los labels)
    if(ARGN)
        set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${ARGN}")
    endif()
endfunction()

# Uso con labels
add_unit_test(test_stack        tests/test_stack.c       "unit;data_structures")
add_unit_test(test_queue        tests/test_queue.c       "unit;data_structures")
add_unit_test(test_string_utils tests/test_string_utils.c "unit;strings")
add_unit_test(test_file_io      tests/test_file_io.c     "integration;io")
```

### 5.4 Autodescubrimiento de tests

```cmake
# Descubrir todos los archivos test_*.c automaticamente
file(GLOB TEST_SOURCES tests/test_*.c)

foreach(TEST_SOURCE ${TEST_SOURCES})
    # Extraer nombre del archivo sin extension
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
```

**Advertencia sobre file(GLOB)**: CMake no re-escanea el glob cuando agregas un nuevo archivo. Necesitas re-ejecutar `cmake ..` para que detecte archivos nuevos. Por eso algunos proyectos prefieren listar los tests explicitamente.

```cmake
# Alternativa mas segura: listar explicitamente
set(TEST_SOURCES
    tests/test_stack.c
    tests/test_queue.c
    tests/test_string_utils.c
)
# Ahora CMake sabe exactamente que archivos hay
```

---

## 6. Argumentos y variables de entorno

### 6.1 Pasar argumentos al ejecutable de test

```c
// tests/test_with_args.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test_data_dir>\n", argv[0]);
        return 1;
    }

    const char *data_dir = argv[1];
    printf("Using test data from: %s\n", data_dir);

    // Construir ruta al archivo de test
    char path[256];
    snprintf(path, sizeof(path), "%s/input.txt", data_dir);

    FILE *f = fopen(path, "r");
    assert(f != NULL && "test data file must exist");

    char line[128];
    assert(fgets(line, sizeof(line), f) != NULL);
    assert(strcmp(line, "expected_content\n") == 0);
    fclose(f);

    printf("PASS: file content matches expected\n");
    return 0;
}
```

```cmake
add_executable(test_with_args tests/test_with_args.c)

add_test(
    NAME test_with_args
    COMMAND test_with_args ${CMAKE_SOURCE_DIR}/test_data
)
# CTest pasara la ruta al directorio de test data como argv[1]
```

### 6.2 Variables de entorno con ENVIRONMENT

```cmake
add_test(NAME test_with_env COMMAND test_with_env)

set_tests_properties(test_with_env PROPERTIES
    ENVIRONMENT "DATABASE_URL=sqlite::memory:;LOG_LEVEL=debug;TIMEOUT=30"
)
# Cada variable separada por ;
```

### 6.3 WORKING_DIRECTORY

```cmake
add_test(
    NAME test_file_operations
    COMMAND test_file_operations
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test_data
)
# El test se ejecuta con cwd = test_data/
# Util cuando los tests leen archivos con rutas relativas
```

### 6.4 Generator expressions en COMMAND

```cmake
# $<TARGET_FILE:target> se resuelve a la ruta completa del ejecutable
add_test(
    NAME test_via_genex
    COMMAND $<TARGET_FILE:test_math_utils>
)
# Util en multi-config generators (Visual Studio, Xcode)
# donde la ruta incluye Debug/ o Release/

# Pasar la ruta de una libreria como argumento
add_test(
    NAME test_plugin_loading
    COMMAND test_plugin_loading $<TARGET_FILE:my_plugin>
)
```

---

## 7. set_tests_properties() — propiedades de test

### 7.1 Referencia de propiedades

`set_tests_properties()` es el mecanismo para configurar como CTest ejecuta cada test. Las propiedades mas importantes:

```cmake
# === TIMEOUT ===
set_tests_properties(test_name PROPERTIES
    TIMEOUT 30   # Matar el test si tarda mas de 30 segundos
)
# Default: 1500 segundos (25 minutos!)
# En CI querras algo mucho menor

# === WILL_FAIL ===
set_tests_properties(test_name PROPERTIES
    WILL_FAIL TRUE   # El test DEBE fallar (exit != 0) para contar como PASS
)
# Uso: verificar que un programa detecta errores correctamente
# Si el test retorna 0, CTest lo marca como FAILED

# === PASS_REGULAR_EXPRESSION / FAIL_REGULAR_EXPRESSION ===
set_tests_properties(test_name PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
)
# El test solo pasa si stdout contiene este patron
# Util con frameworks que no usan exit codes consistentes

set_tests_properties(test_name PROPERTIES
    FAIL_REGULAR_EXPRESSION "SEGFAULT|LEAK DETECTED|ERROR"
)
# El test falla si stdout contiene cualquiera de estos patrones

# === SKIP_REGULAR_EXPRESSION (CMake 3.16+) ===
set_tests_properties(test_name PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIPPED|NOT AVAILABLE"
)
# El test se marca como SKIPPED (no FAILED) si stdout contiene el patron
# Exit code 0 pero output contiene "SKIPPED" → test skip, no pass

# === LABELS ===
set_tests_properties(test_name PROPERTIES
    LABELS "unit;fast;math"
)
# Para filtrar con ctest -L "unit"

# === ENVIRONMENT ===
set_tests_properties(test_name PROPERTIES
    ENVIRONMENT "VAR1=val1;VAR2=val2"
)

# === DEPENDS ===
set_tests_properties(test_b PROPERTIES
    DEPENDS test_a   # test_b solo corre si test_a paso
)
# NO controla el orden de ejecucion con -j
# Solo garantiza que si test_a falla, test_b se salta
# Para orden de ejecucion, ver FIXTURES (seccion 10)

# === COST ===
set_tests_properties(test_name PROPERTIES
    COST 100   # Tests con mayor COST se ejecutan primero (con -j)
)
# Optimiza el scheduling: ejecutar los tests lentos primero
# para que no queden solos al final retrasando todo

# === RESOURCE_LOCK ===
set_tests_properties(test_a PROPERTIES RESOURCE_LOCK "database")
set_tests_properties(test_b PROPERTIES RESOURCE_LOCK "database")
# test_a y test_b NO se ejecutaran en paralelo (ambos necesitan "database")
# Otros tests sin este lock SI corren en paralelo con ellos

# === DISABLED (CMake 3.9+) ===
set_tests_properties(test_name PROPERTIES
    DISABLED TRUE   # No ejecutar este test, mostrar como "Not Run"
)

# === REQUIRED_FILES ===
set_tests_properties(test_name PROPERTIES
    REQUIRED_FILES "/path/to/data.txt;/path/to/config.ini"
)
# Si algun archivo no existe, el test se salta (no falla)

# === RUN_SERIAL ===
set_tests_properties(test_name PROPERTIES
    RUN_SERIAL TRUE   # Este test nunca corre en paralelo con otros
)
# Util para tests que usan recursos globales (puertos, archivos tmp)

# === PROCESSORS ===
set_tests_properties(test_name PROPERTIES
    PROCESSORS 4   # Este test usa 4 cores
)
# CTest lo tiene en cuenta al hacer scheduling con -j
# Un test con PROCESSORS 4 cuenta como 4 para el limite de -j
```

### 7.2 Ejemplo completo con multiples propiedades

```cmake
enable_testing()

# --- Test rapido unitario ---
add_test(NAME test_math COMMAND test_math)
set_tests_properties(test_math PROPERTIES
    LABELS "unit;fast"
    TIMEOUT 5
)

# --- Test de integracion lento ---
add_test(NAME test_database COMMAND test_database)
set_tests_properties(test_database PROPERTIES
    LABELS "integration;slow;database"
    TIMEOUT 60
    RESOURCE_LOCK "database"
    ENVIRONMENT "DB_URL=sqlite::memory:"
)

# --- Test que debe fallar ---
add_test(NAME test_invalid_input COMMAND test_invalid_input)
set_tests_properties(test_invalid_input PROPERTIES
    WILL_FAIL TRUE
    LABELS "unit;validation"
    TIMEOUT 5
)

# --- Test que requiere archivo ---
add_test(NAME test_config_parser COMMAND test_config_parser)
set_tests_properties(test_config_parser PROPERTIES
    REQUIRED_FILES "${CMAKE_SOURCE_DIR}/test_data/config.ini"
    LABELS "unit"
    TIMEOUT 10
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test_data
)

# --- Test que necesita output especifico ---
add_test(NAME test_logger COMMAND test_logger)
set_tests_properties(test_logger PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "SEGFAULT|ABORT"
    LABELS "unit"
    TIMEOUT 10
)
```

```bash
# Ejecutar solo tests unitarios rapidos
ctest -L "fast" --output-on-failure -j$(nproc)

# Ejecutar tests de integracion (de a uno por el RESOURCE_LOCK)
ctest -L "integration" --output-on-failure

# Listar todo
ctest -N
# Test project /home/user/build
#   Test #1: test_math
#   Test #2: test_database
#   Test #3: test_invalid_input
#   Test #4: test_config_parser
#   Test #5: test_logger
```

---

## 8. Labels — filtrar y organizar tests

### 8.1 Asignar labels

```cmake
# Un label
set_tests_properties(test_math PROPERTIES LABELS "unit")

# Multiples labels (separados por ;)
set_tests_properties(test_math PROPERTIES LABELS "unit;fast;math")

# En la funcion helper
function(add_labeled_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Todos los argumentos extra son labels
    set(LABELS "")
    foreach(LABEL ${ARGN})
        if(LABELS)
            set(LABELS "${LABELS};${LABEL}")
        else()
            set(LABELS "${LABEL}")
        endif()
    endforeach()

    if(LABELS)
        set_tests_properties(${TEST_NAME} PROPERTIES LABELS "${LABELS}")
    endif()
endfunction()

add_labeled_test(test_stack   tests/test_stack.c   unit fast data_structures)
add_labeled_test(test_file_io tests/test_file_io.c integration slow io)
```

### 8.2 Filtrar por label

```bash
# Tests con label "unit"
ctest -L "unit"

# Tests con label que contiene "data"
ctest -L "data"
# Matchea "data_structures" porque es regex parcial

# Tests con label exacto (usar anclas regex)
ctest -L "^unit$"

# EXCLUIR label
ctest -LE "slow"

# Combinar: solo unit pero no slow
ctest -L "unit" -LE "slow"

# Combinar label con regex de nombre
ctest -L "unit" -R "stack"
# Solo tests con label "unit" Y nombre que contenga "stack"
```

### 8.3 Taxonomia de labels recomendada

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    TAXONOMIA DE LABELS                                           │
├─────────────────┬────────────────────────────────────────────────────────────────┤
│ Tipo            │ Labels posibles                                               │
├─────────────────┼────────────────────────────────────────────────────────────────┤
│ Alcance         │ unit, integration, system, e2e                               │
│ Velocidad       │ fast, slow                                                   │
│ Modulo          │ math, strings, io, network, database                         │
│ Prioridad       │ smoke, critical, nightly                                     │
│ Recurso         │ needs_db, needs_network, needs_root                          │
│ Plataforma      │ linux_only, windows_only                                     │
└─────────────────┴────────────────────────────────────────────────────────────────┘
```

### 8.4 Labels en CI

```yaml
# GitHub Actions — ejecutar tests por fase
jobs:
  fast-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake -B build && cmake --build build
      - run: cd build && ctest -L "fast" --output-on-failure -j$(nproc)

  integration-tests:
    needs: fast-tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: cmake -B build && cmake --build build
      - run: cd build && ctest -L "integration" --output-on-failure --timeout 120
```

---

## 9. WILL_FAIL y PASS_REGULAR_EXPRESSION — tests de error

### 9.1 WILL_FAIL — verificar que un programa falla

Hay situaciones donde quieres verificar que tu codigo **detecta y reporta** un error. El programa debe terminar con exit code != 0. Con `WILL_FAIL TRUE`, CTest invierte la logica:

```
┌────────────────────────────────────────────────────────────────────┐
│  Sin WILL_FAIL:     exit 0 → PASS    exit != 0 → FAIL            │
│  Con WILL_FAIL:     exit 0 → FAIL    exit != 0 → PASS            │
└────────────────────────────────────────────────────────────────────┘
```

```c
// tests/test_must_reject_negative.c
#include <stdio.h>
#include <stdlib.h>
#include "math_utils.h"

int main(void) {
    // factorial(-1) debe retornar error code
    int result = factorial(-100);
    if (result == -1) {
        fprintf(stderr, "Correctly rejected negative input\n");
        return 1;  // exit != 0 = "fallo esperado"
    }
    // Si llega aqui, factorial no detecto el error
    fprintf(stderr, "BUG: factorial accepted -100 without error\n");
    return 0;
}
```

```cmake
add_test(NAME test_reject_negative COMMAND test_must_reject_negative)
set_tests_properties(test_reject_negative PROPERTIES
    WILL_FAIL TRUE
    LABELS "unit;validation"
)
```

### 9.2 Verificar crashes esperados con WILL_FAIL

```c
// tests/test_null_crash.c
// Verificar que pasar NULL causa un crash (SEGFAULT o abort)
#include "stack.h"

int main(void) {
    stack_push(NULL, 42);  // Debe crashear
    return 0;  // Si llega aqui, hay un bug: acepto NULL
}
```

```cmake
add_test(NAME test_null_crash COMMAND test_null_crash)
set_tests_properties(test_null_crash PROPERTIES
    WILL_FAIL TRUE
    TIMEOUT 5
    LABELS "unit;robustness"
)
```

### 9.3 PASS_REGULAR_EXPRESSION — verificar output

A veces el exit code no es suficiente. Quieres verificar que la salida contiene cierto texto:

```cmake
# El test solo pasa si stdout contiene "ALL 15 TESTS PASSED"
add_test(NAME test_suite COMMAND test_suite)
set_tests_properties(test_suite PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL [0-9]+ TESTS PASSED"
)

# Multiples patrones (separados por ;) — ANY match es suficiente
set_tests_properties(test_suite PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED;SUCCESS;OK"
)
```

### 9.4 FAIL_REGULAR_EXPRESSION — detectar output problematico

```cmake
# El test falla si stdout/stderr contiene alguno de estos patrones
set_tests_properties(test_suite PROPERTIES
    FAIL_REGULAR_EXPRESSION "SEGFAULT;LEAK;AddressSanitizer;UndefinedBehavior"
)
# Muy util con sanitizers: el test puede retornar 0
# pero si ASan detecto un leak, el output contiene "AddressSanitizer"
```

### 9.5 Combinar propiedades de output

```cmake
set_tests_properties(test_full_suite PROPERTIES
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "LEAK|SEGFAULT|ERROR"
    TIMEOUT 30
)
# Logica:
# 1. Si stdout contiene "LEAK", "SEGFAULT", o "ERROR" → FAIL (sin importar exit code)
# 2. Si stdout contiene "ALL TESTS PASSED" Y no contiene patrones de fallo → PASS
# 3. FAIL_REGULAR_EXPRESSION tiene prioridad sobre PASS_REGULAR_EXPRESSION
```

---

## 10. Fixtures en CTest — FIXTURES_SETUP, FIXTURES_CLEANUP, FIXTURES_REQUIRED

### 10.1 El problema

Algunos tests necesitan que un recurso este preparado antes de ejecutarse (base de datos, servidor, archivos temporales). Y necesitan que ese recurso se limpie despues. Con `DEPENDS` puedes ordenar tests, pero no tienes la semantica de setup/cleanup:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  DEPENDS vs FIXTURES                                                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  DEPENDS test_a:                                                                │
│    - "Solo correr test_b si test_a paso"                                        │
│    - No corre test_a antes de test_b automaticamente                            │
│    - Si test_a no esta en el set de ejecucion, test_b se salta                 │
│                                                                                  │
│  FIXTURES_REQUIRED "MyFixture":                                                 │
│    - "Antes de test_b, ejecutar el setup de MyFixture"                          │
│    - "Despues de test_b, ejecutar el cleanup de MyFixture"                      │
│    - CTest incluye setup/cleanup automaticamente aunque no los pidas            │
│    - Es la analogia de setUp()/tearDown() pero a nivel de PROCESO              │
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────┐             │
│  │  Flujo con fixtures:                                            │             │
│  │                                                                  │             │
│  │  setup_db ──▶ test_users ──▶ cleanup_db                         │             │
│  │  (FIXTURES_   (FIXTURES_     (FIXTURES_                          │             │
│  │   SETUP)       REQUIRED)      CLEANUP)                           │             │
│  │                                                                  │             │
│  │  ctest -R "test_users"                                          │             │
│  │  → CTest automaticamente ejecuta setup_db antes y cleanup_db   │             │
│  │    despues, aunque NO los especificaste en -R                   │             │
│  └─────────────────────────────────────────────────────────────────┘             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 10.2 Definir fixtures

```cmake
enable_testing()

# --- SETUP: crear base de datos temporal ---
add_executable(setup_db tests/setup_db.c)
add_test(NAME setup_db COMMAND setup_db)
set_tests_properties(setup_db PROPERTIES
    FIXTURES_SETUP "Database"       # Este test es el SETUP de la fixture "Database"
    LABELS "fixture"
)

# --- CLEANUP: eliminar base de datos temporal ---
add_executable(cleanup_db tests/cleanup_db.c)
add_test(NAME cleanup_db COMMAND cleanup_db)
set_tests_properties(cleanup_db PROPERTIES
    FIXTURES_CLEANUP "Database"     # Este test es el CLEANUP de la fixture "Database"
    LABELS "fixture"
)

# --- TESTS que necesitan la base de datos ---
add_executable(test_user_crud tests/test_user_crud.c)
add_test(NAME test_user_crud COMMAND test_user_crud)
set_tests_properties(test_user_crud PROPERTIES
    FIXTURES_REQUIRED "Database"    # Requiere que "Database" este setup antes
    LABELS "integration;database"
)

add_executable(test_user_search tests/test_user_search.c)
add_test(NAME test_user_search COMMAND test_user_search)
set_tests_properties(test_user_search PROPERTIES
    FIXTURES_REQUIRED "Database"
    LABELS "integration;database"
)
```

```bash
# Ejecutar solo test_user_crud:
ctest -R "test_user_crud" --output-on-failure

# CTest automaticamente ejecuta:
# 1. setup_db          (FIXTURES_SETUP "Database")
# 2. test_user_crud    (FIXTURES_REQUIRED "Database")
# 3. cleanup_db        (FIXTURES_CLEANUP "Database")
#
# Aunque solo pediste test_user_crud, CTest incluye setup y cleanup
```

### 10.3 Implementacion del setup y cleanup

```c
// tests/setup_db.c — crear base de datos temporal
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_PATH "/tmp/test_db.dat"

int main(void) {
    printf("SETUP: Creating test database at %s\n", DB_PATH);

    FILE *f = fopen(DB_PATH, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    // Escribir schema
    fprintf(f, "TABLE users\n");
    fprintf(f, "id:int name:string email:string\n");

    // Datos de prueba
    fprintf(f, "1 alice alice@test.com\n");
    fprintf(f, "2 bob bob@test.com\n");
    fprintf(f, "3 charlie charlie@test.com\n");

    fclose(f);
    printf("SETUP: Database ready with 3 users\n");
    return 0;
}
```

```c
// tests/cleanup_db.c — eliminar base de datos temporal
#include <stdio.h>
#include <unistd.h>

#define DB_PATH "/tmp/test_db.dat"

int main(void) {
    printf("CLEANUP: Removing test database %s\n", DB_PATH);
    if (unlink(DB_PATH) != 0) {
        perror("unlink");
        // No retornar error: el archivo puede no existir si setup fallo
    }
    printf("CLEANUP: Done\n");
    return 0;
}
```

### 10.4 Multiples fixtures

```cmake
# Fixture: Database
add_test(NAME setup_db COMMAND setup_db)
set_tests_properties(setup_db PROPERTIES FIXTURES_SETUP "Database")
add_test(NAME cleanup_db COMMAND cleanup_db)
set_tests_properties(cleanup_db PROPERTIES FIXTURES_CLEANUP "Database")

# Fixture: TempDir
add_test(NAME setup_tmpdir COMMAND setup_tmpdir)
set_tests_properties(setup_tmpdir PROPERTIES FIXTURES_SETUP "TempDir")
add_test(NAME cleanup_tmpdir COMMAND cleanup_tmpdir)
set_tests_properties(cleanup_tmpdir PROPERTIES FIXTURES_CLEANUP "TempDir")

# Test que necesita AMBAS fixtures
add_test(NAME test_export_to_file COMMAND test_export_to_file)
set_tests_properties(test_export_to_file PROPERTIES
    FIXTURES_REQUIRED "Database;TempDir"    # Multiples fixtures
    LABELS "integration"
)
# CTest ejecuta:
# 1. setup_db + setup_tmpdir (pueden correr en paralelo con -j)
# 2. test_export_to_file
# 3. cleanup_db + cleanup_tmpdir (pueden correr en paralelo)
```

### 10.5 Fixtures vs DEPENDS — cuando usar cada uno

```
┌────────────────────────────────────────────────────────────────────┐
│  DEPENDS                    │  FIXTURES                           │
├─────────────────────────────┼────────────────────────────────────  │
│  "Skip B if A failed"       │  "Run setup before, cleanup after"  │
│  No auto-include            │  Auto-include setup/cleanup         │
│  No cleanup semantics       │  Explicit cleanup phase             │
│  Simpler                    │  More powerful                      │
│  Good for: test ordering    │  Good for: resource lifecycle       │
└─────────────────────────────┴─────────────────────────────────────┘

Regla: Si el test "antes" es un setup de recurso → usa FIXTURES
       Si simplemente quieres orden → usa DEPENDS
```

### 10.6 Comunicacion entre setup y tests

CTest ejecuta setup, test y cleanup como **procesos separados**. No comparten memoria. La comunicacion es via el filesystem:

```c
// setup: escribe a un archivo conocido
#define STATE_FILE "/tmp/ctest_state_db_path.txt"

// En setup_db.c:
FILE *f = fopen(STATE_FILE, "w");
fprintf(f, "/tmp/test_db_%d.dat", getpid());
fclose(f);

// En test_user_crud.c:
FILE *f = fopen(STATE_FILE, "r");
char db_path[256];
fscanf(f, "%255s", db_path);
fclose(f);
// Ahora usar db_path
```

Alternativa: usar variables de entorno con `ENVIRONMENT` (pero es estatica, se define en CMakeLists.txt):

```cmake
set_tests_properties(setup_db test_user_crud test_user_search cleanup_db PROPERTIES
    ENVIRONMENT "TEST_DB_PATH=/tmp/test_db.dat"
)
```

---

## 11. Integracion con frameworks — Unity y CMocka con CTest

### 11.1 CTest + Unity

```cmake
cmake_minimum_required(VERSION 3.14)
project(unity_ctest_example C)
set(CMAKE_C_STANDARD 11)

# --- Traer Unity con FetchContent ---
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

# --- Libreria bajo test ---
add_library(mylib src/calculator.c)
target_include_directories(mylib PUBLIC include)

# --- Tests ---
enable_testing()

function(add_unity_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib unity)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Unity imprime "N Tests M Failures" al final
    # Podemos usar FAIL_REGULAR_EXPRESSION para mayor seguridad
    set_tests_properties(${TEST_NAME} PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL"
        TIMEOUT 10
        LABELS "unit"
    )

    # Argumentos extra = labels adicionales
    if(ARGN)
        get_tests_properties(${TEST_NAME} PROPERTIES LABELS EXISTING_LABELS)
        set_tests_properties(${TEST_NAME} PROPERTIES
            LABELS "${EXISTING_LABELS};${ARGN}"
        )
    endif()
endfunction()

add_unity_test(test_calculator tests/test_calculator.c)
add_unity_test(test_tokenizer  tests/test_tokenizer.c)
```

```c
// tests/test_calculator.c (usando Unity)
#include "unity.h"
#include "calculator.h"

void setUp(void) {}
void tearDown(void) {}

void test_add(void) {
    TEST_ASSERT_EQUAL_INT(5, calculator_add(2, 3));
}

void test_subtract(void) {
    TEST_ASSERT_EQUAL_INT(1, calculator_subtract(3, 2));
}

void test_divide_by_zero(void) {
    TEST_ASSERT_EQUAL_INT(-1, calculator_divide(10, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add);
    RUN_TEST(test_subtract);
    RUN_TEST(test_divide_by_zero);
    return UNITY_END();
}
```

```bash
$ cd build && ctest --output-on-failure -j$(nproc)
# Test project /home/user/build
#     Start 1: test_calculator
#     Start 2: test_tokenizer
# 1/2 Test #1: test_calculator ..................   Passed    0.00 sec
# 2/2 Test #2: test_tokenizer ...................   Passed    0.00 sec
#
# 100% tests passed, 0 tests failed out of 2
```

### 11.2 CTest + CMocka

```cmake
cmake_minimum_required(VERSION 3.14)
project(cmocka_ctest_example C)
set(CMAKE_C_STANDARD 11)

# --- CMocka via FetchContent ---
include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG        cmocka-1.1.7
)
set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)

# --- Libreria bajo test ---
add_library(mylib src/user_service.c)
target_include_directories(mylib PUBLIC include)

# --- Tests ---
enable_testing()

function(add_cmocka_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib cmocka-static)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 10
        LABELS "unit"
        # CMocka detecta leaks — si hay, aparece en stderr
        FAIL_REGULAR_EXPRESSION "Blocks allocated|LEAKED"
    )
endfunction()

add_cmocka_test(test_user_service tests/test_user_service.c)
add_cmocka_test(test_auth         tests/test_auth.c)
```

### 11.3 CTest + assert.h simple

```cmake
# El caso mas simple: test ejecutable con assert.h
add_executable(test_simple tests/test_simple.c)
target_link_libraries(test_simple PRIVATE mylib)
add_test(NAME test_simple COMMAND test_simple)

# assert() llama abort() → exit code != 0 → CTest ve FAIL
# Funciona sin configuracion adicional
```

### 11.4 CTest + scripts (test no compilados)

```cmake
# CTest puede ejecutar CUALQUIER comando, no solo ejecutables C
add_test(
    NAME test_valgrind_leak_check
    COMMAND valgrind --leak-check=full --error-exitcode=1
            $<TARGET_FILE:test_math>
)
set_tests_properties(test_valgrind_leak_check PROPERTIES
    LABELS "memory;slow"
    TIMEOUT 60
)

# Script de shell como test
add_test(
    NAME test_install_headers
    COMMAND ${CMAKE_SOURCE_DIR}/tests/check_headers.sh
)

# Python test
add_test(
    NAME test_python_integration
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/integration_test.py
)
```

---

## 12. Subdirectorios — tests distribuidos en el proyecto

### 12.1 Proyecto con subdirectorios

```
proyecto/
├── CMakeLists.txt              ← raiz: enable_testing() aqui
├── src/
│   ├── CMakeLists.txt
│   ├── math/
│   │   ├── CMakeLists.txt
│   │   └── math.c
│   └── strings/
│       ├── CMakeLists.txt
│       └── strings.c
├── include/
│   ├── math.h
│   └── strings.h
└── tests/
    ├── CMakeLists.txt          ← add_test() aqui
    ├── math/
    │   ├── CMakeLists.txt      ← o aqui (sub-subdirectorio)
    │   └── test_math.c
    └── strings/
        ├── CMakeLists.txt
        └── test_strings.c
```

**CMakeLists.txt raiz**:

```cmake
cmake_minimum_required(VERSION 3.14)
project(multi_module C)
set(CMAKE_C_STANDARD 11)

add_subdirectory(src)

# enable_testing() en el raiz aplica a todos los subdirectorios
enable_testing()
add_subdirectory(tests)
```

**src/CMakeLists.txt**:

```cmake
add_subdirectory(math)
add_subdirectory(strings)
```

**src/math/CMakeLists.txt**:

```cmake
add_library(math_lib math.c)
target_include_directories(math_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
```

**src/strings/CMakeLists.txt**:

```cmake
add_library(strings_lib strings.c)
target_include_directories(strings_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
```

**tests/CMakeLists.txt**:

```cmake
add_subdirectory(math)
add_subdirectory(strings)
```

**tests/math/CMakeLists.txt**:

```cmake
add_executable(test_math test_math.c)
target_link_libraries(test_math PRIVATE math_lib)
add_test(NAME test_math COMMAND test_math)
set_tests_properties(test_math PROPERTIES LABELS "unit;math")
```

**tests/strings/CMakeLists.txt**:

```cmake
add_executable(test_strings test_strings.c)
target_link_libraries(test_strings PRIVATE strings_lib)
add_test(NAME test_strings COMMAND test_strings)
set_tests_properties(test_strings PROPERTIES LABELS "unit;strings")
```

### 12.2 Opcion BUILD_TESTING

Convencion estandar en proyectos CMake: hacer los tests opcionales:

```cmake
# CMakeLists.txt raiz
cmake_minimum_required(VERSION 3.14)
project(myproject C)

# ...

# Opcion que el usuario puede desactivar
option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

```bash
# Con tests (default)
cmake -B build
cmake --build build
cd build && ctest

# Sin tests (build mas rapido)
cmake -B build -DBUILD_TESTING=OFF
cmake --build build
# No hay tests
```

Nota: `include(CTest)` define `BUILD_TESTING` automaticamente. Si usas `enable_testing()` directamente, definilas tu con `option()`.

### 12.3 Tests junto al codigo fuente

Algunos proyectos prefieren que los tests esten junto al codigo que testean:

```
src/
├── math/
│   ├── CMakeLists.txt
│   ├── math.c
│   ├── math.h
│   └── test_math.c    ← test junto al codigo
└── strings/
    ├── CMakeLists.txt
    ├── strings.c
    ├── strings.h
    └── test_strings.c
```

```cmake
# src/math/CMakeLists.txt
add_library(math_lib math.c)
target_include_directories(math_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_TESTING)
    add_executable(test_math test_math.c)
    target_link_libraries(test_math PRIVATE math_lib)
    add_test(NAME test_math COMMAND test_math)
endif()
```

---

## 13. Sanitizers con CTest

### 13.1 Configurar sanitizers en CMake

Los sanitizers de GCC/Clang son una de las herramientas mas poderosas para testing en C. CTest se integra naturalmente:

```cmake
# Opcion para habilitar sanitizers
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()
```

```bash
# Compilar con ASan
cmake -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan

# Correr tests — ASan reporta errores como exit code != 0
cd build-asan && ctest --output-on-failure

# Si hay un buffer overflow, CTest vera:
# 1/3 Test #1: test_stack .......................***Failed    0.01 sec
# Output:
# =================================================================
# ==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
# ...
```

### 13.2 FAIL_REGULAR_EXPRESSION para sanitizers

```cmake
# Deteccion extra via output (por si el sanitizer no cambia el exit code)
function(add_sanitized_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE mylib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 30
        FAIL_REGULAR_EXPRESSION
            "AddressSanitizer;LeakSanitizer;UndefinedBehaviorSanitizer;ThreadSanitizer;runtime error"
    )
endfunction()
```

### 13.3 Valgrind como test separado

```cmake
find_program(VALGRIND_EXECUTABLE valgrind)

if(VALGRIND_EXECUTABLE)
    function(add_valgrind_test TEST_NAME TARGET)
        add_test(
            NAME ${TEST_NAME}_valgrind
            COMMAND ${VALGRIND_EXECUTABLE}
                --leak-check=full
                --show-leak-kinds=all
                --track-origins=yes
                --error-exitcode=1
                $<TARGET_FILE:${TARGET}>
        )
        set_tests_properties(${TEST_NAME}_valgrind PROPERTIES
            LABELS "memory;slow"
            TIMEOUT 120
            FAIL_REGULAR_EXPRESSION "definitely lost|indirectly lost|possibly lost"
        )
    endfunction()

    # Para cada test, generar version valgrind
    add_valgrind_test(test_stack test_stack)
    add_valgrind_test(test_queue test_queue)
endif()
```

```bash
# Correr solo tests de memoria
ctest -L "memory" --output-on-failure

# Listar todos los tests (incluyendo valgrind)
ctest -N
# Test #1: test_stack
# Test #2: test_queue
# Test #3: test_stack_valgrind
# Test #4: test_queue_valgrind
```

---

## 14. Salida XML y CI — --output-junit

### 14.1 Generar JUnit XML

```bash
# CMake 3.21+
ctest --output-junit results.xml --output-on-failure -j$(nproc)
```

Formato generado:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
  <testsuite name="CTest" tests="5" failures="1" disabled="0" errors="0"
             time="0.123">
    <testcase name="test_math" classname="CTest" time="0.002" status="run"/>
    <testcase name="test_strings" classname="CTest" time="0.001" status="run"/>
    <testcase name="test_stack" classname="CTest" time="0.003" status="run"/>
    <testcase name="test_queue" classname="CTest" time="0.002" status="run">
      <failure message="Exit code: 1">
        test_queue: tests/test_queue.c:45: test_dequeue_empty:
        Assertion `queue_dequeue(q) == -1' failed.
      </failure>
    </testcase>
  </testsuite>
</testsuites>
```

### 14.2 GitHub Actions completo

```yaml
name: C Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        build_type: [Debug, Release]
        sanitizer: [none, asan, ubsan]

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: sudo apt-get update && sudo apt-get install -y cmake gcc

      - name: Configure
        run: |
          CMAKE_ARGS="-DCMAKE_BUILD_TYPE=${{ matrix.build_type }}"
          if [ "${{ matrix.sanitizer }}" = "asan" ]; then
            CMAKE_ARGS="$CMAKE_ARGS -DENABLE_ASAN=ON"
          elif [ "${{ matrix.sanitizer }}" = "ubsan" ]; then
            CMAKE_ARGS="$CMAKE_ARGS -DENABLE_UBSAN=ON"
          fi
          cmake -B build $CMAKE_ARGS

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Test
        working-directory: build
        run: ctest --output-on-failure -j$(nproc) --output-junit results.xml

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results-${{ matrix.build_type }}-${{ matrix.sanitizer }}
          path: build/results.xml
```

### 14.3 GitLab CI

```yaml
# .gitlab-ci.yml
stages:
  - build
  - test

build:
  stage: build
  image: gcc:latest
  script:
    - apt-get update && apt-get install -y cmake
    - cmake -B build -DCMAKE_BUILD_TYPE=Debug
    - cmake --build build -j$(nproc)
  artifacts:
    paths:
      - build/

test:
  stage: test
  image: gcc:latest
  needs: [build]
  script:
    - cd build
    - ctest --output-on-failure -j$(nproc) --output-junit results.xml
  artifacts:
    when: always
    reports:
      junit: build/results.xml
```

### 14.4 Pre-CMake 3.21 — generar XML con CTest/CDash mode

Si tu CMake es anterior a 3.21 (sin `--output-junit`):

```cmake
# Usar include(CTest) en lugar de enable_testing()
include(CTest)
# Esto habilita el modo CDash que genera XML
```

```bash
# Ejecutar en modo Test (genera XML en Testing/)
ctest -T Test

# Los resultados estan en:
# Testing/TAG
# Testing/Temporary/LastTest.log
# Testing/20260404-1200/Test.xml
```

---

## 15. CMake presets para testing

### 15.1 CMakePresets.json

CMake 3.21 introdujo presets: archivos JSON que definen configuraciones predefinidas. Los **testPresets** configuran CTest:

```json
{
    "version": 6,
    "cmakeMinimumRequired": {
        "major": 3,
        "minor": 21,
        "patch": 0
    },
    "configurePresets": [
        {
            "name": "debug",
            "displayName": "Debug",
            "binaryDir": "${sourceDir}/build-debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "BUILD_TESTING": "ON"
            }
        },
        {
            "name": "debug-asan",
            "displayName": "Debug with ASan",
            "inherits": "debug",
            "binaryDir": "${sourceDir}/build-debug-asan",
            "cacheVariables": {
                "ENABLE_ASAN": "ON"
            }
        },
        {
            "name": "release",
            "displayName": "Release",
            "binaryDir": "${sourceDir}/build-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "BUILD_TESTING": "OFF"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "debug",
            "configurePreset": "debug"
        },
        {
            "name": "debug-asan",
            "configurePreset": "debug-asan"
        }
    ],
    "testPresets": [
        {
            "name": "unit",
            "configurePreset": "debug",
            "output": {
                "outputOnFailure": true,
                "verbosity": "default"
            },
            "filter": {
                "include": {
                    "label": "unit"
                }
            },
            "execution": {
                "jobs": 4,
                "timeout": 30,
                "noTestsAction": "error"
            }
        },
        {
            "name": "integration",
            "configurePreset": "debug",
            "output": {
                "outputOnFailure": true
            },
            "filter": {
                "include": {
                    "label": "integration"
                }
            },
            "execution": {
                "jobs": 1,
                "timeout": 120
            }
        },
        {
            "name": "all",
            "configurePreset": "debug",
            "output": {
                "outputOnFailure": true,
                "outputJUnitFile": "results.xml"
            },
            "execution": {
                "jobs": 4,
                "timeout": 60
            }
        },
        {
            "name": "asan",
            "configurePreset": "debug-asan",
            "output": {
                "outputOnFailure": true
            },
            "execution": {
                "jobs": 2,
                "timeout": 120
            }
        }
    ]
}
```

### 15.2 Usar presets

```bash
# Configurar
cmake --preset debug

# Compilar
cmake --build --preset debug

# Testear con preset
ctest --preset unit        # Solo tests unitarios, -j4, timeout 30s
ctest --preset integration # Solo integracion, -j1, timeout 120s
ctest --preset all         # Todo, genera results.xml
ctest --preset asan        # Con ASan

# Listar presets disponibles
ctest --list-presets
# Available test presets:
#   "unit"        - Unit tests
#   "integration" - Integration tests
#   "all"         - All tests with JUnit output
#   "asan"        - Tests with AddressSanitizer
```

### 15.3 CMakeUserPresets.json

Para configuraciones personales (no versionadas en git):

```json
{
    "version": 6,
    "testPresets": [
        {
            "name": "my-fast",
            "configurePreset": "debug",
            "description": "Solo tests rapidos en mi maquina",
            "filter": {
                "include": {
                    "label": "fast"
                }
            },
            "execution": {
                "jobs": 8
            }
        }
    ]
}
```

```gitignore
# .gitignore
CMakeUserPresets.json
```

---

## 16. Ejemplo completo — sistema de inventario

Un sistema de inventario con multiples modulos, tests con Unity, fixtures CTest, labels, sanitizers y presets.

### 16.1 Estructura del proyecto

```
inventory/
├── CMakeLists.txt
├── CMakePresets.json
├── include/
│   ├── product.h
│   ├── warehouse.h
│   └── report.h
├── src/
│   ├── product.c
│   ├── warehouse.c
│   └── report.c
└── tests/
    ├── CMakeLists.txt
    ├── test_product.c
    ├── test_warehouse.c
    ├── test_report.c
    ├── test_integration.c
    ├── setup_test_data.c
    └── cleanup_test_data.c
```

### 16.2 Codigo fuente

**include/product.h**:

```c
#ifndef PRODUCT_H
#define PRODUCT_H

#include <stdbool.h>

#define PRODUCT_NAME_MAX  64
#define PRODUCT_SKU_MAX   16

typedef struct {
    int id;
    char name[PRODUCT_NAME_MAX];
    char sku[PRODUCT_SKU_MAX];
    double price;
    int quantity;
} Product;

// Crear/destruir
Product *product_new(int id, const char *name, const char *sku,
                     double price, int quantity);
void product_free(Product *p);

// Validacion
bool product_is_valid(const Product *p);
bool product_sku_is_valid(const char *sku);

// Operaciones
bool product_update_quantity(Product *p, int delta);
double product_total_value(const Product *p);

// Serializar (para persistencia)
int product_to_line(const Product *p, char *buf, int buf_size);
Product *product_from_line(const char *line);

#endif
```

**src/product.c**:

```c
#include "product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Product *product_new(int id, const char *name, const char *sku,
                     double price, int quantity) {
    if (id <= 0 || !name || !sku || price < 0 || quantity < 0) {
        return NULL;
    }

    Product *p = calloc(1, sizeof(Product));
    if (!p) return NULL;

    p->id = id;
    snprintf(p->name, PRODUCT_NAME_MAX, "%s", name);
    snprintf(p->sku, PRODUCT_SKU_MAX, "%s", sku);
    p->price = price;
    p->quantity = quantity;

    if (!product_is_valid(p)) {
        free(p);
        return NULL;
    }

    return p;
}

void product_free(Product *p) {
    free(p);
}

bool product_is_valid(const Product *p) {
    if (!p) return false;
    if (p->id <= 0) return false;
    if (strlen(p->name) == 0) return false;
    if (!product_sku_is_valid(p->sku)) return false;
    if (p->price < 0) return false;
    if (p->quantity < 0) return false;
    return true;
}

bool product_sku_is_valid(const char *sku) {
    if (!sku) return false;
    int len = strlen(sku);
    if (len < 3 || len >= PRODUCT_SKU_MAX) return false;

    // SKU: letras mayusculas seguidas de digitos (ej: ABC123)
    int i = 0;
    while (i < len && isupper((unsigned char)sku[i])) i++;
    if (i == 0 || i == len) return false;  // necesita letras Y digitos
    while (i < len && isdigit((unsigned char)sku[i])) i++;
    return i == len;  // todo consumido
}

bool product_update_quantity(Product *p, int delta) {
    if (!p) return false;
    int new_qty = p->quantity + delta;
    if (new_qty < 0) return false;
    p->quantity = new_qty;
    return true;
}

double product_total_value(const Product *p) {
    if (!p) return 0.0;
    return p->price * p->quantity;
}

int product_to_line(const Product *p, char *buf, int buf_size) {
    if (!p || !buf || buf_size <= 0) return -1;
    return snprintf(buf, buf_size, "%d|%s|%s|%.2f|%d",
                    p->id, p->name, p->sku, p->price, p->quantity);
}

Product *product_from_line(const char *line) {
    if (!line) return NULL;

    int id, quantity;
    char name[PRODUCT_NAME_MAX];
    char sku[PRODUCT_SKU_MAX];
    double price;

    if (sscanf(line, "%d|%63[^|]|%15[^|]|%lf|%d",
               &id, name, sku, &price, &quantity) != 5) {
        return NULL;
    }

    return product_new(id, name, sku, price, quantity);
}
```

**include/warehouse.h**:

```c
#ifndef WAREHOUSE_H
#define WAREHOUSE_H

#include "product.h"
#include <stdbool.h>

#define WAREHOUSE_MAX_PRODUCTS 1024

typedef struct {
    Product *products[WAREHOUSE_MAX_PRODUCTS];
    int count;
    char name[64];
} Warehouse;

Warehouse *warehouse_new(const char *name);
void warehouse_free(Warehouse *w);

bool warehouse_add_product(Warehouse *w, Product *p);
Product *warehouse_find_by_sku(const Warehouse *w, const char *sku);
Product *warehouse_find_by_id(const Warehouse *w, int id);
bool warehouse_remove_by_sku(Warehouse *w, const char *sku);
int warehouse_count(const Warehouse *w);

// Operaciones de lote
double warehouse_total_value(const Warehouse *w);
int warehouse_low_stock_count(const Warehouse *w, int threshold);

// Persistencia
int warehouse_save(const Warehouse *w, const char *filepath);
Warehouse *warehouse_load(const char *name, const char *filepath);

#endif
```

**src/warehouse.c**:

```c
#include "warehouse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Warehouse *warehouse_new(const char *name) {
    if (!name) return NULL;

    Warehouse *w = calloc(1, sizeof(Warehouse));
    if (!w) return NULL;

    snprintf(w->name, sizeof(w->name), "%s", name);
    return w;
}

void warehouse_free(Warehouse *w) {
    if (!w) return;
    for (int i = 0; i < w->count; i++) {
        product_free(w->products[i]);
    }
    free(w);
}

bool warehouse_add_product(Warehouse *w, Product *p) {
    if (!w || !p) return false;
    if (w->count >= WAREHOUSE_MAX_PRODUCTS) return false;

    // Verificar SKU duplicado
    if (warehouse_find_by_sku(w, p->sku)) return false;

    w->products[w->count++] = p;
    return true;
}

Product *warehouse_find_by_sku(const Warehouse *w, const char *sku) {
    if (!w || !sku) return NULL;
    for (int i = 0; i < w->count; i++) {
        if (strcmp(w->products[i]->sku, sku) == 0) {
            return w->products[i];
        }
    }
    return NULL;
}

Product *warehouse_find_by_id(const Warehouse *w, int id) {
    if (!w || id <= 0) return NULL;
    for (int i = 0; i < w->count; i++) {
        if (w->products[i]->id == id) {
            return w->products[i];
        }
    }
    return NULL;
}

bool warehouse_remove_by_sku(Warehouse *w, const char *sku) {
    if (!w || !sku) return false;
    for (int i = 0; i < w->count; i++) {
        if (strcmp(w->products[i]->sku, sku) == 0) {
            product_free(w->products[i]);
            // Mover ultimo al hueco
            w->products[i] = w->products[w->count - 1];
            w->products[w->count - 1] = NULL;
            w->count--;
            return true;
        }
    }
    return false;
}

int warehouse_count(const Warehouse *w) {
    return w ? w->count : 0;
}

double warehouse_total_value(const Warehouse *w) {
    if (!w) return 0.0;
    double total = 0.0;
    for (int i = 0; i < w->count; i++) {
        total += product_total_value(w->products[i]);
    }
    return total;
}

int warehouse_low_stock_count(const Warehouse *w, int threshold) {
    if (!w) return 0;
    int count = 0;
    for (int i = 0; i < w->count; i++) {
        if (w->products[i]->quantity <= threshold) {
            count++;
        }
    }
    return count;
}

int warehouse_save(const Warehouse *w, const char *filepath) {
    if (!w || !filepath) return -1;

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    fprintf(f, "# Warehouse: %s\n", w->name);
    fprintf(f, "# Products: %d\n", w->count);

    char buf[256];
    for (int i = 0; i < w->count; i++) {
        product_to_line(w->products[i], buf, sizeof(buf));
        fprintf(f, "%s\n", buf);
    }

    fclose(f);
    return w->count;
}

Warehouse *warehouse_load(const char *name, const char *filepath) {
    if (!name || !filepath) return NULL;

    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    Warehouse *w = warehouse_new(name);
    if (!w) {
        fclose(f);
        return NULL;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Saltar comentarios
        if (line[0] == '#') continue;

        // Quitar newline
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Product *p = product_from_line(line);
        if (p) {
            if (!warehouse_add_product(w, p)) {
                product_free(p);
            }
        }
    }

    fclose(f);
    return w;
}
```

**include/report.h**:

```c
#ifndef REPORT_H
#define REPORT_H

#include "warehouse.h"
#include <stdio.h>

typedef struct {
    int total_products;
    int total_units;
    double total_value;
    int low_stock_count;
    int low_stock_threshold;
    char warehouse_name[64];
} InventoryReport;

InventoryReport report_generate(const Warehouse *w, int low_stock_threshold);
int report_print(const InventoryReport *r, FILE *out);
int report_save(const InventoryReport *r, const char *filepath);

#endif
```

**src/report.c**:

```c
#include "report.h"
#include <string.h>

InventoryReport report_generate(const Warehouse *w, int low_stock_threshold) {
    InventoryReport r = {0};

    if (!w) return r;

    snprintf(r.warehouse_name, sizeof(r.warehouse_name), "%s", w->name);
    r.total_products = warehouse_count(w);
    r.total_value = warehouse_total_value(w);
    r.low_stock_threshold = low_stock_threshold;
    r.low_stock_count = warehouse_low_stock_count(w, low_stock_threshold);

    // Calcular unidades totales
    for (int i = 0; i < w->count; i++) {
        r.total_units += w->products[i]->quantity;
    }

    return r;
}

int report_print(const InventoryReport *r, FILE *out) {
    if (!r || !out) return -1;

    int written = 0;
    written += fprintf(out, "=== Inventory Report: %s ===\n", r->warehouse_name);
    written += fprintf(out, "Total products: %d\n", r->total_products);
    written += fprintf(out, "Total units:    %d\n", r->total_units);
    written += fprintf(out, "Total value:    $%.2f\n", r->total_value);
    written += fprintf(out, "Low stock (<%d): %d products\n",
                       r->low_stock_threshold, r->low_stock_count);
    written += fprintf(out, "===========================\n");

    return written;
}

int report_save(const InventoryReport *r, const char *filepath) {
    if (!r || !filepath) return -1;

    FILE *f = fopen(filepath, "w");
    if (!f) return -1;

    int result = report_print(r, f);
    fclose(f);
    return result;
}
```

### 16.3 Tests

**tests/test_product.c** (con assert.h):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "product.h"

// Helper: comparar doubles con tolerancia
#define ASSERT_DOUBLE_EQ(expected, actual) \
    assert(fabs((expected) - (actual)) < 0.001)

// --- Creacion ---

static void test_product_new_valid(void) {
    Product *p = product_new(1, "Widget", "ABC123", 9.99, 100);
    assert(p != NULL);
    assert(p->id == 1);
    assert(strcmp(p->name, "Widget") == 0);
    assert(strcmp(p->sku, "ABC123") == 0);
    ASSERT_DOUBLE_EQ(9.99, p->price);
    assert(p->quantity == 100);
    product_free(p);
    printf("  PASS: product_new valid\n");
}

static void test_product_new_invalid_id(void) {
    assert(product_new(0, "Widget", "ABC123", 9.99, 100) == NULL);
    assert(product_new(-1, "Widget", "ABC123", 9.99, 100) == NULL);
    printf("  PASS: product_new rejects invalid id\n");
}

static void test_product_new_null_name(void) {
    assert(product_new(1, NULL, "ABC123", 9.99, 100) == NULL);
    printf("  PASS: product_new rejects null name\n");
}

static void test_product_new_negative_price(void) {
    assert(product_new(1, "Widget", "ABC123", -1.0, 100) == NULL);
    printf("  PASS: product_new rejects negative price\n");
}

static void test_product_new_negative_quantity(void) {
    assert(product_new(1, "Widget", "ABC123", 9.99, -1) == NULL);
    printf("  PASS: product_new rejects negative quantity\n");
}

// --- SKU validation ---

static void test_sku_valid(void) {
    assert(product_sku_is_valid("ABC123"));
    assert(product_sku_is_valid("X1"));
    assert(product_sku_is_valid("WIDGET01"));
    printf("  PASS: valid SKUs accepted\n");
}

static void test_sku_invalid(void) {
    assert(!product_sku_is_valid(NULL));
    assert(!product_sku_is_valid(""));
    assert(!product_sku_is_valid("abc123"));   // minusculas
    assert(!product_sku_is_valid("123"));       // solo digitos
    assert(!product_sku_is_valid("ABC"));       // solo letras
    assert(!product_sku_is_valid("AB"));        // muy corto (< 3 chars total)
    printf("  PASS: invalid SKUs rejected\n");
}

// --- Operaciones ---

static void test_update_quantity_add(void) {
    Product *p = product_new(1, "Widget", "ABC123", 9.99, 100);
    assert(product_update_quantity(p, 50));
    assert(p->quantity == 150);
    product_free(p);
    printf("  PASS: update_quantity add\n");
}

static void test_update_quantity_subtract(void) {
    Product *p = product_new(1, "Widget", "ABC123", 9.99, 100);
    assert(product_update_quantity(p, -30));
    assert(p->quantity == 70);
    product_free(p);
    printf("  PASS: update_quantity subtract\n");
}

static void test_update_quantity_reject_negative_result(void) {
    Product *p = product_new(1, "Widget", "ABC123", 9.99, 10);
    assert(!product_update_quantity(p, -20));
    assert(p->quantity == 10);  // sin cambio
    product_free(p);
    printf("  PASS: update_quantity rejects negative result\n");
}

static void test_total_value(void) {
    Product *p = product_new(1, "Widget", "ABC123", 9.99, 10);
    ASSERT_DOUBLE_EQ(99.90, product_total_value(p));
    product_free(p);
    printf("  PASS: total_value\n");
}

// --- Serializacion ---

static void test_roundtrip(void) {
    Product *original = product_new(42, "Gadget", "GAD001", 19.95, 50);
    char buf[256];
    product_to_line(original, buf, sizeof(buf));

    Product *restored = product_from_line(buf);
    assert(restored != NULL);
    assert(restored->id == original->id);
    assert(strcmp(restored->name, original->name) == 0);
    assert(strcmp(restored->sku, original->sku) == 0);
    ASSERT_DOUBLE_EQ(original->price, restored->price);
    assert(restored->quantity == original->quantity);

    product_free(original);
    product_free(restored);
    printf("  PASS: serialize/deserialize roundtrip\n");
}

static void test_from_line_invalid(void) {
    assert(product_from_line(NULL) == NULL);
    assert(product_from_line("") == NULL);
    assert(product_from_line("not a product line") == NULL);
    assert(product_from_line("0|Name|SKU1|10.00|5") == NULL);  // id invalido
    printf("  PASS: from_line rejects invalid input\n");
}

int main(void) {
    printf("=== Product tests ===\n");
    test_product_new_valid();
    test_product_new_invalid_id();
    test_product_new_null_name();
    test_product_new_negative_price();
    test_product_new_negative_quantity();
    test_sku_valid();
    test_sku_invalid();
    test_update_quantity_add();
    test_update_quantity_subtract();
    test_update_quantity_reject_negative_result();
    test_total_value();
    test_roundtrip();
    test_from_line_invalid();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

**tests/test_warehouse.c**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "warehouse.h"

#define ASSERT_DOUBLE_EQ(expected, actual) \
    assert(fabs((expected) - (actual)) < 0.001)

static void test_warehouse_new(void) {
    Warehouse *w = warehouse_new("Main");
    assert(w != NULL);
    assert(strcmp(w->name, "Main") == 0);
    assert(warehouse_count(w) == 0);
    warehouse_free(w);
    printf("  PASS: warehouse_new\n");
}

static void test_warehouse_new_null(void) {
    assert(warehouse_new(NULL) == NULL);
    printf("  PASS: warehouse_new rejects NULL\n");
}

static void test_add_and_find(void) {
    Warehouse *w = warehouse_new("Test");
    Product *p = product_new(1, "Widget", "WDG001", 9.99, 100);

    assert(warehouse_add_product(w, p));
    assert(warehouse_count(w) == 1);

    Product *found = warehouse_find_by_sku(w, "WDG001");
    assert(found == p);

    found = warehouse_find_by_id(w, 1);
    assert(found == p);

    warehouse_free(w);
    printf("  PASS: add and find\n");
}

static void test_add_duplicate_sku(void) {
    Warehouse *w = warehouse_new("Test");
    Product *p1 = product_new(1, "Widget A", "WDG001", 9.99, 100);
    Product *p2 = product_new(2, "Widget B", "WDG001", 19.99, 50);

    assert(warehouse_add_product(w, p1));
    assert(!warehouse_add_product(w, p2));  // SKU duplicado
    assert(warehouse_count(w) == 1);

    product_free(p2);  // No fue agregado, liberarlo manualmente
    warehouse_free(w);
    printf("  PASS: rejects duplicate SKU\n");
}

static void test_remove(void) {
    Warehouse *w = warehouse_new("Test");
    Product *p1 = product_new(1, "Widget", "WDG001", 9.99, 100);
    Product *p2 = product_new(2, "Gadget", "GAD001", 19.99, 50);

    warehouse_add_product(w, p1);
    warehouse_add_product(w, p2);
    assert(warehouse_count(w) == 2);

    assert(warehouse_remove_by_sku(w, "WDG001"));
    assert(warehouse_count(w) == 1);
    assert(warehouse_find_by_sku(w, "WDG001") == NULL);
    assert(warehouse_find_by_sku(w, "GAD001") != NULL);

    warehouse_free(w);
    printf("  PASS: remove\n");
}

static void test_remove_nonexistent(void) {
    Warehouse *w = warehouse_new("Test");
    assert(!warehouse_remove_by_sku(w, "NOPE01"));
    warehouse_free(w);
    printf("  PASS: remove nonexistent returns false\n");
}

static void test_total_value(void) {
    Warehouse *w = warehouse_new("Test");
    warehouse_add_product(w, product_new(1, "A", "AAA001", 10.00, 5));
    warehouse_add_product(w, product_new(2, "B", "BBB001", 20.00, 3));

    // 10*5 + 20*3 = 50 + 60 = 110
    ASSERT_DOUBLE_EQ(110.00, warehouse_total_value(w));

    warehouse_free(w);
    printf("  PASS: total_value\n");
}

static void test_low_stock(void) {
    Warehouse *w = warehouse_new("Test");
    warehouse_add_product(w, product_new(1, "A", "AAA001", 10.00, 2));  // low
    warehouse_add_product(w, product_new(2, "B", "BBB001", 10.00, 5));  // ok
    warehouse_add_product(w, product_new(3, "C", "CCC001", 10.00, 3));  // low

    assert(warehouse_low_stock_count(w, 3) == 2);

    warehouse_free(w);
    printf("  PASS: low_stock_count\n");
}

int main(void) {
    printf("=== Warehouse tests ===\n");
    test_warehouse_new();
    test_warehouse_new_null();
    test_add_and_find();
    test_add_duplicate_sku();
    test_remove();
    test_remove_nonexistent();
    test_total_value();
    test_low_stock();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

**tests/test_report.c**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "report.h"

#define ASSERT_DOUBLE_EQ(expected, actual) \
    assert(fabs((expected) - (actual)) < 0.001)

static void test_generate_empty_warehouse(void) {
    Warehouse *w = warehouse_new("Empty");
    InventoryReport r = report_generate(w, 5);

    assert(r.total_products == 0);
    assert(r.total_units == 0);
    ASSERT_DOUBLE_EQ(0.0, r.total_value);
    assert(r.low_stock_count == 0);
    assert(strcmp(r.warehouse_name, "Empty") == 0);

    warehouse_free(w);
    printf("  PASS: generate empty warehouse\n");
}

static void test_generate_with_products(void) {
    Warehouse *w = warehouse_new("Main");
    warehouse_add_product(w, product_new(1, "A", "AAA001", 10.00, 100));
    warehouse_add_product(w, product_new(2, "B", "BBB001", 5.00, 200));
    warehouse_add_product(w, product_new(3, "C", "CCC001", 20.00, 3));

    InventoryReport r = report_generate(w, 5);

    assert(r.total_products == 3);
    assert(r.total_units == 303);
    ASSERT_DOUBLE_EQ(2060.00, r.total_value);
    assert(r.low_stock_count == 1);  // solo C (3 <= 5)

    warehouse_free(w);
    printf("  PASS: generate with products\n");
}

static void test_generate_null_warehouse(void) {
    InventoryReport r = report_generate(NULL, 5);
    assert(r.total_products == 0);
    assert(r.total_units == 0);
    printf("  PASS: generate null warehouse\n");
}

static void test_save_and_verify(void) {
    Warehouse *w = warehouse_new("SaveTest");
    warehouse_add_product(w, product_new(1, "Widget", "WDG001", 15.00, 10));

    InventoryReport r = report_generate(w, 5);

    const char *path = "/tmp/ctest_report_test.txt";
    assert(report_save(&r, path) > 0);

    // Verificar que el archivo existe y contiene el nombre
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    char buf[256];
    int found = 0;
    while (fgets(buf, sizeof(buf), f)) {
        if (strstr(buf, "SaveTest")) found = 1;
    }
    fclose(f);
    assert(found);

    remove(path);
    warehouse_free(w);
    printf("  PASS: save and verify\n");
}

int main(void) {
    printf("=== Report tests ===\n");
    test_generate_empty_warehouse();
    test_generate_with_products();
    test_generate_null_warehouse();
    test_save_and_verify();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

**tests/setup_test_data.c** (CTest fixture setup):

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define TEST_DATA_DIR "/tmp/ctest_inventory_test"
#define TEST_DB_FILE  TEST_DATA_DIR "/warehouse.dat"

int main(void) {
    printf("FIXTURE SETUP: Creating test data directory\n");

    // Crear directorio
    if (mkdir(TEST_DATA_DIR, 0755) != 0) {
        // Puede ya existir de un run anterior
        perror("mkdir (may already exist)");
    }

    // Crear archivo de warehouse con datos de prueba
    FILE *f = fopen(TEST_DB_FILE, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fprintf(f, "# Warehouse: TestWarehouse\n");
    fprintf(f, "# Products: 3\n");
    fprintf(f, "1|Laptop|LAP001|999.99|10\n");
    fprintf(f, "2|Mouse|MOU001|29.99|150\n");
    fprintf(f, "3|Keyboard|KEY001|79.99|2\n");

    fclose(f);

    printf("FIXTURE SETUP: Test data ready at %s\n", TEST_DATA_DIR);
    return 0;
}
```

**tests/cleanup_test_data.c** (CTest fixture cleanup):

```c
#include <stdio.h>
#include <unistd.h>

#define TEST_DATA_DIR "/tmp/ctest_inventory_test"
#define TEST_DB_FILE  TEST_DATA_DIR "/warehouse.dat"

int main(void) {
    printf("FIXTURE CLEANUP: Removing test data\n");

    unlink(TEST_DB_FILE);
    rmdir(TEST_DATA_DIR);

    printf("FIXTURE CLEANUP: Done\n");
    return 0;
}
```

**tests/test_integration.c** (usa la fixture):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "warehouse.h"
#include "report.h"

#define ASSERT_DOUBLE_EQ(expected, actual) \
    assert(fabs((expected) - (actual)) < 0.001)

#define TEST_DB_FILE "/tmp/ctest_inventory_test/warehouse.dat"

static void test_load_warehouse(void) {
    Warehouse *w = warehouse_load("TestWarehouse", TEST_DB_FILE);
    assert(w != NULL);
    assert(warehouse_count(w) == 3);

    Product *laptop = warehouse_find_by_sku(w, "LAP001");
    assert(laptop != NULL);
    assert(strcmp(laptop->name, "Laptop") == 0);
    ASSERT_DOUBLE_EQ(999.99, laptop->price);

    warehouse_free(w);
    printf("  PASS: load warehouse from file\n");
}

static void test_save_and_reload(void) {
    Warehouse *w = warehouse_load("TestWarehouse", TEST_DB_FILE);
    assert(w != NULL);

    // Agregar un producto
    Product *p = product_new(4, "Monitor", "MON001", 399.99, 5);
    assert(warehouse_add_product(w, p));

    // Guardar
    const char *save_path = "/tmp/ctest_inventory_test/warehouse_save.dat";
    assert(warehouse_save(w, save_path) == 4);
    warehouse_free(w);

    // Recargar
    Warehouse *w2 = warehouse_load("Reloaded", save_path);
    assert(w2 != NULL);
    assert(warehouse_count(w2) == 4);
    assert(warehouse_find_by_sku(w2, "MON001") != NULL);

    warehouse_free(w2);
    remove(save_path);
    printf("  PASS: save and reload\n");
}

static void test_full_workflow(void) {
    // Cargar → modificar → generar reporte
    Warehouse *w = warehouse_load("TestWarehouse", TEST_DB_FILE);
    assert(w != NULL);

    // Actualizar stock del mouse
    Product *mouse = warehouse_find_by_sku(w, "MOU001");
    assert(mouse != NULL);
    assert(product_update_quantity(mouse, -100));

    // Generar reporte
    InventoryReport r = report_generate(w, 5);
    assert(r.total_products == 3);
    assert(r.low_stock_count == 2);  // Keyboard(2) y Mouse(50)... no, Mouse=50

    // Mouse ahora tiene 50, Keyboard tiene 2
    // low stock (<=5): solo Keyboard
    assert(r.low_stock_count == 1);

    warehouse_free(w);
    printf("  PASS: full workflow\n");
}

int main(void) {
    printf("=== Integration tests ===\n");
    test_load_warehouse();
    test_save_and_reload();
    test_full_workflow();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 16.4 CMakeLists.txt raiz

```cmake
cmake_minimum_required(VERSION 3.14)
project(inventory C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Opciones ---
option(BUILD_TESTING "Build tests" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# --- Sanitizers ---
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# --- Warnings ---
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Libreria ---
add_library(inventory_lib
    src/product.c
    src/warehouse.c
    src/report.c
)
target_include_directories(inventory_lib PUBLIC include)

# --- Tests ---
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### 16.5 tests/CMakeLists.txt

```cmake
# --- Funcion helper ---
function(add_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE inventory_lib)
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "unit"
        TIMEOUT 10
        PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
        FAIL_REGULAR_EXPRESSION "AddressSanitizer;LeakSanitizer;UndefinedBehavior"
    )
endfunction()

# --- Tests unitarios ---
add_unit_test(test_product   test_product.c)
add_unit_test(test_warehouse test_warehouse.c)
add_unit_test(test_report    test_report.c)

# --- Fixture: TestData ---
add_executable(setup_test_data setup_test_data.c)
add_test(NAME setup_test_data COMMAND setup_test_data)
set_tests_properties(setup_test_data PROPERTIES
    FIXTURES_SETUP "TestData"
    LABELS "fixture"
)

add_executable(cleanup_test_data cleanup_test_data.c)
add_test(NAME cleanup_test_data COMMAND cleanup_test_data)
set_tests_properties(cleanup_test_data PROPERTIES
    FIXTURES_CLEANUP "TestData"
    LABELS "fixture"
)

# --- Test de integracion (requiere fixture) ---
add_executable(test_integration test_integration.c)
target_link_libraries(test_integration PRIVATE inventory_lib)
target_include_directories(test_integration PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME test_integration COMMAND test_integration)
set_tests_properties(test_integration PROPERTIES
    FIXTURES_REQUIRED "TestData"
    LABELS "integration"
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
    FAIL_REGULAR_EXPRESSION "AddressSanitizer;LeakSanitizer;UndefinedBehavior"
)

# --- Test con valgrind (si disponible) ---
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
    add_test(
        NAME test_product_valgrind
        COMMAND ${VALGRIND_EXECUTABLE}
            --leak-check=full
            --show-leak-kinds=all
            --error-exitcode=1
            $<TARGET_FILE:test_product>
    )
    set_tests_properties(test_product_valgrind PROPERTIES
        LABELS "memory;slow"
        TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "definitely lost|indirectly lost|possibly lost"
    )

    add_test(
        NAME test_warehouse_valgrind
        COMMAND ${VALGRIND_EXECUTABLE}
            --leak-check=full
            --show-leak-kinds=all
            --error-exitcode=1
            $<TARGET_FILE:test_warehouse>
    )
    set_tests_properties(test_warehouse_valgrind PROPERTIES
        LABELS "memory;slow"
        TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "definitely lost|indirectly lost|possibly lost"
    )
endif()
```

### 16.6 Sesion de uso completa

```bash
# --- Configurar y compilar ---
$ mkdir build && cd build
$ cmake .. -DCMAKE_BUILD_TYPE=Debug
-- The C compiler identification is GNU 13.2.0
-- Configuring done
-- Generating done
-- Build files have been written to: /home/user/inventory/build

$ cmake --build . -j$(nproc)
[  8%] Building C object src/product.c
[ 16%] Building C object src/warehouse.c
[ 25%] Building C object src/report.c
...
[100%] Built target test_integration

# --- Listar tests ---
$ ctest -N
Test project /home/user/inventory/build
  Test #1: test_product
  Test #2: test_warehouse
  Test #3: test_report
  Test #4: setup_test_data
  Test #5: cleanup_test_data
  Test #6: test_integration
  Test #7: test_product_valgrind
  Test #8: test_warehouse_valgrind

Total Tests: 8

# --- Ejecutar todos ---
$ ctest --output-on-failure -j$(nproc)
Test project /home/user/inventory/build
    Start 1: test_product
    Start 2: test_warehouse
    Start 3: test_report
    Start 4: setup_test_data
1/8 Test #1: test_product .....................   Passed    0.00 sec
2/8 Test #2: test_warehouse ...................   Passed    0.00 sec
3/8 Test #3: test_report ......................   Passed    0.00 sec
4/8 Test #4: setup_test_data ..................   Passed    0.00 sec
    Start 6: test_integration
5/8 Test #6: test_integration .................   Passed    0.00 sec
    Start 5: cleanup_test_data
6/8 Test #5: cleanup_test_data ................   Passed    0.00 sec
    Start 7: test_product_valgrind
    Start 8: test_warehouse_valgrind
7/8 Test #7: test_product_valgrind ............   Passed    0.45 sec
8/8 Test #8: test_warehouse_valgrind ..........   Passed    0.52 sec

100% tests passed, 0 tests failed out of 8

Total Test time (real) =   0.78 sec

# --- Solo tests unitarios ---
$ ctest -L "unit" --output-on-failure
1/3 Test #1: test_product .....................   Passed    0.00 sec
2/3 Test #2: test_warehouse ...................   Passed    0.00 sec
3/3 Test #3: test_report ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 3

# --- Solo test de integracion (incluye fixture automaticamente) ---
$ ctest -R "test_integration" --output-on-failure
    Start 4: setup_test_data
1/3 Test #4: setup_test_data ..................   Passed    0.00 sec
    Start 6: test_integration
2/3 Test #6: test_integration .................   Passed    0.00 sec
    Start 5: cleanup_test_data
3/3 Test #5: cleanup_test_data ................   Passed    0.00 sec

# Nota: CTest incluyo setup y cleanup automaticamente

# --- Con sanitizers ---
$ cd .. && cmake -B build-asan -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build-asan
$ cd build-asan && ctest --output-on-failure -j$(nproc)

# --- Generar JUnit XML para CI ---
$ ctest --output-on-failure --output-junit results.xml
$ cat results.xml
# (XML con resultados individuales por test)
```

---

## 17. CTest vs Rust cargo test vs Go testing

| Aspecto | CTest (C) | cargo test (Rust) | go test (Go) |
|---------|-----------|-------------------|--------------|
| **Integrado en** | CMake (build system) | Cargo (build + package manager) | go tool (compilador) |
| **Descubrimiento** | `add_test()` en CMakeLists.txt | `#[test]` attribute | `func Test*(t *testing.T)` |
| **Automatismo** | Hay que declarar cada test | 100% automatico | 100% automatico |
| **Granularidad** | Un test = un ejecutable | Un test = una funcion | Un test = una funcion |
| **Paralelo** | `-j N` por ejecutable | `--test-threads N` por funcion | `-parallel N` por funcion |
| **Labels/filtros** | Labels + regex name | `#[ignore]` + `--ignored` | `-run regex` + `-short` |
| **Fixtures** | FIXTURES_SETUP/CLEANUP (proceso) | `once_cell`, constructor | `TestMain` |
| **Timeout** | Propiedad TIMEOUT | No builtin (crate timeout) | `-timeout 30s` |
| **Output format** | JUnit XML | JSON con `--format json` | JSON con `-json` |
| **CI integration** | Buena (JUnit XML es estandar) | Buena | Buena |
| **Framework agnostico** | Si (cualquier ejecutable) | No (solo Rust test) | No (solo Go test) |
| **Retry** | `--repeat until-pass:N` | No builtin | No builtin (via gotestsum) |
| **Coverage** | Separado (gcov/llvm-cov) | `--coverage` (Nightly) / tarpaulin | `-cover` integrado |

### 17.1 Ventaja unica de CTest: framework agnostico

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                                                                  │
│  CTest puede ejecutar CUALQUIER COSA como test:                                 │
│                                                                                  │
│  • Ejecutable C compilado con Unity                                             │
│  • Ejecutable C compilado con CMocka                                            │
│  • Ejecutable C compilado con assert.h                                          │
│  • Script de Python                                                             │
│  • Script de Shell                                                              │
│  • Valgrind wrapping un ejecutable                                              │
│  • Cualquier programa que retorne 0=pass, !=0=fail                              │
│                                                                                  │
│  Esto es UNICO. En Rust/Go, los tests DEBEN ser escritos en el lenguaje        │
│  del proyecto. CTest es un meta-runner.                                         │
│                                                                                  │
│  Desventaja: CTest no sabe cuantos sub-tests hay dentro de cada ejecutable.     │
│  Para CTest, cada add_test() es UN test atomic. Si un ejecutable contiene       │
│  50 assertions y la #30 falla, CTest solo ve "test_X: FAILED".                 │
│  La granularidad fina depende del framework interno (Unity/CMocka).             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 17.2 Rust: equivalentes

```rust
// Rust no necesita CTest — cargo test es el test runner

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_factorial_five() {
        assert_eq!(factorial(5), 120);
    }

    #[test]
    #[ignore]  // Equivalente a label "slow" + ctest -LE "slow"
    fn test_slow_computation() {
        // ...
    }
}

// cargo test               → todos los tests (excepto #[ignore])
// cargo test -- --ignored  → solo los ignorados
// cargo test -- test_factorial  → filtro por nombre (como ctest -R)
// cargo test -j 4          → paralelo (como ctest -j 4)
```

### 17.3 Go: equivalentes

```go
// go test tampoco necesita CTest

func TestFactorial(t *testing.T) {
    if factorial(5) != 120 {
        t.Errorf("factorial(5) = %d, want 120", factorial(5))
    }
}

func TestSlowComputation(t *testing.T) {
    if testing.Short() {
        t.Skip("skipping in short mode")  // Equivalente a label filtering
    }
    // ...
}

// go test ./...              → todos
// go test -run Factorial     → filtro (como ctest -R)
// go test -short             → excluir lentos (como ctest -LE "slow")
// go test -parallel 4        → paralelo (como ctest -j 4)
// go test -timeout 30s       → timeout (como ctest --timeout 30)
// go test -json              → output JSON (como ctest --output-junit)
```

---

## 18. Patrones avanzados

### 18.1 Un add_test() por sub-test (granularidad fina)

El problema de "un ejecutable = un test CTest" se puede resolver si el test acepta argumentos:

```c
// tests/test_math_granular.c
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "math_utils.h"

static void test_factorial_zero(void)     { assert(factorial(0) == 1); }
static void test_factorial_five(void)     { assert(factorial(5) == 120); }
static void test_factorial_negative(void) { assert(factorial(-1) == -1); }
static void test_fibonacci_zero(void)     { assert(fibonacci(0) == 0); }
static void test_fibonacci_ten(void)      { assert(fibonacci(10) == 55); }

typedef struct {
    const char *name;
    void (*func)(void);
} TestEntry;

static TestEntry tests[] = {
    {"factorial_zero",     test_factorial_zero},
    {"factorial_five",     test_factorial_five},
    {"factorial_negative", test_factorial_negative},
    {"fibonacci_zero",     test_fibonacci_zero},
    {"fibonacci_ten",      test_fibonacci_ten},
};

#define NUM_TESTS (sizeof(tests) / sizeof(tests[0]))

int main(int argc, char *argv[]) {
    if (argc == 1) {
        // Sin argumentos: correr todos
        for (int i = 0; i < (int)NUM_TESTS; i++) {
            printf("  RUN: %s\n", tests[i].name);
            tests[i].func();
            printf("  PASS: %s\n", tests[i].name);
        }
        return 0;
    }

    // Con argumento: correr solo ese test
    const char *requested = argv[1];
    for (int i = 0; i < (int)NUM_TESTS; i++) {
        if (strcmp(tests[i].name, requested) == 0) {
            tests[i].func();
            printf("  PASS: %s\n", tests[i].name);
            return 0;
        }
    }

    fprintf(stderr, "Unknown test: %s\n", requested);
    return 1;
}
```

```cmake
add_executable(test_math_granular tests/test_math_granular.c)
target_link_libraries(test_math_granular PRIVATE mylib)

# Registrar cada sub-test como un test CTest independiente
set(MATH_SUBTESTS
    factorial_zero
    factorial_five
    factorial_negative
    fibonacci_zero
    fibonacci_ten
)

foreach(SUBTEST ${MATH_SUBTESTS})
    add_test(
        NAME math.${SUBTEST}
        COMMAND test_math_granular ${SUBTEST}
    )
    set_tests_properties(math.${SUBTEST} PROPERTIES
        LABELS "unit;math"
        TIMEOUT 5
    )
endforeach()
```

```bash
$ ctest -N
Test #1: math.factorial_zero
Test #2: math.factorial_five
Test #3: math.factorial_negative
Test #4: math.fibonacci_zero
Test #5: math.fibonacci_ten

$ ctest -R "factorial" --output-on-failure
1/3 Test #1: math.factorial_zero ..............   Passed
2/3 Test #2: math.factorial_five ..............   Passed
3/3 Test #3: math.factorial_negative ..........   Passed
```

### 18.2 Tests parametrizados via CMake

```cmake
# Tabla de datos para tests parametrizados
set(FACTORIAL_TEST_CASES
    "0:1"
    "1:1"
    "5:120"
    "10:3628800"
    "-1:-1"
)

add_executable(test_factorial_param tests/test_factorial_param.c)
target_link_libraries(test_factorial_param PRIVATE mylib)

foreach(TEST_CASE ${FACTORIAL_TEST_CASES})
    # Separar input:expected
    string(REPLACE ":" ";" PARTS ${TEST_CASE})
    list(GET PARTS 0 INPUT)
    list(GET PARTS 1 EXPECTED)

    add_test(
        NAME factorial_${INPUT}
        COMMAND test_factorial_param ${INPUT} ${EXPECTED}
    )
    set_tests_properties(factorial_${INPUT} PROPERTIES
        LABELS "unit;math;parametric"
        TIMEOUT 5
    )
endforeach()
```

```c
// tests/test_factorial_param.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "math_utils.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input> <expected>\n", argv[0]);
        return 1;
    }

    int input = atoi(argv[1]);
    int expected = atoi(argv[2]);
    int actual = factorial(input);

    printf("factorial(%d) = %d (expected %d)\n", input, actual, expected);
    assert(actual == expected);
    printf("PASS\n");
    return 0;
}
```

### 18.3 Custom test target en CMake

Por defecto, `make test` ejecuta ctest. Pero puedes crear targets personalizados:

```cmake
# Target "test" ya existe si usas enable_testing()
# Crear targets adicionales:

add_custom_target(test-unit
    COMMAND ${CMAKE_CTEST_COMMAND}
        -L "unit"
        --output-on-failure
        -j$\{NPROC\}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running unit tests..."
)

add_custom_target(test-integration
    COMMAND ${CMAKE_CTEST_COMMAND}
        -L "integration"
        --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running integration tests..."
)

add_custom_target(test-all
    COMMAND ${CMAKE_CTEST_COMMAND}
        --output-on-failure
        -j$\{NPROC\}
        --output-junit results.xml
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests..."
)

add_custom_target(test-memory
    COMMAND ${CMAKE_CTEST_COMMAND}
        -L "memory"
        --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running memory tests (valgrind)..."
)
```

```bash
cmake --build build --target test-unit
cmake --build build --target test-integration
cmake --build build --target test-all
cmake --build build --target test-memory
```

### 18.4 Build antes de test (dependencia test → build)

Un problema comun: `ctest` no recompila si cambiaste el codigo. Solucion:

```cmake
# Opcion 1: custom target que compila primero
add_custom_target(check
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -j$\{NPROC\}
    COMMAND ${CMAKE_CTEST_COMMAND}
        --output-on-failure
        -j$\{NPROC\}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and testing..."
)
```

```bash
# Opcion 2: hacerlo desde la linea de comandos
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure -j$(nproc)

# Opcion 3: ctest --build-and-test (menos comun)
ctest --build-and-test \
    /home/user/proyecto \
    /home/user/proyecto/build \
    --build-generator "Unix Makefiles" \
    --test-command ctest --output-on-failure
```

---

## 19. Errores comunes

### 19.1 "No tests were found!!!"

```bash
$ ctest
Test project /home/user/build
No tests were found!!!
```

Causas:

```cmake
# 1. Olvidar enable_testing()
# CMakeLists.txt:
# project(myproject C)
# add_test(...)              ← ignorado silenciosamente
# FIX: agregar enable_testing() ANTES de add_test()

# 2. enable_testing() en subdirectorio pero ctest en raiz
# tests/CMakeLists.txt:
# enable_testing()   ← solo aplica a este subdirectorio
# add_test(...)
# FIX: mover enable_testing() al CMakeLists.txt raiz

# 3. No re-ejecutar cmake despues de agregar tests
# FIX: cmake --build build (a veces) o rm -rf build && cmake -B build

# 4. BUILD_TESTING=OFF
# FIX: cmake -B build -DBUILD_TESTING=ON
```

### 19.2 Test pasa en cmake --build pero falla en ctest

```bash
# Causa comun: WORKING_DIRECTORY diferente
# cmake --build ejecuta en build/
# ctest ejecuta cada test en build/
# Pero si tu test lee archivos con rutas relativas al source dir...

# FIX: usar WORKING_DIRECTORY en add_test() o rutas absolutas
add_test(
    NAME test_config
    COMMAND test_config
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

### 19.3 Tests no se recompilan

```bash
# ctest NO recompila. Solo ejecuta.
# Si cambiaste el codigo:
cmake --build build && cd build && ctest

# O usar el target "check" del patron 18.4
```

### 19.4 Paralelismo rompe tests

```bash
# Sintoma: tests pasan con ctest -j1 pero fallan con ctest -j4
# Causa: tests comparten un recurso (archivo, puerto, variable global)

# FIX 1: RESOURCE_LOCK
set_tests_properties(test_a test_b PROPERTIES
    RESOURCE_LOCK "shared_file"
)

# FIX 2: RUN_SERIAL
set_tests_properties(test_a PROPERTIES RUN_SERIAL TRUE)

# FIX 3: cada test usa su propio archivo temporal
# En el test C:
char tmpfile[64];
snprintf(tmpfile, sizeof(tmpfile), "/tmp/test_%d.dat", getpid());
```

### 19.5 Fixture setup falla silenciosamente

```bash
# Si setup_db falla, los tests que requieren "Database" se SALTAN (Not Run)
# pero cleanup_db AUN se ejecuta
# Verificar con -V:
ctest -R "test_user" -V
# Muestra que setup_db fallo y test_user fue skipped
```

### 19.6 file(GLOB) no detecta tests nuevos

```cmake
# file(GLOB TEST_SOURCES tests/test_*.c) se evalua al hacer cmake ..
# Si agregas tests/test_new.c despues, no se detecta

# FIX: re-ejecutar cmake
cmake -B build   # o cmake .. desde build/
cmake --build build
ctest --test-dir build
```

---

## 20. Programa de practica — sistema de biblioteca

Implementa un sistema de gestion de biblioteca con CTest como test runner.

### 20.1 Requisitos

**Modulo Book** (`book.h` / `book.c`):

```c
typedef struct {
    int id;
    char title[128];
    char author[64];
    char isbn[14];    // ISBN-13
    int year;
    bool available;   // true = disponible para prestamo
} Book;

Book *book_new(int id, const char *title, const char *author,
               const char *isbn, int year);
void book_free(Book *b);
bool book_is_valid(const Book *b);
bool book_isbn_is_valid(const char *isbn);
int book_to_line(const Book *b, char *buf, int buf_size);
Book *book_from_line(const char *line);
```

**Modulo Library** (`library.h` / `library.c`):

```c
#define LIBRARY_MAX_BOOKS 2048

typedef struct {
    Book *books[LIBRARY_MAX_BOOKS];
    int count;
    char name[64];
} Library;

Library *library_new(const char *name);
void library_free(Library *lib);
bool library_add_book(Library *lib, Book *b);
Book *library_find_by_isbn(const Library *lib, const char *isbn);
Book *library_find_by_id(const Library *lib, int id);
int library_search_by_author(const Library *lib, const char *author,
                             Book **results, int max_results);
bool library_checkout(Library *lib, const char *isbn);
bool library_return(Library *lib, const char *isbn);
int library_available_count(const Library *lib);
int library_save(const Library *lib, const char *filepath);
Library *library_load(const char *name, const char *filepath);
```

### 20.2 Estructura esperada del proyecto

```
library/
├── CMakeLists.txt
├── CMakePresets.json           ← presets para test (unit, integration, all, asan)
├── include/
│   ├── book.h
│   └── library.h
├── src/
│   ├── book.c
│   └── library.c
└── tests/
    ├── CMakeLists.txt          ← funcion helper, labels, fixtures, valgrind
    ├── test_book.c             ← unit tests para Book (label: unit)
    ├── test_library.c          ← unit tests para Library (label: unit)
    ├── test_integration.c      ← integracion con archivos (label: integration)
    ├── setup_test_data.c       ← fixture SETUP
    └── cleanup_test_data.c     ← fixture CLEANUP
```

### 20.3 Lo que debes cubrir

1. **CMakeLists.txt raiz**: libreria, opciones (BUILD_TESTING, ENABLE_ASAN, ENABLE_UBSAN), warnings
2. **tests/CMakeLists.txt**: funcion helper `add_unit_test()`, labels (unit, integration, memory), fixture TestData, valgrind tests condicionales
3. **CMakePresets.json**: al menos 4 test presets (unit, integration, all, asan)
4. **Tests unitarios** (18+ tests): validacion de Book (creacion, ISBN, serializacion), operaciones de Library (add, find, search, checkout, return)
5. **Tests de integracion** (5+ tests): load/save con archivos, workflow completo
6. **Fixtures**: setup crea datos de prueba en /tmp, cleanup los elimina
7. **Verificar**: `ctest -L unit`, `ctest -L integration`, `ctest --preset asan`

---

## 21. Ejercicios

### Ejercicio 1: WILL_FAIL y validacion

Crea un programa `validate_input` que lea un archivo de configuracion y retorne exit code 1 si el formato es invalido. Registra tests CTest con `WILL_FAIL TRUE` que verifiquen que el programa rechaza:
- Archivo vacio
- Archivo con lineas mal formateadas
- Archivo con valores fuera de rango

Registra tambien tests normales (sin WILL_FAIL) que verifiquen que acepta archivos validos. Usa `WORKING_DIRECTORY` para que cada test encuentre su archivo de datos.

### Ejercicio 2: RESOURCE_LOCK y paralelismo

Crea 4 tests que todos escriben al mismo archivo `/tmp/shared_counter.txt`. Sin `RESOURCE_LOCK`, fallan con `ctest -j4`. Agrega `RESOURCE_LOCK "counter_file"` y verifica que pasan. Mide la diferencia de tiempo entre `-j1` y `-j4` para entender el impacto.

### Ejercicio 3: Tests parametrizados completos

Usando el patron de la seccion 18.2, crea tests parametrizados para una funcion `int roman_to_int(const char *roman)`. La tabla de test cases debe incluir al menos 15 valores (I=1, IV=4, IX=9, XL=40, MCMXCIV=1994, etc). Cada caso debe ser un test CTest independiente con nombre `roman.<valor>`.

### Ejercicio 4: CI pipeline completo

Crea un proyecto CMake con al menos 3 modulos, tests unitarios y de integracion, fixtures, labels, y un `CMakePresets.json` completo. Escribe un archivo `.github/workflows/test.yml` que:
1. Configure con preset "debug"
2. Build
3. Ejecute tests unitarios (preset "unit")
4. Ejecute tests de integracion (preset "integration")
5. Ejecute tests con ASan (preset "asan")
6. Suba resultados JUnit XML como artefacto
7. Use matrix strategy para GCC y Clang

---

Navegacion:

- Anterior: [T02 — CMocka](../T02_CMocka/README.md)
- Siguiente: [T04 — Comparacion de frameworks](../T04_Comparacion_de_frameworks/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Proximo topico: T04 — Comparacion de frameworks*
