# T02 — Repositorios

## Qué es un repositorio APT

Un repositorio APT es un servidor HTTP/HTTPS que almacena paquetes `.deb` junto con sus metadatos (índice de paquetes, firmas GPG). Cuando ejecutas `apt install`, apt consulta los índices descargados, resuelve dependencias, descarga el `.deb` y lo instala mediante `dpkg`.

```
┌─────────────────────────────────────────────────────────────────┐
│                         REPOSITORIO                             │
│                                                                 │
│   http://deb.debian.org/debian/                                 │
│   ├── bookworm/                    ← suite (release)           │
│   │   ├── main/                    ← componente                │
│   │   │   ├── binary-amd64/                                   │
│   │   │   │   ├── Packages.gz       ← índice comprimido       │
│   │   │   │   ├── Packages.xz                                   │
│   │   │   │   └── Release                                   │
│   │   │   └── nginx_1.22.1-9_amd64.deb  ← paquete            │
│   │   ├── contrib/                                             │
│   │   └── non-free-firmware/                                    │
│   ├── bookworm-security/           ← suite de seguridad       │
│   └── bookworm-updates/             ← suite de actualizaciones │
└─────────────────────────────────────────────────────────────────┘

Flujo:
apt update   →  descarga /var/lib/apt/lists/*  (índices de TODOS los repos)
apt install  →  busca en índices locales → descarga .deb → dpkg -i
```

### Anatomía del índice de paquetes

```bash
# Los índices son archivos comprimidos con metadata de CADA paquete
# apt-cache show/read es leer estos índices (nunca contacta al repo en ese momento)

ls /var/lib/apt/lists/
# deb.debian.org_debian_dists_bookworm_main_binary-amd64_Packages.gz
# security.debian.org_debian-security_dists_bookworm-security_main_binary-amd64_Packages.gz

# Descomprimir y buscar un paquete específico
zcat /var/lib/apt/lists/*Packages.gz | grep -A 50 "^Package: nginx$" | head -60
```

### Sin `apt update`: el problema

```bash
# Sin actualizar, apt usa índices VIEJOS (de cuándo se actualizó la última vez)
# Paquetes nuevos NO existen en el índice local
# Versiones más nuevas NO se detectan

apt-cache policy nginx
# Si dice "500" y no hay versión más nueva, es porque el índice está desactualizado
# Y no porque no exista una versión más nueva en el repo

sudo apt update && apt-cache policy nginx
# AHORA sí muestra la versión real del repo
```

## sources.list — Formato clásico (one-line)

```bash
cat /etc/apt/sources.list
```

```
# Debian 12 Bookworm — formato one-line
# type   URI                                      suite                 components
deb     http://deb.debian.org/debian               bookworm              main contrib non-free-firmware
deb     http://security.debian.org/debian-security bookworm-security     main contrib non-free-firmware
deb     http://deb.debian.org/debian               bookworm-updates      main contrib non-free-firmware
deb     http://deb.debian.org/debian               bookworm-backports    main contrib non-free-firmware
```

### Campos

| Campo | Valores posibles | Descripción |
|---|---|---|
| `type` | `deb` (binarios) o `deb-src` (fuente) | `deb-src` necesario solo si compilas desde source |
| `URI` | URL HTTP/HTTPS/ftp/file | Puede ser local (`file:///`) |
| `suite` | Release name, `-security`, `-updates`, `-backports` | Identifica la versión de Debian |
| `components` | `main`, `contrib`, `non-free`, `non-free-firmware` | Separados por espacios |

### Suites en Debian

| Suite | Contenido | Actualizado |
|---|---|---|
| `bookworm` | Release estable inicial | No (frozen) |
| `bookworm-security` | Actualizaciones de seguridad | Sí, continuamente |
| `bookworm-updates` | Actualizaciones menores (bug fixes) | Sí, regularmente |
| `bookworm-backports` | Versiones más nuevas de software | Sincronizado de testing |

### Componentes de Debian

| Componente | Licencia | Dependencias | Soporte Debian |
|---|---|---|---|
| `main` | 100% libre (DFSG) | Solo en main | Sí |
| `contrib` | Libre (DFSG) | Puede depender de non-free | Parcial |
| `non-free` | No libre (proprietary) | Puede depender de non-free | No |
| `non-free-firmware` | Firmware no libre | Integrado en main | No (desde Debian 12) |

### Componentes de Ubuntu

