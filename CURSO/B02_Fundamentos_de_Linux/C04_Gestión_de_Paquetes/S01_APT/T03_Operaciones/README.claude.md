# T03 — Operaciones APT

## Errata respecto a las fuentes originales

1. **`== es comparativa`** (README.max.md): no existe operador `==` en APT.
   La sintaxis es `apt install package=version` con un solo `=`.
2. **`dpkg trigers pipe /etc/dpki/post-inst/`** (README.max.md): múltiples
   errores — "trigers" → "triggers", y la ruta correcta de los maintainer
   scripts es `/var/lib/dpkg/info/<package>.postinst` (no `/etc/dpki/`).
3. **`lib、PCRE2`** (README.max.md): usa coma china `、` y nombre de
   librería incompleto.
4. **`nuncaambahora`** (README.max.md): texto corrupto, debería decir
   "nunca instala ni elimina".
5. **`distribuction`** (README.max.md): typo → "distribution".
6. **`autoclean` descrito como "versiones ya instaladas"** (README.max.md):
   incorrecto. `autoclean` elimina `.deb` de paquetes que **ya no están
   disponibles en ningún repositorio**, no "versiones ya instaladas".
7. **`dpkg --force-all --configure -a`** (README.max.md): presentado como
   paso razonable de recuperación. Es destructivo — puede ignorar
   conflictos y dejar el sistema peor. Solo usar como último recurso
   absoluto y preferiblemente en un snapshot/VM.
8. **regex `[i-uln]`** (README.max.md): crea un rango `i-u` que incluye
   j,k,l,m,n,o,p,q,r,s,t,u — demasiado amplio. El filtro correcto es
   `awk '/^[a-z]/ && !/^ii/'`.
9. **cowsay para demo remove vs purge** (labs): cowsay no tiene archivos en
   `/etc/`, así que no demuestra la diferencia de configuración. Un paquete
   como `nano` o `less` (que sí ponen config en `/etc/`) sería mejor ejemplo.

---

## apt update — Sincronizar indices

```bash
sudo apt update
# Descarga los indices de paquetes de TODOS los repositorios configurados
# NO instala nada — solo actualiza la lista de disponibles
```

### Flujo de apt update

```
Tu maquina                           Repositorios
   |                                     |
   |  GET /dists/bookworm/InRelease     |
   | ----------------------------------->|
   |                                     |
   |  200 OK  (o 304 Not Modified)      |
   | <-----------------------------------|
   |                                     |
   |  GET /dists/bookworm/main/          |
   |    binary-amd64/Packages.xz        |
   | ----------------------------------->|
   |                                     |
   |  200 OK + Packages.xz              |
   | <-----------------------------------|
   |                                     |
   |  Guardar en /var/lib/apt/lists/     |
```

### Interpretar la salida

```
Hit:1  http://deb.debian.org/debian bookworm InRelease
  → El indice no cambio (304 Not Modified)

Get:2  http://security.debian.org bookworm-security InRelease [48.0 kB]
  → Se descargo un indice actualizado

Ign:3  https://repo.example.com stable InRelease
  → Se ignoro (error temporal, conexion rechazada)

Err:4  https://repo.roto.com stable InRelease
  → Error grave — clave GPG faltante, repo fuera de linea, URL incorrecta
```

```
Al final:
  "All packages are up to date"           → indices ok, no hay updates
  "15 packages can be upgraded"           → hay updates disponibles
  "Some index files failed to download"   → revisar los Err
```

`apt update` debe ejecutarse **antes** de `install` o `upgrade` para
asegurar que los indices estan actualizados. En Dockerfiles es
**obligatorio** antes de cualquier instalacion.

---

## apt install — Instalar paquetes

```bash
# Instalar un paquete
sudo apt install nginx

# Instalar varios
sudo apt install nginx curl htop

# Version especifica (un solo = , no ==)
sudo apt install nginx=1.22.1-9

# Sin paquetes recomendados (reduce tamano en contenedores/servidores)
sudo apt install --no-install-recommends nginx

# Responder "yes" automaticamente
sudo apt install -y nginx

# Simular (dry run) — ver que se instalaria sin hacerlo
apt install -s nginx          # no necesita sudo
apt install --simulate nginx  # equivalente
```

### Que ocurre durante apt install

