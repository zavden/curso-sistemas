# T01 — apt vs apt-get

## Errata y notas sobre el material base

1. **README.md: `cat /etc/apt/apt.conf.d/`** — Es un directorio, no un
   archivo. `cat` no funciona en directorios. Correcto: `ls /etc/apt/apt.conf.d/`.

2. **README.max.md: `apt-cache rdepends` en sección "Comandos exclusivos
   de apt"** — `rdepends` es un comando de `apt-cache`, no de `apt`.
   Está en la sección equivocada.

3. **README.md/README.max.md: "100 /var/lib/dpkg/status ← instalado
   manualmente (hold)"** — La prioridad 100 para `/var/lib/dpkg/status`
   es la prioridad estándar del paquete local instalado. No tiene nada
   que ver con hold. Hold es un flag separado gestionado por `apt-mark`.

4. **README.max.md: `APT::List-Cleanup "false"` para "silenciar
   apt-listchanges"** — Incorrecto. `APT::List-Cleanup` controla si apt
   limpia listas obsoletas tras un update. apt-listchanges se configura
   con su propio paquete (`/etc/apt/listchanges.conf`).

5. **README.max.md: `--download-only` "marcar como descargado"** —
   No "marca" nada. Solo descarga los .deb a la caché sin instalarlos.

6. **README.max.md troubleshooting: `sudo kill $(pgrep apt)`** —
   Consejo peligroso. Matar apt mientras trabaja puede corromper la
   base de datos de dpkg. Mejor esperar a que termine. Si ya está
   muerto, usar `sudo dpkg --configure -a` para reparar.

7. **README.max.md troubleshooting: `sudo rm /var/lib/dpkg/lock*`** —
   Extremadamente peligroso si apt/dpkg sigue activo. Solo hacerlo si
   `lsof` confirma que no hay procesos usando los locks. Añadida
   advertencia más fuerte.

8. **Ambos READMEs: falta `DEBIAN_FRONTEND=noninteractive`** — En
   Dockerfiles es esencial para evitar prompts interactivos de
   paquetes como `tzdata`. Sin esto, el build se cuelga.

---

## La familia de herramientas APT

APT (Advanced Package Tool) es el sistema de paquetes de las
distribuciones basadas en Debian. No es un solo comando — es una
familia en capas:

```
┌───────────────────────────────────────────────┐
│              INTERFAZ DE USUARIO               │
│                    apt                         │
│    (colores, progreso, comandos unificados)    │
├───────────────────────────────────────────────┤
│         RESOLUCIÓN DE DEPENDENCIAS             │
│  apt-get │ apt-cache │ apt-mark │ apt-file    │
│ (install)│ (search)  │  (hold)  │(file→pkg)   │
├───────────────────────────────────────────────┤
│               NIVEL BAJO                       │
│                   dpkg                         │
│     (instala .deb, NO resuelve dependencias)   │
└───────────────────────────────────────────────┘
```

| Herramienta | Rol | Paquete |
|---|---|---|
| `dpkg` | Instala/desinstala .deb individuales, no resuelve deps | dpkg |
| `apt-get` | Descarga, instala, actualiza, resuelve dependencias | apt |
| `apt-cache` | Busca y consulta info de paquetes (sin modificar nada) | apt |
| `apt-mark` | Marca paquetes (hold, manual, auto) | apt |
| `apt-file` | Busca qué paquete contiene un archivo dado | apt-file |
| `apt` | Interfaz unificada moderna con mejor UX | apt |

El flujo real cuando ejecutas `apt install nginx`:

```
1. apt resuelve dependencias (nginx-common, libc6, libpcre2...)
2. apt descarga los .deb de los repositorios al caché
3. apt llama a dpkg para instalar cada .deb en orden
```

---

## apt vs apt-get: diferencias reales

`apt` fue introducido en Debian 8 (2014) como interfaz más amigable.
Internamente usa las mismas bibliotecas que `apt-get` — no son
herramientas distintas, sino la misma lógica con interfaces diferentes.

