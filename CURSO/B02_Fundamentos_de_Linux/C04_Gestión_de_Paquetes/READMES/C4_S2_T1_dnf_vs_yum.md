# T01 — dnf vs yum

## Errata respecto a las fuentes originales

1. **`Performance可怜的`** y **`si带宽 es alta`** (README.max.md): caracteres
   chinos intercalados en el texto. `可怜的` = "pobre", `带宽` = "ancho de
   banda".
2. **"Delta RPMs: descarga solo diferencias de metadata"** (README.max.md y
   README.md): incorrecto. Delta RPMs (`drpm`) son diferencias binarias de
   los **paquetes** mismos (no metadata). La optimizacion de metadata en dnf
   es cache con expiracion (`metadata_expire`, default 48h en RHEL).
3. **`yum-plugin-auto-update`** (README.max.md): no existe. El autoremove en
   yum es built-in desde RHEL 7.2 o via `yum-plugin-remove-with-leaves`.
   `auto-update` es un concepto distinto (actualizaciones automaticas).
4. **`yum resolvedep python3`** (README.max.md): no es un comando estandar de
   yum. Lo correcto es `yum provides python3` o `yum whatprovides python3`.
5. **`installonly_limit=3 (2 = actuales + 1 anterior)`** (README.max.md):
   confuso. Con limit=3 se mantienen 3 kernels: el actual + 2 anteriores.
6. **`https://example.com.repo`** (README.max.md): parece un dominio con TLD
   `.repo`. Deberia ser `https://example.com/something.repo`.

---

## Evolucion de la familia RPM

```
+-------------------------------------------------------------+
|                        rpm (1997)                            |
|   Gestor de bajo nivel: instala .rpm individuales            |
|   NO resuelve dependencias (como dpkg en Debian)             |
+-------------------------------------------------------------+
|                        yum (2003)                            |
|   Gestor de alto nivel: resuelve deps, repositorios          |
|   Python 2, resolver propio (lento, a veces inconsistente)   |
|   RHEL 5/6/7, CentOS 5/6/7                                  |
+-------------------------------------------------------------+
|                        dnf (2015)                            |
|   Reemplazo de yum: misma interfaz, mejor rendimiento        |
|   Python 3, libsolv para deps (SAT solver)                   |
|   Fedora 22+, RHEL 8+, AlmaLinux 8+, Rocky 8+               |
+-------------------------------------------------------------+
|                     dnf5 (2024+)                             |
|   Reescritura en C++ para maximo rendimiento                 |
|   Fedora 39+, disponible en RHEL 9+                          |
+-------------------------------------------------------------+
```

La relacion `rpm` ↔ `dnf` es la misma que `dpkg` ↔ `apt` en Debian:
herramienta de bajo nivel que no resuelve dependencias + herramienta de
alto nivel que si lo hace.

---

## Por que dnf reemplazo a yum

### Problemas de yum

1. **Resolver de dependencias inconsistente**: podia reportar "no hay
   solucion" cuando si existia una, o instalar versiones incorrectas
2. **Rendimiento deficiente** con muchos repositorios: descargaba
   metadata completa cada vez
3. **Python 2 EOL**: riesgo de seguridad al depender de un runtime
   sin soporte

### Que mejoro con dnf

1. **libsolv** (mismo solver que zypper en SUSE): es un solver SAT que
   **garantiza** encontrar la solucion si existe, o reportar
   correctamente que no hay solucion posible
2. **Cache de metadata inteligente**: no re-descarga metadata hasta que
   expire (`metadata_expire`, default 48h en RHEL)
3. **Descargas paralelas**: multiples paquetes simultaneamente
   (`max_parallel_downloads`)
4. **API de plugins estandarizada**

### Como funciona libsolv

```
Problema: Instalar paquete A que depende de B y C
          B depende de D >= 2.0
          C depende de D <= 3.0

libsolv analiza:
  - Todas las restricciones como un problema SAT
  - Encuentra la version de D que satisface TODOS (2.0 <= D <= 3.0)
  - Si existe: propone la solucion
  - Si no existe: error preciso explicando el conflicto

yum podia fallar en este caso
libsolv siempre encuentra la mejor solucion o reporta correctamente
```

