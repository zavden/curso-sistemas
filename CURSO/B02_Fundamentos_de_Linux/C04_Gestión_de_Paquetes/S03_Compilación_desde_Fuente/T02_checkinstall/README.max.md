# T02 — checkinstall

## El problema que resuelve

```
make install — el problema:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   sudo make install                                         │
│       │                                                    │
│       ├── cp programa /usr/local/bin/                      │
│       ├── cp libfoo.so /usr/local/lib/                     │
│       ├── cp docs/*.1 /usr/local/share/man/               │
│       │                                                    │
│       └── EL SISTEMA NO SABE QUE EXISTE                    │
│                                                             │
│   Resultado:                                                │
│   - No queda registrado en dpkg ni rpm                    │
│   - No se puede desinstalar limpiamente                   │
│   - No aparece en apt/dnf                                  │
│   - No se actualiza con apt/dnf upgrade                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘

checkinstall — la solución:
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   sudo checkinstall                                         │
│       │                                                    │
│       ├── Ejecuta make install (monitoreado)               │
│       ├── Captura TODOS los archivos copiados              │
│       ├── Empaqueta en .deb (Debian) o .rpm (RHEL)        │
│       ├── Instala via dpkg -i o rpm -i                    │
│       │                                                    │
│       └── EL SISTEMA SÍ SABE QUE EXISTE                   │
│                                                             │
│   Resultado:                                                │
│   - Aparece en dpkg -l / rpm -qa                         │
│   - Se desinstala con apt remove / dnf remove             │
│   - Se genera un .deb/.rpm reutilizable                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Instalar checkinstall

```bash
# Debian/Ubuntu
sudo apt install checkinstall

# RHEL/AlmaLinux (en EPEL)
sudo dnf install epel-release
sudo dnf install checkinstall

# Fedora
sudo dnf install checkinstall
```

## Uso básico

Reemplazar `sudo make install` por `sudo checkinstall`:

```bash
# Flujo completo
./configure --prefix=/usr/local
make -j$(nproc)

# En lugar de: sudo make install
sudo checkinstall

# checkinstall pregunta interactivamente:
#
# Please write a description for your package.
# (ENTER for blank)
# >>> Software compilado desde fuente
#
# 1 -  Summary:  [Software compilado desde fuente]
# 2 -  Name:     [programa]
# 3 -  Version:  [1.0]
# 4 -  Release:  [1]
# 5 -  License:  [GPL]
# 6 -  Group:    [checkinstall]
# 7 -  Architecture: [amd64]
# 8 -  Source:   [programa-1.0]
# ...
# [ENTER] to accept and continue
```

### Opciones útiles

```bash
# Crear .deb (Debian/Ubuntu)
sudo checkinstall -D --default

# Crear .rpm (RHEL/AlmaLinux)
sudo checkinstall -R --default

# Especificar todo sin preguntar
sudo checkinstall -D \
    --pkgname=jq-local \
    --pkgversion=1.7.1 \
    --pkgrelease=1 \
    --maintainer="admin@company.com" \
    --provides=jq \
    --requires="libc6" \
    --install=yes \
    --default
```

```bash
# Flags importantes:
# -D  → crear .deb (Debian)
# -R  → crear .rpm (RHEL)
# -S  → crear .tar.gz (Slackware)
# --default → aceptar defaults y no preguntar
# --install=yes → instalar el paquete después de crearlo
# --install=no → solo crear el paquete, no instalar
# --fstrans=yes → usar filesystem transiction (más seguro, default)
# --fstrans=no → sin transacción (necesario en algunos filesystems)
```

## Ejemplo completo: jq con checkinstall

```bash
# 1. Preparar
cd /tmp
git clone --depth 1 --branch jq-1.7.1 https://github.com/jqlang/jq.git
cd jq

# 2. Dependencias
sudo apt install build-essential autoconf automake libtool oniguruma-dev
# En RHEL:
# sudo dnf groupinstall "Development Tools"
# sudo dnf install autoconf automake libtool oniguruma-devel

