# T03 — Dependencias de compilación

## Paquetes de desarrollo vs paquetes runtime

Para compilar software se necesitan **paquetes de desarrollo** (headers,
bibliotecas estáticas, herramientas de compilación) que no son necesarios para
ejecutar el programa una vez compilado:

```
Para EJECUTAR nginx:
  - libc6            (biblioteca C, runtime)
  - libpcre2-8-0     (regex, runtime)
  - libssl3          (SSL, runtime)

Para COMPILAR nginx:
  - Todo lo anterior, MÁS:
  - gcc              (compilador)
  - make             (build tool)
  - libc6-dev        (headers de libc)
  - libpcre2-dev     (headers de pcre2)
  - libssl-dev       (headers de openssl)

Los paquetes -dev contienen:
  /usr/include/*.h        ← headers para compilar
  /usr/lib/*.a            ← bibliotecas estáticas
  /usr/lib/*.so           ← symlinks de bibliotecas
  /usr/lib/pkgconfig/*.pc ← metadatos para pkg-config
```

### La convención de nombres

| Distribución | Paquete runtime | Paquete desarrollo |
|---|---|---|
| Debian/Ubuntu | `libfoo1` | `libfoo-dev` |
| RHEL/AlmaLinux | `libfoo` | `libfoo-devel` |

```bash
# Debian: -dev
sudo apt install libssl-dev libpcre2-dev libz-dev

# RHEL: -devel
sudo dnf install openssl-devel pcre2-devel zlib-devel
```

## Herramientas base de compilación

### Debian: build-essential

```bash
# build-essential instala el toolchain mínimo para compilar
sudo apt install build-essential

# Incluye:
# - gcc (compilador C)
# - g++ (compilador C++)
# - make
# - libc6-dev (headers de la biblioteca C)
# - dpkg-dev (herramientas de empaquetado Debian)
# Y sus dependencias:
# - binutils (linker, assembler)
# - cpp (preprocesador C)

# Verificar
gcc --version
make --version
```

### RHEL: Development Tools

```bash
# El grupo "Development Tools" es el equivalente de build-essential
sudo dnf group install "Development Tools"

# Incluye:
# - gcc
# - gcc-c++
# - make
# - autoconf, automake
# - libtool
# - pkgconf (equivalente de pkg-config)
# - gdb (debugger)
# - binutils
# - glibc-devel

# Verificar
gcc --version
make --version
```

### Comparación

| Herramienta | Debian (build-essential) | RHEL (Development Tools) |
|---|---|---|
| Compilador C | gcc | gcc |
| Compilador C++ | g++ | gcc-c++ |
| Build tool | make | make |
| Headers de libc | libc6-dev | glibc-devel |
| Autotools | No incluido | autoconf, automake, libtool |
| Debugger | No incluido | gdb |
| pkg-config | No incluido | pkgconf |

```bash
# Herramientas adicionales comúnmente necesarias (ambas distros):
sudo apt install autoconf automake libtool pkg-config cmake  # Debian
sudo dnf install autoconf automake libtool pkgconf cmake     # RHEL
```

## Encontrar qué dependencias necesitas

### Leer el README/INSTALL

```bash
# La mayoría de proyectos documenta sus dependencias
cat README.md      # o README
cat INSTALL        # instrucciones de compilación
cat BUILDING.md    # o BUILDING

# Buscar secciones como:
# "Dependencies", "Requirements", "Build Requirements",
# "Prerequisites", "Build Dependencies"
```

### Interpretar errores de configure

Cuando `./configure` falla, el mensaje dice qué falta:

```bash
./configure
# ...
# checking for pcre2_config... no
# configure: error: pcre2 library is required

# ¿Qué paquete instalar?

# Debian: buscar el paquete -dev
apt-cache search pcre2 | grep dev
# libpcre2-dev - New Perl Compatible Regular Expression Library - development files
sudo apt install libpcre2-dev

# RHEL: buscar el paquete -devel
dnf search pcre2 | grep devel
# pcre2-devel.x86_64 : Development files for pcre2
sudo dnf install pcre2-devel
```

### pkg-config

`pkg-config` (o `pkgconf`) es la herramienta estándar para encontrar
bibliotecas y sus flags de compilación:

```bash
# Ver si una biblioteca está disponible para compilar
pkg-config --exists libssl && echo "Disponible" || echo "Falta"

# Ver los flags de compilación
pkg-config --cflags libssl
# -I/usr/include/openssl

# Ver los flags de linkeo
pkg-config --libs libssl
# -lssl -lcrypto

# Ver la versión
pkg-config --modversion libssl
# 3.0.11

# Los archivos .pc que usa pkg-config están en:
ls /usr/lib/x86_64-linux-gnu/pkgconfig/    # Debian
ls /usr/lib64/pkgconfig/                    # RHEL
```

