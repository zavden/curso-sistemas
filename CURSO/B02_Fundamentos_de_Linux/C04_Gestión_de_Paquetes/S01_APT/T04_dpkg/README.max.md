# T04 — dpkg

## Qué es dpkg

`dpkg` (Debian Package) es la herramienta de **bajo nivel** que maneja la instalación real de paquetes `.deb`. APT es la capa de alto nivel que le precede:

```
apt install nginx
  │
  ├─[1] apt resolve deps  → sabe que necesita nginx-common, libssl3, etc.
  ├─[2] apt download      → descarga .deb de internet
  └─[3] apt call dpkg     → dpkg -i nginx_1.22.1-9_amd64.deb
                              dpkg -i nginx-common_1.22.1-9_amd64.deb
                              dpkg -i libssl3_*.deb
```

```
┌─────────────────────────────────────────────────────┐
│                    apt (capa alta)                  │
│   update, install, remove, upgrade, search...     │
│   Resuelve dependencias, descarga de repositorios │
├─────────────────────────────────────────────────────┤
│                   dpkg (capa baja)                 │
│   -i (install), -r (remove), -P (purge)           │
│   Solo instala .deb que le pasen                   │
│   NO resuelve dependencias                          │
└─────────────────────────────────────────────────────┘
```

**Diferencia crítica:** si `libfoo` no está instalado y ejecutas `dpkg -i nginx.deb`, falla. APT hace `apt install nginx` → resuelve deps → llama `dpkg -i` para cada `.deb` en orden.

### Cuándo usar dpkg directamente

```bash
# Instalar un .deb descargado manualmente (sin repo)
sudo dpkg -i /tmp/google-chrome.deb

# Consultar info de paquetes instalados
dpkg -l
dpkg -L package
dpkg -S /path/to/file

# Verificar integridad de archivos
dpkg -V nginx

# Extraer .deb sin instalar (auditoría, análisis)
dpkg -x package.deb /tmp/dir/

# Reparar un sistema roto
sudo dpkg --configure -a
```

## Estructura interna de dpkg

```
/var/lib/dpkg/
├── info/           ← archivos de control por paquete (postinst, preinst, md5sums, etc.)
│   ├── nginx.postinst
│   ├── nginx.preinst
│   ├── nginx.prerm
│   ├── nginx.postrm
│   ├── nginx.md5sums
│   └── nginx.conffiles
├── list            ← lista de paquetes conocidos
├── status           ← estado actual de todos los paquetes
├── available       ← info de paquetes disponibles (de repos)
├── diversions       ← archivos desviados
├── alternatives/    ← sistema de alternatives (update-alternatives)
└── lock-frontend    ← lock file (previene instalaciones simultáneas)
```

## Códigos de estado en `dpkg -l`

```bash
dpkg -l
# Desired=Unknown/Install/Remove/Purge/Hold
# | Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
# ||/ Name  Version  Architecture  Description
# ii  bash  5.2-15   amd64        GNU Bourne Again SHell
```

### Primera columna: estado deseado (desired)

| Código | Significado |
|---|---|
| `u` | Unknown — estado desconocido |
| `i` | Install — se desea instalar |
| `h` | Hold — mantener en su versión actual |
| `r` | Remove — se desea desinstalar |
| `p` | Purge — se desea purgar |
| `F` | Failed — la operación falló |

### Segunda columna: estado actual (status)

| Código | Significado |
|---|---|
| `n` | Not — no instalado |
| `i` | Installed — instalado correctamente |
| `c` | Config-files — desinstalado, configs permanecen |
| `U` | Unpacked — descomprimido pero no configurado |
| `F` | Failed config — configuración falló |
| `W` | Wait — esperando trigger |
| `t` | Trig Pend — trigger pendiente |

### Combinaciones comunes

| Código | Significado |
|---|---|
| `ii` | Correctamente instalado |
| `rc` | Removido, archivos de configuración permanecen |
| `pn` | Purgado (nunca instalado o purgado completamente) |
| `hi` | En hold, instalado |
| `uU` | Descomprimido, no configurado (estado roto) |
| `iF` | Instalado pero falló la configuración |
| `?i` | Estado desconocido |

## Instalar y desinstalar

### Instalar un .deb

