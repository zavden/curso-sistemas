# T02 — Repositorios (DNF/YUM)

## Errata respecto a las fuentes originales

1. **`rpm -Va` como "verificar firma de todos los .rpm"** (README.max.md):
   confuso por su ubicacion en el texto. `rpm -Va` verifica **integridad
   de archivos** (como `dpkg -V`), no firmas. `rpm -K file.rpm` verifica
   firmas GPG.
2. **URL `https://repo.almalinux.org/9/BaseOS/...`** (README.max.md):
   falta `/almalinux/` en la ruta. Correcto:
   `https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/`.
3. **`dnf distrosync` como equivalente de `apt full-upgrade` para major
   upgrade** (README.max.md): impreciso. `dnf distro-sync` sincroniza
   paquetes a la version disponible en repos, pero no maneja el cambio
   de release. Para un major upgrade en RHEL se editan los repos
   (cambiar `$releasever`) y luego `dnf upgrade`.

---

## Anatomia del sistema de repositorios

```
/etc/yum.repos.d/              <- archivos .repo (configuracion)
+-- almalinux-baseos.repo
+-- almalinux-appstream.repo
+-- almalinux-extras.repo
+-- epel.repo                  (si esta instalado)
+-- *.repo                     (repos de terceros)

/var/cache/dnf/                <- cache de metadata y .rpm
+-- baseos-*/repodata/         <- metadata descomprimida
+-- *.solv                     <- indice de libsolv (compilado)

/etc/pki/rpm-gpg/              <- claves GPG publicas
+-- RPM-GPG-KEY-AlmaLinux-9
+-- RPM-GPG-KEY-EPEL-9
```

### Flujo de dnf install

```
dnf install nginx
  |
  +--[1] Lee /etc/yum.repos.d/*.repo
  |      Busca nginx en repos habilitados
  +--[2] libsolv resuelve dependencias
  +--[3] Descarga .rpm → /var/cache/dnf/
  +--[4] Llama a rpm internamente para instalar
```

---

## Archivos .repo (formato INI)

```bash
ls /etc/yum.repos.d/
```

```ini
# /etc/yum.repos.d/almalinux-baseos.repo
[baseos]                                    # ID unico del repo
name=AlmaLinux $releasever - BaseOS         # Nombre descriptivo
mirrorlist=https://mirrors.almalinux.org/mirrorlist/$releasever/baseos
# baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/
enabled=1                                   # 1=habilitado, 0=deshabilitado
gpgcheck=1                                  # Verificar firma GPG
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-AlmaLinux-9
metadata_expire=86400                       # Re-descargar metadata cada 24h
countme=1                                   # Contador anonimo para estadisticas
```

### Campos del archivo .repo

| Campo | Significado |
|---|---|
| `[ID]` | Identificador unico usado en comandos dnf |
| `name` | Nombre descriptivo |
| `baseurl` | URL fija directa al repositorio |
| `mirrorlist` | URL que devuelve una lista de mirrors |
| `metalink` | Lista de mirrors + checksums de metadatos |
| `enabled` | 1 = activo, 0 = desactivado |
| `gpgcheck` | 1 = verificar firmas GPG de paquetes |
| `gpgkey` | Ruta a la clave GPG publica |
| `metadata_expire` | Segundos antes de re-descargar metadata |
| `priority` | Prioridad (menor = mas importante, default 99) |
| `module_hotfixes` | 1 = ignorar filtros de module stream |
| `sslverify` | 1 = verificar certificados SSL |
| `skip_if_unavailable` | 1 = no fallar si el repo no responde |

### Variables en archivos .repo

```bash
# Se resuelven automaticamente:
$releasever    # version de la distro (9, 8)
$basearch      # arquitectura (x86_64, aarch64)
$arch          # igual a $basearch
$infra         # tipo de infraestructura (stock, container)

# Ver como se resuelven:
rpm -E %{rhel}        # 9
rpm -E %{_arch}       # x86_64
rpm -E %{dist}        # .el9

# Ejemplo:
# baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/
# Se resuelve a:
# baseurl=https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/
```

