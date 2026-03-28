# T02 — checkinstall

## El problema que resuelve

`make install` copia archivos al sistema sin dejar registro. No hay forma
limpia de saber qué se instaló ni de desinstalarlo:

```
make install:
  Copia /usr/local/bin/programa      ← ¿quién lo puso ahí?
  Copia /usr/local/lib/libfoo.so     ← ¿cómo lo desinstalo?
  Copia /usr/local/share/man/...     ← ¿qué más se instaló?

  No queda registro en dpkg ni en rpm.
  apt/dnf no saben que este software existe.
```

`checkinstall` intercepta `make install`, captura todos los archivos que se
instalarían, y los empaqueta en un `.deb` (Debian) o `.rpm` (RHEL). Luego
instala el paquete usando dpkg/rpm, así el software queda registrado en el
gestor de paquetes:

```
checkinstall:
  Ejecuta make install en un entorno monitoreado
  Captura todos los archivos que se copian
  Crea un paquete .deb o .rpm con esos archivos
  Instala el paquete via dpkg -i o rpm -i

  Resultado: el software aparece en dpkg -l / rpm -qa
  Se puede desinstalar con apt remove / dnf remove
```

## Instalar checkinstall

```bash
# Debian/Ubuntu
sudo apt install checkinstall

# RHEL/AlmaLinux (puede necesitar EPEL)
sudo dnf install checkinstall
# Si no está disponible, se puede compilar desde fuente (ironía)
```

## Uso básico

En lugar de `sudo make install`, usar `sudo checkinstall`:

```bash
# Flujo normal:
./configure --prefix=/usr/local
make -j$(nproc)

# En lugar de "sudo make install":
sudo checkinstall

# checkinstall pregunta:
# Should I create a default set of package docs? [y]
# Please write a description: Software compilado desde fuente
# (enter para terminar la descripción)
#
# 1 - Summary:       [ Software compilado desde fuente ]
# 2 - Name:          [ software ]
# 3 - Version:       [ 1.0 ]
# 4 - Release:       [ 1 ]
# 5 - License:       [ GPL ]
# 6 - Group:         [ checkinstall ]
# 7 - Architecture:  [ amd64 ]
# 8 - Source:        [ software-1.0 ]
# 9 - URL:           [ ]
# ...
# Enter to accept, number to change
```

### Opciones útiles

```bash
# Especificar tipo de paquete
sudo checkinstall --type=debian    # crear .deb (default en Debian)
sudo checkinstall --type=rpm       # crear .rpm (default en RHEL)

# Especificar nombre y versión sin preguntar
sudo checkinstall -D \
    --pkgname=mi-software \
    --pkgversion=1.0 \
    --pkgrelease=1 \
    --maintainer="admin@example.com" \
    --provides=mi-software \
    --requires="libc6,libpcre3" \
    --install=yes \
    --default

# --default: aceptar todos los defaults sin preguntar
# --install=yes: instalar el paquete después de crearlo
# --install=no: solo crear el .deb/.rpm sin instalar
# -D: crear .deb (Debian)
# -R: crear .rpm (RHEL)
```

## Ejemplo completo

```bash
# Compilar e instalar jq con checkinstall

# 1. Preparar
cd /tmp
git clone --depth 1 --branch jq-1.7.1 https://github.com/jqlang/jq.git
cd jq
autoreconf -fi

# 2. Configurar y compilar
./configure --prefix=/usr/local --disable-docs
make -j$(nproc)

# 3. Crear paquete e instalar con checkinstall
sudo checkinstall -D \
    --pkgname=jq-local \
    --pkgversion=1.7.1 \
    --default

# Resultado:
# **********************************************************************
#  Done. The new package has been installed and saved to
#  /tmp/jq/jq-local_1.7.1-1_amd64.deb
# **********************************************************************

# 4. Verificar que está registrado en dpkg
dpkg -l jq-local
# ii  jq-local  1.7.1-1  amd64  Software compilado desde fuente

# Ver los archivos que instaló
dpkg -L jq-local

# 5. Desinstalar limpiamente
sudo apt remove jq-local
# o
sudo dpkg -r jq-local
```

## Ventajas y limitaciones

### Ventajas

```
✓ El software queda registrado en dpkg/rpm
✓ Se puede desinstalar limpiamente (apt remove / dnf remove)
✓ Se genera un .deb/.rpm que se puede redistribuir
✓ Se puede especificar dependencias (--requires)
✓ No hay que cambiar nada en el build process
```

### Limitaciones

