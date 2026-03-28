# T02 — checkinstall

## Errata y mejoras sobre el material base

1. **`filesystem transiction`** → `filesystem transaction` (typo en `--fstrans`).

2. **`problemas dePATH`** → `problemas de PATH` (espacio faltante).

3. **Ejemplo dpkg-deb crea `DEBIAN/control` dos veces** — El segundo `cat >`
   sobrescribe el primero. Consolidado en un solo ejemplo.

4. **`rpm -q rpmbuild`** → `rpm -q rpm-build`. `rpmbuild` es el comando;
   `rpm-build` es el nombre del paquete que lo contiene.

5. **`libonig-dev → Provee libonig.so.5`** — Incorrecto. `libonig-dev` provee
   los headers (`.h`) y el symlink `.so` para compilar. El paquete que provee
   la librería runtime `libonig.so.5` es `libonig5`. Para `--requires` de
   checkinstall se necesita el paquete runtime, no el `-dev`.

6. **"checkinstall no gestiona scripts pre/post instalación"** — Impreciso.
   checkinstall soporta `--preinstall`, `--postinstall`, `--preremove`,
   `--postremove` para incluir scripts. Lo que no hace es generarlos
   automáticamente.

---

## El problema que resuelve

```
make install — el problema:
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   sudo make install                                      │
│       │                                                  │
│       ├── cp programa /usr/local/bin/                    │
│       ├── cp libfoo.so /usr/local/lib/                  │
│       ├── cp docs/*.1 /usr/local/share/man/             │
│       │                                                  │
│       └── EL SISTEMA NO SABE QUE EXISTE                  │
│                                                          │
│   - No queda registrado en dpkg ni rpm                   │
│   - No se puede desinstalar limpiamente                  │
│   - No aparece en apt/dnf                                │
│   - No se actualiza con apt/dnf upgrade                  │
│                                                          │
└──────────────────────────────────────────────────────────┘

checkinstall — la solución:
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   sudo checkinstall                                      │
│       │                                                  │
│       ├── Ejecuta make install (monitoreado)             │
│       ├── Captura TODOS los archivos copiados            │
│       ├── Empaqueta en .deb (Debian) o .rpm (RHEL)      │
│       ├── Instala vía dpkg -i o rpm -i                  │
│       │                                                  │
│       └── EL SISTEMA SÍ SABE QUE EXISTE                 │
│                                                          │
│   - Aparece en dpkg -l / rpm -qa                         │
│   - Se desinstala con apt remove / dnf remove            │
│   - Se genera un .deb/.rpm reutilizable                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## Instalar checkinstall

```bash
# Debian/Ubuntu
sudo apt install checkinstall

# RHEL/AlmaLinux (requiere EPEL)
sudo dnf install epel-release
sudo dnf install checkinstall

# Fedora
sudo dnf install checkinstall
```

---

## Uso básico

Reemplazar `sudo make install` por `sudo checkinstall`:

```bash
# Flujo completo
./configure --prefix=/usr/local
make -j$(nproc)

# En lugar de: sudo make install
sudo checkinstall

# checkinstall pregunta interactivamente:
# Please write a description for your package.
# >>> Software compilado desde fuente
#
# 1 - Summary:      [Software compilado desde fuente]
# 2 - Name:         [programa]
# 3 - Version:      [1.0]
# 4 - Release:      [1]
# 5 - License:      [GPL]
# 6 - Group:        [checkinstall]
# 7 - Architecture: [amd64]
# 8 - Source:       [programa-1.0]
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
    --pkgname=mi-software \
    --pkgversion=1.0 \
    --pkgrelease=1 \
    --maintainer="admin@example.com" \
    --provides=mi-software \
    --requires="libc6,libonig5" \
    --install=yes \
    --default
```

### Referencia de flags

| Flag | Significado |
|------|-------------|
| `-D` | Crear .deb (default en Debian) |
| `-R` | Crear .rpm (default en RHEL) |
| `-S` | Crear .tar.gz (Slackware) |
| `--default` | Aceptar defaults sin preguntar |
| `--install=yes` | Instalar el paquete después de crearlo |
| `--install=no` | Solo crear el paquete, no instalar |
| `--pkgname` | Nombre del paquete |
| `--pkgversion` | Versión |
| `--pkgrelease` | Release (build number) |
| `--requires` | Dependencias (separadas por coma) |
| `--provides` | Capabilities que provee |
| `--maintainer` | Email del mantenedor |
| `--fstrans=yes` | Usar filesystem transaction (default, más seguro) |
| `--preinstall=FILE` | Script a ejecutar antes de instalar |
| `--postinstall=FILE` | Script a ejecutar después de instalar |

---

## Ejemplo completo: jq con checkinstall

```bash
# 1. Preparar
cd /tmp
git clone --depth 1 --branch jq-1.7.1 https://github.com/jqlang/jq.git
cd jq

