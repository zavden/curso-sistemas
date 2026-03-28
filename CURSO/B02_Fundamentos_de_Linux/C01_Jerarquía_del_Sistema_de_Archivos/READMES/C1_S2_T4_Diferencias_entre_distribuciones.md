# T04 — Diferencias entre distribuciones

## Por qué importan las diferencias

Aunque Debian y RHEL siguen el FHS, difieren en detalles concretos: nombres de
archivos de log, rutas de configuración, herramientas por defecto, y convenciones
de nombrado. Un administrador que trabaja con ambas familias necesita conocer
estas diferencias para evitar errores al cambiar de entorno.

Las diferencias no son arbitrarias — reflejan decisiones de diseño, herencia
histórica, y preferencias de cada comunidad. El FHS define la estructura general,
pero deja libertad en los detalles.

---

## Logs del sistema

La diferencia más visible para un administrador:

| Propósito | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Autenticación (login, sudo, su) | `/var/log/auth.log` | `/var/log/secure` |
| Sistema general | `/var/log/syslog` | `/var/log/messages` |
| Kernel | `/var/log/kern.log` | En `/var/log/messages` (si rsyslog) o `journalctl -k` |
| Gestor de paquetes | `/var/log/dpkg.log` | `/var/log/dnf.log` |
| Arranque | `/var/log/boot.log` | `/var/log/boot.log` |
| Daemon de logs | rsyslog (por defecto) | rsyslog (por defecto) |

```bash
# Debian: ver intentos de login fallidos
grep "Failed password" /var/log/auth.log

# RHEL: mismo propósito, diferente archivo
grep "Failed password" /var/log/secure

# Alternativa portátil (funciona en ambos con systemd):
journalctl -u sshd | grep "Failed password"
```

> **Nota sobre kernel logs en RHEL**: RHEL no crea un archivo separado
> `kern.log`. Los mensajes del kernel van a `/var/log/messages` (si rsyslog
> está configurado para ello) o se consultan con `journalctl -k`. Ambas
> familias convergen en **journalctl** como la interfaz principal de logs.

---

## Gestión de paquetes

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Formato de paquete | `.deb` | `.rpm` |
| Gestor de alto nivel | `apt` / `apt-get` | `dnf` / `yum` |
| Gestor de bajo nivel | `dpkg` | `rpm` |
| Repositorios | `/etc/apt/sources.list.d/` | `/etc/yum.repos.d/` |
| Cache de paquetes | `/var/cache/apt/` | `/var/cache/dnf/` |
| Base de datos | `/var/lib/dpkg/` | `/var/lib/rpm/` |
| Grupo de desarrollo | `build-essential` | `"Development Tools"` |

```bash
# Debian: instalar herramientas de compilación
sudo apt install build-essential

# RHEL: equivalente
sudo dnf groupinstall "Development Tools"

# Debian: qué paquete provee un archivo
dpkg -S /usr/bin/gcc
# gcc-12: /usr/bin/gcc

# RHEL: equivalente
rpm -qf /usr/bin/gcc
# gcc-8.5.0-18.el9.x86_64
```

---

## Gestión de servicios

systemd es estándar en ambas, pero algunos nombres de servicios difieren:

| Servicio | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| SSH | `ssh.service` | `sshd.service` |
| Firewall | `ufw.service` (si instalado) | `firewalld.service` |
| NTP | `systemd-timesyncd` | `chronyd.service` |
| DNS resolver | `systemd-resolved` | NetworkManager DNS |
| Cron | `cron.service` | `crond.service` |
| Syslog | `rsyslog.service` | `rsyslog.service` |

```bash
# Debian: reiniciar SSH
sudo systemctl restart ssh

# RHEL: reiniciar SSH (nota la 'd')
sudo systemctl restart sshd

# Error común: usar "ssh" en RHEL
sudo systemctl restart ssh
# Unit ssh.service not found.
```

> **Truco**: si no recuerdas el nombre exacto, `systemctl list-unit-files | grep ssh`
> te muestra lo que hay instalado.

---

## Configuración de red

