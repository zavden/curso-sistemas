# Múltiples discos para labs de RAID/LVM

## Índice

1. [Contexto: por qué múltiples discos](#1-contexto-por-qué-múltiples-discos)
2. [Crear N discos con un loop](#2-crear-n-discos-con-un-loop)
3. [Adjuntar múltiples discos a una VM](#3-adjuntar-múltiples-discos-a-una-vm)
4. [Preparación para lab de RAID](#4-preparación-para-lab-de-raid)
5. [Preparación para lab de LVM](#5-preparación-para-lab-de-lvm)
6. [Limpieza después del lab](#6-limpieza-después-del-lab)
7. [Scripts reutilizables](#7-scripts-reutilizables)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Contexto: por qué múltiples discos

Los labs de B08 requieren configuraciones específicas de discos:

```
┌──────────────────────────────────────────────────────────┐
│                  LABS QUE NECESITAN MÚLTIPLES DISCOS     │
│                                                          │
│  Lab RAID5:     3 discos mínimo + 1 spare = 4 discos    │
│  Lab RAID1:     2 discos (espejo)                        │
│  Lab RAID10:    4 discos (mirror + stripe)               │
│  Lab LVM:       2-3 discos → PVs → VG → LVs             │
│  Lab LVM+RAID:  4+ discos combinando ambas tecnologías   │
│                                                          │
│  Todos usan vdb, vdc, vdd, vde... nunca vda (el SO)     │
└──────────────────────────────────────────────────────────┘
```

Crear los discos manualmente uno por uno (como en T01) funciona, pero con 4 discos
ya se vuelve tedioso. La solución es automatizar la creación y attach con loops.

---

## 2. Crear N discos con un loop

### 2.1. Crear discos para RAID

Un lab de RAID5 necesita 3 discos + 1 spare. Todos del mismo tamaño:

```bash
for i in 1 2 3 4; do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-raid-${i}.qcow2 2G
done
```

**Predicción antes de ejecutar**: se crean 4 archivos qcow2, cada uno de ~200 KB
(thin provisioning). No se consume 8G reales en el host.

Verificar:

```bash
ls -lh /var/lib/libvirt/images/disk-raid-*.qcow2
```

Salida esperada:

```
-rw-r--r--. 1 root root 193K  ... disk-raid-1.qcow2
-rw-r--r--. 1 root root 193K  ... disk-raid-2.qcow2
-rw-r--r--. 1 root root 193K  ... disk-raid-3.qcow2
-rw-r--r--. 1 root root 193K  ... disk-raid-4.qcow2
```

### 2.2. Crear discos para LVM

Un lab de LVM típico usa 2-3 discos de tamaños iguales:

```bash
for i in 1 2 3; do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-lvm-${i}.qcow2 2G
done
```

### 2.3. Convención de nombres

Usar un prefijo descriptivo ayuda a identificar qué discos pertenecen a qué lab:

| Lab | Patrón de nombre | Ejemplo |
|-----|-------------------|---------|
| RAID | `disk-raid-N.qcow2` | `disk-raid-1.qcow2` |
| LVM | `disk-lvm-N.qcow2` | `disk-lvm-1.qcow2` |
| Particionado | `disk-part-N.qcow2` | `disk-part-1.qcow2` |
| Filesystem | `disk-fs-N.qcow2` | `disk-fs-1.qcow2` |
| Genérico | `disk-lab-N.qcow2` | `disk-lab-1.qcow2` |

---

## 3. Adjuntar múltiples discos a una VM

### 3.1. Adjuntar con loop

Después de crear los discos, adjuntarlos a la VM:

```bash
# Array de targets: vdb, vdc, vdd, vde
targets=(vdb vdc vdd vde)

for i in 1 2 3 4; do
  virsh attach-disk lab-debian01 \
    /var/lib/libvirt/images/disk-raid-${i}.qcow2 \
    ${targets[$((i-1))]} \
    --driver qemu --subdriver qcow2 --persistent
done
```

**Predicción**: cada iteración adjunta un disco. Si la VM está encendida, los
discos aparecen inmediatamente por hot-plug.

### 3.2. Mapeo discos → targets

El resultado del loop anterior:

```
Archivo                    Target en VM    Propósito RAID
──────────────────────────────────────────────────────────
disk-raid-1.qcow2     →   /dev/vdb        Disco RAID 1
disk-raid-2.qcow2     →   /dev/vdc        Disco RAID 2
disk-raid-3.qcow2     →   /dev/vdd        Disco RAID 3
disk-raid-4.qcow2     →   /dev/vde        Spare
```

### 3.3. Verificar desde el host

```bash
virsh domblklist lab-debian01
```

Salida esperada:

```
 Target   Source
-----------------------------------------------
 vda      /var/lib/libvirt/images/lab-debian01.qcow2
 vdb      /var/lib/libvirt/images/disk-raid-1.qcow2
 vdc      /var/lib/libvirt/images/disk-raid-2.qcow2
 vdd      /var/lib/libvirt/images/disk-raid-3.qcow2
 vde      /var/lib/libvirt/images/disk-raid-4.qcow2
```

### 3.4. Verificar dentro de la VM

```bash
lsblk
```

```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
vda      252:0    0   10G  0 disk
├─vda1   252:1    0    9G  0 part /
├─vda2   252:2    0    1K  0 part
└─vda5   252:5    0  975M  0 part [SWAP]
vdb      252:16   0    2G  0 disk
vdc      252:32   0    2G  0 disk
vdd      252:48   0    2G  0 disk
vde      252:64   0    2G  0 disk
```

Cuatro discos vírgenes de 2G, listos para RAID o LVM.

---

## 4. Preparación para lab de RAID

Esta sección muestra cómo quedan preparados los discos — el lab de RAID en sí
se cubrirá en detalle en B08.

### 4.1. RAID5: 3 discos + 1 spare

```
┌────────────────────────────────────────────────────────┐
│                     RAID5                              │
│                                                        │
│  /dev/vdb ──┐                                          │
│             ├──► /dev/md0  (RAID5, 3 discos activos)   │
│  /dev/vdc ──┤    Capacidad útil: (N-1) × 2G = 4G      │
│             │    Paridad distribuida entre los 3        │
│  /dev/vdd ──┘                                          │
│                                                        │
│  /dev/vde ──► spare (reemplaza automáticamente          │
│               a un disco fallido)                      │
└────────────────────────────────────────────────────────┘
```

Los comandos que usarás en el lab de RAID (adelanto):

```bash
# Crear RAID5 con 3 discos + 1 spare
sudo mdadm --create /dev/md0 --level=5 \
  --raid-devices=3 /dev/vdb /dev/vdc /dev/vdd \
  --spare-devices=1 /dev/vde

# Verificar
cat /proc/mdstat
sudo mdadm --detail /dev/md0
```

### 4.2. RAID1: 2 discos (espejo)

Para un lab de RAID1 solo necesitas 2 discos:

```bash
# Crear 2 discos
for i in 1 2; do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-raid1-${i}.qcow2 2G
done

# Adjuntar como vdb y vdc
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disk-raid1-1.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disk-raid1-2.qcow2 vdc \
  --driver qemu --subdriver qcow2 --persistent
```

```
┌──────────────────────────────────┐
│            RAID1                 │
│                                  │
│  /dev/vdb ──┐                    │
│             ├──► /dev/md0        │
│  /dev/vdc ──┘    Espejo: 2G     │
│                  Cada disco     │
│                  tiene copia    │
│                  idéntica       │
└──────────────────────────────────┘
```

### 4.3. Tabla de requisitos por tipo de RAID

| Nivel RAID | Discos mínimos | Discos recomendados (con spare) | Capacidad útil |
|------------|----------------|---------------------------------|----------------|
| RAID0 | 2 | 2 (sin spare, sin redundancia) | N × tamaño |
| RAID1 | 2 | 3 (2 + 1 spare) | 1 × tamaño |
| RAID5 | 3 | 4 (3 + 1 spare) | (N-1) × tamaño |
| RAID6 | 4 | 5 (4 + 1 spare) | (N-2) × tamaño |
| RAID10 | 4 | 4 (sin spare habitual) | N/2 × tamaño |

---

## 5. Preparación para lab de LVM

### 5.1. Estructura de LVM

LVM trabaja en tres capas. Los discos extra son la base:

```
┌───────────────────────────────────────────────────────────┐
│                    CAPAS DE LVM                           │
│                                                           │
│  Capa 3: Logical Volumes (LV)                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                  │
│  │ lv-data  │ │ lv-logs  │ │ lv-swap  │   ← filesystems  │
│  │ 2G       │ │ 500M     │ │ 1G       │                  │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘                  │
│       └─────────────┼───────────┘                         │
│                     ▼                                     │
│  Capa 2: Volume Group (VG)                                │
│  ┌──────────────────────────────────────┐                 │
│  │         vg-lab  (total: 4G)         │   ← pool lógico │
│  └──────────┬───────────┬──────────────┘                  │
│             ▼           ▼                                 │
│  Capa 1: Physical Volumes (PV)                            │
│  ┌──────────────┐ ┌──────────────┐                        │
│  │  /dev/vdb    │ │  /dev/vdc    │          ← discos      │
│  │  2G          │ │  2G          │            virtuales   │
│  └──────────────┘ └──────────────┘                        │
└───────────────────────────────────────────────────────────┘
```

### 5.2. Preparar discos para LVM

```bash
# Crear 2 discos de 2G
for i in 1 2; do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-lvm-${i}.qcow2 2G
done

# Adjuntar
virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disk-lvm-1.qcow2 vdb \
  --driver qemu --subdriver qcow2 --persistent

virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disk-lvm-2.qcow2 vdc \
  --driver qemu --subdriver qcow2 --persistent
```

Los comandos que usarás en el lab de LVM (adelanto):

```bash
# Crear PVs
sudo pvcreate /dev/vdb /dev/vdc

# Crear VG con ambos PVs
sudo vgcreate vg-lab /dev/vdb /dev/vdc

# Crear LVs
sudo lvcreate -L 2G -n lv-data vg-lab
sudo lvcreate -L 500M -n lv-logs vg-lab

# Verificar
sudo pvs
sudo vgs
sudo lvs
```

### 5.3. Agregar un tercer disco (extender VG)

Una ventaja de LVM es que puedes agregar espacio dinámicamente. Para practicar
esto, agrega un tercer disco después de crear el VG:

```bash
# En el host: crear y adjuntar un disco más
qemu-img create -f qcow2 \
  /var/lib/libvirt/images/disk-lvm-3.qcow2 2G

virsh attach-disk lab-debian01 \
  /var/lib/libvirt/images/disk-lvm-3.qcow2 vdd \
  --driver qemu --subdriver qcow2 --persistent
```

```bash
# Dentro de la VM: extender el VG
sudo pvcreate /dev/vdd
sudo vgextend vg-lab /dev/vdd

# Ahora vg-lab tiene 6G (3 × 2G) en vez de 4G
sudo vgs
```

```
Antes:                          Después:
┌────────────────────┐          ┌──────────────────────────────┐
│   vg-lab (4G)      │          │       vg-lab (6G)            │
│ ┌──────┐ ┌──────┐  │          │ ┌──────┐ ┌──────┐ ┌──────┐  │
│ │ vdb  │ │ vdc  │  │          │ │ vdb  │ │ vdc  │ │ vdd  │  │
│ │ 2G   │ │ 2G   │  │          │ │ 2G   │ │ 2G   │ │ 2G   │  │
│ └──────┘ └──────┘  │          │ └──────┘ └──────┘ └──────┘  │
└────────────────────┘          └──────────────────────────────┘
```

---

## 6. Limpieza después del lab

### 6.1. Limpieza de lab de RAID

Dentro de la VM, **en orden**:

```bash
# 1. Desmontar el filesystem (si lo montaste)
sudo umount /mnt

# 2. Detener el array RAID
sudo mdadm --stop /dev/md0

# 3. Limpiar los superblocks de cada disco
sudo mdadm --zero-superblock /dev/vdb
sudo mdadm --zero-superblock /dev/vdc
sudo mdadm --zero-superblock /dev/vdd
sudo mdadm --zero-superblock /dev/vde
```

Desde el host:

```bash
# Detach todos los discos
for target in vdb vdc vdd vde; do
  virsh detach-disk lab-debian01 ${target} --persistent
done

# Eliminar los archivos
rm /var/lib/libvirt/images/disk-raid-{1,2,3,4}.qcow2
```

### 6.2. Limpieza de lab de LVM

Dentro de la VM, **en orden**:

```bash
# 1. Desmontar LVs (si los montaste)
sudo umount /mnt/data
sudo umount /mnt/logs

# 2. Desactivar LVs
sudo lvchange -an vg-lab/lv-data
sudo lvchange -an vg-lab/lv-logs

# 3. Eliminar LVs
sudo lvremove -y vg-lab

# 4. Eliminar VG
sudo vgremove vg-lab

# 5. Eliminar PVs
sudo pvremove /dev/vdb /dev/vdc /dev/vdd
```

Desde el host:

```bash
# Detach discos
for target in vdb vdc vdd; do
  virsh detach-disk lab-debian01 ${target} --persistent
done

# Eliminar archivos
rm /var/lib/libvirt/images/disk-lvm-{1,2,3}.qcow2
```

### 6.3. Limpieza rápida: destruir y recrear

Si no quieres limpiar manualmente (RAID superblocks, LVM metadata), puedes
simplemente sobrescribir los discos:

```bash
# Apagar la VM
virsh shutdown lab-debian01

# Recrear todos los discos (sobrescribe contenido anterior)
for i in 1 2 3 4; do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-raid-${i}.qcow2 2G
done

# Encender — los discos están vacíos de nuevo
virsh start lab-debian01
```

Esto es más rápido que desmontar → parar RAID → limpiar superblocks. El tradeoff
es que necesitas apagar la VM.

---

## 7. Scripts reutilizables

### 7.1. Script: preparar discos para un lab

```bash
#!/bin/bash
# lab-disks-create.sh — Create and attach lab disks
# Usage: ./lab-disks-create.sh <vm-name> <prefix> <count> <size>
# Example: ./lab-disks-create.sh lab-debian01 raid 4 2G

VM="$1"
PREFIX="$2"
COUNT="$3"
SIZE="$4"
POOL="/var/lib/libvirt/images"

if [ -z "$VM" ] || [ -z "$PREFIX" ] || [ -z "$COUNT" ] || [ -z "$SIZE" ]; then
  echo "Usage: $0 <vm-name> <prefix> <count> <size>"
  echo "Example: $0 lab-debian01 raid 4 2G"
  exit 1
fi

# Target devices: vdb=0, vdc=1, vdd=2...
targets=(vdb vdc vdd vde vdf vdg vdh)

for i in $(seq 1 "$COUNT"); do
  DISK="${POOL}/disk-${PREFIX}-${i}.qcow2"
  TARGET="${targets[$((i-1))]}"

  echo "Creating ${DISK} (${SIZE})..."
  qemu-img create -f qcow2 "$DISK" "$SIZE"

  echo "Attaching as ${TARGET}..."
  virsh attach-disk "$VM" "$DISK" "$TARGET" \
    --driver qemu --subdriver qcow2 --persistent
done

echo ""
echo "Done. Verify with: virsh domblklist $VM"
```

Uso:

```bash
chmod +x lab-disks-create.sh

# Lab de RAID5: 4 discos de 2G
./lab-disks-create.sh lab-debian01 raid 4 2G

# Lab de LVM: 3 discos de 2G
./lab-disks-create.sh lab-debian01 lvm 3 2G

# Lab de particionado: 1 disco de 5G
./lab-disks-create.sh lab-debian01 part 1 5G
```

### 7.2. Script: limpiar discos de un lab

```bash
#!/bin/bash
# lab-disks-destroy.sh — Detach and delete lab disks
# Usage: ./lab-disks-destroy.sh <vm-name> <prefix> <count>
# Example: ./lab-disks-destroy.sh lab-debian01 raid 4

VM="$1"
PREFIX="$2"
COUNT="$3"
POOL="/var/lib/libvirt/images"

if [ -z "$VM" ] || [ -z "$PREFIX" ] || [ -z "$COUNT" ]; then
  echo "Usage: $0 <vm-name> <prefix> <count>"
  echo "Example: $0 lab-debian01 raid 4"
  exit 1
fi

targets=(vdb vdc vdd vde vdf vdg vdh)

for i in $(seq 1 "$COUNT"); do
  DISK="${POOL}/disk-${PREFIX}-${i}.qcow2"
  TARGET="${targets[$((i-1))]}"

  echo "Detaching ${TARGET}..."
  virsh detach-disk "$VM" "$TARGET" --persistent 2>/dev/null

  if [ -f "$DISK" ]; then
    echo "Deleting ${DISK}..."
    rm "$DISK"
  fi
done

echo ""
echo "Done. Verify with: virsh domblklist $VM"
```

### 7.3. Ejemplo de flujo completo con scripts

```bash
# === LAB DE RAID5 ===

# Preparar
./lab-disks-create.sh lab-debian01 raid 4 2G

# ... hacer el lab dentro de la VM ...

# Limpiar (recordar: primero desmontar/parar RAID dentro de la VM)
./lab-disks-destroy.sh lab-debian01 raid 4

# === LAB DE LVM ===

# Preparar
./lab-disks-create.sh lab-debian01 lvm 3 2G

# ... hacer el lab dentro de la VM ...

# Limpiar
./lab-disks-destroy.sh lab-debian01 lvm 3
```

---

## 8. Errores comunes

### Error 1: exceder los targets disponibles

```bash
# ✗ Intentar adjuntar más de 26 discos (vda-vdz)
# En la práctica nunca necesitarás tantos, pero el límite teórico
# del bus virtio es mayor — aún así, más de 7-8 discos extra
# indica que algo se puede simplificar

# ✓ Para labs del curso, máximo 4-5 discos extra es suficiente
```

### Error 2: crear discos de distinto tamaño para RAID

```bash
# ✗ RAID5 con discos de distinto tamaño
qemu-img create -f qcow2 disk-raid-1.qcow2 2G
qemu-img create -f qcow2 disk-raid-2.qcow2 3G  # ← diferente
qemu-img create -f qcow2 disk-raid-3.qcow2 2G

# mdadm lo permite pero usa solo el tamaño del menor en cada disco
# Resultado: 1G desperdiciado en disk-raid-2

# ✓ Siempre usar el mismo tamaño para RAID
for i in 1 2 3; do
  qemu-img create -f qcow2 disk-raid-${i}.qcow2 2G
done
```

### Error 3: olvidar limpiar LVM antes de detach

```bash
# ✗ Hacer detach sin limpiar LVM
virsh detach-disk lab-debian01 vdb --persistent
# El kernel de la VM sigue viendo PVs/VGs del disco removido
# Mensajes de error en dmesg, pvs muestra PVs "missing"

# ✓ Limpiar LVM primero
sudo lvremove -y vg-lab
sudo vgremove vg-lab
sudo pvremove /dev/vdb /dev/vdc
# Luego detach
```

### Error 4: olvidar limpiar RAID superblocks

```bash
# ✗ Después de parar el RAID, recrear sin limpiar
sudo mdadm --stop /dev/md0
# Recrear los discos y reiniciar el lab...
# mdadm detecta superblocks viejos y puede confundirse

# ✓ Limpiar superblocks después de parar
sudo mdadm --stop /dev/md0
sudo mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd /dev/vde

# O más fácil: recrear los qcow2 desde el host (sobrescribe todo)
```

### Error 5: no verificar targets antes de adjuntar

```bash
# ✗ Adjuntar como vdb cuando otro lab ya ocupó vdb
virsh attach-disk lab-debian01 nuevo-disco.qcow2 vdb ...
# Error: "target vdb already exists"

# ✓ Verificar primero qué targets están libres
virsh domblklist lab-debian01
# Si vdb está ocupado, usar vdc, vdd, etc.
```

---

## 9. Cheatsheet

```bash
# ── Crear N discos ───────────────────────────────────────────────
for i in $(seq 1 N); do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-PREFIX-${i}.qcow2 SIZE
done

# ── Adjuntar N discos (vdb, vdc, vdd...) ─────────────────────────
targets=(vdb vdc vdd vde vdf vdg)
for i in $(seq 1 N); do
  virsh attach-disk VM \
    /var/lib/libvirt/images/disk-PREFIX-${i}.qcow2 \
    ${targets[$((i-1))]} \
    --driver qemu --subdriver qcow2 --persistent
done

# ── Verificar (host) ────────────────────────────────────────────
virsh domblklist VM

# ── Verificar (dentro de VM) ────────────────────────────────────
lsblk
sudo fdisk -l /dev/vd{b,c,d,e}

# ── Detach N discos ──────────────────────────────────────────────
for target in vdb vdc vdd vde; do
  virsh detach-disk VM ${target} --persistent
done

# ── Eliminar archivos ───────────────────────────────────────────
rm /var/lib/libvirt/images/disk-PREFIX-{1,2,3,4}.qcow2

# ── Reinicio rápido (VM apagada) ────────────────────────────────
virsh shutdown VM
for i in $(seq 1 N); do
  qemu-img create -f qcow2 \
    /var/lib/libvirt/images/disk-PREFIX-${i}.qcow2 SIZE
done
virsh start VM

# ── Limpieza RAID (dentro de VM, antes de detach) ───────────────
sudo umount /mnt
sudo mdadm --stop /dev/md0
sudo mdadm --zero-superblock /dev/vdb /dev/vdc /dev/vdd /dev/vde

# ── Limpieza LVM (dentro de VM, antes de detach) ────────────────
sudo umount /mnt/data /mnt/logs
sudo lvremove -y VGNAME
sudo vgremove VGNAME
sudo pvremove /dev/vdb /dev/vdc /dev/vdd

# ── Scripts ──────────────────────────────────────────────────────
./lab-disks-create.sh  VM PREFIX COUNT SIZE
./lab-disks-destroy.sh VM PREFIX COUNT
```

---

## 10. Ejercicios

### Ejercicio 1: preparar discos para RAID5

1. Crea 4 discos de 2G con el prefijo `raid5-test`
2. Adjunta los 4 a tu VM de laboratorio como `vdb`, `vdc`, `vdd`, `vde`
3. Verifica con `virsh domblklist` que los 4 aparecen
4. Entra a la VM y verifica con `lsblk` que ves 4 discos de 2G
5. Limpia: detach los 4 discos y elimina los archivos

**Pregunta de reflexión**: en un RAID5 con 3 discos de 2G + 1 spare, ¿cuánto
espacio útil tendrás para datos? ¿Por qué no son 6G (3 × 2G) ni 8G (4 × 2G)?

### Ejercicio 2: preparar discos para LVM

1. Crea 2 discos de 2G con el prefijo `lvm-test`
2. Adjunta ambos como `vdb` y `vdc`
3. Dentro de la VM, verifica con `lsblk`
4. *Adelanto*: ejecuta `sudo pvcreate /dev/vdb /dev/vdc` y `sudo pvs` — observa
   cómo LVM reconoce los discos como Physical Volumes
5. Limpia: `sudo pvremove /dev/vdb /dev/vdc`, luego detach y eliminar

**Pregunta de reflexión**: si creas un Volume Group con estos 2 PVs de 2G, ¿cuánto
espacio total tendrás en el VG? ¿En qué se diferencia esto de RAID1, donde 2 discos
de 2G dan solo 2G útiles?

### Ejercicio 3: script de preparación

1. Crea el script `lab-disks-create.sh` de la sección 7.1
2. Úsalo para crear y adjuntar 3 discos de 1G con prefijo `script-test`
3. Verifica que funcionó con `virsh domblklist` y `lsblk`
4. Crea el script `lab-disks-destroy.sh` de la sección 7.2
5. Úsalo para limpiar los 3 discos

**Pregunta de reflexión**: ¿por qué el script de limpieza usa `2>/dev/null` en el
`detach-disk`? ¿Qué pasaría si el disco ya fue detachado manualmente y el script
intenta detacharlo de nuevo?
