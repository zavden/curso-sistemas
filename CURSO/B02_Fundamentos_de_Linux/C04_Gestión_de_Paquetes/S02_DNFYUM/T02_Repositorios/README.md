# T02 — Repositorios

## Archivos .repo

Los repositorios en el ecosistema RHEL se configuran como archivos `.repo` en
`/etc/yum.repos.d/`:

```bash
ls /etc/yum.repos.d/
# almalinux-baseos.repo
# almalinux-appstream.repo
# almalinux-extras.repo
# almalinux-crb.repo
# ...

# En RHEL:
# redhat.repo (gestionado por subscription-manager)
```

### Formato de un archivo .repo

```ini
# /etc/yum.repos.d/almalinux-baseos.repo
[baseos]
name=AlmaLinux $releasever - BaseOS
mirrorlist=https://mirrors.almalinux.org/mirrorlist/$releasever/baseos
# baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/
enabled=1
gpgcheck=1
countme=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-AlmaLinux-9
metadata_expire=86400
```

### Campos del archivo .repo

| Campo | Significado |
|---|---|
| `[baseos]` | ID único del repositorio (usado en comandos dnf) |
| `name` | Nombre descriptivo |
| `baseurl` | URL directa al repositorio |
| `mirrorlist` | URL que devuelve una lista de mirrors |
| `metalink` | URL con metadatos del repositorio y checksums |
| `enabled` | 1 = activo, 0 = desactivado |
| `gpgcheck` | 1 = verificar firmas GPG |
| `gpgkey` | Ruta a la clave GPG pública |
| `metadata_expire` | Cuándo re-descargar metadatos (segundos) |
| `priority` | Prioridad (menor = más prioritario, con plugin priorities) |
| `module_hotfixes` | 1 = ignorar filtros de módulos |
| `sslverify` | 1 = verificar certificado SSL |
| `skip_if_unavailable` | 1 = no fallar si el repo no responde |

### Variables en archivos .repo

```bash
# $releasever → versión de la release (9, 8)
# $basearch   → arquitectura (x86_64, aarch64)
# $infra      → tipo de infraestructura

# Estas variables se resuelven automáticamente:
rpm -E %{rhel}        # → 9
rpm -E %{_arch}       # → x86_64

# baseurl con variables:
# https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/
```

### baseurl vs mirrorlist vs metalink

```bash
# baseurl: URL fija a un solo servidor
baseurl=https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/

# mirrorlist: URL que devuelve una lista de mirrors
# dnf elige el más rápido automáticamente
mirrorlist=https://mirrors.almalinux.org/mirrorlist/9/baseos

# metalink: como mirrorlist pero incluye checksums para verificar
# la integridad de los metadatos del repositorio
metalink=https://mirrors.fedoraproject.org/metalink?repo=epel-9&arch=x86_64

# Prioridad: metalink > mirrorlist > baseurl
# mirrorlist/metalink son preferibles por redundancia y velocidad
```

## Repositorios estándar de AlmaLinux/RHEL 9

| Repositorio | Contenido | Habilitado |
|---|---|---|
| BaseOS | Paquetes base del sistema (kernel, glibc, systemd) | Sí |
| AppStream | Aplicaciones y herramientas (nginx, python, php) | Sí |
| CRB | CodeReady Builder — headers, libs de desarrollo | No por defecto |
| Extras | Paquetes adicionales de AlmaLinux | Sí |
| HighAvailability | Clustering (pacemaker, corosync) | No |
| RT / NFV | Kernel real-time | No |

```bash
# Ver repositorios habilitados
dnf repolist
# repo id          repo name
# appstream        AlmaLinux 9 - AppStream
# baseos           AlmaLinux 9 - BaseOS
# extras           AlmaLinux 9 - Extras

# Ver todos (incluidos deshabilitados)
dnf repolist all

# Habilitar un repo
sudo dnf config-manager --set-enabled crb

# Deshabilitar un repo
sudo dnf config-manager --set-disabled extras
```

### BaseOS vs AppStream

```
BaseOS:
  - Paquetes core del sistema operativo
  - Soporte durante toda la vida de la release (10 años en RHEL)
  - Solo paquetes RPM tradicionales
  - Ejemplo: kernel, bash, systemd, openssh

AppStream:
  - Aplicaciones, herramientas, lenguajes
  - Puede tener múltiples versiones via "module streams"
  - RPMs tradicionales + módulos
  - Ejemplo: nginx, python, nodejs, php, postgresql
```

## GPG keys

```bash
# Las claves GPG de AlmaLinux se instalan en:
ls /etc/pki/rpm-gpg/
# RPM-GPG-KEY-AlmaLinux-9

# Ver una clave
rpm -qi gpg-pubkey-* | head -20

# Importar una clave manualmente
sudo rpm --import https://www.redhat.com/security/data/fd431d51.txt

# Verificar que un paquete está firmado
rpm -K paquete.rpm
# paquete.rpm: digests signatures OK

# Si gpgcheck=1 y un paquete no tiene firma válida:
# Public key for paquete.rpm is not installed
# → Importar la clave del repositorio
```