### baseurl vs mirrorlist vs metalink

| Tipo | Ventaja | Desventaja |
|---|---|---|
| `baseurl` | Control total, URL fija | Si el mirror cae, no hay fallback |
| `mirrorlist` | Failover automatico, elige el mas rapido | Depende del servicio de mirrors |
| `metalink` | Failover + verificacion de integridad | Mas complejo |

```bash
# baseurl: URL fija a un solo servidor
baseurl=https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/

# mirrorlist: dnf elige el mirror mas rapido
mirrorlist=https://mirrors.almalinux.org/mirrorlist/9/baseos

# metalink: lista de mirrors + checksums de cada archivo
metalink=https://mirrors.fedoraproject.org/metalink?repo=epel-9&arch=x86_64
```

En produccion, `mirrorlist` o `metalink` son preferibles por redundancia
y seleccion automatica del mirror mas rapido.

---

## Repositorios estandar de AlmaLinux/RHEL 9

| Repo ID | Contenido | Por defecto |
|---|---|---|
| `baseos` | Paquetes core: kernel, glibc, systemd, bash, openssh | Habilitado |
| `appstream` | Aplicaciones: nginx, python, nodejs, php, postgresql | Habilitado |
| `crb` | CodeReady Builder: headers, libs de desarrollo, cmake | Deshabilitado |
| `extras` | Paquetes adicionales de AlmaLinux | Habilitado |
| `highavailability` | Clustering: pacemaker, corosync | Deshabilitado |
| `rt` | Kernel real-time (PREEMPT_RT) | Deshabilitado |

```bash
# Ver repos habilitados
dnf repolist

# Ver todos (habilitados + deshabilitados)
dnf repolist --all

# Habilitar un repo
sudo dnf config-manager --set-enabled crb

# Deshabilitar un repo
sudo dnf config-manager --set-disabled extras

# Info detallada de un repo
dnf repoinfo baseos
```

### BaseOS vs AppStream

```
BaseOS:
  - Paquetes CORE del sistema operativo
  - Solo RPMs tradicionales (no modulos)
  - Soporte: 10 anos en RHEL
  - Actualizaciones: solo security/bugfixes, no features nuevas
  - Ejemplos: kernel, bash, systemd, openssh, openssl

AppStream:
  - Aplicaciones, herramientas, lenguajes
  - RPMs tradicionales + module streams
  - Puede tener MULTIPLES VERSIONES simultaneas
  - Ejemplos: nginx, postgresql, python, nodejs, php
```

### Module streams (AppStream)

Los module streams permiten tener **multiples versiones** de un software
disponibles en el mismo repositorio:

```bash
# Ver modulos disponibles
dnf module list

# Ver versiones de nodejs
dnf module list nodejs
# nodejs:18   [d]    <- [d] = default
# nodejs:20

# Instalar una version especifica
sudo dnf module install nodejs:20

# Resetear a la default
sudo dnf module reset nodejs
```

### CRB (CodeReady Builder)

```bash
# CRB contiene paquetes de desarrollo:
# headers, librerias de desarrollo, cmake, gcc extras
# Necesario para compilar software desde source
# Muchos paquetes de EPEL dependen de CRB

# Habilitar CRB
sudo dnf config-manager --set-enabled crb
```

---

## Claves GPG

```bash
# Ubicacion de claves publicas
ls /etc/pki/rpm-gpg/
# RPM-GPG-KEY-AlmaLinux-9

# Las claves importadas aparecen como "paquetes" en rpm
rpm -qa gpg-pubkey
# gpg-pubkey-e45234a5-5b03ddb6  (AlmaLinux 9)

# Ver informacion de una clave importada
rpm -qi gpg-pubkey-e45234a5

# Importar una clave manualmente
sudo rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-EPEL-9
sudo rpm --import https://www.example.com/RPM-GPG-KEY

# Verificar FIRMA de un .rpm (antes de instalar)
rpm -K nginx-1.22.1-9.el9.x86_64.rpm
# digests signatures OK

# Verificar INTEGRIDAD de archivos instalados (como dpkg -V)
rpm -V nginx
# (sin output = todo OK)
rpm -Va       # verificar TODOS los paquetes (lento)
```