# 2. Dependencias
# Debian:
sudo apt install build-essential autoconf automake libtool
# RHEL:
# sudo dnf groupinstall "Development Tools"
# sudo dnf install autoconf automake libtool

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
    --requires="libc6" \
    --default

# Resultado:
# **********************************************************************
#  Done. The new package has been installed and saved to
#  /tmp/jq/jq-local_1.7.1-1_amd64.deb
# **********************************************************************

# 5. Verificar que está registrado
dpkg -l jq-local
# ii  jq-local  1.7.1-1  amd64  Software compilado desde fuente

# 6. Ver archivos que instaló
dpkg -L jq-local
# /usr/local/bin/jq
# /usr/local/share/man/man1/jq.1
# ...

# 7. Desinstalar limpiamente
sudo apt remove jq-local

# 8. Limpiar fuentes
cd /tmp && rm -rf jq/
```

---

## Cómo detectar dependencias para --requires

checkinstall no detecta dependencias automáticamente. Para saber qué
necesita tu binario compilado:

```bash
# Ejecutar ldd sobre el binario
ldd /usr/local/bin/jq
# linux-vdso.so.1 (0x...)
# libonig.so.5 => /lib/x86_64-linux-gnu/libonig.so.5 (0x...)
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)

# Para cada .so, encontrar el paquete RUNTIME que la provee:
dpkg -S /lib/x86_64-linux-gnu/libonig.so.5
# libonig5:amd64: /lib/x86_64-linux-gnu/libonig.so.5.0.0

dpkg -S /lib/x86_64-linux-gnu/libc.so.6
# libc6:amd64: /lib/x86_64-linux-gnu/libc.so.6

# NOTA: para --requires necesitas el paquete RUNTIME (libonig5),
# NO el paquete -dev (libonig-dev).
# -dev tiene los headers para compilar; el runtime tiene la .so para ejecutar.

# Usar en checkinstall:
sudo checkinstall --requires="libc6,libonig5"
```

---

## Ventajas y limitaciones

```
Ventajas:
  + El software queda registrado en dpkg/rpm
  + Se puede desinstalar limpiamente (apt remove / dnf remove)
  + Se genera un .deb/.rpm redistribuible
  + Se puede especificar dependencias (--requires)
  + Soporta scripts pre/post con --preinstall/--postinstall
  + No hay que cambiar nada en el build process

Limitaciones:
  - No detecta dependencias automáticamente
    (hay que determinarlas con ldd + dpkg -S)
  - El paquete no es tan limpio como uno hecho con debhelper/rpmbuild
    (puede incluir archivos innecesarios o de docs)
  - Si el software se actualiza, hay que repetir el proceso completo
  - Puede no capturar todos los archivos si make install usa
    métodos no estándar (rsync, git checkout, etc.)
  - Proyecto poco mantenido, pero sigue funcional para casos simples
```

---

## Alternativas a checkinstall

### DESTDIR — Instalación en directorio staging

```bash
# DESTDIR es una variable que soportan muchos Makefiles.
# Instala en un directorio temporal SIN tocar el sistema real.

./configure --prefix=/usr/local
make -j$(nproc)
make install DESTDIR=/tmp/staging

# Los archivos quedan en:
# /tmp/staging/usr/local/bin/programa
# /tmp/staging/usr/local/lib/libfoo.so
# /tmp/staging/usr/local/share/man/...

# Desde ahí se puede empaquetar con fpm, dpkg-deb, o inspeccionar
```

DESTDIR no es una alternativa en sí — es un paso intermedio que permite
inspeccionar qué se instalaría antes de empaquetar o instalar realmente.

### fpm — Effing Package Management

```bash
# fpm es más potente y flexible que checkinstall
# Instalar
sudo apt install ruby ruby-dev
sudo gem install fpm
# RHEL: sudo dnf install ruby ruby-devel && sudo gem install fpm

