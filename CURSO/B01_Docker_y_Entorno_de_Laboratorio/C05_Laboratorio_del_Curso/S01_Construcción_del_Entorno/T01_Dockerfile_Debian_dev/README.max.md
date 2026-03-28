# T01 — Dockerfile Debian dev

## Objetivo

Construir una imagen de desarrollo basada en **Debian 12 (bookworm)** que incluya
todas las herramientas necesarias para el curso: compilación C/C++, debugging,
herramientas de red, Rust, y documentación (man pages).

---

## Imagen base

```dockerfile
FROM debian:bookworm
```

### ¿Por qué Debian bookworm?

| Aspecto | Debian bookworm | Debian slim | Alpine |
|---|---|---|---|
| Tamaño base | ~125MB | ~80MB | ~7MB |
| Librerías C | glibc | glibc | musl |
| Compatibilidad | Completa | Completa | Limitada |
| Soporte hasta | 2028 | 2028 | ~2027 |
| Uso en producción | Alto | Alto | Medio |

Se usa la versión completa (no slim) porque las imágenes slim **excluyen man pages,
localization data, y documentación** — todo lo cual es necesario en un entorno
de desarrollo. Además, algunas herramientas de debugging tienen dependencias
que no existen en slim.

---

## Habilitar man pages

Por defecto, las imágenes Docker de Debian **excluyen las man pages** para reducir
tamaño. Esta exclusión está configurada en `/etc/dpkg/dpkg.cfg.d/excludes`:

```bash
# Contenido del archivo excludes
path-exclude /usr/share/man/*
path-exclude /usr/share/doc/*
path-exclude /usr/share/info/*
```

Para un entorno de desarrollo donde necesitamos documentación, hay que
revertir esta exclusión **antes** de instalar paquetes:

```dockerfile
# Revertir la exclusión de man pages de Docker Debian
RUN sed -i 's/^path-exclude \/usr\/share\/man/#path-exclude \/usr\/share\/man/' \
    /etc/dpkg/dpkg.cfg.d/excludes 2>/dev/null || true && \
    apt-get update && apt-get install -y --no-install-recommends man-db && \
    rm -rf /var/lib/apt/lists/*
```

### ¿Por qué el orden importa?

Si instalas paquetes **antes** de modificar esta configuración, las man pages no
se instalan aunque después cambies el archivo. Esto ocurre porque dpkg ya
"decidió" no extraer las páginas durante el primer `apt-get install`.

| Orden | Resultado |
|---|---|
| `sed` → `apt-get install` | Man pages instaladas ✅ |
| `apt-get install` → `sed` → reinstall | Man pages NO instaladas ❌ |

**Solución si ya instalaste sin man pages:** Hay que reinstalar los paquetes:

```bash
# Forzar reinstall de todos los paquetes para obtener man pages
apt-get install --reinstall -y $(dpkg -l | grep '^ii' | awk '{print $2}')
```

---

## Herramientas a instalar

### Compilación C/C++

| Paquete | Proviene de | Propósito |
|---|---|---|
| `gcc` | gcc package | Compilador C (GNU Compiler Collection) |
| `g++` | g++ package | Compilador C++ (incluye gcc como dependencia) |
| `make` | make package | Sistema de build por excelencia |
| `cmake` | cmake package | Sistema de build multiplataforma (moderno) |
| `pkg-config` | pkg-config package | Resolver flags de compilación para librerías |
| `libc6-dev` | libc-dev package | Headers y librerías de desarrollo de glibc |

**Dependencias implícitas importantes:**
- `gcc` depende de `cpp`, `libgcc-12-dev`, `gcc-12`
- `g++` depende de `libstdc++-12-dev`, `gcc-12`
- `make` depende de `make-guile` (para builds paralelos)

### Debugging

| Paquete | Propósito | Limitaciones |
|---|---|---|
| `gdb` | GNU Debugger — breakpoints, step, inspect | Requiere `SYS_PTRACE` capability |
| `valgrind` | Detector de memory leaks y errores de memoria | No funciona conmusl libc |
| `strace` | Trazar syscalls del proceso | Requiere `SYS_PTRACE` capability |
| `ltrace` | Trazar llamadas a librerías dinámicas | No funciona con static linking |

### Documentación

| Paquete | Contenido | Tamaño aprox. |
|---|---|---|
| `man-db` | Demonio y comando `man` | ~1MB |
| `manpages-dev` | Man pages de funciones C de Linux (`printf(3)`, `socket(7)`) | ~15MB |
| `manpages-posix-dev` | Man pages del estándar POSIX | ~5MB |