**Error comun**: `Public key for package.rpm is not installed`

```bash
# Ocurre cuando gpgcheck=1 pero la clave del repo no esta importada

# Solucion 1: instalar el paquete de release (trae la clave)
sudo dnf install epel-release

# Solucion 2: importar la clave directamente
sudo rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-EPEL-9
```

---

## EPEL — Extra Packages for Enterprise Linux

EPEL es el repositorio de la comunidad Fedora que proporciona paquetes
adicionales para RHEL/AlmaLinux que no estan en los repos oficiales.
Contiene paquetes que en Debian estarian en main/universe.

```bash
# Instalar EPEL (en AlmaLinux esta en Extras)
sudo dnf install epel-release

# En RHEL puro:
# sudo dnf install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm

# Verificar
dnf repolist | grep epel

# Cuantos paquetes tiene EPEL
dnf repoinfo epel | grep "Repo-pkgs"
```

### EPEL + CRB

Muchos paquetes de EPEL dependen de paquetes en CRB. Es comun
habilitarlos juntos:

```bash
sudo dnf config-manager --set-enabled crb
sudo dnf install epel-release

# Ahora disponible:
sudo dnf install htop jq bat ripgrep tmux
```

---

## COPR — Community Projects

COPR es el equivalente de los PPAs de Ubuntu para Fedora/RHEL:

```bash
# Habilitar un proyecto COPR
sudo dnf copr enable user/project

# Ejemplo: starship (prompt moderno)
sudo dnf copr enable atim/starship
sudo dnf install starship

# Deshabilitar
sudo dnf copr disable atim/starship

# Listar COPRs habilitados
dnf copr list
```

---

## Habilitar y deshabilitar repos

```bash
# Permanente: modifica el .repo
sudo dnf config-manager --set-enabled crb
sudo dnf config-manager --set-disabled epel

# Temporal: solo para un comando (no modifica archivos)
sudo dnf install --enablerepo=crb package-name
sudo dnf update --disablerepo=epel

# Util cuando un mirror esta caido:
sudo dnf update --disablerepo=epel
```

---

## Agregar un repositorio de terceros

### Metodo 1: dnf config-manager

```bash
sudo dnf config-manager --add-repo \
    https://download.docker.com/linux/centos/docker-ce.repo
```

### Metodo 2: crear .repo manualmente

```bash
sudo tee /etc/yum.repos.d/docker-ce.repo << 'EOF'
[docker-ce-stable]
name=Docker CE Stable - $basearch
baseurl=https://download.docker.com/linux/centos/$releasever/$basearch/stable
enabled=1
gpgcheck=1
gpgkey=https://download.docker.com/linux/centos/gpg
EOF

sudo dnf makecache
```

### Eliminar un repositorio de terceros

```bash
# 1. Eliminar el archivo .repo
sudo rm /etc/yum.repos.d/docker-ce.repo

# 2. Limpiar cache
sudo dnf clean all

# 3. Verificar
dnf repolist | grep docker
# (no deberia aparecer)
```

---

## Prioridades de repositorios

Cuando un paquete existe en varios repos, se usa el de **menor
priority number** (mas prioritario):

```ini
[mi-repo-interno]
name=Internal Repo
baseurl=https://interno.company.com/repo/
enabled=1
gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/RPM-GPG-KEY-interno
priority=10    # 1-99, menor = mas prioritario (default = 99)
```

```bash
# Ejemplo: preferir version de repo interno sobre EPEL
# interno: priority=10   ← se prefiere
# epel:    priority=99   ← default, menos prioritario
# Si nginx existe en ambos → se instala desde interno
```

---

## Diagnostico de problemas

### "Failed to download metadata"

```bash
# 1. Verificar conectividad al repo
curl -I https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/repodata/repomd.xml

# 2. Deshabilitar temporalmente el repo problematico
sudo dnf update --disablerepo=nombre-repo

# 3. Limpiar cache y reintentar
sudo dnf clean all
sudo dnf makecache
```