# Crear .deb desde un directorio DESTDIR:
make install DESTDIR=/tmp/staging
fpm -s dir -t deb \
    -n mi-software \
    -v 1.0 \
    -C /tmp/staging \
    .

# Ventajas sobre checkinstall:
# - Múltiples formatos: deb, rpm, pacman, tar, etc.
# - Conversión entre formatos (deb → rpm)
# - Detecta dependencias de shared libraries
# - Soporta scripts pre/post install
# - Mejor manejo de symlinks
# - Activamente mantenido
```

### dpkg-deb — Crear .deb manualmente

```bash
# Para paquetes simples, se puede crear un .deb a mano

# 1. Crear estructura
mkdir -p /tmp/mi-paquete/DEBIAN
mkdir -p /tmp/mi-paquete/usr/local/bin
mkdir -p /tmp/mi-paquete/usr/local/share/man/man1

# 2. Copiar archivos
cp programa /tmp/mi-paquete/usr/local/bin/
cp programa.1 /tmp/mi-paquete/usr/local/share/man/man1/

# 3. Crear archivo de control
cat > /tmp/mi-paquete/DEBIAN/control << 'EOF'
Package: mi-programa
Version: 1.0
Architecture: amd64
Maintainer: admin@example.com
Depends: libc6
Description: Mi programa compilado desde fuente
 Compilado con ./configure && make && checkinstall.
EOF

# 4. Crear el .deb
dpkg-deb --build /tmp/mi-paquete mi-programa_1.0_amd64.deb

# 5. Instalar
sudo dpkg -i mi-programa_1.0_amd64.deb
```

### alien — Convertir entre formatos

```bash
# alien convierte paquetes entre .deb, .rpm, .tgz
sudo apt install alien

# .rpm → .deb
sudo alien -d paquete.rpm

# .deb → .rpm
sudo alien -r paquete.deb

# No recomendado para paquetes críticos del sistema.
# Útil para paquetes de terceros que solo existen en otro formato.
```

---

## Cuándo usar cada método

| Método | Complejidad | Desinstalable | Cuándo usarlo |
|---|---|---|---|
| `make install` | Mínima | No | Pruebas rápidas, `--prefix=/opt/X` aislado |
| `checkinstall` | Baja | Sí | Software que quieres gestionar con dpkg/rpm |
| `fpm` | Media | Sí | Paquetes redistribuibles de calidad |
| `dpkg-deb` manual | Media | Sí | Control total, paquetes simples |
| `debhelper`/`rpmbuild` | Alta | Sí | Paquetes oficiales para distribución |

**Regla práctica:** para el 90% de compilaciones manuales, `checkinstall`
es suficiente. Si necesitas redistribuir el paquete internamente, usa `fpm`.
Para paquetes públicos/oficiales, usa `debhelper` (Debian) o `rpmbuild` (RHEL).

---

## Solución de problemas

### "checkinstall not found"

```bash
which checkinstall || sudo apt install checkinstall
# RHEL: sudo dnf install epel-release && sudo dnf install checkinstall
```

### "RPM package creation failed" (en Debian)

```bash
# En Debian, usar -D (crear .deb), no -R
sudo checkinstall -D --default

# En RHEL, si falla rpmbuild:
rpm -q rpm-build || sudo dnf install rpm-build
```

### checkinstall no captura todos los archivos

```bash
# Causa: make install usa rsync, git, u otros métodos no estándar