---

## La transicion yum → dnf

### RHEL 7 y anteriores: yum es nativo

```bash
yum install nginx
yum update
yum search nginx
```

### RHEL 8+: dnf nativo, yum es symlink

```bash
# dnf es el binario nativo
dnf install nginx

# yum es un symlink a dnf
ls -la /usr/bin/yum
# lrwxrwxrwx 1 root root 5 ... /usr/bin/yum -> dnf-3

# Ambos producen resultados IDENTICOS
yum install nginx    # internamente ejecuta dnf-3
dnf install nginx    # nativo
```

### RHEL 9 / Fedora 39+

```bash
# RHEL 9 usa dnf 4 (Python)
dnf --version

# Fedora 39+ introduce dnf5 (reescritura en C++)
# yum sigue como symlink en todos lados
```

### Cuando usar cada uno

```
RHEL 8+, AlmaLinux 8+, Rocky 8+, Fedora:
  → dnf (siempre)
  yum funciona pero es alias — mejor usar el nombre real

RHEL 7 / CentOS 7 (legacy):
  → yum (dnf no disponible por defecto)

Scripts que deben funcionar en RHEL 7 Y 8+:
  → yum (funciona en ambos porque en 8+ es alias)

En este curso:
  → dnf (AlmaLinux 9)
```

---

## Diferencias tecnicas yum vs dnf

| Aspecto | `yum` (legacy) | `dnf` |
|---|---|---|
| Lenguaje | Python 2 | Python 3 (dnf5: C++) |
| Resolver de deps | Propio (lento, impreciso) | libsolv (SAT solver) |
| Cache de metadata | Descarga completa cada vez | Cache con expiracion |
| Transacciones | A veces inconsistentes | Robustas y predecibles |
| Autoremove | Limitado (RHEL 7.2+) | Completo |
| Modulos/streams | No | Si (RHEL 8+) |
| Plugins | yum-specific | DNF Plugin API |
| Rendimiento | Lento con muchos repos | 2-3x mas rapido |
| `update` vs `upgrade` | `update` es oficial | `upgrade` es oficial, `update` alias |
| Descargas paralelas | No | Si (`max_parallel_downloads`) |
| Historial + undo | Basico | Completo (`dnf history undo`) |

---

## Comandos equivalentes yum ↔ dnf

Casi todos los comandos son identicos:

| yum | dnf | Descripcion |
|---|---|---|
| `yum install pkg` | `dnf install pkg` | Instalar |
| `yum remove pkg` | `dnf remove pkg` | Desinstalar |
| `yum update` | `dnf upgrade` | Actualizar todos |
| `yum update pkg` | `dnf upgrade pkg` | Actualizar uno |
| `yum check-update` | `dnf check-update` | Ver updates disponibles |
| `yum search term` | `dnf search term` | Buscar |
| `yum info pkg` | `dnf info pkg` | Informacion |
| `yum list installed` | `dnf list installed` | Listar instalados |
| `yum list available` | `dnf list available` | Listar disponibles |
| `yum provides file` | `dnf provides file` | Que paquete provee este archivo |
| `yum repolist` | `dnf repolist` | Listar repos activos |
| `yum repolist all` | `dnf repolist --all` | Todos los repos |
| `yum clean all` | `dnf clean all` | Limpiar cache |
| `yum history` | `dnf history` | Historial de transacciones |
| `yum group list` | `dnf group list` | Listar grupos |
| `yum groupinstall "X"` | `dnf group install "X"` | Instalar grupo |
| `yum localinstall x.rpm` | `dnf install x.rpm` | Instalar .rpm local |
| `yumdownloader pkg` | `dnf download pkg` | Descargar .rpm |

### Diferencias sutiles

