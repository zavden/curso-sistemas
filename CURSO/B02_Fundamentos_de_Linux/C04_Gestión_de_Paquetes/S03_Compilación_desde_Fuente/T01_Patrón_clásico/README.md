# T01 — Patrón clásico: configure, make, make install

## Por qué compilar desde fuente

En la mayoría de casos, instalar desde repositorios (apt/dnf) es la opción
correcta. Compilar desde fuente se justifica cuando:

```
Razones válidas para compilar:
  - El paquete no existe en los repositorios de tu distro
  - Necesitas una versión más nueva que la del repo
  - Necesitas el software compilado con opciones específicas
    (ej: nginx con módulos extra, FFmpeg con codecs específicos)
  - Estás desarrollando o parcheando el software
  - Estás aprendiendo cómo funciona el build system (este curso)

Razones para NO compilar:
  - El paquete existe en el repo con la versión que necesitas
  - No quieres gestionar actualizaciones de seguridad manualmente
  - Es un servidor de producción y prefieres paquetes soportados
```

## El patrón ./configure && make && make install

La gran mayoría del software en C/C++ de código abierto sigue el mismo patrón
de tres pasos, conocido como el **GNU Build System** (autotools):

```bash
# 1. Descargar y extraer
wget https://example.com/software-1.0.tar.gz
tar -xzf software-1.0.tar.gz
cd software-1.0

# 2. Configurar
./configure

# 3. Compilar
make

# 4. Instalar
sudo make install
```

### Paso 1: Descargar y extraer

```bash
# Los formatos comunes de distribución de fuentes:
# .tar.gz  (gzip)
# .tar.bz2 (bzip2)
# .tar.xz  (xz — mejor compresión)

tar -xzf software-1.0.tar.gz    # .tar.gz
tar -xjf software-1.0.tar.bz2   # .tar.bz2
tar -xJf software-1.0.tar.xz    # .tar.xz
tar -xf  software-1.0.tar.xz    # tar auto-detecta la compresión

# Alternativamente, clonar desde git:
git clone https://github.com/project/software.git
cd software
# Puede requerir: autoreconf -fi (para generar ./configure desde configure.ac)
```

### Paso 2: ./configure

`./configure` es un script de shell que:
1. Detecta el sistema operativo y la arquitectura
2. Busca compiladores, bibliotecas y herramientas necesarias
3. Verifica que las dependencias están presentes
4. Genera los `Makefile` con las rutas y opciones correctas

```bash
# Ejecutar configure
./configure
# checking for gcc... gcc
# checking whether the C compiler works... yes
# checking for library headers... yes
# ...
# configure: creating Makefile

# Si falta una dependencia:
# configure: error: pcre2 library not found
# → Instalar el paquete de desarrollo: sudo apt install libpcre2-dev
```

### Opciones comunes de configure

```bash
# Ver todas las opciones disponibles
./configure --help

# Opciones universales:
./configure --prefix=/usr/local          # dónde instalar (default: /usr/local)
./configure --prefix=/opt/software       # instalación aislada

# Habilitar/deshabilitar funcionalidades
./configure --enable-ssl --enable-threads
./configure --disable-docs
./configure --with-pcre=/usr/local/pcre
./configure --without-readline

# Cross-compilation
./configure --host=aarch64-linux-gnu     # compilar para ARM desde x86
```

### --prefix: dónde se instala

```
--prefix=/usr/local (default):
  Binarios:        /usr/local/bin/
  Bibliotecas:     /usr/local/lib/
  Headers:         /usr/local/include/
  Configuración:   /usr/local/etc/
  Man pages:       /usr/local/share/man/

--prefix=/usr:
  Se mezcla con los paquetes del sistema — EVITAR
  Puede romper paquetes gestionados por apt/dnf

--prefix=/opt/software-1.0:
  Todo en un directorio aislado — fácil de desinstalar (rm -rf)
  Pero hay que agregar /opt/software-1.0/bin al PATH
```

La convención es `/usr/local` para software compilado manualmente. Los gestores
de paquetes instalan en `/usr`. Así no se pisan.

### Paso 3: make

```bash
# Compilar
make

# Compilar usando múltiples cores (mucho más rápido)
make -j$(nproc)
# nproc = número de CPUs disponibles
# make -j4  = 4 compilaciones en paralelo

# Solo compilar sin instalar (verificar que compila)
make -j$(nproc)

# Si falla:
# error: 'function_name' was not declared in this scope
# → Error de compilación — puede ser una dependencia faltante,
#   versión del compilador incompatible, o bug en el código
```

### Paso 4: make install

```bash
# Instalar (copiar archivos compilados a las rutas del prefix)
sudo make install
# install -m 755 software /usr/local/bin/software
# install -m 644 software.1 /usr/local/share/man/man1/software.1
# install -m 644 libsoftware.so /usr/local/lib/libsoftware.so

# Si usaste --prefix=/usr/local, necesitas sudo
# Si usaste --prefix=$HOME/software, no necesitas sudo
```