# Solución: usar DESTDIR + fpm en su lugar
make install DESTDIR=/tmp/staging
fpm -s dir -t deb -n mi-software -v 1.0 -C /tmp/staging .
```

---

## Labs

### Lab 1 — El problema de make install

```bash
docker compose exec debian-dev bash -c '
echo "=== make install: el problema ==="
echo ""
echo "make install copia archivos al sistema:"
echo "  /usr/local/bin/programa"
echo "  /usr/local/lib/libfoo.so"
echo "  /usr/local/share/man/man1/programa.1"
echo ""
echo "PERO no:"
echo "  - Registra qué archivos instaló"
echo "  - Gestiona dependencias"
echo "  - Permite desinstalar limpiamente"
echo ""
echo "apt/dnf NO saben que el software existe:"
dpkg -S /usr/local/bin/* 2>/dev/null | head -3 || \
    echo "  dpkg-query: no path found matching pattern"
echo "  (archivos en /usr/local no están gestionados)"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquete del gestor vs make install ==="
echo ""
printf "%-22s %-18s %-18s\n" "Aspecto" "apt install" "make install"
printf "%-22s %-18s %-18s\n" "---------------------" "-----------------" "-----------------"
printf "%-22s %-18s %-18s\n" "Registro de archivos" "Sí (dpkg -L)" "No"
printf "%-22s %-18s %-18s\n" "Dependencias" "Resueltas" "Manual"
printf "%-22s %-18s %-18s\n" "Desinstalar" "apt remove" "Manual/uninstall"
printf "%-22s %-18s %-18s\n" "Actualizar" "apt upgrade" "Recompilar"
printf "%-22s %-18s %-18s\n" "Verificar integridad" "dpkg -V" "No"
printf "%-22s %-18s %-18s\n" "Seguridad" "Firmado GPG" "Confianza manual"
echo ""
echo "checkinstall cierra esta brecha"
'
```

### Lab 2 — checkinstall

```bash
docker compose exec debian-dev bash -c '
echo "=== checkinstall: el concepto ==="
echo ""
echo "En lugar de:   sudo make install"
echo "Usar:          sudo checkinstall"
echo ""
echo "checkinstall:"
echo "  1. Ejecuta make install (monitoreado)"
echo "  2. Captura TODOS los archivos copiados"
echo "  3. Crea un paquete .deb (o .rpm)"
echo "  4. Instala vía dpkg -i (o rpm -i)"
echo ""
echo "=== Opciones principales ==="
echo ""
echo "  -D:          crear .deb (default en Debian)"
echo "  -R:          crear .rpm (default en RHEL)"
echo "  --default:   aceptar defaults sin preguntar"
echo "  --install=yes: instalar después de crear"
echo "  --requires:  dependencias del paquete"
echo ""
echo "=== Disponibilidad ==="
which checkinstall 2>/dev/null && echo "checkinstall: instalado" || \
    echo "checkinstall: no instalado (sudo apt install checkinstall)"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Después de checkinstall ==="
echo ""
echo "El software queda registrado:"
echo "  dpkg -l mi-software     → muestra versión e info"
echo "  dpkg -L mi-software     → lista archivos instalados"
echo "  sudo apt remove mi-soft → desinstala limpiamente"
echo ""
echo "=== Ventajas ==="
echo "  + Registrado en dpkg/rpm"
echo "  + Desinstalación limpia"
echo "  + Genera .deb/.rpm redistribuible"
echo ""
echo "=== Limitaciones ==="
echo "  - No detecta dependencias automáticamente"
echo "  - Puede incluir archivos innecesarios"
echo "  - Hay que repetir el proceso para actualizar"
'
```

### Lab 3 — DESTDIR y alternativas

```bash
docker compose exec debian-dev bash -c '
echo "=== DESTDIR: instalar en directorio temporal ==="
echo ""
echo "  make install DESTDIR=/tmp/staging"
echo ""
echo "Los archivos quedan en /tmp/staging/..."
echo "El sistema real no se toca"
echo ""
echo "=== fpm (alternativa moderna) ==="
echo ""
echo "  make install DESTDIR=/tmp/staging"
echo "  fpm -s dir -t deb -n mi-soft -v 1.0 -C /tmp/staging ."
echo ""
echo "Ventajas sobre checkinstall:"
echo "  - Múltiples formatos (deb, rpm, pacman)"
echo "  - Detecta dependencias de shared libraries"
echo "  - Conversión entre formatos (deb → rpm)"
echo ""
echo "=== dpkg-deb manual ==="
echo ""
echo "  mkdir -p /tmp/pkg/DEBIAN"
echo "  cp binario /tmp/pkg/usr/local/bin/"
echo "  cat > /tmp/pkg/DEBIAN/control << EOF"
echo "  Package: mi-prog"
echo "  Version: 1.0"
echo "  Architecture: amd64"
echo "  ..."
echo "  EOF"
echo "  dpkg-deb --build /tmp/pkg mi-prog_1.0_amd64.deb"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuándo usar cada método ==="
printf "%-18s %-12s %s\n" "Método" "Complejidad" "Cuándo usar"
printf "%-18s %-12s %s\n" "-----------------" "-----------" "----------------------------"
printf "%-18s %-12s %s\n" "make install" "Mínima" "Pruebas, prefix aislado"
printf "%-18s %-12s %s\n" "checkinstall" "Baja" "Gestionar con dpkg/rpm"
printf "%-18s %-12s %s\n" "fpm" "Media" "Paquetes redistribuibles"
printf "%-18s %-12s %s\n" "dpkg-deb manual" "Media" "Control total"
printf "%-18s %-12s %s\n" "debhelper/rpmbuild" "Alta" "Paquetes oficiales"
echo ""
echo "90% de los casos: checkinstall"
echo "Redistribuir: fpm"
echo "Paquetes oficiales: debhelper (Debian) / rpmbuild (RHEL)"
'
```

---

## Ejercicios

### Ejercicio 1 — El problema de make install

Explica con un diagrama por qué `make install` es problemático. ¿Qué
información falta para poder desinstalar? ¿Por qué `apt` y `dpkg` no
pueden ver software instalado con `make install`?

<details><summary>Predicción</summary>

```
make install ejecuta:
  install -m 755 programa /usr/local/bin/programa
  install -m 644 programa.1 /usr/local/share/man/man1/programa.1

Falta:
  - Un manifiesto de archivos (qué se copió y dónde)
  - Registro en la base de datos de dpkg (/var/lib/dpkg/status)
  - Metadatos: versión, dependencias, descripción

dpkg/apt no lo ven porque solo consultan /var/lib/dpkg/status.
Si el paquete no se instaló vía dpkg -i, no existe en esa base
de datos. Es invisible para el gestor de paquetes.

dpkg -S /usr/local/bin/programa
→ "not found" — dpkg no tiene registro de ese archivo.
```

</details>

### Ejercicio 2 — checkinstall básico

Describe el flujo completo de compilar GNU Hello con checkinstall.
¿Qué comandos ejecutarías? ¿Qué nombre y versión le darías al paquete?
¿Cómo verificarías que está registrado en dpkg?

<details><summary>Predicción</summary>

```bash
# 1. Descargar y extraer
cd /tmp
wget https://ftp.gnu.org/gnu/hello/hello-2.12.1.tar.gz
tar -xzf hello-2.12.1.tar.gz && cd hello-2.12.1

# 2. Configurar y compilar
./configure --prefix=/usr/local
make -j$(nproc)

# 3. Instalar con checkinstall
sudo checkinstall -D \
    --pkgname=hello-local \
    --pkgversion=2.12.1 \
    --default

# 4. Verificar
dpkg -l hello-local
# ii  hello-local  2.12.1-1  amd64  ...

dpkg -L hello-local
# /usr/local/bin/hello
# /usr/local/share/man/man1/hello.1
# /usr/local/share/info/hello.info

# 5. Ejecutar
hello
# Hello, world!

# 6. Desinstalar limpiamente
sudo apt remove hello-local
which hello  # → no output (eliminado)
```

</details>

### Ejercicio 3 — Detectar dependencias con ldd

Dado un binario compilado, ¿cómo determinarías sus dependencias para
pasarlas a `--requires`? Usa `ldd` y `dpkg -S` para mapear cada `.so`
a su paquete runtime.

<details><summary>Predicción</summary>

```bash
# 1. Ver qué librerías necesita el binario
ldd /usr/local/bin/jq
# libonig.so.5 => /lib/x86_64-linux-gnu/libonig.so.5
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6

# 2. Para cada .so, encontrar el paquete RUNTIME
dpkg -S /lib/x86_64-linux-gnu/libonig.so.5
# libonig5:amd64: /lib/x86_64-linux-gnu/libonig.so.5.0.0

dpkg -S /lib/x86_64-linux-gnu/libc.so.6
# libc6:amd64: /lib/x86_64-linux-gnu/libc.so.6

# 3. Los paquetes runtime son: libonig5, libc6
# (NO libonig-dev, NO libc6-dev — esos son para compilar)

# 4. Usar en checkinstall
sudo checkinstall --requires="libc6,libonig5"
```

Distinción clave:
- `-dev`/`-devel`: headers + .so symlink → necesario para **compilar**
- Sin `-dev`: la `.so` runtime → necesario para **ejecutar**

</details>

### Ejercicio 4 — DESTDIR

Compila GNU Hello y usa `DESTDIR` para instalar en un directorio temporal.
¿Qué estructura de directorios se crea? ¿Cuál es la diferencia entre
`--prefix` y `DESTDIR`?

<details><summary>Predicción</summary>

```bash
./configure --prefix=/usr/local
make -j$(nproc)
make install DESTDIR=/tmp/staging

find /tmp/staging -type f
# /tmp/staging/usr/local/bin/hello
# /tmp/staging/usr/local/share/man/man1/hello.1
# /tmp/staging/usr/local/share/info/hello.info
```

Diferencia:
- `--prefix=/usr/local`: define la ruta **final** donde vivirá el software.
  Se embebe en los binarios (RPATH, rutas de config).
- `DESTDIR=/tmp/staging`: agrega un directorio raíz **temporal** delante del
  prefix. Los archivos se copian a `$DESTDIR/$prefix/bin/` pero el binario
  internamente cree que está en `$prefix/bin/`.

`DESTDIR` es para staging/empaquetado. `--prefix` es la ruta real de
instalación. Nunca se usan uno en lugar del otro.

</details>

### Ejercicio 5 — fpm vs checkinstall

Compara `checkinstall` con `fpm`. ¿Cuándo usarías cada uno? ¿Qué puede
hacer fpm que checkinstall no?

<details><summary>Predicción</summary>

| Aspecto | checkinstall | fpm |
|---------|-------------|-----|
| Instalar | `sudo apt install checkinstall` | `gem install fpm` (requiere Ruby) |
| Uso | `sudo checkinstall` (reemplaza make install) | DESTDIR + `fpm -s dir -t deb ...` |
| Formatos | .deb, .rpm, .tgz | .deb, .rpm, .pacman, .tar, .sh, ... |
| Dependencias | Manual (`--requires`) | Auto-detecta con shared libraries |
| Scripts | Soporta (`--preinstall`, etc.) | Soporta (`--before-install`, etc.) |
| Conversión | No | Sí (deb→rpm, rpm→deb) |
| Mantenimiento | Poco mantenido | Activamente mantenido |

Usar checkinstall: software propio, uso personal, caso simple.
Usar fpm: paquetes para distribuir internamente, múltiples distros,
necesidad de conversión entre formatos.

</details>

### Ejercicio 6 — dpkg-deb manual

Crea un `.deb` manualmente con `dpkg-deb` para un binario simple
(ej: un script bash que imprima "hola"). ¿Qué estructura de directorios
necesitas? ¿Qué campos son obligatorios en `DEBIAN/control`?

<details><summary>Predicción</summary>

```bash
# 1. Crear el "binario" (un script simple)
echo '#!/bin/bash
echo "Hola desde mi paquete"' > /tmp/mi-saludo

# 2. Crear estructura del .deb
mkdir -p /tmp/mi-pkg/DEBIAN
mkdir -p /tmp/mi-pkg/usr/local/bin
cp /tmp/mi-saludo /tmp/mi-pkg/usr/local/bin/mi-saludo
chmod 755 /tmp/mi-pkg/usr/local/bin/mi-saludo

# 3. Crear control (campos OBLIGATORIOS: Package, Version, Architecture,
#    Maintainer, Description)
cat > /tmp/mi-pkg/DEBIAN/control << 'EOF'
Package: mi-saludo
Version: 1.0
Architecture: all
Maintainer: yo@example.com
Description: Script de saludo
 Un script bash que imprime hola.
EOF

# 4. Construir
dpkg-deb --build /tmp/mi-pkg mi-saludo_1.0_all.deb

# 5. Instalar y probar
sudo dpkg -i mi-saludo_1.0_all.deb
mi-saludo    # → "Hola desde mi paquete"

# 6. Desinstalar
sudo dpkg -r mi-saludo
```

Campos obligatorios en `DEBIAN/control`: Package, Version, Architecture,
Maintainer, Description. Sin ellos, `dpkg-deb --build` falla.

</details>

### Ejercicio 7 — Comparar make install vs checkinstall

Instala el mismo software dos veces: una con `make install` (en
`--prefix=/opt/test1`) y otra con `checkinstall`. Compara: ¿cuál
aparece en `dpkg -l`? ¿Cuál se puede desinstalar con `apt remove`?

<details><summary>Predicción</summary>

```bash
# Opción 1: make install directo
./configure --prefix=/opt/test1
make -j$(nproc)
sudo make install

dpkg -l | grep test1      # → nada (invisible para dpkg)
dpkg -S /opt/test1/bin/hello  # → not found
# Desinstalar: sudo rm -rf /opt/test1

# Opción 2: checkinstall
./configure --prefix=/usr/local
make -j$(nproc)
sudo checkinstall -D --pkgname=hello-check --pkgversion=2.12.1 --default

dpkg -l | grep hello-check  # → ii hello-check 2.12.1-1 amd64
dpkg -L hello-check          # → lista todos los archivos
# Desinstalar: sudo apt remove hello-check
```

make install: rápido pero invisible. Solo viable con prefix aislado.
checkinstall: un paso extra pero el sistema lo gestiona completamente.

</details>

### Ejercicio 8 — alien: convertir entre formatos

¿Qué hace `alien`? ¿Por qué no se recomienda para paquetes críticos?
¿Cuándo tiene sentido usarlo?

<details><summary>Predicción</summary>

```bash
# alien convierte entre formatos de paquetes:
sudo alien -d paquete.rpm    # .rpm → .deb
sudo alien -r paquete.deb    # .deb → .rpm

# No recomendado para paquetes críticos porque:
# 1. Los scripts pre/post pueden usar comandos específicos de una distro
#    (ej: update-rc.d en Debian, systemctl preset en RHEL)
# 2. Las dependencias tienen nombres distintos (libssl-dev vs openssl-devel)
# 3. Las rutas de archivos pueden diferir (/lib vs /usr/lib64)
# 4. Los paquetes del sistema pueden tener macros RPM/debhelper incompatibles

# Tiene sentido cuando:
# - Un vendor solo provee .rpm y necesitas .deb (o viceversa)
# - Es un paquete simple sin scripts ni dependencias complejas
# - Es para probar/evaluar, no para producción
```

</details>

### Ejercicio 9 — Flujo de empaquetado profesional

Diseña el flujo completo para empaquetar un proyecto compilado desde
fuente de manera profesional: desde `git clone` hasta un `.deb`
redistribuible. ¿Qué herramienta usarías en cada paso?

<details><summary>Predicción</summary>

```
1. git clone + git checkout vX.Y.Z (versión específica)
2. Instalar dependencias de compilación (apt install *-dev)
3. autoreconf -fi (si es necesario)
4. ./configure --prefix=/usr/local
5. make -j$(nproc)
6. make install DESTDIR=/tmp/staging
7. Inspeccionar /tmp/staging (verificar que solo tiene lo necesario)
8. Determinar dependencias: ldd binario | dpkg -S cada .so
9. Empaquetar con fpm:
   fpm -s dir -t deb \
     -n mi-software -v X.Y.Z \
     -d "libc6" -d "libonig5" \
     --after-install scripts/postinst.sh \
     -C /tmp/staging .
10. Probar: sudo dpkg -i mi-software_X.Y.Z_amd64.deb
11. Verificar: dpkg -L mi-software, ejecutar el programa
12. Distribuir el .deb internamente
```

Para paquetes públicos/oficiales, reemplazar fpm por `debhelper`
(Debian) o `rpmbuild` con un `.spec` file (RHEL), que producen
paquetes que cumplen las políticas de la distribución.

</details>

### Ejercicio 10 — Panorama: gestión de software compilado

Compara los 5 métodos de gestionar software compilado desde fuente.
¿Cuál elegirías para: (a) un lab de pruebas, (b) tu servidor personal,
(c) distribución interna en una empresa?

<details><summary>Predicción</summary>

| Método | Lab | Servidor personal | Empresa |
|--------|-----|-------------------|---------|
| `make install` + prefix aislado | Sí | Aceptable | No |
| `checkinstall` | Sí | Sí | Aceptable |
| `fpm` | Excesivo | Sí | Sí |
| `dpkg-deb` manual | Excesivo | Sí (simple) | Según caso |
| `debhelper`/`rpmbuild` | No | Excesivo | Sí (oficial) |

**(a) Lab de pruebas:** `make install --prefix=/opt/X` — rápido, desinstalar
con `rm -rf`, no importa si queda sucio.

**(b) Servidor personal:** `checkinstall` — el software queda registrado,
se desinstala limpiamente, bajo esfuerzo.

**(c) Distribución interna en empresa:** `fpm` con DESTDIR — produce
paquetes `.deb`/`.rpm` de calidad, con dependencias declaradas, scripts
pre/post, redistribuibles. Si la empresa tiene repositorio propio,
complementar con `reprepro` (Debian) o `createrepo` (RHEL).

</details>
