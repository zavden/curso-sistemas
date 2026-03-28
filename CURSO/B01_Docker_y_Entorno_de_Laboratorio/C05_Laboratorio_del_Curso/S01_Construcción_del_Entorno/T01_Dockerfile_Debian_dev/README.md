# T01 — Dockerfile Debian dev

## Objetivo

Construir una imagen de desarrollo basada en **Debian 12 (bookworm)** que incluya
todas las herramientas necesarias para el curso: compilación C/C++, debugging,
herramientas de red, Rust, y documentación (man pages).

## Imagen base

```dockerfile
FROM debian:bookworm
```

Debian bookworm tiene soporte hasta 2028. Es una base estable y predecible para
desarrollo. Usamos la versión completa (no slim) porque necesitamos herramientas
de desarrollo.

## Habilitar man pages

Por defecto, las imágenes Docker de Debian **excluyen las man pages** para reducir
tamaño. Para un entorno de desarrollo donde necesitamos documentación, hay que
revertir esta exclusión **antes** de instalar paquetes:

```dockerfile
# Revertir la exclusión de man pages de Docker Debian
RUN sed -i 's/^path-exclude \/usr\/share\/man/#path-exclude \/usr\/share\/man/' \
    /etc/dpkg/dpkg.cfg.d/excludes 2>/dev/null || true && \
    apt-get update && apt-get install -y --no-install-recommends man-db && \
    rm -rf /var/lib/apt/lists/*
```

Si instalas paquetes **antes** de modificar esta configuración, las man pages no
se instalan y hay que reinstalar los paquetes para obtenerlas.

## Herramientas a instalar

### Compilación C/C++

| Paquete | Propósito |
|---|---|
| `gcc` | Compilador C (GNU Compiler Collection) |
| `g++` | Compilador C++ |
| `make` | Sistema de build por excelencia |
| `cmake` | Sistema de build multiplataforma |
| `pkg-config` | Resolver flags de compilación para librerías |
| `libc6-dev` | Headers y librerías de desarrollo de glibc |

### Debugging

| Paquete | Propósito |
|---|---|
| `gdb` | GNU Debugger — breakpoints, step, inspect |
| `valgrind` | Detector de memory leaks y errores de memoria |
| `strace` | Trazar syscalls del proceso |
| `ltrace` | Trazar llamadas a librerías dinámicas |

### Documentación

| Paquete | Propósito |
|---|---|
| `man-db` | Comando `man` para leer man pages |
| `manpages-dev` | Man pages de funciones C de Linux |
| `manpages-posix-dev` | Man pages del estándar POSIX |

### Utilidades

| Paquete | Propósito |
|---|---|
| `git` | Control de versiones |
| `curl`, `wget` | Transferencia HTTP |
| `vim-tiny` | Editor de texto |
| `less` | Paginador |
| `tree` | Visualizar árboles de directorios |
| `file` | Identificar tipo de archivo |
| `bc` | Calculadora de precisión arbitraria |
| `locales` | Soporte de locale (UTF-8) |

### Desarrollo de red

| Paquete | Propósito |
|---|---|
| `netcat-openbsd` | Herramienta de red multiuso (nc) |
| `iproute2` | `ip`, `ss` — herramientas modernas de red |
| `iputils-ping` | `ping` |
| `dnsutils` | `dig`, `nslookup` — herramientas DNS |
| `tcpdump` | Captura de tráfico de red |

## Instalación de Rust

Rust se instala con **rustup** como usuario non-root. rustup gestiona el toolchain
y permite actualizar, instalar components, y cambiar entre versiones:

```dockerfile
# Instalar como usuario dev, no como root
USER dev
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy
```

- `rustfmt`: formateador de código Rust
- `clippy`: linter de Rust (análisis estático)
- rustup instala en `~/.rustup/` (toolchains) y `~/.cargo/` (binarios, cargo)

## El Dockerfile completo

```dockerfile
FROM debian:bookworm

# Locale
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Habilitar man pages (excluidas por defecto en Docker Debian)
RUN sed -i 's/^path-exclude \/usr\/share\/man/#path-exclude \/usr\/share\/man/' \
    /etc/dpkg/dpkg.cfg.d/excludes 2>/dev/null || true

# Instalar todas las herramientas en una sola capa
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Compilación C/C++
    gcc g++ make cmake pkg-config libc6-dev \
    # Debugging
    gdb valgrind strace ltrace \
    # Documentación
    man-db manpages-dev manpages-posix-dev \
    # Utilidades
    git curl wget vim-tiny less tree file bc locales \
    # Desarrollo de red
    netcat-openbsd iproute2 iputils-ping dnsutils tcpdump \
    # Dependencias
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Crear usuario de desarrollo
RUN useradd -m -s /bin/bash dev

# Cambiar a usuario non-root
USER dev
WORKDIR /home/dev

# Instalar Rust via rustup
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy

# Añadir cargo al PATH
ENV PATH="/home/dev/.cargo/bin:${PATH}"

WORKDIR /home/dev/workspace

CMD ["/bin/bash"]
```

