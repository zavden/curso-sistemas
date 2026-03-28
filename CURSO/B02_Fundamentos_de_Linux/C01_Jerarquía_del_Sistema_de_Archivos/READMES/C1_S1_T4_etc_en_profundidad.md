# T04 — /etc en profundidad

## Objetivo

Explorar los archivos de configuración fundamentales en `/etc`, entender el
patrón de directorios `.d/` para configuración modular, y comparar las
diferencias entre Debian y AlmaLinux.

---

## Notas respecto al material original

1. **Etimología de `/etc`** — El original dice "históricamente 'et cetera',
   hoy entendido como 'Editable Text Configuration'". La segunda parte es un
   **backrónimo** popular en la comunidad Linux, no una redefinición oficial.
   Dennis Ritchie (co-creador de Unix) confirmó que el nombre era simplemente
   "et cetera" — el directorio para "lo demás" que no encajaba en otro lugar.
   "Editable Text Configuration" es un buen mnemónico pero no debe presentarse
   como el significado actual.

---

## Propósito de /etc

`/etc` contiene todos los archivos de configuración del sistema. Es el
directorio que define el **comportamiento específico de esta máquina**.

Dos servidores con el mismo software instalado pero diferente `/etc` se
comportan de forma completamente distinta.

```
/usr → software instalado (igual entre máquinas con los mismos paquetes)
/etc → configuración (única por máquina)
/var → datos que genera el software (dinámicos)
```

## Convenciones de archivos

### Archivos de texto plano

La mayoría de archivos en `/etc` son texto plano editable. Esta es una
decisión de diseño de Unix: la configuración es legible y editable con
cualquier editor. No hay registros binarios ni bases de datos opacas.

```bash
file /etc/hostname    # ASCII text
file /etc/fstab       # ASCII text
file /etc/shadow      # ASCII text (permisos restrictivos)
```

### Directorios `.d/`

Patrón moderno para configuración modular: en vez de un solo archivo
monolítico, se usa un directorio `.d/` con fragmentos:

```
/etc/apt/sources.list.d/     ← Fragmentos de sources.list
/etc/sudoers.d/              ← Fragmentos de sudoers
/etc/sysctl.d/               ← Fragmentos de sysctl.conf
/etc/cron.d/                 ← Crontabs del sistema por servicio
/etc/logrotate.d/            ← Rotación de logs por servicio
/etc/profile.d/              ← Scripts ejecutados al iniciar sesión
```

Los archivos dentro de `.d/` se procesan en **orden alfabético** (por eso muchos
se nombran con prefijo numérico):

```bash
ls /etc/sysctl.d/
# 10-console-messages.conf
# 30-postgresql.conf
# 99-sysctl.conf
```

**Ventaja**: paquetes e instaladores pueden agregar/quitar su propia
configuración sin modificar el archivo principal. Esto evita conflictos en
actualizaciones.

### Archivos con extensión `.conf`

Convención estándar para archivos de configuración. No todos siguen esta
convención (muchos no tienen extensión: `fstab`, `passwd`, `shadow`), pero
`.conf` es el estándar para servicios nuevos.

## Archivos fundamentales

### `/etc/hostname`

Nombre del host en una sola línea:

```bash
cat /etc/hostname
# webserver01
```

Cambiar el hostname:

```bash
# Método moderno (persistente)
sudo hostnamectl set-hostname newname

# Verificar
hostnamectl
```

### `/etc/hosts`

Resolución estática de nombres (antes de DNS):

```bash
cat /etc/hosts
# 127.0.0.1       localhost
# 127.0.1.1       webserver01
# ::1             localhost ip6-localhost
#
# 192.168.1.10    db-server
# 192.168.1.11    app-server
```

Se consulta **antes** que DNS (configurable en `/etc/nsswitch.conf`). Útil
para entornos locales, desarrollo, y sobrescribir DNS temporalmente.

### `/etc/resolv.conf`

Configuración de resolución DNS:

```bash
cat /etc/resolv.conf
# nameserver 8.8.8.8
# nameserver 8.8.4.4
# search example.com
```

