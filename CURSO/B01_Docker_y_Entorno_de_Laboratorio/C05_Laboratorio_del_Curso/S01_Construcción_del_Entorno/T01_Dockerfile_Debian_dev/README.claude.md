# T01 — Dockerfile Debian dev

## Objetivo

Construir una imagen de desarrollo basada en **Debian 12 (bookworm)** que incluya
todas las herramientas necesarias para el curso: compilación C/C++, debugging,
herramientas de red, Rust, y documentación (man pages).

El Dockerfile completo está en `labs/Dockerfile.debian-dev`.

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
| Man pages | Excluidas (config) | Excluidas (no existen) | No incluidas |
| Soporte hasta | 2028 | 2028 | ~2027 |

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
    /etc/dpkg/dpkg.cfg.d/excludes 2>/dev/null || true
```

### ¿Por qué el orden importa?

Si instalas paquetes **antes** de modificar esta configuración, las man pages no
se instalan aunque después cambies el archivo. Esto ocurre porque dpkg ya
"decidió" no extraer las páginas durante el primer `apt-get install`.

| Orden | Resultado |
|---|---|
| `sed` → `apt-get install` | Man pages instaladas |
| `apt-get install` → `sed` → reinstall | Man pages NO instaladas |

**Solución si ya instalaste sin man pages:** Hay que reinstalar los paquetes:

```bash
apt-get install --reinstall -y $(dpkg -l | grep '^ii' | awk '{print $2}')
```

---

## Herramientas a instalar

### Compilación C/C++

| Paquete | Propósito |
|---|---|
| `gcc` | Compilador C (GNU Compiler Collection) |
| `g++` | Compilador C++ (incluye gcc como dependencia) |
| `make` | Sistema de build por excelencia |
| `cmake` | Sistema de build multiplataforma (moderno) |
| `pkg-config` | Resolver flags de compilación para librerías |
| `libc6-dev` | Headers y librerías de desarrollo de glibc |

### Debugging

| Paquete | Propósito | Requisitos runtime |
|---|---|---|
| `gdb` | GNU Debugger — breakpoints, step, inspect | Requiere `SYS_PTRACE` capability |
| `valgrind` | Detector de memory leaks y errores de memoria | No requiere capabilities extra |
| `strace` | Trazar syscalls del proceso | Requiere `SYS_PTRACE` capability |
| `ltrace` | Trazar llamadas a librerías dinámicas | Requiere `SYS_PTRACE` capability |

> **Nota sobre valgrind**: A diferencia de gdb y strace, valgrind **no** usa
> ptrace(). Utiliza instrumentación binaria dinámica (DBI) — recompila el
> binario en tiempo de ejecución para interceptar accesos a memoria. Por eso
> no necesita `SYS_PTRACE`.

### Documentación

| Paquete | Contenido |
|---|---|
| `man-db` | Comando `man` y herramientas de base de datos de man pages |
| `manpages-dev` | Man pages de funciones C de Linux (`printf(3)`, `socket(7)`) |
| `manpages-posix-dev` | Man pages del estándar POSIX |

### Utilidades

| Paquete | Comando principal |
|---|---|
| `git` | `git` — control de versiones |
| `curl` | `curl` — transferencia HTTP con SSL/TLS |
| `wget` | `wget` — descarga HTTP con SSL via libssl/gnutls |
| `vim-tiny` | `vim`, `vi` — versión mínima (~2MB) |
| `less` | `less` — paginador con búsqueda |
| `tree` | `tree` — visualizar árboles de directorios |
| `file` | `file` — identificar tipo de archivo por magic bytes |
| `bc` | `bc` — calculadora de precisión arbitraria |
| `locales` | `locale` — soporte de locale (UTF-8) |

**Locale configuration:**
```dockerfile
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
```

### Desarrollo de red

| Paquete | Comandos incluidos |
|---|---|
| `netcat-openbsd` | `nc`, `netcat` — versión OpenBSD (más portable) |
| `iproute2` | `ip`, `ss` — reemplazan a `ifconfig`/`netstat` |
| `iputils-ping` | `ping`, `ping6` |
| `dnsutils` | `dig`, `nslookup`, `host` |
| `tcpdump` | `tcpdump` — captura de tráfico (requiere `CAP_NET_RAW`) |

**`iproute2` vs `net-tools`:**

| Comando net-tools (legacy) | Comando iproute2 (moderno) |
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
USER dev
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --default-toolchain stable --component rustfmt clippy
```