| Aspecto | `apt` | `apt-get` |
|---|---|---|
| Propósito | Uso interactivo (terminal) | Scripts y automatización |
| Barra de progreso | Sí | No |
| Colores y formato | Sí | No |
| API estable | **No** garantizada | **Sí** (formato de salida estable) |
| `list --upgradable` | Sí | No (equivalente: `apt-get -s upgrade`) |
| `list --installed` | Sí | No (equivalente: `dpkg --get-selections`) |
| Disponible desde | Debian 8+ (2015) | Siempre (desde Debian 2.1) |

### Equivalencias de comandos

| `apt` | `apt-get` / `apt-cache` | Descripción |
|---|---|---|
| `apt update` | `apt-get update` | Refrescar lista de paquetes |
| `apt upgrade` | `apt-get upgrade` | Actualizar paquetes (sin remover) |
| `apt full-upgrade` | `apt-get dist-upgrade` | Actualizar resolviendo conflictos |
| `apt install pkg` | `apt-get install pkg` | Instalar un paquete |
| `apt remove pkg` | `apt-get remove pkg` | Desinstalar (mantiene configs) |
| `apt purge pkg` | `apt-get purge pkg` | Desinstalar (elimina configs) |
| `apt autoremove` | `apt-get autoremove` | Eliminar dependencias huérfanas |
| `apt search term` | `apt-cache search term` | Buscar por nombre/descripción |
| `apt show pkg` | `apt-cache show pkg` | Información del paquete |
| `apt list` | `dpkg -l` | Listar paquetes |
| `apt edit-sources` | `editor /etc/apt/sources.list` | Editar repositorios |

### Comandos exclusivos de `apt`

```bash
apt list --upgradable       # paquetes con actualización disponible
apt list --installed        # paquetes instalados
apt list --all-versions pkg # todas las versiones de un paquete
sudo apt edit-sources       # editar sources.list con $EDITOR
```

### Comandos exclusivos de `apt-get`

```bash
apt-get source pkg          # descargar código fuente
sudo apt-get build-dep pkg  # instalar dependencias de compilación
apt-get download pkg        # descargar .deb sin instalar
sudo apt-get clean          # eliminar TODOS los .deb del caché
sudo apt-get autoclean      # eliminar solo versiones obsoletas
apt-get -s install pkg      # simular (dry-run) sin modificar nada
```

---

## Cuándo usar cada uno

```
Terminal interactiva     → apt
  Barras de progreso, colores, output legible.

Scripts y automatización → apt-get
  Salida estable y parseable. Sin elementos interactivos.

Dockerfiles             → apt-get (siempre)
  Docker ejecuta sin terminal interactivo.
  apt puede mostrar warnings sobre la falta de terminal.

CI/CD, Ansible, Chef    → apt-get
  API estable, idempotente, parseable.
```

### Patrón para Dockerfiles

```dockerfile
FROM debian:bookworm

# DEBIAN_FRONTEND evita prompts interactivos (ej: tzdata)
# -y responde "yes" automáticamente
# --no-install-recommends reduce el tamaño de la imagen
# rm -rf .../lists/* limpia la caché del índice
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y --no-install-recommends \
        nginx \
        curl \
    && rm -rf /var/lib/apt/lists/*
```

> Sin `DEBIAN_FRONTEND=noninteractive`, paquetes como `tzdata`
> muestran un prompt interactivo que cuelga el build de Docker.

---

## apt-cache — Consultar información

`apt-cache` consulta la base de datos de paquetes sin instalar ni
modificar nada:

```bash
# Buscar paquetes por nombre o descripción
apt-cache search "web server"
apt-cache search "^nginx"       # regex: empieza con nginx

# Información completa de un paquete
apt-cache show nginx
# Package: nginx
# Version: 1.22.1-9
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28), ...
# Installed-Size: 150
# Download-Size: 60000

# Dependencias directas
apt-cache depends nginx
# nginx
#   Depends: nginx-common
#   Depends: libc6
#   Depends: libpcre2-8-0

# Dependencias inversas: ¿quién depende de ESTE paquete?
apt-cache rdepends libssl3
# libssl3
#   Reverse Depends:
#     curl
#     openssh-client
#     python3

# Política de versiones (repo, prioridades)
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500
#        500 http://deb.debian.org/debian bookworm/main amd64 Packages
#        100 /var/lib/dpkg/status
```

### Campos de `apt-cache policy`

```
Installed:  versión actualmente instalada (o "(none)")
Candidate:  versión que se instalaría con apt install
Version table:
  ***       marca la versión instalada
  500       prioridad estándar de un repositorio habilitado
  100       prioridad del estado local (/var/lib/dpkg/status)
  990       prioridad de un pin explícito
```

> La prioridad 100 de `/var/lib/dpkg/status` simplemente indica que
> el paquete está instalado localmente. **No tiene relación con hold**.

---

## apt-mark — Marcar paquetes

El sistema de marcas controla cómo apt gestiona cada paquete:

### Manual vs Auto

```bash
# Manual: el usuario lo pidió explícitamente
# Auto: se instaló como dependencia de otro paquete

# Ver cuántos de cada tipo
apt-mark showmanual | wc -l    # instalados a propósito
apt-mark showauto | wc -l      # instalados como dependencias

# Cambiar la marca
sudo apt-mark manual libfoo    # proteger de autoremove
sudo apt-mark auto libfoo      # autoremove puede eliminarlo

# apt autoremove elimina paquetes "auto" que ya no son
# dependencia de ningún paquete "manual"
```

### Hold: evitar actualizaciones

```bash
# Poner en hold (apt upgrade NO lo tocará)
sudo apt-mark hold linux-image-amd64
# linux-image-amd64 set on hold.

# Ver paquetes en hold
apt-mark showhold

# Quitar el hold
sudo apt-mark unhold linux-image-amd64

# Caso de uso: mantener una versión específica del kernel
# hasta verificar compatibilidad con drivers
```

---

## La caché de paquetes

```
/var/cache/apt/archives/     ← .deb descargados
/var/lib/apt/lists/          ← índice de repositorios (metadatos)
/var/lib/dpkg/info/          ← scripts y datos de paquetes instalados
/var/lib/dpkg/status         ← base de datos de estado de paquetes
```

```bash
# Tamaño de la caché de .deb
du -sh /var/cache/apt/archives/

# Tamaño del índice de repositorios
du -sh /var/lib/apt/lists/

# Limpiar caché de .deb
sudo apt-get clean         # eliminar TODO
sudo apt-get autoclean     # eliminar solo versiones obsoletas

# Limpiar índice + regenerar (patrón Dockerfile)
sudo rm -rf /var/lib/apt/lists/*
sudo apt-get update        # regenera los índices
```

---

## Configuración de APT

```
/etc/apt/sources.list           ← repositorios principales
/etc/apt/sources.list.d/        ← repos adicionales (.list o .sources)
/etc/apt/apt.conf.d/            ← configuración fragmentada (orden numérico)
/etc/apt/preferences.d/         ← pinning (control de prioridades)
/etc/apt/trusted.gpg.d/         ← claves GPG de repositorios
```

### Archivos de configuración fragmentados

```bash
# Los archivos en apt.conf.d/ se procesan en orden numérico:
ls /etc/apt/apt.conf.d/
# 01autoremove           ← configuración de autoremove
# 70debconf              ← preguntas de paquetes
# docker-clean           ← limpieza en contenedores Docker

# No instalar paquetes "Recommends" por defecto
echo 'APT::Install-Recommends "false";' | \
    sudo tee /etc/apt/apt.conf.d/99norecommends

# Configurar proxy para apt
echo 'Acquire::http::Proxy "http://proxy:3128";' | \
    sudo tee /etc/apt/apt.conf.d/99proxy
```

### Pinning (prioridades)