### "GPG key is not installed"

```bash
# 1. Ver claves importadas
rpm -qa gpg-pubkey

# 2. Importar la clave del repo
sudo rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-nombre
```

### "Status code: 404"

```bash
# La URL del repo es incorrecta
# Verificar $releasever y $basearch en el .repo
cat /etc/yum.repos.d/problema.repo
rpm -E %{rhel}    # deberia coincidir con $releasever
```

---

## Comparacion con APT

| Aspecto | APT (Debian) | DNF (RHEL) |
|---|---|---|
| Directorio de config | `/etc/apt/sources.list.d/` | `/etc/yum.repos.d/` |
| Formato | One-line o DEB822 | INI (secciones `[id]`) |
| Claves GPG | `/usr/share/keyrings/` (Signed-By) | `/etc/pki/rpm-gpg/` (gpgkey) |
| Repos comunitarios | PPA (Ubuntu/Launchpad) | EPEL + COPR |
| Prioridades | `Pin-Priority` en preferences.d | `priority=N` en .repo |
| Mirrors automaticos | No (config manual) | Si (mirrorlist, metalink) |
| Modules/streams | No | Si (`dnf module`) |
| Refresh metadata | `apt update` (manual, obligatorio) | Automatico (cache con expiracion) |
| Verificar firma paquete | InRelease / Release.gpg | `rpm -K file.rpm` |
| Verificar archivos | `dpkg -V` | `rpm -V` |

---

## Ejercicios

### Ejercicio 1 — Explorar archivos .repo

Examina la configuracion de repos del sistema.

```bash
# 1. Listar archivos .repo
ls /etc/yum.repos.d/

# 2. Ver repos habilitados
dnf repolist
```

**Pregunta**: Los repos usan `baseurl`, `mirrorlist`, o `metalink`?
Cual es la ventaja de cada uno?

<details><summary>Prediccion</summary>

La mayoria de repos de AlmaLinux usan `mirrorlist` por defecto.

- `baseurl`: URL fija — control total pero sin failover
- `mirrorlist`: lista de mirrors — dnf elige el mas rapido, failover
  automatico si uno cae
- `metalink`: como mirrorlist + checksums — agrega verificacion de
  integridad de los metadatos

```bash
# 3. Verificar que metodo usa cada repo
for repo in /etc/yum.repos.d/*.repo; do
    name=$(basename "$repo")
    method="desconocido"
    grep -q "mirrorlist" "$repo" 2>/dev/null && method="mirrorlist"
    grep -q "metalink" "$repo" 2>/dev/null && method="metalink"
    grep -q "^baseurl" "$repo" 2>/dev/null && method="baseurl"
    echo "$name: $method"
done

# 4. Ver el contenido de un .repo
cat /etc/yum.repos.d/$(ls /etc/yum.repos.d/ | head -1)
```

En produccion, `mirrorlist`/`metalink` son preferibles. `baseurl` se usa
para repos internos de empresa (un solo servidor conocido).

</details>

---

### Ejercicio 2 — Variables en .repo

Entiende como se resuelven `$releasever` y `$basearch`.

```bash
# 1. Ver valores actuales
rpm -E %{rhel}
rpm -E %{_arch}
rpm -E %{dist}
```

**Pregunta**: Si un .repo dice
`baseurl=https://repo.almalinux.org/almalinux/$releasever/BaseOS/$basearch/os/`,
a que URL se resuelve?

<details><summary>Prediccion</summary>

Se resuelve a:
`https://repo.almalinux.org/almalinux/9/BaseOS/x86_64/os/`

Las variables se sustituyen automaticamente:
- `$releasever` → `9` (version de la release)
- `$basearch` → `x86_64` (arquitectura)

```bash
# 2. Verificar que la URL resuelta funciona
curl -sI "https://repo.almalinux.org/almalinux/$(rpm -E %{rhel})/BaseOS/$(uname -m)/os/repodata/repomd.xml" | head -3

# 3. Ver que contiene repodata/
# repomd.xml es el indice maestro del repo (como InRelease en APT)
```

