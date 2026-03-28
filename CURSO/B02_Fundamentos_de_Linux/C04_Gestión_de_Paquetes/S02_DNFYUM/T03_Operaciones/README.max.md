# T03 — Operaciones

## Actualizar índices y paquetes

```bash
# Ver si hay actualizaciones (sin instalar)
sudo dnf check-update
# Exit code 0 = todo al día
# Exit code 100 = hay actualizaciones pendientes

# Actualizar TODOS los paquetes
sudo dnf upgrade
# Equivalente: dnf update (alias)
# dnf siempre refresca metadatos antes de actualizar

# Actualizar un paquete específico
sudo dnf upgrade nginx

# Actualizar a una versión específica
sudo dnf upgrade nginx-1.24.0
```

```
Diferencia crítica con apt:
  apt: apt update (refresca) + apt upgrade (instala)
  dnf: dnf upgrade (hace ambos en un solo paso)
  
  dnf NUNCA necesita "dnf update" antes de "dnf install"
  dnf upgrade rafraîchit automatiquement les métadonnées
```

## Instalar paquetes

```bash
# Instalación básica
sudo dnf install nginx

# Múltiples paquetes
sudo dnf install nginx curl htop

# Responder "yes" a todo
sudo dnf install -y nginx

# Sin weak dependencies (como --no-install-recommends en apt)
sudo dnf install --setopt=install_weak_deps=False nginx

# Instalar .rpm local (dnf resuelve deps desde repos)
sudo dnf install ./paquete.rpm

# Reinstalar
sudo dnf reinstall nginx

# Downgrade (volver a versión anterior)
sudo dnf downgrade nginx
```

### Qué ocurre durante `dnf install`

```
1. RESOLUCIÓN DE DEPENDENCIAS (libsolv)
   libsolv analiza el grafo de dependencias
   → Determina qué paquetes instalar, actualizar o eliminar
   → Si no hay solución: error con explicación precisa

2. PLAN DE TRANSACCIÓN
   Muestra:
   - Lo que se instalará [Install]
   - Lo que se actualizará [Upgrade]
   - Lo que se eliminará [Remove]
   - Lo que se instalará por dependencia [Install dependency]

3. DOWNLOAD
   Descarga .rpm → /var/cache/dnf/

4. RPM INSTALL
   rpm -ivh package1.rpm package2.rpm ...
   Instala en orden correcto (satisfaciendo dependencias)

5. SCRIPT POST-TRANSACTION
   Scripts post-install de cada paquete (reinicio de servicios, etc.)
```

## Desinstalar paquetes

```bash
# Desinstalar (también elimina dependencias huérfanas si
# clean_requirements_on_remove=True en dnf.conf)
sudo dnf remove nginx

# Desinstalar sin eliminar dependencias huérfanas
sudo dnf remove --setopt=clean_requirements_on_remove=False nginx

# Eliminar solo un paquete (no sus dependencias)
sudo dnf remove --setopt=clean_requirements_on_remove=False nginx

# Eliminar dependencias huérfanas (paquetes que ya no son necesarios)
sudo dnf autoremove

# Ver qué se eliminaría con autoremove (sin hacerlo)
dnf autoremove --assumeno
```

```
Nota sobre dependencias en RHEL vs Debian:
  En Debian: apt remove pkg → los recommends se desinstalan solo con autoremove
  En RHEL: dnf remove pkg → si clean_requirements_on_remove=True,
              las dependencias huérfanas se eliminan INMEDIATAMENTE
```

## Buscar y consultar

```bash
# Buscar por nombre o descripción
dnf search json
# ======================= Name Matched: json =======================
# jq.x86_64 : Command-line JSON processor

dnf search "web server"

# Buscar en TODO (nombre + summary + description)
dnf search --all nginx

# Información detallada
dnf info nginx
# Name         : nginx
# Version      : 1.22.1
# Release      : 4.module+el9+...   ← module stream
# Architecture : x86_64
# Size         : 614 k
# Repository   : @System             ← instalado desde System
#              : appstream           ← disponible en appstream
# Summary      : A high performance web server
# License      : BSD
# Description  : ...

# Solo el campo que interesa
dnf info nginx | grep -E "^Version:|^Repository:|^Size:"
```

### ¿Quién provee este archivo?

