# T03 — Operaciones

## update — Actualizar índices

```bash
sudo apt update
# Descarga los índices de paquetes de TODOS los repositorios configurados
# NO instala nada — solo sincroniza la lista de paquetes disponibles
```

```
Flujo de apt update:

  Tu máquina                           Repositorios
     │                                     │
     │  GET /dists/bookworm/Release        │
     │ ──────────────────────────────────►│
     │                                     │
     │  200 OK + Release.gpg (firma)       │
     │ ◄──────────────────────────────────│
     │                                     │
     │  GET /dists/bookworm/main/          │
     │    binary-amd64/Packages.gz         │
     │ ──────────────────────────────────►│
     │                                     │
     │  200 OK + Packages.gz (firmado)     │
     │ ◄──────────────────────────────────│
     │                                     │
     │  Guardar en /var/lib/apt/lists/     │
```

### Interpretar la salida de `apt update`

```
Hit:10 http://deb.debian.org/debian bookworm InRelease
# Hit: el índice no cambió desde la última descarga (304 Not Modified)

Get:11 http://security.debian.org bookworm-security InRelease 12.3 kB
# Get: se descargó actualización del índice

Ign:9 https://download.docker.com bookworm InRelease
# Ign: se ignoró (error de red temporal, conexión rechazada, etc.)

Err:10 https://repo.example.com stable InRelease
# Err: error grave — clave GPG faltante, repo fuera de línea, URL incorrecta
```

```
Al final:
✓ "All packages are up to date"      → índices ok, no hay updates
⚠ "15 packages can be upgraded"      → hay updates disponibles
✗ "Some index files failed to download" → revisar los Err
```

## install — Instalar paquetes

```bash
# Instalar un paquete
sudo apt install nginx

# Instalar varios a la vez
sudo apt install nginx curl htop

# Versión específica (= es exacta, == es comparativa)
sudo apt install nginx=1.22.1-9

# Sin instalar paquetes Recommends (reducir tamaño en contenedores/servidores)
sudo apt install --no-install-recommends nginx

# Responder "yes" automáticamente
sudo apt install -y nginx

# Simular sin instalar (dry run — no necesita sudo)
apt install -s nginx
sudo apt install --simulate nginx
```

### Qué ocurre durante `apt install`

```
1. RESOLUCIÓN DE DEPENDENCIAS
   apt busca el paquete en /var/lib/apt/lists/
   Construye el grafo de dependencias (qué necesita, qué necesita eso, etc.)

2. PLAN DE ACCIÓN
   Muestra:
   - Los paquetes NUEVOS que se instalarán (+)
   - Los paquetes que se ACTUALIZARÁN (-->)
   - Los paquetes que se ELIMINARÁN (-) si hay conflictos
   - Espacio en disco: will use X MB, download X mb

3. CONFIRMACIÓN (si no es -y)
   "After this operation, X MB of additional disk space will be used.
   Do you want to continue? [Y/n]"

4. DESCARGA
   .deb → /var/cache/apt/archives/ (verificar espacio disponible!)
   Barra de progreso si es apt (no apt-get)

5. INSTALACIÓN con dpkg
   dpkg -i package1.deb package2.deb ...
   Descomprime archivos, coloca en /usr/, /etc/, etc.

6. CONFIGURACIÓN POST-INSTAL
   dpkg trigers pipe /etc/dpki/post-inst/
   Scripts de configuración del paquete (restart services, enable, etc.)
```

### Dependencias: tipos

```bash
# Depends: REQUERIDO para funcionar (duro)
# Recommends: instalado por defecto, mejora funcionalidad
# Suggests: opcional, funcionalidades extra
# Conflicts: incompatible con este paquete
# Pre-Depends: debe estar instalado Y configurado ANTES

apt show nginx
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28)
# Recommends: lib、PCRE2 (para rewrite)
# Suggests: apache2-utils (para htpasswd, ab)
```

### Reinstalar un paquete

```bash
# Reinstalar (misma versión) — útil si:
# - Archivos del paquete fueron modificados/corruptos
# - Un update dejó el sistema en estado inconsistente
sudo apt install --reinstall nginx

# Verificar que archivos fueron reinstalled
dpkg -s nginx | grep -E "Status|Version"
```

