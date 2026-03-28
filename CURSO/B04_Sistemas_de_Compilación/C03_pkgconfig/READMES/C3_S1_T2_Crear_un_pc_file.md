# T02 — Crear un .pc file

## Por que crear un .pc

Cuando distribuyes una biblioteca, los usuarios necesitan saber tres
cosas para compilar contra ella: donde estan los headers, donde esta
el binario, y contra que otras bibliotecas enlazar. Sin un archivo
`.pc`, el usuario tiene que adivinar estos flags o leer documentacion
que puede estar desactualizada:

```bash
# Sin .pc, el usuario tiene que saber todo esto de memoria:
gcc -I/usr/local/include/mylib \
    -L/usr/local/lib \
    -lmylib -lm -lpthread \
    -o app app.c

# Que incluir? -I/usr/local/include? -I/usr/local/include/mylib?
# Que enlazar? Solo -lmylib? Necesita -lm? -lpthread?
# Si la biblioteca cambia de ruta o agrega dependencias,
# todos los usuarios tienen que actualizar sus scripts.
```

```bash
# Con un .pc, el usuario ejecuta:
gcc $(pkg-config --cflags mylib) -o app app.c $(pkg-config --libs mylib)

# pkg-config resuelve los flags correctos automaticamente.
# Si la biblioteca cambia de ruta o agrega dependencias,
# el .pc se actualiza y los scripts del usuario no cambian.
```

Un archivo `.pc` es un contrato entre el autor de la biblioteca
y sus usuarios. Estandariza la informacion de compilacion en un
formato que cualquier sistema de build puede consumir.

## Estructura del archivo .pc

Un `.pc` tiene dos secciones: variables arriba y campos de
metadatos abajo. Las variables se definen con `nombre=valor`
y se referencian con `${nombre}`. Los campos usan la sintaxis
`Campo: valor`.

```pc
# Ejemplo completo — mylib.pc

# === Variables ===
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

# === Metadata ===
Name: mylib
Description: A small utility library for string manipulation
URL: https://github.com/user/mylib
Version: 1.2.0

# === Dependencies ===
Requires: zlib >= 1.2.0
Requires.private: libpng

# === Compiler/Linker flags ===
Cflags: -I${includedir}/mylib
Libs: -L${libdir} -lmylib
Libs.private: -lm -lpthread
```

Cada campo en detalle:

```pc
# --- Variables ---

# prefix: raiz de la instalacion. Todas las demas rutas
# se derivan de aqui. Si cambias prefix, todo se ajusta.
prefix=/usr/local

# exec_prefix: raiz para archivos dependientes de arquitectura.
# Normalmente igual a prefix. Se separa cuando los binarios
# van a /usr/local/bin pero las bibliotecas a /usr/lib64.
exec_prefix=${prefix}

# libdir: donde estan los .a y .so de la biblioteca.
libdir=${exec_prefix}/lib

# includedir: donde estan los headers publicos.
includedir=${prefix}/include
```

```pc
# --- Campos obligatorios ---

# Name: nombre legible para humanos. Se usa en mensajes de error.
# No es el nombre del .pc (el nombre del archivo determina
# como se invoca: pkg-config --cflags mylib busca mylib.pc).
Name: MyLib

# Description: una linea que describe la biblioteca.
Description: A small utility library for string manipulation

# Version: version de la biblioteca. pkg-config puede comparar
# versiones cuando otro .pc dice Requires: mylib >= 1.0.
Version: 1.2.0
```

```pc
# --- Campos opcionales ---

# URL: sitio web del proyecto. Informativo.
URL: https://github.com/user/mylib

# Requires: dependencias publicas. Otras bibliotecas con .pc
# que el usuario tambien necesita al compilar contra tu biblioteca.
# pkg-config agrega los Cflags y Libs de estas dependencias
# automaticamente cuando el usuario consulta tu .pc.
Requires: zlib >= 1.2.0

# Requires.private: dependencias que solo se necesitan para
# enlace estatico. No se incluyen con --cflags/--libs normales,
# solo con --static.
Requires.private: libpng

# Cflags: flags de compilacion que el usuario necesita.
# Tipicamente -I para encontrar los headers.
Cflags: -I${includedir}/mylib

# Libs: flags de enlace para el usuario.
# -L para la ruta de busqueda y -l para la biblioteca.
Libs: -L${libdir} -lmylib

# Libs.private: flags de enlace que solo se necesitan para
# enlace estatico. Dependencias internas que no son parte
# de la API publica.
Libs.private: -lm -lpthread
```