## EPEL — Extra Packages for Enterprise Linux

EPEL es el repositorio de la comunidad Fedora que proporciona paquetes
adicionales para RHEL que no están en los repos oficiales:

```bash
# Instalar EPEL
sudo dnf install epel-release

# En AlmaLinux/Rocky, epel-release está en Extras
# En RHEL, hay que descargarlo:
# sudo dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

# Después de instalar:
dnf repolist | grep epel
# epel    Extra Packages for Enterprise Linux 9 - x86_64

# EPEL contiene paquetes que en Debian estarían en main/universe:
# htop, jq, tmux, nginx (versión más reciente), etc.
```

### EPEL + CRB

Muchos paquetes de EPEL dependen de paquetes en CRB. Es común habilitarlos
juntos:

```bash
# Habilitar CRB (necesario para muchas dependencias de EPEL)
sudo dnf config-manager --set-enabled crb

# Instalar EPEL
sudo dnf install epel-release

# Ahora se puede instalar software que antes no estaba disponible
sudo dnf install htop jq bat ripgrep
```

## Habilitar y deshabilitar repos temporalmente

```bash
# Instalar desde un repo deshabilitado (una sola vez)
sudo dnf install --enablerepo=crb package-name

# Instalar ignorando un repo problemático
sudo dnf install --disablerepo=epel package-name

# Útil cuando un mirror está caído:
sudo dnf update --disablerepo=epel
```

## Agregar un repositorio de terceros

```bash
# Método 1: dnf config-manager
sudo dnf config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

# Método 2: crear el .repo manualmente
sudo tee /etc/yum.repos.d/docker-ce.repo << 'EOF'
[docker-ce-stable]
name=Docker CE Stable - $basearch
baseurl=https://download.docker.com/linux/centos/$releasever/$basearch/stable
enabled=1
gpgcheck=1
gpgkey=https://download.docker.com/linux/centos/gpg
EOF

sudo dnf makecache
```

### COPR — Community Projects (Fedora)

COPR es el equivalente de los PPAs de Ubuntu para Fedora/RHEL:

```bash
# Habilitar un repositorio COPR
sudo dnf copr enable user/project

# Ejemplo
sudo dnf copr enable atim/starship
sudo dnf install starship

# Deshabilitar
sudo dnf copr disable user/project

# Listar COPRs habilitados
dnf copr list
```

## Prioridades de repositorios

Si un paquete existe en múltiples repos, se puede usar prioridades para
controlar cuál se prefiere:

```bash
# En el archivo .repo:
[mi-repo]
name=Mi Repositorio
baseurl=https://mi-server/repo/
enabled=1
gpgcheck=0
priority=10    # menor = más prioritario (default = 99)

# Ejemplo: dar prioridad a un repo interno sobre EPEL
# mi-repo: priority=10
# epel: priority=99 (default)
# Si el paquete existe en ambos, se usa el de mi-repo
```

## Diferencias con APT

| Aspecto | APT (Debian) | DNF (RHEL) |
|---|---|---|
| Configuración | `/etc/apt/sources.list.d/` | `/etc/yum.repos.d/` |
| Formato | one-line o DEB822 | INI (.repo) |
| Claves GPG | `/usr/share/keyrings/` | `/etc/pki/rpm-gpg/` |
| Repos comunitarios | PPAs (Ubuntu) | EPEL, COPR |
| Prioridades | apt pinning (Pin-Priority) | priority= en .repo |
| Mirrors | No automático | mirrorlist/metalink |
| Verificación | Signed-By por repo | gpgkey por repo |

---

## Ejercicios

### Ejercicio 1 — Explorar repositorios

```bash
# Listar repos habilitados
dnf repolist

# Listar todos (incluidos deshabilitados)
dnf repolist all

# Ver detalles de un repo
dnf repoinfo baseos
```

### Ejercicio 2 — EPEL

```bash
# ¿EPEL está instalado?
rpm -q epel-release

# Si no:
# sudo dnf install epel-release

# ¿Cuántos paquetes hay en EPEL?
dnf repoinfo epel 2>/dev/null | grep "Repo-pkgs"
```

### Ejercicio 3 — Inspeccionar un .repo

```bash
# Ver el contenido de un repo
cat /etc/yum.repos.d/almalinux-baseos.repo 2>/dev/null || \
    ls /etc/yum.repos.d/ | head -5

# ¿Usa baseurl o mirrorlist?
# ¿Está habilitado gpgcheck?
# ¿Dónde está la clave GPG?
```