```
1. RESOLUCION DE DEPENDENCIAS
   apt busca el paquete en /var/lib/apt/lists/ (indices locales)
   Construye el grafo de dependencias

2. PLAN DE ACCION
   Muestra:
   - Paquetes NUEVOS que se instalaran
   - Paquetes que se ACTUALIZARAN
   - Paquetes que se ELIMINARAN (si hay conflictos)
   - Espacio en disco necesario

3. CONFIRMACION (si no es -y)
   "Do you want to continue? [Y/n]"

4. DESCARGA
   .deb → /var/cache/apt/archives/
   apt muestra barra de progreso (apt-get no)

5. INSTALACION con dpkg
   dpkg -i package1.deb package2.deb ...
   Descomprime archivos, coloca en /usr/, /etc/, etc.

6. CONFIGURACION POST-INSTALACION
   dpkg ejecuta los maintainer scripts:
   /var/lib/dpkg/info/<package>.postinst
   (reiniciar servicios, enable, etc.)
```

### Tipos de dependencias

```bash
apt-cache depends nginx
# o
apt show nginx
```

| Tipo | Significado | Comportamiento |
|---|---|---|
| `Depends` | Requerido para funcionar | Se instala obligatoriamente |
| `Pre-Depends` | Debe estar instalado Y configurado ANTES | Se instala primero |
| `Recommends` | Mejora funcionalidad | Se instala por defecto (salvo `--no-install-recommends`) |
| `Suggests` | Funcionalidad extra opcional | No se instala por defecto |
| `Conflicts` | Incompatible | Se elimina si esta instalado |
| `Breaks` | Rompe versiones antiguas | Fuerza actualizacion del conflictivo |

### Reinstalar un paquete

```bash
# Reinstalar (misma version) — util si archivos fueron modificados/corruptos
sudo apt install --reinstall nginx

# Verificar
dpkg -s nginx | grep -E "Status|Version"
```

---

## apt remove — Desinstalar (conserva configuracion)

```bash
# Eliminar binarios, preservar /etc/
sudo apt remove nginx

# Verificar estado
dpkg -l nginx
# rc  nginx  1.22.1-9  amd64
#  r = desired: remove
#  c = status: config-files present

# Que archivos de configuracion sobreviven
dpkg -L nginx | grep "/etc/"
# /etc/nginx/nginx.conf
# /etc/nginx/sites-available/
```

---

## apt purge — Desinstalar (elimina todo)

```bash
# Eliminar binarios + configuracion en /etc/
sudo apt purge nginx

# Equivalente:
sudo apt remove --purge nginx

# Verificar
dpkg -l nginx
# pn  nginx  ...
#  p = desired: purge
#  n = status: not-installed

ls /etc/nginx/ 2>/dev/null
# No such file or directory
```

### remove vs purge

| Operacion | Binarios | Config /etc | Datos usuario | Logs |
|---|---|---|---|---|
| `remove` | Eliminados | **Conservados** | No toca | No toca |
| `purge` | Eliminados | **Eliminados** | No toca | No toca |

**Ninguno elimina datos de usuario** (`/home/`, `/var/lib/mysql/`,
`/var/www/html/`). Los datos creados por el servicio en ejecucion son
responsabilidad del administrador.

---

## apt autoremove — Limpiar dependencias huerfanas

Cuando se instala un paquete, sus dependencias se marcan como
**automaticas** (`auto`). Si el paquete padre se elimina, las
dependencias quedan **huerfanas** — nada las necesita.

```
Ejemplo:
  apt install apache2
    apache2     → marcado "manual"
    apache2-bin → marcado "auto"
    libapr1     → marcado "auto"

  apt remove apache2
    apache2     → eliminado
    apache2-bin → huerfano (nadie lo requiere)
    libapr1     → huerfano

  apt autoremove → elimina apache2-bin y libapr1
```

```bash
# Ver que se eliminaria (dry run)
apt autoremove --simulate

# Eliminar huerfanos
sudo apt autoremove

# Eliminar huerfanos + purgar sus configs
sudo apt autoremove --purge

# Ver paquetes marcados como auto
apt-mark showauto | head -10

# Proteger un paquete de autoremove (marcandolo como manual)
sudo apt-mark manual linux-headers-$(uname -r)
```

---

## apt upgrade — Actualizar paquetes (conservador)