## Variables en .pc

Las variables son la clave de la portabilidad del archivo `.pc`.
Se definen en la parte superior y se referencian con `${nombre}`
en los campos de abajo. Esto permite cambiar la raiz de
instalacion sin editar cada ruta individualmente:

```pc
# Las variables forman una cadena de derivacion:
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

# Si prefix=/usr/local:
#   exec_prefix = /usr/local
#   libdir      = /usr/local/lib
#   includedir  = /usr/local/include

# Si se redefine prefix=/opt/mylib:
#   exec_prefix = /opt/mylib
#   libdir      = /opt/mylib/lib
#   includedir  = /opt/mylib/include
```

```bash
# pkg-config permite redefinir prefix desde la linea de comandos:
pkg-config --define-variable=prefix=/opt/mylib --cflags mylib
# Salida: -I/opt/mylib/include/mylib

# Esto es util cuando la biblioteca se instalo en una ruta
# no estandar y el .pc tiene el prefix original hardcodeado.
```

```pc
# Se pueden definir variables propias para evitar repeticion:
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include
plugindir=${libdir}/mylib/plugins

Name: mylib
Description: Library with plugin support
Version: 2.0.0
Cflags: -I${includedir}/mylib -DPLUGIN_DIR=\"${plugindir}\"
Libs: -L${libdir} -lmylib
```

## Ejemplo: crear un .pc para una biblioteca propia

Paso a paso: crear una biblioteca minima, escribir su `.pc`,
instalarlo y verificar que funciona.

```c
// include/vecmath/vecmath.h
#ifndef VECMATH_H
#define VECMATH_H

typedef struct {
    double x, y, z;
} Vec3;

Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, double s);
double vec3_dot(Vec3 a, Vec3 b);
double vec3_length(Vec3 v);

#endif // VECMATH_H
```

```c
// src/vecmath.c
#include "vecmath/vecmath.h"
#include <math.h>

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_scale(Vec3 v, double s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double vec3_length(Vec3 v) {
    return sqrt(vec3_dot(v, v));
}
```

```pc
# vecmath.pc — archivo .pc para la biblioteca
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: vecmath
Description: Simple 3D vector math library
URL: https://github.com/user/vecmath
Version: 1.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lvecmath
Libs.private: -lm
```

```bash
# Compilar la biblioteca:
gcc -c -Iinclude -o vecmath.o src/vecmath.c
ar rcs libvecmath.a vecmath.o

# Instalar (como root o con sudo):
install -d /usr/local/lib
install -d /usr/local/include/vecmath
install -d /usr/local/lib/pkgconfig
install -m 644 libvecmath.a /usr/local/lib/
install -m 644 include/vecmath/vecmath.h /usr/local/include/vecmath/
install -m 644 vecmath.pc /usr/local/lib/pkgconfig/

# Verificar que pkg-config lo encuentra:
pkg-config --cflags vecmath
# -I/usr/local/include

pkg-config --libs vecmath
# -L/usr/local/lib -lvecmath

pkg-config --libs --static vecmath
# -L/usr/local/lib -lvecmath -lm
```

```c
// test_vecmath.c — programa que usa la biblioteca
#include <stdio.h>
#include <vecmath/vecmath.h>

int main(void) {
    Vec3 a = {1.0, 2.0, 3.0};
    Vec3 b = {4.0, 5.0, 6.0};

    Vec3 sum = vec3_add(a, b);
    printf("a + b = (%.1f, %.1f, %.1f)\n", sum.x, sum.y, sum.z);

    double dot = vec3_dot(a, b);
    printf("a . b = %.1f\n", dot);

    double len = vec3_length(a);
    printf("|a| = %.4f\n", len);

    return 0;
}
```

