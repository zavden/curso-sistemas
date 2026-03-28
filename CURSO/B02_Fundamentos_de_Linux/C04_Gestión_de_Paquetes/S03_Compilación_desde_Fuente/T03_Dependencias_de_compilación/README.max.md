# T03 — Dependencias de compilación

## Paquetes de desarrollo vs runtime

```
┌─────────────────────────────────────────────────────────────┐
│                   PAQUETES RUNTIME                          │
│                   (necesarios para EJECUTAR)               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   libssl3         → Proveé libssl.so.3                    │
│   libpcre2-8-0   → Proveé libpcre2.so.8                  │
│   libc6           → Proveé libc.so.6                       │
│                                                             │
│   Se instalan automáticamente con el programa              │
│   pkg: nginx, curl, httpd, etc.                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                PAQUETES DE DESARROLLO                       │
│               (necesarios para COMPILAR)                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   libssl-dev    → Proveé /usr/include/openssl/*.h          │
│                  + libssl.so (symlink para linking)         │
│                                                             │
│   libpcre2-dev  → Proveé /usr/include/pcre2.h             │
│                  + libpcre2.so + pcre2.h                   │
│                                                             │
│   libc6-dev     → Proveé /usr/include/*.h                  │
│                  + bits/*.h, sys/*.h                        │
│                                                             │
│   gcc, make     → El compilador y build system             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```
Los paquetes -dev/-devel contienen:
  /usr/include/*.h     → Headers (necesarios para #include)
  /usr/lib/*.a         → Bibliotecas estátICAS (para linking estático)
  /usr/lib/*.so        → Symlinks de bibliotecas (para linking dinámico)
  /usr/lib/pkgconfig/*.pc  → Metadatos para pkg-config
```

## Convenciones de nombres

| Tipo | Debian/Ubuntu | RHEL/AlmaLinux |
|---|---|---|
| Runtime | `libfoo1` | `libfoo` |
| Desarrollo | `libfoo-dev` | `libfoo-devel` |
| Ejemplo runtime | `libssl3` | `openssl-libs` |
| Ejemplo dev | `libssl-dev` | `openssl-devel` |

```bash
# Debian: buscar e instalar
apt-cache search 'libssl' | grep -E "^libssl|^libssl-dev"
sudo apt install libssl-dev

# RHEL: buscar e instalar
dnf search openssl | grep -E "devel|libs"
sudo dnf install openssl-devel
```

## Herramientas base de compilación

### Debian: build-essential

```bash
# El conjunto mínimo para compilar
sudo apt install build-essential

# Incluye:
# gcc         → Compilador C
# g++         → Compilador C++
# make        → GNU make
# libc6-dev   → Headers de la biblioteca C
# dpkg-dev    → Herramientas de empaquetado Debian
#
# Dependencias transitivas:
# binutils    → ld (linker), as (assembler), objdump
# cpp         → Preprocesador C
# gcc-12      → El compilador real (gcc es un symlink)
```

```bash
# Verificar instalación
gcc --version
g++ --version
make --version
dpkg -l build-essential | grep '^ii'
```

### RHEL: Development Tools

```bash
# El grupo de herramientas para compilar
sudo dnf group install "Development Tools"

# Incluye:
# gcc          → Compilador C
# gcc-c++      → Compilador C++
# make         → GNU make
# autoconf     → Generador de configure scripts
# automake     → Generador de Makefile.am
# libtool      → Script de soporte de bibliotecas
# pkgconf      → Equivalente a pkg-config
# gdb          → Debugger
# binutils     → ld, as, objdump, etc.
# glibc-devel  → Headers de libc
# redhat-rpm-config → Configs de build para RPMs
```

```bash
# Verificar
gcc --version
make --version
dnf group info "Development Tools"
```

### Comparación: Debian vs RHEL

| Herramienta | Debian | RHEL |
|---|---|---|
| Compilador C | `gcc` | `gcc` |
| Compilador C++ | `g++` | `gcc-c++` |
| Build tool | `make` | `make` |
| Headers libc | `libc6-dev` | `glibc-devel` |
| Autotools | `autoconf`, `automake`, `libtool` | `autoconf`, `automake`, `libtool` |
| pkg-config | `pkg-config` | `pkgconf` (pkg-config también disponible) |
| Debugger | `gdb` (no en build-essential) | `gdb` |
| CMake | No incluido | No incluido |

```bash
# Herramientas adicionales comúnmente necesarias:
# Debian
sudo apt install autoconf automake libtool pkg-config cmake git

# RHEL
sudo dnf install autoconf automake libtool pkgconf cmake git
```

## Cómo encontrar dependencias necesarias

### 1. Leer la documentación del proyecto

```bash
# Casi todo proyecto tiene documentación de build
cat README.md        # Primeros pasos
cat INSTALL         # Instrucciones de instalación
cat BUILDING.md     # Instrucciones específicas de build
cat CONTRIBUTING.md  # Para contribuidores

# Buscar secciones específicas
grep -i "dependenc" README.md
grep -i "build" README.md | head -20
```

### 2. Interpretar errores de configure

```bash
./configure
# ...
# checking for pcre2_config... no
# configure: error: pcre2 library is required
# Solución: instalar libpcre2-dev / pcre2-devel
```

```
Error típico:
configure: error: "SSL library not found"

Cómo resolver:
1. Identificar la librería: openssl → libssl
2. Instalar versión dev: apt: libssl-dev / dnf: openssl-devel
3. Reintentar ./configure
```

### 3. Buscar el paquete -dev/-devel

```bash
# Debian: buscar en cache
apt-cache search pcre2 | grep dev
# libpcre2-dev - New Perl Compatible Regular Expression Library

# Instalar
sudo apt install libpcre2-dev

# RHEL: buscar
dnf search pcre2 | grep devel
# pcre2-devel.x86_64 : PCRE2 development files

# Instalar
sudo dnf install pcre2-devel
```

### 4. apt build-dep (Debian)

```bash
# Instalar TODAS las dependencias de compilación de un paquete
sudo apt build-dep nginx

# Funciona porque:
# 1. Lee el campo Build-Depends del paquete nginx en el repo
# 2. Instala cada paquete listaddo automáticamente

# Requiere líneas deb-src habilitadas en sources.list:
# deb-src http://deb.debian.org/debian bookworm main

# Si build-dep no funciona:
# 1. Verificar que tienes deb-src
grep deb-src /etc/apt/sources.list

# 2. Si no están, agregarlas o usar:
sudo apt build-dep nginx
# E: Unable to find package source. You may need to add deb-src lines

# 3. Habilitar deb-src:
sed 's/^deb /deb-src /' /etc/apt/sources.list | sudo tee /etc/apt/sources.list.d/deb-src.list
sudo apt-get update
```

### 5. dnf builddep (RHEL)

```bash
# Instalar todas las dependencias de compilación de un paquete
sudo dnf builddep nginx

# O desde un archivo .spec
sudo dnf builddep nginx.spec

# Funciona con:
# 1. El paquete source RPM (.srpm) disponible en repos
# 2. El .spec file si lo tienes localmente

# Si no está disponible:
sudo dnf install epel-release
# EPEL puede tener los srpm necesarios

# En AlmaLinux/Rocky:
sudo dnf install almalinux-release-epel
```

### 6. pkg-config

```bash
# pkg-config encuentra flags de compilación para bibliotecas
# Información almacenada en archivos .pc

# Verificar si una librería está disponible
pkg-config --exists libssl && echo "OK" || echo "Falta libssl-dev"

# Ver flags de compilación (headers: -I)
pkg-config --cflags libssl
# -I/usr/include/openssl

# Ver flags de linking (libraries: -l)
pkg-config --libs libssl
# -lssl -lcrypto

# Ver versión
pkg-config --modversion libssl
# 3.0.11

# Toda la información en una línea
pkg-config --cflags --libs libssl
# -I/usr/include/openssl -lssl -lcrypto

# Listar todas las libs disponibles
pkg-config --list-all | sort
```

```bash
# Ubicación de archivos .pc:
# Debian:
ls /usr/lib/x86_64-linux-gnu/pkgconfig/

# RHEL:
ls /usr/lib64/pkgconfig/
```

## Dependencias comunes

### Bibliotecas de sistema

| Biblioteca | Debian | RHEL | Uso |
|---|---|---|---|
| zlib | `zlib1g-dev` | `zlib-devel` | Compresión (gzip, zlib) |
| OpenSSL | `libssl-dev` | `openssl-devel` | TLS/SSL, criptografía |
| PCRE2 | `libpcre2-dev` | `pcre2-devel` | Expresiones regulares |
| libcurl | `libcurl4-openssl-dev` | `libcurl-devel` | Cliente HTTP |
| readline | `libreadline-dev` | `readline-devel` | Edición interactiva de línea |
| ncurses | `libncurses-dev` | `ncurses-devel` | Interfaces TUI |
| libxml2 | `libxml2-dev` | `libxml2-devel` | Parsing XML |
| SQLite | `libsqlite3-dev` | `sqlite-devel` | Base de datos embebida |
| libyaml | `libyaml-dev` | `libyaml-devel` | Parsing YAML |
| libevent | `libevent-dev` | `libevent-devel` | Event loop |

### Lenguajes de programación

```bash
# Python (para compilar extensiones C como numpy, lxml, etc.)
sudo apt install python3-dev python3-pip     # Debian
sudo dnf install python3-devel python3-pip   # RHEL

# Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env

# Go
sudo apt install golang-go                   # Debian
sudo dnf install golang                      # RHEL

# Ruby (para gemas nativas)
sudo apt install ruby-dev                    # Debian
sudo dnf install ruby-devel                 # RHEL
gem install bundler

# Java / JDK (para Maven, Gradle)
sudo apt install default-jdk                 # Debian
sudo dnf install java-17-openjdk-devel      # RHEL
```

## Compilación en contenedores (best practice)

Compilar en un contenedor evita contaminar el sistema host con paquetes de desarrollo:

### Multi-stage build (Docker)

```dockerfile
# Etapa 1: compilador
FROM debian:12 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libpcre2-dev \
    libz-dev

WORKDIR /src
COPY . .
RUN ./configure && make -j$(nproc)

# Etapa 2: runtime (sin herramientas de build)
FROM debian:12-slim

RUN apt-get update && apt-get install -y \
    libssl3 \
    libpcre2-8-0 \
    libz1

COPY --from=builder /src/programa /usr/local/bin/
CMD ["programa"]
```

### Compilación desechable con Docker

```bash
# Compilar sin instalar nada en el host
docker run --rm \
    -v $(pwd):/src \
    -w /src \
    debian:12 bash -c '
        apt-get update && apt-get install -y build-essential libssl-dev
        ./configure && make -j$(nproc)
    '

# El binario queda en $(pwd) del host
# El contenedor con todos los paquetes -dev se elimina (--rm)
```

### Build隔离 con unbuild

```bash
# Algunos proyectos tienen makefiles que facilitan la limpieza
make clean           # solo limpieza de objetos
make distclean       # limpieza total (vuelve al estado pre-configure)
make uninstall       # desinstalar si fue instalado

# Antes de reinstalar una nueva versión:
make distclean
./configure
make -j$(nproc)
sudo make install
```

## Solución de problemas

### Error: "gcc: command not found"

```bash
# Solución Debian:
sudo apt install build-essential

# Solución RHEL:
sudo dnf group install "Development Tools"
```

### Error: "No such file or directory: openssl/ssl.h"

```bash
# El paquete de desarrollo no está instalado
# Debian:
sudo apt install libssl-dev

# RHEL:
sudo dnf install openssl-devel
```

### Error: "library not found: -lssl"

```bash
# Puede ser que:
# 1. La librería no está instalada (install -dev)
# 2. pkg-config no la encuentra
pkg-config --exists libssl && echo "OK" || echo "Falta"
ldconfig -p | grep libssl
```

### Error: "undefined reference to `pthread_create`"

```bash
# Falta la librería pthreads
# Solución: agregar -lpthread o -pthread

# Muchos configure scripts lo detectan automáticamente
# Si no, al compilar manualmente:
gcc programa.c -o programa -lpthread

# Algunos proyectos necesitan:
./configure LDFLAGS="-lpthread"
```

---

## Ejercicios

### Ejercicio 1 — Instalar herramientas de compilación

```bash
# 1. Verificar si build-essential / Development Tools está instalado
# Debian:
dpkg -l build-essential | grep '^ii' && echo "build-essential instalado" || echo "Falta"

# RHEL:
dnf group list installed | grep "Development Tools" && echo "Instalado" || echo "Falta"

# 2. Ver versiones
gcc --version | head -1
g++ --version | head -1
make --version | head -1

# 3. Si no está instalado, instalar
# Debian:
sudo apt install build-essential

# RHEL:
sudo dnf group install "Development Tools"
```

### Ejercicio 2 — Encontrar paquetes de desarrollo

```bash
# Elegir una librería y buscar su paquete -dev

# 1. Intentar compilar algo que falle
cd /tmp
git clone --depth 1 https://github.com/jqlang/jq.git
cd jq
autoreconf -fi
./configure 2>&1 | tail -10
# ¿Qué error aparece?

# 2. Identificar la librería faltante

# 3. Buscar el paquete -dev
# Debian:
apt-cache search oniguruma | grep dev

# RHEL:
dnf search oniguruma | grep devel

# 4. Instalar la dependencia
sudo apt install libonig-dev    # Debian
# sudo dnf install oniguruma-devel  # RHEL

# 5. Reintentar configure
./configure

# 6. Limpiar
cd /tmp && rm -rf jq/
```

### Ejercicio 3 — Comparar pkg-config entre distros

```bash
# 1. Listar todas las bibliotecas disponibles en pkg-config
pkg-config --list-all | sort

# 2. Contar cuántas hay
pkg-config --list-all | wc -l

# 3. Buscar una específica
pkg-config --exists libz && echo "zlib disponible" || echo "zlib no disponible"
pkg-config --modversion zlib 2>/dev/null

# 4. Ver los flags completos
pkg-config --cflags --libs libssl

# 5. Comparar con la salida de ldconfig
ldconfig -p | grep -E "libssl|libcrypto"
```

### Ejercicio 4 — Investigar dependencias de un proyecto real

```bash
# Elegir un proyecto de GitHub y analizar sus dependencias

# 1. Clonar un proyecto ligero
cd /tmp
git clone --depth 1 https://github.com/jqlang/jq.git
cd jq

# 2. Leer la documentación de build
cat README.md | grep -A 20 "Build"

# 3. Ver qué archivos de configuración tiene
ls *.ac *.am configure* Makefile* 2>/dev/null

# 4. Si tiene configure.ac, ver qué dependencias busca
grep -E "PKG_CHECK|AC_CHECK_LIB|AC_CHECK_HEADER" configure.ac 2>/dev/null

# 5. Limpiar
cd /tmp && rm -rf jq/
```

### Ejercicio 5 — Compilar en contenedor

```bash
# 1. Crear un proyecto simple de prueba
mkdir /tmp/test-compile && cd /tmp/test-compile
cat > test.c << 'EOF'
#include <stdio.h>
#include <ssl.h>  // esto debería fallar sin libssl-dev

int main() {
    printf("OpenSSL version: %s\n", OPENSSL_VERSION_TEXT);
    return 0;
}
EOF

# 2. Intentar compilar en el host (va a fallar)
gcc test.c -o test 2>&1 | grep "ssl.h"

# 3. Compilar en un contenedor Debian
docker run --rm \
    -v /tmp/test-compile:/src \
    -w /src \
    debian:12 bash -c '
        apt-get update && apt-get install -y libssl-dev build-essential
        gcc test.c -o test -lssl -lcrypto
        ./test
    '

# 4. Verificar que el binario quedó en /tmp/test-compile
ls -la /tmp/test-compile/test

# 5. Limpiar
cd /tmp && rm -rf test-compile/
```