## remove — Desinstalar (conserva configuración)

```bash
# Eliminar binarios, preservar /etc/
sudo apt remove nginx

# Verificar estado
dpkg -l nginx
# rc  nginx  1.22.1-9  amd64  [removed, config-files remain]

# rc = removed, configuration files still on disk
```

```bash
# ¿Qué archivos de configuración sobreviven?
dpkg -L nginx | grep "/etc/"
# /etc/nginx/nginx.conf
# /etc/nginx/sites-available/
# /etc/nginx/sites-enabled/
```

## purge — Desinstalar (elimina TODO)

```bash
# Elimina binarios + configuración en /etc/
sudo apt purge nginx

# Equivalente:
sudo apt remove --purge nginx

# Verificar
dpkg -l nginx
# pn  nginx  ...  [purge, not installed]

ls /etc/nginx/ 2>/dev/null
# ls: cannot access '/etc/nginx/': No such file or directory
```

### remove vs purge vs autoremove

| Operación | Binarios | Config /etc | Deps huérfanas | Logs/Datos |
|---|---|---|---|---|
| `remove` | ❌ Eliminados | ✅ Conservados | ❌ No toca | ❌ No toca |
| `purge` | ❌ Eliminados | ❌ Eliminados | ❌ No toca | ❌ No toca |
| `autoremove` | — | — | ❌ Eliminadas | ❌ No toca |

```bash
# ATENCIÓN: ninguno toca tus datos de usuario
# /home/, /var/lib/mysql/, /var/www/html/ — intactos siempre
```

## autoremove — Limpiar dependencias huérfanas

```bash
# Cuando instalas un paquete, sus deps se marcan "automáticas"
# Si eliminas el paquete padre, las deps quedan huérfanas

# Ver qué se eliminaría (dry run)
apt autoremove --simulate

# Eliminar huérfanos
sudo apt autoremove

# Eliminar huérfanos + purgar sus configs
sudo apt autoremove --purge
```

```
Ejemplo: instalar apache2
  apache2 se instala
  → apache2-bin se marca como "auto"
  → libapr1 se marca como "auto"

Si luego haces apt remove apache2:
  apache2 se elimina
  apache2-bin queda huérfano (nadie lo requiere)
  libapr1 queda huérfano
  
  apt autoremove → elimina apache2-bin y libapr1
```

```bash
# Ver paquetes marcados como auto
apt-mark showauto | head -10

# Marcar uno como manual (proteger de autoremove)
sudo apt-mark manual linux-headers-$(uname -r)
```

## upgrade — Actualizar paquetes instalados

```bash
sudo apt update
sudo apt upgrade
```

```
upgrade es CONSERVADOR — solo actualiza, nuncaambahora ni elimina:

✓ Actualiza paquetes a versiones más nuevas del mismo repo
✓ Nunca instala paquetes NUEVOS
✓ Nunca elimina paquetes existentes
✓ Si alguna actualización requiere instalar o eliminar algo → LA OMITE
```

```bash
# Ejemplo: nginx necesita libssl3 pero tienes libssl1.1
# apt upgrade → nginx se omite, queda en versión vieja
# apt full-upgrade → instala libssl3, elimina libssl1.1, actualiza nginx
```

## full-upgrade — Actualización agresiva

```bash
sudo apt full-upgrade
# Equivalente legacy: sudo apt-get dist-upgrade
```

```
full-upgrade puede:
✓ Todo lo que hace upgrade, más:
✓ INSTALAR paquetes nuevos (nuevas dependencias)
✓ ELIMINAR paquetes en conflicto
✓ Se usa cuando upgrade no puede completar por obstáculos
```

### Cuándo usar cada uno

| Comando | Cuándo usarlo |
|---|---|
| `apt upgrade` | Actualizaciones rutinarias de seguridad (servidores) |
| `apt full-upgrade` | Migración a nueva release, o cuando upgrade dejó paquetes sin actualizar |
| `apt-mark hold` + `apt upgrade` | Mantener un paquete específico en su versión actual |

```bash
# Ejemplo real: distribuction upgrade (ej: buster → bullseye)
sudo apt update
sudo apt upgrade        # actualiza todo lo que se pueda sin conflictos
sudo apt full-upgrade   # resuelve los que quedaron pendientes
```

