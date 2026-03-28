# T01 — Que es pkg-config

## El problema que resuelve

Compilar un programa que usa una biblioteca externa requiere
conocer varios datos: la ruta de los headers (`-I`), la ruta
de las bibliotecas (`-L`), los flags de enlace (`-l`), y a
veces flags adicionales (`-pthread`, `-D_REENTRANT`). Estas
rutas varian entre distribuciones, versiones del sistema y
formas de instalacion.

```bash
# Compilar a mano contra libpng — hay que saber todo esto:
gcc -I/usr/include/libpng16 -o program main.c -L/usr/lib64 -lpng -lz -lm

# En otra distribucion las rutas pueden ser distintas:
gcc -I/usr/local/include/libpng16 -o program main.c -L/usr/local/lib -lpng16 -lz -lm

# En otra version de la biblioteca, el nombre cambia:
gcc -I/opt/libpng/1.6.40/include -o program main.c -L/opt/libpng/1.6.40/lib -lpng16 -lz

# Hardcodear estas rutas en un Makefile o script es fragil:
# - Se rompe al actualizar la biblioteca
# - Se rompe al compilar en otra maquina
# - Se rompe al cambiar de distribucion
# - Se rompe al instalar en /usr/local en vez de /usr
#
# Se necesita una forma de preguntar al sistema:
# "donde estan los headers y las libs de libpng?"
```

## Que es pkg-config

pkg-config es una herramienta de linea de comandos que consulta
los metadatos de bibliotecas instaladas. Cada biblioteca instala
un archivo `.pc` con su informacion (rutas, flags, version,
dependencias). pkg-config lee ese archivo y produce los flags
correctos para el compilador y el enlazador.

```bash
# En vez de adivinar rutas, se pregunta a pkg-config:
pkg-config --cflags libpng
# -I/usr/include/libpng16

pkg-config --libs libpng
# -lpng16 -lz

# pkg-config devuelve los flags exactos para ESTE sistema.
# El mismo comando produce resultados distintos en cada maquina,
# pero siempre correctos para esa maquina.

# Flujo:
#
#   biblioteca instala .pc  →  pkg-config lee .pc  →  produce flags
#
#   zlib.pc                     pkg-config --libs zlib     -lz
#   libpng.pc                   pkg-config --libs libpng   -lpng16 -lz
#   openssl.pc                  pkg-config --libs openssl  -lssl -lcrypto
```

## Instalar pkg-config

```bash
# Fedora (usa pkgconf como implementacion, compatible con pkg-config):
sudo dnf install pkgconf

# pkgconf es un reemplazo moderno de pkg-config.
# Fedora lo instala por defecto y crea un symlink:
ls -l /usr/bin/pkg-config
# lrwxrwxrwx 1 root root 7 ... /usr/bin/pkg-config -> pkgconf

# Ubuntu/Debian:
sudo apt install pkg-config

# Arch Linux:
sudo pacman -S pkgconf

# Verificar la instalacion:
pkg-config --version
# 2.1.0
#
# Si el comando no se encuentra, el paquete no esta instalado
# o no esta en el PATH.
```

## Archivos .pc

Cada biblioteca que soporta pkg-config instala un archivo `.pc`
en un directorio estandar. El archivo contiene los metadatos
necesarios para compilar y enlazar contra esa biblioteca.

```bash
# Ubicaciones estandar de archivos .pc:
#
#   /usr/lib64/pkgconfig/        ← bibliotecas del sistema (64-bit, Fedora/RHEL)
#   /usr/lib/x86_64-linux-gnu/pkgconfig/  ← bibliotecas del sistema (Debian/Ubuntu)
#   /usr/share/pkgconfig/        ← archivos .pc que no dependen de la arquitectura
#   /usr/local/lib/pkgconfig/    ← bibliotecas instaladas manualmente
#   /usr/local/lib64/pkgconfig/  ← idem, 64-bit
#
# Ver los directorios donde pkg-config busca por defecto:
pkg-config --variable pc_path pkg-config
# /usr/lib64/pkgconfig:/usr/share/pkgconfig

# Listar TODOS los paquetes disponibles:
pkg-config --list-all
# zlib                  zlib - zlib compression library
# libpng                libpng - Loads and saves PNG files
# openssl               OpenSSL - Secure Sockets Layer and ...
# ...
# (puede ser una lista larga)

# Buscar un paquete especifico:
pkg-config --list-all | grep zlib
# zlib                  zlib - zlib compression library

# Buscar paquetes relacionados con png:
pkg-config --list-all | grep png
# libpng                libpng - Loads and saves PNG files
# libpng16              libpng - Loads and saves PNG files

# Ver la ruta exacta del archivo .pc de un paquete:
find /usr/lib64/pkgconfig /usr/share/pkgconfig -name "zlib.pc" 2>/dev/null
# /usr/lib64/pkgconfig/zlib.pc
```

