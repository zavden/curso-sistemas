# T02 — Repositorios

## Anatomía del sistema de repositorios en RHEL

```
/etc/yum.repos.d/              ← directorio de archivos .repo
├── almalinux-baseos.repo       ← repos BaseOS
├── almalinux-appstream.repo    ← repos AppStream
├── almalinux-extras.repo       ← extras
├── epel.repo                   ← EPEL (si está instalado)
└── *.repo                      ← repos de terceros

/var/cache/dnf/                 ← caché de metadata y .rpm
├── almalinux-baseos-*/         ← metadata descomprimido
├── epel-*/                     ← ...
└── *.solv                      ← índice de libsolv

/etc/pki/rpm-gpg/               ← claves GPG públicas
├── RPM-GPG-KEY-AlmaLinux-9
└── RPM-GPG-KEY-EPEL-9
```

```
Flujo:
dnf install nginx
  │
  ├─[1] Leer /etc/yum.repos.d/*.repo
  │     Busca nginx en repos habilitados
  ├─[2] libsolv resuelve dependencias
  ├─[3] Descarga .rpm → /var/cache/dnf/
  └─[4] rpm -ivh nginx*.rpm   (instala con rpm)
```

## Archivos .repo (formato INI)

```bash
ls /etc/yum.repos.d/
```

```ini
# /etc/yum.repos.d/almalinux-baseos.repo
[baseos]                                    # ID único del repo
name=AlmaLinux $releasever - BaseOS         # Nombre descriptivo
mirrorlist=https://mirrors.almalinux.org/mirrorlist/$releasever/baseos
# baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/
enabled=1                                   # 1=habilitado, 0=deshabilitado
gpgcheck=1                                  # Verificar firma GPG del paquete
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-AlmaLinux-9
metadata_expire=86400                       # Re-descargar metadata cada 24h
countme=1                                   # Contador anónimo para estadísticas
module_hotfixes=1                           # Ignorar filters de módulo stream
```

### Campos del archivo .repo

| Campo | Ejemplo | Significado |
|---|---|---|
| `[ID]` | `[baseos]` | Identificador único usado en comandos dnf |
| `name` | `AlmaLinux $releasever - BaseOS` | Nombre legible |
| `baseurl` | `https://repo.../$releasever/BaseOS/` | URL fija al repositorio |
| `mirrorlist` | `https://mirrors.../mirrorlist/...` | URL que devuelve lista de mirrors |
| `metalink` | `https://mirrors.fedoraproject.org/metalink?...` | Lista de mirrors + checksums |
| `enabled` | `0` o `1` | Habilitado o no |
| `gpgcheck` | `0` o `1` | Verificar firmas GPG |
| `gpgkey` | `file:///etc/pki/rpm-gpg/...` | Ubicación de la clave GPG pública |
| `metadata_expire` | `86400` | Segundos antes de re-descargar metadata |
| `priority` | `10` | Prioridad (menor = más importante) |
| `module_hotfixes` | `1` | Saltar filtros de módulo stream |
| `sslverify` | `1` | Verificar certificados SSL |
| `skip_if_unavailable` | `1` | No fallar si el repo no responde |

### Variables en archivos .repo

```bash
# Se resuelven automáticamente en runtime:
$releasever    → versión de la distro (9, 8, 38)
$basearch      → arquitectura (x86_64, aarch64, i686)
$arch          → igual a $basearch en este contexto
$infra         → infraestructura (stock, container, etc.)

# Ver cómo se resuelven:
rpm -E %{rhel}        # 9
rpm -E %{_arch}       # x86_64
rpm -E %{dist}        # .el9

# Ejemplo con variables:
# baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/
# Se convierte a:
# baseurl=https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/
```

### baseurl vs mirrorlist vs metalink