```bash
# Instalación directa (NO resuelve dependencias)
sudo dpkg -i package.deb

# Si falla por dependencias:
# dpkg: dependency problems prevent configuration of nginx:
#          nginx depends on libssl3 (>= 3.0); however:
#           Package libssl3 is not installed.

# Solución 1: usar apt para que resuelva deps
sudo apt install -f
# apt detecta el .deb parcialmente instalado y resuelve sus deps

# Solución 2: instalar deps manualmente primero
# (tedioso, no recomendado)

# Solución 3: usar apt con el .deb (MEJOR)
sudo apt install ./package.deb
# apt ./ = "instala desde archivo local, resuelve deps desde repos"
# El ./ es obligatorio: sin él busca el paquete en repos
```

### Desinstalar

```bash
# Remove (conserva configuración)
sudo dpkg -r package
# Equivalent to: dpkg --remove package

# Purge (elimina TODO, incluyendo configs)
sudo dpkg -P package
# Equivalent to: dpkg --purge package

# ⚠️ dpkg NO Previene desinstalar paquetes de los que otros dependen
# Si desinstalas glibc con dpkg, el sistema se rompe
# Usar apt remove para desinstalación segura
```

## Consultar paquetes instalados

### Listar paquetes

```bash
# Todos los paquetes
dpkg -l

# Filtrar solo instalados correctamente
dpkg -l | grep '^ii'

# Buscar un paquete específico
dpkg -l nginx
dpkg -l 'nginx*'       # wildcards

# Solo nombres (formato simple)
dpkg --get-selections
dpkg --get-selections | grep nginx
# nginx    install
# nginx-core    install

# Contar paquetes instalados
dpkg -l | grep '^ii' | wc -l
```

### Ver archivos de un paquete

```bash
# Todos los archivos que instaló un paquete
dpkg -L nginx
# /.
/etc/nginx/nginx.conf
/etc/nginx/mime.types
/usr/sbin/nginx
/usr/share/nginx/html/index.html
/var/log/nginx/access.log
/var/log/nginx/error.log

# Solo los binarios/ejecutables
dpkg -L nginx | grep -E '/bin/|/sbin/'
# /usr/sbin/nginx

# Solo archivos de configuración
dpkg -L nginx | grep -E '^/etc/'
```

### ¿A qué paquete pertenece un archivo?

```bash
# Buscar por ruta exacta
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx

dpkg -S /usr/bin/curl
# curl: /usr/bin/curl

# Patrón glob
dpkg -S '*nginx.conf*'
# nginx-common: /etc/nginx/nginx.conf

dpkg -S /etc/passwd
# base-files: /etc/passwd

# Archivos NO instalados via dpkg
dpkg -S /usr/local/bin/my-script
# dpkg-query: no path found matching pattern /usr/local/bin/my-script
# (fué creado manualmente, no via package manager)
```

### Información detallada de paquete instalado

```bash
dpkg -s nginx
# Package: nginx
# Status: install ok installed
# Priority: optional
# Section: httpd
# Installed-Size: 614
# Maintainer: nginx packagers <nginx@nginx.org>
# Architecture: amd64
# Version: 1.22.1-9
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28), libpcre2-8-0 (>= 10.33)
# Recommends: libPCRE2-32-0 (for 32-bit compatibility)
# Suggests: apache2-utils (for htpasswd, ab)
# Description: small, powerful, scalable web/proxy server
# ...

# Solo el campo que interesa
dpkg -s nginx | grep -E "^Version:|^Depends:|^Status:"
```

## Consultar archivos .deb (sin instalar)

```bash
# Información del paquete DENTRO del .deb
dpkg -I package.deb
dpkg --info package.deb
# Package: nginx
# Version: 1.22.1-9
# Architecture: amd64
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28)...
# Maintainer: ...
# Description: ...
# Binary: nginx-core, nginx-full, nginx-light, etc.

# Listar archivos que contiene el .deb (sin instalar)
dpkg -c package.deb
dpkg --contents package.deb
# drwxr-xr-x root/root    0 2023-06-01 10:00  etc/
# drwxr-xr-x root/root    0 2023-06-01 10:00  etc/nginx/
# -rw-r--r-- root/root  1234 2023-06-01 10:00  etc/nginx/nginx.conf
# -rwxr-xr-x root/root 8676 2023-06-01 10:00  usr/sbin/nginx
```

## Extraer un .deb sin instalar

### Método con dpkg

```bash
# Extraer el contenido (data.tar) a un directorio
dpkg -x package.deb /tmp/extracted/
ls /tmp/extracted/
# etc/  usr/  var/

# Extraer los scripts de control (preinst, postinst, prerm, postrm)
dpkg -e package.deb /tmp/control/
ls /tmp/control/
# control      # metadata del paquete
# conffiles    # lista de archivos de configuración
# postinst     # script post-instalación
# preinst      # script pre-instalación
# prerm        # script pre-desinstalación
# postrm       # script post-desinstalación
```

