# T01 — Patrón clásico: configure, make, make install

## Errata y mejoras sobre el material base

1. **`make -j` sin número ≠ `make -j$(nproc)`** — El material dice que `make -j`
   (sin número) "usa tantos jobs como CPUs disponibles". Incorrecto: `make -j` sin
   argumento lanza **jobs ilimitados** en paralelo, lo que puede saturar la memoria
   y congelar el sistema. Siempre usar `make -j$(nproc)` para limitar al número de
   CPUs.

2. **"Rust y Go producen binarios estáticos"** — Simplificación engañosa. Go sí
   produce binarios estáticos por defecto (incluye su propio runtime). Rust por
   defecto enlaza dinámicamente contra libc (`ldd` muestra dependencias); se puede
   forzar estático con `--target x86_64-unknown-linux-musl`, pero no es el default.

3. **"Hendrix"** como usuario de Meson+Ninja — No existe tal proyecto. Probablemente
   se confundió con **PipeWire** (que sí usa Meson).

4. **`encontrarflags`** → `encontrar flags` (espacio faltante en encabezado).

5. **`[]: stdio.h`** → Artefacto de formato. Debería ser simplemente "headers:
   stdio.h, stdlib.h, etc."

6. **Lab: `cat /etc/ld.so.conf.d/`** → No se puede hacer `cat` a un directorio.
   Corregido a `ls /etc/ld.so.conf.d/`.

7. **Lab dummy library** — Se crea en `/tmp` pero ldconfig no escanea `/tmp`.
   Para que el ejercicio funcione, la biblioteca debe copiarse a una ruta que
   ldconfig conozca (ej: `/usr/local/lib/`).

---

## Por qué compilar desde fuente

```
┌──────────────────────────────────────────────────────────┐
│              COMPILAR DESDE FUENTE                        │
│              ¿Cuándo tiene sentido?                       │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  Sí:                                                     │
│  - El software NO existe en los repos de tu distro       │
│  - Necesitas una versión más nueva que la del repo       │
│  - Necesitas compilar con opciones/módulos específicos   │
│    (ej: nginx con módulos extra, FFmpeg con codecs)      │
│  - Estás desarrollando o parcheando el software          │
│  - Estás aprendiendo build systems (este curso)          │
│                                                          │
│  No:                                                     │
│  - La versión del repo es suficiente                     │
│  - No quieres gestionar updates de seguridad manualmente │
│  - Es un servidor de producción (usa paquetes soportados)│
│  - Necesitas reproducibilidad (paquetes son versionados) │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## El patrón de tres pasos (GNU Build System / Autotools)

```
Fuente: programa-1.0.tar.gz
              │
              ▼
┌──────────────────────────────────────────────────────────┐
│ 1. DESCARGAR Y EXTRAER                                   │
│    tar -xzf programa-1.0.tar.gz                          │
│    cd programa-1.0/                                      │
└─────────────────────────┬────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│ 2. CONFIGURAR (./configure)                              │
│    Detecta SO, arquitectura, compiladores, librerías     │
│    Genera: Makefile, config.h                            │
│    Si falta algo → error con mensaje claro               │
└─────────────────────────┬────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│ 3. COMPILAR (make -j$(nproc))                            │
│    gcc compila cada .c → .o (object file)                │
│    Linker une los .o → ejecutable/librería               │
└─────────────────────────┬────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────┐
│ 4. INSTALAR (sudo make install)                          │
│    Copia binarios → $PREFIX/bin/                         │
│    Copia libs → $PREFIX/lib/                             │
│    Copia headers → $PREFIX/include/                      │
│    Copia man pages → $PREFIX/share/man/                  │
│    ⚠ NO queda registrado en el gestor de paquetes        │
└──────────────────────────────────────────────────────────┘
```

---

## Paso 1: Descargar y extraer

### Formatos de distribución

```bash
# .tar.gz  → gzip (más común históricamente)
# .tar.bz2 → bzip2 (más lento, compresión intermedia)
# .tar.xz  → xz (mejor compresión, estándar actual)

