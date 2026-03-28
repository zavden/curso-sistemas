# Capítulo 11: Discos y Particiones para Virtualización

**Objetivo**: Dominar los conceptos y herramientas de gestión de discos y particiones en Linux
necesarios para crear laboratorios con QEMU/KVM. Este capítulo proporciona las bases prácticas
para que en B08 (Almacenamiento) puedas concentrarte en los conceptos avanzados (LVM, RAID,
Stratis) sin tener que aprender la herramienta al mismo tiempo.

**Prerrequisitos**: C01 (jerarquía del sistema de archivos, especialmente T03_dev), C06 (systemd),
C10 (redes para virtualización).

## Sección 1: Dispositivos de Bloque y Particiones

### T01 — Dispositivos de Bloque
- Qué es un dispositivo de bloque vs carácter
- Nomenclatura en Linux: `/dev/sd*`, `/dev/vd*`, `/dev/nvme*`, `/dev/loop*`
- `lsblk`: árbol de dispositivos y particiones
- `blkid`: identificación de filesystems (UUID, TYPE, LABEL)
- `/dev/disk/by-*`: symlinks estables (by-uuid, by-id, by-path)
- Major y minor numbers: qué representan
- Cómo un disco virtual de QEMU aparece como `/dev/vda` dentro de la VM
- Ejercicio: comparar `lsblk` del host vs una VM

### T02 — Tablas de Particiones: MBR vs GPT
- Qué es una tabla de particiones y por qué existe
- MBR: estructura (446 + 4×16 + 2 bytes), límite 2 TiB, máximo 4 primarias
- GPT: estructura (LBA 0–33, backup al final), hasta 128 particiones, soporte >2 TiB
- Particiones primarias, extendidas y lógicas (solo MBR)
- Herramientas: `fdisk` (MBR + GPT), `gdisk` (GPT), `parted` (ambos)
- `fdisk -l`: leer la tabla de particiones existente
- Crear particiones paso a paso con `fdisk` interactivo
- Cuándo usar MBR vs GPT (BIOS vs UEFI, tamaño del disco)
- Contexto QEMU: los discos virtuales nuevos no tienen tabla de particiones
- Ejercicio: crear particiones en un disco virtual dentro de una VM

## Sección 2: Sistemas de Archivos y Montaje

### T01 — Sistemas de Archivos
- Qué es un filesystem y qué estructura da al disco (superbloque, inodos, bloques de datos)
- Journaling: qué es, por qué importa, journal vs ordered vs writeback
- ext4: el estándar de facto — características, límites, herramientas (`tune2fs`, `resize2fs`)
- XFS: alto rendimiento, no se puede reducir — usado por defecto en RHEL/Fedora
- Btrfs: copy-on-write, snapshots nativos, compresión — usado por defecto en openSUSE
- `mkfs.<tipo>`: crear filesystem en una partición
- `fsck.<tipo>`: verificar y reparar (solo en particiones desmontadas)
- Ejercicio: crear ext4 y xfs en particiones de una VM, comparar con `df -Th`

### T02 — Montaje y fstab
- Concepto de montaje: asociar un filesystem a un punto del árbol de directorios
- `mount` y `umount`: uso básico y opciones comunes
- Opciones de montaje: `ro`, `noexec`, `nosuid`, `nodev`, `defaults`
- `/etc/fstab`: estructura de las 6 columnas (device, mountpoint, type, options, dump, pass)
- UUID vs path en fstab: por qué UUID es más seguro
- `findmnt`: visualizar el árbol de montajes actual
- Systemd mount units: alternativa moderna a fstab
- Montar discos adicionales en una VM: flujo completo (crear disco → attach → particionar → mkfs → mount → fstab)
- Ejercicio: añadir un segundo disco a una VM y montarlo persistentemente

## Sección 3: Imágenes de Disco Virtuales

### T01 — Formatos de Imagen
- Qué es una imagen de disco: un archivo que emula un disco completo
- Formato raw: simple, tamaño fijo, I/O directo, sin features
- Formato qcow2: thin provisioning, snapshots, compresión, cifrado, backing files
- Otros formatos: vmdk (VMware), vdi (VirtualBox), vhd/vhdx (Hyper-V)
- `qemu-img info`: inspeccionar imagen (formato, tamaño real vs virtual, snapshots)
- `qemu-img create`: crear imagen nueva (raw y qcow2)
- `qemu-img convert`: convertir entre formatos
- `qemu-img resize`: cambiar tamaño virtual de la imagen
- Thin provisioning en detalle: la imagen crece según se escribe
- Ejercicio: crear imágenes raw y qcow2, escribir datos, comparar tamaños reales

### T02 — Discos Virtuales en VMs
- Cómo QEMU presenta discos a la VM: virtio-blk vs virtio-scsi vs IDE/SATA emulado
- Bus virtio: por qué es más rápido (paravirtualización vs emulación completa)
- Añadir discos a VMs con `virsh attach-disk` y `virsh detach-disk`
- Añadir discos editando el XML de la VM (`virsh edit`)
- Redimensionar discos: qemu-img resize + growpart + resize2fs/xfs_growfs
- Snapshots de disco con qemu-img: crear, listar, revertir, borrar
- Backing files (qcow2): crear discos basados en una imagen base (linked clones)
- Flujo completo de laboratorio: crear imagen → definir VM → particionar → mkfs → montar
- Ejercicio: crear una VM con dos discos, particionar ambos, configurar montaje persistente