```
✗ No detecta dependencias automáticamente
  (hay que especificarlas con --requires)
✗ El paquete no es tan limpio como uno hecho con debhelper/rpmbuild
  (puede incluir archivos innecesarios)
✗ No gestiona scripts pre/post instalación
✗ Si el software se actualiza, hay que repetir el proceso
✗ checkinstall puede no capturar todos los archivos si make install
  usa métodos no estándar para copiar
✗ No está tan mantenido como antes (pero sigue funcional)
```

## Alternativas a checkinstall

### fpm — Effing Package Management

`fpm` es una herramienta más moderna y flexible para crear paquetes:

```bash
# Instalar fpm (requiere Ruby)
sudo apt install ruby ruby-dev
sudo gem install fpm

# Crear un .deb desde un directorio instalado
# Primero: make install DESTDIR=/tmp/install-root
make install DESTDIR=/tmp/install-root

# Luego: fpm empaqueta ese directorio
fpm -s dir -t deb \
    -n mi-software \
    -v 1.0 \
    --prefix /usr/local \
    -C /tmp/install-root \
    .

# fpm es más potente que checkinstall:
# - Puede crear deb, rpm, pacman, tar, etc.
# - Soporta scripts pre/post
# - Puede convertir entre formatos (deb → rpm)
# - Detecta dependencias de shared libraries
```

### DESTDIR — Instalar en un directorio temporal

Muchos Makefiles soportan `DESTDIR`, que permite instalar en un directorio
temporal sin tocar el sistema:

```bash
# Instalar en un directorio staging
make install DESTDIR=/tmp/staging

# Los archivos quedan en:
# /tmp/staging/usr/local/bin/programa
# /tmp/staging/usr/local/lib/libfoo.so

# Desde ahí se puede empaquetar con checkinstall, fpm, o dpkg-deb
```

### dpkg-deb — Crear .deb manualmente

```bash
# Para paquetes simples, se puede crear un .deb a mano:
mkdir -p /tmp/mi-paquete/DEBIAN
mkdir -p /tmp/mi-paquete/usr/local/bin

# Copiar el binario
cp programa /tmp/mi-paquete/usr/local/bin/

# Crear el archivo de control
cat > /tmp/mi-paquete/DEBIAN/control << 'EOF'
Package: mi-programa
Version: 1.0
Architecture: amd64
Maintainer: admin@example.com
Description: Mi programa compilado
EOF

# Crear el .deb
dpkg-deb --build /tmp/mi-paquete mi-programa_1.0_amd64.deb

# Instalar
sudo dpkg -i mi-programa_1.0_amd64.deb
```

## Cuándo usar cada método

| Método | Complejidad | Cuándo usar |
|---|---|---|
| `make install` directo | Mínima | Pruebas rápidas, directorio aislado (--prefix=/opt/X) |
| `checkinstall` | Baja | Software que quieres gestionar con dpkg/rpm |
| `fpm` | Media | Crear paquetes redistribuibles de calidad |
| `dpkg-deb` manual | Media | Control total sobre el paquete |
| debhelper/rpmbuild | Alta | Paquetes oficiales para distribución |

---

## Ejercicios

### Ejercicio 1 — checkinstall básico

```bash
# Compilar GNU hello con checkinstall
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1

./configure --prefix=/usr/local
make -j$(nproc)

# Instalar con checkinstall
sudo checkinstall -D --pkgname=hello-local --pkgversion=2.12.1 --default

# Verificar
dpkg -l hello-local
hello

# Desinstalar
sudo apt remove hello-local

# Limpiar
rm -rf /tmp/hello-2.12.1 /tmp/hello-2.12.1.tar.gz
```

### Ejercicio 2 — DESTDIR

```bash
# Compilar e instalar en directorio temporal
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz
cd hello-2.12.1

./configure --prefix=/usr/local
make -j$(nproc)

# Instalar en staging
make install DESTDIR=/tmp/hello-staging

# Inspeccionar qué se instaló
find /tmp/hello-staging -type f

# Limpiar
rm -rf /tmp/hello-2.12.1 /tmp/hello-2.12.1.tar.gz /tmp/hello-staging
```

### Ejercicio 3 — Comparar con el paquete del repo

```bash
# Instalar hello desde el repo (si existe)
apt show hello 2>/dev/null && echo "Disponible en repo" || echo "No disponible"

# Si está disponible:
# ¿Qué versión tiene el repo vs la que compilamos?
# ¿Cuántos archivos instala cada uno?
```