### El problema de make install

```
make install NO:
  - Registra qué archivos instaló (no hay manifiesto)
  - Gestiona dependencias
  - Permite desinstalar limpiamente (a menos que el Makefile lo soporte)
  - Se actualiza automáticamente

Para desinstalar:
  make uninstall    ← SOLO si el Makefile lo implementa (muchos no lo hacen)

  Si no hay make uninstall:
  - No hay forma limpia de saber qué archivos se instalaron
  - Hay que eliminar manualmente
  - O haber usado --prefix=/opt/X para poder hacer rm -rf /opt/X
```

Este problema es exactamente lo que `checkinstall` resuelve (siguiente tópico).

## Ejemplo completo: compilar jq

```bash
# jq es un procesador JSON de línea de comandos
# Ejemplo real de compilar desde fuente

# 1. Instalar dependencias de compilación
sudo apt install build-essential autoconf automake libtool git
# o en RHEL:
# sudo dnf groupinstall "Development Tools"
# sudo dnf install autoconf automake libtool git

# 2. Clonar el código fuente
git clone https://github.com/jqlang/jq.git
cd jq
git checkout jq-1.7.1     # versión específica

# 3. Generar configure (necesario cuando se clona desde git)
autoreconf -fi

# 4. Configurar
./configure --prefix=/usr/local --disable-docs

# 5. Compilar
make -j$(nproc)

# 6. Verificar que funciona (sin instalar)
./jq --version
echo '{"name":"test"}' | ./jq '.name'

# 7. Instalar
sudo make install

# 8. Verificar
which jq
# /usr/local/bin/jq
jq --version
```

## Otros build systems

No todo usa autotools. Otros build systems comunes:

### CMake

```bash
# Muchos proyectos C/C++ modernos usan CMake
mkdir build && cd build
cmake ..                    # genera Makefiles
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..   # con prefix
make -j$(nproc)
sudo make install
```

### Meson + Ninja

```bash
# Usado por GNOME, systemd, y otros proyectos modernos
meson setup build
cd build
ninja                       # equivalente a make
sudo ninja install
```

### Makefile directo (sin configure)

```bash
# Algunos proyectos simples solo tienen un Makefile
make
sudo make install PREFIX=/usr/local
```

### Cargo (Rust), Go, Python, etc.

```bash
# Rust
cargo build --release
sudo cp target/release/programa /usr/local/bin/

# Go
go build -o programa
sudo cp programa /usr/local/bin/

# Python
pip install .
# o
python setup.py install
```

## Actualizar las bibliotecas compartidas

Si se instalaron bibliotecas en `/usr/local/lib/`, el sistema necesita saber
que existen:

```bash
# Regenerar la caché de bibliotecas compartidas
sudo ldconfig

# Verificar que la biblioteca se encuentra
ldconfig -p | grep libsoftware

# Si el programa dice "error while loading shared libraries":
# libsoftware.so: cannot open shared object file
# → Ejecutar sudo ldconfig
# → O agregar la ruta a /etc/ld.so.conf.d/
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/local.conf
sudo ldconfig
```

---

## Ejercicios

### Ejercicio 1 — Explorar el proceso

```bash
# Descargar un programa pequeño (GNU hello)
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1

# Explorar
ls
# configure  Makefile.in  src/  ...

# Ver opciones de configure
./configure --help | head -30

# Configurar con prefix temporal
./configure --prefix=/tmp/hello-install

# Compilar
make -j$(nproc)

# Instalar (no necesita sudo porque el prefix es en /tmp)
make install

# Verificar
/tmp/hello-install/bin/hello

# Limpiar
rm -rf /tmp/hello-2.12.1 /tmp/hello-2.12.1.tar.gz /tmp/hello-install
```

### Ejercicio 2 — Cuando falta una dependencia

```bash
# Intentar configurar sin dependencias
# (este ejercicio puede fallar — esa es la intención)
cd /tmp
git clone --depth 1 https://github.com/jqlang/jq.git
cd jq
autoreconf -fi 2>/dev/null
./configure 2>&1 | tail -5
# Si falla: ¿qué dependencia falta?
# Leer el mensaje de error e instalar lo necesario

rm -rf /tmp/jq
```

### Ejercicio 3 — Inspeccionar un Makefile

```bash
# Si tienes un proyecto con Makefile, inspeccionar:
# ¿Tiene target 'install'?
# ¿Tiene target 'uninstall'?
# ¿Cuál es el PREFIX default?

# Ejemplo con GNU hello (si lo descargaste):
grep -E '^install:|^uninstall:|prefix' /tmp/hello-2.12.1/Makefile 2>/dev/null | head -5
```
