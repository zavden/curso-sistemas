# Lab — rpm

## Objetivo

Dominar rpm como herramienta de bajo nivel: entender la anatomia
de un nombre de paquete RPM, usar los flags de consulta (-q con
sus variantes), verificar la integridad de archivos, inspeccionar
scripts de paquetes, y comparar con dpkg.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Consultas rpm -q

### Objetivo

Usar los flags de consulta de rpm para explorar los paquetes
del sistema.

### Paso 1.1: Anatomia de un nombre RPM

```bash
docker compose exec alma-dev bash -c '
echo "=== Anatomia de un nombre de paquete RPM ==="
echo ""
echo "Ejemplo: bash-5.1.8-9.el9.x86_64"
echo ""
echo "  bash     = name (nombre del software)"
echo "  5.1.8    = version (upstream)"
echo "  9.el9    = release (build del empaquetador + distro)"
echo "  x86_64   = arch (arquitectura)"
echo ""
echo "=== Epoch (campo oculto) ==="
echo "Tiene prioridad sobre version para comparaciones"
echo "Formato completo: epoch:name-version-release.arch"
echo ""
echo "--- Ejemplo real ---"
rpm -q bash
echo ""
echo "--- Con epoch ---"
rpm -q --queryformat "%{EPOCH}:%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n" bash
echo "(epoch 0 = default, no se muestra normalmente)"
'
```

### Paso 1.2: Esta instalado (rpm -q)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -q: consultar si esta instalado ==="
echo ""
rpm -q bash
rpm -q curl
rpm -q paquete-inexistente
echo ""
echo "=== rpm -qa: listar TODOS ==="
echo "Total paquetes instalados: $(rpm -qa | wc -l)"
echo ""
echo "--- Buscar con patron ---"
rpm -qa "python3*" | head -5
echo ""
echo "--- Buscar con grep ---"
rpm -qa | grep -i ssh | head -5
'
```

### Paso 1.3: Informacion detallada (rpm -qi)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qi: informacion del paquete ==="
rpm -qi bash
'
```

### Paso 1.4: Listar archivos (rpm -ql)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -ql: archivos instalados ==="
echo ""
echo "--- Archivos de bash ---"
rpm -ql bash | head -15
echo ""
total=$(rpm -ql bash | wc -l)
echo "Total archivos: $total"
echo ""
echo "--- Solo config (rpm -qc) ---"
rpm -qc bash
echo ""
echo "--- Solo documentacion (rpm -qd) ---"
rpm -qd bash | head -5
'
```

### Paso 1.5: Buscar dueno (rpm -qf)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qf: a que paquete pertenece ==="
echo ""
rpm -qf /usr/bin/bash
rpm -qf /usr/bin/curl
rpm -qf /usr/bin/env
rpm -qf /etc/passwd
echo ""
echo "--- Archivo no gestionado por rpm ---"
rpm -qf /usr/local/bin/algo 2>/dev/null || \
    echo "file /usr/local/bin/algo is not owned by any package"
echo "  → fue instalado manualmente (no via rpm)"
'
```

### Paso 1.6: Dependencias (rpm -qR)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qR: dependencias de un paquete ==="
rpm -qR bash | head -10
echo "..."
echo ""
echo "=== Dependencias inversas ==="
echo "Paquetes que dependen de bash:"
rpm -q --whatrequires bash | head -5
'
```

---

## Parte 2 — Verificar integridad

### Objetivo

Usar rpm -V para detectar archivos modificados y entender los
codigos de verificacion.

### Paso 2.1: Verificar un paquete (rpm -V)

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -V: verificar integridad ==="
echo ""
echo "--- Verificar bash ---"
result=$(rpm -V bash 2>/dev/null)
if [ -z "$result" ]; then
    echo "bash: OK (todos los archivos intactos)"
else
    echo "$result"
fi
echo ""
echo "--- Verificar coreutils ---"
result=$(rpm -V coreutils 2>/dev/null)
if [ -z "$result" ]; then
    echo "coreutils: OK"
else
    echo "$result"
fi
echo ""
echo "Sin output = todo OK"
echo "Con output = hay archivos modificados"
'
```