En sistemas modernos con systemd-resolved o NetworkManager, este archivo puede
ser un symlink gestionado automáticamente:

```bash
ls -la /etc/resolv.conf
# lrwxrwxrwx ... /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf
```

### `/etc/nsswitch.conf`

Name Service Switch — define el orden de búsqueda para nombres, usuarios,
hosts, etc.:

```bash
cat /etc/nsswitch.conf
# passwd:     files systemd
# group:      files systemd
# hosts:      files dns mymachines
```

`hosts: files dns` significa: buscar primero en `/etc/hosts`, luego en DNS.

### `/etc/fstab`

Tabla de sistemas de archivos a montar al arranque:

```
# <filesystem>        <mount>   <type>  <options>           <dump> <pass>
UUID=abc123-...       /         ext4    errors=remount-ro   0      1
UUID=def456-...       /home     ext4    defaults            0      2
UUID=ghi789-...       none      swap    sw                  0      0
tmpfs                 /tmp      tmpfs   defaults,size=2G    0      0
```

| Campo | Significado |
|---|---|
| filesystem | Dispositivo (UUID, LABEL, o path) |
| mount | Punto de montaje |
| type | Tipo de filesystem (ext4, xfs, tmpfs, nfs) |
| options | Opciones de montaje (defaults, ro, noexec, nosuid) |
| dump | Backup con dump (0 = no, obsoleto) |
| pass | Orden de fsck al arranque (0 = no verificar, 1 = raíz, 2 = el resto) |

### `/etc/passwd` y `/etc/shadow`

Base de datos de usuarios:

```bash
cat /etc/passwd
# root:x:0:0:root:/root:/bin/bash
# dev:x:1000:1000::/home/dev:/bin/bash
# nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
```

Formato: `usuario:x:UID:GID:comentario:home:shell`

El campo `x` indica que la contraseña está en `/etc/shadow` (no en passwd).
`/etc/shadow` solo es legible por root y contiene los hashes de contraseñas.

Cubierto en detalle en C02 (Usuarios, Grupos y Permisos).

### `/etc/group` y `/etc/gshadow`

Base de datos de grupos:

```bash
cat /etc/group
# root:x:0:
# sudo:x:27:dev
# docker:x:999:dev
# dev:x:1000:
```

Formato: `grupo:x:GID:miembros`

### `/etc/sudoers`

Configuración de privilegios sudo. **Nunca editar directamente** — usar
`visudo` que valida la sintaxis antes de guardar:

```
# Formato: quién dónde=(como_quién) qué
root    ALL=(ALL:ALL) ALL
%sudo   ALL=(ALL:ALL) ALL          # Grupo sudo en Debian
%wheel  ALL=(ALL:ALL) ALL          # Grupo wheel en RHEL
dev     ALL=(ALL) NOPASSWD: ALL    # Sin contraseña (desarrollo)
```

Los fragmentos modulares van en `/etc/sudoers.d/`.

### `/etc/os-release`

Forma estándar de identificar la distribución (definido por systemd):

```bash
cat /etc/os-release
# ID=debian
# VERSION_ID="12"
# PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"
```

Archivos adicionales específicos por distro:
- Debian: `/etc/debian_version`
- AlmaLinux/RHEL: `/etc/redhat-release`

## Configuración de red

### Debian/Ubuntu

```bash
# Moderno (netplan, Ubuntu)
ls /etc/netplan/

# Legacy (ifupdown, Debian)
cat /etc/network/interfaces
```

### RHEL/AlmaLinux

```bash
# NetworkManager (RHEL 9+, keyfiles)
ls /etc/NetworkManager/system-connections/
nmcli connection show
```

En RHEL 9, los archivos legacy de `/etc/sysconfig/network-scripts/` fueron
deprecados a favor de keyfiles de NetworkManager.

## Configuración de servicios

Los servicios colocan su configuración en `/etc`:

```
/etc/nginx/              ← Configuración de nginx
├── nginx.conf           ← Archivo principal
├── sites-available/     ← Sitios disponibles [Debian]
├── sites-enabled/       ← Sitios activos (symlinks) [Debian]
└── conf.d/              ← Fragmentos de configuración [RHEL]

/etc/ssh/                ← Configuración de SSH
├── sshd_config          ← Servidor
└── ssh_config           ← Cliente

/etc/systemd/            ← Overrides de systemd
├── system/              ← Overrides de servicios del sistema
└── user/                ← Overrides de servicios de usuario
```

### Configuración del sistema vs defaults

Los archivos en `/etc` sobrescriben los defaults del paquete:

```
/usr/lib/systemd/system/sshd.service  ← Default (del paquete)
/etc/systemd/system/sshd.service.d/   ← Drop-in overrides (del administrador)
```

La forma recomendada de sobrescribir es con `systemctl edit sshd`, que crea un
drop-in en `/etc/systemd/system/sshd.service.d/override.conf`. Esto permite
modificar campos específicos sin reemplazar todo el archivo.

El gestor de paquetes actualiza `/usr/lib/` sin conflictos. El administrador
edita `/etc/` sin que sus cambios se pierdan en actualizaciones.

## Backup de /etc

`/etc` es el directorio más importante para respaldar. Con un backup de `/etc`
y la lista de paquetes instalados, se puede reconstruir un servidor:

```bash
# Backup completo de /etc
sudo tar czf /backup/etc-$(date +%Y%m%d).tar.gz /etc/

# Lista de paquetes instalados
dpkg --get-selections > /backup/packages.list    # Debian
rpm -qa > /backup/packages.list                  # RHEL
```

---

## Ejercicios

Todos los comandos se ejecutan dentro de los contenedores del curso.

### Ejercicio 1 — Explorar la estructura de /etc

Examina cuántos archivos tiene `/etc` y cuántos usan el patrón `.d/`.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Estadísticas de /etc ==="
echo "Archivos totales: $(find /etc -type f 2>/dev/null | wc -l)"
echo "Directorios .d/:  $(find /etc -maxdepth 2 -name "*.d" -type d 2>/dev/null | wc -l)"
echo ""
echo "=== Directorios .d/ encontrados ==="
find /etc -maxdepth 2 -name "*.d" -type d 2>/dev/null | sort
'
```

<details>
<summary>Predicción</summary>

`/etc` contiene cientos de archivos (probablemente 200-400 en una imagen de
desarrollo con muchos paquetes instalados). El número exacto depende de cuántos
paquetes están instalados.

Los directorios `.d/` incluyen:
- `/etc/apt/sources.list.d/` — repositorios adicionales
- `/etc/cron.d/` — crontabs del sistema
- `/etc/logrotate.d/` — rotación de logs por servicio
- `/etc/profile.d/` — scripts de inicio de sesión
- `/etc/sudoers.d/` — fragmentos de sudoers
- `/etc/sysctl.d/` — parámetros del kernel
- `/etc/ld.so.conf.d/` — rutas de bibliotecas dinámicas

El patrón `.d/` es la forma moderna de organizar configuración: cada paquete
deposita su fragmento sin tocar el archivo principal, evitando conflictos en
actualizaciones.

</details>

---

### Ejercicio 2 — Examinar hostname y hosts

Inspecciona la identidad del contenedor y su resolución de nombres local.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /etc/hostname ==="
cat /etc/hostname

echo ""
echo "=== hostname command ==="
hostname

echo ""
echo "=== /etc/hosts ==="
cat /etc/hosts

echo ""
echo "=== Resolución local ==="
getent hosts localhost
getent hosts $(hostname) 2>/dev/null || echo "hostname no resuelve por getent"
'
```

<details>
<summary>Predicción</summary>

`/etc/hostname` contiene el hostname definido en el `compose.yml` del lab
(campo `hostname:` del servicio). El comando `hostname` devuelve el mismo
valor.

`/etc/hosts` muestra:
- `127.0.0.1 localhost` — la entrada estándar de loopback
- Una línea con la IP del contenedor y su hostname — inyectada por Docker
- Posiblemente entradas de otros contenedores de la misma red Compose