tar -xzf software-1.0.tar.gz    # .tar.gz
tar -xjf software-1.0.tar.bz2   # .tar.bz2
tar -xJf software-1.0.tar.xz    # .tar.xz
tar -xf  software-1.0.tar.xz    # auto-detecta la compresión
```

### Desde git

```bash
# Algunos proyectos solo se distribuyen vía git
git clone https://github.com/project/software.git
cd software
git checkout v1.0    # versión específica (tag)

# Cuando se clona desde git, configure no existe aún.
# Hay que generarlo desde configure.ac:
autoreconf -fi
# Requiere: autoconf, automake, libtool
```

### Verificar la descarga

```bash
# Verificar checksum (buena práctica con fuentes de internet)
sha256sum software-1.0.tar.gz
# Comparar con el hash publicado en el sitio oficial

# Algunos proyectos firman con GPG
gpg --verify software-1.0.tar.gz.sig software-1.0.tar.gz
```

---

## Paso 2: ./configure

El script `configure` es un script de shell (generado por autotools) que:

```
./configure
     │
     ├─ Detecta el sistema operativo (uname)
     ├─ Detecta arquitectura (x86_64, aarch64, etc.)
     ├─ Busca el compilador C (gcc, clang)
     ├─ Busca el compilador C++ (g++, clang++)
     ├─ Verifica headers del sistema (stdio.h, stdlib.h, etc.)
     ├─ Verifica funciones de biblioteca (malloc, memcpy, etc.)
     ├─ Detecta librerías (openssl, zlib, pcre, etc.)
     ├─ Lee las opciones de la línea de comandos
     └─ Genera: Makefile y config.h
```

```bash
./configure
# checking for gcc... gcc
# checking whether the C compiler works... yes
# checking for library containing sqrt... -lm
# checking for SSL... yes
# ...
# configure: creating ./config.status
# configure: creating Makefile
```

### Si falta una dependencia

```
configure: error: "Package 'openssl' not found"

# Solución: instalar el paquete de desarrollo
# Debian/Ubuntu:
sudo apt install libssl-dev

# RHEL/AlmaLinux:
sudo dnf install openssl-devel
```

**Patrón de nombres:** en Debian los paquetes de desarrollo terminan en `-dev`;
en RHEL terminan en `-devel`. Contienen los headers (`.h`) y las librerías de
enlace que configure busca.

### Opciones comunes

```bash
# Ver TODAS las opciones disponibles
./configure --help

# --prefix: dónde se instala (la más importante)
./configure --prefix=/usr/local          # default (recomendado)
./configure --prefix=/opt/software-1.0   # instalación aislada

# Habilitar/deshabilitar funcionalidades
./configure --enable-ssl --enable-threads
./configure --disable-docs

# Especificar ubicación de librerías
./configure --with-ssl=/usr/local/ssl
./configure --without-readline

# Cross-compilation
./configure --host=aarch64-linux-gnu    # compilar para ARM desde x86
```

### --prefix: la decisión más importante

```
--prefix=/usr/local (DEFAULT — recomendado)
├── bin/     → /usr/local/bin/
├── lib/     → /usr/local/lib/
├── include/ → /usr/local/include/
├── etc/     → /usr/local/etc/
├── share/   → /usr/local/share/    (man pages, docs)
└── Es el lugar "oficial" para software compilado manualmente.
    Los gestores de paquetes NO tocan /usr/local.

--prefix=/usr
├── bin/     → /usr/bin/
├── lib/     → /usr/lib/
└── ⚠ SE MEZCLA con paquetes del sistema — PUEDE ROMPER APT/DNF
    NUNCA usar para compilar manualmente.

--prefix=/opt/software-1.0
├── bin/     → /opt/software-1.0/bin/
├── lib/     → /opt/software-1.0/lib/
└── Instalación AISLADA — fácil de desinstalar (rm -rf)
    Permite múltiples versiones en paralelo.
    Requiere agregar /opt/software-1.0/bin al PATH.
