# T04 — dpkg

## Errata respecto a las fuentes originales

1. **`F` como desired state "Failed"** (README.max.md): no existe. Los
   desired states son solo: `u` (Unknown), `i` (Install), `h` (Hold),
   `r` (Remove), `p` (Purge). `F` aparece solo como status (half-configured).
2. **`uU` descrito como "estado roto"** (README.max.md): `u` en desired
   significa Unknown, no es problematico per se. La combinacion rota
   seria `iU` (desired: install, status: unpacked).
3. **`?i` como estado valido** (README.max.md): `?` no es un codigo de
   desired state en dpkg.
4. **`Binary: nginx-core...` en output de `dpkg -I`** (README.max.md):
   ese campo pertenece a paquetes fuente (`.dsc`), no a `.deb` binarios.
   `dpkg -I` muestra Package, Version, Architecture, Depends, Description.
5. **`ar` como "equivalente a tar but binary"** (README.max.md): son
   formatos completamente distintos. `ar` es un archivador simple (usado
   tambien para librerias estaticas `.a`); `tar` es tape archive con
   preservacion de metadatos. Un `.deb` usa `ar` como contenedor externo.
6. **Formato de verificacion `dpkg -V`** (README.max.md): describe las
   posiciones de forma confusa y mezcla campos. "Posicion 9: u=unmerged,
   g=merged" es inventado — no existe en dpkg.
7. **`grep 'c$'` para filtrar conffiles** (README.max.md): `c` no esta
   al final de linea. El formato es `??5?????? c /path/to/file`, asi que
   el filtro correcto es `grep ' c '` o `grep ' c /'`.
8. **regex `[i-uln]`** (README.max.md): crea rango `i-u` demasiado
   amplio. Correcto: `awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/'`.
9. **`fué` con acento** (README.max.md): "fue" es monosilabo, no lleva
   tilde.

---

## Que es dpkg

`dpkg` (Debian Package) es la herramienta de **bajo nivel** que maneja
la instalacion real de paquetes `.deb`. APT es la capa de alto nivel:

```
apt install nginx
  |
  +--[1] apt resuelve dependencias → necesita nginx-common, libssl3, etc.
  +--[2] apt descarga .deb de repositorios
  +--[3] apt llama a dpkg para instalar cada .deb
              dpkg -i nginx-common_1.22.1-9_all.deb
              dpkg -i nginx_1.22.1-9_amd64.deb
```

```
+-----------------------------------------------------+
|                    apt (capa alta)                   |
|   update, install, remove, upgrade, search...       |
|   Resuelve dependencias, descarga de repositorios   |
+--------------------------+--------------------------+
|                   dpkg (capa baja)                  |
|   -i (install), -r (remove), -P (purge)            |
|   Solo instala .deb que le pasen                    |
|   NO resuelve dependencias                          |
+-----------------------------------------------------+
```

**Diferencia critica**: si `libfoo` no esta instalado y ejecutas
`dpkg -i nginx.deb`, falla. APT resuelve deps automaticamente antes
de llamar a dpkg.

### Cuando usar dpkg directamente

- Instalar un `.deb` descargado manualmente (sin repo)
- Consultar informacion de paquetes instalados (`-l`, `-L`, `-S`, `-s`)
- Verificar integridad de archivos (`-V`)
- Extraer `.deb` sin instalar (auditoria, analisis)
- Reparar instalaciones rotas (`--configure -a`)

---

## Estructura interna de dpkg

```
/var/lib/dpkg/
+-- status           <- estado actual de TODOS los paquetes
+-- available        <- info de paquetes disponibles
+-- info/            <- metadatos por paquete
|   +-- bash.list        archivos instalados
|   +-- bash.md5sums     checksums para verificacion
|   +-- bash.conffiles   archivos de configuracion
|   +-- nginx.postinst   script post-instalacion
|   +-- nginx.preinst    script pre-instalacion
|   +-- nginx.prerm      script pre-desinstalacion
|   +-- nginx.postrm     script post-desinstalacion
+-- diversions       <- archivos desviados (dpkg-divert)
+-- alternatives/    <- sistema de alternatives (update-alternatives)
+-- lock-frontend    <- lock file (previene instalaciones simultaneas)
```