## Anatomia de un archivo .pc

Un archivo `.pc` tiene dos secciones: variables (arriba) y
campos de metadatos (abajo). Las variables se usan como
plantillas dentro de los campos:

```bash
# Ver el contenido de zlib.pc:
cat /usr/lib64/pkgconfig/zlib.pc
```

```bash
# Contenido tipico de zlib.pc:
#
# prefix=/usr
# exec_prefix=${prefix}
# libdir=${exec_prefix}/lib64
# sharedlibdir=${libdir}
# includedir=${prefix}/include
#
# Name: zlib
# Description: zlib compression library
# Version: 1.2.13
# Requires:
# Libs: -L${libdir} -lz
# Cflags: -I${includedir}

# --- Variables (parte superior) ---
#
# prefix         → directorio base de la instalacion (/usr, /usr/local, etc.)
# exec_prefix    → directorio de ejecutables, generalmente igual a prefix
# libdir         → directorio de las bibliotecas (.so, .a)
# includedir     → directorio de los headers (.h)
#
# Las variables usan ${...} para referirse unas a otras.
# Si la biblioteca se reinstala en otro prefix, solo cambia una linea.

# --- Campos de metadatos (parte inferior) ---
#
# Name           → nombre legible de la biblioteca
# Description    → descripcion breve
# URL            → pagina web del proyecto (opcional)
# Version        → version de la biblioteca
# Requires       → otras bibliotecas .pc de las que depende (publicas)
# Requires.private → dependencias privadas (solo para enlace estatico)
# Cflags         → flags de compilacion (-I, -D, etc.)
# Libs           → flags de enlace (-L, -l, etc.)
# Libs.private   → flags de enlace privados (solo para enlace estatico)
```

```bash
# Ejemplo mas complejo — libpng16.pc:
#
# prefix=/usr
# exec_prefix=${prefix}
# libdir=${exec_prefix}/lib64
# includedir=${prefix}/include/libpng16
#
# Name: libpng
# Description: Loads and saves PNG files
# URL: http://www.libpng.org/pub/png/libpng.html
# Version: 1.6.37
# Requires: zlib
# Cflags: -I${includedir}
# Libs: -L${libdir} -lpng16
# Libs.private: -lm -lz

# Observar:
# - includedir apunta a un subdirectorio (libpng16), no a /usr/include
#   Esto es porque los headers de libpng estan en /usr/include/libpng16/
# - Requires: zlib → libpng depende de zlib publicamente.
#   Cuando pides --cflags o --libs de libpng, pkg-config
#   automaticamente incluye los flags de zlib tambien.
# - Libs.private: -lm -lz → dependencias adicionales solo para
#   enlace estatico.
```

## --cflags

El flag `--cflags` produce los flags de compilacion necesarios
para usar los headers de la biblioteca. Generalmente son
flags `-I` (include paths) y a veces `-D` (defines):

```bash
# Obtener los flags de compilacion de libpng:
pkg-config --cflags libpng
# -I/usr/include/libpng16

# Si los headers estan en la ruta estandar (/usr/include/),
# --cflags puede devolver vacio — el compilador ya busca ahi:
pkg-config --cflags zlib
# (vacio — los headers de zlib estan en /usr/include/)

# Que el resultado sea vacio es CORRECTO, no es un error.
# Significa que no se necesitan flags adicionales.

# Algunos paquetes producen defines ademas de -I:
pkg-config --cflags glib-2.0
# -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -pthread
#
# Nota: -pthread aparece aqui porque glib requiere soporte de threads
# tanto en compilacion como en enlace.
```

## --libs

El flag `--libs` produce los flags de enlace: `-L` (library
paths), `-l` (bibliotecas a enlazar), y a veces flags extra
como `-pthread`:

```bash
# Obtener los flags de enlace de libpng:
pkg-config --libs libpng
# -lpng16 -lz

# pkg-config omite -L si la biblioteca esta en la ruta estandar
# (/usr/lib64/, /usr/lib/). El enlazador ya busca ahi.

# Si la biblioteca esta en una ruta no estandar:
# -L/usr/local/lib -lpng16 -lz

# Algunos paquetes incluyen flags de enlace adicionales:
pkg-config --libs glib-2.0
# -lglib-2.0 -pthread
# -pthread indica al enlazador que debe soportar threads POSIX.

# Nota sobre el orden: -l flags deben ir DESPUES de los .c/.o
# en la linea de gcc. Esto es importante y se vera en el ejemplo completo.
```

## --modversion

El flag `--modversion` devuelve la version de la biblioteca
instalada:

```bash
pkg-config --modversion zlib
# 1.2.13

pkg-config --modversion libpng
# 1.6.37

pkg-config --modversion openssl
# 3.1.4

# Util para scripts que necesitan verificar versiones:
echo "zlib version: $(pkg-config --modversion zlib)"
```

## --exists

El flag `--exists` verifica si un paquete esta disponible.
No produce salida — solo retorna un exit code (0 = existe,
1 = no existe). Es ideal para scripts y Makefiles:

```bash
# Verificar si zlib esta disponible:
pkg-config --exists zlib
echo $?
# 0   (existe)

# Verificar un paquete que no existe:
pkg-config --exists libnoexiste
echo $?
# 1   (no existe)

# Verificar con requisito de version minima:
pkg-config --exists "zlib >= 1.2.11"
echo $?
# 0   (la version instalada cumple el requisito)

pkg-config --exists "zlib >= 99.0"
echo $?
# 1   (la version instalada no cumple)

# Operadores de version disponibles: =, !=, >, >=, <, <=

# Uso tipico en un script de configuracion:
if pkg-config --exists "libpng >= 1.6"; then
    echo "libpng 1.6+ found"
else
    echo "ERROR: libpng 1.6+ not found" >&2
    exit 1
fi
```

## Usar pkg-config en la linea de compilacion

pkg-config se integra directamente en la invocacion de `gcc`
usando sustitucion de comandos (`$()` o backticks):

```bash
# Forma basica — sustitucion con $():
gcc $(pkg-config --cflags --libs libpng) main.c -o program

# Equivalente con backticks (menos legible, evitar):
gcc `pkg-config --cflags --libs libpng` main.c -o program

# IMPORTANTE: el orden de los argumentos importa.
# Los flags -l DEBEN ir DESPUES de los archivos fuente/.o:

# CORRECTO — fuentes antes de -l:
gcc $(pkg-config --cflags libpng) main.c -o program $(pkg-config --libs libpng)

# INCORRECTO — puede fallar con "undefined reference":
gcc $(pkg-config --libs libpng) main.c -o program
# El enlazador procesa de izquierda a derecha. Si -lpng16 aparece
# antes de main.c, el enlazador la descarta porque "aun no la necesita".
# Luego, cuando procesa main.c y encuentra simbolos de libpng, ya es tarde.

# En la practica, cuando se usa $(pkg-config --cflags --libs lib)
# junto al fuente, gcc suele resolver el orden correctamente con
# bibliotecas compartidas (.so). Pero para mayor seguridad:
gcc $(pkg-config --cflags libpng) -o program main.c $(pkg-config --libs libpng)
```

```bash
# Verificar que produce pkg-config antes de compilar:
echo "CFLAGS: $(pkg-config --cflags libpng)"
echo "LIBS:   $(pkg-config --libs libpng)"
# CFLAGS: -I/usr/include/libpng16
# LIBS:   -lpng16 -lz

# Asi puedes ver exactamente que flags se insertaran en la linea de compilacion.
```

## PKG_CONFIG_PATH

La variable de entorno `PKG_CONFIG_PATH` permite agregar
directorios adicionales donde pkg-config busca archivos `.pc`.
Es necesaria cuando una biblioteca esta instalada en una ruta
no estandar:

```bash
# Escenario: instalaste una biblioteca en /opt/mylib/
# El archivo .pc esta en /opt/mylib/lib/pkgconfig/mylib.pc
# pkg-config NO lo encuentra porque no busca ahi por defecto:
pkg-config --cflags mylib
# Package mylib was not found in the pkg-config search path.

# Solucion: agregar la ruta a PKG_CONFIG_PATH:
export PKG_CONFIG_PATH=/opt/mylib/lib/pkgconfig:$PKG_CONFIG_PATH

# Ahora pkg-config lo encuentra:
pkg-config --cflags mylib
# -I/opt/mylib/include

# El :$PKG_CONFIG_PATH al final preserva las rutas existentes.
# Sin eso, se pierden las rutas por defecto del sistema.
```

