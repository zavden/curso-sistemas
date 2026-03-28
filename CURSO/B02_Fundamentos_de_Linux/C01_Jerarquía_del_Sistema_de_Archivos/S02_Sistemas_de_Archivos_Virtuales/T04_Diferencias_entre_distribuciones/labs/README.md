# Lab — Diferencias entre distribuciones

## Objetivo

Comparar Debian y AlmaLinux lado a lado: identificar la distribucion
de forma portable, comparar logs, paquetes, rutas de configuracion, y
practicar la escritura de scripts que funcionan en ambas familias.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Identificar la distribucion

### Objetivo

Aprender a detectar la familia de distribucion de forma estandar.

### Paso 1.1: /etc/os-release (estandar)

```bash
echo "=== Debian ==="
docker compose exec debian-dev cat /etc/os-release

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev cat /etc/os-release
```

`/etc/os-release` es la forma estandar de identificar la distribucion.
Los campos clave: `ID`, `ID_LIKE`, `VERSION_ID`.

### Paso 1.2: Archivos especificos

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
cat /etc/debian_version 2>/dev/null && echo "(debian_version existe)" || echo "NO existe"
cat /etc/redhat-release 2>/dev/null && echo "(redhat-release existe)" || echo "NO existe"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
cat /etc/debian_version 2>/dev/null && echo "(debian_version existe)" || echo "NO existe"
cat /etc/redhat-release 2>/dev/null && echo "(redhat-release existe)" || echo "NO existe"
'
```

Debian tiene `/etc/debian_version`. AlmaLinux tiene
`/etc/redhat-release`. La forma portable es `/etc/os-release`.

### Paso 1.3: Campos ID e ID_LIKE

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c 'grep "^ID" /etc/os-release'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c 'grep "^ID" /etc/os-release'
```

`ID_LIKE` indica la familia: Debian dice `ID=debian`, AlmaLinux
dice `ID_LIKE="rhel centos fedora"`. Un script puede usar estos
campos para adaptar su comportamiento.

---

## Parte 2 — Comparar archivos y rutas

### Objetivo

Identificar las diferencias concretas en logs, paquetes y
configuracion entre ambas distribuciones.

### Paso 2.1: Archivos de log

```bash
echo "=== Debian: logs ==="
docker compose exec debian-dev ls /var/log/ 2>/dev/null

echo ""
echo "=== AlmaLinux: logs ==="
docker compose exec alma-dev ls /var/log/ 2>/dev/null
```

Busca las diferencias: `auth.log` vs `secure`, `syslog` vs `messages`,
`dpkg.log` vs `dnf.log`.

### Paso 2.2: Gestor de paquetes

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Formato: deb"
echo "Gestor: $(apt --version 2>&1 | head -1)"
echo "Bajo nivel: $(dpkg --version | head -1)"
echo "Paquetes instalados: $(dpkg -l | grep "^ii" | wc -l)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Formato: rpm"
echo "Gestor: $(dnf --version 2>&1 | head -1)"
echo "Bajo nivel: $(rpm --version)"
echo "Paquetes instalados: $(rpm -qa | wc -l)"
'
```

### Paso 2.3: Repositorios

```bash
echo "=== Debian: /etc/apt/ ==="
docker compose exec debian-dev bash -c 'ls /etc/apt/sources.list.d/ 2>/dev/null; cat /etc/apt/sources.list 2>/dev/null | grep -v "^#" | head -3'

echo ""
echo "=== AlmaLinux: /etc/yum.repos.d/ ==="
docker compose exec alma-dev bash -c 'ls /etc/yum.repos.d/'
```

### Paso 2.4: Glibc y librerias

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
ldd --version 2>&1 | head -1
echo "libc: $(ls /usr/lib/x86_64-linux-gnu/libc.so* 2>/dev/null)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
ldd --version 2>&1 | head -1
echo "libc: $(ls /usr/lib64/libc.so* 2>/dev/null)"
'
```

Debian usa multiarch (`/usr/lib/x86_64-linux-gnu/`), AlmaLinux usa
`/usr/lib64/`. Las versiones de glibc tambien difieren.

