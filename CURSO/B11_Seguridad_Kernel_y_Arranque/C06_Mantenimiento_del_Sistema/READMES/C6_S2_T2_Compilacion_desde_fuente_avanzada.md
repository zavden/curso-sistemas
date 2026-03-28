# Compilación desde Fuente Avanzada — configure, make check/test y DESTDIR

## Índice

1. [¿Por qué compilar desde fuente?](#1-por-qué-compilar-desde-fuente)
2. [El flujo estándar: configure → make → make install](#2-el-flujo-estándar-configure--make--make-install)
3. [Autotools en profundidad: configure](#3-autotools-en-profundidad-configure)
4. [Opciones de configure](#4-opciones-de-configure)
5. [make: compilación y targets](#5-make-compilación-y-targets)
6. [make check / make test: verificar la compilación](#6-make-check--make-test-verificar-la-compilación)
7. [make install y DESTDIR](#7-make-install-y-destdir)
8. [Otros sistemas de build: CMake y Meson](#8-otros-sistemas-de-build-cmake-y-meson)
9. [Gestionar software compilado manualmente](#9-gestionar-software-compilado-manualmente)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Por qué compilar desde fuente?

El gestor de paquetes de la distribución (`dnf`, `apt`) es siempre la primera opción. Pero hay situaciones legítimas donde compilar desde fuente es necesario:

```
Cuándo compilar desde fuente:
  ├── Necesitas una versión más nueva que la del repo
  ├── Necesitas flags de compilación específicos (--enable-feature)
  ├── El software no está empaquetado para tu distribución
  ├── Necesitas parches personalizados
  ├── Optimización para tu hardware específico (CPU flags)
  └── Entorno de desarrollo/testing

Cuándo NO hacerlo:
  ├── La versión del repo es suficiente
  ├── Actualizaciones de seguridad (el repo las parcheará automáticamente)
  ├── Servidores de producción (difícil de mantener/actualizar)
  └── Cuando existe un COPR/PPA con la versión que necesitas
```

### El problema de compilar manualmente

```
Software del sistema de paquetes:
  dnf install nginx
  ├── Instalación rastreada (rpm -ql nginx)
  ├── Desinstalación limpia (dnf remove nginx)
  ├── Actualizaciones automáticas (dnf update)
  └── Dependencias resueltas automáticamente

Software compilado manualmente:
  make install
  ├── Archivos dispersos en /usr/local/ (sin rastreo)
  ├── Desinstalar = ¿qué archivos borrar?
  ├── Actualizar = compilar de nuevo + rezar
  └── Conflictos con paquetes del sistema
```

---

## 2. El flujo estándar: configure → make → make install

La inmensa mayoría del software C/C++ en Unix/Linux sigue un flujo de tres pasos conocido como el "Autotools dance":

```
Código fuente                                    Software instalado
     │                                                  ▲
     ▼                                                  │
┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌───────────┐
│ configure │───▶│   make   │───▶│  make check  │───▶│make install│
│           │    │          │    │  (opcional)   │    │           │
│ Detectar  │    │ Compilar │    │  Ejecutar     │    │ Copiar a  │
│ sistema,  │    │ código   │    │  tests        │    │ destino   │
│ generar   │    │ fuente   │    │              │    │ final     │
│ Makefile  │    │          │    │              │    │           │
└──────────┘    └──────────┘    └──────────────┘    └───────────┘
     │                │                                   │
     ▼                ▼                                   ▼
  Makefile         *.o, binario                  /usr/local/bin/
  config.h         ejecutable                    /usr/local/lib/
  config.log                                     /usr/local/share/
```

### Pasos previos: obtener y preparar el código

```bash
# 1. Descargar tarball
wget https://example.org/software-2.0.tar.gz

# 2. Verificar integridad (si hay checksum disponible)
sha256sum software-2.0.tar.gz
# Comparar con el hash publicado

# 3. Verificar firma GPG (si hay .asc/.sig)
gpg --verify software-2.0.tar.gz.asc software-2.0.tar.gz

# 4. Extraer
tar xzf software-2.0.tar.gz
cd software-2.0/

# 5. Leer la documentación
cat README          # O README.md
cat INSTALL         # Instrucciones de compilación
cat DEPENDENCIES    # Dependencias necesarias (si existe)
```

### Dependencias de compilación

```bash
# Instalar herramientas de compilación base
# RHEL/Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install gcc gcc-c++ make autoconf automake libtool

# Debian/Ubuntu
sudo apt install build-essential autoconf automake libtool

# Las dependencias específicas del proyecto se indican en INSTALL/README
# Ejemplo: si necesita libssl
sudo dnf install openssl-devel    # Fedora/RHEL
sudo apt install libssl-dev       # Debian/Ubuntu
```

> **Pregunta de predicción**: ¿Por qué en Fedora se instala `openssl-devel` pero en Debian se llama `libssl-dev`? ¿Qué contienen estos paquetes que no está en `openssl`?

Los paquetes `-devel` (RPM) / `-dev` (DEB) contienen los **headers** (.h) y **libraries estáticas** (.a) / symlinks a las **libraries compartidas** (.so) necesarios para **compilar** software que usa esa librería. El paquete `openssl` solo contiene el binario y las librerías compartidas para **ejecutar** — sin headers no puedes compilar contra él.

---

## 3. Autotools en profundidad: configure

### ¿Qué es el script configure?

`configure` es un script de shell generado por **GNU Autotools** (autoconf + automake). Su trabajo es:

```
1. DETECTAR el sistema:
   ├── ¿Qué compilador hay? (gcc, clang, cc)
   ├── ¿Qué OS/arquitectura? (Linux x86_64, FreeBSD arm64)
   ├── ¿Qué librerías están disponibles? (libssl, libz, libpthread)
   ├── ¿Qué headers existen? (sys/epoll.h, openssl/ssl.h)
   └── ¿Qué funciones soporta el sistema? (epoll_create, kqueue)

2. GENERAR archivos adaptados al sistema:
   ├── Makefile (a partir de Makefile.in)
   ├── config.h (defines: HAVE_EPOLL, HAVE_OPENSSL, etc.)
   └── config.status (registro de la configuración)

3. INFORMAR al usuario:
   └── Resumen de qué se detectó y qué se habilitó
```

### El ecosistema Autotools

```
Autor del proyecto escribe:         configure detecta y genera:
┌──────────────────────┐            ┌──────────────────────┐
│ configure.ac         │─autoconf──▶│ configure            │
│ (checks, features)   │            │ (script de shell)    │
├──────────────────────┤            ├──────────────────────┤
│ Makefile.am          │─automake──▶│ Makefile.in          │
│ (qué compilar)       │            │ (template)           │
└──────────────────────┘            └──────────────────────┘
                                             │
                                      ./configure
                                             │
                                             ▼
                                    ┌──────────────────────┐
                                    │ Makefile             │
                                    │ config.h             │
                                    │ config.log           │
                                    └──────────────────────┘
```

### Ejecutar configure

```bash
# Ejecución básica (usa defaults)
./configure

# La salida es extensa — muestra cada test
# checking for gcc... gcc
# checking for C compiler default output file name... a.out
# checking whether the C compiler works... yes
# checking for OpenSSL... yes
# ...
# config.status: creating Makefile
# config.status: creating config.h

# Si falta una dependencia, configure FALLA con un mensaje claro
# configure: error: Package requirements (libssl >= 1.1.0) were not met
```

### config.log: el archivo de diagnóstico

Cuando configure falla, `config.log` contiene los detalles completos de cada test que se ejecutó, incluyendo los comandos del compilador y sus errores exactos.

```bash
# Después de un error en configure:
tail -50 config.log
# Muestra el último test que falló con el error exacto del compilador

# Buscar el error específico
grep -i "error\|failed\|not found" config.log
```

---

## 4. Opciones de configure

### --help: ver todas las opciones

```bash
./configure --help

# La salida se divide en secciones:
# Installation directories:
#   --prefix=PREFIX    install in PREFIX [/usr/local]
#   --exec-prefix=...
#
# Fine tuning of the installation directories:
#   --bindir=DIR       user executables
#   --libdir=DIR       object code libraries
#   --includedir=DIR   C header files
#   --mandir=DIR       man documentation
#
# Optional Features:
#   --enable-FEATURE   Enable FEATURE
#   --disable-FEATURE  Disable FEATURE
#
# Optional Packages:
#   --with-PACKAGE     Use PACKAGE
#   --without-PACKAGE  Do not use PACKAGE
```

### --prefix: dónde instalar

```bash
# Default: /usr/local
./configure
# Instala en: /usr/local/bin, /usr/local/lib, /usr/local/share

# Prefix personalizado
./configure --prefix=/opt/myapp
# Instala en: /opt/myapp/bin, /opt/myapp/lib, /opt/myapp/share

# En el directorio home (sin root)
./configure --prefix=$HOME/.local
# Instala en: ~/.local/bin, ~/.local/lib, ~/.local/share

# "Como un paquete del sistema" (raramente recomendado)
./configure --prefix=/usr
# Instala en: /usr/bin, /usr/lib — puede CONFLICTAR con paquetes
```

```
Jerarquía de directorios con --prefix=/usr/local:

/usr/local/
├── bin/          Ejecutables (programa principal, herramientas)
├── sbin/         Ejecutables de administración
├── lib/          Librerías compartidas (.so) y estáticas (.a)
├── lib64/        Librerías (en sistemas 64-bit, a veces)
├── include/      Headers (.h) para desarrollo
├── share/
│   ├── man/      Páginas de manual
│   ├── doc/      Documentación
│   └── locale/   Traducciones
├── etc/          Configuración
└── var/          Datos variables
```

### --enable/--disable: features opcionales

```bash
# Habilitar una feature (por defecto deshabilitada)
./configure --enable-debug
./configure --enable-static
./configure --enable-threads

# Deshabilitar una feature (por defecto habilitada)
./configure --disable-shared
./configure --disable-nls

# Ejemplo real: nginx
./configure \
    --prefix=/etc/nginx \
    --sbin-path=/usr/sbin/nginx \
    --with-http_ssl_module \
    --with-http_v2_module \
    --with-http_gzip_static_module \
    --without-http_autoindex_module

# Ejemplo real: Python
./configure \
    --prefix=/opt/python3.12 \
    --enable-optimizations \
    --enable-shared \
    --with-ssl-default-suites=openssl
```

### --with/--without: dependencias externas

```bash
# Especificar la ubicación de una librería
./configure --with-openssl=/usr/local/ssl
./configure --with-pcre=/opt/pcre2
./configure --with-zlib=/usr/lib

# Deshabilitar soporte para una librería
./configure --without-readline
./configure --without-python

# Especificar compilador y flags
CC=clang CXX=clang++ ./configure
CFLAGS="-O3 -march=native" ./configure
LDFLAGS="-L/opt/lib" CPPFLAGS="-I/opt/include" ./configure
```

### Variables de entorno para configure

| Variable | Descripción | Ejemplo |
|----------|-------------|---------|
| `CC` | Compilador C | `CC=clang` |
| `CXX` | Compilador C++ | `CXX=g++-12` |
| `CFLAGS` | Flags de compilación C | `CFLAGS="-O2 -Wall"` |
| `CXXFLAGS` | Flags de compilación C++ | `CXXFLAGS="-O2 -std=c++17"` |
| `LDFLAGS` | Flags del linker | `LDFLAGS="-L/opt/lib -Wl,-rpath,/opt/lib"` |
| `CPPFLAGS` | Flags del preprocesador | `CPPFLAGS="-I/opt/include"` |
| `PKG_CONFIG_PATH` | Buscar .pc files | `PKG_CONFIG_PATH=/opt/lib/pkgconfig` |

### pkg-config: cómo configure encuentra librerías

```bash
# pkg-config proporciona flags de compilación para librerías instaladas
pkg-config --cflags openssl
# -I/usr/include/openssl

pkg-config --libs openssl
# -lssl -lcrypto

# Si configure no encuentra una librería, puede ser que:
# 1. No está instalada (instalar el -devel/-dev)
# 2. El .pc file no está en PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
./configure
```

---

## 5. make: compilación y targets

### Compilación básica

```bash
# Compilar usando los threads disponibles
make -j$(nproc)

# -j N = compilar N archivos en paralelo
# nproc = número de CPUs
# En un sistema con 8 cores: make -j8
```

### Cómo funciona make

```
Makefile define reglas:

  target: dependencias
      comando

Ejemplo simplificado:

  app: main.o utils.o
      gcc -o app main.o utils.o

  main.o: main.c main.h
      gcc -c main.c

  utils.o: utils.c utils.h
      gcc -c utils.c

make construye un DAG (grafo acíclico dirigido) de dependencias
y ejecuta solo lo necesario:

  main.c ──▶ main.o ──┐
                       ├──▶ app
  utils.c ──▶ utils.o ─┘

Si solo cambió utils.c, make recompila solo utils.o y re-linkea app.
No recompila main.o (no cambió su dependencia).
```

### Targets estándar

| Target | Descripción |
|--------|-------------|
| `make` (o `make all`) | Compilar todo |
| `make clean` | Borrar archivos objeto y ejecutables compilados |
| `make distclean` | Borrar todo lo generado (incluyendo Makefile y config.h) |
| `make install` | Instalar en el prefix |
| `make uninstall` | Desinstalar (si el proyecto lo soporta) |
| `make check` / `make test` | Ejecutar tests |
| `make dist` | Crear tarball de distribución |
| `make distcheck` | dist + verificar que el tarball compila |

```bash
# Flujo completo típico
./configure --prefix=/opt/myapp
make -j$(nproc)
make check                    # Verificar que funciona
sudo make install             # Instalar

# Limpiar para recompilar con opciones diferentes
make distclean                # Borra TODO lo generado
./configure --prefix=/opt/myapp --enable-debug
make -j$(nproc)
```

### Verbose output

```bash
# Si make oculta los comandos (muestra solo "CC main.o"):
make V=1                      # Autotools: verbose
make VERBOSE=1                # CMake: verbose

# Ver exactamente qué se ejecuta (debug de problemas de compilación)
```

> **Pregunta de predicción**: Ejecutas `make -j$(nproc)` y la compilación falla en un archivo. Corriges el error y ejecutas `make -j$(nproc)` de nuevo. ¿Recompila todo desde cero o solo lo que faltaba?

Solo recompila lo que faltaba. make rastrea qué archivos .o se generaron exitosamente y cuáles no. Los que ya se compilaron antes del error no se recompilan (sus timestamps son más nuevos que los .c correspondientes). Solo se recompila el archivo que falló y todo lo que depende de él. Esto es mucho más rápido que `make clean && make`.

---

## 6. make check / make test: verificar la compilación

### Por qué ejecutar tests

```
Compiló sin errores ≠ funciona correctamente

El compilador verifica SINTAXIS.
Los tests verifican COMPORTAMIENTO.

Ejemplo:
  El código compila, pero una función de cifrado produce
  resultados incorrectos porque tu sistema usa big-endian
  y el código asumía little-endian. Solo los tests lo detectan.
```

### Ejecutar tests

```bash
# Autotools suele usar "make check"
make check

# Algunos proyectos usan "make test"
make test

# Salida típica:
# PASS: test_basic
# PASS: test_crypto
# PASS: test_network
# FAIL: test_performance    ← un test falló
# ==================
# 3 tests passed
# 1 test failed
# See test-suite.log for details

# Ver detalles del fallo
cat test-suite.log
```

### CMake projects

```bash
# CMake usa ctest
cd build/
ctest
ctest --verbose           # Ver salida de cada test
ctest -j$(nproc)          # Tests en paralelo
ctest -R "test_crypto"    # Solo tests que coincidan con regex
```

### ¿Qué hacer cuando un test falla?

```
1. ¿Es un test conocido como inestable (flaky)?
   → Revisar el issue tracker del proyecto

2. ¿Está relacionado con tu configuración?
   → Revisar si --disable-X o --without-Y afecta
   → Verificar que las dependencias de test están instaladas

3. ¿Es un test de rendimiento?
   → Tests de timing fallan en VMs o sistemas bajo carga
   → Generalmente ignorable

4. ¿Es un test funcional que falla?
   → NO instalar. El software puede estar roto en tu sistema.
   → Reportar el bug upstream con:
     - config.log
     - test-suite.log
     - uname -a
     - versiones de gcc, libc, librerías
```

---

## 7. make install y DESTDIR

### make install estándar

```bash
# Instalar en --prefix (default: /usr/local)
sudo make install

# Qué hace internamente:
# install -m 755 myapp /usr/local/bin/myapp
# install -m 644 libmyapp.so /usr/local/lib/libmyapp.so
# install -m 644 myapp.h /usr/local/include/myapp.h
# install -m 644 myapp.1 /usr/local/share/man/man1/myapp.1
```

### DESTDIR: instalación en raíz alternativa

`DESTDIR` antepone un directorio raíz al prefix **sin cambiar las rutas internas del binario**. Es fundamental para empaquetar software.

```bash
# Sin DESTDIR: instala directamente
sudo make install
# Copia a: /usr/local/bin/myapp

# Con DESTDIR: instala en un staging directory
make DESTDIR=/tmp/staging install
# Copia a: /tmp/staging/usr/local/bin/myapp
#                        ^^^^^^^^^^^^^^^^
#                        La ruta interna sigue siendo /usr/local/bin
```

```
DESTDIR vs --prefix:

--prefix=/opt/myapp
  Cambia la ruta INTERNA (dónde el binario busca sus archivos)
  El binario está hardcoded para buscar en /opt/myapp/
  make install → copia a /opt/myapp/bin/

DESTDIR=/tmp/pkg
  NO cambia la ruta interna (sigue siendo /usr/local o lo que sea prefix)
  Solo cambia DÓNDE se copian los archivos físicamente
  make install → copia a /tmp/pkg/usr/local/bin/
  El binario sigue buscando en /usr/local/ al ejecutarse

Uso típico: combinar ambos
  ./configure --prefix=/usr
  make DESTDIR=/tmp/staging install
  # Archivos en /tmp/staging/usr/bin/
  # Binario busca archivos en /usr/
  # Perfecto para crear un .rpm o .deb
```

### DESTDIR para crear paquetes

```bash
# Compilar
./configure --prefix=/usr
make -j$(nproc)
make check

# Instalar en staging
make DESTDIR=/tmp/pkg-myapp install

# Ahora /tmp/pkg-myapp/ contiene la estructura que iría en /
ls -R /tmp/pkg-myapp/
# /tmp/pkg-myapp/
# └── usr/
#     ├── bin/myapp
#     ├── lib/libmyapp.so.1
#     ├── include/myapp.h
#     └── share/man/man1/myapp.1

# Crear un paquete RPM o DEB a partir de esta estructura
# (ver checkinstall más abajo)
```

### make uninstall

```bash
# Algunos proyectos soportan uninstall
sudo make uninstall
# Borra exactamente los archivos que make install copió

# Verificar ANTES de ejecutar:
make -n uninstall    # -n = dry-run, muestra qué haría

# MUCHOS proyectos NO implementan uninstall
# Si no existe, tendrás que borrar los archivos manualmente
```

### Actualizar software compilado

```bash
# Descargar nueva versión
wget https://example.org/software-2.1.tar.gz
tar xzf software-2.1.tar.gz
cd software-2.1/

# Usar la MISMA configuración que la versión anterior
# (si guardaste la línea de configure)
./configure --prefix=/opt/myapp --enable-threads --with-openssl
make -j$(nproc)
make check

# Desinstalar la versión anterior (si soporta uninstall)
cd ../software-2.0/
sudo make uninstall

# Instalar la nueva
cd ../software-2.1/
sudo make install
```

---

## 8. Otros sistemas de build: CMake y Meson

### CMake

CMake es el sistema de build más común en proyectos C/C++ modernos, especialmente en el ecosistema C++.

```bash
# Flujo CMake estándar
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/myapp
make -j$(nproc)
make test
sudo make install

# O con el wrapper moderno:
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/myapp
cmake --build build -j$(nproc)
ctest --test-dir build
cmake --install build
```

### Opciones de CMake

```bash
# Ver todas las opciones configurables
cmake -B build -L       # Listar variables
cmake -B build -LH      # Con descripciones

# Configurar opciones
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=/opt/myapp \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTING=ON \
    -DWITH_SSL=ON

# Tipos de build
# Debug:          -O0, símbolos de debug (-g)
# Release:        -O3, sin debug
# RelWithDebInfo: -O2, con debug (para profiling)
# MinSizeRel:     -Os, optimizar tamaño
cmake -B build -DCMAKE_BUILD_TYPE=Release

# DESTDIR con CMake
make DESTDIR=/tmp/staging install
# O:
cmake --install build --prefix /tmp/staging
```

### Equivalencias Autotools → CMake

| Autotools | CMake |
|-----------|-------|
| `./configure` | `cmake -B build` |
| `--prefix=/opt` | `-DCMAKE_INSTALL_PREFIX=/opt` |
| `--enable-X` | `-DENABLE_X=ON` |
| `--disable-X` | `-DENABLE_X=OFF` |
| `--with-ssl=/path` | `-DOPENSSL_ROOT_DIR=/path` |
| `CFLAGS="-O2"` | `-DCMAKE_C_FLAGS="-O2"` |
| `make -j$(nproc)` | `cmake --build build -j$(nproc)` |
| `make check` | `ctest --test-dir build` |
| `make install` | `cmake --install build` |
| `make DESTDIR=/tmp install` | `DESTDIR=/tmp cmake --install build` |
| `make clean` | `cmake --build build --target clean` |

### Meson + Ninja

Meson es un sistema de build más reciente, usado por GNOME, systemd, GStreamer y otros.

```bash
# Flujo Meson
meson setup build --prefix=/opt/myapp
ninja -C build
ninja -C build test
sudo ninja -C build install

# Opciones
meson setup build \
    --prefix=/opt/myapp \
    --buildtype=release \
    -Dfeature=enabled \
    -Dssl=true

# DESTDIR con Meson
DESTDIR=/tmp/staging ninja -C build install

# Listar opciones
meson configure build
```

### Resumen de sistemas de build

```
Archivo indicador   Sistema     Comando de configuración
──────────────────────────────────────────────────────────
configure           Autotools   ./configure --prefix=...
configure.ac        Autotools   autoreconf -i && ./configure
CMakeLists.txt      CMake       cmake -B build -DCMAKE_INSTALL_PREFIX=...
meson.build         Meson       meson setup build --prefix=...
Makefile (solo)     Make puro   make PREFIX=/opt/... (editar Makefile)
```

---

## 9. Gestionar software compilado manualmente

### Problema: rastrear qué se instaló

```bash
# Después de "sudo make install", ¿qué archivos se crearon?

# Método 1: install_manifest (algunos proyectos CMake)
cat build/install_manifest.txt

# Método 2: comparar antes y después
# ANTES de make install:
find /usr/local -type f > /tmp/before.txt
sudo make install
find /usr/local -type f > /tmp/after.txt
diff /tmp/before.txt /tmp/after.txt
# Las líneas con > son los archivos nuevos

# Método 3: strace make install (capturar qué archivos escribe)
sudo strace -f -e trace=open,openat,rename,link,symlink \
    make install 2>&1 | grep -E "^(open|rename)" > /tmp/install.log
```

### checkinstall: crear paquetes desde make install

`checkinstall` intercepta `make install` y crea un paquete `.deb` o `.rpm` rastreable por el gestor de paquetes.

```bash
# En vez de:
sudo make install

# Usar:
sudo checkinstall --pkgname=myapp --pkgversion=2.0 --default
# Crea un .deb (Debian/Ubuntu) o .rpm (Fedora/RHEL)
# que puedes instalar con dpkg -i o rpm -i
# y desinstalar limpiamente con apt remove o dnf remove
```

### /usr/local: la zona para software manual

```bash
# /usr/local está en el PATH por defecto
# y es el prefix default de configure
echo $PATH
# /usr/local/bin:/usr/bin:/bin:...

# Verificar que el linker encuentra las librerías
sudo ldconfig
# Actualiza el cache de librerías después de instalar .so en /usr/local/lib

# Si instalaste en un prefix no estándar:
echo "/opt/myapp/lib" | sudo tee /etc/ld.so.conf.d/myapp.conf
sudo ldconfig

# Para que el shell encuentre los binarios:
export PATH="/opt/myapp/bin:$PATH"
# Poner en ~/.bashrc o /etc/profile.d/myapp.sh para persistir
```

### stow: gestión elegante de /usr/local

GNU Stow gestiona symlinks en /usr/local para que cada paquete compilado mantenga sus archivos en un directorio separado.

```bash
# Instalar cada software en su propio directorio
./configure --prefix=/usr/local/stow/myapp-2.0
make -j$(nproc) && sudo make install

# stow crea symlinks en /usr/local apuntando al directorio del paquete
cd /usr/local/stow
sudo stow myapp-2.0
# /usr/local/bin/myapp → /usr/local/stow/myapp-2.0/bin/myapp

# Para actualizar: desactivar versión vieja, activar nueva
sudo stow -D myapp-2.0    # Remove symlinks
sudo stow myapp-2.1       # Create new symlinks

# Para desinstalar limpiamente
sudo stow -D myapp-2.0
sudo rm -rf /usr/local/stow/myapp-2.0
```

```
Sin stow:                          Con stow:

/usr/local/                        /usr/local/
├── bin/                           ├── bin/
│   ├── app1     ← ¿de dónde?     │   ├── app1 → stow/app1-1.0/bin/app1
│   ├── app2     ← ¿de dónde?     │   └── app2 → stow/app2-3.2/bin/app2
│   └── tool     ← ¿de dónde?     ├── stow/
├── lib/                           │   ├── app1-1.0/    ← todo junto
│   ├── libapp1.so                 │   │   ├── bin/app1
│   └── libapp2.so                 │   │   └── lib/libapp1.so
└── include/                       │   └── app2-3.2/    ← todo junto
    ├── app1.h                     │       ├── bin/app2
    └── app2.h                     │       └── lib/libapp2.so
                                   └── ...
Todo mezclado, imposible           Cada paquete aislado,
saber qué pertenece a qué.        desinstalar = rm -rf + stow -D
```

---

## 10. Errores comunes

### Error 1: No leer INSTALL/README antes de compilar

```bash
# MAL — asumir que todo proyecto es ./configure && make
./configure && make -j$(nproc) && sudo make install
# Falla porque este proyecto usa CMake, no Autotools

# BIEN — verificar qué sistema de build usa
ls CMakeLists.txt meson.build configure configure.ac Makefile 2>/dev/null
cat INSTALL
```

### Error 2: Faltan dependencias de desarrollo

```bash
# Error típico de configure:
# configure: error: OpenSSL library not found

# MAL — instalar solo el paquete runtime
sudo dnf install openssl

# BIEN — instalar el paquete de desarrollo
sudo dnf install openssl-devel      # Fedora/RHEL
sudo apt install libssl-dev          # Debian/Ubuntu

# Para identificar qué paquete -devel necesitas:
# Fedora: dnf provides '*/ssl.h'
# Debian: apt-file search ssl.h
```

### Error 3: Instalar con --prefix=/usr

```bash
# PELIGROSO — sobrescribe archivos del gestor de paquetes
./configure --prefix=/usr
sudo make install
# Tu versión compilada reemplaza la del sistema
# Las actualizaciones del sistema la sobrescriben de vuelta
# Confusión total sobre qué versión está instalada

# BIEN — usar /usr/local (default) o /opt/nombre
./configure --prefix=/usr/local    # Default, seguro
./configure --prefix=/opt/myapp    # Aislado
```

### Error 4: Olvidar ldconfig después de instalar librerías

```bash
# Después de make install, el programa no arranca:
# error while loading shared libraries: libmyapp.so.1: cannot open
# shared object file: No such file or directory

# La librería está instalada pero el linker no la conoce
sudo ldconfig    # Actualizar cache
# O verificar que el directorio está en la config:
cat /etc/ld.so.conf.d/*.conf | grep -i myapp

# Alternativa temporal:
LD_LIBRARY_PATH=/usr/local/lib ./myapp
```

### Error 5: No usar DESTDIR al empaquetar

```bash
# MAL — make install durante el build de un paquete
# instala archivos directamente en el sistema del builder
make install

# BIEN — usar DESTDIR para staging
make DESTDIR="$pkgdir" install
# Los archivos se copian a $pkgdir/, no al sistema real
```

---

## 11. Cheatsheet

```
FLUJO AUTOTOOLS
  ./configure --help                      Ver opciones
  ./configure --prefix=/opt/app           Configurar
  make -j$(nproc)                         Compilar
  make check                              Tests
  sudo make install                       Instalar
  sudo make uninstall                     Desinstalar (si existe)
  make distclean                          Limpiar todo

FLUJO CMAKE
  cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/app
  cmake --build build -j$(nproc)          Compilar
  ctest --test-dir build                  Tests
  sudo cmake --install build              Instalar

FLUJO MESON
  meson setup build --prefix=/opt/app     Configurar
  ninja -C build                          Compilar
  ninja -C build test                     Tests
  sudo ninja -C build install             Instalar

DESTDIR (todos los sistemas)
  make DESTDIR=/tmp/staging install       Autotools
  DESTDIR=/tmp/staging cmake --install b  CMake
  DESTDIR=/tmp/staging ninja -C b install Meson

OPCIONES COMUNES DE CONFIGURE
  --prefix=DIR              Directorio base de instalación
  --enable-X / --disable-X  Habilitar/deshabilitar features
  --with-X / --without-X    Con/sin dependencia
  CC=clang CFLAGS="-O3"     Variables de compilador

POST-INSTALACIÓN
  sudo ldconfig                           Actualizar cache de libs
  export PATH="/opt/app/bin:$PATH"        Añadir al PATH
  echo "/opt/app/lib" | sudo tee /etc/ld.so.conf.d/app.conf
                                          Registrar librerías

GESTIÓN
  checkinstall                            Crear .deb/.rpm desde make install
  stow / stow -D                         Gestionar symlinks en /usr/local
  find /usr/local -newer /tmp/timestamp   Encontrar archivos instalados
```

---

## 12. Ejercicios

### Ejercicio 1: Compilar un programa C simple con Autotools

**Objetivo**: Experimentar el flujo configure → make → make install completo.

```bash
# 1. Crear un proyecto mínimo con Autotools
mkdir -p /tmp/autotools-lab/src
cd /tmp/autotools-lab

# Código fuente
cat > src/hello.c << 'EOF'
#include <config.h>
#include <stdio.h>

int main(void) {
    printf("%s version %s\n", PACKAGE, VERSION);
    printf("Compiled with prefix: %s\n", PREFIX);
    return 0;
}
EOF

# configure.ac
cat > configure.ac << 'EOF'
AC_INIT([hello], [1.0], [bug@example.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_DEFINE_UNQUOTED([PREFIX], ["$prefix"], [Installation prefix])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
EOF

# Makefile.am (raíz)
cat > Makefile.am << 'EOF'
SUBDIRS = src
EOF

# src/Makefile.am
cat > src/Makefile.am << 'EOF'
bin_PROGRAMS = hello
hello_SOURCES = hello.c
EOF

# 2. Generar configure y Makefile.in
autoreconf --install

# 3. Configurar con prefix personalizado
./configure --prefix=/tmp/autotools-lab/install

# 4. Compilar
make -j$(nproc)

# 5. Ejecutar el binario directamente (antes de instalar)
./src/hello

# 6. Instalar
make install
ls -R /tmp/autotools-lab/install/

# 7. Ejecutar desde el prefix de instalación
/tmp/autotools-lab/install/bin/hello

# 8. Desinstalar
make uninstall
ls /tmp/autotools-lab/install/bin/    # Debería estar vacío

# 9. DESTDIR: instalar en staging
make DESTDIR=/tmp/autotools-lab/staging install
find /tmp/autotools-lab/staging -type f

# 10. Limpiar
cd /
rm -rf /tmp/autotools-lab
```

**Pregunta de reflexión**: En el paso 9, `DESTDIR=/tmp/autotools-lab/staging` y `--prefix=/tmp/autotools-lab/install`. ¿Cuál es la ruta completa donde se copió el binario? Cuando ese binario se ejecute, ¿dónde buscará sus archivos (librerías, configs)?

> El binario se copió a `/tmp/autotools-lab/staging/tmp/autotools-lab/install/bin/hello` (DESTDIR + prefix). Pero cuando se ejecute, buscará sus archivos en `/tmp/autotools-lab/install/` (solo el prefix), porque DESTDIR no afecta las rutas compiladas dentro del binario. DESTDIR es solo un "punto de montaje virtual" para empaquetar — la idea es que el contenido de staging se copie después a la raíz `/`, y entonces las rutas internas coincidirán.

---

### Ejercicio 2: Compilar con CMake y explorar opciones

**Objetivo**: Experimentar el flujo CMake y entender las opciones configurables.

```bash
# 1. Crear un proyecto CMake mínimo
mkdir -p /tmp/cmake-lab/src
cd /tmp/cmake-lab

cat > src/main.c << 'EOF'
#include <stdio.h>

int main(void) {
#ifdef ENABLE_GREETING
    printf("Hello! Greeting feature is ENABLED\n");
#else
    printf("Greeting feature is DISABLED\n");
#endif
    return 0;
}
EOF

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.10)
project(greeting VERSION 1.0 LANGUAGES C)

option(ENABLE_GREETING "Enable greeting message" OFF)

add_executable(greeting src/main.c)

if(ENABLE_GREETING)
    target_compile_definitions(greeting PRIVATE ENABLE_GREETING)
endif()

install(TARGETS greeting DESTINATION bin)
EOF

# 2. Configurar sin la feature
cmake -B build-default -DCMAKE_INSTALL_PREFIX=/tmp/cmake-lab/inst1
cmake --build build-default
./build-default/greeting
# Debería mostrar: "Greeting feature is DISABLED"

# 3. Configurar con la feature habilitada
cmake -B build-enabled -DCMAKE_INSTALL_PREFIX=/tmp/cmake-lab/inst2 \
    -DENABLE_GREETING=ON
cmake --build build-enabled
./build-enabled/greeting
# Debería mostrar: "Hello! Greeting feature is ENABLED"

# 4. Ver opciones configurables
cmake -B build-default -LH

# 5. Instalar con DESTDIR
DESTDIR=/tmp/cmake-lab/staging cmake --install build-enabled
find /tmp/cmake-lab/staging -type f

# 6. Limpiar
cd /
rm -rf /tmp/cmake-lab
```

**Pregunta de reflexión**: Observa que CMake usa un directorio `build/` separado del código fuente (out-of-source build). ¿Cuál es la ventaja sobre Autotools, que por defecto genera archivos en el directorio del código fuente?

> El out-of-source build mantiene el directorio de código fuente limpio — ningún archivo generado (.o, Makefile, config.h) se mezcla con el código. Esto permite tener **múltiples builds** simultáneos (build-debug/, build-release/, build-enabled/) sin interferencia. Para "limpiar", simplemente borras el directorio de build (`rm -rf build/`) sin riesgo de borrar código fuente. En Autotools, `make distclean` a veces no limpia todo, y es fácil hacer `git add .` accidentalmente incluyendo archivos generados.

---

### Ejercicio 3: Diagnosticar y resolver fallos de compilación

**Objetivo**: Practicar la resolución de los errores más comunes al compilar desde fuente.

```bash
# Este ejercicio simula errores reales y te guía para resolverlos

# 1. Error: falta un header (dependencia -devel no instalada)
mkdir -p /tmp/compile-errors && cd /tmp/compile-errors

cat > needs_header.c << 'EOF'
#include <zlib.h>
#include <stdio.h>
int main(void) {
    printf("zlib version: %s\n", zlibVersion());
    return 0;
}
EOF

# Intentar compilar (puede fallar si zlib-devel no está instalada)
gcc -o needs_header needs_header.c -lz 2>&1
# Si falla: "fatal error: zlib.h: No such file or directory"
# Solución:
#   Fedora: sudo dnf install zlib-devel
#   Debian: sudo apt install zlib1g-dev

# 2. Error: librería no encontrada por el linker
cat > needs_lib.c << 'EOF'
#include <stdio.h>
extern int fake_function(void);
int main(void) {
    printf("result: %d\n", fake_function());
    return 0;
}
EOF

gcc -o needs_lib needs_lib.c -lfake 2>&1
# "cannot find -lfake" → la librería libfake.so no existe
# Diagnóstico:
ldconfig -p | grep fake    # ¿Está en el cache del linker?
find /usr -name "libfake*" # ¿Existe en algún lado?

# 3. Error: versión incompatible de librería
cat > version_check.c << 'EOF'
#include <stdio.h>
// Simular: compiló con header v2 pero linked con lib v1
int main(void) {
    printf("This demonstrates version mismatch issues\n");
    return 0;
}
EOF

# Diagnóstico de versiones:
pkg-config --modversion zlib 2>/dev/null    # Versión de la lib
gcc --version | head -1                      # Versión del compilador

# 4. Limpiar
cd /
rm -rf /tmp/compile-errors
```

**Pregunta de reflexión**: Un proyecto requiere `libfoo >= 2.0` pero tu distribución solo tiene la versión 1.5. ¿Cuáles son tus opciones, de más segura a más arriesgada? ¿Cuál elegirías en un servidor de producción?

> Opciones ordenadas de más segura a más arriesgada: (1) Buscar un repo adicional (COPR/PPA/EPEL) que tenga libfoo >= 2.0 empaquetado — se actualiza con el sistema. (2) Compilar libfoo 2.0 con `--prefix=/opt/libfoo2` y compilar el proyecto con `PKG_CONFIG_PATH=/opt/libfoo2/lib/pkgconfig` — aislado, no afecta al sistema. (3) Compilar libfoo 2.0 con `--prefix=/usr/local` — puede conflictar con la versión del sistema. (4) Compilar libfoo 2.0 con `--prefix=/usr` — reemplaza la versión del sistema, puede romper otros paquetes que dependen de la 1.5. En producción, la opción (1) o (2) — nunca la (4).

---

> **Siguiente capítulo**: C07 — Proyecto: System Health Check.