```bash
# Casos comunes donde PKG_CONFIG_PATH es necesario:

# 1. Biblioteca compilada e instalada en /usr/local/:
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# 2. Biblioteca instalada con un prefix personalizado:
#    (./configure --prefix=/home/user/libs/openssl)
export PKG_CONFIG_PATH=/home/user/libs/openssl/lib/pkgconfig:$PKG_CONFIG_PATH

# 3. Multiples rutas separadas por :
export PKG_CONFIG_PATH=/opt/lib1/lib/pkgconfig:/opt/lib2/lib/pkgconfig:$PKG_CONFIG_PATH

# Para hacerlo permanente, agregar la linea export al .bashrc o .zshrc:
echo 'export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH' >> ~/.bashrc

# Verificar las rutas actuales de busqueda:
pkg-config --variable pc_path pkg-config
# Muestra las rutas por defecto compiladas en pkg-config.
# PKG_CONFIG_PATH se agrega ANTES de esas rutas (tiene prioridad).
```

## pkg-config en Makefiles

La integracion de pkg-config en Makefiles se hace con la
funcion `$(shell ...)` de Make:

```makefile
# Makefile — patron basico con pkg-config

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic
CFLAGS += $(shell pkg-config --cflags libpng)
LDLIBS  = $(shell pkg-config --libs libpng)

program: main.o
	$(CC) -o $@ $^ $(LDLIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f program *.o

.PHONY: clean
```

```makefile
# Makefile — patron robusto con verificacion de existencia
#
# Si el paquete no esta instalado, Make falla con un error claro
# en vez de producir flags vacios y errores crípticos de gcc.

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic

# Verificar que los paquetes necesarios estan disponibles:
PKG_CHECK := $(shell pkg-config --exists libpng || echo "MISSING: libpng")

ifneq ($(PKG_CHECK),)
    $(error pkg-config: $(PKG_CHECK). Install with: sudo dnf install libpng-devel)
endif

CFLAGS += $(shell pkg-config --cflags libpng)
LDLIBS  = $(shell pkg-config --libs libpng)

all: program

program: main.o
	$(CC) -o $@ $^ $(LDLIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f program *.o

.PHONY: all clean
```

```makefile
# Makefile — multiples bibliotecas

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic

# Agrupar todos los paquetes en una variable:
PKGS = libpng zlib

CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDLIBS  = $(shell pkg-config --libs $(PKGS))

all: program

program: main.o image.o
	$(CC) -o $@ $^ $(LDLIBS)

main.o: main.c image.h
	$(CC) $(CFLAGS) -c $<

image.o: image.c image.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f program *.o

.PHONY: all clean
```

## --static

El flag `--static` se usa junto con `--libs` para obtener
todos los flags necesarios para enlace estatico. En enlace
estatico, las dependencias transitivas (privadas) se deben
enlazar explicitamente:

```bash
# Enlace dinamico (normal) — solo las dependencias publicas:
pkg-config --libs zlib
# -lz

# Enlace estatico — incluye tambien Libs.private y Requires.private:
pkg-config --libs --static zlib
# -lz

# Con una biblioteca que tiene dependencias privadas:
pkg-config --libs libpng
# -lpng16 -lz

pkg-config --libs --static libpng
# -lpng16 -lz -lm
# -lm aparece porque esta en Libs.private de libpng.
# En enlace dinamico no es necesario listarla porque libpng.so
# ya la enlaza internamente. En enlace estatico hay que incluir
# TODAS las dependencias.
```

```bash
# Para compilar estaticamente:
gcc $(pkg-config --cflags libpng) -o program main.c $(pkg-config --libs --static libpng) -static

# El flag -static de gcc dice "enlazar todo estaticamente".
# El flag --static de pkg-config dice "incluir dependencias privadas".
# Ambos son necesarios para un enlace estatico correcto.
```

## Multiples paquetes

pkg-config acepta varios paquetes en una sola invocacion y
**deduplica los flags automaticamente**:

```bash
# Pedir flags de multiples paquetes a la vez:
pkg-config --cflags --libs libpng zlib openssl
# -I/usr/include/libpng16 -lpng16 -lz -lssl -lcrypto

# Si pidieras cada uno por separado:
pkg-config --libs libpng
# -lpng16 -lz
pkg-config --libs zlib
# -lz
pkg-config --libs openssl
# -lssl -lcrypto

# -lz aparece en libpng y en zlib, pero pkg-config lo lista
# UNA sola vez cuando se piden juntos. Esto evita warnings del
# enlazador por flags duplicados.

# Esto es especialmente util en Makefiles:
# PKGS = libpng zlib openssl
# LDLIBS = $(shell pkg-config --libs $(PKGS))
```

## Ejemplo completo

Un programa que lee informacion de un archivo comprimido con
zlib, compilado con pkg-config desde la linea de comandos y
desde un Makefile:

```c
// zinfo.c
#include <stdio.h>
#include <zlib.h>

int main(void) {
    printf("zlib version (header):  %s\n", ZLIB_VERSION);
    printf("zlib version (runtime): %s\n", zlibVersion());

    // Verificar que header y biblioteca son compatibles:
    if (zlibVersion()[0] != ZLIB_VERSION[0]) {
        fprintf(stderr, "ERROR: zlib header/library mismatch\n");
        return 1;
    }

    // Demostrar compresion basica:
    const char *input = "Hello, pkg-config! This is a test string for compression.";
    uLong input_len = (uLong)strlen(input) + 1;  // incluir null terminator

    uLong compressed_len = compressBound(input_len);
    Bytef compressed[256];
    Bytef decompressed[256];

    // Comprimir:
    int ret = compress(compressed, &compressed_len, (const Bytef *)input, input_len);
    if (ret != Z_OK) {
        fprintf(stderr, "compress() failed: %d\n", ret);
        return 1;
    }
    printf("Original size:    %lu bytes\n", input_len);
    printf("Compressed size:  %lu bytes\n", compressed_len);

    // Descomprimir:
    uLong decompressed_len = sizeof(decompressed);
    ret = uncompress(decompressed, &decompressed_len, compressed, compressed_len);
    if (ret != Z_OK) {
        fprintf(stderr, "uncompress() failed: %d\n", ret);
        return 1;
    }
    printf("Decompressed:     \"%s\"\n", (char *)decompressed);

    return 0;
}
```

```bash
# Compilar desde la linea de comandos con pkg-config:

# 1. Verificar que zlib esta disponible:
pkg-config --exists zlib && echo "zlib found" || echo "zlib NOT found"

# 2. Ver que flags producira:
echo "CFLAGS: $(pkg-config --cflags zlib)"
echo "LIBS:   $(pkg-config --libs zlib)"
# CFLAGS:
# LIBS:   -lz
# (CFLAGS vacio porque los headers de zlib estan en /usr/include/)

# 3. Compilar:
gcc -Wall -Wextra -Wpedantic $(pkg-config --cflags zlib) -o zinfo zinfo.c $(pkg-config --libs zlib)

# 4. Ejecutar:
./zinfo
# zlib version (header):  1.2.13
# zlib version (runtime): 1.2.13
# Original size:    58 bytes
# Compressed size:  57 bytes
# Decompressed:     "Hello, pkg-config! This is a test string for compression."
```

```bash
# Nota: para compilar este ejemplo necesitas el paquete de desarrollo de zlib.
# Si pkg-config no encuentra zlib:
#
# Fedora:
sudo dnf install zlib-devel
#
# Ubuntu/Debian:
sudo apt install zlib1g-dev
#
# Los paquetes -devel/-dev instalan los headers (.h) y los archivos .pc.
# La biblioteca (.so) suele venir con el paquete base, pero sin -devel
# no tienes los headers ni el .pc necesarios para compilar.
```

```makefile
# Makefile para el ejemplo con zlib

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic

# Verificar que zlib esta disponible:
ifeq ($(shell pkg-config --exists zlib && echo yes),)
    $(error zlib not found. Install with: sudo dnf install zlib-devel)
endif

CFLAGS += $(shell pkg-config --cflags zlib)
LDLIBS  = $(shell pkg-config --libs zlib)

all: zinfo

zinfo: zinfo.o
	$(CC) -o $@ $^ $(LDLIBS)

zinfo.o: zinfo.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f zinfo *.o

.PHONY: all clean
```