```bash
# Compilar el programa usando pkg-config:
gcc $(pkg-config --cflags vecmath) -o test_vecmath test_vecmath.c \
    $(pkg-config --libs vecmath) -lm

./test_vecmath
# a + b = (5.0, 7.0, 9.0)
# a . b = 32.0
# |a| = 3.7417
```

## Donde instalar el .pc

La ubicacion estandar es `${libdir}/pkgconfig/`. Esto es donde
`pkg-config` busca por defecto:

```bash
# Rutas estandar de busqueda (en orden):
# 1. ${PKG_CONFIG_PATH} (si esta definida, se busca primero)
# 2. ${libdir}/pkgconfig (tipicamente /usr/lib/pkgconfig)
# 3. /usr/share/pkgconfig (para .pc que no dependen de arquitectura)

# Ver donde busca pkg-config:
pkg-config --variable pc_path pkg-config
# /usr/local/lib/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig

# Para una instalacion local (sin root):
# Instalar en ~/mylibs/lib/pkgconfig/
# y agregar a PKG_CONFIG_PATH:
export PKG_CONFIG_PATH=$HOME/mylibs/lib/pkgconfig:$PKG_CONFIG_PATH
```

```makefile
# En un Makefile, la regla de instalacion del .pc:
PREFIX  ?= /usr/local
LIBDIR   = $(PREFIX)/lib

.PHONY: install
install: libvecmath.a vecmath.pc
	install -d $(DESTDIR)$(PREFIX)/include/vecmath
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 include/vecmath/vecmath.h $(DESTDIR)$(PREFIX)/include/vecmath/
	install -m 644 libvecmath.a $(DESTDIR)$(LIBDIR)/
	install -m 644 vecmath.pc $(DESTDIR)$(LIBDIR)/pkgconfig/

# DESTDIR permite empaquetado (RPM, DEB):
# make install DESTDIR=/tmp/staging
# Instala en /tmp/staging/usr/local/lib/pkgconfig/vecmath.pc
# El paquete luego mueve todo a las rutas reales.
```

## Generar el .pc con Make

En la practica, no se escribe el `.pc` con rutas hardcodeadas.
Se usa un template `.pc.in` con placeholders que se sustituyen
en tiempo de build con `sed`:

```pc
# vecmath.pc.in — template con placeholders
prefix=@PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: vecmath
Description: Simple 3D vector math library
URL: https://github.com/user/vecmath
Version: @VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lvecmath
Libs.private: -lm
```

```makefile
# Makefile que genera vecmath.pc desde el template
PREFIX  ?= /usr/local
VERSION  = 1.0.0

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
AR       = ar

TARGET   = libvecmath.a
OBJS     = vecmath.o

.PHONY: all
all: $(TARGET) vecmath.pc

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

vecmath.o: src/vecmath.c include/vecmath/vecmath.h
	$(CC) $(CFLAGS) -Iinclude -c -o $@ $<

# Generar el .pc sustituyendo placeholders:
vecmath.pc: vecmath.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    $< > $@

.PHONY: install
install: $(TARGET) vecmath.pc
	install -d $(DESTDIR)$(PREFIX)/include/vecmath
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 include/vecmath/vecmath.h $(DESTDIR)$(PREFIX)/include/vecmath/
	install -m 644 $(TARGET) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 vecmath.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET) vecmath.pc
```

```bash
# Construir con PREFIX personalizado:
make PREFIX=/opt/vecmath
# sed sustituye @PREFIX@ por /opt/vecmath en vecmath.pc

cat vecmath.pc
# prefix=/opt/vecmath
# exec_prefix=${prefix}
# libdir=${exec_prefix}/lib
# includedir=${prefix}/include
# ...

# Instalar en directorio staging:
make install PREFIX=/opt/vecmath DESTDIR=/tmp/pkg
# Instala en /tmp/pkg/opt/vecmath/lib/pkgconfig/vecmath.pc
```

