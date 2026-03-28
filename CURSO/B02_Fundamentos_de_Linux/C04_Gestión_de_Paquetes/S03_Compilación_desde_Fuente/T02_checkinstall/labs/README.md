# Lab — checkinstall

## Objetivo

Entender el problema de make install (no deja registro en el
gestor de paquetes), como checkinstall lo resuelve interceptando
la instalacion y creando un .deb/.rpm, y las alternativas DESTDIR,
fpm, y dpkg-deb.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — El problema de make install

### Objetivo

Entender por que make install es problematico para la gestion
del sistema.

### Paso 1.1: Que hace make install

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
echo "  - Registra que archivos instalo (no hay manifiesto)"
echo "  - Gestiona dependencias"
echo "  - Permite desinstalar limpiamente"
echo "  - Se actualiza automaticamente"
echo ""
echo "=== Consecuencias ==="
echo ""
echo "Para desinstalar:"
echo "  make uninstall    ← SOLO si el Makefile lo implementa"
echo "                       (muchos NO lo hacen)"
echo ""
echo "Si no hay uninstall:"
echo "  No hay forma limpia de saber que se instalo"
echo "  Hay que buscar y eliminar manualmente"
echo ""
echo "apt/dnf NO saben que el software existe:"
dpkg -S /usr/local/bin/* 2>/dev/null | head -3 || \
    echo "  dpkg-query: no path found matching pattern"
echo "  (archivos en /usr/local no estan gestionados)"
'
```

### Paso 1.2: Comparar con paquetes del gestor

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquete del gestor vs make install ==="
echo ""
printf "%-22s %-18s %-18s\n" "Aspecto" "apt install" "make install"
printf "%-22s %-18s %-18s\n" "---------------------" "-----------------" "-----------------"
printf "%-22s %-18s %-18s\n" "Registro de archivos" "Si (dpkg -L)" "No"
printf "%-22s %-18s %-18s\n" "Dependencias" "Resueltas" "Manual"
printf "%-22s %-18s %-18s\n" "Desinstalar" "apt remove" "Manual/uninstall"
printf "%-22s %-18s %-18s\n" "Actualizar" "apt upgrade" "Recompilar"
printf "%-22s %-18s %-18s\n" "Verificar integridad" "dpkg -V" "No"
printf "%-22s %-18s %-18s\n" "Seguridad" "Firmado GPG" "Confianza manual"
echo ""
echo "checkinstall cierra esta brecha:"
echo "  Intercepta make install y crea un .deb/.rpm"
echo "  El software queda registrado en dpkg/rpm"
'
```

---

## Parte 2 — checkinstall

### Objetivo

Entender como checkinstall crea un paquete desde make install.

### Paso 2.1: Que hace checkinstall

```bash
docker compose exec debian-dev bash -c '
echo "=== checkinstall: el concepto ==="
echo ""
echo "En lugar de:   sudo make install"
echo "Usar:          sudo checkinstall"
echo ""
echo "checkinstall:"
echo "  1. Ejecuta make install en un entorno monitoreado"
echo "  2. Captura TODOS los archivos que se copian"
echo "  3. Crea un paquete .deb (o .rpm) con esos archivos"
echo "  4. Instala el paquete via dpkg -i (o rpm -i)"
echo ""
echo "Resultado:"
echo "  El software aparece en dpkg -l / rpm -qa"
echo "  Se desinstala con apt remove / dnf remove"
echo ""
echo "=== Disponibilidad ==="
which checkinstall 2>/dev/null && echo "checkinstall: instalado" || \
    echo "checkinstall: no instalado (sudo apt install checkinstall)"
'
```

### Paso 2.2: Opciones de checkinstall

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones de checkinstall ==="
echo ""
echo "--- Uso basico ---"
echo "  sudo checkinstall"
echo "  (pregunta nombre, version, descripcion)"
echo ""
echo "--- Sin preguntas ---"
echo "  sudo checkinstall -D \\"
echo "    --pkgname=mi-software \\"
echo "    --pkgversion=1.0 \\"
echo "    --pkgrelease=1 \\"
echo "    --default"
echo ""
echo "--- Flags ---"
echo "  -D:          crear .deb (default en Debian)"
echo "  -R:          crear .rpm (default en RHEL)"
echo "  --default:   aceptar defaults sin preguntar"
echo "  --install=yes: instalar despues de crear"
echo "  --install=no:  solo crear el paquete"
echo "  --requires:  especificar dependencias"
echo ""
echo "--- Ejemplo completo ---"
echo "  ./configure --prefix=/usr/local"
echo "  make -j\$(nproc)"
echo "  sudo checkinstall -D --pkgname=jq-local --pkgversion=1.7.1 --default"
'
```

### Paso 2.3: Despues de checkinstall

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar el paquete creado ==="
echo ""
echo "Despues de checkinstall:"
echo ""
echo "  dpkg -l mi-software"
echo "  → ii  mi-software  1.0-1  amd64  Descripcion"
echo ""
echo "  dpkg -L mi-software"
echo "  → /usr/local/bin/programa"
echo "  → /usr/local/lib/libfoo.so"
echo ""
echo "  sudo apt remove mi-software"
echo "  → Desinstala limpiamente"
echo ""
echo "=== Ventajas ==="
echo "  + Registrado en dpkg/rpm"
echo "  + Desinstalacion limpia"
echo "  + Se genera un .deb/.rpm redistribuible"
echo ""
echo "=== Limitaciones ==="
echo "  - No detecta dependencias automaticamente"
echo "  - El paquete puede incluir archivos innecesarios"
echo "  - No gestiona scripts pre/post"
echo "  - Hay que repetir el proceso para actualizar"
'
```

---

## Parte 3 — DESTDIR y alternativas

### Objetivo

Conocer DESTDIR y otras herramientas para crear paquetes.

### Paso 3.1: DESTDIR

```bash
docker compose exec debian-dev bash -c '
echo "=== DESTDIR: instalar en directorio temporal ==="
echo ""
echo "Muchos Makefiles soportan DESTDIR:"
echo ""
echo "  make install DESTDIR=/tmp/staging"
echo ""
echo "Los archivos quedan en:"
echo "  /tmp/staging/usr/local/bin/programa"
echo "  /tmp/staging/usr/local/lib/libfoo.so"
echo ""
echo "El sistema real no se toca"
echo "Desde ahi se puede empaquetar con checkinstall, fpm, o dpkg-deb"
echo ""
echo "=== Patron para crear paquetes limpios ==="
echo ""
echo "  ./configure --prefix=/usr/local"
echo "  make -j\$(nproc)"
echo "  make install DESTDIR=/tmp/staging"
echo "  → Inspeccionar /tmp/staging"
echo "  → Empaquetar con fpm o dpkg-deb"
'
```

### Paso 3.2: fpm

```bash
docker compose exec debian-dev bash -c '
echo "=== fpm: Effing Package Management ==="
echo ""
echo "Herramienta moderna y flexible para crear paquetes"
echo ""
echo "Instalacion:"
echo "  sudo apt install ruby ruby-dev"
echo "  sudo gem install fpm"
echo ""
echo "Uso con DESTDIR:"
echo "  make install DESTDIR=/tmp/staging"
echo "  fpm -s dir -t deb \\"
echo "    -n mi-software \\"
echo "    -v 1.0 \\"
echo "    --prefix /usr/local \\"
echo "    -C /tmp/staging \\"
echo "    ."
echo ""
echo "Ventajas sobre checkinstall:"
echo "  - Puede crear deb, rpm, pacman, tar"
echo "  - Soporta scripts pre/post"
echo "  - Puede convertir entre formatos (deb → rpm)"
echo "  - Detecta dependencias de shared libraries"
'
```

### Paso 3.3: dpkg-deb manual

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg-deb: crear .deb manualmente ==="
echo ""
echo "Para paquetes simples:"
echo ""
echo "  mkdir -p /tmp/mi-pkg/DEBIAN"
echo "  mkdir -p /tmp/mi-pkg/usr/local/bin"
echo "  cp programa /tmp/mi-pkg/usr/local/bin/"
echo ""
echo "  cat > /tmp/mi-pkg/DEBIAN/control << EOF"
echo "  Package: mi-programa"
echo "  Version: 1.0"
echo "  Architecture: amd64"
echo "  Maintainer: admin@example.com"
echo "  Description: Mi programa compilado"
echo "  EOF"
echo ""
echo "  dpkg-deb --build /tmp/mi-pkg mi-programa_1.0_amd64.deb"
echo "  sudo dpkg -i mi-programa_1.0_amd64.deb"
echo ""
echo "Control total sobre el paquete, pero mas trabajo"
'
```

### Paso 3.4: Tabla de metodos

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada metodo ==="
printf "%-18s %-12s %s\n" "Metodo" "Complejidad" "Cuando usar"
printf "%-18s %-12s %s\n" "-----------------" "-----------" "----------------------------"
printf "%-18s %-12s %s\n" "make install" "Minima" "Pruebas, prefix aislado"
printf "%-18s %-12s %s\n" "checkinstall" "Baja" "Gestionar con dpkg/rpm"
printf "%-18s %-12s %s\n" "fpm" "Media" "Paquetes redistribuibles"
printf "%-18s %-12s %s\n" "dpkg-deb manual" "Media" "Control total"
printf "%-18s %-12s %s\n" "debhelper/rpmbuild" "Alta" "Paquetes oficiales"
echo ""
echo "Para el 90% de los casos: checkinstall"
echo "Si necesitas redistribuir: fpm"
echo "Para paquetes oficiales: debhelper (Debian) / rpmbuild (RHEL)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `make install` copia archivos al sistema **sin registrarlos**
   en el gestor de paquetes. No hay forma limpia de desinstalar
   a menos que exista `make uninstall`.

2. **checkinstall** intercepta `make install`, captura los
   archivos, y crea un `.deb` o `.rpm`. El software queda
   registrado en dpkg/rpm y se puede desinstalar con
   `apt remove` / `dnf remove`.

3. **DESTDIR** permite instalar en un directorio temporal sin
   tocar el sistema real: `make install DESTDIR=/tmp/staging`.
   Util para inspeccionar antes de empaquetar.

4. **fpm** es una alternativa moderna a checkinstall: mas
   flexible, soporta multiples formatos, scripts pre/post,
   y conversion entre formatos.

5. **dpkg-deb** permite crear un `.deb` manualmente con control
   total: crear estructura DEBIAN/control, copiar archivos,
   y ejecutar `dpkg-deb --build`.

6. Para la mayoria de compilaciones manuales, checkinstall
   es suficiente. Para paquetes redistribuibles usar fpm.
   Para paquetes oficiales usar debhelper o rpmbuild.
