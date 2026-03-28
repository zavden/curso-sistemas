# T02 — Repositorios

## Errata y notas sobre el material base

1. **README.max.md: estructura del repositorio con .deb en `dists/`** —
   Los archivos .deb NO están dentro de `dists/bookworm/main/`. La
   estructura real es: `dists/` contiene solo metadatos (Packages.gz,
   Release), y `pool/` contiene los .deb. Ejemplo real:
   `pool/main/n/nginx/nginx_1.22.1-9_amd64.deb`.

2. **README.max.md: "Se procesan en orden alfabético — el último que
   coincide gana"** — Incorrecto para pinning. apt usa la pin con el
   **Pin-Priority más alto**, no el último archivo. Si hay empate, el
   primero en orden de lectura gana. El orden de los archivos importa
   solo cuando las prioridades son iguales.

3. **README.max.md: `apt-cache search -t bookworm-backports .`** — El
   flag `-t` no existe en `apt-cache search`. `-t` es para
   `apt-get install` (seleccionar target release). Para buscar en
   backports: `apt list -a | grep backports`.

4. **README.max.md: `100 100` en Version table** — Typo, la prioridad
   aparece duplicada. Debería ser solo `100`.

5. **README.md: `non-free-firmware` "No soportado oficialmente"** —
   En Debian 12, `non-free-firmware` está habilitado por defecto en la
   instalación y es reconocido oficialmente (a diferencia de `non-free`).
   No tiene soporte de seguridad garantizado, pero sí es distribuido
   oficialmente.

6. **Ambos READMEs: falta explicar `InRelease` vs `Release.gpg`** —
   Los repos modernos usan `InRelease` (Release + firma GPG en un solo
   archivo, clearsigned). `Release` + `Release.gpg` (firma separada) es
   el formato legacy.

---

## Qué es un repositorio APT

Un repositorio es un servidor HTTP/HTTPS que contiene paquetes `.deb`
y metadatos. Cuando ejecutas `apt install`, apt busca el paquete en
los índices locales (descargados con `apt update`), descarga el `.deb`
del servidor, y lo pasa a `dpkg` para instalarlo.

```
Estructura real de un repositorio Debian:

  http://deb.debian.org/debian/
  ├── dists/                           ← METADATOS
  │   ├── bookworm/                    ← suite (release)
  │   │   ├── InRelease                ← índice firmado (GPG inline)
  │   │   ├── Release                  ← índice sin firma
  │   │   ├── Release.gpg              ← firma GPG separada (legacy)
  │   │   ├── main/                    ← componente
  │   │   │   └── binary-amd64/
  │   │   │       └── Packages.gz      ← lista de paquetes comprimida
  │   │   ├── contrib/
  │   │   └── non-free-firmware/
  │   ├── bookworm-security/
  │   └── bookworm-updates/
  └── pool/                            ← PAQUETES (.deb)
      ├── main/
      │   ├── n/nginx/
      │   │   └── nginx_1.22.1-9_amd64.deb
      │   └── c/curl/
      │       └── curl_7.88.1_amd64.deb
      └── contrib/
```

> Los `.deb` están en `pool/`, no en `dists/`. Esto permite que
> múltiples suites compartan el mismo `.deb` sin duplicarlo.

### Flujo de apt update → apt install

```
apt update:
  1. Descarga InRelease de cada repo (o Release + Release.gpg)
  2. Verifica la firma GPG
  3. Descarga Packages.gz (índice de paquetes)
  4. Guarda todo en /var/lib/apt/lists/

apt install nginx:
  1. Busca "nginx" en los índices locales (/var/lib/apt/lists/)
  2. Resuelve dependencias (nginx-common, libc6, libpcre2...)
  3. Descarga cada .deb desde pool/ del repositorio
  4. Pasa los .deb a dpkg para instalación
```

Sin `apt update` previo, apt usa índices viejos y puede no encontrar
paquetes nuevos o descargar versiones desactualizadas.

---

## sources.list — Formato clásico (one-line)

```bash
cat /etc/apt/sources.list
```

```
# type   URI                                       suite               components
deb      http://deb.debian.org/debian               bookworm            main contrib non-free-firmware
deb      http://security.debian.org/debian-security  bookworm-security   main contrib non-free-firmware
deb      http://deb.debian.org/debian               bookworm-updates    main contrib non-free-firmware
```

