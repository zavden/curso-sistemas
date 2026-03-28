# T02 — Dockerfile Alma dev

## Objetivo

Construir una imagen de desarrollo basada en **AlmaLinux 9** (compatible RHEL 9,
soporte hasta 2032) con las mismas herramientas que la imagen Debian, para poder
compilar y probar código en ambas distribuciones.

---

## ¿Por qué AlmaLinux?

AlmaLinux es un fork de RHEL mantenido por la comunidad. Es binariamente compatible
con RHEL 9, lo que significa que:

| Característica | Descripción |
|---|---|
| Compatibilidad binaria | Los mismos binarios que RHEL 9 funcionan |
| Repositorios abiertos | Sin suscripción de Red Hat necesaria |
| Ciclo de soporte | Hasta 2032 (vida útil extendida) |
| Certificaciones | Compatible con certificaciones RHCSA/RHCE |
| FOSS | 100% open source, gobernanza comunitaria |

### Alternativas compatibles con RHEL

| Distribución | Diferencia principal | Sponsor |
|---|---|---|
| AlmaLinux | Fork directo de RHEL, gobernanza comunitaria | CloudLinux Inc. |
| Rocky Linux | Fork directo de RHEL, gobernanza comunitaria | Rocky Enterprise Software Foundation |
| CentOS Stream | Rolling release basada en RHEL | Red Hat (IBM) |

Para este curso usamos AlmaLinux por su estabilidad y ciclo de soporte largo.

---

## Imagen base

```dockerfile
FROM almalinux:9
```

### Tamaños comparados

| Imagen | Tamaño base | Con dev tools |
|---|---|---|
| `almalinux:9` | ~107MB | ~750MB |
| `almalinux:9-minimal` | ~52MB | ~700MB |
| `rockylinux:9` | ~107MB | ~750MB |

Se usa la imagen completa porque `minimal` requiere pasos adicionales para
configure `systemd` y otras herramientas base.

---

## Diferencias de paquetes: Debian vs AlmaLinux

### Gestión de paquetes

| Aspecto | Debian (apt/dpkg) | AlmaLinux (dnf/rpm) |
|---|---|---|
| Gestor de paquetes | `apt-get`, `apt-cache` | `dnf`, `yum` (alias) |
| Instalador individual | `dpkg -i package.rpm` | `rpm -i package.rpm` |
| Buscar paquete | `apt-cache search name` | `dnf search name` |
| Info de paquete | `apt-cache show pkg` | `dnf info pkg` |
| Dependencias | Resolución automática | Resolución automática |
| Archivos de sistema | `/etc/apt/sources.list` | `/etc/yum.repos.d/*.repo` |

### Equivalencias de paquetes

| Propósito | Debian (apt) | AlmaLinux (dnf) | Notas |
|---|---|---|---|
| Compilador C | `gcc` | `gcc` | Mismo paquete |
| Compilador C++ | `g++` | `gcc-c++` | Nombre diferente |
| Headers glibc | `libc6-dev` | `glibc-devel` | Nombres diferentes |
| pkg-config | `pkg-config` | `pkgconf-pkg-config` | Paquetes separados en RHEL |
| CMake | `cmake` | `cmake` | Mismo paquete |
| Make | `make` | `make` | Mismo paquete |
| GDB | `gdb` | `gdb` | Mismo paquete |
| Valgrind | `valgrind` | `valgrind` | Mismo paquete |
| strace | `strace` | `strace` | Mismo paquete |
| ltrace | `ltrace` | `ltrace` | Mismo paquete |
| Netcat | `netcat-openbsd` | `nmap-ncat` | Funcionalmente equivalentes |
| DNS tools | `dnsutils` | `bind-utils` | `dig`, `nslookup` en ambos |
| Ping | `iputils-ping` | `iputils` | Mismo comando |
| iproute | `iproute2` | `iproute` | `ip`, `ss` en ambos |
| tcpdump | `tcpdump` | `tcpdump` | Mismo paquete |
| Man pages C | `manpages-dev` | `man-pages` | Contenido similar |
| Man POSIX | `manpages-posix-dev` | No disponible* | Ver nota abajo |
| Vim | `vim-tiny` | `vim-minimal` | Preinstalado en AlmaLinux |
| Git | `git` | `git` | Mismo paquete |
| Curl | `curl` | `curl` | Mismo paquete |
| Wget | `wget` | `wget` | Mismo paquete |
| Locales | `locales` | `glibc-langpack-en` | Configuración diferente |