### Componentes instalados

| Componente | Propósito |
|---|---|
| `rustc` | Compilador de Rust |
| `cargo` | Gestor de paquetes y build tool |
| `rustfmt` | Formateador de código |
| `clippy` | Linter con sugerencias de código idiomático |

### Estructura de directorios

```
~/.rustup/
├── toolchains/
│   └── stable-x86_64-unknown-linux-gnu/
│       ├── bin/         # rustc, cargo, rustdoc
│       └── lib/         # Librerías del toolchain
└── settings.toml        # Configuración global

~/.cargo/
├── bin/                 # Binarios: cargo, rustfmt, clippy, etc.
├── registry/            # Crates descargados
└── env                  # Variables de entorno (sourced por shell)
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
"recomendados" que no son estrictamente necesarios:

| Instalación | Tamaño estimado |
|---|---|
| Sin `--no-install-recommends` | ~800MB |
| Con `--no-install-recommends` | ~550MB |

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
pertenezcan al usuario correcto:

```dockerfile
USER dev
RUN curl ... | sh -s -- -y ...
# ~/.rustup/ y ~/.cargo/ pertenecen a dev
```

Si se instalara como root, el usuario `dev` no podría actualizar ni instalar
paquetes de Rust (`Permission denied` en `~/.cargo` y `~/.rustup`).

### `cap_add: SYS_PTRACE` en runtime

El Dockerfile no puede declarar capabilities — se añaden al ejecutar el
contenedor. `SYS_PTRACE` es necesario para herramientas que usan la syscall
`ptrace()`:

| Herramienta | Usa ptrace() | Necesita SYS_PTRACE |
|---|---|---|
| `gdb` | Sí — attach a procesos, breakpoints | Sí |
| `strace` | Sí — capturar syscalls | Sí |
| `ltrace` | Sí — interceptar llamadas a librerías | Sí |
| `valgrind` | No — usa instrumentación binaria (DBI) | No |

```bash
# Sin SYS_PTRACE: gdb falla
docker run --rm debian-dev gdb ./program
# Could not trace the inferior process — Operation not permitted

# Con SYS_PTRACE: gdb funciona
docker run --rm --cap-add SYS_PTRACE debian-dev gdb ./program
```

---

## Ejercicios

### Ejercicio 1 — Construir la imagen y verificar herramientas

```bash
# Construir desde el lab
docker build -t debian-dev -f labs/Dockerfile.debian-dev labs/

# Verificar tamaño
docker image ls debian-dev
```

**Predicción**: ¿Qué tamaño esperas para la imagen?

<details><summary>Respuesta</summary>

Entre ~600MB y ~800MB. La imagen base de Debian bookworm es ~125MB, las
herramientas de compilación (gcc, g++, cmake) añaden ~300MB, y el toolchain
de Rust (~200MB) completa el total. Es una imagen de desarrollo — no se
optimiza para producción.

</details>

```bash
# Verificar herramientas clave
docker run --rm debian-dev bash -c '
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

