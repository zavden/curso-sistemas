# T01 — apt vs apt-get

## La familia de herramientas APT

APT (Advanced Package Tool) es el sistema de gestión de paquetes de las distribuciones basadas en Debian. No es un solo comando — es una familia de herramientas que trabajan en capas:

```
┌─────────────────────────────────────────────────────────────┐
│                    CAPA DE INTERFAZ USUARIO                  │
│                          apt                                │
│         (colores, progreso, comandos de alto nivel)          │
├─────────────────────────────────────────────────────────────┤
│              CAPA DE RESOLUCIÓN DE DEPENDENCIAS              │
│    apt-get  │  apt-cache  │  apt-mark  │  apt-file         │
│  (install)  │  (search)   │  (mark)    │  (file search)    │
├─────────────────────────────────────────────────────────────┤
│                  CAPA DE BAJO NIVEL                         │
│                         dpkg                                │
│           (instala .deb, no resuelve dependencias)           │
└─────────────────────────────────────────────────────────────┘
```

| Herramienta | Rol | ¿Interactiva? |
|---|---|---|
| `dpkg` | Instala .deb individuales, no resuelve deps | No |
| `apt-get` | Descarga, instala, resuelve dependencias | No (scripts) |
| `apt-cache` | Busca y consulta info de paquetes | No |
| `apt-mark` | Marca paquetes (hold, auto, manual) | No |
| `apt-file` | Busca qué paquete contiene un archivo | No |
| `apt` | Interfaz unificada moderna | Sí |

## apt vs apt-get: diferencias reales

`apt` fue introducido en Debian 8 (2014) como una interfaz más amigable. Internamente usa las mismas bibliotecas que `apt-get` — no son herramientas distintas, sino la misma lógica con interfaces diferentes.

| Aspecto | `apt` | `apt-get` |
|---|---|---|
| Propósito | Uso interactivo en terminal | Scripts y automatización |
| Barra de progreso | Sí | No |
| Colores y formato | Sí, legible para humanos | No, salida estable para máquinas |
| `list --upgradable` | Sí | No |
| `list --installed` | Sí | No |
| API/salida estable | No garantizada | Sí |
| Disponible desde | Debian 8+, Ubuntu 14.04+ | Siempre (desde Debian 2.1) |

### Equivalencias de comandos

| `apt` | `apt-get` / `apt-cache` | Descripción |
|---|---|---|
| `apt update` | `apt-get update` | Refresca la lista de paquetes |
| `apt upgrade` | `apt-get upgrade` | Actualiza todos los paquetes |
| `apt full-upgrade` | `apt-get dist-upgrade` | Actualiza, eliminando/resolviendo conflictos |
| `apt install pkg` | `apt-get install pkg` | Instala un paquete |
| `apt remove pkg` | `apt-get remove pkg` | Desinstala (mantiene configs) |
| `apt purge pkg` | `apt-get purge pkg` | Desinstala (elimina configs) |
| `apt autoremove` | `apt-get autoremove` | Elimina huérfanos |
| `apt search term` | `apt-cache search term` | Busca por nombre/descripción |
| `apt show pkg` | `apt-cache show pkg` | Muestra información del paquete |
| `apt list` | `dpkg -l` | Lista paquetes |
| `apt list --installed` | `dpkg --get-selections` | Lista instalados |
| `apt edit-sources` | `nano /etc/apt/sources.list` | Edita repositorios |

### Comandos exclusivos de `apt`

```bash
# Listar paquetes con actualizaciones disponibles
apt list --upgradable

# Listar TODOS los paquetes instalados (incluye automáticos)
apt list --installed

# Listar TODAS las versiones disponibles de un paquete
apt list --all-versions nginx

# Editar sources.list con el editor configurado en el sistema
sudo apt edit-sources

# Ver por qué está instalado un paquete (y sus dependencias inversas)
apt-cache rdepends nginx
```

### Comandos que solo existen en `apt-get`

```bash
# Descargar código fuente de un paquete
apt-get source nginx
# Crea: nginx-1.22.1/, nginx_1.22.1-9.dsc, nginx_1.22.1-9.tar.xz

# Instalar dependencias de compilación de un paquete fuente
sudo apt-get build-dep nginx

# Descargar .deb sin instalarlo (queda en el directorio actual)
apt-get download nginx

# Limpiar la caché
apt-get clean        # elimina TODOS los .deb descargados
apt-get autoclean    # elimina solo versiones obsoletas del repo

# Marcar como descargado sin instalar (para testing)
apt-get --download-only install nginx
```

