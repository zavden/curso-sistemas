# T02 — Dockerfile Alma dev

## Objetivo

Construir una imagen de desarrollo basada en **AlmaLinux 9** (compatible RHEL 9,
soporte hasta 2032) con las mismas herramientas que la imagen Debian, para poder
compilar y probar código en ambas distribuciones.

## ¿Por qué AlmaLinux?

AlmaLinux es un fork de RHEL mantenido por la comunidad. Es binariamente compatible
con RHEL 9, lo que significa que:
- Los mismos paquetes y versiones que en RHEL
- Los mismos paths de configuración
- Compatible con RHCSA/RHCE
- Repositorios abiertos (sin suscripción de Red Hat)

Otras alternativas compatibles con RHEL: Rocky Linux, CentOS Stream.

## Diferencias de paquetes: Debian vs AlmaLinux

| Propósito | Debian (apt) | AlmaLinux (dnf) |
|---|---|---|
| Compilador C++ | `g++` | `gcc-c++` |
| Headers glibc | `libc6-dev` | `glibc-devel` |
| pkg-config | `pkg-config` | `pkgconf-pkg-config` |
| Netcat | `netcat-openbsd` | `nmap-ncat` |
| DNS tools | `dnsutils` | `bind-utils` |
| Ping | `iputils-ping` | `iputils` |
| iproute | `iproute2` | `iproute` |
| Man POSIX | `manpages-posix-dev` | No disponible directamente* |
| Vim | `vim-tiny` | `vim-minimal` (preinstalado) |
| Locales | `locales` | `glibc-langpack-en` |

\* En RHEL/AlmaLinux, las man pages POSIX no están en los repos base. Se instalan
las man pages estándar con `man-pages`.

## EPEL: repositorio extra

EPEL (Extra Packages for Enterprise Linux) proporciona paquetes que no están en
los repos base de RHEL/AlmaLinux:

```dockerfile
RUN dnf install -y epel-release && dnf clean all
```

Algunos paquetes que necesitamos solo están en EPEL (como `ltrace` en algunas
versiones).

## El Dockerfile completo

```dockerfile
FROM almalinux:9

# Locale
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Instalar EPEL para paquetes adicionales
RUN dnf install -y epel-release && dnf clean all

# Instalar todas las herramientas en una sola capa
RUN dnf install -y \
    # Compilación C/C++
    gcc gcc-c++ make cmake pkgconf-pkg-config glibc-devel \
    # Debugging
    gdb valgrind strace ltrace \
    # Documentación
    man-db man-pages \
    # Utilidades
    git curl wget vim-minimal less tree file bc \
    glibc-langpack-en \
    # Desarrollo de red
    nmap-ncat iproute iputils bind-utils tcpdump \
    # Dependencias
    ca-certificates findutils \
    && dnf clean all

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

### `dnf clean all` vs `rm -rf /var/lib/apt/lists/*`

En el ecosistema RHEL, la limpieza de caché se hace con `dnf clean all`:

```dockerfile
# Debian
RUN apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*

# AlmaLinux
RUN dnf install -y ... && dnf clean all
```

`dnf clean all` elimina los metadatos y paquetes descargados de
`/var/cache/dnf/`.

### Groups de paquetes

dnf tiene el concepto de **grupos de paquetes** que agrupan herramientas
relacionadas:

```bash
# Instalar todo el grupo "Development Tools"
dnf group install -y "Development Tools"
# Incluye: gcc, gcc-c++, make, automake, autoconf, binutils, gdb, etc.
```

No lo usamos en el Dockerfile porque instala paquetes que no necesitamos,
inflando la imagen. Preferimos instalar paquetes individuales.

### Versiones de GCC

AlmaLinux 9 incluye GCC 11 por defecto. Para versiones más recientes, se
usan Application Streams:

```bash
# Ver versiones disponibles
dnf module list gcc-toolset

# Instalar GCC 13
dnf install -y gcc-toolset-13
scl enable gcc-toolset-13 bash
gcc --version   # gcc 13.x
```

Para este curso, GCC 11 (el default) es suficiente.

### Diferencias en man pages

AlmaLinux no incluye `manpages-posix-dev` (man pages del estándar POSIX).
Las man pages estándar del sistema (`man-pages`) cubren la mayoría de funciones
C y syscalls, pero para referencia POSIX estricta, se puede consultar la
documentación online.

## Paridad funcional con Debian

Ambas imágenes deben poder:

| Operación | Debian | AlmaLinux |
|---|---|---|
| Compilar C | `gcc -o prog prog.c` | `gcc -o prog prog.c` |
| Compilar C++ | `g++ -o prog prog.cpp` | `g++ -o prog prog.cpp` |
| Compilar Rust | `cargo build` | `cargo build` |
| Debug con gdb | `gdb ./prog` | `gdb ./prog` |
| Memory check | `valgrind ./prog` | `valgrind ./prog` |
| Trazar syscalls | `strace ./prog` | `strace ./prog` |
| Man pages | `man printf` | `man printf` |
| Herramientas de red | `ping`, `dig`, `ss` | `ping`, `dig`, `ss` |

Las herramientas producen resultados idénticos (son las mismas versiones o
muy similares). Las diferencias están en los paths, la versión de glibc, y
la configuración por defecto del sistema.

## Construir la imagen

```bash
docker build -t alma-dev .

# Verificar el tamaño
docker image ls alma-dev
# ~600-800MB
```

## Comparación de tamaños

```bash
docker image ls --format "{{.Repository}}\t{{.Size}}" | grep dev
# debian-dev   ~700MB
# alma-dev     ~750MB
```

Los tamaños son similares. La diferencia está en las librerías base del sistema
y la cantidad de dependencias que cada gestor de paquetes instala.

---

## Ejercicios

### Ejercicio 1 — Construir y verificar

```bash
mkdir -p /tmp/lab/dockerfiles/alma
# (crear el Dockerfile con el contenido de arriba)

docker build -t alma-dev /tmp/lab/dockerfiles/alma

# Verificar herramientas
docker run --rm alma-dev bash -c '
echo "=== Compiladores ==="
gcc --version | head -1
g++ --version | head -1
rustc --version
cargo --version

echo ""
echo "=== Debugging ==="
gdb --version | head -1
valgrind --version

echo ""
echo "=== Sistema ==="
cat /etc/os-release | head -2
rpm -q glibc
'
```

### Ejercicio 2 — Compilar el mismo programa en ambas distros

```bash
# Programa de prueba
PROG='#include <stdio.h>
#include <sys/utsname.h>
int main(void) {
    struct utsname u;
    uname(&u);
    printf("Hello from %s (%s)\n", u.sysname, u.version);
    return 0;
}'

# Compilar en Debian
echo "$PROG" | docker run --rm -i debian-dev bash -c '
cat > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'

# Compilar en AlmaLinux
echo "$PROG" | docker run --rm -i alma-dev bash -c '
cat > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'

# Ambos compilan y ejecutan — el output muestra la versión del kernel (compartido)
```

### Ejercicio 3 — Documentar diferencias de paquetes

```bash
echo "=== Debian ==="
docker run --rm debian-dev bash -c '
dpkg -l | grep -E "^ii" | wc -l
echo "Paquetes instalados"
gcc --version | head -1
ldd --version | head -1
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm alma-dev bash -c '
rpm -qa | wc -l
echo "Paquetes instalados"
gcc --version | head -1
ldd --version | head -1
'
```
