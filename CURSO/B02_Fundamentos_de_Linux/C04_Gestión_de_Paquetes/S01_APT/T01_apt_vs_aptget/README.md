# T01 — apt vs apt-get

## La familia de herramientas APT

APT (Advanced Package Tool) es el sistema de gestión de paquetes de las
distribuciones basadas en Debian. No es un solo comando — es una familia:

```
dpkg           Nivel bajo: instala/desinstala paquetes .deb individuales
                No resuelve dependencias

apt-get        Nivel medio: resuelve dependencias, descarga de repositorios
apt-cache      Nivel medio: busca y consulta información de paquetes
apt-mark       Nivel medio: marca paquetes (hold, manual, auto)

apt            Nivel alto: interfaz unificada que combina apt-get + apt-cache
                Con mejor UX (barras de progreso, colores, formato legible)
```

## apt vs apt-get: diferencias reales

`apt` fue introducido en Debian 8 (2014) como una interfaz más amigable.
Internamente usa las mismas bibliotecas que `apt-get`:

| Aspecto | apt | apt-get |
|---|---|---|
| Propósito | Uso interactivo (terminal) | Scripts y automatización |
| Barra de progreso | Sí | No |
| Colores | Sí | No |
| Listado de paquetes upgradeable | `apt list --upgradable` | No tiene equivalente directo |
| API estable | No garantizada | Sí (formato de salida estable) |
| Disponibilidad | Debian 8+, Ubuntu 14.04+ | Siempre |

### Equivalencias de comandos

| apt | apt-get / apt-cache |
|---|---|
| `apt update` | `apt-get update` |
| `apt upgrade` | `apt-get upgrade` |
| `apt full-upgrade` | `apt-get dist-upgrade` |
| `apt install pkg` | `apt-get install pkg` |
| `apt remove pkg` | `apt-get remove pkg` |
| `apt purge pkg` | `apt-get purge pkg` |
| `apt autoremove` | `apt-get autoremove` |
| `apt search term` | `apt-cache search term` |
| `apt show pkg` | `apt-cache show pkg` |
| `apt list` | `dpkg -l` (similar) |
| `apt list --installed` | `dpkg --get-selections` |
| `apt edit-sources` | `editor /etc/apt/sources.list` |

### Comandos exclusivos de apt

```bash
# Listar paquetes actualizables
apt list --upgradable

# Listar paquetes instalados
apt list --installed

# Listar todas las versiones disponibles de un paquete
apt list --all-versions nginx

# Editar sources.list con el editor del sistema
sudo apt edit-sources
```

### Comandos que solo existen en apt-get

```bash
# Descargar código fuente de un paquete
apt-get source nginx

# Compilar dependencias de un paquete fuente
sudo apt-get build-dep nginx

# Limpiar la caché de paquetes descargados
apt-get clean       # elimina todo
apt-get autoclean   # elimina solo versiones viejas

# Download sin instalar
apt-get download nginx
# Descarga el .deb al directorio actual
```

## Cuándo usar cada uno

```
En la terminal (uso interactivo):
  → apt
  Tiene barra de progreso, colores, output legible.

En scripts, Dockerfiles, automatización:
  → apt-get
  La salida es estable y parseabe.
  No muestra barras de progreso ni warnings interactivos.

En Dockerfiles:
  → apt-get (siempre)
  Docker ejecuta sin terminal interactivo.
  apt puede mostrar warnings sobre la falta de terminal.
```

```dockerfile
# En Dockerfiles: apt-get + flags de no-interactivo
RUN apt-get update && \
    apt-get install -y --no-install-recommends nginx && \
    rm -rf /var/lib/apt/lists/*

# -y: responder yes automáticamente
# --no-install-recommends: no instalar paquetes recomendados (imagen más pequeña)
# rm -rf /var/lib/apt/lists/*: limpiar caché para reducir tamaño de la capa
```

## apt-cache — Consultar información