```makefile
# El uso de sed con | como delimitador (en lugar de /)
# es importante. Si PREFIX contiene /, el delimitador /
# romperia el comando sed:
#
#   sed 's/@PREFIX@//usr/local/g'  ← ROTO (multiples /)
#   sed 's|@PREFIX@|/usr/local|g'  ← CORRECTO
#
# Siempre usar | o # como delimitador en sustituciones
# que involucren rutas del filesystem.
```

## Generar el .pc con CMake

CMake tiene `configure_file()` que sustituye variables
automaticamente. No se necesita `sed`:

```pc
# vecmath.pc.in — template para CMake
# CMake sustituye @VAR@ por el valor de la variable CMake VAR.
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: vecmath
Description: Simple 3D vector math library
URL: https://github.com/user/vecmath
Version: @PROJECT_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lvecmath
Libs.private: -lm
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(vecmath VERSION 1.0.0 LANGUAGES C)

add_library(vecmath STATIC src/vecmath.c)
target_include_directories(vecmath PUBLIC include/)

# Generar el .pc desde el template.
# @ONLY hace que solo se sustituyan variables con @VAR@,
# no las que usan ${var} (que son variables del .pc).
configure_file(vecmath.pc.in vecmath.pc @ONLY)

# Instalar la biblioteca, los headers y el .pc:
install(TARGETS vecmath ARCHIVE DESTINATION lib)
install(FILES include/vecmath/vecmath.h DESTINATION include/vecmath)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vecmath.pc
        DESTINATION lib/pkgconfig)
```

```bash
# Construir e instalar:
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build

# cmake --install genera el .pc con:
# prefix=/usr/local
# Version: 1.0.0
# (tomados de CMAKE_INSTALL_PREFIX y PROJECT_VERSION)
```

```cmake
# El flag @ONLY es critico. Sin el, configure_file() tambien
# sustituiria ${prefix}, ${libdir}, etc., que son variables
# internas del .pc, NO variables de CMake.
#
#   configure_file(vecmath.pc.in vecmath.pc)      ← ROTO
#     CMake intenta sustituir ${prefix} → vacio
#
#   configure_file(vecmath.pc.in vecmath.pc @ONLY) ← CORRECTO
#     CMake solo sustituye @CMAKE_INSTALL_PREFIX@, @PROJECT_VERSION@
#     Deja ${prefix}, ${libdir} intactas para pkg-config
```

## Requires vs Libs

Cuando tu biblioteca depende de otra que tiene su propio `.pc`,
usa `Requires` en lugar de poner sus flags directamente en `Libs`.
Esto permite que `pkg-config` resuelva la cadena de dependencias
de forma transitiva:

```pc
# INCORRECTO — hardcodear flags de dependencias:
Name: myhttp
Description: HTTP client library
Version: 1.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lmyhttp -lz -lssl -lcrypto
# Problemas:
#   - Si zlib cambia su ruta de include, este .pc no se entera.
#   - Si openssl agrega una nueva dependencia, falta aqui.
#   - No se resuelve transitividad.
```

```pc
# CORRECTO — declarar dependencias con Requires:
Name: myhttp
Description: HTTP client library
Version: 1.0.0
Requires: zlib >= 1.2.0 openssl >= 1.1.0
Cflags: -I${includedir}
Libs: -L${libdir} -lmyhttp
# Ventajas:
#   - pkg-config busca zlib.pc y openssl.pc automaticamente.
#   - Propaga sus Cflags y Libs al usuario.
#   - Si zlib cambia, solo zlib.pc se actualiza.
#   - La transitividad se resuelve automaticamente.
```

```bash
# Con Requires, pkg-config combina los flags:
pkg-config --cflags myhttp
# -I/usr/local/include -I/usr/include  (myhttp + zlib + openssl)

pkg-config --libs myhttp
# -L/usr/local/lib -lmyhttp -lz -lssl -lcrypto

# pkg-config leyo myhttp.pc, vio Requires: zlib openssl,
# busco zlib.pc y openssl.pc, y combino todos los flags.
```