## Buscar y consultar paquetes

```bash
# Buscar por nombre o descripción
apt search json parser
apt search "^nginx$"     # expresión regular

# Buscar solo por nombre de paquete (más preciso)
apt search --names-only "^python3.*$"

# Información detallada
apt show nginx

# Solo el campo que te interesa
apt show nginx | grep -E "^Version:|^Depends:|^Description:"
```

```bash
# ¿Qué archivos instala un paquete? (YA INSTALADO)
dpkg -L nginx
# /usr/sbin/nginx
# /usr/share/nginx/html/index.html
# /etc/nginx/nginx.conf
# /etc/nginx/sites-available/
# /var/log/nginx/

# ¿A qué paquete pertenece un archivo? (YA INSTALADO)
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx

# ¿A qué paquete pertenece un archivo que NO está instalado?
apt-file search /usr/bin/curl
# curl: /usr/bin/curl
# (requiere apt-file install + update)
```

## Listar paquetes

```bash
# TODOS los paquetes conocidos (instalados + disponibles)
apt list

# Solo instalados
apt list --installed

# Solo actualizables (primero apt update)
apt list --upgradable

# Todas las versiones disponibles de un paquete
apt list --all-versions nginx
# nginx/stable 1.22.1-9 amd64 [installed]
# nginx/backports 1.24.0-1~bpo12+1 amd64 [upgradable from: 1.22.1-9]
```

```bash
# Filtrar con grep
apt list --installed | grep nginx
apt list --installed | wc -l    # cuántos paquetes instalados
apt list --upgradable            # qué se puede actualizar
```

## Descargar sin instalar

```bash
# Descargar .deb al directorio actual
apt download nginx
ls nginx_1.22.1-9_amd64.deb

# Descargar con todas sus dependencias (no las instala)
apt install --download-only nginx
ls /var/cache/apt/archives/*.deb

# Solo verificar qué se descargaría
apt-get install --download-only nginx
dpkg -c /var/cache/apt/archives/nginx_1.22.1-9_amd64.deb  # ver contenido del .deb
```

## Limpiar caché

```bash
# Eliminar TODOS los .deb descargados
sudo apt clean

# Eliminar solo los .deb de versiones ya instaladas (obsoletas)
sudo apt autoclean

# Tamaño de la caché
du -sh /var/cache/apt/archives/

# Ver qué hay en la caché
ls -lh /var/cache/apt/archives/*.deb | tail -10
```

```bash
# En Dockerfiles SIEMPRE limpiar
# Reduce tamaño de la capa final
RUN apt-get update && \
    apt-get install -y --no-install-recommends nginx && \
    rm -rf /var/lib/apt/lists/*
```

## Instalación no interactiva (scripts, Docker, CI)

```bash
# Método 1: Variables de entorno
export DEBIAN_FRONTEND=noninteractive
sudo apt install -y nginx

# Método 2: En una sola línea
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q nginx

# Método 3: en Dockerfile
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    nginx curl htop && \
    rm -rf /var/lib/apt/lists/*
```

```bash
# Flags explicados:
# -y    → responde "yes" a todas las confirmaciones
# -q    → quiet, menos output verbose
# --no-install-recommends → no instalar Recommends
# rm -rf /var/lib/apt/lists/* → limpia índice para reducir tamaño de capa Docker
```

## Manejo de paquetes rotos

```bash
# Situación: instalación interrumpida, dependencias a medias, dpkg inconsistente

# 1. Reparar paquetes rotos
sudo apt --fix-broken install
# o equivalentemente:
sudo dpkg --configure -a

# ¿Qué hace --fix-broken?
# - Detecta paquetes con "unpacked" status (instalación incompleta)
# - Intenta completar la configuración de cada uno
# - Si falta algo, intenta descargarlo e instalarlo

# 2. Ver estado de todos los paquetes
dpkg --audit

# 3. Si nada funciona —forzar
sudo dpkg --force-all --configure -a
# ⚠️ Peligroso: fuerza incluso cuando hay conflictos obvios

# 4. Ver logs de qué falló
dpkg -C   # audit, lista paquetes inconsistency
```

### Estados de un paquete en dpkg

