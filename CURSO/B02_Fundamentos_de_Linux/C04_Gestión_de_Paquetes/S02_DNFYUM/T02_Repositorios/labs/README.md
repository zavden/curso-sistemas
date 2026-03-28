# Lab — Repositorios

## Objetivo

Explorar la configuracion de repositorios en RHEL/AlmaLinux:
archivos .repo y su formato INI, los repositorios estandar
(BaseOS, AppStream, CRB), EPEL, claves GPG, y la gestion de
repos con dnf config-manager.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Archivos .repo

### Objetivo

Entender la estructura y campos de los archivos de repositorio.

### Paso 1.1: Directorio de repos

```bash
docker compose exec alma-dev bash -c '
echo "=== /etc/yum.repos.d/ ==="
ls -la /etc/yum.repos.d/
echo ""
echo "Cada archivo .repo puede contener uno o mas repositorios"
echo "Formato: INI con secciones [repo-id]"
'
```

### Paso 1.2: Anatomia de un .repo

```bash
docker compose exec alma-dev bash -c '
echo "=== Contenido de un .repo ==="
repo=$(ls /etc/yum.repos.d/*baseos* 2>/dev/null | head -1)
if [ -f "$repo" ]; then
    echo "--- $(basename $repo) ---"
    cat "$repo"
else
    echo "(buscando cualquier .repo)"
    repo=$(ls /etc/yum.repos.d/*.repo 2>/dev/null | head -1)
    if [ -f "$repo" ]; then
        echo "--- $(basename $repo) ---"
        head -20 "$repo"
    fi
fi
echo ""
echo "--- Campos principales ---"
echo "[repo-id]:        ID unico (usado en comandos dnf)"
echo "name:             nombre descriptivo"
echo "baseurl:          URL directa al repositorio"
echo "mirrorlist:       URL que devuelve lista de mirrors"
echo "metalink:         URL con metadatos + checksums"
echo "enabled:          1 = activo, 0 = desactivado"
echo "gpgcheck:         1 = verificar firmas GPG"
echo "gpgkey:           ruta a la clave GPG"
echo "metadata_expire:  cuando re-descargar metadatos"
'
```

### Paso 1.3: Variables en .repo

```bash
docker compose exec alma-dev bash -c '
echo "=== Variables automaticas ==="
echo ""
echo "Los .repo usan variables que se resuelven automaticamente:"
echo ""
echo "\$releasever → $(rpm -E %{rhel} 2>/dev/null || echo "?")"
echo "\$basearch   → $(rpm -E %{_arch} 2>/dev/null || uname -m)"
echo ""
echo "Ejemplo:"
echo "  baseurl=https://repo.almalinux.org/almalinux/\$releasever/BaseOS/\$basearch/os/"
echo "  Se resuelve a:"
echo "  baseurl=https://repo.almalinux.org/almalinux/$(rpm -E %{rhel})/BaseOS/$(uname -m)/os/"
'
```

### Paso 1.4: baseurl vs mirrorlist vs metalink

```bash
docker compose exec alma-dev bash -c '
echo "=== Metodos de acceso a repos ==="
echo ""
echo "baseurl:    URL fija a un solo servidor"
echo "mirrorlist: URL que devuelve lista de mirrors"
echo "            dnf elige el mas rapido"
echo "metalink:   como mirrorlist + checksums de metadatos"
echo ""
echo "Prioridad: metalink > mirrorlist > baseurl"
echo "mirrorlist/metalink son preferibles por redundancia"
echo ""
echo "=== Que usan nuestros repos ==="
for repo in /etc/yum.repos.d/*.repo; do
    name=$(basename "$repo")
    if grep -q "mirrorlist" "$repo" 2>/dev/null; then
        echo "  $name: mirrorlist"
    elif grep -q "metalink" "$repo" 2>/dev/null; then
        echo "  $name: metalink"
    elif grep -q "baseurl" "$repo" 2>/dev/null; then
        echo "  $name: baseurl"
    fi
done
'
```