| Aspecto | Debian | Ubuntu | AlmaLinux/RHEL |
|---|---|---|---|
| Gestor principal | ifupdown o NetworkManager | netplan (que delega a NetworkManager o networkd) | NetworkManager |
| Archivos de config | `/etc/network/interfaces` | `/etc/netplan/*.yaml` | `/etc/NetworkManager/system-connections/` |
| Legacy config | — | — | `/etc/sysconfig/network-scripts/` (deprecado RHEL 9) |
| Herramienta CLI | `ifup`/`ifdown` o `nmcli` | `netplan apply` | `nmcli` |
| DNS resolver | `/etc/resolv.conf` (directo o symlink) | systemd-resolved → `/etc/resolv.conf` (symlink) | NetworkManager → `/etc/resolv.conf` |

> **Aclaración importante**: Netplan es una herramienta **específica de Ubuntu**, no de
> Debian. En Debian, la configuración de red se maneja con `ifupdown`
> (`/etc/network/interfaces`) en servidores, o con NetworkManager en instalaciones
> de escritorio. Es un error común confundir Debian con Ubuntu en este aspecto.

```bash
# Debian (ifupdown): ver configuración de red
cat /etc/network/interfaces
# auto eth0
# iface eth0 inet dhcp

# Ubuntu (netplan): ver configuración de red
cat /etc/netplan/01-netcfg.yaml
# network:
#   version: 2
#   ethernets:
#     eth0:
#       dhcp4: true

# RHEL (NetworkManager): ver conexiones
nmcli connection show
# NAME    UUID                                  TYPE      DEVICE
# eth0    a1b2c3d4-...                          ethernet  eth0

# Herramientas iproute2: funcionan igual en todas
ip addr show
ip route show
```

---

## Firewall

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Herramienta por defecto | ufw (si instalado, no siempre) | firewalld |
| Backend | nftables (Debian 11+/Ubuntu 21+) | nftables (RHEL 9+) |
| CLI principal | `ufw` | `firewall-cmd` |
| Configuración | `/etc/ufw/` | `/etc/firewalld/` |

```bash
# Debian: abrir puerto 80
sudo ufw allow 80/tcp
sudo ufw enable

# RHEL: abrir puerto 80
sudo firewall-cmd --add-port=80/tcp --permanent
sudo firewall-cmd --reload

# Ambos: nftables subyacente funciona igual
sudo nft list ruleset
```

En Debian, ufw no viene preinstalado en el servidor base — el firewall está
deshabilitado por defecto. En RHEL, firewalld viene activo por defecto.

---

## Usuarios y grupos

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| `adduser` | Script Perl interactivo (pide contraseña, datos, crea home) | Symlink a `useradd` (no interactivo) |
| Grupo de sudo | `sudo` | `wheel` |
| Shell por defecto (`useradd`) | `/bin/sh` | `/bin/bash` |
| UID range para usuarios | 1000–60000 | 1000–60000 |
| Defaults de `useradd` | `/etc/default/useradd` | `/etc/default/useradd` |
| Config adicional | `/etc/adduser.conf` (para el wrapper `adduser`) | — |

> **Corrección**: Ambas familias usan `UID_MAX=60000` en `/etc/login.defs`. El
> rango de UID para usuarios normales es **1000–60000 en ambas**. Ambas también
> tienen `/etc/default/useradd` para los defaults de `useradd`. La diferencia
> es que Debian tiene *adicionalmente* `/etc/adduser.conf` para su wrapper Perl.

```bash
# Debian: adduser es interactivo y amigable
sudo adduser newuser
# Adding user `newuser' ...
# Adding new group `newuser' (1001) ...
# Creating home directory `/home/newuser' ...
# (pide contraseña, nombre completo, etc.)

# RHEL: adduser es symlink a useradd (no interactivo)
sudo adduser newuser   # equivale a: sudo useradd newuser
sudo passwd newuser    # contraseña es un paso separado
# No crea home ni pide datos interactivamente sin flags
# Usar: sudo useradd -m -s /bin/bash newuser
```

```bash
# Debian: el grupo sudo tiene privilegios
grep sudo /etc/group
# sudo:x:27:usuario