```bash
# /etc/apt/preferences.d/pin-nginx
# Forzar que nginx siempre venga del repo testing:
# Package: nginx
# Pin: release a=testing
# Pin-Priority: 900

# Verificar prioridades resultantes
apt-cache policy nginx
```

---

## Solución de problemas comunes

### "Unable to locate package"

```bash
# 1. Actualizar índice (la causa más frecuente)
sudo apt-get update

# 2. Buscar el nombre correcto
apt-cache search nmap

# 3. Verificar repositorios
apt-cache policy nmap

# 4. Si está en non-free o contrib, editar sources.list
```

### "Could not get lock /var/lib/dpkg/lock-frontend"

```bash
# 1. Verificar si hay un apt/dpkg corriendo
sudo lsof /var/lib/dpkg/lock-frontend

# 2. Si hay un proceso activo: ESPERAR a que termine
#    Nunca matar apt mientras trabaja — puede corromper dpkg

# 3. Si NO hay proceso pero el lock existe (crash anterior):
sudo dpkg --configure -a     # reparar estado inconsistente
# Si sigue fallando (y lsof confirma que no hay proceso):
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo dpkg --configure -a
```

### "Unmet dependencies" / broken packages

```bash
# 1. Intentar reparar automáticamente
sudo apt-get install -f

# 2. Ver detalles del conflicto
apt-cache policy paquete-conflictivo

# 3. Si es necesario resolver manualmente
sudo apt-get dist-upgrade    # intenta resolver removiendo conflictos

# 4. Último recurso: reinstalar
sudo apt-get install --reinstall paquete
```

### "Hash Sum mismatch"

```bash
# El índice descargado no coincide con el hash esperado
# (mirror desactualizado, proxy con caché corrupta, red inestable)
sudo rm -rf /var/lib/apt/lists/*
sudo apt-get update
```

---

## Ejercicios

### Ejercicio 1 — Jerarquía de herramientas APT

Identifica qué herramienta se usa y de qué paquete viene.

```bash
# ¿De qué paquete viene cada herramienta?
for cmd in dpkg apt-get apt-cache apt-mark apt; do
    pkg=$(dpkg -S "$(which $cmd)" 2>/dev/null | head -1 | cut -d: -f1)
    echo "$cmd → paquete: $pkg"
done

# ¿Cuántos paquetes hay instalados en el sistema?
dpkg -l | grep "^ii" | wc -l

# ¿Qué versión de apt tenemos?
apt --version
```

<details><summary>Predicción</summary>

- `dpkg` viene del paquete `dpkg`.
- `apt-get`, `apt-cache`, `apt-mark` y `apt` vienen todos del paquete
  `apt` — son interfaces diferentes de la misma herramienta.
- El número de paquetes instalados depende del contenedor. Un Debian
  mínimo tiene ~100-200 paquetes. Con herramientas de desarrollo
  instaladas, puede ser 300+.
- `apt --version` muestra la versión de la suite APT (p.ej. `2.6.1`).

</details>

### Ejercicio 2 — Comparar apt search vs apt-cache search

Busca el mismo paquete con ambos y compara la salida.

```bash
# Buscar con apt-cache (formato plano)
apt-cache search htop

# Buscar con apt (formato enriquecido)
apt search htop

# Información con apt-cache (raw, parseable)
apt-cache show bash | head -10

# Información con apt (formato legible)
apt show bash 2>/dev/null | head -10

# ¿Cuál usarías en un script? ¿Cuál en la terminal?
```

<details><summary>Predicción</summary>

- `apt-cache search htop` muestra una línea por resultado:
  `htop - interactive processes viewer` — formato plano, fácil de
  parsear con `grep` o `awk`.
- `apt search htop` muestra el mismo resultado pero con formato
  enriquecido: nombre en color, versión, arquitectura, y descripción
  en línea separada. También muestra un warning `WARNING: apt does
  not have a stable CLI interface`.
