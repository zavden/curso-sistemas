# T02 — Dockerfile Alma dev

## Objetivo

Construir una imagen de desarrollo basada en **AlmaLinux 9** (compatible RHEL 9,
soporte hasta 2032) con las mismas herramientas que la imagen Debian, para poder
compilar y probar código en ambas distribuciones.

El Dockerfile completo está en `labs/Dockerfile.alma-dev`.

---

## ¿Por qué AlmaLinux?

AlmaLinux es un fork de RHEL mantenido por la comunidad. Es binariamente compatible
con RHEL 9:

| Característica | Descripción |
|---|---|
| Compatibilidad binaria | Los mismos binarios que RHEL 9 funcionan |
| Repositorios abiertos | Sin suscripción de Red Hat necesaria |
| Ciclo de soporte | Hasta 2032 |
| Certificaciones | Compatible con RHCSA/RHCE |

### Alternativas compatibles con RHEL

| Distribución | Diferencia principal | Sponsor |
|---|---|---|
| AlmaLinux | Fork directo de RHEL, gobernanza comunitaria | CloudLinux Inc. |
| Rocky Linux | Fork directo de RHEL, gobernanza comunitaria | Rocky Enterprise Software Foundation |
| CentOS Stream | Rolling release basada en RHEL (upstream) | Red Hat (IBM) |

---

## Imagen base

```dockerfile
FROM almalinux:9
```

| Imagen | Tamaño base |
|---|---|
| `almalinux:9` | ~107MB |
| `almalinux:9-minimal` | ~52MB |

Se usa la imagen completa porque `minimal` usa `microdnf` en lugar de `dnf` y
carece de herramientas base necesarias.

---

## Diferencias de paquetes: Debian vs AlmaLinux

### Gestión de paquetes

| Aspecto | Debian (apt/dpkg) | AlmaLinux (dnf/rpm) |
|---|---|---|
| Gestor de paquetes | `apt-get`, `apt-cache` | `dnf` (`yum` es alias) |
| Instalador individual | `dpkg -i paquete.deb` | `rpm -i paquete.rpm` |
| Buscar paquete | `apt-cache search name` | `dnf search name` |
| Info de paquete | `apt-cache show pkg` | `dnf info pkg` |
| Archivos de repos | `/etc/apt/sources.list.d/` | `/etc/yum.repos.d/*.repo` |
| Limpieza de caché | `rm -rf /var/lib/apt/lists/*` | `dnf clean all` |

### Equivalencias de paquetes

| Propósito | Debian (apt) | AlmaLinux (dnf) |
|---|---|---|
| Compilador C++ | `g++` | `gcc-c++` |
| Headers glibc | `libc6-dev` | `glibc-devel` |
| pkg-config | `pkg-config` | `pkgconf-pkg-config` |
| Netcat | `netcat-openbsd` | `nmap-ncat` |
| DNS tools | `dnsutils` | `bind-utils` |
| Ping | `iputils-ping` | `iputils` |
| iproute | `iproute2` | `iproute` |
| Man pages C | `manpages-dev` | `man-pages` |
| Man POSIX | `manpages-posix-dev` | No disponible* |
| Vim | `vim-tiny` | `vim-minimal` (preinstalado) |
| Locales | `locales` | `glibc-langpack-en` |

\* En RHEL/AlmaLinux no existe `manpages-posix-dev`. Las man pages estándar
(`man-pages`) cubren funciones C y syscalls. Para referencia POSIX estricta,
consultar https://pubs.opengroup.org/onlinepubs/9699919799/.

---

## EPEL: repositorio extra

EPEL (Extra Packages for Enterprise Linux) proporciona paquetes adicionales
que no están en los repos base de RHEL/AlmaLinux:

```dockerfile
RUN dnf install -y epel-release && dnf clean all
```

### Repositorios disponibles en AlmaLinux 9

| Repositorio | Habilitado por defecto | Contenido |
|---|---|---|
| `baseos` | Sí | Paquetes base del sistema |
| `appstream` | Sí | Application Streams (versiones alternativas) |
| `crb` | No | Code Ready Builder (headers de desarrollo) |
| `epel` | No (requiere `epel-release`) | Extra Packages for Enterprise Linux |

