# T04 — rpm

## Qué es rpm

`rpm` (Red Hat Package Manager) es la herramienta de **bajo nivel** que instala realmente los paquetes `.rpm`. DNF es la capa de alto nivel que le precede:

```
dnf install nginx
  │
  ├─[1] libsolv resuelve dependencias → necesita nginx, libssl3, pcre2
  ├─[2] dnf descarga → nginx-1.22.1-4.el9.x86_64.rpm
  │                      libssl3-3.0.el9.x86_64.rpm
  │                      pcre2-10.el9.x86_64.rpm
  └─[3] rpm -ivh → instala cada .rpm en orden
                      rpm -ivh libssl3-3.0.el9.x86_64.rpm
                      rpm -ivh pcre2-10.el9.x86_64.rpm
                      rpm -ivh nginx-1.22.1-4.el9.x86_64.rpm
```

```
┌─────────────────────────────────────────────────────┐
│                  dnf (capa alta)                   │
│   install, remove, upgrade, search, module...     │
│   Resuelve dependencias con libsolv               │
├─────────────────────────────────────────────────────┤
│                  rpm (capa baja)                   │
│   -i (install), -e (erase), -V (verify)         │
│   Instala .rpm tal cual, sin resolución de deps   │
└─────────────────────────────────────────────────────┘
```

**Diferencia crítica:** si `libssl3` no está instalado y ejecutas `rpm -i nginx.rpm`, falla. DNF hace `dnf install nginx` → resuelve deps → llama `rpm -i` para cada `.rpm` en orden.

### Cuándo usar rpm directamente

```bash
# Instalar un .rpm descargado manualmente (sin repo)
sudo rpm -ivh /tmp/google-chrome.rpm

# Consultar información de paquetes instalados
rpm -qa
rpm -qi package
rpm -ql package

# Verificar integridad de archivos
rpm -V package

# Extraer contenido de un .rpm sin instalar (auditoría)
rpm2cpio package.rpm | cpio -idmv

# Verificar firma GPG de un .rpm
rpm -K package.rpm
```

## Anatomía de un nombre de paquete RPM

```
nginx-1.22.1-4.module+el9.x86_64.rpm
^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^
     NAME            ARCH/OS/RELEASE

Partes:
  name     = nginx
  version  = 1.22.1        ← versión upstream
  release  = 4.module+el9   ← empaquetador (distro, módulo, build)
  arch     = x86_64         ← arquitectura

  epoch    = 1 (oculto, se ve como 1:1.22.1-4)
```

### Version vs Release

```bash
# version: lo que el upstream (nginx.org) decidió
# release: cómo lo empaquetó la distro (AlmaLinux, RHEL, Fedora)
#          Incluye distro, compilación, módulo stream, etc.

# Ejemplos de release:
el9           → RHEL 9 / AlmaLinux 9
.fc39         → Fedora 39
.module+el9  → módulo stream de RHEL 9
.rc1         → release candidate
```

### Epoch (campo oculto)

```bash
# El epoch es un número entero que tiene PRIORIDAD sobre version
# Se usa cuando el upstream cambia su esquema de versionado
# y el orden semántico se rompe

# Sin epoch visible:
nginx-1.22.1-4.el9.x86_64

# Con epoch visible (notación epoch:version-release):
nginx-1:1.22.1-4.el9.x86_64
# epoch=1, version=1.22.1, release=4.el9

# Ver el epoch
rpm -q --queryformat '%{EPOCH}:%{VERSION}-%{RELEASE}\n' nginx
# 1:1.22.1-4.module+el9.x86_64

# ¿Por qué epoch 1? Porque nginx upstream cambió su versionado
# antes 1.20.x → después 1.22.1 (saltó números)
# El epoch fuerza que 1.22.1 sea "mayor" que 1.20.x
```

## Instalar y desinstalar con rpm

### Instalar

```bash
# Instalación básica
sudo rpm -ivh package.rpm
# -i  install
# -v  verbose (más output)
# -h  hash marks (barra de progreso con #)

# Actualizar (instala si no existe, actualiza si ya existe)
sudo rpm -Uvh package.rpm

# Freshen (solo actualiza, NO instala si no existe)
sudo rpm -Fvh package.rpm

# Múltiples .rpm a la vez
sudo rpm -ivh package1.rpm package2.rpm
```

