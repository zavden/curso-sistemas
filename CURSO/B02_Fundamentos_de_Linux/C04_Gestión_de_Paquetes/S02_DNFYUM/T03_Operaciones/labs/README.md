# Lab — Operaciones

## Objetivo

Dominar las operaciones de dnf: instalar, buscar, consultar con
provides, grupos de paquetes, module streams, e historial de
transacciones con undo.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Operaciones basicas

### Objetivo

Practicar las operaciones cotidianas de gestion de paquetes con dnf.

### Paso 1.1: Buscar paquetes

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf search ==="
dnf search "json processor" 2>/dev/null | head -10
echo ""
echo "=== dnf search --all ==="
echo "Busca en nombre, summary Y description:"
dnf search --all json 2>/dev/null | head -10
echo ""
echo "dnf search: busca en nombre y summary"
echo "dnf search --all: busca tambien en description"
'
```

### Paso 1.2: Informacion de un paquete

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf info bash ==="
dnf info bash 2>/dev/null
echo ""
echo "--- Campos importantes ---"
echo "Name:        nombre del paquete"
echo "Version:     version upstream"
echo "Release:     build del empaquetador + distro"
echo "Repository:  de que repo viene"
echo "Size:        tamano instalado"
'
```

### Paso 1.3: Quien provee este archivo (provides)

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf provides ==="
echo ""
echo "--- Que paquete instala /usr/bin/curl ---"
dnf provides /usr/bin/curl 2>/dev/null
echo ""
echo "--- Buscar por patron ---"
dnf provides "*/bin/find" 2>/dev/null | head -6
echo ""
echo "--- Buscar una libreria ---"
dnf provides "libssl.so*" 2>/dev/null | head -6
echo ""
echo "dnf provides funciona incluso con paquetes NO instalados"
echo "En APT se necesita apt-file (no incluido por defecto)"
'
```

### Paso 1.4: Listar paquetes

```bash
docker compose exec alma-dev bash -c '
echo "=== Listar paquetes ==="
echo ""
echo "--- Instalados (primeros 10) ---"
dnf list installed 2>/dev/null | head -12
echo ""
echo "--- Actualizaciones disponibles ---"
dnf list updates 2>/dev/null | head -8
echo ""
echo "--- Buscar con patron ---"
dnf list "python3*" 2>/dev/null | head -8
echo ""
echo "Total paquetes instalados: $(rpm -qa | wc -l)"
'
```

### Paso 1.5: check-update

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf check-update ==="
echo ""
echo "Lista paquetes con actualizaciones disponibles"
echo "Exit code: 0 = todo al dia, 100 = hay actualizaciones"
echo ""
dnf check-update 2>/dev/null | head -10
rc=$?
echo "..."
echo ""
echo "Exit code: $rc"
if [ "$rc" -eq 0 ]; then
    echo "  → Todo al dia"
elif [ "$rc" -eq 100 ]; then
    echo "  → Hay actualizaciones disponibles"
fi
echo ""
echo "Nota: dnf NO necesita un update previo como apt"
echo "check-update y upgrade refrescan metadatos automaticamente"
'
```

---

## Parte 2 — Grupos y modulos

### Objetivo

Usar grupos de paquetes y module streams.

### Paso 2.1: Grupos de paquetes

```bash
docker compose exec alma-dev bash -c '
echo "=== Grupos disponibles ==="
dnf group list 2>/dev/null
'
```

### Paso 2.2: Informacion de un grupo

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf group info ==="
echo ""
echo "--- Development Tools ---"
dnf group info "Development Tools" 2>/dev/null | head -25
echo "..."
echo ""
echo "Mandatory: se instalan siempre"
echo "Default:   se instalan por defecto"
echo "Optional:  solo con --with-optional"
echo ""
echo "Instalar grupo:"
echo "  sudo dnf group install \"Development Tools\""
echo ""
echo "Grupos instalados:"
dnf group list --installed 2>/dev/null || echo "(ninguno)"
'
```

### Paso 2.3: Module streams

```bash
docker compose exec alma-dev bash -c '
echo "=== Module streams ==="
echo ""
echo "Modulos: multiples versiones de software en el mismo repo"
echo ""
echo "--- Modulos disponibles ---"
dnf module list 2>/dev/null | head -20
echo "..."
echo ""
echo "--- Campos ---"
echo "Name:    nombre del modulo"
echo "Stream:  version (15, 16, 18, etc.)"
echo "Profile: conjunto de paquetes (server, client, minimal)"
echo "[d]:     stream default"
echo "[e]:     stream habilitado"
echo ""
echo "--- Ejemplo de uso ---"
echo "  dnf module list nodejs        ← ver streams"
echo "  dnf module enable nodejs:18   ← habilitar stream"
echo "  dnf module install nodejs:18  ← instalar"
echo "  dnf module reset nodejs       ← resetear (para cambiar stream)"
'
```

### Paso 2.4: Modulos y filtrado

```bash
docker compose exec alma-dev bash -c '
echo "=== Modulos y filtrado de paquetes ==="
echo ""
echo "Los modulos pueden FILTRAR paquetes:"
echo "  Si el modulo postgresql:15 esta habilitado,"
echo "  dnf NO mostrara paquetes de postgresql 16"
echo ""
echo "Si un paquete de EPEL es filtrado por un modulo:"
echo "  dnf install paquete → No match for argument"
echo ""
echo "Soluciones:"
echo "  1. dnf module disable modulo"
echo "  2. module_hotfixes=1 en el .repo"
echo ""
echo "=== Modulos habilitados ==="
dnf module list --enabled 2>/dev/null || echo "(ninguno habilitado)"
'
```

---

## Parte 3 — Historial y cache

### Objetivo

Usar el historial de transacciones de dnf para auditar y revertir.

### Paso 3.1: Ver historial

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf history ==="
dnf history 2>/dev/null | head -15
echo ""
echo "--- Campos ---"
echo "ID:          numero de transaccion"
echo "Command line: comando ejecutado"
echo "Date and time: cuando"
echo "Action(s):    Install, Upgrade, Remove"
echo "Altered:      paquetes afectados"
'
```