### Método manual (anatomía de un .deb)

```bash
# Un .deb es un archivo ar (equivalente a tar but binary)
ar x package.deb
# Genera 3 archivos:
ls
# debian-binary     ← versión del formato (2.0)
# control.tar.xz    ← scripts de control + metadata
# data.tar.xz       ← archivos reales del paquete

# Inspect
cat debian-binary        # 2.0

# Metadata
tar -tf control.tar.xz
# ./
# ./control
# ./postinst
# ./preinst
# ./prerm
# ./postrm
# ./conffiles
# ./md5sums

# Archivos del paquete
tar -tf data.tar.xz | head -20
# ./
# ./etc/
# ./etc/nginx/
# ./usr/

# Extraer data.tar.xz manualmente
mkdir /tmp/data && tar -xf data.tar.xz -C /tmp/data/
ls /tmp/data/
# etc/  usr/  var/
```

```bash
# AUDITORÍA: Revisar scripts de un .deb antes de instalar
# Especialmente importante para .deb de fuentes no oficiales

dpkg -e nginx_1.22.1-9_amd64.deb /tmp/nginx-control/
cat /tmp/nginx-control/postinst
# Commonly contains: enabling nginx service, updating cache, etc.

cat /tmp/nginx-control/prerm
# Commonly contains: stopping nginx service gracefully
```

## Verificar integridad de archivos

```bash
# Verificar archivos de un paquete vs los originales del package
dpkg -V nginx
# (sin output = todo OK)

# Los archivos de configuración EDITADOS son reportados como "modificados"
# Esto es normal y esperado:
# ??5?????? c /etc/nginx/nginx.conf
#   ^         ^
#   |         tipo: c = conffile
#   md5 diferente (archivo editado por admin)
```

### Códigos de verificación

```
??5?????? c /path/to/file

Posición 1: ¿Hubo error en el checksum?
  . = OK
  5 = MD5sum diferente
  S = Tamaño diferente
  T = mtime diferente

Posición 2: ¿Es un archivo de configuración?
  c = conffile (modificado intencionalmente por admin)
  (blanco = archivo normal)

Posiciones 3-8: otros atributos
  ? = test no disponible
  . = test OK

Posición 9: estado del archivo
  u = unmerged (pending)
  g = merged
```

```bash
# Verificar TODOS los paquetes (toma tiempo)
sudo dpkg -V
# Muestra solo archivos que difieren

# Verificar solo archivos de configuración
sudo dpkg -V | grep 'c$'

# Verificar un paquete específico
dpkg -V bash

# Ejemplo: verificar openssh
dpkg -V openssh-server 2>/dev/null || dpkg -V openssh 2>/dev/null
```

## Reparar problemas de dpkg

### Sistema a medio instalar

```bash
# dpkg se interrumpió (apagón, ctrl+c, kill)
# Estado típico: paquetes "Unpacked" o "Half-configured"

# Reparar: configura todos los paquetes pendientes
sudo dpkg --configure -a
# a = all, configura TODO lo que esté pendiente

# Verificar qué está mal
dpkg --audit
```

### Lock files bloqueados

```
E: Could not get lock /var/lib/dpkg/lock-frontend.
```

```bash
# 1. Verificar que no hay apt/dpkg corriendo
ps aux | grep -E 'apt|dpkg'
# root  1234  ...  aptd
# root  5678  ...  dpkg

# Si hay procesos activos: esperar a que terminen

# 2. Si no hay procesos activos (lock corrupto):
sudo lsof /var/lib/dpkg/lock-frontend
sudo lsof /var/lib/dpkg/lock
sudo lsof /var/lib/apt/lists/lock

# 3. Solo si estás SEGURO de que no hay apt/dpkg activo:
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo rm /var/lib/apt/lists/lock

# 4. Reparar estado
sudo dpkg --configure -a
```

### Dependencias faltantes

```bash
# dpkg: dependency problems prevent configuration of package
sudo apt install -f
# apt resolve e instala las dependencias faltantes,
# luego completa la configuración del paquete
```

### Estado inconsistent

```bash
# Encontrar paquetes en estados anómalos
dpkg -l | grep -E "^[i-uln][^i]"

# Estados problemáticos:
# U = Unpacked (no configurado)
# F = Failed config
# i? = Half-installed
# ?i = Unknown/triggers-awaited

# Reparar
sudo dpkg --configure -a
```

## dpkg-query — Consultas avanzadas