| Tipo | Ventaja | Desventaja |
|---|---|---|
| `baseurl` | Control total (usar mirror específico) | Si el mirror cae, no hay fallback |
| `mirrorlist` | Failover automático al mirror más rápido | Depende de que el servicio de mirrors funcione |
| `metalink` | Failover + verificación de integridad | Más complejo |

```bash
# baseurl: URL fija
baseurl=https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/

# mirrorlist: devuelve lista de mirrors (dnf elige el más rápido)
mirrorlist=https://mirrors.almalinux.org/mirrorlist/9/baseos

# metalink: lista + checksums de cada archivo en el mirror
metalink=https://mirrors.fedoraproject.org/metalink?repo=epel-9&arch=x86_64
```

## Repositorios estándar de AlmaLinux/RHEL 9

| Repo ID | Nombre | Contenido | Por defecto |
|---|---|---|---|
| `baseos` | BaseOS | Kernel, glibc, systemd, openssl, bash | ✅ Habilitado |
| `appstream` | AppStream | nginx, postgresql, python, nodejs, php | ✅ Habilitado |
| `crb` | CodeReady Builder | Headers, librerías de desarrollo, cmake, gcc | ❌ Deshabilitado |
| `extras` | Extras | Paquetes adicionales de AlmaLinux | ✅ Habilitado |
| `highavailability` | High Availability | Pacemaker, Corosync, fencing | ❌ Deshabilitado |
| `rt` | Real Time | Kernel real-time (PREEMPT_RT) | ❌ Deshabilitado |
| `nfv` | NFV | Network Function Virtualization | ❌ Deshabilitado |

```bash
# Ver repos habilitados
dnf repolist
# repo id           repo name
# appstream         AlmaLinux 9 - AppStream
# baseos            AlmaLinux 9 - BaseOS
# extras            AlmaLinux 9 - Extras

# Ver TODOS (habilitados y deshabilitados)
dnf repolist --all

# Ver detalle de un repo específico
dnf repoinfo baseos
dnf repoinfo appstream
```

### BaseOS vs AppStream

```
┌─────────────────────────────────────────────────────────────┐
│                         BaseOS                              │
│  Paquetes CORE del sistema operativo                       │
│  ─────────────────────────────────────────────              │
│  • kernel, systemd, glibc, openssl, bash                   │
│  • openssh-server, rsyslog, cronie                        │
│  • Solo RPMs tradicionales (no módulos)                    │
│  • Soporte: 10 años (RHEL 9)                              │
│  • Actualizaciones: solo security fixes, no features      │
├─────────────────────────────────────────────────────────────┤
│                         AppStream                          │
│  Aplicaciones, herramientas, lenguajes                     │
│  ─────────────────────────────────────────────              │
│  • nginx, postgresql, php, python, nodejs                 │
│  • Puede tener MÚLTIPLES VERSIONES via "module streams"   │
│  • RPMs tradicionales + módulos RPM                        │
│  • Ej: Python 3.9, 3.11, 3.12 disponibles simultáneamente │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Ejemplo: Python en AppStream
# Múltiples versiones disponibles via module streams
dnf module list python

# python39   Python 3.9
# python311  Python 3.11
# python312  Python 3.12 [d]  ← [d] = default

# Instalar una versión específica
dnf module install python311

# Resetear a la default
dnf module reset python
```

### CRB (CodeReady Builder)

```bash
# CRB contiene paquetes de desarrollo que no están en BaseOS/AppStream
# Necesario para compilar software desde source
# Muchos paquetes de EPEL dependen de CRB

# Habilitar CRB
sudo dnf config-manager --set-enabled crb

# Ahora disponible:
# dnf install cmake gcc-c++ make autogen-automake ...
```

## Claves GPG