El archivo `status` es la "base de datos" de dpkg — contiene el estado
de cada paquete en formato texto. Es lo que consultan `dpkg -l`, `dpkg -s`,
etc.

---

## Codigos de estado en dpkg -l

```bash
dpkg -l
# Desired=Unknown/Install/Remove/Purge/Hold
# | Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
# ||/ Name  Version  Architecture  Description
# ii  bash  5.2-15   amd64        GNU Bourne Again SHell
```

### Primera letra: estado deseado (desired)

| Codigo | Significado |
|---|---|
| `u` | Unknown — estado desconocido |
| `i` | Install — se desea instalar |
| `h` | Hold — mantener en version actual |
| `r` | Remove — se desea desinstalar |
| `p` | Purge — se desea purgar |

### Segunda letra: estado actual (status)

| Codigo | Significado |
|---|---|
| `n` | Not-installed |
| `i` | Installed — instalado correctamente |
| `c` | Config-files — desinstalado, configs permanecen |
| `U` | Unpacked — descomprimido pero no configurado |
| `F` | half-conFigured — configuracion fallo |
| `H` | Half-installed — instalacion fallo a medias |
| `W` | triggers-aWait — esperando trigger |
| `t` | triggers-pending |

### Combinaciones comunes

| Codigo | Significado | Accion tipica |
|---|---|---|
| `ii` | Correctamente instalado | Ninguna |
| `rc` | Removido, config permanece | `apt purge` si quieres limpiar |
| `pn` | Purgado completamente | Ninguna |
| `hi` | En hold, instalado | `apt-mark unhold` para permitir updates |
| `iU` | Desempaquetado, no configurado | `dpkg --configure -a` |
| `iF` | Configuracion fallo | `dpkg --configure -a` o `apt -f install` |
| `iH` | Instalacion fallo a medias | `apt -f install` |

---

## Instalar un .deb

```bash
# Instalacion directa (NO resuelve dependencias)
sudo dpkg -i package.deb

# Si falla por dependencias:
# dpkg: dependency problems prevent configuration of nginx:
#          nginx depends on libssl3 (>= 3.0); however:
#           Package libssl3 is not installed.

# Solucion 1: apt resuelve las deps del .deb parcialmente instalado
sudo apt install -f

# Solucion 2 (MEJOR): usar apt directamente con el .deb
sudo apt install ./package.deb
# apt ./ = "instala desde archivo local, resuelve deps desde repos"
# El ./ es OBLIGATORIO: sin el, apt busca en repositorios por nombre
```

---

## Desinstalar con dpkg

```bash
# Remove (conserva configuracion)
sudo dpkg -r package

# Purge (elimina todo incluyendo configs)
sudo dpkg -P package
```

**Advertencia**: dpkg **no verifica dependencias inversas**. Si otro
paquete depende de este, dpkg advierte pero no lo impide. Usar
`apt remove`/`apt purge` es mas seguro para desinstalacion.

---

## Consultar paquetes instalados

### dpkg -l — Listar paquetes

```bash
# Todos los paquetes
dpkg -l

# Filtrar solo instalados correctamente
dpkg -l | grep '^ii'

# Buscar un paquete especifico
dpkg -l nginx
dpkg -l 'nginx*'       # wildcards

# Solo nombres (formato simple)
dpkg --get-selections | grep nginx
# nginx    install

# Contar paquetes instalados
dpkg -l | grep '^ii' | wc -l
```

### dpkg -L — Archivos de un paquete instalado