| Componente | Mantenido por | Licencia | Contiene |
|---|---|---|---|
| `main` | Canonical | Libre | Paquetes con soporte oficial |
| `universe` | Comunidad | Libre | Paquetes de comunidad |
| `restricted` | Canonical | No libre | Drivers propietarios |
| `multiverse` | Comunidad | No libre | Software con restricciones |

## Formato DEB822 (moderno)

Desde Debian 12 y Ubuntu 24.04, el formato preferido usa archivos `.sources` en `/etc/apt/sources.list.d/`:

```bash
cat /etc/apt/sources.list.d/debian.sources
```

```sources
# Debian Main — formato DEB822
Types: deb
URIs: http://deb.debian.org/debian
Suites: bookworm bookworm-updates
Components: main contrib non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

# Debian Security
Types: deb
URIs: http://security.debian.org/debian-security
Suites: bookworm-security
Components: main contrib non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

### Comparación: one-line vs DEB822

```
One-line:
  deb http://deb.debian.org/debian bookworm main contrib non-free-firmware

DEB822 equivalent:
  Types: deb
  URIs: http://deb.debian.org/debian
  Suites: bookworm
  Components: main contrib non-free-firmware
  Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

### Ventajas de DEB822

```bash
# 1. Signed-By inline — la clave GPG vive junto al repositorio que protege
# 2. Múltiples suites en un solo bloque
# 3. Más legible — sin una línea gigante
# 4. Comments con #

# Agregar una suite adicional (ej: backports) editando el mismo archivo:
Suites: bookworm bookworm-updates bookworm-backports
```

### Convertir entre formatos

```bash
# Debian provee una herramienta para esto
# Lee sources.list y lo convierte/representa en formato interno

# No hay conversión automática oficial, pero puedes editar manualmente
# La lógica es: 1 línea deb = 1 bloque DEB822
```

## Repositorios en sources.list.d/

```bash
ls /etc/apt/sources.list.d/
# docker.list          → repositorio de Docker
# nodesource.list      → repositorio de Node.js
# google-chrome.list    → Chrome
# ...

# Cada archivo es independiente
# Agregar = crear archivo nuevo
# Eliminar = borrar archivo
# Más seguro que editar sources.list directamente
```

```bash
# Formato en archivos .list (uno por línea, igual que sources.list)
cat /etc/apt/sources.list.d/nodesource.list
# deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_20.x nodistry main

# Formato en archivos .sources (DEB822)
cat /etc/apt/sources.list.d/docker.list
# Types: deb
# URIs: https://download.docker.com/linux/debian
# Suites: bookworm
# Components: stable
# Signed-By: /etc/apt/keyrings/docker.asc
```

## Claves GPG — Autenticación de repositorios

APT verifica la autenticidad de los paquetes mediante **firmas digitales GPG**. Cada repositorio firma su índice con una clave privada; apt verifica con la clave pública correspondiente.

```
Repositorio (servidor)                     Tu máquina
      │                                         │
      │  Paquete .deb + Release + Release.gpg   │
      │  (índice firmado con clave privada)     │
      │ ───────────────────────────────────────►│
      │                                         │
      │            ¿Está firmado con             │
      │            la clave pública que          │
      │            tengo de este repo?           │
      │                                         │
      │         SÍ → Instalar                   │
      │         NO → Error de GPG               │
```

### Ubicaciones de claves GPG

| Método | Ubicación | Seguridad |
|---|---|---|
| Legacy (`apt-key`) | `/etc/apt/trusted.gpg` (global) | ⚠️ Deprecado — cualquier clave puede firmar cualquier repo |
| Moderno (`Signed-By`) | `/usr/share/keyrings/` o `/etc/apt/keyrings/` | ✅ Seguro — cada repo tiene su clave específica |

```bash
# Ver claves legacy (deprecated)
apt-key list
# /etc/apt/trusted.gpg
# --------------------
# pub   rsa4096 2017-05-22 [SCEA]
# uid           Docker Release (CE deb) <docker@docker.com>

# Ver claves modernas
ls /usr/share/keyrings/
# debian-archive-keyring.gpg       ← claves de Debian oficiales
# debian-archive-removed-keyring.gpg
# docker.gpg                       ← clave de Docker

ls /etc/apt/keyrings/  # ubicación alternativa para claves propias
```

### Instalar clave GPG para un repositorio (método moderno)