### Utilidades

| Paquete | Comando principal | Paquetes relacionados |
|---|---|---|
| `git` | `git` | ~40MB con dependencias |
| `curl` | `curl` | SSL/TLS support integrado |
| `wget` | `wget` | SSL via `warc` |
| `vim-tiny` | `vim`, `vi` | Versión mínima (~2MB) vs vim (~15MB) |
| `less` | `less` | Paginador con búsqueda |
| `tree` | `tree` | Visualizar árboles de directorios |
| `file` | `file` | Identificar tipo de archivo por magic bytes |
| `bc` | `bc` | Calculadora de precisión arbitraria |
| `locales` | `locale` | Soporte de locale (UTF-8) |

**Locale configuration:**
```dockerfile
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
```

Esto habilita UTF-8 para evitar errores como:
```
bash: warning: setlocale: LC_ALL: cannot change locale (en_US.UTF-8)
```

### Desarrollo de red

| Paquete | Comandos incluidos | Notas |
|---|---|---|
| `netcat-openbsd` | `nc`, `netcat` | Versión OpenBSD (más portable que traditional) |
| `iproute2` | `ip`, `ss`, `route`, `arp` | Reemplaza a `net-tools` (`ifconfig`, `route`) |
| `iputils-ping` | `ping`, `ping6` | ICMP echo |
| `dnsutils` | `dig`, `nslookup`, `host` | Consulta DNS |
| `tcpdump` | `tcpdump` | Captura de tráfico (requiere `CAP_NET_RAW`) |

**`iproute2` vs `net-tools`:**

| Comando net-tools | Comando iproute2 equivalente |
|---|---|
| `ifconfig` | `ip addr` |
| `route -n` | `ip route` |
| `arp -a` | `ip neigh` |
| `netstat -tulpn` | `ss -tulpn` |

---

## Instalación de Rust

Rust se instala con **rustup** como usuario non-root. rustup gestiona el toolchain
y permite actualizar, instalar componentes, y cambiar entre versiones:

```dockerfile
# Instalar como usuario dev, no como root
USER dev
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy
```

### Componentes instalados

| Componente | Propósito |
|---|---|
| `rustc` | Compilador de Rust |
| `cargo` | Gestor de paquetes y build tool |
| `rustfmt` | Formateador de código (equivalente a `gofmt` en Go) |
| `clippy` | Linter con sugerencias de código idiomático |

### Estructura de directorios

```
~/.rustup/
├── toolchains/          # Multiple toolchains (stable, nightly, version específica)
│   └── stable-x86_64-unknown-linux-gnu/
│       ├── bin/         # rustc, cargo, rustdoc
│       ├── lib/         # Librerías del toolchain
│       └── libexec/     # Soporte interno
└── settings.toml        # Configuración global

~/.cargo/
├── bin/                 # Binarios instalados via cargo install
│   ├── cargo-fmt
│   └── boot-image
├── registry/            # Crates descargados
└── env                 # Variables de entorno (sourced por shell)
```

### Cambiar toolchain después de instalación

```bash
# Ver toolchains instalados
rustup show

# Cambiar a versión específica
rustup default 1.75.0

# Instalar nightly
rustup toolchain install nightly
rustup default nightly

# Actualizar stable
rustup update stable
```

---

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

---

## Notas sobre el Dockerfile

### `--no-install-recommends`

Reduce significativamente el tamaño. Sin este flag, apt instala paquetes
"recomendados" que no son estrictamente necesarios (~200MB extra):

| Instalación | Tamaño estimado |
|---|---|
| Sin `--no-install-recommends` | ~800MB |
| Con `--no-install-recommends` | ~550MB |
| Diferencia | ~250MB |

**Paquetes "recomendados" que se excluyen:**
- `gcc` recomienda `gcc-doc`, `libc-dev-doc`
- `make` recomienda `make-doc`
- `gdb` recomienda `gdb-doc`, `gdbserver`

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

**¿Por qué importa?** Cada capa Docker permanece en la imagen final aunque
"borres" archivos. Los archivos solo desaparecen de la vista, pero ocupan
espacio en el filesystem de la capa.

```bash
# Ver tamaño real de cada capa
docker history debian-dev
```

### Usuario non-root para Rust

rustup se instala como el usuario `dev` para que los archivos de Rust
pertenezcan al usuario correcto. Si se instalara como root:

```dockerfile
# PROBLEMA: archivos pertenecen a root
USER root
RUN curl ... | sh -s -- -y ...

# Resultado:
# drwxr-xr-x  2 root root 4096 ... /root/.rustup
# drwxr-xr-x  2 root root 4096 ... /root/.cargo
```

Luego cuando el usuario `dev` intenta usar cargo:
```
error: could not create CARGO_HOME: Permission denied
error: could not create RUSTUP_HOME: Permission denied
```

### `cap_add: SYS_PTRACE` en runtime

El Dockerfile no puede declarar capabilities — se añaden al ejecutar el
contenedor. `SYS_PTRACE` es necesario para:

| Herramienta | Función que requiere SYS_PTRACE |
|---|---|
| `gdb` | `ptrace()` para attach a procesos, breakpoints de hardware |
| `strace` | `ptrace()` para capturar syscalls |
| `valgrind` | `ptrace()` para instrumentar la ejecución |

```bash
# Sin SYS_PTRACE
docker run --rm debian-dev gdb ./program
# Could not trace the inferior process — Operation not permitted

# Con SYS_PTRACE
docker run --rm --cap-add SYS_PTRACE debian-dev gdb ./program
# Funciona correctamente
```

---

## Construir la imagen

```bash
# Desde el directorio que contiene el Dockerfile
docker build -t debian-dev .

# Construir con no-cache (forzar rebuild completo)
docker build --no-cache -t debian-dev .

# Ver proceso de build paso a paso
docker build --progress=plain -t debian-dev .

# Verificar el tamaño
docker image ls debian-dev
# REPOSITORY   TAG      SIZE
# debian-dev   latest   ~600-800MB
```

---

## Verificar las herramientas

### Script completo de verificación

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
strace -V 2>&1 | head -1
ltrace -V 2>&1 | head -1

echo ""
echo "=== Documentación ==="
man -k printf | head -3

echo ""
echo "=== Utilidades ==="
git --version
curl --version | head -1
wget --version | head -1

echo ""
echo "=== Red ==="
ip -V 2>&1
ss --version 2>&1 | head -1
nc -h 2>&1 | head -1
dig -v 2>&1 | head -1
tcpdump --version 2>&1 | head -1
'
```

### Verificar capabilities y permisos

```bash
# Ver capabilities del contenedor
docker run --rm --cap-add SYS_PTRACE debian-dev capsh --print

# Output esperado:
# Current: = cap_chown,cap_dac_override,...+sys_ptrace
# Bounding set = ...+sys_ptrace
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
docker run --rm debian-dev g++ --version | head -1
docker run --rm debian-dev rustc --version
docker run --rm debian-dev man -k printf | head -3
docker run --rm debian-dev make --version | head -1
docker run --rm debian-dev cmake --version | head -1

# Ver tamaño
docker image ls debian-dev
```

**Criterios de éxito:**
- `gcc --version` muestra GCC 12.x o superior
- `rustc --version` muestra Rust 1.70 o superior
- `man -k printf` devuelve resultados (printf(3), printf(1))

---

### Ejercicio 2 — Hello World en C y Rust

**C:**
```bash
docker run --rm debian-dev bash -c '
cat > /tmp/hello.c << EOF
#include <stdio.h>

int main(void) {
    printf("Hello from Debian C!\n");
    return 0;
}
EOF

gcc -Wall -Wextra -o /tmp/hello /tmp/hello.c
/tmp/hello
'
```

**Rust:**
```bash
docker run --rm debian-dev bash -c '
cat > /tmp/hello.rs << EOF
fn main() {
    println!("Hello from Debian Rust!");
}
EOF

rustc -o /tmp/hello_rs /tmp/hello.rs
/tmp/hello_rs
'
```

**Compilación y ejecución con make (C):**
```bash
docker run --rm debian-dev bash -c '
mkdir -p /tmp/project && cd /tmp/project

cat > Makefile << EOF
CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = hello

all: $(TARGET)

$(TARGET): hello.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
EOF

cat > hello.c << EOF
#include <stdio.h>

int main(void) {
    printf("Hello from Debian with make!\n");
    return 0;
}
EOF

make
./hello
make clean
'
```

---

### Ejercicio 3 — Verificar gdb con y sin SYS_PTRACE

**Sin SYS_PTRACE (debe fallar en operaciones de trace):**
```bash
docker run --rm debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) {
    int x = 42;
    printf("Value: %d\n", x);
    return 0;
}
EOF