### Errores comunes al instalar

```
error: Failed dependencies:
        libfoo.so is needed by package
```
→ El paquete requiere `libfoo` que no está instalada.

```
error: package is already installed
```
→ Ya existe una versión del paquete. Usar `-Uvh` para actualizar.

```
error: file /usr/bin/foo conflicts with file from package bar-1.0
```
→ Dos paquetes intentan instalar el mismo archivo.

**Nunca usar `--nodeps` para evitar errores de dependencias** — el paquete no funcionará correctamente.

### Desinstalar

```bash
# Eliminar un paquete
sudo rpm -e nginx
# o equivalentemente:
sudo rpm --erase nginx

# Si otros paquetes dependen de él:
# error: Failed dependencies:
#         nginx is needed by (installed) nginx-mod-http-geoip
# → Eliminar primero los dependientes, o usar dnf remove
```

## Consultas con rpm -q

`-q` es el query mode. Siempre consulta la base de datos de paquetes instalados.

### Paquetes instalados

```bash
# ¿Está instalado? (muestra nombre si existe, error si no)
rpm -q nginx
# nginx-1:1.22.1-4.module+el9.x86_64

# Package not installed:
rpm -q nonexistent
# package nonexistent is not installed

# Listar TODOS los paquetes instalados
rpm -qa

# Contar cuántos paquetes hay
rpm -qa | wc -l

# Buscar con patrón (globs)
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
# Install Date: Sun 17 Mar 2024 02:30:00 PM GMT
# Group       : System Environment/Daemons
# Size        : 1234567
# License     : BSD
# Signature   : RSA/SHA256, Mon 01 Jan 2024 00:00:00 AM GMT
# Source RPM  : nginx-1.22.1-4.module+el9.src.rpm
# Build Host  : build.almalinux.org
# Packager    : AlmaLinux Packaging Team
# Vendor      : AlmaLinux
# URL         : https://nginx.org
# Summary     : A high performance web server and reverse proxy server
# Description :
# Nginx is a free, open-source, high-performance HTTP server and...
```

### Listar archivos (-ql)

```bash
# Todos los archivos que instaló un paquete
rpm -ql nginx
# /usr/sbin/nginx
# /usr/lib64/nginx/modules/
# /usr/share/nginx/html/index.html
# /etc/nginx/nginx.conf
# /var/log/nginx/access.log
# ...

# Solo archivos de CONFIGURACIÓN
rpm -qc nginx
# /etc/nginx/nginx.conf
# /etc/nginx/mime.types
# /etc/logrotate.d/nginx
# /etc/sysconfig/nginx

# Solo DOCUMENTACIÓN
rpm -qd nginx
# /usr/share/doc/nginx/CHANGES
# /usr/share/doc/nginx/README
# /usr/share/man/man8/nginx.8.gz

# Solo Scripts (postinst, preinst, etc.)
rpm -q --scripts nginx

# Archivos que son documentación o configuración
rpm -qlc nginx
```

### Quién provee un archivo (-qf)

```bash
# ¿A qué paquete pertenece este archivo?
rpm -qf /usr/sbin/nginx
# nginx-1:1.22.1-4.module+el9.x86_64

rpm -qf /usr/bin/curl
# curl-7.76.1-26.el9.x86_64

rpm -qf /etc/passwd
# setup-2.13.7-9.el9.noarch

# Archivos NO instalados via rpm
rpm -qf /usr/local/bin/my-script
# file /usr/local/bin/my-script is not owned by any package
# (fué creado manualmente, no via package manager)
```

### Dependencias (-qR y --whatrequires)

```bash
# Dependencias de un paquete (qué necesita PARA funcionar)
rpm -qR nginx
# /bin/sh
# libc.so.6()(64bit)
# libpcre2-8.so.0()(64bit)
# nginx-core
# systemd
# ...

# Paquetes que DEPENDEN de este (reverse dependencies)
rpm -q --whatrequires curl
# python3-pycurl-7.43.0.6-6.el9.x86_64
# curl-7.76.1-26.el9.x86_64 (se dependencia de sí mismo)

# Paquetes que requieren una librería
rpm -q --whatrequires libssl.so.3
# postgresql-libs-16.1-1.el9.x86_64
# nginx-1:1.22.1-4.module+el9.x86_64
# ...

# Qué proporciona un paquete (capabilities)
rpm -q --provides postgresql-server
# postgresql-server = 16.1-1.el9
# /usr/bin/pg_ctl
# /usr/bin/initdb
# ...
```