# 3. Configurar y compilar
autoreconf -fi
./configure --prefix=/usr/local --disable-docs
make -j$(nproc)

# 4. Crear paquete e instalar
sudo checkinstall -D \
    --pkgname=jq-local \
    --pkgversion=1.7.1 \
    --pkgrelease=1 \
    --maintainer="admin@local" \
    --provides=jq \
    --requires="libc6,libonig5" \
    --default

# 5. Ver resultado
# **********************************************************************
#  Done. The new package has been installed and saved to
#  /tmp/jq/jq-local_1.7.1-1_amd64.deb
# **********************************************************************

# 6. Verificar que está registrado
dpkg -l jq-local
# ii  jq-local  1.7.1-1  amd64  Software compilado desde fuente

# 7. Ver archivos que instaló
dpkg -L jq-local
# /usr/local/bin/jq
# /usr/local/share/man/man1/jq.1
# ...

# 8. Desinstalar limpiamente
sudo apt remove jq-local
# o: sudo dpkg -r jq-local

# 9. Limpiar fuentes
cd /tmp && rm -rf jq/
```

## Ventajas sobre make install directo

```
┌──────────────────────────────────────────────────────────────┐
│  checkinstall vs make install                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  make install:                                               │
│  ✗ No queda registrado en el gestor de paquetes             │
│  ✗ No se puede desinstalar limpiamente                      │
│  ✗ No aparece en dpkg -l / rpm -qa                         │
│  ✗ No se actualiza con apt/dnf upgrade                     │
│  ✗ Genera archivos huérfanos si se elimina mal             │
│                                                              │
│  checkinstall:                                               │
│  ✓ Queda registrado en dpkg/rpm                            │
│  ✓ Se desinstala con apt remove / dnf remove              │
│  ✓ Aparece en dpkg -l / rpm -qa                            │
│  ✓ Genera un .deb/.rpm reutilizable                        │
│  ✓ Especificable con dependencias                          │
│  ✓ No requiere cambiar el build process                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Limitaciones

```
Limitaciones de checkinstall:

✗ No detecta dependencias automáticamente
  → Solución: --requires (manual)

✗ El paquete no es "oficial" (no tan limpio como debhelper)
  → Puede incluir archivos innecesarios
  → Los scripts pre/postinst son básicos

✗ No soporta todos los Makefiles
  → Algunos make install usan métodos no estándar
  → checkinstall puede no capturar todos los archivos

✗ Proyecto poco mantenido actualmente
  → Funciona bien para casos simples
  → Para casos complejos: fpm o debhelper

✗ En RHEL, puede tener problemas con rpmbuild internamente
  → A veces genera warnings en la creación del .rpm
```

## Alternativas a checkinstall

### fpm — Effing Package Management

```bash
# fpm es más potente y flexible que checkinstall
# Instalar
sudo apt install ruby ruby-dev
sudo gem install fpm

# O en RHEL:
# sudo dnf install ruby ruby-devel
# sudo gem install fpm
```

```bash
# fpm permite crear paquetes desde:
# - Directorio (DESTDIR)
# - Archivo .deb existente
# - Archivo .rpm existente
# - Gem, pip, etc.

# Crear .deb desde un directorio DESTDIR:
make install DESTDIR=/tmp/staging
fpm -s dir -t deb \
    -n mi-software \
    -v 1.0 \
    -C /tmp/staging \
    --prefix /usr/local \
    .

# fpm ventajas sobre checkinstall:
# - Más formatos: deb, rpm, tar, pacman, etc.
# - Conversión entre formatos (deb → rpm)
# - Detecta dependencias de shared libraries automáticamente
# - Soporta scripts pre/post install
# - Mejor manejo de symlinks
```

### DESTDIR — Instalación en directorio staging

```bash
# DESTDIR es una variable que soportan muchos Makefiles
# Instala en un directorio temporal sin tocar el sistema

make install DESTDIR=/tmp/staging

# Los archivos van a:
# /tmp/staging/usr/local/bin/programa
# /tmp/staging/usr/local/lib/libfoo.so
# /tmp/staging/usr/local/share/man/...

# Luego se puede:
# 1. Empaquetar con fpm
# 2. Empaquetar con checkinstall (desde el staging)
# 3. Crear .deb a mano con dpkg-deb
```

