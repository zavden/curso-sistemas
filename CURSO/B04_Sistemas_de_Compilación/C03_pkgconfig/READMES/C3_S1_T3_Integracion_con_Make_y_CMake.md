# T03 — Integracion con Make y CMake

## pkg-config en Makefiles — patron basico

La forma idiomatica de usar pkg-config en un Makefile es capturar
sus flags con `$(shell ...)` y almacenarlos en variables. Se usa
`:=` (asignacion inmediata) para que el comando se ejecute una sola
vez durante la lectura del Makefile, no cada vez que se referencia
la variable:

```makefile
# Capturar flags de compilacion y enlace de una biblioteca:
CFLAGS  := $(shell pkg-config --cflags libpng)
LDLIBS  := $(shell pkg-config --libs libpng)

# Con = (asignacion recursiva), pkg-config se ejecutaria
# CADA VEZ que se referencia CFLAGS o LDLIBS.
# Con :=, se ejecuta UNA sola vez.

app: main.o image.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```

```makefile
# pkg-config produce exactamente los flags que necesita el
# compilador. Por ejemplo:
#
#   pkg-config --cflags libpng   →  -I/usr/include/libpng16
#   pkg-config --libs libpng     →  -lpng16 -lz
#
# Estos flags van en variables separadas:
#   CFLAGS  — flags de compilacion (-I, -D, etc.)
#   LDLIBS  — flags de enlace (-l, -L, etc.)
#
# La separacion es importante: los -l flags DEBEN ir al final
# del comando de enlace, despues de los archivos .o.
# Si van antes, el linker no resuelve los simbolos.
```

```makefile
# MAL — -l flags antes de los objetos:
app: main.o image.o
	$(CC) $(LDLIBS) $^ -o $@
# El linker procesa de izquierda a derecha: cuando ve -lpng16,
# no sabe que simbolos necesita todavia (no ha leido los .o).
# Cuando lee main.o e image.o, los simbolos de libpng quedan
# sin resolver → error de enlace.

# BIEN — -l flags despues de los objetos:
app: main.o image.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
# El linker lee los .o primero, registra los simbolos pendientes,
# y luego busca en -lpng16 para resolverlos.
```

## Verificar existencia en Make

Antes de usar los flags, conviene verificar que la biblioteca
existe. Si pkg-config no encuentra el archivo `.pc`, el comando
falla silenciosamente y las variables quedan vacias, lo que produce
errores confusos durante la compilacion:

```makefile
# Verificar existencia con --exists:
HAS_PNG := $(shell pkg-config --exists libpng && echo yes)

ifeq ($(HAS_PNG),yes)
  PNG_CFLAGS := $(shell pkg-config --cflags libpng)
  PNG_LIBS   := $(shell pkg-config --libs libpng)
else
  $(error libpng not found. Install with: sudo dnf install libpng-devel)
endif
```

```makefile
# Verificar version minima con --atleast-version:
HAS_PNG16 := $(shell pkg-config --atleast-version=1.6.0 libpng && echo yes)

ifeq ($(HAS_PNG16),yes)
  PNG_CFLAGS := $(shell pkg-config --cflags libpng)
  PNG_LIBS   := $(shell pkg-config --libs libpng)
else
  $(error libpng >= 1.6.0 required. Found: \
    $(shell pkg-config --modversion libpng 2>/dev/null || echo "not installed"))
endif
```

```makefile
# Otras opciones de verificacion de version:
#
#   --atleast-version=1.6.0   — >= 1.6.0
#   --exact-version=1.6.40    — == 1.6.40
#   --max-version=2.0.0       — <= 2.0.0
#
# Todas devuelven exit code 0 si la condicion se cumple,
# 1 si no. No producen salida — por eso se agrega && echo yes.
```

```makefile
# Patron con $(warning) en lugar de $(error) para dependencias
# opcionales:
HAS_XML := $(shell pkg-config --exists libxml-2.0 && echo yes)

ifeq ($(HAS_XML),yes)
  XML_CFLAGS := $(shell pkg-config --cflags libxml-2.0)
  XML_LIBS   := $(shell pkg-config --libs libxml-2.0)
  CPPFLAGS   += -DHAS_XML
else
  $(warning libxml-2.0 not found — XML support disabled)
  XML_CFLAGS :=
  XML_LIBS   :=
endif
```

