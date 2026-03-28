# T04 — rpm

## Errata y mejoras sobre el material base

1. **Epoch de nginx mal explicado** — El material dice que nginx tiene epoch 1
   "porque saltó números de versión (1.20.x → 1.22.1)". Eso no justifica un epoch.
   El epoch se usa cuando el **esquema de versionado** cambia y las versiones
   nuevas ordenarían por debajo de las antiguas en comparación lexicográfica.
   Ejemplo clásico: un paquete pasa de `20240101` (fecha) a `2.0` (semántica);
   sin epoch, `2.0` < `20240101`.

2. **`fué`** → `fue` (monosílabo, sin tilde según la RAE).

3. **`se dependencia de sí mismo`** → `depende de sí mismo` (error gramatical).

4. **`rpm -qi --scripts nginx` bajo "Verificar firma de un paquete YA INSTALADO"** —
   Ese comando muestra info + scripts, no verifica firmas. `rpm -K` solo funciona
   sobre archivos `.rpm`, no sobre paquetes ya instalados. Para un paquete instalado,
   el campo `Signature` aparece en la salida de `rpm -qi`.

5. **`rpm -q --scripts --noscripts nginx`** — Flags contradictorios.
   `--noscripts` es un flag de instalación/desinstalación (`rpm -ivh --noscripts`)
   que evita ejecutar scripts. No tiene sentido combinarlo con `--scripts` en modo
   query (`-q`).

6. **`佛山市`** — Caracteres chinos corruptos en la advertencia de rpmdb. Eliminados.

7. **Diagrama de posiciones `rpm -V` incompleto** — Los diagramas solo muestran
   S, 5 y T, marcando las demás posiciones como "reserved". En realidad hay **9
   posiciones definidas**: S, M, 5, D, L, U, G, T, P. Corregido con el diagrama
   completo.

---

## Qué es rpm

`rpm` (Red Hat Package Manager) es la herramienta de **bajo nivel** que instala
realmente los paquetes `.rpm`. La relación con dnf es la misma que dpkg con apt:

```
dnf install nginx
  │
  ├─[1] libsolv resuelve dependencias → necesita nginx, libssl3, pcre2
  ├─[2] dnf descarga los .rpm de los repositorios
  └─[3] rpm -ivh → instala cada .rpm en orden de dependencia
```

```
┌───────────────────────────────────────────────────────┐
│                   dnf (capa alta)                     │
│   install, remove, upgrade, search, module...         │
│   Resuelve dependencias con libsolv (SAT solver)      │
├───────────────────────────────────────────────────────┤
│                   rpm (capa baja)                     │
│   -i (install), -e (erase), -V (verify), -q (query)  │
│   Instala/elimina .rpm tal cual, sin resolución deps  │
└───────────────────────────────────────────────────────┘
```

**Diferencia crítica:** si `libssl3` no está instalado y ejecutas `rpm -i nginx.rpm`,
falla con error de dependencias. DNF resuelve e instala todas las dependencias
antes de llamar a rpm.

### Cuándo usar rpm directamente

```bash
# Consultar información de paquetes instalados
rpm -qa                     # listar todos
rpm -qi nginx               # info detallada
rpm -ql nginx               # archivos instalados

# Verificar integridad de archivos
rpm -V nginx

# Inspeccionar un .rpm sin instalar (auditoría)
rpm -qpi paquete.rpm        # info
rpm -qpl paquete.rpm        # archivos

# Extraer contenido de un .rpm sin instalar
rpm2cpio paquete.rpm | cpio -idmv

# Verificar firma GPG de un .rpm
rpm -K paquete.rpm
```

---

## Anatomía de un nombre de paquete RPM

```
nginx-1.22.1-4.module+el9.x86_64.rpm
^^^^^  ^^^^^^ ^^^^^^^^^^^^^^^^ ^^^^^^
name   version    release       arch

name:    nginx              — nombre del software
version: 1.22.1             — versión upstream (decidida por nginx.org)
release: 4.module+el9       — build del empaquetador (distro, módulo, etc.)
arch:    x86_64             — arquitectura (o noarch, i686, aarch64)
```

### Version vs Release

```bash
# version: lo que decidió el upstream (nginx.org, bash.org, etc.)
# release: cómo lo empaquetó la distribución

# Ejemplos de release:
# 9.el9         → RHEL 9 / AlmaLinux 9
# 3.fc39        → Fedora 39
# 4.module+el9  → module stream de RHEL 9
# 1.rc1         → release candidate
```

### Epoch (campo oculto)