```

```bash
# Si usas --prefix=/opt/X, agregar al PATH:
echo 'export PATH="/opt/software-1.0/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

La convención FHS es clara:
- `/usr` → gestores de paquetes (apt/dnf)
- `/usr/local` → software compilado manualmente
- `/opt` → software autocontenido (aislado)

---

## Paso 3: make

```bash
# Compilar (secuencial — lento)
make

# Compilar en PARALELO (recomendado)
make -j$(nproc)
# nproc = número de CPUs disponibles
# En un 8-core: make -j8

# IMPORTANTE: make -j (sin número) = jobs ILIMITADOS
# Puede saturar la memoria y congelar el sistema.
# SIEMPRE especificar: make -j$(nproc)
```

### Qué ocurre durante make

```
make -j$(nproc)
  │
  ├─[1] Lee el Makefile generado por configure
  ├─[2] Determina qué archivos necesitan compilación
  │      (no recompila lo que no cambió — compilación incremental)
  ├─[3] Compila cada .c → .o en paralelo
  │      gcc -c src/main.c -o src/main.o -Iinclude -DFEATURE_X
  │      gcc -c src/util.c -o src/util.o -Iinclude
  └─[4] Linkea los .o → ejecutable final
         gcc src/main.o src/util.o -o programa -Llib -lssl -lz
```

### Errores comunes

```bash
# Función no declarada → falta un header/librería
# error: 'sqrt' was not declared in this scope
# → sudo apt install libm-dev

# Header no encontrado
# fatal error: openssl/ssl.h: No such file or directory
# → sudo apt install libssl-dev (Debian)
# → sudo dnf install openssl-devel (RHEL)

# Librería no encontrada en el linker
# /usr/bin/ld: cannot find -lpcre2
# → sudo apt install libpcre2-dev (Debian)
# → sudo dnf install pcre2-devel (RHEL)
```

---

## Paso 4: make install

```bash
# Instalar (copiar archivos compilados al prefix)
sudo make install
# install -m 755 programa /usr/local/bin/programa
# install -m 644 programa.1 /usr/local/share/man/man1/programa.1
# install -m 644 libfoo.so /usr/local/lib/libfoo.so

# Si el prefix es en tu home, no necesitas sudo:
./configure --prefix=$HOME/software
make -j$(nproc)
make install    # sin sudo — tu home, tus permisos
```

### El problema de make install

```
┌──────────────────────────────────────────────────────────┐
│ make install NO registra qué archivos instaló.           │
│                                                          │
│ - No hay manifiesto de archivos                          │
│ - No gestiona dependencias                               │
│ - No aparece en dpkg -l ni rpm -qa                       │
│ - No se actualiza con apt upgrade / dnf upgrade          │
│ - No permite desinstalar limpiamente                     │
│                                                          │
│ Para desinstalar:                                        │
│ 1. make uninstall (solo si el Makefile lo implementa)    │
│ 2. rm -rf /opt/X (si usaste prefix aislado)              │
│ 3. Eliminar archivos manualmente (tedioso, propenso a    │
│    errores)                                              │
│                                                          │
│ Solución: checkinstall (siguiente tópico)                │
└──────────────────────────────────────────────────────────┘
```

---

## Bibliotecas compartidas y ldconfig

Cuando compilas e instalas una librería en `/usr/local/lib/`, el linker
dinámico no la encuentra automáticamente:

```bash
# El error clásico:
./mi-programa: error while loading shared libraries:
  libmislib.so: cannot open shared object file

# El linker dinámico (ld-linux.so) busca en este orden:
# 1. LD_LIBRARY_PATH (variable de entorno, si está definida)
# 2. RPATH embebido en el binario (set at compile time)
# 3. /etc/ld.so.cache (generado por ldconfig)
# 4. /lib/, /usr/lib/ (rutas hardcoded de fallback)
```