# RHEL: el grupo wheel tiene privilegios
grep wheel /etc/group
# wheel:x:10:usuario

# Verificar en sudoers
grep -E '%sudo|%wheel' /etc/sudoers
# Debian: %sudo ALL=(ALL:ALL) ALL
# RHEL:   %wheel ALL=(ALL:ALL) ALL
```

---

## Rutas de configuración específicas

| Configuración | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Hostname estático | `/etc/hostname` | `/etc/hostname` |
| Locale | `/etc/default/locale` | `/etc/locale.conf` |
| Keyboard | `/etc/default/keyboard` | `/etc/vconsole.conf` |
| Repositorios | `/etc/apt/sources.list.d/` | `/etc/yum.repos.d/` |
| Crypto policies | N/A | `/etc/crypto-policies/` |

```bash
# Debian: configurar locale
cat /etc/default/locale
# LANG=en_US.UTF-8

# RHEL: configurar locale
cat /etc/locale.conf
# LANG=en_US.UTF-8

# Portátil (systemd):
localectl status
```

---

## SELinux vs AppArmor

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| MAC por defecto | AppArmor | SELinux |
| Estado | `aa-status` | `getenforce` / `sestatus` |
| Perfiles/Políticas | `/etc/apparmor.d/` | `/etc/selinux/` |
| Contextos en ls | N/A (usa perfil por app) | `ls -Z` muestra contexto |
| Modo permisivo | `aa-complain` | `setenforce 0` |

```bash
# Debian: verificar AppArmor
sudo aa-status
# 28 profiles are loaded.
# 26 profiles are in enforce mode.

# RHEL: verificar SELinux
getenforce
# Enforcing

sestatus
# SELinux status:                 enabled
# Current mode:                   enforcing
# Policy from config file:        targeted
```

SELinux y AppArmor se cubren en profundidad en el bloque de seguridad (B11).
Aquí solo importa saber que cada familia usa un MAC diferente y que afecta
permisos de archivos y procesos.

---

## Herramientas que difieren

### Presentes por defecto en una pero no en la otra

| Herramienta | Debian | RHEL | Propósito |
|---|---|---|---|
| `ip` | Sí | Sí | Red (reemplaza ifconfig) |
| `ss` | Sí | Sí | Sockets (reemplaza netstat) |
| `ifconfig` | No (instalar net-tools) | No (instalar net-tools) | Red legacy |
| `netstat` | No (instalar net-tools) | No (instalar net-tools) | Sockets legacy |
| `wget` | Sí | Sí | Descargas |
| `curl` | Sí (usualmente) | Sí | Transferencias |
| `vim` | `vim-tiny` (vi) | `vim-minimal` (vi) | Editor |
| `nano` | Sí | No (instalar) | Editor simple |

Las herramientas modernas (`ip`, `ss`) están en el paquete `iproute2` que
viene instalado en ambas familias. Las herramientas legacy (`ifconfig`,
`netstat`) están en `net-tools` que ya no se instala por defecto.

### Diferencias en la misma herramienta

```bash
# Debian: "vi" es vim-tiny (muy reducido)
vi --version | head -1
# VIM - Vi IMproved 9.0 (... Tiny version ...)

# RHEL: "vi" es vim-minimal (algo más completo)
vi --version | head -1
# VIM - Vi IMproved 8.2 (... without GUI ...)
```

---

## Estructura de /lib

| Directorio | Debian | RHEL |
|---|---|---|
| `/lib` | Symlink a `/usr/lib` | Symlink a `/usr/lib` |
| `/lib64` | Symlink a `/usr/lib64` | Symlink a `/usr/lib64` |
| Multiarch | Sí (`/usr/lib/x86_64-linux-gnu/`) | No (todo en `/usr/lib64/`) |

```bash
# Debian: usa directorios multiarch
ls /usr/lib/x86_64-linux-gnu/libc.so*
# /usr/lib/x86_64-linux-gnu/libc.so.6