```bash
sudo apt update       # primero actualizar indices
sudo apt upgrade
```

`upgrade` es **conservador** — solo actualiza, nunca instala ni elimina:

- Actualiza paquetes a versiones mas nuevas
- **NO** instala paquetes nuevos
- **NO** elimina paquetes existentes
- Si una actualizacion requiere instalar o eliminar otro paquete → **la omite**

---

## apt full-upgrade — Actualizar paquetes (agresivo)

```bash
sudo apt full-upgrade
# Equivalente legacy: sudo apt-get dist-upgrade
```

`full-upgrade` puede:

- Todo lo que hace `upgrade`, mas:
- **Instalar** paquetes nuevos (nuevas dependencias)
- **Eliminar** paquetes en conflicto

### upgrade vs full-upgrade — ejemplo

```
Situacion: nginx 1.22 actualiza a 1.24, pero 1.24 requiere
           libssl3 (nuevo, reemplaza a libssl1.1)

apt upgrade:
  nginx se mantiene en 1.22
  (no puede instalar libssl3 sin eliminar libssl1.1)

apt full-upgrade:
  Instala libssl3, elimina libssl1.1, actualiza nginx a 1.24
```

### Cuando usar cada uno

| Comando | Uso |
|---|---|
| `apt upgrade` | Actualizaciones rutinarias de seguridad (servidores) |
| `apt full-upgrade` | Migracion a nueva release, o cuando `upgrade` deja paquetes sin actualizar |
| `apt-mark hold pkg` + `apt upgrade` | Mantener un paquete especifico en su version actual |

```bash
# Distribution upgrade (ej: bookworm → trixie)
# 1. Editar sources.list cambiando "bookworm" por "trixie"
# 2.
sudo apt update
sudo apt upgrade        # actualiza todo lo que se pueda sin conflictos
sudo apt full-upgrade   # resuelve los que quedaron pendientes
```

---

## Buscar y consultar paquetes

```bash
# Buscar por nombre o descripcion
apt search json parser
apt search "^nginx$"          # expresion regular

# Buscar solo por nombre de paquete (mas preciso)
apt search --names-only "^python3.*$"

# Informacion detallada
apt show nginx

# Solo campos especificos
apt show nginx | grep -E "^Version:|^Depends:|^Description:"
```

```bash
# Que archivos instala un paquete (YA INSTALADO)
dpkg -L nginx
# /usr/sbin/nginx
# /etc/nginx/nginx.conf
# /var/log/nginx/

# A que paquete pertenece un archivo (YA INSTALADO)
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx

# A que paquete pertenece un archivo (NO instalado)
# Requiere: sudo apt install apt-file && sudo apt-file update
apt-file search /usr/bin/curl
# curl: /usr/bin/curl
```

---

## Listar paquetes

```bash
# TODOS los paquetes conocidos
apt list

# Solo instalados
apt list --installed

# Solo actualizables (despues de apt update)
apt list --upgradable

# Todas las versiones disponibles de un paquete
apt list --all-versions nginx

# Contar paquetes instalados
apt list --installed 2>/dev/null | wc -l
```

---

## Descargar sin instalar

```bash
# Descargar .deb al directorio actual
apt download nginx
ls nginx_*.deb

# Descargar con dependencias (no las instala, las deja en cache)
sudo apt install --download-only nginx
ls /var/cache/apt/archives/*.deb

# Ver contenido de un .deb sin instalarlo
dpkg -c nginx_*.deb | head -20
```

---

## Limpiar cache

```bash
# Eliminar TODOS los .deb descargados
sudo apt clean

# Eliminar .deb de paquetes que ya no estan en ningun repositorio
sudo apt autoclean
# NO elimina .deb de paquetes que aun se pueden descargar

# Ver tamano de la cache
du -sh /var/cache/apt/archives/
```

En Dockerfiles **siempre** limpiar para reducir tamano de capa:

```dockerfile
RUN apt-get update && \
    apt-get install -y --no-install-recommends nginx && \
    rm -rf /var/lib/apt/lists/*
```

---

## Operaciones no interactivas (scripts, Docker, CI)

```bash
# DEBIAN_FRONTEND controla como debconf hace preguntas:
#   dialog         → interfaz ncurses (default en terminal)
#   readline       → preguntas en texto plano
#   noninteractive → NO hace preguntas (usa defaults)

# Metodo 1: en una linea
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q nginx

# Metodo 2: exportar variable
export DEBIAN_FRONTEND=noninteractive
sudo apt-get install -y nginx
```