```bash
dpkg -L nginx
# /etc/nginx/nginx.conf
# /usr/sbin/nginx
# /usr/share/nginx/html/index.html
# /var/log/nginx/

# Solo binarios
dpkg -L nginx | grep -E '/bin/|/sbin/'

# Solo archivos de configuracion
dpkg -L nginx | grep '^/etc/'
```

### dpkg -S — A que paquete pertenece un archivo

```bash
# Ruta exacta
dpkg -S /usr/sbin/nginx
# nginx-core: /usr/sbin/nginx

dpkg -S /usr/bin/curl
# curl: /usr/bin/curl

# Patron glob
dpkg -S '*nginx.conf*'
# nginx-common: /etc/nginx/nginx.conf

# Archivo no gestionado por dpkg
dpkg -S /usr/local/bin/my-script
# dpkg-query: no path found matching pattern
# (fue creado manualmente, no via package manager)
```

### dpkg -s — Informacion detallada

```bash
dpkg -s nginx
# Package: nginx
# Status: install ok installed
# Priority: optional
# Section: httpd
# Installed-Size: 614
# Architecture: amd64
# Version: 1.22.1-9
# Depends: nginx-common (= 1.22.1-9), libc6 (>= 2.28)
# Description: small, powerful, scalable web/proxy server

# Solo campos especificos
dpkg -s nginx | grep -E "^Version:|^Depends:|^Status:"
```

---

## Consultar archivos .deb (sin instalar)

```bash
# dpkg -I: informacion del .deb (metadata)
dpkg -I package.deb
dpkg --info package.deb
# Package: nginx
# Version: 1.22.1-9
# Architecture: amd64
# Depends: nginx-common, libc6
# Description: ...

# dpkg -c: listar archivos dentro del .deb
dpkg -c package.deb
dpkg --contents package.deb
# drwxr-xr-x root/root    0 2023-06-01  etc/nginx/
# -rw-r--r-- root/root 1234 2023-06-01  etc/nginx/nginx.conf
# -rwxr-xr-x root/root 8676 2023-06-01  usr/sbin/nginx
```

---

## Extraer un .deb sin instalar

### Metodo con dpkg

```bash
# Extraer contenido (data.tar) a un directorio
dpkg -x package.deb /tmp/extracted/
ls /tmp/extracted/
# etc/  usr/  var/

# Extraer scripts de control (preinst, postinst, prerm, postrm)
dpkg -e package.deb /tmp/control/
ls /tmp/control/
# control      metadata del paquete
# conffiles    lista de archivos de configuracion
# postinst     script post-instalacion
# prerm        script pre-desinstalacion
# md5sums      checksums
```

### Metodo manual (anatomia de un .deb)

Un `.deb` es un archivo `ar` (formato archivador simple, el mismo que
usa `libc.a` y otras librerias estaticas):

```bash
ar x package.deb
ls
# debian-binary     <- version del formato (2.0)
# control.tar.xz    <- metadatos y scripts de control
# data.tar.xz       <- los archivos reales del paquete

cat debian-binary
# 2.0

# Ver contenido de los scripts de control
tar -tf control.tar.xz
# ./control
# ./postinst
# ./prerm
# ./md5sums

# Ver que archivos instalaria
tar -tf data.tar.xz | head -20
```

**Auditoria**: antes de instalar un `.deb` de fuentes no oficiales,
extraer y revisar los scripts `postinst`/`preinst` es buena practica
de seguridad.

---

## Verificar integridad de archivos

```bash
# Verificar que los archivos de un paquete no fueron modificados
dpkg -V nginx
# (sin output = todo OK — todos los archivos coinciden con el original)

# Si hay diferencias:
# ??5?????? c /etc/nginx/nginx.conf
```

### Formato de verificacion

`dpkg -V` muestra una cadena de 9 caracteres + marca de conffile + ruta:

```
??5?????? c /etc/nginx/nginx.conf
|||||||||   |
123456789   c = archivo de configuracion (conffile)
```