`apt-cache` (o `apt show`/`apt search`) permite buscar y consultar paquetes
sin instalar nada:

```bash
# Buscar paquetes por nombre o descripción
apt-cache search web server
apt search web server          # equivalente con apt

# Información detallada de un paquete
apt-cache show nginx
apt show nginx                 # equivalente con apt
# Package: nginx
# Version: 1.22.1-9
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28), ...
# Description: small, powerful, scalable web/proxy server

# Ver solo las dependencias
apt-cache depends nginx
# nginx
#   Depends: nginx-common
#   Depends: libc6
#   Depends: libpcre2-8-0
#   ...

# Ver qué paquetes dependen de este (dependencias inversas)
apt-cache rdepends libssl3
# libssl3
#   Reverse Depends:
#     curl
#     openssh-client
#     python3
#     ...

# Información de la política del paquete (versiones, repositorios, prioridades)
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500
#         500 http://deb.debian.org/debian bookworm/main amd64 Packages
#         100 /var/lib/dpkg/status
```

## apt-mark — Marcar paquetes

```bash
# Poner un paquete en hold (evitar que se actualice)
sudo apt-mark hold linux-image-amd64
# linux-image-amd64 set on hold.

# Ver paquetes en hold
apt-mark showhold

# Quitar el hold
sudo apt-mark unhold linux-image-amd64

# Marcar un paquete como instalado manualmente
sudo apt-mark manual libfoo
# (evita que autoremove lo elimine)

# Marcar como automático (fue instalado como dependencia)
sudo apt-mark auto libfoo
# (autoremove lo eliminará si nada depende de él)

# Ver paquetes manuales vs automáticos
apt-mark showmanual
apt-mark showauto
```

## Configuración de apt

```bash
# Archivo principal de configuración
cat /etc/apt/apt.conf.d/

# Los archivos se procesan en orden numérico:
# 01autoremove  — configuración de autoremove
# 10periodic    — actualizaciones automáticas
# 50unattended-upgrades — actualizaciones desatendidas
# 70debconf     — preguntas de paquetes
# ...

# Configuraciones útiles
# Evitar instalar paquetes recomendados por defecto:
echo 'APT::Install-Recommends "false";' | sudo tee /etc/apt/apt.conf.d/99norecommends

# Configurar proxy:
echo 'Acquire::http::Proxy "http://proxy:3128";' | sudo tee /etc/apt/apt.conf.d/99proxy
```

## La caché de paquetes

```bash
# apt descarga los .deb a /var/cache/apt/archives/
ls /var/cache/apt/archives/
# nginx_1.22.1-9_amd64.deb
# curl_7.88.1-10+deb12u5_amd64.deb
# ...

# Tamaño de la caché
du -sh /var/cache/apt/archives/
# 450M

# Limpiar la caché
sudo apt-get clean        # elimina todos los .deb descargados
sudo apt-get autoclean    # elimina solo versiones que ya no están en el repo

# Verificar espacio liberado
du -sh /var/cache/apt/archives/
# 4.0K
```

---

## Ejercicios

### Ejercicio 1 — Comparar apt y apt-get

```bash
# Buscar un paquete con ambos
apt search htop
apt-cache search htop

# Ver información con ambos
apt show htop
apt-cache show htop

# ¿Cuál es más legible?
```

### Ejercicio 2 — apt-cache policy

```bash
# Ver la política de un paquete instalado
apt-cache policy bash

# ¿Qué versión está instalada?
# ¿De qué repositorio viene?
# ¿Cuál es la prioridad?

# Ver un paquete no instalado
apt-cache policy nginx
```

### Ejercicio 3 — Paquetes manuales vs automáticos

```bash
# ¿Cuántos paquetes manuales hay?
apt-mark showmanual | wc -l

# ¿Cuántos automáticos?
apt-mark showauto | wc -l

# ¿Hay paquetes en hold?
apt-mark showhold
```