```bash
# El epoch es un número entero que tiene PRIORIDAD ABSOLUTA sobre version
# en las comparaciones de versiones.
#
# Se usa cuando el esquema de versionado upstream cambia de forma que
# las versiones nuevas ordenarían DEBAJO de las antiguas.
#
# Ejemplo: un paquete pasa de versiones tipo 20240101 (fecha) a 2.0 (semántica).
# Sin epoch: 2.0 < 20240101 (lexicográficamente). Con epoch=1: 1:2.0 > 20240101.

# Sin epoch visible (epoch=0 implícito, no se muestra):
bash-5.1.8-9.el9.x86_64

# Con epoch visible (notación epoch:version-release):
nginx-1:1.22.1-4.module+el9.x86_64
# epoch=1, version=1.22.1, release=4.module+el9

# Ver el epoch de un paquete
rpm -q --queryformat '%{EPOCH}:%{VERSION}-%{RELEASE}\n' nginx
# 1:1.22.1-4.module+el9.x86_64

# epoch (none) = 0 implícito
rpm -q --queryformat '%{EPOCH}:%{VERSION}-%{RELEASE}\n' bash
# (none):5.1.8-9.el9
```

---

## Instalar y desinstalar con rpm

### Instalar

```bash
# Instalación básica
sudo rpm -ivh paquete.rpm
# -i  install
# -v  verbose (más output)
# -h  hash marks (barra de progreso con #)

# Actualizar (instala si no existe, actualiza si ya existe)
sudo rpm -Uvh paquete.rpm

# Freshen (SOLO actualiza, NO instala si no existe)
sudo rpm -Fvh paquete.rpm

# Múltiples .rpm a la vez
sudo rpm -ivh paquete1.rpm paquete2.rpm
```

### Errores comunes

```
error: Failed dependencies:
        libfoo.so is needed by paquete
```
→ El paquete requiere `libfoo` que no está instalada.
**Solución:** usar `dnf install ./paquete.rpm` que resuelve dependencias.

```
error: package is already installed
```
→ Ya existe una versión. Usar `-Uvh` para actualizar.

```
error: file /usr/bin/foo conflicts with file from package bar-1.0
```
→ Dos paquetes intentan instalar el mismo archivo.

**Nunca usar `--nodeps` para evitar errores de dependencias** — el paquete
quedará instalado sin sus bibliotecas y no funcionará correctamente.

### Desinstalar

```bash
# Eliminar un paquete
sudo rpm -e nginx
# equivalente:
sudo rpm --erase nginx

# Si otros paquetes dependen de él:
# error: Failed dependencies:
#         nginx is needed by (installed) nginx-mod-http-geoip
# → Eliminar primero los dependientes, o usar dnf remove
```

### rpm -i vs dnf install

```bash
# SIEMPRE preferir dnf para instalar:
sudo dnf install ./paquete.rpm
# dnf resuelve e instala las dependencias automáticamente
# También registra la transacción en el historial (dnf history)

# rpm -ivh solo se usa cuando:
# - No hay acceso a repositorios
# - Necesitas instalar un .rpm específico sin resolver deps
# - Estás en un entorno de rescate
```

---

## Consultas con rpm -q

`-q` es el modo query. Siempre consulta la base de datos de paquetes instalados
(salvo con `-p` que consulta un archivo `.rpm`).

### Resumen de flags de query

| Flag | Significado | Ejemplo |
|------|-------------|---------|
| `-q` | ¿Está instalado? | `rpm -q nginx` |
| `-qa` | Listar todos los instalados | `rpm -qa` |
| `-qi` | Información detallada | `rpm -qi nginx` |
| `-ql` | Archivos instalados | `rpm -ql nginx` |
| `-qc` | Solo archivos de configuración | `rpm -qc nginx` |
| `-qd` | Solo documentación | `rpm -qd nginx` |
| `-qf` | ¿A qué paquete pertenece un archivo? | `rpm -qf /usr/bin/curl` |
| `-qR` | Dependencias (qué necesita) | `rpm -qR nginx` |
| `--whatrequires` | Dependencias inversas (quién lo necesita) | `rpm -q --whatrequires bash` |
| `--provides` | Capabilities que provee | `rpm -q --provides nginx` |
| `--scripts` | Scripts pre/post install/uninstall | `rpm -q --scripts nginx` |
| `-qp` + flag | Consultar un `.rpm` sin instalar | `rpm -qpi paquete.rpm` |

### Paquetes instalados

```bash
# ¿Está instalado?
rpm -q nginx
# nginx-1:1.22.1-4.module+el9.x86_64

rpm -q paquete-inexistente
# package paquete-inexistente is not installed

# Listar TODOS los paquetes instalados
rpm -qa

# Contar paquetes
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

### Listar archivos (-ql, -qc, -qd)

```bash
# Todos los archivos instalados por un paquete
rpm -ql nginx
# /usr/sbin/nginx
# /usr/lib64/nginx/modules/
# /usr/share/nginx/html/index.html
# /etc/nginx/nginx.conf
# ...

# Solo archivos de CONFIGURACIÓN
rpm -qc nginx
# /etc/nginx/nginx.conf
# /etc/nginx/mime.types
# /etc/logrotate.d/nginx

# Solo DOCUMENTACIÓN
rpm -qd nginx
# /usr/share/doc/nginx/CHANGES
# /usr/share/doc/nginx/README
# /usr/share/man/man8/nginx.8.gz
```

### Encontrar el paquete dueño de un archivo (-qf)

```bash
# ¿A qué paquete pertenece este archivo?
rpm -qf /usr/sbin/nginx
# nginx-1:1.22.1-4.module+el9.x86_64