```pc
# Requires soporta restricciones de version:
Requires: zlib >= 1.2.0
Requires: glib-2.0 >= 2.50
Requires: libpng >= 1.6 libpng < 2.0

# Operadores validos: =, <, >, <=, >=
# Si la version no cumple, pkg-config da error:
#   Package dependency requirement 'zlib >= 1.2.0' could not be satisfied.
```

## Requires.private y Libs.private

La distincion entre public y private determina que se
expone en enlace dinamico vs estatico. Dependencias internas
que no forman parte de la API publica van en los campos
`.private`:

```pc
# vecmath usa math.h internamente (sqrt, sin, cos)
# pero el usuario no necesita -lm al enlazar dinamicamente
# porque libvecmath.so ya contiene esas referencias resueltas.
# Solo se necesita -lm para enlace estatico.

prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: vecmath
Description: 3D vector math library
Version: 1.0.0
Cflags: -I${includedir}
Libs: -L${libdir} -lvecmath
Libs.private: -lm
```

```bash
# Enlace dinamico — solo los flags publicos:
pkg-config --cflags vecmath
# -I/usr/local/include

pkg-config --libs vecmath
# -L/usr/local/lib -lvecmath
# No incluye -lm — no es necesario con .so

# Enlace estatico — incluye publicos Y privados:
pkg-config --libs --static vecmath
# -L/usr/local/lib -lvecmath -lm
# Ahora si incluye -lm — necesario con .a porque
# el linker necesita resolver sqrt/sin/cos
```

```pc
# Requires.private funciona igual pero con dependencias .pc:

Name: myimage
Description: Image processing library
Version: 2.0.0
Requires: libpng >= 1.6       # dependencia publica (API expone PNG)
Requires.private: zlib         # dependencia interna (compresion interna)
Cflags: -I${includedir}
Libs: -L${libdir} -lmyimage

# Enlace dinamico: propaga libpng, NO zlib
# Enlace estatico: propaga libpng Y zlib
```

```bash
# Comparacion:
pkg-config --libs myimage
# -L/usr/local/lib -lmyimage -lpng16
# Solo dependencias publicas (Requires + Libs)

pkg-config --libs --static myimage
# -L/usr/local/lib -lmyimage -lpng16 -lz
# Publicas + privadas (Requires.private + Libs.private)
```

## Verificar el .pc

Antes de distribuir un `.pc`, hay que verificar que funciona
correctamente. `pkg-config` ofrece varias formas de depuracion:

```bash
# Verificar que el .pc se encuentra:
pkg-config --exists mylib && echo "found" || echo "not found"

# Si no se encuentra, verificar la ruta de busqueda:
pkg-config --variable pc_path pkg-config

# Agregar rutas personalizadas:
export PKG_CONFIG_PATH=/opt/mylib/lib/pkgconfig
pkg-config --exists mylib && echo "found" || echo "not found"
```

```bash
# Imprimir errores detallados:
pkg-config --print-errors mylib
# Si falta el .pc:
#   Package mylib was not found in the pkg-config search path.
#   Perhaps you should add the directory containing `mylib.pc'
#   to the PKG_CONFIG_PATH environment variable

# Si falta una dependencia en Requires:
#   Package 'zlib', required by 'mylib', not found
```

```bash
# Validar el .pc (disponible en versiones recientes de pkg-config):
pkg-config --validate mylib
# Si hay errores de sintaxis o campos faltantes, los reporta.
```

```bash
# Depuracion maxima — ver todo lo que pkg-config hace internamente:
PKG_CONFIG_DEBUG_SPEW=1 pkg-config --cflags --libs mylib

# Salida detallada que muestra:
#   - Que directorios busca
#   - Que archivos .pc lee
#   - Como resuelve variables
#   - Como combina flags de dependencias
# Util cuando los flags no son los esperados.
```

```bash
# Verificar campos individuales:
pkg-config --modversion mylib     # imprime Version
# 1.0.0

pkg-config --variable=prefix mylib
# /usr/local

pkg-config --variable=libdir mylib
# /usr/local/lib