### Flags para automatizacion

| Flag | Efecto |
|---|---|
| `-y` / `--yes` | Responder yes a confirmaciones |
| `-q` / `--quiet` | Reducir output (`-qq` para output minimo) |
| `--no-install-recommends` | No instalar paquetes `Recommends` |
| `-s` / `--simulate` | Dry run — no ejecuta nada |

### Patron para Dockerfiles

```dockerfile
# CORRECTO: una sola capa, usa apt-get, limpia al final
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        curl ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# INCORRECTO: multiples capas, usa apt, no limpia
RUN apt update
RUN apt install curl    # cache de indices persiste en capa anterior
```

Por que `rm -rf /var/lib/apt/lists/*`: elimina los indices descargados
por `apt-get update`, que ya no se necesitan despues de instalar. Reduce
el tamano de la capa Docker significativamente.

---

## Manejo de paquetes rotos

```bash
# Si una instalacion fallo a medias:
# 1. Completar configuraciones pendientes
sudo dpkg --configure -a

# 2. Reparar dependencias rotas
sudo apt --fix-broken install
# Detecta paquetes con status incompleto
# Intenta completar la instalacion/configuracion
# Si falta algo, intenta descargarlo

# 3. Auditar el estado del sistema
dpkg --audit
# o equivalente:
dpkg -C
```

### Estados de un paquete en dpkg

```bash
dpkg -l | head -5
# Primera letra: estado deseado
#   i = install    h = hold    r = remove    p = purge
#
# Segunda letra: estado actual
#   i = installed       n = not-installed
#   c = config-files    U = unpacked (falta configurar)
#   F = half-configured (configuracion fallo)
#   H = half-installed  (instalacion fallo)
```

| Codigo | Significado |
|---|---|
| `ii` | Correctamente instalado |
| `rc` | Eliminado, configuracion presente |
| `pn` | Purgado, no instalado |
| `iU` | Desempaquetado pero no configurado |
| `iF` | Configuracion fallo |
| `iH` | Instalacion fallo a medias |

```bash
# Encontrar paquetes en estado problematico
dpkg -l | awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/ {print}'
```

**Importante**: nunca usar `dpkg --force-all` como primer recurso. Es
destructivo y puede dejar el sistema peor. Solo usar en un snapshot/VM
como ultimo recurso absoluto.

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida completo de un paquete

Instala un paquete, usalo, luego recorre cada estado de dpkg.

```bash
# 1. Actualizar indices
sudo apt-get update -qq

# 2. Instalar
sudo apt-get install -y sl

# 3. Verificar estado
dpkg -l sl | tail -1
```

**Pregunta**: Que prefijo muestra dpkg -l para un paquete instalado correctamente?

<details><summary>Prediccion</summary>

`ii` — primera `i` = desired: install, segunda `i` = status: installed.

```bash
# 4. Ver archivos instalados
dpkg -L sl

# 5. Usar el paquete
/usr/games/sl

# 6. Remove (conserva config)
sudo apt-get remove -y sl
dpkg -l sl | tail -1
# rc → removed, config-files present

# 7. Purge (elimina config)
sudo apt-get purge -y sl
dpkg -l sl | tail -1
# pn → purged, not-installed (o desaparece del listado)

# 8. Limpiar huerfanos
sudo apt-get autoremove -y
```

La secuencia de estados es: `ii` → `rc` → `pn`.

</details>

---

### Ejercicio 2 — remove vs purge con archivos de configuracion

Demuestra la diferencia real entre remove y purge usando un paquete que
tenga archivos en `/etc/`.

```bash
# 1. Instalar nano (tiene config en /etc/)
sudo apt-get install -y nano

# 2. Verificar que tiene archivos en /etc/
dpkg -L nano | grep "^/etc/"

# 3. Remove
sudo apt-get remove -y nano

# 4. Verificar que el binario no existe pero /etc/ si
ls /usr/bin/nano 2>/dev/null
```

**Pregunta**: Despues de `apt remove`, existen aun los archivos de `/etc/` del paquete?

<details><summary>Prediccion</summary>