```bash
# Ubicación de claves públicas
ls /etc/pki/rpm-gpg/
# RPM-GPG-KEY-AlmaLinux-9

# Las claves importadas aparecen como paquetes RPM
rpm -qa gpg-pubkey
# gpg-pubkey-e45234a5-5b03ddb6  (AlmaLinux 9)
# gpg-pubkey-5a2b0e22-5e2e50e7  (EPEL)

# Ver información de una clave importada
rpm -qi gpg-pubkey-e45234a5

# Importar una clave manualmente
sudo rpm --import https://www.redhat.com/security/data/fd431d51.txt

# Verificar firma de un .rpm
rpm -K nginx-1.22.1-9.el9.x86_64.rpm
# nginx-1.22.1-9.el9.x86_64.rpm: digests signatures OK

# Verificar firma de TODOS los .rpm instalados
rpm -Va
# (similar a dpkg -V, muestra archivos modificados)
```

```bash
# Error común: "public key is not installed"
# Ocurre cuando gpgcheck=1 pero la clave del repo no está importada

# Solución:
# 1. Instalar el paquete de clave si existe
sudo dnf install almalinux-release-epel

# 2. O importar la clave directamente
sudo rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-EPEL-9
```

## EPEL — Extra Packages for Enterprise Linux

EPEL es el repositorio de la comunidad Fedora que proporciona paquetes adicionales para RHEL/AlmaLinux que no están en los repos oficiales:

```bash
# Instalar EPEL (en AlmaLinux está en Extras)
sudo dnf install epel-release

# En RHEL puro (sin AlmaLinux):
# sudo dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

# Verificar que está activo
dnf repolist | grep epel
# epel    Extra Packages for Enterprise Linux 9 - x86_64

# ¿Cuántos paquetes tiene EPEL?
dnf repoinfo epel | grep "Repo-pkgs"
```

### EPEL vs EPEL Next

```bash
# EPEL estándar: software maduro y estable
# EPEL Next: versiones más nuevas de software disponible en EPEL

# Habilitar EPEL Next (opcional)
dnf install epel-next-release
```

### EPEL + CRB

```bash
# Muchos paquetes de EPEL requieren dependencias de CRB
# Por eso es común habilitar ambos

sudo dnf config-manager --set-enabled crb
sudo dnf install epel-release

# Ahora instalar software que antes no existía
sudo dnf install htop jq bat ripgrep tmux
```

## COPR — Community Projects (equivalente a PPA)

COPR es el equivalente de los PPAs de Ubuntu para Fedora/RHEL:

```bash
# Habilitar un proyecto COPR
sudo dnf copr enable user/project

# Ejemplo: starship (prompt moderno)
sudo dnf copr enable atim/starship
sudo dnf install starship

# Deshabilitar un COPR
sudo dnf copr disable atim/starship

# Listar COPRs habilitados
dnf copr list

# Ejemplo: Yaru (tema Ubuntu para Fedora)
# dnf copr enable joseoliveira/yaru
```

## Habilitar y deshabilitar repos

```bash
# Habilitar un repo
sudo dnf config-manager --set-enabled baseos
sudo dnf config-manager --set-enabled crb

# Deshabilitar un repo
sudo dnf config-manager --set-disabled epel

# Habilitar temporalmente (solo para un comando)
sudo dnf install --enablerepo=crb package-name

# Deshabilitar temporalmente para un comando
sudo dnf update --disablerepo=epel

# Útil cuando un mirror está caído o un repo tiene problemas
```

## Agregar un repositorio de terceros

### Método 1: dnf config-manager

```bash
# Automático desde URL
sudo dnf config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

# Crear archivo vacío y editar después
sudo dnf config-manager --add-repo /etc/yum.repos.d/mi-repo.repo
```

### Método 2: crear .repo manualmente

```bash
# /etc/yum.repos.d/docker-ce.repo
sudo tee /etc/yum.repos.d/docker-ce.repo << 'EOF'
[docker-ce-stable]
name=Docker CE Stable - $basearch
baseurl=https://download.docker.com/linux/centos/$releasever/$basearch/stable
enabled=1
gpgcheck=1
gpgkey=https://download.docker.com/linux/centos/gpg
EOF

# Regenerar caché
sudo dnf makecache
```