### apt build-dep (Debian)

Si quieres compilar un paquete que ya existe en los repos de Debian, `apt`
puede instalar todas sus dependencias de compilación automáticamente:

```bash
# Instalar todas las dependencias para compilar nginx
sudo apt build-dep nginx

# Esto lee el paquete fuente de nginx y su campo Build-Depends,
# luego instala todos los paquetes listados

# Requiere que las líneas deb-src estén habilitadas en sources.list
```

### dnf builddep (RHEL)

```bash
# Equivalente en RHEL
sudo dnf builddep nginx

# O desde un .spec file
sudo dnf builddep nginx.spec

# Requiere que el paquete srpm esté disponible o se especifique el spec
```

## Dependencias de compilación comunes

### Bibliotecas que casi siempre se necesitan

| Biblioteca | Debian | RHEL | Para qué |
|---|---|---|---|
| zlib | `zlib1g-dev` | `zlib-devel` | Compresión |
| OpenSSL | `libssl-dev` | `openssl-devel` | TLS/SSL, criptografía |
| PCRE2 | `libpcre2-dev` | `pcre2-devel` | Expresiones regulares |
| libcurl | `libcurl4-openssl-dev` | `libcurl-devel` | HTTP client |
| readline | `libreadline-dev` | `readline-devel` | Edición de línea interactiva |
| ncurses | `libncurses-dev` | `ncurses-devel` | Interfaces de terminal |
| libxml2 | `libxml2-dev` | `libxml2-devel` | Parsing XML |
| SQLite | `libsqlite3-dev` | `sqlite-devel` | Base de datos embebida |

### Lenguajes de programación

```bash
# Python (headers para compilar extensiones C)
sudo apt install python3-dev          # Debian
sudo dnf install python3-devel        # RHEL

# Rust (no usa paquetes del sistema, tiene su propio toolchain)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Go
sudo apt install golang-go            # Debian
sudo dnf install golang               # RHEL

# Node.js (para compilar módulos nativos)
sudo apt install nodejs npm            # Debian
sudo dnf install nodejs npm            # RHEL
```

## Compilación limpia en contenedores

Una buena práctica es compilar dentro de un contenedor para no contaminar el
sistema host con paquetes de desarrollo:

```dockerfile
# Multi-stage build: compilar en una imagen, ejecutar en otra
FROM debian:12 AS builder
RUN apt-get update && apt-get install -y \
    build-essential libssl-dev libpcre2-dev
COPY . /src
WORKDIR /src
RUN ./configure && make -j$(nproc)

FROM debian:12-slim
RUN apt-get update && apt-get install -y libssl3 libpcre2-8-0
COPY --from=builder /src/programa /usr/local/bin/
# La imagen final no tiene gcc, make, ni paquetes -dev
```

```bash
# O simplemente compilar en un contenedor desechable
docker run --rm -v $(pwd):/src -w /src debian:12 bash -c '
    apt-get update && apt-get install -y build-essential
    ./configure && make -j$(nproc)
'
# El binario compilado queda en el directorio actual del host
# El contenedor con todos los paquetes de desarrollo se elimina
```

---

## Ejercicios

### Ejercicio 1 — Instalar herramientas base

```bash
# Debian:
dpkg -l build-essential 2>/dev/null | grep '^ii' && echo "Instalado" || echo "No instalado"

# RHEL:
dnf group list installed 2>/dev/null | grep "Development Tools" && echo "Instalado" || echo "No instalado"

# Verificar herramientas
gcc --version 2>/dev/null || echo "gcc no instalado"
make --version 2>/dev/null || echo "make no instalado"
pkg-config --version 2>/dev/null || echo "pkg-config no instalado"
```

### Ejercicio 2 — Buscar paquetes de desarrollo

```bash
# ¿Cuál es el paquete -dev/-devel para estas bibliotecas?

# Debian:
apt-cache search 'zlib.*dev' | grep '^lib'
apt-cache search 'ssl.*dev' | grep '^lib'
apt-cache search 'curl.*dev' | grep '^lib'

# RHEL:
# dnf search 'zlib.*devel'
# dnf search 'openssl.*devel'
# dnf search 'curl.*devel'
```

### Ejercicio 3 — pkg-config

```bash
# ¿Qué bibliotecas tiene pkg-config disponibles?
pkg-config --list-all | wc -l

# Información de zlib (si está instalado el -dev)
pkg-config --modversion zlib 2>/dev/null || echo "zlib-dev no instalado"
pkg-config --cflags --libs zlib 2>/dev/null
```