### Paso 2.2: Codigos de verificacion

```bash
docker compose exec alma-dev bash -c '
echo "=== Codigos de rpm -V ==="
echo ""
echo "Formato: S.5....T.  c /etc/nginx/nginx.conf"
echo ""
echo "Cada posicion es una verificacion:"
echo "  S = tamano cambio"
echo "  . = (reservado / test paso)"
echo "  5 = checksum MD5/SHA cambio (contenido)"
echo "  . = (reservado)"
echo "  . = (reservado)"
echo "  . = (reservado)"
echo "  . = (reservado)"
echo "  T = mtime cambio"
echo "  . = (reservado)"
echo ""
echo "Despues del espacio:"
echo "  c = archivo de configuracion"
echo "  d = documentacion"
echo "  g = ghost"
echo "  (vacio) = archivo regular"
echo ""
echo "Config (c) modificado = NORMAL"
echo "Binario modificado    = ALERTA de seguridad"
'
```

### Paso 2.3: Verificar todos los paquetes

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar todos los paquetes ==="
echo "(puede tardar unos segundos)"
echo ""
echo "--- Solo archivos NO de configuracion modificados ---"
rpm -Va 2>/dev/null | grep -v " c " | head -10
echo ""
echo "--- Solo archivos de configuracion ---"
rpm -Va 2>/dev/null | grep " c " | head -10
echo ""
echo "Archivos de config (c) modificados: normal"
echo "Archivos regulares modificados: investigar"
echo ""
echo "Si /usr/bin/ls o similar aparece con 5 (checksum),"
echo "el sistema puede estar comprometido"
'
```

---

## Parte 3 — Scripts y comparacion

### Objetivo

Inspeccionar los scripts de un paquete y comparar rpm con dpkg.

### Paso 3.1: Scripts del paquete

```bash
docker compose exec alma-dev bash -c '
echo "=== Scripts de un paquete ==="
echo ""
echo "rpm -q --scripts muestra los scripts pre/post install/uninstall:"
echo ""
pkg=$(rpm -qa | grep "^openssh-server\|^systemd-" | head -1)
if [ -n "$pkg" ]; then
    echo "--- Scripts de $pkg ---"
    rpm -q --scripts "$pkg" 2>/dev/null | head -25
    echo "..."
else
    echo "--- Scripts de bash ---"
    scripts=$(rpm -q --scripts bash 2>/dev/null)
    if [ -z "$scripts" ]; then
        echo "(bash no tiene scripts de instalacion)"
    else
        echo "$scripts"
    fi