```bash
# 1. "update" vs "upgrade"
yum update              # oficial en yum
dnf update              # funciona (alias)
dnf upgrade             # oficial en dnf (preferir este)

# 2. "groupinstall" vs "group install"
yum groupinstall "Development Tools"   # una sola palabra
dnf group install "Development Tools"  # dos palabras (oficial)
dnf groupinstall "Development Tools"   # alias, tambien funciona

# 3. autoremove
yum autoremove         # limitado, built-in desde RHEL 7.2
dnf autoremove         # completo (como apt autoremove)

# 4. download
yumdownloader pkg      # requiere paquete yum-utils
dnf download pkg       # nativo en dnf
dnf download --resolve pkg  # con dependencias
```

### Comandos exclusivos de dnf

```bash
# Descargar sin instalar
dnf download nginx
dnf download --resolve nginx    # con dependencias

# Que paquete provee un archivo (incluyendo no instalados)
dnf provides /usr/sbin/nginx
dnf whatprovides /usr/bin/python3

# Listar archivos de un paquete (sin instalarlo)
dnf repoquery -l nginx

# Historial detallado
dnf history
dnf history info 5              # detalle de transaccion 5
dnf history undo 5              # deshacer transaccion 5

# Modulos (RHEL 8+)
dnf module list
dnf module enable nodejs:18
dnf module install nodejs:18
```

---

## Configuracion de dnf

### /etc/dnf/dnf.conf

```ini
[main]
gpgcheck=1                             # verificar firmas GPG (NO cambiar a 0)
installonly_limit=3                    # max kernels instalados (actual + 2 anteriores)
clean_requirements_on_remove=True      # autoremove al hacer dnf remove
best=True                              # instalar la mejor version o fallar
skip_if_unavailable=False              # fallar si un repo no responde
max_parallel_downloads=3               # descargas simultaneas
defaultyes=False                       # esperar confirmacion (Enter = yes si True)
keepcache=0                            # 0=no conservar .rpm descargados
```

### Opciones explicadas

| Opcion | Default | Efecto |
|---|---|---|
| `gpgcheck` | 1 | Verificar firmas GPG (no cambiar a 0) |
| `installonly_limit` | 3 | Kernels simultaneos: actual + 2 anteriores (rollback) |
| `clean_requirements_on_remove` | True | Eliminar deps huerfanas con `dnf remove` |
| `best` | True | Instalar la version mas nueva disponible o fallar |
| `skip_if_unavailable` | False | Si un repo falla, abortar la operacion |
| `max_parallel_downloads` | 3 | Conexiones simultaneas (subir en conexiones rapidas) |
| `defaultyes` | False | Si True, Enter acepta sin escribir "y" |
| `keepcache` | 0 | 0=no guardar .rpm (ahorra espacio), 1=guardar |
| `metadata_expire` | 172800 (48h) | Segundos antes de refrescar metadata de repos |

```bash
# Acelerar descargas en conexiones rapidas
echo 'max_parallel_downloads=10' | sudo tee -a /etc/dnf/dnf.conf
```

### Metadata: dnf vs apt

Una diferencia importante con APT: **dnf no necesita un comando
`update` previo** para instalar paquetes. dnf mantiene cache de metadata
y la refresca automaticamente cuando expira (`metadata_expire`).

```bash
# APT: obligatorio antes de instalar
sudo apt update && sudo apt install nginx

# DNF: no necesario — refresca metadata automaticamente
sudo dnf install nginx
# (si la metadata tiene menos de 48h, usa cache)

# Forzar refresco manual de metadata (equivalente a apt update)
dnf makecache
dnf clean metadata && dnf makecache    # forzar refresco completo
```

### Repositorios

```bash
# Ver repos activos
dnf repolist

# Ver todos (activos + deshabilitados)
dnf repolist --all

# Habilitar/deshabilitar un repo
sudo dnf config-manager --enable epel
sudo dnf config-manager --disable epel

# Agregar un repo desde URL
sudo dnf config-manager --add-repo https://example.com/repo/file.repo

# Informacion detallada de un repo
dnf repoinfo epel
```

