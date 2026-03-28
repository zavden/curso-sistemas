# T04 — dpkg

## Qué es dpkg

`dpkg` (Debian Package) es la herramienta de **bajo nivel** para gestionar
paquetes `.deb`. APT usa dpkg internamente para la instalación real:

```
apt install nginx
  │
  ├── Resuelve dependencias (apt)
  ├── Descarga .deb de repositorios (apt)
  └── Llama a dpkg para instalar cada .deb (dpkg)
```

La diferencia clave: **dpkg no resuelve dependencias**. Si un paquete requiere
`libfoo` y `libfoo` no está instalado, dpkg falla. APT existe para resolver
esto automáticamente.

¿Cuándo se usa dpkg directamente?
- Instalar un `.deb` descargado manualmente
- Consultar información de paquetes instalados
- Verificar integridad de archivos de un paquete
- Reparar instalaciones rotas
- Extraer contenido de un `.deb` sin instalar

## Instalar un .deb

```bash
# Instalar un paquete .deb descargado manualmente
sudo dpkg -i paquete.deb

# Si falla por dependencias:
# dpkg: dependency problems prevent configuration of paquete:
#  paquete depends on libfoo; however:
#   Package libfoo is not installed.

# Solución: usar apt para resolver las dependencias faltantes
sudo apt install -f
# apt descarga e instala las dependencias, luego configura el paquete

# O mejor: usar apt directamente con el .deb
sudo apt install ./paquete.deb
# apt resuelve dependencias ANTES de instalar
# Nota el ./ — sin él, apt busca en los repositorios
```

## Consultar paquetes instalados

### Listar paquetes

```bash
# Todos los paquetes instalados
dpkg -l
# Desired=Unknown/Install/Remove/Purge/Hold
# | Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
# |/ Err?=(none)/Reinst-required (Status,Err: uppercase=bad)
# ||/ Name             Version        Architecture Description
# ii  adduser          3.134          all          add and remove users and groups
# ii  apt              2.6.1          amd64        commandline package manager
# ii  base-files       12.4+deb12u5   amd64        Debian base system miscellaneous files
# ...

# El prefijo tiene dos letras:
# Primera letra (desired): i=install, r=remove, p=purge, h=hold
# Segunda letra (status):  i=installed, c=config-files, n=not-installed, u=unpacked

# ii = correctamente instalado
# rc = eliminado pero con config files
# pn = purgado
# hi = en hold pero instalado
```

```bash
# Buscar un paquete específico
dpkg -l nginx
dpkg -l 'nginx*'      # wildcard

# Solo nombres, sin formato
dpkg --get-selections | grep nginx
# nginx    install

# Contar paquetes instalados
dpkg -l | grep '^ii' | wc -l
```

### Ver archivos de un paquete instalado

```bash
# Listar todos los archivos que instaló un paquete
dpkg -L nginx
# /.
# /usr
# /usr/sbin
# /usr/sbin/nginx
# /usr/share
# /usr/share/doc
# /usr/share/doc/nginx
# /usr/share/man
# /usr/share/man/man8
# /usr/share/man/man8/nginx.8.gz

# Solo binarios/ejecutables
dpkg -L nginx | grep bin/
```

### Encontrar a qué paquete pertenece un archivo

```bash
# ¿Quién instaló este archivo?
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx

dpkg -S /usr/bin/curl
# curl: /usr/bin/curl

# Buscar con patrón
dpkg -S '*nginx.conf*'
# nginx-common: /etc/nginx/nginx.conf

# Si el archivo no pertenece a ningún paquete:
dpkg -S /usr/local/bin/my-script
# dpkg-query: no path found matching pattern
# (fue instalado manualmente, no via dpkg)
```

### Información detallada

```bash
# Información de un paquete instalado
dpkg -s nginx
# Package: nginx
# Status: install ok installed
# Priority: optional
# Section: httpd
# Installed-Size: 614
# Maintainer: ...
# Architecture: amd64
# Version: 1.22.1-9
# Depends: nginx-common, libc6, ...
# Description: small, powerful web/proxy server

# Solo el estado
dpkg -s nginx | grep Status
# Status: install ok installed
```

## Consultar un archivo .deb (sin instalar)

```bash
# Información del paquete dentro del .deb
dpkg -I paquete.deb
# o
dpkg --info paquete.deb
# Package: nginx
# Version: 1.22.1-9
# Architecture: amd64
# Depends: ...

# Listar archivos que contiene el .deb (sin instalar)
dpkg -c paquete.deb
# o
dpkg --contents paquete.deb
# drwxr-xr-x root/root    0 2023-06-01 usr/
# drwxr-xr-x root/root    0 2023-06-01 usr/sbin/
# -rwxr-xr-x root/root 1234 2023-06-01 usr/sbin/nginx
```