fi
echo ""
echo "Tipos de scripts:"
echo "  preinstall:    antes de instalar"
echo "  postinstall:   despues de instalar"
echo "  preuninstall:  antes de desinstalar"
echo "  postuninstall: despues de desinstalar"
'
```

### Paso 3.2: Consultar un .rpm sin instalar

```bash
docker compose exec alma-dev bash -c '
echo "=== Consultar .rpm sin instalar ==="
echo ""
echo "Agregar -p para consultar un archivo .rpm:"
echo ""
echo "rpm -qpi paquete.rpm    → info"
echo "rpm -qpl paquete.rpm    → listar archivos"
echo "rpm -qpR paquete.rpm    → dependencias"
echo "rpm -qpc paquete.rpm    → archivos de config"
echo ""
echo "Util para inspeccionar ANTES de instalar"
echo "especialmente paquetes de fuentes no confiables"
echo ""
echo "--- Intentar descargar un .rpm ---"
cd /tmp
dnf download bash --destdir=/tmp/rpm-test 2>/dev/null
if ls /tmp/rpm-test/*.rpm 2>/dev/null | head -1; then
    pkg=$(ls /tmp/rpm-test/*.rpm | head -1)
    echo ""
    echo "--- rpm -qpi (info) ---"
    rpm -qpi "$pkg" | head -10
    echo "..."
    echo ""
    echo "--- rpm -qpl (archivos) ---"
    rpm -qpl "$pkg" | head -10
    echo "..."
fi
rm -rf /tmp/rpm-test
'
```

### Paso 3.3: Verificar firma GPG

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar firma GPG de un paquete ==="
echo ""
echo "rpm -K paquete.rpm"
echo "  → digests signatures OK"
echo ""
echo "rpm --checksig paquete.rpm (equivalente)"
echo ""
echo "Si la clave no esta instalada:"
echo "  → RSA sha256 (MD5) PGP NOT OK (MISSING KEYS: ...)"
echo "  → Importar: sudo rpm --import URL-de-la-clave"
echo ""
echo "=== Importar claves ==="
echo "sudo rpm --import https://www.redhat.com/security/data/fd431d51.txt"
echo ""
echo "Las claves se guardan como paquetes gpg-pubkey-*:"
rpm -qa gpg-pubkey* | head -3
'
```

### Paso 3.4: Comparacion rpm vs dpkg

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm vs dpkg ==="
printf "%-25s %-22s %-22s\n" "Operacion" "dpkg" "rpm"
printf "%-25s %-22s %-22s\n" "------------------------" "---------------------" "---------------------"
printf "%-25s %-22s %-22s\n" "Instalar" "dpkg -i pkg.deb" "rpm -ivh pkg.rpm"
printf "%-25s %-22s %-22s\n" "Desinstalar" "dpkg -r pkg" "rpm -e pkg"
printf "%-25s %-22s %-22s\n" "Listar instalados" "dpkg -l" "rpm -qa"
printf "%-25s %-22s %-22s\n" "Info paquete" "dpkg -s pkg" "rpm -qi pkg"
printf "%-25s %-22s %-22s\n" "Archivos del paquete" "dpkg -L pkg" "rpm -ql pkg"
printf "%-25s %-22s %-22s\n" "Dueno de archivo" "dpkg -S file" "rpm -qf file"
printf "%-25s %-22s %-22s\n" "Verificar" "dpkg -V pkg" "rpm -V pkg"
printf "%-25s %-22s %-22s\n" "Config files" "dpkg -L + filtrar" "rpm -qc pkg"
printf "%-25s %-22s %-22s\n" "Inspeccionar pkg" "dpkg -I pkg.deb" "rpm -qpi pkg.rpm"
printf "%-25s %-22s %-22s\n" "Scripts" "(manual)" "rpm --scripts pkg"
echo ""
echo "rpm tiene flag dedicado para config files (-qc)"
echo "y para scripts (--scripts), que dpkg no tiene"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un nombre RPM tiene 4 partes: **name-version-release.arch**.
   El campo **epoch** (oculto) tiene prioridad sobre version
   para comparaciones.

2. `rpm -q` es el modo consulta, combinable con: **-i** (info),
   **-l** (archivos), **-f** (dueno de archivo), **-R**
   (dependencias), **-c** (config files), **-d** (docs).

3. `rpm -V` verifica integridad. Cada posicion indica: **S**
   (tamano), **5** (checksum), **T** (mtime), etc. Archivos
   de configuracion (**c**) modificados son normales; binarios
   modificados son alerta de seguridad.

4. `rpm -q --scripts` muestra los scripts pre/post de
   instalacion/desinstalacion. Util para entender que hace
   un paquete al instalarse.

5. El flag **-p** permite consultar un archivo `.rpm` sin
   instalarlo: `rpm -qpi`, `rpm -qpl`, `rpm -qpR`.

6. rpm y dpkg son analogos: mismas capacidades, diferente
   sintaxis. rpm tiene flags dedicados para config files (-qc)
   y scripts (--scripts).