### Consultar un .rpm sin instalar (-qp)

```bash
# -p = package (archivo .rpm, no instalado)

rpm -qpi package.rpm      # info del .rpm
rpm -qpl package.rpm      # listar archivos
rpm -qpR package.rpm      # dependencias que requiere
rpm -qpc package.rpm      # archivos de configuración
rpm -qpd package.rpm      # documentación
rpm -qp --scripts package.rpm  # scripts
rpm -qp --provides package.rpm # capabilities que provee

# Ejemplo: inspeccionar antes de instalar
rpm -qpi /tmp/nginx-1.22.1-4.el9.x86_64.rpm
rpm -qpl /tmp/nginx-1.22.1-4.el9.x86_64.rpm | head -20
```

## Verificar integridad de archivos (-V)

```bash
# Verificar archivos de un paquete vs los originales
rpm -V nginx
# (sin output = todo OK)

# Archivos de configuración modificados son REPORTADOS
# Esto es normal (el admin los editó):
# S.5....T.  c /etc/nginx/nginx.conf
#   ^         ^
#   |         └── tipo: c=conffile, d=doc, l=license
#   checksum md5 diferente

# Verificar TODOS los paquetes (toma tiempo)
sudo rpm -Va

# Verificar solo binarios (ignorar configs)
rpm -Va | grep -v ' c '
```

### Códigos de verificación en detalle

```
S.5....T.  c /etc/nginx/nginx.conf
||  ||  ||  |
||  ||  ||  └─ Tipo de archivo
||  ||  ||      c = conffile (config, esperado que cambie)
||  ||  ||      d = documentation
||  ||  ||      g = ghost (archivo no presente en el .rpm)
||  ||  ||      l = license
||  ||  ||      r = readme
||  ||  |└── T = mtime cambió
||  ||  └── (reserved)
||  |└── 5 = digest/checksum MD5/SHA256 diferente
||  └── (reserved)
|└── S = size diferente
└── . = test pasó OK (punto = todo bien)
```

| Código | Significado | ¿Normal si es un binario? |
|---|---|---|
| `S` | Tamaño del archivo cambió | ❌ Alerta |
| `5` | Checksum (MD5/SHA256) diferente | ❌ Alerta |
| `L` | Symlink path cambió | ❌ Alerta |
| `T` | mtime (fecha modificación) cambió | ⚠️ Probablemente normal |
| `D` | Device major/minor cambió | ❌ Raro |
| `U` | User owner cambió | ⚠️ Puede ser normal |
| `G` | Group owner cambió | ⚠️ Puede ser normal |
| `M` | Mode (permisos) cambió | ⚠️ Puede ser normal |
| `.` | Test pasó OK | ✅ Bien |

```bash
# Es normal: archivos de configuración (tipo c) aparecen modificados
rpm -V nginx | grep ' c '
# S.5....T.  c /etc/nginx/nginx.conf

# Es ALERTA: binarios en /usr/bin o /usr/sbin modificados
rpm -V bash | grep -v ' c '
# Si muestra algo, investigate immediately
```

## Verificar firmas GPG (-K)

```bash
# Verificar que el .rpm fue firmado por un repositorio confiable
rpm -K package.rpm
# package.rpm: digests signatures OK

# Verificación extendida (mostrar detalles)
rpm -Kv package.rpm

# Salida sin firma o con clave faltante:
# package.rpm: RSA sha256 (MD5) PGP NOT OK (MISSING KEYS: GPG-KEY-xxx)
# → Importar la clave del repositorio:
sudo rpm --import https://repo.example.com/gpg.key

# Verificar firma de un paquete YA INSTALADO
rpm -qi --scripts nginx | head
```

## Scripts de paquete

