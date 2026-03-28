# Scripts de provisioning

## Índice

1. [Por qué automatizar](#1-por-qué-automatizar)
2. [Arquitectura de los scripts](#2-arquitectura-de-los-scripts)
3. [Script de provisioning: create-lab.sh](#3-script-de-provisioning-create-labsh)
4. [Script de teardown: destroy-lab.sh](#4-script-de-teardown-destroy-labsh)
5. [Ejemplo concreto: entorno de B08](#5-ejemplo-concreto-entorno-de-b08)
6. [Funciones reutilizables](#6-funciones-reutilizables)
7. [Verificación del entorno](#7-verificación-del-entorno)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Por qué automatizar

A lo largo de S03-S07 hemos aprendido a crear imágenes, VMs, redes y discos
manualmente. Pero preparar un entorno de lab completo requiere muchos pasos:

```
Preparar un lab manualmente:
  1. Crear overlay sobre imagen base        ─┐
  2. Crear VM con virt-install --import       │
  3. Crear discos extra (4 discos × 2 VMs)    │  ~15 comandos
  4. Adjuntar discos a cada VM                │  ~10 minutos
  5. Verificar que todo funciona              │  propenso a errores
  6. Repetir para la segunda VM              ─┘

Con un script:
  ./create-lab.sh b08                         ─► 30 segundos
                                                 reproducible
                                                 sin errores
```

La automatización no es un lujo — es la forma en que trabajarás en los labs.
Cada bloque del curso (B08, B09, B10, B11) tendrá su propio entorno, y
necesitas poder levantarlo y destruirlo rápidamente.

---

## 2. Arquitectura de los scripts

### 2.1. Dos scripts, un ciclo

```
┌─────────────────────────────────────────────────────────────┐
│                    CICLO DE UN LAB                          │
│                                                             │
│  create-lab.sh                    destroy-lab.sh            │
│  ┌───────────────────┐            ┌──────────────────┐     │
│  │ 1. Crear overlays │            │ 1. Apagar VMs    │     │
│  │ 2. Crear VMs      │───────────►│ 2. Undefine VMs  │     │
│  │ 3. Crear discos   │  hacer el  │ 3. Borrar discos │     │
│  │ 4. Adjuntar discos│    lab     │ 4. Borrar overlays│    │
│  │ 5. Crear redes    │            │ 5. Borrar redes  │     │
│  │ 6. Arrancar VMs   │            │    (si propias)  │     │
│  └───────────────────┘            └──────────────────┘     │
│         │                                   │               │
│         └───────────────────────────────────┘               │
│              VM base e imágenes base intactas               │
└─────────────────────────────────────────────────────────────┘
```

### 2.2. Convenciones

| Aspecto | Convención |
|---------|------------|
| Imágenes base | `/var/lib/libvirt/images/base-debian12.qcow2`, `base-alma9.qcow2` |
| Overlays | `/var/lib/libvirt/images/lab-BLOQUE-NOMBRE.qcow2` |
| Discos extra | `/var/lib/libvirt/images/lab-BLOQUE-NOMBRE-diskN.qcow2` |
| VMs | `lab-BLOQUE-NOMBRE` (ej: `lab-b08-debian`, `lab-b08-alma`) |
| Redes | `lab-BLOQUE-NOMBRE` (ej: `lab-b09-internal`) |

El prefijo `lab-BLOQUE-` permite identificar y limpiar todos los recursos de un
bloque fácilmente.

### 2.3. Prerequisitos

Los scripts asumen que ya tienes (de S04/T04):

```bash
# Imágenes base selladas (read-only)
ls -la /var/lib/libvirt/images/base-debian12.qcow2
ls -la /var/lib/libvirt/images/base-alma9.qcow2
# Permisos: -r--r--r-- (444) — protegidas contra escritura accidental
```

---

## 3. Script de provisioning: create-lab.sh

### 3.1. Estructura del script

Un script de provisioning sigue siempre la misma secuencia:

```
┌──────────────────────────────────────────────────┐
│                 create-lab.sh                    │
│                                                  │
│  1. Variables de configuración                   │
│     - nombres, rutas, tamaños                    │
│                                                  │
│  2. Validaciones                                 │
│     - ¿existen las imágenes base?                │
│     - ¿ya existe una VM con ese nombre?          │
│                                                  │
│  3. Crear overlays                               │
│     - qemu-img create -b base -F qcow2          │
│                                                  │
│  4. Crear VMs                                    │
│     - virt-install --import                      │
│                                                  │
│  5. Crear y adjuntar discos extra                │
│     - qemu-img create + virsh attach-disk        │
│                                                  │
│  6. Crear redes (si el lab las necesita)          │
│     - virsh net-define + net-start               │
│                                                  │
│  7. Resumen final                                │
│     - mostrar IPs, discos, estado                │
└──────────────────────────────────────────────────┘
```

### 3.2. Script genérico comentado

```bash
#!/bin/bash
# create-lab.sh — Provision a complete lab environment
# Usage: ./create-lab.sh
set -euo pipefail

# ── Configuration ────────────────────────────────────────────────
POOL="/var/lib/libvirt/images"
BASE_DEBIAN="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"

LAB_PREFIX="lab-b08"

# VMs to create: name, base image, RAM (MB), vCPUs
declare -A VMS=(
  ["debian"]="${BASE_DEBIAN}:2048:2"
  ["alma"]="${BASE_ALMA}:2048:2"
)

# Extra disks: count and size per VM
DISK_COUNT=4
DISK_SIZE="2G"

# ── Validation ───────────────────────────────────────────────────
echo "=== Validating prerequisites ==="

for base in "$BASE_DEBIAN" "$BASE_ALMA"; do
  if [ ! -f "$base" ]; then
    echo "ERROR: Base image not found: $base"
    echo "Create it first (see S04/T04)"
    exit 1
  fi
done

for vm_name in "${!VMS[@]}"; do
  full_name="${LAB_PREFIX}-${vm_name}"
  if virsh dominfo "$full_name" &>/dev/null; then
    echo "ERROR: VM $full_name already exists"
    echo "Run ./destroy-lab.sh first"
    exit 1
  fi
done

echo "Prerequisites OK"
echo ""

# ── Create overlays and VMs ──────────────────────────────────────
for vm_name in "${!VMS[@]}"; do
  IFS=':' read -r base_img ram vcpus <<< "${VMS[$vm_name]}"
  full_name="${LAB_PREFIX}-${vm_name}"
  overlay="${POOL}/${full_name}.qcow2"

  echo "=== Creating VM: $full_name ==="

  # Create overlay
  echo "  Creating overlay..."
  qemu-img create -f qcow2 -b "$base_img" -F qcow2 "$overlay"

  # Create VM
  echo "  Creating VM (import)..."
  virt-install \
    --name "$full_name" \
    --ram "$ram" \
    --vcpus "$vcpus" \
    --disk "path=${overlay},format=qcow2" \
    --import \
    --os-variant "$([ "$vm_name" = "debian" ] && echo debian12 || echo almalinux9)" \
    --network network=default \
    --graphics none \
    --noautoconsole \
    --quiet

  echo "  VM created and started"
  echo ""
done

# ── Create and attach extra disks ────────────────────────────────
targets=(vdb vdc vdd vde vdf vdg vdh)

for vm_name in "${!VMS[@]}"; do
  full_name="${LAB_PREFIX}-${vm_name}"

  echo "=== Attaching disks to $full_name ==="

  for i in $(seq 1 "$DISK_COUNT"); do
    disk="${POOL}/${full_name}-disk${i}.qcow2"
    target="${targets[$((i-1))]}"

    echo "  Creating ${disk} (${DISK_SIZE})..."
    qemu-img create -f qcow2 "$disk" "$DISK_SIZE"

    echo "  Attaching as ${target}..."
    virsh attach-disk "$full_name" "$disk" "$target" \
      --driver qemu --subdriver qcow2 --persistent
  done

  echo ""
done

# ── Summary ──────────────────────────────────────────────────────
echo "=== Lab environment ready ==="
echo ""

for vm_name in "${!VMS[@]}"; do
  full_name="${LAB_PREFIX}-${vm_name}"
  echo "--- $full_name ---"
  echo "  State: $(virsh domstate "$full_name")"
  echo "  Disks:"
  virsh domblklist "$full_name" | grep -v "^$" | grep -v "^-"
  echo ""
done

echo "Wait ~30s for VMs to boot, then:"
echo "  virsh console ${LAB_PREFIX}-debian"
echo "  virsh console ${LAB_PREFIX}-alma"
```

### 3.3. Cada paso explicado

**Variables de configuración**: centralizan todo lo que puede cambiar entre labs.
Para otro bloque, solo modificas `LAB_PREFIX`, las VMs y los discos.

**Validaciones**: evitan problemas antes de empezar. Si la imagen base no existe
o la VM ya existe, el script se detiene con un mensaje claro.

**`set -euo pipefail`**: detiene el script ante cualquier error (`-e`), variables
indefinidas (`-u`) y fallos en pipes (`-o pipefail`). Crítico para evitar estados
parciales.

**`--noautoconsole`**: evita que `virt-install` espere y se enganche a la consola.
El script continúa mientras la VM arranca en background.

**`--quiet`**: reduce la salida de virt-install para mantener el output limpio.

---

## 4. Script de teardown: destroy-lab.sh

### 4.1. Script comentado

```bash
#!/bin/bash
# destroy-lab.sh — Destroy a complete lab environment
# Usage: ./destroy-lab.sh
set -euo pipefail

# ── Configuration (must match create-lab.sh) ─────────────────────
POOL="/var/lib/libvirt/images"
LAB_PREFIX="lab-b08"

VM_NAMES=("debian" "alma")
DISK_COUNT=4

# ── Destroy VMs ──────────────────────────────────────────────────
for vm_name in "${VM_NAMES[@]}"; do
  full_name="${LAB_PREFIX}-${vm_name}"

  if ! virsh dominfo "$full_name" &>/dev/null; then
    echo "VM $full_name does not exist, skipping"
    continue
  fi

  echo "=== Destroying $full_name ==="

  # Shut down if running
  state=$(virsh domstate "$full_name" 2>/dev/null)
  if [ "$state" = "running" ]; then
    echo "  Shutting down..."
    virsh destroy "$full_name" 2>/dev/null || true
  fi

  # Undefine
  echo "  Undefining..."
  virsh undefine "$full_name" --remove-all-storage 2>/dev/null || true

  echo ""
done

# ── Remove extra disks (if --remove-all-storage missed them) ─────
for vm_name in "${VM_NAMES[@]}"; do
  full_name="${LAB_PREFIX}-${vm_name}"

  for i in $(seq 1 "$DISK_COUNT"); do
    disk="${POOL}/${full_name}-disk${i}.qcow2"
    if [ -f "$disk" ]; then
      echo "Removing leftover disk: $disk"
      rm "$disk"
    fi
  done

  # Remove overlay (if not removed by --remove-all-storage)
  overlay="${POOL}/${full_name}.qcow2"
  if [ -f "$overlay" ]; then
    echo "Removing leftover overlay: $overlay"
    rm "$overlay"
  fi
done

# ── Summary ──────────────────────────────────────────────────────
echo ""
echo "=== Lab environment destroyed ==="
echo ""
echo "Base images preserved:"
ls -lh "${POOL}/base-"*.qcow2 2>/dev/null || echo "  (none found)"
echo ""
echo "Remaining VMs:"
virsh list --all
```

### 4.2. Puntos clave del teardown

**`virsh destroy`**: apaga la VM forzosamente (equivalente a cortar la
electricidad). En un lab desechable esto es aceptable — no necesitamos shutdown
limpio.

**`--remove-all-storage`**: elimina todos los discos asociados a la VM al hacer
`undefine`. Esto cubre el overlay y los discos extra que estén en el XML.

**Limpieza de remanentes**: el loop final de `rm` captura discos que
`--remove-all-storage` pudiera no haber eliminado (por ejemplo, si el attach
no fue `--persistent` y el disco no quedó en el XML).

**`|| true`**: evita que el script se detenga si `virsh destroy` falla (la VM
ya estaba apagada) o si `virsh undefine` falla (ya fue eliminada).

**Imágenes base intactas**: el script nunca toca `base-debian12.qcow2` ni
`base-alma9.qcow2`. Solo elimina overlays y discos extra.

---

## 5. Ejemplo concreto: entorno de B08

### 5.1. Lo que crea el script

El entorno de B08 (Almacenamiento) es:

```
┌──────────────────────────────────────────────────────────────┐
│               ENTORNO LAB B08 — ALMACENAMIENTO              │
│                                                              │
│  ┌─────────────────────┐    ┌─────────────────────┐          │
│  │  lab-b08-debian     │    │  lab-b08-alma       │          │
│  │  Debian 12          │    │  AlmaLinux 9        │          │
│  │  2GB RAM, 2 vCPUs   │    │  2GB RAM, 2 vCPUs   │          │
│  │                     │    │                     │          │
│  │  vda: SO (overlay)  │    │  vda: SO (overlay)  │          │
│  │  vdb: disk1 (2G)    │    │  vdb: disk1 (2G)    │          │
│  │  vdc: disk2 (2G)    │    │  vdc: disk2 (2G)    │          │
│  │  vdd: disk3 (2G)    │    │  vdd: disk3 (2G)    │          │
│  │  vde: disk4 (2G)    │    │  vde: disk4 (2G)    │          │
│  └────────┬────────────┘    └────────┬────────────┘          │
│           │                          │                       │
│           └──────────┬───────────────┘                       │
│                      │                                       │
│              ┌───────▼────────┐                              │
│              │   default      │                              │
│              │ 192.168.122.0  │                              │
│              │   (NAT)        │                              │
│              └────────────────┘                              │
│                                                              │
│  Archivos en /var/lib/libvirt/images/:                       │
│    base-debian12.qcow2     (read-only, NO se toca)           │
│    base-alma9.qcow2        (read-only, NO se toca)           │
│    lab-b08-debian.qcow2    (overlay — desechable)            │
│    lab-b08-alma.qcow2      (overlay — desechable)            │
│    lab-b08-debian-disk{1..4}.qcow2  (discos extra)           │
│    lab-b08-alma-disk{1..4}.qcow2    (discos extra)           │
└──────────────────────────────────────────────────────────────┘
```

### 5.2. Ejecución

```bash
# Levantar el entorno
sudo ./create-lab.sh
```

Salida esperada:

```
=== Validating prerequisites ===
Prerequisites OK

=== Creating VM: lab-b08-debian ===
  Creating overlay...
  Creating VM (import)...
  VM created and started

=== Creating VM: lab-b08-alma ===
  Creating overlay...
  Creating VM (import)...
  VM created and started

=== Attaching disks to lab-b08-debian ===
  Creating /var/lib/libvirt/images/lab-b08-debian-disk1.qcow2 (2G)...
  Attaching as vdb...
  Creating /var/lib/libvirt/images/lab-b08-debian-disk2.qcow2 (2G)...
  Attaching as vdc...
  Creating /var/lib/libvirt/images/lab-b08-debian-disk3.qcow2 (2G)...
  Attaching as vdd...
  Creating /var/lib/libvirt/images/lab-b08-debian-disk4.qcow2 (2G)...
  Attaching as vde...

=== Attaching disks to lab-b08-alma ===
  ...

=== Lab environment ready ===

--- lab-b08-debian ---
  State: running
  Disks:
 Target   Source
 vda      /var/lib/libvirt/images/lab-b08-debian.qcow2
 vdb      /var/lib/libvirt/images/lab-b08-debian-disk1.qcow2
 vdc      /var/lib/libvirt/images/lab-b08-debian-disk2.qcow2
 vdd      /var/lib/libvirt/images/lab-b08-debian-disk3.qcow2
 vde      /var/lib/libvirt/images/lab-b08-debian-disk4.qcow2

Wait ~30s for VMs to boot, then:
  virsh console lab-b08-debian
  virsh console lab-b08-alma
```

```bash
# Destruir todo después del lab
sudo ./destroy-lab.sh
```

---

## 6. Funciones reutilizables

### 6.1. Librería de funciones

Para no repetir código entre scripts de distintos bloques, puedes extraer
funciones comunes a un archivo `lab-functions.sh`:

```bash
#!/bin/bash
# lab-functions.sh — Reusable functions for lab scripts

POOL="/var/lib/libvirt/images"

# Create an overlay from a base image
create_overlay() {
  local base="$1"
  local overlay="$2"

  if [ ! -f "$base" ]; then
    echo "ERROR: Base image not found: $base"
    return 1
  fi

  qemu-img create -f qcow2 -b "$base" -F qcow2 "$overlay"
}

# Create a VM from an overlay (already created)
create_vm() {
  local name="$1"
  local overlay="$2"
  local ram="$3"
  local vcpus="$4"
  local os_variant="$5"
  local network="${6:-default}"

  virt-install \
    --name "$name" \
    --ram "$ram" \
    --vcpus "$vcpus" \
    --disk "path=${overlay},format=qcow2" \
    --import \
    --os-variant "$os_variant" \
    --network "network=${network}" \
    --graphics none \
    --noautoconsole \
    --quiet
}

# Create and attach N extra disks to a VM
attach_lab_disks() {
  local vm_name="$1"
  local count="$2"
  local size="$3"
  local targets=(vdb vdc vdd vde vdf vdg vdh)

  for i in $(seq 1 "$count"); do
    local disk="${POOL}/${vm_name}-disk${i}.qcow2"
    local target="${targets[$((i-1))]}"

    qemu-img create -f qcow2 "$disk" "$size"
    virsh attach-disk "$vm_name" "$disk" "$target" \
      --driver qemu --subdriver qcow2 --persistent
  done
}

# Destroy a VM and remove its storage
destroy_vm() {
  local name="$1"

  if ! virsh dominfo "$name" &>/dev/null; then
    echo "  VM $name does not exist"
    return 0
  fi

  local state
  state=$(virsh domstate "$name" 2>/dev/null)
  if [ "$state" = "running" ]; then
    virsh destroy "$name" 2>/dev/null || true
  fi

  virsh undefine "$name" --remove-all-storage 2>/dev/null || true
}

# Remove leftover disk files for a VM
cleanup_disks() {
  local vm_name="$1"
  local count="$2"

  for i in $(seq 1 "$count"); do
    local disk="${POOL}/${vm_name}-disk${i}.qcow2"
    [ -f "$disk" ] && rm "$disk"
  done

  local overlay="${POOL}/${vm_name}.qcow2"
  [ -f "$overlay" ] && rm "$overlay"
}

# Wait for a VM to get a DHCP lease
wait_for_ip() {
  local name="$1"
  local max_wait="${2:-60}"
  local elapsed=0

  while [ $elapsed -lt $max_wait ]; do
    local ip
    ip=$(virsh domifaddr "$name" 2>/dev/null | grep -oP '(\d+\.){3}\d+' | head -1)
    if [ -n "$ip" ]; then
      echo "$ip"
      return 0
    fi
    sleep 5
    elapsed=$((elapsed + 5))
  done

  echo "TIMEOUT"
  return 1
}
```

### 6.2. Usar la librería en scripts

Con las funciones extraídas, el script de cada bloque queda más limpio:

```bash
#!/bin/bash
# create-lab-b08.sh — Lab environment for B08 (Storage)
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

BASE_DEBIAN="${POOL}/base-debian12.qcow2"
BASE_ALMA="${POOL}/base-alma9.qcow2"
PREFIX="lab-b08"

echo "=== Creating B08 lab environment ==="

# Create VMs
create_overlay "$BASE_DEBIAN" "${POOL}/${PREFIX}-debian.qcow2"
create_vm "${PREFIX}-debian" "${POOL}/${PREFIX}-debian.qcow2" 2048 2 debian12

create_overlay "$BASE_ALMA" "${POOL}/${PREFIX}-alma.qcow2"
create_vm "${PREFIX}-alma" "${POOL}/${PREFIX}-alma.qcow2" 2048 2 almalinux9

# Attach 4 extra disks of 2G to each VM
attach_lab_disks "${PREFIX}-debian" 4 2G
attach_lab_disks "${PREFIX}-alma" 4 2G

echo "=== B08 lab ready ==="
virsh list --all | grep "$PREFIX"
```

```bash
#!/bin/bash
# destroy-lab-b08.sh — Teardown B08 lab
set -euo pipefail
source "$(dirname "$0")/lab-functions.sh"

PREFIX="lab-b08"

echo "=== Destroying B08 lab environment ==="

destroy_vm "${PREFIX}-debian"
destroy_vm "${PREFIX}-alma"

cleanup_disks "${PREFIX}-debian" 4
cleanup_disks "${PREFIX}-alma" 4

echo "=== B08 lab destroyed ==="
```

De ~60 líneas por script a ~15. La lógica compleja vive en `lab-functions.sh`.

---

## 7. Verificación del entorno

Después de ejecutar `create-lab.sh`, verifica que todo quedó correcto:

### 7.1. Script de verificación

```bash
#!/bin/bash
# verify-lab.sh — Verify lab environment is ready
set -euo pipefail

PREFIX="${1:-lab-b08}"

echo "=== Verifying lab: $PREFIX ==="
echo ""

# Check VMs
echo "--- VMs ---"
virsh list --all | grep "$PREFIX" || echo "  No VMs found with prefix $PREFIX"
echo ""

# Check disks per VM
for vm in $(virsh list --all --name | grep "$PREFIX"); do
  echo "--- Disks: $vm ---"
  virsh domblklist "$vm"
  echo ""
done

# Check network connectivity
echo "--- Network (DHCP leases) ---"
virsh net-dhcp-leases default 2>/dev/null | grep "$PREFIX" || \
  echo "  No leases yet (VMs may still be booting)"
echo ""

# Check IPs
echo "--- IP addresses ---"
for vm in $(virsh list --name | grep "$PREFIX"); do
  ip=$(virsh domifaddr "$vm" 2>/dev/null | grep -oP '(\d+\.){3}\d+' | head -1)
  echo "  $vm: ${ip:-waiting...}"
done
```

### 7.2. Verificación manual rápida

```bash
# ¿Las VMs están corriendo?
virsh list --all | grep lab-b08

# ¿Tienen los discos correctos?
virsh domblklist lab-b08-debian
virsh domblklist lab-b08-alma

# ¿Tienen IP?
virsh domifaddr lab-b08-debian
virsh domifaddr lab-b08-alma

# ¿Se puede acceder?
virsh console lab-b08-debian    # Ctrl+] para salir
```

### 7.3. Verificación dentro de la VM

```bash
# Dentro de lab-b08-debian:
lsblk
```

Salida esperada:

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk
├─vda1   252:1    0    9G  0 part /
├─vda2   252:2    0    1K  0 part
└─vda5   252:5    0  975M  0 part [SWAP]
vdb      252:16   0    2G  0 disk          ← disco extra 1
vdc      252:32   0    2G  0 disk          ← disco extra 2
vdd      252:48   0    2G  0 disk          ← disco extra 3
vde      252:64   0    2G  0 disk          ← disco extra 4
```

Cuatro discos vacíos listos para particionado, LVM, RAID, etc.

---

## 8. Errores comunes

### Error 1: ejecutar sin sudo

```bash
# ✗ Sin permisos suficientes
./create-lab.sh
# Error: "failed to connect to the hypervisor"
# o: "Permission denied" al crear archivos en /var/lib/libvirt/images/

# ✓ Con sudo
sudo ./create-lab.sh
```

### Error 2: imagen base no existe

```bash
# ✗ No creaste las plantillas de S04/T04
./create-lab.sh
# ERROR: Base image not found: /var/lib/libvirt/images/base-debian12.qcow2

# ✓ Crear las imágenes base primero (ver S04/T04)
# Las plantillas son prerequisito para toda la automatización
```

### Error 3: ejecutar create sin destroy previo

```bash
# ✗ Crear el lab dos veces
sudo ./create-lab.sh
sudo ./create-lab.sh    # ERROR: VM lab-b08-debian already exists

# ✓ Destruir antes de recrear
sudo ./destroy-lab.sh
sudo ./create-lab.sh
```

### Error 4: olvidar `set -euo pipefail`

```bash
# ✗ Sin set -e, el script continúa después de errores
# Si qemu-img create falla, virt-install intenta usar un overlay inexistente
# Si virt-install falla, attach-disk intenta adjuntar discos a una VM inexistente
# Resultado: estado parcial difícil de limpiar

# ✓ Con set -euo pipefail
# Primer error → script se detiene → mensaje claro → limpiar lo parcial
```

### Error 5: usar `--noautoconsole` sin `--quiet` confunde la salida

```bash
# ✗ Sin --quiet, virt-install imprime detalles XML extensos
# El output del script se vuelve ilegible con 2 VMs

# ✓ Usar ambos juntos
virt-install ... --noautoconsole --quiet
# Output limpio: solo los mensajes de echo del script
```

---

## 9. Cheatsheet

```bash
# ── Levantar un lab ──────────────────────────────────────────────
sudo ./create-lab.sh

# ── Destruir un lab ──────────────────────────────────────────────
sudo ./destroy-lab.sh

# ── Verificar ────────────────────────────────────────────────────
virsh list --all | grep lab-b08
virsh domblklist lab-b08-debian
virsh domifaddr lab-b08-debian

# ── Estructura de un script de provisioning ──────────────────────
# 1. Variables de configuración (PREFIX, bases, discos)
# 2. Validaciones (base images, VMs existentes)
# 3. Crear overlays (qemu-img create -b)
# 4. Crear VMs (virt-install --import)
# 5. Crear y adjuntar discos (qemu-img create + attach-disk)
# 6. Resumen (virsh list, domblklist)

# ── Estructura de un script de teardown ──────────────────────────
# 1. Apagar VMs (virsh destroy)
# 2. Eliminar VMs (virsh undefine --remove-all-storage)
# 3. Limpiar remanentes (rm overlays y discos huérfanos)

# ── Funciones reutilizables ──────────────────────────────────────
source lab-functions.sh
create_overlay BASE OVERLAY
create_vm NAME OVERLAY RAM VCPUS OS_VARIANT [NETWORK]
attach_lab_disks VM_NAME COUNT SIZE
destroy_vm NAME
cleanup_disks VM_NAME DISK_COUNT
wait_for_ip VM_NAME [TIMEOUT]

# ── Convención de nombres ───────────────────────────────────────
# VMs:      lab-BLOQUE-NOMBRE     (lab-b08-debian)
# Overlays: lab-BLOQUE-NOMBRE.qcow2
# Discos:   lab-BLOQUE-NOMBRE-diskN.qcow2
# Redes:    lab-BLOQUE-NOMBRE     (lab-b09-internal)
```

---

## 10. Ejercicios

### Ejercicio 1: script de provisioning mínimo

1. Crea un script `create-lab-test.sh` que:
   - Cree un overlay sobre tu imagen base de Debian
   - Cree una VM llamada `lab-test-debian` con `--import`
   - Adjunte 2 discos extra de 1G como `vdb` y `vdc`
   - Muestre `virsh domblklist` al final
2. Ejecútalo con `sudo` y verifica que la VM arranca con los discos
3. Crea `destroy-lab-test.sh` que destruya todo
4. Ejecuta destroy y verifica que no queda nada

**Pregunta de reflexión**: ¿por qué el script usa `--noautoconsole` en vez de
dejar que `virt-install` se enganche a la consola? ¿Qué pasaría si el script
tiene que crear dos VMs y la primera se queda esperando en la consola?

### Ejercicio 2: funciones reutilizables

1. Crea el archivo `lab-functions.sh` con las funciones de la sección 6.1
2. Crea `create-lab-func.sh` que use `source lab-functions.sh` y las funciones
   `create_overlay`, `create_vm` y `attach_lab_disks` para crear una VM con
   3 discos
3. Crea `destroy-lab-func.sh` que use `destroy_vm` y `cleanup_disks`
4. Ejecuta create, verifica, y luego destroy

**Pregunta de reflexión**: comparando el script de la sección 3.2 (~60 líneas)
con el de la sección 6.2 (~15 líneas), ¿qué se gana y qué se pierde al extraer
funciones a una librería? ¿Cuándo vale la pena el esfuerzo de crear la librería?

### Ejercicio 3: agregar verificación al script

1. Modifica tu script de provisioning para que al final ejecute una verificación
   automática:
   - Compruebe que cada VM está en estado `running`
   - Compruebe que cada VM tiene el número correcto de discos en `domblklist`
   - Espere hasta que cada VM tenga una IP (con `wait_for_ip` o un loop propio)
   - Imprima un resumen con nombre, estado, número de discos e IP de cada VM
2. Si alguna verificación falla, el script debe imprimir un mensaje de error claro

**Pregunta de reflexión**: el script espera a que la VM obtenga IP para considerar
el entorno listo. ¿Por qué no basta con que la VM esté en estado `running`?
¿Qué ocurre entre el momento en que la VM arranca y el momento en que obtiene IP?