rpm -qf /usr/bin/curl
# curl-7.76.1-26.el9.x86_64

rpm -qf /etc/passwd
# setup-2.13.7-9.el9.noarch

# Archivo NO gestionado por rpm:
rpm -qf /usr/local/bin/my-script
# file /usr/local/bin/my-script is not owned by any package
# (fue creado manualmente, no vía package manager)
```

### Dependencias (-qR y --whatrequires)

```bash
# Dependencias de un paquete (qué necesita para funcionar)
rpm -qR nginx
# /bin/sh
# libc.so.6()(64bit)
# libpcre2-8.so.0()(64bit)
# nginx-core
# systemd
# ...

# Dependencias inversas (qué paquetes DEPENDEN de este)
rpm -q --whatrequires bash
# system-release-9.3-1.el9.x86_64
# util-linux-core-2.37.4-9.el9.x86_64
# ...

# Paquetes que requieren una librería específica
rpm -q --whatrequires libssl.so.3
# postgresql-libs-16.1-1.el9.x86_64
# nginx-1:1.22.1-4.module+el9.x86_64

# Qué capabilities provee un paquete
rpm -q --provides nginx
# nginx = 1:1.22.1-4.module+el9
# nginx(x86-64) = 1:1.22.1-4.module+el9
# ...
```

### Consultar un .rpm sin instalar (-qp)

```bash
# -p = consultar un archivo .rpm (no instalado)
rpm -qpi paquete.rpm       # info
rpm -qpl paquete.rpm       # archivos que instalaría
rpm -qpR paquete.rpm       # dependencias que requiere
rpm -qpc paquete.rpm       # archivos de configuración
rpm -qpd paquete.rpm       # documentación
rpm -qp --scripts paquete.rpm   # scripts
rpm -qp --provides paquete.rpm  # capabilities que provee

# Caso de uso: inspeccionar antes de instalar
rpm -qpi /tmp/nginx-1.22.1-4.el9.x86_64.rpm
rpm -qpl /tmp/nginx-1.22.1-4.el9.x86_64.rpm | head -20
```

### Queryformat (consultas programáticas)

```bash
# --queryformat permite extraer campos específicos
rpm -q --queryformat '%{NAME} %{VERSION}-%{RELEASE} %{ARCH}\n' bash
# bash 5.1.8-9.el9 x86_64

# Todos los campos disponibles
rpm --querytags | head -20
# ARCH, BASENAMES, BUILDHOST, BUILDTIME, ...

# Fecha de instalación legible
rpm -q --queryformat 'Installed: %{INSTALLTIME:date}\n' bash
# Installed: Thu 01 Feb 2024 03:14:00 PM GMT

# Listar paquetes ordenados por tamaño
rpm -qa --queryformat '%{SIZE} %{NAME}\n' | sort -rn | head -10

# Epoch + Name-Version-Release completo
rpm -q --queryformat '%{EPOCH}:%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' bash
```

---

## Verificar integridad de archivos (rpm -V)

```bash
# Verificar que los archivos de un paquete no fueron modificados
rpm -V nginx
# (sin output = todo OK)

# Si hay modificaciones:
# S.5....T.  c /etc/nginx/nginx.conf

# Verificar TODOS los paquetes (lento)
sudo rpm -Va

# Verificar solo binarios (ignorar configs)
rpm -Va | grep -v ' c '
```

### Las 9 posiciones de verificación

Cada posición indica un tipo de verificación diferente:

```
S.5....T.  c /etc/nginx/nginx.conf
|||||||||  |
|||||||||  └── tipo: c=config, d=doc, g=ghost, l=license, r=readme
||||||||└── P = caPabilities (atributos de seguridad)
|||||||└──  T = mTime (fecha de modificación cambió)
||||||└───  G = Group ownership cambió
|||||└────  U = User ownership cambió
||||└─────  L = readLink (symlink cambió)
|||└──────  D = Device major/minor cambió
||└───────  5 = digest (MD5/SHA256 cambió = contenido modificado)
|└────────  M = Mode (permisos/tipo cambió)
└─────────  S = Size (tamaño cambió)
```

| Pos | Código | Significado | ¿Normal en binario? |
|-----|--------|-------------|---------------------|
| 1 | `S` | Tamaño cambió | Alerta |
| 2 | `M` | Permisos/tipo cambió | Investigar |
| 3 | `5` | Checksum (MD5/SHA256) diferente | Alerta |
| 4 | `D` | Device major/minor cambió | Raro |
| 5 | `L` | Symlink path cambió | Alerta |
| 6 | `U` | User owner cambió | Investigar |
| 7 | `G` | Group owner cambió | Investigar |
| 8 | `T` | mtime cambió | Probablemente normal |
| 9 | `P` | Capabilities cambió | Investigar |
| — | `.` | Test pasó OK | OK |
| — | `?` | Test no se pudo realizar | — |

```bash
# Archivos de configuración (tipo c) modificados = NORMAL
# El administrador los editó intencionalmente
rpm -V nginx | grep ' c '
# S.5....T.  c /etc/nginx/nginx.conf