Si. `remove` elimina binarios pero conserva archivos de configuracion en `/etc/`.

```bash
# Verificar config files sobreviven
dpkg -L nano 2>/dev/null | grep "^/etc/"
dpkg -l nano | tail -1
# rc → config-files present

# 5. Ahora purge
sudo apt-get purge -y nano

# 6. Verificar que /etc/ config ya no existe
dpkg -l nano | tail -1
# pn → purged, not-installed
dpkg -L nano 2>/dev/null | grep "^/etc/"
# (vacio — no quedan archivos)
```

Con `purge`, tanto binarios como configuracion en `/etc/` se eliminan.
Los datos de usuario (en `/home/`, `/var/lib/`, etc.) nunca se tocan.

</details>

---

### Ejercicio 3 — Simular antes de ejecutar

Usa `--simulate` para inspeccionar operaciones sin ejecutarlas.

```bash
# 1. Simular upgrade del sistema
apt-get upgrade --simulate 2>/dev/null | tail -10

# 2. Simular full-upgrade
apt-get dist-upgrade --simulate 2>/dev/null | tail -10
```

**Pregunta**: Necesitas `sudo` para `--simulate`? Y hay diferencia en la
cantidad de paquetes entre `upgrade` y `full-upgrade`?

<details><summary>Prediccion</summary>

No necesitas `sudo` para `--simulate` — no modifica nada en el sistema.

La diferencia: `full-upgrade` puede mostrar mas paquetes porque puede
instalar nuevos y eliminar existentes para resolver conflictos. `upgrade`
omite esos paquetes.

```bash
# 3. Comparar cuantos paquetes actualizaria cada uno
echo "upgrade:"
apt-get upgrade --simulate 2>/dev/null | grep "upgraded" | tail -1
echo "full-upgrade:"
apt-get dist-upgrade --simulate 2>/dev/null | grep "upgraded" | tail -1

# 4. Simular instalacion de un meta-paquete pesado
apt-get install --simulate build-essential 2>/dev/null | grep "^Inst" | wc -l
# Muestra cuantas dependencias se instalarian
```

Si ambos muestran lo mismo, significa que no hay paquetes "retenidos" por
conflictos de dependencias — el sistema esta limpio.

</details>

---

### Ejercicio 4 — Dependencias directas e inversas

Explora el grafo de dependencias de un paquete.

```bash
# 1. Dependencias de curl
apt-cache depends curl
```

**Pregunta**: Cual es la diferencia entre `Depends`, `Recommends` y `Suggests`
en la salida?

<details><summary>Prediccion</summary>

- `Depends`: obligatorio, se instala siempre
- `Recommends`: se instala por defecto pero se puede omitir con `--no-install-recommends`
- `Suggests`: nunca se instala automaticamente, funcionalidad extra opcional

```bash
# 2. Dependencias inversas (quien depende de curl)
apt-cache rdepends curl | head -15

# 3. Dependencias inversas de una libreria critica
apt-cache rdepends libssl3 2>/dev/null | tail -n +2 | wc -l
# Cientos de paquetes dependen de libssl3 — eliminarla romperia todo

# 4. Comparar: paquete con pocas vs muchas reverse depends
echo "=== cowsay ==="
apt-cache rdepends cowsay 2>/dev/null | tail -n +2 | wc -l
echo "=== bash ==="
apt-cache rdepends bash | tail -n +2 | wc -l
```

Los paquetes con muchas reverse depends son **criticos del sistema** —
eliminarlos causaria una cascada de eliminaciones.

</details>

---

### Ejercicio 5 — autoremove y marcas manual/auto

Comprende el sistema de marcas que decide que paquetes son "huerfanos".

```bash
# 1. Ver paquetes marcados como auto
apt-mark showauto | head -10

# 2. Ver paquetes marcados como manual
apt-mark showmanual | head -10
```

**Pregunta**: Si un paquete esta marcado como `auto` y ningun paquete `manual`
depende de el, que pasa con `apt autoremove`?

<details><summary>Prediccion</summary>

`apt autoremove` lo eliminaria — es un huerfano. El sistema funciona asi:

- `apt install pkg` → `pkg` se marca `manual`, sus deps se marcan `auto`
- `apt remove pkg` → `pkg` se elimina, sus deps `auto` quedan huerfanas
- `apt autoremove` → elimina todo lo marcado `auto` que nadie necesita