## Multiples dependencias en Make

`pkg-config` acepta multiples paquetes en un solo comando. Esto
es eficiente y ademas elimina flags duplicados automaticamente:

```makefile
# Un solo comando para tres paquetes:
CFLAGS := $(shell pkg-config --cflags libpng zlib openssl)
LDLIBS := $(shell pkg-config --libs libpng zlib openssl)

# pkg-config combina los flags de los tres paquetes y elimina
# duplicados. Si libpng depende de zlib, el -lz no aparece dos veces.
```

```makefile
# Tambien se puede pedir todo junto con --cflags --libs:
ALL_FLAGS := $(shell pkg-config --cflags --libs libpng zlib openssl)

# Pero esto mezcla -I flags con -l flags en una sola variable.
# Es MEJOR separarlos para respetar el orden del comando de enlace:

# BIEN — separados:
PNG_CFLAGS := $(shell pkg-config --cflags libpng zlib)
PNG_LIBS   := $(shell pkg-config --libs libpng zlib)

app: main.o
	$(CC) $(LDFLAGS) $^ $(PNG_LIBS) -o $@
#                        ^^^^^^^^^^
#                        -l flags al final

# MAL — mezclados:
app: main.o
	$(CC) $(ALL_FLAGS) $^ -o $@
# Si ALL_FLAGS contiene -lpng16 -lz, esos -l van ANTES de $^.
# El linker no sabe que simbolos necesita cuando los procesa.
```

```makefile
# Verificar multiples dependencias a la vez:
REQUIRED_PKGS := libpng zlib openssl

# Verificar cada uno individualmente para dar mensajes claros:
$(foreach pkg,$(REQUIRED_PKGS),\
  $(if $(shell pkg-config --exists $(pkg) && echo yes),,\
    $(error $(pkg) not found. Install the development package.)))

# Si todos existen, extraer los flags:
CFLAGS := $(shell pkg-config --cflags $(REQUIRED_PKGS))
LDLIBS := $(shell pkg-config --libs $(REQUIRED_PKGS))
```

## Patron completo de Makefile con pkg-config

Un Makefile real que verifica dependencias, extrae flags, compila
y enlaza. Este patron es reutilizable para cualquier proyecto C
con dependencias gestionadas por pkg-config:

```makefile
# Makefile

# --- Herramientas ---
CC       := gcc
RM       := rm -f

# --- Configuracion ---
TARGET   := imgcompress
SRC_DIR  := src
BUILD_DIR := build

# --- Fuentes y objetos ---
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# --- Dependencias externas ---
REQUIRED_PKGS := libpng zlib

# Verificar que pkg-config existe:
ifeq ($(shell which pkg-config 2>/dev/null),)
  $(error pkg-config not found. Install with: sudo dnf install pkgconf-pkg-config)
endif

# Verificar que cada paquete esta instalado:
$(foreach pkg,$(REQUIRED_PKGS),\
  $(if $(shell pkg-config --exists $(pkg) && echo yes),,\
    $(error $(pkg) not found. Install the -devel package.)))

# Extraer flags (una sola llamada a pkg-config, eficiente):
PKG_CFLAGS := $(shell pkg-config --cflags $(REQUIRED_PKGS))
PKG_LIBS   := $(shell pkg-config --libs $(REQUIRED_PKGS))

# --- Flags de compilacion ---
CFLAGS   := -std=c17 -Wall -Wextra -Wpedantic $(PKG_CFLAGS)
CPPFLAGS := -MMD -MP
LDFLAGS  :=
LDLIBS   := $(PKG_LIBS)

ifdef DEBUG
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

# --- Informacion ---
$(info === Dependencies ===)
$(info   Packages: $(REQUIRED_PKGS))
$(info   CFLAGS:   $(PKG_CFLAGS))
$(info   LIBS:     $(PKG_LIBS))
$(info ====================)

# --- Reglas ---
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)

clean:
	$(RM) -r $(BUILD_DIR) $(TARGET)
```

