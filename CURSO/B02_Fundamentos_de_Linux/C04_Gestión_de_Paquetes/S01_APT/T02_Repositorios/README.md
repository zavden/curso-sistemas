# T02 — Repositorios

## Qué es un repositorio APT

Un repositorio es un servidor (HTTP/HTTPS) que contiene paquetes `.deb` y
metadatos (índices de paquetes, firmas GPG). Cuando ejecutas `apt install`,
apt busca el paquete en los repositorios configurados, descarga el `.deb` y
lo instala.

```
apt update       → Descarga los índices de los repositorios
apt install foo  → Busca "foo" en los índices, descarga el .deb, instala

Sin apt update previo, apt usa índices viejos y puede no encontrar
paquetes nuevos o descargar versiones desactualizadas.
```

## sources.list — Formato clásico (one-line)

```bash
# Archivo principal
cat /etc/apt/sources.list

# Formato de una línea:
# type  URI                              suite      components
deb     http://deb.debian.org/debian     bookworm   main contrib non-free-firmware
deb-src http://deb.debian.org/debian     bookworm   main contrib non-free-firmware
deb     http://security.debian.org/      bookworm-security main contrib non-free-firmware
deb     http://deb.debian.org/debian     bookworm-updates  main contrib non-free-firmware
```

### Campos del formato one-line

| Campo | Significado | Ejemplo |
|---|---|---|
| type | `deb` (binarios) o `deb-src` (código fuente) | `deb` |
| URI | URL del repositorio | `http://deb.debian.org/debian` |
| suite | Nombre de la release o distribución | `bookworm`, `bookworm-security` |
| components | Secciones del repositorio | `main`, `contrib`, `non-free` |

### Componentes de Debian

| Componente | Licencia | Soporte |
|---|---|---|
| `main` | 100% libre (DFSG) | Soportado por Debian |
| `contrib` | Libre pero depende de non-free | Soportado por Debian |
| `non-free` | No libre (drivers propietarios, etc.) | No soportado oficialmente |
| `non-free-firmware` | Firmware propietario (Debian 12+) | Separado de non-free |

### Componentes de Ubuntu

| Componente | Mantenido por | Licencia |
|---|---|---|
| `main` | Canonical | Libre |
| `universe` | Comunidad | Libre |
| `restricted` | Canonical | No libre (drivers) |
| `multiverse` | Comunidad | No libre |

## Formato DEB822 (moderno)

Desde Debian 12 y Ubuntu 24.04, el formato preferido es **DEB822** en archivos
`.sources` dentro de `/etc/apt/sources.list.d/`:

```bash
cat /etc/apt/sources.list.d/debian.sources
# Types: deb deb-src
# URIs: http://deb.debian.org/debian
# Suites: bookworm bookworm-updates
# Components: main contrib non-free-firmware
# Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
#
# Types: deb deb-src
# URIs: http://security.debian.org/debian-security
# Suites: bookworm-security
# Components: main contrib non-free-firmware
# Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

El formato DEB822 es más legible y soporta la directiva `Signed-By` para
vincular la clave GPG específica a cada repositorio.

## Archivos adicionales en sources.list.d/

```bash
# Repositorios de terceros se agregan como archivos individuales
ls /etc/apt/sources.list.d/
# debian.sources
# docker.list        ← repositorio de Docker
# nodesource.list    ← repositorio de Node.js
# ...

# Cada archivo es independiente — más fácil de agregar y eliminar
# que editar sources.list directamente
```

## GPG keys — Autenticación de repositorios

APT verifica la autenticidad de los paquetes usando firmas GPG. Cada
repositorio tiene una clave pública que firma sus índices:

```bash
# Método moderno (Debian 12+): clave en /usr/share/keyrings/
# y referenciada con Signed-By en el .sources o .list

# Descargar y guardar la clave de un repositorio de terceros
curl -fsSL https://download.docker.com/linux/debian/gpg | \
    sudo gpg --dearmor -o /usr/share/keyrings/docker.gpg

# Usar la clave en el .list
echo "deb [signed-by=/usr/share/keyrings/docker.gpg] \
    https://download.docker.com/linux/debian bookworm stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list
```

### Método legacy vs moderno

```bash
# LEGACY (deprecated): apt-key
# Las claves se guardaban en un keyring global
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys AABBCCDD
# Problema: TODAS las claves podían firmar CUALQUIER repositorio
# apt-key está deprecated desde Debian 11

# MODERNO: Signed-By
# Cada repositorio tiene su propia clave en /usr/share/keyrings/
# Solo esa clave puede autenticar ese repositorio específico
# Más seguro: un repositorio comprometido no puede inyectar
# paquetes en repositorios de otros
```

```bash
# Ver claves legacy instaladas
apt-key list      # deprecated pero funcional

# Ver claves modernas
ls /usr/share/keyrings/
ls /etc/apt/keyrings/        # ubicación alternativa

