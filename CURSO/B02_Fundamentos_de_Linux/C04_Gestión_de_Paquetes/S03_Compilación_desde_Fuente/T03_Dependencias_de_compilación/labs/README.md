# Lab — Dependencias de compilacion

## Objetivo

Entender la diferencia entre paquetes de desarrollo (-dev/-devel)
y paquetes runtime, los meta-paquetes de compilacion (build-essential,
Development Tools), como usar pkg-config para encontrar flags de
compilacion, y apt build-dep / dnf builddep.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Paquetes -dev vs runtime

### Objetivo

Entender que contienen los paquetes de desarrollo y por que
son necesarios para compilar.

### Paso 1.1: Que es un paquete -dev

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquetes de desarrollo ==="
echo ""
echo "Para EJECUTAR un programa:"
echo "  Solo necesita las bibliotecas runtime (.so)"
echo "  Ejemplo: libssl3, libpcre2-8-0, zlib1g"
echo ""
echo "Para COMPILAR un programa:"
echo "  Necesita las bibliotecas runtime MAS:"
echo "  - Headers (.h) para compilar"
echo "  - Bibliotecas estaticas (.a)"
echo "  - Symlinks de bibliotecas (.so sin version)"
echo "  - Archivos .pc para pkg-config"
echo ""
echo "=== Contenido de un paquete -dev ==="
echo "/usr/include/*.h         ← headers"
echo "/usr/lib/*.a             ← bibliotecas estaticas"
echo "/usr/lib/*.so            ← symlinks (sin version)"
echo "/usr/lib/pkgconfig/*.pc  ← metadatos para pkg-config"
'
```

### Paso 1.2: Convencion de nombres

```bash
docker compose exec debian-dev bash -c '
echo "=== Convencion de nombres ==="
echo ""
printf "%-15s %-22s %-22s\n" "Biblioteca" "Debian (-dev)" "RHEL (-devel)"
printf "%-15s %-22s %-22s\n" "--------------" "---------------------" "---------------------"
printf "%-15s %-22s %-22s\n" "zlib" "zlib1g-dev" "zlib-devel"
printf "%-15s %-22s %-22s\n" "OpenSSL" "libssl-dev" "openssl-devel"
printf "%-15s %-22s %-22s\n" "PCRE2" "libpcre2-dev" "pcre2-devel"
printf "%-15s %-22s %-22s\n" "libcurl" "libcurl4-openssl-dev" "libcurl-devel"
printf "%-15s %-22s %-22s\n" "readline" "libreadline-dev" "readline-devel"
printf "%-15s %-22s %-22s\n" "ncurses" "libncurses-dev" "ncurses-devel"
printf "%-15s %-22s %-22s\n" "libxml2" "libxml2-dev" "libxml2-devel"
printf "%-15s %-22s %-22s\n" "SQLite" "libsqlite3-dev" "sqlite-devel"
echo ""
echo "Debian: sufijo -dev"
echo "RHEL:   sufijo -devel"
'
```

### Paso 1.3: Buscar paquetes de desarrollo

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar paquetes -dev en Debian ==="
echo ""
echo "--- Buscar por nombre ---"
apt-cache search "ssl.*dev" 2>/dev/null | grep "^lib" | head -5
echo ""
echo "--- Buscar por nombre ---"
apt-cache search "zlib.*dev" 2>/dev/null | grep "^lib" | head -3
echo ""
echo "--- Verificar si un -dev esta instalado ---"
dpkg -l "lib*-dev" 2>/dev/null | grep "^ii" | head -10
echo ""
echo "Total paquetes -dev instalados: $(dpkg -l 'lib*-dev' 2>/dev/null | grep '^ii' | wc -l)"
'
```

### Paso 1.4: Ver contenido de un -dev

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenido de un paquete -dev ==="
echo ""
# Buscar un paquete -dev instalado
devpkg=$(dpkg -l "lib*-dev" 2>/dev/null | grep "^ii" | head -1 | awk "{print \$2}")
if [ -n "$devpkg" ]; then
    echo "--- $devpkg ---"
    echo ""
    echo "Headers (.h):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.h$" | head -5
    echo ""
    echo "Bibliotecas estaticas (.a):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.a$" | head -3
    echo ""
    echo "Symlinks (.so sin version):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.so$" | head -3
    echo ""
    echo "pkg-config (.pc):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.pc$" | head -3