- `apt-cache show` da output raw con headers exactos (parseable).
  `apt show` da output más legible pero con el warning de API
  inestable.
- **Script**: `apt-cache` (output estable). **Terminal**: `apt`
  (mejor UX).

</details>

### Ejercicio 3 — apt-cache policy: versiones y repositorios

Investiga de dónde vienen tus paquetes.

```bash
# Ver política de bash
apt-cache policy bash

# ¿Qué versión está instalada?
# ¿Cuál es la "Candidate"?
# ¿De qué URL viene?
# ¿Qué significa la prioridad 500?

# Comparar con un paquete no instalado
apt-cache policy nginx

# ¿Cuál es la diferencia en "Installed"?
```

<details><summary>Predicción</summary>

- Para `bash` (instalado):
  - `Installed:` muestra la versión actual (p.ej. `5.2.15-2+b7`).
  - `Candidate:` es la misma (o una más nueva si hay update).
  - URL: `http://deb.debian.org/debian bookworm/main`.
  - Prioridad `500` = prioridad estándar de un repositorio habilitado.
  - `***` marca la versión instalada.
- Para `nginx` (no instalado):
  - `Installed: (none)`.
  - `Candidate:` muestra la versión disponible.
- La prioridad `100` de `/var/lib/dpkg/status` indica que el paquete
  está registrado localmente, no que esté en hold.

</details>

### Ejercicio 4 — apt-cache depends y rdepends

Explora las relaciones de dependencia.

```bash
# ¿De qué depende curl?
apt-cache depends curl

# ¿Quién depende de libc6? (dependencia inversa)
apt-cache rdepends libc6 | head -20

# ¿Cuántos paquetes dependen de libc6?
apt-cache rdepends libc6 | grep -c "^ "

# Investigar un paquete antes de instalar:
apt-cache show vim | grep -E "^(Package|Version|Installed-Size|Depends)"
apt-cache depends vim | grep Depends | wc -l
```

<details><summary>Predicción</summary>

- `curl` depende de `libc6`, `libcurl4`, `zlib1g`, y posiblemente
  `libssl3`.
- `libc6` tiene MUCHAS dependencias inversas — casi todo depende de
  la librería C estándar. Probablemente 100+ paquetes.
- `rdepends` lista todos los paquetes que usan `libc6` como
  dependencia (directa o sugerida).
- Para `vim`: la línea `Installed-Size` muestra el espacio en disco,
  `Depends` muestra cuántas dependencias necesita. `vim` tiene
  bastantes dependencias (vim-runtime, libncurses, etc.).

</details>

### Ejercicio 5 — Paquetes manuales vs automáticos

Entiende cómo funciona autoremove.

```bash
# ¿Cuántos paquetes manuales?
echo "Manuales: $(apt-mark showmanual | wc -l)"

# ¿Cuántos automáticos?
echo "Automáticos: $(apt-mark showauto | wc -l)"

# Primeros 10 manuales
apt-mark showmanual | head -10

# ¿Hay paquetes en hold?
apt-mark showhold

# ¿Hay paquetes candidatos a autoremove?
apt-get autoremove --dry-run 2>/dev/null | grep "^Remv" | wc -l
```

<details><summary>Predicción</summary>

- Los paquetes **manuales** son los que alguien instaló explícitamente
  (p.ej. `apt install curl`). En un contenedor Docker, suelen ser
  los del Dockerfile.
- Los paquetes **automáticos** son dependencias instaladas
  automáticamente. Suelen ser más que los manuales.
- Probablemente no hay paquetes en hold (salvo configuración especial).
- `autoremove --dry-run` muestra qué se eliminaría sin hacerlo.
  En un contenedor limpio, probablemente 0 paquetes candidatos.
- Si marcas un paquete manual como auto (`apt-mark auto pkg`) y
  nada depende de él, `autoremove` lo eliminaría.

</details>

### Ejercicio 6 — La caché de paquetes

Explora dónde guarda apt los archivos descargados.