## Extraer un .deb sin instalar

```bash
# Extraer el contenido a un directorio
dpkg -x paquete.deb /tmp/extracted/
# Extrae los archivos (data.tar) al directorio especificado

# Extraer todo (incluye control scripts)
dpkg -e paquete.deb /tmp/control/
# Extrae: preinst, postinst, prerm, postrm, conffiles, control

# O manualmente (un .deb es un archive ar):
ar x paquete.deb
# Genera:
# debian-binary      ← versión del formato (2.0)
# control.tar.xz     ← metadatos y scripts
# data.tar.xz        ← los archivos del paquete

tar -tf data.tar.xz | head    # ver contenido
tar -xf data.tar.xz           # extraer
```

Esto es útil para inspeccionar scripts de pre/post instalación antes de
instalar un paquete de fuentes no confiables.

## Verificar integridad

```bash
# Verificar que los archivos de un paquete no fueron modificados
dpkg -V nginx
# (sin output = todo OK)
# Si hay diferencias:
# ??5?????? c /etc/nginx/nginx.conf
#   ^         ^
#   |         archivo de configuración
#   md5 cambió (fue editado)

# Códigos de verificación:
# ?  = test no realizado
# .  = test pasó
# 5  = MD5 diferente (archivo modificado)
# S  = tamaño diferente
# T  = mtime diferente

# Verificar TODOS los paquetes (lento)
dpkg -V
# Solo muestra los archivos que difieren

# Verificar un solo paquete
dpkg -V coreutils
```

## Desinstalar con dpkg

```bash
# Eliminar (mantiene configuración)
sudo dpkg -r paquete

# Purgar (elimina todo)
sudo dpkg -P paquete
# o
sudo dpkg --purge paquete

# dpkg NO maneja dependencias inversas:
# Si otro paquete depende de este, dpkg advierte pero no lo impide
# apt remove/purge es más seguro para esto
```

## Reparar problemas de dpkg

```bash
# Si dpkg se interrumpió a medias (apagón, kill)
sudo dpkg --configure -a
# Configura todos los paquetes que quedaron sin configurar

# Si hay paquetes en estado broken
sudo apt install -f
# apt intenta resolver las dependencias rotas

# Si el lock está activo (otro proceso usando dpkg)
# E: Could not get lock /var/lib/dpkg/lock-frontend
# 1. Verificar que no hay otro apt/dpkg corriendo
ps aux | grep -E 'apt|dpkg'
# 2. Si no hay otro proceso, eliminar el lock (último recurso)
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo dpkg --configure -a

# Base de datos de dpkg
ls /var/lib/dpkg/
# available   ← paquetes disponibles
# status      ← estado de todos los paquetes
# info/       ← metadatos por paquete (list, conffiles, md5sums)
```

## dpkg-query — Consultas avanzadas

```bash
# Formato personalizado
dpkg-query -W -f '${Package} ${Version} ${Installed-Size}\n' nginx
# nginx 1.22.1-9 614

# Los 10 paquetes más grandes
dpkg-query -W -f '${Installed-Size}\t${Package}\n' | sort -rn | head -10

# Listar con formato específico
dpkg-query -W -f '${Package}\t${Status}\n' | grep 'installed'

# Buscar paquetes por patrón
dpkg-query -l '*python3*'
```

---

## Ejercicios

### Ejercicio 1 — Consultar paquetes

```bash
# ¿Cuántos paquetes hay instalados?
dpkg -l | grep '^ii' | wc -l

# ¿A qué paquete pertenece /usr/bin/passwd?
dpkg -S /usr/bin/passwd

# ¿Qué archivos instaló el paquete coreutils?
dpkg -L coreutils | head -20
```

### Ejercicio 2 — Inspeccionar un .deb

```bash
# Descargar un .deb sin instalar
apt download cowsay

# Inspeccionar sin instalar
dpkg -I cowsay*.deb
dpkg -c cowsay*.deb | head -10

# Extraer contenido
mkdir /tmp/cowsay-extracted
dpkg -x cowsay*.deb /tmp/cowsay-extracted
ls /tmp/cowsay-extracted/

# Limpiar
rm cowsay*.deb
rm -rf /tmp/cowsay-extracted
```

### Ejercicio 3 — Verificar integridad

```bash
# Verificar que los archivos de bash no fueron modificados
dpkg -V bash

# Verificar un paquete cuya config editaste
dpkg -V openssh-server 2>/dev/null || dpkg -V ssh

# ¿Qué archivos cambiaron?
# Los archivos de configuración (marcados con 'c') son normales
```