Docker gestiona `/etc/hosts` automáticamente: agrega la IP del contenedor
al arrancar. No es un archivo estático como en un host normal.

`getent hosts localhost` devuelve `127.0.0.1 localhost` (resuelve via
`/etc/hosts`). `getent hosts $(hostname)` devuelve la IP asignada por Docker
al contenedor.

</details>

---

### Ejercicio 3 — Analizar passwd y group

Examina el formato de los archivos de usuarios y grupos.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Primeros usuarios (sistema) ==="
head -3 /etc/passwd

echo ""
echo "=== Últimos usuarios (regulares) ==="
tail -3 /etc/passwd

echo ""
echo "=== Formato: usuario:x:UID:GID:comentario:home:shell ==="
echo "Desglose de root:"
IFS=: read user pass uid gid comment home shell <<< "$(grep "^root:" /etc/passwd)"
echo "  Usuario:    $user"
echo "  UID:        $uid"
echo "  GID:        $gid"
echo "  Home:       $home"
echo "  Shell:      $shell"

echo ""
echo "=== Grupos del sistema ==="
head -5 /etc/group

echo ""
echo "=== Shells disponibles ==="
cat /etc/shells
'
```

<details>
<summary>Predicción</summary>

Los primeros usuarios en `/etc/passwd` son usuarios del sistema:
- `root:x:0:0:root:/root:/bin/bash` — UID 0 = superusuario
- `daemon:x:1:1:...:/usr/sbin:/usr/sbin/nologin`
- `bin:x:2:2:...`

Los últimos son los usuarios más recientes (posiblemente un usuario creado en
el Dockerfile). Los usuarios del sistema tienen UIDs bajos (<1000) y shells
`/usr/sbin/nologin` o `/bin/false` (no pueden iniciar sesión).

El campo `x` en la segunda columna indica que la contraseña está en
`/etc/shadow`, no en `/etc/passwd`. Históricamente el hash estaba en passwd
(legible por todos). La migración a shadow fue por seguridad.

`/etc/shells` lista los shells válidos del sistema (`/bin/bash`, `/bin/sh`,
`/bin/dash`, etc.). Un usuario con shell no listado aquí puede tener
restricciones (algunos servicios como FTP verifican este archivo).

</details>

---

### Ejercicio 4 — Seguir la cadena de resolución de nombres

Traza cómo el sistema resuelve un nombre: nsswitch → hosts → DNS.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Paso 1: nsswitch.conf (orden de búsqueda) ==="
grep "^hosts:" /etc/nsswitch.conf

echo ""
echo "=== Paso 2: /etc/hosts (búsqueda local) ==="
cat /etc/hosts

echo ""
echo "=== Paso 3: /etc/resolv.conf (DNS) ==="
cat /etc/resolv.conf

echo ""
echo "=== Prueba de resolución ==="
echo "localhost:"
getent hosts localhost

echo ""
echo "hostname del contenedor:"
getent hosts $(hostname)

echo ""
echo "google.com (requiere DNS):"
getent hosts google.com 2>/dev/null || echo "  DNS no disponible en este contenedor"
'
```

<details>
<summary>Predicción</summary>

`nsswitch.conf` muestra `hosts: files dns` (o similar con `mymachines`). Esto
define el orden:
1. `files` → busca en `/etc/hosts`
2. `dns` → busca en los nameservers de `/etc/resolv.conf`

Para `localhost`: se resuelve inmediatamente con `/etc/hosts` (paso 1, no llega
a DNS).

Para el hostname del contenedor: Docker lo agregó a `/etc/hosts`, así que
también se resuelve localmente.

Para `google.com`: no está en `/etc/hosts`, así que pasa a DNS.
`/etc/resolv.conf` muestra los nameservers inyectados por Docker (normalmente
los del host). Si el contenedor no tiene conectividad DNS, `getent` fallará.

`getent` es la herramienta correcta para probar la resolución completa
(respeta nsswitch.conf). `nslookup` y `dig` siempre van directo a DNS,
saltándose `/etc/hosts`.