## Cuándo usar cada uno

```
Uso interactivo en terminal
  → apt
  Barras de progreso, colores, output legible para humanos.

Scripts, Dockerfiles, automatización, CI/CD
  → apt-get
  Salida estable y parseable. Sin elementos interactivos.

ansible, puppet, chef, terraform (proveedores)
  → apt_get (módulo correspondiente)
  Siempre usar apt-get para garantizar idempotencia.
```

```dockerfile
# EN DOCKERFILES: siempre apt-get
FROM debian:bookworm

# Wrong: apt puede mostrar warnings de "no interactive terminal"
# RUN apt update && apt install nginx

# Correct: apt-get con flags de no-interactivo
RUN apt-get update && \
    apt-get install -y --no-install-recommends nginx && \
    rm -rf /var/lib/apt/lists/*

# Flags explicados:
# -y                    → responde "yes" a todas las preguntas
# --no-install-recommends → no instala paquetes "Recomendados"
#                           (imagen más pequeña)
# rm -rf /var/lib/apt/lists/* → limpia caché (reduce tamaño de capa)
```

```bash
# En scripts de provisionamiento (bash/sh):
apt-get update
apt-get install -y apache2 curl htop

# Nunca: apt install -y apache2   # puede fallar en algunos contextos
```

## apt-cache — Consultar información

`apt-cache` permite consultar la base de datos de paquetes sin instalar ni modificar nada:

```bash
# Buscar por nombre o descripción
apt-cache search "web server"
apt-cache search "^nginx"

# Mostrar información completa de un paquete
apt-cache show nginx
# Package: nginx
# Version: 1.22.1-9
# Architecture: amd64
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28), ...
# Description: small, powerful, scalable web/proxy server
# Homepage: https://nginx.org
# ...

# Ver SOLO las dependencias directas
apt-cache depends nginx
# nginx
#   Depends: nginx-common
#   Depends: libc6
#   Depends: libpcre2-8-0
#   ...

# Ver TODAS las dependencias (incluye transitivas)
apt-cache depends --installed nginx
apt-cache depends nginx | grep -E "Depends|Recommends|Suggests"

# Dependencias INVERSAS: qué paquetes dependen de ESTE
apt-cache rdepends libssl3
# libssl3
#   Reverse Depends:
#     curl
#     openssh-client
#     python3
#     nodejs
#     ...

# Política de versiones (de qué repo viene, prioridades)
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500        ← 500 = prioridad del repositorio
#         500 http://deb.debian.org/debian bookworm/main amd64 Packages
#         100 /var/lib/dpkg/status   ← 100 = instalado manualmente (hold)
```

### Ejemplo práctico: investigar antes de instalar

```bash
# 1. Buscar qué paquete es mejor para tu caso
apt-cache search "redis client"

# 2. Ver información detallada
apt-cache show redis-tools
apt-cache show hiredis-bin

# 3. Ver dependencias
apt-cache depends redis-tools

# 4. Ver tamaño de descarga y al installed
apt-cache show redis-tools | grep -E "Size|Installed-Size"

# 5. Ver de qué repositorio viene
apt-cache policy redis-tools
```

## apt-mark — Marcar paquetes

El sistema de marcas (flags) controla si un paquete puede ser desinstalado automáticamente:

```bash
# HOLD: evitar que un paquete se actualice
sudo apt-mark hold linux-image-amd64
# Output: linux-image-amd64 set on hold.

sudo apt-mark hold nginx

# Ver todos los paquetes en hold
apt-mark showhold

# Quitar el hold (permitir actualización)
sudo apt-mark unhold linux-image-amd64

# MANUAL: marcar como instalado intencionalmente (no auto-remove)
sudo apt-mark manual linux-firmware

# AUTO: marcar como dependencia (puede ser eliminado por autoremove)
sudo apt-mark auto libssl3

# Ver estado de un paquete específico
apt-mark showhold | grep nginx
apt-mark showmanual | grep nginx
```

### Ejemplo: proteger un paquete en un sistema mixto

```bash
# En un sistema donde instalaste kernel nuevo pero quieres mantener el viejo:
sudo apt-mark hold linux-image-6.1.0-21-amd64
sudo apt-mark hold linux-headers-6.1.0-21-amd64

# Verificar
apt-mark showhold | grep 6.1.0
```