Cada posicion indica un tipo de verificacion:

| Posicion | Letra | Significado |
|---|---|---|
| 1 | `S` | Size (tamano cambio) |
| 2 | `M` | Mode (permisos cambiaron) |
| 3 | `5` | MD5 (contenido modificado) |
| 4 | `D` | Device (dispositivo cambio) |
| 5 | `L` | Link (enlace cambio) |
| 6 | `U` | User (propietario cambio) |
| 7 | `G` | Group (grupo cambio) |
| 8 | `T` | Time (mtime cambio) |
| 9 | `P` | caPabilities |

En cada posicion: `.` = test paso OK, `?` = test no realizado, la letra
mayuscula = cambio detectado.

```bash
# Verificar TODOS los paquetes (lento)
sudo dpkg -V

# Filtrar solo conffiles modificados (normal)
sudo dpkg -V | grep ' c '

# Filtrar binarios modificados (alerta de seguridad!)
sudo dpkg -V | grep -v ' c '
# Si un binario fue modificado sin reinstalacion, investigar

# Verificar un paquete especifico
dpkg -V bash
dpkg -V coreutils
```

---

## Reparar problemas de dpkg

### Paquetes a medio instalar

```bash
# dpkg se interrumpio (apagon, ctrl+c, kill)
# Estado tipico: paquetes en "Unpacked" (iU) o "Half-configured" (iF)

# Configurar todos los paquetes pendientes
sudo dpkg --configure -a

# Verificar que esta mal
dpkg --audit
# o equivalente:
dpkg -C
```

### Dependencias faltantes

```bash
# dpkg: dependency problems prevent configuration of package
sudo apt install -f
# apt detecta el .deb parcialmente instalado y resuelve sus deps
```

### Lock files bloqueados

```
E: Could not get lock /var/lib/dpkg/lock-frontend.
```

```bash
# 1. Verificar que no hay apt/dpkg corriendo
ps aux | grep -E 'apt|dpkg'

# 2. Si hay procesos activos: ESPERAR a que terminen

# 3. Si no hay procesos, verificar con lsof
sudo lsof /var/lib/dpkg/lock-frontend
sudo lsof /var/lib/dpkg/lock

# 4. Solo si estas SEGURO de que no hay apt/dpkg activo:
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo dpkg --configure -a
```

### Encontrar paquetes en estado problematico

```bash
# Paquetes que no estan en estado normal (ii, rc, pn)
dpkg -l | awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/ {print}'
```

---

## dpkg-query — Consultas avanzadas

```bash
# Formato personalizado de salida
dpkg-query -W -f='${Package} ${Version} ${Installed-Size}\n' nginx
# nginx 1.22.1-9 614

# Los 10 paquetes mas grandes (en KB)
dpkg-query -W -f='${Installed-Size}\t${Package}\n' | sort -rn | head -10

# Listar paquetes instalados con formato especifico
dpkg-query -W -f='${Package}\t${Status}\n' | grep 'installed'

# Buscar paquetes por patron
dpkg-query -l '*python3*'
dpkg-query -l 'nginx*'
```

---

## Ejercicios

### Ejercicio 1 — dpkg en la jerarquia APT

Ubica dpkg en la jerarquia de herramientas de gestion de paquetes.

```bash
# 1. Instalar un paquete con apt y observar que llama a dpkg
sudo apt-get install -y sl 2>&1 | grep -i dpkg
```

**Pregunta**: Si `dpkg -i package.deb` falla por dependencias, que dos
formas tienes de resolver las dependencias?

<details><summary>Prediccion</summary>

Dos formas:

1. `sudo apt install -f` — apt detecta el `.deb` parcialmente instalado
   y descarga/instala las dependencias faltantes
2. `sudo apt install ./package.deb` — apt resuelve dependencias ANTES
   de instalar (mejor opcion, previene el estado roto)