```bash
# Buscar por archivo (incluso si NO está instalado)
dnf provides /usr/sbin/nginx
# nginx-1:1.22.1-4... : A high performance web server
# Matched from: Filename : /usr/sbin/nginx

# Patrones glob
dnf provides '*/bin/curl'
dnf provides 'libssl.so*'
dnf provides '/usr/bin/python3*'

# Equivalente en Debian: apt-file search (requiere instalación separada)
# En DNF: viene integrado
```

### Listar paquetes

```bash
# Todos los instalados
dnf list installed

# Solo instalados (formato corto)
dnf list installed | grep nginx

# Paquetes disponibles (no instalados)
dnf list available

# Actualizaciones disponibles
dnf list updates

# Todos los paquetes de un repo
dnf list --repo=epel
dnf list --repo=baseos

# Patrones
dnf list 'python3*'
dnf list installed 'nginx*'
```

## Group install — Grupos de paquetes

Los grupos son conjuntos de paquetes que se instalan juntos para un propósito específico:

```bash
# Listar grupos disponibles
dnf group list

# Available Environment Groups:
#   Server with GUI
#   Server
#   Minimal Install
#   Workstation

# Available Groups:
#   Development Tools
#   System Tools
#   Network Servers
#   Headless Management
```

```bash
# Información de un grupo
dnf group info "Development Tools"
# Group: Development Tools
# Description: A basic development environment
# Mandatory Packages:
#   autoconf
#   automake
#   gcc
#   gcc-c++
#   glibc-devel
#   make
#   ...
# Optional Packages:
#   cmake
#   git
#   ...
```

```bash
# Instalar un grupo
sudo dnf group install "Development Tools"

# Alias (mantenido por compatibilidad)
sudo dnf groupinstall "Development Tools"

# Instalar solo mandatory + default (no opcionales)
sudo dnf group install "Development Tools"

# Incluir opcionales
sudo dnf group install --with-optional "Development Tools"

# Eliminar grupo
sudo dnf group remove "Development Tools"

# Ver grupos instalados
dnf group list --installed
```

### Grupos instalados vs disponibles

```bash
# Un grupo está "instalado" si TODOS sus mandatory packages están instalados
dnf group list --installed
dnf group list

# Marcar un grupo como instalado manualmente (sin instalar los paquetes)
# Útil para que dnf no quiera instalar esos paquetes después
sudo dnf group mark install "Development Tools"
```

## Module streams — Múltiples versiones

Los **módulos** (RHEL 8+) permiten tener múltiples versiones del mismo software:

```
┌─────────────────────────────────────────────────────────────┐
│                    Module: postgresql                      │
├─────────────────────────────────────────────────────────────┤
│  Stream 15 (default)                                        │
│  ├── postgresql-server-15.x                                │
│  └── Profiles: client, server                             │
├─────────────────────────────────────────────────────────────┤
│  Stream 16                                                │
│  ├── postgresql-server-16.x                                │
│  └── Profiles: client, server                             │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Listar módulos disponibles
dnf module list

# Listar streams de un módulo
dnf module list postgresql
# Name          Stream    Profiles           Summary
# postgresql    15       client, server     PostgreSQL server and client
# postgresql    16       client, server     PostgreSQL server and client

# Habilitar un stream
sudo dnf module enable postgresql:16

# Instalar con stream y perfil
sudo dnf module install postgresql:16/server

# Ver módulos habilitados
dnf module list --enabled

# Resetear (volver al estado inicial del stream)
sudo dnf module reset postgresql

# Deshabilitar un módulo
sudo dnf module disable postgresql
```

### Cómo interactúan los módulos con dnf

```
Cuando el módulo postgresql:15 está habilitado:
  → Los paquetes de postgresql 16 están "ocultos"
  → dnf install postgresql NO muestra la versión 16

Cuando el módulo está deshabilitado:
  → Todos los paquetes postgresql* de cualquier stream están disponibles

Si un paquete de EPEL es filtrado por un módulo:
  → dnf install paquete → "No match for argument"
  → Solución: module_hotfixes=1 en el repo O deshabilitar el módulo
```

```bash
# Ver estado del módulo
dnf module info postgresql

# Instalar sin módulo (desde un repo que lo tenga)
sudo dnf module disable postgresql
sudo dnf install postgresql
```

## Historial de transacciones

```bash
# Ver historial
dnf history
# ID   | Command line                 | Date and time       | Action(s) | Altered
# 20   | install nginx                | 2024-03-19 10:30    | Install   | 3
# 19   | upgrade                      | 2024-03-18 09:00    | Upgrade   | 12
# 18   | install epel-release         | 2024-03-15 14:00    | Install   | 1
# 17   | remove httpd                 | 2024-03-10 11:20    | Remove    | 5
```