Estas variables permiten que el mismo archivo `.repo` funcione en
diferentes versiones de AlmaLinux (8, 9) y arquitecturas (x86_64,
aarch64) sin modificacion.

</details>

---

### Ejercicio 3 — BaseOS vs AppStream

Compara los dos repositorios principales.

```bash
# 1. Paquetes en cada repo
dnf repoinfo baseos 2>/dev/null | grep "Repo-pkgs"
dnf repoinfo appstream 2>/dev/null | grep "Repo-pkgs"
```

**Pregunta**: En que repo esta `bash`? Y `nginx`? Por que estan separados?

<details><summary>Prediccion</summary>

- `bash` esta en **BaseOS** — es un paquete core del sistema
- `nginx` esta en **AppStream** — es una aplicacion

Estan separados porque tienen ciclos de vida distintos:
- BaseOS: paquetes estables, solo reciben security/bugfixes, sin
  features nuevas durante 10 anos
- AppStream: aplicaciones que pueden tener multiples versiones via
  module streams (ej: python 3.9, 3.11, 3.12)

```bash
# 2. Verificar
dnf info bash 2>/dev/null | grep "Repository"
dnf info nginx 2>/dev/null | grep "Repository" || echo "(nginx no instalado)"

# 3. Buscar paquetes en repos especificos
dnf repoquery --repoid=baseos bash
dnf repoquery --repoid=appstream nginx

# 4. Module streams en AppStream
dnf module list 2>/dev/null | head -15
```

</details>

---

### Ejercicio 4 — EPEL y CRB

Explora los repos adicionales mas importantes.

```bash
# 1. Verificar si EPEL esta instalado
rpm -q epel-release 2>/dev/null || echo "EPEL no instalado"

# 2. Verificar estado de CRB
dnf repolist --all 2>/dev/null | grep -i crb
```

**Pregunta**: Por que se habilitan CRB y EPEL juntos?

<details><summary>Prediccion</summary>

Muchos paquetes de EPEL dependen de paquetes de desarrollo (headers,
librerias) que estan en CRB. Sin CRB habilitado, `dnf install` de
un paquete EPEL puede fallar por dependencias faltantes.

```bash
# 3. Habilitar CRB + instalar EPEL (si no esta)
# sudo dnf config-manager --set-enabled crb
# sudo dnf install epel-release

# 4. Si EPEL esta instalado, ver cuantos paquetes tiene
dnf repoinfo epel 2>/dev/null | grep "Repo-pkgs"

# 5. Verificar que htop (tipico paquete EPEL) esta disponible
dnf info htop 2>/dev/null | grep "Repository"
```

EPEL es el equivalente de los repositorios `universe` de Ubuntu — software
mantenido por la comunidad, no por Red Hat directamente.

</details>

---

### Ejercicio 5 — Claves GPG

Explora la autenticacion GPG de paquetes RPM.

```bash
# 1. Ver claves GPG instaladas
ls /etc/pki/rpm-gpg/

# 2. Ver claves importadas en rpm
rpm -qa gpg-pubkey
```

**Pregunta**: Cual es la diferencia entre `rpm -K file.rpm` y `rpm -V pkg`?

<details><summary>Prediccion</summary>

- `rpm -K file.rpm`: verifica la **firma GPG** de un archivo `.rpm`
  antes de instalarlo — confirma que el paquete fue firmado por quien
  dice ser y no fue modificado en transito
- `rpm -V pkg`: verifica la **integridad de archivos** de un paquete
  ya instalado — compara los archivos en disco contra los checksums
  originales del paquete (como `dpkg -V`)

Son verificaciones en momentos distintos:
- `rpm -K`: **antes** de instalar (autenticidad del `.rpm`)
- `rpm -V`: **despues** de instalar (integridad de los archivos)

```bash
# 3. Ver informacion de una clave
rpm -qi $(rpm -qa gpg-pubkey | head -1) 2>/dev/null

# 4. Verificar gpgcheck en dnf.conf
grep gpgcheck /etc/dnf/dnf.conf

# 5. Verificar integridad de bash
rpm -V bash
# (sin output = todo OK)
```