La diferencia: con `dpkg -i` + `apt -f install` el paquete queda
temporalmente en estado `iU` (unpacked, sin configurar). Con
`apt install ./` nunca llega a ese estado roto.

```bash
# 2. Demostrar la diferencia
apt-get download sl 2>/dev/null
dpkg -s sl | grep Status
# Status: install ok installed  (porque lo instalamos con apt arriba)

# 3. Ver la jerarquia
echo "dpkg -i:  instala .deb, NO resuelve deps"
echo "apt -f:   resuelve deps de paquetes parcialmente instalados"
echo "apt ./x:  resuelve deps ANTES de instalar (mejor)"

# Limpiar
sudo apt-get purge -y sl 2>/dev/null
rm -f sl_*.deb
```

</details>

---

### Ejercicio 2 — Consultas basicas: -l, -L, -S, -s

Domina los cuatro flags de consulta de dpkg.

```bash
# 1. Listar todos los paquetes instalados y contar
dpkg -l | grep '^ii' | wc -l

# 2. Buscar un paquete especifico
dpkg -l bash
```

**Pregunta**: Cual es la diferencia entre `dpkg -s bash` y `dpkg -l bash`?

<details><summary>Prediccion</summary>

- `dpkg -l bash`: muestra una linea resumida con codigo de estado
  (`ii`), nombre, version, arquitectura y descripcion breve
- `dpkg -s bash`: muestra informacion completa del paquete — Status,
  Version, Depends, Installed-Size, Description larga, Maintainer, etc.

```bash
# 3. Comparar
dpkg -l bash | tail -1
dpkg -s bash | head -15

# 4. Ver archivos instalados por bash
dpkg -L bash | head -10

# 5. Buscar solo binarios
dpkg -L bash | grep -E '/bin/|/sbin/'

# 6. Buscar dueno de archivos
dpkg -S /usr/bin/env
dpkg -S /usr/bin/find
dpkg -S /etc/passwd

# 7. Buscar archivo no gestionado por dpkg
dpkg -S /usr/local/bin/algo 2>&1
# dpkg-query: no path found → fue creado manualmente
```

Resumen de los 4 flags:
- `-l`: listar paquetes (summary)
- `-L`: listar archivos de un paquete
- `-s`: status/info detallada
- `-S`: buscar que paquete instalo un archivo (Search)

</details>

---

### Ejercicio 3 — Anatomia de un .deb

Descarga, inspecciona y extrae un `.deb` sin instalarlo.

```bash
# 1. Descargar sin instalar
cd /tmp
apt-get download cowsay 2>/dev/null
ls cowsay*.deb
```

**Pregunta**: Cuales son los 3 componentes internos de un archivo `.deb`?

<details><summary>Prediccion</summary>

Un `.deb` es un archivo `ar` que contiene 3 partes:
1. `debian-binary` — version del formato (normalmente "2.0")
2. `control.tar.xz` — metadatos y scripts (postinst, prerm, etc.)
3. `data.tar.xz` — los archivos reales del paquete

```bash
# 2. Inspeccionar sin instalar
dpkg -I cowsay*.deb     # metadata
dpkg -c cowsay*.deb     # listar archivos

# 3. Extraer contenido
mkdir -p /tmp/cowsay-data /tmp/cowsay-control
dpkg -x cowsay*.deb /tmp/cowsay-data/
dpkg -e cowsay*.deb /tmp/cowsay-control/

echo "=== Archivos del paquete ==="
find /tmp/cowsay-data -type f | head -10

echo "=== Scripts de control ==="
ls /tmp/cowsay-control/

# 4. Descomposicion manual con ar
mkdir -p /tmp/cowsay-ar && cd /tmp/cowsay-ar
ar x /tmp/cowsay*.deb
ls -la
cat debian-binary

# 5. Limpiar
cd /tmp
rm -rf cowsay*.deb cowsay-data cowsay-control cowsay-ar
```

</details>

---

### Ejercicio 4 — Verificar integridad con dpkg -V