### dpkg-deb — Crear .deb manualmente

```bash
# Para paquetes simples, se puede crear un .deb directo
# sin herramientas adicionales

# 1. Crear estructura de directorios
mkdir -p /tmp/mi-paquete/DEBIAN
mkdir -p /tmp/mi-paquete/usr/local/bin
mkdir -p /tmp/mi-paquete/usr/local/share/man/man1

# 2. Copiar archivos
cp programa /tmp/mi-paquete/usr/local/bin/
cp programa.1 /tmp/mi-paquete/usr/local/share/man/man1/

# 3. Crear control
cat > /tmp/mi-paquete/DEBIAN/control << 'EOF'
Package: mi-programa
Version: 1.0
Architecture: amd64
Maintainer: admin@example.com
Description: Mi programa compilado desde fuente
 Instalar como: ./configure && make && make install
EOF

# 4. Agregar información de dependencias si hay
cat > /tmp/mi-paquete/DEBIAN/control << 'EOF'
Package: mi-programa
Version: 1.0
Architecture: amd64
Maintainer: admin@example.com
Depends: libc6, libpcre2-8-0
Description: Mi programa compilado desde fuente
EOF

# 5. Crear el .deb
dpkg-deb --build /tmp/mi-paquete .
# Genera: mi-programa_1.0_amd64.deb

# 6. Instalar
sudo dpkg -i mi-programa_1.0_amd64.deb
```

### alien — Convertir entre formatos

```bash
# alien convierte paquetes entre .deb, .rpm, .tgz, etc.
sudo apt install alien

# .rpm → .deb
sudo alien -d package.rpm

# .deb → .rpm
sudo alien -r package.deb

# ⚠️ No recomendado para paquetes críticos del sistema
# Útil para paquetes de terceros que solo existen en otro formato
```

## Cuándo usar cada método

| Método | Complejidad | Desinstalable | Cuándo usarlo |
|---|---|---|---|
| `make install` directo | Mínima | ❌ | Pruebas rápidas, prefix=/tmp o /opt |
| `checkinstall` | Baja | ✅ | Software propio que quieres gestionar |
| `fpm` | Media | ✅ | Paquetes para distribución interna |
| `dpkg-deb` manual | Media | ✅ | Control total, paquetes simples |
| `debhelper`/`rpmbuild` | Alta | ✅ | Paquetes oficiales para distribución pública |

## Solución de problemas

### Error: "checkinstall not found" o problemas dePATH

```bash
# Verificar que está instalado
which checkinstall
dpkg -L checkinstall | grep bin

# Si no está:
sudo apt install checkinstall
```

### Error: "RPM package creation failed"

```bash
# En sistemas Debian, crear .rpm puede fallar
# Usar -D (crear .deb) en Debian
# Usar -R (crear .rpm) en RHEL

# Si en RHEL falla:
# 1. Verificar que rpmbuild está instalado
rpm -q rpmbuild

# 2. Instalar herramientas de build de rpm
sudo dnf install rpm-build
```

### checkinstall no captura todos los archivos

```bash
# Causa: make install usa comandos no estándar (rsync, git, etc.)

# Solución 1: usar fpm en lugar de checkinstall
make install DESTDIR=/tmp/staging
fpm -s dir -t deb -n mi-software -v 1.0 -C /tmp/staging --prefix /usr/local .

# Solución 2: hacer strace del make install
sudo strace -f -e open,openat,execve make install 2>&1 | grep "O_CREAT|O_WRONLY"
```

### Problemas con --requires

```bash
# checkinstall no detecta dependencias de librerías automáticamente
# Pero sí puedes especificarlas

# Para shared libraries (.so):
# Ejecutar ldd sobre el binario para ver qué libs necesita
ldd /usr/local/bin/jq
# linux-vdso.so.1 (0x00007ffe...)
# libonig.so.5 => /lib/x86_64-linux-gnu/libonig.so.5...
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6...

# En Debian, los nombres de paquete son:
# libonig-dev → Provee libonig.so.5
# libc6-dev → Proveer libc.so.6

# Especificar en checkinstall:
sudo checkinstall --requires="libc6,libonig5"
```