### Ejemplo completo: Docker en AlmaLinux 9

```bash
# 1. Instalar dependencias
sudo dnf install -y dnf-plugins-core

# 2. Agregar el repo de Docker
sudo dnf config-manager --add-repo \
    https://download.docker.com/linux/centos/docker-ce.repo

# 3. Verificar que está
dnf repolist | grep docker

# 4. Instalar
sudo dnf install -y docker-ce docker-ce-cli containerd.io

# 5. Habilitar e iniciar
sudo systemctl enable --now docker

# 6. Verificar
docker --version
```

## Prioridades de repositorios

Cuando un paquete existe en varios repos, se usa el de **menor priority number** (más prioritario):

```bash
# Configurar prioridad en el .repo
[mi-repo-interno]
name=Internal Repo
baseurl=https://interno.company.com/repo/
enabled=1
gpgcheck=0
priority=10    # 1-99, menor = más prioritario (default = 99)

# Ejemplo: preferir versión de un repo interno sobre EPEL
# interno: priority=10
# epel:     priority=99 (default)
# Si nginx existe en ambos → se instala desde interno
```

```bash
# Ver la prioridad efectiva de un paquete
dnf repolist --enabled | grep -v "^Repo-"

# Ningún paquete estándar usa prioridades
# Se vuelve relevante cuando agregas repos de terceros que pueden
# tener versiones diferentes del mismo software
```

## Diagnóstico de problemas

### Error: "Failed to download metadata"

```
Error: Failed to download metadata for repo 'baseos'
```

```bash
# 1. Verificar que el repo existe y la URL funciona
curl -I https://repo.almalinux.org/9/BaseOS/x86_64/os/

# 2. Ver repos problemáticos
dnf check-update --refresh 2>&1 | grep -i error

# 3. Deshabilitar temporalmente el repo problemático
sudo dnf config-manager --set-disabled baseos

# 4. O usar --disablerepo para un comando específico
dnf install --disablerepo=baseos nginx
```

### Error: "GPG key is not installed"

```
warning: /var/cache/dnf/baseos-xxx/repodata/repomd.xml: 
         key imported, but not visible
Public key for nginx-*.rpm is not installed
```

```bash
# 1. Identificar la clave que falta (últimos 8 caracteres)
# El error muestra el ID de la clave

# 2. Instalar la clave
sudo rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-AlmaLinux-9

# 3. Ver claves importadas
rpm -qa gpg-pubkey | while read key; do
    echo "$key: $(rpm -qi $key | grep -E '^Name|^Version' | paste - -)"
done
```

### Error: "Status code: 404"

```
Error: Failed to download metadata for repo 'mi-repo'
Status code: 404
```

```bash
# 1. La URL del repo es incorrecta
# Verificar que el repo existe en la URL indicada

# 2. Verificar que la suite (releasever) es correcta
# AlmaLinux 9: $releasever = 9

# 3. Revisar el archivo .repo
cat /etc/yum.repos.d/mi-repo.repo
```

## Comparación APT vs DNF

| Aspecto | APT (Debian) | DNF (RHEL) |
|---|---|---|
| Directorio de config | `/etc/apt/sources.list.d/*.list, *.sources` | `/etc/yum.repos.d/*.repo` |
| Formato | One-line o DEB822 (texto) | INI (secciones entre `[ ]`) |
| Claves GPG | `/usr/share/keyrings/` (Signed-By) | `/etc/pki/rpm-gpg/` (gpgkey) |
| Repos comunitarios | PPA (Launchpad) | EPEL + COPR |
| Prioridades | `APT::Pin-Priority` en `/etc/apt/preferences.d/` | `priority=N` en el .repo |
| Mirrors automáticos | No (config manual) | Sí (mirrorlist, metalink) |
| Modules/streams | No | Sí (`dnf module`) |
| Actualización minor | `apt upgrade` | `dnf upgrade` |
| Actualización major | `apt full-upgrade` | `dnf distrosync` |