Usa `dpkg -V` para detectar archivos modificados.

```bash
# 1. Verificar un paquete del sistema
dpkg -V bash
```

**Pregunta**: Si `dpkg -V` muestra `??5?????? c /etc/nginx/nginx.conf`,
que significa cada parte?

<details><summary>Prediccion</summary>

- `??5??????`: cadena de 9 posiciones de verificacion
  - Posiciones 1,2 (`??`): Size y Mode no verificados
  - Posicion 3 (`5`): MD5 checksum **cambio** — el contenido fue modificado
  - Posiciones 4-9 (`??????`): Device, Link, User, Group, Time, Capabilities no verificados
- `c`: es un **conffile** (archivo de configuracion)
- `/etc/nginx/nginx.conf`: ruta del archivo

Que esto sea un conffile (`c`) es **normal** — el administrador lo edito.
Si un binario (sin `c`) aparece modificado, es alerta de seguridad.

```bash
# 2. Verificar varios paquetes
dpkg -V coreutils
# (sin output = todo OK)

# 3. Buscar archivos de configuracion modificados (normal)
sudo dpkg -V 2>/dev/null | grep ' c ' | head -5

# 4. Buscar binarios modificados (alerta!)
sudo dpkg -V 2>/dev/null | grep -v ' c ' | head -5
# Si hay output aqui, investigar — podria indicar
# compromiso del sistema o instalacion corrupta
```

</details>

---

### Ejercicio 5 — Maintainer scripts (postinst, prerm)

Examina los scripts que dpkg ejecuta durante instalacion/desinstalacion.

```bash
# 1. Ver los scripts de un paquete instalado
ls /var/lib/dpkg/info/bash.*
```

**Pregunta**: En que orden ejecuta dpkg los 4 maintainer scripts durante
install y remove?

<details><summary>Prediccion</summary>

**Durante instalacion** (`dpkg -i`):
1. `preinst` — antes de desempaquetar (preparar el terreno)
2. (dpkg desempaqueta los archivos)
3. `postinst` — despues de instalar (configurar servicio, etc.)

**Durante desinstalacion** (`dpkg -r`):
1. `prerm` — antes de eliminar (detener servicio)
2. (dpkg elimina los archivos)
3. `postrm` — despues de eliminar (limpiar estado)

```bash
# 2. Leer los scripts de bash
cat /var/lib/dpkg/info/bash.postinst 2>/dev/null | head -20
cat /var/lib/dpkg/info/bash.prerm 2>/dev/null | head -20

# 3. Ver otros metadatos
cat /var/lib/dpkg/info/bash.conffiles 2>/dev/null
# Lista de archivos de configuracion — dpkg los protege en upgrades

head -5 /var/lib/dpkg/info/bash.md5sums
# Checksums para verificacion (dpkg -V los usa)

wc -l /var/lib/dpkg/info/bash.list
# Numero de archivos instalados por bash
```

Los `conffiles` son especiales: si dpkg detecta que el admin los edito,
pregunta durante un upgrade si mantener la version del admin o usar la
nueva del paquete.

</details>

---

### Ejercicio 6 — dpkg-query: consultas con formato

Usa `dpkg-query` para consultas avanzadas con formato personalizado.

```bash
# 1. Los 10 paquetes mas grandes
dpkg-query -W -f='${Installed-Size}\t${Package}\n' | sort -rn | head -10
```

**Pregunta**: Cual es la diferencia entre `dpkg -l` y `dpkg-query -W`?

<details><summary>Prediccion</summary>

- `dpkg -l`: salida formateada con header y columnas fijas (humano)
- `dpkg-query -W`: salida programatica, acepta formato personalizado con
  `-f` (scripts)

`dpkg -l` es en realidad un wrapper que llama a `dpkg-query -l`.

