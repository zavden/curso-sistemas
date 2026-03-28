# T03 — Operaciones

## update — Actualizar índices

```bash
sudo apt update
# Descarga los índices de paquetes de TODOS los repositorios configurados
# NO instala nada — solo actualiza la lista de paquetes disponibles

# Hit:  = el índice no cambió desde la última vez
# Get:  = se descargó un índice actualizado
# Ign:  = se ignoró (no disponible o no configurado)
# Err:  = error de descarga

# Al final muestra:
# "All packages are up to date" o
# "5 packages can be upgraded. Run 'apt list --upgradable' to see them."
```

`apt update` debe ejecutarse **antes** de `apt install` o `apt upgrade` para
asegurar que los índices están actualizados. En Dockerfiles es obligatorio
antes de cualquier instalación.

## install — Instalar paquetes

```bash
# Instalar un paquete
sudo apt install nginx

# Instalar varios
sudo apt install nginx curl htop

# Instalar una versión específica
sudo apt install nginx=1.22.1-9
# El = indica la versión exacta

# Instalar sin paquetes recomendados
sudo apt install --no-install-recommends nginx
# Los "Recommends" son paquetes que mejoran la funcionalidad pero no son necesarios
# En servidores y contenedores, omitirlos ahorra espacio

# Responder "yes" automáticamente
sudo apt install -y nginx

# Simular (dry run) — ver qué se instalaría sin hacerlo
sudo apt install -s nginx
# o
apt install --simulate nginx   # no necesita sudo
```

### Qué pasa durante un install

```
1. apt busca el paquete en los índices locales (resultado de apt update)
2. Resuelve dependencias: calcula qué otros paquetes necesita
3. Muestra un resumen:
   - Paquetes nuevos que se instalarán
   - Paquetes que se actualizarán
   - Paquetes que se eliminarán (si hay conflictos)
   - Espacio en disco necesario
4. Pide confirmación (a menos que se use -y)
5. Descarga los .deb a /var/cache/apt/archives/
6. Ejecuta dpkg para instalar cada .deb
7. Ejecuta los scripts de configuración del paquete
```

### Reinstalar

```bash
# Reinstalar un paquete (misma versión)
sudo apt install --reinstall nginx
# Útil si archivos del paquete fueron modificados o eliminados
```

## remove — Desinstalar paquetes

```bash
# Eliminar un paquete (mantiene archivos de configuración)
sudo apt remove nginx

# Verificar que la configuración quedó
ls /etc/nginx/
# nginx.conf  (sigue ahí)

dpkg -l nginx
# rc  nginx  ...
# r = removed, c = config-files present
```

## purge — Desinstalar paquetes con configuración

```bash
# Eliminar paquete Y sus archivos de configuración
sudo apt purge nginx

# Equivalente a:
sudo apt remove --purge nginx

# Verificar
ls /etc/nginx/ 2>/dev/null
# No such file or directory

dpkg -l nginx
# pn  nginx  ...
# p = purged, n = not installed
```

### remove vs purge

| Acción | Binarios | Config en /etc | Datos de usuario | Logs |
|---|---|---|---|---|
| `remove` | Eliminados | Conservados | No toca | No toca |
| `purge` | Eliminados | Eliminados | No toca | No toca |

Ninguno elimina datos de usuario (`/home`, `/var/lib/mysql`, etc.). Los datos
creados por el servicio en ejecución son responsabilidad del administrador.

## autoremove — Limpiar dependencias huérfanas

```bash
# Cuando se instala un paquete, sus dependencias se marcan como "auto"
# Si se elimina el paquete principal, las dependencias quedan huérfanas

sudo apt autoremove
# Elimina paquetes que fueron instalados como dependencia
# y ya ningún paquete instalado manualmente los necesita

# Con purge: también elimina la configuración de los huérfanos
sudo apt autoremove --purge

# Ver qué paquetes se eliminarían
apt autoremove --simulate
```

## upgrade — Actualizar paquetes instalados

```bash
# Actualizar TODOS los paquetes a la última versión disponible
sudo apt update       # primero actualizar índices
sudo apt upgrade

# upgrade es CONSERVADOR:
# - Actualiza paquetes a versiones nuevas
# - NO instala paquetes nuevos
# - NO elimina paquetes existentes
# - Si una actualización requiere instalar o eliminar otro paquete,
#   NO se realiza (el paquete se mantiene en la versión actual)
```