```bash
# Sesion de uso:

make
# === Dependencies ===
#   Packages: libpng zlib
#   CFLAGS:   -I/usr/include/libpng16
#   LIBS:     -lpng16 -lz
# ====================
# mkdir -p build
# gcc -MMD -MP -std=c17 -Wall -Wextra -Wpedantic -I/usr/include/libpng16 -c src/main.c -o build/main.o
# gcc -MMD -MP -std=c17 -Wall -Wextra -Wpedantic -I/usr/include/libpng16 -c src/compress.c -o build/compress.o
# gcc  build/main.o build/compress.o -lpng16 -lz -o imgcompress

make DEBUG=1
# Recompila con -g -O0 -DDEBUG

make clean
# Elimina build/ y el ejecutable
```

## pkg-config en CMake — PkgConfig module

CMake tiene un modulo built-in llamado `PkgConfig` que proporciona
los comandos `pkg_check_modules()` y `pkg_search_module()`. Para
usarlo, primero hay que cargarlo con `find_package(PkgConfig)`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(myapp C)

# Cargar el modulo PkgConfig:
find_package(PkgConfig REQUIRED)
# Esto verifica que pkg-config esta instalado en el sistema
# y habilita los comandos pkg_check_modules y pkg_search_module.
```

### Forma legacy con variables

```cmake
# pkg_check_modules crea variables con un prefijo dado:
pkg_check_modules(PNG REQUIRED libpng)

# Variables creadas (prefijo PNG):
#   PNG_FOUND          — TRUE si se encontro
#   PNG_VERSION        — version encontrada (e.g., "1.6.40")
#   PNG_INCLUDE_DIRS   — directorios de headers
#   PNG_LIBRARIES      — nombres de bibliotecas
#   PNG_LIBRARY_DIRS   — directorios de bibliotecas
#   PNG_CFLAGS         — flags de compilacion completos
#   PNG_LDFLAGS        — flags de enlace completos
#   PNG_CFLAGS_OTHER   — flags que no son -I ni -D