```bash
# 2. Comparar
dpkg -l bash | tail -1
dpkg-query -W -f='${Package} ${Version} ${Installed-Size}KB\n' bash

# 3. Listar con formato personalizado
dpkg-query -W -f='${Package}\t${Version}\n' | head -10

# 4. Buscar por patron
dpkg-query -l '*python3*' 2>/dev/null | grep '^ii' | head -5

# 5. Solo paquetes de un tamano mayor a X
dpkg-query -W -f='${Installed-Size}\t${Package}\n' | \
    awk '$1 > 10000 {print}' | sort -rn
```

</details>

---

### Ejercicio 7 — Base de datos de dpkg

Explora la base de datos en `/var/lib/dpkg/`.

```bash
# 1. Estructura del directorio
ls /var/lib/dpkg/

# 2. Tamano total
du -sh /var/lib/dpkg/
```

**Pregunta**: Que archivo contiene el estado de todos los paquetes y en
que formato esta?

<details><summary>Prediccion</summary>

`/var/lib/dpkg/status` — es un archivo de texto plano con formato
similar a los headers de email (RFC 822). Cada paquete es un bloque
separado por una linea en blanco.

```bash
# 3. Ver un bloque del archivo status
grep -A 10 "^Package: bash$" /var/lib/dpkg/status

# 4. Contar paquetes en la base de datos
grep "^Package:" /var/lib/dpkg/status | wc -l

# 5. Ver metadatos individuales de un paquete
ls /var/lib/dpkg/info/bash.*
# .list      archivos instalados (lo que muestra dpkg -L)
# .md5sums   checksums (lo que usa dpkg -V)
# .conffiles archivos de config (protegidos en upgrades)

# 6. Comparar: dpkg -L vs .list
dpkg -L bash | head -5
head -5 /var/lib/dpkg/info/bash.list
# Son la misma informacion
```

</details>

---

### Ejercicio 8 — Resolver un paquete roto

Simula y repara un estado de paquete inconsistente.

```bash
# 1. Descargar un .deb
cd /tmp
apt-get download sl 2>/dev/null

# 2. Instalar con dpkg (puede fallar por deps)
sudo dpkg -i sl_*.deb 2>&1
```

**Pregunta**: Despues de un `dpkg -i` que falla por dependencias, en que
estado queda el paquete? Y como se repara?

<details><summary>Prediccion</summary>

El paquete queda en estado `iU` (desired: install, status: Unpacked) o
`iF` (desired: install, status: half-configured). Los archivos se
extrajeron pero dpkg no completo la configuracion.

```bash
# 3. Ver el estado
dpkg -l sl | tail -1
# iU o iF dependiendo de donde fallo

# 4. Reparar con apt
sudo apt install -f -y
# apt detecta el paquete parcialmente instalado,
# descarga las dependencias faltantes, y completa la configuracion

# 5. Verificar que ahora esta ii
dpkg -l sl | tail -1
# ii  sl  ...

# 6. Limpiar
sudo apt-get purge -y sl 2>/dev/null
sudo apt-get autoremove -y 2>/dev/null
rm -f /tmp/sl_*.deb
```

Flujo de reparacion:
1. `dpkg --configure -a` — completa configuraciones pendientes
2. `apt install -f` — resuelve dependencias rotas
3. `dpkg --audit` — verifica que todo esta limpio

</details>

---

### Ejercicio 9 — Auditoria de seguridad con dpkg

Usa dpkg para verificar la integridad del sistema.

```bash
# 1. Verificar paquetes criticos
dpkg -V bash
dpkg -V coreutils
dpkg -V login
```

**Pregunta**: Por que un binario modificado (sin marca `c`) es una alerta
de seguridad, pero un conffile modificado no lo es?

<details><summary>Prediccion</summary>

- **Conffiles** (`c`): archivos en `/etc/` que el administrador edita
  intencionalmente (configuracion de servicios). Es **esperado** que
  cambien — dpkg los marca como conffiles precisamente para protegerlos
  durante upgrades.