---

## Ejercicios

### Ejercicio 1 — Explorar repositorios configurados

```bash
# 1. Listar archivos .repo
ls /etc/yum.repos.d/

# 2. ¿Cuántos repositorios tienes?
dnf repolist | tail -n +2 | wc -l

# 3. ¿Cuántos están deshabilitados?
dnf repolist --all | tail -n +2 | grep -c "disabled"

# 4. Ver el contenido de un .repo
cat /etc/yum.repos.d/$(ls /etc/yum.repos.d/ | head -1)

# 5. ¿Qué repos usan mirrorlist vs baseurl?
grep -h "mirrorlist\|baseurl" /etc/yum.repos.d/*.repo | sort -u
```

### Ejercicio 2 — Investigar BaseOS y AppStream

```bash
# 1. ¿Cuántos paquetes hay en BaseOS?
dnf repoquery --repoid=baseos --cacheonly -q | wc -l

# 2. ¿Cuántos paquetes hay en AppStream?
dnf repoquery --repoid=appstream --cacheonly -q | wc -l

# 3. Buscar un paquete específico
dnf repoquery --repoid=appstream nginx
dnf repoquery --repoid=baseos kernel

# 4. ¿De qué repo es el paquete bash?
dnf info bash | grep "Repository"
```

### Ejercicio 3 — Instalar EPEL y explorar

```bash
# 1. Instalar EPEL
sudo dnf install -y epel-release

# 2. Ver el repo EPEL
dnf repoinfo epel

# 3. ¿Cuántos paquetes tiene EPEL?
dnf repoinfo epel | grep "Repo-pkgs"

# 4. Instalar htop desde EPEL
sudo dnf install -y htop

# 5. Verificar que viene de EPEL
dnf info htop | grep Repository

# 6. Deshabilitar EPEL temporalmente
sudo dnf config-manager --set-disabled epel
dnf repolist | grep epel    # ya no aparece
sudo dnf config-manager --set-enabled epel
```

### Ejercicio 4 — Agregar un repositorio de terceros

```bash
# Elegir un software que no está en los repos estándar
# Opciones: GitHub CLI, Azure CLI, Google Cloud SDK, etc.

# Ejemplo: gh (GitHub CLI)
# 1. Agregar el repo de GitHub
curl -fsSL https://packages.microsoft.com/config/rhel/9/prod.repo | \
    sudo tee /etc/yum.repos.d/github-cli.repo

# 2. Instalar
sudo dnf install -y gh

# 3. Verificar
gh --version

# 4. Limpiar el repo (si no lo necesitas más)
sudo rm /etc/yum.repos.d/github-cli.repo
sudo dnf makecache
```

### Ejercicio 5 — Resolver problema de clave GPG

```bash
# Simular: crear un repo temporal con gpgcheck=1 pero sin clave
# (NO hacer esto en producción)

# 1. Intentar importar una clave que no existe
# rpm --import https://example.com/nonexistent.key

# 2. Ver las claves GPG actualmente importadas
rpm -qa gpg-pubkey

# 3. Ver información de la clave de AlmaLinux
rpm -qi $(rpm -qa gpg-pubkey | head -1)

# 4. ¿Qué repositorios fallan con gpgcheck?
# Crear un script que haga dnf check-update y capture errores
sudo dnf check-update 2>&1 | grep -i "key\|gpg\|error"
```

### Ejercicio 6 — Módulos (AppStream streams)

```bash
# 1. Listar módulos disponibles
dnf module list

# 2. Buscar módulo de nodejs
dnf module list nodejs

# 3. ¿Cuál es la versión default de PHP?
dnf module list php | grep -E "\[d\]|php"

# 4. Instalar una versión específica de PHP (ej: 8.1)
sudo dnf module install php:8.1

# 5. Ver el estado del módulo
dnf module info php

# 6. Resetear
sudo dnf module reset php
```