```bash
dpkg -l | head -20
# ii = properly installed
# iu = unpacked but not configured
# iF = unpacked, failed to configure
# rc = removed, config-files remain
# pn = purge, not installed
# ?  = state unknown
```

```bash
# Encontrar paquetes en estado raro
dpkg -l | grep -E "^[i-uln][^i]"
dpkg -l | awk '$1 != "ii" {print $0}'
```

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida completo

```bash
# 1. Actualizar índices
sudo apt-get update

# 2. Instalar un paquete pequeño de prueba
sudo apt-get install -y cowsay

# 3. Ejecutarlo
cowsay "Funciono!"

# 4. Ver información del paquete instalado
dpkg -s cowsay | grep -E "Status|Version|Depends"

# 5. Ver los archivos que instaló
dpkg -L cowsay

# 6. Ver qué archivos de configuración dejó
dpkg -L cowsay | grep "^/etc/"

# 7. Desinstalar (remove — conserva config)
sudo apt-get remove cowsay
dpkg -l cowsay    # debería mostrar "rc"

# 8. Purgar (elimina config)
sudo apt-get purge cowsay
dpkg -l cowsay    # debería mostrar "pn"
ls /usr/games/cowsay 2>/dev/null  # debe dar error

# 9. Limpiar huérfanos
sudo apt-get autoremove -y
```

### Ejercicio 2 — Simular antes de ejecutar

```bash
# 1. Simular un upgrade completo del sistema
apt upgrade --simulate
# ¿Cuántos paquetes se actualizarán?
# ¿Cuánto espacio adicional se usará?

# 2. Simular instalación de un meta-paquete pesado
apt install --simulate build-essential
# ¿Cuántas dependencias nuevas?

# 3. Simular uninstall de un servicio
apt remove --simulate nginx
# ¿Qué dependencias quedarían huérfanas?
```

### Ejercicio 3 — Investigar un paquete a fondo

```bash
# Elegir un paquete (ej: postgresql-15 o mariadb-server)

# 1. Información general
apt show postgresql-15 | grep -E "^Package:|^Version:|^Depends:|^Recommends:"

# 2. ¿Cuánto pesa?
apt show postgresql-15 | grep "Installed-Size"

# 3. ¿Todos sus Recommends?
apt show postgresql-15 | grep "^Recommends:"
apt-cache depends postgresql-15 | grep -E "Recommends|Suggests"

# 4. ¿Qué archivos instalaría?
apt install --download-only postgresql-15
dpkg -c /var/cache/apt/archives/postgresql-15*.deb | head -20

# 5. ¿Hay múltiples versiones?
apt list --all-versions postgresql-15
```

### Ejercicio 4 — Dependencias invertidas

```bash
# 1. Elegir una librería del sistema (ej: libssl3)
# ¿Cuántos paquetes dependen de ella?
apt-cache rdepends libssl3 | tail -n +2 | wc -l

# 2. Mostrar los primeros 10
apt-cache rdepends libssl3 | tail -n +2 | head -10

# 3. Si desinstalaras libssl3, ¿cuántos paquetes se afectarían?
# (no hacerlo realmente)

# 4. Elegir un paquete pequeño y ver sus reverse depends
apt-cache rdepends bash
```

### Ejercicio 5 — Scripts de automatización

```bash
# Escribe un script llamado update-server.sh:

#!/bin/bash
# Script para actualizar un servidor de producción
set -e

echo "=== Actualizando índices ==="
apt-get update

echo "=== Paquetes actualizables ==="
apt list --upgradable

echo "=== Espacio en disco antes ==="
df -h /

echo "=== Ejecutando upgrade ==="
apt-get upgrade -y

echo "=== Espacio en disco después ==="
df -h /

echo "=== Paquetes huérfanos ==="
apt autoremove --simulate

# Ejecutar y observar la salida
```

### Ejercicio 6 — Resolver un sistema roto (simulado)

```bash
# En una VM o entorno de prueba:

# 1. Simular una interrupción de instalación
# (No hacer esto en producción)

# Provocar un estado inconsistente manualmente:
# dpkg --set-selections <<< "nginx hold"

# 2. Ver el estado del sistema
dpkg --audit
apt --fix-broken install --simulate

# 3. Ejecutar la reparación
sudo apt --fix-broken install

# 4. Verificar que todo está ok
dpkg -C
```
