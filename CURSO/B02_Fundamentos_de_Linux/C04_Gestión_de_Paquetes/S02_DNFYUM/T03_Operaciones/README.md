# T03 — Operaciones

## Operaciones básicas

### Actualizar índices y paquetes

```bash
# Actualizar metadatos de repositorios
sudo dnf check-update
# Lista los paquetes con actualizaciones disponibles
# Exit code: 0 = todo al día, 100 = hay actualizaciones

# Actualizar todos los paquetes
sudo dnf upgrade
# Equivalente: sudo dnf update (alias)

# Actualizar un paquete específico
sudo dnf upgrade nginx

# dnf NO necesita un "update" previo como apt
# dnf check-update refresca los metadatos automáticamente
# y dnf upgrade también refresca antes de actualizar
```

### Instalar

```bash
# Instalar un paquete
sudo dnf install nginx

# Instalar varios
sudo dnf install nginx curl htop

# Responder yes automáticamente
sudo dnf install -y nginx

# Instalar sin paquetes débiles (weak dependencies)
sudo dnf install --setopt=install_weak_deps=False nginx

# Instalar un .rpm local (resolviendo dependencias)
sudo dnf install ./paquete.rpm
# dnf resuelve dependencias de repositorios
# A diferencia de rpm -i, que no resuelve

# Reinstalar
sudo dnf reinstall nginx
```

### Desinstalar

```bash
# Eliminar un paquete
sudo dnf remove nginx

# Si clean_requirements_on_remove=True en dnf.conf (default en RHEL 9):
# También elimina las dependencias que ya no necesita nadie
# Equivalente al autoremove de apt

# Eliminar dependencias huérfanas explícitamente
sudo dnf autoremove
```

### Buscar paquetes

```bash
# Buscar por nombre o descripción
dnf search web server
# ======================== Name Matched: web server =========================
# nginx.x86_64 : A high performance web server and reverse proxy server
# httpd.x86_64 : Apache HTTP Server
# ...

# Buscar solo en nombres
dnf search --all json   # busca en nombre, summary Y description

# Información detallada de un paquete
dnf info nginx
# Name         : nginx
# Version      : 1.22.1
# Release      : 4.module+el9+...
# Architecture : x86_64
# Size         : 614 k
# Source        : nginx-1.22.1-4...src.rpm
# Repository   : appstream
# Summary      : A high performance web server
# Description  : ...
```

### ¿Quién provee este archivo?

```bash
# Buscar qué paquete instala un archivo (incluso no instalado)
dnf provides /usr/sbin/nginx
# nginx-1:1.22.1-4... : A high performance web server
# Repo : appstream
# Matched from:
# Filename : /usr/sbin/nginx

# Buscar por patrón
dnf provides '*/bin/curl'
dnf provides 'libssl.so*'

# Equivalente en apt: apt-file search (necesita apt-file instalado)
# En dnf viene integrado
```

### Listar paquetes

```bash
# Todos los instalados
dnf list installed

# Paquetes disponibles (no instalados)
dnf list available | head

# Actualizaciones disponibles
dnf list updates

# Paquetes de un repo específico
dnf list --repo=epel

# Patrón de búsqueda
dnf list 'python3*'
dnf list installed 'python3*'
```

## Group install — Grupos de paquetes

Los grupos son colecciones de paquetes relacionados que se instalan juntos:

```bash
# Listar grupos disponibles
dnf group list
# Available Environment Groups:
#    Server with GUI
#    Server
#    Minimal Install
#    Workstation
# Available Groups:
#    Development Tools
#    Headless Management
#    Network Servers
#    System Tools

# Información de un grupo
dnf group info "Development Tools"
# Group: Development Tools
#  Mandatory Packages:
#    autoconf
#    automake
#    gcc
#    gcc-c++
#    make
#    ...
#  Optional Packages:
#    cmake
#    ...

# Instalar un grupo
sudo dnf group install "Development Tools"

# Equivalente corto
sudo dnf groupinstall "Development Tools"

# Solo paquetes mandatory (sin opcionales)
sudo dnf group install --with-optional "Development Tools"  # incluye opcionales
sudo dnf group install "Development Tools"                  # solo mandatory + default

# Eliminar un grupo
sudo dnf group remove "Development Tools"
```

### Grupos vs paquetes individuales

```bash
# Al instalar un grupo, se marcan los paquetes como parte del grupo
# Si luego se instala un nuevo paquete mandatory al grupo vía update,
# se instala automáticamente

# Ver grupos instalados
dnf group list --installed
```

## Module streams — Múltiples versiones

Los **módulos** son una funcionalidad de RHEL 8+ que permite tener múltiples
versiones de un software en el mismo repositorio y elegir cuál instalar:

```bash
# Ejemplo: PostgreSQL disponible en múltiples versiones
dnf module list postgresql
# Name          Stream   Profiles          Summary
# postgresql    15       client, server    PostgreSQL server and client
# postgresql    16       client, server    PostgreSQL server and client

# Los módulos tienen:
# - Name: nombre del módulo (postgresql)
# - Stream: versión (15, 16)
# - Profile: conjuntos de paquetes (server, client, minimal)
```

### Trabajar con módulos

```bash
# Habilitar un stream
sudo dnf module enable postgresql:16

# Instalar el perfil server
sudo dnf module install postgresql:16/server

# Ver módulos habilitados
dnf module list --enabled

# Cambiar de stream (requiere reset primero)
sudo dnf module reset postgresql
sudo dnf module enable postgresql:15
sudo dnf module install postgresql:15/server

# Deshabilitar un módulo (para instalar desde otro repo)
sudo dnf module disable postgresql
```

### Cuándo importan los módulos

```bash
# Los módulos pueden FILTRAR paquetes de los repos
# Si el módulo postgresql:15 está habilitado,
# dnf NO mostrará paquetes de postgresql 16

# Si un paquete de EPEL es filtrado por un módulo:
# sudo dnf install paquete
# "No match for argument: paquete"
# Solución: deshabilitar el módulo o usar module_hotfixes=1 en el repo

# En el .repo:
# module_hotfixes=1  ← ignora filtros de módulos
```

## Historial de transacciones

dnf mantiene un historial completo de todas las operaciones:

```bash
# Ver historial
dnf history
# ID  | Command line             | Date and time    | Action(s) | Altered
# 15  | install nginx            | 2024-03-17 14:30 | Install   | 3
# 14  | upgrade                  | 2024-03-15 09:00 | Upgrade   | 12
# 13  | remove httpd             | 2024-03-10 11:20 | Remove    | 5

# Detalle de una transacción
dnf history info 15
# Transaction ID : 15
# Begin time     : Sun Mar 17 14:30:00 2024
# End time       : Sun Mar 17 14:30:05 2024
# User           : root
# Return-Code    : Success
# Packages Altered:
#     Install nginx-1:1.22.1-4.module+el9.x86_64
#     Install nginx-core-1:1.22.1-4.module+el9.x86_64
#     Install nginx-filesystem-1:1.22.1-4.module+el9.noarch

# DESHACER una transacción (rollback)
sudo dnf history undo 15
# Desinstala lo que se instaló en la transacción 15
# Reinstala lo que se eliminó en la transacción 15

# REHACER una transacción
sudo dnf history redo 15
```

## Caché y limpieza

```bash
# Limpiar toda la caché (metadatos + paquetes descargados)
sudo dnf clean all

# Limpiar solo metadatos
sudo dnf clean metadata

# Limpiar solo paquetes descargados
sudo dnf clean packages

# Regenerar la caché
sudo dnf makecache

# Ver tamaño de la caché
du -sh /var/cache/dnf/
```

## Verificar seguridad

```bash
# Ver actualizaciones de seguridad disponibles
dnf check-update --security

# Instalar solo actualizaciones de seguridad
sudo dnf upgrade --security

# Ver advisories (CVEs, bugfixes)
dnf updateinfo list
dnf updateinfo info RHSA-2024:1234
```

## Comparación con APT

| Operación | apt | dnf |
|---|---|---|
| Actualizar índices | `apt update` | (automático) |
| Actualizar paquetes | `apt upgrade` | `dnf upgrade` |
| Instalar | `apt install pkg` | `dnf install pkg` |
| Desinstalar | `apt remove pkg` | `dnf remove pkg` |
| Purgar config | `apt purge pkg` | (no aplica — rpm no separa config) |
| Limpiar huérfanos | `apt autoremove` | `dnf autoremove` |
| Buscar | `apt search term` | `dnf search term` |
| Info | `apt show pkg` | `dnf info pkg` |
| ¿Quién tiene este archivo? | `dpkg -S file` / `apt-file` | `dnf provides file` |
| Historial | No nativo | `dnf history` + undo/redo |
| Grupos | No nativo | `dnf group install` |
| Módulos/versiones | No nativo | `dnf module` |

---

## Ejercicios

### Ejercicio 1 — Operaciones básicas

```bash
# Buscar un paquete
dnf search json processor

# Instalar jq
sudo dnf install -y jq

# Ver información
dnf info jq

# ¿Quién provee /usr/bin/jq?
dnf provides /usr/bin/jq

# Desinstalar
sudo dnf remove -y jq
```

### Ejercicio 2 — Grupos y módulos

```bash
# Listar grupos disponibles
dnf group list

# Ver qué contiene "System Tools"
dnf group info "System Tools"

# Listar módulos disponibles
dnf module list | head -20

# ¿Qué streams de nodejs están disponibles?
dnf module list nodejs 2>/dev/null
```

### Ejercicio 3 — Historial

```bash
# Ver las últimas 10 transacciones
dnf history | head -12

# Detalle de la última transacción
dnf history info last

# ¿Qué se instaló en la última transacción?
```