# Binarios en /usr/bin o /usr/sbin modificados = ALERTA
rpm -V bash | grep -v ' c '
# Si muestra algo, investigar inmediatamente: posible compromiso del sistema

# Buscar binarios modificados en todo el sistema
rpm -Va 2>/dev/null | grep -v ' c ' | grep '/usr/bin/\|/usr/sbin/'
```

---

## Verificar firmas GPG (rpm -K)

```bash
# Verificar la firma GPG de un archivo .rpm
rpm -K paquete.rpm
# paquete.rpm: digests signatures OK

# Verificación con detalles
rpm -Kv paquete.rpm

# rpm --checksig (equivalente a rpm -K)
rpm --checksig paquete.rpm

# Si la clave no está importada:
# paquete.rpm: RSA sha256 (MD5) PGP NOT OK (MISSING KEYS: GPG-KEY-xxx)

# Importar la clave del repositorio
sudo rpm --import https://repo.example.com/gpg.key

# Las claves se almacenan como paquetes gpg-pubkey-*
rpm -qa gpg-pubkey*
# gpg-pubkey-xxxxxxxx-yyyyyyyy
```

**Nota:** `rpm -K` solo funciona sobre archivos `.rpm`, no sobre paquetes ya
instalados. Para un paquete instalado, el campo `Signature` aparece en la salida
de `rpm -qi`.

---

## Scripts de paquete

Los paquetes RPM pueden contener scripts que se ejecutan en momentos específicos
del ciclo de vida:

```bash
# Ver TODOS los scripts de un paquete
rpm -q --scripts nginx
# preinstall scriptlet (%pre):
#   getent group nginx > /dev/null || groupadd -r nginx
#   getent passwd nginx > /dev/null || useradd -r -g nginx -s /sbin/nologin ...
# postinstall scriptlet (%post):
#   systemctl preset nginx.service
# preuninstall scriptlet (%preun):
#   systemctl --no-reload disable nginx.service 2>/dev/null || :
#   systemctl stop nginx.service 2>/dev/null || :
# postuninstall scriptlet (%postun):
#   systemctl daemon-reload
```

### Orden de ejecución

```
Instalación:     %pre → se copian archivos → %post
Desinstalación:  %preun → se borran archivos → %postun
Actualización:   %pre(new) → archivos(new) → %post(new) → %preun(old) → archivos(old) → %postun(old)
```

### Consultar scripts individuales con queryformat

```bash
# Solo preinstall
rpm -q --queryformat '%{PREIN}\n' nginx

# Solo postinstall
rpm -q --queryformat '%{POSTIN}\n' nginx

# Solo preuninstall
rpm -q --queryformat '%{PREUN}\n' nginx

# Solo postuninstall
rpm -q --queryformat '%{POSTUN}\n' nginx

# Ver triggers (se ejecutan cuando OTRO paquete se instala/elimina)
rpm -q --triggers nginx
```

---

## Extraer un .rpm sin instalar (rpm2cpio)

```bash
# rpm2cpio convierte un .rpm a formato cpio (stream de archivos)
rpm2cpio paquete.rpm | cpio -idmv
# -i  extract
# -d  create directories as needed
# -m  preserve modification time
# -v  verbose

# Ejemplo práctico
mkdir /tmp/rpm-extract && cd /tmp/rpm-extract
dnf download nginx
rpm2cpio nginx-*.rpm | cpio -idmv
ls usr/sbin/        # → nginx
ls etc/nginx/       # → nginx.conf, mime.types
cd ~ && rm -rf /tmp/rpm-extract
```

**Caso de uso:** inspeccionar o extraer archivos individuales sin instalar el
paquete. Útil en auditorías y entornos de rescate.

---

## La base de datos rpm

```bash
# Ubicación (RHEL 9+ usa sqlite)
ls /var/lib/rpm/
# rpmdb.sqlite          → base de datos principal (sqlite3 en RHEL 9+)
# .rpm.lock             → lock file

# En RHEL 7/8 era Berkeley DB con múltiples archivos:
# Basenames, Conflictname, Dirnames, Group, Installtid,
# Name, Packages, Providename, Requirename, Sha1header, Sigkeys

# Reconstruir la base de datos (en caso de corrupción)
sudo rpm --rebuilddb