# Si apt muestra "NO_PUBKEY" durante update:
# El repositorio no tiene su clave GPG instalada
# Descargar la clave del proveedor e instalarla con gpg --dearmor
```

## PPAs (Ubuntu)

Los **PPA** (Personal Package Archives) son repositorios de terceros alojados
en Launchpad, exclusivos de Ubuntu:

```bash
# Agregar un PPA
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt update
sudo apt install python3.12

# Lo que hace add-apt-repository:
# 1. Descarga la clave GPG del PPA
# 2. Crea un archivo en /etc/apt/sources.list.d/
# 3. Ejecuta apt update

# Eliminar un PPA
sudo add-apt-repository --remove ppa:deadsnakes/ppa

# Listar PPAs activos
grep -r "ppa" /etc/apt/sources.list.d/
```

Los PPA no existen en Debian. En Debian, los repositorios de terceros se
configuran manualmente.

## Prioridades (pinning)

Cuando un paquete está disponible en múltiples repositorios con diferentes
versiones, **apt pinning** controla cuál se prefiere:

```bash
# Ver la prioridad actual de un paquete
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500
#         500 http://deb.debian.org/debian bookworm/main
#   1.24.0-1 100
#         100 http://deb.debian.org/debian trixie/main
```

### Cómo funcionan las prioridades

| Prioridad | Comportamiento |
|---|---|
| > 1000 | Se instala incluso si es un downgrade |
| 500 | Se instala si es más nueva que la instalada (default para repos habilitados) |
| 100 | Solo se instala si no hay otra versión (default para repos no-release) |
| < 0 | Nunca se instala |

### Configurar pinning

```bash
# Archivo de preferencias
cat /etc/apt/preferences.d/pin-nginx

# Ejemplo: preferir nginx de backports
# Package: nginx
# Pin: release a=bookworm-backports
# Pin-Priority: 600

# Ejemplo: bloquear un paquete de un repo
# Package: *
# Pin: origin "ppa.launchpad.net"
# Pin-Priority: 400

# Ejemplo: nunca instalar un paquete
# Package: snapd
# Pin: release *
# Pin-Priority: -1
```

### Patrones de pin

```bash
# Por release
Pin: release a=bookworm-backports      # por archive
Pin: release n=bookworm                 # por codename
Pin: release l=Debian                   # por label
Pin: release o=Debian                   # por origin

# Por origen (URL)
Pin: origin "deb.debian.org"
Pin: origin "download.docker.com"

# Por versión específica
Pin: version 1.22.*
```

## Backports

Los **backports** son versiones más nuevas de paquetes adaptadas para funcionar
en la release estable:

```bash
# Habilitar backports en Debian
echo "deb http://deb.debian.org/debian bookworm-backports main" | \
    sudo tee /etc/apt/sources.list.d/backports.list
sudo apt update

# Los backports tienen prioridad 100 por defecto
# No se instalan automáticamente — hay que pedirlo explícitamente
sudo apt install -t bookworm-backports nginx

# Ver qué paquetes están disponibles en backports
apt list -a 2>/dev/null | grep backports | head
```

## Agregar un repositorio de terceros (procedimiento completo)

```bash
# Ejemplo: agregar el repositorio oficial de Docker

# 1. Instalar dependencias
sudo apt install ca-certificates curl

# 2. Descargar la clave GPG
sudo curl -fsSL https://download.docker.com/linux/debian/gpg \
    -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

# 3. Agregar el repositorio
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] \
    https://download.docker.com/linux/debian \
    $(. /etc/os-release && echo $VERSION_CODENAME) stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list

# 4. Actualizar índices
sudo apt update

# 5. Instalar
sudo apt install docker-ce
```

## Diagnóstico de problemas

```bash
# "E: Unable to locate package xxx"
# → El índice no está actualizado o el paquete no existe en los repos configurados
sudo apt update
apt-cache search xxx

# "W: GPG error: ... NO_PUBKEY AABBCCDD"
# → Falta la clave GPG del repositorio
# Descargar e instalar la clave

# "E: Repository ... does not have a Release file"
# → URL incorrecta o repo fuera de línea
# Verificar la URL en el .list o .sources

# "N: Skipping acquire of configured file ..."
# → Componente incorrecto o formato del .list erróneo
```

---

## Ejercicios

### Ejercicio 1 — Explorar la configuración

```bash
# Ver tus repositorios configurados
cat /etc/apt/sources.list 2>/dev/null
ls /etc/apt/sources.list.d/

# ¿Qué componentes tienes habilitados?
# ¿Tienes repos de terceros?

# Ver las claves GPG instaladas
ls /usr/share/keyrings/ /etc/apt/keyrings/ 2>/dev/null
```

### Ejercicio 2 — apt-cache policy

```bash
# Ver la política de un paquete común
apt-cache policy bash

# ¿De qué repositorio viene?
# ¿Cuál es su prioridad?
# ¿Hay múltiples versiones disponibles?

apt-cache policy curl
```

### Ejercicio 3 — Buscar paquetes

```bash
# Buscar paquetes relacionados con JSON
apt-cache search json | head -10

# Ver detalles de jq
apt show jq

# ¿Cuáles son sus dependencias?
apt-cache depends jq
```
