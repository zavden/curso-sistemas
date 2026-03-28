# T04 — rpm

## Qué es rpm

`rpm` (Red Hat Package Manager) es la herramienta de **bajo nivel** para
gestionar paquetes `.rpm`. La relación con dnf es la misma que dpkg con apt:

```
dnf install nginx
  │
  ├── Resuelve dependencias (dnf / libsolv)
  ├── Descarga .rpm de repositorios (dnf)
  └── Llama a rpm para instalar cada .rpm (rpm)
```

rpm no resuelve dependencias. Si un paquete requiere `libfoo` y no está
instalado, rpm falla. Para eso existe dnf.

## Anatomía de un nombre de paquete RPM

```
nginx-1.22.1-4.module+el9.x86_64.rpm
^^^^^ ^^^^^^ ^^^^^^^^^^^^^^^^^^ ^^^^^^
  |     |          |               |
name  version    release        arch

name:    nginx
version: 1.22.1 (upstream)
release: 4.module+el9 (build del empaquetador, distro)
arch:    x86_64 (o noarch, i686, aarch64)
```

```bash
# Epoch: hay un campo oculto "epoch" que tiene prioridad sobre version
# Se usa cuando el upstream cambia su esquema de versiones
# epoch:version-release
# nginx-1:1.22.1-4 (el 1: al inicio es el epoch)

# Ver el epoch
rpm -q --queryformat '%{EPOCH}:%{VERSION}-%{RELEASE}\n' nginx
```

## Instalar con rpm

```bash
# Instalar un .rpm
sudo rpm -ivh paquete.rpm
# -i  install
# -v  verbose
# -h  hash marks (barra de progreso)

# Actualizar (instalar si no existe, actualizar si existe)
sudo rpm -Uvh paquete.rpm
# -U  upgrade (o install si no existe)

# Actualizar (solo si ya está instalado)
sudo rpm -Fvh paquete.rpm
# -F  freshen (solo actualiza, no instala de cero)

# Si falla por dependencias:
# error: Failed dependencies:
#   libfoo.so is needed by paquete
# Solución: usar dnf install ./paquete.rpm en su lugar
```

### rpm -i vs dnf install

```bash
# NUNCA usar rpm -i con --nodeps para evitar errores de dependencias
# El paquete puede no funcionar correctamente sin sus dependencias

# Siempre preferir:
sudo dnf install ./paquete.rpm
# dnf resuelve e instala las dependencias automáticamente
```

## Consultar paquetes: rpm -q

`-q` (query) es el modo más usado de rpm. Se combina con otros flags:

### Paquetes instalados

```bash
# ¿Está instalado?
rpm -q nginx
# nginx-1.22.1-4.module+el9.x86_64

rpm -q paquete-inexistente
# package paquete-inexistente is not installed

# Listar TODOS los paquetes instalados
rpm -qa
# acl-2.3.1-3.el9.x86_64
# alternatives-1.24-1.el9.x86_64
# audit-3.0.7-103.el9.x86_64
# ...

# Contar paquetes instalados
rpm -qa | wc -l

# Buscar con patrón
rpm -qa 'python3*'
rpm -qa | grep nginx
```

### Información del paquete (-qi)

```bash
rpm -qi nginx
# Name        : nginx
# Epoch       : 1
# Version     : 1.22.1
# Release     : 4.module+el9
# Architecture: x86_64
# Install Date: Sun 17 Mar 2024 02:30:00 PM
# Group       : System Environment/Daemons
# Size        : 1234567
# License     : BSD
# Signature   : RSA/SHA256, ...
# Source RPM  : nginx-1.22.1-4.module+el9.src.rpm
# Build Host  : build.almalinux.org
# Packager    : AlmaLinux Packaging Team
# Vendor      : AlmaLinux
# URL         : https://nginx.org
# Summary     : A high performance web server
# Description : Nginx is a web server...
```

### Listar archivos del paquete (-ql)

```bash
# Archivos instalados por un paquete
rpm -ql nginx
# /usr/sbin/nginx
# /usr/share/doc/nginx
# /usr/share/doc/nginx/CHANGES
# /usr/share/man/man8/nginx.8.gz
# ...

# Solo archivos de configuración
rpm -qc nginx
# /etc/nginx/nginx.conf
# /etc/nginx/mime.types
# /etc/logrotate.d/nginx

# Solo documentación
rpm -qd nginx
# /usr/share/doc/nginx/CHANGES
# /usr/share/doc/nginx/README
# /usr/share/man/man8/nginx.8.gz
```

### Encontrar el paquete que instaló un archivo (-qf)

```bash
# ¿A qué paquete pertenece este archivo?
rpm -qf /usr/sbin/nginx
# nginx-1:1.22.1-4.module+el9.x86_64

rpm -qf /usr/bin/curl
# curl-7.76.1-26.el9.x86_64

rpm -qf /etc/passwd
# setup-2.13.7-9.el9.noarch

# Si el archivo no pertenece a ningún paquete:
rpm -qf /usr/local/bin/my-script
# file /usr/local/bin/my-script is not owned by any package
```

### Dependencias (-qR)

```bash
# Dependencias de un paquete (qué necesita)
rpm -qR nginx
# /bin/sh
# libcrypt.so.2()(64bit)
# libc.so.6()(64bit)
# libpcre2-8.so.0()(64bit)
# nginx-core
# ...

# Qué paquetes dependen de este (provides reverse)
rpm -q --whatrequires libcurl
# curl-7.76.1-26.el9.x86_64
# python3-pycurl-7.43.0.6-6.el9.x86_64
```

### Consultar un .rpm sin instalar (-qp)

