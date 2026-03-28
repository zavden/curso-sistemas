# Lab — Patron clasico: configure, make, make install

## Objetivo

Entender el patron clasico de compilacion desde fuente en Linux:
que hace cada paso (configure, make, make install), como controlar
el destino con --prefix, y como se compara con otros build systems.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — El proceso completo

### Objetivo

Entender cada paso del patron ./configure && make && make install.

### Paso 1.1: Por que compilar desde fuente

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando compilar desde fuente ==="
echo ""
echo "Razones validas:"
echo "  - El paquete NO existe en los repos de tu distro"
echo "  - Necesitas una version mas nueva que la del repo"
echo "  - Necesitas opciones de compilacion especificas"
echo "    (ej: nginx con modulos extra)"
echo "  - Estas desarrollando o parcheando el software"
echo ""
echo "Razones para NO compilar:"
echo "  - El paquete existe en el repo con la version correcta"
echo "  - No quieres gestionar actualizaciones manualmente"
echo "  - Es un servidor de produccion"
echo ""
echo "=== El patron de 3 pasos ==="
echo ""
echo "  ./configure     ← detectar sistema, verificar deps"
echo "  make            ← compilar"
echo "  sudo make install ← copiar archivos al sistema"
'
```

### Paso 1.2: configure — detectar y generar

```bash
docker compose exec debian-dev bash -c '
echo "=== ./configure ==="
echo ""
echo "Es un script de shell que:"
echo "  1. Detecta el sistema operativo y arquitectura"
echo "  2. Busca compiladores (gcc, g++)"
echo "  3. Busca bibliotecas y headers necesarios"
echo "  4. Verifica que las dependencias estan presentes"
echo "  5. Genera los Makefile con rutas y opciones correctas"
echo ""
echo "Si falta una dependencia:"
echo "  configure: error: pcre2 library not found"
echo "  → Instalar: sudo apt install libpcre2-dev"
echo ""
echo "--- Ver opciones de configure ---"
echo "  ./configure --help"
echo ""
echo "--- Opciones universales ---"
echo "  --prefix=/usr/local        destino (default)"
echo "  --enable-feature           habilitar algo"
echo "  --disable-docs             deshabilitar algo"
echo "  --with-lib=/path           ruta a una libreria"
echo "  --without-readline         excluir una libreria"
'
```

### Paso 1.3: make — compilar

```bash
docker compose exec debian-dev bash -c '
echo "=== make ==="
echo ""
echo "Lee el Makefile generado por configure y compila:"
echo ""
echo "  make           ← un core (lento)"
echo "  make -j\$(nproc) ← todos los cores (rapido)"
echo ""
echo "nproc en este sistema: $(nproc)"
echo "  → make -j$(nproc) usaria $(nproc) compilaciones en paralelo"
echo ""
echo "Si falla la compilacion:"
echo "  Puede ser una dependencia faltante"
echo "  Puede ser version del compilador incompatible"
echo "  Leer el error: suele indicar que header o funcion falta"
echo ""
echo "=== make install ==="
echo ""
echo "Copia los archivos compilados a las rutas del prefix:"
echo "  /usr/local/bin/programa"
echo "  /usr/local/lib/libfoo.so"
echo "  /usr/local/share/man/..."
echo ""
echo "PROBLEMA: make install NO registra que archivos instalo"
echo "  No hay manifiesto, no hay desinstalacion limpia"
echo "  → checkinstall resuelve esto (siguiente topico)"
'
```

### Paso 1.4: Formatos de fuentes

```bash
docker compose exec debian-dev bash -c '
echo "=== Formatos de distribucion de codigo fuente ==="
echo ""
echo "tar -xzf software.tar.gz    → gzip"
echo "tar -xjf software.tar.bz2   → bzip2"
echo "tar -xJf software.tar.xz    → xz (mejor compresion)"
echo "tar -xf  software.tar.xz    → auto-detecta"
echo ""
echo "O clonar desde git:"
echo "  git clone https://github.com/project/software.git"
echo "  cd software"
echo "  autoreconf -fi   ← generar ./configure desde configure.ac"
echo ""
echo "=== Herramientas necesarias ==="
echo ""
echo "Minimo: gcc, make"
echo "Autotools: autoconf, automake, libtool"
echo ""
echo "--- Verificar disponibilidad ---"
gcc --version 2>/dev/null | head -1 || echo "gcc: NO instalado"
make --version 2>/dev/null | head -1 || echo "make: NO instalado"
autoconf --version 2>/dev/null | head -1 || echo "autoconf: NO instalado"
'
```

---

## Parte 2 — --prefix y compilacion paralela

### Objetivo

Controlar donde se instala el software y optimizar la compilacion.

### Paso 2.1: --prefix

```bash
docker compose exec debian-dev bash -c '
echo "=== --prefix: donde se instala ==="
echo ""
echo "--- --prefix=/usr/local (default) ---"
echo "  Binarios:     /usr/local/bin/"
echo "  Bibliotecas:  /usr/local/lib/"
echo "  Headers:      /usr/local/include/"
echo "  Config:       /usr/local/etc/"
echo "  Man pages:    /usr/local/share/man/"
echo ""
echo "--- --prefix=/usr ---"
echo "  SE MEZCLA con paquetes del sistema — EVITAR"
echo "  Puede romper paquetes de apt/dnf"
echo ""
echo "--- --prefix=/opt/software-1.0 ---"
echo "  Todo en un directorio aislado"
echo "  Facil de desinstalar: rm -rf /opt/software-1.0"
echo "  Pero: agregar /opt/software-1.0/bin al PATH"
echo ""
echo "=== Convencion ==="
echo "/usr         → paquetes del gestor (apt/dnf)"
echo "/usr/local   → software compilado manualmente"
echo "/opt/X       → software aislado (por aplicacion)"
echo ""
echo "--- Verificar que hay en /usr/local/bin ---"
ls /usr/local/bin/ 2>/dev/null | head -5 || echo "(vacio)"
'
```

### Paso 2.2: make -j para compilacion paralela

```bash
docker compose exec debian-dev bash -c '
echo "=== Compilacion en paralelo ==="
echo ""
echo "make -j\$(nproc): usar todos los cores"
echo ""
echo "CPUs disponibles: $(nproc)"
echo ""
echo "Impacto en velocidad (ejemplo tipico):"
echo "  make        → 10 minutos"
echo "  make -j2    → 5 minutos"
echo "  make -j4    → 3 minutos"
echo "  make -j\$(nproc) → lo mas rapido posible"
echo ""
echo "Regla practica: make -j\$(nproc) en builds locales"
echo "En CI/CD: a veces se limita para no saturar el runner"
'
```

### Paso 2.3: make uninstall

```bash
docker compose exec debian-dev bash -c '
echo "=== Desinstalar software compilado ==="
echo ""
echo "--- make uninstall ---"
echo "SOLO funciona si el Makefile lo implementa"
echo "Muchos proyectos NO lo implementan"
echo ""
echo "--- Si no hay uninstall ---"
echo "1. No hay forma limpia de saber que se instalo"
echo "2. Eliminar manualmente los archivos"
echo "3. O haber usado --prefix=/opt/X para rm -rf"
echo ""
echo "--- Solucion ---"
echo "Usar checkinstall en lugar de make install"
echo "→ Crea un .deb/.rpm que registra los archivos"
echo "→ Se desinstala con apt remove / dnf remove"
'
```

---

## Parte 3 — Otros build systems y ldconfig

### Objetivo

Conocer alternativas a autotools y entender ldconfig.

### Paso 3.1: CMake

```bash
docker compose exec debian-dev bash -c '
echo "=== CMake ==="
echo ""
echo "Build system moderno para C/C++"
echo "Usado por: LLVM, KDE, muchos proyectos modernos"
echo ""
echo "Patron:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  cmake -DCMAKE_INSTALL_PREFIX=/usr/local .."
echo "  make -j\$(nproc)"
echo "  sudo make install"
echo ""
echo "Diferencia con autotools:"
echo "  autotools: ./configure genera Makefiles"
echo "  CMake: cmake genera Makefiles (u otros: Ninja)"
echo ""
cmake --version 2>/dev/null || echo "cmake: NO instalado"
'
```

### Paso 3.2: Meson + Ninja

```bash
docker compose exec debian-dev bash -c '
echo "=== Meson + Ninja ==="
echo ""
echo "Build system moderno, mas rapido que autotools y CMake"
echo "Usado por: GNOME, systemd, PipeWire"
echo ""
echo "Patron:"
echo "  meson setup build"
echo "  cd build"
echo "  ninja           ← equivalente a make"
echo "  sudo ninja install"
echo ""
meson --version 2>/dev/null || echo "meson: NO instalado"
ninja --version 2>/dev/null || echo "ninja: NO instalado"
'
```

### Paso 3.3: Makefile directo y otros

```bash
docker compose exec debian-dev bash -c '
echo "=== Otros patrones ==="
echo ""
echo "--- Makefile directo (sin configure) ---"
echo "  make"
echo "  sudo make install PREFIX=/usr/local"
echo "  (proyectos simples, sin deteccion de sistema)"
echo ""
echo "--- Cargo (Rust) ---"
echo "  cargo build --release"
echo "  sudo cp target/release/programa /usr/local/bin/"
echo ""
echo "--- Go ---"
echo "  go build -o programa"
echo "  sudo cp programa /usr/local/bin/"
echo ""
echo "--- Python ---"
echo "  pip install ."
echo "  python setup.py install"
echo ""
echo "Lenguajes como Rust y Go producen binarios estaticos"
echo "— no necesitan librerias del sistema en runtime"
'
```

### Paso 3.4: ldconfig

```bash
docker compose exec debian-dev bash -c '
echo "=== ldconfig: bibliotecas compartidas ==="
echo ""
echo "Si se instalan bibliotecas en /usr/local/lib/,"
echo "el sistema necesita saber que existen:"
echo ""
echo "  sudo ldconfig"
echo ""
echo "Verificar que una libreria se encuentra:"
echo "  ldconfig -p | grep libfoo"
echo ""
echo "Si un programa dice:"
echo "  error while loading shared libraries: libfoo.so"
echo ""
echo "Solucion:"
echo "  echo \"/usr/local/lib\" | sudo tee /etc/ld.so.conf.d/local.conf"
echo "  sudo ldconfig"
echo ""
echo "--- Librerias en /usr/local/lib ---"
ls /usr/local/lib/*.so* 2>/dev/null | head -5 || echo "(ninguna)"
echo ""
echo "--- Directorios en ld.so.conf ---"
cat /etc/ld.so.conf 2>/dev/null
ls /etc/ld.so.conf.d/ 2>/dev/null
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. El patron clasico es **./configure && make && make install**.
   `configure` detecta el sistema y genera Makefiles; `make`
   compila; `make install` copia archivos.

2. `--prefix` controla el destino: `/usr/local` (default) para
   software manual, `/opt/X` para instalacion aislada. **Nunca**
   usar `--prefix=/usr` — pisa paquetes del gestor.

3. `make -j$(nproc)` compila en paralelo usando todos los cores,
   reduciendo significativamente el tiempo de compilacion.

4. `make install` **no registra** que archivos instalo. No hay
   forma limpia de desinstalar a menos que exista `make uninstall`
   o se haya usado `checkinstall`.

5. Otros build systems: **CMake** (mkdir build, cmake, make),
   **Meson+Ninja** (meson setup, ninja), **Makefile directo**
   (make, make install PREFIX=).

6. Si se instalan bibliotecas compartidas, ejecutar `sudo ldconfig`
   para que el sistema las encuentre. Agregar rutas no estandar
   a `/etc/ld.so.conf.d/`.