## Notas sobre el Dockerfile

### `--no-install-recommends`

Reduce significativamente el tamaño. Sin este flag, apt instala paquetes
"recomendados" que no son estrictamente necesarios (~200MB extra):

```bash
# Sin --no-install-recommends: ~800MB
# Con --no-install-recommends: ~550MB
```

### Una sola capa de `RUN apt-get`

Combinar `apt-get update` + `install` + limpieza en un solo `RUN` evita que
los archivos temporales de apt queden en capas intermedias:

```dockerfile
# BIEN: una capa, limpieza incluida
RUN apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*

# MAL: la capa 1 contiene los índices de apt aunque se borren en la capa 2
RUN apt-get update && apt-get install -y ...
RUN rm -rf /var/lib/apt/lists/*
```

### Usuario non-root para Rust

rustup se instala como el usuario `dev` para que los archivos de Rust
pertenezcan al usuario correcto. Si se instalara como root, el usuario `dev`
no podría actualizar ni instalar paquetes de Rust:

```dockerfile
USER dev
RUN curl ... | sh -s -- -y ...
# ~/.rustup/ y ~/.cargo/ pertenecen a dev
```

### `cap_add: SYS_PTRACE` en runtime

El Dockerfile no puede declarar capabilities — se añaden al ejecutar el
contenedor. `SYS_PTRACE` es necesario para:
- `gdb`: attach a procesos, breakpoints
- `strace`: trazar syscalls
- `valgrind`: instrumentar la ejecución del programa

```bash
# Sin SYS_PTRACE
docker run --rm debian-dev gdb ./program
# Could not trace the inferior process — Operation not permitted

# Con SYS_PTRACE
docker run --rm --cap-add SYS_PTRACE debian-dev gdb ./program
# Funciona correctamente
```

## Construir la imagen

```bash
# Desde el directorio que contiene el Dockerfile
docker build -t debian-dev .

# Verificar el tamaño
docker image ls debian-dev
# ~600-800MB (herramientas de desarrollo + Rust)
```

## Verificar las herramientas

```bash
docker run --rm --cap-add SYS_PTRACE debian-dev bash -c '
echo "=== Compiladores ==="
gcc --version | head -1
g++ --version | head -1
rustc --version
cargo --version

echo ""
echo "=== Build tools ==="
make --version | head -1
cmake --version | head -1

echo ""
echo "=== Debugging ==="
gdb --version | head -1
valgrind --version

echo ""
echo "=== Utilidades ==="
git --version
curl --version | head -1

echo ""
echo "=== Red ==="
ip -V 2>&1
ss --version 2>&1 | head -1
'
```

---

## Ejercicios

### Ejercicio 1 — Construir la imagen y verificar

```bash
# Crear directorio para el Dockerfile
mkdir -p /tmp/lab/dockerfiles/debian

# Copiar el Dockerfile (usar el contenido de arriba)
# ...

# Construir
docker build -t debian-dev /tmp/lab/dockerfiles/debian

# Verificar herramientas clave
docker run --rm debian-dev gcc --version | head -1
docker run --rm debian-dev rustc --version
docker run --rm debian-dev man -k printf | head -3
```

### Ejercicio 2 — Hello World en C y Rust

```bash
# C
docker run --rm debian-dev bash -c '
cat > /tmp/hello.c << EOF
#include <stdio.h>
int main(void) {
    printf("Hello from Debian C!\n");
    return 0;
}
EOF
gcc -o /tmp/hello /tmp/hello.c && /tmp/hello
'

# Rust
docker run --rm debian-dev bash -c '
cat > /tmp/hello.rs << EOF
fn main() {
    println!("Hello from Debian Rust!");
}
EOF
rustc -o /tmp/hello_rs /tmp/hello.rs && /tmp/hello_rs
'
```

### Ejercicio 3 — Verificar gdb con SYS_PTRACE

```bash
# Sin SYS_PTRACE: falla
docker run --rm debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) { int x = 42; printf("%d\n", x); return 0; }
EOF
gcc -g -o /tmp/test /tmp/test.c
echo "run" | gdb -batch /tmp/test 2>&1 | tail -3
'

# Con SYS_PTRACE: funciona
docker run --rm --cap-add SYS_PTRACE debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) { int x = 42; printf("%d\n", x); return 0; }
EOF
gcc -g -o /tmp/test /tmp/test.c
echo -e "break main\nrun\nprint x\ncontinue\nquit" | gdb -batch /tmp/test 2>&1 | tail -10
'
```