```bash
# Ver TODOS los scripts de un paquete
rpm -q --scripts nginx
# preinstall scriptlet (%pre):
#   getent group nginx > /dev/null || groupadd -r nginx
#   getent passwd nginx > /dev/null || useradd -r -g nginx -s /sbin/nologin ...
# postinstall scriptlet (%post):
#   systemctl preset nginx.service
#   [ -z "$(cat /etc/nginx/nginx.conf)" ] && ...
# preuninstall scriptlet (%preun):
#   systemctl --no-reload disable nginx.service 2>/dev/null || :
#   systemctl stop nginx.service 2>/dev/null || :
# postuninstall scriptlet (%postun):
#   systemctl daemon-reload
# verify scriptlet (%verify):
#   (empty)

# Ver solo el preinstall
rpm -q --scripts --noscripts nginx  # para probar

# Triggers (se ejecutan cuando OTRO paquete cambia algo)
rpm -q --triggers nginx
```

## Extraer un .rpm sin instalar (rpm2cpio)

```bash
# rpm2cpio convierte un .rpm a formato cpio
rpm2cpio package.rpm | cpio -idmv
# -i  extract
# -d  create directories as needed
# -m  preserve modification time
# -v  verbose

# Ejemplo práctico: extraer contenido de un .rpm
cd /tmp
dnf download nginx
rpm2cpio nginx-*.rpm | cpio -idmv
ls usr/ etc/ var/
```

## La base de datos rpm

```bash
# Ubicación
ls /var/lib/rpm/
# Basenames     → nombres base de archivos
# Conflictname  → nombres de conflictos
# Dirnames      → directorios
# Groups        → grupos de paquetes
# Installtid    → orden de instalación
# Name          → nombres de paquetes
# Packages      → base de datos principal (sqlite)
# Providename   → capabilities/provides
# Requirename   → dependencias
# Sha1header    → headers de paquetes
# Sigkeys       → claves GPG
# Triggername   → triggers

# Verificar la base de datos
rpm -Va 2>&1 | head
# rpmdb: Warning:佛山市: cookie file (var/lib/rpm/.dbenv.lock)
# rpm database is up to date

# Reconstruir la base de datos (en caso de corrupción)
sudo rpm --rebuilddb
```

---

## Comparación rpm vs dpkg

| Operación | dpkg (Debian) | rpm (RHEL) |
|---|---|---|
| Instalar | `dpkg -i pkg.deb` | `rpm -ivh pkg.rpm` |
| Desinstalar | `dpkg -r pkg` | `rpm -e pkg` |
| Purge | `dpkg -P pkg` | No aplica (rpm no separa config) |
| Listar instalados | `dpkg -l` | `rpm -qa` |
| Info paquete | `dpkg -s pkg` | `rpm -qi pkg` |
| Archivos instalados | `dpkg -L pkg` | `rpm -ql pkg` |
| Archivos config | `dpkg -L pkg` \| grep /etc/ | `rpm -qc pkg` |
| Quién tiene archivo | `dpkg -S file` | `rpm -qf file` |
| Verificar archivos | `dpkg -V pkg` | `rpm -V pkg` |
| Verificar firma | `dpkg -I pkg.deb` | `rpm -K pkg.rpm` |
| Consultar .deb/.rpm | `dpkg -I pkg.deb` | `rpm -qpi pkg.rpm` |
| Scripts | `dpkg -e pkg.deb /tmp/` | `rpm -q --scripts pkg` |
| Dependencias | `dpkg -I pkg.deb` | `rpm -qR pkg` |
| Deps inversas | `apt-cache rdepends pkg` | `rpm -q --whatrequires pkg` |

---

## Ejercicios

### Ejercicio 1 — Consultas básicas

```bash
# 1. ¿Cuántos paquetes hay instalados?
rpm -qa | wc -l

# 2. ¿A qué paquete pertenece /usr/bin/bash?
rpm -qf /usr/bin/bash

# 3. ¿A qué paquete pertenece /etc/shadow?
rpm -qf /etc/shadow

# 4. Archivos que instaló el paquete bash
rpm -ql bash | head -15

# 5. Archivos de configuración de bash
rpm -qc bash
```