```bash
# Compilar con el Makefile:
make
# gcc -Wall -Wextra -Wpedantic  -c zinfo.c
# gcc -o zinfo zinfo.o -lz

./zinfo
# zlib version (header):  1.2.13
# zlib version (runtime): 1.2.13
# Original size:    58 bytes
# Compressed size:  57 bytes
# Decompressed:     "Hello, pkg-config! This is a test string for compression."

# Ver los comandos exactos con verbose:
make clean && make
# rm -f zinfo *.o
# gcc -Wall -Wextra -Wpedantic  -c zinfo.c
# gcc -o zinfo zinfo.o -lz
# Los flags de pkg-config se expandieron automaticamente.
```

---

## Ejercicios

### Ejercicio 1 — Explorar archivos .pc

```bash
# 1. Listar todos los paquetes disponibles en tu sistema:
#    pkg-config --list-all
#    Contar cuantos hay: pkg-config --list-all | wc -l
#
# 2. Elegir tres paquetes de la lista (por ejemplo zlib, libcurl, openssl)
#    y para cada uno:
#    a) Mostrar la version con --modversion
#    b) Mostrar los flags de compilacion con --cflags
#    c) Mostrar los flags de enlace con --libs
#    d) Localizar el archivo .pc en el sistema de archivos
#    e) Leer el contenido del .pc con cat y correlacionar cada campo
#       con la salida de --cflags y --libs
#
# 3. Verificar con --exists si tienes estos paquetes:
#    - "zlib >= 1.2"
#    - "zlib >= 99.0"
#    - "libnoexiste"
#    Observar el exit code con echo $? en cada caso.
#
# 4. Probar --libs --static en un paquete que tenga Libs.private
#    (libpng es buen candidato). Comparar la salida con --libs sin --static.
#    Explicar por que aparecen flags adicionales en la version estatica.
```

### Ejercicio 2 — Compilar con pkg-config

```c
// Crear un programa (sysinfo.c) que:
//   - Incluya <zlib.h>
//   - Imprima la version de zlib con zlibVersion()
//   - Comprima un string de al menos 100 caracteres con compress()
//   - Descomprima el resultado con uncompress()
//   - Imprima el tamano original, comprimido y descomprimido
//   - Verifique que el string descomprimido es identico al original
//     usando strcmp()
//
// Compilar de dos formas:
//
// 1. Desde la linea de comandos:
//    gcc -Wall -Wextra $(pkg-config --cflags zlib) -o sysinfo sysinfo.c $(pkg-config --libs zlib)
//
// 2. Con un Makefile que:
//    - Use $(shell pkg-config --cflags zlib) en CFLAGS
//    - Use $(shell pkg-config --libs zlib) en LDLIBS
//    - Verifique que zlib existe con --exists antes de compilar
//    - Tenga reglas all y clean
//
// Verificar que ambas formas producen el mismo ejecutable funcional.
```

### Ejercicio 3 — PKG_CONFIG_PATH y .pc personalizado

```bash
# 1. Crear una "biblioteca" minima:
#    - Crear el directorio /tmp/mymath/
#    - Crear /tmp/mymath/include/mymath.h con:
#        #ifndef MYMATH_H
#        #define MYMATH_H
#        int square(int x);
#        int cube(int x);
#        #endif
#    - Crear /tmp/mymath/src/mymath.c con la implementacion
#    - Compilar a biblioteca estatica:
#        gcc -c -o mymath.o mymath.c
#        ar rcs /tmp/mymath/lib/libmymath.a mymath.o
#
# 2. Crear el archivo /tmp/mymath/lib/pkgconfig/mymath.pc con:
#    - prefix=/tmp/mymath
#    - includedir=${prefix}/include
#    - libdir=${prefix}/lib
#    - Name, Description, Version
#    - Cflags: -I${includedir}
#    - Libs: -L${libdir} -lmymath
#
# 3. Intentar compilar un programa que use mymath SIN PKG_CONFIG_PATH:
#    pkg-config --cflags mymath
#    (deberia fallar: "Package mymath was not found")
#
# 4. Establecer PKG_CONFIG_PATH y compilar:
#    export PKG_CONFIG_PATH=/tmp/mymath/lib/pkgconfig:$PKG_CONFIG_PATH
#    pkg-config --cflags mymath
#    pkg-config --libs mymath
#    gcc $(pkg-config --cflags --libs mymath) -o usemymath main.c
#
# 5. Escribir un Makefile que compile el programa usando pkg-config para mymath.
#    Verificar que funciona solo cuando PKG_CONFIG_PATH esta configurado.
```