```bash
# 3. Ver que eliminaria autoremove
apt-get autoremove --simulate 2>/dev/null | grep "^Remv"

# 4. Proteger un paquete de autoremove
# (marcarlo como manual)
# sudo apt-mark manual <package>

# 5. Simular: marcar un paquete como auto
# sudo apt-mark auto <package>
# Ahora autoremove lo consideraria para eliminacion

# 6. Diferencia: autoremove vs autoremove --purge
echo "autoremove:        elimina binarios, conserva configs"
echo "autoremove --purge: elimina binarios Y configs"
```

</details>

---

### Ejercicio 6 — Informacion detallada de un paquete

Investiga un paquete a fondo antes de instalarlo.

```bash
# 1. Informacion general
apt show curl 2>/dev/null | grep -E "^(Package|Version|Installed-Size|Depends|Recommends|Suggests):"

# 2. Todas las versiones disponibles
apt list --all-versions curl 2>/dev/null
```

**Pregunta**: Que diferencia hay entre `apt show` y `dpkg -s`?

<details><summary>Prediccion</summary>

- `apt show`: muestra informacion del indice (funciona para paquetes **no instalados** tambien)
- `dpkg -s`: muestra informacion del estado local (solo para paquetes **instalados**)

```bash
# 3. Probar la diferencia
apt show nginx 2>/dev/null | head -5    # funciona aunque no este instalado
dpkg -s nginx 2>/dev/null | head -5     # falla si no esta instalado

# 4. Que archivos instalaria
# Opcion A: si ya esta instalado
dpkg -L curl | head -15

# Opcion B: sin instalar — descargar y explorar
apt download curl 2>/dev/null
dpkg -c curl_*.deb 2>/dev/null | head -15
rm -f curl_*.deb

# 5. A que paquete pertenece un archivo
dpkg -S /usr/bin/curl
dpkg -S /usr/bin/env
```

</details>

---

### Ejercicio 7 — Cache de paquetes descargados

Explora como funciona la cache de `.deb` y las opciones de limpieza.

```bash
# 1. Ver tamano actual de la cache
du -sh /var/cache/apt/archives/

# 2. Ver cuantos .deb hay
ls /var/cache/apt/archives/*.deb 2>/dev/null | wc -l
```

**Pregunta**: Cual es la diferencia entre `apt clean` y `apt autoclean`?

<details><summary>Prediccion</summary>

- `apt clean`: elimina **todos** los `.deb` de la cache
- `apt autoclean`: elimina solo los `.deb` de paquetes que **ya no estan
  disponibles en ningun repositorio** (versiones obsoletas que ya no se
  pueden descargar)

```bash
# 3. Ver que hay en la cache
ls -lh /var/cache/apt/archives/*.deb 2>/dev/null | tail -5

# 4. Limpiar toda la cache
sudo apt clean

# 5. Verificar que esta vacia
du -sh /var/cache/apt/archives/
ls /var/cache/apt/archives/*.deb 2>/dev/null | wc -l
```

En servidores de produccion, `autoclean` es preferible — mantiene los
`.deb` de paquetes instalados (util para `--reinstall`). En Dockerfiles,
limpiamos con `rm -rf /var/lib/apt/lists/*` (indices, no cache de debs).

</details>

---

### Ejercicio 8 — Patron correcto para Dockerfiles

Analiza buenos y malos patrones de APT en Dockerfiles.

```bash
# Patron A — INCORRECTO (multiples capas)
cat <<'EOF'
RUN apt update
RUN apt install -y curl
RUN apt install -y nginx
EOF

# Patron B — CORRECTO (una capa, apt-get, limpia)
cat <<'EOF'
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        curl nginx && \
    rm -rf /var/lib/apt/lists/*
EOF
```

**Pregunta**: Por que el patron A es malo? Identifica al menos 3 problemas.

<details><summary>Prediccion</summary>

Problemas del patron A:

1. **`apt` en vez de `apt-get`**: `apt` esta disenado para uso interactivo
   y su output/comportamiento puede cambiar entre versiones. `apt-get`
   tiene output estable para scripts.

2. **Capas separadas**: `apt update` crea una capa con los indices. Si
   Docker usa la capa cacheada de `apt update` pero el repositorio cambio,
   los `install` siguientes pueden fallar con paquetes no encontrados.
   Siempre `update && install` en la misma capa.