**Nota sobre manpages-posix:** En RHEL/AlmaLinux no existe el paquete
`manpages-posix-dev`. Las páginas POSIX están disponibles online o se pueden
instalar manualmente desde fuentes upstream.

### glibc vs musl (Alpine)

| Aspecto | glibc (Debian/AlmaLinux) | musl (Alpine) |
|---|---|---|
| Biblioteca C estándar | GNU libc | musl libc |
| Compatibilidad | Estándar POSIX completo | POSIX básico |
| Dynamic linker | `/lib/ld-linux.so` | `/lib/ld-musl-x86_64.so` |
| Nombres de funciones | GNU extensions | Limitado |
| Soportethreads | pthread más completo | pthread subset |

---

## EPEL: repositorio extra

EPEL (Extra Packages for Enterprise Linux) proporciona paquetes que no están en
los repos base de RHEL/AlmaLinux:

```dockerfile
RUN dnf install -y epel-release && dnf clean all
```

### Repositorios disponibles en AlmaLinux 9

| Repositorio | Habilitado por defecto | Contenido |
|---|---|---|
| `baseos` | Sí | Paquetes base del sistema |
| `appstream` | Sí | Application Streams (versiones alternativas) |
| `crb` | No | Code Ready Builder (herramientas de desarrollo) |
| `epel` | No | Extra Packages for Enterprise Linux |
| `epel-next` | No | EPEL para actualizaciones |

### Habilitar CRB (Code Ready Builder)

```bash
dnf config-manager --set-enabled crb
```

CRB incluye herramientas adicionales como `rustup` disponible via `dnf`:

```bash
# Instalar Rust via dnf (alternativa a rustup)
dnf install -y rust
```

Sin embargo, para este curso usamos `rustup` para obtener versiones más
actualizadas y control sobre el toolchain.

---

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

---

## Notas sobre el Dockerfile

### `dnf clean all` vs `rm -rf /var/lib/apt/lists/*`

En el ecosistema RHEL, la limpieza de caché se hace con `dnf clean all`:

```dockerfile
# Debian
RUN apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*

# AlmaLinux
RUN dnf install -y ... && dnf clean all
```

`dnf clean all` elimina:
- `/var/cache/dnf/` — paquetes descargados
- `/var/cache/dnf/*/repodata/` — metadatos de repos

### ¿Por qué una sola capa?

Igual que en Debian, combinar `dnf install` + `dnf clean all` en una sola
capa evita que la caché de dnf quede en capas intermedias:

```bash
# Ver tamaño de caché
docker run --rm almalinux:9 du -sh /var/cache/dnf
# ~50MB
```

### Groups de paquetes

dnf tiene el concepto de **grupos de paquetes** que agrupan herramientas
relacionadas:

```bash
# Listar grupos disponibles
dnf group list

# Instalar todo el grupo "Development Tools"
dnf group install -y "Development Tools"

# Ver qué incluye
dnf group info "Development Tools"
```

**Development Tools incluye:**
- gcc, gcc-c++, make, automake, autoconf
- binutils, bison, flex
- gdb, strace
- rpm-build, patch, rpmdevtools

No lo usamos en el Dockerfile porque instala ~150 paquetes (incluyendo
herramientas que no necesitamos), inflando la imagen.

### Versiones de GCC

AlmaLinux 9 incluye GCC 11 por defecto:

```bash
# Ver versión
gcc --version
# gcc (GCC) 11.4.1 20230605 (Red Hat 11.4.1-2)
```

Para versiones más recientes, se usan **Application Streams**:

```bash
# Ver streams disponibles para GCC
dnf module list gcc-toolset

# Output:
# AlmaLinux 9 - AppStream
# Name      Stream   Versions   Profile
# gcc-toolset   11      11      common [d]
# gcc-toolset   12      12      common [d]
# gcc-toolset   13      13      common [d]

# Instalar GCC 13
dnf install -y gcc-toolset-13

# Usar GCC 13
scl enable gcc-toolset-13 bash
gcc --version
# gcc (GCC) 13.2.1 20231109
```