</details>

---

### Ejercicio 6 — Habilitar/deshabilitar repos temporalmente

Practica el uso temporal de repos sin modificar archivos.

```bash
# 1. Ver repos actuales
dnf repolist
```

**Pregunta**: Cual es la diferencia entre `--set-enabled` y `--enablerepo`?

<details><summary>Prediccion</summary>

- `dnf config-manager --set-enabled repo`: **permanente** — modifica el
  campo `enabled=1` en el archivo `.repo`
- `dnf install --enablerepo=repo pkg`: **temporal** — habilita el repo
  solo para ese comando, no modifica archivos

```bash
# 2. Temporal: instalar desde un repo deshabilitado
# sudo dnf install --enablerepo=crb cmake

# 3. Temporal: ignorar un repo problematico
# sudo dnf update --disablerepo=epel

# 4. Permanente: deshabilitar y re-habilitar
# sudo dnf config-manager --set-disabled extras
# dnf repolist | grep extras    # no aparece
# sudo dnf config-manager --set-enabled extras
# dnf repolist | grep extras    # aparece
```

`--enablerepo`/`--disablerepo` son utiles cuando un mirror esta caido
o cuando necesitas un paquete de un repo que normalmente no usas.

</details>

---

### Ejercicio 7 — Agregar y remover un repo de terceros

Practica el flujo completo de agregar un repositorio externo.

```bash
# 1. Metodo: dnf config-manager
# sudo dnf config-manager --add-repo \
#     https://download.docker.com/linux/centos/docker-ce.repo
```

**Pregunta**: Que pasos son necesarios para agregar un repo de terceros
de forma segura?

<details><summary>Prediccion</summary>

Pasos para agregar un repo de forma segura:

1. **Obtener el archivo .repo** o la URL del repo
2. **Importar la clave GPG** del proveedor
3. **Verificar que `gpgcheck=1`** en el archivo .repo
4. **Regenerar cache**: `dnf makecache`
5. **Verificar**: `dnf repolist | grep nombre`

```bash
# Ejemplo completo: Docker en AlmaLinux 9
# sudo dnf install -y dnf-plugins-core
# sudo dnf config-manager --add-repo \
#     https://download.docker.com/linux/centos/docker-ce.repo
# dnf repolist | grep docker
# sudo dnf install docker-ce

# Para remover:
# sudo rm /etc/yum.repos.d/docker-ce.repo
# sudo dnf clean all
# dnf repolist | grep docker    # no aparece

# 2. Ver el .repo que se crearia
cat <<'REPO'
[docker-ce-stable]
name=Docker CE Stable - $basearch
baseurl=https://download.docker.com/linux/centos/$releasever/$basearch/stable
enabled=1
gpgcheck=1
gpgkey=https://download.docker.com/linux/centos/gpg
REPO
```

Nunca usar `gpgcheck=0` en produccion — permite instalar paquetes no
firmados, riesgo de seguridad.

</details>

---

### Ejercicio 8 — Prioridades de repos

Entiende como dnf elige entre multiples repos con el mismo paquete.

```bash
# 1. Ver si algun repo tiene prioridad configurada
grep -r "priority" /etc/yum.repos.d/ 2>/dev/null
```

**Pregunta**: Si nginx esta en EPEL (priority=99) y en un repo interno
(priority=10), cual se instala? Y si no tienen priority configurado?

<details><summary>Prediccion</summary>

- Con prioridades: se instala desde el repo interno (priority=10 <
  priority=99, menor = mas prioritario)
- Sin priority configurado: ambos tienen default=99, y dnf elige la
  **version mas nueva** disponible entre todos los repos

```bash
# 2. Ver de que repo viene un paquete
dnf info bash 2>/dev/null | grep -E "Repository|Version"

# 3. Ver todas las versiones disponibles de un paquete
dnf repoquery --show-duplicates bash 2>/dev/null | head -5

# 4. Ejemplo de .repo con prioridad
cat <<'REPO'
[mi-repo]
name=Internal Repo
baseurl=https://interno.company.com/repo/
enabled=1
gpgcheck=1
priority=10    # mas prioritario que default (99)
REPO
```

