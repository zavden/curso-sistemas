# Formatos de Imagen de Disco

## Erratas detectadas en el material base

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| README.md:717-721 (Error 4) | El ejemplo muestra un qcow2 de 3.1G que "explota" a 20G al copiarlo con `cp`, como si la copia materializara el tamaño virtual completo. Esto es incorrecto: con preasignación por defecto (`off`), qcow2 gestiona thin provisioning internamente (clusters no asignados simplemente no existen en el archivo), y el archivo en el host **no es sparse**. La copia con `cp` produce otro archivo de ~3.1G. El escenario descrito es propio de **archivos raw sparse**, donde el archivo aparente puede ser mucho mayor que su uso real en disco. Solo aplicaría a qcow2 si se usó `preallocation=falloc` con posterior discard (caso atípico). | Se corrige el ejemplo para distinguir claramente raw sparse (donde `cp` sí puede materializar ceros) de qcow2 (donde el riesgo real está en copias de raw). El consejo `--sparse=always` / `qemu-img convert` sigue siendo válido como buena práctica general. |

---

## Indice

1. [Que es una imagen de disco](#1-que-es-una-imagen-de-disco)
2. [Formato raw: simple y directo](#2-formato-raw-simple-y-directo)
3. [Formato qcow2: el estandar de QEMU](#3-formato-qcow2-el-estandar-de-qemu)
4. [Thin provisioning: como qcow2 ahorra espacio](#4-thin-provisioning-como-qcow2-ahorra-espacio)
5. [Backing files: imagenes base y linked clones](#5-backing-files-imagenes-base-y-linked-clones)
6. [Snapshots internos en qcow2](#6-snapshots-internos-en-qcow2)
7. [Otros formatos: vmdk, vdi, vhd](#7-otros-formatos-vmdk-vdi-vhd)
8. [qemu-img: la herramienta de gestion](#8-qemu-img-la-herramienta-de-gestion)
9. [Raw vs qcow2: cuando usar cada uno](#9-raw-vs-qcow2-cuando-usar-cada-uno)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Que es una imagen de disco

En S01 y S02 trabajaste con dispositivos de bloque reales (`/dev/vda`, `/dev/vdb`). Pero esos discos "reales" que la VM ve son, desde la perspectiva del host, **archivos normales**. Una imagen de disco es un archivo que emula un disco completo — QEMU lo presenta a la VM como si fuera hardware:

```
 Host                                    VM
┌────────────────────────────────┐    ┌─────────────────┐
│ /var/lib/libvirt/images/       │    │                 │
│   debian-lab.qcow2             │    │ /dev/vda (20G)  │
│   (archivo de 4.2 GiB)        │───►│ ├─vda1  /boot   │
│                                │    │ ├─vda2  swap    │
│ Es un archivo regular:        │    │ └─vda3  /       │
│ -rw------- 1 qemu qemu 4.2G   │    │                 │
│                                │    │ "Tengo un disco │
│                                │    │  de 20 GiB"     │
└────────────────────────────────┘    └─────────────────┘
```

El formato de ese archivo determina que funcionalidades tiene: si ocupa todo el espacio desde el inicio o crece segun se usa, si soporta snapshots, si se puede comprimir.

---

## 2. Formato raw: simple y directo

Un archivo raw es una **copia exacta, byte por byte**, de lo que la VM ve como su disco. Si la VM tiene un disco de 20 GiB, el archivo raw ocupa 20 GiB en el host — sin cabeceras, sin metadatos, sin compresion.

### Estructura interna

```
 Archivo raw de 20 GiB:

 Byte 0                                         Byte 21,474,836,479
 ┌──────────────────────────────────────────────────────────────┐
 │ [MBR/GPT] [particion 1] [particion 2] [particion 3] [vacio] │
 │                                                              │
 │  Exactamente lo que /dev/vda contiene dentro de la VM        │
 │  Sector por sector, sin transformacion alguna                │
 └──────────────────────────────────────────────────────────────┘
```

La correspondencia es directa: el byte N del archivo raw es el byte N del disco virtual. Puedes acceder al contenido con herramientas estandar de Linux:

```bash
# Mount a partition from a raw image directly on the host
sudo losetup -fP /var/lib/libvirt/images/disk.raw
sudo mount /dev/loop0p3 /mnt/vm-root

ls /mnt/vm-root/etc/hostname
# debian-lab

sudo umount /mnt/vm-root
sudo losetup -d /dev/loop0
```

### Crear una imagen raw

```bash
# Standard method with qemu-img
qemu-img create -f raw disk.raw 10G

# Equivalent but slower (writes all zeros)
dd if=/dev/zero of=disk.raw bs=1M count=10240

# Instant (creates sparse file)
truncate -s 10G disk.raw
```

Diferencia entre los tres metodos:

| Metodo | Tiempo | Espacio en disco | Contenido |
|--------|--------|:----------------:|-----------|
| `qemu-img create` | Instantaneo | ~0 (sparse) | Ceros logicos |
| `dd if=/dev/zero` | Lento (escribe todo) | 10 GiB | Ceros reales |
| `truncate -s 10G` | Instantaneo | ~0 (sparse) | Ceros logicos |

`qemu-img create` y `truncate` crean archivos **sparse**: el filesystem del host sabe que el contenido es todo ceros y no almacena los bloques vacios. El archivo "ocupa" 10 GiB de espacio logico pero casi cero espacio real:

```bash
qemu-img create -f raw disk.raw 10G

ls -lh disk.raw
# -rw-r--r-- 1 user user 10G  ...    <- logical size: 10 GiB

du -h disk.raw
# 0       disk.raw                     <- real size on disk: ~0
```

### Ventajas de raw

- **Rendimiento maximo**: sin overhead de traduccion — el kernel lee/escribe directamente.
- **Simplicidad**: cualquier herramienta puede operar sobre el archivo (dd, losetup, mount).
- **Compatibilidad**: funciona con cualquier hipervisor y herramienta.

### Desventajas de raw

- **Sin snapshots**: no hay mecanismo para guardar estados.
- **Sin compresion**: el archivo es lo que es.
- **Tamano fijo** (si no es sparse): ocupa todo el espacio declarado desde el inicio.
- **Sin backing files**: no se pueden crear clones ligeros.

---

## 3. Formato qcow2: el estandar de QEMU

qcow2 (QEMU Copy-On-Write version 2) es el formato nativo de QEMU y el mas usado en el ecosistema libvirt/KVM. Anade una capa de metadatos sobre los datos del disco que habilita funcionalidades avanzadas.

### Estructura interna

```
 Archivo qcow2:

 ┌────────────┬─────────────┬──────────────┬──────────────────┐
 │   Header   │  L1 Table   │  L2 Tables   │  Data clusters   │
 │ (104 bytes │ (punteros)  │ (punteros)   │  (datos reales)  │
 │  en v3)    │             │              │                  │
 │            │ Indice de   │ Indice de    │ Bloques de 64KiB │
 │ Magic:     │ L2 tables   │ clusters     │ (o el tamano     │
 │ "QFI\xfb"  │             │ de datos     │  configurado)    │
 │ Version: 3 │             │              │                  │
 │ Virtual    │             │              │                  │
 │ size: 20G  │             │              │                  │
 └────────────┴─────────────┴──────────────┘──────────────────┘
```

qcow2 usa una tabla de dos niveles (L1 → L2 → data cluster) similar a las page tables de un procesador:

```
 L1 Table                L2 Tables              Data Clusters
┌──────────┐          ┌──────────────┐        ┌──────────────┐
│ Entry 0 ─┼─────────►│ Cluster 0  ──┼───────►│ [64 KiB data]│
│ Entry 1 ─┼──┐      │ Cluster 1  ──┼──┐    ├──────────────┤
│ Entry 2  │  │      │ Cluster 2    │  │    │ [64 KiB data]│
│ (NULL)   │  │      │ (unallocated)│  │    └──────────────┘
└──────────┘  │      └──────────────┘  │
              │                         │
              │      ┌──────────────┐  │    ┌──────────────┐
              └─────►│ Cluster 0    │  └───►│ [64 KiB data]│
                     │ Cluster 1    │       └──────────────┘
                     │ (unallocated)│
                     └──────────────┘
```

Cuando un cluster esta marcado como **unallocated**, la VM lee ceros — pero el cluster no existe en el archivo. Solo cuando la VM escribe datos se asigna un cluster real. Esto es thin provisioning.

### Funcionalidades de qcow2

| Funcionalidad | Descripcion |
|---------------|-------------|
| **Thin provisioning** | El archivo crece segun la VM escribe datos |
| **Snapshots internos** | Guardar/restaurar el estado del disco completo |
| **Backing files** | Imagen base + deltas (linked clones) |
| **Compresion** | Clusters individuales pueden estar comprimidos (zlib/zstd) |
| **Cifrado** | Cifrado AES-256 del contenido (LUKS format) |
| **Preallocation** | Opcion de preasignar espacio para mejor rendimiento |
| **Lazy refcounts** | Mejor rendimiento en escritura a costa de un fsck mas lento |

### Crear una imagen qcow2

```bash
# Basic: 20 GiB, thin provisioned
qemu-img create -f qcow2 disk.qcow2 20G

# With metadata preallocation (better performance, little extra space)
qemu-img create -f qcow2 -o preallocation=metadata disk.qcow2 20G

# Full preallocation (maximum performance, uses all space)
qemu-img create -f qcow2 -o preallocation=full disk.qcow2 20G

# Specific cluster size (default 64K)
qemu-img create -f qcow2 -o cluster_size=128K disk.qcow2 20G

# Compression is applied during conversion, not creation:
qemu-img convert -c -f qcow2 -O qcow2 original.qcow2 compressed.qcow2
```

### Modos de preallocation

| Modo | Espacio en disco | Rendimiento | Uso |
|------|:----------------:|:-----------:|-----|
| `off` (default) | Minimo, crece | Bueno | General, desarrollo |
| `metadata` | Poco extra | Mejor | Produccion ligera |
| `falloc` | Todo preasignado | Muy bueno | Produccion |
| `full` | Todo preasignado + escrito | Maximo | Produccion exigente |

Para labs y aprendizaje, `off` (default) es perfecto — ahorra espacio en disco.

---

## 4. Thin provisioning: como qcow2 ahorra espacio

Thin provisioning es la funcionalidad mas importante de qcow2 en el dia a dia. Un disco de 20 GiB que solo tiene 3 GiB escritos ocupa ~3 GiB en el host:

```bash
qemu-img create -f qcow2 disk.qcow2 20G

qemu-img info disk.qcow2
# image: disk.qcow2
# file format: qcow2
# virtual size: 20 GiB (21474836480 bytes)     <- what the VM sees
# disk size: 196 KiB                            <- actual space used
# cluster_size: 65536
```

### Como crece el archivo

```
 Instante 0: imagen recien creada
 ┌─────────────────────────────────────────────┐
 │ Header + L1 table (196 KiB)                 │
 │ Sin clusters de datos                       │
 └─────────────────────────────────────────────┘
 Virtual: 20 GiB | Real: 196 KiB

 Instante 1: instalar SO (~3 GiB escritos)
 ┌─────────────────────────────────────────────┐
 │ Header + Tables + Clusters con datos (3 GiB)│
 │ Clusters sin escribir: no existen           │
 └─────────────────────────────────────────────┘
 Virtual: 20 GiB | Real: ~3.1 GiB

 Instante 2: VM llena de datos (20 GiB escritos)
 ┌─────────────────────────────────────────────┐
 │ Header + Tables + TODOS los clusters (20 GiB)│
 │ Todos los bloques virtuales tienen cluster  │
 └─────────────────────────────────────────────┘
 Virtual: 20 GiB | Real: ~20.1 GiB (un poco mas por metadatos)
```

### Observar el crecimiento en la practica

```bash
# Monitor real size on the host while the VM works
watch -n 5 'ls -lh /var/lib/libvirt/images/debian-lab.qcow2 && \
  qemu-img info /var/lib/libvirt/images/debian-lab.qcow2 | grep "disk size"'
```

### Thin provisioning no es gratis

**El archivo qcow2 nunca se reduce automaticamente.** Si la VM escribe 15 GiB y luego borra 10 GiB, el archivo sigue ocupando ~15 GiB en el host. Los clusters asignados no se liberan cuando la VM borra archivos — el filesystem de la VM solo marca los bloques como libres en su tabla de inodos, pero el archivo qcow2 no lo sabe.

```
 VM escribe 15 GiB:     qcow2 real = ~15 GiB
 VM borra 10 GiB:       qcow2 real = ~15 GiB  <- no se reduce
```

Para recuperar ese espacio:

```bash
# 1. Inside the VM: notify about free space
sudo fstrim -v /                    # if TRIM/discard is supported
# Or manually:
dd if=/dev/zero of=/tmp/zeros bs=1M; rm /tmp/zeros

# 2. Shut down the VM

# 3. On the host: compact the image
qemu-img convert -O qcow2 original.qcow2 compactada.qcow2
mv compactada.qcow2 original.qcow2
```

---

## 5. Backing files: imagenes base y linked clones

Un **backing file** es una imagen base de solo lectura. Puedes crear nuevas imagenes que usen esa base y solo almacenen los **cambios** (deltas). Es como un snapshot persistente que permite crear multiples VMs a partir de una plantilla sin duplicar el disco:

```
 debian-base.qcow2 (3 GiB)  <- imagen base, solo lectura
        |
        +-- lab-vm1.qcow2 (200 MiB)  <- solo los cambios de VM1
        +-- lab-vm2.qcow2 (150 MiB)  <- solo los cambios de VM2
        +-- lab-vm3.qcow2 (180 MiB)  <- solo los cambios de VM3

 Total real: 3 GiB + 200 + 150 + 180 MiB ~ 3.5 GiB
 Sin backing files: 3 GiB x 3 = 9 GiB
```

### Como funciona

Cuando la VM lee un bloque:
1. qcow2 busca en la imagen overlay (lab-vm1.qcow2).
2. Si el bloque existe ahi (fue modificado) → devuelve ese bloque.
3. Si no existe → busca en el backing file (debian-base.qcow2) → devuelve el original.

Cuando la VM escribe un bloque:
1. El bloque se escribe SOLO en la imagen overlay.
2. El backing file NUNCA se modifica.

```
 Lectura del bloque 500:

 lab-vm1.qcow2                    debian-base.qcow2
┌──────────────────┐             ┌──────────────────┐
│ Bloque 500:      │             │ Bloque 500:      │
│ (no existe)      │────────────►│ [datos originales]│ <- returns this
│                  │  "no tengo  │                  │
│ Bloque 700:      │   este      │ Bloque 700:      │
│ [datos nuevos]   │   bloque"   │ [datos originales]│
└──────────────────┘             └──────────────────┘

 Lectura del bloque 700:

 lab-vm1.qcow2
┌──────────────────┐
│ Bloque 700:      │
│ [datos nuevos]   │ <- returns this (doesn't consult backing file)
└──────────────────┘
```

### Crear un linked clone

```bash
# 1. Prepare base image (install OS, configure)
# 2. Shut down the base VM

# 3. Create overlays using the base
qemu-img create -f qcow2 -b /var/lib/libvirt/images/debian-base.qcow2 \
  -F qcow2 /var/lib/libvirt/images/lab-vm1.qcow2

qemu-img create -f qcow2 -b /var/lib/libvirt/images/debian-base.qcow2 \
  -F qcow2 /var/lib/libvirt/images/lab-vm2.qcow2
```

- `-b`: path al backing file.
- `-F`: formato del backing file.
- No necesitas especificar tamano — lo hereda del backing file.

### Verificar la cadena de backing

```bash
qemu-img info --backing-chain /var/lib/libvirt/images/lab-vm1.qcow2
```

```
image: /var/lib/libvirt/images/lab-vm1.qcow2
file format: qcow2
virtual size: 20 GiB
disk size: 200 MiB
backing file: /var/lib/libvirt/images/debian-base.qcow2
backing file format: qcow2

image: /var/lib/libvirt/images/debian-base.qcow2
file format: qcow2
virtual size: 20 GiB
disk size: 3 GiB
```

### Regla de oro: nunca modificar el backing file

Si modificas `debian-base.qcow2` despues de crear overlays, los overlays se corrompen — sus deltas son relativos al estado original de la base. La imagen base debe tratarse como **inmutable** una vez que tiene overlays.

### Commit: fusionar overlay con base

Si quieres incorporar los cambios del overlay de vuelta a la base:

```bash
# Merge lab-vm1.qcow2 -> debian-base.qcow2
qemu-img commit lab-vm1.qcow2
# The backing file now includes the changes
# WARNING: this affects ALL overlays using that base
```

En la practica, `commit` se usa poco — es mas seguro crear una nueva base.

### Rebase: cambiar el backing file

```bash
# Change the backing file of an overlay
qemu-img rebase -b base-v2.qcow2 -F qcow2 overlay.qcow2
```

Util cuando necesitas actualizar la base: crear `base-v2.qcow2` y redirigir los overlays a ella.

---

## 6. Snapshots internos en qcow2

qcow2 puede almacenar **snapshots internos** — estados completos del disco que se pueden restaurar. A diferencia de los backing files (que son archivos separados), los snapshots viven dentro del mismo archivo qcow2.

### Crear un snapshot

```bash
# From the host (VM should be shut down for internal disk snapshots)
qemu-img snapshot -c snap-before-update debian-lab.qcow2

# With virsh (can also save RAM state if VM is running):
virsh snapshot-create-as debian-lab snap-before-update \
  --description "Before apt upgrade"
```

### Listar snapshots

```bash
# With virsh
virsh snapshot-list debian-lab

# With qemu-img
qemu-img snapshot -l debian-lab.qcow2
# Snapshot list:
# ID  TAG                  VM SIZE    DATE                 VM CLOCK
# 1   snap-before-update   0 B       2026-03-19 14:30:00  00:00:00
```

### Restaurar un snapshot

```bash
# With virsh (shut down VM first for disk-only snapshots)
virsh snapshot-revert debian-lab snap-before-update

# With qemu-img (VM must be off)
qemu-img snapshot -a snap-before-update debian-lab.qcow2
```

### Borrar un snapshot

```bash
# With virsh
virsh snapshot-delete debian-lab snap-before-update

# With qemu-img
qemu-img snapshot -d snap-before-update debian-lab.qcow2
```

### Snapshots internos vs backing files

| Aspecto | Snapshots internos | Backing files |
|---------|:------------------:|:-------------:|
| Ubicacion | Dentro del mismo archivo | Archivos separados |
| Multiples VMs | No (un snapshot por VM) | Si (una base, muchos overlays) |
| Caso de uso | "Guardar estado antes de cambio" | "Crear N VMs desde plantilla" |
| Rendimiento | Ligero impacto acumulativo | Ligero impacto por lectura en cadena |
| Gestion | virsh snapshot-* / qemu-img snapshot | qemu-img create -b |

---

## 7. Otros formatos: vmdk, vdi, vhd

QEMU puede trabajar con formatos de otros hipervisores, principalmente para **migrar VMs** entre plataformas.

### vmdk (VMware)

```bash
# Native format of VMware ESXi / Workstation
qemu-img info disk.vmdk

# Convert VMDK -> qcow2 for use in QEMU/KVM
qemu-img convert -f vmdk -O qcow2 disk.vmdk disk.qcow2
```

### vdi (VirtualBox)

```bash
# Native format of Oracle VirtualBox
qemu-img convert -f vdi -O qcow2 disk.vdi disk.qcow2
```

### vhd / vhdx (Hyper-V)

```bash
# Microsoft Hyper-V format
qemu-img convert -f vpc -O qcow2 disk.vhd disk.qcow2    # vhd (format name: vpc)
qemu-img convert -f vhdx -O qcow2 disk.vhdx disk.qcow2  # vhdx
```

### Tabla de compatibilidad

| Formato | Hipervisor nativo | Thin provisioning | Snapshots | Convertible a qcow2 |
|---------|------------------|:-----------------:|:---------:|:--------------------:|
| raw | Todos | Sparse files | No | Si |
| qcow2 | QEMU/KVM | Si | Si | — |
| vmdk | VMware | Si | Si (ESXi) | Si |
| vdi | VirtualBox | Si | No | Si |
| vhd | Hyper-V (gen1) | Si | No | Si |
| vhdx | Hyper-V (gen2) | Si | No | Si |

---

## 8. qemu-img: la herramienta de gestion

`qemu-img` es la navaja suiza para imagenes de disco. Todos los subcomandos importantes:

### info: inspeccionar una imagen

```bash
qemu-img info disk.qcow2
```

```
image: disk.qcow2
file format: qcow2
virtual size: 20 GiB (21474836480 bytes)
disk size: 3.1 GiB
cluster_size: 65536
Format specific information:
    compat: 1.1
    compression type: zlib
    lazy refcounts: false
    refcount bits: 16
    corrupt: false
    extended l2: false
```

Campos clave:
- **virtual size**: lo que la VM ve como tamano del disco.
- **disk size**: espacio real que ocupa en el host.
- **cluster_size**: tamano de cada unidad de asignacion (64 KiB por defecto).
- **corrupt**: si qemu-img detecta corrupcion en los metadatos.

```bash
# With backing file chain
qemu-img info --backing-chain disk.qcow2

# JSON output (for scripts)
qemu-img info --output=json disk.qcow2
```

### create: crear imagen nueva

```bash
# Basic qcow2
qemu-img create -f qcow2 disk.qcow2 20G

# Raw
qemu-img create -f raw disk.raw 20G

# qcow2 with backing file
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2

# qcow2 with advanced options
qemu-img create -f qcow2 \
  -o preallocation=metadata,cluster_size=128K,lazy_refcounts=on \
  disk.qcow2 20G
```

### convert: convertir entre formatos

```bash
# qcow2 -> raw
qemu-img convert -f qcow2 -O raw disk.qcow2 disk.raw

# raw -> qcow2
qemu-img convert -f raw -O qcow2 disk.raw disk.qcow2

# qcow2 -> compressed qcow2 (reduces size for distribution)
qemu-img convert -c -f qcow2 -O qcow2 disk.qcow2 compressed.qcow2

# vmdk -> qcow2
qemu-img convert -f vmdk -O qcow2 disk.vmdk disk.qcow2

# Flatten backing chain into a single standalone file
qemu-img convert -f qcow2 -O qcow2 overlay.qcow2 standalone.qcow2
# standalone.qcow2 no longer depends on the backing file
```

La conversion crea un archivo **nuevo** — el original no se modifica. Los clusters vacios no se copian, asi que convertir un qcow2 fragmentado a qcow2 nuevo lo compacta.

### resize: cambiar tamano virtual

```bash
# Grow by 5 GiB (VM must be shut down)
qemu-img resize disk.qcow2 +5G

# Set exact size
qemu-img resize disk.qcow2 25G

# Shrink (dangerous - can destroy data)
qemu-img resize --shrink disk.qcow2 15G
```

**Importante**: `resize` solo cambia el tamano del "disco virtual". Dentro de la VM todavia necesitas expandir la particion y el filesystem:

```
 qemu-img resize +5G          Inside the VM:
 (changes disk size)          1. growpart /dev/vda 3  (expand partition)
                              2. resize2fs /dev/vda3  (expand ext4)
                              or xfs_growfs /         (expand XFS)
```

Esto se detalla en T02.

### check: verificar integridad

```bash
# Verify a qcow2 image
qemu-img check disk.qcow2
# No errors were found on the image.

# Repair refcount errors
qemu-img check -r all disk.qcow2
```

### snapshot: gestionar snapshots

```bash
# Create snapshot
qemu-img snapshot -c snap1 disk.qcow2

# List snapshots
qemu-img snapshot -l disk.qcow2

# Restore (apply) snapshot
qemu-img snapshot -a snap1 disk.qcow2

# Delete snapshot
qemu-img snapshot -d snap1 disk.qcow2
```

### map: ver asignacion de clusters

```bash
# Show which parts of the image have data
qemu-img map --output=json disk.qcow2 | head -20
# Shows which offsets have data and which are zero
```

---

## 9. Raw vs qcow2: cuando usar cada uno

### Comparacion directa

| Criterio | raw | qcow2 |
|----------|:---:|:-----:|
| Rendimiento I/O | Mejor (~5-10% mas rapido) | Bueno |
| Thin provisioning | Solo sparse files (depende del FS host) | Nativo |
| Snapshots | No | Si |
| Backing files | No | Si |
| Compresion | No | Si |
| Cifrado | No (usar LUKS externo) | Si (LUKS integrado) |
| Herramientas host | losetup, mount directo | Necesita qemu-nbd |
| Portabilidad | Maxima | QEMU/KVM |
| Overhead de metadatos | Ninguno | Minimo (~0.1%) |

### Diagrama de decision

```
 Que formato usar?

 Necesitas snapshots, backing files o compresion?
     Si -> qcow2
     No |
        v
 El rendimiento I/O es critico (base de datos)?
     Si -> raw (con preallocation)
     No |
        v
 Quieres ahorrar espacio en el host?
     Si -> qcow2 (thin provisioning nativo)
     No |
        v
 Necesitas acceder desde el host con losetup/mount?
     Si -> raw
     No |
        v
 -> qcow2 (default recomendado para QEMU/KVM)
```

### Para este curso

Usa **qcow2** para todo. El ahorro de espacio con thin provisioning y la capacidad de hacer snapshots antes de experimentar valen mucho mas que la pequena ganancia de rendimiento de raw.

---

## 10. Errores comunes

### Error 1: no entender virtual size vs disk size

```bash
qemu-img info disk.qcow2
# virtual size: 20 GiB
# disk size: 196 KiB

ls -lh disk.qcow2
# -rw-r--r-- 1 user user 193K disk.qcow2

# "My 20 GiB disk is only 193K! Something is wrong!"
```

**Por que confunde**: `ls -lh` muestra el tamano del archivo qcow2 en el host, no el tamano virtual del disco. Con qcow2, una imagen de 20 GiB puede ocupar solo KiB cuando esta vacia gracias al thin provisioning.

**Solucion**: siempre usar `qemu-img info` para ver ambos tamanos. La VM ve el `virtual size`; el host ocupa el `disk size`.

### Error 2: modificar el backing file despues de crear overlays

```bash
# Create overlay based on base
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2

# "Let me update the base"
virsh start vm-base    # <- modifies base.qcow2
# overlay.qcow2 is now corrupt - its deltas don't match the modified base
```

**Solucion**: una vez que una imagen tiene overlays, tratarla como **inmutable**. Si necesitas actualizar:

```bash
# Option 1: create new base
cp base.qcow2 base-v2.qcow2
# Work on base-v2, create new overlays

# Option 2: rebase overlay to new base
qemu-img rebase -b base-v2.qcow2 -F qcow2 overlay.qcow2
```

### Error 3: hacer resize sin expandir particion y filesystem

```bash
qemu-img resize disk.qcow2 +10G
virsh start debian-lab
# Inside the VM:
df -h /
# Only shows 20 GiB... where are the extra 10G?
```

**Por que pasa**: `qemu-img resize` solo cambia el tamano del disco virtual. La tabla de particiones y el filesystem siguen con el tamano original.

**Solucion**: dentro de la VM, expandir la particion y el filesystem (se detalla en T02).

### Error 4: copiar archivos raw sparse con cp

```bash
# Raw sparse file: apparent size != real size
qemu-img create -f raw disk.raw 20G
ls -lh disk.raw
# 20G   <- apparent size
du -h disk.raw
# 0     <- actual disk usage (sparse)

cp disk.raw backup.raw
du -h backup.raw
# 20G   <- cp materialized all the zero-holes!
```

**Por que pasa**: `cp` sin opciones lee el archivo completo, incluyendo las regiones sparse (rellenas logicamente con ceros). Al escribir la copia, materializa esos ceros como bloques reales.

**Nota sobre qcow2**: este problema es especifico de archivos **raw sparse**. Un archivo qcow2 con preasignacion por defecto (`off`) gestiona thin provisioning internamente mediante tablas L1/L2 — los clusters no asignados simplemente no existen en el archivo. El archivo qcow2 no es sparse a nivel del filesystem del host, asi que `cp` no causa este tipo de expansion. Solo podria ocurrir con qcow2 si se uso `preallocation=falloc` combinado con operaciones de discard posteriores.

**Solucion**:

```bash
# Option 1: cp preserving sparseness
cp --sparse=always disk.raw backup.raw

# Option 2: qemu-img convert (safer, also compacts)
qemu-img convert -O qcow2 disk.raw backup.qcow2
```

### Error 5: olvidar -F al crear overlay

```bash
qemu-img create -f qcow2 -b base.qcow2 overlay.qcow2
# WARNING: could not determine format of backing file
```

**Por que pasa**: sin `-F qcow2`, QEMU intenta autodetectar el formato del backing file, lo que es un riesgo de seguridad (un archivo malicioso podria enganar al autodetector).

**Solucion**: siempre especificar `-F`:

```bash
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│            IMAGENES DE DISCO — REFERENCIA                        │
├──────────────────────────────────────────────────────────────────┤
│ FORMATOS                                                         │
│   raw         Byte a byte, sin metadatos, maximo rendimiento    │
│   qcow2       Thin provisioning, snapshots, backing files       │
│   vmdk/vdi    VMware / VirtualBox (convertibles a qcow2)        │
├──────────────────────────────────────────────────────────────────┤
│ qemu-img SUBCOMANDOS                                             │
│   create -f qcow2 disk.qcow2 20G      Crear imagen             │
│   create -f qcow2 -b base -F qcow2    Crear overlay             │
│     overlay.qcow2                                                │
│   info disk.qcow2                      Inspeccionar              │
│   info --backing-chain overlay.qcow2   Ver cadena completa       │
│   convert -f qcow2 -O raw d.qcow2     Convertir formato         │
│     d.raw                                                        │
│   convert -c -O qcow2 d.qcow2         Compactar/comprimir       │
│     compact.qcow2                                                │
│   resize disk.qcow2 +5G               Crecer disco              │
│   resize --shrink disk.qcow2 15G      Reducir (peligroso)       │
│   check disk.qcow2                    Verificar integridad       │
│   check -r all disk.qcow2             Reparar                   │
│   snapshot -c snap1 disk.qcow2        Crear snapshot             │
│   snapshot -l disk.qcow2              Listar snapshots           │
│   snapshot -a snap1 disk.qcow2        Restaurar snapshot         │
│   snapshot -d snap1 disk.qcow2        Borrar snapshot            │
│   map --output=json disk.qcow2        Ver asignacion clusters    │
├──────────────────────────────────────────────────────────────────┤
│ TAMANOS                                                          │
│   virtual size    Lo que la VM ve (ej: 20 GiB)                  │
│   disk size       Lo que ocupa en el host (ej: 3.1 GiB)         │
│   ls -lh          Tamano aparente del archivo                    │
│   du -h           Tamano real en disco                           │
│   qemu-img info   Muestra ambos (fuente de verdad)              │
├──────────────────────────────────────────────────────────────────┤
│ BACKING FILES                                                    │
│   Crear:    qemu-img create -f qcow2 -b base -F qcow2 overlay  │
│   Regla:    NUNCA modificar el backing file                      │
│   Flatten:  qemu-img convert -O qcow2 overlay standalone        │
│   Copiar:   cp --sparse=always (para raw sparse)                │
│   Rebase:   qemu-img rebase -b nueva-base -F qcow2 overlay     │
│   Commit:   qemu-img commit overlay (fusiona en base)           │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Crear y comparar raw vs qcow2

Crea dos imagenes de 5 GiB, una raw y una qcow2, y compara sus tamanos.

```bash
qemu-img create -f raw /tmp/test-raw.img 5G
qemu-img create -f qcow2 /tmp/test-qcow2.qcow2 5G
```

1. Ejecuta `ls -lh /tmp/test-raw.img /tmp/test-qcow2.qcow2` y `du -h /tmp/test-raw.img /tmp/test-qcow2.qcow2`.

**Prediccion**: antes de ejecutar, responde:
- `ls -lh` de la raw mostrara ____
- `du -h` de la raw mostrara ____
- `ls -lh` de la qcow2 mostrara ____
- `du -h` de la qcow2 mostrara ____

<details><summary>Respuesta</summary>

- `ls -lh` raw: **5.0G** (tamano aparente = tamano virtual, porque raw es byte-a-byte)
- `du -h` raw: **0** (o pocas K) — es un archivo sparse, no se escribieron bloques reales
- `ls -lh` qcow2: **193K** (solo header + tablas L1/L2, no el tamano virtual)
- `du -h` qcow2: **193K** — el archivo realmente ocupa eso, no es sparse

La diferencia clave: raw sparse "parece" de 5G pero no ocupa nada; qcow2 directamente es un archivo pequeno que sabe internamente que representa 5G.

</details>

2. Ejecuta `qemu-img info /tmp/test-raw.img` y `qemu-img info /tmp/test-qcow2.qcow2`.

**Prediccion**: el campo `virtual size` de ambas sera ____ y el campo `disk size` de ambas sera ____

<details><summary>Respuesta</summary>

- Ambas: `virtual size: 5 GiB`
- Raw: `disk size: 0` (sparse)
- qcow2: `disk size: 196 KiB` (header + tablas)

`qemu-img info` es la fuente de verdad para entender los dos tamanos.

</details>

3. Limpia: `rm /tmp/test-raw.img /tmp/test-qcow2.qcow2`

---

### Ejercicio 2: Observar el crecimiento de raw sparse

```bash
qemu-img create -f raw /tmp/sparse.raw 10G
```

1. Verifica con `du -h` que apenas ocupa espacio.
2. Escribe 200 MiB de datos aleatorios al inicio: `dd if=/dev/urandom of=/tmp/sparse.raw bs=1M count=200 conv=notrunc`
3. Ejecuta `du -h /tmp/sparse.raw` de nuevo.

**Prediccion**: despues de `dd`, `du -h` mostrara ____ y `ls -lh` mostrara ____

<details><summary>Respuesta</summary>

- `du -h`: **200M** — solo se escribieron 200 MiB de bloques reales
- `ls -lh`: **10G** — el tamano aparente no cambio, sigue siendo 10 GiB

El archivo sparse mantiene su tamano aparente pero solo consume espacio real donde se escribieron datos. Los bloques no tocados (del byte 200M al byte 10G) siguen siendo "holes" sin bloques en disco.

</details>

4. Limpia: `rm /tmp/sparse.raw`

---

### Ejercicio 3: Modos de preallocation de qcow2

Crea tres imagenes qcow2 de 2 GiB con diferentes preasignaciones:

```bash
qemu-img create -f qcow2 /tmp/pre-off.qcow2 2G
qemu-img create -f qcow2 -o preallocation=metadata /tmp/pre-meta.qcow2 2G
qemu-img create -f qcow2 -o preallocation=falloc /tmp/pre-falloc.qcow2 2G
```

1. Compara los tres con `qemu-img info` y `du -h`.

**Prediccion**: ordena de menor a mayor `disk size`:

<details><summary>Respuesta</summary>

- `off`: **196 KiB** — solo header y tabla L1
- `metadata`: **unos pocos MiB** — tablas L1 + L2 preasignadas, pero sin clusters de datos
- `falloc`: **~2 GiB** — todo el espacio preasignado con `fallocate()`

Con `metadata`, las tablas L2 existen desde el inicio, asi que la primera escritura a cada cluster es mas rapida (no necesita asignar la tabla L2). Con `falloc`, ademas los clusters de datos estan reservados, eliminando fragmentacion.

</details>

2. Limpia: `rm /tmp/pre-off.qcow2 /tmp/pre-meta.qcow2 /tmp/pre-falloc.qcow2`

---

### Ejercicio 4: Backing files y linked clones

```bash
# Create a base image
qemu-img create -f qcow2 /tmp/base.qcow2 2G

# Create two overlays
qemu-img create -f qcow2 -b /tmp/base.qcow2 -F qcow2 /tmp/vm1.qcow2
qemu-img create -f qcow2 -b /tmp/base.qcow2 -F qcow2 /tmp/vm2.qcow2
```

1. Ejecuta `qemu-img info /tmp/vm1.qcow2`.

**Prediccion**: el campo `backing file` dira ____ y el `virtual size` sera ____

<details><summary>Respuesta</summary>

- `backing file: /tmp/base.qcow2`
- `backing file format: qcow2`
- `virtual size: 2 GiB` (heredado de la base)
- `disk size`: unos pocos KiB (solo metadata del overlay, sin datos propios aun)

El overlay no especifico tamano, asi que hereda el virtual size del backing file.

</details>

2. Ejecuta `qemu-img info --backing-chain /tmp/vm1.qcow2`.

**Prediccion**: cuantas imagenes aparecen en la cadena?

<details><summary>Respuesta</summary>

**Dos**: primero `vm1.qcow2` (el overlay) y luego `base.qcow2` (el backing file). La cadena muestra toda la jerarquia de dependencias de la imagen.

</details>

3. Limpia: `rm /tmp/base.qcow2 /tmp/vm1.qcow2 /tmp/vm2.qcow2`

---

### Ejercicio 5: Flatten de un overlay

```bash
qemu-img create -f qcow2 /tmp/base.qcow2 2G
qemu-img create -f qcow2 -b /tmp/base.qcow2 -F qcow2 /tmp/overlay.qcow2
```

1. Convierte el overlay a un archivo independiente:
   ```bash
   qemu-img convert -f qcow2 -O qcow2 /tmp/overlay.qcow2 /tmp/standalone.qcow2
   ```

2. Ejecuta `qemu-img info /tmp/standalone.qcow2`.

**Prediccion**: el campo `backing file` aparecera? Si/No y por que.

<details><summary>Respuesta</summary>

**No** — `qemu-img convert` "aplana" la cadena de backing. Lee todos los datos (del overlay y de la base) y los escribe en un unico archivo nuevo sin referencia al backing file. `standalone.qcow2` es completamente independiente.

Esto es util cuando quieres distribuir una imagen sin tener que incluir la base, o cuando quieres eliminar la dependencia del backing file.

</details>

3. Limpia: `rm /tmp/base.qcow2 /tmp/overlay.qcow2 /tmp/standalone.qcow2`

---

### Ejercicio 6: Conversion entre formatos

```bash
qemu-img create -f qcow2 /tmp/original.qcow2 1G
```

1. Convierte a raw:
   ```bash
   qemu-img convert -f qcow2 -O raw /tmp/original.qcow2 /tmp/converted.raw
   ```

2. Compara tamanos con `ls -lh` y `du -h` de ambos archivos.

**Prediccion**: el archivo raw tendra un `ls -lh` de ____ y un `du -h` de ____

<details><summary>Respuesta</summary>

- `ls -lh` raw: **1.0G** — raw siempre tiene un tamano aparente igual al virtual size
- `du -h` raw: **0** (o pocas K) — `qemu-img convert` crea archivos sparse por defecto; como el qcow2 original estaba vacio, todos los bloques son ceros

El qcow2 original: `ls -lh` ~193K, `du -h` ~193K.

</details>

3. Convierte de vuelta a qcow2:
   ```bash
   qemu-img convert -f raw -O qcow2 /tmp/converted.raw /tmp/roundtrip.qcow2
   ```

4. Verifica con `qemu-img info /tmp/roundtrip.qcow2` que el formato y virtual size son correctos.

5. Limpia: `rm /tmp/original.qcow2 /tmp/converted.raw /tmp/roundtrip.qcow2`

---

### Ejercicio 7: Resize de imagen

```bash
qemu-img create -f qcow2 /tmp/resize-test.qcow2 5G
```

1. Verifica el `virtual size` con `qemu-img info`.
2. Amplia 3 GiB: `qemu-img resize /tmp/resize-test.qcow2 +3G`
3. Verifica el nuevo `virtual size`.

**Prediccion**: despues del resize, virtual size sera ____ y disk size sera ____

<details><summary>Respuesta</summary>

- `virtual size`: **8 GiB** (5 + 3)
- `disk size`: sigue siendo minimo (~196 KiB) — resize solo cambia el numero en el header, no asigna nuevos clusters

Dentro de una VM, el disco apareceria como 8 GiB, pero la particion y el filesystem seguirian con el tamano original. Necesitarias `growpart` + `resize2fs` para expandirlos.

</details>

4. Intenta reducir (sin --shrink): `qemu-img resize /tmp/resize-test.qcow2 4G`

**Prediccion**: que ocurrira?

<details><summary>Respuesta</summary>

**Error**: `qemu-img: Use the --shrink option to perform a shrink operation.`

QEMU requiere `--shrink` explicito para reducir el tamano virtual, como proteccion contra perdida accidental de datos. Con `--shrink`: `qemu-img resize --shrink /tmp/resize-test.qcow2 4G`.

</details>

5. Limpia: `rm /tmp/resize-test.qcow2`

---

### Ejercicio 8: Snapshots con qemu-img

```bash
qemu-img create -f qcow2 /tmp/snap-test.qcow2 2G
```

1. Crea un snapshot: `qemu-img snapshot -c "estado-limpio" /tmp/snap-test.qcow2`
2. Lista snapshots: `qemu-img snapshot -l /tmp/snap-test.qcow2`

**Prediccion**: cuantos snapshots aparecen y que columnas muestra la salida?

<details><summary>Respuesta</summary>

**1 snapshot**. Columnas: `ID`, `TAG` (nombre), `VM SIZE`, `DATE`, `VM CLOCK`.

- `TAG`: "estado-limpio"
- `VM SIZE`: 0 B (no se guardo estado de RAM, solo disco)
- `VM CLOCK`: 00:00:00 (no habia VM corriendo)

</details>

3. Crea un segundo snapshot: `qemu-img snapshot -c "segundo" /tmp/snap-test.qcow2`
4. Lista de nuevo — deberian aparecer 2.
5. Restaura el primero: `qemu-img snapshot -a "estado-limpio" /tmp/snap-test.qcow2`
6. Borra ambos:
   ```bash
   qemu-img snapshot -d "estado-limpio" /tmp/snap-test.qcow2
   qemu-img snapshot -d "segundo" /tmp/snap-test.qcow2
   ```
7. Verifica con `-l` que no quedan snapshots.
8. Limpia: `rm /tmp/snap-test.qcow2`

---

### Ejercicio 9: Verificar integridad con check

```bash
qemu-img create -f qcow2 /tmp/check-test.qcow2 1G
```

1. Ejecuta `qemu-img check /tmp/check-test.qcow2`.

**Prediccion**: el resultado dira ____

<details><summary>Respuesta</summary>

`No errors were found on the image.` — la imagen recien creada no tiene errores. Tambien muestra estadisticas: `Image end offset`, numero de clusters asignados, etc.

</details>

2. Ahora intenta `qemu-img check /tmp/check-test.raw` (si tienes una imagen raw).

**Prediccion**: que ocurrira?

<details><summary>Respuesta</summary>

**Error** o mensaje indicando que raw no soporta check. `qemu-img check` solo funciona con formatos que tienen metadatos verificables (qcow2, principalmente). Raw es byte-a-byte sin estructura de metadatos, asi que no hay nada que verificar a nivel de formato.

</details>

3. Limpia: `rm /tmp/check-test.qcow2`

---

### Ejercicio 10: Copiar imagenes correctamente

```bash
qemu-img create -f raw /tmp/sparse.raw 5G
dd if=/dev/urandom of=/tmp/sparse.raw bs=1M count=100 conv=notrunc
```

1. Verifica los tamanos: `ls -lh /tmp/sparse.raw` y `du -h /tmp/sparse.raw`.

**Prediccion**: `ls -lh` mostrara ____ y `du -h` mostrara ____

<details><summary>Respuesta</summary>

- `ls -lh`: **5.0G** (tamano aparente, no cambio)
- `du -h`: **100M** (solo los 100 MiB escritos por `dd`)

</details>

2. Copia sin preservar sparseness: `cp /tmp/sparse.raw /tmp/copy-normal.raw`
3. Copia preservando sparseness: `cp --sparse=always /tmp/sparse.raw /tmp/copy-sparse.raw`
4. Compara con `du -h` las tres copias.

**Prediccion**: `du -h` de copy-normal sera ____ y de copy-sparse sera ____

<details><summary>Respuesta</summary>

- `copy-normal`: **5.0G** — `cp` leyo todo el archivo (incluyendo los ~4.9G de holes/ceros) y los escribio como bloques reales
- `copy-sparse`: **100M** — `--sparse=always` detecta secuencias de ceros y las convierte en holes, preservando la sparseness

Por eso para imagenes raw sparse siempre se debe usar `cp --sparse=always` o mejor aun `qemu-img convert`.

</details>

5. Alternativa con qemu-img:
   ```bash
   qemu-img convert -f raw -O qcow2 /tmp/sparse.raw /tmp/converted.qcow2
   du -h /tmp/converted.qcow2
   ```

**Prediccion**: el qcow2 resultante ocupara aproximadamente ____

<details><summary>Respuesta</summary>

**~100 MiB** (mas overhead de metadatos). `qemu-img convert` solo copia los bloques con datos reales y usa thin provisioning de qcow2 para el resto. Es la forma mas eficiente de copiar/convertir imagenes de disco.

</details>

6. Limpia: `rm /tmp/sparse.raw /tmp/copy-normal.raw /tmp/copy-sparse.raw /tmp/converted.qcow2`