add_executable(app main.c)
target_include_directories(app PRIVATE ${PNG_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${PNG_LIBRARIES})
target_link_directories(app PRIVATE ${PNG_LIBRARY_DIRS})
# Tres lineas para lo que deberia ser una sola.
# No propaga dependencias transitivas.
```

### Forma moderna con IMPORTED_TARGET

```cmake
# IMPORTED_TARGET crea un target importado PkgConfig::<PREFIX>:
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::PNG)
# Una sola linea. Headers, flags y bibliotecas se propagan
# automaticamente. Siempre preferir esta forma.
```

```cmake
# El target PkgConfig::PNG encapsula:
#   - INTERFACE_INCLUDE_DIRECTORIES  → los -I flags
#   - INTERFACE_LINK_LIBRARIES       → las -l flags
#   - INTERFACE_COMPILE_OPTIONS      → otros flags de compilacion
#
# Se puede inspeccionar:
get_target_property(INC PkgConfig::PNG INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(LIBS PkgConfig::PNG INTERFACE_LINK_LIBRARIES)
message(STATUS "PNG includes: ${INC}")
message(STATUS "PNG libs: ${LIBS}")
```

## pkg_check_modules vs find_package

La decision entre usar `find_package()` directamente o pasar por
`pkg_check_modules()` depende de lo que ofrece cada biblioteca:

```cmake
# Prioridad de busqueda (de mejor a peor soporte):
#
#   1. find_package con Config mode (PackageConfig.cmake)
#      → El paquete se instalo con soporte CMake nativo.
#      → Targets importados de primera clase.
#      Ejemplo: find_package(fmt REQUIRED) → fmt::fmt
#
#   2. find_package con Module mode (FindPackage.cmake)
#      → CMake trae un modulo Find built-in.
#      → Targets importados en la mayoria de modulos modernos.
#      Ejemplo: find_package(ZLIB REQUIRED) → ZLIB::ZLIB
#
#   3. pkg_check_modules via PkgConfig
#      → La biblioteca tiene un .pc pero no soporte CMake.
#      → Funciona bien con IMPORTED_TARGET.
#      Ejemplo: pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)
#               → PkgConfig::PNG
```

```cmake
# Regla practica:
#
# - Intentar find_package() primero. Si CMake encuentra la
#   biblioteca (ya sea con Config o Module mode), usarlo.
#
# - Si find_package() falla (no hay Config.cmake ni Find module),
#   usar pkg_check_modules() como fallback.
#
# - Muchas bibliotecas de Linux tienen .pc pero no soporte CMake.
#   En ese caso, pkg_check_modules es la unica opcion automatica
#   (la alternativa es escribir un Find module propio).
```

```cmake
# Patron de fallback: intentar find_package, caer a pkg-config:
find_package(MyLib QUIET)

if(NOT MyLib_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MyLib REQUIRED IMPORTED_TARGET mylib)
    # Crear un alias para unificar el nombre del target:
    add_library(MyLib::MyLib ALIAS PkgConfig::MyLib)
endif()

add_executable(app main.c)
target_link_libraries(app PRIVATE MyLib::MyLib)
# Funciona sin importar cual metodo encontro la biblioteca.
```

## pkg_search_module

`pkg_search_module()` busca entre varias alternativas y usa la
primera que encuentre. Es util cuando existen multiples
implementaciones de la misma funcionalidad:

```cmake
find_package(PkgConfig REQUIRED)

# Buscar una biblioteca de criptografia: preferir libsodium,
# pero aceptar openssl como alternativa:
pkg_search_module(CRYPTO REQUIRED IMPORTED_TARGET
    libsodium
    openssl
)
# Si libsodium esta instalada, la usa.
# Si no, intenta openssl.
# Si ninguna esta disponible, falla (REQUIRED).

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::CRYPTO)
```

```cmake
# Diferencia con pkg_check_modules:
#
# pkg_check_modules(X REQUIRED a b c)
#   → Busca las TRES: a, b y c. Todas deben existir.
#   → Los flags se combinan en un solo target PkgConfig::X.
#
# pkg_search_module(X REQUIRED a b c)
#   → Busca UNA de las tres: prueba a, luego b, luego c.
#   → Usa la PRIMERA que encuentre.
#   → El target PkgConfig::X tiene los flags de la que encontro.
```

```cmake
# Ejemplo real: buscar un backend de JSON.
# Diferentes distribuciones empaquetan JSON de formas distintas:
pkg_search_module(JSON REQUIRED IMPORTED_TARGET
    json-c
    jansson
    yajl
)
message(STATUS "Using JSON library: ${JSON_MODULE_NAME} ${JSON_VERSION}")

add_executable(app main.c)
target_link_libraries(app PRIVATE PkgConfig::JSON)
```

## QUIET e IMPORTED_TARGET en CMake — detalles

```cmake
# QUIET suprime los mensajes informativos durante la busqueda:
pkg_check_modules(PNG QUIET IMPORTED_TARGET libpng)

# Sin QUIET:
#   -- Checking for module 'libpng'
#   --   Found libpng, version 1.6.40
#
# Con QUIET:
#   (sin salida)

# Util para dependencias opcionales donde no quieres
# ensuciar la salida de configuracion:
pkg_check_modules(PNG QUIET IMPORTED_TARGET libpng)
if(PNG_FOUND)
    message(STATUS "libpng ${PNG_VERSION} found — PNG support enabled")
else()
    message(STATUS "libpng not found — PNG support disabled")
endif()
```

```cmake
# QUIET + REQUIRED: si la busqueda falla, el error SI se muestra.
# QUIET solo suprime los mensajes informativos, no los de error.
pkg_check_modules(PNG QUIET REQUIRED IMPORTED_TARGET libpng)
# Si libpng existe: sin salida.
# Si libpng no existe: error con mensaje claro.
```

```cmake
# IMPORTED_TARGET — detalles de implementacion:
#
# Sin IMPORTED_TARGET, pkg_check_modules solo crea variables.
# Con IMPORTED_TARGET, ademas crea un target importado tipo
# INTERFACE IMPORTED que encapsula todas las propiedades.
#
# Requisito: CMake >= 3.6 para IMPORTED_TARGET.
# En la practica, CMake 3.20+ es el baseline moderno.

# El target creado es de tipo INTERFACE:
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