# La base de datos contiene:
# - Lista de todos los paquetes instalados
# - Archivos que cada paquete instaló
# - Dependencias y provides
# - Firmas y checksums originales (para rpm -V)
```

---

## Comparación rpm vs dpkg

| Operación | dpkg (Debian) | rpm (RHEL) |
|---|---|---|
| Instalar | `dpkg -i pkg.deb` | `rpm -ivh pkg.rpm` |
| Desinstalar | `dpkg -r pkg` | `rpm -e pkg` |
| Purge (borrar configs) | `dpkg -P pkg` | No aplica (rpm no separa config de remove) |
| Listar instalados | `dpkg -l` | `rpm -qa` |
| Info paquete | `dpkg -s pkg` | `rpm -qi pkg` |
| Archivos instalados | `dpkg -L pkg` | `rpm -ql pkg` |
| Archivos de config | `dpkg -L pkg` + filtrar | `rpm -qc pkg` (flag dedicado) |
| Dueño de archivo | `dpkg -S file` | `rpm -qf file` |
| Verificar integridad | `dpkg -V pkg` | `rpm -V pkg` |
| Verificar firma | N/A (dpkg no verifica firmas de .deb) | `rpm -K pkg.rpm` |
| Inspeccionar sin instalar | `dpkg -I pkg.deb` / `dpkg -c pkg.deb` | `rpm -qpi pkg.rpm` / `rpm -qpl pkg.rpm` |
| Scripts | Extraer con `dpkg -e pkg.deb /tmp/` | `rpm -q --scripts pkg` (flag dedicado) |
| Dependencias | `dpkg -I pkg.deb` | `rpm -qR pkg` |
| Deps inversas | `apt-cache rdepends pkg` | `rpm -q --whatrequires pkg` |
| Extraer sin instalar | `dpkg -x pkg.deb dir/` | `rpm2cpio pkg.rpm \| cpio -idmv` |
| Reconstruir DB | N/A | `rpm --rebuilddb` |

**Ventajas de rpm sobre dpkg:**
- Flag dedicado para config files (`-qc`) — dpkg requiere filtrar manualmente.
- Flag dedicado para scripts (`--scripts`) — dpkg requiere extraer el control archive.
- `--queryformat` para consultas programáticas — dpkg usa `dpkg-query -W -f`.
- `rpm -K` verifica firmas GPG de archivos `.rpm`.

---

## Labs

### Lab 1 — Consultas rpm -q

```bash
docker compose exec alma-dev bash -c '
echo "=== Anatomía de un nombre RPM ==="
echo ""
echo "Ejemplo: bash-5.1.8-9.el9.x86_64"
echo "  bash   = name"
echo "  5.1.8  = version (upstream)"
echo "  9.el9  = release (empaquetador + distro)"
echo "  x86_64 = arch"
echo ""
echo "--- Ejemplo real ---"
rpm -q bash
echo ""
echo "--- Con queryformat (cada campo separado) ---"
rpm -q --queryformat "Name: %{NAME}\nEpoch: %{EPOCH}\nVersion: %{VERSION}\nRelease: %{RELEASE}\nArch: %{ARCH}\n" bash
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -q: ¿está instalado? ==="
rpm -q bash
rpm -q curl
rpm -q paquete-inexistente
echo ""
echo "=== rpm -qa: listar todos ==="
echo "Total paquetes instalados: $(rpm -qa | wc -l)"
echo ""
echo "--- Buscar con patrón ---"
rpm -qa "python3*" | head -5
echo ""
echo "--- Buscar con grep ---"
rpm -qa | grep -i ssh | head -5
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qi: información detallada ==="
rpm -qi bash
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -ql: archivos instalados ==="
rpm -ql bash | head -15
echo "Total archivos: $(rpm -ql bash | wc -l)"
echo ""
echo "--- Solo config (rpm -qc) ---"
rpm -qc bash
echo ""
echo "--- Solo documentación (rpm -qd) ---"
rpm -qd bash | head -5
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qf: dueño de un archivo ==="
rpm -qf /usr/bin/bash
rpm -qf /usr/bin/curl
rpm -qf /usr/bin/env
rpm -qf /etc/passwd
echo ""
echo "--- Archivo no gestionado por rpm ---"
rpm -qf /usr/local/bin/algo 2>&1 || true
echo "  → fue instalado manualmente, no vía rpm"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm -qR: dependencias ==="
rpm -qR bash | head -10
echo "..."
echo ""
echo "=== Dependencias inversas ==="
echo "Paquetes que dependen de bash:"
rpm -q --whatrequires bash 2>/dev/null | head -5
'
```

### Lab 2 — Verificar integridad

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

```bash
docker compose exec alma-dev bash -c '
echo "=== Las 9 posiciones de rpm -V ==="
echo ""
echo "  S.5....T.  c /etc/nginx/nginx.conf"
echo "  |||||||||  |"
echo "  ||||||||└── P = caPabilities"
echo "  |||||||└──  T = mTime"
echo "  ||||||└───  G = Group"
echo "  |||||└────  U = User"
echo "  ||||└─────  L = readLink (symlink)"
echo "  |||└──────  D = Device"
echo "  ||└───────  5 = digest (checksum)"
echo "  |└────────  M = Mode (permisos)"
echo "  └─────────  S = Size"
echo ""
echo "Tipo: c=config, d=doc, g=ghost, l=license, r=readme"
echo ""
echo "Config (c) modificado = NORMAL"
echo "Binario modificado    = ALERTA de seguridad"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar todos los paquetes ==="
echo "(puede tardar unos segundos)"
echo ""
echo "--- Archivos NO de configuración modificados ---"
rpm -Va 2>/dev/null | grep -v " c " | head -10
echo ""
echo "--- Archivos de configuración modificados ---"
rpm -Va 2>/dev/null | grep " c " | head -10
echo ""
echo "Config (c) modificado: normal"
echo "Binario en /usr/bin modificado: investigar inmediatamente"
'
```

### Lab 3 — Scripts, .rpm sin instalar y comparación

```bash
docker compose exec alma-dev bash -c '
echo "=== Scripts de un paquete ==="
echo ""
pkg=$(rpm -qa | grep -E "^openssh-server|^systemd-" | head -1)
if [ -n "$pkg" ]; then
    echo "--- Scripts de $pkg ---"
    rpm -q --scripts "$pkg" 2>/dev/null | head -25
    echo "..."
