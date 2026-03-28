# T01 — Patrón clásico: configure, make, make install

## Por qué compilar desde fuente

```
┌─────────────────────────────────────────────────────────────┐
│                    COMPILAR DESDE FUENTE                    │
│                    ¿Cuándo tiene sentido?                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ✓ Software NO disponible en repos de tu distro            │
│  ✓ Necesitas versión más nueva que la del repo              │
│  ✓ Necesitas compilar con opciones/módulos específicos     │
│  ✓ Estás desarrollando o parcheando el software             │
│  ✓ Estás aprendiendo build systems (este capítulo)         │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                    ¿Cuándo NO?                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ✗ La versión del repo es suficiente                        │
│  ✗ No quieres gestionar updates de seguridad manual         │
│  ✗ Es servidor de producción (usa paquetes del vendor)     │
│  ✗ Es un entorno donde la reproducibilidad es crítica       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## El patrón de tres pasos

```
Software: programa-1.0.tar.gz
              │
              ▼
┌─────────────────────────────────────────────────────────────┐
│ 1. DESCARGAR Y EXTRAER                                     │
│    tar -xzf programa-1.0.tar.gz                           │
│    cd programa-1.0/                                        │
└─────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. CONFIGURAR (./configure)                                 │
│    Detecta sistema, compiladores, librerías                 │
│    Genera: Makefile, config.h, archivos intermedios        │
│    Si falta algo → error con mensaje claro                 │
└─────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. COMPILAR (make)                                         │
│    gcc/g++ compila cada archivo .c → .o                     │
│    Linker une los .o en ejecutable/binario                  │
│    Resultado: binarios en el directorio del proyecto        │
└─────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. INSTALAR (sudo make install)                            │
│    Copia binarios → $PREFIX/bin/                           │
│    Copia libs → $PREFIX/lib/                               │
│    Copia headers → $PREFIX/include/                         │
│    Copia docs → $PREFIX/share/man/                          │
│    ⚠️ NO queda registrado en el gestor de paquetes          │
└─────────────────────────────────────────────────────────────┘
```

## Paso 1: Descargar y extraer

### Formatos de distribución

```bash
# .tar.gz  → gzip compression (más común)
# .tar.bz2 → bzip2 compression (más antiguo)
# .tar.xz  → xz compression (mejor compresión, estándar actual)
# .zip     → para proyectos Windows-oriented

tar -xzf software-1.0.tar.gz    # .tar.gz
tar -xjf software-1.0.tar.bz2   # .tar.bz2
tar -xJf software-1.0.tar.xz    # .tar.xz
tar -xf  software-1.0.tar.xz    # tar auto-detecta la compresión
```

### Desde git

```bash
# Algunos proyectos solo se distribuyen via git
git clone https://github.com/project/software.git
cd software

# Muchos necesitan generar configure desde configure.ac
autoreconf -fi

# Otros tienen configure listo desde el repo
ls configure  # si existe, usar directo
```

### Verificar la descarga

```bash
# Verificar checksum (siempre hacer esto con fuentes sensibles)
sha256sum software-1.0.tar.gz
# a1b2c3d4...  software-1.0.tar.gz

# Algunos proyectos firman con GPG
gpg --verify software-1.0.tar.gz.sig software-1.0.tar.gz
```

## Paso 2: ./configure

El script `configure` es un script de shell que hace varias verificaciones:

```bash
./configure
# checking for gcc... yes
# checking for C header file stdio.h... yes
# checking for library containing sqrt... -lm
# checking for SSL... yes
# checking for zlib... yes
# ...
# configure: creating ./config.status
# configure: creating Makefile
```

### Qué hace configure internamente

```
configure script
     │
     ├─[1] Detecta el sistema operativo (uname)
     ├─[2] Detecta arquitectura (x86_64, aarch64, etc.)
     ├─[3] Busca el compilador C (gcc, clang)
     ├─[4] Busca el compilador C++ (g++, clang++)
     ├─[5] Verifica headers ([]: stdio.h, stdlib.h, etc.)
     ├─[6] Verifica funciones de biblioteca (malloc, memcpy, etc.)
     ├─[7] Detecta librerías (openssl, zlib, pcre, etc.)
     ├─[8] Lee las opciones de la línea de comandos
     └─[9] Genera Makefile y config.h
```

### Si falta una dependencia

```
configure: error: "Package 'openssl' not found"
        Install the development package or use --with-ssl=no to build without SSL

configure: error: "libpcre2 not found"
        Try: sudo apt install libpcre2-dev
        Or:   sudo dnf install pcre2-devel