### Plugins

```bash
# Ver plugins instalados
ls /etc/dnf/plugins/

# Plugins comunes:
# copr         → repositorios COPR (como PPAs de Ubuntu)
# versionlock  → bloquear version de un paquete (como apt-mark hold)
# builddep     → instalar deps de compilacion
```

---

## dnf vs apt: comparacion rapida

| Aspecto | apt (Debian) | dnf (RHEL) |
|---|---|---|
| Actualizar indices | `apt update` (manual, obligatorio) | Automatico (cache con expiracion) |
| Instalar | `apt install pkg` | `dnf install pkg` |
| Actualizar todo | `apt upgrade` | `dnf upgrade` |
| Desinstalar | `apt remove` / `apt purge` | `dnf remove` (no hay purge separado) |
| Buscar | `apt search` | `dnf search` |
| Info | `apt show` | `dnf info` |
| Que archivo → paquete | `dpkg -S file` | `rpm -qf file` o `dnf provides file` |
| Historial | No nativo | `dnf history` + `dnf history undo` |
| Grupos | No nativo | `dnf group install` |
| Modulos/streams | No | `dnf module` (RHEL 8+) |
| Herramienta baja | `dpkg` (`.deb`) | `rpm` (`.rpm`) |
| Config de repos | `sources.list` / `.sources` | `.repo` files en `/etc/yum.repos.d/` |

---

## Ejercicios

### Ejercicio 1 — Verificar la relacion yum/dnf

Confirma que yum es un alias de dnf en tu sistema.

```bash
# 1. Version del sistema
cat /etc/redhat-release

# 2. Version de dnf
dnf --version
```

**Pregunta**: A donde apunta `/usr/bin/yum`? Y que significa eso?

<details><summary>Prediccion</summary>

`/usr/bin/yum` es un symlink a `dnf-3` (o `dnf`). Esto significa que
ejecutar `yum` y `dnf` produce **exactamente el mismo resultado** —
son el mismo binario.

```bash
# 3. Verificar el symlink
ls -la /usr/bin/yum
readlink -f /usr/bin/yum

# 4. Confirmar que son el mismo binario
file /usr/bin/yum
file /usr/bin/dnf

# 5. Comparar versiones
yum --version 2>/dev/null | head -1
dnf --version 2>/dev/null | head -1
# (identicas)
```

yum se mantiene como alias por compatibilidad con scripts antiguos
escritos para RHEL 7 y anteriores. En sistemas nuevos, siempre usar `dnf`.

</details>

---

### Ejercicio 2 — Comparar salida de yum vs dnf

Demuestra que ambos comandos producen output identico.

```bash
# 1. Comparar repolist
yum repolist 2>&1 > /tmp/yum_out.txt
dnf repolist 2>&1 > /tmp/dnf_out.txt
diff /tmp/yum_out.txt /tmp/dnf_out.txt
```

**Pregunta**: El diff muestra diferencias?

<details><summary>Prediccion</summary>

No deberia haber diferencias (o solo timestamps). Son el mismo binario,
asi que producen el mismo output.

```bash
# 2. Comparar info de un paquete
yum info bash > /tmp/yum_info.txt 2>&1
dnf info bash > /tmp/dnf_info.txt 2>&1
diff /tmp/yum_info.txt /tmp/dnf_info.txt

# 3. Comparar search
yum search "web server" > /tmp/yum_search.txt 2>&1
dnf search "web server" > /tmp/dnf_search.txt 2>&1
diff /tmp/yum_search.txt /tmp/dnf_search.txt

# 4. Limpiar
rm -f /tmp/yum_*.txt /tmp/dnf_*.txt
```

</details>

---

### Ejercicio 3 — Jerarquia rpm/dnf vs dpkg/apt

Compara las jerarquias de gestion de paquetes.

```bash
# 1. La jerarquia en RHEL
echo "rpm  = bajo nivel (como dpkg)"
echo "dnf  = alto nivel (como apt)"
```