---

## Parte 2 — BaseOS, AppStream, EPEL

### Objetivo

Entender los repositorios estandar y como habilitar repos extras.

### Paso 2.1: Repos habilitados

```bash
docker compose exec alma-dev bash -c '
echo "=== Repositorios habilitados ==="
dnf repolist 2>/dev/null
echo ""
echo "=== Todos los repos (incluidos deshabilitados) ==="
dnf repolist all 2>/dev/null
'
```

### Paso 2.2: BaseOS vs AppStream

```bash
docker compose exec alma-dev bash -c '
echo "=== BaseOS vs AppStream ==="
echo ""
echo "BaseOS:"
echo "  Paquetes core del sistema operativo"
echo "  Soporte durante toda la vida de la release"
echo "  Solo RPMs tradicionales"
echo "  Ejemplos: kernel, bash, systemd, openssh"
echo ""
echo "AppStream:"
echo "  Aplicaciones, herramientas, lenguajes"
echo "  Puede tener multiples versiones via module streams"
echo "  RPMs + modulos"
echo "  Ejemplos: nginx, python, nodejs, php, postgresql"
echo ""
echo "=== Info de cada repo ==="
dnf repoinfo baseos 2>/dev/null | head -10
echo "..."
echo ""
dnf repoinfo appstream 2>/dev/null | head -10
echo "..."
'
```

### Paso 2.3: CRB (CodeReady Builder)

```bash
docker compose exec alma-dev bash -c '
echo "=== CRB: CodeReady Builder ==="
echo ""
echo "Contiene headers, libs de desarrollo, herramientas"
echo "Necesario para compilar software y para muchos paquetes de EPEL"
echo ""
echo "Estado actual de CRB:"
dnf repolist all 2>/dev/null | grep -i crb || echo "(no encontrado)"
echo ""
echo "Habilitar CRB:"
echo "  sudo dnf config-manager --set-enabled crb"
echo ""
echo "Deshabilitar CRB:"
echo "  sudo dnf config-manager --set-disabled crb"
'
```

### Paso 2.4: EPEL

```bash
docker compose exec alma-dev bash -c '
echo "=== EPEL: Extra Packages for Enterprise Linux ==="
echo ""
echo "Repositorio de la comunidad Fedora para RHEL"
echo "Contiene paquetes que en Debian estarian en main/universe"
echo "Ejemplos: htop, jq, tmux, bat, ripgrep"
echo ""
echo "Estado de EPEL:"
rpm -q epel-release 2>/dev/null || echo "  (no instalado)"
echo ""
echo "Instalar EPEL:"
echo "  sudo dnf install epel-release"
echo ""
echo "EPEL + CRB se habilitan juntos frecuentemente:"
echo "  sudo dnf config-manager --set-enabled crb"
echo "  sudo dnf install epel-release"
echo ""
dnf repolist 2>/dev/null | grep -i epel || echo "EPEL no esta habilitado"
'
```

---

## Parte 3 — GPG y gestion de repos

### Objetivo

Entender la autenticacion GPG y como gestionar repos.

### Paso 3.1: Claves GPG

```bash
docker compose exec alma-dev bash -c '
echo "=== Claves GPG de RPM ==="
echo ""
echo "--- Claves instaladas ---"
ls /etc/pki/rpm-gpg/
echo ""
echo "--- Claves importadas en RPM ---"
rpm -qa gpg-pubkey* 2>/dev/null | while read key; do
    echo ""
    rpm -qi "$key" 2>/dev/null | grep -E "^(Name|Summary|Packager)"
done
echo ""
echo "--- Verificar gpgcheck ---"
grep gpgcheck /etc/dnf/dnf.conf
echo ""
echo "gpgcheck=1 verifica que los paquetes esten firmados"
echo "NUNCA desactivar en produccion"
'
```