```

### Opciones comunes de configure

```bash
# Ver TODAS las opciones disponibles
./configure --help

# --prefix: dónde se instala TODO
./configure --prefix=/usr/local          # default (recomendado)
./configure --prefix=/opt/software-1.0  # instalación aislada

# Habilitar/deshabilitar funcionalidades
./configure --enable-ssl
./configure --disable-docs
./configure --enable-threads
./configure --without-readline

# Especificar ubicación de librerías
./configure --with-ssl=/usr/local/ssl
./configure --with-pcre=/usr/local/pcre

# Cross-compilation
./configure --host=aarch64-linux-gnu    # compilar para ARM desde x86
```

### --prefix: la decisión más importante

```
--prefix=/usr/local (DEFAULT — recomendado)
├── bin/     → /usr/local/bin/       (ejecutables)
├── lib/     → /usr/local/lib/       (librerías)
├── include/ → /usr/local/include/   (headers)
├── etc/     → /usr/local/etc/       (configuración)
├── share/   → /usr/local/share/     (docs, man pages, etc.)
└── Es el lugar "oficial" para software compilado manualmente
    Los gestores de paquetes NO tocan /usr/local

--prefix=/usr
├── bin/     → /usr/bin/
├── lib/     → /usr/lib/
└── ⚠️ SE MEZCLA con el sistema — PUEDE ROMPER APT/DNF
    NO USAR para compilar manualmente

--prefix=/opt/mi-software-1.0
├── bin/     → /opt/mi-software-1.0/bin/
├── lib/     → /opt/mi-software-1.0/lib/
└── ✓ Instalación AISLADA — fácil de desinstalar (rm -rf)
    ✓ Actualizar = instalar nueva versión en paralelo
    ✗ Hay que agregar /opt/mi-software-1.0/bin al PATH
```

```bash
# Si usas --prefix=/opt/mi-software, agrega al PATH:
echo 'export PATH="/opt/mi-software/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# O usa alternatives (gestión oficial de versiones múltiples):
sudo update-alternatives --install /usr/bin/mi-software mi-software /opt/mi-software-1.0/bin/mi-software 100
```

## Paso 3: make

```bash
# Compilar (secuencial, lento)
make

# Compilar en PARALELO (rápido)
make -j$(nproc)
# nproc = número de CPUs → make -j8 en un 8-core

# make -j sin número = tantos jobs como CPUs disponibles
```

### Qué ocurre durante make

```
make
  │
  ├─[1] Lee el Makefile generado por configure
  ├─[2] Determina qué archivos necesitan recompilación
  │      (no recompila lo que no cambió desde la última vez)
  ├─[3] Compila cada archivo .c → .o en paralelo (-j)
  │      gcc -c archivo.c -o archivo.o -Iinclude -DFEATURE_X
  └─[4] Linkea todos los .o → ejecutable final
         gcc archivo1.o archivo2.o -o ejecutable -Llib -lssl -lz
```

### Errores comunes durante make

```bash
# Error: función no declarada
# error: 'sqrt' was not declared in this scope
# Solución: instalar el package -dev (ej: libm-dev)

# Error: header no encontrado
# fatal error: openssl/ssl.h: No such file or directory
# Solución: sudo apt install libssl-dev (o dnf: openssl-devel)

# Error: librería no encontrada
# /usr/bin/ld: cannot find -lpcre
# Solución: sudo apt install libpcre2-dev (o dnf: pcre2-devel)

# Error: versión del compilador
# error: invalid conversion from 'void*' to 'char*'
# Solución: el código es antiguo, usar compilador más viejo o flags de compatibilidad
```

## Paso 4: make install

```bash
# Instalar (copiar archivos a las rutas del prefix)
sudo make install

# install es el comando que copia los archivos
# make muestra cada línea de install que ejecuta:
# install -m 755 programa /usr/local/bin/programa
# install -m 644 programa.1 /usr/local/share/man/man1/programa.1
```

```bash
# install SIN sudo si el prefix es en tu home
./configure --prefix=$HOME/software
make
make install
# Los archivos van a ~/software/bin/, ~/software/lib/, etc.

# Si el prefix es /usr/local, SÍ necesitas sudo
# porque /usr/local/bin/ es owned by root
```

### El problema de make install

```
┌─────────────────────────────────────────────────────────────┐
│ ⚠️ make install NO es idempotente de forma nativa             │
│                                                              │
│ - NO registra qué archivos instaló                          │
│ - NO registra la versión instalada                          │
│ - NO permite desinstalar limpiamente                         │
│ - NO aparece en dpkg -l ni rpm -qa                          │
│ - NO se actualiza con apt/dnf upgrade                       │
│                                                              │
│ Esto significa:                                             │
│   - Hacer "rm -rf /usr/local/bin/*" BORRA TODO             │
│     incluyendo binarios del sistema y otros software         │
│   - No hay forma de saber qué archivos pertenecen a qué     │
│     paquete compilado                                        │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Desinstalar:
make uninstall    # SOLO si el Makefile lo implementa
                  # Muchos proyectos NO lo implementan