```bash
# Solución 1: ejecutar ldconfig (si la ruta ya está configurada)
sudo ldconfig
# Lee /etc/ld.so.conf y /etc/ld.so.conf.d/*.conf
# Reconstruye la caché en /etc/ld.so.cache

# Solución 2: agregar la ruta a la configuración de ldconfig
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/local.conf
sudo ldconfig

# Solución 3: LD_LIBRARY_PATH (temporal, para testing)
LD_LIBRARY_PATH=/usr/local/lib ./mi-programa

# Verificar que ldconfig conoce tu librería
ldconfig -p | grep mislib
```

### pkg-config: encontrar flags de compilación

```bash
# pkg-config ayuda a encontrar los flags correctos para compilar
# contra una librería instalada

# Sin pkg-config (manual, propenso a errores):
gcc -o programa programa.c -I/usr/local/include -L/usr/local/lib -lmislib

# Con pkg-config (automático):
gcc -o programa programa.c $(pkg-config --cflags --libs mislib)

# Ver qué devuelve:
pkg-config --cflags mislib   # → -I/usr/local/include
pkg-config --libs mislib     # → -L/usr/local/lib -lmislib

# Muchos configure scripts usan pkg-config internamente
```

---

## Otros build systems

No todo usa autotools. Los build systems más comunes:

### CMake (C/C++ moderno)

```bash
# Usado por: LLVM/Clang, KDE, muchos proyectos C++ modernos
mkdir build && cd build
cmake ..
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..   # con prefix
make -j$(nproc)
sudo make install
```

### Meson + Ninja

```bash
# Usado por: GNOME, systemd, PipeWire, Mesa
meson setup build
cd build
ninja                    # compilación más rápida que make
sudo ninja install
```

### Makefile directo (sin configure)

```bash
# Proyectos simples que incluyen un Makefile hecho a mano
make
sudo make install PREFIX=/usr/local
```

### Cargo (Rust)

```bash
cargo build --release
sudo cp target/release/programa /usr/local/bin/

# O instalar directamente (en ~/.cargo/bin/):
cargo install programa
```

### Go

```bash
go build -o programa .
sudo cp programa /usr/local/bin/
# Go produce binarios estáticos por defecto (incluye su runtime)
```

**Nota sobre enlace estático vs dinámico:** Go produce binarios verdaderamente
estáticos por defecto. Rust enlaza dinámicamente contra libc por defecto
(verificable con `ldd`), aunque puede compilar estáticamente usando el target
`musl`. La mayoría de software C/C++ se enlaza dinámicamente.

---

## Ejemplo completo: compilar jq desde fuente

```bash
# jq es un procesador JSON de línea de comandos

# 1. Instalar dependencias de compilación
# Debian/Ubuntu:
sudo apt install build-essential autoconf automake libtool git
# RHEL/AlmaLinux:
# sudo dnf groupinstall "Development Tools"
# sudo dnf install autoconf automake libtool git

# 2. Clonar
cd /tmp
git clone https://github.com/jqlang/jq.git
cd jq
git checkout jq-1.7.1

# 3. Generar configure (necesario al clonar desde git)
autoreconf -fi

# 4. Configurar
./configure --prefix=/usr/local --disable-docs

# 5. Compilar
make -j$(nproc)

# 6. Probar sin instalar
./jq --version
echo '{"name":"test","value":123}' | ./jq '.'

# 7. Instalar
sudo make install

# 8. Verificar
which jq          # /usr/local/bin/jq
jq --version

# 9. Si instaló librerías, actualizar caché
sudo ldconfig

# 10. Limpiar fuentes
cd /tmp && rm -rf jq/
```

---

## Labs

### Lab 1 — El proceso completo