### Paso 3.2: Detalle de una transaccion

```bash
docker compose exec alma-dev bash -c '
echo "=== Detalle de la ultima transaccion ==="
dnf history info last 2>/dev/null
echo ""
echo "Muestra exactamente que paquetes se instalaron/eliminaron/actualizaron"
'
```

### Paso 3.3: Undo y redo

```bash
docker compose exec alma-dev bash -c '
echo "=== Revertir una transaccion ==="
echo ""
echo "dnf history undo N"
echo "  Deshace la transaccion N:"
echo "  - Desinstala lo que se instalo"
echo "  - Reinstala lo que se elimino"
echo ""
echo "dnf history redo N"
echo "  Repite la transaccion N"
echo ""
echo "Ejemplo:"
echo "  ID 15: install nginx (instalo nginx + deps)"
echo "  dnf history undo 15 → desinstala nginx + deps"
echo ""
echo "--- Esto NO existe en APT ---"
echo "APT no tiene historial nativo con undo/redo"
echo "dnf history es una ventaja significativa de DNF"
'
```

### Paso 3.4: Cache y limpieza

```bash
docker compose exec alma-dev bash -c '
echo "=== Cache de dnf ==="
echo ""
echo "Ubicacion: /var/cache/dnf/"
du -sh /var/cache/dnf/ 2>/dev/null
echo ""
echo "--- Comandos de limpieza ---"
echo "dnf clean all:       limpiar todo (metadatos + .rpm)"
echo "dnf clean metadata:  solo metadatos"
echo "dnf clean packages:  solo .rpm descargados"
echo "dnf makecache:       regenerar cache"
echo ""
echo "--- Actualizaciones de seguridad ---"
echo "dnf check-update --security   ← ver security updates"
echo "dnf upgrade --security         ← instalar solo security"
echo ""
echo "=== Comparacion con APT ==="
printf "%-25s %-20s %-20s\n" "Operacion" "apt" "dnf"
printf "%-25s %-20s %-20s\n" "------------------------" "-------------------" "-------------------"
printf "%-25s %-20s %-20s\n" "Actualizar indices" "apt update" "(automatico)"
printf "%-25s %-20s %-20s\n" "Actualizar paquetes" "apt upgrade" "dnf upgrade"
printf "%-25s %-20s %-20s\n" "Purgar config" "apt purge" "(no aplica)"
printf "%-25s %-20s %-20s\n" "Historial" "(no nativo)" "dnf history"
printf "%-25s %-20s %-20s\n" "Grupos" "(no nativo)" "dnf group install"
printf "%-25s %-20s %-20s\n" "Modulos" "(no nativo)" "dnf module"
printf "%-25s %-20s %-20s\n" "Security updates" "(no nativo)" "dnf upgrade --security"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `dnf search` busca por nombre y summary; `--all` incluye
   description. `dnf provides` busca que paquete instala un
   archivo, incluso si no esta instalado.

2. **Grupos** son colecciones de paquetes (Development Tools,
   System Tools). Se instalan con `dnf group install` y
   contienen paquetes mandatory, default, y optional.

3. **Module streams** permiten tener multiples versiones de
   software en el mismo repo (ej: postgresql 15 y 16).
   Se gestionan con `dnf module enable/install/reset`.

4. `dnf history` registra TODAS las transacciones. `undo`
   revierte una transaccion; `redo` la repite. Esto no existe
   en APT.

5. dnf no necesita `update` previo — refresca metadatos
   automaticamente. `dnf check-update` lista actualizaciones
   disponibles con exit code 100 si hay.

6. `dnf upgrade --security` instala solo actualizaciones de
   seguridad, util para servidores de produccion.