```bash
# ¿Hay .deb en la caché?
ls /var/cache/apt/archives/*.deb 2>/dev/null | wc -l

# Tamaño de la caché
du -sh /var/cache/apt/archives/

# Tamaño del índice de repositorios
du -sh /var/lib/apt/lists/

# En un Dockerfile se limpian ambos:
echo "rm -rf /var/lib/apt/lists/*  → limpia índice"
echo "apt-get clean                → limpia .deb descargados"

# ¿Por qué Docker limpia la caché?
echo "Cada RUN crea una capa en la imagen."
echo "Los .deb descargados y el índice NO se necesitan en runtime."
echo "Limpiarlos reduce el tamaño de la imagen."
```

<details><summary>Predicción</summary>

- En un contenedor Docker, probablemente hay 0 .deb en la caché —
  los Dockerfiles bien escritos limpian con `apt-get clean` o
  `rm -rf /var/lib/apt/lists/*`.
- El tamaño de la caché vacía es ~4K (solo el directorio).
- El índice (`/var/lib/apt/lists/`) puede ocupar 10-30 MB si se hizo
  `apt-get update` recientemente, o estar vacío si se limpió.
- Docker limpia la caché porque cada instrucción `RUN` crea una capa
  inmutable. Si descargas 50 MB de .deb y luego los limpias en un
  `RUN` separado, los 50 MB siguen en la capa anterior. Por eso se
  hace todo en un solo `RUN`: update + install + clean.

</details>

### Ejercicio 7 — Configuración de APT

Explora los archivos de configuración del sistema.

```bash
# ¿Qué archivos hay en apt.conf.d?
ls /etc/apt/apt.conf.d/

# ¿Qué configuran?
for f in /etc/apt/apt.conf.d/*; do
    echo "--- $(basename $f) ---"
    head -2 "$f" 2>/dev/null
    echo ""
done

# ¿Qué repositorios están configurados?
cat /etc/apt/sources.list 2>/dev/null
ls /etc/apt/sources.list.d/ 2>/dev/null

# ¿Cuántas líneas "deb" activas?
grep -rh "^deb " /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | wc -l
```

<details><summary>Predicción</summary>

- En un contenedor Debian, `apt.conf.d/` tendrá archivos como
  `01autoremove`, `docker-clean` (Docker agrega uno para limpieza
  automática), y posiblemente `70debconf`.
- `docker-clean` suele contener `DPkg::Post-Invoke` que limpia
  automáticamente el caché después de cada operación dpkg.
- `sources.list` tendrá las fuentes de Debian Bookworm (o la versión
  del contenedor). Formato: `deb URL distribución componentes`.
- El número de líneas `deb` activas depende de la configuración.
  Un Debian estándar tiene 2-4 (main, security, updates).

</details>

### Ejercicio 8 — Simular una instalación (dry-run)

Usa `apt-get -s` para ver qué pasaría sin modificar nada.

```bash
# Simular instalar vim (sin instalarlo realmente)
apt-get -s install vim

# ¿Cuántos paquetes se instalarían?
apt-get -s install vim 2>/dev/null | grep "newly installed"

# ¿Cuánto espacio ocuparía?
apt-get -s install vim 2>/dev/null | grep "additional disk space"

# Simular un upgrade completo
apt-get -s upgrade 2>/dev/null | tail -5

# ¿Hay paquetes por actualizar?
```

<details><summary>Predicción</summary>

- `apt-get -s install vim` muestra "The following NEW packages will
  be installed:" seguido de vim y sus dependencias (vim-common,
  vim-runtime, libncurses, etc.). Probablemente 5-10 paquetes.
- La línea "X newly installed" indica cuántos se instalarían.
- "additional disk space" muestra cuánto ocuparían en total.
- `-s` (simulate) no modifica nada — es perfecto para verificar
  antes de actuar.
- `apt-get -s upgrade` muestra si hay paquetes actualizables. En un
  contenedor recién construido, probablemente hay 0 (o algunos si
  pasó tiempo desde el build).