</details>

---

### Ejercicio 5 — Explorar el patrón .d/ en detalle

Examina cómo funcionan los directorios `.d/` y el orden de carga.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== /etc/profile.d/ (scripts de inicio de sesión) ==="
ls -la /etc/profile.d/ 2>/dev/null

echo ""
echo "=== /etc/ld.so.conf.d/ (rutas de bibliotecas) ==="
ls /etc/ld.so.conf.d/ 2>/dev/null
echo ""
echo "Contenido de ld.so.conf:"
cat /etc/ld.so.conf

echo ""
echo "=== /etc/sudoers.d/ ==="
ls /etc/sudoers.d/ 2>/dev/null || echo "(vacío o sin permisos)"

echo ""
echo "=== Demostración del orden alfabético ==="
echo "Los archivos se cargan en este orden:"
find /etc/profile.d/ -name "*.sh" 2>/dev/null | sort
'
```

<details>
<summary>Predicción</summary>

`/etc/profile.d/` contiene scripts `.sh` ejecutados al iniciar sesión
interactiva. Pueden incluir configuración de colores del prompt, aliases,
variables de entorno de aplicaciones, etc.

`/etc/ld.so.conf` contiene una sola línea: `include /etc/ld.so.conf.d/*.conf`.
Esto demuestra el patrón `.d/` en acción: el archivo principal solo incluye los
fragmentos del directorio. Cada paquete que instala bibliotecas en rutas no
estándar agrega un archivo en `/etc/ld.so.conf.d/` (por ejemplo,
`x86_64-linux-gnu.conf` en Debian con multiarch).

El orden de carga es alfabético: un archivo `10-defaults.conf` se carga antes
que `99-override.conf`. Esto permite que configuraciones con prefijo alto
sobrescriban valores de prefijos bajos.

`/etc/sudoers.d/` puede estar vacío o contener un archivo creado en el
Dockerfile para dar permisos sin contraseña al usuario de desarrollo.

</details>

---

### Ejercicio 6 — Comparar identificación entre distribuciones

Verifica las diferentes formas de identificar cada distribución.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
echo "os-release:"
grep -E "^(ID|VERSION_ID|PRETTY_NAME)=" /etc/os-release

echo ""
echo "debian_version:"
cat /etc/debian_version

echo ""
echo "redhat-release:"
cat /etc/redhat-release 2>/dev/null || echo "  no existe (normal en Debian)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
echo "os-release:"
grep -E "^(ID|VERSION_ID|PRETTY_NAME)=" /etc/os-release

echo ""
echo "redhat-release:"
cat /etc/redhat-release

echo ""
echo "debian_version:"
cat /etc/debian_version 2>/dev/null || echo "  no existe (normal en RHEL)"
'
```

<details>
<summary>Predicción</summary>

**Debian** muestra:
- `ID=debian`, `VERSION_ID="12"`, `PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"`
- `/etc/debian_version`: `12.x` (versión puntual)
- `/etc/redhat-release`: no existe

**AlmaLinux** muestra:
- `ID="almalinux"`, `VERSION_ID="9.x"`, `PRETTY_NAME="AlmaLinux 9.x (...)"`
- `/etc/redhat-release`: `AlmaLinux release 9.x (...)`
- `/etc/debian_version`: no existe

`/etc/os-release` es el método **portable** (funciona en todas las distros
modernas, definido por systemd). Los archivos específicos (`debian_version`,
`redhat-release`) son legacy pero siguen presentes.

En scripts que necesitan detectar la distribución:
```bash
source /etc/os-release
if [ "$ID" = "debian" ]; then ...
```

</details>

---

### Ejercicio 7 — Comparar configuración de repositorios

Observa cómo cada distribución gestiona sus fuentes de paquetes.

```bash
echo "=== Debian: APT ==="
docker compose exec -T debian-dev bash -c '
echo "/etc/apt/ contiene:"
ls /etc/apt/

echo ""
echo "sources.list:"
cat /etc/apt/sources.list 2>/dev/null | grep -v "^#" | grep -v "^$" || echo "  (vacío o no existe)"

echo ""
echo "sources.list.d/:"
ls /etc/apt/sources.list.d/ 2>/dev/null || echo "  (vacío)"
'

echo ""
echo "=== AlmaLinux: DNF/YUM ==="
docker compose exec -T alma-dev bash -c '
echo "/etc/yum.repos.d/ contiene:"
ls /etc/yum.repos.d/

echo ""
echo "Primer repo (primeras líneas):"
head -8 /etc/yum.repos.d/*.repo 2>/dev/null | head -15
'
```

<details>
<summary>Predicción</summary>

**Debian** organiza repos en:
- `/etc/apt/sources.list` — archivo principal (puede estar vacío en imágenes
  Docker si se usa `sources.list.d/`)
- `/etc/apt/sources.list.d/` — fragmentos (patrón `.d/`)

El formato es: `deb http://url distro componentes`

**AlmaLinux** organiza repos en:
- `/etc/yum.repos.d/` — cada archivo `.repo` define uno o más repositorios

El formato es INI:
```ini
[baseos]
name=AlmaLinux $releasever - BaseOS
baseurl=https://...
enabled=1
gpgcheck=1
```

Ambas usan el patrón de directorio con fragmentos, pero con formatos
diferentes. En Debian, `apt-get update` descarga los índices. En AlmaLinux,
`dnf makecache` hace lo equivalente.

</details>

---

### Ejercicio 8 — Comparar configuración de locale

Verifica cómo cada distribución configura el idioma y la localización.

```bash
echo "=== Debian ==="
docker compose exec -T debian-dev bash -c '
echo "Locale config file:"
cat /etc/default/locale 2>/dev/null || echo "  /etc/default/locale no existe"
echo ""
echo "Variable LANG:"
echo "  LANG=$LANG"
echo ""
echo "Locales disponibles:"
locale -a 2>/dev/null | head -5
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec -T alma-dev bash -c '
echo "Locale config file:"
cat /etc/locale.conf 2>/dev/null || echo "  /etc/locale.conf no existe"
echo ""
echo "Variable LANG:"
echo "  LANG=$LANG"
echo ""
echo "Locales disponibles:"
locale -a 2>/dev/null | head -5
'
```

<details>
<summary>Predicción</summary>

**Debian** usa `/etc/default/locale` para definir `LANG` y variables
relacionadas. En un contenedor, puede no existir si no se configuró un locale
específico. La variable `LANG` puede estar vacía o ser `C.UTF-8` / `POSIX`.

**AlmaLinux** usa `/etc/locale.conf`. Similar al caso de Debian, en
contenedores suele estar configurado mínimamente.

`locale -a` lista los locales instalados. En contenedores mínimos puede haber
solo `C`, `C.utf8`, y `POSIX`. En sistemas completos hay cientos
(`en_US.utf8`, `es_ES.utf8`, etc.).

Las rutas diferentes (`/etc/default/locale` vs `/etc/locale.conf`) son un
ejemplo de cómo la misma funcionalidad se configura en diferentes archivos
según la familia de distribuciones. `localectl` es la herramienta portable
de systemd que funciona en ambas.

</details>

---

### Ejercicio 9 — Explorar overrides de systemd

Entiende la relación entre configuración por defecto y overrides del
administrador.

```bash
docker compose exec -T debian-dev bash -c '
echo "=== Defaults del paquete (no editar) ==="
echo "Ruta: /usr/lib/systemd/system/"
ls /usr/lib/systemd/system/*.service 2>/dev/null | head -5 || echo "  (no hay servicios)"

echo ""
echo "=== Overrides del administrador ==="
echo "Ruta: /etc/systemd/system/"
ls /etc/systemd/system/ 2>/dev/null | head -10

echo ""
echo "=== Ejemplo: buscar un servicio ==="
service="ssh"
echo "Default:"
ls /usr/lib/systemd/system/${service}* 2>/dev/null || echo "  no encontrado"
echo "Override:"
ls /etc/systemd/system/${service}* 2>/dev/null || echo "  no hay override"
ls -la /etc/systemd/system/${service}*.d/ 2>/dev/null || echo "  no hay drop-in"
'
```

<details>
<summary>Predicción</summary>

`/usr/lib/systemd/system/` contiene los unit files instalados por los paquetes.
Estos son los "defaults" y no deben editarse directamente porque las
actualizaciones los sobrescriben.

`/etc/systemd/system/` contiene los overrides del administrador. Puede tener:
- Unit files completos (reemplazo total del default)
- Directorios `.d/` con drop-ins (override parcial, preferido)
- Symlinks a `/dev/null` (para deshabilitar un servicio)

En un contenedor Docker, `/etc/systemd/system/` puede tener pocos archivos
porque los servicios se gestionan de forma diferente (Docker no usa systemd
como init por defecto).

La recomendación: usar `systemctl edit <servicio>` para crear drop-ins en
`/etc/systemd/system/<servicio>.service.d/override.conf`. Esto modifica solo
los campos específicos sin reemplazar todo el archivo.

</details>

---

### Ejercicio 10 — Archivos comunes vs específicos entre distros

Verifica qué archivos fundamentales existen en ambas distribuciones y cuáles
son exclusivos.

```bash
echo "=== Archivos comunes del FHS ==="
for f in hostname hosts passwd group shadow resolv.conf nsswitch.conf os-release fstab shells; do
    deb=$(docker compose exec -T debian-dev test -f /etc/$f && echo "✓" || echo "✗")
    alma=$(docker compose exec -T alma-dev test -f /etc/$f && echo "✓" || echo "✗")
    printf "  /etc/%-20s Debian: %s  AlmaLinux: %s\n" "$f" "$deb" "$alma"
done

echo ""
echo "=== Archivos específicos ==="
echo "Solo Debian:"
for f in debian_version apt/sources.list default/locale dpkg/; do
    docker compose exec -T debian-dev test -e /etc/$f && echo "  /etc/$f" || true
done

echo ""
echo "Solo AlmaLinux:"
for f in redhat-release yum.repos.d/ locale.conf rpm/; do
    docker compose exec -T alma-dev test -e /etc/$f && echo "  /etc/$f" || true
done
```

<details>
<summary>Predicción</summary>

Los archivos comunes muestran ✓ en ambas columnas:
- `hostname`, `hosts`, `passwd`, `group`, `shadow`, `resolv.conf`,
  `nsswitch.conf`, `os-release`, `shells` — todos presentes en ambas

`fstab` puede no existir en contenedores Docker (los contenedores no montan
filesystems al arranque — el runtime se encarga).

Los archivos específicos:
- **Debian**: `/etc/debian_version`, `/etc/apt/sources.list`,
  `/etc/default/locale`, `/etc/dpkg/`
- **AlmaLinux**: `/etc/redhat-release`, `/etc/yum.repos.d/`,
  `/etc/locale.conf`, `/etc/rpm/`

El patrón: los archivos del FHS (definidos por el estándar) son comunes a
todas las distribuciones. Los archivos específicos son los del gestor de
paquetes (apt vs dnf), la configuración de locale (paths diferentes), y los
archivos de identificación de la distro.

</details>

---

## Resumen de conceptos

| Concepto | Detalle clave |
|---|---|
| `/etc` | Configuración única por máquina; texto plano editable |
| Patrón `.d/` | Configuración modular; orden alfabético; evita conflictos |
| `/etc/hostname` | Nombre del host; cambiar con `hostnamectl` |
| `/etc/hosts` | Resolución estática; se consulta antes que DNS |
| `/etc/nsswitch.conf` | Define el orden de búsqueda: `files dns` |
| `/etc/passwd` + `/etc/shadow` | Usuarios públicos + hashes privados |
| `/etc/sudoers` | Editar solo con `visudo`; fragmentos en `.d/` |
| `/etc/os-release` | Forma portable de identificar la distribución |
| `/etc/fstab` | Tabla de montajes al arranque |
| `/etc` vs `/usr/lib` | Admin overrides vs defaults del paquete |