# Si no hay make uninstall:
# Opción 1: desinstalar manualmente (tedioso, error-prone)
# Opción 2: usar checkinstall (próximo tópico)
# Opción 3: instalar en prefijo tmp y luego mover
```

## Bibliotecas compartidas y ldconfig

Cuando compilas e instalas una librería en `/usr/local/lib/`, el sistema no la ve automáticamente:

```bash
# El error clásico:
./mi-programa: error while loading shared libraries: 
  libmislib.so: cannot open shared object file

# Causa: el sistema busca libs en /lib/, /usr/lib/, NO en /usr/local/lib/
```

```bash
# Solución 1: ejecutar ldconfig
sudo ldconfig
# ldconfig lee /etc/ld.so.conf y /etc/ld.so.conf.d/
# Rebuild la caché de librerías del sistema
# /etc/ld.so.conf generalmente incluye /etc/ld.so.conf.d/*.conf
# que a su vez incluyen /usr/local/lib/

# Solución 2: agregar la ruta manualmente
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/local.conf
sudo ldconfig

# Solución 3: usar LD_LIBRARY_PATH (temporal, para testing)
LD_LIBRARY_PATH=/usr/local/lib ./mi-programa

# Verificar que ldconfig conoce tu librería
ldconfig -p | grep mislib
```

```bash
# Cómo funciona ldconfig internamente:
# 1. Lee /etc/ld.so.conf (líneas con rutas de libs)
# 2. Escanea cada directorio buscando .so files
# 3. Construye caché en /etc/ld.so.cache
# 4. El linker dinámico (ld.so) usa la caché para encontrar libs

# El orden de búsqueda de libs:
# 1. LD_LIBRARY_PATH (si está definido)
# 2. -rpath links en el binario (embedded path)
# 3. /etc/ld.so.cache (generado por ldconfig)
# 4. /lib/, /usr/lib/ (hardcoded fallback)
```

## pkg-config: encontrarflags de compilación

```bash
# pkg-config ayuda a encontrar los flags correctos para compilar
# contra una librería

# Sin pkg-config:
gcc -o programa programa.c \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lmislib

# Con pkg-config:
pkg-config --cflags --libs mislib
# -I/usr/local/include -L/usr/local/lib -lmislib

gcc -o programa programa.c $(pkg-config --cflags --libs mislib)

# Muchos configure scripts usan pkg-config internamente
```

## Otros Build Systems

### CMake (C++ moderno)

```bash
# CMake genera Makefiles o proyectos para IDEs
mkdir build && cd build
cmake ..                       # genera Makefile
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc)
sudo make install
```

### Meson + Ninja

```bash
# Usado por GNOME, systemd, Hendrix
meson setup build
cd build
ninja              # compilación muy rápida
sudo ninja install
```

### Cargo (Rust)

```bash
cargo build --release
sudo cp target/release/mi-programa /usr/local/bin/

# O instalar directamente
cargo install mi-programa
# Instala en ~/.cargo/bin/
```

### Go

```bash
go build -o mi-programa .
sudo cp mi-programa /usr/local/bin/
```

### Python

```bash
# Con setuptools
python setup.py build
sudo python setup.py install

# Con pip (recomendado)
pip install .
pip install --user .    # sin sudo, en ~/.local/

# Con poetry
poetry install
```

## Ejemplo completo: compilar jq desde fuente

```bash
# jq es un procesador JSON de línea de comandos
# Fuente: https://github.com/jqlang/jq

# 1. Instalar dependencias de compilación
sudo apt install build-essential autoconf automake libtool git oniguruma-dev
# En RHEL:
# sudo dnf groupinstall "Development Tools"
# sudo dnf install autoconf automake libtool oniguruma-devel git

# 2. Clonar el código
cd /tmp
git clone https://github.com/jqlang/jq.git
cd jq
git checkout jq-1.7                       # versión específica

# 3. Generar configure (porque clonamos desde git)
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

# 8. Verificar instalación
which jq
# /usr/local/bin/jq
jq --version

# 9. Asegurar que el sistema encuentra la librería si jq la usa
sudo ldconfig