### Ejercicio 2 — Anatomía de un paquete

```bash
# 1. Elegir un paquete instalado
PKG=bash

# 2. Ver toda la información
rpm -qi $PKG

# 3. Extraer epoch, version, release, arch por separado
rpm -q --queryformat 'Name: %{NAME}\n' $PKG
rpm -q --queryformat 'Epoch: %{EPOCH}\n' $PKG
rpm -q --queryformat 'Version: %{VERSION}\n' $PKG
rpm -q --queryformat 'Release: %{RELEASE}\n' $PKG
rpm -q --queryformat 'Arch: %{ARCH}\n' $PKG
rpm -q --queryformat 'Epoch:Version-Release: %{EPOCH}:%{VERSION}-%{RELEASE}\n' $PKG

# 4. ¿Cuándo se instaló?
rpm -q --queryformat 'Install date: %{INSTALLTIME}\n' $PKG
rpm -q --queryformat 'Install date: %{INSTALLTIME:date}\n' $PKG
```

### Ejercicio 3 — Verificar integridad

```bash
# 1. Verificar un paquete crítico del sistema
rpm -V bash

# 2. Verificar openssh-server
rpm -V openssh-server 2>/dev/null || rpm -V openssh 2>/dev/null

# 3. Ver TODOS los binarios que cambiaron (ignorar configs)
rpm -Va | grep -v ' c '

# 4. ¿Cuántos archivos de config están modificados?
rpm -Va | grep ' c ' | wc -l

# 5. Ver solo archivos en /usr/bin que cambiaron
rpm -Va | grep '/usr/bin/' | grep -v ' c '
```

### Ejercicio 4 — Inspeccionar un .rpm sin instalar

```bash
# 1. Crear directorio de trabajo
mkdir /tmp/rpm-test && cd /tmp/rpm-test

# 2. Descargar un .rpm (elegir uno disponible)
dnf download nginx

# 3. Ver información
rpm -qpi nginx-*.rpm

# 4. Listar archivos
rpm -qpl nginx-*.rpm | head -20

# 5. Ver dependencias
rpm -qpR nginx-*.rpm

# 6. Ver qué provee
rpm -qp --provides nginx-*.rpm

# 7. Extraer sin instalar
rpm2cpio nginx-*.rpm | cpio -idmv
ls -la usr/ etc/

# 8. Limpiar
cd ~ && rm -rf /tmp/rpm-test
```

### Ejercicio 5 — Scripts de paquete

```bash
# 1. Elegir un paquete instalado
PKG=postfix

# 2. Ver todos los scripts
rpm -q --scripts $PKG

# 3. ¿Qué hace el preinstall?
rpm -q --queryformat '%{PREIN}\n' $PKG

# 4. ¿Qué hace el postinstall?
rpm -q --queryformat '%{POSTIN}\n' $PKG

# 5. ¿Y cuando se desinstala?
rpm -q --queryformat '%{POSTUN}\n' $PKG

# 6. Elegir nginx y ver qué hace cuando se reinstala
# preuninstall: stop service
# postuninstall: daemon-reload
```

### Ejercicio 6 — Dependencias invertidas

```bash
# 1. Elegir una librería (ej: openssl)
# ¿Cuántos paquetes dependen de ella?
rpm -q --whatrequires libssl.so.3 | wc -l

# 2. Listar los primeros 10
rpm -q --whatrequires libssl.so.3 | head -10

# 3. Elegir un paquete crítico
# ¿Qué requiere?
rpm -qR systemd

# 4. Elegir httpd
# ¿Qué capacidades provee?
rpm -q --provides httpd
```

### Ejercicio 7 — Simular un problema de rpm

```bash
# SIMULAR (en entorno de prueba, NO en producción):
# 1. Descargar un .rpm
cd /tmp
dnf download nginx

# 2. Intentar instalar directamente con rpm (va a fallar por deps)
sudo rpm -ivh nginx-*.rpm
# Ver el error de dependencias

# 3. Ahora instalar con dnf (que sí resuelve)
sudo dnf install -y ./nginx-*.rpm

# 4. Verificar que está instalado
rpm -q nginx

# 5. Limpiar
rm -f /tmp/nginx-*.rpm
```