Para este curso, GCC 11 (el default) es suficiente.

### Diferencias en man pages

AlmaLinux no incluye `manpages-posix-dev`. Las páginas disponibles:

| Paquete | Contenido |
|---|---|
| `man-pages` | Man pages de funciones C de Linux y syscalls |
| `man-pages-overrides` | Actualizaciones de Red Hat a man-pages |
| `man-db` | Demonio y comando `man` |

Las páginas POSIX están disponibles en:
- https://pubs.opengroup.org/onlinepubs/9699919799/
- https://man7.org/linux/man-pages/

---

## Paridad funcional con Debian

Ambas imágenes deben poder ejecutar las mismas operaciones:

| Operación | Debian | AlmaLinux | Comando |
|---|---|---|---|
| Compilar C | ✅ | ✅ | `gcc -o prog prog.c` |
| Compilar C++ | ✅ | ✅ | `g++ -o prog prog.cpp` |
| Compilar Rust | ✅ | ✅ | `cargo build` |
| Debug con gdb | ✅ | ✅ | `gdb ./prog` |
| Memory check | ✅ | ✅ | `valgrind ./prog` |
| Trazar syscalls | ✅ | ✅ | `strace ./prog` |
| Trazar libs | ✅ | ✅ | `ltrace ./prog` |
| Man pages | ✅ | ✅ | `man printf` |
| Ping | ✅ | ✅ | `ping -c 1 host` |
| DNS lookup | ✅ | ✅ | `dig host` |
| Socket stats | ✅ | ✅ | `ss -tulpn` |

Las herramientas producen resultados idénticos. Las diferencias están en:
- Paths de configuración del sistema
- Versión de glibc (2.36 en Debian 12 vs 2.34 en AlmaLinux 9)
- Configuración por defecto (locales, SELinux, firewalld)

---

## Construir la imagen

```bash
# Construir
docker build -t alma-dev .

# Construir sin caché
docker build --no-cache -t alma-dev .

# Ver proceso detallado
docker build --progress=plain -t alma-dev .

# Verificar el tamaño
docker image ls alma-dev
# REPOSITORY   TAG      SIZE
# alma-dev     latest   ~750MB
```

---

## Comparación de tamaños

```bash
docker image ls --format "{{.Repository}}\t{{.Size}}" | grep dev
# debian-dev   ~700MB
# alma-dev     ~750MB
```

### Desglose aproximado

| Componente | Debian | AlmaLinux |
|---|---|---|
| Imagen base | ~125MB | ~107MB |
| GCC + herramientas C | ~200MB | ~250MB |
| Rust (rustup + stable) | ~300MB | ~300MB |
| Herramientas de red | ~50MB | ~50MB |
| Documentación (man) | ~25MB | ~15MB |
| Misceláneos | ~20MB | ~30MB |
| **Total** | **~720MB** | **~750MB** |

La diferencia principal está en las dependencias de GCC (GCC en AlmaLinux
requiere más dependencias transitive debido al sistema de módulos de RHEL).

---

## Ejercicios

### Ejercicio 1 — Construir y verificar

```bash
mkdir -p /tmp/lab/dockerfiles/alma

# Crear el Dockerfile (usar contenido de arriba)
cat > /tmp/lab/dockerfiles/alma/Dockerfile << 'EOF'
# (contenido del Dockerfile)
EOF

# Construir
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
cat /etc/os-release | head -4
rpm -q glibc
'
```

**Criterios de éxito:**
- `gcc --version` muestra GCC 11.x
- `rustc --version` muestra Rust 1.70+
- `gdb --version` funciona sin errores

---

### Ejercicio 2 — Compilar el mismo programa en ambas distros

```bash
# Programa de prueba
PROG='
#include <stdio.h>
#include <sys/utsname.h>

int main(void) {
    struct utsname u;
    uname(&u);
    printf("Hello from %s (%s)\n", u.sysname, u.version);
    return 0;
}'

# Compilar en Debian
echo "$PROG" | docker run --rm -i debian-dev bash -c '
cat > /tmp/test.c
gcc -o /tmp/test /tmp/test.c
/tmp/test
'

# Compilar en AlmaLinux
echo "$PROG" | docker run --rm -i alma-dev bash -c '
cat > /tmp/test.c
gcc -o /tmp/test /tmp/test.c
/tmp/test
'
```