# Esto es equivalente a que CMake hiciera internamente:
#   add_library(PkgConfig::PNG INTERFACE IMPORTED)
#   set_target_properties(PkgConfig::PNG PROPERTIES
#       INTERFACE_INCLUDE_DIRECTORIES "/usr/include/libpng16"
#       INTERFACE_LINK_LIBRARIES "png16;z"
#       INTERFACE_COMPILE_OPTIONS ""
#   )
```

## Patron completo de CMake con pkg-config

Un CMakeLists.txt real que usa el modulo PkgConfig para encontrar
y enlazar dependencias externas:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(imageproc C)

# --- Dependencia con soporte CMake nativo ---
find_package(ZLIB REQUIRED)

# --- Dependencias via pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng>=1.6.0)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)

# --- Dependencia opcional via pkg-config ---
pkg_check_modules(TIFF QUIET IMPORTED_TARGET libtiff-4)

# --- Biblioteca interna ---
add_library(imgcore STATIC
    src/png_handler.c
    src/jpeg_handler.c
    src/compress.c
)

target_include_directories(imgcore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(imgcore
    PUBLIC  ZLIB::ZLIB          # zlib se propaga a consumidores
    PRIVATE PkgConfig::PNG      # libpng solo la necesita imgcore internamente
    PRIVATE PkgConfig::JPEG     # idem libjpeg
)

target_compile_options(imgcore PRIVATE -Wall -Wextra)

# --- Soporte opcional de TIFF ---
if(TIFF_FOUND)
    target_sources(imgcore PRIVATE src/tiff_handler.c)
    target_link_libraries(imgcore PRIVATE PkgConfig::TIFF)
    target_compile_definitions(imgcore PUBLIC HAS_TIFF)
    message(STATUS "TIFF support: enabled (${TIFF_VERSION})")
else()
    message(STATUS "TIFF support: disabled (libtiff-4 not found)")
endif()

# --- Ejecutable ---
add_executable(imageproc src/main.c)
target_link_libraries(imageproc PRIVATE imgcore)
```

```bash
cmake -B build
# -- Found ZLIB: /usr/lib64/libz.so (found version "1.2.13")
# -- Found PkgConfig: /usr/bin/pkg-config (found version "2.1.0")
# -- Checking for module 'libpng>=1.6.0'
# --   Found libpng, version 1.6.40
# -- Checking for module 'libjpeg'
# --   Found libjpeg, version 3.0.1
# -- Checking for module 'libtiff-4'
# --   Found libtiff-4, version 4.6.0
# -- TIFF support: enabled (4.6.0)

cmake --build build -- VERBOSE=1
# Cada .c se compila con los -I correctos.
# El enlace final incluye -lz -lpng16 -ljpeg -ltiff.
```

## Comparacion de los tres enfoques

La misma dependencia (libpng) se puede resolver de tres formas.
Cada una tiene sus ventajas y contextos apropiados:

```
Aspecto             Manual (gcc)           Make + pkg-config          CMake + PkgConfig
──────────────────  ─────────────────────  ────────────────────────   ─────────────────────────
Descubrimiento      Manual, hardcodeado    Automatico via shell       Automatico via modulo
Portabilidad        Nula (rutas fijas)     Buena (pkg-config          Excelente (CMake abstrae
                                           abstrae rutas)             plataforma + pkg-config)
Verificacion        Falla al compilar      Falla al leer Makefile     Falla al configurar
de dependencias                            ($(error))                 (REQUIRED)
Version minima      Manual                 --atleast-version          >=1.6.0 en el argumento
Dep. transitivas    Manual                 Automatico (pkg-config     Automatico (target
                                           resuelve Requires)         importado propaga)
Escalabilidad       Mala                   Buena para proyectos       Excelente para proyectos
                                           medianos                   grandes
Curva de            Ninguna                Conocer Make + pkg-config  Conocer CMake
aprendizaje
Caso de uso         Scripts rapidos,       Proyectos Unix/Linux       Proyectos multiplataforma,
                    one-liners             tradicionales              bibliotecas reutilizables
```

## Ejemplo completo — tres formas de compilar

El mismo programa compilado de tres formas: manualmente con gcc,
con un Makefile, y con CMake. Todos usando pkg-config para resolver
la misma dependencia (zlib).