# RHEL: usa /lib64 directamente
ls /usr/lib64/libc.so*
# /usr/lib64/libc.so.6
```

Debian implementa **multiarch**: las bibliotecas de cada arquitectura están en
subdirectorios separados (`x86_64-linux-gnu`, `aarch64-linux-gnu`), lo que
permite instalar bibliotecas de múltiples arquitecturas simultáneamente (útil
para cross-compilation). RHEL no usa este esquema.

---

## Cómo escribir scripts portátiles

Para que un script funcione en ambas familias:

```bash
# 1. Detectar la familia de distribución
if [ -f /etc/debian_version ]; then
    DISTRO_FAMILY="debian"
elif [ -f /etc/redhat-release ]; then
    DISTRO_FAMILY="rhel"
fi

# 2. Usar herramientas portátiles
# En vez de:  ifconfig eth0
# Usar:       ip addr show eth0

# 3. Usar paths de configuración correctos
case "$DISTRO_FAMILY" in
    debian)
        AUTH_LOG="/var/log/auth.log"
        SUDO_GROUP="sudo"
        ;;
    rhel)
        AUTH_LOG="/var/log/secure"
        SUDO_GROUP="wheel"
        ;;
esac

# 4. Preferir journalctl cuando sea posible
# En vez de:  tail -f /var/log/auth.log  o  tail -f /var/log/secure
# Portátil:   journalctl -u sshd -f
```

### Detección con /etc/os-release (método más robusto)

```bash
# Sourcear convierte los campos en variables de shell
. /etc/os-release
echo "ID: $ID"
echo "Version: $VERSION_ID"

case "$ID" in
    debian|ubuntu)
        echo "Acción para familia Debian"
        ;;
    almalinux|rocky|rhel|centos|fedora)
        echo "Acción para familia RHEL"
        ;;
    *)
        echo "Distribución no reconocida: $ID"
        ;;
esac
```

### Archivos de detección

```bash
# Debian
cat /etc/debian_version
# 12.5

# RHEL/Alma
cat /etc/redhat-release
# AlmaLinux release 9.3 (Shamrock Pampas Cat)

# Portátil (todas las distros modernas):
cat /etc/os-release
# NAME="Debian GNU/Linux"    o    NAME="AlmaLinux"
# VERSION_ID="12"            o    VERSION_ID="9.3"
# ID=debian                  o    ID=almalinux
# ID_LIKE=debian             o    ID_LIKE="rhel centos fedora"
```

`/etc/os-release` es la forma estándar de detectar la distribución. Está
definido por systemd y presente en todas las distribuciones modernas.

---

## Lab — Diferencias entre distribuciones

### Objetivo

Comparar Debian y AlmaLinux lado a lado: identificar la distribución
de forma portable, comparar logs, paquetes, rutas de configuración, y
practicar la escritura de scripts que funcionan en ambas familias.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Identificar la distribución

#### Paso 1.1: /etc/os-release (estándar)

```bash
echo "=== Debian ==="
docker compose exec debian-dev cat /etc/os-release

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev cat /etc/os-release
```

`/etc/os-release` es la forma estándar de identificar la distribución.
Los campos clave: `ID`, `ID_LIKE`, `VERSION_ID`.

#### Paso 1.2: Archivos específicos

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

#### Paso 1.3: Campos ID e ID_LIKE

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

### Parte 2 — Comparar archivos y rutas

#### Paso 2.1: Archivos de log

```bash
echo "=== Debian: logs ==="
docker compose exec debian-dev ls /var/log/ 2>/dev/null

echo ""
echo "=== AlmaLinux: logs ==="
docker compose exec alma-dev ls /var/log/ 2>/dev/null
```

Busca las diferencias: `auth.log` vs `secure`, `syslog` vs `messages`,
`dpkg.log` vs `dnf.log`.

#### Paso 2.2: Gestor de paquetes

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

#### Paso 2.3: Repositorios

```bash
echo "=== Debian: /etc/apt/ ==="
docker compose exec debian-dev bash -c '
ls /etc/apt/sources.list.d/ 2>/dev/null
cat /etc/apt/sources.list 2>/dev/null | grep -v "^#" | head -3
'