### Campos

| Campo | Significado | Ejemplo |
|---|---|---|
| `type` | `deb` (binarios) o `deb-src` (código fuente) | `deb` |
| `URI` | URL del repositorio (HTTP, HTTPS, file://) | `http://deb.debian.org/debian` |
| `suite` | Nombre de la release | `bookworm`, `bookworm-security` |
| `components` | Secciones (separadas por espacios) | `main contrib non-free-firmware` |

> `deb-src` solo se necesita si vas a compilar paquetes desde fuente
> (`apt-get source`, `apt-get build-dep`). Para uso normal, solo `deb`.

### Suites en Debian

| Suite | Contenido | Frecuencia de actualización |
|---|---|---|
| `bookworm` | Release estable inicial | Congelada (no cambia) |
| `bookworm-security` | Parches de seguridad | Continua (urgente) |
| `bookworm-updates` | Bug fixes menores | Regular |
| `bookworm-backports` | Versiones más nuevas de testing | Sincronizada |

### Componentes de Debian

| Componente | Licencia | Soporte Debian |
|---|---|---|
| `main` | 100% libre (DFSG), solo depende de main | Sí (soporte completo) |
| `contrib` | Libre, pero depende de non-free | Parcial |
| `non-free` | Propietario (drivers, codecs, etc.) | No |
| `non-free-firmware` | Firmware propietario (Debian 12+) | Distribuido oficialmente, sin soporte de seguridad garantizado |

### Componentes de Ubuntu

| Componente | Mantenido por | Licencia |
|---|---|---|
| `main` | Canonical | Libre, soporte completo |
| `universe` | Comunidad | Libre, sin soporte oficial |
| `restricted` | Canonical | No libre (drivers) |
| `multiverse` | Comunidad | No libre |

---

## Formato DEB822 (moderno)

Desde Debian 12 y Ubuntu 24.04, el formato preferido usa archivos
`.sources` en `/etc/apt/sources.list.d/`:

```bash
cat /etc/apt/sources.list.d/debian.sources
```

```
Types: deb
URIs: http://deb.debian.org/debian
Suites: bookworm bookworm-updates
Components: main contrib non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg

Types: deb
URIs: http://security.debian.org/debian-security
Suites: bookworm-security
Components: main contrib non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

### one-line vs DEB822

```
One-line:
  deb http://deb.debian.org/debian bookworm main contrib

DEB822:
  Types: deb
  URIs: http://deb.debian.org/debian
  Suites: bookworm
  Components: main contrib
  Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
```

Ventajas de DEB822:
- `Signed-By` inline — la clave GPG vive junto al repo que protege
- Múltiples suites en un solo bloque (`Suites: bookworm bookworm-updates`)
- Más legible — campos separados en lugar de una línea larga
- Archivos `.sources` en `sources.list.d/` — fácil de agregar/eliminar

---

## Archivos en sources.list.d/

```bash
ls /etc/apt/sources.list.d/
# debian.sources       ← distribución base (DEB822)
# docker.list          ← repositorio de Docker (one-line)
# nodesource.list      ← Node.js (one-line)

# Cada archivo es independiente
# Agregar repo = crear archivo
# Eliminar repo = borrar archivo
# Más seguro que editar sources.list
```

Ambos formatos coexisten: `.list` (one-line) y `.sources` (DEB822).

---

## Claves GPG — Autenticación de repositorios

APT verifica la autenticidad de los paquetes con firmas GPG. Cada
repositorio firma su `InRelease` con una clave privada; apt verifica
con la clave pública correspondiente:

```
Repositorio                           Tu máquina
    │                                     │
    │  InRelease (índice firmado)         │
    │  ────────────────────────────────► │
    │                                     │
    │                   ¿Firmado con la   │
    │                   clave pública     │
    │                   que tengo?        │
    │                                     │
    │              SÍ → OK, instalar      │
    │              NO → Error GPG         │
```

### Método legacy vs moderno

| Método | Ubicación | Seguridad |
|---|---|---|
| `apt-key` (deprecated) | `/etc/apt/trusted.gpg` (global) | Inseguro: cualquier clave firma cualquier repo |
| `Signed-By` (moderno) | `/usr/share/keyrings/` o `/etc/apt/keyrings/` | Seguro: cada repo tiene su clave propia |

```bash
# Ver claves modernas
ls /usr/share/keyrings/
# debian-archive-keyring.gpg     ← claves oficiales de Debian

ls /etc/apt/keyrings/
# docker.gpg                     ← clave de Docker (si se instaló)
```

### Instalar clave GPG (método moderno)

```bash
# 1. Descargar la clave del proveedor
curl -fsSL https://repo.example.com/key.gpg -o /tmp/key.gpg

# 2. (Opcional) Verificar que es una clave GPG válida
gpg --show-keys /tmp/key.gpg

# 3. Convertir a formato binario (dearmor) y guardar
sudo gpg --dearmor -o /etc/apt/keyrings/example.gpg /tmp/key.gpg

# 4. Asignar permisos de lectura
sudo chmod a+r /etc/apt/keyrings/example.gpg

# 5. Referenciar en el archivo del repositorio
# .list:    deb [signed-by=/etc/apt/keyrings/example.gpg] https://... suite main
# .sources: Signed-By: /etc/apt/keyrings/example.gpg
```

> Algunos proveedores dan la clave ya en formato binario (`.asc` o
> `.gpg`). En ese caso, solo copiar a `/etc/apt/keyrings/` sin
> `--dearmor`.

---

## PPAs (solo Ubuntu)

Los **PPA** (Personal Package Archives) son repos de terceros alojados
en Launchpad:

```bash
# Agregar un PPA (automatiza: clave GPG + .list + update)
sudo add-apt-repository ppa:deadsnakes/ppa
sudo apt install python3.12

# Lo que hace internamente:
# 1. Descarga clave GPG del PPA
# 2. Crea /etc/apt/sources.list.d/deadsnakes-*.list
# 3. Ejecuta apt update

# Eliminar un PPA
sudo add-apt-repository --remove ppa:deadsnakes/ppa

# Listar PPAs activos
grep -r "ppa" /etc/apt/sources.list.d/
```

Los PPA **no existen en Debian**. En Debian, los repos de terceros se
configuran manualmente.

---

## Prioridades (apt pinning)

Cuando un paquete existe en varios repos con versiones distintas, apt
usa prioridades para decidir cuál instalar:

### Sistema de prioridades

| Prioridad | Comportamiento |
|---|---|
| `> 1000` | Force install (incluso downgrade) — solo emergencias |
| `500` | Default para repos habilitados — se instala si es más nueva |
| `100` | Default para backports/not-automatic — solo si se pide explícitamente |
| `< 0` | **Nunca** se instala |

```bash
# Ver prioridades de un paquete
apt-cache policy nginx
# nginx:
#   Installed: 1.22.1-9
#   Candidate: 1.22.1-9
#   Version table:
#  *** 1.22.1-9 500
#        500 http://deb.debian.org/debian bookworm/main
#   1.24.0-1~bpo12+1 100
#        100 http://deb.debian.org/debian bookworm-backports/main

# La versión de backports tiene prioridad 100 → no se instala
# automáticamente. Hay que pedir: apt install -t bookworm-backports nginx
```

### Archivo de preferencias (pinning)

```bash
# Ubicación: /etc/apt/preferences.d/<nombre>

# Ejemplo: preferir nginx de backports
# /etc/apt/preferences.d/nginx-backports
Package: nginx
Pin: release a=bookworm-backports
Pin-Priority: 600

# Ejemplo: bloquear un paquete (nunca instalar)
# /etc/apt/preferences.d/block-snapd
Package: snapd
Pin: release *
Pin-Priority: -1

# Ejemplo: preferir todo de un repo específico
# /etc/apt/preferences.d/prefer-docker
Package: docker-ce*
Pin: origin "download.docker.com"
Pin-Priority: 900
```

### Patrones de Pin

```bash
Pin: release a=bookworm-backports    # por archive (suite)
Pin: release n=bookworm              # por codename
Pin: release l=Debian                # por label
Pin: release o=Debian                # por origin (operador)
Pin: origin "deb.debian.org"         # por URL del servidor
Pin: version 1.22.*                  # por versión (wildcards)
```

> apt usa la pin con el **Pin-Priority más alto** que coincida. Si
> hay empate, la primera coincidencia gana. No es "el último archivo
> sobreescribe" como en algunos otros sistemas.

---

## Backports

Los **backports** son versiones más nuevas de paquetes, recompiladas
desde `testing`/`unstable` para funcionar en `stable`:

```bash
# Habilitar backports
echo "deb http://deb.debian.org/debian bookworm-backports main" | \
    sudo tee /etc/apt/sources.list.d/backports.list
sudo apt-get update

# Prioridad default: 100 → no se instalan automáticamente
# Hay que pedirlo explícitamente con -t:
sudo apt-get install -t bookworm-backports nginx

# Ver versiones disponibles (incluyendo backports)
apt list -a nginx

# Nota: -t NO funciona con apt-cache search.
# Para buscar en backports:
apt list -a 2>/dev/null | grep backports | head
```

---

## Agregar un repositorio de terceros (procedimiento completo)

### Ejemplo: repositorio oficial de Docker

```bash
# 1. Instalar dependencias
sudo apt-get install -y ca-certificates curl

# 2. Crear directorio de claves si no existe
sudo install -m 0755 -d /etc/apt/keyrings

# 3. Descargar la clave GPG
curl -fsSL https://download.docker.com/linux/debian/gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# 4. Crear el archivo del repositorio
echo "deb [arch=$(dpkg --print-architecture) \
    signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/debian \
    $(. /etc/os-release && echo $VERSION_CODENAME) stable" | \
    sudo tee /etc/apt/sources.list.d/docker.list

# 5. Actualizar índices
sudo apt-get update

# 6. Verificar
apt-cache policy docker-ce

# 7. Instalar
sudo apt-get install -y docker-ce docker-ce-cli containerd.io
```

### Para eliminar un repositorio

```bash
# 1. Borrar el archivo del repo
sudo rm /etc/apt/sources.list.d/docker.list

# 2. Borrar la clave GPG
sudo rm /etc/apt/keyrings/docker.gpg

# 3. Actualizar
sudo apt-get update
```

---

## Diagnóstico de problemas

### "Unable to locate package"

```bash
# 1. Actualizar índices (causa más frecuente)
sudo apt-get update

# 2. Buscar el nombre correcto
apt-cache search nombre

# 3. Verificar si está en un componente no habilitado
apt-cache policy nombre
# Si no aparece, el paquete no está en ningún repo configurado

# 4. Si está en non-free o contrib, editar sources.list
```

### "GPG error: NO_PUBKEY"

```bash
# 1. Identificar la clave faltante (del mensaje de error)
# W: GPG error: ... NO_PUBKEY AABBCCDD11223344

# 2. Buscar la clave en la documentación del proveedor
# 3. Descargar e instalar
curl -fsSL https://repo.example.com/key.gpg | \
    sudo gpg --dearmor -o /etc/apt/keyrings/repo.gpg

# 4. Verificar que el .list tiene Signed-By apuntando a esa clave
# 5. sudo apt-get update
```

### "Repository does not have a Release file"

```bash
# La URL del repo es incorrecta o el repo está offline
# Verificar que la suite existe:
curl -I http://deb.debian.org/debian/dists/bookworm/InRelease
# 200 OK = existe

# Revisar el archivo .list para errores de tipeo
cat /etc/apt/sources.list.d/repo.list
```

### Salida de `apt update`

```bash
sudo apt-get update
# Hit:1  http://deb.debian.org/debian bookworm InRelease
# Get:2  http://security.debian.org bookworm-security InRelease [48.0 kB]
# Ign:3  http://repo.offline.com/debian stable InRelease
# Err:4  http://repo.broken.com/debian stable InRelease
#   Could not resolve 'repo.broken.com'

# Hit  = índice sin cambios (ya lo tenías, autenticado OK)
# Get  = índice descargado (nuevo o actualizado)
# Ign  = ignorado (sin Release o InRelease)
# Err  = error (DNS, red, GPG)
```

---

## Ejercicios

### Ejercicio 1 — Explorar los repositorios configurados

```bash
# ¿Qué repositorios tienes?
cat /etc/apt/sources.list 2>/dev/null
ls /etc/apt/sources.list.d/

# ¿Qué formato usan? (.list o .sources)
file /etc/apt/sources.list.d/* 2>/dev/null

# ¿Cuántas líneas "deb" activas hay?
grep -rh "^deb " /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | wc -l

# ¿Qué componentes están habilitados?
grep -rh "^deb " /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null
```

<details><summary>Predicción</summary>

- En un contenedor Debian, `sources.list` puede estar vacío o tener
  las fuentes principales. Alternativamente, puede haber un archivo
  `.sources` en `sources.list.d/` con formato DEB822.
- Los archivos `.list` son "ASCII text" y `.sources` también.
- Habrá 1-4 líneas `deb` activas (main, security, updates).
- Componentes: al menos `main`. El contenedor del curso puede tener
  `contrib` y `non-free-firmware` también.

</details>

### Ejercicio 2 — Índices locales en /var/lib/apt/lists/

```bash
# ¿Qué hay en el directorio de índices?
ls /var/lib/apt/lists/ | head -10

# ¿Cuánto ocupan?
du -sh /var/lib/apt/lists/

# ¿Cuántos paquetes están en el índice principal?
PKGFILE=$(ls /var/lib/apt/lists/*Packages* 2>/dev/null | head -1)
if [ -n "$PKGFILE" ]; then
    zcat "$PKGFILE" 2>/dev/null | grep -c "^Package:" || \
    cat "$PKGFILE" | grep -c "^Package:"
fi

# Buscar un paquete directamente en el índice
zcat "$PKGFILE" 2>/dev/null | grep -A 5 "^Package: curl$" || \
cat "$PKGFILE" 2>/dev/null | grep -A 5 "^Package: curl$"
```

<details><summary>Predicción</summary>

- Los archivos tienen nombres largos basados en la URL del repo:
  `deb.debian.org_debian_dists_bookworm_main_binary-amd64_Packages`.
  Pueden estar comprimidos (.gz, .xz) o sin comprimir.
- Ocupan 10-30 MB dependiendo de cuántos repos hay.
- El índice principal de Debian `main` tiene ~60,000+ paquetes.
- Cada entrada en el índice tiene: Package, Version, Architecture,
  Depends, Size, Filename (ruta en `pool/`), SHA256, Description.
- Estos son los archivos que `apt-cache show` y `apt-cache search`
  consultan — nunca contactan al servidor para estas operaciones.

</details>

### Ejercicio 3 — Claves GPG

```bash
# ¿Qué claves modernas hay?
echo "=== /usr/share/keyrings/ ==="
ls /usr/share/keyrings/ 2>/dev/null

echo ""
echo "=== /etc/apt/keyrings/ ==="
ls /etc/apt/keyrings/ 2>/dev/null || echo "(no existe)"

echo ""
echo "=== Claves legacy (deprecated) ==="
ls /etc/apt/trusted.gpg.d/ 2>/dev/null || echo "(vacío)"

# ¿Hay un keyring global legacy?
ls -la /etc/apt/trusted.gpg 2>/dev/null || echo "No hay trusted.gpg global"

# ¿Algún repo usa Signed-By?
grep -r "Signed-By\|signed-by" /etc/apt/sources.list.d/ 2>/dev/null
```

<details><summary>Predicción</summary>

- `/usr/share/keyrings/` tendrá `debian-archive-keyring.gpg` (las
  claves oficiales de Debian) y posiblemente
  `debian-archive-removed-keyring.gpg` (claves antiguas revocadas).
- `/etc/apt/keyrings/` puede no existir aún (se crea manualmente
  cuando agregas repos de terceros).
- `/etc/apt/trusted.gpg.d/` puede tener claves legacy de repos
  añadidos con el método antiguo.
- Si hay archivos `.sources` con formato DEB822, usarán `Signed-By`.
  Archivos `.list` modernos usan `[signed-by=...]`.

</details>

### Ejercicio 4 — apt-cache policy sin argumentos

```bash
# Ver la política global (todos los repos y sus prioridades)
apt-cache policy | head -30

# ¿Cuántos repos aparecen?
apt-cache policy | grep "http" | wc -l

# ¿Qué prioridades tienen?
apt-cache policy | grep -oP '\d+(?= http)' | sort -u

# Comparar con la política de un paquete específico
apt-cache policy bash
apt-cache policy curl
```

<details><summary>Predicción</summary>

- `apt-cache policy` sin argumentos lista todos los repos con sus
  prioridades. Los repos estándar de Debian tendrán prioridad `500`.
- Verás líneas como `500 http://deb.debian.org/debian bookworm/main`.
- Las prioridades serán `500` (repos normales) y `100` (backports
  si están habilitados).
- Para `bash`: Installed y Candidate serán la misma versión (no hay
  upgrade disponible si el índice está actualizado).
- Para `curl`: similar — la versión de bookworm estándar.

</details>

### Ejercicio 5 — Componentes de Debian

```bash
# ¿Qué componentes están habilitados en tus repos?
grep -rh "^deb " /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | \
    awk '{for(i=4;i<=NF;i++) print $i}' | sort -u

# Intentar buscar un paquete que esté en non-free
apt-cache search firmware | head -5

# ¿Está non-free habilitado?
grep -r "non-free" /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null

# Diferencia: non-free vs non-free-firmware (Debian 12+)
echo "non-free: software propietario (drivers, codecs)"
echo "non-free-firmware: solo firmware (Debian 12 lo separó)"
```

<details><summary>Predicción</summary>

- `main` siempre estará presente. `contrib` y `non-free-firmware`
  probablemente también (Debian 12 los habilita por defecto en la
  instalación).
- `non-free` puede o no estar habilitado — depende de si se necesita
  software propietario.
- Paquetes de firmware como `firmware-linux-nonfree` están en
  `non-free-firmware` (antes estaban en `non-free`).
- Debian 12 separó firmware del resto de non-free para permitir
  incluir firmware en el instalador sin habilitar todo non-free.

</details>

### Ejercicio 6 — Formato DEB822 vs one-line

```bash
# ¿Hay archivos .sources (DEB822)?
ls /etc/apt/sources.list.d/*.sources 2>/dev/null

# Si hay, leerlos
for f in /etc/apt/sources.list.d/*.sources; do
    if [ -f "$f" ]; then
        echo "=== $(basename $f) ==="
        cat "$f"
        echo ""
    fi
done

# Comparar con los .list
for f in /etc/apt/sources.list.d/*.list; do
    if [ -f "$f" ]; then
        echo "=== $(basename $f) ==="
        cat "$f"
        echo ""
    fi
done

# ¿Cuál es más legible?
```

<details><summary>Predicción</summary>

- Si el contenedor usa Debian 12+ con la configuración por defecto,
  habrá un archivo `debian.sources` en formato DEB822 con los bloques
  de Debian main y security.
- Los archivos `.list` (si los hay) serán repos de terceros añadidos
  manualmente.
- DEB822 es más legible: campos separados, `Signed-By` inline, y
  soporta múltiples suites en un solo bloque.
- Ambos formatos coexisten — apt lee todos los archivos de ambos tipos.

</details>

### Ejercicio 7 — Simular agregar un repositorio

```bash
# No vamos a instalar nada, solo entender el procedimiento

echo "=== Pasos para agregar un repo de terceros ==="
echo ""
echo "1. Descargar la clave GPG del proveedor:"
echo "   curl -fsSL https://example.com/key.gpg | \\"
echo "       sudo gpg --dearmor -o /etc/apt/keyrings/example.gpg"
echo ""
echo "2. Crear el archivo del repo:"
echo "   echo 'deb [signed-by=/etc/apt/keyrings/example.gpg] \\"
echo "       https://repo.example.com/debian bookworm main' | \\"
echo "       sudo tee /etc/apt/sources.list.d/example.list"
echo ""
echo "3. apt-get update"
echo ""
echo "4. apt-get install package-name"
echo ""

# Verificar que entiendes: ¿qué hace cada flag de la línea deb?
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian bookworm stable"
echo ""
echo "Pregunta: identifica type, opciones, URI, suite, components"
```

<details><summary>Predicción</summary>

- Desglose de la línea:
  - `deb` → type (binarios)
  - `[arch=amd64 signed-by=...]` → opciones (arquitectura y clave GPG)
  - `https://download.docker.com/linux/debian` → URI
  - `bookworm` → suite
  - `stable` → component (no es main/contrib sino "stable" del repo
    de Docker)
- Los corchetes `[...]` son una extensión del formato one-line que
  permite especificar opciones como `arch`, `signed-by`, `trusted`.
- `signed-by` vincula la clave GPG específica al repo, evitando que
  otras claves puedan firmar paquetes de este repo.

</details>

### Ejercicio 8 — Prioridades y pinning

```bash
# ¿Hay archivos de preferencias?
ls /etc/apt/preferences.d/ 2>/dev/null || echo "(ninguno)"

# Ver prioridades actuales de un paquete
apt-cache policy apt

# ¿Qué pasaría con esta configuración?
echo "Escenario hipotético:"
echo ""
echo "Package: nginx"
echo "Pin: release a=bookworm-backports"
echo "Pin-Priority: 600"
echo ""
echo "Pregunta: si nginx 1.22 está en bookworm (prioridad 500)"
echo "y nginx 1.24 está en backports (normalmente prioridad 100),"
echo "¿cuál se instalaría con esta pin?"
```

<details><summary>Predicción</summary>

- Sin pin: nginx 1.22 (prioridad 500 > 100 de backports).
- Con la pin `Pin-Priority: 600` para backports: nginx 1.24 de
  backports (prioridad 600 > 500 de bookworm).
- La pin **sube** la prioridad de backports a 600 solo para nginx,
  haciéndola mayor que los 500 del repo estándar.
- Sin la pin, habría que usar `apt install -t bookworm-backports nginx`
  para forzar la versión de backports.
- Probablemente no hay archivos de preferencias en el contenedor
  del curso (no se configuró pinning).

</details>

### Ejercicio 9 — Salida de apt update

```bash
# Ejecutar update y analizar la salida
apt-get update 2>&1 | head -20

# Interpretar cada línea:
echo ""
echo "=== Significado ==="
echo "Hit  = índice sin cambios (ya lo tenías, firma OK)"
echo "Get  = índice descargado (nuevo o actualizado)"
echo "Ign  = ignorado (sin Release/InRelease)"
echo "Err  = error (DNS, red, GPG, repo offline)"
echo ""

# ¿Cuántos repos se consultaron?
apt-get update 2>&1 | grep -cE "^(Hit|Get|Ign|Err)"
```

<details><summary>Predicción</summary>

- La mayoría de líneas serán `Hit:` si el update se ejecutó
  recientemente (los índices no cambiaron).
- Si es la primera vez, serán `Get:` (descargando todo).
- Cada línea corresponde a un componente de un repo: main, contrib,
  security, updates — cada uno se descarga por separado.
- Probablemente 3-6 repos se consultarán (main, security, updates,
  posiblemente contrib y non-free-firmware).

</details>

### Ejercicio 10 — Panorama: del repo al paquete instalado

```bash
echo "=== Flujo completo: del repo al paquete ==="
echo ""
echo "1. Repo: ¿de dónde vienen los paquetes?"
grep -rh "^deb " /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null | head -3
echo ""

echo "2. GPG: ¿cómo se verifican?"
ls /usr/share/keyrings/ 2>/dev/null | head -3
echo ""

echo "3. Índice: ¿dónde se guardan los metadatos?"
echo "   /var/lib/apt/lists/ ($(ls /var/lib/apt/lists/ | wc -l) archivos)"
echo ""

echo "4. Policy: ¿qué versión se instala?"
apt-cache policy bash | head -5
echo ""

echo "5. Caché: ¿dónde se descargan los .deb?"
echo "   /var/cache/apt/archives/ ($(ls /var/cache/apt/archives/*.deb 2>/dev/null | wc -l) archivos)"
echo ""

echo "6. Instalado: ¿dónde queda registrado?"
echo "   /var/lib/dpkg/status ($(dpkg -l | grep -c '^ii') paquetes)"
```

<details><summary>Predicción</summary>

- Este ejercicio traza el flujo completo:
  1. **Repos**: las URLs de donde vienen los paquetes (Debian mirrors).
  2. **GPG**: las claves en `/usr/share/keyrings/` que verifican la
     autenticidad.
  3. **Índice**: los archivos Packages descargados en
     `/var/lib/apt/lists/` — esto es lo que consulta `apt-cache`.
  4. **Policy**: la versión instalada y candidata con sus prioridades.
  5. **Caché**: los .deb descargados (probablemente 0 en un
     contenedor limpio).
  6. **Instalado**: el registro en `/var/lib/dpkg/status` con el
     conteo total de paquetes.
- El flujo es: repo → GPG verifica → índice local → policy decide
  → descarga .deb → dpkg instala → status actualizado.

</details>