## full-upgrade (dist-upgrade) — Actualización completa

```bash
# Actualización más agresiva
sudo apt full-upgrade
# Equivalente legacy: sudo apt-get dist-upgrade

# full-upgrade PUEDE:
# - Instalar paquetes nuevos (si una dependencia cambió de nombre)
# - Eliminar paquetes (si hay conflictos irresolubles)
# - Hacer todo lo que upgrade hace, más estos cambios adicionales

# Cuándo usar cada uno:
# apt upgrade       → actualizaciones rutinarias de seguridad
# apt full-upgrade  → migrar a una nueva release o cuando upgrade
#                     deja paquetes sin actualizar
```

### upgrade vs full-upgrade

```
Situación: nginx 1.22 actualiza a 1.24, pero 1.24 requiere
           libssl3 (nuevo, reemplaza a libssl1.1)

apt upgrade:
  nginx se mantiene en 1.22 (no puede instalar libssl3 sin eliminar libssl1.1)

apt full-upgrade:
  Instala libssl3, elimina libssl1.1, actualiza nginx a 1.24
```

## Otras operaciones útiles

### Buscar paquetes

```bash
# Buscar por nombre o descripción
apt search json parser
# Sorting...
# jq - lightweight and flexible command-line JSON processor
# python3-json-pointer - resolve JSON pointers
# ...

# Buscar solo por nombre
apt search --names-only nginx
```

### Información de un paquete

```bash
# Detalles del paquete
apt show nginx
# Package: nginx
# Version: 1.22.1-9
# Priority: optional
# Section: httpd
# Maintainer: ...
# Installed-Size: 614 kB
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28)
# Recommends: ...
# Description: ...

# Ver archivos que instala un paquete (debe estar instalado)
dpkg -L nginx

# Ver a qué paquete pertenece un archivo
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx
```

### Listar paquetes

```bash
# Paquetes instalados
apt list --installed

# Paquetes actualizables
apt list --upgradable

# Todas las versiones de un paquete
apt list --all-versions nginx
```

### Descargar sin instalar

```bash
# Descargar el .deb al directorio actual
apt download nginx

# Descargar con dependencias
apt install --download-only nginx
# Los .deb quedan en /var/cache/apt/archives/
```

### Limpiar caché

```bash
# Eliminar todos los .deb descargados
sudo apt clean

# Eliminar solo los que ya no están en los repositorios
sudo apt autoclean

# Ver tamaño de la caché
du -sh /var/cache/apt/archives/
```

## Operaciones no interactivas (para scripts)

```bash
# Variables de entorno para evitar preguntas
export DEBIAN_FRONTEND=noninteractive

# Instalación completamente silenciosa
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q nginx

# -y: responder yes
# -q: quiet (menos output)
# DEBIAN_FRONTEND=noninteractive: no hacer preguntas de debconf

# En Dockerfiles:
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    nginx curl \
    && rm -rf /var/lib/apt/lists/*
```

## Manejo de paquetes rotos

```bash
# Si una instalación falló a medias:
sudo apt --fix-broken install
# o
sudo dpkg --configure -a

# Si hay conflictos de dependencias irresolubles:
sudo apt install -f
# Intenta resolver instalando o eliminando paquetes según necesidad
```

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida de un paquete

```bash
# Instalar
sudo apt update
sudo apt install -y cowsay

# Usar
/usr/games/cowsay "Hola"

# Ver archivos instalados
dpkg -L cowsay | head -10

# Eliminar (mantener config)
sudo apt remove cowsay

# ¿Sigue la configuración?
dpkg -l cowsay | tail -1

# Purgar
sudo apt purge cowsay

# Limpiar dependencias huérfanas
sudo apt autoremove -y
```

### Ejercicio 2 — Simular operaciones

```bash
# Simular un upgrade
apt upgrade --simulate

# ¿Cuántos paquetes se actualizarían?
# ¿Cuánto espacio se necesita?

# Simular instalar un paquete grande
apt install --simulate build-essential

# ¿Cuántas dependencias adicionales se instalarían?
```

### Ejercicio 3 — Buscar y consultar

```bash
# ¿A qué paquete pertenece /usr/bin/curl?
dpkg -S /usr/bin/curl

# ¿Cuáles son las dependencias de curl?
apt-cache depends curl

# ¿Qué paquetes dependen de curl?
apt-cache rdepends curl | head -10
```