echo ""
echo "=== AlmaLinux: /etc/yum.repos.d/ ==="
docker compose exec alma-dev bash -c 'ls /etc/yum.repos.d/'
```

#### Paso 2.4: Glibc y bibliotecas

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
`/usr/lib64/`. Las versiones de glibc también difieren.

#### Paso 2.5: Locale

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

### Parte 3 — Scripts portables

#### Paso 3.1: Detección de familia

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

#### Paso 3.2: Paths condicionales

```bash
docker compose exec debian-dev bash -c '
# Determinar familia y configurar paths
if [ -f /etc/debian_version ]; then
    AUTH_LOG="/var/log/auth.log"
    PKG_COUNT=$(dpkg -l | grep "^ii" | wc -l)
else
    AUTH_LOG="/var/log/secure"
    PKG_COUNT=$(rpm -qa | wc -l)
fi

echo "Log de autenticación: $AUTH_LOG"
echo "Existe: $([ -f "$AUTH_LOG" ] && echo sí || echo no)"
echo "Paquetes: $PKG_COUNT"
'
```

#### Paso 3.3: Herramientas portables

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
idénticamente en ambas familias. Preferirlas sobre las legacy
(`ifconfig`, `netstat`).

#### Paso 3.4: Usar os-release en scripts

```bash
docker compose exec debian-dev bash -c '
# Forma portable y robusta
. /etc/os-release
echo "ID: $ID"
echo "Version: $VERSION_ID"

case "$ID" in
    debian|ubuntu)
        echo "Acción para familia Debian"
        ;;
    almalinux|rocky|rhel|centos|fedora)
        echo "Acción para familia RHEL"
        ;;
    *)
        echo "Distribución no reconocida: $ID"
        ;;