</details>

### Ejercicio 9 — Patrón Dockerfile completo

Analiza un Dockerfile bien escrito y uno mal escrito.

```bash
echo "=== Dockerfile MAL escrito ==="
cat << 'BAD'
FROM debian:bookworm
RUN apt update
RUN apt install nginx curl
RUN apt clean
BAD

echo ""
echo "Problemas:"
echo "  1. Usa 'apt' en vez de 'apt-get' (warnings de CLI inestable)"
echo "  2. Sin -y (se cuelga esperando confirmación)"
echo "  3. Sin DEBIAN_FRONTEND=noninteractive"
echo "  4. Sin --no-install-recommends (imagen más grande)"
echo "  5. 'apt clean' en RUN separado (la caché ya está en capa anterior)"
echo "  6. No limpia /var/lib/apt/lists/*"

echo ""
echo "=== Dockerfile BIEN escrito ==="
cat << 'GOOD'
FROM debian:bookworm
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y --no-install-recommends \
        nginx \
        curl \
    && rm -rf /var/lib/apt/lists/*
GOOD

echo ""
echo "Todo en un solo RUN = una sola capa limpia"
```

<details><summary>Predicción</summary>

- El Dockerfile malo genera una imagen más grande porque:
  - Cada `RUN` crea una capa. `apt update` descarga el índice en la
    capa 1. `apt install` instala paquetes en la capa 2. `apt clean`
    limpia en la capa 3 — pero las capas 1 y 2 siguen conteniendo
    el índice y los .deb.
  - Sin `-y`, Docker no puede responder "yes" y el build falla.
  - Sin `DEBIAN_FRONTEND=noninteractive`, `tzdata` (si es dependencia)
    muestra un prompt interactivo que cuelga el build.
  - Sin `--no-install-recommends`, se instalan paquetes "Recommended"
    que no son necesarios.
- El Dockerfile bueno hace todo en un solo `RUN`, así la capa final
  no contiene el índice ni los .deb temporales.

</details>

### Ejercicio 10 — Panorama: cuándo usar cada herramienta

Resumen práctico.

```bash
echo "=== ¿Qué herramienta uso? ==="
echo ""
echo "Quiero buscar un paquete:"
echo "  Terminal → apt search"
echo "  Script   → apt-cache search"
echo ""
echo "Quiero ver info antes de instalar:"
echo "  apt-cache show pkg        → info completa"
echo "  apt-cache depends pkg     → qué necesita"
echo "  apt-cache rdepends pkg    → quién lo usa"
echo "  apt-cache policy pkg      → de dónde viene, versión"
echo ""
echo "Quiero instalar:"
echo "  Terminal → apt install"
echo "  Script   → apt-get install -y"
echo "  Docker   → DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends"
echo ""
echo "Quiero proteger un paquete:"
echo "  apt-mark hold pkg         → evitar upgrades"
echo "  apt-mark manual pkg       → evitar autoremove"
echo ""
echo "Quiero limpiar:"
echo "  apt-get clean             → borrar .deb del caché"
echo "  apt-get autoremove        → borrar dependencias huérfanas"
echo ""
echo "Quiero código fuente:"
echo "  apt-get source pkg        → descargar fuentes"
echo "  apt-get build-dep pkg     → instalar deps de compilación"
```

<details><summary>Predicción</summary>

- Este ejercicio no tiene output sorprendente — es un resumen de
  referencia.
- La regla de oro: **`apt` para humanos, `apt-get` para máquinas**.
- `apt-cache` es la herramienta de investigación: `show`, `depends`,
  `rdepends`, `policy` son comandos que todo sysadmin debe conocer
  para diagnosticar problemas de paquetes.
- `apt-mark` es la herramienta de control: `hold` protege versiones
  críticas (kernel, drivers), `manual/auto` controla autoremove.
- En Dockerfiles: siempre `apt-get`, siempre un solo `RUN`, siempre
  limpiar al final.

</details>