gcc -g -o /tmp/test /tmp/test.c

# Intentar attach con gdb en modo batch
echo "attach 1" | gdb -batch /tmp/test 2>&1 | grep -E "(Attaching|Permission)"
'
```

**Con SYS_PTRACE (funciona correctamente):**
```bash
docker run --rm --cap-add SYS_PTRACE debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) {
    int x = 42;
    printf("Value: %d\n", x);
    return 0;
}
EOF

gcc -g -o /tmp/test /tmp/test.c

# Debugging con breakpoints
gdb -batch -ex "break main" -ex "run" -ex "print x" -ex "continue" /tmp/test
'
```

---

### Ejercicio 4 — Investigar dependencias de binarios

```bash
# Ver dependencias dinámicas de gcc
docker run --rm debian-dev bash -c '
ldd $(which gcc)
'

# Ver dependencias de un binario compilado
docker run --rm debian-dev bash -c '
cat > /tmp/deps.c << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *buf = malloc(100);
    strcpy(buf, "test");
    printf("%s\n", buf);
    free(buf);
    return 0;
}
EOF

gcc -o /tmp/deps /tmp/deps.c
ldd /tmp/deps
'
```

---

### Ejercicio 5 — Verificar strace y ltrace

```bash
# strace: ver syscalls de un comando simple
docker run --rm --cap-add SYS_PTRACE debian-dev strace -c ls /tmp 2>&1

# ltrace: ver llamadas a librerías
docker run --rm --cap-add SYS_PTRACE debian-dev bash -c '
cat > /tmp/libtest.c << EOF
#include <stdio.h>
#include <string.h>

int main(void) {
    char s1[] = "hello";
    char s2[] = "world";
    printf("%s %s\n", s1, s2);
    return 0;
}
EOF

gcc -o /tmp/libtest /tmp/libtest.c
ltrace ./libtest 2>&1
'
```

---

### Ejercicio 6 — Explorar la estructura de rustup

```bash
docker run --rm --user dev debian-dev bash -c '
echo "=== rustup home ==="
ls -la ~/.rustup/

echo ""
echo "=== rustup toolchains ==="
ls -la ~/.rustup/toolchains/

echo ""
echo "=== cargo home ==="
ls -la ~/.cargo/

echo ""
echo "=== default toolchain ==="
rustup show

echo ""
echo "=== cargo binaries ==="
ls ~/.cargo/bin/
'
```

---

### Ejercicio 7 — Construir un proyecto Rust con Cargo

```bash
docker run --rm --user dev debian-dev bash -c '
cd /tmp

# Crear nuevo proyecto
cargo new hello-cargo --name hello_cargo
cd hello-cargo

# Compilar
cargo build --release

# Ver binary
ls -la target/release/hello_cargo

# Ejecutar
./target/release/hello_cargo

# Ver dependencias
cargo tree
'
```

---

### Ejercicio 8 — Comparar tamaño con y sin --no-install-recommends

```bash
# Crear Dockerfile sin --no-install-recommends
mkdir -p /tmp/test-recommends
cat > /tmp/test-recommends/Dockerfile << EOF
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc g++ make
EOF

docker build -t test-recommends /tmp/test-recommends
docker image ls test-recommends

# Comparar con versión con --no-install-recommends
mkdir -p /tmp/test-no-recommends
cat > /tmp/test-no-recommends/Dockerfile << EOF
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends gcc g++ make
EOF

docker build -t test-no-recommends /tmp/test-no-recommends
docker image ls test-no-recommends
```

---

### Ejercicio 9 — Verificar man pages de funciones C

```bash
docker run --rm debian-dev bash -c '
# Buscar documentación de funciones de red
man -k socket | head -5
man -k fork

# Ver man page específica
man -k "printf" | grep "(3)"
MANWIDTH=80 man 3 printf | head -20
'
```

---

### Ejercicio 10 — Probar herramientas de red

```bash
# Probar nc (netcat) como servidor y cliente
docker run --rm debian-dev bash -c '
# Terminal 1: servidor
nc -l -p 12345 &
SERVER_PID=$!

# Terminal 2: cliente (enviar y recibir)
echo "Hello via nc" | nc localhost 12345 &
sleep 1

# Verificar que el mensaje llegó
kill $SERVER_PID 2>/dev/null
'

# Probar ss (socket statistics)
docker run --rm debian-dev ss -tulpn

# Probar dig
docker run --rm debian-dev dig google.com +short
```