else
    echo "(ningun paquete -dev instalado)"
    echo ""
    echo "Un paquete -dev tipicamente contiene:"
    echo "  /usr/include/openssl/ssl.h"
    echo "  /usr/lib/x86_64-linux-gnu/libssl.a"
    echo "  /usr/lib/x86_64-linux-gnu/libssl.so → libssl.so.3"
    echo "  /usr/lib/x86_64-linux-gnu/pkgconfig/libssl.pc"
fi
'
```

---

## Parte 2 — Toolchains base

### Objetivo

Conocer los meta-paquetes que instalan las herramientas minimas
de compilacion.

### Paso 2.1: build-essential (Debian)

```bash
docker compose exec debian-dev bash -c '
echo "=== build-essential (Debian) ==="
echo ""
echo "Meta-paquete que instala el toolchain minimo:"
echo "  - gcc (compilador C)"
echo "  - g++ (compilador C++)"
echo "  - make"
echo "  - libc6-dev (headers de la biblioteca C)"
echo "  - dpkg-dev (herramientas de empaquetado)"
echo "  + dependencias: binutils, cpp"
echo ""
echo "--- Estado ---"
dpkg -l build-essential 2>/dev/null | tail -1 || echo "(no instalado)"
echo ""
echo "--- Herramientas disponibles ---"
gcc --version 2>/dev/null | head -1 || echo "gcc: NO"
g++ --version 2>/dev/null | head -1 || echo "g++: NO"
make --version 2>/dev/null | head -1 || echo "make: NO"
echo ""
echo "Instalar: sudo apt install build-essential"
'
```

### Paso 2.2: Development Tools (RHEL)

```bash
docker compose exec alma-dev bash -c '
echo "=== Development Tools (RHEL/AlmaLinux) ==="
echo ""
echo "Grupo de paquetes equivalente a build-essential:"
echo "  - gcc"
echo "  - gcc-c++"
echo "  - make"
echo "  - autoconf, automake, libtool"
echo "  - pkgconf (pkg-config)"
echo "  - gdb (debugger)"
echo "  - binutils"
echo "  - glibc-devel"
echo ""
echo "--- Estado ---"
dnf group list --installed 2>/dev/null | grep -i "development" || echo "(no instalado)"
echo ""
echo "--- Herramientas disponibles ---"
gcc --version 2>/dev/null | head -1 || echo "gcc: NO"
make --version 2>/dev/null | head -1 || echo "make: NO"
echo ""
echo "Instalar: sudo dnf group install \"Development Tools\""
'
```

### Paso 2.3: Comparacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion de toolchains ==="
printf "%-18s %-22s %-22s\n" "Herramienta" "Debian (build-ess)" "RHEL (Dev Tools)"
printf "%-18s %-22s %-22s\n" "-----------------" "---------------------" "---------------------"
printf "%-18s %-22s %-22s\n" "Compilador C" "gcc" "gcc"
printf "%-18s %-22s %-22s\n" "Compilador C++" "g++" "gcc-c++"
printf "%-18s %-22s %-22s\n" "Build tool" "make" "make"
printf "%-18s %-22s %-22s\n" "Headers libc" "libc6-dev" "glibc-devel"
printf "%-18s %-22s %-22s\n" "Autotools" "NO incluido" "autoconf, automake"
printf "%-18s %-22s %-22s\n" "Debugger" "NO incluido" "gdb"
printf "%-18s %-22s %-22s\n" "pkg-config" "NO incluido" "pkgconf"
echo ""
echo "En Debian, herramientas extra se instalan por separado:"
echo "  sudo apt install autoconf automake libtool pkg-config cmake"
'
```

---

## Parte 3 — pkg-config y build-dep

### Objetivo

Usar pkg-config para encontrar flags de compilacion y apt build-dep
para instalar dependencias automaticamente.

### Paso 3.1: pkg-config

```bash
docker compose exec debian-dev bash -c '
echo "=== pkg-config ==="
echo ""
echo "Herramienta estandar para encontrar librerias:"
echo ""
pkg-config --version 2>/dev/null || echo "(no instalado)"
echo ""
echo "--- Verificar si una libreria esta disponible ---"
echo "pkg-config --exists libssl && echo Disponible"
pkg-config --exists libssl 2>/dev/null && echo "libssl: Disponible" || echo "libssl: No encontrada (falta libssl-dev)"
echo ""
echo "--- Flags de compilacion ---"
echo "pkg-config --cflags libssl"
pkg-config --cflags libssl 2>/dev/null || echo "  (no disponible)"
echo ""
echo "--- Flags de linkeo ---"
echo "pkg-config --libs libssl"
pkg-config --libs libssl 2>/dev/null || echo "  (no disponible)"
echo ""
echo "--- Version ---"
echo "pkg-config --modversion libssl"
pkg-config --modversion libssl 2>/dev/null || echo "  (no disponible)"
'
```