esac
'
```

Sourcear (`. /etc/os-release`) convierte sus campos en variables de shell.
Es la forma más robusta de detectar la distribución en scripts.

---

## Ejercicios

### Ejercicio 1 — Identificar la distribución con os-release

En cada contenedor, examina `/etc/os-release` y responde:

```bash
docker compose exec debian-dev bash -c '. /etc/os-release; echo "ID=$ID ID_LIKE=$ID_LIKE VERSION_ID=$VERSION_ID"'
docker compose exec alma-dev bash -c '. /etc/os-release; echo "ID=$ID ID_LIKE=$ID_LIKE VERSION_ID=$VERSION_ID"'
```

**Predicción**: ¿Qué valor tendrá `ID_LIKE` en cada contenedor?

<details><summary>Respuesta</summary>

- **Debian**: `ID=debian`, `ID_LIKE=` (vacío o no presente — Debian *es* la referencia)
- **AlmaLinux**: `ID=almalinux`, `ID_LIKE="rhel centos fedora"` (indica su linaje)

`ID_LIKE` solo tiene valor cuando la distribución *deriva* de otra.
Debian no deriva de nadie, así que no define `ID_LIKE`.

</details>

### Ejercicio 2 — Localizar el log de autenticación

Encuentra dónde cada distribución registra eventos de autenticación:

```bash
docker compose exec debian-dev bash -c 'ls -la /var/log/auth* /var/log/secure* 2>/dev/null || echo "No se encontró"'
docker compose exec alma-dev bash -c 'ls -la /var/log/auth* /var/log/secure* 2>/dev/null || echo "No se encontró"'
```

**Predicción**: ¿Qué archivo existirá en cada uno?

<details><summary>Respuesta</summary>

- **Debian**: `/var/log/auth.log`
- **AlmaLinux**: `/var/log/secure`

En contenedores sin rsyslog corriendo, es posible que ninguno exista.
En ese caso, la alternativa portátil es `journalctl _COMM=sshd` (si
systemd está funcionando) o consultar el journal directamente.

</details>

### Ejercicio 3 — Comparar el gestor de paquetes

Muestra cuántos paquetes hay instalados en cada contenedor:

```bash
docker compose exec debian-dev bash -c 'dpkg -l | grep "^ii" | wc -l'
docker compose exec alma-dev bash -c 'rpm -qa | wc -l'
```

**Predicción**: ¿Qué contenedor tendrá más paquetes instalados?

<details><summary>Respuesta</summary>

Depende de las imágenes base del curso, pero típicamente:
- Debian tiende a tener más paquetes porque instala más dependencias por defecto
- AlmaLinux (minimal) suele tener una base más reducida

Lo importante es la diferencia en herramientas: `dpkg -l | grep "^ii"` en
Debian vs `rpm -qa` en RHEL. El filtro `"^ii"` es necesario porque `dpkg -l`
incluye paquetes desinstalados (marcados `rc` = config residual).

</details>

### Ejercicio 4 — Encontrar dónde vive libc

Localiza `libc.so.6` en ambos contenedores:

```bash
docker compose exec debian-dev find /usr/lib -name "libc.so.6" 2>/dev/null
docker compose exec alma-dev find /usr/lib -name "libc.so.6" 2>/dev/null
```

**Predicción**: ¿En qué subdirectorio estará libc en cada uno?

<details><summary>Respuesta</summary>

- **Debian**: `/usr/lib/x86_64-linux-gnu/libc.so.6` (multiarch)
- **AlmaLinux**: `/usr/lib64/libc.so.6`

Debian usa el esquema **multiarch** donde cada arquitectura tiene su propio
subdirectorio (`x86_64-linux-gnu`, `aarch64-linux-gnu`). Esto permite
instalar bibliotecas de múltiples arquitecturas simultáneamente.
RHEL simplemente usa `/usr/lib64/` para bibliotecas de 64 bits.

</details>

### Ejercicio 5 — Verificar el grupo de sudo

¿Qué grupo otorga privilegios de sudo en cada distribución?

```bash
docker compose exec debian-dev bash -c 'grep -E "^%sudo|^%wheel" /etc/sudoers 2>/dev/null || echo "Verificar /etc/sudoers.d/"'
docker compose exec alma-dev bash -c 'grep -E "^%sudo|^%wheel" /etc/sudoers 2>/dev/null || echo "Verificar /etc/sudoers.d/"'
```

**Predicción**: ¿Qué grupo aparecerá en cada contenedor?

<details><summary>Respuesta</summary>

- **Debian**: `%sudo ALL=(ALL:ALL) ALL` — grupo `sudo` (GID 27)
- **AlmaLinux**: `%wheel ALL=(ALL) ALL` — grupo `wheel` (GID 10)

Para scripts portátiles, determina el grupo correcto según la familia:
```bash
. /etc/os-release
case "$ID" in
    debian|ubuntu) SUDO_GROUP="sudo" ;;
    *)             SUDO_GROUP="wheel" ;;
esac
```

</details>

### Ejercicio 6 — Comparar adduser vs useradd

Verifica qué es `adduser` en cada distribución:

```bash
docker compose exec debian-dev bash -c 'file $(command -v adduser) && head -3 $(command -v adduser)'
docker compose exec alma-dev bash -c 'file $(command -v adduser) && ls -la $(command -v adduser)'
```

**Predicción**: ¿`adduser` será el mismo tipo de archivo en ambos?

<details><summary>Respuesta</summary>

- **Debian**: `adduser` es un **script Perl** independiente. Las primeras líneas
  mostrarán `#!/usr/bin/perl`. Proporciona una experiencia interactiva: pide
  contraseña, crea home, copia skel, solicita datos del usuario.
- **AlmaLinux**: `adduser` es un **symlink a `useradd`** (`/usr/sbin/adduser -> useradd`).
  No es interactivo; hay que pasar flags (`-m`, `-s`) y usar `passwd` aparte.

Esta diferencia es fuente frecuente de confusión al cambiar de familia.

</details>

### Ejercicio 7 — Locale: diferentes archivos, mismo resultado

Compara dónde se configura el locale:

```bash
docker compose exec debian-dev bash -c '
echo "--- /etc/default/locale ---"
cat /etc/default/locale 2>/dev/null || echo "no existe"
echo "--- /etc/locale.conf ---"
cat /etc/locale.conf 2>/dev/null || echo "no existe"
echo "--- localectl ---"
localectl status 2>/dev/null || echo "localectl no disponible"
'

docker compose exec alma-dev bash -c '
echo "--- /etc/default/locale ---"
cat /etc/default/locale 2>/dev/null || echo "no existe"
echo "--- /etc/locale.conf ---"
cat /etc/locale.conf 2>/dev/null || echo "no existe"
echo "--- localectl ---"
localectl status 2>/dev/null || echo "localectl no disponible"
'
```

**Predicción**: ¿Qué archivo existirá en cada contenedor?

<details><summary>Respuesta</summary>

- **Debian**: `/etc/default/locale` (no tendrá `/etc/locale.conf`)
- **AlmaLinux**: `/etc/locale.conf` (no tendrá `/etc/default/locale`)

`localectl status` funciona en ambos (si systemd está activo) y es la
forma portátil de consultar el locale. En contenedores sin systemd
completo, `localectl` puede no estar disponible.

</details>

### Ejercicio 8 — Escribir un script portable de detección

Crea un script que funcione en ambos contenedores y reporte información del sistema:

```bash
SCRIPT='
#!/bin/sh
. /etc/os-release

echo "=== Sistema ==="
echo "Distribución: $PRETTY_NAME"
echo "Familia ID: $ID"

# Determinar paths según familia
case "$ID" in
    debian|ubuntu)
        AUTH_LOG="/var/log/auth.log"
        SUDO_GRP="sudo"
        PKG_CMD="dpkg -l | grep ^ii | wc -l"
        ;;
    almalinux|rocky|rhel|centos|fedora)
        AUTH_LOG="/var/log/secure"
        SUDO_GRP="wheel"
        PKG_CMD="rpm -qa | wc -l"
        ;;
esac

echo "Log auth: $AUTH_LOG (existe: $([ -f "$AUTH_LOG" ] && echo sí || echo no))"
echo "Grupo sudo: $SUDO_GRP"
echo "Paquetes: $(eval "$PKG_CMD")"
echo "Kernel: $(uname -r)"
'

docker compose exec debian-dev bash -c "$SCRIPT"
echo "---"
docker compose exec alma-dev bash -c "$SCRIPT"
```

**Predicción**: ¿El mismo script producirá salida diferente en cada contenedor?

<details><summary>Respuesta</summary>

Sí. El script usa `/etc/os-release` para detectar la familia y adaptar
las variables. La salida diferirá en:
- Nombre de la distribución
- Path del log de autenticación
- Nombre del grupo de sudo
- Número de paquetes (y herramienta usada para contarlos)
- La versión de kernel será la **misma** en ambos contenedores (comparten
  el kernel del host — los contenedores no tienen kernel propio)

</details>

### Ejercicio 9 — Verificar qué herramientas están disponibles

Comprueba la disponibilidad de herramientas comunes:

```bash
for tool in ip ss ifconfig netstat curl wget nano vim; do
    DEB=$(docker compose exec debian-dev bash -c "command -v $tool >/dev/null 2>&1 && echo SÍ || echo NO")
    ALMA=$(docker compose exec alma-dev bash -c "command -v $tool >/dev/null 2>&1 && echo SÍ || echo NO")
    printf "%-12s Debian: %-4s AlmaLinux: %s\n" "$tool" "$DEB" "$ALMA"
done
```

**Predicción**: ¿Qué herramientas estarán presentes en ambos y cuáles solo en uno?

<details><summary>Respuesta</summary>

Herramientas del paquete `iproute2` (`ip`, `ss`) estarán en **ambos**.
Herramientas legacy (`ifconfig`, `netstat` del paquete `net-tools`) no
estarán en **ninguno** (ya no se instalan por defecto).

`nano` estará en Debian pero probablemente no en AlmaLinux.
`curl` y `wget` dependen de las imágenes del curso.
`vim` (como `vi`) existirá en ambos: `vim-tiny` en Debian, `vim-minimal`
en AlmaLinux.