---

## Ejercicios

### Ejercicio 1 — Compilar e instalar con checkinstall

```bash
# 1. Descargar GNU hello
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1

# 2. Configurar y compilar
./configure --prefix=/usr/local
make -j$(nproc)

# 3. Instalar con checkinstall
sudo checkinstall -D \
    --pkgname=hello-local \
    --pkgversion=2.12.1 \
    --pkgrelease=1 \
    --maintainer="tu@email.com" \
    --provides=hello \
    --default

# 4. Verificar que está registrado
dpkg -l hello-local

# 5. Ver archivos que instaló
dpkg -L hello-local

# 6. Ejecutar
hello

# 7. Ver qué passwd tiene hello
which hello
ldd $(which hello)

# 8. Desinstalar
sudo apt remove hello-local

# 9. Limpiar
cd /tmp && rm -rf hello-2.12.1/ hello-2.12.1.tar.gz
```

### Ejercicio 2 — DESTDIR + fpm

```bash
# Comparar: DESTDIR con fpm

# 1. Compilar
cd /tmp
wget -q https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1
./configure --prefix=/usr/local
make -j$(nproc)

# 2. Instalar en staging directory
make install DESTDIR=/tmp/hello-staging

# 3. Ver qué hay en el staging
find /tmp/hello-staging -type f | head -20

# 4. Crear .deb con fpm
which fpm || sudo apt install ruby ruby-dev && sudo gem install fpm
fpm -s dir -t deb \
    -n hello-fpm \
    -v 2.12.1 \
    -C /tmp/hello-staging \
    --prefix /usr/local \
    .

# 5. Instalar el .deb generado
sudo dpkg -i hello-fpm_2.12.1_amd64.deb

# 6. Verificar
dpkg -l hello-fpm
hello

# 7. Desinstalar
sudo apt remove hello-fpm

# 8. Limpiar
cd /tmp && rm -rf hello-2.12.1/ hello-2.12.1.tar.gz hello-staging/ hello-fpm_2.12.1_amd64.deb
```

### Ejercicio 3 — Investigar dependencias de un binario

```bash
# Elegir un binario compilado (ej: hello-local del ejercicio anterior)

# 1. Instalar con checkinstall (del ejercicio 1 si no lo hiciste)

# 2. Ver las dependencias con ldd
ldd /usr/local/bin/hello
# linux-vdso.so.1 (0x...)
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)
# libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x...)

# 3. Traducir libs a nombres de paquete Debian
# libc.so.6 → libc6-dev
# libdl.so.2 → libc6-dev
# libpcre.so.3 → libpcre3-dev

# 4. Ver qué dependencias detecta checkinstall vs ldd
```

### Ejercicio 4 — Comparar paquetes: repo vs checkinstall

```bash
# 1. Instalar versión del repo de un paquete (ej: htop)
sudo apt install -y htop

# 2. Ver información del paquete del repo
dpkg -l htop
dpkg -L htop | head -10

# 3. Ahora desinstalar
sudo apt remove htop

# 4. Compilar htop desde fuente (si hay tiempo) y usar checkinstall
# o comparar: tamaño, archivos, diferencias

# 5. ¿Qué diferencias notas entre el .deb del repo y el de checkinstall?
```

### Ejercicio 5 — Solución de problemas

```bash
# Simular un error común: dependencias faltantes

# 1. Compilar un proyecto
cd /tmp
git clone --depth 1 https://github.com/libucl/libucl.git
cd libucl
autoreconf -fi

# 2. Intentar checkinstall sin configurar
# Esto probablemente falle porque falta algo
# ¿Qué error aparece?

# 3. Instalar la dependencia que falta
sudo apt install autoconf automake libtool

# 4. Reintentar
./configure
make -j$(nproc)

# 5. Limpiar
cd /tmp && rm -rf libucl/
```