### Paso 3.2: Archivos .pc

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos .pc (pkg-config) ==="
echo ""
echo "Los archivos .pc estan en los paquetes -dev"
echo ""
echo "--- Ubicaciones ---"
echo "Debian: /usr/lib/x86_64-linux-gnu/pkgconfig/"
echo "RHEL:   /usr/lib64/pkgconfig/"
echo ""
echo "--- Librerias disponibles ---"
count=$(pkg-config --list-all 2>/dev/null | wc -l)
echo "Total: $count librerias registradas en pkg-config"
echo ""
echo "--- Primeras 10 ---"
pkg-config --list-all 2>/dev/null | head -10
echo ""
echo "Para instalar una libreria para compilacion:"
echo "  1. Buscar el paquete -dev: apt-cache search libfoo-dev"
echo "  2. Instalar: sudo apt install libfoo-dev"
echo "  3. Verificar: pkg-config --exists libfoo"
'
```

### Paso 3.3: apt build-dep

```bash
docker compose exec debian-dev bash -c '
echo "=== apt build-dep ==="
echo ""
echo "Instala TODAS las dependencias de compilacion de un paquete"
echo "que ya existe en los repos:"
echo ""
echo "  sudo apt build-dep nginx"
echo "  → Lee Build-Depends del paquete fuente"
echo "  → Instala todos los paquetes -dev necesarios"
echo ""
echo "Requiere lineas deb-src en sources.list"
echo ""
echo "--- Equivalente en RHEL ---"
echo "  sudo dnf builddep nginx"
echo ""
echo "--- Interpretar errores de configure ---"
echo ""
echo "Cuando ./configure falla:"
echo "  configure: error: pcre2 library is required"
echo ""
echo "Solucion:"
echo "  apt-cache search pcre2 | grep dev"
echo "  → libpcre2-dev"
echo "  sudo apt install libpcre2-dev"
'
```

### Paso 3.4: Compilacion en contenedores

```bash
docker compose exec debian-dev bash -c '
echo "=== Compilacion limpia en contenedores ==="
echo ""
echo "Buena practica: compilar en un contenedor para no"
echo "contaminar el host con paquetes de desarrollo"
echo ""
echo "--- Multi-stage build (Dockerfile) ---"
echo ""
echo "FROM debian:12 AS builder"
echo "RUN apt-get update && apt-get install -y \\"
echo "    build-essential libssl-dev libpcre2-dev"
echo "COPY . /src"
echo "WORKDIR /src"
echo "RUN ./configure && make -j\$(nproc)"
echo ""
echo "FROM debian:12-slim"
echo "RUN apt-get update && apt-get install -y libssl3 libpcre2-8-0"
echo "COPY --from=builder /src/programa /usr/local/bin/"
echo ""
echo "La imagen final NO tiene gcc, make, ni paquetes -dev"
echo "Solo las librerias runtime necesarias"
echo ""
echo "--- Contenedor desechable ---"
echo "docker run --rm -v \$(pwd):/src -w /src debian:12 bash -c '"
echo "    apt-get update && apt-get install -y build-essential"
echo "    ./configure && make -j\$(nproc)"
echo "'"
echo "El binario queda en el host, el contenedor se elimina"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Para compilar se necesitan **paquetes de desarrollo** (-dev
   en Debian, -devel en RHEL) que contienen headers (.h),
   bibliotecas estaticas (.a), y archivos .pc para pkg-config.

2. **build-essential** (Debian) y **Development Tools** (RHEL)
   son meta-paquetes que instalan el toolchain minimo: gcc,
   make, y headers de libc.

3. **pkg-config** encuentra flags de compilacion y linkeo de
   bibliotecas: `--cflags` (headers), `--libs` (linkeo),
   `--modversion` (version).

4. `apt build-dep <pkg>` instala automaticamente todas las
   dependencias de compilacion de un paquete que existe en
   los repos. Requiere lineas `deb-src`.

5. Cuando `./configure` falla por una libreria faltante, buscar
   el paquete -dev correspondiente:
   `apt-cache search libfoo-dev` o `dnf search libfoo-devel`.

6. Compilar en **contenedores** (multi-stage build) evita
   contaminar el sistema host con paquetes de desarrollo.
   La imagen final solo lleva las librerias runtime.
