# Formatos de Imagen de Disco

## Índice

1. [Qué es una imagen de disco](#1-qué-es-una-imagen-de-disco)
2. [Formato raw: simple y directo](#2-formato-raw-simple-y-directo)
3. [Formato qcow2: el estándar de QEMU](#3-formato-qcow2-el-estándar-de-qemu)
4. [Thin provisioning: cómo qcow2 ahorra espacio](#4-thin-provisioning-cómo-qcow2-ahorra-espacio)
5. [Backing files: imágenes base y linked clones](#5-backing-files-imágenes-base-y-linked-clones)
6. [Snapshots internos en qcow2](#6-snapshots-internos-en-qcow2)
7. [Otros formatos: vmdk, vdi, vhd](#7-otros-formatos-vmdk-vdi-vhd)
8. [qemu-img: la herramienta de gestión](#8-qemu-img-la-herramienta-de-gestión)
9. [Raw vs qcow2: cuándo usar cada uno](#9-raw-vs-qcow2-cuándo-usar-cada-uno)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es una imagen de disco

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

El formato de ese archivo determina qué funcionalidades tiene: ¿ocupa todo el espacio desde el inicio o crece según se usa? ¿Soporta snapshots? ¿Se puede comprimir?

---

## 2. Formato raw: simple y directo

Un archivo raw es una **copia exacta, byte por byte**, de lo que la VM ve como su disco. Si la VM tiene un disco de 20 GiB, el archivo raw ocupa 20 GiB en el host — sin cabeceras, sin metadatos, sin compresión.

### Estructura interna

```
 Archivo raw de 20 GiB:

 Byte 0                                         Byte 21,474,836,479
 ┌──────────────────────────────────────────────────────────────┐
 │ [MBR/GPT] [partición 1] [partición 2] [partición 3] [vacío] │
 │                                                              │
 │  Exactamente lo que /dev/vda contiene dentro de la VM        │
 │  Sector por sector, sin transformación alguna                │
 └──────────────────────────────────────────────────────────────┘
```

La correspondencia es directa: el byte N del archivo raw es el byte N del disco virtual. Esto significa que puedes acceder al contenido con herramientas estándar de Linux:

```bash
# Montar una partición de una imagen raw directamente en el host
sudo losetup -fP /var/lib/libvirt/images/disk.raw
sudo mount /dev/loop0p3 /mnt/vm-root
# Ahora ves el filesystem de la VM como si fuera un disco local

ls /mnt/vm-root/etc/hostname
# debian-lab

sudo umount /mnt/vm-root
sudo losetup -d /dev/loop0
```

### Crear una imagen raw

```bash
# Con qemu-img (la manera estándar)
qemu-img create -f raw disk.raw 10G

# Con dd (equivalente pero más lento)
dd if=/dev/zero of=disk.raw bs=1M count=10240

# Con truncate (instantáneo — crea archivo sparse)
truncate -s 10G disk.raw
```

La diferencia entre estos tres métodos:

| Método | Tiempo | Espacio en disco | Contenido |
|--------|--------|:----------------:|-----------|
| `qemu-img create` | Instantáneo | ~0 (sparse) | Ceros lógicos |
| `dd if=/dev/zero` | Lento (escribe todo) | 10 GiB | Ceros reales |
| `truncate -s 10G` | Instantáneo | ~0 (sparse) | Ceros lógicos |

`qemu-img create` y `truncate` crean archivos **sparse**: el filesystem del host sabe que el contenido es todo ceros y no almacena los bloques vacíos. El archivo "ocupa" 10 GiB de espacio lógico pero casi cero espacio real:

```bash
qemu-img create -f raw disk.raw 10G

ls -lh disk.raw
# -rw-r--r-- 1 user user 10G  ...    ← tamaño lógico: 10 GiB

du -h disk.raw
# 0       disk.raw                     ← tamaño real: ~0
```

### Ventajas de raw

- **Rendimiento máximo**: sin overhead de traducción — el kernel lee/escribe directamente.
- **Simplicidad**: cualquier herramienta puede operar sobre el archivo (dd, losetup, mount).
- **Compatibilidad**: funciona con cualquier hipervisor y herramienta.

### Desventajas de raw

- **Sin snapshots**: no hay mecanismo para guardar estados.
- **Sin compresión**: el archivo es lo que es.
- **Tamaño fijo** (si no es sparse): ocupa todo el espacio declarado desde el inicio.
- **Sin backing files**: no se pueden crear clones ligeros.

---

## 3. Formato qcow2: el estándar de QEMU

qcow2 (QEMU Copy-On-Write version 2) es el formato nativo de QEMU y el más usado en el ecosistema libvirt/KVM. Añade una capa de metadatos sobre los datos del disco que habilita funcionalidades avanzadas.

### Estructura interna

```
 Archivo qcow2:

 ┌────────────┬─────────────┬──────────────┬──────────────────┐
 │   Header   │  L1 Table   │  L2 Tables   │  Data clusters   │
 │  (72+ bytes)│ (punteros) │ (punteros)   │  (datos reales)  │
 │            │             │              │                  │
 │ Magic:     │ Índice de   │ Índice de    │ Bloques de 64KiB │
 │ "QFI\xfb"  │ L2 tables   │ clusters     │ (o el tamaño     │
 │ Version: 3 │             │ de datos     │  configurado)    │
 │ Virtual    │             │              │                  │
 │ size: 20G  │             │              │                  │
 └────────────┴─────────────┴──────────────┴──────────────────┘
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

Cuando un cluster está marcado como **unallocated**, la VM lee ceros — pero el cluster no existe en el archivo. Solo cuando la VM escribe datos se asigna un cluster real. Esto es thin provisioning.

### Funcionalidades de qcow2

| Funcionalidad | Descripción |
|---------------|-------------|
| **Thin provisioning** | El archivo crece según la VM escribe datos |
| **Snapshots internos** | Guardar/restaurar el estado del disco completo |
| **Backing files** | Imagen base + deltas (linked clones) |
| **Compresión** | Clusters individuales pueden estar comprimidos (zlib/zstd) |
| **Cifrado** | Cifrado AES-256 del contenido (LUKS format) |
| **Preallocation** | Opción de preasignar espacio para mejor rendimiento |
| **Lazy refcounts** | Mejor rendimiento en escritura a costa de un fsck más lento |

### Crear una imagen qcow2

```bash
# Básico: 20 GiB, thin provisioned
qemu-img create -f qcow2 disk.qcow2 20G

# Con preallocation de metadatos (mejor rendimiento, poco espacio extra)
qemu-img create -f qcow2 -o preallocation=metadata disk.qcow2 20G

# Con preallocation completa (rendimiento máximo, ocupa todo)
qemu-img create -f qcow2 -o preallocation=full disk.qcow2 20G

# Con tamaño de cluster específico (default 64K)
qemu-img create -f qcow2 -o cluster_size=128K disk.qcow2 20G

# Con compresión (se comprime al escribir, útil para plantillas)
# La compresión se aplica al convertir, no al crear:
qemu-img convert -c -f qcow2 -O qcow2 original.qcow2 compressed.qcow2
```

### Modos de preallocation

| Modo | Espacio en disco | Rendimiento | Uso |
|------|:----------------:|:-----------:|-----|
| `off` (default) | Mínimo, crece | Bueno | General, desarrollo |
| `metadata` | Poco extra | Mejor | Producción ligera |
| `falloc` | Todo preasignado | Muy bueno | Producción |
| `full` | Todo preasignado + escrito | Máximo | Producción exigente |

Para labs y aprendizaje, `off` (default) es perfecto — ahorra espacio en disco.

---

## 4. Thin provisioning: cómo qcow2 ahorra espacio

Thin provisioning es la funcionalidad más importante de qcow2 en el día a día. Un disco de 20 GiB que solo tiene 3 GiB escritos ocupa ~3 GiB en el host:

```bash
qemu-img create -f qcow2 disk.qcow2 20G

qemu-img info disk.qcow2
# image: disk.qcow2
# file format: qcow2
# virtual size: 20 GiB (21474836480 bytes)     ← lo que la VM ve
# disk size: 196 KiB                            ← lo que ocupa realmente
# cluster_size: 65536
```

### Cómo crece el archivo

```
 Instante 0: imagen recién creada
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
 Virtual: 20 GiB | Real: ~20.1 GiB (un poco más por metadatos)
```

### Observar el crecimiento en la práctica

```bash
# En el host, monitorear el tamaño real mientras la VM trabaja
watch -n 5 'ls -lh /var/lib/libvirt/images/debian-lab.qcow2 && qemu-img info /var/lib/libvirt/images/debian-lab.qcow2 | grep "disk size"'
```

### Thin provisioning no es gratis

**El archivo qcow2 nunca se reduce automáticamente.** Si la VM escribe 15 GiB y luego borra 10 GiB, el archivo sigue ocupando ~15 GiB en el host. Los clusters asignados no se liberan cuando la VM borra archivos — el filesystem de la VM solo marca los bloques como libres en su tabla de inodos, pero el archivo qcow2 no lo sabe.

```
 VM escribe 15 GiB:     qcow2 real = ~15 GiB
 VM borra 10 GiB:       qcow2 real = ~15 GiB  ← ¡no se reduce!
```

Para recuperar ese espacio:

```bash
# 1. Dentro de la VM: llenar espacio libre con ceros
sudo fstrim -v /                    # si soporta TRIM/discard
# o manualmente:
dd if=/dev/zero of=/tmp/zeros bs=1M; rm /tmp/zeros

# 2. Apagar la VM

# 3. En el host: compactar la imagen
qemu-img convert -O qcow2 original.qcow2 compactada.qcow2
mv compactada.qcow2 original.qcow2
```

---

## 5. Backing files: imágenes base y linked clones

Un **backing file** es una imagen base de solo lectura. Puedes crear nuevas imágenes que usen esa base y solo almacenen los **cambios** (deltas). Es como un snapshot persistente que permite crear múltiples VMs a partir de una plantilla sin duplicar el disco:

```
 debian-base.qcow2 (3 GiB)  ← imagen base, solo lectura
        │
        ├── lab-vm1.qcow2 (200 MiB)  ← solo los cambios de VM1
        ├── lab-vm2.qcow2 (150 MiB)  ← solo los cambios de VM2
        └── lab-vm3.qcow2 (180 MiB)  ← solo los cambios de VM3

 Total real: 3 GiB + 200 + 150 + 180 MiB ≈ 3.5 GiB
 Sin backing files: 3 GiB × 3 = 9 GiB
```

### Cómo funciona

Cuando la VM lee un bloque:
1. qcow2 busca en la imagen overlay (lab-vm1.qcow2).
2. Si el bloque existe ahí (fue modificado) → devuelve ese bloque.
3. Si no existe → busca en el backing file (debian-base.qcow2) → devuelve el original.

Cuando la VM escribe un bloque:
1. El bloque se escribe SOLO en la imagen overlay.
2. El backing file NUNCA se modifica.

```
 Lectura del bloque 500:

 lab-vm1.qcow2                    debian-base.qcow2
┌──────────────────┐             ┌──────────────────┐
│ Bloque 500:      │             │ Bloque 500:      │
│ (no existe)      │────────────►│ [datos originales]│ ← devuelve esto
│                  │  "no tengo  │                  │
│ Bloque 700:      │   este      │ Bloque 700:      │
│ [datos nuevos]   │   bloque"   │ [datos originales]│
└──────────────────┘             └──────────────────┘

 Lectura del bloque 700:

 lab-vm1.qcow2
┌──────────────────┐
│ Bloque 700:      │
│ [datos nuevos]   │ ← devuelve esto (no consulta backing file)
└──────────────────┘
```

### Crear un linked clone

```bash
# 1. Preparar la imagen base (instalar SO, configurar)
# ... (proceso normal de instalación)

# 2. Apagar la VM base

# 3. Crear overlays que usan la base
qemu-img create -f qcow2 -b /var/lib/libvirt/images/debian-base.qcow2 \
  -F qcow2 /var/lib/libvirt/images/lab-vm1.qcow2

qemu-img create -f qcow2 -b /var/lib/libvirt/images/debian-base.qcow2 \
  -F qcow2 /var/lib/libvirt/images/lab-vm2.qcow2
```

- `-b`: path al backing file.
- `-F`: formato del backing file.
- No necesitas especificar tamaño — lo hereda del backing file.

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

Si modificas `debian-base.qcow2` después de crear overlays, los overlays se corrompen — sus deltas son relativos al estado original de la base. Por eso la imagen base debe tratarse como **inmutable** una vez que tiene overlays.

### Commit: fusionar overlay con base

Si quieres incorporar los cambios del overlay de vuelta a la base:

```bash
# Fusionar lab-vm1.qcow2 → debian-base.qcow2
qemu-img commit lab-vm1.qcow2
# El backing file ahora incluye los cambios
# ⚠️ Esto afecta a TODOS los overlays que usen esa base
```

En la práctica, `commit` se usa poco — es más seguro crear una nueva base.

---

## 6. Snapshots internos en qcow2

qcow2 puede almacenar **snapshots internos** — estados completos del disco que se pueden restaurar. A diferencia de los backing files (que son archivos separados), los snapshots viven dentro del mismo archivo qcow2.

### Crear un snapshot

```bash
# Desde el host (VM puede estar corriendo o parada)
virsh snapshot-create-as debian-lab snap-before-update \
  --description "Antes de apt upgrade"

# Solo del disco (sin estado de RAM):
qemu-img snapshot -c snap-before-update debian-lab.qcow2
```

### Listar snapshots

```bash
# Con virsh
virsh snapshot-list debian-lab

# Con qemu-img
qemu-img snapshot -l debian-lab.qcow2
# Snapshot list:
# ID  TAG                  VM SIZE    DATE                 VM CLOCK
# 1   snap-before-update   0 B       2026-03-19 14:30:00  00:00:00
```

### Restaurar un snapshot

```bash
# Con virsh (apagar la VM primero si es snapshot de disco)
virsh snapshot-revert debian-lab snap-before-update

# Con qemu-img (VM debe estar apagada)
qemu-img snapshot -a snap-before-update debian-lab.qcow2
```

### Borrar un snapshot

```bash
# Con virsh
virsh snapshot-delete debian-lab snap-before-update

# Con qemu-img
qemu-img snapshot -d snap-before-update debian-lab.qcow2
```

### Snapshots internos vs backing files

| Aspecto | Snapshots internos | Backing files |
|---------|:------------------:|:-------------:|
| Ubicación | Dentro del mismo archivo | Archivos separados |
| Múltiples VMs | No (un snapshot por VM) | Sí (una base, muchos overlays) |
| Caso de uso | "Guardar estado antes de cambio" | "Crear N VMs desde plantilla" |
| Rendimiento | Ligero impacto acumulativo | Ligero impacto por lectura en cadena |
| Gestión | virsh snapshot-* | qemu-img create -b |

---

## 7. Otros formatos: vmdk, vdi, vhd

QEMU puede trabajar con formatos de otros hipervisores, principalmente para **migrar VMs** entre plataformas.

### vmdk (VMware)

```bash
# Formato nativo de VMware ESXi / Workstation
qemu-img info disk.vmdk

# Convertir VMDK → qcow2 para usar en QEMU/KVM
qemu-img convert -f vmdk -O qcow2 disk.vmdk disk.qcow2
```

### vdi (VirtualBox)

```bash
# Formato nativo de Oracle VirtualBox
qemu-img convert -f vdi -O qcow2 disk.vdi disk.qcow2
```

### vhd / vhdx (Hyper-V)

```bash
# Formato de Microsoft Hyper-V
qemu-img convert -f vpc -O qcow2 disk.vhd disk.qcow2    # vhd
qemu-img convert -f vhdx -O qcow2 disk.vhdx disk.qcow2  # vhdx
```

### Tabla de compatibilidad

| Formato | Hipervisor nativo | Thin provisioning | Snapshots | Convertible |
|---------|------------------|:-----------------:|:---------:|:-----------:|
| raw | Todos | Sparse files | No | Sí |
| qcow2 | QEMU/KVM | Sí | Sí | Sí |
| vmdk | VMware | Sí | Sí (ESXi) | Sí |
| vdi | VirtualBox | Sí | No | Sí |
| vhd | Hyper-V (gen1) | Sí | No | Sí |
| vhdx | Hyper-V (gen2) | Sí | No | Sí |

---

## 8. qemu-img: la herramienta de gestión

`qemu-img` es la navaja suiza para imágenes de disco. Todos los subcomandos importantes:

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
- **virtual size**: lo que la VM ve como tamaño del disco.
- **disk size**: espacio real que ocupa en el host.
- **cluster_size**: tamaño de cada unidad de asignación (64 KiB por defecto).
- **corrupt**: si qemu-img detecta corrupción en los metadatos.

```bash
# Con cadena de backing files
qemu-img info --backing-chain disk.qcow2

# Salida en JSON (para scripts)
qemu-img info --output=json disk.qcow2
```

### create: crear imagen nueva

```bash
# qcow2 básico
qemu-img create -f qcow2 disk.qcow2 20G

# raw
qemu-img create -f raw disk.raw 20G

# qcow2 con backing file
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2

# qcow2 con opciones avanzadas
qemu-img create -f qcow2 -o preallocation=metadata,cluster_size=128K,lazy_refcounts=on disk.qcow2 20G
```

### convert: convertir entre formatos

```bash
# qcow2 → raw
qemu-img convert -f qcow2 -O raw disk.qcow2 disk.raw

# raw → qcow2
qemu-img convert -f raw -O qcow2 disk.raw disk.qcow2

# qcow2 → qcow2 comprimido (reduce tamaño para distribución)
qemu-img convert -c -f qcow2 -O qcow2 disk.qcow2 compressed.qcow2

# vmdk → qcow2
qemu-img convert -f vmdk -O qcow2 disk.vmdk disk.qcow2

# Fusionar backing chain en un solo archivo
qemu-img convert -f qcow2 -O qcow2 overlay.qcow2 standalone.qcow2
# standalone.qcow2 ya no depende del backing file
```

La conversión crea un archivo **nuevo** — el original no se modifica. Los clusters vacíos no se copian, así que convertir un qcow2 fragmentado a qcow2 nuevo lo compacta.

### resize: cambiar tamaño virtual

```bash
# Crecer 5 GiB (la VM debe estar apagada)
qemu-img resize disk.qcow2 +5G

# Establecer tamaño exacto
qemu-img resize disk.qcow2 25G

# Reducir (peligroso — puede destruir datos)
qemu-img resize --shrink disk.qcow2 15G
```

**Importante**: `resize` solo cambia el tamaño del "disco virtual". Dentro de la VM todavía necesitas:

```
 qemu-img resize +5G          Dentro de la VM:
 (cambia tamaño del disco)    1. growpart /dev/vda 3  (expandir partición)
                              2. resize2fs /dev/vda3  (expandir ext4)
                              o  xfs_growfs /         (expandir XFS)
```

Esto se detalla en T02.

### check: verificar integridad

```bash
# Verificar una imagen qcow2
qemu-img check disk.qcow2
# No errors were found on the image.

# Reparar errores de refcount
qemu-img check -r all disk.qcow2
```

### map: ver asignación de clusters

```bash
# Ver qué partes de la imagen tienen datos
qemu-img map --output=json disk.qcow2 | head -20
# Muestra qué offsets tienen datos y cuáles son cero
```

---

## 9. Raw vs qcow2: cuándo usar cada uno

### Comparación directa

| Criterio | raw | qcow2 |
|----------|:---:|:-----:|
| Rendimiento I/O | Mejor (~5-10% más rápido) | Bueno |
| Thin provisioning | Solo sparse files (depende del FS host) | Nativo |
| Snapshots | No | Sí |
| Backing files | No | Sí |
| Compresión | No | Sí |
| Cifrado | No (usar LUKS externo) | Sí (LUKS integrado) |
| Herramientas host | losetup, mount directo | Necesita qemu-nbd |
| Portabilidad | Máxima | QEMU/KVM |
| Overhead de metadatos | Ninguno | Mínimo (~0.1%) |

### Diagrama de decisión

```
 ¿Qué formato usar?

 ¿Necesitas snapshots, backing files o compresión?
     Sí → qcow2
     No ↓

 ¿El rendimiento I/O es crítico (base de datos)?
     Sí → raw (con preallocation)
     No ↓

 ¿Quieres ahorrar espacio en el host?
     Sí → qcow2 (thin provisioning nativo)
     No ↓

 ¿Necesitas acceder desde el host con losetup/mount?
     Sí → raw
     No ↓

 → qcow2 (default recomendado para QEMU/KVM)
```

### Para este curso

Usa **qcow2** para todo. El ahorro de espacio con thin provisioning y la capacidad de hacer snapshots antes de experimentar valen mucho más que la pequeña ganancia de rendimiento de raw.

---

## 10. Errores comunes

### Error 1: no entender virtual size vs disk size

```bash
qemu-img info disk.qcow2
# virtual size: 20 GiB
# disk size: 196 KiB

ls -lh disk.qcow2
# -rw-r--r-- 1 user user 193K disk.qcow2

# "¡Mi disco de 20 GiB solo tiene 193K! ¡Algo está mal!"
```

**Por qué confunde**: `ls -lh` puede mostrar el tamaño del archivo, no el tamaño virtual. Con qcow2, una imagen de 20 GiB puede ocupar solo KiB cuando está vacía.

**Solución**: siempre usar `qemu-img info` para ver ambos tamaños. La VM ve el `virtual size`; el host ocupa el `disk size`.

### Error 2: modificar el backing file después de crear overlays

```bash
# Crear overlay basado en base
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2

# "Voy a actualizar la base"
virsh start vm-base    # ← modifica base.qcow2
# Ahora overlay.qcow2 está corrupto — sus deltas no coinciden con la base modificada
```

**Solución**: una vez que una imagen tiene overlays, tratarla como **inmutable**. Si necesitas actualizar la base:

```bash
# Opción 1: crear nueva base
cp base.qcow2 base-v2.qcow2
# Trabajar sobre base-v2, crear nuevos overlays

# Opción 2: rebase del overlay a nueva base
qemu-img rebase -b base-v2.qcow2 -F qcow2 overlay.qcow2
```

### Error 3: hacer resize sin expandir partición y filesystem

```bash
qemu-img resize disk.qcow2 +10G
virsh start debian-lab
# Dentro de la VM:
df -h /
# Solo muestra 20 GiB... ¿dónde están los 10G extra?
```

**Por qué pasa**: `qemu-img resize` solo cambia el tamaño del disco virtual. La tabla de particiones y el filesystem siguen con el tamaño original.

**Solución**: dentro de la VM, expandir la partición y el filesystem (se detalla en T02).

### Error 4: copiar qcow2 con cp y perder sparseness

```bash
cp disk.qcow2 backup.qcow2
# Si el qcow2 tiene sparse regions, cp puede materializar los ceros
du -h disk.qcow2 backup.qcow2
# disk.qcow2     3.1G
# backup.qcow2  20.0G    ← ¡explotó!
```

**Por qué pasa**: `cp` sin opciones puede no preservar archivos sparse.

**Solución**:

```bash
# Opción 1: cp con --sparse=always
cp --sparse=always disk.qcow2 backup.qcow2

# Opción 2: qemu-img convert (más seguro, también compacta)
qemu-img convert -O qcow2 disk.qcow2 backup.qcow2
```

### Error 5: olvidar -F al crear overlay

```bash
qemu-img create -f qcow2 -b base.qcow2 overlay.qcow2
# WARNING: could not determine format of backing file
```

**Por qué pasa**: sin `-F qcow2`, QEMU intenta autodetectar el formato del backing file, lo que es un riesgo de seguridad (un archivo malicioso podría engañar al autodetector).

**Solución**: siempre especificar `-F`:

```bash
qemu-img create -f qcow2 -b base.qcow2 -F qcow2 overlay.qcow2
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│            IMÁGENES DE DISCO — REFERENCIA                        │
├──────────────────────────────────────────────────────────────────┤
│ FORMATOS                                                         │
│   raw         Byte a byte, sin metadatos, máximo rendimiento    │
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
├──────────────────────────────────────────────────────────────────┤
│ TAMAÑOS                                                          │
│   virtual size    Lo que la VM ve (ej: 20 GiB)                  │
│   disk size       Lo que ocupa en el host (ej: 3.1 GiB)         │
│   ls -lh          Puede no reflejar tamaño real (sparse)        │
│   du -h           Tamaño real en disco                           │
│   qemu-img info   Muestra ambos (fuente de verdad)              │
├──────────────────────────────────────────────────────────────────┤
│ BACKING FILES                                                    │
│   Crear:    qemu-img create -f qcow2 -b base -F qcow2 overlay  │
│   Regla:    NUNCA modificar el backing file                      │
│   Fusionar: qemu-img convert -O qcow2 overlay standalone        │
│   Copiar:   cp --sparse=always / qemu-img convert               │
│   Rebase:   qemu-img rebase -b nueva-base -F qcow2 overlay     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Comparar raw vs qcow2

1. Crea dos imágenes de 5 GiB, una raw y una qcow2:
   ```bash
   qemu-img create -f raw /tmp/test-raw.img 5G
   qemu-img create -f qcow2 /tmp/test-qcow2.qcow2 5G
   ```
2. Compara el tamaño real de cada una con `ls -lh` y `du -h`. ¿Por qué `ls` muestra 5G para la raw pero `du` muestra mucho menos?
3. Ejecuta `qemu-img info` en ambas. ¿Cuál es el `virtual size` y `disk size` de cada una?
4. Escribe 500 MiB de datos aleatorios en la raw:
   ```bash
   dd if=/dev/urandom of=/tmp/test-raw.img bs=1M count=500 conv=notrunc
   ```
5. Compara `du -h` de nuevo. ¿La raw creció? ¿Y la qcow2?
6. Convierte la raw a qcow2:
   ```bash
   qemu-img convert -f raw -O qcow2 /tmp/test-raw.img /tmp/converted.qcow2
   ```
7. ¿Cuánto ocupa `converted.qcow2`? ¿Es más o menos que la raw original?
8. Limpia: `rm /tmp/test-raw.img /tmp/test-qcow2.qcow2 /tmp/converted.qcow2`

> **Pregunta de reflexión**: `ls -lh` mostró 5G para la imagen raw, pero `du -h` mostró mucho menos. Esto se llama "sparse file". ¿Qué hace el filesystem del host para no ocupar espacio en los bloques que son todo ceros? ¿Qué pasa si copias la imagen con `cp` sin `--sparse=always`?

### Ejercicio 2: Backing files y linked clones

1. Crea una imagen base de 2 GiB:
   ```bash
   qemu-img create -f qcow2 /tmp/base.qcow2 2G
   ```
2. Simula "instalar un SO" escribiendo datos:
   ```bash
   # Asociar a loop device y escribir
   sudo modprobe nbd max_part=8
   sudo qemu-nbd --connect=/dev/nbd0 /tmp/base.qcow2
   sudo mkfs.ext4 /dev/nbd0
   sudo mount /dev/nbd0 /mnt
   echo "Este es el SO base" | sudo tee /mnt/base-file.txt
   sudo umount /mnt
   sudo qemu-nbd --disconnect /dev/nbd0
   ```
   (Si `qemu-nbd` no está disponible, puedes omitir este paso y simplemente usar la imagen vacía como base.)
3. Verifica el tamaño con `qemu-img info /tmp/base.qcow2`.
4. Crea dos overlays:
   ```bash
   qemu-img create -f qcow2 -b /tmp/base.qcow2 -F qcow2 /tmp/vm1.qcow2
   qemu-img create -f qcow2 -b /tmp/base.qcow2 -F qcow2 /tmp/vm2.qcow2
   ```
5. ¿Cuánto ocupan los overlays? (`qemu-img info`)
6. Verifica la cadena de backing con `qemu-img info --backing-chain /tmp/vm1.qcow2`.
7. Fusiona vm1 en un archivo independiente:
   ```bash
   qemu-img convert -f qcow2 -O qcow2 /tmp/vm1.qcow2 /tmp/vm1-standalone.qcow2
   ```
8. ¿`vm1-standalone.qcow2` tiene backing file? Verifica con `qemu-img info`.
9. Limpia los archivos temporales.

> **Pregunta de reflexión**: si tuvieras 20 VMs de laboratorio idénticas excepto por pequeñas configuraciones, ¿cuánto espacio ahorrarías usando backing files vs copias completas? ¿Cuál es el riesgo de que todas dependan del mismo archivo base?

### Ejercicio 3: Snapshots y recuperación

Si tienes una VM con QEMU/KVM:

1. Verifica que la imagen usa qcow2:
   ```bash
   virsh domblklist <tu-vm>
   qemu-img info /var/lib/libvirt/images/<imagen>.qcow2
   ```
2. Apaga la VM y crea un snapshot de disco:
   ```bash
   qemu-img snapshot -c "pre-experiment" /var/lib/libvirt/images/<imagen>.qcow2
   ```
3. Lista los snapshots:
   ```bash
   qemu-img snapshot -l /var/lib/libvirt/images/<imagen>.qcow2
   ```
4. Arranca la VM y haz un cambio visible (crea un archivo, instala un paquete, etc.).
5. Apaga la VM y verifica con `qemu-img info` que el `disk size` creció.
6. Restaura el snapshot:
   ```bash
   qemu-img snapshot -a "pre-experiment" /var/lib/libvirt/images/<imagen>.qcow2
   ```
7. Arranca la VM. ¿El cambio que hiciste desapareció?
8. Borra el snapshot:
   ```bash
   qemu-img snapshot -d "pre-experiment" /var/lib/libvirt/images/<imagen>.qcow2
   ```

> **Pregunta de reflexión**: después de restaurar el snapshot y borrar el cambio, ¿el `disk size` de la imagen volvió a ser lo que era antes? Si no, ¿por qué? ¿Cómo podrías recuperar ese espacio?