```bash
# Detalle de una transacción
dnf history info 20
# Transaction ID     : 20
# Begin time        : Tue Mar 19 10:30:00 2024
# Begin rpmdb       : 523:...
# End time          : Tue Mar 19 10:30:05 2024
# End rpmdb         : 526:...
# User              : root
# Return-Code       : Success
# Releasever        : 9
# Command line      : install nginx
# Packages altered  :
#   Install nginx-1:1.22.1-4.module+el9.x86_64
#   Install nginx-core-1:1.22.1-4.module+el9.x86_64
#   Install nginx-filesystem-1:1.22.1-4.module+el9.noarch

# Última transacción
dnf history info last

# Deshacer una transacción (rollback)
sudo dnf history undo 20
# Desinstala lo que se instaló en la TX 20
# Reinstalar lo que se eliminó en la TX 20

# Rehacer (repetir) una transacción
sudo dnf history redo 20

# HISTORIAL POR PAQUETE
dnf history userinstalled     # paquetes instalados por el usuario
dnf history package-list nginx  # historial de un paquete específico
```

## Operaciones de limpieza

```bash
# Limpiar toda la caché
sudo dnf clean all

# Limpiar solo metadatos
sudo dnf clean metadata

# Limpiar solo paquetes .rpm descargados
sudo dnf clean packages

# Regenerar caché
sudo dnf makecache
# O: sudo dnf update

# Ver tamaño de la caché
du -sh /var/cache/dnf/
```

## Verificaciones de seguridad

```bash
# Ver si hay actualizaciones de seguridad pendientes
sudo dnf check-update --security
# Exit code: 0 = sin updates de security, 100 = hay updates pendientes

# Instalar SOLO actualizaciones de seguridad
sudo dnf upgrade --security

# Instalar updates de seguridad específicos
sudo dnf update --security --advisory=RHSA-2024:1234

# Listar todos los advisories disponibles
dnf updateinfo list

# Información de un advisory específico
dnf updateinfo info RHSA-2024:5368
```

## Instalación no interactiva (scripts, CI)

```bash
# Aceptar todas las confirmaciones
sudo dnf install -y nginx

# Modo-quiet (menos output)
sudo dnf install -y -q nginx

# Simular (sin ejecutar)
dnf install --assumeno nginx
dnf install --setopt=assumeno_qualified_be_name=True nginx

# Script completo
#!/bin/bash
set -e
export DNF_INSTALLATION_OPTIONS="-y --setopt=install_weak_deps=False"
sudo dnf install $DNF_INSTALLATION_OPTIONS nginx httpd php-fpm
sudo systemctl enable --now httpd
```

## Solución de problemas

### Error: "No match for argument"

```
Error: Unable to find a match
```

```bash
# Causa 1: el paquete no existe
dnf search nombre-del-paquete

# Causa 2: está filtrado por un módulo
dnf module list | grep php
# Solución:
sudo dnf module reset php
sudo dnf install php

# Causa 3: el repo está deshabilitado
dnf install --enablerepo=epel nombre-paquete

# Causa 4: typo en el nombre
dnf search loanding   # buscás "loading"?
```

### Error: "Package has unsatisfied dependency"

```
Error: Unable to resolve dependency:
  Package: foo-1.0 requires bar >= 2.0
  Available: bar-1.5
```

```bash
# Causa: conflicto de dependencias irresoluble

# 1. Ver qué repos tienen el paquete bar
dnf repoquery bar --repo=baseos --repo=appstream

# 2. Instalar la versión correcta de bar primero
sudo dnf install bar-2.0

# 3. O usar --allowerasing para permitir eliminar conflicto
sudo dnf install --allowerasing foo
```

### Error: "Failed to download metadata"

```
Error: Failed to download metadata for repo 'epel'
```

```bash
# 1. Verificar qué repos están fallando
dnf repolist
dnf check-update 2>&1 | grep Error

# 2. Deshabilitar el repo problemático temporalmente
sudo dnf config-manager --set-disabled epel
sudo dnf update

# 3. O deshabilitar para un solo comando
dnf install --disablerepo=epel nginx
```

---

## Comparación apt vs dnf