```bash
# Método correcto (Debian 12+):
# 1. Descargar la clave
curl -fsSL https://repo.example.com/key.gpg -o /tmp/key.gpg

# 2. Verificar que es una clave GPG válida
gpg --show-keys /tmp/key.gpg

# 3. Convertir a formato "dearmor" (binary) y guardar en keyrings
sudo gpg --dearmor -o /etc/apt/keyrings/example.gpg /tmp/key.gpg

# 4. Asignar permisos de lectura para todos
sudo chmod a+r /etc/apt/keyrings/example.gpg

# 5. Referenciar en el archivo .sources o .list
# En .sources (DEB822):
# Signed-By: /etc/apt/keyrings/example.gpg

# En .list (one-line):
# deb [signed-by=/etc/apt/keyrings/example.gpg] https://repo.example.com/debian stable main
```

### El error "NO_PUBKEY" — solución

```
W: GPG error: https://repo.example.com/debian stable InRelease: 
    No pubkey AABBCCDD11223344
E: The repository is not signed.
```

```bash
# 1. Identificar qué clave falta
apt-cache policy  # mostrará el error

# 2. Buscar la clave del proveedor (generalmente en su documentación)
# Por ejemplo, para Docker:
curl -fsSL https://download.docker.com/linux/debian/gpg | gpg --show-keys

# 3. Instalar la clave
curl -fsSL https://download.docker.com/linux/debian/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# 4. Verificar
ls -la /etc/apt/keyrings/docker.gpg

# 5. Reintentar
sudo apt-get update
```

## PPAs en Ubuntu

Los **Personal Package Archives** son repositorios propios alojados en Launchpad, exclusivos de Ubuntu:

```bash
# Agregar un PPA (esto hace TODO automáticamente)
sudo add-apt-repository ppa:deadsnakes/ppa
# 1. Descarga clave GPG del PPA
# 2. Crea /etc/apt/sources.list.d/deadsnakes-ubuntu-ppa-*.list
# 3. Ejecuta apt update

# Instalar versión de Python del PPA
sudo apt update
sudo apt install python3.12

# Eliminar un PPA y todo lo que instaló de él
sudo add-apt-repository --remove ppa:deadsnakes/ppa

# Listar todos los PPAs activos
grep -rh "^deb" /etc/apt/sources.list.d/ | grep ppa

# Ver archivos de un PPA
ls /etc/apt/sources.list.d/ | grep deadsnakes
```

**En Debian los PPAs no existen.** La alternativa es agregar repositorios manualmente.

## Prioridades de repositorios (apt pinning)

Cuando un paquete existe en varios repositorios con versiones distintas, apt usa **prioridades** para decidir cuál instalar.

### Sistema de prioridades

| Prioridad | Significado | Uso típico |
|---|---|---|
| `> 1000` | Force install (incluso downgrade) | Solo en emergencia |
| `1000` | Instalar esta versión sí o sí | Paquetes de apt pref |
| `990` | Prioridad por defecto de apt (no es estándar) | — |
| `500` | Prioridad por defecto de repos estándar | Repos normales |
| `100` | Prioridad por defecto de repos no-release (ej: backports) | Backports |
| `0` | Ignorar | Paquetes no instalados |
| `< 0` | Nunca instalar | Bloquear paquetes |

```bash
# Ver prioridades de un paquete
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500
#         500 http://deb.debian.org/debian bookworm/main amd64 Packages
#        100 100 http://deb.debian.org/debian bookworm-backports/main amd64 Packages
# En backports aparece con prioridad 100 (no se instala automáticamente)
```

### Archivo de preferencias

```bash
# Ubicación: /etc/apt/preferences.d/<nombre>
# Se procesan en orden alfabético — el último que coincide gana

cat /etc/apt/preferences.d/pin-docker
```

```
# Ejemplo 1: Preferir versión de Docker del repo oficial sobre Debian
Package: docker-ce
Pin: origin "download.docker.com"
Pin-Priority: 900

# Ejemplo 2: Instalar kernel de backports automáticamente (NO recomendado)
Package: linux-image-amd64
Pin: release a=bookworm-backports
Pin-Priority: 500

# Ejemplo 3: Bloquear un paquete (nunca instalarlo desde ningún repo)
Package: snapd
Pin: release *
Pin-Priority: -1

# Ejemplo 4: Siempre la última versión disponible (aunque sea testing)
Package: *
Pin: release a=testing
Pin-Priority: 900
```

### Patrones de Pin disponibles

```bash
# Por archive (suite)
Pin: release a=bookworm-backports

# Por codename (suite)
Pin: release n=bookworm

# Por label
Pin: release l=Debian

# Por origin (nombre del operador del repo)
Pin: release o=Debian

# Por origin (URL del repo, específico)
Pin: origin "deb.debian.org"

# Por versión (wildcards válidos)
Pin: version 1.22.*
Pin: version 1.22.1-9

# Por arquitectura
Pin: release c=amd64
```