pkg-config --print-requires mylib
# zlib >= 1.2.0

pkg-config --print-requires-private mylib
# libpng
```

## Ejemplo completo

Crear una biblioteca estatica con su `.pc`, instalarla
localmente (sin root), y compilar un programa que la use:

```
# Estructura del proyecto:
#
#   strutil/
#   ├── Makefile
#   ├── strutil.pc.in
#   ├── include/
#   │   └── strutil/
#   │       └── strutil.h
#   ├── src/
#   │   └── strutil.c
#   └── examples/
#       └── demo.c
```

```c
// include/strutil/strutil.h
#ifndef STRUTIL_H
#define STRUTIL_H

#include <stddef.h>

// Reverse a string in place. Returns s.
char *str_reverse(char *s);

// Convert a string to uppercase in place. Returns s.
char *str_upper(char *s);

// Count occurrences of character c in string s.
size_t str_count(const char *s, char c);

#endif // STRUTIL_H
```

```c
// src/strutil.c
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
        *p = toupper((unsigned char)*p);
    }
    return s;
}

size_t str_count(const char *s, char c) {
    size_t count = 0;
    for (; *s; s++) {
        if (*s == c) count++;
    }
    return count;
}
```

```pc
# strutil.pc.in — template
prefix=@PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: strutil
Description: Simple string utility library
URL: https://github.com/user/strutil
Version: @VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lstrutil
```

```makefile
# Makefile
PREFIX  ?= $(HOME)/.local
VERSION  = 0.1.0

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
AR       = ar

TARGET   = libstrutil.a
OBJS     = strutil.o

.PHONY: all
all: $(TARGET) strutil.pc

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

strutil.o: src/strutil.c include/strutil/strutil.h
	$(CC) $(CFLAGS) -Iinclude -c -o $@ $<

strutil.pc: strutil.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' \
	    $< > $@

.PHONY: install
install: $(TARGET) strutil.pc
	install -d $(DESTDIR)$(PREFIX)/include/strutil
	install -d $(DESTDIR)$(PREFIX)/lib
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 include/strutil/strutil.h $(DESTDIR)$(PREFIX)/include/strutil/
	install -m 644 $(TARGET) $(DESTDIR)$(PREFIX)/lib/
	install -m 644 strutil.pc $(DESTDIR)$(PREFIX)/lib/pkgconfig/

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/include/strutil/strutil.h
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/lib/pkgconfig/strutil.pc
	-rmdir $(DESTDIR)$(PREFIX)/include/strutil

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET) strutil.pc
```

```c
// examples/demo.c
#include <stdio.h>
#include <string.h>
#include <strutil/strutil.h>

int main(void) {
    char word[] = "hello";

    printf("Original:  %s\n", word);
    str_reverse(word);
    printf("Reversed:  %s\n", word);
    str_upper(word);
    printf("Upper:     %s\n", word);

    const char *text = "mississippi";
    printf("'s' in \"%s\": %zu\n", text, str_count(text, 's'));

    return 0;
}
```

```bash
# Paso 1 — Compilar e instalar la biblioteca localmente:
make
# gcc -Wall -Wextra -std=c11 -Iinclude -c -o strutil.o src/strutil.c
# ar rcs libstrutil.a strutil.o
# sed ... strutil.pc.in > strutil.pc

make install
# install -d /home/user/.local/include/strutil
# install -d /home/user/.local/lib
# install -d /home/user/.local/lib/pkgconfig
# install -m 644 include/strutil/strutil.h /home/user/.local/include/strutil/
# install -m 644 libstrutil.a /home/user/.local/lib/
# install -m 644 strutil.pc /home/user/.local/lib/pkgconfig/
```

```bash
# Paso 2 — Configurar PKG_CONFIG_PATH para la instalacion local:
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH

# Verificar que pkg-config encuentra la biblioteca:
pkg-config --exists strutil && echo "found" || echo "not found"
# found

pkg-config --modversion strutil
# 0.1.0

pkg-config --cflags strutil
# -I/home/user/.local/include

