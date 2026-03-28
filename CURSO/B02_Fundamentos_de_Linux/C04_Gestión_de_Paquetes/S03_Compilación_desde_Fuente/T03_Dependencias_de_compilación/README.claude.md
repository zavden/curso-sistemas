# T03 — Dependencias de compilación

## Errata y mejoras sobre el material base

1. **`Build隔离 con unbuild`** — Encabezado con caracteres chinos (`隔离` = "aislamiento")
   y herramienta `unbuild` que no existe. Eliminado. La sección describía `make clean`,
   `make distclean`, `make uninstall` — esos comandos se integran en la sección correspondiente.

2. **`bibliotecas estátICAS`** → `estáticas` (capitalización errónea aleatoria).

3. **`listaddo`** → `listado` (doble d).

4. **`Proveé`** → `Provee` (tilde incorrecta en la segunda e).

5. **`#include <ssl.h>`** → `#include <openssl/ssl.h>` (el header correcto de OpenSSL).

6. **`libz1`** en Dockerfile → `zlib1g` (nombre real del paquete runtime de zlib en Debian).

7. **`libz-dev`** → `zlib1g-dev` (nombre real del paquete de desarrollo de zlib en Debian).

---

## Paquetes de desarrollo vs runtime

```
┌──────────────────────────────────────────────────────────┐
│                   PAQUETES RUNTIME                        │
│                (necesarios para EJECUTAR)                  │
├──────────────────────────────────────────────────────────┤
│                                                          │
│   libssl3        → Provee libssl.so.3                    │
│   libpcre2-8-0   → Provee libpcre2-8.so.0               │
│   libc6          → Provee libc.so.6                      │
│   zlib1g         → Provee libz.so.1                      │
│                                                          │
│   Se instalan automáticamente como dependencias          │
│   del programa (nginx, curl, etc.)                       │
│                                                          │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│              PAQUETES DE DESARROLLO                       │
│             (necesarios para COMPILAR)                    │
├──────────────────────────────────────────────────────────┤
│                                                          │
│   libssl-dev   → /usr/include/openssl/*.h (headers)      │
│                + libssl.so (symlink para linking)         │
│                + /usr/lib/pkgconfig/libssl.pc             │
│                                                          │
│   libpcre2-dev → /usr/include/pcre2.h                    │
│                + libpcre2-8.so + pcre2.pc                │
│                                                          │
│   libc6-dev    → /usr/include/*.h, sys/*.h, bits/*.h     │
│                                                          │
│   gcc, make    → Compilador y build tool                 │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Qué contiene un paquete -dev

```
/usr/include/*.h              ← Headers (para #include en el código)
/usr/lib/*.a                  ← Bibliotecas estáticas (linking estático)
/usr/lib/*.so                 ← Symlinks sin versión (para el linker)
/usr/lib/pkgconfig/*.pc       ← Metadatos para pkg-config
```

El paquete runtime (`libssl3`) contiene la `.so` versionada
(`libssl.so.3.0.0`) que usa el programa en ejecución. El paquete
`-dev` (`libssl-dev`) contiene los headers y el symlink `libssl.so →
libssl.so.3` que el linker necesita durante la compilación.

### Convención de nombres

| Tipo | Debian/Ubuntu | RHEL/AlmaLinux |
|------|---------------|----------------|
| Runtime | `libfoo1` | `libfoo` |
| Desarrollo | `libfoo-dev` | `libfoo-devel` |
| Ejemplo runtime | `libssl3` | `openssl-libs` |
| Ejemplo dev | `libssl-dev` | `openssl-devel` |

```bash
# Debian: sufijo -dev
sudo apt install libssl-dev libpcre2-dev zlib1g-dev

# RHEL: sufijo -devel
sudo dnf install openssl-devel pcre2-devel zlib-devel
```

---

## Herramientas base de compilación

### Debian: build-essential

```bash
# Meta-paquete que instala el toolchain mínimo
sudo apt install build-essential

# Incluye:
# gcc         → Compilador C
# g++         → Compilador C++
# make        → GNU Make
# libc6-dev   → Headers de la biblioteca C
# dpkg-dev    → Herramientas de empaquetado Debian
#
# Dependencias transitivas:
# binutils    → ld (linker), as (assembler), objdump
# cpp         → Preprocesador C

# Verificar
gcc --version
g++ --version
make --version
```

### RHEL: Development Tools

```bash
# Grupo de paquetes equivalente a build-essential
sudo dnf group install "Development Tools"

# Incluye (más completo que build-essential):
# gcc          → Compilador C
# gcc-c++      → Compilador C++
# make         → GNU Make
# autoconf     → Generador de configure scripts
# automake     → Generador de Makefile templates
# libtool      → Soporte de bibliotecas compartidas
# pkgconf      → Equivalente a pkg-config
# gdb          → Debugger
# binutils     → ld, as, objdump, etc.
# glibc-devel  → Headers de libc
# redhat-rpm-config → Configs para builds RPM

# Verificar
gcc --version
make --version
dnf group info "Development Tools"
```

### Comparación

| Herramienta | Debian (build-essential) | RHEL (Development Tools) |
|---|---|---|
| Compilador C | `gcc` | `gcc` |
| Compilador C++ | `g++` | `gcc-c++` |
| Build tool | `make` | `make` |
| Headers libc | `libc6-dev` | `glibc-devel` |
| Autotools | No incluido | `autoconf`, `automake`, `libtool` |
| Debugger | No incluido | `gdb` |
| pkg-config | No incluido | `pkgconf` |

```bash
# Herramientas adicionales comúnmente necesarias:
# Debian (lo que falta respecto a RHEL):
sudo apt install autoconf automake libtool pkg-config cmake git

# RHEL (lo que puede faltar):
sudo dnf install cmake git
```

---

## Cómo encontrar dependencias necesarias

### 1. Leer la documentación del proyecto

```bash
# La mayoría de proyectos documenta sus dependencias
cat README.md        # Primeros pasos
cat INSTALL          # Instrucciones de instalación
cat BUILDING.md      # Build específico

# Buscar secciones clave
grep -i "dependenc\|requirement\|prerequisit\|build" README.md
```

### 2. Interpretar errores de configure

```bash
./configure
# checking for pcre2_config... no
# configure: error: pcre2 library is required

# El error dice qué falta. Proceso de resolución:
# 1. Identificar la librería: pcre2
# 2. Buscar el paquete -dev:
apt-cache search pcre2 | grep dev
# libpcre2-dev - New Perl Compatible Regular Expression Library
# 3. Instalar:
sudo apt install libpcre2-dev
# 4. Reintentar configure
```

```
Errores típicos y sus soluciones:

"SSL library not found"
  → apt: sudo apt install libssl-dev
  → dnf: sudo dnf install openssl-devel

"zlib not found"
  → apt: sudo apt install zlib1g-dev
  → dnf: sudo dnf install zlib-devel

"No such file or directory: openssl/ssl.h"
  → Falta el paquete -dev que contiene los headers

"/usr/bin/ld: cannot find -lpcre2"
  → Falta el paquete -dev que contiene el symlink .so
```

### 3. pkg-config

`pkg-config` (o `pkgconf`) es la herramienta estándar para encontrar
bibliotecas y sus flags de compilación:

```bash
# Verificar si una librería está disponible
pkg-config --exists libssl && echo "Disponible" || echo "Falta libssl-dev"

# Ver flags de compilación (headers: -I)
pkg-config --cflags libssl
# -I/usr/include/openssl

# Ver flags de linkeo (libraries: -l)
pkg-config --libs libssl
# -lssl -lcrypto

# Ver la versión
pkg-config --modversion libssl
# 3.0.11

# Todo junto (lo que pasa gcc)
pkg-config --cflags --libs libssl
# -I/usr/include/openssl -lssl -lcrypto

# Listar todas las libs disponibles
pkg-config --list-all | sort

# Ubicación de archivos .pc:
# Debian: /usr/lib/x86_64-linux-gnu/pkgconfig/
# RHEL:   /usr/lib64/pkgconfig/
```

### 4. apt build-dep / dnf builddep

Si quieres compilar un paquete que ya existe en los repos, el gestor puede
instalar todas sus dependencias de compilación automáticamente:

```bash
# Debian: instalar todas las Build-Depends de un paquete
sudo apt build-dep nginx
# Lee el campo Build-Depends del paquete fuente
# Instala todos los paquetes listados automáticamente

# Requiere líneas deb-src habilitadas en sources.list:
# deb-src http://deb.debian.org/debian bookworm main
# En Debian 12+ con formato DEB822, verificar:
grep -r "Types.*deb-src\|^deb-src" /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null
```

```bash
# RHEL: equivalente
sudo dnf builddep nginx

# O desde un .spec file local
sudo dnf builddep nginx.spec
```

---

## Dependencias de compilación comunes

### Bibliotecas de sistema

| Biblioteca | Debian (`-dev`) | RHEL (`-devel`) | Uso |
|---|---|---|---|
| zlib | `zlib1g-dev` | `zlib-devel` | Compresión (gzip) |
| OpenSSL | `libssl-dev` | `openssl-devel` | TLS/SSL, criptografía |
| PCRE2 | `libpcre2-dev` | `pcre2-devel` | Expresiones regulares |
| libcurl | `libcurl4-openssl-dev` | `libcurl-devel` | Cliente HTTP |
| readline | `libreadline-dev` | `readline-devel` | Edición de línea interactiva |
| ncurses | `libncurses-dev` | `ncurses-devel` | Interfaces de terminal (TUI) |
| libxml2 | `libxml2-dev` | `libxml2-devel` | Parsing XML |
| SQLite | `libsqlite3-dev` | `sqlite-devel` | Base de datos embebida |
| libyaml | `libyaml-dev` | `libyaml-devel` | Parsing YAML |
| libevent | `libevent-dev` | `libevent-devel` | Event loop asíncrono |

### Lenguajes de programación

```bash
# Python (headers para compilar extensiones C como numpy, lxml)
sudo apt install python3-dev python3-pip     # Debian
sudo dnf install python3-devel python3-pip   # RHEL

# Rust (toolchain propio, no usa paquetes del sistema)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source ~/.cargo/env

# Go
sudo apt install golang-go                   # Debian
sudo dnf install golang                      # RHEL

# Ruby (para gemas con extensiones nativas)
sudo apt install ruby-dev                    # Debian
sudo dnf install ruby-devel                  # RHEL

# Java / JDK
sudo apt install default-jdk                 # Debian
sudo dnf install java-17-openjdk-devel       # RHEL
```

---

## Compilación limpia en contenedores

Compilar en un contenedor evita contaminar el host con paquetes `-dev`
que solo se necesitan para compilar, no para ejecutar.

### Multi-stage build (Docker)

```dockerfile
# Etapa 1: compilación (imagen pesada con gcc, make, -dev)
FROM debian:12 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    libssl-dev \
    libpcre2-dev \
    zlib1g-dev

WORKDIR /src
COPY . .
RUN ./configure && make -j$(nproc)

# Etapa 2: runtime (imagen ligera, solo librerías runtime)
FROM debian:12-slim

RUN apt-get update && apt-get install -y \
    libssl3 \
    libpcre2-8-0 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/programa /usr/local/bin/
CMD ["programa"]

# La imagen final NO tiene gcc, make, ni paquetes -dev
# Solo las librerías runtime necesarias para ejecutar
```

### Contenedor desechable

```bash
# Compilar sin instalar nada en el host
docker run --rm \
    -v $(pwd):/src \
    -w /src \
    debian:12 bash -c '
        apt-get update && apt-get install -y build-essential libssl-dev
        ./configure && make -j$(nproc)
    '

# El binario compilado queda en $(pwd) del host
# El contenedor con todos los paquetes -dev se elimina (--rm)
```

### Limpieza del directorio de compilación

```bash
# Después de compilar, limpiar los artefactos
make clean        # Elimina .o y ejecutables (mantiene Makefile)
make distclean    # Elimina TODO generado (vuelve al estado pre-configure)
                  # Necesitas re-ejecutar ./configure después

# Si ya instalaste y quieres reinstalar otra versión:
make distclean
./configure --prefix=/usr/local
make -j$(nproc)
sudo make install
```

---

## Solución de problemas

### "gcc: command not found"

```bash
# Debian:
sudo apt install build-essential
# RHEL:
sudo dnf group install "Development Tools"
```

### "No such file or directory: openssl/ssl.h"

```bash
# Falta el paquete -dev con los headers
# Debian:
sudo apt install libssl-dev
# RHEL:
sudo dnf install openssl-devel
```

### "/usr/bin/ld: cannot find -lssl"

```bash
# 1. ¿Está instalado el paquete -dev?
pkg-config --exists libssl && echo "OK" || echo "Falta"

# 2. ¿Existe el symlink .so?
ls -la /usr/lib/x86_64-linux-gnu/libssl.so   # Debian
ls -la /usr/lib64/libssl.so                    # RHEL

# 3. ¿ldconfig la conoce?
ldconfig -p | grep libssl
```

### "undefined reference to `pthread_create`"

```bash
# Falta enlazar la librería pthreads
# Solución manual:
gcc programa.c -o programa -lpthread

# Con configure:
./configure LDFLAGS="-lpthread"
# La mayoría de configure scripts lo detectan automáticamente
```

### "configure: error: ... not found" (genérico)

```bash
# Flujo de resolución:
# 1. Leer qué librería falta en el error
# 2. Buscar el paquete -dev
apt-cache search <nombre> | grep dev     # Debian
dnf search <nombre> | grep devel         # RHEL

# 3. Instalar
sudo apt install lib<nombre>-dev         # Debian
sudo dnf install <nombre>-devel          # RHEL

# 4. Reintentar ./configure
```

---

## Labs

### Lab 1 — Paquetes -dev vs runtime

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquetes de desarrollo ==="
echo ""
echo "Para EJECUTAR: solo librerías runtime (.so versionadas)"
echo "Para COMPILAR: runtime + headers (.h) + symlinks (.so) + .pc"
echo ""
echo "=== Contenido de un -dev ==="
echo "/usr/include/*.h              ← headers"
echo "/usr/lib/*.a                  ← bibliotecas estáticas"
echo "/usr/lib/*.so                 ← symlinks (sin versión)"
echo "/usr/lib/pkgconfig/*.pc       ← metadatos pkg-config"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Convención de nombres ==="
printf "%-15s %-22s %-22s\n" "Biblioteca" "Debian (-dev)" "RHEL (-devel)"
printf "%-15s %-22s %-22s\n" "--------------" "---------------------" "---------------------"
printf "%-15s %-22s %-22s\n" "zlib" "zlib1g-dev" "zlib-devel"
printf "%-15s %-22s %-22s\n" "OpenSSL" "libssl-dev" "openssl-devel"
printf "%-15s %-22s %-22s\n" "PCRE2" "libpcre2-dev" "pcre2-devel"
printf "%-15s %-22s %-22s\n" "libcurl" "libcurl4-openssl-dev" "libcurl-devel"
printf "%-15s %-22s %-22s\n" "readline" "libreadline-dev" "readline-devel"
printf "%-15s %-22s %-22s\n" "ncurses" "libncurses-dev" "ncurses-devel"
echo ""
echo "Debian: -dev | RHEL: -devel"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenido real de un paquete -dev ==="
echo ""
devpkg=$(dpkg -l "lib*-dev" 2>/dev/null | grep "^ii" | head -1 | awk "{print \$2}")
if [ -n "$devpkg" ]; then
    echo "--- $devpkg ---"
    echo ""
    echo "Headers (.h):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.h$" | head -5
    echo ""
    echo "Bibliotecas estáticas (.a):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.a$" | head -3
    echo ""
    echo "Symlinks (.so):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.so$" | head -3
    echo ""
    echo "pkg-config (.pc):"
    dpkg -L "$devpkg" 2>/dev/null | grep "\.pc$" | head -3
else
    echo "(ningún paquete -dev instalado — ejemplo teórico)"
    echo ""
    echo "  /usr/include/openssl/ssl.h"
    echo "  /usr/lib/x86_64-linux-gnu/libssl.a"
    echo "  /usr/lib/x86_64-linux-gnu/libssl.so → libssl.so.3"
    echo "  /usr/lib/x86_64-linux-gnu/pkgconfig/libssl.pc"
fi
'
```

### Lab 2 — Toolchains base

```bash
docker compose exec debian-dev bash -c '
echo "=== build-essential (Debian) ==="
echo ""
echo "Meta-paquete: gcc, g++, make, libc6-dev, dpkg-dev"
echo ""
echo "--- Estado ---"
dpkg -l build-essential 2>/dev/null | tail -1 || echo "(no instalado)"
echo ""
echo "--- Herramientas ---"
gcc --version 2>/dev/null | head -1 || echo "gcc: NO"
g++ --version 2>/dev/null | head -1 || echo "g++: NO"
make --version 2>/dev/null | head -1 || echo "make: NO"
echo ""
echo "Instalar: sudo apt install build-essential"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Development Tools (RHEL/AlmaLinux) ==="
echo ""
echo "Grupo: gcc, gcc-c++, make, autoconf, automake, libtool, pkgconf, gdb"
echo ""
echo "--- Estado ---"
dnf group list --installed 2>/dev/null | grep -i "development" || echo "(no instalado)"
echo ""
echo "--- Herramientas ---"
gcc --version 2>/dev/null | head -1 || echo "gcc: NO"
make --version 2>/dev/null | head -1 || echo "make: NO"
echo ""
echo "Instalar: sudo dnf group install \"Development Tools\""
'
```

### Lab 3 — pkg-config y resolución de dependencias

```bash
docker compose exec debian-dev bash -c '
echo "=== pkg-config ==="
echo ""
pkg-config --version 2>/dev/null || echo "(no instalado)"
echo ""
echo "--- Total librerías disponibles ---"
count=$(pkg-config --list-all 2>/dev/null | wc -l)
echo "Total: $count librerías registradas en pkg-config"
echo ""
echo "--- Primeras 10 ---"
pkg-config --list-all 2>/dev/null | sort | head -10
echo ""
echo "--- Ejemplo: verificar libssl ---"
pkg-config --exists libssl 2>/dev/null && echo "libssl: Disponible" || echo "libssl: No (falta libssl-dev)"
pkg-config --cflags libssl 2>/dev/null && echo "" || true
pkg-config --libs libssl 2>/dev/null || true
pkg-config --modversion libssl 2>/dev/null || true
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== apt build-dep / dnf builddep ==="
echo ""
echo "Instala TODAS las dependencias de compilación de un paquete:"
echo ""
echo "  Debian: sudo apt build-dep nginx"
echo "  RHEL:   sudo dnf builddep nginx"
echo ""
echo "Requiere líneas deb-src habilitadas"
echo ""
echo "--- Resolver errores de configure ---"
echo ""
echo "Error: pcre2 library is required"
echo "  1. apt-cache search pcre2 | grep dev"
echo "  2. sudo apt install libpcre2-dev"
echo "  3. Reintentar ./configure"
'
```

### Lab 4 — Compilación en contenedores

```bash
docker compose exec debian-dev bash -c '
echo "=== Multi-stage build ==="
echo ""
echo "FROM debian:12 AS builder"
echo "RUN apt-get update && apt-get install -y \\"
echo "    build-essential libssl-dev libpcre2-dev"
echo "COPY . /src && WORKDIR /src"
echo "RUN ./configure && make -j\$(nproc)"
echo ""
echo "FROM debian:12-slim"
echo "RUN apt-get update && apt-get install -y libssl3 libpcre2-8-0"
echo "COPY --from=builder /src/programa /usr/local/bin/"
echo ""
echo "Imagen final: solo librerías runtime, sin gcc ni -dev"
echo ""
echo "--- Limpieza de compilación ---"
echo ""
echo "make clean      → elimina .o y ejecutables"
echo "make distclean  → vuelve al estado pre-configure"
echo "make uninstall  → desinstala (si el Makefile lo soporta)"
'
```

---

## Ejercicios

### Ejercicio 1 — Paquetes -dev vs runtime

Explica la diferencia entre `libssl3` y `libssl-dev`. ¿Qué contiene
cada uno? ¿Cuándo necesitas cada uno? ¿Qué pasa si intentas compilar
contra OpenSSL sin instalar `libssl-dev`?

<details><summary>Predicción</summary>

```bash
# libssl3 (runtime):
dpkg -L libssl3 | head -5
# /usr/lib/x86_64-linux-gnu/libssl.so.3        ← la .so versionada
# /usr/lib/x86_64-linux-gnu/libssl.so.3.0.0    ← la .so real
# Necesario para EJECUTAR programas que usan OpenSSL

# libssl-dev (desarrollo):
dpkg -L libssl-dev | head -10
# /usr/include/openssl/ssl.h           ← headers
# /usr/include/openssl/tls1.h
# /usr/lib/x86_64-linux-gnu/libssl.so  ← symlink (sin versión)
# /usr/lib/x86_64-linux-gnu/libssl.a   ← biblioteca estática
# /usr/lib/x86_64-linux-gnu/pkgconfig/libssl.pc
# Necesario para COMPILAR contra OpenSSL
```

Sin `libssl-dev` instalado:
- `./configure` falla: "SSL library not found"
- `gcc -c programa.c`: "fatal error: openssl/ssl.h: No such file or directory"
- `gcc programa.o -lssl`: "/usr/bin/ld: cannot find -lssl"

</details>

### Ejercicio 2 — build-essential vs Development Tools

Compara lo que incluye `build-essential` (Debian) con el grupo
"Development Tools" (RHEL). ¿Cuál es más completo? ¿Qué necesitas
instalar adicionalmente en Debian para tener paridad?

<details><summary>Predicción</summary>

| Herramienta | build-essential | Development Tools |
|---|---|---|
| gcc | Sí | Sí |
| g++/gcc-c++ | Sí | Sí |
| make | Sí | Sí |
| libc headers | Sí (libc6-dev) | Sí (glibc-devel) |
| autoconf | **No** | Sí |
| automake | **No** | Sí |
| libtool | **No** | Sí |
| pkg-config | **No** | Sí (pkgconf) |
| gdb | **No** | Sí |

Development Tools es más completo. Para paridad en Debian:
```bash
sudo apt install build-essential autoconf automake libtool pkg-config gdb
```

build-essential es intencionalmente mínimo — solo lo imprescindible
para compilar. Development Tools incluye todo el ecosistema autotools
y debugging.

</details>

### Ejercicio 3 — Interpretar errores de configure

Para cada error, identifica qué paquete falta y cómo instalarlo en
Debian y RHEL:
- `configure: error: pcre2 library is required`
- `configure: error: SSL library not found`
- `configure: error: zlib not found`

<details><summary>Predicción</summary>

| Error | Debian | RHEL |
|---|---|---|
| pcre2 not found | `sudo apt install libpcre2-dev` | `sudo dnf install pcre2-devel` |
| SSL not found | `sudo apt install libssl-dev` | `sudo dnf install openssl-devel` |
| zlib not found | `sudo apt install zlib1g-dev` | `sudo dnf install zlib-devel` |

Proceso de resolución:
1. Leer el nombre de la librería en el error
2. Buscar: `apt-cache search <nombre> | grep dev` (Debian)
   o `dnf search <nombre> | grep devel` (RHEL)
3. Instalar el paquete encontrado
4. Reintentar `./configure`

Nota: los nombres en RHEL suelen ser más predecibles (`<nombre>-devel`).
En Debian, a veces el nombre tiene prefijos o sufijos inesperados
(ej: `zlib1g-dev` en vez de `libz-dev`).

</details>

### Ejercicio 4 — pkg-config

Lista todas las librerías disponibles en pkg-config. ¿Cuántas hay?
Elige 3 y muestra sus `--cflags`, `--libs` y `--modversion`. ¿Dónde
están los archivos `.pc`?

<details><summary>Predicción</summary>

```bash
pkg-config --list-all | wc -l    # varía: ~50-200 según paquetes instalados
pkg-config --list-all | sort | head -10

# Ejemplo 1: zlib
pkg-config --cflags zlib         # (vacío — headers en ruta estándar)
pkg-config --libs zlib           # -lz
pkg-config --modversion zlib     # 1.2.13

# Ejemplo 2: libssl
pkg-config --cflags libssl       # -I/usr/include/openssl (o vacío)
pkg-config --libs libssl         # -lssl -lcrypto
pkg-config --modversion libssl   # 3.0.11

# Ejemplo 3: libcurl
pkg-config --cflags libcurl      # (vacío)
pkg-config --libs libcurl        # -lcurl
pkg-config --modversion libcurl  # 7.88.1

# Archivos .pc:
ls /usr/lib/x86_64-linux-gnu/pkgconfig/   # Debian
ls /usr/lib64/pkgconfig/                    # RHEL
```

Los archivos `.pc` se instalan con los paquetes `-dev`/`-devel`.
Sin el paquete de desarrollo, `pkg-config --exists` retorna false.

</details>

### Ejercicio 5 — apt build-dep

¿Qué hace `apt build-dep nginx`? ¿Qué campo del paquete fuente lee?
¿Qué necesitas tener habilitado en `sources.list`? ¿Cuál es el
equivalente en RHEL?

<details><summary>Predicción</summary>

```bash
# apt build-dep nginx:
# 1. Busca el paquete fuente "nginx" en los repositorios
# 2. Lee el campo "Build-Depends" del archivo debian/control
# 3. Instala TODOS los paquetes listados
#
# Build-Depends típico de nginx:
#   debhelper, libpcre2-dev, libssl-dev, zlib1g-dev, ...

# Requiere líneas deb-src habilitadas:
# deb-src http://deb.debian.org/debian bookworm main
# Sin ellas: "E: Unable to find a source package for nginx"

# Equivalente RHEL:
sudo dnf builddep nginx
# Lee el .spec del SRPM (Source RPM) y su campo BuildRequires
```

`build-dep`/`builddep` es ideal cuando quieres recompilar un paquete
que ya existe en los repos con opciones diferentes (ej: nginx con
módulos extra).

</details>

### Ejercicio 6 — Contenido de un paquete -dev

Elige un paquete `-dev` instalado y examina su contenido con `dpkg -L`.
Clasifica los archivos en: headers, bibliotecas estáticas, symlinks,
y archivos pkg-config.

<details><summary>Predicción</summary>

```bash
# Ejemplo: libssl-dev
dpkg -L libssl-dev

# Headers:
# /usr/include/openssl/ssl.h
# /usr/include/openssl/tls1.h
# /usr/include/openssl/err.h
# ... (~80+ headers)

# Bibliotecas estáticas:
# /usr/lib/x86_64-linux-gnu/libssl.a
# /usr/lib/x86_64-linux-gnu/libcrypto.a

# Symlinks (para el linker):
# /usr/lib/x86_64-linux-gnu/libssl.so → libssl.so.3
# /usr/lib/x86_64-linux-gnu/libcrypto.so → libcrypto.so.3

# pkg-config:
# /usr/lib/x86_64-linux-gnu/pkgconfig/libssl.pc
# /usr/lib/x86_64-linux-gnu/pkgconfig/libcrypto.pc
# /usr/lib/x86_64-linux-gnu/pkgconfig/openssl.pc
```

El symlink sin versión (`libssl.so`) es crucial: el linker (`ld`) busca
`-lssl` como `libssl.so` (sin versión). Sin el `-dev`, solo existe
`libssl.so.3` y el linker no la encuentra.

</details>

### Ejercicio 7 — Resolver dependencias de un proyecto real

Clona `jq` desde GitHub y analiza qué dependencias necesita. ¿Qué
archivos de configuración indican las dependencias? ¿Qué busca
`configure.ac`?

<details><summary>Predicción</summary>

```bash
git clone --depth 1 https://github.com/jqlang/jq.git
cd jq

# Archivos relevantes:
ls configure.ac Makefile.am README.md BUILDING.md 2>/dev/null

# En configure.ac, buscar macros que detectan dependencias:
grep -E "PKG_CHECK|AC_CHECK_LIB|AC_CHECK_HEADER" configure.ac
# PKG_CHECK_MODULES([ONIGURUMA], [oniguruma])
# → Necesita libonig-dev (Debian) o oniguruma-devel (RHEL)

# Sin la dependencia:
autoreconf -fi
./configure 2>&1 | tail -5
# checking for ONIGURUMA... no
# configure: error: oniguruma is required
```

La macro `PKG_CHECK_MODULES` usa `pkg-config` internamente para
verificar si la librería existe. Si `oniguruma.pc` no está en el
sistema (porque falta `libonig-dev`), configure falla.

</details>

### Ejercicio 8 — Multi-stage Docker build

Escribe un Dockerfile multi-stage para compilar un programa C simple
que usa OpenSSL. ¿Qué paquetes van en la etapa builder? ¿Cuáles en
la etapa runtime? ¿Cuánto más pequeña es la imagen final?

<details><summary>Predicción</summary>

```dockerfile
# Etapa 1: builder (~300MB con gcc + -dev)
FROM debian:12 AS builder
RUN apt-get update && apt-get install -y \
    build-essential libssl-dev
WORKDIR /src
COPY programa.c .
RUN gcc programa.c -o programa -lssl -lcrypto

# Etapa 2: runtime (~80MB sin gcc ni -dev)
FROM debian:12-slim
RUN apt-get update && apt-get install -y \
    libssl3 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/programa /usr/local/bin/
CMD ["programa"]
```

Builder: `build-essential` + `libssl-dev` (gcc, make, headers, .a, .so)
Runtime: solo `libssl3` (la .so runtime)

La imagen final es ~80MB vs ~300MB del builder. No contiene gcc, make,
headers, ni bibliotecas estáticas — solo lo necesario para ejecutar.

</details>

### Ejercicio 9 — Tabla de referencia de dependencias

Crea una tabla de referencia rápida con 8 librerías comunes: nombre,
paquete Debian, paquete RHEL, para qué se usa, y qué header provee.

<details><summary>Predicción</summary>

| Librería | Debian `-dev` | RHEL `-devel` | Uso | Header principal |
|---|---|---|---|---|
| zlib | `zlib1g-dev` | `zlib-devel` | Compresión | `<zlib.h>` |
| OpenSSL | `libssl-dev` | `openssl-devel` | TLS/cripto | `<openssl/ssl.h>` |
| PCRE2 | `libpcre2-dev` | `pcre2-devel` | Regex | `<pcre2.h>` |
| libcurl | `libcurl4-openssl-dev` | `libcurl-devel` | HTTP | `<curl/curl.h>` |
| readline | `libreadline-dev` | `readline-devel` | CLI interactivo | `<readline/readline.h>` |
| ncurses | `libncurses-dev` | `ncurses-devel` | TUI | `<ncurses.h>` |
| libxml2 | `libxml2-dev` | `libxml2-devel` | XML | `<libxml/parser.h>` |
| SQLite | `libsqlite3-dev` | `sqlite-devel` | DB embebida | `<sqlite3.h>` |

Patrón: en Debian el prefijo suele ser `lib<nombre>-dev`; en RHEL suele
ser `<nombre>-devel`. Excepción notable: `zlib1g-dev` en Debian.

</details>

### Ejercicio 10 — Panorama: ciclo de vida de dependencias

Describe el ciclo completo de dependencias al compilar desde fuente:
desde identificar qué falta → instalar `-dev` → compilar → determinar
runtime deps → empaquetar con `--requires`. ¿Cómo se conecta con
los tópicos anteriores (T01 y T02)?

<details><summary>Predicción</summary>

```
1. IDENTIFICAR dependencias
   - Leer README/INSTALL del proyecto
   - Ejecutar ./configure → leer errores
   - Buscar paquete -dev: apt-cache search <nombre> | grep dev

2. INSTALAR dependencias de compilación
   - sudo apt install build-essential libfoo-dev libbar-dev
   - O automáticamente: sudo apt build-dep <paquete>

3. COMPILAR (T01: patrón clásico)
   - ./configure --prefix=/usr/local
   - make -j$(nproc)

4. DETERMINAR dependencias runtime
   - ldd ./programa → lista las .so necesarias
   - dpkg -S /path/to/libfoo.so.X → nombre del paquete runtime

5. EMPAQUETAR (T02: checkinstall/fpm)
   - sudo checkinstall --requires="libc6,libfoo1" -D --default
   - O: make install DESTDIR=/tmp/staging && fpm ...

6. DISTRIBUIR
   - El .deb/.rpm incluye las dependencias runtime declaradas
   - dpkg -i / apt install resuelve las deps al instalar
```

Conexión: T01 (compilar) necesita saber QUÉ instalar → T03 (este tópico).
T02 (empaquetar) necesita saber las deps RUNTIME → `ldd` + `dpkg -S` de
este tópico. Los tres tópicos forman un flujo completo.

</details>