# 10. Limpiar fuentes
cd /tmp && rm -rf jq/
```

---

## Ejercicios

### Ejercicio 1 — Compilar GNU Hello

```bash
# GNU Hello es el programa "Hola Mundo" oficial de GNU
# Muy simple, pero demuestra el proceso completo

# 1. Descargar
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz

# 2. Verificar checksum
sha256sum hello-2.12.1.tar.gz
# Compara con https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz.sig

# 3. Extraer
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1

# 4. Explorar
ls
# AUTHORS  configure  Makefile.in  README  src/  ...

# 5. Ver opciones de configure
./configure --help | head -30

# 6. Configurar con prefix temporal
./configure --prefix=/tmp/hello-install

# 7. Compilar
make -j$(nproc)

# 8. Instalar
make install

# 9. Ejecutar
/tmp/hello-install/bin/hello

# 10. Limpiar TODO
cd /tmp && rm -rf hello-2.12.1/ hello-2.12.1.tar.gz hello-install/
```

### Ejercicio 2 — Dependencias faltantes

```bash
# Intentar compilar un proyecto que requiere dependencias

# 1. Descargar un proyecto simple con dependencias
cd /tmp
git clone --depth 1 https://github.com/libucl/libucl.git
cd libucl

# 2. Generar configure
autoreconf -fi 2>&1 | tail -5

# 3. Intentar configurar
./configure 2>&1 | tail -10
# ¿Qué error aparece?

# 4. Instalar la dependencia que falta
sudo apt install autoconf automake libtool   # o similar

# 5. Reintentar configure
./configure

# 6. Limpiar
cd /tmp && rm -rf libucl/
```

### Ejercicio 3 — Explorar ldconfig

```bash
# 1. Ver qué rutas están configuradas para libs del sistema
cat /etc/ld.so.conf
cat /etc/ld.so.conf.d/

# 2. Ver la caché actual
ldconfig -p | head -10
ldconfig -p | wc -l     # cuántas libs conoce el sistema

# 3. Crear una librería "dummy" en /usr/local/lib
# (esto es SIMULADO, no hacer en producción)
echo 'int dummy() { return 42; }' > /tmp/dummy.c
gcc -shared -fPIC -o /tmp/libdummy.so /tmp/dummy.c

# 4. Antes de ldconfig: ¿la encuentra?
ldconfig -p | grep dummy

# 5. Ejecutar ldconfig
sudo ldconfig

# 6. Después de ldconfig: ¿la encuentra?
ldconfig -p | grep dummy

# 7. Limpiar
rm /tmp/dummy.c /tmp/libdummy.so
sudo ldconfig    # regenerar caché
```

### Ejercicio 4 — Comparar build systems

```bash
# Compilar el mismo proyecto con diferentes build systems

# Opción A: CMake (si el proyecto lo soporta)
mkdir project-cmake && cd project-cmake
cmake ..
make -j$(nproc)

# Opción B: Meson (si el proyecto lo soporta)
meson setup project-meson project-meson/
cd project-meson
ninja

# Comparar:
# - Tiempo de compilación
# - Facilidad de uso
# - Limpieza (rm -rf build/ vs ninja -t clean)
```

### Ejercicio 5 — Investigar un Makefile

```bash
# Elegir un proyecto que ya hayas descomprimido

# 1. Leer el Makefile
head -50 Makefile

# 2. ¿Qué targets existen?
grep -E '^[a-zA-Z_-]+:' Makefile | head -20

# 3. ¿Hay target install?
grep -E '^install:' Makefile

# 4. ¿Hay target uninstall?
grep -E '^uninstall:' Makefile

# 5. ¿Cuál es el PREFIX default?
grep 'prefix' Makefile | head -5

# 6. ¿Qué variable controla el prefix?
grep '\${prefix}' Makefile | head -5
```

### Ejercicio 6 — Instalar con prefix aislado y usar alternatives

```bash
# Compilar e instalar con prefix aislado + alternatives

# 1. Compilar un proyecto simple (ej: hello de nuevo)
cd /tmp
wget -q https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1
./configure --prefix=/opt/hello-2.12.1
make -j$(nproc)
sudo make install

# 2. Crear alternativa
sudo update-alternatives --install \
    /usr/bin/hello \
    hello \
    /opt/hello-2.12.1/bin/hello \
    100

# 3. Ver alternativas
sudo update-alternatives --display hello

# 4. Usar
hello

# 5. Desinstalar versión
sudo update-alternatives --remove hello /opt/hello-2.12.1/bin/hello
sudo rm -rf /opt/hello-2.12.1

# 6. Limpiar fuentes
cd /tmp && rm -rf hello-2.12.1/
```