## La caché de paquetes

```
/var/cache/apt/archives/        ← .deb descargados
/var/lib/apt/lists/              ← índice de repositorios
/var/lib/dpkg/info/              ← información de paquetes instalados
/var/lib/dpkg/alternatives/      ← symlinks alternativos
```

```bash
# Ver contenido de la caché
ls /var/cache/apt/archives/
# nginx_1.22.1-9_amd64.deb
# curl_7.88.1-10+deb12u5_amd64.deb

# Tamaño total de la caché
du -sh /var/cache/apt/archives/

# Ver tamaño individual
ls -lh /var/cache/apt/archives/*.deb

# Limpiar caché de .deb descargados
sudo apt-get clean        # todo
sudo apt-get autoclean    # solo versiones obsoletas

# Limpiar índice de repositorios (libera espacio)
sudo apt-get clean
sudo rm -rf /var/lib/apt/lists/*
sudo apt-get update       # regenera los índices
```

### ¿Cuánto espacio ocupa la caché?

```bash
# Mostrar uso de disco de la caché
du -sh /var/cache/apt/archives/

# Mostrar uso de los índices
du -sh /var/lib/apt/lists/

# Ver espacio en disco general
df -h /
```

## Configuración de APT

```
/etc/apt/sources.list              ← repositorios principales
/etc/apt/sources.list.d/           ← archivos .list adicionales
/etc/apt/apt.conf.d/               ← configuraciones fragmentadas
/etc/apt/preferences.d/            ← control de prioridades de versiones
```

### Fuentes de repositorios

```bash
# Ver repositorios configurados
cat /etc/apt/sources.list
# deb http://deb.debian.org/debian bookworm main contrib non-free
# deb-src http://deb.debian.org/debian bookworm main contrib non-free
# deb http://security.debian.org/debian-security bookworm-security main

# Formato de cada línea:
# deb|deb-src  URL  distribución  componentes
# deb = binarios, deb-src = código fuente

# Agregar un repositorio adicional
echo "deb http://repo.example.com/debian bullseye main" | sudo tee /etc/apt/sources.list.d/example.list
sudo apt-get update

# Agregar repositorio + clave GPG
# 1. Importar la clave
curl -fsSL https://repo.example.com/key.gpg | sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/example.gpg
# 2. Agregar el repo
echo "deb https://repo.example.com/debian stable main" | sudo tee /etc/apt/sources.list.d/example.list
sudo apt-get update
```

### Configuraciones útiles en apt.conf.d

```bash
# No instalar paquetes recomendados por defecto
echo 'APT::Install-Recommends "false";' | sudo tee /etc/apt/apt.conf.d/99norecommends

# Configurar proxy solo para apt
echo 'Acquire::http::Proxy "http://proxy.company.com:3128";' | sudo tee /etc/apt/apt.conf.d/99proxy

# Limitar ancho de banda de descarga (0 = ilimitado)
echo 'Acquire::http::Dl-Limit "500";' | sudo tee /etc/apt/apt.conf.d/50download-limit

# Silenciar mensajes de apt-listchanges
echo 'APT::List-Cleanup "false";' | sudo tee /etc/apt/apt.conf.d/99no-list-cleanup
```

### priorities (preferences)

```bash
# /etc/apt/preferences.d/pin-nginx
# Forzar que nginx siempre venga de testing, no de unstable:
Package: nginx
Pin: release a=testing
Pin-Priority: 900

# Ver prioridades de un paquete
apt-cache policy nginx
```

## Solución de problemas

### Problema: "Package XXX is not available"

```
$ sudo apt-get install nmap
Reading package lists... Done
Building dependency tree... Done
E: Unable to locate package nmap
```

**Causas posibles:**
1. El índice de paquetes está desactualizado
2. El paquete no existe en los repositorios configurados
3. El nombre del paquete es incorrecto

**Solución:**
```bash
# 1. Actualizar índice
sudo apt-get update

# 2. Buscar el nombre correcto
apt-cache search nmap | grep -i scanner

# 3. Ver si el repo está habilitado
apt-cache policy nmap

# 4. Si el paquete está en non-free o contrib:
#    Editar /etc/apt/sources.list y agregar esos componentes
```

### Problema: "Unable to acquire dpkg frontend lock"