- **Binarios**: archivos en `/usr/bin/`, `/usr/sbin/`, librerias en
  `/usr/lib/`. Estos **nunca deberian cambiar** fuera de un
  upgrade/reinstall. Si cambian, podria indicar:
  - Compromiso del sistema (malware reemplazo un binario)
  - Instalacion corrupta
  - Modificacion manual accidental

```bash
# 2. Verificar todos los paquetes y separar
echo "=== Conffiles modificados (normal) ==="
sudo dpkg -V 2>/dev/null | grep ' c ' | head -5

echo "=== Archivos NO-config modificados (investigar!) ==="
sudo dpkg -V 2>/dev/null | grep -v ' c ' | head -5

# 3. Buscar paquetes en estado anomalo
dpkg -l | awk '/^[a-z]/ && !/^ii/ && !/^rc/ && !/^pn/ {print}'

# 4. Estado de la base de datos
dpkg --audit
# (sin output = sistema limpio)
```

En un servidor de produccion, ejecutar `dpkg -V` periodicamente y
alertar si binarios cambian es una practica basica de seguridad
(complementaria a herramientas como AIDE o tripwire).

</details>

---

### Ejercicio 10 — Panorama: dpkg en el ecosistema APT

Reconstruye el mapa completo de herramientas de gestion de paquetes.

```bash
cat <<'EOF'
JERARQUIA COMPLETA:

  apt / apt-get / apt-cache        ← Alto nivel (usuario)
    |   Resuelve dependencias
    |   Descarga de repositorios
    |   Interfaz amigable
    |
  dpkg                             ← Bajo nivel (sistema)
    |   Instala/remove .deb individuales
    |   Consulta estado de paquetes
    |   Verifica integridad
    |   NO resuelve dependencias
    |
  archivos .deb                    ← Formato de paquete
      ar archive con:
      debian-binary + control.tar + data.tar
EOF
```

**Pregunta**: Cuando usarias `dpkg` directamente en vez de `apt`?
Identifica al menos 4 escenarios.

<details><summary>Prediccion</summary>

Escenarios donde dpkg es necesario o preferible:

1. **Instalar `.deb` descargado manualmente** — cuando el paquete no
   esta en ningun repositorio (`dpkg -i` o mejor `apt install ./`)

2. **Consultar que paquete instalo un archivo** — `dpkg -S /path/to/file`
   (apt no tiene equivalente directo para archivos ya instalados)

3. **Verificar integridad del sistema** — `dpkg -V` compara archivos
   instalados contra checksums originales (auditoria de seguridad)

4. **Inspeccionar un `.deb` antes de instalar** — `dpkg -I` (metadata),
   `dpkg -c` (archivos), `dpkg -e` (scripts de control) para auditar
   paquetes de fuentes no confiables

5. **Reparar instalaciones rotas** — `dpkg --configure -a` completa
   configuraciones interrumpidas

6. **Consultas avanzadas** — `dpkg-query -W -f` para formato
   personalizado (top paquetes por tamano, filtros especificos)

7. **Extraer archivos sin instalar** — `dpkg -x` para obtener un
   binario o config de un `.deb` sin alterar el sistema

```bash
# Resumen de flags de dpkg
echo "CONSULTA:"
echo "  -l          listar paquetes"
echo "  -L pkg      archivos de un paquete"
echo "  -S file     buscar dueno de un archivo"
echo "  -s pkg      info detallada"
echo ""
echo "ARCHIVO .deb:"
echo "  -I file.deb info del .deb"
echo "  -c file.deb listar contenido"
echo "  -x file.deb extraer contenido"
echo "  -e file.deb extraer scripts de control"
echo ""
echo "ACCION:"
echo "  -i file.deb instalar"
echo "  -r pkg      remove"
echo "  -P pkg      purge"
echo "  -V pkg      verificar integridad"
echo "  --configure -a  completar configuraciones"
```

</details>