pkg-config --libs strutil
# -L/home/user/.local/lib -lstrutil
```

```bash
# Paso 3 — Compilar el programa de ejemplo usando pkg-config:
gcc $(pkg-config --cflags strutil) \
    -o demo examples/demo.c \
    $(pkg-config --libs strutil)

./demo
# Original:  hello
# Reversed:  olleh
# Upper:     OLLEH
# 's' in "mississippi": 4
```

```bash
# Paso 4 — Verificar con depuracion:
PKG_CONFIG_DEBUG_SPEW=1 pkg-config --cflags --libs strutil 2>&1 | head -20
# Muestra los directorios de busqueda, el archivo .pc leido,
# la resolucion de variables y los flags resultantes.
```

---

## Ejercicios

### Ejercicio 1 — Crear un .pc para una biblioteca con dependencias

```text
Crear una biblioteca "imgscale" que dependa de libm (internamente,
para calculos de interpolacion) y de libpng (como parte de su API
publica, si libpng tiene un .pc en tu sistema).

1. Escribir un imgscale.pc con:
   - Variables: prefix, exec_prefix, libdir, includedir
   - Requires: libpng (o dejarla vacia si no hay libpng.pc)
   - Libs.private: -lm
   - Version: 0.1.0

2. Instalar el .pc en un directorio local (ej: /tmp/imgscale/lib/pkgconfig/).

3. Verificar con:
   - PKG_CONFIG_PATH=/tmp/imgscale/lib/pkgconfig pkg-config --cflags imgscale
   - PKG_CONFIG_PATH=/tmp/imgscale/lib/pkgconfig pkg-config --libs imgscale
   - PKG_CONFIG_PATH=/tmp/imgscale/lib/pkgconfig pkg-config --libs --static imgscale
     (debe mostrar -lm solo con --static)

4. Si Requires: libpng no funciona (porque no hay libpng.pc),
   usar --print-errors para ver el mensaje de error exacto.
   Luego quitar el Requires y poner -lpng directamente en Libs.
   Reflexionar sobre que se pierde al no usar Requires.
```

### Ejercicio 2 — Template .pc.in con Make y sed

```text
Crear un proyecto con:
  - mathext.pc.in que tenga @PREFIX@, @VERSION@ y un placeholder
    adicional @LIBNAME@ (para el nombre de la biblioteca).
  - Un Makefile con una regla que genere mathext.pc desde el template
    usando sed con tres sustituciones.
  - Variables en el Makefile: PREFIX=/usr/local, VERSION=2.3.1,
    LIBNAME=mathext.

Verificar:
  1. make mathext.pc genera el archivo con los valores sustituidos.
  2. Cambiar PREFIX a /opt/custom y regenerar (make clean && make mathext.pc).
     El .pc debe reflejar el nuevo prefix.
  3. Comprobar que las variables ${prefix}, ${libdir}, ${includedir}
     dentro del .pc NO fueron alteradas por sed (solo los @PLACEHOLDERS@).
```

### Ejercicio 3 — Biblioteca completa con .pc generado por CMake

```text
Crear un proyecto CMake para una biblioteca "hashmap" con:
  - include/hashmap/hashmap.h (declaraciones de hashmap_create, hashmap_set,
    hashmap_get, hashmap_destroy)
  - src/hashmap.c (implementacion que usa malloc/free internamente)
  - hashmap.pc.in con @CMAKE_INSTALL_PREFIX@ y @PROJECT_VERSION@
  - CMakeLists.txt que:
    1. Defina project(hashmap VERSION 1.0.0 LANGUAGES C)
    2. Cree la biblioteca estatica
    3. Use configure_file() con @ONLY para generar hashmap.pc
    4. Instale la biblioteca, el header y el .pc

Verificar:
  1. cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local
  2. cmake --build build
  3. cmake --install build
  4. PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig pkg-config --cflags --libs hashmap
     Debe mostrar: -I/home/user/.local/include -L/home/user/.local/lib -lhashmap
  5. Escribir un programa test.c que use la biblioteca y compilarlo con pkg-config.
```