Las prioridades son importantes cuando tienes repos internos de empresa
que deben prevalecer sobre repos publicos como EPEL.

</details>

---

### Ejercicio 9 — Comparacion con repos APT

Mapea los conceptos de repos APT a DNF.

```bash
cat <<'EOF'
APT (Debian/Ubuntu)              DNF (RHEL/AlmaLinux)
-------------------              --------------------
/etc/apt/sources.list.d/   →    /etc/yum.repos.d/
.list / .sources           →    .repo (formato INI)
deb http://... suite comp  →    [id] + baseurl/mirrorlist
/usr/share/keyrings/       →    /etc/pki/rpm-gpg/
Signed-By=                 →    gpgkey=
apt update                 →    dnf makecache (o automatico)
PPAs (Ubuntu)              →    EPEL + COPR
Pin-Priority               →    priority=
InRelease                  →    repomd.xml
dists/ + pool/             →    repodata/ + Packages/
EOF
```

**Pregunta**: Que ventaja tiene el sistema de mirrors de DNF sobre APT?

<details><summary>Prediccion</summary>

DNF tiene `mirrorlist` y `metalink` nativos que:
- Seleccionan automaticamente el mirror mas rapido
- Hacen failover si un mirror cae
- metalink ademas verifica integridad de metadatos

APT no tiene seleccion automatica de mirrors integrada. Se configura
un mirror fijo en `sources.list` y si cae, hay que cambiar manualmente
(o usar `apt-transport-mirror` que no es comun).

```bash
# Ver que mirror esta usando dnf actualmente
dnf repolist -v 2>/dev/null | grep -i "baseurl\|mirror" | head -5
```

La desventaja de DNF: el formato INI de `.repo` es mas verboso que el
one-line de APT. Y la separacion BaseOS/AppStream puede confundir al
principio (APT tiene todo en `main`/`universe`).

</details>

---

### Ejercicio 10 — Panorama: de repo a paquete instalado

Reconstruye el flujo completo desde la configuracion del repo hasta
el paquete funcionando en el sistema.

```bash
cat <<'EOF'
FLUJO COMPLETO:

  /etc/yum.repos.d/*.repo      Configuracion de repos
       |
  dnf makecache                 Descargar metadata (o automatico)
       |
  /var/cache/dnf/*/repodata/   Metadata cacheada localmente
       |                        (repomd.xml, primary.xml.gz, etc.)
  dnf install pkg               Buscar en metadata → libsolv resuelve deps
       |
  Descarga .rpm                 Verificar firma GPG (rpm -K)
       |
  rpm -i (interno)              Instalar archivos en el filesystem
       |
  /var/lib/rpm/                 Base de datos de paquetes instalados
EOF
```

**Pregunta**: Que diferencias hay en este flujo comparado con APT?

<details><summary>Prediccion</summary>

| Paso | APT | DNF |
|---|---|---|
| Config repos | `sources.list.d/` (one-line/DEB822) | `yum.repos.d/` (INI) |
| Refresh metadata | `apt update` (manual, obligatorio) | Automatico (cache con `metadata_expire`) |
| Metadata local | `/var/lib/apt/lists/` | `/var/cache/dnf/*/repodata/` |
| Resolver deps | libapt-pkg | libsolv (SAT solver) |
| Verificar firma | InRelease/Release.gpg (del repo) | `rpm -K` (por paquete) |
| Instalar | `dpkg -i` (interno) | `rpm -i` (interno) |
| DB paquetes | `/var/lib/dpkg/status` (texto) | `/var/lib/rpm/` (BerkeleyDB/SQLite) |
| Formato paquete | `.deb` (ar archive) | `.rpm` (cpio + headers) |

La diferencia mas visible: APT requiere `apt update` explicito antes de
instalar, DNF no (aunque tiene `dnf makecache` para forzar).

La diferencia tecnica mas importante: libsolv (SAT solver) en dnf
garantiza encontrar la solucion optima si existe.

</details>