**Pregunta**: Que analogias hay entre `rpm -qa` y `dpkg -l`, entre
`rpm -qf /path` y `dpkg -S /path`, entre `rpm -ql pkg` y `dpkg -L pkg`?

<details><summary>Prediccion</summary>

Son equivalentes funcionales:

| Operacion | Debian (dpkg) | RHEL (rpm) |
|---|---|---|
| Listar todos los paquetes | `dpkg -l` | `rpm -qa` |
| Archivos de un paquete | `dpkg -L pkg` | `rpm -ql pkg` |
| Que paquete instalo un archivo | `dpkg -S /path` | `rpm -qf /path` |
| Info detallada | `dpkg -s pkg` | `rpm -qi pkg` |
| Info de un .deb/.rpm sin instalar | `dpkg -I file.deb` | `rpm -qip file.rpm` |
| Contenido sin instalar | `dpkg -c file.deb` | `rpm -qlp file.rpm` |
| Verificar integridad | `dpkg -V pkg` | `rpm -V pkg` |

```bash
# 2. Probar los equivalentes
rpm -qa | wc -l           # total paquetes instalados
rpm -ql bash | head -5    # archivos de bash
rpm -qf /usr/bin/env      # quien instalo /usr/bin/env
rpm -qi bash | head -10   # info detallada
```

La "q" en los flags de rpm significa "query" (consulta).

</details>

---

### Ejercicio 4 — Explorar dnf.conf

Examina la configuracion de dnf y entiende cada opcion.

```bash
# 1. Ver configuracion actual
cat /etc/dnf/dnf.conf
```

**Pregunta**: Que valor tiene `installonly_limit`? Y que pasaria si
lo pones en 1?

<details><summary>Prediccion</summary>

`installonly_limit=3` por defecto — mantiene 3 kernels: el actual +
2 anteriores. Esto permite hacer rollback si un kernel nuevo causa
problemas.

Con `installonly_limit=1` solo se mantendria el kernel mas reciente.
Si ese kernel tiene un bug, **no habria kernel anterior** al cual
volver en GRUB — riesgo de dejar el sistema sin arranque.

```bash
# 2. Ver kernels instalados actualmente
rpm -q kernel-core 2>/dev/null || rpm -q kernel

# 3. Ver opciones configuradas
grep -v "^#" /etc/dnf/dnf.conf | grep -v "^$"

# 4. Verificar descargas paralelas
grep max_parallel_downloads /etc/dnf/dnf.conf || echo "default: 3"

# 5. Verificar metadata_expire
grep metadata_expire /etc/dnf/dnf.conf || echo "default: 172800 (48h)"
```

`gpgcheck=1` es la opcion mas critica — deshabilitarla permitiria
instalar paquetes no firmados, un riesgo de seguridad grave.

</details>

---

### Ejercicio 5 — Metadata: dnf vs apt

Comprende como dnf maneja la metadata comparado con apt.

```bash
# 1. Ver cuando se actualizo la metadata por ultima vez
dnf repolist -v 2>/dev/null | grep -i "updated\|expire"
```

**Pregunta**: Por que `dnf install nginx` funciona sin ejecutar
`dnf update` primero, pero `apt install nginx` falla sin `apt update`?

<details><summary>Prediccion</summary>

dnf mantiene un **cache de metadata con expiracion** (`metadata_expire`,
default 48h en RHEL). Cuando ejecutas `dnf install`, dnf verifica si
la metadata cacheada sigue vigente:
- Si no ha expirado → usa la cache directamente
- Si ha expirado → refresca automaticamente antes de operar

APT no tiene este mecanismo — los indices locales
(`/var/lib/apt/lists/`) no se actualizan solos. `apt update` es
obligatorio para sincronizarlos.

```bash
# 2. Forzar refresco manual (equivalente a apt update)
dnf makecache

# 3. Ver la cache
ls /var/cache/dnf/

# 4. Limpiar cache y refrescar
dnf clean metadata
dnf makecache

# 5. Expirar toda la cache
dnf clean all
```