### Ejemplo real: preferir nginx de un repo específico

```bash
# /etc/apt/preferences.d/nginx
Explanation: Give nginx from the official repo priority over Debian's
Package: nginx
Pin: origin "nginx.org"
Pin-Priority: 950

# Verificar
apt-cache policy nginx
```

## Backports

Los **backports** son paquetes recompilados de `testing`/`unstable` para que funcionen en `stable`. Ofrecen versiones más nuevas sin cambiar toda la distribución.

```bash
# Habilitar backports en Debian
echo "deb http://deb.debian.org/debian bookworm-backports main" | \
    sudo tee /etc/apt/sources.list.d/backports.list

sudo apt-get update

# Listar paquetes disponibles en backports
apt-cache search -t bookworm-backports .

# Instalar versión de backports (debe especificarse -t)
sudo apt-get install -t bookworm-backports nginx

# Listar todas las versiones disponibles (incluyendo backports)
apt list -a nginx
```

```bash
# Comportamiento sin y con -t
apt-cache policy nginx
# 1.22.1-9 500 ← versión stable (se instala por defecto)
# 1.24.0-1~bpo12+1 100 ← versión backports (NO se instala automáticamente)

# Con -t bookworm-backports
sudo apt-get install -t bookworm-backports nginx
# Ahora sí instala 1.24.0-1~bpo12+1
```

## Procedimiento completo: Agregar un repositorio de terceros

### Ejemplo: Docker en Debian

```bash
# Paso 1: Instalar dependencias
sudo apt install ca-certificates curl gnupg

# Paso 2: Crear directorio de claves si no existe
sudo install -m 0755 -d /etc/apt/keyrings

# Paso 3: Descargar la clave GPG oficial
curl -fsSL https://download.docker.com/linux/debian/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# Paso 4: Asignar permisos
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Paso 5: Crear archivo del repositorio
# Obtener el codename de la distribución actual
. /etc/os-release
echo $VERSION_CODENAME  # bookworm

# Crear el archivo
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/debian \
    $VERSION_CODENAME stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list

# Paso 6: Actualizar índices
sudo apt-get update

# Paso 7: Verificar que el repo está activo
apt-cache policy docker-ce

# Paso 8: Instalar
sudo apt-get install docker-ce docker-ce-cli containerd.io
```

### Ejemplo: Node.js (nodesource)

```bash
# 1. Instalar curl si no está
sudo apt install -y curl

# 2. Descargar y ejecutar el script de setup (automatiza todo)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo bash -

# Lo que hace internamente:
# - Crea /etc/apt/keyrings/nodesource.gpg
# - Crea /etc/apt/sources.list.d/nodesource.list
# - Ejecuta apt update

# 3. Instalar Node.js
sudo apt-get install -y nodejs

# 4. Verificar
node --version   # v20.x.x
npm --version
```

## Diagnóstico de problemas

### Error: "Unable to locate package"

```
E: Unable to locate package nginx
```

```bash
# Causa 1: Índices desactualizados
sudo apt-get update

# Causa 2: El paquete no existe en los repos configurados
apt-cache search nginx
apt-cache search "^nginx$"

# Causa 3: El repositorio no está habilitado (componente faltante)
# Ej: si el paquete está en non-free y no tienes non-free habilitado
grep -r "non-free" /etc/apt/sources.list.d/

# Causa 4: Typo en el nombre del paquete
apt-cache show nginx   # existe
apt-cache show nginz   # no existe
```

### Error: "GPG error: NO_PUBKEY"

```
W: GPG error: http://repo.example.com ... No pubkey AABBCCDD
```

```bash
# 1. Identificar la clave faltante (últimos 8 caracteres del error)
# AABBCCDD → buscar en la documentación del repositorio

# 2. Instalar la clave
curl -fsSL https://repo.example.com/key.gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/repo.gpg

# 3. Si el archivo .list usa Signed-By, verificar que apunte a la clave correcta
grep -r "Signed-By" /etc/apt/sources.list.d/repo.list

# 4. Reintentar
sudo apt-get update
```

### Error: "Repository does not have a Release file"

```
E: The repository ... does not have a Release file.
```