### Paso 3.2: Gestionar repos con config-manager

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf config-manager ==="
echo ""
echo "Habilitar un repo:"
echo "  sudo dnf config-manager --set-enabled crb"
echo ""
echo "Deshabilitar un repo:"
echo "  sudo dnf config-manager --set-disabled extras"
echo ""
echo "Agregar un repo de terceros:"
echo "  sudo dnf config-manager --add-repo URL"
echo ""
echo "Habilitar/deshabilitar temporalmente:"
echo "  dnf install --enablerepo=crb paquete"
echo "  dnf update --disablerepo=epel"
echo ""
echo "=== Repos temporales ==="
echo "Cuando un mirror esta caido:"
echo "  sudo dnf update --disablerepo=epel"
echo "  (ignora EPEL para esta operacion)"
'
```

### Paso 3.3: COPR

```bash
docker compose exec alma-dev bash -c '
echo "=== COPR: Community Projects ==="
echo ""
echo "Equivalente de los PPAs de Ubuntu para Fedora/RHEL"
echo ""
echo "Habilitar:"
echo "  sudo dnf copr enable usuario/proyecto"
echo ""
echo "Ejemplo:"
echo "  sudo dnf copr enable atim/starship"
echo "  sudo dnf install starship"
echo ""
echo "Deshabilitar:"
echo "  sudo dnf copr disable usuario/proyecto"
echo ""
echo "Listar COPRs habilitados:"
dnf copr list 2>/dev/null || echo "  (no hay COPRs habilitados)"
'
```

### Paso 3.4: Prioridades

```bash
docker compose exec alma-dev bash -c '
echo "=== Prioridades de repositorios ==="
echo ""
echo "Si un paquete existe en multiples repos:"
echo "  priority=N en el .repo (menor = mas prioritario)"
echo "  Default: 99"
echo ""
echo "Ejemplo: dar prioridad a repo interno sobre EPEL"
echo "  [mi-repo]"
echo "  priority=10    ← mas prioritario"
echo ""
echo "  [epel]"
echo "  priority=99    ← default"
echo ""
echo "=== Comparacion con APT ==="
printf "%-20s %-25s %-25s\n" "Aspecto" "APT (Debian)" "DNF (RHEL)"
printf "%-20s %-25s %-25s\n" "-------------------" "------------------------" "------------------------"
printf "%-20s %-25s %-25s\n" "Configuracion" "/etc/apt/sources.list.d/" "/etc/yum.repos.d/"
printf "%-20s %-25s %-25s\n" "Formato" "one-line o DEB822" "INI (.repo)"
printf "%-20s %-25s %-25s\n" "Claves GPG" "/usr/share/keyrings/" "/etc/pki/rpm-gpg/"
printf "%-20s %-25s %-25s\n" "Repos comunitarios" "PPAs (Ubuntu)" "EPEL, COPR"
printf "%-20s %-25s %-25s\n" "Prioridades" "Pin-Priority" "priority="
printf "%-20s %-25s %-25s\n" "Mirrors" "No automatico" "mirrorlist/metalink"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los repositorios en RHEL se configuran como archivos `.repo`
   en `/etc/yum.repos.d/` con formato INI. Cada seccion
   `[repo-id]` define un repositorio.

2. **BaseOS** contiene paquetes core del sistema; **AppStream**
   contiene aplicaciones con soporte de module streams. Ambos
   estan habilitados por defecto.

3. **CRB** (CodeReady Builder) contiene paquetes de desarrollo
   y es necesario para muchas dependencias de EPEL. Se habilita
   con `dnf config-manager --set-enabled crb`.

4. **EPEL** es el repositorio comunitario de Fedora para RHEL.
   Se instala con `dnf install epel-release` y contiene paquetes
   que en Debian estarian en main/universe.

5. Las claves GPG estan en `/etc/pki/rpm-gpg/`. `gpgcheck=1` en
   dnf.conf verifica que los paquetes esten firmados.

6. `mirrorlist` y `metalink` son preferibles a `baseurl` porque
   proporcionan redundancia y seleccion automatica del mirror
   mas rapido.