`dnf makecache` es el equivalente de `apt update`, pero rara vez es
necesario usarlo explicitamente.

</details>

---

### Ejercicio 6 — update vs upgrade

Aclara la diferencia entre `update` y `upgrade` en yum vs dnf.

```bash
# 1. En dnf, update es alias de upgrade
dnf upgrade --assumeno 2>/dev/null | tail -5
dnf update --assumeno 2>/dev/null | tail -5
```

**Pregunta**: Por que dnf cambio el nombre de `update` a `upgrade`?

<details><summary>Prediccion</summary>

Para alinear la terminologia con APT y otros gestores:
- `update` en APT = actualizar **indices** (metadata)
- `upgrade` en APT = actualizar **paquetes**

yum usaba `update` para actualizar paquetes, lo cual era confuso. dnf
adopto `upgrade` como nombre oficial para actualizar paquetes, y mantiene
`update` como alias por compatibilidad.

```bash
# 2. Verificar que son identicos
dnf upgrade --assumeno 2>&1 | md5sum
dnf update --assumeno 2>&1 | md5sum
# (misma salida, mismo checksum)

# 3. check-update: ver que se puede actualizar
dnf check-update 2>/dev/null | head -10
```

En resumen:
- `dnf upgrade` = actualizar paquetes (oficial)
- `dnf update` = alias de upgrade (funciona, pero preferir upgrade)
- `dnf makecache` = actualizar metadata (equivalente a apt update)

</details>

---

### Ejercicio 7 — Historial de transacciones

Explora `dnf history`, una funcionalidad que APT no tiene.

```bash
# 1. Ver historial
dnf history
```

**Pregunta**: Que informacion muestra cada linea del historial? Y para
que sirve `dnf history undo`?

<details><summary>Prediccion</summary>

Cada linea muestra:
- **ID**: numero de transaccion
- **Command line**: el comando que se ejecuto
- **Date and time**: cuando se ejecuto
- **Action(s)**: que se hizo (Install, Upgrade, Remove)
- **Altered**: cuantos paquetes se modificaron

`dnf history undo N` **revierte** la transaccion N — si la transaccion
instalo paquetes, los desinstala; si los desinstalo, los reinstala.
APT no tiene equivalente nativo.

```bash
# 2. Ver detalle de una transaccion
dnf history info 1

# 3. Ver paquetes instalados por el usuario (no como deps)
dnf history userinstalled | head -10

# 4. dnf history undo deshace una transaccion
# (no ejecutar sin verificar primero que se desharia)
# dnf history undo 5 --assumeno
```

Esta funcionalidad es especialmente util en servidores: si un `dnf
upgrade` causa problemas, puedes revertirlo con `dnf history undo`.

</details>

---

### Ejercicio 8 — Consultas con dnf provides y repoquery

Usa las herramientas de consulta exclusivas de dnf.

```bash
# 1. Que paquete provee un archivo
dnf provides /usr/sbin/useradd
```

**Pregunta**: Cual es la diferencia entre `rpm -qf /path` y
`dnf provides /path`?

<details><summary>Prediccion</summary>

- `rpm -qf /path`: busca solo entre paquetes **instalados** localmente.
  Si el archivo no pertenece a ningun paquete instalado, falla.
- `dnf provides /path`: busca en **todos los repositorios**, incluyendo
  paquetes no instalados. Util para encontrar que paquete instalar
  cuando necesitas un archivo que no tienes.

```bash
# 2. Comparar
rpm -qf /usr/bin/env          # solo busca en instalados
dnf provides /usr/bin/env     # busca en repos tambien

# 3. Buscar un archivo que NO esta instalado
dnf provides /usr/bin/htpasswd
# Muestra que paquete lo instalaria (ej: httpd-tools)

# 4. Listar archivos de un paquete sin instalarlo
dnf repoquery -l httpd | head -15

# 5. Ver dependencias de un paquete
dnf repoquery --deplist httpd | head -15
```