```bash
# Causa 1: URL incorrecta (la suite no existe en ese repo)
# Verificar:
curl -I http://deb.debian.org/debian/dists/bookworm/Release
# 200 OK = existe

curl -I http://deb.debian.org/debian/dists/trixie/Release
# 404 = la suite no existe

# Causa 2: El repositorio está fuera de línea o la URL cambió
# Revisar la documentación del proveedor

# Causa 3: Error de tipeo en el archivo .list
cat /etc/apt/sources.list.d/repo.list
# Verificar suite, URI, componentes
```

### Error: "Conflicting values"

```
E: Conflicting values set for option Signed-By regarding source ...
```

```bash
# Causa: Un mismo repo está definido en DOS archivos con valores distintos
grep -r "docker" /etc/apt/sources.list.d/

# Solución: editar o eliminar el archivo duplicado
sudo rm /etc/apt/sources.list.d/docker.list.backup
```

---

## Ejercicios

### Ejercicio 1 — Explorar repositorios configurados

```bash
# 1. Ver el archivo principal
cat /etc/apt/sources.list

# 2. Listar todos los archivos .list y .sources en sources.list.d/
ls /etc/apt/sources.list.d/
file /etc/apt/sources.list.d/*

# 3. ¿Cuántos repositorios tienes configurados en total?
grep -rh "^deb" /etc/apt/sources.list /etc/apt/sources.list.d/*.list 2>/dev/null | wc -l

# 4. ¿Qué componentes tienes habilitados?
grep -rh "^deb" /etc/apt/sources.list /etc/apt/sources.list.d/*.list 2>/dev/null | \
    awk '{print $4}' | tr ' ' '\n' | sort -u

# 5. Ver las claves GPG instaladas
ls -la /usr/share/keyrings/ /etc/apt/keyrings/ 2>/dev/null
```

### Ejercicio 2 — Investigar un repositorio de terceros

```bash
# Elige un software (ej: Signal, VS Code, Chrome, etc.)
# 1. Buscar cómo agregar su repositorio oficial

# Para Google Chrome (ejemplo):
wget -q -O - https://dl.google.com/linux/linux_signing_key.pub | \
    sudo gpg --dearmor -o /usr/share/keyrings/google-chrome.gpg

echo "deb [arch=amd64 signed-by=/usr/share/keyrings/google-chrome.gpg] \
    http://dl.google.com/linux/chrome/deb/ stable main" | \
    sudo tee /etc/apt/sources.list.d/google-chrome.list

# 2. Ejecutar apt update
sudo apt-get update

# 3. Verificar que está activo
apt-cache policy google-chrome-stable

# 4. Ver los archivos que se crearon
ls -la /etc/apt/sources.list.d/google-chrome*
```

### Ejercicio 3 — Simular pinning

```bash
# 1. Ver las prioridades por defecto de un paquete disponible en múltiples repos
apt-cache policy curl
# Si solo hay un repo, no hay conflicto

# 2. Buscar un paquete que exista en main y en backports
apt-cache policy nginx

# 3. Crear un archivo de preferencias para ese paquete
echo "Package: nginx
Pin: release a=bookworm-backports
Pin-Priority: 900" | sudo tee /etc/apt/preferences.d/test-pin

# 4. Ver cómo cambia la prioridad
apt-cache policy nginx

# 5. Eliminar el archivo de prueba
sudo rm /etc/apt/preferences.d/test-pin
```

### Ejercicio 4 — Resolver error de GPG

```bash
# Simular un error removiendo temporalmente una clave
# (NO hacer esto en producción)

# 1. Guardar el nombre de un repo de terceros
ls /etc/apt/sources.list.d/

# 2. Buscar si algún repo muestra error de clave
sudo apt-get update 2>&1 | grep -i "NO_PUBKEY\|GPG error"

# 3. Si hay error, resolverlo buscando la clave del proveedor
# (investigar cómo hacerlo para ese repositorio específico)

# 4. Verificar que se resolvió
sudo apt-get update
```

### Ejercicio 5 — Investigar la estructura interna

```bash
# 1. ¿Cuántos paquetes hay en el índice principal?
zcat /var/lib/apt/lists/*bookworm_main_binary-amd64_Packages.gz | \
    grep "^Package:" | wc -l

# 2. Buscar un paquete específico en el índice
zcat /var/lib/apt/lists/*bookworm_main_binary-amd64_Packages.gz | \
    grep -A 20 "^Package: bash$"

# 3. Ver el archivo Release de un repositorio
cat /var/lib/apt/lists/*bookworm Release | head -30

# 4. ¿Qué pasa si borras un índice y haces apt update?
sudo rm /var/lib/apt/lists/*security*
sudo apt-get update  # solo el security va a pedir re-descargar
```