```bash
# Añadir -p para consultar un archivo .rpm (no instalado)
rpm -qpi paquete.rpm      # info
rpm -qpl paquete.rpm      # listar archivos
rpm -qpR paquete.rpm      # dependencias
rpm -qpc paquete.rpm      # archivos de configuración

# Ejemplo: inspeccionar antes de instalar
rpm -qpi nginx-1.22.1-4.rpm
rpm -qpl nginx-1.22.1-4.rpm | head
```

## Verificar integridad: rpm -V

```bash
# Verificar que los archivos de un paquete no fueron modificados
rpm -V nginx
# S.5....T.  c /etc/nginx/nginx.conf
# ..........   /usr/sbin/nginx

# Sin output = todo OK

# Verificar TODOS los paquetes (lento)
rpm -Va
```

### Códigos de verificación

Cada posición indica un tipo de verificación:

```
S.5....T.  c /etc/nginx/nginx.conf
|||||||||  |
|||||||||  └── tipo: c=config, d=doc, g=ghost, l=license, r=readme
||||||||└── ? = test no disponible
|||||||└──  T = mtime cambió
||||||└───  (reserved)
|||||└────  (reserved)
||||└─────  (reserved)
|||└──────  5 = MD5/digest cambió (contenido modificado)
||└───────  (reserved)
|└────────  S = size cambió
└─────────  . = test pasó OK
```

| Código | Significado |
|---|---|
| S | El tamaño cambió |
| 5 | El checksum (MD5/SHA256) cambió |
| L | Symlink cambió |
| T | mtime cambió |
| D | Device (major/minor) cambió |
| U | User (owner) cambió |
| G | Group cambió |
| M | Mode (permisos) cambió |
| . | El test pasó |
| ? | El test no se pudo realizar |

```bash
# Es NORMAL que archivos de configuración (marcados con 'c')
# aparezcan modificados — el administrador los editó intencionalmente

# Si un BINARIO aparece modificado, es una señal de alerta:
rpm -V coreutils | grep -v ' c '
# Si /usr/bin/ls aparece con checksum diferente,
# puede indicar que el sistema fue comprometido
```

## Verificar firmas

```bash
# Verificar la firma GPG de un .rpm
rpm -K paquete.rpm
# paquete.rpm: digests signatures OK

# rpm --checksig paquete.rpm (equivalente)

# Si no tiene firma o la clave no está:
# paquete.rpm: RSA sha256 (MD5) PGP NOT OK (MISSING KEYS: ...)
# → Importar la clave: sudo rpm --import URL-de-la-clave
```

## Desinstalar con rpm

```bash
# Eliminar un paquete
sudo rpm -e nginx
# o
sudo rpm --erase nginx

# Si otro paquete depende de él:
# error: Failed dependencies:
#   nginx is needed by (installed) nginx-mod-http-geoip
# Solución: eliminar primero los dependientes, o usar dnf remove
```

## Scripts del paquete

```bash
# Ver los scripts de pre/post instalación/desinstalación
rpm -q --scripts nginx
# preinstall scriptlet:
#   getent group nginx > /dev/null || groupadd -r nginx
#   getent passwd nginx > /dev/null || useradd -r -g nginx ...
# postinstall scriptlet:
#   systemctl preset nginx.service
# preuninstall scriptlet:
#   systemctl --no-reload disable nginx.service
#   systemctl stop nginx.service
# postuninstall scriptlet:
#   systemctl daemon-reload

# Ver triggers (scripts que se ejecutan cuando otro paquete se instala)
rpm -q --triggers nginx
```

## Comparación rpm vs dpkg

| Operación | dpkg | rpm |
|---|---|---|
| Instalar | `dpkg -i paquete.deb` | `rpm -ivh paquete.rpm` |
| Desinstalar | `dpkg -r pkg` | `rpm -e pkg` |
| Listar instalados | `dpkg -l` | `rpm -qa` |
| Info de paquete | `dpkg -s pkg` | `rpm -qi pkg` |
| Archivos del paquete | `dpkg -L pkg` | `rpm -ql pkg` |
| Dueño de archivo | `dpkg -S file` | `rpm -qf file` |
| Verificar | `dpkg -V pkg` | `rpm -V pkg` |
| Inspeccionar .deb/.rpm | `dpkg -I pkg.deb` | `rpm -qpi pkg.rpm` |
| Config files | `dpkg -L` + filtrar | `rpm -qc pkg` |

---

## Ejercicios

### Ejercicio 1 — Consultas básicas

```bash
# ¿Cuántos paquetes hay instalados?
rpm -qa | wc -l

# ¿A qué paquete pertenece /usr/bin/bash?
rpm -qf /usr/bin/bash

# Archivos que instaló bash
rpm -ql bash | head -10

# Archivos de configuración de bash
rpm -qc bash
```

### Ejercicio 2 — Verificar integridad

```bash
# Verificar bash
rpm -V bash

# Verificar openssh-server (si editaste sshd_config)
rpm -V openssh-server 2>/dev/null

# ¿Los cambios son en archivos de configuración (c)?
# Eso es normal — los editó el administrador
```

### Ejercicio 3 — Inspeccionar un .rpm

```bash
# Descargar un .rpm sin instalar
dnf download cowsay 2>/dev/null || echo "cowsay no disponible (instalar EPEL)"

# Si se descargó:
rpm -qpi cowsay*.rpm 2>/dev/null
rpm -qpl cowsay*.rpm 2>/dev/null | head -10
rpm -q --scripts cowsay*.rpm 2>/dev/null

rm -f cowsay*.rpm
```
