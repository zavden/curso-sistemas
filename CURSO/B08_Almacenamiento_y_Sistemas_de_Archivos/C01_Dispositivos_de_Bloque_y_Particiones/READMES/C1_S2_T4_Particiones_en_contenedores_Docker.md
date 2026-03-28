# Loop devices

## Índice

1. [Qué es un loop device](#1-qué-es-un-loop-device)
2. [Crear un archivo de imagen](#2-crear-un-archivo-de-imagen)
3. [losetup: asociar archivo a loop device](#3-losetup-asociar-archivo-a-loop-device)
4. [Particionar un loop device](#4-particionar-un-loop-device)
5. [Loop devices con particiones](#5-loop-devices-con-particiones)
6. [Caso de uso: practicar sin discos extra](#6-caso-de-uso-practicar-sin-discos-extra)
7. [Caso de uso: contenedores Docker](#7-caso-de-uso-contenedores-docker)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es un loop device

Un loop device permite usar un **archivo regular** como si fuera un
**dispositivo de bloque**. El kernel presenta el archivo como `/dev/loopN`,
y todas las herramientas de disco (fdisk, mkfs, mount, etc.) funcionan
sobre él como si fuera un disco real.

```
┌──────────────────────────────────────────────────────────┐
│  Disco virtual (qcow2 + VM):                            │
│                                                          │
│  archivo.qcow2 ──► QEMU ──► /dev/vdb (dentro de VM)     │
│                                                          │
│  Loop device (sin VM):                                   │
│                                                          │
│  archivo.img ──► losetup ──► /dev/loop0 (en el host)     │
│                                                          │
│  Misma idea: un archivo se comporta como un disco.       │
│  Pero loop devices funcionan directamente en el host,    │
│  sin necesidad de una VM.                                │
└──────────────────────────────────────────────────────────┘
```

### Cuándo usar loop devices

| Situación | ¿Loop device? |
|-----------|---------------|
| No tienes VMs configuradas todavía | Sí — practicar sin VM |
| Quieres practicar en un contenedor Docker | Sí — Docker no tiene discos extra |
| Ya tienes VMs con discos extra (C00) | No necesario — usa los discos extra |
| Montar una imagen ISO | Sí — `mount -o loop` |
| Crear un filesystem cifrado de prueba | Sí — útil para labs de LUKS |

---

## 2. Crear un archivo de imagen

### 2.1. Con dd

```bash
# Crear un archivo de 200MB lleno de zeros
dd if=/dev/zero of=disk.img bs=1M count=200
```

```
200+0 records in
200+0 records out
209715200 bytes (210 MB, 200 MiB) copied, 0.123 s, 1.7 GB/s
```

El archivo ocupa 200MB reales en disco (no es thin provisioning).

### 2.2. Con truncate (sparse file)

```bash
# Crear un archivo sparse de 1GB (ocupa ~0 bytes reales)
truncate -s 1G disk-sparse.img
```

```bash
# Comparar tamaño aparente vs real
ls -lh disk-sparse.img
# -rw-r--r-- 1 user user 1.0G  ... disk-sparse.img

du -h disk-sparse.img
# 0       disk-sparse.img     ← ocupa 0 bytes reales
```

Un sparse file es análogo al thin provisioning de qcow2: el archivo "dice"
que mide 1GB, pero solo ocupa espacio cuando se escriben datos.

### 2.3. Con fallocate

```bash
# Pre-allocar espacio sin escribir zeros
fallocate -l 500M disk-alloc.img
```

`fallocate` reserva el espacio en el filesystem sin escribir datos. Es más
rápido que `dd` para archivos grandes.

### 2.4. Comparación

| Método | Velocidad | Espacio real | Cuándo usar |
|--------|-----------|--------------|-------------|
| `dd if=/dev/zero` | Lento | Todo el tamaño | Cuando necesitas datos inicializados |
| `truncate -s` | Instantáneo | ~0 (sparse) | Labs rápidos, pruebas |
| `fallocate -l` | Rápido | Todo el tamaño | Pre-alocar sin initializar |

Para labs, `truncate` es ideal — crea archivos grandes instantáneamente.

---

## 3. losetup: asociar archivo a loop device

### 3.1. Asociar un archivo

```bash
# Crear el archivo
truncate -s 500M /tmp/disk.img

# Asociar al primer loop device disponible
sudo losetup --find --show /tmp/disk.img
# /dev/loop0
```

| Flag | Significado |
|------|-------------|
| `--find` (`-f`) | Buscar el primer loop device libre |
| `--show` | Imprimir el nombre del device asignado |

Ahora `/dev/loop0` se comporta como un disco de 500MB.

### 3.2. Verificar

```bash
# Ver todos los loop devices activos
losetup -a
# /dev/loop0: [65025]:12345 (/tmp/disk.img)

# Ver en lsblk
lsblk /dev/loop0
# NAME  MAJ:MIN RM  SIZE RO TYPE MOUNTPOINTS
# loop0   7:0    0  500M  0 loop
```

### 3.3. Desasociar

```bash
sudo losetup -d /dev/loop0
```

Esto libera el loop device. El archivo `/tmp/disk.img` sigue existiendo con
los datos que se escribieron.

### 3.4. Asociar a un loop device específico

```bash
# Forzar un loop device particular
sudo losetup /dev/loop5 /tmp/disk.img
```

### 3.5. Loop device de solo lectura

```bash
sudo losetup --read-only --find --show /tmp/disk.img
# /dev/loop0 (read-only)
```

Útil para inspeccionar imágenes sin riesgo de modificarlas.

---

## 4. Particionar un loop device

Una vez asociado, el loop device se particiona exactamente igual que cualquier
disco:

### 4.1. Con fdisk

```bash
# Crear archivo y asociar
truncate -s 1G /tmp/lab-disk.img
sudo losetup --find --show /tmp/lab-disk.img
# /dev/loop0

# Particionar
sudo fdisk /dev/loop0
```

```
Command: o                          ← tabla MBR
Command: n → p → 1 → Enter → +500M  ← partición 1
Command: n → p → 2 → Enter → Enter  ← partición 2 (resto)
Command: t → 2 → 82                 ← swap
Command: w
```

### 4.2. Con parted

```bash
sudo parted -s /dev/loop0 mklabel gpt
sudo parted -s /dev/loop0 mkpart primary ext4 1MiB 500MiB
sudo parted -s /dev/loop0 mkpart primary linux-swap 500MiB 100%
```

### 4.3. Verificar

```bash
sudo fdisk -l /dev/loop0
```

```
Disk /dev/loop0: 1 GiB, 1073741824 bytes, 2097152 sectors
...
Device       Boot  Start     End Sectors  Size Id Type
/dev/loop0p1        2048 1026047 1024000  500M 83 Linux
/dev/loop0p2     1026048 2097151 1071104  523M 82 Linux swap
```

---

## 5. Loop devices con particiones

### 5.1. El problema: loop0 vs loop0p1

Después de particionar, necesitas que el kernel cree los devices de las
particiones (`/dev/loop0p1`, `/dev/loop0p2`). Esto no siempre ocurre
automáticamente:

```bash
ls /dev/loop0*
# /dev/loop0              ← el disco, pero ¿dónde están loop0p1, loop0p2?
```

### 5.2. Solución: partprobe o losetup -P

**Opción A**: `partprobe`

```bash
sudo partprobe /dev/loop0
ls /dev/loop0*
# /dev/loop0  /dev/loop0p1  /dev/loop0p2
```

**Opción B**: re-asociar con `-P` (partitions)

```bash
sudo losetup -d /dev/loop0
sudo losetup -P --find --show /tmp/lab-disk.img
# /dev/loop0

ls /dev/loop0*
# /dev/loop0  /dev/loop0p1  /dev/loop0p2
```

El flag `-P` le dice a losetup que escanee la tabla de particiones y cree los
devices de partición.

**Opción C**: `kpartx` (paquete `kpartx`)

```bash
sudo kpartx -av /tmp/lab-disk.img
# add map loop0p1 (253:0): ...
# add map loop0p2 (253:1): ...

# Los devices aparecen en /dev/mapper/
ls /dev/mapper/loop0p*
# /dev/mapper/loop0p1  /dev/mapper/loop0p2
```

### 5.3. Crear filesystem y montar

Con las particiones visibles:

```bash
# Crear filesystem
sudo mkfs.ext4 /dev/loop0p1
sudo mkswap /dev/loop0p2

# Montar
sudo mkdir -p /mnt/loop-test
sudo mount /dev/loop0p1 /mnt/loop-test

# Verificar
df -h /mnt/loop-test
# /dev/loop0p1  488M  24K  452M   1% /mnt/loop-test

# Usar normalmente
echo "Hello from loop device" | sudo tee /mnt/loop-test/test.txt
```

### 5.4. Limpiar

```bash
# Desmontar
sudo umount /mnt/loop-test

# Desasociar
sudo losetup -d /dev/loop0

# Eliminar archivo (opcional)
rm /tmp/lab-disk.img
```

---

## 6. Caso de uso: practicar sin discos extra

### 6.1. Escenario

No tienes VMs configuradas o no quieres encender una VM solo para practicar
particionado. Loop devices te permiten practicar directamente en tu host:

```
┌────────────────────────────────────────────────────────────┐
│  Sin VM ni discos extra disponibles:                      │
│                                                            │
│  1. truncate -s 2G /tmp/practice.img                       │
│  2. sudo losetup -Pf --show /tmp/practice.img              │
│  3. sudo fdisk /dev/loop0   ← particionar                  │
│  4. sudo mkfs.ext4 /dev/loop0p1  ← crear filesystem        │
│  5. sudo mount /dev/loop0p1 /mnt  ← montar                 │
│  6. ... practicar ...                                      │
│  7. sudo umount /mnt                                       │
│  8. sudo losetup -d /dev/loop0                             │
│  9. rm /tmp/practice.img                                   │
│                                                            │
│  Todo ocurre en el host. No necesitas VM ni discos reales. │
└────────────────────────────────────────────────────────────┘
```

### 6.2. Lab de LVM con loop devices

```bash
# Crear 3 "discos" de 500M
for i in 1 2 3; do
  truncate -s 500M /tmp/lvm-disk${i}.img
  sudo losetup -f --show /tmp/lvm-disk${i}.img
done
# /dev/loop0
# /dev/loop1
# /dev/loop2

# Ahora tienes 3 dispositivos de bloque para practicar LVM
sudo pvcreate /dev/loop0 /dev/loop1 /dev/loop2
sudo vgcreate vg-test /dev/loop0 /dev/loop1 /dev/loop2
sudo lvcreate -L 1G -n lv-data vg-test

# Limpiar
sudo lvremove -y vg-test/lv-data
sudo vgremove vg-test
sudo pvremove /dev/loop0 /dev/loop1 /dev/loop2
for i in 0 1 2; do sudo losetup -d /dev/loop${i}; done
rm /tmp/lvm-disk{1,2,3}.img
```

### 6.3. Lab de RAID con loop devices

```bash
# Crear 4 "discos" de 200M
for i in 1 2 3 4; do
  truncate -s 200M /tmp/raid-disk${i}.img
  sudo losetup -f --show /tmp/raid-disk${i}.img
done

# RAID5 con 3 discos + 1 spare
sudo mdadm --create /dev/md0 --level=5 \
  --raid-devices=3 /dev/loop0 /dev/loop1 /dev/loop2 \
  --spare-devices=1 /dev/loop3

# Verificar
cat /proc/mdstat

# Limpiar
sudo mdadm --stop /dev/md0
sudo mdadm --zero-superblock /dev/loop{0,1,2,3}
for i in 0 1 2 3; do sudo losetup -d /dev/loop${i}; done
rm /tmp/raid-disk{1,2,3,4}.img
```

---

## 7. Caso de uso: contenedores Docker

### 7.1. El problema con Docker

Docker comparte el kernel del host y no tiene discos propios. No puedes
adjuntar `/dev/vdb` a un contenedor como en una VM. Pero puedes usar
loop devices:

```
┌────────────────────────────────────────────────────────────┐
│  VM (QEMU/KVM):                                           │
│  ┌─────────────┐                                           │
│  │ /dev/vdb    │ ← disco virtual real, con su propio driver│
│  │ /dev/vdc    │ ← hot-plug disponible                     │
│  └─────────────┘                                           │
│                                                            │
│  Contenedor Docker:                                        │
│  ┌─────────────┐                                           │
│  │ No hay      │ ← no existe /dev/vdb                      │
│  │ discos      │ ← comparte kernel con el host             │
│  │ propios     │                                           │
│  └─────────────┘                                           │
│                                                            │
│  Solución: crear loop devices dentro del contenedor        │
│  (requiere --privileged)                                   │
└────────────────────────────────────────────────────────────┘
```

### 7.2. Loop devices en Docker

```bash
# Contenedor privileged con acceso a loop devices
docker run -it --privileged debian:12 bash
```

Dentro del contenedor:

```bash
# Instalar herramientas
apt-get update && apt-get install -y fdisk parted lvm2 mdadm

# Crear un "disco" con loop device
truncate -s 500M /tmp/disk.img
losetup -Pf --show /tmp/disk.img
# /dev/loop0

# Ahora puedes particionar, crear filesystems, etc.
fdisk /dev/loop0
# ... particionar ...

mkfs.ext4 /dev/loop0p1
mount /dev/loop0p1 /mnt
```

### 7.3. Limitaciones de Docker vs VM

| Aspecto | VM con discos extra | Docker con loop devices |
|---------|--------------------|-----------------------|
| Rendimiento | Bueno (virtio) | Bueno (directo al host fs) |
| Hot-plug | Sí (`virsh attach-disk`) | No |
| Aislamiento | Completo (kernel propio) | Parcial (kernel compartido) |
| `--privileged` | No necesario | Sí (requerido) |
| RAID/LVM | Funciona completamente | Funciona (con `--privileged`) |
| GRUB/Boot | Sí | No (no hay proceso de boot) |
| Producción | Sí | Solo para labs |

### 7.4. Recomendación para el curso

```
Para B08 (particiones, LVM, RAID, filesystems):
  VMs con discos extra    ← RECOMENDADO (C00 ya está configurado)
  Docker + loop devices   ← alternativa rápida si no tienes VM

Para B09-B11 (redes, servicios, seguridad, boot):
  VMs obligatorias        ← Docker no puede simular boot/kernel/firewalls
```

---

## 8. Errores comunes

### Error 1: olvidar `-P` y no ver las particiones

```bash
# ✗ Asociar sin -P
sudo losetup -f --show /tmp/disk.img
# /dev/loop0
ls /dev/loop0*
# /dev/loop0              ← solo el disco, sin loop0p1

# ✓ Usar -P o partprobe
sudo losetup -Pf --show /tmp/disk.img
# o
sudo partprobe /dev/loop0
ls /dev/loop0*
# /dev/loop0  /dev/loop0p1  /dev/loop0p2
```

### Error 2: olvidar desasociar antes de borrar el archivo

```bash
# ✗ Borrar el archivo con el loop device activo
rm /tmp/disk.img
# El loop device sigue activo pero apuntando a un archivo borrado
# Puede causar corrupción de datos o comportamiento extraño
losetup -a
# /dev/loop0: [65025]:12345 (/tmp/disk.img (deleted))

# ✓ Desasociar primero
sudo umount /mnt          # si estaba montado
sudo losetup -d /dev/loop0
rm /tmp/disk.img
```

### Error 3: quedarse sin loop devices

```bash
# Si creas muchos loops sin liberarlos:
losetup -a
# /dev/loop0: ... disk1.img
# /dev/loop1: ... disk2.img
# ...
# /dev/loop15: ... disk16.img

# ✓ Liberar los que no uses
sudo losetup -d /dev/loop5

# ✓ Liberar TODOS
sudo losetup -D              # detach all
```

### Error 4: Docker sin --privileged

```bash
# ✗ Contenedor sin privilegios
docker run -it debian:12 bash
losetup -f --show /tmp/disk.img
# EPERM: Operation not permitted

# ✓ Con --privileged
docker run -it --privileged debian:12 bash
losetup -f --show /tmp/disk.img
# /dev/loop0
```

### Error 5: confundir mount -o loop con losetup

```bash
# mount -o loop es un atajo que hace losetup + mount en un paso:
sudo mount -o loop /tmp/disk.img /mnt
# Equivale a:
#   sudo losetup -f --show /tmp/disk.img  → /dev/loop0
#   sudo mount /dev/loop0 /mnt

# mount -o loop funciona solo si el archivo tiene un filesystem
# directamente (sin tabla de particiones)

# Si el archivo tiene particiones, necesitas losetup -P
```

---

## 9. Cheatsheet

```bash
# ── Crear archivos de imagen ────────────────────────────────────
truncate -s 1G /tmp/disk.img         # sparse (instantáneo, ~0 bytes)
dd if=/dev/zero of=/tmp/disk.img bs=1M count=500   # zeros (lento, real)
fallocate -l 500M /tmp/disk.img      # pre-alocar (rápido, real)

# ── losetup: asociar / desasociar ────────────────────────────────
sudo losetup -Pf --show /tmp/disk.img    # asociar con particiones
sudo losetup -f --show /tmp/disk.img     # asociar sin particiones
sudo losetup /dev/loop5 /tmp/disk.img    # asociar a loop específico
sudo losetup --read-only -f --show FILE  # solo lectura
sudo losetup -d /dev/loop0               # desasociar uno
sudo losetup -D                          # desasociar TODOS

# ── Verificar ────────────────────────────────────────────────────
losetup -a                               # listar loops activos
lsblk /dev/loop0                         # ver como block device

# ── Particiones en loop devices ──────────────────────────────────
sudo fdisk /dev/loop0                    # particionar
sudo partprobe /dev/loop0               # forzar kernel a ver particiones
ls /dev/loop0*                           # verificar loop0p1, loop0p2...

# ── Filesystem y mount ───────────────────────────────────────────
sudo mkfs.ext4 /dev/loop0p1             # crear filesystem
sudo mount /dev/loop0p1 /mnt            # montar partición
sudo mount -o loop /tmp/simple.img /mnt  # atajo (sin particiones)

# ── Limpiar ──────────────────────────────────────────────────────
sudo umount /mnt
sudo losetup -d /dev/loop0
rm /tmp/disk.img

# ── Lab rápido de LVM (sin VM) ──────────────────────────────────
for i in 1 2 3; do
  truncate -s 500M /tmp/lvm${i}.img
  sudo losetup -f --show /tmp/lvm${i}.img
done
# ... practicar LVM con /dev/loop0, loop1, loop2 ...
for i in 0 1 2; do sudo losetup -d /dev/loop${i}; done
rm /tmp/lvm{1,2,3}.img

# ── Docker ───────────────────────────────────────────────────────
docker run -it --privileged debian:12 bash
# Dentro: truncate + losetup + fdisk + mkfs + mount
```

---

## 10. Ejercicios

### Ejercicio 1: loop device básico

1. Crea un archivo sparse de 500M con `truncate`
2. Asócialo con `losetup -Pf --show`
3. Crea una tabla MBR y una partición con `fdisk`
4. Verifica que `/dev/loop0p1` existe (`ls /dev/loop0*`)
   - Si no existe, usa `partprobe`
5. Crea un filesystem ext4 en la partición
6. Monta, crea un archivo de texto, verifica con `cat`
7. Desmonta, desasocia y elimina el archivo imagen

**Pregunta de reflexión**: el archivo sparse ocupa ~0 bytes al crearlo. Después
de crear el filesystem y escribir datos, ¿cuánto ocupa realmente? Verifica con
`du -h` vs `ls -lh`. ¿Por qué la diferencia?

### Ejercicio 2: múltiples loop devices para LVM

1. Crea 2 archivos sparse de 300M cada uno
2. Asócialos a loop devices
3. Crea Physical Volumes: `sudo pvcreate /dev/loop0 /dev/loop1`
4. Crea un Volume Group: `sudo vgcreate vg-loop /dev/loop0 /dev/loop1`
5. Crea un Logical Volume de 400M: `sudo lvcreate -L 400M -n lv-test vg-loop`
6. Crea filesystem y monta el LV
7. Verifica con `pvs`, `vgs`, `lvs`, `df -h`
8. Limpia todo en el orden correcto

**Pregunta de reflexión**: el LV de 400M se extiende sobre 2 archivos de 300M.
¿Cómo sabe LVM dónde está cada parte de los datos? ¿Qué pasaría si borras uno
de los archivos imagen sin limpiar LVM primero?

### Ejercicio 3: comparar loop device vs disco virtual

1. En tu VM de lab, ejecuta `lsblk` — observa los discos virtio (`vdX`)
2. Dentro de la VM, crea un loop device de 200M
3. Particiona el loop device con `fdisk`
4. Particiona `/dev/vdb` (disco virtual) con `fdisk`
5. Compara las salidas de `lsblk` — ¿en qué se diferencian `loop0` y `vdb`?
6. Limpia el loop device

**Pregunta de reflexión**: tanto el loop device como el disco virtio son
"virtuales" — ninguno corresponde a hardware real. ¿Cuál tiene mejor
rendimiento de I/O y por qué? Pista: piensa en cuántas capas de indirección
tiene cada uno (archivo → filesystem del host → loop → filesystem del loop
vs qcow2 → QEMU → virtio → filesystem de la VM).