```bash
docker compose exec debian-dev bash -c '
echo "=== El patrón de 3 pasos ==="
echo ""
echo "  ./configure       ← detectar sistema, verificar deps, generar Makefile"
echo "  make -j\$(nproc)   ← compilar usando todos los cores"
echo "  sudo make install ← copiar archivos al prefix"
echo ""
echo "=== Herramientas necesarias ==="
echo ""
gcc --version 2>/dev/null | head -1 || echo "gcc: NO instalado"
make --version 2>/dev/null | head -1 || echo "make: NO instalado"
autoconf --version 2>/dev/null | head -1 || echo "autoconf: NO instalado"
echo ""
echo "CPUs disponibles: $(nproc)"
echo "  → make -j$(nproc) usaría $(nproc) compilaciones en paralelo"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Formatos de código fuente ==="
echo ""
echo "tar -xzf software.tar.gz    → gzip"
echo "tar -xjf software.tar.bz2   → bzip2"
echo "tar -xJf software.tar.xz    → xz (mejor compresión)"
echo "tar -xf  software.tar.xz    → auto-detecta"
echo ""
echo "Desde git:"
echo "  git clone URL && cd proyecto"
echo "  autoreconf -fi   ← generar ./configure desde configure.ac"
'
```

### Lab 2 — --prefix y ubicación de archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== --prefix: dónde se instala ==="
echo ""
echo "--- /usr/local (default) ---"
echo "  bin/     → /usr/local/bin/"
echo "  lib/     → /usr/local/lib/"
echo "  include/ → /usr/local/include/"
echo "  share/   → /usr/local/share/"
echo ""
echo "--- /usr (EVITAR) ---"
echo "  Se mezcla con paquetes del sistema"
echo "  Puede romper apt/dnf"
echo ""
echo "--- /opt/software-1.0 (aislado) ---"
echo "  Fácil de desinstalar: rm -rf"
echo "  Requiere agregar al PATH"
echo ""
echo "=== Convención FHS ==="
echo "  /usr        → paquetes del gestor (apt/dnf)"
echo "  /usr/local  → compilado manualmente"
echo "  /opt        → software autocontenido"
echo ""
echo "--- Contenido actual de /usr/local/bin ---"
ls /usr/local/bin/ 2>/dev/null | head -5 || echo "(vacío)"
'
```

### Lab 3 — ldconfig y bibliotecas compartidas

```bash
docker compose exec debian-dev bash -c '
echo "=== ldconfig: bibliotecas compartidas ==="
echo ""
echo "Si un programa dice:"
echo "  error while loading shared libraries: libfoo.so"
echo ""
echo "Solución:"
echo "  1. Verificar que la .so existe en /usr/local/lib/"
echo "  2. Agregar la ruta a /etc/ld.so.conf.d/"
echo "  3. Ejecutar sudo ldconfig"
echo ""
echo "--- Directorios configurados en ld.so.conf ---"
cat /etc/ld.so.conf 2>/dev/null
echo ""
echo "--- Archivos en /etc/ld.so.conf.d/ ---"
ls /etc/ld.so.conf.d/ 2>/dev/null
echo ""
echo "--- Total de librerías en la caché ---"
ldconfig -p 2>/dev/null | wc -l
echo ""
echo "--- Librerías en /usr/local/lib ---"
ls /usr/local/lib/*.so* 2>/dev/null | head -5 || echo "(ninguna)"
'
```

### Lab 4 — Otros build systems

```bash
docker compose exec debian-dev bash -c '
echo "=== Otros build systems ==="
echo ""
echo "--- Autotools (clásico) ---"
echo "  ./configure && make && make install"
echo ""
echo "--- CMake (C++ moderno) ---"
echo "  mkdir build && cd build && cmake .. && make"
cmake --version 2>/dev/null | head -1 || echo "  cmake: NO instalado"
echo ""
echo "--- Meson + Ninja ---"
echo "  meson setup build && cd build && ninja"
meson --version 2>/dev/null && echo "  meson: instalado" || echo "  meson: NO instalado"
ninja --version 2>/dev/null && echo "  ninja: instalado" || echo "  ninja: NO instalado"
echo ""
echo "--- Makefile directo ---"
echo "  make && make install PREFIX=/usr/local"
echo ""
echo "Todos producen el mismo resultado: binarios instalados en el sistema."
echo "La diferencia es cómo generan los Makefiles y qué tan rápido compilan."
'
```

---

## Ejercicios

### Ejercicio 1 — El flujo completo

Dibuja el flujo de compilar software desde fuente: desde `wget` hasta
`make install`. ¿Qué genera cada paso? ¿Qué necesita cada paso como
input? ¿Dónde quedan los archivos al final?

<details><summary>Predicción</summary>

```
wget/git clone
  → código fuente (.c, .h, configure.ac, Makefile.in)

autoreconf -fi (si se clonó desde git)
  → configure (script de shell ejecutable)

./configure --prefix=/usr/local
  → Makefile (con rutas y opciones detectadas)
  → config.h (macros de preprocesador)

make -j$(nproc)
  → .o (object files: uno por cada .c)
  → ejecutable/librería (resultado del linker)

sudo make install
  → Copia archivos al prefix:
    /usr/local/bin/programa
    /usr/local/lib/libfoo.so
    /usr/local/share/man/man1/programa.1
```

Cada paso consume el output del anterior. Si configure falla, no hay
Makefile y make no puede ejecutarse. Si make falla, no hay binarios
y make install no tiene nada que copiar.

</details>

### Ejercicio 2 — Opciones de configure

Ejecuta `./configure --help` en un proyecto (ej: GNU Hello). ¿Cuántas
opciones tiene? Identifica al menos 3 categorías: opciones de instalación,
de features, y de dependencias opcionales.

<details><summary>Predicción</summary>

```bash
cd /tmp
wget -q https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1
./configure --help | wc -l    # ~150+ líneas de opciones
```

Categorías típicas:
- **Installation directories:** `--prefix`, `--bindir`, `--libdir`, `--sysconfdir`
- **Features:** `--enable-FEATURE`, `--disable-FEATURE`
- **Dependencies:** `--with-PACKAGE`, `--without-PACKAGE`
- **System types:** `--build`, `--host`, `--target` (cross-compilation)

Todos los proyectos basados en autotools comparten las mismas opciones
de instalación (`--prefix`, `--bindir`, etc.) porque las hereda del
framework autotools. Las opciones de features y dependencias son
específicas de cada proyecto.

</details>

### Ejercicio 3 — --prefix aislado

Compila GNU Hello con `--prefix=/tmp/hello-install`. ¿Qué estructura de
directorios crea? ¿Qué contiene cada subdirectorio? Después borra todo
con un solo comando.

<details><summary>Predicción</summary>

```bash
cd /tmp/hello-2.12.1
./configure --prefix=/tmp/hello-install
make -j$(nproc)
make install    # sin sudo — /tmp es escribible por tu usuario

ls /tmp/hello-install/
# bin/    → hello (el ejecutable)
# share/  → man/, info/, locale/ (documentación y traducciones)

/tmp/hello-install/bin/hello
# Hello, world!

# Desinstalar: un solo comando
rm -rf /tmp/hello-install
```

La ventaja de `--prefix` aislado es clara: `rm -rf` elimina todo. No hay
archivos dispersos por el sistema, no hay riesgo de borrar algo de otro
paquete. El costo es tener que agregar `/tmp/hello-install/bin` al PATH.

</details>

### Ejercicio 4 — make -j y compilación paralela

Compara el tiempo de compilación de un proyecto con `make` (secuencial)
vs `make -j$(nproc)` (paralelo). ¿Cuántas CPUs tiene tu sistema? ¿Qué
pasa si usas `make -j` sin número?

<details><summary>Predicción</summary>

```bash
nproc    # ej: 4

# Secuencial
make clean
time make           # ~10 segundos (en un proyecto pequeño)

# Paralelo
make clean
time make -j$(nproc)   # ~3-4 segundos

# Sin número (PELIGROSO en proyectos grandes)
# make -j    → lanza jobs ILIMITADOS en paralelo
# En un proyecto con 1000 archivos .c, intenta compilar los 1000
# simultáneamente → satura la RAM → el sistema puede congelarse
```

`make -j$(nproc)` es la mejor opción: usa todos los cores sin exceder
la capacidad del sistema. En CI/CD a veces se usa `-j2` para no saturar
el runner compartido.

</details>

### Ejercicio 5 — Errores de configure

Intenta configurar un proyecto sin instalar sus dependencias de compilación.
¿Qué error produce `./configure`? ¿Cómo identificas qué paquete necesitas?
¿Cuál es la diferencia entre `libssl-dev` (Debian) y `openssl-devel` (RHEL)?

<details><summary>Predicción</summary>

```bash
# Al ejecutar ./configure sin las dependencias:
# configure: error: "Package 'openssl' not found"

# El error indica QUÉ falta. Para encontrar el paquete:
# Debian:
apt-cache search libssl | grep dev
# libssl-dev - Secure Sockets Layer toolkit - development files

# RHEL:
dnf search openssl | grep devel
# openssl-devel - Files for development of applications which...
```

La diferencia de nombres es convención de cada distro:
- Debian: `lib<nombre>-dev` (ej: `libssl-dev`, `libpcre2-dev`)
- RHEL: `<nombre>-devel` (ej: `openssl-devel`, `pcre2-devel`)

Ambos contienen lo mismo: headers (`.h`) y librerías de enlace (`.so`)
necesarios para compilar contra esa librería. El paquete base (sin
`-dev`/`-devel`) solo tiene la `.so` de runtime.

</details>

### Ejercicio 6 — Inspeccionar un Makefile

Después de ejecutar `./configure`, examina el Makefile generado. ¿Qué
targets tiene? ¿Existe `install`? ¿Existe `uninstall`? ¿Cuál es el
valor de `prefix`? ¿De dónde vino ese valor?

<details><summary>Predicción</summary>

```bash
# Targets comunes en un Makefile generado por autotools:
grep -E '^[a-zA-Z_-]+:' Makefile | head -15
# all:          ← target default (compila todo)
# install:      ← copia archivos al prefix
# install-strip: ← install + strip debug symbols
# uninstall:    ← elimina archivos instalados
# clean:        ← borra .o y ejecutables compilados
# distclean:    ← clean + borra Makefile y config.h (vuelve al estado pre-configure)
# check:        ← ejecuta tests
# dist:         ← crea el tarball de distribución

grep '^prefix' Makefile
# prefix = /usr/local   (o lo que se pasó a --configure)
```

El valor de `prefix` viene del argumento `--prefix=` pasado a `./configure`.
Si no se especificó, autotools usa `/usr/local` como default. Configure
sustituye `@prefix@` en `Makefile.in` para generar `Makefile`.

</details>

### Ejercicio 7 — ldconfig

Examina la configuración de ldconfig en tu sistema. ¿Cuántas librerías
conoce? ¿Qué rutas están configuradas? ¿Está `/usr/local/lib` incluido?
¿Qué pasaría si instalas una `.so` sin ejecutar `ldconfig`?

<details><summary>Predicción</summary>

```bash
# Rutas configuradas
cat /etc/ld.so.conf
# include /etc/ld.so.conf.d/*.conf

ls /etc/ld.so.conf.d/
# libc.conf    → /usr/local/lib (usualmente incluido aquí)
# x86_64-linux-gnu.conf → /usr/lib/x86_64-linux-gnu

# Total de librerías en la caché
ldconfig -p | wc -l    # ~1000-3000 dependiendo del sistema

# Buscar una librería específica
ldconfig -p | grep libssl
# libssl.so.3 (libc6,x86-64) => /lib/x86_64-linux-gnu/libssl.so.3
```

Si instalas una `.so` sin ejecutar `ldconfig`, la caché (`/etc/ld.so.cache`)
no se actualiza. Los programas que buscan esa librería a través del linker
dinámico no la encontrarán y fallarán con "cannot open shared object file".
La excepción es si la librería está en una ruta ya en `LD_LIBRARY_PATH`.

</details>

### Ejercicio 8 — make install vs checkinstall

Explica el problema fundamental de `make install`: ¿por qué no se puede
desinstalar limpiamente? ¿Qué información falta? ¿Cómo resuelve
`checkinstall` este problema? (Preview del siguiente tópico.)

<details><summary>Predicción</summary>

```
make install simplemente ejecuta comandos install/cp:
  install -m 755 programa /usr/local/bin/programa
  install -m 644 programa.1 /usr/local/share/man/man1/programa.1

NO mantiene un registro de qué archivos copió.
NO registra la versión ni las dependencias.
No aparece en dpkg -l ni rpm -qa.

Para desinstalar necesitarías:
1. Tener el directorio de compilación intacto (con el Makefile)
2. Que el Makefile tenga un target "uninstall"
3. Muchos proyectos NO implementan uninstall

checkinstall resuelve esto interceptando make install:
  En vez de: sudo make install
  Usas:      sudo checkinstall

checkinstall monitorea qué archivos se crean durante make install
y genera un .deb (o .rpm) que:
- Registra todos los archivos en el gestor de paquetes
- Permite desinstalar con apt remove / dpkg -r
- Aparece en dpkg -l
```

</details>

### Ejercicio 9 — Comparar build systems

Describe las diferencias entre autotools (configure/make), CMake, y
Meson+Ninja. ¿Qué genera cada uno? ¿Cuál es más rápido? ¿Cuál es el
más antiguo?

<details><summary>Predicción</summary>

| Aspecto | Autotools | CMake | Meson+Ninja |
|---------|-----------|-------|-------------|
| Antigüedad | ~1991 (el más antiguo) | ~2000 | ~2013 (el más moderno) |
| Genera | Makefiles | Makefiles o Ninja | Ninja files |
| Configurar | `./configure` | `cmake ..` | `meson setup build` |
| Compilar | `make -j$(nproc)` | `make -j$(nproc)` | `ninja` |
| Instalar | `make install` | `make install` | `ninja install` |
| Prefix | `--prefix=` | `-DCMAKE_INSTALL_PREFIX=` | `--prefix=` |
| Velocidad | La más lenta | Intermedia | La más rápida |
| Lenguaje | Shell/M4 macros | Su propio DSL | Python |
| Usuarios | GNU projects | LLVM, KDE, Qt | GNOME, systemd, PipeWire |

Autotools es el estándar de facto de GNU pero su sistema de macros M4 es
complejo. CMake es el más popular en C++ moderno. Meson es el más moderno
y rápido, con Ninja como backend de compilación optimizado.

</details>

### Ejercicio 10 — Panorama: compilación vs paquetes

Compara instalar nginx desde repositorio (`apt install nginx`) vs compilar
desde fuente. Lista al menos 5 diferencias en: tiempo, control, seguridad,
actualizaciones y limpieza.

<details><summary>Predicción</summary>

| Aspecto | `apt install nginx` | Compilar desde fuente |
|---------|--------------------|-----------------------|
| **Tiempo** | ~10 segundos | ~5-15 minutos (deps + compilación) |
| **Control** | Módulos que eligió el empaquetador | Eliges exactamente qué módulos incluir |
| **Seguridad** | Updates automáticos con `apt upgrade` | Debes monitorear CVEs y recompilar tú |
| **Versión** | La del repo (puede ser vieja) | Cualquier versión, incluida la última |
| **Dependencias** | Resueltas automáticamente | Debes instalar `*-dev` manualmente |
| **Desinstalar** | `apt remove nginx` (limpio) | `make uninstall` (si existe) o manual |
| **Registro** | En dpkg database | No registrado (invisible para apt) |
| **Reproducibilidad** | Misma versión en todos los servidores | Depende del entorno de compilación |

**Regla general:** usar paquetes siempre que sea posible. Compilar solo
cuando necesitas una versión específica, módulos no incluidos en el paquete,
o el software no está disponible en los repos de tu distro.

</details>