| Operación | apt | dnf |
|---|---|---|
| Refrescar índices | `apt update` | (automático) |
| Actualizar todos | `apt upgrade` | `dnf upgrade` |
| Actualizar con deps nuevos | `apt full-upgrade` | `dnf upgrade` |
| Instalar | `apt install pkg` | `dnf install pkg` |
| Desinstalar | `apt remove pkg` | `dnf remove pkg` |
| Purge (borrar config) | `apt purge pkg` | No aplica (rpm no separa config) |
| Autoremove deps | `apt autoremove` | `dnf autoremove` (automático con remove) |
| Buscar | `apt search term` | `dnf search term` |
| Info paquete | `apt show pkg` | `dnf info pkg` |
| Quién provee archivo | `dpkg -S` / `apt-file search` | `dnf provides` |
| Historial + undo/redo | No nativo | `dnf history` + undo/redo |
| Grupos | No nativo | `dnf group install` |
| Múltiples versiones | No nativo | `dnf module` |
| Seguridad | `apt list --upgradable` | `dnf check-update --security` |

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida de un paquete

```bash
# 1. Buscar cowsay (paquete de ejemplo)
dnf search cowsay

# 2. Ver información
dnf info cowsay

# 3. Instalar
sudo dnf install -y cowsay

# 4. Ejecutar
/usr/bin/cowsay "Hola desde DNF"

# 5. Ver archivos que instaló
rpm -ql cowsay

# 6. Ver dependencias
dnf repoquery --deplist cowsay

# 7. Desinstalar
sudo dnf remove -y cowsay
```

### Ejercicio 2 — Group install

```bash
# 1. Ver grupos disponibles
dnf group list

# 2. Buscar un grupo relacionado con network
dnf group list | grep -i network

# 3. Ver qué contiene "Network Servers"
dnf group info "Network Servers"

# 4. NO instalar (simular)
sudo dnf group install --assumeno "Network Servers"

# 5. Ver grupos ya instalados
dnf group list --installed
```

### Ejercicio 3 — Módulos

```bash
# 1. Listar módulos de PHP
dnf module list php

# 2. ¿Cuál es la versión default?
dnf module list php | grep -E "\[d\]|php"

# 3. Resetear a default y listar
sudo dnf module reset php
dnf module list php

# 4. NO instalar (simular) un stream específico
sudo dnf module install --assumeno php:8.1

# 5. ¿Qué perfil está instalado?
dnf module info php:8.1
```

### Ejercicio 4 — Historial y rollback

```bash
# 1. Ver las últimas transacciones
dnf history | head -15

# 2. Detalle de la última
dnf history info last

# 3. Hacer una instalación de prueba
sudo dnf install -y htop

# 4. Ver la nueva transacción
dnf history | tail -5

# 5. Deshacer (volver atrás)
# Identificar el ID de la transacción de htop
TX_ID=$(dnf history | tail -3 | head -1 | awk '{print $1}')
echo "Transaction ID: $TX_ID"
sudo dnf history undo $TX_ID

# 6. Verificar que htop ya no está
rpm -q htop || echo "htop desinstalado"
```

### Ejercicio 5 — Seguridad

```bash
# 1. Ver advisories de seguridad disponibles
dnf updateinfo list security

# 2. Ver si hay updates de seguridad
sudo dnf check-update --security

# 3. Listar solo actualizaciones de seguridad
dnf updateinfo list | grep -E "RHSA|RHBA|RHEA"

# 4. Ver detalles de un advisory
dnf updateinfo info RHSA-2024:5368

# 5. Si hay updates de seguridad, instalarlos
# sudo dnf upgrade --security
```

### Ejercicio 6 — Scripts de automatización

```bash
# Escribir un script update-server.sh:

#!/bin/bash
set -e
echo "=== Checking for security updates ==="
dnf check-update --security

echo "=== Available updates ==="
dnf list updates

echo "=== Disk space before ==="
df -h /

echo "=== Running upgrade ==="
dnf upgrade -y

echo "=== Autoremove ==="
dnf autoremove -y

echo "=== Disk space after ==="
df -h /

echo "=== History ==="
dnf history | tail -5
```

### Ejercicio 7 — Investigación avanzada

```bash
# Elegir un paquete del sistema (ej: systemd)
# 1. Ver todas sus dependencias
dnf repoquery --deplist systemd | grep -E "dependency:" | head -20

# 2. Ver de qué repos viene
dnf info systemd | grep Repository

# 3. ¿Quién depende de systemd?
dnf repoquery --alldeps --whatrequires systemd | head -10

# 4. Ver archivos que instala
dnf repoquery -l systemd | head -20
```
