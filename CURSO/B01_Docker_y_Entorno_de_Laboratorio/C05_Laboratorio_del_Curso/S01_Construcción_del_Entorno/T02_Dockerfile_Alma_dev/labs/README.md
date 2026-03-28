# Lab — Dockerfile Alma dev

## Objetivo

Construir la imagen de desarrollo AlmaLinux 9 del curso, entender las
diferencias de paquetes respecto a Debian, y verificar que ambas
distribuciones ofrecen paridad funcional para el desarrollo C y Rust.

## Prerequisitos

- Docker instalado y funcionando
- Conexion a internet
- Imagen `lab-debian-dev` construida (del lab T01) para la parte 4
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.alma-dev` | Dockerfile completo de la imagen AlmaLinux dev |

---

## Parte 1 — Diferencias con Debian

### Objetivo

Comparar el Dockerfile de AlmaLinux con el de Debian, identificando
las diferencias en nombres de paquetes, gestor de paquetes y EPEL.

### Paso 1.1: Examinar el Dockerfile

```bash
cat Dockerfile.alma-dev
```

### Paso 1.2: Diferencias clave

Compara mentalmente con el Dockerfile de Debian (T01). Las diferencias
principales son:

```
Debian (apt)                    AlmaLinux (dnf)
─────────────────────────────────────────────────
apt-get update && install       dnf install
g++                             gcc-c++
libc6-dev                       glibc-devel
pkg-config                      pkgconf-pkg-config
netcat-openbsd                  nmap-ncat
dnsutils                        bind-utils
iputils-ping                    iputils
iproute2                        iproute
vim-tiny                        vim-minimal
locales                         glibc-langpack-en
manpages-posix-dev              (no disponible)
rm -rf /var/lib/apt/lists/*     dnf clean all
```

### Paso 1.3: EPEL

```bash
grep -A 1 "EPEL" Dockerfile.alma-dev
```

EPEL (Extra Packages for Enterprise Linux) agrega paquetes que no estan
en los repos base de RHEL/AlmaLinux. Se instala en un `RUN` separado
porque `epel-release` debe estar disponible antes de instalar paquetes
que dependen de EPEL (como `ltrace`).

### Paso 1.4: Limpieza

```bash
grep "clean" Dockerfile.alma-dev
```

En el ecosistema RHEL, la limpieza de cache se hace con `dnf clean all`
(equivalente a `rm -rf /var/lib/apt/lists/*` en Debian). Elimina
metadatos y paquetes descargados de `/var/cache/dnf/`.

---

## Parte 2 — Construir la imagen

### Objetivo

Construir la imagen AlmaLinux dev y observar las diferencias en el
proceso de build respecto a Debian.

### Paso 2.1: Construir

```bash
docker build -t lab-alma-dev -f Dockerfile.alma-dev .
```

Antes de ejecutar, predice:

- Tardara mas o menos que la imagen Debian?
- Sera mas grande o mas pequena?

Observa que `dnf install` muestra informacion diferente a `apt-get`
(resolucion de dependencias, verificacion de transaccion).

### Paso 2.2: Verificar tamano

```bash
docker image ls lab-alma-dev
```

Salida esperada:

```
REPOSITORY      TAG       IMAGE ID       CREATED          SIZE
lab-alma-dev    latest    <hash>         <time>           ~700-800MB
```

### Paso 2.3: Comparar tamanos base

```bash
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep -E "(debian|alma)"
```

Las imagenes base son diferentes:

```
debian          bookworm    ~120MB
almalinux       9           ~190MB
```

AlmaLinux base es mas grande porque incluye mas utilidades del sistema
por defecto.

---

## Parte 3 — Verificar herramientas

### Objetivo

Comprobar que todas las herramientas del curso estan instaladas.

### Paso 3.1: Compiladores

```bash
docker run --rm lab-alma-dev bash -c '
echo "=== gcc ==="
gcc --version | head -1
echo ""
echo "=== g++ ==="
g++ --version | head -1
'
```

AlmaLinux 9 incluye GCC 11 por defecto (Debian bookworm incluye GCC 12).

### Paso 3.2: Debugging

```bash
docker run --rm lab-alma-dev bash -c '
echo "=== gdb ==="
gdb --version | head -1
echo ""
echo "=== valgrind ==="
valgrind --version
echo ""
echo "=== strace ==="
strace --version 2>&1 | head -1
'
```

### Paso 3.3: Rust

```bash
docker run --rm lab-alma-dev bash -c '
echo "=== rustc ==="
rustc --version
echo ""
echo "=== cargo ==="
cargo --version
echo ""
echo "=== rustfmt ==="
rustfmt --version
'
```

La version de Rust es identica en ambas imagenes porque rustup instala
la misma version independientemente de la distribucion.

### Paso 3.4: Herramientas de red

```bash
docker run --rm lab-alma-dev bash -c '
echo "=== ip ==="
ip -V 2>&1
echo ""
echo "=== ss ==="
ss --version 2>&1 | head -1
echo ""
echo "=== ncat ==="
ncat --version 2>&1 | head -1
echo ""
echo "=== dig ==="
dig -v 2>&1 | head -1
'
```

Nota: AlmaLinux usa `ncat` (de nmap) en lugar de `nc` (netcat-openbsd).
La funcionalidad es similar pero los flags pueden diferir.

### Paso 3.5: Sistema

```bash
docker run --rm lab-alma-dev bash -c '
echo "=== OS ==="
cat /etc/os-release | head -2
echo ""
echo "=== glibc ==="
rpm -q glibc
echo ""
echo "=== Paquetes instalados ==="
rpm -qa | wc -l
'
```

---

## Parte 4 — Paridad funcional

### Objetivo

Demostrar que el mismo codigo C y Rust compila y ejecuta
identicamente en ambas distribuciones.

### Paso 4.1: Compilar C en ambas distros

Si no tienes la imagen de Debian del lab anterior, construyela primero
desde el directorio del T01.

```bash
echo "=== Debian ==="
docker run --rm lab-debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
#include <sys/utsname.h>
int main(void) {
    struct utsname u;
    uname(&u);
    printf("OS: %s\nKernel: %s\n", u.sysname, u.release);
    return 0;
}
EOF
gcc -o /tmp/test /tmp/test.c && /tmp/test
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm lab-alma-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
#include <sys/utsname.h>
int main(void) {
    struct utsname u;
    uname(&u);
    printf("OS: %s\nKernel: %s\n", u.sysname, u.release);
    return 0;
}
EOF
gcc -o /tmp/test /tmp/test.c && /tmp/test
'
```

Ambos muestran el mismo kernel (compartido con el host). La diferencia
esta en las versiones de glibc y las librerias del sistema.

### Paso 4.2: Compilar Rust en ambas distros

```bash
echo "=== Debian ==="
docker run --rm lab-debian-dev bash -c '
cat > /tmp/test.rs << EOF
fn main() {
    println!("Rust from {}", std::env::consts::OS);
}
EOF
rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs
'

echo ""
echo "=== AlmaLinux ==="
docker run --rm lab-alma-dev bash -c '
cat > /tmp/test.rs << EOF
fn main() {
    println!("Rust from {}", std::env::consts::OS);
}
EOF
rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs
'
```

Rust produce exactamente la misma salida en ambas distribuciones.

### Paso 4.3: Comparar versiones de glibc

```bash
echo "=== Debian ==="
docker run --rm lab-debian-dev ldd --version 2>&1 | head -1

echo ""
echo "=== AlmaLinux ==="
docker run --rm lab-alma-dev ldd --version 2>&1 | head -1
```

Las versiones de glibc difieren. Un binario compilado en una
distribucion **no es necesariamente compatible** con la otra si usa
librerias dinamicas con versiones diferentes.

### Paso 4.4: Verificar diferencias de man pages

```bash
echo "=== Debian ==="
docker run --rm lab-debian-dev man -k printf 2>&1 | head -3

echo ""
echo "=== AlmaLinux ==="
docker run --rm lab-alma-dev man -k printf 2>&1 | head -3
```

Debian tiene `manpages-posix-dev` con man pages del estandar POSIX.
AlmaLinux tiene `man-pages` que cubre funciones C y syscalls, pero
sin la referencia POSIX estricta.

---

## Limpieza final

```bash
docker rmi lab-alma-dev
docker rmi lab-debian-dev 2>/dev/null
```

---

## Conceptos reforzados

1. Los **nombres de paquetes difieren** entre Debian y AlmaLinux
   (`g++` vs `gcc-c++`, `netcat-openbsd` vs `nmap-ncat`), pero las
   herramientas instaladas son funcionalmente equivalentes.

2. AlmaLinux necesita **EPEL** para algunos paquetes que en Debian
   estan en los repos principales.

3. `dnf clean all` es el equivalente de `rm -rf /var/lib/apt/lists/*`
   para limpiar cache del gestor de paquetes en la imagen.

4. **Rust es identico** en ambas distribuciones porque rustup instala
   la misma version independientemente del sistema base.

5. El mismo codigo C compila en ambas distros, pero los **binarios no
   son intercambiables** debido a diferencias en glibc y librerias
   dinamicas.