</details>

---

### Ejercicio 9 — Descargar sin instalar

Descarga paquetes `.rpm` para inspeccion o instalacion offline.

```bash
# 1. Descargar un paquete
mkdir -p /tmp/rpm-test && cd /tmp/rpm-test
dnf download tree 2>/dev/null
ls -lh *.rpm
```

**Pregunta**: Que diferencia hay entre `dnf download pkg` y
`dnf download --resolve pkg`?

<details><summary>Prediccion</summary>

- `dnf download pkg`: descarga solo el `.rpm` del paquete especificado
- `dnf download --resolve pkg`: descarga el `.rpm` **mas todas sus
  dependencias** que no estan instaladas

El flag `--resolve` es util para crear un repositorio offline o instalar
en maquinas sin acceso a internet.

```bash
# 2. Comparar
echo "=== Sin --resolve ==="
dnf download tree 2>/dev/null
ls *.rpm | wc -l

echo "=== Con --resolve ==="
dnf download --resolve tree 2>/dev/null
ls *.rpm | wc -l
# Mas archivos — incluye dependencias

# 3. Inspeccionar el .rpm sin instalar
rpm -qip tree-*.rpm    # info
rpm -qlp tree-*.rpm    # archivos que contiene

# 4. Limpiar
cd ~ && rm -rf /tmp/rpm-test
```

En yum, esta funcionalidad requeria el paquete `yum-utils` y el
comando `yumdownloader`. En dnf es nativo.

</details>

---

### Ejercicio 10 — Panorama: ecosistema de paquetes Debian vs RHEL

Construye el mapa mental completo comparando ambos ecosistemas.

```bash
cat <<'EOF'
ECOSISTEMA DEBIAN:
  dpkg (.deb)  →  apt-get/apt  →  repositorios deb
  dpkg -i          apt install      sources.list
  dpkg -l          apt search       dists/ + pool/
  dpkg -L          apt show         InRelease (GPG)
  dpkg -S          apt update       (indices manuales)
  dpkg -V          apt upgrade

ECOSISTEMA RHEL:
  rpm (.rpm)   →  yum/dnf      →  repositorios rpm
  rpm -ivh         dnf install      .repo files
  rpm -qa          dnf search       repodata/
  rpm -ql          dnf info         repomd.xml (GPG)
  rpm -qf          dnf makecache    (metadata_expire auto)
  rpm -V           dnf upgrade
EOF
```

**Pregunta**: Que funcionalidades tiene dnf que APT no tiene nativamente?
Y al reves?

<details><summary>Prediccion</summary>

**dnf tiene, APT no:**
- `dnf history` + `dnf history undo` — historial de transacciones con
  capacidad de revertir
- `dnf group install` — instalar grupos de paquetes predefinidos
- `dnf module` — module streams para multiples versiones de un software
  (ej: nodejs:18, nodejs:20)
- Metadata con expiracion automatica (no necesita "update" manual)
- `dnf download --resolve` — descarga con dependencias nativo

**APT tiene, dnf no:**
- `apt purge` vs `apt remove` — separacion entre eliminar binarios y
  eliminar configuracion. En RHEL, `dnf remove` siempre elimina todo
  (rpm no distingue conffiles del mismo modo que dpkg)
- `apt-mark hold/unhold` — mas intuitivo que `dnf versionlock`
- `apt show` muestra paquetes no instalados (dnf necesita `dnf info`)
- DEB822 format (`.sources`) — formato moderno de repos

```bash
# Resumen de la seccion S02:
echo "rpm:  bajo nivel — instala .rpm, no resuelve deps"
echo "yum:  legacy — Python 2, resolver propio"
echo "dnf:  actual — Python 3/C++, libsolv (SAT solver)"
echo "yum en RHEL 8+ = symlink a dnf"
echo ""
echo "Proximo: repositorios en dnf/yum (T02)"
```

</details>