### Habilitar CRB (Code Ready Builder)

```bash
dnf config-manager --set-enabled crb
```

CRB incluye headers de desarrollo y librerías adicionales necesarias para
compilar cierto software. No incluye compiladores ni toolchains — esos están
en AppStream.

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

```dockerfile
# Debian
RUN apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*

# AlmaLinux
RUN dnf install -y ... && dnf clean all
```

`dnf clean all` elimina:
- `/var/cache/dnf/` — paquetes descargados y metadatos de repos

### ¿Por qué EPEL en un RUN separado?

```dockerfile
RUN dnf install -y epel-release && dnf clean all
RUN dnf install -y gcc ... && dnf clean all
```

`epel-release` configura el repositorio EPEL. Debe instalarse antes de los
paquetes que lo necesitan porque `dnf` necesita los metadatos del repo EPEL
disponibles al resolver dependencias. Separarlo en dos capas aprovecha el
caché de Docker — si cambias la lista de paquetes, no necesitas reinstalar EPEL.

### Groups de paquetes

dnf tiene **grupos de paquetes** que agrupan herramientas relacionadas:

```bash
# Ver qué incluye "Development Tools"
dnf group info "Development Tools"
# gcc, gcc-c++, make, automake, autoconf, binutils, gdb, strace, etc.
```

No lo usamos en el Dockerfile porque instala ~150 paquetes (incluyendo
herramientas que no necesitamos), inflando la imagen.

### Versiones de GCC

AlmaLinux 9 incluye GCC 11 por defecto. Para versiones más recientes, se usan
**gcc-toolset** (Software Collections):

```bash
# Ver toolsets disponibles
dnf list gcc-toolset-*

# Instalar GCC 13
dnf install -y gcc-toolset-13

# Activar en la sesión actual
scl enable gcc-toolset-13 bash
gcc --version
# gcc (GCC) 13.x.x
```

Para este curso, GCC 11 (el default) es suficiente.

### Rust: rustup vs paquete del sistema

AlmaLinux tiene un paquete `rust` en AppStream (`dnf install -y rust`), pero
usamos rustup por:
- Versiones más actualizadas (AppStream puede tener versiones atrasadas)
- Control sobre componentes (rustfmt, clippy)
- Posibilidad de instalar nightly o versiones específicas

---

## Paridad funcional con Debian

Ambas imágenes deben poder ejecutar las mismas operaciones:

| Operación | Debian | AlmaLinux | Comando |
|---|---|---|---|
| Compilar C | gcc 12.x | gcc 11.x | `gcc -o prog prog.c` |
| Compilar C++ | g++ 12.x | g++ 11.x | `g++ -o prog prog.cpp` |
| Compilar Rust | rustc (rustup) | rustc (rustup) | `cargo build` |
| Debug | gdb | gdb | `gdb ./prog` |
| Memory check | valgrind | valgrind | `valgrind ./prog` |
| Trazar syscalls | strace | strace | `strace ./prog` |
| Man pages | man-pages + POSIX | man-pages | `man printf` |
| Red | ping, dig, ss | ping, dig, ss | Idéntico |

Las diferencias están en:
- Versión de GCC (12.x en Debian 12 vs 11.x en AlmaLinux 9)
- Versión de glibc (2.36 en Debian 12 vs 2.34 en AlmaLinux 9)
- Paths de configuración del sistema
- Man pages POSIX solo disponibles en Debian

---

## Ejercicios

### Ejercicio 1 — Construir y verificar

```bash
# Construir desde el lab
docker build -t alma-dev -f labs/Dockerfile.alma-dev labs/

# Verificar tamaño
docker image ls alma-dev
```

**Predicción**: ¿Qué versión de GCC esperas ver?

```bash
docker run --rm alma-dev bash -c '
echo "=== Compiladores ==="
gcc --version | head -1
g++ --version | head -1
rustc --version
cargo --version

echo ""
echo "=== Sistema ==="
cat /etc/os-release | head -2
rpm -q glibc
'
```

<details><summary>Respuesta</summary>