```
E: Could not get lock /var/lib/dpkg/lock-frontend.
```

**Solución:**
```bash
# 1. Ver qué proceso tiene el lock
sudo lsof /var/lib/dpkg/lock-frontend
sudo lsof /var/lib/apt/lists/lock

# 2. Si es apt/dpkg en ejecución, esperar o matar
sudo kill $(pgrep apt)
sudo kill $(pgrep dpkg)

# 3. Si quedó lock corrupto (NO hacer esto si apt está activo)
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo dpkg --configure -a
```

### Problema: "The following packages have unmet dependencies"

```
E: Unable to correct problems, you have held broken packages.
```

**Solución:**
```bash
# 1. Ver detalles del conflicto
apt-cache depends nginx

# 2. Ver si hay versiones alternativas
apt-cache policy nginx nginx-common

# 3. Intentar con -f (fix broken)
sudo apt-get install -f

# 4. Si es conflicto de dependencias, puede requerir:
sudo apt-get dist-upgrade   # intenta resolver removiendo conflicto
# O reinstalar:
sudo apt-get install --reinstall nginx
```

### Problema: Hash sum mismatch

```
E: Failed to fetch http://deb.debian.org/debian/...  Hash Sum mismatch
```

**Solución:**
```bash
# Limpiar índice y volver a descargar
sudo rm -rf /var/lib/apt/lists/*
sudo apt-get update
```

---

## Ejercicios

### Ejercicio 1 — Comparar apt y apt-get

```bash
# 1. Buscar un paquete con ambos métodos
apt search htop
apt-cache search htop

# 2. Ver información con ambos
apt show htop
apt-cache show htop

# 3. Observar diferencias en formato y colores
# ¿Cuál es más legible?

# 4. Listar solo los primeros 5 resultados de apt-cache search
apt-cache search "web server" | head -5
```

### Ejercicio 2 — Investigar un paquete antes de instalar

```bash
# Elegir un paquete (ej: postgresql)
# Investiga:
# 1. ¿Qué versión está disponible?
apt-cache policy postgresql

# 2. ¿Cuánto pesa la descarga?
apt-cache show postgresql | grep -E "Size|Installed-Size"

# 3. ¿De qué depende?
apt-cache depends postgresql | head -20

# 4. ¿Qué paquetes dependen de él? (reverse depends)
apt-cache rdepends postgresql | head -10

# 5. ¿De qué repositorio viene?
apt-cache policy postgresql
```

### Ejercicio 3 — Gestión de hold y marcas

```bash
# 1. Crear un paquete ficticio (o buscar uno irrelevante) para probar
#    Instala un paquete pequeño de prueba
sudo apt-get install -y cowsay

# 2. Márcalo como automático
sudo apt-mark auto cowsay

# 3. Ver su estado
apt-mark showauto | grep cowsay
apt-mark showmanual | grep cowsay

# 4. Ponlo en hold (pruébalo con un paquete menos crítico)
sudo apt-mark hold cowsay
apt-mark showhold | grep cowsay

# 5. Quita el hold
sudo apt-mark unhold cowsay

# 6. Limpia con autoremove
sudo apt-get autoremove -y
```

### Ejercicio 4 — Explorar la caché y configuración

```bash
# 1. ¿Cuánto espacio ocupa la caché de apt?
du -sh /var/cache/apt/archives/

# 2. ¿Cuántos archivos hay en la caché?
ls /var/cache/apt/archives/*.deb | wc -l

# 3. Ver los archivos de configuración de apt
ls /etc/apt/apt.conf.d/

# 4. Contar cuántos repositorios están configurados
grep -rh "^deb " /etc/apt/sources.list.d/ /etc/apt/sources.list 2>/dev/null | wc -l

# 5. Limpiar la caché y observar el cambio
sudo apt-get clean
du -sh /var/cache/apt/archives/
```

### Ejercicio 5 — Escribir un Dockerfile basado en apt-get

```bash
# Escribe un Dockerfile que:
# 1. Use debian:bookworm como base
# 2. Instale nginx + curl + htop usando apt-get
# 3. Limpie la caché
# 4. Exponga el puerto 80
# 5. Cmd que ejecute nginx

# Build y verifica que funciona
docker build -t mi-nginx . -f Dockerfile
docker run -d -p 8080:80 mi-nginx
curl localhost:8080
```