Ambas salidas mostrarán el mismo resultado porque comparten el mismo kernel.

---

### Ejercicio 3 — Comparar glibc entre distribuciones

```bash
# Ver versión de glibc
docker run --rm debian-dev ldd --version | head -1
docker run --rm alma-dev ldd --version | head -1

# Compilar programa que usa функции glibc
docker run --rm debian-dev bash -c '
cat > /tmp/glibc_test.c << EOF
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

int main(void) {
    char *s = "hello world";
    char *pos = strchrnul(s, "x");
    printf("strchrnul: %s\n", pos);
    return 0;
}
EOF
gcc -o /tmp/test /tmp/test.c && /tmp/test
'

# Probar en AlmaLinux
docker run --rm alma-dev bash -c '
cat > /tmp/glibc_test.c << EOF
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

int main(void) {
    char *s = "hello world";
    char *pos = strchrnul(s, "x");
    printf("strchrnul: %s\n", pos);
    return 0;
}
EOF
gcc -o /tmp/test /tmp/test.c && /tmp/test
'
```

---

### Ejercicio 4 — Comparar rpm vs dpkg

```bash
# Listar paquetes instalados (Debian)
docker run --rm debian-dev bash -c '
dpkg -l | grep "^ii" | wc -l
dpkg -l | grep -E "gcc|g\+\+" | head -5
'

# Listar paquetes instalados (AlmaLinux)
docker run --rm alma-dev bash -c '
rpm -qa | wc -l
rpm -qa | grep -E "gcc|gcc-c" | head -5
'
```

---

### Ejercicio 5 — Usar strace en ambas imágenes

```bash
# strace en Debian
docker run --rm --cap-add SYS_PTRACE debian-dev strace -c ls /tmp 2>&1

# strace en AlmaLinux
docker run --rm --cap-add SYS_PTRACE alma-dev strace -c ls /tmp 2>&1

# Comparar syscalls utilizados (deberían ser muy similares)
```

---

### Ejercicio 6 — Instalar un paquete de EPEL

```bash
docker run --rm alma-dev bash -c '
# Buscar htop en repos
dnf search htop

# Instalar desde EPEL
dnf install -y epel-release
dnf install -y htop

# Verificar
htop --version
'
```

---

### Ejercicio 7 — Comparar man pages entre distribuciones

```bash
# Buscar man pages en Debian
docker run --rm debian-dev bash -c '
man -k "^printf" | head -5
man -k socket | wc -l
'

# Buscar man pages en AlmaLinux
docker run --rm alma-dev bash -c '
man -k "^printf" | head -5
man -k socket | wc -l
'
```

---

### Ejercicio 8 — Probar containerd runtime con strace

Este ejercicio prueba que las capacidades funcionan igual en ambas imágenes:

```bash
# Debian
docker run --rm --cap-add SYS_PTRACE debian-dev bash -c '
cat > /tmp/perf.c << EOF
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("PID: %d\n", getpid());
    while(1) { sleep(1); }
    return 0;
}
EOF

gcc -o /tmp/perf /tmp/perf.c &
PID=$!
sleep 1
strace -p $PID 2>&1 | head -10
kill $PID 2>/dev/null
'

# Hacer lo mismo en AlmaLinux
docker run --rm --cap-add SYS_PTRACE alma-dev bash -c '
# (mismo script)
'
```

---

### Ejercicio 9 — Documentar diferencias de paquetes

```bash
echo "=== Debian ==="
docker run --rm debian-dev bash -c '
echo "Total paquetes instalados:"
dpkg -l | grep "^ii" | wc -l
echo ""
echo "Versiones clave:"
gcc --version | head -1
g++ --version | head -1
ldd --version | head -1
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm alma-dev bash -c '
echo "Total paquetes instalados:"
rpm -qa | wc -l
echo ""
echo "Versiones clave:"
gcc --version | head -1
g++ --version | head -1
ldd --version | head -1
'
```

---

### Ejercicio 10 — Build multiplatform (opcional avanzado)

Explorar cómo crear una imagen que funcione en ambas arquitecturas:

```bash
# Inspecionar arquitectura
docker run --rm debian-dev uname -m
docker run --rm alma-dev uname -m

# Ambas deberían mostrar x86_64 en hardware Intel/AMD
```