echo ""
echo "=== Documentación ==="
man -k printf | head -3
'
```

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

**Predicción**: ¿Produce warnings el flag `-Wextra`?

<details><summary>Respuesta</summary>

No. Este programa es limpio — no tiene variables sin usar, no ignora valores
de retorno, ni tiene otros problemas que `-Wextra` detectaría. Si añadieras
una variable declarada pero no usada (`int unused = 0;`), `-Wextra` junto con
`-Wall` la reportaría.

</details>

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

### Ejercicio 3 — Verificar gdb con y sin SYS_PTRACE

```bash
# Sin SYS_PTRACE
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
gdb -batch -ex "break main" -ex "run" /tmp/test 2>&1 | tail -5
'
```

**Predicción**: ¿Qué error produce gdb sin SYS_PTRACE?

<details><summary>Respuesta</summary>

Muestra algo como `Warning: ptrace: Operation not permitted` o
`Could not trace the inferior process`. gdb necesita la syscall `ptrace()` para
poner breakpoints y controlar la ejecución del proceso, pero Docker la bloquea
por defecto por seguridad.

</details>

```bash
# Con SYS_PTRACE
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
gdb -batch -ex "break main" -ex "run" -ex "print x" -ex "continue" /tmp/test
'
```

### Ejercicio 4 — Valgrind sin SYS_PTRACE

```bash
docker run --rm debian-dev bash -c '
cat > /tmp/leak.c << EOF
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int) * 10);
    p[0] = 42;
    // Deliberadamente no hacemos free(p)
    return 0;
}
EOF

gcc -g -o /tmp/leak /tmp/leak.c
valgrind --leak-check=full /tmp/leak
'
```

**Predicción**: ¿Funciona valgrind sin `--cap-add SYS_PTRACE`?

<details><summary>Respuesta</summary>

Sí. A diferencia de gdb y strace, valgrind **no usa ptrace()**. Utiliza
instrumentación binaria dinámica (DBI) — recompila el binario en tiempo de
ejecución para interceptar accesos a memoria. No necesita capabilities extra.

El output muestra el memory leak: `definitely lost: 40 bytes in 1 blocks`
(10 ints * 4 bytes = 40 bytes no liberados).

</details>

### Ejercicio 5 — strace y ltrace

```bash
# strace: ver syscalls de un comando simple
docker run --rm --cap-add SYS_PTRACE debian-dev strace -c ls /tmp 2>&1
```

**Predicción**: ¿Qué syscalls esperas ver con más frecuencia?

<details><summary>Respuesta</summary>

Las syscalls más frecuentes de `ls` suelen ser:
- `openat` / `close` — abrir y cerrar archivos/directorios
- `fstat` / `newfstatat` — obtener información de archivos
- `getdents64` — leer entradas del directorio
- `write` — escribir la salida a stdout
- `mmap` / `mprotect` — mapeo de memoria (carga de librerías)

</details>

```bash
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
ltrace /tmp/libtest 2>&1
'
```

### Ejercicio 6 — Explorar la estructura de rustup

```bash
docker run --rm debian-dev bash -c '
echo "=== rustup home ==="
ls ~/.rustup/

echo ""
echo "=== toolchains instalados ==="
rustup show

echo ""
echo "=== cargo binaries ==="
ls ~/.cargo/bin/
'
```

**Predicción**: ¿Qué binarios hay en `~/.cargo/bin/`?

<details><summary>Respuesta</summary>

Los binarios principales son:
- `cargo` — gestor de paquetes y build tool
- `rustc` — compilador
- `rustdoc` — generador de documentación
- `rustfmt` — formateador de código
- `cargo-fmt` — wrapper de rustfmt para cargo
- `cargo-clippy` — wrapper de clippy para cargo
- `clippy-driver` — driver interno de clippy

Todos son proxies de rustup que redirigen al toolchain activo.

</details>

### Ejercicio 7 — Proyecto Rust con Cargo

```bash
docker run --rm debian-dev bash -c '
cd /tmp
cargo new hello-cargo --name hello_cargo
cd hello-cargo

# Compilar en modo release
cargo build --release

# Ver binario
ls -la target/release/hello_cargo
file target/release/hello_cargo

# Ejecutar
./target/release/hello_cargo
'
```

**Predicción**: ¿Qué tipo de archivo es el binario compilado?

<details><summary>Respuesta</summary>

Un ejecutable ELF de 64 bits linkado dinámicamente contra glibc:
```
ELF 64-bit LSB pie executable, x86-64, ... dynamically linked, ... for GNU/Linux ...
```

Es dinámico por defecto (usa libc del sistema). Para compilar estáticamente
en Rust, se necesita el target `x86_64-unknown-linux-musl`.

</details>

### Ejercicio 8 — Comparar tamaño con y sin `--no-install-recommends`

```bash
mkdir -p /tmp/test-recommends /tmp/test-no-recommends