### Paso 2.5: Locale

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "LANG=$LANG"
cat /etc/default/locale 2>/dev/null || echo "no existe /etc/default/locale"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "LANG=$LANG"
cat /etc/locale.conf 2>/dev/null || echo "no existe /etc/locale.conf"
'
```

Debian: `/etc/default/locale`. AlmaLinux: `/etc/locale.conf`.

---

## Parte 3 — Scripts portables

### Objetivo

Practicar la escritura de scripts que funcionan en ambas familias de
distribuciones.

### Paso 3.1: Deteccion de familia

```bash
docker compose exec debian-dev bash -c '
if [ -f /etc/debian_version ]; then
    echo "Familia: Debian"
elif [ -f /etc/redhat-release ]; then
    echo "Familia: RHEL"
else
    echo "Familia: desconocida"
fi
'

docker compose exec alma-dev bash -c '
if [ -f /etc/debian_version ]; then
    echo "Familia: Debian"
elif [ -f /etc/redhat-release ]; then
    echo "Familia: RHEL"
else
    echo "Familia: desconocida"
fi
'
```

### Paso 3.2: Paths condicionales

```bash
docker compose exec debian-dev bash -c '
# Determinar familia y configurar paths
if [ -f /etc/debian_version ]; then
    AUTH_LOG="/var/log/auth.log"
    PKG_CMD="dpkg -l | grep ^ii | wc -l"
else
    AUTH_LOG="/var/log/secure"
    PKG_CMD="rpm -qa | wc -l"
fi

echo "Log de autenticacion: $AUTH_LOG"
echo "Existe: $([ -f $AUTH_LOG ] && echo si || echo no)"
echo "Paquetes: $(eval $PKG_CMD)"
'
```

### Paso 3.3: Herramientas portables

```bash
echo "=== Herramientas que funcionan igual ==="
for cmd in "ip addr show eth0" "ss -tlnp" "hostname" "uname -r" "whoami"; do
    echo "--- $cmd ---"
    echo "Debian:"
    docker compose exec debian-dev bash -c "$cmd" 2>/dev/null | head -2
    echo "AlmaLinux:"
    docker compose exec alma-dev bash -c "$cmd" 2>/dev/null | head -2
    echo ""
done
```

Las herramientas del paquete `iproute2` (`ip`, `ss`) funcionan
identicamente en ambas familias. Preferirlas sobre las legacy
(`ifconfig`, `netstat`).

### Paso 3.4: Usar os-release en scripts

```bash
docker compose exec debian-dev bash -c '
# Forma portable y robusta
. /etc/os-release
echo "ID: $ID"
echo "Version: $VERSION_ID"

case "$ID" in
    debian|ubuntu)
        echo "Accion para familia Debian"
        ;;
    almalinux|rocky|rhel|centos|fedora)
        echo "Accion para familia RHEL"
        ;;
    *)
        echo "Distribucion no reconocida: $ID"
        ;;
esac
'
```

Sourcear (`source` o `.`) `/etc/os-release` convierte sus campos en
variables de shell. Es la forma mas robusta de detectar la
distribucion en scripts.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/etc/os-release` es la forma **estandar** de identificar la
   distribucion. Sourcear el archivo convierte los campos en
   variables de shell.

2. Las diferencias mas visibles: logs (`auth.log` vs `secure`),
   paquetes (`apt`/`dpkg` vs `dnf`/`rpm`), librerias (multiarch vs
   lib64), locale (`/etc/default/locale` vs `/etc/locale.conf`).

3. Las herramientas de red modernas (`ip`, `ss`) del paquete
   `iproute2` funcionan **identicamente** en ambas familias. Son
   preferibles a las legacy (`ifconfig`, `netstat`).

4. Para scripts portables: detectar la familia con archivos
   especificos (`/etc/debian_version`, `/etc/redhat-release`) o
   con `os-release`, y usar variables condicionales para paths
   y comandos.

5. El FHS define la estructura **general** pero deja libertad en los
   detalles. Ambas familias convergen cada vez mas gracias a systemd
   y al /usr merge.