3. **No limpia**: los indices de `apt update` persisten en la capa,
   aumentando el tamano de la imagen innecesariamente.

4. **No usa `--no-install-recommends`**: instala paquetes `Recommends`
   que raramente se necesitan en un contenedor, inflando la imagen.

5. **No usa `-y`**: sin `-y`, el build se bloquearia esperando
   confirmacion (aunque DEBIAN_FRONTEND=noninteractive evita otros
   prompts, `-y` sigue siendo necesario para la confirmacion de apt).

</details>

---

### Ejercicio 9 — Diagnosticar estado del sistema de paquetes

Verifica la salud del sistema de paquetes.

```bash
# 1. Auditar paquetes
dpkg --audit

# 2. Buscar paquetes en estado no-instalado
dpkg -l | awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/ {print}'
```

**Pregunta**: Que significa un paquete en estado `iU`? Y como se repara?

<details><summary>Prediccion</summary>

`iU` significa:
- `i` = desired: install (se quiere instalado)
- `U` = status: unpacked (desempaquetado pero **no configurado**)

Esto ocurre cuando `dpkg` fue interrumpido entre la extraccion de archivos
y la ejecucion de los scripts de configuracion.

```bash
# 3. Reparar paquetes en estado incompleto
sudo dpkg --configure -a
# Completa la configuracion de todos los paquetes pendientes

# 4. Si hay dependencias rotas
sudo apt --fix-broken install
# Intenta resolver descargando lo que falta

# 5. Verificar que todo esta limpio
dpkg --audit
# (sin output = todo ok)

dpkg -l | awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/ {print}' | wc -l
# 0 = ningun paquete en estado problematico
```

El flujo de reparacion es: `dpkg --configure -a` → `apt --fix-broken install`
→ `dpkg --audit`. Si nada funciona, investigar logs en `/var/log/dpkg.log`.

</details>

---

### Ejercicio 10 — Panorama: flujo completo de operaciones APT

Reconstruye el mapa mental completo de operaciones APT.

```bash
# 1. Recorre el flujo completo mentalmente:
cat <<'EOF'
FLUJO DE VIDA DE UN PAQUETE:

  apt update          Sincronizar indices
       |
  apt install pkg     Instalar (deps marcadas auto)
       |
  apt upgrade         Actualizar a nueva version
       |              (conservador: no instala/elimina)
  apt full-upgrade    Actualizar agresivamente
       |              (puede instalar/eliminar)
  apt remove pkg      Eliminar binarios (config queda, estado rc)
       |
  apt purge pkg       Eliminar binarios + config (estado pn)
       |
  apt autoremove      Eliminar deps huerfanas (marcadas auto)
       |
  apt clean           Limpiar cache de .deb
EOF
```

**Pregunta**: En que se diferencia el flujo para un servidor de produccion
vs un Dockerfile?

<details><summary>Prediccion</summary>

**Servidor de produccion:**
```
apt update → apt upgrade (conservador, rutinario)
                → apt full-upgrade (solo para releases)
apt autoremove (periodico)
apt autoclean  (mantiene .deb de paquetes actuales)
```

**Dockerfile:**
```
apt-get update && apt-get install -y --no-install-recommends ... && rm -rf /var/lib/apt/lists/*
```

Diferencias clave:

| Aspecto | Servidor | Dockerfile |
|---|---|---|
| Comando | `apt` (interactivo) | `apt-get` (estable) |
| Indices | Se mantienen (`apt update` periodico) | Se eliminan (`rm -rf /var/lib/apt/lists/*`) |
| Recommends | Se instalan (funcionalidad completa) | Se omiten (`--no-install-recommends`) |
| Cache .deb | `autoclean` (mantiene utiles) | No aplica (se limpia en misma capa) |
| Upgrade | `upgrade` rutinario + `full-upgrade` para releases | No se hace — se reconstruye la imagen |
| `autoremove` | Periodico | No se usa — se controla que se instala |
| DEBIAN_FRONTEND | No necesario (hay terminal) | `noninteractive` obligatorio |

En Docker, el "upgrade" se hace **reconstruyendo la imagen** (`docker build
--no-cache`), no ejecutando `apt upgrade` dentro del contenedor.

</details>