cat > /tmp/test-recommends/Dockerfile << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y gcc g++ make && rm -rf /var/lib/apt/lists/*
EOF

cat > /tmp/test-no-recommends/Dockerfile << 'EOF'
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends gcc g++ make && rm -rf /var/lib/apt/lists/*
EOF

docker build -t test-recommends /tmp/test-recommends
docker build -t test-no-recommends /tmp/test-no-recommends
```

**Predicción**: ¿Cuánta diferencia de tamaño esperas?

<details><summary>Respuesta</summary>

La diferencia es significativa — típicamente ~100-200MB. Sin
`--no-install-recommends`, apt instala paquetes "recomendados" como `gcc-doc`,
`libc-dev-doc`, `make-doc`, y otros que no son necesarios para compilar.

```bash
docker image ls --format "{{.Repository}}\t{{.Size}}" | grep test-
```

</details>

```bash
# Comparar
docker image ls test-recommends
docker image ls test-no-recommends

# Limpiar
docker rmi test-recommends test-no-recommends
rm -rf /tmp/test-recommends /tmp/test-no-recommends
```

### Ejercicio 9 — Man pages de funciones C

```bash
docker run --rm debian-dev bash -c '
# Buscar documentación de funciones C
man -k printf | grep "(3)"
echo "---"
man -k socket | head -5
echo "---"
man -k fork

# Ver una man page específica (sección 3 = funciones C)
MANWIDTH=80 man 3 printf | head -25
'
```

**Predicción**: ¿Qué diferencia hay entre `printf(1)` y `printf(3)`?

<details><summary>Respuesta</summary>

- `printf(1)` — sección 1: el **comando** de shell (`/usr/bin/printf`)
- `printf(3)` — sección 3: la **función C** de la librería estándar
  (`#include <stdio.h>`)

Las secciones de man: 1=comandos, 2=syscalls, 3=funciones C, 5=formatos de
archivo, 7=miscelánea, 8=administración. `manpages-dev` instala las secciones
2 y 3; `manpages-posix-dev` añade las versiones POSIX.

</details>

### Ejercicio 10 — Herramientas de red

```bash
docker run --rm debian-dev bash -c '
echo "=== ip (iproute2) ==="
ip addr show | head -10

echo ""
echo "=== ss (sockets) ==="
ss -tulpn

echo ""
echo "=== dig (DNS) ==="
dig google.com +short 2>/dev/null || echo "Sin acceso DNS"
'
```

**Predicción**: ¿Qué interfaces de red ve el contenedor?

<details><summary>Respuesta</summary>

Típicamente dos interfaces:
- `lo` — loopback (127.0.0.1)
- `eth0` — interfaz virtual del bridge de Docker (con IP del rango 172.17.x.x
  por defecto)

El contenedor tiene su propio network namespace con estas interfaces virtuales.
`ss -tulpn` probablemente no muestra nada escuchando porque el contenedor
no tiene servicios activos.

</details>

---

## Resumen de capabilities necesarias

```
┌──────────────────────────────────────────────────────────┐
│           CAPABILITIES PARA HERRAMIENTAS                 │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  SYS_PTRACE (--cap-add SYS_PTRACE):                     │
│  • gdb      — breakpoints, attach, step                 │
│  • strace   — trazar syscalls                            │
│  • ltrace   — trazar llamadas a librerías                │
│                                                          │
│  NO necesita SYS_PTRACE:                                 │
│  • valgrind — usa DBI, no ptrace                         │
│  • gcc/g++  — compilación normal                         │
│  • cargo    — compilación Rust                           │
│                                                          │
│  NET_RAW (--cap-add NET_RAW):                            │
│  • tcpdump  — captura de paquetes raw                    │
│                                                          │
└──────────────────────────────────────────────────────────┘
```