La lección: `ip` y `ss` son las herramientas portátiles que funcionan
en todas las distribuciones modernas.

</details>

### Ejercicio 10 — Tabla resumen de diferencias

Ejecuta este bloque para generar una tabla comparativa automática:

```bash
COMPARE='
. /etc/os-release
echo "Distro: $PRETTY_NAME"
echo "ID: $ID"
echo "Pkg format: $([ "$ID" = "debian" ] && echo deb || echo rpm)"
echo "Auth log: $([ -f /var/log/auth.log ] && echo auth.log || echo secure)"
echo "Sudo group: $(grep -q "^%sudo" /etc/sudoers 2>/dev/null && echo sudo || echo wheel)"
echo "Locale file: $([ -f /etc/default/locale ] && echo /etc/default/locale || echo /etc/locale.conf)"
echo "libc path: $(dirname $(find /usr/lib -name "libc.so.6" 2>/dev/null | head -1))"
echo "adduser type: $(file $(command -v adduser) 2>/dev/null | grep -q "script" && echo "script Perl" || echo "symlink a useradd")"
'

echo "=== Debian ==="
docker compose exec debian-dev bash -c "$COMPARE"
echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c "$COMPARE"
```

**Predicción**: Antes de ejecutar, intenta completar la tabla de memoria:

| Aspecto | Debian | AlmaLinux |
|---|---|---|
| Formato paquete | ¿? | ¿? |
| Log autenticación | ¿? | ¿? |
| Grupo sudo | ¿? | ¿? |
| Archivo locale | ¿? | ¿? |
| Path de libc | ¿? | ¿? |
| Tipo de adduser | ¿? | ¿? |

<details><summary>Respuesta</summary>

| Aspecto | Debian | AlmaLinux |
|---|---|---|
| Formato paquete | `.deb` | `.rpm` |
| Log autenticación | `/var/log/auth.log` | `/var/log/secure` |
| Grupo sudo | `sudo` | `wheel` |
| Archivo locale | `/etc/default/locale` | `/etc/locale.conf` |
| Path de libc | `/usr/lib/x86_64-linux-gnu/` | `/usr/lib64/` |
| Tipo de adduser | Script Perl (interactivo) | Symlink a useradd (no interactivo) |

Estas seis diferencias son las que más frecuentemente causan confusión al
cambiar entre familias. La clave para scripts portátiles: detectar con
`/etc/os-release` y usar variables condicionales.

</details>

---

## Conceptos reforzados

1. `/etc/os-release` es la forma **estándar** de identificar la distribución.
   Sourcear el archivo (`. /etc/os-release`) convierte los campos en variables
   de shell.

2. Las diferencias más visibles: logs (`auth.log` vs `secure`), paquetes
   (`apt`/`dpkg` vs `dnf`/`rpm`), bibliotecas (multiarch vs lib64), locale
   (`/etc/default/locale` vs `/etc/locale.conf`).

3. Las herramientas de red modernas (`ip`, `ss`) del paquete `iproute2`
   funcionan **idénticamente** en ambas familias. Son preferibles a las legacy
   (`ifconfig`, `netstat`).

4. **Netplan es de Ubuntu, no de Debian**. Debian usa `ifupdown` o
   NetworkManager según el tipo de instalación. No confundir las dos
   distribuciones aunque pertenezcan a la misma familia.

5. `adduser` es radicalmente diferente: en Debian es un script Perl interactivo;
   en RHEL es un simple symlink a `useradd`.

6. Para scripts portátiles: detectar la familia con `/etc/os-release` (método
   robusto) o archivos específicos (`/etc/debian_version`, `/etc/redhat-release`),
   y usar variables condicionales para paths y comandos.

7. Ambas familias tienen `UID_MAX=60000` en `/etc/login.defs` y
   `/etc/default/useradd`. La diferencia real está en que Debian tiene
   adicionalmente `/etc/adduser.conf` para su wrapper.

8. El FHS define la estructura **general** pero deja libertad en los detalles.
   Ambas familias convergen cada vez más gracias a systemd y al /usr merge.