else
    echo "--- Scripts de bash ---"
    scripts=$(rpm -q --scripts bash 2>/dev/null)
    if [ -z "$scripts" ]; then
        echo "(bash no tiene scripts de instalación)"
    else
        echo "$scripts"
    fi
fi
echo ""
echo "Tipos de scripts:"
echo "  %pre:    antes de instalar"
echo "  %post:   después de instalar"
echo "  %preun:  antes de desinstalar"
echo "  %postun: después de desinstalar"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Consultar .rpm sin instalar (-p) ==="
echo ""
echo "rpm -qpi paquete.rpm    → info"
echo "rpm -qpl paquete.rpm    → archivos"
echo "rpm -qpR paquete.rpm    → dependencias"
echo "rpm -qpc paquete.rpm    → config files"
echo ""
echo "--- Intentar descargar un .rpm ---"
mkdir -p /tmp/rpm-test
dnf download bash --destdir=/tmp/rpm-test 2>/dev/null
pkg=$(ls /tmp/rpm-test/*.rpm 2>/dev/null | head -1)
if [ -n "$pkg" ]; then
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

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar firma GPG ==="
echo ""
echo "rpm -K paquete.rpm → digests signatures OK"
echo "rpm --checksig     → equivalente"
echo ""
echo "Claves GPG importadas:"
rpm -qa gpg-pubkey* | head -3
echo ""
echo "Las claves se importan con:"
echo "  sudo rpm --import URL-de-la-clave"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== rpm vs dpkg ==="
printf "%-22s %-20s %-20s\n" "Operación" "dpkg" "rpm"
printf "%-22s %-20s %-20s\n" "---------------------" "-------------------" "-------------------"
printf "%-22s %-20s %-20s\n" "Instalar" "dpkg -i pkg.deb" "rpm -ivh pkg.rpm"
printf "%-22s %-20s %-20s\n" "Desinstalar" "dpkg -r pkg" "rpm -e pkg"
printf "%-22s %-20s %-20s\n" "Listar instalados" "dpkg -l" "rpm -qa"
printf "%-22s %-20s %-20s\n" "Info paquete" "dpkg -s pkg" "rpm -qi pkg"
printf "%-22s %-20s %-20s\n" "Archivos" "dpkg -L pkg" "rpm -ql pkg"
printf "%-22s %-20s %-20s\n" "Dueño de archivo" "dpkg -S file" "rpm -qf file"
printf "%-22s %-20s %-20s\n" "Verificar" "dpkg -V pkg" "rpm -V pkg"
printf "%-22s %-20s %-20s\n" "Config files" "dpkg -L + filtrar" "rpm -qc pkg"
printf "%-22s %-20s %-20s\n" "Scripts" "(extraer manual)" "rpm --scripts pkg"
echo ""
echo "Ventaja rpm: flags dedicados para -qc y --scripts"
'
```

---

## Ejercicios

### Ejercicio 1 — rpm en la jerarquía de paquetes

Dibuja un diagrama que muestre la relación entre rpm, dnf, libsolv y los
repositorios. Responde: ¿por qué rpm no puede reemplazar a dnf? ¿Y por qué
dnf no puede prescindir de rpm?

<details><summary>Predicción</summary>

```
Repositorios (.rpm files)
        │
        ▼
┌─────────────────────────┐
│        dnf              │  ← Capa alta: resuelve dependencias,
│   (libsolv SAT solver)  │    descarga .rpm, gestiona repos
└────────────┬────────────┘
             │  llama a rpm para cada .rpm
             ▼
┌─────────────────────────┐
│         rpm             │  ← Capa baja: instala/elimina .rpm,
│  (base de datos local)  │    registra en /var/lib/rpm/
└─────────────────────────┘
```

rpm no puede reemplazar a dnf porque no resuelve dependencias ni descarga
de repositorios. Si un paquete necesita 5 bibliotecas, rpm falla — dnf
las busca, descarga e instala en orden.

dnf no puede prescindir de rpm porque rpm es quien realmente instala los
archivos, ejecuta los scripts, y mantiene la base de datos local
(`/var/lib/rpm/`). dnf orquesta, rpm ejecuta.

</details>

### Ejercicio 2 — Anatomía del nombre RPM

Usa `rpm -q --queryformat` para extraer cada campo (Name, Epoch, Version,
Release, Arch) de tres paquetes distintos. ¿Cuántos tienen epoch distinto
de `(none)`?

<details><summary>Predicción</summary>

```bash
for pkg in bash curl openssl-libs; do
    rpm -q --queryformat \
      'Name=%{NAME} Epoch=%{EPOCH} Ver=%{VERSION} Rel=%{RELEASE} Arch=%{ARCH}\n' \
      "$pkg"
done
# bash:        Epoch=(none) → 0 implícito
# curl:        Epoch=(none) → 0 implícito
# openssl-libs: Epoch=1     → tiene epoch explícito
```

La mayoría de paquetes tienen epoch `(none)` (= 0 implícito). Los que
tienen epoch explícito son aquellos cuyo upstream cambió el esquema de
versionado en algún momento.

</details>

### Ejercicio 3 — Consultas -ql, -qc, -qd, -qf

Para el paquete `coreutils`: ¿cuántos archivos instaló en total?
¿Cuántos son de configuración? ¿Cuántos de documentación? Luego elige
3 archivos de `/usr/bin/` y verifica con `rpm -qf` que pertenecen a
coreutils.

<details><summary>Predicción</summary>

```bash
rpm -ql coreutils | wc -l          # ~100+ archivos
rpm -qc coreutils | wc -l          # 0-2 (coreutils tiene poca config)
rpm -qd coreutils | wc -l          # ~80+ (muchas man pages)

rpm -qf /usr/bin/ls                # coreutils-...
rpm -qf /usr/bin/cp                # coreutils-...
rpm -qf /usr/bin/chmod             # coreutils-...
```

Coreutils instala muchos binarios (`ls`, `cp`, `mv`, `chmod`, `cat`, etc.)
y muchas man pages, pero casi ningún archivo de configuración — su
comportamiento se controla por argumentos de línea de comandos, no por
archivos de config.

</details>

### Ejercicio 4 — Dependencias directas e inversas

¿Cuántas dependencias tiene `bash` (rpm -qR)? ¿Cuántos paquetes dependen
de bash (--whatrequires)? ¿Por qué bash tiene muchos dependientes inversos?

<details><summary>Predicción</summary>

```bash
rpm -qR bash | wc -l               # ~10-15 dependencias
rpm -q --whatrequires bash | wc -l  # muchos (50+)
```

bash tiene pocas dependencias propias (libc, libtinfo, filesystem...) pero
es un dependiente inverso muy demandado porque muchísimos paquetes ejecutan
scripts de shell (`/bin/sh`, `/bin/bash`) en sus `%pre`, `%post`, etc.
Cualquier paquete que tenga scripts de instalación depende implícitamente
de bash (o `/bin/sh`).

</details>

### Ejercicio 5 — Verificación de integridad (rpm -V)

Ejecuta `rpm -V bash` y `rpm -Va | grep -v ' c ' | head -10`. Interpreta
cada posición del output. ¿Por qué los archivos marcados con `c` no son
preocupantes?

<details><summary>Predicción</summary>

```bash
rpm -V bash
# (probablemente sin output = todo OK)

rpm -Va | grep -v ' c ' | head -10
# Puede mostrar archivos con T (mtime cambió) o similar
# Si aparece un binario con 5 (checksum): investigar
```

Interpretación del formato `S.5....T.`:
- Pos 1 (S): tamaño — Pos 2 (M): permisos — Pos 3 (5): checksum
- Pos 4 (D): device — Pos 5 (L): symlink — Pos 6 (U): user
- Pos 7 (G): group — Pos 8 (T): mtime — Pos 9 (P): capabilities

Los archivos `c` (config) no preocupan porque el administrador los edita
intencionalmente como parte de la configuración del sistema. Un cambio en
`/etc/nginx/nginx.conf` es esperado. Un cambio en `/usr/bin/ls` no lo es.

</details>

### Ejercicio 6 — Scripts de paquete

Usa `rpm -q --scripts` en dos paquetes (ej: `openssh-server` y `systemd`).
¿Qué hace el `%pre` de openssh-server? ¿Y el `%post`? ¿Qué patrón común
se repite?

<details><summary>Predicción</summary>

```bash
rpm -q --scripts openssh-server
# %pre:  crea usuario/grupo sshd (getent + useradd -r)
# %post: systemctl preset sshd.service
# %preun: systemctl disable/stop sshd.service
# %postun: systemctl daemon-reload
```

Patrón común en paquetes que instalan servicios:
1. `%pre`: crear usuario/grupo del servicio (useradd -r)
2. `%post`: `systemctl preset` para habilitar según presets
3. `%preun`: `systemctl disable && stop` el servicio
4. `%postun`: `systemctl daemon-reload`

Este patrón es el equivalente RPM de los maintainer scripts de dpkg
(preinst/postinst/prerm/postrm).

</details>

### Ejercicio 7 — Inspeccionar un .rpm sin instalar

Descarga un `.rpm` con `dnf download`, inspecciona con `rpm -qpi`,
`rpm -qpl`, `rpm -qpR`, y extrae con `rpm2cpio`. ¿Cuántos archivos
contiene? ¿Qué dependencias declara?

<details><summary>Predicción</summary>

```bash
mkdir /tmp/rpm-test && cd /tmp/rpm-test
dnf download bash
rpm -qpi bash-*.rpm       # muestra Name, Version, License, Description...
rpm -qpl bash-*.rpm       # lista todos los archivos
rpm -qpR bash-*.rpm       # dependencias: libc, libtinfo, filesystem...

rpm2cpio bash-*.rpm | cpio -idmv
ls -R usr/                # binarios en usr/bin/, man pages en usr/share/man/
cd ~ && rm -rf /tmp/rpm-test
```

Al extraer con rpm2cpio, se crean los directorios relativos (usr/, etc/)
que reflejan dónde se instalarían los archivos. Es como "ver el interior"
del paquete sin instalarlo.

</details>

### Ejercicio 8 — Firma GPG y claves

Lista las claves GPG importadas con `rpm -qa gpg-pubkey*`. ¿Cuántas hay?
¿De qué repositorios provienen? Muestra los detalles de una clave con
`rpm -qi gpg-pubkey-XXXXXXXX`.

<details><summary>Predicción</summary>

```bash
rpm -qa gpg-pubkey*
# gpg-pubkey-xxxxxxxx-yyyyyyyy  (una por cada repo confiable)

# Ver detalles de una clave
rpm -qi gpg-pubkey-xxxxxxxx-yyyyyyyy
# Name: gpg-pubkey
# Version: xxxxxxxx
# Summary: gpg(AlmaLinux OS 9 <packager@almalinux.org>)
# Description: -----BEGIN PGP PUBLIC KEY BLOCK-----
# ...
```

Cada clave gpg-pubkey corresponde a un repositorio que fue configurado
como confiable (importado con `rpm --import`). El dnf/yum las importa
automáticamente la primera vez que se instala desde un repo con
`gpgcheck=1`.

</details>

### Ejercicio 9 — Auditoría de seguridad con rpm

Diseña un flujo de auditoría de un sistema: ¿cómo usarías `rpm -Va`,
`rpm -qf`, y `rpm -V pkg` para detectar un posible compromiso?
¿Qué archivos nunca deberían aparecer modificados?

<details><summary>Predicción</summary>

```bash
# 1. Buscar binarios modificados (ignorar configs)
rpm -Va 2>/dev/null | grep -v ' c ' > /tmp/audit.txt

# 2. Filtrar solo archivos en rutas críticas
grep '/usr/bin/\|/usr/sbin/\|/usr/lib' /tmp/audit.txt

# 3. Para cada archivo sospechoso, verificar el paquete completo
rpm -qf /usr/bin/sospechoso     # ¿a qué paquete pertenece?
rpm -V paquete-sospechoso       # ¿qué más cambió del paquete?

# 4. Buscar archivos que no pertenecen a ningún paquete
find /usr/bin -type f -exec rpm -qf {} \; 2>&1 | grep "not owned"

# Archivos que NUNCA deberían estar modificados:
# - /usr/bin/ls, /usr/bin/ps, /usr/bin/netstat (coreutils, procps, net-tools)
# - /usr/sbin/sshd (openssh-server)
# - Librerías en /usr/lib64/
# Si el checksum (5) de alguno de estos difiere, el sistema
# probablemente está comprometido.
```

Limitación: si el atacante reemplazó también `rpm` o la base de datos
`/var/lib/rpm/`, esta verificación no es confiable. En ese caso, verificar
desde un medio externo (live USB, otro sistema).

</details>

### Ejercicio 10 — Panorama: rpm vs dpkg

Compara `rpm` con `dpkg` en al menos 8 operaciones. ¿Qué puede hacer
rpm que dpkg no puede (o hace de forma más directa)? ¿Y viceversa?
¿Cómo afecta la ausencia de `purge` en rpm?

<details><summary>Predicción</summary>

| Operación | dpkg | rpm | Diferencia |
|---|---|---|---|
| Instalar | `dpkg -i pkg.deb` | `rpm -ivh pkg.rpm` | rpm tiene -v (verbose) y -h (hash) integrados |
| Desinstalar | `dpkg -r pkg` | `rpm -e pkg` | Equivalentes |
| Purge | `dpkg -P pkg` | No existe | rpm borra todo; dpkg puede dejar configs |
| Config files | `dpkg -L` + filtrar | `rpm -qc pkg` | rpm tiene flag dedicado |
| Scripts | Extraer con `dpkg -e` | `rpm -q --scripts` | rpm tiene flag dedicado |
| Firma | N/A | `rpm -K pkg.rpm` | rpm verifica firmas GPG |
| Queryformat | `dpkg-query -W -f` | `rpm -q --queryformat` | Ambos, sintaxis diferente |
| Extraer | `dpkg -x pkg.deb dir/` | `rpm2cpio \| cpio` | dpkg más directo |

**rpm aventaja** en: flag dedicado para configs (`-qc`), scripts (`--scripts`),
verificación de firmas (`-K`), y `--queryformat` con más tags disponibles.

**dpkg aventaja** en: `purge` (borrar configs huérfanas), extracción directa
sin pipeline (`dpkg -x`), y separación explícita remove vs purge.

Sin `purge` en rpm, cuando se desinstala un paquete, los archivos de
configuración modificados se renombran a `.rpmsave` — quedan en disco pero
no se borran. dpkg con `purge` los elimina completamente.

</details>
