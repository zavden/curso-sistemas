# T04 — Diferencias entre distribuciones

## Por qué importan las diferencias

Aunque Debian y RHEL siguen el FHS, difieren en detalles concretos: nombres de
archivos de log, rutas de configuración, herramientas por defecto, y convenciones
de nombrado. Un administrador que trabaja con ambas familias necesita conocer
estas diferencias para evitar errores al cambiar de entorno.

Las diferencias no son arbitrarias — reflejan decisiones de diseño, herencia
histórica, y preferencias de cada comunidad. El FHS define la estructura general,
pero deja libertad en los detalles.

## Logs del sistema

La diferencia más visible para un administrador:

| Propósito | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Autenticación (login, sudo, su) | `/var/log/auth.log` | `/var/log/secure` |
| Sistema general | `/var/log/syslog` | `/var/log/messages` |
| Kernel | `/var/log/kern.log` | via `journalctl -k` |
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

La tendencia es que ambas familias converjan en **journalctl** como la
interfaz principal de logs, con los archivos de texto como complemento
gestionado por rsyslog.

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

## Configuración de red

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Gestor principal | netplan (moderno) o ifupdown (legacy) | NetworkManager |
| Archivos de config | `/etc/netplan/*.yaml` | `/etc/NetworkManager/system-connections/` |
| Legacy config | `/etc/network/interfaces` | `/etc/sysconfig/network-scripts/` (deprecado en RHEL 9) |
| Herramienta CLI | `netplan apply` | `nmcli` |
| DNS resolver | systemd-resolved → `/etc/resolv.conf` (symlink) | NetworkManager → `/etc/resolv.conf` |

```bash
# Debian (netplan): ver configuración de red
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

# Ambos: herramientas del stack iproute2 funcionan igual
ip addr show
ip route show
```

## Firewall

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Herramienta por defecto | ufw (si instalado, no siempre) | firewalld |
| Backend | iptables/nftables | nftables (RHEL 9+) |
| CLI principal | `ufw` | `firewall-cmd` |
| Configuración | `/etc/ufw/` | `/etc/firewalld/` |

```bash
# Debian: abrir puerto 80
sudo ufw allow 80/tcp
sudo ufw enable

# RHEL: abrir puerto 80
sudo firewall-cmd --add-port=80/tcp --permanent
sudo firewall-cmd --reload

# Ambos: iptables/nftables subyacente funciona igual
sudo iptables -L -n
sudo nft list ruleset
```

En Debian, ufw no viene preinstalado en el servidor base — el firewall está
deshabilitado por defecto. En RHEL, firewalld viene activo por defecto.

## Usuarios y grupos

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Crear usuario interactivo | `adduser` (wrapper amigable) | `useradd` (misma herramienta) |
| Grupo de sudo | `sudo` | `wheel` |
| Shell por defecto (useradd) | `/bin/sh` | `/bin/bash` |
| UID range para usuarios | 1000-59999 | 1000-60000 |
| Archivo de defaults | `/etc/adduser.conf` | `/etc/default/useradd` |

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

```bash
# Debian: adduser es interactivo y amigable
sudo adduser newuser
# Adding user `newuser' ...
# Adding new group `newuser' (1001) ...
# Adding new user `newuser' (1001) with group `newuser' ...
# Creating home directory `/home/newuser' ...
# (pide contraseña, nombre completo, etc.)

# RHEL: useradd es la herramienta estándar
sudo useradd -m -s /bin/bash newuser
sudo passwd newuser
# (dos comandos separados)
```

## Rutas de configuración específicas

| Configuración | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Hostname estático | `/etc/hostname` | `/etc/hostname` |
| Config de red legacy | `/etc/network/interfaces` | `/etc/sysconfig/network-scripts/` |
| Default para useradd | `/etc/adduser.conf` | `/etc/default/useradd` |
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

## Ejercicios

### Ejercicio 1 — Identificar la distribución

```bash
# En ambos contenedores del lab
docker compose exec debian-dev cat /etc/os-release
docker compose exec alma-dev cat /etc/os-release

# Comparar los campos ID, ID_LIKE, VERSION_ID
```

### Ejercicio 2 — Comparar archivos de log

```bash
# ¿Qué archivos de log existen en cada distribución?
docker compose exec debian-dev ls /var/log/
docker compose exec alma-dev ls /var/log/

# ¿Cuáles existen en una y no en la otra?
```

### Ejercicio 3 — Verificar servicios

```bash
# ¿Cómo se llama el servicio SSH en cada uno?
docker compose exec debian-dev systemctl list-unit-files | grep ssh
docker compose exec alma-dev systemctl list-unit-files | grep ssh

# ¿Qué grupo tiene privilegios de sudo?
docker compose exec debian-dev grep -E '%sudo|%wheel' /etc/sudoers
docker compose exec alma-dev grep -E '%sudo|%wheel' /etc/sudoers
```