GCC 11.x (GCC 11.4.1 o similar). AlmaLinux 9 / RHEL 9 incluye GCC 11 como
versión base. Debian 12 incluye GCC 12.x. Ambas versiones soportan C17/C18 y
C++17 completamente.

glibc será 2.34 (vs 2.36 en Debian 12).

</details>

### Ejercicio 2 — Compilar el mismo programa en ambas distros

```bash
PROG='
#include <stdio.h>
#include <sys/utsname.h>

int main(void) {
    struct utsname u;
    uname(&u);
    printf("Hello from %s (%s)\n", u.sysname, u.release);
    return 0;
}
'

# Compilar en Debian
echo "$PROG" | docker run --rm -i debian-dev bash -c '
cat > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'

# Compilar en AlmaLinux
echo "$PROG" | docker run --rm -i alma-dev bash -c '
cat > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'
```

**Predicción**: ¿El output es idéntico o diferente?

<details><summary>Respuesta</summary>

**Idéntico**. Ambos contenedores comparten el mismo kernel del host. `uname()`
devuelve la información del kernel, no de la distribución. La función
`u.release` muestra la versión del kernel del host (ej. `6.19.8-200.fc43`),
que es la misma en ambos contenedores.

La distribución del contenedor (Debian/AlmaLinux) solo afecta al userspace
(librerías, herramientas), no al kernel.

</details>

### Ejercicio 3 — Comparar glibc entre distribuciones

```bash
docker run --rm debian-dev ldd --version | head -1
docker run --rm alma-dev ldd --version | head -1
```

**Predicción**: ¿Son la misma versión?

<details><summary>Respuesta</summary>

No. Debian 12 usa glibc 2.36 y AlmaLinux 9 usa glibc 2.34. La versión de glibc
afecta qué funciones están disponibles y su comportamiento. En la práctica, para
el código del curso la diferencia es imperceptible — ambas versiones soportan el
estándar POSIX completo y las GNU extensions más comunes.

</details>

```bash
# Probar una GNU extension (strchrnul) en ambas
docker run --rm debian-dev bash -c '
cat > /tmp/gnu_test.c << "EOF"
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

int main(void) {
    const char *s = "hello world";
    const char *pos = strchrnul(s, '"'"'x'"'"');
    printf("strchrnul para x: apunta a offset %ld (caracter nulo)\n", pos - s);
    return 0;
}
EOF
gcc -o /tmp/gnu_test /tmp/gnu_test.c && /tmp/gnu_test
'

docker run --rm alma-dev bash -c '
cat > /tmp/gnu_test.c << "EOF"
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

int main(void) {
    const char *s = "hello world";
    const char *pos = strchrnul(s, '"'"'x'"'"');
    printf("strchrnul para x: apunta a offset %ld (caracter nulo)\n", pos - s);
    return 0;
}
EOF
gcc -o /tmp/gnu_test /tmp/gnu_test.c && /tmp/gnu_test
'
```

### Ejercicio 4 — Comparar rpm vs dpkg

```bash
echo "=== Debian ==="
docker run --rm debian-dev bash -c '
echo "Total paquetes: $(dpkg -l | grep "^ii" | wc -l)"
dpkg -l | grep -E "gcc|g\+\+" | awk "{print \$2}" | head -5
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm alma-dev bash -c '
echo "Total paquetes: $(rpm -qa | wc -l)"
rpm -qa | grep -E "^gcc" | head -5
'
```

**Predicción**: ¿Qué sistema tiene más paquetes instalados?

<details><summary>Respuesta</summary>

Depende de cómo cada gestor de paquetes divide el software. AlmaLinux tiende
a tener más paquetes porque RPM divide el software en subpaquetes más granulares
(ej. `gcc`, `gcc-c++`, `libgcc`, `gcc-plugin-devel`, etc.), mientras que Debian
agrupa más funcionalidad por paquete.

Pero el software instalado es funcionalmente equivalente — la diferencia es
de empaquetado, no de capacidad.

</details>

### Ejercicio 5 — strace en ambas imágenes