```bash
# Formato personalizado de salida
dpkg-query -W -f='${Package} ${Version} ${Architecture}\n' nginx
# nginx 1.22.1-9 amd64

# Listar con formato
dpkg-query -W -f='${Package}\t${Status}\n' | grep 'installed'

# Top 10 paquetes más pesados (Installed-Size)
dpkg-query -W -f='${Installed-Size}\t${Package}\n' | sort -rn | head -10
# 184320  linux-image-6.1.0-21-amd64
# 92340   linux-headers-6.1.0-21-amd64
# 61340   gcc-12-base

# Buscar por patrón
dpkg-query -l '*python3*'
dpkg-query -l 'nginx*'

# Mostrar solo paquetes con versión específica
dpkg-query -W -f='${Package} ${Version}\n' 'php*'
```

---

## Ejercicios

### Ejercicio 1 — Consultas básicas

```bash
# 1. ¿Cuántos paquetes tienes instalados?
dpkg -l | grep '^ii' | wc -l

# 2. ¿Cuántos están en estado raro (no ii)?
dpkg -l | grep -v '^ii' | grep -v '^++' | wc -l

# 3. ¿A qué paquete pertenece /bin/bash?
dpkg -S /bin/bash

# 4. ¿A qué paquete pertenece /etc/shadow?
dpkg -S /etc/shadow

# 5. ¿Qué archivos tiene el paquete adduser?
dpkg -L adduser | head -15
```

### Ejercicio 2 — Anatomía de un .deb

```bash
# 1. Descargar un .deb sin instalar
cd /tmp
apt download cowsay

# 2. Ver información
dpkg -I cowsay*.deb

# 3. Ver contenido
dpkg -c cowsay*.deb

# 4. Extraer todo
mkdir deb-extract control-extract
dpkg -x cowsay*.deb deb-extract/
dpkg -e cowsay*.deb control-extract/

# 5. Ver estructura
ls -la deb-extract/
ls -la control-extract/

# 6. Leer los scripts de control
cat control-extract/postinst
cat control-extract/prerm

# 7. Limpiar
rm -rf /tmp/cowsay*.deb /tmp/deb-extract /tmp/control-extract
```

### Ejercicio 3 — Verificar integridad

```bash
# 1. Elegir un paquete crítico del sistema
# Verificar que sus archivos no fueron modificados
dpkg -V bash

# 2. Verificar un paquete de configuración (probablemente muestre "c")
dpkg -V nginx 2>/dev/null || echo "nginx no instalado"

# 3. Buscar TODOS los archivos modificados en /etc/
sudo dpkg -V | grep '/etc/' | grep 'c$'

# 4. ¿Cuántos archivos tienes en total?
sudo dpkg -V | wc -l
```

### Ejercicio 4 — Resolver un paquete roto

```bash
# SIMULAR (en un entorno de prueba):
# 1. Descargar un .deb
cd /tmp
apt download nginx

# 2. Instalar directamente con dpkg (forzar)
sudo dpkg -i nginx*.deb
# Va a fallar por dependencias

# 3. Ver el estado
dpkg -l | grep nginx

# 4. Reparar con apt -f
sudo apt install -f

# 5. Verificar estado final
dpkg -l | grep nginx

# 6. Limpiar
rm /tmp/nginx*.deb
sudo apt purge nginx
sudo apt autoremove -y
```

### Ejercicio 5 — Consultas avanzadas con dpkg-query

```bash
# 1. Top 5 paquetes más pesados
dpkg-query -W -f='${Installed-Size} KB\t${Package}\n' | \
    sort -rn | head -5

# 2. Todos los paquetes de nginx
dpkg-query -l 'nginx*'

# 3. Versión instalada de Python 3
dpkg-query -W -f='${Package}: ${Version}\n' python3*

# 4. Listar solo paquetes manually installed (vs auto)
# (esto requiere apt-mark)
apt-mark showmanual | head -10
apt-mark showauto | head -10
```

### Ejercicio 6 — Scripts de pre/post instalación

```bash
# 1. Elegir un paquete pequeno
apt download dpkg

# 2. Extraer y examinar los scripts
dpkg -e dpkg_*.deb /tmp/dpkg-control/
cat /tmp/dpkg-control/postinst
cat /tmp/dpkg-control/preinst

# 3. ¿Qué hace el postinst de dpkg?
# (busca líneas relevantes: register, enable, etc.)

# 4. Limpiar
rm -rf /tmp/dpkg-control
rm /tmp/dpkg_*.deb
```