```c
// main.c
// Program that compresses a string using zlib and prints
// the original and compressed sizes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int main(void) {
    const char *input = "pkg-config makes dependency management portable "
                        "across Linux distributions and build systems.";
    size_t input_len = strlen(input) + 1;

    uLongf comp_len = compressBound((uLong)input_len);
    unsigned char *compressed = malloc(comp_len);
    if (!compressed) {
        perror("malloc");
        return 1;
    }

    int ret = compress2(compressed, &comp_len,
                        (const unsigned char *)input,
                        (uLong)input_len,
                        Z_BEST_COMPRESSION);
    if (ret != Z_OK) {
        fprintf(stderr, "compress2 failed: %d\n", ret);
        free(compressed);
        return 1;
    }

    printf("Original:   %zu bytes\n", input_len);
    printf("Compressed: %lu bytes\n", (unsigned long)comp_len);
    printf("Ratio:      %.1f%%\n",
           (1.0 - (double)comp_len / (double)input_len) * 100.0);

    // Decompress to verify.
    char restored[256];
    uLongf rest_len = sizeof(restored);
    ret = uncompress((unsigned char *)restored, &rest_len,
                     compressed, comp_len);
    if (ret != Z_OK) {
        fprintf(stderr, "uncompress failed: %d\n", ret);
        free(compressed);
        return 1;
    }

    printf("Restored:   %s\n", restored);
    free(compressed);
    return 0;
}
```

### Forma 1: manual con gcc

```bash
# Obtener flags de pkg-config y pasarlos directamente a gcc:
gcc -std=c17 -Wall $(pkg-config --cflags zlib) \
    -o app main.c \
    $(pkg-config --libs zlib)

# Expandido, esto es algo como:
#   gcc -std=c17 -Wall -o app main.c -lz
#
# Si zlib estuviera en una ruta no estandar:
#   gcc -std=c17 -Wall -I/opt/zlib/include -o app main.c -L/opt/zlib/lib -lz

./app
# Original:   98 bytes
# Compressed: 88 bytes
# Ratio:      10.2%
# Restored:   pkg-config makes dependency management portable ...
```

### Forma 2: Makefile con pkg-config

```makefile
# Makefile
CC     := gcc
CFLAGS := -std=c17 -Wall -Wextra

# --- Dependencia: zlib via pkg-config ---
ifeq ($(shell pkg-config --exists zlib && echo yes),yes)
  CFLAGS += $(shell pkg-config --cflags zlib)
  LDLIBS := $(shell pkg-config --libs zlib)
else
  $(error zlib not found. Install zlib-devel.)
endif

TARGET := app
SRCS   := main.c
OBJS   := $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
```

```bash
make
# gcc -std=c17 -Wall -Wextra -c main.c -o main.o
# gcc  main.o -lz -o app

./app
# Original:   98 bytes
# Compressed: 88 bytes
# Ratio:      10.2%
# Restored:   pkg-config makes dependency management portable ...
```

### Forma 3: CMake con PkgConfig

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(app C)

# Opcion A: usar find_package directo (zlib tiene Find module):
find_package(ZLIB REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE ZLIB::ZLIB)
target_compile_options(app PRIVATE -Wall -Wextra)

# Opcion B: usar PkgConfig (si no hubiera Find module):
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
# add_executable(app main.c)
# target_link_libraries(app PRIVATE PkgConfig::ZLIB)
# target_compile_options(app PRIVATE -Wall -Wextra)

# Ambas opciones producen el mismo resultado al compilar.
```

```bash
cmake -B build
# -- Found ZLIB: /usr/lib64/libz.so (found version "1.2.13")

cmake --build build
# [ 50%] Building C object CMakeFiles/app.dir/main.c.o
# [100%] Linking C executable app

./build/app
# Original:   98 bytes
# Compressed: 88 bytes
# Ratio:      10.2%
# Restored:   pkg-config makes dependency management portable ...
```

```bash
# Verificar que los tres metodos producen el mismo comando final:

# Manual:
#   gcc -std=c17 -Wall -o app main.c -lz

# Make:
#   gcc -std=c17 -Wall -Wextra -c main.c -o main.o
#   gcc main.o -lz -o app