```bash
# strace en Debian
docker run --rm --cap-add SYS_PTRACE debian-dev strace -c echo "hello" 2>&1 | tail -5

# strace en AlmaLinux
docker run --rm --cap-add SYS_PTRACE alma-dev strace -c echo "hello" 2>&1 | tail -5
```

**Predicción**: ¿Las syscalls son las mismas en ambas distros?

<details><summary>Respuesta</summary>

Muy similares. Las syscalls son llamadas al **kernel**, que es el mismo en ambos
contenedores (comparten el kernel del host). Las pequeñas diferencias se deben a
la versión de glibc — diferentes versiones pueden usar syscalls ligeramente
diferentes para la misma operación (ej. `stat` vs `fstat` vs `newfstatat`).

</details>

### Ejercicio 6 — EPEL: instalar un paquete extra

```bash
docker run --rm alma-dev bash -c '
# EPEL ya está instalado en la imagen
dnf repolist | grep epel

# Buscar htop
dnf search htop

# Instalar desde EPEL
sudo dnf install -y htop 2>/dev/null || dnf install -y htop
htop --version
'
```

**Predicción**: ¿De qué repositorio viene `htop`?

<details><summary>Respuesta</summary>

De **EPEL** (Extra Packages for Enterprise Linux). `htop` no está en los repos
base de AlmaLinux (baseos/appstream). EPEL lo proporciona porque es un paquete
popular que no forma parte del core de RHEL.

Nota: el `dnf install` falla como usuario `dev` porque no tiene permisos de root.
Habría que ejecutar el contenedor como root o usar una imagen con sudo configurado.

</details>

### Ejercicio 7 — Comparar man pages entre distribuciones

```bash
echo "=== Debian ==="
docker run --rm debian-dev bash -c '
man -k "^printf" 2>/dev/null | head -5
echo "---"
echo "Total entries socket: $(man -k socket 2>/dev/null | wc -l)"
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm alma-dev bash -c '
man -k "^printf" 2>/dev/null | head -5
echo "---"
echo "Total entries socket: $(man -k socket 2>/dev/null | wc -l)"
'
```

**Predicción**: ¿Qué distribución tiene más man pages?

<details><summary>Respuesta</summary>

**Debian**, porque incluye `manpages-posix-dev` que añade las páginas POSIX.
AlmaLinux solo tiene `man-pages` (man pages de Linux) sin las POSIX. Para la
mayoría del desarrollo del curso la diferencia es menor — `man 3 printf` y
`man 2 socket` existen en ambas.

</details>

### Ejercicio 8 — Comparar tamaños de imagen

```bash
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep -E "dev|REPO"
```

**Predicción**: ¿Cuál es más grande — debian-dev o alma-dev?

<details><summary>Respuesta</summary>

Tamaños similares (~700-750MB ambas). AlmaLinux suele ser ligeramente más grande
porque las dependencias de GCC incluyen más paquetes transitivos por el sistema
de módulos de RHEL. Rust (instalado via rustup) ocupa lo mismo en ambas (~300MB)
porque se descarga del mismo lugar, independiente de la distro.

La diferencia real es la imagen base: Debian ~125MB vs AlmaLinux ~107MB, pero
las dependencias de paquetes compensan esta diferencia.

</details>

---

## Resumen: Debian vs AlmaLinux en el curso

```
┌──────────────────────────────────────────────────────────────┐
│              DEBIAN-DEV vs ALMA-DEV                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DEBIAN (bookworm)              ALMALINUX (9)                │
│  ────────────────               ─────────────                │
│  apt-get / dpkg                 dnf / rpm                    │
│  gcc 12.x                      gcc 11.x                     │
│  glibc 2.36                    glibc 2.34                    │
│  man-pages + POSIX             man-pages (sin POSIX)         │
│  g++                           gcc-c++                       │
│  netcat-openbsd                nmap-ncat                     │
│  dnsutils                      bind-utils                    │
│                                                              │
│  IDENTICO EN AMBAS:                                          │
│  • Rust via rustup (misma versión)                           │
│  • gcc/make/cmake (misma funcionalidad)                      │
│  • gdb/valgrind/strace (mismo uso)                           │
│  • Kernel compartido (del host)                              │
│  • Formato binario ELF + glibc                               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```