# CMake (VERBOSE=1):
#   gcc -Wall -Wextra -o app main.c -lz
#
# En los tres casos, el programa enlaza contra libz
# y produce el mismo ejecutable funcional.
# La diferencia esta en cuanto trabajo manual requiere
# cada enfoque cuando el proyecto crece.
```

---

## Ejercicios

### Ejercicio 1 — Makefile multi-dependencia con verificacion

```makefile
# Crear un Makefile para un programa que depende de zlib y openssl.
#
# Requisitos:
#   1. Verificar que pkg-config esta disponible en el sistema
#   2. Verificar que zlib esta instalada (REQUIRED)
#   3. Verificar que openssl >= 1.1.0 esta instalada (REQUIRED)
#   4. Extraer CFLAGS y LDLIBS de ambas dependencias
#      usando un solo comando pkg-config
#   5. Compilar src/main.c y src/crypto.c a objetos en build/
#   6. Enlazar los objetos con las bibliotecas
#   7. Imprimir con $(info) un resumen de las dependencias
#      encontradas (nombre y version de cada una)
#   8. Tener targets: all, clean
#
# Verificar:
#   a. make funciona si las dependencias estan instaladas
#   b. Desinstalar openssl-devel y verificar que Make aborta
#      con un mensaje claro antes de intentar compilar
#   c. make V=1 muestra los comandos completos con los -I y -l flags
#
# PISTA: usar --modversion para obtener la version que se imprime
# con $(info). Ejemplo:
#   $(shell pkg-config --modversion zlib)
```

### Ejercicio 2 — CMake con fallback de find_package a pkg-config

```cmake
# Crear un CMakeLists.txt que busque libcurl de dos formas:
#
#   1. Primero intentar find_package(CURL QUIET)
#      (CMake tiene FindCURL.cmake built-in)
#   2. Si no lo encuentra, usar pkg_check_modules como fallback:
#      pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
#   3. Crear un alias para unificar el nombre del target:
#      si se encontro via find_package, el target es CURL::libcurl
#      si se encontro via pkg-config, el target es PkgConfig::CURL
#      Crear un target ALIAS que unifique ambos nombres
#   4. Imprimir con message(STATUS ...) cual metodo se uso
#   5. Compilar un ejecutable que enlace contra el target unificado
#
# Verificar:
#   a. Con curl-devel instalado normalmente, debe encontrarlo
#      via find_package
#   b. Agregar set(CMAKE_DISABLE_FIND_PACKAGE_CURL TRUE) antes
#      del find_package para forzar el fallback a pkg-config
#   c. Verificar con cmake --build build -- VERBOSE=1 que los
#      flags son correctos en ambos casos
#
# PISTA para el alias: add_library(app::curl ALIAS ...) no funciona
# con targets importados. Usar una variable intermedia:
#   set(CURL_TARGET "CURL::libcurl") o set(CURL_TARGET "PkgConfig::CURL")
#   target_link_libraries(app PRIVATE ${CURL_TARGET})
```

### Ejercicio 3 — Tres formas de compilar el mismo programa

```
# Crear un programa en C que use libpng para leer las dimensiones
# de un archivo PNG (ancho y alto) e imprimirlas por stdout.
#
# Compilar ese mismo programa de tres formas:
#
#   1. Manual (build_manual.sh):
#      Un script bash que use gcc + $(pkg-config --cflags --libs libpng)
#
#   2. Makefile (Makefile):
#      Con verificacion de existencia de libpng, extraccion de flags
#      con $(shell pkg-config ...), y targets all/clean
#
#   3. CMake (CMakeLists.txt):
#      Usando find_package(PkgConfig) + pkg_check_modules con
#      IMPORTED_TARGET
#
# Estructura del proyecto:
#   png_info/
#   ├── build_manual.sh
#   ├── Makefile
#   ├── CMakeLists.txt
#   └── src/
#       └── main.c
#
# Verificar:
#   a. Los tres metodos compilan sin errores
#   b. Los tres ejecutables producen la misma salida
#   c. Eliminar libpng-devel y verificar que:
#      - build_manual.sh falla con error de pkg-config
#      - make falla con $(error) antes de compilar
#      - cmake -B build falla durante la configuracion
#      En los tres casos, el mensaje indica que falta libpng
```
